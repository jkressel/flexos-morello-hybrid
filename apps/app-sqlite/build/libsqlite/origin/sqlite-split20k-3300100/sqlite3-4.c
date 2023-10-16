#include <flexos/isolation.h>

/************** Begin file pager.c *******************************************/
/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This is the implementation of the page cache subsystem or "pager".
** 
** The pager is used to access a database disk file.  It implements
** atomic commit and rollback through the use of a journal file that
** is separate from the database file.  The pager also implements file
** locking to prevent two processes from writing the same database
** file simultaneously, or one process from reading the database while
** another is writing.
*/
#ifndef SQLITE_OMIT_DISKIO
/* #include "sqliteInt.h" */
/************** Include wal.h in the middle of pager.c ***********************/
/************** Begin file wal.h *********************************************/
/*
** 2010 February 1
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This header file defines the interface to the write-ahead logging 
** system. Refer to the comments below and the header comment attached to 
** the implementation of each function in log.c for further details.
*/

#ifndef SQLITE_WAL_H
#define SQLITE_WAL_H

/* #include "sqliteInt.h" */

/* Macros for extracting appropriate sync flags for either transaction
** commits (WAL_SYNC_FLAGS(X)) or for checkpoint ops (CKPT_SYNC_FLAGS(X)):
*/
#define WAL_SYNC_FLAGS(X)   ((X)&0x03)
#define CKPT_SYNC_FLAGS(X)  (((X)>>2)&0x03)

#ifdef SQLITE_OMIT_WAL
# define sqlite3WalOpen(x,y,z)                   0
# define sqlite3WalLimit(x,y)
# define sqlite3WalClose(v,w,x,y,z)              0
# define sqlite3WalBeginReadTransaction(y,z)     0
# define sqlite3WalEndReadTransaction(z)
# define sqlite3WalDbsize(y)                     0
# define sqlite3WalBeginWriteTransaction(y)      0
# define sqlite3WalEndWriteTransaction(x)        0
# define sqlite3WalUndo(x,y,z)                   0
# define sqlite3WalSavepoint(y,z)
# define sqlite3WalSavepointUndo(y,z)            0
# define sqlite3WalFrames(u,v,w,x,y,z)           0
# define sqlite3WalCheckpoint(q,r,s,t,u,v,w,x,y,z) 0
# define sqlite3WalCallback(z)                   0
# define sqlite3WalExclusiveMode(y,z)            0
# define sqlite3WalHeapMemory(z)                 0
# define sqlite3WalFramesize(z)                  0
# define sqlite3WalFindFrame(x,y,z)              0
# define sqlite3WalFile(x)                       0
#else

#define WAL_SAVEPOINT_NDATA 4

/* Connection to a write-ahead log (WAL) file. 
** There is one object of this type for each pager. 
*/
typedef struct Wal Wal;

/* Open and close a connection to a write-ahead log. */
SQLITE_PRIVATE int sqlite3WalOpen(sqlite3_vfs*, sqlite3_file*, const char *, int, i64, Wal**);
SQLITE_PRIVATE int sqlite3WalClose(Wal *pWal, sqlite3*, int sync_flags, int, u8 *);

/* Set the limiting size of a WAL file. */
SQLITE_PRIVATE void sqlite3WalLimit(Wal*, i64);

/* Used by readers to open (lock) and close (unlock) a snapshot.  A 
** snapshot is like a read-transaction.  It is the state of the database
** at an instant in time.  sqlite3WalOpenSnapshot gets a read lock and
** preserves the current state even if the other threads or processes
** write to or checkpoint the WAL.  sqlite3WalCloseSnapshot() closes the
** transaction and releases the lock.
*/
SQLITE_PRIVATE int sqlite3WalBeginReadTransaction(Wal *pWal, int *);
SQLITE_PRIVATE void sqlite3WalEndReadTransaction(Wal *pWal);

/* Read a page from the write-ahead log, if it is present. */
SQLITE_PRIVATE int sqlite3WalFindFrame(Wal *, Pgno, u32 *);
SQLITE_PRIVATE int sqlite3WalReadFrame(Wal *, u32, int, u8 *);

/* If the WAL is not empty, return the size of the database. */
SQLITE_PRIVATE Pgno sqlite3WalDbsize(Wal *pWal);

/* Obtain or release the WRITER lock. */
SQLITE_PRIVATE int sqlite3WalBeginWriteTransaction(Wal *pWal);
SQLITE_PRIVATE int sqlite3WalEndWriteTransaction(Wal *pWal);

/* Undo any frames written (but not committed) to the log */
SQLITE_PRIVATE int sqlite3WalUndo(Wal *pWal, int (*xUndo)(void *, Pgno), void *pUndoCtx);

/* Return an integer that records the current (uncommitted) write
** position in the WAL */
SQLITE_PRIVATE void sqlite3WalSavepoint(Wal *pWal, u32 *aWalData);

/* Move the write position of the WAL back to iFrame.  Called in
** response to a ROLLBACK TO command. */
SQLITE_PRIVATE int sqlite3WalSavepointUndo(Wal *pWal, u32 *aWalData);

/* Write a frame or frames to the log. */
SQLITE_PRIVATE int sqlite3WalFrames(Wal *pWal, int, PgHdr *, Pgno, int, int);

/* Copy pages from the log to the database file */ 
SQLITE_PRIVATE int sqlite3WalCheckpoint(
  Wal *pWal,                      /* Write-ahead log connection */
  sqlite3 *db,                    /* Check this handle's interrupt flag */
  int eMode,                      /* One of PASSIVE, FULL and RESTART */
  int (*xBusy)(void*),            /* Function to call when busy */
  void *pBusyArg,                 /* Context argument for xBusyHandler */
  int sync_flags,                 /* Flags to sync db file with (or 0) */
  int nBuf,                       /* Size of buffer nBuf */
  u8 *zBuf,                       /* Temporary buffer to use */
  int *pnLog,                     /* OUT: Number of frames in WAL */
  int *pnCkpt                     /* OUT: Number of backfilled frames in WAL */
);

/* Return the value to pass to a sqlite3_wal_hook callback, the
** number of frames in the WAL at the point of the last commit since
** sqlite3WalCallback() was called.  If no commits have occurred since
** the last call, then return 0.
*/
SQLITE_PRIVATE int sqlite3WalCallback(Wal *pWal);

/* Tell the wal layer that an EXCLUSIVE lock has been obtained (or released)
** by the pager layer on the database file.
*/
SQLITE_PRIVATE int sqlite3WalExclusiveMode(Wal *pWal, int op);

/* Return true if the argument is non-NULL and the WAL module is using
** heap-memory for the wal-index. Otherwise, if the argument is NULL or the
** WAL module is using shared-memory, return false. 
*/
SQLITE_PRIVATE int sqlite3WalHeapMemory(Wal *pWal);

#ifdef SQLITE_ENABLE_SNAPSHOT
SQLITE_PRIVATE int sqlite3WalSnapshotGet(Wal *pWal, sqlite3_snapshot **ppSnapshot);
SQLITE_PRIVATE void sqlite3WalSnapshotOpen(Wal *pWal, sqlite3_snapshot *pSnapshot);
SQLITE_PRIVATE int sqlite3WalSnapshotRecover(Wal *pWal);
SQLITE_PRIVATE int sqlite3WalSnapshotCheck(Wal *pWal, sqlite3_snapshot *pSnapshot);
SQLITE_PRIVATE void sqlite3WalSnapshotUnlock(Wal *pWal);
#endif

#ifdef SQLITE_ENABLE_ZIPVFS
/* If the WAL file is not empty, return the number of bytes of content
** stored in each frame (i.e. the db page-size when the WAL was created).
*/
SQLITE_PRIVATE int sqlite3WalFramesize(Wal *pWal);
#endif

/* Return the sqlite3_file object for the WAL file */
SQLITE_PRIVATE sqlite3_file *sqlite3WalFile(Wal *pWal);

#endif /* ifndef SQLITE_OMIT_WAL */
#endif /* SQLITE_WAL_H */

/************** End of wal.h *************************************************/
/************** Continuing where we left off in pager.c **********************/


/******************* NOTES ON THE DESIGN OF THE PAGER ************************
**
** This comment block describes invariants that hold when using a rollback
** journal.  These invariants do not apply for journal_mode=WAL,
** journal_mode=MEMORY, or journal_mode=OFF.
**
** Within this comment block, a page is deemed to have been synced
** automatically as soon as it is written when PRAGMA synchronous=OFF.
** Otherwise, the page is not synced until the xSync method of the VFS
** is called successfully on the file containing the page.
**
** Definition:  A page of the database file is said to be "overwriteable" if
** one or more of the following are true about the page:
** 
**     (a)  The original content of the page as it was at the beginning of
**          the transaction has been written into the rollback journal and
**          synced.
** 
**     (b)  The page was a freelist leaf page at the start of the transaction.
** 
**     (c)  The page number is greater than the largest page that existed in
**          the database file at the start of the transaction.
** 
** (1) A page of the database file is never overwritten unless one of the
**     following are true:
** 
**     (a) The page and all other pages on the same sector are overwriteable.
** 
**     (b) The atomic page write optimization is enabled, and the entire
**         transaction other than the update of the transaction sequence
**         number consists of a single page change.
** 
** (2) The content of a page written into the rollback journal exactly matches
**     both the content in the database when the rollback journal was written
**     and the content in the database at the beginning of the current
**     transaction.
** 
** (3) Writes to the database file are an integer multiple of the page size
**     in length and are aligned on a page boundary.
** 
** (4) Reads from the database file are either aligned on a page boundary and
**     an integer multiple of the page size in length or are taken from the
**     first 100 bytes of the database file.
** 
** (5) All writes to the database file are synced prior to the rollback journal
**     being deleted, truncated, or zeroed.
** 
** (6) If a master journal file is used, then all writes to the database file
**     are synced prior to the master journal being deleted.
** 
** Definition: Two databases (or the same database at two points it time)
** are said to be "logically equivalent" if they give the same answer to
** all queries.  Note in particular the content of freelist leaf
** pages can be changed arbitrarily without affecting the logical equivalence
** of the database.
** 
** (7) At any time, if any subset, including the empty set and the total set,
**     of the unsynced changes to a rollback journal are removed and the 
**     journal is rolled back, the resulting database file will be logically
**     equivalent to the database file at the beginning of the transaction.
** 
** (8) When a transaction is rolled back, the xTruncate method of the VFS
**     is called to restore the database file to the same size it was at
**     the beginning of the transaction.  (In some VFSes, the xTruncate
**     method is a no-op, but that does not change the fact the SQLite will
**     invoke it.)
** 
** (9) Whenever the database file is modified, at least one bit in the range
**     of bytes from 24 through 39 inclusive will be changed prior to releasing
**     the EXCLUSIVE lock, thus signaling other connections on the same
**     database to flush their caches.
**
** (10) The pattern of bits in bytes 24 through 39 shall not repeat in less
**      than one billion transactions.
**
** (11) A database file is well-formed at the beginning and at the conclusion
**      of every transaction.
**
** (12) An EXCLUSIVE lock is held on the database file when writing to
**      the database file.
**
** (13) A SHARED lock is held on the database file while reading any
**      content out of the database file.
**
******************************************************************************/

/*
** Macros for troubleshooting.  Normally turned off
*/
#if 0
int sqlite3PagerTrace=1;  /* True to enable tracing */
#define sqlite3DebugPrintf printf
#define PAGERTRACE(X)     if( sqlite3PagerTrace ){ sqlite3DebugPrintf X; }
#else
#define PAGERTRACE(X)
#endif

/*
** The following two macros are used within the PAGERTRACE() macros above
** to print out file-descriptors. 
**
** PAGERID() takes a pointer to a Pager struct as its argument. The
** associated file-descriptor is returned. FILEHANDLEID() takes an sqlite3_file
** struct as its argument.
*/
#define PAGERID(p) (SQLITE_PTR_TO_INT(p->fd))
#define FILEHANDLEID(fd) (SQLITE_PTR_TO_INT(fd))

/*
** The Pager.eState variable stores the current 'state' of a pager. A
** pager may be in any one of the seven states shown in the following
** state diagram.
**
**                            OPEN <------+------+
**                              |         |      |
**                              V         |      |
**               +---------> READER-------+      |
**               |              |                |
**               |              V                |
**               |<-------WRITER_LOCKED------> ERROR
**               |              |                ^  
**               |              V                |
**               |<------WRITER_CACHEMOD-------->|
**               |              |                |
**               |              V                |
**               |<-------WRITER_DBMOD---------->|
**               |              |                |
**               |              V                |
**               +<------WRITER_FINISHED-------->+
**
**
** List of state transitions and the C [function] that performs each:
** 
**   OPEN              -> READER              [sqlite3PagerSharedLock]
**   READER            -> OPEN                [pager_unlock]
**
**   READER            -> WRITER_LOCKED       [sqlite3PagerBegin]
**   WRITER_LOCKED     -> WRITER_CACHEMOD     [pager_open_journal]
**   WRITER_CACHEMOD   -> WRITER_DBMOD        [syncJournal]
**   WRITER_DBMOD      -> WRITER_FINISHED     [sqlite3PagerCommitPhaseOne]
**   WRITER_***        -> READER              [pager_end_transaction]
**
**   WRITER_***        -> ERROR               [pager_error]
**   ERROR             -> OPEN                [pager_unlock]
** 
**
**  OPEN:
**
**    The pager starts up in this state. Nothing is guaranteed in this
**    state - the file may or may not be locked and the database size is
**    unknown. The database may not be read or written.
**
**    * No read or write transaction is active.
**    * Any lock, or no lock at all, may be held on the database file.
**    * The dbSize, dbOrigSize and dbFileSize variables may not be trusted.
**
**  READER:
**
**    In this state all the requirements for reading the database in 
**    rollback (non-WAL) mode are met. Unless the pager is (or recently
**    was) in exclusive-locking mode, a user-level read transaction is 
**    open. The database size is known in this state.
**
**    A connection running with locking_mode=normal enters this state when
**    it opens a read-transaction on the database and returns to state
**    OPEN after the read-transaction is completed. However a connection
**    running in locking_mode=exclusive (including temp databases) remains in
**    this state even after the read-transaction is closed. The only way
**    a locking_mode=exclusive connection can transition from READER to OPEN
**    is via the ERROR state (see below).
** 
**    * A read transaction may be active (but a write-transaction cannot).
**    * A SHARED or greater lock is held on the database file.
**    * The dbSize variable may be trusted (even if a user-level read 
**      transaction is not active). The dbOrigSize and dbFileSize variables
**      may not be trusted at this point.
**    * If the database is a WAL database, then the WAL connection is open.
**    * Even if a read-transaction is not open, it is guaranteed that 
**      there is no hot-journal in the file-system.
**
**  WRITER_LOCKED:
**
**    The pager moves to this state from READER when a write-transaction
**    is first opened on the database. In WRITER_LOCKED state, all locks 
**    required to start a write-transaction are held, but no actual 
**    modifications to the cache or database have taken place.
**
**    In rollback mode, a RESERVED or (if the transaction was opened with 
**    BEGIN EXCLUSIVE) EXCLUSIVE lock is obtained on the database file when
**    moving to this state, but the journal file is not written to or opened 
**    to in this state. If the transaction is committed or rolled back while 
**    in WRITER_LOCKED state, all that is required is to unlock the database 
**    file.
**
**    IN WAL mode, WalBeginWriteTransaction() is called to lock the log file.
**    If the connection is running with locking_mode=exclusive, an attempt
**    is made to obtain an EXCLUSIVE lock on the database file.
**
**    * A write transaction is active.
**    * If the connection is open in rollback-mode, a RESERVED or greater 
**      lock is held on the database file.
**    * If the connection is open in WAL-mode, a WAL write transaction
**      is open (i.e. sqlite3WalBeginWriteTransaction() has been successfully
**      called).
**    * The dbSize, dbOrigSize and dbFileSize variables are all valid.
**    * The contents of the pager cache have not been modified.
**    * The journal file may or may not be open.
**    * Nothing (not even the first header) has been written to the journal.
**
**  WRITER_CACHEMOD:
**
**    A pager moves from WRITER_LOCKED state to this state when a page is
**    first modified by the upper layer. In rollback mode the journal file
**    is opened (if it is not already open) and a header written to the
**    start of it. The database file on disk has not been modified.
**
**    * A write transaction is active.
**    * A RESERVED or greater lock is held on the database file.
**    * The journal file is open and the first header has been written 
**      to it, but the header has not been synced to disk.
**    * The contents of the page cache have been modified.
**
**  WRITER_DBMOD:
**
**    The pager transitions from WRITER_CACHEMOD into WRITER_DBMOD state
**    when it modifies the contents of the database file. WAL connections
**    never enter this state (since they do not modify the database file,
**    just the log file).
**
**    * A write transaction is active.
**    * An EXCLUSIVE or greater lock is held on the database file.
**    * The journal file is open and the first header has been written 
**      and synced to disk.
**    * The contents of the page cache have been modified (and possibly
**      written to disk).
**
**  WRITER_FINISHED:
**
**    It is not possible for a WAL connection to enter this state.
**
**    A rollback-mode pager changes to WRITER_FINISHED state from WRITER_DBMOD
**    state after the entire transaction has been successfully written into the
**    database file. In this state the transaction may be committed simply
**    by finalizing the journal file. Once in WRITER_FINISHED state, it is 
**    not possible to modify the database further. At this point, the upper 
**    layer must either commit or rollback the transaction.
**
**    * A write transaction is active.
**    * An EXCLUSIVE or greater lock is held on the database file.
**    * All writing and syncing of journal and database data has finished.
**      If no error occurred, all that remains is to finalize the journal to
**      commit the transaction. If an error did occur, the caller will need
**      to rollback the transaction. 
**
**  ERROR:
**
**    The ERROR state is entered when an IO or disk-full error (including
**    SQLITE_IOERR_NOMEM) occurs at a point in the code that makes it 
**    difficult to be sure that the in-memory pager state (cache contents, 
**    db size etc.) are consistent with the contents of the file-system.
**
**    Temporary pager files may enter the ERROR state, but in-memory pagers
**    cannot.
**
**    For example, if an IO error occurs while performing a rollback, 
**    the contents of the page-cache may be left in an inconsistent state.
**    At this point it would be dangerous to change back to READER state
**    (as usually happens after a rollback). Any subsequent readers might
**    report database corruption (due to the inconsistent cache), and if
**    they upgrade to writers, they may inadvertently corrupt the database
**    file. To avoid this hazard, the pager switches into the ERROR state
**    instead of READER following such an error.
**
**    Once it has entered the ERROR state, any attempt to use the pager
**    to read or write data returns an error. Eventually, once all 
**    outstanding transactions have been abandoned, the pager is able to
**    transition back to OPEN state, discarding the contents of the 
**    page-cache and any other in-memory state at the same time. Everything
**    is reloaded from disk (and, if necessary, hot-journal rollback peformed)
**    when a read-transaction is next opened on the pager (transitioning
**    the pager into READER state). At that point the system has recovered 
**    from the error.
**
**    Specifically, the pager jumps into the ERROR state if:
**
**      1. An error occurs while attempting a rollback. This happens in
**         function sqlite3PagerRollback().
**
**      2. An error occurs while attempting to finalize a journal file
**         following a commit in function sqlite3PagerCommitPhaseTwo().
**
**      3. An error occurs while attempting to write to the journal or
**         database file in function pagerStress() in order to free up
**         memory.
**
**    In other cases, the error is returned to the b-tree layer. The b-tree
**    layer then attempts a rollback operation. If the error condition 
**    persists, the pager enters the ERROR state via condition (1) above.
**
**    Condition (3) is necessary because it can be triggered by a read-only
**    statement executed within a transaction. In this case, if the error
**    code were simply returned to the user, the b-tree layer would not
**    automatically attempt a rollback, as it assumes that an error in a
**    read-only statement cannot leave the pager in an internally inconsistent 
**    state.
**
**    * The Pager.errCode variable is set to something other than SQLITE_OK.
**    * There are one or more outstanding references to pages (after the
**      last reference is dropped the pager should move back to OPEN state).
**    * The pager is not an in-memory pager.
**    
**
** Notes:
**
**   * A pager is never in WRITER_DBMOD or WRITER_FINISHED state if the
**     connection is open in WAL mode. A WAL connection is always in one
**     of the first four states.
**
**   * Normally, a connection open in exclusive mode is never in PAGER_OPEN
**     state. There are two exceptions: immediately after exclusive-mode has
**     been turned on (and before any read or write transactions are 
**     executed), and when the pager is leaving the "error state".
**
**   * See also: assert_pager_state().
*/
#define PAGER_OPEN                  0
#define PAGER_READER                1
#define PAGER_WRITER_LOCKED         2
#define PAGER_WRITER_CACHEMOD       3
#define PAGER_WRITER_DBMOD          4
#define PAGER_WRITER_FINISHED       5
#define PAGER_ERROR                 6

/*
** The Pager.eLock variable is almost always set to one of the 
** following locking-states, according to the lock currently held on
** the database file: NO_LOCK, SHARED_LOCK, RESERVED_LOCK or EXCLUSIVE_LOCK.
** This variable is kept up to date as locks are taken and released by
** the pagerLockDb() and pagerUnlockDb() wrappers.
**
** If the VFS xLock() or xUnlock() returns an error other than SQLITE_BUSY
** (i.e. one of the SQLITE_IOERR subtypes), it is not clear whether or not
** the operation was successful. In these circumstances pagerLockDb() and
** pagerUnlockDb() take a conservative approach - eLock is always updated
** when unlocking the file, and only updated when locking the file if the
** VFS call is successful. This way, the Pager.eLock variable may be set
** to a less exclusive (lower) value than the lock that is actually held
** at the system level, but it is never set to a more exclusive value.
**
** This is usually safe. If an xUnlock fails or appears to fail, there may 
** be a few redundant xLock() calls or a lock may be held for longer than
** required, but nothing really goes wrong.
**
** The exception is when the database file is unlocked as the pager moves
** from ERROR to OPEN state. At this point there may be a hot-journal file 
** in the file-system that needs to be rolled back (as part of an OPEN->SHARED
** transition, by the same pager or any other). If the call to xUnlock()
** fails at this point and the pager is left holding an EXCLUSIVE lock, this
** can confuse the call to xCheckReservedLock() call made later as part
** of hot-journal detection.
**
** xCheckReservedLock() is defined as returning true "if there is a RESERVED 
** lock held by this process or any others". So xCheckReservedLock may 
** return true because the caller itself is holding an EXCLUSIVE lock (but
** doesn't know it because of a previous error in xUnlock). If this happens
** a hot-journal may be mistaken for a journal being created by an active
** transaction in another process, causing SQLite to read from the database
** without rolling it back.
**
** To work around this, if a call to xUnlock() fails when unlocking the
** database in the ERROR state, Pager.eLock is set to UNKNOWN_LOCK. It
** is only changed back to a real locking state after a successful call
** to xLock(EXCLUSIVE). Also, the code to do the OPEN->SHARED state transition
** omits the check for a hot-journal if Pager.eLock is set to UNKNOWN_LOCK 
** lock. Instead, it assumes a hot-journal exists and obtains an EXCLUSIVE
** lock on the database file before attempting to roll it back. See function
** PagerSharedLock() for more detail.
**
** Pager.eLock may only be set to UNKNOWN_LOCK when the pager is in 
** PAGER_OPEN state.
*/
#define UNKNOWN_LOCK                (EXCLUSIVE_LOCK+1)

/*
** A macro used for invoking the codec if there is one
*/
#ifdef SQLITE_HAS_CODEC
# define CODEC1(P,D,N,X,E) \
    if( P->xCodec && P->xCodec(P->pCodec,D,N,X)==0 ){ E; }
# define CODEC2(P,D,N,X,E,O) \
    if( P->xCodec==0 ){ O=(char*)D; }else \
    if( (O=(char*)(P->xCodec(P->pCodec,D,N,X)))==0 ){ E; }
#else
# define CODEC1(P,D,N,X,E)   /* NO-OP */
# define CODEC2(P,D,N,X,E,O) O=(char*)D
#endif

/*
** The maximum allowed sector size. 64KiB. If the xSectorsize() method 
** returns a value larger than this, then MAX_SECTOR_SIZE is used instead.
** This could conceivably cause corruption following a power failure on
** such a system. This is currently an undocumented limit.
*/
#define MAX_SECTOR_SIZE 0x10000


/*
** An instance of the following structure is allocated for each active
** savepoint and statement transaction in the system. All such structures
** are stored in the Pager.aSavepoint[] array, which is allocated and
** resized using sqlite3Realloc().
**
** When a savepoint is created, the PagerSavepoint.iHdrOffset field is
** set to 0. If a journal-header is written into the main journal while
** the savepoint is active, then iHdrOffset is set to the byte offset 
** immediately following the last journal record written into the main
** journal before the journal-header. This is required during savepoint
** rollback (see pagerPlaybackSavepoint()).
*/
typedef struct PagerSavepoint PagerSavepoint;
struct PagerSavepoint {
  i64 iOffset;                 /* Starting offset in main journal */
  i64 iHdrOffset;              /* See above */
  Bitvec *pInSavepoint;        /* Set of pages in this savepoint */
  Pgno nOrig;                  /* Original number of pages in file */
  Pgno iSubRec;                /* Index of first record in sub-journal */
#ifndef SQLITE_OMIT_WAL
  u32 aWalData[WAL_SAVEPOINT_NDATA];        /* WAL savepoint context */
#endif
};

/*
** Bits of the Pager.doNotSpill flag.  See further description below.
*/
#define SPILLFLAG_OFF         0x01 /* Never spill cache.  Set via pragma */
#define SPILLFLAG_ROLLBACK    0x02 /* Current rolling back, so do not spill */
#define SPILLFLAG_NOSYNC      0x04 /* Spill is ok, but do not sync */

/*
** An open page cache is an instance of struct Pager. A description of
** some of the more important member variables follows:
**
** eState
**
**   The current 'state' of the pager object. See the comment and state
**   diagram above for a description of the pager state.
**
** eLock
**
**   For a real on-disk database, the current lock held on the database file -
**   NO_LOCK, SHARED_LOCK, RESERVED_LOCK or EXCLUSIVE_LOCK.
**
**   For a temporary or in-memory database (neither of which require any
**   locks), this variable is always set to EXCLUSIVE_LOCK. Since such
**   databases always have Pager.exclusiveMode==1, this tricks the pager
**   logic into thinking that it already has all the locks it will ever
**   need (and no reason to release them).
**
**   In some (obscure) circumstances, this variable may also be set to
**   UNKNOWN_LOCK. See the comment above the #define of UNKNOWN_LOCK for
**   details.
**
** changeCountDone
**
**   This boolean variable is used to make sure that the change-counter 
**   (the 4-byte header field at byte offset 24 of the database file) is 
**   not updated more often than necessary. 
**
**   It is set to true when the change-counter field is updated, which 
**   can only happen if an exclusive lock is held on the database file.
**   It is cleared (set to false) whenever an exclusive lock is 
**   relinquished on the database file. Each time a transaction is committed,
**   The changeCountDone flag is inspected. If it is true, the work of
**   updating the change-counter is omitted for the current transaction.
**
**   This mechanism means that when running in exclusive mode, a connection 
**   need only update the change-counter once, for the first transaction
**   committed.
**
** setMaster
**
**   When PagerCommitPhaseOne() is called to commit a transaction, it may
**   (or may not) specify a master-journal name to be written into the 
**   journal file before it is synced to disk.
**
**   Whether or not a journal file contains a master-journal pointer affects 
**   the way in which the journal file is finalized after the transaction is 
**   committed or rolled back when running in "journal_mode=PERSIST" mode.
**   If a journal file does not contain a master-journal pointer, it is
**   finalized by overwriting the first journal header with zeroes. If
**   it does contain a master-journal pointer the journal file is finalized 
**   by truncating it to zero bytes, just as if the connection were 
**   running in "journal_mode=truncate" mode.
**
**   Journal files that contain master journal pointers cannot be finalized
**   simply by overwriting the first journal-header with zeroes, as the
**   master journal pointer could interfere with hot-journal rollback of any
**   subsequently interrupted transaction that reuses the journal file.
**
**   The flag is cleared as soon as the journal file is finalized (either
**   by PagerCommitPhaseTwo or PagerRollback). If an IO error prevents the
**   journal file from being successfully finalized, the setMaster flag
**   is cleared anyway (and the pager will move to ERROR state).
**
** doNotSpill
**
**   This variables control the behavior of cache-spills  (calls made by
**   the pcache module to the pagerStress() routine to write cached data
**   to the file-system in order to free up memory).
**
**   When bits SPILLFLAG_OFF or SPILLFLAG_ROLLBACK of doNotSpill are set,
**   writing to the database from pagerStress() is disabled altogether.
**   The SPILLFLAG_ROLLBACK case is done in a very obscure case that
**   comes up during savepoint rollback that requires the pcache module
**   to allocate a new page to prevent the journal file from being written
**   while it is being traversed by code in pager_playback().  The SPILLFLAG_OFF
**   case is a user preference.
** 
**   If the SPILLFLAG_NOSYNC bit is set, writing to the database from
**   pagerStress() is permitted, but syncing the journal file is not.
**   This flag is set by sqlite3PagerWrite() when the file-system sector-size
**   is larger than the database page-size in order to prevent a journal sync
**   from happening in between the journalling of two pages on the same sector. 
**
** subjInMemory
**
**   This is a boolean variable. If true, then any required sub-journal
**   is opened as an in-memory journal file. If false, then in-memory
**   sub-journals are only used for in-memory pager files.
**
**   This variable is updated by the upper layer each time a new 
**   write-transaction is opened.
**
** dbSize, dbOrigSize, dbFileSize
**
**   Variable dbSize is set to the number of pages in the database file.
**   It is valid in PAGER_READER and higher states (all states except for
**   OPEN and ERROR). 
**
**   dbSize is set based on the size of the database file, which may be 
**   larger than the size of the database (the value stored at offset
**   28 of the database header by the btree). If the size of the file
**   is not an integer multiple of the page-size, the value stored in
**   dbSize is rounded down (i.e. a 5KB file with 2K page-size has dbSize==2).
**   Except, any file that is greater than 0 bytes in size is considered
**   to have at least one page. (i.e. a 1KB file with 2K page-size leads
**   to dbSize==1).
**
**   During a write-transaction, if pages with page-numbers greater than
**   dbSize are modified in the cache, dbSize is updated accordingly.
**   Similarly, if the database is truncated using PagerTruncateImage(), 
**   dbSize is updated.
**
**   Variables dbOrigSize and dbFileSize are valid in states 
**   PAGER_WRITER_LOCKED and higher. dbOrigSize is a copy of the dbSize
**   variable at the start of the transaction. It is used during rollback,
**   and to determine whether or not pages need to be journalled before
**   being modified.
**
**   Throughout a write-transaction, dbFileSize contains the size of
**   the file on disk in pages. It is set to a copy of dbSize when the
**   write-transaction is first opened, and updated when VFS calls are made
**   to write or truncate the database file on disk. 
**
**   The only reason the dbFileSize variable is required is to suppress 
**   unnecessary calls to xTruncate() after committing a transaction. If, 
**   when a transaction is committed, the dbFileSize variable indicates 
**   that the database file is larger than the database image (Pager.dbSize), 
**   pager_truncate() is called. The pager_truncate() call uses xFilesize()
**   to measure the database file on disk, and then truncates it if required.
**   dbFileSize is not used when rolling back a transaction. In this case
**   pager_truncate() is called unconditionally (which means there may be
**   a call to xFilesize() that is not strictly required). In either case,
**   pager_truncate() may cause the file to become smaller or larger.
**
** dbHintSize
**
**   The dbHintSize variable is used to limit the number of calls made to
**   the VFS xFileControl(FCNTL_SIZE_HINT) method. 
**
**   dbHintSize is set to a copy of the dbSize variable when a
**   write-transaction is opened (at the same time as dbFileSize and
**   dbOrigSize). If the xFileControl(FCNTL_SIZE_HINT) method is called,
**   dbHintSize is increased to the number of pages that correspond to the
**   size-hint passed to the method call. See pager_write_pagelist() for 
**   details.
**
** errCode
**
**   The Pager.errCode variable is only ever used in PAGER_ERROR state. It
**   is set to zero in all other states. In PAGER_ERROR state, Pager.errCode 
**   is always set to SQLITE_FULL, SQLITE_IOERR or one of the SQLITE_IOERR_XXX 
**   sub-codes.
**
** syncFlags, walSyncFlags
**
**   syncFlags is either SQLITE_SYNC_NORMAL (0x02) or SQLITE_SYNC_FULL (0x03).
**   syncFlags is used for rollback mode.  walSyncFlags is used for WAL mode
**   and contains the flags used to sync the checkpoint operations in the
**   lower two bits, and sync flags used for transaction commits in the WAL
**   file in bits 0x04 and 0x08.  In other words, to get the correct sync flags
**   for checkpoint operations, use (walSyncFlags&0x03) and to get the correct
**   sync flags for transaction commit, use ((walSyncFlags>>2)&0x03).  Note
**   that with synchronous=NORMAL in WAL mode, transaction commit is not synced
**   meaning that the 0x04 and 0x08 bits are both zero.
*/
struct Pager {
  sqlite3_vfs *pVfs;          /* OS functions to use for IO */
  u8 exclusiveMode;           /* Boolean. True if locking_mode==EXCLUSIVE */
  u8 journalMode;             /* One of the PAGER_JOURNALMODE_* values */
  u8 useJournal;              /* Use a rollback journal on this file */
  u8 noSync;                  /* Do not sync the journal if true */
  u8 fullSync;                /* Do extra syncs of the journal for robustness */
  u8 extraSync;               /* sync directory after journal delete */
  u8 syncFlags;               /* SYNC_NORMAL or SYNC_FULL otherwise */
  u8 walSyncFlags;            /* See description above */
  u8 tempFile;                /* zFilename is a temporary or immutable file */
  u8 noLock;                  /* Do not lock (except in WAL mode) */
  u8 readOnly;                /* True for a read-only database */
  u8 memDb;                   /* True to inhibit all file I/O */

  /**************************************************************************
  ** The following block contains those class members that change during
  ** routine operation.  Class members not in this block are either fixed
  ** when the pager is first created or else only change when there is a
  ** significant mode change (such as changing the page_size, locking_mode,
  ** or the journal_mode).  From another view, these class members describe
  ** the "state" of the pager, while other class members describe the
  ** "configuration" of the pager.
  */
  u8 eState;                  /* Pager state (OPEN, READER, WRITER_LOCKED..) */
  u8 eLock;                   /* Current lock held on database file */
  u8 changeCountDone;         /* Set after incrementing the change-counter */
  u8 setMaster;               /* True if a m-j name has been written to jrnl */
  u8 doNotSpill;              /* Do not spill the cache when non-zero */
  u8 subjInMemory;            /* True to use in-memory sub-journals */
  u8 bUseFetch;               /* True to use xFetch() */
  u8 hasHeldSharedLock;       /* True if a shared lock has ever been held */
  Pgno dbSize;                /* Number of pages in the database */
  Pgno dbOrigSize;            /* dbSize before the current transaction */
  Pgno dbFileSize;            /* Number of pages in the database file */
  Pgno dbHintSize;            /* Value passed to FCNTL_SIZE_HINT call */
  int errCode;                /* One of several kinds of errors */
  int nRec;                   /* Pages journalled since last j-header written */
  u32 cksumInit;              /* Quasi-random value added to every checksum */
  u32 nSubRec;                /* Number of records written to sub-journal */
  Bitvec *pInJournal;         /* One bit for each page in the database file */
  sqlite3_file *fd;           /* File descriptor for database */
  sqlite3_file *jfd;          /* File descriptor for main journal */
  sqlite3_file *sjfd;         /* File descriptor for sub-journal */
  i64 journalOff;             /* Current write offset in the journal file */
  i64 journalHdr;             /* Byte offset to previous journal header */
  sqlite3_backup *pBackup;    /* Pointer to list of ongoing backup processes */
  PagerSavepoint *aSavepoint; /* Array of active savepoints */
  int nSavepoint;             /* Number of elements in aSavepoint[] */
  u32 iDataVersion;           /* Changes whenever database content changes */
  char dbFileVers[16];        /* Changes whenever database file changes */

  int nMmapOut;               /* Number of mmap pages currently outstanding */
  sqlite3_int64 szMmap;       /* Desired maximum mmap size */
  PgHdr *pMmapFreelist;       /* List of free mmap page headers (pDirty) */
  /*
  ** End of the routinely-changing class members
  ***************************************************************************/

  u16 nExtra;                 /* Add this many bytes to each in-memory page */
  i16 nReserve;               /* Number of unused bytes at end of each page */
  u32 vfsFlags;               /* Flags for sqlite3_vfs.xOpen() */
  u32 sectorSize;             /* Assumed sector size during rollback */
  int pageSize;               /* Number of bytes in a page */
  Pgno mxPgno;                /* Maximum allowed size of the database */
  i64 journalSizeLimit;       /* Size limit for persistent journal files */
  char *zFilename;            /* Name of the database file */
  char *zJournal;             /* Name of the journal file */
  int (*xBusyHandler)(void*); /* Function to call when busy */
  void *pBusyHandlerArg;      /* Context argument for xBusyHandler */
  int aStat[4];               /* Total cache hits, misses, writes, spills */
#ifdef SQLITE_TEST
  int nRead;                  /* Database pages read */
#endif
  void (*xReiniter)(DbPage*); /* Call this routine when reloading pages */
  int (*xGet)(Pager*,Pgno,DbPage**,int); /* Routine to fetch a patch */
#ifdef SQLITE_HAS_CODEC
  void *(*xCodec)(void*,void*,Pgno,int); /* Routine for en/decoding data */
  void (*xCodecSizeChng)(void*,int,int); /* Notify of page size changes */
  void (*xCodecFree)(void*);             /* Destructor for the codec */
  void *pCodec;               /* First argument to xCodec... methods */
#endif
  char *pTmpSpace;            /* Pager.pageSize bytes of space for tmp use */
  PCache *pPCache;            /* Pointer to page cache object */
#ifndef SQLITE_OMIT_WAL
  Wal *pWal;                  /* Write-ahead log used by "journal_mode=wal" */
  char *zWal;                 /* File name for write-ahead log */
#endif
};

/*
** Indexes for use with Pager.aStat[]. The Pager.aStat[] array contains
** the values accessed by passing SQLITE_DBSTATUS_CACHE_HIT, CACHE_MISS 
** or CACHE_WRITE to sqlite3_db_status().
*/
#define PAGER_STAT_HIT   0
#define PAGER_STAT_MISS  1
#define PAGER_STAT_WRITE 2
#define PAGER_STAT_SPILL 3

/*
** The following global variables hold counters used for
** testing purposes only.  These variables do not exist in
** a non-testing build.  These variables are not thread-safe.
*/
#ifdef SQLITE_TEST
SQLITE_API int sqlite3_pager_readdb_count = 0;    /* Number of full pages read from DB */
SQLITE_API int sqlite3_pager_writedb_count = 0;   /* Number of full pages written to DB */
SQLITE_API int sqlite3_pager_writej_count = 0;    /* Number of pages written to journal */
# define PAGER_INCR(v)  v++
#else
# define PAGER_INCR(v)
#endif



/*
** Journal files begin with the following magic string.  The data
** was obtained from /dev/random.  It is used only as a sanity check.
**
** Since version 2.8.0, the journal format contains additional sanity
** checking information.  If the power fails while the journal is being
** written, semi-random garbage data might appear in the journal
** file after power is restored.  If an attempt is then made
** to roll the journal back, the database could be corrupted.  The additional
** sanity checking data is an attempt to discover the garbage in the
** journal and ignore it.
**
** The sanity checking information for the new journal format consists
** of a 32-bit checksum on each page of data.  The checksum covers both
** the page number and the pPager->pageSize bytes of data for the page.
** This cksum is initialized to a 32-bit random value that appears in the
** journal file right after the header.  The random initializer is important,
** because garbage data that appears at the end of a journal is likely
** data that was once in other files that have now been deleted.  If the
** garbage data came from an obsolete journal file, the checksums might
** be correct.  But by initializing the checksum to random value which
** is different for every journal, we minimize that risk.
*/
static const unsigned char aJournalMagic[] = {
  0xd9, 0xd5, 0x05, 0xf9, 0x20, 0xa1, 0x63, 0xd7,
};

/*
** The size of the of each page record in the journal is given by
** the following macro.
*/
#define JOURNAL_PG_SZ(pPager)  ((pPager->pageSize) + 8)

/*
** The journal header size for this pager. This is usually the same 
** size as a single disk sector. See also setSectorSize().
*/
#define JOURNAL_HDR_SZ(pPager) (pPager->sectorSize)

/*
** The macro MEMDB is true if we are dealing with an in-memory database.
** We do this as a macro so that if the SQLITE_OMIT_MEMORYDB macro is set,
** the value of MEMDB will be a constant and the compiler will optimize
** out code that would never execute.
*/
#ifdef SQLITE_OMIT_MEMORYDB
# define MEMDB 0
#else
# define MEMDB pPager->memDb
#endif

/*
** The macro USEFETCH is true if we are allowed to use the xFetch and xUnfetch
** interfaces to access the database using memory-mapped I/O.
*/
#if SQLITE_MAX_MMAP_SIZE>0
# define USEFETCH(x) ((x)->bUseFetch)
#else
# define USEFETCH(x) 0
#endif

/*
** The maximum legal page number is (2^31 - 1).
*/
#define PAGER_MAX_PGNO 2147483647

/*
** The argument to this macro is a file descriptor (type sqlite3_file*).
** Return 0 if it is not open, or non-zero (but not 1) if it is.
**
** This is so that expressions can be written as:
**
**   if( isOpen(pPager->jfd) ){ ...
**
** instead of
**
**   if( pPager->jfd->pMethods ){ ...
*/
#define isOpen(pFd) ((pFd)->pMethods!=0)

#ifdef SQLITE_DIRECT_OVERFLOW_READ
/*
** Return true if page pgno can be read directly from the database file
** by the b-tree layer. This is the case if:
**
**   * the database file is open,
**   * there are no dirty pages in the cache, and
**   * the desired page is not currently in the wal file.
*/
SQLITE_PRIVATE int sqlite3PagerDirectReadOk(Pager *pPager, Pgno pgno){
  if( pPager->fd->pMethods==0 ) return 0;
  if( sqlite3PCacheIsDirty(pPager->pPCache) ) return 0;
#ifdef SQLITE_HAS_CODEC
  if( pPager->xCodec!=0 ) return 0;
#endif
#ifndef SQLITE_OMIT_WAL
  if( pPager->pWal ){
    u32 iRead = 0;
    int rc;
    rc = sqlite3WalFindFrame(pPager->pWal, pgno, &iRead);
    return (rc==SQLITE_OK && iRead==0);
  }
#endif
  return 1;
}
#endif

#ifndef SQLITE_OMIT_WAL
# define pagerUseWal(x) ((x)->pWal!=0)
#else
# define pagerUseWal(x) 0
# define pagerRollbackWal(x) 0
# define pagerWalFrames(v,w,x,y) 0
# define pagerOpenWalIfPresent(z) SQLITE_OK
# define pagerBeginReadTransaction(z) SQLITE_OK
#endif

#ifndef NDEBUG 
/*
** Usage:
**
**   assert( assert_pager_state(pPager) );
**
** This function runs many asserts to try to find inconsistencies in
** the internal state of the Pager object.
*/
static int assert_pager_state(Pager *p){
  Pager *pPager = p;

  /* State must be valid. */
  assert( p->eState==PAGER_OPEN
       || p->eState==PAGER_READER
       || p->eState==PAGER_WRITER_LOCKED
       || p->eState==PAGER_WRITER_CACHEMOD
       || p->eState==PAGER_WRITER_DBMOD
       || p->eState==PAGER_WRITER_FINISHED
       || p->eState==PAGER_ERROR
  );

  /* Regardless of the current state, a temp-file connection always behaves
  ** as if it has an exclusive lock on the database file. It never updates
  ** the change-counter field, so the changeCountDone flag is always set.
  */
  assert( p->tempFile==0 || p->eLock==EXCLUSIVE_LOCK );
  assert( p->tempFile==0 || pPager->changeCountDone );

  /* If the useJournal flag is clear, the journal-mode must be "OFF". 
  ** And if the journal-mode is "OFF", the journal file must not be open.
  */
  assert( p->journalMode==PAGER_JOURNALMODE_OFF || p->useJournal );
  assert( p->journalMode!=PAGER_JOURNALMODE_OFF || !isOpen(p->jfd) );

  /* Check that MEMDB implies noSync. And an in-memory journal. Since 
  ** this means an in-memory pager performs no IO at all, it cannot encounter 
  ** either SQLITE_IOERR or SQLITE_FULL during rollback or while finalizing 
  ** a journal file. (although the in-memory journal implementation may 
  ** return SQLITE_IOERR_NOMEM while the journal file is being written). It 
  ** is therefore not possible for an in-memory pager to enter the ERROR 
  ** state.
  */
  if( MEMDB ){
    assert( !isOpen(p->fd) );
    assert( p->noSync );
    assert( p->journalMode==PAGER_JOURNALMODE_OFF 
         || p->journalMode==PAGER_JOURNALMODE_MEMORY 
    );
    assert( p->eState!=PAGER_ERROR && p->eState!=PAGER_OPEN );
    assert( pagerUseWal(p)==0 );
  }

  /* If changeCountDone is set, a RESERVED lock or greater must be held
  ** on the file.
  */
  assert( pPager->changeCountDone==0 || pPager->eLock>=RESERVED_LOCK );
  assert( p->eLock!=PENDING_LOCK );

  switch( p->eState ){
    case PAGER_OPEN:
      assert( !MEMDB );
      assert( pPager->errCode==SQLITE_OK );
      assert( sqlite3PcacheRefCount(pPager->pPCache)==0 || pPager->tempFile );
      break;

    case PAGER_READER:
      assert( pPager->errCode==SQLITE_OK );
      assert( p->eLock!=UNKNOWN_LOCK );
      assert( p->eLock>=SHARED_LOCK );
      break;

    case PAGER_WRITER_LOCKED:
      assert( p->eLock!=UNKNOWN_LOCK );
      assert( pPager->errCode==SQLITE_OK );
      if( !pagerUseWal(pPager) ){
        assert( p->eLock>=RESERVED_LOCK );
      }
      assert( pPager->dbSize==pPager->dbOrigSize );
      assert( pPager->dbOrigSize==pPager->dbFileSize );
      assert( pPager->dbOrigSize==pPager->dbHintSize );
      assert( pPager->setMaster==0 );
      break;

    case PAGER_WRITER_CACHEMOD:
      assert( p->eLock!=UNKNOWN_LOCK );
      assert( pPager->errCode==SQLITE_OK );
      if( !pagerUseWal(pPager) ){
        /* It is possible that if journal_mode=wal here that neither the
        ** journal file nor the WAL file are open. This happens during
        ** a rollback transaction that switches from journal_mode=off
        ** to journal_mode=wal.
        */
        assert( p->eLock>=RESERVED_LOCK );
        assert( isOpen(p->jfd) 
             || p->journalMode==PAGER_JOURNALMODE_OFF 
             || p->journalMode==PAGER_JOURNALMODE_WAL 
        );
      }
      assert( pPager->dbOrigSize==pPager->dbFileSize );
      assert( pPager->dbOrigSize==pPager->dbHintSize );
      break;

    case PAGER_WRITER_DBMOD:
      assert( p->eLock==EXCLUSIVE_LOCK );
      assert( pPager->errCode==SQLITE_OK );
      assert( !pagerUseWal(pPager) );
      assert( p->eLock>=EXCLUSIVE_LOCK );
      assert( isOpen(p->jfd) 
           || p->journalMode==PAGER_JOURNALMODE_OFF 
           || p->journalMode==PAGER_JOURNALMODE_WAL 
           || (sqlite3OsDeviceCharacteristics(p->fd)&SQLITE_IOCAP_BATCH_ATOMIC)
      );
      assert( pPager->dbOrigSize<=pPager->dbHintSize );
      break;

    case PAGER_WRITER_FINISHED:
      assert( p->eLock==EXCLUSIVE_LOCK );
      assert( pPager->errCode==SQLITE_OK );
      assert( !pagerUseWal(pPager) );
      assert( isOpen(p->jfd) 
           || p->journalMode==PAGER_JOURNALMODE_OFF 
           || p->journalMode==PAGER_JOURNALMODE_WAL 
           || (sqlite3OsDeviceCharacteristics(p->fd)&SQLITE_IOCAP_BATCH_ATOMIC)
      );
      break;

    case PAGER_ERROR:
      /* There must be at least one outstanding reference to the pager if
      ** in ERROR state. Otherwise the pager should have already dropped
      ** back to OPEN state.
      */
      assert( pPager->errCode!=SQLITE_OK );
      assert( sqlite3PcacheRefCount(pPager->pPCache)>0 || pPager->tempFile );
      break;
  }

  return 1;
}
#endif /* ifndef NDEBUG */

#ifdef SQLITE_DEBUG 
/*
** Return a pointer to a human readable string in a static buffer
** containing the state of the Pager object passed as an argument. This
** is intended to be used within debuggers. For example, as an alternative
** to "print *pPager" in gdb:
**
** (gdb) printf "%s", print_pager_state(pPager)
**
** This routine has external linkage in order to suppress compiler warnings
** about an unused function.  It is enclosed within SQLITE_DEBUG and so does
** not appear in normal builds.
*/
char *print_pager_state(Pager *p){
  static char zRet[1024];

  sqlite3_snprintf(1024, zRet,
      "Filename:      %s\n"
      "State:         %s errCode=%d\n"
      "Lock:          %s\n"
      "Locking mode:  locking_mode=%s\n"
      "Journal mode:  journal_mode=%s\n"
      "Backing store: tempFile=%d memDb=%d useJournal=%d\n"
      "Journal:       journalOff=%lld journalHdr=%lld\n"
      "Size:          dbsize=%d dbOrigSize=%d dbFileSize=%d\n"
      , p->zFilename
      , p->eState==PAGER_OPEN            ? "OPEN" :
        p->eState==PAGER_READER          ? "READER" :
        p->eState==PAGER_WRITER_LOCKED   ? "WRITER_LOCKED" :
        p->eState==PAGER_WRITER_CACHEMOD ? "WRITER_CACHEMOD" :
        p->eState==PAGER_WRITER_DBMOD    ? "WRITER_DBMOD" :
        p->eState==PAGER_WRITER_FINISHED ? "WRITER_FINISHED" :
        p->eState==PAGER_ERROR           ? "ERROR" : "?error?"
      , (int)p->errCode
      , p->eLock==NO_LOCK         ? "NO_LOCK" :
        p->eLock==RESERVED_LOCK   ? "RESERVED" :
        p->eLock==EXCLUSIVE_LOCK  ? "EXCLUSIVE" :
        p->eLock==SHARED_LOCK     ? "SHARED" :
        p->eLock==UNKNOWN_LOCK    ? "UNKNOWN" : "?error?"
      , p->exclusiveMode ? "exclusive" : "normal"
      , p->journalMode==PAGER_JOURNALMODE_MEMORY   ? "memory" :
        p->journalMode==PAGER_JOURNALMODE_OFF      ? "off" :
        p->journalMode==PAGER_JOURNALMODE_DELETE   ? "delete" :
        p->journalMode==PAGER_JOURNALMODE_PERSIST  ? "persist" :
        p->journalMode==PAGER_JOURNALMODE_TRUNCATE ? "truncate" :
        p->journalMode==PAGER_JOURNALMODE_WAL      ? "wal" : "?error?"
      , (int)p->tempFile, (int)p->memDb, (int)p->useJournal
      , p->journalOff, p->journalHdr
      , (int)p->dbSize, (int)p->dbOrigSize, (int)p->dbFileSize
  );

  return zRet;
}
#endif

/* Forward references to the various page getters */
static int getPageNormal(Pager*,Pgno,DbPage**,int);
static int getPageError(Pager*,Pgno,DbPage**,int);
#if SQLITE_MAX_MMAP_SIZE>0
static int getPageMMap(Pager*,Pgno,DbPage**,int);
#endif

/*
** Set the Pager.xGet method for the appropriate routine used to fetch
** content from the pager.
*/
static void setGetterMethod(Pager *pPager){
  if( pPager->errCode ){
    pPager->xGet = getPageError;
#if SQLITE_MAX_MMAP_SIZE>0
  }else if( USEFETCH(pPager)
#ifdef SQLITE_HAS_CODEC
   && pPager->xCodec==0
#endif
  ){
    pPager->xGet = getPageMMap;
#endif /* SQLITE_MAX_MMAP_SIZE>0 */
  }else{
    pPager->xGet = getPageNormal;
  }
}

/*
** Return true if it is necessary to write page *pPg into the sub-journal.
** A page needs to be written into the sub-journal if there exists one
** or more open savepoints for which:
**
**   * The page-number is less than or equal to PagerSavepoint.nOrig, and
**   * The bit corresponding to the page-number is not set in
**     PagerSavepoint.pInSavepoint.
*/
static int subjRequiresPage(PgHdr *pPg){
  Pager *pPager = pPg->pPager;
  PagerSavepoint *p;
  Pgno pgno = pPg->pgno;
  int i;
  for(i=0; i<pPager->nSavepoint; i++){
    p = &pPager->aSavepoint[i];
    if( p->nOrig>=pgno && 0==sqlite3BitvecTestNotNull(p->pInSavepoint, pgno) ){
      return 1;
    }
  }
  return 0;
}

#ifdef SQLITE_DEBUG
/*
** Return true if the page is already in the journal file.
*/
static int pageInJournal(Pager *pPager, PgHdr *pPg){
  return sqlite3BitvecTest(pPager->pInJournal, pPg->pgno);
}
#endif

/*
** Read a 32-bit integer from the given file descriptor.  Store the integer
** that is read in *pRes.  Return SQLITE_OK if everything worked, or an
** error code is something goes wrong.
**
** All values are stored on disk as big-endian.
*/
static int read32bits(sqlite3_file *fd, i64 offset, u32 *pRes){
  unsigned char ac[4];
  int rc = sqlite3OsRead(fd, ac, sizeof(ac), offset);
  if( rc==SQLITE_OK ){
    *pRes = sqlite3Get4byte(ac);
  }
  return rc;
}

/*
** Write a 32-bit integer into a string buffer in big-endian byte order.
*/
#define put32bits(A,B)  sqlite3Put4byte((u8*)A,B)


/*
** Write a 32-bit integer into the given file descriptor.  Return SQLITE_OK
** on success or an error code is something goes wrong.
*/
static int write32bits(sqlite3_file *fd, i64 offset, u32 val){
  //char ac[4];
  char* ac = uk_malloc(flexos_shared_alloc, 4 * sizeof(char));
  put32bits(ac, val);
  int ret = sqlite3OsWrite(fd, ac, 4, offset);
  uk_free(flexos_shared_alloc, ac);
  return ret;
}

/*
** Unlock the database file to level eLock, which must be either NO_LOCK
** or SHARED_LOCK. Regardless of whether or not the call to xUnlock()
** succeeds, set the Pager.eLock variable to match the (attempted) new lock.
**
** Except, if Pager.eLock is set to UNKNOWN_LOCK when this function is
** called, do not modify it. See the comment above the #define of 
** UNKNOWN_LOCK for an explanation of this.
*/
static int pagerUnlockDb(Pager *pPager, int eLock){
  int rc = SQLITE_OK;

  assert( !pPager->exclusiveMode || pPager->eLock==eLock );
  assert( eLock==NO_LOCK || eLock==SHARED_LOCK );
  assert( eLock!=NO_LOCK || pagerUseWal(pPager)==0 );
  if( isOpen(pPager->fd) ){
    assert( pPager->eLock>=eLock );
    rc = pPager->noLock ? SQLITE_OK : sqlite3OsUnlock(pPager->fd, eLock);
    if( pPager->eLock!=UNKNOWN_LOCK ){
      pPager->eLock = (u8)eLock;
    }
    IOTRACE(("UNLOCK %p %d\n", pPager, eLock))
  }
  return rc;
}

/*
** Lock the database file to level eLock, which must be either SHARED_LOCK,
** RESERVED_LOCK or EXCLUSIVE_LOCK. If the caller is successful, set the
** Pager.eLock variable to the new locking state. 
**
** Except, if Pager.eLock is set to UNKNOWN_LOCK when this function is 
** called, do not modify it unless the new locking state is EXCLUSIVE_LOCK. 
** See the comment above the #define of UNKNOWN_LOCK for an explanation 
** of this.
*/
static int pagerLockDb(Pager *pPager, int eLock){
  int rc = SQLITE_OK;

  assert( eLock==SHARED_LOCK || eLock==RESERVED_LOCK || eLock==EXCLUSIVE_LOCK );
  if( pPager->eLock<eLock || pPager->eLock==UNKNOWN_LOCK ){
    rc = pPager->noLock ? SQLITE_OK : sqlite3OsLock(pPager->fd, eLock);
    if( rc==SQLITE_OK && (pPager->eLock!=UNKNOWN_LOCK||eLock==EXCLUSIVE_LOCK) ){
      pPager->eLock = (u8)eLock;
      IOTRACE(("LOCK %p %d\n", pPager, eLock))
    }
  }
  return rc;
}

/*
** This function determines whether or not the atomic-write or
** atomic-batch-write optimizations can be used with this pager. The
** atomic-write optimization can be used if:
**
**  (a) the value returned by OsDeviceCharacteristics() indicates that
**      a database page may be written atomically, and
**  (b) the value returned by OsSectorSize() is less than or equal
**      to the page size.
**
** If it can be used, then the value returned is the size of the journal 
** file when it contains rollback data for exactly one page.
**
** The atomic-batch-write optimization can be used if OsDeviceCharacteristics()
** returns a value with the SQLITE_IOCAP_BATCH_ATOMIC bit set. -1 is
** returned in this case.
**
** If neither optimization can be used, 0 is returned.
*/
static int jrnlBufferSize(Pager *pPager){
  assert( !MEMDB );

#if defined(SQLITE_ENABLE_ATOMIC_WRITE) \
 || defined(SQLITE_ENABLE_BATCH_ATOMIC_WRITE)
  int dc;                           /* Device characteristics */

  assert( isOpen(pPager->fd) );
  dc = sqlite3OsDeviceCharacteristics(pPager->fd);
#else
  UNUSED_PARAMETER(pPager);
#endif

#ifdef SQLITE_ENABLE_BATCH_ATOMIC_WRITE
  if( pPager->dbSize>0 && (dc&SQLITE_IOCAP_BATCH_ATOMIC) ){
    return -1;
  }
#endif

#ifdef SQLITE_ENABLE_ATOMIC_WRITE
  {
    int nSector = pPager->sectorSize;
    int szPage = pPager->pageSize;

    assert(SQLITE_IOCAP_ATOMIC512==(512>>8));
    assert(SQLITE_IOCAP_ATOMIC64K==(65536>>8));
    if( 0==(dc&(SQLITE_IOCAP_ATOMIC|(szPage>>8)) || nSector>szPage) ){
      return 0;
    }
  }

  return JOURNAL_HDR_SZ(pPager) + JOURNAL_PG_SZ(pPager);
#endif

  return 0;
}

/*
** If SQLITE_CHECK_PAGES is defined then we do some sanity checking
** on the cache using a hash function.  This is used for testing
** and debugging only.
*/
#ifdef SQLITE_CHECK_PAGES
/*
** Return a 32-bit hash of the page data for pPage.
*/
static u32 pager_datahash(int nByte, unsigned char *pData){
  u32 hash = 0;
  int i;
  for(i=0; i<nByte; i++){
    hash = (hash*1039) + pData[i];
  }
  return hash;
}
static u32 pager_pagehash(PgHdr *pPage){
  return pager_datahash(pPage->pPager->pageSize, (unsigned char *)pPage->pData);
}
static void pager_set_pagehash(PgHdr *pPage){
  pPage->pageHash = pager_pagehash(pPage);
}

/*
** The CHECK_PAGE macro takes a PgHdr* as an argument. If SQLITE_CHECK_PAGES
** is defined, and NDEBUG is not defined, an assert() statement checks
** that the page is either dirty or still matches the calculated page-hash.
*/
#define CHECK_PAGE(x) checkPage(x)
static void checkPage(PgHdr *pPg){
  Pager *pPager = pPg->pPager;
  assert( pPager->eState!=PAGER_ERROR );
  assert( (pPg->flags&PGHDR_DIRTY) || pPg->pageHash==pager_pagehash(pPg) );
}

#else
#define pager_datahash(X,Y)  0
#define pager_pagehash(X)  0
#define pager_set_pagehash(X)
#define CHECK_PAGE(x)
#endif  /* SQLITE_CHECK_PAGES */

/*
** When this is called the journal file for pager pPager must be open.
** This function attempts to read a master journal file name from the 
** end of the file and, if successful, copies it into memory supplied 
** by the caller. See comments above writeMasterJournal() for the format
** used to store a master journal file name at the end of a journal file.
**
** zMaster must point to a buffer of at least nMaster bytes allocated by
** the caller. This should be sqlite3_vfs.mxPathname+1 (to ensure there is
** enough space to write the master journal name). If the master journal
** name in the journal is longer than nMaster bytes (including a
** nul-terminator), then this is handled as if no master journal name
** were present in the journal.
**
** If a master journal file name is present at the end of the journal
** file, then it is copied into the buffer pointed to by zMaster. A
** nul-terminator byte is appended to the buffer following the master
** journal file name.
**
** If it is determined that no master journal file name is present 
** zMaster[0] is set to 0 and SQLITE_OK returned.
**
** If an error occurs while reading from the journal file, an SQLite
** error code is returned.
*/
static int readMasterJournal(sqlite3_file *pJrnl, char *zMaster, u32 nMaster){
  int rc;                    /* Return code */
  u32 len;                   /* Length in bytes of master journal name */
  i64 szJ;                   /* Total size in bytes of journal file pJrnl */
  u32 cksum;                 /* MJ checksum value read from journal */
  u32 u;                     /* Unsigned loop counter */
  unsigned char aMagic[8];   /* A buffer to hold the magic header */
  zMaster[0] = '\0';

  if( SQLITE_OK!=(rc = sqlite3OsFileSize(pJrnl, &szJ))
   || szJ<16
   || SQLITE_OK!=(rc = read32bits(pJrnl, szJ-16, &len))
   || len>=nMaster 
   || len>szJ-16
   || len==0 
   || SQLITE_OK!=(rc = read32bits(pJrnl, szJ-12, &cksum))
   || SQLITE_OK!=(rc = sqlite3OsRead(pJrnl, aMagic, 8, szJ-8))
   || memcmp(aMagic, aJournalMagic, 8)
   || SQLITE_OK!=(rc = sqlite3OsRead(pJrnl, zMaster, len, szJ-16-len))
  ){
    return rc;
  }

  /* See if the checksum matches the master journal name */
  for(u=0; u<len; u++){
    cksum -= zMaster[u];
  }
  if( cksum ){
    /* If the checksum doesn't add up, then one or more of the disk sectors
    ** containing the master journal filename is corrupted. This means
    ** definitely roll back, so just return SQLITE_OK and report a (nul)
    ** master-journal filename.
    */
    len = 0;
  }
  zMaster[len] = '\0';
   
  return SQLITE_OK;
}

/*
** Return the offset of the sector boundary at or immediately 
** following the value in pPager->journalOff, assuming a sector 
** size of pPager->sectorSize bytes.
**
** i.e for a sector size of 512:
**
**   Pager.journalOff          Return value
**   ---------------------------------------
**   0                         0
**   512                       512
**   100                       512
**   2000                      2048
** 
*/
static i64 journalHdrOffset(Pager *pPager){
  i64 offset = 0;
  i64 c = pPager->journalOff;
  if( c ){
    offset = ((c-1)/JOURNAL_HDR_SZ(pPager) + 1) * JOURNAL_HDR_SZ(pPager);
  }
  assert( offset%JOURNAL_HDR_SZ(pPager)==0 );
  assert( offset>=c );
  assert( (offset-c)<JOURNAL_HDR_SZ(pPager) );
  return offset;
}

/*
** The journal file must be open when this function is called.
**
** This function is a no-op if the journal file has not been written to
** within the current transaction (i.e. if Pager.journalOff==0).
**
** If doTruncate is non-zero or the Pager.journalSizeLimit variable is
** set to 0, then truncate the journal file to zero bytes in size. Otherwise,
** zero the 28-byte header at the start of the journal file. In either case, 
** if the pager is not in no-sync mode, sync the journal file immediately 
** after writing or truncating it.
**
** If Pager.journalSizeLimit is set to a positive, non-zero value, and
** following the truncation or zeroing described above the size of the 
** journal file in bytes is larger than this value, then truncate the
** journal file to Pager.journalSizeLimit bytes. The journal file does
** not need to be synced following this operation.
**
** If an IO error occurs, abandon processing and return the IO error code.
** Otherwise, return SQLITE_OK.
*/
static int zeroJournalHdr(Pager *pPager, int doTruncate){
  int rc = SQLITE_OK;                               /* Return code */
  assert( isOpen(pPager->jfd) );
  assert( !sqlite3JournalIsInMemory(pPager->jfd) );
  if( pPager->journalOff ){
    const i64 iLimit = pPager->journalSizeLimit;    /* Local cache of jsl */

    IOTRACE(("JZEROHDR %p\n", pPager))
    if( doTruncate || iLimit==0 ){
      rc = sqlite3OsTruncate(pPager->jfd, 0);
    }else{
      static const char zeroHdr[28] = {0};
      rc = sqlite3OsWrite(pPager->jfd, zeroHdr, sizeof(zeroHdr), 0);
    }
    if( rc==SQLITE_OK && !pPager->noSync ){
      rc = sqlite3OsSync(pPager->jfd, SQLITE_SYNC_DATAONLY|pPager->syncFlags);
    }

    /* At this point the transaction is committed but the write lock 
    ** is still held on the file. If there is a size limit configured for 
    ** the persistent journal and the journal file currently consumes more
    ** space than that limit allows for, truncate it now. There is no need
    ** to sync the file following this operation.
    */
    if( rc==SQLITE_OK && iLimit>0 ){
      i64 sz;
      rc = sqlite3OsFileSize(pPager->jfd, &sz);
      if( rc==SQLITE_OK && sz>iLimit ){
        rc = sqlite3OsTruncate(pPager->jfd, iLimit);
      }
    }
  }
  return rc;
}

/*
** The journal file must be open when this routine is called. A journal
** header (JOURNAL_HDR_SZ bytes) is written into the journal file at the
** current location.
**
** The format for the journal header is as follows:
** - 8 bytes: Magic identifying journal format.
** - 4 bytes: Number of records in journal, or -1 no-sync mode is on.
** - 4 bytes: Random number used for page hash.
** - 4 bytes: Initial database page count.
** - 4 bytes: Sector size used by the process that wrote this journal.
** - 4 bytes: Database page size.
** 
** Followed by (JOURNAL_HDR_SZ - 28) bytes of unused space.
*/
static int writeJournalHdr(Pager *pPager){
  int rc = SQLITE_OK;                 /* Return code */
  char *zHeader = pPager->pTmpSpace;  /* Temporary space used to build header */
  u32 nHeader = (u32)pPager->pageSize;/* Size of buffer pointed to by zHeader */
  u32 nWrite;                         /* Bytes of header sector written */
  int ii;                             /* Loop counter */

  assert( isOpen(pPager->jfd) );      /* Journal file must be open. */

  if( nHeader>JOURNAL_HDR_SZ(pPager) ){
    nHeader = JOURNAL_HDR_SZ(pPager);
  }

  /* If there are active savepoints and any of them were created 
  ** since the most recent journal header was written, update the 
  ** PagerSavepoint.iHdrOffset fields now.
  */
  for(ii=0; ii<pPager->nSavepoint; ii++){
    if( pPager->aSavepoint[ii].iHdrOffset==0 ){
      pPager->aSavepoint[ii].iHdrOffset = pPager->journalOff;
    }
  }

  pPager->journalHdr = pPager->journalOff = journalHdrOffset(pPager);

  /* 
  ** Write the nRec Field - the number of page records that follow this
  ** journal header. Normally, zero is written to this value at this time.
  ** After the records are added to the journal (and the journal synced, 
  ** if in full-sync mode), the zero is overwritten with the true number
  ** of records (see syncJournal()).
  **
  ** A faster alternative is to write 0xFFFFFFFF to the nRec field. When
  ** reading the journal this value tells SQLite to assume that the
  ** rest of the journal file contains valid page records. This assumption
  ** is dangerous, as if a failure occurred whilst writing to the journal
  ** file it may contain some garbage data. There are two scenarios
  ** where this risk can be ignored:
  **
  **   * When the pager is in no-sync mode. Corruption can follow a
  **     power failure in this case anyway.
  **
  **   * When the SQLITE_IOCAP_SAFE_APPEND flag is set. This guarantees
  **     that garbage data is never appended to the journal file.
  */
  assert( isOpen(pPager->fd) || pPager->noSync );
  if( pPager->noSync || (pPager->journalMode==PAGER_JOURNALMODE_MEMORY)
   || (sqlite3OsDeviceCharacteristics(pPager->fd)&SQLITE_IOCAP_SAFE_APPEND) 
  ){
    memcpy(zHeader, aJournalMagic, sizeof(aJournalMagic));
    put32bits(&zHeader[sizeof(aJournalMagic)], 0xffffffff);
  }else{
    memset(zHeader, 0, sizeof(aJournalMagic)+4);
  }

  /* The random check-hash initializer */ 
  sqlite3_randomness(sizeof(pPager->cksumInit), &pPager->cksumInit);
  put32bits(&zHeader[sizeof(aJournalMagic)+4], pPager->cksumInit);
  /* The initial database size */
  put32bits(&zHeader[sizeof(aJournalMagic)+8], pPager->dbOrigSize);
  /* The assumed sector size for this process */
  put32bits(&zHeader[sizeof(aJournalMagic)+12], pPager->sectorSize);

  /* The page size */
  put32bits(&zHeader[sizeof(aJournalMagic)+16], pPager->pageSize);

  /* Initializing the tail of the buffer is not necessary.  Everything
  ** works find if the following memset() is omitted.  But initializing
  ** the memory prevents valgrind from complaining, so we are willing to
  ** take the performance hit.
  */
  memset(&zHeader[sizeof(aJournalMagic)+20], 0,
         nHeader-(sizeof(aJournalMagic)+20));

  /* In theory, it is only necessary to write the 28 bytes that the 
  ** journal header consumes to the journal file here. Then increment the 
  ** Pager.journalOff variable by JOURNAL_HDR_SZ so that the next 
  ** record is written to the following sector (leaving a gap in the file
  ** that will be implicitly filled in by the OS).
  **
  ** However it has been discovered that on some systems this pattern can 
  ** be significantly slower than contiguously writing data to the file,
  ** even if that means explicitly writing data to the block of 
  ** (JOURNAL_HDR_SZ - 28) bytes that will not be used. So that is what
  ** is done. 
  **
  ** The loop is required here in case the sector-size is larger than the 
  ** database page size. Since the zHeader buffer is only Pager.pageSize
  ** bytes in size, more than one call to sqlite3OsWrite() may be required
  ** to populate the entire journal header sector.
  */ 
  for(nWrite=0; rc==SQLITE_OK&&nWrite<JOURNAL_HDR_SZ(pPager); nWrite+=nHeader){
    IOTRACE(("JHDR %p %lld %d\n", pPager, pPager->journalHdr, nHeader))
    rc = sqlite3OsWrite(pPager->jfd, zHeader, nHeader, pPager->journalOff);
    assert( pPager->journalHdr <= pPager->journalOff );
    pPager->journalOff += nHeader;
  }

  return rc;
}

/*
** The journal file must be open when this is called. A journal header file
** (JOURNAL_HDR_SZ bytes) is read from the current location in the journal
** file. The current location in the journal file is given by
** pPager->journalOff. See comments above function writeJournalHdr() for
** a description of the journal header format.
**
** If the header is read successfully, *pNRec is set to the number of
** page records following this header and *pDbSize is set to the size of the
** database before the transaction began, in pages. Also, pPager->cksumInit
** is set to the value read from the journal header. SQLITE_OK is returned
** in this case.
**
** If the journal header file appears to be corrupted, SQLITE_DONE is
** returned and *pNRec and *PDbSize are undefined.  If JOURNAL_HDR_SZ bytes
** cannot be read from the journal file an error code is returned.
*/
static int readJournalHdr(
  Pager *pPager,               /* Pager object */
  int isHot,
  i64 journalSize,             /* Size of the open journal file in bytes */
  u32 *pNRec,                  /* OUT: Value read from the nRec field */
  u32 *pDbSize                 /* OUT: Value of original database size field */
){
  int rc;                      /* Return code */
  unsigned char aMagic[8];     /* A buffer to hold the magic header */
  i64 iHdrOff;                 /* Offset of journal header being read */

  assert( isOpen(pPager->jfd) );      /* Journal file must be open. */

  /* Advance Pager.journalOff to the start of the next sector. If the
  ** journal file is too small for there to be a header stored at this
  ** point, return SQLITE_DONE.
  */
  pPager->journalOff = journalHdrOffset(pPager);
  if( pPager->journalOff+JOURNAL_HDR_SZ(pPager) > journalSize ){
    return SQLITE_DONE;
  }
  iHdrOff = pPager->journalOff;

  /* Read in the first 8 bytes of the journal header. If they do not match
  ** the  magic string found at the start of each journal header, return
  ** SQLITE_DONE. If an IO error occurs, return an error code. Otherwise,
  ** proceed.
  */
  if( isHot || iHdrOff!=pPager->journalHdr ){
    rc = sqlite3OsRead(pPager->jfd, aMagic, sizeof(aMagic), iHdrOff);
    if( rc ){
      return rc;
    }
    if( memcmp(aMagic, aJournalMagic, sizeof(aMagic))!=0 ){
      return SQLITE_DONE;
    }
  }

  /* Read the first three 32-bit fields of the journal header: The nRec
  ** field, the checksum-initializer and the database size at the start
  ** of the transaction. Return an error code if anything goes wrong.
  */
  if( SQLITE_OK!=(rc = read32bits(pPager->jfd, iHdrOff+8, pNRec))
   || SQLITE_OK!=(rc = read32bits(pPager->jfd, iHdrOff+12, &pPager->cksumInit))
   || SQLITE_OK!=(rc = read32bits(pPager->jfd, iHdrOff+16, pDbSize))
  ){
    return rc;
  }

  if( pPager->journalOff==0 ){
    u32 iPageSize;               /* Page-size field of journal header */
    u32 iSectorSize;             /* Sector-size field of journal header */

    /* Read the page-size and sector-size journal header fields. */
    if( SQLITE_OK!=(rc = read32bits(pPager->jfd, iHdrOff+20, &iSectorSize))
     || SQLITE_OK!=(rc = read32bits(pPager->jfd, iHdrOff+24, &iPageSize))
    ){
      return rc;
    }

    /* Versions of SQLite prior to 3.5.8 set the page-size field of the
    ** journal header to zero. In this case, assume that the Pager.pageSize
    ** variable is already set to the correct page size.
    */
    if( iPageSize==0 ){
      iPageSize = pPager->pageSize;
    }

    /* Check that the values read from the page-size and sector-size fields
    ** are within range. To be 'in range', both values need to be a power
    ** of two greater than or equal to 512 or 32, and not greater than their 
    ** respective compile time maximum limits.
    */
    if( iPageSize<512                  || iSectorSize<32
     || iPageSize>SQLITE_MAX_PAGE_SIZE || iSectorSize>MAX_SECTOR_SIZE
     || ((iPageSize-1)&iPageSize)!=0   || ((iSectorSize-1)&iSectorSize)!=0 
    ){
      /* If the either the page-size or sector-size in the journal-header is 
      ** invalid, then the process that wrote the journal-header must have 
      ** crashed before the header was synced. In this case stop reading 
      ** the journal file here.
      */
      return SQLITE_DONE;
    }

    /* Update the page-size to match the value read from the journal. 
    ** Use a testcase() macro to make sure that malloc failure within 
    ** PagerSetPagesize() is tested.
    */
    rc = sqlite3PagerSetPagesize(pPager, &iPageSize, -1);
    testcase( rc!=SQLITE_OK );

    /* Update the assumed sector-size to match the value used by 
    ** the process that created this journal. If this journal was
    ** created by a process other than this one, then this routine
    ** is being called from within pager_playback(). The local value
    ** of Pager.sectorSize is restored at the end of that routine.
    */
    pPager->sectorSize = iSectorSize;
  }

  pPager->journalOff += JOURNAL_HDR_SZ(pPager);
  return rc;
}


/*
** Write the supplied master journal name into the journal file for pager
** pPager at the current location. The master journal name must be the last
** thing written to a journal file. If the pager is in full-sync mode, the
** journal file descriptor is advanced to the next sector boundary before
** anything is written. The format is:
**
**   + 4 bytes: PAGER_MJ_PGNO.
**   + N bytes: Master journal filename in utf-8.
**   + 4 bytes: N (length of master journal name in bytes, no nul-terminator).
**   + 4 bytes: Master journal name checksum.
**   + 8 bytes: aJournalMagic[].
**
** The master journal page checksum is the sum of the bytes in the master
** journal name, where each byte is interpreted as a signed 8-bit integer.
**
** If zMaster is a NULL pointer (occurs for a single database transaction), 
** this call is a no-op.
*/
static int writeMasterJournal(Pager *pPager, const char *zMaster){
  int rc;                          /* Return code */
  int nMaster;                     /* Length of string zMaster */
  i64 iHdrOff;                     /* Offset of header in journal file */
  i64 jrnlSize;                    /* Size of journal file on disk */
  u32 cksum = 0;                   /* Checksum of string zMaster */

  assert( pPager->setMaster==0 );
  assert( !pagerUseWal(pPager) );

  if( !zMaster 
   || pPager->journalMode==PAGER_JOURNALMODE_MEMORY 
   || !isOpen(pPager->jfd)
  ){
    return SQLITE_OK;
  }
  pPager->setMaster = 1;
  assert( pPager->journalHdr <= pPager->journalOff );

  /* Calculate the length in bytes and the checksum of zMaster */
  for(nMaster=0; zMaster[nMaster]; nMaster++){
    cksum += zMaster[nMaster];
  }

  /* If in full-sync mode, advance to the next disk sector before writing
  ** the master journal name. This is in case the previous page written to
  ** the journal has already been synced.
  */
  if( pPager->fullSync ){
    pPager->journalOff = journalHdrOffset(pPager);
  }
  iHdrOff = pPager->journalOff;

  /* Write the master journal data to the end of the journal file. If
  ** an error occurs, return the error code to the caller.
  */
  if( (0 != (rc = write32bits(pPager->jfd, iHdrOff, PAGER_MJ_PGNO(pPager))))
   || (0 != (rc = sqlite3OsWrite(pPager->jfd, zMaster, nMaster, iHdrOff+4)))
   || (0 != (rc = write32bits(pPager->jfd, iHdrOff+4+nMaster, nMaster)))
   || (0 != (rc = write32bits(pPager->jfd, iHdrOff+4+nMaster+4, cksum)))
   || (0 != (rc = sqlite3OsWrite(pPager->jfd, aJournalMagic, 8,
                                 iHdrOff+4+nMaster+8)))
  ){
    return rc;
  }
  pPager->journalOff += (nMaster+20);

  /* If the pager is in peristent-journal mode, then the physical 
  ** journal-file may extend past the end of the master-journal name
  ** and 8 bytes of magic data just written to the file. This is 
  ** dangerous because the code to rollback a hot-journal file
  ** will not be able to find the master-journal name to determine 
  ** whether or not the journal is hot. 
  **
  ** Easiest thing to do in this scenario is to truncate the journal 
  ** file to the required size.
  */ 
  if( SQLITE_OK==(rc = sqlite3OsFileSize(pPager->jfd, &jrnlSize))
   && jrnlSize>pPager->journalOff
  ){
    rc = sqlite3OsTruncate(pPager->jfd, pPager->journalOff);
  }
  return rc;
}

/*
** Discard the entire contents of the in-memory page-cache.
*/
static void pager_reset(Pager *pPager){
  pPager->iDataVersion++;
  sqlite3BackupRestart(pPager->pBackup);
  sqlite3PcacheClear(pPager->pPCache);
}

/*
** Return the pPager->iDataVersion value
*/
SQLITE_PRIVATE u32 sqlite3PagerDataVersion(Pager *pPager){
  return pPager->iDataVersion;
}

/*
** Free all structures in the Pager.aSavepoint[] array and set both
** Pager.aSavepoint and Pager.nSavepoint to zero. Close the sub-journal
** if it is open and the pager is not in exclusive mode.
*/
static void releaseAllSavepoints(Pager *pPager){
  int ii;               /* Iterator for looping through Pager.aSavepoint */
  for(ii=0; ii<pPager->nSavepoint; ii++){
    sqlite3BitvecDestroy(pPager->aSavepoint[ii].pInSavepoint);
  }
  if( !pPager->exclusiveMode || sqlite3JournalIsInMemory(pPager->sjfd) ){
    sqlite3OsClose(pPager->sjfd);
  }
  sqlite3_free(pPager->aSavepoint);
  pPager->aSavepoint = 0;
  pPager->nSavepoint = 0;
  pPager->nSubRec = 0;
}

/*
** Set the bit number pgno in the PagerSavepoint.pInSavepoint 
** bitvecs of all open savepoints. Return SQLITE_OK if successful
** or SQLITE_NOMEM if a malloc failure occurs.
*/
static int addToSavepointBitvecs(Pager *pPager, Pgno pgno){
  int ii;                   /* Loop counter */
  int rc = SQLITE_OK;       /* Result code */

  for(ii=0; ii<pPager->nSavepoint; ii++){
    PagerSavepoint *p = &pPager->aSavepoint[ii];
    if( pgno<=p->nOrig ){
      rc |= sqlite3BitvecSet(p->pInSavepoint, pgno);
      testcase( rc==SQLITE_NOMEM );
      assert( rc==SQLITE_OK || rc==SQLITE_NOMEM );
    }
  }
  return rc;
}

/*
** This function is a no-op if the pager is in exclusive mode and not
** in the ERROR state. Otherwise, it switches the pager to PAGER_OPEN
** state.
**
** If the pager is not in exclusive-access mode, the database file is
** completely unlocked. If the file is unlocked and the file-system does
** not exhibit the UNDELETABLE_WHEN_OPEN property, the journal file is
** closed (if it is open).
**
** If the pager is in ERROR state when this function is called, the 
** contents of the pager cache are discarded before switching back to 
** the OPEN state. Regardless of whether the pager is in exclusive-mode
** or not, any journal file left in the file-system will be treated
** as a hot-journal and rolled back the next time a read-transaction
** is opened (by this or by any other connection).
*/
static void pager_unlock(Pager *pPager){

  assert( pPager->eState==PAGER_READER 
       || pPager->eState==PAGER_OPEN 
       || pPager->eState==PAGER_ERROR 
  );

  sqlite3BitvecDestroy(pPager->pInJournal);
  pPager->pInJournal = 0;
  releaseAllSavepoints(pPager);

  if( pagerUseWal(pPager) ){
    assert( !isOpen(pPager->jfd) );
    sqlite3WalEndReadTransaction(pPager->pWal);
    pPager->eState = PAGER_OPEN;
  }else if( !pPager->exclusiveMode ){
    int rc;                       /* Error code returned by pagerUnlockDb() */
    int iDc = isOpen(pPager->fd)?sqlite3OsDeviceCharacteristics(pPager->fd):0;

    /* If the operating system support deletion of open files, then
    ** close the journal file when dropping the database lock.  Otherwise
    ** another connection with journal_mode=delete might delete the file
    ** out from under us.
    */
    assert( (PAGER_JOURNALMODE_MEMORY   & 5)!=1 );
    assert( (PAGER_JOURNALMODE_OFF      & 5)!=1 );
    assert( (PAGER_JOURNALMODE_WAL      & 5)!=1 );
    assert( (PAGER_JOURNALMODE_DELETE   & 5)!=1 );
    assert( (PAGER_JOURNALMODE_TRUNCATE & 5)==1 );
    assert( (PAGER_JOURNALMODE_PERSIST  & 5)==1 );
    if( 0==(iDc & SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN)
     || 1!=(pPager->journalMode & 5)
    ){
      sqlite3OsClose(pPager->jfd);
    }

    /* If the pager is in the ERROR state and the call to unlock the database
    ** file fails, set the current lock to UNKNOWN_LOCK. See the comment
    ** above the #define for UNKNOWN_LOCK for an explanation of why this
    ** is necessary.
    */
    rc = pagerUnlockDb(pPager, NO_LOCK);
    if( rc!=SQLITE_OK && pPager->eState==PAGER_ERROR ){
      pPager->eLock = UNKNOWN_LOCK;
    }

    /* The pager state may be changed from PAGER_ERROR to PAGER_OPEN here
    ** without clearing the error code. This is intentional - the error
    ** code is cleared and the cache reset in the block below.
    */
    assert( pPager->errCode || pPager->eState!=PAGER_ERROR );
    pPager->changeCountDone = 0;
    pPager->eState = PAGER_OPEN;
  }

  /* If Pager.errCode is set, the contents of the pager cache cannot be
  ** trusted. Now that there are no outstanding references to the pager,
  ** it can safely move back to PAGER_OPEN state. This happens in both
  ** normal and exclusive-locking mode.
  */
  assert( pPager->errCode==SQLITE_OK || !MEMDB );
  if( pPager->errCode ){
    if( pPager->tempFile==0 ){
      pager_reset(pPager);
      pPager->changeCountDone = 0;
      pPager->eState = PAGER_OPEN;
    }else{
      pPager->eState = (isOpen(pPager->jfd) ? PAGER_OPEN : PAGER_READER);
    }
    if( USEFETCH(pPager) ) sqlite3OsUnfetch(pPager->fd, 0, 0);
    pPager->errCode = SQLITE_OK;
    setGetterMethod(pPager);
  }

  pPager->journalOff = 0;
  pPager->journalHdr = 0;
  pPager->setMaster = 0;
}

/*
** This function is called whenever an IOERR or FULL error that requires
** the pager to transition into the ERROR state may ahve occurred.
** The first argument is a pointer to the pager structure, the second 
** the error-code about to be returned by a pager API function. The 
** value returned is a copy of the second argument to this function. 
**
** If the second argument is SQLITE_FULL, SQLITE_IOERR or one of the
** IOERR sub-codes, the pager enters the ERROR state and the error code
** is stored in Pager.errCode. While the pager remains in the ERROR state,
** all major API calls on the Pager will immediately return Pager.errCode.
**
** The ERROR state indicates that the contents of the pager-cache 
** cannot be trusted. This state can be cleared by completely discarding 
** the contents of the pager-cache. If a transaction was active when
** the persistent error occurred, then the rollback journal may need
** to be replayed to restore the contents of the database file (as if
** it were a hot-journal).
*/
static int pager_error(Pager *pPager, int rc){
  int rc2 = rc & 0xff;
  assert( rc==SQLITE_OK || !MEMDB );
  assert(
       pPager->errCode==SQLITE_FULL ||
       pPager->errCode==SQLITE_OK ||
       (pPager->errCode & 0xff)==SQLITE_IOERR
  );
  if( rc2==SQLITE_FULL || rc2==SQLITE_IOERR ){
    pPager->errCode = rc;
    pPager->eState = PAGER_ERROR;
    setGetterMethod(pPager);
  }
  return rc;
}

static int pager_truncate(Pager *pPager, Pgno nPage);

/*
** The write transaction open on pPager is being committed (bCommit==1)
** or rolled back (bCommit==0).
**
** Return TRUE if and only if all dirty pages should be flushed to disk.
**
** Rules:
**
**   *  For non-TEMP databases, always sync to disk.  This is necessary
**      for transactions to be durable.
**
**   *  Sync TEMP database only on a COMMIT (not a ROLLBACK) when the backing
**      file has been created already (via a spill on pagerStress()) and
**      when the number of dirty pages in memory exceeds 25% of the total
**      cache size.
*/
static int pagerFlushOnCommit(Pager *pPager, int bCommit){
  if( pPager->tempFile==0 ) return 1;
  if( !bCommit ) return 0;
  if( !isOpen(pPager->fd) ) return 0;
  return (sqlite3PCachePercentDirty(pPager->pPCache)>=25);
}

/*
** This routine ends a transaction. A transaction is usually ended by 
** either a COMMIT or a ROLLBACK operation. This routine may be called 
** after rollback of a hot-journal, or if an error occurs while opening
** the journal file or writing the very first journal-header of a
** database transaction.
** 
** This routine is never called in PAGER_ERROR state. If it is called
** in PAGER_NONE or PAGER_SHARED state and the lock held is less
** exclusive than a RESERVED lock, it is a no-op.
**
** Otherwise, any active savepoints are released.
**
** If the journal file is open, then it is "finalized". Once a journal 
** file has been finalized it is not possible to use it to roll back a 
** transaction. Nor will it be considered to be a hot-journal by this
** or any other database connection. Exactly how a journal is finalized
** depends on whether or not the pager is running in exclusive mode and
** the current journal-mode (Pager.journalMode value), as follows:
**
**   journalMode==MEMORY
**     Journal file descriptor is simply closed. This destroys an 
**     in-memory journal.
**
**   journalMode==TRUNCATE
**     Journal file is truncated to zero bytes in size.
**
**   journalMode==PERSIST
**     The first 28 bytes of the journal file are zeroed. This invalidates
**     the first journal header in the file, and hence the entire journal
**     file. An invalid journal file cannot be rolled back.
**
**   journalMode==DELETE
**     The journal file is closed and deleted using sqlite3OsDelete().
**
**     If the pager is running in exclusive mode, this method of finalizing
**     the journal file is never used. Instead, if the journalMode is
**     DELETE and the pager is in exclusive mode, the method described under
**     journalMode==PERSIST is used instead.
**
** After the journal is finalized, the pager moves to PAGER_READER state.
** If running in non-exclusive rollback mode, the lock on the file is 
** downgraded to a SHARED_LOCK.
**
** SQLITE_OK is returned if no error occurs. If an error occurs during
** any of the IO operations to finalize the journal file or unlock the
** database then the IO error code is returned to the user. If the 
** operation to finalize the journal file fails, then the code still
** tries to unlock the database file if not in exclusive mode. If the
** unlock operation fails as well, then the first error code related
** to the first error encountered (the journal finalization one) is
** returned.
*/
static int pager_end_transaction(Pager *pPager, int hasMaster, int bCommit){
  int rc = SQLITE_OK;      /* Error code from journal finalization operation */
  int rc2 = SQLITE_OK;     /* Error code from db file unlock operation */

  /* Do nothing if the pager does not have an open write transaction
  ** or at least a RESERVED lock. This function may be called when there
  ** is no write-transaction active but a RESERVED or greater lock is
  ** held under two circumstances:
  **
  **   1. After a successful hot-journal rollback, it is called with
  **      eState==PAGER_NONE and eLock==EXCLUSIVE_LOCK.
  **
  **   2. If a connection with locking_mode=exclusive holding an EXCLUSIVE 
  **      lock switches back to locking_mode=normal and then executes a
  **      read-transaction, this function is called with eState==PAGER_READER 
  **      and eLock==EXCLUSIVE_LOCK when the read-transaction is closed.
  */
  assert( assert_pager_state(pPager) );
  assert( pPager->eState!=PAGER_ERROR );
  if( pPager->eState<PAGER_WRITER_LOCKED && pPager->eLock<RESERVED_LOCK ){
    return SQLITE_OK;
  }

  releaseAllSavepoints(pPager);
  assert( isOpen(pPager->jfd) || pPager->pInJournal==0 
      || (sqlite3OsDeviceCharacteristics(pPager->fd)&SQLITE_IOCAP_BATCH_ATOMIC)
  );
  if( isOpen(pPager->jfd) ){
    assert( !pagerUseWal(pPager) );

    /* Finalize the journal file. */
    if( sqlite3JournalIsInMemory(pPager->jfd) ){
      /* assert( pPager->journalMode==PAGER_JOURNALMODE_MEMORY ); */
      sqlite3OsClose(pPager->jfd);
    }else if( pPager->journalMode==PAGER_JOURNALMODE_TRUNCATE ){
      if( pPager->journalOff==0 ){
        rc = SQLITE_OK;
      }else{
        rc = sqlite3OsTruncate(pPager->jfd, 0);
        if( rc==SQLITE_OK && pPager->fullSync ){
          /* Make sure the new file size is written into the inode right away.
          ** Otherwise the journal might resurrect following a power loss and
          ** cause the last transaction to roll back.  See
          ** https://bugzilla.mozilla.org/show_bug.cgi?id=1072773
          */
          rc = sqlite3OsSync(pPager->jfd, pPager->syncFlags);
        }
      }
      pPager->journalOff = 0;
    }else if( pPager->journalMode==PAGER_JOURNALMODE_PERSIST
      || (pPager->exclusiveMode && pPager->journalMode!=PAGER_JOURNALMODE_WAL)
    ){
      rc = zeroJournalHdr(pPager, hasMaster||pPager->tempFile);
      pPager->journalOff = 0;
    }else{
      /* This branch may be executed with Pager.journalMode==MEMORY if
      ** a hot-journal was just rolled back. In this case the journal
      ** file should be closed and deleted. If this connection writes to
      ** the database file, it will do so using an in-memory journal.
      */
      int bDelete = !pPager->tempFile;
      assert( sqlite3JournalIsInMemory(pPager->jfd)==0 );
      assert( pPager->journalMode==PAGER_JOURNALMODE_DELETE 
           || pPager->journalMode==PAGER_JOURNALMODE_MEMORY 
           || pPager->journalMode==PAGER_JOURNALMODE_WAL 
      );
      sqlite3OsClose(pPager->jfd);
      if( bDelete ){
        rc = sqlite3OsDelete(pPager->pVfs, pPager->zJournal, pPager->extraSync);
      }
    }
  }

#ifdef SQLITE_CHECK_PAGES
  sqlite3PcacheIterateDirty(pPager->pPCache, pager_set_pagehash);
  if( pPager->dbSize==0 && sqlite3PcacheRefCount(pPager->pPCache)>0 ){
    PgHdr *p = sqlite3PagerLookup(pPager, 1);
    if( p ){
      p->pageHash = 0;
      sqlite3PagerUnrefNotNull(p);
    }
  }
#endif

  sqlite3BitvecDestroy(pPager->pInJournal);
  pPager->pInJournal = 0;
  pPager->nRec = 0;
  if( rc==SQLITE_OK ){
    if( MEMDB || pagerFlushOnCommit(pPager, bCommit) ){
      sqlite3PcacheCleanAll(pPager->pPCache);
    }else{
      sqlite3PcacheClearWritable(pPager->pPCache);
    }
    sqlite3PcacheTruncate(pPager->pPCache, pPager->dbSize);
  }

  if( pagerUseWal(pPager) ){
    /* Drop the WAL write-lock, if any. Also, if the connection was in 
    ** locking_mode=exclusive mode but is no longer, drop the EXCLUSIVE 
    ** lock held on the database file.
    */
    rc2 = sqlite3WalEndWriteTransaction(pPager->pWal);
    assert( rc2==SQLITE_OK );
  }else if( rc==SQLITE_OK && bCommit && pPager->dbFileSize>pPager->dbSize ){
    /* This branch is taken when committing a transaction in rollback-journal
    ** mode if the database file on disk is larger than the database image.
    ** At this point the journal has been finalized and the transaction 
    ** successfully committed, but the EXCLUSIVE lock is still held on the
    ** file. So it is safe to truncate the database file to its minimum
    ** required size.  */
    assert( pPager->eLock==EXCLUSIVE_LOCK );
    rc = pager_truncate(pPager, pPager->dbSize);
  }

  if( rc==SQLITE_OK && bCommit ){
    rc = sqlite3OsFileControl(pPager->fd, SQLITE_FCNTL_COMMIT_PHASETWO, 0);
    if( rc==SQLITE_NOTFOUND ) rc = SQLITE_OK;
  }

  if( !pPager->exclusiveMode 
   && (!pagerUseWal(pPager) || sqlite3WalExclusiveMode(pPager->pWal, 0))
  ){
    rc2 = pagerUnlockDb(pPager, SHARED_LOCK);
    pPager->changeCountDone = 0;
  }
  pPager->eState = PAGER_READER;
  pPager->setMaster = 0;

  return (rc==SQLITE_OK?rc2:rc);
}

/*
** Execute a rollback if a transaction is active and unlock the 
** database file. 
**
** If the pager has already entered the ERROR state, do not attempt 
** the rollback at this time. Instead, pager_unlock() is called. The
** call to pager_unlock() will discard all in-memory pages, unlock
** the database file and move the pager back to OPEN state. If this 
** means that there is a hot-journal left in the file-system, the next 
** connection to obtain a shared lock on the pager (which may be this one) 
** will roll it back.
**
** If the pager has not already entered the ERROR state, but an IO or
** malloc error occurs during a rollback, then this will itself cause 
** the pager to enter the ERROR state. Which will be cleared by the
** call to pager_unlock(), as described above.
*/
static void pagerUnlockAndRollback(Pager *pPager){
  if( pPager->eState!=PAGER_ERROR && pPager->eState!=PAGER_OPEN ){
    assert( assert_pager_state(pPager) );
    if( pPager->eState>=PAGER_WRITER_LOCKED ){
      sqlite3BeginBenignMalloc();
      sqlite3PagerRollback(pPager);
      sqlite3EndBenignMalloc();
    }else if( !pPager->exclusiveMode ){
      assert( pPager->eState==PAGER_READER );
      pager_end_transaction(pPager, 0, 0);
    }
  }
  pager_unlock(pPager);
}

/*
** Parameter aData must point to a buffer of pPager->pageSize bytes
** of data. Compute and return a checksum based ont the contents of the 
** page of data and the current value of pPager->cksumInit.
**
** This is not a real checksum. It is really just the sum of the 
** random initial value (pPager->cksumInit) and every 200th byte
** of the page data, starting with byte offset (pPager->pageSize%200).
** Each byte is interpreted as an 8-bit unsigned integer.
**
** Changing the formula used to compute this checksum results in an
** incompatible journal file format.
**
** If journal corruption occurs due to a power failure, the most likely 
** scenario is that one end or the other of the record will be changed. 
** It is much less likely that the two ends of the journal record will be
** correct and the middle be corrupt.  Thus, this "checksum" scheme,
** though fast and simple, catches the mostly likely kind of corruption.
*/
static u32 pager_cksum(Pager *pPager, const u8 *aData){
  u32 cksum = pPager->cksumInit;         /* Checksum value to return */
  int i = pPager->pageSize-200;          /* Loop counter */
  while( i>0 ){
    cksum += aData[i];
    i -= 200;
  }
  return cksum;
}

/*
** Report the current page size and number of reserved bytes back
** to the codec.
*/
#ifdef SQLITE_HAS_CODEC
static void pagerReportSize(Pager *pPager){
  if( pPager->xCodecSizeChng ){
    pPager->xCodecSizeChng(pPager->pCodec, pPager->pageSize,
                           (int)pPager->nReserve);
  }
}
#else
# define pagerReportSize(X)     /* No-op if we do not support a codec */
#endif

#ifdef SQLITE_HAS_CODEC
/*
** Make sure the number of reserved bits is the same in the destination
** pager as it is in the source.  This comes up when a VACUUM changes the
** number of reserved bits to the "optimal" amount.
*/
SQLITE_PRIVATE void sqlite3PagerAlignReserve(Pager *pDest, Pager *pSrc){
  if( pDest->nReserve!=pSrc->nReserve ){
    pDest->nReserve = pSrc->nReserve;
    pagerReportSize(pDest);
  }
}
#endif

/*
** Read a single page from either the journal file (if isMainJrnl==1) or
** from the sub-journal (if isMainJrnl==0) and playback that page.
** The page begins at offset *pOffset into the file. The *pOffset
** value is increased to the start of the next page in the journal.
**
** The main rollback journal uses checksums - the statement journal does 
** not.
**
** If the page number of the page record read from the (sub-)journal file
** is greater than the current value of Pager.dbSize, then playback is
** skipped and SQLITE_OK is returned.
**
** If pDone is not NULL, then it is a record of pages that have already
** been played back.  If the page at *pOffset has already been played back
** (if the corresponding pDone bit is set) then skip the playback.
** Make sure the pDone bit corresponding to the *pOffset page is set
** prior to returning.
**
** If the page record is successfully read from the (sub-)journal file
** and played back, then SQLITE_OK is returned. If an IO error occurs
** while reading the record from the (sub-)journal file or while writing
** to the database file, then the IO error code is returned. If data
** is successfully read from the (sub-)journal file but appears to be
** corrupted, SQLITE_DONE is returned. Data is considered corrupted in
** two circumstances:
** 
**   * If the record page-number is illegal (0 or PAGER_MJ_PGNO), or
**   * If the record is being rolled back from the main journal file
**     and the checksum field does not match the record content.
**
** Neither of these two scenarios are possible during a savepoint rollback.
**
** If this is a savepoint rollback, then memory may have to be dynamically
** allocated by this function. If this is the case and an allocation fails,
** SQLITE_NOMEM is returned.
*/
static int pager_playback_one_page(
  Pager *pPager,                /* The pager being played back */
  i64 *pOffset,                 /* Offset of record to playback */
  Bitvec *pDone,                /* Bitvec of pages already played back */
  int isMainJrnl,               /* 1 -> main journal. 0 -> sub-journal. */
  int isSavepnt                 /* True for a savepoint rollback */
){
  int rc;
  PgHdr *pPg;                   /* An existing page in the cache */
  Pgno pgno;                    /* The page number of a page in journal */
  u32 cksum;                    /* Checksum used for sanity checking */
  char *aData;                  /* Temporary storage for the page */
  sqlite3_file *jfd;            /* The file descriptor for the journal file */
  int isSynced;                 /* True if journal page is synced */
#ifdef SQLITE_HAS_CODEC
  /* The jrnlEnc flag is true if Journal pages should be passed through
  ** the codec.  It is false for pure in-memory journals. */
  const int jrnlEnc = (isMainJrnl || pPager->subjInMemory==0);
#endif

  assert( (isMainJrnl&~1)==0 );      /* isMainJrnl is 0 or 1 */
  assert( (isSavepnt&~1)==0 );       /* isSavepnt is 0 or 1 */
  assert( isMainJrnl || pDone );     /* pDone always used on sub-journals */
  assert( isSavepnt || pDone==0 );   /* pDone never used on non-savepoint */

  aData = pPager->pTmpSpace;
  assert( aData );         /* Temp storage must have already been allocated */
  assert( pagerUseWal(pPager)==0 || (!isMainJrnl && isSavepnt) );

  /* Either the state is greater than PAGER_WRITER_CACHEMOD (a transaction 
  ** or savepoint rollback done at the request of the caller) or this is
  ** a hot-journal rollback. If it is a hot-journal rollback, the pager
  ** is in state OPEN and holds an EXCLUSIVE lock. Hot-journal rollback
  ** only reads from the main journal, not the sub-journal.
  */
  assert( pPager->eState>=PAGER_WRITER_CACHEMOD
       || (pPager->eState==PAGER_OPEN && pPager->eLock==EXCLUSIVE_LOCK)
  );
  assert( pPager->eState>=PAGER_WRITER_CACHEMOD || isMainJrnl );

  /* Read the page number and page data from the journal or sub-journal
  ** file. Return an error code to the caller if an IO error occurs.
  */
  jfd = isMainJrnl ? pPager->jfd : pPager->sjfd;
  rc = read32bits(jfd, *pOffset, &pgno);
  if( rc!=SQLITE_OK ) return rc;
  rc = sqlite3OsRead(jfd, (u8*)aData, pPager->pageSize, (*pOffset)+4);
  if( rc!=SQLITE_OK ) return rc;
  *pOffset += pPager->pageSize + 4 + isMainJrnl*4;

  /* Sanity checking on the page.  This is more important that I originally
  ** thought.  If a power failure occurs while the journal is being written,
  ** it could cause invalid data to be written into the journal.  We need to
  ** detect this invalid data (with high probability) and ignore it.
  */
  if( pgno==0 || pgno==PAGER_MJ_PGNO(pPager) ){
    assert( !isSavepnt );
    return SQLITE_DONE;
  }
  if( pgno>(Pgno)pPager->dbSize || sqlite3BitvecTest(pDone, pgno) ){
    return SQLITE_OK;
  }
  if( isMainJrnl ){
    rc = read32bits(jfd, (*pOffset)-4, &cksum);
    if( rc ) return rc;
    if( !isSavepnt && pager_cksum(pPager, (u8*)aData)!=cksum ){
      return SQLITE_DONE;
    }
  }

  /* If this page has already been played back before during the current
  ** rollback, then don't bother to play it back again.
  */
  if( pDone && (rc = sqlite3BitvecSet(pDone, pgno))!=SQLITE_OK ){
    return rc;
  }

  /* When playing back page 1, restore the nReserve setting
  */
  if( pgno==1 && pPager->nReserve!=((u8*)aData)[20] ){
    pPager->nReserve = ((u8*)aData)[20];
    pagerReportSize(pPager);
  }

  /* If the pager is in CACHEMOD state, then there must be a copy of this
  ** page in the pager cache. In this case just update the pager cache,
  ** not the database file. The page is left marked dirty in this case.
  **
  ** An exception to the above rule: If the database is in no-sync mode
  ** and a page is moved during an incremental vacuum then the page may
  ** not be in the pager cache. Later: if a malloc() or IO error occurs
  ** during a Movepage() call, then the page may not be in the cache
  ** either. So the condition described in the above paragraph is not
  ** assert()able.
  **
  ** If in WRITER_DBMOD, WRITER_FINISHED or OPEN state, then we update the
  ** pager cache if it exists and the main file. The page is then marked 
  ** not dirty. Since this code is only executed in PAGER_OPEN state for
  ** a hot-journal rollback, it is guaranteed that the page-cache is empty
  ** if the pager is in OPEN state.
  **
  ** Ticket #1171:  The statement journal might contain page content that is
  ** different from the page content at the start of the transaction.
  ** This occurs when a page is changed prior to the start of a statement
  ** then changed again within the statement.  When rolling back such a
  ** statement we must not write to the original database unless we know
  ** for certain that original page contents are synced into the main rollback
  ** journal.  Otherwise, a power loss might leave modified data in the
  ** database file without an entry in the rollback journal that can
  ** restore the database to its original form.  Two conditions must be
  ** met before writing to the database files. (1) the database must be
  ** locked.  (2) we know that the original page content is fully synced
  ** in the main journal either because the page is not in cache or else
  ** the page is marked as needSync==0.
  **
  ** 2008-04-14:  When attempting to vacuum a corrupt database file, it
  ** is possible to fail a statement on a database that does not yet exist.
  ** Do not attempt to write if database file has never been opened.
  */
  if( pagerUseWal(pPager) ){
    pPg = 0;
  }else{
    pPg = sqlite3PagerLookup(pPager, pgno);
  }
  assert( pPg || !MEMDB );
  assert( pPager->eState!=PAGER_OPEN || pPg==0 || pPager->tempFile );
  PAGERTRACE(("PLAYBACK %d page %d hash(%08x) %s\n",
           PAGERID(pPager), pgno, pager_datahash(pPager->pageSize, (u8*)aData),
           (isMainJrnl?"main-journal":"sub-journal")
  ));
  if( isMainJrnl ){
    isSynced = pPager->noSync || (*pOffset <= pPager->journalHdr);
  }else{
    isSynced = (pPg==0 || 0==(pPg->flags & PGHDR_NEED_SYNC));
  }
  if( isOpen(pPager->fd)
   && (pPager->eState>=PAGER_WRITER_DBMOD || pPager->eState==PAGER_OPEN)
   && isSynced
  ){
    i64 ofst = (pgno-1)*(i64)pPager->pageSize;
    testcase( !isSavepnt && pPg!=0 && (pPg->flags&PGHDR_NEED_SYNC)!=0 );
    assert( !pagerUseWal(pPager) );

    /* Write the data read from the journal back into the database file.
    ** This is usually safe even for an encrypted database - as the data
    ** was encrypted before it was written to the journal file. The exception
    ** is if the data was just read from an in-memory sub-journal. In that
    ** case it must be encrypted here before it is copied into the database
    ** file.  */
#ifdef SQLITE_HAS_CODEC
    if( !jrnlEnc ){
      CODEC2(pPager, aData, pgno, 7, rc=SQLITE_NOMEM_BKPT, aData);
      rc = sqlite3OsWrite(pPager->fd, (u8 *)aData, pPager->pageSize, ofst);
      CODEC1(pPager, aData, pgno, 3, rc=SQLITE_NOMEM_BKPT);
    }else
#endif
    rc = sqlite3OsWrite(pPager->fd, (u8 *)aData, pPager->pageSize, ofst);

    if( pgno>pPager->dbFileSize ){
      pPager->dbFileSize = pgno;
    }
    if( pPager->pBackup ){
#ifdef SQLITE_HAS_CODEC
      if( jrnlEnc ){
        CODEC1(pPager, aData, pgno, 3, rc=SQLITE_NOMEM_BKPT);
        sqlite3BackupUpdate(pPager->pBackup, pgno, (u8*)aData);
        CODEC2(pPager, aData, pgno, 7, rc=SQLITE_NOMEM_BKPT,aData);
      }else
#endif
      sqlite3BackupUpdate(pPager->pBackup, pgno, (u8*)aData);
    }
  }else if( !isMainJrnl && pPg==0 ){
    /* If this is a rollback of a savepoint and data was not written to
    ** the database and the page is not in-memory, there is a potential
    ** problem. When the page is next fetched by the b-tree layer, it 
    ** will be read from the database file, which may or may not be 
    ** current. 
    **
    ** There are a couple of different ways this can happen. All are quite
    ** obscure. When running in synchronous mode, this can only happen 
    ** if the page is on the free-list at the start of the transaction, then
    ** populated, then moved using sqlite3PagerMovepage().
    **
    ** The solution is to add an in-memory page to the cache containing
    ** the data just read from the sub-journal. Mark the page as dirty 
    ** and if the pager requires a journal-sync, then mark the page as 
    ** requiring a journal-sync before it is written.
    */
    assert( isSavepnt );
    assert( (pPager->doNotSpill & SPILLFLAG_ROLLBACK)==0 );
    pPager->doNotSpill |= SPILLFLAG_ROLLBACK;
    rc = sqlite3PagerGet(pPager, pgno, &pPg, 1);
    assert( (pPager->doNotSpill & SPILLFLAG_ROLLBACK)!=0 );
    pPager->doNotSpill &= ~SPILLFLAG_ROLLBACK;
    if( rc!=SQLITE_OK ) return rc;
    sqlite3PcacheMakeDirty(pPg);
  }
  if( pPg ){
    /* No page should ever be explicitly rolled back that is in use, except
    ** for page 1 which is held in use in order to keep the lock on the
    ** database active. However such a page may be rolled back as a result
    ** of an internal error resulting in an automatic call to
    ** sqlite3PagerRollback().
    */
    void *pData;
    pData = pPg->pData;
    memcpy(pData, (u8*)aData, pPager->pageSize);
    pPager->xReiniter(pPg);
    /* It used to be that sqlite3PcacheMakeClean(pPg) was called here.  But
    ** that call was dangerous and had no detectable benefit since the cache
    ** is normally cleaned by sqlite3PcacheCleanAll() after rollback and so
    ** has been removed. */
    pager_set_pagehash(pPg);

    /* If this was page 1, then restore the value of Pager.dbFileVers.
    ** Do this before any decoding. */
    if( pgno==1 ){
      memcpy(&pPager->dbFileVers, &((u8*)pData)[24],sizeof(pPager->dbFileVers));
    }

    /* Decode the page just read from disk */
#if SQLITE_HAS_CODEC
    if( jrnlEnc ){ CODEC1(pPager, pData, pPg->pgno, 3, rc=SQLITE_NOMEM_BKPT); }
#endif
    sqlite3PcacheRelease(pPg);
  }
  return rc;
}

/*
** Parameter zMaster is the name of a master journal file. A single journal
** file that referred to the master journal file has just been rolled back.
** This routine checks if it is possible to delete the master journal file,
** and does so if it is.
**
** Argument zMaster may point to Pager.pTmpSpace. So that buffer is not 
** available for use within this function.
**
** When a master journal file is created, it is populated with the names 
** of all of its child journals, one after another, formatted as utf-8 
** encoded text. The end of each child journal file is marked with a 
** nul-terminator byte (0x00). i.e. the entire contents of a master journal
** file for a transaction involving two databases might be:
**
**   "/home/bill/a.db-journal\x00/home/bill/b.db-journal\x00"
**
** A master journal file may only be deleted once all of its child 
** journals have been rolled back.
**
** This function reads the contents of the master-journal file into 
** memory and loops through each of the child journal names. For
** each child journal, it checks if:
**
**   * if the child journal exists, and if so
**   * if the child journal contains a reference to master journal 
**     file zMaster
**
** If a child journal can be found that matches both of the criteria
** above, this function returns without doing anything. Otherwise, if
** no such child journal can be found, file zMaster is deleted from
** the file-system using sqlite3OsDelete().
**
** If an IO error within this function, an error code is returned. This
** function allocates memory by calling sqlite3Malloc(). If an allocation
** fails, SQLITE_NOMEM is returned. Otherwise, if no IO or malloc errors 
** occur, SQLITE_OK is returned.
**
** TODO: This function allocates a single block of memory to load
** the entire contents of the master journal file. This could be
** a couple of kilobytes or so - potentially larger than the page 
** size.
*/
static int pager_delmaster(Pager *pPager, const char *zMaster){
  sqlite3_vfs *pVfs = pPager->pVfs;
  int rc;                   /* Return code */
  sqlite3_file *pMaster;    /* Malloc'd master-journal file descriptor */
  sqlite3_file *pJournal;   /* Malloc'd child-journal file descriptor */
  char *zMasterJournal = 0; /* Contents of master journal file */
  i64 nMasterJournal;       /* Size of master journal file */
  char *zJournal;           /* Pointer to one journal within MJ file */
  char *zMasterPtr;         /* Space to hold MJ filename from a journal file */
  int nMasterPtr;           /* Amount of space allocated to zMasterPtr[] */

  /* Allocate space for both the pJournal and pMaster file descriptors.
  ** If successful, open the master journal file for reading.
  */
  pMaster = (sqlite3_file *)sqlite3MallocZero(pVfs->szOsFile * 2);
  pJournal = (sqlite3_file *)(((u8 *)pMaster) + pVfs->szOsFile);
  if( !pMaster ){
    rc = SQLITE_NOMEM_BKPT;
  }else{
    const int flags = (SQLITE_OPEN_READONLY|SQLITE_OPEN_MASTER_JOURNAL);
    rc = sqlite3OsOpen(pVfs, zMaster, pMaster, flags, 0);
  }
  if( rc!=SQLITE_OK ) goto delmaster_out;

  /* Load the entire master journal file into space obtained from
  ** sqlite3_malloc() and pointed to by zMasterJournal.   Also obtain
  ** sufficient space (in zMasterPtr) to hold the names of master
  ** journal files extracted from regular rollback-journals.
  */
  rc = sqlite3OsFileSize(pMaster, &nMasterJournal);
  if( rc!=SQLITE_OK ) goto delmaster_out;
  nMasterPtr = pVfs->mxPathname+1;
  zMasterJournal = sqlite3Malloc(nMasterJournal + nMasterPtr + 1);
  if( !zMasterJournal ){
    rc = SQLITE_NOMEM_BKPT;
    goto delmaster_out;
  }
  zMasterPtr = &zMasterJournal[nMasterJournal+1];
  rc = sqlite3OsRead(pMaster, zMasterJournal, (int)nMasterJournal, 0);
  if( rc!=SQLITE_OK ) goto delmaster_out;
  zMasterJournal[nMasterJournal] = 0;

  zJournal = zMasterJournal;
  while( (zJournal-zMasterJournal)<nMasterJournal ){
    int exists;
    rc = sqlite3OsAccess(pVfs, zJournal, SQLITE_ACCESS_EXISTS, &exists);
    if( rc!=SQLITE_OK ){
      goto delmaster_out;
    }
    if( exists ){
      /* One of the journals pointed to by the master journal exists.
      ** Open it and check if it points at the master journal. If
      ** so, return without deleting the master journal file.
      */
      int c;
      int flags = (SQLITE_OPEN_READONLY|SQLITE_OPEN_MAIN_JOURNAL);
      rc = sqlite3OsOpen(pVfs, zJournal, pJournal, flags, 0);
      if( rc!=SQLITE_OK ){
        goto delmaster_out;
      }

      rc = readMasterJournal(pJournal, zMasterPtr, nMasterPtr);
      sqlite3OsClose(pJournal);
      if( rc!=SQLITE_OK ){
        goto delmaster_out;
      }

      c = zMasterPtr[0]!=0 && strcmp(zMasterPtr, zMaster)==0;
      if( c ){
        /* We have a match. Do not delete the master journal file. */
        goto delmaster_out;
      }
    }
    zJournal += (sqlite3Strlen30(zJournal)+1);
  }
 
  sqlite3OsClose(pMaster);
  rc = sqlite3OsDelete(pVfs, zMaster, 0);

delmaster_out:
  sqlite3_free(zMasterJournal);
  if( pMaster ){
    sqlite3OsClose(pMaster);
    assert( !isOpen(pJournal) );
    sqlite3_free(pMaster);
  }
  return rc;
}


/*
** This function is used to change the actual size of the database 
** file in the file-system. This only happens when committing a transaction,
** or rolling back a transaction (including rolling back a hot-journal).
**
** If the main database file is not open, or the pager is not in either
** DBMOD or OPEN state, this function is a no-op. Otherwise, the size 
** of the file is changed to nPage pages (nPage*pPager->pageSize bytes). 
** If the file on disk is currently larger than nPage pages, then use the VFS
** xTruncate() method to truncate it.
**
** Or, it might be the case that the file on disk is smaller than 
** nPage pages. Some operating system implementations can get confused if 
** you try to truncate a file to some size that is larger than it 
** currently is, so detect this case and write a single zero byte to 
** the end of the new file instead.
**
** If successful, return SQLITE_OK. If an IO error occurs while modifying
** the database file, return the error code to the caller.
*/
static int pager_truncate(Pager *pPager, Pgno nPage){
  int rc = SQLITE_OK;
  assert( pPager->eState!=PAGER_ERROR );
  assert( pPager->eState!=PAGER_READER );
  
  if( isOpen(pPager->fd) 
   && (pPager->eState>=PAGER_WRITER_DBMOD || pPager->eState==PAGER_OPEN) 
  ){
    i64 currentSize, newSize;
    int szPage = pPager->pageSize;
    assert( pPager->eLock==EXCLUSIVE_LOCK );
    /* TODO: Is it safe to use Pager.dbFileSize here? */
    rc = sqlite3OsFileSize(pPager->fd, &currentSize);
    newSize = szPage*(i64)nPage;
    if( rc==SQLITE_OK && currentSize!=newSize ){
      if( currentSize>newSize ){
        rc = sqlite3OsTruncate(pPager->fd, newSize);
      }else if( (currentSize+szPage)<=newSize ){
        char *pTmp = pPager->pTmpSpace;
        memset(pTmp, 0, szPage);
        testcase( (newSize-szPage) == currentSize );
        testcase( (newSize-szPage) >  currentSize );
        rc = sqlite3OsWrite(pPager->fd, pTmp, szPage, newSize-szPage);
      }
      if( rc==SQLITE_OK ){
        pPager->dbFileSize = nPage;
      }
    }
  }
  return rc;
}

/*
** Return a sanitized version of the sector-size of OS file pFile. The
** return value is guaranteed to lie between 32 and MAX_SECTOR_SIZE.
*/
SQLITE_PRIVATE int sqlite3SectorSize(sqlite3_file *pFile){
  int iRet = sqlite3OsSectorSize(pFile);
  if( iRet<32 ){
    iRet = 512;
  }else if( iRet>MAX_SECTOR_SIZE ){
    assert( MAX_SECTOR_SIZE>=512 );
    iRet = MAX_SECTOR_SIZE;
  }
  return iRet;
}

/*
** Set the value of the Pager.sectorSize variable for the given
** pager based on the value returned by the xSectorSize method
** of the open database file. The sector size will be used 
** to determine the size and alignment of journal header and 
** master journal pointers within created journal files.
**
** For temporary files the effective sector size is always 512 bytes.
**
** Otherwise, for non-temporary files, the effective sector size is
** the value returned by the xSectorSize() method rounded up to 32 if
** it is less than 32, or rounded down to MAX_SECTOR_SIZE if it
** is greater than MAX_SECTOR_SIZE.
**
** If the file has the SQLITE_IOCAP_POWERSAFE_OVERWRITE property, then set
** the effective sector size to its minimum value (512).  The purpose of
** pPager->sectorSize is to define the "blast radius" of bytes that
** might change if a crash occurs while writing to a single byte in
** that range.  But with POWERSAFE_OVERWRITE, the blast radius is zero
** (that is what POWERSAFE_OVERWRITE means), so we minimize the sector
** size.  For backwards compatibility of the rollback journal file format,
** we cannot reduce the effective sector size below 512.
*/
static void setSectorSize(Pager *pPager){
  assert( isOpen(pPager->fd) || pPager->tempFile );

  if( pPager->tempFile
   || (sqlite3OsDeviceCharacteristics(pPager->fd) & 
              SQLITE_IOCAP_POWERSAFE_OVERWRITE)!=0
  ){
    /* Sector size doesn't matter for temporary files. Also, the file
    ** may not have been opened yet, in which case the OsSectorSize()
    ** call will segfault. */
    pPager->sectorSize = 512;
  }else{
    pPager->sectorSize = sqlite3SectorSize(pPager->fd);
  }
}

/*
** Playback the journal and thus restore the database file to
** the state it was in before we started making changes.  
**
** The journal file format is as follows: 
**
**  (1)  8 byte prefix.  A copy of aJournalMagic[].
**  (2)  4 byte big-endian integer which is the number of valid page records
**       in the journal.  If this value is 0xffffffff, then compute the
**       number of page records from the journal size.
**  (3)  4 byte big-endian integer which is the initial value for the 
**       sanity checksum.
**  (4)  4 byte integer which is the number of pages to truncate the
**       database to during a rollback.
**  (5)  4 byte big-endian integer which is the sector size.  The header
**       is this many bytes in size.
**  (6)  4 byte big-endian integer which is the page size.
**  (7)  zero padding out to the next sector size.
**  (8)  Zero or more pages instances, each as follows:
**        +  4 byte page number.
**        +  pPager->pageSize bytes of data.
**        +  4 byte checksum
**
** When we speak of the journal header, we mean the first 7 items above.
** Each entry in the journal is an instance of the 8th item.
**
** Call the value from the second bullet "nRec".  nRec is the number of
** valid page entries in the journal.  In most cases, you can compute the
** value of nRec from the size of the journal file.  But if a power
** failure occurred while the journal was being written, it could be the
** case that the size of the journal file had already been increased but
** the extra entries had not yet made it safely to disk.  In such a case,
** the value of nRec computed from the file size would be too large.  For
** that reason, we always use the nRec value in the header.
**
** If the nRec value is 0xffffffff it means that nRec should be computed
** from the file size.  This value is used when the user selects the
** no-sync option for the journal.  A power failure could lead to corruption
** in this case.  But for things like temporary table (which will be
** deleted when the power is restored) we don't care.  
**
** If the file opened as the journal file is not a well-formed
** journal file then all pages up to the first corrupted page are rolled
** back (or no pages if the journal header is corrupted). The journal file
** is then deleted and SQLITE_OK returned, just as if no corruption had
** been encountered.
**
** If an I/O or malloc() error occurs, the journal-file is not deleted
** and an error code is returned.
**
** The isHot parameter indicates that we are trying to rollback a journal
** that might be a hot journal.  Or, it could be that the journal is 
** preserved because of JOURNALMODE_PERSIST or JOURNALMODE_TRUNCATE.
** If the journal really is hot, reset the pager cache prior rolling
** back any content.  If the journal is merely persistent, no reset is
** needed.
*/
static int pager_playback(Pager *pPager, int isHot){
  sqlite3_vfs *pVfs = pPager->pVfs;
  i64 szJ;                 /* Size of the journal file in bytes */
  u32 nRec;                /* Number of Records in the journal */
  u32 u;                   /* Unsigned loop counter */
  Pgno mxPg = 0;           /* Size of the original file in pages */
  int rc;                  /* Result code of a subroutine */
  int res = 1;             /* Value returned by sqlite3OsAccess() */
  char *zMaster = 0;       /* Name of master journal file if any */
  int needPagerReset;      /* True to reset page prior to first page rollback */
  int nPlayback = 0;       /* Total number of pages restored from journal */
  u32 savedPageSize = pPager->pageSize;

  /* Figure out how many records are in the journal.  Abort early if
  ** the journal is empty.
  */
  assert( isOpen(pPager->jfd) );
  rc = sqlite3OsFileSize(pPager->jfd, &szJ);
  if( rc!=SQLITE_OK ){
    goto end_playback;
  }

  /* Read the master journal name from the journal, if it is present.
  ** If a master journal file name is specified, but the file is not
  ** present on disk, then the journal is not hot and does not need to be
  ** played back.
  **
  ** TODO: Technically the following is an error because it assumes that
  ** buffer Pager.pTmpSpace is (mxPathname+1) bytes or larger. i.e. that
  ** (pPager->pageSize >= pPager->pVfs->mxPathname+1). Using os_unix.c,
  ** mxPathname is 512, which is the same as the minimum allowable value
  ** for pageSize.
  */
  zMaster = pPager->pTmpSpace;
  rc = readMasterJournal(pPager->jfd, zMaster, pPager->pVfs->mxPathname+1);
  if( rc==SQLITE_OK && zMaster[0] ){
    rc = sqlite3OsAccess(pVfs, zMaster, SQLITE_ACCESS_EXISTS, &res);
  }
  zMaster = 0;
  if( rc!=SQLITE_OK || !res ){
    goto end_playback;
  }
  pPager->journalOff = 0;
  needPagerReset = isHot;

  /* This loop terminates either when a readJournalHdr() or 
  ** pager_playback_one_page() call returns SQLITE_DONE or an IO error 
  ** occurs. 
  */
  while( 1 ){
    /* Read the next journal header from the journal file.  If there are
    ** not enough bytes left in the journal file for a complete header, or
    ** it is corrupted, then a process must have failed while writing it.
    ** This indicates nothing more needs to be rolled back.
    */
    rc = readJournalHdr(pPager, isHot, szJ, &nRec, &mxPg);
    if( rc!=SQLITE_OK ){ 
      if( rc==SQLITE_DONE ){
        rc = SQLITE_OK;
      }
      goto end_playback;
    }

    /* If nRec is 0xffffffff, then this journal was created by a process
    ** working in no-sync mode. This means that the rest of the journal
    ** file consists of pages, there are no more journal headers. Compute
    ** the value of nRec based on this assumption.
    */
    if( nRec==0xffffffff ){
      assert( pPager->journalOff==JOURNAL_HDR_SZ(pPager) );
      nRec = (int)((szJ - JOURNAL_HDR_SZ(pPager))/JOURNAL_PG_SZ(pPager));
    }

    /* If nRec is 0 and this rollback is of a transaction created by this
    ** process and if this is the final header in the journal, then it means
    ** that this part of the journal was being filled but has not yet been
    ** synced to disk.  Compute the number of pages based on the remaining
    ** size of the file.
    **
    ** The third term of the test was added to fix ticket #2565.
    ** When rolling back a hot journal, nRec==0 always means that the next
    ** chunk of the journal contains zero pages to be rolled back.  But
    ** when doing a ROLLBACK and the nRec==0 chunk is the last chunk in
    ** the journal, it means that the journal might contain additional
    ** pages that need to be rolled back and that the number of pages 
    ** should be computed based on the journal file size.
    */
    if( nRec==0 && !isHot &&
        pPager->journalHdr+JOURNAL_HDR_SZ(pPager)==pPager->journalOff ){
      nRec = (int)((szJ - pPager->journalOff) / JOURNAL_PG_SZ(pPager));
    }

    /* If this is the first header read from the journal, truncate the
    ** database file back to its original size.
    */
    if( pPager->journalOff==JOURNAL_HDR_SZ(pPager) ){
      rc = pager_truncate(pPager, mxPg);
      if( rc!=SQLITE_OK ){
        goto end_playback;
      }
      pPager->dbSize = mxPg;
    }

    /* Copy original pages out of the journal and back into the 
    ** database file and/or page cache.
    */
    for(u=0; u<nRec; u++){
      if( needPagerReset ){
        pager_reset(pPager);
        needPagerReset = 0;
      }
      rc = pager_playback_one_page(pPager,&pPager->journalOff,0,1,0);
      if( rc==SQLITE_OK ){
        nPlayback++;
      }else{
        if( rc==SQLITE_DONE ){
          pPager->journalOff = szJ;
          break;
        }else if( rc==SQLITE_IOERR_SHORT_READ ){
          /* If the journal has been truncated, simply stop reading and
          ** processing the journal. This might happen if the journal was
          ** not completely written and synced prior to a crash.  In that
          ** case, the database should have never been written in the
          ** first place so it is OK to simply abandon the rollback. */
          rc = SQLITE_OK;
          goto end_playback;
        }else{
          /* If we are unable to rollback, quit and return the error
          ** code.  This will cause the pager to enter the error state
          ** so that no further harm will be done.  Perhaps the next
          ** process to come along will be able to rollback the database.
          */
          goto end_playback;
        }
      }
    }
  }
  /*NOTREACHED*/
  assert( 0 );

end_playback:
  if( rc==SQLITE_OK ){
    rc = sqlite3PagerSetPagesize(pPager, &savedPageSize, -1);
  }
  /* Following a rollback, the database file should be back in its original
  ** state prior to the start of the transaction, so invoke the
  ** SQLITE_FCNTL_DB_UNCHANGED file-control method to disable the
  ** assertion that the transaction counter was modified.
  */
#ifdef SQLITE_DEBUG
  sqlite3OsFileControlHint(pPager->fd,SQLITE_FCNTL_DB_UNCHANGED,0);
#endif

  /* If this playback is happening automatically as a result of an IO or 
  ** malloc error that occurred after the change-counter was updated but 
  ** before the transaction was committed, then the change-counter 
  ** modification may just have been reverted. If this happens in exclusive 
  ** mode, then subsequent transactions performed by the connection will not
  ** update the change-counter at all. This may lead to cache inconsistency
  ** problems for other processes at some point in the future. So, just
  ** in case this has happened, clear the changeCountDone flag now.
  */
  pPager->changeCountDone = pPager->tempFile;

  if( rc==SQLITE_OK ){
    zMaster = pPager->pTmpSpace;
    rc = readMasterJournal(pPager->jfd, zMaster, pPager->pVfs->mxPathname+1);
    testcase( rc!=SQLITE_OK );
  }
  if( rc==SQLITE_OK
   && (pPager->eState>=PAGER_WRITER_DBMOD || pPager->eState==PAGER_OPEN)
  ){
    rc = sqlite3PagerSync(pPager, 0);
  }
  if( rc==SQLITE_OK ){
    rc = pager_end_transaction(pPager, zMaster[0]!='\0', 0);
    testcase( rc!=SQLITE_OK );
  }
  if( rc==SQLITE_OK && zMaster[0] && res ){
    /* If there was a master journal and this routine will return success,
    ** see if it is possible to delete the master journal.
    */
    rc = pager_delmaster(pPager, zMaster);
    testcase( rc!=SQLITE_OK );
  }
  if( isHot && nPlayback ){
    sqlite3_log(SQLITE_NOTICE_RECOVER_ROLLBACK, "recovered %d pages from %s",
                nPlayback, pPager->zJournal);
  }

  /* The Pager.sectorSize variable may have been updated while rolling
  ** back a journal created by a process with a different sector size
  ** value. Reset it to the correct value for this process.
  */
  setSectorSize(pPager);
  return rc;
}


/*
** Read the content for page pPg out of the database file (or out of
** the WAL if that is where the most recent copy if found) into 
** pPg->pData. A shared lock or greater must be held on the database
** file before this function is called.
**
** If page 1 is read, then the value of Pager.dbFileVers[] is set to
** the value read from the database file.
**
** If an IO error occurs, then the IO error is returned to the caller.
** Otherwise, SQLITE_OK is returned.
*/
static int readDbPage(PgHdr *pPg){
  Pager *pPager = pPg->pPager; /* Pager object associated with page pPg */
  int rc = SQLITE_OK;          /* Return code */

#ifndef SQLITE_OMIT_WAL
  u32 iFrame = 0;              /* Frame of WAL containing pgno */

  assert( pPager->eState>=PAGER_READER && !MEMDB );
  assert( isOpen(pPager->fd) );

  if( pagerUseWal(pPager) ){
    rc = sqlite3WalFindFrame(pPager->pWal, pPg->pgno, &iFrame);
    if( rc ) return rc;
  }
  if( iFrame ){
    rc = sqlite3WalReadFrame(pPager->pWal, iFrame,pPager->pageSize,pPg->pData);
  }else
#endif
  {
    i64 iOffset = (pPg->pgno-1)*(i64)pPager->pageSize;
    rc = sqlite3OsRead(pPager->fd, pPg->pData, pPager->pageSize, iOffset);
    if( rc==SQLITE_IOERR_SHORT_READ ){
      rc = SQLITE_OK;
    }
  }

  if( pPg->pgno==1 ){
    if( rc ){
      /* If the read is unsuccessful, set the dbFileVers[] to something
      ** that will never be a valid file version.  dbFileVers[] is a copy
      ** of bytes 24..39 of the database.  Bytes 28..31 should always be
      ** zero or the size of the database in page. Bytes 32..35 and 35..39
      ** should be page numbers which are never 0xffffffff.  So filling
      ** pPager->dbFileVers[] with all 0xff bytes should suffice.
      **
      ** For an encrypted database, the situation is more complex:  bytes
      ** 24..39 of the database are white noise.  But the probability of
      ** white noise equaling 16 bytes of 0xff is vanishingly small so
      ** we should still be ok.
      */
      memset(pPager->dbFileVers, 0xff, sizeof(pPager->dbFileVers));
    }else{
      u8 *dbFileVers = &((u8*)pPg->pData)[24];
      memcpy(&pPager->dbFileVers, dbFileVers, sizeof(pPager->dbFileVers));
    }
  }
  CODEC1(pPager, pPg->pData, pPg->pgno, 3, rc = SQLITE_NOMEM_BKPT);

  PAGER_INCR(sqlite3_pager_readdb_count);
  PAGER_INCR(pPager->nRead);
  IOTRACE(("PGIN %p %d\n", pPager, pPg->pgno));
  PAGERTRACE(("FETCH %d page %d hash(%08x)\n",
               PAGERID(pPager), pPg->pgno, pager_pagehash(pPg)));

  return rc;
}

/*
** Update the value of the change-counter at offsets 24 and 92 in
** the header and the sqlite version number at offset 96.
**
** This is an unconditional update.  See also the pager_incr_changecounter()
** routine which only updates the change-counter if the update is actually
** needed, as determined by the pPager->changeCountDone state variable.
*/
static void pager_write_changecounter(PgHdr *pPg){
  u32 change_counter;

  /* Increment the value just read and write it back to byte 24. */
  change_counter = sqlite3Get4byte((u8*)pPg->pPager->dbFileVers)+1;
  put32bits(((char*)pPg->pData)+24, change_counter);

  /* Also store the SQLite version number in bytes 96..99 and in
  ** bytes 92..95 store the change counter for which the version number
  ** is valid. */
  put32bits(((char*)pPg->pData)+92, change_counter);
  put32bits(((char*)pPg->pData)+96, SQLITE_VERSION_NUMBER);
}

#ifndef SQLITE_OMIT_WAL
/*
** This function is invoked once for each page that has already been 
** written into the log file when a WAL transaction is rolled back.
** Parameter iPg is the page number of said page. The pCtx argument 
** is actually a pointer to the Pager structure.
**
** If page iPg is present in the cache, and has no outstanding references,
** it is discarded. Otherwise, if there are one or more outstanding
** references, the page content is reloaded from the database. If the
** attempt to reload content from the database is required and fails, 
** return an SQLite error code. Otherwise, SQLITE_OK.
*/
static int pagerUndoCallback(void *pCtx, Pgno iPg){
  int rc = SQLITE_OK;
  Pager *pPager = (Pager *)pCtx;
  PgHdr *pPg;

  assert( pagerUseWal(pPager) );
  pPg = sqlite3PagerLookup(pPager, iPg);
  if( pPg ){
    if( sqlite3PcachePageRefcount(pPg)==1 ){
      sqlite3PcacheDrop(pPg);
    }else{
      rc = readDbPage(pPg);
      if( rc==SQLITE_OK ){
        pPager->xReiniter(pPg);
      }
      sqlite3PagerUnrefNotNull(pPg);
    }
  }

  /* Normally, if a transaction is rolled back, any backup processes are
  ** updated as data is copied out of the rollback journal and into the
  ** database. This is not generally possible with a WAL database, as
  ** rollback involves simply truncating the log file. Therefore, if one
  ** or more frames have already been written to the log (and therefore 
  ** also copied into the backup databases) as part of this transaction,
  ** the backups must be restarted.
  */
  sqlite3BackupRestart(pPager->pBackup);

  return rc;
}

/*
** This function is called to rollback a transaction on a WAL database.
*/
static int pagerRollbackWal(Pager *pPager){
  int rc;                         /* Return Code */
  PgHdr *pList;                   /* List of dirty pages to revert */

  /* For all pages in the cache that are currently dirty or have already
  ** been written (but not committed) to the log file, do one of the 
  ** following:
  **
  **   + Discard the cached page (if refcount==0), or
  **   + Reload page content from the database (if refcount>0).
  */
  pPager->dbSize = pPager->dbOrigSize;
  rc = sqlite3WalUndo(pPager->pWal, pagerUndoCallback, (void *)pPager);
  pList = sqlite3PcacheDirtyList(pPager->pPCache);
  while( pList && rc==SQLITE_OK ){
    PgHdr *pNext = pList->pDirty;
    rc = pagerUndoCallback((void *)pPager, pList->pgno);
    pList = pNext;
  }

  return rc;
}

/*
** This function is a wrapper around sqlite3WalFrames(). As well as logging
** the contents of the list of pages headed by pList (connected by pDirty),
** this function notifies any active backup processes that the pages have
** changed. 
**
** The list of pages passed into this routine is always sorted by page number.
** Hence, if page 1 appears anywhere on the list, it will be the first page.
*/ 
static int pagerWalFrames(
  Pager *pPager,                  /* Pager object */
  PgHdr *pList,                   /* List of frames to log */
  Pgno nTruncate,                 /* Database size after this commit */
  int isCommit                    /* True if this is a commit */
){
  int rc;                         /* Return code */
  int nList;                      /* Number of pages in pList */
  PgHdr *p;                       /* For looping over pages */

  assert( pPager->pWal );
  assert( pList );
#ifdef SQLITE_DEBUG
  /* Verify that the page list is in accending order */
  for(p=pList; p && p->pDirty; p=p->pDirty){
    assert( p->pgno < p->pDirty->pgno );
  }
#endif

  assert( pList->pDirty==0 || isCommit );
  if( isCommit ){
    /* If a WAL transaction is being committed, there is no point in writing
    ** any pages with page numbers greater than nTruncate into the WAL file.
    ** They will never be read by any client. So remove them from the pDirty
    ** list here. */
    PgHdr **ppNext = &pList;
    nList = 0;
    for(p=pList; (*ppNext = p)!=0; p=p->pDirty){
      if( p->pgno<=nTruncate ){
        ppNext = &p->pDirty;
        nList++;
      }
    }
    assert( pList );
  }else{
    nList = 1;
  }
  pPager->aStat[PAGER_STAT_WRITE] += nList;

  if( pList->pgno==1 ) pager_write_changecounter(pList);
  rc = sqlite3WalFrames(pPager->pWal, 
      pPager->pageSize, pList, nTruncate, isCommit, pPager->walSyncFlags
  );
  if( rc==SQLITE_OK && pPager->pBackup ){
    for(p=pList; p; p=p->pDirty){
      sqlite3BackupUpdate(pPager->pBackup, p->pgno, (u8 *)p->pData);
    }
  }

#ifdef SQLITE_CHECK_PAGES
  pList = sqlite3PcacheDirtyList(pPager->pPCache);
  for(p=pList; p; p=p->pDirty){
    pager_set_pagehash(p);
  }
#endif

  return rc;
}

/*
** Begin a read transaction on the WAL.
**
** This routine used to be called "pagerOpenSnapshot()" because it essentially
** makes a snapshot of the database at the current point in time and preserves
** that snapshot for use by the reader in spite of concurrently changes by
** other writers or checkpointers.
*/
static int pagerBeginReadTransaction(Pager *pPager){
  int rc;                         /* Return code */
  int changed = 0;                /* True if cache must be reset */

  assert( pagerUseWal(pPager) );
  assert( pPager->eState==PAGER_OPEN || pPager->eState==PAGER_READER );

  /* sqlite3WalEndReadTransaction() was not called for the previous
  ** transaction in locking_mode=EXCLUSIVE.  So call it now.  If we
  ** are in locking_mode=NORMAL and EndRead() was previously called,
  ** the duplicate call is harmless.
  */
  sqlite3WalEndReadTransaction(pPager->pWal);

  rc = sqlite3WalBeginReadTransaction(pPager->pWal, &changed);
  if( rc!=SQLITE_OK || changed ){
    pager_reset(pPager);
    if( USEFETCH(pPager) ) sqlite3OsUnfetch(pPager->fd, 0, 0);
  }

  return rc;
}
#endif

/*
** This function is called as part of the transition from PAGER_OPEN
** to PAGER_READER state to determine the size of the database file
** in pages (assuming the page size currently stored in Pager.pageSize).
**
** If no error occurs, SQLITE_OK is returned and the size of the database
** in pages is stored in *pnPage. Otherwise, an error code (perhaps
** SQLITE_IOERR_FSTAT) is returned and *pnPage is left unmodified.
*/
static int pagerPagecount(Pager *pPager, Pgno *pnPage){
  Pgno nPage;                     /* Value to return via *pnPage */

  /* Query the WAL sub-system for the database size. The WalDbsize()
  ** function returns zero if the WAL is not open (i.e. Pager.pWal==0), or
  ** if the database size is not available. The database size is not
  ** available from the WAL sub-system if the log file is empty or
  ** contains no valid committed transactions.
  */
  assert( pPager->eState==PAGER_OPEN );
  assert( pPager->eLock>=SHARED_LOCK );
  assert( isOpen(pPager->fd) );
  assert( pPager->tempFile==0 );
  nPage = sqlite3WalDbsize(pPager->pWal);

  /* If the number of pages in the database is not available from the
  ** WAL sub-system, determine the page count based on the size of
  ** the database file.  If the size of the database file is not an
  ** integer multiple of the page-size, round up the result.
  */
  if( nPage==0 && ALWAYS(isOpen(pPager->fd)) ){
    i64 n = 0;                    /* Size of db file in bytes */
    int rc = sqlite3OsFileSize(pPager->fd, &n);
    if( rc!=SQLITE_OK ){
      return rc;
    }
    nPage = (Pgno)((n+pPager->pageSize-1) / pPager->pageSize);
  }

  /* If the current number of pages in the file is greater than the
  ** configured maximum pager number, increase the allowed limit so
  ** that the file can be read.
  */
  if( nPage>pPager->mxPgno ){
    pPager->mxPgno = (Pgno)nPage;
  }

  *pnPage = nPage;
  return SQLITE_OK;
}

#ifndef SQLITE_OMIT_WAL
/*
** Check if the *-wal file that corresponds to the database opened by pPager
** exists if the database is not empy, or verify that the *-wal file does
** not exist (by deleting it) if the database file is empty.
**
** If the database is not empty and the *-wal file exists, open the pager
** in WAL mode.  If the database is empty or if no *-wal file exists and
** if no error occurs, make sure Pager.journalMode is not set to
** PAGER_JOURNALMODE_WAL.
**
** Return SQLITE_OK or an error code.
**
** The caller must hold a SHARED lock on the database file to call this
** function. Because an EXCLUSIVE lock on the db file is required to delete 
** a WAL on a none-empty database, this ensures there is no race condition 
** between the xAccess() below and an xDelete() being executed by some 
** other connection.
*/
static int pagerOpenWalIfPresent(Pager *pPager){
  int rc = SQLITE_OK;
  assert( pPager->eState==PAGER_OPEN );
  assert( pPager->eLock>=SHARED_LOCK );

  if( !pPager->tempFile ){
    int isWal;                    /* True if WAL file exists */
    rc = sqlite3OsAccess(
        pPager->pVfs, pPager->zWal, SQLITE_ACCESS_EXISTS, &isWal
    );
    if( rc==SQLITE_OK ){
      if( isWal ){
        Pgno nPage;                   /* Size of the database file */

        rc = pagerPagecount(pPager, &nPage);
        if( rc ) return rc;
        if( nPage==0 ){
          rc = sqlite3OsDelete(pPager->pVfs, pPager->zWal, 0);
        }else{
          testcase( sqlite3PcachePagecount(pPager->pPCache)==0 );
          rc = sqlite3PagerOpenWal(pPager, 0);
        }
      }else if( pPager->journalMode==PAGER_JOURNALMODE_WAL ){
        pPager->journalMode = PAGER_JOURNALMODE_DELETE;
      }
    }
  }
  return rc;
}
#endif

/*
** Playback savepoint pSavepoint. Or, if pSavepoint==NULL, then playback
** the entire master journal file. The case pSavepoint==NULL occurs when 
** a ROLLBACK TO command is invoked on a SAVEPOINT that is a transaction 
** savepoint.
**
** When pSavepoint is not NULL (meaning a non-transaction savepoint is 
** being rolled back), then the rollback consists of up to three stages,
** performed in the order specified:
**
**   * Pages are played back from the main journal starting at byte
**     offset PagerSavepoint.iOffset and continuing to 
**     PagerSavepoint.iHdrOffset, or to the end of the main journal
**     file if PagerSavepoint.iHdrOffset is zero.
**
**   * If PagerSavepoint.iHdrOffset is not zero, then pages are played
**     back starting from the journal header immediately following 
**     PagerSavepoint.iHdrOffset to the end of the main journal file.
**
**   * Pages are then played back from the sub-journal file, starting
**     with the PagerSavepoint.iSubRec and continuing to the end of
**     the journal file.
**
** Throughout the rollback process, each time a page is rolled back, the
** corresponding bit is set in a bitvec structure (variable pDone in the
** implementation below). This is used to ensure that a page is only
** rolled back the first time it is encountered in either journal.
**
** If pSavepoint is NULL, then pages are only played back from the main
** journal file. There is no need for a bitvec in this case.
**
** In either case, before playback commences the Pager.dbSize variable
** is reset to the value that it held at the start of the savepoint 
** (or transaction). No page with a page-number greater than this value
** is played back. If one is encountered it is simply skipped.
*/
static int pagerPlaybackSavepoint(Pager *pPager, PagerSavepoint *pSavepoint){
  i64 szJ;                 /* Effective size of the main journal */
  i64 iHdrOff;             /* End of first segment of main-journal records */
  int rc = SQLITE_OK;      /* Return code */
  Bitvec *pDone = 0;       /* Bitvec to ensure pages played back only once */

  assert( pPager->eState!=PAGER_ERROR );
  assert( pPager->eState>=PAGER_WRITER_LOCKED );

  /* Allocate a bitvec to use to store the set of pages rolled back */
  if( pSavepoint ){
    pDone = sqlite3BitvecCreate(pSavepoint->nOrig);
    if( !pDone ){
      return SQLITE_NOMEM_BKPT;
    }
  }

  /* Set the database size back to the value it was before the savepoint 
  ** being reverted was opened.
  */
  pPager->dbSize = pSavepoint ? pSavepoint->nOrig : pPager->dbOrigSize;
  pPager->changeCountDone = pPager->tempFile;

  if( !pSavepoint && pagerUseWal(pPager) ){
    return pagerRollbackWal(pPager);
  }

  /* Use pPager->journalOff as the effective size of the main rollback
  ** journal.  The actual file might be larger than this in
  ** PAGER_JOURNALMODE_TRUNCATE or PAGER_JOURNALMODE_PERSIST.  But anything
  ** past pPager->journalOff is off-limits to us.
  */
  szJ = pPager->journalOff;
  assert( pagerUseWal(pPager)==0 || szJ==0 );

  /* Begin by rolling back records from the main journal starting at
  ** PagerSavepoint.iOffset and continuing to the next journal header.
  ** There might be records in the main journal that have a page number
  ** greater than the current database size (pPager->dbSize) but those
  ** will be skipped automatically.  Pages are added to pDone as they
  ** are played back.
  */
  if( pSavepoint && !pagerUseWal(pPager) ){
    iHdrOff = pSavepoint->iHdrOffset ? pSavepoint->iHdrOffset : szJ;
    pPager->journalOff = pSavepoint->iOffset;
    while( rc==SQLITE_OK && pPager->journalOff<iHdrOff ){
      rc = pager_playback_one_page(pPager, &pPager->journalOff, pDone, 1, 1);
    }
    assert( rc!=SQLITE_DONE );
  }else{
    pPager->journalOff = 0;
  }

  /* Continue rolling back records out of the main journal starting at
  ** the first journal header seen and continuing until the effective end
  ** of the main journal file.  Continue to skip out-of-range pages and
  ** continue adding pages rolled back to pDone.
  */
  while( rc==SQLITE_OK && pPager->journalOff<szJ ){
    u32 ii;            /* Loop counter */
    u32 nJRec = 0;     /* Number of Journal Records */
    u32 dummy;
    rc = readJournalHdr(pPager, 0, szJ, &nJRec, &dummy);
    assert( rc!=SQLITE_DONE );

    /*
    ** The "pPager->journalHdr+JOURNAL_HDR_SZ(pPager)==pPager->journalOff"
    ** test is related to ticket #2565.  See the discussion in the
    ** pager_playback() function for additional information.
    */
    if( nJRec==0 
     && pPager->journalHdr+JOURNAL_HDR_SZ(pPager)==pPager->journalOff
    ){
      nJRec = (u32)((szJ - pPager->journalOff)/JOURNAL_PG_SZ(pPager));
    }
    for(ii=0; rc==SQLITE_OK && ii<nJRec && pPager->journalOff<szJ; ii++){
      rc = pager_playback_one_page(pPager, &pPager->journalOff, pDone, 1, 1);
    }
    assert( rc!=SQLITE_DONE );
  }
  assert( rc!=SQLITE_OK || pPager->journalOff>=szJ );

  /* Finally,  rollback pages from the sub-journal.  Page that were
  ** previously rolled back out of the main journal (and are hence in pDone)
  ** will be skipped.  Out-of-range pages are also skipped.
  */
  if( pSavepoint ){
    u32 ii;            /* Loop counter */
    i64 offset = (i64)pSavepoint->iSubRec*(4+pPager->pageSize);

    if( pagerUseWal(pPager) ){
      rc = sqlite3WalSavepointUndo(pPager->pWal, pSavepoint->aWalData);
    }
    for(ii=pSavepoint->iSubRec; rc==SQLITE_OK && ii<pPager->nSubRec; ii++){
      assert( offset==(i64)ii*(4+pPager->pageSize) );
      rc = pager_playback_one_page(pPager, &offset, pDone, 0, 1);
    }
    assert( rc!=SQLITE_DONE );
  }

  sqlite3BitvecDestroy(pDone);
  if( rc==SQLITE_OK ){
    pPager->journalOff = szJ;
  }

  return rc;
}

/*
** Change the maximum number of in-memory pages that are allowed
** before attempting to recycle clean and unused pages.
*/
SQLITE_PRIVATE void sqlite3PagerSetCachesize(Pager *pPager, int mxPage){
  sqlite3PcacheSetCachesize(pPager->pPCache, mxPage);
}

/*
** Change the maximum number of in-memory pages that are allowed
** before attempting to spill pages to journal.
*/
SQLITE_PRIVATE int sqlite3PagerSetSpillsize(Pager *pPager, int mxPage){
  return sqlite3PcacheSetSpillsize(pPager->pPCache, mxPage);
}

/*
** Invoke SQLITE_FCNTL_MMAP_SIZE based on the current value of szMmap.
*/
static void pagerFixMaplimit(Pager *pPager){
#if SQLITE_MAX_MMAP_SIZE>0
  sqlite3_file *fd = pPager->fd;
  if( isOpen(fd) && fd->pMethods->iVersion>=3 ){
    sqlite3_int64 sz;
    sz = pPager->szMmap;
    pPager->bUseFetch = (sz>0);
    setGetterMethod(pPager);
    sqlite3OsFileControlHint(pPager->fd, SQLITE_FCNTL_MMAP_SIZE, &sz);
  }
#endif
}

/*
** Change the maximum size of any memory mapping made of the database file.
*/
SQLITE_PRIVATE void sqlite3PagerSetMmapLimit(Pager *pPager, sqlite3_int64 szMmap){
  pPager->szMmap = szMmap;
  pagerFixMaplimit(pPager);
}

/*
** Free as much memory as possible from the pager.
*/
SQLITE_PRIVATE void sqlite3PagerShrink(Pager *pPager){
  sqlite3PcacheShrink(pPager->pPCache);
}

/*
** Adjust settings of the pager to those specified in the pgFlags parameter.
**
** The "level" in pgFlags & PAGER_SYNCHRONOUS_MASK sets the robustness
** of the database to damage due to OS crashes or power failures by
** changing the number of syncs()s when writing the journals.
** There are four levels:
**
**    OFF       sqlite3OsSync() is never called.  This is the default
**              for temporary and transient files.
**
**    NORMAL    The journal is synced once before writes begin on the
**              database.  This is normally adequate protection, but
**              it is theoretically possible, though very unlikely,
**              that an inopertune power failure could leave the journal
**              in a state which would cause damage to the database
**              when it is rolled back.
**
**    FULL      The journal is synced twice before writes begin on the
**              database (with some additional information - the nRec field
**              of the journal header - being written in between the two
**              syncs).  If we assume that writing a
**              single disk sector is atomic, then this mode provides
**              assurance that the journal will not be corrupted to the
**              point of causing damage to the database during rollback.
**
**    EXTRA     This is like FULL except that is also syncs the directory
**              that contains the rollback journal after the rollback
**              journal is unlinked.
**
** The above is for a rollback-journal mode.  For WAL mode, OFF continues
** to mean that no syncs ever occur.  NORMAL means that the WAL is synced
** prior to the start of checkpoint and that the database file is synced
** at the conclusion of the checkpoint if the entire content of the WAL
** was written back into the database.  But no sync operations occur for
** an ordinary commit in NORMAL mode with WAL.  FULL means that the WAL
** file is synced following each commit operation, in addition to the
** syncs associated with NORMAL.  There is no difference between FULL
** and EXTRA for WAL mode.
**
** Do not confuse synchronous=FULL with SQLITE_SYNC_FULL.  The
** SQLITE_SYNC_FULL macro means to use the MacOSX-style full-fsync
** using fcntl(F_FULLFSYNC).  SQLITE_SYNC_NORMAL means to do an
** ordinary fsync() call.  There is no difference between SQLITE_SYNC_FULL
** and SQLITE_SYNC_NORMAL on platforms other than MacOSX.  But the
** synchronous=FULL versus synchronous=NORMAL setting determines when
** the xSync primitive is called and is relevant to all platforms.
**
** Numeric values associated with these states are OFF==1, NORMAL=2,
** and FULL=3.
*/
#ifndef SQLITE_OMIT_PAGER_PRAGMAS
SQLITE_PRIVATE void sqlite3PagerSetFlags(
  Pager *pPager,        /* The pager to set safety level for */
  unsigned pgFlags      /* Various flags */
){
  unsigned level = pgFlags & PAGER_SYNCHRONOUS_MASK;
  if( pPager->tempFile ){
    pPager->noSync = 1;
    pPager->fullSync = 0;
    pPager->extraSync = 0;
  }else{
    pPager->noSync =  level==PAGER_SYNCHRONOUS_OFF ?1:0;
    pPager->fullSync = level>=PAGER_SYNCHRONOUS_FULL ?1:0;
    pPager->extraSync = level==PAGER_SYNCHRONOUS_EXTRA ?1:0;
  }
  if( pPager->noSync ){
    pPager->syncFlags = 0;
  }else if( pgFlags & PAGER_FULLFSYNC ){
    pPager->syncFlags = SQLITE_SYNC_FULL;
  }else{
    pPager->syncFlags = SQLITE_SYNC_NORMAL;
  }
  pPager->walSyncFlags = (pPager->syncFlags<<2);
  if( pPager->fullSync ){
    pPager->walSyncFlags |= pPager->syncFlags;
  }
  if( (pgFlags & PAGER_CKPT_FULLFSYNC) && !pPager->noSync ){
    pPager->walSyncFlags |= (SQLITE_SYNC_FULL<<2);
  }
  if( pgFlags & PAGER_CACHESPILL ){
    pPager->doNotSpill &= ~SPILLFLAG_OFF;
  }else{
    pPager->doNotSpill |= SPILLFLAG_OFF;
  }
}
#endif

/*
** The following global variable is incremented whenever the library
** attempts to open a temporary file.  This information is used for
** testing and analysis only.  
*/
#ifdef SQLITE_TEST
SQLITE_API int sqlite3_opentemp_count = 0;
#endif

/*
** Open a temporary file.
**
** Write the file descriptor into *pFile. Return SQLITE_OK on success 
** or some other error code if we fail. The OS will automatically 
** delete the temporary file when it is closed.
**
** The flags passed to the VFS layer xOpen() call are those specified
** by parameter vfsFlags ORed with the following:
**
**     SQLITE_OPEN_READWRITE
**     SQLITE_OPEN_CREATE
**     SQLITE_OPEN_EXCLUSIVE
**     SQLITE_OPEN_DELETEONCLOSE
*/
static int pagerOpentemp(
  Pager *pPager,        /* The pager object */
  sqlite3_file *pFile,  /* Write the file descriptor here */
  int vfsFlags          /* Flags passed through to the VFS */
){
  int rc;               /* Return code */

#ifdef SQLITE_TEST
  sqlite3_opentemp_count++;  /* Used for testing and analysis only */
#endif

  vfsFlags |=  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
            SQLITE_OPEN_EXCLUSIVE | SQLITE_OPEN_DELETEONCLOSE;
  rc = sqlite3OsOpen(pPager->pVfs, 0, pFile, vfsFlags, 0);
  assert( rc!=SQLITE_OK || isOpen(pFile) );
  return rc;
}

/*
** Set the busy handler function.
**
** The pager invokes the busy-handler if sqlite3OsLock() returns 
** SQLITE_BUSY when trying to upgrade from no-lock to a SHARED lock,
** or when trying to upgrade from a RESERVED lock to an EXCLUSIVE 
** lock. It does *not* invoke the busy handler when upgrading from
** SHARED to RESERVED, or when upgrading from SHARED to EXCLUSIVE
** (which occurs during hot-journal rollback). Summary:
**
**   Transition                        | Invokes xBusyHandler
**   --------------------------------------------------------
**   NO_LOCK       -> SHARED_LOCK      | Yes
**   SHARED_LOCK   -> RESERVED_LOCK    | No
**   SHARED_LOCK   -> EXCLUSIVE_LOCK   | No
**   RESERVED_LOCK -> EXCLUSIVE_LOCK   | Yes
**
** If the busy-handler callback returns non-zero, the lock is 
** retried. If it returns zero, then the SQLITE_BUSY error is
** returned to the caller of the pager API function.
*/
SQLITE_PRIVATE void sqlite3PagerSetBusyHandler(
  Pager *pPager,                       /* Pager object */
  int (*xBusyHandler)(void *),         /* Pointer to busy-handler function */
  void *pBusyHandlerArg                /* Argument to pass to xBusyHandler */
){
  void **ap;
  pPager->xBusyHandler = xBusyHandler;
  pPager->pBusyHandlerArg = pBusyHandlerArg;
  ap = (void **)&pPager->xBusyHandler;
  assert( ((int(*)(void *))(ap[0]))==xBusyHandler );
  assert( ap[1]==pBusyHandlerArg );
  sqlite3OsFileControlHint(pPager->fd, SQLITE_FCNTL_BUSYHANDLER, (void *)ap);
}

/*
** Change the page size used by the Pager object. The new page size 
** is passed in *pPageSize.
**
** If the pager is in the error state when this function is called, it
** is a no-op. The value returned is the error state error code (i.e. 
** one of SQLITE_IOERR, an SQLITE_IOERR_xxx sub-code or SQLITE_FULL).
**
** Otherwise, if all of the following are true:
**
**   * the new page size (value of *pPageSize) is valid (a power 
**     of two between 512 and SQLITE_MAX_PAGE_SIZE, inclusive), and
**
**   * there are no outstanding page references, and
**
**   * the database is either not an in-memory database or it is
**     an in-memory database that currently consists of zero pages.
**
** then the pager object page size is set to *pPageSize.
**
** If the page size is changed, then this function uses sqlite3PagerMalloc() 
** to obtain a new Pager.pTmpSpace buffer. If this allocation attempt 
** fails, SQLITE_NOMEM is returned and the page size remains unchanged. 
** In all other cases, SQLITE_OK is returned.
**
** If the page size is not changed, either because one of the enumerated
** conditions above is not true, the pager was in error state when this
** function was called, or because the memory allocation attempt failed, 
** then *pPageSize is set to the old, retained page size before returning.
*/
SQLITE_PRIVATE int sqlite3PagerSetPagesize(Pager *pPager, u32 *pPageSize, int nReserve){
  int rc = SQLITE_OK;

  /* It is not possible to do a full assert_pager_state() here, as this
  ** function may be called from within PagerOpen(), before the state
  ** of the Pager object is internally consistent.
  **
  ** At one point this function returned an error if the pager was in 
  ** PAGER_ERROR state. But since PAGER_ERROR state guarantees that
  ** there is at least one outstanding page reference, this function
  ** is a no-op for that case anyhow.
  */

  u32 pageSize = *pPageSize;
  assert( pageSize==0 || (pageSize>=512 && pageSize<=SQLITE_MAX_PAGE_SIZE) );
  if( (pPager->memDb==0 || pPager->dbSize==0)
   && sqlite3PcacheRefCount(pPager->pPCache)==0 
   && pageSize && pageSize!=(u32)pPager->pageSize 
  ){
    char *pNew = NULL;             /* New temp space */
    i64 nByte = 0;

    if( pPager->eState>PAGER_OPEN && isOpen(pPager->fd) ){
      rc = sqlite3OsFileSize(pPager->fd, &nByte);
    }
    if( rc==SQLITE_OK ){
      /* 8 bytes of zeroed overrun space is sufficient so that the b-tree
      * cell header parser will never run off the end of the allocation */
      pNew = (char *)sqlite3PageMalloc(pageSize+8);
      if( !pNew ){
        rc = SQLITE_NOMEM_BKPT;
      }else{
        memset(pNew+pageSize, 0, 8);
      }
    }

    if( rc==SQLITE_OK ){
      pager_reset(pPager);
      rc = sqlite3PcacheSetPageSize(pPager->pPCache, pageSize);
    }
    if( rc==SQLITE_OK ){
      sqlite3PageFree(pPager->pTmpSpace);
      pPager->pTmpSpace = pNew;
      pPager->dbSize = (Pgno)((nByte+pageSize-1)/pageSize);
      pPager->pageSize = pageSize;
    }else{
      sqlite3PageFree(pNew);
    }
  }

  *pPageSize = pPager->pageSize;
  if( rc==SQLITE_OK ){
    if( nReserve<0 ) nReserve = pPager->nReserve;
    assert( nReserve>=0 && nReserve<1000 );
    pPager->nReserve = (i16)nReserve;
    pagerReportSize(pPager);
    pagerFixMaplimit(pPager);
  }
  return rc;
}

/*
** Return a pointer to the "temporary page" buffer held internally
** by the pager.  This is a buffer that is big enough to hold the
** entire content of a database page.  This buffer is used internally
** during rollback and will be overwritten whenever a rollback
** occurs.  But other modules are free to use it too, as long as
** no rollbacks are happening.
*/
SQLITE_PRIVATE void *sqlite3PagerTempSpace(Pager *pPager){
  return pPager->pTmpSpace;
}

/*
** Attempt to set the maximum database page count if mxPage is positive. 
** Make no changes if mxPage is zero or negative.  And never reduce the
** maximum page count below the current size of the database.
**
** Regardless of mxPage, return the current maximum page count.
*/
SQLITE_PRIVATE int sqlite3PagerMaxPageCount(Pager *pPager, int mxPage){
  if( mxPage>0 ){
    pPager->mxPgno = mxPage;
  }
  assert( pPager->eState!=PAGER_OPEN );      /* Called only by OP_MaxPgcnt */
  /* assert( pPager->mxPgno>=pPager->dbSize ); */
  /* OP_MaxPgcnt ensures that the parameter passed to this function is not
  ** less than the total number of valid pages in the database. But this
  ** may be less than Pager.dbSize, and so the assert() above is not valid */
  return pPager->mxPgno;
}

/*
** The following set of routines are used to disable the simulated
** I/O error mechanism.  These routines are used to avoid simulated
** errors in places where we do not care about errors.
**
** Unless -DSQLITE_TEST=1 is used, these routines are all no-ops
** and generate no code.
*/
#ifdef SQLITE_TEST
SQLITE_API extern int sqlite3_io_error_pending;
SQLITE_API extern int sqlite3_io_error_hit;
static int saved_cnt;
void disable_simulated_io_errors(void){
  saved_cnt = sqlite3_io_error_pending;
  sqlite3_io_error_pending = -1;
}
void enable_simulated_io_errors(void){
  sqlite3_io_error_pending = saved_cnt;
}
#else
# define disable_simulated_io_errors()
# define enable_simulated_io_errors()
#endif

/*
** Read the first N bytes from the beginning of the file into memory
** that pDest points to. 
**
** If the pager was opened on a transient file (zFilename==""), or
** opened on a file less than N bytes in size, the output buffer is
** zeroed and SQLITE_OK returned. The rationale for this is that this 
** function is used to read database headers, and a new transient or
** zero sized database has a header than consists entirely of zeroes.
**
** If any IO error apart from SQLITE_IOERR_SHORT_READ is encountered,
** the error code is returned to the caller and the contents of the
** output buffer undefined.
*/
SQLITE_PRIVATE int sqlite3PagerReadFileheader(Pager *pPager, int N, unsigned char *pDest){
  int rc = SQLITE_OK;
  memset(pDest, 0, N);
  assert( isOpen(pPager->fd) || pPager->tempFile );

  /* This routine is only called by btree immediately after creating
  ** the Pager object.  There has not been an opportunity to transition
  ** to WAL mode yet.
  */
  assert( !pagerUseWal(pPager) );

  if( isOpen(pPager->fd) ){
    IOTRACE(("DBHDR %p 0 %d\n", pPager, N))
    rc = sqlite3OsRead(pPager->fd, pDest, N, 0);
    if( rc==SQLITE_IOERR_SHORT_READ ){
      rc = SQLITE_OK;
    }
  }
  return rc;
}

/*
** This function may only be called when a read-transaction is open on
** the pager. It returns the total number of pages in the database.
**
** However, if the file is between 1 and <page-size> bytes in size, then 
** this is considered a 1 page file.
*/
SQLITE_PRIVATE void sqlite3PagerPagecount(Pager *pPager, int *pnPage){
  assert( pPager->eState>=PAGER_READER );
  assert( pPager->eState!=PAGER_WRITER_FINISHED );
  *pnPage = (int)pPager->dbSize;
}


/*
** Try to obtain a lock of type locktype on the database file. If
** a similar or greater lock is already held, this function is a no-op
** (returning SQLITE_OK immediately).
**
** Otherwise, attempt to obtain the lock using sqlite3OsLock(). Invoke 
** the busy callback if the lock is currently not available. Repeat 
** until the busy callback returns false or until the attempt to 
** obtain the lock succeeds.
**
** Return SQLITE_OK on success and an error code if we cannot obtain
** the lock. If the lock is obtained successfully, set the Pager.state 
** variable to locktype before returning.
*/
static int pager_wait_on_lock(Pager *pPager, int locktype){
  int rc;                              /* Return code */

  /* Check that this is either a no-op (because the requested lock is 
  ** already held), or one of the transitions that the busy-handler
  ** may be invoked during, according to the comment above
  ** sqlite3PagerSetBusyhandler().
  */
  assert( (pPager->eLock>=locktype)
       || (pPager->eLock==NO_LOCK && locktype==SHARED_LOCK)
       || (pPager->eLock==RESERVED_LOCK && locktype==EXCLUSIVE_LOCK)
  );

  do {
    rc = pagerLockDb(pPager, locktype);
  }while( rc==SQLITE_BUSY && pPager->xBusyHandler(pPager->pBusyHandlerArg) );
  return rc;
}

/*
** Function assertTruncateConstraint(pPager) checks that one of the 
** following is true for all dirty pages currently in the page-cache:
**
**   a) The page number is less than or equal to the size of the 
**      current database image, in pages, OR
**
**   b) if the page content were written at this time, it would not
**      be necessary to write the current content out to the sub-journal
**      (as determined by function subjRequiresPage()).
**
** If the condition asserted by this function were not true, and the
** dirty page were to be discarded from the cache via the pagerStress()
** routine, pagerStress() would not write the current page content to
** the database file. If a savepoint transaction were rolled back after
** this happened, the correct behavior would be to restore the current
** content of the page. However, since this content is not present in either
** the database file or the portion of the rollback journal and 
** sub-journal rolled back the content could not be restored and the
** database image would become corrupt. It is therefore fortunate that 
** this circumstance cannot arise.
*/
#if defined(SQLITE_DEBUG)
static void assertTruncateConstraintCb(PgHdr *pPg){
  assert( pPg->flags&PGHDR_DIRTY );
  assert( !subjRequiresPage(pPg) || pPg->pgno<=pPg->pPager->dbSize );
}
static void assertTruncateConstraint(Pager *pPager){
  sqlite3PcacheIterateDirty(pPager->pPCache, assertTruncateConstraintCb);
}
#else
# define assertTruncateConstraint(pPager)
#endif

/*
** Truncate the in-memory database file image to nPage pages. This 
** function does not actually modify the database file on disk. It 
** just sets the internal state of the pager object so that the 
** truncation will be done when the current transaction is committed.
**
** This function is only called right before committing a transaction.
** Once this function has been called, the transaction must either be
** rolled back or committed. It is not safe to call this function and
** then continue writing to the database.
*/
SQLITE_PRIVATE void sqlite3PagerTruncateImage(Pager *pPager, Pgno nPage){
  assert( pPager->dbSize>=nPage );
  assert( pPager->eState>=PAGER_WRITER_CACHEMOD );
  pPager->dbSize = nPage;

  /* At one point the code here called assertTruncateConstraint() to
  ** ensure that all pages being truncated away by this operation are,
  ** if one or more savepoints are open, present in the savepoint 
  ** journal so that they can be restored if the savepoint is rolled
  ** back. This is no longer necessary as this function is now only
  ** called right before committing a transaction. So although the 
  ** Pager object may still have open savepoints (Pager.nSavepoint!=0), 
  ** they cannot be rolled back. So the assertTruncateConstraint() call
  ** is no longer correct. */
}


/*
** This function is called before attempting a hot-journal rollback. It
** syncs the journal file to disk, then sets pPager->journalHdr to the
** size of the journal file so that the pager_playback() routine knows
** that the entire journal file has been synced.
**
** Syncing a hot-journal to disk before attempting to roll it back ensures 
** that if a power-failure occurs during the rollback, the process that
** attempts rollback following system recovery sees the same journal
** content as this process.
**
** If everything goes as planned, SQLITE_OK is returned. Otherwise, 
** an SQLite error code.
*/
static int pagerSyncHotJournal(Pager *pPager){
  int rc = SQLITE_OK;
  if( !pPager->noSync ){
    rc = sqlite3OsSync(pPager->jfd, SQLITE_SYNC_NORMAL);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3OsFileSize(pPager->jfd, &pPager->journalHdr);
  }
  return rc;
}

#if SQLITE_MAX_MMAP_SIZE>0
/*
** Obtain a reference to a memory mapped page object for page number pgno. 
** The new object will use the pointer pData, obtained from xFetch().
** If successful, set *ppPage to point to the new page reference
** and return SQLITE_OK. Otherwise, return an SQLite error code and set
** *ppPage to zero.
**
** Page references obtained by calling this function should be released
** by calling pagerReleaseMapPage().
*/
static int pagerAcquireMapPage(
  Pager *pPager,                  /* Pager object */
  Pgno pgno,                      /* Page number */
  void *pData,                    /* xFetch()'d data for this page */
  PgHdr **ppPage                  /* OUT: Acquired page object */
){
  PgHdr *p;                       /* Memory mapped page to return */
  
  if( pPager->pMmapFreelist ){
    *ppPage = p = pPager->pMmapFreelist;
    pPager->pMmapFreelist = p->pDirty;
    p->pDirty = 0;
    assert( pPager->nExtra>=8 );
    memset(p->pExtra, 0, 8);
  }else{
    *ppPage = p = (PgHdr *)sqlite3MallocZero(sizeof(PgHdr) + pPager->nExtra);
    if( p==0 ){
      sqlite3OsUnfetch(pPager->fd, (i64)(pgno-1) * pPager->pageSize, pData);
      return SQLITE_NOMEM_BKPT;
    }
    p->pExtra = (void *)&p[1];
    p->flags = PGHDR_MMAP;
    p->nRef = 1;
    p->pPager = pPager;
  }

  assert( p->pExtra==(void *)&p[1] );
  assert( p->pPage==0 );
  assert( p->flags==PGHDR_MMAP );
  assert( p->pPager==pPager );
  assert( p->nRef==1 );

  p->pgno = pgno;
  p->pData = pData;
  pPager->nMmapOut++;

  return SQLITE_OK;
}
#endif

/*
** Release a reference to page pPg. pPg must have been returned by an 
** earlier call to pagerAcquireMapPage().
*/
static void pagerReleaseMapPage(PgHdr *pPg){
  Pager *pPager = pPg->pPager;
  pPager->nMmapOut--;
  pPg->pDirty = pPager->pMmapFreelist;
  pPager->pMmapFreelist = pPg;

  assert( pPager->fd->pMethods->iVersion>=3 );
  sqlite3OsUnfetch(pPager->fd, (i64)(pPg->pgno-1)*pPager->pageSize, pPg->pData);
}

/*
** Free all PgHdr objects stored in the Pager.pMmapFreelist list.
*/
static void pagerFreeMapHdrs(Pager *pPager){
  PgHdr *p;
  PgHdr *pNext;
  for(p=pPager->pMmapFreelist; p; p=pNext){
    pNext = p->pDirty;
    sqlite3_free(p);
  }
}

/* Verify that the database file has not be deleted or renamed out from
** under the pager.  Return SQLITE_OK if the database is still where it ought
** to be on disk.  Return non-zero (SQLITE_READONLY_DBMOVED or some other error
** code from sqlite3OsAccess()) if the database has gone missing.
*/
static int databaseIsUnmoved(Pager *pPager){
  int bHasMoved = 0;
  int rc;

  if( pPager->tempFile ) return SQLITE_OK;
  if( pPager->dbSize==0 ) return SQLITE_OK;
  assert( pPager->zFilename && pPager->zFilename[0] );
  rc = sqlite3OsFileControl(pPager->fd, SQLITE_FCNTL_HAS_MOVED, &bHasMoved);
  if( rc==SQLITE_NOTFOUND ){
    /* If the HAS_MOVED file-control is unimplemented, assume that the file
    ** has not been moved.  That is the historical behavior of SQLite: prior to
    ** version 3.8.3, it never checked */
    rc = SQLITE_OK;
  }else if( rc==SQLITE_OK && bHasMoved ){
    rc = SQLITE_READONLY_DBMOVED;
  }
  return rc;
}


/*
** Shutdown the page cache.  Free all memory and close all files.
**
** If a transaction was in progress when this routine is called, that
** transaction is rolled back.  All outstanding pages are invalidated
** and their memory is freed.  Any attempt to use a page associated
** with this page cache after this function returns will likely
** result in a coredump.
**
** This function always succeeds. If a transaction is active an attempt
** is made to roll it back. If an error occurs during the rollback 
** a hot journal may be left in the filesystem but no error is returned
** to the caller.
*/
SQLITE_PRIVATE int sqlite3PagerClose(Pager *pPager, sqlite3 *db){
  u8 *pTmp = (u8*)pPager->pTmpSpace;
  assert( db || pagerUseWal(pPager)==0 );
  assert( assert_pager_state(pPager) );
  disable_simulated_io_errors();
  sqlite3BeginBenignMalloc();
  pagerFreeMapHdrs(pPager);
  /* pPager->errCode = 0; */
  pPager->exclusiveMode = 0;
#ifndef SQLITE_OMIT_WAL
  {
    u8 *a = 0;
    assert( db || pPager->pWal==0 );
    if( db && 0==(db->flags & SQLITE_NoCkptOnClose) 
     && SQLITE_OK==databaseIsUnmoved(pPager)
    ){
      a = pTmp;
    }
    sqlite3WalClose(pPager->pWal, db, pPager->walSyncFlags, pPager->pageSize,a);
    pPager->pWal = 0;
  }
#endif
  pager_reset(pPager);
  if( MEMDB ){
    pager_unlock(pPager);
  }else{
    /* If it is open, sync the journal file before calling UnlockAndRollback.
    ** If this is not done, then an unsynced portion of the open journal 
    ** file may be played back into the database. If a power failure occurs 
    ** while this is happening, the database could become corrupt.
    **
    ** If an error occurs while trying to sync the journal, shift the pager
    ** into the ERROR state. This causes UnlockAndRollback to unlock the
    ** database and close the journal file without attempting to roll it
    ** back or finalize it. The next database user will have to do hot-journal
    ** rollback before accessing the database file.
    */
    if( isOpen(pPager->jfd) ){
      pager_error(pPager, pagerSyncHotJournal(pPager));
    }
    pagerUnlockAndRollback(pPager);
  }
  sqlite3EndBenignMalloc();
  enable_simulated_io_errors();
  PAGERTRACE(("CLOSE %d\n", PAGERID(pPager)));
  IOTRACE(("CLOSE %p\n", pPager))
  sqlite3OsClose(pPager->jfd);
  sqlite3OsClose(pPager->fd);
  sqlite3PageFree(pTmp);
  sqlite3PcacheClose(pPager->pPCache);

#ifdef SQLITE_HAS_CODEC
  if( pPager->xCodecFree ) pPager->xCodecFree(pPager->pCodec);
#endif

  assert( !pPager->aSavepoint && !pPager->pInJournal );
  assert( !isOpen(pPager->jfd) && !isOpen(pPager->sjfd) );

  uk_free(flexos_shared_alloc, pPager);
  return SQLITE_OK;
}

#if !defined(NDEBUG) || defined(SQLITE_TEST)
/*
** Return the page number for page pPg.
*/
SQLITE_PRIVATE Pgno sqlite3PagerPagenumber(DbPage *pPg){
  return pPg->pgno;
}
#endif

/*
** Increment the reference count for page pPg.
*/
SQLITE_PRIVATE void sqlite3PagerRef(DbPage *pPg){
  sqlite3PcacheRef(pPg);
}

/*
** Sync the journal. In other words, make sure all the pages that have
** been written to the journal have actually reached the surface of the
** disk and can be restored in the event of a hot-journal rollback.
**
** If the Pager.noSync flag is set, then this function is a no-op.
** Otherwise, the actions required depend on the journal-mode and the 
** device characteristics of the file-system, as follows:
**
**   * If the journal file is an in-memory journal file, no action need
**     be taken.
**
**   * Otherwise, if the device does not support the SAFE_APPEND property,
**     then the nRec field of the most recently written journal header
**     is updated to contain the number of journal records that have
**     been written following it. If the pager is operating in full-sync
**     mode, then the journal file is synced before this field is updated.
**
**   * If the device does not support the SEQUENTIAL property, then 
**     journal file is synced.
**
** Or, in pseudo-code:
**
**   if( NOT <in-memory journal> ){
**     if( NOT SAFE_APPEND ){
**       if( <full-sync mode> ) xSync(<journal file>);
**       <update nRec field>
**     } 
**     if( NOT SEQUENTIAL ) xSync(<journal file>);
**   }
**
** If successful, this routine clears the PGHDR_NEED_SYNC flag of every 
** page currently held in memory before returning SQLITE_OK. If an IO
** error is encountered, then the IO error code is returned to the caller.
*/
static int _syncJournal(Pager *pPager, int newHdr){
  int rc;                         /* Return code */

  assert( pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
  );
  assert( assert_pager_state(pPager) );
  assert( !pagerUseWal(pPager) );

  rc = sqlite3PagerExclusiveLock(pPager);
  if( rc!=SQLITE_OK ) return rc;

  if( !pPager->noSync ){
    assert( !pPager->tempFile );
    if( isOpen(pPager->jfd) && pPager->journalMode!=PAGER_JOURNALMODE_MEMORY ){
      const int iDc = sqlite3OsDeviceCharacteristics(pPager->fd);
      assert( isOpen(pPager->jfd) );

      if( 0==(iDc&SQLITE_IOCAP_SAFE_APPEND) ){
        /* This block deals with an obscure problem. If the last connection
        ** that wrote to this database was operating in persistent-journal
        ** mode, then the journal file may at this point actually be larger
        ** than Pager.journalOff bytes. If the next thing in the journal
        ** file happens to be a journal-header (written as part of the
        ** previous connection's transaction), and a crash or power-failure 
        ** occurs after nRec is updated but before this connection writes 
        ** anything else to the journal file (or commits/rolls back its 
        ** transaction), then SQLite may become confused when doing the 
        ** hot-journal rollback following recovery. It may roll back all
        ** of this connections data, then proceed to rolling back the old,
        ** out-of-date data that follows it. Database corruption.
        **
        ** To work around this, if the journal file does appear to contain
        ** a valid header following Pager.journalOff, then write a 0x00
        ** byte to the start of it to prevent it from being recognized.
        **
        ** Variable iNextHdrOffset is set to the offset at which this
        ** problematic header will occur, if it exists. aMagic is used 
        ** as a temporary buffer to inspect the first couple of bytes of
        ** the potential journal header.
        */
        i64 iNextHdrOffset;
        u8 aMagic[8];
        //u8 zHeader[sizeof(aJournalMagic)+4];
        u8* zHeader = uk_malloc(flexos_shared_alloc, sizeof(aJournalMagic)+4);

        memcpy(zHeader, aJournalMagic, sizeof(aJournalMagic));
        put32bits(&zHeader[sizeof(aJournalMagic)], pPager->nRec);

        iNextHdrOffset = journalHdrOffset(pPager);
        rc = sqlite3OsRead(pPager->jfd, aMagic, 8, iNextHdrOffset);
        if( rc==SQLITE_OK && 0==memcmp(aMagic, aJournalMagic, 8) ){
          static const u8 zerobyte = 0;
          rc = sqlite3OsWrite(pPager->jfd, &zerobyte, 1, iNextHdrOffset);
        }
        if( rc!=SQLITE_OK && rc!=SQLITE_IOERR_SHORT_READ ){
          uk_free(flexos_shared_alloc, zHeader);
          return rc;
        }

        /* Write the nRec value into the journal file header. If in
        ** full-synchronous mode, sync the journal first. This ensures that
        ** all data has really hit the disk before nRec is updated to mark
        ** it as a candidate for rollback.
        **
        ** This is not required if the persistent media supports the
        ** SAFE_APPEND property. Because in this case it is not possible 
        ** for garbage data to be appended to the file, the nRec field
        ** is populated with 0xFFFFFFFF when the journal header is written
        ** and never needs to be updated.
        */
        if( pPager->fullSync && 0==(iDc&SQLITE_IOCAP_SEQUENTIAL) ){
          PAGERTRACE(("SYNC journal of %d\n", PAGERID(pPager)));
          IOTRACE(("JSYNC %p\n", pPager))
          rc = sqlite3OsSync(pPager->jfd, pPager->syncFlags);
          if( rc!=SQLITE_OK ) return rc;
        }
        IOTRACE(("JHDR %p %lld\n", pPager, pPager->journalHdr));
        rc = sqlite3OsWrite(
            pPager->jfd, zHeader, sizeof(zHeader), pPager->journalHdr
        );
        uk_free(flexos_shared_alloc, zHeader);
        if( rc!=SQLITE_OK ) return rc;
      }
      if( 0==(iDc&SQLITE_IOCAP_SEQUENTIAL) ){
        PAGERTRACE(("SYNC journal of %d\n", PAGERID(pPager)));
        IOTRACE(("JSYNC %p\n", pPager))
        rc = sqlite3OsSync(pPager->jfd, pPager->syncFlags| 
          (pPager->syncFlags==SQLITE_SYNC_FULL?SQLITE_SYNC_DATAONLY:0)
        );
        if( rc!=SQLITE_OK ) return rc;
      }

      pPager->journalHdr = pPager->journalOff;
      if( newHdr && 0==(iDc&SQLITE_IOCAP_SAFE_APPEND) ){
        pPager->nRec = 0;
        rc = writeJournalHdr(pPager);
        if( rc!=SQLITE_OK ) return rc;
      }
    }else{
      pPager->journalHdr = pPager->journalOff;
    }
  }

  /* Unless the pager is in noSync mode, the journal file was just 
  ** successfully synced. Either way, clear the PGHDR_NEED_SYNC flag on 
  ** all pages.
  */
  sqlite3PcacheClearSyncFlags(pPager->pPCache);
  pPager->eState = PAGER_WRITER_DBMOD;
  assert( assert_pager_state(pPager) );
  return SQLITE_OK;
}

static int syncJournal(Pager *pPager, int newHdr){
  int rc;                         /* Return code */

  assert( pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
  );
  assert( assert_pager_state(pPager) );
  assert( !pagerUseWal(pPager) );

  rc = sqlite3PagerExclusiveLock(pPager);
  if( rc!=SQLITE_OK ) return rc;

  if( !pPager->noSync ){
    assert( !pPager->tempFile );
    if( isOpen(pPager->jfd) && pPager->journalMode!=PAGER_JOURNALMODE_MEMORY ){
      const int iDc = sqlite3OsDeviceCharacteristics(pPager->fd);
      assert( isOpen(pPager->jfd) );

      if( 0==(iDc&SQLITE_IOCAP_SAFE_APPEND) ){
        rc = _syncJournal(pPager, iDc);
	if( rc!=SQLITE_OK ) return rc;
      }
      if( 0==(iDc&SQLITE_IOCAP_SEQUENTIAL) ){
        PAGERTRACE(("SYNC journal of %d\n", PAGERID(pPager)));
        IOTRACE(("JSYNC %p\n", pPager))
        rc = sqlite3OsSync(pPager->jfd, pPager->syncFlags| 
          (pPager->syncFlags==SQLITE_SYNC_FULL?SQLITE_SYNC_DATAONLY:0)
        );
        if( rc!=SQLITE_OK ) return rc;
      }

      pPager->journalHdr = pPager->journalOff;
      if( newHdr && 0==(iDc&SQLITE_IOCAP_SAFE_APPEND) ){
        pPager->nRec = 0;
        rc = writeJournalHdr(pPager);
        if( rc!=SQLITE_OK ) return rc;
      }
    }else{
      pPager->journalHdr = pPager->journalOff;
    }
  }

  /* Unless the pager is in noSync mode, the journal file was just 
  ** successfully synced. Either way, clear the PGHDR_NEED_SYNC flag on 
  ** all pages.
  */
  sqlite3PcacheClearSyncFlags(pPager->pPCache);
  pPager->eState = PAGER_WRITER_DBMOD;
  assert( assert_pager_state(pPager) );
  return SQLITE_OK;
}

/*
** The argument is the first in a linked list of dirty pages connected
** by the PgHdr.pDirty pointer. This function writes each one of the
** in-memory pages in the list to the database file. The argument may
** be NULL, representing an empty list. In this case this function is
** a no-op.
**
** The pager must hold at least a RESERVED lock when this function
** is called. Before writing anything to the database file, this lock
** is upgraded to an EXCLUSIVE lock. If the lock cannot be obtained,
** SQLITE_BUSY is returned and no data is written to the database file.
** 
** If the pager is a temp-file pager and the actual file-system file
** is not yet open, it is created and opened before any data is 
** written out.
**
** Once the lock has been upgraded and, if necessary, the file opened,
** the pages are written out to the database file in list order. Writing
** a page is skipped if it meets either of the following criteria:
**
**   * The page number is greater than Pager.dbSize, or
**   * The PGHDR_DONT_WRITE flag is set on the page.
**
** If writing out a page causes the database file to grow, Pager.dbFileSize
** is updated accordingly. If page 1 is written out, then the value cached
** in Pager.dbFileVers[] is updated to match the new value stored in
** the database file.
**
** If everything is successful, SQLITE_OK is returned. If an IO error 
** occurs, an IO error code is returned. Or, if the EXCLUSIVE lock cannot
** be obtained, SQLITE_BUSY is returned.
*/
static int pager_write_pagelist(Pager *pPager, PgHdr *pList){
  int rc = SQLITE_OK;                  /* Return code */

  /* This function is only called for rollback pagers in WRITER_DBMOD state. */
  assert( !pagerUseWal(pPager) );
  assert( pPager->tempFile || pPager->eState==PAGER_WRITER_DBMOD );
  assert( pPager->eLock==EXCLUSIVE_LOCK );
  assert( isOpen(pPager->fd) || pList->pDirty==0 );

  /* If the file is a temp-file has not yet been opened, open it now. It
  ** is not possible for rc to be other than SQLITE_OK if this branch
  ** is taken, as pager_wait_on_lock() is a no-op for temp-files.
  */
  if( !isOpen(pPager->fd) ){
    assert( pPager->tempFile && rc==SQLITE_OK );
    rc = pagerOpentemp(pPager, pPager->fd, pPager->vfsFlags);
  }

  /* Before the first write, give the VFS a hint of what the final
  ** file size will be.
  */
  assert( rc!=SQLITE_OK || isOpen(pPager->fd) );
  if( rc==SQLITE_OK 
   && pPager->dbHintSize<pPager->dbSize
   && (pList->pDirty || pList->pgno>pPager->dbHintSize)
  ){
    sqlite3_int64 szFile = pPager->pageSize * (sqlite3_int64)pPager->dbSize;
    sqlite3OsFileControlHint(pPager->fd, SQLITE_FCNTL_SIZE_HINT, &szFile);
    pPager->dbHintSize = pPager->dbSize;
  }

  while( rc==SQLITE_OK && pList ){
    Pgno pgno = pList->pgno;

    /* If there are dirty pages in the page cache with page numbers greater
    ** than Pager.dbSize, this means sqlite3PagerTruncateImage() was called to
    ** make the file smaller (presumably by auto-vacuum code). Do not write
    ** any such pages to the file.
    **
    ** Also, do not write out any page that has the PGHDR_DONT_WRITE flag
    ** set (set by sqlite3PagerDontWrite()).
    */
    if( pgno<=pPager->dbSize && 0==(pList->flags&PGHDR_DONT_WRITE) ){
      i64 offset = (pgno-1)*(i64)pPager->pageSize;   /* Offset to write */
      char *pData;                                   /* Data to write */    

      assert( (pList->flags&PGHDR_NEED_SYNC)==0 );
      if( pList->pgno==1 ) pager_write_changecounter(pList);

      /* Encode the database */
      CODEC2(pPager, pList->pData, pgno, 6, return SQLITE_NOMEM_BKPT, pData);

      /* Write out the page data. */
      rc = sqlite3OsWrite(pPager->fd, pData, pPager->pageSize, offset);

      /* If page 1 was just written, update Pager.dbFileVers to match
      ** the value now stored in the database file. If writing this 
      ** page caused the database file to grow, update dbFileSize. 
      */
      if( pgno==1 ){
        memcpy(&pPager->dbFileVers, &pData[24], sizeof(pPager->dbFileVers));
      }
      if( pgno>pPager->dbFileSize ){
        pPager->dbFileSize = pgno;
      }
      pPager->aStat[PAGER_STAT_WRITE]++;

      /* Update any backup objects copying the contents of this pager. */
      sqlite3BackupUpdate(pPager->pBackup, pgno, (u8*)pList->pData);

      PAGERTRACE(("STORE %d page %d hash(%08x)\n",
                   PAGERID(pPager), pgno, pager_pagehash(pList)));
      IOTRACE(("PGOUT %p %d\n", pPager, pgno));
      PAGER_INCR(sqlite3_pager_writedb_count);
    }else{
      PAGERTRACE(("NOSTORE %d page %d\n", PAGERID(pPager), pgno));
    }
    pager_set_pagehash(pList);
    pList = pList->pDirty;
  }

  return rc;
}

/*
** Ensure that the sub-journal file is open. If it is already open, this 
** function is a no-op.
**
** SQLITE_OK is returned if everything goes according to plan. An 
** SQLITE_IOERR_XXX error code is returned if a call to sqlite3OsOpen() 
** fails.
*/
static int openSubJournal(Pager *pPager){
  int rc = SQLITE_OK;
  if( !isOpen(pPager->sjfd) ){
    const int flags =  SQLITE_OPEN_SUBJOURNAL | SQLITE_OPEN_READWRITE 
      | SQLITE_OPEN_CREATE | SQLITE_OPEN_EXCLUSIVE 
      | SQLITE_OPEN_DELETEONCLOSE;
    int nStmtSpill = sqlite3Config.nStmtSpill;
    if( pPager->journalMode==PAGER_JOURNALMODE_MEMORY || pPager->subjInMemory ){
      nStmtSpill = -1;
    }
    rc = sqlite3JournalOpen(pPager->pVfs, 0, pPager->sjfd, flags, nStmtSpill);
  }
  return rc;
}

/*
** Append a record of the current state of page pPg to the sub-journal. 
**
** If successful, set the bit corresponding to pPg->pgno in the bitvecs
** for all open savepoints before returning.
**
** This function returns SQLITE_OK if everything is successful, an IO
** error code if the attempt to write to the sub-journal fails, or 
** SQLITE_NOMEM if a malloc fails while setting a bit in a savepoint
** bitvec.
*/
static int subjournalPage(PgHdr *pPg){
  int rc = SQLITE_OK;
  Pager *pPager = pPg->pPager;
  if( pPager->journalMode!=PAGER_JOURNALMODE_OFF ){

    /* Open the sub-journal, if it has not already been opened */
    assert( pPager->useJournal );
    assert( isOpen(pPager->jfd) || pagerUseWal(pPager) );
    assert( isOpen(pPager->sjfd) || pPager->nSubRec==0 );
    assert( pagerUseWal(pPager) 
         || pageInJournal(pPager, pPg) 
         || pPg->pgno>pPager->dbOrigSize 
    );
    rc = openSubJournal(pPager);

    /* If the sub-journal was opened successfully (or was already open),
    ** write the journal record into the file.  */
    if( rc==SQLITE_OK ){
      void *pData = pPg->pData;
      i64 offset = (i64)pPager->nSubRec*(4+pPager->pageSize);
      char *pData2;

#if SQLITE_HAS_CODEC   
      if( !pPager->subjInMemory ){
        CODEC2(pPager, pData, pPg->pgno, 7, return SQLITE_NOMEM_BKPT, pData2);
      }else
#endif
      pData2 = pData;
      PAGERTRACE(("STMT-JOURNAL %d page %d\n", PAGERID(pPager), pPg->pgno));
      rc = write32bits(pPager->sjfd, offset, pPg->pgno);
      if( rc==SQLITE_OK ){
        rc = sqlite3OsWrite(pPager->sjfd, pData2, pPager->pageSize, offset+4);
      }
    }
  }
  if( rc==SQLITE_OK ){
    pPager->nSubRec++;
    assert( pPager->nSavepoint>0 );
    rc = addToSavepointBitvecs(pPager, pPg->pgno);
  }
  return rc;
}
static int subjournalPageIfRequired(PgHdr *pPg){
  if( subjRequiresPage(pPg) ){
    return subjournalPage(pPg);
  }else{
    return SQLITE_OK;
  }
}

/*
** This function is called by the pcache layer when it has reached some
** soft memory limit. The first argument is a pointer to a Pager object
** (cast as a void*). The pager is always 'purgeable' (not an in-memory
** database). The second argument is a reference to a page that is 
** currently dirty but has no outstanding references. The page
** is always associated with the Pager object passed as the first 
** argument.
**
** The job of this function is to make pPg clean by writing its contents
** out to the database file, if possible. This may involve syncing the
** journal file. 
**
** If successful, sqlite3PcacheMakeClean() is called on the page and
** SQLITE_OK returned. If an IO error occurs while trying to make the
** page clean, the IO error code is returned. If the page cannot be
** made clean for some other reason, but no error occurs, then SQLITE_OK
** is returned by sqlite3PcacheMakeClean() is not called.
*/
static int pagerStress(void *p, PgHdr *pPg){
  Pager *pPager = (Pager *)p;
  int rc = SQLITE_OK;

  assert( pPg->pPager==pPager );
  assert( pPg->flags&PGHDR_DIRTY );

  /* The doNotSpill NOSYNC bit is set during times when doing a sync of
  ** journal (and adding a new header) is not allowed.  This occurs
  ** during calls to sqlite3PagerWrite() while trying to journal multiple
  ** pages belonging to the same sector.
  **
  ** The doNotSpill ROLLBACK and OFF bits inhibits all cache spilling
  ** regardless of whether or not a sync is required.  This is set during
  ** a rollback or by user request, respectively.
  **
  ** Spilling is also prohibited when in an error state since that could
  ** lead to database corruption.   In the current implementation it 
  ** is impossible for sqlite3PcacheFetch() to be called with createFlag==3
  ** while in the error state, hence it is impossible for this routine to
  ** be called in the error state.  Nevertheless, we include a NEVER()
  ** test for the error state as a safeguard against future changes.
  */
  if( NEVER(pPager->errCode) ) return SQLITE_OK;
  testcase( pPager->doNotSpill & SPILLFLAG_ROLLBACK );
  testcase( pPager->doNotSpill & SPILLFLAG_OFF );
  testcase( pPager->doNotSpill & SPILLFLAG_NOSYNC );
  if( pPager->doNotSpill
   && ((pPager->doNotSpill & (SPILLFLAG_ROLLBACK|SPILLFLAG_OFF))!=0
      || (pPg->flags & PGHDR_NEED_SYNC)!=0)
  ){
    return SQLITE_OK;
  }

  pPager->aStat[PAGER_STAT_SPILL]++;
  pPg->pDirty = 0;
  if( pagerUseWal(pPager) ){
    /* Write a single frame for this page to the log. */
    rc = subjournalPageIfRequired(pPg); 
    if( rc==SQLITE_OK ){
      rc = pagerWalFrames(pPager, pPg, 0, 0);
    }
  }else{
    
#ifdef SQLITE_ENABLE_BATCH_ATOMIC_WRITE
    if( pPager->tempFile==0 ){
      rc = sqlite3JournalCreate(pPager->jfd);
      if( rc!=SQLITE_OK ) return pager_error(pPager, rc);
    }
#endif
  
    /* Sync the journal file if required. */
    if( pPg->flags&PGHDR_NEED_SYNC 
     || pPager->eState==PAGER_WRITER_CACHEMOD
    ){
      rc = syncJournal(pPager, 1);
    }
  
    /* Write the contents of the page out to the database file. */
    if( rc==SQLITE_OK ){
      assert( (pPg->flags&PGHDR_NEED_SYNC)==0 );
      rc = pager_write_pagelist(pPager, pPg);
    }
  }

  /* Mark the page as clean. */
  if( rc==SQLITE_OK ){
    PAGERTRACE(("STRESS %d page %d\n", PAGERID(pPager), pPg->pgno));
    sqlite3PcacheMakeClean(pPg);
  }

  return pager_error(pPager, rc); 
}

/*
** Flush all unreferenced dirty pages to disk.
*/
SQLITE_PRIVATE int sqlite3PagerFlush(Pager *pPager){
  int rc = pPager->errCode;
  if( !MEMDB ){
    PgHdr *pList = sqlite3PcacheDirtyList(pPager->pPCache);
    assert( assert_pager_state(pPager) );
    while( rc==SQLITE_OK && pList ){
      PgHdr *pNext = pList->pDirty;
      if( pList->nRef==0 ){
        rc = pagerStress((void*)pPager, pList);
      }
      pList = pNext;
    }
  }

  return rc;
}

/*
** Allocate and initialize a new Pager object and put a pointer to it
** in *ppPager. The pager should eventually be freed by passing it
** to sqlite3PagerClose().
**
** The zFilename argument is the path to the database file to open.
** If zFilename is NULL then a randomly-named temporary file is created
** and used as the file to be cached. Temporary files are be deleted
** automatically when they are closed. If zFilename is ":memory:" then 
** all information is held in cache. It is never written to disk. 
** This can be used to implement an in-memory database.
**
** The nExtra parameter specifies the number of bytes of space allocated
** along with each page reference. This space is available to the user
** via the sqlite3PagerGetExtra() API.  When a new page is allocated, the
** first 8 bytes of this space are zeroed but the remainder is uninitialized.
** (The extra space is used by btree as the MemPage object.)
**
** The flags argument is used to specify properties that affect the
** operation of the pager. It should be passed some bitwise combination
** of the PAGER_* flags.
**
** The vfsFlags parameter is a bitmask to pass to the flags parameter
** of the xOpen() method of the supplied VFS when opening files. 
**
** If the pager object is allocated and the specified file opened 
** successfully, SQLITE_OK is returned and *ppPager set to point to
** the new pager object. If an error occurs, *ppPager is set to NULL
** and error code returned. This function may return SQLITE_NOMEM
** (sqlite3Malloc() is used to allocate memory), SQLITE_CANTOPEN or 
** various SQLITE_IO_XXX errors.
*/
SQLITE_PRIVATE int sqlite3PagerOpen(
  sqlite3_vfs *pVfs,       /* The virtual file system to use */
  Pager **ppPager,         /* OUT: Return the Pager structure here */
  const char *zFilename,   /* Name of the database file to open */
  int nExtra,              /* Extra bytes append to each in-memory page */
  int flags,               /* flags controlling this file */
  int vfsFlags,            /* flags passed through to sqlite3_vfs.xOpen() */
  void (*xReinit)(DbPage*) /* Function to reinitialize pages */
){
  u8 *pPtr;
  Pager *pPager = 0;       /* Pager object to allocate and return */
  int rc = SQLITE_OK;      /* Return code */
  int tempFile = 0;        /* True for temp files (incl. in-memory files) */
  int memDb = 0;           /* True if this is an in-memory file */
#ifdef SQLITE_ENABLE_DESERIALIZE
  int memJM = 0;           /* Memory journal mode */
#else
# define memJM 0
#endif
  int readOnly = 0;        /* True if this is a read-only file */
  int journalFileSize;     /* Bytes to allocate for each journal fd */
  char *zPathname = 0;     /* Full path to database file */
  int nPathname = 0;       /* Number of bytes in zPathname */
  int useJournal = (flags & PAGER_OMIT_JOURNAL)==0; /* False to omit journal */
  int pcacheSize = sqlite3PcacheSize();       /* Bytes to allocate for PCache */
  u32 szPageDflt = SQLITE_DEFAULT_PAGE_SIZE;  /* Default page size */
  const char *zUri = 0;    /* URI args to copy */
  int nUri = 0;            /* Number of bytes of URI args at *zUri */

  /* Figure out how much space is required for each journal file-handle
  ** (there are two of them, the main journal and the sub-journal).  */
  journalFileSize = ROUND8(sqlite3JournalSize(pVfs));

  /* Set the output variable to NULL in case an error occurs. */
  *ppPager = 0;

#ifndef SQLITE_OMIT_MEMORYDB
  if( flags & PAGER_MEMORY ){
    memDb = 1;
    if( zFilename && zFilename[0] ){
      zPathname = sqlite3DbStrDup(0, zFilename);
      if( zPathname==0  ) return SQLITE_NOMEM_BKPT;
      nPathname = sqlite3Strlen30(zPathname);
      zFilename = 0;
    }
  }
#endif

  /* Compute and store the full pathname in an allocated buffer pointed
  ** to by zPathname, length nPathname. Or, if this is a temporary file,
  ** leave both nPathname and zPathname set to 0.
  */
  if( zFilename && zFilename[0] ){
    const char *z;
    nPathname = pVfs->mxPathname+1;
    zPathname = uk_malloc(flexos_shared_alloc, nPathname * 2);
    if( zPathname==0 ){
      return SQLITE_NOMEM_BKPT;
    }
    zPathname[0] = 0; /* Make sure initialized even if FullPathname() fails */
    rc = sqlite3OsFullPathname(pVfs, zFilename, nPathname, zPathname);
    nPathname = sqlite3Strlen30(zPathname);
    z = zUri = &zFilename[sqlite3Strlen30(zFilename)+1];
    while( *z ){
      z += sqlite3Strlen30(z)+1;
      z += sqlite3Strlen30(z)+1;
    }
    nUri = (int)(&z[1] - zUri);
    assert( nUri>=0 );
    if( rc==SQLITE_OK && nPathname+8>pVfs->mxPathname ){
      /* This branch is taken when the journal path required by
      ** the database being opened will be more than pVfs->mxPathname
      ** bytes in length. This means the database cannot be opened,
      ** as it will not be possible to open the journal file or even
      ** check for a hot-journal before reading.
      */
      rc = SQLITE_CANTOPEN_BKPT;
    }
    if( rc!=SQLITE_OK ){
      uk_free(flexos_shared_alloc, zPathname);
      return rc;
    }
  }

  /* Allocate memory for the Pager structure, PCache object, the
  ** three file descriptors, the database file name and the journal 
  ** file name. The layout in memory is as follows:
  **
  **     Pager object                    (sizeof(Pager) bytes)
  **     PCache object                   (sqlite3PcacheSize() bytes)
  **     Database file handle            (pVfs->szOsFile bytes)
  **     Sub-journal file handle         (journalFileSize bytes)
  **     Main journal file handle        (journalFileSize bytes)
  **     Database file name              (nPathname+1 bytes)
  **     Journal file name               (nPathname+8+1 bytes)
  */
#ifdef SQLITE_OMIT_WAL
  pPtr = (u8 *) uk_calloc(flexos_shared_alloc, 1,
                          ROUND8(sizeof(*pPager)) + ROUND8(pcacheSize) + ROUND8(pVfs->szOsFile) + journalFileSize * 2 + nPathname + 1 + nUri + nPathname + 8 + 2);
#else
  pPtr = (u8 *) uk_calloc(flexos_shared_alloc, 1,
                          ROUND8(sizeof(*pPager)) + ROUND8(pcacheSize) + ROUND8(pVfs->szOsFile) + journalFileSize * 2 + nPathname + 1 + nUri + nPathname + 8 + 2 + nPathname + 4 + 2);
#endif
  assert( EIGHT_BYTE_ALIGNMENT(SQLITE_INT_TO_PTR(journalFileSize)) );
  if( !pPtr ){
    uk_free(flexos_shared_alloc, zPathname);
    return SQLITE_NOMEM_BKPT;
  }
  pPager =              (Pager*)(pPtr);
  pPager->pPCache =    (PCache*)(pPtr += ROUND8(sizeof(*pPager)));
  pPager->fd =   (sqlite3_file*)(pPtr += ROUND8(pcacheSize));
  pPager->sjfd = (sqlite3_file*)(pPtr += ROUND8(pVfs->szOsFile));
  pPager->jfd =  (sqlite3_file*)(pPtr += journalFileSize);
  pPager->zFilename =    (char*)(pPtr += journalFileSize);
  assert( EIGHT_BYTE_ALIGNMENT(pPager->jfd) );

  /* Fill in the Pager.zFilename and Pager.zJournal buffers, if required. */
  if( zPathname ){
    assert( nPathname>0 );
    pPager->zJournal =   (char*)(pPtr += nPathname + 1 + nUri);
    memcpy(pPager->zFilename, zPathname, nPathname);
    if( nUri ) memcpy(&pPager->zFilename[nPathname+1], zUri, nUri);
    memcpy(pPager->zJournal, zPathname, nPathname);
    memcpy(&pPager->zJournal[nPathname], "-journal\000", 8+2);
    sqlite3FileSuffix3(pPager->zFilename, pPager->zJournal);
#ifndef SQLITE_OMIT_WAL
    pPager->zWal = &pPager->zJournal[nPathname+8+1];
    memcpy(pPager->zWal, zPathname, nPathname);
    memcpy(&pPager->zWal[nPathname], "-wal\000", 4+1);
    sqlite3FileSuffix3(pPager->zFilename, pPager->zWal);
#endif
    uk_free(flexos_shared_alloc, zPathname);
  }
  pPager->pVfs = pVfs;
  pPager->vfsFlags = vfsFlags;

  /* Open the pager file.
  */
  if( zFilename && zFilename[0] ){
    int fout = 0;                    /* VFS flags returned by xOpen() */
    rc = sqlite3OsOpen(pVfs, pPager->zFilename, pPager->fd, vfsFlags, &fout);
    assert( !memDb );
#ifdef SQLITE_ENABLE_DESERIALIZE
    memJM = (fout&SQLITE_OPEN_MEMORY)!=0;
#endif
    readOnly = (fout&SQLITE_OPEN_READONLY)!=0;

    /* If the file was successfully opened for read/write access,
    ** choose a default page size in case we have to create the
    ** database file. The default page size is the maximum of:
    **
    **    + SQLITE_DEFAULT_PAGE_SIZE,
    **    + The value returned by sqlite3OsSectorSize()
    **    + The largest page size that can be written atomically.
    */
    if( rc==SQLITE_OK ){
      int iDc = sqlite3OsDeviceCharacteristics(pPager->fd);
      if( !readOnly ){
        setSectorSize(pPager);
        assert(SQLITE_DEFAULT_PAGE_SIZE<=SQLITE_MAX_DEFAULT_PAGE_SIZE);
        if( szPageDflt<pPager->sectorSize ){
          if( pPager->sectorSize>SQLITE_MAX_DEFAULT_PAGE_SIZE ){
            szPageDflt = SQLITE_MAX_DEFAULT_PAGE_SIZE;
          }else{
            szPageDflt = (u32)pPager->sectorSize;
          }
        }
#ifdef SQLITE_ENABLE_ATOMIC_WRITE
        {
          int ii;
          assert(SQLITE_IOCAP_ATOMIC512==(512>>8));
          assert(SQLITE_IOCAP_ATOMIC64K==(65536>>8));
          assert(SQLITE_MAX_DEFAULT_PAGE_SIZE<=65536);
          for(ii=szPageDflt; ii<=SQLITE_MAX_DEFAULT_PAGE_SIZE; ii=ii*2){
            if( iDc&(SQLITE_IOCAP_ATOMIC|(ii>>8)) ){
              szPageDflt = ii;
            }
          }
        }
#endif
      }
      pPager->noLock = sqlite3_uri_boolean(zFilename, "nolock", 0);
      if( (iDc & SQLITE_IOCAP_IMMUTABLE)!=0
       || sqlite3_uri_boolean(zFilename, "immutable", 0) ){
          vfsFlags |= SQLITE_OPEN_READONLY;
          goto act_like_temp_file;
      }
    }
  }else{
    /* If a temporary file is requested, it is not opened immediately.
    ** In this case we accept the default page size and delay actually
    ** opening the file until the first call to OsWrite().
    **
    ** This branch is also run for an in-memory database. An in-memory
    ** database is the same as a temp-file that is never written out to
    ** disk and uses an in-memory rollback journal.
    **
    ** This branch also runs for files marked as immutable.
    */ 
act_like_temp_file:
    tempFile = 1;
    pPager->eState = PAGER_READER;     /* Pretend we already have a lock */
    pPager->eLock = EXCLUSIVE_LOCK;    /* Pretend we are in EXCLUSIVE mode */
    pPager->noLock = 1;                /* Do no locking */
    readOnly = (vfsFlags&SQLITE_OPEN_READONLY);
  }

  /* The following call to PagerSetPagesize() serves to set the value of 
  ** Pager.pageSize and to allocate the Pager.pTmpSpace buffer.
  */
  if( rc==SQLITE_OK ){
    assert( pPager->memDb==0 );
    rc = sqlite3PagerSetPagesize(pPager, &szPageDflt, -1);
    testcase( rc!=SQLITE_OK );
  }

  /* Initialize the PCache object. */
  if( rc==SQLITE_OK ){
    nExtra = ROUND8(nExtra);
    assert( nExtra>=8 && nExtra<1000 );
    rc = sqlite3PcacheOpen(szPageDflt, nExtra, !memDb,
                       !memDb?pagerStress:0, (void *)pPager, pPager->pPCache);
  }

  /* If an error occurred above, free the  Pager structure and close the file.
  */
  if( rc!=SQLITE_OK ){
    sqlite3OsClose(pPager->fd);
    sqlite3PageFree(pPager->pTmpSpace);
    uk_free(flexos_shared_alloc, pPager);
    return rc;
  }

  PAGERTRACE(("OPEN %d %s\n", FILEHANDLEID(pPager->fd), pPager->zFilename));
  IOTRACE(("OPEN %p %s\n", pPager, pPager->zFilename))

  pPager->useJournal = (u8)useJournal;
  /* pPager->stmtOpen = 0; */
  /* pPager->stmtInUse = 0; */
  /* pPager->nRef = 0; */
  /* pPager->stmtSize = 0; */
  /* pPager->stmtJSize = 0; */
  /* pPager->nPage = 0; */
  pPager->mxPgno = SQLITE_MAX_PAGE_COUNT;
  /* pPager->state = PAGER_UNLOCK; */
  /* pPager->errMask = 0; */
  pPager->tempFile = (u8)tempFile;
  assert( tempFile==PAGER_LOCKINGMODE_NORMAL 
          || tempFile==PAGER_LOCKINGMODE_EXCLUSIVE );
  assert( PAGER_LOCKINGMODE_EXCLUSIVE==1 );
  pPager->exclusiveMode = (u8)tempFile; 
  pPager->changeCountDone = pPager->tempFile;
  pPager->memDb = (u8)memDb;
  pPager->readOnly = (u8)readOnly;
  assert( useJournal || pPager->tempFile );
  pPager->noSync = pPager->tempFile;
  if( pPager->noSync ){
    assert( pPager->fullSync==0 );
    assert( pPager->extraSync==0 );
    assert( pPager->syncFlags==0 );
    assert( pPager->walSyncFlags==0 );
  }else{
    pPager->fullSync = 1;
    pPager->extraSync = 0;
    pPager->syncFlags = SQLITE_SYNC_NORMAL;
    pPager->walSyncFlags = SQLITE_SYNC_NORMAL | (SQLITE_SYNC_NORMAL<<2);
  }
  /* pPager->pFirst = 0; */
  /* pPager->pFirstSynced = 0; */
  /* pPager->pLast = 0; */
  pPager->nExtra = (u16)nExtra;
  pPager->journalSizeLimit = SQLITE_DEFAULT_JOURNAL_SIZE_LIMIT;
  assert( isOpen(pPager->fd) || tempFile );
  setSectorSize(pPager);
  if( !useJournal ){
    pPager->journalMode = PAGER_JOURNALMODE_OFF;
  }else if( memDb || memJM ){
    pPager->journalMode = PAGER_JOURNALMODE_MEMORY;
  }
  /* pPager->xBusyHandler = 0; */
  /* pPager->pBusyHandlerArg = 0; */
  pPager->xReiniter = xReinit;
  setGetterMethod(pPager);
  /* memset(pPager->aHash, 0, sizeof(pPager->aHash)); */
  /* pPager->szMmap = SQLITE_DEFAULT_MMAP_SIZE // will be set by btree.c */

  *ppPager = pPager;
  return SQLITE_OK;
}



/*
** This function is called after transitioning from PAGER_UNLOCK to
** PAGER_SHARED state. It tests if there is a hot journal present in
** the file-system for the given pager. A hot journal is one that 
** needs to be played back. According to this function, a hot-journal
** file exists if the following criteria are met:
**
**   * The journal file exists in the file system, and
**   * No process holds a RESERVED or greater lock on the database file, and
**   * The database file itself is greater than 0 bytes in size, and
**   * The first byte of the journal file exists and is not 0x00.
**
** If the current size of the database file is 0 but a journal file
** exists, that is probably an old journal left over from a prior
** database with the same name. In this case the journal file is
** just deleted using OsDelete, *pExists is set to 0 and SQLITE_OK
** is returned.
**
** This routine does not check if there is a master journal filename
** at the end of the file. If there is, and that master journal file
** does not exist, then the journal file is not really hot. In this
** case this routine will return a false-positive. The pager_playback()
** routine will discover that the journal file is not really hot and 
** will not roll it back. 
**
** If a hot-journal file is found to exist, *pExists is set to 1 and 
** SQLITE_OK returned. If no hot-journal file is present, *pExists is
** set to 0 and SQLITE_OK returned. If an IO error occurs while trying
** to determine whether or not a hot-journal file exists, the IO error
** code is returned and the value of *pExists is undefined.
*/
static int hasHotJournal(Pager *pPager, int *pExists){
  sqlite3_vfs * const pVfs = pPager->pVfs;
  int rc = SQLITE_OK;           /* Return code */
  int exists = 1;               /* True if a journal file is present */
  int jrnlOpen = !!isOpen(pPager->jfd);

  assert( pPager->useJournal );
  assert( isOpen(pPager->fd) );
  assert( pPager->eState==PAGER_OPEN );

  assert( jrnlOpen==0 || ( sqlite3OsDeviceCharacteristics(pPager->jfd) &
    SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN
  ));

  *pExists = 0;
  if( !jrnlOpen ){
    rc = sqlite3OsAccess(pVfs, pPager->zJournal, SQLITE_ACCESS_EXISTS, &exists);
  }
  if( rc==SQLITE_OK && exists ){
    int locked = 0;             /* True if some process holds a RESERVED lock */

    /* Race condition here:  Another process might have been holding the
    ** the RESERVED lock and have a journal open at the sqlite3OsAccess() 
    ** call above, but then delete the journal and drop the lock before
    ** we get to the following sqlite3OsCheckReservedLock() call.  If that
    ** is the case, this routine might think there is a hot journal when
    ** in fact there is none.  This results in a false-positive which will
    ** be dealt with by the playback routine.  Ticket #3883.
    */
    rc = sqlite3OsCheckReservedLock(pPager->fd, &locked);
    if( rc==SQLITE_OK && !locked ){
      Pgno nPage;                 /* Number of pages in database file */

      assert( pPager->tempFile==0 );
      rc = pagerPagecount(pPager, &nPage);
      if( rc==SQLITE_OK ){
        /* If the database is zero pages in size, that means that either (1) the
        ** journal is a remnant from a prior database with the same name where
        ** the database file but not the journal was deleted, or (2) the initial
        ** transaction that populates a new database is being rolled back.
        ** In either case, the journal file can be deleted.  However, take care
        ** not to delete the journal file if it is already open due to
        ** journal_mode=PERSIST.
        */
        if( nPage==0 && !jrnlOpen ){
          sqlite3BeginBenignMalloc();
          if( pagerLockDb(pPager, RESERVED_LOCK)==SQLITE_OK ){
            sqlite3OsDelete(pVfs, pPager->zJournal, 0);
            if( !pPager->exclusiveMode ) pagerUnlockDb(pPager, SHARED_LOCK);
          }
          sqlite3EndBenignMalloc();
        }else{
          /* The journal file exists and no other connection has a reserved
          ** or greater lock on the database file. Now check that there is
          ** at least one non-zero bytes at the start of the journal file.
          ** If there is, then we consider this journal to be hot. If not, 
          ** it can be ignored.
          */
          if( !jrnlOpen ){
            int f = SQLITE_OPEN_READONLY|SQLITE_OPEN_MAIN_JOURNAL;
            rc = sqlite3OsOpen(pVfs, pPager->zJournal, pPager->jfd, f, &f);
          }
          if( rc==SQLITE_OK ){
            u8 first = 0;
            rc = sqlite3OsRead(pPager->jfd, (void *)&first, 1, 0);
            if( rc==SQLITE_IOERR_SHORT_READ ){
              rc = SQLITE_OK;
            }
            if( !jrnlOpen ){
              sqlite3OsClose(pPager->jfd);
            }
            *pExists = (first!=0);
          }else if( rc==SQLITE_CANTOPEN ){
            /* If we cannot open the rollback journal file in order to see if
            ** it has a zero header, that might be due to an I/O error, or
            ** it might be due to the race condition described above and in
            ** ticket #3883.  Either way, assume that the journal is hot.
            ** This might be a false positive.  But if it is, then the
            ** automatic journal playback and recovery mechanism will deal
            ** with it under an EXCLUSIVE lock where we do not need to
            ** worry so much with race conditions.
            */
            *pExists = 1;
            rc = SQLITE_OK;
          }
        }
      }
    }
  }

  return rc;
}

/*
** This function is called to obtain a shared lock on the database file.
** It is illegal to call sqlite3PagerGet() until after this function
** has been successfully called. If a shared-lock is already held when
** this function is called, it is a no-op.
**
** The following operations are also performed by this function.
**
**   1) If the pager is currently in PAGER_OPEN state (no lock held
**      on the database file), then an attempt is made to obtain a
**      SHARED lock on the database file. Immediately after obtaining
**      the SHARED lock, the file-system is checked for a hot-journal,
**      which is played back if present. Following any hot-journal 
**      rollback, the contents of the cache are validated by checking
**      the 'change-counter' field of the database file header and
**      discarded if they are found to be invalid.
**
**   2) If the pager is running in exclusive-mode, and there are currently
**      no outstanding references to any pages, and is in the error state,
**      then an attempt is made to clear the error state by discarding
**      the contents of the page cache and rolling back any open journal
**      file.
**
** If everything is successful, SQLITE_OK is returned. If an IO error 
** occurs while locking the database, checking for a hot-journal file or 
** rolling back a journal file, the IO error code is returned.
*/
SQLITE_PRIVATE int sqlite3PagerSharedLock(Pager *pPager){
  int rc = SQLITE_OK;                /* Return code */
  //char dbFileVers[sizeof(pPager->dbFileVers)];
  char* dbFileVers = uk_malloc(flexos_shared_alloc, (sizeof(pPager->dbFileVers) * sizeof(char)));

  /* This routine is only called from b-tree and only when there are no
  ** outstanding pages. This implies that the pager state should either
  ** be OPEN or READER. READER is only possible if the pager is or was in 
  ** exclusive access mode.  */
  assert( sqlite3PcacheRefCount(pPager->pPCache)==0 );
  assert( assert_pager_state(pPager) );
  assert( pPager->eState==PAGER_OPEN || pPager->eState==PAGER_READER );
  assert( pPager->errCode==SQLITE_OK );

  if( !pagerUseWal(pPager) && pPager->eState==PAGER_OPEN ){
    int bHotJournal = 1;          /* True if there exists a hot journal-file */

    assert( !MEMDB );
    assert( pPager->tempFile==0 || pPager->eLock==EXCLUSIVE_LOCK );

    rc = pager_wait_on_lock(pPager, SHARED_LOCK);
    if( rc!=SQLITE_OK ){
      assert( pPager->eLock==NO_LOCK || pPager->eLock==UNKNOWN_LOCK );
      goto failed;
    }

    /* If a journal file exists, and there is no RESERVED lock on the
    ** database file, then it either needs to be played back or deleted.
    */
    if( pPager->eLock<=SHARED_LOCK ){
      rc = hasHotJournal(pPager, &bHotJournal);
    }
    if( rc!=SQLITE_OK ){
      goto failed;
    }
    if( bHotJournal ){
      if( pPager->readOnly ){
        rc = SQLITE_READONLY_ROLLBACK;
        goto failed;
      }

      /* Get an EXCLUSIVE lock on the database file. At this point it is
      ** important that a RESERVED lock is not obtained on the way to the
      ** EXCLUSIVE lock. If it were, another process might open the
      ** database file, detect the RESERVED lock, and conclude that the
      ** database is safe to read while this process is still rolling the 
      ** hot-journal back.
      ** 
      ** Because the intermediate RESERVED lock is not requested, any
      ** other process attempting to access the database file will get to 
      ** this point in the code and fail to obtain its own EXCLUSIVE lock 
      ** on the database file.
      **
      ** Unless the pager is in locking_mode=exclusive mode, the lock is
      ** downgraded to SHARED_LOCK before this function returns.
      */
      rc = pagerLockDb(pPager, EXCLUSIVE_LOCK);
      if( rc!=SQLITE_OK ){
        goto failed;
      }
 
      /* If it is not already open and the file exists on disk, open the 
      ** journal for read/write access. Write access is required because 
      ** in exclusive-access mode the file descriptor will be kept open 
      ** and possibly used for a transaction later on. Also, write-access 
      ** is usually required to finalize the journal in journal_mode=persist 
      ** mode (and also for journal_mode=truncate on some systems).
      **
      ** If the journal does not exist, it usually means that some 
      ** other connection managed to get in and roll it back before 
      ** this connection obtained the exclusive lock above. Or, it 
      ** may mean that the pager was in the error-state when this
      ** function was called and the journal file does not exist.
      */
      if( !isOpen(pPager->jfd) ){
        sqlite3_vfs * const pVfs = pPager->pVfs;
        int bExists;              /* True if journal file exists */
        rc = sqlite3OsAccess(
            pVfs, pPager->zJournal, SQLITE_ACCESS_EXISTS, &bExists);
        if( rc==SQLITE_OK && bExists ){
          int fout = 0;
          int f = SQLITE_OPEN_READWRITE|SQLITE_OPEN_MAIN_JOURNAL;
          assert( !pPager->tempFile );
          rc = sqlite3OsOpen(pVfs, pPager->zJournal, pPager->jfd, f, &fout);
          assert( rc!=SQLITE_OK || isOpen(pPager->jfd) );
          if( rc==SQLITE_OK && fout&SQLITE_OPEN_READONLY ){
            rc = SQLITE_CANTOPEN_BKPT;
            sqlite3OsClose(pPager->jfd);
          }
        }
      }
 
      /* Playback and delete the journal.  Drop the database write
      ** lock and reacquire the read lock. Purge the cache before
      ** playing back the hot-journal so that we don't end up with
      ** an inconsistent cache.  Sync the hot journal before playing
      ** it back since the process that crashed and left the hot journal
      ** probably did not sync it and we are required to always sync
      ** the journal before playing it back.
      */
      if( isOpen(pPager->jfd) ){
        assert( rc==SQLITE_OK );
        rc = pagerSyncHotJournal(pPager);
        if( rc==SQLITE_OK ){
          rc = pager_playback(pPager, !pPager->tempFile);
          pPager->eState = PAGER_OPEN;
        }
      }else if( !pPager->exclusiveMode ){
        pagerUnlockDb(pPager, SHARED_LOCK);
      }

      if( rc!=SQLITE_OK ){
        /* This branch is taken if an error occurs while trying to open
        ** or roll back a hot-journal while holding an EXCLUSIVE lock. The
        ** pager_unlock() routine will be called before returning to unlock
        ** the file. If the unlock attempt fails, then Pager.eLock must be
        ** set to UNKNOWN_LOCK (see the comment above the #define for 
        ** UNKNOWN_LOCK above for an explanation). 
        **
        ** In order to get pager_unlock() to do this, set Pager.eState to
        ** PAGER_ERROR now. This is not actually counted as a transition
        ** to ERROR state in the state diagram at the top of this file,
        ** since we know that the same call to pager_unlock() will very
        ** shortly transition the pager object to the OPEN state. Calling
        ** assert_pager_state() would fail now, as it should not be possible
        ** to be in ERROR state when there are zero outstanding page 
        ** references.
        */
        pager_error(pPager, rc);
        goto failed;
      }

      assert( pPager->eState==PAGER_OPEN );
      assert( (pPager->eLock==SHARED_LOCK)
           || (pPager->exclusiveMode && pPager->eLock>SHARED_LOCK)
      );
    }

    if( !pPager->tempFile && pPager->hasHeldSharedLock ){
      /* The shared-lock has just been acquired then check to
      ** see if the database has been modified.  If the database has changed,
      ** flush the cache.  The hasHeldSharedLock flag prevents this from
      ** occurring on the very first access to a file, in order to save a
      ** single unnecessary sqlite3OsRead() call at the start-up.
      **
      ** Database changes are detected by looking at 15 bytes beginning
      ** at offset 24 into the file.  The first 4 of these 16 bytes are
      ** a 32-bit counter that is incremented with each change.  The
      ** other bytes change randomly with each file change when
      ** a codec is in use.
      ** 
      ** There is a vanishingly small chance that a change will not be 
      ** detected.  The chance of an undetected change is so small that
      ** it can be neglected.
      */

      IOTRACE(("CKVERS %p %d\n", pPager, sizeof(dbFileVers)));
      rc = sqlite3OsRead(pPager->fd, dbFileVers, sizeof(dbFileVers), 24);
      if( rc!=SQLITE_OK ){
        if( rc!=SQLITE_IOERR_SHORT_READ ){
          goto failed;
        }
        memset(dbFileVers, 0, sizeof(dbFileVers));
      }

      if( memcmp(pPager->dbFileVers, dbFileVers, sizeof(dbFileVers))!=0 ){
        pager_reset(pPager);

        /* Unmap the database file. It is possible that external processes
        ** may have truncated the database file and then extended it back
        ** to its original size while this process was not holding a lock.
        ** In this case there may exist a Pager.pMap mapping that appears
        ** to be the right size but is not actually valid. Avoid this
        ** possibility by unmapping the db here. */
        if( USEFETCH(pPager) ){
          sqlite3OsUnfetch(pPager->fd, 0, 0);
        }
      }
      uk_free(flexos_shared_alloc, dbFileVers);
    }

    /* If there is a WAL file in the file-system, open this database in WAL
    ** mode. Otherwise, the following function call is a no-op.
    */
    rc = pagerOpenWalIfPresent(pPager);
#ifndef SQLITE_OMIT_WAL
    assert( pPager->pWal==0 || rc==SQLITE_OK );
#endif
  }

  if( pagerUseWal(pPager) ){
    assert( rc==SQLITE_OK );
    rc = pagerBeginReadTransaction(pPager);
  }

  if( pPager->tempFile==0 && pPager->eState==PAGER_OPEN && rc==SQLITE_OK ){
    rc = pagerPagecount(pPager, &pPager->dbSize);
  }

 failed:
  if( rc!=SQLITE_OK ){
    assert( !MEMDB );
    pager_unlock(pPager);
    assert( pPager->eState==PAGER_OPEN );
  }else{
    pPager->eState = PAGER_READER;
    pPager->hasHeldSharedLock = 1;
  }
  return rc;
}

/*
** If the reference count has reached zero, rollback any active
** transaction and unlock the pager.
**
** Except, in locking_mode=EXCLUSIVE when there is nothing to in
** the rollback journal, the unlock is not performed and there is
** nothing to rollback, so this routine is a no-op.
*/ 
static void pagerUnlockIfUnused(Pager *pPager){
  if( sqlite3PcacheRefCount(pPager->pPCache)==0 ){
    assert( pPager->nMmapOut==0 ); /* because page1 is never memory mapped */
    pagerUnlockAndRollback(pPager);
  }
}

/*
** The page getter methods each try to acquire a reference to a
** page with page number pgno. If the requested reference is 
** successfully obtained, it is copied to *ppPage and SQLITE_OK returned.
**
** There are different implementations of the getter method depending
** on the current state of the pager.
**
**     getPageNormal()         --  The normal getter
**     getPageError()          --  Used if the pager is in an error state
**     getPageMmap()           --  Used if memory-mapped I/O is enabled
**
** If the requested page is already in the cache, it is returned. 
** Otherwise, a new page object is allocated and populated with data
** read from the database file. In some cases, the pcache module may
** choose not to allocate a new page object and may reuse an existing
** object with no outstanding references.
**
** The extra data appended to a page is always initialized to zeros the 
** first time a page is loaded into memory. If the page requested is 
** already in the cache when this function is called, then the extra
** data is left as it was when the page object was last used.
**
** If the database image is smaller than the requested page or if 
** the flags parameter contains the PAGER_GET_NOCONTENT bit and the 
** requested page is not already stored in the cache, then no 
** actual disk read occurs. In this case the memory image of the 
** page is initialized to all zeros. 
**
** If PAGER_GET_NOCONTENT is true, it means that we do not care about
** the contents of the page. This occurs in two scenarios:
**
**   a) When reading a free-list leaf page from the database, and
**
**   b) When a savepoint is being rolled back and we need to load
**      a new page into the cache to be filled with the data read
**      from the savepoint journal.
**
** If PAGER_GET_NOCONTENT is true, then the data returned is zeroed instead
** of being read from the database. Additionally, the bits corresponding
** to pgno in Pager.pInJournal (bitvec of pages already written to the
** journal file) and the PagerSavepoint.pInSavepoint bitvecs of any open
** savepoints are set. This means if the page is made writable at any
** point in the future, using a call to sqlite3PagerWrite(), its contents
** will not be journaled. This saves IO.
**
** The acquisition might fail for several reasons.  In all cases,
** an appropriate error code is returned and *ppPage is set to NULL.
**
** See also sqlite3PagerLookup().  Both this routine and Lookup() attempt
** to find a page in the in-memory cache first.  If the page is not already
** in memory, this routine goes to disk to read it in whereas Lookup()
** just returns 0.  This routine acquires a read-lock the first time it
** has to go to disk, and could also playback an old journal if necessary.
** Since Lookup() never goes to disk, it never has to deal with locks
** or journal files.
*/
static int getPageNormal(
  Pager *pPager,      /* The pager open on the database file */
  Pgno pgno,          /* Page number to fetch */
  DbPage **ppPage,    /* Write a pointer to the page here */
  int flags           /* PAGER_GET_XXX flags */
){
  int rc = SQLITE_OK;
  PgHdr *pPg;
  u8 noContent;                   /* True if PAGER_GET_NOCONTENT is set */
  sqlite3_pcache_page *pBase;

  assert( pPager->errCode==SQLITE_OK );
  assert( pPager->eState>=PAGER_READER );
  assert( assert_pager_state(pPager) );
  assert( pPager->hasHeldSharedLock==1 );

  if( pgno==0 ) return SQLITE_CORRUPT_BKPT;
  pBase = sqlite3PcacheFetch(pPager->pPCache, pgno, 3);
  if( pBase==0 ){
    pPg = 0;
    rc = sqlite3PcacheFetchStress(pPager->pPCache, pgno, &pBase);
    if( rc!=SQLITE_OK ) goto pager_acquire_err;
    if( pBase==0 ){
      rc = SQLITE_NOMEM_BKPT;
      goto pager_acquire_err;
    }
  }
  pPg = *ppPage = sqlite3PcacheFetchFinish(pPager->pPCache, pgno, pBase);
  assert( pPg==(*ppPage) );
  assert( pPg->pgno==pgno );
  assert( pPg->pPager==pPager || pPg->pPager==0 );

  noContent = (flags & PAGER_GET_NOCONTENT)!=0;
  if( pPg->pPager && !noContent ){
    /* In this case the pcache already contains an initialized copy of
    ** the page. Return without further ado.  */
    assert( pgno<=PAGER_MAX_PGNO && pgno!=PAGER_MJ_PGNO(pPager) );
    pPager->aStat[PAGER_STAT_HIT]++;
    return SQLITE_OK;

  }else{
    /* The pager cache has created a new page. Its content needs to 
    ** be initialized. But first some error checks:
    **
    ** (1) The maximum page number is 2^31
    ** (2) Never try to fetch the locking page
    */
    if( pgno>PAGER_MAX_PGNO || pgno==PAGER_MJ_PGNO(pPager) ){
      rc = SQLITE_CORRUPT_BKPT;
      goto pager_acquire_err;
    }

    pPg->pPager = pPager;

    assert( !isOpen(pPager->fd) || !MEMDB );
    if( !isOpen(pPager->fd) || pPager->dbSize<pgno || noContent ){
      if( pgno>pPager->mxPgno ){
        rc = SQLITE_FULL;
        goto pager_acquire_err;
      }
      if( noContent ){
        /* Failure to set the bits in the InJournal bit-vectors is benign.
        ** It merely means that we might do some extra work to journal a 
        ** page that does not need to be journaled.  Nevertheless, be sure 
        ** to test the case where a malloc error occurs while trying to set 
        ** a bit in a bit vector.
        */
        sqlite3BeginBenignMalloc();
        if( pgno<=pPager->dbOrigSize ){
          TESTONLY( rc = ) sqlite3BitvecSet(pPager->pInJournal, pgno);
          testcase( rc==SQLITE_NOMEM );
        }
        TESTONLY( rc = ) addToSavepointBitvecs(pPager, pgno);
        testcase( rc==SQLITE_NOMEM );
        sqlite3EndBenignMalloc();
      }
      memset(pPg->pData, 0, pPager->pageSize);
      IOTRACE(("ZERO %p %d\n", pPager, pgno));
    }else{
      assert( pPg->pPager==pPager );
      pPager->aStat[PAGER_STAT_MISS]++;
      rc = readDbPage(pPg);
      if( rc!=SQLITE_OK ){
        goto pager_acquire_err;
      }
    }
    pager_set_pagehash(pPg);
  }
  return SQLITE_OK;

pager_acquire_err:
  assert( rc!=SQLITE_OK );
  if( pPg ){
    sqlite3PcacheDrop(pPg);
  }
  pagerUnlockIfUnused(pPager);
  *ppPage = 0;
  return rc;
}

#if SQLITE_MAX_MMAP_SIZE>0
/* The page getter for when memory-mapped I/O is enabled */
static int getPageMMap(
  Pager *pPager,      /* The pager open on the database file */
  Pgno pgno,          /* Page number to fetch */
  DbPage **ppPage,    /* Write a pointer to the page here */
  int flags           /* PAGER_GET_XXX flags */
){
  int rc = SQLITE_OK;
  PgHdr *pPg = 0;
  u32 iFrame = 0;                 /* Frame to read from WAL file */

  /* It is acceptable to use a read-only (mmap) page for any page except
  ** page 1 if there is no write-transaction open or the ACQUIRE_READONLY
  ** flag was specified by the caller. And so long as the db is not a 
  ** temporary or in-memory database.  */
  const int bMmapOk = (pgno>1
   && (pPager->eState==PAGER_READER || (flags & PAGER_GET_READONLY))
  );

  assert( USEFETCH(pPager) );
#ifdef SQLITE_HAS_CODEC
  assert( pPager->xCodec==0 );
#endif

  /* Optimization note:  Adding the "pgno<=1" term before "pgno==0" here
  ** allows the compiler optimizer to reuse the results of the "pgno>1"
  ** test in the previous statement, and avoid testing pgno==0 in the
  ** common case where pgno is large. */
  if( pgno<=1 && pgno==0 ){
    return SQLITE_CORRUPT_BKPT;
  }
  assert( pPager->eState>=PAGER_READER );
  assert( assert_pager_state(pPager) );
  assert( pPager->hasHeldSharedLock==1 );
  assert( pPager->errCode==SQLITE_OK );

  if( bMmapOk && pagerUseWal(pPager) ){
    rc = sqlite3WalFindFrame(pPager->pWal, pgno, &iFrame);
    if( rc!=SQLITE_OK ){
      *ppPage = 0;
      return rc;
    }
  }
  if( bMmapOk && iFrame==0 ){
    void *pData = 0;
    rc = sqlite3OsFetch(pPager->fd, 
        (i64)(pgno-1) * pPager->pageSize, pPager->pageSize, &pData
    );
    if( rc==SQLITE_OK && pData ){
      if( pPager->eState>PAGER_READER || pPager->tempFile ){
        pPg = sqlite3PagerLookup(pPager, pgno);
      }
      if( pPg==0 ){
        rc = pagerAcquireMapPage(pPager, pgno, pData, &pPg);
      }else{
        sqlite3OsUnfetch(pPager->fd, (i64)(pgno-1)*pPager->pageSize, pData);
      }
      if( pPg ){
        assert( rc==SQLITE_OK );
        *ppPage = pPg;
        return SQLITE_OK;
      }
    }
    if( rc!=SQLITE_OK ){
      *ppPage = 0;
      return rc;
    }
  }
  return getPageNormal(pPager, pgno, ppPage, flags);
}
#endif /* SQLITE_MAX_MMAP_SIZE>0 */

/* The page getter method for when the pager is an error state */
static int getPageError(
  Pager *pPager,      /* The pager open on the database file */
  Pgno pgno,          /* Page number to fetch */
  DbPage **ppPage,    /* Write a pointer to the page here */
  int flags           /* PAGER_GET_XXX flags */
){
  UNUSED_PARAMETER(pgno);
  UNUSED_PARAMETER(flags);
  assert( pPager->errCode!=SQLITE_OK );
  *ppPage = 0;
  return pPager->errCode;
}


/* Dispatch all page fetch requests to the appropriate getter method.
*/
SQLITE_PRIVATE int sqlite3PagerGet(
  Pager *pPager,      /* The pager open on the database file */
  Pgno pgno,          /* Page number to fetch */
  DbPage **ppPage,    /* Write a pointer to the page here */
  int flags           /* PAGER_GET_XXX flags */
){
  return pPager->xGet(pPager, pgno, ppPage, flags);
}

/*
** Acquire a page if it is already in the in-memory cache.  Do
** not read the page from disk.  Return a pointer to the page,
** or 0 if the page is not in cache. 
**
** See also sqlite3PagerGet().  The difference between this routine
** and sqlite3PagerGet() is that _get() will go to the disk and read
** in the page if the page is not already in cache.  This routine
** returns NULL if the page is not in cache or if a disk I/O error 
** has ever happened.
*/
SQLITE_PRIVATE DbPage *sqlite3PagerLookup(Pager *pPager, Pgno pgno){
  sqlite3_pcache_page *pPage;
  assert( pPager!=0 );
  assert( pgno!=0 );
  assert( pPager->pPCache!=0 );
  pPage = sqlite3PcacheFetch(pPager->pPCache, pgno, 0);
  assert( pPage==0 || pPager->hasHeldSharedLock );
  if( pPage==0 ) return 0;
  return sqlite3PcacheFetchFinish(pPager->pPCache, pgno, pPage);
}

/*
** Release a page reference.
**
** The sqlite3PagerUnref() and sqlite3PagerUnrefNotNull() may only be
** used if we know that the page being released is not the last page.
** The btree layer always holds page1 open until the end, so these first
** to routines can be used to release any page other than BtShared.pPage1.
**
** Use sqlite3PagerUnrefPageOne() to release page1.  This latter routine
** checks the total number of outstanding pages and if the number of
** pages reaches zero it drops the database lock.
*/
SQLITE_PRIVATE void sqlite3PagerUnrefNotNull(DbPage *pPg){
  TESTONLY( Pager *pPager = pPg->pPager; )
  assert( pPg!=0 );
  if( pPg->flags & PGHDR_MMAP ){
    assert( pPg->pgno!=1 );  /* Page1 is never memory mapped */
    pagerReleaseMapPage(pPg);
  }else{
    sqlite3PcacheRelease(pPg);
  }
  /* Do not use this routine to release the last reference to page1 */
  assert( sqlite3PcacheRefCount(pPager->pPCache)>0 );
}
SQLITE_PRIVATE void sqlite3PagerUnref(DbPage *pPg){
  if( pPg ) sqlite3PagerUnrefNotNull(pPg);
}
SQLITE_PRIVATE void sqlite3PagerUnrefPageOne(DbPage *pPg){
  Pager *pPager;
  assert( pPg!=0 );
  assert( pPg->pgno==1 );
  assert( (pPg->flags & PGHDR_MMAP)==0 ); /* Page1 is never memory mapped */
  pPager = pPg->pPager;
  sqlite3PagerResetLockTimeout(pPager);
  sqlite3PcacheRelease(pPg);
  pagerUnlockIfUnused(pPager);
}

/*
** This function is called at the start of every write transaction.
** There must already be a RESERVED or EXCLUSIVE lock on the database 
** file when this routine is called.
**
** Open the journal file for pager pPager and write a journal header
** to the start of it. If there are active savepoints, open the sub-journal
** as well. This function is only used when the journal file is being 
** opened to write a rollback log for a transaction. It is not used 
** when opening a hot journal file to roll it back.
**
** If the journal file is already open (as it may be in exclusive mode),
** then this function just writes a journal header to the start of the
** already open file. 
**
** Whether or not the journal file is opened by this function, the
** Pager.pInJournal bitvec structure is allocated.
**
** Return SQLITE_OK if everything is successful. Otherwise, return 
** SQLITE_NOMEM if the attempt to allocate Pager.pInJournal fails, or 
** an IO error code if opening or writing the journal file fails.
*/
static int pager_open_journal(Pager *pPager){
  int rc = SQLITE_OK;                        /* Return code */
  sqlite3_vfs * const pVfs = pPager->pVfs;   /* Local cache of vfs pointer */

  assert( pPager->eState==PAGER_WRITER_LOCKED );
  assert( assert_pager_state(pPager) );
  assert( pPager->pInJournal==0 );
  
  /* If already in the error state, this function is a no-op.  But on
  ** the other hand, this routine is never called if we are already in
  ** an error state. */
  if( NEVER(pPager->errCode) ) return pPager->errCode;

  if( !pagerUseWal(pPager) && pPager->journalMode!=PAGER_JOURNALMODE_OFF ){
    pPager->pInJournal = sqlite3BitvecCreate(pPager->dbSize);
    if( pPager->pInJournal==0 ){
      return SQLITE_NOMEM_BKPT;
    }
  
    /* Open the journal file if it is not already open. */
    if( !isOpen(pPager->jfd) ){
      if( pPager->journalMode==PAGER_JOURNALMODE_MEMORY ){
        sqlite3MemJournalOpen(pPager->jfd);
      }else{
        int flags = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE;
        int nSpill;

        if( pPager->tempFile ){
          flags |= (SQLITE_OPEN_DELETEONCLOSE|SQLITE_OPEN_TEMP_JOURNAL);
          nSpill = sqlite3Config.nStmtSpill;
        }else{
          flags |= SQLITE_OPEN_MAIN_JOURNAL;
          nSpill = jrnlBufferSize(pPager);
        }
          
        /* Verify that the database still has the same name as it did when
        ** it was originally opened. */
        rc = databaseIsUnmoved(pPager);
        if( rc==SQLITE_OK ){
          rc = sqlite3JournalOpen (
              pVfs, pPager->zJournal, pPager->jfd, flags, nSpill
          );
        }
      }
      assert( rc!=SQLITE_OK || isOpen(pPager->jfd) );
    }
  
  
    /* Write the first journal header to the journal file and open 
    ** the sub-journal if necessary.
    */
    if( rc==SQLITE_OK ){
      /* TODO: Check if all of these are really required. */
      pPager->nRec = 0;
      pPager->journalOff = 0;
      pPager->setMaster = 0;
      pPager->journalHdr = 0;
      rc = writeJournalHdr(pPager);
    }
  }

  if( rc!=SQLITE_OK ){
    sqlite3BitvecDestroy(pPager->pInJournal);
    pPager->pInJournal = 0;
  }else{
    assert( pPager->eState==PAGER_WRITER_LOCKED );
    pPager->eState = PAGER_WRITER_CACHEMOD;
  }

  return rc;
}

/*
** Begin a write-transaction on the specified pager object. If a 
** write-transaction has already been opened, this function is a no-op.
**
** If the exFlag argument is false, then acquire at least a RESERVED
** lock on the database file. If exFlag is true, then acquire at least
** an EXCLUSIVE lock. If such a lock is already held, no locking 
** functions need be called.
**
** If the subjInMemory argument is non-zero, then any sub-journal opened
** within this transaction will be opened as an in-memory file. This
** has no effect if the sub-journal is already opened (as it may be when
** running in exclusive mode) or if the transaction does not require a
** sub-journal. If the subjInMemory argument is zero, then any required
** sub-journal is implemented in-memory if pPager is an in-memory database, 
** or using a temporary file otherwise.
*/
SQLITE_PRIVATE int sqlite3PagerBegin(Pager *pPager, int exFlag, int subjInMemory){
  int rc = SQLITE_OK;

  if( pPager->errCode ) return pPager->errCode;
  assert( pPager->eState>=PAGER_READER && pPager->eState<PAGER_ERROR );
  pPager->subjInMemory = (u8)subjInMemory;

  if( ALWAYS(pPager->eState==PAGER_READER) ){
    assert( pPager->pInJournal==0 );

    if( pagerUseWal(pPager) ){
      /* If the pager is configured to use locking_mode=exclusive, and an
      ** exclusive lock on the database is not already held, obtain it now.
      */
      if( pPager->exclusiveMode && sqlite3WalExclusiveMode(pPager->pWal, -1) ){
        rc = pagerLockDb(pPager, EXCLUSIVE_LOCK);
        if( rc!=SQLITE_OK ){
          return rc;
        }
        (void)sqlite3WalExclusiveMode(pPager->pWal, 1);
      }

      /* Grab the write lock on the log file. If successful, upgrade to
      ** PAGER_RESERVED state. Otherwise, return an error code to the caller.
      ** The busy-handler is not invoked if another connection already
      ** holds the write-lock. If possible, the upper layer will call it.
      */
      rc = sqlite3WalBeginWriteTransaction(pPager->pWal);
    }else{
      /* Obtain a RESERVED lock on the database file. If the exFlag parameter
      ** is true, then immediately upgrade this to an EXCLUSIVE lock. The
      ** busy-handler callback can be used when upgrading to the EXCLUSIVE
      ** lock, but not when obtaining the RESERVED lock.
      */
      rc = pagerLockDb(pPager, RESERVED_LOCK);
      if( rc==SQLITE_OK && exFlag ){
        rc = pager_wait_on_lock(pPager, EXCLUSIVE_LOCK);
      }
    }

    if( rc==SQLITE_OK ){
      /* Change to WRITER_LOCKED state.
      **
      ** WAL mode sets Pager.eState to PAGER_WRITER_LOCKED or CACHEMOD
      ** when it has an open transaction, but never to DBMOD or FINISHED.
      ** This is because in those states the code to roll back savepoint 
      ** transactions may copy data from the sub-journal into the database 
      ** file as well as into the page cache. Which would be incorrect in 
      ** WAL mode.
      */
      pPager->eState = PAGER_WRITER_LOCKED;
      pPager->dbHintSize = pPager->dbSize;
      pPager->dbFileSize = pPager->dbSize;
      pPager->dbOrigSize = pPager->dbSize;
      pPager->journalOff = 0;
    }

    assert( rc==SQLITE_OK || pPager->eState==PAGER_READER );
    assert( rc!=SQLITE_OK || pPager->eState==PAGER_WRITER_LOCKED );
    assert( assert_pager_state(pPager) );
  }

  PAGERTRACE(("TRANSACTION %d\n", PAGERID(pPager)));
  return rc;
}

/*
** Write page pPg onto the end of the rollback journal.
*/
static SQLITE_NOINLINE int pagerAddPageToRollbackJournal(PgHdr *pPg){
  Pager *pPager = pPg->pPager;
  int rc;
  u32 cksum;
  i64 iOff = pPager->journalOff;
  //char *pData2 = uk_malloc(flexos_shared_alloc, pPager->pageSize * sizeof(char) * 2);
  char* pData2;
  //memcpy(pData2, pPg->pData +iOff+4, pPager->pageSize);
  //uk_pr_crit("this pointer is to %p, that one to %p\n", pData2, pPg->pData);

  /* We should never write to the journal file the page that
  ** contains the database locks.  The following assert verifies
  ** that we do not. */
  assert( pPg->pgno!=PAGER_MJ_PGNO(pPager) );

  assert( pPager->journalHdr<=pPager->journalOff );
  CODEC2(pPager, pPg->pData, pPg->pgno, 7, return SQLITE_NOMEM_BKPT, pData2);
  cksum = pager_cksum(pPager, (u8*)pData2);

  /* Even if an IO or diskfull error occurs while journalling the
  ** page in the block above, set the need-sync flag for the page.
  ** Otherwise, when the transaction is rolled back, the logic in
  ** playback_one_page() will think that the page needs to be restored
  ** in the database file. And if an IO error occurs while doing so,
  ** then corruption may follow.
  */
  pPg->flags |= PGHDR_NEED_SYNC;

  rc = write32bits(pPager->jfd, iOff, pPg->pgno);
  //uk_free(flexos_shared_alloc, pData2);
  if( rc!=SQLITE_OK ) { 
    //uk_free(flexos_shared_alloc, pData2); 
    return rc; }
  rc = sqlite3OsWrite(pPager->jfd, pData2, pPager->pageSize, iOff+4);
  if( rc!=SQLITE_OK ) { 
    //uk_free(flexos_shared_alloc, pData2); 
    return rc; }
  rc = write32bits(pPager->jfd, iOff+pPager->pageSize+4, cksum);
  if( rc!=SQLITE_OK ) { 
    //uk_free(flexos_shared_alloc, pData2); 
    return rc; }

  //uk_free(flexos_shared_alloc, pData2);

  IOTRACE(("JOUT %p %d %lld %d\n", pPager, pPg->pgno, 
           pPager->journalOff, pPager->pageSize));
  PAGER_INCR(sqlite3_pager_writej_count);
  PAGERTRACE(("JOURNAL %d page %d needSync=%d hash(%08x)\n",
       PAGERID(pPager), pPg->pgno, 
       ((pPg->flags&PGHDR_NEED_SYNC)?1:0), pager_pagehash(pPg)));

  pPager->journalOff += 8 + pPager->pageSize;
  pPager->nRec++;
  assert( pPager->pInJournal!=0 );
  rc = sqlite3BitvecSet(pPager->pInJournal, pPg->pgno);
  testcase( rc==SQLITE_NOMEM );
  assert( rc==SQLITE_OK || rc==SQLITE_NOMEM );
  rc |= addToSavepointBitvecs(pPager, pPg->pgno);
  assert( rc==SQLITE_OK || rc==SQLITE_NOMEM );
  return rc;
}

/*
** Mark a single data page as writeable. The page is written into the 
** main journal or sub-journal as required. If the page is written into
** one of the journals, the corresponding bit is set in the 
** Pager.pInJournal bitvec and the PagerSavepoint.pInSavepoint bitvecs
** of any open savepoints as appropriate.
*/
static int pager_write(PgHdr *pPg){
  Pager *pPager = pPg->pPager;
  int rc = SQLITE_OK;

  /* This routine is not called unless a write-transaction has already 
  ** been started. The journal file may or may not be open at this point.
  ** It is never called in the ERROR state.
  */
  assert( pPager->eState==PAGER_WRITER_LOCKED
       || pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
  );
  assert( assert_pager_state(pPager) );
  assert( pPager->errCode==0 );
  assert( pPager->readOnly==0 );
  CHECK_PAGE(pPg);

  /* The journal file needs to be opened. Higher level routines have already
  ** obtained the necessary locks to begin the write-transaction, but the
  ** rollback journal might not yet be open. Open it now if this is the case.
  **
  ** This is done before calling sqlite3PcacheMakeDirty() on the page. 
  ** Otherwise, if it were done after calling sqlite3PcacheMakeDirty(), then
  ** an error might occur and the pager would end up in WRITER_LOCKED state
  ** with pages marked as dirty in the cache.
  */
  if( pPager->eState==PAGER_WRITER_LOCKED ){
    rc = pager_open_journal(pPager);
    if( rc!=SQLITE_OK ) return rc;
  }
  assert( pPager->eState>=PAGER_WRITER_CACHEMOD );
  assert( assert_pager_state(pPager) );

  /* Mark the page that is about to be modified as dirty. */
  sqlite3PcacheMakeDirty(pPg);

  /* If a rollback journal is in use, them make sure the page that is about
  ** to change is in the rollback journal, or if the page is a new page off
  ** then end of the file, make sure it is marked as PGHDR_NEED_SYNC.
  */
  assert( (pPager->pInJournal!=0) == isOpen(pPager->jfd) );
  if( pPager->pInJournal!=0
   && sqlite3BitvecTestNotNull(pPager->pInJournal, pPg->pgno)==0
  ){
    assert( pagerUseWal(pPager)==0 );
    if( pPg->pgno<=pPager->dbOrigSize ){
      rc = pagerAddPageToRollbackJournal(pPg);
      if( rc!=SQLITE_OK ){
        return rc;
      }
    }else{
      if( pPager->eState!=PAGER_WRITER_DBMOD ){
        pPg->flags |= PGHDR_NEED_SYNC;
      }
      PAGERTRACE(("APPEND %d page %d needSync=%d\n",
              PAGERID(pPager), pPg->pgno,
             ((pPg->flags&PGHDR_NEED_SYNC)?1:0)));
    }
  }

  /* The PGHDR_DIRTY bit is set above when the page was added to the dirty-list
  ** and before writing the page into the rollback journal.  Wait until now,
  ** after the page has been successfully journalled, before setting the
  ** PGHDR_WRITEABLE bit that indicates that the page can be safely modified.
  */
  pPg->flags |= PGHDR_WRITEABLE;
  
  /* If the statement journal is open and the page is not in it,
  ** then write the page into the statement journal.
  */
  if( pPager->nSavepoint>0 ){
    rc = subjournalPageIfRequired(pPg);
  }

  /* Update the database size and return. */
  if( pPager->dbSize<pPg->pgno ){
    pPager->dbSize = pPg->pgno;
  }
  return rc;
}

/*
** This is a variant of sqlite3PagerWrite() that runs when the sector size
** is larger than the page size.  SQLite makes the (reasonable) assumption that
** all bytes of a sector are written together by hardware.  Hence, all bytes of
** a sector need to be journalled in case of a power loss in the middle of
** a write.
**
** Usually, the sector size is less than or equal to the page size, in which
** case pages can be individually written.  This routine only runs in the
** exceptional case where the page size is smaller than the sector size.
*/
static SQLITE_NOINLINE int pagerWriteLargeSector(PgHdr *pPg){
  int rc = SQLITE_OK;          /* Return code */
  Pgno nPageCount;             /* Total number of pages in database file */
  Pgno pg1;                    /* First page of the sector pPg is located on. */
  int nPage = 0;               /* Number of pages starting at pg1 to journal */
  int ii;                      /* Loop counter */
  int needSync = 0;            /* True if any page has PGHDR_NEED_SYNC */
  Pager *pPager = pPg->pPager; /* The pager that owns pPg */
  Pgno nPagePerSector = (pPager->sectorSize/pPager->pageSize);

  /* Set the doNotSpill NOSYNC bit to 1. This is because we cannot allow
  ** a journal header to be written between the pages journaled by
  ** this function.
  */
  assert( !MEMDB );
  assert( (pPager->doNotSpill & SPILLFLAG_NOSYNC)==0 );
  pPager->doNotSpill |= SPILLFLAG_NOSYNC;

  /* This trick assumes that both the page-size and sector-size are
  ** an integer power of 2. It sets variable pg1 to the identifier
  ** of the first page of the sector pPg is located on.
  */
  pg1 = ((pPg->pgno-1) & ~(nPagePerSector-1)) + 1;

  nPageCount = pPager->dbSize;
  if( pPg->pgno>nPageCount ){
    nPage = (pPg->pgno - pg1)+1;
  }else if( (pg1+nPagePerSector-1)>nPageCount ){
    nPage = nPageCount+1-pg1;
  }else{
    nPage = nPagePerSector;
  }
  assert(nPage>0);
  assert(pg1<=pPg->pgno);
  assert((pg1+nPage)>pPg->pgno);

  for(ii=0; ii<nPage && rc==SQLITE_OK; ii++){
    Pgno pg = pg1+ii;
    PgHdr *pPage;
    if( pg==pPg->pgno || !sqlite3BitvecTest(pPager->pInJournal, pg) ){
      if( pg!=PAGER_MJ_PGNO(pPager) ){
        rc = sqlite3PagerGet(pPager, pg, &pPage, 0);
        if( rc==SQLITE_OK ){
          rc = pager_write(pPage);
          if( pPage->flags&PGHDR_NEED_SYNC ){
            needSync = 1;
          }
          sqlite3PagerUnrefNotNull(pPage);
        }
      }
    }else if( (pPage = sqlite3PagerLookup(pPager, pg))!=0 ){
      if( pPage->flags&PGHDR_NEED_SYNC ){
        needSync = 1;
      }
      sqlite3PagerUnrefNotNull(pPage);
    }
  }

  /* If the PGHDR_NEED_SYNC flag is set for any of the nPage pages 
  ** starting at pg1, then it needs to be set for all of them. Because
  ** writing to any of these nPage pages may damage the others, the
  ** journal file must contain sync()ed copies of all of them
  ** before any of them can be written out to the database file.
  */
  if( rc==SQLITE_OK && needSync ){
    assert( !MEMDB );
    for(ii=0; ii<nPage; ii++){
      PgHdr *pPage = sqlite3PagerLookup(pPager, pg1+ii);
      if( pPage ){
        pPage->flags |= PGHDR_NEED_SYNC;
        sqlite3PagerUnrefNotNull(pPage);
      }
    }
  }

  assert( (pPager->doNotSpill & SPILLFLAG_NOSYNC)!=0 );
  pPager->doNotSpill &= ~SPILLFLAG_NOSYNC;
  return rc;
}

/*
** Mark a data page as writeable. This routine must be called before 
** making changes to a page. The caller must check the return value 
** of this function and be careful not to change any page data unless 
** this routine returns SQLITE_OK.
**
** The difference between this function and pager_write() is that this
** function also deals with the special case where 2 or more pages
** fit on a single disk sector. In this case all co-resident pages
** must have been written to the journal file before returning.
**
** If an error occurs, SQLITE_NOMEM or an IO error code is returned
** as appropriate. Otherwise, SQLITE_OK.
*/
SQLITE_PRIVATE int sqlite3PagerWrite(PgHdr *pPg){
  Pager *pPager = pPg->pPager;
  assert( (pPg->flags & PGHDR_MMAP)==0 );
  assert( pPager->eState>=PAGER_WRITER_LOCKED );
  assert( assert_pager_state(pPager) );
  if( (pPg->flags & PGHDR_WRITEABLE)!=0 && pPager->dbSize>=pPg->pgno ){
    if( pPager->nSavepoint ) return subjournalPageIfRequired(pPg);
    return SQLITE_OK;
  }else if( pPager->errCode ){
    return pPager->errCode;
  }else if( pPager->sectorSize > (u32)pPager->pageSize ){
    assert( pPager->tempFile==0 );
    return pagerWriteLargeSector(pPg);
  }else{
    return pager_write(pPg);
  }
}

/*
** Return TRUE if the page given in the argument was previously passed
** to sqlite3PagerWrite().  In other words, return TRUE if it is ok
** to change the content of the page.
*/
#ifndef NDEBUG
SQLITE_PRIVATE int sqlite3PagerIswriteable(DbPage *pPg){
  return pPg->flags & PGHDR_WRITEABLE;
}
#endif

/*
** A call to this routine tells the pager that it is not necessary to
** write the information on page pPg back to the disk, even though
** that page might be marked as dirty.  This happens, for example, when
** the page has been added as a leaf of the freelist and so its
** content no longer matters.
**
** The overlying software layer calls this routine when all of the data
** on the given page is unused. The pager marks the page as clean so
** that it does not get written to disk.
**
** Tests show that this optimization can quadruple the speed of large 
** DELETE operations.
**
** This optimization cannot be used with a temp-file, as the page may
** have been dirty at the start of the transaction. In that case, if
** memory pressure forces page pPg out of the cache, the data does need 
** to be written out to disk so that it may be read back in if the 
** current transaction is rolled back.
*/
SQLITE_PRIVATE void sqlite3PagerDontWrite(PgHdr *pPg){
  Pager *pPager = pPg->pPager;
  if( !pPager->tempFile && (pPg->flags&PGHDR_DIRTY) && pPager->nSavepoint==0 ){
    PAGERTRACE(("DONT_WRITE page %d of %d\n", pPg->pgno, PAGERID(pPager)));
    IOTRACE(("CLEAN %p %d\n", pPager, pPg->pgno))
    pPg->flags |= PGHDR_DONT_WRITE;
    pPg->flags &= ~PGHDR_WRITEABLE;
    testcase( pPg->flags & PGHDR_NEED_SYNC );
    pager_set_pagehash(pPg);
  }
}

/*
** This routine is called to increment the value of the database file 
** change-counter, stored as a 4-byte big-endian integer starting at 
** byte offset 24 of the pager file.  The secondary change counter at
** 92 is also updated, as is the SQLite version number at offset 96.
**
** But this only happens if the pPager->changeCountDone flag is false.
** To avoid excess churning of page 1, the update only happens once.
** See also the pager_write_changecounter() routine that does an 
** unconditional update of the change counters.
**
** If the isDirectMode flag is zero, then this is done by calling 
** sqlite3PagerWrite() on page 1, then modifying the contents of the
** page data. In this case the file will be updated when the current
** transaction is committed.
**
** The isDirectMode flag may only be non-zero if the library was compiled
** with the SQLITE_ENABLE_ATOMIC_WRITE macro defined. In this case,
** if isDirect is non-zero, then the database file is updated directly
** by writing an updated version of page 1 using a call to the 
** sqlite3OsWrite() function.
*/
static int pager_incr_changecounter(Pager *pPager, int isDirectMode){
  int rc = SQLITE_OK;

  assert( pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
  );
  assert( assert_pager_state(pPager) );

  /* Declare and initialize constant integer 'isDirect'. If the
  ** atomic-write optimization is enabled in this build, then isDirect
  ** is initialized to the value passed as the isDirectMode parameter
  ** to this function. Otherwise, it is always set to zero.
  **
  ** The idea is that if the atomic-write optimization is not
  ** enabled at compile time, the compiler can omit the tests of
  ** 'isDirect' below, as well as the block enclosed in the
  ** "if( isDirect )" condition.
  */
#ifndef SQLITE_ENABLE_ATOMIC_WRITE
# define DIRECT_MODE 0
  assert( isDirectMode==0 );
  UNUSED_PARAMETER(isDirectMode);
#else
# define DIRECT_MODE isDirectMode
#endif

  if( !pPager->changeCountDone && ALWAYS(pPager->dbSize>0) ){
    PgHdr *pPgHdr;                /* Reference to page 1 */

    assert( !pPager->tempFile && isOpen(pPager->fd) );

    /* Open page 1 of the file for writing. */
    rc = sqlite3PagerGet(pPager, 1, &pPgHdr, 0);
    assert( pPgHdr==0 || rc==SQLITE_OK );

    /* If page one was fetched successfully, and this function is not
    ** operating in direct-mode, make page 1 writable.  When not in 
    ** direct mode, page 1 is always held in cache and hence the PagerGet()
    ** above is always successful - hence the ALWAYS on rc==SQLITE_OK.
    */
    if( !DIRECT_MODE && ALWAYS(rc==SQLITE_OK) ){
      rc = sqlite3PagerWrite(pPgHdr);
    }

    if( rc==SQLITE_OK ){
      /* Actually do the update of the change counter */
      pager_write_changecounter(pPgHdr);

      /* If running in direct mode, write the contents of page 1 to the file. */
      if( DIRECT_MODE ){
        const void *zBuf;
        assert( pPager->dbFileSize>0 );
        CODEC2(pPager, pPgHdr->pData, 1, 6, rc=SQLITE_NOMEM_BKPT, zBuf);
        if( rc==SQLITE_OK ){
          rc = sqlite3OsWrite(pPager->fd, zBuf, pPager->pageSize, 0);
          pPager->aStat[PAGER_STAT_WRITE]++;
        }
        if( rc==SQLITE_OK ){
          /* Update the pager's copy of the change-counter. Otherwise, the
          ** next time a read transaction is opened the cache will be
          ** flushed (as the change-counter values will not match).  */
          const void *pCopy = (const void *)&((const char *)zBuf)[24];
          memcpy(&pPager->dbFileVers, pCopy, sizeof(pPager->dbFileVers));
          pPager->changeCountDone = 1;
        }
      }else{
        pPager->changeCountDone = 1;
      }
    }

    /* Release the page reference. */
    sqlite3PagerUnref(pPgHdr);
  }
  return rc;
}

/*
** Sync the database file to disk. This is a no-op for in-memory databases
** or pages with the Pager.noSync flag set.
**
** If successful, or if called on a pager for which it is a no-op, this
** function returns SQLITE_OK. Otherwise, an IO error code is returned.
*/
SQLITE_PRIVATE int sqlite3PagerSync(Pager *pPager, const char *zMaster){
  int rc = SQLITE_OK;
  void *pArg = (void*)zMaster;
  rc = sqlite3OsFileControl(pPager->fd, SQLITE_FCNTL_SYNC, pArg);
  if( rc==SQLITE_NOTFOUND ) rc = SQLITE_OK;
  if( rc==SQLITE_OK && !pPager->noSync ){
    assert( !MEMDB );
    rc = sqlite3OsSync(pPager->fd, pPager->syncFlags);
  }
  return rc;
}

/*
** This function may only be called while a write-transaction is active in
** rollback. If the connection is in WAL mode, this call is a no-op. 
** Otherwise, if the connection does not already have an EXCLUSIVE lock on 
** the database file, an attempt is made to obtain one.
**
** If the EXCLUSIVE lock is already held or the attempt to obtain it is
** successful, or the connection is in WAL mode, SQLITE_OK is returned.
** Otherwise, either SQLITE_BUSY or an SQLITE_IOERR_XXX error code is 
** returned.
*/
SQLITE_PRIVATE int sqlite3PagerExclusiveLock(Pager *pPager){
  int rc = pPager->errCode;
  assert( assert_pager_state(pPager) );
  if( rc==SQLITE_OK ){
    assert( pPager->eState==PAGER_WRITER_CACHEMOD 
         || pPager->eState==PAGER_WRITER_DBMOD 
         || pPager->eState==PAGER_WRITER_LOCKED 
    );
    assert( assert_pager_state(pPager) );
    if( 0==pagerUseWal(pPager) ){
      rc = pager_wait_on_lock(pPager, EXCLUSIVE_LOCK);
    }
  }
  return rc;
}

/*
** Sync the database file for the pager pPager. zMaster points to the name
** of a master journal file that should be written into the individual
** journal file. zMaster may be NULL, which is interpreted as no master
** journal (a single database transaction).
**
** This routine ensures that:
**
**   * The database file change-counter is updated,
**   * the journal is synced (unless the atomic-write optimization is used),
**   * all dirty pages are written to the database file, 
**   * the database file is truncated (if required), and
**   * the database file synced. 
**
** The only thing that remains to commit the transaction is to finalize 
** (delete, truncate or zero the first part of) the journal file (or 
** delete the master journal file if specified).
**
** Note that if zMaster==NULL, this does not overwrite a previous value
** passed to an sqlite3PagerCommitPhaseOne() call.
**
** If the final parameter - noSync - is true, then the database file itself
** is not synced. The caller must call sqlite3PagerSync() directly to
** sync the database file before calling CommitPhaseTwo() to delete the
** journal file in this case.
*/
SQLITE_PRIVATE int sqlite3PagerCommitPhaseOne(
  Pager *pPager,                  /* Pager object */
  const char *zMaster,            /* If not NULL, the master journal name */
  int noSync                      /* True to omit the xSync on the db file */
){
  int rc = SQLITE_OK;             /* Return code */

  assert( pPager->eState==PAGER_WRITER_LOCKED
       || pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
       || pPager->eState==PAGER_ERROR
  );
  assert( assert_pager_state(pPager) );

  /* If a prior error occurred, report that error again. */
  if( NEVER(pPager->errCode) ) return pPager->errCode;

  /* Provide the ability to easily simulate an I/O error during testing */
  if( sqlite3FaultSim(400) ) return SQLITE_IOERR;

  PAGERTRACE(("DATABASE SYNC: File=%s zMaster=%s nSize=%d\n", 
      pPager->zFilename, zMaster, pPager->dbSize));

  /* If no database changes have been made, return early. */
  if( pPager->eState<PAGER_WRITER_CACHEMOD ) return SQLITE_OK;

  assert( MEMDB==0 || pPager->tempFile );
  assert( isOpen(pPager->fd) || pPager->tempFile );
  if( 0==pagerFlushOnCommit(pPager, 1) ){
    /* If this is an in-memory db, or no pages have been written to, or this
    ** function has already been called, it is mostly a no-op.  However, any
    ** backup in progress needs to be restarted.  */
    sqlite3BackupRestart(pPager->pBackup);
  }else{
    PgHdr *pList;
    if( pagerUseWal(pPager) ){
      PgHdr *pPageOne = 0;
      pList = sqlite3PcacheDirtyList(pPager->pPCache);
      if( pList==0 ){
        /* Must have at least one page for the WAL commit flag.
        ** Ticket [2d1a5c67dfc2363e44f29d9bbd57f] 2011-05-18 */
        rc = sqlite3PagerGet(pPager, 1, &pPageOne, 0);
        pList = pPageOne;
        pList->pDirty = 0;
      }
      assert( rc==SQLITE_OK );
      if( ALWAYS(pList) ){
        rc = pagerWalFrames(pPager, pList, pPager->dbSize, 1);
      }
      sqlite3PagerUnref(pPageOne);
      if( rc==SQLITE_OK ){
        sqlite3PcacheCleanAll(pPager->pPCache);
      }
    }else{
      /* The bBatch boolean is true if the batch-atomic-write commit method
      ** should be used.  No rollback journal is created if batch-atomic-write
      ** is enabled.
      */
#ifdef SQLITE_ENABLE_BATCH_ATOMIC_WRITE
      sqlite3_file *fd = pPager->fd;
      int bBatch = zMaster==0    /* An SQLITE_IOCAP_BATCH_ATOMIC commit */
        && (sqlite3OsDeviceCharacteristics(fd) & SQLITE_IOCAP_BATCH_ATOMIC)
        && !pPager->noSync
        && sqlite3JournalIsInMemory(pPager->jfd);
#else
#     define bBatch 0
#endif

#ifdef SQLITE_ENABLE_ATOMIC_WRITE
      /* The following block updates the change-counter. Exactly how it
      ** does this depends on whether or not the atomic-update optimization
      ** was enabled at compile time, and if this transaction meets the 
      ** runtime criteria to use the operation: 
      **
      **    * The file-system supports the atomic-write property for
      **      blocks of size page-size, and 
      **    * This commit is not part of a multi-file transaction, and
      **    * Exactly one page has been modified and store in the journal file.
      **
      ** If the optimization was not enabled at compile time, then the
      ** pager_incr_changecounter() function is called to update the change
      ** counter in 'indirect-mode'. If the optimization is compiled in but
      ** is not applicable to this transaction, call sqlite3JournalCreate()
      ** to make sure the journal file has actually been created, then call
      ** pager_incr_changecounter() to update the change-counter in indirect
      ** mode. 
      **
      ** Otherwise, if the optimization is both enabled and applicable,
      ** then call pager_incr_changecounter() to update the change-counter
      ** in 'direct' mode. In this case the journal file will never be
      ** created for this transaction.
      */
      if( bBatch==0 ){
        PgHdr *pPg;
        assert( isOpen(pPager->jfd) 
            || pPager->journalMode==PAGER_JOURNALMODE_OFF 
            || pPager->journalMode==PAGER_JOURNALMODE_WAL 
            );
        if( !zMaster && isOpen(pPager->jfd) 
         && pPager->journalOff==jrnlBufferSize(pPager) 
         && pPager->dbSize>=pPager->dbOrigSize
         && (!(pPg = sqlite3PcacheDirtyList(pPager->pPCache)) || 0==pPg->pDirty)
        ){
          /* Update the db file change counter via the direct-write method. The 
          ** following call will modify the in-memory representation of page 1 
          ** to include the updated change counter and then write page 1 
          ** directly to the database file. Because of the atomic-write 
          ** property of the host file-system, this is safe.
          */
          rc = pager_incr_changecounter(pPager, 1);
        }else{
          rc = sqlite3JournalCreate(pPager->jfd);
          if( rc==SQLITE_OK ){
            rc = pager_incr_changecounter(pPager, 0);
          }
        }
      }
#else  /* SQLITE_ENABLE_ATOMIC_WRITE */
#ifdef SQLITE_ENABLE_BATCH_ATOMIC_WRITE
      if( zMaster ){
        rc = sqlite3JournalCreate(pPager->jfd);
        if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
        assert( bBatch==0 );
      }
#endif
      rc = pager_incr_changecounter(pPager, 0);
#endif /* !SQLITE_ENABLE_ATOMIC_WRITE */
      if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
  
      /* Write the master journal name into the journal file. If a master 
      ** journal file name has already been written to the journal file, 
      ** or if zMaster is NULL (no master journal), then this call is a no-op.
      */
      rc = writeMasterJournal(pPager, zMaster);
      if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
  
      /* Sync the journal file and write all dirty pages to the database.
      ** If the atomic-update optimization is being used, this sync will not 
      ** create the journal file or perform any real IO.
      **
      ** Because the change-counter page was just modified, unless the
      ** atomic-update optimization is used it is almost certain that the
      ** journal requires a sync here. However, in locking_mode=exclusive
      ** on a system under memory pressure it is just possible that this is 
      ** not the case. In this case it is likely enough that the redundant
      ** xSync() call will be changed to a no-op by the OS anyhow. 
      */
      rc = syncJournal(pPager, 0);
      if( rc!=SQLITE_OK ) goto commit_phase_one_exit;

      pList = sqlite3PcacheDirtyList(pPager->pPCache);
#ifdef SQLITE_ENABLE_BATCH_ATOMIC_WRITE
      if( bBatch ){
        rc = sqlite3OsFileControl(fd, SQLITE_FCNTL_BEGIN_ATOMIC_WRITE, 0);
        if( rc==SQLITE_OK ){
          rc = pager_write_pagelist(pPager, pList);
          if( rc==SQLITE_OK ){
            rc = sqlite3OsFileControl(fd, SQLITE_FCNTL_COMMIT_ATOMIC_WRITE, 0);
          }
          if( rc!=SQLITE_OK ){
            sqlite3OsFileControlHint(fd, SQLITE_FCNTL_ROLLBACK_ATOMIC_WRITE, 0);
          }
        }

        if( (rc&0xFF)==SQLITE_IOERR && rc!=SQLITE_IOERR_NOMEM ){
          rc = sqlite3JournalCreate(pPager->jfd);
          if( rc!=SQLITE_OK ){
            sqlite3OsClose(pPager->jfd);
            goto commit_phase_one_exit;
          }
          bBatch = 0;
        }else{
          sqlite3OsClose(pPager->jfd);
        }
      }
#endif /* SQLITE_ENABLE_BATCH_ATOMIC_WRITE */

      if( bBatch==0 ){
        rc = pager_write_pagelist(pPager, pList);
      }
      if( rc!=SQLITE_OK ){
        assert( rc!=SQLITE_IOERR_BLOCKED );
        goto commit_phase_one_exit;
      }
      sqlite3PcacheCleanAll(pPager->pPCache);

      /* If the file on disk is smaller than the database image, use 
      ** pager_truncate to grow the file here. This can happen if the database
      ** image was extended as part of the current transaction and then the
      ** last page in the db image moved to the free-list. In this case the
      ** last page is never written out to disk, leaving the database file
      ** undersized. Fix this now if it is the case.  */
      if( pPager->dbSize>pPager->dbFileSize ){
        Pgno nNew = pPager->dbSize - (pPager->dbSize==PAGER_MJ_PGNO(pPager));
        assert( pPager->eState==PAGER_WRITER_DBMOD );
        rc = pager_truncate(pPager, nNew);
        if( rc!=SQLITE_OK ) goto commit_phase_one_exit;
      }
  
      /* Finally, sync the database file. */
      if( !noSync ){
        rc = sqlite3PagerSync(pPager, zMaster);
      }
      IOTRACE(("DBSYNC %p\n", pPager))
    }
  }

commit_phase_one_exit:
  if( rc==SQLITE_OK && !pagerUseWal(pPager) ){
    pPager->eState = PAGER_WRITER_FINISHED;
  }
  return rc;
}


/*
** When this function is called, the database file has been completely
** updated to reflect the changes made by the current transaction and
** synced to disk. The journal file still exists in the file-system 
** though, and if a failure occurs at this point it will eventually
** be used as a hot-journal and the current transaction rolled back.
**
** This function finalizes the journal file, either by deleting, 
** truncating or partially zeroing it, so that it cannot be used 
** for hot-journal rollback. Once this is done the transaction is
** irrevocably committed.
**
** If an error occurs, an IO error code is returned and the pager
** moves into the error state. Otherwise, SQLITE_OK is returned.
*/
SQLITE_PRIVATE int sqlite3PagerCommitPhaseTwo(Pager *pPager){
  int rc = SQLITE_OK;                  /* Return code */

  /* This routine should not be called if a prior error has occurred.
  ** But if (due to a coding error elsewhere in the system) it does get
  ** called, just return the same error code without doing anything. */
  if( NEVER(pPager->errCode) ) return pPager->errCode;

  assert( pPager->eState==PAGER_WRITER_LOCKED
       || pPager->eState==PAGER_WRITER_FINISHED
       || (pagerUseWal(pPager) && pPager->eState==PAGER_WRITER_CACHEMOD)
  );
  assert( assert_pager_state(pPager) );

  /* An optimization. If the database was not actually modified during
  ** this transaction, the pager is running in exclusive-mode and is
  ** using persistent journals, then this function is a no-op.
  **
  ** The start of the journal file currently contains a single journal 
  ** header with the nRec field set to 0. If such a journal is used as
  ** a hot-journal during hot-journal rollback, 0 changes will be made
  ** to the database file. So there is no need to zero the journal 
  ** header. Since the pager is in exclusive mode, there is no need
  ** to drop any locks either.
  */
  if( pPager->eState==PAGER_WRITER_LOCKED 
   && pPager->exclusiveMode 
   && pPager->journalMode==PAGER_JOURNALMODE_PERSIST
  ){
    assert( pPager->journalOff==JOURNAL_HDR_SZ(pPager) || !pPager->journalOff );
    pPager->eState = PAGER_READER;
    return SQLITE_OK;
  }

  PAGERTRACE(("COMMIT %d\n", PAGERID(pPager)));
  pPager->iDataVersion++;
  rc = pager_end_transaction(pPager, pPager->setMaster, 1);
  return pager_error(pPager, rc);
}

/*
** If a write transaction is open, then all changes made within the 
** transaction are reverted and the current write-transaction is closed.
** The pager falls back to PAGER_READER state if successful, or PAGER_ERROR
** state if an error occurs.
**
** If the pager is already in PAGER_ERROR state when this function is called,
** it returns Pager.errCode immediately. No work is performed in this case.
**
** Otherwise, in rollback mode, this function performs two functions:
**
**   1) It rolls back the journal file, restoring all database file and 
**      in-memory cache pages to the state they were in when the transaction
**      was opened, and
**
**   2) It finalizes the journal file, so that it is not used for hot
**      rollback at any point in the future.
**
** Finalization of the journal file (task 2) is only performed if the 
** rollback is successful.
**
** In WAL mode, all cache-entries containing data modified within the
** current transaction are either expelled from the cache or reverted to
** their pre-transaction state by re-reading data from the database or
** WAL files. The WAL transaction is then closed.
*/
SQLITE_PRIVATE int sqlite3PagerRollback(Pager *pPager){
  int rc = SQLITE_OK;                  /* Return code */
  PAGERTRACE(("ROLLBACK %d\n", PAGERID(pPager)));

  /* PagerRollback() is a no-op if called in READER or OPEN state. If
  ** the pager is already in the ERROR state, the rollback is not 
  ** attempted here. Instead, the error code is returned to the caller.
  */
  assert( assert_pager_state(pPager) );
  if( pPager->eState==PAGER_ERROR ) return pPager->errCode;
  if( pPager->eState<=PAGER_READER ) return SQLITE_OK;

  if( pagerUseWal(pPager) ){
    int rc2;
    rc = sqlite3PagerSavepoint(pPager, SAVEPOINT_ROLLBACK, -1);
    rc2 = pager_end_transaction(pPager, pPager->setMaster, 0);
    if( rc==SQLITE_OK ) rc = rc2;
  }else if( !isOpen(pPager->jfd) || pPager->eState==PAGER_WRITER_LOCKED ){
    int eState = pPager->eState;
    rc = pager_end_transaction(pPager, 0, 0);
    if( !MEMDB && eState>PAGER_WRITER_LOCKED ){
      /* This can happen using journal_mode=off. Move the pager to the error 
      ** state to indicate that the contents of the cache may not be trusted.
      ** Any active readers will get SQLITE_ABORT.
      */
      pPager->errCode = SQLITE_ABORT;
      pPager->eState = PAGER_ERROR;
      setGetterMethod(pPager);
      return rc;
    }
  }else{
    rc = pager_playback(pPager, 0);
  }

  assert( pPager->eState==PAGER_READER || rc!=SQLITE_OK );
  assert( rc==SQLITE_OK || rc==SQLITE_FULL || rc==SQLITE_CORRUPT
          || rc==SQLITE_NOMEM || (rc&0xFF)==SQLITE_IOERR 
          || rc==SQLITE_CANTOPEN
  );

  /* If an error occurs during a ROLLBACK, we can no longer trust the pager
  ** cache. So call pager_error() on the way out to make any error persistent.
  */
  return pager_error(pPager, rc);
}

/*
** Return TRUE if the database file is opened read-only.  Return FALSE
** if the database is (in theory) writable.
*/
SQLITE_PRIVATE u8 sqlite3PagerIsreadonly(Pager *pPager){
  return pPager->readOnly;
}

#ifdef SQLITE_DEBUG
/*
** Return the sum of the reference counts for all pages held by pPager.
*/
SQLITE_PRIVATE int sqlite3PagerRefcount(Pager *pPager){
  return sqlite3PcacheRefCount(pPager->pPCache);
}
#endif

/*
** Return the approximate number of bytes of memory currently
** used by the pager and its associated cache.
*/
SQLITE_PRIVATE int sqlite3PagerMemUsed(Pager *pPager){
  int perPageSize = pPager->pageSize + pPager->nExtra + sizeof(PgHdr)
                                     + 5*sizeof(void*);
  return perPageSize*sqlite3PcachePagecount(pPager->pPCache)
           + sqlite3MallocSize(pPager)
           + pPager->pageSize;
}

/*
** Return the number of references to the specified page.
*/
SQLITE_PRIVATE int sqlite3PagerPageRefcount(DbPage *pPage){
  return sqlite3PcachePageRefcount(pPage);
}

#ifdef SQLITE_TEST
/*
** This routine is used for testing and analysis only.
*/
SQLITE_PRIVATE int *sqlite3PagerStats(Pager *pPager){
  static int a[11];
  a[0] = sqlite3PcacheRefCount(pPager->pPCache);
  a[1] = sqlite3PcachePagecount(pPager->pPCache);
  a[2] = sqlite3PcacheGetCachesize(pPager->pPCache);
  a[3] = pPager->eState==PAGER_OPEN ? -1 : (int) pPager->dbSize;
  a[4] = pPager->eState;
  a[5] = pPager->errCode;
  a[6] = pPager->aStat[PAGER_STAT_HIT];
  a[7] = pPager->aStat[PAGER_STAT_MISS];
  a[8] = 0;  /* Used to be pPager->nOvfl */
  a[9] = pPager->nRead;
  a[10] = pPager->aStat[PAGER_STAT_WRITE];
  return a;
}
#endif

/*
** Parameter eStat must be one of SQLITE_DBSTATUS_CACHE_HIT, _MISS, _WRITE,
** or _WRITE+1.  The SQLITE_DBSTATUS_CACHE_WRITE+1 case is a translation
** of SQLITE_DBSTATUS_CACHE_SPILL.  The _SPILL case is not contiguous because
** it was added later.
**
** Before returning, *pnVal is incremented by the
** current cache hit or miss count, according to the value of eStat. If the 
** reset parameter is non-zero, the cache hit or miss count is zeroed before 
** returning.
*/
SQLITE_PRIVATE void sqlite3PagerCacheStat(Pager *pPager, int eStat, int reset, int *pnVal){

  assert( eStat==SQLITE_DBSTATUS_CACHE_HIT
       || eStat==SQLITE_DBSTATUS_CACHE_MISS
       || eStat==SQLITE_DBSTATUS_CACHE_WRITE
       || eStat==SQLITE_DBSTATUS_CACHE_WRITE+1
  );

  assert( SQLITE_DBSTATUS_CACHE_HIT+1==SQLITE_DBSTATUS_CACHE_MISS );
  assert( SQLITE_DBSTATUS_CACHE_HIT+2==SQLITE_DBSTATUS_CACHE_WRITE );
  assert( PAGER_STAT_HIT==0 && PAGER_STAT_MISS==1
           && PAGER_STAT_WRITE==2 && PAGER_STAT_SPILL==3 );

  eStat -= SQLITE_DBSTATUS_CACHE_HIT;
  *pnVal += pPager->aStat[eStat];
  if( reset ){
    pPager->aStat[eStat] = 0;
  }
}

/*
** Return true if this is an in-memory or temp-file backed pager.
*/
SQLITE_PRIVATE int sqlite3PagerIsMemdb(Pager *pPager){
  return pPager->tempFile;
}

/*
** Check that there are at least nSavepoint savepoints open. If there are
** currently less than nSavepoints open, then open one or more savepoints
** to make up the difference. If the number of savepoints is already
** equal to nSavepoint, then this function is a no-op.
**
** If a memory allocation fails, SQLITE_NOMEM is returned. If an error 
** occurs while opening the sub-journal file, then an IO error code is
** returned. Otherwise, SQLITE_OK.
*/
static SQLITE_NOINLINE int pagerOpenSavepoint(Pager *pPager, int nSavepoint){
  int rc = SQLITE_OK;                       /* Return code */
  int nCurrent = pPager->nSavepoint;        /* Current number of savepoints */
  int ii;                                   /* Iterator variable */
  PagerSavepoint *aNew;                     /* New Pager.aSavepoint array */

  assert( pPager->eState>=PAGER_WRITER_LOCKED );
  assert( assert_pager_state(pPager) );
  assert( nSavepoint>nCurrent && pPager->useJournal );

  /* Grow the Pager.aSavepoint array using realloc(). Return SQLITE_NOMEM
  ** if the allocation fails. Otherwise, zero the new portion in case a 
  ** malloc failure occurs while populating it in the for(...) loop below.
  */
  aNew = (PagerSavepoint *)sqlite3Realloc(
      pPager->aSavepoint, sizeof(PagerSavepoint)*nSavepoint
  );
  if( !aNew ){
    return SQLITE_NOMEM_BKPT;
  }
  memset(&aNew[nCurrent], 0, (nSavepoint-nCurrent) * sizeof(PagerSavepoint));
  pPager->aSavepoint = aNew;

  /* Populate the PagerSavepoint structures just allocated. */
  for(ii=nCurrent; ii<nSavepoint; ii++){
    aNew[ii].nOrig = pPager->dbSize;
    if( isOpen(pPager->jfd) && pPager->journalOff>0 ){
      aNew[ii].iOffset = pPager->journalOff;
    }else{
      aNew[ii].iOffset = JOURNAL_HDR_SZ(pPager);
    }
    aNew[ii].iSubRec = pPager->nSubRec;
    aNew[ii].pInSavepoint = sqlite3BitvecCreate(pPager->dbSize);
    if( !aNew[ii].pInSavepoint ){
      return SQLITE_NOMEM_BKPT;
    }
    if( pagerUseWal(pPager) ){
      sqlite3WalSavepoint(pPager->pWal, aNew[ii].aWalData);
    }
    pPager->nSavepoint = ii+1;
  }
  assert( pPager->nSavepoint==nSavepoint );
  assertTruncateConstraint(pPager);
  return rc;
}
SQLITE_PRIVATE int sqlite3PagerOpenSavepoint(Pager *pPager, int nSavepoint){
  assert( pPager->eState>=PAGER_WRITER_LOCKED );
  assert( assert_pager_state(pPager) );

  if( nSavepoint>pPager->nSavepoint && pPager->useJournal ){
    return pagerOpenSavepoint(pPager, nSavepoint);
  }else{
    return SQLITE_OK;
  }
}


/*
** This function is called to rollback or release (commit) a savepoint.
** The savepoint to release or rollback need not be the most recently 
** created savepoint.
**
** Parameter op is always either SAVEPOINT_ROLLBACK or SAVEPOINT_RELEASE.
** If it is SAVEPOINT_RELEASE, then release and destroy the savepoint with
** index iSavepoint. If it is SAVEPOINT_ROLLBACK, then rollback all changes
** that have occurred since the specified savepoint was created.
**
** The savepoint to rollback or release is identified by parameter 
** iSavepoint. A value of 0 means to operate on the outermost savepoint
** (the first created). A value of (Pager.nSavepoint-1) means operate
** on the most recently created savepoint. If iSavepoint is greater than
** (Pager.nSavepoint-1), then this function is a no-op.
**
** If a negative value is passed to this function, then the current
** transaction is rolled back. This is different to calling 
** sqlite3PagerRollback() because this function does not terminate
** the transaction or unlock the database, it just restores the 
** contents of the database to its original state. 
**
** In any case, all savepoints with an index greater than iSavepoint 
** are destroyed. If this is a release operation (op==SAVEPOINT_RELEASE),
** then savepoint iSavepoint is also destroyed.
**
** This function may return SQLITE_NOMEM if a memory allocation fails,
** or an IO error code if an IO error occurs while rolling back a 
** savepoint. If no errors occur, SQLITE_OK is returned.
*/ 
SQLITE_PRIVATE int sqlite3PagerSavepoint(Pager *pPager, int op, int iSavepoint){
  int rc = pPager->errCode;
  
#ifdef SQLITE_ENABLE_ZIPVFS
  if( op==SAVEPOINT_RELEASE ) rc = SQLITE_OK;
#endif

  assert( op==SAVEPOINT_RELEASE || op==SAVEPOINT_ROLLBACK );
  assert( iSavepoint>=0 || op==SAVEPOINT_ROLLBACK );

  if( rc==SQLITE_OK && iSavepoint<pPager->nSavepoint ){
    int ii;            /* Iterator variable */
    int nNew;          /* Number of remaining savepoints after this op. */

    /* Figure out how many savepoints will still be active after this
    ** operation. Store this value in nNew. Then free resources associated 
    ** with any savepoints that are destroyed by this operation.
    */
    nNew = iSavepoint + (( op==SAVEPOINT_RELEASE ) ? 0 : 1);
    for(ii=nNew; ii<pPager->nSavepoint; ii++){
      sqlite3BitvecDestroy(pPager->aSavepoint[ii].pInSavepoint);
    }
    pPager->nSavepoint = nNew;

    /* If this is a release of the outermost savepoint, truncate 
    ** the sub-journal to zero bytes in size. */
    if( op==SAVEPOINT_RELEASE ){
      if( nNew==0 && isOpen(pPager->sjfd) ){
        /* Only truncate if it is an in-memory sub-journal. */
        if( sqlite3JournalIsInMemory(pPager->sjfd) ){
          rc = sqlite3OsTruncate(pPager->sjfd, 0);
          assert( rc==SQLITE_OK );
        }
        pPager->nSubRec = 0;
      }
    }
    /* Else this is a rollback operation, playback the specified savepoint.
    ** If this is a temp-file, it is possible that the journal file has
    ** not yet been opened. In this case there have been no changes to
    ** the database file, so the playback operation can be skipped.
    */
    else if( pagerUseWal(pPager) || isOpen(pPager->jfd) ){
      PagerSavepoint *pSavepoint = (nNew==0)?0:&pPager->aSavepoint[nNew-1];
      rc = pagerPlaybackSavepoint(pPager, pSavepoint);
      assert(rc!=SQLITE_DONE);
    }
    
#ifdef SQLITE_ENABLE_ZIPVFS
    /* If the cache has been modified but the savepoint cannot be rolled 
    ** back journal_mode=off, put the pager in the error state. This way,
    ** if the VFS used by this pager includes ZipVFS, the entire transaction
    ** can be rolled back at the ZipVFS level.  */
    else if( 
        pPager->journalMode==PAGER_JOURNALMODE_OFF 
     && pPager->eState>=PAGER_WRITER_CACHEMOD
    ){
      pPager->errCode = SQLITE_ABORT;
      pPager->eState = PAGER_ERROR;
      setGetterMethod(pPager);
    }
#endif
  }

  return rc;
}

/*
** Return the full pathname of the database file.
**
** Except, if the pager is in-memory only, then return an empty string if
** nullIfMemDb is true.  This routine is called with nullIfMemDb==1 when
** used to report the filename to the user, for compatibility with legacy
** behavior.  But when the Btree needs to know the filename for matching to
** shared cache, it uses nullIfMemDb==0 so that in-memory databases can
** participate in shared-cache.
*/
SQLITE_PRIVATE const char *sqlite3PagerFilename(Pager *pPager, int nullIfMemDb){
  return (nullIfMemDb && pPager->memDb) ? "" : pPager->zFilename;
}

/*
** Return the VFS structure for the pager.
*/
SQLITE_PRIVATE sqlite3_vfs *sqlite3PagerVfs(Pager *pPager){
  return pPager->pVfs;
}

/*
** Return the file handle for the database file associated
** with the pager.  This might return NULL if the file has
** not yet been opened.
*/
SQLITE_PRIVATE sqlite3_file *sqlite3PagerFile(Pager *pPager){
  return pPager->fd;
}

#ifdef SQLITE_ENABLE_SETLK_TIMEOUT
/*
** Reset the lock timeout for pager.
*/
SQLITE_PRIVATE void sqlite3PagerResetLockTimeout(Pager *pPager){
  int x = 0;
  sqlite3OsFileControl(pPager->fd, SQLITE_FCNTL_LOCK_TIMEOUT, &x);
}
#endif

/*
** Return the file handle for the journal file (if it exists).
** This will be either the rollback journal or the WAL file.
*/
SQLITE_PRIVATE sqlite3_file *sqlite3PagerJrnlFile(Pager *pPager){
#if SQLITE_OMIT_WAL
  return pPager->jfd;
#else
  return pPager->pWal ? sqlite3WalFile(pPager->pWal) : pPager->jfd;
#endif
}

/*
** Return the full pathname of the journal file.
*/
SQLITE_PRIVATE const char *sqlite3PagerJournalname(Pager *pPager){
  return pPager->zJournal;
}

#ifdef SQLITE_HAS_CODEC
/*
** Set or retrieve the codec for this pager
*/
SQLITE_PRIVATE void sqlite3PagerSetCodec(
  Pager *pPager,
  void *(*xCodec)(void*,void*,Pgno,int),
  void (*xCodecSizeChng)(void*,int,int),
  void (*xCodecFree)(void*),
  void *pCodec
){
  if( pPager->xCodecFree ){
    pPager->xCodecFree(pPager->pCodec);
  }else{
    pager_reset(pPager);
  }
  pPager->xCodec = pPager->memDb ? 0 : xCodec;
  pPager->xCodecSizeChng = xCodecSizeChng;
  pPager->xCodecFree = xCodecFree;
  pPager->pCodec = pCodec;
  setGetterMethod(pPager);
  pagerReportSize(pPager);
}
SQLITE_PRIVATE void *sqlite3PagerGetCodec(Pager *pPager){
  return pPager->pCodec;
}

/*
** This function is called by the wal module when writing page content
** into the log file.
**
** This function returns a pointer to a buffer containing the encrypted
** page content. If a malloc fails, this function may return NULL.
*/
SQLITE_PRIVATE void *sqlite3PagerCodec(PgHdr *pPg){
  void *aData = 0;
  CODEC2(pPg->pPager, pPg->pData, pPg->pgno, 6, return 0, aData);
  return aData;
}

/*
** Return the current pager state
*/
SQLITE_PRIVATE int sqlite3PagerState(Pager *pPager){
  return pPager->eState;
}
#endif /* SQLITE_HAS_CODEC */

#ifndef SQLITE_OMIT_AUTOVACUUM
/*
** Move the page pPg to location pgno in the file.
**
** There must be no references to the page previously located at
** pgno (which we call pPgOld) though that page is allowed to be
** in cache.  If the page previously located at pgno is not already
** in the rollback journal, it is not put there by by this routine.
**
** References to the page pPg remain valid. Updating any
** meta-data associated with pPg (i.e. data stored in the nExtra bytes
** allocated along with the page) is the responsibility of the caller.
**
** A transaction must be active when this routine is called. It used to be
** required that a statement transaction was not active, but this restriction
** has been removed (CREATE INDEX needs to move a page when a statement
** transaction is active).
**
** If the fourth argument, isCommit, is non-zero, then this page is being
** moved as part of a database reorganization just before the transaction 
** is being committed. In this case, it is guaranteed that the database page 
** pPg refers to will not be written to again within this transaction.
**
** This function may return SQLITE_NOMEM or an IO error code if an error
** occurs. Otherwise, it returns SQLITE_OK.
*/
SQLITE_PRIVATE int sqlite3PagerMovepage(Pager *pPager, DbPage *pPg, Pgno pgno, int isCommit){
  PgHdr *pPgOld;               /* The page being overwritten. */
  Pgno needSyncPgno = 0;       /* Old value of pPg->pgno, if sync is required */
  int rc;                      /* Return code */
  Pgno origPgno;               /* The original page number */

  assert( pPg->nRef>0 );
  assert( pPager->eState==PAGER_WRITER_CACHEMOD
       || pPager->eState==PAGER_WRITER_DBMOD
  );
  assert( assert_pager_state(pPager) );

  /* In order to be able to rollback, an in-memory database must journal
  ** the page we are moving from.
  */
  assert( pPager->tempFile || !MEMDB );
  if( pPager->tempFile ){
    rc = sqlite3PagerWrite(pPg);
    if( rc ) return rc;
  }

  /* If the page being moved is dirty and has not been saved by the latest
  ** savepoint, then save the current contents of the page into the 
  ** sub-journal now. This is required to handle the following scenario:
  **
  **   BEGIN;
  **     <journal page X, then modify it in memory>
  **     SAVEPOINT one;
  **       <Move page X to location Y>
  **     ROLLBACK TO one;
  **
  ** If page X were not written to the sub-journal here, it would not
  ** be possible to restore its contents when the "ROLLBACK TO one"
  ** statement were is processed.
  **
  ** subjournalPage() may need to allocate space to store pPg->pgno into
  ** one or more savepoint bitvecs. This is the reason this function
  ** may return SQLITE_NOMEM.
  */
  if( (pPg->flags & PGHDR_DIRTY)!=0
   && SQLITE_OK!=(rc = subjournalPageIfRequired(pPg))
  ){
    return rc;
  }

  PAGERTRACE(("MOVE %d page %d (needSync=%d) moves to %d\n", 
      PAGERID(pPager), pPg->pgno, (pPg->flags&PGHDR_NEED_SYNC)?1:0, pgno));
  IOTRACE(("MOVE %p %d %d\n", pPager, pPg->pgno, pgno))

  /* If the journal needs to be sync()ed before page pPg->pgno can
  ** be written to, store pPg->pgno in local variable needSyncPgno.
  **
  ** If the isCommit flag is set, there is no need to remember that
  ** the journal needs to be sync()ed before database page pPg->pgno 
  ** can be written to. The caller has already promised not to write to it.
  */
  if( (pPg->flags&PGHDR_NEED_SYNC) && !isCommit ){
    needSyncPgno = pPg->pgno;
    assert( pPager->journalMode==PAGER_JOURNALMODE_OFF ||
            pageInJournal(pPager, pPg) || pPg->pgno>pPager->dbOrigSize );
    assert( pPg->flags&PGHDR_DIRTY );
  }

  /* If the cache contains a page with page-number pgno, remove it
  ** from its hash chain. Also, if the PGHDR_NEED_SYNC flag was set for 
  ** page pgno before the 'move' operation, it needs to be retained 
  ** for the page moved there.
  */
  pPg->flags &= ~PGHDR_NEED_SYNC;
  pPgOld = sqlite3PagerLookup(pPager, pgno);
  assert( !pPgOld || pPgOld->nRef==1 || CORRUPT_DB );
  if( pPgOld ){
    if( pPgOld->nRef>1 ){
      sqlite3PagerUnrefNotNull(pPgOld);
      return SQLITE_CORRUPT_BKPT;
    }
    pPg->flags |= (pPgOld->flags&PGHDR_NEED_SYNC);
    if( pPager->tempFile ){
      /* Do not discard pages from an in-memory database since we might
      ** need to rollback later.  Just move the page out of the way. */
      sqlite3PcacheMove(pPgOld, pPager->dbSize+1);
    }else{
      sqlite3PcacheDrop(pPgOld);
    }
  }

  origPgno = pPg->pgno;
  sqlite3PcacheMove(pPg, pgno);
  sqlite3PcacheMakeDirty(pPg);

  /* For an in-memory database, make sure the original page continues
  ** to exist, in case the transaction needs to roll back.  Use pPgOld
  ** as the original page since it has already been allocated.
  */
  if( pPager->tempFile && pPgOld ){
    sqlite3PcacheMove(pPgOld, origPgno);
    sqlite3PagerUnrefNotNull(pPgOld);
  }

  if( needSyncPgno ){
    /* If needSyncPgno is non-zero, then the journal file needs to be 
    ** sync()ed before any data is written to database file page needSyncPgno.
    ** Currently, no such page exists in the page-cache and the 
    ** "is journaled" bitvec flag has been set. This needs to be remedied by
    ** loading the page into the pager-cache and setting the PGHDR_NEED_SYNC
    ** flag.
    **
    ** If the attempt to load the page into the page-cache fails, (due
    ** to a malloc() or IO failure), clear the bit in the pInJournal[]
    ** array. Otherwise, if the page is loaded and written again in
    ** this transaction, it may be written to the database file before
    ** it is synced into the journal file. This way, it may end up in
    ** the journal file twice, but that is not a problem.
    */
    PgHdr *pPgHdr;
    rc = sqlite3PagerGet(pPager, needSyncPgno, &pPgHdr, 0);
    if( rc!=SQLITE_OK ){
      if( needSyncPgno<=pPager->dbOrigSize ){
        assert( pPager->pTmpSpace!=0 );
        sqlite3BitvecClear(pPager->pInJournal, needSyncPgno, pPager->pTmpSpace);
      }
      return rc;
    }
    pPgHdr->flags |= PGHDR_NEED_SYNC;
    sqlite3PcacheMakeDirty(pPgHdr);
    sqlite3PagerUnrefNotNull(pPgHdr);
  }

  return SQLITE_OK;
}
#endif

/*
** The page handle passed as the first argument refers to a dirty page 
** with a page number other than iNew. This function changes the page's 
** page number to iNew and sets the value of the PgHdr.flags field to 
** the value passed as the third parameter.
*/
SQLITE_PRIVATE void sqlite3PagerRekey(DbPage *pPg, Pgno iNew, u16 flags){
  assert( pPg->pgno!=iNew );
  pPg->flags = flags;
  sqlite3PcacheMove(pPg, iNew);
}

/*
** Return a pointer to the data for the specified page.
*/
SQLITE_PRIVATE void *sqlite3PagerGetData(DbPage *pPg){
  assert( pPg->nRef>0 || pPg->pPager->memDb );
  return pPg->pData;
}

/*
** Return a pointer to the Pager.nExtra bytes of "extra" space 
** allocated along with the specified page.
*/
SQLITE_PRIVATE void *sqlite3PagerGetExtra(DbPage *pPg){
  return pPg->pExtra;
}

/*
** Get/set the locking-mode for this pager. Parameter eMode must be one
** of PAGER_LOCKINGMODE_QUERY, PAGER_LOCKINGMODE_NORMAL or 
** PAGER_LOCKINGMODE_EXCLUSIVE. If the parameter is not _QUERY, then
** the locking-mode is set to the value specified.
**
** The returned value is either PAGER_LOCKINGMODE_NORMAL or
** PAGER_LOCKINGMODE_EXCLUSIVE, indicating the current (possibly updated)
** locking-mode.
*/
SQLITE_PRIVATE int sqlite3PagerLockingMode(Pager *pPager, int eMode){
  assert( eMode==PAGER_LOCKINGMODE_QUERY
            || eMode==PAGER_LOCKINGMODE_NORMAL
            || eMode==PAGER_LOCKINGMODE_EXCLUSIVE );
  assert( PAGER_LOCKINGMODE_QUERY<0 );
  assert( PAGER_LOCKINGMODE_NORMAL>=0 && PAGER_LOCKINGMODE_EXCLUSIVE>=0 );
  assert( pPager->exclusiveMode || 0==sqlite3WalHeapMemory(pPager->pWal) );
  if( eMode>=0 && !pPager->tempFile && !sqlite3WalHeapMemory(pPager->pWal) ){
    pPager->exclusiveMode = (u8)eMode;
  }
  return (int)pPager->exclusiveMode;
}

/*
** Set the journal-mode for this pager. Parameter eMode must be one of:
**
**    PAGER_JOURNALMODE_DELETE
**    PAGER_JOURNALMODE_TRUNCATE
**    PAGER_JOURNALMODE_PERSIST
**    PAGER_JOURNALMODE_OFF
**    PAGER_JOURNALMODE_MEMORY
**    PAGER_JOURNALMODE_WAL
**
** The journalmode is set to the value specified if the change is allowed.
** The change may be disallowed for the following reasons:
**
**   *  An in-memory database can only have its journal_mode set to _OFF
**      or _MEMORY.
**
**   *  Temporary databases cannot have _WAL journalmode.
**
** The returned indicate the current (possibly updated) journal-mode.
*/
SQLITE_PRIVATE int sqlite3PagerSetJournalMode(Pager *pPager, int eMode){
  u8 eOld = pPager->journalMode;    /* Prior journalmode */

  /* The eMode parameter is always valid */
  assert(      eMode==PAGER_JOURNALMODE_DELETE
            || eMode==PAGER_JOURNALMODE_TRUNCATE
            || eMode==PAGER_JOURNALMODE_PERSIST
            || eMode==PAGER_JOURNALMODE_OFF 
            || eMode==PAGER_JOURNALMODE_WAL 
            || eMode==PAGER_JOURNALMODE_MEMORY );

  /* This routine is only called from the OP_JournalMode opcode, and
  ** the logic there will never allow a temporary file to be changed
  ** to WAL mode.
  */
  assert( pPager->tempFile==0 || eMode!=PAGER_JOURNALMODE_WAL );

  /* Do allow the journalmode of an in-memory database to be set to
  ** anything other than MEMORY or OFF
  */
  if( MEMDB ){
    assert( eOld==PAGER_JOURNALMODE_MEMORY || eOld==PAGER_JOURNALMODE_OFF );
    if( eMode!=PAGER_JOURNALMODE_MEMORY && eMode!=PAGER_JOURNALMODE_OFF ){
      eMode = eOld;
    }
  }

  if( eMode!=eOld ){

    /* Change the journal mode. */
    assert( pPager->eState!=PAGER_ERROR );
    pPager->journalMode = (u8)eMode;

    /* When transistioning from TRUNCATE or PERSIST to any other journal
    ** mode except WAL, unless the pager is in locking_mode=exclusive mode,
    ** delete the journal file.
    */
    assert( (PAGER_JOURNALMODE_TRUNCATE & 5)==1 );
    assert( (PAGER_JOURNALMODE_PERSIST & 5)==1 );
    assert( (PAGER_JOURNALMODE_DELETE & 5)==0 );
    assert( (PAGER_JOURNALMODE_MEMORY & 5)==4 );
    assert( (PAGER_JOURNALMODE_OFF & 5)==0 );
    assert( (PAGER_JOURNALMODE_WAL & 5)==5 );

    assert( isOpen(pPager->fd) || pPager->exclusiveMode );
    if( !pPager->exclusiveMode && (eOld & 5)==1 && (eMode & 1)==0 ){

      /* In this case we would like to delete the journal file. If it is
      ** not possible, then that is not a problem. Deleting the journal file
      ** here is an optimization only.
      **
      ** Before deleting the journal file, obtain a RESERVED lock on the
      ** database file. This ensures that the journal file is not deleted
      ** while it is in use by some other client.
      */
      sqlite3OsClose(pPager->jfd);
      if( pPager->eLock>=RESERVED_LOCK ){
        sqlite3OsDelete(pPager->pVfs, pPager->zJournal, 0);
      }else{
        int rc = SQLITE_OK;
        int state = pPager->eState;
        assert( state==PAGER_OPEN || state==PAGER_READER );
        if( state==PAGER_OPEN ){
          rc = sqlite3PagerSharedLock(pPager);
        }
        if( pPager->eState==PAGER_READER ){
          assert( rc==SQLITE_OK );
          rc = pagerLockDb(pPager, RESERVED_LOCK);
        }
        if( rc==SQLITE_OK ){
          sqlite3OsDelete(pPager->pVfs, pPager->zJournal, 0);
        }
        if( rc==SQLITE_OK && state==PAGER_READER ){
          pagerUnlockDb(pPager, SHARED_LOCK);
        }else if( state==PAGER_OPEN ){
          pager_unlock(pPager);
        }
        assert( state==pPager->eState );
      }
    }else if( eMode==PAGER_JOURNALMODE_OFF ){
      sqlite3OsClose(pPager->jfd);
    }
  }

  /* Return the new journal mode */
  return (int)pPager->journalMode;
}

/*
** Return the current journal mode.
*/
SQLITE_PRIVATE int sqlite3PagerGetJournalMode(Pager *pPager){
  return (int)pPager->journalMode;
}

/*
** Return TRUE if the pager is in a state where it is OK to change the
** journalmode.  Journalmode changes can only happen when the database
** is unmodified.
*/
SQLITE_PRIVATE int sqlite3PagerOkToChangeJournalMode(Pager *pPager){
  assert( assert_pager_state(pPager) );
  if( pPager->eState>=PAGER_WRITER_CACHEMOD ) return 0;
  if( NEVER(isOpen(pPager->jfd) && pPager->journalOff>0) ) return 0;
  return 1;
}

/*
** Get/set the size-limit used for persistent journal files.
**
** Setting the size limit to -1 means no limit is enforced.
** An attempt to set a limit smaller than -1 is a no-op.
*/
SQLITE_PRIVATE i64 sqlite3PagerJournalSizeLimit(Pager *pPager, i64 iLimit){
  if( iLimit>=-1 ){
    pPager->journalSizeLimit = iLimit;
    sqlite3WalLimit(pPager->pWal, iLimit);
  }
  return pPager->journalSizeLimit;
}

/*
** Return a pointer to the pPager->pBackup variable. The backup module
** in backup.c maintains the content of this variable. This module
** uses it opaquely as an argument to sqlite3BackupRestart() and
** sqlite3BackupUpdate() only.
*/
SQLITE_PRIVATE sqlite3_backup **sqlite3PagerBackupPtr(Pager *pPager){
  return &pPager->pBackup;
}

#ifndef SQLITE_OMIT_VACUUM
/*
** Unless this is an in-memory or temporary database, clear the pager cache.
*/
SQLITE_PRIVATE void sqlite3PagerClearCache(Pager *pPager){
  assert( MEMDB==0 || pPager->tempFile );
  if( pPager->tempFile==0 ) pager_reset(pPager);
}
#endif


#ifndef SQLITE_OMIT_WAL
/*
** This function is called when the user invokes "PRAGMA wal_checkpoint",
** "PRAGMA wal_blocking_checkpoint" or calls the sqlite3_wal_checkpoint()
** or wal_blocking_checkpoint() API functions.
**
** Parameter eMode is one of SQLITE_CHECKPOINT_PASSIVE, FULL or RESTART.
*/
SQLITE_PRIVATE int sqlite3PagerCheckpoint(
  Pager *pPager,                  /* Checkpoint on this pager */
  sqlite3 *db,                    /* Db handle used to check for interrupts */
  int eMode,                      /* Type of checkpoint */
  int *pnLog,                     /* OUT: Final number of frames in log */
  int *pnCkpt                     /* OUT: Final number of checkpointed frames */
){
  int rc = SQLITE_OK;
  if( pPager->pWal ){
    rc = sqlite3WalCheckpoint(pPager->pWal, db, eMode,
        (eMode==SQLITE_CHECKPOINT_PASSIVE ? 0 : pPager->xBusyHandler),
        pPager->pBusyHandlerArg,
        pPager->walSyncFlags, pPager->pageSize, (u8 *)pPager->pTmpSpace,
        pnLog, pnCkpt
    );
    sqlite3PagerResetLockTimeout(pPager);
  }
  return rc;
}

SQLITE_PRIVATE int sqlite3PagerWalCallback(Pager *pPager){
  return sqlite3WalCallback(pPager->pWal);
}

/*
** Return true if the underlying VFS for the given pager supports the
** primitives necessary for write-ahead logging.
*/
SQLITE_PRIVATE int sqlite3PagerWalSupported(Pager *pPager){
  const sqlite3_io_methods *pMethods = pPager->fd->pMethods;
  if( pPager->noLock ) return 0;
  return pPager->exclusiveMode || (pMethods->iVersion>=2 && pMethods->xShmMap);
}

/*
** Attempt to take an exclusive lock on the database file. If a PENDING lock
** is obtained instead, immediately release it.
*/
static int pagerExclusiveLock(Pager *pPager){
  int rc;                         /* Return code */

  assert( pPager->eLock==SHARED_LOCK || pPager->eLock==EXCLUSIVE_LOCK );
  rc = pagerLockDb(pPager, EXCLUSIVE_LOCK);
  if( rc!=SQLITE_OK ){
    /* If the attempt to grab the exclusive lock failed, release the 
    ** pending lock that may have been obtained instead.  */
    pagerUnlockDb(pPager, SHARED_LOCK);
  }

  return rc;
}

/*
** Call sqlite3WalOpen() to open the WAL handle. If the pager is in 
** exclusive-locking mode when this function is called, take an EXCLUSIVE
** lock on the database file and use heap-memory to store the wal-index
** in. Otherwise, use the normal shared-memory.
*/
static int pagerOpenWal(Pager *pPager){
  int rc = SQLITE_OK;

  assert( pPager->pWal==0 && pPager->tempFile==0 );
  assert( pPager->eLock==SHARED_LOCK || pPager->eLock==EXCLUSIVE_LOCK );

  /* If the pager is already in exclusive-mode, the WAL module will use 
  ** heap-memory for the wal-index instead of the VFS shared-memory 
  ** implementation. Take the exclusive lock now, before opening the WAL
  ** file, to make sure this is safe.
  */
  if( pPager->exclusiveMode ){
    rc = pagerExclusiveLock(pPager);
  }

  /* Open the connection to the log file. If this operation fails, 
  ** (e.g. due to malloc() failure), return an error code.
  */
  if( rc==SQLITE_OK ){
    rc = sqlite3WalOpen(pPager->pVfs,
        pPager->fd, pPager->zWal, pPager->exclusiveMode,
        pPager->journalSizeLimit, &pPager->pWal
    );
  }
  pagerFixMaplimit(pPager);

  return rc;
}


/*
** The caller must be holding a SHARED lock on the database file to call
** this function.
**
** If the pager passed as the first argument is open on a real database
** file (not a temp file or an in-memory database), and the WAL file
** is not already open, make an attempt to open it now. If successful,
** return SQLITE_OK. If an error occurs or the VFS used by the pager does 
** not support the xShmXXX() methods, return an error code. *pbOpen is
** not modified in either case.
**
** If the pager is open on a temp-file (or in-memory database), or if
** the WAL file is already open, set *pbOpen to 1 and return SQLITE_OK
** without doing anything.
*/
SQLITE_PRIVATE int sqlite3PagerOpenWal(
  Pager *pPager,                  /* Pager object */
  int *pbOpen                     /* OUT: Set to true if call is a no-op */
){
  int rc = SQLITE_OK;             /* Return code */

  assert( assert_pager_state(pPager) );
  assert( pPager->eState==PAGER_OPEN   || pbOpen );
  assert( pPager->eState==PAGER_READER || !pbOpen );
  assert( pbOpen==0 || *pbOpen==0 );
  assert( pbOpen!=0 || (!pPager->tempFile && !pPager->pWal) );

  if( !pPager->tempFile && !pPager->pWal ){
    if( !sqlite3PagerWalSupported(pPager) ) return SQLITE_CANTOPEN;

    /* Close any rollback journal previously open */
    sqlite3OsClose(pPager->jfd);

    rc = pagerOpenWal(pPager);
    if( rc==SQLITE_OK ){
      pPager->journalMode = PAGER_JOURNALMODE_WAL;
      pPager->eState = PAGER_OPEN;
    }
  }else{
    *pbOpen = 1;
  }

  return rc;
}

/*
** This function is called to close the connection to the log file prior
** to switching from WAL to rollback mode.
**
** Before closing the log file, this function attempts to take an 
** EXCLUSIVE lock on the database file. If this cannot be obtained, an
** error (SQLITE_BUSY) is returned and the log connection is not closed.
** If successful, the EXCLUSIVE lock is not released before returning.
*/
SQLITE_PRIVATE int sqlite3PagerCloseWal(Pager *pPager, sqlite3 *db){
  int rc = SQLITE_OK;

  assert( pPager->journalMode==PAGER_JOURNALMODE_WAL );

  /* If the log file is not already open, but does exist in the file-system,
  ** it may need to be checkpointed before the connection can switch to
  ** rollback mode. Open it now so this can happen.
  */
  if( !pPager->pWal ){
    int logexists = 0;
    rc = pagerLockDb(pPager, SHARED_LOCK);
    if( rc==SQLITE_OK ){
      rc = sqlite3OsAccess(
          pPager->pVfs, pPager->zWal, SQLITE_ACCESS_EXISTS, &logexists
      );
    }
    if( rc==SQLITE_OK && logexists ){
      rc = pagerOpenWal(pPager);
    }
  }
    
  /* Checkpoint and close the log. Because an EXCLUSIVE lock is held on
  ** the database file, the log and log-summary files will be deleted.
  */
  if( rc==SQLITE_OK && pPager->pWal ){
    rc = pagerExclusiveLock(pPager);
    if( rc==SQLITE_OK ){
      rc = sqlite3WalClose(pPager->pWal, db, pPager->walSyncFlags,
                           pPager->pageSize, (u8*)pPager->pTmpSpace);
      pPager->pWal = 0;
      pagerFixMaplimit(pPager);
      if( rc && !pPager->exclusiveMode ) pagerUnlockDb(pPager, SHARED_LOCK);
    }
  }
  return rc;
}

#ifdef SQLITE_ENABLE_SNAPSHOT
/*
** If this is a WAL database, obtain a snapshot handle for the snapshot
** currently open. Otherwise, return an error.
*/
SQLITE_PRIVATE int sqlite3PagerSnapshotGet(Pager *pPager, sqlite3_snapshot **ppSnapshot){
  int rc = SQLITE_ERROR;
  if( pPager->pWal ){
    rc = sqlite3WalSnapshotGet(pPager->pWal, ppSnapshot);
  }
  return rc;
}

/*
** If this is a WAL database, store a pointer to pSnapshot. Next time a
** read transaction is opened, attempt to read from the snapshot it 
** identifies. If this is not a WAL database, return an error.
*/
SQLITE_PRIVATE int sqlite3PagerSnapshotOpen(Pager *pPager, sqlite3_snapshot *pSnapshot){
  int rc = SQLITE_OK;
  if( pPager->pWal ){
    sqlite3WalSnapshotOpen(pPager->pWal, pSnapshot);
  }else{
    rc = SQLITE_ERROR;
  }
  return rc;
}

/*
** If this is a WAL database, call sqlite3WalSnapshotRecover(). If this 
** is not a WAL database, return an error.
*/
SQLITE_PRIVATE int sqlite3PagerSnapshotRecover(Pager *pPager){
  int rc;
  if( pPager->pWal ){
    rc = sqlite3WalSnapshotRecover(pPager->pWal);
  }else{
    rc = SQLITE_ERROR;
  }
  return rc;
}

/*
** The caller currently has a read transaction open on the database.
** If this is not a WAL database, SQLITE_ERROR is returned. Otherwise,
** this function takes a SHARED lock on the CHECKPOINTER slot and then
** checks if the snapshot passed as the second argument is still 
** available. If so, SQLITE_OK is returned.
**
** If the snapshot is not available, SQLITE_ERROR is returned. Or, if
** the CHECKPOINTER lock cannot be obtained, SQLITE_BUSY. If any error
** occurs (any value other than SQLITE_OK is returned), the CHECKPOINTER
** lock is released before returning.
*/
SQLITE_PRIVATE int sqlite3PagerSnapshotCheck(Pager *pPager, sqlite3_snapshot *pSnapshot){
  int rc;
  if( pPager->pWal ){
    rc = sqlite3WalSnapshotCheck(pPager->pWal, pSnapshot);
  }else{
    rc = SQLITE_ERROR;
  }
  return rc;
}

/*
** Release a lock obtained by an earlier successful call to
** sqlite3PagerSnapshotCheck().
*/
SQLITE_PRIVATE void sqlite3PagerSnapshotUnlock(Pager *pPager){
  assert( pPager->pWal );
  sqlite3WalSnapshotUnlock(pPager->pWal);
}

#endif /* SQLITE_ENABLE_SNAPSHOT */
#endif /* !SQLITE_OMIT_WAL */

#ifdef SQLITE_ENABLE_ZIPVFS
/*
** A read-lock must be held on the pager when this function is called. If
** the pager is in WAL mode and the WAL file currently contains one or more
** frames, return the size in bytes of the page images stored within the
** WAL frames. Otherwise, if this is not a WAL database or the WAL file
** is empty, return 0.
*/
SQLITE_PRIVATE int sqlite3PagerWalFramesize(Pager *pPager){
  assert( pPager->eState>=PAGER_READER );
  return sqlite3WalFramesize(pPager->pWal);
}
#endif

#endif /* SQLITE_OMIT_DISKIO */

/************** End of pager.c ***********************************************/
/************** Begin file wal.c *********************************************/
/*
** 2010 February 1
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
** This file contains the implementation of a write-ahead log (WAL) used in 
** "journal_mode=WAL" mode.
**
** WRITE-AHEAD LOG (WAL) FILE FORMAT
**
** A WAL file consists of a header followed by zero or more "frames".
** Each frame records the revised content of a single page from the
** database file.  All changes to the database are recorded by writing
** frames into the WAL.  Transactions commit when a frame is written that
** contains a commit marker.  A single WAL can and usually does record 
** multiple transactions.  Periodically, the content of the WAL is
** transferred back into the database file in an operation called a
** "checkpoint".
**
** A single WAL file can be used multiple times.  In other words, the
** WAL can fill up with frames and then be checkpointed and then new
** frames can overwrite the old ones.  A WAL always grows from beginning
** toward the end.  Checksums and counters attached to each frame are
** used to determine which frames within the WAL are valid and which
** are leftovers from prior checkpoints.
**
** The WAL header is 32 bytes in size and consists of the following eight
** big-endian 32-bit unsigned integer values:
**
**     0: Magic number.  0x377f0682 or 0x377f0683
**     4: File format version.  Currently 3007000
**     8: Database page size.  Example: 1024
**    12: Checkpoint sequence number
**    16: Salt-1, random integer incremented with each checkpoint
**    20: Salt-2, a different random integer changing with each ckpt
**    24: Checksum-1 (first part of checksum for first 24 bytes of header).
**    28: Checksum-2 (second part of checksum for first 24 bytes of header).
**
** Immediately following the wal-header are zero or more frames. Each
** frame consists of a 24-byte frame-header followed by a <page-size> bytes
** of page data. The frame-header is six big-endian 32-bit unsigned 
** integer values, as follows:
**
**     0: Page number.
**     4: For commit records, the size of the database image in pages 
**        after the commit. For all other records, zero.
**     8: Salt-1 (copied from the header)
**    12: Salt-2 (copied from the header)
**    16: Checksum-1.
**    20: Checksum-2.
**
** A frame is considered valid if and only if the following conditions are
** true:
**
**    (1) The salt-1 and salt-2 values in the frame-header match
**        salt values in the wal-header
**
**    (2) The checksum values in the final 8 bytes of the frame-header
**        exactly match the checksum computed consecutively on the
**        WAL header and the first 8 bytes and the content of all frames
**        up to and including the current frame.
**
** The checksum is computed using 32-bit big-endian integers if the
** magic number in the first 4 bytes of the WAL is 0x377f0683 and it
** is computed using little-endian if the magic number is 0x377f0682.
** The checksum values are always stored in the frame header in a
** big-endian format regardless of which byte order is used to compute
** the checksum.  The checksum is computed by interpreting the input as
** an even number of unsigned 32-bit integers: x[0] through x[N].  The
** algorithm used for the checksum is as follows:
** 
**   for i from 0 to n-1 step 2:
**     s0 += x[i] + s1;
**     s1 += x[i+1] + s0;
**   endfor
**
** Note that s0 and s1 are both weighted checksums using fibonacci weights
** in reverse order (the largest fibonacci weight occurs on the first element
** of the sequence being summed.)  The s1 value spans all 32-bit 
** terms of the sequence whereas s0 omits the final term.
**
** On a checkpoint, the WAL is first VFS.xSync-ed, then valid content of the
** WAL is transferred into the database, then the database is VFS.xSync-ed.
** The VFS.xSync operations serve as write barriers - all writes launched
** before the xSync must complete before any write that launches after the
** xSync begins.
**
** After each checkpoint, the salt-1 value is incremented and the salt-2
** value is randomized.  This prevents old and new frames in the WAL from
** being considered valid at the same time and being checkpointing together
** following a crash.
**
** READER ALGORITHM
**
** To read a page from the database (call it page number P), a reader
** first checks the WAL to see if it contains page P.  If so, then the
** last valid instance of page P that is a followed by a commit frame
** or is a commit frame itself becomes the value read.  If the WAL
** contains no copies of page P that are valid and which are a commit
** frame or are followed by a commit frame, then page P is read from
** the database file.
**
** To start a read transaction, the reader records the index of the last
** valid frame in the WAL.  The reader uses this recorded "mxFrame" value
** for all subsequent read operations.  New transactions can be appended
** to the WAL, but as long as the reader uses its original mxFrame value
** and ignores the newly appended content, it will see a consistent snapshot
** of the database from a single point in time.  This technique allows
** multiple concurrent readers to view different versions of the database
** content simultaneously.
**
** The reader algorithm in the previous paragraphs works correctly, but 
** because frames for page P can appear anywhere within the WAL, the
** reader has to scan the entire WAL looking for page P frames.  If the
** WAL is large (multiple megabytes is typical) that scan can be slow,
** and read performance suffers.  To overcome this problem, a separate
** data structure called the wal-index is maintained to expedite the
** search for frames of a particular page.
** 
** WAL-INDEX FORMAT
**
** Conceptually, the wal-index is shared memory, though VFS implementations
** might choose to implement the wal-index using a mmapped file.  Because
** the wal-index is shared memory, SQLite does not support journal_mode=WAL 
** on a network filesystem.  All users of the database must be able to
** share memory.
**
** In the default unix and windows implementation, the wal-index is a mmapped
** file whose name is the database name with a "-shm" suffix added.  For that
** reason, the wal-index is sometimes called the "shm" file.
**
** The wal-index is transient.  After a crash, the wal-index can (and should
** be) reconstructed from the original WAL file.  In fact, the VFS is required
** to either truncate or zero the header of the wal-index when the last
** connection to it closes.  Because the wal-index is transient, it can
** use an architecture-specific format; it does not have to be cross-platform.
** Hence, unlike the database and WAL file formats which store all values
** as big endian, the wal-index can store multi-byte values in the native
** byte order of the host computer.
**
** The purpose of the wal-index is to answer this question quickly:  Given
** a page number P and a maximum frame index M, return the index of the 
** last frame in the wal before frame M for page P in the WAL, or return
** NULL if there are no frames for page P in the WAL prior to M.
**
** The wal-index consists of a header region, followed by an one or
** more index blocks.  
**
** The wal-index header contains the total number of frames within the WAL
** in the mxFrame field.
**
** Each index block except for the first contains information on 
** HASHTABLE_NPAGE frames. The first index block contains information on
** HASHTABLE_NPAGE_ONE frames. The values of HASHTABLE_NPAGE_ONE and 
** HASHTABLE_NPAGE are selected so that together the wal-index header and
** first index block are the same size as all other index blocks in the
** wal-index.
**
** Each index block contains two sections, a page-mapping that contains the
** database page number associated with each wal frame, and a hash-table 
** that allows readers to query an index block for a specific page number.
** The page-mapping is an array of HASHTABLE_NPAGE (or HASHTABLE_NPAGE_ONE
** for the first index block) 32-bit page numbers. The first entry in the 
** first index-block contains the database page number corresponding to the
** first frame in the WAL file. The first entry in the second index block
** in the WAL file corresponds to the (HASHTABLE_NPAGE_ONE+1)th frame in
** the log, and so on.
**
** The last index block in a wal-index usually contains less than the full
** complement of HASHTABLE_NPAGE (or HASHTABLE_NPAGE_ONE) page-numbers,
** depending on the contents of the WAL file. This does not change the
** allocated size of the page-mapping array - the page-mapping array merely
** contains unused entries.
**
** Even without using the hash table, the last frame for page P
** can be found by scanning the page-mapping sections of each index block
** starting with the last index block and moving toward the first, and
** within each index block, starting at the end and moving toward the
** beginning.  The first entry that equals P corresponds to the frame
** holding the content for that page.
**
** The hash table consists of HASHTABLE_NSLOT 16-bit unsigned integers.
** HASHTABLE_NSLOT = 2*HASHTABLE_NPAGE, and there is one entry in the
** hash table for each page number in the mapping section, so the hash 
** table is never more than half full.  The expected number of collisions 
** prior to finding a match is 1.  Each entry of the hash table is an
** 1-based index of an entry in the mapping section of the same
** index block.   Let K be the 1-based index of the largest entry in
** the mapping section.  (For index blocks other than the last, K will
** always be exactly HASHTABLE_NPAGE (4096) and for the last index block
** K will be (mxFrame%HASHTABLE_NPAGE).)  Unused slots of the hash table
** contain a value of 0.
**
** To look for page P in the hash table, first compute a hash iKey on
** P as follows:
**
**      iKey = (P * 383) % HASHTABLE_NSLOT
**
** Then start scanning entries of the hash table, starting with iKey
** (wrapping around to the beginning when the end of the hash table is
** reached) until an unused hash slot is found. Let the first unused slot
** be at index iUnused.  (iUnused might be less than iKey if there was
** wrap-around.) Because the hash table is never more than half full,
** the search is guaranteed to eventually hit an unused entry.  Let 
** iMax be the value between iKey and iUnused, closest to iUnused,
** where aHash[iMax]==P.  If there is no iMax entry (if there exists
** no hash slot such that aHash[i]==p) then page P is not in the
** current index block.  Otherwise the iMax-th mapping entry of the
** current index block corresponds to the last entry that references 
** page P.
**
** A hash search begins with the last index block and moves toward the
** first index block, looking for entries corresponding to page P.  On
** average, only two or three slots in each index block need to be
** examined in order to either find the last entry for page P, or to
** establish that no such entry exists in the block.  Each index block
** holds over 4000 entries.  So two or three index blocks are sufficient
** to cover a typical 10 megabyte WAL file, assuming 1K pages.  8 or 10
** comparisons (on average) suffice to either locate a frame in the
** WAL or to establish that the frame does not exist in the WAL.  This
** is much faster than scanning the entire 10MB WAL.
**
** Note that entries are added in order of increasing K.  Hence, one
** reader might be using some value K0 and a second reader that started
** at a later time (after additional transactions were added to the WAL
** and to the wal-index) might be using a different value K1, where K1>K0.
** Both readers can use the same hash table and mapping section to get
** the correct result.  There may be entries in the hash table with
** K>K0 but to the first reader, those entries will appear to be unused
** slots in the hash table and so the first reader will get an answer as
** if no values greater than K0 had ever been inserted into the hash table
** in the first place - which is what reader one wants.  Meanwhile, the
** second reader using K1 will see additional values that were inserted
** later, which is exactly what reader two wants.  
**
** When a rollback occurs, the value of K is decreased. Hash table entries
** that correspond to frames greater than the new K value are removed
** from the hash table at this point.
*/
#ifndef SQLITE_OMIT_WAL

/* #include "wal.h" */

/*
** Trace output macros
*/
#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)
SQLITE_PRIVATE int sqlite3WalTrace = 0;
# define WALTRACE(X)  if(sqlite3WalTrace) sqlite3DebugPrintf X
#else
# define WALTRACE(X)
#endif

/*
** WAL mode depends on atomic aligned 32-bit loads and stores in a few
** places.  The following macros try to make this explicit.
*/
#if GCC_VESRION>=5004000
# define AtomicLoad(PTR)       __atomic_load_n((PTR),__ATOMIC_RELAXED)
# define AtomicStore(PTR,VAL)  __atomic_store_n((PTR),(VAL),__ATOMIC_RELAXED)
#else
# define AtomicLoad(PTR)       (*(PTR))
# define AtomicStore(PTR,VAL)  (*(PTR) = (VAL))
#endif

/*
** The maximum (and only) versions of the wal and wal-index formats
** that may be interpreted by this version of SQLite.
**
** If a client begins recovering a WAL file and finds that (a) the checksum
** values in the wal-header are correct and (b) the version field is not
** WAL_MAX_VERSION, recovery fails and SQLite returns SQLITE_CANTOPEN.
**
** Similarly, if a client successfully reads a wal-index header (i.e. the 
** checksum test is successful) and finds that the version field is not
** WALINDEX_MAX_VERSION, then no read-transaction is opened and SQLite
** returns SQLITE_CANTOPEN.
*/
#define WAL_MAX_VERSION      3007000
#define WALINDEX_MAX_VERSION 3007000

/*
** Index numbers for various locking bytes.   WAL_NREADER is the number
** of available reader locks and should be at least 3.  The default
** is SQLITE_SHM_NLOCK==8 and  WAL_NREADER==5.
**
** Technically, the various VFSes are free to implement these locks however
** they see fit.  However, compatibility is encouraged so that VFSes can
** interoperate.  The standard implemention used on both unix and windows
** is for the index number to indicate a byte offset into the
** WalCkptInfo.aLock[] array in the wal-index header.  In other words, all
** locks are on the shm file.  The WALINDEX_LOCK_OFFSET constant (which
** should be 120) is the location in the shm file for the first locking
** byte.
*/
#define WAL_WRITE_LOCK         0
#define WAL_ALL_BUT_WRITE      1
#define WAL_CKPT_LOCK          1
#define WAL_RECOVER_LOCK       2
#define WAL_READ_LOCK(I)       (3+(I))
#define WAL_NREADER            (SQLITE_SHM_NLOCK-3)


/* Object declarations */
typedef struct WalIndexHdr WalIndexHdr;
typedef struct WalIterator WalIterator;
typedef struct WalCkptInfo WalCkptInfo;


/*
** The following object holds a copy of the wal-index header content.
**
** The actual header in the wal-index consists of two copies of this
** object followed by one instance of the WalCkptInfo object.
** For all versions of SQLite through 3.10.0 and probably beyond,
** the locking bytes (WalCkptInfo.aLock) start at offset 120 and
** the total header size is 136 bytes.
**
** The szPage value can be any power of 2 between 512 and 32768, inclusive.
** Or it can be 1 to represent a 65536-byte page.  The latter case was
** added in 3.7.1 when support for 64K pages was added.  
*/
struct WalIndexHdr {
  u32 iVersion;                   /* Wal-index version */
  u32 unused;                     /* Unused (padding) field */
  u32 iChange;                    /* Counter incremented each transaction */
  u8 isInit;                      /* 1 when initialized */
  u8 bigEndCksum;                 /* True if checksums in WAL are big-endian */
  u16 szPage;                     /* Database page size in bytes. 1==64K */
  u32 mxFrame;                    /* Index of last valid frame in the WAL */
  u32 nPage;                      /* Size of database in pages */
  u32 aFrameCksum[2];             /* Checksum of last frame in log */
  u32 aSalt[2];                   /* Two salt values copied from WAL header */
  u32 aCksum[2];                  /* Checksum over all prior fields */
};

/*
** A copy of the following object occurs in the wal-index immediately
** following the second copy of the WalIndexHdr.  This object stores
** information used by checkpoint.
**
** nBackfill is the number of frames in the WAL that have been written
** back into the database. (We call the act of moving content from WAL to
** database "backfilling".)  The nBackfill number is never greater than
** WalIndexHdr.mxFrame.  nBackfill can only be increased by threads
** holding the WAL_CKPT_LOCK lock (which includes a recovery thread).
** However, a WAL_WRITE_LOCK thread can move the value of nBackfill from
** mxFrame back to zero when the WAL is reset.
**
** nBackfillAttempted is the largest value of nBackfill that a checkpoint
** has attempted to achieve.  Normally nBackfill==nBackfillAtempted, however
** the nBackfillAttempted is set before any backfilling is done and the
** nBackfill is only set after all backfilling completes.  So if a checkpoint
** crashes, nBackfillAttempted might be larger than nBackfill.  The
** WalIndexHdr.mxFrame must never be less than nBackfillAttempted.
**
** The aLock[] field is a set of bytes used for locking.  These bytes should
** never be read or written.
**
** There is one entry in aReadMark[] for each reader lock.  If a reader
** holds read-lock K, then the value in aReadMark[K] is no greater than
** the mxFrame for that reader.  The value READMARK_NOT_USED (0xffffffff)
** for any aReadMark[] means that entry is unused.  aReadMark[0] is 
** a special case; its value is never used and it exists as a place-holder
** to avoid having to offset aReadMark[] indexs by one.  Readers holding
** WAL_READ_LOCK(0) always ignore the entire WAL and read all content
** directly from the database.
**
** The value of aReadMark[K] may only be changed by a thread that
** is holding an exclusive lock on WAL_READ_LOCK(K).  Thus, the value of
** aReadMark[K] cannot changed while there is a reader is using that mark
** since the reader will be holding a shared lock on WAL_READ_LOCK(K).
**
** The checkpointer may only transfer frames from WAL to database where
** the frame numbers are less than or equal to every aReadMark[] that is
** in use (that is, every aReadMark[j] for which there is a corresponding
** WAL_READ_LOCK(j)).  New readers (usually) pick the aReadMark[] with the
** largest value and will increase an unused aReadMark[] to mxFrame if there
** is not already an aReadMark[] equal to mxFrame.  The exception to the
** previous sentence is when nBackfill equals mxFrame (meaning that everything
** in the WAL has been backfilled into the database) then new readers
** will choose aReadMark[0] which has value 0 and hence such reader will
** get all their all content directly from the database file and ignore 
** the WAL.
**
** Writers normally append new frames to the end of the WAL.  However,
** if nBackfill equals mxFrame (meaning that all WAL content has been
** written back into the database) and if no readers are using the WAL
** (in other words, if there are no WAL_READ_LOCK(i) where i>0) then
** the writer will first "reset" the WAL back to the beginning and start
** writing new content beginning at frame 1.
**
** We assume that 32-bit loads are atomic and so no locks are needed in
** order to read from any aReadMark[] entries.
*/
struct WalCkptInfo {
  u32 nBackfill;                  /* Number of WAL frames backfilled into DB */
  u32 aReadMark[WAL_NREADER];     /* Reader marks */
  u8 aLock[SQLITE_SHM_NLOCK];     /* Reserved space for locks */
  u32 nBackfillAttempted;         /* WAL frames perhaps written, or maybe not */
  u32 notUsed0;                   /* Available for future enhancements */
};
#define READMARK_NOT_USED  0xffffffff


/* A block of WALINDEX_LOCK_RESERVED bytes beginning at
** WALINDEX_LOCK_OFFSET is reserved for locks. Since some systems
** only support mandatory file-locks, we do not read or write data
** from the region of the file on which locks are applied.
*/
#define WALINDEX_LOCK_OFFSET (sizeof(WalIndexHdr)*2+offsetof(WalCkptInfo,aLock))
#define WALINDEX_HDR_SIZE    (sizeof(WalIndexHdr)*2+sizeof(WalCkptInfo))

/* Size of header before each frame in wal */
#define WAL_FRAME_HDRSIZE 24

/* Size of write ahead log header, including checksum. */
#define WAL_HDRSIZE 32

/* WAL magic value. Either this value, or the same value with the least
** significant bit also set (WAL_MAGIC | 0x00000001) is stored in 32-bit
** big-endian format in the first 4 bytes of a WAL file.
**
** If the LSB is set, then the checksums for each frame within the WAL
** file are calculated by treating all data as an array of 32-bit 
** big-endian words. Otherwise, they are calculated by interpreting 
** all data as 32-bit little-endian words.
*/
#define WAL_MAGIC 0x377f0682

/*
** Return the offset of frame iFrame in the write-ahead log file, 
** assuming a database page size of szPage bytes. The offset returned
** is to the start of the write-ahead log frame-header.
*/
#define walFrameOffset(iFrame, szPage) (                               \
  WAL_HDRSIZE + ((iFrame)-1)*(i64)((szPage)+WAL_FRAME_HDRSIZE)         \
)

/*
** An open write-ahead log file is represented by an instance of the
** following object.
*/
struct Wal {
  sqlite3_vfs *pVfs;         /* The VFS used to create pDbFd */
  sqlite3_file *pDbFd;       /* File handle for the database file */
  sqlite3_file *pWalFd;      /* File handle for WAL file */
  u32 iCallback;             /* Value to pass to log callback (or 0) */
  i64 mxWalSize;             /* Truncate WAL to this size upon reset */
  int nWiData;               /* Size of array apWiData */
  int szFirstBlock;          /* Size of first block written to WAL file */
  volatile u32 **apWiData;   /* Pointer to wal-index content in memory */
  u32 szPage;                /* Database page size */
  i16 readLock;              /* Which read lock is being held.  -1 for none */
  u8 syncFlags;              /* Flags to use to sync header writes */
  u8 exclusiveMode;          /* Non-zero if connection is in exclusive mode */
  u8 writeLock;              /* True if in a write transaction */
  u8 ckptLock;               /* True if holding a checkpoint lock */
  u8 readOnly;               /* WAL_RDWR, WAL_RDONLY, or WAL_SHM_RDONLY */
  u8 truncateOnCommit;       /* True to truncate WAL file on commit */
  u8 syncHeader;             /* Fsync the WAL header if true */
  u8 padToSectorBoundary;    /* Pad transactions out to the next sector */
  u8 bShmUnreliable;         /* SHM content is read-only and unreliable */
  WalIndexHdr hdr;           /* Wal-index header for current transaction */
  u32 minFrame;              /* Ignore wal frames before this one */
  u32 iReCksum;              /* On commit, recalculate checksums from here */
  const char *zWalName;      /* Name of WAL file */
  u32 nCkpt;                 /* Checkpoint sequence counter in the wal-header */
#ifdef SQLITE_DEBUG
  u8 lockError;              /* True if a locking error has occurred */
#endif
#ifdef SQLITE_ENABLE_SNAPSHOT
  WalIndexHdr *pSnapshot;    /* Start transaction here if not NULL */
#endif
};

/*
** Candidate values for Wal.exclusiveMode.
*/
#define WAL_NORMAL_MODE     0
#define WAL_EXCLUSIVE_MODE  1     
#define WAL_HEAPMEMORY_MODE 2

/*
** Possible values for WAL.readOnly
*/
#define WAL_RDWR        0    /* Normal read/write connection */
#define WAL_RDONLY      1    /* The WAL file is readonly */
#define WAL_SHM_RDONLY  2    /* The SHM file is readonly */

/*
** Each page of the wal-index mapping contains a hash-table made up of
** an array of HASHTABLE_NSLOT elements of the following type.
*/
typedef u16 ht_slot;

/*
** This structure is used to implement an iterator that loops through
** all frames in the WAL in database page order. Where two or more frames
** correspond to the same database page, the iterator visits only the 
** frame most recently written to the WAL (in other words, the frame with
** the largest index).
**
** The internals of this structure are only accessed by:
**
**   walIteratorInit() - Create a new iterator,
**   walIteratorNext() - Step an iterator,
**   walIteratorFree() - Free an iterator.
**
** This functionality is used by the checkpoint code (see walCheckpoint()).
*/
struct WalIterator {
  int iPrior;                     /* Last result returned from the iterator */
  int nSegment;                   /* Number of entries in aSegment[] */
  struct WalSegment {
    int iNext;                    /* Next slot in aIndex[] not yet returned */
    ht_slot *aIndex;              /* i0, i1, i2... such that aPgno[iN] ascend */
    u32 *aPgno;                   /* Array of page numbers. */
    int nEntry;                   /* Nr. of entries in aPgno[] and aIndex[] */
    int iZero;                    /* Frame number associated with aPgno[0] */
  } aSegment[1];                  /* One for every 32KB page in the wal-index */
};

/*
** Define the parameters of the hash tables in the wal-index file. There
** is a hash-table following every HASHTABLE_NPAGE page numbers in the
** wal-index.
**
** Changing any of these constants will alter the wal-index format and
** create incompatibilities.
*/
#define HASHTABLE_NPAGE      4096                 /* Must be power of 2 */
#define HASHTABLE_HASH_1     383                  /* Should be prime */
#define HASHTABLE_NSLOT      (HASHTABLE_NPAGE*2)  /* Must be a power of 2 */

/* 
** The block of page numbers associated with the first hash-table in a
** wal-index is smaller than usual. This is so that there is a complete
** hash-table on each aligned 32KB page of the wal-index.
*/
#define HASHTABLE_NPAGE_ONE  (HASHTABLE_NPAGE - (WALINDEX_HDR_SIZE/sizeof(u32)))

/* The wal-index is divided into pages of WALINDEX_PGSZ bytes each. */
#define WALINDEX_PGSZ   (                                         \
    sizeof(ht_slot)*HASHTABLE_NSLOT + HASHTABLE_NPAGE*sizeof(u32) \
)

/*
** Obtain a pointer to the iPage'th page of the wal-index. The wal-index
** is broken into pages of WALINDEX_PGSZ bytes. Wal-index pages are
** numbered from zero.
**
** If the wal-index is currently smaller the iPage pages then the size
** of the wal-index might be increased, but only if it is safe to do
** so.  It is safe to enlarge the wal-index if pWal->writeLock is true
** or pWal->exclusiveMode==WAL_HEAPMEMORY_MODE.
**
** If this call is successful, *ppPage is set to point to the wal-index
** page and SQLITE_OK is returned. If an error (an OOM or VFS error) occurs,
** then an SQLite error code is returned and *ppPage is set to 0.
*/
static SQLITE_NOINLINE int walIndexPageRealloc(
  Wal *pWal,               /* The WAL context */
  int iPage,               /* The page we seek */
  volatile u32 **ppPage    /* Write the page pointer here */
){
  int rc = SQLITE_OK;

  /* Enlarge the pWal->apWiData[] array if required */
  if( pWal->nWiData<=iPage ){
    sqlite3_int64 nByte = sizeof(u32*)*(iPage+1);
    volatile u32 **apNew;
    apNew = (volatile u32 **)sqlite3_realloc64((void *)pWal->apWiData, nByte);
    if( !apNew ){
      *ppPage = 0;
      return SQLITE_NOMEM_BKPT;
    }
    memset((void*)&apNew[pWal->nWiData], 0,
           sizeof(u32*)*(iPage+1-pWal->nWiData));
    pWal->apWiData = apNew;
    pWal->nWiData = iPage+1;
  }

  /* Request a pointer to the required page from the VFS */
  assert( pWal->apWiData[iPage]==0 );
  if( pWal->exclusiveMode==WAL_HEAPMEMORY_MODE ){
    pWal->apWiData[iPage] = (u32 volatile *)sqlite3MallocZero(WALINDEX_PGSZ);
    if( !pWal->apWiData[iPage] ) rc = SQLITE_NOMEM_BKPT;
  }else{
    rc = sqlite3OsShmMap(pWal->pDbFd, iPage, WALINDEX_PGSZ, 
        pWal->writeLock, (void volatile **)&pWal->apWiData[iPage]
    );
    assert( pWal->apWiData[iPage]!=0 || rc!=SQLITE_OK || pWal->writeLock==0 );
    testcase( pWal->apWiData[iPage]==0 && rc==SQLITE_OK );
    if( (rc&0xff)==SQLITE_READONLY ){
      pWal->readOnly |= WAL_SHM_RDONLY;
      if( rc==SQLITE_READONLY ){
        rc = SQLITE_OK;
      }
    }
  }

  *ppPage = pWal->apWiData[iPage];
  assert( iPage==0 || *ppPage || rc!=SQLITE_OK );
  return rc;
}
static int walIndexPage(
  Wal *pWal,               /* The WAL context */
  int iPage,               /* The page we seek */
  volatile u32 **ppPage    /* Write the page pointer here */
){
  if( pWal->nWiData<=iPage || (*ppPage = pWal->apWiData[iPage])==0 ){
    return walIndexPageRealloc(pWal, iPage, ppPage);
  }
  return SQLITE_OK;
}

/*
** Return a pointer to the WalCkptInfo structure in the wal-index.
*/
static volatile WalCkptInfo *walCkptInfo(Wal *pWal){
  assert( pWal->nWiData>0 && pWal->apWiData[0] );
  return (volatile WalCkptInfo*)&(pWal->apWiData[0][sizeof(WalIndexHdr)/2]);
}

/*
** Return a pointer to the WalIndexHdr structure in the wal-index.
*/
static volatile WalIndexHdr *walIndexHdr(Wal *pWal){
  assert( pWal->nWiData>0 && pWal->apWiData[0] );
  return (volatile WalIndexHdr*)pWal->apWiData[0];
}

/*
** The argument to this macro must be of type u32. On a little-endian
** architecture, it returns the u32 value that results from interpreting
** the 4 bytes as a big-endian value. On a big-endian architecture, it
** returns the value that would be produced by interpreting the 4 bytes
** of the input value as a little-endian integer.
*/
#define BYTESWAP32(x) ( \
    (((x)&0x000000FF)<<24) + (((x)&0x0000FF00)<<8)  \
  + (((x)&0x00FF0000)>>8)  + (((x)&0xFF000000)>>24) \
)

/*
** Generate or extend an 8 byte checksum based on the data in 
** array aByte[] and the initial values of aIn[0] and aIn[1] (or
** initial values of 0 and 0 if aIn==NULL).
**
** The checksum is written back into aOut[] before returning.
**
** nByte must be a positive multiple of 8.
*/
static void walChecksumBytes(
  int nativeCksum, /* True for native byte-order, false for non-native */
  u8 *a,           /* Content to be checksummed */
  int nByte,       /* Bytes of content in a[].  Must be a multiple of 8. */
  const u32 *aIn,  /* Initial checksum value input */
  u32 *aOut        /* OUT: Final checksum value output */
){
  u32 s1, s2;
  u32 *aData = (u32 *)a;
  u32 *aEnd = (u32 *)&a[nByte];

  if( aIn ){
    s1 = aIn[0];
    s2 = aIn[1];
  }else{
    s1 = s2 = 0;
  }

  assert( nByte>=8 );
  assert( (nByte&0x00000007)==0 );
  assert( nByte<=65536 );

  if( nativeCksum ){
    do {
      s1 += *aData++ + s2;
      s2 += *aData++ + s1;
    }while( aData<aEnd );
  }else{
    do {
      s1 += BYTESWAP32(aData[0]) + s2;
      s2 += BYTESWAP32(aData[1]) + s1;
      aData += 2;
    }while( aData<aEnd );
  }

  aOut[0] = s1;
  aOut[1] = s2;
}

static void walShmBarrier(Wal *pWal){
  if( pWal->exclusiveMode!=WAL_HEAPMEMORY_MODE ){
    sqlite3OsShmBarrier(pWal->pDbFd);
  }
}

/*
** Write the header information in pWal->hdr into the wal-index.
**
** The checksum on pWal->hdr is updated before it is written.
*/
static void walIndexWriteHdr(Wal *pWal){
  volatile WalIndexHdr *aHdr = walIndexHdr(pWal);
  const int nCksum = offsetof(WalIndexHdr, aCksum);

  assert( pWal->writeLock );
  pWal->hdr.isInit = 1;
  pWal->hdr.iVersion = WALINDEX_MAX_VERSION;
  walChecksumBytes(1, (u8*)&pWal->hdr, nCksum, 0, pWal->hdr.aCksum);
  memcpy((void*)&aHdr[1], (const void*)&pWal->hdr, sizeof(WalIndexHdr));
  walShmBarrier(pWal);
  memcpy((void*)&aHdr[0], (const void*)&pWal->hdr, sizeof(WalIndexHdr));
}

/*
** This function encodes a single frame header and writes it to a buffer
** supplied by the caller. A frame-header is made up of a series of 
** 4-byte big-endian integers, as follows:
**
**     0: Page number.
**     4: For commit records, the size of the database image in pages 
**        after the commit. For all other records, zero.
**     8: Salt-1 (copied from the wal-header)
**    12: Salt-2 (copied from the wal-header)
**    16: Checksum-1.
**    20: Checksum-2.
*/
static void walEncodeFrame(
  Wal *pWal,                      /* The write-ahead log */
  u32 iPage,                      /* Database page number for frame */
  u32 nTruncate,                  /* New db size (or 0 for non-commit frames) */
  u8 *aData,                      /* Pointer to page data */
  u8 *aFrame                      /* OUT: Write encoded frame here */
){
  int nativeCksum;                /* True for native byte-order checksums */
  u32 *aCksum = pWal->hdr.aFrameCksum;
  assert( WAL_FRAME_HDRSIZE==24 );
  sqlite3Put4byte(&aFrame[0], iPage);
  sqlite3Put4byte(&aFrame[4], nTruncate);
  if( pWal->iReCksum==0 ){
    memcpy(&aFrame[8], pWal->hdr.aSalt, 8);

    nativeCksum = (pWal->hdr.bigEndCksum==SQLITE_BIGENDIAN);
    walChecksumBytes(nativeCksum, aFrame, 8, aCksum, aCksum);
    walChecksumBytes(nativeCksum, aData, pWal->szPage, aCksum, aCksum);

    sqlite3Put4byte(&aFrame[16], aCksum[0]);
    sqlite3Put4byte(&aFrame[20], aCksum[1]);
  }else{
    memset(&aFrame[8], 0, 16);
  }
}

/*
** Check to see if the frame with header in aFrame[] and content
** in aData[] is valid.  If it is a valid frame, fill *piPage and
** *pnTruncate and return true.  Return if the frame is not valid.
*/
static int walDecodeFrame(
  Wal *pWal,                      /* The write-ahead log */
  u32 *piPage,                    /* OUT: Database page number for frame */
  u32 *pnTruncate,                /* OUT: New db size (or 0 if not commit) */
  u8 *aData,                      /* Pointer to page data (for checksum) */
  u8 *aFrame                      /* Frame data */
){
  int nativeCksum;                /* True for native byte-order checksums */
  u32 *aCksum = pWal->hdr.aFrameCksum;
  u32 pgno;                       /* Page number of the frame */
  assert( WAL_FRAME_HDRSIZE==24 );

  /* A frame is only valid if the salt values in the frame-header
  ** match the salt values in the wal-header. 
  */
  if( memcmp(&pWal->hdr.aSalt, &aFrame[8], 8)!=0 ){
    return 0;
  }

  /* A frame is only valid if the page number is creater than zero.
  */
  pgno = sqlite3Get4byte(&aFrame[0]);
  if( pgno==0 ){
    return 0;
  }

  /* A frame is only valid if a checksum of the WAL header,
  ** all prior frams, the first 16 bytes of this frame-header, 
  ** and the frame-data matches the checksum in the last 8 
  ** bytes of this frame-header.
  */
  nativeCksum = (pWal->hdr.bigEndCksum==SQLITE_BIGENDIAN);
  walChecksumBytes(nativeCksum, aFrame, 8, aCksum, aCksum);
  walChecksumBytes(nativeCksum, aData, pWal->szPage, aCksum, aCksum);
  if( aCksum[0]!=sqlite3Get4byte(&aFrame[16]) 
   || aCksum[1]!=sqlite3Get4byte(&aFrame[20]) 
  ){
    /* Checksum failed. */
    return 0;
  }

  /* If we reach this point, the frame is valid.  Return the page number
  ** and the new database size.
  */
  *piPage = pgno;
  *pnTruncate = sqlite3Get4byte(&aFrame[4]);
  return 1;
}


#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)
/*
** Names of locks.  This routine is used to provide debugging output and is not
** a part of an ordinary build.
*/
static const char *walLockName(int lockIdx){
  if( lockIdx==WAL_WRITE_LOCK ){
    return "WRITE-LOCK";
  }else if( lockIdx==WAL_CKPT_LOCK ){
    return "CKPT-LOCK";
  }else if( lockIdx==WAL_RECOVER_LOCK ){
    return "RECOVER-LOCK";
  }else{
    static char zName[15];
    sqlite3_snprintf(sizeof(zName), zName, "READ-LOCK[%d]",
                     lockIdx-WAL_READ_LOCK(0));
    return zName;
  }
}
#endif /*defined(SQLITE_TEST) || defined(SQLITE_DEBUG) */
    

/*
** Set or release locks on the WAL.  Locks are either shared or exclusive.
** A lock cannot be moved directly between shared and exclusive - it must go
** through the unlocked state first.
**
** In locking_mode=EXCLUSIVE, all of these routines become no-ops.
*/
static int walLockShared(Wal *pWal, int lockIdx){
  int rc;
  if( pWal->exclusiveMode ) return SQLITE_OK;
  rc = sqlite3OsShmLock(pWal->pDbFd, lockIdx, 1,
                        SQLITE_SHM_LOCK | SQLITE_SHM_SHARED);
  WALTRACE(("WAL%p: acquire SHARED-%s %s\n", pWal,
            walLockName(lockIdx), rc ? "failed" : "ok"));
  VVA_ONLY( pWal->lockError = (u8)(rc!=SQLITE_OK && rc!=SQLITE_BUSY); )
  return rc;
}
static void walUnlockShared(Wal *pWal, int lockIdx){
  if( pWal->exclusiveMode ) return;
  (void)sqlite3OsShmLock(pWal->pDbFd, lockIdx, 1,
                         SQLITE_SHM_UNLOCK | SQLITE_SHM_SHARED);
  WALTRACE(("WAL%p: release SHARED-%s\n", pWal, walLockName(lockIdx)));
}
static int walLockExclusive(Wal *pWal, int lockIdx, int n){
  int rc;
  if( pWal->exclusiveMode ) return SQLITE_OK;
  rc = sqlite3OsShmLock(pWal->pDbFd, lockIdx, n,
                        SQLITE_SHM_LOCK | SQLITE_SHM_EXCLUSIVE);
  WALTRACE(("WAL%p: acquire EXCLUSIVE-%s cnt=%d %s\n", pWal,
            walLockName(lockIdx), n, rc ? "failed" : "ok"));
  VVA_ONLY( pWal->lockError = (u8)(rc!=SQLITE_OK && rc!=SQLITE_BUSY); )
  return rc;
}
static void walUnlockExclusive(Wal *pWal, int lockIdx, int n){
  if( pWal->exclusiveMode ) return;
  (void)sqlite3OsShmLock(pWal->pDbFd, lockIdx, n,
                         SQLITE_SHM_UNLOCK | SQLITE_SHM_EXCLUSIVE);
  WALTRACE(("WAL%p: release EXCLUSIVE-%s cnt=%d\n", pWal,
             walLockName(lockIdx), n));
}

/*
** Compute a hash on a page number.  The resulting hash value must land
** between 0 and (HASHTABLE_NSLOT-1).  The walHashNext() function advances
** the hash to the next value in the event of a collision.
*/
static int walHash(u32 iPage){
  assert( iPage>0 );
  assert( (HASHTABLE_NSLOT & (HASHTABLE_NSLOT-1))==0 );
  return (iPage*HASHTABLE_HASH_1) & (HASHTABLE_NSLOT-1);
}
static int walNextHash(int iPriorHash){
  return (iPriorHash+1)&(HASHTABLE_NSLOT-1);
}

/*
** An instance of the WalHashLoc object is used to describe the location
** of a page hash table in the wal-index.  This becomes the return value
** from walHashGet().
*/
typedef struct WalHashLoc WalHashLoc;
struct WalHashLoc {
  volatile ht_slot *aHash;  /* Start of the wal-index hash table */
  volatile u32 *aPgno;      /* aPgno[1] is the page of first frame indexed */
  u32 iZero;                /* One less than the frame number of first indexed*/
};

/* 
** Return pointers to the hash table and page number array stored on
** page iHash of the wal-index. The wal-index is broken into 32KB pages
** numbered starting from 0.
**
** Set output variable pLoc->aHash to point to the start of the hash table
** in the wal-index file. Set pLoc->iZero to one less than the frame 
** number of the first frame indexed by this hash table. If a
** slot in the hash table is set to N, it refers to frame number 
** (pLoc->iZero+N) in the log.
**
** Finally, set pLoc->aPgno so that pLoc->aPgno[1] is the page number of the
** first frame indexed by the hash table, frame (pLoc->iZero+1).
*/
static int walHashGet(
  Wal *pWal,                      /* WAL handle */
  int iHash,                      /* Find the iHash'th table */
  WalHashLoc *pLoc                /* OUT: Hash table location */
){
  int rc;                         /* Return code */

  rc = walIndexPage(pWal, iHash, &pLoc->aPgno);
  assert( rc==SQLITE_OK || iHash>0 );

  if( rc==SQLITE_OK ){
    pLoc->aHash = (volatile ht_slot *)&pLoc->aPgno[HASHTABLE_NPAGE];
    if( iHash==0 ){
      pLoc->aPgno = &pLoc->aPgno[WALINDEX_HDR_SIZE/sizeof(u32)];
      pLoc->iZero = 0;
    }else{
      pLoc->iZero = HASHTABLE_NPAGE_ONE + (iHash-1)*HASHTABLE_NPAGE;
    }
    pLoc->aPgno = &pLoc->aPgno[-1];
  }
  return rc;
}

/*
** Return the number of the wal-index page that contains the hash-table
** and page-number array that contain entries corresponding to WAL frame
** iFrame. The wal-index is broken up into 32KB pages. Wal-index pages 
** are numbered starting from 0.
*/
static int walFramePage(u32 iFrame){
  int iHash = (iFrame+HASHTABLE_NPAGE-HASHTABLE_NPAGE_ONE-1) / HASHTABLE_NPAGE;
  assert( (iHash==0 || iFrame>HASHTABLE_NPAGE_ONE)
       && (iHash>=1 || iFrame<=HASHTABLE_NPAGE_ONE)
       && (iHash<=1 || iFrame>(HASHTABLE_NPAGE_ONE+HASHTABLE_NPAGE))
       && (iHash>=2 || iFrame<=HASHTABLE_NPAGE_ONE+HASHTABLE_NPAGE)
       && (iHash<=2 || iFrame>(HASHTABLE_NPAGE_ONE+2*HASHTABLE_NPAGE))
  );
  return iHash;
}

/*
** Return the page number associated with frame iFrame in this WAL.
*/
static u32 walFramePgno(Wal *pWal, u32 iFrame){
  int iHash = walFramePage(iFrame);
  if( iHash==0 ){
    return pWal->apWiData[0][WALINDEX_HDR_SIZE/sizeof(u32) + iFrame - 1];
  }
  return pWal->apWiData[iHash][(iFrame-1-HASHTABLE_NPAGE_ONE)%HASHTABLE_NPAGE];
}

/*
** Remove entries from the hash table that point to WAL slots greater
** than pWal->hdr.mxFrame.
**
** This function is called whenever pWal->hdr.mxFrame is decreased due
** to a rollback or savepoint.
**
** At most only the hash table containing pWal->hdr.mxFrame needs to be
** updated.  Any later hash tables will be automatically cleared when
** pWal->hdr.mxFrame advances to the point where those hash tables are
** actually needed.
*/
static void walCleanupHash(Wal *pWal){
  WalHashLoc sLoc;                /* Hash table location */
  int iLimit = 0;                 /* Zero values greater than this */
  int nByte;                      /* Number of bytes to zero in aPgno[] */
  int i;                          /* Used to iterate through aHash[] */
  int rc;                         /* Return code form walHashGet() */

  assert( pWal->writeLock );
  testcase( pWal->hdr.mxFrame==HASHTABLE_NPAGE_ONE-1 );
  testcase( pWal->hdr.mxFrame==HASHTABLE_NPAGE_ONE );
  testcase( pWal->hdr.mxFrame==HASHTABLE_NPAGE_ONE+1 );

  if( pWal->hdr.mxFrame==0 ) return;

  /* Obtain pointers to the hash-table and page-number array containing 
  ** the entry that corresponds to frame pWal->hdr.mxFrame. It is guaranteed
  ** that the page said hash-table and array reside on is already mapped.(1)
  */
  assert( pWal->nWiData>walFramePage(pWal->hdr.mxFrame) );
  assert( pWal->apWiData[walFramePage(pWal->hdr.mxFrame)] );
  rc = walHashGet(pWal, walFramePage(pWal->hdr.mxFrame), &sLoc);
  if( NEVER(rc) ) return; /* Defense-in-depth, in case (1) above is wrong */

  /* Zero all hash-table entries that correspond to frame numbers greater
  ** than pWal->hdr.mxFrame.
  */
  iLimit = pWal->hdr.mxFrame - sLoc.iZero;
  assert( iLimit>0 );
  for(i=0; i<HASHTABLE_NSLOT; i++){
    if( sLoc.aHash[i]>iLimit ){
      sLoc.aHash[i] = 0;
    }
  }
  
  /* Zero the entries in the aPgno array that correspond to frames with
  ** frame numbers greater than pWal->hdr.mxFrame. 
  */
  nByte = (int)((char *)sLoc.aHash - (char *)&sLoc.aPgno[iLimit+1]);
  memset((void *)&sLoc.aPgno[iLimit+1], 0, nByte);

#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT
  /* Verify that the every entry in the mapping region is still reachable
  ** via the hash table even after the cleanup.
  */
  if( iLimit ){
    int j;           /* Loop counter */
    int iKey;        /* Hash key */
    for(j=1; j<=iLimit; j++){
      for(iKey=walHash(sLoc.aPgno[j]);sLoc.aHash[iKey];iKey=walNextHash(iKey)){
        if( sLoc.aHash[iKey]==j ) break;
      }
      assert( sLoc.aHash[iKey]==j );
    }
  }
#endif /* SQLITE_ENABLE_EXPENSIVE_ASSERT */
}


/*
** Set an entry in the wal-index that will map database page number
** pPage into WAL frame iFrame.
*/
static int walIndexAppend(Wal *pWal, u32 iFrame, u32 iPage){
  int rc;                         /* Return code */
  WalHashLoc sLoc;                /* Wal-index hash table location */

  rc = walHashGet(pWal, walFramePage(iFrame), &sLoc);

  /* Assuming the wal-index file was successfully mapped, populate the
  ** page number array and hash table entry.
  */
  if( rc==SQLITE_OK ){
    int iKey;                     /* Hash table key */
    int idx;                      /* Value to write to hash-table slot */
    int nCollide;                 /* Number of hash collisions */

    idx = iFrame - sLoc.iZero;
    assert( idx <= HASHTABLE_NSLOT/2 + 1 );
    
    /* If this is the first entry to be added to this hash-table, zero the
    ** entire hash table and aPgno[] array before proceeding. 
    */
    if( idx==1 ){
      int nByte = (int)((u8 *)&sLoc.aHash[HASHTABLE_NSLOT]
                               - (u8 *)&sLoc.aPgno[1]);
      memset((void*)&sLoc.aPgno[1], 0, nByte);
    }

    /* If the entry in aPgno[] is already set, then the previous writer
    ** must have exited unexpectedly in the middle of a transaction (after
    ** writing one or more dirty pages to the WAL to free up memory). 
    ** Remove the remnants of that writers uncommitted transaction from 
    ** the hash-table before writing any new entries.
    */
    if( sLoc.aPgno[idx] ){
      walCleanupHash(pWal);
      assert( !sLoc.aPgno[idx] );
    }

    /* Write the aPgno[] array entry and the hash-table slot. */
    nCollide = idx;
    for(iKey=walHash(iPage); sLoc.aHash[iKey]; iKey=walNextHash(iKey)){
      if( (nCollide--)==0 ) return SQLITE_CORRUPT_BKPT;
    }
    sLoc.aPgno[idx] = iPage;
    sLoc.aHash[iKey] = (ht_slot)idx;

#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT
    /* Verify that the number of entries in the hash table exactly equals
    ** the number of entries in the mapping region.
    */
    {
      int i;           /* Loop counter */
      int nEntry = 0;  /* Number of entries in the hash table */
      for(i=0; i<HASHTABLE_NSLOT; i++){ if( sLoc.aHash[i] ) nEntry++; }
      assert( nEntry==idx );
    }

    /* Verify that the every entry in the mapping region is reachable
    ** via the hash table.  This turns out to be a really, really expensive
    ** thing to check, so only do this occasionally - not on every
    ** iteration.
    */
    if( (idx&0x3ff)==0 ){
      int i;           /* Loop counter */
      for(i=1; i<=idx; i++){
        for(iKey=walHash(sLoc.aPgno[i]);
            sLoc.aHash[iKey];
            iKey=walNextHash(iKey)){
          if( sLoc.aHash[iKey]==i ) break;
        }
        assert( sLoc.aHash[iKey]==i );
      }
    }
#endif /* SQLITE_ENABLE_EXPENSIVE_ASSERT */
  }


  return rc;
}


/*
** Recover the wal-index by reading the write-ahead log file. 
**
** This routine first tries to establish an exclusive lock on the
** wal-index to prevent other threads/processes from doing anything
** with the WAL or wal-index while recovery is running.  The
** WAL_RECOVER_LOCK is also held so that other threads will know
** that this thread is running recovery.  If unable to establish
** the necessary locks, this routine returns SQLITE_BUSY.
*/
static int walIndexRecover(Wal *pWal){
  int rc;                         /* Return Code */
  i64 nSize;                      /* Size of log file */
  u32 aFrameCksum[2] = {0, 0};
  int iLock;                      /* Lock offset to lock for checkpoint */

  /* Obtain an exclusive lock on all byte in the locking range not already
  ** locked by the caller. The caller is guaranteed to have locked the
  ** WAL_WRITE_LOCK byte, and may have also locked the WAL_CKPT_LOCK byte.
  ** If successful, the same bytes that are locked here are unlocked before
  ** this function returns.
  */
  assert( pWal->ckptLock==1 || pWal->ckptLock==0 );
  assert( WAL_ALL_BUT_WRITE==WAL_WRITE_LOCK+1 );
  assert( WAL_CKPT_LOCK==WAL_ALL_BUT_WRITE );
  assert( pWal->writeLock );
  iLock = WAL_ALL_BUT_WRITE + pWal->ckptLock;
  rc = walLockExclusive(pWal, iLock, WAL_READ_LOCK(0)-iLock);
  if( rc==SQLITE_OK ){
    rc = walLockExclusive(pWal, WAL_READ_LOCK(1), WAL_NREADER-1);
    if( rc!=SQLITE_OK ){
      walUnlockExclusive(pWal, iLock, WAL_READ_LOCK(0)-iLock);
    }
  }
  if( rc ){
    return rc;
  }

  WALTRACE(("WAL%p: recovery begin...\n", pWal));

  memset(&pWal->hdr, 0, sizeof(WalIndexHdr));

  rc = sqlite3OsFileSize(pWal->pWalFd, &nSize);
  if( rc!=SQLITE_OK ){
    goto recovery_error;
  }

  if( nSize>WAL_HDRSIZE ){
    u8 aBuf[WAL_HDRSIZE];         /* Buffer to load WAL header into */
    u8 *aFrame = 0;               /* Malloc'd buffer to load entire frame */
    int szFrame;                  /* Number of bytes in buffer aFrame[] */
    u8 *aData;                    /* Pointer to data part of aFrame buffer */
    int iFrame;                   /* Index of last frame read */
    i64 iOffset;                  /* Next offset to read from log file */
    int szPage;                   /* Page size according to the log */
    u32 magic;                    /* Magic value read from WAL header */
    u32 version;                  /* Magic value read from WAL header */
    int isValid;                  /* True if this frame is valid */

    /* Read in the WAL header. */
    rc = sqlite3OsRead(pWal->pWalFd, aBuf, WAL_HDRSIZE, 0);
    if( rc!=SQLITE_OK ){
      goto recovery_error;
    }

    /* If the database page size is not a power of two, or is greater than
    ** SQLITE_MAX_PAGE_SIZE, conclude that the WAL file contains no valid 
    ** data. Similarly, if the 'magic' value is invalid, ignore the whole
    ** WAL file.
    */
    magic = sqlite3Get4byte(&aBuf[0]);
    szPage = sqlite3Get4byte(&aBuf[8]);
    if( (magic&0xFFFFFFFE)!=WAL_MAGIC 
     || szPage&(szPage-1) 
     || szPage>SQLITE_MAX_PAGE_SIZE 
     || szPage<512 
    ){
      goto finished;
    }
    pWal->hdr.bigEndCksum = (u8)(magic&0x00000001);
    pWal->szPage = szPage;
    pWal->nCkpt = sqlite3Get4byte(&aBuf[12]);
    memcpy(&pWal->hdr.aSalt, &aBuf[16], 8);

    /* Verify that the WAL header checksum is correct */
    walChecksumBytes(pWal->hdr.bigEndCksum==SQLITE_BIGENDIAN, 
        aBuf, WAL_HDRSIZE-2*4, 0, pWal->hdr.aFrameCksum
    );
    if( pWal->hdr.aFrameCksum[0]!=sqlite3Get4byte(&aBuf[24])
     || pWal->hdr.aFrameCksum[1]!=sqlite3Get4byte(&aBuf[28])
    ){
      goto finished;
    }

    /* Verify that the version number on the WAL format is one that
    ** are able to understand */
    version = sqlite3Get4byte(&aBuf[4]);
    if( version!=WAL_MAX_VERSION ){
      rc = SQLITE_CANTOPEN_BKPT;
      goto finished;
    }

    /* Malloc a buffer to read frames into. */
    szFrame = szPage + WAL_FRAME_HDRSIZE;
    aFrame = (u8 *)sqlite3_malloc64(szFrame);
    if( !aFrame ){
      rc = SQLITE_NOMEM_BKPT;
      goto recovery_error;
    }
    aData = &aFrame[WAL_FRAME_HDRSIZE];

    /* Read all frames from the log file. */
    iFrame = 0;
    for(iOffset=WAL_HDRSIZE; (iOffset+szFrame)<=nSize; iOffset+=szFrame){
      u32 pgno;                   /* Database page number for frame */
      u32 nTruncate;              /* dbsize field from frame header */

      /* Read and decode the next log frame. */
      iFrame++;
      rc = sqlite3OsRead(pWal->pWalFd, aFrame, szFrame, iOffset);
      if( rc!=SQLITE_OK ) break;
      isValid = walDecodeFrame(pWal, &pgno, &nTruncate, aData, aFrame);
      if( !isValid ) break;
      rc = walIndexAppend(pWal, iFrame, pgno);
      if( rc!=SQLITE_OK ) break;

      /* If nTruncate is non-zero, this is a commit record. */
      if( nTruncate ){
        pWal->hdr.mxFrame = iFrame;
        pWal->hdr.nPage = nTruncate;
        pWal->hdr.szPage = (u16)((szPage&0xff00) | (szPage>>16));
        testcase( szPage<=32768 );
        testcase( szPage>=65536 );
        aFrameCksum[0] = pWal->hdr.aFrameCksum[0];
        aFrameCksum[1] = pWal->hdr.aFrameCksum[1];
      }
    }

    sqlite3_free(aFrame);
  }

finished:
  if( rc==SQLITE_OK ){
    volatile WalCkptInfo *pInfo;
    int i;
    pWal->hdr.aFrameCksum[0] = aFrameCksum[0];
    pWal->hdr.aFrameCksum[1] = aFrameCksum[1];
    walIndexWriteHdr(pWal);

    /* Reset the checkpoint-header. This is safe because this thread is 
    ** currently holding locks that exclude all other readers, writers and
    ** checkpointers.
    */
    pInfo = walCkptInfo(pWal);
    pInfo->nBackfill = 0;
    pInfo->nBackfillAttempted = pWal->hdr.mxFrame;
    pInfo->aReadMark[0] = 0;
    for(i=1; i<WAL_NREADER; i++) pInfo->aReadMark[i] = READMARK_NOT_USED;
    if( pWal->hdr.mxFrame ) pInfo->aReadMark[1] = pWal->hdr.mxFrame;

    /* If more than one frame was recovered from the log file, report an
    ** event via sqlite3_log(). This is to help with identifying performance
    ** problems caused by applications routinely shutting down without
    ** checkpointing the log file.
    */
    if( pWal->hdr.nPage ){
      sqlite3_log(SQLITE_NOTICE_RECOVER_WAL,
          "recovered %d frames from WAL file %s",
          pWal->hdr.mxFrame, pWal->zWalName
      );
    }
  }

recovery_error:
  WALTRACE(("WAL%p: recovery %s\n", pWal, rc ? "failed" : "ok"));
  walUnlockExclusive(pWal, iLock, WAL_READ_LOCK(0)-iLock);
  walUnlockExclusive(pWal, WAL_READ_LOCK(1), WAL_NREADER-1);
  return rc;
}

/*
** Close an open wal-index.
*/
static void walIndexClose(Wal *pWal, int isDelete){
  if( pWal->exclusiveMode==WAL_HEAPMEMORY_MODE || pWal->bShmUnreliable ){
    int i;
    for(i=0; i<pWal->nWiData; i++){
      sqlite3_free((void *)pWal->apWiData[i]);
      pWal->apWiData[i] = 0;
    }
  }
  if( pWal->exclusiveMode!=WAL_HEAPMEMORY_MODE ){
    sqlite3OsShmUnmap(pWal->pDbFd, isDelete);
  }
}

/* 
** Open a connection to the WAL file zWalName. The database file must 
** already be opened on connection pDbFd. The buffer that zWalName points
** to must remain valid for the lifetime of the returned Wal* handle.
**
** A SHARED lock should be held on the database file when this function
** is called. The purpose of this SHARED lock is to prevent any other
** client from unlinking the WAL or wal-index file. If another process
** were to do this just after this client opened one of these files, the
** system would be badly broken.
**
** If the log file is successfully opened, SQLITE_OK is returned and 
** *ppWal is set to point to a new WAL handle. If an error occurs,
** an SQLite error code is returned and *ppWal is left unmodified.
*/
SQLITE_PRIVATE int sqlite3WalOpen(
  sqlite3_vfs *pVfs,              /* vfs module to open wal and wal-index */
  sqlite3_file *pDbFd,            /* The open database file */
  const char *zWalName,           /* Name of the WAL file */
  int bNoShm,                     /* True to run in heap-memory mode */
  i64 mxWalSize,                  /* Truncate WAL to this size on reset */
  Wal **ppWal                     /* OUT: Allocated Wal handle */
){
  int rc;                         /* Return Code */
  Wal *pRet;                      /* Object to allocate and return */
  int flags;                      /* Flags passed to OsOpen() */

  assert( zWalName && zWalName[0] );
  assert( pDbFd );

  /* In the amalgamation, the os_unix.c and os_win.c source files come before
  ** this source file.  Verify that the #defines of the locking byte offsets
  ** in os_unix.c and os_win.c agree with the WALINDEX_LOCK_OFFSET value.
  ** For that matter, if the lock offset ever changes from its initial design
  ** value of 120, we need to know that so there is an assert() to check it.
  */
  assert( 120==WALINDEX_LOCK_OFFSET );
  assert( 136==WALINDEX_HDR_SIZE );
#ifdef WIN_SHM_BASE
  assert( WIN_SHM_BASE==WALINDEX_LOCK_OFFSET );
#endif
#ifdef UNIX_SHM_BASE
  assert( UNIX_SHM_BASE==WALINDEX_LOCK_OFFSET );
#endif


  /* Allocate an instance of struct Wal to return. */
  *ppWal = 0;
  pRet = (Wal*)sqlite3MallocZero(sizeof(Wal) + pVfs->szOsFile);
  if( !pRet ){
    return SQLITE_NOMEM_BKPT;
  }

  pRet->pVfs = pVfs;
  pRet->pWalFd = (sqlite3_file *)&pRet[1];
  pRet->pDbFd = pDbFd;
  pRet->readLock = -1;
  pRet->mxWalSize = mxWalSize;
  pRet->zWalName = zWalName;
  pRet->syncHeader = 1;
  pRet->padToSectorBoundary = 1;
  pRet->exclusiveMode = (bNoShm ? WAL_HEAPMEMORY_MODE: WAL_NORMAL_MODE);

  /* Open file handle on the write-ahead log file. */
  flags = (SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_WAL);
  rc = sqlite3OsOpen(pVfs, zWalName, pRet->pWalFd, flags, &flags);
  if( rc==SQLITE_OK && flags&SQLITE_OPEN_READONLY ){
    pRet->readOnly = WAL_RDONLY;
  }

  if( rc!=SQLITE_OK ){
    walIndexClose(pRet, 0);
    sqlite3OsClose(pRet->pWalFd);
    sqlite3_free(pRet);
  }else{
    int iDC = sqlite3OsDeviceCharacteristics(pDbFd);
    if( iDC & SQLITE_IOCAP_SEQUENTIAL ){ pRet->syncHeader = 0; }
    if( iDC & SQLITE_IOCAP_POWERSAFE_OVERWRITE ){
      pRet->padToSectorBoundary = 0;
    }
    *ppWal = pRet;
    WALTRACE(("WAL%d: opened\n", pRet));
  }
  return rc;
}

/*
** Change the size to which the WAL file is trucated on each reset.
*/
SQLITE_PRIVATE void sqlite3WalLimit(Wal *pWal, i64 iLimit){
  if( pWal ) pWal->mxWalSize = iLimit;
}

/*
** Find the smallest page number out of all pages held in the WAL that
** has not been returned by any prior invocation of this method on the
** same WalIterator object.   Write into *piFrame the frame index where
** that page was last written into the WAL.  Write into *piPage the page
** number.
**
** Return 0 on success.  If there are no pages in the WAL with a page
** number larger than *piPage, then return 1.
*/
static int walIteratorNext(
  WalIterator *p,               /* Iterator */
  u32 *piPage,                  /* OUT: The page number of the next page */
  u32 *piFrame                  /* OUT: Wal frame index of next page */
){
  u32 iMin;                     /* Result pgno must be greater than iMin */
  u32 iRet = 0xFFFFFFFF;        /* 0xffffffff is never a valid page number */
  int i;                        /* For looping through segments */

  iMin = p->iPrior;
  assert( iMin<0xffffffff );
  for(i=p->nSegment-1; i>=0; i--){
    struct WalSegment *pSegment = &p->aSegment[i];
    while( pSegment->iNext<pSegment->nEntry ){
      u32 iPg = pSegment->aPgno[pSegment->aIndex[pSegment->iNext]];
      if( iPg>iMin ){
        if( iPg<iRet ){
          iRet = iPg;
          *piFrame = pSegment->iZero + pSegment->aIndex[pSegment->iNext];
        }
        break;
      }
      pSegment->iNext++;
    }
  }

  *piPage = p->iPrior = iRet;
  return (iRet==0xFFFFFFFF);
}

/*
** This function merges two sorted lists into a single sorted list.
**
** aLeft[] and aRight[] are arrays of indices.  The sort key is
** aContent[aLeft[]] and aContent[aRight[]].  Upon entry, the following
** is guaranteed for all J<K:
**
**        aContent[aLeft[J]] < aContent[aLeft[K]]
**        aContent[aRight[J]] < aContent[aRight[K]]
**
** This routine overwrites aRight[] with a new (probably longer) sequence
** of indices such that the aRight[] contains every index that appears in
** either aLeft[] or the old aRight[] and such that the second condition
** above is still met.
**
** The aContent[aLeft[X]] values will be unique for all X.  And the
** aContent[aRight[X]] values will be unique too.  But there might be
** one or more combinations of X and Y such that
**
**      aLeft[X]!=aRight[Y]  &&  aContent[aLeft[X]] == aContent[aRight[Y]]
**
** When that happens, omit the aLeft[X] and use the aRight[Y] index.
*/
static void walMerge(
  const u32 *aContent,            /* Pages in wal - keys for the sort */
  ht_slot *aLeft,                 /* IN: Left hand input list */
  int nLeft,                      /* IN: Elements in array *paLeft */
  ht_slot **paRight,              /* IN/OUT: Right hand input list */
  int *pnRight,                   /* IN/OUT: Elements in *paRight */
  ht_slot *aTmp                   /* Temporary buffer */
){
  int iLeft = 0;                  /* Current index in aLeft */
  int iRight = 0;                 /* Current index in aRight */
  int iOut = 0;                   /* Current index in output buffer */
  int nRight = *pnRight;
  ht_slot *aRight = *paRight;

  assert( nLeft>0 && nRight>0 );
  while( iRight<nRight || iLeft<nLeft ){
    ht_slot logpage;
    Pgno dbpage;

    if( (iLeft<nLeft) 
     && (iRight>=nRight || aContent[aLeft[iLeft]]<aContent[aRight[iRight]])
    ){
      logpage = aLeft[iLeft++];
    }else{
      logpage = aRight[iRight++];
    }
    dbpage = aContent[logpage];

    aTmp[iOut++] = logpage;
    if( iLeft<nLeft && aContent[aLeft[iLeft]]==dbpage ) iLeft++;

    assert( iLeft>=nLeft || aContent[aLeft[iLeft]]>dbpage );
    assert( iRight>=nRight || aContent[aRight[iRight]]>dbpage );
  }

  *paRight = aLeft;
  *pnRight = iOut;
  memcpy(aLeft, aTmp, sizeof(aTmp[0])*iOut);
}

/*
** Sort the elements in list aList using aContent[] as the sort key.
** Remove elements with duplicate keys, preferring to keep the
** larger aList[] values.
**
** The aList[] entries are indices into aContent[].  The values in
** aList[] are to be sorted so that for all J<K:
**
**      aContent[aList[J]] < aContent[aList[K]]
**
** For any X and Y such that
**
**      aContent[aList[X]] == aContent[aList[Y]]
**
** Keep the larger of the two values aList[X] and aList[Y] and discard
** the smaller.
*/
static void walMergesort(
  const u32 *aContent,            /* Pages in wal */
  ht_slot *aBuffer,               /* Buffer of at least *pnList items to use */
  ht_slot *aList,                 /* IN/OUT: List to sort */
  int *pnList                     /* IN/OUT: Number of elements in aList[] */
){
  struct Sublist {
    int nList;                    /* Number of elements in aList */
    ht_slot *aList;               /* Pointer to sub-list content */
  };

  const int nList = *pnList;      /* Size of input list */
  int nMerge = 0;                 /* Number of elements in list aMerge */
  ht_slot *aMerge = 0;            /* List to be merged */
  int iList;                      /* Index into input list */
  u32 iSub = 0;                   /* Index into aSub array */
  struct Sublist aSub[13];        /* Array of sub-lists */

  memset(aSub, 0, sizeof(aSub));
  assert( nList<=HASHTABLE_NPAGE && nList>0 );
  assert( HASHTABLE_NPAGE==(1<<(ArraySize(aSub)-1)) );

  for(iList=0; iList<nList; iList++){
    nMerge = 1;
    aMerge = &aList[iList];
    for(iSub=0; iList & (1<<iSub); iSub++){
      struct Sublist *p;
      assert( iSub<ArraySize(aSub) );
      p = &aSub[iSub];
      assert( p->aList && p->nList<=(1<<iSub) );
      assert( p->aList==&aList[iList&~((2<<iSub)-1)] );
      walMerge(aContent, p->aList, p->nList, &aMerge, &nMerge, aBuffer);
    }
    aSub[iSub].aList = aMerge;
    aSub[iSub].nList = nMerge;
  }

  for(iSub++; iSub<ArraySize(aSub); iSub++){
    if( nList & (1<<iSub) ){
      struct Sublist *p;
      assert( iSub<ArraySize(aSub) );
      p = &aSub[iSub];
      assert( p->nList<=(1<<iSub) );
      assert( p->aList==&aList[nList&~((2<<iSub)-1)] );
      walMerge(aContent, p->aList, p->nList, &aMerge, &nMerge, aBuffer);
    }
  }
  assert( aMerge==aList );
  *pnList = nMerge;

#ifdef SQLITE_DEBUG
  {
    int i;
    for(i=1; i<*pnList; i++){
      assert( aContent[aList[i]] > aContent[aList[i-1]] );
    }
  }
#endif
}

/* 
** Free an iterator allocated by walIteratorInit().
*/
static void walIteratorFree(WalIterator *p){
  sqlite3_free(p);
}

/*
** Construct a WalInterator object that can be used to loop over all 
** pages in the WAL following frame nBackfill in ascending order. Frames
** nBackfill or earlier may be included - excluding them is an optimization
** only. The caller must hold the checkpoint lock.
**
** On success, make *pp point to the newly allocated WalInterator object
** return SQLITE_OK. Otherwise, return an error code. If this routine
** returns an error, the value of *pp is undefined.
**
** The calling routine should invoke walIteratorFree() to destroy the
** WalIterator object when it has finished with it.
*/
static int walIteratorInit(Wal *pWal, u32 nBackfill, WalIterator **pp){
  WalIterator *p;                 /* Return value */
  int nSegment;                   /* Number of segments to merge */
  u32 iLast;                      /* Last frame in log */
  sqlite3_int64 nByte;            /* Number of bytes to allocate */
  int i;                          /* Iterator variable */
  ht_slot *aTmp;                  /* Temp space used by merge-sort */
  int rc = SQLITE_OK;             /* Return Code */

  /* This routine only runs while holding the checkpoint lock. And
  ** it only runs if there is actually content in the log (mxFrame>0).
  */
  assert( pWal->ckptLock && pWal->hdr.mxFrame>0 );
  iLast = pWal->hdr.mxFrame;

  /* Allocate space for the WalIterator object. */
  nSegment = walFramePage(iLast) + 1;
  nByte = sizeof(WalIterator) 
        + (nSegment-1)*sizeof(struct WalSegment)
        + iLast*sizeof(ht_slot);
  p = (WalIterator *)sqlite3_malloc64(nByte);
  if( !p ){
    return SQLITE_NOMEM_BKPT;
  }
  memset(p, 0, nByte);
  p->nSegment = nSegment;

  /* Allocate temporary space used by the merge-sort routine. This block
  ** of memory will be freed before this function returns.
  */
  aTmp = (ht_slot *)sqlite3_malloc64(
      sizeof(ht_slot) * (iLast>HASHTABLE_NPAGE?HASHTABLE_NPAGE:iLast)
  );
  if( !aTmp ){
    rc = SQLITE_NOMEM_BKPT;
  }

  for(i=walFramePage(nBackfill+1); rc==SQLITE_OK && i<nSegment; i++){
    WalHashLoc sLoc;

    rc = walHashGet(pWal, i, &sLoc);
    if( rc==SQLITE_OK ){
      int j;                      /* Counter variable */
      int nEntry;                 /* Number of entries in this segment */
      ht_slot *aIndex;            /* Sorted index for this segment */

      sLoc.aPgno++;
      if( (i+1)==nSegment ){
        nEntry = (int)(iLast - sLoc.iZero);
      }else{
        nEntry = (int)((u32*)sLoc.aHash - (u32*)sLoc.aPgno);
      }
      aIndex = &((ht_slot *)&p->aSegment[p->nSegment])[sLoc.iZero];
      sLoc.iZero++;
  
      for(j=0; j<nEntry; j++){
        aIndex[j] = (ht_slot)j;
      }
      walMergesort((u32 *)sLoc.aPgno, aTmp, aIndex, &nEntry);
      p->aSegment[i].iZero = sLoc.iZero;
      p->aSegment[i].nEntry = nEntry;
      p->aSegment[i].aIndex = aIndex;
      p->aSegment[i].aPgno = (u32 *)sLoc.aPgno;
    }
  }
  sqlite3_free(aTmp);

  if( rc!=SQLITE_OK ){
    walIteratorFree(p);
    p = 0;
  }
  *pp = p;
  return rc;
}

/*
** Attempt to obtain the exclusive WAL lock defined by parameters lockIdx and
** n. If the attempt fails and parameter xBusy is not NULL, then it is a
** busy-handler function. Invoke it and retry the lock until either the
** lock is successfully obtained or the busy-handler returns 0.
*/
static int walBusyLock(
  Wal *pWal,                      /* WAL connection */
  int (*xBusy)(void*),            /* Function to call when busy */
  void *pBusyArg,                 /* Context argument for xBusyHandler */
  int lockIdx,                    /* Offset of first byte to lock */
  int n                           /* Number of bytes to lock */
){
  int rc;
  do {
    rc = walLockExclusive(pWal, lockIdx, n);
  }while( xBusy && rc==SQLITE_BUSY && xBusy(pBusyArg) );
  return rc;
}

/*
** The cache of the wal-index header must be valid to call this function.
** Return the page-size in bytes used by the database.
*/
static int walPagesize(Wal *pWal){
  return (pWal->hdr.szPage&0xfe00) + ((pWal->hdr.szPage&0x0001)<<16);
}

/*
** The following is guaranteed when this function is called:
**
**   a) the WRITER lock is held,
**   b) the entire log file has been checkpointed, and
**   c) any existing readers are reading exclusively from the database
**      file - there are no readers that may attempt to read a frame from
**      the log file.
**
** This function updates the shared-memory structures so that the next
** client to write to the database (which may be this one) does so by
** writing frames into the start of the log file.
**
** The value of parameter salt1 is used as the aSalt[1] value in the 
** new wal-index header. It should be passed a pseudo-random value (i.e. 
** one obtained from sqlite3_randomness()).
*/
static void walRestartHdr(Wal *pWal, u32 salt1){
  volatile WalCkptInfo *pInfo = walCkptInfo(pWal);
  int i;                          /* Loop counter */
  u32 *aSalt = pWal->hdr.aSalt;   /* Big-endian salt values */
  pWal->nCkpt++;
  pWal->hdr.mxFrame = 0;
  sqlite3Put4byte((u8*)&aSalt[0], 1 + sqlite3Get4byte((u8*)&aSalt[0]));
  memcpy(&pWal->hdr.aSalt[1], &salt1, 4);
  walIndexWriteHdr(pWal);
  pInfo->nBackfill = 0;
  pInfo->nBackfillAttempted = 0;
  pInfo->aReadMark[1] = 0;
  for(i=2; i<WAL_NREADER; i++) pInfo->aReadMark[i] = READMARK_NOT_USED;
  assert( pInfo->aReadMark[0]==0 );
}

/*
** Copy as much content as we can from the WAL back into the database file
** in response to an sqlite3_wal_checkpoint() request or the equivalent.
**
** The amount of information copies from WAL to database might be limited
** by active readers.  This routine will never overwrite a database page
** that a concurrent reader might be using.
**
** All I/O barrier operations (a.k.a fsyncs) occur in this routine when
** SQLite is in WAL-mode in synchronous=NORMAL.  That means that if 
** checkpoints are always run by a background thread or background 
** process, foreground threads will never block on a lengthy fsync call.
**
** Fsync is called on the WAL before writing content out of the WAL and
** into the database.  This ensures that if the new content is persistent
** in the WAL and can be recovered following a power-loss or hard reset.
**
** Fsync is also called on the database file if (and only if) the entire
** WAL content is copied into the database file.  This second fsync makes
** it safe to delete the WAL since the new content will persist in the
** database file.
**
** This routine uses and updates the nBackfill field of the wal-index header.
** This is the only routine that will increase the value of nBackfill.  
** (A WAL reset or recovery will revert nBackfill to zero, but not increase
** its value.)
**
** The caller must be holding sufficient locks to ensure that no other
** checkpoint is running (in any other thread or process) at the same
** time.
*/
static int walCheckpoint(
  Wal *pWal,                      /* Wal connection */
  sqlite3 *db,                    /* Check for interrupts on this handle */
  int eMode,                      /* One of PASSIVE, FULL or RESTART */
  int (*xBusy)(void*),            /* Function to call when busy */
  void *pBusyArg,                 /* Context argument for xBusyHandler */
  int sync_flags,                 /* Flags for OsSync() (or 0) */
  u8 *zBuf                        /* Temporary buffer to use */
){
  int rc = SQLITE_OK;             /* Return code */
  int szPage;                     /* Database page-size */
  WalIterator *pIter = 0;         /* Wal iterator context */
  u32 iDbpage = 0;                /* Next database page to write */
  u32 iFrame = 0;                 /* Wal frame containing data for iDbpage */
  u32 mxSafeFrame;                /* Max frame that can be backfilled */
  u32 mxPage;                     /* Max database page to write */
  int i;                          /* Loop counter */
  volatile WalCkptInfo *pInfo;    /* The checkpoint status information */

  szPage = walPagesize(pWal);
  testcase( szPage<=32768 );
  testcase( szPage>=65536 );
  pInfo = walCkptInfo(pWal);
  if( pInfo->nBackfill<pWal->hdr.mxFrame ){

    /* EVIDENCE-OF: R-62920-47450 The busy-handler callback is never invoked
    ** in the SQLITE_CHECKPOINT_PASSIVE mode. */
    assert( eMode!=SQLITE_CHECKPOINT_PASSIVE || xBusy==0 );

    /* Compute in mxSafeFrame the index of the last frame of the WAL that is
    ** safe to write into the database.  Frames beyond mxSafeFrame might
    ** overwrite database pages that are in use by active readers and thus
    ** cannot be backfilled from the WAL.
    */
    mxSafeFrame = pWal->hdr.mxFrame;
    mxPage = pWal->hdr.nPage;
    for(i=1; i<WAL_NREADER; i++){
      /* Thread-sanitizer reports that the following is an unsafe read,
      ** as some other thread may be in the process of updating the value
      ** of the aReadMark[] slot. The assumption here is that if that is
      ** happening, the other client may only be increasing the value,
      ** not decreasing it. So assuming either that either the "old" or
      ** "new" version of the value is read, and not some arbitrary value
      ** that would never be written by a real client, things are still 
      ** safe.  */
      u32 y = pInfo->aReadMark[i];
      if( mxSafeFrame>y ){
        assert( y<=pWal->hdr.mxFrame );
        rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_READ_LOCK(i), 1);
        if( rc==SQLITE_OK ){
          pInfo->aReadMark[i] = (i==1 ? mxSafeFrame : READMARK_NOT_USED);
          walUnlockExclusive(pWal, WAL_READ_LOCK(i), 1);
        }else if( rc==SQLITE_BUSY ){
          mxSafeFrame = y;
          xBusy = 0;
        }else{
          goto walcheckpoint_out;
        }
      }
    }

    /* Allocate the iterator */
    if( pInfo->nBackfill<mxSafeFrame ){
      rc = walIteratorInit(pWal, pInfo->nBackfill, &pIter);
      assert( rc==SQLITE_OK || pIter==0 );
    }

    if( pIter
     && (rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_READ_LOCK(0),1))==SQLITE_OK
    ){
      u32 nBackfill = pInfo->nBackfill;

      pInfo->nBackfillAttempted = mxSafeFrame;

      /* Sync the WAL to disk */
      rc = sqlite3OsSync(pWal->pWalFd, CKPT_SYNC_FLAGS(sync_flags));

      /* If the database may grow as a result of this checkpoint, hint
      ** about the eventual size of the db file to the VFS layer.
      */
      if( rc==SQLITE_OK ){
        i64 nReq = ((i64)mxPage * szPage);
        i64 nSize;                    /* Current size of database file */
        rc = sqlite3OsFileSize(pWal->pDbFd, &nSize);
        if( rc==SQLITE_OK && nSize<nReq ){
          sqlite3OsFileControlHint(pWal->pDbFd, SQLITE_FCNTL_SIZE_HINT, &nReq);
        }
      }


      /* Iterate through the contents of the WAL, copying data to the db file */
      while( rc==SQLITE_OK && 0==walIteratorNext(pIter, &iDbpage, &iFrame) ){
        i64 iOffset;
        assert( walFramePgno(pWal, iFrame)==iDbpage );
        if( db->u1.isInterrupted ){
          rc = db->mallocFailed ? SQLITE_NOMEM_BKPT : SQLITE_INTERRUPT;
          break;
        }
        if( iFrame<=nBackfill || iFrame>mxSafeFrame || iDbpage>mxPage ){
          continue;
        }
        iOffset = walFrameOffset(iFrame, szPage) + WAL_FRAME_HDRSIZE;
        /* testcase( IS_BIG_INT(iOffset) ); // requires a 4GiB WAL file */
        rc = sqlite3OsRead(pWal->pWalFd, zBuf, szPage, iOffset);
        if( rc!=SQLITE_OK ) break;
        iOffset = (iDbpage-1)*(i64)szPage;
        testcase( IS_BIG_INT(iOffset) );
        rc = sqlite3OsWrite(pWal->pDbFd, zBuf, szPage, iOffset);
        if( rc!=SQLITE_OK ) break;
      }

      /* If work was actually accomplished... */
      if( rc==SQLITE_OK ){
        if( mxSafeFrame==walIndexHdr(pWal)->mxFrame ){
          i64 szDb = pWal->hdr.nPage*(i64)szPage;
          testcase( IS_BIG_INT(szDb) );
          rc = sqlite3OsTruncate(pWal->pDbFd, szDb);
          if( rc==SQLITE_OK ){
            rc = sqlite3OsSync(pWal->pDbFd, CKPT_SYNC_FLAGS(sync_flags));
          }
        }
        if( rc==SQLITE_OK ){
          pInfo->nBackfill = mxSafeFrame;
        }
      }

      /* Release the reader lock held while backfilling */
      walUnlockExclusive(pWal, WAL_READ_LOCK(0), 1);
    }

    if( rc==SQLITE_BUSY ){
      /* Reset the return code so as not to report a checkpoint failure
      ** just because there are active readers.  */
      rc = SQLITE_OK;
    }
  }

  /* If this is an SQLITE_CHECKPOINT_RESTART or TRUNCATE operation, and the
  ** entire wal file has been copied into the database file, then block 
  ** until all readers have finished using the wal file. This ensures that 
  ** the next process to write to the database restarts the wal file.
  */
  if( rc==SQLITE_OK && eMode!=SQLITE_CHECKPOINT_PASSIVE ){
    assert( pWal->writeLock );
    if( pInfo->nBackfill<pWal->hdr.mxFrame ){
      rc = SQLITE_BUSY;
    }else if( eMode>=SQLITE_CHECKPOINT_RESTART ){
      u32 salt1;
      sqlite3_randomness(4, &salt1);
      assert( pInfo->nBackfill==pWal->hdr.mxFrame );
      rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_READ_LOCK(1), WAL_NREADER-1);
      if( rc==SQLITE_OK ){
        if( eMode==SQLITE_CHECKPOINT_TRUNCATE ){
          /* IMPLEMENTATION-OF: R-44699-57140 This mode works the same way as
          ** SQLITE_CHECKPOINT_RESTART with the addition that it also
          ** truncates the log file to zero bytes just prior to a
          ** successful return.
          **
          ** In theory, it might be safe to do this without updating the
          ** wal-index header in shared memory, as all subsequent reader or
          ** writer clients should see that the entire log file has been
          ** checkpointed and behave accordingly. This seems unsafe though,
          ** as it would leave the system in a state where the contents of
          ** the wal-index header do not match the contents of the 
          ** file-system. To avoid this, update the wal-index header to
          ** indicate that the log file contains zero valid frames.  */
          walRestartHdr(pWal, salt1);
          rc = sqlite3OsTruncate(pWal->pWalFd, 0);
        }
        walUnlockExclusive(pWal, WAL_READ_LOCK(1), WAL_NREADER-1);
      }
    }
  }

 walcheckpoint_out:
  walIteratorFree(pIter);
  return rc;
}

/*
** If the WAL file is currently larger than nMax bytes in size, truncate
** it to exactly nMax bytes. If an error occurs while doing so, ignore it.
*/
static void walLimitSize(Wal *pWal, i64 nMax){
  i64 sz;
  int rx;
  sqlite3BeginBenignMalloc();
  rx = sqlite3OsFileSize(pWal->pWalFd, &sz);
  if( rx==SQLITE_OK && (sz > nMax ) ){
    rx = sqlite3OsTruncate(pWal->pWalFd, nMax);
  }
  sqlite3EndBenignMalloc();
  if( rx ){
    sqlite3_log(rx, "cannot limit WAL size: %s", pWal->zWalName);
  }
}

/*
** Close a connection to a log file.
*/
SQLITE_PRIVATE int sqlite3WalClose(
  Wal *pWal,                      /* Wal to close */
  sqlite3 *db,                    /* For interrupt flag */
  int sync_flags,                 /* Flags to pass to OsSync() (or 0) */
  int nBuf,
  u8 *zBuf                        /* Buffer of at least nBuf bytes */
){
  int rc = SQLITE_OK;
  if( pWal ){
    int isDelete = 0;             /* True to unlink wal and wal-index files */

    /* If an EXCLUSIVE lock can be obtained on the database file (using the
    ** ordinary, rollback-mode locking methods, this guarantees that the
    ** connection associated with this log file is the only connection to
    ** the database. In this case checkpoint the database and unlink both
    ** the wal and wal-index files.
    **
    ** The EXCLUSIVE lock is not released before returning.
    */
    if( zBuf!=0
     && SQLITE_OK==(rc = sqlite3OsLock(pWal->pDbFd, SQLITE_LOCK_EXCLUSIVE))
    ){
      if( pWal->exclusiveMode==WAL_NORMAL_MODE ){
        pWal->exclusiveMode = WAL_EXCLUSIVE_MODE;
      }
      rc = sqlite3WalCheckpoint(pWal, db, 
          SQLITE_CHECKPOINT_PASSIVE, 0, 0, sync_flags, nBuf, zBuf, 0, 0
      );
      if( rc==SQLITE_OK ){
        int bPersist = -1;
        sqlite3OsFileControlHint(
            pWal->pDbFd, SQLITE_FCNTL_PERSIST_WAL, &bPersist
        );
        if( bPersist!=1 ){
          /* Try to delete the WAL file if the checkpoint completed and
          ** fsyned (rc==SQLITE_OK) and if we are not in persistent-wal
          ** mode (!bPersist) */
          isDelete = 1;
        }else if( pWal->mxWalSize>=0 ){
          /* Try to truncate the WAL file to zero bytes if the checkpoint
          ** completed and fsynced (rc==SQLITE_OK) and we are in persistent
          ** WAL mode (bPersist) and if the PRAGMA journal_size_limit is a
          ** non-negative value (pWal->mxWalSize>=0).  Note that we truncate
          ** to zero bytes as truncating to the journal_size_limit might
          ** leave a corrupt WAL file on disk. */
          walLimitSize(pWal, 0);
        }
      }
    }

    walIndexClose(pWal, isDelete);
    sqlite3OsClose(pWal->pWalFd);
    if( isDelete ){
      sqlite3BeginBenignMalloc();
      sqlite3OsDelete(pWal->pVfs, pWal->zWalName, 0);
      sqlite3EndBenignMalloc();
    }
    WALTRACE(("WAL%p: closed\n", pWal));
    sqlite3_free((void *)pWal->apWiData);
    sqlite3_free(pWal);
  }
  return rc;
}

/*
** Try to read the wal-index header.  Return 0 on success and 1 if
** there is a problem.
**
** The wal-index is in shared memory.  Another thread or process might
** be writing the header at the same time this procedure is trying to
** read it, which might result in inconsistency.  A dirty read is detected
** by verifying that both copies of the header are the same and also by
** a checksum on the header.
**
** If and only if the read is consistent and the header is different from
** pWal->hdr, then pWal->hdr is updated to the content of the new header
** and *pChanged is set to 1.
**
** If the checksum cannot be verified return non-zero. If the header
** is read successfully and the checksum verified, return zero.
*/
static int walIndexTryHdr(Wal *pWal, int *pChanged){
  u32 aCksum[2];                  /* Checksum on the header content */
  WalIndexHdr h1, h2;             /* Two copies of the header content */
  WalIndexHdr volatile *aHdr;     /* Header in shared memory */

  /* The first page of the wal-index must be mapped at this point. */
  assert( pWal->nWiData>0 && pWal->apWiData[0] );

  /* Read the header. This might happen concurrently with a write to the
  ** same area of shared memory on a different CPU in a SMP,
  ** meaning it is possible that an inconsistent snapshot is read
  ** from the file. If this happens, return non-zero.
  **
  ** There are two copies of the header at the beginning of the wal-index.
  ** When reading, read [0] first then [1].  Writes are in the reverse order.
  ** Memory barriers are used to prevent the compiler or the hardware from
  ** reordering the reads and writes.
  */
  aHdr = walIndexHdr(pWal);
  memcpy(&h1, (void *)&aHdr[0], sizeof(h1));
  walShmBarrier(pWal);
  memcpy(&h2, (void *)&aHdr[1], sizeof(h2));

  if( memcmp(&h1, &h2, sizeof(h1))!=0 ){
    return 1;   /* Dirty read */
  }  
  if( h1.isInit==0 ){
    return 1;   /* Malformed header - probably all zeros */
  }
  walChecksumBytes(1, (u8*)&h1, sizeof(h1)-sizeof(h1.aCksum), 0, aCksum);
  if( aCksum[0]!=h1.aCksum[0] || aCksum[1]!=h1.aCksum[1] ){
    return 1;   /* Checksum does not match */
  }

  if( memcmp(&pWal->hdr, &h1, sizeof(WalIndexHdr)) ){
    *pChanged = 1;
    memcpy(&pWal->hdr, &h1, sizeof(WalIndexHdr));
    pWal->szPage = (pWal->hdr.szPage&0xfe00) + ((pWal->hdr.szPage&0x0001)<<16);
    testcase( pWal->szPage<=32768 );
    testcase( pWal->szPage>=65536 );
  }

  /* The header was successfully read. Return zero. */
  return 0;
}

/*
** This is the value that walTryBeginRead returns when it needs to
** be retried.
*/
#define WAL_RETRY  (-1)

/*
** Read the wal-index header from the wal-index and into pWal->hdr.
** If the wal-header appears to be corrupt, try to reconstruct the
** wal-index from the WAL before returning.
**
** Set *pChanged to 1 if the wal-index header value in pWal->hdr is
** changed by this operation.  If pWal->hdr is unchanged, set *pChanged
** to 0.
**
** If the wal-index header is successfully read, return SQLITE_OK. 
** Otherwise an SQLite error code.
*/
static int walIndexReadHdr(Wal *pWal, int *pChanged){
  int rc;                         /* Return code */
  int badHdr;                     /* True if a header read failed */
  volatile u32 *page0;            /* Chunk of wal-index containing header */

  /* Ensure that page 0 of the wal-index (the page that contains the 
  ** wal-index header) is mapped. Return early if an error occurs here.
  */
  assert( pChanged );
  rc = walIndexPage(pWal, 0, &page0);
  if( rc!=SQLITE_OK ){
    assert( rc!=SQLITE_READONLY ); /* READONLY changed to OK in walIndexPage */
    if( rc==SQLITE_READONLY_CANTINIT ){
      /* The SQLITE_READONLY_CANTINIT return means that the shared-memory
      ** was openable but is not writable, and this thread is unable to
      ** confirm that another write-capable connection has the shared-memory
      ** open, and hence the content of the shared-memory is unreliable,
      ** since the shared-memory might be inconsistent with the WAL file
      ** and there is no writer on hand to fix it. */
      assert( page0==0 );
      assert( pWal->writeLock==0 );
      assert( pWal->readOnly & WAL_SHM_RDONLY );
      pWal->bShmUnreliable = 1;
      pWal->exclusiveMode = WAL_HEAPMEMORY_MODE;
      *pChanged = 1;
    }else{
      return rc; /* Any other non-OK return is just an error */
    }
  }else{
    /* page0 can be NULL if the SHM is zero bytes in size and pWal->writeLock
    ** is zero, which prevents the SHM from growing */
    testcase( page0!=0 );
  }
  assert( page0!=0 || pWal->writeLock==0 );

  /* If the first page of the wal-index has been mapped, try to read the
  ** wal-index header immediately, without holding any lock. This usually
  ** works, but may fail if the wal-index header is corrupt or currently 
  ** being modified by another thread or process.
  */
  badHdr = (page0 ? walIndexTryHdr(pWal, pChanged) : 1);

  /* If the first attempt failed, it might have been due to a race
  ** with a writer.  So get a WRITE lock and try again.
  */
  assert( badHdr==0 || pWal->writeLock==0 );
  if( badHdr ){
    if( pWal->bShmUnreliable==0 && (pWal->readOnly & WAL_SHM_RDONLY) ){
      if( SQLITE_OK==(rc = walLockShared(pWal, WAL_WRITE_LOCK)) ){
        walUnlockShared(pWal, WAL_WRITE_LOCK);
        rc = SQLITE_READONLY_RECOVERY;
      }
    }else if( SQLITE_OK==(rc = walLockExclusive(pWal, WAL_WRITE_LOCK, 1)) ){
      pWal->writeLock = 1;
      if( SQLITE_OK==(rc = walIndexPage(pWal, 0, &page0)) ){
        badHdr = walIndexTryHdr(pWal, pChanged);
        if( badHdr ){
          /* If the wal-index header is still malformed even while holding
          ** a WRITE lock, it can only mean that the header is corrupted and
          ** needs to be reconstructed.  So run recovery to do exactly that.
          */
          rc = walIndexRecover(pWal);
          *pChanged = 1;
        }
      }
      pWal->writeLock = 0;
      walUnlockExclusive(pWal, WAL_WRITE_LOCK, 1);
    }
  }

  /* If the header is read successfully, check the version number to make
  ** sure the wal-index was not constructed with some future format that
  ** this version of SQLite cannot understand.
  */
  if( badHdr==0 && pWal->hdr.iVersion!=WALINDEX_MAX_VERSION ){
    rc = SQLITE_CANTOPEN_BKPT;
  }
  if( pWal->bShmUnreliable ){
    if( rc!=SQLITE_OK ){
      walIndexClose(pWal, 0);
      pWal->bShmUnreliable = 0;
      assert( pWal->nWiData>0 && pWal->apWiData[0]==0 );
      /* walIndexRecover() might have returned SHORT_READ if a concurrent
      ** writer truncated the WAL out from under it.  If that happens, it
      ** indicates that a writer has fixed the SHM file for us, so retry */
      if( rc==SQLITE_IOERR_SHORT_READ ) rc = WAL_RETRY;
    }
    pWal->exclusiveMode = WAL_NORMAL_MODE;
  }

  return rc;
}

/*
** Open a transaction in a connection where the shared-memory is read-only
** and where we cannot verify that there is a separate write-capable connection
** on hand to keep the shared-memory up-to-date with the WAL file.
**
** This can happen, for example, when the shared-memory is implemented by
** memory-mapping a *-shm file, where a prior writer has shut down and
** left the *-shm file on disk, and now the present connection is trying
** to use that database but lacks write permission on the *-shm file.
** Other scenarios are also possible, depending on the VFS implementation.
**
** Precondition:
**
**    The *-wal file has been read and an appropriate wal-index has been
**    constructed in pWal->apWiData[] using heap memory instead of shared
**    memory. 
**
** If this function returns SQLITE_OK, then the read transaction has
** been successfully opened. In this case output variable (*pChanged) 
** is set to true before returning if the caller should discard the
** contents of the page cache before proceeding. Or, if it returns 
** WAL_RETRY, then the heap memory wal-index has been discarded and 
** the caller should retry opening the read transaction from the 
** beginning (including attempting to map the *-shm file). 
**
** If an error occurs, an SQLite error code is returned.
*/
static int walBeginShmUnreliable(Wal *pWal, int *pChanged){
  i64 szWal;                      /* Size of wal file on disk in bytes */
  i64 iOffset;                    /* Current offset when reading wal file */
  u8 aBuf[WAL_HDRSIZE];           /* Buffer to load WAL header into */
  u8 *aFrame = 0;                 /* Malloc'd buffer to load entire frame */
  int szFrame;                    /* Number of bytes in buffer aFrame[] */
  u8 *aData;                      /* Pointer to data part of aFrame buffer */
  volatile void *pDummy;          /* Dummy argument for xShmMap */
  int rc;                         /* Return code */
  u32 aSaveCksum[2];              /* Saved copy of pWal->hdr.aFrameCksum */

  assert( pWal->bShmUnreliable );
  assert( pWal->readOnly & WAL_SHM_RDONLY );
  assert( pWal->nWiData>0 && pWal->apWiData[0] );

  /* Take WAL_READ_LOCK(0). This has the effect of preventing any
  ** writers from running a checkpoint, but does not stop them
  ** from running recovery.  */
  rc = walLockShared(pWal, WAL_READ_LOCK(0));
  if( rc!=SQLITE_OK ){
    if( rc==SQLITE_BUSY ) rc = WAL_RETRY;
    goto begin_unreliable_shm_out;
  }
  pWal->readLock = 0;

  /* Check to see if a separate writer has attached to the shared-memory area,
  ** thus making the shared-memory "reliable" again.  Do this by invoking
  ** the xShmMap() routine of the VFS and looking to see if the return
  ** is SQLITE_READONLY instead of SQLITE_READONLY_CANTINIT.
  **
  ** If the shared-memory is now "reliable" return WAL_RETRY, which will
  ** cause the heap-memory WAL-index to be discarded and the actual
  ** shared memory to be used in its place.
  **
  ** This step is important because, even though this connection is holding
  ** the WAL_READ_LOCK(0) which prevents a checkpoint, a writer might
  ** have already checkpointed the WAL file and, while the current
  ** is active, wrap the WAL and start overwriting frames that this
  ** process wants to use.
  **
  ** Once sqlite3OsShmMap() has been called for an sqlite3_file and has
  ** returned any SQLITE_READONLY value, it must return only SQLITE_READONLY
  ** or SQLITE_READONLY_CANTINIT or some error for all subsequent invocations,
  ** even if some external agent does a "chmod" to make the shared-memory
  ** writable by us, until sqlite3OsShmUnmap() has been called.
  ** This is a requirement on the VFS implementation.
   */
  rc = sqlite3OsShmMap(pWal->pDbFd, 0, WALINDEX_PGSZ, 0, &pDummy);
  assert( rc!=SQLITE_OK ); /* SQLITE_OK not possible for read-only connection */
  if( rc!=SQLITE_READONLY_CANTINIT ){
    rc = (rc==SQLITE_READONLY ? WAL_RETRY : rc);
    goto begin_unreliable_shm_out;
  }

  /* We reach this point only if the real shared-memory is still unreliable.
  ** Assume the in-memory WAL-index substitute is correct and load it
  ** into pWal->hdr.
  */
  memcpy(&pWal->hdr, (void*)walIndexHdr(pWal), sizeof(WalIndexHdr));

  /* Make sure some writer hasn't come in and changed the WAL file out
  ** from under us, then disconnected, while we were not looking.
  */
  rc = sqlite3OsFileSize(pWal->pWalFd, &szWal);
  if( rc!=SQLITE_OK ){
    goto begin_unreliable_shm_out;
  }
  if( szWal<WAL_HDRSIZE ){
    /* If the wal file is too small to contain a wal-header and the
    ** wal-index header has mxFrame==0, then it must be safe to proceed
    ** reading the database file only. However, the page cache cannot
    ** be trusted, as a read/write connection may have connected, written
    ** the db, run a checkpoint, truncated the wal file and disconnected
    ** since this client's last read transaction.  */
    *pChanged = 1;
    rc = (pWal->hdr.mxFrame==0 ? SQLITE_OK : WAL_RETRY);
    goto begin_unreliable_shm_out;
  }

  /* Check the salt keys at the start of the wal file still match. */
  rc = sqlite3OsRead(pWal->pWalFd, aBuf, WAL_HDRSIZE, 0);
  if( rc!=SQLITE_OK ){
    goto begin_unreliable_shm_out;
  }
  if( memcmp(&pWal->hdr.aSalt, &aBuf[16], 8) ){
    /* Some writer has wrapped the WAL file while we were not looking.
    ** Return WAL_RETRY which will cause the in-memory WAL-index to be
    ** rebuilt. */
    rc = WAL_RETRY;
    goto begin_unreliable_shm_out;
  }

  /* Allocate a buffer to read frames into */
  szFrame = pWal->hdr.szPage + WAL_FRAME_HDRSIZE;
  aFrame = (u8 *)sqlite3_malloc64(szFrame);
  if( aFrame==0 ){
    rc = SQLITE_NOMEM_BKPT;
    goto begin_unreliable_shm_out;
  }
  aData = &aFrame[WAL_FRAME_HDRSIZE];

  /* Check to see if a complete transaction has been appended to the
  ** wal file since the heap-memory wal-index was created. If so, the
  ** heap-memory wal-index is discarded and WAL_RETRY returned to
  ** the caller.  */
  aSaveCksum[0] = pWal->hdr.aFrameCksum[0];
  aSaveCksum[1] = pWal->hdr.aFrameCksum[1];
  for(iOffset=walFrameOffset(pWal->hdr.mxFrame+1, pWal->hdr.szPage); 
      iOffset+szFrame<=szWal; 
      iOffset+=szFrame
  ){
    u32 pgno;                   /* Database page number for frame */
    u32 nTruncate;              /* dbsize field from frame header */

    /* Read and decode the next log frame. */
    rc = sqlite3OsRead(pWal->pWalFd, aFrame, szFrame, iOffset);
    if( rc!=SQLITE_OK ) break;
    if( !walDecodeFrame(pWal, &pgno, &nTruncate, aData, aFrame) ) break;

    /* If nTruncate is non-zero, then a complete transaction has been
    ** appended to this wal file. Set rc to WAL_RETRY and break out of
    ** the loop.  */
    if( nTruncate ){
      rc = WAL_RETRY;
      break;
    }
  }
  pWal->hdr.aFrameCksum[0] = aSaveCksum[0];
  pWal->hdr.aFrameCksum[1] = aSaveCksum[1];

 begin_unreliable_shm_out:
  sqlite3_free(aFrame);
  if( rc!=SQLITE_OK ){
    int i;
    for(i=0; i<pWal->nWiData; i++){
      sqlite3_free((void*)pWal->apWiData[i]);
      pWal->apWiData[i] = 0;
    }
    pWal->bShmUnreliable = 0;
    sqlite3WalEndReadTransaction(pWal);
    *pChanged = 1;
  }
  return rc;
}

/*
** Attempt to start a read transaction.  This might fail due to a race or
** other transient condition.  When that happens, it returns WAL_RETRY to
** indicate to the caller that it is safe to retry immediately.
**
** On success return SQLITE_OK.  On a permanent failure (such an
** I/O error or an SQLITE_BUSY because another process is running
** recovery) return a positive error code.
**
** The useWal parameter is true to force the use of the WAL and disable
** the case where the WAL is bypassed because it has been completely
** checkpointed.  If useWal==0 then this routine calls walIndexReadHdr() 
** to make a copy of the wal-index header into pWal->hdr.  If the 
** wal-index header has changed, *pChanged is set to 1 (as an indication 
** to the caller that the local page cache is obsolete and needs to be 
** flushed.)  When useWal==1, the wal-index header is assumed to already
** be loaded and the pChanged parameter is unused.
**
** The caller must set the cnt parameter to the number of prior calls to
** this routine during the current read attempt that returned WAL_RETRY.
** This routine will start taking more aggressive measures to clear the
** race conditions after multiple WAL_RETRY returns, and after an excessive
** number of errors will ultimately return SQLITE_PROTOCOL.  The
** SQLITE_PROTOCOL return indicates that some other process has gone rogue
** and is not honoring the locking protocol.  There is a vanishingly small
** chance that SQLITE_PROTOCOL could be returned because of a run of really
** bad luck when there is lots of contention for the wal-index, but that
** possibility is so small that it can be safely neglected, we believe.
**
** On success, this routine obtains a read lock on 
** WAL_READ_LOCK(pWal->readLock).  The pWal->readLock integer is
** in the range 0 <= pWal->readLock < WAL_NREADER.  If pWal->readLock==(-1)
** that means the Wal does not hold any read lock.  The reader must not
** access any database page that is modified by a WAL frame up to and
** including frame number aReadMark[pWal->readLock].  The reader will
** use WAL frames up to and including pWal->hdr.mxFrame if pWal->readLock>0
** Or if pWal->readLock==0, then the reader will ignore the WAL
** completely and get all content directly from the database file.
** If the useWal parameter is 1 then the WAL will never be ignored and
** this routine will always set pWal->readLock>0 on success.
** When the read transaction is completed, the caller must release the
** lock on WAL_READ_LOCK(pWal->readLock) and set pWal->readLock to -1.
**
** This routine uses the nBackfill and aReadMark[] fields of the header
** to select a particular WAL_READ_LOCK() that strives to let the
** checkpoint process do as much work as possible.  This routine might
** update values of the aReadMark[] array in the header, but if it does
** so it takes care to hold an exclusive lock on the corresponding
** WAL_READ_LOCK() while changing values.
*/
static int walTryBeginRead(Wal *pWal, int *pChanged, int useWal, int cnt){
  volatile WalCkptInfo *pInfo;    /* Checkpoint information in wal-index */
  u32 mxReadMark;                 /* Largest aReadMark[] value */
  int mxI;                        /* Index of largest aReadMark[] value */
  int i;                          /* Loop counter */
  int rc = SQLITE_OK;             /* Return code  */
  u32 mxFrame;                    /* Wal frame to lock to */

  assert( pWal->readLock<0 );     /* Not currently locked */

  /* useWal may only be set for read/write connections */
  assert( (pWal->readOnly & WAL_SHM_RDONLY)==0 || useWal==0 );

  /* Take steps to avoid spinning forever if there is a protocol error.
  **
  ** Circumstances that cause a RETRY should only last for the briefest
  ** instances of time.  No I/O or other system calls are done while the
  ** locks are held, so the locks should not be held for very long. But 
  ** if we are unlucky, another process that is holding a lock might get
  ** paged out or take a page-fault that is time-consuming to resolve, 
  ** during the few nanoseconds that it is holding the lock.  In that case,
  ** it might take longer than normal for the lock to free.
  **
  ** After 5 RETRYs, we begin calling sqlite3OsSleep().  The first few
  ** calls to sqlite3OsSleep() have a delay of 1 microsecond.  Really this
  ** is more of a scheduler yield than an actual delay.  But on the 10th
  ** an subsequent retries, the delays start becoming longer and longer, 
  ** so that on the 100th (and last) RETRY we delay for 323 milliseconds.
  ** The total delay time before giving up is less than 10 seconds.
  */
  if( cnt>5 ){
    int nDelay = 1;                      /* Pause time in microseconds */
    if( cnt>100 ){
      VVA_ONLY( pWal->lockError = 1; )
      return SQLITE_PROTOCOL;
    }
    if( cnt>=10 ) nDelay = (cnt-9)*(cnt-9)*39;
    sqlite3OsSleep(pWal->pVfs, nDelay);
  }

  if( !useWal ){
    assert( rc==SQLITE_OK );
    if( pWal->bShmUnreliable==0 ){
      rc = walIndexReadHdr(pWal, pChanged);
    }
    if( rc==SQLITE_BUSY ){
      /* If there is not a recovery running in another thread or process
      ** then convert BUSY errors to WAL_RETRY.  If recovery is known to
      ** be running, convert BUSY to BUSY_RECOVERY.  There is a race here
      ** which might cause WAL_RETRY to be returned even if BUSY_RECOVERY
      ** would be technically correct.  But the race is benign since with
      ** WAL_RETRY this routine will be called again and will probably be
      ** right on the second iteration.
      */
      if( pWal->apWiData[0]==0 ){
        /* This branch is taken when the xShmMap() method returns SQLITE_BUSY.
        ** We assume this is a transient condition, so return WAL_RETRY. The
        ** xShmMap() implementation used by the default unix and win32 VFS 
        ** modules may return SQLITE_BUSY due to a race condition in the 
        ** code that determines whether or not the shared-memory region 
        ** must be zeroed before the requested page is returned.
        */
        rc = WAL_RETRY;
      }else if( SQLITE_OK==(rc = walLockShared(pWal, WAL_RECOVER_LOCK)) ){
        walUnlockShared(pWal, WAL_RECOVER_LOCK);
        rc = WAL_RETRY;
      }else if( rc==SQLITE_BUSY ){
        rc = SQLITE_BUSY_RECOVERY;
      }
    }
    if( rc!=SQLITE_OK ){
      return rc;
    }
    else if( pWal->bShmUnreliable ){
      return walBeginShmUnreliable(pWal, pChanged);
    }
  }

  assert( pWal->nWiData>0 );
  assert( pWal->apWiData[0]!=0 );
  pInfo = walCkptInfo(pWal);
  if( !useWal && pInfo->nBackfill==pWal->hdr.mxFrame
#ifdef SQLITE_ENABLE_SNAPSHOT
   && (pWal->pSnapshot==0 || pWal->hdr.mxFrame==0)
#endif
  ){
    /* The WAL has been completely backfilled (or it is empty).
    ** and can be safely ignored.
    */
    rc = walLockShared(pWal, WAL_READ_LOCK(0));
    walShmBarrier(pWal);
    if( rc==SQLITE_OK ){
      if( memcmp((void *)walIndexHdr(pWal), &pWal->hdr, sizeof(WalIndexHdr)) ){
        /* It is not safe to allow the reader to continue here if frames
        ** may have been appended to the log before READ_LOCK(0) was obtained.
        ** When holding READ_LOCK(0), the reader ignores the entire log file,
        ** which implies that the database file contains a trustworthy
        ** snapshot. Since holding READ_LOCK(0) prevents a checkpoint from
        ** happening, this is usually correct.
        **
        ** However, if frames have been appended to the log (or if the log 
        ** is wrapped and written for that matter) before the READ_LOCK(0)
        ** is obtained, that is not necessarily true. A checkpointer may
        ** have started to backfill the appended frames but crashed before
        ** it finished. Leaving a corrupt image in the database file.
        */
        walUnlockShared(pWal, WAL_READ_LOCK(0));
        return WAL_RETRY;
      }
      pWal->readLock = 0;
      return SQLITE_OK;
    }else if( rc!=SQLITE_BUSY ){
      return rc;
    }
  }

  /* If we get this far, it means that the reader will want to use
  ** the WAL to get at content from recent commits.  The job now is
  ** to select one of the aReadMark[] entries that is closest to
  ** but not exceeding pWal->hdr.mxFrame and lock that entry.
  */
  mxReadMark = 0;
  mxI = 0;
  mxFrame = pWal->hdr.mxFrame;
#ifdef SQLITE_ENABLE_SNAPSHOT
  if( pWal->pSnapshot && pWal->pSnapshot->mxFrame<mxFrame ){
    mxFrame = pWal->pSnapshot->mxFrame;
  }
#endif
  for(i=1; i<WAL_NREADER; i++){
    u32 thisMark = AtomicLoad(pInfo->aReadMark+i);
    if( mxReadMark<=thisMark && thisMark<=mxFrame ){
      assert( thisMark!=READMARK_NOT_USED );
      mxReadMark = thisMark;
      mxI = i;
    }
  }
  if( (pWal->readOnly & WAL_SHM_RDONLY)==0
   && (mxReadMark<mxFrame || mxI==0)
  ){
    for(i=1; i<WAL_NREADER; i++){
      rc = walLockExclusive(pWal, WAL_READ_LOCK(i), 1);
      if( rc==SQLITE_OK ){
        mxReadMark = AtomicStore(pInfo->aReadMark+i,mxFrame);
        mxI = i;
        walUnlockExclusive(pWal, WAL_READ_LOCK(i), 1);
        break;
      }else if( rc!=SQLITE_BUSY ){
        return rc;
      }
    }
  }
  if( mxI==0 ){
    assert( rc==SQLITE_BUSY || (pWal->readOnly & WAL_SHM_RDONLY)!=0 );
    return rc==SQLITE_BUSY ? WAL_RETRY : SQLITE_READONLY_CANTINIT;
  }

  rc = walLockShared(pWal, WAL_READ_LOCK(mxI));
  if( rc ){
    return rc==SQLITE_BUSY ? WAL_RETRY : rc;
  }
  /* Now that the read-lock has been obtained, check that neither the
  ** value in the aReadMark[] array or the contents of the wal-index
  ** header have changed.
  **
  ** It is necessary to check that the wal-index header did not change
  ** between the time it was read and when the shared-lock was obtained
  ** on WAL_READ_LOCK(mxI) was obtained to account for the possibility
  ** that the log file may have been wrapped by a writer, or that frames
  ** that occur later in the log than pWal->hdr.mxFrame may have been
  ** copied into the database by a checkpointer. If either of these things
  ** happened, then reading the database with the current value of
  ** pWal->hdr.mxFrame risks reading a corrupted snapshot. So, retry
  ** instead.
  **
  ** Before checking that the live wal-index header has not changed
  ** since it was read, set Wal.minFrame to the first frame in the wal
  ** file that has not yet been checkpointed. This client will not need
  ** to read any frames earlier than minFrame from the wal file - they
  ** can be safely read directly from the database file.
  **
  ** Because a ShmBarrier() call is made between taking the copy of 
  ** nBackfill and checking that the wal-header in shared-memory still
  ** matches the one cached in pWal->hdr, it is guaranteed that the 
  ** checkpointer that set nBackfill was not working with a wal-index
  ** header newer than that cached in pWal->hdr. If it were, that could
  ** cause a problem. The checkpointer could omit to checkpoint
  ** a version of page X that lies before pWal->minFrame (call that version
  ** A) on the basis that there is a newer version (version B) of the same
  ** page later in the wal file. But if version B happens to like past
  ** frame pWal->hdr.mxFrame - then the client would incorrectly assume
  ** that it can read version A from the database file. However, since
  ** we can guarantee that the checkpointer that set nBackfill could not
  ** see any pages past pWal->hdr.mxFrame, this problem does not come up.
  */
  pWal->minFrame = AtomicLoad(&pInfo->nBackfill)+1;
  walShmBarrier(pWal);
  if( AtomicLoad(pInfo->aReadMark+mxI)!=mxReadMark
   || memcmp((void *)walIndexHdr(pWal), &pWal->hdr, sizeof(WalIndexHdr))
  ){
    walUnlockShared(pWal, WAL_READ_LOCK(mxI));
    return WAL_RETRY;
  }else{
    assert( mxReadMark<=pWal->hdr.mxFrame );
    pWal->readLock = (i16)mxI;
  }
  return rc;
}

#ifdef SQLITE_ENABLE_SNAPSHOT
/*
** Attempt to reduce the value of the WalCkptInfo.nBackfillAttempted 
** variable so that older snapshots can be accessed. To do this, loop
** through all wal frames from nBackfillAttempted to (nBackfill+1), 
** comparing their content to the corresponding page with the database
** file, if any. Set nBackfillAttempted to the frame number of the
** first frame for which the wal file content matches the db file.
**
** This is only really safe if the file-system is such that any page 
** writes made by earlier checkpointers were atomic operations, which 
** is not always true. It is also possible that nBackfillAttempted
** may be left set to a value larger than expected, if a wal frame
** contains content that duplicate of an earlier version of the same
** page.
**
** SQLITE_OK is returned if successful, or an SQLite error code if an
** error occurs. It is not an error if nBackfillAttempted cannot be
** decreased at all.
*/
SQLITE_PRIVATE int sqlite3WalSnapshotRecover(Wal *pWal){
  int rc;

  assert( pWal->readLock>=0 );
  rc = walLockExclusive(pWal, WAL_CKPT_LOCK, 1);
  if( rc==SQLITE_OK ){
    volatile WalCkptInfo *pInfo = walCkptInfo(pWal);
    int szPage = (int)pWal->szPage;
    i64 szDb;                   /* Size of db file in bytes */

    rc = sqlite3OsFileSize(pWal->pDbFd, &szDb);
    if( rc==SQLITE_OK ){
      void *pBuf1 = sqlite3_malloc(szPage);
      void *pBuf2 = sqlite3_malloc(szPage);
      if( pBuf1==0 || pBuf2==0 ){
        rc = SQLITE_NOMEM;
      }else{
        u32 i = pInfo->nBackfillAttempted;
        for(i=pInfo->nBackfillAttempted; i>pInfo->nBackfill; i--){
          WalHashLoc sLoc;          /* Hash table location */
          u32 pgno;                 /* Page number in db file */
          i64 iDbOff;               /* Offset of db file entry */
          i64 iWalOff;              /* Offset of wal file entry */

          rc = walHashGet(pWal, walFramePage(i), &sLoc);
          if( rc!=SQLITE_OK ) break;
          pgno = sLoc.aPgno[i-sLoc.iZero];
          iDbOff = (i64)(pgno-1) * szPage;

          if( iDbOff+szPage<=szDb ){
            iWalOff = walFrameOffset(i, szPage) + WAL_FRAME_HDRSIZE;
            rc = sqlite3OsRead(pWal->pWalFd, pBuf1, szPage, iWalOff);

            if( rc==SQLITE_OK ){
              rc = sqlite3OsRead(pWal->pDbFd, pBuf2, szPage, iDbOff);
            }

            if( rc!=SQLITE_OK || 0==memcmp(pBuf1, pBuf2, szPage) ){
              break;
            }
          }

          pInfo->nBackfillAttempted = i-1;
        }
      }

      sqlite3_free(pBuf1);
      sqlite3_free(pBuf2);
    }
    walUnlockExclusive(pWal, WAL_CKPT_LOCK, 1);
  }

  return rc;
}
#endif /* SQLITE_ENABLE_SNAPSHOT */

/*
** Begin a read transaction on the database.
**
** This routine used to be called sqlite3OpenSnapshot() and with good reason:
** it takes a snapshot of the state of the WAL and wal-index for the current
** instant in time.  The current thread will continue to use this snapshot.
** Other threads might append new content to the WAL and wal-index but
** that extra content is ignored by the current thread.
**
** If the database contents have changes since the previous read
** transaction, then *pChanged is set to 1 before returning.  The
** Pager layer will use this to know that its cache is stale and
** needs to be flushed.
*/
SQLITE_PRIVATE int sqlite3WalBeginReadTransaction(Wal *pWal, int *pChanged){
  int rc;                         /* Return code */
  int cnt = 0;                    /* Number of TryBeginRead attempts */

#ifdef SQLITE_ENABLE_SNAPSHOT
  int bChanged = 0;
  WalIndexHdr *pSnapshot = pWal->pSnapshot;
  if( pSnapshot && memcmp(pSnapshot, &pWal->hdr, sizeof(WalIndexHdr))!=0 ){
    bChanged = 1;
  }
#endif

  do{
    rc = walTryBeginRead(pWal, pChanged, 0, ++cnt);
  }while( rc==WAL_RETRY );
  testcase( (rc&0xff)==SQLITE_BUSY );
  testcase( (rc&0xff)==SQLITE_IOERR );
  testcase( rc==SQLITE_PROTOCOL );
  testcase( rc==SQLITE_OK );

#ifdef SQLITE_ENABLE_SNAPSHOT
  if( rc==SQLITE_OK ){
    if( pSnapshot && memcmp(pSnapshot, &pWal->hdr, sizeof(WalIndexHdr))!=0 ){
      /* At this point the client has a lock on an aReadMark[] slot holding
      ** a value equal to or smaller than pSnapshot->mxFrame, but pWal->hdr
      ** is populated with the wal-index header corresponding to the head
      ** of the wal file. Verify that pSnapshot is still valid before
      ** continuing.  Reasons why pSnapshot might no longer be valid:
      **
      **    (1)  The WAL file has been reset since the snapshot was taken.
      **         In this case, the salt will have changed.
      **
      **    (2)  A checkpoint as been attempted that wrote frames past
      **         pSnapshot->mxFrame into the database file.  Note that the
      **         checkpoint need not have completed for this to cause problems.
      */
      volatile WalCkptInfo *pInfo = walCkptInfo(pWal);

      assert( pWal->readLock>0 || pWal->hdr.mxFrame==0 );
      assert( pInfo->aReadMark[pWal->readLock]<=pSnapshot->mxFrame );

      /* It is possible that there is a checkpointer thread running 
      ** concurrent with this code. If this is the case, it may be that the
      ** checkpointer has already determined that it will checkpoint 
      ** snapshot X, where X is later in the wal file than pSnapshot, but 
      ** has not yet set the pInfo->nBackfillAttempted variable to indicate 
      ** its intent. To avoid the race condition this leads to, ensure that
      ** there is no checkpointer process by taking a shared CKPT lock 
      ** before checking pInfo->nBackfillAttempted.  
      **
      ** TODO: Does the aReadMark[] lock prevent a checkpointer from doing
      **       this already?
      */
      rc = walLockShared(pWal, WAL_CKPT_LOCK);

      if( rc==SQLITE_OK ){
        /* Check that the wal file has not been wrapped. Assuming that it has
        ** not, also check that no checkpointer has attempted to checkpoint any
        ** frames beyond pSnapshot->mxFrame. If either of these conditions are
        ** true, return SQLITE_ERROR_SNAPSHOT. Otherwise, overwrite pWal->hdr
        ** with *pSnapshot and set *pChanged as appropriate for opening the
        ** snapshot.  */
        if( !memcmp(pSnapshot->aSalt, pWal->hdr.aSalt, sizeof(pWal->hdr.aSalt))
         && pSnapshot->mxFrame>=pInfo->nBackfillAttempted
        ){
          assert( pWal->readLock>0 );
          memcpy(&pWal->hdr, pSnapshot, sizeof(WalIndexHdr));
          *pChanged = bChanged;
        }else{
          rc = SQLITE_ERROR_SNAPSHOT;
        }

        /* Release the shared CKPT lock obtained above. */
        walUnlockShared(pWal, WAL_CKPT_LOCK);
        pWal->minFrame = 1;
      }


      if( rc!=SQLITE_OK ){
        sqlite3WalEndReadTransaction(pWal);
      }
    }
  }
#endif
  return rc;
}

/*
** Finish with a read transaction.  All this does is release the
** read-lock.
*/
SQLITE_PRIVATE void sqlite3WalEndReadTransaction(Wal *pWal){
  sqlite3WalEndWriteTransaction(pWal);
  if( pWal->readLock>=0 ){
    walUnlockShared(pWal, WAL_READ_LOCK(pWal->readLock));
    pWal->readLock = -1;
  }
}

/*
** Search the wal file for page pgno. If found, set *piRead to the frame that
** contains the page. Otherwise, if pgno is not in the wal file, set *piRead
** to zero.
**
** Return SQLITE_OK if successful, or an error code if an error occurs. If an
** error does occur, the final value of *piRead is undefined.
*/
SQLITE_PRIVATE int sqlite3WalFindFrame(
  Wal *pWal,                      /* WAL handle */
  Pgno pgno,                      /* Database page number to read data for */
  u32 *piRead                     /* OUT: Frame number (or zero) */
){
  u32 iRead = 0;                  /* If !=0, WAL frame to return data from */
  u32 iLast = pWal->hdr.mxFrame;  /* Last page in WAL for this reader */
  int iHash;                      /* Used to loop through N hash tables */
  int iMinHash;

  /* This routine is only be called from within a read transaction. */
  assert( pWal->readLock>=0 || pWal->lockError );

  /* If the "last page" field of the wal-index header snapshot is 0, then
  ** no data will be read from the wal under any circumstances. Return early
  ** in this case as an optimization.  Likewise, if pWal->readLock==0, 
  ** then the WAL is ignored by the reader so return early, as if the 
  ** WAL were empty.
  */
  if( iLast==0 || (pWal->readLock==0 && pWal->bShmUnreliable==0) ){
    *piRead = 0;
    return SQLITE_OK;
  }

  /* Search the hash table or tables for an entry matching page number
  ** pgno. Each iteration of the following for() loop searches one
  ** hash table (each hash table indexes up to HASHTABLE_NPAGE frames).
  **
  ** This code might run concurrently to the code in walIndexAppend()
  ** that adds entries to the wal-index (and possibly to this hash 
  ** table). This means the value just read from the hash 
  ** slot (aHash[iKey]) may have been added before or after the 
  ** current read transaction was opened. Values added after the
  ** read transaction was opened may have been written incorrectly -
  ** i.e. these slots may contain garbage data. However, we assume
  ** that any slots written before the current read transaction was
  ** opened remain unmodified.
  **
  ** For the reasons above, the if(...) condition featured in the inner
  ** loop of the following block is more stringent that would be required 
  ** if we had exclusive access to the hash-table:
  **
  **   (aPgno[iFrame]==pgno): 
  **     This condition filters out normal hash-table collisions.
  **
  **   (iFrame<=iLast): 
  **     This condition filters out entries that were added to the hash
  **     table after the current read-transaction had started.
  */
  iMinHash = walFramePage(pWal->minFrame);
  for(iHash=walFramePage(iLast); iHash>=iMinHash; iHash--){
    WalHashLoc sLoc;              /* Hash table location */
    int iKey;                     /* Hash slot index */
    int nCollide;                 /* Number of hash collisions remaining */
    int rc;                       /* Error code */

    rc = walHashGet(pWal, iHash, &sLoc);
    if( rc!=SQLITE_OK ){
      return rc;
    }
    nCollide = HASHTABLE_NSLOT;
    for(iKey=walHash(pgno); sLoc.aHash[iKey]; iKey=walNextHash(iKey)){
      u32 iH = sLoc.aHash[iKey];
      u32 iFrame = iH + sLoc.iZero;
      if( iFrame<=iLast && iFrame>=pWal->minFrame && sLoc.aPgno[iH]==pgno ){
        assert( iFrame>iRead || CORRUPT_DB );
        iRead = iFrame;
      }
      if( (nCollide--)==0 ){
        return SQLITE_CORRUPT_BKPT;
      }
    }
    if( iRead ) break;
  }

#ifdef SQLITE_ENABLE_EXPENSIVE_ASSERT
  /* If expensive assert() statements are available, do a linear search
  ** of the wal-index file content. Make sure the results agree with the
  ** result obtained using the hash indexes above.  */
  {
    u32 iRead2 = 0;
    u32 iTest;
    assert( pWal->bShmUnreliable || pWal->minFrame>0 );
    for(iTest=iLast; iTest>=pWal->minFrame && iTest>0; iTest--){
      if( walFramePgno(pWal, iTest)==pgno ){
        iRead2 = iTest;
        break;
      }
    }
    assert( iRead==iRead2 );
  }
#endif

  *piRead = iRead;
  return SQLITE_OK;
}

/*
** Read the contents of frame iRead from the wal file into buffer pOut
** (which is nOut bytes in size). Return SQLITE_OK if successful, or an
** error code otherwise.
*/
SQLITE_PRIVATE int sqlite3WalReadFrame(
  Wal *pWal,                      /* WAL handle */
  u32 iRead,                      /* Frame to read */
  int nOut,                       /* Size of buffer pOut in bytes */
  u8 *pOut                        /* Buffer to write page data to */
){
  int sz;
  i64 iOffset;
  sz = pWal->hdr.szPage;
  sz = (sz&0xfe00) + ((sz&0x0001)<<16);
  testcase( sz<=32768 );
  testcase( sz>=65536 );
  iOffset = walFrameOffset(iRead, sz) + WAL_FRAME_HDRSIZE;
  /* testcase( IS_BIG_INT(iOffset) ); // requires a 4GiB WAL */
  return sqlite3OsRead(pWal->pWalFd, pOut, (nOut>sz ? sz : nOut), iOffset);
}

/* 
** Return the size of the database in pages (or zero, if unknown).
*/
SQLITE_PRIVATE Pgno sqlite3WalDbsize(Wal *pWal){
  if( pWal && ALWAYS(pWal->readLock>=0) ){
    return pWal->hdr.nPage;
  }
  return 0;
}


/* 
** This function starts a write transaction on the WAL.
**
** A read transaction must have already been started by a prior call
** to sqlite3WalBeginReadTransaction().
**
** If another thread or process has written into the database since
** the read transaction was started, then it is not possible for this
** thread to write as doing so would cause a fork.  So this routine
** returns SQLITE_BUSY in that case and no write transaction is started.
**
** There can only be a single writer active at a time.
*/
SQLITE_PRIVATE int sqlite3WalBeginWriteTransaction(Wal *pWal){
  int rc;

  /* Cannot start a write transaction without first holding a read
  ** transaction. */
  assert( pWal->readLock>=0 );
  assert( pWal->writeLock==0 && pWal->iReCksum==0 );

  if( pWal->readOnly ){
    return SQLITE_READONLY;
  }

  /* Only one writer allowed at a time.  Get the write lock.  Return
  ** SQLITE_BUSY if unable.
  */
  rc = walLockExclusive(pWal, WAL_WRITE_LOCK, 1);
  if( rc ){
    return rc;
  }
  pWal->writeLock = 1;

  /* If another connection has written to the database file since the
  ** time the read transaction on this connection was started, then
  ** the write is disallowed.
  */
  if( memcmp(&pWal->hdr, (void *)walIndexHdr(pWal), sizeof(WalIndexHdr))!=0 ){
    walUnlockExclusive(pWal, WAL_WRITE_LOCK, 1);
    pWal->writeLock = 0;
    rc = SQLITE_BUSY_SNAPSHOT;
  }

  return rc;
}

/*
** End a write transaction.  The commit has already been done.  This
** routine merely releases the lock.
*/
SQLITE_PRIVATE int sqlite3WalEndWriteTransaction(Wal *pWal){
  if( pWal->writeLock ){
    walUnlockExclusive(pWal, WAL_WRITE_LOCK, 1);
    pWal->writeLock = 0;
    pWal->iReCksum = 0;
    pWal->truncateOnCommit = 0;
  }
  return SQLITE_OK;
}

/*
** If any data has been written (but not committed) to the log file, this
** function moves the write-pointer back to the start of the transaction.
**
** Additionally, the callback function is invoked for each frame written
** to the WAL since the start of the transaction. If the callback returns
** other than SQLITE_OK, it is not invoked again and the error code is
** returned to the caller.
**
** Otherwise, if the callback function does not return an error, this
** function returns SQLITE_OK.
*/
SQLITE_PRIVATE int sqlite3WalUndo(Wal *pWal, int (*xUndo)(void *, Pgno), void *pUndoCtx){
  int rc = SQLITE_OK;
  if( ALWAYS(pWal->writeLock) ){
    Pgno iMax = pWal->hdr.mxFrame;
    Pgno iFrame;
  
    /* Restore the clients cache of the wal-index header to the state it
    ** was in before the client began writing to the database. 
    */
    memcpy(&pWal->hdr, (void *)walIndexHdr(pWal), sizeof(WalIndexHdr));

    for(iFrame=pWal->hdr.mxFrame+1; 
        ALWAYS(rc==SQLITE_OK) && iFrame<=iMax; 
        iFrame++
    ){
      /* This call cannot fail. Unless the page for which the page number
      ** is passed as the second argument is (a) in the cache and 
      ** (b) has an outstanding reference, then xUndo is either a no-op
      ** (if (a) is false) or simply expels the page from the cache (if (b)
      ** is false).
      **
      ** If the upper layer is doing a rollback, it is guaranteed that there
      ** are no outstanding references to any page other than page 1. And
      ** page 1 is never written to the log until the transaction is
      ** committed. As a result, the call to xUndo may not fail.
      */
      assert( walFramePgno(pWal, iFrame)!=1 );
      rc = xUndo(pUndoCtx, walFramePgno(pWal, iFrame));
    }
    if( iMax!=pWal->hdr.mxFrame ) walCleanupHash(pWal);
  }
  return rc;
}

/* 
** Argument aWalData must point to an array of WAL_SAVEPOINT_NDATA u32 
** values. This function populates the array with values required to 
** "rollback" the write position of the WAL handle back to the current 
** point in the event of a savepoint rollback (via WalSavepointUndo()).
*/
SQLITE_PRIVATE void sqlite3WalSavepoint(Wal *pWal, u32 *aWalData){
  assert( pWal->writeLock );
  aWalData[0] = pWal->hdr.mxFrame;
  aWalData[1] = pWal->hdr.aFrameCksum[0];
  aWalData[2] = pWal->hdr.aFrameCksum[1];
  aWalData[3] = pWal->nCkpt;
}

/* 
** Move the write position of the WAL back to the point identified by
** the values in the aWalData[] array. aWalData must point to an array
** of WAL_SAVEPOINT_NDATA u32 values that has been previously populated
** by a call to WalSavepoint().
*/
SQLITE_PRIVATE int sqlite3WalSavepointUndo(Wal *pWal, u32 *aWalData){
  int rc = SQLITE_OK;

  assert( pWal->writeLock );
  assert( aWalData[3]!=pWal->nCkpt || aWalData[0]<=pWal->hdr.mxFrame );

  if( aWalData[3]!=pWal->nCkpt ){
    /* This savepoint was opened immediately after the write-transaction
    ** was started. Right after that, the writer decided to wrap around
    ** to the start of the log. Update the savepoint values to match.
    */
    aWalData[0] = 0;
    aWalData[3] = pWal->nCkpt;
  }

  if( aWalData[0]<pWal->hdr.mxFrame ){
    pWal->hdr.mxFrame = aWalData[0];
    pWal->hdr.aFrameCksum[0] = aWalData[1];
    pWal->hdr.aFrameCksum[1] = aWalData[2];
    walCleanupHash(pWal);
  }

  return rc;
}

/*
** This function is called just before writing a set of frames to the log
** file (see sqlite3WalFrames()). It checks to see if, instead of appending
** to the current log file, it is possible to overwrite the start of the
** existing log file with the new frames (i.e. "reset" the log). If so,
** it sets pWal->hdr.mxFrame to 0. Otherwise, pWal->hdr.mxFrame is left
** unchanged.
**
** SQLITE_OK is returned if no error is encountered (regardless of whether
** or not pWal->hdr.mxFrame is modified). An SQLite error code is returned
** if an error occurs.
*/
static int walRestartLog(Wal *pWal){
  int rc = SQLITE_OK;
  int cnt;

  if( pWal->readLock==0 ){
    volatile WalCkptInfo *pInfo = walCkptInfo(pWal);
    assert( pInfo->nBackfill==pWal->hdr.mxFrame );
    if( pInfo->nBackfill>0 ){
      u32 salt1;
      sqlite3_randomness(4, &salt1);
      rc = walLockExclusive(pWal, WAL_READ_LOCK(1), WAL_NREADER-1);
      if( rc==SQLITE_OK ){
        /* If all readers are using WAL_READ_LOCK(0) (in other words if no
        ** readers are currently using the WAL), then the transactions
        ** frames will overwrite the start of the existing log. Update the
        ** wal-index header to reflect this.
        **
        ** In theory it would be Ok to update the cache of the header only
        ** at this point. But updating the actual wal-index header is also
        ** safe and means there is no special case for sqlite3WalUndo()
        ** to handle if this transaction is rolled back.  */
        walRestartHdr(pWal, salt1);
        walUnlockExclusive(pWal, WAL_READ_LOCK(1), WAL_NREADER-1);
      }else if( rc!=SQLITE_BUSY ){
        return rc;
      }
    }
    walUnlockShared(pWal, WAL_READ_LOCK(0));
    pWal->readLock = -1;
    cnt = 0;
    do{
      int notUsed;
      rc = walTryBeginRead(pWal, &notUsed, 1, ++cnt);
    }while( rc==WAL_RETRY );
    assert( (rc&0xff)!=SQLITE_BUSY ); /* BUSY not possible when useWal==1 */
    testcase( (rc&0xff)==SQLITE_IOERR );
    testcase( rc==SQLITE_PROTOCOL );
    testcase( rc==SQLITE_OK );
  }
  return rc;
}

/*
** Information about the current state of the WAL file and where
** the next fsync should occur - passed from sqlite3WalFrames() into
** walWriteToLog().
*/
typedef struct WalWriter {
  Wal *pWal;                   /* The complete WAL information */
  sqlite3_file *pFd;           /* The WAL file to which we write */
  sqlite3_int64 iSyncPoint;    /* Fsync at this offset */
  int syncFlags;               /* Flags for the fsync */
  int szPage;                  /* Size of one page */
} WalWriter;

/*
** Write iAmt bytes of content into the WAL file beginning at iOffset.
** Do a sync when crossing the p->iSyncPoint boundary.
**
** In other words, if iSyncPoint is in between iOffset and iOffset+iAmt,
** first write the part before iSyncPoint, then sync, then write the
** rest.
*/
static int walWriteToLog(
  WalWriter *p,              /* WAL to write to */
  void *pContent,            /* Content to be written */
  int iAmt,                  /* Number of bytes to write */
  sqlite3_int64 iOffset      /* Start writing at this offset */
){
  int rc;
  if( iOffset<p->iSyncPoint && iOffset+iAmt>=p->iSyncPoint ){
    int iFirstAmt = (int)(p->iSyncPoint - iOffset);
    rc = sqlite3OsWrite(p->pFd, pContent, iFirstAmt, iOffset);
    if( rc ) return rc;
    iOffset += iFirstAmt;
    iAmt -= iFirstAmt;
    pContent = (void*)(iFirstAmt + (char*)pContent);
    assert( WAL_SYNC_FLAGS(p->syncFlags)!=0 );
    rc = sqlite3OsSync(p->pFd, WAL_SYNC_FLAGS(p->syncFlags));
    if( iAmt==0 || rc ) return rc;
  }
  rc = sqlite3OsWrite(p->pFd, pContent, iAmt, iOffset);
  return rc;
}

/*
** Write out a single frame of the WAL
*/
static int walWriteOneFrame(
  WalWriter *p,               /* Where to write the frame */
  PgHdr *pPage,               /* The page of the frame to be written */
  int nTruncate,              /* The commit flag.  Usually 0.  >0 for commit */
  sqlite3_int64 iOffset       /* Byte offset at which to write */
){
  int rc;                         /* Result code from subfunctions */
  void *pData;                    /* Data actually written */
  u8 aFrame[WAL_FRAME_HDRSIZE];   /* Buffer to assemble frame-header in */
#if defined(SQLITE_HAS_CODEC)
  if( (pData = sqlite3PagerCodec(pPage))==0 ) return SQLITE_NOMEM_BKPT;
#else
  pData = pPage->pData;
#endif
  walEncodeFrame(p->pWal, pPage->pgno, nTruncate, pData, aFrame);
  rc = walWriteToLog(p, aFrame, sizeof(aFrame), iOffset);
  if( rc ) return rc;
  /* Write the page data */
  rc = walWriteToLog(p, pData, p->szPage, iOffset+sizeof(aFrame));
  return rc;
}

/*
** This function is called as part of committing a transaction within which
** one or more frames have been overwritten. It updates the checksums for
** all frames written to the wal file by the current transaction starting
** with the earliest to have been overwritten.
**
** SQLITE_OK is returned if successful, or an SQLite error code otherwise.
*/
static int walRewriteChecksums(Wal *pWal, u32 iLast){
  const int szPage = pWal->szPage;/* Database page size */
  int rc = SQLITE_OK;             /* Return code */
  u8 *aBuf;                       /* Buffer to load data from wal file into */
  u8 aFrame[WAL_FRAME_HDRSIZE];   /* Buffer to assemble frame-headers in */
  u32 iRead;                      /* Next frame to read from wal file */
  i64 iCksumOff;

  aBuf = sqlite3_malloc(szPage + WAL_FRAME_HDRSIZE);
  if( aBuf==0 ) return SQLITE_NOMEM_BKPT;

  /* Find the checksum values to use as input for the recalculating the
  ** first checksum. If the first frame is frame 1 (implying that the current
  ** transaction restarted the wal file), these values must be read from the
  ** wal-file header. Otherwise, read them from the frame header of the
  ** previous frame.  */
  assert( pWal->iReCksum>0 );
  if( pWal->iReCksum==1 ){
    iCksumOff = 24;
  }else{
    iCksumOff = walFrameOffset(pWal->iReCksum-1, szPage) + 16;
  }
  rc = sqlite3OsRead(pWal->pWalFd, aBuf, sizeof(u32)*2, iCksumOff);
  pWal->hdr.aFrameCksum[0] = sqlite3Get4byte(aBuf);
  pWal->hdr.aFrameCksum[1] = sqlite3Get4byte(&aBuf[sizeof(u32)]);

  iRead = pWal->iReCksum;
  pWal->iReCksum = 0;
  for(; rc==SQLITE_OK && iRead<=iLast; iRead++){
    i64 iOff = walFrameOffset(iRead, szPage);
    rc = sqlite3OsRead(pWal->pWalFd, aBuf, szPage+WAL_FRAME_HDRSIZE, iOff);
    if( rc==SQLITE_OK ){
      u32 iPgno, nDbSize;
      iPgno = sqlite3Get4byte(aBuf);
      nDbSize = sqlite3Get4byte(&aBuf[4]);

      walEncodeFrame(pWal, iPgno, nDbSize, &aBuf[WAL_FRAME_HDRSIZE], aFrame);
      rc = sqlite3OsWrite(pWal->pWalFd, aFrame, sizeof(aFrame), iOff);
    }
  }

  sqlite3_free(aBuf);
  return rc;
}

/* 
** Write a set of frames to the log. The caller must hold the write-lock
** on the log file (obtained using sqlite3WalBeginWriteTransaction()).
*/
SQLITE_PRIVATE int sqlite3WalFrames(
  Wal *pWal,                      /* Wal handle to write to */
  int szPage,                     /* Database page-size in bytes */
  PgHdr *pList,                   /* List of dirty pages to write */
  Pgno nTruncate,                 /* Database size after this commit */
  int isCommit,                   /* True if this is a commit */
  int sync_flags                  /* Flags to pass to OsSync() (or 0) */
){
  int rc;                         /* Used to catch return codes */
  u32 iFrame;                     /* Next frame address */
  PgHdr *p;                       /* Iterator to run through pList with. */
  PgHdr *pLast = 0;               /* Last frame in list */
  int nExtra = 0;                 /* Number of extra copies of last page */
  int szFrame;                    /* The size of a single frame */
  i64 iOffset;                    /* Next byte to write in WAL file */
  WalWriter w;                    /* The writer */
  u32 iFirst = 0;                 /* First frame that may be overwritten */
  WalIndexHdr *pLive;             /* Pointer to shared header */

  assert( pList );
  assert( pWal->writeLock );

  /* If this frame set completes a transaction, then nTruncate>0.  If
  ** nTruncate==0 then this frame set does not complete the transaction. */
  assert( (isCommit!=0)==(nTruncate!=0) );

#if defined(SQLITE_TEST) && defined(SQLITE_DEBUG)
  { int cnt; for(cnt=0, p=pList; p; p=p->pDirty, cnt++){}
    WALTRACE(("WAL%p: frame write begin. %d frames. mxFrame=%d. %s\n",
              pWal, cnt, pWal->hdr.mxFrame, isCommit ? "Commit" : "Spill"));
  }
#endif

  pLive = (WalIndexHdr*)walIndexHdr(pWal);
  if( memcmp(&pWal->hdr, (void *)pLive, sizeof(WalIndexHdr))!=0 ){
    iFirst = pLive->mxFrame+1;
  }

  /* See if it is possible to write these frames into the start of the
  ** log file, instead of appending to it at pWal->hdr.mxFrame.
  */
  if( SQLITE_OK!=(rc = walRestartLog(pWal)) ){
    return rc;
  }

  /* If this is the first frame written into the log, write the WAL
  ** header to the start of the WAL file. See comments at the top of
  ** this source file for a description of the WAL header format.
  */
  iFrame = pWal->hdr.mxFrame;
  if( iFrame==0 ){
    u8 aWalHdr[WAL_HDRSIZE];      /* Buffer to assemble wal-header in */
    u32 aCksum[2];                /* Checksum for wal-header */

    sqlite3Put4byte(&aWalHdr[0], (WAL_MAGIC | SQLITE_BIGENDIAN));
    sqlite3Put4byte(&aWalHdr[4], WAL_MAX_VERSION);
    sqlite3Put4byte(&aWalHdr[8], szPage);
    sqlite3Put4byte(&aWalHdr[12], pWal->nCkpt);
    if( pWal->nCkpt==0 ) sqlite3_randomness(8, pWal->hdr.aSalt);
    memcpy(&aWalHdr[16], pWal->hdr.aSalt, 8);
    walChecksumBytes(1, aWalHdr, WAL_HDRSIZE-2*4, 0, aCksum);
    sqlite3Put4byte(&aWalHdr[24], aCksum[0]);
    sqlite3Put4byte(&aWalHdr[28], aCksum[1]);
    
    pWal->szPage = szPage;
    pWal->hdr.bigEndCksum = SQLITE_BIGENDIAN;
    pWal->hdr.aFrameCksum[0] = aCksum[0];
    pWal->hdr.aFrameCksum[1] = aCksum[1];
    pWal->truncateOnCommit = 1;

    rc = sqlite3OsWrite(pWal->pWalFd, aWalHdr, sizeof(aWalHdr), 0);
    WALTRACE(("WAL%p: wal-header write %s\n", pWal, rc ? "failed" : "ok"));
    if( rc!=SQLITE_OK ){
      return rc;
    }

    /* Sync the header (unless SQLITE_IOCAP_SEQUENTIAL is true or unless
    ** all syncing is turned off by PRAGMA synchronous=OFF).  Otherwise
    ** an out-of-order write following a WAL restart could result in
    ** database corruption.  See the ticket:
    **
    **     https://sqlite.org/src/info/ff5be73dee
    */
    if( pWal->syncHeader ){
      rc = sqlite3OsSync(pWal->pWalFd, CKPT_SYNC_FLAGS(sync_flags));
      if( rc ) return rc;
    }
  }
  assert( (int)pWal->szPage==szPage );

  /* Setup information needed to write frames into the WAL */
  w.pWal = pWal;
  w.pFd = pWal->pWalFd;
  w.iSyncPoint = 0;
  w.syncFlags = sync_flags;
  w.szPage = szPage;
  iOffset = walFrameOffset(iFrame+1, szPage);
  szFrame = szPage + WAL_FRAME_HDRSIZE;

  /* Write all frames into the log file exactly once */
  for(p=pList; p; p=p->pDirty){
    int nDbSize;   /* 0 normally.  Positive == commit flag */

    /* Check if this page has already been written into the wal file by
    ** the current transaction. If so, overwrite the existing frame and
    ** set Wal.writeLock to WAL_WRITELOCK_RECKSUM - indicating that 
    ** checksums must be recomputed when the transaction is committed.  */
    if( iFirst && (p->pDirty || isCommit==0) ){
      u32 iWrite = 0;
      VVA_ONLY(rc =) sqlite3WalFindFrame(pWal, p->pgno, &iWrite);
      assert( rc==SQLITE_OK || iWrite==0 );
      if( iWrite>=iFirst ){
        i64 iOff = walFrameOffset(iWrite, szPage) + WAL_FRAME_HDRSIZE;
        void *pData;
        if( pWal->iReCksum==0 || iWrite<pWal->iReCksum ){
          pWal->iReCksum = iWrite;
        }
#if defined(SQLITE_HAS_CODEC)
        if( (pData = sqlite3PagerCodec(p))==0 ) return SQLITE_NOMEM;
#else
        pData = p->pData;
#endif
        rc = sqlite3OsWrite(pWal->pWalFd, pData, szPage, iOff);
        if( rc ) return rc;
        p->flags &= ~PGHDR_WAL_APPEND;
        continue;
      }
    }

    iFrame++;
    assert( iOffset==walFrameOffset(iFrame, szPage) );
    nDbSize = (isCommit && p->pDirty==0) ? nTruncate : 0;
    rc = walWriteOneFrame(&w, p, nDbSize, iOffset);
    if( rc ) return rc;
    pLast = p;
    iOffset += szFrame;
    p->flags |= PGHDR_WAL_APPEND;
  }

  /* Recalculate checksums within the wal file if required. */
  if( isCommit && pWal->iReCksum ){
    rc = walRewriteChecksums(pWal, iFrame);
    if( rc ) return rc;
  }

  /* If this is the end of a transaction, then we might need to pad
  ** the transaction and/or sync the WAL file.
  **
  ** Padding and syncing only occur if this set of frames complete a
  ** transaction and if PRAGMA synchronous=FULL.  If synchronous==NORMAL
  ** or synchronous==OFF, then no padding or syncing are needed.
  **
  ** If SQLITE_IOCAP_POWERSAFE_OVERWRITE is defined, then padding is not
  ** needed and only the sync is done.  If padding is needed, then the
  ** final frame is repeated (with its commit mark) until the next sector
  ** boundary is crossed.  Only the part of the WAL prior to the last
  ** sector boundary is synced; the part of the last frame that extends
  ** past the sector boundary is written after the sync.
  */
  if( isCommit && WAL_SYNC_FLAGS(sync_flags)!=0 ){
    int bSync = 1;
    if( pWal->padToSectorBoundary ){
      int sectorSize = sqlite3SectorSize(pWal->pWalFd);
      w.iSyncPoint = ((iOffset+sectorSize-1)/sectorSize)*sectorSize;
      bSync = (w.iSyncPoint==iOffset);
      testcase( bSync );
      while( iOffset<w.iSyncPoint ){
        rc = walWriteOneFrame(&w, pLast, nTruncate, iOffset);
        if( rc ) return rc;
        iOffset += szFrame;
        nExtra++;
        assert( pLast!=0 );
      }
    }
    if( bSync ){
      assert( rc==SQLITE_OK );
      rc = sqlite3OsSync(w.pFd, WAL_SYNC_FLAGS(sync_flags));
    }
  }

  /* If this frame set completes the first transaction in the WAL and
  ** if PRAGMA journal_size_limit is set, then truncate the WAL to the
  ** journal size limit, if possible.
  */
  if( isCommit && pWal->truncateOnCommit && pWal->mxWalSize>=0 ){
    i64 sz = pWal->mxWalSize;
    if( walFrameOffset(iFrame+nExtra+1, szPage)>pWal->mxWalSize ){
      sz = walFrameOffset(iFrame+nExtra+1, szPage);
    }
    walLimitSize(pWal, sz);
    pWal->truncateOnCommit = 0;
  }

  /* Append data to the wal-index. It is not necessary to lock the 
  ** wal-index to do this as the SQLITE_SHM_WRITE lock held on the wal-index
  ** guarantees that there are no other writers, and no data that may
  ** be in use by existing readers is being overwritten.
  */
  iFrame = pWal->hdr.mxFrame;
  for(p=pList; p && rc==SQLITE_OK; p=p->pDirty){
    if( (p->flags & PGHDR_WAL_APPEND)==0 ) continue;
    iFrame++;
    rc = walIndexAppend(pWal, iFrame, p->pgno);
  }
  assert( pLast!=0 || nExtra==0 );
  while( rc==SQLITE_OK && nExtra>0 ){
    iFrame++;
    nExtra--;
    rc = walIndexAppend(pWal, iFrame, pLast->pgno);
  }

  if( rc==SQLITE_OK ){
    /* Update the private copy of the header. */
    pWal->hdr.szPage = (u16)((szPage&0xff00) | (szPage>>16));
    testcase( szPage<=32768 );
    testcase( szPage>=65536 );
    pWal->hdr.mxFrame = iFrame;
    if( isCommit ){
      pWal->hdr.iChange++;
      pWal->hdr.nPage = nTruncate;
    }
    /* If this is a commit, update the wal-index header too. */
    if( isCommit ){
      walIndexWriteHdr(pWal);
      pWal->iCallback = iFrame;
    }
  }

  WALTRACE(("WAL%p: frame write %s\n", pWal, rc ? "failed" : "ok"));
  return rc;
}

/* 
** This routine is called to implement sqlite3_wal_checkpoint() and
** related interfaces.
**
** Obtain a CHECKPOINT lock and then backfill as much information as
** we can from WAL into the database.
**
** If parameter xBusy is not NULL, it is a pointer to a busy-handler
** callback. In this case this function runs a blocking checkpoint.
*/
SQLITE_PRIVATE int sqlite3WalCheckpoint(
  Wal *pWal,                      /* Wal connection */
  sqlite3 *db,                    /* Check this handle's interrupt flag */
  int eMode,                      /* PASSIVE, FULL, RESTART, or TRUNCATE */
  int (*xBusy)(void*),            /* Function to call when busy */
  void *pBusyArg,                 /* Context argument for xBusyHandler */
  int sync_flags,                 /* Flags to sync db file with (or 0) */
  int nBuf,                       /* Size of temporary buffer */
  u8 *zBuf,                       /* Temporary buffer to use */
  int *pnLog,                     /* OUT: Number of frames in WAL */
  int *pnCkpt                     /* OUT: Number of backfilled frames in WAL */
){
  int rc;                         /* Return code */
  int isChanged = 0;              /* True if a new wal-index header is loaded */
  int eMode2 = eMode;             /* Mode to pass to walCheckpoint() */
  int (*xBusy2)(void*) = xBusy;   /* Busy handler for eMode2 */

  assert( pWal->ckptLock==0 );
  assert( pWal->writeLock==0 );

  /* EVIDENCE-OF: R-62920-47450 The busy-handler callback is never invoked
  ** in the SQLITE_CHECKPOINT_PASSIVE mode. */
  assert( eMode!=SQLITE_CHECKPOINT_PASSIVE || xBusy==0 );

  if( pWal->readOnly ) return SQLITE_READONLY;
  WALTRACE(("WAL%p: checkpoint begins\n", pWal));

  /* IMPLEMENTATION-OF: R-62028-47212 All calls obtain an exclusive 
  ** "checkpoint" lock on the database file. */
  rc = walLockExclusive(pWal, WAL_CKPT_LOCK, 1);
  if( rc ){
    /* EVIDENCE-OF: R-10421-19736 If any other process is running a
    ** checkpoint operation at the same time, the lock cannot be obtained and
    ** SQLITE_BUSY is returned.
    ** EVIDENCE-OF: R-53820-33897 Even if there is a busy-handler configured,
    ** it will not be invoked in this case.
    */
    testcase( rc==SQLITE_BUSY );
    testcase( xBusy!=0 );
    return rc;
  }
  pWal->ckptLock = 1;

  /* IMPLEMENTATION-OF: R-59782-36818 The SQLITE_CHECKPOINT_FULL, RESTART and
  ** TRUNCATE modes also obtain the exclusive "writer" lock on the database
  ** file.
  **
  ** EVIDENCE-OF: R-60642-04082 If the writer lock cannot be obtained
  ** immediately, and a busy-handler is configured, it is invoked and the
  ** writer lock retried until either the busy-handler returns 0 or the
  ** lock is successfully obtained.
  */
  if( eMode!=SQLITE_CHECKPOINT_PASSIVE ){
    rc = walBusyLock(pWal, xBusy, pBusyArg, WAL_WRITE_LOCK, 1);
    if( rc==SQLITE_OK ){
      pWal->writeLock = 1;
    }else if( rc==SQLITE_BUSY ){
      eMode2 = SQLITE_CHECKPOINT_PASSIVE;
      xBusy2 = 0;
      rc = SQLITE_OK;
    }
  }

  /* Read the wal-index header. */
  if( rc==SQLITE_OK ){
    rc = walIndexReadHdr(pWal, &isChanged);
    if( isChanged && pWal->pDbFd->pMethods->iVersion>=3 ){
      sqlite3OsUnfetch(pWal->pDbFd, 0, 0);
    }
  }

  /* Copy data from the log to the database file. */
  if( rc==SQLITE_OK ){

    if( pWal->hdr.mxFrame && walPagesize(pWal)!=nBuf ){
      rc = SQLITE_CORRUPT_BKPT;
    }else{
      rc = walCheckpoint(pWal, db, eMode2, xBusy2, pBusyArg, sync_flags, zBuf);
    }

    /* If no error occurred, set the output variables. */
    if( rc==SQLITE_OK || rc==SQLITE_BUSY ){
      if( pnLog ) *pnLog = (int)pWal->hdr.mxFrame;
      if( pnCkpt ) *pnCkpt = (int)(walCkptInfo(pWal)->nBackfill);
    }
  }

  if( isChanged ){
    /* If a new wal-index header was loaded before the checkpoint was 
    ** performed, then the pager-cache associated with pWal is now
    ** out of date. So zero the cached wal-index header to ensure that
    ** next time the pager opens a snapshot on this database it knows that
    ** the cache needs to be reset.
    */
    memset(&pWal->hdr, 0, sizeof(WalIndexHdr));
  }

  /* Release the locks. */
  sqlite3WalEndWriteTransaction(pWal);
  walUnlockExclusive(pWal, WAL_CKPT_LOCK, 1);
  pWal->ckptLock = 0;
  WALTRACE(("WAL%p: checkpoint %s\n", pWal, rc ? "failed" : "ok"));
  return (rc==SQLITE_OK && eMode!=eMode2 ? SQLITE_BUSY : rc);
}

/* Return the value to pass to a sqlite3_wal_hook callback, the
** number of frames in the WAL at the point of the last commit since
** sqlite3WalCallback() was called.  If no commits have occurred since
** the last call, then return 0.
*/
SQLITE_PRIVATE int sqlite3WalCallback(Wal *pWal){
  u32 ret = 0;
  if( pWal ){
    ret = pWal->iCallback;
    pWal->iCallback = 0;
  }
  return (int)ret;
}

/*
** This function is called to change the WAL subsystem into or out
** of locking_mode=EXCLUSIVE.
**
** If op is zero, then attempt to change from locking_mode=EXCLUSIVE
** into locking_mode=NORMAL.  This means that we must acquire a lock
** on the pWal->readLock byte.  If the WAL is already in locking_mode=NORMAL
** or if the acquisition of the lock fails, then return 0.  If the
** transition out of exclusive-mode is successful, return 1.  This
** operation must occur while the pager is still holding the exclusive
** lock on the main database file.
**
** If op is one, then change from locking_mode=NORMAL into 
** locking_mode=EXCLUSIVE.  This means that the pWal->readLock must
** be released.  Return 1 if the transition is made and 0 if the
** WAL is already in exclusive-locking mode - meaning that this
** routine is a no-op.  The pager must already hold the exclusive lock
** on the main database file before invoking this operation.
**
** If op is negative, then do a dry-run of the op==1 case but do
** not actually change anything. The pager uses this to see if it
** should acquire the database exclusive lock prior to invoking
** the op==1 case.
*/
SQLITE_PRIVATE int sqlite3WalExclusiveMode(Wal *pWal, int op){
  int rc;
  assert( pWal->writeLock==0 );
  assert( pWal->exclusiveMode!=WAL_HEAPMEMORY_MODE || op==-1 );

  /* pWal->readLock is usually set, but might be -1 if there was a 
  ** prior error while attempting to acquire are read-lock. This cannot 
  ** happen if the connection is actually in exclusive mode (as no xShmLock
  ** locks are taken in this case). Nor should the pager attempt to
  ** upgrade to exclusive-mode following such an error.
  */
  assert( pWal->readLock>=0 || pWal->lockError );
  assert( pWal->readLock>=0 || (op<=0 && pWal->exclusiveMode==0) );

  if( op==0 ){
    if( pWal->exclusiveMode!=WAL_NORMAL_MODE ){
      pWal->exclusiveMode = WAL_NORMAL_MODE;
      if( walLockShared(pWal, WAL_READ_LOCK(pWal->readLock))!=SQLITE_OK ){
        pWal->exclusiveMode = WAL_EXCLUSIVE_MODE;
      }
      rc = pWal->exclusiveMode==WAL_NORMAL_MODE;
    }else{
      /* Already in locking_mode=NORMAL */
      rc = 0;
    }
  }else if( op>0 ){
    assert( pWal->exclusiveMode==WAL_NORMAL_MODE );
    assert( pWal->readLock>=0 );
    walUnlockShared(pWal, WAL_READ_LOCK(pWal->readLock));
    pWal->exclusiveMode = WAL_EXCLUSIVE_MODE;
    rc = 1;
  }else{
    rc = pWal->exclusiveMode==WAL_NORMAL_MODE;
  }
  return rc;
}

/* 
** Return true if the argument is non-NULL and the WAL module is using
** heap-memory for the wal-index. Otherwise, if the argument is NULL or the
** WAL module is using shared-memory, return false. 
*/
SQLITE_PRIVATE int sqlite3WalHeapMemory(Wal *pWal){
  return (pWal && pWal->exclusiveMode==WAL_HEAPMEMORY_MODE );
}

#ifdef SQLITE_ENABLE_SNAPSHOT
/* Create a snapshot object.  The content of a snapshot is opaque to
** every other subsystem, so the WAL module can put whatever it needs
** in the object.
*/
SQLITE_PRIVATE int sqlite3WalSnapshotGet(Wal *pWal, sqlite3_snapshot **ppSnapshot){
  int rc = SQLITE_OK;
  WalIndexHdr *pRet;
  static const u32 aZero[4] = { 0, 0, 0, 0 };

  assert( pWal->readLock>=0 && pWal->writeLock==0 );

  if( memcmp(&pWal->hdr.aFrameCksum[0],aZero,16)==0 ){
    *ppSnapshot = 0;
    return SQLITE_ERROR;
  }
  pRet = (WalIndexHdr*)sqlite3_malloc(sizeof(WalIndexHdr));
  if( pRet==0 ){
    rc = SQLITE_NOMEM_BKPT;
  }else{
    memcpy(pRet, &pWal->hdr, sizeof(WalIndexHdr));
    *ppSnapshot = (sqlite3_snapshot*)pRet;
  }

  return rc;
}

/* Try to open on pSnapshot when the next read-transaction starts
*/
SQLITE_PRIVATE void sqlite3WalSnapshotOpen(Wal *pWal, sqlite3_snapshot *pSnapshot){
  pWal->pSnapshot = (WalIndexHdr*)pSnapshot;
}

/* 
** Return a +ve value if snapshot p1 is newer than p2. A -ve value if
** p1 is older than p2 and zero if p1 and p2 are the same snapshot.
*/
SQLITE_API int sqlite3_snapshot_cmp(sqlite3_snapshot *p1, sqlite3_snapshot *p2){
  WalIndexHdr *pHdr1 = (WalIndexHdr*)p1;
  WalIndexHdr *pHdr2 = (WalIndexHdr*)p2;

  /* aSalt[0] is a copy of the value stored in the wal file header. It
  ** is incremented each time the wal file is restarted.  */
  if( pHdr1->aSalt[0]<pHdr2->aSalt[0] ) return -1;
  if( pHdr1->aSalt[0]>pHdr2->aSalt[0] ) return +1;
  if( pHdr1->mxFrame<pHdr2->mxFrame ) return -1;
  if( pHdr1->mxFrame>pHdr2->mxFrame ) return +1;
  return 0;
}

/*
** The caller currently has a read transaction open on the database.
** This function takes a SHARED lock on the CHECKPOINTER slot and then
** checks if the snapshot passed as the second argument is still 
** available. If so, SQLITE_OK is returned.
**
** If the snapshot is not available, SQLITE_ERROR is returned. Or, if
** the CHECKPOINTER lock cannot be obtained, SQLITE_BUSY. If any error
** occurs (any value other than SQLITE_OK is returned), the CHECKPOINTER
** lock is released before returning.
*/
SQLITE_PRIVATE int sqlite3WalSnapshotCheck(Wal *pWal, sqlite3_snapshot *pSnapshot){
  int rc;
  rc = walLockShared(pWal, WAL_CKPT_LOCK);
  if( rc==SQLITE_OK ){
    WalIndexHdr *pNew = (WalIndexHdr*)pSnapshot;
    if( memcmp(pNew->aSalt, pWal->hdr.aSalt, sizeof(pWal->hdr.aSalt))
     || pNew->mxFrame<walCkptInfo(pWal)->nBackfillAttempted
    ){
      rc = SQLITE_ERROR_SNAPSHOT;
      walUnlockShared(pWal, WAL_CKPT_LOCK);
    }
  }
  return rc;
}

/*
** Release a lock obtained by an earlier successful call to
** sqlite3WalSnapshotCheck().
*/
SQLITE_PRIVATE void sqlite3WalSnapshotUnlock(Wal *pWal){
  assert( pWal );
  walUnlockShared(pWal, WAL_CKPT_LOCK);
}


#endif /* SQLITE_ENABLE_SNAPSHOT */

#ifdef SQLITE_ENABLE_ZIPVFS
/*
** If the argument is not NULL, it points to a Wal object that holds a
** read-lock. This function returns the database page-size if it is known,
** or zero if it is not (or if pWal is NULL).
*/
SQLITE_PRIVATE int sqlite3WalFramesize(Wal *pWal){
  assert( pWal==0 || pWal->readLock>=0 );
  return (pWal ? pWal->szPage : 0);
}
#endif

/* Return the sqlite3_file object for the WAL file
*/
SQLITE_PRIVATE sqlite3_file *sqlite3WalFile(Wal *pWal){
  return pWal->pWalFd;
}

#endif /* #ifndef SQLITE_OMIT_WAL */

/************** End of wal.c *************************************************/
/************** Begin file btmutex.c *****************************************/
/*
** 2007 August 27
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
** This file contains code used to implement mutexes on Btree objects.
** This code really belongs in btree.c.  But btree.c is getting too
** big and we want to break it down some.  This packaged seemed like
** a good breakout.
*/
/************** Include btreeInt.h in the middle of btmutex.c ****************/
/************** Begin file btreeInt.h ****************************************/
/*
** 2004 April 6
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file implements an external (disk-based) database using BTrees.
** For a detailed discussion of BTrees, refer to
**
**     Donald E. Knuth, THE ART OF COMPUTER PROGRAMMING, Volume 3:
**     "Sorting And Searching", pages 473-480. Addison-Wesley
**     Publishing Company, Reading, Massachusetts.
**
** The basic idea is that each page of the file contains N database
** entries and N+1 pointers to subpages.
**
**   ----------------------------------------------------------------
**   |  Ptr(0) | Key(0) | Ptr(1) | Key(1) | ... | Key(N-1) | Ptr(N) |
**   ----------------------------------------------------------------
**
** All of the keys on the page that Ptr(0) points to have values less
** than Key(0).  All of the keys on page Ptr(1) and its subpages have
** values greater than Key(0) and less than Key(1).  All of the keys
** on Ptr(N) and its subpages have values greater than Key(N-1).  And
** so forth.
**
** Finding a particular key requires reading O(log(M)) pages from the 
** disk where M is the number of entries in the tree.
**
** In this implementation, a single file can hold one or more separate 
** BTrees.  Each BTree is identified by the index of its root page.  The
** key and data for any entry are combined to form the "payload".  A
** fixed amount of payload can be carried directly on the database
** page.  If the payload is larger than the preset amount then surplus
** bytes are stored on overflow pages.  The payload for an entry
** and the preceding pointer are combined to form a "Cell".  Each 
** page has a small header which contains the Ptr(N) pointer and other
** information such as the size of key and data.
**
** FORMAT DETAILS
**
** The file is divided into pages.  The first page is called page 1,
** the second is page 2, and so forth.  A page number of zero indicates
** "no such page".  The page size can be any power of 2 between 512 and 65536.
** Each page can be either a btree page, a freelist page, an overflow
** page, or a pointer-map page.
**
** The first page is always a btree page.  The first 100 bytes of the first
** page contain a special header (the "file header") that describes the file.
** The format of the file header is as follows:
**
**   OFFSET   SIZE    DESCRIPTION
**      0      16     Header string: "SQLite format 3\000"
**     16       2     Page size in bytes.  (1 means 65536)
**     18       1     File format write version
**     19       1     File format read version
**     20       1     Bytes of unused space at the end of each page
**     21       1     Max embedded payload fraction (must be 64)
**     22       1     Min embedded payload fraction (must be 32)
**     23       1     Min leaf payload fraction (must be 32)
**     24       4     File change counter
**     28       4     Reserved for future use
**     32       4     First freelist page
**     36       4     Number of freelist pages in the file
**     40      60     15 4-byte meta values passed to higher layers
**
**     40       4     Schema cookie
**     44       4     File format of schema layer
**     48       4     Size of page cache
**     52       4     Largest root-page (auto/incr_vacuum)
**     56       4     1=UTF-8 2=UTF16le 3=UTF16be
**     60       4     User version
**     64       4     Incremental vacuum mode
**     68       4     Application-ID
**     72      20     unused
**     92       4     The version-valid-for number
**     96       4     SQLITE_VERSION_NUMBER
**
** All of the integer values are big-endian (most significant byte first).
**
** The file change counter is incremented when the database is changed
** This counter allows other processes to know when the file has changed
** and thus when they need to flush their cache.
**
** The max embedded payload fraction is the amount of the total usable
** space in a page that can be consumed by a single cell for standard
** B-tree (non-LEAFDATA) tables.  A value of 255 means 100%.  The default
** is to limit the maximum cell size so that at least 4 cells will fit
** on one page.  Thus the default max embedded payload fraction is 64.
**
** If the payload for a cell is larger than the max payload, then extra
** payload is spilled to overflow pages.  Once an overflow page is allocated,
** as many bytes as possible are moved into the overflow pages without letting
** the cell size drop below the min embedded payload fraction.
**
** The min leaf payload fraction is like the min embedded payload fraction
** except that it applies to leaf nodes in a LEAFDATA tree.  The maximum
** payload fraction for a LEAFDATA tree is always 100% (or 255) and it
** not specified in the header.
**
** Each btree pages is divided into three sections:  The header, the
** cell pointer array, and the cell content area.  Page 1 also has a 100-byte
** file header that occurs before the page header.
**
**      |----------------|
**      | file header    |   100 bytes.  Page 1 only.
**      |----------------|
**      | page header    |   8 bytes for leaves.  12 bytes for interior nodes
**      |----------------|
**      | cell pointer   |   |  2 bytes per cell.  Sorted order.
**      | array          |   |  Grows downward
**      |                |   v
**      |----------------|
**      | unallocated    |
**      | space          |
**      |----------------|   ^  Grows upwards
**      | cell content   |   |  Arbitrary order interspersed with freeblocks.
**      | area           |   |  and free space fragments.
**      |----------------|
**
** The page headers looks like this:
**
**   OFFSET   SIZE     DESCRIPTION
**      0       1      Flags. 1: intkey, 2: zerodata, 4: leafdata, 8: leaf
**      1       2      byte offset to the first freeblock
**      3       2      number of cells on this page
**      5       2      first byte of the cell content area
**      7       1      number of fragmented free bytes
**      8       4      Right child (the Ptr(N) value).  Omitted on leaves.
**
** The flags define the format of this btree page.  The leaf flag means that
** this page has no children.  The zerodata flag means that this page carries
** only keys and no data.  The intkey flag means that the key is an integer
** which is stored in the key size entry of the cell header rather than in
** the payload area.
**
** The cell pointer array begins on the first byte after the page header.
** The cell pointer array contains zero or more 2-byte numbers which are
** offsets from the beginning of the page to the cell content in the cell
** content area.  The cell pointers occur in sorted order.  The system strives
** to keep free space after the last cell pointer so that new cells can
** be easily added without having to defragment the page.
**
** Cell content is stored at the very end of the page and grows toward the
** beginning of the page.
**
** Unused space within the cell content area is collected into a linked list of
** freeblocks.  Each freeblock is at least 4 bytes in size.  The byte offset
** to the first freeblock is given in the header.  Freeblocks occur in
** increasing order.  Because a freeblock must be at least 4 bytes in size,
** any group of 3 or fewer unused bytes in the cell content area cannot
** exist on the freeblock chain.  A group of 3 or fewer free bytes is called
** a fragment.  The total number of bytes in all fragments is recorded.
** in the page header at offset 7.
**
**    SIZE    DESCRIPTION
**      2     Byte offset of the next freeblock
**      2     Bytes in this freeblock
**
** Cells are of variable length.  Cells are stored in the cell content area at
** the end of the page.  Pointers to the cells are in the cell pointer array
** that immediately follows the page header.  Cells is not necessarily
** contiguous or in order, but cell pointers are contiguous and in order.
**
** Cell content makes use of variable length integers.  A variable
** length integer is 1 to 9 bytes where the lower 7 bits of each 
** byte are used.  The integer consists of all bytes that have bit 8 set and
** the first byte with bit 8 clear.  The most significant byte of the integer
** appears first.  A variable-length integer may not be more than 9 bytes long.
** As a special case, all 8 bytes of the 9th byte are used as data.  This
** allows a 64-bit integer to be encoded in 9 bytes.
**
**    0x00                      becomes  0x00000000
**    0x7f                      becomes  0x0000007f
**    0x81 0x00                 becomes  0x00000080
**    0x82 0x00                 becomes  0x00000100
**    0x80 0x7f                 becomes  0x0000007f
**    0x8a 0x91 0xd1 0xac 0x78  becomes  0x12345678
**    0x81 0x81 0x81 0x81 0x01  becomes  0x10204081
**
** Variable length integers are used for rowids and to hold the number of
** bytes of key and data in a btree cell.
**
** The content of a cell looks like this:
**
**    SIZE    DESCRIPTION
**      4     Page number of the left child. Omitted if leaf flag is set.
**     var    Number of bytes of data. Omitted if the zerodata flag is set.
**     var    Number of bytes of key. Or the key itself if intkey flag is set.
**      *     Payload
**      4     First page of the overflow chain.  Omitted if no overflow
**
** Overflow pages form a linked list.  Each page except the last is completely
** filled with data (pagesize - 4 bytes).  The last page can have as little
** as 1 byte of data.
**
**    SIZE    DESCRIPTION
**      4     Page number of next overflow page
**      *     Data
**
** Freelist pages come in two subtypes: trunk pages and leaf pages.  The
** file header points to the first in a linked list of trunk page.  Each trunk
** page points to multiple leaf pages.  The content of a leaf page is
** unspecified.  A trunk page looks like this:
**
**    SIZE    DESCRIPTION
**      4     Page number of next trunk page
**      4     Number of leaf pointers on this page
**      *     zero or more pages numbers of leaves
*/
/* #include "sqliteInt.h" */


/* The following value is the maximum cell size assuming a maximum page
** size give above.
*/
#define MX_CELL_SIZE(pBt)  ((int)(pBt->pageSize-8))

/* The maximum number of cells on a single page of the database.  This
** assumes a minimum cell size of 6 bytes  (4 bytes for the cell itself
** plus 2 bytes for the index to the cell in the page header).  Such
** small cells will be rare, but they are possible.
*/
#define MX_CELL(pBt) ((pBt->pageSize-8)/6)

/* Forward declarations */
typedef struct MemPage MemPage;
typedef struct BtLock BtLock;
typedef struct CellInfo CellInfo;

/*
** This is a magic string that appears at the beginning of every
** SQLite database in order to identify the file as a real database.
**
** You can change this value at compile-time by specifying a
** -DSQLITE_FILE_HEADER="..." on the compiler command-line.  The
** header must be exactly 16 bytes including the zero-terminator so
** the string itself should be 15 characters long.  If you change
** the header, then your custom library will not be able to read 
** databases generated by the standard tools and the standard tools
** will not be able to read databases created by your custom library.
*/
#ifndef SQLITE_FILE_HEADER /* 123456789 123456 */
#  define SQLITE_FILE_HEADER "SQLite format 3"
#endif

/*
** Page type flags.  An ORed combination of these flags appear as the
** first byte of on-disk image of every BTree page.
*/
#define PTF_INTKEY    0x01
#define PTF_ZERODATA  0x02
#define PTF_LEAFDATA  0x04
#define PTF_LEAF      0x08

/*
** An instance of this object stores information about each a single database
** page that has been loaded into memory.  The information in this object
** is derived from the raw on-disk page content.
**
** As each database page is loaded into memory, the pager allocats an
** instance of this object and zeros the first 8 bytes.  (This is the
** "extra" information associated with each page of the pager.)
**
** Access to all fields of this structure is controlled by the mutex
** stored in MemPage.pBt->mutex.
*/
struct MemPage {
  u8 isInit;           /* True if previously initialized. MUST BE FIRST! */
  u8 bBusy;            /* Prevent endless loops on corrupt database files */
  u8 intKey;           /* True if table b-trees.  False for index b-trees */
  u8 intKeyLeaf;       /* True if the leaf of an intKey table */
  Pgno pgno;           /* Page number for this page */
  /* Only the first 8 bytes (above) are zeroed by pager.c when a new page
  ** is allocated. All fields that follow must be initialized before use */
  u8 leaf;             /* True if a leaf page */
  u8 hdrOffset;        /* 100 for page 1.  0 otherwise */
  u8 childPtrSize;     /* 0 if leaf==1.  4 if leaf==0 */
  u8 max1bytePayload;  /* min(maxLocal,127) */
  u8 nOverflow;        /* Number of overflow cell bodies in aCell[] */
  u16 maxLocal;        /* Copy of BtShared.maxLocal or BtShared.maxLeaf */
  u16 minLocal;        /* Copy of BtShared.minLocal or BtShared.minLeaf */
  u16 cellOffset;      /* Index in aData of first cell pointer */
  int nFree;           /* Number of free bytes on the page. -1 for unknown */
  u16 nCell;           /* Number of cells on this page, local and ovfl */
  u16 maskPage;        /* Mask for page offset */
  u16 aiOvfl[4];       /* Insert the i-th overflow cell before the aiOvfl-th
                       ** non-overflow cell */
  u8 *apOvfl[4];       /* Pointers to the body of overflow cells */
  BtShared *pBt;       /* Pointer to BtShared that this page is part of */
  u8 *aData;           /* Pointer to disk image of the page data */
  u8 *aDataEnd;        /* One byte past the end of usable data */
  u8 *aCellIdx;        /* The cell index area */
  u8 *aDataOfst;       /* Same as aData for leaves.  aData+4 for interior */
  DbPage *pDbPage;     /* Pager page handle */
  u16 (*xCellSize)(MemPage*,u8*);             /* cellSizePtr method */
  void (*xParseCell)(MemPage*,u8*,CellInfo*); /* btreeParseCell method */
};

/*
** A linked list of the following structures is stored at BtShared.pLock.
** Locks are added (or upgraded from READ_LOCK to WRITE_LOCK) when a cursor 
** is opened on the table with root page BtShared.iTable. Locks are removed
** from this list when a transaction is committed or rolled back, or when
** a btree handle is closed.
*/
struct BtLock {
  Btree *pBtree;        /* Btree handle holding this lock */
  Pgno iTable;          /* Root page of table */
  u8 eLock;             /* READ_LOCK or WRITE_LOCK */
  BtLock *pNext;        /* Next in BtShared.pLock list */
};

/* Candidate values for BtLock.eLock */
#define READ_LOCK     1
#define WRITE_LOCK    2

/* A Btree handle
**
** A database connection contains a pointer to an instance of
** this object for every database file that it has open.  This structure
** is opaque to the database connection.  The database connection cannot
** see the internals of this structure and only deals with pointers to
** this structure.
**
** For some database files, the same underlying database cache might be 
** shared between multiple connections.  In that case, each connection
** has it own instance of this object.  But each instance of this object
** points to the same BtShared object.  The database cache and the
** schema associated with the database file are all contained within
** the BtShared object.
**
** All fields in this structure are accessed under sqlite3.mutex.
** The pBt pointer itself may not be changed while there exists cursors 
** in the referenced BtShared that point back to this Btree since those
** cursors have to go through this Btree to find their BtShared and
** they often do so without holding sqlite3.mutex.
*/
struct Btree {
  sqlite3 *db;       /* The database connection holding this btree */
  BtShared *pBt;     /* Sharable content of this btree */
  u8 inTrans;        /* TRANS_NONE, TRANS_READ or TRANS_WRITE */
  u8 sharable;       /* True if we can share pBt with another db */
  u8 locked;         /* True if db currently has pBt locked */
  u8 hasIncrblobCur; /* True if there are one or more Incrblob cursors */
  int wantToLock;    /* Number of nested calls to sqlite3BtreeEnter() */
  int nBackup;       /* Number of backup operations reading this btree */
  u32 iDataVersion;  /* Combines with pBt->pPager->iDataVersion */
  Btree *pNext;      /* List of other sharable Btrees from the same db */
  Btree *pPrev;      /* Back pointer of the same list */
#ifndef SQLITE_OMIT_SHARED_CACHE
  BtLock lock;       /* Object used to lock page 1 */
#endif
};

/*
** Btree.inTrans may take one of the following values.
**
** If the shared-data extension is enabled, there may be multiple users
** of the Btree structure. At most one of these may open a write transaction,
** but any number may have active read transactions.
*/
#define TRANS_NONE  0
#define TRANS_READ  1
#define TRANS_WRITE 2

/*
** An instance of this object represents a single database file.
** 
** A single database file can be in use at the same time by two
** or more database connections.  When two or more connections are
** sharing the same database file, each connection has it own
** private Btree object for the file and each of those Btrees points
** to this one BtShared object.  BtShared.nRef is the number of
** connections currently sharing this database file.
**
** Fields in this structure are accessed under the BtShared.mutex
** mutex, except for nRef and pNext which are accessed under the
** global SQLITE_MUTEX_STATIC_MASTER mutex.  The pPager field
** may not be modified once it is initially set as long as nRef>0.
** The pSchema field may be set once under BtShared.mutex and
** thereafter is unchanged as long as nRef>0.
**
** isPending:
**
**   If a BtShared client fails to obtain a write-lock on a database
**   table (because there exists one or more read-locks on the table),
**   the shared-cache enters 'pending-lock' state and isPending is
**   set to true.
**
**   The shared-cache leaves the 'pending lock' state when either of
**   the following occur:
**
**     1) The current writer (BtShared.pWriter) concludes its transaction, OR
**     2) The number of locks held by other connections drops to zero.
**
**   while in the 'pending-lock' state, no connection may start a new
**   transaction.
**
**   This feature is included to help prevent writer-starvation.
*/
struct BtShared {
  Pager *pPager;        /* The page cache */
  sqlite3 *db;          /* Database connection currently using this Btree */
  BtCursor *pCursor;    /* A list of all open cursors */
  MemPage *pPage1;      /* First page of the database */
  u8 openFlags;         /* Flags to sqlite3BtreeOpen() */
#ifndef SQLITE_OMIT_AUTOVACUUM
  u8 autoVacuum;        /* True if auto-vacuum is enabled */
  u8 incrVacuum;        /* True if incr-vacuum is enabled */
  u8 bDoTruncate;       /* True to truncate db on commit */
#endif
  u8 inTransaction;     /* Transaction state */
  u8 max1bytePayload;   /* Maximum first byte of cell for a 1-byte payload */
#ifdef SQLITE_HAS_CODEC
  u8 optimalReserve;    /* Desired amount of reserved space per page */
#endif
  u16 btsFlags;         /* Boolean parameters.  See BTS_* macros below */
  u16 maxLocal;         /* Maximum local payload in non-LEAFDATA tables */
  u16 minLocal;         /* Minimum local payload in non-LEAFDATA tables */
  u16 maxLeaf;          /* Maximum local payload in a LEAFDATA table */
  u16 minLeaf;          /* Minimum local payload in a LEAFDATA table */
  u32 pageSize;         /* Total number of bytes on a page */
  u32 usableSize;       /* Number of usable bytes on each page */
  int nTransaction;     /* Number of open transactions (read + write) */
  u32 nPage;            /* Number of pages in the database */
  void *pSchema;        /* Pointer to space allocated by sqlite3BtreeSchema() */
  void (*xFreeSchema)(void*);  /* Destructor for BtShared.pSchema */
  sqlite3_mutex *mutex; /* Non-recursive mutex required to access this object */
  Bitvec *pHasContent;  /* Set of pages moved to free-list this transaction */
#ifndef SQLITE_OMIT_SHARED_CACHE
  int nRef;             /* Number of references to this structure */
  BtShared *pNext;      /* Next on a list of sharable BtShared structs */
  BtLock *pLock;        /* List of locks held on this shared-btree struct */
  Btree *pWriter;       /* Btree with currently open write transaction */
#endif
  u8 *pTmpSpace;        /* Temp space sufficient to hold a single cell */
};

/*
** Allowed values for BtShared.btsFlags
*/
#define BTS_READ_ONLY        0x0001   /* Underlying file is readonly */
#define BTS_PAGESIZE_FIXED   0x0002   /* Page size can no longer be changed */
#define BTS_SECURE_DELETE    0x0004   /* PRAGMA secure_delete is enabled */
#define BTS_OVERWRITE        0x0008   /* Overwrite deleted content with zeros */
#define BTS_FAST_SECURE      0x000c   /* Combination of the previous two */
#define BTS_INITIALLY_EMPTY  0x0010   /* Database was empty at trans start */
#define BTS_NO_WAL           0x0020   /* Do not open write-ahead-log files */
#define BTS_EXCLUSIVE        0x0040   /* pWriter has an exclusive lock */
#define BTS_PENDING          0x0080   /* Waiting for read-locks to clear */

/*
** An instance of the following structure is used to hold information
** about a cell.  The parseCellPtr() function fills in this structure
** based on information extract from the raw disk page.
*/
struct CellInfo {
  i64 nKey;      /* The key for INTKEY tables, or nPayload otherwise */
  u8 *pPayload;  /* Pointer to the start of payload */
  u32 nPayload;  /* Bytes of payload */
  u16 nLocal;    /* Amount of payload held locally, not on overflow */
  u16 nSize;     /* Size of the cell content on the main b-tree page */
};

/*
** Maximum depth of an SQLite B-Tree structure. Any B-Tree deeper than
** this will be declared corrupt. This value is calculated based on a
** maximum database size of 2^31 pages a minimum fanout of 2 for a
** root-node and 3 for all other internal nodes.
**
** If a tree that appears to be taller than this is encountered, it is
** assumed that the database is corrupt.
*/
#define BTCURSOR_MAX_DEPTH 20

/*
** A cursor is a pointer to a particular entry within a particular
** b-tree within a database file.
**
** The entry is identified by its MemPage and the index in
** MemPage.aCell[] of the entry.
**
** A single database file can be shared by two more database connections,
** but cursors cannot be shared.  Each cursor is associated with a
** particular database connection identified BtCursor.pBtree.db.
**
** Fields in this structure are accessed under the BtShared.mutex
** found at self->pBt->mutex. 
**
** skipNext meaning:
** The meaning of skipNext depends on the value of eState:
**
**   eState            Meaning of skipNext
**   VALID             skipNext is meaningless and is ignored
**   INVALID           skipNext is meaningless and is ignored
**   SKIPNEXT          sqlite3BtreeNext() is a no-op if skipNext>0 and
**                     sqlite3BtreePrevious() is no-op if skipNext<0.
**   REQUIRESEEK       restoreCursorPosition() restores the cursor to
**                     eState=SKIPNEXT if skipNext!=0
**   FAULT             skipNext holds the cursor fault error code.
*/
struct BtCursor {
  u8 eState;                /* One of the CURSOR_XXX constants (see below) */
  u8 curFlags;              /* zero or more BTCF_* flags defined below */
  u8 curPagerFlags;         /* Flags to send to sqlite3PagerGet() */
  u8 hints;                 /* As configured by CursorSetHints() */
  int skipNext;    /* Prev() is noop if negative. Next() is noop if positive.
                   ** Error code if eState==CURSOR_FAULT */
  Btree *pBtree;            /* The Btree to which this cursor belongs */
  Pgno *aOverflow;          /* Cache of overflow page locations */
  void *pKey;               /* Saved key that was cursor last known position */
  /* All fields above are zeroed when the cursor is allocated.  See
  ** sqlite3BtreeCursorZero().  Fields that follow must be manually
  ** initialized. */
#define BTCURSOR_FIRST_UNINIT pBt   /* Name of first uninitialized field */
  BtShared *pBt;            /* The BtShared this cursor points to */
  BtCursor *pNext;          /* Forms a linked list of all cursors */
  CellInfo info;            /* A parse of the cell we are pointing at */
  i64 nKey;                 /* Size of pKey, or last integer key */
  Pgno pgnoRoot;            /* The root page of this tree */
  i8 iPage;                 /* Index of current page in apPage */
  u8 curIntKey;             /* Value of apPage[0]->intKey */
  u16 ix;                   /* Current index for apPage[iPage] */
  u16 aiIdx[BTCURSOR_MAX_DEPTH-1];     /* Current index in apPage[i] */
  struct KeyInfo *pKeyInfo;            /* Arg passed to comparison function */
  MemPage *pPage;                        /* Current page */
  MemPage *apPage[BTCURSOR_MAX_DEPTH-1]; /* Stack of parents of current page */
};

/*
** Legal values for BtCursor.curFlags
*/
#define BTCF_WriteFlag    0x01   /* True if a write cursor */
#define BTCF_ValidNKey    0x02   /* True if info.nKey is valid */
#define BTCF_ValidOvfl    0x04   /* True if aOverflow is valid */
#define BTCF_AtLast       0x08   /* Cursor is pointing ot the last entry */
#define BTCF_Incrblob     0x10   /* True if an incremental I/O handle */
#define BTCF_Multiple     0x20   /* Maybe another cursor on the same btree */

/*
** Potential values for BtCursor.eState.
**
** CURSOR_INVALID:
**   Cursor does not point to a valid entry. This can happen (for example) 
**   because the table is empty or because BtreeCursorFirst() has not been
**   called.
**
** CURSOR_VALID:
**   Cursor points to a valid entry. getPayload() etc. may be called.
**
** CURSOR_SKIPNEXT:
**   Cursor is valid except that the Cursor.skipNext field is non-zero
**   indicating that the next sqlite3BtreeNext() or sqlite3BtreePrevious()
**   operation should be a no-op.
**
** CURSOR_REQUIRESEEK:
**   The table that this cursor was opened on still exists, but has been 
**   modified since the cursor was last used. The cursor position is saved
**   in variables BtCursor.pKey and BtCursor.nKey. When a cursor is in 
**   this state, restoreCursorPosition() can be called to attempt to
**   seek the cursor to the saved position.
**
** CURSOR_FAULT:
**   An unrecoverable error (an I/O error or a malloc failure) has occurred
**   on a different connection that shares the BtShared cache with this
**   cursor.  The error has left the cache in an inconsistent state.
**   Do nothing else with this cursor.  Any attempt to use the cursor
**   should return the error code stored in BtCursor.skipNext
*/
#define CURSOR_VALID             0
#define CURSOR_INVALID           1
#define CURSOR_SKIPNEXT          2
#define CURSOR_REQUIRESEEK       3
#define CURSOR_FAULT             4

/* 
** The database page the PENDING_BYTE occupies. This page is never used.
*/
# define PENDING_BYTE_PAGE(pBt) PAGER_MJ_PGNO(pBt)

/*
** These macros define the location of the pointer-map entry for a 
** database page. The first argument to each is the number of usable
** bytes on each page of the database (often 1024). The second is the
** page number to look up in the pointer map.
**
** PTRMAP_PAGENO returns the database page number of the pointer-map
** page that stores the required pointer. PTRMAP_PTROFFSET returns
** the offset of the requested map entry.
**
** If the pgno argument passed to PTRMAP_PAGENO is a pointer-map page,
** then pgno is returned. So (pgno==PTRMAP_PAGENO(pgsz, pgno)) can be
** used to test if pgno is a pointer-map page. PTRMAP_ISPAGE implements
** this test.
*/
#define PTRMAP_PAGENO(pBt, pgno) ptrmapPageno(pBt, pgno)
#define PTRMAP_PTROFFSET(pgptrmap, pgno) (5*(pgno-pgptrmap-1))
#define PTRMAP_ISPAGE(pBt, pgno) (PTRMAP_PAGENO((pBt),(pgno))==(pgno))

/*
** The pointer map is a lookup table that identifies the parent page for
** each child page in the database file.  The parent page is the page that
** contains a pointer to the child.  Every page in the database contains
** 0 or 1 parent pages.  (In this context 'database page' refers
** to any page that is not part of the pointer map itself.)  Each pointer map
** entry consists of a single byte 'type' and a 4 byte parent page number.
** The PTRMAP_XXX identifiers below are the valid types.
**
** The purpose of the pointer map is to facility moving pages from one
** position in the file to another as part of autovacuum.  When a page
** is moved, the pointer in its parent must be updated to point to the
** new location.  The pointer map is used to locate the parent page quickly.
**
** PTRMAP_ROOTPAGE: The database page is a root-page. The page-number is not
**                  used in this case.
**
** PTRMAP_FREEPAGE: The database page is an unused (free) page. The page-number 
**                  is not used in this case.
**
** PTRMAP_OVERFLOW1: The database page is the first page in a list of 
**                   overflow pages. The page number identifies the page that
**                   contains the cell with a pointer to this overflow page.
**
** PTRMAP_OVERFLOW2: The database page is the second or later page in a list of
**                   overflow pages. The page-number identifies the previous
**                   page in the overflow page list.
**
** PTRMAP_BTREE: The database page is a non-root btree page. The page number
**               identifies the parent page in the btree.
*/
#define PTRMAP_ROOTPAGE 1
#define PTRMAP_FREEPAGE 2
#define PTRMAP_OVERFLOW1 3
#define PTRMAP_OVERFLOW2 4
#define PTRMAP_BTREE 5

/* A bunch of assert() statements to check the transaction state variables
** of handle p (type Btree*) are internally consistent.
*/
#define btreeIntegrity(p) \
  assert( p->pBt->inTransaction!=TRANS_NONE || p->pBt->nTransaction==0 ); \
  assert( p->pBt->inTransaction>=p->inTrans ); 


/*
** The ISAUTOVACUUM macro is used within balance_nonroot() to determine
** if the database supports auto-vacuum or not. Because it is used
** within an expression that is an argument to another macro 
** (sqliteMallocRaw), it is not possible to use conditional compilation.
** So, this macro is defined instead.
*/
#ifndef SQLITE_OMIT_AUTOVACUUM
#define ISAUTOVACUUM (pBt->autoVacuum)
#else
#define ISAUTOVACUUM 0
#endif


/*
** This structure is passed around through all the sanity checking routines
** in order to keep track of some global state information.
**
** The aRef[] array is allocated so that there is 1 bit for each page in
** the database. As the integrity-check proceeds, for each page used in
** the database the corresponding bit is set. This allows integrity-check to 
** detect pages that are used twice and orphaned pages (both of which 
** indicate corruption).
*/
typedef struct IntegrityCk IntegrityCk;
struct IntegrityCk {
  BtShared *pBt;    /* The tree being checked out */
  Pager *pPager;    /* The associated pager.  Also accessible by pBt->pPager */
  u8 *aPgRef;       /* 1 bit per page in the db (see above) */
  Pgno nPage;       /* Number of pages in the database */
  int mxErr;        /* Stop accumulating errors when this reaches zero */
  int nErr;         /* Number of messages written to zErrMsg so far */
  int mallocFailed; /* A memory allocation error has occurred */
  const char *zPfx; /* Error message prefix */
  int v1, v2;       /* Values for up to two %d fields in zPfx */
  StrAccum errMsg;  /* Accumulate the error message text here */
  u32 *heap;        /* Min-heap used for analyzing cell coverage */
};

/*
** Routines to read or write a two- and four-byte big-endian integer values.
*/
#define get2byte(x)   ((x)[0]<<8 | (x)[1])
#define put2byte(p,v) ((p)[0] = (u8)((v)>>8), (p)[1] = (u8)(v))
#define get4byte sqlite3Get4byte
#define put4byte sqlite3Put4byte

/*
** get2byteAligned(), unlike get2byte(), requires that its argument point to a
** two-byte aligned address.  get2bytea() is only used for accessing the
** cell addresses in a btree header.
*/
#if SQLITE_BYTEORDER==4321
# define get2byteAligned(x)  (*(u16*)(x))
#elif SQLITE_BYTEORDER==1234 && GCC_VERSION>=4008000
# define get2byteAligned(x)  __builtin_bswap16(*(u16*)(x))
#elif SQLITE_BYTEORDER==1234 && MSVC_VERSION>=1300
# define get2byteAligned(x)  _byteswap_ushort(*(u16*)(x))
#else
# define get2byteAligned(x)  ((x)[0]<<8 | (x)[1])
#endif

/************** End of btreeInt.h ********************************************/
/************** Continuing where we left off in btmutex.c ********************/
#ifndef SQLITE_OMIT_SHARED_CACHE
#if SQLITE_THREADSAFE

/*
** Obtain the BtShared mutex associated with B-Tree handle p. Also,
** set BtShared.db to the database handle associated with p and the
** p->locked boolean to true.
*/
static void lockBtreeMutex(Btree *p){
  assert( p->locked==0 );
  assert( sqlite3_mutex_notheld(p->pBt->mutex) );
  assert( sqlite3_mutex_held(p->db->mutex) );

  sqlite3_mutex_enter(p->pBt->mutex);
  p->pBt->db = p->db;
  p->locked = 1;
}

/*
** Release the BtShared mutex associated with B-Tree handle p and
** clear the p->locked boolean.
*/
static void SQLITE_NOINLINE unlockBtreeMutex(Btree *p){
  BtShared *pBt = p->pBt;
  assert( p->locked==1 );
  assert( sqlite3_mutex_held(pBt->mutex) );
  assert( sqlite3_mutex_held(p->db->mutex) );
  assert( p->db==pBt->db );

  sqlite3_mutex_leave(pBt->mutex);
  p->locked = 0;
}

/* Forward reference */
static void SQLITE_NOINLINE btreeLockCarefully(Btree *p);

/*
** Enter a mutex on the given BTree object.
**
** If the object is not sharable, then no mutex is ever required
** and this routine is a no-op.  The underlying mutex is non-recursive.
** But we keep a reference count in Btree.wantToLock so the behavior
** of this interface is recursive.
**
** To avoid deadlocks, multiple Btrees are locked in the same order
** by all database connections.  The p->pNext is a list of other
** Btrees belonging to the same database connection as the p Btree
** which need to be locked after p.  If we cannot get a lock on
** p, then first unlock all of the others on p->pNext, then wait
** for the lock to become available on p, then relock all of the
** subsequent Btrees that desire a lock.
*/
SQLITE_PRIVATE void sqlite3BtreeEnter(Btree *p){
  /* Some basic sanity checking on the Btree.  The list of Btrees
  ** connected by pNext and pPrev should be in sorted order by
  ** Btree.pBt value. All elements of the list should belong to
  ** the same connection. Only shared Btrees are on the list. */
  assert( p->pNext==0 || p->pNext->pBt>p->pBt );
  assert( p->pPrev==0 || p->pPrev->pBt<p->pBt );
  assert( p->pNext==0 || p->pNext->db==p->db );
  assert( p->pPrev==0 || p->pPrev->db==p->db );
  assert( p->sharable || (p->pNext==0 && p->pPrev==0) );

  /* Check for locking consistency */
  assert( !p->locked || p->wantToLock>0 );
  assert( p->sharable || p->wantToLock==0 );

  /* We should already hold a lock on the database connection */
  assert( sqlite3_mutex_held(p->db->mutex) );

  /* Unless the database is sharable and unlocked, then BtShared.db
  ** should already be set correctly. */
  assert( (p->locked==0 && p->sharable) || p->pBt->db==p->db );

  if( !p->sharable ) return;
  p->wantToLock++;
  if( p->locked ) return;
  btreeLockCarefully(p);
}

/* This is a helper function for sqlite3BtreeLock(). By moving
** complex, but seldom used logic, out of sqlite3BtreeLock() and
** into this routine, we avoid unnecessary stack pointer changes
** and thus help the sqlite3BtreeLock() routine to run much faster
** in the common case.
*/
static void SQLITE_NOINLINE btreeLockCarefully(Btree *p){
  Btree *pLater;

  /* In most cases, we should be able to acquire the lock we
  ** want without having to go through the ascending lock
  ** procedure that follows.  Just be sure not to block.
  */
  if( sqlite3_mutex_try(p->pBt->mutex)==SQLITE_OK ){
    p->pBt->db = p->db;
    p->locked = 1;
    return;
  }

  /* To avoid deadlock, first release all locks with a larger
  ** BtShared address.  Then acquire our lock.  Then reacquire
  ** the other BtShared locks that we used to hold in ascending
  ** order.
  */
  for(pLater=p->pNext; pLater; pLater=pLater->pNext){
    assert( pLater->sharable );
    assert( pLater->pNext==0 || pLater->pNext->pBt>pLater->pBt );
    assert( !pLater->locked || pLater->wantToLock>0 );
    if( pLater->locked ){
      unlockBtreeMutex(pLater);
    }
  }
  lockBtreeMutex(p);
  for(pLater=p->pNext; pLater; pLater=pLater->pNext){
    if( pLater->wantToLock ){
      lockBtreeMutex(pLater);
    }
  }
}


/*
** Exit the recursive mutex on a Btree.
*/
SQLITE_PRIVATE void sqlite3BtreeLeave(Btree *p){
  assert( sqlite3_mutex_held(p->db->mutex) );
  if( p->sharable ){
    assert( p->wantToLock>0 );
    p->wantToLock--;
    if( p->wantToLock==0 ){
      unlockBtreeMutex(p);
    }
  }
}

#ifndef NDEBUG
/*
** Return true if the BtShared mutex is held on the btree, or if the
** B-Tree is not marked as sharable.
**
** This routine is used only from within assert() statements.
*/
SQLITE_PRIVATE int sqlite3BtreeHoldsMutex(Btree *p){
  assert( p->sharable==0 || p->locked==0 || p->wantToLock>0 );
  assert( p->sharable==0 || p->locked==0 || p->db==p->pBt->db );
  assert( p->sharable==0 || p->locked==0 || sqlite3_mutex_held(p->pBt->mutex) );
  assert( p->sharable==0 || p->locked==0 || sqlite3_mutex_held(p->db->mutex) );

  return (p->sharable==0 || p->locked);
}
#endif


/*
** Enter the mutex on every Btree associated with a database
** connection.  This is needed (for example) prior to parsing
** a statement since we will be comparing table and column names
** against all schemas and we do not want those schemas being
** reset out from under us.
**
** There is a corresponding leave-all procedures.
**
** Enter the mutexes in accending order by BtShared pointer address
** to avoid the possibility of deadlock when two threads with
** two or more btrees in common both try to lock all their btrees
** at the same instant.
*/
static void SQLITE_NOINLINE btreeEnterAll(sqlite3 *db){
  int i;
  int skipOk = 1;
  Btree *p;
  assert( sqlite3_mutex_held(db->mutex) );
  for(i=0; i<db->nDb; i++){
    p = db->aDb[i].pBt;
    if( p && p->sharable ){
      sqlite3BtreeEnter(p);
      skipOk = 0;
    }
  }
  db->noSharedCache = skipOk;
}
SQLITE_PRIVATE void sqlite3BtreeEnterAll(sqlite3 *db){
  if( db->noSharedCache==0 ) btreeEnterAll(db);
}
static void SQLITE_NOINLINE btreeLeaveAll(sqlite3 *db){
  int i;
  Btree *p;
  assert( sqlite3_mutex_held(db->mutex) );
  for(i=0; i<db->nDb; i++){
    p = db->aDb[i].pBt;
    if( p ) sqlite3BtreeLeave(p);
  }
}
SQLITE_PRIVATE void sqlite3BtreeLeaveAll(sqlite3 *db){
  if( db->noSharedCache==0 ) btreeLeaveAll(db);
}

#ifndef NDEBUG
/*
** Return true if the current thread holds the database connection
** mutex and all required BtShared mutexes.
**
** This routine is used inside assert() statements only.
*/
SQLITE_PRIVATE int sqlite3BtreeHoldsAllMutexes(sqlite3 *db){
  int i;
  if( !sqlite3_mutex_held(db->mutex) ){
    return 0;
  }
  for(i=0; i<db->nDb; i++){
    Btree *p;
    p = db->aDb[i].pBt;
    if( p && p->sharable &&
         (p->wantToLock==0 || !sqlite3_mutex_held(p->pBt->mutex)) ){
      return 0;
    }
  }
  return 1;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/*
** Return true if the correct mutexes are held for accessing the
** db->aDb[iDb].pSchema structure.  The mutexes required for schema
** access are:
**
**   (1) The mutex on db
**   (2) if iDb!=1, then the mutex on db->aDb[iDb].pBt.
**
** If pSchema is not NULL, then iDb is computed from pSchema and
** db using sqlite3SchemaToIndex().
*/
SQLITE_PRIVATE int sqlite3SchemaMutexHeld(sqlite3 *db, int iDb, Schema *pSchema){
  Btree *p;
  assert( db!=0 );
  if( pSchema ) iDb = sqlite3SchemaToIndex(db, pSchema);
  assert( iDb>=0 && iDb<db->nDb );
  if( !sqlite3_mutex_held(db->mutex) ) return 0;
  if( iDb==1 ) return 1;
  p = db->aDb[iDb].pBt;
  assert( p!=0 );
  return p->sharable==0 || p->locked==1;
}
#endif /* NDEBUG */

#else /* SQLITE_THREADSAFE>0 above.  SQLITE_THREADSAFE==0 below */
/*
** The following are special cases for mutex enter routines for use
** in single threaded applications that use shared cache.  Except for
** these two routines, all mutex operations are no-ops in that case and
** are null #defines in btree.h.
**
** If shared cache is disabled, then all btree mutex routines, including
** the ones below, are no-ops and are null #defines in btree.h.
*/

SQLITE_PRIVATE void sqlite3BtreeEnter(Btree *p){
  p->pBt->db = p->db;
}
SQLITE_PRIVATE void sqlite3BtreeEnterAll(sqlite3 *db){
  int i;
  for(i=0; i<db->nDb; i++){
    Btree *p = db->aDb[i].pBt;
    if( p ){
      p->pBt->db = p->db;
    }
  }
}
#endif /* if SQLITE_THREADSAFE */

#ifndef SQLITE_OMIT_INCRBLOB
/*
** Enter a mutex on a Btree given a cursor owned by that Btree. 
**
** These entry points are used by incremental I/O only. Enter() is required 
** any time OMIT_SHARED_CACHE is not defined, regardless of whether or not 
** the build is threadsafe. Leave() is only required by threadsafe builds.
*/
SQLITE_PRIVATE void sqlite3BtreeEnterCursor(BtCursor *pCur){
  sqlite3BtreeEnter(pCur->pBtree);
}
# if SQLITE_THREADSAFE
SQLITE_PRIVATE void sqlite3BtreeLeaveCursor(BtCursor *pCur){
  sqlite3BtreeLeave(pCur->pBtree);
}
# endif
#endif /* ifndef SQLITE_OMIT_INCRBLOB */

#endif /* ifndef SQLITE_OMIT_SHARED_CACHE */

/************** End of btmutex.c *********************************************/
