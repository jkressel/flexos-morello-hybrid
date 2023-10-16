/************** Begin file sqlite3rbu.c **************************************/
/*
** 2014 August 30
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
**
** OVERVIEW 
**
**  The RBU extension requires that the RBU update be packaged as an
**  SQLite database. The tables it expects to find are described in
**  sqlite3rbu.h.  Essentially, for each table xyz in the target database
**  that the user wishes to write to, a corresponding data_xyz table is
**  created in the RBU database and populated with one row for each row to
**  update, insert or delete from the target table.
** 
**  The update proceeds in three stages:
** 
**  1) The database is updated. The modified database pages are written
**     to a *-oal file. A *-oal file is just like a *-wal file, except
**     that it is named "<database>-oal" instead of "<database>-wal".
**     Because regular SQLite clients do not look for file named
**     "<database>-oal", they go on using the original database in
**     rollback mode while the *-oal file is being generated.
** 
**     During this stage RBU does not update the database by writing
**     directly to the target tables. Instead it creates "imposter"
**     tables using the SQLITE_TESTCTRL_IMPOSTER interface that it uses
**     to update each b-tree individually. All updates required by each
**     b-tree are completed before moving on to the next, and all
**     updates are done in sorted key order.
** 
**  2) The "<database>-oal" file is moved to the equivalent "<database>-wal"
**     location using a call to rename(2). Before doing this the RBU
**     module takes an EXCLUSIVE lock on the database file, ensuring
**     that there are no other active readers.
** 
**     Once the EXCLUSIVE lock is released, any other database readers
**     detect the new *-wal file and read the database in wal mode. At
**     this point they see the new version of the database - including
**     the updates made as part of the RBU update.
** 
**  3) The new *-wal file is checkpointed. This proceeds in the same way 
**     as a regular database checkpoint, except that a single frame is
**     checkpointed each time sqlite3rbu_step() is called. If the RBU
**     handle is closed before the entire *-wal file is checkpointed,
**     the checkpoint progress is saved in the RBU database and the
**     checkpoint can be resumed by another RBU client at some point in
**     the future.
**
** POTENTIAL PROBLEMS
** 
**  The rename() call might not be portable. And RBU is not currently
**  syncing the directory after renaming the file.
**
**  When state is saved, any commit to the *-oal file and the commit to
**  the RBU update database are not atomic. So if the power fails at the
**  wrong moment they might get out of sync. As the main database will be
**  committed before the RBU update database this will likely either just
**  pass unnoticed, or result in SQLITE_CONSTRAINT errors (due to UNIQUE
**  constraint violations).
**
**  If some client does modify the target database mid RBU update, or some
**  other error occurs, the RBU extension will keep throwing errors. It's
**  not really clear how to get out of this state. The system could just
**  by delete the RBU update database and *-oal file and have the device
**  download the update again and start over.
**
**  At present, for an UPDATE, both the new.* and old.* records are
**  collected in the rbu_xyz table. And for both UPDATEs and DELETEs all
**  fields are collected.  This means we're probably writing a lot more
**  data to disk when saving the state of an ongoing update to the RBU
**  update database than is strictly necessary.
** 
*/

/* #include <assert.h> */
/* #include <string.h> */
/* #include <stdio.h> */

/* #include "sqlite3.h" */

#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_RBU)
/************** Include sqlite3rbu.h in the middle of sqlite3rbu.c ***********/
/************** Begin file sqlite3rbu.h **************************************/
/*
** 2014 August 30
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This file contains the public interface for the RBU extension. 
*/

/*
** SUMMARY
**
** Writing a transaction containing a large number of operations on 
** b-tree indexes that are collectively larger than the available cache
** memory can be very inefficient. 
**
** The problem is that in order to update a b-tree, the leaf page (at least)
** containing the entry being inserted or deleted must be modified. If the
** working set of leaves is larger than the available cache memory, then a 
** single leaf that is modified more than once as part of the transaction 
** may be loaded from or written to the persistent media multiple times.
** Additionally, because the index updates are likely to be applied in
** random order, access to pages within the database is also likely to be in 
** random order, which is itself quite inefficient.
**
** One way to improve the situation is to sort the operations on each index
** by index key before applying them to the b-tree. This leads to an IO
** pattern that resembles a single linear scan through the index b-tree,
** and all but guarantees each modified leaf page is loaded and stored 
** exactly once. SQLite uses this trick to improve the performance of
** CREATE INDEX commands. This extension allows it to be used to improve
** the performance of large transactions on existing databases.
**
** Additionally, this extension allows the work involved in writing the 
** large transaction to be broken down into sub-transactions performed 
** sequentially by separate processes. This is useful if the system cannot 
** guarantee that a single update process will run for long enough to apply 
** the entire update, for example because the update is being applied on a 
** mobile device that is frequently rebooted. Even after the writer process 
** has committed one or more sub-transactions, other database clients continue
** to read from the original database snapshot. In other words, partially 
** applied transactions are not visible to other clients. 
**
** "RBU" stands for "Resumable Bulk Update". As in a large database update
** transmitted via a wireless network to a mobile device. A transaction
** applied using this extension is hence refered to as an "RBU update".
**
**
** LIMITATIONS
**
** An "RBU update" transaction is subject to the following limitations:
**
**   * The transaction must consist of INSERT, UPDATE and DELETE operations
**     only.
**
**   * INSERT statements may not use any default values.
**
**   * UPDATE and DELETE statements must identify their target rows by 
**     non-NULL PRIMARY KEY values. Rows with NULL values stored in PRIMARY
**     KEY fields may not be updated or deleted. If the table being written 
**     has no PRIMARY KEY, affected rows must be identified by rowid.
**
**   * UPDATE statements may not modify PRIMARY KEY columns.
**
**   * No triggers will be fired.
**
**   * No foreign key violations are detected or reported.
**
**   * CHECK constraints are not enforced.
**
**   * No constraint handling mode except for "OR ROLLBACK" is supported.
**
**
** PREPARATION
**
** An "RBU update" is stored as a separate SQLite database. A database
** containing an RBU update is an "RBU database". For each table in the 
** target database to be updated, the RBU database should contain a table
** named "data_<target name>" containing the same set of columns as the
** target table, and one more - "rbu_control". The data_% table should 
** have no PRIMARY KEY or UNIQUE constraints, but each column should have
** the same type as the corresponding column in the target database.
** The "rbu_control" column should have no type at all. For example, if
** the target database contains:
**
**   CREATE TABLE t1(a INTEGER PRIMARY KEY, b TEXT, c UNIQUE);
**
** Then the RBU database should contain:
**
**   CREATE TABLE data_t1(a INTEGER, b TEXT, c, rbu_control);
**
** The order of the columns in the data_% table does not matter.
**
** Instead of a regular table, the RBU database may also contain virtual
** tables or view named using the data_<target> naming scheme. 
**
** Instead of the plain data_<target> naming scheme, RBU database tables 
** may also be named data<integer>_<target>, where <integer> is any sequence
** of zero or more numeric characters (0-9). This can be significant because
** tables within the RBU database are always processed in order sorted by 
** name. By judicious selection of the <integer> portion of the names
** of the RBU tables the user can therefore control the order in which they
** are processed. This can be useful, for example, to ensure that "external
** content" FTS4 tables are updated before their underlying content tables.
**
** If the target database table is a virtual table or a table that has no
** PRIMARY KEY declaration, the data_% table must also contain a column 
** named "rbu_rowid". This column is mapped to the tables implicit primary 
** key column - "rowid". Virtual tables for which the "rowid" column does 
** not function like a primary key value cannot be updated using RBU. For 
** example, if the target db contains either of the following:
**
**   CREATE VIRTUAL TABLE x1 USING fts3(a, b);
**   CREATE TABLE x1(a, b)
**
** then the RBU database should contain:
**
**   CREATE TABLE data_x1(a, b, rbu_rowid, rbu_control);
**
** All non-hidden columns (i.e. all columns matched by "SELECT *") of the
** target table must be present in the input table. For virtual tables,
** hidden columns are optional - they are updated by RBU if present in
** the input table, or not otherwise. For example, to write to an fts4
** table with a hidden languageid column such as:
**
**   CREATE VIRTUAL TABLE ft1 USING fts4(a, b, languageid='langid');
**
** Either of the following input table schemas may be used:
**
**   CREATE TABLE data_ft1(a, b, langid, rbu_rowid, rbu_control);
**   CREATE TABLE data_ft1(a, b, rbu_rowid, rbu_control);
**
** For each row to INSERT into the target database as part of the RBU 
** update, the corresponding data_% table should contain a single record
** with the "rbu_control" column set to contain integer value 0. The
** other columns should be set to the values that make up the new record 
** to insert. 
**
** If the target database table has an INTEGER PRIMARY KEY, it is not 
** possible to insert a NULL value into the IPK column. Attempting to 
** do so results in an SQLITE_MISMATCH error.
**
** For each row to DELETE from the target database as part of the RBU 
** update, the corresponding data_% table should contain a single record
** with the "rbu_control" column set to contain integer value 1. The
** real primary key values of the row to delete should be stored in the
** corresponding columns of the data_% table. The values stored in the
** other columns are not used.
**
** For each row to UPDATE from the target database as part of the RBU 
** update, the corresponding data_% table should contain a single record
** with the "rbu_control" column set to contain a value of type text.
** The real primary key values identifying the row to update should be 
** stored in the corresponding columns of the data_% table row, as should
** the new values of all columns being update. The text value in the 
** "rbu_control" column must contain the same number of characters as
** there are columns in the target database table, and must consist entirely
** of 'x' and '.' characters (or in some special cases 'd' - see below). For 
** each column that is being updated, the corresponding character is set to
** 'x'. For those that remain as they are, the corresponding character of the
** rbu_control value should be set to '.'. For example, given the tables 
** above, the update statement:
**
**   UPDATE t1 SET c = 'usa' WHERE a = 4;
**
** is represented by the data_t1 row created by:
**
**   INSERT INTO data_t1(a, b, c, rbu_control) VALUES(4, NULL, 'usa', '..x');
**
** Instead of an 'x' character, characters of the rbu_control value specified
** for UPDATEs may also be set to 'd'. In this case, instead of updating the
** target table with the value stored in the corresponding data_% column, the
** user-defined SQL function "rbu_delta()" is invoked and the result stored in
** the target table column. rbu_delta() is invoked with two arguments - the
** original value currently stored in the target table column and the 
** value specified in the data_xxx table.
**
** For example, this row:
**
**   INSERT INTO data_t1(a, b, c, rbu_control) VALUES(4, NULL, 'usa', '..d');
**
** is similar to an UPDATE statement such as: 
**
**   UPDATE t1 SET c = rbu_delta(c, 'usa') WHERE a = 4;
**
** Finally, if an 'f' character appears in place of a 'd' or 's' in an 
** ota_control string, the contents of the data_xxx table column is assumed
** to be a "fossil delta" - a patch to be applied to a blob value in the
** format used by the fossil source-code management system. In this case
** the existing value within the target database table must be of type BLOB. 
** It is replaced by the result of applying the specified fossil delta to
** itself.
**
** If the target database table is a virtual table or a table with no PRIMARY
** KEY, the rbu_control value should not include a character corresponding 
** to the rbu_rowid value. For example, this:
**
**   INSERT INTO data_ft1(a, b, rbu_rowid, rbu_control) 
**       VALUES(NULL, 'usa', 12, '.x');
**
** causes a result similar to:
**
**   UPDATE ft1 SET b = 'usa' WHERE rowid = 12;
**
** The data_xxx tables themselves should have no PRIMARY KEY declarations.
** However, RBU is more efficient if reading the rows in from each data_xxx
** table in "rowid" order is roughly the same as reading them sorted by
** the PRIMARY KEY of the corresponding target database table. In other 
** words, rows should be sorted using the destination table PRIMARY KEY 
** fields before they are inserted into the data_xxx tables.
**
** USAGE
**
** The API declared below allows an application to apply an RBU update 
** stored on disk to an existing target database. Essentially, the 
** application:
**
**     1) Opens an RBU handle using the sqlite3rbu_open() function.
**
**     2) Registers any required virtual table modules with the database
**        handle returned by sqlite3rbu_db(). Also, if required, register
**        the rbu_delta() implementation.
**
**     3) Calls the sqlite3rbu_step() function one or more times on
**        the new handle. Each call to sqlite3rbu_step() performs a single
**        b-tree operation, so thousands of calls may be required to apply 
**        a complete update.
**
**     4) Calls sqlite3rbu_close() to close the RBU update handle. If
**        sqlite3rbu_step() has been called enough times to completely
**        apply the update to the target database, then the RBU database
**        is marked as fully applied. Otherwise, the state of the RBU 
**        update application is saved in the RBU database for later 
**        resumption.
**
** See comments below for more detail on APIs.
**
** If an update is only partially applied to the target database by the
** time sqlite3rbu_close() is called, various state information is saved 
** within the RBU database. This allows subsequent processes to automatically
** resume the RBU update from where it left off.
**
** To remove all RBU extension state information, returning an RBU database 
** to its original contents, it is sufficient to drop all tables that begin
** with the prefix "rbu_"
**
** DATABASE LOCKING
**
** An RBU update may not be applied to a database in WAL mode. Attempting
** to do so is an error (SQLITE_ERROR).
**
** While an RBU handle is open, a SHARED lock may be held on the target
** database file. This means it is possible for other clients to read the
** database, but not to write it.
**
** If an RBU update is started and then suspended before it is completed,
** then an external client writes to the database, then attempting to resume
** the suspended RBU update is also an error (SQLITE_BUSY).
*/

#ifndef _SQLITE3RBU_H
#define _SQLITE3RBU_H

/* #include "sqlite3.h"              ** Required for error code definitions ** */

#if 0
extern "C" {
#endif

typedef struct sqlite3rbu sqlite3rbu;

/*
** Open an RBU handle.
**
** Argument zTarget is the path to the target database. Argument zRbu is
** the path to the RBU database. Each call to this function must be matched
** by a call to sqlite3rbu_close(). When opening the databases, RBU passes
** the SQLITE_CONFIG_URI flag to sqlite3_open_v2(). So if either zTarget
** or zRbu begin with "file:", it will be interpreted as an SQLite 
** database URI, not a regular file name.
**
** If the zState argument is passed a NULL value, the RBU extension stores 
** the current state of the update (how many rows have been updated, which 
** indexes are yet to be updated etc.) within the RBU database itself. This
** can be convenient, as it means that the RBU application does not need to
** organize removing a separate state file after the update is concluded. 
** Or, if zState is non-NULL, it must be a path to a database file in which 
** the RBU extension can store the state of the update.
**
** When resuming an RBU update, the zState argument must be passed the same
** value as when the RBU update was started.
**
** Once the RBU update is finished, the RBU extension does not 
** automatically remove any zState database file, even if it created it.
**
** By default, RBU uses the default VFS to access the files on disk. To
** use a VFS other than the default, an SQLite "file:" URI containing a
** "vfs=..." option may be passed as the zTarget option.
**
** IMPORTANT NOTE FOR ZIPVFS USERS: The RBU extension works with all of
** SQLite's built-in VFSs, including the multiplexor VFS. However it does
** not work out of the box with zipvfs. Refer to the comment describing
** the zipvfs_create_vfs() API below for details on using RBU with zipvfs.
*/
SQLITE_API sqlite3rbu *sqlite3rbu_open(
  const char *zTarget, 
  const char *zRbu,
  const char *zState
);

/*
** Open an RBU handle to perform an RBU vacuum on database file zTarget.
** An RBU vacuum is similar to SQLite's built-in VACUUM command, except
** that it can be suspended and resumed like an RBU update.
**
** The second argument to this function identifies a database in which 
** to store the state of the RBU vacuum operation if it is suspended. The 
** first time sqlite3rbu_vacuum() is called, to start an RBU vacuum
** operation, the state database should either not exist or be empty
** (contain no tables). If an RBU vacuum is suspended by calling 
** sqlite3rbu_close() on the RBU handle before sqlite3rbu_step() has
** returned SQLITE_DONE, the vacuum state is stored in the state database. 
** The vacuum can be resumed by calling this function to open a new RBU
** handle specifying the same target and state databases.
**
** If the second argument passed to this function is NULL, then the
** name of the state database is "<database>-vacuum", where <database>
** is the name of the target database file. In this case, on UNIX, if the
** state database is not already present in the file-system, it is created
** with the same permissions as the target db is made. 
**
** With an RBU vacuum, it is an SQLITE_MISUSE error if the name of the 
** state database ends with "-vactmp". This name is reserved for internal 
** use.
**
** This function does not delete the state database after an RBU vacuum
** is completed, even if it created it. However, if the call to
** sqlite3rbu_close() returns any value other than SQLITE_OK, the contents
** of the state tables within the state database are zeroed. This way,
** the next call to sqlite3rbu_vacuum() opens a handle that starts a 
** new RBU vacuum operation.
**
** As with sqlite3rbu_open(), Zipvfs users should rever to the comment
** describing the sqlite3rbu_create_vfs() API function below for 
** a description of the complications associated with using RBU with 
** zipvfs databases.
*/
SQLITE_API sqlite3rbu *sqlite3rbu_vacuum(
  const char *zTarget, 
  const char *zState
);

/*
** Configure a limit for the amount of temp space that may be used by
** the RBU handle passed as the first argument. The new limit is specified
** in bytes by the second parameter. If it is positive, the limit is updated.
** If the second parameter to this function is passed zero, then the limit
** is removed entirely. If the second parameter is negative, the limit is
** not modified (this is useful for querying the current limit).
**
** In all cases the returned value is the current limit in bytes (zero 
** indicates unlimited).
**
** If the temp space limit is exceeded during operation, an SQLITE_FULL
** error is returned.
*/
SQLITE_API sqlite3_int64 sqlite3rbu_temp_size_limit(sqlite3rbu*, sqlite3_int64);

/*
** Return the current amount of temp file space, in bytes, currently used by 
** the RBU handle passed as the only argument.
*/
SQLITE_API sqlite3_int64 sqlite3rbu_temp_size(sqlite3rbu*);

/*
** Internally, each RBU connection uses a separate SQLite database 
** connection to access the target and rbu update databases. This
** API allows the application direct access to these database handles.
**
** The first argument passed to this function must be a valid, open, RBU
** handle. The second argument should be passed zero to access the target
** database handle, or non-zero to access the rbu update database handle.
** Accessing the underlying database handles may be useful in the
** following scenarios:
**
**   * If any target tables are virtual tables, it may be necessary to
**     call sqlite3_create_module() on the target database handle to 
**     register the required virtual table implementations.
**
**   * If the data_xxx tables in the RBU source database are virtual 
**     tables, the application may need to call sqlite3_create_module() on
**     the rbu update db handle to any required virtual table
**     implementations.
**
**   * If the application uses the "rbu_delta()" feature described above,
**     it must use sqlite3_create_function() or similar to register the
**     rbu_delta() implementation with the target database handle.
**
** If an error has occurred, either while opening or stepping the RBU object,
** this function may return NULL. The error code and message may be collected
** when sqlite3rbu_close() is called.
**
** Database handles returned by this function remain valid until the next
** call to any sqlite3rbu_xxx() function other than sqlite3rbu_db().
*/
SQLITE_API sqlite3 *sqlite3rbu_db(sqlite3rbu*, int bRbu);

/*
** Do some work towards applying the RBU update to the target db. 
**
** Return SQLITE_DONE if the update has been completely applied, or 
** SQLITE_OK if no error occurs but there remains work to do to apply
** the RBU update. If an error does occur, some other error code is 
** returned. 
**
** Once a call to sqlite3rbu_step() has returned a value other than
** SQLITE_OK, all subsequent calls on the same RBU handle are no-ops
** that immediately return the same value.
*/
SQLITE_API int sqlite3rbu_step(sqlite3rbu *pRbu);

/*
** Force RBU to save its state to disk.
**
** If a power failure or application crash occurs during an update, following
** system recovery RBU may resume the update from the point at which the state
** was last saved. In other words, from the most recent successful call to 
** sqlite3rbu_close() or this function.
**
** SQLITE_OK is returned if successful, or an SQLite error code otherwise.
*/
SQLITE_API int sqlite3rbu_savestate(sqlite3rbu *pRbu);

/*
** Close an RBU handle. 
**
** If the RBU update has been completely applied, mark the RBU database
** as fully applied. Otherwise, assuming no error has occurred, save the
** current state of the RBU update appliation to the RBU database.
**
** If an error has already occurred as part of an sqlite3rbu_step()
** or sqlite3rbu_open() call, or if one occurs within this function, an
** SQLite error code is returned. Additionally, if pzErrmsg is not NULL,
** *pzErrmsg may be set to point to a buffer containing a utf-8 formatted
** English language error message. It is the responsibility of the caller to
** eventually free any such buffer using sqlite3_free().
**
** Otherwise, if no error occurs, this function returns SQLITE_OK if the
** update has been partially applied, or SQLITE_DONE if it has been 
** completely applied.
*/
SQLITE_API int sqlite3rbu_close(sqlite3rbu *pRbu, char **pzErrmsg);

/*
** Return the total number of key-value operations (inserts, deletes or 
** updates) that have been performed on the target database since the
** current RBU update was started.
*/
SQLITE_API sqlite3_int64 sqlite3rbu_progress(sqlite3rbu *pRbu);

/*
** Obtain permyriadage (permyriadage is to 10000 as percentage is to 100) 
** progress indications for the two stages of an RBU update. This API may
** be useful for driving GUI progress indicators and similar.
**
** An RBU update is divided into two stages:
**
**   * Stage 1, in which changes are accumulated in an oal/wal file, and
**   * Stage 2, in which the contents of the wal file are copied into the
**     main database.
**
** The update is visible to non-RBU clients during stage 2. During stage 1
** non-RBU reader clients may see the original database.
**
** If this API is called during stage 2 of the update, output variable 
** (*pnOne) is set to 10000 to indicate that stage 1 has finished and (*pnTwo)
** to a value between 0 and 10000 to indicate the permyriadage progress of
** stage 2. A value of 5000 indicates that stage 2 is half finished, 
** 9000 indicates that it is 90% finished, and so on.
**
** If this API is called during stage 1 of the update, output variable 
** (*pnTwo) is set to 0 to indicate that stage 2 has not yet started. The
** value to which (*pnOne) is set depends on whether or not the RBU 
** database contains an "rbu_count" table. The rbu_count table, if it 
** exists, must contain the same columns as the following:
**
**   CREATE TABLE rbu_count(tbl TEXT PRIMARY KEY, cnt INTEGER) WITHOUT ROWID;
**
** There must be one row in the table for each source (data_xxx) table within
** the RBU database. The 'tbl' column should contain the name of the source
** table. The 'cnt' column should contain the number of rows within the
** source table.
**
** If the rbu_count table is present and populated correctly and this
** API is called during stage 1, the *pnOne output variable is set to the
** permyriadage progress of the same stage. If the rbu_count table does
** not exist, then (*pnOne) is set to -1 during stage 1. If the rbu_count
** table exists but is not correctly populated, the value of the *pnOne
** output variable during stage 1 is undefined.
*/
SQLITE_API void sqlite3rbu_bp_progress(sqlite3rbu *pRbu, int *pnOne, int*pnTwo);

/*
** Obtain an indication as to the current stage of an RBU update or vacuum.
** This function always returns one of the SQLITE_RBU_STATE_XXX constants
** defined in this file. Return values should be interpreted as follows:
**
** SQLITE_RBU_STATE_OAL:
**   RBU is currently building a *-oal file. The next call to sqlite3rbu_step()
**   may either add further data to the *-oal file, or compute data that will
**   be added by a subsequent call.
**
** SQLITE_RBU_STATE_MOVE:
**   RBU has finished building the *-oal file. The next call to sqlite3rbu_step()
**   will move the *-oal file to the equivalent *-wal path. If the current
**   operation is an RBU update, then the updated version of the database
**   file will become visible to ordinary SQLite clients following the next
**   call to sqlite3rbu_step().
**
** SQLITE_RBU_STATE_CHECKPOINT:
**   RBU is currently performing an incremental checkpoint. The next call to
**   sqlite3rbu_step() will copy a page of data from the *-wal file into
**   the target database file.
**
** SQLITE_RBU_STATE_DONE:
**   The RBU operation has finished. Any subsequent calls to sqlite3rbu_step()
**   will immediately return SQLITE_DONE.
**
** SQLITE_RBU_STATE_ERROR:
**   An error has occurred. Any subsequent calls to sqlite3rbu_step() will
**   immediately return the SQLite error code associated with the error.
*/
#define SQLITE_RBU_STATE_OAL        1
#define SQLITE_RBU_STATE_MOVE       2
#define SQLITE_RBU_STATE_CHECKPOINT 3
#define SQLITE_RBU_STATE_DONE       4
#define SQLITE_RBU_STATE_ERROR      5

SQLITE_API int sqlite3rbu_state(sqlite3rbu *pRbu);

/*
** Create an RBU VFS named zName that accesses the underlying file-system
** via existing VFS zParent. Or, if the zParent parameter is passed NULL, 
** then the new RBU VFS uses the default system VFS to access the file-system.
** The new object is registered as a non-default VFS with SQLite before 
** returning.
**
** Part of the RBU implementation uses a custom VFS object. Usually, this
** object is created and deleted automatically by RBU. 
**
** The exception is for applications that also use zipvfs. In this case,
** the custom VFS must be explicitly created by the user before the RBU
** handle is opened. The RBU VFS should be installed so that the zipvfs
** VFS uses the RBU VFS, which in turn uses any other VFS layers in use 
** (for example multiplexor) to access the file-system. For example,
** to assemble an RBU enabled VFS stack that uses both zipvfs and 
** multiplexor (error checking omitted):
**
**     // Create a VFS named "multiplex" (not the default).
**     sqlite3_multiplex_initialize(0, 0);
**
**     // Create an rbu VFS named "rbu" that uses multiplexor. If the
**     // second argument were replaced with NULL, the "rbu" VFS would
**     // access the file-system via the system default VFS, bypassing the
**     // multiplexor.
**     sqlite3rbu_create_vfs("rbu", "multiplex");
**
**     // Create a zipvfs VFS named "zipvfs" that uses rbu.
**     zipvfs_create_vfs_v3("zipvfs", "rbu", 0, xCompressorAlgorithmDetector);
**
**     // Make zipvfs the default VFS.
**     sqlite3_vfs_register(sqlite3_vfs_find("zipvfs"), 1);
**
** Because the default VFS created above includes a RBU functionality, it
** may be used by RBU clients. Attempting to use RBU with a zipvfs VFS stack
** that does not include the RBU layer results in an error.
**
** The overhead of adding the "rbu" VFS to the system is negligible for 
** non-RBU users. There is no harm in an application accessing the 
** file-system via "rbu" all the time, even if it only uses RBU functionality 
** occasionally.
*/
SQLITE_API int sqlite3rbu_create_vfs(const char *zName, const char *zParent);

/*
** Deregister and destroy an RBU vfs created by an earlier call to
** sqlite3rbu_create_vfs().
**
** VFS objects are not reference counted. If a VFS object is destroyed
** before all database handles that use it have been closed, the results
** are undefined.
*/
SQLITE_API void sqlite3rbu_destroy_vfs(const char *zName);

#if 0
}  /* end of the 'extern "C"' block */
#endif

#endif /* _SQLITE3RBU_H */

/************** End of sqlite3rbu.h ******************************************/
/************** Continuing where we left off in sqlite3rbu.c *****************/

#if defined(_WIN32_WCE)
/* #include "windows.h" */
#endif

/* Maximum number of prepared UPDATE statements held by this module */
#define SQLITE_RBU_UPDATE_CACHESIZE 16

/* Delta checksums disabled by default.  Compile with -DRBU_ENABLE_DELTA_CKSUM
** to enable checksum verification.
*/
#ifndef RBU_ENABLE_DELTA_CKSUM
# define RBU_ENABLE_DELTA_CKSUM 0
#endif

/*
** Swap two objects of type TYPE.
*/
#if !defined(SQLITE_AMALGAMATION)
# define SWAP(TYPE,A,B) {TYPE t=A; A=B; B=t;}
#endif

/*
** The rbu_state table is used to save the state of a partially applied
** update so that it can be resumed later. The table consists of integer
** keys mapped to values as follows:
**
** RBU_STATE_STAGE:
**   May be set to integer values 1, 2, 4 or 5. As follows:
**       1: the *-rbu file is currently under construction.
**       2: the *-rbu file has been constructed, but not yet moved 
**          to the *-wal path.
**       4: the checkpoint is underway.
**       5: the rbu update has been checkpointed.
**
** RBU_STATE_TBL:
**   Only valid if STAGE==1. The target database name of the table 
**   currently being written.
**
** RBU_STATE_IDX:
**   Only valid if STAGE==1. The target database name of the index 
**   currently being written, or NULL if the main table is currently being
**   updated.
**
** RBU_STATE_ROW:
**   Only valid if STAGE==1. Number of rows already processed for the current
**   table/index.
**
** RBU_STATE_PROGRESS:
**   Trbul number of sqlite3rbu_step() calls made so far as part of this
**   rbu update.
**
** RBU_STATE_CKPT:
**   Valid if STAGE==4. The 64-bit checksum associated with the wal-index
**   header created by recovering the *-wal file. This is used to detect
**   cases when another client appends frames to the *-wal file in the
**   middle of an incremental checkpoint (an incremental checkpoint cannot
**   be continued if this happens).
**
** RBU_STATE_COOKIE:
**   Valid if STAGE==1. The current change-counter cookie value in the 
**   target db file.
**
** RBU_STATE_OALSZ:
**   Valid if STAGE==1. The size in bytes of the *-oal file.
**
** RBU_STATE_DATATBL:
**   Only valid if STAGE==1. The RBU database name of the table 
**   currently being read.
*/
#define RBU_STATE_STAGE        1
#define RBU_STATE_TBL          2
#define RBU_STATE_IDX          3
#define RBU_STATE_ROW          4
#define RBU_STATE_PROGRESS     5
#define RBU_STATE_CKPT         6
#define RBU_STATE_COOKIE       7
#define RBU_STATE_OALSZ        8
#define RBU_STATE_PHASEONESTEP 9
#define RBU_STATE_DATATBL     10

#define RBU_STAGE_OAL         1
#define RBU_STAGE_MOVE        2
#define RBU_STAGE_CAPTURE     3
#define RBU_STAGE_CKPT        4
#define RBU_STAGE_DONE        5


#define RBU_CREATE_STATE \
  "CREATE TABLE IF NOT EXISTS %s.rbu_state(k INTEGER PRIMARY KEY, v)"

typedef struct RbuFrame RbuFrame;
typedef struct RbuObjIter RbuObjIter;
typedef struct RbuState RbuState;
typedef struct RbuSpan RbuSpan;
typedef struct rbu_vfs rbu_vfs;
typedef struct rbu_file rbu_file;
typedef struct RbuUpdateStmt RbuUpdateStmt;

#if !defined(SQLITE_AMALGAMATION)
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef sqlite3_int64 i64;
#endif

/*
** These values must match the values defined in wal.c for the equivalent
** locks. These are not magic numbers as they are part of the SQLite file
** format.
*/
#define WAL_LOCK_WRITE  0
#define WAL_LOCK_CKPT   1
#define WAL_LOCK_READ0  3

#define SQLITE_FCNTL_RBUCNT    5149216

/*
** A structure to store values read from the rbu_state table in memory.
*/
struct RbuState {
  int eStage;
  char *zTbl;
  char *zDataTbl;
  char *zIdx;
  i64 iWalCksum;
  int nRow;
  i64 nProgress;
  u32 iCookie;
  i64 iOalSz;
  i64 nPhaseOneStep;
};

struct RbuUpdateStmt {
  char *zMask;                    /* Copy of update mask used with pUpdate */
  sqlite3_stmt *pUpdate;          /* Last update statement (or NULL) */
  RbuUpdateStmt *pNext;
};

struct RbuSpan {
  const char *zSpan;
  int nSpan;
};

/*
** An iterator of this type is used to iterate through all objects in
** the target database that require updating. For each such table, the
** iterator visits, in order:
**
**     * the table itself, 
**     * each index of the table (zero or more points to visit), and
**     * a special "cleanup table" state.
**
** abIndexed:
**   If the table has no indexes on it, abIndexed is set to NULL. Otherwise,
**   it points to an array of flags nTblCol elements in size. The flag is
**   set for each column that is either a part of the PK or a part of an
**   index. Or clear otherwise.
**
**   If there are one or more partial indexes on the table, all fields of
**   this array set set to 1. This is because in that case, the module has
**   no way to tell which fields will be required to add and remove entries
**   from the partial indexes.
**   
*/
struct RbuObjIter {
  sqlite3_stmt *pTblIter;         /* Iterate through tables */
  sqlite3_stmt *pIdxIter;         /* Index iterator */
  int nTblCol;                    /* Size of azTblCol[] array */
  char **azTblCol;                /* Array of unquoted target column names */
  char **azTblType;               /* Array of target column types */
  int *aiSrcOrder;                /* src table col -> target table col */
  u8 *abTblPk;                    /* Array of flags, set on target PK columns */
  u8 *abNotNull;                  /* Array of flags, set on NOT NULL columns */
  u8 *abIndexed;                  /* Array of flags, set on indexed & PK cols */
  int eType;                      /* Table type - an RBU_PK_XXX value */

  /* Output variables. zTbl==0 implies EOF. */
  int bCleanup;                   /* True in "cleanup" state */
  const char *zTbl;               /* Name of target db table */
  const char *zDataTbl;           /* Name of rbu db table (or null) */
  const char *zIdx;               /* Name of target db index (or null) */
  int iTnum;                      /* Root page of current object */
  int iPkTnum;                    /* If eType==EXTERNAL, root of PK index */
  int bUnique;                    /* Current index is unique */
  int nIndex;                     /* Number of aux. indexes on table zTbl */

  /* Statements created by rbuObjIterPrepareAll() */
  int nCol;                       /* Number of columns in current object */
  sqlite3_stmt *pSelect;          /* Source data */
  sqlite3_stmt *pInsert;          /* Statement for INSERT operations */
  sqlite3_stmt *pDelete;          /* Statement for DELETE ops */
  sqlite3_stmt *pTmpInsert;       /* Insert into rbu_tmp_$zDataTbl */
  int nIdxCol;
  RbuSpan *aIdxCol;
  char *zIdxSql;

  /* Last UPDATE used (for PK b-tree updates only), or NULL. */
  RbuUpdateStmt *pRbuUpdate;
};

/*
** Values for RbuObjIter.eType
**
**     0: Table does not exist (error)
**     1: Table has an implicit rowid.
**     2: Table has an explicit IPK column.
**     3: Table has an external PK index.
**     4: Table is WITHOUT ROWID.
**     5: Table is a virtual table.
*/
#define RBU_PK_NOTABLE        0
#define RBU_PK_NONE           1
#define RBU_PK_IPK            2
#define RBU_PK_EXTERNAL       3
#define RBU_PK_WITHOUT_ROWID  4
#define RBU_PK_VTAB           5


/*
** Within the RBU_STAGE_OAL stage, each call to sqlite3rbu_step() performs
** one of the following operations.
*/
#define RBU_INSERT     1          /* Insert on a main table b-tree */
#define RBU_DELETE     2          /* Delete a row from a main table b-tree */
#define RBU_REPLACE    3          /* Delete and then insert a row */
#define RBU_IDX_DELETE 4          /* Delete a row from an aux. index b-tree */
#define RBU_IDX_INSERT 5          /* Insert on an aux. index b-tree */

#define RBU_UPDATE     6          /* Update a row in a main table b-tree */

/*
** A single step of an incremental checkpoint - frame iWalFrame of the wal
** file should be copied to page iDbPage of the database file.
*/
struct RbuFrame {
  u32 iDbPage;
  u32 iWalFrame;
};

/*
** RBU handle.
**
** nPhaseOneStep:
**   If the RBU database contains an rbu_count table, this value is set to
**   a running estimate of the number of b-tree operations required to 
**   finish populating the *-oal file. This allows the sqlite3_bp_progress()
**   API to calculate the permyriadage progress of populating the *-oal file
**   using the formula:
**
**     permyriadage = (10000 * nProgress) / nPhaseOneStep
**
**   nPhaseOneStep is initialized to the sum of:
**
**     nRow * (nIndex + 1)
**
**   for all source tables in the RBU database, where nRow is the number
**   of rows in the source table and nIndex the number of indexes on the
**   corresponding target database table.
**
**   This estimate is accurate if the RBU update consists entirely of
**   INSERT operations. However, it is inaccurate if:
**
**     * the RBU update contains any UPDATE operations. If the PK specified
**       for an UPDATE operation does not exist in the target table, then
**       no b-tree operations are required on index b-trees. Or if the 
**       specified PK does exist, then (nIndex*2) such operations are
**       required (one delete and one insert on each index b-tree).
**
**     * the RBU update contains any DELETE operations for which the specified
**       PK does not exist. In this case no operations are required on index
**       b-trees.
**
**     * the RBU update contains REPLACE operations. These are similar to
**       UPDATE operations.
**
**   nPhaseOneStep is updated to account for the conditions above during the
**   first pass of each source table. The updated nPhaseOneStep value is
**   stored in the rbu_state table if the RBU update is suspended.
*/
struct sqlite3rbu {
  int eStage;                     /* Value of RBU_STATE_STAGE field */
  sqlite3 *dbMain;                /* target database handle */
  sqlite3 *dbRbu;                 /* rbu database handle */
  char *zTarget;                  /* Path to target db */
  char *zRbu;                     /* Path to rbu db */
  char *zState;                   /* Path to state db (or NULL if zRbu) */
  char zStateDb[5];               /* Db name for state ("stat" or "main") */
  int rc;                         /* Value returned by last rbu_step() call */
  char *zErrmsg;                  /* Error message if rc!=SQLITE_OK */
  int nStep;                      /* Rows processed for current object */
  int nProgress;                  /* Rows processed for all objects */
  RbuObjIter objiter;             /* Iterator for skipping through tbl/idx */
  const char *zVfsName;           /* Name of automatically created rbu vfs */
  rbu_file *pTargetFd;            /* File handle open on target db */
  int nPagePerSector;             /* Pages per sector for pTargetFd */
  i64 iOalSz;
  i64 nPhaseOneStep;

  /* The following state variables are used as part of the incremental
  ** checkpoint stage (eStage==RBU_STAGE_CKPT). See comments surrounding
  ** function rbuSetupCheckpoint() for details.  */
  u32 iMaxFrame;                  /* Largest iWalFrame value in aFrame[] */
  u32 mLock;
  int nFrame;                     /* Entries in aFrame[] array */
  int nFrameAlloc;                /* Allocated size of aFrame[] array */
  RbuFrame *aFrame;
  int pgsz;
  u8 *aBuf;
  i64 iWalCksum;
  i64 szTemp;                     /* Current size of all temp files in use */
  i64 szTempLimit;                /* Total size limit for temp files */

  /* Used in RBU vacuum mode only */
  int nRbu;                       /* Number of RBU VFS in the stack */
  rbu_file *pRbuFd;               /* Fd for main db of dbRbu */
};

/*
** An rbu VFS is implemented using an instance of this structure.
**
** Variable pRbu is only non-NULL for automatically created RBU VFS objects.
** It is NULL for RBU VFS objects created explicitly using
** sqlite3rbu_create_vfs(). It is used to track the total amount of temp
** space used by the RBU handle.
*/
struct rbu_vfs {
  sqlite3_vfs base;               /* rbu VFS shim methods */
  sqlite3_vfs *pRealVfs;          /* Underlying VFS */
  sqlite3_mutex *mutex;           /* Mutex to protect pMain */
  sqlite3rbu *pRbu;               /* Owner RBU object */
  rbu_file *pMain;                /* List of main db files */
  rbu_file *pMainRbu;             /* List of main db files with pRbu!=0 */
};

/*
** Each file opened by an rbu VFS is represented by an instance of
** the following structure.
**
** If this is a temporary file (pRbu!=0 && flags&DELETE_ON_CLOSE), variable
** "sz" is set to the current size of the database file.
*/
struct rbu_file {
  sqlite3_file base;              /* sqlite3_file methods */
  sqlite3_file *pReal;            /* Underlying file handle */
  rbu_vfs *pRbuVfs;               /* Pointer to the rbu_vfs object */
  sqlite3rbu *pRbu;               /* Pointer to rbu object (rbu target only) */
  i64 sz;                         /* Size of file in bytes (temp only) */

  int openFlags;                  /* Flags this file was opened with */
  u32 iCookie;                    /* Cookie value for main db files */
  u8 iWriteVer;                   /* "write-version" value for main db files */
  u8 bNolock;                     /* True to fail EXCLUSIVE locks */

  int nShm;                       /* Number of entries in apShm[] array */
  char **apShm;                   /* Array of mmap'd *-shm regions */
  char *zDel;                     /* Delete this when closing file */

  const char *zWal;               /* Wal filename for this main db file */
  rbu_file *pWalFd;               /* Wal file descriptor for this main db */
  rbu_file *pMainNext;            /* Next MAIN_DB file */
  rbu_file *pMainRbuNext;         /* Next MAIN_DB file with pRbu!=0 */
};

/*
** True for an RBU vacuum handle, or false otherwise.
*/
#define rbuIsVacuum(p) ((p)->zTarget==0)


/*************************************************************************
** The following three functions, found below:
**
**   rbuDeltaGetInt()
**   rbuDeltaChecksum()
**   rbuDeltaApply()
**
** are lifted from the fossil source code (http://fossil-scm.org). They
** are used to implement the scalar SQL function rbu_fossil_delta().
*/

/*
** Read bytes from *pz and convert them into a positive integer.  When
** finished, leave *pz pointing to the first character past the end of
** the integer.  The *pLen parameter holds the length of the string
** in *pz and is decremented once for each character in the integer.
*/
static unsigned int rbuDeltaGetInt(const char **pz, int *pLen){
  static const signed char zValue[] = {
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7,    8,  9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, 16,   17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32,   33, 34, 35, -1, -1, -1, -1, 36,
    -1, 37, 38, 39, 40, 41, 42, 43,   44, 45, 46, 47, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 58, 59,   60, 61, 62, -1, -1, -1, 63, -1,
  };
  unsigned int v = 0;
  int c;
  unsigned char *z = (unsigned char*)*pz;
  unsigned char *zStart = z;
  while( (c = zValue[0x7f&*(z++)])>=0 ){
     v = (v<<6) + c;
  }
  z--;
  *pLen -= z - zStart;
  *pz = (char*)z;
  return v;
}

#if RBU_ENABLE_DELTA_CKSUM
/*
** Compute a 32-bit checksum on the N-byte buffer.  Return the result.
*/
static unsigned int rbuDeltaChecksum(const char *zIn, size_t N){
  const unsigned char *z = (const unsigned char *)zIn;
  unsigned sum0 = 0;
  unsigned sum1 = 0;
  unsigned sum2 = 0;
  unsigned sum3 = 0;
  while(N >= 16){
    sum0 += ((unsigned)z[0] + z[4] + z[8] + z[12]);
    sum1 += ((unsigned)z[1] + z[5] + z[9] + z[13]);
    sum2 += ((unsigned)z[2] + z[6] + z[10]+ z[14]);
    sum3 += ((unsigned)z[3] + z[7] + z[11]+ z[15]);
    z += 16;
    N -= 16;
  }
  while(N >= 4){
    sum0 += z[0];
    sum1 += z[1];
    sum2 += z[2];
    sum3 += z[3];
    z += 4;
    N -= 4;
  }
  sum3 += (sum2 << 8) + (sum1 << 16) + (sum0 << 24);
  switch(N){
    case 3:   sum3 += (z[2] << 8);
    case 2:   sum3 += (z[1] << 16);
    case 1:   sum3 += (z[0] << 24);
    default:  ;
  }
  return sum3;
}
#endif

/*
** Apply a delta.
**
** The output buffer should be big enough to hold the whole output
** file and a NUL terminator at the end.  The delta_output_size()
** routine will determine this size for you.
**
** The delta string should be null-terminated.  But the delta string
** may contain embedded NUL characters (if the input and output are
** binary files) so we also have to pass in the length of the delta in
** the lenDelta parameter.
**
** This function returns the size of the output file in bytes (excluding
** the final NUL terminator character).  Except, if the delta string is
** malformed or intended for use with a source file other than zSrc,
** then this routine returns -1.
**
** Refer to the delta_create() documentation above for a description
** of the delta file format.
*/
static int rbuDeltaApply(
  const char *zSrc,      /* The source or pattern file */
  int lenSrc,            /* Length of the source file */
  const char *zDelta,    /* Delta to apply to the pattern */
  int lenDelta,          /* Length of the delta */
  char *zOut             /* Write the output into this preallocated buffer */
){
  unsigned int limit;
  unsigned int total = 0;
#if RBU_ENABLE_DELTA_CKSUM
  char *zOrigOut = zOut;
#endif

  limit = rbuDeltaGetInt(&zDelta, &lenDelta);
  if( *zDelta!='\n' ){
    /* ERROR: size integer not terminated by "\n" */
    return -1;
  }
  zDelta++; lenDelta--;
  while( *zDelta && lenDelta>0 ){
    unsigned int cnt, ofst;
    cnt = rbuDeltaGetInt(&zDelta, &lenDelta);
    switch( zDelta[0] ){
      case '@': {
        zDelta++; lenDelta--;
        ofst = rbuDeltaGetInt(&zDelta, &lenDelta);
        if( lenDelta>0 && zDelta[0]!=',' ){
          /* ERROR: copy command not terminated by ',' */
          return -1;
        }
        zDelta++; lenDelta--;
        total += cnt;
        if( total>limit ){
          /* ERROR: copy exceeds output file size */
          return -1;
        }
        if( (int)(ofst+cnt) > lenSrc ){
          /* ERROR: copy extends past end of input */
          return -1;
        }
        memcpy(zOut, &zSrc[ofst], cnt);
        zOut += cnt;
        break;
      }
      case ':': {
        zDelta++; lenDelta--;
        total += cnt;
        if( total>limit ){
          /* ERROR:  insert command gives an output larger than predicted */
          return -1;
        }
        if( (int)cnt>lenDelta ){
          /* ERROR: insert count exceeds size of delta */
          return -1;
        }
        memcpy(zOut, zDelta, cnt);
        zOut += cnt;
        zDelta += cnt;
        lenDelta -= cnt;
        break;
      }
      case ';': {
        zDelta++; lenDelta--;
        zOut[0] = 0;
#if RBU_ENABLE_DELTA_CKSUM
        if( cnt!=rbuDeltaChecksum(zOrigOut, total) ){
          /* ERROR:  bad checksum */
          return -1;
        }
#endif
        if( total!=limit ){
          /* ERROR: generated size does not match predicted size */
          return -1;
        }
        return total;
      }
      default: {
        /* ERROR: unknown delta operator */
        return -1;
      }
    }
  }
  /* ERROR: unterminated delta */
  return -1;
}

static int rbuDeltaOutputSize(const char *zDelta, int lenDelta){
  int size;
  size = rbuDeltaGetInt(&zDelta, &lenDelta);
  if( *zDelta!='\n' ){
    /* ERROR: size integer not terminated by "\n" */
    return -1;
  }
  return size;
}

/*
** End of code taken from fossil.
*************************************************************************/

/*
** Implementation of SQL scalar function rbu_fossil_delta().
**
** This function applies a fossil delta patch to a blob. Exactly two
** arguments must be passed to this function. The first is the blob to
** patch and the second the patch to apply. If no error occurs, this
** function returns the patched blob.
*/
static void rbuFossilDeltaFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const char *aDelta;
  int nDelta;
  const char *aOrig;
  int nOrig;

  int nOut;
  int nOut2;
  char *aOut;

  assert( argc==2 );

  nOrig = sqlite3_value_bytes(argv[0]);
  aOrig = (const char*)sqlite3_value_blob(argv[0]);
  nDelta = sqlite3_value_bytes(argv[1]);
  aDelta = (const char*)sqlite3_value_blob(argv[1]);

  /* Figure out the size of the output */
  nOut = rbuDeltaOutputSize(aDelta, nDelta);
  if( nOut<0 ){
    sqlite3_result_error(context, "corrupt fossil delta", -1);
    return;
  }

  aOut = sqlite3_malloc(nOut+1);
  if( aOut==0 ){
    sqlite3_result_error_nomem(context);
  }else{
    nOut2 = rbuDeltaApply(aOrig, nOrig, aDelta, nDelta, aOut);
    if( nOut2!=nOut ){
      sqlite3_free(aOut);
      sqlite3_result_error(context, "corrupt fossil delta", -1);
    }else{
      sqlite3_result_blob(context, aOut, nOut, sqlite3_free);
    }
  }
}


/*
** Prepare the SQL statement in buffer zSql against database handle db.
** If successful, set *ppStmt to point to the new statement and return
** SQLITE_OK. 
**
** Otherwise, if an error does occur, set *ppStmt to NULL and return
** an SQLite error code. Additionally, set output variable *pzErrmsg to
** point to a buffer containing an error message. It is the responsibility
** of the caller to (eventually) free this buffer using sqlite3_free().
*/
static int prepareAndCollectError(
  sqlite3 *db, 
  sqlite3_stmt **ppStmt,
  char **pzErrmsg,
  const char *zSql
){
  int rc = sqlite3_prepare_v2(db, zSql, -1, ppStmt, 0);
  if( rc!=SQLITE_OK ){
    *pzErrmsg = sqlite3_mprintf("%s", sqlite3_errmsg(db));
    *ppStmt = 0;
  }
  return rc;
}

/*
** Reset the SQL statement passed as the first argument. Return a copy
** of the value returned by sqlite3_reset().
**
** If an error has occurred, then set *pzErrmsg to point to a buffer
** containing an error message. It is the responsibility of the caller
** to eventually free this buffer using sqlite3_free().
*/
static int resetAndCollectError(sqlite3_stmt *pStmt, char **pzErrmsg){
  int rc = sqlite3_reset(pStmt);
  if( rc!=SQLITE_OK ){
    *pzErrmsg = sqlite3_mprintf("%s", sqlite3_errmsg(sqlite3_db_handle(pStmt)));
  }
  return rc;
}

/*
** Unless it is NULL, argument zSql points to a buffer allocated using
** sqlite3_malloc containing an SQL statement. This function prepares the SQL
** statement against database db and frees the buffer. If statement 
** compilation is successful, *ppStmt is set to point to the new statement 
** handle and SQLITE_OK is returned. 
**
** Otherwise, if an error occurs, *ppStmt is set to NULL and an error code
** returned. In this case, *pzErrmsg may also be set to point to an error
** message. It is the responsibility of the caller to free this error message
** buffer using sqlite3_free().
**
** If argument zSql is NULL, this function assumes that an OOM has occurred.
** In this case SQLITE_NOMEM is returned and *ppStmt set to NULL.
*/
static int prepareFreeAndCollectError(
  sqlite3 *db, 
  sqlite3_stmt **ppStmt,
  char **pzErrmsg,
  char *zSql
){
  int rc;
  assert( *pzErrmsg==0 );
  if( zSql==0 ){
    rc = SQLITE_NOMEM;
    *ppStmt = 0;
  }else{
    rc = prepareAndCollectError(db, ppStmt, pzErrmsg, zSql);
    sqlite3_free(zSql);
  }
  return rc;
}

/*
** Free the RbuObjIter.azTblCol[] and RbuObjIter.abTblPk[] arrays allocated
** by an earlier call to rbuObjIterCacheTableInfo().
*/
static void rbuObjIterFreeCols(RbuObjIter *pIter){
  int i;
  for(i=0; i<pIter->nTblCol; i++){
    sqlite3_free(pIter->azTblCol[i]);
    sqlite3_free(pIter->azTblType[i]);
  }
  sqlite3_free(pIter->azTblCol);
  pIter->azTblCol = 0;
  pIter->azTblType = 0;
  pIter->aiSrcOrder = 0;
  pIter->abTblPk = 0;
  pIter->abNotNull = 0;
  pIter->nTblCol = 0;
  pIter->eType = 0;               /* Invalid value */
}

/*
** Finalize all statements and free all allocations that are specific to
** the current object (table/index pair).
*/
static void rbuObjIterClearStatements(RbuObjIter *pIter){
  RbuUpdateStmt *pUp;

  sqlite3_finalize(pIter->pSelect);
  sqlite3_finalize(pIter->pInsert);
  sqlite3_finalize(pIter->pDelete);
  sqlite3_finalize(pIter->pTmpInsert);
  pUp = pIter->pRbuUpdate;
  while( pUp ){
    RbuUpdateStmt *pTmp = pUp->pNext;
    sqlite3_finalize(pUp->pUpdate);
    sqlite3_free(pUp);
    pUp = pTmp;
  }
  sqlite3_free(pIter->aIdxCol);
  sqlite3_free(pIter->zIdxSql);
  
  pIter->pSelect = 0;
  pIter->pInsert = 0;
  pIter->pDelete = 0;
  pIter->pRbuUpdate = 0;
  pIter->pTmpInsert = 0;
  pIter->nCol = 0;
  pIter->nIdxCol = 0;
  pIter->aIdxCol = 0;
  pIter->zIdxSql = 0;
}

/*
** Clean up any resources allocated as part of the iterator object passed
** as the only argument.
*/
static void rbuObjIterFinalize(RbuObjIter *pIter){
  rbuObjIterClearStatements(pIter);
  sqlite3_finalize(pIter->pTblIter);
  sqlite3_finalize(pIter->pIdxIter);
  rbuObjIterFreeCols(pIter);
  memset(pIter, 0, sizeof(RbuObjIter));
}

/*
** Advance the iterator to the next position.
**
** If no error occurs, SQLITE_OK is returned and the iterator is left 
** pointing to the next entry. Otherwise, an error code and message is 
** left in the RBU handle passed as the first argument. A copy of the 
** error code is returned.
*/
static int rbuObjIterNext(sqlite3rbu *p, RbuObjIter *pIter){
  int rc = p->rc;
  if( rc==SQLITE_OK ){

    /* Free any SQLite statements used while processing the previous object */ 
    rbuObjIterClearStatements(pIter);
    if( pIter->zIdx==0 ){
      rc = sqlite3_exec(p->dbMain,
          "DROP TRIGGER IF EXISTS temp.rbu_insert_tr;"
          "DROP TRIGGER IF EXISTS temp.rbu_update1_tr;"
          "DROP TRIGGER IF EXISTS temp.rbu_update2_tr;"
          "DROP TRIGGER IF EXISTS temp.rbu_delete_tr;"
          , 0, 0, &p->zErrmsg
      );
    }

    if( rc==SQLITE_OK ){
      if( pIter->bCleanup ){
        rbuObjIterFreeCols(pIter);
        pIter->bCleanup = 0;
        rc = sqlite3_step(pIter->pTblIter);
        if( rc!=SQLITE_ROW ){
          rc = resetAndCollectError(pIter->pTblIter, &p->zErrmsg);
          pIter->zTbl = 0;
        }else{
          pIter->zTbl = (const char*)sqlite3_column_text(pIter->pTblIter, 0);
          pIter->zDataTbl = (const char*)sqlite3_column_text(pIter->pTblIter,1);
          rc = (pIter->zDataTbl && pIter->zTbl) ? SQLITE_OK : SQLITE_NOMEM;
        }
      }else{
        if( pIter->zIdx==0 ){
          sqlite3_stmt *pIdx = pIter->pIdxIter;
          rc = sqlite3_bind_text(pIdx, 1, pIter->zTbl, -1, SQLITE_STATIC);
        }
        if( rc==SQLITE_OK ){
          rc = sqlite3_step(pIter->pIdxIter);
          if( rc!=SQLITE_ROW ){
            rc = resetAndCollectError(pIter->pIdxIter, &p->zErrmsg);
            pIter->bCleanup = 1;
            pIter->zIdx = 0;
          }else{
            pIter->zIdx = (const char*)sqlite3_column_text(pIter->pIdxIter, 0);
            pIter->iTnum = sqlite3_column_int(pIter->pIdxIter, 1);
            pIter->bUnique = sqlite3_column_int(pIter->pIdxIter, 2);
            rc = pIter->zIdx ? SQLITE_OK : SQLITE_NOMEM;
          }
        }
      }
    }
  }

  if( rc!=SQLITE_OK ){
    rbuObjIterFinalize(pIter);
    p->rc = rc;
  }
  return rc;
}


/*
** The implementation of the rbu_target_name() SQL function. This function
** accepts one or two arguments. The first argument is the name of a table -
** the name of a table in the RBU database.  The second, if it is present, is 1
** for a view or 0 for a table. 
**
** For a non-vacuum RBU handle, if the table name matches the pattern:
**
**     data[0-9]_<name>
**
** where <name> is any sequence of 1 or more characters, <name> is returned.
** Otherwise, if the only argument does not match the above pattern, an SQL
** NULL is returned.
**
**     "data_t1"     -> "t1"
**     "data0123_t2" -> "t2"
**     "dataAB_t3"   -> NULL
**
** For an rbu vacuum handle, a copy of the first argument is returned if
** the second argument is either missing or 0 (not a view).
*/
static void rbuTargetNameFunc(
  sqlite3_context *pCtx,
  int argc,
  sqlite3_value **argv
){
  sqlite3rbu *p = sqlite3_user_data(pCtx);
  const char *zIn;
  assert( argc==1 || argc==2 );

  zIn = (const char*)sqlite3_value_text(argv[0]);
  if( zIn ){
    if( rbuIsVacuum(p) ){
      assert( argc==2 || argc==1 );
      if( argc==1 || 0==sqlite3_value_int(argv[1]) ){
        sqlite3_result_text(pCtx, zIn, -1, SQLITE_STATIC);
      }
    }else{
      if( strlen(zIn)>4 && memcmp("data", zIn, 4)==0 ){
        int i;
        for(i=4; zIn[i]>='0' && zIn[i]<='9'; i++);
        if( zIn[i]=='_' && zIn[i+1] ){
          sqlite3_result_text(pCtx, &zIn[i+1], -1, SQLITE_STATIC);
        }
      }
    }
  }
}

/*
** Initialize the iterator structure passed as the second argument.
**
** If no error occurs, SQLITE_OK is returned and the iterator is left 
** pointing to the first entry. Otherwise, an error code and message is 
** left in the RBU handle passed as the first argument. A copy of the 
** error code is returned.
*/
static int rbuObjIterFirst(sqlite3rbu *p, RbuObjIter *pIter){
  int rc;
  memset(pIter, 0, sizeof(RbuObjIter));

  rc = prepareFreeAndCollectError(p->dbRbu, &pIter->pTblIter, &p->zErrmsg, 
    sqlite3_mprintf(
      "SELECT rbu_target_name(name, type='view') AS target, name "
      "FROM sqlite_master "
      "WHERE type IN ('table', 'view') AND target IS NOT NULL "
      " %s "
      "ORDER BY name"
  , rbuIsVacuum(p) ? "AND rootpage!=0 AND rootpage IS NOT NULL" : ""));

  if( rc==SQLITE_OK ){
    rc = prepareAndCollectError(p->dbMain, &pIter->pIdxIter, &p->zErrmsg,
        "SELECT name, rootpage, sql IS NULL OR substr(8, 6)=='UNIQUE' "
        "  FROM main.sqlite_master "
        "  WHERE type='index' AND tbl_name = ?"
    );
  }

  pIter->bCleanup = 1;
  p->rc = rc;
  return rbuObjIterNext(p, pIter);
}

/*
** This is a wrapper around "sqlite3_mprintf(zFmt, ...)". If an OOM occurs,
** an error code is stored in the RBU handle passed as the first argument.
**
** If an error has already occurred (p->rc is already set to something other
** than SQLITE_OK), then this function returns NULL without modifying the
** stored error code. In this case it still calls sqlite3_free() on any 
** printf() parameters associated with %z conversions.
*/
static char *rbuMPrintf(sqlite3rbu *p, const char *zFmt, ...){
  char *zSql = 0;
  va_list ap;
  va_start(ap, zFmt);
  zSql = sqlite3_vmprintf(zFmt, ap);
  if( p->rc==SQLITE_OK ){
    if( zSql==0 ) p->rc = SQLITE_NOMEM;
  }else{
    sqlite3_free(zSql);
    zSql = 0;
  }
  va_end(ap);
  return zSql;
}

/*
** Argument zFmt is a sqlite3_mprintf() style format string. The trailing
** arguments are the usual subsitution values. This function performs
** the printf() style substitutions and executes the result as an SQL
** statement on the RBU handles database.
**
** If an error occurs, an error code and error message is stored in the
** RBU handle. If an error has already occurred when this function is
** called, it is a no-op.
*/
static int rbuMPrintfExec(sqlite3rbu *p, sqlite3 *db, const char *zFmt, ...){
  va_list ap;
  char *zSql;
  va_start(ap, zFmt);
  zSql = sqlite3_vmprintf(zFmt, ap);
  if( p->rc==SQLITE_OK ){
    if( zSql==0 ){
      p->rc = SQLITE_NOMEM;
    }else{
      p->rc = sqlite3_exec(db, zSql, 0, 0, &p->zErrmsg);
    }
  }
  sqlite3_free(zSql);
  va_end(ap);
  return p->rc;
}

/*
** Attempt to allocate and return a pointer to a zeroed block of nByte 
** bytes. 
**
** If an error (i.e. an OOM condition) occurs, return NULL and leave an 
** error code in the rbu handle passed as the first argument. Or, if an 
** error has already occurred when this function is called, return NULL 
** immediately without attempting the allocation or modifying the stored
** error code.
*/
static void *rbuMalloc(sqlite3rbu *p, sqlite3_int64 nByte){
  void *pRet = 0;
  if( p->rc==SQLITE_OK ){
    assert( nByte>0 );
    pRet = sqlite3_malloc64(nByte);
    if( pRet==0 ){
      p->rc = SQLITE_NOMEM;
    }else{
      memset(pRet, 0, nByte);
    }
  }
  return pRet;
}


/*
** Allocate and zero the pIter->azTblCol[] and abTblPk[] arrays so that
** there is room for at least nCol elements. If an OOM occurs, store an
** error code in the RBU handle passed as the first argument.
*/
static void rbuAllocateIterArrays(sqlite3rbu *p, RbuObjIter *pIter, int nCol){
  sqlite3_int64 nByte = (2*sizeof(char*) + sizeof(int) + 3*sizeof(u8)) * nCol;
  char **azNew;

  azNew = (char**)rbuMalloc(p, nByte);
  if( azNew ){
    pIter->azTblCol = azNew;
    pIter->azTblType = &azNew[nCol];
    pIter->aiSrcOrder = (int*)&pIter->azTblType[nCol];
    pIter->abTblPk = (u8*)&pIter->aiSrcOrder[nCol];
    pIter->abNotNull = (u8*)&pIter->abTblPk[nCol];
    pIter->abIndexed = (u8*)&pIter->abNotNull[nCol];
  }
}

/*
** The first argument must be a nul-terminated string. This function
** returns a copy of the string in memory obtained from sqlite3_malloc().
** It is the responsibility of the caller to eventually free this memory
** using sqlite3_free().
**
** If an OOM condition is encountered when attempting to allocate memory,
** output variable (*pRc) is set to SQLITE_NOMEM before returning. Otherwise,
** if the allocation succeeds, (*pRc) is left unchanged.
*/
static char *rbuStrndup(const char *zStr, int *pRc){
  char *zRet = 0;

  if( *pRc==SQLITE_OK ){
    if( zStr ){
      size_t nCopy = strlen(zStr) + 1;
      zRet = (char*)sqlite3_malloc64(nCopy);
      if( zRet ){
        memcpy(zRet, zStr, nCopy);
      }else{
        *pRc = SQLITE_NOMEM;
      }
    }
  }

  return zRet;
}

/*
** Finalize the statement passed as the second argument.
**
** If the sqlite3_finalize() call indicates that an error occurs, and the
** rbu handle error code is not already set, set the error code and error
** message accordingly.
*/
static void rbuFinalize(sqlite3rbu *p, sqlite3_stmt *pStmt){
  sqlite3 *db = sqlite3_db_handle(pStmt);
  int rc = sqlite3_finalize(pStmt);
  if( p->rc==SQLITE_OK && rc!=SQLITE_OK ){
    p->rc = rc;
    p->zErrmsg = sqlite3_mprintf("%s", sqlite3_errmsg(db));
  }
}

/* Determine the type of a table.
**
**   peType is of type (int*), a pointer to an output parameter of type
**   (int). This call sets the output parameter as follows, depending
**   on the type of the table specified by parameters dbName and zTbl.
**
**     RBU_PK_NOTABLE:       No such table.
**     RBU_PK_NONE:          Table has an implicit rowid.
**     RBU_PK_IPK:           Table has an explicit IPK column.
**     RBU_PK_EXTERNAL:      Table has an external PK index.
**     RBU_PK_WITHOUT_ROWID: Table is WITHOUT ROWID.
**     RBU_PK_VTAB:          Table is a virtual table.
**
**   Argument *piPk is also of type (int*), and also points to an output
**   parameter. Unless the table has an external primary key index 
**   (i.e. unless *peType is set to 3), then *piPk is set to zero. Or,
**   if the table does have an external primary key index, then *piPk
**   is set to the root page number of the primary key index before
**   returning.
**
** ALGORITHM:
**
**   if( no entry exists in sqlite_master ){
**     return RBU_PK_NOTABLE
**   }else if( sql for the entry starts with "CREATE VIRTUAL" ){
**     return RBU_PK_VTAB
**   }else if( "PRAGMA index_list()" for the table contains a "pk" index ){
**     if( the index that is the pk exists in sqlite_master ){
**       *piPK = rootpage of that index.
**       return RBU_PK_EXTERNAL
**     }else{
**       return RBU_PK_WITHOUT_ROWID
**     }
**   }else if( "PRAGMA table_info()" lists one or more "pk" columns ){
**     return RBU_PK_IPK
**   }else{
**     return RBU_PK_NONE
**   }
*/
static void rbuTableType(
  sqlite3rbu *p,
  const char *zTab,
  int *peType,
  int *piTnum,
  int *piPk
){
  /*
  ** 0) SELECT count(*) FROM sqlite_master where name=%Q AND IsVirtual(%Q)
  ** 1) PRAGMA index_list = ?
  ** 2) SELECT count(*) FROM sqlite_master where name=%Q 
  ** 3) PRAGMA table_info = ?
  */
  sqlite3_stmt *aStmt[4] = {0, 0, 0, 0};

  *peType = RBU_PK_NOTABLE;
  *piPk = 0;

  assert( p->rc==SQLITE_OK );
  p->rc = prepareFreeAndCollectError(p->dbMain, &aStmt[0], &p->zErrmsg, 
    sqlite3_mprintf(
          "SELECT (sql LIKE 'create virtual%%'), rootpage"
          "  FROM sqlite_master"
          " WHERE name=%Q", zTab
  ));
  if( p->rc!=SQLITE_OK || sqlite3_step(aStmt[0])!=SQLITE_ROW ){
    /* Either an error, or no such table. */
    goto rbuTableType_end;
  }
  if( sqlite3_column_int(aStmt[0], 0) ){
    *peType = RBU_PK_VTAB;                     /* virtual table */
    goto rbuTableType_end;
  }
  *piTnum = sqlite3_column_int(aStmt[0], 1);

  p->rc = prepareFreeAndCollectError(p->dbMain, &aStmt[1], &p->zErrmsg, 
    sqlite3_mprintf("PRAGMA index_list=%Q",zTab)
  );
  if( p->rc ) goto rbuTableType_end;
  while( sqlite3_step(aStmt[1])==SQLITE_ROW ){
    const u8 *zOrig = sqlite3_column_text(aStmt[1], 3);
    const u8 *zIdx = sqlite3_column_text(aStmt[1], 1);
    if( zOrig && zIdx && zOrig[0]=='p' ){
      p->rc = prepareFreeAndCollectError(p->dbMain, &aStmt[2], &p->zErrmsg, 
          sqlite3_mprintf(
            "SELECT rootpage FROM sqlite_master WHERE name = %Q", zIdx
      ));
      if( p->rc==SQLITE_OK ){
        if( sqlite3_step(aStmt[2])==SQLITE_ROW ){
          *piPk = sqlite3_column_int(aStmt[2], 0);
          *peType = RBU_PK_EXTERNAL;
        }else{
          *peType = RBU_PK_WITHOUT_ROWID;
        }
      }
      goto rbuTableType_end;
    }
  }

  p->rc = prepareFreeAndCollectError(p->dbMain, &aStmt[3], &p->zErrmsg, 
    sqlite3_mprintf("PRAGMA table_info=%Q",zTab)
  );
  if( p->rc==SQLITE_OK ){
    while( sqlite3_step(aStmt[3])==SQLITE_ROW ){
      if( sqlite3_column_int(aStmt[3],5)>0 ){
        *peType = RBU_PK_IPK;                /* explicit IPK column */
        goto rbuTableType_end;
      }
    }
    *peType = RBU_PK_NONE;
  }

rbuTableType_end: {
    unsigned int i;
    for(i=0; i<sizeof(aStmt)/sizeof(aStmt[0]); i++){
      rbuFinalize(p, aStmt[i]);
    }
  }
}

/*
** This is a helper function for rbuObjIterCacheTableInfo(). It populates
** the pIter->abIndexed[] array.
*/
static void rbuObjIterCacheIndexedCols(sqlite3rbu *p, RbuObjIter *pIter){
  sqlite3_stmt *pList = 0;
  int bIndex = 0;

  if( p->rc==SQLITE_OK ){
    memcpy(pIter->abIndexed, pIter->abTblPk, sizeof(u8)*pIter->nTblCol);
    p->rc = prepareFreeAndCollectError(p->dbMain, &pList, &p->zErrmsg,
        sqlite3_mprintf("PRAGMA main.index_list = %Q", pIter->zTbl)
    );
  }

  pIter->nIndex = 0;
  while( p->rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pList) ){
    const char *zIdx = (const char*)sqlite3_column_text(pList, 1);
    int bPartial = sqlite3_column_int(pList, 4);
    sqlite3_stmt *pXInfo = 0;
    if( zIdx==0 ) break;
    if( bPartial ){
      memset(pIter->abIndexed, 0x01, sizeof(u8)*pIter->nTblCol);
    }
    p->rc = prepareFreeAndCollectError(p->dbMain, &pXInfo, &p->zErrmsg,
        sqlite3_mprintf("PRAGMA main.index_xinfo = %Q", zIdx)
    );
    while( p->rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pXInfo) ){
      int iCid = sqlite3_column_int(pXInfo, 1);
      if( iCid>=0 ) pIter->abIndexed[iCid] = 1;
      if( iCid==-2 ){
        memset(pIter->abIndexed, 0x01, sizeof(u8)*pIter->nTblCol);
      }
    }
    rbuFinalize(p, pXInfo);
    bIndex = 1;
    pIter->nIndex++;
  }

  if( pIter->eType==RBU_PK_WITHOUT_ROWID ){
    /* "PRAGMA index_list" includes the main PK b-tree */
    pIter->nIndex--;
  }

  rbuFinalize(p, pList);
  if( bIndex==0 ) pIter->abIndexed = 0;
}


/*
** If they are not already populated, populate the pIter->azTblCol[],
** pIter->abTblPk[], pIter->nTblCol and pIter->bRowid variables according to
** the table (not index) that the iterator currently points to.
**
** Return SQLITE_OK if successful, or an SQLite error code otherwise. If
** an error does occur, an error code and error message are also left in 
** the RBU handle.
*/
static int rbuObjIterCacheTableInfo(sqlite3rbu *p, RbuObjIter *pIter){
  if( pIter->azTblCol==0 ){
    sqlite3_stmt *pStmt = 0;
    int nCol = 0;
    int i;                        /* for() loop iterator variable */
    int bRbuRowid = 0;            /* If input table has column "rbu_rowid" */
    int iOrder = 0;
    int iTnum = 0;

    /* Figure out the type of table this step will deal with. */
    assert( pIter->eType==0 );
    rbuTableType(p, pIter->zTbl, &pIter->eType, &iTnum, &pIter->iPkTnum);
    if( p->rc==SQLITE_OK && pIter->eType==RBU_PK_NOTABLE ){
      p->rc = SQLITE_ERROR;
      p->zErrmsg = sqlite3_mprintf("no such table: %s", pIter->zTbl);
    }
    if( p->rc ) return p->rc;
    if( pIter->zIdx==0 ) pIter->iTnum = iTnum;

    assert( pIter->eType==RBU_PK_NONE || pIter->eType==RBU_PK_IPK 
         || pIter->eType==RBU_PK_EXTERNAL || pIter->eType==RBU_PK_WITHOUT_ROWID
         || pIter->eType==RBU_PK_VTAB
    );

    /* Populate the azTblCol[] and nTblCol variables based on the columns
    ** of the input table. Ignore any input table columns that begin with
    ** "rbu_".  */
    p->rc = prepareFreeAndCollectError(p->dbRbu, &pStmt, &p->zErrmsg, 
        sqlite3_mprintf("SELECT * FROM '%q'", pIter->zDataTbl)
    );
    if( p->rc==SQLITE_OK ){
      nCol = sqlite3_column_count(pStmt);
      rbuAllocateIterArrays(p, pIter, nCol);
    }
    for(i=0; p->rc==SQLITE_OK && i<nCol; i++){
      const char *zName = (const char*)sqlite3_column_name(pStmt, i);
      if( sqlite3_strnicmp("rbu_", zName, 4) ){
        char *zCopy = rbuStrndup(zName, &p->rc);
        pIter->aiSrcOrder[pIter->nTblCol] = pIter->nTblCol;
        pIter->azTblCol[pIter->nTblCol++] = zCopy;
      }
      else if( 0==sqlite3_stricmp("rbu_rowid", zName) ){
        bRbuRowid = 1;
      }
    }
    sqlite3_finalize(pStmt);
    pStmt = 0;

    if( p->rc==SQLITE_OK
     && rbuIsVacuum(p)==0
     && bRbuRowid!=(pIter->eType==RBU_PK_VTAB || pIter->eType==RBU_PK_NONE)
    ){
      p->rc = SQLITE_ERROR;
      p->zErrmsg = sqlite3_mprintf(
          "table %q %s rbu_rowid column", pIter->zDataTbl,
          (bRbuRowid ? "may not have" : "requires")
      );
    }

    /* Check that all non-HIDDEN columns in the destination table are also
    ** present in the input table. Populate the abTblPk[], azTblType[] and
    ** aiTblOrder[] arrays at the same time.  */
    if( p->rc==SQLITE_OK ){
      p->rc = prepareFreeAndCollectError(p->dbMain, &pStmt, &p->zErrmsg, 
          sqlite3_mprintf("PRAGMA table_info(%Q)", pIter->zTbl)
      );
    }
    while( p->rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pStmt) ){
      const char *zName = (const char*)sqlite3_column_text(pStmt, 1);
      if( zName==0 ) break;  /* An OOM - finalize() below returns S_NOMEM */
      for(i=iOrder; i<pIter->nTblCol; i++){
        if( 0==strcmp(zName, pIter->azTblCol[i]) ) break;
      }
      if( i==pIter->nTblCol ){
        p->rc = SQLITE_ERROR;
        p->zErrmsg = sqlite3_mprintf("column missing from %q: %s",
            pIter->zDataTbl, zName
        );
      }else{
        int iPk = sqlite3_column_int(pStmt, 5);
        int bNotNull = sqlite3_column_int(pStmt, 3);
        const char *zType = (const char*)sqlite3_column_text(pStmt, 2);

        if( i!=iOrder ){
          SWAP(int, pIter->aiSrcOrder[i], pIter->aiSrcOrder[iOrder]);
          SWAP(char*, pIter->azTblCol[i], pIter->azTblCol[iOrder]);
        }

        pIter->azTblType[iOrder] = rbuStrndup(zType, &p->rc);
        assert( iPk>=0 );
        pIter->abTblPk[iOrder] = (u8)iPk;
        pIter->abNotNull[iOrder] = (u8)bNotNull || (iPk!=0);
        iOrder++;
      }
    }

    rbuFinalize(p, pStmt);
    rbuObjIterCacheIndexedCols(p, pIter);
    assert( pIter->eType!=RBU_PK_VTAB || pIter->abIndexed==0 );
    assert( pIter->eType!=RBU_PK_VTAB || pIter->nIndex==0 );
  }

  return p->rc;
}

/*
** This function constructs and returns a pointer to a nul-terminated 
** string containing some SQL clause or list based on one or more of the 
** column names currently stored in the pIter->azTblCol[] array.
*/
static char *rbuObjIterGetCollist(
  sqlite3rbu *p,                  /* RBU object */
  RbuObjIter *pIter               /* Object iterator for column names */
){
  char *zList = 0;
  const char *zSep = "";
  int i;
  for(i=0; i<pIter->nTblCol; i++){
    const char *z = pIter->azTblCol[i];
    zList = rbuMPrintf(p, "%z%s\"%w\"", zList, zSep, z);
    zSep = ", ";
  }
  return zList;
}

/*
** Return a comma separated list of the quoted PRIMARY KEY column names,
** in order, for the current table. Before each column name, add the text
** zPre. After each column name, add the zPost text. Use zSeparator as
** the separator text (usually ", ").
*/
static char *rbuObjIterGetPkList(
  sqlite3rbu *p,                  /* RBU object */
  RbuObjIter *pIter,              /* Object iterator for column names */
  const char *zPre,               /* Before each quoted column name */
  const char *zSeparator,         /* Separator to use between columns */
  const char *zPost               /* After each quoted column name */
){
  int iPk = 1;
  char *zRet = 0;
  const char *zSep = "";
  while( 1 ){
    int i;
    for(i=0; i<pIter->nTblCol; i++){
      if( (int)pIter->abTblPk[i]==iPk ){
        const char *zCol = pIter->azTblCol[i];
        zRet = rbuMPrintf(p, "%z%s%s\"%w\"%s", zRet, zSep, zPre, zCol, zPost);
        zSep = zSeparator;
        break;
      }
    }
    if( i==pIter->nTblCol ) break;
    iPk++;
  }
  return zRet;
}

/*
** This function is called as part of restarting an RBU vacuum within 
** stage 1 of the process (while the *-oal file is being built) while
** updating a table (not an index). The table may be a rowid table or
** a WITHOUT ROWID table. It queries the target database to find the 
** largest key that has already been written to the target table and
** constructs a WHERE clause that can be used to extract the remaining
** rows from the source table. For a rowid table, the WHERE clause
** is of the form:
**
**     "WHERE _rowid_ > ?"
**
** and for WITHOUT ROWID tables:
**
**     "WHERE (key1, key2) > (?, ?)"
**
** Instead of "?" placeholders, the actual WHERE clauses created by
** this function contain literal SQL values.
*/
static char *rbuVacuumTableStart(
  sqlite3rbu *p,                  /* RBU handle */
  RbuObjIter *pIter,              /* RBU iterator object */
  int bRowid,                     /* True for a rowid table */
  const char *zWrite              /* Target table name prefix */
){
  sqlite3_stmt *pMax = 0;
  char *zRet = 0;
  if( bRowid ){
    p->rc = prepareFreeAndCollectError(p->dbMain, &pMax, &p->zErrmsg, 
        sqlite3_mprintf(
          "SELECT max(_rowid_) FROM \"%s%w\"", zWrite, pIter->zTbl
        )
    );
    if( p->rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pMax) ){
      sqlite3_int64 iMax = sqlite3_column_int64(pMax, 0);
      zRet = rbuMPrintf(p, " WHERE _rowid_ > %lld ", iMax);
    }
    rbuFinalize(p, pMax);
  }else{
    char *zOrder = rbuObjIterGetPkList(p, pIter, "", ", ", " DESC");
    char *zSelect = rbuObjIterGetPkList(p, pIter, "quote(", "||','||", ")");
    char *zList = rbuObjIterGetPkList(p, pIter, "", ", ", "");

    if( p->rc==SQLITE_OK ){
      p->rc = prepareFreeAndCollectError(p->dbMain, &pMax, &p->zErrmsg, 
          sqlite3_mprintf(
            "SELECT %s FROM \"%s%w\" ORDER BY %s LIMIT 1", 
                zSelect, zWrite, pIter->zTbl, zOrder
          )
      );
      if( p->rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pMax) ){
        const char *zVal = (const char*)sqlite3_column_text(pMax, 0);
        zRet = rbuMPrintf(p, " WHERE (%s) > (%s) ", zList, zVal);
      }
      rbuFinalize(p, pMax);
    }

    sqlite3_free(zOrder);
    sqlite3_free(zSelect);
    sqlite3_free(zList);
  }
  return zRet;
}

/*
** This function is called as part of restating an RBU vacuum when the
** current operation is writing content to an index. If possible, it
** queries the target index b-tree for the largest key already written to
** it, then composes and returns an expression that can be used in a WHERE 
** clause to select the remaining required rows from the source table. 
** It is only possible to return such an expression if:
**
**   * The index contains no DESC columns, and
**   * The last key written to the index before the operation was 
**     suspended does not contain any NULL values.
**
** The expression is of the form:
**
**   (index-field1, index-field2, ...) > (?, ?, ...)
**
** except that the "?" placeholders are replaced with literal values.
**
** If the expression cannot be created, NULL is returned. In this case,
** the caller has to use an OFFSET clause to extract only the required 
** rows from the sourct table, just as it does for an RBU update operation.
*/
char *rbuVacuumIndexStart(
  sqlite3rbu *p,                  /* RBU handle */
  RbuObjIter *pIter               /* RBU iterator object */
){
  char *zOrder = 0;
  char *zLhs = 0;
  char *zSelect = 0;
  char *zVector = 0;
  char *zRet = 0;
  int bFailed = 0;
  const char *zSep = "";
  int iCol = 0;
  sqlite3_stmt *pXInfo = 0;

  p->rc = prepareFreeAndCollectError(p->dbMain, &pXInfo, &p->zErrmsg,
      sqlite3_mprintf("PRAGMA main.index_xinfo = %Q", pIter->zIdx)
  );
  while( p->rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pXInfo) ){
    int iCid = sqlite3_column_int(pXInfo, 1);
    const char *zCollate = (const char*)sqlite3_column_text(pXInfo, 4);
    const char *zCol;
    if( sqlite3_column_int(pXInfo, 3) ){
      bFailed = 1;
      break;
    }

    if( iCid<0 ){
      if( pIter->eType==RBU_PK_IPK ){
        int i;
        for(i=0; pIter->abTblPk[i]==0; i++);
        assert( i<pIter->nTblCol );
        zCol = pIter->azTblCol[i];
      }else{
        zCol = "_rowid_";
      }
    }else{
      zCol = pIter->azTblCol[iCid];
    }

    zLhs = rbuMPrintf(p, "%z%s \"%w\" COLLATE %Q",
        zLhs, zSep, zCol, zCollate
        );
    zOrder = rbuMPrintf(p, "%z%s \"rbu_imp_%d%w\" COLLATE %Q DESC",
        zOrder, zSep, iCol, zCol, zCollate
        );
    zSelect = rbuMPrintf(p, "%z%s quote(\"rbu_imp_%d%w\")",
        zSelect, zSep, iCol, zCol
        );
    zSep = ", ";
    iCol++;
  }
  rbuFinalize(p, pXInfo);
  if( bFailed ) goto index_start_out;

  if( p->rc==SQLITE_OK ){
    sqlite3_stmt *pSel = 0;

    p->rc = prepareFreeAndCollectError(p->dbMain, &pSel, &p->zErrmsg,
        sqlite3_mprintf("SELECT %s FROM \"rbu_imp_%w\" ORDER BY %s LIMIT 1",
          zSelect, pIter->zTbl, zOrder
        )
    );
    if( p->rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pSel) ){
      zSep = "";
      for(iCol=0; iCol<pIter->nCol; iCol++){
        const char *zQuoted = (const char*)sqlite3_column_text(pSel, iCol);
        if( zQuoted[0]=='N' ){
          bFailed = 1;
          break;
        }
        zVector = rbuMPrintf(p, "%z%s%s", zVector, zSep, zQuoted);
        zSep = ", ";
      }

      if( !bFailed ){
        zRet = rbuMPrintf(p, "(%s) > (%s)", zLhs, zVector);
      }
    }
    rbuFinalize(p, pSel);
  }

 index_start_out:
  sqlite3_free(zOrder);
  sqlite3_free(zSelect);
  sqlite3_free(zVector);
  sqlite3_free(zLhs);
  return zRet;
}

/*
** This function is used to create a SELECT list (the list of SQL 
** expressions that follows a SELECT keyword) for a SELECT statement 
** used to read from an data_xxx or rbu_tmp_xxx table while updating the 
** index object currently indicated by the iterator object passed as the 
** second argument. A "PRAGMA index_xinfo = <idxname>" statement is used 
** to obtain the required information.
**
** If the index is of the following form:
**
**   CREATE INDEX i1 ON t1(c, b COLLATE nocase);
**
** and "t1" is a table with an explicit INTEGER PRIMARY KEY column 
** "ipk", the returned string is:
**
**   "`c` COLLATE 'BINARY', `b` COLLATE 'NOCASE', `ipk` COLLATE 'BINARY'"
**
** As well as the returned string, three other malloc'd strings are 
** returned via output parameters. As follows:
**
**   pzImposterCols: ...
**   pzImposterPk: ...
**   pzWhere: ...
*/
static char *rbuObjIterGetIndexCols(
  sqlite3rbu *p,                  /* RBU object */
  RbuObjIter *pIter,              /* Object iterator for column names */
  char **pzImposterCols,          /* OUT: Columns for imposter table */
  char **pzImposterPk,            /* OUT: Imposter PK clause */
  char **pzWhere,                 /* OUT: WHERE clause */
  int *pnBind                     /* OUT: Trbul number of columns */
){
  int rc = p->rc;                 /* Error code */
  int rc2;                        /* sqlite3_finalize() return code */
  char *zRet = 0;                 /* String to return */
  char *zImpCols = 0;             /* String to return via *pzImposterCols */
  char *zImpPK = 0;               /* String to return via *pzImposterPK */
  char *zWhere = 0;               /* String to return via *pzWhere */
  int nBind = 0;                  /* Value to return via *pnBind */
  const char *zCom = "";          /* Set to ", " later on */
  const char *zAnd = "";          /* Set to " AND " later on */
  sqlite3_stmt *pXInfo = 0;       /* PRAGMA index_xinfo = ? */

  if( rc==SQLITE_OK ){
    assert( p->zErrmsg==0 );
    rc = prepareFreeAndCollectError(p->dbMain, &pXInfo, &p->zErrmsg,
        sqlite3_mprintf("PRAGMA main.index_xinfo = %Q", pIter->zIdx)
    );
  }

  while( rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pXInfo) ){
    int iCid = sqlite3_column_int(pXInfo, 1);
    int bDesc = sqlite3_column_int(pXInfo, 3);
    const char *zCollate = (const char*)sqlite3_column_text(pXInfo, 4);
    const char *zCol = 0;
    const char *zType;

    if( iCid==-2 ){
      int iSeq = sqlite3_column_int(pXInfo, 0);
      zRet = sqlite3_mprintf("%z%s(%.*s) COLLATE %Q", zRet, zCom,
          pIter->aIdxCol[iSeq].nSpan, pIter->aIdxCol[iSeq].zSpan, zCollate
      );
      zType = "";
    }else {
      if( iCid<0 ){
        /* An integer primary key. If the table has an explicit IPK, use
        ** its name. Otherwise, use "rbu_rowid".  */
        if( pIter->eType==RBU_PK_IPK ){
          int i;
          for(i=0; pIter->abTblPk[i]==0; i++);
          assert( i<pIter->nTblCol );
          zCol = pIter->azTblCol[i];
        }else if( rbuIsVacuum(p) ){
          zCol = "_rowid_";
        }else{
          zCol = "rbu_rowid";
        }
        zType = "INTEGER";
      }else{
        zCol = pIter->azTblCol[iCid];
        zType = pIter->azTblType[iCid];
      }
      zRet = sqlite3_mprintf("%z%s\"%w\" COLLATE %Q", zRet, zCom,zCol,zCollate);
    }

    if( pIter->bUnique==0 || sqlite3_column_int(pXInfo, 5) ){
      const char *zOrder = (bDesc ? " DESC" : "");
      zImpPK = sqlite3_mprintf("%z%s\"rbu_imp_%d%w\"%s", 
          zImpPK, zCom, nBind, zCol, zOrder
      );
    }
    zImpCols = sqlite3_mprintf("%z%s\"rbu_imp_%d%w\" %s COLLATE %Q", 
        zImpCols, zCom, nBind, zCol, zType, zCollate
    );
    zWhere = sqlite3_mprintf(
        "%z%s\"rbu_imp_%d%w\" IS ?", zWhere, zAnd, nBind, zCol
    );
    if( zRet==0 || zImpPK==0 || zImpCols==0 || zWhere==0 ) rc = SQLITE_NOMEM;
    zCom = ", ";
    zAnd = " AND ";
    nBind++;
  }

  rc2 = sqlite3_finalize(pXInfo);
  if( rc==SQLITE_OK ) rc = rc2;

  if( rc!=SQLITE_OK ){
    sqlite3_free(zRet);
    sqlite3_free(zImpCols);
    sqlite3_free(zImpPK);
    sqlite3_free(zWhere);
    zRet = 0;
    zImpCols = 0;
    zImpPK = 0;
    zWhere = 0;
    p->rc = rc;
  }

  *pzImposterCols = zImpCols;
  *pzImposterPk = zImpPK;
  *pzWhere = zWhere;
  *pnBind = nBind;
  return zRet;
}

/*
** Assuming the current table columns are "a", "b" and "c", and the zObj
** paramter is passed "old", return a string of the form:
**
**     "old.a, old.b, old.b"
**
** With the column names escaped.
**
** For tables with implicit rowids - RBU_PK_EXTERNAL and RBU_PK_NONE, append
** the text ", old._rowid_" to the returned value.
*/
static char *rbuObjIterGetOldlist(
  sqlite3rbu *p, 
  RbuObjIter *pIter,
  const char *zObj
){
  char *zList = 0;
  if( p->rc==SQLITE_OK && pIter->abIndexed ){
    const char *zS = "";
    int i;
    for(i=0; i<pIter->nTblCol; i++){
      if( pIter->abIndexed[i] ){
        const char *zCol = pIter->azTblCol[i];
        zList = sqlite3_mprintf("%z%s%s.\"%w\"", zList, zS, zObj, zCol);
      }else{
        zList = sqlite3_mprintf("%z%sNULL", zList, zS);
      }
      zS = ", ";
      if( zList==0 ){
        p->rc = SQLITE_NOMEM;
        break;
      }
    }

    /* For a table with implicit rowids, append "old._rowid_" to the list. */
    if( pIter->eType==RBU_PK_EXTERNAL || pIter->eType==RBU_PK_NONE ){
      zList = rbuMPrintf(p, "%z, %s._rowid_", zList, zObj);
    }
  }
  return zList;
}

/*
** Return an expression that can be used in a WHERE clause to match the
** primary key of the current table. For example, if the table is:
**
**   CREATE TABLE t1(a, b, c, PRIMARY KEY(b, c));
**
** Return the string:
**
**   "b = ?1 AND c = ?2"
*/
static char *rbuObjIterGetWhere(
  sqlite3rbu *p, 
  RbuObjIter *pIter
){
  char *zList = 0;
  if( pIter->eType==RBU_PK_VTAB || pIter->eType==RBU_PK_NONE ){
    zList = rbuMPrintf(p, "_rowid_ = ?%d", pIter->nTblCol+1);
  }else if( pIter->eType==RBU_PK_EXTERNAL ){
    const char *zSep = "";
    int i;
    for(i=0; i<pIter->nTblCol; i++){
      if( pIter->abTblPk[i] ){
        zList = rbuMPrintf(p, "%z%sc%d=?%d", zList, zSep, i, i+1);
        zSep = " AND ";
      }
    }
    zList = rbuMPrintf(p, 
        "_rowid_ = (SELECT id FROM rbu_imposter2 WHERE %z)", zList
    );

  }else{
    const char *zSep = "";
    int i;
    for(i=0; i<pIter->nTblCol; i++){
      if( pIter->abTblPk[i] ){
        const char *zCol = pIter->azTblCol[i];
        zList = rbuMPrintf(p, "%z%s\"%w\"=?%d", zList, zSep, zCol, i+1);
        zSep = " AND ";
      }
    }
  }
  return zList;
}

/*
** The SELECT statement iterating through the keys for the current object
** (p->objiter.pSelect) currently points to a valid row. However, there
** is something wrong with the rbu_control value in the rbu_control value
** stored in the (p->nCol+1)'th column. Set the error code and error message
** of the RBU handle to something reflecting this.
*/
static void rbuBadControlError(sqlite3rbu *p){
  p->rc = SQLITE_ERROR;
  p->zErrmsg = sqlite3_mprintf("invalid rbu_control value");
}


/*
** Return a nul-terminated string containing the comma separated list of
** assignments that should be included following the "SET" keyword of
** an UPDATE statement used to update the table object that the iterator
** passed as the second argument currently points to if the rbu_control
** column of the data_xxx table entry is set to zMask.
**
** The memory for the returned string is obtained from sqlite3_malloc().
** It is the responsibility of the caller to eventually free it using
** sqlite3_free(). 
**
** If an OOM error is encountered when allocating space for the new
** string, an error code is left in the rbu handle passed as the first
** argument and NULL is returned. Or, if an error has already occurred
** when this function is called, NULL is returned immediately, without
** attempting the allocation or modifying the stored error code.
*/
static char *rbuObjIterGetSetlist(
  sqlite3rbu *p,
  RbuObjIter *pIter,
  const char *zMask
){
  char *zList = 0;
  if( p->rc==SQLITE_OK ){
    int i;

    if( (int)strlen(zMask)!=pIter->nTblCol ){
      rbuBadControlError(p);
    }else{
      const char *zSep = "";
      for(i=0; i<pIter->nTblCol; i++){
        char c = zMask[pIter->aiSrcOrder[i]];
        if( c=='x' ){
          zList = rbuMPrintf(p, "%z%s\"%w\"=?%d", 
              zList, zSep, pIter->azTblCol[i], i+1
          );
          zSep = ", ";
        }
        else if( c=='d' ){
          zList = rbuMPrintf(p, "%z%s\"%w\"=rbu_delta(\"%w\", ?%d)", 
              zList, zSep, pIter->azTblCol[i], pIter->azTblCol[i], i+1
          );
          zSep = ", ";
        }
        else if( c=='f' ){
          zList = rbuMPrintf(p, "%z%s\"%w\"=rbu_fossil_delta(\"%w\", ?%d)", 
              zList, zSep, pIter->azTblCol[i], pIter->azTblCol[i], i+1
          );
          zSep = ", ";
        }
      }
    }
  }
  return zList;
}

/*
** Return a nul-terminated string consisting of nByte comma separated
** "?" expressions. For example, if nByte is 3, return a pointer to
** a buffer containing the string "?,?,?".
**
** The memory for the returned string is obtained from sqlite3_malloc().
** It is the responsibility of the caller to eventually free it using
** sqlite3_free(). 
**
** If an OOM error is encountered when allocating space for the new
** string, an error code is left in the rbu handle passed as the first
** argument and NULL is returned. Or, if an error has already occurred
** when this function is called, NULL is returned immediately, without
** attempting the allocation or modifying the stored error code.
*/
static char *rbuObjIterGetBindlist(sqlite3rbu *p, int nBind){
  char *zRet = 0;
  sqlite3_int64 nByte = 2*(sqlite3_int64)nBind + 1;

  zRet = (char*)rbuMalloc(p, nByte);
  if( zRet ){
    int i;
    for(i=0; i<nBind; i++){
      zRet[i*2] = '?';
      zRet[i*2+1] = (i+1==nBind) ? '\0' : ',';
    }
  }
  return zRet;
}

/*
** The iterator currently points to a table (not index) of type 
** RBU_PK_WITHOUT_ROWID. This function creates the PRIMARY KEY 
** declaration for the corresponding imposter table. For example,
** if the iterator points to a table created as:
**
**   CREATE TABLE t1(a, b, c, PRIMARY KEY(b, a DESC)) WITHOUT ROWID
**
** this function returns:
**
**   PRIMARY KEY("b", "a" DESC)
*/
static char *rbuWithoutRowidPK(sqlite3rbu *p, RbuObjIter *pIter){
  char *z = 0;
  assert( pIter->zIdx==0 );
  if( p->rc==SQLITE_OK ){
    const char *zSep = "PRIMARY KEY(";
    sqlite3_stmt *pXList = 0;     /* PRAGMA index_list = (pIter->zTbl) */
    sqlite3_stmt *pXInfo = 0;     /* PRAGMA index_xinfo = <pk-index> */
   
    p->rc = prepareFreeAndCollectError(p->dbMain, &pXList, &p->zErrmsg,
        sqlite3_mprintf("PRAGMA main.index_list = %Q", pIter->zTbl)
    );
    while( p->rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pXList) ){
      const char *zOrig = (const char*)sqlite3_column_text(pXList,3);
      if( zOrig && strcmp(zOrig, "pk")==0 ){
        const char *zIdx = (const char*)sqlite3_column_text(pXList,1);
        if( zIdx ){
          p->rc = prepareFreeAndCollectError(p->dbMain, &pXInfo, &p->zErrmsg,
              sqlite3_mprintf("PRAGMA main.index_xinfo = %Q", zIdx)
          );
        }
        break;
      }
    }
    rbuFinalize(p, pXList);

    while( p->rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pXInfo) ){
      if( sqlite3_column_int(pXInfo, 5) ){
        /* int iCid = sqlite3_column_int(pXInfo, 0); */
        const char *zCol = (const char*)sqlite3_column_text(pXInfo, 2);
        const char *zDesc = sqlite3_column_int(pXInfo, 3) ? " DESC" : "";
        z = rbuMPrintf(p, "%z%s\"%w\"%s", z, zSep, zCol, zDesc);
        zSep = ", ";
      }
    }
    z = rbuMPrintf(p, "%z)", z);
    rbuFinalize(p, pXInfo);
  }
  return z;
}

/*
** This function creates the second imposter table used when writing to
** a table b-tree where the table has an external primary key. If the
** iterator passed as the second argument does not currently point to
** a table (not index) with an external primary key, this function is a
** no-op. 
**
** Assuming the iterator does point to a table with an external PK, this
** function creates a WITHOUT ROWID imposter table named "rbu_imposter2"
** used to access that PK index. For example, if the target table is
** declared as follows:
**
**   CREATE TABLE t1(a, b TEXT, c REAL, PRIMARY KEY(b, c));
**
** then the imposter table schema is:
**
**   CREATE TABLE rbu_imposter2(c1 TEXT, c2 REAL, id INTEGER) WITHOUT ROWID;
**
*/
static void rbuCreateImposterTable2(sqlite3rbu *p, RbuObjIter *pIter){
  if( p->rc==SQLITE_OK && pIter->eType==RBU_PK_EXTERNAL ){
    int tnum = pIter->iPkTnum;    /* Root page of PK index */
    sqlite3_stmt *pQuery = 0;     /* SELECT name ... WHERE rootpage = $tnum */
    const char *zIdx = 0;         /* Name of PK index */
    sqlite3_stmt *pXInfo = 0;     /* PRAGMA main.index_xinfo = $zIdx */
    const char *zComma = "";
    char *zCols = 0;              /* Used to build up list of table cols */
    char *zPk = 0;                /* Used to build up table PK declaration */

    /* Figure out the name of the primary key index for the current table.
    ** This is needed for the argument to "PRAGMA index_xinfo". Set
    ** zIdx to point to a nul-terminated string containing this name. */
    p->rc = prepareAndCollectError(p->dbMain, &pQuery, &p->zErrmsg, 
        "SELECT name FROM sqlite_master WHERE rootpage = ?"
    );
    if( p->rc==SQLITE_OK ){
      sqlite3_bind_int(pQuery, 1, tnum);
      if( SQLITE_ROW==sqlite3_step(pQuery) ){
        zIdx = (const char*)sqlite3_column_text(pQuery, 0);
      }
    }
    if( zIdx ){
      p->rc = prepareFreeAndCollectError(p->dbMain, &pXInfo, &p->zErrmsg,
          sqlite3_mprintf("PRAGMA main.index_xinfo = %Q", zIdx)
      );
    }
    rbuFinalize(p, pQuery);

    while( p->rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pXInfo) ){
      int bKey = sqlite3_column_int(pXInfo, 5);
      if( bKey ){
        int iCid = sqlite3_column_int(pXInfo, 1);
        int bDesc = sqlite3_column_int(pXInfo, 3);
        const char *zCollate = (const char*)sqlite3_column_text(pXInfo, 4);
        zCols = rbuMPrintf(p, "%z%sc%d %s COLLATE %Q", zCols, zComma, 
            iCid, pIter->azTblType[iCid], zCollate
        );
        zPk = rbuMPrintf(p, "%z%sc%d%s", zPk, zComma, iCid, bDesc?" DESC":"");
        zComma = ", ";
      }
    }
    zCols = rbuMPrintf(p, "%z, id INTEGER", zCols);
    rbuFinalize(p, pXInfo);

    sqlite3_test_control(SQLITE_TESTCTRL_IMPOSTER, p->dbMain, "main", 1, tnum);
    rbuMPrintfExec(p, p->dbMain,
        "CREATE TABLE rbu_imposter2(%z, PRIMARY KEY(%z)) WITHOUT ROWID", 
        zCols, zPk
    );
    sqlite3_test_control(SQLITE_TESTCTRL_IMPOSTER, p->dbMain, "main", 0, 0);
  }
}

/*
** If an error has already occurred when this function is called, it 
** immediately returns zero (without doing any work). Or, if an error
** occurs during the execution of this function, it sets the error code
** in the sqlite3rbu object indicated by the first argument and returns
** zero.
**
** The iterator passed as the second argument is guaranteed to point to
** a table (not an index) when this function is called. This function
** attempts to create any imposter table required to write to the main
** table b-tree of the table before returning. Non-zero is returned if
** an imposter table are created, or zero otherwise.
**
** An imposter table is required in all cases except RBU_PK_VTAB. Only
** virtual tables are written to directly. The imposter table has the 
** same schema as the actual target table (less any UNIQUE constraints). 
** More precisely, the "same schema" means the same columns, types, 
** collation sequences. For tables that do not have an external PRIMARY
** KEY, it also means the same PRIMARY KEY declaration.
*/
static void rbuCreateImposterTable(sqlite3rbu *p, RbuObjIter *pIter){
  if( p->rc==SQLITE_OK && pIter->eType!=RBU_PK_VTAB ){
    int tnum = pIter->iTnum;
    const char *zComma = "";
    char *zSql = 0;
    int iCol;
    sqlite3_test_control(SQLITE_TESTCTRL_IMPOSTER, p->dbMain, "main", 0, 1);

    for(iCol=0; p->rc==SQLITE_OK && iCol<pIter->nTblCol; iCol++){
      const char *zPk = "";
      const char *zCol = pIter->azTblCol[iCol];
      const char *zColl = 0;

      p->rc = sqlite3_table_column_metadata(
          p->dbMain, "main", pIter->zTbl, zCol, 0, &zColl, 0, 0, 0
      );

      if( pIter->eType==RBU_PK_IPK && pIter->abTblPk[iCol] ){
        /* If the target table column is an "INTEGER PRIMARY KEY", add
        ** "PRIMARY KEY" to the imposter table column declaration. */
        zPk = "PRIMARY KEY ";
      }
      zSql = rbuMPrintf(p, "%z%s\"%w\" %s %sCOLLATE %Q%s", 
          zSql, zComma, zCol, pIter->azTblType[iCol], zPk, zColl,
          (pIter->abNotNull[iCol] ? " NOT NULL" : "")
      );
      zComma = ", ";
    }

    if( pIter->eType==RBU_PK_WITHOUT_ROWID ){
      char *zPk = rbuWithoutRowidPK(p, pIter);
      if( zPk ){
        zSql = rbuMPrintf(p, "%z, %z", zSql, zPk);
      }
    }

    sqlite3_test_control(SQLITE_TESTCTRL_IMPOSTER, p->dbMain, "main", 1, tnum);
    rbuMPrintfExec(p, p->dbMain, "CREATE TABLE \"rbu_imp_%w\"(%z)%s", 
        pIter->zTbl, zSql, 
        (pIter->eType==RBU_PK_WITHOUT_ROWID ? " WITHOUT ROWID" : "")
    );
    sqlite3_test_control(SQLITE_TESTCTRL_IMPOSTER, p->dbMain, "main", 0, 0);
  }
}

/*
** Prepare a statement used to insert rows into the "rbu_tmp_xxx" table.
** Specifically a statement of the form:
**
**     INSERT INTO rbu_tmp_xxx VALUES(?, ?, ? ...);
**
** The number of bound variables is equal to the number of columns in
** the target table, plus one (for the rbu_control column), plus one more 
** (for the rbu_rowid column) if the target table is an implicit IPK or 
** virtual table.
*/
static void rbuObjIterPrepareTmpInsert(
  sqlite3rbu *p, 
  RbuObjIter *pIter,
  const char *zCollist,
  const char *zRbuRowid
){
  int bRbuRowid = (pIter->eType==RBU_PK_EXTERNAL || pIter->eType==RBU_PK_NONE);
  char *zBind = rbuObjIterGetBindlist(p, pIter->nTblCol + 1 + bRbuRowid);
  if( zBind ){
    assert( pIter->pTmpInsert==0 );
    p->rc = prepareFreeAndCollectError(
        p->dbRbu, &pIter->pTmpInsert, &p->zErrmsg, sqlite3_mprintf(
          "INSERT INTO %s.'rbu_tmp_%q'(rbu_control,%s%s) VALUES(%z)", 
          p->zStateDb, pIter->zDataTbl, zCollist, zRbuRowid, zBind
    ));
  }
}

static void rbuTmpInsertFunc(
  sqlite3_context *pCtx, 
  int nVal,
  sqlite3_value **apVal
){
  sqlite3rbu *p = sqlite3_user_data(pCtx);
  int rc = SQLITE_OK;
  int i;

  assert( sqlite3_value_int(apVal[0])!=0
      || p->objiter.eType==RBU_PK_EXTERNAL 
      || p->objiter.eType==RBU_PK_NONE 
  );
  if( sqlite3_value_int(apVal[0])!=0 ){
    p->nPhaseOneStep += p->objiter.nIndex;
  }

  for(i=0; rc==SQLITE_OK && i<nVal; i++){
    rc = sqlite3_bind_value(p->objiter.pTmpInsert, i+1, apVal[i]);
  }
  if( rc==SQLITE_OK ){
    sqlite3_step(p->objiter.pTmpInsert);
    rc = sqlite3_reset(p->objiter.pTmpInsert);
  }

  if( rc!=SQLITE_OK ){
    sqlite3_result_error_code(pCtx, rc);
  }
}

static char *rbuObjIterGetIndexWhere(sqlite3rbu *p, RbuObjIter *pIter){
  sqlite3_stmt *pStmt = 0;
  int rc = p->rc;
  char *zRet = 0;

  assert( pIter->zIdxSql==0 && pIter->nIdxCol==0 && pIter->aIdxCol==0 );

  if( rc==SQLITE_OK ){
    rc = prepareAndCollectError(p->dbMain, &pStmt, &p->zErrmsg,
        "SELECT trim(sql) FROM sqlite_master WHERE type='index' AND name=?"
    );
  }
  if( rc==SQLITE_OK ){
    int rc2;
    rc = sqlite3_bind_text(pStmt, 1, pIter->zIdx, -1, SQLITE_STATIC);
    if( rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pStmt) ){
      char *zSql = (char*)sqlite3_column_text(pStmt, 0);
      if( zSql ){
        pIter->zIdxSql = zSql = rbuStrndup(zSql, &rc);
      }
      if( zSql ){
        int nParen = 0;           /* Number of open parenthesis */
        int i;
        int iIdxCol = 0;
        int nIdxAlloc = 0;
        for(i=0; zSql[i]; i++){
          char c = zSql[i];

          /* If necessary, grow the pIter->aIdxCol[] array */
          if( iIdxCol==nIdxAlloc ){
            RbuSpan *aIdxCol = (RbuSpan*)sqlite3_realloc(
                pIter->aIdxCol, (nIdxAlloc+16)*sizeof(RbuSpan)
            );
            if( aIdxCol==0 ){
              rc = SQLITE_NOMEM;
              break;
            }
            pIter->aIdxCol = aIdxCol;
            nIdxAlloc += 16;
          }

          if( c=='(' ){
            if( nParen==0 ){
              assert( iIdxCol==0 );
              pIter->aIdxCol[0].zSpan = &zSql[i+1];
            }
            nParen++;
          }
          else if( c==')' ){
            nParen--;
            if( nParen==0 ){
              int nSpan = &zSql[i] - pIter->aIdxCol[iIdxCol].zSpan;
              pIter->aIdxCol[iIdxCol++].nSpan = nSpan;
              i++;
              break;
            }
          }else if( c==',' && nParen==1 ){
            int nSpan = &zSql[i] - pIter->aIdxCol[iIdxCol].zSpan;
            pIter->aIdxCol[iIdxCol++].nSpan = nSpan;
            pIter->aIdxCol[iIdxCol].zSpan = &zSql[i+1];
          }else if( c=='"' || c=='\'' || c=='`' ){
            for(i++; 1; i++){
              if( zSql[i]==c ){
                if( zSql[i+1]!=c ) break;
                i++;
              }
            }
          }else if( c=='[' ){
            for(i++; 1; i++){
              if( zSql[i]==']' ) break;
            }
          }else if( c=='-' && zSql[i+1]=='-' ){
            for(i=i+2; zSql[i] && zSql[i]!='\n'; i++);
            if( zSql[i]=='\0' ) break;
          }else if( c=='/' && zSql[i+1]=='*' ){
            for(i=i+2; zSql[i] && (zSql[i]!='*' || zSql[i+1]!='/'); i++);
            if( zSql[i]=='\0' ) break;
            i++;
          }
        }
        if( zSql[i] ){
          zRet = rbuStrndup(&zSql[i], &rc);
        }
        pIter->nIdxCol = iIdxCol;
      }
    }

    rc2 = sqlite3_finalize(pStmt);
    if( rc==SQLITE_OK ) rc = rc2;
  }

  p->rc = rc;
  return zRet;
}

/*
** Ensure that the SQLite statement handles required to update the 
** target database object currently indicated by the iterator passed 
** as the second argument are available.
*/
static int rbuObjIterPrepareAll(
  sqlite3rbu *p, 
  RbuObjIter *pIter,
  int nOffset                     /* Add "LIMIT -1 OFFSET $nOffset" to SELECT */
){
  assert( pIter->bCleanup==0 );
  if( pIter->pSelect==0 && rbuObjIterCacheTableInfo(p, pIter)==SQLITE_OK ){
    const int tnum = pIter->iTnum;
    char *zCollist = 0;           /* List of indexed columns */
    char **pz = &p->zErrmsg;
    const char *zIdx = pIter->zIdx;
    char *zLimit = 0;

    if( nOffset ){
      zLimit = sqlite3_mprintf(" LIMIT -1 OFFSET %d", nOffset);
      if( !zLimit ) p->rc = SQLITE_NOMEM;
    }

    if( zIdx ){
      const char *zTbl = pIter->zTbl;
      char *zImposterCols = 0;    /* Columns for imposter table */
      char *zImposterPK = 0;      /* Primary key declaration for imposter */
      char *zWhere = 0;           /* WHERE clause on PK columns */
      char *zBind = 0;
      char *zPart = 0;
      int nBind = 0;

      assert( pIter->eType!=RBU_PK_VTAB );
      zPart = rbuObjIterGetIndexWhere(p, pIter);
      zCollist = rbuObjIterGetIndexCols(
          p, pIter, &zImposterCols, &zImposterPK, &zWhere, &nBind
      );
      zBind = rbuObjIterGetBindlist(p, nBind);

      /* Create the imposter table used to write to this index. */
      sqlite3_test_control(SQLITE_TESTCTRL_IMPOSTER, p->dbMain, "main", 0, 1);
      sqlite3_test_control(SQLITE_TESTCTRL_IMPOSTER, p->dbMain, "main", 1,tnum);
      rbuMPrintfExec(p, p->dbMain,
          "CREATE TABLE \"rbu_imp_%w\"( %s, PRIMARY KEY( %s ) ) WITHOUT ROWID",
          zTbl, zImposterCols, zImposterPK
      );
      sqlite3_test_control(SQLITE_TESTCTRL_IMPOSTER, p->dbMain, "main", 0, 0);

      /* Create the statement to insert index entries */
      pIter->nCol = nBind;
      if( p->rc==SQLITE_OK ){
        p->rc = prepareFreeAndCollectError(
            p->dbMain, &pIter->pInsert, &p->zErrmsg,
          sqlite3_mprintf("INSERT INTO \"rbu_imp_%w\" VALUES(%s)", zTbl, zBind)
        );
      }

      /* And to delete index entries */
      if( rbuIsVacuum(p)==0 && p->rc==SQLITE_OK ){
        p->rc = prepareFreeAndCollectError(
            p->dbMain, &pIter->pDelete, &p->zErrmsg,
          sqlite3_mprintf("DELETE FROM \"rbu_imp_%w\" WHERE %s", zTbl, zWhere)
        );
      }

      /* Create the SELECT statement to read keys in sorted order */
      if( p->rc==SQLITE_OK ){
        char *zSql;
        if( rbuIsVacuum(p) ){
          char *zStart = 0;
          if( nOffset ){
            zStart = rbuVacuumIndexStart(p, pIter);
            if( zStart ){
              sqlite3_free(zLimit);
              zLimit = 0;
            }
          }

          zSql = sqlite3_mprintf(
              "SELECT %s, 0 AS rbu_control FROM '%q' %s %s %s ORDER BY %s%s",
              zCollist, 
              pIter->zDataTbl,
              zPart, 
              (zStart ? (zPart ? "AND" : "WHERE") : ""), zStart,
              zCollist, zLimit
          );
          sqlite3_free(zStart);
        }else

        if( pIter->eType==RBU_PK_EXTERNAL || pIter->eType==RBU_PK_NONE ){
          zSql = sqlite3_mprintf(
              "SELECT %s, rbu_control FROM %s.'rbu_tmp_%q' %s ORDER BY %s%s",
              zCollist, p->zStateDb, pIter->zDataTbl,
              zPart, zCollist, zLimit
          );
        }else{
          zSql = sqlite3_mprintf(
              "SELECT %s, rbu_control FROM %s.'rbu_tmp_%q' %s "
              "UNION ALL "
              "SELECT %s, rbu_control FROM '%q' "
              "%s %s typeof(rbu_control)='integer' AND rbu_control!=1 "
              "ORDER BY %s%s",
              zCollist, p->zStateDb, pIter->zDataTbl, zPart,
              zCollist, pIter->zDataTbl, 
              zPart,
              (zPart ? "AND" : "WHERE"),
              zCollist, zLimit
          );
        }
        if( p->rc==SQLITE_OK ){
          p->rc = prepareFreeAndCollectError(p->dbRbu,&pIter->pSelect,pz,zSql);
        }else{
          sqlite3_free(zSql);
        }
      }

      sqlite3_free(zImposterCols);
      sqlite3_free(zImposterPK);
      sqlite3_free(zWhere);
      sqlite3_free(zBind);
      sqlite3_free(zPart);
    }else{
      int bRbuRowid = (pIter->eType==RBU_PK_VTAB)
                    ||(pIter->eType==RBU_PK_NONE)
                    ||(pIter->eType==RBU_PK_EXTERNAL && rbuIsVacuum(p));
      const char *zTbl = pIter->zTbl;       /* Table this step applies to */
      const char *zWrite;                   /* Imposter table name */

      char *zBindings = rbuObjIterGetBindlist(p, pIter->nTblCol + bRbuRowid);
      char *zWhere = rbuObjIterGetWhere(p, pIter);
      char *zOldlist = rbuObjIterGetOldlist(p, pIter, "old");
      char *zNewlist = rbuObjIterGetOldlist(p, pIter, "new");

      zCollist = rbuObjIterGetCollist(p, pIter);
      pIter->nCol = pIter->nTblCol;

      /* Create the imposter table or tables (if required). */
      rbuCreateImposterTable(p, pIter);
      rbuCreateImposterTable2(p, pIter);
      zWrite = (pIter->eType==RBU_PK_VTAB ? "" : "rbu_imp_");

      /* Create the INSERT statement to write to the target PK b-tree */
      if( p->rc==SQLITE_OK ){
        p->rc = prepareFreeAndCollectError(p->dbMain, &pIter->pInsert, pz,
            sqlite3_mprintf(
              "INSERT INTO \"%s%w\"(%s%s) VALUES(%s)", 
              zWrite, zTbl, zCollist, (bRbuRowid ? ", _rowid_" : ""), zBindings
            )
        );
      }

      /* Create the DELETE statement to write to the target PK b-tree.
      ** Because it only performs INSERT operations, this is not required for
      ** an rbu vacuum handle.  */
      if( rbuIsVacuum(p)==0 && p->rc==SQLITE_OK ){
        p->rc = prepareFreeAndCollectError(p->dbMain, &pIter->pDelete, pz,
            sqlite3_mprintf(
              "DELETE FROM \"%s%w\" WHERE %s", zWrite, zTbl, zWhere
            )
        );
      }

      if( rbuIsVacuum(p)==0 && pIter->abIndexed ){
        const char *zRbuRowid = "";
        if( pIter->eType==RBU_PK_EXTERNAL || pIter->eType==RBU_PK_NONE ){
          zRbuRowid = ", rbu_rowid";
        }

        /* Create the rbu_tmp_xxx table and the triggers to populate it. */
        rbuMPrintfExec(p, p->dbRbu,
            "CREATE TABLE IF NOT EXISTS %s.'rbu_tmp_%q' AS "
            "SELECT *%s FROM '%q' WHERE 0;"
            , p->zStateDb, pIter->zDataTbl
            , (pIter->eType==RBU_PK_EXTERNAL ? ", 0 AS rbu_rowid" : "")
            , pIter->zDataTbl
        );

        rbuMPrintfExec(p, p->dbMain,
            "CREATE TEMP TRIGGER rbu_delete_tr BEFORE DELETE ON \"%s%w\" "
            "BEGIN "
            "  SELECT rbu_tmp_insert(3, %s);"
            "END;"

            "CREATE TEMP TRIGGER rbu_update1_tr BEFORE UPDATE ON \"%s%w\" "
            "BEGIN "
            "  SELECT rbu_tmp_insert(3, %s);"
            "END;"

            "CREATE TEMP TRIGGER rbu_update2_tr AFTER UPDATE ON \"%s%w\" "
            "BEGIN "
            "  SELECT rbu_tmp_insert(4, %s);"
            "END;",
            zWrite, zTbl, zOldlist,
            zWrite, zTbl, zOldlist,
            zWrite, zTbl, zNewlist
        );

        if( pIter->eType==RBU_PK_EXTERNAL || pIter->eType==RBU_PK_NONE ){
          rbuMPrintfExec(p, p->dbMain,
              "CREATE TEMP TRIGGER rbu_insert_tr AFTER INSERT ON \"%s%w\" "
              "BEGIN "
              "  SELECT rbu_tmp_insert(0, %s);"
              "END;",
              zWrite, zTbl, zNewlist
          );
        }

        rbuObjIterPrepareTmpInsert(p, pIter, zCollist, zRbuRowid);
      }

      /* Create the SELECT statement to read keys from data_xxx */
      if( p->rc==SQLITE_OK ){
        const char *zRbuRowid = "";
        char *zStart = 0;
        char *zOrder = 0;
        if( bRbuRowid ){
          zRbuRowid = rbuIsVacuum(p) ? ",_rowid_ " : ",rbu_rowid";
        }

        if( rbuIsVacuum(p) ){
          if( nOffset ){
            zStart = rbuVacuumTableStart(p, pIter, bRbuRowid, zWrite);
            if( zStart ){
              sqlite3_free(zLimit);
              zLimit = 0;
            }
          }
          if( bRbuRowid ){
            zOrder = rbuMPrintf(p, "_rowid_");
          }else{
            zOrder = rbuObjIterGetPkList(p, pIter, "", ", ", "");
          }
        }

        if( p->rc==SQLITE_OK ){
          p->rc = prepareFreeAndCollectError(p->dbRbu, &pIter->pSelect, pz,
              sqlite3_mprintf(
                "SELECT %s,%s rbu_control%s FROM '%q'%s %s %s %s",
                zCollist, 
                (rbuIsVacuum(p) ? "0 AS " : ""),
                zRbuRowid,
                pIter->zDataTbl, (zStart ? zStart : ""), 
                (zOrder ? "ORDER BY" : ""), zOrder,
                zLimit
              )
          );
        }
        sqlite3_free(zStart);
        sqlite3_free(zOrder);
      }

      sqlite3_free(zWhere);
      sqlite3_free(zOldlist);
      sqlite3_free(zNewlist);
      sqlite3_free(zBindings);
    }
    sqlite3_free(zCollist);
    sqlite3_free(zLimit);
  }
  
  return p->rc;
}

/*
** Set output variable *ppStmt to point to an UPDATE statement that may
** be used to update the imposter table for the main table b-tree of the
** table object that pIter currently points to, assuming that the 
** rbu_control column of the data_xyz table contains zMask.
** 
** If the zMask string does not specify any columns to update, then this
** is not an error. Output variable *ppStmt is set to NULL in this case.
*/
static int rbuGetUpdateStmt(
  sqlite3rbu *p,                  /* RBU handle */
  RbuObjIter *pIter,              /* Object iterator */
  const char *zMask,              /* rbu_control value ('x.x.') */
  sqlite3_stmt **ppStmt           /* OUT: UPDATE statement handle */
){
  RbuUpdateStmt **pp;
  RbuUpdateStmt *pUp = 0;
  int nUp = 0;

  /* In case an error occurs */
  *ppStmt = 0;

  /* Search for an existing statement. If one is found, shift it to the front
  ** of the LRU queue and return immediately. Otherwise, leave nUp pointing
  ** to the number of statements currently in the cache and pUp to the
  ** last object in the list.  */
  for(pp=&pIter->pRbuUpdate; *pp; pp=&((*pp)->pNext)){
    pUp = *pp;
    if( strcmp(pUp->zMask, zMask)==0 ){
      *pp = pUp->pNext;
      pUp->pNext = pIter->pRbuUpdate;
      pIter->pRbuUpdate = pUp;
      *ppStmt = pUp->pUpdate; 
      return SQLITE_OK;
    }
    nUp++;
  }
  assert( pUp==0 || pUp->pNext==0 );

  if( nUp>=SQLITE_RBU_UPDATE_CACHESIZE ){
    for(pp=&pIter->pRbuUpdate; *pp!=pUp; pp=&((*pp)->pNext));
    *pp = 0;
    sqlite3_finalize(pUp->pUpdate);
    pUp->pUpdate = 0;
  }else{
    pUp = (RbuUpdateStmt*)rbuMalloc(p, sizeof(RbuUpdateStmt)+pIter->nTblCol+1);
  }

  if( pUp ){
    char *zWhere = rbuObjIterGetWhere(p, pIter);
    char *zSet = rbuObjIterGetSetlist(p, pIter, zMask);
    char *zUpdate = 0;

    pUp->zMask = (char*)&pUp[1];
    memcpy(pUp->zMask, zMask, pIter->nTblCol);
    pUp->pNext = pIter->pRbuUpdate;
    pIter->pRbuUpdate = pUp;

    if( zSet ){
      const char *zPrefix = "";

      if( pIter->eType!=RBU_PK_VTAB ) zPrefix = "rbu_imp_";
      zUpdate = sqlite3_mprintf("UPDATE \"%s%w\" SET %s WHERE %s", 
          zPrefix, pIter->zTbl, zSet, zWhere
      );
      p->rc = prepareFreeAndCollectError(
          p->dbMain, &pUp->pUpdate, &p->zErrmsg, zUpdate
      );
      *ppStmt = pUp->pUpdate;
    }
    sqlite3_free(zWhere);
    sqlite3_free(zSet);
  }

  return p->rc;
}

static sqlite3 *rbuOpenDbhandle(
  sqlite3rbu *p, 
  const char *zName, 
  int bUseVfs
){
  sqlite3 *db = 0;
  if( p->rc==SQLITE_OK ){
    const int flags = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_URI;
    p->rc = sqlite3_open_v2(zName, &db, flags, bUseVfs ? p->zVfsName : 0);
    if( p->rc ){
      p->zErrmsg = sqlite3_mprintf("%s", sqlite3_errmsg(db));
      sqlite3_close(db);
      db = 0;
    }
  }
  return db;
}

/*
** Free an RbuState object allocated by rbuLoadState().
*/
static void rbuFreeState(RbuState *p){
  if( p ){
    sqlite3_free(p->zTbl);
    sqlite3_free(p->zDataTbl);
    sqlite3_free(p->zIdx);
    sqlite3_free(p);
  }
}

/*
** Allocate an RbuState object and load the contents of the rbu_state 
** table into it. Return a pointer to the new object. It is the 
** responsibility of the caller to eventually free the object using
** sqlite3_free().
**
** If an error occurs, leave an error code and message in the rbu handle
** and return NULL.
*/
static RbuState *rbuLoadState(sqlite3rbu *p){
  RbuState *pRet = 0;
  sqlite3_stmt *pStmt = 0;
  int rc;
  int rc2;

  pRet = (RbuState*)rbuMalloc(p, sizeof(RbuState));
  if( pRet==0 ) return 0;

  rc = prepareFreeAndCollectError(p->dbRbu, &pStmt, &p->zErrmsg, 
      sqlite3_mprintf("SELECT k, v FROM %s.rbu_state", p->zStateDb)
  );
  while( rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pStmt) ){
    switch( sqlite3_column_int(pStmt, 0) ){
      case RBU_STATE_STAGE:
        pRet->eStage = sqlite3_column_int(pStmt, 1);
        if( pRet->eStage!=RBU_STAGE_OAL
         && pRet->eStage!=RBU_STAGE_MOVE
         && pRet->eStage!=RBU_STAGE_CKPT
        ){
          p->rc = SQLITE_CORRUPT;
        }
        break;

      case RBU_STATE_TBL:
        pRet->zTbl = rbuStrndup((char*)sqlite3_column_text(pStmt, 1), &rc);
        break;

      case RBU_STATE_IDX:
        pRet->zIdx = rbuStrndup((char*)sqlite3_column_text(pStmt, 1), &rc);
        break;

      case RBU_STATE_ROW:
        pRet->nRow = sqlite3_column_int(pStmt, 1);
        break;

      case RBU_STATE_PROGRESS:
        pRet->nProgress = sqlite3_column_int64(pStmt, 1);
        break;

      case RBU_STATE_CKPT:
        pRet->iWalCksum = sqlite3_column_int64(pStmt, 1);
        break;

      case RBU_STATE_COOKIE:
        pRet->iCookie = (u32)sqlite3_column_int64(pStmt, 1);
        break;

      case RBU_STATE_OALSZ:
        pRet->iOalSz = (u32)sqlite3_column_int64(pStmt, 1);
        break;

      case RBU_STATE_PHASEONESTEP:
        pRet->nPhaseOneStep = sqlite3_column_int64(pStmt, 1);
        break;

      case RBU_STATE_DATATBL:
        pRet->zDataTbl = rbuStrndup((char*)sqlite3_column_text(pStmt, 1), &rc);
        break;

      default:
        rc = SQLITE_CORRUPT;
        break;
    }
  }
  rc2 = sqlite3_finalize(pStmt);
  if( rc==SQLITE_OK ) rc = rc2;

  p->rc = rc;
  return pRet;
}


/*
** Open the database handle and attach the RBU database as "rbu". If an
** error occurs, leave an error code and message in the RBU handle.
*/
static void rbuOpenDatabase(sqlite3rbu *p, int *pbRetry){
  assert( p->rc || (p->dbMain==0 && p->dbRbu==0) );
  assert( p->rc || rbuIsVacuum(p) || p->zTarget!=0 );

  /* Open the RBU database */
  p->dbRbu = rbuOpenDbhandle(p, p->zRbu, 1);

  if( p->rc==SQLITE_OK && rbuIsVacuum(p) ){
    sqlite3_file_control(p->dbRbu, "main", SQLITE_FCNTL_RBUCNT, (void*)p);
    if( p->zState==0 ){
      const char *zFile = sqlite3_db_filename(p->dbRbu, "main");
      p->zState = rbuMPrintf(p, "file://%s-vacuum?modeof=%s", zFile, zFile);
    }
  }

  /* If using separate RBU and state databases, attach the state database to
  ** the RBU db handle now.  */
  if( p->zState ){
    rbuMPrintfExec(p, p->dbRbu, "ATTACH %Q AS stat", p->zState);
    memcpy(p->zStateDb, "stat", 4);
  }else{
    memcpy(p->zStateDb, "main", 4);
  }

#if 0
  if( p->rc==SQLITE_OK && rbuIsVacuum(p) ){
    p->rc = sqlite3_exec(p->dbRbu, "BEGIN", 0, 0, 0);
  }
#endif

  /* If it has not already been created, create the rbu_state table */
  rbuMPrintfExec(p, p->dbRbu, RBU_CREATE_STATE, p->zStateDb);

#if 0
  if( rbuIsVacuum(p) ){
    if( p->rc==SQLITE_OK ){
      int rc2;
      int bOk = 0;
      sqlite3_stmt *pCnt = 0;
      p->rc = prepareAndCollectError(p->dbRbu, &pCnt, &p->zErrmsg,
          "SELECT count(*) FROM stat.sqlite_master"
      );
      if( p->rc==SQLITE_OK 
       && sqlite3_step(pCnt)==SQLITE_ROW
       && 1==sqlite3_column_int(pCnt, 0)
      ){
        bOk = 1;
      }
      rc2 = sqlite3_finalize(pCnt);
      if( p->rc==SQLITE_OK ) p->rc = rc2;

      if( p->rc==SQLITE_OK && bOk==0 ){
        p->rc = SQLITE_ERROR;
        p->zErrmsg = sqlite3_mprintf("invalid state database");
      }
    
      if( p->rc==SQLITE_OK ){
        p->rc = sqlite3_exec(p->dbRbu, "COMMIT", 0, 0, 0);
      }
    }
  }
#endif

  if( p->rc==SQLITE_OK && rbuIsVacuum(p) ){
    int bOpen = 0;
    int rc;
    p->nRbu = 0;
    p->pRbuFd = 0;
    rc = sqlite3_file_control(p->dbRbu, "main", SQLITE_FCNTL_RBUCNT, (void*)p);
    if( rc!=SQLITE_NOTFOUND ) p->rc = rc;
    if( p->eStage>=RBU_STAGE_MOVE ){
      bOpen = 1;
    }else{
      RbuState *pState = rbuLoadState(p);
      if( pState ){
        bOpen = (pState->eStage>=RBU_STAGE_MOVE);
        rbuFreeState(pState);
      }
    }
    if( bOpen ) p->dbMain = rbuOpenDbhandle(p, p->zRbu, p->nRbu<=1);
  }

  p->eStage = 0;
  if( p->rc==SQLITE_OK && p->dbMain==0 ){
    if( !rbuIsVacuum(p) ){
      p->dbMain = rbuOpenDbhandle(p, p->zTarget, 1);
    }else if( p->pRbuFd->pWalFd ){
      if( pbRetry ){
        p->pRbuFd->bNolock = 0;
        sqlite3_close(p->dbRbu);
        sqlite3_close(p->dbMain);
        p->dbMain = 0;
        p->dbRbu = 0;
        *pbRetry = 1;
        return;
      }
      p->rc = SQLITE_ERROR;
      p->zErrmsg = sqlite3_mprintf("cannot vacuum wal mode database");
    }else{
      char *zTarget;
      char *zExtra = 0;
      if( strlen(p->zRbu)>=5 && 0==memcmp("file:", p->zRbu, 5) ){
        zExtra = &p->zRbu[5];
        while( *zExtra ){
          if( *zExtra++=='?' ) break;
        }
        if( *zExtra=='\0' ) zExtra = 0;
      }

      zTarget = sqlite3_mprintf("file:%s-vactmp?rbu_memory=1%s%s", 
          sqlite3_db_filename(p->dbRbu, "main"),
          (zExtra==0 ? "" : "&"), (zExtra==0 ? "" : zExtra)
      );

      if( zTarget==0 ){
        p->rc = SQLITE_NOMEM;
        return;
      }
      p->dbMain = rbuOpenDbhandle(p, zTarget, p->nRbu<=1);
      sqlite3_free(zTarget);
    }
  }

  if( p->rc==SQLITE_OK ){
    p->rc = sqlite3_create_function(p->dbMain, 
        "rbu_tmp_insert", -1, SQLITE_UTF8, (void*)p, rbuTmpInsertFunc, 0, 0
    );
  }

  if( p->rc==SQLITE_OK ){
    p->rc = sqlite3_create_function(p->dbMain, 
        "rbu_fossil_delta", 2, SQLITE_UTF8, 0, rbuFossilDeltaFunc, 0, 0
    );
  }

  if( p->rc==SQLITE_OK ){
    p->rc = sqlite3_create_function(p->dbRbu, 
        "rbu_target_name", -1, SQLITE_UTF8, (void*)p, rbuTargetNameFunc, 0, 0
    );
  }

  if( p->rc==SQLITE_OK ){
    p->rc = sqlite3_file_control(p->dbMain, "main", SQLITE_FCNTL_RBU, (void*)p);
  }
  rbuMPrintfExec(p, p->dbMain, "SELECT * FROM sqlite_master");

  /* Mark the database file just opened as an RBU target database. If 
  ** this call returns SQLITE_NOTFOUND, then the RBU vfs is not in use.
  ** This is an error.  */
  if( p->rc==SQLITE_OK ){
    p->rc = sqlite3_file_control(p->dbMain, "main", SQLITE_FCNTL_RBU, (void*)p);
  }

  if( p->rc==SQLITE_NOTFOUND ){
    p->rc = SQLITE_ERROR;
    p->zErrmsg = sqlite3_mprintf("rbu vfs not found");
  }
}

/*
** This routine is a copy of the sqlite3FileSuffix3() routine from the core.
** It is a no-op unless SQLITE_ENABLE_8_3_NAMES is defined.
**
** If SQLITE_ENABLE_8_3_NAMES is set at compile-time and if the database
** filename in zBaseFilename is a URI with the "8_3_names=1" parameter and
** if filename in z[] has a suffix (a.k.a. "extension") that is longer than
** three characters, then shorten the suffix on z[] to be the last three
** characters of the original suffix.
**
** If SQLITE_ENABLE_8_3_NAMES is set to 2 at compile-time, then always
** do the suffix shortening regardless of URI parameter.
**
** Examples:
**
**     test.db-journal    =>   test.nal
**     test.db-wal        =>   test.wal
**     test.db-shm        =>   test.shm
**     test.db-mj7f3319fa =>   test.9fa
*/
static void rbuFileSuffix3(const char *zBase, char *z){
#ifdef SQLITE_ENABLE_8_3_NAMES
#if SQLITE_ENABLE_8_3_NAMES<2
  if( sqlite3_uri_boolean(zBase, "8_3_names", 0) )
#endif
  {
    int i, sz;
    sz = (int)strlen(z)&0xffffff;
    for(i=sz-1; i>0 && z[i]!='/' && z[i]!='.'; i--){}
    if( z[i]=='.' && sz>i+4 ) memmove(&z[i+1], &z[sz-3], 4);
  }
#endif
}

/*
** Return the current wal-index header checksum for the target database 
** as a 64-bit integer.
**
** The checksum is store in the first page of xShmMap memory as an 8-byte 
** blob starting at byte offset 40.
*/
static i64 rbuShmChecksum(sqlite3rbu *p){
  i64 iRet = 0;
  if( p->rc==SQLITE_OK ){
    sqlite3_file *pDb = p->pTargetFd->pReal;
    u32 volatile *ptr;
    p->rc = pDb->pMethods->xShmMap(pDb, 0, 32*1024, 0, (void volatile**)&ptr);
    if( p->rc==SQLITE_OK ){
      iRet = ((i64)ptr[10] << 32) + ptr[11];
    }
  }
  return iRet;
}

/*
** This function is called as part of initializing or reinitializing an
** incremental checkpoint. 
**
** It populates the sqlite3rbu.aFrame[] array with the set of 
** (wal frame -> db page) copy operations required to checkpoint the 
** current wal file, and obtains the set of shm locks required to safely 
** perform the copy operations directly on the file-system.
**
** If argument pState is not NULL, then the incremental checkpoint is
** being resumed. In this case, if the checksum of the wal-index-header
** following recovery is not the same as the checksum saved in the RbuState
** object, then the rbu handle is set to DONE state. This occurs if some
** other client appends a transaction to the wal file in the middle of
** an incremental checkpoint.
*/
static void rbuSetupCheckpoint(sqlite3rbu *p, RbuState *pState){

  /* If pState is NULL, then the wal file may not have been opened and
  ** recovered. Running a read-statement here to ensure that doing so
  ** does not interfere with the "capture" process below.  */
  if( pState==0 ){
    p->eStage = 0;
    if( p->rc==SQLITE_OK ){
      p->rc = sqlite3_exec(p->dbMain, "SELECT * FROM sqlite_master", 0, 0, 0);
    }
  }

  /* Assuming no error has occurred, run a "restart" checkpoint with the
  ** sqlite3rbu.eStage variable set to CAPTURE. This turns on the following
  ** special behaviour in the rbu VFS:
  **
  **   * If the exclusive shm WRITER or READ0 lock cannot be obtained,
  **     the checkpoint fails with SQLITE_BUSY (normally SQLite would
  **     proceed with running a passive checkpoint instead of failing).
  **
  **   * Attempts to read from the *-wal file or write to the database file
  **     do not perform any IO. Instead, the frame/page combinations that
  **     would be read/written are recorded in the sqlite3rbu.aFrame[]
  **     array.
  **
  **   * Calls to xShmLock(UNLOCK) to release the exclusive shm WRITER, 
  **     READ0 and CHECKPOINT locks taken as part of the checkpoint are
  **     no-ops. These locks will not be released until the connection
  **     is closed.
  **
  **   * Attempting to xSync() the database file causes an SQLITE_INTERNAL 
  **     error.
  **
  ** As a result, unless an error (i.e. OOM or SQLITE_BUSY) occurs, the
  ** checkpoint below fails with SQLITE_INTERNAL, and leaves the aFrame[]
  ** array populated with a set of (frame -> page) mappings. Because the 
  ** WRITER, CHECKPOINT and READ0 locks are still held, it is safe to copy 
  ** data from the wal file into the database file according to the 
  ** contents of aFrame[].
  */
  if( p->rc==SQLITE_OK ){
    int rc2;
    p->eStage = RBU_STAGE_CAPTURE;
    rc2 = sqlite3_exec(p->dbMain, "PRAGMA main.wal_checkpoint=restart", 0, 0,0);
    if( rc2!=SQLITE_INTERNAL ) p->rc = rc2;
  }

  if( p->rc==SQLITE_OK && p->nFrame>0 ){
    p->eStage = RBU_STAGE_CKPT;
    p->nStep = (pState ? pState->nRow : 0);
    p->aBuf = rbuMalloc(p, p->pgsz);
    p->iWalCksum = rbuShmChecksum(p);
  }

  if( p->rc==SQLITE_OK ){
    if( p->nFrame==0 || (pState && pState->iWalCksum!=p->iWalCksum) ){
      p->rc = SQLITE_DONE;
      p->eStage = RBU_STAGE_DONE;
    }else{
      int nSectorSize;
      sqlite3_file *pDb = p->pTargetFd->pReal;
      sqlite3_file *pWal = p->pTargetFd->pWalFd->pReal;
      assert( p->nPagePerSector==0 );
      nSectorSize = pDb->pMethods->xSectorSize(pDb);
      if( nSectorSize>p->pgsz ){
        p->nPagePerSector = nSectorSize / p->pgsz;
      }else{
        p->nPagePerSector = 1;
      }

      /* Call xSync() on the wal file. This causes SQLite to sync the 
      ** directory in which the target database and the wal file reside, in 
      ** case it has not been synced since the rename() call in 
      ** rbuMoveOalFile(). */
      p->rc = pWal->pMethods->xSync(pWal, SQLITE_SYNC_NORMAL);
    }
  }
}

/*
** Called when iAmt bytes are read from offset iOff of the wal file while
** the rbu object is in capture mode. Record the frame number of the frame
** being read in the aFrame[] array.
*/
static int rbuCaptureWalRead(sqlite3rbu *pRbu, i64 iOff, int iAmt){
  const u32 mReq = (1<<WAL_LOCK_WRITE)|(1<<WAL_LOCK_CKPT)|(1<<WAL_LOCK_READ0);
  u32 iFrame;

  if( pRbu->mLock!=mReq ){
    pRbu->rc = SQLITE_BUSY;
    return SQLITE_INTERNAL;
  }

  pRbu->pgsz = iAmt;
  if( pRbu->nFrame==pRbu->nFrameAlloc ){
    int nNew = (pRbu->nFrameAlloc ? pRbu->nFrameAlloc : 64) * 2;
    RbuFrame *aNew;
    aNew = (RbuFrame*)sqlite3_realloc64(pRbu->aFrame, nNew * sizeof(RbuFrame));
    if( aNew==0 ) return SQLITE_NOMEM;
    pRbu->aFrame = aNew;
    pRbu->nFrameAlloc = nNew;
  }

  iFrame = (u32)((iOff-32) / (i64)(iAmt+24)) + 1;
  if( pRbu->iMaxFrame<iFrame ) pRbu->iMaxFrame = iFrame;
  pRbu->aFrame[pRbu->nFrame].iWalFrame = iFrame;
  pRbu->aFrame[pRbu->nFrame].iDbPage = 0;
  pRbu->nFrame++;
  return SQLITE_OK;
}

/*
** Called when a page of data is written to offset iOff of the database
** file while the rbu handle is in capture mode. Record the page number 
** of the page being written in the aFrame[] array.
*/
static int rbuCaptureDbWrite(sqlite3rbu *pRbu, i64 iOff){
  pRbu->aFrame[pRbu->nFrame-1].iDbPage = (u32)(iOff / pRbu->pgsz) + 1;
  return SQLITE_OK;
}

/*
** This is called as part of an incremental checkpoint operation. Copy
** a single frame of data from the wal file into the database file, as
** indicated by the RbuFrame object.
*/
static void rbuCheckpointFrame(sqlite3rbu *p, RbuFrame *pFrame){
  sqlite3_file *pWal = p->pTargetFd->pWalFd->pReal;
  sqlite3_file *pDb = p->pTargetFd->pReal;
  i64 iOff;

  assert( p->rc==SQLITE_OK );
  iOff = (i64)(pFrame->iWalFrame-1) * (p->pgsz + 24) + 32 + 24;
  p->rc = pWal->pMethods->xRead(pWal, p->aBuf, p->pgsz, iOff);
  if( p->rc ) return;

  iOff = (i64)(pFrame->iDbPage-1) * p->pgsz;
  p->rc = pDb->pMethods->xWrite(pDb, p->aBuf, p->pgsz, iOff);
}


/*
** Take an EXCLUSIVE lock on the database file.
*/
static void rbuLockDatabase(sqlite3rbu *p){
  sqlite3_file *pReal = p->pTargetFd->pReal;
  assert( p->rc==SQLITE_OK );
  p->rc = pReal->pMethods->xLock(pReal, SQLITE_LOCK_SHARED);
  if( p->rc==SQLITE_OK ){
    p->rc = pReal->pMethods->xLock(pReal, SQLITE_LOCK_EXCLUSIVE);
  }
}

#if defined(_WIN32_WCE)
static LPWSTR rbuWinUtf8ToUnicode(const char *zFilename){
  int nChar;
  LPWSTR zWideFilename;

  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, NULL, 0);
  if( nChar==0 ){
    return 0;
  }
  zWideFilename = sqlite3_malloc64( nChar*sizeof(zWideFilename[0]) );
  if( zWideFilename==0 ){
    return 0;
  }
  memset(zWideFilename, 0, nChar*sizeof(zWideFilename[0]));
  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, zWideFilename,
                                nChar);
  if( nChar==0 ){
    sqlite3_free(zWideFilename);
    zWideFilename = 0;
  }
  return zWideFilename;
}
#endif

/*
** The RBU handle is currently in RBU_STAGE_OAL state, with a SHARED lock
** on the database file. This proc moves the *-oal file to the *-wal path,
** then reopens the database file (this time in vanilla, non-oal, WAL mode).
** If an error occurs, leave an error code and error message in the rbu 
** handle.
*/
static void rbuMoveOalFile(sqlite3rbu *p){
  const char *zBase = sqlite3_db_filename(p->dbMain, "main");
  const char *zMove = zBase;
  char *zOal;
  char *zWal;

  if( rbuIsVacuum(p) ){
    zMove = sqlite3_db_filename(p->dbRbu, "main");
  }
  zOal = sqlite3_mprintf("%s-oal", zMove);
  zWal = sqlite3_mprintf("%s-wal", zMove);

  assert( p->eStage==RBU_STAGE_MOVE );
  assert( p->rc==SQLITE_OK && p->zErrmsg==0 );
  if( zWal==0 || zOal==0 ){
    p->rc = SQLITE_NOMEM;
  }else{
    /* Move the *-oal file to *-wal. At this point connection p->db is
    ** holding a SHARED lock on the target database file (because it is
    ** in WAL mode). So no other connection may be writing the db. 
    **
    ** In order to ensure that there are no database readers, an EXCLUSIVE
    ** lock is obtained here before the *-oal is moved to *-wal.
    */
    rbuLockDatabase(p);
    if( p->rc==SQLITE_OK ){
      rbuFileSuffix3(zBase, zWal);
      rbuFileSuffix3(zBase, zOal);

      /* Re-open the databases. */
      rbuObjIterFinalize(&p->objiter);
      sqlite3_close(p->dbRbu);
      sqlite3_close(p->dbMain);
      p->dbMain = 0;
      p->dbRbu = 0;

#if defined(_WIN32_WCE)
      {
        LPWSTR zWideOal;
        LPWSTR zWideWal;

        zWideOal = rbuWinUtf8ToUnicode(zOal);
        if( zWideOal ){
          zWideWal = rbuWinUtf8ToUnicode(zWal);
          if( zWideWal ){
            if( MoveFileW(zWideOal, zWideWal) ){
              p->rc = SQLITE_OK;
            }else{
              p->rc = SQLITE_IOERR;
            }
            sqlite3_free(zWideWal);
          }else{
            p->rc = SQLITE_IOERR_NOMEM;
          }
          sqlite3_free(zWideOal);
        }else{
          p->rc = SQLITE_IOERR_NOMEM;
        }
      }
#else
      p->rc = rename(zOal, zWal) ? SQLITE_IOERR : SQLITE_OK;
#endif

      if( p->rc==SQLITE_OK ){
        rbuOpenDatabase(p, 0);
        rbuSetupCheckpoint(p, 0);
      }
    }
  }

  sqlite3_free(zWal);
  sqlite3_free(zOal);
}

/*
** The SELECT statement iterating through the keys for the current object
** (p->objiter.pSelect) currently points to a valid row. This function
** determines the type of operation requested by this row and returns
** one of the following values to indicate the result:
**
**     * RBU_INSERT
**     * RBU_DELETE
**     * RBU_IDX_DELETE
**     * RBU_UPDATE
**
** If RBU_UPDATE is returned, then output variable *pzMask is set to
** point to the text value indicating the columns to update.
**
** If the rbu_control field contains an invalid value, an error code and
** message are left in the RBU handle and zero returned.
*/
static int rbuStepType(sqlite3rbu *p, const char **pzMask){
  int iCol = p->objiter.nCol;     /* Index of rbu_control column */
  int res = 0;                    /* Return value */

  switch( sqlite3_column_type(p->objiter.pSelect, iCol) ){
    case SQLITE_INTEGER: {
      int iVal = sqlite3_column_int(p->objiter.pSelect, iCol);
      switch( iVal ){
        case 0: res = RBU_INSERT;     break;
        case 1: res = RBU_DELETE;     break;
        case 2: res = RBU_REPLACE;    break;
        case 3: res = RBU_IDX_DELETE; break;
        case 4: res = RBU_IDX_INSERT; break;
      }
      break;
    }

    case SQLITE_TEXT: {
      const unsigned char *z = sqlite3_column_text(p->objiter.pSelect, iCol);
      if( z==0 ){
        p->rc = SQLITE_NOMEM;
      }else{
        *pzMask = (const char*)z;
      }
      res = RBU_UPDATE;

      break;
    }

    default:
      break;
  }

  if( res==0 ){
    rbuBadControlError(p);
  }
  return res;
}

#ifdef SQLITE_DEBUG
/*
** Assert that column iCol of statement pStmt is named zName.
*/
static void assertColumnName(sqlite3_stmt *pStmt, int iCol, const char *zName){
  const char *zCol = sqlite3_column_name(pStmt, iCol);
  assert( 0==sqlite3_stricmp(zName, zCol) );
}
#else
# define assertColumnName(x,y,z)
#endif

/*
** Argument eType must be one of RBU_INSERT, RBU_DELETE, RBU_IDX_INSERT or
** RBU_IDX_DELETE. This function performs the work of a single
** sqlite3rbu_step() call for the type of operation specified by eType.
*/
static void rbuStepOneOp(sqlite3rbu *p, int eType){
  RbuObjIter *pIter = &p->objiter;
  sqlite3_value *pVal;
  sqlite3_stmt *pWriter;
  int i;

  assert( p->rc==SQLITE_OK );
  assert( eType!=RBU_DELETE || pIter->zIdx==0 );
  assert( eType==RBU_DELETE || eType==RBU_IDX_DELETE
       || eType==RBU_INSERT || eType==RBU_IDX_INSERT
  );

  /* If this is a delete, decrement nPhaseOneStep by nIndex. If the DELETE
  ** statement below does actually delete a row, nPhaseOneStep will be
  ** incremented by the same amount when SQL function rbu_tmp_insert()
  ** is invoked by the trigger.  */
  if( eType==RBU_DELETE ){
    p->nPhaseOneStep -= p->objiter.nIndex;
  }

  if( eType==RBU_IDX_DELETE || eType==RBU_DELETE ){
    pWriter = pIter->pDelete;
  }else{
    pWriter = pIter->pInsert;
  }

  for(i=0; i<pIter->nCol; i++){
    /* If this is an INSERT into a table b-tree and the table has an
    ** explicit INTEGER PRIMARY KEY, check that this is not an attempt
    ** to write a NULL into the IPK column. That is not permitted.  */
    if( eType==RBU_INSERT 
     && pIter->zIdx==0 && pIter->eType==RBU_PK_IPK && pIter->abTblPk[i] 
     && sqlite3_column_type(pIter->pSelect, i)==SQLITE_NULL
    ){
      p->rc = SQLITE_MISMATCH;
      p->zErrmsg = sqlite3_mprintf("datatype mismatch");
      return;
    }

    if( eType==RBU_DELETE && pIter->abTblPk[i]==0 ){
      continue;
    }

    pVal = sqlite3_column_value(pIter->pSelect, i);
    p->rc = sqlite3_bind_value(pWriter, i+1, pVal);
    if( p->rc ) return;
  }
  if( pIter->zIdx==0 ){
    if( pIter->eType==RBU_PK_VTAB 
     || pIter->eType==RBU_PK_NONE 
     || (pIter->eType==RBU_PK_EXTERNAL && rbuIsVacuum(p)) 
    ){
      /* For a virtual table, or a table with no primary key, the 
      ** SELECT statement is:
      **
      **   SELECT <cols>, rbu_control, rbu_rowid FROM ....
      **
      ** Hence column_value(pIter->nCol+1).
      */
      assertColumnName(pIter->pSelect, pIter->nCol+1, 
          rbuIsVacuum(p) ? "rowid" : "rbu_rowid"
      );
      pVal = sqlite3_column_value(pIter->pSelect, pIter->nCol+1);
      p->rc = sqlite3_bind_value(pWriter, pIter->nCol+1, pVal);
    }
  }
  if( p->rc==SQLITE_OK ){
    sqlite3_step(pWriter);
    p->rc = resetAndCollectError(pWriter, &p->zErrmsg);
  }
}

/*
** This function does the work for an sqlite3rbu_step() call.
**
** The object-iterator (p->objiter) currently points to a valid object,
** and the input cursor (p->objiter.pSelect) currently points to a valid
** input row. Perform whatever processing is required and return.
**
** If no  error occurs, SQLITE_OK is returned. Otherwise, an error code
** and message is left in the RBU handle and a copy of the error code
** returned.
*/
static int rbuStep(sqlite3rbu *p){
  RbuObjIter *pIter = &p->objiter;
  const char *zMask = 0;
  int eType = rbuStepType(p, &zMask);

  if( eType ){
    assert( eType==RBU_INSERT     || eType==RBU_DELETE
         || eType==RBU_REPLACE    || eType==RBU_IDX_DELETE
         || eType==RBU_IDX_INSERT || eType==RBU_UPDATE
    );
    assert( eType!=RBU_UPDATE || pIter->zIdx==0 );

    if( pIter->zIdx==0 && (eType==RBU_IDX_DELETE || eType==RBU_IDX_INSERT) ){
      rbuBadControlError(p);
    }
    else if( eType==RBU_REPLACE ){
      if( pIter->zIdx==0 ){
        p->nPhaseOneStep += p->objiter.nIndex;
        rbuStepOneOp(p, RBU_DELETE);
      }
      if( p->rc==SQLITE_OK ) rbuStepOneOp(p, RBU_INSERT);
    }
    else if( eType!=RBU_UPDATE ){
      rbuStepOneOp(p, eType);
    }
    else{
      sqlite3_value *pVal;
      sqlite3_stmt *pUpdate = 0;
      assert( eType==RBU_UPDATE );
      p->nPhaseOneStep -= p->objiter.nIndex;
      rbuGetUpdateStmt(p, pIter, zMask, &pUpdate);
      if( pUpdate ){
        int i;
        for(i=0; p->rc==SQLITE_OK && i<pIter->nCol; i++){
          char c = zMask[pIter->aiSrcOrder[i]];
          pVal = sqlite3_column_value(pIter->pSelect, i);
          if( pIter->abTblPk[i] || c!='.' ){
            p->rc = sqlite3_bind_value(pUpdate, i+1, pVal);
          }
        }
        if( p->rc==SQLITE_OK 
         && (pIter->eType==RBU_PK_VTAB || pIter->eType==RBU_PK_NONE) 
        ){
          /* Bind the rbu_rowid value to column _rowid_ */
          assertColumnName(pIter->pSelect, pIter->nCol+1, "rbu_rowid");
          pVal = sqlite3_column_value(pIter->pSelect, pIter->nCol+1);
          p->rc = sqlite3_bind_value(pUpdate, pIter->nCol+1, pVal);
        }
        if( p->rc==SQLITE_OK ){
          sqlite3_step(pUpdate);
          p->rc = resetAndCollectError(pUpdate, &p->zErrmsg);
        }
      }
    }
  }
  return p->rc;
}

/*
** Increment the schema cookie of the main database opened by p->dbMain.
**
** Or, if this is an RBU vacuum, set the schema cookie of the main db
** opened by p->dbMain to one more than the schema cookie of the main
** db opened by p->dbRbu.
*/
static void rbuIncrSchemaCookie(sqlite3rbu *p){
  if( p->rc==SQLITE_OK ){
    sqlite3 *dbread = (rbuIsVacuum(p) ? p->dbRbu : p->dbMain);
    int iCookie = 1000000;
    sqlite3_stmt *pStmt;

    p->rc = prepareAndCollectError(dbread, &pStmt, &p->zErrmsg, 
        "PRAGMA schema_version"
    );
    if( p->rc==SQLITE_OK ){
      /* Coverage: it may be that this sqlite3_step() cannot fail. There
      ** is already a transaction open, so the prepared statement cannot
      ** throw an SQLITE_SCHEMA exception. The only database page the
      ** statement reads is page 1, which is guaranteed to be in the cache.
      ** And no memory allocations are required.  */
      if( SQLITE_ROW==sqlite3_step(pStmt) ){
        iCookie = sqlite3_column_int(pStmt, 0);
      }
      rbuFinalize(p, pStmt);
    }
    if( p->rc==SQLITE_OK ){
      rbuMPrintfExec(p, p->dbMain, "PRAGMA schema_version = %d", iCookie+1);
    }
  }
}

/*
** Update the contents of the rbu_state table within the rbu database. The
** value stored in the RBU_STATE_STAGE column is eStage. All other values
** are determined by inspecting the rbu handle passed as the first argument.
*/
static void rbuSaveState(sqlite3rbu *p, int eStage){
  if( p->rc==SQLITE_OK || p->rc==SQLITE_DONE ){
    sqlite3_stmt *pInsert = 0;
    rbu_file *pFd = (rbuIsVacuum(p) ? p->pRbuFd : p->pTargetFd);
    int rc;

    assert( p->zErrmsg==0 );
    rc = prepareFreeAndCollectError(p->dbRbu, &pInsert, &p->zErrmsg, 
        sqlite3_mprintf(
          "INSERT OR REPLACE INTO %s.rbu_state(k, v) VALUES "
          "(%d, %d), "
          "(%d, %Q), "
          "(%d, %Q), "
          "(%d, %d), "
          "(%d, %d), "
          "(%d, %lld), "
          "(%d, %lld), "
          "(%d, %lld), "
          "(%d, %lld), "
          "(%d, %Q)  ",
          p->zStateDb,
          RBU_STATE_STAGE, eStage,
          RBU_STATE_TBL, p->objiter.zTbl, 
          RBU_STATE_IDX, p->objiter.zIdx, 
          RBU_STATE_ROW, p->nStep, 
          RBU_STATE_PROGRESS, p->nProgress,
          RBU_STATE_CKPT, p->iWalCksum,
          RBU_STATE_COOKIE, (i64)pFd->iCookie,
          RBU_STATE_OALSZ, p->iOalSz,
          RBU_STATE_PHASEONESTEP, p->nPhaseOneStep,
          RBU_STATE_DATATBL, p->objiter.zDataTbl
      )
    );
    assert( pInsert==0 || rc==SQLITE_OK );

    if( rc==SQLITE_OK ){
      sqlite3_step(pInsert);
      rc = sqlite3_finalize(pInsert);
    }
    if( rc!=SQLITE_OK ) p->rc = rc;
  }
}


/*
** The second argument passed to this function is the name of a PRAGMA 
** setting - "page_size", "auto_vacuum", "user_version" or "application_id".
** This function executes the following on sqlite3rbu.dbRbu:
**
**   "PRAGMA main.$zPragma"
**
** where $zPragma is the string passed as the second argument, then
** on sqlite3rbu.dbMain:
**
**   "PRAGMA main.$zPragma = $val"
**
** where $val is the value returned by the first PRAGMA invocation.
**
** In short, it copies the value  of the specified PRAGMA setting from
** dbRbu to dbMain.
*/
static void rbuCopyPragma(sqlite3rbu *p, const char *zPragma){
  if( p->rc==SQLITE_OK ){
    sqlite3_stmt *pPragma = 0;
    p->rc = prepareFreeAndCollectError(p->dbRbu, &pPragma, &p->zErrmsg, 
        sqlite3_mprintf("PRAGMA main.%s", zPragma)
    );
    if( p->rc==SQLITE_OK && SQLITE_ROW==sqlite3_step(pPragma) ){
      p->rc = rbuMPrintfExec(p, p->dbMain, "PRAGMA main.%s = %d",
          zPragma, sqlite3_column_int(pPragma, 0)
      );
    }
    rbuFinalize(p, pPragma);
  }
}

/*
** The RBU handle passed as the only argument has just been opened and 
** the state database is empty. If this RBU handle was opened for an
** RBU vacuum operation, create the schema in the target db.
*/
static void rbuCreateTargetSchema(sqlite3rbu *p){
  sqlite3_stmt *pSql = 0;
  sqlite3_stmt *pInsert = 0;

  assert( rbuIsVacuum(p) );
  p->rc = sqlite3_exec(p->dbMain, "PRAGMA writable_schema=1", 0,0, &p->zErrmsg);
  if( p->rc==SQLITE_OK ){
    p->rc = prepareAndCollectError(p->dbRbu, &pSql, &p->zErrmsg, 
      "SELECT sql FROM sqlite_master WHERE sql!='' AND rootpage!=0"
      " AND name!='sqlite_sequence' "
      " ORDER BY type DESC"
    );
  }

  while( p->rc==SQLITE_OK && sqlite3_step(pSql)==SQLITE_ROW ){
    const char *zSql = (const char*)sqlite3_column_text(pSql, 0);
    p->rc = sqlite3_exec(p->dbMain, zSql, 0, 0, &p->zErrmsg);
  }
  rbuFinalize(p, pSql);
  if( p->rc!=SQLITE_OK ) return;

  if( p->rc==SQLITE_OK ){
    p->rc = prepareAndCollectError(p->dbRbu, &pSql, &p->zErrmsg, 
        "SELECT * FROM sqlite_master WHERE rootpage=0 OR rootpage IS NULL" 
    );
  }

  if( p->rc==SQLITE_OK ){
    p->rc = prepareAndCollectError(p->dbMain, &pInsert, &p->zErrmsg, 
        "INSERT INTO sqlite_master VALUES(?,?,?,?,?)"
    );
  }

  while( p->rc==SQLITE_OK && sqlite3_step(pSql)==SQLITE_ROW ){
    int i;
    for(i=0; i<5; i++){
      sqlite3_bind_value(pInsert, i+1, sqlite3_column_value(pSql, i));
    }
    sqlite3_step(pInsert);
    p->rc = sqlite3_reset(pInsert);
  }
  if( p->rc==SQLITE_OK ){
    p->rc = sqlite3_exec(p->dbMain, "PRAGMA writable_schema=0",0,0,&p->zErrmsg);
  }

  rbuFinalize(p, pSql);
  rbuFinalize(p, pInsert);
}

/*
** Step the RBU object.
*/
SQLITE_API int sqlite3rbu_step(sqlite3rbu *p){
  if( p ){
    switch( p->eStage ){
      case RBU_STAGE_OAL: {
        RbuObjIter *pIter = &p->objiter;

        /* If this is an RBU vacuum operation and the state table was empty
        ** when this handle was opened, create the target database schema. */
        if( rbuIsVacuum(p) && p->nProgress==0 && p->rc==SQLITE_OK ){
          rbuCreateTargetSchema(p);
          rbuCopyPragma(p, "user_version");
          rbuCopyPragma(p, "application_id");
        }

        while( p->rc==SQLITE_OK && pIter->zTbl ){

          if( pIter->bCleanup ){
            /* Clean up the rbu_tmp_xxx table for the previous table. It 
            ** cannot be dropped as there are currently active SQL statements.
            ** But the contents can be deleted.  */
            if( rbuIsVacuum(p)==0 && pIter->abIndexed ){
              rbuMPrintfExec(p, p->dbRbu, 
                  "DELETE FROM %s.'rbu_tmp_%q'", p->zStateDb, pIter->zDataTbl
              );
            }
          }else{
            rbuObjIterPrepareAll(p, pIter, 0);

            /* Advance to the next row to process. */
            if( p->rc==SQLITE_OK ){
              int rc = sqlite3_step(pIter->pSelect);
              if( rc==SQLITE_ROW ){
                p->nProgress++;
                p->nStep++;
                return rbuStep(p);
              }
              p->rc = sqlite3_reset(pIter->pSelect);
              p->nStep = 0;
            }
          }

          rbuObjIterNext(p, pIter);
        }

        if( p->rc==SQLITE_OK ){
          assert( pIter->zTbl==0 );
          rbuSaveState(p, RBU_STAGE_MOVE);
          rbuIncrSchemaCookie(p);
          if( p->rc==SQLITE_OK ){
            p->rc = sqlite3_exec(p->dbMain, "COMMIT", 0, 0, &p->zErrmsg);
          }
          if( p->rc==SQLITE_OK ){
            p->rc = sqlite3_exec(p->dbRbu, "COMMIT", 0, 0, &p->zErrmsg);
          }
          p->eStage = RBU_STAGE_MOVE;
        }
        break;
      }

      case RBU_STAGE_MOVE: {
        if( p->rc==SQLITE_OK ){
          rbuMoveOalFile(p);
          p->nProgress++;
        }
        break;
      }

      case RBU_STAGE_CKPT: {
        if( p->rc==SQLITE_OK ){
          if( p->nStep>=p->nFrame ){
            sqlite3_file *pDb = p->pTargetFd->pReal;
  
            /* Sync the db file */
            p->rc = pDb->pMethods->xSync(pDb, SQLITE_SYNC_NORMAL);
  
            /* Update nBackfill */
            if( p->rc==SQLITE_OK ){
              void volatile *ptr;
              p->rc = pDb->pMethods->xShmMap(pDb, 0, 32*1024, 0, &ptr);
              if( p->rc==SQLITE_OK ){
                ((u32 volatile*)ptr)[24] = p->iMaxFrame;
              }
            }
  
            if( p->rc==SQLITE_OK ){
              p->eStage = RBU_STAGE_DONE;
              p->rc = SQLITE_DONE;
            }
          }else{
            /* At one point the following block copied a single frame from the
            ** wal file to the database file. So that one call to sqlite3rbu_step()
            ** checkpointed a single frame. 
            **
            ** However, if the sector-size is larger than the page-size, and the
            ** application calls sqlite3rbu_savestate() or close() immediately
            ** after this step, then rbu_step() again, then a power failure occurs,
            ** then the database page written here may be damaged. Work around
            ** this by checkpointing frames until the next page in the aFrame[]
            ** lies on a different disk sector to the current one. */
            u32 iSector;
            do{
              RbuFrame *pFrame = &p->aFrame[p->nStep];
              iSector = (pFrame->iDbPage-1) / p->nPagePerSector;
              rbuCheckpointFrame(p, pFrame);
              p->nStep++;
            }while( p->nStep<p->nFrame 
                 && iSector==((p->aFrame[p->nStep].iDbPage-1) / p->nPagePerSector)
                 && p->rc==SQLITE_OK
            );
          }
          p->nProgress++;
        }
        break;
      }

      default:
        break;
    }
    return p->rc;
  }else{
    return SQLITE_NOMEM;
  }
}

/*
** Compare strings z1 and z2, returning 0 if they are identical, or non-zero
** otherwise. Either or both argument may be NULL. Two NULL values are
** considered equal, and NULL is considered distinct from all other values.
*/
static int rbuStrCompare(const char *z1, const char *z2){
  if( z1==0 && z2==0 ) return 0;
  if( z1==0 || z2==0 ) return 1;
  return (sqlite3_stricmp(z1, z2)!=0);
}

/*
** This function is called as part of sqlite3rbu_open() when initializing
** an rbu handle in OAL stage. If the rbu update has not started (i.e.
** the rbu_state table was empty) it is a no-op. Otherwise, it arranges
** things so that the next call to sqlite3rbu_step() continues on from
** where the previous rbu handle left off.
**
** If an error occurs, an error code and error message are left in the
** rbu handle passed as the first argument.
*/
static void rbuSetupOal(sqlite3rbu *p, RbuState *pState){
  assert( p->rc==SQLITE_OK );
  if( pState->zTbl ){
    RbuObjIter *pIter = &p->objiter;
    int rc = SQLITE_OK;

    while( rc==SQLITE_OK && pIter->zTbl && (pIter->bCleanup 
       || rbuStrCompare(pIter->zIdx, pState->zIdx)
       || (pState->zDataTbl==0 && rbuStrCompare(pIter->zTbl, pState->zTbl))
       || (pState->zDataTbl && rbuStrCompare(pIter->zDataTbl, pState->zDataTbl))
    )){
      rc = rbuObjIterNext(p, pIter);
    }

    if( rc==SQLITE_OK && !pIter->zTbl ){
      rc = SQLITE_ERROR;
      p->zErrmsg = sqlite3_mprintf("rbu_state mismatch error");
    }

    if( rc==SQLITE_OK ){
      p->nStep = pState->nRow;
      rc = rbuObjIterPrepareAll(p, &p->objiter, p->nStep);
    }

    p->rc = rc;
  }
}

/*
** If there is a "*-oal" file in the file-system corresponding to the
** target database in the file-system, delete it. If an error occurs,
** leave an error code and error message in the rbu handle.
*/
static void rbuDeleteOalFile(sqlite3rbu *p){
  char *zOal = rbuMPrintf(p, "%s-oal", p->zTarget);
  if( zOal ){
    sqlite3_vfs *pVfs = sqlite3_vfs_find(0);
    assert( pVfs && p->rc==SQLITE_OK && p->zErrmsg==0 );
    pVfs->xDelete(pVfs, zOal, 0);
    sqlite3_free(zOal);
  }
}

/*
** Allocate a private rbu VFS for the rbu handle passed as the only
** argument. This VFS will be used unless the call to sqlite3rbu_open()
** specified a URI with a vfs=? option in place of a target database
** file name.
*/
static void rbuCreateVfs(sqlite3rbu *p){
  int rnd;
  char zRnd[64];

  assert( p->rc==SQLITE_OK );
  sqlite3_randomness(sizeof(int), (void*)&rnd);
  sqlite3_snprintf(sizeof(zRnd), zRnd, "rbu_vfs_%d", rnd);
  p->rc = sqlite3rbu_create_vfs(zRnd, 0);
  if( p->rc==SQLITE_OK ){
    sqlite3_vfs *pVfs = sqlite3_vfs_find(zRnd);
    assert( pVfs );
    p->zVfsName = pVfs->zName;
    ((rbu_vfs*)pVfs)->pRbu = p;
  }
}

/*
** Destroy the private VFS created for the rbu handle passed as the only
** argument by an earlier call to rbuCreateVfs().
*/
static void rbuDeleteVfs(sqlite3rbu *p){
  if( p->zVfsName ){
    sqlite3rbu_destroy_vfs(p->zVfsName);
    p->zVfsName = 0;
  }
}

/*
** This user-defined SQL function is invoked with a single argument - the
** name of a table expected to appear in the target database. It returns
** the number of auxilliary indexes on the table.
*/
static void rbuIndexCntFunc(
  sqlite3_context *pCtx, 
  int nVal,
  sqlite3_value **apVal
){
  sqlite3rbu *p = (sqlite3rbu*)sqlite3_user_data(pCtx);
  sqlite3_stmt *pStmt = 0;
  char *zErrmsg = 0;
  int rc;
  sqlite3 *db = (rbuIsVacuum(p) ? p->dbRbu : p->dbMain);

  assert( nVal==1 );
  
  rc = prepareFreeAndCollectError(db, &pStmt, &zErrmsg, 
      sqlite3_mprintf("SELECT count(*) FROM sqlite_master "
        "WHERE type='index' AND tbl_name = %Q", sqlite3_value_text(apVal[0]))
  );
  if( rc!=SQLITE_OK ){
    sqlite3_result_error(pCtx, zErrmsg, -1);
  }else{
    int nIndex = 0;
    if( SQLITE_ROW==sqlite3_step(pStmt) ){
      nIndex = sqlite3_column_int(pStmt, 0);
    }
    rc = sqlite3_finalize(pStmt);
    if( rc==SQLITE_OK ){
      sqlite3_result_int(pCtx, nIndex);
    }else{
      sqlite3_result_error(pCtx, sqlite3_errmsg(db), -1);
    }
  }

  sqlite3_free(zErrmsg);
}

/*
** If the RBU database contains the rbu_count table, use it to initialize
** the sqlite3rbu.nPhaseOneStep variable. The schema of the rbu_count table
** is assumed to contain the same columns as:
**
**   CREATE TABLE rbu_count(tbl TEXT PRIMARY KEY, cnt INTEGER) WITHOUT ROWID;
**
** There should be one row in the table for each data_xxx table in the
** database. The 'tbl' column should contain the name of a data_xxx table,
** and the cnt column the number of rows it contains.
**
** sqlite3rbu.nPhaseOneStep is initialized to the sum of (1 + nIndex) * cnt
** for all rows in the rbu_count table, where nIndex is the number of 
** indexes on the corresponding target database table.
*/
static void rbuInitPhaseOneSteps(sqlite3rbu *p){
  if( p->rc==SQLITE_OK ){
    sqlite3_stmt *pStmt = 0;
    int bExists = 0;                /* True if rbu_count exists */

    p->nPhaseOneStep = -1;

    p->rc = sqlite3_create_function(p->dbRbu, 
        "rbu_index_cnt", 1, SQLITE_UTF8, (void*)p, rbuIndexCntFunc, 0, 0
    );
  
    /* Check for the rbu_count table. If it does not exist, or if an error
    ** occurs, nPhaseOneStep will be left set to -1. */
    if( p->rc==SQLITE_OK ){
      p->rc = prepareAndCollectError(p->dbRbu, &pStmt, &p->zErrmsg,
          "SELECT 1 FROM sqlite_master WHERE tbl_name = 'rbu_count'"
      );
    }
    if( p->rc==SQLITE_OK ){
      if( SQLITE_ROW==sqlite3_step(pStmt) ){
        bExists = 1;
      }
      p->rc = sqlite3_finalize(pStmt);
    }
  
    if( p->rc==SQLITE_OK && bExists ){
      p->rc = prepareAndCollectError(p->dbRbu, &pStmt, &p->zErrmsg,
          "SELECT sum(cnt * (1 + rbu_index_cnt(rbu_target_name(tbl))))"
          "FROM rbu_count"
      );
      if( p->rc==SQLITE_OK ){
        if( SQLITE_ROW==sqlite3_step(pStmt) ){
          p->nPhaseOneStep = sqlite3_column_int64(pStmt, 0);
        }
        p->rc = sqlite3_finalize(pStmt);
      }
    }
  }
}


static sqlite3rbu *openRbuHandle(
  const char *zTarget, 
  const char *zRbu,
  const char *zState
){
  sqlite3rbu *p;
  size_t nTarget = zTarget ? strlen(zTarget) : 0;
  size_t nRbu = strlen(zRbu);
  size_t nByte = sizeof(sqlite3rbu) + nTarget+1 + nRbu+1;

  p = (sqlite3rbu*)sqlite3_malloc64(nByte);
  if( p ){
    RbuState *pState = 0;

    /* Create the custom VFS. */
    memset(p, 0, sizeof(sqlite3rbu));
    rbuCreateVfs(p);

    /* Open the target, RBU and state databases */
    if( p->rc==SQLITE_OK ){
      char *pCsr = (char*)&p[1];
      int bRetry = 0;
      if( zTarget ){
        p->zTarget = pCsr;
        memcpy(p->zTarget, zTarget, nTarget+1);
        pCsr += nTarget+1;
      }
      p->zRbu = pCsr;
      memcpy(p->zRbu, zRbu, nRbu+1);
      pCsr += nRbu+1;
      if( zState ){
        p->zState = rbuMPrintf(p, "%s", zState);
      }

      /* If the first attempt to open the database file fails and the bRetry
      ** flag it set, this means that the db was not opened because it seemed
      ** to be a wal-mode db. But, this may have happened due to an earlier
      ** RBU vacuum operation leaving an old wal file in the directory.
      ** If this is the case, it will have been checkpointed and deleted
      ** when the handle was closed and a second attempt to open the 
      ** database may succeed.  */
      rbuOpenDatabase(p, &bRetry);
      if( bRetry ){
        rbuOpenDatabase(p, 0);
      }
    }

    if( p->rc==SQLITE_OK ){
      pState = rbuLoadState(p);
      assert( pState || p->rc!=SQLITE_OK );
      if( p->rc==SQLITE_OK ){

        if( pState->eStage==0 ){ 
          rbuDeleteOalFile(p);
          rbuInitPhaseOneSteps(p);
          p->eStage = RBU_STAGE_OAL;
        }else{
          p->eStage = pState->eStage;
          p->nPhaseOneStep = pState->nPhaseOneStep;
        }
        p->nProgress = pState->nProgress;
        p->iOalSz = pState->iOalSz;
      }
    }
    assert( p->rc!=SQLITE_OK || p->eStage!=0 );

    if( p->rc==SQLITE_OK && p->pTargetFd->pWalFd ){
      if( p->eStage==RBU_STAGE_OAL ){
        p->rc = SQLITE_ERROR;
        p->zErrmsg = sqlite3_mprintf("cannot update wal mode database");
      }else if( p->eStage==RBU_STAGE_MOVE ){
        p->eStage = RBU_STAGE_CKPT;
        p->nStep = 0;
      }
    }

    if( p->rc==SQLITE_OK 
     && (p->eStage==RBU_STAGE_OAL || p->eStage==RBU_STAGE_MOVE)
     && pState->eStage!=0
    ){
      rbu_file *pFd = (rbuIsVacuum(p) ? p->pRbuFd : p->pTargetFd);
      if( pFd->iCookie!=pState->iCookie ){   
        /* At this point (pTargetFd->iCookie) contains the value of the
        ** change-counter cookie (the thing that gets incremented when a 
        ** transaction is committed in rollback mode) currently stored on 
        ** page 1 of the database file. */
        p->rc = SQLITE_BUSY;
        p->zErrmsg = sqlite3_mprintf("database modified during rbu %s",
            (rbuIsVacuum(p) ? "vacuum" : "update")
        );
      }
    }

    if( p->rc==SQLITE_OK ){
      if( p->eStage==RBU_STAGE_OAL ){
        sqlite3 *db = p->dbMain;
        p->rc = sqlite3_exec(p->dbRbu, "BEGIN", 0, 0, &p->zErrmsg);

        /* Point the object iterator at the first object */
        if( p->rc==SQLITE_OK ){
          p->rc = rbuObjIterFirst(p, &p->objiter);
        }

        /* If the RBU database contains no data_xxx tables, declare the RBU
        ** update finished.  */
        if( p->rc==SQLITE_OK && p->objiter.zTbl==0 ){
          p->rc = SQLITE_DONE;
          p->eStage = RBU_STAGE_DONE;
        }else{
          if( p->rc==SQLITE_OK && pState->eStage==0 && rbuIsVacuum(p) ){
            rbuCopyPragma(p, "page_size");
            rbuCopyPragma(p, "auto_vacuum");
          }

          /* Open transactions both databases. The *-oal file is opened or
          ** created at this point. */
          if( p->rc==SQLITE_OK ){
            p->rc = sqlite3_exec(db, "BEGIN IMMEDIATE", 0, 0, &p->zErrmsg);
          }

          /* Check if the main database is a zipvfs db. If it is, set the upper
          ** level pager to use "journal_mode=off". This prevents it from 
          ** generating a large journal using a temp file.  */
          if( p->rc==SQLITE_OK ){
            int frc = sqlite3_file_control(db, "main", SQLITE_FCNTL_ZIPVFS, 0);
            if( frc==SQLITE_OK ){
              p->rc = sqlite3_exec(
                db, "PRAGMA journal_mode=off",0,0,&p->zErrmsg);
            }
          }

          if( p->rc==SQLITE_OK ){
            rbuSetupOal(p, pState);
          }
        }
      }else if( p->eStage==RBU_STAGE_MOVE ){
        /* no-op */
      }else if( p->eStage==RBU_STAGE_CKPT ){
        rbuSetupCheckpoint(p, pState);
      }else if( p->eStage==RBU_STAGE_DONE ){
        p->rc = SQLITE_DONE;
      }else{
        p->rc = SQLITE_CORRUPT;
      }
    }

    rbuFreeState(pState);
  }

  return p;
}

/*
** Allocate and return an RBU handle with all fields zeroed except for the
** error code, which is set to SQLITE_MISUSE.
*/
static sqlite3rbu *rbuMisuseError(void){
  sqlite3rbu *pRet;
  pRet = sqlite3_malloc64(sizeof(sqlite3rbu));
  if( pRet ){
    memset(pRet, 0, sizeof(sqlite3rbu));
    pRet->rc = SQLITE_MISUSE;
  }
  return pRet;
}

/*
** Open and return a new RBU handle. 
*/
SQLITE_API sqlite3rbu *sqlite3rbu_open(
  const char *zTarget, 
  const char *zRbu,
  const char *zState
){
  if( zTarget==0 || zRbu==0 ){ return rbuMisuseError(); }
  /* TODO: Check that zTarget and zRbu are non-NULL */
  return openRbuHandle(zTarget, zRbu, zState);
}

/*
** Open a handle to begin or resume an RBU VACUUM operation.
*/
SQLITE_API sqlite3rbu *sqlite3rbu_vacuum(
  const char *zTarget, 
  const char *zState
){
  if( zTarget==0 ){ return rbuMisuseError(); }
  if( zState ){
    int n = strlen(zState);
    if( n>=7 && 0==memcmp("-vactmp", &zState[n-7], 7) ){
      return rbuMisuseError();
    }
  }
  /* TODO: Check that both arguments are non-NULL */
  return openRbuHandle(0, zTarget, zState);
}

/*
** Return the database handle used by pRbu.
*/
SQLITE_API sqlite3 *sqlite3rbu_db(sqlite3rbu *pRbu, int bRbu){
  sqlite3 *db = 0;
  if( pRbu ){
    db = (bRbu ? pRbu->dbRbu : pRbu->dbMain);
  }
  return db;
}


/*
** If the error code currently stored in the RBU handle is SQLITE_CONSTRAINT,
** then edit any error message string so as to remove all occurrences of
** the pattern "rbu_imp_[0-9]*".
*/
static void rbuEditErrmsg(sqlite3rbu *p){
  if( p->rc==SQLITE_CONSTRAINT && p->zErrmsg ){
    unsigned int i;
    size_t nErrmsg = strlen(p->zErrmsg);
    for(i=0; i<(nErrmsg-8); i++){
      if( memcmp(&p->zErrmsg[i], "rbu_imp_", 8)==0 ){
        int nDel = 8;
        while( p->zErrmsg[i+nDel]>='0' && p->zErrmsg[i+nDel]<='9' ) nDel++;
        memmove(&p->zErrmsg[i], &p->zErrmsg[i+nDel], nErrmsg + 1 - i - nDel);
        nErrmsg -= nDel;
      }
    }
  }
}

/*
** Close the RBU handle.
*/
SQLITE_API int sqlite3rbu_close(sqlite3rbu *p, char **pzErrmsg){
  int rc;
  if( p ){

    /* Commit the transaction to the *-oal file. */
    if( p->rc==SQLITE_OK && p->eStage==RBU_STAGE_OAL ){
      p->rc = sqlite3_exec(p->dbMain, "COMMIT", 0, 0, &p->zErrmsg);
    }

    /* Sync the db file if currently doing an incremental checkpoint */
    if( p->rc==SQLITE_OK && p->eStage==RBU_STAGE_CKPT ){
      sqlite3_file *pDb = p->pTargetFd->pReal;
      p->rc = pDb->pMethods->xSync(pDb, SQLITE_SYNC_NORMAL);
    }

    rbuSaveState(p, p->eStage);

    if( p->rc==SQLITE_OK && p->eStage==RBU_STAGE_OAL ){
      p->rc = sqlite3_exec(p->dbRbu, "COMMIT", 0, 0, &p->zErrmsg);
    }

    /* Close any open statement handles. */
    rbuObjIterFinalize(&p->objiter);

    /* If this is an RBU vacuum handle and the vacuum has either finished
    ** successfully or encountered an error, delete the contents of the 
    ** state table. This causes the next call to sqlite3rbu_vacuum() 
    ** specifying the current target and state databases to start a new
    ** vacuum from scratch.  */
    if( rbuIsVacuum(p) && p->rc!=SQLITE_OK && p->dbRbu ){
      int rc2 = sqlite3_exec(p->dbRbu, "DELETE FROM stat.rbu_state", 0, 0, 0);
      if( p->rc==SQLITE_DONE && rc2!=SQLITE_OK ) p->rc = rc2;
    }

    /* Close the open database handle and VFS object. */
    sqlite3_close(p->dbRbu);
    sqlite3_close(p->dbMain);
    assert( p->szTemp==0 );
    rbuDeleteVfs(p);
    sqlite3_free(p->aBuf);
    sqlite3_free(p->aFrame);

    rbuEditErrmsg(p);
    rc = p->rc;
    if( pzErrmsg ){
      *pzErrmsg = p->zErrmsg;
    }else{
      sqlite3_free(p->zErrmsg);
    }
    sqlite3_free(p->zState);
    sqlite3_free(p);
  }else{
    rc = SQLITE_NOMEM;
    *pzErrmsg = 0;
  }
  return rc;
}

/*
** Return the total number of key-value operations (inserts, deletes or 
** updates) that have been performed on the target database since the
** current RBU update was started.
*/
SQLITE_API sqlite3_int64 sqlite3rbu_progress(sqlite3rbu *pRbu){
  return pRbu->nProgress;
}

/*
** Return permyriadage progress indications for the two main stages of
** an RBU update.
*/
SQLITE_API void sqlite3rbu_bp_progress(sqlite3rbu *p, int *pnOne, int *pnTwo){
  const int MAX_PROGRESS = 10000;
  switch( p->eStage ){
    case RBU_STAGE_OAL:
      if( p->nPhaseOneStep>0 ){
        *pnOne = (int)(MAX_PROGRESS * (i64)p->nProgress/(i64)p->nPhaseOneStep);
      }else{
        *pnOne = -1;
      }
      *pnTwo = 0;
      break;

    case RBU_STAGE_MOVE:
      *pnOne = MAX_PROGRESS;
      *pnTwo = 0;
      break;

    case RBU_STAGE_CKPT:
      *pnOne = MAX_PROGRESS;
      *pnTwo = (int)(MAX_PROGRESS * (i64)p->nStep / (i64)p->nFrame);
      break;

    case RBU_STAGE_DONE:
      *pnOne = MAX_PROGRESS;
      *pnTwo = MAX_PROGRESS;
      break;

    default:
      assert( 0 );
  }
}

/*
** Return the current state of the RBU vacuum or update operation.
*/
SQLITE_API int sqlite3rbu_state(sqlite3rbu *p){
  int aRes[] = {
    0, SQLITE_RBU_STATE_OAL, SQLITE_RBU_STATE_MOVE,
    0, SQLITE_RBU_STATE_CHECKPOINT, SQLITE_RBU_STATE_DONE
  };

  assert( RBU_STAGE_OAL==1 );
  assert( RBU_STAGE_MOVE==2 );
  assert( RBU_STAGE_CKPT==4 );
  assert( RBU_STAGE_DONE==5 );
  assert( aRes[RBU_STAGE_OAL]==SQLITE_RBU_STATE_OAL );
  assert( aRes[RBU_STAGE_MOVE]==SQLITE_RBU_STATE_MOVE );
  assert( aRes[RBU_STAGE_CKPT]==SQLITE_RBU_STATE_CHECKPOINT );
  assert( aRes[RBU_STAGE_DONE]==SQLITE_RBU_STATE_DONE );

  if( p->rc!=SQLITE_OK && p->rc!=SQLITE_DONE ){
    return SQLITE_RBU_STATE_ERROR;
  }else{
    assert( p->rc!=SQLITE_DONE || p->eStage==RBU_STAGE_DONE );
    assert( p->eStage==RBU_STAGE_OAL
         || p->eStage==RBU_STAGE_MOVE
         || p->eStage==RBU_STAGE_CKPT
         || p->eStage==RBU_STAGE_DONE
    );
    return aRes[p->eStage];
  }
}

SQLITE_API int sqlite3rbu_savestate(sqlite3rbu *p){
  int rc = p->rc;
  if( rc==SQLITE_DONE ) return SQLITE_OK;

  assert( p->eStage>=RBU_STAGE_OAL && p->eStage<=RBU_STAGE_DONE );
  if( p->eStage==RBU_STAGE_OAL ){
    assert( rc!=SQLITE_DONE );
    if( rc==SQLITE_OK ) rc = sqlite3_exec(p->dbMain, "COMMIT", 0, 0, 0);
  }

  /* Sync the db file */
  if( rc==SQLITE_OK && p->eStage==RBU_STAGE_CKPT ){
    sqlite3_file *pDb = p->pTargetFd->pReal;
    rc = pDb->pMethods->xSync(pDb, SQLITE_SYNC_NORMAL);
  }

  p->rc = rc;
  rbuSaveState(p, p->eStage);
  rc = p->rc;

  if( p->eStage==RBU_STAGE_OAL ){
    assert( rc!=SQLITE_DONE );
    if( rc==SQLITE_OK ) rc = sqlite3_exec(p->dbRbu, "COMMIT", 0, 0, 0);
    if( rc==SQLITE_OK ){ 
      const char *zBegin = rbuIsVacuum(p) ? "BEGIN" : "BEGIN IMMEDIATE";
      rc = sqlite3_exec(p->dbRbu, zBegin, 0, 0, 0);
    }
    if( rc==SQLITE_OK ) rc = sqlite3_exec(p->dbMain, "BEGIN IMMEDIATE", 0, 0,0);
  }

  p->rc = rc;
  return rc;
}

/**************************************************************************
** Beginning of RBU VFS shim methods. The VFS shim modifies the behaviour
** of a standard VFS in the following ways:
**
** 1. Whenever the first page of a main database file is read or 
**    written, the value of the change-counter cookie is stored in
**    rbu_file.iCookie. Similarly, the value of the "write-version"
**    database header field is stored in rbu_file.iWriteVer. This ensures
**    that the values are always trustworthy within an open transaction.
**
** 2. Whenever an SQLITE_OPEN_WAL file is opened, the (rbu_file.pWalFd)
**    member variable of the associated database file descriptor is set
**    to point to the new file. A mutex protected linked list of all main 
**    db fds opened using a particular RBU VFS is maintained at 
**    rbu_vfs.pMain to facilitate this.
**
** 3. Using a new file-control "SQLITE_FCNTL_RBU", a main db rbu_file 
**    object can be marked as the target database of an RBU update. This
**    turns on the following extra special behaviour:
**
** 3a. If xAccess() is called to check if there exists a *-wal file 
**     associated with an RBU target database currently in RBU_STAGE_OAL
**     stage (preparing the *-oal file), the following special handling
**     applies:
**
**      * if the *-wal file does exist, return SQLITE_CANTOPEN. An RBU
**        target database may not be in wal mode already.
**
**      * if the *-wal file does not exist, set the output parameter to
**        non-zero (to tell SQLite that it does exist) anyway.
**
**     Then, when xOpen() is called to open the *-wal file associated with
**     the RBU target in RBU_STAGE_OAL stage, instead of opening the *-wal
**     file, the rbu vfs opens the corresponding *-oal file instead. 
**
** 3b. The *-shm pages returned by xShmMap() for a target db file in
**     RBU_STAGE_OAL mode are actually stored in heap memory. This is to
**     avoid creating a *-shm file on disk. Additionally, xShmLock() calls
**     are no-ops on target database files in RBU_STAGE_OAL mode. This is
**     because assert() statements in some VFS implementations fail if 
**     xShmLock() is called before xShmMap().
**
** 3c. If an EXCLUSIVE lock is attempted on a target database file in any
**     mode except RBU_STAGE_DONE (all work completed and checkpointed), it 
**     fails with an SQLITE_BUSY error. This is to stop RBU connections
**     from automatically checkpointing a *-wal (or *-oal) file from within
**     sqlite3_close().
**
** 3d. In RBU_STAGE_CAPTURE mode, all xRead() calls on the wal file, and
**     all xWrite() calls on the target database file perform no IO. 
**     Instead the frame and page numbers that would be read and written
**     are recorded. Additionally, successful attempts to obtain exclusive
**     xShmLock() WRITER, CHECKPOINTER and READ0 locks on the target 
**     database file are recorded. xShmLock() calls to unlock the same
**     locks are no-ops (so that once obtained, these locks are never
**     relinquished). Finally, calls to xSync() on the target database
**     file fail with SQLITE_INTERNAL errors.
*/

static void rbuUnlockShm(rbu_file *p){
  assert( p->openFlags & SQLITE_OPEN_MAIN_DB );
  if( p->pRbu ){
    int (*xShmLock)(sqlite3_file*,int,int,int) = p->pReal->pMethods->xShmLock;
    int i;
    for(i=0; i<SQLITE_SHM_NLOCK;i++){
      if( (1<<i) & p->pRbu->mLock ){
        xShmLock(p->pReal, i, 1, SQLITE_SHM_UNLOCK|SQLITE_SHM_EXCLUSIVE);
      }
    }
    p->pRbu->mLock = 0;
  }
}

/*
*/
static int rbuUpdateTempSize(rbu_file *pFd, sqlite3_int64 nNew){
  sqlite3rbu *pRbu = pFd->pRbu;
  i64 nDiff = nNew - pFd->sz;
  pRbu->szTemp += nDiff;
  pFd->sz = nNew;
  assert( pRbu->szTemp>=0 );
  if( pRbu->szTempLimit && pRbu->szTemp>pRbu->szTempLimit ) return SQLITE_FULL;
  return SQLITE_OK;
}

/*
** Add an item to the main-db lists, if it is not already present.
**
** There are two main-db lists. One for all file descriptors, and one
** for all file descriptors with rbu_file.pDb!=0. If the argument has
** rbu_file.pDb!=0, then it is assumed to already be present on the
** main list and is only added to the pDb!=0 list.
*/
static void rbuMainlistAdd(rbu_file *p){
  rbu_vfs *pRbuVfs = p->pRbuVfs;
  rbu_file *pIter;
  assert( (p->openFlags & SQLITE_OPEN_MAIN_DB) );
  sqlite3_mutex_enter(pRbuVfs->mutex);
  if( p->pRbu==0 ){
    for(pIter=pRbuVfs->pMain; pIter; pIter=pIter->pMainNext);
    p->pMainNext = pRbuVfs->pMain;
    pRbuVfs->pMain = p;
  }else{
    for(pIter=pRbuVfs->pMainRbu; pIter && pIter!=p; pIter=pIter->pMainRbuNext){}
    if( pIter==0 ){
      p->pMainRbuNext = pRbuVfs->pMainRbu;
      pRbuVfs->pMainRbu = p;
    }
  }
  sqlite3_mutex_leave(pRbuVfs->mutex);
}

/*
** Remove an item from the main-db lists.
*/
static void rbuMainlistRemove(rbu_file *p){
  rbu_file **pp;
  sqlite3_mutex_enter(p->pRbuVfs->mutex);
  for(pp=&p->pRbuVfs->pMain; *pp && *pp!=p; pp=&((*pp)->pMainNext)){}
  if( *pp ) *pp = p->pMainNext;
  p->pMainNext = 0;
  for(pp=&p->pRbuVfs->pMainRbu; *pp && *pp!=p; pp=&((*pp)->pMainRbuNext)){}
  if( *pp ) *pp = p->pMainRbuNext;
  p->pMainRbuNext = 0;
  sqlite3_mutex_leave(p->pRbuVfs->mutex);
}

/*
** Given that zWal points to a buffer containing a wal file name passed to 
** either the xOpen() or xAccess() VFS method, search the main-db list for
** a file-handle opened by the same database connection on the corresponding
** database file.
**
** If parameter bRbu is true, only search for file-descriptors with
** rbu_file.pDb!=0.
*/
static rbu_file *rbuFindMaindb(rbu_vfs *pRbuVfs, const char *zWal, int bRbu){
  rbu_file *pDb;
  sqlite3_mutex_enter(pRbuVfs->mutex);
  if( bRbu ){
    for(pDb=pRbuVfs->pMainRbu; pDb && pDb->zWal!=zWal; pDb=pDb->pMainRbuNext){}
  }else{
    for(pDb=pRbuVfs->pMain; pDb && pDb->zWal!=zWal; pDb=pDb->pMainNext){}
  }
  sqlite3_mutex_leave(pRbuVfs->mutex);
  return pDb;
}

/*
** Close an rbu file.
*/
static int rbuVfsClose(sqlite3_file *pFile){
  rbu_file *p = (rbu_file*)pFile;
  int rc;
  int i;

  /* Free the contents of the apShm[] array. And the array itself. */
  for(i=0; i<p->nShm; i++){
    sqlite3_free(p->apShm[i]);
  }
  sqlite3_free(p->apShm);
  p->apShm = 0;
  sqlite3_free(p->zDel);

  if( p->openFlags & SQLITE_OPEN_MAIN_DB ){
    rbuMainlistRemove(p);
    rbuUnlockShm(p);
    p->pReal->pMethods->xShmUnmap(p->pReal, 0);
  }
  else if( (p->openFlags & SQLITE_OPEN_DELETEONCLOSE) && p->pRbu ){
    rbuUpdateTempSize(p, 0);
  }
  assert( p->pMainNext==0 && p->pRbuVfs->pMain!=p );

  /* Close the underlying file handle */
  rc = p->pReal->pMethods->xClose(p->pReal);
  return rc;
}


/*
** Read and return an unsigned 32-bit big-endian integer from the buffer 
** passed as the only argument.
*/
static u32 rbuGetU32(u8 *aBuf){
  return ((u32)aBuf[0] << 24)
       + ((u32)aBuf[1] << 16)
       + ((u32)aBuf[2] <<  8)
       + ((u32)aBuf[3]);
}

/*
** Write an unsigned 32-bit value in big-endian format to the supplied
** buffer.
*/
static void rbuPutU32(u8 *aBuf, u32 iVal){
  aBuf[0] = (iVal >> 24) & 0xFF;
  aBuf[1] = (iVal >> 16) & 0xFF;
  aBuf[2] = (iVal >>  8) & 0xFF;
  aBuf[3] = (iVal >>  0) & 0xFF;
}

static void rbuPutU16(u8 *aBuf, u16 iVal){
  aBuf[0] = (iVal >>  8) & 0xFF;
  aBuf[1] = (iVal >>  0) & 0xFF;
}

/*
** Read data from an rbuVfs-file.
*/
static int rbuVfsRead(
  sqlite3_file *pFile, 
  void *zBuf, 
  int iAmt, 
  sqlite_int64 iOfst
){
  rbu_file *p = (rbu_file*)pFile;
  sqlite3rbu *pRbu = p->pRbu;
  int rc;

  if( pRbu && pRbu->eStage==RBU_STAGE_CAPTURE ){
    assert( p->openFlags & SQLITE_OPEN_WAL );
    rc = rbuCaptureWalRead(p->pRbu, iOfst, iAmt);
  }else{
    if( pRbu && pRbu->eStage==RBU_STAGE_OAL 
     && (p->openFlags & SQLITE_OPEN_WAL) 
     && iOfst>=pRbu->iOalSz 
    ){
      rc = SQLITE_OK;
      memset(zBuf, 0, iAmt);
    }else{
      rc = p->pReal->pMethods->xRead(p->pReal, zBuf, iAmt, iOfst);
#if 1
      /* If this is being called to read the first page of the target 
      ** database as part of an rbu vacuum operation, synthesize the 
      ** contents of the first page if it does not yet exist. Otherwise,
      ** SQLite will not check for a *-wal file.  */
      if( pRbu && rbuIsVacuum(pRbu) 
          && rc==SQLITE_IOERR_SHORT_READ && iOfst==0
          && (p->openFlags & SQLITE_OPEN_MAIN_DB)
          && pRbu->rc==SQLITE_OK
      ){
        sqlite3_file *pFd = (sqlite3_file*)pRbu->pRbuFd;
        rc = pFd->pMethods->xRead(pFd, zBuf, iAmt, iOfst);
        if( rc==SQLITE_OK ){
          u8 *aBuf = (u8*)zBuf;
          u32 iRoot = rbuGetU32(&aBuf[52]) ? 1 : 0;
          rbuPutU32(&aBuf[52], iRoot);      /* largest root page number */
          rbuPutU32(&aBuf[36], 0);          /* number of free pages */
          rbuPutU32(&aBuf[32], 0);          /* first page on free list trunk */
          rbuPutU32(&aBuf[28], 1);          /* size of db file in pages */
          rbuPutU32(&aBuf[24], pRbu->pRbuFd->iCookie+1);  /* Change counter */

          if( iAmt>100 ){
            memset(&aBuf[100], 0, iAmt-100);
            rbuPutU16(&aBuf[105], iAmt & 0xFFFF);
            aBuf[100] = 0x0D;
          }
        }
      }
#endif
    }
    if( rc==SQLITE_OK && iOfst==0 && (p->openFlags & SQLITE_OPEN_MAIN_DB) ){
      /* These look like magic numbers. But they are stable, as they are part
       ** of the definition of the SQLite file format, which may not change. */
      u8 *pBuf = (u8*)zBuf;
      p->iCookie = rbuGetU32(&pBuf[24]);
      p->iWriteVer = pBuf[19];
    }
  }
  return rc;
}

/*
** Write data to an rbuVfs-file.
*/
static int rbuVfsWrite(
  sqlite3_file *pFile, 
  const void *zBuf, 
  int iAmt, 
  sqlite_int64 iOfst
){
  rbu_file *p = (rbu_file*)pFile;
  sqlite3rbu *pRbu = p->pRbu;
  int rc;

  if( pRbu && pRbu->eStage==RBU_STAGE_CAPTURE ){
    assert( p->openFlags & SQLITE_OPEN_MAIN_DB );
    rc = rbuCaptureDbWrite(p->pRbu, iOfst);
  }else{
    if( pRbu ){
      if( pRbu->eStage==RBU_STAGE_OAL 
       && (p->openFlags & SQLITE_OPEN_WAL) 
       && iOfst>=pRbu->iOalSz
      ){
        pRbu->iOalSz = iAmt + iOfst;
      }else if( p->openFlags & SQLITE_OPEN_DELETEONCLOSE ){
        i64 szNew = iAmt+iOfst;
        if( szNew>p->sz ){
          rc = rbuUpdateTempSize(p, szNew);
          if( rc!=SQLITE_OK ) return rc;
        }
      }
    }
    rc = p->pReal->pMethods->xWrite(p->pReal, zBuf, iAmt, iOfst);
    if( rc==SQLITE_OK && iOfst==0 && (p->openFlags & SQLITE_OPEN_MAIN_DB) ){
      /* These look like magic numbers. But they are stable, as they are part
      ** of the definition of the SQLite file format, which may not change. */
      u8 *pBuf = (u8*)zBuf;
      p->iCookie = rbuGetU32(&pBuf[24]);
      p->iWriteVer = pBuf[19];
    }
  }
  return rc;
}

/*
** Truncate an rbuVfs-file.
*/
static int rbuVfsTruncate(sqlite3_file *pFile, sqlite_int64 size){
  rbu_file *p = (rbu_file*)pFile;
  if( (p->openFlags & SQLITE_OPEN_DELETEONCLOSE) && p->pRbu ){
    int rc = rbuUpdateTempSize(p, size);
    if( rc!=SQLITE_OK ) return rc;
  }
  return p->pReal->pMethods->xTruncate(p->pReal, size);
}

/*
** Sync an rbuVfs-file.
*/
static int rbuVfsSync(sqlite3_file *pFile, int flags){
  rbu_file *p = (rbu_file *)pFile;
  if( p->pRbu && p->pRbu->eStage==RBU_STAGE_CAPTURE ){
    if( p->openFlags & SQLITE_OPEN_MAIN_DB ){
      return SQLITE_INTERNAL;
    }
    return SQLITE_OK;
  }
  return p->pReal->pMethods->xSync(p->pReal, flags);
}

/*
** Return the current file-size of an rbuVfs-file.
*/
static int rbuVfsFileSize(sqlite3_file *pFile, sqlite_int64 *pSize){
  rbu_file *p = (rbu_file *)pFile;
  int rc;
  rc = p->pReal->pMethods->xFileSize(p->pReal, pSize);

  /* If this is an RBU vacuum operation and this is the target database,
  ** pretend that it has at least one page. Otherwise, SQLite will not
  ** check for the existance of a *-wal file. rbuVfsRead() contains 
  ** similar logic.  */
  if( rc==SQLITE_OK && *pSize==0 
   && p->pRbu && rbuIsVacuum(p->pRbu) 
   && (p->openFlags & SQLITE_OPEN_MAIN_DB)
  ){
    *pSize = 1024;
  }
  return rc;
}

/*
** Lock an rbuVfs-file.
*/
static int rbuVfsLock(sqlite3_file *pFile, int eLock){
  rbu_file *p = (rbu_file*)pFile;
  sqlite3rbu *pRbu = p->pRbu;
  int rc = SQLITE_OK;

  assert( p->openFlags & (SQLITE_OPEN_MAIN_DB|SQLITE_OPEN_TEMP_DB) );
  if( eLock==SQLITE_LOCK_EXCLUSIVE 
   && (p->bNolock || (pRbu && pRbu->eStage!=RBU_STAGE_DONE))
  ){
    /* Do not allow EXCLUSIVE locks. Preventing SQLite from taking this 
    ** prevents it from checkpointing the database from sqlite3_close(). */
    rc = SQLITE_BUSY;
  }else{
    rc = p->pReal->pMethods->xLock(p->pReal, eLock);
  }

  return rc;
}

/*
** Unlock an rbuVfs-file.
*/
static int rbuVfsUnlock(sqlite3_file *pFile, int eLock){
  rbu_file *p = (rbu_file *)pFile;
  return p->pReal->pMethods->xUnlock(p->pReal, eLock);
}

/*
** Check if another file-handle holds a RESERVED lock on an rbuVfs-file.
*/
static int rbuVfsCheckReservedLock(sqlite3_file *pFile, int *pResOut){
  rbu_file *p = (rbu_file *)pFile;
  return p->pReal->pMethods->xCheckReservedLock(p->pReal, pResOut);
}

/*
** File control method. For custom operations on an rbuVfs-file.
*/
static int rbuVfsFileControl(sqlite3_file *pFile, int op, void *pArg){
  rbu_file *p = (rbu_file *)pFile;
  int (*xControl)(sqlite3_file*,int,void*) = p->pReal->pMethods->xFileControl;
  int rc;

  assert( p->openFlags & (SQLITE_OPEN_MAIN_DB|SQLITE_OPEN_TEMP_DB)
       || p->openFlags & (SQLITE_OPEN_TRANSIENT_DB|SQLITE_OPEN_TEMP_JOURNAL)
  );
  if( op==SQLITE_FCNTL_RBU ){
    sqlite3rbu *pRbu = (sqlite3rbu*)pArg;

    /* First try to find another RBU vfs lower down in the vfs stack. If
    ** one is found, this vfs will operate in pass-through mode. The lower
    ** level vfs will do the special RBU handling.  */
    rc = xControl(p->pReal, op, pArg);

    if( rc==SQLITE_NOTFOUND ){
      /* Now search for a zipvfs instance lower down in the VFS stack. If
      ** one is found, this is an error.  */
      void *dummy = 0;
      rc = xControl(p->pReal, SQLITE_FCNTL_ZIPVFS, &dummy);
      if( rc==SQLITE_OK ){
        rc = SQLITE_ERROR;
        pRbu->zErrmsg = sqlite3_mprintf("rbu/zipvfs setup error");
      }else if( rc==SQLITE_NOTFOUND ){
        pRbu->pTargetFd = p;
        p->pRbu = pRbu;
        rbuMainlistAdd(p);
        if( p->pWalFd ) p->pWalFd->pRbu = pRbu;
        rc = SQLITE_OK;
      }
    }
    return rc;
  }
  else if( op==SQLITE_FCNTL_RBUCNT ){
    sqlite3rbu *pRbu = (sqlite3rbu*)pArg;
    pRbu->nRbu++;
    pRbu->pRbuFd = p;
    p->bNolock = 1;
  }

  rc = xControl(p->pReal, op, pArg);
  if( rc==SQLITE_OK && op==SQLITE_FCNTL_VFSNAME ){
    rbu_vfs *pRbuVfs = p->pRbuVfs;
    char *zIn = *(char**)pArg;
    char *zOut = sqlite3_mprintf("rbu(%s)/%z", pRbuVfs->base.zName, zIn);
    *(char**)pArg = zOut;
    if( zOut==0 ) rc = SQLITE_NOMEM;
  }

  return rc;
}

/*
** Return the sector-size in bytes for an rbuVfs-file.
*/
static int rbuVfsSectorSize(sqlite3_file *pFile){
  rbu_file *p = (rbu_file *)pFile;
  return p->pReal->pMethods->xSectorSize(p->pReal);
}

/*
** Return the device characteristic flags supported by an rbuVfs-file.
*/
static int rbuVfsDeviceCharacteristics(sqlite3_file *pFile){
  rbu_file *p = (rbu_file *)pFile;
  return p->pReal->pMethods->xDeviceCharacteristics(p->pReal);
}

/*
** Take or release a shared-memory lock.
*/
static int rbuVfsShmLock(sqlite3_file *pFile, int ofst, int n, int flags){
  rbu_file *p = (rbu_file*)pFile;
  sqlite3rbu *pRbu = p->pRbu;
  int rc = SQLITE_OK;

#ifdef SQLITE_AMALGAMATION
    assert( WAL_CKPT_LOCK==1 );
#endif

  assert( p->openFlags & (SQLITE_OPEN_MAIN_DB|SQLITE_OPEN_TEMP_DB) );
  if( pRbu && (pRbu->eStage==RBU_STAGE_OAL || pRbu->eStage==RBU_STAGE_MOVE) ){
    /* Magic number 1 is the WAL_CKPT_LOCK lock. Preventing SQLite from
    ** taking this lock also prevents any checkpoints from occurring. 
    ** todo: really, it's not clear why this might occur, as 
    ** wal_autocheckpoint ought to be turned off.  */
    if( ofst==WAL_LOCK_CKPT && n==1 ) rc = SQLITE_BUSY;
  }else{
    int bCapture = 0;
    if( pRbu && pRbu->eStage==RBU_STAGE_CAPTURE ){
      bCapture = 1;
    }

    if( bCapture==0 || 0==(flags & SQLITE_SHM_UNLOCK) ){
      rc = p->pReal->pMethods->xShmLock(p->pReal, ofst, n, flags);
      if( bCapture && rc==SQLITE_OK ){
        pRbu->mLock |= (1 << ofst);
      }
    }
  }

  return rc;
}

/*
** Obtain a pointer to a mapping of a single 32KiB page of the *-shm file.
*/
static int rbuVfsShmMap(
  sqlite3_file *pFile, 
  int iRegion, 
  int szRegion, 
  int isWrite, 
  void volatile **pp
){
  rbu_file *p = (rbu_file*)pFile;
  int rc = SQLITE_OK;
  int eStage = (p->pRbu ? p->pRbu->eStage : 0);

  /* If not in RBU_STAGE_OAL, allow this call to pass through. Or, if this
  ** rbu is in the RBU_STAGE_OAL state, use heap memory for *-shm space 
  ** instead of a file on disk.  */
  assert( p->openFlags & (SQLITE_OPEN_MAIN_DB|SQLITE_OPEN_TEMP_DB) );
  if( eStage==RBU_STAGE_OAL ){
    sqlite3_int64 nByte = (iRegion+1) * sizeof(char*);
    char **apNew = (char**)sqlite3_realloc64(p->apShm, nByte);

    /* This is an RBU connection that uses its own heap memory for the
    ** pages of the *-shm file. Since no other process can have run
    ** recovery, the connection must request *-shm pages in order
    ** from start to finish.  */
    assert( iRegion==p->nShm );
    if( apNew==0 ){
      rc = SQLITE_NOMEM;
    }else{
      memset(&apNew[p->nShm], 0, sizeof(char*) * (1 + iRegion - p->nShm));
      p->apShm = apNew;
      p->nShm = iRegion+1;
    }

    if( rc==SQLITE_OK ){
      char *pNew = (char*)sqlite3_malloc64(szRegion);
      if( pNew==0 ){
        rc = SQLITE_NOMEM;
      }else{
        memset(pNew, 0, szRegion);
        p->apShm[iRegion] = pNew;
      }
    }

    if( rc==SQLITE_OK ){
      *pp = p->apShm[iRegion];
    }else{
      *pp = 0;
    }
  }else{
    assert( p->apShm==0 );
    rc = p->pReal->pMethods->xShmMap(p->pReal, iRegion, szRegion, isWrite, pp);
  }

  return rc;
}

/*
** Memory barrier.
*/
static void rbuVfsShmBarrier(sqlite3_file *pFile){
  rbu_file *p = (rbu_file *)pFile;
  p->pReal->pMethods->xShmBarrier(p->pReal);
}

/*
** The xShmUnmap method.
*/
static int rbuVfsShmUnmap(sqlite3_file *pFile, int delFlag){
  rbu_file *p = (rbu_file*)pFile;
  int rc = SQLITE_OK;
  int eStage = (p->pRbu ? p->pRbu->eStage : 0);

  assert( p->openFlags & (SQLITE_OPEN_MAIN_DB|SQLITE_OPEN_TEMP_DB) );
  if( eStage==RBU_STAGE_OAL || eStage==RBU_STAGE_MOVE ){
    /* no-op */
  }else{
    /* Release the checkpointer and writer locks */
    rbuUnlockShm(p);
    rc = p->pReal->pMethods->xShmUnmap(p->pReal, delFlag);
  }
  return rc;
}

/* 
** A main database named zName has just been opened. The following 
** function returns a pointer to a buffer owned by SQLite that contains
** the name of the *-wal file this db connection will use. SQLite
** happens to pass a pointer to this buffer when using xAccess()
** or xOpen() to operate on the *-wal file.  
*/
static const char *rbuMainToWal(const char *zName, int flags){
  int n = (int)strlen(zName);
  const char *z = &zName[n];
  if( flags & SQLITE_OPEN_URI ){
    int odd = 0;
    while( 1 ){
      if( z[0]==0 ){
        odd = 1 - odd;
        if( odd && z[1]==0 ) break;
      }
      z++;
    }
    z += 2;
  }else{
    while( *z==0 ) z++;
  }
  z += (n + 8 + 1);
  return z;
}

/*
** Open an rbu file handle.
*/
static int rbuVfsOpen(
  sqlite3_vfs *pVfs,
  const char *zName,
  sqlite3_file *pFile,
  int flags,
  int *pOutFlags
){
  static sqlite3_io_methods rbuvfs_io_methods = {
    2,                            /* iVersion */
    rbuVfsClose,                  /* xClose */
    rbuVfsRead,                   /* xRead */
    rbuVfsWrite,                  /* xWrite */
    rbuVfsTruncate,               /* xTruncate */
    rbuVfsSync,                   /* xSync */
    rbuVfsFileSize,               /* xFileSize */
    rbuVfsLock,                   /* xLock */
    rbuVfsUnlock,                 /* xUnlock */
    rbuVfsCheckReservedLock,      /* xCheckReservedLock */
    rbuVfsFileControl,            /* xFileControl */
    rbuVfsSectorSize,             /* xSectorSize */
    rbuVfsDeviceCharacteristics,  /* xDeviceCharacteristics */
    rbuVfsShmMap,                 /* xShmMap */
    rbuVfsShmLock,                /* xShmLock */
    rbuVfsShmBarrier,             /* xShmBarrier */
    rbuVfsShmUnmap,               /* xShmUnmap */
    0, 0                          /* xFetch, xUnfetch */
  };
  rbu_vfs *pRbuVfs = (rbu_vfs*)pVfs;
  sqlite3_vfs *pRealVfs = pRbuVfs->pRealVfs;
  rbu_file *pFd = (rbu_file *)pFile;
  int rc = SQLITE_OK;
  const char *zOpen = zName;
  int oflags = flags;

  memset(pFd, 0, sizeof(rbu_file));
  pFd->pReal = (sqlite3_file*)&pFd[1];
  pFd->pRbuVfs = pRbuVfs;
  pFd->openFlags = flags;
  if( zName ){
    if( flags & SQLITE_OPEN_MAIN_DB ){
      /* A main database has just been opened. The following block sets
      ** (pFd->zWal) to point to a buffer owned by SQLite that contains
      ** the name of the *-wal file this db connection will use. SQLite
      ** happens to pass a pointer to this buffer when using xAccess()
      ** or xOpen() to operate on the *-wal file.  */
      pFd->zWal = rbuMainToWal(zName, flags);
    }
    else if( flags & SQLITE_OPEN_WAL ){
      rbu_file *pDb = rbuFindMaindb(pRbuVfs, zName, 0);
      if( pDb ){
        if( pDb->pRbu && pDb->pRbu->eStage==RBU_STAGE_OAL ){
          /* This call is to open a *-wal file. Intead, open the *-oal. This
          ** code ensures that the string passed to xOpen() is terminated by a
          ** pair of '\0' bytes in case the VFS attempts to extract a URI 
          ** parameter from it.  */
          const char *zBase = zName;
          size_t nCopy;
          char *zCopy;
          if( rbuIsVacuum(pDb->pRbu) ){
            zBase = sqlite3_db_filename(pDb->pRbu->dbRbu, "main");
            zBase = rbuMainToWal(zBase, SQLITE_OPEN_URI);
          }
          nCopy = strlen(zBase);
          zCopy = sqlite3_malloc64(nCopy+2);
          if( zCopy ){
            memcpy(zCopy, zBase, nCopy);
            zCopy[nCopy-3] = 'o';
            zCopy[nCopy] = '\0';
            zCopy[nCopy+1] = '\0';
            zOpen = (const char*)(pFd->zDel = zCopy);
          }else{
            rc = SQLITE_NOMEM;
          }
          pFd->pRbu = pDb->pRbu;
        }
        pDb->pWalFd = pFd;
      }
    }
  }else{
    pFd->pRbu = pRbuVfs->pRbu;
  }

  if( oflags & SQLITE_OPEN_MAIN_DB 
   && sqlite3_uri_boolean(zName, "rbu_memory", 0) 
  ){
    assert( oflags & SQLITE_OPEN_MAIN_DB );
    oflags =  SQLITE_OPEN_TEMP_DB | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
              SQLITE_OPEN_EXCLUSIVE | SQLITE_OPEN_DELETEONCLOSE;
    zOpen = 0;
  }

  if( rc==SQLITE_OK ){
    rc = pRealVfs->xOpen(pRealVfs, zOpen, pFd->pReal, oflags, pOutFlags);
  }
  if( pFd->pReal->pMethods ){
    /* The xOpen() operation has succeeded. Set the sqlite3_file.pMethods
    ** pointer and, if the file is a main database file, link it into the
    ** mutex protected linked list of all such files.  */
    pFile->pMethods = &rbuvfs_io_methods;
    if( flags & SQLITE_OPEN_MAIN_DB ){
      rbuMainlistAdd(pFd);
    }
  }else{
    sqlite3_free(pFd->zDel);
  }

  return rc;
}

/*
** Delete the file located at zPath.
*/
static int rbuVfsDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync){
  sqlite3_vfs *pRealVfs = ((rbu_vfs*)pVfs)->pRealVfs;
  return pRealVfs->xDelete(pRealVfs, zPath, dirSync);
}

/*
** Test for access permissions. Return true if the requested permission
** is available, or false otherwise.
*/
static int rbuVfsAccess(
  sqlite3_vfs *pVfs, 
  const char *zPath, 
  int flags, 
  int *pResOut
){
  rbu_vfs *pRbuVfs = (rbu_vfs*)pVfs;
  sqlite3_vfs *pRealVfs = pRbuVfs->pRealVfs;
  int rc;

  rc = pRealVfs->xAccess(pRealVfs, zPath, flags, pResOut);

  /* If this call is to check if a *-wal file associated with an RBU target
  ** database connection exists, and the RBU update is in RBU_STAGE_OAL,
  ** the following special handling is activated:
  **
  **   a) if the *-wal file does exist, return SQLITE_CANTOPEN. This
  **      ensures that the RBU extension never tries to update a database
  **      in wal mode, even if the first page of the database file has
  **      been damaged. 
  **
  **   b) if the *-wal file does not exist, claim that it does anyway,
  **      causing SQLite to call xOpen() to open it. This call will also
  **      be intercepted (see the rbuVfsOpen() function) and the *-oal
  **      file opened instead.
  */
  if( rc==SQLITE_OK && flags==SQLITE_ACCESS_EXISTS ){
    rbu_file *pDb = rbuFindMaindb(pRbuVfs, zPath, 1);
    if( pDb && pDb->pRbu->eStage==RBU_STAGE_OAL ){
      assert( pDb->pRbu );
      if( *pResOut ){
        rc = SQLITE_CANTOPEN;
      }else{
        sqlite3_int64 sz = 0;
        rc = rbuVfsFileSize(&pDb->base, &sz);
        *pResOut = (sz>0);
      }
    }
  }

  return rc;
}

/*
** Populate buffer zOut with the full canonical pathname corresponding
** to the pathname in zPath. zOut is guaranteed to point to a buffer
** of at least (DEVSYM_MAX_PATHNAME+1) bytes.
*/
static int rbuVfsFullPathname(
  sqlite3_vfs *pVfs, 
  const char *zPath, 
  int nOut, 
  char *zOut
){
  sqlite3_vfs *pRealVfs = ((rbu_vfs*)pVfs)->pRealVfs;
  return pRealVfs->xFullPathname(pRealVfs, zPath, nOut, zOut);
}

#ifndef SQLITE_OMIT_LOAD_EXTENSION
/*
** Open the dynamic library located at zPath and return a handle.
*/
static void *rbuVfsDlOpen(sqlite3_vfs *pVfs, const char *zPath){
  sqlite3_vfs *pRealVfs = ((rbu_vfs*)pVfs)->pRealVfs;
  return pRealVfs->xDlOpen(pRealVfs, zPath);
}

/*
** Populate the buffer zErrMsg (size nByte bytes) with a human readable
** utf-8 string describing the most recent error encountered associated 
** with dynamic libraries.
*/
static void rbuVfsDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg){
  sqlite3_vfs *pRealVfs = ((rbu_vfs*)pVfs)->pRealVfs;
  pRealVfs->xDlError(pRealVfs, nByte, zErrMsg);
}

/*
** Return a pointer to the symbol zSymbol in the dynamic library pHandle.
*/
static void (*rbuVfsDlSym(
  sqlite3_vfs *pVfs, 
  void *pArg, 
  const char *zSym
))(void){
  sqlite3_vfs *pRealVfs = ((rbu_vfs*)pVfs)->pRealVfs;
  return pRealVfs->xDlSym(pRealVfs, pArg, zSym);
}

/*
** Close the dynamic library handle pHandle.
*/
static void rbuVfsDlClose(sqlite3_vfs *pVfs, void *pHandle){
  sqlite3_vfs *pRealVfs = ((rbu_vfs*)pVfs)->pRealVfs;
  pRealVfs->xDlClose(pRealVfs, pHandle);
}
#endif /* SQLITE_OMIT_LOAD_EXTENSION */

/*
** Populate the buffer pointed to by zBufOut with nByte bytes of 
** random data.
*/
static int rbuVfsRandomness(sqlite3_vfs *pVfs, int nByte, char *zBufOut){
  sqlite3_vfs *pRealVfs = ((rbu_vfs*)pVfs)->pRealVfs;
  return pRealVfs->xRandomness(pRealVfs, nByte, zBufOut);
}

/*
** Sleep for nMicro microseconds. Return the number of microseconds 
** actually slept.
*/
static int rbuVfsSleep(sqlite3_vfs *pVfs, int nMicro){
  sqlite3_vfs *pRealVfs = ((rbu_vfs*)pVfs)->pRealVfs;
  return pRealVfs->xSleep(pRealVfs, nMicro);
}

/*
** Return the current time as a Julian Day number in *pTimeOut.
*/
static int rbuVfsCurrentTime(sqlite3_vfs *pVfs, double *pTimeOut){
  sqlite3_vfs *pRealVfs = ((rbu_vfs*)pVfs)->pRealVfs;
  return pRealVfs->xCurrentTime(pRealVfs, pTimeOut);
}

/*
** No-op.
*/
static int rbuVfsGetLastError(sqlite3_vfs *pVfs, int a, char *b){
  return 0;
}

/*
** Deregister and destroy an RBU vfs created by an earlier call to
** sqlite3rbu_create_vfs().
*/
SQLITE_API void sqlite3rbu_destroy_vfs(const char *zName){
  sqlite3_vfs *pVfs = sqlite3_vfs_find(zName);
  if( pVfs && pVfs->xOpen==rbuVfsOpen ){
    sqlite3_mutex_free(((rbu_vfs*)pVfs)->mutex);
    sqlite3_vfs_unregister(pVfs);
    sqlite3_free(pVfs);
  }
}

/*
** Create an RBU VFS named zName that accesses the underlying file-system
** via existing VFS zParent. The new object is registered as a non-default
** VFS with SQLite before returning.
*/
SQLITE_API int sqlite3rbu_create_vfs(const char *zName, const char *zParent){

  /* Template for VFS */
  static sqlite3_vfs vfs_template = {
    1,                            /* iVersion */
    0,                            /* szOsFile */
    0,                            /* mxPathname */
    0,                            /* pNext */
    0,                            /* zName */
    0,                            /* pAppData */
    rbuVfsOpen,                   /* xOpen */
    rbuVfsDelete,                 /* xDelete */
    rbuVfsAccess,                 /* xAccess */
    rbuVfsFullPathname,           /* xFullPathname */

#ifndef SQLITE_OMIT_LOAD_EXTENSION
    rbuVfsDlOpen,                 /* xDlOpen */
    rbuVfsDlError,                /* xDlError */
    rbuVfsDlSym,                  /* xDlSym */
    rbuVfsDlClose,                /* xDlClose */
#else
    0, 0, 0, 0,
#endif

    rbuVfsRandomness,             /* xRandomness */
    rbuVfsSleep,                  /* xSleep */
    rbuVfsCurrentTime,            /* xCurrentTime */
    rbuVfsGetLastError,           /* xGetLastError */
    0,                            /* xCurrentTimeInt64 (version 2) */
    0, 0, 0                       /* Unimplemented version 3 methods */
  };

  rbu_vfs *pNew = 0;              /* Newly allocated VFS */
  int rc = SQLITE_OK;
  size_t nName;
  size_t nByte;

  nName = strlen(zName);
  nByte = sizeof(rbu_vfs) + nName + 1;
  pNew = (rbu_vfs*)sqlite3_malloc64(nByte);
  if( pNew==0 ){
    rc = SQLITE_NOMEM;
  }else{
    sqlite3_vfs *pParent;           /* Parent VFS */
    memset(pNew, 0, nByte);
    pParent = sqlite3_vfs_find(zParent);
    if( pParent==0 ){
      rc = SQLITE_NOTFOUND;
    }else{
      char *zSpace;
      memcpy(&pNew->base, &vfs_template, sizeof(sqlite3_vfs));
      pNew->base.mxPathname = pParent->mxPathname;
      pNew->base.szOsFile = sizeof(rbu_file) + pParent->szOsFile;
      pNew->pRealVfs = pParent;
      pNew->base.zName = (const char*)(zSpace = (char*)&pNew[1]);
      memcpy(zSpace, zName, nName);

      /* Allocate the mutex and register the new VFS (not as the default) */
      pNew->mutex = sqlite3_mutex_alloc(SQLITE_MUTEX_RECURSIVE);
      if( pNew->mutex==0 ){
        rc = SQLITE_NOMEM;
      }else{
        rc = sqlite3_vfs_register(&pNew->base, 0);
      }
    }

    if( rc!=SQLITE_OK ){
      sqlite3_mutex_free(pNew->mutex);
      sqlite3_free(pNew);
    }
  }

  return rc;
}

/*
** Configure the aggregate temp file size limit for this RBU handle.
*/
SQLITE_API sqlite3_int64 sqlite3rbu_temp_size_limit(sqlite3rbu *pRbu, sqlite3_int64 n){
  if( n>=0 ){
    pRbu->szTempLimit = n;
  }
  return pRbu->szTempLimit;
}

SQLITE_API sqlite3_int64 sqlite3rbu_temp_size(sqlite3rbu *pRbu){
  return pRbu->szTemp;
}


/**************************************************************************/

#endif /* !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_RBU) */

/************** End of sqlite3rbu.c ******************************************/
/************** Begin file dbstat.c ******************************************/
/*
** 2010 July 12
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This file contains an implementation of the "dbstat" virtual table.
**
** The dbstat virtual table is used to extract low-level formatting
** information from an SQLite database in order to implement the
** "sqlite3_analyzer" utility.  See the ../tool/spaceanal.tcl script
** for an example implementation.
**
** Additional information is available on the "dbstat.html" page of the
** official SQLite documentation.
*/

/* #include "sqliteInt.h"   ** Requires access to internal data structures ** */
#if (defined(SQLITE_ENABLE_DBSTAT_VTAB) || defined(SQLITE_TEST)) \
    && !defined(SQLITE_OMIT_VIRTUALTABLE)

/*
** Page paths:
** 
**   The value of the 'path' column describes the path taken from the 
**   root-node of the b-tree structure to each page. The value of the 
**   root-node path is '/'.
**
**   The value of the path for the left-most child page of the root of
**   a b-tree is '/000/'. (Btrees store content ordered from left to right
**   so the pages to the left have smaller keys than the pages to the right.)
**   The next to left-most child of the root page is
**   '/001', and so on, each sibling page identified by a 3-digit hex 
**   value. The children of the 451st left-most sibling have paths such
**   as '/1c2/000/, '/1c2/001/' etc.
**
**   Overflow pages are specified by appending a '+' character and a 
**   six-digit hexadecimal value to the path to the cell they are linked
**   from. For example, the three overflow pages in a chain linked from 
**   the left-most cell of the 450th child of the root page are identified
**   by the paths:
**
**      '/1c2/000+000000'         // First page in overflow chain
**      '/1c2/000+000001'         // Second page in overflow chain
**      '/1c2/000+000002'         // Third page in overflow chain
**
**   If the paths are sorted using the BINARY collation sequence, then
**   the overflow pages associated with a cell will appear earlier in the
**   sort-order than its child page:
**
**      '/1c2/000/'               // Left-most child of 451st child of root
*/
#define VTAB_SCHEMA                                                         \
  "CREATE TABLE xx( "                                                       \
  "  name       TEXT,             /* Name of table or index */"             \
  "  path       TEXT,             /* Path to page from root */"             \
  "  pageno     INTEGER,          /* Page number */"                        \
  "  pagetype   TEXT,             /* 'internal', 'leaf' or 'overflow' */"   \
  "  ncell      INTEGER,          /* Cells on page (0 for overflow) */"     \
  "  payload    INTEGER,          /* Bytes of payload on this page */"      \
  "  unused     INTEGER,          /* Bytes of unused space on this page */" \
  "  mx_payload INTEGER,          /* Largest payload size of all cells */"  \
  "  pgoffset   INTEGER,          /* Offset of page in file */"             \
  "  pgsize     INTEGER,          /* Size of the page */"                   \
  "  schema     TEXT HIDDEN       /* Database schema being analyzed */"     \
  ");"


typedef struct StatTable StatTable;
typedef struct StatCursor StatCursor;
typedef struct StatPage StatPage;
typedef struct StatCell StatCell;

struct StatCell {
  int nLocal;                     /* Bytes of local payload */
  u32 iChildPg;                   /* Child node (or 0 if this is a leaf) */
  int nOvfl;                      /* Entries in aOvfl[] */
  u32 *aOvfl;                     /* Array of overflow page numbers */
  int nLastOvfl;                  /* Bytes of payload on final overflow page */
  int iOvfl;                      /* Iterates through aOvfl[] */
};

struct StatPage {
  u32 iPgno;
  DbPage *pPg;
  int iCell;

  char *zPath;                    /* Path to this page */

  /* Variables populated by statDecodePage(): */
  u8 flags;                       /* Copy of flags byte */
  int nCell;                      /* Number of cells on page */
  int nUnused;                    /* Number of unused bytes on page */
  StatCell *aCell;                /* Array of parsed cells */
  u32 iRightChildPg;              /* Right-child page number (or 0) */
  int nMxPayload;                 /* Largest payload of any cell on this page */
};

struct StatCursor {
  sqlite3_vtab_cursor base;
  sqlite3_stmt *pStmt;            /* Iterates through set of root pages */
  int isEof;                      /* After pStmt has returned SQLITE_DONE */
  int iDb;                        /* Schema used for this query */

  StatPage aPage[32];
  int iPage;                      /* Current entry in aPage[] */

  /* Values to return. */
  char *zName;                    /* Value of 'name' column */
  char *zPath;                    /* Value of 'path' column */
  u32 iPageno;                    /* Value of 'pageno' column */
  char *zPagetype;                /* Value of 'pagetype' column */
  int nCell;                      /* Value of 'ncell' column */
  int nPayload;                   /* Value of 'payload' column */
  int nUnused;                    /* Value of 'unused' column */
  int nMxPayload;                 /* Value of 'mx_payload' column */
  i64 iOffset;                    /* Value of 'pgOffset' column */
  int szPage;                     /* Value of 'pgSize' column */
};

struct StatTable {
  sqlite3_vtab base;
  sqlite3 *db;
  int iDb;                        /* Index of database to analyze */
};

#ifndef get2byte
# define get2byte(x)   ((x)[0]<<8 | (x)[1])
#endif

/*
** Connect to or create a statvfs virtual table.
*/
static int statConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  StatTable *pTab = 0;
  int rc = SQLITE_OK;
  int iDb;

  if( argc>=4 ){
    Token nm;
    sqlite3TokenInit(&nm, (char*)argv[3]);
    iDb = sqlite3FindDb(db, &nm);
    if( iDb<0 ){
      *pzErr = sqlite3_mprintf("no such database: %s", argv[3]);
      return SQLITE_ERROR;
    }
  }else{
    iDb = 0;
  }
  rc = sqlite3_declare_vtab(db, VTAB_SCHEMA);
  if( rc==SQLITE_OK ){
    pTab = (StatTable *)sqlite3_malloc64(sizeof(StatTable));
    if( pTab==0 ) rc = SQLITE_NOMEM_BKPT;
  }

  assert( rc==SQLITE_OK || pTab==0 );
  if( rc==SQLITE_OK ){
    memset(pTab, 0, sizeof(StatTable));
    pTab->db = db;
    pTab->iDb = iDb;
  }

  *ppVtab = (sqlite3_vtab*)pTab;
  return rc;
}

/*
** Disconnect from or destroy a statvfs virtual table.
*/
static int statDisconnect(sqlite3_vtab *pVtab){
  sqlite3_free(pVtab);
  return SQLITE_OK;
}

/*
** There is no "best-index". This virtual table always does a linear
** scan.  However, a schema=? constraint should cause this table to
** operate on a different database schema, so check for it.
**
** idxNum is normally 0, but will be 1 if a schema=? constraint exists.
*/
static int statBestIndex(sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo){
  int i;

  /* Look for a valid schema=? constraint.  If found, change the idxNum to
  ** 1 and request the value of that constraint be sent to xFilter.  And
  ** lower the cost estimate to encourage the constrained version to be
  ** used.
  */
  for(i=0; i<pIdxInfo->nConstraint; i++){
    if( pIdxInfo->aConstraint[i].iColumn!=10 ) continue;
    if( pIdxInfo->aConstraint[i].usable==0 ) return SQLITE_CONSTRAINT;
    if( pIdxInfo->aConstraint[i].op!=SQLITE_INDEX_CONSTRAINT_EQ ) continue;
    pIdxInfo->idxNum = 1;
    pIdxInfo->estimatedCost = 1.0;
    pIdxInfo->aConstraintUsage[i].argvIndex = 1;
    pIdxInfo->aConstraintUsage[i].omit = 1;
    break;
  }


  /* Records are always returned in ascending order of (name, path). 
  ** If this will satisfy the client, set the orderByConsumed flag so that 
  ** SQLite does not do an external sort.
  */
  if( ( pIdxInfo->nOrderBy==1
     && pIdxInfo->aOrderBy[0].iColumn==0
     && pIdxInfo->aOrderBy[0].desc==0
     ) ||
      ( pIdxInfo->nOrderBy==2
     && pIdxInfo->aOrderBy[0].iColumn==0
     && pIdxInfo->aOrderBy[0].desc==0
     && pIdxInfo->aOrderBy[1].iColumn==1
     && pIdxInfo->aOrderBy[1].desc==0
     )
  ){
    pIdxInfo->orderByConsumed = 1;
  }

  return SQLITE_OK;
}

/*
** Open a new statvfs cursor.
*/
static int statOpen(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor){
  StatTable *pTab = (StatTable *)pVTab;
  StatCursor *pCsr;

  pCsr = (StatCursor *)sqlite3_malloc64(sizeof(StatCursor));
  if( pCsr==0 ){
    return SQLITE_NOMEM_BKPT;
  }else{
    memset(pCsr, 0, sizeof(StatCursor));
    pCsr->base.pVtab = pVTab;
    pCsr->iDb = pTab->iDb;
  }

  *ppCursor = (sqlite3_vtab_cursor *)pCsr;
  return SQLITE_OK;
}

static void statClearCells(StatPage *p){
  int i;
  if( p->aCell ){
    for(i=0; i<p->nCell; i++){
      sqlite3_free(p->aCell[i].aOvfl);
    }
    sqlite3_free(p->aCell);
  }
  p->nCell = 0;
  p->aCell = 0;
}

static void statClearPage(StatPage *p){
  statClearCells(p);
  sqlite3PagerUnref(p->pPg);
  sqlite3_free(p->zPath);
  memset(p, 0, sizeof(StatPage));
}

static void statResetCsr(StatCursor *pCsr){
  int i;
  sqlite3_reset(pCsr->pStmt);
  for(i=0; i<ArraySize(pCsr->aPage); i++){
    statClearPage(&pCsr->aPage[i]);
  }
  pCsr->iPage = 0;
  sqlite3_free(pCsr->zPath);
  pCsr->zPath = 0;
  pCsr->isEof = 0;
}

/*
** Close a statvfs cursor.
*/
static int statClose(sqlite3_vtab_cursor *pCursor){
  StatCursor *pCsr = (StatCursor *)pCursor;
  statResetCsr(pCsr);
  sqlite3_finalize(pCsr->pStmt);
  sqlite3_free(pCsr);
  return SQLITE_OK;
}

static void getLocalPayload(
  int nUsable,                    /* Usable bytes per page */
  u8 flags,                       /* Page flags */
  int nTotal,                     /* Total record (payload) size */
  int *pnLocal                    /* OUT: Bytes stored locally */
){
  int nLocal;
  int nMinLocal;
  int nMaxLocal;
 
  if( flags==0x0D ){              /* Table leaf node */
    nMinLocal = (nUsable - 12) * 32 / 255 - 23;
    nMaxLocal = nUsable - 35;
  }else{                          /* Index interior and leaf nodes */
    nMinLocal = (nUsable - 12) * 32 / 255 - 23;
    nMaxLocal = (nUsable - 12) * 64 / 255 - 23;
  }

  nLocal = nMinLocal + (nTotal - nMinLocal) % (nUsable - 4);
  if( nLocal>nMaxLocal ) nLocal = nMinLocal;
  *pnLocal = nLocal;
}

static int statDecodePage(Btree *pBt, StatPage *p){
  int nUnused;
  int iOff;
  int nHdr;
  int isLeaf;
  int szPage;

  u8 *aData = sqlite3PagerGetData(p->pPg);
  u8 *aHdr = &aData[p->iPgno==1 ? 100 : 0];

  p->flags = aHdr[0];
  if( p->flags==0x0A || p->flags==0x0D ){
    isLeaf = 1;
    nHdr = 8;
  }else if( p->flags==0x05 || p->flags==0x02 ){
    isLeaf = 0;
    nHdr = 12;
  }else{
    goto statPageIsCorrupt;
  }
  if( p->iPgno==1 ) nHdr += 100;
  p->nCell = get2byte(&aHdr[3]);
  p->nMxPayload = 0;
  szPage = sqlite3BtreeGetPageSize(pBt);

  nUnused = get2byte(&aHdr[5]) - nHdr - 2*p->nCell;
  nUnused += (int)aHdr[7];
  iOff = get2byte(&aHdr[1]);
  while( iOff ){
    int iNext;
    if( iOff>=szPage ) goto statPageIsCorrupt;
    nUnused += get2byte(&aData[iOff+2]);
    iNext = get2byte(&aData[iOff]);
    if( iNext<iOff+4 && iNext>0 ) goto statPageIsCorrupt;
    iOff = iNext;
  }
  p->nUnused = nUnused;
  p->iRightChildPg = isLeaf ? 0 : sqlite3Get4byte(&aHdr[8]);

  if( p->nCell ){
    int i;                        /* Used to iterate through cells */
    int nUsable;                  /* Usable bytes per page */

    sqlite3BtreeEnter(pBt);
    nUsable = szPage - sqlite3BtreeGetReserveNoMutex(pBt);
    sqlite3BtreeLeave(pBt);
    p->aCell = sqlite3_malloc64((p->nCell+1) * sizeof(StatCell));
    if( p->aCell==0 ) return SQLITE_NOMEM_BKPT;
    memset(p->aCell, 0, (p->nCell+1) * sizeof(StatCell));

    for(i=0; i<p->nCell; i++){
      StatCell *pCell = &p->aCell[i];

      iOff = get2byte(&aData[nHdr+i*2]);
      if( iOff<nHdr || iOff>=szPage ) goto statPageIsCorrupt;
      if( !isLeaf ){
        pCell->iChildPg = sqlite3Get4byte(&aData[iOff]);
        iOff += 4;
      }
      if( p->flags==0x05 ){
        /* A table interior node. nPayload==0. */
      }else{
        u32 nPayload;             /* Bytes of payload total (local+overflow) */
        int nLocal;               /* Bytes of payload stored locally */
        iOff += getVarint32(&aData[iOff], nPayload);
        if( p->flags==0x0D ){
          u64 dummy;
          iOff += sqlite3GetVarint(&aData[iOff], &dummy);
        }
        if( nPayload>(u32)p->nMxPayload ) p->nMxPayload = nPayload;
        getLocalPayload(nUsable, p->flags, nPayload, &nLocal);
        if( nLocal<0 ) goto statPageIsCorrupt;
        pCell->nLocal = nLocal;
        assert( nPayload>=(u32)nLocal );
        assert( nLocal<=(nUsable-35) );
        if( nPayload>(u32)nLocal ){
          int j;
          int nOvfl = ((nPayload - nLocal) + nUsable-4 - 1) / (nUsable - 4);
          if( iOff+nLocal>nUsable ) goto statPageIsCorrupt;
          pCell->nLastOvfl = (nPayload-nLocal) - (nOvfl-1) * (nUsable-4);
          pCell->nOvfl = nOvfl;
          pCell->aOvfl = sqlite3_malloc64(sizeof(u32)*nOvfl);
          if( pCell->aOvfl==0 ) return SQLITE_NOMEM_BKPT;
          pCell->aOvfl[0] = sqlite3Get4byte(&aData[iOff+nLocal]);
          for(j=1; j<nOvfl; j++){
            int rc;
            u32 iPrev = pCell->aOvfl[j-1];
            DbPage *pPg = 0;
            rc = sqlite3PagerGet(sqlite3BtreePager(pBt), iPrev, &pPg, 0);
            if( rc!=SQLITE_OK ){
              assert( pPg==0 );
              return rc;
            } 
            pCell->aOvfl[j] = sqlite3Get4byte(sqlite3PagerGetData(pPg));
            sqlite3PagerUnref(pPg);
          }
        }
      }
    }
  }

  return SQLITE_OK;

statPageIsCorrupt:
  p->flags = 0;
  statClearCells(p);
  return SQLITE_OK;
}

/*
** Populate the pCsr->iOffset and pCsr->szPage member variables. Based on
** the current value of pCsr->iPageno.
*/
static void statSizeAndOffset(StatCursor *pCsr){
  StatTable *pTab = (StatTable *)((sqlite3_vtab_cursor *)pCsr)->pVtab;
  Btree *pBt = pTab->db->aDb[pTab->iDb].pBt;
  Pager *pPager = sqlite3BtreePager(pBt);
  sqlite3_file *fd;
  sqlite3_int64 x[2];

  /* The default page size and offset */
  pCsr->szPage = sqlite3BtreeGetPageSize(pBt);
  pCsr->iOffset = (i64)pCsr->szPage * (pCsr->iPageno - 1);

  /* If connected to a ZIPVFS backend, override the page size and
  ** offset with actual values obtained from ZIPVFS.
  */
  fd = sqlite3PagerFile(pPager);
  x[0] = pCsr->iPageno;
  if( sqlite3OsFileControl(fd, 230440, &x)==SQLITE_OK ){
    pCsr->iOffset = x[0];
    pCsr->szPage = (int)x[1];
  }
}

/*
** Move a statvfs cursor to the next entry in the file.
*/
static int statNext(sqlite3_vtab_cursor *pCursor){
  int rc;
  int nPayload;
  char *z;
  StatCursor *pCsr = (StatCursor *)pCursor;
  StatTable *pTab = (StatTable *)pCursor->pVtab;
  Btree *pBt = pTab->db->aDb[pCsr->iDb].pBt;
  Pager *pPager = sqlite3BtreePager(pBt);

  sqlite3_free(pCsr->zPath);
  pCsr->zPath = 0;

statNextRestart:
  if( pCsr->aPage[0].pPg==0 ){
    rc = sqlite3_step(pCsr->pStmt);
    if( rc==SQLITE_ROW ){
      int nPage;
      u32 iRoot = (u32)sqlite3_column_int64(pCsr->pStmt, 1);
      sqlite3PagerPagecount(pPager, &nPage);
      if( nPage==0 ){
        pCsr->isEof = 1;
        return sqlite3_reset(pCsr->pStmt);
      }
      rc = sqlite3PagerGet(pPager, iRoot, &pCsr->aPage[0].pPg, 0);
      pCsr->aPage[0].iPgno = iRoot;
      pCsr->aPage[0].iCell = 0;
      pCsr->aPage[0].zPath = z = sqlite3_mprintf("/");
      pCsr->iPage = 0;
      if( z==0 ) rc = SQLITE_NOMEM_BKPT;
    }else{
      pCsr->isEof = 1;
      return sqlite3_reset(pCsr->pStmt);
    }
  }else{

    /* Page p itself has already been visited. */
    StatPage *p = &pCsr->aPage[pCsr->iPage];

    while( p->iCell<p->nCell ){
      StatCell *pCell = &p->aCell[p->iCell];
      if( pCell->iOvfl<pCell->nOvfl ){
        int nUsable;
        sqlite3BtreeEnter(pBt);
        nUsable = sqlite3BtreeGetPageSize(pBt) - 
                        sqlite3BtreeGetReserveNoMutex(pBt);
        sqlite3BtreeLeave(pBt);
        pCsr->zName = (char *)sqlite3_column_text(pCsr->pStmt, 0);
        pCsr->iPageno = pCell->aOvfl[pCell->iOvfl];
        pCsr->zPagetype = "overflow";
        pCsr->nCell = 0;
        pCsr->nMxPayload = 0;
        pCsr->zPath = z = sqlite3_mprintf(
            "%s%.3x+%.6x", p->zPath, p->iCell, pCell->iOvfl
        );
        if( pCell->iOvfl<pCell->nOvfl-1 ){
          pCsr->nUnused = 0;
          pCsr->nPayload = nUsable - 4;
        }else{
          pCsr->nPayload = pCell->nLastOvfl;
          pCsr->nUnused = nUsable - 4 - pCsr->nPayload;
        }
        pCell->iOvfl++;
        statSizeAndOffset(pCsr);
        return z==0 ? SQLITE_NOMEM_BKPT : SQLITE_OK;
      }
      if( p->iRightChildPg ) break;
      p->iCell++;
    }

    if( !p->iRightChildPg || p->iCell>p->nCell ){
      statClearPage(p);
      if( pCsr->iPage==0 ) return statNext(pCursor);
      pCsr->iPage--;
      goto statNextRestart; /* Tail recursion */
    }
    pCsr->iPage++;
    if( pCsr->iPage>=ArraySize(pCsr->aPage) ){
      statResetCsr(pCsr);
      return SQLITE_CORRUPT_BKPT;
    }
    assert( p==&pCsr->aPage[pCsr->iPage-1] );

    if( p->iCell==p->nCell ){
      p[1].iPgno = p->iRightChildPg;
    }else{
      p[1].iPgno = p->aCell[p->iCell].iChildPg;
    }
    rc = sqlite3PagerGet(pPager, p[1].iPgno, &p[1].pPg, 0);
    p[1].iCell = 0;
    p[1].zPath = z = sqlite3_mprintf("%s%.3x/", p->zPath, p->iCell);
    p->iCell++;
    if( z==0 ) rc = SQLITE_NOMEM_BKPT;
  }


  /* Populate the StatCursor fields with the values to be returned
  ** by the xColumn() and xRowid() methods.
  */
  if( rc==SQLITE_OK ){
    int i;
    StatPage *p = &pCsr->aPage[pCsr->iPage];
    pCsr->zName = (char *)sqlite3_column_text(pCsr->pStmt, 0);
    pCsr->iPageno = p->iPgno;

    rc = statDecodePage(pBt, p);
    if( rc==SQLITE_OK ){
      statSizeAndOffset(pCsr);

      switch( p->flags ){
        case 0x05:             /* table internal */
        case 0x02:             /* index internal */
          pCsr->zPagetype = "internal";
          break;
        case 0x0D:             /* table leaf */
        case 0x0A:             /* index leaf */
          pCsr->zPagetype = "leaf";
          break;
        default:
          pCsr->zPagetype = "corrupted";
          break;
      }
      pCsr->nCell = p->nCell;
      pCsr->nUnused = p->nUnused;
      pCsr->nMxPayload = p->nMxPayload;
      pCsr->zPath = z = sqlite3_mprintf("%s", p->zPath);
      if( z==0 ) rc = SQLITE_NOMEM_BKPT;
      nPayload = 0;
      for(i=0; i<p->nCell; i++){
        nPayload += p->aCell[i].nLocal;
      }
      pCsr->nPayload = nPayload;
    }
  }

  return rc;
}

static int statEof(sqlite3_vtab_cursor *pCursor){
  StatCursor *pCsr = (StatCursor *)pCursor;
  return pCsr->isEof;
}

static int statFilter(
  sqlite3_vtab_cursor *pCursor, 
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  StatCursor *pCsr = (StatCursor *)pCursor;
  StatTable *pTab = (StatTable*)(pCursor->pVtab);
  char *zSql;
  int rc = SQLITE_OK;

  if( idxNum==1 ){
    const char *zDbase = (const char*)sqlite3_value_text(argv[0]);
    pCsr->iDb = sqlite3FindDbName(pTab->db, zDbase);
    if( pCsr->iDb<0 ){
      sqlite3_free(pCursor->pVtab->zErrMsg);
      pCursor->pVtab->zErrMsg = sqlite3_mprintf("no such schema: %s", zDbase);
      return pCursor->pVtab->zErrMsg ? SQLITE_ERROR : SQLITE_NOMEM_BKPT;
    }
  }else{
    pCsr->iDb = pTab->iDb;
  }
  statResetCsr(pCsr);
  sqlite3_finalize(pCsr->pStmt);
  pCsr->pStmt = 0;
  zSql = sqlite3_mprintf(
      "SELECT 'sqlite_master' AS name, 1 AS rootpage, 'table' AS type"
      "  UNION ALL  "
      "SELECT name, rootpage, type"
      "  FROM \"%w\".sqlite_master WHERE rootpage!=0"
      "  ORDER BY name", pTab->db->aDb[pCsr->iDb].zDbSName);
  if( zSql==0 ){
    return SQLITE_NOMEM_BKPT;
  }else{
    rc = sqlite3_prepare_v2(pTab->db, zSql, -1, &pCsr->pStmt, 0);
    sqlite3_free(zSql);
  }

  if( rc==SQLITE_OK ){
    rc = statNext(pCursor);
  }
  return rc;
}

static int statColumn(
  sqlite3_vtab_cursor *pCursor, 
  sqlite3_context *ctx, 
  int i
){
  StatCursor *pCsr = (StatCursor *)pCursor;
  switch( i ){
    case 0:            /* name */
      sqlite3_result_text(ctx, pCsr->zName, -1, SQLITE_TRANSIENT);
      break;
    case 1:            /* path */
      sqlite3_result_text(ctx, pCsr->zPath, -1, SQLITE_TRANSIENT);
      break;
    case 2:            /* pageno */
      sqlite3_result_int64(ctx, pCsr->iPageno);
      break;
    case 3:            /* pagetype */
      sqlite3_result_text(ctx, pCsr->zPagetype, -1, SQLITE_STATIC);
      break;
    case 4:            /* ncell */
      sqlite3_result_int(ctx, pCsr->nCell);
      break;
    case 5:            /* payload */
      sqlite3_result_int(ctx, pCsr->nPayload);
      break;
    case 6:            /* unused */
      sqlite3_result_int(ctx, pCsr->nUnused);
      break;
    case 7:            /* mx_payload */
      sqlite3_result_int(ctx, pCsr->nMxPayload);
      break;
    case 8:            /* pgoffset */
      sqlite3_result_int64(ctx, pCsr->iOffset);
      break;
    case 9:            /* pgsize */
      sqlite3_result_int(ctx, pCsr->szPage);
      break;
    default: {          /* schema */
      sqlite3 *db = sqlite3_context_db_handle(ctx);
      int iDb = pCsr->iDb;
      sqlite3_result_text(ctx, db->aDb[iDb].zDbSName, -1, SQLITE_STATIC);
      break;
    }
  }
  return SQLITE_OK;
}

static int statRowid(sqlite3_vtab_cursor *pCursor, sqlite_int64 *pRowid){
  StatCursor *pCsr = (StatCursor *)pCursor;
  *pRowid = pCsr->iPageno;
  return SQLITE_OK;
}

/*
** Invoke this routine to register the "dbstat" virtual table module
*/
SQLITE_PRIVATE int sqlite3DbstatRegister(sqlite3 *db){
  static sqlite3_module dbstat_module = {
    0,                            /* iVersion */
    statConnect,                  /* xCreate */
    statConnect,                  /* xConnect */
    statBestIndex,                /* xBestIndex */
    statDisconnect,               /* xDisconnect */
    statDisconnect,               /* xDestroy */
    statOpen,                     /* xOpen - open a cursor */
    statClose,                    /* xClose - close a cursor */
    statFilter,                   /* xFilter - configure scan constraints */
    statNext,                     /* xNext - advance a cursor */
    statEof,                      /* xEof - check for end of scan */
    statColumn,                   /* xColumn - read data */
    statRowid,                    /* xRowid - read data */
    0,                            /* xUpdate */
    0,                            /* xBegin */
    0,                            /* xSync */
    0,                            /* xCommit */
    0,                            /* xRollback */
    0,                            /* xFindMethod */
    0,                            /* xRename */
    0,                            /* xSavepoint */
    0,                            /* xRelease */
    0,                            /* xRollbackTo */
    0                             /* xShadowName */
  };
  return sqlite3_create_module(db, "dbstat", &dbstat_module, 0);
}
#elif defined(SQLITE_ENABLE_DBSTAT_VTAB)
SQLITE_PRIVATE int sqlite3DbstatRegister(sqlite3 *db){ return SQLITE_OK; }
#endif /* SQLITE_ENABLE_DBSTAT_VTAB */

/************** End of dbstat.c **********************************************/
/************** Begin file dbpage.c ******************************************/
/*
** 2017-10-11
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This file contains an implementation of the "sqlite_dbpage" virtual table.
**
** The sqlite_dbpage virtual table is used to read or write whole raw
** pages of the database file.  The pager interface is used so that 
** uncommitted changes and changes recorded in the WAL file are correctly
** retrieved.
**
** Usage example:
**
**    SELECT data FROM sqlite_dbpage('aux1') WHERE pgno=123;
**
** This is an eponymous virtual table so it does not need to be created before
** use.  The optional argument to the sqlite_dbpage() table name is the
** schema for the database file that is to be read.  The default schema is
** "main".
**
** The data field of sqlite_dbpage table can be updated.  The new
** value must be a BLOB which is the correct page size, otherwise the
** update fails.  Rows may not be deleted or inserted.
*/

/* #include "sqliteInt.h"   ** Requires access to internal data structures ** */
#if (defined(SQLITE_ENABLE_DBPAGE_VTAB) || defined(SQLITE_TEST)) \
    && !defined(SQLITE_OMIT_VIRTUALTABLE)

typedef struct DbpageTable DbpageTable;
typedef struct DbpageCursor DbpageCursor;

struct DbpageCursor {
  sqlite3_vtab_cursor base;       /* Base class.  Must be first */
  int pgno;                       /* Current page number */
  int mxPgno;                     /* Last page to visit on this scan */
  Pager *pPager;                  /* Pager being read/written */
  DbPage *pPage1;                 /* Page 1 of the database */
  int iDb;                        /* Index of database to analyze */
  int szPage;                     /* Size of each page in bytes */
};

struct DbpageTable {
  sqlite3_vtab base;              /* Base class.  Must be first */
  sqlite3 *db;                    /* The database */
};

/* Columns */
#define DBPAGE_COLUMN_PGNO    0
#define DBPAGE_COLUMN_DATA    1
#define DBPAGE_COLUMN_SCHEMA  2



/*
** Connect to or create a dbpagevfs virtual table.
*/
static int dbpageConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  DbpageTable *pTab = 0;
  int rc = SQLITE_OK;

  rc = sqlite3_declare_vtab(db, 
          "CREATE TABLE x(pgno INTEGER PRIMARY KEY, data BLOB, schema HIDDEN)");
  if( rc==SQLITE_OK ){
    pTab = (DbpageTable *)sqlite3_malloc64(sizeof(DbpageTable));
    if( pTab==0 ) rc = SQLITE_NOMEM_BKPT;
  }

  assert( rc==SQLITE_OK || pTab==0 );
  if( rc==SQLITE_OK ){
    memset(pTab, 0, sizeof(DbpageTable));
    pTab->db = db;
  }

  *ppVtab = (sqlite3_vtab*)pTab;
  return rc;
}

/*
** Disconnect from or destroy a dbpagevfs virtual table.
*/
static int dbpageDisconnect(sqlite3_vtab *pVtab){
  sqlite3_free(pVtab);
  return SQLITE_OK;
}

/*
** idxNum:
**
**     0     schema=main, full table scan
**     1     schema=main, pgno=?1
**     2     schema=?1, full table scan
**     3     schema=?1, pgno=?2
*/
static int dbpageBestIndex(sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo){
  int i;
  int iPlan = 0;

  /* If there is a schema= constraint, it must be honored.  Report a
  ** ridiculously large estimated cost if the schema= constraint is
  ** unavailable
  */
  for(i=0; i<pIdxInfo->nConstraint; i++){
    struct sqlite3_index_constraint *p = &pIdxInfo->aConstraint[i];
    if( p->iColumn!=DBPAGE_COLUMN_SCHEMA ) continue;
    if( p->op!=SQLITE_INDEX_CONSTRAINT_EQ ) continue;
    if( !p->usable ){
      /* No solution. */
      return SQLITE_CONSTRAINT;
    }
    iPlan = 2;
    pIdxInfo->aConstraintUsage[i].argvIndex = 1;
    pIdxInfo->aConstraintUsage[i].omit = 1;
    break;
  }

  /* If we reach this point, it means that either there is no schema=
  ** constraint (in which case we use the "main" schema) or else the
  ** schema constraint was accepted.  Lower the estimated cost accordingly
  */
  pIdxInfo->estimatedCost = 1.0e6;

  /* Check for constraints against pgno */
  for(i=0; i<pIdxInfo->nConstraint; i++){
    struct sqlite3_index_constraint *p = &pIdxInfo->aConstraint[i];
    if( p->usable && p->iColumn<=0 && p->op==SQLITE_INDEX_CONSTRAINT_EQ ){
      pIdxInfo->estimatedRows = 1;
      pIdxInfo->idxFlags = SQLITE_INDEX_SCAN_UNIQUE;
      pIdxInfo->estimatedCost = 1.0;
      pIdxInfo->aConstraintUsage[i].argvIndex = iPlan ? 2 : 1;
      pIdxInfo->aConstraintUsage[i].omit = 1;
      iPlan |= 1;
      break;
    }
  }
  pIdxInfo->idxNum = iPlan;

  if( pIdxInfo->nOrderBy>=1
   && pIdxInfo->aOrderBy[0].iColumn<=0
   && pIdxInfo->aOrderBy[0].desc==0
  ){
    pIdxInfo->orderByConsumed = 1;
  }
  return SQLITE_OK;
}

/*
** Open a new dbpagevfs cursor.
*/
static int dbpageOpen(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor){
  DbpageCursor *pCsr;

  pCsr = (DbpageCursor *)sqlite3_malloc64(sizeof(DbpageCursor));
  if( pCsr==0 ){
    return SQLITE_NOMEM_BKPT;
  }else{
    memset(pCsr, 0, sizeof(DbpageCursor));
    pCsr->base.pVtab = pVTab;
    pCsr->pgno = -1;
  }

  *ppCursor = (sqlite3_vtab_cursor *)pCsr;
  return SQLITE_OK;
}

/*
** Close a dbpagevfs cursor.
*/
static int dbpageClose(sqlite3_vtab_cursor *pCursor){
  DbpageCursor *pCsr = (DbpageCursor *)pCursor;
  if( pCsr->pPage1 ) sqlite3PagerUnrefPageOne(pCsr->pPage1);
  sqlite3_free(pCsr);
  return SQLITE_OK;
}

/*
** Move a dbpagevfs cursor to the next entry in the file.
*/
static int dbpageNext(sqlite3_vtab_cursor *pCursor){
  int rc = SQLITE_OK;
  DbpageCursor *pCsr = (DbpageCursor *)pCursor;
  pCsr->pgno++;
  return rc;
}

static int dbpageEof(sqlite3_vtab_cursor *pCursor){
  DbpageCursor *pCsr = (DbpageCursor *)pCursor;
  return pCsr->pgno > pCsr->mxPgno;
}

/*
** idxNum:
**
**     0     schema=main, full table scan
**     1     schema=main, pgno=?1
**     2     schema=?1, full table scan
**     3     schema=?1, pgno=?2
**
** idxStr is not used
*/
static int dbpageFilter(
  sqlite3_vtab_cursor *pCursor, 
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  DbpageCursor *pCsr = (DbpageCursor *)pCursor;
  DbpageTable *pTab = (DbpageTable *)pCursor->pVtab;
  int rc;
  sqlite3 *db = pTab->db;
  Btree *pBt;

  /* Default setting is no rows of result */
  pCsr->pgno = 1; 
  pCsr->mxPgno = 0;

  if( idxNum & 2 ){
    const char *zSchema;
    assert( argc>=1 );
    zSchema = (const char*)sqlite3_value_text(argv[0]);
    pCsr->iDb = sqlite3FindDbName(db, zSchema);
    if( pCsr->iDb<0 ) return SQLITE_OK;
  }else{
    pCsr->iDb = 0;
  }
  pBt = db->aDb[pCsr->iDb].pBt;
  if( pBt==0 ) return SQLITE_OK;
  pCsr->pPager = sqlite3BtreePager(pBt);
  pCsr->szPage = sqlite3BtreeGetPageSize(pBt);
  pCsr->mxPgno = sqlite3BtreeLastPage(pBt);
  if( idxNum & 1 ){
    assert( argc>(idxNum>>1) );
    pCsr->pgno = sqlite3_value_int(argv[idxNum>>1]);
    if( pCsr->pgno<1 || pCsr->pgno>pCsr->mxPgno ){
      pCsr->pgno = 1;
      pCsr->mxPgno = 0;
    }else{
      pCsr->mxPgno = pCsr->pgno;
    }
  }else{
    assert( pCsr->pgno==1 );
  }
  if( pCsr->pPage1 ) sqlite3PagerUnrefPageOne(pCsr->pPage1);
  rc = sqlite3PagerGet(pCsr->pPager, 1, &pCsr->pPage1, 0);
  return rc;
}

static int dbpageColumn(
  sqlite3_vtab_cursor *pCursor, 
  sqlite3_context *ctx, 
  int i
){
  DbpageCursor *pCsr = (DbpageCursor *)pCursor;
  int rc = SQLITE_OK;
  switch( i ){
    case 0: {           /* pgno */
      sqlite3_result_int(ctx, pCsr->pgno);
      break;
    }
    case 1: {           /* data */
      DbPage *pDbPage = 0;
      rc = sqlite3PagerGet(pCsr->pPager, pCsr->pgno, (DbPage**)&pDbPage, 0);
      if( rc==SQLITE_OK ){
        sqlite3_result_blob(ctx, sqlite3PagerGetData(pDbPage), pCsr->szPage,
                            SQLITE_TRANSIENT);
      }
      sqlite3PagerUnref(pDbPage);
      break;
    }
    default: {          /* schema */
      sqlite3 *db = sqlite3_context_db_handle(ctx);
      sqlite3_result_text(ctx, db->aDb[pCsr->iDb].zDbSName, -1, SQLITE_STATIC);
      break;
    }
  }
  return SQLITE_OK;
}

static int dbpageRowid(sqlite3_vtab_cursor *pCursor, sqlite_int64 *pRowid){
  DbpageCursor *pCsr = (DbpageCursor *)pCursor;
  *pRowid = pCsr->pgno;
  return SQLITE_OK;
}

static int dbpageUpdate(
  sqlite3_vtab *pVtab,
  int argc,
  sqlite3_value **argv,
  sqlite_int64 *pRowid
){
  DbpageTable *pTab = (DbpageTable *)pVtab;
  Pgno pgno;
  DbPage *pDbPage = 0;
  int rc = SQLITE_OK;
  char *zErr = 0;
  const char *zSchema;
  int iDb;
  Btree *pBt;
  Pager *pPager;
  int szPage;

  if( pTab->db->flags & SQLITE_Defensive ){
    zErr = "read-only";
    goto update_fail;
  }
  if( argc==1 ){
    zErr = "cannot delete";
    goto update_fail;
  }
  pgno = sqlite3_value_int(argv[0]);
  if( (Pgno)sqlite3_value_int(argv[1])!=pgno ){
    zErr = "cannot insert";
    goto update_fail;
  }
  zSchema = (const char*)sqlite3_value_text(argv[4]);
  iDb = zSchema ? sqlite3FindDbName(pTab->db, zSchema) : -1;
  if( iDb<0 ){
    zErr = "no such schema";
    goto update_fail;
  }
  pBt = pTab->db->aDb[iDb].pBt;
  if( pgno<1 || pBt==0 || pgno>(int)sqlite3BtreeLastPage(pBt) ){
    zErr = "bad page number";
    goto update_fail;
  }
  szPage = sqlite3BtreeGetPageSize(pBt);
  if( sqlite3_value_type(argv[3])!=SQLITE_BLOB 
   || sqlite3_value_bytes(argv[3])!=szPage
  ){
    zErr = "bad page value";
    goto update_fail;
  }
  pPager = sqlite3BtreePager(pBt);
  rc = sqlite3PagerGet(pPager, pgno, (DbPage**)&pDbPage, 0);
  if( rc==SQLITE_OK ){
    rc = sqlite3PagerWrite(pDbPage);
    if( rc==SQLITE_OK ){
      memcpy(sqlite3PagerGetData(pDbPage),
             sqlite3_value_blob(argv[3]),
             szPage);
    }
  }
  sqlite3PagerUnref(pDbPage);
  return rc;

update_fail:
  sqlite3_free(pVtab->zErrMsg);
  pVtab->zErrMsg = sqlite3_mprintf("%s", zErr);
  return SQLITE_ERROR;
}

/* Since we do not know in advance which database files will be
** written by the sqlite_dbpage virtual table, start a write transaction
** on them all.
*/
static int dbpageBegin(sqlite3_vtab *pVtab){
  DbpageTable *pTab = (DbpageTable *)pVtab;
  sqlite3 *db = pTab->db;
  int i;
  for(i=0; i<db->nDb; i++){
    Btree *pBt = db->aDb[i].pBt;
    if( pBt ) sqlite3BtreeBeginTrans(pBt, 1, 0);
  }
  return SQLITE_OK;
}


/*
** Invoke this routine to register the "dbpage" virtual table module
*/
SQLITE_PRIVATE int sqlite3DbpageRegister(sqlite3 *db){
  static sqlite3_module dbpage_module = {
    0,                            /* iVersion */
    dbpageConnect,                /* xCreate */
    dbpageConnect,                /* xConnect */
    dbpageBestIndex,              /* xBestIndex */
    dbpageDisconnect,             /* xDisconnect */
    dbpageDisconnect,             /* xDestroy */
    dbpageOpen,                   /* xOpen - open a cursor */
    dbpageClose,                  /* xClose - close a cursor */
    dbpageFilter,                 /* xFilter - configure scan constraints */
    dbpageNext,                   /* xNext - advance a cursor */
    dbpageEof,                    /* xEof - check for end of scan */
    dbpageColumn,                 /* xColumn - read data */
    dbpageRowid,                  /* xRowid - read data */
    dbpageUpdate,                 /* xUpdate */
    dbpageBegin,                  /* xBegin */
    0,                            /* xSync */
    0,                            /* xCommit */
    0,                            /* xRollback */
    0,                            /* xFindMethod */
    0,                            /* xRename */
    0,                            /* xSavepoint */
    0,                            /* xRelease */
    0,                            /* xRollbackTo */
    0                             /* xShadowName */
  };
  return sqlite3_create_module(db, "sqlite_dbpage", &dbpage_module, 0);
}
#elif defined(SQLITE_ENABLE_DBPAGE_VTAB)
SQLITE_PRIVATE int sqlite3DbpageRegister(sqlite3 *db){ return SQLITE_OK; }
#endif /* SQLITE_ENABLE_DBSTAT_VTAB */

/************** End of dbpage.c **********************************************/
/************** Begin file sqlite3session.c **********************************/

#if defined(SQLITE_ENABLE_SESSION) && defined(SQLITE_ENABLE_PREUPDATE_HOOK)
/* #include "sqlite3session.h" */
/* #include <assert.h> */
/* #include <string.h> */

#ifndef SQLITE_AMALGAMATION
/* # include "sqliteInt.h" */
/* # include "vdbeInt.h" */
#endif

typedef struct SessionTable SessionTable;
typedef struct SessionChange SessionChange;
typedef struct SessionBuffer SessionBuffer;
typedef struct SessionInput SessionInput;

/*
** Minimum chunk size used by streaming versions of functions.
*/
#ifndef SESSIONS_STRM_CHUNK_SIZE
# ifdef SQLITE_TEST
#   define SESSIONS_STRM_CHUNK_SIZE 64
# else
#   define SESSIONS_STRM_CHUNK_SIZE 1024
# endif
#endif

static int sessions_strm_chunk_size = SESSIONS_STRM_CHUNK_SIZE;

typedef struct SessionHook SessionHook;
struct SessionHook {
  void *pCtx;
  int (*xOld)(void*,int,sqlite3_value**);
  int (*xNew)(void*,int,sqlite3_value**);
  int (*xCount)(void*);
  int (*xDepth)(void*);
};

/*
** Session handle structure.
*/
struct sqlite3_session {
  sqlite3 *db;                    /* Database handle session is attached to */
  char *zDb;                      /* Name of database session is attached to */
  int bEnable;                    /* True if currently recording */
  int bIndirect;                  /* True if all changes are indirect */
  int bAutoAttach;                /* True to auto-attach tables */
  int rc;                         /* Non-zero if an error has occurred */
  void *pFilterCtx;               /* First argument to pass to xTableFilter */
  int (*xTableFilter)(void *pCtx, const char *zTab);
  sqlite3_value *pZeroBlob;       /* Value containing X'' */
  sqlite3_session *pNext;         /* Next session object on same db. */
  SessionTable *pTable;           /* List of attached tables */
  SessionHook hook;               /* APIs to grab new and old data with */
};

/*
** Instances of this structure are used to build strings or binary records.
*/
struct SessionBuffer {
  u8 *aBuf;                       /* Pointer to changeset buffer */
  int nBuf;                       /* Size of buffer aBuf */
  int nAlloc;                     /* Size of allocation containing aBuf */
};

/*
** An object of this type is used internally as an abstraction for 
** input data. Input data may be supplied either as a single large buffer
** (e.g. sqlite3changeset_start()) or using a stream function (e.g.
**  sqlite3changeset_start_strm()).
*/
struct SessionInput {
  int bNoDiscard;                 /* If true, do not discard in InputBuffer() */
  int iCurrent;                   /* Offset in aData[] of current change */
  int iNext;                      /* Offset in aData[] of next change */
  u8 *aData;                      /* Pointer to buffer containing changeset */
  int nData;                      /* Number of bytes in aData */

  SessionBuffer buf;              /* Current read buffer */
  int (*xInput)(void*, void*, int*);        /* Input stream call (or NULL) */
  void *pIn;                                /* First argument to xInput */
  int bEof;                       /* Set to true after xInput finished */
};

/*
** Structure for changeset iterators.
*/
struct sqlite3_changeset_iter {
  SessionInput in;                /* Input buffer or stream */
  SessionBuffer tblhdr;           /* Buffer to hold apValue/zTab/abPK/ */
  int bPatchset;                  /* True if this is a patchset */
  int bInvert;                    /* True to invert changeset */
  int rc;                         /* Iterator error code */
  sqlite3_stmt *pConflict;        /* Points to conflicting row, if any */
  char *zTab;                     /* Current table */
  int nCol;                       /* Number of columns in zTab */
  int op;                         /* Current operation */
  int bIndirect;                  /* True if current change was indirect */
  u8 *abPK;                       /* Primary key array */
  sqlite3_value **apValue;        /* old.* and new.* values */
};

/*
** Each session object maintains a set of the following structures, one
** for each table the session object is monitoring. The structures are
** stored in a linked list starting at sqlite3_session.pTable.
**
** The keys of the SessionTable.aChange[] hash table are all rows that have
** been modified in any way since the session object was attached to the
** table.
**
** The data associated with each hash-table entry is a structure containing
** a subset of the initial values that the modified row contained at the
** start of the session. Or no initial values if the row was inserted.
*/
struct SessionTable {
  SessionTable *pNext;
  char *zName;                    /* Local name of table */
  int nCol;                       /* Number of columns in table zName */
  int bStat1;                     /* True if this is sqlite_stat1 */
  const char **azCol;             /* Column names */
  u8 *abPK;                       /* Array of primary key flags */
  int nEntry;                     /* Total number of entries in hash table */
  int nChange;                    /* Size of apChange[] array */
  SessionChange **apChange;       /* Hash table buckets */
};

/* 
** RECORD FORMAT:
**
** The following record format is similar to (but not compatible with) that 
** used in SQLite database files. This format is used as part of the 
** change-set binary format, and so must be architecture independent.
**
** Unlike the SQLite database record format, each field is self-contained -
** there is no separation of header and data. Each field begins with a
** single byte describing its type, as follows:
**
**       0x00: Undefined value.
**       0x01: Integer value.
**       0x02: Real value.
**       0x03: Text value.
**       0x04: Blob value.
**       0x05: SQL NULL value.
**
** Note that the above match the definitions of SQLITE_INTEGER, SQLITE_TEXT
** and so on in sqlite3.h. For undefined and NULL values, the field consists
** only of the single type byte. For other types of values, the type byte
** is followed by:
**
**   Text values:
**     A varint containing the number of bytes in the value (encoded using
**     UTF-8). Followed by a buffer containing the UTF-8 representation
**     of the text value. There is no nul terminator.
**
**   Blob values:
**     A varint containing the number of bytes in the value, followed by
**     a buffer containing the value itself.
**
**   Integer values:
**     An 8-byte big-endian integer value.
**
**   Real values:
**     An 8-byte big-endian IEEE 754-2008 real value.
**
** Varint values are encoded in the same way as varints in the SQLite 
** record format.
**
** CHANGESET FORMAT:
**
** A changeset is a collection of DELETE, UPDATE and INSERT operations on
** one or more tables. Operations on a single table are grouped together,
** but may occur in any order (i.e. deletes, updates and inserts are all
** mixed together).
**
** Each group of changes begins with a table header:
**
**   1 byte: Constant 0x54 (capital 'T')
**   Varint: Number of columns in the table.
**   nCol bytes: 0x01 for PK columns, 0x00 otherwise.
**   N bytes: Unqualified table name (encoded using UTF-8). Nul-terminated.
**
** Followed by one or more changes to the table.
**
**   1 byte: Either SQLITE_INSERT (0x12), UPDATE (0x17) or DELETE (0x09).
**   1 byte: The "indirect-change" flag.
**   old.* record: (delete and update only)
**   new.* record: (insert and update only)
**
** The "old.*" and "new.*" records, if present, are N field records in the
** format described above under "RECORD FORMAT", where N is the number of
** columns in the table. The i'th field of each record is associated with
** the i'th column of the table, counting from left to right in the order
** in which columns were declared in the CREATE TABLE statement.
**
** The new.* record that is part of each INSERT change contains the values
** that make up the new row. Similarly, the old.* record that is part of each
** DELETE change contains the values that made up the row that was deleted 
** from the database. In the changeset format, the records that are part
** of INSERT or DELETE changes never contain any undefined (type byte 0x00)
** fields.
**
** Within the old.* record associated with an UPDATE change, all fields
** associated with table columns that are not PRIMARY KEY columns and are
** not modified by the UPDATE change are set to "undefined". Other fields
** are set to the values that made up the row before the UPDATE that the
** change records took place. Within the new.* record, fields associated 
** with table columns modified by the UPDATE change contain the new 
** values. Fields associated with table columns that are not modified
** are set to "undefined".
**
** PATCHSET FORMAT:
**
** A patchset is also a collection of changes. It is similar to a changeset,
** but leaves undefined those fields that are not useful if no conflict
** resolution is required when applying the changeset.
**
** Each group of changes begins with a table header:
**
**   1 byte: Constant 0x50 (capital 'P')
**   Varint: Number of columns in the table.
**   nCol bytes: 0x01 for PK columns, 0x00 otherwise.
**   N bytes: Unqualified table name (encoded using UTF-8). Nul-terminated.
**
** Followed by one or more changes to the table.
**
**   1 byte: Either SQLITE_INSERT (0x12), UPDATE (0x17) or DELETE (0x09).
**   1 byte: The "indirect-change" flag.
**   single record: (PK fields for DELETE, PK and modified fields for UPDATE,
**                   full record for INSERT).
**
** As in the changeset format, each field of the single record that is part
** of a patchset change is associated with the correspondingly positioned
** table column, counting from left to right within the CREATE TABLE 
** statement.
**
** For a DELETE change, all fields within the record except those associated
** with PRIMARY KEY columns are omitted. The PRIMARY KEY fields contain the
** values identifying the row to delete.
**
** For an UPDATE change, all fields except those associated with PRIMARY KEY
** columns and columns that are modified by the UPDATE are set to "undefined".
** PRIMARY KEY fields contain the values identifying the table row to update,
** and fields associated with modified columns contain the new column values.
**
** The records associated with INSERT changes are in the same format as for
** changesets. It is not possible for a record associated with an INSERT
** change to contain a field set to "undefined".
**
** REBASE BLOB FORMAT:
**
** A rebase blob may be output by sqlite3changeset_apply_v2() and its 
** streaming equivalent for use with the sqlite3_rebaser APIs to rebase
** existing changesets. A rebase blob contains one entry for each conflict
** resolved using either the OMIT or REPLACE strategies within the apply_v2()
** call.
**
** The format used for a rebase blob is very similar to that used for
** changesets. All entries related to a single table are grouped together.
**
** Each group of entries begins with a table header in changeset format:
**
**   1 byte: Constant 0x54 (capital 'T')
**   Varint: Number of columns in the table.
**   nCol bytes: 0x01 for PK columns, 0x00 otherwise.
**   N bytes: Unqualified table name (encoded using UTF-8). Nul-terminated.
**
** Followed by one or more entries associated with the table.
**
**   1 byte: Either SQLITE_INSERT (0x12), DELETE (0x09).
**   1 byte: Flag. 0x01 for REPLACE, 0x00 for OMIT.
**   record: (in the record format defined above).
**
** In a rebase blob, the first field is set to SQLITE_INSERT if the change
** that caused the conflict was an INSERT or UPDATE, or to SQLITE_DELETE if
** it was a DELETE. The second field is set to 0x01 if the conflict 
** resolution strategy was REPLACE, or 0x00 if it was OMIT.
**
** If the change that caused the conflict was a DELETE, then the single
** record is a copy of the old.* record from the original changeset. If it
** was an INSERT, then the single record is a copy of the new.* record. If
** the conflicting change was an UPDATE, then the single record is a copy
** of the new.* record with the PK fields filled in based on the original
** old.* record.
*/

/*
** For each row modified during a session, there exists a single instance of
** this structure stored in a SessionTable.aChange[] hash table.
*/
struct SessionChange {
  int op;                         /* One of UPDATE, DELETE, INSERT */
  int bIndirect;                  /* True if this change is "indirect" */
  int nRecord;                    /* Number of bytes in buffer aRecord[] */
  u8 *aRecord;                    /* Buffer containing old.* record */
  SessionChange *pNext;           /* For hash-table collisions */
};

/*
** Write a varint with value iVal into the buffer at aBuf. Return the 
** number of bytes written.
*/
static int sessionVarintPut(u8 *aBuf, int iVal){
  return putVarint32(aBuf, iVal);
}

/*
** Return the number of bytes required to store value iVal as a varint.
*/
static int sessionVarintLen(int iVal){
  return sqlite3VarintLen(iVal);
}

/*
** Read a varint value from aBuf[] into *piVal. Return the number of 
** bytes read.
*/
static int sessionVarintGet(u8 *aBuf, int *piVal){
  return getVarint32(aBuf, *piVal);
}

/* Load an unaligned and unsigned 32-bit integer */
#define SESSION_UINT32(x) (((u32)(x)[0]<<24)|((x)[1]<<16)|((x)[2]<<8)|(x)[3])

/*
** Read a 64-bit big-endian integer value from buffer aRec[]. Return
** the value read.
*/
static sqlite3_int64 sessionGetI64(u8 *aRec){
  u64 x = SESSION_UINT32(aRec);
  u32 y = SESSION_UINT32(aRec+4);
  x = (x<<32) + y;
  return (sqlite3_int64)x;
}

/*
** Write a 64-bit big-endian integer value to the buffer aBuf[].
*/
static void sessionPutI64(u8 *aBuf, sqlite3_int64 i){
  aBuf[0] = (i>>56) & 0xFF;
  aBuf[1] = (i>>48) & 0xFF;
  aBuf[2] = (i>>40) & 0xFF;
  aBuf[3] = (i>>32) & 0xFF;
  aBuf[4] = (i>>24) & 0xFF;
  aBuf[5] = (i>>16) & 0xFF;
  aBuf[6] = (i>> 8) & 0xFF;
  aBuf[7] = (i>> 0) & 0xFF;
}

/*
** This function is used to serialize the contents of value pValue (see
** comment titled "RECORD FORMAT" above).
**
** If it is non-NULL, the serialized form of the value is written to 
** buffer aBuf. *pnWrite is set to the number of bytes written before
** returning. Or, if aBuf is NULL, the only thing this function does is
** set *pnWrite.
**
** If no error occurs, SQLITE_OK is returned. Or, if an OOM error occurs
** within a call to sqlite3_value_text() (may fail if the db is utf-16)) 
** SQLITE_NOMEM is returned.
*/
static int sessionSerializeValue(
  u8 *aBuf,                       /* If non-NULL, write serialized value here */
  sqlite3_value *pValue,          /* Value to serialize */
  sqlite3_int64 *pnWrite          /* IN/OUT: Increment by bytes written */
){
  int nByte;                      /* Size of serialized value in bytes */

  if( pValue ){
    int eType;                    /* Value type (SQLITE_NULL, TEXT etc.) */
  
    eType = sqlite3_value_type(pValue);
    if( aBuf ) aBuf[0] = eType;
  
    switch( eType ){
      case SQLITE_NULL: 
        nByte = 1;
        break;
  
      case SQLITE_INTEGER: 
      case SQLITE_FLOAT:
        if( aBuf ){
          /* TODO: SQLite does something special to deal with mixed-endian
          ** floating point values (e.g. ARM7). This code probably should
          ** too.  */
          u64 i;
          if( eType==SQLITE_INTEGER ){
            i = (u64)sqlite3_value_int64(pValue);
          }else{
            double r;
            assert( sizeof(double)==8 && sizeof(u64)==8 );
            r = sqlite3_value_double(pValue);
            memcpy(&i, &r, 8);
          }
          sessionPutI64(&aBuf[1], i);
        }
        nByte = 9; 
        break;
  
      default: {
        u8 *z;
        int n;
        int nVarint;
  
        assert( eType==SQLITE_TEXT || eType==SQLITE_BLOB );
        if( eType==SQLITE_TEXT ){
          z = (u8 *)sqlite3_value_text(pValue);
        }else{
          z = (u8 *)sqlite3_value_blob(pValue);
        }
        n = sqlite3_value_bytes(pValue);
        if( z==0 && (eType!=SQLITE_BLOB || n>0) ) return SQLITE_NOMEM;
        nVarint = sessionVarintLen(n);
  
        if( aBuf ){
          sessionVarintPut(&aBuf[1], n);
          if( n ) memcpy(&aBuf[nVarint + 1], z, n);
        }
  
        nByte = 1 + nVarint + n;
        break;
      }
    }
  }else{
    nByte = 1;
    if( aBuf ) aBuf[0] = '\0';
  }

  if( pnWrite ) *pnWrite += nByte;
  return SQLITE_OK;
}


/*
** This macro is used to calculate hash key values for data structures. In
** order to use this macro, the entire data structure must be represented
** as a series of unsigned integers. In order to calculate a hash-key value
** for a data structure represented as three such integers, the macro may
** then be used as follows:
**
**    int hash_key_value;
**    hash_key_value = HASH_APPEND(0, <value 1>);
**    hash_key_value = HASH_APPEND(hash_key_value, <value 2>);
**    hash_key_value = HASH_APPEND(hash_key_value, <value 3>);
**
** In practice, the data structures this macro is used for are the primary
** key values of modified rows.
*/
#define HASH_APPEND(hash, add) ((hash) << 3) ^ (hash) ^ (unsigned int)(add)

/*
** Append the hash of the 64-bit integer passed as the second argument to the
** hash-key value passed as the first. Return the new hash-key value.
*/
static unsigned int sessionHashAppendI64(unsigned int h, i64 i){
  h = HASH_APPEND(h, i & 0xFFFFFFFF);
  return HASH_APPEND(h, (i>>32)&0xFFFFFFFF);
}

/*
** Append the hash of the blob passed via the second and third arguments to 
** the hash-key value passed as the first. Return the new hash-key value.
*/
static unsigned int sessionHashAppendBlob(unsigned int h, int n, const u8 *z){
  int i;
  for(i=0; i<n; i++) h = HASH_APPEND(h, z[i]);
  return h;
}

/*
** Append the hash of the data type passed as the second argument to the
** hash-key value passed as the first. Return the new hash-key value.
*/
static unsigned int sessionHashAppendType(unsigned int h, int eType){
  return HASH_APPEND(h, eType);
}

/*
** This function may only be called from within a pre-update callback.
** It calculates a hash based on the primary key values of the old.* or 
** new.* row currently available and, assuming no error occurs, writes it to
** *piHash before returning. If the primary key contains one or more NULL
** values, *pbNullPK is set to true before returning.
**
** If an error occurs, an SQLite error code is returned and the final values
** of *piHash asn *pbNullPK are undefined. Otherwise, SQLITE_OK is returned
** and the output variables are set as described above.
*/
static int sessionPreupdateHash(
  sqlite3_session *pSession,      /* Session object that owns pTab */
  SessionTable *pTab,             /* Session table handle */
  int bNew,                       /* True to hash the new.* PK */
  int *piHash,                    /* OUT: Hash value */
  int *pbNullPK                   /* OUT: True if there are NULL values in PK */
){
  unsigned int h = 0;             /* Hash value to return */
  int i;                          /* Used to iterate through columns */

  assert( *pbNullPK==0 );
  assert( pTab->nCol==pSession->hook.xCount(pSession->hook.pCtx) );
  for(i=0; i<pTab->nCol; i++){
    if( pTab->abPK[i] ){
      int rc;
      int eType;
      sqlite3_value *pVal;

      if( bNew ){
        rc = pSession->hook.xNew(pSession->hook.pCtx, i, &pVal);
      }else{
        rc = pSession->hook.xOld(pSession->hook.pCtx, i, &pVal);
      }
      if( rc!=SQLITE_OK ) return rc;

      eType = sqlite3_value_type(pVal);
      h = sessionHashAppendType(h, eType);
      if( eType==SQLITE_INTEGER || eType==SQLITE_FLOAT ){
        i64 iVal;
        if( eType==SQLITE_INTEGER ){
          iVal = sqlite3_value_int64(pVal);
        }else{
          double rVal = sqlite3_value_double(pVal);
          assert( sizeof(iVal)==8 && sizeof(rVal)==8 );
          memcpy(&iVal, &rVal, 8);
        }
        h = sessionHashAppendI64(h, iVal);
      }else if( eType==SQLITE_TEXT || eType==SQLITE_BLOB ){
        const u8 *z;
        int n;
        if( eType==SQLITE_TEXT ){
          z = (const u8 *)sqlite3_value_text(pVal);
        }else{
          z = (const u8 *)sqlite3_value_blob(pVal);
        }
        n = sqlite3_value_bytes(pVal);
        if( !z && (eType!=SQLITE_BLOB || n>0) ) return SQLITE_NOMEM;
        h = sessionHashAppendBlob(h, n, z);
      }else{
        assert( eType==SQLITE_NULL );
        assert( pTab->bStat1==0 || i!=1 );
        *pbNullPK = 1;
      }
    }
  }

  *piHash = (h % pTab->nChange);
  return SQLITE_OK;
}

/*
** The buffer that the argument points to contains a serialized SQL value.
** Return the number of bytes of space occupied by the value (including
** the type byte).
*/
static int sessionSerialLen(u8 *a){
  int e = *a;
  int n;
  if( e==0 || e==0xFF ) return 1;
  if( e==SQLITE_NULL ) return 1;
  if( e==SQLITE_INTEGER || e==SQLITE_FLOAT ) return 9;
  return sessionVarintGet(&a[1], &n) + 1 + n;
}

/*
** Based on the primary key values stored in change aRecord, calculate a
** hash key. Assume the has table has nBucket buckets. The hash keys
** calculated by this function are compatible with those calculated by
** sessionPreupdateHash().
**
** The bPkOnly argument is non-zero if the record at aRecord[] is from
** a patchset DELETE. In this case the non-PK fields are omitted entirely.
*/
static unsigned int sessionChangeHash(
  SessionTable *pTab,             /* Table handle */
  int bPkOnly,                    /* Record consists of PK fields only */
  u8 *aRecord,                    /* Change record */
  int nBucket                     /* Assume this many buckets in hash table */
){
  unsigned int h = 0;             /* Value to return */
  int i;                          /* Used to iterate through columns */
  u8 *a = aRecord;                /* Used to iterate through change record */

  for(i=0; i<pTab->nCol; i++){
    int eType = *a;
    int isPK = pTab->abPK[i];
    if( bPkOnly && isPK==0 ) continue;

    /* It is not possible for eType to be SQLITE_NULL here. The session 
    ** module does not record changes for rows with NULL values stored in
    ** primary key columns. */
    assert( eType==SQLITE_INTEGER || eType==SQLITE_FLOAT 
         || eType==SQLITE_TEXT || eType==SQLITE_BLOB 
         || eType==SQLITE_NULL || eType==0 
    );
    assert( !isPK || (eType!=0 && eType!=SQLITE_NULL) );

    if( isPK ){
      a++;
      h = sessionHashAppendType(h, eType);
      if( eType==SQLITE_INTEGER || eType==SQLITE_FLOAT ){
        h = sessionHashAppendI64(h, sessionGetI64(a));
        a += 8;
      }else{
        int n; 
        a += sessionVarintGet(a, &n);
        h = sessionHashAppendBlob(h, n, a);
        a += n;
      }
    }else{
      a += sessionSerialLen(a);
    }
  }
  return (h % nBucket);
}

/*
** Arguments aLeft and aRight are pointers to change records for table pTab.
** This function returns true if the two records apply to the same row (i.e.
** have the same values stored in the primary key columns), or false 
** otherwise.
*/
static int sessionChangeEqual(
  SessionTable *pTab,             /* Table used for PK definition */
  int bLeftPkOnly,                /* True if aLeft[] contains PK fields only */
  u8 *aLeft,                      /* Change record */
  int bRightPkOnly,               /* True if aRight[] contains PK fields only */
  u8 *aRight                      /* Change record */
){
  u8 *a1 = aLeft;                 /* Cursor to iterate through aLeft */
  u8 *a2 = aRight;                /* Cursor to iterate through aRight */
  int iCol;                       /* Used to iterate through table columns */

  for(iCol=0; iCol<pTab->nCol; iCol++){
    if( pTab->abPK[iCol] ){
      int n1 = sessionSerialLen(a1);
      int n2 = sessionSerialLen(a2);

      if( n1!=n2 || memcmp(a1, a2, n1) ){
        return 0;
      }
      a1 += n1;
      a2 += n2;
    }else{
      if( bLeftPkOnly==0 ) a1 += sessionSerialLen(a1);
      if( bRightPkOnly==0 ) a2 += sessionSerialLen(a2);
    }
  }

  return 1;
}

/*
** Arguments aLeft and aRight both point to buffers containing change
** records with nCol columns. This function "merges" the two records into
** a single records which is written to the buffer at *paOut. *paOut is
** then set to point to one byte after the last byte written before 
** returning.
**
** The merging of records is done as follows: For each column, if the 
** aRight record contains a value for the column, copy the value from
** their. Otherwise, if aLeft contains a value, copy it. If neither
** record contains a value for a given column, then neither does the
** output record.
*/
static void sessionMergeRecord(
  u8 **paOut, 
  int nCol,
  u8 *aLeft,
  u8 *aRight
){
  u8 *a1 = aLeft;                 /* Cursor used to iterate through aLeft */
  u8 *a2 = aRight;                /* Cursor used to iterate through aRight */
  u8 *aOut = *paOut;              /* Output cursor */
  int iCol;                       /* Used to iterate from 0 to nCol */

  for(iCol=0; iCol<nCol; iCol++){
    int n1 = sessionSerialLen(a1);
    int n2 = sessionSerialLen(a2);
    if( *a2 ){
      memcpy(aOut, a2, n2);
      aOut += n2;
    }else{
      memcpy(aOut, a1, n1);
      aOut += n1;
    }
    a1 += n1;
    a2 += n2;
  }

  *paOut = aOut;
}

/*
** This is a helper function used by sessionMergeUpdate().
**
** When this function is called, both *paOne and *paTwo point to a value 
** within a change record. Before it returns, both have been advanced so 
** as to point to the next value in the record.
**
** If, when this function is called, *paTwo points to a valid value (i.e.
** *paTwo[0] is not 0x00 - the "no value" placeholder), a copy of the *paTwo
** pointer is returned and *pnVal is set to the number of bytes in the 
** serialized value. Otherwise, a copy of *paOne is returned and *pnVal
** set to the number of bytes in the value at *paOne. If *paOne points
** to the "no value" placeholder, *pnVal is set to 1. In other words:
**
**   if( *paTwo is valid ) return *paTwo;
**   return *paOne;
**
*/
static u8 *sessionMergeValue(
  u8 **paOne,                     /* IN/OUT: Left-hand buffer pointer */
  u8 **paTwo,                     /* IN/OUT: Right-hand buffer pointer */
  int *pnVal                      /* OUT: Bytes in returned value */
){
  u8 *a1 = *paOne;
  u8 *a2 = *paTwo;
  u8 *pRet = 0;
  int n1;

  assert( a1 );
  if( a2 ){
    int n2 = sessionSerialLen(a2);
    if( *a2 ){
      *pnVal = n2;
      pRet = a2;
    }
    *paTwo = &a2[n2];
  }

  n1 = sessionSerialLen(a1);
  if( pRet==0 ){
    *pnVal = n1;
    pRet = a1;
  }
  *paOne = &a1[n1];

  return pRet;
}

/*
** This function is used by changeset_concat() to merge two UPDATE changes
** on the same row.
*/
static int sessionMergeUpdate(
  u8 **paOut,                     /* IN/OUT: Pointer to output buffer */
  SessionTable *pTab,             /* Table change pertains to */
  int bPatchset,                  /* True if records are patchset records */
  u8 *aOldRecord1,                /* old.* record for first change */
  u8 *aOldRecord2,                /* old.* record for second change */
  u8 *aNewRecord1,                /* new.* record for first change */
  u8 *aNewRecord2                 /* new.* record for second change */
){
  u8 *aOld1 = aOldRecord1;
  u8 *aOld2 = aOldRecord2;
  u8 *aNew1 = aNewRecord1;
  u8 *aNew2 = aNewRecord2;

  u8 *aOut = *paOut;
  int i;

  if( bPatchset==0 ){
    int bRequired = 0;

    assert( aOldRecord1 && aNewRecord1 );

    /* Write the old.* vector first. */
    for(i=0; i<pTab->nCol; i++){
      int nOld;
      u8 *aOld;
      int nNew;
      u8 *aNew;

      aOld = sessionMergeValue(&aOld1, &aOld2, &nOld);
      aNew = sessionMergeValue(&aNew1, &aNew2, &nNew);
      if( pTab->abPK[i] || nOld!=nNew || memcmp(aOld, aNew, nNew) ){
        if( pTab->abPK[i]==0 ) bRequired = 1;
        memcpy(aOut, aOld, nOld);
        aOut += nOld;
      }else{
        *(aOut++) = '\0';
      }
    }

    if( !bRequired ) return 0;
  }

  /* Write the new.* vector */
  aOld1 = aOldRecord1;
  aOld2 = aOldRecord2;
  aNew1 = aNewRecord1;
  aNew2 = aNewRecord2;
  for(i=0; i<pTab->nCol; i++){
    int nOld;
    u8 *aOld;
    int nNew;
    u8 *aNew;

    aOld = sessionMergeValue(&aOld1, &aOld2, &nOld);
    aNew = sessionMergeValue(&aNew1, &aNew2, &nNew);
    if( bPatchset==0 
     && (pTab->abPK[i] || (nOld==nNew && 0==memcmp(aOld, aNew, nNew))) 
    ){
      *(aOut++) = '\0';
    }else{
      memcpy(aOut, aNew, nNew);
      aOut += nNew;
    }
  }

  *paOut = aOut;
  return 1;
}

/*
** This function is only called from within a pre-update-hook callback.
** It determines if the current pre-update-hook change affects the same row
** as the change stored in argument pChange. If so, it returns true. Otherwise
** if the pre-update-hook does not affect the same row as pChange, it returns
** false.
*/
static int sessionPreupdateEqual(
  sqlite3_session *pSession,      /* Session object that owns SessionTable */
  SessionTable *pTab,             /* Table associated with change */
  SessionChange *pChange,         /* Change to compare to */
  int op                          /* Current pre-update operation */
){
  int iCol;                       /* Used to iterate through columns */
  u8 *a = pChange->aRecord;       /* Cursor used to scan change record */

  assert( op==SQLITE_INSERT || op==SQLITE_UPDATE || op==SQLITE_DELETE );
  for(iCol=0; iCol<pTab->nCol; iCol++){
    if( !pTab->abPK[iCol] ){
      a += sessionSerialLen(a);
    }else{
      sqlite3_value *pVal;        /* Value returned by preupdate_new/old */
      int rc;                     /* Error code from preupdate_new/old */
      int eType = *a++;           /* Type of value from change record */

      /* The following calls to preupdate_new() and preupdate_old() can not
      ** fail. This is because they cache their return values, and by the
      ** time control flows to here they have already been called once from
      ** within sessionPreupdateHash(). The first two asserts below verify
      ** this (that the method has already been called). */
      if( op==SQLITE_INSERT ){
        /* assert( db->pPreUpdate->pNewUnpacked || db->pPreUpdate->aNew ); */
        rc = pSession->hook.xNew(pSession->hook.pCtx, iCol, &pVal);
      }else{
        /* assert( db->pPreUpdate->pUnpacked ); */
        rc = pSession->hook.xOld(pSession->hook.pCtx, iCol, &pVal);
      }
      assert( rc==SQLITE_OK );
      if( sqlite3_value_type(pVal)!=eType ) return 0;

      /* A SessionChange object never has a NULL value in a PK column */
      assert( eType==SQLITE_INTEGER || eType==SQLITE_FLOAT
           || eType==SQLITE_BLOB    || eType==SQLITE_TEXT
      );

      if( eType==SQLITE_INTEGER || eType==SQLITE_FLOAT ){
        i64 iVal = sessionGetI64(a);
        a += 8;
        if( eType==SQLITE_INTEGER ){
          if( sqlite3_value_int64(pVal)!=iVal ) return 0;
        }else{
          double rVal;
          assert( sizeof(iVal)==8 && sizeof(rVal)==8 );
          memcpy(&rVal, &iVal, 8);
          if( sqlite3_value_double(pVal)!=rVal ) return 0;
        }
      }else{
        int n;
        const u8 *z;
        a += sessionVarintGet(a, &n);
        if( sqlite3_value_bytes(pVal)!=n ) return 0;
        if( eType==SQLITE_TEXT ){
          z = sqlite3_value_text(pVal);
        }else{
          z = sqlite3_value_blob(pVal);
        }
        if( n>0 && memcmp(a, z, n) ) return 0;
        a += n;
      }
    }
  }

  return 1;
}

/*
** If required, grow the hash table used to store changes on table pTab 
** (part of the session pSession). If a fatal OOM error occurs, set the
** session object to failed and return SQLITE_ERROR. Otherwise, return
** SQLITE_OK.
**
** It is possible that a non-fatal OOM error occurs in this function. In
** that case the hash-table does not grow, but SQLITE_OK is returned anyway.
** Growing the hash table in this case is a performance optimization only,
** it is not required for correct operation.
*/
static int sessionGrowHash(int bPatchset, SessionTable *pTab){
  if( pTab->nChange==0 || pTab->nEntry>=(pTab->nChange/2) ){
    int i;
    SessionChange **apNew;
    sqlite3_int64 nNew = 2*(sqlite3_int64)(pTab->nChange ? pTab->nChange : 128);

    apNew = (SessionChange **)sqlite3_malloc64(sizeof(SessionChange *) * nNew);
    if( apNew==0 ){
      if( pTab->nChange==0 ){
        return SQLITE_ERROR;
      }
      return SQLITE_OK;
    }
    memset(apNew, 0, sizeof(SessionChange *) * nNew);

    for(i=0; i<pTab->nChange; i++){
      SessionChange *p;
      SessionChange *pNext;
      for(p=pTab->apChange[i]; p; p=pNext){
        int bPkOnly = (p->op==SQLITE_DELETE && bPatchset);
        int iHash = sessionChangeHash(pTab, bPkOnly, p->aRecord, nNew);
        pNext = p->pNext;
        p->pNext = apNew[iHash];
        apNew[iHash] = p;
      }
    }

    sqlite3_free(pTab->apChange);
    pTab->nChange = nNew;
    pTab->apChange = apNew;
  }

  return SQLITE_OK;
}

/*
** This function queries the database for the names of the columns of table
** zThis, in schema zDb.
**
** Otherwise, if they are not NULL, variable *pnCol is set to the number
** of columns in the database table and variable *pzTab is set to point to a
** nul-terminated copy of the table name. *pazCol (if not NULL) is set to
** point to an array of pointers to column names. And *pabPK (again, if not
** NULL) is set to point to an array of booleans - true if the corresponding
** column is part of the primary key.
**
** For example, if the table is declared as:
**
**     CREATE TABLE tbl1(w, x, y, z, PRIMARY KEY(w, z));
**
** Then the four output variables are populated as follows:
**
**     *pnCol  = 4
**     *pzTab  = "tbl1"
**     *pazCol = {"w", "x", "y", "z"}
**     *pabPK  = {1, 0, 0, 1}
**
** All returned buffers are part of the same single allocation, which must
** be freed using sqlite3_free() by the caller
*/
static int sessionTableInfo(
  sqlite3 *db,                    /* Database connection */
  const char *zDb,                /* Name of attached database (e.g. "main") */
  const char *zThis,              /* Table name */
  int *pnCol,                     /* OUT: number of columns */
  const char **pzTab,             /* OUT: Copy of zThis */
  const char ***pazCol,           /* OUT: Array of column names for table */
  u8 **pabPK                      /* OUT: Array of booleans - true for PK col */
){
  char *zPragma;
  sqlite3_stmt *pStmt;
  int rc;
  sqlite3_int64 nByte;
  int nDbCol = 0;
  int nThis;
  int i;
  u8 *pAlloc = 0;
  char **azCol = 0;
  u8 *abPK = 0;

  assert( pazCol && pabPK );

  nThis = sqlite3Strlen30(zThis);
  if( nThis==12 && 0==sqlite3_stricmp("sqlite_stat1", zThis) ){
    rc = sqlite3_table_column_metadata(db, zDb, zThis, 0, 0, 0, 0, 0, 0);
    if( rc==SQLITE_OK ){
      /* For sqlite_stat1, pretend that (tbl,idx) is the PRIMARY KEY. */
      zPragma = sqlite3_mprintf(
          "SELECT 0, 'tbl',  '', 0, '', 1     UNION ALL "
          "SELECT 1, 'idx',  '', 0, '', 2     UNION ALL "
          "SELECT 2, 'stat', '', 0, '', 0"
      );
    }else if( rc==SQLITE_ERROR ){
      zPragma = sqlite3_mprintf("");
    }else{
      return rc;
    }
  }else{
    zPragma = sqlite3_mprintf("PRAGMA '%q'.table_info('%q')", zDb, zThis);
  }
  if( !zPragma ) return SQLITE_NOMEM;

  rc = sqlite3_prepare_v2(db, zPragma, -1, &pStmt, 0);
  sqlite3_free(zPragma);
  if( rc!=SQLITE_OK ) return rc;

  nByte = nThis + 1;
  while( SQLITE_ROW==sqlite3_step(pStmt) ){
    nByte += sqlite3_column_bytes(pStmt, 1);
    nDbCol++;
  }
  rc = sqlite3_reset(pStmt);

  if( rc==SQLITE_OK ){
    nByte += nDbCol * (sizeof(const char *) + sizeof(u8) + 1);
    pAlloc = sqlite3_malloc64(nByte);
    if( pAlloc==0 ){
      rc = SQLITE_NOMEM;
    }
  }
  if( rc==SQLITE_OK ){
    azCol = (char **)pAlloc;
    pAlloc = (u8 *)&azCol[nDbCol];
    abPK = (u8 *)pAlloc;
    pAlloc = &abPK[nDbCol];
    if( pzTab ){
      memcpy(pAlloc, zThis, nThis+1);
      *pzTab = (char *)pAlloc;
      pAlloc += nThis+1;
    }
  
    i = 0;
    while( SQLITE_ROW==sqlite3_step(pStmt) ){
      int nName = sqlite3_column_bytes(pStmt, 1);
      const unsigned char *zName = sqlite3_column_text(pStmt, 1);
      if( zName==0 ) break;
      memcpy(pAlloc, zName, nName+1);
      azCol[i] = (char *)pAlloc;
      pAlloc += nName+1;
      abPK[i] = sqlite3_column_int(pStmt, 5);
      i++;
    }
    rc = sqlite3_reset(pStmt);
  
  }

  /* If successful, populate the output variables. Otherwise, zero them and
  ** free any allocation made. An error code will be returned in this case.
  */
  if( rc==SQLITE_OK ){
    *pazCol = (const char **)azCol;
    *pabPK = abPK;
    *pnCol = nDbCol;
  }else{
    *pazCol = 0;
    *pabPK = 0;
    *pnCol = 0;
    if( pzTab ) *pzTab = 0;
    sqlite3_free(azCol);
  }
  sqlite3_finalize(pStmt);
  return rc;
}

/*
** This function is only called from within a pre-update handler for a
** write to table pTab, part of session pSession. If this is the first
** write to this table, initalize the SessionTable.nCol, azCol[] and
** abPK[] arrays accordingly.
**
** If an error occurs, an error code is stored in sqlite3_session.rc and
** non-zero returned. Or, if no error occurs but the table has no primary
** key, sqlite3_session.rc is left set to SQLITE_OK and non-zero returned to
** indicate that updates on this table should be ignored. SessionTable.abPK 
** is set to NULL in this case.
*/
static int sessionInitTable(sqlite3_session *pSession, SessionTable *pTab){
  if( pTab->nCol==0 ){
    u8 *abPK;
    assert( pTab->azCol==0 || pTab->abPK==0 );
    pSession->rc = sessionTableInfo(pSession->db, pSession->zDb, 
        pTab->zName, &pTab->nCol, 0, &pTab->azCol, &abPK
    );
    if( pSession->rc==SQLITE_OK ){
      int i;
      for(i=0; i<pTab->nCol; i++){
        if( abPK[i] ){
          pTab->abPK = abPK;
          break;
        }
      }
      if( 0==sqlite3_stricmp("sqlite_stat1", pTab->zName) ){
        pTab->bStat1 = 1;
      }
    }
  }
  return (pSession->rc || pTab->abPK==0);
}

/*
** Versions of the four methods in object SessionHook for use with the
** sqlite_stat1 table. The purpose of this is to substitute a zero-length
** blob each time a NULL value is read from the "idx" column of the
** sqlite_stat1 table.
*/
typedef struct SessionStat1Ctx SessionStat1Ctx;
struct SessionStat1Ctx {
  SessionHook hook;
  sqlite3_session *pSession;
};
static int sessionStat1Old(void *pCtx, int iCol, sqlite3_value **ppVal){
  SessionStat1Ctx *p = (SessionStat1Ctx*)pCtx;
  sqlite3_value *pVal = 0;
  int rc = p->hook.xOld(p->hook.pCtx, iCol, &pVal);
  if( rc==SQLITE_OK && iCol==1 && sqlite3_value_type(pVal)==SQLITE_NULL ){
    pVal = p->pSession->pZeroBlob;
  }
  *ppVal = pVal;
  return rc;
}
static int sessionStat1New(void *pCtx, int iCol, sqlite3_value **ppVal){
  SessionStat1Ctx *p = (SessionStat1Ctx*)pCtx;
  sqlite3_value *pVal = 0;
  int rc = p->hook.xNew(p->hook.pCtx, iCol, &pVal);
  if( rc==SQLITE_OK && iCol==1 && sqlite3_value_type(pVal)==SQLITE_NULL ){
    pVal = p->pSession->pZeroBlob;
  }
  *ppVal = pVal;
  return rc;
}
static int sessionStat1Count(void *pCtx){
  SessionStat1Ctx *p = (SessionStat1Ctx*)pCtx;
  return p->hook.xCount(p->hook.pCtx);
}
static int sessionStat1Depth(void *pCtx){
  SessionStat1Ctx *p = (SessionStat1Ctx*)pCtx;
  return p->hook.xDepth(p->hook.pCtx);
}


/*
** This function is only called from with a pre-update-hook reporting a 
** change on table pTab (attached to session pSession). The type of change
** (UPDATE, INSERT, DELETE) is specified by the first argument.
**
** Unless one is already present or an error occurs, an entry is added
** to the changed-rows hash table associated with table pTab.
*/
static void sessionPreupdateOneChange(
  int op,                         /* One of SQLITE_UPDATE, INSERT, DELETE */
  sqlite3_session *pSession,      /* Session object pTab is attached to */
  SessionTable *pTab              /* Table that change applies to */
){
  int iHash; 
  int bNull = 0; 
  int rc = SQLITE_OK;
  SessionStat1Ctx stat1 = {{0,0,0,0,0},0};

  if( pSession->rc ) return;

  /* Load table details if required */
  if( sessionInitTable(pSession, pTab) ) return;

  /* Check the number of columns in this xPreUpdate call matches the 
  ** number of columns in the table.  */
  if( pTab->nCol!=pSession->hook.xCount(pSession->hook.pCtx) ){
    pSession->rc = SQLITE_SCHEMA;
    return;
  }

  /* Grow the hash table if required */
  if( sessionGrowHash(0, pTab) ){
    pSession->rc = SQLITE_NOMEM;
    return;
  }

  if( pTab->bStat1 ){
    stat1.hook = pSession->hook;
    stat1.pSession = pSession;
    pSession->hook.pCtx = (void*)&stat1;
    pSession->hook.xNew = sessionStat1New;
    pSession->hook.xOld = sessionStat1Old;
    pSession->hook.xCount = sessionStat1Count;
    pSession->hook.xDepth = sessionStat1Depth;
    if( pSession->pZeroBlob==0 ){
      sqlite3_value *p = sqlite3ValueNew(0);
      if( p==0 ){
        rc = SQLITE_NOMEM;
        goto error_out;
      }
      sqlite3ValueSetStr(p, 0, "", 0, SQLITE_STATIC);
      pSession->pZeroBlob = p;
    }
  }

  /* Calculate the hash-key for this change. If the primary key of the row
  ** includes a NULL value, exit early. Such changes are ignored by the
  ** session module. */
  rc = sessionPreupdateHash(pSession, pTab, op==SQLITE_INSERT, &iHash, &bNull);
  if( rc!=SQLITE_OK ) goto error_out;

  if( bNull==0 ){
    /* Search the hash table for an existing record for this row. */
    SessionChange *pC;
    for(pC=pTab->apChange[iHash]; pC; pC=pC->pNext){
      if( sessionPreupdateEqual(pSession, pTab, pC, op) ) break;
    }

    if( pC==0 ){
      /* Create a new change object containing all the old values (if
      ** this is an SQLITE_UPDATE or SQLITE_DELETE), or just the PK
      ** values (if this is an INSERT). */
      SessionChange *pChange; /* New change object */
      sqlite3_int64 nByte;    /* Number of bytes to allocate */
      int i;                  /* Used to iterate through columns */
  
      assert( rc==SQLITE_OK );
      pTab->nEntry++;
  
      /* Figure out how large an allocation is required */
      nByte = sizeof(SessionChange);
      for(i=0; i<pTab->nCol; i++){
        sqlite3_value *p = 0;
        if( op!=SQLITE_INSERT ){
          TESTONLY(int trc = ) pSession->hook.xOld(pSession->hook.pCtx, i, &p);
          assert( trc==SQLITE_OK );
        }else if( pTab->abPK[i] ){
          TESTONLY(int trc = ) pSession->hook.xNew(pSession->hook.pCtx, i, &p);
          assert( trc==SQLITE_OK );
        }

        /* This may fail if SQLite value p contains a utf-16 string that must
        ** be converted to utf-8 and an OOM error occurs while doing so. */
        rc = sessionSerializeValue(0, p, &nByte);
        if( rc!=SQLITE_OK ) goto error_out;
      }
  
      /* Allocate the change object */
      pChange = (SessionChange *)sqlite3_malloc64(nByte);
      if( !pChange ){
        rc = SQLITE_NOMEM;
        goto error_out;
      }else{
        memset(pChange, 0, sizeof(SessionChange));
        pChange->aRecord = (u8 *)&pChange[1];
      }
  
      /* Populate the change object. None of the preupdate_old(),
      ** preupdate_new() or SerializeValue() calls below may fail as all
      ** required values and encodings have already been cached in memory.
      ** It is not possible for an OOM to occur in this block. */
      nByte = 0;
      for(i=0; i<pTab->nCol; i++){
        sqlite3_value *p = 0;
        if( op!=SQLITE_INSERT ){
          pSession->hook.xOld(pSession->hook.pCtx, i, &p);
        }else if( pTab->abPK[i] ){
          pSession->hook.xNew(pSession->hook.pCtx, i, &p);
        }
        sessionSerializeValue(&pChange->aRecord[nByte], p, &nByte);
      }

      /* Add the change to the hash-table */
      if( pSession->bIndirect || pSession->hook.xDepth(pSession->hook.pCtx) ){
        pChange->bIndirect = 1;
      }
      pChange->nRecord = nByte;
      pChange->op = op;
      pChange->pNext = pTab->apChange[iHash];
      pTab->apChange[iHash] = pChange;

    }else if( pC->bIndirect ){
      /* If the existing change is considered "indirect", but this current
      ** change is "direct", mark the change object as direct. */
      if( pSession->hook.xDepth(pSession->hook.pCtx)==0 
       && pSession->bIndirect==0 
      ){
        pC->bIndirect = 0;
      }
    }
  }

  /* If an error has occurred, mark the session object as failed. */
 error_out:
  if( pTab->bStat1 ){
    pSession->hook = stat1.hook;
  }
  if( rc!=SQLITE_OK ){
    pSession->rc = rc;
  }
}

static int sessionFindTable(
  sqlite3_session *pSession, 
  const char *zName,
  SessionTable **ppTab
){
  int rc = SQLITE_OK;
  int nName = sqlite3Strlen30(zName);
  SessionTable *pRet;

  /* Search for an existing table */
  for(pRet=pSession->pTable; pRet; pRet=pRet->pNext){
    if( 0==sqlite3_strnicmp(pRet->zName, zName, nName+1) ) break;
  }

  if( pRet==0 && pSession->bAutoAttach ){
    /* If there is a table-filter configured, invoke it. If it returns 0,
    ** do not automatically add the new table. */
    if( pSession->xTableFilter==0
     || pSession->xTableFilter(pSession->pFilterCtx, zName) 
    ){
      rc = sqlite3session_attach(pSession, zName);
      if( rc==SQLITE_OK ){
        for(pRet=pSession->pTable; pRet->pNext; pRet=pRet->pNext);
        assert( 0==sqlite3_strnicmp(pRet->zName, zName, nName+1) );
      }
    }
  }

  assert( rc==SQLITE_OK || pRet==0 );
  *ppTab = pRet;
  return rc;
}

/*
** The 'pre-update' hook registered by this module with SQLite databases.
*/
static void xPreUpdate(
  void *pCtx,                     /* Copy of third arg to preupdate_hook() */
  sqlite3 *db,                    /* Database handle */
  int op,                         /* SQLITE_UPDATE, DELETE or INSERT */
  char const *zDb,                /* Database name */
  char const *zName,              /* Table name */
  sqlite3_int64 iKey1,            /* Rowid of row about to be deleted/updated */
  sqlite3_int64 iKey2             /* New rowid value (for a rowid UPDATE) */
){
  sqlite3_session *pSession;
  int nDb = sqlite3Strlen30(zDb);

  assert( sqlite3_mutex_held(db->mutex) );

  for(pSession=(sqlite3_session *)pCtx; pSession; pSession=pSession->pNext){
    SessionTable *pTab;

    /* If this session is attached to a different database ("main", "temp" 
    ** etc.), or if it is not currently enabled, there is nothing to do. Skip 
    ** to the next session object attached to this database. */
    if( pSession->bEnable==0 ) continue;
    if( pSession->rc ) continue;
    if( sqlite3_strnicmp(zDb, pSession->zDb, nDb+1) ) continue;

    pSession->rc = sessionFindTable(pSession, zName, &pTab);
    if( pTab ){
      assert( pSession->rc==SQLITE_OK );
      sessionPreupdateOneChange(op, pSession, pTab);
      if( op==SQLITE_UPDATE ){
        sessionPreupdateOneChange(SQLITE_INSERT, pSession, pTab);
      }
    }
  }
}

/*
** The pre-update hook implementations.
*/
static int sessionPreupdateOld(void *pCtx, int iVal, sqlite3_value **ppVal){
  return sqlite3_preupdate_old((sqlite3*)pCtx, iVal, ppVal);
}
static int sessionPreupdateNew(void *pCtx, int iVal, sqlite3_value **ppVal){
  return sqlite3_preupdate_new((sqlite3*)pCtx, iVal, ppVal);
}
static int sessionPreupdateCount(void *pCtx){
  return sqlite3_preupdate_count((sqlite3*)pCtx);
}
static int sessionPreupdateDepth(void *pCtx){
  return sqlite3_preupdate_depth((sqlite3*)pCtx);
}

/*
** Install the pre-update hooks on the session object passed as the only
** argument.
*/
static void sessionPreupdateHooks(
  sqlite3_session *pSession
){
  pSession->hook.pCtx = (void*)pSession->db;
  pSession->hook.xOld = sessionPreupdateOld;
  pSession->hook.xNew = sessionPreupdateNew;
  pSession->hook.xCount = sessionPreupdateCount;
  pSession->hook.xDepth = sessionPreupdateDepth;
}

typedef struct SessionDiffCtx SessionDiffCtx;
struct SessionDiffCtx {
  sqlite3_stmt *pStmt;
  int nOldOff;
};

/*
** The diff hook implementations.
*/
static int sessionDiffOld(void *pCtx, int iVal, sqlite3_value **ppVal){
  SessionDiffCtx *p = (SessionDiffCtx*)pCtx;
  *ppVal = sqlite3_column_value(p->pStmt, iVal+p->nOldOff);
  return SQLITE_OK;
}
static int sessionDiffNew(void *pCtx, int iVal, sqlite3_value **ppVal){
  SessionDiffCtx *p = (SessionDiffCtx*)pCtx;
  *ppVal = sqlite3_column_value(p->pStmt, iVal);
   return SQLITE_OK;
}
static int sessionDiffCount(void *pCtx){
  SessionDiffCtx *p = (SessionDiffCtx*)pCtx;
  return p->nOldOff ? p->nOldOff : sqlite3_column_count(p->pStmt);
}
static int sessionDiffDepth(void *pCtx){
  return 0;
}

/*
** Install the diff hooks on the session object passed as the only
** argument.
*/
static void sessionDiffHooks(
  sqlite3_session *pSession,
  SessionDiffCtx *pDiffCtx
){
  pSession->hook.pCtx = (void*)pDiffCtx;
  pSession->hook.xOld = sessionDiffOld;
  pSession->hook.xNew = sessionDiffNew;
  pSession->hook.xCount = sessionDiffCount;
  pSession->hook.xDepth = sessionDiffDepth;
}

static char *sessionExprComparePK(
  int nCol,
  const char *zDb1, const char *zDb2, 
  const char *zTab,
  const char **azCol, u8 *abPK
){
  int i;
  const char *zSep = "";
  char *zRet = 0;

  for(i=0; i<nCol; i++){
    if( abPK[i] ){
      zRet = sqlite3_mprintf("%z%s\"%w\".\"%w\".\"%w\"=\"%w\".\"%w\".\"%w\"",
          zRet, zSep, zDb1, zTab, azCol[i], zDb2, zTab, azCol[i]
      );
      zSep = " AND ";
      if( zRet==0 ) break;
    }
  }

  return zRet;
}

static char *sessionExprCompareOther(
  int nCol,
  const char *zDb1, const char *zDb2, 
  const char *zTab,
  const char **azCol, u8 *abPK
){
  int i;
  const char *zSep = "";
  char *zRet = 0;
  int bHave = 0;

  for(i=0; i<nCol; i++){
    if( abPK[i]==0 ){
      bHave = 1;
      zRet = sqlite3_mprintf(
          "%z%s\"%w\".\"%w\".\"%w\" IS NOT \"%w\".\"%w\".\"%w\"",
          zRet, zSep, zDb1, zTab, azCol[i], zDb2, zTab, azCol[i]
      );
      zSep = " OR ";
      if( zRet==0 ) break;
    }
  }

  if( bHave==0 ){
    assert( zRet==0 );
    zRet = sqlite3_mprintf("0");
  }

  return zRet;
}

static char *sessionSelectFindNew(
  int nCol,
  const char *zDb1,      /* Pick rows in this db only */
  const char *zDb2,      /* But not in this one */
  const char *zTbl,      /* Table name */
  const char *zExpr
){
  char *zRet = sqlite3_mprintf(
      "SELECT * FROM \"%w\".\"%w\" WHERE NOT EXISTS ("
      "  SELECT 1 FROM \"%w\".\"%w\" WHERE %s"
      ")",
      zDb1, zTbl, zDb2, zTbl, zExpr
  );
  return zRet;
}

static int sessionDiffFindNew(
  int op,
  sqlite3_session *pSession,
  SessionTable *pTab,
  const char *zDb1,
  const char *zDb2,
  char *zExpr
){
  int rc = SQLITE_OK;
  char *zStmt = sessionSelectFindNew(pTab->nCol, zDb1, zDb2, pTab->zName,zExpr);

  if( zStmt==0 ){
    rc = SQLITE_NOMEM;
  }else{
    sqlite3_stmt *pStmt;
    rc = sqlite3_prepare(pSession->db, zStmt, -1, &pStmt, 0);
    if( rc==SQLITE_OK ){
      SessionDiffCtx *pDiffCtx = (SessionDiffCtx*)pSession->hook.pCtx;
      pDiffCtx->pStmt = pStmt;
      pDiffCtx->nOldOff = 0;
      while( SQLITE_ROW==sqlite3_step(pStmt) ){
        sessionPreupdateOneChange(op, pSession, pTab);
      }
      rc = sqlite3_finalize(pStmt);
    }
    sqlite3_free(zStmt);
  }

  return rc;
}

static int sessionDiffFindModified(
  sqlite3_session *pSession, 
  SessionTable *pTab, 
  const char *zFrom, 
  const char *zExpr
){
  int rc = SQLITE_OK;

  char *zExpr2 = sessionExprCompareOther(pTab->nCol,
      pSession->zDb, zFrom, pTab->zName, pTab->azCol, pTab->abPK
  );
  if( zExpr2==0 ){
    rc = SQLITE_NOMEM;
  }else{
    char *zStmt = sqlite3_mprintf(
        "SELECT * FROM \"%w\".\"%w\", \"%w\".\"%w\" WHERE %s AND (%z)",
        pSession->zDb, pTab->zName, zFrom, pTab->zName, zExpr, zExpr2
    );
    if( zStmt==0 ){
      rc = SQLITE_NOMEM;
    }else{
      sqlite3_stmt *pStmt;
      rc = sqlite3_prepare(pSession->db, zStmt, -1, &pStmt, 0);

      if( rc==SQLITE_OK ){
        SessionDiffCtx *pDiffCtx = (SessionDiffCtx*)pSession->hook.pCtx;
        pDiffCtx->pStmt = pStmt;
        pDiffCtx->nOldOff = pTab->nCol;
        while( SQLITE_ROW==sqlite3_step(pStmt) ){
          sessionPreupdateOneChange(SQLITE_UPDATE, pSession, pTab);
        }
        rc = sqlite3_finalize(pStmt);
      }
      sqlite3_free(zStmt);
    }
  }

  return rc;
}

SQLITE_API int sqlite3session_diff(
  sqlite3_session *pSession,
  const char *zFrom,
  const char *zTbl,
  char **pzErrMsg
){
  const char *zDb = pSession->zDb;
  int rc = pSession->rc;
  SessionDiffCtx d;

  memset(&d, 0, sizeof(d));
  sessionDiffHooks(pSession, &d);

  sqlite3_mutex_enter(sqlite3_db_mutex(pSession->db));
  if( pzErrMsg ) *pzErrMsg = 0;
  if( rc==SQLITE_OK ){
    char *zExpr = 0;
    sqlite3 *db = pSession->db;
    SessionTable *pTo;            /* Table zTbl */

    /* Locate and if necessary initialize the target table object */
    rc = sessionFindTable(pSession, zTbl, &pTo);
    if( pTo==0 ) goto diff_out;
    if( sessionInitTable(pSession, pTo) ){
      rc = pSession->rc;
      goto diff_out;
    }

    /* Check the table schemas match */
    if( rc==SQLITE_OK ){
      int bHasPk = 0;
      int bMismatch = 0;
      int nCol;                   /* Columns in zFrom.zTbl */
      u8 *abPK;
      const char **azCol = 0;
      rc = sessionTableInfo(db, zFrom, zTbl, &nCol, 0, &azCol, &abPK);
      if( rc==SQLITE_OK ){
        if( pTo->nCol!=nCol ){
          bMismatch = 1;
        }else{
          int i;
          for(i=0; i<nCol; i++){
            if( pTo->abPK[i]!=abPK[i] ) bMismatch = 1;
            if( sqlite3_stricmp(azCol[i], pTo->azCol[i]) ) bMismatch = 1;
            if( abPK[i] ) bHasPk = 1;
          }
        }
      }
      sqlite3_free((char*)azCol);
      if( bMismatch ){
        if( pzErrMsg ){
          *pzErrMsg = sqlite3_mprintf("table schemas do not match");
        }
        rc = SQLITE_SCHEMA;
      }
      if( bHasPk==0 ){
        /* Ignore tables with no primary keys */
        goto diff_out;
      }
    }

    if( rc==SQLITE_OK ){
      zExpr = sessionExprComparePK(pTo->nCol, 
          zDb, zFrom, pTo->zName, pTo->azCol, pTo->abPK
      );
    }

    /* Find new rows */
    if( rc==SQLITE_OK ){
      rc = sessionDiffFindNew(SQLITE_INSERT, pSession, pTo, zDb, zFrom, zExpr);
    }

    /* Find old rows */
    if( rc==SQLITE_OK ){
      rc = sessionDiffFindNew(SQLITE_DELETE, pSession, pTo, zFrom, zDb, zExpr);
    }

    /* Find modified rows */
    if( rc==SQLITE_OK ){
      rc = sessionDiffFindModified(pSession, pTo, zFrom, zExpr);
    }

    sqlite3_free(zExpr);
  }

 diff_out:
  sessionPreupdateHooks(pSession);
  sqlite3_mutex_leave(sqlite3_db_mutex(pSession->db));
  return rc;
}

/*
** Create a session object. This session object will record changes to
** database zDb attached to connection db.
*/
SQLITE_API int sqlite3session_create(
  sqlite3 *db,                    /* Database handle */
  const char *zDb,                /* Name of db (e.g. "main") */
  sqlite3_session **ppSession     /* OUT: New session object */
){
  sqlite3_session *pNew;          /* Newly allocated session object */
  sqlite3_session *pOld;          /* Session object already attached to db */
  int nDb = sqlite3Strlen30(zDb); /* Length of zDb in bytes */

  /* Zero the output value in case an error occurs. */
  *ppSession = 0;

  /* Allocate and populate the new session object. */
  pNew = (sqlite3_session *)sqlite3_malloc64(sizeof(sqlite3_session) + nDb + 1);
  if( !pNew ) return SQLITE_NOMEM;
  memset(pNew, 0, sizeof(sqlite3_session));
  pNew->db = db;
  pNew->zDb = (char *)&pNew[1];
  pNew->bEnable = 1;
  memcpy(pNew->zDb, zDb, nDb+1);
  sessionPreupdateHooks(pNew);

  /* Add the new session object to the linked list of session objects 
  ** attached to database handle $db. Do this under the cover of the db
  ** handle mutex.  */
  sqlite3_mutex_enter(sqlite3_db_mutex(db));
  pOld = (sqlite3_session*)sqlite3_preupdate_hook(db, xPreUpdate, (void*)pNew);
  pNew->pNext = pOld;
  sqlite3_mutex_leave(sqlite3_db_mutex(db));

  *ppSession = pNew;
  return SQLITE_OK;
}

/*
** Free the list of table objects passed as the first argument. The contents
** of the changed-rows hash tables are also deleted.
*/
static void sessionDeleteTable(SessionTable *pList){
  SessionTable *pNext;
  SessionTable *pTab;

  for(pTab=pList; pTab; pTab=pNext){
    int i;
    pNext = pTab->pNext;
    for(i=0; i<pTab->nChange; i++){
      SessionChange *p;
      SessionChange *pNextChange;
      for(p=pTab->apChange[i]; p; p=pNextChange){
        pNextChange = p->pNext;
        sqlite3_free(p);
      }
    }
    sqlite3_free((char*)pTab->azCol);  /* cast works around VC++ bug */
    sqlite3_free(pTab->apChange);
    sqlite3_free(pTab);
  }
}

/*
** Delete a session object previously allocated using sqlite3session_create().
*/
SQLITE_API void sqlite3session_delete(sqlite3_session *pSession){
  sqlite3 *db = pSession->db;
  sqlite3_session *pHead;
  sqlite3_session **pp;

  /* Unlink the session from the linked list of sessions attached to the
  ** database handle. Hold the db mutex while doing so.  */
  sqlite3_mutex_enter(sqlite3_db_mutex(db));
  pHead = (sqlite3_session*)sqlite3_preupdate_hook(db, 0, 0);
  for(pp=&pHead; ALWAYS((*pp)!=0); pp=&((*pp)->pNext)){
    if( (*pp)==pSession ){
      *pp = (*pp)->pNext;
      if( pHead ) sqlite3_preupdate_hook(db, xPreUpdate, (void*)pHead);
      break;
    }
  }
  sqlite3_mutex_leave(sqlite3_db_mutex(db));
  sqlite3ValueFree(pSession->pZeroBlob);

  /* Delete all attached table objects. And the contents of their 
  ** associated hash-tables. */
  sessionDeleteTable(pSession->pTable);

  /* Free the session object itself. */
  sqlite3_free(pSession);
}

/*
** Set a table filter on a Session Object.
*/
SQLITE_API void sqlite3session_table_filter(
  sqlite3_session *pSession, 
  int(*xFilter)(void*, const char*),
  void *pCtx                      /* First argument passed to xFilter */
){
  pSession->bAutoAttach = 1;
  pSession->pFilterCtx = pCtx;
  pSession->xTableFilter = xFilter;
}

/*
** Attach a table to a session. All subsequent changes made to the table
** while the session object is enabled will be recorded.
**
** Only tables that have a PRIMARY KEY defined may be attached. It does
** not matter if the PRIMARY KEY is an "INTEGER PRIMARY KEY" (rowid alias)
** or not.
*/
SQLITE_API int sqlite3session_attach(
  sqlite3_session *pSession,      /* Session object */
  const char *zName               /* Table name */
){
  int rc = SQLITE_OK;
  sqlite3_mutex_enter(sqlite3_db_mutex(pSession->db));

  if( !zName ){
    pSession->bAutoAttach = 1;
  }else{
    SessionTable *pTab;           /* New table object (if required) */
    int nName;                    /* Number of bytes in string zName */

    /* First search for an existing entry. If one is found, this call is
    ** a no-op. Return early. */
    nName = sqlite3Strlen30(zName);
    for(pTab=pSession->pTable; pTab; pTab=pTab->pNext){
      if( 0==sqlite3_strnicmp(pTab->zName, zName, nName+1) ) break;
    }

    if( !pTab ){
      /* Allocate new SessionTable object. */
      pTab = (SessionTable *)sqlite3_malloc64(sizeof(SessionTable) + nName + 1);
      if( !pTab ){
        rc = SQLITE_NOMEM;
      }else{
        /* Populate the new SessionTable object and link it into the list.
        ** The new object must be linked onto the end of the list, not 
        ** simply added to the start of it in order to ensure that tables
        ** appear in the correct order when a changeset or patchset is
        ** eventually generated. */
        SessionTable **ppTab;
        memset(pTab, 0, sizeof(SessionTable));
        pTab->zName = (char *)&pTab[1];
        memcpy(pTab->zName, zName, nName+1);
        for(ppTab=&pSession->pTable; *ppTab; ppTab=&(*ppTab)->pNext);
        *ppTab = pTab;
      }
    }
  }

  sqlite3_mutex_leave(sqlite3_db_mutex(pSession->db));
  return rc;
}

/*
** Ensure that there is room in the buffer to append nByte bytes of data.
** If not, use sqlite3_realloc() to grow the buffer so that there is.
**
** If successful, return zero. Otherwise, if an OOM condition is encountered,
** set *pRc to SQLITE_NOMEM and return non-zero.
*/
static int sessionBufferGrow(SessionBuffer *p, size_t nByte, int *pRc){
  if( *pRc==SQLITE_OK && (size_t)(p->nAlloc-p->nBuf)<nByte ){
    u8 *aNew;
    i64 nNew = p->nAlloc ? p->nAlloc : 128;
    do {
      nNew = nNew*2;
    }while( (size_t)(nNew-p->nBuf)<nByte );

    aNew = (u8 *)sqlite3_realloc64(p->aBuf, nNew);
    if( 0==aNew ){
      *pRc = SQLITE_NOMEM;
    }else{
      p->aBuf = aNew;
      p->nAlloc = nNew;
    }
  }
  return (*pRc!=SQLITE_OK);
}

/*
** Append the value passed as the second argument to the buffer passed
** as the first.
**
** This function is a no-op if *pRc is non-zero when it is called.
** Otherwise, if an error occurs, *pRc is set to an SQLite error code
** before returning.
*/
static void sessionAppendValue(SessionBuffer *p, sqlite3_value *pVal, int *pRc){
  int rc = *pRc;
  if( rc==SQLITE_OK ){
    sqlite3_int64 nByte = 0;
    rc = sessionSerializeValue(0, pVal, &nByte);
    sessionBufferGrow(p, nByte, &rc);
    if( rc==SQLITE_OK ){
      rc = sessionSerializeValue(&p->aBuf[p->nBuf], pVal, 0);
      p->nBuf += nByte;
    }else{
      *pRc = rc;
    }
  }
}

/*
** This function is a no-op if *pRc is other than SQLITE_OK when it is 
** called. Otherwise, append a single byte to the buffer. 
**
** If an OOM condition is encountered, set *pRc to SQLITE_NOMEM before
** returning.
*/
static void sessionAppendByte(SessionBuffer *p, u8 v, int *pRc){
  if( 0==sessionBufferGrow(p, 1, pRc) ){
    p->aBuf[p->nBuf++] = v;
  }
}

/*
** This function is a no-op if *pRc is other than SQLITE_OK when it is 
** called. Otherwise, append a single varint to the buffer. 
**
** If an OOM condition is encountered, set *pRc to SQLITE_NOMEM before
** returning.
*/
static void sessionAppendVarint(SessionBuffer *p, int v, int *pRc){
  if( 0==sessionBufferGrow(p, 9, pRc) ){
    p->nBuf += sessionVarintPut(&p->aBuf[p->nBuf], v);
  }
}

/*
** This function is a no-op if *pRc is other than SQLITE_OK when it is 
** called. Otherwise, append a blob of data to the buffer. 
**
** If an OOM condition is encountered, set *pRc to SQLITE_NOMEM before
** returning.
*/
static void sessionAppendBlob(
  SessionBuffer *p, 
  const u8 *aBlob, 
  int nBlob, 
  int *pRc
){
  if( nBlob>0 && 0==sessionBufferGrow(p, nBlob, pRc) ){
    memcpy(&p->aBuf[p->nBuf], aBlob, nBlob);
    p->nBuf += nBlob;
  }
}

/*
** This function is a no-op if *pRc is other than SQLITE_OK when it is 
** called. Otherwise, append a string to the buffer. All bytes in the string
** up to (but not including) the nul-terminator are written to the buffer.
**
** If an OOM condition is encountered, set *pRc to SQLITE_NOMEM before
** returning.
*/
static void sessionAppendStr(
  SessionBuffer *p, 
  const char *zStr, 
  int *pRc
){
  int nStr = sqlite3Strlen30(zStr);
  if( 0==sessionBufferGrow(p, nStr, pRc) ){
    memcpy(&p->aBuf[p->nBuf], zStr, nStr);
    p->nBuf += nStr;
  }
}

/*
** This function is a no-op if *pRc is other than SQLITE_OK when it is 
** called. Otherwise, append the string representation of integer iVal
** to the buffer. No nul-terminator is written.
**
** If an OOM condition is encountered, set *pRc to SQLITE_NOMEM before
** returning.
*/
static void sessionAppendInteger(
  SessionBuffer *p,               /* Buffer to append to */
  int iVal,                       /* Value to write the string rep. of */
  int *pRc                        /* IN/OUT: Error code */
){
  char aBuf[24];
  sqlite3_snprintf(sizeof(aBuf)-1, aBuf, "%d", iVal);
  sessionAppendStr(p, aBuf, pRc);
}

/*
** This function is a no-op if *pRc is other than SQLITE_OK when it is 
** called. Otherwise, append the string zStr enclosed in quotes (") and
** with any embedded quote characters escaped to the buffer. No 
** nul-terminator byte is written.
**
** If an OOM condition is encountered, set *pRc to SQLITE_NOMEM before
** returning.
*/
static void sessionAppendIdent(
  SessionBuffer *p,               /* Buffer to a append to */
  const char *zStr,               /* String to quote, escape and append */
  int *pRc                        /* IN/OUT: Error code */
){
  int nStr = sqlite3Strlen30(zStr)*2 + 2 + 1;
  if( 0==sessionBufferGrow(p, nStr, pRc) ){
    char *zOut = (char *)&p->aBuf[p->nBuf];
    const char *zIn = zStr;
    *zOut++ = '"';
    while( *zIn ){
      if( *zIn=='"' ) *zOut++ = '"';
      *zOut++ = *(zIn++);
    }
    *zOut++ = '"';
    p->nBuf = (int)((u8 *)zOut - p->aBuf);
  }
}

/*
** This function is a no-op if *pRc is other than SQLITE_OK when it is
** called. Otherwse, it appends the serialized version of the value stored
** in column iCol of the row that SQL statement pStmt currently points
** to to the buffer.
*/
static void sessionAppendCol(
  SessionBuffer *p,               /* Buffer to append to */
  sqlite3_stmt *pStmt,            /* Handle pointing to row containing value */
  int iCol,                       /* Column to read value from */
  int *pRc                        /* IN/OUT: Error code */
){
  if( *pRc==SQLITE_OK ){
    int eType = sqlite3_column_type(pStmt, iCol);
    sessionAppendByte(p, (u8)eType, pRc);
    if( eType==SQLITE_INTEGER || eType==SQLITE_FLOAT ){
      sqlite3_int64 i;
      u8 aBuf[8];
      if( eType==SQLITE_INTEGER ){
        i = sqlite3_column_int64(pStmt, iCol);
      }else{
        double r = sqlite3_column_double(pStmt, iCol);
        memcpy(&i, &r, 8);
      }
      sessionPutI64(aBuf, i);
      sessionAppendBlob(p, aBuf, 8, pRc);
    }
    if( eType==SQLITE_BLOB || eType==SQLITE_TEXT ){
      u8 *z;
      int nByte;
      if( eType==SQLITE_BLOB ){
        z = (u8 *)sqlite3_column_blob(pStmt, iCol);
      }else{
        z = (u8 *)sqlite3_column_text(pStmt, iCol);
      }
      nByte = sqlite3_column_bytes(pStmt, iCol);
      if( z || (eType==SQLITE_BLOB && nByte==0) ){
        sessionAppendVarint(p, nByte, pRc);
        sessionAppendBlob(p, z, nByte, pRc);
      }else{
        *pRc = SQLITE_NOMEM;
      }
    }
  }
}

/*
**
** This function appends an update change to the buffer (see the comments 
** under "CHANGESET FORMAT" at the top of the file). An update change 
** consists of:
**
**   1 byte:  SQLITE_UPDATE (0x17)
**   n bytes: old.* record (see RECORD FORMAT)
**   m bytes: new.* record (see RECORD FORMAT)
**
** The SessionChange object passed as the third argument contains the
** values that were stored in the row when the session began (the old.*
** values). The statement handle passed as the second argument points
** at the current version of the row (the new.* values).
**
** If all of the old.* values are equal to their corresponding new.* value
** (i.e. nothing has changed), then no data at all is appended to the buffer.
**
** Otherwise, the old.* record contains all primary key values and the 
** original values of any fields that have been modified. The new.* record 
** contains the new values of only those fields that have been modified.
*/ 
static int sessionAppendUpdate(
  SessionBuffer *pBuf,            /* Buffer to append to */
  int bPatchset,                  /* True for "patchset", 0 for "changeset" */
  sqlite3_stmt *pStmt,            /* Statement handle pointing at new row */
  SessionChange *p,               /* Object containing old values */
  u8 *abPK                        /* Boolean array - true for PK columns */
){
  int rc = SQLITE_OK;
  SessionBuffer buf2 = {0,0,0}; /* Buffer to accumulate new.* record in */
  int bNoop = 1;                /* Set to zero if any values are modified */
  int nRewind = pBuf->nBuf;     /* Set to zero if any values are modified */
  int i;                        /* Used to iterate through columns */
  u8 *pCsr = p->aRecord;        /* Used to iterate through old.* values */

  sessionAppendByte(pBuf, SQLITE_UPDATE, &rc);
  sessionAppendByte(pBuf, p->bIndirect, &rc);
  for(i=0; i<sqlite3_column_count(pStmt); i++){
    int bChanged = 0;
    int nAdvance;
    int eType = *pCsr;
    switch( eType ){
      case SQLITE_NULL:
        nAdvance = 1;
        if( sqlite3_column_type(pStmt, i)!=SQLITE_NULL ){
          bChanged = 1;
        }
        break;

      case SQLITE_FLOAT:
      case SQLITE_INTEGER: {
        nAdvance = 9;
        if( eType==sqlite3_column_type(pStmt, i) ){
          sqlite3_int64 iVal = sessionGetI64(&pCsr[1]);
          if( eType==SQLITE_INTEGER ){
            if( iVal==sqlite3_column_int64(pStmt, i) ) break;
          }else{
            double dVal;
            memcpy(&dVal, &iVal, 8);
            if( dVal==sqlite3_column_double(pStmt, i) ) break;
          }
        }
        bChanged = 1;
        break;
      }

      default: {
        int n;
        int nHdr = 1 + sessionVarintGet(&pCsr[1], &n);
        assert( eType==SQLITE_TEXT || eType==SQLITE_BLOB );
        nAdvance = nHdr + n;
        if( eType==sqlite3_column_type(pStmt, i) 
         && n==sqlite3_column_bytes(pStmt, i) 
         && (n==0 || 0==memcmp(&pCsr[nHdr], sqlite3_column_blob(pStmt, i), n))
        ){
          break;
        }
        bChanged = 1;
      }
    }

    /* If at least one field has been modified, this is not a no-op. */
    if( bChanged ) bNoop = 0;

    /* Add a field to the old.* record. This is omitted if this modules is
    ** currently generating a patchset. */
    if( bPatchset==0 ){
      if( bChanged || abPK[i] ){
        sessionAppendBlob(pBuf, pCsr, nAdvance, &rc);
      }else{
        sessionAppendByte(pBuf, 0, &rc);
      }
    }

    /* Add a field to the new.* record. Or the only record if currently
    ** generating a patchset.  */
    if( bChanged || (bPatchset && abPK[i]) ){
      sessionAppendCol(&buf2, pStmt, i, &rc);
    }else{
      sessionAppendByte(&buf2, 0, &rc);
    }

    pCsr += nAdvance;
  }

  if( bNoop ){
    pBuf->nBuf = nRewind;
  }else{
    sessionAppendBlob(pBuf, buf2.aBuf, buf2.nBuf, &rc);
  }
  sqlite3_free(buf2.aBuf);

  return rc;
}

/*
** Append a DELETE change to the buffer passed as the first argument. Use
** the changeset format if argument bPatchset is zero, or the patchset
** format otherwise.
*/
static int sessionAppendDelete(
  SessionBuffer *pBuf,            /* Buffer to append to */
  int bPatchset,                  /* True for "patchset", 0 for "changeset" */
  SessionChange *p,               /* Object containing old values */
  int nCol,                       /* Number of columns in table */
  u8 *abPK                        /* Boolean array - true for PK columns */
){
  int rc = SQLITE_OK;

  sessionAppendByte(pBuf, SQLITE_DELETE, &rc);
  sessionAppendByte(pBuf, p->bIndirect, &rc);

  if( bPatchset==0 ){
    sessionAppendBlob(pBuf, p->aRecord, p->nRecord, &rc);
  }else{
    int i;
    u8 *a = p->aRecord;
    for(i=0; i<nCol; i++){
      u8 *pStart = a;
      int eType = *a++;

      switch( eType ){
        case 0:
        case SQLITE_NULL:
          assert( abPK[i]==0 );
          break;

        case SQLITE_FLOAT:
        case SQLITE_INTEGER:
          a += 8;
          break;

        default: {
          int n;
          a += sessionVarintGet(a, &n);
          a += n;
          break;
        }
      }
      if( abPK[i] ){
        sessionAppendBlob(pBuf, pStart, (int)(a-pStart), &rc);
      }
    }
    assert( (a - p->aRecord)==p->nRecord );
  }

  return rc;
}

/*
** Formulate and prepare a SELECT statement to retrieve a row from table
** zTab in database zDb based on its primary key. i.e.
**
**   SELECT * FROM zDb.zTab WHERE pk1 = ? AND pk2 = ? AND ...
*/
static int sessionSelectStmt(
  sqlite3 *db,                    /* Database handle */
  const char *zDb,                /* Database name */
  const char *zTab,               /* Table name */
  int nCol,                       /* Number of columns in table */
  const char **azCol,             /* Names of table columns */
  u8 *abPK,                       /* PRIMARY KEY  array */
  sqlite3_stmt **ppStmt           /* OUT: Prepared SELECT statement */
){
  int rc = SQLITE_OK;
  char *zSql = 0;
  int nSql = -1;

  if( 0==sqlite3_stricmp("sqlite_stat1", zTab) ){
    zSql = sqlite3_mprintf(
        "SELECT tbl, ?2, stat FROM %Q.sqlite_stat1 WHERE tbl IS ?1 AND "
        "idx IS (CASE WHEN ?2=X'' THEN NULL ELSE ?2 END)", zDb
    );
    if( zSql==0 ) rc = SQLITE_NOMEM;
  }else{
    int i;
    const char *zSep = "";
    SessionBuffer buf = {0, 0, 0};

    sessionAppendStr(&buf, "SELECT * FROM ", &rc);
    sessionAppendIdent(&buf, zDb, &rc);
    sessionAppendStr(&buf, ".", &rc);
    sessionAppendIdent(&buf, zTab, &rc);
    sessionAppendStr(&buf, " WHERE ", &rc);
    for(i=0; i<nCol; i++){
      if( abPK[i] ){
        sessionAppendStr(&buf, zSep, &rc);
        sessionAppendIdent(&buf, azCol[i], &rc);
        sessionAppendStr(&buf, " IS ?", &rc);
        sessionAppendInteger(&buf, i+1, &rc);
        zSep = " AND ";
      }
    }
    zSql = (char*)buf.aBuf;
    nSql = buf.nBuf;
  }

  if( rc==SQLITE_OK ){
    rc = sqlite3_prepare_v2(db, zSql, nSql, ppStmt, 0);
  }
  sqlite3_free(zSql);
  return rc;
}

/*
** Bind the PRIMARY KEY values from the change passed in argument pChange
** to the SELECT statement passed as the first argument. The SELECT statement
** is as prepared by function sessionSelectStmt().
**
** Return SQLITE_OK if all PK values are successfully bound, or an SQLite
** error code (e.g. SQLITE_NOMEM) otherwise.
*/
static int sessionSelectBind(
  sqlite3_stmt *pSelect,          /* SELECT from sessionSelectStmt() */
  int nCol,                       /* Number of columns in table */
  u8 *abPK,                       /* PRIMARY KEY array */
  SessionChange *pChange          /* Change structure */
){
  int i;
  int rc = SQLITE_OK;
  u8 *a = pChange->aRecord;

  for(i=0; i<nCol && rc==SQLITE_OK; i++){
    int eType = *a++;

    switch( eType ){
      case 0:
      case SQLITE_NULL:
        assert( abPK[i]==0 );
        break;

      case SQLITE_INTEGER: {
        if( abPK[i] ){
          i64 iVal = sessionGetI64(a);
          rc = sqlite3_bind_int64(pSelect, i+1, iVal);
        }
        a += 8;
        break;
      }

      case SQLITE_FLOAT: {
        if( abPK[i] ){
          double rVal;
          i64 iVal = sessionGetI64(a);
          memcpy(&rVal, &iVal, 8);
          rc = sqlite3_bind_double(pSelect, i+1, rVal);
        }
        a += 8;
        break;
      }

      case SQLITE_TEXT: {
        int n;
        a += sessionVarintGet(a, &n);
        if( abPK[i] ){
          rc = sqlite3_bind_text(pSelect, i+1, (char *)a, n, SQLITE_TRANSIENT);
        }
        a += n;
        break;
      }

      default: {
        int n;
        assert( eType==SQLITE_BLOB );
        a += sessionVarintGet(a, &n);
        if( abPK[i] ){
          rc = sqlite3_bind_blob(pSelect, i+1, a, n, SQLITE_TRANSIENT);
        }
        a += n;
        break;
      }
    }
  }

  return rc;
}

/*
** This function is a no-op if *pRc is set to other than SQLITE_OK when it
** is called. Otherwise, append a serialized table header (part of the binary 
** changeset format) to buffer *pBuf. If an error occurs, set *pRc to an
** SQLite error code before returning.
*/
static void sessionAppendTableHdr(
  SessionBuffer *pBuf,            /* Append header to this buffer */
  int bPatchset,                  /* Use the patchset format if true */
  SessionTable *pTab,             /* Table object to append header for */
  int *pRc                        /* IN/OUT: Error code */
){
  /* Write a table header */
  sessionAppendByte(pBuf, (bPatchset ? 'P' : 'T'), pRc);
  sessionAppendVarint(pBuf, pTab->nCol, pRc);
  sessionAppendBlob(pBuf, pTab->abPK, pTab->nCol, pRc);
  sessionAppendBlob(pBuf, (u8 *)pTab->zName, (int)strlen(pTab->zName)+1, pRc);
}

/*
** Generate either a changeset (if argument bPatchset is zero) or a patchset
** (if it is non-zero) based on the current contents of the session object
** passed as the first argument.
**
** If no error occurs, SQLITE_OK is returned and the new changeset/patchset
** stored in output variables *pnChangeset and *ppChangeset. Or, if an error
** occurs, an SQLite error code is returned and both output variables set 
** to 0.
*/
static int sessionGenerateChangeset(
  sqlite3_session *pSession,      /* Session object */
  int bPatchset,                  /* True for patchset, false for changeset */
  int (*xOutput)(void *pOut, const void *pData, int nData),
  void *pOut,                     /* First argument for xOutput */
  int *pnChangeset,               /* OUT: Size of buffer at *ppChangeset */
  void **ppChangeset              /* OUT: Buffer containing changeset */
){
  sqlite3 *db = pSession->db;     /* Source database handle */
  SessionTable *pTab;             /* Used to iterate through attached tables */
  SessionBuffer buf = {0,0,0};    /* Buffer in which to accumlate changeset */
  int rc;                         /* Return code */

  assert( xOutput==0 || (pnChangeset==0 && ppChangeset==0 ) );

  /* Zero the output variables in case an error occurs. If this session
  ** object is already in the error state (sqlite3_session.rc != SQLITE_OK),
  ** this call will be a no-op.  */
  if( xOutput==0 ){
    *pnChangeset = 0;
    *ppChangeset = 0;
  }

  if( pSession->rc ) return pSession->rc;
  rc = sqlite3_exec(pSession->db, "SAVEPOINT changeset", 0, 0, 0);
  if( rc!=SQLITE_OK ) return rc;

  sqlite3_mutex_enter(sqlite3_db_mutex(db));

  for(pTab=pSession->pTable; rc==SQLITE_OK && pTab; pTab=pTab->pNext){
    if( pTab->nEntry ){
      const char *zName = pTab->zName;
      int nCol;                   /* Number of columns in table */
      u8 *abPK;                   /* Primary key array */
      const char **azCol = 0;     /* Table columns */
      int i;                      /* Used to iterate through hash buckets */
      sqlite3_stmt *pSel = 0;     /* SELECT statement to query table pTab */
      int nRewind = buf.nBuf;     /* Initial size of write buffer */
      int nNoop;                  /* Size of buffer after writing tbl header */

      /* Check the table schema is still Ok. */
      rc = sessionTableInfo(db, pSession->zDb, zName, &nCol, 0, &azCol, &abPK);
      if( !rc && (pTab->nCol!=nCol || memcmp(abPK, pTab->abPK, nCol)) ){
        rc = SQLITE_SCHEMA;
      }

      /* Write a table header */
      sessionAppendTableHdr(&buf, bPatchset, pTab, &rc);

      /* Build and compile a statement to execute: */
      if( rc==SQLITE_OK ){
        rc = sessionSelectStmt(
            db, pSession->zDb, zName, nCol, azCol, abPK, &pSel);
      }

      nNoop = buf.nBuf;
      for(i=0; i<pTab->nChange && rc==SQLITE_OK; i++){
        SessionChange *p;         /* Used to iterate through changes */

        for(p=pTab->apChange[i]; rc==SQLITE_OK && p; p=p->pNext){
          rc = sessionSelectBind(pSel, nCol, abPK, p);
          if( rc!=SQLITE_OK ) continue;
          if( sqlite3_step(pSel)==SQLITE_ROW ){
            if( p->op==SQLITE_INSERT ){
              int iCol;
              sessionAppendByte(&buf, SQLITE_INSERT, &rc);
              sessionAppendByte(&buf, p->bIndirect, &rc);
              for(iCol=0; iCol<nCol; iCol++){
                sessionAppendCol(&buf, pSel, iCol, &rc);
              }
            }else{
              rc = sessionAppendUpdate(&buf, bPatchset, pSel, p, abPK);
            }
          }else if( p->op!=SQLITE_INSERT ){
            rc = sessionAppendDelete(&buf, bPatchset, p, nCol, abPK);
          }
          if( rc==SQLITE_OK ){
            rc = sqlite3_reset(pSel);
          }

          /* If the buffer is now larger than sessions_strm_chunk_size, pass
          ** its contents to the xOutput() callback. */
          if( xOutput 
           && rc==SQLITE_OK 
           && buf.nBuf>nNoop 
           && buf.nBuf>sessions_strm_chunk_size 
          ){
            rc = xOutput(pOut, (void*)buf.aBuf, buf.nBuf);
            nNoop = -1;
            buf.nBuf = 0;
          }

        }
      }

      sqlite3_finalize(pSel);
      if( buf.nBuf==nNoop ){
        buf.nBuf = nRewind;
      }
      sqlite3_free((char*)azCol);  /* cast works around VC++ bug */
    }
  }

  if( rc==SQLITE_OK ){
    if( xOutput==0 ){
      *pnChangeset = buf.nBuf;
      *ppChangeset = buf.aBuf;
      buf.aBuf = 0;
    }else if( buf.nBuf>0 ){
      rc = xOutput(pOut, (void*)buf.aBuf, buf.nBuf);
    }
  }

  sqlite3_free(buf.aBuf);
  sqlite3_exec(db, "RELEASE changeset", 0, 0, 0);
  sqlite3_mutex_leave(sqlite3_db_mutex(db));
  return rc;
}

/*
** Obtain a changeset object containing all changes recorded by the 
** session object passed as the first argument.
**
** It is the responsibility of the caller to eventually free the buffer 
** using sqlite3_free().
*/
SQLITE_API int sqlite3session_changeset(
  sqlite3_session *pSession,      /* Session object */
  int *pnChangeset,               /* OUT: Size of buffer at *ppChangeset */
  void **ppChangeset              /* OUT: Buffer containing changeset */
){
  return sessionGenerateChangeset(pSession, 0, 0, 0, pnChangeset, ppChangeset);
}

/*
** Streaming version of sqlite3session_changeset().
*/
SQLITE_API int sqlite3session_changeset_strm(
  sqlite3_session *pSession,
  int (*xOutput)(void *pOut, const void *pData, int nData),
  void *pOut
){
  return sessionGenerateChangeset(pSession, 0, xOutput, pOut, 0, 0);
}

/*
** Streaming version of sqlite3session_patchset().
*/
SQLITE_API int sqlite3session_patchset_strm(
  sqlite3_session *pSession,
  int (*xOutput)(void *pOut, const void *pData, int nData),
  void *pOut
){
  return sessionGenerateChangeset(pSession, 1, xOutput, pOut, 0, 0);
}

/*
** Obtain a patchset object containing all changes recorded by the 
** session object passed as the first argument.
**
** It is the responsibility of the caller to eventually free the buffer 
** using sqlite3_free().
*/
SQLITE_API int sqlite3session_patchset(
  sqlite3_session *pSession,      /* Session object */
  int *pnPatchset,                /* OUT: Size of buffer at *ppChangeset */
  void **ppPatchset               /* OUT: Buffer containing changeset */
){
  return sessionGenerateChangeset(pSession, 1, 0, 0, pnPatchset, ppPatchset);
}

/*
** Enable or disable the session object passed as the first argument.
*/
SQLITE_API int sqlite3session_enable(sqlite3_session *pSession, int bEnable){
  int ret;
  sqlite3_mutex_enter(sqlite3_db_mutex(pSession->db));
  if( bEnable>=0 ){
    pSession->bEnable = bEnable;
  }
  ret = pSession->bEnable;
  sqlite3_mutex_leave(sqlite3_db_mutex(pSession->db));
  return ret;
}

/*
** Enable or disable the session object passed as the first argument.
*/
SQLITE_API int sqlite3session_indirect(sqlite3_session *pSession, int bIndirect){
  int ret;
  sqlite3_mutex_enter(sqlite3_db_mutex(pSession->db));
  if( bIndirect>=0 ){
    pSession->bIndirect = bIndirect;
  }
  ret = pSession->bIndirect;
  sqlite3_mutex_leave(sqlite3_db_mutex(pSession->db));
  return ret;
}

/*
** Return true if there have been no changes to monitored tables recorded
** by the session object passed as the only argument.
*/
SQLITE_API int sqlite3session_isempty(sqlite3_session *pSession){
  int ret = 0;
  SessionTable *pTab;

  sqlite3_mutex_enter(sqlite3_db_mutex(pSession->db));
  for(pTab=pSession->pTable; pTab && ret==0; pTab=pTab->pNext){
    ret = (pTab->nEntry>0);
  }
  sqlite3_mutex_leave(sqlite3_db_mutex(pSession->db));

  return (ret==0);
}

/*
** Do the work for either sqlite3changeset_start() or start_strm().
*/
static int sessionChangesetStart(
  sqlite3_changeset_iter **pp,    /* OUT: Changeset iterator handle */
  int (*xInput)(void *pIn, void *pData, int *pnData),
  void *pIn,
  int nChangeset,                 /* Size of buffer pChangeset in bytes */
  void *pChangeset,               /* Pointer to buffer containing changeset */
  int bInvert                     /* True to invert changeset */
){
  sqlite3_changeset_iter *pRet;   /* Iterator to return */
  int nByte;                      /* Number of bytes to allocate for iterator */

  assert( xInput==0 || (pChangeset==0 && nChangeset==0) );

  /* Zero the output variable in case an error occurs. */
  *pp = 0;

  /* Allocate and initialize the iterator structure. */
  nByte = sizeof(sqlite3_changeset_iter);
  pRet = (sqlite3_changeset_iter *)sqlite3_malloc(nByte);
  if( !pRet ) return SQLITE_NOMEM;
  memset(pRet, 0, sizeof(sqlite3_changeset_iter));
  pRet->in.aData = (u8 *)pChangeset;
  pRet->in.nData = nChangeset;
  pRet->in.xInput = xInput;
  pRet->in.pIn = pIn;
  pRet->in.bEof = (xInput ? 0 : 1);
  pRet->bInvert = bInvert;

  /* Populate the output variable and return success. */
  *pp = pRet;
  return SQLITE_OK;
}

/*
** Create an iterator used to iterate through the contents of a changeset.
*/
SQLITE_API int sqlite3changeset_start(
  sqlite3_changeset_iter **pp,    /* OUT: Changeset iterator handle */
  int nChangeset,                 /* Size of buffer pChangeset in bytes */
  void *pChangeset                /* Pointer to buffer containing changeset */
){
  return sessionChangesetStart(pp, 0, 0, nChangeset, pChangeset, 0);
}
SQLITE_API int sqlite3changeset_start_v2(
  sqlite3_changeset_iter **pp,    /* OUT: Changeset iterator handle */
  int nChangeset,                 /* Size of buffer pChangeset in bytes */
  void *pChangeset,               /* Pointer to buffer containing changeset */
  int flags
){
  int bInvert = !!(flags & SQLITE_CHANGESETSTART_INVERT);
  return sessionChangesetStart(pp, 0, 0, nChangeset, pChangeset, bInvert);
}

/*
** Streaming version of sqlite3changeset_start().
*/
SQLITE_API int sqlite3changeset_start_strm(
  sqlite3_changeset_iter **pp,    /* OUT: Changeset iterator handle */
  int (*xInput)(void *pIn, void *pData, int *pnData),
  void *pIn
){
  return sessionChangesetStart(pp, xInput, pIn, 0, 0, 0);
}
SQLITE_API int sqlite3changeset_start_v2_strm(
  sqlite3_changeset_iter **pp,    /* OUT: Changeset iterator handle */
  int (*xInput)(void *pIn, void *pData, int *pnData),
  void *pIn,
  int flags
){
  int bInvert = !!(flags & SQLITE_CHANGESETSTART_INVERT);
  return sessionChangesetStart(pp, xInput, pIn, 0, 0, bInvert);
}

/*
** If the SessionInput object passed as the only argument is a streaming
** object and the buffer is full, discard some data to free up space.
*/
static void sessionDiscardData(SessionInput *pIn){
  if( pIn->xInput && pIn->iNext>=sessions_strm_chunk_size ){
    int nMove = pIn->buf.nBuf - pIn->iNext;
    assert( nMove>=0 );
    if( nMove>0 ){
      memmove(pIn->buf.aBuf, &pIn->buf.aBuf[pIn->iNext], nMove);
    }
    pIn->buf.nBuf -= pIn->iNext;
    pIn->iNext = 0;
    pIn->nData = pIn->buf.nBuf;
  }
}

/*
** Ensure that there are at least nByte bytes available in the buffer. Or,
** if there are not nByte bytes remaining in the input, that all available
** data is in the buffer.
**
** Return an SQLite error code if an error occurs, or SQLITE_OK otherwise.
*/
static int sessionInputBuffer(SessionInput *pIn, int nByte){
  int rc = SQLITE_OK;
  if( pIn->xInput ){
    while( !pIn->bEof && (pIn->iNext+nByte)>=pIn->nData && rc==SQLITE_OK ){
      int nNew = sessions_strm_chunk_size;

      if( pIn->bNoDiscard==0 ) sessionDiscardData(pIn);
      if( SQLITE_OK==sessionBufferGrow(&pIn->buf, nNew, &rc) ){
        rc = pIn->xInput(pIn->pIn, &pIn->buf.aBuf[pIn->buf.nBuf], &nNew);
        if( nNew==0 ){
          pIn->bEof = 1;
        }else{
          pIn->buf.nBuf += nNew;
        }
      }

      pIn->aData = pIn->buf.aBuf;
      pIn->nData = pIn->buf.nBuf;
    }
  }
  return rc;
}

/*
** When this function is called, *ppRec points to the start of a record
** that contains nCol values. This function advances the pointer *ppRec
** until it points to the byte immediately following that record.
*/
static void sessionSkipRecord(
  u8 **ppRec,                     /* IN/OUT: Record pointer */
  int nCol                        /* Number of values in record */
){
  u8 *aRec = *ppRec;
  int i;
  for(i=0; i<nCol; i++){
    int eType = *aRec++;
    if( eType==SQLITE_TEXT || eType==SQLITE_BLOB ){
      int nByte;
      aRec += sessionVarintGet((u8*)aRec, &nByte);
      aRec += nByte;
    }else if( eType==SQLITE_INTEGER || eType==SQLITE_FLOAT ){
      aRec += 8;
    }
  }

  *ppRec = aRec;
}

/*
** This function sets the value of the sqlite3_value object passed as the
** first argument to a copy of the string or blob held in the aData[] 
** buffer. SQLITE_OK is returned if successful, or SQLITE_NOMEM if an OOM
** error occurs.
*/
static int sessionValueSetStr(
  sqlite3_value *pVal,            /* Set the value of this object */
  u8 *aData,                      /* Buffer containing string or blob data */
  int nData,                      /* Size of buffer aData[] in bytes */
  u8 enc                          /* String encoding (0 for blobs) */
){
  /* In theory this code could just pass SQLITE_TRANSIENT as the final
  ** argument to sqlite3ValueSetStr() and have the copy created 
  ** automatically. But doing so makes it difficult to detect any OOM
  ** error. Hence the code to create the copy externally. */
  u8 *aCopy = sqlite3_malloc64((sqlite3_int64)nData+1);
  if( aCopy==0 ) return SQLITE_NOMEM;
  memcpy(aCopy, aData, nData);
  sqlite3ValueSetStr(pVal, nData, (char*)aCopy, enc, sqlite3_free);
  return SQLITE_OK;
}

/*
** Deserialize a single record from a buffer in memory. See "RECORD FORMAT"
** for details.
**
** When this function is called, *paChange points to the start of the record
** to deserialize. Assuming no error occurs, *paChange is set to point to
** one byte after the end of the same record before this function returns.
** If the argument abPK is NULL, then the record contains nCol values. Or,
** if abPK is other than NULL, then the record contains only the PK fields
** (in other words, it is a patchset DELETE record).
**
** If successful, each element of the apOut[] array (allocated by the caller)
** is set to point to an sqlite3_value object containing the value read
** from the corresponding position in the record. If that value is not
** included in the record (i.e. because the record is part of an UPDATE change
** and the field was not modified), the corresponding element of apOut[] is
** set to NULL.
**
** It is the responsibility of the caller to free all sqlite_value structures
** using sqlite3_free().
**
** If an error occurs, an SQLite error code (e.g. SQLITE_NOMEM) is returned.
** The apOut[] array may have been partially populated in this case.
*/
static int sessionReadRecord(
  SessionInput *pIn,              /* Input data */
  int nCol,                       /* Number of values in record */
  u8 *abPK,                       /* Array of primary key flags, or NULL */
  sqlite3_value **apOut           /* Write values to this array */
){
  int i;                          /* Used to iterate through columns */
  int rc = SQLITE_OK;

  for(i=0; i<nCol && rc==SQLITE_OK; i++){
    int eType = 0;                /* Type of value (SQLITE_NULL, TEXT etc.) */
    if( abPK && abPK[i]==0 ) continue;
    rc = sessionInputBuffer(pIn, 9);
    if( rc==SQLITE_OK ){
      if( pIn->iNext>=pIn->nData ){
        rc = SQLITE_CORRUPT_BKPT;
      }else{
        eType = pIn->aData[pIn->iNext++];
        assert( apOut[i]==0 );
        if( eType ){
          apOut[i] = sqlite3ValueNew(0);
          if( !apOut[i] ) rc = SQLITE_NOMEM;
        }
      }
    }

    if( rc==SQLITE_OK ){
      u8 *aVal = &pIn->aData[pIn->iNext];
      if( eType==SQLITE_TEXT || eType==SQLITE_BLOB ){
        int nByte;
        pIn->iNext += sessionVarintGet(aVal, &nByte);
        rc = sessionInputBuffer(pIn, nByte);
        if( rc==SQLITE_OK ){
          if( nByte<0 || nByte>pIn->nData-pIn->iNext ){
            rc = SQLITE_CORRUPT_BKPT;
          }else{
            u8 enc = (eType==SQLITE_TEXT ? SQLITE_UTF8 : 0);
            rc = sessionValueSetStr(apOut[i],&pIn->aData[pIn->iNext],nByte,enc);
            pIn->iNext += nByte;
          }
        }
      }
      if( eType==SQLITE_INTEGER || eType==SQLITE_FLOAT ){
        sqlite3_int64 v = sessionGetI64(aVal);
        if( eType==SQLITE_INTEGER ){
          sqlite3VdbeMemSetInt64(apOut[i], v);
        }else{
          double d;
          memcpy(&d, &v, 8);
          sqlite3VdbeMemSetDouble(apOut[i], d);
        }
        pIn->iNext += 8;
      }
    }
  }

  return rc;
}

/*
** The input pointer currently points to the second byte of a table-header.
** Specifically, to the following:
**
**   + number of columns in table (varint)
**   + array of PK flags (1 byte per column),
**   + table name (nul terminated).
**
** This function ensures that all of the above is present in the input 
** buffer (i.e. that it can be accessed without any calls to xInput()).
** If successful, SQLITE_OK is returned. Otherwise, an SQLite error code.
** The input pointer is not moved.
*/
static int sessionChangesetBufferTblhdr(SessionInput *pIn, int *pnByte){
  int rc = SQLITE_OK;
  int nCol = 0;
  int nRead = 0;

  rc = sessionInputBuffer(pIn, 9);
  if( rc==SQLITE_OK ){
    nRead += sessionVarintGet(&pIn->aData[pIn->iNext + nRead], &nCol);
    /* The hard upper limit for the number of columns in an SQLite
    ** database table is, according to sqliteLimit.h, 32676. So 
    ** consider any table-header that purports to have more than 65536 
    ** columns to be corrupt. This is convenient because otherwise, 
    ** if the (nCol>65536) condition below were omitted, a sufficiently 
    ** large value for nCol may cause nRead to wrap around and become 
    ** negative. Leading to a crash. */
    if( nCol<0 || nCol>65536 ){
      rc = SQLITE_CORRUPT_BKPT;
    }else{
      rc = sessionInputBuffer(pIn, nRead+nCol+100);
      nRead += nCol;
    }
  }

  while( rc==SQLITE_OK ){
    while( (pIn->iNext + nRead)<pIn->nData && pIn->aData[pIn->iNext + nRead] ){
      nRead++;
    }
    if( (pIn->iNext + nRead)<pIn->nData ) break;
    rc = sessionInputBuffer(pIn, nRead + 100);
  }
  *pnByte = nRead+1;
  return rc;
}

/*
** The input pointer currently points to the first byte of the first field
** of a record consisting of nCol columns. This function ensures the entire
** record is buffered. It does not move the input pointer.
**
** If successful, SQLITE_OK is returned and *pnByte is set to the size of
** the record in bytes. Otherwise, an SQLite error code is returned. The
** final value of *pnByte is undefined in this case.
*/
static int sessionChangesetBufferRecord(
  SessionInput *pIn,              /* Input data */
  int nCol,                       /* Number of columns in record */
  int *pnByte                     /* OUT: Size of record in bytes */
){
  int rc = SQLITE_OK;
  int nByte = 0;
  int i;
  for(i=0; rc==SQLITE_OK && i<nCol; i++){
    int eType;
    rc = sessionInputBuffer(pIn, nByte + 10);
    if( rc==SQLITE_OK ){
      eType = pIn->aData[pIn->iNext + nByte++];
      if( eType==SQLITE_TEXT || eType==SQLITE_BLOB ){
        int n;
        nByte += sessionVarintGet(&pIn->aData[pIn->iNext+nByte], &n);
        nByte += n;
        rc = sessionInputBuffer(pIn, nByte);
      }else if( eType==SQLITE_INTEGER || eType==SQLITE_FLOAT ){
        nByte += 8;
      }
    }
  }
  *pnByte = nByte;
  return rc;
}

/*
** The input pointer currently points to the second byte of a table-header.
** Specifically, to the following:
**
**   + number of columns in table (varint)
**   + array of PK flags (1 byte per column),
**   + table name (nul terminated).
**
** This function decodes the table-header and populates the p->nCol, 
** p->zTab and p->abPK[] variables accordingly. The p->apValue[] array is 
** also allocated or resized according to the new value of p->nCol. The
** input pointer is left pointing to the byte following the table header.
**
** If successful, SQLITE_OK is returned. Otherwise, an SQLite error code
** is returned and the final values of the various fields enumerated above
** are undefined.
*/
static int sessionChangesetReadTblhdr(sqlite3_changeset_iter *p){
  int rc;
  int nCopy;
  assert( p->rc==SQLITE_OK );

  rc = sessionChangesetBufferTblhdr(&p->in, &nCopy);
  if( rc==SQLITE_OK ){
    int nByte;
    int nVarint;
    nVarint = sessionVarintGet(&p->in.aData[p->in.iNext], &p->nCol);
    if( p->nCol>0 ){
      nCopy -= nVarint;
      p->in.iNext += nVarint;
      nByte = p->nCol * sizeof(sqlite3_value*) * 2 + nCopy;
      p->tblhdr.nBuf = 0;
      sessionBufferGrow(&p->tblhdr, nByte, &rc);
    }else{
      rc = SQLITE_CORRUPT_BKPT;
    }
  }

  if( rc==SQLITE_OK ){
    size_t iPK = sizeof(sqlite3_value*)*p->nCol*2;
    memset(p->tblhdr.aBuf, 0, iPK);
    memcpy(&p->tblhdr.aBuf[iPK], &p->in.aData[p->in.iNext], nCopy);
    p->in.iNext += nCopy;
  }

  p->apValue = (sqlite3_value**)p->tblhdr.aBuf;
  p->abPK = (u8*)&p->apValue[p->nCol*2];
  p->zTab = (char*)&p->abPK[p->nCol];
  return (p->rc = rc);
}

/*
** Advance the changeset iterator to the next change.
**
** If both paRec and pnRec are NULL, then this function works like the public
** API sqlite3changeset_next(). If SQLITE_ROW is returned, then the
** sqlite3changeset_new() and old() APIs may be used to query for values.
**
** Otherwise, if paRec and pnRec are not NULL, then a pointer to the change
** record is written to *paRec before returning and the number of bytes in
** the record to *pnRec.
**
** Either way, this function returns SQLITE_ROW if the iterator is 
** successfully advanced to the next change in the changeset, an SQLite 
** error code if an error occurs, or SQLITE_DONE if there are no further 
** changes in the changeset.
*/
static int sessionChangesetNext(
  sqlite3_changeset_iter *p,      /* Changeset iterator */
  u8 **paRec,                     /* If non-NULL, store record pointer here */
  int *pnRec,                     /* If non-NULL, store size of record here */
  int *pbNew                      /* If non-NULL, true if new table */
){
  int i;
  u8 op;

  assert( (paRec==0 && pnRec==0) || (paRec && pnRec) );

  /* If the iterator is in the error-state, return immediately. */
  if( p->rc!=SQLITE_OK ) return p->rc;

  /* Free the current contents of p->apValue[], if any. */
  if( p->apValue ){
    for(i=0; i<p->nCol*2; i++){
      sqlite3ValueFree(p->apValue[i]);
    }
    memset(p->apValue, 0, sizeof(sqlite3_value*)*p->nCol*2);
  }

  /* Make sure the buffer contains at least 10 bytes of input data, or all
  ** remaining data if there are less than 10 bytes available. This is
  ** sufficient either for the 'T' or 'P' byte and the varint that follows
  ** it, or for the two single byte values otherwise. */
  p->rc = sessionInputBuffer(&p->in, 2);
  if( p->rc!=SQLITE_OK ) return p->rc;

  /* If the iterator is already at the end of the changeset, return DONE. */
  if( p->in.iNext>=p->in.nData ){
    return SQLITE_DONE;
  }

  sessionDiscardData(&p->in);
  p->in.iCurrent = p->in.iNext;

  op = p->in.aData[p->in.iNext++];
  while( op=='T' || op=='P' ){
    if( pbNew ) *pbNew = 1;
    p->bPatchset = (op=='P');
    if( sessionChangesetReadTblhdr(p) ) return p->rc;
    if( (p->rc = sessionInputBuffer(&p->in, 2)) ) return p->rc;
    p->in.iCurrent = p->in.iNext;
    if( p->in.iNext>=p->in.nData ) return SQLITE_DONE;
    op = p->in.aData[p->in.iNext++];
  }

  if( p->zTab==0 || (p->bPatchset && p->bInvert) ){
    /* The first record in the changeset is not a table header. Must be a
    ** corrupt changeset. */
    assert( p->in.iNext==1 || p->zTab );
    return (p->rc = SQLITE_CORRUPT_BKPT);
  }

  p->op = op;
  p->bIndirect = p->in.aData[p->in.iNext++];
  if( p->op!=SQLITE_UPDATE && p->op!=SQLITE_DELETE && p->op!=SQLITE_INSERT ){
    return (p->rc = SQLITE_CORRUPT_BKPT);
  }

  if( paRec ){ 
    int nVal;                     /* Number of values to buffer */
    if( p->bPatchset==0 && op==SQLITE_UPDATE ){
      nVal = p->nCol * 2;
    }else if( p->bPatchset && op==SQLITE_DELETE ){
      nVal = 0;
      for(i=0; i<p->nCol; i++) if( p->abPK[i] ) nVal++;
    }else{
      nVal = p->nCol;
    }
    p->rc = sessionChangesetBufferRecord(&p->in, nVal, pnRec);
    if( p->rc!=SQLITE_OK ) return p->rc;
    *paRec = &p->in.aData[p->in.iNext];
    p->in.iNext += *pnRec;
  }else{
    sqlite3_value **apOld = (p->bInvert ? &p->apValue[p->nCol] : p->apValue);
    sqlite3_value **apNew = (p->bInvert ? p->apValue : &p->apValue[p->nCol]);

    /* If this is an UPDATE or DELETE, read the old.* record. */
    if( p->op!=SQLITE_INSERT && (p->bPatchset==0 || p->op==SQLITE_DELETE) ){
      u8 *abPK = p->bPatchset ? p->abPK : 0;
      p->rc = sessionReadRecord(&p->in, p->nCol, abPK, apOld);
      if( p->rc!=SQLITE_OK ) return p->rc;
    }

    /* If this is an INSERT or UPDATE, read the new.* record. */
    if( p->op!=SQLITE_DELETE ){
      p->rc = sessionReadRecord(&p->in, p->nCol, 0, apNew);
      if( p->rc!=SQLITE_OK ) return p->rc;
    }

    if( (p->bPatchset || p->bInvert) && p->op==SQLITE_UPDATE ){
      /* If this is an UPDATE that is part of a patchset, then all PK and
      ** modified fields are present in the new.* record. The old.* record
      ** is currently completely empty. This block shifts the PK fields from
      ** new.* to old.*, to accommodate the code that reads these arrays.  */
      for(i=0; i<p->nCol; i++){
        assert( p->bPatchset==0 || p->apValue[i]==0 );
        if( p->abPK[i] ){
          assert( p->apValue[i]==0 );
          p->apValue[i] = p->apValue[i+p->nCol];
          if( p->apValue[i]==0 ) return (p->rc = SQLITE_CORRUPT_BKPT);
          p->apValue[i+p->nCol] = 0;
        }
      }
    }else if( p->bInvert ){
      if( p->op==SQLITE_INSERT ) p->op = SQLITE_DELETE;
      else if( p->op==SQLITE_DELETE ) p->op = SQLITE_INSERT;
    }
  }

  return SQLITE_ROW;
}

/*
** Advance an iterator created by sqlite3changeset_start() to the next
** change in the changeset. This function may return SQLITE_ROW, SQLITE_DONE
** or SQLITE_CORRUPT.
**
** This function may not be called on iterators passed to a conflict handler
** callback by changeset_apply().
*/
SQLITE_API int sqlite3changeset_next(sqlite3_changeset_iter *p){
  return sessionChangesetNext(p, 0, 0, 0);
}

/*
** The following function extracts information on the current change
** from a changeset iterator. It may only be called after changeset_next()
** has returned SQLITE_ROW.
*/
SQLITE_API int sqlite3changeset_op(
  sqlite3_changeset_iter *pIter,  /* Iterator handle */
  const char **pzTab,             /* OUT: Pointer to table name */
  int *pnCol,                     /* OUT: Number of columns in table */
  int *pOp,                       /* OUT: SQLITE_INSERT, DELETE or UPDATE */
  int *pbIndirect                 /* OUT: True if change is indirect */
){
  *pOp = pIter->op;
  *pnCol = pIter->nCol;
  *pzTab = pIter->zTab;
  if( pbIndirect ) *pbIndirect = pIter->bIndirect;
  return SQLITE_OK;
}

/*
** Return information regarding the PRIMARY KEY and number of columns in
** the database table affected by the change that pIter currently points
** to. This function may only be called after changeset_next() returns
** SQLITE_ROW.
*/
SQLITE_API int sqlite3changeset_pk(
  sqlite3_changeset_iter *pIter,  /* Iterator object */
  unsigned char **pabPK,          /* OUT: Array of boolean - true for PK cols */
  int *pnCol                      /* OUT: Number of entries in output array */
){
  *pabPK = pIter->abPK;
  if( pnCol ) *pnCol = pIter->nCol;
  return SQLITE_OK;
}

/*
** This function may only be called while the iterator is pointing to an
** SQLITE_UPDATE or SQLITE_DELETE change (see sqlite3changeset_op()).
** Otherwise, SQLITE_MISUSE is returned.
**
** It sets *ppValue to point to an sqlite3_value structure containing the
** iVal'th value in the old.* record. Or, if that particular value is not
** included in the record (because the change is an UPDATE and the field
** was not modified and is not a PK column), set *ppValue to NULL.
**
** If value iVal is out-of-range, SQLITE_RANGE is returned and *ppValue is
** not modified. Otherwise, SQLITE_OK.
*/
SQLITE_API int sqlite3changeset_old(
  sqlite3_changeset_iter *pIter,  /* Changeset iterator */
  int iVal,                       /* Index of old.* value to retrieve */
  sqlite3_value **ppValue         /* OUT: Old value (or NULL pointer) */
){
  if( pIter->op!=SQLITE_UPDATE && pIter->op!=SQLITE_DELETE ){
    return SQLITE_MISUSE;
  }
  if( iVal<0 || iVal>=pIter->nCol ){
    return SQLITE_RANGE;
  }
  *ppValue = pIter->apValue[iVal];
  return SQLITE_OK;
}

/*
** This function may only be called while the iterator is pointing to an
** SQLITE_UPDATE or SQLITE_INSERT change (see sqlite3changeset_op()).
** Otherwise, SQLITE_MISUSE is returned.
**
** It sets *ppValue to point to an sqlite3_value structure containing the
** iVal'th value in the new.* record. Or, if that particular value is not
** included in the record (because the change is an UPDATE and the field
** was not modified), set *ppValue to NULL.
**
** If value iVal is out-of-range, SQLITE_RANGE is returned and *ppValue is
** not modified. Otherwise, SQLITE_OK.
*/
SQLITE_API int sqlite3changeset_new(
  sqlite3_changeset_iter *pIter,  /* Changeset iterator */
  int iVal,                       /* Index of new.* value to retrieve */
  sqlite3_value **ppValue         /* OUT: New value (or NULL pointer) */
){
  if( pIter->op!=SQLITE_UPDATE && pIter->op!=SQLITE_INSERT ){
    return SQLITE_MISUSE;
  }
  if( iVal<0 || iVal>=pIter->nCol ){
    return SQLITE_RANGE;
  }
  *ppValue = pIter->apValue[pIter->nCol+iVal];
  return SQLITE_OK;
}

/*
** The following two macros are used internally. They are similar to the
** sqlite3changeset_new() and sqlite3changeset_old() functions, except that
** they omit all error checking and return a pointer to the requested value.
*/
#define sessionChangesetNew(pIter, iVal) (pIter)->apValue[(pIter)->nCol+(iVal)]
#define sessionChangesetOld(pIter, iVal) (pIter)->apValue[(iVal)]

/*
** This function may only be called with a changeset iterator that has been
** passed to an SQLITE_CHANGESET_DATA or SQLITE_CHANGESET_CONFLICT 
** conflict-handler function. Otherwise, SQLITE_MISUSE is returned.
**
** If successful, *ppValue is set to point to an sqlite3_value structure
** containing the iVal'th value of the conflicting record.
**
** If value iVal is out-of-range or some other error occurs, an SQLite error
** code is returned. Otherwise, SQLITE_OK.
*/
SQLITE_API int sqlite3changeset_conflict(
  sqlite3_changeset_iter *pIter,  /* Changeset iterator */
  int iVal,                       /* Index of conflict record value to fetch */
  sqlite3_value **ppValue         /* OUT: Value from conflicting row */
){
  if( !pIter->pConflict ){
    return SQLITE_MISUSE;
  }
  if( iVal<0 || iVal>=pIter->nCol ){
    return SQLITE_RANGE;
  }
  *ppValue = sqlite3_column_value(pIter->pConflict, iVal);
  return SQLITE_OK;
}

/*
** This function may only be called with an iterator passed to an
** SQLITE_CHANGESET_FOREIGN_KEY conflict handler callback. In this case
** it sets the output variable to the total number of known foreign key
** violations in the destination database and returns SQLITE_OK.
**
** In all other cases this function returns SQLITE_MISUSE.
*/
SQLITE_API int sqlite3changeset_fk_conflicts(
  sqlite3_changeset_iter *pIter,  /* Changeset iterator */
  int *pnOut                      /* OUT: Number of FK violations */
){
  if( pIter->pConflict || pIter->apValue ){
    return SQLITE_MISUSE;
  }
  *pnOut = pIter->nCol;
  return SQLITE_OK;
}


/*
** Finalize an iterator allocated with sqlite3changeset_start().
**
** This function may not be called on iterators passed to a conflict handler
** callback by changeset_apply().
*/
SQLITE_API int sqlite3changeset_finalize(sqlite3_changeset_iter *p){
  int rc = SQLITE_OK;
  if( p ){
    int i;                        /* Used to iterate through p->apValue[] */
    rc = p->rc;
    if( p->apValue ){
      for(i=0; i<p->nCol*2; i++) sqlite3ValueFree(p->apValue[i]);
    }
    sqlite3_free(p->tblhdr.aBuf);
    sqlite3_free(p->in.buf.aBuf);
    sqlite3_free(p);
  }
  return rc;
}

static int sessionChangesetInvert(
  SessionInput *pInput,           /* Input changeset */
  int (*xOutput)(void *pOut, const void *pData, int nData),
  void *pOut,
  int *pnInverted,                /* OUT: Number of bytes in output changeset */
  void **ppInverted               /* OUT: Inverse of pChangeset */
){
  int rc = SQLITE_OK;             /* Return value */
  SessionBuffer sOut;             /* Output buffer */
  int nCol = 0;                   /* Number of cols in current table */
  u8 *abPK = 0;                   /* PK array for current table */
  sqlite3_value **apVal = 0;      /* Space for values for UPDATE inversion */
  SessionBuffer sPK = {0, 0, 0};  /* PK array for current table */

  /* Initialize the output buffer */
  memset(&sOut, 0, sizeof(SessionBuffer));

  /* Zero the output variables in case an error occurs. */
  if( ppInverted ){
    *ppInverted = 0;
    *pnInverted = 0;
  }

  while( 1 ){
    u8 eType;

    /* Test for EOF. */
    if( (rc = sessionInputBuffer(pInput, 2)) ) goto finished_invert;
    if( pInput->iNext>=pInput->nData ) break;
    eType = pInput->aData[pInput->iNext];

    switch( eType ){
      case 'T': {
        /* A 'table' record consists of:
        **
        **   * A constant 'T' character,
        **   * Number of columns in said table (a varint),
        **   * An array of nCol bytes (sPK),
        **   * A nul-terminated table name.
        */
        int nByte;
        int nVar;
        pInput->iNext++;
        if( (rc = sessionChangesetBufferTblhdr(pInput, &nByte)) ){
          goto finished_invert;
        }
        nVar = sessionVarintGet(&pInput->aData[pInput->iNext], &nCol);
        sPK.nBuf = 0;
        sessionAppendBlob(&sPK, &pInput->aData[pInput->iNext+nVar], nCol, &rc);
        sessionAppendByte(&sOut, eType, &rc);
        sessionAppendBlob(&sOut, &pInput->aData[pInput->iNext], nByte, &rc);
        if( rc ) goto finished_invert;

        pInput->iNext += nByte;
        sqlite3_free(apVal);
        apVal = 0;
        abPK = sPK.aBuf;
        break;
      }

      case SQLITE_INSERT:
      case SQLITE_DELETE: {
        int nByte;
        int bIndirect = pInput->aData[pInput->iNext+1];
        int eType2 = (eType==SQLITE_DELETE ? SQLITE_INSERT : SQLITE_DELETE);
        pInput->iNext += 2;
        assert( rc==SQLITE_OK );
        rc = sessionChangesetBufferRecord(pInput, nCol, &nByte);
        sessionAppendByte(&sOut, eType2, &rc);
        sessionAppendByte(&sOut, bIndirect, &rc);
        sessionAppendBlob(&sOut, &pInput->aData[pInput->iNext], nByte, &rc);
        pInput->iNext += nByte;
        if( rc ) goto finished_invert;
        break;
      }

      case SQLITE_UPDATE: {
        int iCol;

        if( 0==apVal ){
          apVal = (sqlite3_value **)sqlite3_malloc64(sizeof(apVal[0])*nCol*2);
          if( 0==apVal ){
            rc = SQLITE_NOMEM;
            goto finished_invert;
          }
          memset(apVal, 0, sizeof(apVal[0])*nCol*2);
        }

        /* Write the header for the new UPDATE change. Same as the original. */
        sessionAppendByte(&sOut, eType, &rc);
        sessionAppendByte(&sOut, pInput->aData[pInput->iNext+1], &rc);

        /* Read the old.* and new.* records for the update change. */
        pInput->iNext += 2;
        rc = sessionReadRecord(pInput, nCol, 0, &apVal[0]);
        if( rc==SQLITE_OK ){
          rc = sessionReadRecord(pInput, nCol, 0, &apVal[nCol]);
        }

        /* Write the new old.* record. Consists of the PK columns from the
        ** original old.* record, and the other values from the original
        ** new.* record. */
        for(iCol=0; iCol<nCol; iCol++){
          sqlite3_value *pVal = apVal[iCol + (abPK[iCol] ? 0 : nCol)];
          sessionAppendValue(&sOut, pVal, &rc);
        }

        /* Write the new new.* record. Consists of a copy of all values
        ** from the original old.* record, except for the PK columns, which
        ** are set to "undefined". */
        for(iCol=0; iCol<nCol; iCol++){
          sqlite3_value *pVal = (abPK[iCol] ? 0 : apVal[iCol]);
          sessionAppendValue(&sOut, pVal, &rc);
        }

        for(iCol=0; iCol<nCol*2; iCol++){
          sqlite3ValueFree(apVal[iCol]);
        }
        memset(apVal, 0, sizeof(apVal[0])*nCol*2);
        if( rc!=SQLITE_OK ){
          goto finished_invert;
        }

        break;
      }

      default:
        rc = SQLITE_CORRUPT_BKPT;
        goto finished_invert;
    }

    assert( rc==SQLITE_OK );
    if( xOutput && sOut.nBuf>=sessions_strm_chunk_size ){
      rc = xOutput(pOut, sOut.aBuf, sOut.nBuf);
      sOut.nBuf = 0;
      if( rc!=SQLITE_OK ) goto finished_invert;
    }
  }

  assert( rc==SQLITE_OK );
  if( pnInverted ){
    *pnInverted = sOut.nBuf;
    *ppInverted = sOut.aBuf;
    sOut.aBuf = 0;
  }else if( sOut.nBuf>0 ){
    rc = xOutput(pOut, sOut.aBuf, sOut.nBuf);
  }

 finished_invert:
  sqlite3_free(sOut.aBuf);
  sqlite3_free(apVal);
  sqlite3_free(sPK.aBuf);
  return rc;
}


/*
** Invert a changeset object.
*/
SQLITE_API int sqlite3changeset_invert(
  int nChangeset,                 /* Number of bytes in input */
  const void *pChangeset,         /* Input changeset */
  int *pnInverted,                /* OUT: Number of bytes in output changeset */
  void **ppInverted               /* OUT: Inverse of pChangeset */
){
  SessionInput sInput;

  /* Set up the input stream */
  memset(&sInput, 0, sizeof(SessionInput));
  sInput.nData = nChangeset;
  sInput.aData = (u8*)pChangeset;

  return sessionChangesetInvert(&sInput, 0, 0, pnInverted, ppInverted);
}

/*
** Streaming version of sqlite3changeset_invert().
*/
SQLITE_API int sqlite3changeset_invert_strm(
  int (*xInput)(void *pIn, void *pData, int *pnData),
  void *pIn,
  int (*xOutput)(void *pOut, const void *pData, int nData),
  void *pOut
){
  SessionInput sInput;
  int rc;

  /* Set up the input stream */
  memset(&sInput, 0, sizeof(SessionInput));
  sInput.xInput = xInput;
  sInput.pIn = pIn;

  rc = sessionChangesetInvert(&sInput, xOutput, pOut, 0, 0);
  sqlite3_free(sInput.buf.aBuf);
  return rc;
}

typedef struct SessionApplyCtx SessionApplyCtx;
struct SessionApplyCtx {
  sqlite3 *db;
  sqlite3_stmt *pDelete;          /* DELETE statement */
  sqlite3_stmt *pUpdate;          /* UPDATE statement */
  sqlite3_stmt *pInsert;          /* INSERT statement */
  sqlite3_stmt *pSelect;          /* SELECT statement */
  int nCol;                       /* Size of azCol[] and abPK[] arrays */
  const char **azCol;             /* Array of column names */
  u8 *abPK;                       /* Boolean array - true if column is in PK */
  int bStat1;                     /* True if table is sqlite_stat1 */
  int bDeferConstraints;          /* True to defer constraints */
  SessionBuffer constraints;      /* Deferred constraints are stored here */
  SessionBuffer rebase;           /* Rebase information (if any) here */
  u8 bRebaseStarted;              /* If table header is already in rebase */
  u8 bRebase;                     /* True to collect rebase information */
};

/*
** Formulate a statement to DELETE a row from database db. Assuming a table
** structure like this:
**
**     CREATE TABLE x(a, b, c, d, PRIMARY KEY(a, c));
**
** The DELETE statement looks like this:
**
**     DELETE FROM x WHERE a = :1 AND c = :3 AND (:5 OR b IS :2 AND d IS :4)
**
** Variable :5 (nCol+1) is a boolean. It should be set to 0 if we require
** matching b and d values, or 1 otherwise. The second case comes up if the
** conflict handler is invoked with NOTFOUND and returns CHANGESET_REPLACE.
**
** If successful, SQLITE_OK is returned and SessionApplyCtx.pDelete is left
** pointing to the prepared version of the SQL statement.
*/
static int sessionDeleteRow(
  sqlite3 *db,                    /* Database handle */
  const char *zTab,               /* Table name */
  SessionApplyCtx *p              /* Session changeset-apply context */
){
  int i;
  const char *zSep = "";
  int rc = SQLITE_OK;
  SessionBuffer buf = {0, 0, 0};
  int nPk = 0;

  sessionAppendStr(&buf, "DELETE FROM ", &rc);
  sessionAppendIdent(&buf, zTab, &rc);
  sessionAppendStr(&buf, " WHERE ", &rc);

  for(i=0; i<p->nCol; i++){
    if( p->abPK[i] ){
      nPk++;
      sessionAppendStr(&buf, zSep, &rc);
      sessionAppendIdent(&buf, p->azCol[i], &rc);
      sessionAppendStr(&buf, " = ?", &rc);
      sessionAppendInteger(&buf, i+1, &rc);
      zSep = " AND ";
    }
  }

  if( nPk<p->nCol ){
    sessionAppendStr(&buf, " AND (?", &rc);
    sessionAppendInteger(&buf, p->nCol+1, &rc);
    sessionAppendStr(&buf, " OR ", &rc);

    zSep = "";
    for(i=0; i<p->nCol; i++){
      if( !p->abPK[i] ){
        sessionAppendStr(&buf, zSep, &rc);
        sessionAppendIdent(&buf, p->azCol[i], &rc);
        sessionAppendStr(&buf, " IS ?", &rc);
        sessionAppendInteger(&buf, i+1, &rc);
        zSep = "AND ";
      }
    }
    sessionAppendStr(&buf, ")", &rc);
  }

  if( rc==SQLITE_OK ){
    rc = sqlite3_prepare_v2(db, (char *)buf.aBuf, buf.nBuf, &p->pDelete, 0);
  }
  sqlite3_free(buf.aBuf);

  return rc;
}

/*
** Formulate and prepare a statement to UPDATE a row from database db. 
** Assuming a table structure like this:
**
**     CREATE TABLE x(a, b, c, d, PRIMARY KEY(a, c));
**
** The UPDATE statement looks like this:
**
**     UPDATE x SET
**     a = CASE WHEN ?2  THEN ?3  ELSE a END,
**     b = CASE WHEN ?5  THEN ?6  ELSE b END,
**     c = CASE WHEN ?8  THEN ?9  ELSE c END,
**     d = CASE WHEN ?11 THEN ?12 ELSE d END
**     WHERE a = ?1 AND c = ?7 AND (?13 OR 
**       (?5==0 OR b IS ?4) AND (?11==0 OR d IS ?10) AND
**     )
**
** For each column in the table, there are three variables to bind:
**
**     ?(i*3+1)    The old.* value of the column, if any.
**     ?(i*3+2)    A boolean flag indicating that the value is being modified.
**     ?(i*3+3)    The new.* value of the column, if any.
**
** Also, a boolean flag that, if set to true, causes the statement to update
** a row even if the non-PK values do not match. This is required if the
** conflict-handler is invoked with CHANGESET_DATA and returns
** CHANGESET_REPLACE. This is variable "?(nCol*3+1)".
**
** If successful, SQLITE_OK is returned and SessionApplyCtx.pUpdate is left
** pointing to the prepared version of the SQL statement.
*/
static int sessionUpdateRow(
  sqlite3 *db,                    /* Database handle */
  const char *zTab,               /* Table name */
  SessionApplyCtx *p              /* Session changeset-apply context */
){
  int rc = SQLITE_OK;
  int i;
  const char *zSep = "";
  SessionBuffer buf = {0, 0, 0};

  /* Append "UPDATE tbl SET " */
  sessionAppendStr(&buf, "UPDATE ", &rc);
  sessionAppendIdent(&buf, zTab, &rc);
  sessionAppendStr(&buf, " SET ", &rc);

  /* Append the assignments */
  for(i=0; i<p->nCol; i++){
    sessionAppendStr(&buf, zSep, &rc);
    sessionAppendIdent(&buf, p->azCol[i], &rc);
    sessionAppendStr(&buf, " = CASE WHEN ?", &rc);
    sessionAppendInteger(&buf, i*3+2, &rc);
    sessionAppendStr(&buf, " THEN ?", &rc);
    sessionAppendInteger(&buf, i*3+3, &rc);
    sessionAppendStr(&buf, " ELSE ", &rc);
    sessionAppendIdent(&buf, p->azCol[i], &rc);
    sessionAppendStr(&buf, " END", &rc);
    zSep = ", ";
  }

  /* Append the PK part of the WHERE clause */
  sessionAppendStr(&buf, " WHERE ", &rc);
  for(i=0; i<p->nCol; i++){
    if( p->abPK[i] ){
      sessionAppendIdent(&buf, p->azCol[i], &rc);
      sessionAppendStr(&buf, " = ?", &rc);
      sessionAppendInteger(&buf, i*3+1, &rc);
      sessionAppendStr(&buf, " AND ", &rc);
    }
  }

  /* Append the non-PK part of the WHERE clause */
  sessionAppendStr(&buf, " (?", &rc);
  sessionAppendInteger(&buf, p->nCol*3+1, &rc);
  sessionAppendStr(&buf, " OR 1", &rc);
  for(i=0; i<p->nCol; i++){
    if( !p->abPK[i] ){
      sessionAppendStr(&buf, " AND (?", &rc);
      sessionAppendInteger(&buf, i*3+2, &rc);
      sessionAppendStr(&buf, "=0 OR ", &rc);
      sessionAppendIdent(&buf, p->azCol[i], &rc);
      sessionAppendStr(&buf, " IS ?", &rc);
      sessionAppendInteger(&buf, i*3+1, &rc);
      sessionAppendStr(&buf, ")", &rc);
    }
  }
  sessionAppendStr(&buf, ")", &rc);

  if( rc==SQLITE_OK ){
    rc = sqlite3_prepare_v2(db, (char *)buf.aBuf, buf.nBuf, &p->pUpdate, 0);
  }
  sqlite3_free(buf.aBuf);

  return rc;
}


/*
** Formulate and prepare an SQL statement to query table zTab by primary
** key. Assuming the following table structure:
**
**     CREATE TABLE x(a, b, c, d, PRIMARY KEY(a, c));
**
** The SELECT statement looks like this:
**
**     SELECT * FROM x WHERE a = ?1 AND c = ?3
**
** If successful, SQLITE_OK is returned and SessionApplyCtx.pSelect is left
** pointing to the prepared version of the SQL statement.
*/
static int sessionSelectRow(
  sqlite3 *db,                    /* Database handle */
  const char *zTab,               /* Table name */
  SessionApplyCtx *p              /* Session changeset-apply context */
){
  return sessionSelectStmt(
      db, "main", zTab, p->nCol, p->azCol, p->abPK, &p->pSelect);
}

/*
** Formulate and prepare an INSERT statement to add a record to table zTab.
** For example:
**
**     INSERT INTO main."zTab" VALUES(?1, ?2, ?3 ...);
**
** If successful, SQLITE_OK is returned and SessionApplyCtx.pInsert is left
** pointing to the prepared version of the SQL statement.
*/
static int sessionInsertRow(
  sqlite3 *db,                    /* Database handle */
  const char *zTab,               /* Table name */
  SessionApplyCtx *p              /* Session changeset-apply context */
){
  int rc = SQLITE_OK;
  int i;
  SessionBuffer buf = {0, 0, 0};

  sessionAppendStr(&buf, "INSERT INTO main.", &rc);
  sessionAppendIdent(&buf, zTab, &rc);
  sessionAppendStr(&buf, "(", &rc);
  for(i=0; i<p->nCol; i++){
    if( i!=0 ) sessionAppendStr(&buf, ", ", &rc);
    sessionAppendIdent(&buf, p->azCol[i], &rc);
  }

  sessionAppendStr(&buf, ") VALUES(?", &rc);
  for(i=1; i<p->nCol; i++){
    sessionAppendStr(&buf, ", ?", &rc);
  }
  sessionAppendStr(&buf, ")", &rc);

  if( rc==SQLITE_OK ){
    rc = sqlite3_prepare_v2(db, (char *)buf.aBuf, buf.nBuf, &p->pInsert, 0);
  }
  sqlite3_free(buf.aBuf);
  return rc;
}

static int sessionPrepare(sqlite3 *db, sqlite3_stmt **pp, const char *zSql){
  return sqlite3_prepare_v2(db, zSql, -1, pp, 0);
}

/*
** Prepare statements for applying changes to the sqlite_stat1 table.
** These are similar to those created by sessionSelectRow(),
** sessionInsertRow(), sessionUpdateRow() and sessionDeleteRow() for 
** other tables.
*/
static int sessionStat1Sql(sqlite3 *db, SessionApplyCtx *p){
  int rc = sessionSelectRow(db, "sqlite_stat1", p);
  if( rc==SQLITE_OK ){
    rc = sessionPrepare(db, &p->pInsert,
        "INSERT INTO main.sqlite_stat1 VALUES(?1, "
        "CASE WHEN length(?2)=0 AND typeof(?2)='blob' THEN NULL ELSE ?2 END, "
        "?3)"
    );
  }
  if( rc==SQLITE_OK ){
    rc = sessionPrepare(db, &p->pUpdate,
        "UPDATE main.sqlite_stat1 SET "
        "tbl = CASE WHEN ?2 THEN ?3 ELSE tbl END, "
        "idx = CASE WHEN ?5 THEN ?6 ELSE idx END, "
        "stat = CASE WHEN ?8 THEN ?9 ELSE stat END  "
        "WHERE tbl=?1 AND idx IS "
        "CASE WHEN length(?4)=0 AND typeof(?4)='blob' THEN NULL ELSE ?4 END "
        "AND (?10 OR ?8=0 OR stat IS ?7)"
    );
  }
  if( rc==SQLITE_OK ){
    rc = sessionPrepare(db, &p->pDelete,
        "DELETE FROM main.sqlite_stat1 WHERE tbl=?1 AND idx IS "
        "CASE WHEN length(?2)=0 AND typeof(?2)='blob' THEN NULL ELSE ?2 END "
        "AND (?4 OR stat IS ?3)"
    );
  }
  return rc;
}

/*
** A wrapper around sqlite3_bind_value() that detects an extra problem. 
** See comments in the body of this function for details.
*/
static int sessionBindValue(
  sqlite3_stmt *pStmt,            /* Statement to bind value to */
  int i,                          /* Parameter number to bind to */
  sqlite3_value *pVal             /* Value to bind */
){
  int eType = sqlite3_value_type(pVal);
  /* COVERAGE: The (pVal->z==0) branch is never true using current versions
  ** of SQLite. If a malloc fails in an sqlite3_value_xxx() function, either
  ** the (pVal->z) variable remains as it was or the type of the value is
  ** set to SQLITE_NULL.  */
  if( (eType==SQLITE_TEXT || eType==SQLITE_BLOB) && pVal->z==0 ){
    /* This condition occurs when an earlier OOM in a call to
    ** sqlite3_value_text() or sqlite3_value_blob() (perhaps from within
    ** a conflict-handler) has zeroed the pVal->z pointer. Return NOMEM. */
    return SQLITE_NOMEM;
  }
  return sqlite3_bind_value(pStmt, i, pVal);
}

/*
** Iterator pIter must point to an SQLITE_INSERT entry. This function 
** transfers new.* values from the current iterator entry to statement
** pStmt. The table being inserted into has nCol columns.
**
** New.* value $i from the iterator is bound to variable ($i+1) of 
** statement pStmt. If parameter abPK is NULL, all values from 0 to (nCol-1)
** are transfered to the statement. Otherwise, if abPK is not NULL, it points
** to an array nCol elements in size. In this case only those values for 
** which abPK[$i] is true are read from the iterator and bound to the 
** statement.
**
** An SQLite error code is returned if an error occurs. Otherwise, SQLITE_OK.
*/
static int sessionBindRow(
  sqlite3_changeset_iter *pIter,  /* Iterator to read values from */
  int(*xValue)(sqlite3_changeset_iter *, int, sqlite3_value **),
  int nCol,                       /* Number of columns */
  u8 *abPK,                       /* If not NULL, bind only if true */
  sqlite3_stmt *pStmt             /* Bind values to this statement */
){
  int i;
  int rc = SQLITE_OK;

  /* Neither sqlite3changeset_old or sqlite3changeset_new can fail if the
  ** argument iterator points to a suitable entry. Make sure that xValue 
  ** is one of these to guarantee that it is safe to ignore the return 
  ** in the code below. */
  assert( xValue==sqlite3changeset_old || xValue==sqlite3changeset_new );

  for(i=0; rc==SQLITE_OK && i<nCol; i++){
    if( !abPK || abPK[i] ){
      sqlite3_value *pVal;
      (void)xValue(pIter, i, &pVal);
      if( pVal==0 ){
        /* The value in the changeset was "undefined". This indicates a
        ** corrupt changeset blob.  */
        rc = SQLITE_CORRUPT_BKPT;
      }else{
        rc = sessionBindValue(pStmt, i+1, pVal);
      }
    }
  }
  return rc;
}

/*
** SQL statement pSelect is as generated by the sessionSelectRow() function.
** This function binds the primary key values from the change that changeset
** iterator pIter points to to the SELECT and attempts to seek to the table
** entry. If a row is found, the SELECT statement left pointing at the row 
** and SQLITE_ROW is returned. Otherwise, if no row is found and no error
** has occured, the statement is reset and SQLITE_OK is returned. If an
** error occurs, the statement is reset and an SQLite error code is returned.
**
** If this function returns SQLITE_ROW, the caller must eventually reset() 
** statement pSelect. If any other value is returned, the statement does
** not require a reset().
**
** If the iterator currently points to an INSERT record, bind values from the
** new.* record to the SELECT statement. Or, if it points to a DELETE or
** UPDATE, bind values from the old.* record. 
*/
static int sessionSeekToRow(
  sqlite3 *db,                    /* Database handle */
  sqlite3_changeset_iter *pIter,  /* Changeset iterator */
  u8 *abPK,                       /* Primary key flags array */
  sqlite3_stmt *pSelect           /* SELECT statement from sessionSelectRow() */
){
  int rc;                         /* Return code */
  int nCol;                       /* Number of columns in table */
  int op;                         /* Changset operation (SQLITE_UPDATE etc.) */
  const char *zDummy;             /* Unused */

  sqlite3changeset_op(pIter, &zDummy, &nCol, &op, 0);
  rc = sessionBindRow(pIter, 
      op==SQLITE_INSERT ? sqlite3changeset_new : sqlite3changeset_old,
      nCol, abPK, pSelect
  );

  if( rc==SQLITE_OK ){
    rc = sqlite3_step(pSelect);
    if( rc!=SQLITE_ROW ) rc = sqlite3_reset(pSelect);
  }

  return rc;
}

/*
** This function is called from within sqlite3changeset_apply_v2() when
** a conflict is encountered and resolved using conflict resolution
** mode eType (either SQLITE_CHANGESET_OMIT or SQLITE_CHANGESET_REPLACE)..
** It adds a conflict resolution record to the buffer in 
** SessionApplyCtx.rebase, which will eventually be returned to the caller
** of apply_v2() as the "rebase" buffer.
**
** Return SQLITE_OK if successful, or an SQLite error code otherwise.
*/
static int sessionRebaseAdd(
  SessionApplyCtx *p,             /* Apply context */
  int eType,                      /* Conflict resolution (OMIT or REPLACE) */
  sqlite3_changeset_iter *pIter   /* Iterator pointing at current change */
){
  int rc = SQLITE_OK;
  if( p->bRebase ){
    int i;
    int eOp = pIter->op;
    if( p->bRebaseStarted==0 ){
      /* Append a table-header to the rebase buffer */
      const char *zTab = pIter->zTab;
      sessionAppendByte(&p->rebase, 'T', &rc);
      sessionAppendVarint(&p->rebase, p->nCol, &rc);
      sessionAppendBlob(&p->rebase, p->abPK, p->nCol, &rc);
      sessionAppendBlob(&p->rebase, (u8*)zTab, (int)strlen(zTab)+1, &rc);
      p->bRebaseStarted = 1;
    }

    assert( eType==SQLITE_CHANGESET_REPLACE||eType==SQLITE_CHANGESET_OMIT );
    assert( eOp==SQLITE_DELETE || eOp==SQLITE_INSERT || eOp==SQLITE_UPDATE );

    sessionAppendByte(&p->rebase, 
        (eOp==SQLITE_DELETE ? SQLITE_DELETE : SQLITE_INSERT), &rc
        );
    sessionAppendByte(&p->rebase, (eType==SQLITE_CHANGESET_REPLACE), &rc);
    for(i=0; i<p->nCol; i++){
      sqlite3_value *pVal = 0;
      if( eOp==SQLITE_DELETE || (eOp==SQLITE_UPDATE && p->abPK[i]) ){
        sqlite3changeset_old(pIter, i, &pVal);
      }else{
        sqlite3changeset_new(pIter, i, &pVal);
      }
      sessionAppendValue(&p->rebase, pVal, &rc);
    }
  }
  return rc;
}

/*
** Invoke the conflict handler for the change that the changeset iterator
** currently points to.
**
** Argument eType must be either CHANGESET_DATA or CHANGESET_CONFLICT.
** If argument pbReplace is NULL, then the type of conflict handler invoked
** depends solely on eType, as follows:
**
**    eType value                 Value passed to xConflict
**    -------------------------------------------------
**    CHANGESET_DATA              CHANGESET_NOTFOUND
**    CHANGESET_CONFLICT          CHANGESET_CONSTRAINT
**
** Or, if pbReplace is not NULL, then an attempt is made to find an existing
** record with the same primary key as the record about to be deleted, updated
** or inserted. If such a record can be found, it is available to the conflict
** handler as the "conflicting" record. In this case the type of conflict
** handler invoked is as follows:
**
**    eType value         PK Record found?   Value passed to xConflict
**    ----------------------------------------------------------------
**    CHANGESET_DATA      Yes                CHANGESET_DATA
**    CHANGESET_DATA      No                 CHANGESET_NOTFOUND
**    CHANGESET_CONFLICT  Yes                CHANGESET_CONFLICT
**    CHANGESET_CONFLICT  No                 CHANGESET_CONSTRAINT
**
** If pbReplace is not NULL, and a record with a matching PK is found, and
** the conflict handler function returns SQLITE_CHANGESET_REPLACE, *pbReplace
** is set to non-zero before returning SQLITE_OK.
**
** If the conflict handler returns SQLITE_CHANGESET_ABORT, SQLITE_ABORT is
** returned. Or, if the conflict handler returns an invalid value, 
** SQLITE_MISUSE. If the conflict handler returns SQLITE_CHANGESET_OMIT,
** this function returns SQLITE_OK.
*/
static int sessionConflictHandler(
  int eType,                      /* Either CHANGESET_DATA or CONFLICT */
  SessionApplyCtx *p,             /* changeset_apply() context */
  sqlite3_changeset_iter *pIter,  /* Changeset iterator */
  int(*xConflict)(void *, int, sqlite3_changeset_iter*),
  void *pCtx,                     /* First argument for conflict handler */
  int *pbReplace                  /* OUT: Set to true if PK row is found */
){
  int res = 0;                    /* Value returned by conflict handler */
  int rc;
  int nCol;
  int op;
  const char *zDummy;

  sqlite3changeset_op(pIter, &zDummy, &nCol, &op, 0);

  assert( eType==SQLITE_CHANGESET_CONFLICT || eType==SQLITE_CHANGESET_DATA );
  assert( SQLITE_CHANGESET_CONFLICT+1==SQLITE_CHANGESET_CONSTRAINT );
  assert( SQLITE_CHANGESET_DATA+1==SQLITE_CHANGESET_NOTFOUND );

  /* Bind the new.* PRIMARY KEY values to the SELECT statement. */
  if( pbReplace ){
    rc = sessionSeekToRow(p->db, pIter, p->abPK, p->pSelect);
  }else{
    rc = SQLITE_OK;
  }

  if( rc==SQLITE_ROW ){
    /* There exists another row with the new.* primary key. */
    pIter->pConflict = p->pSelect;
    res = xConflict(pCtx, eType, pIter);
    pIter->pConflict = 0;
    rc = sqlite3_reset(p->pSelect);
  }else if( rc==SQLITE_OK ){
    if( p->bDeferConstraints && eType==SQLITE_CHANGESET_CONFLICT ){
      /* Instead of invoking the conflict handler, append the change blob
      ** to the SessionApplyCtx.constraints buffer. */
      u8 *aBlob = &pIter->in.aData[pIter->in.iCurrent];
      int nBlob = pIter->in.iNext - pIter->in.iCurrent;
      sessionAppendBlob(&p->constraints, aBlob, nBlob, &rc);
      return SQLITE_OK;
    }else{
      /* No other row with the new.* primary key. */
      res = xConflict(pCtx, eType+1, pIter);
      if( res==SQLITE_CHANGESET_REPLACE ) rc = SQLITE_MISUSE;
    }
  }

  if( rc==SQLITE_OK ){
    switch( res ){
      case SQLITE_CHANGESET_REPLACE:
        assert( pbReplace );
        *pbReplace = 1;
        break;

      case SQLITE_CHANGESET_OMIT:
        break;

      case SQLITE_CHANGESET_ABORT:
        rc = SQLITE_ABORT;
        break;

      default:
        rc = SQLITE_MISUSE;
        break;
    }
    if( rc==SQLITE_OK ){
      rc = sessionRebaseAdd(p, res, pIter);
    }
  }

  return rc;
}

/*
** Attempt to apply the change that the iterator passed as the first argument
** currently points to to the database. If a conflict is encountered, invoke
** the conflict handler callback.
**
** If argument pbRetry is NULL, then ignore any CHANGESET_DATA conflict. If
** one is encountered, update or delete the row with the matching primary key
** instead. Or, if pbRetry is not NULL and a CHANGESET_DATA conflict occurs,
** invoke the conflict handler. If it returns CHANGESET_REPLACE, set *pbRetry
** to true before returning. In this case the caller will invoke this function
** again, this time with pbRetry set to NULL.
**
** If argument pbReplace is NULL and a CHANGESET_CONFLICT conflict is 
** encountered invoke the conflict handler with CHANGESET_CONSTRAINT instead.
** Or, if pbReplace is not NULL, invoke it with CHANGESET_CONFLICT. If such
** an invocation returns SQLITE_CHANGESET_REPLACE, set *pbReplace to true
** before retrying. In this case the caller attempts to remove the conflicting
** row before invoking this function again, this time with pbReplace set 
** to NULL.
**
** If any conflict handler returns SQLITE_CHANGESET_ABORT, this function
** returns SQLITE_ABORT. Otherwise, if no error occurs, SQLITE_OK is 
** returned.
*/
static int sessionApplyOneOp(
  sqlite3_changeset_iter *pIter,  /* Changeset iterator */
  SessionApplyCtx *p,             /* changeset_apply() context */
  int(*xConflict)(void *, int, sqlite3_changeset_iter *),
  void *pCtx,                     /* First argument for the conflict handler */
  int *pbReplace,                 /* OUT: True to remove PK row and retry */
  int *pbRetry                    /* OUT: True to retry. */
){
  const char *zDummy;
  int op;
  int nCol;
  int rc = SQLITE_OK;

  assert( p->pDelete && p->pUpdate && p->pInsert && p->pSelect );
  assert( p->azCol && p->abPK );
  assert( !pbReplace || *pbReplace==0 );

  sqlite3changeset_op(pIter, &zDummy, &nCol, &op, 0);

  if( op==SQLITE_DELETE ){

    /* Bind values to the DELETE statement. If conflict handling is required,
    ** bind values for all columns and set bound variable (nCol+1) to true.
    ** Or, if conflict handling is not required, bind just the PK column
    ** values and, if it exists, set (nCol+1) to false. Conflict handling
    ** is not required if:
    **
    **   * this is a patchset, or
    **   * (pbRetry==0), or
    **   * all columns of the table are PK columns (in this case there is
    **     no (nCol+1) variable to bind to).
    */
    u8 *abPK = (pIter->bPatchset ? p->abPK : 0);
    rc = sessionBindRow(pIter, sqlite3changeset_old, nCol, abPK, p->pDelete);
    if( rc==SQLITE_OK && sqlite3_bind_parameter_count(p->pDelete)>nCol ){
      rc = sqlite3_bind_int(p->pDelete, nCol+1, (pbRetry==0 || abPK));
    }
    if( rc!=SQLITE_OK ) return rc;

    sqlite3_step(p->pDelete);
    rc = sqlite3_reset(p->pDelete);
    if( rc==SQLITE_OK && sqlite3_changes(p->db)==0 ){
      rc = sessionConflictHandler(
          SQLITE_CHANGESET_DATA, p, pIter, xConflict, pCtx, pbRetry
      );
    }else if( (rc&0xff)==SQLITE_CONSTRAINT ){
      rc = sessionConflictHandler(
          SQLITE_CHANGESET_CONFLICT, p, pIter, xConflict, pCtx, 0
      );
    }

  }else if( op==SQLITE_UPDATE ){
    int i;

    /* Bind values to the UPDATE statement. */
    for(i=0; rc==SQLITE_OK && i<nCol; i++){
      sqlite3_value *pOld = sessionChangesetOld(pIter, i);
      sqlite3_value *pNew = sessionChangesetNew(pIter, i);

      sqlite3_bind_int(p->pUpdate, i*3+2, !!pNew);
      if( pOld ){
        rc = sessionBindValue(p->pUpdate, i*3+1, pOld);
      }
      if( rc==SQLITE_OK && pNew ){
        rc = sessionBindValue(p->pUpdate, i*3+3, pNew);
      }
    }
    if( rc==SQLITE_OK ){
      sqlite3_bind_int(p->pUpdate, nCol*3+1, pbRetry==0 || pIter->bPatchset);
    }
    if( rc!=SQLITE_OK ) return rc;

    /* Attempt the UPDATE. In the case of a NOTFOUND or DATA conflict,
    ** the result will be SQLITE_OK with 0 rows modified. */
    sqlite3_step(p->pUpdate);
    rc = sqlite3_reset(p->pUpdate);

    if( rc==SQLITE_OK && sqlite3_changes(p->db)==0 ){
      /* A NOTFOUND or DATA error. Search the table to see if it contains
      ** a row with a matching primary key. If so, this is a DATA conflict.
      ** Otherwise, if there is no primary key match, it is a NOTFOUND. */

      rc = sessionConflictHandler(
          SQLITE_CHANGESET_DATA, p, pIter, xConflict, pCtx, pbRetry
      );

    }else if( (rc&0xff)==SQLITE_CONSTRAINT ){
      /* This is always a CONSTRAINT conflict. */
      rc = sessionConflictHandler(
          SQLITE_CHANGESET_CONFLICT, p, pIter, xConflict, pCtx, 0
      );
    }

  }else{
    assert( op==SQLITE_INSERT );
    if( p->bStat1 ){
      /* Check if there is a conflicting row. For sqlite_stat1, this needs
      ** to be done using a SELECT, as there is no PRIMARY KEY in the 
      ** database schema to throw an exception if a duplicate is inserted.  */
      rc = sessionSeekToRow(p->db, pIter, p->abPK, p->pSelect);
      if( rc==SQLITE_ROW ){
        rc = SQLITE_CONSTRAINT;
        sqlite3_reset(p->pSelect);
      }
    }

    if( rc==SQLITE_OK ){
      rc = sessionBindRow(pIter, sqlite3changeset_new, nCol, 0, p->pInsert);
      if( rc!=SQLITE_OK ) return rc;

      sqlite3_step(p->pInsert);
      rc = sqlite3_reset(p->pInsert);
    }

    if( (rc&0xff)==SQLITE_CONSTRAINT ){
      rc = sessionConflictHandler(
          SQLITE_CHANGESET_CONFLICT, p, pIter, xConflict, pCtx, pbReplace
      );
    }
  }

  return rc;
}

/*
** Attempt to apply the change that the iterator passed as the first argument
** currently points to to the database. If a conflict is encountered, invoke
** the conflict handler callback.
**
** The difference between this function and sessionApplyOne() is that this
** function handles the case where the conflict-handler is invoked and 
** returns SQLITE_CHANGESET_REPLACE - indicating that the change should be
** retried in some manner.
*/
static int sessionApplyOneWithRetry(
  sqlite3 *db,                    /* Apply change to "main" db of this handle */
  sqlite3_changeset_iter *pIter,  /* Changeset iterator to read change from */
  SessionApplyCtx *pApply,        /* Apply context */
  int(*xConflict)(void*, int, sqlite3_changeset_iter*),
  void *pCtx                      /* First argument passed to xConflict */
){
  int bReplace = 0;
  int bRetry = 0;
  int rc;

  rc = sessionApplyOneOp(pIter, pApply, xConflict, pCtx, &bReplace, &bRetry);
  if( rc==SQLITE_OK ){
    /* If the bRetry flag is set, the change has not been applied due to an
    ** SQLITE_CHANGESET_DATA problem (i.e. this is an UPDATE or DELETE and
    ** a row with the correct PK is present in the db, but one or more other
    ** fields do not contain the expected values) and the conflict handler 
    ** returned SQLITE_CHANGESET_REPLACE. In this case retry the operation,
    ** but pass NULL as the final argument so that sessionApplyOneOp() ignores
    ** the SQLITE_CHANGESET_DATA problem.  */
    if( bRetry ){
      assert( pIter->op==SQLITE_UPDATE || pIter->op==SQLITE_DELETE );
      rc = sessionApplyOneOp(pIter, pApply, xConflict, pCtx, 0, 0);
    }

    /* If the bReplace flag is set, the change is an INSERT that has not
    ** been performed because the database already contains a row with the
    ** specified primary key and the conflict handler returned
    ** SQLITE_CHANGESET_REPLACE. In this case remove the conflicting row
    ** before reattempting the INSERT.  */
    else if( bReplace ){
      assert( pIter->op==SQLITE_INSERT );
      rc = sqlite3_exec(db, "SAVEPOINT replace_op", 0, 0, 0);
      if( rc==SQLITE_OK ){
        rc = sessionBindRow(pIter, 
            sqlite3changeset_new, pApply->nCol, pApply->abPK, pApply->pDelete);
        sqlite3_bind_int(pApply->pDelete, pApply->nCol+1, 1);
      }
      if( rc==SQLITE_OK ){
        sqlite3_step(pApply->pDelete);
        rc = sqlite3_reset(pApply->pDelete);
      }
      if( rc==SQLITE_OK ){
        rc = sessionApplyOneOp(pIter, pApply, xConflict, pCtx, 0, 0);
      }
      if( rc==SQLITE_OK ){
        rc = sqlite3_exec(db, "RELEASE replace_op", 0, 0, 0);
      }
    }
  }

  return rc;
}

/*
** Retry the changes accumulated in the pApply->constraints buffer.
*/
static int sessionRetryConstraints(
  sqlite3 *db, 
  int bPatchset,
  const char *zTab,
  SessionApplyCtx *pApply,
  int(*xConflict)(void*, int, sqlite3_changeset_iter*),
  void *pCtx                      /* First argument passed to xConflict */
){
  int rc = SQLITE_OK;

  while( pApply->constraints.nBuf ){
    sqlite3_changeset_iter *pIter2 = 0;
    SessionBuffer cons = pApply->constraints;
    memset(&pApply->constraints, 0, sizeof(SessionBuffer));

    rc = sessionChangesetStart(&pIter2, 0, 0, cons.nBuf, cons.aBuf, 0);
    if( rc==SQLITE_OK ){
      size_t nByte = 2*pApply->nCol*sizeof(sqlite3_value*);
      int rc2;
      pIter2->bPatchset = bPatchset;
      pIter2->zTab = (char*)zTab;
      pIter2->nCol = pApply->nCol;
      pIter2->abPK = pApply->abPK;
      sessionBufferGrow(&pIter2->tblhdr, nByte, &rc);
      pIter2->apValue = (sqlite3_value**)pIter2->tblhdr.aBuf;
      if( rc==SQLITE_OK ) memset(pIter2->apValue, 0, nByte);

      while( rc==SQLITE_OK && SQLITE_ROW==sqlite3changeset_next(pIter2) ){
        rc = sessionApplyOneWithRetry(db, pIter2, pApply, xConflict, pCtx);
      }

      rc2 = sqlite3changeset_finalize(pIter2);
      if( rc==SQLITE_OK ) rc = rc2;
    }
    assert( pApply->bDeferConstraints || pApply->constraints.nBuf==0 );

    sqlite3_free(cons.aBuf);
    if( rc!=SQLITE_OK ) break;
    if( pApply->constraints.nBuf>=cons.nBuf ){
      /* No progress was made on the last round. */
      pApply->bDeferConstraints = 0;
    }
  }

  return rc;
}

/*
** Argument pIter is a changeset iterator that has been initialized, but
** not yet passed to sqlite3changeset_next(). This function applies the 
** changeset to the main database attached to handle "db". The supplied
** conflict handler callback is invoked to resolve any conflicts encountered
** while applying the change.
*/
static int sessionChangesetApply(
  sqlite3 *db,                    /* Apply change to "main" db of this handle */
  sqlite3_changeset_iter *pIter,  /* Changeset to apply */
  int(*xFilter)(
    void *pCtx,                   /* Copy of sixth arg to _apply() */
    const char *zTab              /* Table name */
  ),
  int(*xConflict)(
    void *pCtx,                   /* Copy of fifth arg to _apply() */
    int eConflict,                /* DATA, MISSING, CONFLICT, CONSTRAINT */
    sqlite3_changeset_iter *p     /* Handle describing change and conflict */
  ),
  void *pCtx,                     /* First argument passed to xConflict */
  void **ppRebase, int *pnRebase, /* OUT: Rebase information */
  int flags                       /* SESSION_APPLY_XXX flags */
){
  int schemaMismatch = 0;
  int rc = SQLITE_OK;             /* Return code */
  const char *zTab = 0;           /* Name of current table */
  int nTab = 0;                   /* Result of sqlite3Strlen30(zTab) */
  SessionApplyCtx sApply;         /* changeset_apply() context object */
  int bPatchset;

  assert( xConflict!=0 );

  pIter->in.bNoDiscard = 1;
  memset(&sApply, 0, sizeof(sApply));
  sApply.bRebase = (ppRebase && pnRebase);
  sqlite3_mutex_enter(sqlite3_db_mutex(db));
  if( (flags & SQLITE_CHANGESETAPPLY_NOSAVEPOINT)==0 ){
    rc = sqlite3_exec(db, "SAVEPOINT changeset_apply", 0, 0, 0);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3_exec(db, "PRAGMA defer_foreign_keys = 1", 0, 0, 0);
  }
  while( rc==SQLITE_OK && SQLITE_ROW==sqlite3changeset_next(pIter) ){
    int nCol;
    int op;
    const char *zNew;
    
    sqlite3changeset_op(pIter, &zNew, &nCol, &op, 0);

    if( zTab==0 || sqlite3_strnicmp(zNew, zTab, nTab+1) ){
      u8 *abPK;

      rc = sessionRetryConstraints(
          db, pIter->bPatchset, zTab, &sApply, xConflict, pCtx
      );
      if( rc!=SQLITE_OK ) break;

      sqlite3_free((char*)sApply.azCol);  /* cast works around VC++ bug */
      sqlite3_finalize(sApply.pDelete);
      sqlite3_finalize(sApply.pUpdate); 
      sqlite3_finalize(sApply.pInsert);
      sqlite3_finalize(sApply.pSelect);
      sApply.db = db;
      sApply.pDelete = 0;
      sApply.pUpdate = 0;
      sApply.pInsert = 0;
      sApply.pSelect = 0;
      sApply.nCol = 0;
      sApply.azCol = 0;
      sApply.abPK = 0;
      sApply.bStat1 = 0;
      sApply.bDeferConstraints = 1;
      sApply.bRebaseStarted = 0;
      memset(&sApply.constraints, 0, sizeof(SessionBuffer));

      /* If an xFilter() callback was specified, invoke it now. If the 
      ** xFilter callback returns zero, skip this table. If it returns
      ** non-zero, proceed. */
      schemaMismatch = (xFilter && (0==xFilter(pCtx, zNew)));
      if( schemaMismatch ){
        zTab = sqlite3_mprintf("%s", zNew);
        if( zTab==0 ){
          rc = SQLITE_NOMEM;
          break;
        }
        nTab = (int)strlen(zTab);
        sApply.azCol = (const char **)zTab;
      }else{
        int nMinCol = 0;
        int i;

        sqlite3changeset_pk(pIter, &abPK, 0);
        rc = sessionTableInfo(
            db, "main", zNew, &sApply.nCol, &zTab, &sApply.azCol, &sApply.abPK
        );
        if( rc!=SQLITE_OK ) break;
        for(i=0; i<sApply.nCol; i++){
          if( sApply.abPK[i] ) nMinCol = i+1;
        }
  
        if( sApply.nCol==0 ){
          schemaMismatch = 1;
          sqlite3_log(SQLITE_SCHEMA, 
              "sqlite3changeset_apply(): no such table: %s", zTab
          );
        }
        else if( sApply.nCol<nCol ){
          schemaMismatch = 1;
          sqlite3_log(SQLITE_SCHEMA, 
              "sqlite3changeset_apply(): table %s has %d columns, "
              "expected %d or more", 
              zTab, sApply.nCol, nCol
          );
        }
        else if( nCol<nMinCol || memcmp(sApply.abPK, abPK, nCol)!=0 ){
          schemaMismatch = 1;
          sqlite3_log(SQLITE_SCHEMA, "sqlite3changeset_apply(): "
              "primary key mismatch for table %s", zTab
          );
        }
        else{
          sApply.nCol = nCol;
          if( 0==sqlite3_stricmp(zTab, "sqlite_stat1") ){
            if( (rc = sessionStat1Sql(db, &sApply) ) ){
              break;
            }
            sApply.bStat1 = 1;
          }else{
            if((rc = sessionSelectRow(db, zTab, &sApply))
                || (rc = sessionUpdateRow(db, zTab, &sApply))
                || (rc = sessionDeleteRow(db, zTab, &sApply))
                || (rc = sessionInsertRow(db, zTab, &sApply))
              ){
              break;
            }
            sApply.bStat1 = 0;
          }
        }
        nTab = sqlite3Strlen30(zTab);
      }
    }

    /* If there is a schema mismatch on the current table, proceed to the
    ** next change. A log message has already been issued. */
    if( schemaMismatch ) continue;

    rc = sessionApplyOneWithRetry(db, pIter, &sApply, xConflict, pCtx);
  }

  bPatchset = pIter->bPatchset;
  if( rc==SQLITE_OK ){
    rc = sqlite3changeset_finalize(pIter);
  }else{
    sqlite3changeset_finalize(pIter);
  }

  if( rc==SQLITE_OK ){
    rc = sessionRetryConstraints(db, bPatchset, zTab, &sApply, xConflict, pCtx);
  }

  if( rc==SQLITE_OK ){
    int nFk, notUsed;
    sqlite3_db_status(db, SQLITE_DBSTATUS_DEFERRED_FKS, &nFk, &notUsed, 0);
    if( nFk!=0 ){
      int res = SQLITE_CHANGESET_ABORT;
      sqlite3_changeset_iter sIter;
      memset(&sIter, 0, sizeof(sIter));
      sIter.nCol = nFk;
      res = xConflict(pCtx, SQLITE_CHANGESET_FOREIGN_KEY, &sIter);
      if( res!=SQLITE_CHANGESET_OMIT ){
        rc = SQLITE_CONSTRAINT;
      }
    }
  }
  sqlite3_exec(db, "PRAGMA defer_foreign_keys = 0", 0, 0, 0);

  if( (flags & SQLITE_CHANGESETAPPLY_NOSAVEPOINT)==0 ){
    if( rc==SQLITE_OK ){
      rc = sqlite3_exec(db, "RELEASE changeset_apply", 0, 0, 0);
    }else{
      sqlite3_exec(db, "ROLLBACK TO changeset_apply", 0, 0, 0);
      sqlite3_exec(db, "RELEASE changeset_apply", 0, 0, 0);
    }
  }

  assert( sApply.bRebase || sApply.rebase.nBuf==0 );
  if( rc==SQLITE_OK && bPatchset==0 && sApply.bRebase ){
    *ppRebase = (void*)sApply.rebase.aBuf;
    *pnRebase = sApply.rebase.nBuf;
    sApply.rebase.aBuf = 0;
  }
  sqlite3_finalize(sApply.pInsert);
  sqlite3_finalize(sApply.pDelete);
  sqlite3_finalize(sApply.pUpdate);
  sqlite3_finalize(sApply.pSelect);
  sqlite3_free((char*)sApply.azCol);  /* cast works around VC++ bug */
  sqlite3_free((char*)sApply.constraints.aBuf);
  sqlite3_free((char*)sApply.rebase.aBuf);
  sqlite3_mutex_leave(sqlite3_db_mutex(db));
  return rc;
}

/*
** Apply the changeset passed via pChangeset/nChangeset to the main 
** database attached to handle "db".
*/
SQLITE_API int sqlite3changeset_apply_v2(
  sqlite3 *db,                    /* Apply change to "main" db of this handle */
  int nChangeset,                 /* Size of changeset in bytes */
  void *pChangeset,               /* Changeset blob */
  int(*xFilter)(
    void *pCtx,                   /* Copy of sixth arg to _apply() */
    const char *zTab              /* Table name */
  ),
  int(*xConflict)(
    void *pCtx,                   /* Copy of sixth arg to _apply() */
    int eConflict,                /* DATA, MISSING, CONFLICT, CONSTRAINT */
    sqlite3_changeset_iter *p     /* Handle describing change and conflict */
  ),
  void *pCtx,                     /* First argument passed to xConflict */
  void **ppRebase, int *pnRebase,
  int flags
){
  sqlite3_changeset_iter *pIter;  /* Iterator to skip through changeset */  
  int bInverse = !!(flags & SQLITE_CHANGESETAPPLY_INVERT);
  int rc = sessionChangesetStart(&pIter, 0, 0, nChangeset, pChangeset,bInverse);
  if( rc==SQLITE_OK ){
    rc = sessionChangesetApply(
        db, pIter, xFilter, xConflict, pCtx, ppRebase, pnRebase, flags
    );
  }
  return rc;
}

/*
** Apply the changeset passed via pChangeset/nChangeset to the main database
** attached to handle "db". Invoke the supplied conflict handler callback
** to resolve any conflicts encountered while applying the change.
*/
SQLITE_API int sqlite3changeset_apply(
  sqlite3 *db,                    /* Apply change to "main" db of this handle */
  int nChangeset,                 /* Size of changeset in bytes */
  void *pChangeset,               /* Changeset blob */
  int(*xFilter)(
    void *pCtx,                   /* Copy of sixth arg to _apply() */
    const char *zTab              /* Table name */
  ),
  int(*xConflict)(
    void *pCtx,                   /* Copy of fifth arg to _apply() */
    int eConflict,                /* DATA, MISSING, CONFLICT, CONSTRAINT */
    sqlite3_changeset_iter *p     /* Handle describing change and conflict */
  ),
  void *pCtx                      /* First argument passed to xConflict */
){
  return sqlite3changeset_apply_v2(
      db, nChangeset, pChangeset, xFilter, xConflict, pCtx, 0, 0, 0
  );
}

/*
** Apply the changeset passed via xInput/pIn to the main database
** attached to handle "db". Invoke the supplied conflict handler callback
** to resolve any conflicts encountered while applying the change.
*/
SQLITE_API int sqlite3changeset_apply_v2_strm(
  sqlite3 *db,                    /* Apply change to "main" db of this handle */
  int (*xInput)(void *pIn, void *pData, int *pnData), /* Input function */
  void *pIn,                                          /* First arg for xInput */
  int(*xFilter)(
    void *pCtx,                   /* Copy of sixth arg to _apply() */
    const char *zTab              /* Table name */
  ),
  int(*xConflict)(
    void *pCtx,                   /* Copy of sixth arg to _apply() */
    int eConflict,                /* DATA, MISSING, CONFLICT, CONSTRAINT */
    sqlite3_changeset_iter *p     /* Handle describing change and conflict */
  ),
  void *pCtx,                     /* First argument passed to xConflict */
  void **ppRebase, int *pnRebase,
  int flags
){
  sqlite3_changeset_iter *pIter;  /* Iterator to skip through changeset */  
  int bInverse = !!(flags & SQLITE_CHANGESETAPPLY_INVERT);
  int rc = sessionChangesetStart(&pIter, xInput, pIn, 0, 0, bInverse);
  if( rc==SQLITE_OK ){
    rc = sessionChangesetApply(
        db, pIter, xFilter, xConflict, pCtx, ppRebase, pnRebase, flags
    );
  }
  return rc;
}
SQLITE_API int sqlite3changeset_apply_strm(
  sqlite3 *db,                    /* Apply change to "main" db of this handle */
  int (*xInput)(void *pIn, void *pData, int *pnData), /* Input function */
  void *pIn,                                          /* First arg for xInput */
  int(*xFilter)(
    void *pCtx,                   /* Copy of sixth arg to _apply() */
    const char *zTab              /* Table name */
  ),
  int(*xConflict)(
    void *pCtx,                   /* Copy of sixth arg to _apply() */
    int eConflict,                /* DATA, MISSING, CONFLICT, CONSTRAINT */
    sqlite3_changeset_iter *p     /* Handle describing change and conflict */
  ),
  void *pCtx                      /* First argument passed to xConflict */
){
  return sqlite3changeset_apply_v2_strm(
      db, xInput, pIn, xFilter, xConflict, pCtx, 0, 0, 0
  );
}

/*
** sqlite3_changegroup handle.
*/
struct sqlite3_changegroup {
  int rc;                         /* Error code */
  int bPatch;                     /* True to accumulate patchsets */
  SessionTable *pList;            /* List of tables in current patch */
};

/*
** This function is called to merge two changes to the same row together as
** part of an sqlite3changeset_concat() operation. A new change object is
** allocated and a pointer to it stored in *ppNew.
*/
static int sessionChangeMerge(
  SessionTable *pTab,             /* Table structure */
  int bRebase,                    /* True for a rebase hash-table */
  int bPatchset,                  /* True for patchsets */
  SessionChange *pExist,          /* Existing change */
  int op2,                        /* Second change operation */
  int bIndirect,                  /* True if second change is indirect */
  u8 *aRec,                       /* Second change record */
  int nRec,                       /* Number of bytes in aRec */
  SessionChange **ppNew           /* OUT: Merged change */
){
  SessionChange *pNew = 0;
  int rc = SQLITE_OK;

  if( !pExist ){
    pNew = (SessionChange *)sqlite3_malloc64(sizeof(SessionChange) + nRec);
    if( !pNew ){
      return SQLITE_NOMEM;
    }
    memset(pNew, 0, sizeof(SessionChange));
    pNew->op = op2;
    pNew->bIndirect = bIndirect;
    pNew->aRecord = (u8*)&pNew[1];
    if( bIndirect==0 || bRebase==0 ){
      pNew->nRecord = nRec;
      memcpy(pNew->aRecord, aRec, nRec);
    }else{
      int i;
      u8 *pIn = aRec;
      u8 *pOut = pNew->aRecord;
      for(i=0; i<pTab->nCol; i++){
        int nIn = sessionSerialLen(pIn);
        if( *pIn==0 ){
          *pOut++ = 0;
        }else if( pTab->abPK[i]==0 ){
          *pOut++ = 0xFF;
        }else{
          memcpy(pOut, pIn, nIn);
          pOut += nIn;
        }
        pIn += nIn;
      }
      pNew->nRecord = pOut - pNew->aRecord;
    }
  }else if( bRebase ){
    if( pExist->op==SQLITE_DELETE && pExist->bIndirect ){
      *ppNew = pExist;
    }else{
      sqlite3_int64 nByte = nRec + pExist->nRecord + sizeof(SessionChange);
      pNew = (SessionChange*)sqlite3_malloc64(nByte);
      if( pNew==0 ){
        rc = SQLITE_NOMEM;
      }else{
        int i;
        u8 *a1 = pExist->aRecord;
        u8 *a2 = aRec;
        u8 *pOut;

        memset(pNew, 0, nByte);
        pNew->bIndirect = bIndirect || pExist->bIndirect;
        pNew->op = op2;
        pOut = pNew->aRecord = (u8*)&pNew[1];

        for(i=0; i<pTab->nCol; i++){
          int n1 = sessionSerialLen(a1);
          int n2 = sessionSerialLen(a2);
          if( *a1==0xFF || (pTab->abPK[i]==0 && bIndirect) ){
            *pOut++ = 0xFF;
          }else if( *a2==0 ){
            memcpy(pOut, a1, n1);
            pOut += n1;
          }else{
            memcpy(pOut, a2, n2);
            pOut += n2;
          }
          a1 += n1;
          a2 += n2;
        }
        pNew->nRecord = pOut - pNew->aRecord;
      }
      sqlite3_free(pExist);
    }
  }else{
    int op1 = pExist->op;

    /* 
    **   op1=INSERT, op2=INSERT      ->      Unsupported. Discard op2.
    **   op1=INSERT, op2=UPDATE      ->      INSERT.
    **   op1=INSERT, op2=DELETE      ->      (none)
    **
    **   op1=UPDATE, op2=INSERT      ->      Unsupported. Discard op2.
    **   op1=UPDATE, op2=UPDATE      ->      UPDATE.
    **   op1=UPDATE, op2=DELETE      ->      DELETE.
    **
    **   op1=DELETE, op2=INSERT      ->      UPDATE.
    **   op1=DELETE, op2=UPDATE      ->      Unsupported. Discard op2.
    **   op1=DELETE, op2=DELETE      ->      Unsupported. Discard op2.
    */   
    if( (op1==SQLITE_INSERT && op2==SQLITE_INSERT)
     || (op1==SQLITE_UPDATE && op2==SQLITE_INSERT)
     || (op1==SQLITE_DELETE && op2==SQLITE_UPDATE)
     || (op1==SQLITE_DELETE && op2==SQLITE_DELETE)
    ){
      pNew = pExist;
    }else if( op1==SQLITE_INSERT && op2==SQLITE_DELETE ){
      sqlite3_free(pExist);
      assert( pNew==0 );
    }else{
      u8 *aExist = pExist->aRecord;
      sqlite3_int64 nByte;
      u8 *aCsr;

      /* Allocate a new SessionChange object. Ensure that the aRecord[]
      ** buffer of the new object is large enough to hold any record that
      ** may be generated by combining the input records.  */
      nByte = sizeof(SessionChange) + pExist->nRecord + nRec;
      pNew = (SessionChange *)sqlite3_malloc64(nByte);
      if( !pNew ){
        sqlite3_free(pExist);
        return SQLITE_NOMEM;
      }
      memset(pNew, 0, sizeof(SessionChange));
      pNew->bIndirect = (bIndirect && pExist->bIndirect);
      aCsr = pNew->aRecord = (u8 *)&pNew[1];

      if( op1==SQLITE_INSERT ){             /* INSERT + UPDATE */
        u8 *a1 = aRec;
        assert( op2==SQLITE_UPDATE );
        pNew->op = SQLITE_INSERT;
        if( bPatchset==0 ) sessionSkipRecord(&a1, pTab->nCol);
        sessionMergeRecord(&aCsr, pTab->nCol, aExist, a1);
      }else if( op1==SQLITE_DELETE ){       /* DELETE + INSERT */
        assert( op2==SQLITE_INSERT );
        pNew->op = SQLITE_UPDATE;
        if( bPatchset ){
          memcpy(aCsr, aRec, nRec);
          aCsr += nRec;
        }else{
          if( 0==sessionMergeUpdate(&aCsr, pTab, bPatchset, aExist, 0,aRec,0) ){
            sqlite3_free(pNew);
            pNew = 0;
          }
        }
      }else if( op2==SQLITE_UPDATE ){       /* UPDATE + UPDATE */
        u8 *a1 = aExist;
        u8 *a2 = aRec;
        assert( op1==SQLITE_UPDATE );
        if( bPatchset==0 ){
          sessionSkipRecord(&a1, pTab->nCol);
          sessionSkipRecord(&a2, pTab->nCol);
        }
        pNew->op = SQLITE_UPDATE;
        if( 0==sessionMergeUpdate(&aCsr, pTab, bPatchset, aRec, aExist,a1,a2) ){
          sqlite3_free(pNew);
          pNew = 0;
        }
      }else{                                /* UPDATE + DELETE */
        assert( op1==SQLITE_UPDATE && op2==SQLITE_DELETE );
        pNew->op = SQLITE_DELETE;
        if( bPatchset ){
          memcpy(aCsr, aRec, nRec);
          aCsr += nRec;
        }else{
          sessionMergeRecord(&aCsr, pTab->nCol, aRec, aExist);
        }
      }

      if( pNew ){
        pNew->nRecord = (int)(aCsr - pNew->aRecord);
      }
      sqlite3_free(pExist);
    }
  }

  *ppNew = pNew;
  return rc;
}

/*
** Add all changes in the changeset traversed by the iterator passed as
** the first argument to the changegroup hash tables.
*/
static int sessionChangesetToHash(
  sqlite3_changeset_iter *pIter,   /* Iterator to read from */
  sqlite3_changegroup *pGrp,       /* Changegroup object to add changeset to */
  int bRebase                      /* True if hash table is for rebasing */
){
  u8 *aRec;
  int nRec;
  int rc = SQLITE_OK;
  SessionTable *pTab = 0;

  while( SQLITE_ROW==sessionChangesetNext(pIter, &aRec, &nRec, 0) ){
    const char *zNew;
    int nCol;
    int op;
    int iHash;
    int bIndirect;
    SessionChange *pChange;
    SessionChange *pExist = 0;
    SessionChange **pp;

    if( pGrp->pList==0 ){
      pGrp->bPatch = pIter->bPatchset;
    }else if( pIter->bPatchset!=pGrp->bPatch ){
      rc = SQLITE_ERROR;
      break;
    }

    sqlite3changeset_op(pIter, &zNew, &nCol, &op, &bIndirect);
    if( !pTab || sqlite3_stricmp(zNew, pTab->zName) ){
      /* Search the list for a matching table */
      int nNew = (int)strlen(zNew);
      u8 *abPK;

      sqlite3changeset_pk(pIter, &abPK, 0);
      for(pTab = pGrp->pList; pTab; pTab=pTab->pNext){
        if( 0==sqlite3_strnicmp(pTab->zName, zNew, nNew+1) ) break;
      }
      if( !pTab ){
        SessionTable **ppTab;

        pTab = sqlite3_malloc64(sizeof(SessionTable) + nCol + nNew+1);
        if( !pTab ){
          rc = SQLITE_NOMEM;
          break;
        }
        memset(pTab, 0, sizeof(SessionTable));
        pTab->nCol = nCol;
        pTab->abPK = (u8*)&pTab[1];
        memcpy(pTab->abPK, abPK, nCol);
        pTab->zName = (char*)&pTab->abPK[nCol];
        memcpy(pTab->zName, zNew, nNew+1);

        /* The new object must be linked on to the end of the list, not
        ** simply added to the start of it. This is to ensure that the
        ** tables within the output of sqlite3changegroup_output() are in 
        ** the right order.  */
        for(ppTab=&pGrp->pList; *ppTab; ppTab=&(*ppTab)->pNext);
        *ppTab = pTab;
      }else if( pTab->nCol!=nCol || memcmp(pTab->abPK, abPK, nCol) ){
        rc = SQLITE_SCHEMA;
        break;
      }
    }

    if( sessionGrowHash(pIter->bPatchset, pTab) ){
      rc = SQLITE_NOMEM;
      break;
    }
    iHash = sessionChangeHash(
        pTab, (pIter->bPatchset && op==SQLITE_DELETE), aRec, pTab->nChange
    );

    /* Search for existing entry. If found, remove it from the hash table. 
    ** Code below may link it back in.
    */
    for(pp=&pTab->apChange[iHash]; *pp; pp=&(*pp)->pNext){
      int bPkOnly1 = 0;
      int bPkOnly2 = 0;
      if( pIter->bPatchset ){
        bPkOnly1 = (*pp)->op==SQLITE_DELETE;
        bPkOnly2 = op==SQLITE_DELETE;
      }
      if( sessionChangeEqual(pTab, bPkOnly1, (*pp)->aRecord, bPkOnly2, aRec) ){
        pExist = *pp;
        *pp = (*pp)->pNext;
        pTab->nEntry--;
        break;
      }
    }

    rc = sessionChangeMerge(pTab, bRebase, 
        pIter->bPatchset, pExist, op, bIndirect, aRec, nRec, &pChange
    );
    if( rc ) break;
    if( pChange ){
      pChange->pNext = pTab->apChange[iHash];
      pTab->apChange[iHash] = pChange;
      pTab->nEntry++;
    }
  }

  if( rc==SQLITE_OK ) rc = pIter->rc;
  return rc;
}

/*
** Serialize a changeset (or patchset) based on all changesets (or patchsets)
** added to the changegroup object passed as the first argument.
**
** If xOutput is not NULL, then the changeset/patchset is returned to the
** user via one or more calls to xOutput, as with the other streaming
** interfaces. 
**
** Or, if xOutput is NULL, then (*ppOut) is populated with a pointer to a
** buffer containing the output changeset before this function returns. In
** this case (*pnOut) is set to the size of the output buffer in bytes. It
** is the responsibility of the caller to free the output buffer using
** sqlite3_free() when it is no longer required.
**
** If successful, SQLITE_OK is returned. Or, if an error occurs, an SQLite
** error code. If an error occurs and xOutput is NULL, (*ppOut) and (*pnOut)
** are both set to 0 before returning.
*/
static int sessionChangegroupOutput(
  sqlite3_changegroup *pGrp,
  int (*xOutput)(void *pOut, const void *pData, int nData),
  void *pOut,
  int *pnOut,
  void **ppOut
){
  int rc = SQLITE_OK;
  SessionBuffer buf = {0, 0, 0};
  SessionTable *pTab;
  assert( xOutput==0 || (ppOut==0 && pnOut==0) );

  /* Create the serialized output changeset based on the contents of the
  ** hash tables attached to the SessionTable objects in list p->pList. 
  */
  for(pTab=pGrp->pList; rc==SQLITE_OK && pTab; pTab=pTab->pNext){
    int i;
    if( pTab->nEntry==0 ) continue;

    sessionAppendTableHdr(&buf, pGrp->bPatch, pTab, &rc);
    for(i=0; i<pTab->nChange; i++){
      SessionChange *p;
      for(p=pTab->apChange[i]; p; p=p->pNext){
        sessionAppendByte(&buf, p->op, &rc);
        sessionAppendByte(&buf, p->bIndirect, &rc);
        sessionAppendBlob(&buf, p->aRecord, p->nRecord, &rc);
        if( rc==SQLITE_OK && xOutput && buf.nBuf>=sessions_strm_chunk_size ){
          rc = xOutput(pOut, buf.aBuf, buf.nBuf);
          buf.nBuf = 0;
        }
      }
    }
  }

  if( rc==SQLITE_OK ){
    if( xOutput ){
      if( buf.nBuf>0 ) rc = xOutput(pOut, buf.aBuf, buf.nBuf);
    }else{
      *ppOut = buf.aBuf;
      *pnOut = buf.nBuf;
      buf.aBuf = 0;
    }
  }
  sqlite3_free(buf.aBuf);

  return rc;
}

/*
** Allocate a new, empty, sqlite3_changegroup.
*/
SQLITE_API int sqlite3changegroup_new(sqlite3_changegroup **pp){
  int rc = SQLITE_OK;             /* Return code */
  sqlite3_changegroup *p;         /* New object */
  p = (sqlite3_changegroup*)sqlite3_malloc(sizeof(sqlite3_changegroup));
  if( p==0 ){
    rc = SQLITE_NOMEM;
  }else{
    memset(p, 0, sizeof(sqlite3_changegroup));
  }
  *pp = p;
  return rc;
}

/*
** Add the changeset currently stored in buffer pData, size nData bytes,
** to changeset-group p.
*/
SQLITE_API int sqlite3changegroup_add(sqlite3_changegroup *pGrp, int nData, void *pData){
  sqlite3_changeset_iter *pIter;  /* Iterator opened on pData/nData */
  int rc;                         /* Return code */

  rc = sqlite3changeset_start(&pIter, nData, pData);
  if( rc==SQLITE_OK ){
    rc = sessionChangesetToHash(pIter, pGrp, 0);
  }
  sqlite3changeset_finalize(pIter);
  return rc;
}

/*
** Obtain a buffer containing a changeset representing the concatenation
** of all changesets added to the group so far.
*/
SQLITE_API int sqlite3changegroup_output(
    sqlite3_changegroup *pGrp,
    int *pnData,
    void **ppData
){
  return sessionChangegroupOutput(pGrp, 0, 0, pnData, ppData);
}

/*
** Streaming versions of changegroup_add().
*/
SQLITE_API int sqlite3changegroup_add_strm(
  sqlite3_changegroup *pGrp,
  int (*xInput)(void *pIn, void *pData, int *pnData),
  void *pIn
){
  sqlite3_changeset_iter *pIter;  /* Iterator opened on pData/nData */
  int rc;                         /* Return code */

  rc = sqlite3changeset_start_strm(&pIter, xInput, pIn);
  if( rc==SQLITE_OK ){
    rc = sessionChangesetToHash(pIter, pGrp, 0);
  }
  sqlite3changeset_finalize(pIter);
  return rc;
}

/*
** Streaming versions of changegroup_output().
*/
SQLITE_API int sqlite3changegroup_output_strm(
  sqlite3_changegroup *pGrp,
  int (*xOutput)(void *pOut, const void *pData, int nData), 
  void *pOut
){
  return sessionChangegroupOutput(pGrp, xOutput, pOut, 0, 0);
}

/*
** Delete a changegroup object.
*/
SQLITE_API void sqlite3changegroup_delete(sqlite3_changegroup *pGrp){
  if( pGrp ){
    sessionDeleteTable(pGrp->pList);
    sqlite3_free(pGrp);
  }
}

/* 
** Combine two changesets together.
*/
SQLITE_API int sqlite3changeset_concat(
  int nLeft,                      /* Number of bytes in lhs input */
  void *pLeft,                    /* Lhs input changeset */
  int nRight                      /* Number of bytes in rhs input */,
  void *pRight,                   /* Rhs input changeset */
  int *pnOut,                     /* OUT: Number of bytes in output changeset */
  void **ppOut                    /* OUT: changeset (left <concat> right) */
){
  sqlite3_changegroup *pGrp;
  int rc;

  rc = sqlite3changegroup_new(&pGrp);
  if( rc==SQLITE_OK ){
    rc = sqlite3changegroup_add(pGrp, nLeft, pLeft);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3changegroup_add(pGrp, nRight, pRight);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3changegroup_output(pGrp, pnOut, ppOut);
  }
  sqlite3changegroup_delete(pGrp);

  return rc;
}

/*
** Streaming version of sqlite3changeset_concat().
*/
SQLITE_API int sqlite3changeset_concat_strm(
  int (*xInputA)(void *pIn, void *pData, int *pnData),
  void *pInA,
  int (*xInputB)(void *pIn, void *pData, int *pnData),
  void *pInB,
  int (*xOutput)(void *pOut, const void *pData, int nData),
  void *pOut
){
  sqlite3_changegroup *pGrp;
  int rc;

  rc = sqlite3changegroup_new(&pGrp);
  if( rc==SQLITE_OK ){
    rc = sqlite3changegroup_add_strm(pGrp, xInputA, pInA);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3changegroup_add_strm(pGrp, xInputB, pInB);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3changegroup_output_strm(pGrp, xOutput, pOut);
  }
  sqlite3changegroup_delete(pGrp);

  return rc;
}

/*
** Changeset rebaser handle.
*/
struct sqlite3_rebaser {
  sqlite3_changegroup grp;        /* Hash table */
};

/*
** Buffers a1 and a2 must both contain a sessions module record nCol
** fields in size. This function appends an nCol sessions module 
** record to buffer pBuf that is a copy of a1, except that for
** each field that is undefined in a1[], swap in the field from a2[].
*/
static void sessionAppendRecordMerge(
  SessionBuffer *pBuf,            /* Buffer to append to */
  int nCol,                       /* Number of columns in each record */
  u8 *a1, int n1,                 /* Record 1 */
  u8 *a2, int n2,                 /* Record 2 */
  int *pRc                        /* IN/OUT: error code */
){
  sessionBufferGrow(pBuf, n1+n2, pRc);
  if( *pRc==SQLITE_OK ){
    int i;
    u8 *pOut = &pBuf->aBuf[pBuf->nBuf];
    for(i=0; i<nCol; i++){
      int nn1 = sessionSerialLen(a1);
      int nn2 = sessionSerialLen(a2);
      if( *a1==0 || *a1==0xFF ){
        memcpy(pOut, a2, nn2);
        pOut += nn2;
      }else{
        memcpy(pOut, a1, nn1);
        pOut += nn1;
      }
      a1 += nn1;
      a2 += nn2;
    }

    pBuf->nBuf = pOut-pBuf->aBuf;
    assert( pBuf->nBuf<=pBuf->nAlloc );
  }
}

/*
** This function is called when rebasing a local UPDATE change against one 
** or more remote UPDATE changes. The aRec/nRec buffer contains the current
** old.* and new.* records for the change. The rebase buffer (a single
** record) is in aChange/nChange. The rebased change is appended to buffer
** pBuf.
**
** Rebasing the UPDATE involves: 
**
**   * Removing any changes to fields for which the corresponding field
**     in the rebase buffer is set to "replaced" (type 0xFF). If this
**     means the UPDATE change updates no fields, nothing is appended
**     to the output buffer.
**
**   * For each field modified by the local change for which the 
**     corresponding field in the rebase buffer is not "undefined" (0x00)
**     or "replaced" (0xFF), the old.* value is replaced by the value
**     in the rebase buffer.
*/
static void sessionAppendPartialUpdate(
  SessionBuffer *pBuf,            /* Append record here */
  sqlite3_changeset_iter *pIter,  /* Iterator pointed at local change */
  u8 *aRec, int nRec,             /* Local change */
  u8 *aChange, int nChange,       /* Record to rebase against */
  int *pRc                        /* IN/OUT: Return Code */
){
  sessionBufferGrow(pBuf, 2+nRec+nChange, pRc);
  if( *pRc==SQLITE_OK ){
    int bData = 0;
    u8 *pOut = &pBuf->aBuf[pBuf->nBuf];
    int i;
    u8 *a1 = aRec;
    u8 *a2 = aChange;

    *pOut++ = SQLITE_UPDATE;
    *pOut++ = pIter->bIndirect;
    for(i=0; i<pIter->nCol; i++){
      int n1 = sessionSerialLen(a1);
      int n2 = sessionSerialLen(a2);
      if( pIter->abPK[i] || a2[0]==0 ){
        if( !pIter->abPK[i] ) bData = 1;
        memcpy(pOut, a1, n1);
        pOut += n1;
      }else if( a2[0]!=0xFF ){
        bData = 1;
        memcpy(pOut, a2, n2);
        pOut += n2;
      }else{
        *pOut++ = '\0';
      }
      a1 += n1;
      a2 += n2;
    }
    if( bData ){
      a2 = aChange;
      for(i=0; i<pIter->nCol; i++){
        int n1 = sessionSerialLen(a1);
        int n2 = sessionSerialLen(a2);
        if( pIter->abPK[i] || a2[0]!=0xFF ){
          memcpy(pOut, a1, n1);
          pOut += n1;
        }else{
          *pOut++ = '\0';
        }
        a1 += n1;
        a2 += n2;
      }
      pBuf->nBuf = (pOut - pBuf->aBuf);
    }
  }
}

/*
** pIter is configured to iterate through a changeset. This function rebases 
** that changeset according to the current configuration of the rebaser 
** object passed as the first argument. If no error occurs and argument xOutput
** is not NULL, then the changeset is returned to the caller by invoking
** xOutput zero or more times and SQLITE_OK returned. Or, if xOutput is NULL,
** then (*ppOut) is set to point to a buffer containing the rebased changeset
** before this function returns. In this case (*pnOut) is set to the size of
** the buffer in bytes.  It is the responsibility of the caller to eventually
** free the (*ppOut) buffer using sqlite3_free(). 
**
** If an error occurs, an SQLite error code is returned. If ppOut and
** pnOut are not NULL, then the two output parameters are set to 0 before
** returning.
*/
static int sessionRebase(
  sqlite3_rebaser *p,             /* Rebaser hash table */
  sqlite3_changeset_iter *pIter,  /* Input data */
  int (*xOutput)(void *pOut, const void *pData, int nData),
  void *pOut,                     /* Context for xOutput callback */
  int *pnOut,                     /* OUT: Number of bytes in output changeset */
  void **ppOut                    /* OUT: Inverse of pChangeset */
){
  int rc = SQLITE_OK;
  u8 *aRec = 0;
  int nRec = 0;
  int bNew = 0;
  SessionTable *pTab = 0;
  SessionBuffer sOut = {0,0,0};

  while( SQLITE_ROW==sessionChangesetNext(pIter, &aRec, &nRec, &bNew) ){
    SessionChange *pChange = 0;
    int bDone = 0;

    if( bNew ){
      const char *zTab = pIter->zTab;
      for(pTab=p->grp.pList; pTab; pTab=pTab->pNext){
        if( 0==sqlite3_stricmp(pTab->zName, zTab) ) break;
      }
      bNew = 0;

      /* A patchset may not be rebased */
      if( pIter->bPatchset ){
        rc = SQLITE_ERROR;
      }

      /* Append a table header to the output for this new table */
      sessionAppendByte(&sOut, pIter->bPatchset ? 'P' : 'T', &rc);
      sessionAppendVarint(&sOut, pIter->nCol, &rc);
      sessionAppendBlob(&sOut, pIter->abPK, pIter->nCol, &rc);
      sessionAppendBlob(&sOut,(u8*)pIter->zTab,(int)strlen(pIter->zTab)+1,&rc);
    }

    if( pTab && rc==SQLITE_OK ){
      int iHash = sessionChangeHash(pTab, 0, aRec, pTab->nChange);

      for(pChange=pTab->apChange[iHash]; pChange; pChange=pChange->pNext){
        if( sessionChangeEqual(pTab, 0, aRec, 0, pChange->aRecord) ){
          break;
        }
      }
    }

    if( pChange ){
      assert( pChange->op==SQLITE_DELETE || pChange->op==SQLITE_INSERT );
      switch( pIter->op ){
        case SQLITE_INSERT:
          if( pChange->op==SQLITE_INSERT ){
            bDone = 1;
            if( pChange->bIndirect==0 ){
              sessionAppendByte(&sOut, SQLITE_UPDATE, &rc);
              sessionAppendByte(&sOut, pIter->bIndirect, &rc);
              sessionAppendBlob(&sOut, pChange->aRecord, pChange->nRecord, &rc);
              sessionAppendBlob(&sOut, aRec, nRec, &rc);
            }
          }
          break;

        case SQLITE_UPDATE:
          bDone = 1;
          if( pChange->op==SQLITE_DELETE ){
            if( pChange->bIndirect==0 ){
              u8 *pCsr = aRec;
              sessionSkipRecord(&pCsr, pIter->nCol);
              sessionAppendByte(&sOut, SQLITE_INSERT, &rc);
              sessionAppendByte(&sOut, pIter->bIndirect, &rc);
              sessionAppendRecordMerge(&sOut, pIter->nCol,
                  pCsr, nRec-(pCsr-aRec), 
                  pChange->aRecord, pChange->nRecord, &rc
              );
            }
          }else{
            sessionAppendPartialUpdate(&sOut, pIter,
                aRec, nRec, pChange->aRecord, pChange->nRecord, &rc
            );
          }
          break;

        default:
          assert( pIter->op==SQLITE_DELETE );
          bDone = 1;
          if( pChange->op==SQLITE_INSERT ){
            sessionAppendByte(&sOut, SQLITE_DELETE, &rc);
            sessionAppendByte(&sOut, pIter->bIndirect, &rc);
            sessionAppendRecordMerge(&sOut, pIter->nCol,
                pChange->aRecord, pChange->nRecord, aRec, nRec, &rc
            );
          }
          break;
      }
    }

    if( bDone==0 ){
      sessionAppendByte(&sOut, pIter->op, &rc);
      sessionAppendByte(&sOut, pIter->bIndirect, &rc);
      sessionAppendBlob(&sOut, aRec, nRec, &rc);
    }
    if( rc==SQLITE_OK && xOutput && sOut.nBuf>sessions_strm_chunk_size ){
      rc = xOutput(pOut, sOut.aBuf, sOut.nBuf);
      sOut.nBuf = 0;
    }
    if( rc ) break;
  }

  if( rc!=SQLITE_OK ){
    sqlite3_free(sOut.aBuf);
    memset(&sOut, 0, sizeof(sOut));
  }

  if( rc==SQLITE_OK ){
    if( xOutput ){
      if( sOut.nBuf>0 ){
        rc = xOutput(pOut, sOut.aBuf, sOut.nBuf);
      }
    }else{
      *ppOut = (void*)sOut.aBuf;
      *pnOut = sOut.nBuf;
      sOut.aBuf = 0;
    }
  }
  sqlite3_free(sOut.aBuf);
  return rc;
}

/* 
** Create a new rebaser object.
*/
SQLITE_API int sqlite3rebaser_create(sqlite3_rebaser **ppNew){
  int rc = SQLITE_OK;
  sqlite3_rebaser *pNew;

  pNew = sqlite3_malloc(sizeof(sqlite3_rebaser));
  if( pNew==0 ){
    rc = SQLITE_NOMEM;
  }else{
    memset(pNew, 0, sizeof(sqlite3_rebaser));
  }
  *ppNew = pNew;
  return rc;
}

/* 
** Call this one or more times to configure a rebaser.
*/
SQLITE_API int sqlite3rebaser_configure(
  sqlite3_rebaser *p, 
  int nRebase, const void *pRebase
){
  sqlite3_changeset_iter *pIter = 0;   /* Iterator opened on pData/nData */
  int rc;                              /* Return code */
  rc = sqlite3changeset_start(&pIter, nRebase, (void*)pRebase);
  if( rc==SQLITE_OK ){
    rc = sessionChangesetToHash(pIter, &p->grp, 1);
  }
  sqlite3changeset_finalize(pIter);
  return rc;
}

/* 
** Rebase a changeset according to current rebaser configuration 
*/
SQLITE_API int sqlite3rebaser_rebase(
  sqlite3_rebaser *p,
  int nIn, const void *pIn, 
  int *pnOut, void **ppOut 
){
  sqlite3_changeset_iter *pIter = 0;   /* Iterator to skip through input */  
  int rc = sqlite3changeset_start(&pIter, nIn, (void*)pIn);

  if( rc==SQLITE_OK ){
    rc = sessionRebase(p, pIter, 0, 0, pnOut, ppOut);
    sqlite3changeset_finalize(pIter);
  }

  return rc;
}

/* 
** Rebase a changeset according to current rebaser configuration 
*/
SQLITE_API int sqlite3rebaser_rebase_strm(
  sqlite3_rebaser *p,
  int (*xInput)(void *pIn, void *pData, int *pnData),
  void *pIn,
  int (*xOutput)(void *pOut, const void *pData, int nData),
  void *pOut
){
  sqlite3_changeset_iter *pIter = 0;   /* Iterator to skip through input */  
  int rc = sqlite3changeset_start_strm(&pIter, xInput, pIn);

  if( rc==SQLITE_OK ){
    rc = sessionRebase(p, pIter, xOutput, pOut, 0, 0);
    sqlite3changeset_finalize(pIter);
  }

  return rc;
}

/* 
** Destroy a rebaser object 
*/
SQLITE_API void sqlite3rebaser_delete(sqlite3_rebaser *p){
  if( p ){
    sessionDeleteTable(p->grp.pList);
    sqlite3_free(p);
  }
}

/* 
** Global configuration
*/
SQLITE_API int sqlite3session_config(int op, void *pArg){
  int rc = SQLITE_OK;
  switch( op ){
    case SQLITE_SESSION_CONFIG_STRMSIZE: {
      int *pInt = (int*)pArg;
      if( *pInt>0 ){
        sessions_strm_chunk_size = *pInt;
      }
      *pInt = sessions_strm_chunk_size;
      break;
    }
    default:
      rc = SQLITE_MISUSE;
      break;
  }
  return rc;
}

#endif /* SQLITE_ENABLE_SESSION && SQLITE_ENABLE_PREUPDATE_HOOK */

/************** End of sqlite3session.c **************************************/
