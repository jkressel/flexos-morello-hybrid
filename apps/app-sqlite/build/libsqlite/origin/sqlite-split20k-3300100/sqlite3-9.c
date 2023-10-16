/************** Begin file wherecode.c ***************************************/
/*
** 2015-06-06
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This module contains C code that generates VDBE code used to process
** the WHERE clause of SQL statements.
**
** This file was split off from where.c on 2015-06-06 in order to reduce the
** size of where.c and make it easier to edit.  This file contains the routines
** that actually generate the bulk of the WHERE loop code.  The original where.c
** file retains the code that does query planning and analysis.
*/
/* #include "sqliteInt.h" */
/************** Include whereInt.h in the middle of wherecode.c **************/
/************** Begin file whereInt.h ****************************************/
/*
** 2013-11-12
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
** This file contains structure and macro definitions for the query
** planner logic in "where.c".  These definitions are broken out into
** a separate source file for easier editing.
*/
#ifndef SQLITE_WHEREINT_H
#define SQLITE_WHEREINT_H

/*
** Trace output macros
*/
#if defined(SQLITE_TEST) || defined(SQLITE_DEBUG)
/***/ extern int sqlite3WhereTrace;
#endif
#if defined(SQLITE_DEBUG) \
    && (defined(SQLITE_TEST) || defined(SQLITE_ENABLE_WHERETRACE))
# define WHERETRACE(K,X)  if(sqlite3WhereTrace&(K)) sqlite3DebugPrintf X
# define WHERETRACE_ENABLED 1
#else
# define WHERETRACE(K,X)
#endif

/* Forward references
*/
typedef struct WhereClause WhereClause;
typedef struct WhereMaskSet WhereMaskSet;
typedef struct WhereOrInfo WhereOrInfo;
typedef struct WhereAndInfo WhereAndInfo;
typedef struct WhereLevel WhereLevel;
typedef struct WhereLoop WhereLoop;
typedef struct WherePath WherePath;
typedef struct WhereTerm WhereTerm;
typedef struct WhereLoopBuilder WhereLoopBuilder;
typedef struct WhereScan WhereScan;
typedef struct WhereOrCost WhereOrCost;
typedef struct WhereOrSet WhereOrSet;

/*
** This object contains information needed to implement a single nested
** loop in WHERE clause.
**
** Contrast this object with WhereLoop.  This object describes the
** implementation of the loop.  WhereLoop describes the algorithm.
** This object contains a pointer to the WhereLoop algorithm as one of
** its elements.
**
** The WhereInfo object contains a single instance of this object for
** each term in the FROM clause (which is to say, for each of the
** nested loops as implemented).  The order of WhereLevel objects determines
** the loop nested order, with WhereInfo.a[0] being the outer loop and
** WhereInfo.a[WhereInfo.nLevel-1] being the inner loop.
*/
struct WhereLevel {
  int iLeftJoin;        /* Memory cell used to implement LEFT OUTER JOIN */
  int iTabCur;          /* The VDBE cursor used to access the table */
  int iIdxCur;          /* The VDBE cursor used to access pIdx */
  int addrBrk;          /* Jump here to break out of the loop */
  int addrNxt;          /* Jump here to start the next IN combination */
  int addrSkip;         /* Jump here for next iteration of skip-scan */
  int addrCont;         /* Jump here to continue with the next loop cycle */
  int addrFirst;        /* First instruction of interior of the loop */
  int addrBody;         /* Beginning of the body of this loop */
  int regBignull;       /* big-null flag reg. True if a NULL-scan is needed */
  int addrBignull;      /* Jump here for next part of big-null scan */
#ifndef SQLITE_LIKE_DOESNT_MATCH_BLOBS
  u32 iLikeRepCntr;     /* LIKE range processing counter register (times 2) */
  int addrLikeRep;      /* LIKE range processing address */
#endif
  u8 iFrom;             /* Which entry in the FROM clause */
  u8 op, p3, p5;        /* Opcode, P3 & P5 of the opcode that ends the loop */
  int p1, p2;           /* Operands of the opcode used to end the loop */
  union {               /* Information that depends on pWLoop->wsFlags */
    struct {
      int nIn;              /* Number of entries in aInLoop[] */
      struct InLoop {
        int iCur;              /* The VDBE cursor used by this IN operator */
        int addrInTop;         /* Top of the IN loop */
        int iBase;             /* Base register of multi-key index record */
        int nPrefix;           /* Number of prior entires in the key */
        u8 eEndLoopOp;         /* IN Loop terminator. OP_Next or OP_Prev */
      } *aInLoop;           /* Information about each nested IN operator */
    } in;                 /* Used when pWLoop->wsFlags&WHERE_IN_ABLE */
    Index *pCovidx;       /* Possible covering index for WHERE_MULTI_OR */
  } u;
  struct WhereLoop *pWLoop;  /* The selected WhereLoop object */
  Bitmask notReady;          /* FROM entries not usable at this level */
#ifdef SQLITE_ENABLE_STMT_SCANSTATUS
  int addrVisit;        /* Address at which row is visited */
#endif
};

/*
** Each instance of this object represents an algorithm for evaluating one
** term of a join.  Every term of the FROM clause will have at least
** one corresponding WhereLoop object (unless INDEXED BY constraints
** prevent a query solution - which is an error) and many terms of the
** FROM clause will have multiple WhereLoop objects, each describing a
** potential way of implementing that FROM-clause term, together with
** dependencies and cost estimates for using the chosen algorithm.
**
** Query planning consists of building up a collection of these WhereLoop
** objects, then computing a particular sequence of WhereLoop objects, with
** one WhereLoop object per FROM clause term, that satisfy all dependencies
** and that minimize the overall cost.
*/
struct WhereLoop {
  Bitmask prereq;       /* Bitmask of other loops that must run first */
  Bitmask maskSelf;     /* Bitmask identifying table iTab */
#ifdef SQLITE_DEBUG
  char cId;             /* Symbolic ID of this loop for debugging use */
#endif
  u8 iTab;              /* Position in FROM clause of table for this loop */
  u8 iSortIdx;          /* Sorting index number.  0==None */
  LogEst rSetup;        /* One-time setup cost (ex: create transient index) */
  LogEst rRun;          /* Cost of running each loop */
  LogEst nOut;          /* Estimated number of output rows */
  union {
    struct {               /* Information for internal btree tables */
      u16 nEq;               /* Number of equality constraints */
      u16 nBtm;              /* Size of BTM vector */
      u16 nTop;              /* Size of TOP vector */
      u16 nDistinctCol;      /* Index columns used to sort for DISTINCT */
      Index *pIndex;         /* Index used, or NULL */
    } btree;
    struct {               /* Information for virtual tables */
      int idxNum;            /* Index number */
      u8 needFree;           /* True if sqlite3_free(idxStr) is needed */
      i8 isOrdered;          /* True if satisfies ORDER BY */
      u16 omitMask;          /* Terms that may be omitted */
      char *idxStr;          /* Index identifier string */
    } vtab;
  } u;
  u32 wsFlags;          /* WHERE_* flags describing the plan */
  u16 nLTerm;           /* Number of entries in aLTerm[] */
  u16 nSkip;            /* Number of NULL aLTerm[] entries */
  /**** whereLoopXfer() copies fields above ***********************/
# define WHERE_LOOP_XFER_SZ offsetof(WhereLoop,nLSlot)
  u16 nLSlot;           /* Number of slots allocated for aLTerm[] */
  WhereTerm **aLTerm;   /* WhereTerms used */
  WhereLoop *pNextLoop; /* Next WhereLoop object in the WhereClause */
  WhereTerm *aLTermSpace[3];  /* Initial aLTerm[] space */
};

/* This object holds the prerequisites and the cost of running a
** subquery on one operand of an OR operator in the WHERE clause.
** See WhereOrSet for additional information 
*/
struct WhereOrCost {
  Bitmask prereq;     /* Prerequisites */
  LogEst rRun;        /* Cost of running this subquery */
  LogEst nOut;        /* Number of outputs for this subquery */
};

/* The WhereOrSet object holds a set of possible WhereOrCosts that
** correspond to the subquery(s) of OR-clause processing.  Only the
** best N_OR_COST elements are retained.
*/
#define N_OR_COST 3
struct WhereOrSet {
  u16 n;                      /* Number of valid a[] entries */
  WhereOrCost a[N_OR_COST];   /* Set of best costs */
};

/*
** Each instance of this object holds a sequence of WhereLoop objects
** that implement some or all of a query plan.
**
** Think of each WhereLoop object as a node in a graph with arcs
** showing dependencies and costs for travelling between nodes.  (That is
** not a completely accurate description because WhereLoop costs are a
** vector, not a scalar, and because dependencies are many-to-one, not
** one-to-one as are graph nodes.  But it is a useful visualization aid.)
** Then a WherePath object is a path through the graph that visits some
** or all of the WhereLoop objects once.
**
** The "solver" works by creating the N best WherePath objects of length
** 1.  Then using those as a basis to compute the N best WherePath objects
** of length 2.  And so forth until the length of WherePaths equals the
** number of nodes in the FROM clause.  The best (lowest cost) WherePath
** at the end is the chosen query plan.
*/
struct WherePath {
  Bitmask maskLoop;     /* Bitmask of all WhereLoop objects in this path */
  Bitmask revLoop;      /* aLoop[]s that should be reversed for ORDER BY */
  LogEst nRow;          /* Estimated number of rows generated by this path */
  LogEst rCost;         /* Total cost of this path */
  LogEst rUnsorted;     /* Total cost of this path ignoring sorting costs */
  i8 isOrdered;         /* No. of ORDER BY terms satisfied. -1 for unknown */
  WhereLoop **aLoop;    /* Array of WhereLoop objects implementing this path */
};

/*
** The query generator uses an array of instances of this structure to
** help it analyze the subexpressions of the WHERE clause.  Each WHERE
** clause subexpression is separated from the others by AND operators,
** usually, or sometimes subexpressions separated by OR.
**
** All WhereTerms are collected into a single WhereClause structure.  
** The following identity holds:
**
**        WhereTerm.pWC->a[WhereTerm.idx] == WhereTerm
**
** When a term is of the form:
**
**              X <op> <expr>
**
** where X is a column name and <op> is one of certain operators,
** then WhereTerm.leftCursor and WhereTerm.u.leftColumn record the
** cursor number and column number for X.  WhereTerm.eOperator records
** the <op> using a bitmask encoding defined by WO_xxx below.  The
** use of a bitmask encoding for the operator allows us to search
** quickly for terms that match any of several different operators.
**
** A WhereTerm might also be two or more subterms connected by OR:
**
**         (t1.X <op> <expr>) OR (t1.Y <op> <expr>) OR ....
**
** In this second case, wtFlag has the TERM_ORINFO bit set and eOperator==WO_OR
** and the WhereTerm.u.pOrInfo field points to auxiliary information that
** is collected about the OR clause.
**
** If a term in the WHERE clause does not match either of the two previous
** categories, then eOperator==0.  The WhereTerm.pExpr field is still set
** to the original subexpression content and wtFlags is set up appropriately
** but no other fields in the WhereTerm object are meaningful.
**
** When eOperator!=0, prereqRight and prereqAll record sets of cursor numbers,
** but they do so indirectly.  A single WhereMaskSet structure translates
** cursor number into bits and the translated bit is stored in the prereq
** fields.  The translation is used in order to maximize the number of
** bits that will fit in a Bitmask.  The VDBE cursor numbers might be
** spread out over the non-negative integers.  For example, the cursor
** numbers might be 3, 8, 9, 10, 20, 23, 41, and 45.  The WhereMaskSet
** translates these sparse cursor numbers into consecutive integers
** beginning with 0 in order to make the best possible use of the available
** bits in the Bitmask.  So, in the example above, the cursor numbers
** would be mapped into integers 0 through 7.
**
** The number of terms in a join is limited by the number of bits
** in prereqRight and prereqAll.  The default is 64 bits, hence SQLite
** is only able to process joins with 64 or fewer tables.
*/
struct WhereTerm {
  Expr *pExpr;            /* Pointer to the subexpression that is this term */
  WhereClause *pWC;       /* The clause this term is part of */
  LogEst truthProb;       /* Probability of truth for this expression */
  u16 wtFlags;            /* TERM_xxx bit flags.  See below */
  u16 eOperator;          /* A WO_xx value describing <op> */
  u8 nChild;              /* Number of children that must disable us */
  u8 eMatchOp;            /* Op for vtab MATCH/LIKE/GLOB/REGEXP terms */
  int iParent;            /* Disable pWC->a[iParent] when this term disabled */
  int leftCursor;         /* Cursor number of X in "X <op> <expr>" */
  int iField;             /* Field in (?,?,?) IN (SELECT...) vector */
  union {
    int leftColumn;         /* Column number of X in "X <op> <expr>" */
    WhereOrInfo *pOrInfo;   /* Extra information if (eOperator & WO_OR)!=0 */
    WhereAndInfo *pAndInfo; /* Extra information if (eOperator& WO_AND)!=0 */
  } u;
  Bitmask prereqRight;    /* Bitmask of tables used by pExpr->pRight */
  Bitmask prereqAll;      /* Bitmask of tables referenced by pExpr */
};

/*
** Allowed values of WhereTerm.wtFlags
*/
#define TERM_DYNAMIC    0x01   /* Need to call sqlite3ExprDelete(db, pExpr) */
#define TERM_VIRTUAL    0x02   /* Added by the optimizer.  Do not code */
#define TERM_CODED      0x04   /* This term is already coded */
#define TERM_COPIED     0x08   /* Has a child */
#define TERM_ORINFO     0x10   /* Need to free the WhereTerm.u.pOrInfo object */
#define TERM_ANDINFO    0x20   /* Need to free the WhereTerm.u.pAndInfo obj */
#define TERM_OR_OK      0x40   /* Used during OR-clause processing */
#ifdef SQLITE_ENABLE_STAT4
#  define TERM_VNULL    0x80   /* Manufactured x>NULL or x<=NULL term */
#else
#  define TERM_VNULL    0x00   /* Disabled if not using stat4 */
#endif
#define TERM_LIKEOPT    0x100  /* Virtual terms from the LIKE optimization */
#define TERM_LIKECOND   0x200  /* Conditionally this LIKE operator term */
#define TERM_LIKE       0x400  /* The original LIKE operator */
#define TERM_IS         0x800  /* Term.pExpr is an IS operator */
#define TERM_VARSELECT  0x1000 /* Term.pExpr contains a correlated sub-query */
#define TERM_NOPARTIDX  0x2000 /* Not for use to enable a partial index */

/*
** An instance of the WhereScan object is used as an iterator for locating
** terms in the WHERE clause that are useful to the query planner.
*/
struct WhereScan {
  WhereClause *pOrigWC;      /* Original, innermost WhereClause */
  WhereClause *pWC;          /* WhereClause currently being scanned */
  const char *zCollName;     /* Required collating sequence, if not NULL */
  Expr *pIdxExpr;            /* Search for this index expression */
  char idxaff;               /* Must match this affinity, if zCollName!=NULL */
  unsigned char nEquiv;      /* Number of entries in aEquiv[] */
  unsigned char iEquiv;      /* Next unused slot in aEquiv[] */
  u32 opMask;                /* Acceptable operators */
  int k;                     /* Resume scanning at this->pWC->a[this->k] */
  int aiCur[11];             /* Cursors in the equivalence class */
  i16 aiColumn[11];          /* Corresponding column number in the eq-class */
};

/*
** An instance of the following structure holds all information about a
** WHERE clause.  Mostly this is a container for one or more WhereTerms.
**
** Explanation of pOuter:  For a WHERE clause of the form
**
**           a AND ((b AND c) OR (d AND e)) AND f
**
** There are separate WhereClause objects for the whole clause and for
** the subclauses "(b AND c)" and "(d AND e)".  The pOuter field of the
** subclauses points to the WhereClause object for the whole clause.
*/
struct WhereClause {
  WhereInfo *pWInfo;       /* WHERE clause processing context */
  WhereClause *pOuter;     /* Outer conjunction */
  u8 op;                   /* Split operator.  TK_AND or TK_OR */
  u8 hasOr;                /* True if any a[].eOperator is WO_OR */
  int nTerm;               /* Number of terms */
  int nSlot;               /* Number of entries in a[] */
  WhereTerm *a;            /* Each a[] describes a term of the WHERE cluase */
#if defined(SQLITE_SMALL_STACK)
  WhereTerm aStatic[1];    /* Initial static space for a[] */
#else
  WhereTerm aStatic[8];    /* Initial static space for a[] */
#endif
};

/*
** A WhereTerm with eOperator==WO_OR has its u.pOrInfo pointer set to
** a dynamically allocated instance of the following structure.
*/
struct WhereOrInfo {
  WhereClause wc;          /* Decomposition into subterms */
  Bitmask indexable;       /* Bitmask of all indexable tables in the clause */
};

/*
** A WhereTerm with eOperator==WO_AND has its u.pAndInfo pointer set to
** a dynamically allocated instance of the following structure.
*/
struct WhereAndInfo {
  WhereClause wc;          /* The subexpression broken out */
};

/*
** An instance of the following structure keeps track of a mapping
** between VDBE cursor numbers and bits of the bitmasks in WhereTerm.
**
** The VDBE cursor numbers are small integers contained in 
** SrcList_item.iCursor and Expr.iTable fields.  For any given WHERE 
** clause, the cursor numbers might not begin with 0 and they might
** contain gaps in the numbering sequence.  But we want to make maximum
** use of the bits in our bitmasks.  This structure provides a mapping
** from the sparse cursor numbers into consecutive integers beginning
** with 0.
**
** If WhereMaskSet.ix[A]==B it means that The A-th bit of a Bitmask
** corresponds VDBE cursor number B.  The A-th bit of a bitmask is 1<<A.
**
** For example, if the WHERE clause expression used these VDBE
** cursors:  4, 5, 8, 29, 57, 73.  Then the  WhereMaskSet structure
** would map those cursor numbers into bits 0 through 5.
**
** Note that the mapping is not necessarily ordered.  In the example
** above, the mapping might go like this:  4->3, 5->1, 8->2, 29->0,
** 57->5, 73->4.  Or one of 719 other combinations might be used. It
** does not really matter.  What is important is that sparse cursor
** numbers all get mapped into bit numbers that begin with 0 and contain
** no gaps.
*/
struct WhereMaskSet {
  int bVarSelect;               /* Used by sqlite3WhereExprUsage() */
  int n;                        /* Number of assigned cursor values */
  int ix[BMS];                  /* Cursor assigned to each bit */
};

/*
** Initialize a WhereMaskSet object
*/
#define initMaskSet(P)  (P)->n=0

/*
** This object is a convenience wrapper holding all information needed
** to construct WhereLoop objects for a particular query.
*/
struct WhereLoopBuilder {
  WhereInfo *pWInfo;        /* Information about this WHERE */
  WhereClause *pWC;         /* WHERE clause terms */
  ExprList *pOrderBy;       /* ORDER BY clause */
  WhereLoop *pNew;          /* Template WhereLoop */
  WhereOrSet *pOrSet;       /* Record best loops here, if not NULL */
#ifdef SQLITE_ENABLE_STAT4
  UnpackedRecord *pRec;     /* Probe for stat4 (if required) */
  int nRecValid;            /* Number of valid fields currently in pRec */
#endif
  unsigned int bldFlags;    /* SQLITE_BLDF_* flags */
  unsigned int iPlanLimit;  /* Search limiter */
};

/* Allowed values for WhereLoopBuider.bldFlags */
#define SQLITE_BLDF_INDEXED  0x0001   /* An index is used */
#define SQLITE_BLDF_UNIQUE   0x0002   /* All keys of a UNIQUE index used */

/* The WhereLoopBuilder.iPlanLimit is used to limit the number of
** index+constraint combinations the query planner will consider for a
** particular query.  If this parameter is unlimited, then certain
** pathological queries can spend excess time in the sqlite3WhereBegin()
** routine.  The limit is high enough that is should not impact real-world
** queries.
**
** SQLITE_QUERY_PLANNER_LIMIT is the baseline limit.  The limit is
** increased by SQLITE_QUERY_PLANNER_LIMIT_INCR before each term of the FROM
** clause is processed, so that every table in a join is guaranteed to be
** able to propose a some index+constraint combinations even if the initial
** baseline limit was exhausted by prior tables of the join.
*/
#ifndef SQLITE_QUERY_PLANNER_LIMIT
# define SQLITE_QUERY_PLANNER_LIMIT 20000
#endif
#ifndef SQLITE_QUERY_PLANNER_LIMIT_INCR
# define SQLITE_QUERY_PLANNER_LIMIT_INCR 1000
#endif

/*
** The WHERE clause processing routine has two halves.  The
** first part does the start of the WHERE loop and the second
** half does the tail of the WHERE loop.  An instance of
** this structure is returned by the first half and passed
** into the second half to give some continuity.
**
** An instance of this object holds the complete state of the query
** planner.
*/
struct WhereInfo {
  Parse *pParse;            /* Parsing and code generating context */
  SrcList *pTabList;        /* List of tables in the join */
  ExprList *pOrderBy;       /* The ORDER BY clause or NULL */
  ExprList *pResultSet;     /* Result set of the query */
  Expr *pWhere;             /* The complete WHERE clause */
  LogEst iLimit;            /* LIMIT if wctrlFlags has WHERE_USE_LIMIT */
  int aiCurOnePass[2];      /* OP_OpenWrite cursors for the ONEPASS opt */
  int iContinue;            /* Jump here to continue with next record */
  int iBreak;               /* Jump here to break out of the loop */
  int savedNQueryLoop;      /* pParse->nQueryLoop outside the WHERE loop */
  u16 wctrlFlags;           /* Flags originally passed to sqlite3WhereBegin() */
  u8 nLevel;                /* Number of nested loop */
  i8 nOBSat;                /* Number of ORDER BY terms satisfied by indices */
  u8 sorted;                /* True if really sorted (not just grouped) */
  u8 eOnePass;              /* ONEPASS_OFF, or _SINGLE, or _MULTI */
  u8 untestedTerms;         /* Not all WHERE terms resolved by outer loop */
  u8 eDistinct;             /* One of the WHERE_DISTINCT_* values */
  u8 bOrderedInnerLoop;     /* True if only the inner-most loop is ordered */
  int iTop;                 /* The very beginning of the WHERE loop */
  WhereLoop *pLoops;        /* List of all WhereLoop objects */
  Bitmask revMask;          /* Mask of ORDER BY terms that need reversing */
  LogEst nRowOut;           /* Estimated number of output rows */
  WhereClause sWC;          /* Decomposition of the WHERE clause */
  WhereMaskSet sMaskSet;    /* Map cursor numbers to bitmasks */
  WhereLevel a[1];          /* Information about each nest loop in WHERE */
};

/*
** Private interfaces - callable only by other where.c routines.
**
** where.c:
*/
SQLITE_PRIVATE Bitmask sqlite3WhereGetMask(WhereMaskSet*,int);
#ifdef WHERETRACE_ENABLED
SQLITE_PRIVATE void sqlite3WhereClausePrint(WhereClause *pWC);
#endif
SQLITE_PRIVATE WhereTerm *sqlite3WhereFindTerm(
  WhereClause *pWC,     /* The WHERE clause to be searched */
  int iCur,             /* Cursor number of LHS */
  int iColumn,          /* Column number of LHS */
  Bitmask notReady,     /* RHS must not overlap with this mask */
  u32 op,               /* Mask of WO_xx values describing operator */
  Index *pIdx           /* Must be compatible with this index, if not NULL */
);

/* wherecode.c: */
#ifndef SQLITE_OMIT_EXPLAIN
SQLITE_PRIVATE int sqlite3WhereExplainOneScan(
  Parse *pParse,                  /* Parse context */
  SrcList *pTabList,              /* Table list this loop refers to */
  WhereLevel *pLevel,             /* Scan to write OP_Explain opcode for */
  u16 wctrlFlags                  /* Flags passed to sqlite3WhereBegin() */
);
#else
# define sqlite3WhereExplainOneScan(u,v,w,x) 0
#endif /* SQLITE_OMIT_EXPLAIN */
#ifdef SQLITE_ENABLE_STMT_SCANSTATUS
SQLITE_PRIVATE void sqlite3WhereAddScanStatus(
  Vdbe *v,                        /* Vdbe to add scanstatus entry to */
  SrcList *pSrclist,              /* FROM clause pLvl reads data from */
  WhereLevel *pLvl,               /* Level to add scanstatus() entry for */
  int addrExplain                 /* Address of OP_Explain (or 0) */
);
#else
# define sqlite3WhereAddScanStatus(a, b, c, d) ((void)d)
#endif
SQLITE_PRIVATE Bitmask sqlite3WhereCodeOneLoopStart(
  Parse *pParse,       /* Parsing context */
  Vdbe *v,             /* Prepared statement under construction */
  WhereInfo *pWInfo,   /* Complete information about the WHERE clause */
  int iLevel,          /* Which level of pWInfo->a[] should be coded */
  WhereLevel *pLevel,  /* The current level pointer */
  Bitmask notReady     /* Which tables are currently available */
);

/* whereexpr.c: */
SQLITE_PRIVATE void sqlite3WhereClauseInit(WhereClause*,WhereInfo*);
SQLITE_PRIVATE void sqlite3WhereClauseClear(WhereClause*);
SQLITE_PRIVATE void sqlite3WhereSplit(WhereClause*,Expr*,u8);
SQLITE_PRIVATE Bitmask sqlite3WhereExprUsage(WhereMaskSet*, Expr*);
SQLITE_PRIVATE Bitmask sqlite3WhereExprUsageNN(WhereMaskSet*, Expr*);
SQLITE_PRIVATE Bitmask sqlite3WhereExprListUsage(WhereMaskSet*, ExprList*);
SQLITE_PRIVATE void sqlite3WhereExprAnalyze(SrcList*, WhereClause*);
SQLITE_PRIVATE void sqlite3WhereTabFuncArgs(Parse*, struct SrcList_item*, WhereClause*);





/*
** Bitmasks for the operators on WhereTerm objects.  These are all
** operators that are of interest to the query planner.  An
** OR-ed combination of these values can be used when searching for
** particular WhereTerms within a WhereClause.
**
** Value constraints:
**     WO_EQ    == SQLITE_INDEX_CONSTRAINT_EQ
**     WO_LT    == SQLITE_INDEX_CONSTRAINT_LT
**     WO_LE    == SQLITE_INDEX_CONSTRAINT_LE
**     WO_GT    == SQLITE_INDEX_CONSTRAINT_GT
**     WO_GE    == SQLITE_INDEX_CONSTRAINT_GE
*/
#define WO_IN     0x0001
#define WO_EQ     0x0002
#define WO_LT     (WO_EQ<<(TK_LT-TK_EQ))
#define WO_LE     (WO_EQ<<(TK_LE-TK_EQ))
#define WO_GT     (WO_EQ<<(TK_GT-TK_EQ))
#define WO_GE     (WO_EQ<<(TK_GE-TK_EQ))
#define WO_AUX    0x0040       /* Op useful to virtual tables only */
#define WO_IS     0x0080
#define WO_ISNULL 0x0100
#define WO_OR     0x0200       /* Two or more OR-connected terms */
#define WO_AND    0x0400       /* Two or more AND-connected terms */
#define WO_EQUIV  0x0800       /* Of the form A==B, both columns */
#define WO_NOOP   0x1000       /* This term does not restrict search space */

#define WO_ALL    0x1fff       /* Mask of all possible WO_* values */
#define WO_SINGLE 0x01ff       /* Mask of all non-compound WO_* values */

/*
** These are definitions of bits in the WhereLoop.wsFlags field.
** The particular combination of bits in each WhereLoop help to
** determine the algorithm that WhereLoop represents.
*/
#define WHERE_COLUMN_EQ    0x00000001  /* x=EXPR */
#define WHERE_COLUMN_RANGE 0x00000002  /* x<EXPR and/or x>EXPR */
#define WHERE_COLUMN_IN    0x00000004  /* x IN (...) */
#define WHERE_COLUMN_NULL  0x00000008  /* x IS NULL */
#define WHERE_CONSTRAINT   0x0000000f  /* Any of the WHERE_COLUMN_xxx values */
#define WHERE_TOP_LIMIT    0x00000010  /* x<EXPR or x<=EXPR constraint */
#define WHERE_BTM_LIMIT    0x00000020  /* x>EXPR or x>=EXPR constraint */
#define WHERE_BOTH_LIMIT   0x00000030  /* Both x>EXPR and x<EXPR */
#define WHERE_IDX_ONLY     0x00000040  /* Use index only - omit table */
#define WHERE_IPK          0x00000100  /* x is the INTEGER PRIMARY KEY */
#define WHERE_INDEXED      0x00000200  /* WhereLoop.u.btree.pIndex is valid */
#define WHERE_VIRTUALTABLE 0x00000400  /* WhereLoop.u.vtab is valid */
#define WHERE_IN_ABLE      0x00000800  /* Able to support an IN operator */
#define WHERE_ONEROW       0x00001000  /* Selects no more than one row */
#define WHERE_MULTI_OR     0x00002000  /* OR using multiple indices */
#define WHERE_AUTO_INDEX   0x00004000  /* Uses an ephemeral index */
#define WHERE_SKIPSCAN     0x00008000  /* Uses the skip-scan algorithm */
#define WHERE_UNQ_WANTED   0x00010000  /* WHERE_ONEROW would have been helpful*/
#define WHERE_PARTIALIDX   0x00020000  /* The automatic index is partial */
#define WHERE_IN_EARLYOUT  0x00040000  /* Perhaps quit IN loops early */
#define WHERE_BIGNULL_SORT 0x00080000  /* Column nEq of index is BIGNULL */

#endif /* !defined(SQLITE_WHEREINT_H) */

/************** End of whereInt.h ********************************************/
/************** Continuing where we left off in wherecode.c ******************/

#ifndef SQLITE_OMIT_EXPLAIN

/*
** Return the name of the i-th column of the pIdx index.
*/
static const char *explainIndexColumnName(Index *pIdx, int i){
  i = pIdx->aiColumn[i];
  if( i==XN_EXPR ) return "<expr>";
  if( i==XN_ROWID ) return "rowid";
  return pIdx->pTable->aCol[i].zName;
}

/*
** This routine is a helper for explainIndexRange() below
**
** pStr holds the text of an expression that we are building up one term
** at a time.  This routine adds a new term to the end of the expression.
** Terms are separated by AND so add the "AND" text for second and subsequent
** terms only.
*/
static void explainAppendTerm(
  StrAccum *pStr,             /* The text expression being built */
  Index *pIdx,                /* Index to read column names from */
  int nTerm,                  /* Number of terms */
  int iTerm,                  /* Zero-based index of first term. */
  int bAnd,                   /* Non-zero to append " AND " */
  const char *zOp             /* Name of the operator */
){
  int i;

  assert( nTerm>=1 );
  if( bAnd ) sqlite3_str_append(pStr, " AND ", 5);

  if( nTerm>1 ) sqlite3_str_append(pStr, "(", 1);
  for(i=0; i<nTerm; i++){
    if( i ) sqlite3_str_append(pStr, ",", 1);
    sqlite3_str_appendall(pStr, explainIndexColumnName(pIdx, iTerm+i));
  }
  if( nTerm>1 ) sqlite3_str_append(pStr, ")", 1);

  sqlite3_str_append(pStr, zOp, 1);

  if( nTerm>1 ) sqlite3_str_append(pStr, "(", 1);
  for(i=0; i<nTerm; i++){
    if( i ) sqlite3_str_append(pStr, ",", 1);
    sqlite3_str_append(pStr, "?", 1);
  }
  if( nTerm>1 ) sqlite3_str_append(pStr, ")", 1);
}

/*
** Argument pLevel describes a strategy for scanning table pTab. This 
** function appends text to pStr that describes the subset of table
** rows scanned by the strategy in the form of an SQL expression.
**
** For example, if the query:
**
**   SELECT * FROM t1 WHERE a=1 AND b>2;
**
** is run and there is an index on (a, b), then this function returns a
** string similar to:
**
**   "a=? AND b>?"
*/
static void explainIndexRange(StrAccum *pStr, WhereLoop *pLoop){
  Index *pIndex = pLoop->u.btree.pIndex;
  u16 nEq = pLoop->u.btree.nEq;
  u16 nSkip = pLoop->nSkip;
  int i, j;

  if( nEq==0 && (pLoop->wsFlags&(WHERE_BTM_LIMIT|WHERE_TOP_LIMIT))==0 ) return;
  sqlite3_str_append(pStr, " (", 2);
  for(i=0; i<nEq; i++){
    const char *z = explainIndexColumnName(pIndex, i);
    if( i ) sqlite3_str_append(pStr, " AND ", 5);
    sqlite3_str_appendf(pStr, i>=nSkip ? "%s=?" : "ANY(%s)", z);
  }

  j = i;
  if( pLoop->wsFlags&WHERE_BTM_LIMIT ){
    explainAppendTerm(pStr, pIndex, pLoop->u.btree.nBtm, j, i, ">");
    i = 1;
  }
  if( pLoop->wsFlags&WHERE_TOP_LIMIT ){
    explainAppendTerm(pStr, pIndex, pLoop->u.btree.nTop, j, i, "<");
  }
  sqlite3_str_append(pStr, ")", 1);
}

/*
** This function is a no-op unless currently processing an EXPLAIN QUERY PLAN
** command, or if either SQLITE_DEBUG or SQLITE_ENABLE_STMT_SCANSTATUS was
** defined at compile-time. If it is not a no-op, a single OP_Explain opcode 
** is added to the output to describe the table scan strategy in pLevel.
**
** If an OP_Explain opcode is added to the VM, its address is returned.
** Otherwise, if no OP_Explain is coded, zero is returned.
*/
SQLITE_PRIVATE int sqlite3WhereExplainOneScan(
  Parse *pParse,                  /* Parse context */
  SrcList *pTabList,              /* Table list this loop refers to */
  WhereLevel *pLevel,             /* Scan to write OP_Explain opcode for */
  u16 wctrlFlags                  /* Flags passed to sqlite3WhereBegin() */
){
  int ret = 0;
#if !defined(SQLITE_DEBUG) && !defined(SQLITE_ENABLE_STMT_SCANSTATUS)
  if( sqlite3ParseToplevel(pParse)->explain==2 )
#endif
  {
    struct SrcList_item *pItem = &pTabList->a[pLevel->iFrom];
    Vdbe *v = pParse->pVdbe;      /* VM being constructed */
    sqlite3 *db = pParse->db;     /* Database handle */
    int isSearch;                 /* True for a SEARCH. False for SCAN. */
    WhereLoop *pLoop;             /* The controlling WhereLoop object */
    u32 flags;                    /* Flags that describe this loop */
    char *zMsg;                   /* Text to add to EQP output */
    StrAccum str;                 /* EQP output string */
    char zBuf[100];               /* Initial space for EQP output string */

    pLoop = pLevel->pWLoop;
    flags = pLoop->wsFlags;
    if( (flags&WHERE_MULTI_OR) || (wctrlFlags&WHERE_OR_SUBCLAUSE) ) return 0;

    isSearch = (flags&(WHERE_BTM_LIMIT|WHERE_TOP_LIMIT))!=0
            || ((flags&WHERE_VIRTUALTABLE)==0 && (pLoop->u.btree.nEq>0))
            || (wctrlFlags&(WHERE_ORDERBY_MIN|WHERE_ORDERBY_MAX));

    sqlite3StrAccumInit(&str, db, zBuf, sizeof(zBuf), SQLITE_MAX_LENGTH);
    sqlite3_str_appendall(&str, isSearch ? "SEARCH" : "SCAN");
    if( pItem->pSelect ){
      sqlite3_str_appendf(&str, " SUBQUERY %u", pItem->pSelect->selId);
    }else{
      sqlite3_str_appendf(&str, " TABLE %s", pItem->zName);
    }

    if( pItem->zAlias ){
      sqlite3_str_appendf(&str, " AS %s", pItem->zAlias);
    }
    if( (flags & (WHERE_IPK|WHERE_VIRTUALTABLE))==0 ){
      const char *zFmt = 0;
      Index *pIdx;

      assert( pLoop->u.btree.pIndex!=0 );
      pIdx = pLoop->u.btree.pIndex;
      assert( !(flags&WHERE_AUTO_INDEX) || (flags&WHERE_IDX_ONLY) );
      if( !HasRowid(pItem->pTab) && IsPrimaryKeyIndex(pIdx) ){
        if( isSearch ){
          zFmt = "PRIMARY KEY";
        }
      }else if( flags & WHERE_PARTIALIDX ){
        zFmt = "AUTOMATIC PARTIAL COVERING INDEX";
      }else if( flags & WHERE_AUTO_INDEX ){
        zFmt = "AUTOMATIC COVERING INDEX";
      }else if( flags & WHERE_IDX_ONLY ){
        zFmt = "COVERING INDEX %s";
      }else{
        zFmt = "INDEX %s";
      }
      if( zFmt ){
        sqlite3_str_append(&str, " USING ", 7);
        sqlite3_str_appendf(&str, zFmt, pIdx->zName);
        explainIndexRange(&str, pLoop);
      }
    }else if( (flags & WHERE_IPK)!=0 && (flags & WHERE_CONSTRAINT)!=0 ){
      const char *zRangeOp;
      if( flags&(WHERE_COLUMN_EQ|WHERE_COLUMN_IN) ){
        zRangeOp = "=";
      }else if( (flags&WHERE_BOTH_LIMIT)==WHERE_BOTH_LIMIT ){
        zRangeOp = ">? AND rowid<";
      }else if( flags&WHERE_BTM_LIMIT ){
        zRangeOp = ">";
      }else{
        assert( flags&WHERE_TOP_LIMIT);
        zRangeOp = "<";
      }
      sqlite3_str_appendf(&str, 
          " USING INTEGER PRIMARY KEY (rowid%s?)",zRangeOp);
    }
#ifndef SQLITE_OMIT_VIRTUALTABLE
    else if( (flags & WHERE_VIRTUALTABLE)!=0 ){
      sqlite3_str_appendf(&str, " VIRTUAL TABLE INDEX %d:%s",
                  pLoop->u.vtab.idxNum, pLoop->u.vtab.idxStr);
    }
#endif
#ifdef SQLITE_EXPLAIN_ESTIMATED_ROWS
    if( pLoop->nOut>=10 ){
      sqlite3_str_appendf(&str, " (~%llu rows)",
             sqlite3LogEstToInt(pLoop->nOut));
    }else{
      sqlite3_str_append(&str, " (~1 row)", 9);
    }
#endif
    zMsg = sqlite3StrAccumFinish(&str);
    sqlite3ExplainBreakpoint("",zMsg);
    ret = sqlite3VdbeAddOp4(v, OP_Explain, sqlite3VdbeCurrentAddr(v),
                            pParse->addrExplain, 0, zMsg,P4_DYNAMIC);
  }
  return ret;
}
#endif /* SQLITE_OMIT_EXPLAIN */

#ifdef SQLITE_ENABLE_STMT_SCANSTATUS
/*
** Configure the VM passed as the first argument with an
** sqlite3_stmt_scanstatus() entry corresponding to the scan used to 
** implement level pLvl. Argument pSrclist is a pointer to the FROM 
** clause that the scan reads data from.
**
** If argument addrExplain is not 0, it must be the address of an 
** OP_Explain instruction that describes the same loop.
*/
SQLITE_PRIVATE void sqlite3WhereAddScanStatus(
  Vdbe *v,                        /* Vdbe to add scanstatus entry to */
  SrcList *pSrclist,              /* FROM clause pLvl reads data from */
  WhereLevel *pLvl,               /* Level to add scanstatus() entry for */
  int addrExplain                 /* Address of OP_Explain (or 0) */
){
  const char *zObj = 0;
  WhereLoop *pLoop = pLvl->pWLoop;
  if( (pLoop->wsFlags & WHERE_VIRTUALTABLE)==0  &&  pLoop->u.btree.pIndex!=0 ){
    zObj = pLoop->u.btree.pIndex->zName;
  }else{
    zObj = pSrclist->a[pLvl->iFrom].zName;
  }
  sqlite3VdbeScanStatus(
      v, addrExplain, pLvl->addrBody, pLvl->addrVisit, pLoop->nOut, zObj
  );
}
#endif


/*
** Disable a term in the WHERE clause.  Except, do not disable the term
** if it controls a LEFT OUTER JOIN and it did not originate in the ON
** or USING clause of that join.
**
** Consider the term t2.z='ok' in the following queries:
**
**   (1)  SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.x WHERE t2.z='ok'
**   (2)  SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.x AND t2.z='ok'
**   (3)  SELECT * FROM t1, t2 WHERE t1.a=t2.x AND t2.z='ok'
**
** The t2.z='ok' is disabled in the in (2) because it originates
** in the ON clause.  The term is disabled in (3) because it is not part
** of a LEFT OUTER JOIN.  In (1), the term is not disabled.
**
** Disabling a term causes that term to not be tested in the inner loop
** of the join.  Disabling is an optimization.  When terms are satisfied
** by indices, we disable them to prevent redundant tests in the inner
** loop.  We would get the correct results if nothing were ever disabled,
** but joins might run a little slower.  The trick is to disable as much
** as we can without disabling too much.  If we disabled in (1), we'd get
** the wrong answer.  See ticket #813.
**
** If all the children of a term are disabled, then that term is also
** automatically disabled.  In this way, terms get disabled if derived
** virtual terms are tested first.  For example:
**
**      x GLOB 'abc*' AND x>='abc' AND x<'acd'
**      \___________/     \______/     \_____/
**         parent          child1       child2
**
** Only the parent term was in the original WHERE clause.  The child1
** and child2 terms were added by the LIKE optimization.  If both of
** the virtual child terms are valid, then testing of the parent can be 
** skipped.
**
** Usually the parent term is marked as TERM_CODED.  But if the parent
** term was originally TERM_LIKE, then the parent gets TERM_LIKECOND instead.
** The TERM_LIKECOND marking indicates that the term should be coded inside
** a conditional such that is only evaluated on the second pass of a
** LIKE-optimization loop, when scanning BLOBs instead of strings.
*/
static void disableTerm(WhereLevel *pLevel, WhereTerm *pTerm){
  int nLoop = 0;
  assert( pTerm!=0 );
  while( (pTerm->wtFlags & TERM_CODED)==0
      && (pLevel->iLeftJoin==0 || ExprHasProperty(pTerm->pExpr, EP_FromJoin))
      && (pLevel->notReady & pTerm->prereqAll)==0
  ){
    if( nLoop && (pTerm->wtFlags & TERM_LIKE)!=0 ){
      pTerm->wtFlags |= TERM_LIKECOND;
    }else{
      pTerm->wtFlags |= TERM_CODED;
    }
    if( pTerm->iParent<0 ) break;
    pTerm = &pTerm->pWC->a[pTerm->iParent];
    assert( pTerm!=0 );
    pTerm->nChild--;
    if( pTerm->nChild!=0 ) break;
    nLoop++;
  }
}

/*
** Code an OP_Affinity opcode to apply the column affinity string zAff
** to the n registers starting at base. 
**
** As an optimization, SQLITE_AFF_BLOB and SQLITE_AFF_NONE entries (which
** are no-ops) at the beginning and end of zAff are ignored.  If all entries
** in zAff are SQLITE_AFF_BLOB or SQLITE_AFF_NONE, then no code gets generated.
**
** This routine makes its own copy of zAff so that the caller is free
** to modify zAff after this routine returns.
*/
static void codeApplyAffinity(Parse *pParse, int base, int n, char *zAff){
  Vdbe *v = pParse->pVdbe;
  if( zAff==0 ){
    assert( pParse->db->mallocFailed );
    return;
  }
  assert( v!=0 );

  /* Adjust base and n to skip over SQLITE_AFF_BLOB and SQLITE_AFF_NONE
  ** entries at the beginning and end of the affinity string.
  */
  assert( SQLITE_AFF_NONE<SQLITE_AFF_BLOB );
  while( n>0 && zAff[0]<=SQLITE_AFF_BLOB ){
    n--;
    base++;
    zAff++;
  }
  while( n>1 && zAff[n-1]<=SQLITE_AFF_BLOB ){
    n--;
  }

  /* Code the OP_Affinity opcode if there is anything left to do. */
  if( n>0 ){
    sqlite3VdbeAddOp4(v, OP_Affinity, base, n, 0, zAff, n);
  }
}

/*
** Expression pRight, which is the RHS of a comparison operation, is 
** either a vector of n elements or, if n==1, a scalar expression.
** Before the comparison operation, affinity zAff is to be applied
** to the pRight values. This function modifies characters within the
** affinity string to SQLITE_AFF_BLOB if either:
**
**   * the comparison will be performed with no affinity, or
**   * the affinity change in zAff is guaranteed not to change the value.
*/
static void updateRangeAffinityStr(
  Expr *pRight,                   /* RHS of comparison */
  int n,                          /* Number of vector elements in comparison */
  char *zAff                      /* Affinity string to modify */
){
  int i;
  for(i=0; i<n; i++){
    Expr *p = sqlite3VectorFieldSubexpr(pRight, i);
    if( sqlite3CompareAffinity(p, zAff[i])==SQLITE_AFF_BLOB
     || sqlite3ExprNeedsNoAffinityChange(p, zAff[i])
    ){
      zAff[i] = SQLITE_AFF_BLOB;
    }
  }
}


/*
** pX is an expression of the form:  (vector) IN (SELECT ...)
** In other words, it is a vector IN operator with a SELECT clause on the
** LHS.  But not all terms in the vector are indexable and the terms might
** not be in the correct order for indexing.
**
** This routine makes a copy of the input pX expression and then adjusts
** the vector on the LHS with corresponding changes to the SELECT so that
** the vector contains only index terms and those terms are in the correct
** order.  The modified IN expression is returned.  The caller is responsible
** for deleting the returned expression.
**
** Example:
**
**    CREATE TABLE t1(a,b,c,d,e,f);
**    CREATE INDEX t1x1 ON t1(e,c);
**    SELECT * FROM t1 WHERE (a,b,c,d,e) IN (SELECT v,w,x,y,z FROM t2)
**                           \_______________________________________/
**                                     The pX expression
**
** Since only columns e and c can be used with the index, in that order,
** the modified IN expression that is returned will be:
**
**        (e,c) IN (SELECT z,x FROM t2)
**
** The reduced pX is different from the original (obviously) and thus is
** only used for indexing, to improve performance.  The original unaltered
** IN expression must also be run on each output row for correctness.
*/
static Expr *removeUnindexableInClauseTerms(
  Parse *pParse,        /* The parsing context */
  int iEq,              /* Look at loop terms starting here */
  WhereLoop *pLoop,     /* The current loop */
  Expr *pX              /* The IN expression to be reduced */
){
  sqlite3 *db = pParse->db;
  Expr *pNew = sqlite3ExprDup(db, pX, 0);
  if( db->mallocFailed==0 ){
    ExprList *pOrigRhs = pNew->x.pSelect->pEList;  /* Original unmodified RHS */
    ExprList *pOrigLhs = pNew->pLeft->x.pList;     /* Original unmodified LHS */
    ExprList *pRhs = 0;         /* New RHS after modifications */
    ExprList *pLhs = 0;         /* New LHS after mods */
    int i;                      /* Loop counter */
    Select *pSelect;            /* Pointer to the SELECT on the RHS */

    for(i=iEq; i<pLoop->nLTerm; i++){
      if( pLoop->aLTerm[i]->pExpr==pX ){
        int iField = pLoop->aLTerm[i]->iField - 1;
        if( pOrigRhs->a[iField].pExpr==0 ) continue; /* Duplicate PK column */
        pRhs = sqlite3ExprListAppend(pParse, pRhs, pOrigRhs->a[iField].pExpr);
        pOrigRhs->a[iField].pExpr = 0;
        assert( pOrigLhs->a[iField].pExpr!=0 );
        pLhs = sqlite3ExprListAppend(pParse, pLhs, pOrigLhs->a[iField].pExpr);
        pOrigLhs->a[iField].pExpr = 0;
      }
    }
    sqlite3ExprListDelete(db, pOrigRhs);
    sqlite3ExprListDelete(db, pOrigLhs);
    pNew->pLeft->x.pList = pLhs;
    pNew->x.pSelect->pEList = pRhs;
    if( pLhs && pLhs->nExpr==1 ){
      /* Take care here not to generate a TK_VECTOR containing only a
      ** single value. Since the parser never creates such a vector, some
      ** of the subroutines do not handle this case.  */
      Expr *p = pLhs->a[0].pExpr;
      pLhs->a[0].pExpr = 0;
      sqlite3ExprDelete(db, pNew->pLeft);
      pNew->pLeft = p;
    }
    pSelect = pNew->x.pSelect;
    if( pSelect->pOrderBy ){
      /* If the SELECT statement has an ORDER BY clause, zero the 
      ** iOrderByCol variables. These are set to non-zero when an 
      ** ORDER BY term exactly matches one of the terms of the 
      ** result-set. Since the result-set of the SELECT statement may
      ** have been modified or reordered, these variables are no longer 
      ** set correctly.  Since setting them is just an optimization, 
      ** it's easiest just to zero them here.  */
      ExprList *pOrderBy = pSelect->pOrderBy;
      for(i=0; i<pOrderBy->nExpr; i++){
        pOrderBy->a[i].u.x.iOrderByCol = 0;
      }
    }

#if 0
    printf("For indexing, change the IN expr:\n");
    sqlite3TreeViewExpr(0, pX, 0);
    printf("Into:\n");
    sqlite3TreeViewExpr(0, pNew, 0);
#endif
  }
  return pNew;
}


/*
** Generate code for a single equality term of the WHERE clause.  An equality
** term can be either X=expr or X IN (...).   pTerm is the term to be 
** coded.
**
** The current value for the constraint is left in a register, the index
** of which is returned.  An attempt is made store the result in iTarget but
** this is only guaranteed for TK_ISNULL and TK_IN constraints.  If the
** constraint is a TK_EQ or TK_IS, then the current value might be left in
** some other register and it is the caller's responsibility to compensate.
**
** For a constraint of the form X=expr, the expression is evaluated in
** straight-line code.  For constraints of the form X IN (...)
** this routine sets up a loop that will iterate over all values of X.
*/
static int codeEqualityTerm(
  Parse *pParse,      /* The parsing context */
  WhereTerm *pTerm,   /* The term of the WHERE clause to be coded */
  WhereLevel *pLevel, /* The level of the FROM clause we are working on */
  int iEq,            /* Index of the equality term within this level */
  int bRev,           /* True for reverse-order IN operations */
  int iTarget         /* Attempt to leave results in this register */
){
  Expr *pX = pTerm->pExpr;
  Vdbe *v = pParse->pVdbe;
  int iReg;                  /* Register holding results */

  assert( pLevel->pWLoop->aLTerm[iEq]==pTerm );
  assert( iTarget>0 );
  if( pX->op==TK_EQ || pX->op==TK_IS ){
    iReg = sqlite3ExprCodeTarget(pParse, pX->pRight, iTarget);
  }else if( pX->op==TK_ISNULL ){
    iReg = iTarget;
    sqlite3VdbeAddOp2(v, OP_Null, 0, iReg);
#ifndef SQLITE_OMIT_SUBQUERY
  }else{
    int eType = IN_INDEX_NOOP;
    int iTab;
    struct InLoop *pIn;
    WhereLoop *pLoop = pLevel->pWLoop;
    int i;
    int nEq = 0;
    int *aiMap = 0;

    if( (pLoop->wsFlags & WHERE_VIRTUALTABLE)==0
      && pLoop->u.btree.pIndex!=0
      && pLoop->u.btree.pIndex->aSortOrder[iEq]
    ){
      testcase( iEq==0 );
      testcase( bRev );
      bRev = !bRev;
    }
    assert( pX->op==TK_IN );
    iReg = iTarget;

    for(i=0; i<iEq; i++){
      if( pLoop->aLTerm[i] && pLoop->aLTerm[i]->pExpr==pX ){
        disableTerm(pLevel, pTerm);
        return iTarget;
      }
    }
    for(i=iEq;i<pLoop->nLTerm; i++){
      assert( pLoop->aLTerm[i]!=0 );
      if( pLoop->aLTerm[i]->pExpr==pX ) nEq++;
    }

    iTab = 0;
    if( (pX->flags & EP_xIsSelect)==0 || pX->x.pSelect->pEList->nExpr==1 ){
      eType = sqlite3FindInIndex(pParse, pX, IN_INDEX_LOOP, 0, 0, &iTab);
    }else{
      sqlite3 *db = pParse->db;
      pX = removeUnindexableInClauseTerms(pParse, iEq, pLoop, pX);

      if( !db->mallocFailed ){
        aiMap = (int*)sqlite3DbMallocZero(pParse->db, sizeof(int)*nEq);
        eType = sqlite3FindInIndex(pParse, pX, IN_INDEX_LOOP, 0, aiMap, &iTab);
        pTerm->pExpr->iTable = iTab;
      }
      sqlite3ExprDelete(db, pX);
      pX = pTerm->pExpr;
    }

    if( eType==IN_INDEX_INDEX_DESC ){
      testcase( bRev );
      bRev = !bRev;
    }
    sqlite3VdbeAddOp2(v, bRev ? OP_Last : OP_Rewind, iTab, 0);
    VdbeCoverageIf(v, bRev);
    VdbeCoverageIf(v, !bRev);
    assert( (pLoop->wsFlags & WHERE_MULTI_OR)==0 );

    pLoop->wsFlags |= WHERE_IN_ABLE;
    if( pLevel->u.in.nIn==0 ){
      pLevel->addrNxt = sqlite3VdbeMakeLabel(pParse);
    }

    i = pLevel->u.in.nIn;
    pLevel->u.in.nIn += nEq;
    pLevel->u.in.aInLoop =
       sqlite3DbReallocOrFree(pParse->db, pLevel->u.in.aInLoop,
                              sizeof(pLevel->u.in.aInLoop[0])*pLevel->u.in.nIn);
    pIn = pLevel->u.in.aInLoop;
    if( pIn ){
      int iMap = 0;               /* Index in aiMap[] */
      pIn += i;
      for(i=iEq;i<pLoop->nLTerm; i++){
        if( pLoop->aLTerm[i]->pExpr==pX ){
          int iOut = iReg + i - iEq;
          if( eType==IN_INDEX_ROWID ){
            pIn->addrInTop = sqlite3VdbeAddOp2(v, OP_Rowid, iTab, iOut);
          }else{
            int iCol = aiMap ? aiMap[iMap++] : 0;
            pIn->addrInTop = sqlite3VdbeAddOp3(v,OP_Column,iTab, iCol, iOut);
          }
          sqlite3VdbeAddOp1(v, OP_IsNull, iOut); VdbeCoverage(v);
          if( i==iEq ){
            pIn->iCur = iTab;
            pIn->eEndLoopOp = bRev ? OP_Prev : OP_Next;
            if( iEq>0 && (pLoop->wsFlags & WHERE_VIRTUALTABLE)==0 ){
              pIn->iBase = iReg - i;
              pIn->nPrefix = i;
              pLoop->wsFlags |= WHERE_IN_EARLYOUT;
            }else{
              pIn->nPrefix = 0;
            }
          }else{
            pIn->eEndLoopOp = OP_Noop;
          }
          pIn++;
        }
      }
    }else{
      pLevel->u.in.nIn = 0;
    }
    sqlite3DbFree(pParse->db, aiMap);
#endif
  }
  disableTerm(pLevel, pTerm);
  return iReg;
}

/*
** Generate code that will evaluate all == and IN constraints for an
** index scan.
**
** For example, consider table t1(a,b,c,d,e,f) with index i1(a,b,c).
** Suppose the WHERE clause is this:  a==5 AND b IN (1,2,3) AND c>5 AND c<10
** The index has as many as three equality constraints, but in this
** example, the third "c" value is an inequality.  So only two 
** constraints are coded.  This routine will generate code to evaluate
** a==5 and b IN (1,2,3).  The current values for a and b will be stored
** in consecutive registers and the index of the first register is returned.
**
** In the example above nEq==2.  But this subroutine works for any value
** of nEq including 0.  If nEq==0, this routine is nearly a no-op.
** The only thing it does is allocate the pLevel->iMem memory cell and
** compute the affinity string.
**
** The nExtraReg parameter is 0 or 1.  It is 0 if all WHERE clause constraints
** are == or IN and are covered by the nEq.  nExtraReg is 1 if there is
** an inequality constraint (such as the "c>=5 AND c<10" in the example) that
** occurs after the nEq quality constraints.
**
** This routine allocates a range of nEq+nExtraReg memory cells and returns
** the index of the first memory cell in that range. The code that
** calls this routine will use that memory range to store keys for
** start and termination conditions of the loop.
** key value of the loop.  If one or more IN operators appear, then
** this routine allocates an additional nEq memory cells for internal
** use.
**
** Before returning, *pzAff is set to point to a buffer containing a
** copy of the column affinity string of the index allocated using
** sqlite3DbMalloc(). Except, entries in the copy of the string associated
** with equality constraints that use BLOB or NONE affinity are set to
** SQLITE_AFF_BLOB. This is to deal with SQL such as the following:
**
**   CREATE TABLE t1(a TEXT PRIMARY KEY, b);
**   SELECT ... FROM t1 AS t2, t1 WHERE t1.a = t2.b;
**
** In the example above, the index on t1(a) has TEXT affinity. But since
** the right hand side of the equality constraint (t2.b) has BLOB/NONE affinity,
** no conversion should be attempted before using a t2.b value as part of
** a key to search the index. Hence the first byte in the returned affinity
** string in this example would be set to SQLITE_AFF_BLOB.
*/
static int codeAllEqualityTerms(
  Parse *pParse,        /* Parsing context */
  WhereLevel *pLevel,   /* Which nested loop of the FROM we are coding */
  int bRev,             /* Reverse the order of IN operators */
  int nExtraReg,        /* Number of extra registers to allocate */
  char **pzAff          /* OUT: Set to point to affinity string */
){
  u16 nEq;                      /* The number of == or IN constraints to code */
  u16 nSkip;                    /* Number of left-most columns to skip */
  Vdbe *v = pParse->pVdbe;      /* The vm under construction */
  Index *pIdx;                  /* The index being used for this loop */
  WhereTerm *pTerm;             /* A single constraint term */
  WhereLoop *pLoop;             /* The WhereLoop object */
  int j;                        /* Loop counter */
  int regBase;                  /* Base register */
  int nReg;                     /* Number of registers to allocate */
  char *zAff;                   /* Affinity string to return */

  /* This module is only called on query plans that use an index. */
  pLoop = pLevel->pWLoop;
  assert( (pLoop->wsFlags & WHERE_VIRTUALTABLE)==0 );
  nEq = pLoop->u.btree.nEq;
  nSkip = pLoop->nSkip;
  pIdx = pLoop->u.btree.pIndex;
  assert( pIdx!=0 );

  /* Figure out how many memory cells we will need then allocate them.
  */
  regBase = pParse->nMem + 1;
  nReg = pLoop->u.btree.nEq + nExtraReg;
  pParse->nMem += nReg;

  zAff = sqlite3DbStrDup(pParse->db,sqlite3IndexAffinityStr(pParse->db,pIdx));
  assert( zAff!=0 || pParse->db->mallocFailed );

  if( nSkip ){
    int iIdxCur = pLevel->iIdxCur;
    sqlite3VdbeAddOp1(v, (bRev?OP_Last:OP_Rewind), iIdxCur);
    VdbeCoverageIf(v, bRev==0);
    VdbeCoverageIf(v, bRev!=0);
    VdbeComment((v, "begin skip-scan on %s", pIdx->zName));
    j = sqlite3VdbeAddOp0(v, OP_Goto);
    pLevel->addrSkip = sqlite3VdbeAddOp4Int(v, (bRev?OP_SeekLT:OP_SeekGT),
                            iIdxCur, 0, regBase, nSkip);
    VdbeCoverageIf(v, bRev==0);
    VdbeCoverageIf(v, bRev!=0);
    sqlite3VdbeJumpHere(v, j);
    for(j=0; j<nSkip; j++){
      sqlite3VdbeAddOp3(v, OP_Column, iIdxCur, j, regBase+j);
      testcase( pIdx->aiColumn[j]==XN_EXPR );
      VdbeComment((v, "%s", explainIndexColumnName(pIdx, j)));
    }
  }    

  /* Evaluate the equality constraints
  */
  assert( zAff==0 || (int)strlen(zAff)>=nEq );
  for(j=nSkip; j<nEq; j++){
    int r1;
    pTerm = pLoop->aLTerm[j];
    assert( pTerm!=0 );
    /* The following testcase is true for indices with redundant columns. 
    ** Ex: CREATE INDEX i1 ON t1(a,b,a); SELECT * FROM t1 WHERE a=0 AND b=0; */
    testcase( (pTerm->wtFlags & TERM_CODED)!=0 );
    testcase( pTerm->wtFlags & TERM_VIRTUAL );
    r1 = codeEqualityTerm(pParse, pTerm, pLevel, j, bRev, regBase+j);
    if( r1!=regBase+j ){
      if( nReg==1 ){
        sqlite3ReleaseTempReg(pParse, regBase);
        regBase = r1;
      }else{
        sqlite3VdbeAddOp2(v, OP_SCopy, r1, regBase+j);
      }
    }
    if( pTerm->eOperator & WO_IN ){
      if( pTerm->pExpr->flags & EP_xIsSelect ){
        /* No affinity ever needs to be (or should be) applied to a value
        ** from the RHS of an "? IN (SELECT ...)" expression. The 
        ** sqlite3FindInIndex() routine has already ensured that the 
        ** affinity of the comparison has been applied to the value.  */
        if( zAff ) zAff[j] = SQLITE_AFF_BLOB;
      }
    }else if( (pTerm->eOperator & WO_ISNULL)==0 ){
      Expr *pRight = pTerm->pExpr->pRight;
      if( (pTerm->wtFlags & TERM_IS)==0 && sqlite3ExprCanBeNull(pRight) ){
        sqlite3VdbeAddOp2(v, OP_IsNull, regBase+j, pLevel->addrBrk);
        VdbeCoverage(v);
      }
      if( zAff ){
        if( sqlite3CompareAffinity(pRight, zAff[j])==SQLITE_AFF_BLOB ){
          zAff[j] = SQLITE_AFF_BLOB;
        }
        if( sqlite3ExprNeedsNoAffinityChange(pRight, zAff[j]) ){
          zAff[j] = SQLITE_AFF_BLOB;
        }
      }
    }
  }
  *pzAff = zAff;
  return regBase;
}

#ifndef SQLITE_LIKE_DOESNT_MATCH_BLOBS
/*
** If the most recently coded instruction is a constant range constraint
** (a string literal) that originated from the LIKE optimization, then 
** set P3 and P5 on the OP_String opcode so that the string will be cast
** to a BLOB at appropriate times.
**
** The LIKE optimization trys to evaluate "x LIKE 'abc%'" as a range
** expression: "x>='ABC' AND x<'abd'".  But this requires that the range
** scan loop run twice, once for strings and a second time for BLOBs.
** The OP_String opcodes on the second pass convert the upper and lower
** bound string constants to blobs.  This routine makes the necessary changes
** to the OP_String opcodes for that to happen.
**
** Except, of course, if SQLITE_LIKE_DOESNT_MATCH_BLOBS is defined, then
** only the one pass through the string space is required, so this routine
** becomes a no-op.
*/
static void whereLikeOptimizationStringFixup(
  Vdbe *v,                /* prepared statement under construction */
  WhereLevel *pLevel,     /* The loop that contains the LIKE operator */
  WhereTerm *pTerm        /* The upper or lower bound just coded */
){
  if( pTerm->wtFlags & TERM_LIKEOPT ){
    VdbeOp *pOp;
    assert( pLevel->iLikeRepCntr>0 );
    pOp = sqlite3VdbeGetOp(v, -1);
    assert( pOp!=0 );
    assert( pOp->opcode==OP_String8 
            || pTerm->pWC->pWInfo->pParse->db->mallocFailed );
    pOp->p3 = (int)(pLevel->iLikeRepCntr>>1);  /* Register holding counter */
    pOp->p5 = (u8)(pLevel->iLikeRepCntr&1);    /* ASC or DESC */
  }
}
#else
# define whereLikeOptimizationStringFixup(A,B,C)
#endif

#ifdef SQLITE_ENABLE_CURSOR_HINTS
/*
** Information is passed from codeCursorHint() down to individual nodes of
** the expression tree (by sqlite3WalkExpr()) using an instance of this
** structure.
*/
struct CCurHint {
  int iTabCur;    /* Cursor for the main table */
  int iIdxCur;    /* Cursor for the index, if pIdx!=0.  Unused otherwise */
  Index *pIdx;    /* The index used to access the table */
};

/*
** This function is called for every node of an expression that is a candidate
** for a cursor hint on an index cursor.  For TK_COLUMN nodes that reference
** the table CCurHint.iTabCur, verify that the same column can be
** accessed through the index.  If it cannot, then set pWalker->eCode to 1.
*/
static int codeCursorHintCheckExpr(Walker *pWalker, Expr *pExpr){
  struct CCurHint *pHint = pWalker->u.pCCurHint;
  assert( pHint->pIdx!=0 );
  if( pExpr->op==TK_COLUMN
   && pExpr->iTable==pHint->iTabCur
   && sqlite3ColumnOfIndex(pHint->pIdx, pExpr->iColumn)<0
  ){
    pWalker->eCode = 1;
  }
  return WRC_Continue;
}

/*
** Test whether or not expression pExpr, which was part of a WHERE clause,
** should be included in the cursor-hint for a table that is on the rhs
** of a LEFT JOIN. Set Walker.eCode to non-zero before returning if the 
** expression is not suitable.
**
** An expression is unsuitable if it might evaluate to non NULL even if
** a TK_COLUMN node that does affect the value of the expression is set
** to NULL. For example:
**
**   col IS NULL
**   col IS NOT NULL
**   coalesce(col, 1)
**   CASE WHEN col THEN 0 ELSE 1 END
*/
static int codeCursorHintIsOrFunction(Walker *pWalker, Expr *pExpr){
  if( pExpr->op==TK_IS 
   || pExpr->op==TK_ISNULL || pExpr->op==TK_ISNOT 
   || pExpr->op==TK_NOTNULL || pExpr->op==TK_CASE 
  ){
    pWalker->eCode = 1;
  }else if( pExpr->op==TK_FUNCTION ){
    int d1;
    char d2[4];
    if( 0==sqlite3IsLikeFunction(pWalker->pParse->db, pExpr, &d1, d2) ){
      pWalker->eCode = 1;
    }
  }

  return WRC_Continue;
}


/*
** This function is called on every node of an expression tree used as an
** argument to the OP_CursorHint instruction. If the node is a TK_COLUMN
** that accesses any table other than the one identified by
** CCurHint.iTabCur, then do the following:
**
**   1) allocate a register and code an OP_Column instruction to read 
**      the specified column into the new register, and
**
**   2) transform the expression node to a TK_REGISTER node that reads 
**      from the newly populated register.
**
** Also, if the node is a TK_COLUMN that does access the table idenified
** by pCCurHint.iTabCur, and an index is being used (which we will
** know because CCurHint.pIdx!=0) then transform the TK_COLUMN into
** an access of the index rather than the original table.
*/
static int codeCursorHintFixExpr(Walker *pWalker, Expr *pExpr){
  int rc = WRC_Continue;
  struct CCurHint *pHint = pWalker->u.pCCurHint;
  if( pExpr->op==TK_COLUMN ){
    if( pExpr->iTable!=pHint->iTabCur ){
      int reg = ++pWalker->pParse->nMem;   /* Register for column value */
      sqlite3ExprCode(pWalker->pParse, pExpr, reg);
      pExpr->op = TK_REGISTER;
      pExpr->iTable = reg;
    }else if( pHint->pIdx!=0 ){
      pExpr->iTable = pHint->iIdxCur;
      pExpr->iColumn = sqlite3ColumnOfIndex(pHint->pIdx, pExpr->iColumn);
      assert( pExpr->iColumn>=0 );
    }
  }else if( pExpr->op==TK_AGG_FUNCTION ){
    /* An aggregate function in the WHERE clause of a query means this must
    ** be a correlated sub-query, and expression pExpr is an aggregate from
    ** the parent context. Do not walk the function arguments in this case.
    **
    ** todo: It should be possible to replace this node with a TK_REGISTER
    ** expression, as the result of the expression must be stored in a 
    ** register at this point. The same holds for TK_AGG_COLUMN nodes. */
    rc = WRC_Prune;
  }
  return rc;
}

/*
** Insert an OP_CursorHint instruction if it is appropriate to do so.
*/
static void codeCursorHint(
  struct SrcList_item *pTabItem,  /* FROM clause item */
  WhereInfo *pWInfo,    /* The where clause */
  WhereLevel *pLevel,   /* Which loop to provide hints for */
  WhereTerm *pEndRange  /* Hint this end-of-scan boundary term if not NULL */
){
  Parse *pParse = pWInfo->pParse;
  sqlite3 *db = pParse->db;
  Vdbe *v = pParse->pVdbe;
  Expr *pExpr = 0;
  WhereLoop *pLoop = pLevel->pWLoop;
  int iCur;
  WhereClause *pWC;
  WhereTerm *pTerm;
  int i, j;
  struct CCurHint sHint;
  Walker sWalker;

  if( OptimizationDisabled(db, SQLITE_CursorHints) ) return;
  iCur = pLevel->iTabCur;
  assert( iCur==pWInfo->pTabList->a[pLevel->iFrom].iCursor );
  sHint.iTabCur = iCur;
  sHint.iIdxCur = pLevel->iIdxCur;
  sHint.pIdx = pLoop->u.btree.pIndex;
  memset(&sWalker, 0, sizeof(sWalker));
  sWalker.pParse = pParse;
  sWalker.u.pCCurHint = &sHint;
  pWC = &pWInfo->sWC;
  for(i=0; i<pWC->nTerm; i++){
    pTerm = &pWC->a[i];
    if( pTerm->wtFlags & (TERM_VIRTUAL|TERM_CODED) ) continue;
    if( pTerm->prereqAll & pLevel->notReady ) continue;

    /* Any terms specified as part of the ON(...) clause for any LEFT 
    ** JOIN for which the current table is not the rhs are omitted
    ** from the cursor-hint. 
    **
    ** If this table is the rhs of a LEFT JOIN, "IS" or "IS NULL" terms 
    ** that were specified as part of the WHERE clause must be excluded.
    ** This is to address the following:
    **
    **   SELECT ... t1 LEFT JOIN t2 ON (t1.a=t2.b) WHERE t2.c IS NULL;
    **
    ** Say there is a single row in t2 that matches (t1.a=t2.b), but its
    ** t2.c values is not NULL. If the (t2.c IS NULL) constraint is 
    ** pushed down to the cursor, this row is filtered out, causing
    ** SQLite to synthesize a row of NULL values. Which does match the
    ** WHERE clause, and so the query returns a row. Which is incorrect.
    **
    ** For the same reason, WHERE terms such as:
    **
    **   WHERE 1 = (t2.c IS NULL)
    **
    ** are also excluded. See codeCursorHintIsOrFunction() for details.
    */
    if( pTabItem->fg.jointype & JT_LEFT ){
      Expr *pExpr = pTerm->pExpr;
      if( !ExprHasProperty(pExpr, EP_FromJoin) 
       || pExpr->iRightJoinTable!=pTabItem->iCursor
      ){
        sWalker.eCode = 0;
        sWalker.xExprCallback = codeCursorHintIsOrFunction;
        sqlite3WalkExpr(&sWalker, pTerm->pExpr);
        if( sWalker.eCode ) continue;
      }
    }else{
      if( ExprHasProperty(pTerm->pExpr, EP_FromJoin) ) continue;
    }

    /* All terms in pWLoop->aLTerm[] except pEndRange are used to initialize
    ** the cursor.  These terms are not needed as hints for a pure range
    ** scan (that has no == terms) so omit them. */
    if( pLoop->u.btree.nEq==0 && pTerm!=pEndRange ){
      for(j=0; j<pLoop->nLTerm && pLoop->aLTerm[j]!=pTerm; j++){}
      if( j<pLoop->nLTerm ) continue;
    }

    /* No subqueries or non-deterministic functions allowed */
    if( sqlite3ExprContainsSubquery(pTerm->pExpr) ) continue;

    /* For an index scan, make sure referenced columns are actually in
    ** the index. */
    if( sHint.pIdx!=0 ){
      sWalker.eCode = 0;
      sWalker.xExprCallback = codeCursorHintCheckExpr;
      sqlite3WalkExpr(&sWalker, pTerm->pExpr);
      if( sWalker.eCode ) continue;
    }

    /* If we survive all prior tests, that means this term is worth hinting */
    pExpr = sqlite3ExprAnd(pParse, pExpr, sqlite3ExprDup(db, pTerm->pExpr, 0));
  }
  if( pExpr!=0 ){
    sWalker.xExprCallback = codeCursorHintFixExpr;
    sqlite3WalkExpr(&sWalker, pExpr);
    sqlite3VdbeAddOp4(v, OP_CursorHint, 
                      (sHint.pIdx ? sHint.iIdxCur : sHint.iTabCur), 0, 0,
                      (const char*)pExpr, P4_EXPR);
  }
}
#else
# define codeCursorHint(A,B,C,D)  /* No-op */
#endif /* SQLITE_ENABLE_CURSOR_HINTS */

/*
** Cursor iCur is open on an intkey b-tree (a table). Register iRowid contains
** a rowid value just read from cursor iIdxCur, open on index pIdx. This
** function generates code to do a deferred seek of cursor iCur to the 
** rowid stored in register iRowid.
**
** Normally, this is just:
**
**   OP_DeferredSeek $iCur $iRowid
**
** However, if the scan currently being coded is a branch of an OR-loop and
** the statement currently being coded is a SELECT, then P3 of OP_DeferredSeek
** is set to iIdxCur and P4 is set to point to an array of integers
** containing one entry for each column of the table cursor iCur is open 
** on. For each table column, if the column is the i'th column of the 
** index, then the corresponding array entry is set to (i+1). If the column
** does not appear in the index at all, the array entry is set to 0.
*/
static void codeDeferredSeek(
  WhereInfo *pWInfo,              /* Where clause context */
  Index *pIdx,                    /* Index scan is using */
  int iCur,                       /* Cursor for IPK b-tree */
  int iIdxCur                     /* Index cursor */
){
  Parse *pParse = pWInfo->pParse; /* Parse context */
  Vdbe *v = pParse->pVdbe;        /* Vdbe to generate code within */

  assert( iIdxCur>0 );
  assert( pIdx->aiColumn[pIdx->nColumn-1]==-1 );
  
  sqlite3VdbeAddOp3(v, OP_DeferredSeek, iIdxCur, 0, iCur);
  if( (pWInfo->wctrlFlags & WHERE_OR_SUBCLAUSE)
   && DbMaskAllZero(sqlite3ParseToplevel(pParse)->writeMask)
  ){
    int i;
    Table *pTab = pIdx->pTable;
    int *ai = (int*)sqlite3DbMallocZero(pParse->db, sizeof(int)*(pTab->nCol+1));
    if( ai ){
      ai[0] = pTab->nCol;
      for(i=0; i<pIdx->nColumn-1; i++){
        assert( pIdx->aiColumn[i]<pTab->nCol );
        if( pIdx->aiColumn[i]>=0 ) ai[pIdx->aiColumn[i]+1] = i+1;
      }
      sqlite3VdbeChangeP4(v, -1, (char*)ai, P4_INTARRAY);
    }
  }
}

/*
** If the expression passed as the second argument is a vector, generate
** code to write the first nReg elements of the vector into an array
** of registers starting with iReg.
**
** If the expression is not a vector, then nReg must be passed 1. In
** this case, generate code to evaluate the expression and leave the
** result in register iReg.
*/
static void codeExprOrVector(Parse *pParse, Expr *p, int iReg, int nReg){
  assert( nReg>0 );
  if( p && sqlite3ExprIsVector(p) ){
#ifndef SQLITE_OMIT_SUBQUERY
    if( (p->flags & EP_xIsSelect) ){
      Vdbe *v = pParse->pVdbe;
      int iSelect;
      assert( p->op==TK_SELECT );
      iSelect = sqlite3CodeSubselect(pParse, p);
      sqlite3VdbeAddOp3(v, OP_Copy, iSelect, iReg, nReg-1);
    }else
#endif
    {
      int i;
      ExprList *pList = p->x.pList;
      assert( nReg<=pList->nExpr );
      for(i=0; i<nReg; i++){
        sqlite3ExprCode(pParse, pList->a[i].pExpr, iReg+i);
      }
    }
  }else{
    assert( nReg==1 );
    sqlite3ExprCode(pParse, p, iReg);
  }
}

/* An instance of the IdxExprTrans object carries information about a
** mapping from an expression on table columns into a column in an index
** down through the Walker.
*/
typedef struct IdxExprTrans {
  Expr *pIdxExpr;    /* The index expression */
  int iTabCur;       /* The cursor of the corresponding table */
  int iIdxCur;       /* The cursor for the index */
  int iIdxCol;       /* The column for the index */
} IdxExprTrans;

/* The walker node callback used to transform matching expressions into
** a reference to an index column for an index on an expression.
**
** If pExpr matches, then transform it into a reference to the index column
** that contains the value of pExpr.
*/
static int whereIndexExprTransNode(Walker *p, Expr *pExpr){
  IdxExprTrans *pX = p->u.pIdxTrans;
  if( sqlite3ExprCompare(0, pExpr, pX->pIdxExpr, pX->iTabCur)==0 ){
    pExpr->affExpr = sqlite3ExprAffinity(pExpr);
    pExpr->op = TK_COLUMN;
    pExpr->iTable = pX->iIdxCur;
    pExpr->iColumn = pX->iIdxCol;
    pExpr->y.pTab = 0;
    return WRC_Prune;
  }else{
    return WRC_Continue;
  }
}

/*
** For an indexes on expression X, locate every instance of expression X
** in pExpr and change that subexpression into a reference to the appropriate
** column of the index.
*/
static void whereIndexExprTrans(
  Index *pIdx,      /* The Index */
  int iTabCur,      /* Cursor of the table that is being indexed */
  int iIdxCur,      /* Cursor of the index itself */
  WhereInfo *pWInfo /* Transform expressions in this WHERE clause */
){
  int iIdxCol;               /* Column number of the index */
  ExprList *aColExpr;        /* Expressions that are indexed */
  Walker w;
  IdxExprTrans x;
  aColExpr = pIdx->aColExpr;
  if( aColExpr==0 ) return;  /* Not an index on expressions */
  memset(&w, 0, sizeof(w));
  w.xExprCallback = whereIndexExprTransNode;
  w.u.pIdxTrans = &x;
  x.iTabCur = iTabCur;
  x.iIdxCur = iIdxCur;
  for(iIdxCol=0; iIdxCol<aColExpr->nExpr; iIdxCol++){
    if( pIdx->aiColumn[iIdxCol]!=XN_EXPR ) continue;
    assert( aColExpr->a[iIdxCol].pExpr!=0 );
    x.iIdxCol = iIdxCol;
    x.pIdxExpr = aColExpr->a[iIdxCol].pExpr;
    sqlite3WalkExpr(&w, pWInfo->pWhere);
    sqlite3WalkExprList(&w, pWInfo->pOrderBy);
    sqlite3WalkExprList(&w, pWInfo->pResultSet);
  }
}

/*
** The pTruth expression is always true because it is the WHERE clause
** a partial index that is driving a query loop.  Look through all of the
** WHERE clause terms on the query, and if any of those terms must be
** true because pTruth is true, then mark those WHERE clause terms as
** coded.
*/
static void whereApplyPartialIndexConstraints(
  Expr *pTruth,
  int iTabCur,
  WhereClause *pWC
){
  int i;
  WhereTerm *pTerm;
  while( pTruth->op==TK_AND ){
    whereApplyPartialIndexConstraints(pTruth->pLeft, iTabCur, pWC);
    pTruth = pTruth->pRight;
  }
  for(i=0, pTerm=pWC->a; i<pWC->nTerm; i++, pTerm++){
    Expr *pExpr;
    if( pTerm->wtFlags & TERM_CODED ) continue;
    pExpr = pTerm->pExpr;
    if( sqlite3ExprCompare(0, pExpr, pTruth, iTabCur)==0 ){
      pTerm->wtFlags |= TERM_CODED;
    }
  }
}

/*
** Generate code for the start of the iLevel-th loop in the WHERE clause
** implementation described by pWInfo.
*/
SQLITE_PRIVATE Bitmask sqlite3WhereCodeOneLoopStart(
  Parse *pParse,       /* Parsing context */
  Vdbe *v,             /* Prepared statement under construction */
  WhereInfo *pWInfo,   /* Complete information about the WHERE clause */
  int iLevel,          /* Which level of pWInfo->a[] should be coded */
  WhereLevel *pLevel,  /* The current level pointer */
  Bitmask notReady     /* Which tables are currently available */
){
  int j, k;            /* Loop counters */
  int iCur;            /* The VDBE cursor for the table */
  int addrNxt;         /* Where to jump to continue with the next IN case */
  int bRev;            /* True if we need to scan in reverse order */
  WhereLoop *pLoop;    /* The WhereLoop object being coded */
  WhereClause *pWC;    /* Decomposition of the entire WHERE clause */
  WhereTerm *pTerm;               /* A WHERE clause term */
  sqlite3 *db;                    /* Database connection */
  struct SrcList_item *pTabItem;  /* FROM clause term being coded */
  int addrBrk;                    /* Jump here to break out of the loop */
  int addrHalt;                   /* addrBrk for the outermost loop */
  int addrCont;                   /* Jump here to continue with next cycle */
  int iRowidReg = 0;        /* Rowid is stored in this register, if not zero */
  int iReleaseReg = 0;      /* Temp register to free before returning */
  Index *pIdx = 0;          /* Index used by loop (if any) */
  int iLoop;                /* Iteration of constraint generator loop */

  pWC = &pWInfo->sWC;
  db = pParse->db;
  pLoop = pLevel->pWLoop;
  pTabItem = &pWInfo->pTabList->a[pLevel->iFrom];
  iCur = pTabItem->iCursor;
  pLevel->notReady = notReady & ~sqlite3WhereGetMask(&pWInfo->sMaskSet, iCur);
  bRev = (pWInfo->revMask>>iLevel)&1;
  VdbeModuleComment((v, "Begin WHERE-loop%d: %s",iLevel,pTabItem->pTab->zName));

  /* Create labels for the "break" and "continue" instructions
  ** for the current loop.  Jump to addrBrk to break out of a loop.
  ** Jump to cont to go immediately to the next iteration of the
  ** loop.
  **
  ** When there is an IN operator, we also have a "addrNxt" label that
  ** means to continue with the next IN value combination.  When
  ** there are no IN operators in the constraints, the "addrNxt" label
  ** is the same as "addrBrk".
  */
  addrBrk = pLevel->addrBrk = pLevel->addrNxt = sqlite3VdbeMakeLabel(pParse);
  addrCont = pLevel->addrCont = sqlite3VdbeMakeLabel(pParse);

  /* If this is the right table of a LEFT OUTER JOIN, allocate and
  ** initialize a memory cell that records if this table matches any
  ** row of the left table of the join.
  */
  assert( (pWInfo->wctrlFlags & WHERE_OR_SUBCLAUSE)
       || pLevel->iFrom>0 || (pTabItem[0].fg.jointype & JT_LEFT)==0
  );
  if( pLevel->iFrom>0 && (pTabItem[0].fg.jointype & JT_LEFT)!=0 ){
    pLevel->iLeftJoin = ++pParse->nMem;
    sqlite3VdbeAddOp2(v, OP_Integer, 0, pLevel->iLeftJoin);
    VdbeComment((v, "init LEFT JOIN no-match flag"));
  }

  /* Compute a safe address to jump to if we discover that the table for
  ** this loop is empty and can never contribute content. */
  for(j=iLevel; j>0 && pWInfo->a[j].iLeftJoin==0; j--){}
  addrHalt = pWInfo->a[j].addrBrk;

  /* Special case of a FROM clause subquery implemented as a co-routine */
  if( pTabItem->fg.viaCoroutine ){
    int regYield = pTabItem->regReturn;
    sqlite3VdbeAddOp3(v, OP_InitCoroutine, regYield, 0, pTabItem->addrFillSub);
    pLevel->p2 =  sqlite3VdbeAddOp2(v, OP_Yield, regYield, addrBrk);
    VdbeCoverage(v);
    VdbeComment((v, "next row of %s", pTabItem->pTab->zName));
    pLevel->op = OP_Goto;
  }else

#ifndef SQLITE_OMIT_VIRTUALTABLE
  if(  (pLoop->wsFlags & WHERE_VIRTUALTABLE)!=0 ){
    /* Case 1:  The table is a virtual-table.  Use the VFilter and VNext
    **          to access the data.
    */
    int iReg;   /* P3 Value for OP_VFilter */
    int addrNotFound;
    int nConstraint = pLoop->nLTerm;
    int iIn;    /* Counter for IN constraints */

    iReg = sqlite3GetTempRange(pParse, nConstraint+2);
    addrNotFound = pLevel->addrBrk;
    for(j=0; j<nConstraint; j++){
      int iTarget = iReg+j+2;
      pTerm = pLoop->aLTerm[j];
      if( NEVER(pTerm==0) ) continue;
      if( pTerm->eOperator & WO_IN ){
        codeEqualityTerm(pParse, pTerm, pLevel, j, bRev, iTarget);
        addrNotFound = pLevel->addrNxt;
      }else{
        Expr *pRight = pTerm->pExpr->pRight;
        codeExprOrVector(pParse, pRight, iTarget, 1);
      }
    }
    sqlite3VdbeAddOp2(v, OP_Integer, pLoop->u.vtab.idxNum, iReg);
    sqlite3VdbeAddOp2(v, OP_Integer, nConstraint, iReg+1);
    sqlite3VdbeAddOp4(v, OP_VFilter, iCur, addrNotFound, iReg,
                      pLoop->u.vtab.idxStr,
                      pLoop->u.vtab.needFree ? P4_DYNAMIC : P4_STATIC);
    VdbeCoverage(v);
    pLoop->u.vtab.needFree = 0;
    pLevel->p1 = iCur;
    pLevel->op = pWInfo->eOnePass ? OP_Noop : OP_VNext;
    pLevel->p2 = sqlite3VdbeCurrentAddr(v);
    iIn = pLevel->u.in.nIn;
    for(j=nConstraint-1; j>=0; j--){
      pTerm = pLoop->aLTerm[j];
      if( j<16 && (pLoop->u.vtab.omitMask>>j)&1 ){
        disableTerm(pLevel, pTerm);
      }else if( (pTerm->eOperator & WO_IN)!=0 ){
        Expr *pCompare;  /* The comparison operator */
        Expr *pRight;    /* RHS of the comparison */
        VdbeOp *pOp;     /* Opcode to access the value of the IN constraint */

        /* Reload the constraint value into reg[iReg+j+2].  The same value
        ** was loaded into the same register prior to the OP_VFilter, but
        ** the xFilter implementation might have changed the datatype or
        ** encoding of the value in the register, so it *must* be reloaded. */
        assert( pLevel->u.in.aInLoop!=0 || db->mallocFailed );
        if( !db->mallocFailed ){
          assert( iIn>0 );
          pOp = sqlite3VdbeGetOp(v, pLevel->u.in.aInLoop[--iIn].addrInTop);
          assert( pOp->opcode==OP_Column || pOp->opcode==OP_Rowid );
          assert( pOp->opcode!=OP_Column || pOp->p3==iReg+j+2 );
          assert( pOp->opcode!=OP_Rowid || pOp->p2==iReg+j+2 );
          testcase( pOp->opcode==OP_Rowid );
          sqlite3VdbeAddOp3(v, pOp->opcode, pOp->p1, pOp->p2, pOp->p3);
        }

        /* Generate code that will continue to the next row if 
        ** the IN constraint is not satisfied */
        pCompare = sqlite3PExpr(pParse, TK_EQ, 0, 0);
        assert( pCompare!=0 || db->mallocFailed );
        if( pCompare ){
          pCompare->pLeft = pTerm->pExpr->pLeft;
          pCompare->pRight = pRight = sqlite3Expr(db, TK_REGISTER, 0);
          if( pRight ){
            pRight->iTable = iReg+j+2;
            sqlite3ExprIfFalse(pParse, pCompare, pLevel->addrCont, 0);
          }
          pCompare->pLeft = 0;
          sqlite3ExprDelete(db, pCompare);
        }
      }
    }
    /* These registers need to be preserved in case there is an IN operator
    ** loop.  So we could deallocate the registers here (and potentially
    ** reuse them later) if (pLoop->wsFlags & WHERE_IN_ABLE)==0.  But it seems
    ** simpler and safer to simply not reuse the registers.
    **
    **    sqlite3ReleaseTempRange(pParse, iReg, nConstraint+2);
    */
  }else
#endif /* SQLITE_OMIT_VIRTUALTABLE */

  if( (pLoop->wsFlags & WHERE_IPK)!=0
   && (pLoop->wsFlags & (WHERE_COLUMN_IN|WHERE_COLUMN_EQ))!=0
  ){
    /* Case 2:  We can directly reference a single row using an
    **          equality comparison against the ROWID field.  Or
    **          we reference multiple rows using a "rowid IN (...)"
    **          construct.
    */
    assert( pLoop->u.btree.nEq==1 );
    pTerm = pLoop->aLTerm[0];
    assert( pTerm!=0 );
    assert( pTerm->pExpr!=0 );
    testcase( pTerm->wtFlags & TERM_VIRTUAL );
    iReleaseReg = ++pParse->nMem;
    iRowidReg = codeEqualityTerm(pParse, pTerm, pLevel, 0, bRev, iReleaseReg);
    if( iRowidReg!=iReleaseReg ) sqlite3ReleaseTempReg(pParse, iReleaseReg);
    addrNxt = pLevel->addrNxt;
    sqlite3VdbeAddOp3(v, OP_SeekRowid, iCur, addrNxt, iRowidReg);
    VdbeCoverage(v);
    pLevel->op = OP_Noop;
    if( (pTerm->prereqAll & pLevel->notReady)==0 ){
      pTerm->wtFlags |= TERM_CODED;
    }
  }else if( (pLoop->wsFlags & WHERE_IPK)!=0
         && (pLoop->wsFlags & WHERE_COLUMN_RANGE)!=0
  ){
    /* Case 3:  We have an inequality comparison against the ROWID field.
    */
    int testOp = OP_Noop;
    int start;
    int memEndValue = 0;
    WhereTerm *pStart, *pEnd;

    j = 0;
    pStart = pEnd = 0;
    if( pLoop->wsFlags & WHERE_BTM_LIMIT ) pStart = pLoop->aLTerm[j++];
    if( pLoop->wsFlags & WHERE_TOP_LIMIT ) pEnd = pLoop->aLTerm[j++];
    assert( pStart!=0 || pEnd!=0 );
    if( bRev ){
      pTerm = pStart;
      pStart = pEnd;
      pEnd = pTerm;
    }
    codeCursorHint(pTabItem, pWInfo, pLevel, pEnd);
    if( pStart ){
      Expr *pX;             /* The expression that defines the start bound */
      int r1, rTemp;        /* Registers for holding the start boundary */
      int op;               /* Cursor seek operation */

      /* The following constant maps TK_xx codes into corresponding 
      ** seek opcodes.  It depends on a particular ordering of TK_xx
      */
      const u8 aMoveOp[] = {
           /* TK_GT */  OP_SeekGT,
           /* TK_LE */  OP_SeekLE,
           /* TK_LT */  OP_SeekLT,
           /* TK_GE */  OP_SeekGE
      };
      assert( TK_LE==TK_GT+1 );      /* Make sure the ordering.. */
      assert( TK_LT==TK_GT+2 );      /*  ... of the TK_xx values... */
      assert( TK_GE==TK_GT+3 );      /*  ... is correcct. */

      assert( (pStart->wtFlags & TERM_VNULL)==0 );
      testcase( pStart->wtFlags & TERM_VIRTUAL );
      pX = pStart->pExpr;
      assert( pX!=0 );
      testcase( pStart->leftCursor!=iCur ); /* transitive constraints */
      if( sqlite3ExprIsVector(pX->pRight) ){
        r1 = rTemp = sqlite3GetTempReg(pParse);
        codeExprOrVector(pParse, pX->pRight, r1, 1);
        testcase( pX->op==TK_GT );
        testcase( pX->op==TK_GE );
        testcase( pX->op==TK_LT );
        testcase( pX->op==TK_LE );
        op = aMoveOp[((pX->op - TK_GT - 1) & 0x3) | 0x1];
        assert( pX->op!=TK_GT || op==OP_SeekGE );
        assert( pX->op!=TK_GE || op==OP_SeekGE );
        assert( pX->op!=TK_LT || op==OP_SeekLE );
        assert( pX->op!=TK_LE || op==OP_SeekLE );
      }else{
        r1 = sqlite3ExprCodeTemp(pParse, pX->pRight, &rTemp);
        disableTerm(pLevel, pStart);
        op = aMoveOp[(pX->op - TK_GT)];
      }
      sqlite3VdbeAddOp3(v, op, iCur, addrBrk, r1);
      VdbeComment((v, "pk"));
      VdbeCoverageIf(v, pX->op==TK_GT);
      VdbeCoverageIf(v, pX->op==TK_LE);
      VdbeCoverageIf(v, pX->op==TK_LT);
      VdbeCoverageIf(v, pX->op==TK_GE);
      sqlite3ReleaseTempReg(pParse, rTemp);
    }else{
      sqlite3VdbeAddOp2(v, bRev ? OP_Last : OP_Rewind, iCur, addrHalt);
      VdbeCoverageIf(v, bRev==0);
      VdbeCoverageIf(v, bRev!=0);
    }
    if( pEnd ){
      Expr *pX;
      pX = pEnd->pExpr;
      assert( pX!=0 );
      assert( (pEnd->wtFlags & TERM_VNULL)==0 );
      testcase( pEnd->leftCursor!=iCur ); /* Transitive constraints */
      testcase( pEnd->wtFlags & TERM_VIRTUAL );
      memEndValue = ++pParse->nMem;
      codeExprOrVector(pParse, pX->pRight, memEndValue, 1);
      if( 0==sqlite3ExprIsVector(pX->pRight) 
       && (pX->op==TK_LT || pX->op==TK_GT) 
      ){
        testOp = bRev ? OP_Le : OP_Ge;
      }else{
        testOp = bRev ? OP_Lt : OP_Gt;
      }
      if( 0==sqlite3ExprIsVector(pX->pRight) ){
        disableTerm(pLevel, pEnd);
      }
    }
    start = sqlite3VdbeCurrentAddr(v);
    pLevel->op = bRev ? OP_Prev : OP_Next;
    pLevel->p1 = iCur;
    pLevel->p2 = start;
    assert( pLevel->p5==0 );
    if( testOp!=OP_Noop ){
      iRowidReg = ++pParse->nMem;
      sqlite3VdbeAddOp2(v, OP_Rowid, iCur, iRowidReg);
      sqlite3VdbeAddOp3(v, testOp, memEndValue, addrBrk, iRowidReg);
      VdbeCoverageIf(v, testOp==OP_Le);
      VdbeCoverageIf(v, testOp==OP_Lt);
      VdbeCoverageIf(v, testOp==OP_Ge);
      VdbeCoverageIf(v, testOp==OP_Gt);
      sqlite3VdbeChangeP5(v, SQLITE_AFF_NUMERIC | SQLITE_JUMPIFNULL);
    }
  }else if( pLoop->wsFlags & WHERE_INDEXED ){
    /* Case 4: A scan using an index.
    **
    **         The WHERE clause may contain zero or more equality 
    **         terms ("==" or "IN" operators) that refer to the N
    **         left-most columns of the index. It may also contain
    **         inequality constraints (>, <, >= or <=) on the indexed
    **         column that immediately follows the N equalities. Only 
    **         the right-most column can be an inequality - the rest must
    **         use the "==" and "IN" operators. For example, if the 
    **         index is on (x,y,z), then the following clauses are all 
    **         optimized:
    **
    **            x=5
    **            x=5 AND y=10
    **            x=5 AND y<10
    **            x=5 AND y>5 AND y<10
    **            x=5 AND y=5 AND z<=10
    **
    **         The z<10 term of the following cannot be used, only
    **         the x=5 term:
    **
    **            x=5 AND z<10
    **
    **         N may be zero if there are inequality constraints.
    **         If there are no inequality constraints, then N is at
    **         least one.
    **
    **         This case is also used when there are no WHERE clause
    **         constraints but an index is selected anyway, in order
    **         to force the output order to conform to an ORDER BY.
    */  
    static const u8 aStartOp[] = {
      0,
      0,
      OP_Rewind,           /* 2: (!start_constraints && startEq &&  !bRev) */
      OP_Last,             /* 3: (!start_constraints && startEq &&   bRev) */
      OP_SeekGT,           /* 4: (start_constraints  && !startEq && !bRev) */
      OP_SeekLT,           /* 5: (start_constraints  && !startEq &&  bRev) */
      OP_SeekGE,           /* 6: (start_constraints  &&  startEq && !bRev) */
      OP_SeekLE            /* 7: (start_constraints  &&  startEq &&  bRev) */
    };
    static const u8 aEndOp[] = {
      OP_IdxGE,            /* 0: (end_constraints && !bRev && !endEq) */
      OP_IdxGT,            /* 1: (end_constraints && !bRev &&  endEq) */
      OP_IdxLE,            /* 2: (end_constraints &&  bRev && !endEq) */
      OP_IdxLT,            /* 3: (end_constraints &&  bRev &&  endEq) */
    };
    u16 nEq = pLoop->u.btree.nEq;     /* Number of == or IN terms */
    u16 nBtm = pLoop->u.btree.nBtm;   /* Length of BTM vector */
    u16 nTop = pLoop->u.btree.nTop;   /* Length of TOP vector */
    int regBase;                 /* Base register holding constraint values */
    WhereTerm *pRangeStart = 0;  /* Inequality constraint at range start */
    WhereTerm *pRangeEnd = 0;    /* Inequality constraint at range end */
    int startEq;                 /* True if range start uses ==, >= or <= */
    int endEq;                   /* True if range end uses ==, >= or <= */
    int start_constraints;       /* Start of range is constrained */
    int nConstraint;             /* Number of constraint terms */
    int iIdxCur;                 /* The VDBE cursor for the index */
    int nExtraReg = 0;           /* Number of extra registers needed */
    int op;                      /* Instruction opcode */
    char *zStartAff;             /* Affinity for start of range constraint */
    char *zEndAff = 0;           /* Affinity for end of range constraint */
    u8 bSeekPastNull = 0;        /* True to seek past initial nulls */
    u8 bStopAtNull = 0;          /* Add condition to terminate at NULLs */
    int omitTable;               /* True if we use the index only */
    int regBignull = 0;          /* big-null flag register */

    pIdx = pLoop->u.btree.pIndex;
    iIdxCur = pLevel->iIdxCur;
    assert( nEq>=pLoop->nSkip );

    /* Find any inequality constraint terms for the start and end 
    ** of the range. 
    */
    j = nEq;
    if( pLoop->wsFlags & WHERE_BTM_LIMIT ){
      pRangeStart = pLoop->aLTerm[j++];
      nExtraReg = MAX(nExtraReg, pLoop->u.btree.nBtm);
      /* Like optimization range constraints always occur in pairs */
      assert( (pRangeStart->wtFlags & TERM_LIKEOPT)==0 || 
              (pLoop->wsFlags & WHERE_TOP_LIMIT)!=0 );
    }
    if( pLoop->wsFlags & WHERE_TOP_LIMIT ){
      pRangeEnd = pLoop->aLTerm[j++];
      nExtraReg = MAX(nExtraReg, pLoop->u.btree.nTop);
#ifndef SQLITE_LIKE_DOESNT_MATCH_BLOBS
      if( (pRangeEnd->wtFlags & TERM_LIKEOPT)!=0 ){
        assert( pRangeStart!=0 );                     /* LIKE opt constraints */
        assert( pRangeStart->wtFlags & TERM_LIKEOPT );   /* occur in pairs */
        pLevel->iLikeRepCntr = (u32)++pParse->nMem;
        sqlite3VdbeAddOp2(v, OP_Integer, 1, (int)pLevel->iLikeRepCntr);
        VdbeComment((v, "LIKE loop counter"));
        pLevel->addrLikeRep = sqlite3VdbeCurrentAddr(v);
        /* iLikeRepCntr actually stores 2x the counter register number.  The
        ** bottom bit indicates whether the search order is ASC or DESC. */
        testcase( bRev );
        testcase( pIdx->aSortOrder[nEq]==SQLITE_SO_DESC );
        assert( (bRev & ~1)==0 );
        pLevel->iLikeRepCntr <<=1;
        pLevel->iLikeRepCntr |= bRev ^ (pIdx->aSortOrder[nEq]==SQLITE_SO_DESC);
      }
#endif
      if( pRangeStart==0 ){
        j = pIdx->aiColumn[nEq];
        if( (j>=0 && pIdx->pTable->aCol[j].notNull==0) || j==XN_EXPR ){
          bSeekPastNull = 1;
        }
      }
    }
    assert( pRangeEnd==0 || (pRangeEnd->wtFlags & TERM_VNULL)==0 );

    /* If the WHERE_BIGNULL_SORT flag is set, then index column nEq uses
    ** a non-default "big-null" sort (either ASC NULLS LAST or DESC NULLS 
    ** FIRST). In both cases separate ordered scans are made of those
    ** index entries for which the column is null and for those for which
    ** it is not. For an ASC sort, the non-NULL entries are scanned first.
    ** For DESC, NULL entries are scanned first.
    */
    if( (pLoop->wsFlags & (WHERE_TOP_LIMIT|WHERE_BTM_LIMIT))==0
     && (pLoop->wsFlags & WHERE_BIGNULL_SORT)!=0
    ){
      assert( bSeekPastNull==0 && nExtraReg==0 && nBtm==0 && nTop==0 );
      assert( pRangeEnd==0 && pRangeStart==0 );
      assert( pLoop->nSkip==0 );
      nExtraReg = 1;
      bSeekPastNull = 1;
      pLevel->regBignull = regBignull = ++pParse->nMem;
      pLevel->addrBignull = sqlite3VdbeMakeLabel(pParse);
    }

    /* If we are doing a reverse order scan on an ascending index, or
    ** a forward order scan on a descending index, interchange the 
    ** start and end terms (pRangeStart and pRangeEnd).
    */
    if( (nEq<pIdx->nKeyCol && bRev==(pIdx->aSortOrder[nEq]==SQLITE_SO_ASC))
     || (bRev && pIdx->nKeyCol==nEq)
    ){
      SWAP(WhereTerm *, pRangeEnd, pRangeStart);
      SWAP(u8, bSeekPastNull, bStopAtNull);
      SWAP(u8, nBtm, nTop);
    }

    /* Generate code to evaluate all constraint terms using == or IN
    ** and store the values of those terms in an array of registers
    ** starting at regBase.
    */
    codeCursorHint(pTabItem, pWInfo, pLevel, pRangeEnd);
    regBase = codeAllEqualityTerms(pParse,pLevel,bRev,nExtraReg,&zStartAff);
    assert( zStartAff==0 || sqlite3Strlen30(zStartAff)>=nEq );
    if( zStartAff && nTop ){
      zEndAff = sqlite3DbStrDup(db, &zStartAff[nEq]);
    }
    addrNxt = (regBignull ? pLevel->addrBignull : pLevel->addrNxt);

    testcase( pRangeStart && (pRangeStart->eOperator & WO_LE)!=0 );
    testcase( pRangeStart && (pRangeStart->eOperator & WO_GE)!=0 );
    testcase( pRangeEnd && (pRangeEnd->eOperator & WO_LE)!=0 );
    testcase( pRangeEnd && (pRangeEnd->eOperator & WO_GE)!=0 );
    startEq = !pRangeStart || pRangeStart->eOperator & (WO_LE|WO_GE);
    endEq =   !pRangeEnd || pRangeEnd->eOperator & (WO_LE|WO_GE);
    start_constraints = pRangeStart || nEq>0;

    /* Seek the index cursor to the start of the range. */
    nConstraint = nEq;
    if( pRangeStart ){
      Expr *pRight = pRangeStart->pExpr->pRight;
      codeExprOrVector(pParse, pRight, regBase+nEq, nBtm);
      whereLikeOptimizationStringFixup(v, pLevel, pRangeStart);
      if( (pRangeStart->wtFlags & TERM_VNULL)==0
       && sqlite3ExprCanBeNull(pRight)
      ){
        sqlite3VdbeAddOp2(v, OP_IsNull, regBase+nEq, addrNxt);
        VdbeCoverage(v);
      }
      if( zStartAff ){
        updateRangeAffinityStr(pRight, nBtm, &zStartAff[nEq]);
      }  
      nConstraint += nBtm;
      testcase( pRangeStart->wtFlags & TERM_VIRTUAL );
      if( sqlite3ExprIsVector(pRight)==0 ){
        disableTerm(pLevel, pRangeStart);
      }else{
        startEq = 1;
      }
      bSeekPastNull = 0;
    }else if( bSeekPastNull ){
      startEq = 0;
      sqlite3VdbeAddOp2(v, OP_Null, 0, regBase+nEq);
      start_constraints = 1;
      nConstraint++;
    }else if( regBignull ){
      sqlite3VdbeAddOp2(v, OP_Null, 0, regBase+nEq);
      start_constraints = 1;
      nConstraint++;
    }
    codeApplyAffinity(pParse, regBase, nConstraint - bSeekPastNull, zStartAff);
    if( pLoop->nSkip>0 && nConstraint==pLoop->nSkip ){
      /* The skip-scan logic inside the call to codeAllEqualityConstraints()
      ** above has already left the cursor sitting on the correct row,
      ** so no further seeking is needed */
    }else{
      if( pLoop->wsFlags & WHERE_IN_EARLYOUT ){
        sqlite3VdbeAddOp1(v, OP_SeekHit, iIdxCur);
      }
      if( regBignull ){
        sqlite3VdbeAddOp2(v, OP_Integer, 1, regBignull);
        VdbeComment((v, "NULL-scan pass ctr"));
      }

      op = aStartOp[(start_constraints<<2) + (startEq<<1) + bRev];
      assert( op!=0 );
      sqlite3VdbeAddOp4Int(v, op, iIdxCur, addrNxt, regBase, nConstraint);
      VdbeCoverage(v);
      VdbeCoverageIf(v, op==OP_Rewind);  testcase( op==OP_Rewind );
      VdbeCoverageIf(v, op==OP_Last);    testcase( op==OP_Last );
      VdbeCoverageIf(v, op==OP_SeekGT);  testcase( op==OP_SeekGT );
      VdbeCoverageIf(v, op==OP_SeekGE);  testcase( op==OP_SeekGE );
      VdbeCoverageIf(v, op==OP_SeekLE);  testcase( op==OP_SeekLE );
      VdbeCoverageIf(v, op==OP_SeekLT);  testcase( op==OP_SeekLT );

      assert( bSeekPastNull==0 || bStopAtNull==0 );
      if( regBignull ){
        assert( bSeekPastNull==1 || bStopAtNull==1 );
        assert( bSeekPastNull==!bStopAtNull );
        assert( bStopAtNull==startEq );
        sqlite3VdbeAddOp2(v, OP_Goto, 0, sqlite3VdbeCurrentAddr(v)+2);
        op = aStartOp[(nConstraint>1)*4 + 2 + bRev];
        sqlite3VdbeAddOp4Int(v, op, iIdxCur, addrNxt, regBase, 
                             nConstraint-startEq);
        VdbeCoverage(v);
        VdbeCoverageIf(v, op==OP_Rewind);  testcase( op==OP_Rewind );
        VdbeCoverageIf(v, op==OP_Last);    testcase( op==OP_Last );
        VdbeCoverageIf(v, op==OP_SeekGE);  testcase( op==OP_SeekGE );
        VdbeCoverageIf(v, op==OP_SeekLE);  testcase( op==OP_SeekLE );
        assert( op==OP_Rewind || op==OP_Last || op==OP_SeekGE || op==OP_SeekLE);
      }
    }

    /* Load the value for the inequality constraint at the end of the
    ** range (if any).
    */
    nConstraint = nEq;
    if( pRangeEnd ){
      Expr *pRight = pRangeEnd->pExpr->pRight;
      codeExprOrVector(pParse, pRight, regBase+nEq, nTop);
      whereLikeOptimizationStringFixup(v, pLevel, pRangeEnd);
      if( (pRangeEnd->wtFlags & TERM_VNULL)==0
       && sqlite3ExprCanBeNull(pRight)
      ){
        sqlite3VdbeAddOp2(v, OP_IsNull, regBase+nEq, addrNxt);
        VdbeCoverage(v);
      }
      if( zEndAff ){
        updateRangeAffinityStr(pRight, nTop, zEndAff);
        codeApplyAffinity(pParse, regBase+nEq, nTop, zEndAff);
      }else{
        assert( pParse->db->mallocFailed );
      }
      nConstraint += nTop;
      testcase( pRangeEnd->wtFlags & TERM_VIRTUAL );

      if( sqlite3ExprIsVector(pRight)==0 ){
        disableTerm(pLevel, pRangeEnd);
      }else{
        endEq = 1;
      }
    }else if( bStopAtNull ){
      if( regBignull==0 ){
        sqlite3VdbeAddOp2(v, OP_Null, 0, regBase+nEq);
        endEq = 0;
      }
      nConstraint++;
    }
    sqlite3DbFree(db, zStartAff);
    sqlite3DbFree(db, zEndAff);

    /* Top of the loop body */
    pLevel->p2 = sqlite3VdbeCurrentAddr(v);

    /* Check if the index cursor is past the end of the range. */
    if( nConstraint ){
      if( regBignull ){
        /* Except, skip the end-of-range check while doing the NULL-scan */
        sqlite3VdbeAddOp2(v, OP_IfNot, regBignull, sqlite3VdbeCurrentAddr(v)+3);
        VdbeComment((v, "If NULL-scan 2nd pass"));
        VdbeCoverage(v);
      }
      op = aEndOp[bRev*2 + endEq];
      sqlite3VdbeAddOp4Int(v, op, iIdxCur, addrNxt, regBase, nConstraint);
      testcase( op==OP_IdxGT );  VdbeCoverageIf(v, op==OP_IdxGT );
      testcase( op==OP_IdxGE );  VdbeCoverageIf(v, op==OP_IdxGE );
      testcase( op==OP_IdxLT );  VdbeCoverageIf(v, op==OP_IdxLT );
      testcase( op==OP_IdxLE );  VdbeCoverageIf(v, op==OP_IdxLE );
    }
    if( regBignull ){
      /* During a NULL-scan, check to see if we have reached the end of
      ** the NULLs */
      assert( bSeekPastNull==!bStopAtNull );
      assert( bSeekPastNull+bStopAtNull==1 );
      assert( nConstraint+bSeekPastNull>0 );
      sqlite3VdbeAddOp2(v, OP_If, regBignull, sqlite3VdbeCurrentAddr(v)+2);
      VdbeComment((v, "If NULL-scan 1st pass"));
      VdbeCoverage(v);
      op = aEndOp[bRev*2 + bSeekPastNull];
      sqlite3VdbeAddOp4Int(v, op, iIdxCur, addrNxt, regBase,
                           nConstraint+bSeekPastNull);
      testcase( op==OP_IdxGT );  VdbeCoverageIf(v, op==OP_IdxGT );
      testcase( op==OP_IdxGE );  VdbeCoverageIf(v, op==OP_IdxGE );
      testcase( op==OP_IdxLT );  VdbeCoverageIf(v, op==OP_IdxLT );
      testcase( op==OP_IdxLE );  VdbeCoverageIf(v, op==OP_IdxLE );
    }

    if( pLoop->wsFlags & WHERE_IN_EARLYOUT ){
      sqlite3VdbeAddOp2(v, OP_SeekHit, iIdxCur, 1);
    }

    /* Seek the table cursor, if required */
    omitTable = (pLoop->wsFlags & WHERE_IDX_ONLY)!=0 
           && (pWInfo->wctrlFlags & WHERE_OR_SUBCLAUSE)==0;
    if( omitTable ){
      /* pIdx is a covering index.  No need to access the main table. */
    }else if( HasRowid(pIdx->pTable) ){
      if( (pWInfo->wctrlFlags & WHERE_SEEK_TABLE) || (
          (pWInfo->wctrlFlags & WHERE_SEEK_UNIQ_TABLE) 
       && (pWInfo->eOnePass==ONEPASS_SINGLE)
      )){
        iRowidReg = ++pParse->nMem;
        sqlite3VdbeAddOp2(v, OP_IdxRowid, iIdxCur, iRowidReg);
        sqlite3VdbeAddOp3(v, OP_NotExists, iCur, 0, iRowidReg);
        VdbeCoverage(v);
      }else{
        codeDeferredSeek(pWInfo, pIdx, iCur, iIdxCur);
      }
    }else if( iCur!=iIdxCur ){
      Index *pPk = sqlite3PrimaryKeyIndex(pIdx->pTable);
      iRowidReg = sqlite3GetTempRange(pParse, pPk->nKeyCol);
      for(j=0; j<pPk->nKeyCol; j++){
        k = sqlite3ColumnOfIndex(pIdx, pPk->aiColumn[j]);
        sqlite3VdbeAddOp3(v, OP_Column, iIdxCur, k, iRowidReg+j);
      }
      sqlite3VdbeAddOp4Int(v, OP_NotFound, iCur, addrCont,
                           iRowidReg, pPk->nKeyCol); VdbeCoverage(v);
    }

    /* If pIdx is an index on one or more expressions, then look through
    ** all the expressions in pWInfo and try to transform matching expressions
    ** into reference to index columns.
    **
    ** Do not do this for the RHS of a LEFT JOIN. This is because the 
    ** expression may be evaluated after OP_NullRow has been executed on
    ** the cursor. In this case it is important to do the full evaluation,
    ** as the result of the expression may not be NULL, even if all table
    ** column values are.  https://www.sqlite.org/src/info/7fa8049685b50b5a
    **
    ** Also, do not do this when processing one index an a multi-index
    ** OR clause, since the transformation will become invalid once we
    ** move forward to the next index.
    ** https://sqlite.org/src/info/4e8e4857d32d401f
    */
    if( pLevel->iLeftJoin==0 && (pWInfo->wctrlFlags & WHERE_OR_SUBCLAUSE)==0 ){
      whereIndexExprTrans(pIdx, iCur, iIdxCur, pWInfo);
    }

    /* If a partial index is driving the loop, try to eliminate WHERE clause
    ** terms from the query that must be true due to the WHERE clause of
    ** the partial index
    */
    if( pIdx->pPartIdxWhere ){
      whereApplyPartialIndexConstraints(pIdx->pPartIdxWhere, iCur, pWC);
    }

    /* Record the instruction used to terminate the loop. */
    if( pLoop->wsFlags & WHERE_ONEROW ){
      pLevel->op = OP_Noop;
    }else if( bRev ){
      pLevel->op = OP_Prev;
    }else{
      pLevel->op = OP_Next;
    }
    pLevel->p1 = iIdxCur;
    pLevel->p3 = (pLoop->wsFlags&WHERE_UNQ_WANTED)!=0 ? 1:0;
    if( (pLoop->wsFlags & WHERE_CONSTRAINT)==0 ){
      pLevel->p5 = SQLITE_STMTSTATUS_FULLSCAN_STEP;
    }else{
      assert( pLevel->p5==0 );
    }
    if( omitTable ) pIdx = 0;
  }else

#ifndef SQLITE_OMIT_OR_OPTIMIZATION
  if( pLoop->wsFlags & WHERE_MULTI_OR ){
    /* Case 5:  Two or more separately indexed terms connected by OR
    **
    ** Example:
    **
    **   CREATE TABLE t1(a,b,c,d);
    **   CREATE INDEX i1 ON t1(a);
    **   CREATE INDEX i2 ON t1(b);
    **   CREATE INDEX i3 ON t1(c);
    **
    **   SELECT * FROM t1 WHERE a=5 OR b=7 OR (c=11 AND d=13)
    **
    ** In the example, there are three indexed terms connected by OR.
    ** The top of the loop looks like this:
    **
    **          Null       1                # Zero the rowset in reg 1
    **
    ** Then, for each indexed term, the following. The arguments to
    ** RowSetTest are such that the rowid of the current row is inserted
    ** into the RowSet. If it is already present, control skips the
    ** Gosub opcode and jumps straight to the code generated by WhereEnd().
    **
    **        sqlite3WhereBegin(<term>)
    **          RowSetTest                  # Insert rowid into rowset
    **          Gosub      2 A
    **        sqlite3WhereEnd()
    **
    ** Following the above, code to terminate the loop. Label A, the target
    ** of the Gosub above, jumps to the instruction right after the Goto.
    **
    **          Null       1                # Zero the rowset in reg 1
    **          Goto       B                # The loop is finished.
    **
    **       A: <loop body>                 # Return data, whatever.
    **
    **          Return     2                # Jump back to the Gosub
    **
    **       B: <after the loop>
    **
    ** Added 2014-05-26: If the table is a WITHOUT ROWID table, then
    ** use an ephemeral index instead of a RowSet to record the primary
    ** keys of the rows we have already seen.
    **
    */
    WhereClause *pOrWc;    /* The OR-clause broken out into subterms */
    SrcList *pOrTab;       /* Shortened table list or OR-clause generation */
    Index *pCov = 0;             /* Potential covering index (or NULL) */
    int iCovCur = pParse->nTab++;  /* Cursor used for index scans (if any) */

    int regReturn = ++pParse->nMem;           /* Register used with OP_Gosub */
    int regRowset = 0;                        /* Register for RowSet object */
    int regRowid = 0;                         /* Register holding rowid */
    int iLoopBody = sqlite3VdbeMakeLabel(pParse);/* Start of loop body */
    int iRetInit;                             /* Address of regReturn init */
    int untestedTerms = 0;             /* Some terms not completely tested */
    int ii;                            /* Loop counter */
    u16 wctrlFlags;                    /* Flags for sub-WHERE clause */
    Expr *pAndExpr = 0;                /* An ".. AND (...)" expression */
    Table *pTab = pTabItem->pTab;

    pTerm = pLoop->aLTerm[0];
    assert( pTerm!=0 );
    assert( pTerm->eOperator & WO_OR );
    assert( (pTerm->wtFlags & TERM_ORINFO)!=0 );
    pOrWc = &pTerm->u.pOrInfo->wc;
    pLevel->op = OP_Return;
    pLevel->p1 = regReturn;

    /* Set up a new SrcList in pOrTab containing the table being scanned
    ** by this loop in the a[0] slot and all notReady tables in a[1..] slots.
    ** This becomes the SrcList in the recursive call to sqlite3WhereBegin().
    */
    if( pWInfo->nLevel>1 ){
      int nNotReady;                 /* The number of notReady tables */
      struct SrcList_item *origSrc;     /* Original list of tables */
      nNotReady = pWInfo->nLevel - iLevel - 1;
      pOrTab = sqlite3StackAllocRaw(db,
                            sizeof(*pOrTab)+ nNotReady*sizeof(pOrTab->a[0]));
      if( pOrTab==0 ) return notReady;
      pOrTab->nAlloc = (u8)(nNotReady + 1);
      pOrTab->nSrc = pOrTab->nAlloc;
      memcpy(pOrTab->a, pTabItem, sizeof(*pTabItem));
      origSrc = pWInfo->pTabList->a;
      for(k=1; k<=nNotReady; k++){
        memcpy(&pOrTab->a[k], &origSrc[pLevel[k].iFrom], sizeof(pOrTab->a[k]));
      }
    }else{
      pOrTab = pWInfo->pTabList;
    }

    /* Initialize the rowset register to contain NULL. An SQL NULL is 
    ** equivalent to an empty rowset.  Or, create an ephemeral index
    ** capable of holding primary keys in the case of a WITHOUT ROWID.
    **
    ** Also initialize regReturn to contain the address of the instruction 
    ** immediately following the OP_Return at the bottom of the loop. This
    ** is required in a few obscure LEFT JOIN cases where control jumps
    ** over the top of the loop into the body of it. In this case the 
    ** correct response for the end-of-loop code (the OP_Return) is to 
    ** fall through to the next instruction, just as an OP_Next does if
    ** called on an uninitialized cursor.
    */
    if( (pWInfo->wctrlFlags & WHERE_DUPLICATES_OK)==0 ){
      if( HasRowid(pTab) ){
        regRowset = ++pParse->nMem;
        sqlite3VdbeAddOp2(v, OP_Null, 0, regRowset);
      }else{
        Index *pPk = sqlite3PrimaryKeyIndex(pTab);
        regRowset = pParse->nTab++;
        sqlite3VdbeAddOp2(v, OP_OpenEphemeral, regRowset, pPk->nKeyCol);
        sqlite3VdbeSetP4KeyInfo(pParse, pPk);
      }
      regRowid = ++pParse->nMem;
    }
    iRetInit = sqlite3VdbeAddOp2(v, OP_Integer, 0, regReturn);

    /* If the original WHERE clause is z of the form:  (x1 OR x2 OR ...) AND y
    ** Then for every term xN, evaluate as the subexpression: xN AND z
    ** That way, terms in y that are factored into the disjunction will
    ** be picked up by the recursive calls to sqlite3WhereBegin() below.
    **
    ** Actually, each subexpression is converted to "xN AND w" where w is
    ** the "interesting" terms of z - terms that did not originate in the
    ** ON or USING clause of a LEFT JOIN, and terms that are usable as 
    ** indices.
    **
    ** This optimization also only applies if the (x1 OR x2 OR ...) term
    ** is not contained in the ON clause of a LEFT JOIN.
    ** See ticket http://www.sqlite.org/src/info/f2369304e4
    */
    if( pWC->nTerm>1 ){
      int iTerm;
      for(iTerm=0; iTerm<pWC->nTerm; iTerm++){
        Expr *pExpr = pWC->a[iTerm].pExpr;
        if( &pWC->a[iTerm] == pTerm ) continue;
        testcase( pWC->a[iTerm].wtFlags & TERM_VIRTUAL );
        testcase( pWC->a[iTerm].wtFlags & TERM_CODED );
        if( (pWC->a[iTerm].wtFlags & (TERM_VIRTUAL|TERM_CODED))!=0 ) continue;
        if( (pWC->a[iTerm].eOperator & WO_ALL)==0 ) continue;
        testcase( pWC->a[iTerm].wtFlags & TERM_ORINFO );
        pExpr = sqlite3ExprDup(db, pExpr, 0);
        pAndExpr = sqlite3ExprAnd(pParse, pAndExpr, pExpr);
      }
      if( pAndExpr ){
        /* The extra 0x10000 bit on the opcode is masked off and does not
        ** become part of the new Expr.op.  However, it does make the
        ** op==TK_AND comparison inside of sqlite3PExpr() false, and this
        ** prevents sqlite3PExpr() from implementing AND short-circuit 
        ** optimization, which we do not want here. */
        pAndExpr = sqlite3PExpr(pParse, TK_AND|0x10000, 0, pAndExpr);
      }
    }

    /* Run a separate WHERE clause for each term of the OR clause.  After
    ** eliminating duplicates from other WHERE clauses, the action for each
    ** sub-WHERE clause is to to invoke the main loop body as a subroutine.
    */
    wctrlFlags =  WHERE_OR_SUBCLAUSE | (pWInfo->wctrlFlags & WHERE_SEEK_TABLE);
    ExplainQueryPlan((pParse, 1, "MULTI-INDEX OR"));
    for(ii=0; ii<pOrWc->nTerm; ii++){
      WhereTerm *pOrTerm = &pOrWc->a[ii];
      if( pOrTerm->leftCursor==iCur || (pOrTerm->eOperator & WO_AND)!=0 ){
        WhereInfo *pSubWInfo;           /* Info for single OR-term scan */
        Expr *pOrExpr = pOrTerm->pExpr; /* Current OR clause term */
        int jmp1 = 0;                   /* Address of jump operation */
        assert( (pTabItem[0].fg.jointype & JT_LEFT)==0 
             || ExprHasProperty(pOrExpr, EP_FromJoin) 
        );
        if( pAndExpr ){
          pAndExpr->pLeft = pOrExpr;
          pOrExpr = pAndExpr;
        }
        /* Loop through table entries that match term pOrTerm. */
        ExplainQueryPlan((pParse, 1, "INDEX %d", ii+1));
        WHERETRACE(0xffff, ("Subplan for OR-clause:\n"));
        pSubWInfo = sqlite3WhereBegin(pParse, pOrTab, pOrExpr, 0, 0,
                                      wctrlFlags, iCovCur);
        assert( pSubWInfo || pParse->nErr || db->mallocFailed );
        if( pSubWInfo ){
          WhereLoop *pSubLoop;
          int addrExplain = sqlite3WhereExplainOneScan(
              pParse, pOrTab, &pSubWInfo->a[0], 0
          );
          sqlite3WhereAddScanStatus(v, pOrTab, &pSubWInfo->a[0], addrExplain);

          /* This is the sub-WHERE clause body.  First skip over
          ** duplicate rows from prior sub-WHERE clauses, and record the
          ** rowid (or PRIMARY KEY) for the current row so that the same
          ** row will be skipped in subsequent sub-WHERE clauses.
          */
          if( (pWInfo->wctrlFlags & WHERE_DUPLICATES_OK)==0 ){
            int iSet = ((ii==pOrWc->nTerm-1)?-1:ii);
            if( HasRowid(pTab) ){
              sqlite3ExprCodeGetColumnOfTable(v, pTab, iCur, -1, regRowid);
              jmp1 = sqlite3VdbeAddOp4Int(v, OP_RowSetTest, regRowset, 0,
                                          regRowid, iSet);
              VdbeCoverage(v);
            }else{
              Index *pPk = sqlite3PrimaryKeyIndex(pTab);
              int nPk = pPk->nKeyCol;
              int iPk;
              int r;

              /* Read the PK into an array of temp registers. */
              r = sqlite3GetTempRange(pParse, nPk);
              for(iPk=0; iPk<nPk; iPk++){
                int iCol = pPk->aiColumn[iPk];
                sqlite3ExprCodeGetColumnOfTable(v, pTab, iCur, iCol, r+iPk);
              }

              /* Check if the temp table already contains this key. If so,
              ** the row has already been included in the result set and
              ** can be ignored (by jumping past the Gosub below). Otherwise,
              ** insert the key into the temp table and proceed with processing
              ** the row.
              **
              ** Use some of the same optimizations as OP_RowSetTest: If iSet
              ** is zero, assume that the key cannot already be present in
              ** the temp table. And if iSet is -1, assume that there is no 
              ** need to insert the key into the temp table, as it will never 
              ** be tested for.  */ 
              if( iSet ){
                jmp1 = sqlite3VdbeAddOp4Int(v, OP_Found, regRowset, 0, r, nPk);
                VdbeCoverage(v);
              }
              if( iSet>=0 ){
                sqlite3VdbeAddOp3(v, OP_MakeRecord, r, nPk, regRowid);
                sqlite3VdbeAddOp4Int(v, OP_IdxInsert, regRowset, regRowid,
                                     r, nPk);
                if( iSet ) sqlite3VdbeChangeP5(v, OPFLAG_USESEEKRESULT);
              }

              /* Release the array of temp registers */
              sqlite3ReleaseTempRange(pParse, r, nPk);
            }
          }

          /* Invoke the main loop body as a subroutine */
          sqlite3VdbeAddOp2(v, OP_Gosub, regReturn, iLoopBody);

          /* Jump here (skipping the main loop body subroutine) if the
          ** current sub-WHERE row is a duplicate from prior sub-WHEREs. */
          if( jmp1 ) sqlite3VdbeJumpHere(v, jmp1);

          /* The pSubWInfo->untestedTerms flag means that this OR term
          ** contained one or more AND term from a notReady table.  The
          ** terms from the notReady table could not be tested and will
          ** need to be tested later.
          */
          if( pSubWInfo->untestedTerms ) untestedTerms = 1;

          /* If all of the OR-connected terms are optimized using the same
          ** index, and the index is opened using the same cursor number
          ** by each call to sqlite3WhereBegin() made by this loop, it may
          ** be possible to use that index as a covering index.
          **
          ** If the call to sqlite3WhereBegin() above resulted in a scan that
          ** uses an index, and this is either the first OR-connected term
          ** processed or the index is the same as that used by all previous
          ** terms, set pCov to the candidate covering index. Otherwise, set 
          ** pCov to NULL to indicate that no candidate covering index will 
          ** be available.
          */
          pSubLoop = pSubWInfo->a[0].pWLoop;
          assert( (pSubLoop->wsFlags & WHERE_AUTO_INDEX)==0 );
          if( (pSubLoop->wsFlags & WHERE_INDEXED)!=0
           && (ii==0 || pSubLoop->u.btree.pIndex==pCov)
           && (HasRowid(pTab) || !IsPrimaryKeyIndex(pSubLoop->u.btree.pIndex))
          ){
            assert( pSubWInfo->a[0].iIdxCur==iCovCur );
            pCov = pSubLoop->u.btree.pIndex;
          }else{
            pCov = 0;
          }

          /* Finish the loop through table entries that match term pOrTerm. */
          sqlite3WhereEnd(pSubWInfo);
          ExplainQueryPlanPop(pParse);
        }
      }
    }
    ExplainQueryPlanPop(pParse);
    pLevel->u.pCovidx = pCov;
    if( pCov ) pLevel->iIdxCur = iCovCur;
    if( pAndExpr ){
      pAndExpr->pLeft = 0;
      sqlite3ExprDelete(db, pAndExpr);
    }
    sqlite3VdbeChangeP1(v, iRetInit, sqlite3VdbeCurrentAddr(v));
    sqlite3VdbeGoto(v, pLevel->addrBrk);
    sqlite3VdbeResolveLabel(v, iLoopBody);

    if( pWInfo->nLevel>1 ){ sqlite3StackFree(db, pOrTab); }
    if( !untestedTerms ) disableTerm(pLevel, pTerm);
  }else
#endif /* SQLITE_OMIT_OR_OPTIMIZATION */

  {
    /* Case 6:  There is no usable index.  We must do a complete
    **          scan of the entire table.
    */
    static const u8 aStep[] = { OP_Next, OP_Prev };
    static const u8 aStart[] = { OP_Rewind, OP_Last };
    assert( bRev==0 || bRev==1 );
    if( pTabItem->fg.isRecursive ){
      /* Tables marked isRecursive have only a single row that is stored in
      ** a pseudo-cursor.  No need to Rewind or Next such cursors. */
      pLevel->op = OP_Noop;
    }else{
      codeCursorHint(pTabItem, pWInfo, pLevel, 0);
      pLevel->op = aStep[bRev];
      pLevel->p1 = iCur;
      pLevel->p2 = 1 + sqlite3VdbeAddOp2(v, aStart[bRev], iCur, addrHalt);
      VdbeCoverageIf(v, bRev==0);
      VdbeCoverageIf(v, bRev!=0);
      pLevel->p5 = SQLITE_STMTSTATUS_FULLSCAN_STEP;
    }
  }

#ifdef SQLITE_ENABLE_STMT_SCANSTATUS
  pLevel->addrVisit = sqlite3VdbeCurrentAddr(v);
#endif

  /* Insert code to test every subexpression that can be completely
  ** computed using the current set of tables.
  **
  ** This loop may run between one and three times, depending on the
  ** constraints to be generated. The value of stack variable iLoop
  ** determines the constraints coded by each iteration, as follows:
  **
  ** iLoop==1: Code only expressions that are entirely covered by pIdx.
  ** iLoop==2: Code remaining expressions that do not contain correlated
  **           sub-queries.  
  ** iLoop==3: Code all remaining expressions.
  **
  ** An effort is made to skip unnecessary iterations of the loop.
  */
  iLoop = (pIdx ? 1 : 2);
  do{
    int iNext = 0;                /* Next value for iLoop */
    for(pTerm=pWC->a, j=pWC->nTerm; j>0; j--, pTerm++){
      Expr *pE;
      int skipLikeAddr = 0;
      testcase( pTerm->wtFlags & TERM_VIRTUAL );
      testcase( pTerm->wtFlags & TERM_CODED );
      if( pTerm->wtFlags & (TERM_VIRTUAL|TERM_CODED) ) continue;
      if( (pTerm->prereqAll & pLevel->notReady)!=0 ){
        testcase( pWInfo->untestedTerms==0
            && (pWInfo->wctrlFlags & WHERE_OR_SUBCLAUSE)!=0 );
        pWInfo->untestedTerms = 1;
        continue;
      }
      pE = pTerm->pExpr;
      assert( pE!=0 );
      if( (pTabItem->fg.jointype&JT_LEFT) && !ExprHasProperty(pE,EP_FromJoin) ){
        continue;
      }
      
      if( iLoop==1 && !sqlite3ExprCoveredByIndex(pE, pLevel->iTabCur, pIdx) ){
        iNext = 2;
        continue;
      }
      if( iLoop<3 && (pTerm->wtFlags & TERM_VARSELECT) ){
        if( iNext==0 ) iNext = 3;
        continue;
      }

      if( (pTerm->wtFlags & TERM_LIKECOND)!=0 ){
        /* If the TERM_LIKECOND flag is set, that means that the range search
        ** is sufficient to guarantee that the LIKE operator is true, so we
        ** can skip the call to the like(A,B) function.  But this only works
        ** for strings.  So do not skip the call to the function on the pass
        ** that compares BLOBs. */
#ifdef SQLITE_LIKE_DOESNT_MATCH_BLOBS
        continue;
#else
        u32 x = pLevel->iLikeRepCntr;
        if( x>0 ){
          skipLikeAddr = sqlite3VdbeAddOp1(v, (x&1)?OP_IfNot:OP_If,(int)(x>>1));
          VdbeCoverageIf(v, (x&1)==1);
          VdbeCoverageIf(v, (x&1)==0);
        }
#endif
      }
#ifdef WHERETRACE_ENABLED /* 0xffff */
      if( sqlite3WhereTrace ){
        VdbeNoopComment((v, "WhereTerm[%d] (%p) priority=%d",
                         pWC->nTerm-j, pTerm, iLoop));
      }
#endif
      sqlite3ExprIfFalse(pParse, pE, addrCont, SQLITE_JUMPIFNULL);
      if( skipLikeAddr ) sqlite3VdbeJumpHere(v, skipLikeAddr);
      pTerm->wtFlags |= TERM_CODED;
    }
    iLoop = iNext;
  }while( iLoop>0 );

  /* Insert code to test for implied constraints based on transitivity
  ** of the "==" operator.
  **
  ** Example: If the WHERE clause contains "t1.a=t2.b" and "t2.b=123"
  ** and we are coding the t1 loop and the t2 loop has not yet coded,
  ** then we cannot use the "t1.a=t2.b" constraint, but we can code
  ** the implied "t1.a=123" constraint.
  */
  for(pTerm=pWC->a, j=pWC->nTerm; j>0; j--, pTerm++){
    Expr *pE, sEAlt;
    WhereTerm *pAlt;
    if( pTerm->wtFlags & (TERM_VIRTUAL|TERM_CODED) ) continue;
    if( (pTerm->eOperator & (WO_EQ|WO_IS))==0 ) continue;
    if( (pTerm->eOperator & WO_EQUIV)==0 ) continue;
    if( pTerm->leftCursor!=iCur ) continue;
    if( pLevel->iLeftJoin ) continue;
    pE = pTerm->pExpr;
    assert( !ExprHasProperty(pE, EP_FromJoin) );
    assert( (pTerm->prereqRight & pLevel->notReady)!=0 );
    pAlt = sqlite3WhereFindTerm(pWC, iCur, pTerm->u.leftColumn, notReady,
                    WO_EQ|WO_IN|WO_IS, 0);
    if( pAlt==0 ) continue;
    if( pAlt->wtFlags & (TERM_CODED) ) continue;
    if( (pAlt->eOperator & WO_IN) 
     && (pAlt->pExpr->flags & EP_xIsSelect)
     && (pAlt->pExpr->x.pSelect->pEList->nExpr>1)
    ){
      continue;
    }
    testcase( pAlt->eOperator & WO_EQ );
    testcase( pAlt->eOperator & WO_IS );
    testcase( pAlt->eOperator & WO_IN );
    VdbeModuleComment((v, "begin transitive constraint"));
    sEAlt = *pAlt->pExpr;
    sEAlt.pLeft = pE->pLeft;
    sqlite3ExprIfFalse(pParse, &sEAlt, addrCont, SQLITE_JUMPIFNULL);
  }

  /* For a LEFT OUTER JOIN, generate code that will record the fact that
  ** at least one row of the right table has matched the left table.  
  */
  if( pLevel->iLeftJoin ){
    pLevel->addrFirst = sqlite3VdbeCurrentAddr(v);
    sqlite3VdbeAddOp2(v, OP_Integer, 1, pLevel->iLeftJoin);
    VdbeComment((v, "record LEFT JOIN hit"));
    for(pTerm=pWC->a, j=0; j<pWC->nTerm; j++, pTerm++){
      testcase( pTerm->wtFlags & TERM_VIRTUAL );
      testcase( pTerm->wtFlags & TERM_CODED );
      if( pTerm->wtFlags & (TERM_VIRTUAL|TERM_CODED) ) continue;
      if( (pTerm->prereqAll & pLevel->notReady)!=0 ){
        assert( pWInfo->untestedTerms );
        continue;
      }
      assert( pTerm->pExpr );
      sqlite3ExprIfFalse(pParse, pTerm->pExpr, addrCont, SQLITE_JUMPIFNULL);
      pTerm->wtFlags |= TERM_CODED;
    }
  }

  return pLevel->notReady;
}

/************** End of wherecode.c *******************************************/
/************** Begin file whereexpr.c ***************************************/
/*
** 2015-06-08
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This module contains C code that generates VDBE code used to process
** the WHERE clause of SQL statements.
**
** This file was originally part of where.c but was split out to improve
** readability and editabiliity.  This file contains utility routines for
** analyzing Expr objects in the WHERE clause.
*/
/* #include "sqliteInt.h" */
/* #include "whereInt.h" */

/* Forward declarations */
static void exprAnalyze(SrcList*, WhereClause*, int);

/*
** Deallocate all memory associated with a WhereOrInfo object.
*/
static void whereOrInfoDelete(sqlite3 *db, WhereOrInfo *p){
  sqlite3WhereClauseClear(&p->wc);
  sqlite3DbFree(db, p);
}

/*
** Deallocate all memory associated with a WhereAndInfo object.
*/
static void whereAndInfoDelete(sqlite3 *db, WhereAndInfo *p){
  sqlite3WhereClauseClear(&p->wc);
  sqlite3DbFree(db, p);
}

/*
** Add a single new WhereTerm entry to the WhereClause object pWC.
** The new WhereTerm object is constructed from Expr p and with wtFlags.
** The index in pWC->a[] of the new WhereTerm is returned on success.
** 0 is returned if the new WhereTerm could not be added due to a memory
** allocation error.  The memory allocation failure will be recorded in
** the db->mallocFailed flag so that higher-level functions can detect it.
**
** This routine will increase the size of the pWC->a[] array as necessary.
**
** If the wtFlags argument includes TERM_DYNAMIC, then responsibility
** for freeing the expression p is assumed by the WhereClause object pWC.
** This is true even if this routine fails to allocate a new WhereTerm.
**
** WARNING:  This routine might reallocate the space used to store
** WhereTerms.  All pointers to WhereTerms should be invalidated after
** calling this routine.  Such pointers may be reinitialized by referencing
** the pWC->a[] array.
*/
static int whereClauseInsert(WhereClause *pWC, Expr *p, u16 wtFlags){
  WhereTerm *pTerm;
  int idx;
  testcase( wtFlags & TERM_VIRTUAL );
  if( pWC->nTerm>=pWC->nSlot ){
    WhereTerm *pOld = pWC->a;
    sqlite3 *db = pWC->pWInfo->pParse->db;
    pWC->a = sqlite3DbMallocRawNN(db, sizeof(pWC->a[0])*pWC->nSlot*2 );
    if( pWC->a==0 ){
      if( wtFlags & TERM_DYNAMIC ){
        sqlite3ExprDelete(db, p);
      }
      pWC->a = pOld;
      return 0;
    }
    memcpy(pWC->a, pOld, sizeof(pWC->a[0])*pWC->nTerm);
    if( pOld!=pWC->aStatic ){
      sqlite3DbFree(db, pOld);
    }
    pWC->nSlot = sqlite3DbMallocSize(db, pWC->a)/sizeof(pWC->a[0]);
  }
  pTerm = &pWC->a[idx = pWC->nTerm++];
  if( p && ExprHasProperty(p, EP_Unlikely) ){
    pTerm->truthProb = sqlite3LogEst(p->iTable) - 270;
  }else{
    pTerm->truthProb = 1;
  }
  pTerm->pExpr = sqlite3ExprSkipCollateAndLikely(p);
  pTerm->wtFlags = wtFlags;
  pTerm->pWC = pWC;
  pTerm->iParent = -1;
  memset(&pTerm->eOperator, 0,
         sizeof(WhereTerm) - offsetof(WhereTerm,eOperator));
  return idx;
}

/*
** Return TRUE if the given operator is one of the operators that is
** allowed for an indexable WHERE clause term.  The allowed operators are
** "=", "<", ">", "<=", ">=", "IN", "IS", and "IS NULL"
*/
static int allowedOp(int op){
  assert( TK_GT>TK_EQ && TK_GT<TK_GE );
  assert( TK_LT>TK_EQ && TK_LT<TK_GE );
  assert( TK_LE>TK_EQ && TK_LE<TK_GE );
  assert( TK_GE==TK_EQ+4 );
  return op==TK_IN || (op>=TK_EQ && op<=TK_GE) || op==TK_ISNULL || op==TK_IS;
}

/*
** Commute a comparison operator.  Expressions of the form "X op Y"
** are converted into "Y op X".
**
** If left/right precedence rules come into play when determining the
** collating sequence, then COLLATE operators are adjusted to ensure
** that the collating sequence does not change.  For example:
** "Y collate NOCASE op X" becomes "X op Y" because any collation sequence on
** the left hand side of a comparison overrides any collation sequence 
** attached to the right. For the same reason the EP_Collate flag
** is not commuted.
**
** The return value is extra flags that are added to the WhereTerm object
** after it is commuted.  The only extra flag ever added is TERM_NOPARTIDX
** which prevents the term from being used to enable a partial index if
** COLLATE changes have been made.
*/
static u16 exprCommute(Parse *pParse, Expr *pExpr){
  u16 expRight = (pExpr->pRight->flags & EP_Collate);
  u16 expLeft = (pExpr->pLeft->flags & EP_Collate);
  u16 wtFlags = 0;
  assert( allowedOp(pExpr->op) && pExpr->op!=TK_IN );
  if( expRight==expLeft ){
    /* Either X and Y both have COLLATE operator or neither do */
    if( expRight ){
      /* Both X and Y have COLLATE operators.  Make sure X is always
      ** used by clearing the EP_Collate flag from Y. */
      pExpr->pRight->flags &= ~EP_Collate;
      wtFlags |= TERM_NOPARTIDX;
    }else if( sqlite3ExprCollSeq(pParse, pExpr->pLeft)!=0 ){
      /* Neither X nor Y have COLLATE operators, but X has a non-default
      ** collating sequence.  So add the EP_Collate marker on X to cause
      ** it to be searched first. */
      pExpr->pLeft->flags |= EP_Collate;
      wtFlags |= TERM_NOPARTIDX;
    }
  }
  SWAP(Expr*,pExpr->pRight,pExpr->pLeft);
  if( pExpr->op>=TK_GT ){
    assert( TK_LT==TK_GT+2 );
    assert( TK_GE==TK_LE+2 );
    assert( TK_GT>TK_EQ );
    assert( TK_GT<TK_LE );
    assert( pExpr->op>=TK_GT && pExpr->op<=TK_GE );
    pExpr->op = ((pExpr->op-TK_GT)^2)+TK_GT;
  }
  return wtFlags;
}

/*
** Translate from TK_xx operator to WO_xx bitmask.
*/
static u16 operatorMask(int op){
  u16 c;
  assert( allowedOp(op) );
  if( op==TK_IN ){
    c = WO_IN;
  }else if( op==TK_ISNULL ){
    c = WO_ISNULL;
  }else if( op==TK_IS ){
    c = WO_IS;
  }else{
    assert( (WO_EQ<<(op-TK_EQ)) < 0x7fff );
    c = (u16)(WO_EQ<<(op-TK_EQ));
  }
  assert( op!=TK_ISNULL || c==WO_ISNULL );
  assert( op!=TK_IN || c==WO_IN );
  assert( op!=TK_EQ || c==WO_EQ );
  assert( op!=TK_LT || c==WO_LT );
  assert( op!=TK_LE || c==WO_LE );
  assert( op!=TK_GT || c==WO_GT );
  assert( op!=TK_GE || c==WO_GE );
  assert( op!=TK_IS || c==WO_IS );
  return c;
}


#ifndef SQLITE_OMIT_LIKE_OPTIMIZATION
/*
** Check to see if the given expression is a LIKE or GLOB operator that
** can be optimized using inequality constraints.  Return TRUE if it is
** so and false if not.
**
** In order for the operator to be optimizible, the RHS must be a string
** literal that does not begin with a wildcard.  The LHS must be a column
** that may only be NULL, a string, or a BLOB, never a number. (This means
** that virtual tables cannot participate in the LIKE optimization.)  The
** collating sequence for the column on the LHS must be appropriate for
** the operator.
*/
static int isLikeOrGlob(
  Parse *pParse,    /* Parsing and code generating context */
  Expr *pExpr,      /* Test this expression */
  Expr **ppPrefix,  /* Pointer to TK_STRING expression with pattern prefix */
  int *pisComplete, /* True if the only wildcard is % in the last character */
  int *pnoCase      /* True if uppercase is equivalent to lowercase */
){
  const u8 *z = 0;           /* String on RHS of LIKE operator */
  Expr *pRight, *pLeft;      /* Right and left size of LIKE operator */
  ExprList *pList;           /* List of operands to the LIKE operator */
  u8 c;                      /* One character in z[] */
  int cnt;                   /* Number of non-wildcard prefix characters */
  u8 wc[4];                  /* Wildcard characters */
  sqlite3 *db = pParse->db;  /* Database connection */
  sqlite3_value *pVal = 0;
  int op;                    /* Opcode of pRight */
  int rc;                    /* Result code to return */

  if( !sqlite3IsLikeFunction(db, pExpr, pnoCase, (char*)wc) ){
    return 0;
  }
#ifdef SQLITE_EBCDIC
  if( *pnoCase ) return 0;
#endif
  pList = pExpr->x.pList;
  pLeft = pList->a[1].pExpr;

  pRight = sqlite3ExprSkipCollate(pList->a[0].pExpr);
  op = pRight->op;
  if( op==TK_VARIABLE && (db->flags & SQLITE_EnableQPSG)==0 ){
    Vdbe *pReprepare = pParse->pReprepare;
    int iCol = pRight->iColumn;
    pVal = sqlite3VdbeGetBoundValue(pReprepare, iCol, SQLITE_AFF_BLOB);
    if( pVal && sqlite3_value_type(pVal)==SQLITE_TEXT ){
      z = sqlite3_value_text(pVal);
    }
    sqlite3VdbeSetVarmask(pParse->pVdbe, iCol);
    assert( pRight->op==TK_VARIABLE || pRight->op==TK_REGISTER );
  }else if( op==TK_STRING ){
    z = (u8*)pRight->u.zToken;
  }
  if( z ){

    /* Count the number of prefix characters prior to the first wildcard */
    cnt = 0;
    while( (c=z[cnt])!=0 && c!=wc[0] && c!=wc[1] && c!=wc[2] ){
      cnt++;
      if( c==wc[3] && z[cnt]!=0 ) cnt++;
    }

    /* The optimization is possible only if (1) the pattern does not begin
    ** with a wildcard and if (2) the non-wildcard prefix does not end with
    ** an (illegal 0xff) character, or (3) the pattern does not consist of
    ** a single escape character. The second condition is necessary so
    ** that we can increment the prefix key to find an upper bound for the
    ** range search. The third is because the caller assumes that the pattern
    ** consists of at least one character after all escapes have been
    ** removed.  */
    if( cnt!=0 && 255!=(u8)z[cnt-1] && (cnt>1 || z[0]!=wc[3]) ){
      Expr *pPrefix;

      /* A "complete" match if the pattern ends with "*" or "%" */
      *pisComplete = c==wc[0] && z[cnt+1]==0;

      /* Get the pattern prefix.  Remove all escapes from the prefix. */
      pPrefix = sqlite3Expr(db, TK_STRING, (char*)z);
      if( pPrefix ){
        int iFrom, iTo;
        char *zNew = pPrefix->u.zToken;
        zNew[cnt] = 0;
        for(iFrom=iTo=0; iFrom<cnt; iFrom++){
          if( zNew[iFrom]==wc[3] ) iFrom++;
          zNew[iTo++] = zNew[iFrom];
        }
        zNew[iTo] = 0;
        assert( iTo>0 );

        /* If the LHS is not an ordinary column with TEXT affinity, then the
        ** pattern prefix boundaries (both the start and end boundaries) must
        ** not look like a number.  Otherwise the pattern might be treated as
        ** a number, which will invalidate the LIKE optimization.
        **
        ** Getting this right has been a persistent source of bugs in the
        ** LIKE optimization.  See, for example:
        **    2018-09-10 https://sqlite.org/src/info/c94369cae9b561b1
        **    2019-05-02 https://sqlite.org/src/info/b043a54c3de54b28
        **    2019-06-10 https://sqlite.org/src/info/fd76310a5e843e07
        **    2019-06-14 https://sqlite.org/src/info/ce8717f0885af975
        **    2019-09-03 https://sqlite.org/src/info/0f0428096f17252a
        */
        if( pLeft->op!=TK_COLUMN 
         || sqlite3ExprAffinity(pLeft)!=SQLITE_AFF_TEXT 
         || IsVirtual(pLeft->y.pTab)  /* Value might be numeric */
        ){
          int isNum;
          double rDummy;
          isNum = sqlite3AtoF(zNew, &rDummy, iTo, SQLITE_UTF8);
          if( isNum<=0 ){
            if( iTo==1 && zNew[0]=='-' ){
              isNum = +1;
            }else{
              zNew[iTo-1]++;
              isNum = sqlite3AtoF(zNew, &rDummy, iTo, SQLITE_UTF8);
              zNew[iTo-1]--;
            }
          }
          if( isNum>0 ){
            sqlite3ExprDelete(db, pPrefix);
            sqlite3ValueFree(pVal);
            return 0;
          }
        }
      }
      *ppPrefix = pPrefix;

      /* If the RHS pattern is a bound parameter, make arrangements to
      ** reprepare the statement when that parameter is rebound */
      if( op==TK_VARIABLE ){
        Vdbe *v = pParse->pVdbe;
        sqlite3VdbeSetVarmask(v, pRight->iColumn);
        if( *pisComplete && pRight->u.zToken[1] ){
          /* If the rhs of the LIKE expression is a variable, and the current
          ** value of the variable means there is no need to invoke the LIKE
          ** function, then no OP_Variable will be added to the program.
          ** This causes problems for the sqlite3_bind_parameter_name()
          ** API. To work around them, add a dummy OP_Variable here.
          */ 
          int r1 = sqlite3GetTempReg(pParse);
          sqlite3ExprCodeTarget(pParse, pRight, r1);
          sqlite3VdbeChangeP3(v, sqlite3VdbeCurrentAddr(v)-1, 0);
          sqlite3ReleaseTempReg(pParse, r1);
        }
      }
    }else{
      z = 0;
    }
  }

  rc = (z!=0);
  sqlite3ValueFree(pVal);
  return rc;
}
#endif /* SQLITE_OMIT_LIKE_OPTIMIZATION */


#ifndef SQLITE_OMIT_VIRTUALTABLE
/*
** Check to see if the pExpr expression is a form that needs to be passed
** to the xBestIndex method of virtual tables.  Forms of interest include:
**
**          Expression                   Virtual Table Operator
**          -----------------------      ---------------------------------
**      1.  column MATCH expr            SQLITE_INDEX_CONSTRAINT_MATCH
**      2.  column GLOB expr             SQLITE_INDEX_CONSTRAINT_GLOB
**      3.  column LIKE expr             SQLITE_INDEX_CONSTRAINT_LIKE
**      4.  column REGEXP expr           SQLITE_INDEX_CONSTRAINT_REGEXP
**      5.  column != expr               SQLITE_INDEX_CONSTRAINT_NE
**      6.  expr != column               SQLITE_INDEX_CONSTRAINT_NE
**      7.  column IS NOT expr           SQLITE_INDEX_CONSTRAINT_ISNOT
**      8.  expr IS NOT column           SQLITE_INDEX_CONSTRAINT_ISNOT
**      9.  column IS NOT NULL           SQLITE_INDEX_CONSTRAINT_ISNOTNULL
**
** In every case, "column" must be a column of a virtual table.  If there
** is a match, set *ppLeft to the "column" expression, set *ppRight to the 
** "expr" expression (even though in forms (6) and (8) the column is on the
** right and the expression is on the left).  Also set *peOp2 to the
** appropriate virtual table operator.  The return value is 1 or 2 if there
** is a match.  The usual return is 1, but if the RHS is also a column
** of virtual table in forms (5) or (7) then return 2.
**
** If the expression matches none of the patterns above, return 0.
*/
static int isAuxiliaryVtabOperator(
  sqlite3 *db,                    /* Parsing context */
  Expr *pExpr,                    /* Test this expression */
  unsigned char *peOp2,           /* OUT: 0 for MATCH, or else an op2 value */
  Expr **ppLeft,                  /* Column expression to left of MATCH/op2 */
  Expr **ppRight                  /* Expression to left of MATCH/op2 */
){
  if( pExpr->op==TK_FUNCTION ){
    static const struct Op2 {
      const char *zOp;
      unsigned char eOp2;
    } aOp[] = {
      { "match",  SQLITE_INDEX_CONSTRAINT_MATCH },
      { "glob",   SQLITE_INDEX_CONSTRAINT_GLOB },
      { "like",   SQLITE_INDEX_CONSTRAINT_LIKE },
      { "regexp", SQLITE_INDEX_CONSTRAINT_REGEXP }
    };
    ExprList *pList;
    Expr *pCol;                     /* Column reference */
    int i;

    pList = pExpr->x.pList;
    if( pList==0 || pList->nExpr!=2 ){
      return 0;
    }

    /* Built-in operators MATCH, GLOB, LIKE, and REGEXP attach to a
    ** virtual table on their second argument, which is the same as
    ** the left-hand side operand in their in-fix form.
    **
    **       vtab_column MATCH expression
    **       MATCH(expression,vtab_column)
    */
    pCol = pList->a[1].pExpr;
    if( pCol->op==TK_COLUMN && IsVirtual(pCol->y.pTab) ){
      for(i=0; i<ArraySize(aOp); i++){
        if( sqlite3StrICmp(pExpr->u.zToken, aOp[i].zOp)==0 ){
          *peOp2 = aOp[i].eOp2;
          *ppRight = pList->a[0].pExpr;
          *ppLeft = pCol;
          return 1;
        }
      }
    }

    /* We can also match against the first column of overloaded
    ** functions where xFindFunction returns a value of at least
    ** SQLITE_INDEX_CONSTRAINT_FUNCTION.
    **
    **      OVERLOADED(vtab_column,expression)
    **
    ** Historically, xFindFunction expected to see lower-case function
    ** names.  But for this use case, xFindFunction is expected to deal
    ** with function names in an arbitrary case.
    */
    pCol = pList->a[0].pExpr;
    if( pCol->op==TK_COLUMN && IsVirtual(pCol->y.pTab) ){
      sqlite3_vtab *pVtab;
      sqlite3_module *pMod;
      void (*xNotUsed)(sqlite3_context*,int,sqlite3_value**);
      void *pNotUsed;
      pVtab = sqlite3GetVTable(db, pCol->y.pTab)->pVtab;
      assert( pVtab!=0 );
      assert( pVtab->pModule!=0 );
      pMod = (sqlite3_module *)pVtab->pModule;
      if( pMod->xFindFunction!=0 ){
        i = pMod->xFindFunction(pVtab,2, pExpr->u.zToken, &xNotUsed, &pNotUsed);
        if( i>=SQLITE_INDEX_CONSTRAINT_FUNCTION ){
          *peOp2 = i;
          *ppRight = pList->a[1].pExpr;
          *ppLeft = pCol;
          return 1;
        }
      }
    }
  }else if( pExpr->op==TK_NE || pExpr->op==TK_ISNOT || pExpr->op==TK_NOTNULL ){
    int res = 0;
    Expr *pLeft = pExpr->pLeft;
    Expr *pRight = pExpr->pRight;
    if( pLeft->op==TK_COLUMN && IsVirtual(pLeft->y.pTab) ){
      res++;
    }
    if( pRight && pRight->op==TK_COLUMN && IsVirtual(pRight->y.pTab) ){
      res++;
      SWAP(Expr*, pLeft, pRight);
    }
    *ppLeft = pLeft;
    *ppRight = pRight;
    if( pExpr->op==TK_NE ) *peOp2 = SQLITE_INDEX_CONSTRAINT_NE;
    if( pExpr->op==TK_ISNOT ) *peOp2 = SQLITE_INDEX_CONSTRAINT_ISNOT;
    if( pExpr->op==TK_NOTNULL ) *peOp2 = SQLITE_INDEX_CONSTRAINT_ISNOTNULL;
    return res;
  }
  return 0;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

/*
** If the pBase expression originated in the ON or USING clause of
** a join, then transfer the appropriate markings over to derived.
*/
static void transferJoinMarkings(Expr *pDerived, Expr *pBase){
  if( pDerived ){
    pDerived->flags |= pBase->flags & EP_FromJoin;
    pDerived->iRightJoinTable = pBase->iRightJoinTable;
  }
}

/*
** Mark term iChild as being a child of term iParent
*/
static void markTermAsChild(WhereClause *pWC, int iChild, int iParent){
  pWC->a[iChild].iParent = iParent;
  pWC->a[iChild].truthProb = pWC->a[iParent].truthProb;
  pWC->a[iParent].nChild++;
}

/*
** Return the N-th AND-connected subterm of pTerm.  Or if pTerm is not
** a conjunction, then return just pTerm when N==0.  If N is exceeds
** the number of available subterms, return NULL.
*/
static WhereTerm *whereNthSubterm(WhereTerm *pTerm, int N){
  if( pTerm->eOperator!=WO_AND ){
    return N==0 ? pTerm : 0;
  }
  if( N<pTerm->u.pAndInfo->wc.nTerm ){
    return &pTerm->u.pAndInfo->wc.a[N];
  }
  return 0;
}

/*
** Subterms pOne and pTwo are contained within WHERE clause pWC.  The
** two subterms are in disjunction - they are OR-ed together.
**
** If these two terms are both of the form:  "A op B" with the same
** A and B values but different operators and if the operators are
** compatible (if one is = and the other is <, for example) then
** add a new virtual AND term to pWC that is the combination of the
** two.
**
** Some examples:
**
**    x<y OR x=y    -->     x<=y
**    x=y OR x=y    -->     x=y
**    x<=y OR x<y   -->     x<=y
**
** The following is NOT generated:
**
**    x<y OR x>y    -->     x!=y     
*/
static void whereCombineDisjuncts(
  SrcList *pSrc,         /* the FROM clause */
  WhereClause *pWC,      /* The complete WHERE clause */
  WhereTerm *pOne,       /* First disjunct */
  WhereTerm *pTwo        /* Second disjunct */
){
  u16 eOp = pOne->eOperator | pTwo->eOperator;
  sqlite3 *db;           /* Database connection (for malloc) */
  Expr *pNew;            /* New virtual expression */
  int op;                /* Operator for the combined expression */
  int idxNew;            /* Index in pWC of the next virtual term */

  if( (pOne->eOperator & (WO_EQ|WO_LT|WO_LE|WO_GT|WO_GE))==0 ) return;
  if( (pTwo->eOperator & (WO_EQ|WO_LT|WO_LE|WO_GT|WO_GE))==0 ) return;
  if( (eOp & (WO_EQ|WO_LT|WO_LE))!=eOp
   && (eOp & (WO_EQ|WO_GT|WO_GE))!=eOp ) return;
  assert( pOne->pExpr->pLeft!=0 && pOne->pExpr->pRight!=0 );
  assert( pTwo->pExpr->pLeft!=0 && pTwo->pExpr->pRight!=0 );
  if( sqlite3ExprCompare(0,pOne->pExpr->pLeft, pTwo->pExpr->pLeft, -1) ) return;
  if( sqlite3ExprCompare(0,pOne->pExpr->pRight, pTwo->pExpr->pRight,-1) )return;
  /* If we reach this point, it means the two subterms can be combined */
  if( (eOp & (eOp-1))!=0 ){
    if( eOp & (WO_LT|WO_LE) ){
      eOp = WO_LE;
    }else{
      assert( eOp & (WO_GT|WO_GE) );
      eOp = WO_GE;
    }
  }
  db = pWC->pWInfo->pParse->db;
  pNew = sqlite3ExprDup(db, pOne->pExpr, 0);
  if( pNew==0 ) return;
  for(op=TK_EQ; eOp!=(WO_EQ<<(op-TK_EQ)); op++){ assert( op<TK_GE ); }
  pNew->op = op;
  idxNew = whereClauseInsert(pWC, pNew, TERM_VIRTUAL|TERM_DYNAMIC);
  exprAnalyze(pSrc, pWC, idxNew);
}

#if !defined(SQLITE_OMIT_OR_OPTIMIZATION) && !defined(SQLITE_OMIT_SUBQUERY)
/*
** Analyze a term that consists of two or more OR-connected
** subterms.  So in:
**
**     ... WHERE  (a=5) AND (b=7 OR c=9 OR d=13) AND (d=13)
**                          ^^^^^^^^^^^^^^^^^^^^
**
** This routine analyzes terms such as the middle term in the above example.
** A WhereOrTerm object is computed and attached to the term under
** analysis, regardless of the outcome of the analysis.  Hence:
**
**     WhereTerm.wtFlags   |=  TERM_ORINFO
**     WhereTerm.u.pOrInfo  =  a dynamically allocated WhereOrTerm object
**
** The term being analyzed must have two or more of OR-connected subterms.
** A single subterm might be a set of AND-connected sub-subterms.
** Examples of terms under analysis:
**
**     (A)     t1.x=t2.y OR t1.x=t2.z OR t1.y=15 OR t1.z=t3.a+5
**     (B)     x=expr1 OR expr2=x OR x=expr3
**     (C)     t1.x=t2.y OR (t1.x=t2.z AND t1.y=15)
**     (D)     x=expr1 OR (y>11 AND y<22 AND z LIKE '*hello*')
**     (E)     (p.a=1 AND q.b=2 AND r.c=3) OR (p.x=4 AND q.y=5 AND r.z=6)
**     (F)     x>A OR (x=A AND y>=B)
**
** CASE 1:
**
** If all subterms are of the form T.C=expr for some single column of C and
** a single table T (as shown in example B above) then create a new virtual
** term that is an equivalent IN expression.  In other words, if the term
** being analyzed is:
**
**      x = expr1  OR  expr2 = x  OR  x = expr3
**
** then create a new virtual term like this:
**
**      x IN (expr1,expr2,expr3)
**
** CASE 2:
**
** If there are exactly two disjuncts and one side has x>A and the other side
** has x=A (for the same x and A) then add a new virtual conjunct term to the
** WHERE clause of the form "x>=A".  Example:
**
**      x>A OR (x=A AND y>B)    adds:    x>=A
**
** The added conjunct can sometimes be helpful in query planning.
**
** CASE 3:
**
** If all subterms are indexable by a single table T, then set
**
**     WhereTerm.eOperator              =  WO_OR
**     WhereTerm.u.pOrInfo->indexable  |=  the cursor number for table T
**
** A subterm is "indexable" if it is of the form
** "T.C <op> <expr>" where C is any column of table T and 
** <op> is one of "=", "<", "<=", ">", ">=", "IS NULL", or "IN".
** A subterm is also indexable if it is an AND of two or more
** subsubterms at least one of which is indexable.  Indexable AND 
** subterms have their eOperator set to WO_AND and they have
** u.pAndInfo set to a dynamically allocated WhereAndTerm object.
**
** From another point of view, "indexable" means that the subterm could
** potentially be used with an index if an appropriate index exists.
** This analysis does not consider whether or not the index exists; that
** is decided elsewhere.  This analysis only looks at whether subterms
** appropriate for indexing exist.
**
** All examples A through E above satisfy case 3.  But if a term
** also satisfies case 1 (such as B) we know that the optimizer will
** always prefer case 1, so in that case we pretend that case 3 is not
** satisfied.
**
** It might be the case that multiple tables are indexable.  For example,
** (E) above is indexable on tables P, Q, and R.
**
** Terms that satisfy case 3 are candidates for lookup by using
** separate indices to find rowids for each subterm and composing
** the union of all rowids using a RowSet object.  This is similar
** to "bitmap indices" in other database engines.
**
** OTHERWISE:
**
** If none of cases 1, 2, or 3 apply, then leave the eOperator set to
** zero.  This term is not useful for search.
*/
static void exprAnalyzeOrTerm(
  SrcList *pSrc,            /* the FROM clause */
  WhereClause *pWC,         /* the complete WHERE clause */
  int idxTerm               /* Index of the OR-term to be analyzed */
){
  WhereInfo *pWInfo = pWC->pWInfo;        /* WHERE clause processing context */
  Parse *pParse = pWInfo->pParse;         /* Parser context */
  sqlite3 *db = pParse->db;               /* Database connection */
  WhereTerm *pTerm = &pWC->a[idxTerm];    /* The term to be analyzed */
  Expr *pExpr = pTerm->pExpr;             /* The expression of the term */
  int i;                                  /* Loop counters */
  WhereClause *pOrWc;       /* Breakup of pTerm into subterms */
  WhereTerm *pOrTerm;       /* A Sub-term within the pOrWc */
  WhereOrInfo *pOrInfo;     /* Additional information associated with pTerm */
  Bitmask chngToIN;         /* Tables that might satisfy case 1 */
  Bitmask indexable;        /* Tables that are indexable, satisfying case 2 */

  /*
  ** Break the OR clause into its separate subterms.  The subterms are
  ** stored in a WhereClause structure containing within the WhereOrInfo
  ** object that is attached to the original OR clause term.
  */
  assert( (pTerm->wtFlags & (TERM_DYNAMIC|TERM_ORINFO|TERM_ANDINFO))==0 );
  assert( pExpr->op==TK_OR );
  pTerm->u.pOrInfo = pOrInfo = sqlite3DbMallocZero(db, sizeof(*pOrInfo));
  if( pOrInfo==0 ) return;
  pTerm->wtFlags |= TERM_ORINFO;
  pOrWc = &pOrInfo->wc;
  memset(pOrWc->aStatic, 0, sizeof(pOrWc->aStatic));
  sqlite3WhereClauseInit(pOrWc, pWInfo);
  sqlite3WhereSplit(pOrWc, pExpr, TK_OR);
  sqlite3WhereExprAnalyze(pSrc, pOrWc);
  if( db->mallocFailed ) return;
  assert( pOrWc->nTerm>=2 );

  /*
  ** Compute the set of tables that might satisfy cases 1 or 3.
  */
  indexable = ~(Bitmask)0;
  chngToIN = ~(Bitmask)0;
  for(i=pOrWc->nTerm-1, pOrTerm=pOrWc->a; i>=0 && indexable; i--, pOrTerm++){
    if( (pOrTerm->eOperator & WO_SINGLE)==0 ){
      WhereAndInfo *pAndInfo;
      assert( (pOrTerm->wtFlags & (TERM_ANDINFO|TERM_ORINFO))==0 );
      chngToIN = 0;
      pAndInfo = sqlite3DbMallocRawNN(db, sizeof(*pAndInfo));
      if( pAndInfo ){
        WhereClause *pAndWC;
        WhereTerm *pAndTerm;
        int j;
        Bitmask b = 0;
        pOrTerm->u.pAndInfo = pAndInfo;
        pOrTerm->wtFlags |= TERM_ANDINFO;
        pOrTerm->eOperator = WO_AND;
        pAndWC = &pAndInfo->wc;
        memset(pAndWC->aStatic, 0, sizeof(pAndWC->aStatic));
        sqlite3WhereClauseInit(pAndWC, pWC->pWInfo);
        sqlite3WhereSplit(pAndWC, pOrTerm->pExpr, TK_AND);
        sqlite3WhereExprAnalyze(pSrc, pAndWC);
        pAndWC->pOuter = pWC;
        if( !db->mallocFailed ){
          for(j=0, pAndTerm=pAndWC->a; j<pAndWC->nTerm; j++, pAndTerm++){
            assert( pAndTerm->pExpr );
            if( allowedOp(pAndTerm->pExpr->op) 
             || pAndTerm->eOperator==WO_AUX
            ){
              b |= sqlite3WhereGetMask(&pWInfo->sMaskSet, pAndTerm->leftCursor);
            }
          }
        }
        indexable &= b;
      }
    }else if( pOrTerm->wtFlags & TERM_COPIED ){
      /* Skip this term for now.  We revisit it when we process the
      ** corresponding TERM_VIRTUAL term */
    }else{
      Bitmask b;
      b = sqlite3WhereGetMask(&pWInfo->sMaskSet, pOrTerm->leftCursor);
      if( pOrTerm->wtFlags & TERM_VIRTUAL ){
        WhereTerm *pOther = &pOrWc->a[pOrTerm->iParent];
        b |= sqlite3WhereGetMask(&pWInfo->sMaskSet, pOther->leftCursor);
      }
      indexable &= b;
      if( (pOrTerm->eOperator & WO_EQ)==0 ){
        chngToIN = 0;
      }else{
        chngToIN &= b;
      }
    }
  }

  /*
  ** Record the set of tables that satisfy case 3.  The set might be
  ** empty.
  */
  pOrInfo->indexable = indexable;
  if( indexable ){
    pTerm->eOperator = WO_OR;
    pWC->hasOr = 1;
  }else{
    pTerm->eOperator = WO_OR;
  }

  /* For a two-way OR, attempt to implementation case 2.
  */
  if( indexable && pOrWc->nTerm==2 ){
    int iOne = 0;
    WhereTerm *pOne;
    while( (pOne = whereNthSubterm(&pOrWc->a[0],iOne++))!=0 ){
      int iTwo = 0;
      WhereTerm *pTwo;
      while( (pTwo = whereNthSubterm(&pOrWc->a[1],iTwo++))!=0 ){
        whereCombineDisjuncts(pSrc, pWC, pOne, pTwo);
      }
    }
  }

  /*
  ** chngToIN holds a set of tables that *might* satisfy case 1.  But
  ** we have to do some additional checking to see if case 1 really
  ** is satisfied.
  **
  ** chngToIN will hold either 0, 1, or 2 bits.  The 0-bit case means
  ** that there is no possibility of transforming the OR clause into an
  ** IN operator because one or more terms in the OR clause contain
  ** something other than == on a column in the single table.  The 1-bit
  ** case means that every term of the OR clause is of the form
  ** "table.column=expr" for some single table.  The one bit that is set
  ** will correspond to the common table.  We still need to check to make
  ** sure the same column is used on all terms.  The 2-bit case is when
  ** the all terms are of the form "table1.column=table2.column".  It
  ** might be possible to form an IN operator with either table1.column
  ** or table2.column as the LHS if either is common to every term of
  ** the OR clause.
  **
  ** Note that terms of the form "table.column1=table.column2" (the
  ** same table on both sizes of the ==) cannot be optimized.
  */
  if( chngToIN ){
    int okToChngToIN = 0;     /* True if the conversion to IN is valid */
    int iColumn = -1;         /* Column index on lhs of IN operator */
    int iCursor = -1;         /* Table cursor common to all terms */
    int j = 0;                /* Loop counter */

    /* Search for a table and column that appears on one side or the
    ** other of the == operator in every subterm.  That table and column
    ** will be recorded in iCursor and iColumn.  There might not be any
    ** such table and column.  Set okToChngToIN if an appropriate table
    ** and column is found but leave okToChngToIN false if not found.
    */
    for(j=0; j<2 && !okToChngToIN; j++){
      Expr *pLeft = 0;
      pOrTerm = pOrWc->a;
      for(i=pOrWc->nTerm-1; i>=0; i--, pOrTerm++){
        assert( pOrTerm->eOperator & WO_EQ );
        pOrTerm->wtFlags &= ~TERM_OR_OK;
        if( pOrTerm->leftCursor==iCursor ){
          /* This is the 2-bit case and we are on the second iteration and
          ** current term is from the first iteration.  So skip this term. */
          assert( j==1 );
          continue;
        }
        if( (chngToIN & sqlite3WhereGetMask(&pWInfo->sMaskSet,
                                            pOrTerm->leftCursor))==0 ){
          /* This term must be of the form t1.a==t2.b where t2 is in the
          ** chngToIN set but t1 is not.  This term will be either preceded
          ** or follwed by an inverted copy (t2.b==t1.a).  Skip this term 
          ** and use its inversion. */
          testcase( pOrTerm->wtFlags & TERM_COPIED );
          testcase( pOrTerm->wtFlags & TERM_VIRTUAL );
          assert( pOrTerm->wtFlags & (TERM_COPIED|TERM_VIRTUAL) );
          continue;
        }
        iColumn = pOrTerm->u.leftColumn;
        iCursor = pOrTerm->leftCursor;
        pLeft = pOrTerm->pExpr->pLeft;
        break;
      }
      if( i<0 ){
        /* No candidate table+column was found.  This can only occur
        ** on the second iteration */
        assert( j==1 );
        assert( IsPowerOfTwo(chngToIN) );
        assert( chngToIN==sqlite3WhereGetMask(&pWInfo->sMaskSet, iCursor) );
        break;
      }
      testcase( j==1 );

      /* We have found a candidate table and column.  Check to see if that
      ** table and column is common to every term in the OR clause */
      okToChngToIN = 1;
      for(; i>=0 && okToChngToIN; i--, pOrTerm++){
        assert( pOrTerm->eOperator & WO_EQ );
        if( pOrTerm->leftCursor!=iCursor ){
          pOrTerm->wtFlags &= ~TERM_OR_OK;
        }else if( pOrTerm->u.leftColumn!=iColumn || (iColumn==XN_EXPR 
               && sqlite3ExprCompare(pParse, pOrTerm->pExpr->pLeft, pLeft, -1)
        )){
          okToChngToIN = 0;
        }else{
          int affLeft, affRight;
          /* If the right-hand side is also a column, then the affinities
          ** of both right and left sides must be such that no type
          ** conversions are required on the right.  (Ticket #2249)
          */
          affRight = sqlite3ExprAffinity(pOrTerm->pExpr->pRight);
          affLeft = sqlite3ExprAffinity(pOrTerm->pExpr->pLeft);
          if( affRight!=0 && affRight!=affLeft ){
            okToChngToIN = 0;
          }else{
            pOrTerm->wtFlags |= TERM_OR_OK;
          }
        }
      }
    }

    /* At this point, okToChngToIN is true if original pTerm satisfies
    ** case 1.  In that case, construct a new virtual term that is 
    ** pTerm converted into an IN operator.
    */
    if( okToChngToIN ){
      Expr *pDup;            /* A transient duplicate expression */
      ExprList *pList = 0;   /* The RHS of the IN operator */
      Expr *pLeft = 0;       /* The LHS of the IN operator */
      Expr *pNew;            /* The complete IN operator */

      for(i=pOrWc->nTerm-1, pOrTerm=pOrWc->a; i>=0; i--, pOrTerm++){
        if( (pOrTerm->wtFlags & TERM_OR_OK)==0 ) continue;
        assert( pOrTerm->eOperator & WO_EQ );
        assert( pOrTerm->leftCursor==iCursor );
        assert( pOrTerm->u.leftColumn==iColumn );
        pDup = sqlite3ExprDup(db, pOrTerm->pExpr->pRight, 0);
        pList = sqlite3ExprListAppend(pWInfo->pParse, pList, pDup);
        pLeft = pOrTerm->pExpr->pLeft;
      }
      assert( pLeft!=0 );
      pDup = sqlite3ExprDup(db, pLeft, 0);
      pNew = sqlite3PExpr(pParse, TK_IN, pDup, 0);
      if( pNew ){
        int idxNew;
        transferJoinMarkings(pNew, pExpr);
        assert( !ExprHasProperty(pNew, EP_xIsSelect) );
        pNew->x.pList = pList;
        idxNew = whereClauseInsert(pWC, pNew, TERM_VIRTUAL|TERM_DYNAMIC);
        testcase( idxNew==0 );
        exprAnalyze(pSrc, pWC, idxNew);
        /* pTerm = &pWC->a[idxTerm]; // would be needed if pTerm where used again */
        markTermAsChild(pWC, idxNew, idxTerm);
      }else{
        sqlite3ExprListDelete(db, pList);
      }
    }
  }
}
#endif /* !SQLITE_OMIT_OR_OPTIMIZATION && !SQLITE_OMIT_SUBQUERY */

/*
** We already know that pExpr is a binary operator where both operands are
** column references.  This routine checks to see if pExpr is an equivalence
** relation:
**   1.  The SQLITE_Transitive optimization must be enabled
**   2.  Must be either an == or an IS operator
**   3.  Not originating in the ON clause of an OUTER JOIN
**   4.  The affinities of A and B must be compatible
**   5a. Both operands use the same collating sequence OR
**   5b. The overall collating sequence is BINARY
** If this routine returns TRUE, that means that the RHS can be substituted
** for the LHS anyplace else in the WHERE clause where the LHS column occurs.
** This is an optimization.  No harm comes from returning 0.  But if 1 is
** returned when it should not be, then incorrect answers might result.
*/
static int termIsEquivalence(Parse *pParse, Expr *pExpr){
  char aff1, aff2;
  CollSeq *pColl;
  if( !OptimizationEnabled(pParse->db, SQLITE_Transitive) ) return 0;
  if( pExpr->op!=TK_EQ && pExpr->op!=TK_IS ) return 0;
  if( ExprHasProperty(pExpr, EP_FromJoin) ) return 0;
  aff1 = sqlite3ExprAffinity(pExpr->pLeft);
  aff2 = sqlite3ExprAffinity(pExpr->pRight);
  if( aff1!=aff2
   && (!sqlite3IsNumericAffinity(aff1) || !sqlite3IsNumericAffinity(aff2))
  ){
    return 0;
  }
  pColl = sqlite3BinaryCompareCollSeq(pParse, pExpr->pLeft, pExpr->pRight);
  if( sqlite3IsBinary(pColl) ) return 1;
  return sqlite3ExprCollSeqMatch(pParse, pExpr->pLeft, pExpr->pRight);
}

/*
** Recursively walk the expressions of a SELECT statement and generate
** a bitmask indicating which tables are used in that expression
** tree.
*/
static Bitmask exprSelectUsage(WhereMaskSet *pMaskSet, Select *pS){
  Bitmask mask = 0;
  while( pS ){
    SrcList *pSrc = pS->pSrc;
    mask |= sqlite3WhereExprListUsage(pMaskSet, pS->pEList);
    mask |= sqlite3WhereExprListUsage(pMaskSet, pS->pGroupBy);
    mask |= sqlite3WhereExprListUsage(pMaskSet, pS->pOrderBy);
    mask |= sqlite3WhereExprUsage(pMaskSet, pS->pWhere);
    mask |= sqlite3WhereExprUsage(pMaskSet, pS->pHaving);
    if( ALWAYS(pSrc!=0) ){
      int i;
      for(i=0; i<pSrc->nSrc; i++){
        mask |= exprSelectUsage(pMaskSet, pSrc->a[i].pSelect);
        mask |= sqlite3WhereExprUsage(pMaskSet, pSrc->a[i].pOn);
        if( pSrc->a[i].fg.isTabFunc ){
          mask |= sqlite3WhereExprListUsage(pMaskSet, pSrc->a[i].u1.pFuncArg);
        }
      }
    }
    pS = pS->pPrior;
  }
  return mask;
}

/*
** Expression pExpr is one operand of a comparison operator that might
** be useful for indexing.  This routine checks to see if pExpr appears
** in any index.  Return TRUE (1) if pExpr is an indexed term and return
** FALSE (0) if not.  If TRUE is returned, also set aiCurCol[0] to the cursor
** number of the table that is indexed and aiCurCol[1] to the column number
** of the column that is indexed, or XN_EXPR (-2) if an expression is being
** indexed.
**
** If pExpr is a TK_COLUMN column reference, then this routine always returns
** true even if that particular column is not indexed, because the column
** might be added to an automatic index later.
*/
static SQLITE_NOINLINE int exprMightBeIndexed2(
  SrcList *pFrom,        /* The FROM clause */
  Bitmask mPrereq,       /* Bitmask of FROM clause terms referenced by pExpr */
  int *aiCurCol,         /* Write the referenced table cursor and column here */
  Expr *pExpr            /* An operand of a comparison operator */
){
  Index *pIdx;
  int i;
  int iCur;
  for(i=0; mPrereq>1; i++, mPrereq>>=1){}
  iCur = pFrom->a[i].iCursor;
  for(pIdx=pFrom->a[i].pTab->pIndex; pIdx; pIdx=pIdx->pNext){
    if( pIdx->aColExpr==0 ) continue;
    for(i=0; i<pIdx->nKeyCol; i++){
      if( pIdx->aiColumn[i]!=XN_EXPR ) continue;
      if( sqlite3ExprCompareSkip(pExpr, pIdx->aColExpr->a[i].pExpr, iCur)==0 ){
        aiCurCol[0] = iCur;
        aiCurCol[1] = XN_EXPR;
        return 1;
      }
    }
  }
  return 0;
}
static int exprMightBeIndexed(
  SrcList *pFrom,        /* The FROM clause */
  Bitmask mPrereq,       /* Bitmask of FROM clause terms referenced by pExpr */
  int *aiCurCol,         /* Write the referenced table cursor & column here */
  Expr *pExpr,           /* An operand of a comparison operator */
  int op                 /* The specific comparison operator */
){
  /* If this expression is a vector to the left or right of a 
  ** inequality constraint (>, <, >= or <=), perform the processing 
  ** on the first element of the vector.  */
  assert( TK_GT+1==TK_LE && TK_GT+2==TK_LT && TK_GT+3==TK_GE );
  assert( TK_IS<TK_GE && TK_ISNULL<TK_GE && TK_IN<TK_GE );
  assert( op<=TK_GE );
  if( pExpr->op==TK_VECTOR && (op>=TK_GT && ALWAYS(op<=TK_GE)) ){
    pExpr = pExpr->x.pList->a[0].pExpr;
  }

  if( pExpr->op==TK_COLUMN ){
    aiCurCol[0] = pExpr->iTable;
    aiCurCol[1] = pExpr->iColumn;
    return 1;
  }
  if( mPrereq==0 ) return 0;                 /* No table references */
  if( (mPrereq&(mPrereq-1))!=0 ) return 0;   /* Refs more than one table */
  return exprMightBeIndexed2(pFrom,mPrereq,aiCurCol,pExpr);
}

/*
** The input to this routine is an WhereTerm structure with only the
** "pExpr" field filled in.  The job of this routine is to analyze the
** subexpression and populate all the other fields of the WhereTerm
** structure.
**
** If the expression is of the form "<expr> <op> X" it gets commuted
** to the standard form of "X <op> <expr>".
**
** If the expression is of the form "X <op> Y" where both X and Y are
** columns, then the original expression is unchanged and a new virtual
** term of the form "Y <op> X" is added to the WHERE clause and
** analyzed separately.  The original term is marked with TERM_COPIED
** and the new term is marked with TERM_DYNAMIC (because it's pExpr
** needs to be freed with the WhereClause) and TERM_VIRTUAL (because it
** is a commuted copy of a prior term.)  The original term has nChild=1
** and the copy has idxParent set to the index of the original term.
*/
static void exprAnalyze(
  SrcList *pSrc,            /* the FROM clause */
  WhereClause *pWC,         /* the WHERE clause */
  int idxTerm               /* Index of the term to be analyzed */
){
  WhereInfo *pWInfo = pWC->pWInfo; /* WHERE clause processing context */
  WhereTerm *pTerm;                /* The term to be analyzed */
  WhereMaskSet *pMaskSet;          /* Set of table index masks */
  Expr *pExpr;                     /* The expression to be analyzed */
  Bitmask prereqLeft;              /* Prerequesites of the pExpr->pLeft */
  Bitmask prereqAll;               /* Prerequesites of pExpr */
  Bitmask extraRight = 0;          /* Extra dependencies on LEFT JOIN */
  Expr *pStr1 = 0;                 /* RHS of LIKE/GLOB operator */
  int isComplete = 0;              /* RHS of LIKE/GLOB ends with wildcard */
  int noCase = 0;                  /* uppercase equivalent to lowercase */
  int op;                          /* Top-level operator.  pExpr->op */
  Parse *pParse = pWInfo->pParse;  /* Parsing context */
  sqlite3 *db = pParse->db;        /* Database connection */
  unsigned char eOp2 = 0;          /* op2 value for LIKE/REGEXP/GLOB */
  int nLeft;                       /* Number of elements on left side vector */

  if( db->mallocFailed ){
    return;
  }
  pTerm = &pWC->a[idxTerm];
  pMaskSet = &pWInfo->sMaskSet;
  pExpr = pTerm->pExpr;
  assert( pExpr->op!=TK_AS && pExpr->op!=TK_COLLATE );
  prereqLeft = sqlite3WhereExprUsage(pMaskSet, pExpr->pLeft);
  op = pExpr->op;
  if( op==TK_IN ){
    assert( pExpr->pRight==0 );
    if( sqlite3ExprCheckIN(pParse, pExpr) ) return;
    if( ExprHasProperty(pExpr, EP_xIsSelect) ){
      pTerm->prereqRight = exprSelectUsage(pMaskSet, pExpr->x.pSelect);
    }else{
      pTerm->prereqRight = sqlite3WhereExprListUsage(pMaskSet, pExpr->x.pList);
    }
  }else if( op==TK_ISNULL ){
    pTerm->prereqRight = 0;
  }else{
    pTerm->prereqRight = sqlite3WhereExprUsage(pMaskSet, pExpr->pRight);
  }
  pMaskSet->bVarSelect = 0;
  prereqAll = sqlite3WhereExprUsageNN(pMaskSet, pExpr);
  if( pMaskSet->bVarSelect ) pTerm->wtFlags |= TERM_VARSELECT;
  if( ExprHasProperty(pExpr, EP_FromJoin) ){
    Bitmask x = sqlite3WhereGetMask(pMaskSet, pExpr->iRightJoinTable);
    prereqAll |= x;
    extraRight = x-1;  /* ON clause terms may not be used with an index
                       ** on left table of a LEFT JOIN.  Ticket #3015 */
    if( (prereqAll>>1)>=x ){
      sqlite3ErrorMsg(pParse, "ON clause references tables to its right");
      return;
    }
  }
  pTerm->prereqAll = prereqAll;
  pTerm->leftCursor = -1;
  pTerm->iParent = -1;
  pTerm->eOperator = 0;
  if( allowedOp(op) ){
    int aiCurCol[2];
    Expr *pLeft = sqlite3ExprSkipCollate(pExpr->pLeft);
    Expr *pRight = sqlite3ExprSkipCollate(pExpr->pRight);
    u16 opMask = (pTerm->prereqRight & prereqLeft)==0 ? WO_ALL : WO_EQUIV;

    if( pTerm->iField>0 ){
      assert( op==TK_IN );
      assert( pLeft->op==TK_VECTOR );
      pLeft = pLeft->x.pList->a[pTerm->iField-1].pExpr;
    }

    if( exprMightBeIndexed(pSrc, prereqLeft, aiCurCol, pLeft, op) ){
      pTerm->leftCursor = aiCurCol[0];
      pTerm->u.leftColumn = aiCurCol[1];
      pTerm->eOperator = operatorMask(op) & opMask;
    }
    if( op==TK_IS ) pTerm->wtFlags |= TERM_IS;
    if( pRight 
     && exprMightBeIndexed(pSrc, pTerm->prereqRight, aiCurCol, pRight, op)
    ){
      WhereTerm *pNew;
      Expr *pDup;
      u16 eExtraOp = 0;        /* Extra bits for pNew->eOperator */
      assert( pTerm->iField==0 );
      if( pTerm->leftCursor>=0 ){
        int idxNew;
        pDup = sqlite3ExprDup(db, pExpr, 0);
        if( db->mallocFailed ){
          sqlite3ExprDelete(db, pDup);
          return;
        }
        idxNew = whereClauseInsert(pWC, pDup, TERM_VIRTUAL|TERM_DYNAMIC);
        if( idxNew==0 ) return;
        pNew = &pWC->a[idxNew];
        markTermAsChild(pWC, idxNew, idxTerm);
        if( op==TK_IS ) pNew->wtFlags |= TERM_IS;
        pTerm = &pWC->a[idxTerm];
        pTerm->wtFlags |= TERM_COPIED;

        if( termIsEquivalence(pParse, pDup) ){
          pTerm->eOperator |= WO_EQUIV;
          eExtraOp = WO_EQUIV;
        }
      }else{
        pDup = pExpr;
        pNew = pTerm;
      }
      pNew->wtFlags |= exprCommute(pParse, pDup);
      pNew->leftCursor = aiCurCol[0];
      pNew->u.leftColumn = aiCurCol[1];
      testcase( (prereqLeft | extraRight) != prereqLeft );
      pNew->prereqRight = prereqLeft | extraRight;
      pNew->prereqAll = prereqAll;
      pNew->eOperator = (operatorMask(pDup->op) + eExtraOp) & opMask;
    }
  }

#ifndef SQLITE_OMIT_BETWEEN_OPTIMIZATION
  /* If a term is the BETWEEN operator, create two new virtual terms
  ** that define the range that the BETWEEN implements.  For example:
  **
  **      a BETWEEN b AND c
  **
  ** is converted into:
  **
  **      (a BETWEEN b AND c) AND (a>=b) AND (a<=c)
  **
  ** The two new terms are added onto the end of the WhereClause object.
  ** The new terms are "dynamic" and are children of the original BETWEEN
  ** term.  That means that if the BETWEEN term is coded, the children are
  ** skipped.  Or, if the children are satisfied by an index, the original
  ** BETWEEN term is skipped.
  */
  else if( pExpr->op==TK_BETWEEN && pWC->op==TK_AND ){
    ExprList *pList = pExpr->x.pList;
    int i;
    static const u8 ops[] = {TK_GE, TK_LE};
    assert( pList!=0 );
    assert( pList->nExpr==2 );
    for(i=0; i<2; i++){
      Expr *pNewExpr;
      int idxNew;
      pNewExpr = sqlite3PExpr(pParse, ops[i], 
                             sqlite3ExprDup(db, pExpr->pLeft, 0),
                             sqlite3ExprDup(db, pList->a[i].pExpr, 0));
      transferJoinMarkings(pNewExpr, pExpr);
      idxNew = whereClauseInsert(pWC, pNewExpr, TERM_VIRTUAL|TERM_DYNAMIC);
      testcase( idxNew==0 );
      exprAnalyze(pSrc, pWC, idxNew);
      pTerm = &pWC->a[idxTerm];
      markTermAsChild(pWC, idxNew, idxTerm);
    }
  }
#endif /* SQLITE_OMIT_BETWEEN_OPTIMIZATION */

#if !defined(SQLITE_OMIT_OR_OPTIMIZATION) && !defined(SQLITE_OMIT_SUBQUERY)
  /* Analyze a term that is composed of two or more subterms connected by
  ** an OR operator.
  */
  else if( pExpr->op==TK_OR ){
    assert( pWC->op==TK_AND );
    exprAnalyzeOrTerm(pSrc, pWC, idxTerm);
    pTerm = &pWC->a[idxTerm];
  }
#endif /* SQLITE_OMIT_OR_OPTIMIZATION */

#ifndef SQLITE_OMIT_LIKE_OPTIMIZATION
  /* Add constraints to reduce the search space on a LIKE or GLOB
  ** operator.
  **
  ** A like pattern of the form "x LIKE 'aBc%'" is changed into constraints
  **
  **          x>='ABC' AND x<'abd' AND x LIKE 'aBc%'
  **
  ** The last character of the prefix "abc" is incremented to form the
  ** termination condition "abd".  If case is not significant (the default
  ** for LIKE) then the lower-bound is made all uppercase and the upper-
  ** bound is made all lowercase so that the bounds also work when comparing
  ** BLOBs.
  */
  if( pWC->op==TK_AND 
   && isLikeOrGlob(pParse, pExpr, &pStr1, &isComplete, &noCase)
  ){
    Expr *pLeft;       /* LHS of LIKE/GLOB operator */
    Expr *pStr2;       /* Copy of pStr1 - RHS of LIKE/GLOB operator */
    Expr *pNewExpr1;
    Expr *pNewExpr2;
    int idxNew1;
    int idxNew2;
    const char *zCollSeqName;     /* Name of collating sequence */
    const u16 wtFlags = TERM_LIKEOPT | TERM_VIRTUAL | TERM_DYNAMIC;

    pLeft = pExpr->x.pList->a[1].pExpr;
    pStr2 = sqlite3ExprDup(db, pStr1, 0);

    /* Convert the lower bound to upper-case and the upper bound to
    ** lower-case (upper-case is less than lower-case in ASCII) so that
    ** the range constraints also work for BLOBs
    */
    if( noCase && !pParse->db->mallocFailed ){
      int i;
      char c;
      pTerm->wtFlags |= TERM_LIKE;
      for(i=0; (c = pStr1->u.zToken[i])!=0; i++){
        pStr1->u.zToken[i] = sqlite3Toupper(c);
        pStr2->u.zToken[i] = sqlite3Tolower(c);
      }
    }

    if( !db->mallocFailed ){
      u8 c, *pC;       /* Last character before the first wildcard */
      pC = (u8*)&pStr2->u.zToken[sqlite3Strlen30(pStr2->u.zToken)-1];
      c = *pC;
      if( noCase ){
        /* The point is to increment the last character before the first
        ** wildcard.  But if we increment '@', that will push it into the
        ** alphabetic range where case conversions will mess up the 
        ** inequality.  To avoid this, make sure to also run the full
        ** LIKE on all candidate expressions by clearing the isComplete flag
        */
        if( c=='A'-1 ) isComplete = 0;
        c = sqlite3UpperToLower[c];
      }
      *pC = c + 1;
    }
    zCollSeqName = noCase ? "NOCASE" : sqlite3StrBINARY;
    pNewExpr1 = sqlite3ExprDup(db, pLeft, 0);
    pNewExpr1 = sqlite3PExpr(pParse, TK_GE,
           sqlite3ExprAddCollateString(pParse,pNewExpr1,zCollSeqName),
           pStr1);
    transferJoinMarkings(pNewExpr1, pExpr);
    idxNew1 = whereClauseInsert(pWC, pNewExpr1, wtFlags);
    testcase( idxNew1==0 );
    exprAnalyze(pSrc, pWC, idxNew1);
    pNewExpr2 = sqlite3ExprDup(db, pLeft, 0);
    pNewExpr2 = sqlite3PExpr(pParse, TK_LT,
           sqlite3ExprAddCollateString(pParse,pNewExpr2,zCollSeqName),
           pStr2);
    transferJoinMarkings(pNewExpr2, pExpr);
    idxNew2 = whereClauseInsert(pWC, pNewExpr2, wtFlags);
    testcase( idxNew2==0 );
    exprAnalyze(pSrc, pWC, idxNew2);
    pTerm = &pWC->a[idxTerm];
    if( isComplete ){
      markTermAsChild(pWC, idxNew1, idxTerm);
      markTermAsChild(pWC, idxNew2, idxTerm);
    }
  }
#endif /* SQLITE_OMIT_LIKE_OPTIMIZATION */

#ifndef SQLITE_OMIT_VIRTUALTABLE
  /* Add a WO_AUX auxiliary term to the constraint set if the
  ** current expression is of the form "column OP expr" where OP
  ** is an operator that gets passed into virtual tables but which is
  ** not normally optimized for ordinary tables.  In other words, OP
  ** is one of MATCH, LIKE, GLOB, REGEXP, !=, IS, IS NOT, or NOT NULL.
  ** This information is used by the xBestIndex methods of
  ** virtual tables.  The native query optimizer does not attempt
  ** to do anything with MATCH functions.
  */
  if( pWC->op==TK_AND ){
    Expr *pRight = 0, *pLeft = 0;
    int res = isAuxiliaryVtabOperator(db, pExpr, &eOp2, &pLeft, &pRight);
    while( res-- > 0 ){
      int idxNew;
      WhereTerm *pNewTerm;
      Bitmask prereqColumn, prereqExpr;

      prereqExpr = sqlite3WhereExprUsage(pMaskSet, pRight);
      prereqColumn = sqlite3WhereExprUsage(pMaskSet, pLeft);
      if( (prereqExpr & prereqColumn)==0 ){
        Expr *pNewExpr;
        pNewExpr = sqlite3PExpr(pParse, TK_MATCH, 
            0, sqlite3ExprDup(db, pRight, 0));
        if( ExprHasProperty(pExpr, EP_FromJoin) && pNewExpr ){
          ExprSetProperty(pNewExpr, EP_FromJoin);
        }
        idxNew = whereClauseInsert(pWC, pNewExpr, TERM_VIRTUAL|TERM_DYNAMIC);
        testcase( idxNew==0 );
        pNewTerm = &pWC->a[idxNew];
        pNewTerm->prereqRight = prereqExpr;
        pNewTerm->leftCursor = pLeft->iTable;
        pNewTerm->u.leftColumn = pLeft->iColumn;
        pNewTerm->eOperator = WO_AUX;
        pNewTerm->eMatchOp = eOp2;
        markTermAsChild(pWC, idxNew, idxTerm);
        pTerm = &pWC->a[idxTerm];
        pTerm->wtFlags |= TERM_COPIED;
        pNewTerm->prereqAll = pTerm->prereqAll;
      }
      SWAP(Expr*, pLeft, pRight);
    }
  }
#endif /* SQLITE_OMIT_VIRTUALTABLE */

  /* If there is a vector == or IS term - e.g. "(a, b) == (?, ?)" - create
  ** new terms for each component comparison - "a = ?" and "b = ?".  The
  ** new terms completely replace the original vector comparison, which is
  ** no longer used.
  **
  ** This is only required if at least one side of the comparison operation
  ** is not a sub-select.  */
  if( pWC->op==TK_AND 
  && (pExpr->op==TK_EQ || pExpr->op==TK_IS)
  && (nLeft = sqlite3ExprVectorSize(pExpr->pLeft))>1
  && sqlite3ExprVectorSize(pExpr->pRight)==nLeft
  && ( (pExpr->pLeft->flags & EP_xIsSelect)==0 
    || (pExpr->pRight->flags & EP_xIsSelect)==0)
  ){
    int i;
    for(i=0; i<nLeft; i++){
      int idxNew;
      Expr *pNew;
      Expr *pLeft = sqlite3ExprForVectorField(pParse, pExpr->pLeft, i);
      Expr *pRight = sqlite3ExprForVectorField(pParse, pExpr->pRight, i);

      pNew = sqlite3PExpr(pParse, pExpr->op, pLeft, pRight);
      transferJoinMarkings(pNew, pExpr);
      idxNew = whereClauseInsert(pWC, pNew, TERM_DYNAMIC);
      exprAnalyze(pSrc, pWC, idxNew);
    }
    pTerm = &pWC->a[idxTerm];
    pTerm->wtFlags |= TERM_CODED|TERM_VIRTUAL;  /* Disable the original */
    pTerm->eOperator = 0;
  }

  /* If there is a vector IN term - e.g. "(a, b) IN (SELECT ...)" - create
  ** a virtual term for each vector component. The expression object
  ** used by each such virtual term is pExpr (the full vector IN(...) 
  ** expression). The WhereTerm.iField variable identifies the index within
  ** the vector on the LHS that the virtual term represents.
  **
  ** This only works if the RHS is a simple SELECT, not a compound
  */
  if( pWC->op==TK_AND && pExpr->op==TK_IN && pTerm->iField==0
   && pExpr->pLeft->op==TK_VECTOR
   && pExpr->x.pSelect->pPrior==0
  ){
    int i;
    for(i=0; i<sqlite3ExprVectorSize(pExpr->pLeft); i++){
      int idxNew;
      idxNew = whereClauseInsert(pWC, pExpr, TERM_VIRTUAL);
      pWC->a[idxNew].iField = i+1;
      exprAnalyze(pSrc, pWC, idxNew);
      markTermAsChild(pWC, idxNew, idxTerm);
    }
  }

#ifdef SQLITE_ENABLE_STAT4
  /* When sqlite_stat4 histogram data is available an operator of the
  ** form "x IS NOT NULL" can sometimes be evaluated more efficiently
  ** as "x>NULL" if x is not an INTEGER PRIMARY KEY.  So construct a
  ** virtual term of that form.
  **
  ** Note that the virtual term must be tagged with TERM_VNULL.
  */
  if( pExpr->op==TK_NOTNULL
   && pExpr->pLeft->op==TK_COLUMN
   && pExpr->pLeft->iColumn>=0
   && !ExprHasProperty(pExpr, EP_FromJoin)
   && OptimizationEnabled(db, SQLITE_Stat4)
  ){
    Expr *pNewExpr;
    Expr *pLeft = pExpr->pLeft;
    int idxNew;
    WhereTerm *pNewTerm;

    pNewExpr = sqlite3PExpr(pParse, TK_GT,
                            sqlite3ExprDup(db, pLeft, 0),
                            sqlite3ExprAlloc(db, TK_NULL, 0, 0));

    idxNew = whereClauseInsert(pWC, pNewExpr,
                              TERM_VIRTUAL|TERM_DYNAMIC|TERM_VNULL);
    if( idxNew ){
      pNewTerm = &pWC->a[idxNew];
      pNewTerm->prereqRight = 0;
      pNewTerm->leftCursor = pLeft->iTable;
      pNewTerm->u.leftColumn = pLeft->iColumn;
      pNewTerm->eOperator = WO_GT;
      markTermAsChild(pWC, idxNew, idxTerm);
      pTerm = &pWC->a[idxTerm];
      pTerm->wtFlags |= TERM_COPIED;
      pNewTerm->prereqAll = pTerm->prereqAll;
    }
  }
#endif /* SQLITE_ENABLE_STAT4 */

  /* Prevent ON clause terms of a LEFT JOIN from being used to drive
  ** an index for tables to the left of the join.
  */
  testcase( pTerm!=&pWC->a[idxTerm] );
  pTerm = &pWC->a[idxTerm];
  pTerm->prereqRight |= extraRight;
}

/***************************************************************************
** Routines with file scope above.  Interface to the rest of the where.c
** subsystem follows.
***************************************************************************/

/*
** This routine identifies subexpressions in the WHERE clause where
** each subexpression is separated by the AND operator or some other
** operator specified in the op parameter.  The WhereClause structure
** is filled with pointers to subexpressions.  For example:
**
**    WHERE  a=='hello' AND coalesce(b,11)<10 AND (c+12!=d OR c==22)
**           \________/     \_______________/     \________________/
**            slot[0]            slot[1]               slot[2]
**
** The original WHERE clause in pExpr is unaltered.  All this routine
** does is make slot[] entries point to substructure within pExpr.
**
** In the previous sentence and in the diagram, "slot[]" refers to
** the WhereClause.a[] array.  The slot[] array grows as needed to contain
** all terms of the WHERE clause.
*/
SQLITE_PRIVATE void sqlite3WhereSplit(WhereClause *pWC, Expr *pExpr, u8 op){
  Expr *pE2 = sqlite3ExprSkipCollateAndLikely(pExpr);
  pWC->op = op;
  if( pE2==0 ) return;
  if( pE2->op!=op ){
    whereClauseInsert(pWC, pExpr, 0);
  }else{
    sqlite3WhereSplit(pWC, pE2->pLeft, op);
    sqlite3WhereSplit(pWC, pE2->pRight, op);
  }
}

/*
** Initialize a preallocated WhereClause structure.
*/
SQLITE_PRIVATE void sqlite3WhereClauseInit(
  WhereClause *pWC,        /* The WhereClause to be initialized */
  WhereInfo *pWInfo        /* The WHERE processing context */
){
  pWC->pWInfo = pWInfo;
  pWC->hasOr = 0;
  pWC->pOuter = 0;
  pWC->nTerm = 0;
  pWC->nSlot = ArraySize(pWC->aStatic);
  pWC->a = pWC->aStatic;
}

/*
** Deallocate a WhereClause structure.  The WhereClause structure
** itself is not freed.  This routine is the inverse of
** sqlite3WhereClauseInit().
*/
SQLITE_PRIVATE void sqlite3WhereClauseClear(WhereClause *pWC){
  int i;
  WhereTerm *a;
  sqlite3 *db = pWC->pWInfo->pParse->db;
  for(i=pWC->nTerm-1, a=pWC->a; i>=0; i--, a++){
    if( a->wtFlags & TERM_DYNAMIC ){
      sqlite3ExprDelete(db, a->pExpr);
    }
    if( a->wtFlags & TERM_ORINFO ){
      whereOrInfoDelete(db, a->u.pOrInfo);
    }else if( a->wtFlags & TERM_ANDINFO ){
      whereAndInfoDelete(db, a->u.pAndInfo);
    }
  }
  if( pWC->a!=pWC->aStatic ){
    sqlite3DbFree(db, pWC->a);
  }
}


/*
** These routines walk (recursively) an expression tree and generate
** a bitmask indicating which tables are used in that expression
** tree.
*/
SQLITE_PRIVATE Bitmask sqlite3WhereExprUsageNN(WhereMaskSet *pMaskSet, Expr *p){
  Bitmask mask;
  if( p->op==TK_COLUMN && !ExprHasProperty(p, EP_FixedCol) ){
    return sqlite3WhereGetMask(pMaskSet, p->iTable);
  }else if( ExprHasProperty(p, EP_TokenOnly|EP_Leaf) ){
    assert( p->op!=TK_IF_NULL_ROW );
    return 0;
  }
  mask = (p->op==TK_IF_NULL_ROW) ? sqlite3WhereGetMask(pMaskSet, p->iTable) : 0;
  if( p->pLeft ) mask |= sqlite3WhereExprUsageNN(pMaskSet, p->pLeft);
  if( p->pRight ){
    mask |= sqlite3WhereExprUsageNN(pMaskSet, p->pRight);
    assert( p->x.pList==0 );
  }else if( ExprHasProperty(p, EP_xIsSelect) ){
    if( ExprHasProperty(p, EP_VarSelect) ) pMaskSet->bVarSelect = 1;
    mask |= exprSelectUsage(pMaskSet, p->x.pSelect);
  }else if( p->x.pList ){
    mask |= sqlite3WhereExprListUsage(pMaskSet, p->x.pList);
  }
#ifndef SQLITE_OMIT_WINDOWFUNC
  if( p->op==TK_FUNCTION && p->y.pWin ){
    mask |= sqlite3WhereExprListUsage(pMaskSet, p->y.pWin->pPartition);
    mask |= sqlite3WhereExprListUsage(pMaskSet, p->y.pWin->pOrderBy);
  }
#endif
  return mask;
}
SQLITE_PRIVATE Bitmask sqlite3WhereExprUsage(WhereMaskSet *pMaskSet, Expr *p){
  return p ? sqlite3WhereExprUsageNN(pMaskSet,p) : 0;
}
SQLITE_PRIVATE Bitmask sqlite3WhereExprListUsage(WhereMaskSet *pMaskSet, ExprList *pList){
  int i;
  Bitmask mask = 0;
  if( pList ){
    for(i=0; i<pList->nExpr; i++){
      mask |= sqlite3WhereExprUsage(pMaskSet, pList->a[i].pExpr);
    }
  }
  return mask;
}


/*
** Call exprAnalyze on all terms in a WHERE clause.  
**
** Note that exprAnalyze() might add new virtual terms onto the
** end of the WHERE clause.  We do not want to analyze these new
** virtual terms, so start analyzing at the end and work forward
** so that the added virtual terms are never processed.
*/
SQLITE_PRIVATE void sqlite3WhereExprAnalyze(
  SrcList *pTabList,       /* the FROM clause */
  WhereClause *pWC         /* the WHERE clause to be analyzed */
){
  int i;
  for(i=pWC->nTerm-1; i>=0; i--){
    exprAnalyze(pTabList, pWC, i);
  }
}

/*
** For table-valued-functions, transform the function arguments into
** new WHERE clause terms.  
**
** Each function argument translates into an equality constraint against
** a HIDDEN column in the table.
*/
SQLITE_PRIVATE void sqlite3WhereTabFuncArgs(
  Parse *pParse,                    /* Parsing context */
  struct SrcList_item *pItem,       /* The FROM clause term to process */
  WhereClause *pWC                  /* Xfer function arguments to here */
){
  Table *pTab;
  int j, k;
  ExprList *pArgs;
  Expr *pColRef;
  Expr *pTerm;
  if( pItem->fg.isTabFunc==0 ) return;
  pTab = pItem->pTab;
  assert( pTab!=0 );
  pArgs = pItem->u1.pFuncArg;
  if( pArgs==0 ) return;
  for(j=k=0; j<pArgs->nExpr; j++){
    Expr *pRhs;
    while( k<pTab->nCol && (pTab->aCol[k].colFlags & COLFLAG_HIDDEN)==0 ){k++;}
    if( k>=pTab->nCol ){
      sqlite3ErrorMsg(pParse, "too many arguments on %s() - max %d",
                      pTab->zName, j);
      return;
    }
    pColRef = sqlite3ExprAlloc(pParse->db, TK_COLUMN, 0, 0);
    if( pColRef==0 ) return;
    pColRef->iTable = pItem->iCursor;
    pColRef->iColumn = k++;
    pColRef->y.pTab = pTab;
    pRhs = sqlite3PExpr(pParse, TK_UPLUS, 
        sqlite3ExprDup(pParse->db, pArgs->a[j].pExpr, 0), 0);
    pTerm = sqlite3PExpr(pParse, TK_EQ, pColRef, pRhs);
    whereClauseInsert(pWC, pTerm, TERM_DYNAMIC);
  }
}

/************** End of whereexpr.c *******************************************/
/************** Begin file where.c *******************************************/
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
** This module contains C code that generates VDBE code used to process
** the WHERE clause of SQL statements.  This module is responsible for
** generating the code that loops through a table looking for applicable
** rows.  Indices are selected and used to speed the search when doing
** so is applicable.  Because this module is responsible for selecting
** indices, you might also think of this module as the "query optimizer".
*/
/* #include "sqliteInt.h" */
/* #include "whereInt.h" */

/*
** Extra information appended to the end of sqlite3_index_info but not
** visible to the xBestIndex function, at least not directly.  The
** sqlite3_vtab_collation() interface knows how to reach it, however.
**
** This object is not an API and can be changed from one release to the
** next.  As long as allocateIndexInfo() and sqlite3_vtab_collation()
** agree on the structure, all will be well.
*/
typedef struct HiddenIndexInfo HiddenIndexInfo;
struct HiddenIndexInfo {
  WhereClause *pWC;   /* The Where clause being analyzed */
  Parse *pParse;      /* The parsing context */
};

/* Forward declaration of methods */
static int whereLoopResize(sqlite3*, WhereLoop*, int);

/* Test variable that can be set to enable WHERE tracing */
#if defined(SQLITE_TEST) || defined(SQLITE_DEBUG)
/***/ int sqlite3WhereTrace = 0;
#endif


/*
** Return the estimated number of output rows from a WHERE clause
*/
SQLITE_PRIVATE LogEst sqlite3WhereOutputRowCount(WhereInfo *pWInfo){
  return pWInfo->nRowOut;
}

/*
** Return one of the WHERE_DISTINCT_xxxxx values to indicate how this
** WHERE clause returns outputs for DISTINCT processing.
*/
SQLITE_PRIVATE int sqlite3WhereIsDistinct(WhereInfo *pWInfo){
  return pWInfo->eDistinct;
}

/*
** Return TRUE if the WHERE clause returns rows in ORDER BY order.
** Return FALSE if the output needs to be sorted.
*/
SQLITE_PRIVATE int sqlite3WhereIsOrdered(WhereInfo *pWInfo){
  return pWInfo->nOBSat;
}

/*
** In the ORDER BY LIMIT optimization, if the inner-most loop is known
** to emit rows in increasing order, and if the last row emitted by the
** inner-most loop did not fit within the sorter, then we can skip all
** subsequent rows for the current iteration of the inner loop (because they
** will not fit in the sorter either) and continue with the second inner
** loop - the loop immediately outside the inner-most.
**
** When a row does not fit in the sorter (because the sorter already
** holds LIMIT+OFFSET rows that are smaller), then a jump is made to the
** label returned by this function.
**
** If the ORDER BY LIMIT optimization applies, the jump destination should
** be the continuation for the second-inner-most loop.  If the ORDER BY
** LIMIT optimization does not apply, then the jump destination should
** be the continuation for the inner-most loop.
**
** It is always safe for this routine to return the continuation of the
** inner-most loop, in the sense that a correct answer will result.  
** Returning the continuation the second inner loop is an optimization
** that might make the code run a little faster, but should not change
** the final answer.
*/
SQLITE_PRIVATE int sqlite3WhereOrderByLimitOptLabel(WhereInfo *pWInfo){
  WhereLevel *pInner;
  if( !pWInfo->bOrderedInnerLoop ){
    /* The ORDER BY LIMIT optimization does not apply.  Jump to the 
    ** continuation of the inner-most loop. */
    return pWInfo->iContinue;
  }
  pInner = &pWInfo->a[pWInfo->nLevel-1];
  assert( pInner->addrNxt!=0 );
  return pInner->addrNxt;
}

/*
** Return the VDBE address or label to jump to in order to continue
** immediately with the next row of a WHERE clause.
*/
SQLITE_PRIVATE int sqlite3WhereContinueLabel(WhereInfo *pWInfo){
  assert( pWInfo->iContinue!=0 );
  return pWInfo->iContinue;
}

/*
** Return the VDBE address or label to jump to in order to break
** out of a WHERE loop.
*/
SQLITE_PRIVATE int sqlite3WhereBreakLabel(WhereInfo *pWInfo){
  return pWInfo->iBreak;
}

/*
** Return ONEPASS_OFF (0) if an UPDATE or DELETE statement is unable to
** operate directly on the rowis returned by a WHERE clause.  Return
** ONEPASS_SINGLE (1) if the statement can operation directly because only
** a single row is to be changed.  Return ONEPASS_MULTI (2) if the one-pass
** optimization can be used on multiple 
**
** If the ONEPASS optimization is used (if this routine returns true)
** then also write the indices of open cursors used by ONEPASS
** into aiCur[0] and aiCur[1].  iaCur[0] gets the cursor of the data
** table and iaCur[1] gets the cursor used by an auxiliary index.
** Either value may be -1, indicating that cursor is not used.
** Any cursors returned will have been opened for writing.
**
** aiCur[0] and aiCur[1] both get -1 if the where-clause logic is
** unable to use the ONEPASS optimization.
*/
SQLITE_PRIVATE int sqlite3WhereOkOnePass(WhereInfo *pWInfo, int *aiCur){
  memcpy(aiCur, pWInfo->aiCurOnePass, sizeof(int)*2);
#ifdef WHERETRACE_ENABLED
  if( sqlite3WhereTrace && pWInfo->eOnePass!=ONEPASS_OFF ){
    sqlite3DebugPrintf("%s cursors: %d %d\n",
         pWInfo->eOnePass==ONEPASS_SINGLE ? "ONEPASS_SINGLE" : "ONEPASS_MULTI",
         aiCur[0], aiCur[1]);
  }
#endif
  return pWInfo->eOnePass;
}

/*
** Move the content of pSrc into pDest
*/
static void whereOrMove(WhereOrSet *pDest, WhereOrSet *pSrc){
  pDest->n = pSrc->n;
  memcpy(pDest->a, pSrc->a, pDest->n*sizeof(pDest->a[0]));
}

/*
** Try to insert a new prerequisite/cost entry into the WhereOrSet pSet.
**
** The new entry might overwrite an existing entry, or it might be
** appended, or it might be discarded.  Do whatever is the right thing
** so that pSet keeps the N_OR_COST best entries seen so far.
*/
static int whereOrInsert(
  WhereOrSet *pSet,      /* The WhereOrSet to be updated */
  Bitmask prereq,        /* Prerequisites of the new entry */
  LogEst rRun,           /* Run-cost of the new entry */
  LogEst nOut            /* Number of outputs for the new entry */
){
  u16 i;
  WhereOrCost *p;
  for(i=pSet->n, p=pSet->a; i>0; i--, p++){
    if( rRun<=p->rRun && (prereq & p->prereq)==prereq ){
      goto whereOrInsert_done;
    }
    if( p->rRun<=rRun && (p->prereq & prereq)==p->prereq ){
      return 0;
    }
  }
  if( pSet->n<N_OR_COST ){
    p = &pSet->a[pSet->n++];
    p->nOut = nOut;
  }else{
    p = pSet->a;
    for(i=1; i<pSet->n; i++){
      if( p->rRun>pSet->a[i].rRun ) p = pSet->a + i;
    }
    if( p->rRun<=rRun ) return 0;
  }
whereOrInsert_done:
  p->prereq = prereq;
  p->rRun = rRun;
  if( p->nOut>nOut ) p->nOut = nOut;
  return 1;
}

/*
** Return the bitmask for the given cursor number.  Return 0 if
** iCursor is not in the set.
*/
SQLITE_PRIVATE Bitmask sqlite3WhereGetMask(WhereMaskSet *pMaskSet, int iCursor){
  int i;
  assert( pMaskSet->n<=(int)sizeof(Bitmask)*8 );
  for(i=0; i<pMaskSet->n; i++){
    if( pMaskSet->ix[i]==iCursor ){
      return MASKBIT(i);
    }
  }
  return 0;
}

/*
** Create a new mask for cursor iCursor.
**
** There is one cursor per table in the FROM clause.  The number of
** tables in the FROM clause is limited by a test early in the
** sqlite3WhereBegin() routine.  So we know that the pMaskSet->ix[]
** array will never overflow.
*/
static void createMask(WhereMaskSet *pMaskSet, int iCursor){
  assert( pMaskSet->n < ArraySize(pMaskSet->ix) );
  pMaskSet->ix[pMaskSet->n++] = iCursor;
}

/*
** Advance to the next WhereTerm that matches according to the criteria
** established when the pScan object was initialized by whereScanInit().
** Return NULL if there are no more matching WhereTerms.
*/
static WhereTerm *whereScanNext(WhereScan *pScan){
  int iCur;            /* The cursor on the LHS of the term */
  i16 iColumn;         /* The column on the LHS of the term.  -1 for IPK */
  Expr *pX;            /* An expression being tested */
  WhereClause *pWC;    /* Shorthand for pScan->pWC */
  WhereTerm *pTerm;    /* The term being tested */
  int k = pScan->k;    /* Where to start scanning */

  assert( pScan->iEquiv<=pScan->nEquiv );
  pWC = pScan->pWC;
  while(1){
    iColumn = pScan->aiColumn[pScan->iEquiv-1];
    iCur = pScan->aiCur[pScan->iEquiv-1];
    assert( pWC!=0 );
    do{
      for(pTerm=pWC->a+k; k<pWC->nTerm; k++, pTerm++){
        if( pTerm->leftCursor==iCur
         && pTerm->u.leftColumn==iColumn
         && (iColumn!=XN_EXPR
             || sqlite3ExprCompareSkip(pTerm->pExpr->pLeft,
                                       pScan->pIdxExpr,iCur)==0)
         && (pScan->iEquiv<=1 || !ExprHasProperty(pTerm->pExpr, EP_FromJoin))
        ){
          if( (pTerm->eOperator & WO_EQUIV)!=0
           && pScan->nEquiv<ArraySize(pScan->aiCur)
           && (pX = sqlite3ExprSkipCollateAndLikely(pTerm->pExpr->pRight))->op
               ==TK_COLUMN
          ){
            int j;
            for(j=0; j<pScan->nEquiv; j++){
              if( pScan->aiCur[j]==pX->iTable
               && pScan->aiColumn[j]==pX->iColumn ){
                  break;
              }
            }
            if( j==pScan->nEquiv ){
              pScan->aiCur[j] = pX->iTable;
              pScan->aiColumn[j] = pX->iColumn;
              pScan->nEquiv++;
            }
          }
          if( (pTerm->eOperator & pScan->opMask)!=0 ){
            /* Verify the affinity and collating sequence match */
            if( pScan->zCollName && (pTerm->eOperator & WO_ISNULL)==0 ){
              CollSeq *pColl;
              Parse *pParse = pWC->pWInfo->pParse;
              pX = pTerm->pExpr;
              if( !sqlite3IndexAffinityOk(pX, pScan->idxaff) ){
                continue;
              }
              assert(pX->pLeft);
              pColl = sqlite3BinaryCompareCollSeq(pParse,
                                                  pX->pLeft, pX->pRight);
              if( pColl==0 ) pColl = pParse->db->pDfltColl;
              if( sqlite3StrICmp(pColl->zName, pScan->zCollName) ){
                continue;
              }
            }
            if( (pTerm->eOperator & (WO_EQ|WO_IS))!=0
             && (pX = pTerm->pExpr->pRight)->op==TK_COLUMN
             && pX->iTable==pScan->aiCur[0]
             && pX->iColumn==pScan->aiColumn[0]
            ){
              testcase( pTerm->eOperator & WO_IS );
              continue;
            }
            pScan->pWC = pWC;
            pScan->k = k+1;
            return pTerm;
          }
        }
      }
      pWC = pWC->pOuter;
      k = 0;
    }while( pWC!=0 );
    if( pScan->iEquiv>=pScan->nEquiv ) break;
    pWC = pScan->pOrigWC;
    k = 0;
    pScan->iEquiv++;
  }
  return 0;
}

/*
** This is whereScanInit() for the case of an index on an expression.
** It is factored out into a separate tail-recursion subroutine so that
** the normal whereScanInit() routine, which is a high-runner, does not
** need to push registers onto the stack as part of its prologue.
*/
static SQLITE_NOINLINE WhereTerm *whereScanInitIndexExpr(WhereScan *pScan){
  pScan->idxaff = sqlite3ExprAffinity(pScan->pIdxExpr);
  return whereScanNext(pScan);
}

/*
** Initialize a WHERE clause scanner object.  Return a pointer to the
** first match.  Return NULL if there are no matches.
**
** The scanner will be searching the WHERE clause pWC.  It will look
** for terms of the form "X <op> <expr>" where X is column iColumn of table
** iCur.   Or if pIdx!=0 then X is column iColumn of index pIdx.  pIdx
** must be one of the indexes of table iCur.
**
** The <op> must be one of the operators described by opMask.
**
** If the search is for X and the WHERE clause contains terms of the
** form X=Y then this routine might also return terms of the form
** "Y <op> <expr>".  The number of levels of transitivity is limited,
** but is enough to handle most commonly occurring SQL statements.
**
** If X is not the INTEGER PRIMARY KEY then X must be compatible with
** index pIdx.
*/
static WhereTerm *whereScanInit(
  WhereScan *pScan,       /* The WhereScan object being initialized */
  WhereClause *pWC,       /* The WHERE clause to be scanned */
  int iCur,               /* Cursor to scan for */
  int iColumn,            /* Column to scan for */
  u32 opMask,             /* Operator(s) to scan for */
  Index *pIdx             /* Must be compatible with this index */
){
  pScan->pOrigWC = pWC;
  pScan->pWC = pWC;
  pScan->pIdxExpr = 0;
  pScan->idxaff = 0;
  pScan->zCollName = 0;
  pScan->opMask = opMask;
  pScan->k = 0;
  pScan->aiCur[0] = iCur;
  pScan->nEquiv = 1;
  pScan->iEquiv = 1;
  if( pIdx ){
    int j = iColumn;
    iColumn = pIdx->aiColumn[j];
    if( iColumn==XN_EXPR ){
      pScan->pIdxExpr = pIdx->aColExpr->a[j].pExpr;
      pScan->zCollName = pIdx->azColl[j];
      pScan->aiColumn[0] = XN_EXPR;
      return whereScanInitIndexExpr(pScan);
    }else if( iColumn==pIdx->pTable->iPKey ){
      iColumn = XN_ROWID;
    }else if( iColumn>=0 ){
      pScan->idxaff = pIdx->pTable->aCol[iColumn].affinity;
      pScan->zCollName = pIdx->azColl[j];
    }
  }else if( iColumn==XN_EXPR ){
    return 0;
  }
  pScan->aiColumn[0] = iColumn;
  return whereScanNext(pScan);
}

/*
** Search for a term in the WHERE clause that is of the form "X <op> <expr>"
** where X is a reference to the iColumn of table iCur or of index pIdx
** if pIdx!=0 and <op> is one of the WO_xx operator codes specified by
** the op parameter.  Return a pointer to the term.  Return 0 if not found.
**
** If pIdx!=0 then it must be one of the indexes of table iCur.  
** Search for terms matching the iColumn-th column of pIdx
** rather than the iColumn-th column of table iCur.
**
** The term returned might by Y=<expr> if there is another constraint in
** the WHERE clause that specifies that X=Y.  Any such constraints will be
** identified by the WO_EQUIV bit in the pTerm->eOperator field.  The
** aiCur[]/iaColumn[] arrays hold X and all its equivalents. There are 11
** slots in aiCur[]/aiColumn[] so that means we can look for X plus up to 10
** other equivalent values.  Hence a search for X will return <expr> if X=A1
** and A1=A2 and A2=A3 and ... and A9=A10 and A10=<expr>.
**
** If there are multiple terms in the WHERE clause of the form "X <op> <expr>"
** then try for the one with no dependencies on <expr> - in other words where
** <expr> is a constant expression of some kind.  Only return entries of
** the form "X <op> Y" where Y is a column in another table if no terms of
** the form "X <op> <const-expr>" exist.   If no terms with a constant RHS
** exist, try to return a term that does not use WO_EQUIV.
*/
SQLITE_PRIVATE WhereTerm *sqlite3WhereFindTerm(
  WhereClause *pWC,     /* The WHERE clause to be searched */
  int iCur,             /* Cursor number of LHS */
  int iColumn,          /* Column number of LHS */
  Bitmask notReady,     /* RHS must not overlap with this mask */
  u32 op,               /* Mask of WO_xx values describing operator */
  Index *pIdx           /* Must be compatible with this index, if not NULL */
){
  WhereTerm *pResult = 0;
  WhereTerm *p;
  WhereScan scan;

  p = whereScanInit(&scan, pWC, iCur, iColumn, op, pIdx);
  op &= WO_EQ|WO_IS;
  while( p ){
    if( (p->prereqRight & notReady)==0 ){
      if( p->prereqRight==0 && (p->eOperator&op)!=0 ){
        testcase( p->eOperator & WO_IS );
        return p;
      }
      if( pResult==0 ) pResult = p;
    }
    p = whereScanNext(&scan);
  }
  return pResult;
}

/*
** This function searches pList for an entry that matches the iCol-th column
** of index pIdx.
**
** If such an expression is found, its index in pList->a[] is returned. If
** no expression is found, -1 is returned.
*/
static int findIndexCol(
  Parse *pParse,                  /* Parse context */
  ExprList *pList,                /* Expression list to search */
  int iBase,                      /* Cursor for table associated with pIdx */
  Index *pIdx,                    /* Index to match column of */
  int iCol                        /* Column of index to match */
){
  int i;
  const char *zColl = pIdx->azColl[iCol];

  for(i=0; i<pList->nExpr; i++){
    Expr *p = sqlite3ExprSkipCollateAndLikely(pList->a[i].pExpr);
    if( p->op==TK_COLUMN
     && p->iColumn==pIdx->aiColumn[iCol]
     && p->iTable==iBase
    ){
      CollSeq *pColl = sqlite3ExprNNCollSeq(pParse, pList->a[i].pExpr);
      if( 0==sqlite3StrICmp(pColl->zName, zColl) ){
        return i;
      }
    }
  }

  return -1;
}

/*
** Return TRUE if the iCol-th column of index pIdx is NOT NULL
*/
static int indexColumnNotNull(Index *pIdx, int iCol){
  int j;
  assert( pIdx!=0 );
  assert( iCol>=0 && iCol<pIdx->nColumn );
  j = pIdx->aiColumn[iCol];
  if( j>=0 ){
    return pIdx->pTable->aCol[j].notNull;
  }else if( j==(-1) ){
    return 1;
  }else{
    assert( j==(-2) );
    return 0;  /* Assume an indexed expression can always yield a NULL */

  }
}

/*
** Return true if the DISTINCT expression-list passed as the third argument
** is redundant.
**
** A DISTINCT list is redundant if any subset of the columns in the
** DISTINCT list are collectively unique and individually non-null.
*/
static int isDistinctRedundant(
  Parse *pParse,            /* Parsing context */
  SrcList *pTabList,        /* The FROM clause */
  WhereClause *pWC,         /* The WHERE clause */
  ExprList *pDistinct       /* The result set that needs to be DISTINCT */
){
  Table *pTab;
  Index *pIdx;
  int i;                          
  int iBase;

  /* If there is more than one table or sub-select in the FROM clause of
  ** this query, then it will not be possible to show that the DISTINCT 
  ** clause is redundant. */
  if( pTabList->nSrc!=1 ) return 0;
  iBase = pTabList->a[0].iCursor;
  pTab = pTabList->a[0].pTab;

  /* If any of the expressions is an IPK column on table iBase, then return 
  ** true. Note: The (p->iTable==iBase) part of this test may be false if the
  ** current SELECT is a correlated sub-query.
  */
  for(i=0; i<pDistinct->nExpr; i++){
    Expr *p = sqlite3ExprSkipCollateAndLikely(pDistinct->a[i].pExpr);
    if( p->op==TK_COLUMN && p->iTable==iBase && p->iColumn<0 ) return 1;
  }

  /* Loop through all indices on the table, checking each to see if it makes
  ** the DISTINCT qualifier redundant. It does so if:
  **
  **   1. The index is itself UNIQUE, and
  **
  **   2. All of the columns in the index are either part of the pDistinct
  **      list, or else the WHERE clause contains a term of the form "col=X",
  **      where X is a constant value. The collation sequences of the
  **      comparison and select-list expressions must match those of the index.
  **
  **   3. All of those index columns for which the WHERE clause does not
  **      contain a "col=X" term are subject to a NOT NULL constraint.
  */
  for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){
    if( !IsUniqueIndex(pIdx) ) continue;
    for(i=0; i<pIdx->nKeyCol; i++){
      if( 0==sqlite3WhereFindTerm(pWC, iBase, i, ~(Bitmask)0, WO_EQ, pIdx) ){
        if( findIndexCol(pParse, pDistinct, iBase, pIdx, i)<0 ) break;
        if( indexColumnNotNull(pIdx, i)==0 ) break;
      }
    }
    if( i==pIdx->nKeyCol ){
      /* This index implies that the DISTINCT qualifier is redundant. */
      return 1;
    }
  }

  return 0;
}


/*
** Estimate the logarithm of the input value to base 2.
*/
static LogEst estLog(LogEst N){
  return N<=10 ? 0 : sqlite3LogEst(N) - 33;
}

/*
** Convert OP_Column opcodes to OP_Copy in previously generated code.
**
** This routine runs over generated VDBE code and translates OP_Column
** opcodes into OP_Copy when the table is being accessed via co-routine 
** instead of via table lookup.
**
** If the iAutoidxCur is not zero, then any OP_Rowid instructions on
** cursor iTabCur are transformed into OP_Sequence opcode for the
** iAutoidxCur cursor, in order to generate unique rowids for the
** automatic index being generated.
*/
static void translateColumnToCopy(
  Parse *pParse,      /* Parsing context */
  int iStart,         /* Translate from this opcode to the end */
  int iTabCur,        /* OP_Column/OP_Rowid references to this table */
  int iRegister,      /* The first column is in this register */
  int iAutoidxCur     /* If non-zero, cursor of autoindex being generated */
){
  Vdbe *v = pParse->pVdbe;
  VdbeOp *pOp = sqlite3VdbeGetOp(v, iStart);
  int iEnd = sqlite3VdbeCurrentAddr(v);
  if( pParse->db->mallocFailed ) return;
  for(; iStart<iEnd; iStart++, pOp++){
    if( pOp->p1!=iTabCur ) continue;
    if( pOp->opcode==OP_Column ){
      pOp->opcode = OP_Copy;
      pOp->p1 = pOp->p2 + iRegister;
      pOp->p2 = pOp->p3;
      pOp->p3 = 0;
    }else if( pOp->opcode==OP_Rowid ){
      if( iAutoidxCur ){
        pOp->opcode = OP_Sequence;
        pOp->p1 = iAutoidxCur;
      }else{
        pOp->opcode = OP_Null;
        pOp->p1 = 0;
        pOp->p3 = 0;
      }
    }
  }
}

/*
** Two routines for printing the content of an sqlite3_index_info
** structure.  Used for testing and debugging only.  If neither
** SQLITE_TEST or SQLITE_DEBUG are defined, then these routines
** are no-ops.
*/
#if !defined(SQLITE_OMIT_VIRTUALTABLE) && defined(WHERETRACE_ENABLED)
static void TRACE_IDX_INPUTS(sqlite3_index_info *p){
  int i;
  if( !sqlite3WhereTrace ) return;
  for(i=0; i<p->nConstraint; i++){
    sqlite3DebugPrintf("  constraint[%d]: col=%d termid=%d op=%d usabled=%d\n",
       i,
       p->aConstraint[i].iColumn,
       p->aConstraint[i].iTermOffset,
       p->aConstraint[i].op,
       p->aConstraint[i].usable);
  }
  for(i=0; i<p->nOrderBy; i++){
    sqlite3DebugPrintf("  orderby[%d]: col=%d desc=%d\n",
       i,
       p->aOrderBy[i].iColumn,
       p->aOrderBy[i].desc);
  }
}
static void TRACE_IDX_OUTPUTS(sqlite3_index_info *p){
  int i;
  if( !sqlite3WhereTrace ) return;
  for(i=0; i<p->nConstraint; i++){
    sqlite3DebugPrintf("  usage[%d]: argvIdx=%d omit=%d\n",
       i,
       p->aConstraintUsage[i].argvIndex,
       p->aConstraintUsage[i].omit);
  }
  sqlite3DebugPrintf("  idxNum=%d\n", p->idxNum);
  sqlite3DebugPrintf("  idxStr=%s\n", p->idxStr);
  sqlite3DebugPrintf("  orderByConsumed=%d\n", p->orderByConsumed);
  sqlite3DebugPrintf("  estimatedCost=%g\n", p->estimatedCost);
  sqlite3DebugPrintf("  estimatedRows=%lld\n", p->estimatedRows);
}
#else
#define TRACE_IDX_INPUTS(A)
#define TRACE_IDX_OUTPUTS(A)
#endif

#ifndef SQLITE_OMIT_AUTOMATIC_INDEX
/*
** Return TRUE if the WHERE clause term pTerm is of a form where it
** could be used with an index to access pSrc, assuming an appropriate
** index existed.
*/
static int termCanDriveIndex(
  WhereTerm *pTerm,              /* WHERE clause term to check */
  struct SrcList_item *pSrc,     /* Table we are trying to access */
  Bitmask notReady               /* Tables in outer loops of the join */
){
  char aff;
  if( pTerm->leftCursor!=pSrc->iCursor ) return 0;
  if( (pTerm->eOperator & (WO_EQ|WO_IS))==0 ) return 0;
  if( (pSrc->fg.jointype & JT_LEFT) 
   && !ExprHasProperty(pTerm->pExpr, EP_FromJoin)
   && (pTerm->eOperator & WO_IS)
  ){
    /* Cannot use an IS term from the WHERE clause as an index driver for
    ** the RHS of a LEFT JOIN. Such a term can only be used if it is from
    ** the ON clause.  */
    return 0;
  }
  if( (pTerm->prereqRight & notReady)!=0 ) return 0;
  if( pTerm->u.leftColumn<0 ) return 0;
  aff = pSrc->pTab->aCol[pTerm->u.leftColumn].affinity;
  if( !sqlite3IndexAffinityOk(pTerm->pExpr, aff) ) return 0;
  testcase( pTerm->pExpr->op==TK_IS );
  return 1;
}
#endif


#ifndef SQLITE_OMIT_AUTOMATIC_INDEX
/*
** Generate code to construct the Index object for an automatic index
** and to set up the WhereLevel object pLevel so that the code generator
** makes use of the automatic index.
*/
static void constructAutomaticIndex(
  Parse *pParse,              /* The parsing context */
  WhereClause *pWC,           /* The WHERE clause */
  struct SrcList_item *pSrc,  /* The FROM clause term to get the next index */
  Bitmask notReady,           /* Mask of cursors that are not available */
  WhereLevel *pLevel          /* Write new index here */
){
  int nKeyCol;                /* Number of columns in the constructed index */
  WhereTerm *pTerm;           /* A single term of the WHERE clause */
  WhereTerm *pWCEnd;          /* End of pWC->a[] */
  Index *pIdx;                /* Object describing the transient index */
  Vdbe *v;                    /* Prepared statement under construction */
  int addrInit;               /* Address of the initialization bypass jump */
  Table *pTable;              /* The table being indexed */
  int addrTop;                /* Top of the index fill loop */
  int regRecord;              /* Register holding an index record */
  int n;                      /* Column counter */
  int i;                      /* Loop counter */
  int mxBitCol;               /* Maximum column in pSrc->colUsed */
  CollSeq *pColl;             /* Collating sequence to on a column */
  WhereLoop *pLoop;           /* The Loop object */
  char *zNotUsed;             /* Extra space on the end of pIdx */
  Bitmask idxCols;            /* Bitmap of columns used for indexing */
  Bitmask extraCols;          /* Bitmap of additional columns */
  u8 sentWarning = 0;         /* True if a warnning has been issued */
  Expr *pPartial = 0;         /* Partial Index Expression */
  int iContinue = 0;          /* Jump here to skip excluded rows */
  struct SrcList_item *pTabItem;  /* FROM clause term being indexed */
  int addrCounter = 0;        /* Address where integer counter is initialized */
  int regBase;                /* Array of registers where record is assembled */

  /* Generate code to skip over the creation and initialization of the
  ** transient index on 2nd and subsequent iterations of the loop. */
  v = pParse->pVdbe;
  assert( v!=0 );
  addrInit = sqlite3VdbeAddOp0(v, OP_Once); VdbeCoverage(v);

  /* Count the number of columns that will be added to the index
  ** and used to match WHERE clause constraints */
  nKeyCol = 0;
  pTable = pSrc->pTab;
  pWCEnd = &pWC->a[pWC->nTerm];
  pLoop = pLevel->pWLoop;
  idxCols = 0;
  for(pTerm=pWC->a; pTerm<pWCEnd; pTerm++){
    Expr *pExpr = pTerm->pExpr;
    assert( !ExprHasProperty(pExpr, EP_FromJoin)    /* prereq always non-zero */
         || pExpr->iRightJoinTable!=pSrc->iCursor   /*   for the right-hand   */
         || pLoop->prereq!=0 );                     /*   table of a LEFT JOIN */
    if( pLoop->prereq==0
     && (pTerm->wtFlags & TERM_VIRTUAL)==0
     && !ExprHasProperty(pExpr, EP_FromJoin)
     && sqlite3ExprIsTableConstant(pExpr, pSrc->iCursor) ){
      pPartial = sqlite3ExprAnd(pParse, pPartial,
                                sqlite3ExprDup(pParse->db, pExpr, 0));
    }
    if( termCanDriveIndex(pTerm, pSrc, notReady) ){
      int iCol = pTerm->u.leftColumn;
      Bitmask cMask = iCol>=BMS ? MASKBIT(BMS-1) : MASKBIT(iCol);
      testcase( iCol==BMS );
      testcase( iCol==BMS-1 );
      if( !sentWarning ){
        sqlite3_log(SQLITE_WARNING_AUTOINDEX,
            "automatic index on %s(%s)", pTable->zName,
            pTable->aCol[iCol].zName);
        sentWarning = 1;
      }
      if( (idxCols & cMask)==0 ){
        if( whereLoopResize(pParse->db, pLoop, nKeyCol+1) ){
          goto end_auto_index_create;
        }
        pLoop->aLTerm[nKeyCol++] = pTerm;
        idxCols |= cMask;
      }
    }
  }
  assert( nKeyCol>0 );
  pLoop->u.btree.nEq = pLoop->nLTerm = nKeyCol;
  pLoop->wsFlags = WHERE_COLUMN_EQ | WHERE_IDX_ONLY | WHERE_INDEXED
                     | WHERE_AUTO_INDEX;

  /* Count the number of additional columns needed to create a
  ** covering index.  A "covering index" is an index that contains all
  ** columns that are needed by the query.  With a covering index, the
  ** original table never needs to be accessed.  Automatic indices must
  ** be a covering index because the index will not be updated if the
  ** original table changes and the index and table cannot both be used
  ** if they go out of sync.
  */
  extraCols = pSrc->colUsed & (~idxCols | MASKBIT(BMS-1));
  mxBitCol = MIN(BMS-1,pTable->nCol);
  testcase( pTable->nCol==BMS-1 );
  testcase( pTable->nCol==BMS-2 );
  for(i=0; i<mxBitCol; i++){
    if( extraCols & MASKBIT(i) ) nKeyCol++;
  }
  if( pSrc->colUsed & MASKBIT(BMS-1) ){
    nKeyCol += pTable->nCol - BMS + 1;
  }

  /* Construct the Index object to describe this index */
  pIdx = sqlite3AllocateIndexObject(pParse->db, nKeyCol+1, 0, &zNotUsed);
  if( pIdx==0 ) goto end_auto_index_create;
  pLoop->u.btree.pIndex = pIdx;
  pIdx->zName = "auto-index";
  pIdx->pTable = pTable;
  n = 0;
  idxCols = 0;
  for(pTerm=pWC->a; pTerm<pWCEnd; pTerm++){
    if( termCanDriveIndex(pTerm, pSrc, notReady) ){
      int iCol = pTerm->u.leftColumn;
      Bitmask cMask = iCol>=BMS ? MASKBIT(BMS-1) : MASKBIT(iCol);
      testcase( iCol==BMS-1 );
      testcase( iCol==BMS );
      if( (idxCols & cMask)==0 ){
        Expr *pX = pTerm->pExpr;
        idxCols |= cMask;
        pIdx->aiColumn[n] = pTerm->u.leftColumn;
        pColl = sqlite3BinaryCompareCollSeq(pParse, pX->pLeft, pX->pRight);
        pIdx->azColl[n] = pColl ? pColl->zName : sqlite3StrBINARY;
        n++;
      }
    }
  }
  assert( (u32)n==pLoop->u.btree.nEq );

  /* Add additional columns needed to make the automatic index into
  ** a covering index */
  for(i=0; i<mxBitCol; i++){
    if( extraCols & MASKBIT(i) ){
      pIdx->aiColumn[n] = i;
      pIdx->azColl[n] = sqlite3StrBINARY;
      n++;
    }
  }
  if( pSrc->colUsed & MASKBIT(BMS-1) ){
    for(i=BMS-1; i<pTable->nCol; i++){
      pIdx->aiColumn[n] = i;
      pIdx->azColl[n] = sqlite3StrBINARY;
      n++;
    }
  }
  assert( n==nKeyCol );
  pIdx->aiColumn[n] = XN_ROWID;
  pIdx->azColl[n] = sqlite3StrBINARY;

  /* Create the automatic index */
  assert( pLevel->iIdxCur>=0 );
  pLevel->iIdxCur = pParse->nTab++;
  sqlite3VdbeAddOp2(v, OP_OpenAutoindex, pLevel->iIdxCur, nKeyCol+1);
  sqlite3VdbeSetP4KeyInfo(pParse, pIdx);
  VdbeComment((v, "for %s", pTable->zName));

  /* Fill the automatic index with content */
  pTabItem = &pWC->pWInfo->pTabList->a[pLevel->iFrom];
  if( pTabItem->fg.viaCoroutine ){
    int regYield = pTabItem->regReturn;
    addrCounter = sqlite3VdbeAddOp2(v, OP_Integer, 0, 0);
    sqlite3VdbeAddOp3(v, OP_InitCoroutine, regYield, 0, pTabItem->addrFillSub);
    addrTop =  sqlite3VdbeAddOp1(v, OP_Yield, regYield);
    VdbeCoverage(v);
    VdbeComment((v, "next row of %s", pTabItem->pTab->zName));
  }else{
    addrTop = sqlite3VdbeAddOp1(v, OP_Rewind, pLevel->iTabCur); VdbeCoverage(v);
  }
  if( pPartial ){
    iContinue = sqlite3VdbeMakeLabel(pParse);
    sqlite3ExprIfFalse(pParse, pPartial, iContinue, SQLITE_JUMPIFNULL);
    pLoop->wsFlags |= WHERE_PARTIALIDX;
  }
  regRecord = sqlite3GetTempReg(pParse);
  regBase = sqlite3GenerateIndexKey(
      pParse, pIdx, pLevel->iTabCur, regRecord, 0, 0, 0, 0
  );
  sqlite3VdbeAddOp2(v, OP_IdxInsert, pLevel->iIdxCur, regRecord);
  sqlite3VdbeChangeP5(v, OPFLAG_USESEEKRESULT);
  if( pPartial ) sqlite3VdbeResolveLabel(v, iContinue);
  if( pTabItem->fg.viaCoroutine ){
    sqlite3VdbeChangeP2(v, addrCounter, regBase+n);
    testcase( pParse->db->mallocFailed );
    assert( pLevel->iIdxCur>0 );
    translateColumnToCopy(pParse, addrTop, pLevel->iTabCur,
                          pTabItem->regResult, pLevel->iIdxCur);
    sqlite3VdbeGoto(v, addrTop);
    pTabItem->fg.viaCoroutine = 0;
  }else{
    sqlite3VdbeAddOp2(v, OP_Next, pLevel->iTabCur, addrTop+1); VdbeCoverage(v);
  }
  sqlite3VdbeChangeP5(v, SQLITE_STMTSTATUS_AUTOINDEX);
  sqlite3VdbeJumpHere(v, addrTop);
  sqlite3ReleaseTempReg(pParse, regRecord);
  
  /* Jump here when skipping the initialization */
  sqlite3VdbeJumpHere(v, addrInit);

end_auto_index_create:
  sqlite3ExprDelete(pParse->db, pPartial);
}
#endif /* SQLITE_OMIT_AUTOMATIC_INDEX */

#ifndef SQLITE_OMIT_VIRTUALTABLE
/*
** Allocate and populate an sqlite3_index_info structure. It is the 
** responsibility of the caller to eventually release the structure
** by passing the pointer returned by this function to sqlite3_free().
*/
static sqlite3_index_info *allocateIndexInfo(
  Parse *pParse,                  /* The parsing context */
  WhereClause *pWC,               /* The WHERE clause being analyzed */
  Bitmask mUnusable,              /* Ignore terms with these prereqs */
  struct SrcList_item *pSrc,      /* The FROM clause term that is the vtab */
  ExprList *pOrderBy,             /* The ORDER BY clause */
  u16 *pmNoOmit                   /* Mask of terms not to omit */
){
  int i, j;
  int nTerm;
  struct sqlite3_index_constraint *pIdxCons;
  struct sqlite3_index_orderby *pIdxOrderBy;
  struct sqlite3_index_constraint_usage *pUsage;
  struct HiddenIndexInfo *pHidden;
  WhereTerm *pTerm;
  int nOrderBy;
  sqlite3_index_info *pIdxInfo;
  u16 mNoOmit = 0;

  /* Count the number of possible WHERE clause constraints referring
  ** to this virtual table */
  for(i=nTerm=0, pTerm=pWC->a; i<pWC->nTerm; i++, pTerm++){
    if( pTerm->leftCursor != pSrc->iCursor ) continue;
    if( pTerm->prereqRight & mUnusable ) continue;
    assert( IsPowerOfTwo(pTerm->eOperator & ~WO_EQUIV) );
    testcase( pTerm->eOperator & WO_IN );
    testcase( pTerm->eOperator & WO_ISNULL );
    testcase( pTerm->eOperator & WO_IS );
    testcase( pTerm->eOperator & WO_ALL );
    if( (pTerm->eOperator & ~(WO_EQUIV))==0 ) continue;
    if( pTerm->wtFlags & TERM_VNULL ) continue;
    assert( pTerm->u.leftColumn>=(-1) );
    nTerm++;
  }

  /* If the ORDER BY clause contains only columns in the current 
  ** virtual table then allocate space for the aOrderBy part of
  ** the sqlite3_index_info structure.
  */
  nOrderBy = 0;
  if( pOrderBy ){
    int n = pOrderBy->nExpr;
    for(i=0; i<n; i++){
      Expr *pExpr = pOrderBy->a[i].pExpr;
      if( pExpr->op!=TK_COLUMN || pExpr->iTable!=pSrc->iCursor ) break;
      if( pOrderBy->a[i].sortFlags & KEYINFO_ORDER_BIGNULL ) break;
    }
    if( i==n){
      nOrderBy = n;
    }
  }

  /* Allocate the sqlite3_index_info structure
  */
  pIdxInfo = sqlite3DbMallocZero(pParse->db, sizeof(*pIdxInfo)
                           + (sizeof(*pIdxCons) + sizeof(*pUsage))*nTerm
                           + sizeof(*pIdxOrderBy)*nOrderBy + sizeof(*pHidden) );
  if( pIdxInfo==0 ){
    sqlite3ErrorMsg(pParse, "out of memory");
    return 0;
  }

  /* Initialize the structure.  The sqlite3_index_info structure contains
  ** many fields that are declared "const" to prevent xBestIndex from
  ** changing them.  We have to do some funky casting in order to
  ** initialize those fields.
  */
  pHidden = (struct HiddenIndexInfo*)&pIdxInfo[1];
  pIdxCons = (struct sqlite3_index_constraint*)&pHidden[1];
  pIdxOrderBy = (struct sqlite3_index_orderby*)&pIdxCons[nTerm];
  pUsage = (struct sqlite3_index_constraint_usage*)&pIdxOrderBy[nOrderBy];
  *(int*)&pIdxInfo->nConstraint = nTerm;
  *(int*)&pIdxInfo->nOrderBy = nOrderBy;
  *(struct sqlite3_index_constraint**)&pIdxInfo->aConstraint = pIdxCons;
  *(struct sqlite3_index_orderby**)&pIdxInfo->aOrderBy = pIdxOrderBy;
  *(struct sqlite3_index_constraint_usage**)&pIdxInfo->aConstraintUsage =
                                                                   pUsage;

  pHidden->pWC = pWC;
  pHidden->pParse = pParse;
  for(i=j=0, pTerm=pWC->a; i<pWC->nTerm; i++, pTerm++){
    u16 op;
    if( pTerm->leftCursor != pSrc->iCursor ) continue;
    if( pTerm->prereqRight & mUnusable ) continue;
    assert( IsPowerOfTwo(pTerm->eOperator & ~WO_EQUIV) );
    testcase( pTerm->eOperator & WO_IN );
    testcase( pTerm->eOperator & WO_IS );
    testcase( pTerm->eOperator & WO_ISNULL );
    testcase( pTerm->eOperator & WO_ALL );
    if( (pTerm->eOperator & ~(WO_EQUIV))==0 ) continue;
    if( pTerm->wtFlags & TERM_VNULL ) continue;
    if( (pSrc->fg.jointype & JT_LEFT)!=0
     && !ExprHasProperty(pTerm->pExpr, EP_FromJoin)
     && (pTerm->eOperator & (WO_IS|WO_ISNULL))
    ){
      /* An "IS" term in the WHERE clause where the virtual table is the rhs
      ** of a LEFT JOIN. Do not pass this term to the virtual table
      ** implementation, as this can lead to incorrect results from SQL such
      ** as:
      **
      **   "LEFT JOIN vtab WHERE vtab.col IS NULL"  */
      testcase( pTerm->eOperator & WO_ISNULL );
      testcase( pTerm->eOperator & WO_IS );
      continue;
    }
    assert( pTerm->u.leftColumn>=(-1) );
    pIdxCons[j].iColumn = pTerm->u.leftColumn;
    pIdxCons[j].iTermOffset = i;
    op = pTerm->eOperator & WO_ALL;
    if( op==WO_IN ) op = WO_EQ;
    if( op==WO_AUX ){
      pIdxCons[j].op = pTerm->eMatchOp;
    }else if( op & (WO_ISNULL|WO_IS) ){
      if( op==WO_ISNULL ){
        pIdxCons[j].op = SQLITE_INDEX_CONSTRAINT_ISNULL;
      }else{
        pIdxCons[j].op = SQLITE_INDEX_CONSTRAINT_IS;
      }
    }else{
      pIdxCons[j].op = (u8)op;
      /* The direct assignment in the previous line is possible only because
      ** the WO_ and SQLITE_INDEX_CONSTRAINT_ codes are identical.  The
      ** following asserts verify this fact. */
      assert( WO_EQ==SQLITE_INDEX_CONSTRAINT_EQ );
      assert( WO_LT==SQLITE_INDEX_CONSTRAINT_LT );
      assert( WO_LE==SQLITE_INDEX_CONSTRAINT_LE );
      assert( WO_GT==SQLITE_INDEX_CONSTRAINT_GT );
      assert( WO_GE==SQLITE_INDEX_CONSTRAINT_GE );
      assert( pTerm->eOperator&(WO_IN|WO_EQ|WO_LT|WO_LE|WO_GT|WO_GE|WO_AUX) );

      if( op & (WO_LT|WO_LE|WO_GT|WO_GE)
       && sqlite3ExprIsVector(pTerm->pExpr->pRight) 
      ){
        if( i<16 ) mNoOmit |= (1 << i);
        if( op==WO_LT ) pIdxCons[j].op = WO_LE;
        if( op==WO_GT ) pIdxCons[j].op = WO_GE;
      }
    }

    j++;
  }
  for(i=0; i<nOrderBy; i++){
    Expr *pExpr = pOrderBy->a[i].pExpr;
    pIdxOrderBy[i].iColumn = pExpr->iColumn;
    pIdxOrderBy[i].desc = pOrderBy->a[i].sortFlags & KEYINFO_ORDER_DESC;
  }

  *pmNoOmit = mNoOmit;
  return pIdxInfo;
}

/*
** The table object reference passed as the second argument to this function
** must represent a virtual table. This function invokes the xBestIndex()
** method of the virtual table with the sqlite3_index_info object that
** comes in as the 3rd argument to this function.
**
** If an error occurs, pParse is populated with an error message and an
** appropriate error code is returned.  A return of SQLITE_CONSTRAINT from
** xBestIndex is not considered an error.  SQLITE_CONSTRAINT indicates that
** the current configuration of "unusable" flags in sqlite3_index_info can
** not result in a valid plan.
**
** Whether or not an error is returned, it is the responsibility of the
** caller to eventually free p->idxStr if p->needToFreeIdxStr indicates
** that this is required.
*/
static int vtabBestIndex(Parse *pParse, Table *pTab, sqlite3_index_info *p){
  sqlite3_vtab *pVtab = sqlite3GetVTable(pParse->db, pTab)->pVtab;
  int rc;

  TRACE_IDX_INPUTS(p);
  rc = pVtab->pModule->xBestIndex(pVtab, p);
  TRACE_IDX_OUTPUTS(p);

  if( rc!=SQLITE_OK && rc!=SQLITE_CONSTRAINT ){
    if( rc==SQLITE_NOMEM ){
      sqlite3OomFault(pParse->db);
    }else if( !pVtab->zErrMsg ){
      sqlite3ErrorMsg(pParse, "%s", sqlite3ErrStr(rc));
    }else{
      sqlite3ErrorMsg(pParse, "%s", pVtab->zErrMsg);
    }
  }
  sqlite3_free(pVtab->zErrMsg);
  pVtab->zErrMsg = 0;
  return rc;
}
#endif /* !defined(SQLITE_OMIT_VIRTUALTABLE) */

#ifdef SQLITE_ENABLE_STAT4
/*
** Estimate the location of a particular key among all keys in an
** index.  Store the results in aStat as follows:
**
**    aStat[0]      Est. number of rows less than pRec
**    aStat[1]      Est. number of rows equal to pRec
**
** Return the index of the sample that is the smallest sample that
** is greater than or equal to pRec. Note that this index is not an index
** into the aSample[] array - it is an index into a virtual set of samples
** based on the contents of aSample[] and the number of fields in record 
** pRec. 
*/
static int whereKeyStats(
  Parse *pParse,              /* Database connection */
  Index *pIdx,                /* Index to consider domain of */
  UnpackedRecord *pRec,       /* Vector of values to consider */
  int roundUp,                /* Round up if true.  Round down if false */
  tRowcnt *aStat              /* OUT: stats written here */
){
  IndexSample *aSample = pIdx->aSample;
  int iCol;                   /* Index of required stats in anEq[] etc. */
  int i;                      /* Index of first sample >= pRec */
  int iSample;                /* Smallest sample larger than or equal to pRec */
  int iMin = 0;               /* Smallest sample not yet tested */
  int iTest;                  /* Next sample to test */
  int res;                    /* Result of comparison operation */
  int nField;                 /* Number of fields in pRec */
  tRowcnt iLower = 0;         /* anLt[] + anEq[] of largest sample pRec is > */

#ifndef SQLITE_DEBUG
  UNUSED_PARAMETER( pParse );
#endif
  assert( pRec!=0 );
  assert( pIdx->nSample>0 );
  assert( pRec->nField>0 && pRec->nField<=pIdx->nSampleCol );

  /* Do a binary search to find the first sample greater than or equal
  ** to pRec. If pRec contains a single field, the set of samples to search
  ** is simply the aSample[] array. If the samples in aSample[] contain more
  ** than one fields, all fields following the first are ignored.
  **
  ** If pRec contains N fields, where N is more than one, then as well as the
  ** samples in aSample[] (truncated to N fields), the search also has to
  ** consider prefixes of those samples. For example, if the set of samples
  ** in aSample is:
  **
  **     aSample[0] = (a, 5) 
  **     aSample[1] = (a, 10) 
  **     aSample[2] = (b, 5) 
  **     aSample[3] = (c, 100) 
  **     aSample[4] = (c, 105)
  **
  ** Then the search space should ideally be the samples above and the 
  ** unique prefixes [a], [b] and [c]. But since that is hard to organize, 
  ** the code actually searches this set:
  **
  **     0: (a) 
  **     1: (a, 5) 
  **     2: (a, 10) 
  **     3: (a, 10) 
  **     4: (b) 
  **     5: (b, 5) 
  **     6: (c) 
  **     7: (c, 100) 
  **     8: (c, 105)
  **     9: (c, 105)
  **
  ** For each sample in the aSample[] array, N samples are present in the
  ** effective sample array. In the above, samples 0 and 1 are based on 
  ** sample aSample[0]. Samples 2 and 3 on aSample[1] etc.
  **
  ** Often, sample i of each block of N effective samples has (i+1) fields.
  ** Except, each sample may be extended to ensure that it is greater than or
  ** equal to the previous sample in the array. For example, in the above, 
  ** sample 2 is the first sample of a block of N samples, so at first it 
  ** appears that it should be 1 field in size. However, that would make it 
  ** smaller than sample 1, so the binary search would not work. As a result, 
  ** it is extended to two fields. The duplicates that this creates do not 
  ** cause any problems.
  */
  nField = pRec->nField;
  iCol = 0;
  iSample = pIdx->nSample * nField;
  do{
    int iSamp;                    /* Index in aSample[] of test sample */
    int n;                        /* Number of fields in test sample */

    iTest = (iMin+iSample)/2;
    iSamp = iTest / nField;
    if( iSamp>0 ){
      /* The proposed effective sample is a prefix of sample aSample[iSamp].
      ** Specifically, the shortest prefix of at least (1 + iTest%nField) 
      ** fields that is greater than the previous effective sample.  */
      for(n=(iTest % nField) + 1; n<nField; n++){
        if( aSample[iSamp-1].anLt[n-1]!=aSample[iSamp].anLt[n-1] ) break;
      }
    }else{
      n = iTest + 1;
    }

    pRec->nField = n;
    res = sqlite3VdbeRecordCompare(aSample[iSamp].n, aSample[iSamp].p, pRec);
    if( res<0 ){
      iLower = aSample[iSamp].anLt[n-1] + aSample[iSamp].anEq[n-1];
      iMin = iTest+1;
    }else if( res==0 && n<nField ){
      iLower = aSample[iSamp].anLt[n-1];
      iMin = iTest+1;
      res = -1;
    }else{
      iSample = iTest;
      iCol = n-1;
    }
  }while( res && iMin<iSample );
  i = iSample / nField;

#ifdef SQLITE_DEBUG
  /* The following assert statements check that the binary search code
  ** above found the right answer. This block serves no purpose other
  ** than to invoke the asserts.  */
  if( pParse->db->mallocFailed==0 ){
    if( res==0 ){
      /* If (res==0) is true, then pRec must be equal to sample i. */
      assert( i<pIdx->nSample );
      assert( iCol==nField-1 );
      pRec->nField = nField;
      assert( 0==sqlite3VdbeRecordCompare(aSample[i].n, aSample[i].p, pRec) 
           || pParse->db->mallocFailed 
      );
    }else{
      /* Unless i==pIdx->nSample, indicating that pRec is larger than
      ** all samples in the aSample[] array, pRec must be smaller than the
      ** (iCol+1) field prefix of sample i.  */
      assert( i<=pIdx->nSample && i>=0 );
      pRec->nField = iCol+1;
      assert( i==pIdx->nSample 
           || sqlite3VdbeRecordCompare(aSample[i].n, aSample[i].p, pRec)>0
           || pParse->db->mallocFailed );

      /* if i==0 and iCol==0, then record pRec is smaller than all samples
      ** in the aSample[] array. Otherwise, if (iCol>0) then pRec must
      ** be greater than or equal to the (iCol) field prefix of sample i.
      ** If (i>0), then pRec must also be greater than sample (i-1).  */
      if( iCol>0 ){
        pRec->nField = iCol;
        assert( sqlite3VdbeRecordCompare(aSample[i].n, aSample[i].p, pRec)<=0
             || pParse->db->mallocFailed );
      }
      if( i>0 ){
        pRec->nField = nField;
        assert( sqlite3VdbeRecordCompare(aSample[i-1].n, aSample[i-1].p, pRec)<0
             || pParse->db->mallocFailed );
      }
    }
  }
#endif /* ifdef SQLITE_DEBUG */

  if( res==0 ){
    /* Record pRec is equal to sample i */
    assert( iCol==nField-1 );
    aStat[0] = aSample[i].anLt[iCol];
    aStat[1] = aSample[i].anEq[iCol];
  }else{
    /* At this point, the (iCol+1) field prefix of aSample[i] is the first 
    ** sample that is greater than pRec. Or, if i==pIdx->nSample then pRec
    ** is larger than all samples in the array. */
    tRowcnt iUpper, iGap;
    if( i>=pIdx->nSample ){
      iUpper = sqlite3LogEstToInt(pIdx->aiRowLogEst[0]);
    }else{
      iUpper = aSample[i].anLt[iCol];
    }

    if( iLower>=iUpper ){
      iGap = 0;
    }else{
      iGap = iUpper - iLower;
    }
    if( roundUp ){
      iGap = (iGap*2)/3;
    }else{
      iGap = iGap/3;
    }
    aStat[0] = iLower + iGap;
    aStat[1] = pIdx->aAvgEq[nField-1];
  }

  /* Restore the pRec->nField value before returning.  */
  pRec->nField = nField;
  return i;
}
#endif /* SQLITE_ENABLE_STAT4 */

/*
** If it is not NULL, pTerm is a term that provides an upper or lower
** bound on a range scan. Without considering pTerm, it is estimated 
** that the scan will visit nNew rows. This function returns the number
** estimated to be visited after taking pTerm into account.
**
** If the user explicitly specified a likelihood() value for this term,
** then the return value is the likelihood multiplied by the number of
** input rows. Otherwise, this function assumes that an "IS NOT NULL" term
** has a likelihood of 0.50, and any other term a likelihood of 0.25.
*/
static LogEst whereRangeAdjust(WhereTerm *pTerm, LogEst nNew){
  LogEst nRet = nNew;
  if( pTerm ){
    if( pTerm->truthProb<=0 ){
      nRet += pTerm->truthProb;
    }else if( (pTerm->wtFlags & TERM_VNULL)==0 ){
      nRet -= 20;        assert( 20==sqlite3LogEst(4) );
    }
  }
  return nRet;
}


#ifdef SQLITE_ENABLE_STAT4
/*
** Return the affinity for a single column of an index.
*/
SQLITE_PRIVATE char sqlite3IndexColumnAffinity(sqlite3 *db, Index *pIdx, int iCol){
  assert( iCol>=0 && iCol<pIdx->nColumn );
  if( !pIdx->zColAff ){
    if( sqlite3IndexAffinityStr(db, pIdx)==0 ) return SQLITE_AFF_BLOB;
  }
  assert( pIdx->zColAff[iCol]!=0 );
  return pIdx->zColAff[iCol];
}
#endif


#ifdef SQLITE_ENABLE_STAT4
/* 
** This function is called to estimate the number of rows visited by a
** range-scan on a skip-scan index. For example:
**
**   CREATE INDEX i1 ON t1(a, b, c);
**   SELECT * FROM t1 WHERE a=? AND c BETWEEN ? AND ?;
**
** Value pLoop->nOut is currently set to the estimated number of rows 
** visited for scanning (a=? AND b=?). This function reduces that estimate 
** by some factor to account for the (c BETWEEN ? AND ?) expression based
** on the stat4 data for the index. this scan will be peformed multiple 
** times (once for each (a,b) combination that matches a=?) is dealt with 
** by the caller.
**
** It does this by scanning through all stat4 samples, comparing values
** extracted from pLower and pUpper with the corresponding column in each
** sample. If L and U are the number of samples found to be less than or
** equal to the values extracted from pLower and pUpper respectively, and
** N is the total number of samples, the pLoop->nOut value is adjusted
** as follows:
**
**   nOut = nOut * ( min(U - L, 1) / N )
**
** If pLower is NULL, or a value cannot be extracted from the term, L is
** set to zero. If pUpper is NULL, or a value cannot be extracted from it,
** U is set to N.
**
** Normally, this function sets *pbDone to 1 before returning. However,
** if no value can be extracted from either pLower or pUpper (and so the
** estimate of the number of rows delivered remains unchanged), *pbDone
** is left as is.
**
** If an error occurs, an SQLite error code is returned. Otherwise, 
** SQLITE_OK.
*/
static int whereRangeSkipScanEst(
  Parse *pParse,       /* Parsing & code generating context */
  WhereTerm *pLower,   /* Lower bound on the range. ex: "x>123" Might be NULL */
  WhereTerm *pUpper,   /* Upper bound on the range. ex: "x<455" Might be NULL */
  WhereLoop *pLoop,    /* Update the .nOut value of this loop */
  int *pbDone          /* Set to true if at least one expr. value extracted */
){
  Index *p = pLoop->u.btree.pIndex;
  int nEq = pLoop->u.btree.nEq;
  sqlite3 *db = pParse->db;
  int nLower = -1;
  int nUpper = p->nSample+1;
  int rc = SQLITE_OK;
  u8 aff = sqlite3IndexColumnAffinity(db, p, nEq);
  CollSeq *pColl;
  
  sqlite3_value *p1 = 0;          /* Value extracted from pLower */
  sqlite3_value *p2 = 0;          /* Value extracted from pUpper */
  sqlite3_value *pVal = 0;        /* Value extracted from record */

  pColl = sqlite3LocateCollSeq(pParse, p->azColl[nEq]);
  if( pLower ){
    rc = sqlite3Stat4ValueFromExpr(pParse, pLower->pExpr->pRight, aff, &p1);
    nLower = 0;
  }
  if( pUpper && rc==SQLITE_OK ){
    rc = sqlite3Stat4ValueFromExpr(pParse, pUpper->pExpr->pRight, aff, &p2);
    nUpper = p2 ? 0 : p->nSample;
  }

  if( p1 || p2 ){
    int i;
    int nDiff;
    for(i=0; rc==SQLITE_OK && i<p->nSample; i++){
      rc = sqlite3Stat4Column(db, p->aSample[i].p, p->aSample[i].n, nEq, &pVal);
      if( rc==SQLITE_OK && p1 ){
        int res = sqlite3MemCompare(p1, pVal, pColl);
        if( res>=0 ) nLower++;
      }
      if( rc==SQLITE_OK && p2 ){
        int res = sqlite3MemCompare(p2, pVal, pColl);
        if( res>=0 ) nUpper++;
      }
    }
    nDiff = (nUpper - nLower);
    if( nDiff<=0 ) nDiff = 1;

    /* If there is both an upper and lower bound specified, and the 
    ** comparisons indicate that they are close together, use the fallback
    ** method (assume that the scan visits 1/64 of the rows) for estimating
    ** the number of rows visited. Otherwise, estimate the number of rows
    ** using the method described in the header comment for this function. */
    if( nDiff!=1 || pUpper==0 || pLower==0 ){
      int nAdjust = (sqlite3LogEst(p->nSample) - sqlite3LogEst(nDiff));
      pLoop->nOut -= nAdjust;
      *pbDone = 1;
      WHERETRACE(0x10, ("range skip-scan regions: %u..%u  adjust=%d est=%d\n",
                           nLower, nUpper, nAdjust*-1, pLoop->nOut));
    }

  }else{
    assert( *pbDone==0 );
  }

  sqlite3ValueFree(p1);
  sqlite3ValueFree(p2);
  sqlite3ValueFree(pVal);

  return rc;
}
#endif /* SQLITE_ENABLE_STAT4 */

/*
** This function is used to estimate the number of rows that will be visited
** by scanning an index for a range of values. The range may have an upper
** bound, a lower bound, or both. The WHERE clause terms that set the upper
** and lower bounds are represented by pLower and pUpper respectively. For
** example, assuming that index p is on t1(a):
**
**   ... FROM t1 WHERE a > ? AND a < ? ...
**                    |_____|   |_____|
**                       |         |
**                     pLower    pUpper
**
** If either of the upper or lower bound is not present, then NULL is passed in
** place of the corresponding WhereTerm.
**
** The value in (pBuilder->pNew->u.btree.nEq) is the number of the index
** column subject to the range constraint. Or, equivalently, the number of
** equality constraints optimized by the proposed index scan. For example,
** assuming index p is on t1(a, b), and the SQL query is:
**
**   ... FROM t1 WHERE a = ? AND b > ? AND b < ? ...
**
** then nEq is set to 1 (as the range restricted column, b, is the second 
** left-most column of the index). Or, if the query is:
**
**   ... FROM t1 WHERE a > ? AND a < ? ...
**
** then nEq is set to 0.
**
** When this function is called, *pnOut is set to the sqlite3LogEst() of the
** number of rows that the index scan is expected to visit without 
** considering the range constraints. If nEq is 0, then *pnOut is the number of 
** rows in the index. Assuming no error occurs, *pnOut is adjusted (reduced)
** to account for the range constraints pLower and pUpper.
** 
** In the absence of sqlite_stat4 ANALYZE data, or if such data cannot be
** used, a single range inequality reduces the search space by a factor of 4. 
** and a pair of constraints (x>? AND x<?) reduces the expected number of
** rows visited by a factor of 64.
*/
static int whereRangeScanEst(
  Parse *pParse,       /* Parsing & code generating context */
  WhereLoopBuilder *pBuilder,
  WhereTerm *pLower,   /* Lower bound on the range. ex: "x>123" Might be NULL */
  WhereTerm *pUpper,   /* Upper bound on the range. ex: "x<455" Might be NULL */
  WhereLoop *pLoop     /* Modify the .nOut and maybe .rRun fields */
){
  int rc = SQLITE_OK;
  int nOut = pLoop->nOut;
  LogEst nNew;

#ifdef SQLITE_ENABLE_STAT4
  Index *p = pLoop->u.btree.pIndex;
  int nEq = pLoop->u.btree.nEq;

  if( p->nSample>0 && ALWAYS(nEq<p->nSampleCol)
   && OptimizationEnabled(pParse->db, SQLITE_Stat4)
  ){
    if( nEq==pBuilder->nRecValid ){
      UnpackedRecord *pRec = pBuilder->pRec;
      tRowcnt a[2];
      int nBtm = pLoop->u.btree.nBtm;
      int nTop = pLoop->u.btree.nTop;

      /* Variable iLower will be set to the estimate of the number of rows in 
      ** the index that are less than the lower bound of the range query. The
      ** lower bound being the concatenation of $P and $L, where $P is the
      ** key-prefix formed by the nEq values matched against the nEq left-most
      ** columns of the index, and $L is the value in pLower.
      **
      ** Or, if pLower is NULL or $L cannot be extracted from it (because it
      ** is not a simple variable or literal value), the lower bound of the
      ** range is $P. Due to a quirk in the way whereKeyStats() works, even
      ** if $L is available, whereKeyStats() is called for both ($P) and 
      ** ($P:$L) and the larger of the two returned values is used.
      **
      ** Similarly, iUpper is to be set to the estimate of the number of rows
      ** less than the upper bound of the range query. Where the upper bound
      ** is either ($P) or ($P:$U). Again, even if $U is available, both values
      ** of iUpper are requested of whereKeyStats() and the smaller used.
      **
      ** The number of rows between the two bounds is then just iUpper-iLower.
      */
      tRowcnt iLower;     /* Rows less than the lower bound */
      tRowcnt iUpper;     /* Rows less than the upper bound */
      int iLwrIdx = -2;   /* aSample[] for the lower bound */
      int iUprIdx = -1;   /* aSample[] for the upper bound */

      if( pRec ){
        testcase( pRec->nField!=pBuilder->nRecValid );
        pRec->nField = pBuilder->nRecValid;
      }
      /* Determine iLower and iUpper using ($P) only. */
      if( nEq==0 ){
        iLower = 0;
        iUpper = p->nRowEst0;
      }else{
        /* Note: this call could be optimized away - since the same values must 
        ** have been requested when testing key $P in whereEqualScanEst().  */
        whereKeyStats(pParse, p, pRec, 0, a);
        iLower = a[0];
        iUpper = a[0] + a[1];
      }

      assert( pLower==0 || (pLower->eOperator & (WO_GT|WO_GE))!=0 );
      assert( pUpper==0 || (pUpper->eOperator & (WO_LT|WO_LE))!=0 );
      assert( p->aSortOrder!=0 );
      if( p->aSortOrder[nEq] ){
        /* The roles of pLower and pUpper are swapped for a DESC index */
        SWAP(WhereTerm*, pLower, pUpper);
        SWAP(int, nBtm, nTop);
      }

      /* If possible, improve on the iLower estimate using ($P:$L). */
      if( pLower ){
        int n;                    /* Values extracted from pExpr */
        Expr *pExpr = pLower->pExpr->pRight;
        rc = sqlite3Stat4ProbeSetValue(pParse, p, &pRec, pExpr, nBtm, nEq, &n);
        if( rc==SQLITE_OK && n ){
          tRowcnt iNew;
          u16 mask = WO_GT|WO_LE;
          if( sqlite3ExprVectorSize(pExpr)>n ) mask = (WO_LE|WO_LT);
          iLwrIdx = whereKeyStats(pParse, p, pRec, 0, a);
          iNew = a[0] + ((pLower->eOperator & mask) ? a[1] : 0);
          if( iNew>iLower ) iLower = iNew;
          nOut--;
          pLower = 0;
        }
      }

      /* If possible, improve on the iUpper estimate using ($P:$U). */
      if( pUpper ){
        int n;                    /* Values extracted from pExpr */
        Expr *pExpr = pUpper->pExpr->pRight;
        rc = sqlite3Stat4ProbeSetValue(pParse, p, &pRec, pExpr, nTop, nEq, &n);
        if( rc==SQLITE_OK && n ){
          tRowcnt iNew;
          u16 mask = WO_GT|WO_LE;
          if( sqlite3ExprVectorSize(pExpr)>n ) mask = (WO_LE|WO_LT);
          iUprIdx = whereKeyStats(pParse, p, pRec, 1, a);
          iNew = a[0] + ((pUpper->eOperator & mask) ? a[1] : 0);
          if( iNew<iUpper ) iUpper = iNew;
          nOut--;
          pUpper = 0;
        }
      }

      pBuilder->pRec = pRec;
      if( rc==SQLITE_OK ){
        if( iUpper>iLower ){
          nNew = sqlite3LogEst(iUpper - iLower);
          /* TUNING:  If both iUpper and iLower are derived from the same
          ** sample, then assume they are 4x more selective.  This brings
          ** the estimated selectivity more in line with what it would be
          ** if estimated without the use of STAT4 tables. */
          if( iLwrIdx==iUprIdx ) nNew -= 20;  assert( 20==sqlite3LogEst(4) );
        }else{
          nNew = 10;        assert( 10==sqlite3LogEst(2) );
        }
        if( nNew<nOut ){
          nOut = nNew;
        }
        WHERETRACE(0x10, ("STAT4 range scan: %u..%u  est=%d\n",
                           (u32)iLower, (u32)iUpper, nOut));
      }
    }else{
      int bDone = 0;
      rc = whereRangeSkipScanEst(pParse, pLower, pUpper, pLoop, &bDone);
      if( bDone ) return rc;
    }
  }
#else
  UNUSED_PARAMETER(pParse);
  UNUSED_PARAMETER(pBuilder);
  assert( pLower || pUpper );
#endif
  assert( pUpper==0 || (pUpper->wtFlags & TERM_VNULL)==0 );
  nNew = whereRangeAdjust(pLower, nOut);
  nNew = whereRangeAdjust(pUpper, nNew);

  /* TUNING: If there is both an upper and lower limit and neither limit
  ** has an application-defined likelihood(), assume the range is
  ** reduced by an additional 75%. This means that, by default, an open-ended
  ** range query (e.g. col > ?) is assumed to match 1/4 of the rows in the
  ** index. While a closed range (e.g. col BETWEEN ? AND ?) is estimated to
  ** match 1/64 of the index. */ 
  if( pLower && pLower->truthProb>0 && pUpper && pUpper->truthProb>0 ){
    nNew -= 20;
  }

  nOut -= (pLower!=0) + (pUpper!=0);
  if( nNew<10 ) nNew = 10;
  if( nNew<nOut ) nOut = nNew;
#if defined(WHERETRACE_ENABLED)
  if( pLoop->nOut>nOut ){
    WHERETRACE(0x10,("Range scan lowers nOut from %d to %d\n",
                    pLoop->nOut, nOut));
  }
#endif
  pLoop->nOut = (LogEst)nOut;
  return rc;
}

#ifdef SQLITE_ENABLE_STAT4
/*
** Estimate the number of rows that will be returned based on
** an equality constraint x=VALUE and where that VALUE occurs in
** the histogram data.  This only works when x is the left-most
** column of an index and sqlite_stat4 histogram data is available
** for that index.  When pExpr==NULL that means the constraint is
** "x IS NULL" instead of "x=VALUE".
**
** Write the estimated row count into *pnRow and return SQLITE_OK. 
** If unable to make an estimate, leave *pnRow unchanged and return
** non-zero.
**
** This routine can fail if it is unable to load a collating sequence
** required for string comparison, or if unable to allocate memory
** for a UTF conversion required for comparison.  The error is stored
** in the pParse structure.
*/
static int whereEqualScanEst(
  Parse *pParse,       /* Parsing & code generating context */
  WhereLoopBuilder *pBuilder,
  Expr *pExpr,         /* Expression for VALUE in the x=VALUE constraint */
  tRowcnt *pnRow       /* Write the revised row estimate here */
){
  Index *p = pBuilder->pNew->u.btree.pIndex;
  int nEq = pBuilder->pNew->u.btree.nEq;
  UnpackedRecord *pRec = pBuilder->pRec;
  int rc;                   /* Subfunction return code */
  tRowcnt a[2];             /* Statistics */
  int bOk;

  assert( nEq>=1 );
  assert( nEq<=p->nColumn );
  assert( p->aSample!=0 );
  assert( p->nSample>0 );
  assert( pBuilder->nRecValid<nEq );

  /* If values are not available for all fields of the index to the left
  ** of this one, no estimate can be made. Return SQLITE_NOTFOUND. */
  if( pBuilder->nRecValid<(nEq-1) ){
    return SQLITE_NOTFOUND;
  }

  /* This is an optimization only. The call to sqlite3Stat4ProbeSetValue()
  ** below would return the same value.  */
  if( nEq>=p->nColumn ){
    *pnRow = 1;
    return SQLITE_OK;
  }

  rc = sqlite3Stat4ProbeSetValue(pParse, p, &pRec, pExpr, 1, nEq-1, &bOk);
  pBuilder->pRec = pRec;
  if( rc!=SQLITE_OK ) return rc;
  if( bOk==0 ) return SQLITE_NOTFOUND;
  pBuilder->nRecValid = nEq;

  whereKeyStats(pParse, p, pRec, 0, a);
  WHERETRACE(0x10,("equality scan regions %s(%d): %d\n",
                   p->zName, nEq-1, (int)a[1]));
  *pnRow = a[1];
  
  return rc;
}
#endif /* SQLITE_ENABLE_STAT4 */

#ifdef SQLITE_ENABLE_STAT4
/*
** Estimate the number of rows that will be returned based on
** an IN constraint where the right-hand side of the IN operator
** is a list of values.  Example:
**
**        WHERE x IN (1,2,3,4)
**
** Write the estimated row count into *pnRow and return SQLITE_OK. 
** If unable to make an estimate, leave *pnRow unchanged and return
** non-zero.
**
** This routine can fail if it is unable to load a collating sequence
** required for string comparison, or if unable to allocate memory
** for a UTF conversion required for comparison.  The error is stored
** in the pParse structure.
*/
static int whereInScanEst(
  Parse *pParse,       /* Parsing & code generating context */
  WhereLoopBuilder *pBuilder,
  ExprList *pList,     /* The value list on the RHS of "x IN (v1,v2,v3,...)" */
  tRowcnt *pnRow       /* Write the revised row estimate here */
){
  Index *p = pBuilder->pNew->u.btree.pIndex;
  i64 nRow0 = sqlite3LogEstToInt(p->aiRowLogEst[0]);
  int nRecValid = pBuilder->nRecValid;
  int rc = SQLITE_OK;     /* Subfunction return code */
  tRowcnt nEst;           /* Number of rows for a single term */
  tRowcnt nRowEst = 0;    /* New estimate of the number of rows */
  int i;                  /* Loop counter */

  assert( p->aSample!=0 );
  for(i=0; rc==SQLITE_OK && i<pList->nExpr; i++){
    nEst = nRow0;
    rc = whereEqualScanEst(pParse, pBuilder, pList->a[i].pExpr, &nEst);
    nRowEst += nEst;
    pBuilder->nRecValid = nRecValid;
  }

  if( rc==SQLITE_OK ){
    if( nRowEst > nRow0 ) nRowEst = nRow0;
    *pnRow = nRowEst;
    WHERETRACE(0x10,("IN row estimate: est=%d\n", nRowEst));
  }
  assert( pBuilder->nRecValid==nRecValid );
  return rc;
}
#endif /* SQLITE_ENABLE_STAT4 */


#ifdef WHERETRACE_ENABLED
/*
** Print the content of a WhereTerm object
*/
static void whereTermPrint(WhereTerm *pTerm, int iTerm){
  if( pTerm==0 ){
    sqlite3DebugPrintf("TERM-%-3d NULL\n", iTerm);
  }else{
    char zType[4];
    char zLeft[50];
    memcpy(zType, "...", 4);
    if( pTerm->wtFlags & TERM_VIRTUAL ) zType[0] = 'V';
    if( pTerm->eOperator & WO_EQUIV  ) zType[1] = 'E';
    if( ExprHasProperty(pTerm->pExpr, EP_FromJoin) ) zType[2] = 'L';
    if( pTerm->eOperator & WO_SINGLE ){
      sqlite3_snprintf(sizeof(zLeft),zLeft,"left={%d:%d}",
                       pTerm->leftCursor, pTerm->u.leftColumn);
    }else if( (pTerm->eOperator & WO_OR)!=0 && pTerm->u.pOrInfo!=0 ){
      sqlite3_snprintf(sizeof(zLeft),zLeft,"indexable=0x%lld", 
                       pTerm->u.pOrInfo->indexable);
    }else{
      sqlite3_snprintf(sizeof(zLeft),zLeft,"left=%d", pTerm->leftCursor);
    }
    sqlite3DebugPrintf(
       "TERM-%-3d %p %s %-12s prob=%-3d op=0x%03x wtFlags=0x%04x",
       iTerm, pTerm, zType, zLeft, pTerm->truthProb,
       pTerm->eOperator, pTerm->wtFlags);
    if( pTerm->iField ){
      sqlite3DebugPrintf(" iField=%d\n", pTerm->iField);
    }else{
      sqlite3DebugPrintf("\n");
    }
    sqlite3TreeViewExpr(0, pTerm->pExpr, 0);
  }
}
#endif

#ifdef WHERETRACE_ENABLED
/*
** Show the complete content of a WhereClause
*/
SQLITE_PRIVATE void sqlite3WhereClausePrint(WhereClause *pWC){
  int i;
  for(i=0; i<pWC->nTerm; i++){
    whereTermPrint(&pWC->a[i], i);
  }
}
#endif

#ifdef WHERETRACE_ENABLED
/*
** Print a WhereLoop object for debugging purposes
*/
static void whereLoopPrint(WhereLoop *p, WhereClause *pWC){
  WhereInfo *pWInfo = pWC->pWInfo;
  int nb = 1+(pWInfo->pTabList->nSrc+3)/4;
  struct SrcList_item *pItem = pWInfo->pTabList->a + p->iTab;
  Table *pTab = pItem->pTab;
  Bitmask mAll = (((Bitmask)1)<<(nb*4)) - 1;
  sqlite3DebugPrintf("%c%2d.%0*llx.%0*llx", p->cId,
                     p->iTab, nb, p->maskSelf, nb, p->prereq & mAll);
  sqlite3DebugPrintf(" %12s",
                     pItem->zAlias ? pItem->zAlias : pTab->zName);
  if( (p->wsFlags & WHERE_VIRTUALTABLE)==0 ){
    const char *zName;
    if( p->u.btree.pIndex && (zName = p->u.btree.pIndex->zName)!=0 ){
      if( strncmp(zName, "sqlite_autoindex_", 17)==0 ){
        int i = sqlite3Strlen30(zName) - 1;
        while( zName[i]!='_' ) i--;
        zName += i;
      }
      sqlite3DebugPrintf(".%-16s %2d", zName, p->u.btree.nEq);
    }else{
      sqlite3DebugPrintf("%20s","");
    }
  }else{
    char *z;
    if( p->u.vtab.idxStr ){
      z = sqlite3_mprintf("(%d,\"%s\",%x)",
                p->u.vtab.idxNum, p->u.vtab.idxStr, p->u.vtab.omitMask);
    }else{
      z = sqlite3_mprintf("(%d,%x)", p->u.vtab.idxNum, p->u.vtab.omitMask);
    }
    sqlite3DebugPrintf(" %-19s", z);
    sqlite3_free(z);
  }
  if( p->wsFlags & WHERE_SKIPSCAN ){
    sqlite3DebugPrintf(" f %05x %d-%d", p->wsFlags, p->nLTerm,p->nSkip);
  }else{
    sqlite3DebugPrintf(" f %05x N %d", p->wsFlags, p->nLTerm);
  }
  sqlite3DebugPrintf(" cost %d,%d,%d\n", p->rSetup, p->rRun, p->nOut);
  if( p->nLTerm && (sqlite3WhereTrace & 0x100)!=0 ){
    int i;
    for(i=0; i<p->nLTerm; i++){
      whereTermPrint(p->aLTerm[i], i);
    }
  }
}
#endif

/*
** Convert bulk memory into a valid WhereLoop that can be passed
** to whereLoopClear harmlessly.
*/
static void whereLoopInit(WhereLoop *p){
  p->aLTerm = p->aLTermSpace;
  p->nLTerm = 0;
  p->nLSlot = ArraySize(p->aLTermSpace);
  p->wsFlags = 0;
}

/*
** Clear the WhereLoop.u union.  Leave WhereLoop.pLTerm intact.
*/
static void whereLoopClearUnion(sqlite3 *db, WhereLoop *p){
  if( p->wsFlags & (WHERE_VIRTUALTABLE|WHERE_AUTO_INDEX) ){
    if( (p->wsFlags & WHERE_VIRTUALTABLE)!=0 && p->u.vtab.needFree ){
      sqlite3_free(p->u.vtab.idxStr);
      p->u.vtab.needFree = 0;
      p->u.vtab.idxStr = 0;
    }else if( (p->wsFlags & WHERE_AUTO_INDEX)!=0 && p->u.btree.pIndex!=0 ){
      sqlite3DbFree(db, p->u.btree.pIndex->zColAff);
      sqlite3DbFreeNN(db, p->u.btree.pIndex);
      p->u.btree.pIndex = 0;
    }
  }
}

/*
** Deallocate internal memory used by a WhereLoop object
*/
static void whereLoopClear(sqlite3 *db, WhereLoop *p){
  if( p->aLTerm!=p->aLTermSpace ) sqlite3DbFreeNN(db, p->aLTerm);
  whereLoopClearUnion(db, p);
  whereLoopInit(p);
}

/*
** Increase the memory allocation for pLoop->aLTerm[] to be at least n.
*/
static int whereLoopResize(sqlite3 *db, WhereLoop *p, int n){
  WhereTerm **paNew;
  if( p->nLSlot>=n ) return SQLITE_OK;
  n = (n+7)&~7;
  paNew = sqlite3DbMallocRawNN(db, sizeof(p->aLTerm[0])*n);
  if( paNew==0 ) return SQLITE_NOMEM_BKPT;
  memcpy(paNew, p->aLTerm, sizeof(p->aLTerm[0])*p->nLSlot);
  if( p->aLTerm!=p->aLTermSpace ) sqlite3DbFreeNN(db, p->aLTerm);
  p->aLTerm = paNew;
  p->nLSlot = n;
  return SQLITE_OK;
}

/*
** Transfer content from the second pLoop into the first.
*/
static int whereLoopXfer(sqlite3 *db, WhereLoop *pTo, WhereLoop *pFrom){
  whereLoopClearUnion(db, pTo);
  if( whereLoopResize(db, pTo, pFrom->nLTerm) ){
    memset(&pTo->u, 0, sizeof(pTo->u));
    return SQLITE_NOMEM_BKPT;
  }
  memcpy(pTo, pFrom, WHERE_LOOP_XFER_SZ);
  memcpy(pTo->aLTerm, pFrom->aLTerm, pTo->nLTerm*sizeof(pTo->aLTerm[0]));
  if( pFrom->wsFlags & WHERE_VIRTUALTABLE ){
    pFrom->u.vtab.needFree = 0;
  }else if( (pFrom->wsFlags & WHERE_AUTO_INDEX)!=0 ){
    pFrom->u.btree.pIndex = 0;
  }
  return SQLITE_OK;
}

/*
** Delete a WhereLoop object
*/
static void whereLoopDelete(sqlite3 *db, WhereLoop *p){
  whereLoopClear(db, p);
  sqlite3DbFreeNN(db, p);
}

/*
** Free a WhereInfo structure
*/
static void whereInfoFree(sqlite3 *db, WhereInfo *pWInfo){
  int i;
  assert( pWInfo!=0 );
  for(i=0; i<pWInfo->nLevel; i++){
    WhereLevel *pLevel = &pWInfo->a[i];
    if( pLevel->pWLoop && (pLevel->pWLoop->wsFlags & WHERE_IN_ABLE) ){
      sqlite3DbFree(db, pLevel->u.in.aInLoop);
    }
  }
  sqlite3WhereClauseClear(&pWInfo->sWC);
  while( pWInfo->pLoops ){
    WhereLoop *p = pWInfo->pLoops;
    pWInfo->pLoops = p->pNextLoop;
    whereLoopDelete(db, p);
  }
  sqlite3DbFreeNN(db, pWInfo);
}

/*
** Return TRUE if all of the following are true:
**
**   (1)  X has the same or lower cost that Y
**   (2)  X uses fewer WHERE clause terms than Y
**   (3)  Every WHERE clause term used by X is also used by Y
**   (4)  X skips at least as many columns as Y
**   (5)  If X is a covering index, than Y is too
**
** Conditions (2) and (3) mean that X is a "proper subset" of Y.
** If X is a proper subset of Y then Y is a better choice and ought
** to have a lower cost.  This routine returns TRUE when that cost 
** relationship is inverted and needs to be adjusted.  Constraint (4)
** was added because if X uses skip-scan less than Y it still might
** deserve a lower cost even if it is a proper subset of Y.  Constraint (5)
** was added because a covering index probably deserves to have a lower cost
** than a non-covering index even if it is a proper subset.
*/
static int whereLoopCheaperProperSubset(
  const WhereLoop *pX,       /* First WhereLoop to compare */
  const WhereLoop *pY        /* Compare against this WhereLoop */
){
  int i, j;
  if( pX->nLTerm-pX->nSkip >= pY->nLTerm-pY->nSkip ){
    return 0; /* X is not a subset of Y */
  }
  if( pY->nSkip > pX->nSkip ) return 0;
  if( pX->rRun >= pY->rRun ){
    if( pX->rRun > pY->rRun ) return 0;    /* X costs more than Y */
    if( pX->nOut > pY->nOut ) return 0;    /* X costs more than Y */
  }
  for(i=pX->nLTerm-1; i>=0; i--){
    if( pX->aLTerm[i]==0 ) continue;
    for(j=pY->nLTerm-1; j>=0; j--){
      if( pY->aLTerm[j]==pX->aLTerm[i] ) break;
    }
    if( j<0 ) return 0;  /* X not a subset of Y since term X[i] not used by Y */
  }
  if( (pX->wsFlags&WHERE_IDX_ONLY)!=0 
   && (pY->wsFlags&WHERE_IDX_ONLY)==0 ){
    return 0;  /* Constraint (5) */
  }
  return 1;  /* All conditions meet */
}

/*
** Try to adjust the cost of WhereLoop pTemplate upwards or downwards so
** that:
**
**   (1) pTemplate costs less than any other WhereLoops that are a proper
**       subset of pTemplate
**
**   (2) pTemplate costs more than any other WhereLoops for which pTemplate
**       is a proper subset.
**
** To say "WhereLoop X is a proper subset of Y" means that X uses fewer
** WHERE clause terms than Y and that every WHERE clause term used by X is
** also used by Y.
*/
static void whereLoopAdjustCost(const WhereLoop *p, WhereLoop *pTemplate){
  if( (pTemplate->wsFlags & WHERE_INDEXED)==0 ) return;
  for(; p; p=p->pNextLoop){
    if( p->iTab!=pTemplate->iTab ) continue;
    if( (p->wsFlags & WHERE_INDEXED)==0 ) continue;
    if( whereLoopCheaperProperSubset(p, pTemplate) ){
      /* Adjust pTemplate cost downward so that it is cheaper than its 
      ** subset p. */
      WHERETRACE(0x80,("subset cost adjustment %d,%d to %d,%d\n",
                       pTemplate->rRun, pTemplate->nOut, p->rRun, p->nOut-1));
      pTemplate->rRun = p->rRun;
      pTemplate->nOut = p->nOut - 1;
    }else if( whereLoopCheaperProperSubset(pTemplate, p) ){
      /* Adjust pTemplate cost upward so that it is costlier than p since
      ** pTemplate is a proper subset of p */
      WHERETRACE(0x80,("subset cost adjustment %d,%d to %d,%d\n",
                       pTemplate->rRun, pTemplate->nOut, p->rRun, p->nOut+1));
      pTemplate->rRun = p->rRun;
      pTemplate->nOut = p->nOut + 1;
    }
  }
}

/*
** Search the list of WhereLoops in *ppPrev looking for one that can be
** replaced by pTemplate.
**
** Return NULL if pTemplate does not belong on the WhereLoop list.
** In other words if pTemplate ought to be dropped from further consideration.
**
** If pX is a WhereLoop that pTemplate can replace, then return the
** link that points to pX.
**
** If pTemplate cannot replace any existing element of the list but needs
** to be added to the list as a new entry, then return a pointer to the
** tail of the list.
*/
static WhereLoop **whereLoopFindLesser(
  WhereLoop **ppPrev,
  const WhereLoop *pTemplate
){
  WhereLoop *p;
  for(p=(*ppPrev); p; ppPrev=&p->pNextLoop, p=*ppPrev){
    if( p->iTab!=pTemplate->iTab || p->iSortIdx!=pTemplate->iSortIdx ){
      /* If either the iTab or iSortIdx values for two WhereLoop are different
      ** then those WhereLoops need to be considered separately.  Neither is
      ** a candidate to replace the other. */
      continue;
    }
    /* In the current implementation, the rSetup value is either zero
    ** or the cost of building an automatic index (NlogN) and the NlogN
    ** is the same for compatible WhereLoops. */
    assert( p->rSetup==0 || pTemplate->rSetup==0 
                 || p->rSetup==pTemplate->rSetup );

    /* whereLoopAddBtree() always generates and inserts the automatic index
    ** case first.  Hence compatible candidate WhereLoops never have a larger
    ** rSetup. Call this SETUP-INVARIANT */
    assert( p->rSetup>=pTemplate->rSetup );

    /* Any loop using an appliation-defined index (or PRIMARY KEY or
    ** UNIQUE constraint) with one or more == constraints is better
    ** than an automatic index. Unless it is a skip-scan. */
    if( (p->wsFlags & WHERE_AUTO_INDEX)!=0
     && (pTemplate->nSkip)==0
     && (pTemplate->wsFlags & WHERE_INDEXED)!=0
     && (pTemplate->wsFlags & WHERE_COLUMN_EQ)!=0
     && (p->prereq & pTemplate->prereq)==pTemplate->prereq
    ){
      break;
    }

    /* If existing WhereLoop p is better than pTemplate, pTemplate can be
    ** discarded.  WhereLoop p is better if:
    **   (1)  p has no more dependencies than pTemplate, and
    **   (2)  p has an equal or lower cost than pTemplate
    */
    if( (p->prereq & pTemplate->prereq)==p->prereq    /* (1)  */
     && p->rSetup<=pTemplate->rSetup                  /* (2a) */
     && p->rRun<=pTemplate->rRun                      /* (2b) */
     && p->nOut<=pTemplate->nOut                      /* (2c) */
    ){
      return 0;  /* Discard pTemplate */
    }

    /* If pTemplate is always better than p, then cause p to be overwritten
    ** with pTemplate.  pTemplate is better than p if:
    **   (1)  pTemplate has no more dependences than p, and
    **   (2)  pTemplate has an equal or lower cost than p.
    */
    if( (p->prereq & pTemplate->prereq)==pTemplate->prereq   /* (1)  */
     && p->rRun>=pTemplate->rRun                             /* (2a) */
     && p->nOut>=pTemplate->nOut                             /* (2b) */
    ){
      assert( p->rSetup>=pTemplate->rSetup ); /* SETUP-INVARIANT above */
      break;   /* Cause p to be overwritten by pTemplate */
    }
  }
  return ppPrev;
}

/*
** Insert or replace a WhereLoop entry using the template supplied.
**
** An existing WhereLoop entry might be overwritten if the new template
** is better and has fewer dependencies.  Or the template will be ignored
** and no insert will occur if an existing WhereLoop is faster and has
** fewer dependencies than the template.  Otherwise a new WhereLoop is
** added based on the template.
**
** If pBuilder->pOrSet is not NULL then we care about only the
** prerequisites and rRun and nOut costs of the N best loops.  That
** information is gathered in the pBuilder->pOrSet object.  This special
** processing mode is used only for OR clause processing.
**
** When accumulating multiple loops (when pBuilder->pOrSet is NULL) we
** still might overwrite similar loops with the new template if the
** new template is better.  Loops may be overwritten if the following 
** conditions are met:
**
**    (1)  They have the same iTab.
**    (2)  They have the same iSortIdx.
**    (3)  The template has same or fewer dependencies than the current loop
**    (4)  The template has the same or lower cost than the current loop
*/
static int whereLoopInsert(WhereLoopBuilder *pBuilder, WhereLoop *pTemplate){
  WhereLoop **ppPrev, *p;
  WhereInfo *pWInfo = pBuilder->pWInfo;
  sqlite3 *db = pWInfo->pParse->db;
  int rc;

  /* Stop the search once we hit the query planner search limit */
  if( pBuilder->iPlanLimit==0 ){
    WHERETRACE(0xffffffff,("=== query planner search limit reached ===\n"));
    if( pBuilder->pOrSet ) pBuilder->pOrSet->n = 0;
    return SQLITE_DONE;
  }
  pBuilder->iPlanLimit--;

  /* If pBuilder->pOrSet is defined, then only keep track of the costs
  ** and prereqs.
  */
  if( pBuilder->pOrSet!=0 ){
    if( pTemplate->nLTerm ){
#if WHERETRACE_ENABLED
      u16 n = pBuilder->pOrSet->n;
      int x =
#endif
      whereOrInsert(pBuilder->pOrSet, pTemplate->prereq, pTemplate->rRun,
                                    pTemplate->nOut);
#if WHERETRACE_ENABLED /* 0x8 */
      if( sqlite3WhereTrace & 0x8 ){
        sqlite3DebugPrintf(x?"   or-%d:  ":"   or-X:  ", n);
        whereLoopPrint(pTemplate, pBuilder->pWC);
      }
#endif
    }
    return SQLITE_OK;
  }

  /* Look for an existing WhereLoop to replace with pTemplate
  */
  whereLoopAdjustCost(pWInfo->pLoops, pTemplate);
  ppPrev = whereLoopFindLesser(&pWInfo->pLoops, pTemplate);

  if( ppPrev==0 ){
    /* There already exists a WhereLoop on the list that is better
    ** than pTemplate, so just ignore pTemplate */
#if WHERETRACE_ENABLED /* 0x8 */
    if( sqlite3WhereTrace & 0x8 ){
      sqlite3DebugPrintf("   skip: ");
      whereLoopPrint(pTemplate, pBuilder->pWC);
    }
#endif
    return SQLITE_OK;  
  }else{
    p = *ppPrev;
  }

  /* If we reach this point it means that either p[] should be overwritten
  ** with pTemplate[] if p[] exists, or if p==NULL then allocate a new
  ** WhereLoop and insert it.
  */
#if WHERETRACE_ENABLED /* 0x8 */
  if( sqlite3WhereTrace & 0x8 ){
    if( p!=0 ){
      sqlite3DebugPrintf("replace: ");
      whereLoopPrint(p, pBuilder->pWC);
      sqlite3DebugPrintf("   with: ");
    }else{
      sqlite3DebugPrintf("    add: ");
    }
    whereLoopPrint(pTemplate, pBuilder->pWC);
  }
#endif
  if( p==0 ){
    /* Allocate a new WhereLoop to add to the end of the list */
    *ppPrev = p = sqlite3DbMallocRawNN(db, sizeof(WhereLoop));
    if( p==0 ) return SQLITE_NOMEM_BKPT;
    whereLoopInit(p);
    p->pNextLoop = 0;
  }else{
    /* We will be overwriting WhereLoop p[].  But before we do, first
    ** go through the rest of the list and delete any other entries besides
    ** p[] that are also supplated by pTemplate */
    WhereLoop **ppTail = &p->pNextLoop;
    WhereLoop *pToDel;
    while( *ppTail ){
      ppTail = whereLoopFindLesser(ppTail, pTemplate);
      if( ppTail==0 ) break;
      pToDel = *ppTail;
      if( pToDel==0 ) break;
      *ppTail = pToDel->pNextLoop;
#if WHERETRACE_ENABLED /* 0x8 */
      if( sqlite3WhereTrace & 0x8 ){
        sqlite3DebugPrintf(" delete: ");
        whereLoopPrint(pToDel, pBuilder->pWC);
      }
#endif
      whereLoopDelete(db, pToDel);
    }
  }
  rc = whereLoopXfer(db, p, pTemplate);
  if( (p->wsFlags & WHERE_VIRTUALTABLE)==0 ){
    Index *pIndex = p->u.btree.pIndex;
    if( pIndex && pIndex->idxType==SQLITE_IDXTYPE_IPK ){
      p->u.btree.pIndex = 0;
    }
  }
  return rc;
}

/*
** Adjust the WhereLoop.nOut value downward to account for terms of the
** WHERE clause that reference the loop but which are not used by an
** index.
*
** For every WHERE clause term that is not used by the index
** and which has a truth probability assigned by one of the likelihood(),
** likely(), or unlikely() SQL functions, reduce the estimated number
** of output rows by the probability specified.
**
** TUNING:  For every WHERE clause term that is not used by the index
** and which does not have an assigned truth probability, heuristics
** described below are used to try to estimate the truth probability.
** TODO --> Perhaps this is something that could be improved by better
** table statistics.
**
** Heuristic 1:  Estimate the truth probability as 93.75%.  The 93.75%
** value corresponds to -1 in LogEst notation, so this means decrement
** the WhereLoop.nOut field for every such WHERE clause term.
**
** Heuristic 2:  If there exists one or more WHERE clause terms of the
** form "x==EXPR" and EXPR is not a constant 0 or 1, then make sure the
** final output row estimate is no greater than 1/4 of the total number
** of rows in the table.  In other words, assume that x==EXPR will filter
** out at least 3 out of 4 rows.  If EXPR is -1 or 0 or 1, then maybe the
** "x" column is boolean or else -1 or 0 or 1 is a common default value
** on the "x" column and so in that case only cap the output row estimate
** at 1/2 instead of 1/4.
*/
static void whereLoopOutputAdjust(
  WhereClause *pWC,      /* The WHERE clause */
  WhereLoop *pLoop,      /* The loop to adjust downward */
  LogEst nRow            /* Number of rows in the entire table */
){
  WhereTerm *pTerm, *pX;
  Bitmask notAllowed = ~(pLoop->prereq|pLoop->maskSelf);
  int i, j;
  LogEst iReduce = 0;    /* pLoop->nOut should not exceed nRow-iReduce */

  assert( (pLoop->wsFlags & WHERE_AUTO_INDEX)==0 );
  for(i=pWC->nTerm, pTerm=pWC->a; i>0; i--, pTerm++){
    assert( pTerm!=0 );
    if( (pTerm->wtFlags & TERM_VIRTUAL)!=0 ) break;
    if( (pTerm->prereqAll & pLoop->maskSelf)==0 ) continue;
    if( (pTerm->prereqAll & notAllowed)!=0 ) continue;
    for(j=pLoop->nLTerm-1; j>=0; j--){
      pX = pLoop->aLTerm[j];
      if( pX==0 ) continue;
      if( pX==pTerm ) break;
      if( pX->iParent>=0 && (&pWC->a[pX->iParent])==pTerm ) break;
    }
    if( j<0 ){
      if( pTerm->truthProb<=0 ){
        /* If a truth probability is specified using the likelihood() hints,
        ** then use the probability provided by the application. */
        pLoop->nOut += pTerm->truthProb;
      }else{
        /* In the absence of explicit truth probabilities, use heuristics to
        ** guess a reasonable truth probability. */
        pLoop->nOut--;
        if( pTerm->eOperator&(WO_EQ|WO_IS) ){
          Expr *pRight = pTerm->pExpr->pRight;
          int k = 0;
          testcase( pTerm->pExpr->op==TK_IS );
          if( sqlite3ExprIsInteger(pRight, &k) && k>=(-1) && k<=1 ){
            k = 10;
          }else{
            k = 20;
          }
          if( iReduce<k ) iReduce = k;
        }
      }
    }
  }
  if( pLoop->nOut > nRow-iReduce )  pLoop->nOut = nRow - iReduce;
}

/* 
** Term pTerm is a vector range comparison operation. The first comparison
** in the vector can be optimized using column nEq of the index. This
** function returns the total number of vector elements that can be used
** as part of the range comparison.
**
** For example, if the query is:
**
**   WHERE a = ? AND (b, c, d) > (?, ?, ?)
**
** and the index:
**
**   CREATE INDEX ... ON (a, b, c, d, e)
**
** then this function would be invoked with nEq=1. The value returned in
** this case is 3.
*/
static int whereRangeVectorLen(
  Parse *pParse,       /* Parsing context */
  int iCur,            /* Cursor open on pIdx */
  Index *pIdx,         /* The index to be used for a inequality constraint */
  int nEq,             /* Number of prior equality constraints on same index */
  WhereTerm *pTerm     /* The vector inequality constraint */
){
  int nCmp = sqlite3ExprVectorSize(pTerm->pExpr->pLeft);
  int i;

  nCmp = MIN(nCmp, (pIdx->nColumn - nEq));
  for(i=1; i<nCmp; i++){
    /* Test if comparison i of pTerm is compatible with column (i+nEq) 
    ** of the index. If not, exit the loop.  */
    char aff;                     /* Comparison affinity */
    char idxaff = 0;              /* Indexed columns affinity */
    CollSeq *pColl;               /* Comparison collation sequence */
    Expr *pLhs = pTerm->pExpr->pLeft->x.pList->a[i].pExpr;
    Expr *pRhs = pTerm->pExpr->pRight;
    if( pRhs->flags & EP_xIsSelect ){
      pRhs = pRhs->x.pSelect->pEList->a[i].pExpr;
    }else{
      pRhs = pRhs->x.pList->a[i].pExpr;
    }

    /* Check that the LHS of the comparison is a column reference to
    ** the right column of the right source table. And that the sort
    ** order of the index column is the same as the sort order of the
    ** leftmost index column.  */
    if( pLhs->op!=TK_COLUMN 
     || pLhs->iTable!=iCur 
     || pLhs->iColumn!=pIdx->aiColumn[i+nEq] 
     || pIdx->aSortOrder[i+nEq]!=pIdx->aSortOrder[nEq]
    ){
      break;
    }

    testcase( pLhs->iColumn==XN_ROWID );
    aff = sqlite3CompareAffinity(pRhs, sqlite3ExprAffinity(pLhs));
    idxaff = sqlite3TableColumnAffinity(pIdx->pTable, pLhs->iColumn);
    if( aff!=idxaff ) break;

    pColl = sqlite3BinaryCompareCollSeq(pParse, pLhs, pRhs);
    if( pColl==0 ) break;
    if( sqlite3StrICmp(pColl->zName, pIdx->azColl[i+nEq]) ) break;
  }
  return i;
}

/*
** Adjust the cost C by the costMult facter T.  This only occurs if
** compiled with -DSQLITE_ENABLE_COSTMULT
*/
#ifdef SQLITE_ENABLE_COSTMULT
# define ApplyCostMultiplier(C,T)  C += T
#else
# define ApplyCostMultiplier(C,T)
#endif

/*
** We have so far matched pBuilder->pNew->u.btree.nEq terms of the 
** index pIndex. Try to match one more.
**
** When this function is called, pBuilder->pNew->nOut contains the 
** number of rows expected to be visited by filtering using the nEq 
** terms only. If it is modified, this value is restored before this 
** function returns.
**
** If pProbe->idxType==SQLITE_IDXTYPE_IPK, that means pIndex is 
** a fake index used for the INTEGER PRIMARY KEY.
*/
static int whereLoopAddBtreeIndex(
  WhereLoopBuilder *pBuilder,     /* The WhereLoop factory */
  struct SrcList_item *pSrc,      /* FROM clause term being analyzed */
  Index *pProbe,                  /* An index on pSrc */
  LogEst nInMul                   /* log(Number of iterations due to IN) */
){
  WhereInfo *pWInfo = pBuilder->pWInfo;  /* WHERE analyse context */
  Parse *pParse = pWInfo->pParse;        /* Parsing context */
  sqlite3 *db = pParse->db;       /* Database connection malloc context */
  WhereLoop *pNew;                /* Template WhereLoop under construction */
  WhereTerm *pTerm;               /* A WhereTerm under consideration */
  int opMask;                     /* Valid operators for constraints */
  WhereScan scan;                 /* Iterator for WHERE terms */
  Bitmask saved_prereq;           /* Original value of pNew->prereq */
  u16 saved_nLTerm;               /* Original value of pNew->nLTerm */
  u16 saved_nEq;                  /* Original value of pNew->u.btree.nEq */
  u16 saved_nBtm;                 /* Original value of pNew->u.btree.nBtm */
  u16 saved_nTop;                 /* Original value of pNew->u.btree.nTop */
  u16 saved_nSkip;                /* Original value of pNew->nSkip */
  u32 saved_wsFlags;              /* Original value of pNew->wsFlags */
  LogEst saved_nOut;              /* Original value of pNew->nOut */
  int rc = SQLITE_OK;             /* Return code */
  LogEst rSize;                   /* Number of rows in the table */
  LogEst rLogSize;                /* Logarithm of table size */
  WhereTerm *pTop = 0, *pBtm = 0; /* Top and bottom range constraints */

  pNew = pBuilder->pNew;
  if( db->mallocFailed ) return SQLITE_NOMEM_BKPT;
  WHERETRACE(0x800, ("BEGIN %s.addBtreeIdx(%s), nEq=%d\n",
                     pProbe->pTable->zName,pProbe->zName, pNew->u.btree.nEq));

  assert( (pNew->wsFlags & WHERE_VIRTUALTABLE)==0 );
  assert( (pNew->wsFlags & WHERE_TOP_LIMIT)==0 );
  if( pNew->wsFlags & WHERE_BTM_LIMIT ){
    opMask = WO_LT|WO_LE;
  }else{
    assert( pNew->u.btree.nBtm==0 );
    opMask = WO_EQ|WO_IN|WO_GT|WO_GE|WO_LT|WO_LE|WO_ISNULL|WO_IS;
  }
  if( pProbe->bUnordered ) opMask &= ~(WO_GT|WO_GE|WO_LT|WO_LE);

  assert( pNew->u.btree.nEq<pProbe->nColumn );

  saved_nEq = pNew->u.btree.nEq;
  saved_nBtm = pNew->u.btree.nBtm;
  saved_nTop = pNew->u.btree.nTop;
  saved_nSkip = pNew->nSkip;
  saved_nLTerm = pNew->nLTerm;
  saved_wsFlags = pNew->wsFlags;
  saved_prereq = pNew->prereq;
  saved_nOut = pNew->nOut;
  pTerm = whereScanInit(&scan, pBuilder->pWC, pSrc->iCursor, saved_nEq,
                        opMask, pProbe);
  pNew->rSetup = 0;
  rSize = pProbe->aiRowLogEst[0];
  rLogSize = estLog(rSize);
  for(; rc==SQLITE_OK && pTerm!=0; pTerm = whereScanNext(&scan)){
    u16 eOp = pTerm->eOperator;   /* Shorthand for pTerm->eOperator */
    LogEst rCostIdx;
    LogEst nOutUnadjusted;        /* nOut before IN() and WHERE adjustments */
    int nIn = 0;
#ifdef SQLITE_ENABLE_STAT4
    int nRecValid = pBuilder->nRecValid;
#endif
    if( (eOp==WO_ISNULL || (pTerm->wtFlags&TERM_VNULL)!=0)
     && indexColumnNotNull(pProbe, saved_nEq)
    ){
      continue; /* ignore IS [NOT] NULL constraints on NOT NULL columns */
    }
    if( pTerm->prereqRight & pNew->maskSelf ) continue;

    /* Do not allow the upper bound of a LIKE optimization range constraint
    ** to mix with a lower range bound from some other source */
    if( pTerm->wtFlags & TERM_LIKEOPT && pTerm->eOperator==WO_LT ) continue;

    /* Do not allow constraints from the WHERE clause to be used by the
    ** right table of a LEFT JOIN.  Only constraints in the ON clause are
    ** allowed */
    if( (pSrc->fg.jointype & JT_LEFT)!=0
     && !ExprHasProperty(pTerm->pExpr, EP_FromJoin)
    ){
      continue;
    }

    if( IsUniqueIndex(pProbe) && saved_nEq==pProbe->nKeyCol-1 ){
      pBuilder->bldFlags |= SQLITE_BLDF_UNIQUE;
    }else{
      pBuilder->bldFlags |= SQLITE_BLDF_INDEXED;
    }
    pNew->wsFlags = saved_wsFlags;
    pNew->u.btree.nEq = saved_nEq;
    pNew->u.btree.nBtm = saved_nBtm;
    pNew->u.btree.nTop = saved_nTop;
    pNew->nLTerm = saved_nLTerm;
    if( whereLoopResize(db, pNew, pNew->nLTerm+1) ) break; /* OOM */
    pNew->aLTerm[pNew->nLTerm++] = pTerm;
    pNew->prereq = (saved_prereq | pTerm->prereqRight) & ~pNew->maskSelf;

    assert( nInMul==0
        || (pNew->wsFlags & WHERE_COLUMN_NULL)!=0 
        || (pNew->wsFlags & WHERE_COLUMN_IN)!=0 
        || (pNew->wsFlags & WHERE_SKIPSCAN)!=0 
    );

    if( eOp & WO_IN ){
      Expr *pExpr = pTerm->pExpr;
      if( ExprHasProperty(pExpr, EP_xIsSelect) ){
        /* "x IN (SELECT ...)":  TUNING: the SELECT returns 25 rows */
        int i;
        nIn = 46;  assert( 46==sqlite3LogEst(25) );

        /* The expression may actually be of the form (x, y) IN (SELECT...).
        ** In this case there is a separate term for each of (x) and (y).
        ** However, the nIn multiplier should only be applied once, not once
        ** for each such term. The following loop checks that pTerm is the
        ** first such term in use, and sets nIn back to 0 if it is not. */
        for(i=0; i<pNew->nLTerm-1; i++){
          if( pNew->aLTerm[i] && pNew->aLTerm[i]->pExpr==pExpr ) nIn = 0;
        }
      }else if( ALWAYS(pExpr->x.pList && pExpr->x.pList->nExpr) ){
        /* "x IN (value, value, ...)" */
        nIn = sqlite3LogEst(pExpr->x.pList->nExpr);
      }
      if( pProbe->hasStat1 ){
        LogEst M, logK, safetyMargin;
        /* Let:
        **   N = the total number of rows in the table
        **   K = the number of entries on the RHS of the IN operator
        **   M = the number of rows in the table that match terms to the 
        **       to the left in the same index.  If the IN operator is on
        **       the left-most index column, M==N.
        **
        ** Given the definitions above, it is better to omit the IN operator
        ** from the index lookup and instead do a scan of the M elements,
        ** testing each scanned row against the IN operator separately, if:
        **
        **        M*log(K) < K*log(N)
        **
        ** Our estimates for M, K, and N might be inaccurate, so we build in
        ** a safety margin of 2 (LogEst: 10) that favors using the IN operator
        ** with the index, as using an index has better worst-case behavior.
        ** If we do not have real sqlite_stat1 data, always prefer to use
        ** the index.
        */
        M = pProbe->aiRowLogEst[saved_nEq];
        logK = estLog(nIn);
        safetyMargin = 10;  /* TUNING: extra weight for indexed IN */
        if( M + logK + safetyMargin < nIn + rLogSize ){
          WHERETRACE(0x40,
            ("Scan preferred over IN operator on column %d of \"%s\" (%d<%d)\n",
             saved_nEq, pProbe->zName, M+logK+10, nIn+rLogSize));
          continue;
        }else{
          WHERETRACE(0x40,
            ("IN operator preferred on column %d of \"%s\" (%d>=%d)\n",
             saved_nEq, pProbe->zName, M+logK+10, nIn+rLogSize));
        }
      }
      pNew->wsFlags |= WHERE_COLUMN_IN;
    }else if( eOp & (WO_EQ|WO_IS) ){
      int iCol = pProbe->aiColumn[saved_nEq];
      pNew->wsFlags |= WHERE_COLUMN_EQ;
      assert( saved_nEq==pNew->u.btree.nEq );
      if( iCol==XN_ROWID 
       || (iCol>=0 && nInMul==0 && saved_nEq==pProbe->nKeyCol-1)
      ){
        if( iCol==XN_ROWID || pProbe->uniqNotNull 
         || (pProbe->nKeyCol==1 && pProbe->onError && eOp==WO_EQ) 
        ){
          pNew->wsFlags |= WHERE_ONEROW;
        }else{
          pNew->wsFlags |= WHERE_UNQ_WANTED;
        }
      }
    }else if( eOp & WO_ISNULL ){
      pNew->wsFlags |= WHERE_COLUMN_NULL;
    }else if( eOp & (WO_GT|WO_GE) ){
      testcase( eOp & WO_GT );
      testcase( eOp & WO_GE );
      pNew->wsFlags |= WHERE_COLUMN_RANGE|WHERE_BTM_LIMIT;
      pNew->u.btree.nBtm = whereRangeVectorLen(
          pParse, pSrc->iCursor, pProbe, saved_nEq, pTerm
      );
      pBtm = pTerm;
      pTop = 0;
      if( pTerm->wtFlags & TERM_LIKEOPT ){
        /* Range contraints that come from the LIKE optimization are
        ** always used in pairs. */
        pTop = &pTerm[1];
        assert( (pTop-(pTerm->pWC->a))<pTerm->pWC->nTerm );
        assert( pTop->wtFlags & TERM_LIKEOPT );
        assert( pTop->eOperator==WO_LT );
        if( whereLoopResize(db, pNew, pNew->nLTerm+1) ) break; /* OOM */
        pNew->aLTerm[pNew->nLTerm++] = pTop;
        pNew->wsFlags |= WHERE_TOP_LIMIT;
        pNew->u.btree.nTop = 1;
      }
    }else{
      assert( eOp & (WO_LT|WO_LE) );
      testcase( eOp & WO_LT );
      testcase( eOp & WO_LE );
      pNew->wsFlags |= WHERE_COLUMN_RANGE|WHERE_TOP_LIMIT;
      pNew->u.btree.nTop = whereRangeVectorLen(
          pParse, pSrc->iCursor, pProbe, saved_nEq, pTerm
      );
      pTop = pTerm;
      pBtm = (pNew->wsFlags & WHERE_BTM_LIMIT)!=0 ?
                     pNew->aLTerm[pNew->nLTerm-2] : 0;
    }

    /* At this point pNew->nOut is set to the number of rows expected to
    ** be visited by the index scan before considering term pTerm, or the
    ** values of nIn and nInMul. In other words, assuming that all 
    ** "x IN(...)" terms are replaced with "x = ?". This block updates
    ** the value of pNew->nOut to account for pTerm (but not nIn/nInMul).  */
    assert( pNew->nOut==saved_nOut );
    if( pNew->wsFlags & WHERE_COLUMN_RANGE ){
      /* Adjust nOut using stat4 data. Or, if there is no stat4
      ** data, using some other estimate.  */
      whereRangeScanEst(pParse, pBuilder, pBtm, pTop, pNew);
    }else{
      int nEq = ++pNew->u.btree.nEq;
      assert( eOp & (WO_ISNULL|WO_EQ|WO_IN|WO_IS) );

      assert( pNew->nOut==saved_nOut );
      if( pTerm->truthProb<=0 && pProbe->aiColumn[saved_nEq]>=0 ){
        assert( (eOp & WO_IN) || nIn==0 );
        testcase( eOp & WO_IN );
        pNew->nOut += pTerm->truthProb;
        pNew->nOut -= nIn;
      }else{
#ifdef SQLITE_ENABLE_STAT4
        tRowcnt nOut = 0;
        if( nInMul==0 
         && pProbe->nSample 
         && pNew->u.btree.nEq<=pProbe->nSampleCol
         && ((eOp & WO_IN)==0 || !ExprHasProperty(pTerm->pExpr, EP_xIsSelect))
         && OptimizationEnabled(db, SQLITE_Stat4)
        ){
          Expr *pExpr = pTerm->pExpr;
          if( (eOp & (WO_EQ|WO_ISNULL|WO_IS))!=0 ){
            testcase( eOp & WO_EQ );
            testcase( eOp & WO_IS );
            testcase( eOp & WO_ISNULL );
            rc = whereEqualScanEst(pParse, pBuilder, pExpr->pRight, &nOut);
          }else{
            rc = whereInScanEst(pParse, pBuilder, pExpr->x.pList, &nOut);
          }
          if( rc==SQLITE_NOTFOUND ) rc = SQLITE_OK;
          if( rc!=SQLITE_OK ) break;          /* Jump out of the pTerm loop */
          if( nOut ){
            pNew->nOut = sqlite3LogEst(nOut);
            if( pNew->nOut>saved_nOut ) pNew->nOut = saved_nOut;
            pNew->nOut -= nIn;
          }
        }
        if( nOut==0 )
#endif
        {
          pNew->nOut += (pProbe->aiRowLogEst[nEq] - pProbe->aiRowLogEst[nEq-1]);
          if( eOp & WO_ISNULL ){
            /* TUNING: If there is no likelihood() value, assume that a 
            ** "col IS NULL" expression matches twice as many rows 
            ** as (col=?). */
            pNew->nOut += 10;
          }
        }
      }
    }

    /* Set rCostIdx to the cost of visiting selected rows in index. Add
    ** it to pNew->rRun, which is currently set to the cost of the index
    ** seek only. Then, if this is a non-covering index, add the cost of
    ** visiting the rows in the main table.  */
    assert( pSrc->pTab->szTabRow>0 );
    rCostIdx = pNew->nOut + 1 + (15*pProbe->szIdxRow)/pSrc->pTab->szTabRow;
    pNew->rRun = sqlite3LogEstAdd(rLogSize, rCostIdx);
    if( (pNew->wsFlags & (WHERE_IDX_ONLY|WHERE_IPK))==0 ){
      pNew->rRun = sqlite3LogEstAdd(pNew->rRun, pNew->nOut + 16);
    }
    ApplyCostMultiplier(pNew->rRun, pProbe->pTable->costMult);

    nOutUnadjusted = pNew->nOut;
    pNew->rRun += nInMul + nIn;
    pNew->nOut += nInMul + nIn;
    whereLoopOutputAdjust(pBuilder->pWC, pNew, rSize);
    rc = whereLoopInsert(pBuilder, pNew);

    if( pNew->wsFlags & WHERE_COLUMN_RANGE ){
      pNew->nOut = saved_nOut;
    }else{
      pNew->nOut = nOutUnadjusted;
    }

    if( (pNew->wsFlags & WHERE_TOP_LIMIT)==0
     && pNew->u.btree.nEq<pProbe->nColumn
    ){
      whereLoopAddBtreeIndex(pBuilder, pSrc, pProbe, nInMul+nIn);
    }
    pNew->nOut = saved_nOut;
#ifdef SQLITE_ENABLE_STAT4
    pBuilder->nRecValid = nRecValid;
#endif
  }
  pNew->prereq = saved_prereq;
  pNew->u.btree.nEq = saved_nEq;
  pNew->u.btree.nBtm = saved_nBtm;
  pNew->u.btree.nTop = saved_nTop;
  pNew->nSkip = saved_nSkip;
  pNew->wsFlags = saved_wsFlags;
  pNew->nOut = saved_nOut;
  pNew->nLTerm = saved_nLTerm;

  /* Consider using a skip-scan if there are no WHERE clause constraints
  ** available for the left-most terms of the index, and if the average
  ** number of repeats in the left-most terms is at least 18. 
  **
  ** The magic number 18 is selected on the basis that scanning 17 rows
  ** is almost always quicker than an index seek (even though if the index
  ** contains fewer than 2^17 rows we assume otherwise in other parts of
  ** the code). And, even if it is not, it should not be too much slower. 
  ** On the other hand, the extra seeks could end up being significantly
  ** more expensive.  */
  assert( 42==sqlite3LogEst(18) );
  if( saved_nEq==saved_nSkip
   && saved_nEq+1<pProbe->nKeyCol
   && pProbe->noSkipScan==0
   && OptimizationEnabled(db, SQLITE_SkipScan)
   && pProbe->aiRowLogEst[saved_nEq+1]>=42  /* TUNING: Minimum for skip-scan */
   && (rc = whereLoopResize(db, pNew, pNew->nLTerm+1))==SQLITE_OK
  ){
    LogEst nIter;
    pNew->u.btree.nEq++;
    pNew->nSkip++;
    pNew->aLTerm[pNew->nLTerm++] = 0;
    pNew->wsFlags |= WHERE_SKIPSCAN;
    nIter = pProbe->aiRowLogEst[saved_nEq] - pProbe->aiRowLogEst[saved_nEq+1];
    pNew->nOut -= nIter;
    /* TUNING:  Because uncertainties in the estimates for skip-scan queries,
    ** add a 1.375 fudge factor to make skip-scan slightly less likely. */
    nIter += 5;
    whereLoopAddBtreeIndex(pBuilder, pSrc, pProbe, nIter + nInMul);
    pNew->nOut = saved_nOut;
    pNew->u.btree.nEq = saved_nEq;
    pNew->nSkip = saved_nSkip;
    pNew->wsFlags = saved_wsFlags;
  }

  WHERETRACE(0x800, ("END %s.addBtreeIdx(%s), nEq=%d, rc=%d\n",
                      pProbe->pTable->zName, pProbe->zName, saved_nEq, rc));
  return rc;
}

/*
** Return True if it is possible that pIndex might be useful in
** implementing the ORDER BY clause in pBuilder.
**
** Return False if pBuilder does not contain an ORDER BY clause or
** if there is no way for pIndex to be useful in implementing that
** ORDER BY clause.
*/
static int indexMightHelpWithOrderBy(
  WhereLoopBuilder *pBuilder,
  Index *pIndex,
  int iCursor
){
  ExprList *pOB;
  ExprList *aColExpr;
  int ii, jj;

  if( pIndex->bUnordered ) return 0;
  if( (pOB = pBuilder->pWInfo->pOrderBy)==0 ) return 0;
  for(ii=0; ii<pOB->nExpr; ii++){
    Expr *pExpr = sqlite3ExprSkipCollateAndLikely(pOB->a[ii].pExpr);
    if( pExpr->op==TK_COLUMN && pExpr->iTable==iCursor ){
      if( pExpr->iColumn<0 ) return 1;
      for(jj=0; jj<pIndex->nKeyCol; jj++){
        if( pExpr->iColumn==pIndex->aiColumn[jj] ) return 1;
      }
    }else if( (aColExpr = pIndex->aColExpr)!=0 ){
      for(jj=0; jj<pIndex->nKeyCol; jj++){
        if( pIndex->aiColumn[jj]!=XN_EXPR ) continue;
        if( sqlite3ExprCompareSkip(pExpr,aColExpr->a[jj].pExpr,iCursor)==0 ){
          return 1;
        }
      }
    }
  }
  return 0;
}

/* Check to see if a partial index with pPartIndexWhere can be used
** in the current query.  Return true if it can be and false if not.
*/
static int whereUsablePartialIndex(int iTab, WhereClause *pWC, Expr *pWhere){
  int i;
  WhereTerm *pTerm;
  Parse *pParse = pWC->pWInfo->pParse;
  while( pWhere->op==TK_AND ){
    if( !whereUsablePartialIndex(iTab,pWC,pWhere->pLeft) ) return 0;
    pWhere = pWhere->pRight;
  }
  if( pParse->db->flags & SQLITE_EnableQPSG ) pParse = 0;
  for(i=0, pTerm=pWC->a; i<pWC->nTerm; i++, pTerm++){
    Expr *pExpr;
    if( pTerm->wtFlags & TERM_NOPARTIDX ) continue;
    pExpr = pTerm->pExpr;
    if( (!ExprHasProperty(pExpr, EP_FromJoin) || pExpr->iRightJoinTable==iTab)
     && sqlite3ExprImpliesExpr(pParse, pExpr, pWhere, iTab) 
    ){
      return 1;
    }
  }
  return 0;
}

/*
** Add all WhereLoop objects for a single table of the join where the table
** is identified by pBuilder->pNew->iTab.  That table is guaranteed to be
** a b-tree table, not a virtual table.
**
** The costs (WhereLoop.rRun) of the b-tree loops added by this function
** are calculated as follows:
**
** For a full scan, assuming the table (or index) contains nRow rows:
**
**     cost = nRow * 3.0                    // full-table scan
**     cost = nRow * K                      // scan of covering index
**     cost = nRow * (K+3.0)                // scan of non-covering index
**
** where K is a value between 1.1 and 3.0 set based on the relative 
** estimated average size of the index and table records.
**
** For an index scan, where nVisit is the number of index rows visited
** by the scan, and nSeek is the number of seek operations required on 
** the index b-tree:
**
**     cost = nSeek * (log(nRow) + K * nVisit)          // covering index
**     cost = nSeek * (log(nRow) + (K+3.0) * nVisit)    // non-covering index
**
** Normally, nSeek is 1. nSeek values greater than 1 come about if the 
** WHERE clause includes "x IN (....)" terms used in place of "x=?". Or when 
** implicit "x IN (SELECT x FROM tbl)" terms are added for skip-scans.
**
** The estimated values (nRow, nVisit, nSeek) often contain a large amount
** of uncertainty.  For this reason, scoring is designed to pick plans that
** "do the least harm" if the estimates are inaccurate.  For example, a
** log(nRow) factor is omitted from a non-covering index scan in order to
** bias the scoring in favor of using an index, since the worst-case
** performance of using an index is far better than the worst-case performance
** of a full table scan.
*/
static int whereLoopAddBtree(
  WhereLoopBuilder *pBuilder, /* WHERE clause information */
  Bitmask mPrereq             /* Extra prerequesites for using this table */
){
  WhereInfo *pWInfo;          /* WHERE analysis context */
  Index *pProbe;              /* An index we are evaluating */
  Index sPk;                  /* A fake index object for the primary key */
  LogEst aiRowEstPk[2];       /* The aiRowLogEst[] value for the sPk index */
  i16 aiColumnPk = -1;        /* The aColumn[] value for the sPk index */
  SrcList *pTabList;          /* The FROM clause */
  struct SrcList_item *pSrc;  /* The FROM clause btree term to add */
  WhereLoop *pNew;            /* Template WhereLoop object */
  int rc = SQLITE_OK;         /* Return code */
  int iSortIdx = 1;           /* Index number */
  int b;                      /* A boolean value */
  LogEst rSize;               /* number of rows in the table */
  LogEst rLogSize;            /* Logarithm of the number of rows in the table */
  WhereClause *pWC;           /* The parsed WHERE clause */
  Table *pTab;                /* Table being queried */
  
  pNew = pBuilder->pNew;
  pWInfo = pBuilder->pWInfo;
  pTabList = pWInfo->pTabList;
  pSrc = pTabList->a + pNew->iTab;
  pTab = pSrc->pTab;
  pWC = pBuilder->pWC;
  assert( !IsVirtual(pSrc->pTab) );

  if( pSrc->pIBIndex ){
    /* An INDEXED BY clause specifies a particular index to use */
    pProbe = pSrc->pIBIndex;
  }else if( !HasRowid(pTab) ){
    pProbe = pTab->pIndex;
  }else{
    /* There is no INDEXED BY clause.  Create a fake Index object in local
    ** variable sPk to represent the rowid primary key index.  Make this
    ** fake index the first in a chain of Index objects with all of the real
    ** indices to follow */
    Index *pFirst;                  /* First of real indices on the table */
    memset(&sPk, 0, sizeof(Index));
    sPk.nKeyCol = 1;
    sPk.nColumn = 1;
    sPk.aiColumn = &aiColumnPk;
    sPk.aiRowLogEst = aiRowEstPk;
    sPk.onError = OE_Replace;
    sPk.pTable = pTab;
    sPk.szIdxRow = pTab->szTabRow;
    sPk.idxType = SQLITE_IDXTYPE_IPK;
    aiRowEstPk[0] = pTab->nRowLogEst;
    aiRowEstPk[1] = 0;
    pFirst = pSrc->pTab->pIndex;
    if( pSrc->fg.notIndexed==0 ){
      /* The real indices of the table are only considered if the
      ** NOT INDEXED qualifier is omitted from the FROM clause */
      sPk.pNext = pFirst;
    }
    pProbe = &sPk;
  }
  rSize = pTab->nRowLogEst;
  rLogSize = estLog(rSize);

#ifndef SQLITE_OMIT_AUTOMATIC_INDEX
  /* Automatic indexes */
  if( !pBuilder->pOrSet      /* Not part of an OR optimization */
   && (pWInfo->wctrlFlags & WHERE_OR_SUBCLAUSE)==0
   && (pWInfo->pParse->db->flags & SQLITE_AutoIndex)!=0
   && pSrc->pIBIndex==0      /* Has no INDEXED BY clause */
   && !pSrc->fg.notIndexed   /* Has no NOT INDEXED clause */
   && HasRowid(pTab)         /* Not WITHOUT ROWID table. (FIXME: Why not?) */
   && !pSrc->fg.isCorrelated /* Not a correlated subquery */
   && !pSrc->fg.isRecursive  /* Not a recursive common table expression. */
  ){
    /* Generate auto-index WhereLoops */
    WhereTerm *pTerm;
    WhereTerm *pWCEnd = pWC->a + pWC->nTerm;
    for(pTerm=pWC->a; rc==SQLITE_OK && pTerm<pWCEnd; pTerm++){
      if( pTerm->prereqRight & pNew->maskSelf ) continue;
      if( termCanDriveIndex(pTerm, pSrc, 0) ){
        pNew->u.btree.nEq = 1;
        pNew->nSkip = 0;
        pNew->u.btree.pIndex = 0;
        pNew->nLTerm = 1;
        pNew->aLTerm[0] = pTerm;
        /* TUNING: One-time cost for computing the automatic index is
        ** estimated to be X*N*log2(N) where N is the number of rows in
        ** the table being indexed and where X is 7 (LogEst=28) for normal
        ** tables or 0.5 (LogEst=-10) for views and subqueries.  The value
        ** of X is smaller for views and subqueries so that the query planner
        ** will be more aggressive about generating automatic indexes for
        ** those objects, since there is no opportunity to add schema
        ** indexes on subqueries and views. */
        pNew->rSetup = rLogSize + rSize;
        if( pTab->pSelect==0 && (pTab->tabFlags & TF_Ephemeral)==0 ){
          pNew->rSetup += 28;
        }else{
          pNew->rSetup -= 10;
        }
        ApplyCostMultiplier(pNew->rSetup, pTab->costMult);
        if( pNew->rSetup<0 ) pNew->rSetup = 0;
        /* TUNING: Each index lookup yields 20 rows in the table.  This
        ** is more than the usual guess of 10 rows, since we have no way
        ** of knowing how selective the index will ultimately be.  It would
        ** not be unreasonable to make this value much larger. */
        pNew->nOut = 43;  assert( 43==sqlite3LogEst(20) );
        pNew->rRun = sqlite3LogEstAdd(rLogSize,pNew->nOut);
        pNew->wsFlags = WHERE_AUTO_INDEX;
        pNew->prereq = mPrereq | pTerm->prereqRight;
        rc = whereLoopInsert(pBuilder, pNew);
      }
    }
  }
#endif /* SQLITE_OMIT_AUTOMATIC_INDEX */

  /* Loop over all indices. If there was an INDEXED BY clause, then only 
  ** consider index pProbe.  */
  for(; rc==SQLITE_OK && pProbe; 
      pProbe=(pSrc->pIBIndex ? 0 : pProbe->pNext), iSortIdx++
  ){
    if( pProbe->pPartIdxWhere!=0
     && !whereUsablePartialIndex(pSrc->iCursor, pWC, pProbe->pPartIdxWhere) ){
      testcase( pNew->iTab!=pSrc->iCursor );  /* See ticket [98d973b8f5] */
      continue;  /* Partial index inappropriate for this query */
    }
    if( pProbe->bNoQuery ) continue;
    rSize = pProbe->aiRowLogEst[0];
    pNew->u.btree.nEq = 0;
    pNew->u.btree.nBtm = 0;
    pNew->u.btree.nTop = 0;
    pNew->nSkip = 0;
    pNew->nLTerm = 0;
    pNew->iSortIdx = 0;
    pNew->rSetup = 0;
    pNew->prereq = mPrereq;
    pNew->nOut = rSize;
    pNew->u.btree.pIndex = pProbe;
    b = indexMightHelpWithOrderBy(pBuilder, pProbe, pSrc->iCursor);
    /* The ONEPASS_DESIRED flags never occurs together with ORDER BY */
    assert( (pWInfo->wctrlFlags & WHERE_ONEPASS_DESIRED)==0 || b==0 );
    if( pProbe->idxType==SQLITE_IDXTYPE_IPK ){
      /* Integer primary key index */
      pNew->wsFlags = WHERE_IPK;

      /* Full table scan */
      pNew->iSortIdx = b ? iSortIdx : 0;
      /* TUNING: Cost of full table scan is (N*3.0). */
      pNew->rRun = rSize + 16;
      ApplyCostMultiplier(pNew->rRun, pTab->costMult);
      whereLoopOutputAdjust(pWC, pNew, rSize);
      rc = whereLoopInsert(pBuilder, pNew);
      pNew->nOut = rSize;
      if( rc ) break;
    }else{
      Bitmask m;
      if( pProbe->isCovering ){
        pNew->wsFlags = WHERE_IDX_ONLY | WHERE_INDEXED;
        m = 0;
      }else{
        m = pSrc->colUsed & pProbe->colNotIdxed;
        pNew->wsFlags = (m==0) ? (WHERE_IDX_ONLY|WHERE_INDEXED) : WHERE_INDEXED;
      }

      /* Full scan via index */
      if( b
       || !HasRowid(pTab)
       || pProbe->pPartIdxWhere!=0
       || ( m==0
         && pProbe->bUnordered==0
         && (pProbe->szIdxRow<pTab->szTabRow)
         && (pWInfo->wctrlFlags & WHERE_ONEPASS_DESIRED)==0
         && sqlite3GlobalConfig.bUseCis
         && OptimizationEnabled(pWInfo->pParse->db, SQLITE_CoverIdxScan)
          )
      ){
        pNew->iSortIdx = b ? iSortIdx : 0;

        /* The cost of visiting the index rows is N*K, where K is
        ** between 1.1 and 3.0, depending on the relative sizes of the
        ** index and table rows. */
        pNew->rRun = rSize + 1 + (15*pProbe->szIdxRow)/pTab->szTabRow;
        if( m!=0 ){
          /* If this is a non-covering index scan, add in the cost of
          ** doing table lookups.  The cost will be 3x the number of
          ** lookups.  Take into account WHERE clause terms that can be
          ** satisfied using just the index, and that do not require a
          ** table lookup. */
          LogEst nLookup = rSize + 16;  /* Base cost:  N*3 */
          int ii;
          int iCur = pSrc->iCursor;
          WhereClause *pWC2 = &pWInfo->sWC;
          for(ii=0; ii<pWC2->nTerm; ii++){
            WhereTerm *pTerm = &pWC2->a[ii];
            if( !sqlite3ExprCoveredByIndex(pTerm->pExpr, iCur, pProbe) ){
              break;
            }
            /* pTerm can be evaluated using just the index.  So reduce
            ** the expected number of table lookups accordingly */
            if( pTerm->truthProb<=0 ){
              nLookup += pTerm->truthProb;
            }else{
              nLookup--;
              if( pTerm->eOperator & (WO_EQ|WO_IS) ) nLookup -= 19;
            }
          }
          
          pNew->rRun = sqlite3LogEstAdd(pNew->rRun, nLookup);
        }
        ApplyCostMultiplier(pNew->rRun, pTab->costMult);
        whereLoopOutputAdjust(pWC, pNew, rSize);
        rc = whereLoopInsert(pBuilder, pNew);
        pNew->nOut = rSize;
        if( rc ) break;
      }
    }

    pBuilder->bldFlags = 0;
    rc = whereLoopAddBtreeIndex(pBuilder, pSrc, pProbe, 0);
    if( pBuilder->bldFlags==SQLITE_BLDF_INDEXED ){
      /* If a non-unique index is used, or if a prefix of the key for
      ** unique index is used (making the index functionally non-unique)
      ** then the sqlite_stat1 data becomes important for scoring the
      ** plan */
      pTab->tabFlags |= TF_StatsUsed;
    }
#ifdef SQLITE_ENABLE_STAT4
    sqlite3Stat4ProbeFree(pBuilder->pRec);
    pBuilder->nRecValid = 0;
    pBuilder->pRec = 0;
#endif
  }
  return rc;
}

#ifndef SQLITE_OMIT_VIRTUALTABLE

/*
** Argument pIdxInfo is already populated with all constraints that may
** be used by the virtual table identified by pBuilder->pNew->iTab. This
** function marks a subset of those constraints usable, invokes the
** xBestIndex method and adds the returned plan to pBuilder.
**
** A constraint is marked usable if:
**
**   * Argument mUsable indicates that its prerequisites are available, and
**
**   * It is not one of the operators specified in the mExclude mask passed
**     as the fourth argument (which in practice is either WO_IN or 0).
**
** Argument mPrereq is a mask of tables that must be scanned before the
** virtual table in question. These are added to the plans prerequisites
** before it is added to pBuilder.
**
** Output parameter *pbIn is set to true if the plan added to pBuilder
** uses one or more WO_IN terms, or false otherwise.
*/
static int whereLoopAddVirtualOne(
  WhereLoopBuilder *pBuilder,
  Bitmask mPrereq,                /* Mask of tables that must be used. */
  Bitmask mUsable,                /* Mask of usable tables */
  u16 mExclude,                   /* Exclude terms using these operators */
  sqlite3_index_info *pIdxInfo,   /* Populated object for xBestIndex */
  u16 mNoOmit,                    /* Do not omit these constraints */
  int *pbIn                       /* OUT: True if plan uses an IN(...) op */
){
  WhereClause *pWC = pBuilder->pWC;
  struct sqlite3_index_constraint *pIdxCons;
  struct sqlite3_index_constraint_usage *pUsage = pIdxInfo->aConstraintUsage;
  int i;
  int mxTerm;
  int rc = SQLITE_OK;
  WhereLoop *pNew = pBuilder->pNew;
  Parse *pParse = pBuilder->pWInfo->pParse;
  struct SrcList_item *pSrc = &pBuilder->pWInfo->pTabList->a[pNew->iTab];
  int nConstraint = pIdxInfo->nConstraint;

  assert( (mUsable & mPrereq)==mPrereq );
  *pbIn = 0;
  pNew->prereq = mPrereq;

  /* Set the usable flag on the subset of constraints identified by 
  ** arguments mUsable and mExclude. */
  pIdxCons = *(struct sqlite3_index_constraint**)&pIdxInfo->aConstraint;
  for(i=0; i<nConstraint; i++, pIdxCons++){
    WhereTerm *pTerm = &pWC->a[pIdxCons->iTermOffset];
    pIdxCons->usable = 0;
    if( (pTerm->prereqRight & mUsable)==pTerm->prereqRight 
     && (pTerm->eOperator & mExclude)==0
    ){
      pIdxCons->usable = 1;
    }
  }

  /* Initialize the output fields of the sqlite3_index_info structure */
  memset(pUsage, 0, sizeof(pUsage[0])*nConstraint);
  assert( pIdxInfo->needToFreeIdxStr==0 );
  pIdxInfo->idxStr = 0;
  pIdxInfo->idxNum = 0;
  pIdxInfo->orderByConsumed = 0;
  pIdxInfo->estimatedCost = SQLITE_BIG_DBL / (double)2;
  pIdxInfo->estimatedRows = 25;
  pIdxInfo->idxFlags = 0;
  pIdxInfo->colUsed = (sqlite3_int64)pSrc->colUsed;

  /* Invoke the virtual table xBestIndex() method */
  rc = vtabBestIndex(pParse, pSrc->pTab, pIdxInfo);
  if( rc ){
    if( rc==SQLITE_CONSTRAINT ){
      /* If the xBestIndex method returns SQLITE_CONSTRAINT, that means
      ** that the particular combination of parameters provided is unusable.
      ** Make no entries in the loop table.
      */
      WHERETRACE(0xffff, ("  ^^^^--- non-viable plan rejected!\n"));
      return SQLITE_OK;
    }
    return rc;
  }

  mxTerm = -1;
  assert( pNew->nLSlot>=nConstraint );
  for(i=0; i<nConstraint; i++) pNew->aLTerm[i] = 0;
  pNew->u.vtab.omitMask = 0;
  pIdxCons = *(struct sqlite3_index_constraint**)&pIdxInfo->aConstraint;
  for(i=0; i<nConstraint; i++, pIdxCons++){
    int iTerm;
    if( (iTerm = pUsage[i].argvIndex - 1)>=0 ){
      WhereTerm *pTerm;
      int j = pIdxCons->iTermOffset;
      if( iTerm>=nConstraint
       || j<0
       || j>=pWC->nTerm
       || pNew->aLTerm[iTerm]!=0
       || pIdxCons->usable==0
      ){
        sqlite3ErrorMsg(pParse,"%s.xBestIndex malfunction",pSrc->pTab->zName);
        testcase( pIdxInfo->needToFreeIdxStr );
        return SQLITE_ERROR;
      }
      testcase( iTerm==nConstraint-1 );
      testcase( j==0 );
      testcase( j==pWC->nTerm-1 );
      pTerm = &pWC->a[j];
      pNew->prereq |= pTerm->prereqRight;
      assert( iTerm<pNew->nLSlot );
      pNew->aLTerm[iTerm] = pTerm;
      if( iTerm>mxTerm ) mxTerm = iTerm;
      testcase( iTerm==15 );
      testcase( iTerm==16 );
      if( iTerm<16 && pUsage[i].omit ) pNew->u.vtab.omitMask |= 1<<iTerm;
      if( (pTerm->eOperator & WO_IN)!=0 ){
        /* A virtual table that is constrained by an IN clause may not
        ** consume the ORDER BY clause because (1) the order of IN terms
        ** is not necessarily related to the order of output terms and
        ** (2) Multiple outputs from a single IN value will not merge
        ** together.  */
        pIdxInfo->orderByConsumed = 0;
        pIdxInfo->idxFlags &= ~SQLITE_INDEX_SCAN_UNIQUE;
        *pbIn = 1; assert( (mExclude & WO_IN)==0 );
      }
    }
  }
  pNew->u.vtab.omitMask &= ~mNoOmit;

  pNew->nLTerm = mxTerm+1;
  for(i=0; i<=mxTerm; i++){
    if( pNew->aLTerm[i]==0 ){
      /* The non-zero argvIdx values must be contiguous.  Raise an
      ** error if they are not */
      sqlite3ErrorMsg(pParse,"%s.xBestIndex malfunction",pSrc->pTab->zName);
      testcase( pIdxInfo->needToFreeIdxStr );
      return SQLITE_ERROR;
    }
  }
  assert( pNew->nLTerm<=pNew->nLSlot );
  pNew->u.vtab.idxNum = pIdxInfo->idxNum;
  pNew->u.vtab.needFree = pIdxInfo->needToFreeIdxStr;
  pIdxInfo->needToFreeIdxStr = 0;
  pNew->u.vtab.idxStr = pIdxInfo->idxStr;
  pNew->u.vtab.isOrdered = (i8)(pIdxInfo->orderByConsumed ?
      pIdxInfo->nOrderBy : 0);
  pNew->rSetup = 0;
  pNew->rRun = sqlite3LogEstFromDouble(pIdxInfo->estimatedCost);
  pNew->nOut = sqlite3LogEst(pIdxInfo->estimatedRows);

  /* Set the WHERE_ONEROW flag if the xBestIndex() method indicated
  ** that the scan will visit at most one row. Clear it otherwise. */
  if( pIdxInfo->idxFlags & SQLITE_INDEX_SCAN_UNIQUE ){
    pNew->wsFlags |= WHERE_ONEROW;
  }else{
    pNew->wsFlags &= ~WHERE_ONEROW;
  }
  rc = whereLoopInsert(pBuilder, pNew);
  if( pNew->u.vtab.needFree ){
    sqlite3_free(pNew->u.vtab.idxStr);
    pNew->u.vtab.needFree = 0;
  }
  WHERETRACE(0xffff, ("  bIn=%d prereqIn=%04llx prereqOut=%04llx\n",
                      *pbIn, (sqlite3_uint64)mPrereq,
                      (sqlite3_uint64)(pNew->prereq & ~mPrereq)));

  return rc;
}

/*
** If this function is invoked from within an xBestIndex() callback, it
** returns a pointer to a buffer containing the name of the collation
** sequence associated with element iCons of the sqlite3_index_info.aConstraint
** array. Or, if iCons is out of range or there is no active xBestIndex
** call, return NULL.
*/
SQLITE_API const char *sqlite3_vtab_collation(sqlite3_index_info *pIdxInfo, int iCons){
  HiddenIndexInfo *pHidden = (HiddenIndexInfo*)&pIdxInfo[1];
  const char *zRet = 0;
  if( iCons>=0 && iCons<pIdxInfo->nConstraint ){
    CollSeq *pC = 0;
    int iTerm = pIdxInfo->aConstraint[iCons].iTermOffset;
    Expr *pX = pHidden->pWC->a[iTerm].pExpr;
    if( pX->pLeft ){
      pC = sqlite3BinaryCompareCollSeq(pHidden->pParse, pX->pLeft, pX->pRight);
    }
    zRet = (pC ? pC->zName : sqlite3StrBINARY);
  }
  return zRet;
}

/*
** Add all WhereLoop objects for a table of the join identified by
** pBuilder->pNew->iTab.  That table is guaranteed to be a virtual table.
**
** If there are no LEFT or CROSS JOIN joins in the query, both mPrereq and
** mUnusable are set to 0. Otherwise, mPrereq is a mask of all FROM clause
** entries that occur before the virtual table in the FROM clause and are
** separated from it by at least one LEFT or CROSS JOIN. Similarly, the
** mUnusable mask contains all FROM clause entries that occur after the
** virtual table and are separated from it by at least one LEFT or 
** CROSS JOIN. 
**
** For example, if the query were:
**
**   ... FROM t1, t2 LEFT JOIN t3, t4, vt CROSS JOIN t5, t6;
**
** then mPrereq corresponds to (t1, t2) and mUnusable to (t5, t6).
**
** All the tables in mPrereq must be scanned before the current virtual 
** table. So any terms for which all prerequisites are satisfied by 
** mPrereq may be specified as "usable" in all calls to xBestIndex. 
** Conversely, all tables in mUnusable must be scanned after the current
** virtual table, so any terms for which the prerequisites overlap with
** mUnusable should always be configured as "not-usable" for xBestIndex.
*/
static int whereLoopAddVirtual(
  WhereLoopBuilder *pBuilder,  /* WHERE clause information */
  Bitmask mPrereq,             /* Tables that must be scanned before this one */
  Bitmask mUnusable            /* Tables that must be scanned after this one */
){
  int rc = SQLITE_OK;          /* Return code */
  WhereInfo *pWInfo;           /* WHERE analysis context */
  Parse *pParse;               /* The parsing context */
  WhereClause *pWC;            /* The WHERE clause */
  struct SrcList_item *pSrc;   /* The FROM clause term to search */
  sqlite3_index_info *p;       /* Object to pass to xBestIndex() */
  int nConstraint;             /* Number of constraints in p */
  int bIn;                     /* True if plan uses IN(...) operator */
  WhereLoop *pNew;
  Bitmask mBest;               /* Tables used by best possible plan */
  u16 mNoOmit;

  assert( (mPrereq & mUnusable)==0 );
  pWInfo = pBuilder->pWInfo;
  pParse = pWInfo->pParse;
  pWC = pBuilder->pWC;
  pNew = pBuilder->pNew;
  pSrc = &pWInfo->pTabList->a[pNew->iTab];
  assert( IsVirtual(pSrc->pTab) );
  p = allocateIndexInfo(pParse, pWC, mUnusable, pSrc, pBuilder->pOrderBy, 
      &mNoOmit);
  if( p==0 ) return SQLITE_NOMEM_BKPT;
  pNew->rSetup = 0;
  pNew->wsFlags = WHERE_VIRTUALTABLE;
  pNew->nLTerm = 0;
  pNew->u.vtab.needFree = 0;
  nConstraint = p->nConstraint;
  if( whereLoopResize(pParse->db, pNew, nConstraint) ){
    sqlite3DbFree(pParse->db, p);
    return SQLITE_NOMEM_BKPT;
  }

  /* First call xBestIndex() with all constraints usable. */
  WHERETRACE(0x800, ("BEGIN %s.addVirtual()\n", pSrc->pTab->zName));
  WHERETRACE(0x40, ("  VirtualOne: all usable\n"));
  rc = whereLoopAddVirtualOne(pBuilder, mPrereq, ALLBITS, 0, p, mNoOmit, &bIn);

  /* If the call to xBestIndex() with all terms enabled produced a plan
  ** that does not require any source tables (IOW: a plan with mBest==0)
  ** and does not use an IN(...) operator, then there is no point in making 
  ** any further calls to xBestIndex() since they will all return the same
  ** result (if the xBestIndex() implementation is sane). */
  if( rc==SQLITE_OK && ((mBest = (pNew->prereq & ~mPrereq))!=0 || bIn) ){
    int seenZero = 0;             /* True if a plan with no prereqs seen */
    int seenZeroNoIN = 0;         /* Plan with no prereqs and no IN(...) seen */
    Bitmask mPrev = 0;
    Bitmask mBestNoIn = 0;

    /* If the plan produced by the earlier call uses an IN(...) term, call
    ** xBestIndex again, this time with IN(...) terms disabled. */
    if( bIn ){
      WHERETRACE(0x40, ("  VirtualOne: all usable w/o IN\n"));
      rc = whereLoopAddVirtualOne(
          pBuilder, mPrereq, ALLBITS, WO_IN, p, mNoOmit, &bIn);
      assert( bIn==0 );
      mBestNoIn = pNew->prereq & ~mPrereq;
      if( mBestNoIn==0 ){
        seenZero = 1;
        seenZeroNoIN = 1;
      }
    }

    /* Call xBestIndex once for each distinct value of (prereqRight & ~mPrereq) 
    ** in the set of terms that apply to the current virtual table.  */
    while( rc==SQLITE_OK ){
      int i;
      Bitmask mNext = ALLBITS;
      assert( mNext>0 );
      for(i=0; i<nConstraint; i++){
        Bitmask mThis = (
            pWC->a[p->aConstraint[i].iTermOffset].prereqRight & ~mPrereq
        );
        if( mThis>mPrev && mThis<mNext ) mNext = mThis;
      }
      mPrev = mNext;
      if( mNext==ALLBITS ) break;
      if( mNext==mBest || mNext==mBestNoIn ) continue;
      WHERETRACE(0x40, ("  VirtualOne: mPrev=%04llx mNext=%04llx\n",
                       (sqlite3_uint64)mPrev, (sqlite3_uint64)mNext));
      rc = whereLoopAddVirtualOne(
          pBuilder, mPrereq, mNext|mPrereq, 0, p, mNoOmit, &bIn);
      if( pNew->prereq==mPrereq ){
        seenZero = 1;
        if( bIn==0 ) seenZeroNoIN = 1;
      }
    }

    /* If the calls to xBestIndex() in the above loop did not find a plan
    ** that requires no source tables at all (i.e. one guaranteed to be
    ** usable), make a call here with all source tables disabled */
    if( rc==SQLITE_OK && seenZero==0 ){
      WHERETRACE(0x40, ("  VirtualOne: all disabled\n"));
      rc = whereLoopAddVirtualOne(
          pBuilder, mPrereq, mPrereq, 0, p, mNoOmit, &bIn);
      if( bIn==0 ) seenZeroNoIN = 1;
    }

    /* If the calls to xBestIndex() have so far failed to find a plan
    ** that requires no source tables at all and does not use an IN(...)
    ** operator, make a final call to obtain one here.  */
    if( rc==SQLITE_OK && seenZeroNoIN==0 ){
      WHERETRACE(0x40, ("  VirtualOne: all disabled and w/o IN\n"));
      rc = whereLoopAddVirtualOne(
          pBuilder, mPrereq, mPrereq, WO_IN, p, mNoOmit, &bIn);
    }
  }

  if( p->needToFreeIdxStr ) sqlite3_free(p->idxStr);
  sqlite3DbFreeNN(pParse->db, p);
  WHERETRACE(0x800, ("END %s.addVirtual(), rc=%d\n", pSrc->pTab->zName, rc));
  return rc;
}
#endif /* SQLITE_OMIT_VIRTUALTABLE */

/*
** Add WhereLoop entries to handle OR terms.  This works for either
** btrees or virtual tables.
*/
static int whereLoopAddOr(
  WhereLoopBuilder *pBuilder, 
  Bitmask mPrereq, 
  Bitmask mUnusable
){
  WhereInfo *pWInfo = pBuilder->pWInfo;
  WhereClause *pWC;
  WhereLoop *pNew;
  WhereTerm *pTerm, *pWCEnd;
  int rc = SQLITE_OK;
  int iCur;
  WhereClause tempWC;
  WhereLoopBuilder sSubBuild;
  WhereOrSet sSum, sCur;
  struct SrcList_item *pItem;
  
  pWC = pBuilder->pWC;
  pWCEnd = pWC->a + pWC->nTerm;
  pNew = pBuilder->pNew;
  memset(&sSum, 0, sizeof(sSum));
  pItem = pWInfo->pTabList->a + pNew->iTab;
  iCur = pItem->iCursor;

  for(pTerm=pWC->a; pTerm<pWCEnd && rc==SQLITE_OK; pTerm++){
    if( (pTerm->eOperator & WO_OR)!=0
     && (pTerm->u.pOrInfo->indexable & pNew->maskSelf)!=0 
    ){
      WhereClause * const pOrWC = &pTerm->u.pOrInfo->wc;
      WhereTerm * const pOrWCEnd = &pOrWC->a[pOrWC->nTerm];
      WhereTerm *pOrTerm;
      int once = 1;
      int i, j;
    
      sSubBuild = *pBuilder;
      sSubBuild.pOrderBy = 0;
      sSubBuild.pOrSet = &sCur;

      WHERETRACE(0x200, ("Begin processing OR-clause %p\n", pTerm));
      for(pOrTerm=pOrWC->a; pOrTerm<pOrWCEnd; pOrTerm++){
        if( (pOrTerm->eOperator & WO_AND)!=0 ){
          sSubBuild.pWC = &pOrTerm->u.pAndInfo->wc;
        }else if( pOrTerm->leftCursor==iCur ){
          tempWC.pWInfo = pWC->pWInfo;
          tempWC.pOuter = pWC;
          tempWC.op = TK_AND;
          tempWC.nTerm = 1;
          tempWC.a = pOrTerm;
          sSubBuild.pWC = &tempWC;
        }else{
          continue;
        }
        sCur.n = 0;
#ifdef WHERETRACE_ENABLED
        WHERETRACE(0x200, ("OR-term %d of %p has %d subterms:\n", 
                   (int)(pOrTerm-pOrWC->a), pTerm, sSubBuild.pWC->nTerm));
        if( sqlite3WhereTrace & 0x400 ){
          sqlite3WhereClausePrint(sSubBuild.pWC);
        }
#endif
#ifndef SQLITE_OMIT_VIRTUALTABLE
        if( IsVirtual(pItem->pTab) ){
          rc = whereLoopAddVirtual(&sSubBuild, mPrereq, mUnusable);
        }else
#endif
        {
          rc = whereLoopAddBtree(&sSubBuild, mPrereq);
        }
        if( rc==SQLITE_OK ){
          rc = whereLoopAddOr(&sSubBuild, mPrereq, mUnusable);
        }
        assert( rc==SQLITE_OK || sCur.n==0 );
        if( sCur.n==0 ){
          sSum.n = 0;
          break;
        }else if( once ){
          whereOrMove(&sSum, &sCur);
          once = 0;
        }else{
          WhereOrSet sPrev;
          whereOrMove(&sPrev, &sSum);
          sSum.n = 0;
          for(i=0; i<sPrev.n; i++){
            for(j=0; j<sCur.n; j++){
              whereOrInsert(&sSum, sPrev.a[i].prereq | sCur.a[j].prereq,
                            sqlite3LogEstAdd(sPrev.a[i].rRun, sCur.a[j].rRun),
                            sqlite3LogEstAdd(sPrev.a[i].nOut, sCur.a[j].nOut));
            }
          }
        }
      }
      pNew->nLTerm = 1;
      pNew->aLTerm[0] = pTerm;
      pNew->wsFlags = WHERE_MULTI_OR;
      pNew->rSetup = 0;
      pNew->iSortIdx = 0;
      memset(&pNew->u, 0, sizeof(pNew->u));
      for(i=0; rc==SQLITE_OK && i<sSum.n; i++){
        /* TUNING: Currently sSum.a[i].rRun is set to the sum of the costs
        ** of all sub-scans required by the OR-scan. However, due to rounding
        ** errors, it may be that the cost of the OR-scan is equal to its
        ** most expensive sub-scan. Add the smallest possible penalty 
        ** (equivalent to multiplying the cost by 1.07) to ensure that 
        ** this does not happen. Otherwise, for WHERE clauses such as the
        ** following where there is an index on "y":
        **
        **     WHERE likelihood(x=?, 0.99) OR y=?
        **
        ** the planner may elect to "OR" together a full-table scan and an
        ** index lookup. And other similarly odd results.  */
        pNew->rRun = sSum.a[i].rRun + 1;
        pNew->nOut = sSum.a[i].nOut;
        pNew->prereq = sSum.a[i].prereq;
        rc = whereLoopInsert(pBuilder, pNew);
      }
      WHERETRACE(0x200, ("End processing OR-clause %p\n", pTerm));
    }
  }
  return rc;
}

/*
** Add all WhereLoop objects for all tables 
*/
static int whereLoopAddAll(WhereLoopBuilder *pBuilder){
  WhereInfo *pWInfo = pBuilder->pWInfo;
  Bitmask mPrereq = 0;
  Bitmask mPrior = 0;
  int iTab;
  SrcList *pTabList = pWInfo->pTabList;
  struct SrcList_item *pItem;
  struct SrcList_item *pEnd = &pTabList->a[pWInfo->nLevel];
  sqlite3 *db = pWInfo->pParse->db;
  int rc = SQLITE_OK;
  WhereLoop *pNew;
  u8 priorJointype = 0;

  /* Loop over the tables in the join, from left to right */
  pNew = pBuilder->pNew;
  whereLoopInit(pNew);
  pBuilder->iPlanLimit = SQLITE_QUERY_PLANNER_LIMIT;
  for(iTab=0, pItem=pTabList->a; pItem<pEnd; iTab++, pItem++){
    Bitmask mUnusable = 0;
    pNew->iTab = iTab;
    pBuilder->iPlanLimit += SQLITE_QUERY_PLANNER_LIMIT_INCR;
    pNew->maskSelf = sqlite3WhereGetMask(&pWInfo->sMaskSet, pItem->iCursor);
    if( ((pItem->fg.jointype|priorJointype) & (JT_LEFT|JT_CROSS))!=0 ){
      /* This condition is true when pItem is the FROM clause term on the
      ** right-hand-side of a LEFT or CROSS JOIN.  */
      mPrereq = mPrior;
    }
    priorJointype = pItem->fg.jointype;
#ifndef SQLITE_OMIT_VIRTUALTABLE
    if( IsVirtual(pItem->pTab) ){
      struct SrcList_item *p;
      for(p=&pItem[1]; p<pEnd; p++){
        if( mUnusable || (p->fg.jointype & (JT_LEFT|JT_CROSS)) ){
          mUnusable |= sqlite3WhereGetMask(&pWInfo->sMaskSet, p->iCursor);
        }
      }
      rc = whereLoopAddVirtual(pBuilder, mPrereq, mUnusable);
    }else
#endif /* SQLITE_OMIT_VIRTUALTABLE */
    {
      rc = whereLoopAddBtree(pBuilder, mPrereq);
    }
    if( rc==SQLITE_OK && pBuilder->pWC->hasOr ){
      rc = whereLoopAddOr(pBuilder, mPrereq, mUnusable);
    }
    mPrior |= pNew->maskSelf;
    if( rc || db->mallocFailed ){
      if( rc==SQLITE_DONE ){
        /* We hit the query planner search limit set by iPlanLimit */
        sqlite3_log(SQLITE_WARNING, "abbreviated query algorithm search");
        rc = SQLITE_OK;
      }else{
        break;
      }
    }
  }

  whereLoopClear(db, pNew);
  return rc;
}

/*
** Examine a WherePath (with the addition of the extra WhereLoop of the 6th
** parameters) to see if it outputs rows in the requested ORDER BY
** (or GROUP BY) without requiring a separate sort operation.  Return N:
** 
**   N>0:   N terms of the ORDER BY clause are satisfied
**   N==0:  No terms of the ORDER BY clause are satisfied
**   N<0:   Unknown yet how many terms of ORDER BY might be satisfied.   
**
** Note that processing for WHERE_GROUPBY and WHERE_DISTINCTBY is not as
** strict.  With GROUP BY and DISTINCT the only requirement is that
** equivalent rows appear immediately adjacent to one another.  GROUP BY
** and DISTINCT do not require rows to appear in any particular order as long
** as equivalent rows are grouped together.  Thus for GROUP BY and DISTINCT
** the pOrderBy terms can be matched in any order.  With ORDER BY, the 
** pOrderBy terms must be matched in strict left-to-right order.
*/
static i8 wherePathSatisfiesOrderBy(
  WhereInfo *pWInfo,    /* The WHERE clause */
  ExprList *pOrderBy,   /* ORDER BY or GROUP BY or DISTINCT clause to check */
  WherePath *pPath,     /* The WherePath to check */
  u16 wctrlFlags,       /* WHERE_GROUPBY or _DISTINCTBY or _ORDERBY_LIMIT */
  u16 nLoop,            /* Number of entries in pPath->aLoop[] */
  WhereLoop *pLast,     /* Add this WhereLoop to the end of pPath->aLoop[] */
  Bitmask *pRevMask     /* OUT: Mask of WhereLoops to run in reverse order */
){
  u8 revSet;            /* True if rev is known */
  u8 rev;               /* Composite sort order */
  u8 revIdx;            /* Index sort order */
  u8 isOrderDistinct;   /* All prior WhereLoops are order-distinct */
  u8 distinctColumns;   /* True if the loop has UNIQUE NOT NULL columns */
  u8 isMatch;           /* iColumn matches a term of the ORDER BY clause */
  u16 eqOpMask;         /* Allowed equality operators */
  u16 nKeyCol;          /* Number of key columns in pIndex */
  u16 nColumn;          /* Total number of ordered columns in the index */
  u16 nOrderBy;         /* Number terms in the ORDER BY clause */
  int iLoop;            /* Index of WhereLoop in pPath being processed */
  int i, j;             /* Loop counters */
  int iCur;             /* Cursor number for current WhereLoop */
  int iColumn;          /* A column number within table iCur */
  WhereLoop *pLoop = 0; /* Current WhereLoop being processed. */
  WhereTerm *pTerm;     /* A single term of the WHERE clause */
  Expr *pOBExpr;        /* An expression from the ORDER BY clause */
  CollSeq *pColl;       /* COLLATE function from an ORDER BY clause term */
  Index *pIndex;        /* The index associated with pLoop */
  sqlite3 *db = pWInfo->pParse->db;  /* Database connection */
  Bitmask obSat = 0;    /* Mask of ORDER BY terms satisfied so far */
  Bitmask obDone;       /* Mask of all ORDER BY terms */
  Bitmask orderDistinctMask;  /* Mask of all well-ordered loops */
  Bitmask ready;              /* Mask of inner loops */

  /*
  ** We say the WhereLoop is "one-row" if it generates no more than one
  ** row of output.  A WhereLoop is one-row if all of the following are true:
  **  (a) All index columns match with WHERE_COLUMN_EQ.
  **  (b) The index is unique
  ** Any WhereLoop with an WHERE_COLUMN_EQ constraint on the rowid is one-row.
  ** Every one-row WhereLoop will have the WHERE_ONEROW bit set in wsFlags.
  **
  ** We say the WhereLoop is "order-distinct" if the set of columns from
  ** that WhereLoop that are in the ORDER BY clause are different for every
  ** row of the WhereLoop.  Every one-row WhereLoop is automatically
  ** order-distinct.   A WhereLoop that has no columns in the ORDER BY clause
  ** is not order-distinct. To be order-distinct is not quite the same as being
  ** UNIQUE since a UNIQUE column or index can have multiple rows that 
  ** are NULL and NULL values are equivalent for the purpose of order-distinct.
  ** To be order-distinct, the columns must be UNIQUE and NOT NULL.
  **
  ** The rowid for a table is always UNIQUE and NOT NULL so whenever the
  ** rowid appears in the ORDER BY clause, the corresponding WhereLoop is
  ** automatically order-distinct.
  */

  assert( pOrderBy!=0 );
  if( nLoop && OptimizationDisabled(db, SQLITE_OrderByIdxJoin) ) return 0;

  nOrderBy = pOrderBy->nExpr;
  testcase( nOrderBy==BMS-1 );
  if( nOrderBy>BMS-1 ) return 0;  /* Cannot optimize overly large ORDER BYs */
  isOrderDistinct = 1;
  obDone = MASKBIT(nOrderBy)-1;
  orderDistinctMask = 0;
  ready = 0;
  eqOpMask = WO_EQ | WO_IS | WO_ISNULL;
  if( wctrlFlags & WHERE_ORDERBY_LIMIT ) eqOpMask |= WO_IN;
  for(iLoop=0; isOrderDistinct && obSat<obDone && iLoop<=nLoop; iLoop++){
    if( iLoop>0 ) ready |= pLoop->maskSelf;
    if( iLoop<nLoop ){
      pLoop = pPath->aLoop[iLoop];
      if( wctrlFlags & WHERE_ORDERBY_LIMIT ) continue;
    }else{
      pLoop = pLast;
    }
    if( pLoop->wsFlags & WHERE_VIRTUALTABLE ){
      if( pLoop->u.vtab.isOrdered ) obSat = obDone;
      break;
    }else if( wctrlFlags & WHERE_DISTINCTBY ){
      pLoop->u.btree.nDistinctCol = 0;
    }
    iCur = pWInfo->pTabList->a[pLoop->iTab].iCursor;

    /* Mark off any ORDER BY term X that is a column in the table of
    ** the current loop for which there is term in the WHERE
    ** clause of the form X IS NULL or X=? that reference only outer
    ** loops.
    */
    for(i=0; i<nOrderBy; i++){
      if( MASKBIT(i) & obSat ) continue;
      pOBExpr = sqlite3ExprSkipCollateAndLikely(pOrderBy->a[i].pExpr);
      if( pOBExpr->op!=TK_COLUMN ) continue;
      if( pOBExpr->iTable!=iCur ) continue;
      pTerm = sqlite3WhereFindTerm(&pWInfo->sWC, iCur, pOBExpr->iColumn,
                       ~ready, eqOpMask, 0);
      if( pTerm==0 ) continue;
      if( pTerm->eOperator==WO_IN ){
        /* IN terms are only valid for sorting in the ORDER BY LIMIT 
        ** optimization, and then only if they are actually used
        ** by the query plan */
        assert( wctrlFlags & WHERE_ORDERBY_LIMIT );
        for(j=0; j<pLoop->nLTerm && pTerm!=pLoop->aLTerm[j]; j++){}
        if( j>=pLoop->nLTerm ) continue;
      }
      if( (pTerm->eOperator&(WO_EQ|WO_IS))!=0 && pOBExpr->iColumn>=0 ){
        if( sqlite3ExprCollSeqMatch(pWInfo->pParse, 
                  pOrderBy->a[i].pExpr, pTerm->pExpr)==0 ){
          continue;
        }
        testcase( pTerm->pExpr->op==TK_IS );
      }
      obSat |= MASKBIT(i);
    }

    if( (pLoop->wsFlags & WHERE_ONEROW)==0 ){
      if( pLoop->wsFlags & WHERE_IPK ){
        pIndex = 0;
        nKeyCol = 0;
        nColumn = 1;
      }else if( (pIndex = pLoop->u.btree.pIndex)==0 || pIndex->bUnordered ){
        return 0;
      }else{
        nKeyCol = pIndex->nKeyCol;
        nColumn = pIndex->nColumn;
        assert( nColumn==nKeyCol+1 || !HasRowid(pIndex->pTable) );
        assert( pIndex->aiColumn[nColumn-1]==XN_ROWID
                          || !HasRowid(pIndex->pTable));
        isOrderDistinct = IsUniqueIndex(pIndex)
                          && (pLoop->wsFlags & WHERE_SKIPSCAN)==0;
      }

      /* Loop through all columns of the index and deal with the ones
      ** that are not constrained by == or IN.
      */
      rev = revSet = 0;
      distinctColumns = 0;
      for(j=0; j<nColumn; j++){
        u8 bOnce = 1; /* True to run the ORDER BY search loop */

        assert( j>=pLoop->u.btree.nEq 
            || (pLoop->aLTerm[j]==0)==(j<pLoop->nSkip)
        );
        if( j<pLoop->u.btree.nEq && j>=pLoop->nSkip ){
          u16 eOp = pLoop->aLTerm[j]->eOperator;

          /* Skip over == and IS and ISNULL terms.  (Also skip IN terms when
          ** doing WHERE_ORDERBY_LIMIT processing).  Except, IS and ISNULL
          ** terms imply that the index is not UNIQUE NOT NULL in which case
          ** the loop need to be marked as not order-distinct because it can
          ** have repeated NULL rows.
          **
          ** If the current term is a column of an ((?,?) IN (SELECT...)) 
          ** expression for which the SELECT returns more than one column,
          ** check that it is the only column used by this loop. Otherwise,
          ** if it is one of two or more, none of the columns can be
          ** considered to match an ORDER BY term.
          */
          if( (eOp & eqOpMask)!=0 ){
            if( eOp & (WO_ISNULL|WO_IS) ){
              testcase( eOp & WO_ISNULL );
              testcase( eOp & WO_IS );
              testcase( isOrderDistinct );
              isOrderDistinct = 0;
            }
            continue;  
          }else if( ALWAYS(eOp & WO_IN) ){
            /* ALWAYS() justification: eOp is an equality operator due to the
            ** j<pLoop->u.btree.nEq constraint above.  Any equality other
            ** than WO_IN is captured by the previous "if".  So this one
            ** always has to be WO_IN. */
            Expr *pX = pLoop->aLTerm[j]->pExpr;
            for(i=j+1; i<pLoop->u.btree.nEq; i++){
              if( pLoop->aLTerm[i]->pExpr==pX ){
                assert( (pLoop->aLTerm[i]->eOperator & WO_IN) );
                bOnce = 0;
                break;
              }
            }
          }
        }

        /* Get the column number in the table (iColumn) and sort order
        ** (revIdx) for the j-th column of the index.
        */
        if( pIndex ){
          iColumn = pIndex->aiColumn[j];
          revIdx = pIndex->aSortOrder[j] & KEYINFO_ORDER_DESC;
          if( iColumn==pIndex->pTable->iPKey ) iColumn = XN_ROWID;
        }else{
          iColumn = XN_ROWID;
          revIdx = 0;
        }

        /* An unconstrained column that might be NULL means that this
        ** WhereLoop is not well-ordered
        */
        if( isOrderDistinct
         && iColumn>=0
         && j>=pLoop->u.btree.nEq
         && pIndex->pTable->aCol[iColumn].notNull==0
        ){
          isOrderDistinct = 0;
        }

        /* Find the ORDER BY term that corresponds to the j-th column
        ** of the index and mark that ORDER BY term off 
        */
        isMatch = 0;
        for(i=0; bOnce && i<nOrderBy; i++){
          if( MASKBIT(i) & obSat ) continue;
          pOBExpr = sqlite3ExprSkipCollateAndLikely(pOrderBy->a[i].pExpr);
          testcase( wctrlFlags & WHERE_GROUPBY );
          testcase( wctrlFlags & WHERE_DISTINCTBY );
          if( (wctrlFlags & (WHERE_GROUPBY|WHERE_DISTINCTBY))==0 ) bOnce = 0;
          if( iColumn>=XN_ROWID ){
            if( pOBExpr->op!=TK_COLUMN ) continue;
            if( pOBExpr->iTable!=iCur ) continue;
            if( pOBExpr->iColumn!=iColumn ) continue;
          }else{
            Expr *pIdxExpr = pIndex->aColExpr->a[j].pExpr;
            if( sqlite3ExprCompareSkip(pOBExpr, pIdxExpr, iCur) ){
              continue;
            }
          }
          if( iColumn!=XN_ROWID ){
            pColl = sqlite3ExprNNCollSeq(pWInfo->pParse, pOrderBy->a[i].pExpr);
            if( sqlite3StrICmp(pColl->zName, pIndex->azColl[j])!=0 ) continue;
          }
          if( wctrlFlags & WHERE_DISTINCTBY ){
            pLoop->u.btree.nDistinctCol = j+1;
          }
          isMatch = 1;
          break;
        }
        if( isMatch && (wctrlFlags & WHERE_GROUPBY)==0 ){
          /* Make sure the sort order is compatible in an ORDER BY clause.
          ** Sort order is irrelevant for a GROUP BY clause. */
          if( revSet ){
            if( (rev ^ revIdx)!=(pOrderBy->a[i].sortFlags&KEYINFO_ORDER_DESC) ){
              isMatch = 0;
            }
          }else{
            rev = revIdx ^ (pOrderBy->a[i].sortFlags & KEYINFO_ORDER_DESC);
            if( rev ) *pRevMask |= MASKBIT(iLoop);
            revSet = 1;
          }
        }
        if( isMatch && (pOrderBy->a[i].sortFlags & KEYINFO_ORDER_BIGNULL) ){
          if( j==pLoop->u.btree.nEq ){
            pLoop->wsFlags |= WHERE_BIGNULL_SORT;
          }else{
            isMatch = 0;
          }
        }
        if( isMatch ){
          if( iColumn==XN_ROWID ){
            testcase( distinctColumns==0 );
            distinctColumns = 1;
          }
          obSat |= MASKBIT(i);
        }else{
          /* No match found */
          if( j==0 || j<nKeyCol ){
            testcase( isOrderDistinct!=0 );
            isOrderDistinct = 0;
          }
          break;
        }
      } /* end Loop over all index columns */
      if( distinctColumns ){
        testcase( isOrderDistinct==0 );
        isOrderDistinct = 1;
      }
    } /* end-if not one-row */

    /* Mark off any other ORDER BY terms that reference pLoop */
    if( isOrderDistinct ){
      orderDistinctMask |= pLoop->maskSelf;
      for(i=0; i<nOrderBy; i++){
        Expr *p;
        Bitmask mTerm;
        if( MASKBIT(i) & obSat ) continue;
        p = pOrderBy->a[i].pExpr;
        mTerm = sqlite3WhereExprUsage(&pWInfo->sMaskSet,p);
        if( mTerm==0 && !sqlite3ExprIsConstant(p) ) continue;
        if( (mTerm&~orderDistinctMask)==0 ){
          obSat |= MASKBIT(i);
        }
      }
    }
  } /* End the loop over all WhereLoops from outer-most down to inner-most */
  if( obSat==obDone ) return (i8)nOrderBy;
  if( !isOrderDistinct ){
    for(i=nOrderBy-1; i>0; i--){
      Bitmask m = MASKBIT(i) - 1;
      if( (obSat&m)==m ) return i;
    }
    return 0;
  }
  return -1;
}


/*
** If the WHERE_GROUPBY flag is set in the mask passed to sqlite3WhereBegin(),
** the planner assumes that the specified pOrderBy list is actually a GROUP
** BY clause - and so any order that groups rows as required satisfies the
** request.
**
** Normally, in this case it is not possible for the caller to determine
** whether or not the rows are really being delivered in sorted order, or
** just in some other order that provides the required grouping. However,
** if the WHERE_SORTBYGROUP flag is also passed to sqlite3WhereBegin(), then
** this function may be called on the returned WhereInfo object. It returns
** true if the rows really will be sorted in the specified order, or false
** otherwise.
**
** For example, assuming:
**
**   CREATE INDEX i1 ON t1(x, Y);
**
** then
**
**   SELECT * FROM t1 GROUP BY x,y ORDER BY x,y;   -- IsSorted()==1
**   SELECT * FROM t1 GROUP BY y,x ORDER BY y,x;   -- IsSorted()==0
*/
SQLITE_PRIVATE int sqlite3WhereIsSorted(WhereInfo *pWInfo){
  assert( pWInfo->wctrlFlags & WHERE_GROUPBY );
  assert( pWInfo->wctrlFlags & WHERE_SORTBYGROUP );
  return pWInfo->sorted;
}

#ifdef WHERETRACE_ENABLED
/* For debugging use only: */
static const char *wherePathName(WherePath *pPath, int nLoop, WhereLoop *pLast){
  static char zName[65];
  int i;
  for(i=0; i<nLoop; i++){ zName[i] = pPath->aLoop[i]->cId; }
  if( pLast ) zName[i++] = pLast->cId;
  zName[i] = 0;
  return zName;
}
#endif

/*
** Return the cost of sorting nRow rows, assuming that the keys have 
** nOrderby columns and that the first nSorted columns are already in
** order.
*/
static LogEst whereSortingCost(
  WhereInfo *pWInfo,
  LogEst nRow,
  int nOrderBy,
  int nSorted
){
  /* TUNING: Estimated cost of a full external sort, where N is 
  ** the number of rows to sort is:
  **
  **   cost = (3.0 * N * log(N)).
  ** 
  ** Or, if the order-by clause has X terms but only the last Y 
  ** terms are out of order, then block-sorting will reduce the 
  ** sorting cost to:
  **
  **   cost = (3.0 * N * log(N)) * (Y/X)
  **
  ** The (Y/X) term is implemented using stack variable rScale
  ** below.  */
  LogEst rScale, rSortCost;
  assert( nOrderBy>0 && 66==sqlite3LogEst(100) );
  rScale = sqlite3LogEst((nOrderBy-nSorted)*100/nOrderBy) - 66;
  rSortCost = nRow + rScale + 16;

  /* Multiple by log(M) where M is the number of output rows.
  ** Use the LIMIT for M if it is smaller */
  if( (pWInfo->wctrlFlags & WHERE_USE_LIMIT)!=0 && pWInfo->iLimit<nRow ){
    nRow = pWInfo->iLimit;
  }
  rSortCost += estLog(nRow);
  return rSortCost;
}

/*
** Given the list of WhereLoop objects at pWInfo->pLoops, this routine
** attempts to find the lowest cost path that visits each WhereLoop
** once.  This path is then loaded into the pWInfo->a[].pWLoop fields.
**
** Assume that the total number of output rows that will need to be sorted
** will be nRowEst (in the 10*log2 representation).  Or, ignore sorting
** costs if nRowEst==0.
**
** Return SQLITE_OK on success or SQLITE_NOMEM of a memory allocation
** error occurs.
*/
static int wherePathSolver(WhereInfo *pWInfo, LogEst nRowEst){
  int mxChoice;             /* Maximum number of simultaneous paths tracked */
  int nLoop;                /* Number of terms in the join */
  Parse *pParse;            /* Parsing context */
  sqlite3 *db;              /* The database connection */
  int iLoop;                /* Loop counter over the terms of the join */
  int ii, jj;               /* Loop counters */
  int mxI = 0;              /* Index of next entry to replace */
  int nOrderBy;             /* Number of ORDER BY clause terms */
  LogEst mxCost = 0;        /* Maximum cost of a set of paths */
  LogEst mxUnsorted = 0;    /* Maximum unsorted cost of a set of path */
  int nTo, nFrom;           /* Number of valid entries in aTo[] and aFrom[] */
  WherePath *aFrom;         /* All nFrom paths at the previous level */
  WherePath *aTo;           /* The nTo best paths at the current level */
  WherePath *pFrom;         /* An element of aFrom[] that we are working on */
  WherePath *pTo;           /* An element of aTo[] that we are working on */
  WhereLoop *pWLoop;        /* One of the WhereLoop objects */
  WhereLoop **pX;           /* Used to divy up the pSpace memory */
  LogEst *aSortCost = 0;    /* Sorting and partial sorting costs */
  char *pSpace;             /* Temporary memory used by this routine */
  int nSpace;               /* Bytes of space allocated at pSpace */

  pParse = pWInfo->pParse;
  db = pParse->db;
  nLoop = pWInfo->nLevel;
  /* TUNING: For simple queries, only the best path is tracked.
  ** For 2-way joins, the 5 best paths are followed.
  ** For joins of 3 or more tables, track the 10 best paths */
  mxChoice = (nLoop<=1) ? 1 : (nLoop==2 ? 5 : 10);
  assert( nLoop<=pWInfo->pTabList->nSrc );
  WHERETRACE(0x002, ("---- begin solver.  (nRowEst=%d)\n", nRowEst));

  /* If nRowEst is zero and there is an ORDER BY clause, ignore it. In this
  ** case the purpose of this call is to estimate the number of rows returned
  ** by the overall query. Once this estimate has been obtained, the caller
  ** will invoke this function a second time, passing the estimate as the
  ** nRowEst parameter.  */
  if( pWInfo->pOrderBy==0 || nRowEst==0 ){
    nOrderBy = 0;
  }else{
    nOrderBy = pWInfo->pOrderBy->nExpr;
  }

  /* Allocate and initialize space for aTo, aFrom and aSortCost[] */
  nSpace = (sizeof(WherePath)+sizeof(WhereLoop*)*nLoop)*mxChoice*2;
  nSpace += sizeof(LogEst) * nOrderBy;
  pSpace = sqlite3DbMallocRawNN(db, nSpace);
  if( pSpace==0 ) return SQLITE_NOMEM_BKPT;
  aTo = (WherePath*)pSpace;
  aFrom = aTo+mxChoice;
  memset(aFrom, 0, sizeof(aFrom[0]));
  pX = (WhereLoop**)(aFrom+mxChoice);
  for(ii=mxChoice*2, pFrom=aTo; ii>0; ii--, pFrom++, pX += nLoop){
    pFrom->aLoop = pX;
  }
  if( nOrderBy ){
    /* If there is an ORDER BY clause and it is not being ignored, set up
    ** space for the aSortCost[] array. Each element of the aSortCost array
    ** is either zero - meaning it has not yet been initialized - or the
    ** cost of sorting nRowEst rows of data where the first X terms of
    ** the ORDER BY clause are already in order, where X is the array 
    ** index.  */
    aSortCost = (LogEst*)pX;
    memset(aSortCost, 0, sizeof(LogEst) * nOrderBy);
  }
  assert( aSortCost==0 || &pSpace[nSpace]==(char*)&aSortCost[nOrderBy] );
  assert( aSortCost!=0 || &pSpace[nSpace]==(char*)pX );

  /* Seed the search with a single WherePath containing zero WhereLoops.
  **
  ** TUNING: Do not let the number of iterations go above 28.  If the cost
  ** of computing an automatic index is not paid back within the first 28
  ** rows, then do not use the automatic index. */
  aFrom[0].nRow = MIN(pParse->nQueryLoop, 48);  assert( 48==sqlite3LogEst(28) );
  nFrom = 1;
  assert( aFrom[0].isOrdered==0 );
  if( nOrderBy ){
    /* If nLoop is zero, then there are no FROM terms in the query. Since
    ** in this case the query may return a maximum of one row, the results
    ** are already in the requested order. Set isOrdered to nOrderBy to
    ** indicate this. Or, if nLoop is greater than zero, set isOrdered to
    ** -1, indicating that the result set may or may not be ordered, 
    ** depending on the loops added to the current plan.  */
    aFrom[0].isOrdered = nLoop>0 ? -1 : nOrderBy;
  }

  /* Compute successively longer WherePaths using the previous generation
  ** of WherePaths as the basis for the next.  Keep track of the mxChoice
  ** best paths at each generation */
  for(iLoop=0; iLoop<nLoop; iLoop++){
    nTo = 0;
    for(ii=0, pFrom=aFrom; ii<nFrom; ii++, pFrom++){
      for(pWLoop=pWInfo->pLoops; pWLoop; pWLoop=pWLoop->pNextLoop){
        LogEst nOut;                      /* Rows visited by (pFrom+pWLoop) */
        LogEst rCost;                     /* Cost of path (pFrom+pWLoop) */
        LogEst rUnsorted;                 /* Unsorted cost of (pFrom+pWLoop) */
        i8 isOrdered = pFrom->isOrdered;  /* isOrdered for (pFrom+pWLoop) */
        Bitmask maskNew;                  /* Mask of src visited by (..) */
        Bitmask revMask = 0;              /* Mask of rev-order loops for (..) */

        if( (pWLoop->prereq & ~pFrom->maskLoop)!=0 ) continue;
        if( (pWLoop->maskSelf & pFrom->maskLoop)!=0 ) continue;
        if( (pWLoop->wsFlags & WHERE_AUTO_INDEX)!=0 && pFrom->nRow<3 ){
          /* Do not use an automatic index if the this loop is expected
          ** to run less than 1.25 times.  It is tempting to also exclude
          ** automatic index usage on an outer loop, but sometimes an automatic
          ** index is useful in the outer loop of a correlated subquery. */
          assert( 10==sqlite3LogEst(2) );
          continue;
        }

        /* At this point, pWLoop is a candidate to be the next loop. 
        ** Compute its cost */
        rUnsorted = sqlite3LogEstAdd(pWLoop->rSetup,pWLoop->rRun + pFrom->nRow);
        rUnsorted = sqlite3LogEstAdd(rUnsorted, pFrom->rUnsorted);
        nOut = pFrom->nRow + pWLoop->nOut;
        maskNew = pFrom->maskLoop | pWLoop->maskSelf;
        if( isOrdered<0 ){
          isOrdered = wherePathSatisfiesOrderBy(pWInfo,
                       pWInfo->pOrderBy, pFrom, pWInfo->wctrlFlags,
                       iLoop, pWLoop, &revMask);
        }else{
          revMask = pFrom->revLoop;
        }
        if( isOrdered>=0 && isOrdered<nOrderBy ){
          if( aSortCost[isOrdered]==0 ){
            aSortCost[isOrdered] = whereSortingCost(
                pWInfo, nRowEst, nOrderBy, isOrdered
            );
          }
          /* TUNING:  Add a small extra penalty (5) to sorting as an
          ** extra encouragment to the query planner to select a plan
          ** where the rows emerge in the correct order without any sorting
          ** required. */
          rCost = sqlite3LogEstAdd(rUnsorted, aSortCost[isOrdered]) + 5;

          WHERETRACE(0x002,
              ("---- sort cost=%-3d (%d/%d) increases cost %3d to %-3d\n",
               aSortCost[isOrdered], (nOrderBy-isOrdered), nOrderBy, 
               rUnsorted, rCost));
        }else{
          rCost = rUnsorted;
          rUnsorted -= 2;  /* TUNING:  Slight bias in favor of no-sort plans */
        }

        /* Check to see if pWLoop should be added to the set of
        ** mxChoice best-so-far paths.
        **
        ** First look for an existing path among best-so-far paths
        ** that covers the same set of loops and has the same isOrdered
        ** setting as the current path candidate.
        **
        ** The term "((pTo->isOrdered^isOrdered)&0x80)==0" is equivalent
        ** to (pTo->isOrdered==(-1))==(isOrdered==(-1))" for the range
        ** of legal values for isOrdered, -1..64.
        */
        for(jj=0, pTo=aTo; jj<nTo; jj++, pTo++){
          if( pTo->maskLoop==maskNew
           && ((pTo->isOrdered^isOrdered)&0x80)==0
          ){
            testcase( jj==nTo-1 );
            break;
          }
        }
        if( jj>=nTo ){
          /* None of the existing best-so-far paths match the candidate. */
          if( nTo>=mxChoice
           && (rCost>mxCost || (rCost==mxCost && rUnsorted>=mxUnsorted))
          ){
            /* The current candidate is no better than any of the mxChoice
            ** paths currently in the best-so-far buffer.  So discard
            ** this candidate as not viable. */
#ifdef WHERETRACE_ENABLED /* 0x4 */
            if( sqlite3WhereTrace&0x4 ){
              sqlite3DebugPrintf("Skip   %s cost=%-3d,%3d,%3d order=%c\n",
                  wherePathName(pFrom, iLoop, pWLoop), rCost, nOut, rUnsorted,
                  isOrdered>=0 ? isOrdered+'0' : '?');
            }
#endif
            continue;
          }
          /* If we reach this points it means that the new candidate path
          ** needs to be added to the set of best-so-far paths. */
          if( nTo<mxChoice ){
            /* Increase the size of the aTo set by one */
            jj = nTo++;
          }else{
            /* New path replaces the prior worst to keep count below mxChoice */
            jj = mxI;
          }
          pTo = &aTo[jj];
#ifdef WHERETRACE_ENABLED /* 0x4 */
          if( sqlite3WhereTrace&0x4 ){
            sqlite3DebugPrintf("New    %s cost=%-3d,%3d,%3d order=%c\n",
                wherePathName(pFrom, iLoop, pWLoop), rCost, nOut, rUnsorted,
                isOrdered>=0 ? isOrdered+'0' : '?');
          }
#endif
        }else{
          /* Control reaches here if best-so-far path pTo=aTo[jj] covers the
          ** same set of loops and has the same isOrdered setting as the
          ** candidate path.  Check to see if the candidate should replace
          ** pTo or if the candidate should be skipped.
          ** 
          ** The conditional is an expanded vector comparison equivalent to:
          **   (pTo->rCost,pTo->nRow,pTo->rUnsorted) <= (rCost,nOut,rUnsorted)
          */
          if( pTo->rCost<rCost 
           || (pTo->rCost==rCost
               && (pTo->nRow<nOut
                   || (pTo->nRow==nOut && pTo->rUnsorted<=rUnsorted)
                  )
              )
          ){
#ifdef WHERETRACE_ENABLED /* 0x4 */
            if( sqlite3WhereTrace&0x4 ){
              sqlite3DebugPrintf(
                  "Skip   %s cost=%-3d,%3d,%3d order=%c",
                  wherePathName(pFrom, iLoop, pWLoop), rCost, nOut, rUnsorted,
                  isOrdered>=0 ? isOrdered+'0' : '?');
              sqlite3DebugPrintf("   vs %s cost=%-3d,%3d,%3d order=%c\n",
                  wherePathName(pTo, iLoop+1, 0), pTo->rCost, pTo->nRow,
                  pTo->rUnsorted, pTo->isOrdered>=0 ? pTo->isOrdered+'0' : '?');
            }
#endif
            /* Discard the candidate path from further consideration */
            testcase( pTo->rCost==rCost );
            continue;
          }
          testcase( pTo->rCost==rCost+1 );
          /* Control reaches here if the candidate path is better than the
          ** pTo path.  Replace pTo with the candidate. */
#ifdef WHERETRACE_ENABLED /* 0x4 */
          if( sqlite3WhereTrace&0x4 ){
            sqlite3DebugPrintf(
                "Update %s cost=%-3d,%3d,%3d order=%c",
                wherePathName(pFrom, iLoop, pWLoop), rCost, nOut, rUnsorted,
                isOrdered>=0 ? isOrdered+'0' : '?');
            sqlite3DebugPrintf("  was %s cost=%-3d,%3d,%3d order=%c\n",
                wherePathName(pTo, iLoop+1, 0), pTo->rCost, pTo->nRow,
                pTo->rUnsorted, pTo->isOrdered>=0 ? pTo->isOrdered+'0' : '?');
          }
#endif
        }
        /* pWLoop is a winner.  Add it to the set of best so far */
        pTo->maskLoop = pFrom->maskLoop | pWLoop->maskSelf;
        pTo->revLoop = revMask;
        pTo->nRow = nOut;
        pTo->rCost = rCost;
        pTo->rUnsorted = rUnsorted;
        pTo->isOrdered = isOrdered;
        memcpy(pTo->aLoop, pFrom->aLoop, sizeof(WhereLoop*)*iLoop);
        pTo->aLoop[iLoop] = pWLoop;
        if( nTo>=mxChoice ){
          mxI = 0;
          mxCost = aTo[0].rCost;
          mxUnsorted = aTo[0].nRow;
          for(jj=1, pTo=&aTo[1]; jj<mxChoice; jj++, pTo++){
            if( pTo->rCost>mxCost 
             || (pTo->rCost==mxCost && pTo->rUnsorted>mxUnsorted) 
            ){
              mxCost = pTo->rCost;
              mxUnsorted = pTo->rUnsorted;
              mxI = jj;
            }
          }
        }
      }
    }

#ifdef WHERETRACE_ENABLED  /* >=2 */
    if( sqlite3WhereTrace & 0x02 ){
      sqlite3DebugPrintf("---- after round %d ----\n", iLoop);
      for(ii=0, pTo=aTo; ii<nTo; ii++, pTo++){
        sqlite3DebugPrintf(" %s cost=%-3d nrow=%-3d order=%c",
           wherePathName(pTo, iLoop+1, 0), pTo->rCost, pTo->nRow,
           pTo->isOrdered>=0 ? (pTo->isOrdered+'0') : '?');
        if( pTo->isOrdered>0 ){
          sqlite3DebugPrintf(" rev=0x%llx\n", pTo->revLoop);
        }else{
          sqlite3DebugPrintf("\n");
        }
      }
    }
#endif

    /* Swap the roles of aFrom and aTo for the next generation */
    pFrom = aTo;
    aTo = aFrom;
    aFrom = pFrom;
    nFrom = nTo;
  }

  if( nFrom==0 ){
    sqlite3ErrorMsg(pParse, "no query solution");
    sqlite3DbFreeNN(db, pSpace);
    return SQLITE_ERROR;
  }
  
  /* Find the lowest cost path.  pFrom will be left pointing to that path */
  pFrom = aFrom;
  for(ii=1; ii<nFrom; ii++){
    if( pFrom->rCost>aFrom[ii].rCost ) pFrom = &aFrom[ii];
  }
  assert( pWInfo->nLevel==nLoop );
  /* Load the lowest cost path into pWInfo */
  for(iLoop=0; iLoop<nLoop; iLoop++){
    WhereLevel *pLevel = pWInfo->a + iLoop;
    pLevel->pWLoop = pWLoop = pFrom->aLoop[iLoop];
    pLevel->iFrom = pWLoop->iTab;
    pLevel->iTabCur = pWInfo->pTabList->a[pLevel->iFrom].iCursor;
  }
  if( (pWInfo->wctrlFlags & WHERE_WANT_DISTINCT)!=0
   && (pWInfo->wctrlFlags & WHERE_DISTINCTBY)==0
   && pWInfo->eDistinct==WHERE_DISTINCT_NOOP
   && nRowEst
  ){
    Bitmask notUsed;
    int rc = wherePathSatisfiesOrderBy(pWInfo, pWInfo->pResultSet, pFrom,
                 WHERE_DISTINCTBY, nLoop-1, pFrom->aLoop[nLoop-1], &notUsed);
    if( rc==pWInfo->pResultSet->nExpr ){
      pWInfo->eDistinct = WHERE_DISTINCT_ORDERED;
    }
  }
  pWInfo->bOrderedInnerLoop = 0;
  if( pWInfo->pOrderBy ){
    if( pWInfo->wctrlFlags & WHERE_DISTINCTBY ){
      if( pFrom->isOrdered==pWInfo->pOrderBy->nExpr ){
        pWInfo->eDistinct = WHERE_DISTINCT_ORDERED;
      }
    }else{
      pWInfo->nOBSat = pFrom->isOrdered;
      pWInfo->revMask = pFrom->revLoop;
      if( pWInfo->nOBSat<=0 ){
        pWInfo->nOBSat = 0;
        if( nLoop>0 ){
          u32 wsFlags = pFrom->aLoop[nLoop-1]->wsFlags;
          if( (wsFlags & WHERE_ONEROW)==0 
           && (wsFlags&(WHERE_IPK|WHERE_COLUMN_IN))!=(WHERE_IPK|WHERE_COLUMN_IN)
          ){
            Bitmask m = 0;
            int rc = wherePathSatisfiesOrderBy(pWInfo, pWInfo->pOrderBy, pFrom,
                      WHERE_ORDERBY_LIMIT, nLoop-1, pFrom->aLoop[nLoop-1], &m);
            testcase( wsFlags & WHERE_IPK );
            testcase( wsFlags & WHERE_COLUMN_IN );
            if( rc==pWInfo->pOrderBy->nExpr ){
              pWInfo->bOrderedInnerLoop = 1;
              pWInfo->revMask = m;
            }
          }
        }
      }
    }
    if( (pWInfo->wctrlFlags & WHERE_SORTBYGROUP)
        && pWInfo->nOBSat==pWInfo->pOrderBy->nExpr && nLoop>0
    ){
      Bitmask revMask = 0;
      int nOrder = wherePathSatisfiesOrderBy(pWInfo, pWInfo->pOrderBy, 
          pFrom, 0, nLoop-1, pFrom->aLoop[nLoop-1], &revMask
      );
      assert( pWInfo->sorted==0 );
      if( nOrder==pWInfo->pOrderBy->nExpr ){
        pWInfo->sorted = 1;
        pWInfo->revMask = revMask;
      }
    }
  }


  pWInfo->nRowOut = pFrom->nRow;

  /* Free temporary memory and return success */
  sqlite3DbFreeNN(db, pSpace);
  return SQLITE_OK;
}

/*
** Most queries use only a single table (they are not joins) and have
** simple == constraints against indexed fields.  This routine attempts
** to plan those simple cases using much less ceremony than the
** general-purpose query planner, and thereby yield faster sqlite3_prepare()
** times for the common case.
**
** Return non-zero on success, if this query can be handled by this
** no-frills query planner.  Return zero if this query needs the 
** general-purpose query planner.
*/
static int whereShortCut(WhereLoopBuilder *pBuilder){
  WhereInfo *pWInfo;
  struct SrcList_item *pItem;
  WhereClause *pWC;
  WhereTerm *pTerm;
  WhereLoop *pLoop;
  int iCur;
  int j;
  Table *pTab;
  Index *pIdx;

  pWInfo = pBuilder->pWInfo;
  if( pWInfo->wctrlFlags & WHERE_OR_SUBCLAUSE ) return 0;
  assert( pWInfo->pTabList->nSrc>=1 );
  pItem = pWInfo->pTabList->a;
  pTab = pItem->pTab;
  if( IsVirtual(pTab) ) return 0;
  if( pItem->fg.isIndexedBy ) return 0;
  iCur = pItem->iCursor;
  pWC = &pWInfo->sWC;
  pLoop = pBuilder->pNew;
  pLoop->wsFlags = 0;
  pLoop->nSkip = 0;
  pTerm = sqlite3WhereFindTerm(pWC, iCur, -1, 0, WO_EQ|WO_IS, 0);
  if( pTerm ){
    testcase( pTerm->eOperator & WO_IS );
    pLoop->wsFlags = WHERE_COLUMN_EQ|WHERE_IPK|WHERE_ONEROW;
    pLoop->aLTerm[0] = pTerm;
    pLoop->nLTerm = 1;
    pLoop->u.btree.nEq = 1;
    /* TUNING: Cost of a rowid lookup is 10 */
    pLoop->rRun = 33;  /* 33==sqlite3LogEst(10) */
  }else{
    for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){
      int opMask;
      assert( pLoop->aLTermSpace==pLoop->aLTerm );
      if( !IsUniqueIndex(pIdx)
       || pIdx->pPartIdxWhere!=0 
       || pIdx->nKeyCol>ArraySize(pLoop->aLTermSpace) 
      ) continue;
      opMask = pIdx->uniqNotNull ? (WO_EQ|WO_IS) : WO_EQ;
      for(j=0; j<pIdx->nKeyCol; j++){
        pTerm = sqlite3WhereFindTerm(pWC, iCur, j, 0, opMask, pIdx);
        if( pTerm==0 ) break;
        testcase( pTerm->eOperator & WO_IS );
        pLoop->aLTerm[j] = pTerm;
      }
      if( j!=pIdx->nKeyCol ) continue;
      pLoop->wsFlags = WHERE_COLUMN_EQ|WHERE_ONEROW|WHERE_INDEXED;
      if( pIdx->isCovering || (pItem->colUsed & pIdx->colNotIdxed)==0 ){
        pLoop->wsFlags |= WHERE_IDX_ONLY;
      }
      pLoop->nLTerm = j;
      pLoop->u.btree.nEq = j;
      pLoop->u.btree.pIndex = pIdx;
      /* TUNING: Cost of a unique index lookup is 15 */
      pLoop->rRun = 39;  /* 39==sqlite3LogEst(15) */
      break;
    }
  }
  if( pLoop->wsFlags ){
    pLoop->nOut = (LogEst)1;
    pWInfo->a[0].pWLoop = pLoop;
    assert( pWInfo->sMaskSet.n==1 && iCur==pWInfo->sMaskSet.ix[0] );
    pLoop->maskSelf = 1; /* sqlite3WhereGetMask(&pWInfo->sMaskSet, iCur); */
    pWInfo->a[0].iTabCur = iCur;
    pWInfo->nRowOut = 1;
    if( pWInfo->pOrderBy ) pWInfo->nOBSat =  pWInfo->pOrderBy->nExpr;
    if( pWInfo->wctrlFlags & WHERE_WANT_DISTINCT ){
      pWInfo->eDistinct = WHERE_DISTINCT_UNIQUE;
    }
#ifdef SQLITE_DEBUG
    pLoop->cId = '0';
#endif
    return 1;
  }
  return 0;
}

/*
** Helper function for exprIsDeterministic().
*/
static int exprNodeIsDeterministic(Walker *pWalker, Expr *pExpr){
  if( pExpr->op==TK_FUNCTION && ExprHasProperty(pExpr, EP_ConstFunc)==0 ){
    pWalker->eCode = 0;
    return WRC_Abort;
  }
  return WRC_Continue;
}

/*
** Return true if the expression contains no non-deterministic SQL 
** functions. Do not consider non-deterministic SQL functions that are 
** part of sub-select statements.
*/
static int exprIsDeterministic(Expr *p){
  Walker w;
  memset(&w, 0, sizeof(w));
  w.eCode = 1;
  w.xExprCallback = exprNodeIsDeterministic;
  w.xSelectCallback = sqlite3SelectWalkFail;
  sqlite3WalkExpr(&w, p);
  return w.eCode;
}

/*
** Generate the beginning of the loop used for WHERE clause processing.
** The return value is a pointer to an opaque structure that contains
** information needed to terminate the loop.  Later, the calling routine
** should invoke sqlite3WhereEnd() with the return value of this function
** in order to complete the WHERE clause processing.
**
** If an error occurs, this routine returns NULL.
**
** The basic idea is to do a nested loop, one loop for each table in
** the FROM clause of a select.  (INSERT and UPDATE statements are the
** same as a SELECT with only a single table in the FROM clause.)  For
** example, if the SQL is this:
**
**       SELECT * FROM t1, t2, t3 WHERE ...;
**
** Then the code generated is conceptually like the following:
**
**      foreach row1 in t1 do       \    Code generated
**        foreach row2 in t2 do      |-- by sqlite3WhereBegin()
**          foreach row3 in t3 do   /
**            ...
**          end                     \    Code generated
**        end                        |-- by sqlite3WhereEnd()
**      end                         /
**
** Note that the loops might not be nested in the order in which they
** appear in the FROM clause if a different order is better able to make
** use of indices.  Note also that when the IN operator appears in
** the WHERE clause, it might result in additional nested loops for
** scanning through all values on the right-hand side of the IN.
**
** There are Btree cursors associated with each table.  t1 uses cursor
** number pTabList->a[0].iCursor.  t2 uses the cursor pTabList->a[1].iCursor.
** And so forth.  This routine generates code to open those VDBE cursors
** and sqlite3WhereEnd() generates the code to close them.
**
** The code that sqlite3WhereBegin() generates leaves the cursors named
** in pTabList pointing at their appropriate entries.  The [...] code
** can use OP_Column and OP_Rowid opcodes on these cursors to extract
** data from the various tables of the loop.
**
** If the WHERE clause is empty, the foreach loops must each scan their
** entire tables.  Thus a three-way join is an O(N^3) operation.  But if
** the tables have indices and there are terms in the WHERE clause that
** refer to those indices, a complete table scan can be avoided and the
** code will run much faster.  Most of the work of this routine is checking
** to see if there are indices that can be used to speed up the loop.
**
** Terms of the WHERE clause are also used to limit which rows actually
** make it to the "..." in the middle of the loop.  After each "foreach",
** terms of the WHERE clause that use only terms in that loop and outer
** loops are evaluated and if false a jump is made around all subsequent
** inner loops (or around the "..." if the test occurs within the inner-
** most loop)
**
** OUTER JOINS
**
** An outer join of tables t1 and t2 is conceptally coded as follows:
**
**    foreach row1 in t1 do
**      flag = 0
**      foreach row2 in t2 do
**        start:
**          ...
**          flag = 1
**      end
**      if flag==0 then
**        move the row2 cursor to a null row
**        goto start
**      fi
**    end
**
** ORDER BY CLAUSE PROCESSING
**
** pOrderBy is a pointer to the ORDER BY clause (or the GROUP BY clause
** if the WHERE_GROUPBY flag is set in wctrlFlags) of a SELECT statement
** if there is one.  If there is no ORDER BY clause or if this routine
** is called from an UPDATE or DELETE statement, then pOrderBy is NULL.
**
** The iIdxCur parameter is the cursor number of an index.  If 
** WHERE_OR_SUBCLAUSE is set, iIdxCur is the cursor number of an index
** to use for OR clause processing.  The WHERE clause should use this
** specific cursor.  If WHERE_ONEPASS_DESIRED is set, then iIdxCur is
** the first cursor in an array of cursors for all indices.  iIdxCur should
** be used to compute the appropriate cursor depending on which index is
** used.
*/
SQLITE_PRIVATE WhereInfo *sqlite3WhereBegin(
  Parse *pParse,          /* The parser context */
  SrcList *pTabList,      /* FROM clause: A list of all tables to be scanned */
  Expr *pWhere,           /* The WHERE clause */
  ExprList *pOrderBy,     /* An ORDER BY (or GROUP BY) clause, or NULL */
  ExprList *pResultSet,   /* Query result set.  Req'd for DISTINCT */
  u16 wctrlFlags,         /* The WHERE_* flags defined in sqliteInt.h */
  int iAuxArg             /* If WHERE_OR_SUBCLAUSE is set, index cursor number
                          ** If WHERE_USE_LIMIT, then the limit amount */
){
  int nByteWInfo;            /* Num. bytes allocated for WhereInfo struct */
  int nTabList;              /* Number of elements in pTabList */
  WhereInfo *pWInfo;         /* Will become the return value of this function */
  Vdbe *v = pParse->pVdbe;   /* The virtual database engine */
  Bitmask notReady;          /* Cursors that are not yet positioned */
  WhereLoopBuilder sWLB;     /* The WhereLoop builder */
  WhereMaskSet *pMaskSet;    /* The expression mask set */
  WhereLevel *pLevel;        /* A single level in pWInfo->a[] */
  WhereLoop *pLoop;          /* Pointer to a single WhereLoop object */
  int ii;                    /* Loop counter */
  sqlite3 *db;               /* Database connection */
  int rc;                    /* Return code */
  u8 bFordelete = 0;         /* OPFLAG_FORDELETE or zero, as appropriate */

  assert( (wctrlFlags & WHERE_ONEPASS_MULTIROW)==0 || (
        (wctrlFlags & WHERE_ONEPASS_DESIRED)!=0 
     && (wctrlFlags & WHERE_OR_SUBCLAUSE)==0 
  ));

  /* Only one of WHERE_OR_SUBCLAUSE or WHERE_USE_LIMIT */
  assert( (wctrlFlags & WHERE_OR_SUBCLAUSE)==0
            || (wctrlFlags & WHERE_USE_LIMIT)==0 );

  /* Variable initialization */
  db = pParse->db;
  memset(&sWLB, 0, sizeof(sWLB));

  /* An ORDER/GROUP BY clause of more than 63 terms cannot be optimized */
  testcase( pOrderBy && pOrderBy->nExpr==BMS-1 );
  if( pOrderBy && pOrderBy->nExpr>=BMS ) pOrderBy = 0;
  sWLB.pOrderBy = pOrderBy;

  /* Disable the DISTINCT optimization if SQLITE_DistinctOpt is set via
  ** sqlite3_test_ctrl(SQLITE_TESTCTRL_OPTIMIZATIONS,...) */
  if( OptimizationDisabled(db, SQLITE_DistinctOpt) ){
    wctrlFlags &= ~WHERE_WANT_DISTINCT;
  }

  /* The number of tables in the FROM clause is limited by the number of
  ** bits in a Bitmask 
  */
  testcase( pTabList->nSrc==BMS );
  if( pTabList->nSrc>BMS ){
    sqlite3ErrorMsg(pParse, "at most %d tables in a join", BMS);
    return 0;
  }

  /* This function normally generates a nested loop for all tables in 
  ** pTabList.  But if the WHERE_OR_SUBCLAUSE flag is set, then we should
  ** only generate code for the first table in pTabList and assume that
  ** any cursors associated with subsequent tables are uninitialized.
  */
  nTabList = (wctrlFlags & WHERE_OR_SUBCLAUSE) ? 1 : pTabList->nSrc;

  /* Allocate and initialize the WhereInfo structure that will become the
  ** return value. A single allocation is used to store the WhereInfo
  ** struct, the contents of WhereInfo.a[], the WhereClause structure
  ** and the WhereMaskSet structure. Since WhereClause contains an 8-byte
  ** field (type Bitmask) it must be aligned on an 8-byte boundary on
  ** some architectures. Hence the ROUND8() below.
  */
  nByteWInfo = ROUND8(sizeof(WhereInfo)+(nTabList-1)*sizeof(WhereLevel));
  pWInfo = sqlite3DbMallocRawNN(db, nByteWInfo + sizeof(WhereLoop));
  if( db->mallocFailed ){
    sqlite3DbFree(db, pWInfo);
    pWInfo = 0;
    goto whereBeginError;
  }
  pWInfo->pParse = pParse;
  pWInfo->pTabList = pTabList;
  pWInfo->pOrderBy = pOrderBy;
  pWInfo->pWhere = pWhere;
  pWInfo->pResultSet = pResultSet;
  pWInfo->aiCurOnePass[0] = pWInfo->aiCurOnePass[1] = -1;
  pWInfo->nLevel = nTabList;
  pWInfo->iBreak = pWInfo->iContinue = sqlite3VdbeMakeLabel(pParse);
  pWInfo->wctrlFlags = wctrlFlags;
  pWInfo->iLimit = iAuxArg;
  pWInfo->savedNQueryLoop = pParse->nQueryLoop;
  memset(&pWInfo->nOBSat, 0, 
         offsetof(WhereInfo,sWC) - offsetof(WhereInfo,nOBSat));
  memset(&pWInfo->a[0], 0, sizeof(WhereLoop)+nTabList*sizeof(WhereLevel));
  assert( pWInfo->eOnePass==ONEPASS_OFF );  /* ONEPASS defaults to OFF */
  pMaskSet = &pWInfo->sMaskSet;
  sWLB.pWInfo = pWInfo;
  sWLB.pWC = &pWInfo->sWC;
  sWLB.pNew = (WhereLoop*)(((char*)pWInfo)+nByteWInfo);
  assert( EIGHT_BYTE_ALIGNMENT(sWLB.pNew) );
  whereLoopInit(sWLB.pNew);
#ifdef SQLITE_DEBUG
  sWLB.pNew->cId = '*';
#endif

  /* Split the WHERE clause into separate subexpressions where each
  ** subexpression is separated by an AND operator.
  */
  initMaskSet(pMaskSet);
  sqlite3WhereClauseInit(&pWInfo->sWC, pWInfo);
  sqlite3WhereSplit(&pWInfo->sWC, pWhere, TK_AND);
    
  /* Special case: No FROM clause
  */
  if( nTabList==0 ){
    if( pOrderBy ) pWInfo->nOBSat = pOrderBy->nExpr;
    if( wctrlFlags & WHERE_WANT_DISTINCT ){
      pWInfo->eDistinct = WHERE_DISTINCT_UNIQUE;
    }
    ExplainQueryPlan((pParse, 0, "SCAN CONSTANT ROW"));
  }else{
    /* Assign a bit from the bitmask to every term in the FROM clause.
    **
    ** The N-th term of the FROM clause is assigned a bitmask of 1<<N.
    **
    ** The rule of the previous sentence ensures thta if X is the bitmask for
    ** a table T, then X-1 is the bitmask for all other tables to the left of T.
    ** Knowing the bitmask for all tables to the left of a left join is
    ** important.  Ticket #3015.
    **
    ** Note that bitmasks are created for all pTabList->nSrc tables in
    ** pTabList, not just the first nTabList tables.  nTabList is normally
    ** equal to pTabList->nSrc but might be shortened to 1 if the
    ** WHERE_OR_SUBCLAUSE flag is set.
    */
    ii = 0;
    do{
      createMask(pMaskSet, pTabList->a[ii].iCursor);
      sqlite3WhereTabFuncArgs(pParse, &pTabList->a[ii], &pWInfo->sWC);
    }while( (++ii)<pTabList->nSrc );
  #ifdef SQLITE_DEBUG
    {
      Bitmask mx = 0;
      for(ii=0; ii<pTabList->nSrc; ii++){
        Bitmask m = sqlite3WhereGetMask(pMaskSet, pTabList->a[ii].iCursor);
        assert( m>=mx );
        mx = m;
      }
    }
  #endif
  }
  
  /* Analyze all of the subexpressions. */
  sqlite3WhereExprAnalyze(pTabList, &pWInfo->sWC);
  if( db->mallocFailed ) goto whereBeginError;

  /* Special case: WHERE terms that do not refer to any tables in the join
  ** (constant expressions). Evaluate each such term, and jump over all the
  ** generated code if the result is not true.  
  **
  ** Do not do this if the expression contains non-deterministic functions
  ** that are not within a sub-select. This is not strictly required, but
  ** preserves SQLite's legacy behaviour in the following two cases:
  **
  **   FROM ... WHERE random()>0;           -- eval random() once per row
  **   FROM ... WHERE (SELECT random())>0;  -- eval random() once overall
  */
  for(ii=0; ii<sWLB.pWC->nTerm; ii++){
    WhereTerm *pT = &sWLB.pWC->a[ii];
    if( pT->wtFlags & TERM_VIRTUAL ) continue;
    if( pT->prereqAll==0 && (nTabList==0 || exprIsDeterministic(pT->pExpr)) ){
      sqlite3ExprIfFalse(pParse, pT->pExpr, pWInfo->iBreak, SQLITE_JUMPIFNULL);
      pT->wtFlags |= TERM_CODED;
    }
  }

  if( wctrlFlags & WHERE_WANT_DISTINCT ){
    if( isDistinctRedundant(pParse, pTabList, &pWInfo->sWC, pResultSet) ){
      /* The DISTINCT marking is pointless.  Ignore it. */
      pWInfo->eDistinct = WHERE_DISTINCT_UNIQUE;
    }else if( pOrderBy==0 ){
      /* Try to ORDER BY the result set to make distinct processing easier */
      pWInfo->wctrlFlags |= WHERE_DISTINCTBY;
      pWInfo->pOrderBy = pResultSet;
    }
  }

  /* Construct the WhereLoop objects */
#if defined(WHERETRACE_ENABLED)
  if( sqlite3WhereTrace & 0xffff ){
    sqlite3DebugPrintf("*** Optimizer Start *** (wctrlFlags: 0x%x",wctrlFlags);
    if( wctrlFlags & WHERE_USE_LIMIT ){
      sqlite3DebugPrintf(", limit: %d", iAuxArg);
    }
    sqlite3DebugPrintf(")\n");
    if( sqlite3WhereTrace & 0x100 ){
      Select sSelect;
      memset(&sSelect, 0, sizeof(sSelect));
      sSelect.selFlags = SF_WhereBegin;
      sSelect.pSrc = pTabList;
      sSelect.pWhere = pWhere;
      sSelect.pOrderBy = pOrderBy;
      sSelect.pEList = pResultSet;
      sqlite3TreeViewSelect(0, &sSelect, 0);
    }
  }
  if( sqlite3WhereTrace & 0x100 ){ /* Display all terms of the WHERE clause */
    sqlite3WhereClausePrint(sWLB.pWC);
  }
#endif

  if( nTabList!=1 || whereShortCut(&sWLB)==0 ){
    rc = whereLoopAddAll(&sWLB);
    if( rc ) goto whereBeginError;
  
#ifdef WHERETRACE_ENABLED
    if( sqlite3WhereTrace ){    /* Display all of the WhereLoop objects */
      WhereLoop *p;
      int i;
      static const char zLabel[] = "0123456789abcdefghijklmnopqrstuvwyxz"
                                             "ABCDEFGHIJKLMNOPQRSTUVWYXZ";
      for(p=pWInfo->pLoops, i=0; p; p=p->pNextLoop, i++){
        p->cId = zLabel[i%(sizeof(zLabel)-1)];
        whereLoopPrint(p, sWLB.pWC);
      }
    }
#endif
  
    wherePathSolver(pWInfo, 0);
    if( db->mallocFailed ) goto whereBeginError;
    if( pWInfo->pOrderBy ){
       wherePathSolver(pWInfo, pWInfo->nRowOut+1);
       if( db->mallocFailed ) goto whereBeginError;
    }
  }
  if( pWInfo->pOrderBy==0 && (db->flags & SQLITE_ReverseOrder)!=0 ){
     pWInfo->revMask = ALLBITS;
  }
  if( pParse->nErr || NEVER(db->mallocFailed) ){
    goto whereBeginError;
  }
#ifdef WHERETRACE_ENABLED
  if( sqlite3WhereTrace ){
    sqlite3DebugPrintf("---- Solution nRow=%d", pWInfo->nRowOut);
    if( pWInfo->nOBSat>0 ){
      sqlite3DebugPrintf(" ORDERBY=%d,0x%llx", pWInfo->nOBSat, pWInfo->revMask);
    }
    switch( pWInfo->eDistinct ){
      case WHERE_DISTINCT_UNIQUE: {
        sqlite3DebugPrintf("  DISTINCT=unique");
        break;
      }
      case WHERE_DISTINCT_ORDERED: {
        sqlite3DebugPrintf("  DISTINCT=ordered");
        break;
      }
      case WHERE_DISTINCT_UNORDERED: {
        sqlite3DebugPrintf("  DISTINCT=unordered");
        break;
      }
    }
    sqlite3DebugPrintf("\n");
    for(ii=0; ii<pWInfo->nLevel; ii++){
      whereLoopPrint(pWInfo->a[ii].pWLoop, sWLB.pWC);
    }
  }
#endif

  /* Attempt to omit tables from the join that do not affect the result.
  ** For a table to not affect the result, the following must be true:
  **
  **   1) The query must not be an aggregate.
  **   2) The table must be the RHS of a LEFT JOIN.
  **   3) Either the query must be DISTINCT, or else the ON or USING clause
  **      must contain a constraint that limits the scan of the table to 
  **      at most a single row.
  **   4) The table must not be referenced by any part of the query apart
  **      from its own USING or ON clause.
  **
  ** For example, given:
  **
  **     CREATE TABLE t1(ipk INTEGER PRIMARY KEY, v1);
  **     CREATE TABLE t2(ipk INTEGER PRIMARY KEY, v2);
  **     CREATE TABLE t3(ipk INTEGER PRIMARY KEY, v3);
  **
  ** then table t2 can be omitted from the following:
  **
  **     SELECT v1, v3 FROM t1 
  **       LEFT JOIN t2 USING (t1.ipk=t2.ipk)
  **       LEFT JOIN t3 USING (t1.ipk=t3.ipk)
  **
  ** or from:
  **
  **     SELECT DISTINCT v1, v3 FROM t1 
  **       LEFT JOIN t2
  **       LEFT JOIN t3 USING (t1.ipk=t3.ipk)
  */
  notReady = ~(Bitmask)0;
  if( pWInfo->nLevel>=2
   && pResultSet!=0               /* guarantees condition (1) above */
   && OptimizationEnabled(db, SQLITE_OmitNoopJoin)
  ){
    int i;
    Bitmask tabUsed = sqlite3WhereExprListUsage(pMaskSet, pResultSet);
    if( sWLB.pOrderBy ){
      tabUsed |= sqlite3WhereExprListUsage(pMaskSet, sWLB.pOrderBy);
    }
    for(i=pWInfo->nLevel-1; i>=1; i--){
      WhereTerm *pTerm, *pEnd;
      struct SrcList_item *pItem;
      pLoop = pWInfo->a[i].pWLoop;
      pItem = &pWInfo->pTabList->a[pLoop->iTab];
      if( (pItem->fg.jointype & JT_LEFT)==0 ) continue;
      if( (wctrlFlags & WHERE_WANT_DISTINCT)==0
       && (pLoop->wsFlags & WHERE_ONEROW)==0
      ){
        continue;
      }
      if( (tabUsed & pLoop->maskSelf)!=0 ) continue;
      pEnd = sWLB.pWC->a + sWLB.pWC->nTerm;
      for(pTerm=sWLB.pWC->a; pTerm<pEnd; pTerm++){
        if( (pTerm->prereqAll & pLoop->maskSelf)!=0 ){
          if( !ExprHasProperty(pTerm->pExpr, EP_FromJoin)
           || pTerm->pExpr->iRightJoinTable!=pItem->iCursor
          ){
            break;
          }
        }
      }
      if( pTerm<pEnd ) continue;
      WHERETRACE(0xffff, ("-> drop loop %c not used\n", pLoop->cId));
      notReady &= ~pLoop->maskSelf;
      for(pTerm=sWLB.pWC->a; pTerm<pEnd; pTerm++){
        if( (pTerm->prereqAll & pLoop->maskSelf)!=0 ){
          pTerm->wtFlags |= TERM_CODED;
        }
      }
      if( i!=pWInfo->nLevel-1 ){
        int nByte = (pWInfo->nLevel-1-i) * sizeof(WhereLevel);
        memmove(&pWInfo->a[i], &pWInfo->a[i+1], nByte);
      }
      pWInfo->nLevel--;
      nTabList--;
    }
  }
  WHERETRACE(0xffff,("*** Optimizer Finished ***\n"));
  pWInfo->pParse->nQueryLoop += pWInfo->nRowOut;

  /* If the caller is an UPDATE or DELETE statement that is requesting
  ** to use a one-pass algorithm, determine if this is appropriate.
  **
  ** A one-pass approach can be used if the caller has requested one
  ** and either (a) the scan visits at most one row or (b) each
  ** of the following are true:
  **
  **   * the caller has indicated that a one-pass approach can be used
  **     with multiple rows (by setting WHERE_ONEPASS_MULTIROW), and
  **   * the table is not a virtual table, and
  **   * either the scan does not use the OR optimization or the caller
  **     is a DELETE operation (WHERE_DUPLICATES_OK is only specified
  **     for DELETE).
  **
  ** The last qualification is because an UPDATE statement uses
  ** WhereInfo.aiCurOnePass[1] to determine whether or not it really can
  ** use a one-pass approach, and this is not set accurately for scans
  ** that use the OR optimization.
  */
  assert( (wctrlFlags & WHERE_ONEPASS_DESIRED)==0 || pWInfo->nLevel==1 );
  if( (wctrlFlags & WHERE_ONEPASS_DESIRED)!=0 ){
    int wsFlags = pWInfo->a[0].pWLoop->wsFlags;
    int bOnerow = (wsFlags & WHERE_ONEROW)!=0;
    assert( !(wsFlags & WHERE_VIRTUALTABLE) || IsVirtual(pTabList->a[0].pTab) );
    if( bOnerow || (
        0!=(wctrlFlags & WHERE_ONEPASS_MULTIROW)
     && !IsVirtual(pTabList->a[0].pTab)
     && (0==(wsFlags & WHERE_MULTI_OR) || (wctrlFlags & WHERE_DUPLICATES_OK))
    )){
      pWInfo->eOnePass = bOnerow ? ONEPASS_SINGLE : ONEPASS_MULTI;
      if( HasRowid(pTabList->a[0].pTab) && (wsFlags & WHERE_IDX_ONLY) ){
        if( wctrlFlags & WHERE_ONEPASS_MULTIROW ){
          bFordelete = OPFLAG_FORDELETE;
        }
        pWInfo->a[0].pWLoop->wsFlags = (wsFlags & ~WHERE_IDX_ONLY);
      }
    }
  }

  /* Open all tables in the pTabList and any indices selected for
  ** searching those tables.
  */
  for(ii=0, pLevel=pWInfo->a; ii<nTabList; ii++, pLevel++){
    Table *pTab;     /* Table to open */
    int iDb;         /* Index of database containing table/index */
    struct SrcList_item *pTabItem;

    pTabItem = &pTabList->a[pLevel->iFrom];
    pTab = pTabItem->pTab;
    iDb = sqlite3SchemaToIndex(db, pTab->pSchema);
    pLoop = pLevel->pWLoop;
    if( (pTab->tabFlags & TF_Ephemeral)!=0 || pTab->pSelect ){
      /* Do nothing */
    }else
#ifndef SQLITE_OMIT_VIRTUALTABLE
    if( (pLoop->wsFlags & WHERE_VIRTUALTABLE)!=0 ){
      const char *pVTab = (const char *)sqlite3GetVTable(db, pTab);
      int iCur = pTabItem->iCursor;
      sqlite3VdbeAddOp4(v, OP_VOpen, iCur, 0, 0, pVTab, P4_VTAB);
    }else if( IsVirtual(pTab) ){
      /* noop */
    }else
#endif
    if( (pLoop->wsFlags & WHERE_IDX_ONLY)==0
         && (wctrlFlags & WHERE_OR_SUBCLAUSE)==0 ){
      int op = OP_OpenRead;
      if( pWInfo->eOnePass!=ONEPASS_OFF ){
        op = OP_OpenWrite;
        pWInfo->aiCurOnePass[0] = pTabItem->iCursor;
      };
      sqlite3OpenTable(pParse, pTabItem->iCursor, iDb, pTab, op);
      assert( pTabItem->iCursor==pLevel->iTabCur );
      testcase( pWInfo->eOnePass==ONEPASS_OFF && pTab->nCol==BMS-1 );
      testcase( pWInfo->eOnePass==ONEPASS_OFF && pTab->nCol==BMS );
      if( pWInfo->eOnePass==ONEPASS_OFF && pTab->nCol<BMS && HasRowid(pTab) ){
        Bitmask b = pTabItem->colUsed;
        int n = 0;
        for(; b; b=b>>1, n++){}
        sqlite3VdbeChangeP4(v, -1, SQLITE_INT_TO_PTR(n), P4_INT32);
        assert( n<=pTab->nCol );
      }
#ifdef SQLITE_ENABLE_CURSOR_HINTS
      if( pLoop->u.btree.pIndex!=0 ){
        sqlite3VdbeChangeP5(v, OPFLAG_SEEKEQ|bFordelete);
      }else
#endif
      {
        sqlite3VdbeChangeP5(v, bFordelete);
      }
#ifdef SQLITE_ENABLE_COLUMN_USED_MASK
      sqlite3VdbeAddOp4Dup8(v, OP_ColumnsUsed, pTabItem->iCursor, 0, 0,
                            (const u8*)&pTabItem->colUsed, P4_INT64);
#endif
    }else{
      sqlite3TableLock(pParse, iDb, pTab->tnum, 0, pTab->zName);
    }
    if( pLoop->wsFlags & WHERE_INDEXED ){
      Index *pIx = pLoop->u.btree.pIndex;
      int iIndexCur;
      int op = OP_OpenRead;
      /* iAuxArg is always set to a positive value if ONEPASS is possible */
      assert( iAuxArg!=0 || (pWInfo->wctrlFlags & WHERE_ONEPASS_DESIRED)==0 );
      if( !HasRowid(pTab) && IsPrimaryKeyIndex(pIx)
       && (wctrlFlags & WHERE_OR_SUBCLAUSE)!=0
      ){
        /* This is one term of an OR-optimization using the PRIMARY KEY of a
        ** WITHOUT ROWID table.  No need for a separate index */
        iIndexCur = pLevel->iTabCur;
        op = 0;
      }else if( pWInfo->eOnePass!=ONEPASS_OFF ){
        Index *pJ = pTabItem->pTab->pIndex;
        iIndexCur = iAuxArg;
        assert( wctrlFlags & WHERE_ONEPASS_DESIRED );
        while( ALWAYS(pJ) && pJ!=pIx ){
          iIndexCur++;
          pJ = pJ->pNext;
        }
        op = OP_OpenWrite;
        pWInfo->aiCurOnePass[1] = iIndexCur;
      }else if( iAuxArg && (wctrlFlags & WHERE_OR_SUBCLAUSE)!=0 ){
        iIndexCur = iAuxArg;
        op = OP_ReopenIdx;
      }else{
        iIndexCur = pParse->nTab++;
      }
      pLevel->iIdxCur = iIndexCur;
      assert( pIx->pSchema==pTab->pSchema );
      assert( iIndexCur>=0 );
      if( op ){
        sqlite3VdbeAddOp3(v, op, iIndexCur, pIx->tnum, iDb);
        sqlite3VdbeSetP4KeyInfo(pParse, pIx);
        if( (pLoop->wsFlags & WHERE_CONSTRAINT)!=0
         && (pLoop->wsFlags & (WHERE_COLUMN_RANGE|WHERE_SKIPSCAN))==0
         && (pLoop->wsFlags & WHERE_BIGNULL_SORT)==0
         && (pWInfo->wctrlFlags&WHERE_ORDERBY_MIN)==0
         && pWInfo->eDistinct!=WHERE_DISTINCT_ORDERED
        ){
          sqlite3VdbeChangeP5(v, OPFLAG_SEEKEQ); /* Hint to COMDB2 */
        }
        VdbeComment((v, "%s", pIx->zName));
#ifdef SQLITE_ENABLE_COLUMN_USED_MASK
        {
          u64 colUsed = 0;
          int ii, jj;
          for(ii=0; ii<pIx->nColumn; ii++){
            jj = pIx->aiColumn[ii];
            if( jj<0 ) continue;
            if( jj>63 ) jj = 63;
            if( (pTabItem->colUsed & MASKBIT(jj))==0 ) continue;
            colUsed |= ((u64)1)<<(ii<63 ? ii : 63);
          }
          sqlite3VdbeAddOp4Dup8(v, OP_ColumnsUsed, iIndexCur, 0, 0,
                                (u8*)&colUsed, P4_INT64);
        }
#endif /* SQLITE_ENABLE_COLUMN_USED_MASK */
      }
    }
    if( iDb>=0 ) sqlite3CodeVerifySchema(pParse, iDb);
  }
  pWInfo->iTop = sqlite3VdbeCurrentAddr(v);
  if( db->mallocFailed ) goto whereBeginError;

  /* Generate the code to do the search.  Each iteration of the for
  ** loop below generates code for a single nested loop of the VM
  ** program.
  */
  for(ii=0; ii<nTabList; ii++){
    int addrExplain;
    int wsFlags;
    pLevel = &pWInfo->a[ii];
    wsFlags = pLevel->pWLoop->wsFlags;
#ifndef SQLITE_OMIT_AUTOMATIC_INDEX
    if( (pLevel->pWLoop->wsFlags & WHERE_AUTO_INDEX)!=0 ){
      constructAutomaticIndex(pParse, &pWInfo->sWC,
                &pTabList->a[pLevel->iFrom], notReady, pLevel);
      if( db->mallocFailed ) goto whereBeginError;
    }
#endif
    addrExplain = sqlite3WhereExplainOneScan(
        pParse, pTabList, pLevel, wctrlFlags
    );
    pLevel->addrBody = sqlite3VdbeCurrentAddr(v);
    notReady = sqlite3WhereCodeOneLoopStart(pParse,v,pWInfo,ii,pLevel,notReady);
    pWInfo->iContinue = pLevel->addrCont;
    if( (wsFlags&WHERE_MULTI_OR)==0 && (wctrlFlags&WHERE_OR_SUBCLAUSE)==0 ){
      sqlite3WhereAddScanStatus(v, pTabList, pLevel, addrExplain);
    }
  }

  /* Done. */
  VdbeModuleComment((v, "Begin WHERE-core"));
  return pWInfo;

  /* Jump here if malloc fails */
whereBeginError:
  if( pWInfo ){
    pParse->nQueryLoop = pWInfo->savedNQueryLoop;
    whereInfoFree(db, pWInfo);
  }
  return 0;
}

/*
** Part of sqlite3WhereEnd() will rewrite opcodes to reference the
** index rather than the main table.  In SQLITE_DEBUG mode, we want
** to trace those changes if PRAGMA vdbe_addoptrace=on.  This routine
** does that.
*/
#ifndef SQLITE_DEBUG
# define OpcodeRewriteTrace(D,K,P) /* no-op */
#else
# define OpcodeRewriteTrace(D,K,P) sqlite3WhereOpcodeRewriteTrace(D,K,P)
  static void sqlite3WhereOpcodeRewriteTrace(
    sqlite3 *db,
    int pc,
    VdbeOp *pOp
  ){
    if( (db->flags & SQLITE_VdbeAddopTrace)==0 ) return;
    sqlite3VdbePrintOp(0, pc, pOp);
  }
#endif

/*
** Generate the end of the WHERE loop.  See comments on 
** sqlite3WhereBegin() for additional information.
*/
SQLITE_PRIVATE void sqlite3WhereEnd(WhereInfo *pWInfo){
  Parse *pParse = pWInfo->pParse;
  Vdbe *v = pParse->pVdbe;
  int i;
  WhereLevel *pLevel;
  WhereLoop *pLoop;
  SrcList *pTabList = pWInfo->pTabList;
  sqlite3 *db = pParse->db;

  /* Generate loop termination code.
  */
  VdbeModuleComment((v, "End WHERE-core"));
  for(i=pWInfo->nLevel-1; i>=0; i--){
    int addr;
    pLevel = &pWInfo->a[i];
    pLoop = pLevel->pWLoop;
    if( pLevel->op!=OP_Noop ){
#ifndef SQLITE_DISABLE_SKIPAHEAD_DISTINCT
      int addrSeek = 0;
      Index *pIdx;
      int n;
      if( pWInfo->eDistinct==WHERE_DISTINCT_ORDERED
       && i==pWInfo->nLevel-1  /* Ticket [ef9318757b152e3] 2017-10-21 */
       && (pLoop->wsFlags & WHERE_INDEXED)!=0
       && (pIdx = pLoop->u.btree.pIndex)->hasStat1
       && (n = pLoop->u.btree.nDistinctCol)>0
       && pIdx->aiRowLogEst[n]>=36
      ){
        int r1 = pParse->nMem+1;
        int j, op;
        for(j=0; j<n; j++){
          sqlite3VdbeAddOp3(v, OP_Column, pLevel->iIdxCur, j, r1+j);
        }
        pParse->nMem += n+1;
        op = pLevel->op==OP_Prev ? OP_SeekLT : OP_SeekGT;
        addrSeek = sqlite3VdbeAddOp4Int(v, op, pLevel->iIdxCur, 0, r1, n);
        VdbeCoverageIf(v, op==OP_SeekLT);
        VdbeCoverageIf(v, op==OP_SeekGT);
        sqlite3VdbeAddOp2(v, OP_Goto, 1, pLevel->p2);
      }
#endif /* SQLITE_DISABLE_SKIPAHEAD_DISTINCT */
      /* The common case: Advance to the next row */
      sqlite3VdbeResolveLabel(v, pLevel->addrCont);
      sqlite3VdbeAddOp3(v, pLevel->op, pLevel->p1, pLevel->p2, pLevel->p3);
      sqlite3VdbeChangeP5(v, pLevel->p5);
      VdbeCoverage(v);
      VdbeCoverageIf(v, pLevel->op==OP_Next);
      VdbeCoverageIf(v, pLevel->op==OP_Prev);
      VdbeCoverageIf(v, pLevel->op==OP_VNext);
      if( pLevel->regBignull ){
        sqlite3VdbeResolveLabel(v, pLevel->addrBignull);
        sqlite3VdbeAddOp2(v, OP_DecrJumpZero, pLevel->regBignull, pLevel->p2-1);
        VdbeCoverage(v);
      }
#ifndef SQLITE_DISABLE_SKIPAHEAD_DISTINCT
      if( addrSeek ) sqlite3VdbeJumpHere(v, addrSeek);
#endif
    }else{
      sqlite3VdbeResolveLabel(v, pLevel->addrCont);
    }
    if( pLoop->wsFlags & WHERE_IN_ABLE && pLevel->u.in.nIn>0 ){
      struct InLoop *pIn;
      int j;
      sqlite3VdbeResolveLabel(v, pLevel->addrNxt);
      for(j=pLevel->u.in.nIn, pIn=&pLevel->u.in.aInLoop[j-1]; j>0; j--, pIn--){
        sqlite3VdbeJumpHere(v, pIn->addrInTop+1);
        if( pIn->eEndLoopOp!=OP_Noop ){
          if( pIn->nPrefix ){
            assert( pLoop->wsFlags & WHERE_IN_EARLYOUT );
            sqlite3VdbeAddOp4Int(v, OP_IfNoHope, pLevel->iIdxCur,
                              sqlite3VdbeCurrentAddr(v)+2,
                              pIn->iBase, pIn->nPrefix);
            VdbeCoverage(v);
          }
          sqlite3VdbeAddOp2(v, pIn->eEndLoopOp, pIn->iCur, pIn->addrInTop);
          VdbeCoverage(v);
          VdbeCoverageIf(v, pIn->eEndLoopOp==OP_Prev);
          VdbeCoverageIf(v, pIn->eEndLoopOp==OP_Next);
        }
        sqlite3VdbeJumpHere(v, pIn->addrInTop-1);
      }
    }
    sqlite3VdbeResolveLabel(v, pLevel->addrBrk);
    if( pLevel->addrSkip ){
      sqlite3VdbeGoto(v, pLevel->addrSkip);
      VdbeComment((v, "next skip-scan on %s", pLoop->u.btree.pIndex->zName));
      sqlite3VdbeJumpHere(v, pLevel->addrSkip);
      sqlite3VdbeJumpHere(v, pLevel->addrSkip-2);
    }
#ifndef SQLITE_LIKE_DOESNT_MATCH_BLOBS
    if( pLevel->addrLikeRep ){
      sqlite3VdbeAddOp2(v, OP_DecrJumpZero, (int)(pLevel->iLikeRepCntr>>1),
                        pLevel->addrLikeRep);
      VdbeCoverage(v);
    }
#endif
    if( pLevel->iLeftJoin ){
      int ws = pLoop->wsFlags;
      addr = sqlite3VdbeAddOp1(v, OP_IfPos, pLevel->iLeftJoin); VdbeCoverage(v);
      assert( (ws & WHERE_IDX_ONLY)==0 || (ws & WHERE_INDEXED)!=0 );
      if( (ws & WHERE_IDX_ONLY)==0 ){
        assert( pLevel->iTabCur==pTabList->a[pLevel->iFrom].iCursor );
        sqlite3VdbeAddOp1(v, OP_NullRow, pLevel->iTabCur);
      }
      if( (ws & WHERE_INDEXED) 
       || ((ws & WHERE_MULTI_OR) && pLevel->u.pCovidx) 
      ){
        sqlite3VdbeAddOp1(v, OP_NullRow, pLevel->iIdxCur);
      }
      if( pLevel->op==OP_Return ){
        sqlite3VdbeAddOp2(v, OP_Gosub, pLevel->p1, pLevel->addrFirst);
      }else{
        sqlite3VdbeGoto(v, pLevel->addrFirst);
      }
      sqlite3VdbeJumpHere(v, addr);
    }
    VdbeModuleComment((v, "End WHERE-loop%d: %s", i,
                     pWInfo->pTabList->a[pLevel->iFrom].pTab->zName));
  }

  /* The "break" point is here, just past the end of the outer loop.
  ** Set it.
  */
  sqlite3VdbeResolveLabel(v, pWInfo->iBreak);

  assert( pWInfo->nLevel<=pTabList->nSrc );
  for(i=0, pLevel=pWInfo->a; i<pWInfo->nLevel; i++, pLevel++){
    int k, last;
    VdbeOp *pOp;
    Index *pIdx = 0;
    struct SrcList_item *pTabItem = &pTabList->a[pLevel->iFrom];
    Table *pTab = pTabItem->pTab;
    assert( pTab!=0 );
    pLoop = pLevel->pWLoop;

    /* For a co-routine, change all OP_Column references to the table of
    ** the co-routine into OP_Copy of result contained in a register.
    ** OP_Rowid becomes OP_Null.
    */
    if( pTabItem->fg.viaCoroutine ){
      testcase( pParse->db->mallocFailed );
      translateColumnToCopy(pParse, pLevel->addrBody, pLevel->iTabCur,
                            pTabItem->regResult, 0);
      continue;
    }

#ifdef SQLITE_ENABLE_EARLY_CURSOR_CLOSE
    /* Close all of the cursors that were opened by sqlite3WhereBegin.
    ** Except, do not close cursors that will be reused by the OR optimization
    ** (WHERE_OR_SUBCLAUSE).  And do not close the OP_OpenWrite cursors
    ** created for the ONEPASS optimization.
    */
    if( (pTab->tabFlags & TF_Ephemeral)==0
     && pTab->pSelect==0
     && (pWInfo->wctrlFlags & WHERE_OR_SUBCLAUSE)==0
    ){
      int ws = pLoop->wsFlags;
      if( pWInfo->eOnePass==ONEPASS_OFF && (ws & WHERE_IDX_ONLY)==0 ){
        sqlite3VdbeAddOp1(v, OP_Close, pTabItem->iCursor);
      }
      if( (ws & WHERE_INDEXED)!=0
       && (ws & (WHERE_IPK|WHERE_AUTO_INDEX))==0 
       && pLevel->iIdxCur!=pWInfo->aiCurOnePass[1]
      ){
        sqlite3VdbeAddOp1(v, OP_Close, pLevel->iIdxCur);
      }
    }
#endif

    /* If this scan uses an index, make VDBE code substitutions to read data
    ** from the index instead of from the table where possible.  In some cases
    ** this optimization prevents the table from ever being read, which can
    ** yield a significant performance boost.
    ** 
    ** Calls to the code generator in between sqlite3WhereBegin and
    ** sqlite3WhereEnd will have created code that references the table
    ** directly.  This loop scans all that code looking for opcodes
    ** that reference the table and converts them into opcodes that
    ** reference the index.
    */
    if( pLoop->wsFlags & (WHERE_INDEXED|WHERE_IDX_ONLY) ){
      pIdx = pLoop->u.btree.pIndex;
    }else if( pLoop->wsFlags & WHERE_MULTI_OR ){
      pIdx = pLevel->u.pCovidx;
    }
    if( pIdx
     && (pWInfo->eOnePass==ONEPASS_OFF || !HasRowid(pIdx->pTable))
     && !db->mallocFailed
    ){
      last = sqlite3VdbeCurrentAddr(v);
      k = pLevel->addrBody;
#ifdef SQLITE_DEBUG
      if( db->flags & SQLITE_VdbeAddopTrace ){
        printf("TRANSLATE opcodes in range %d..%d\n", k, last-1);
      }
#endif
      pOp = sqlite3VdbeGetOp(v, k);
      for(; k<last; k++, pOp++){
        if( pOp->p1!=pLevel->iTabCur ) continue;
        if( pOp->opcode==OP_Column
#ifdef SQLITE_ENABLE_OFFSET_SQL_FUNC
         || pOp->opcode==OP_Offset
#endif
        ){
          int x = pOp->p2;
          assert( pIdx->pTable==pTab );
          if( !HasRowid(pTab) ){
            Index *pPk = sqlite3PrimaryKeyIndex(pTab);
            x = pPk->aiColumn[x];
            assert( x>=0 );
          }
          x = sqlite3ColumnOfIndex(pIdx, x);
          if( x>=0 ){
            pOp->p2 = x;
            pOp->p1 = pLevel->iIdxCur;
            OpcodeRewriteTrace(db, k, pOp);
          }
          assert( (pLoop->wsFlags & WHERE_IDX_ONLY)==0 || x>=0 
              || pWInfo->eOnePass );
        }else if( pOp->opcode==OP_Rowid ){
          pOp->p1 = pLevel->iIdxCur;
          pOp->opcode = OP_IdxRowid;
          OpcodeRewriteTrace(db, k, pOp);
        }else if( pOp->opcode==OP_IfNullRow ){
          pOp->p1 = pLevel->iIdxCur;
          OpcodeRewriteTrace(db, k, pOp);
        }
      }
#ifdef SQLITE_DEBUG
      if( db->flags & SQLITE_VdbeAddopTrace ) printf("TRANSLATE complete\n");
#endif
    }
  }

  /* Final cleanup
  */
  pParse->nQueryLoop = pWInfo->savedNQueryLoop;
  whereInfoFree(db, pWInfo);
  return;
}

/************** End of where.c ***********************************************/
/************** Begin file window.c ******************************************/
/*
** 2018 May 08
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
*/
/* #include "sqliteInt.h" */

#ifndef SQLITE_OMIT_WINDOWFUNC

/*
** SELECT REWRITING
**
**   Any SELECT statement that contains one or more window functions in
**   either the select list or ORDER BY clause (the only two places window
**   functions may be used) is transformed by function sqlite3WindowRewrite()
**   in order to support window function processing. For example, with the
**   schema:
**
**     CREATE TABLE t1(a, b, c, d, e, f, g);
**
**   the statement:
**
**     SELECT a+1, max(b) OVER (PARTITION BY c ORDER BY d) FROM t1 ORDER BY e;
**
**   is transformed to:
**
**     SELECT a+1, max(b) OVER (PARTITION BY c ORDER BY d) FROM (
**         SELECT a, e, c, d, b FROM t1 ORDER BY c, d
**     ) ORDER BY e;
**
**   The flattening optimization is disabled when processing this transformed
**   SELECT statement. This allows the implementation of the window function
**   (in this case max()) to process rows sorted in order of (c, d), which
**   makes things easier for obvious reasons. More generally:
**
**     * FROM, WHERE, GROUP BY and HAVING clauses are all moved to 
**       the sub-query.
**
**     * ORDER BY, LIMIT and OFFSET remain part of the parent query.
**
**     * Terminals from each of the expression trees that make up the 
**       select-list and ORDER BY expressions in the parent query are
**       selected by the sub-query. For the purposes of the transformation,
**       terminals are column references and aggregate functions.
**
**   If there is more than one window function in the SELECT that uses
**   the same window declaration (the OVER bit), then a single scan may
**   be used to process more than one window function. For example:
**
**     SELECT max(b) OVER (PARTITION BY c ORDER BY d), 
**            min(e) OVER (PARTITION BY c ORDER BY d) 
**     FROM t1;
**
**   is transformed in the same way as the example above. However:
**
**     SELECT max(b) OVER (PARTITION BY c ORDER BY d), 
**            min(e) OVER (PARTITION BY a ORDER BY b) 
**     FROM t1;
**
**   Must be transformed to:
**
**     SELECT max(b) OVER (PARTITION BY c ORDER BY d) FROM (
**         SELECT e, min(e) OVER (PARTITION BY a ORDER BY b), c, d, b FROM
**           SELECT a, e, c, d, b FROM t1 ORDER BY a, b
**         ) ORDER BY c, d
**     ) ORDER BY e;
**
**   so that both min() and max() may process rows in the order defined by
**   their respective window declarations.
**
** INTERFACE WITH SELECT.C
**
**   When processing the rewritten SELECT statement, code in select.c calls
**   sqlite3WhereBegin() to begin iterating through the results of the
**   sub-query, which is always implemented as a co-routine. It then calls
**   sqlite3WindowCodeStep() to process rows and finish the scan by calling
**   sqlite3WhereEnd().
**
**   sqlite3WindowCodeStep() generates VM code so that, for each row returned
**   by the sub-query a sub-routine (OP_Gosub) coded by select.c is invoked.
**   When the sub-routine is invoked:
**
**     * The results of all window-functions for the row are stored
**       in the associated Window.regResult registers.
**
**     * The required terminal values are stored in the current row of
**       temp table Window.iEphCsr.
**
**   In some cases, depending on the window frame and the specific window
**   functions invoked, sqlite3WindowCodeStep() caches each entire partition
**   in a temp table before returning any rows. In other cases it does not.
**   This detail is encapsulated within this file, the code generated by
**   select.c is the same in either case.
**
** BUILT-IN WINDOW FUNCTIONS
**
**   This implementation features the following built-in window functions:
**
**     row_number()
**     rank()
**     dense_rank()
**     percent_rank()
**     cume_dist()
**     ntile(N)
**     lead(expr [, offset [, default]])
**     lag(expr [, offset [, default]])
**     first_value(expr)
**     last_value(expr)
**     nth_value(expr, N)
**   
**   These are the same built-in window functions supported by Postgres. 
**   Although the behaviour of aggregate window functions (functions that
**   can be used as either aggregates or window funtions) allows them to
**   be implemented using an API, built-in window functions are much more
**   esoteric. Additionally, some window functions (e.g. nth_value()) 
**   may only be implemented by caching the entire partition in memory.
**   As such, some built-in window functions use the same API as aggregate
**   window functions and some are implemented directly using VDBE 
**   instructions. Additionally, for those functions that use the API, the
**   window frame is sometimes modified before the SELECT statement is
**   rewritten. For example, regardless of the specified window frame, the
**   row_number() function always uses:
**
**     ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
**
**   See sqlite3WindowUpdate() for details.
**
**   As well as some of the built-in window functions, aggregate window
**   functions min() and max() are implemented using VDBE instructions if
**   the start of the window frame is declared as anything other than 
**   UNBOUNDED PRECEDING.
*/

/*
** Implementation of built-in window function row_number(). Assumes that the
** window frame has been coerced to:
**
**   ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
*/
static void row_numberStepFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  i64 *p = (i64*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p ) (*p)++;
  UNUSED_PARAMETER(nArg);
  UNUSED_PARAMETER(apArg);
}
static void row_numberValueFunc(sqlite3_context *pCtx){
  i64 *p = (i64*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  sqlite3_result_int64(pCtx, (p ? *p : 0));
}

/*
** Context object type used by rank(), dense_rank(), percent_rank() and
** cume_dist().
*/
struct CallCount {
  i64 nValue;
  i64 nStep;
  i64 nTotal;
};

/*
** Implementation of built-in window function dense_rank(). Assumes that
** the window frame has been set to:
**
**   RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW 
*/
static void dense_rankStepFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct CallCount *p;
  p = (struct CallCount*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p ) p->nStep = 1;
  UNUSED_PARAMETER(nArg);
  UNUSED_PARAMETER(apArg);
}
static void dense_rankValueFunc(sqlite3_context *pCtx){
  struct CallCount *p;
  p = (struct CallCount*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p ){
    if( p->nStep ){
      p->nValue++;
      p->nStep = 0;
    }
    sqlite3_result_int64(pCtx, p->nValue);
  }
}

/*
** Implementation of built-in window function nth_value(). This
** implementation is used in "slow mode" only - when the EXCLUDE clause
** is not set to the default value "NO OTHERS".
*/
struct NthValueCtx {
  i64 nStep;
  sqlite3_value *pValue;
};
static void nth_valueStepFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct NthValueCtx *p;
  p = (struct NthValueCtx*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p ){
    i64 iVal;
    switch( sqlite3_value_numeric_type(apArg[1]) ){
      case SQLITE_INTEGER:
        iVal = sqlite3_value_int64(apArg[1]);
        break;
      case SQLITE_FLOAT: {
        double fVal = sqlite3_value_double(apArg[1]);
        if( ((i64)fVal)!=fVal ) goto error_out;
        iVal = (i64)fVal;
        break;
      }
      default:
        goto error_out;
    }
    if( iVal<=0 ) goto error_out;

    p->nStep++;
    if( iVal==p->nStep ){
      p->pValue = sqlite3_value_dup(apArg[0]);
      if( !p->pValue ){
        sqlite3_result_error_nomem(pCtx);
      }
    }
  }
  UNUSED_PARAMETER(nArg);
  UNUSED_PARAMETER(apArg);
  return;

 error_out:
  sqlite3_result_error(
      pCtx, "second argument to nth_value must be a positive integer", -1
  );
}
static void nth_valueFinalizeFunc(sqlite3_context *pCtx){
  struct NthValueCtx *p;
  p = (struct NthValueCtx*)sqlite3_aggregate_context(pCtx, 0);
  if( p && p->pValue ){
    sqlite3_result_value(pCtx, p->pValue);
    sqlite3_value_free(p->pValue);
    p->pValue = 0;
  }
}
#define nth_valueInvFunc noopStepFunc
#define nth_valueValueFunc noopValueFunc

static void first_valueStepFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct NthValueCtx *p;
  p = (struct NthValueCtx*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p && p->pValue==0 ){
    p->pValue = sqlite3_value_dup(apArg[0]);
    if( !p->pValue ){
      sqlite3_result_error_nomem(pCtx);
    }
  }
  UNUSED_PARAMETER(nArg);
  UNUSED_PARAMETER(apArg);
}
static void first_valueFinalizeFunc(sqlite3_context *pCtx){
  struct NthValueCtx *p;
  p = (struct NthValueCtx*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p && p->pValue ){
    sqlite3_result_value(pCtx, p->pValue);
    sqlite3_value_free(p->pValue);
    p->pValue = 0;
  }
}
#define first_valueInvFunc noopStepFunc
#define first_valueValueFunc noopValueFunc

/*
** Implementation of built-in window function rank(). Assumes that
** the window frame has been set to:
**
**   RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW 
*/
static void rankStepFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct CallCount *p;
  p = (struct CallCount*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p ){
    p->nStep++;
    if( p->nValue==0 ){
      p->nValue = p->nStep;
    }
  }
  UNUSED_PARAMETER(nArg);
  UNUSED_PARAMETER(apArg);
}
static void rankValueFunc(sqlite3_context *pCtx){
  struct CallCount *p;
  p = (struct CallCount*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p ){
    sqlite3_result_int64(pCtx, p->nValue);
    p->nValue = 0;
  }
}

/*
** Implementation of built-in window function percent_rank(). Assumes that
** the window frame has been set to:
**
**   GROUPS BETWEEN CURRENT ROW AND UNBOUNDED FOLLOWING
*/
static void percent_rankStepFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct CallCount *p;
  UNUSED_PARAMETER(nArg); assert( nArg==0 );
  UNUSED_PARAMETER(apArg);
  p = (struct CallCount*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p ){
    p->nTotal++;
  }
}
static void percent_rankInvFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct CallCount *p;
  UNUSED_PARAMETER(nArg); assert( nArg==0 );
  UNUSED_PARAMETER(apArg);
  p = (struct CallCount*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  p->nStep++;
}
static void percent_rankValueFunc(sqlite3_context *pCtx){
  struct CallCount *p;
  p = (struct CallCount*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p ){
    p->nValue = p->nStep;
    if( p->nTotal>1 ){
      double r = (double)p->nValue / (double)(p->nTotal-1);
      sqlite3_result_double(pCtx, r);
    }else{
      sqlite3_result_double(pCtx, 0.0);
    }
  }
}
#define percent_rankFinalizeFunc percent_rankValueFunc

/*
** Implementation of built-in window function cume_dist(). Assumes that
** the window frame has been set to:
**
**   GROUPS BETWEEN 1 FOLLOWING AND UNBOUNDED FOLLOWING
*/
static void cume_distStepFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct CallCount *p;
  UNUSED_PARAMETER(nArg); assert( nArg==0 );
  UNUSED_PARAMETER(apArg);
  p = (struct CallCount*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p ){
    p->nTotal++;
  }
}
static void cume_distInvFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct CallCount *p;
  UNUSED_PARAMETER(nArg); assert( nArg==0 );
  UNUSED_PARAMETER(apArg);
  p = (struct CallCount*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  p->nStep++;
}
static void cume_distValueFunc(sqlite3_context *pCtx){
  struct CallCount *p;
  p = (struct CallCount*)sqlite3_aggregate_context(pCtx, 0);
  if( p ){
    double r = (double)(p->nStep) / (double)(p->nTotal);
    sqlite3_result_double(pCtx, r);
  }
}
#define cume_distFinalizeFunc cume_distValueFunc

/*
** Context object for ntile() window function.
*/
struct NtileCtx {
  i64 nTotal;                     /* Total rows in partition */
  i64 nParam;                     /* Parameter passed to ntile(N) */
  i64 iRow;                       /* Current row */
};

/*
** Implementation of ntile(). This assumes that the window frame has
** been coerced to:
**
**   ROWS CURRENT ROW AND UNBOUNDED FOLLOWING
*/
static void ntileStepFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct NtileCtx *p;
  assert( nArg==1 ); UNUSED_PARAMETER(nArg);
  p = (struct NtileCtx*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p ){
    if( p->nTotal==0 ){
      p->nParam = sqlite3_value_int64(apArg[0]);
      if( p->nParam<=0 ){
        sqlite3_result_error(
            pCtx, "argument of ntile must be a positive integer", -1
        );
      }
    }
    p->nTotal++;
  }
}
static void ntileInvFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct NtileCtx *p;
  assert( nArg==1 ); UNUSED_PARAMETER(nArg);
  UNUSED_PARAMETER(apArg);
  p = (struct NtileCtx*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  p->iRow++;
}
static void ntileValueFunc(sqlite3_context *pCtx){
  struct NtileCtx *p;
  p = (struct NtileCtx*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p && p->nParam>0 ){
    int nSize = (p->nTotal / p->nParam);
    if( nSize==0 ){
      sqlite3_result_int64(pCtx, p->iRow+1);
    }else{
      i64 nLarge = p->nTotal - p->nParam*nSize;
      i64 iSmall = nLarge*(nSize+1);
      i64 iRow = p->iRow;

      assert( (nLarge*(nSize+1) + (p->nParam-nLarge)*nSize)==p->nTotal );

      if( iRow<iSmall ){
        sqlite3_result_int64(pCtx, 1 + iRow/(nSize+1));
      }else{
        sqlite3_result_int64(pCtx, 1 + nLarge + (iRow-iSmall)/nSize);
      }
    }
  }
}
#define ntileFinalizeFunc ntileValueFunc

/*
** Context object for last_value() window function.
*/
struct LastValueCtx {
  sqlite3_value *pVal;
  int nVal;
};

/*
** Implementation of last_value().
*/
static void last_valueStepFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct LastValueCtx *p;
  UNUSED_PARAMETER(nArg);
  p = (struct LastValueCtx*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p ){
    sqlite3_value_free(p->pVal);
    p->pVal = sqlite3_value_dup(apArg[0]);
    if( p->pVal==0 ){
      sqlite3_result_error_nomem(pCtx);
    }else{
      p->nVal++;
    }
  }
}
static void last_valueInvFunc(
  sqlite3_context *pCtx, 
  int nArg,
  sqlite3_value **apArg
){
  struct LastValueCtx *p;
  UNUSED_PARAMETER(nArg);
  UNUSED_PARAMETER(apArg);
  p = (struct LastValueCtx*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( ALWAYS(p) ){
    p->nVal--;
    if( p->nVal==0 ){
      sqlite3_value_free(p->pVal);
      p->pVal = 0;
    }
  }
}
static void last_valueValueFunc(sqlite3_context *pCtx){
  struct LastValueCtx *p;
  p = (struct LastValueCtx*)sqlite3_aggregate_context(pCtx, 0);
  if( p && p->pVal ){
    sqlite3_result_value(pCtx, p->pVal);
  }
}
static void last_valueFinalizeFunc(sqlite3_context *pCtx){
  struct LastValueCtx *p;
  p = (struct LastValueCtx*)sqlite3_aggregate_context(pCtx, sizeof(*p));
  if( p && p->pVal ){
    sqlite3_result_value(pCtx, p->pVal);
    sqlite3_value_free(p->pVal);
    p->pVal = 0;
  }
}

/*
** Static names for the built-in window function names.  These static
** names are used, rather than string literals, so that FuncDef objects
** can be associated with a particular window function by direct
** comparison of the zName pointer.  Example:
**
**       if( pFuncDef->zName==row_valueName ){ ... }
*/
static const char row_numberName[] =   "row_number";
static const char dense_rankName[] =   "dense_rank";
static const char rankName[] =         "rank";
static const char percent_rankName[] = "percent_rank";
static const char cume_distName[] =    "cume_dist";
static const char ntileName[] =        "ntile";
static const char last_valueName[] =   "last_value";
static const char nth_valueName[] =    "nth_value";
static const char first_valueName[] =  "first_value";
static const char leadName[] =         "lead";
static const char lagName[] =          "lag";

/*
** No-op implementations of xStep() and xFinalize().  Used as place-holders
** for built-in window functions that never call those interfaces.
**
** The noopValueFunc() is called but is expected to do nothing.  The
** noopStepFunc() is never called, and so it is marked with NO_TEST to
** let the test coverage routine know not to expect this function to be
** invoked.
*/
static void noopStepFunc(    /*NO_TEST*/
  sqlite3_context *p,        /*NO_TEST*/
  int n,                     /*NO_TEST*/
  sqlite3_value **a          /*NO_TEST*/
){                           /*NO_TEST*/
  UNUSED_PARAMETER(p);       /*NO_TEST*/
  UNUSED_PARAMETER(n);       /*NO_TEST*/
  UNUSED_PARAMETER(a);       /*NO_TEST*/
  assert(0);                 /*NO_TEST*/
}                            /*NO_TEST*/
static void noopValueFunc(sqlite3_context *p){ UNUSED_PARAMETER(p); /*no-op*/ }

/* Window functions that use all window interfaces: xStep, xFinal,
** xValue, and xInverse */
#define WINDOWFUNCALL(name,nArg,extra) {                                   \
  nArg, (SQLITE_UTF8|SQLITE_FUNC_WINDOW|extra), 0, 0,                      \
  name ## StepFunc, name ## FinalizeFunc, name ## ValueFunc,               \
  name ## InvFunc, name ## Name, {0}                                       \
}

/* Window functions that are implemented using bytecode and thus have
** no-op routines for their methods */
#define WINDOWFUNCNOOP(name,nArg,extra) {                                  \
  nArg, (SQLITE_UTF8|SQLITE_FUNC_WINDOW|extra), 0, 0,                      \
  noopStepFunc, noopValueFunc, noopValueFunc,                              \
  noopStepFunc, name ## Name, {0}                                          \
}

/* Window functions that use all window interfaces: xStep, the
** same routine for xFinalize and xValue and which never call
** xInverse. */
#define WINDOWFUNCX(name,nArg,extra) {                                     \
  nArg, (SQLITE_UTF8|SQLITE_FUNC_WINDOW|extra), 0, 0,                      \
  name ## StepFunc, name ## ValueFunc, name ## ValueFunc,                  \
  noopStepFunc, name ## Name, {0}                                          \
}


/*
** Register those built-in window functions that are not also aggregates.
*/
SQLITE_PRIVATE void sqlite3WindowFunctions(void){
  static FuncDef aWindowFuncs[] = {
    WINDOWFUNCX(row_number, 0, 0),
    WINDOWFUNCX(dense_rank, 0, 0),
    WINDOWFUNCX(rank, 0, 0),
    WINDOWFUNCALL(percent_rank, 0, 0),
    WINDOWFUNCALL(cume_dist, 0, 0),
    WINDOWFUNCALL(ntile, 1, 0),
    WINDOWFUNCALL(last_value, 1, 0),
    WINDOWFUNCALL(nth_value, 2, 0),
    WINDOWFUNCALL(first_value, 1, 0),
    WINDOWFUNCNOOP(lead, 1, 0),
    WINDOWFUNCNOOP(lead, 2, 0),
    WINDOWFUNCNOOP(lead, 3, 0),
    WINDOWFUNCNOOP(lag, 1, 0),
    WINDOWFUNCNOOP(lag, 2, 0),
    WINDOWFUNCNOOP(lag, 3, 0),
  };
  sqlite3InsertBuiltinFuncs(aWindowFuncs, ArraySize(aWindowFuncs));
}

static Window *windowFind(Parse *pParse, Window *pList, const char *zName){
  Window *p;
  for(p=pList; p; p=p->pNextWin){
    if( sqlite3StrICmp(p->zName, zName)==0 ) break;
  }
  if( p==0 ){
    sqlite3ErrorMsg(pParse, "no such window: %s", zName);
  }
  return p;
}

/*
** This function is called immediately after resolving the function name
** for a window function within a SELECT statement. Argument pList is a
** linked list of WINDOW definitions for the current SELECT statement.
** Argument pFunc is the function definition just resolved and pWin
** is the Window object representing the associated OVER clause. This
** function updates the contents of pWin as follows:
**
**   * If the OVER clause refered to a named window (as in "max(x) OVER win"),
**     search list pList for a matching WINDOW definition, and update pWin
**     accordingly. If no such WINDOW clause can be found, leave an error
**     in pParse.
**
**   * If the function is a built-in window function that requires the
**     window to be coerced (see "BUILT-IN WINDOW FUNCTIONS" at the top
**     of this file), pWin is updated here.
*/
SQLITE_PRIVATE void sqlite3WindowUpdate(
  Parse *pParse, 
  Window *pList,                  /* List of named windows for this SELECT */
  Window *pWin,                   /* Window frame to update */
  FuncDef *pFunc                  /* Window function definition */
){
  if( pWin->zName && pWin->eFrmType==0 ){
    Window *p = windowFind(pParse, pList, pWin->zName);
    if( p==0 ) return;
    pWin->pPartition = sqlite3ExprListDup(pParse->db, p->pPartition, 0);
    pWin->pOrderBy = sqlite3ExprListDup(pParse->db, p->pOrderBy, 0);
    pWin->pStart = sqlite3ExprDup(pParse->db, p->pStart, 0);
    pWin->pEnd = sqlite3ExprDup(pParse->db, p->pEnd, 0);
    pWin->eStart = p->eStart;
    pWin->eEnd = p->eEnd;
    pWin->eFrmType = p->eFrmType;
    pWin->eExclude = p->eExclude;
  }else{
    sqlite3WindowChain(pParse, pWin, pList);
  }
  if( (pWin->eFrmType==TK_RANGE)
   && (pWin->pStart || pWin->pEnd) 
   && (pWin->pOrderBy==0 || pWin->pOrderBy->nExpr!=1)
  ){
    sqlite3ErrorMsg(pParse, 
      "RANGE with offset PRECEDING/FOLLOWING requires one ORDER BY expression"
    );
  }else
  if( pFunc->funcFlags & SQLITE_FUNC_WINDOW ){
    sqlite3 *db = pParse->db;
    if( pWin->pFilter ){
      sqlite3ErrorMsg(pParse, 
          "FILTER clause may only be used with aggregate window functions"
      );
    }else{
      struct WindowUpdate {
        const char *zFunc;
        int eFrmType;
        int eStart;
        int eEnd;
      } aUp[] = {
        { row_numberName,   TK_ROWS,   TK_UNBOUNDED, TK_CURRENT }, 
        { dense_rankName,   TK_RANGE,  TK_UNBOUNDED, TK_CURRENT }, 
        { rankName,         TK_RANGE,  TK_UNBOUNDED, TK_CURRENT }, 
        { percent_rankName, TK_GROUPS, TK_CURRENT,   TK_UNBOUNDED }, 
        { cume_distName,    TK_GROUPS, TK_FOLLOWING, TK_UNBOUNDED }, 
        { ntileName,        TK_ROWS,   TK_CURRENT,   TK_UNBOUNDED }, 
        { leadName,         TK_ROWS,   TK_UNBOUNDED, TK_UNBOUNDED }, 
        { lagName,          TK_ROWS,   TK_UNBOUNDED, TK_CURRENT }, 
      };
      int i;
      for(i=0; i<ArraySize(aUp); i++){
        if( pFunc->zName==aUp[i].zFunc ){
          sqlite3ExprDelete(db, pWin->pStart);
          sqlite3ExprDelete(db, pWin->pEnd);
          pWin->pEnd = pWin->pStart = 0;
          pWin->eFrmType = aUp[i].eFrmType;
          pWin->eStart = aUp[i].eStart;
          pWin->eEnd = aUp[i].eEnd;
          pWin->eExclude = 0;
          if( pWin->eStart==TK_FOLLOWING ){
            pWin->pStart = sqlite3Expr(db, TK_INTEGER, "1");
          }
          break;
        }
      }
    }
  }
  pWin->pFunc = pFunc;
}

/*
** Context object passed through sqlite3WalkExprList() to
** selectWindowRewriteExprCb() by selectWindowRewriteEList().
*/
typedef struct WindowRewrite WindowRewrite;
struct WindowRewrite {
  Window *pWin;
  SrcList *pSrc;
  ExprList *pSub;
  Table *pTab;
  Select *pSubSelect;             /* Current sub-select, if any */
};

/*
** Callback function used by selectWindowRewriteEList(). If necessary,
** this function appends to the output expression-list and updates 
** expression (*ppExpr) in place.
*/
static int selectWindowRewriteExprCb(Walker *pWalker, Expr *pExpr){
  struct WindowRewrite *p = pWalker->u.pRewrite;
  Parse *pParse = pWalker->pParse;
  assert( p!=0 );
  assert( p->pWin!=0 );

  /* If this function is being called from within a scalar sub-select
  ** that used by the SELECT statement being processed, only process
  ** TK_COLUMN expressions that refer to it (the outer SELECT). Do
  ** not process aggregates or window functions at all, as they belong
  ** to the scalar sub-select.  */
  if( p->pSubSelect ){
    if( pExpr->op!=TK_COLUMN ){
      return WRC_Continue;
    }else{
      int nSrc = p->pSrc->nSrc;
      int i;
      for(i=0; i<nSrc; i++){
        if( pExpr->iTable==p->pSrc->a[i].iCursor ) break;
      }
      if( i==nSrc ) return WRC_Continue;
    }
  }

  switch( pExpr->op ){

    case TK_FUNCTION:
      if( !ExprHasProperty(pExpr, EP_WinFunc) ){
        break;
      }else{
        Window *pWin;
        for(pWin=p->pWin; pWin; pWin=pWin->pNextWin){
          if( pExpr->y.pWin==pWin ){
            assert( pWin->pOwner==pExpr );
            return WRC_Prune;
          }
        }
      }
      /* Fall through.  */

    case TK_AGG_FUNCTION:
    case TK_COLUMN: {
      Expr *pDup = sqlite3ExprDup(pParse->db, pExpr, 0);
      p->pSub = sqlite3ExprListAppend(pParse, p->pSub, pDup);
      if( p->pSub ){
        assert( ExprHasProperty(pExpr, EP_Static)==0 );
        ExprSetProperty(pExpr, EP_Static);
        sqlite3ExprDelete(pParse->db, pExpr);
        ExprClearProperty(pExpr, EP_Static);
        memset(pExpr, 0, sizeof(Expr));

        pExpr->op = TK_COLUMN;
        pExpr->iColumn = p->pSub->nExpr-1;
        pExpr->iTable = p->pWin->iEphCsr;
        pExpr->y.pTab = p->pTab;
      }

      break;
    }

    default: /* no-op */
      break;
  }

  return WRC_Continue;
}
static int selectWindowRewriteSelectCb(Walker *pWalker, Select *pSelect){
  struct WindowRewrite *p = pWalker->u.pRewrite;
  Select *pSave = p->pSubSelect;
  if( pSave==pSelect ){
    return WRC_Continue;
  }else{
    p->pSubSelect = pSelect;
    sqlite3WalkSelect(pWalker, pSelect);
    p->pSubSelect = pSave;
  }
  return WRC_Prune;
}


/*
** Iterate through each expression in expression-list pEList. For each:
**
**   * TK_COLUMN,
**   * aggregate function, or
**   * window function with a Window object that is not a member of the 
**     Window list passed as the second argument (pWin).
**
** Append the node to output expression-list (*ppSub). And replace it
** with a TK_COLUMN that reads the (N-1)th element of table 
** pWin->iEphCsr, where N is the number of elements in (*ppSub) after
** appending the new one.
*/
static void selectWindowRewriteEList(
  Parse *pParse, 
  Window *pWin,
  SrcList *pSrc,
  ExprList *pEList,               /* Rewrite expressions in this list */
  Table *pTab,
  ExprList **ppSub                /* IN/OUT: Sub-select expression-list */
){
  Walker sWalker;
  WindowRewrite sRewrite;

  assert( pWin!=0 );
  memset(&sWalker, 0, sizeof(Walker));
  memset(&sRewrite, 0, sizeof(WindowRewrite));

  sRewrite.pSub = *ppSub;
  sRewrite.pWin = pWin;
  sRewrite.pSrc = pSrc;
  sRewrite.pTab = pTab;

  sWalker.pParse = pParse;
  sWalker.xExprCallback = selectWindowRewriteExprCb;
  sWalker.xSelectCallback = selectWindowRewriteSelectCb;
  sWalker.u.pRewrite = &sRewrite;

  (void)sqlite3WalkExprList(&sWalker, pEList);

  *ppSub = sRewrite.pSub;
}

/*
** Append a copy of each expression in expression-list pAppend to
** expression list pList. Return a pointer to the result list.
*/
static ExprList *exprListAppendList(
  Parse *pParse,          /* Parsing context */
  ExprList *pList,        /* List to which to append. Might be NULL */
  ExprList *pAppend,      /* List of values to append. Might be NULL */
  int bIntToNull
){
  if( pAppend ){
    int i;
    int nInit = pList ? pList->nExpr : 0;
    for(i=0; i<pAppend->nExpr; i++){
      Expr *pDup = sqlite3ExprDup(pParse->db, pAppend->a[i].pExpr, 0);
      if( bIntToNull && pDup && pDup->op==TK_INTEGER ){
        pDup->op = TK_NULL;
        pDup->flags &= ~(EP_IntValue|EP_IsTrue|EP_IsFalse);
      }
      pList = sqlite3ExprListAppend(pParse, pList, pDup);
      if( pList ) pList->a[nInit+i].sortFlags = pAppend->a[i].sortFlags;
    }
  }
  return pList;
}

/*
** If the SELECT statement passed as the second argument does not invoke
** any SQL window functions, this function is a no-op. Otherwise, it 
** rewrites the SELECT statement so that window function xStep functions
** are invoked in the correct order as described under "SELECT REWRITING"
** at the top of this file.
*/
SQLITE_PRIVATE int sqlite3WindowRewrite(Parse *pParse, Select *p){
  int rc = SQLITE_OK;
  if( p->pWin && p->pPrior==0 ){
    Vdbe *v = sqlite3GetVdbe(pParse);
    sqlite3 *db = pParse->db;
    Select *pSub = 0;             /* The subquery */
    SrcList *pSrc = p->pSrc;
    Expr *pWhere = p->pWhere;
    ExprList *pGroupBy = p->pGroupBy;
    Expr *pHaving = p->pHaving;
    ExprList *pSort = 0;

    ExprList *pSublist = 0;       /* Expression list for sub-query */
    Window *pMWin = p->pWin;      /* Master window object */
    Window *pWin;                 /* Window object iterator */
    Table *pTab;

    pTab = sqlite3DbMallocZero(db, sizeof(Table));
    if( pTab==0 ){
      return SQLITE_NOMEM;
    }

    p->pSrc = 0;
    p->pWhere = 0;
    p->pGroupBy = 0;
    p->pHaving = 0;
    p->selFlags &= ~SF_Aggregate;

    /* Create the ORDER BY clause for the sub-select. This is the concatenation
    ** of the window PARTITION and ORDER BY clauses. Then, if this makes it
    ** redundant, remove the ORDER BY from the parent SELECT.  */
    pSort = sqlite3ExprListDup(db, pMWin->pPartition, 0);
    pSort = exprListAppendList(pParse, pSort, pMWin->pOrderBy, 1);
    if( pSort && p->pOrderBy && p->pOrderBy->nExpr<=pSort->nExpr ){
      int nSave = pSort->nExpr;
      pSort->nExpr = p->pOrderBy->nExpr;
      if( sqlite3ExprListCompare(pSort, p->pOrderBy, -1)==0 ){
        sqlite3ExprListDelete(db, p->pOrderBy);
        p->pOrderBy = 0;
      }
      pSort->nExpr = nSave;
    }

    /* Assign a cursor number for the ephemeral table used to buffer rows.
    ** The OpenEphemeral instruction is coded later, after it is known how
    ** many columns the table will have.  */
    pMWin->iEphCsr = pParse->nTab++;
    pParse->nTab += 3;

    selectWindowRewriteEList(pParse, pMWin, pSrc, p->pEList, pTab, &pSublist);
    selectWindowRewriteEList(pParse, pMWin, pSrc, p->pOrderBy, pTab, &pSublist);
    pMWin->nBufferCol = (pSublist ? pSublist->nExpr : 0);

    /* Append the PARTITION BY and ORDER BY expressions to the to the 
    ** sub-select expression list. They are required to figure out where 
    ** boundaries for partitions and sets of peer rows lie.  */
    pSublist = exprListAppendList(pParse, pSublist, pMWin->pPartition, 0);
    pSublist = exprListAppendList(pParse, pSublist, pMWin->pOrderBy, 0);

    /* Append the arguments passed to each window function to the
    ** sub-select expression list. Also allocate two registers for each
    ** window function - one for the accumulator, another for interim
    ** results.  */
    for(pWin=pMWin; pWin; pWin=pWin->pNextWin){
      ExprList *pArgs = pWin->pOwner->x.pList;
      if( pWin->pFunc->funcFlags & SQLITE_FUNC_SUBTYPE ){
        selectWindowRewriteEList(pParse, pMWin, pSrc, pArgs, pTab, &pSublist);
        pWin->iArgCol = (pSublist ? pSublist->nExpr : 0);
        pWin->bExprArgs = 1;
      }else{
        pWin->iArgCol = (pSublist ? pSublist->nExpr : 0);
        pSublist = exprListAppendList(pParse, pSublist, pArgs, 0);
      }
      if( pWin->pFilter ){
        Expr *pFilter = sqlite3ExprDup(db, pWin->pFilter, 0);
        pSublist = sqlite3ExprListAppend(pParse, pSublist, pFilter);
      }
      pWin->regAccum = ++pParse->nMem;
      pWin->regResult = ++pParse->nMem;
      sqlite3VdbeAddOp2(v, OP_Null, 0, pWin->regAccum);
    }

    /* If there is no ORDER BY or PARTITION BY clause, and the window
    ** function accepts zero arguments, and there are no other columns
    ** selected (e.g. "SELECT row_number() OVER () FROM t1"), it is possible
    ** that pSublist is still NULL here. Add a constant expression here to 
    ** keep everything legal in this case. 
    */
    if( pSublist==0 ){
      pSublist = sqlite3ExprListAppend(pParse, 0, 
        sqlite3Expr(db, TK_INTEGER, "0")
      );
    }

    pSub = sqlite3SelectNew(
        pParse, pSublist, pSrc, pWhere, pGroupBy, pHaving, pSort, 0, 0
    );
    p->pSrc = sqlite3SrcListAppend(pParse, 0, 0, 0);
    if( p->pSrc ){
      Table *pTab2;
      p->pSrc->a[0].pSelect = pSub;
      sqlite3SrcListAssignCursors(pParse, p->pSrc);
      pSub->selFlags |= SF_Expanded;
      pTab2 = sqlite3ResultSetOfSelect(pParse, pSub, SQLITE_AFF_NONE);
      if( pTab2==0 ){
        rc = SQLITE_NOMEM;
      }else{
        memcpy(pTab, pTab2, sizeof(Table));
        pTab->tabFlags |= TF_Ephemeral;
        p->pSrc->a[0].pTab = pTab;
        pTab = pTab2;
      }
      sqlite3VdbeAddOp2(v, OP_OpenEphemeral, pMWin->iEphCsr, pSublist->nExpr);
      sqlite3VdbeAddOp2(v, OP_OpenDup, pMWin->iEphCsr+1, pMWin->iEphCsr);
      sqlite3VdbeAddOp2(v, OP_OpenDup, pMWin->iEphCsr+2, pMWin->iEphCsr);
      sqlite3VdbeAddOp2(v, OP_OpenDup, pMWin->iEphCsr+3, pMWin->iEphCsr);
    }else{
      sqlite3SelectDelete(db, pSub);
    }
    if( db->mallocFailed ) rc = SQLITE_NOMEM;
    sqlite3DbFree(db, pTab);
  }

  return rc;
}

/*
** Unlink the Window object from the Select to which it is attached,
** if it is attached.
*/
SQLITE_PRIVATE void sqlite3WindowUnlinkFromSelect(Window *p){
  if( p->ppThis ){
    *p->ppThis = p->pNextWin;
    if( p->pNextWin ) p->pNextWin->ppThis = p->ppThis;
    p->ppThis = 0;
  }
}

/*
** Free the Window object passed as the second argument.
*/
SQLITE_PRIVATE void sqlite3WindowDelete(sqlite3 *db, Window *p){
  if( p ){
    sqlite3WindowUnlinkFromSelect(p);
    sqlite3ExprDelete(db, p->pFilter);
    sqlite3ExprListDelete(db, p->pPartition);
    sqlite3ExprListDelete(db, p->pOrderBy);
    sqlite3ExprDelete(db, p->pEnd);
    sqlite3ExprDelete(db, p->pStart);
    sqlite3DbFree(db, p->zName);
    sqlite3DbFree(db, p->zBase);
    sqlite3DbFree(db, p);
  }
}

/*
** Free the linked list of Window objects starting at the second argument.
*/
SQLITE_PRIVATE void sqlite3WindowListDelete(sqlite3 *db, Window *p){
  while( p ){
    Window *pNext = p->pNextWin;
    sqlite3WindowDelete(db, p);
    p = pNext;
  }
}

/*
** The argument expression is an PRECEDING or FOLLOWING offset.  The
** value should be a non-negative integer.  If the value is not a
** constant, change it to NULL.  The fact that it is then a non-negative
** integer will be caught later.  But it is important not to leave
** variable values in the expression tree.
*/
static Expr *sqlite3WindowOffsetExpr(Parse *pParse, Expr *pExpr){
  if( 0==sqlite3ExprIsConstant(pExpr) ){
    if( IN_RENAME_OBJECT ) sqlite3RenameExprUnmap(pParse, pExpr);
    sqlite3ExprDelete(pParse->db, pExpr);
    pExpr = sqlite3ExprAlloc(pParse->db, TK_NULL, 0, 0);
  }
  return pExpr;
}

/*
** Allocate and return a new Window object describing a Window Definition.
*/
SQLITE_PRIVATE Window *sqlite3WindowAlloc(
  Parse *pParse,    /* Parsing context */
  int eType,        /* Frame type. TK_RANGE, TK_ROWS, TK_GROUPS, or 0 */
  int eStart,       /* Start type: CURRENT, PRECEDING, FOLLOWING, UNBOUNDED */
  Expr *pStart,     /* Start window size if TK_PRECEDING or FOLLOWING */
  int eEnd,         /* End type: CURRENT, FOLLOWING, TK_UNBOUNDED, PRECEDING */
  Expr *pEnd,       /* End window size if TK_FOLLOWING or PRECEDING */
  u8 eExclude       /* EXCLUDE clause */
){
  Window *pWin = 0;
  int bImplicitFrame = 0;

  /* Parser assures the following: */
  assert( eType==0 || eType==TK_RANGE || eType==TK_ROWS || eType==TK_GROUPS );
  assert( eStart==TK_CURRENT || eStart==TK_PRECEDING
           || eStart==TK_UNBOUNDED || eStart==TK_FOLLOWING );
  assert( eEnd==TK_CURRENT || eEnd==TK_FOLLOWING
           || eEnd==TK_UNBOUNDED || eEnd==TK_PRECEDING );
  assert( (eStart==TK_PRECEDING || eStart==TK_FOLLOWING)==(pStart!=0) );
  assert( (eEnd==TK_FOLLOWING || eEnd==TK_PRECEDING)==(pEnd!=0) );

  if( eType==0 ){
    bImplicitFrame = 1;
    eType = TK_RANGE;
  }

  /* Additionally, the
  ** starting boundary type may not occur earlier in the following list than
  ** the ending boundary type:
  **
  **   UNBOUNDED PRECEDING
  **   <expr> PRECEDING
  **   CURRENT ROW
  **   <expr> FOLLOWING
  **   UNBOUNDED FOLLOWING
  **
  ** The parser ensures that "UNBOUNDED PRECEDING" cannot be used as an ending
  ** boundary, and than "UNBOUNDED FOLLOWING" cannot be used as a starting
  ** frame boundary.
  */
  if( (eStart==TK_CURRENT && eEnd==TK_PRECEDING)
   || (eStart==TK_FOLLOWING && (eEnd==TK_PRECEDING || eEnd==TK_CURRENT))
  ){
    sqlite3ErrorMsg(pParse, "unsupported frame specification");
    goto windowAllocErr;
  }

  pWin = (Window*)sqlite3DbMallocZero(pParse->db, sizeof(Window));
  if( pWin==0 ) goto windowAllocErr;
  pWin->eFrmType = eType;
  pWin->eStart = eStart;
  pWin->eEnd = eEnd;
  if( eExclude==0 && OptimizationDisabled(pParse->db, SQLITE_WindowFunc) ){
    eExclude = TK_NO;
  }
  pWin->eExclude = eExclude;
  pWin->bImplicitFrame = bImplicitFrame;
  pWin->pEnd = sqlite3WindowOffsetExpr(pParse, pEnd);
  pWin->pStart = sqlite3WindowOffsetExpr(pParse, pStart);
  return pWin;

windowAllocErr:
  sqlite3ExprDelete(pParse->db, pEnd);
  sqlite3ExprDelete(pParse->db, pStart);
  return 0;
}

/*
** Attach PARTITION and ORDER BY clauses pPartition and pOrderBy to window
** pWin. Also, if parameter pBase is not NULL, set pWin->zBase to the
** equivalent nul-terminated string.
*/
SQLITE_PRIVATE Window *sqlite3WindowAssemble(
  Parse *pParse, 
  Window *pWin, 
  ExprList *pPartition, 
  ExprList *pOrderBy, 
  Token *pBase
){
  if( pWin ){
    pWin->pPartition = pPartition;
    pWin->pOrderBy = pOrderBy;
    if( pBase ){
      pWin->zBase = sqlite3DbStrNDup(pParse->db, pBase->z, pBase->n);
    }
  }else{
    sqlite3ExprListDelete(pParse->db, pPartition);
    sqlite3ExprListDelete(pParse->db, pOrderBy);
  }
  return pWin;
}

/*
** Window *pWin has just been created from a WINDOW clause. Tokne pBase
** is the base window. Earlier windows from the same WINDOW clause are
** stored in the linked list starting at pWin->pNextWin. This function
** either updates *pWin according to the base specification, or else
** leaves an error in pParse.
*/
SQLITE_PRIVATE void sqlite3WindowChain(Parse *pParse, Window *pWin, Window *pList){
  if( pWin->zBase ){
    sqlite3 *db = pParse->db;
    Window *pExist = windowFind(pParse, pList, pWin->zBase);
    if( pExist ){
      const char *zErr = 0;
      /* Check for errors */
      if( pWin->pPartition ){
        zErr = "PARTITION clause";
      }else if( pExist->pOrderBy && pWin->pOrderBy ){
        zErr = "ORDER BY clause";
      }else if( pExist->bImplicitFrame==0 ){
        zErr = "frame specification";
      }
      if( zErr ){
        sqlite3ErrorMsg(pParse, 
            "cannot override %s of window: %s", zErr, pWin->zBase
        );
      }else{
        pWin->pPartition = sqlite3ExprListDup(db, pExist->pPartition, 0);
        if( pExist->pOrderBy ){
          assert( pWin->pOrderBy==0 );
          pWin->pOrderBy = sqlite3ExprListDup(db, pExist->pOrderBy, 0);
        }
        sqlite3DbFree(db, pWin->zBase);
        pWin->zBase = 0;
      }
    }
  }
}

/*
** Attach window object pWin to expression p.
*/
SQLITE_PRIVATE void sqlite3WindowAttach(Parse *pParse, Expr *p, Window *pWin){
  if( p ){
    assert( p->op==TK_FUNCTION );
    assert( pWin );
    p->y.pWin = pWin;
    ExprSetProperty(p, EP_WinFunc);
    pWin->pOwner = p;
    if( (p->flags & EP_Distinct) && pWin->eFrmType!=TK_FILTER ){
      sqlite3ErrorMsg(pParse,
          "DISTINCT is not supported for window functions"
      );
    }
  }else{
    sqlite3WindowDelete(pParse->db, pWin);
  }
}

/*
** Possibly link window pWin into the list at pSel->pWin (window functions
** to be processed as part of SELECT statement pSel). The window is linked
** in if either (a) there are no other windows already linked to this
** SELECT, or (b) the windows already linked use a compatible window frame.
*/
SQLITE_PRIVATE void sqlite3WindowLink(Select *pSel, Window *pWin){
  if( 0==pSel->pWin 
   || 0==sqlite3WindowCompare(0, pSel->pWin, pWin, 0)
  ){
    pWin->pNextWin = pSel->pWin;
    if( pSel->pWin ){
      pSel->pWin->ppThis = &pWin->pNextWin;
    }
    pSel->pWin = pWin;
    pWin->ppThis = &pSel->pWin;
  }
}

/*
** Return 0 if the two window objects are identical, or non-zero otherwise.
** Identical window objects can be processed in a single scan.
*/
SQLITE_PRIVATE int sqlite3WindowCompare(Parse *pParse, Window *p1, Window *p2, int bFilter){
  if( p1->eFrmType!=p2->eFrmType ) return 1;
  if( p1->eStart!=p2->eStart ) return 1;
  if( p1->eEnd!=p2->eEnd ) return 1;
  if( p1->eExclude!=p2->eExclude ) return 1;
  if( sqlite3ExprCompare(pParse, p1->pStart, p2->pStart, -1) ) return 1;
  if( sqlite3ExprCompare(pParse, p1->pEnd, p2->pEnd, -1) ) return 1;
  if( sqlite3ExprListCompare(p1->pPartition, p2->pPartition, -1) ) return 1;
  if( sqlite3ExprListCompare(p1->pOrderBy, p2->pOrderBy, -1) ) return 1;
  if( bFilter ){
    if( sqlite3ExprCompare(pParse, p1->pFilter, p2->pFilter, -1) ) return 1;
  }
  return 0;
}


/*
** This is called by code in select.c before it calls sqlite3WhereBegin()
** to begin iterating through the sub-query results. It is used to allocate
** and initialize registers and cursors used by sqlite3WindowCodeStep().
*/
SQLITE_PRIVATE void sqlite3WindowCodeInit(Parse *pParse, Window *pMWin){
  Window *pWin;
  Vdbe *v = sqlite3GetVdbe(pParse);

  /* Allocate registers to use for PARTITION BY values, if any. Initialize
  ** said registers to NULL.  */
  if( pMWin->pPartition ){
    int nExpr = pMWin->pPartition->nExpr;
    pMWin->regPart = pParse->nMem+1;
    pParse->nMem += nExpr;
    sqlite3VdbeAddOp3(v, OP_Null, 0, pMWin->regPart, pMWin->regPart+nExpr-1);
  }

  pMWin->regOne = ++pParse->nMem;
  sqlite3VdbeAddOp2(v, OP_Integer, 1, pMWin->regOne);

  if( pMWin->eExclude ){
    pMWin->regStartRowid = ++pParse->nMem;
    pMWin->regEndRowid = ++pParse->nMem;
    pMWin->csrApp = pParse->nTab++;
    sqlite3VdbeAddOp2(v, OP_Integer, 1, pMWin->regStartRowid);
    sqlite3VdbeAddOp2(v, OP_Integer, 0, pMWin->regEndRowid);
    sqlite3VdbeAddOp2(v, OP_OpenDup, pMWin->csrApp, pMWin->iEphCsr);
    return;
  }

  for(pWin=pMWin; pWin; pWin=pWin->pNextWin){
    FuncDef *p = pWin->pFunc;
    if( (p->funcFlags & SQLITE_FUNC_MINMAX) && pWin->eStart!=TK_UNBOUNDED ){
      /* The inline versions of min() and max() require a single ephemeral
      ** table and 3 registers. The registers are used as follows:
      **
      **   regApp+0: slot to copy min()/max() argument to for MakeRecord
      **   regApp+1: integer value used to ensure keys are unique
      **   regApp+2: output of MakeRecord
      */
      ExprList *pList = pWin->pOwner->x.pList;
      KeyInfo *pKeyInfo = sqlite3KeyInfoFromExprList(pParse, pList, 0, 0);
      pWin->csrApp = pParse->nTab++;
      pWin->regApp = pParse->nMem+1;
      pParse->nMem += 3;
      if( pKeyInfo && pWin->pFunc->zName[1]=='i' ){
        assert( pKeyInfo->aSortFlags[0]==0 );
        pKeyInfo->aSortFlags[0] = KEYINFO_ORDER_DESC;
      }
      sqlite3VdbeAddOp2(v, OP_OpenEphemeral, pWin->csrApp, 2);
      sqlite3VdbeAppendP4(v, pKeyInfo, P4_KEYINFO);
      sqlite3VdbeAddOp2(v, OP_Integer, 0, pWin->regApp+1);
    }
    else if( p->zName==nth_valueName || p->zName==first_valueName ){
      /* Allocate two registers at pWin->regApp. These will be used to
      ** store the start and end index of the current frame.  */
      pWin->regApp = pParse->nMem+1;
      pWin->csrApp = pParse->nTab++;
      pParse->nMem += 2;
      sqlite3VdbeAddOp2(v, OP_OpenDup, pWin->csrApp, pMWin->iEphCsr);
    }
    else if( p->zName==leadName || p->zName==lagName ){
      pWin->csrApp = pParse->nTab++;
      sqlite3VdbeAddOp2(v, OP_OpenDup, pWin->csrApp, pMWin->iEphCsr);
    }
  }
}

#define WINDOW_STARTING_INT  0
#define WINDOW_ENDING_INT    1
#define WINDOW_NTH_VALUE_INT 2
#define WINDOW_STARTING_NUM  3
#define WINDOW_ENDING_NUM    4

/*
** A "PRECEDING <expr>" (eCond==0) or "FOLLOWING <expr>" (eCond==1) or the
** value of the second argument to nth_value() (eCond==2) has just been
** evaluated and the result left in register reg. This function generates VM
** code to check that the value is a non-negative integer and throws an
** exception if it is not.
*/
static void windowCheckValue(Parse *pParse, int reg, int eCond){
  static const char *azErr[] = {
    "frame starting offset must be a non-negative integer",
    "frame ending offset must be a non-negative integer",
    "second argument to nth_value must be a positive integer",
    "frame starting offset must be a non-negative number",
    "frame ending offset must be a non-negative number",
  };
  static int aOp[] = { OP_Ge, OP_Ge, OP_Gt, OP_Ge, OP_Ge };
  Vdbe *v = sqlite3GetVdbe(pParse);
  int regZero = sqlite3GetTempReg(pParse);
  assert( eCond>=0 && eCond<ArraySize(azErr) );
  sqlite3VdbeAddOp2(v, OP_Integer, 0, regZero);
  if( eCond>=WINDOW_STARTING_NUM ){
    int regString = sqlite3GetTempReg(pParse);
    sqlite3VdbeAddOp4(v, OP_String8, 0, regString, 0, "", P4_STATIC);
    sqlite3VdbeAddOp3(v, OP_Ge, regString, sqlite3VdbeCurrentAddr(v)+2, reg);
    sqlite3VdbeChangeP5(v, SQLITE_AFF_NUMERIC|SQLITE_JUMPIFNULL);
    VdbeCoverage(v);
    assert( eCond==3 || eCond==4 );
    VdbeCoverageIf(v, eCond==3);
    VdbeCoverageIf(v, eCond==4);
  }else{
    sqlite3VdbeAddOp2(v, OP_MustBeInt, reg, sqlite3VdbeCurrentAddr(v)+2);
    VdbeCoverage(v);
    assert( eCond==0 || eCond==1 || eCond==2 );
    VdbeCoverageIf(v, eCond==0);
    VdbeCoverageIf(v, eCond==1);
    VdbeCoverageIf(v, eCond==2);
  }
  sqlite3VdbeAddOp3(v, aOp[eCond], regZero, sqlite3VdbeCurrentAddr(v)+2, reg);
  VdbeCoverageNeverNullIf(v, eCond==0); /* NULL case captured by */
  VdbeCoverageNeverNullIf(v, eCond==1); /*   the OP_MustBeInt */
  VdbeCoverageNeverNullIf(v, eCond==2);
  VdbeCoverageNeverNullIf(v, eCond==3); /* NULL case caught by */
  VdbeCoverageNeverNullIf(v, eCond==4); /*   the OP_Ge */
  sqlite3MayAbort(pParse);
  sqlite3VdbeAddOp2(v, OP_Halt, SQLITE_ERROR, OE_Abort);
  sqlite3VdbeAppendP4(v, (void*)azErr[eCond], P4_STATIC);
  sqlite3ReleaseTempReg(pParse, regZero);
}

/*
** Return the number of arguments passed to the window-function associated
** with the object passed as the only argument to this function.
*/
static int windowArgCount(Window *pWin){
  ExprList *pList = pWin->pOwner->x.pList;
  return (pList ? pList->nExpr : 0);
}

typedef struct WindowCodeArg WindowCodeArg;
typedef struct WindowCsrAndReg WindowCsrAndReg;

/*
** See comments above struct WindowCodeArg.
*/
struct WindowCsrAndReg {
  int csr;                        /* Cursor number */
  int reg;                        /* First in array of peer values */
};

/*
** A single instance of this structure is allocated on the stack by 
** sqlite3WindowCodeStep() and a pointer to it passed to the various helper
** routines. This is to reduce the number of arguments required by each
** helper function.
**
** regArg:
**   Each window function requires an accumulator register (just as an
**   ordinary aggregate function does). This variable is set to the first
**   in an array of accumulator registers - one for each window function
**   in the WindowCodeArg.pMWin list.
**
** eDelete:
**   The window functions implementation sometimes caches the input rows
**   that it processes in a temporary table. If it is not zero, this
**   variable indicates when rows may be removed from the temp table (in
**   order to reduce memory requirements - it would always be safe just
**   to leave them there). Possible values for eDelete are:
**
**      WINDOW_RETURN_ROW:
**        An input row can be discarded after it is returned to the caller.
**
**      WINDOW_AGGINVERSE:
**        An input row can be discarded after the window functions xInverse()
**        callbacks have been invoked in it.
**
**      WINDOW_AGGSTEP:
**        An input row can be discarded after the window functions xStep()
**        callbacks have been invoked in it.
**
** start,current,end
**   Consider a window-frame similar to the following:
**
**     (ORDER BY a, b GROUPS BETWEEN 2 PRECEDING AND 2 FOLLOWING)
**
**   The windows functions implmentation caches the input rows in a temp
**   table, sorted by "a, b" (it actually populates the cache lazily, and
**   aggressively removes rows once they are no longer required, but that's
**   a mere detail). It keeps three cursors open on the temp table. One
**   (current) that points to the next row to return to the query engine
**   once its window function values have been calculated. Another (end)
**   points to the next row to call the xStep() method of each window function
**   on (so that it is 2 groups ahead of current). And a third (start) that
**   points to the next row to call the xInverse() method of each window
**   function on.
**
**   Each cursor (start, current and end) consists of a VDBE cursor
**   (WindowCsrAndReg.csr) and an array of registers (starting at
**   WindowCodeArg.reg) that always contains a copy of the peer values 
**   read from the corresponding cursor.
**
**   Depending on the window-frame in question, all three cursors may not
**   be required. In this case both WindowCodeArg.csr and reg are set to
**   0.
*/
struct WindowCodeArg {
  Parse *pParse;             /* Parse context */
  Window *pMWin;             /* First in list of functions being processed */
  Vdbe *pVdbe;               /* VDBE object */
  int addrGosub;             /* OP_Gosub to this address to return one row */
  int regGosub;              /* Register used with OP_Gosub(addrGosub) */
  int regArg;                /* First in array of accumulator registers */
  int eDelete;               /* See above */

  WindowCsrAndReg start;
  WindowCsrAndReg current;
  WindowCsrAndReg end;
};

/*
** Generate VM code to read the window frames peer values from cursor csr into
** an array of registers starting at reg.
*/
static void windowReadPeerValues(
  WindowCodeArg *p,
  int csr,
  int reg
){
  Window *pMWin = p->pMWin;
  ExprList *pOrderBy = pMWin->pOrderBy;
  if( pOrderBy ){
    Vdbe *v = sqlite3GetVdbe(p->pParse);
    ExprList *pPart = pMWin->pPartition;
    int iColOff = pMWin->nBufferCol + (pPart ? pPart->nExpr : 0);
    int i;
    for(i=0; i<pOrderBy->nExpr; i++){
      sqlite3VdbeAddOp3(v, OP_Column, csr, iColOff+i, reg+i);
    }
  }
}

/*
** Generate VM code to invoke either xStep() (if bInverse is 0) or 
** xInverse (if bInverse is non-zero) for each window function in the 
** linked list starting at pMWin. Or, for built-in window functions
** that do not use the standard function API, generate the required
** inline VM code.
**
** If argument csr is greater than or equal to 0, then argument reg is
** the first register in an array of registers guaranteed to be large
** enough to hold the array of arguments for each function. In this case
** the arguments are extracted from the current row of csr into the
** array of registers before invoking OP_AggStep or OP_AggInverse
**
** Or, if csr is less than zero, then the array of registers at reg is
** already populated with all columns from the current row of the sub-query.
**
** If argument regPartSize is non-zero, then it is a register containing the
** number of rows in the current partition.
*/
static void windowAggStep(
  WindowCodeArg *p,
  Window *pMWin,                  /* Linked list of window functions */
  int csr,                        /* Read arguments from this cursor */
  int bInverse,                   /* True to invoke xInverse instead of xStep */
  int reg                         /* Array of registers */
){
  Parse *pParse = p->pParse;
  Vdbe *v = sqlite3GetVdbe(pParse);
  Window *pWin;
  for(pWin=pMWin; pWin; pWin=pWin->pNextWin){
    FuncDef *pFunc = pWin->pFunc;
    int regArg;
    int nArg = pWin->bExprArgs ? 0 : windowArgCount(pWin);
    int i;

    assert( bInverse==0 || pWin->eStart!=TK_UNBOUNDED );

    /* All OVER clauses in the same window function aggregate step must
    ** be the same. */
    assert( pWin==pMWin || sqlite3WindowCompare(pParse,pWin,pMWin,0)==0 );

    for(i=0; i<nArg; i++){
      if( i!=1 || pFunc->zName!=nth_valueName ){
        sqlite3VdbeAddOp3(v, OP_Column, csr, pWin->iArgCol+i, reg+i);
      }else{
        sqlite3VdbeAddOp3(v, OP_Column, pMWin->iEphCsr, pWin->iArgCol+i, reg+i);
      }
    }
    regArg = reg;

    if( pMWin->regStartRowid==0
     && (pFunc->funcFlags & SQLITE_FUNC_MINMAX) 
     && (pWin->eStart!=TK_UNBOUNDED)
    ){
      int addrIsNull = sqlite3VdbeAddOp1(v, OP_IsNull, regArg);
      VdbeCoverage(v);
      if( bInverse==0 ){
        sqlite3VdbeAddOp2(v, OP_AddImm, pWin->regApp+1, 1);
        sqlite3VdbeAddOp2(v, OP_SCopy, regArg, pWin->regApp);
        sqlite3VdbeAddOp3(v, OP_MakeRecord, pWin->regApp, 2, pWin->regApp+2);
        sqlite3VdbeAddOp2(v, OP_IdxInsert, pWin->csrApp, pWin->regApp+2);
      }else{
        sqlite3VdbeAddOp4Int(v, OP_SeekGE, pWin->csrApp, 0, regArg, 1);
        VdbeCoverageNeverTaken(v);
        sqlite3VdbeAddOp1(v, OP_Delete, pWin->csrApp);
        sqlite3VdbeJumpHere(v, sqlite3VdbeCurrentAddr(v)-2);
      }
      sqlite3VdbeJumpHere(v, addrIsNull);
    }else if( pWin->regApp ){
      assert( pFunc->zName==nth_valueName
           || pFunc->zName==first_valueName
      );
      assert( bInverse==0 || bInverse==1 );
      sqlite3VdbeAddOp2(v, OP_AddImm, pWin->regApp+1-bInverse, 1);
    }else if( pFunc->xSFunc!=noopStepFunc ){
      int addrIf = 0;
      if( pWin->pFilter ){
        int regTmp;
        assert( pWin->bExprArgs || !nArg ||nArg==pWin->pOwner->x.pList->nExpr );
        assert( pWin->bExprArgs || nArg  ||pWin->pOwner->x.pList==0 );
        regTmp = sqlite3GetTempReg(pParse);
        sqlite3VdbeAddOp3(v, OP_Column, csr, pWin->iArgCol+nArg,regTmp);
        addrIf = sqlite3VdbeAddOp3(v, OP_IfNot, regTmp, 0, 1);
        VdbeCoverage(v);
        sqlite3ReleaseTempReg(pParse, regTmp);
      }
      
      if( pWin->bExprArgs ){
        int iStart = sqlite3VdbeCurrentAddr(v);
        VdbeOp *pOp, *pEnd;

        nArg = pWin->pOwner->x.pList->nExpr;
        regArg = sqlite3GetTempRange(pParse, nArg);
        sqlite3ExprCodeExprList(pParse, pWin->pOwner->x.pList, regArg, 0, 0);

        pEnd = sqlite3VdbeGetOp(v, -1);
        for(pOp=sqlite3VdbeGetOp(v, iStart); pOp<=pEnd; pOp++){
          if( pOp->opcode==OP_Column && pOp->p1==pWin->iEphCsr ){
            pOp->p1 = csr;
          }
        }
      }
      if( pFunc->funcFlags & SQLITE_FUNC_NEEDCOLL ){
        CollSeq *pColl;
        assert( nArg>0 );
        pColl = sqlite3ExprNNCollSeq(pParse, pWin->pOwner->x.pList->a[0].pExpr);
        sqlite3VdbeAddOp4(v, OP_CollSeq, 0,0,0, (const char*)pColl, P4_COLLSEQ);
      }
      sqlite3VdbeAddOp3(v, bInverse? OP_AggInverse : OP_AggStep, 
                        bInverse, regArg, pWin->regAccum);
      sqlite3VdbeAppendP4(v, pFunc, P4_FUNCDEF);
      sqlite3VdbeChangeP5(v, (u8)nArg);
      if( pWin->bExprArgs ){
        sqlite3ReleaseTempRange(pParse, regArg, nArg);
      }
      if( addrIf ) sqlite3VdbeJumpHere(v, addrIf);
    }
  }
}

/*
** Values that may be passed as the second argument to windowCodeOp().
*/
#define WINDOW_RETURN_ROW 1
#define WINDOW_AGGINVERSE 2
#define WINDOW_AGGSTEP    3

/*
** Generate VM code to invoke either xValue() (bFin==0) or xFinalize()
** (bFin==1) for each window function in the linked list starting at
** pMWin. Or, for built-in window-functions that do not use the standard
** API, generate the equivalent VM code.
*/
static void windowAggFinal(WindowCodeArg *p, int bFin){
  Parse *pParse = p->pParse;
  Window *pMWin = p->pMWin;
  Vdbe *v = sqlite3GetVdbe(pParse);
  Window *pWin;

  for(pWin=pMWin; pWin; pWin=pWin->pNextWin){
    if( pMWin->regStartRowid==0
     && (pWin->pFunc->funcFlags & SQLITE_FUNC_MINMAX) 
     && (pWin->eStart!=TK_UNBOUNDED)
    ){
      sqlite3VdbeAddOp2(v, OP_Null, 0, pWin->regResult);
      sqlite3VdbeAddOp1(v, OP_Last, pWin->csrApp);
      VdbeCoverage(v);
      sqlite3VdbeAddOp3(v, OP_Column, pWin->csrApp, 0, pWin->regResult);
      sqlite3VdbeJumpHere(v, sqlite3VdbeCurrentAddr(v)-2);
    }else if( pWin->regApp ){
      assert( pMWin->regStartRowid==0 );
    }else{
      int nArg = windowArgCount(pWin);
      if( bFin ){
        sqlite3VdbeAddOp2(v, OP_AggFinal, pWin->regAccum, nArg);
        sqlite3VdbeAppendP4(v, pWin->pFunc, P4_FUNCDEF);
        sqlite3VdbeAddOp2(v, OP_Copy, pWin->regAccum, pWin->regResult);
        sqlite3VdbeAddOp2(v, OP_Null, 0, pWin->regAccum);
      }else{
        sqlite3VdbeAddOp3(v, OP_AggValue,pWin->regAccum,nArg,pWin->regResult);
        sqlite3VdbeAppendP4(v, pWin->pFunc, P4_FUNCDEF);
      }
    }
  }
}

/*
** Generate code to calculate the current values of all window functions in the
** p->pMWin list by doing a full scan of the current window frame. Store the
** results in the Window.regResult registers, ready to return the upper
** layer.
*/
static void windowFullScan(WindowCodeArg *p){
  Window *pWin;
  Parse *pParse = p->pParse;
  Window *pMWin = p->pMWin;
  Vdbe *v = p->pVdbe;

  int regCRowid = 0;              /* Current rowid value */
  int regCPeer = 0;               /* Current peer values */
  int regRowid = 0;               /* AggStep rowid value */
  int regPeer = 0;                /* AggStep peer values */

  int nPeer;
  int lblNext;
  int lblBrk;
  int addrNext;
  int csr;

  VdbeModuleComment((v, "windowFullScan begin"));

  assert( pMWin!=0 );
  csr = pMWin->csrApp;
  nPeer = (pMWin->pOrderBy ? pMWin->pOrderBy->nExpr : 0);

  lblNext = sqlite3VdbeMakeLabel(pParse);
  lblBrk = sqlite3VdbeMakeLabel(pParse);

  regCRowid = sqlite3GetTempReg(pParse);
  regRowid = sqlite3GetTempReg(pParse);
  if( nPeer ){
    regCPeer = sqlite3GetTempRange(pParse, nPeer);
    regPeer = sqlite3GetTempRange(pParse, nPeer);
  }

  sqlite3VdbeAddOp2(v, OP_Rowid, pMWin->iEphCsr, regCRowid);
  windowReadPeerValues(p, pMWin->iEphCsr, regCPeer);

  for(pWin=pMWin; pWin; pWin=pWin->pNextWin){
    sqlite3VdbeAddOp2(v, OP_Null, 0, pWin->regAccum);
  }

  sqlite3VdbeAddOp3(v, OP_SeekGE, csr, lblBrk, pMWin->regStartRowid);
  VdbeCoverage(v);
  addrNext = sqlite3VdbeCurrentAddr(v);
  sqlite3VdbeAddOp2(v, OP_Rowid, csr, regRowid);
  sqlite3VdbeAddOp3(v, OP_Gt, pMWin->regEndRowid, lblBrk, regRowid);
  VdbeCoverageNeverNull(v);

  if( pMWin->eExclude==TK_CURRENT ){
    sqlite3VdbeAddOp3(v, OP_Eq, regCRowid, lblNext, regRowid);
    VdbeCoverageNeverNull(v);
  }else if( pMWin->eExclude!=TK_NO ){
    int addr;
    int addrEq = 0;
    KeyInfo *pKeyInfo = 0;

    if( pMWin->pOrderBy ){
      pKeyInfo = sqlite3KeyInfoFromExprList(pParse, pMWin->pOrderBy, 0, 0);
    }
    if( pMWin->eExclude==TK_TIES ){
      addrEq = sqlite3VdbeAddOp3(v, OP_Eq, regCRowid, 0, regRowid);
      VdbeCoverageNeverNull(v);
    }
    if( pKeyInfo ){
      windowReadPeerValues(p, csr, regPeer);
      sqlite3VdbeAddOp3(v, OP_Compare, regPeer, regCPeer, nPeer);
      sqlite3VdbeAppendP4(v, (void*)pKeyInfo, P4_KEYINFO);
      addr = sqlite3VdbeCurrentAddr(v)+1;
      sqlite3VdbeAddOp3(v, OP_Jump, addr, lblNext, addr);
      VdbeCoverageEqNe(v);
    }else{
      sqlite3VdbeAddOp2(v, OP_Goto, 0, lblNext);
    }
    if( addrEq ) sqlite3VdbeJumpHere(v, addrEq);
  }

  windowAggStep(p, pMWin, csr, 0, p->regArg);

  sqlite3VdbeResolveLabel(v, lblNext);
  sqlite3VdbeAddOp2(v, OP_Next, csr, addrNext);
  VdbeCoverage(v);
  sqlite3VdbeJumpHere(v, addrNext-1);
  sqlite3VdbeJumpHere(v, addrNext+1);
  sqlite3ReleaseTempReg(pParse, regRowid);
  sqlite3ReleaseTempReg(pParse, regCRowid);
  if( nPeer ){
    sqlite3ReleaseTempRange(pParse, regPeer, nPeer);
    sqlite3ReleaseTempRange(pParse, regCPeer, nPeer);
  }

  windowAggFinal(p, 1);
  VdbeModuleComment((v, "windowFullScan end"));
}

/*
** Invoke the sub-routine at regGosub (generated by code in select.c) to
** return the current row of Window.iEphCsr. If all window functions are
** aggregate window functions that use the standard API, a single
** OP_Gosub instruction is all that this routine generates. Extra VM code
** for per-row processing is only generated for the following built-in window
** functions:
**
**   nth_value()
**   first_value()
**   lag()
**   lead()
*/
static void windowReturnOneRow(WindowCodeArg *p){
  Window *pMWin = p->pMWin;
  Vdbe *v = p->pVdbe;

  if( pMWin->regStartRowid ){
    windowFullScan(p);
  }else{
    Parse *pParse = p->pParse;
    Window *pWin;

    for(pWin=pMWin; pWin; pWin=pWin->pNextWin){
      FuncDef *pFunc = pWin->pFunc;
      if( pFunc->zName==nth_valueName
       || pFunc->zName==first_valueName
      ){
        int csr = pWin->csrApp;
        int lbl = sqlite3VdbeMakeLabel(pParse);
        int tmpReg = sqlite3GetTempReg(pParse);
        sqlite3VdbeAddOp2(v, OP_Null, 0, pWin->regResult);
  
        if( pFunc->zName==nth_valueName ){
          sqlite3VdbeAddOp3(v, OP_Column,pMWin->iEphCsr,pWin->iArgCol+1,tmpReg);
          windowCheckValue(pParse, tmpReg, 2);
        }else{
          sqlite3VdbeAddOp2(v, OP_Integer, 1, tmpReg);
        }
        sqlite3VdbeAddOp3(v, OP_Add, tmpReg, pWin->regApp, tmpReg);
        sqlite3VdbeAddOp3(v, OP_Gt, pWin->regApp+1, lbl, tmpReg);
        VdbeCoverageNeverNull(v);
        sqlite3VdbeAddOp3(v, OP_SeekRowid, csr, 0, tmpReg);
        VdbeCoverageNeverTaken(v);
        sqlite3VdbeAddOp3(v, OP_Column, csr, pWin->iArgCol, pWin->regResult);
        sqlite3VdbeResolveLabel(v, lbl);
        sqlite3ReleaseTempReg(pParse, tmpReg);
      }
      else if( pFunc->zName==leadName || pFunc->zName==lagName ){
        int nArg = pWin->pOwner->x.pList->nExpr;
        int csr = pWin->csrApp;
        int lbl = sqlite3VdbeMakeLabel(pParse);
        int tmpReg = sqlite3GetTempReg(pParse);
        int iEph = pMWin->iEphCsr;
  
        if( nArg<3 ){
          sqlite3VdbeAddOp2(v, OP_Null, 0, pWin->regResult);
        }else{
          sqlite3VdbeAddOp3(v, OP_Column, iEph,pWin->iArgCol+2,pWin->regResult);
        }
        sqlite3VdbeAddOp2(v, OP_Rowid, iEph, tmpReg);
        if( nArg<2 ){
          int val = (pFunc->zName==leadName ? 1 : -1);
          sqlite3VdbeAddOp2(v, OP_AddImm, tmpReg, val);
        }else{
          int op = (pFunc->zName==leadName ? OP_Add : OP_Subtract);
          int tmpReg2 = sqlite3GetTempReg(pParse);
          sqlite3VdbeAddOp3(v, OP_Column, iEph, pWin->iArgCol+1, tmpReg2);
          sqlite3VdbeAddOp3(v, op, tmpReg2, tmpReg, tmpReg);
          sqlite3ReleaseTempReg(pParse, tmpReg2);
        }
  
        sqlite3VdbeAddOp3(v, OP_SeekRowid, csr, lbl, tmpReg);
        VdbeCoverage(v);
        sqlite3VdbeAddOp3(v, OP_Column, csr, pWin->iArgCol, pWin->regResult);
        sqlite3VdbeResolveLabel(v, lbl);
        sqlite3ReleaseTempReg(pParse, tmpReg);
      }
    }
  }
  sqlite3VdbeAddOp2(v, OP_Gosub, p->regGosub, p->addrGosub);
}

/*
** Generate code to set the accumulator register for each window function
** in the linked list passed as the second argument to NULL. And perform
** any equivalent initialization required by any built-in window functions
** in the list.
*/
static int windowInitAccum(Parse *pParse, Window *pMWin){
  Vdbe *v = sqlite3GetVdbe(pParse);
  int regArg;
  int nArg = 0;
  Window *pWin;
  for(pWin=pMWin; pWin; pWin=pWin->pNextWin){
    FuncDef *pFunc = pWin->pFunc;
    sqlite3VdbeAddOp2(v, OP_Null, 0, pWin->regAccum);
    nArg = MAX(nArg, windowArgCount(pWin));
    if( pMWin->regStartRowid==0 ){
      if( pFunc->zName==nth_valueName || pFunc->zName==first_valueName ){
        sqlite3VdbeAddOp2(v, OP_Integer, 0, pWin->regApp);
        sqlite3VdbeAddOp2(v, OP_Integer, 0, pWin->regApp+1);
      }

      if( (pFunc->funcFlags & SQLITE_FUNC_MINMAX) && pWin->csrApp ){
        assert( pWin->eStart!=TK_UNBOUNDED );
        sqlite3VdbeAddOp1(v, OP_ResetSorter, pWin->csrApp);
        sqlite3VdbeAddOp2(v, OP_Integer, 0, pWin->regApp+1);
      }
    }
  }
  regArg = pParse->nMem+1;
  pParse->nMem += nArg;
  return regArg;
}

/* 
** Return true if the current frame should be cached in the ephemeral table,
** even if there are no xInverse() calls required.
*/
static int windowCacheFrame(Window *pMWin){
  Window *pWin;
  if( pMWin->regStartRowid ) return 1;
  for(pWin=pMWin; pWin; pWin=pWin->pNextWin){
    FuncDef *pFunc = pWin->pFunc;
    if( (pFunc->zName==nth_valueName)
     || (pFunc->zName==first_valueName)
     || (pFunc->zName==leadName)
     || (pFunc->zName==lagName)
    ){
      return 1;
    }
  }
  return 0;
}

/*
** regOld and regNew are each the first register in an array of size
** pOrderBy->nExpr. This function generates code to compare the two
** arrays of registers using the collation sequences and other comparison
** parameters specified by pOrderBy. 
**
** If the two arrays are not equal, the contents of regNew is copied to 
** regOld and control falls through. Otherwise, if the contents of the arrays
** are equal, an OP_Goto is executed. The address of the OP_Goto is returned.
*/
static void windowIfNewPeer(
  Parse *pParse,
  ExprList *pOrderBy,
  int regNew,                     /* First in array of new values */
  int regOld,                     /* First in array of old values */
  int addr                        /* Jump here */
){
  Vdbe *v = sqlite3GetVdbe(pParse);
  if( pOrderBy ){
    int nVal = pOrderBy->nExpr;
    KeyInfo *pKeyInfo = sqlite3KeyInfoFromExprList(pParse, pOrderBy, 0, 0);
    sqlite3VdbeAddOp3(v, OP_Compare, regOld, regNew, nVal);
    sqlite3VdbeAppendP4(v, (void*)pKeyInfo, P4_KEYINFO);
    sqlite3VdbeAddOp3(v, OP_Jump, 
      sqlite3VdbeCurrentAddr(v)+1, addr, sqlite3VdbeCurrentAddr(v)+1
    );
    VdbeCoverageEqNe(v);
    sqlite3VdbeAddOp3(v, OP_Copy, regNew, regOld, nVal-1);
  }else{
    sqlite3VdbeAddOp2(v, OP_Goto, 0, addr);
  }
}

/*
** This function is called as part of generating VM programs for RANGE
** offset PRECEDING/FOLLOWING frame boundaries. Assuming "ASC" order for
** the ORDER BY term in the window, and that argument op is OP_Ge, it generates
** code equivalent to:
**
**   if( csr1.peerVal + regVal >= csr2.peerVal ) goto lbl;
**
** The value of parameter op may also be OP_Gt or OP_Le. In these cases the
** operator in the above pseudo-code is replaced with ">" or "<=", respectively.
**
** If the sort-order for the ORDER BY term in the window is DESC, then the
** comparison is reversed. Instead of adding regVal to csr1.peerVal, it is
** subtracted. And the comparison operator is inverted to - ">=" becomes "<=",
** ">" becomes "<", and so on. So, with DESC sort order, if the argument op
** is OP_Ge, the generated code is equivalent to:
**
**   if( csr1.peerVal - regVal <= csr2.peerVal ) goto lbl;
**
** A special type of arithmetic is used such that if csr1.peerVal is not
** a numeric type (real or integer), then the result of the addition addition
** or subtraction is a a copy of csr1.peerVal.
*/
static void windowCodeRangeTest(
  WindowCodeArg *p, 
  int op,                         /* OP_Ge, OP_Gt, or OP_Le */
  int csr1,                       /* Cursor number for cursor 1 */
  int regVal,                     /* Register containing non-negative number */
  int csr2,                       /* Cursor number for cursor 2 */
  int lbl                         /* Jump destination if condition is true */
){
  Parse *pParse = p->pParse;
  Vdbe *v = sqlite3GetVdbe(pParse);
  ExprList *pOrderBy = p->pMWin->pOrderBy;  /* ORDER BY clause for window */
  int reg1 = sqlite3GetTempReg(pParse);     /* Reg. for csr1.peerVal+regVal */
  int reg2 = sqlite3GetTempReg(pParse);     /* Reg. for csr2.peerVal */
  int regString = ++pParse->nMem;           /* Reg. for constant value '' */
  int arith = OP_Add;                       /* OP_Add or OP_Subtract */
  int addrGe;                               /* Jump destination */

  assert( op==OP_Ge || op==OP_Gt || op==OP_Le );
  assert( pOrderBy && pOrderBy->nExpr==1 );
  if( pOrderBy->a[0].sortFlags & KEYINFO_ORDER_DESC ){
    switch( op ){
      case OP_Ge: op = OP_Le; break;
      case OP_Gt: op = OP_Lt; break;
      default: assert( op==OP_Le ); op = OP_Ge; break;
    }
    arith = OP_Subtract;
  }

  /* Read the peer-value from each cursor into a register */
  windowReadPeerValues(p, csr1, reg1);
  windowReadPeerValues(p, csr2, reg2);

  VdbeModuleComment((v, "CodeRangeTest: if( R%d %s R%d %s R%d ) goto lbl",
      reg1, (arith==OP_Add ? "+" : "-"), regVal,
      ((op==OP_Ge) ? ">=" : (op==OP_Le) ? "<=" : (op==OP_Gt) ? ">" : "<"), reg2
  ));

  /* Register reg1 currently contains csr1.peerVal (the peer-value from csr1).
  ** This block adds (or subtracts for DESC) the numeric value in regVal
  ** from it. Or, if reg1 is not numeric (it is a NULL, a text value or a blob),
  ** then leave reg1 as it is. In pseudo-code, this is implemented as:
  **
  **   if( reg1>='' ) goto addrGe;
  **   reg1 = reg1 +/- regVal
  **   addrGe:
  **
  ** Since all strings and blobs are greater-than-or-equal-to an empty string,
  ** the add/subtract is skipped for these, as required. If reg1 is a NULL,
  ** then the arithmetic is performed, but since adding or subtracting from
  ** NULL is always NULL anyway, this case is handled as required too.  */
  sqlite3VdbeAddOp4(v, OP_String8, 0, regString, 0, "", P4_STATIC);
  addrGe = sqlite3VdbeAddOp3(v, OP_Ge, regString, 0, reg1);
  VdbeCoverage(v);
  sqlite3VdbeAddOp3(v, arith, regVal, reg1, reg1);
  sqlite3VdbeJumpHere(v, addrGe);

  /* If the BIGNULL flag is set for the ORDER BY, then it is required to 
  ** consider NULL values to be larger than all other values, instead of 
  ** the usual smaller. The VDBE opcodes OP_Ge and so on do not handle this
  ** (and adding that capability causes a performance regression), so
  ** instead if the BIGNULL flag is set then cases where either reg1 or
  ** reg2 are NULL are handled separately in the following block. The code
  ** generated is equivalent to:
  **
  **   if( reg1 IS NULL ){
  **     if( op==OP_Ge ) goto lbl;
  **     if( op==OP_Gt && reg2 IS NOT NULL ) goto lbl;
  **     if( op==OP_Le && reg2 IS NULL ) goto lbl;
  **   }else if( reg2 IS NULL ){
  **     if( op==OP_Le ) goto lbl;
  **   }
  **
  ** Additionally, if either reg1 or reg2 are NULL but the jump to lbl is 
  ** not taken, control jumps over the comparison operator coded below this
  ** block.  */
  if( pOrderBy->a[0].sortFlags & KEYINFO_ORDER_BIGNULL ){
    /* This block runs if reg1 contains a NULL. */
    int addr = sqlite3VdbeAddOp1(v, OP_NotNull, reg1); VdbeCoverage(v);
    switch( op ){
      case OP_Ge: 
        sqlite3VdbeAddOp2(v, OP_Goto, 0, lbl); 
        break;
      case OP_Gt: 
        sqlite3VdbeAddOp2(v, OP_NotNull, reg2, lbl); 
        VdbeCoverage(v); 
        break;
      case OP_Le: 
        sqlite3VdbeAddOp2(v, OP_IsNull, reg2, lbl); 
        VdbeCoverage(v); 
        break;
      default: assert( op==OP_Lt ); /* no-op */ break;
    }
    sqlite3VdbeAddOp2(v, OP_Goto, 0, sqlite3VdbeCurrentAddr(v)+3);

    /* This block runs if reg1 is not NULL, but reg2 is. */
    sqlite3VdbeJumpHere(v, addr);
    sqlite3VdbeAddOp2(v, OP_IsNull, reg2, lbl); VdbeCoverage(v);
    if( op==OP_Gt || op==OP_Ge ){
      sqlite3VdbeChangeP2(v, -1, sqlite3VdbeCurrentAddr(v)+1);
    }
  }

  /* Compare registers reg2 and reg1, taking the jump if required. Note that
  ** control skips over this test if the BIGNULL flag is set and either
  ** reg1 or reg2 contain a NULL value.  */
  sqlite3VdbeAddOp3(v, op, reg2, lbl, reg1); VdbeCoverage(v);
  sqlite3VdbeChangeP5(v, SQLITE_NULLEQ);

  assert( op==OP_Ge || op==OP_Gt || op==OP_Lt || op==OP_Le );
  testcase(op==OP_Ge); VdbeCoverageIf(v, op==OP_Ge);
  testcase(op==OP_Lt); VdbeCoverageIf(v, op==OP_Lt);
  testcase(op==OP_Le); VdbeCoverageIf(v, op==OP_Le);
  testcase(op==OP_Gt); VdbeCoverageIf(v, op==OP_Gt);
  sqlite3ReleaseTempReg(pParse, reg1);
  sqlite3ReleaseTempReg(pParse, reg2);

  VdbeModuleComment((v, "CodeRangeTest: end"));
}

/*
** Helper function for sqlite3WindowCodeStep(). Each call to this function
** generates VM code for a single RETURN_ROW, AGGSTEP or AGGINVERSE 
** operation. Refer to the header comment for sqlite3WindowCodeStep() for
** details.
*/
static int windowCodeOp(
 WindowCodeArg *p,                /* Context object */
 int op,                          /* WINDOW_RETURN_ROW, AGGSTEP or AGGINVERSE */
 int regCountdown,                /* Register for OP_IfPos countdown */
 int jumpOnEof                    /* Jump here if stepped cursor reaches EOF */
){
  int csr, reg;
  Parse *pParse = p->pParse;
  Window *pMWin = p->pMWin;
  int ret = 0;
  Vdbe *v = p->pVdbe;
  int addrContinue = 0;
  int bPeer = (pMWin->eFrmType!=TK_ROWS);

  int lblDone = sqlite3VdbeMakeLabel(pParse);
  int addrNextRange = 0;

  /* Special case - WINDOW_AGGINVERSE is always a no-op if the frame
  ** starts with UNBOUNDED PRECEDING. */
  if( op==WINDOW_AGGINVERSE && pMWin->eStart==TK_UNBOUNDED ){
    assert( regCountdown==0 && jumpOnEof==0 );
    return 0;
  }

  if( regCountdown>0 ){
    if( pMWin->eFrmType==TK_RANGE ){
      addrNextRange = sqlite3VdbeCurrentAddr(v);
      assert( op==WINDOW_AGGINVERSE || op==WINDOW_AGGSTEP );
      if( op==WINDOW_AGGINVERSE ){
        if( pMWin->eStart==TK_FOLLOWING ){
          windowCodeRangeTest(
              p, OP_Le, p->current.csr, regCountdown, p->start.csr, lblDone
          );
        }else{
          windowCodeRangeTest(
              p, OP_Ge, p->start.csr, regCountdown, p->current.csr, lblDone
          );
        }
      }else{
        windowCodeRangeTest(
            p, OP_Gt, p->end.csr, regCountdown, p->current.csr, lblDone
        );
      }
    }else{
      sqlite3VdbeAddOp3(v, OP_IfPos, regCountdown, lblDone, 1);
      VdbeCoverage(v);
    }
  }

  if( op==WINDOW_RETURN_ROW && pMWin->regStartRowid==0 ){
    windowAggFinal(p, 0);
  }
  addrContinue = sqlite3VdbeCurrentAddr(v);

  /* If this is a (RANGE BETWEEN a FOLLOWING AND b FOLLOWING) or
  ** (RANGE BETWEEN b PRECEDING AND a PRECEDING) frame, ensure the 
  ** start cursor does not advance past the end cursor within the 
  ** temporary table. It otherwise might, if (a>b).  */
  if( pMWin->eStart==pMWin->eEnd && regCountdown
   && pMWin->eFrmType==TK_RANGE && op==WINDOW_AGGINVERSE
  ){
    int regRowid1 = sqlite3GetTempReg(pParse);
    int regRowid2 = sqlite3GetTempReg(pParse);
    sqlite3VdbeAddOp2(v, OP_Rowid, p->start.csr, regRowid1);
    sqlite3VdbeAddOp2(v, OP_Rowid, p->end.csr, regRowid2);
    sqlite3VdbeAddOp3(v, OP_Ge, regRowid2, lblDone, regRowid1);
    VdbeCoverage(v);
    sqlite3ReleaseTempReg(pParse, regRowid1);
    sqlite3ReleaseTempReg(pParse, regRowid2);
    assert( pMWin->eStart==TK_PRECEDING || pMWin->eStart==TK_FOLLOWING );
  }

  switch( op ){
    case WINDOW_RETURN_ROW:
      csr = p->current.csr;
      reg = p->current.reg;
      windowReturnOneRow(p);
      break;

    case WINDOW_AGGINVERSE:
      csr = p->start.csr;
      reg = p->start.reg;
      if( pMWin->regStartRowid ){
        assert( pMWin->regEndRowid );
        sqlite3VdbeAddOp2(v, OP_AddImm, pMWin->regStartRowid, 1);
      }else{
        windowAggStep(p, pMWin, csr, 1, p->regArg);
      }
      break;

    default:
      assert( op==WINDOW_AGGSTEP );
      csr = p->end.csr;
      reg = p->end.reg;
      if( pMWin->regStartRowid ){
        assert( pMWin->regEndRowid );
        sqlite3VdbeAddOp2(v, OP_AddImm, pMWin->regEndRowid, 1);
      }else{
        windowAggStep(p, pMWin, csr, 0, p->regArg);
      }
      break;
  }

  if( op==p->eDelete ){
    sqlite3VdbeAddOp1(v, OP_Delete, csr);
    sqlite3VdbeChangeP5(v, OPFLAG_SAVEPOSITION);
  }

  if( jumpOnEof ){
    sqlite3VdbeAddOp2(v, OP_Next, csr, sqlite3VdbeCurrentAddr(v)+2);
    VdbeCoverage(v);
    ret = sqlite3VdbeAddOp0(v, OP_Goto);
  }else{
    sqlite3VdbeAddOp2(v, OP_Next, csr, sqlite3VdbeCurrentAddr(v)+1+bPeer);
    VdbeCoverage(v);
    if( bPeer ){
      sqlite3VdbeAddOp2(v, OP_Goto, 0, lblDone);
    }
  }

  if( bPeer ){
    int nReg = (pMWin->pOrderBy ? pMWin->pOrderBy->nExpr : 0);
    int regTmp = (nReg ? sqlite3GetTempRange(pParse, nReg) : 0);
    windowReadPeerValues(p, csr, regTmp);
    windowIfNewPeer(pParse, pMWin->pOrderBy, regTmp, reg, addrContinue);
    sqlite3ReleaseTempRange(pParse, regTmp, nReg);
  }

  if( addrNextRange ){
    sqlite3VdbeAddOp2(v, OP_Goto, 0, addrNextRange);
  }
  sqlite3VdbeResolveLabel(v, lblDone);
  return ret;
}


/*
** Allocate and return a duplicate of the Window object indicated by the
** third argument. Set the Window.pOwner field of the new object to
** pOwner.
*/
SQLITE_PRIVATE Window *sqlite3WindowDup(sqlite3 *db, Expr *pOwner, Window *p){
  Window *pNew = 0;
  if( ALWAYS(p) ){
    pNew = sqlite3DbMallocZero(db, sizeof(Window));
    if( pNew ){
      pNew->zName = sqlite3DbStrDup(db, p->zName);
      pNew->zBase = sqlite3DbStrDup(db, p->zBase);
      pNew->pFilter = sqlite3ExprDup(db, p->pFilter, 0);
      pNew->pFunc = p->pFunc;
      pNew->pPartition = sqlite3ExprListDup(db, p->pPartition, 0);
      pNew->pOrderBy = sqlite3ExprListDup(db, p->pOrderBy, 0);
      pNew->eFrmType = p->eFrmType;
      pNew->eEnd = p->eEnd;
      pNew->eStart = p->eStart;
      pNew->eExclude = p->eExclude;
      pNew->regResult = p->regResult;
      pNew->pStart = sqlite3ExprDup(db, p->pStart, 0);
      pNew->pEnd = sqlite3ExprDup(db, p->pEnd, 0);
      pNew->pOwner = pOwner;
      pNew->bImplicitFrame = p->bImplicitFrame;
    }
  }
  return pNew;
}

/*
** Return a copy of the linked list of Window objects passed as the
** second argument.
*/
SQLITE_PRIVATE Window *sqlite3WindowListDup(sqlite3 *db, Window *p){
  Window *pWin;
  Window *pRet = 0;
  Window **pp = &pRet;

  for(pWin=p; pWin; pWin=pWin->pNextWin){
    *pp = sqlite3WindowDup(db, 0, pWin);
    if( *pp==0 ) break;
    pp = &((*pp)->pNextWin);
  }

  return pRet;
}

/*
** Return true if it can be determined at compile time that expression 
** pExpr evaluates to a value that, when cast to an integer, is greater 
** than zero. False otherwise.
**
** If an OOM error occurs, this function sets the Parse.db.mallocFailed 
** flag and returns zero.
*/
static int windowExprGtZero(Parse *pParse, Expr *pExpr){
  int ret = 0;
  sqlite3 *db = pParse->db;
  sqlite3_value *pVal = 0;
  sqlite3ValueFromExpr(db, pExpr, db->enc, SQLITE_AFF_NUMERIC, &pVal);
  if( pVal && sqlite3_value_int(pVal)>0 ){
    ret = 1;
  }
  sqlite3ValueFree(pVal);
  return ret;
}

/*
** sqlite3WhereBegin() has already been called for the SELECT statement 
** passed as the second argument when this function is invoked. It generates
** code to populate the Window.regResult register for each window function 
** and invoke the sub-routine at instruction addrGosub once for each row.
** sqlite3WhereEnd() is always called before returning. 
**
** This function handles several different types of window frames, which
** require slightly different processing. The following pseudo code is
** used to implement window frames of the form:
**
**   ROWS BETWEEN <expr1> PRECEDING AND <expr2> FOLLOWING
**
** Other window frame types use variants of the following:
**
**     ... loop started by sqlite3WhereBegin() ...
**       if( new partition ){
**         Gosub flush
**       }
**       Insert new row into eph table.
**       
**       if( first row of partition ){
**         // Rewind three cursors, all open on the eph table.
**         Rewind(csrEnd);
**         Rewind(csrStart);
**         Rewind(csrCurrent);
**       
**         regEnd = <expr2>          // FOLLOWING expression
**         regStart = <expr1>        // PRECEDING expression
**       }else{
**         // First time this branch is taken, the eph table contains two 
**         // rows. The first row in the partition, which all three cursors
**         // currently point to, and the following row.
**         AGGSTEP
**         if( (regEnd--)<=0 ){
**           RETURN_ROW
**           if( (regStart--)<=0 ){
**             AGGINVERSE
**           }
**         }
**       }
**     }
**     flush:
**       AGGSTEP
**       while( 1 ){
**         RETURN ROW
**         if( csrCurrent is EOF ) break;
**         if( (regStart--)<=0 ){
**           AggInverse(csrStart)
**           Next(csrStart)
**         }
**       }
**
** The pseudo-code above uses the following shorthand:
**
**   AGGSTEP:    invoke the aggregate xStep() function for each window function
**               with arguments read from the current row of cursor csrEnd, then
**               step cursor csrEnd forward one row (i.e. sqlite3BtreeNext()).
**
**   RETURN_ROW: return a row to the caller based on the contents of the 
**               current row of csrCurrent and the current state of all 
**               aggregates. Then step cursor csrCurrent forward one row.
**
**   AGGINVERSE: invoke the aggregate xInverse() function for each window 
**               functions with arguments read from the current row of cursor
**               csrStart. Then step csrStart forward one row.
**
** There are two other ROWS window frames that are handled significantly
** differently from the above - "BETWEEN <expr> PRECEDING AND <expr> PRECEDING"
** and "BETWEEN <expr> FOLLOWING AND <expr> FOLLOWING". These are special 
** cases because they change the order in which the three cursors (csrStart,
** csrCurrent and csrEnd) iterate through the ephemeral table. Cases that
** use UNBOUNDED or CURRENT ROW are much simpler variations on one of these
** three.
**
**   ROWS BETWEEN <expr1> PRECEDING AND <expr2> PRECEDING
**
**     ... loop started by sqlite3WhereBegin() ...
**       if( new partition ){
**         Gosub flush
**       }
**       Insert new row into eph table.
**       if( first row of partition ){
**         Rewind(csrEnd) ; Rewind(csrStart) ; Rewind(csrCurrent)
**         regEnd = <expr2>
**         regStart = <expr1>
**       }else{
**         if( (regEnd--)<=0 ){
**           AGGSTEP
**         }
**         RETURN_ROW
**         if( (regStart--)<=0 ){
**           AGGINVERSE
**         }
**       }
**     }
**     flush:
**       if( (regEnd--)<=0 ){
**         AGGSTEP
**       }
**       RETURN_ROW
**
**
**   ROWS BETWEEN <expr1> FOLLOWING AND <expr2> FOLLOWING
**
**     ... loop started by sqlite3WhereBegin() ...
**     if( new partition ){
**       Gosub flush
**     }
**     Insert new row into eph table.
**     if( first row of partition ){
**       Rewind(csrEnd) ; Rewind(csrStart) ; Rewind(csrCurrent)
**       regEnd = <expr2>
**       regStart = regEnd - <expr1>
**     }else{
**       AGGSTEP
**       if( (regEnd--)<=0 ){
**         RETURN_ROW
**       }
**       if( (regStart--)<=0 ){
**         AGGINVERSE
**       }
**     }
**   }
**   flush:
**     AGGSTEP
**     while( 1 ){
**       if( (regEnd--)<=0 ){
**         RETURN_ROW
**         if( eof ) break;
**       }
**       if( (regStart--)<=0 ){
**         AGGINVERSE
**         if( eof ) break
**       }
**     }
**     while( !eof csrCurrent ){
**       RETURN_ROW
**     }
**
** For the most part, the patterns above are adapted to support UNBOUNDED by
** assuming that it is equivalent to "infinity PRECEDING/FOLLOWING" and
** CURRENT ROW by assuming that it is equivilent to "0 PRECEDING/FOLLOWING".
** This is optimized of course - branches that will never be taken and
** conditions that are always true are omitted from the VM code. The only
** exceptional case is:
**
**   ROWS BETWEEN <expr1> FOLLOWING AND UNBOUNDED FOLLOWING
**
**     ... loop started by sqlite3WhereBegin() ...
**     if( new partition ){
**       Gosub flush
**     }
**     Insert new row into eph table.
**     if( first row of partition ){
**       Rewind(csrEnd) ; Rewind(csrStart) ; Rewind(csrCurrent)
**       regStart = <expr1>
**     }else{
**       AGGSTEP
**     }
**   }
**   flush:
**     AGGSTEP
**     while( 1 ){
**       if( (regStart--)<=0 ){
**         AGGINVERSE
**         if( eof ) break
**       }
**       RETURN_ROW
**     }
**     while( !eof csrCurrent ){
**       RETURN_ROW
**     }
**
** Also requiring special handling are the cases:
**
**   ROWS BETWEEN <expr1> PRECEDING AND <expr2> PRECEDING
**   ROWS BETWEEN <expr1> FOLLOWING AND <expr2> FOLLOWING
**
** when (expr1 < expr2). This is detected at runtime, not by this function.
** To handle this case, the pseudo-code programs depicted above are modified
** slightly to be:
**
**     ... loop started by sqlite3WhereBegin() ...
**     if( new partition ){
**       Gosub flush
**     }
**     Insert new row into eph table.
**     if( first row of partition ){
**       Rewind(csrEnd) ; Rewind(csrStart) ; Rewind(csrCurrent)
**       regEnd = <expr2>
**       regStart = <expr1>
**       if( regEnd < regStart ){
**         RETURN_ROW
**         delete eph table contents
**         continue
**       }
**     ...
**
** The new "continue" statement in the above jumps to the next iteration
** of the outer loop - the one started by sqlite3WhereBegin().
**
** The various GROUPS cases are implemented using the same patterns as
** ROWS. The VM code is modified slightly so that:
**
**   1. The else branch in the main loop is only taken if the row just
**      added to the ephemeral table is the start of a new group. In
**      other words, it becomes:
**
**         ... loop started by sqlite3WhereBegin() ...
**         if( new partition ){
**           Gosub flush
**         }
**         Insert new row into eph table.
**         if( first row of partition ){
**           Rewind(csrEnd) ; Rewind(csrStart) ; Rewind(csrCurrent)
**           regEnd = <expr2>
**           regStart = <expr1>
**         }else if( new group ){
**           ... 
**         }
**       }
**
**   2. Instead of processing a single row, each RETURN_ROW, AGGSTEP or 
**      AGGINVERSE step processes the current row of the relevant cursor and
**      all subsequent rows belonging to the same group.
**
** RANGE window frames are a little different again. As for GROUPS, the 
** main loop runs once per group only. And RETURN_ROW, AGGSTEP and AGGINVERSE
** deal in groups instead of rows. As for ROWS and GROUPS, there are three
** basic cases:
**
**   RANGE BETWEEN <expr1> PRECEDING AND <expr2> FOLLOWING
**
**     ... loop started by sqlite3WhereBegin() ...
**       if( new partition ){
**         Gosub flush
**       }
**       Insert new row into eph table.
**       if( first row of partition ){
**         Rewind(csrEnd) ; Rewind(csrStart) ; Rewind(csrCurrent)
**         regEnd = <expr2>
**         regStart = <expr1>
**       }else{
**         AGGSTEP
**         while( (csrCurrent.key + regEnd) < csrEnd.key ){
**           RETURN_ROW
**           while( csrStart.key + regStart) < csrCurrent.key ){
**             AGGINVERSE
**           }
**         }
**       }
**     }
**     flush:
**       AGGSTEP
**       while( 1 ){
**         RETURN ROW
**         if( csrCurrent is EOF ) break;
**           while( csrStart.key + regStart) < csrCurrent.key ){
**             AGGINVERSE
**           }
**         }
**       }
**
** In the above notation, "csr.key" means the current value of the ORDER BY 
** expression (there is only ever 1 for a RANGE that uses an <expr> FOLLOWING
** or <expr PRECEDING) read from cursor csr.
**
**   RANGE BETWEEN <expr1> PRECEDING AND <expr2> PRECEDING
**
**     ... loop started by sqlite3WhereBegin() ...
**       if( new partition ){
**         Gosub flush
**       }
**       Insert new row into eph table.
**       if( first row of partition ){
**         Rewind(csrEnd) ; Rewind(csrStart) ; Rewind(csrCurrent)
**         regEnd = <expr2>
**         regStart = <expr1>
**       }else{
**         while( (csrEnd.key + regEnd) <= csrCurrent.key ){
**           AGGSTEP
**         }
**         while( (csrStart.key + regStart) < csrCurrent.key ){
**           AGGINVERSE
**         }
**         RETURN_ROW
**       }
**     }
**     flush:
**       while( (csrEnd.key + regEnd) <= csrCurrent.key ){
**         AGGSTEP
**       }
**       while( (csrStart.key + regStart) < csrCurrent.key ){
**         AGGINVERSE
**       }
**       RETURN_ROW
**
**   RANGE BETWEEN <expr1> FOLLOWING AND <expr2> FOLLOWING
**
**     ... loop started by sqlite3WhereBegin() ...
**       if( new partition ){
**         Gosub flush
**       }
**       Insert new row into eph table.
**       if( first row of partition ){
**         Rewind(csrEnd) ; Rewind(csrStart) ; Rewind(csrCurrent)
**         regEnd = <expr2>
**         regStart = <expr1>
**       }else{
**         AGGSTEP
**         while( (csrCurrent.key + regEnd) < csrEnd.key ){
**           while( (csrCurrent.key + regStart) > csrStart.key ){
**             AGGINVERSE
**           }
**           RETURN_ROW
**         }
**       }
**     }
**     flush:
**       AGGSTEP
**       while( 1 ){
**         while( (csrCurrent.key + regStart) > csrStart.key ){
**           AGGINVERSE
**           if( eof ) break "while( 1 )" loop.
**         }
**         RETURN_ROW
**       }
**       while( !eof csrCurrent ){
**         RETURN_ROW
**       }
**
** The text above leaves out many details. Refer to the code and comments
** below for a more complete picture.
*/
SQLITE_PRIVATE void sqlite3WindowCodeStep(
  Parse *pParse,                  /* Parse context */
  Select *p,                      /* Rewritten SELECT statement */
  WhereInfo *pWInfo,              /* Context returned by sqlite3WhereBegin() */
  int regGosub,                   /* Register for OP_Gosub */
  int addrGosub                   /* OP_Gosub here to return each row */
){
  Window *pMWin = p->pWin;
  ExprList *pOrderBy = pMWin->pOrderBy;
  Vdbe *v = sqlite3GetVdbe(pParse);
  int csrWrite;                   /* Cursor used to write to eph. table */
  int csrInput = p->pSrc->a[0].iCursor;     /* Cursor of sub-select */
  int nInput = p->pSrc->a[0].pTab->nCol;    /* Number of cols returned by sub */
  int iInput;                               /* To iterate through sub cols */
  int addrNe;                     /* Address of OP_Ne */
  int addrGosubFlush = 0;         /* Address of OP_Gosub to flush: */
  int addrInteger = 0;            /* Address of OP_Integer */
  int addrEmpty;                  /* Address of OP_Rewind in flush: */
  int regNew;                     /* Array of registers holding new input row */
  int regRecord;                  /* regNew array in record form */
  int regRowid;                   /* Rowid for regRecord in eph table */
  int regNewPeer = 0;             /* Peer values for new row (part of regNew) */
  int regPeer = 0;                /* Peer values for current row */
  int regFlushPart = 0;           /* Register for "Gosub flush_partition" */
  WindowCodeArg s;                /* Context object for sub-routines */
  int lblWhereEnd;                /* Label just before sqlite3WhereEnd() code */
  int regStart = 0;               /* Value of <expr> PRECEDING */
  int regEnd = 0;                 /* Value of <expr> FOLLOWING */

  assert( pMWin->eStart==TK_PRECEDING || pMWin->eStart==TK_CURRENT 
       || pMWin->eStart==TK_FOLLOWING || pMWin->eStart==TK_UNBOUNDED 
  );
  assert( pMWin->eEnd==TK_FOLLOWING || pMWin->eEnd==TK_CURRENT 
       || pMWin->eEnd==TK_UNBOUNDED || pMWin->eEnd==TK_PRECEDING 
  );
  assert( pMWin->eExclude==0 || pMWin->eExclude==TK_CURRENT
       || pMWin->eExclude==TK_GROUP || pMWin->eExclude==TK_TIES
       || pMWin->eExclude==TK_NO
  );

  lblWhereEnd = sqlite3VdbeMakeLabel(pParse);

  /* Fill in the context object */
  memset(&s, 0, sizeof(WindowCodeArg));
  s.pParse = pParse;
  s.pMWin = pMWin;
  s.pVdbe = v;
  s.regGosub = regGosub;
  s.addrGosub = addrGosub;
  s.current.csr = pMWin->iEphCsr;
  csrWrite = s.current.csr+1;
  s.start.csr = s.current.csr+2;
  s.end.csr = s.current.csr+3;

  /* Figure out when rows may be deleted from the ephemeral table. There
  ** are four options - they may never be deleted (eDelete==0), they may 
  ** be deleted as soon as they are no longer part of the window frame
  ** (eDelete==WINDOW_AGGINVERSE), they may be deleted as after the row 
  ** has been returned to the caller (WINDOW_RETURN_ROW), or they may
  ** be deleted after they enter the frame (WINDOW_AGGSTEP). */
  switch( pMWin->eStart ){
    case TK_FOLLOWING:
      if( pMWin->eFrmType!=TK_RANGE
       && windowExprGtZero(pParse, pMWin->pStart)
      ){
        s.eDelete = WINDOW_RETURN_ROW;
      }
      break;
    case TK_UNBOUNDED:
      if( windowCacheFrame(pMWin)==0 ){
        if( pMWin->eEnd==TK_PRECEDING ){
          if( pMWin->eFrmType!=TK_RANGE
           && windowExprGtZero(pParse, pMWin->pEnd)
          ){
            s.eDelete = WINDOW_AGGSTEP;
          }
        }else{
          s.eDelete = WINDOW_RETURN_ROW;
        }
      }
      break;
    default:
      s.eDelete = WINDOW_AGGINVERSE;
      break;
  }

  /* Allocate registers for the array of values from the sub-query, the
  ** samve values in record form, and the rowid used to insert said record
  ** into the ephemeral table.  */
  regNew = pParse->nMem+1;
  pParse->nMem += nInput;
  regRecord = ++pParse->nMem;
  regRowid = ++pParse->nMem;

  /* If the window frame contains an "<expr> PRECEDING" or "<expr> FOLLOWING"
  ** clause, allocate registers to store the results of evaluating each
  ** <expr>.  */
  if( pMWin->eStart==TK_PRECEDING || pMWin->eStart==TK_FOLLOWING ){
    regStart = ++pParse->nMem;
  }
  if( pMWin->eEnd==TK_PRECEDING || pMWin->eEnd==TK_FOLLOWING ){
    regEnd = ++pParse->nMem;
  }

  /* If this is not a "ROWS BETWEEN ..." frame, then allocate arrays of
  ** registers to store copies of the ORDER BY expressions (peer values) 
  ** for the main loop, and for each cursor (start, current and end). */
  if( pMWin->eFrmType!=TK_ROWS ){
    int nPeer = (pOrderBy ? pOrderBy->nExpr : 0);
    regNewPeer = regNew + pMWin->nBufferCol;
    if( pMWin->pPartition ) regNewPeer += pMWin->pPartition->nExpr;
    regPeer = pParse->nMem+1;       pParse->nMem += nPeer;
    s.start.reg = pParse->nMem+1;   pParse->nMem += nPeer;
    s.current.reg = pParse->nMem+1; pParse->nMem += nPeer;
    s.end.reg = pParse->nMem+1;     pParse->nMem += nPeer;
  }

  /* Load the column values for the row returned by the sub-select
  ** into an array of registers starting at regNew. Assemble them into
  ** a record in register regRecord. */
  for(iInput=0; iInput<nInput; iInput++){
    sqlite3VdbeAddOp3(v, OP_Column, csrInput, iInput, regNew+iInput);
  }
  sqlite3VdbeAddOp3(v, OP_MakeRecord, regNew, nInput, regRecord);

  /* An input row has just been read into an array of registers starting
  ** at regNew. If the window has a PARTITION clause, this block generates 
  ** VM code to check if the input row is the start of a new partition.
  ** If so, it does an OP_Gosub to an address to be filled in later. The
  ** address of the OP_Gosub is stored in local variable addrGosubFlush. */
  if( pMWin->pPartition ){
    int addr;
    ExprList *pPart = pMWin->pPartition;
    int nPart = pPart->nExpr;
    int regNewPart = regNew + pMWin->nBufferCol;
    KeyInfo *pKeyInfo = sqlite3KeyInfoFromExprList(pParse, pPart, 0, 0);

    regFlushPart = ++pParse->nMem;
    addr = sqlite3VdbeAddOp3(v, OP_Compare, regNewPart, pMWin->regPart, nPart);
    sqlite3VdbeAppendP4(v, (void*)pKeyInfo, P4_KEYINFO);
    sqlite3VdbeAddOp3(v, OP_Jump, addr+2, addr+4, addr+2);
    VdbeCoverageEqNe(v);
    addrGosubFlush = sqlite3VdbeAddOp1(v, OP_Gosub, regFlushPart);
    VdbeComment((v, "call flush_partition"));
    sqlite3VdbeAddOp3(v, OP_Copy, regNewPart, pMWin->regPart, nPart-1);
  }

  /* Insert the new row into the ephemeral table */
  sqlite3VdbeAddOp2(v, OP_NewRowid, csrWrite, regRowid);
  sqlite3VdbeAddOp3(v, OP_Insert, csrWrite, regRecord, regRowid);
  addrNe = sqlite3VdbeAddOp3(v, OP_Ne, pMWin->regOne, 0, regRowid);
  VdbeCoverageNeverNull(v);

  /* This block is run for the first row of each partition */
  s.regArg = windowInitAccum(pParse, pMWin);

  if( regStart ){
    sqlite3ExprCode(pParse, pMWin->pStart, regStart);
    windowCheckValue(pParse, regStart, 0 + (pMWin->eFrmType==TK_RANGE?3:0));
  }
  if( regEnd ){
    sqlite3ExprCode(pParse, pMWin->pEnd, regEnd);
    windowCheckValue(pParse, regEnd, 1 + (pMWin->eFrmType==TK_RANGE?3:0));
  }

  if( pMWin->eFrmType!=TK_RANGE && pMWin->eStart==pMWin->eEnd && regStart ){
    int op = ((pMWin->eStart==TK_FOLLOWING) ? OP_Ge : OP_Le);
    int addrGe = sqlite3VdbeAddOp3(v, op, regStart, 0, regEnd);
    VdbeCoverageNeverNullIf(v, op==OP_Ge); /* NeverNull because bound <expr> */
    VdbeCoverageNeverNullIf(v, op==OP_Le); /*   values previously checked */
    windowAggFinal(&s, 0);
    sqlite3VdbeAddOp2(v, OP_Rewind, s.current.csr, 1);
    VdbeCoverageNeverTaken(v);
    windowReturnOneRow(&s);
    sqlite3VdbeAddOp1(v, OP_ResetSorter, s.current.csr);
    sqlite3VdbeAddOp2(v, OP_Goto, 0, lblWhereEnd);
    sqlite3VdbeJumpHere(v, addrGe);
  }
  if( pMWin->eStart==TK_FOLLOWING && pMWin->eFrmType!=TK_RANGE && regEnd ){
    assert( pMWin->eEnd==TK_FOLLOWING );
    sqlite3VdbeAddOp3(v, OP_Subtract, regStart, regEnd, regStart);
  }

  if( pMWin->eStart!=TK_UNBOUNDED ){
    sqlite3VdbeAddOp2(v, OP_Rewind, s.start.csr, 1);
    VdbeCoverageNeverTaken(v);
  }
  sqlite3VdbeAddOp2(v, OP_Rewind, s.current.csr, 1);
  VdbeCoverageNeverTaken(v);
  sqlite3VdbeAddOp2(v, OP_Rewind, s.end.csr, 1);
  VdbeCoverageNeverTaken(v);
  if( regPeer && pOrderBy ){
    sqlite3VdbeAddOp3(v, OP_Copy, regNewPeer, regPeer, pOrderBy->nExpr-1);
    sqlite3VdbeAddOp3(v, OP_Copy, regPeer, s.start.reg, pOrderBy->nExpr-1);
    sqlite3VdbeAddOp3(v, OP_Copy, regPeer, s.current.reg, pOrderBy->nExpr-1);
    sqlite3VdbeAddOp3(v, OP_Copy, regPeer, s.end.reg, pOrderBy->nExpr-1);
  }

  sqlite3VdbeAddOp2(v, OP_Goto, 0, lblWhereEnd);

  sqlite3VdbeJumpHere(v, addrNe);

  /* Beginning of the block executed for the second and subsequent rows. */
  if( regPeer ){
    windowIfNewPeer(pParse, pOrderBy, regNewPeer, regPeer, lblWhereEnd);
  }
  if( pMWin->eStart==TK_FOLLOWING ){
    windowCodeOp(&s, WINDOW_AGGSTEP, 0, 0);
    if( pMWin->eEnd!=TK_UNBOUNDED ){
      if( pMWin->eFrmType==TK_RANGE ){
        int lbl = sqlite3VdbeMakeLabel(pParse);
        int addrNext = sqlite3VdbeCurrentAddr(v);
        windowCodeRangeTest(&s, OP_Ge, s.current.csr, regEnd, s.end.csr, lbl);
        windowCodeOp(&s, WINDOW_AGGINVERSE, regStart, 0);
        windowCodeOp(&s, WINDOW_RETURN_ROW, 0, 0);
        sqlite3VdbeAddOp2(v, OP_Goto, 0, addrNext);
        sqlite3VdbeResolveLabel(v, lbl);
      }else{
        windowCodeOp(&s, WINDOW_RETURN_ROW, regEnd, 0);
        windowCodeOp(&s, WINDOW_AGGINVERSE, regStart, 0);
      }
    }
  }else
  if( pMWin->eEnd==TK_PRECEDING ){
    int bRPS = (pMWin->eStart==TK_PRECEDING && pMWin->eFrmType==TK_RANGE);
    windowCodeOp(&s, WINDOW_AGGSTEP, regEnd, 0);
    if( bRPS ) windowCodeOp(&s, WINDOW_AGGINVERSE, regStart, 0);
    windowCodeOp(&s, WINDOW_RETURN_ROW, 0, 0);
    if( !bRPS ) windowCodeOp(&s, WINDOW_AGGINVERSE, regStart, 0);
  }else{
    int addr = 0;
    windowCodeOp(&s, WINDOW_AGGSTEP, 0, 0);
    if( pMWin->eEnd!=TK_UNBOUNDED ){
      if( pMWin->eFrmType==TK_RANGE ){
        int lbl = 0;
        addr = sqlite3VdbeCurrentAddr(v);
        if( regEnd ){
          lbl = sqlite3VdbeMakeLabel(pParse);
          windowCodeRangeTest(&s, OP_Ge, s.current.csr, regEnd, s.end.csr, lbl);
        }
        windowCodeOp(&s, WINDOW_RETURN_ROW, 0, 0);
        windowCodeOp(&s, WINDOW_AGGINVERSE, regStart, 0);
        if( regEnd ){
          sqlite3VdbeAddOp2(v, OP_Goto, 0, addr);
          sqlite3VdbeResolveLabel(v, lbl);
        }
      }else{
        if( regEnd ){
          addr = sqlite3VdbeAddOp3(v, OP_IfPos, regEnd, 0, 1);
          VdbeCoverage(v);
        }
        windowCodeOp(&s, WINDOW_RETURN_ROW, 0, 0);
        windowCodeOp(&s, WINDOW_AGGINVERSE, regStart, 0);
        if( regEnd ) sqlite3VdbeJumpHere(v, addr);
      }
    }
  }

  /* End of the main input loop */
  sqlite3VdbeResolveLabel(v, lblWhereEnd);
  sqlite3WhereEnd(pWInfo);

  /* Fall through */
  if( pMWin->pPartition ){
    addrInteger = sqlite3VdbeAddOp2(v, OP_Integer, 0, regFlushPart);
    sqlite3VdbeJumpHere(v, addrGosubFlush);
  }

  addrEmpty = sqlite3VdbeAddOp1(v, OP_Rewind, csrWrite);
  VdbeCoverage(v);
  if( pMWin->eEnd==TK_PRECEDING ){
    int bRPS = (pMWin->eStart==TK_PRECEDING && pMWin->eFrmType==TK_RANGE);
    windowCodeOp(&s, WINDOW_AGGSTEP, regEnd, 0);
    if( bRPS ) windowCodeOp(&s, WINDOW_AGGINVERSE, regStart, 0);
    windowCodeOp(&s, WINDOW_RETURN_ROW, 0, 0);
  }else if( pMWin->eStart==TK_FOLLOWING ){
    int addrStart;
    int addrBreak1;
    int addrBreak2;
    int addrBreak3;
    windowCodeOp(&s, WINDOW_AGGSTEP, 0, 0);
    if( pMWin->eFrmType==TK_RANGE ){
      addrStart = sqlite3VdbeCurrentAddr(v);
      addrBreak2 = windowCodeOp(&s, WINDOW_AGGINVERSE, regStart, 1);
      addrBreak1 = windowCodeOp(&s, WINDOW_RETURN_ROW, 0, 1);
    }else
    if( pMWin->eEnd==TK_UNBOUNDED ){
      addrStart = sqlite3VdbeCurrentAddr(v);
      addrBreak1 = windowCodeOp(&s, WINDOW_RETURN_ROW, regStart, 1);
      addrBreak2 = windowCodeOp(&s, WINDOW_AGGINVERSE, 0, 1);
    }else{
      assert( pMWin->eEnd==TK_FOLLOWING );
      addrStart = sqlite3VdbeCurrentAddr(v);
      addrBreak1 = windowCodeOp(&s, WINDOW_RETURN_ROW, regEnd, 1);
      addrBreak2 = windowCodeOp(&s, WINDOW_AGGINVERSE, regStart, 1);
    }
    sqlite3VdbeAddOp2(v, OP_Goto, 0, addrStart);
    sqlite3VdbeJumpHere(v, addrBreak2);
    addrStart = sqlite3VdbeCurrentAddr(v);
    addrBreak3 = windowCodeOp(&s, WINDOW_RETURN_ROW, 0, 1);
    sqlite3VdbeAddOp2(v, OP_Goto, 0, addrStart);
    sqlite3VdbeJumpHere(v, addrBreak1);
    sqlite3VdbeJumpHere(v, addrBreak3);
  }else{
    int addrBreak;
    int addrStart;
    windowCodeOp(&s, WINDOW_AGGSTEP, 0, 0);
    addrStart = sqlite3VdbeCurrentAddr(v);
    addrBreak = windowCodeOp(&s, WINDOW_RETURN_ROW, 0, 1);
    windowCodeOp(&s, WINDOW_AGGINVERSE, regStart, 0);
    sqlite3VdbeAddOp2(v, OP_Goto, 0, addrStart);
    sqlite3VdbeJumpHere(v, addrBreak);
  }
  sqlite3VdbeJumpHere(v, addrEmpty);

  sqlite3VdbeAddOp1(v, OP_ResetSorter, s.current.csr);
  if( pMWin->pPartition ){
    if( pMWin->regStartRowid ){
      sqlite3VdbeAddOp2(v, OP_Integer, 1, pMWin->regStartRowid);
      sqlite3VdbeAddOp2(v, OP_Integer, 0, pMWin->regEndRowid);
    }
    sqlite3VdbeChangeP1(v, addrInteger, sqlite3VdbeCurrentAddr(v));
    sqlite3VdbeAddOp1(v, OP_Return, regFlushPart);
  }
}

#endif /* SQLITE_OMIT_WINDOWFUNC */

/************** End of window.c **********************************************/
/************** Begin file parse.c *******************************************/
/*
** 2000-05-29
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Driver template for the LEMON parser generator.
**
** The "lemon" program processes an LALR(1) input grammar file, then uses
** this template to construct a parser.  The "lemon" program inserts text
** at each "%%" line.  Also, any "P-a-r-s-e" identifer prefix (without the
** interstitial "-" characters) contained in this template is changed into
** the value of the %name directive from the grammar.  Otherwise, the content
** of this template is copied straight through into the generate parser
** source file.
**
** The following is the concatenation of all %include directives from the
** input grammar file:
*/
/* #include <stdio.h> */
/* #include <assert.h> */
/************ Begin %include sections from the grammar ************************/

/* #include "sqliteInt.h" */

/*
** Disable all error recovery processing in the parser push-down
** automaton.
*/
#define YYNOERRORRECOVERY 1

/*
** Make yytestcase() the same as testcase()
*/
#define yytestcase(X) testcase(X)

/*
** Indicate that sqlite3ParserFree() will never be called with a null
** pointer.
*/
#define YYPARSEFREENEVERNULL 1

/*
** In the amalgamation, the parse.c file generated by lemon and the
** tokenize.c file are concatenated.  In that case, sqlite3RunParser()
** has access to the the size of the yyParser object and so the parser
** engine can be allocated from stack.  In that case, only the
** sqlite3ParserInit() and sqlite3ParserFinalize() routines are invoked
** and the sqlite3ParserAlloc() and sqlite3ParserFree() routines can be
** omitted.
*/
#ifdef SQLITE_AMALGAMATION
# define sqlite3Parser_ENGINEALWAYSONSTACK 1
#endif

/*
** Alternative datatype for the argument to the malloc() routine passed
** into sqlite3ParserAlloc().  The default is size_t.
*/
#define YYMALLOCARGTYPE  u64

/*
** An instance of the following structure describes the event of a
** TRIGGER.  "a" is the event type, one of TK_UPDATE, TK_INSERT,
** TK_DELETE, or TK_INSTEAD.  If the event is of the form
**
**      UPDATE ON (a,b,c)
**
** Then the "b" IdList records the list "a,b,c".
*/
struct TrigEvent { int a; IdList * b; };

struct FrameBound     { int eType; Expr *pExpr; };

/*
** Disable lookaside memory allocation for objects that might be
** shared across database connections.
*/
static void disableLookaside(Parse *pParse){
  pParse->disableLookaside++;
  pParse->db->lookaside.bDisable++;
}


  /*
  ** For a compound SELECT statement, make sure p->pPrior->pNext==p for
  ** all elements in the list.  And make sure list length does not exceed
  ** SQLITE_LIMIT_COMPOUND_SELECT.
  */
  static void parserDoubleLinkSelect(Parse *pParse, Select *p){
    assert( p!=0 );
    if( p->pPrior ){
      Select *pNext = 0, *pLoop;
      int mxSelect, cnt = 0;
      for(pLoop=p; pLoop; pNext=pLoop, pLoop=pLoop->pPrior, cnt++){
        pLoop->pNext = pNext;
        pLoop->selFlags |= SF_Compound;
      }
      if( (p->selFlags & SF_MultiValue)==0 && 
        (mxSelect = pParse->db->aLimit[SQLITE_LIMIT_COMPOUND_SELECT])>0 &&
        cnt>mxSelect
      ){
        sqlite3ErrorMsg(pParse, "too many terms in compound SELECT");
      }
    }
  }


  /* Construct a new Expr object from a single identifier.  Use the
  ** new Expr to populate pOut.  Set the span of pOut to be the identifier
  ** that created the expression.
  */
  static Expr *tokenExpr(Parse *pParse, int op, Token t){
    Expr *p = sqlite3DbMallocRawNN(pParse->db, sizeof(Expr)+t.n+1);
    if( p ){
      /* memset(p, 0, sizeof(Expr)); */
      p->op = (u8)op;
      p->affExpr = 0;
      p->flags = EP_Leaf;
      p->iAgg = -1;
      p->pLeft = p->pRight = 0;
      p->x.pList = 0;
      p->pAggInfo = 0;
      p->y.pTab = 0;
      p->op2 = 0;
      p->iTable = 0;
      p->iColumn = 0;
      p->u.zToken = (char*)&p[1];
      memcpy(p->u.zToken, t.z, t.n);
      p->u.zToken[t.n] = 0;
      if( sqlite3Isquote(p->u.zToken[0]) ){
        sqlite3DequoteExpr(p);
      }
#if SQLITE_MAX_EXPR_DEPTH>0
      p->nHeight = 1;
#endif  
      if( IN_RENAME_OBJECT ){
        return (Expr*)sqlite3RenameTokenMap(pParse, (void*)p, &t);
      }
    }
    return p;
  }


  /* A routine to convert a binary TK_IS or TK_ISNOT expression into a
  ** unary TK_ISNULL or TK_NOTNULL expression. */
  static void binaryToUnaryIfNull(Parse *pParse, Expr *pY, Expr *pA, int op){
    sqlite3 *db = pParse->db;
    if( pA && pY && pY->op==TK_NULL && !IN_RENAME_OBJECT ){
      pA->op = (u8)op;
      sqlite3ExprDelete(db, pA->pRight);
      pA->pRight = 0;
    }
  }

  /* Add a single new term to an ExprList that is used to store a
  ** list of identifiers.  Report an error if the ID list contains
  ** a COLLATE clause or an ASC or DESC keyword, except ignore the
  ** error while parsing a legacy schema.
  */
  static ExprList *parserAddExprIdListTerm(
    Parse *pParse,
    ExprList *pPrior,
    Token *pIdToken,
    int hasCollate,
    int sortOrder
  ){
    ExprList *p = sqlite3ExprListAppend(pParse, pPrior, 0);
    if( (hasCollate || sortOrder!=SQLITE_SO_UNDEFINED)
        && pParse->db->init.busy==0
    ){
      sqlite3ErrorMsg(pParse, "syntax error after column name \"%.*s\"",
                         pIdToken->n, pIdToken->z);
    }
    sqlite3ExprListSetName(pParse, p, pIdToken, 1);
    return p;
  }

#if TK_SPAN>255
# error too many tokens in the grammar
#endif
/**************** End of %include directives **********************************/
/* These constants specify the various numeric values for terminal symbols
** in a format understandable to "makeheaders".  This section is blank unless
** "lemon" is run with the "-m" command-line option.
***************** Begin makeheaders token definitions *************************/
/**************** End makeheaders token definitions ***************************/

/* The next sections is a series of control #defines.
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used to store the integer codes
**                       that represent terminal and non-terminal symbols.
**                       "unsigned char" is used if there are fewer than
**                       256 symbols.  Larger types otherwise.
**    YYNOCODE           is a number of type YYCODETYPE that is not used for
**                       any terminal or nonterminal symbol.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       (also known as: "terminal symbols") have fall-back
**                       values which should be used if the original symbol
**                       would not parse.  This permits keywords to sometimes
**                       be used as identifiers, for example.
**    YYACTIONTYPE       is the data type used for "action codes" - numbers
**                       that indicate what to do in response to the next
**                       token.
**    sqlite3ParserTOKENTYPE     is the data type used for minor type for terminal
**                       symbols.  Background: A "minor type" is a semantic
**                       value associated with a terminal or non-terminal
**                       symbols.  For example, for an "ID" terminal symbol,
**                       the minor type might be the name of the identifier.
**                       Each non-terminal can have a different minor type.
**                       Terminal symbols all have the same minor type, though.
**                       This macros defines the minor type for terminal 
**                       symbols.
**    YYMINORTYPE        is the data type used for all minor types.
**                       This is typically a union of many types, one of
**                       which is sqlite3ParserTOKENTYPE.  The entry in the union
**                       for terminal symbols is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    sqlite3ParserARG_SDECL     A static variable declaration for the %extra_argument
**    sqlite3ParserARG_PDECL     A parameter declaration for the %extra_argument
**    sqlite3ParserARG_PARAM     Code to pass %extra_argument as a subroutine parameter
**    sqlite3ParserARG_STORE     Code to store %extra_argument into yypParser
**    sqlite3ParserARG_FETCH     Code to extract %extra_argument from yypParser
**    sqlite3ParserCTX_*         As sqlite3ParserARG_ except for %extra_context
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYNTOKEN           Number of terminal symbols
**    YY_MAX_SHIFT       Maximum value for shift actions
**    YY_MIN_SHIFTREDUCE Minimum value for shift-reduce actions
**    YY_MAX_SHIFTREDUCE Maximum value for shift-reduce actions
**    YY_ERROR_ACTION    The yy_action[] code for syntax error
**    YY_ACCEPT_ACTION   The yy_action[] code for accept
**    YY_NO_ACTION       The yy_action[] code for no-op
**    YY_MIN_REDUCE      Minimum value for reduce actions
**    YY_MAX_REDUCE      Maximum value for reduce actions
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/************* Begin control #defines *****************************************/
#define YYCODETYPE unsigned short int
#define YYNOCODE 307
#define YYACTIONTYPE unsigned short int
#define YYWILDCARD 98
#define sqlite3ParserTOKENTYPE Token
typedef union {
  int yyinit;
  sqlite3ParserTOKENTYPE yy0;
  const char* yy8;
  Select* yy25;
  int yy32;
  Expr* yy46;
  struct FrameBound yy57;
  u8 yy118;
  ExprList* yy138;
  Upsert* yy288;
  With* yy297;
  IdList* yy406;
  Window* yy455;
  struct {int value; int mask;} yy495;
  TriggerStep* yy527;
  struct TrigEvent yy572;
  SrcList* yy609;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 100
#endif
#define sqlite3ParserARG_SDECL
#define sqlite3ParserARG_PDECL
#define sqlite3ParserARG_PARAM
#define sqlite3ParserARG_FETCH
#define sqlite3ParserARG_STORE
#define sqlite3ParserCTX_SDECL Parse *pParse;
#define sqlite3ParserCTX_PDECL ,Parse *pParse
#define sqlite3ParserCTX_PARAM ,pParse
#define sqlite3ParserCTX_FETCH Parse *pParse=yypParser->pParse;
#define sqlite3ParserCTX_STORE yypParser->pParse=pParse;
#define YYFALLBACK 1
#define YYNSTATE             543
#define YYNRULE              381
#define YYNTOKEN             179
#define YY_MAX_SHIFT         542
#define YY_MIN_SHIFTREDUCE   790
#define YY_MAX_SHIFTREDUCE   1170
#define YY_ERROR_ACTION      1171
#define YY_ACCEPT_ACTION     1172
#define YY_NO_ACTION         1173
#define YY_MIN_REDUCE        1174
#define YY_MAX_REDUCE        1554
/************* End control #defines *******************************************/
#define YY_NLOOKAHEAD ((int)(sizeof(yy_lookahead)/sizeof(yy_lookahead[0])))

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N <= YY_MAX_SHIFT             Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   N between YY_MIN_SHIFTREDUCE       Shift to an arbitrary state then
**     and YY_MAX_SHIFTREDUCE           reduce by rule N-YY_MIN_SHIFTREDUCE.
**
**   N == YY_ERROR_ACTION               A syntax error has occurred.
**
**   N == YY_ACCEPT_ACTION              The parser accepts its input.
**
**   N == YY_NO_ACTION                  No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
**   N between YY_MIN_REDUCE            Reduce by rule N-YY_MIN_REDUCE
**     and YY_MAX_REDUCE
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as either:
**
**    (A)   N = yy_action[ yy_shift_ofst[S] + X ]
**    (B)   N = yy_default[S]
**
** The (A) formula is preferred.  The B formula is used instead if
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X.
**
** The formulas above are for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
**
*********** Begin parsing tables **********************************************/
#define YY_ACTTAB_COUNT (1913)
static const YYACTIONTYPE yy_action[] = {
 /*     0 */   537,  339,  537, 1241, 1220,  537,   12,  537,  112,  109,
 /*    10 */   209,  537, 1241,  537, 1205,  462,  112,  109,  209,  386,
 /*    20 */   338,  462,   42,   42,   42,   42,  445,   42,   42,   70,
 /*    30 */    70,  922, 1208,   70,   70,   70,   70, 1443,  403,  923,
 /*    40 */   531,  531,  531,  119,  120,  110, 1148, 1148,  991,  994,
 /*    50 */   984,  984,  117,  117,  118,  118,  118,  118,  425,  386,
 /*    60 */  1498,  542,    2, 1176, 1442,  519,  141, 1518,  289,  519,
 /*    70 */   134,  519,   95,  259,  495, 1215,  189, 1254,  518,  494,
 /*    80 */   484,  437,  296,  119,  120,  110, 1148, 1148,  991,  994,
 /*    90 */   984,  984,  117,  117,  118,  118,  118,  118,  270,  116,
 /*   100 */   116,  116,  116,  115,  115,  114,  114,  114,  113,  418,
 /*   110 */   264,  264,  264,  264,  423, 1479,  352, 1481,  123,  351,
 /*   120 */  1479,  508, 1094,  534, 1034,  534, 1099,  386, 1099,  239,
 /*   130 */   206,  112,  109,  209,   96, 1094,  376,  219, 1094,  116,
 /*   140 */   116,  116,  116,  115,  115,  114,  114,  114,  113,  418,
 /*   150 */   480,  119,  120,  110, 1148, 1148,  991,  994,  984,  984,
 /*   160 */   117,  117,  118,  118,  118,  118,  353,  422, 1407,  264,
 /*   170 */   264,  114,  114,  114,  113,  418,  883,  121,  416,  416,
 /*   180 */   416,  882,  534,  116,  116,  116,  116,  115,  115,  114,
 /*   190 */   114,  114,  113,  418,  212,  415,  414,  386,  443,  383,
 /*   200 */   382,  118,  118,  118,  118,  111,  177,  116,  116,  116,
 /*   210 */   116,  115,  115,  114,  114,  114,  113,  418,  112,  109,
 /*   220 */   209,  119,  120,  110, 1148, 1148,  991,  994,  984,  984,
 /*   230 */   117,  117,  118,  118,  118,  118,  386,  438,  312, 1163,
 /*   240 */  1155,   80, 1155, 1127,  514,   79,  116,  116,  116,  116,
 /*   250 */   115,  115,  114,  114,  114,  113,  418,  514,  428,  418,
 /*   260 */   119,  120,  110, 1148, 1148,  991,  994,  984,  984,  117,
 /*   270 */   117,  118,  118,  118,  118,  428,  427,  116,  116,  116,
 /*   280 */   116,  115,  115,  114,  114,  114,  113,  418,  115,  115,
 /*   290 */   114,  114,  114,  113,  418, 1127, 1127, 1128, 1129, 1094,
 /*   300 */   258,  258,  192,  386,  408,  371, 1168,  326,  118,  118,
 /*   310 */   118,  118, 1094,  534,  374, 1094,  116,  116,  116,  116,
 /*   320 */   115,  115,  114,  114,  114,  113,  418,  119,  120,  110,
 /*   330 */  1148, 1148,  991,  994,  984,  984,  117,  117,  118,  118,
 /*   340 */   118,  118,  386,  354,  445,  428,  829,  238, 1127, 1128,
 /*   350 */  1129,  515, 1466,  116,  116,  116,  116,  115,  115,  114,
 /*   360 */   114,  114,  113,  418, 1127, 1467,  119,  120,  110, 1148,
 /*   370 */  1148,  991,  994,  984,  984,  117,  117,  118,  118,  118,
 /*   380 */   118, 1169,   82,  116,  116,  116,  116,  115,  115,  114,
 /*   390 */   114,  114,  113,  418,  405,  112,  109,  209,  161,  445,
 /*   400 */   250,  267,  336,  478,  331,  477,  236,  951, 1127,  386,
 /*   410 */   888, 1521,  329,  822,  852,  162,  274, 1127, 1128, 1129,
 /*   420 */   338,  169,  116,  116,  116,  116,  115,  115,  114,  114,
 /*   430 */   114,  113,  418,  119,  120,  110, 1148, 1148,  991,  994,
 /*   440 */   984,  984,  117,  117,  118,  118,  118,  118,  386,  438,
 /*   450 */   312, 1502, 1112, 1176,  161,  288,  528,  311,  289,  883,
 /*   460 */   134, 1127, 1128, 1129,  882,  537,  143, 1254,  288,  528,
 /*   470 */   297,  275,  119,  120,  110, 1148, 1148,  991,  994,  984,
 /*   480 */   984,  117,  117,  118,  118,  118,  118,   70,   70,  116,
 /*   490 */   116,  116,  116,  115,  115,  114,  114,  114,  113,  418,
 /*   500 */   264,  264,   12,  264,  264,  395, 1127,  483, 1473, 1094,
 /*   510 */   204,  482,    6,  534, 1258,  386,  534, 1474,  825,  972,
 /*   520 */   504,    6, 1094,  500,   95, 1094,  534,  219,  116,  116,
 /*   530 */   116,  116,  115,  115,  114,  114,  114,  113,  418,  119,
 /*   540 */   120,  110, 1148, 1148,  991,  994,  984,  984,  117,  117,
 /*   550 */   118,  118,  118,  118,  386, 1339,  971,  422,  956, 1127,
 /*   560 */  1128, 1129,  231,  512, 1473,  475,  472,  471,    6,  113,
 /*   570 */   418,  825,  962,  298,  503,  470,  961,  452,  119,  120,
 /*   580 */   110, 1148, 1148,  991,  994,  984,  984,  117,  117,  118,
 /*   590 */   118,  118,  118,  395,  537,  116,  116,  116,  116,  115,
 /*   600 */   115,  114,  114,  114,  113,  418,  202,  961,  961,  963,
 /*   610 */   231,  971, 1127,  475,  472,  471,   13,   13,  951, 1127,
 /*   620 */   834,  386, 1207,  470,  399,  183,  447,  962,  462,  162,
 /*   630 */   397,  961, 1246, 1246,  116,  116,  116,  116,  115,  115,
 /*   640 */   114,  114,  114,  113,  418,  119,  120,  110, 1148, 1148,
 /*   650 */   991,  994,  984,  984,  117,  117,  118,  118,  118,  118,
 /*   660 */   386,  271,  961,  961,  963, 1127, 1128, 1129,  311,  433,
 /*   670 */   299, 1406, 1127, 1128, 1129,  178, 1471,  138,  162,   32,
 /*   680 */     6, 1127,  288,  528,  119,  120,  110, 1148, 1148,  991,
 /*   690 */   994,  984,  984,  117,  117,  118,  118,  118,  118,  909,
 /*   700 */   390,  116,  116,  116,  116,  115,  115,  114,  114,  114,
 /*   710 */   113,  418, 1127,  429,  817,  537, 1127,  265,  265,  981,
 /*   720 */   981,  992,  995,  324, 1055,   93,  520,    5,  338,  537,
 /*   730 */   534,  288,  528, 1522, 1127, 1128, 1129,   70,   70, 1056,
 /*   740 */   116,  116,  116,  116,  115,  115,  114,  114,  114,  113,
 /*   750 */   418,   70,   70, 1495, 1057,  537,   98, 1244, 1244,  264,
 /*   760 */   264,  908,  371, 1076, 1127, 1127, 1128, 1129,  817, 1127,
 /*   770 */  1128, 1129,  534,  519,  140,  863,  386,   13,   13,  456,
 /*   780 */   192,  193,  521,  453,  319,  864,  322,  284,  365,  430,
 /*   790 */   985,  402,  379, 1077, 1548,  101,  386, 1548,    3,  395,
 /*   800 */   119,  120,  110, 1148, 1148,  991,  994,  984,  984,  117,
 /*   810 */   117,  118,  118,  118,  118,  386,  451, 1127, 1128, 1129,
 /*   820 */   119,  120,  110, 1148, 1148,  991,  994,  984,  984,  117,
 /*   830 */   117,  118,  118,  118,  118, 1127, 1354, 1412, 1169,  119,
 /*   840 */   108,  110, 1148, 1148,  991,  994,  984,  984,  117,  117,
 /*   850 */   118,  118,  118,  118, 1412, 1414,  116,  116,  116,  116,
 /*   860 */   115,  115,  114,  114,  114,  113,  418,  272,  535, 1075,
 /*   870 */   877,  877,  337, 1492,  309,  462,  116,  116,  116,  116,
 /*   880 */   115,  115,  114,  114,  114,  113,  418,  537, 1127, 1128,
 /*   890 */  1129,  537,  360,  537,  356,  116,  116,  116,  116,  115,
 /*   900 */   115,  114,  114,  114,  113,  418,  386,  264,  264,   13,
 /*   910 */    13,  273, 1127,   13,   13,   13,   13,  304, 1253,  386,
 /*   920 */   534, 1077, 1549,  404, 1412, 1549,  496,  277,  451,  186,
 /*   930 */  1252,  120,  110, 1148, 1148,  991,  994,  984,  984,  117,
 /*   940 */   117,  118,  118,  118,  118,  110, 1148, 1148,  991,  994,
 /*   950 */   984,  984,  117,  117,  118,  118,  118,  118,  105,  529,
 /*   960 */   537,    4, 1339,  264,  264, 1127, 1128, 1129, 1039, 1039,
 /*   970 */   459,  795,  796,  797,  536,  532,  534,  242,  301,  807,
 /*   980 */   303,  462,   69,   69,  451, 1353,  116,  116,  116,  116,
 /*   990 */   115,  115,  114,  114,  114,  113,  418, 1075,  419,  116,
 /*  1000 */   116,  116,  116,  115,  115,  114,  114,  114,  113,  418,
 /*  1010 */   526,  537, 1146,  192,  350,  105,  529,  537,    4,  497,
 /*  1020 */   162,  337, 1492,  310, 1249,  385, 1550,  372,    9,  462,
 /*  1030 */   242,  400,  532,   13,   13,  499,  971,  843,  436,   70,
 /*  1040 */    70,  359,  103,  103,    8,  339,  278,  187,  278,  104,
 /*  1050 */  1127,  419,  539,  538, 1339,  419,  961,  302, 1339, 1172,
 /*  1060 */     1,    1,  542,    2, 1176, 1146, 1146,  526,  476,  289,
 /*  1070 */    30,  134,  317,  288,  528,  285,  844, 1014, 1254,  276,
 /*  1080 */  1472,  506,  410, 1194,    6,  207,  505,  961,  961,  963,
 /*  1090 */   964,   27,  449,  971,  415,  414,  234,  233,  232,  103,
 /*  1100 */   103,   31, 1152, 1127, 1128, 1129,  104, 1154,  419,  539,
 /*  1110 */   538,  264,  264,  961, 1399, 1153,  264,  264, 1470, 1146,
 /*  1120 */   537,  216,    6,  401,  534, 1197,  392,  458,  406,  534,
 /*  1130 */   537,  485,  358,  537,  261,  537, 1339,  907,  219, 1155,
 /*  1140 */   467, 1155,   50,   50,  961,  961,  963,  964,   27, 1497,
 /*  1150 */  1116,  421,   70,   70,  268,   70,   70,   13,   13,  369,
 /*  1160 */   369,  368,  253,  366,  264,  264,  804,  235,  422,  105,
 /*  1170 */   529,  516,    4,  287,  487,  510,  493,  534,  486,  213,
 /*  1180 */  1055,  294,  490,  384, 1127,  450,  532,  338,  413,  293,
 /*  1190 */   522,  417,  335, 1036,  509, 1056,  107, 1036,   16,   16,
 /*  1200 */  1469, 1094,  334, 1105,    6,  411, 1145,  264,  264,  419,
 /*  1210 */  1057,  102,  511,  100, 1094,  264,  264, 1094,  922,  215,
 /*  1220 */   534,  526,  907,  264,  264,  208,  923,  154,  534,  457,
 /*  1230 */   156,  525,  391,  142,  218,  506,  534, 1127, 1128, 1129,
 /*  1240 */   507,  139, 1131,   38,  214,  530,  392,  971,  329, 1454,
 /*  1250 */   907, 1105,  537,  103,  103,  105,  529,  537,    4,  537,
 /*  1260 */   104,  424,  419,  539,  538,  537,  502,  961,  517,  537,
 /*  1270 */  1072,  537,  532,  373,   54,   54,  288,  528,  387,   55,
 /*  1280 */    55,   15,   15,  288,  528,   17,  136,   44,   44, 1451,
 /*  1290 */   537,   56,   56,   57,   57,  419, 1131,  291,  961,  961,
 /*  1300 */   963,  964,   27,  393,  163,  537,  426,  526,  263,  206,
 /*  1310 */   208,  517,   58,   58,  235,  440,  842,  841,  197,  105,
 /*  1320 */   529,  506,    4, 1033,  439, 1033,  505,   59,   59,  308,
 /*  1330 */   849,  850,   95,  971,  537,  907,  532,  948,  832,  103,
 /*  1340 */   103,  105,  529,  537,    4, 1021,  104,  537,  419,  539,
 /*  1350 */   538, 1116,  421,  961,  537,  268,   60,   60,  532,  419,
 /*  1360 */   369,  369,  368,  253,  366,   61,   61,  804,  965,   45,
 /*  1370 */    45,  526,  537, 1032, 1277, 1032,   46,   46,  537,  391,
 /*  1380 */   213,  419,  294,  266,  961,  961,  963,  964,   27,  292,
 /*  1390 */   293,  295,  832,  526,   48,   48, 1290,  971, 1289, 1021,
 /*  1400 */    49,   49,  432,  103,  103,  887,  953,  537, 1457,  241,
 /*  1410 */   104,  305,  419,  539,  538,  925,  926,  961,  444,  971,
 /*  1420 */   215,  241,  965, 1224,  537,  103,  103, 1431,  154,   62,
 /*  1430 */    62,  156,  104, 1430,  419,  539,  538,   97,  529,  961,
 /*  1440 */     4,  537,  454,  537,  314,  214,   63,   63,  961,  961,
 /*  1450 */   963,  964,   27,  537,  532,  446, 1286,  318,  241,  537,
 /*  1460 */   321,  323,  325,   64,   64,   14,   14, 1237,  537, 1223,
 /*  1470 */   961,  961,  963,  964,   27,   65,   65,  419,  537,  387,
 /*  1480 */   537,  125,  125,  537,  288,  528,  537, 1486,  537,  526,
 /*  1490 */    66,   66,  313,  524,  537,   95,  468, 1221, 1511,  237,
 /*  1500 */    51,   51,   67,   67,  330,   68,   68,  426,   52,   52,
 /*  1510 */   149,  149, 1222,  340,  341,  971,  150,  150, 1298,  463,
 /*  1520 */   327,  103,  103,   95,  537, 1338, 1273,  537,  104,  537,
 /*  1530 */   419,  539,  538, 1284,  537,  961,  268,  283,  523, 1344,
 /*  1540 */  1204,  369,  369,  368,  253,  366,   75,   75,  804,   53,
 /*  1550 */    53,   71,   71,  537, 1196,  537,  126,  126,  537, 1017,
 /*  1560 */   537,  213,  237,  294,  537, 1185,  961,  961,  963,  964,
 /*  1570 */    27,  293,  537, 1184,  537,   72,   72,  127,  127, 1186,
 /*  1580 */   128,  128,  124,  124, 1505,  537,  148,  148,  537,  256,
 /*  1590 */   195,  537, 1270,  537,  147,  147,  132,  132,  537,   11,
 /*  1600 */   537,  215,  537,  199,  343,  345,  347,  131,  131,  154,
 /*  1610 */   129,  129,  156,  130,  130,   74,   74,  537,  370, 1323,
 /*  1620 */    76,   76,   73,   73,   43,   43,  214,  431,  211, 1331,
 /*  1630 */   300,  916,  880,  815,  241,  107,  137,  307,  881,   47,
 /*  1640 */    47,  107,  473,  378,  203,  448,  333, 1403, 1220, 1402,
 /*  1650 */   349,  190,  527,  191,  363,  198, 1508, 1163,  245,  165,
 /*  1660 */   387, 1450, 1448, 1160,   78,  288,  528, 1408,   81,  394,
 /*  1670 */    82,  442,  175,  159,  167,   93, 1328,   35, 1320,  434,
 /*  1680 */   170,  171,  172,  173,  435,  466,  221,  375,  426,  377,
 /*  1690 */  1334,  179,  455,  441, 1397,  225,   87,   36,  461, 1419,
 /*  1700 */   316,  257,  227,  184,  320,  464,  228,  479, 1187,  229,
 /*  1710 */   380, 1240, 1239,  407, 1238, 1212,  834,  332, 1231,  381,
 /*  1720 */   409, 1211,  204, 1210, 1491,  498, 1520, 1281,   92,  281,
 /*  1730 */  1230,  489,  282,  492,  342,  243, 1282,  344,  244, 1280,
 /*  1740 */   346,  412, 1279, 1477,  348,  122, 1476,  517,   10,  357,
 /*  1750 */   286, 1305, 1304,   99, 1383,   94,  501,  251, 1193,   34,
 /*  1760 */  1263,  355,  540,  194, 1262,  361,  362, 1122,  252,  254,
 /*  1770 */   255,  388,  541, 1182, 1177,  151, 1435,  389, 1436, 1434,
 /*  1780 */  1433,  791,  152,  135,  279,  200,  201,  420,  196,   77,
 /*  1790 */   153,  290,  269,  210, 1031,  133, 1029,  945,  166,  155,
 /*  1800 */   217,  168,  866,  306,  220, 1045,  174,  949,  157,  396,
 /*  1810 */    83,  398,  176,   84,   85,  164,   86,  158, 1048,  222,
 /*  1820 */   223, 1044,  144,   18,  224,  315, 1037,  180,  241,  460,
 /*  1830 */  1157,  226,  181,   37,  806,  465,  334,  230,  328,  469,
 /*  1840 */   182,   88,  474,   19,   20,  160,   89,  280,  145,   90,
 /*  1850 */   481,  845, 1110,  146,  997,  205, 1080,   39,   91,   40,
 /*  1860 */   488, 1081,  915,  491,  260,  262,  185,  910,  240,  107,
 /*  1870 */  1100, 1096, 1098, 1104,   21, 1084,   33,  513,  247,   22,
 /*  1880 */    23,   24, 1103,   25,  188,   95, 1012,  998,  996,   26,
 /*  1890 */  1000, 1054,    7, 1053, 1001,  246,   28,   41,  533,  966,
 /*  1900 */   816,  106,   29,  367,  248,  249, 1513, 1512,  364, 1117,
 /*  1910 */  1173, 1173,  876,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */   187,  187,  187,  216,  217,  187,  206,  187,  264,  265,
 /*    10 */   266,  187,  225,  187,  209,  187,  264,  265,  266,   19,
 /*    20 */   187,  187,  209,  210,  209,  210,  187,  209,  210,  209,
 /*    30 */   210,   31,  209,  209,  210,  209,  210,  285,  224,   39,
 /*    40 */   203,  204,  205,   43,   44,   45,   46,   47,   48,   49,
 /*    50 */    50,   51,   52,   53,   54,   55,   56,   57,  230,   19,
 /*    60 */   181,  182,  183,  184,  230,  245,  233,  208,  189,  245,
 /*    70 */   191,  245,   26,  206,  254,  216,  276,  198,  254,  198,
 /*    80 */   254,  281,  187,   43,   44,   45,   46,   47,   48,   49,
 /*    90 */    50,   51,   52,   53,   54,   55,   56,   57,  259,   99,
 /*   100 */   100,  101,  102,  103,  104,  105,  106,  107,  108,  109,
 /*   110 */   231,  232,  231,  232,  286,  302,  303,  302,   22,  304,
 /*   120 */   302,  303,   76,  244,   11,  244,   86,   19,   88,  248,
 /*   130 */   249,  264,  265,  266,   26,   89,  198,  258,   92,   99,
 /*   140 */   100,  101,  102,  103,  104,  105,  106,  107,  108,  109,
 /*   150 */   105,   43,   44,   45,   46,   47,   48,   49,   50,   51,
 /*   160 */    52,   53,   54,   55,   56,   57,  212,  288,  273,  231,
 /*   170 */   232,  105,  106,  107,  108,  109,  131,   69,  203,  204,
 /*   180 */   205,  136,  244,   99,  100,  101,  102,  103,  104,  105,
 /*   190 */   106,  107,  108,  109,   15,  103,  104,   19,  260,  103,
 /*   200 */   104,   54,   55,   56,   57,   58,   22,   99,  100,  101,
 /*   210 */   102,  103,  104,  105,  106,  107,  108,  109,  264,  265,
 /*   220 */   266,   43,   44,   45,   46,   47,   48,   49,   50,   51,
 /*   230 */    52,   53,   54,   55,   56,   57,   19,  124,  125,   60,
 /*   240 */   148,   24,  150,   59,  187,   67,   99,  100,  101,  102,
 /*   250 */   103,  104,  105,  106,  107,  108,  109,  187,  187,  109,
 /*   260 */    43,   44,   45,   46,   47,   48,   49,   50,   51,   52,
 /*   270 */    53,   54,   55,   56,   57,  204,  205,   99,  100,  101,
 /*   280 */   102,  103,  104,  105,  106,  107,  108,  109,  103,  104,
 /*   290 */   105,  106,  107,  108,  109,   59,  112,  113,  114,   76,
 /*   300 */   231,  232,  187,   19,   19,   22,   23,   23,   54,   55,
 /*   310 */    56,   57,   89,  244,  199,   92,   99,  100,  101,  102,
 /*   320 */   103,  104,  105,  106,  107,  108,  109,   43,   44,   45,
 /*   330 */    46,   47,   48,   49,   50,   51,   52,   53,   54,   55,
 /*   340 */    56,   57,   19,  212,  187,  274,   23,   26,  112,  113,
 /*   350 */   114,  294,  295,   99,  100,  101,  102,  103,  104,  105,
 /*   360 */   106,  107,  108,  109,   59,  295,   43,   44,   45,   46,
 /*   370 */    47,   48,   49,   50,   51,   52,   53,   54,   55,   56,
 /*   380 */    57,   98,  146,   99,  100,  101,  102,  103,  104,  105,
 /*   390 */   106,  107,  108,  109,  109,  264,  265,  266,  187,  187,
 /*   400 */   115,  116,  117,  118,  119,  120,  121,   73,   59,   19,
 /*   410 */   105,   23,  127,   23,   26,   81,  259,  112,  113,  114,
 /*   420 */   187,   72,   99,  100,  101,  102,  103,  104,  105,  106,
 /*   430 */   107,  108,  109,   43,   44,   45,   46,   47,   48,   49,
 /*   440 */    50,   51,   52,   53,   54,   55,   56,   57,   19,  124,
 /*   450 */   125,  182,   23,  184,  187,  134,  135,  123,  189,  131,
 /*   460 */   191,  112,  113,  114,  136,  187,  233,  198,  134,  135,
 /*   470 */   198,  259,   43,   44,   45,   46,   47,   48,   49,   50,
 /*   480 */    51,   52,   53,   54,   55,   56,   57,  209,  210,   99,
 /*   490 */   100,  101,  102,  103,  104,  105,  106,  107,  108,  109,
 /*   500 */   231,  232,  206,  231,  232,  187,   59,  296,  297,   76,
 /*   510 */   160,  161,  301,  244,  232,   19,  244,  297,   59,   23,
 /*   520 */    87,  301,   89,  245,   26,   92,  244,  258,   99,  100,
 /*   530 */   101,  102,  103,  104,  105,  106,  107,  108,  109,   43,
 /*   540 */    44,   45,   46,   47,   48,   49,   50,   51,   52,   53,
 /*   550 */    54,   55,   56,   57,   19,  187,   97,  288,   23,  112,
 /*   560 */   113,  114,  115,  296,  297,  118,  119,  120,  301,  108,
 /*   570 */   109,  112,  113,  255,  141,  128,  117,  281,   43,   44,
 /*   580 */    45,   46,   47,   48,   49,   50,   51,   52,   53,   54,
 /*   590 */    55,   56,   57,  187,  187,   99,  100,  101,  102,  103,
 /*   600 */   104,  105,  106,  107,  108,  109,   26,  148,  149,  150,
 /*   610 */   115,   97,   59,  118,  119,  120,  209,  210,   73,   59,
 /*   620 */   122,   19,  209,  128,  256,   72,  187,  113,  187,   81,
 /*   630 */   223,  117,  227,  228,   99,  100,  101,  102,  103,  104,
 /*   640 */   105,  106,  107,  108,  109,   43,   44,   45,   46,   47,
 /*   650 */    48,   49,   50,   51,   52,   53,   54,   55,   56,   57,
 /*   660 */    19,  255,  148,  149,  150,  112,  113,  114,  123,  124,
 /*   670 */   125,  230,  112,  113,  114,   22,  297,   22,   81,   22,
 /*   680 */   301,   59,  134,  135,   43,   44,   45,   46,   47,   48,
 /*   690 */    49,   50,   51,   52,   53,   54,   55,   56,   57,  139,
 /*   700 */   192,   99,  100,  101,  102,  103,  104,  105,  106,  107,
 /*   710 */   108,  109,   59,  116,   59,  187,   59,  231,  232,   46,
 /*   720 */    47,   48,   49,   16,   12,  145,  198,   22,  187,  187,
 /*   730 */   244,  134,  135,  222,  112,  113,  114,  209,  210,   27,
 /*   740 */    99,  100,  101,  102,  103,  104,  105,  106,  107,  108,
 /*   750 */   109,  209,  210,  187,   42,  187,  154,  227,  228,  231,
 /*   760 */   232,  139,   22,   23,   59,  112,  113,  114,  113,  112,
 /*   770 */   113,  114,  244,  245,  233,   63,   19,  209,  210,  271,
 /*   780 */   187,   24,  254,  275,   77,   73,   79,  245,  195,  260,
 /*   790 */   117,  223,  199,   22,   23,  154,   19,   26,   22,  187,
 /*   800 */    43,   44,   45,   46,   47,   48,   49,   50,   51,   52,
 /*   810 */    53,   54,   55,   56,   57,   19,  187,  112,  113,  114,
 /*   820 */    43,   44,   45,   46,   47,   48,   49,   50,   51,   52,
 /*   830 */    53,   54,   55,   56,   57,   59,  263,  187,   98,   43,
 /*   840 */    44,   45,   46,   47,   48,   49,   50,   51,   52,   53,
 /*   850 */    54,   55,   56,   57,  204,  205,   99,  100,  101,  102,
 /*   860 */   103,  104,  105,  106,  107,  108,  109,  255,  130,   98,
 /*   870 */   132,  133,  299,  300,  198,  187,   99,  100,  101,  102,
 /*   880 */   103,  104,  105,  106,  107,  108,  109,  187,  112,  113,
 /*   890 */   114,  187,  241,  187,  243,   99,  100,  101,  102,  103,
 /*   900 */   104,  105,  106,  107,  108,  109,   19,  231,  232,  209,
 /*   910 */   210,  282,   59,  209,  210,  209,  210,   16,  230,   19,
 /*   920 */   244,   22,   23,  223,  274,   26,   19,  223,  187,  223,
 /*   930 */   198,   44,   45,   46,   47,   48,   49,   50,   51,   52,
 /*   940 */    53,   54,   55,   56,   57,   45,   46,   47,   48,   49,
 /*   950 */    50,   51,   52,   53,   54,   55,   56,   57,   19,   20,
 /*   960 */   187,   22,  187,  231,  232,  112,  113,  114,  123,  124,
 /*   970 */   125,    7,    8,    9,  187,   36,  244,   24,   77,   21,
 /*   980 */    79,  187,  209,  210,  187,  263,   99,  100,  101,  102,
 /*   990 */   103,  104,  105,  106,  107,  108,  109,   98,   59,   99,
 /*  1000 */   100,  101,  102,  103,  104,  105,  106,  107,  108,  109,
 /*  1010 */    71,  187,   59,  187,  187,   19,   20,  187,   22,  112,
 /*  1020 */    81,  299,  300,  282,  230,  199,  291,  292,   22,  187,
 /*  1030 */    24,  256,   36,  209,  210,  187,   97,   35,   80,  209,
 /*  1040 */   210,  268,  103,  104,   48,  187,  220,  223,  222,  110,
 /*  1050 */    59,  112,  113,  114,  187,   59,  117,  156,  187,  179,
 /*  1060 */   180,  181,  182,  183,  184,   59,  113,   71,   66,  189,
 /*  1070 */    22,  191,  230,  134,  135,  245,   74,  119,  198,  282,
 /*  1080 */   297,   85,  224,  198,  301,  187,   90,  148,  149,  150,
 /*  1090 */   151,  152,   19,   97,  103,  104,  123,  124,  125,  103,
 /*  1100 */   104,   53,  111,  112,  113,  114,  110,  116,  112,  113,
 /*  1110 */   114,  231,  232,  117,  156,  124,  231,  232,  297,  113,
 /*  1120 */   187,   24,  301,  256,  244,  201,  202,  256,  126,  244,
 /*  1130 */   187,  198,  187,  187,   23,  187,  187,   26,  258,  148,
 /*  1140 */    19,  150,  209,  210,  148,  149,  150,  151,  152,    0,
 /*  1150 */     1,    2,  209,  210,    5,  209,  210,  209,  210,   10,
 /*  1160 */    11,   12,   13,   14,  231,  232,   17,   46,  288,   19,
 /*  1170 */    20,  223,   22,  236,  198,   66,  187,  244,  245,   30,
 /*  1180 */    12,   32,  198,  246,   59,  112,   36,  187,  245,   40,
 /*  1190 */   198,  245,  117,   29,   85,   27,   26,   33,  209,  210,
 /*  1200 */   297,   76,  127,   94,  301,  256,   26,  231,  232,   59,
 /*  1210 */    42,  153,   87,  155,   89,  231,  232,   92,   31,   70,
 /*  1220 */   244,   71,   26,  231,  232,  114,   39,   78,  244,   65,
 /*  1230 */    81,   63,  111,  233,  137,   85,  244,  112,  113,  114,
 /*  1240 */    90,   22,   59,   24,   95,  201,  202,   97,  127,  187,
 /*  1250 */   139,  142,  187,  103,  104,   19,   20,  187,   22,  187,
 /*  1260 */   110,  187,  112,  113,  114,  187,  141,  117,  141,  187,
 /*  1270 */    23,  187,   36,   26,  209,  210,  134,  135,  129,  209,
 /*  1280 */   210,  209,  210,  134,  135,   22,  159,  209,  210,  187,
 /*  1290 */   187,  209,  210,  209,  210,   59,  113,  187,  148,  149,
 /*  1300 */   150,  151,  152,  289,  290,  187,  157,   71,  248,  249,
 /*  1310 */   114,  141,  209,  210,   46,  125,  116,  117,  138,   19,
 /*  1320 */    20,   85,   22,  148,   61,  150,   90,  209,  210,   23,
 /*  1330 */     7,    8,   26,   97,  187,  139,   36,  147,   59,  103,
 /*  1340 */   104,   19,   20,  187,   22,   59,  110,  187,  112,  113,
 /*  1350 */   114,    1,    2,  117,  187,    5,  209,  210,   36,   59,
 /*  1360 */    10,   11,   12,   13,   14,  209,  210,   17,   59,  209,
 /*  1370 */   210,   71,  187,  148,  250,  150,  209,  210,  187,  111,
 /*  1380 */    30,   59,   32,   22,  148,  149,  150,  151,  152,  187,
 /*  1390 */    40,  187,  113,   71,  209,  210,  187,   97,  187,  113,
 /*  1400 */   209,  210,  187,  103,  104,  105,   23,  187,  187,   26,
 /*  1410 */   110,  187,  112,  113,  114,   83,   84,  117,   23,   97,
 /*  1420 */    70,   26,  113,  218,  187,  103,  104,  187,   78,  209,
 /*  1430 */   210,   81,  110,  187,  112,  113,  114,   19,   20,  117,
 /*  1440 */    22,  187,  187,  187,  187,   95,  209,  210,  148,  149,
 /*  1450 */   150,  151,  152,  187,   36,   23,  187,  187,   26,  187,
 /*  1460 */   187,  187,  187,  209,  210,  209,  210,  187,  187,  218,
 /*  1470 */   148,  149,  150,  151,  152,  209,  210,   59,  187,  129,
 /*  1480 */   187,  209,  210,  187,  134,  135,  187,  306,  187,   71,
 /*  1490 */   209,  210,   23,  228,  187,   26,   23,  187,  137,   26,
 /*  1500 */   209,  210,  209,  210,  187,  209,  210,  157,  209,  210,
 /*  1510 */   209,  210,  218,  187,  187,   97,  209,  210,  187,  278,
 /*  1520 */    23,  103,  104,   26,  187,  187,  187,  187,  110,  187,
 /*  1530 */   112,  113,  114,  187,  187,  117,    5,  247,  187,  187,
 /*  1540 */   187,   10,   11,   12,   13,   14,  209,  210,   17,  209,
 /*  1550 */   210,  209,  210,  187,  187,  187,  209,  210,  187,   23,
 /*  1560 */   187,   30,   26,   32,  187,  187,  148,  149,  150,  151,
 /*  1570 */   152,   40,  187,  187,  187,  209,  210,  209,  210,  187,
 /*  1580 */   209,  210,  209,  210,  187,  187,  209,  210,  187,  277,
 /*  1590 */   234,  187,  247,  187,  209,  210,  209,  210,  187,  235,
 /*  1600 */   187,   70,  187,  207,  247,  247,  247,  209,  210,   78,
 /*  1610 */   209,  210,   81,  209,  210,  209,  210,  187,  185,  238,
 /*  1620 */   209,  210,  209,  210,  209,  210,   95,  251,  287,  238,
 /*  1630 */   251,   23,   23,   23,   26,   26,   26,  283,   23,  209,
 /*  1640 */   210,   26,  213,  238,  221,  283,  212,  212,  217,  212,
 /*  1650 */   251,  241,  270,  241,  237,  235,  190,   60,  137,  287,
 /*  1660 */   129,  194,  194,   38,  284,  134,  135,  273,  284,  194,
 /*  1670 */   146,  111,   22,   43,  226,  145,  262,  261,  238,   18,
 /*  1680 */   229,  229,  229,  229,  194,   18,  193,  238,  157,  262,
 /*  1690 */   226,  226,  194,  238,  238,  193,  153,  261,   62,  280,
 /*  1700 */   279,  194,  193,   22,  194,  214,  193,  111,  194,  193,
 /*  1710 */   214,  211,  211,   64,  211,  211,  122,  211,  219,  214,
 /*  1720 */   109,  213,  160,  211,  300,  140,  211,  253,  111,  272,
 /*  1730 */   219,  214,  272,  214,  252,  194,  253,  252,   91,  253,
 /*  1740 */   252,   82,  253,  305,  252,  144,  305,  141,   22,  194,
 /*  1750 */   269,  257,  257,  153,  267,  143,  142,   25,  197,   26,
 /*  1760 */   242,  241,  196,  240,  242,  239,  238,   13,  188,  188,
 /*  1770 */     6,  293,  186,  186,  186,  200,  206,  293,  206,  206,
 /*  1780 */   206,    4,  200,  215,  215,  207,  207,    3,   22,  206,
 /*  1790 */   200,  158,   96,   15,   23,   16,   23,  135,  146,  126,
 /*  1800 */    24,  138,   20,   16,  140,    1,  138,  147,  126,   61,
 /*  1810 */    53,   37,  146,   53,   53,  290,   53,  126,  112,   34,
 /*  1820 */   137,    1,    5,   22,  111,  156,   68,   68,   26,   41,
 /*  1830 */    75,  137,  111,   24,   20,   19,  127,  121,   23,   67,
 /*  1840 */    22,   22,   67,   22,   22,   37,   22,   67,   23,  145,
 /*  1850 */    22,   28,   23,   23,   23,  137,   23,   22,   26,   22,
 /*  1860 */    24,   23,  112,   24,   23,   23,   22,  139,   34,   26,
 /*  1870 */    75,   88,   86,   75,   34,   23,   22,   24,   22,   34,
 /*  1880 */    34,   34,   93,   34,   26,   26,   23,   23,   23,   34,
 /*  1890 */    23,   23,   44,   23,   11,   26,   22,   22,   26,   23,
 /*  1900 */    23,   22,   22,   15,  137,  137,  137,  137,   23,    1,
 /*  1910 */   307,  307,  131,  307,  307,  307,  307,  307,  307,  307,
 /*  1920 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  1930 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  1940 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  1950 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  1960 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  1970 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  1980 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  1990 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  2000 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  2010 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  2020 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  2030 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  2040 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  2050 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  2060 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  2070 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  2080 */   307,  307,  307,  307,  307,  307,  307,  307,  307,  307,
 /*  2090 */   307,  307,
};
#define YY_SHIFT_COUNT    (542)
#define YY_SHIFT_MIN      (0)
#define YY_SHIFT_MAX      (1908)
static const unsigned short int yy_shift_ofst[] = {
 /*     0 */  1350, 1149, 1531,  939,  939,  548,  996, 1150, 1236, 1322,
 /*    10 */  1322, 1322,  334,    0,    0,  178,  777, 1322, 1322, 1322,
 /*    20 */  1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322,
 /*    30 */   991,  991, 1125, 1125,  447,  597,  548,  548,  548,  548,
 /*    40 */   548,  548,   40,  108,  217,  284,  323,  390,  429,  496,
 /*    50 */   535,  602,  641,  757,  777,  777,  777,  777,  777,  777,
 /*    60 */   777,  777,  777,  777,  777,  777,  777,  777,  777,  777,
 /*    70 */   777,  777,  796,  777,  887,  900,  900, 1300, 1322, 1322,
 /*    80 */  1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322,
 /*    90 */  1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322,
 /*   100 */  1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322,
 /*   110 */  1418, 1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322, 1322,
 /*   120 */  1322, 1322, 1322, 1322,  147,  254,  254,  254,  254,  254,
 /*   130 */    84,  185,   66,  853,  958, 1121,  853,   92,   92,  853,
 /*   140 */   321,  321,  321,  321,  325,  350,  350,  461,  150, 1913,
 /*   150 */  1913,  285,  285,  285,  236,  184,  349,  184,  184,  712,
 /*   160 */   712,  433,  553,  771,  899,  853,  853,  853,  853,  853,
 /*   170 */   853,  853,  853,  853,  853,  853,  853,  853,  853,  853,
 /*   180 */   853,  853,  853,  853,  853,  853,   46,   46,  853,  113,
 /*   190 */   223,  223, 1183, 1183, 1127, 1142, 1913, 1913, 1913,  459,
 /*   200 */   514,  514,  653,  495,  657,  305,  705,  560,  622,  776,
 /*   210 */   853,  853,  853,  853,  853,  853,  853,  853,  853,  545,
 /*   220 */   853,  853,  853,  853,  853,  853,  853,  853,  853,  853,
 /*   230 */   853,  853, 1002, 1002, 1002,  853,  853,  853,  853, 1111,
 /*   240 */   853,  853,  853, 1006, 1109,  853,  853, 1168,  853,  853,
 /*   250 */   853,  853,  853,  853,  853,  853,  845, 1164,  738,  953,
 /*   260 */   953,  953,  953, 1196,  738,  738,   45,   96,  964,  179,
 /*   270 */   580,  907,  907, 1073,  580,  580, 1073,  498,  388, 1268,
 /*   280 */  1187, 1187, 1187,  907, 1170, 1170, 1058, 1180,  328, 1219,
 /*   290 */  1597, 1521, 1521, 1625, 1625, 1521, 1524, 1560, 1650, 1630,
 /*   300 */  1530, 1661, 1661, 1661, 1661, 1521, 1667, 1530, 1530, 1560,
 /*   310 */  1650, 1630, 1630, 1530, 1521, 1667, 1543, 1636, 1521, 1667,
 /*   320 */  1681, 1521, 1667, 1521, 1667, 1681, 1596, 1596, 1596, 1649,
 /*   330 */  1681, 1596, 1594, 1596, 1649, 1596, 1596, 1562, 1681, 1611,
 /*   340 */  1611, 1681, 1585, 1617, 1585, 1617, 1585, 1617, 1585, 1617,
 /*   350 */  1521, 1647, 1647, 1659, 1659, 1601, 1606, 1726, 1521, 1600,
 /*   360 */  1601, 1612, 1614, 1530, 1732, 1733, 1754, 1754, 1764, 1764,
 /*   370 */  1764, 1913, 1913, 1913, 1913, 1913, 1913, 1913, 1913, 1913,
 /*   380 */  1913, 1913, 1913, 1913, 1913, 1913,  673,  901,  283,  740,
 /*   390 */   707,  973,  655, 1247, 1048, 1097, 1190, 1306, 1263, 1383,
 /*   400 */  1395, 1432, 1469, 1473, 1497, 1279, 1200, 1323, 1075, 1286,
 /*   410 */  1536, 1608, 1332, 1609, 1175, 1225, 1610, 1615, 1309, 1361,
 /*   420 */  1777, 1784, 1766, 1633, 1778, 1696, 1779, 1771, 1773, 1662,
 /*   430 */  1652, 1673, 1776, 1663, 1782, 1664, 1787, 1804, 1668, 1660,
 /*   440 */  1682, 1748, 1774, 1666, 1757, 1760, 1761, 1763, 1691, 1706,
 /*   450 */  1785, 1683, 1820, 1817, 1801, 1713, 1669, 1758, 1802, 1759,
 /*   460 */  1755, 1788, 1694, 1721, 1809, 1814, 1816, 1709, 1716, 1818,
 /*   470 */  1772, 1819, 1821, 1815, 1822, 1775, 1823, 1824, 1780, 1808,
 /*   480 */  1825, 1704, 1828, 1829, 1830, 1831, 1832, 1833, 1835, 1836,
 /*   490 */  1838, 1837, 1839, 1718, 1841, 1842, 1750, 1834, 1844, 1728,
 /*   500 */  1843, 1840, 1845, 1846, 1847, 1783, 1795, 1786, 1848, 1798,
 /*   510 */  1789, 1849, 1852, 1854, 1853, 1858, 1859, 1855, 1863, 1843,
 /*   520 */  1864, 1865, 1867, 1868, 1869, 1870, 1856, 1883, 1874, 1875,
 /*   530 */  1876, 1877, 1879, 1880, 1872, 1781, 1767, 1768, 1769, 1770,
 /*   540 */  1885, 1888, 1908,
};
#define YY_REDUCE_COUNT (385)
#define YY_REDUCE_MIN   (-256)
#define YY_REDUCE_MAX   (1590)
static const short yy_reduce_ofst[] = {
 /*     0 */   880, -121,  269,  528,  933, -119, -187, -185, -182, -180,
 /*    10 */  -176, -174,  -62,  -46,  131, -248, -133,  407,  568,  700,
 /*    20 */   704,  278,  706,  824,  542,  830,  948,  773,  943,  946,
 /*    30 */    71,  650,  211,  267,  826,  272,  676,  732,  885,  976,
 /*    40 */   984,  992, -256, -256, -256, -256, -256, -256, -256, -256,
 /*    50 */  -256, -256, -256, -256, -256, -256, -256, -256, -256, -256,
 /*    60 */  -256, -256, -256, -256, -256, -256, -256, -256, -256, -256,
 /*    70 */  -256, -256, -256, -256, -256, -256, -256,  989, 1065, 1070,
 /*    80 */  1072, 1078, 1082, 1084, 1103, 1118, 1147, 1156, 1160, 1167,
 /*    90 */  1185, 1191, 1220, 1237, 1254, 1256, 1266, 1272, 1281, 1291,
 /*   100 */  1293, 1296, 1299, 1301, 1307, 1337, 1340, 1342, 1347, 1366,
 /*   110 */  1368, 1371, 1373, 1377, 1385, 1387, 1398, 1401, 1404, 1406,
 /*   120 */  1411, 1413, 1415, 1430, -256, -256, -256, -256, -256, -256,
 /*   130 */  -256, -256, -256, -172,  508, -213,   57, -163,  -25,  593,
 /*   140 */    69,  486,   69,  486, -200,  573,  722, -256, -256, -256,
 /*   150 */  -256, -141, -141, -141, -105, -161, -167,  157,  212,  405,
 /*   160 */   530,  220,  233,  735,  735,  115,  318,  406,  612,  541,
 /*   170 */  -166,  441,  688,  794,  629,  368,  741,  775,  867,  797,
 /*   180 */   871,  842, -186, 1000,  858,  949,  379,  783,   70,  296,
 /*   190 */   821,  903,  924, 1044,  651,  282, 1014, 1060,  937, -195,
 /*   200 */  -177,  413,  439,  511,  566,  787,  827,  848,  898,  945,
 /*   210 */  1062, 1074, 1102, 1110, 1202, 1204, 1209, 1211, 1215,  529,
 /*   220 */  1221, 1224, 1240, 1246, 1255, 1257, 1269, 1270, 1273, 1274,
 /*   230 */  1275, 1280, 1205, 1251, 1294, 1310, 1317, 1326, 1327, 1124,
 /*   240 */  1331, 1338, 1339, 1290, 1181, 1346, 1351, 1265, 1352,  787,
 /*   250 */  1353, 1367, 1378, 1386, 1392, 1397, 1241, 1312, 1356, 1345,
 /*   260 */  1357, 1358, 1359, 1124, 1356, 1356, 1364, 1396, 1433, 1341,
 /*   270 */  1381, 1376, 1379, 1354, 1391, 1405, 1362, 1429, 1423, 1431,
 /*   280 */  1434, 1435, 1437, 1399, 1410, 1412, 1382, 1417, 1420, 1466,
 /*   290 */  1372, 1467, 1468, 1380, 1384, 1475, 1394, 1414, 1416, 1448,
 /*   300 */  1440, 1451, 1452, 1453, 1454, 1490, 1493, 1449, 1455, 1427,
 /*   310 */  1436, 1464, 1465, 1456, 1498, 1502, 1419, 1421, 1507, 1509,
 /*   320 */  1491, 1510, 1513, 1514, 1516, 1496, 1500, 1501, 1503, 1499,
 /*   330 */  1505, 1504, 1508, 1506, 1511, 1512, 1515, 1424, 1517, 1457,
 /*   340 */  1460, 1519, 1474, 1482, 1483, 1485, 1486, 1488, 1489, 1492,
 /*   350 */  1541, 1438, 1441, 1494, 1495, 1518, 1520, 1487, 1555, 1481,
 /*   360 */  1522, 1523, 1526, 1528, 1561, 1566, 1580, 1581, 1586, 1587,
 /*   370 */  1588, 1478, 1484, 1525, 1575, 1570, 1572, 1573, 1574, 1582,
 /*   380 */  1568, 1569, 1578, 1579, 1583, 1590,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */  1554, 1554, 1554, 1392, 1171, 1278, 1171, 1171, 1171, 1392,
 /*    10 */  1392, 1392, 1171, 1308, 1308, 1445, 1202, 1171, 1171, 1171,
 /*    20 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1391, 1171, 1171,
 /*    30 */  1171, 1171, 1475, 1475, 1171, 1171, 1171, 1171, 1171, 1171,
 /*    40 */  1171, 1171, 1171, 1317, 1171, 1171, 1171, 1171, 1171, 1393,
 /*    50 */  1394, 1171, 1171, 1171, 1444, 1446, 1409, 1327, 1326, 1325,
 /*    60 */  1324, 1427, 1295, 1322, 1315, 1319, 1387, 1388, 1386, 1390,
 /*    70 */  1394, 1393, 1171, 1318, 1358, 1372, 1357, 1171, 1171, 1171,
 /*    80 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*    90 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   100 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   110 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   120 */  1171, 1171, 1171, 1171, 1366, 1371, 1377, 1370, 1367, 1360,
 /*   130 */  1359, 1361, 1362, 1171, 1192, 1242, 1171, 1171, 1171, 1171,
 /*   140 */  1463, 1462, 1171, 1171, 1202, 1352, 1351, 1363, 1364, 1374,
 /*   150 */  1373, 1452, 1510, 1509, 1410, 1171, 1171, 1171, 1171, 1171,
 /*   160 */  1171, 1475, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   170 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   180 */  1171, 1171, 1171, 1171, 1171, 1171, 1475, 1475, 1171, 1202,
 /*   190 */  1475, 1475, 1198, 1198, 1302, 1171, 1458, 1278, 1269, 1171,
 /*   200 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   210 */  1171, 1171, 1171, 1449, 1447, 1171, 1171, 1171, 1171, 1171,
 /*   220 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   230 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   240 */  1171, 1171, 1171, 1274, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   250 */  1171, 1171, 1171, 1171, 1171, 1504, 1171, 1422, 1256, 1274,
 /*   260 */  1274, 1274, 1274, 1276, 1257, 1255, 1268, 1203, 1178, 1546,
 /*   270 */  1321, 1297, 1297, 1543, 1321, 1321, 1543, 1217, 1524, 1214,
 /*   280 */  1308, 1308, 1308, 1297, 1302, 1302, 1389, 1275, 1268, 1171,
 /*   290 */  1546, 1283, 1283, 1545, 1545, 1283, 1410, 1330, 1336, 1245,
 /*   300 */  1321, 1251, 1251, 1251, 1251, 1283, 1189, 1321, 1321, 1330,
 /*   310 */  1336, 1245, 1245, 1321, 1283, 1189, 1426, 1540, 1283, 1189,
 /*   320 */  1400, 1283, 1189, 1283, 1189, 1400, 1243, 1243, 1243, 1232,
 /*   330 */  1400, 1243, 1217, 1243, 1232, 1243, 1243, 1493, 1400, 1404,
 /*   340 */  1404, 1400, 1301, 1296, 1301, 1296, 1301, 1296, 1301, 1296,
 /*   350 */  1283, 1485, 1485, 1311, 1311, 1316, 1302, 1395, 1283, 1171,
 /*   360 */  1316, 1314, 1312, 1321, 1195, 1235, 1507, 1507, 1503, 1503,
 /*   370 */  1503, 1551, 1551, 1458, 1519, 1202, 1202, 1202, 1202, 1519,
 /*   380 */  1219, 1219, 1203, 1203, 1202, 1519, 1171, 1171, 1171, 1171,
 /*   390 */  1171, 1171, 1514, 1171, 1411, 1287, 1171, 1171, 1171, 1171,
 /*   400 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   410 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1341,
 /*   420 */  1171, 1174, 1455, 1171, 1171, 1453, 1171, 1171, 1171, 1171,
 /*   430 */  1171, 1171, 1288, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   440 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   450 */  1171, 1542, 1171, 1171, 1171, 1171, 1171, 1171, 1425, 1424,
 /*   460 */  1171, 1171, 1285, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   470 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   480 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   490 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   500 */  1313, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   510 */  1171, 1171, 1171, 1171, 1171, 1490, 1303, 1171, 1171, 1533,
 /*   520 */  1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171, 1171,
 /*   530 */  1171, 1171, 1171, 1171, 1528, 1259, 1343, 1171, 1342, 1346,
 /*   540 */  1171, 1183, 1171,
};
/********** End of lemon-generated parsing tables *****************************/

/* The next table maps tokens (terminal symbols) into fallback tokens.  
** If a construct like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
**
** This feature can be used, for example, to cause some keywords in a language
** to revert to identifiers if they keyword does not apply in the context where
** it appears.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
    0,  /*          $ => nothing */
    0,  /*       SEMI => nothing */
   59,  /*    EXPLAIN => ID */
   59,  /*      QUERY => ID */
   59,  /*       PLAN => ID */
   59,  /*      BEGIN => ID */
    0,  /* TRANSACTION => nothing */
   59,  /*   DEFERRED => ID */
   59,  /*  IMMEDIATE => ID */
   59,  /*  EXCLUSIVE => ID */
    0,  /*     COMMIT => nothing */
   59,  /*        END => ID */
   59,  /*   ROLLBACK => ID */
   59,  /*  SAVEPOINT => ID */
   59,  /*    RELEASE => ID */
    0,  /*         TO => nothing */
    0,  /*      TABLE => nothing */
    0,  /*     CREATE => nothing */
   59,  /*         IF => ID */
    0,  /*        NOT => nothing */
    0,  /*     EXISTS => nothing */
   59,  /*       TEMP => ID */
    0,  /*         LP => nothing */
    0,  /*         RP => nothing */
    0,  /*         AS => nothing */
   59,  /*    WITHOUT => ID */
    0,  /*      COMMA => nothing */
   59,  /*      ABORT => ID */
   59,  /*     ACTION => ID */
   59,  /*      AFTER => ID */
   59,  /*    ANALYZE => ID */
   59,  /*        ASC => ID */
   59,  /*     ATTACH => ID */
   59,  /*     BEFORE => ID */
   59,  /*         BY => ID */
   59,  /*    CASCADE => ID */
   59,  /*       CAST => ID */
   59,  /*   CONFLICT => ID */
   59,  /*   DATABASE => ID */
   59,  /*       DESC => ID */
   59,  /*     DETACH => ID */
   59,  /*       EACH => ID */
   59,  /*       FAIL => ID */
    0,  /*         OR => nothing */
    0,  /*        AND => nothing */
    0,  /*         IS => nothing */
   59,  /*      MATCH => ID */
   59,  /*    LIKE_KW => ID */
    0,  /*    BETWEEN => nothing */
    0,  /*         IN => nothing */
    0,  /*     ISNULL => nothing */
    0,  /*    NOTNULL => nothing */
    0,  /*         NE => nothing */
    0,  /*         EQ => nothing */
    0,  /*         GT => nothing */
    0,  /*         LE => nothing */
    0,  /*         LT => nothing */
    0,  /*         GE => nothing */
    0,  /*     ESCAPE => nothing */
    0,  /*         ID => nothing */
   59,  /*   COLUMNKW => ID */
   59,  /*         DO => ID */
   59,  /*        FOR => ID */
   59,  /*     IGNORE => ID */
   59,  /*  INITIALLY => ID */
   59,  /*    INSTEAD => ID */
   59,  /*         NO => ID */
   59,  /*        KEY => ID */
   59,  /*         OF => ID */
   59,  /*     OFFSET => ID */
   59,  /*     PRAGMA => ID */
   59,  /*      RAISE => ID */
   59,  /*  RECURSIVE => ID */
   59,  /*    REPLACE => ID */
   59,  /*   RESTRICT => ID */
   59,  /*        ROW => ID */
   59,  /*       ROWS => ID */
   59,  /*    TRIGGER => ID */
   59,  /*     VACUUM => ID */
   59,  /*       VIEW => ID */
   59,  /*    VIRTUAL => ID */
   59,  /*       WITH => ID */
   59,  /*      NULLS => ID */
   59,  /*      FIRST => ID */
   59,  /*       LAST => ID */
   59,  /*    CURRENT => ID */
   59,  /*  FOLLOWING => ID */
   59,  /*  PARTITION => ID */
   59,  /*  PRECEDING => ID */
   59,  /*      RANGE => ID */
   59,  /*  UNBOUNDED => ID */
   59,  /*    EXCLUDE => ID */
   59,  /*     GROUPS => ID */
   59,  /*     OTHERS => ID */
   59,  /*       TIES => ID */
   59,  /*    REINDEX => ID */
   59,  /*     RENAME => ID */
   59,  /*   CTIME_KW => ID */
    0,  /*        ANY => nothing */
    0,  /*     BITAND => nothing */
    0,  /*      BITOR => nothing */
    0,  /*     LSHIFT => nothing */
    0,  /*     RSHIFT => nothing */
    0,  /*       PLUS => nothing */
    0,  /*      MINUS => nothing */
    0,  /*       STAR => nothing */
    0,  /*      SLASH => nothing */
    0,  /*        REM => nothing */
    0,  /*     CONCAT => nothing */
    0,  /*    COLLATE => nothing */
    0,  /*     BITNOT => nothing */
    0,  /*         ON => nothing */
    0,  /*    INDEXED => nothing */
    0,  /*     STRING => nothing */
    0,  /*    JOIN_KW => nothing */
    0,  /* CONSTRAINT => nothing */
    0,  /*    DEFAULT => nothing */
    0,  /*       NULL => nothing */
    0,  /*    PRIMARY => nothing */
    0,  /*     UNIQUE => nothing */
    0,  /*      CHECK => nothing */
    0,  /* REFERENCES => nothing */
    0,  /*   AUTOINCR => nothing */
    0,  /*     INSERT => nothing */
    0,  /*     DELETE => nothing */
    0,  /*     UPDATE => nothing */
    0,  /*        SET => nothing */
    0,  /* DEFERRABLE => nothing */
    0,  /*    FOREIGN => nothing */
    0,  /*       DROP => nothing */
    0,  /*      UNION => nothing */
    0,  /*        ALL => nothing */
    0,  /*     EXCEPT => nothing */
    0,  /*  INTERSECT => nothing */
    0,  /*     SELECT => nothing */
    0,  /*     VALUES => nothing */
    0,  /*   DISTINCT => nothing */
    0,  /*        DOT => nothing */
    0,  /*       FROM => nothing */
    0,  /*       JOIN => nothing */
    0,  /*      USING => nothing */
    0,  /*      ORDER => nothing */
    0,  /*      GROUP => nothing */
    0,  /*     HAVING => nothing */
    0,  /*      LIMIT => nothing */
    0,  /*      WHERE => nothing */
    0,  /*       INTO => nothing */
    0,  /*    NOTHING => nothing */
    0,  /*      FLOAT => nothing */
    0,  /*       BLOB => nothing */
    0,  /*    INTEGER => nothing */
    0,  /*   VARIABLE => nothing */
    0,  /*       CASE => nothing */
    0,  /*       WHEN => nothing */
    0,  /*       THEN => nothing */
    0,  /*       ELSE => nothing */
    0,  /*      INDEX => nothing */
    0,  /*      ALTER => nothing */
    0,  /*        ADD => nothing */
    0,  /*     WINDOW => nothing */
    0,  /*       OVER => nothing */
    0,  /*     FILTER => nothing */
    0,  /*     COLUMN => nothing */
    0,  /* AGG_FUNCTION => nothing */
    0,  /* AGG_COLUMN => nothing */
    0,  /*  TRUEFALSE => nothing */
    0,  /*      ISNOT => nothing */
    0,  /*   FUNCTION => nothing */
    0,  /*     UMINUS => nothing */
    0,  /*      UPLUS => nothing */
    0,  /*      TRUTH => nothing */
    0,  /*   REGISTER => nothing */
    0,  /*     VECTOR => nothing */
    0,  /* SELECT_COLUMN => nothing */
    0,  /* IF_NULL_ROW => nothing */
    0,  /*   ASTERISK => nothing */
    0,  /*       SPAN => nothing */
    0,  /*      SPACE => nothing */
    0,  /*    ILLEGAL => nothing */
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
**
** After the "shift" half of a SHIFTREDUCE action, the stateno field
** actually contains the reduce action for the second half of the
** SHIFTREDUCE.
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number, or reduce action in SHIFTREDUCE */
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level */
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  yyStackEntry *yytos;          /* Pointer to top element of the stack */
#ifdef YYTRACKMAXSTACKDEPTH
  int yyhwm;                    /* High-water mark of the stack */
#endif
#ifndef YYNOERRORRECOVERY
  int yyerrcnt;                 /* Shifts left before out of the error */
#endif
  sqlite3ParserARG_SDECL                /* A place to hold %extra_argument */
  sqlite3ParserCTX_SDECL                /* A place to hold %extra_context */
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
  yyStackEntry yystk0;          /* First stack entry */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
  yyStackEntry *yystackEnd;            /* Last entry in the stack */
#endif
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
/* #include <stdio.h> */
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
SQLITE_PRIVATE void sqlite3ParserTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#if defined(YYCOVERAGE) || !defined(NDEBUG)
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  /*    0 */ "$",
  /*    1 */ "SEMI",
  /*    2 */ "EXPLAIN",
  /*    3 */ "QUERY",
  /*    4 */ "PLAN",
  /*    5 */ "BEGIN",
  /*    6 */ "TRANSACTION",
  /*    7 */ "DEFERRED",
  /*    8 */ "IMMEDIATE",
  /*    9 */ "EXCLUSIVE",
  /*   10 */ "COMMIT",
  /*   11 */ "END",
  /*   12 */ "ROLLBACK",
  /*   13 */ "SAVEPOINT",
  /*   14 */ "RELEASE",
  /*   15 */ "TO",
  /*   16 */ "TABLE",
  /*   17 */ "CREATE",
  /*   18 */ "IF",
  /*   19 */ "NOT",
  /*   20 */ "EXISTS",
  /*   21 */ "TEMP",
  /*   22 */ "LP",
  /*   23 */ "RP",
  /*   24 */ "AS",
  /*   25 */ "WITHOUT",
  /*   26 */ "COMMA",
  /*   27 */ "ABORT",
  /*   28 */ "ACTION",
  /*   29 */ "AFTER",
  /*   30 */ "ANALYZE",
  /*   31 */ "ASC",
  /*   32 */ "ATTACH",
  /*   33 */ "BEFORE",
  /*   34 */ "BY",
  /*   35 */ "CASCADE",
  /*   36 */ "CAST",
  /*   37 */ "CONFLICT",
  /*   38 */ "DATABASE",
  /*   39 */ "DESC",
  /*   40 */ "DETACH",
  /*   41 */ "EACH",
  /*   42 */ "FAIL",
  /*   43 */ "OR",
  /*   44 */ "AND",
  /*   45 */ "IS",
  /*   46 */ "MATCH",
  /*   47 */ "LIKE_KW",
  /*   48 */ "BETWEEN",
  /*   49 */ "IN",
  /*   50 */ "ISNULL",
  /*   51 */ "NOTNULL",
  /*   52 */ "NE",
  /*   53 */ "EQ",
  /*   54 */ "GT",
  /*   55 */ "LE",
  /*   56 */ "LT",
  /*   57 */ "GE",
  /*   58 */ "ESCAPE",
  /*   59 */ "ID",
  /*   60 */ "COLUMNKW",
  /*   61 */ "DO",
  /*   62 */ "FOR",
  /*   63 */ "IGNORE",
  /*   64 */ "INITIALLY",
  /*   65 */ "INSTEAD",
  /*   66 */ "NO",
  /*   67 */ "KEY",
  /*   68 */ "OF",
  /*   69 */ "OFFSET",
  /*   70 */ "PRAGMA",
  /*   71 */ "RAISE",
  /*   72 */ "RECURSIVE",
  /*   73 */ "REPLACE",
  /*   74 */ "RESTRICT",
  /*   75 */ "ROW",
  /*   76 */ "ROWS",
  /*   77 */ "TRIGGER",
  /*   78 */ "VACUUM",
  /*   79 */ "VIEW",
  /*   80 */ "VIRTUAL",
  /*   81 */ "WITH",
  /*   82 */ "NULLS",
  /*   83 */ "FIRST",
  /*   84 */ "LAST",
  /*   85 */ "CURRENT",
  /*   86 */ "FOLLOWING",
  /*   87 */ "PARTITION",
  /*   88 */ "PRECEDING",
  /*   89 */ "RANGE",
  /*   90 */ "UNBOUNDED",
  /*   91 */ "EXCLUDE",
  /*   92 */ "GROUPS",
  /*   93 */ "OTHERS",
  /*   94 */ "TIES",
  /*   95 */ "REINDEX",
  /*   96 */ "RENAME",
  /*   97 */ "CTIME_KW",
  /*   98 */ "ANY",
  /*   99 */ "BITAND",
  /*  100 */ "BITOR",
  /*  101 */ "LSHIFT",
  /*  102 */ "RSHIFT",
  /*  103 */ "PLUS",
  /*  104 */ "MINUS",
  /*  105 */ "STAR",
  /*  106 */ "SLASH",
  /*  107 */ "REM",
  /*  108 */ "CONCAT",
  /*  109 */ "COLLATE",
  /*  110 */ "BITNOT",
  /*  111 */ "ON",
  /*  112 */ "INDEXED",
  /*  113 */ "STRING",
  /*  114 */ "JOIN_KW",
  /*  115 */ "CONSTRAINT",
  /*  116 */ "DEFAULT",
  /*  117 */ "NULL",
  /*  118 */ "PRIMARY",
  /*  119 */ "UNIQUE",
  /*  120 */ "CHECK",
  /*  121 */ "REFERENCES",
  /*  122 */ "AUTOINCR",
  /*  123 */ "INSERT",
  /*  124 */ "DELETE",
  /*  125 */ "UPDATE",
  /*  126 */ "SET",
  /*  127 */ "DEFERRABLE",
  /*  128 */ "FOREIGN",
  /*  129 */ "DROP",
  /*  130 */ "UNION",
  /*  131 */ "ALL",
  /*  132 */ "EXCEPT",
  /*  133 */ "INTERSECT",
  /*  134 */ "SELECT",
  /*  135 */ "VALUES",
  /*  136 */ "DISTINCT",
  /*  137 */ "DOT",
  /*  138 */ "FROM",
  /*  139 */ "JOIN",
  /*  140 */ "USING",
  /*  141 */ "ORDER",
  /*  142 */ "GROUP",
  /*  143 */ "HAVING",
  /*  144 */ "LIMIT",
  /*  145 */ "WHERE",
  /*  146 */ "INTO",
  /*  147 */ "NOTHING",
  /*  148 */ "FLOAT",
  /*  149 */ "BLOB",
  /*  150 */ "INTEGER",
  /*  151 */ "VARIABLE",
  /*  152 */ "CASE",
  /*  153 */ "WHEN",
  /*  154 */ "THEN",
  /*  155 */ "ELSE",
  /*  156 */ "INDEX",
  /*  157 */ "ALTER",
  /*  158 */ "ADD",
  /*  159 */ "WINDOW",
  /*  160 */ "OVER",
  /*  161 */ "FILTER",
  /*  162 */ "COLUMN",
  /*  163 */ "AGG_FUNCTION",
  /*  164 */ "AGG_COLUMN",
  /*  165 */ "TRUEFALSE",
  /*  166 */ "ISNOT",
  /*  167 */ "FUNCTION",
  /*  168 */ "UMINUS",
  /*  169 */ "UPLUS",
  /*  170 */ "TRUTH",
  /*  171 */ "REGISTER",
  /*  172 */ "VECTOR",
  /*  173 */ "SELECT_COLUMN",
  /*  174 */ "IF_NULL_ROW",
  /*  175 */ "ASTERISK",
  /*  176 */ "SPAN",
  /*  177 */ "SPACE",
  /*  178 */ "ILLEGAL",
  /*  179 */ "input",
  /*  180 */ "cmdlist",
  /*  181 */ "ecmd",
  /*  182 */ "cmdx",
  /*  183 */ "explain",
  /*  184 */ "cmd",
  /*  185 */ "transtype",
  /*  186 */ "trans_opt",
  /*  187 */ "nm",
  /*  188 */ "savepoint_opt",
  /*  189 */ "create_table",
  /*  190 */ "create_table_args",
  /*  191 */ "createkw",
  /*  192 */ "temp",
  /*  193 */ "ifnotexists",
  /*  194 */ "dbnm",
  /*  195 */ "columnlist",
  /*  196 */ "conslist_opt",
  /*  197 */ "table_options",
  /*  198 */ "select",
  /*  199 */ "columnname",
  /*  200 */ "carglist",
  /*  201 */ "typetoken",
  /*  202 */ "typename",
  /*  203 */ "signed",
  /*  204 */ "plus_num",
  /*  205 */ "minus_num",
  /*  206 */ "scanpt",
  /*  207 */ "scantok",
  /*  208 */ "ccons",
  /*  209 */ "term",
  /*  210 */ "expr",
  /*  211 */ "onconf",
  /*  212 */ "sortorder",
  /*  213 */ "autoinc",
  /*  214 */ "eidlist_opt",
  /*  215 */ "refargs",
  /*  216 */ "defer_subclause",
  /*  217 */ "refarg",
  /*  218 */ "refact",
  /*  219 */ "init_deferred_pred_opt",
  /*  220 */ "conslist",
  /*  221 */ "tconscomma",
  /*  222 */ "tcons",
  /*  223 */ "sortlist",
  /*  224 */ "eidlist",
  /*  225 */ "defer_subclause_opt",
  /*  226 */ "orconf",
  /*  227 */ "resolvetype",
  /*  228 */ "raisetype",
  /*  229 */ "ifexists",
  /*  230 */ "fullname",
  /*  231 */ "selectnowith",
  /*  232 */ "oneselect",
  /*  233 */ "wqlist",
  /*  234 */ "multiselect_op",
  /*  235 */ "distinct",
  /*  236 */ "selcollist",
  /*  237 */ "from",
  /*  238 */ "where_opt",
  /*  239 */ "groupby_opt",
  /*  240 */ "having_opt",
  /*  241 */ "orderby_opt",
  /*  242 */ "limit_opt",
  /*  243 */ "window_clause",
  /*  244 */ "values",
  /*  245 */ "nexprlist",
  /*  246 */ "sclp",
  /*  247 */ "as",
  /*  248 */ "seltablist",
  /*  249 */ "stl_prefix",
  /*  250 */ "joinop",
  /*  251 */ "indexed_opt",
  /*  252 */ "on_opt",
  /*  253 */ "using_opt",
  /*  254 */ "exprlist",
  /*  255 */ "xfullname",
  /*  256 */ "idlist",
  /*  257 */ "nulls",
  /*  258 */ "with",
  /*  259 */ "setlist",
  /*  260 */ "insert_cmd",
  /*  261 */ "idlist_opt",
  /*  262 */ "upsert",
  /*  263 */ "filter_over",
  /*  264 */ "likeop",
  /*  265 */ "between_op",
  /*  266 */ "in_op",
  /*  267 */ "paren_exprlist",
  /*  268 */ "case_operand",
  /*  269 */ "case_exprlist",
  /*  270 */ "case_else",
  /*  271 */ "uniqueflag",
  /*  272 */ "collate",
  /*  273 */ "vinto",
  /*  274 */ "nmnum",
  /*  275 */ "trigger_decl",
  /*  276 */ "trigger_cmd_list",
  /*  277 */ "trigger_time",
  /*  278 */ "trigger_event",
  /*  279 */ "foreach_clause",
  /*  280 */ "when_clause",
  /*  281 */ "trigger_cmd",
  /*  282 */ "trnm",
  /*  283 */ "tridxby",
  /*  284 */ "database_kw_opt",
  /*  285 */ "key_opt",
  /*  286 */ "add_column_fullname",
  /*  287 */ "kwcolumn_opt",
  /*  288 */ "create_vtab",
  /*  289 */ "vtabarglist",
  /*  290 */ "vtabarg",
  /*  291 */ "vtabargtoken",
  /*  292 */ "lp",
  /*  293 */ "anylist",
  /*  294 */ "windowdefn_list",
  /*  295 */ "windowdefn",
  /*  296 */ "window",
  /*  297 */ "frame_opt",
  /*  298 */ "part_opt",
  /*  299 */ "filter_clause",
  /*  300 */ "over_clause",
  /*  301 */ "range_or_rows",
  /*  302 */ "frame_bound",
  /*  303 */ "frame_bound_s",
  /*  304 */ "frame_bound_e",
  /*  305 */ "frame_exclude_opt",
  /*  306 */ "frame_exclude",
};
#endif /* defined(YYCOVERAGE) || !defined(NDEBUG) */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "explain ::= EXPLAIN",
 /*   1 */ "explain ::= EXPLAIN QUERY PLAN",
 /*   2 */ "cmdx ::= cmd",
 /*   3 */ "cmd ::= BEGIN transtype trans_opt",
 /*   4 */ "transtype ::=",
 /*   5 */ "transtype ::= DEFERRED",
 /*   6 */ "transtype ::= IMMEDIATE",
 /*   7 */ "transtype ::= EXCLUSIVE",
 /*   8 */ "cmd ::= COMMIT|END trans_opt",
 /*   9 */ "cmd ::= ROLLBACK trans_opt",
 /*  10 */ "cmd ::= SAVEPOINT nm",
 /*  11 */ "cmd ::= RELEASE savepoint_opt nm",
 /*  12 */ "cmd ::= ROLLBACK trans_opt TO savepoint_opt nm",
 /*  13 */ "create_table ::= createkw temp TABLE ifnotexists nm dbnm",
 /*  14 */ "createkw ::= CREATE",
 /*  15 */ "ifnotexists ::=",
 /*  16 */ "ifnotexists ::= IF NOT EXISTS",
 /*  17 */ "temp ::= TEMP",
 /*  18 */ "temp ::=",
 /*  19 */ "create_table_args ::= LP columnlist conslist_opt RP table_options",
 /*  20 */ "create_table_args ::= AS select",
 /*  21 */ "table_options ::=",
 /*  22 */ "table_options ::= WITHOUT nm",
 /*  23 */ "columnname ::= nm typetoken",
 /*  24 */ "typetoken ::=",
 /*  25 */ "typetoken ::= typename LP signed RP",
 /*  26 */ "typetoken ::= typename LP signed COMMA signed RP",
 /*  27 */ "typename ::= typename ID|STRING",
 /*  28 */ "scanpt ::=",
 /*  29 */ "scantok ::=",
 /*  30 */ "ccons ::= CONSTRAINT nm",
 /*  31 */ "ccons ::= DEFAULT scantok term",
 /*  32 */ "ccons ::= DEFAULT LP expr RP",
 /*  33 */ "ccons ::= DEFAULT PLUS scantok term",
 /*  34 */ "ccons ::= DEFAULT MINUS scantok term",
 /*  35 */ "ccons ::= DEFAULT scantok ID|INDEXED",
 /*  36 */ "ccons ::= NOT NULL onconf",
 /*  37 */ "ccons ::= PRIMARY KEY sortorder onconf autoinc",
 /*  38 */ "ccons ::= UNIQUE onconf",
 /*  39 */ "ccons ::= CHECK LP expr RP",
 /*  40 */ "ccons ::= REFERENCES nm eidlist_opt refargs",
 /*  41 */ "ccons ::= defer_subclause",
 /*  42 */ "ccons ::= COLLATE ID|STRING",
 /*  43 */ "autoinc ::=",
 /*  44 */ "autoinc ::= AUTOINCR",
 /*  45 */ "refargs ::=",
 /*  46 */ "refargs ::= refargs refarg",
 /*  47 */ "refarg ::= MATCH nm",
 /*  48 */ "refarg ::= ON INSERT refact",
 /*  49 */ "refarg ::= ON DELETE refact",
 /*  50 */ "refarg ::= ON UPDATE refact",
 /*  51 */ "refact ::= SET NULL",
 /*  52 */ "refact ::= SET DEFAULT",
 /*  53 */ "refact ::= CASCADE",
 /*  54 */ "refact ::= RESTRICT",
 /*  55 */ "refact ::= NO ACTION",
 /*  56 */ "defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt",
 /*  57 */ "defer_subclause ::= DEFERRABLE init_deferred_pred_opt",
 /*  58 */ "init_deferred_pred_opt ::=",
 /*  59 */ "init_deferred_pred_opt ::= INITIALLY DEFERRED",
 /*  60 */ "init_deferred_pred_opt ::= INITIALLY IMMEDIATE",
 /*  61 */ "conslist_opt ::=",
 /*  62 */ "tconscomma ::= COMMA",
 /*  63 */ "tcons ::= CONSTRAINT nm",
 /*  64 */ "tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf",
 /*  65 */ "tcons ::= UNIQUE LP sortlist RP onconf",
 /*  66 */ "tcons ::= CHECK LP expr RP onconf",
 /*  67 */ "tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt",
 /*  68 */ "defer_subclause_opt ::=",
 /*  69 */ "onconf ::=",
 /*  70 */ "onconf ::= ON CONFLICT resolvetype",
 /*  71 */ "orconf ::=",
 /*  72 */ "orconf ::= OR resolvetype",
 /*  73 */ "resolvetype ::= IGNORE",
 /*  74 */ "resolvetype ::= REPLACE",
 /*  75 */ "cmd ::= DROP TABLE ifexists fullname",
 /*  76 */ "ifexists ::= IF EXISTS",
 /*  77 */ "ifexists ::=",
 /*  78 */ "cmd ::= createkw temp VIEW ifnotexists nm dbnm eidlist_opt AS select",
 /*  79 */ "cmd ::= DROP VIEW ifexists fullname",
 /*  80 */ "cmd ::= select",
 /*  81 */ "select ::= WITH wqlist selectnowith",
 /*  82 */ "select ::= WITH RECURSIVE wqlist selectnowith",
 /*  83 */ "select ::= selectnowith",
 /*  84 */ "selectnowith ::= selectnowith multiselect_op oneselect",
 /*  85 */ "multiselect_op ::= UNION",
 /*  86 */ "multiselect_op ::= UNION ALL",
 /*  87 */ "multiselect_op ::= EXCEPT|INTERSECT",
 /*  88 */ "oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt",
 /*  89 */ "oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt window_clause orderby_opt limit_opt",
 /*  90 */ "values ::= VALUES LP nexprlist RP",
 /*  91 */ "values ::= values COMMA LP nexprlist RP",
 /*  92 */ "distinct ::= DISTINCT",
 /*  93 */ "distinct ::= ALL",
 /*  94 */ "distinct ::=",
 /*  95 */ "sclp ::=",
 /*  96 */ "selcollist ::= sclp scanpt expr scanpt as",
 /*  97 */ "selcollist ::= sclp scanpt STAR",
 /*  98 */ "selcollist ::= sclp scanpt nm DOT STAR",
 /*  99 */ "as ::= AS nm",
 /* 100 */ "as ::=",
 /* 101 */ "from ::=",
 /* 102 */ "from ::= FROM seltablist",
 /* 103 */ "stl_prefix ::= seltablist joinop",
 /* 104 */ "stl_prefix ::=",
 /* 105 */ "seltablist ::= stl_prefix nm dbnm as indexed_opt on_opt using_opt",
 /* 106 */ "seltablist ::= stl_prefix nm dbnm LP exprlist RP as on_opt using_opt",
 /* 107 */ "seltablist ::= stl_prefix LP select RP as on_opt using_opt",
 /* 108 */ "seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt",
 /* 109 */ "dbnm ::=",
 /* 110 */ "dbnm ::= DOT nm",
 /* 111 */ "fullname ::= nm",
 /* 112 */ "fullname ::= nm DOT nm",
 /* 113 */ "xfullname ::= nm",
 /* 114 */ "xfullname ::= nm DOT nm",
 /* 115 */ "xfullname ::= nm DOT nm AS nm",
 /* 116 */ "xfullname ::= nm AS nm",
 /* 117 */ "joinop ::= COMMA|JOIN",
 /* 118 */ "joinop ::= JOIN_KW JOIN",
 /* 119 */ "joinop ::= JOIN_KW nm JOIN",
 /* 120 */ "joinop ::= JOIN_KW nm nm JOIN",
 /* 121 */ "on_opt ::= ON expr",
 /* 122 */ "on_opt ::=",
 /* 123 */ "indexed_opt ::=",
 /* 124 */ "indexed_opt ::= INDEXED BY nm",
 /* 125 */ "indexed_opt ::= NOT INDEXED",
 /* 126 */ "using_opt ::= USING LP idlist RP",
 /* 127 */ "using_opt ::=",
 /* 128 */ "orderby_opt ::=",
 /* 129 */ "orderby_opt ::= ORDER BY sortlist",
 /* 130 */ "sortlist ::= sortlist COMMA expr sortorder nulls",
 /* 131 */ "sortlist ::= expr sortorder nulls",
 /* 132 */ "sortorder ::= ASC",
 /* 133 */ "sortorder ::= DESC",
 /* 134 */ "sortorder ::=",
 /* 135 */ "nulls ::= NULLS FIRST",
 /* 136 */ "nulls ::= NULLS LAST",
 /* 137 */ "nulls ::=",
 /* 138 */ "groupby_opt ::=",
 /* 139 */ "groupby_opt ::= GROUP BY nexprlist",
 /* 140 */ "having_opt ::=",
 /* 141 */ "having_opt ::= HAVING expr",
 /* 142 */ "limit_opt ::=",
 /* 143 */ "limit_opt ::= LIMIT expr",
 /* 144 */ "limit_opt ::= LIMIT expr OFFSET expr",
 /* 145 */ "limit_opt ::= LIMIT expr COMMA expr",
 /* 146 */ "cmd ::= with DELETE FROM xfullname indexed_opt where_opt",
 /* 147 */ "where_opt ::=",
 /* 148 */ "where_opt ::= WHERE expr",
 /* 149 */ "cmd ::= with UPDATE orconf xfullname indexed_opt SET setlist where_opt",
 /* 150 */ "setlist ::= setlist COMMA nm EQ expr",
 /* 151 */ "setlist ::= setlist COMMA LP idlist RP EQ expr",
 /* 152 */ "setlist ::= nm EQ expr",
 /* 153 */ "setlist ::= LP idlist RP EQ expr",
 /* 154 */ "cmd ::= with insert_cmd INTO xfullname idlist_opt select upsert",
 /* 155 */ "cmd ::= with insert_cmd INTO xfullname idlist_opt DEFAULT VALUES",
 /* 156 */ "upsert ::=",
 /* 157 */ "upsert ::= ON CONFLICT LP sortlist RP where_opt DO UPDATE SET setlist where_opt",
 /* 158 */ "upsert ::= ON CONFLICT LP sortlist RP where_opt DO NOTHING",
 /* 159 */ "upsert ::= ON CONFLICT DO NOTHING",
 /* 160 */ "insert_cmd ::= INSERT orconf",
 /* 161 */ "insert_cmd ::= REPLACE",
 /* 162 */ "idlist_opt ::=",
 /* 163 */ "idlist_opt ::= LP idlist RP",
 /* 164 */ "idlist ::= idlist COMMA nm",
 /* 165 */ "idlist ::= nm",
 /* 166 */ "expr ::= LP expr RP",
 /* 167 */ "expr ::= ID|INDEXED",
 /* 168 */ "expr ::= JOIN_KW",
 /* 169 */ "expr ::= nm DOT nm",
 /* 170 */ "expr ::= nm DOT nm DOT nm",
 /* 171 */ "term ::= NULL|FLOAT|BLOB",
 /* 172 */ "term ::= STRING",
 /* 173 */ "term ::= INTEGER",
 /* 174 */ "expr ::= VARIABLE",
 /* 175 */ "expr ::= expr COLLATE ID|STRING",
 /* 176 */ "expr ::= CAST LP expr AS typetoken RP",
 /* 177 */ "expr ::= ID|INDEXED LP distinct exprlist RP",
 /* 178 */ "expr ::= ID|INDEXED LP STAR RP",
 /* 179 */ "expr ::= ID|INDEXED LP distinct exprlist RP filter_over",
 /* 180 */ "expr ::= ID|INDEXED LP STAR RP filter_over",
 /* 181 */ "term ::= CTIME_KW",
 /* 182 */ "expr ::= LP nexprlist COMMA expr RP",
 /* 183 */ "expr ::= expr AND expr",
 /* 184 */ "expr ::= expr OR expr",
 /* 185 */ "expr ::= expr LT|GT|GE|LE expr",
 /* 186 */ "expr ::= expr EQ|NE expr",
 /* 187 */ "expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr",
 /* 188 */ "expr ::= expr PLUS|MINUS expr",
 /* 189 */ "expr ::= expr STAR|SLASH|REM expr",
 /* 190 */ "expr ::= expr CONCAT expr",
 /* 191 */ "likeop ::= NOT LIKE_KW|MATCH",
 /* 192 */ "expr ::= expr likeop expr",
 /* 193 */ "expr ::= expr likeop expr ESCAPE expr",
 /* 194 */ "expr ::= expr ISNULL|NOTNULL",
 /* 195 */ "expr ::= expr NOT NULL",
 /* 196 */ "expr ::= expr IS expr",
 /* 197 */ "expr ::= expr IS NOT expr",
 /* 198 */ "expr ::= NOT expr",
 /* 199 */ "expr ::= BITNOT expr",
 /* 200 */ "expr ::= PLUS|MINUS expr",
 /* 201 */ "between_op ::= BETWEEN",
 /* 202 */ "between_op ::= NOT BETWEEN",
 /* 203 */ "expr ::= expr between_op expr AND expr",
 /* 204 */ "in_op ::= IN",
 /* 205 */ "in_op ::= NOT IN",
 /* 206 */ "expr ::= expr in_op LP exprlist RP",
 /* 207 */ "expr ::= LP select RP",
 /* 208 */ "expr ::= expr in_op LP select RP",
 /* 209 */ "expr ::= expr in_op nm dbnm paren_exprlist",
 /* 210 */ "expr ::= EXISTS LP select RP",
 /* 211 */ "expr ::= CASE case_operand case_exprlist case_else END",
 /* 212 */ "case_exprlist ::= case_exprlist WHEN expr THEN expr",
 /* 213 */ "case_exprlist ::= WHEN expr THEN expr",
 /* 214 */ "case_else ::= ELSE expr",
 /* 215 */ "case_else ::=",
 /* 216 */ "case_operand ::= expr",
 /* 217 */ "case_operand ::=",
 /* 218 */ "exprlist ::=",
 /* 219 */ "nexprlist ::= nexprlist COMMA expr",
 /* 220 */ "nexprlist ::= expr",
 /* 221 */ "paren_exprlist ::=",
 /* 222 */ "paren_exprlist ::= LP exprlist RP",
 /* 223 */ "cmd ::= createkw uniqueflag INDEX ifnotexists nm dbnm ON nm LP sortlist RP where_opt",
 /* 224 */ "uniqueflag ::= UNIQUE",
 /* 225 */ "uniqueflag ::=",
 /* 226 */ "eidlist_opt ::=",
 /* 227 */ "eidlist_opt ::= LP eidlist RP",
 /* 228 */ "eidlist ::= eidlist COMMA nm collate sortorder",
 /* 229 */ "eidlist ::= nm collate sortorder",
 /* 230 */ "collate ::=",
 /* 231 */ "collate ::= COLLATE ID|STRING",
 /* 232 */ "cmd ::= DROP INDEX ifexists fullname",
 /* 233 */ "cmd ::= VACUUM vinto",
 /* 234 */ "cmd ::= VACUUM nm vinto",
 /* 235 */ "vinto ::= INTO expr",
 /* 236 */ "vinto ::=",
 /* 237 */ "cmd ::= PRAGMA nm dbnm",
 /* 238 */ "cmd ::= PRAGMA nm dbnm EQ nmnum",
 /* 239 */ "cmd ::= PRAGMA nm dbnm LP nmnum RP",
 /* 240 */ "cmd ::= PRAGMA nm dbnm EQ minus_num",
 /* 241 */ "cmd ::= PRAGMA nm dbnm LP minus_num RP",
 /* 242 */ "plus_num ::= PLUS INTEGER|FLOAT",
 /* 243 */ "minus_num ::= MINUS INTEGER|FLOAT",
 /* 244 */ "cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END",
 /* 245 */ "trigger_decl ::= temp TRIGGER ifnotexists nm dbnm trigger_time trigger_event ON fullname foreach_clause when_clause",
 /* 246 */ "trigger_time ::= BEFORE|AFTER",
 /* 247 */ "trigger_time ::= INSTEAD OF",
 /* 248 */ "trigger_time ::=",
 /* 249 */ "trigger_event ::= DELETE|INSERT",
 /* 250 */ "trigger_event ::= UPDATE",
 /* 251 */ "trigger_event ::= UPDATE OF idlist",
 /* 252 */ "when_clause ::=",
 /* 253 */ "when_clause ::= WHEN expr",
 /* 254 */ "trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI",
 /* 255 */ "trigger_cmd_list ::= trigger_cmd SEMI",
 /* 256 */ "trnm ::= nm DOT nm",
 /* 257 */ "tridxby ::= INDEXED BY nm",
 /* 258 */ "tridxby ::= NOT INDEXED",
 /* 259 */ "trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt scanpt",
 /* 260 */ "trigger_cmd ::= scanpt insert_cmd INTO trnm idlist_opt select upsert scanpt",
 /* 261 */ "trigger_cmd ::= DELETE FROM trnm tridxby where_opt scanpt",
 /* 262 */ "trigger_cmd ::= scanpt select scanpt",
 /* 263 */ "expr ::= RAISE LP IGNORE RP",
 /* 264 */ "expr ::= RAISE LP raisetype COMMA nm RP",
 /* 265 */ "raisetype ::= ROLLBACK",
 /* 266 */ "raisetype ::= ABORT",
 /* 267 */ "raisetype ::= FAIL",
 /* 268 */ "cmd ::= DROP TRIGGER ifexists fullname",
 /* 269 */ "cmd ::= ATTACH database_kw_opt expr AS expr key_opt",
 /* 270 */ "cmd ::= DETACH database_kw_opt expr",
 /* 271 */ "key_opt ::=",
 /* 272 */ "key_opt ::= KEY expr",
 /* 273 */ "cmd ::= REINDEX",
 /* 274 */ "cmd ::= REINDEX nm dbnm",
 /* 275 */ "cmd ::= ANALYZE",
 /* 276 */ "cmd ::= ANALYZE nm dbnm",
 /* 277 */ "cmd ::= ALTER TABLE fullname RENAME TO nm",
 /* 278 */ "cmd ::= ALTER TABLE add_column_fullname ADD kwcolumn_opt columnname carglist",
 /* 279 */ "add_column_fullname ::= fullname",
 /* 280 */ "cmd ::= ALTER TABLE fullname RENAME kwcolumn_opt nm TO nm",
 /* 281 */ "cmd ::= create_vtab",
 /* 282 */ "cmd ::= create_vtab LP vtabarglist RP",
 /* 283 */ "create_vtab ::= createkw VIRTUAL TABLE ifnotexists nm dbnm USING nm",
 /* 284 */ "vtabarg ::=",
 /* 285 */ "vtabargtoken ::= ANY",
 /* 286 */ "vtabargtoken ::= lp anylist RP",
 /* 287 */ "lp ::= LP",
 /* 288 */ "with ::= WITH wqlist",
 /* 289 */ "with ::= WITH RECURSIVE wqlist",
 /* 290 */ "wqlist ::= nm eidlist_opt AS LP select RP",
 /* 291 */ "wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP",
 /* 292 */ "windowdefn_list ::= windowdefn",
 /* 293 */ "windowdefn_list ::= windowdefn_list COMMA windowdefn",
 /* 294 */ "windowdefn ::= nm AS LP window RP",
 /* 295 */ "window ::= PARTITION BY nexprlist orderby_opt frame_opt",
 /* 296 */ "window ::= nm PARTITION BY nexprlist orderby_opt frame_opt",
 /* 297 */ "window ::= ORDER BY sortlist frame_opt",
 /* 298 */ "window ::= nm ORDER BY sortlist frame_opt",
 /* 299 */ "window ::= frame_opt",
 /* 300 */ "window ::= nm frame_opt",
 /* 301 */ "frame_opt ::=",
 /* 302 */ "frame_opt ::= range_or_rows frame_bound_s frame_exclude_opt",
 /* 303 */ "frame_opt ::= range_or_rows BETWEEN frame_bound_s AND frame_bound_e frame_exclude_opt",
 /* 304 */ "range_or_rows ::= RANGE|ROWS|GROUPS",
 /* 305 */ "frame_bound_s ::= frame_bound",
 /* 306 */ "frame_bound_s ::= UNBOUNDED PRECEDING",
 /* 307 */ "frame_bound_e ::= frame_bound",
 /* 308 */ "frame_bound_e ::= UNBOUNDED FOLLOWING",
 /* 309 */ "frame_bound ::= expr PRECEDING|FOLLOWING",
 /* 310 */ "frame_bound ::= CURRENT ROW",
 /* 311 */ "frame_exclude_opt ::=",
 /* 312 */ "frame_exclude_opt ::= EXCLUDE frame_exclude",
 /* 313 */ "frame_exclude ::= NO OTHERS",
 /* 314 */ "frame_exclude ::= CURRENT ROW",
 /* 315 */ "frame_exclude ::= GROUP|TIES",
 /* 316 */ "window_clause ::= WINDOW windowdefn_list",
 /* 317 */ "filter_over ::= filter_clause over_clause",
 /* 318 */ "filter_over ::= over_clause",
 /* 319 */ "filter_over ::= filter_clause",
 /* 320 */ "over_clause ::= OVER LP window RP",
 /* 321 */ "over_clause ::= OVER nm",
 /* 322 */ "filter_clause ::= FILTER LP WHERE expr RP",
 /* 323 */ "input ::= cmdlist",
 /* 324 */ "cmdlist ::= cmdlist ecmd",
 /* 325 */ "cmdlist ::= ecmd",
 /* 326 */ "ecmd ::= SEMI",
 /* 327 */ "ecmd ::= cmdx SEMI",
 /* 328 */ "ecmd ::= explain cmdx",
 /* 329 */ "trans_opt ::=",
 /* 330 */ "trans_opt ::= TRANSACTION",
 /* 331 */ "trans_opt ::= TRANSACTION nm",
 /* 332 */ "savepoint_opt ::= SAVEPOINT",
 /* 333 */ "savepoint_opt ::=",
 /* 334 */ "cmd ::= create_table create_table_args",
 /* 335 */ "columnlist ::= columnlist COMMA columnname carglist",
 /* 336 */ "columnlist ::= columnname carglist",
 /* 337 */ "nm ::= ID|INDEXED",
 /* 338 */ "nm ::= STRING",
 /* 339 */ "nm ::= JOIN_KW",
 /* 340 */ "typetoken ::= typename",
 /* 341 */ "typename ::= ID|STRING",
 /* 342 */ "signed ::= plus_num",
 /* 343 */ "signed ::= minus_num",
 /* 344 */ "carglist ::= carglist ccons",
 /* 345 */ "carglist ::=",
 /* 346 */ "ccons ::= NULL onconf",
 /* 347 */ "conslist_opt ::= COMMA conslist",
 /* 348 */ "conslist ::= conslist tconscomma tcons",
 /* 349 */ "conslist ::= tcons",
 /* 350 */ "tconscomma ::=",
 /* 351 */ "defer_subclause_opt ::= defer_subclause",
 /* 352 */ "resolvetype ::= raisetype",
 /* 353 */ "selectnowith ::= oneselect",
 /* 354 */ "oneselect ::= values",
 /* 355 */ "sclp ::= selcollist COMMA",
 /* 356 */ "as ::= ID|STRING",
 /* 357 */ "expr ::= term",
 /* 358 */ "likeop ::= LIKE_KW|MATCH",
 /* 359 */ "exprlist ::= nexprlist",
 /* 360 */ "nmnum ::= plus_num",
 /* 361 */ "nmnum ::= nm",
 /* 362 */ "nmnum ::= ON",
 /* 363 */ "nmnum ::= DELETE",
 /* 364 */ "nmnum ::= DEFAULT",
 /* 365 */ "plus_num ::= INTEGER|FLOAT",
 /* 366 */ "foreach_clause ::=",
 /* 367 */ "foreach_clause ::= FOR EACH ROW",
 /* 368 */ "trnm ::= nm",
 /* 369 */ "tridxby ::=",
 /* 370 */ "database_kw_opt ::= DATABASE",
 /* 371 */ "database_kw_opt ::=",
 /* 372 */ "kwcolumn_opt ::=",
 /* 373 */ "kwcolumn_opt ::= COLUMNKW",
 /* 374 */ "vtabarglist ::= vtabarg",
 /* 375 */ "vtabarglist ::= vtabarglist COMMA vtabarg",
 /* 376 */ "vtabarg ::= vtabarg vtabargtoken",
 /* 377 */ "anylist ::=",
 /* 378 */ "anylist ::= anylist LP anylist RP",
 /* 379 */ "anylist ::= anylist ANY",
 /* 380 */ "with ::=",
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.  Return the number
** of errors.  Return 0 on success.
*/
static int yyGrowStack(yyParser *p){
  int newSize;
  int idx;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 100;
  idx = p->yytos ? (int)(p->yytos - p->yystack) : 0;
  if( p->yystack==&p->yystk0 ){
    pNew = malloc(newSize*sizeof(pNew[0]));
    if( pNew ) pNew[0] = p->yystk0;
  }else{
    pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  }
  if( pNew ){
    p->yystack = pNew;
    p->yytos = &p->yystack[idx];
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows from %d to %d entries.\n",
              yyTracePrompt, p->yystksz, newSize);
    }
#endif
    p->yystksz = newSize;
  }
  return pNew==0; 
}
#endif

/* Datatype of the argument to the memory allocated passed as the
** second argument to sqlite3ParserAlloc() below.  This can be changed by
** putting an appropriate #define in the %include section of the input
** grammar.
*/
#ifndef YYMALLOCARGTYPE
# define YYMALLOCARGTYPE size_t
#endif

/* Initialize a new parser that has already been allocated.
*/
SQLITE_PRIVATE void sqlite3ParserInit(void *yypRawParser sqlite3ParserCTX_PDECL){
  yyParser *yypParser = (yyParser*)yypRawParser;
  sqlite3ParserCTX_STORE
#ifdef YYTRACKMAXSTACKDEPTH
  yypParser->yyhwm = 0;
#endif
#if YYSTACKDEPTH<=0
  yypParser->yytos = NULL;
  yypParser->yystack = NULL;
  yypParser->yystksz = 0;
  if( yyGrowStack(yypParser) ){
    yypParser->yystack = &yypParser->yystk0;
    yypParser->yystksz = 1;
  }
#endif
#ifndef YYNOERRORRECOVERY
  yypParser->yyerrcnt = -1;
#endif
  yypParser->yytos = yypParser->yystack;
  yypParser->yystack[0].stateno = 0;
  yypParser->yystack[0].major = 0;
#if YYSTACKDEPTH>0
  yypParser->yystackEnd = &yypParser->yystack[YYSTACKDEPTH-1];
#endif
}

#ifndef sqlite3Parser_ENGINEALWAYSONSTACK
/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to sqlite3Parser and sqlite3ParserFree.
*/
SQLITE_PRIVATE void *sqlite3ParserAlloc(void *(*mallocProc)(YYMALLOCARGTYPE) sqlite3ParserCTX_PDECL){
  yyParser *yypParser;
  yypParser = (yyParser*)(*mallocProc)( (YYMALLOCARGTYPE)sizeof(yyParser) );
  if( yypParser ){
    sqlite3ParserCTX_STORE
    sqlite3ParserInit(yypParser sqlite3ParserCTX_PARAM);
  }
  return (void*)yypParser;
}
#endif /* sqlite3Parser_ENGINEALWAYSONSTACK */


/* The following function deletes the "minor type" or semantic value
** associated with a symbol.  The symbol can be either a terminal
** or nonterminal. "yymajor" is the symbol code, and "yypminor" is
** a pointer to the value to be deleted.  The code used to do the 
** deletions is derived from the %destructor and/or %token_destructor
** directives of the input grammar.
*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  sqlite3ParserARG_FETCH
  sqlite3ParserCTX_FETCH
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are *not* used
    ** inside the C code.
    */
/********* Begin destructor definitions ***************************************/
    case 198: /* select */
    case 231: /* selectnowith */
    case 232: /* oneselect */
    case 244: /* values */
{
sqlite3SelectDelete(pParse->db, (yypminor->yy25));
}
      break;
    case 209: /* term */
    case 210: /* expr */
    case 238: /* where_opt */
    case 240: /* having_opt */
    case 252: /* on_opt */
    case 268: /* case_operand */
    case 270: /* case_else */
    case 273: /* vinto */
    case 280: /* when_clause */
    case 285: /* key_opt */
    case 299: /* filter_clause */
{
sqlite3ExprDelete(pParse->db, (yypminor->yy46));
}
      break;
    case 214: /* eidlist_opt */
    case 223: /* sortlist */
    case 224: /* eidlist */
    case 236: /* selcollist */
    case 239: /* groupby_opt */
    case 241: /* orderby_opt */
    case 245: /* nexprlist */
    case 246: /* sclp */
    case 254: /* exprlist */
    case 259: /* setlist */
    case 267: /* paren_exprlist */
    case 269: /* case_exprlist */
    case 298: /* part_opt */
{
sqlite3ExprListDelete(pParse->db, (yypminor->yy138));
}
      break;
    case 230: /* fullname */
    case 237: /* from */
    case 248: /* seltablist */
    case 249: /* stl_prefix */
    case 255: /* xfullname */
{
sqlite3SrcListDelete(pParse->db, (yypminor->yy609));
}
      break;
    case 233: /* wqlist */
{
sqlite3WithDelete(pParse->db, (yypminor->yy297));
}
      break;
    case 243: /* window_clause */
    case 294: /* windowdefn_list */
{
sqlite3WindowListDelete(pParse->db, (yypminor->yy455));
}
      break;
    case 253: /* using_opt */
    case 256: /* idlist */
    case 261: /* idlist_opt */
{
sqlite3IdListDelete(pParse->db, (yypminor->yy406));
}
      break;
    case 263: /* filter_over */
    case 295: /* windowdefn */
    case 296: /* window */
    case 297: /* frame_opt */
    case 300: /* over_clause */
{
sqlite3WindowDelete(pParse->db, (yypminor->yy455));
}
      break;
    case 276: /* trigger_cmd_list */
    case 281: /* trigger_cmd */
{
sqlite3DeleteTriggerStep(pParse->db, (yypminor->yy527));
}
      break;
    case 278: /* trigger_event */
{
sqlite3IdListDelete(pParse->db, (yypminor->yy572).b);
}
      break;
    case 302: /* frame_bound */
    case 303: /* frame_bound_s */
    case 304: /* frame_bound_e */
{
sqlite3ExprDelete(pParse->db, (yypminor->yy57).pExpr);
}
      break;
/********* End destructor definitions *****************************************/
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
*/
static void yy_pop_parser_stack(yyParser *pParser){
  yyStackEntry *yytos;
  assert( pParser->yytos!=0 );
  assert( pParser->yytos > pParser->yystack );
  yytos = pParser->yytos--;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yy_destructor(pParser, yytos->major, &yytos->minor);
}

/*
** Clear all secondary memory allocations from the parser
*/
SQLITE_PRIVATE void sqlite3ParserFinalize(void *p){
  yyParser *pParser = (yyParser*)p;
  while( pParser->yytos>pParser->yystack ) yy_pop_parser_stack(pParser);
#if YYSTACKDEPTH<=0
  if( pParser->yystack!=&pParser->yystk0 ) free(pParser->yystack);
#endif
}

#ifndef sqlite3Parser_ENGINEALWAYSONSTACK
/* 
** Deallocate and destroy a parser.  Destructors are called for
** all stack elements before shutting the parser down.
**
** If the YYPARSEFREENEVERNULL macro exists (for example because it
** is defined in a %include section of the input grammar) then it is
** assumed that the input pointer is never NULL.
*/
SQLITE_PRIVATE void sqlite3ParserFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
#ifndef YYPARSEFREENEVERNULL
  if( p==0 ) return;
#endif
  sqlite3ParserFinalize(p);
  (*freeProc)(p);
}
#endif /* sqlite3Parser_ENGINEALWAYSONSTACK */

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
SQLITE_PRIVATE int sqlite3ParserStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyhwm;
}
#endif

/* This array of booleans keeps track of the parser statement
** coverage.  The element yycoverage[X][Y] is set when the parser
** is in state X and has a lookahead token Y.  In a well-tested
** systems, every element of this matrix should end up being set.
*/
#if defined(YYCOVERAGE)
static unsigned char yycoverage[YYNSTATE][YYNTOKEN];
#endif

/*
** Write into out a description of every state/lookahead combination that
**
**   (1)  has not been used by the parser, and
**   (2)  is not a syntax error.
**
** Return the number of missed state/lookahead combinations.
*/
#if defined(YYCOVERAGE)
SQLITE_PRIVATE int sqlite3ParserCoverage(FILE *out){
  int stateno, iLookAhead, i;
  int nMissed = 0;
  for(stateno=0; stateno<YYNSTATE; stateno++){
    i = yy_shift_ofst[stateno];
    for(iLookAhead=0; iLookAhead<YYNTOKEN; iLookAhead++){
      if( yy_lookahead[i+iLookAhead]!=iLookAhead ) continue;
      if( yycoverage[stateno][iLookAhead]==0 ) nMissed++;
      if( out ){
        fprintf(out,"State %d lookahead %s %s\n", stateno,
                yyTokenName[iLookAhead],
                yycoverage[stateno][iLookAhead] ? "ok" : "missed");
      }
    }
  }
  return nMissed;
}
#endif

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
*/
static YYACTIONTYPE yy_find_shift_action(
  YYCODETYPE iLookAhead,    /* The look-ahead token */
  YYACTIONTYPE stateno      /* Current state number */
){
  int i;

  if( stateno>YY_MAX_SHIFT ) return stateno;
  assert( stateno <= YY_SHIFT_COUNT );
#if defined(YYCOVERAGE)
  yycoverage[stateno][iLookAhead] = 1;
#endif
  do{
    i = yy_shift_ofst[stateno];
    assert( i>=0 );
    assert( i<=YY_ACTTAB_COUNT );
    assert( i+YYNTOKEN<=(int)YY_NLOOKAHEAD );
    assert( iLookAhead!=YYNOCODE );
    assert( iLookAhead < YYNTOKEN );
    i += iLookAhead;
    assert( i<(int)YY_NLOOKAHEAD );
    if( yy_lookahead[i]!=iLookAhead ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback;            /* Fallback token */
      assert( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0]) );
      iFallback = yyFallback[iLookAhead];
      if( iFallback!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        assert( yyFallback[iFallback]==0 ); /* Fallback loop must terminate */
        iLookAhead = iFallback;
        continue;
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        assert( j<(int)(sizeof(yy_lookahead)/sizeof(yy_lookahead[0])) );
        if( yy_lookahead[j]==YYWILDCARD && iLookAhead>0 ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead],
               yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
      return yy_default[stateno];
    }else{
      assert( i>=0 && i<sizeof(yy_action)/sizeof(yy_action[0]) );
      return yy_action[i];
    }
  }while(1);
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
*/
static YYACTIONTYPE yy_find_reduce_action(
  YYACTIONTYPE stateno,     /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_COUNT );
#endif
  i = yy_reduce_ofst[stateno];
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_ACTTAB_COUNT );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser){
   sqlite3ParserARG_FETCH
   sqlite3ParserCTX_FETCH
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
/******** Begin %stack_overflow code ******************************************/

  sqlite3ErrorMsg(pParse, "parser stack overflow");
/******** End %stack_overflow code ********************************************/
   sqlite3ParserARG_STORE /* Suppress warning about unused %extra_argument var */
   sqlite3ParserCTX_STORE
}

/*
** Print tracing information for a SHIFT action
*/
#ifndef NDEBUG
static void yyTraceShift(yyParser *yypParser, int yyNewState, const char *zTag){
  if( yyTraceFILE ){
    if( yyNewState<YYNSTATE ){
      fprintf(yyTraceFILE,"%s%s '%s', go to state %d\n",
         yyTracePrompt, zTag, yyTokenName[yypParser->yytos->major],
         yyNewState);
    }else{
      fprintf(yyTraceFILE,"%s%s '%s', pending reduce %d\n",
         yyTracePrompt, zTag, yyTokenName[yypParser->yytos->major],
         yyNewState - YY_MIN_REDUCE);
    }
  }
}
#else
# define yyTraceShift(X,Y,Z)
#endif

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  YYACTIONTYPE yyNewState,      /* The new state to shift in */
  YYCODETYPE yyMajor,           /* The major token to shift in */
  sqlite3ParserTOKENTYPE yyMinor        /* The minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yytos++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
    yypParser->yyhwm++;
    assert( yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack) );
  }
#endif
#if YYSTACKDEPTH>0 
  if( yypParser->yytos>yypParser->yystackEnd ){
    yypParser->yytos--;
    yyStackOverflow(yypParser);
    return;
  }
#else
  if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz] ){
    if( yyGrowStack(yypParser) ){
      yypParser->yytos--;
      yyStackOverflow(yypParser);
      return;
    }
  }
#endif
  if( yyNewState > YY_MAX_SHIFT ){
    yyNewState += YY_MIN_REDUCE - YY_MIN_SHIFTREDUCE;
  }
  yytos = yypParser->yytos;
  yytos->stateno = yyNewState;
  yytos->major = yyMajor;
  yytos->minor.yy0 = yyMinor;
  yyTraceShift(yypParser, yyNewState, "Shift");
}

/* For rule J, yyRuleInfoLhs[J] contains the symbol on the left-hand side
** of that rule */
static const YYCODETYPE yyRuleInfoLhs[] = {
   183,  /* (0) explain ::= EXPLAIN */
   183,  /* (1) explain ::= EXPLAIN QUERY PLAN */
   182,  /* (2) cmdx ::= cmd */
   184,  /* (3) cmd ::= BEGIN transtype trans_opt */
   185,  /* (4) transtype ::= */
   185,  /* (5) transtype ::= DEFERRED */
   185,  /* (6) transtype ::= IMMEDIATE */
   185,  /* (7) transtype ::= EXCLUSIVE */
   184,  /* (8) cmd ::= COMMIT|END trans_opt */
   184,  /* (9) cmd ::= ROLLBACK trans_opt */
   184,  /* (10) cmd ::= SAVEPOINT nm */
   184,  /* (11) cmd ::= RELEASE savepoint_opt nm */
   184,  /* (12) cmd ::= ROLLBACK trans_opt TO savepoint_opt nm */
   189,  /* (13) create_table ::= createkw temp TABLE ifnotexists nm dbnm */
   191,  /* (14) createkw ::= CREATE */
   193,  /* (15) ifnotexists ::= */
   193,  /* (16) ifnotexists ::= IF NOT EXISTS */
   192,  /* (17) temp ::= TEMP */
   192,  /* (18) temp ::= */
   190,  /* (19) create_table_args ::= LP columnlist conslist_opt RP table_options */
   190,  /* (20) create_table_args ::= AS select */
   197,  /* (21) table_options ::= */
   197,  /* (22) table_options ::= WITHOUT nm */
   199,  /* (23) columnname ::= nm typetoken */
   201,  /* (24) typetoken ::= */
   201,  /* (25) typetoken ::= typename LP signed RP */
   201,  /* (26) typetoken ::= typename LP signed COMMA signed RP */
   202,  /* (27) typename ::= typename ID|STRING */
   206,  /* (28) scanpt ::= */
   207,  /* (29) scantok ::= */
   208,  /* (30) ccons ::= CONSTRAINT nm */
   208,  /* (31) ccons ::= DEFAULT scantok term */
   208,  /* (32) ccons ::= DEFAULT LP expr RP */
   208,  /* (33) ccons ::= DEFAULT PLUS scantok term */
   208,  /* (34) ccons ::= DEFAULT MINUS scantok term */
   208,  /* (35) ccons ::= DEFAULT scantok ID|INDEXED */
   208,  /* (36) ccons ::= NOT NULL onconf */
   208,  /* (37) ccons ::= PRIMARY KEY sortorder onconf autoinc */
   208,  /* (38) ccons ::= UNIQUE onconf */
   208,  /* (39) ccons ::= CHECK LP expr RP */
   208,  /* (40) ccons ::= REFERENCES nm eidlist_opt refargs */
   208,  /* (41) ccons ::= defer_subclause */
   208,  /* (42) ccons ::= COLLATE ID|STRING */
   213,  /* (43) autoinc ::= */
   213,  /* (44) autoinc ::= AUTOINCR */
   215,  /* (45) refargs ::= */
   215,  /* (46) refargs ::= refargs refarg */
   217,  /* (47) refarg ::= MATCH nm */
   217,  /* (48) refarg ::= ON INSERT refact */
   217,  /* (49) refarg ::= ON DELETE refact */
   217,  /* (50) refarg ::= ON UPDATE refact */
   218,  /* (51) refact ::= SET NULL */
   218,  /* (52) refact ::= SET DEFAULT */
   218,  /* (53) refact ::= CASCADE */
   218,  /* (54) refact ::= RESTRICT */
   218,  /* (55) refact ::= NO ACTION */
   216,  /* (56) defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt */
   216,  /* (57) defer_subclause ::= DEFERRABLE init_deferred_pred_opt */
   219,  /* (58) init_deferred_pred_opt ::= */
   219,  /* (59) init_deferred_pred_opt ::= INITIALLY DEFERRED */
   219,  /* (60) init_deferred_pred_opt ::= INITIALLY IMMEDIATE */
   196,  /* (61) conslist_opt ::= */
   221,  /* (62) tconscomma ::= COMMA */
   222,  /* (63) tcons ::= CONSTRAINT nm */
   222,  /* (64) tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf */
   222,  /* (65) tcons ::= UNIQUE LP sortlist RP onconf */
   222,  /* (66) tcons ::= CHECK LP expr RP onconf */
   222,  /* (67) tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt */
   225,  /* (68) defer_subclause_opt ::= */
   211,  /* (69) onconf ::= */
   211,  /* (70) onconf ::= ON CONFLICT resolvetype */
   226,  /* (71) orconf ::= */
   226,  /* (72) orconf ::= OR resolvetype */
   227,  /* (73) resolvetype ::= IGNORE */
   227,  /* (74) resolvetype ::= REPLACE */
   184,  /* (75) cmd ::= DROP TABLE ifexists fullname */
   229,  /* (76) ifexists ::= IF EXISTS */
   229,  /* (77) ifexists ::= */
   184,  /* (78) cmd ::= createkw temp VIEW ifnotexists nm dbnm eidlist_opt AS select */
   184,  /* (79) cmd ::= DROP VIEW ifexists fullname */
   184,  /* (80) cmd ::= select */
   198,  /* (81) select ::= WITH wqlist selectnowith */
   198,  /* (82) select ::= WITH RECURSIVE wqlist selectnowith */
   198,  /* (83) select ::= selectnowith */
   231,  /* (84) selectnowith ::= selectnowith multiselect_op oneselect */
   234,  /* (85) multiselect_op ::= UNION */
   234,  /* (86) multiselect_op ::= UNION ALL */
   234,  /* (87) multiselect_op ::= EXCEPT|INTERSECT */
   232,  /* (88) oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt */
   232,  /* (89) oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt window_clause orderby_opt limit_opt */
   244,  /* (90) values ::= VALUES LP nexprlist RP */
   244,  /* (91) values ::= values COMMA LP nexprlist RP */
   235,  /* (92) distinct ::= DISTINCT */
   235,  /* (93) distinct ::= ALL */
   235,  /* (94) distinct ::= */
   246,  /* (95) sclp ::= */
   236,  /* (96) selcollist ::= sclp scanpt expr scanpt as */
   236,  /* (97) selcollist ::= sclp scanpt STAR */
   236,  /* (98) selcollist ::= sclp scanpt nm DOT STAR */
   247,  /* (99) as ::= AS nm */
   247,  /* (100) as ::= */
   237,  /* (101) from ::= */
   237,  /* (102) from ::= FROM seltablist */
   249,  /* (103) stl_prefix ::= seltablist joinop */
   249,  /* (104) stl_prefix ::= */
   248,  /* (105) seltablist ::= stl_prefix nm dbnm as indexed_opt on_opt using_opt */
   248,  /* (106) seltablist ::= stl_prefix nm dbnm LP exprlist RP as on_opt using_opt */
   248,  /* (107) seltablist ::= stl_prefix LP select RP as on_opt using_opt */
   248,  /* (108) seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt */
   194,  /* (109) dbnm ::= */
   194,  /* (110) dbnm ::= DOT nm */
   230,  /* (111) fullname ::= nm */
   230,  /* (112) fullname ::= nm DOT nm */
   255,  /* (113) xfullname ::= nm */
   255,  /* (114) xfullname ::= nm DOT nm */
   255,  /* (115) xfullname ::= nm DOT nm AS nm */
   255,  /* (116) xfullname ::= nm AS nm */
   250,  /* (117) joinop ::= COMMA|JOIN */
   250,  /* (118) joinop ::= JOIN_KW JOIN */
   250,  /* (119) joinop ::= JOIN_KW nm JOIN */
   250,  /* (120) joinop ::= JOIN_KW nm nm JOIN */
   252,  /* (121) on_opt ::= ON expr */
   252,  /* (122) on_opt ::= */
   251,  /* (123) indexed_opt ::= */
   251,  /* (124) indexed_opt ::= INDEXED BY nm */
   251,  /* (125) indexed_opt ::= NOT INDEXED */
   253,  /* (126) using_opt ::= USING LP idlist RP */
   253,  /* (127) using_opt ::= */
   241,  /* (128) orderby_opt ::= */
   241,  /* (129) orderby_opt ::= ORDER BY sortlist */
   223,  /* (130) sortlist ::= sortlist COMMA expr sortorder nulls */
   223,  /* (131) sortlist ::= expr sortorder nulls */
   212,  /* (132) sortorder ::= ASC */
   212,  /* (133) sortorder ::= DESC */
   212,  /* (134) sortorder ::= */
   257,  /* (135) nulls ::= NULLS FIRST */
   257,  /* (136) nulls ::= NULLS LAST */
   257,  /* (137) nulls ::= */
   239,  /* (138) groupby_opt ::= */
   239,  /* (139) groupby_opt ::= GROUP BY nexprlist */
   240,  /* (140) having_opt ::= */
   240,  /* (141) having_opt ::= HAVING expr */
   242,  /* (142) limit_opt ::= */
   242,  /* (143) limit_opt ::= LIMIT expr */
   242,  /* (144) limit_opt ::= LIMIT expr OFFSET expr */
   242,  /* (145) limit_opt ::= LIMIT expr COMMA expr */
   184,  /* (146) cmd ::= with DELETE FROM xfullname indexed_opt where_opt */
   238,  /* (147) where_opt ::= */
   238,  /* (148) where_opt ::= WHERE expr */
   184,  /* (149) cmd ::= with UPDATE orconf xfullname indexed_opt SET setlist where_opt */
   259,  /* (150) setlist ::= setlist COMMA nm EQ expr */
   259,  /* (151) setlist ::= setlist COMMA LP idlist RP EQ expr */
   259,  /* (152) setlist ::= nm EQ expr */
   259,  /* (153) setlist ::= LP idlist RP EQ expr */
   184,  /* (154) cmd ::= with insert_cmd INTO xfullname idlist_opt select upsert */
   184,  /* (155) cmd ::= with insert_cmd INTO xfullname idlist_opt DEFAULT VALUES */
   262,  /* (156) upsert ::= */
   262,  /* (157) upsert ::= ON CONFLICT LP sortlist RP where_opt DO UPDATE SET setlist where_opt */
   262,  /* (158) upsert ::= ON CONFLICT LP sortlist RP where_opt DO NOTHING */
   262,  /* (159) upsert ::= ON CONFLICT DO NOTHING */
   260,  /* (160) insert_cmd ::= INSERT orconf */
   260,  /* (161) insert_cmd ::= REPLACE */
   261,  /* (162) idlist_opt ::= */
   261,  /* (163) idlist_opt ::= LP idlist RP */
   256,  /* (164) idlist ::= idlist COMMA nm */
   256,  /* (165) idlist ::= nm */
   210,  /* (166) expr ::= LP expr RP */
   210,  /* (167) expr ::= ID|INDEXED */
   210,  /* (168) expr ::= JOIN_KW */
   210,  /* (169) expr ::= nm DOT nm */
   210,  /* (170) expr ::= nm DOT nm DOT nm */
   209,  /* (171) term ::= NULL|FLOAT|BLOB */
   209,  /* (172) term ::= STRING */
   209,  /* (173) term ::= INTEGER */
   210,  /* (174) expr ::= VARIABLE */
   210,  /* (175) expr ::= expr COLLATE ID|STRING */
   210,  /* (176) expr ::= CAST LP expr AS typetoken RP */
   210,  /* (177) expr ::= ID|INDEXED LP distinct exprlist RP */
   210,  /* (178) expr ::= ID|INDEXED LP STAR RP */
   210,  /* (179) expr ::= ID|INDEXED LP distinct exprlist RP filter_over */
   210,  /* (180) expr ::= ID|INDEXED LP STAR RP filter_over */
   209,  /* (181) term ::= CTIME_KW */
   210,  /* (182) expr ::= LP nexprlist COMMA expr RP */
   210,  /* (183) expr ::= expr AND expr */
   210,  /* (184) expr ::= expr OR expr */
   210,  /* (185) expr ::= expr LT|GT|GE|LE expr */
   210,  /* (186) expr ::= expr EQ|NE expr */
   210,  /* (187) expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr */
   210,  /* (188) expr ::= expr PLUS|MINUS expr */
   210,  /* (189) expr ::= expr STAR|SLASH|REM expr */
   210,  /* (190) expr ::= expr CONCAT expr */
   264,  /* (191) likeop ::= NOT LIKE_KW|MATCH */
   210,  /* (192) expr ::= expr likeop expr */
   210,  /* (193) expr ::= expr likeop expr ESCAPE expr */
   210,  /* (194) expr ::= expr ISNULL|NOTNULL */
   210,  /* (195) expr ::= expr NOT NULL */
   210,  /* (196) expr ::= expr IS expr */
   210,  /* (197) expr ::= expr IS NOT expr */
   210,  /* (198) expr ::= NOT expr */
   210,  /* (199) expr ::= BITNOT expr */
   210,  /* (200) expr ::= PLUS|MINUS expr */
   265,  /* (201) between_op ::= BETWEEN */
   265,  /* (202) between_op ::= NOT BETWEEN */
   210,  /* (203) expr ::= expr between_op expr AND expr */
   266,  /* (204) in_op ::= IN */
   266,  /* (205) in_op ::= NOT IN */
   210,  /* (206) expr ::= expr in_op LP exprlist RP */
   210,  /* (207) expr ::= LP select RP */
   210,  /* (208) expr ::= expr in_op LP select RP */
   210,  /* (209) expr ::= expr in_op nm dbnm paren_exprlist */
   210,  /* (210) expr ::= EXISTS LP select RP */
   210,  /* (211) expr ::= CASE case_operand case_exprlist case_else END */
   269,  /* (212) case_exprlist ::= case_exprlist WHEN expr THEN expr */
   269,  /* (213) case_exprlist ::= WHEN expr THEN expr */
   270,  /* (214) case_else ::= ELSE expr */
   270,  /* (215) case_else ::= */
   268,  /* (216) case_operand ::= expr */
   268,  /* (217) case_operand ::= */
   254,  /* (218) exprlist ::= */
   245,  /* (219) nexprlist ::= nexprlist COMMA expr */
   245,  /* (220) nexprlist ::= expr */
   267,  /* (221) paren_exprlist ::= */
   267,  /* (222) paren_exprlist ::= LP exprlist RP */
   184,  /* (223) cmd ::= createkw uniqueflag INDEX ifnotexists nm dbnm ON nm LP sortlist RP where_opt */
   271,  /* (224) uniqueflag ::= UNIQUE */
   271,  /* (225) uniqueflag ::= */
   214,  /* (226) eidlist_opt ::= */
   214,  /* (227) eidlist_opt ::= LP eidlist RP */
   224,  /* (228) eidlist ::= eidlist COMMA nm collate sortorder */
   224,  /* (229) eidlist ::= nm collate sortorder */
   272,  /* (230) collate ::= */
   272,  /* (231) collate ::= COLLATE ID|STRING */
   184,  /* (232) cmd ::= DROP INDEX ifexists fullname */
   184,  /* (233) cmd ::= VACUUM vinto */
   184,  /* (234) cmd ::= VACUUM nm vinto */
   273,  /* (235) vinto ::= INTO expr */
   273,  /* (236) vinto ::= */
   184,  /* (237) cmd ::= PRAGMA nm dbnm */
   184,  /* (238) cmd ::= PRAGMA nm dbnm EQ nmnum */
   184,  /* (239) cmd ::= PRAGMA nm dbnm LP nmnum RP */
   184,  /* (240) cmd ::= PRAGMA nm dbnm EQ minus_num */
   184,  /* (241) cmd ::= PRAGMA nm dbnm LP minus_num RP */
   204,  /* (242) plus_num ::= PLUS INTEGER|FLOAT */
   205,  /* (243) minus_num ::= MINUS INTEGER|FLOAT */
   184,  /* (244) cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END */
   275,  /* (245) trigger_decl ::= temp TRIGGER ifnotexists nm dbnm trigger_time trigger_event ON fullname foreach_clause when_clause */
   277,  /* (246) trigger_time ::= BEFORE|AFTER */
   277,  /* (247) trigger_time ::= INSTEAD OF */
   277,  /* (248) trigger_time ::= */
   278,  /* (249) trigger_event ::= DELETE|INSERT */
   278,  /* (250) trigger_event ::= UPDATE */
   278,  /* (251) trigger_event ::= UPDATE OF idlist */
   280,  /* (252) when_clause ::= */
   280,  /* (253) when_clause ::= WHEN expr */
   276,  /* (254) trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI */
   276,  /* (255) trigger_cmd_list ::= trigger_cmd SEMI */
   282,  /* (256) trnm ::= nm DOT nm */
   283,  /* (257) tridxby ::= INDEXED BY nm */
   283,  /* (258) tridxby ::= NOT INDEXED */
   281,  /* (259) trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt scanpt */
   281,  /* (260) trigger_cmd ::= scanpt insert_cmd INTO trnm idlist_opt select upsert scanpt */
   281,  /* (261) trigger_cmd ::= DELETE FROM trnm tridxby where_opt scanpt */
   281,  /* (262) trigger_cmd ::= scanpt select scanpt */
   210,  /* (263) expr ::= RAISE LP IGNORE RP */
   210,  /* (264) expr ::= RAISE LP raisetype COMMA nm RP */
   228,  /* (265) raisetype ::= ROLLBACK */
   228,  /* (266) raisetype ::= ABORT */
   228,  /* (267) raisetype ::= FAIL */
   184,  /* (268) cmd ::= DROP TRIGGER ifexists fullname */
   184,  /* (269) cmd ::= ATTACH database_kw_opt expr AS expr key_opt */
   184,  /* (270) cmd ::= DETACH database_kw_opt expr */
   285,  /* (271) key_opt ::= */
   285,  /* (272) key_opt ::= KEY expr */
   184,  /* (273) cmd ::= REINDEX */
   184,  /* (274) cmd ::= REINDEX nm dbnm */
   184,  /* (275) cmd ::= ANALYZE */
   184,  /* (276) cmd ::= ANALYZE nm dbnm */
   184,  /* (277) cmd ::= ALTER TABLE fullname RENAME TO nm */
   184,  /* (278) cmd ::= ALTER TABLE add_column_fullname ADD kwcolumn_opt columnname carglist */
   286,  /* (279) add_column_fullname ::= fullname */
   184,  /* (280) cmd ::= ALTER TABLE fullname RENAME kwcolumn_opt nm TO nm */
   184,  /* (281) cmd ::= create_vtab */
   184,  /* (282) cmd ::= create_vtab LP vtabarglist RP */
   288,  /* (283) create_vtab ::= createkw VIRTUAL TABLE ifnotexists nm dbnm USING nm */
   290,  /* (284) vtabarg ::= */
   291,  /* (285) vtabargtoken ::= ANY */
   291,  /* (286) vtabargtoken ::= lp anylist RP */
   292,  /* (287) lp ::= LP */
   258,  /* (288) with ::= WITH wqlist */
   258,  /* (289) with ::= WITH RECURSIVE wqlist */
   233,  /* (290) wqlist ::= nm eidlist_opt AS LP select RP */
   233,  /* (291) wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP */
   294,  /* (292) windowdefn_list ::= windowdefn */
   294,  /* (293) windowdefn_list ::= windowdefn_list COMMA windowdefn */
   295,  /* (294) windowdefn ::= nm AS LP window RP */
   296,  /* (295) window ::= PARTITION BY nexprlist orderby_opt frame_opt */
   296,  /* (296) window ::= nm PARTITION BY nexprlist orderby_opt frame_opt */
   296,  /* (297) window ::= ORDER BY sortlist frame_opt */
   296,  /* (298) window ::= nm ORDER BY sortlist frame_opt */
   296,  /* (299) window ::= frame_opt */
   296,  /* (300) window ::= nm frame_opt */
   297,  /* (301) frame_opt ::= */
   297,  /* (302) frame_opt ::= range_or_rows frame_bound_s frame_exclude_opt */
   297,  /* (303) frame_opt ::= range_or_rows BETWEEN frame_bound_s AND frame_bound_e frame_exclude_opt */
   301,  /* (304) range_or_rows ::= RANGE|ROWS|GROUPS */
   303,  /* (305) frame_bound_s ::= frame_bound */
   303,  /* (306) frame_bound_s ::= UNBOUNDED PRECEDING */
   304,  /* (307) frame_bound_e ::= frame_bound */
   304,  /* (308) frame_bound_e ::= UNBOUNDED FOLLOWING */
   302,  /* (309) frame_bound ::= expr PRECEDING|FOLLOWING */
   302,  /* (310) frame_bound ::= CURRENT ROW */
   305,  /* (311) frame_exclude_opt ::= */
   305,  /* (312) frame_exclude_opt ::= EXCLUDE frame_exclude */
   306,  /* (313) frame_exclude ::= NO OTHERS */
   306,  /* (314) frame_exclude ::= CURRENT ROW */
   306,  /* (315) frame_exclude ::= GROUP|TIES */
   243,  /* (316) window_clause ::= WINDOW windowdefn_list */
   263,  /* (317) filter_over ::= filter_clause over_clause */
   263,  /* (318) filter_over ::= over_clause */
   263,  /* (319) filter_over ::= filter_clause */
   300,  /* (320) over_clause ::= OVER LP window RP */
   300,  /* (321) over_clause ::= OVER nm */
   299,  /* (322) filter_clause ::= FILTER LP WHERE expr RP */
   179,  /* (323) input ::= cmdlist */
   180,  /* (324) cmdlist ::= cmdlist ecmd */
   180,  /* (325) cmdlist ::= ecmd */
   181,  /* (326) ecmd ::= SEMI */
   181,  /* (327) ecmd ::= cmdx SEMI */
   181,  /* (328) ecmd ::= explain cmdx */
   186,  /* (329) trans_opt ::= */
   186,  /* (330) trans_opt ::= TRANSACTION */
   186,  /* (331) trans_opt ::= TRANSACTION nm */
   188,  /* (332) savepoint_opt ::= SAVEPOINT */
   188,  /* (333) savepoint_opt ::= */
   184,  /* (334) cmd ::= create_table create_table_args */
   195,  /* (335) columnlist ::= columnlist COMMA columnname carglist */
   195,  /* (336) columnlist ::= columnname carglist */
   187,  /* (337) nm ::= ID|INDEXED */
   187,  /* (338) nm ::= STRING */
   187,  /* (339) nm ::= JOIN_KW */
   201,  /* (340) typetoken ::= typename */
   202,  /* (341) typename ::= ID|STRING */
   203,  /* (342) signed ::= plus_num */
   203,  /* (343) signed ::= minus_num */
   200,  /* (344) carglist ::= carglist ccons */
   200,  /* (345) carglist ::= */
   208,  /* (346) ccons ::= NULL onconf */
   196,  /* (347) conslist_opt ::= COMMA conslist */
   220,  /* (348) conslist ::= conslist tconscomma tcons */
   220,  /* (349) conslist ::= tcons */
   221,  /* (350) tconscomma ::= */
   225,  /* (351) defer_subclause_opt ::= defer_subclause */
   227,  /* (352) resolvetype ::= raisetype */
   231,  /* (353) selectnowith ::= oneselect */
   232,  /* (354) oneselect ::= values */
   246,  /* (355) sclp ::= selcollist COMMA */
   247,  /* (356) as ::= ID|STRING */
   210,  /* (357) expr ::= term */
   264,  /* (358) likeop ::= LIKE_KW|MATCH */
   254,  /* (359) exprlist ::= nexprlist */
   274,  /* (360) nmnum ::= plus_num */
   274,  /* (361) nmnum ::= nm */
   274,  /* (362) nmnum ::= ON */
   274,  /* (363) nmnum ::= DELETE */
   274,  /* (364) nmnum ::= DEFAULT */
   204,  /* (365) plus_num ::= INTEGER|FLOAT */
   279,  /* (366) foreach_clause ::= */
   279,  /* (367) foreach_clause ::= FOR EACH ROW */
   282,  /* (368) trnm ::= nm */
   283,  /* (369) tridxby ::= */
   284,  /* (370) database_kw_opt ::= DATABASE */
   284,  /* (371) database_kw_opt ::= */
   287,  /* (372) kwcolumn_opt ::= */
   287,  /* (373) kwcolumn_opt ::= COLUMNKW */
   289,  /* (374) vtabarglist ::= vtabarg */
   289,  /* (375) vtabarglist ::= vtabarglist COMMA vtabarg */
   290,  /* (376) vtabarg ::= vtabarg vtabargtoken */
   293,  /* (377) anylist ::= */
   293,  /* (378) anylist ::= anylist LP anylist RP */
   293,  /* (379) anylist ::= anylist ANY */
   258,  /* (380) with ::= */
};

/* For rule J, yyRuleInfoNRhs[J] contains the negative of the number
** of symbols on the right-hand side of that rule. */
static const signed char yyRuleInfoNRhs[] = {
   -1,  /* (0) explain ::= EXPLAIN */
   -3,  /* (1) explain ::= EXPLAIN QUERY PLAN */
   -1,  /* (2) cmdx ::= cmd */
   -3,  /* (3) cmd ::= BEGIN transtype trans_opt */
    0,  /* (4) transtype ::= */
   -1,  /* (5) transtype ::= DEFERRED */
   -1,  /* (6) transtype ::= IMMEDIATE */
   -1,  /* (7) transtype ::= EXCLUSIVE */
   -2,  /* (8) cmd ::= COMMIT|END trans_opt */
   -2,  /* (9) cmd ::= ROLLBACK trans_opt */
   -2,  /* (10) cmd ::= SAVEPOINT nm */
   -3,  /* (11) cmd ::= RELEASE savepoint_opt nm */
   -5,  /* (12) cmd ::= ROLLBACK trans_opt TO savepoint_opt nm */
   -6,  /* (13) create_table ::= createkw temp TABLE ifnotexists nm dbnm */
   -1,  /* (14) createkw ::= CREATE */
    0,  /* (15) ifnotexists ::= */
   -3,  /* (16) ifnotexists ::= IF NOT EXISTS */
   -1,  /* (17) temp ::= TEMP */
    0,  /* (18) temp ::= */
   -5,  /* (19) create_table_args ::= LP columnlist conslist_opt RP table_options */
   -2,  /* (20) create_table_args ::= AS select */
    0,  /* (21) table_options ::= */
   -2,  /* (22) table_options ::= WITHOUT nm */
   -2,  /* (23) columnname ::= nm typetoken */
    0,  /* (24) typetoken ::= */
   -4,  /* (25) typetoken ::= typename LP signed RP */
   -6,  /* (26) typetoken ::= typename LP signed COMMA signed RP */
   -2,  /* (27) typename ::= typename ID|STRING */
    0,  /* (28) scanpt ::= */
    0,  /* (29) scantok ::= */
   -2,  /* (30) ccons ::= CONSTRAINT nm */
   -3,  /* (31) ccons ::= DEFAULT scantok term */
   -4,  /* (32) ccons ::= DEFAULT LP expr RP */
   -4,  /* (33) ccons ::= DEFAULT PLUS scantok term */
   -4,  /* (34) ccons ::= DEFAULT MINUS scantok term */
   -3,  /* (35) ccons ::= DEFAULT scantok ID|INDEXED */
   -3,  /* (36) ccons ::= NOT NULL onconf */
   -5,  /* (37) ccons ::= PRIMARY KEY sortorder onconf autoinc */
   -2,  /* (38) ccons ::= UNIQUE onconf */
   -4,  /* (39) ccons ::= CHECK LP expr RP */
   -4,  /* (40) ccons ::= REFERENCES nm eidlist_opt refargs */
   -1,  /* (41) ccons ::= defer_subclause */
   -2,  /* (42) ccons ::= COLLATE ID|STRING */
    0,  /* (43) autoinc ::= */
   -1,  /* (44) autoinc ::= AUTOINCR */
    0,  /* (45) refargs ::= */
   -2,  /* (46) refargs ::= refargs refarg */
   -2,  /* (47) refarg ::= MATCH nm */
   -3,  /* (48) refarg ::= ON INSERT refact */
   -3,  /* (49) refarg ::= ON DELETE refact */
   -3,  /* (50) refarg ::= ON UPDATE refact */
   -2,  /* (51) refact ::= SET NULL */
   -2,  /* (52) refact ::= SET DEFAULT */
   -1,  /* (53) refact ::= CASCADE */
   -1,  /* (54) refact ::= RESTRICT */
   -2,  /* (55) refact ::= NO ACTION */
   -3,  /* (56) defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt */
   -2,  /* (57) defer_subclause ::= DEFERRABLE init_deferred_pred_opt */
    0,  /* (58) init_deferred_pred_opt ::= */
   -2,  /* (59) init_deferred_pred_opt ::= INITIALLY DEFERRED */
   -2,  /* (60) init_deferred_pred_opt ::= INITIALLY IMMEDIATE */
    0,  /* (61) conslist_opt ::= */
   -1,  /* (62) tconscomma ::= COMMA */
   -2,  /* (63) tcons ::= CONSTRAINT nm */
   -7,  /* (64) tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf */
   -5,  /* (65) tcons ::= UNIQUE LP sortlist RP onconf */
   -5,  /* (66) tcons ::= CHECK LP expr RP onconf */
  -10,  /* (67) tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt */
    0,  /* (68) defer_subclause_opt ::= */
    0,  /* (69) onconf ::= */
   -3,  /* (70) onconf ::= ON CONFLICT resolvetype */
    0,  /* (71) orconf ::= */
   -2,  /* (72) orconf ::= OR resolvetype */
   -1,  /* (73) resolvetype ::= IGNORE */
   -1,  /* (74) resolvetype ::= REPLACE */
   -4,  /* (75) cmd ::= DROP TABLE ifexists fullname */
   -2,  /* (76) ifexists ::= IF EXISTS */
    0,  /* (77) ifexists ::= */
   -9,  /* (78) cmd ::= createkw temp VIEW ifnotexists nm dbnm eidlist_opt AS select */
   -4,  /* (79) cmd ::= DROP VIEW ifexists fullname */
   -1,  /* (80) cmd ::= select */
   -3,  /* (81) select ::= WITH wqlist selectnowith */
   -4,  /* (82) select ::= WITH RECURSIVE wqlist selectnowith */
   -1,  /* (83) select ::= selectnowith */
   -3,  /* (84) selectnowith ::= selectnowith multiselect_op oneselect */
   -1,  /* (85) multiselect_op ::= UNION */
   -2,  /* (86) multiselect_op ::= UNION ALL */
   -1,  /* (87) multiselect_op ::= EXCEPT|INTERSECT */
   -9,  /* (88) oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt */
  -10,  /* (89) oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt window_clause orderby_opt limit_opt */
   -4,  /* (90) values ::= VALUES LP nexprlist RP */
   -5,  /* (91) values ::= values COMMA LP nexprlist RP */
   -1,  /* (92) distinct ::= DISTINCT */
   -1,  /* (93) distinct ::= ALL */
    0,  /* (94) distinct ::= */
    0,  /* (95) sclp ::= */
   -5,  /* (96) selcollist ::= sclp scanpt expr scanpt as */
   -3,  /* (97) selcollist ::= sclp scanpt STAR */
   -5,  /* (98) selcollist ::= sclp scanpt nm DOT STAR */
   -2,  /* (99) as ::= AS nm */
    0,  /* (100) as ::= */
    0,  /* (101) from ::= */
   -2,  /* (102) from ::= FROM seltablist */
   -2,  /* (103) stl_prefix ::= seltablist joinop */
    0,  /* (104) stl_prefix ::= */
   -7,  /* (105) seltablist ::= stl_prefix nm dbnm as indexed_opt on_opt using_opt */
   -9,  /* (106) seltablist ::= stl_prefix nm dbnm LP exprlist RP as on_opt using_opt */
   -7,  /* (107) seltablist ::= stl_prefix LP select RP as on_opt using_opt */
   -7,  /* (108) seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt */
    0,  /* (109) dbnm ::= */
   -2,  /* (110) dbnm ::= DOT nm */
   -1,  /* (111) fullname ::= nm */
   -3,  /* (112) fullname ::= nm DOT nm */
   -1,  /* (113) xfullname ::= nm */
   -3,  /* (114) xfullname ::= nm DOT nm */
   -5,  /* (115) xfullname ::= nm DOT nm AS nm */
   -3,  /* (116) xfullname ::= nm AS nm */
   -1,  /* (117) joinop ::= COMMA|JOIN */
   -2,  /* (118) joinop ::= JOIN_KW JOIN */
   -3,  /* (119) joinop ::= JOIN_KW nm JOIN */
   -4,  /* (120) joinop ::= JOIN_KW nm nm JOIN */
   -2,  /* (121) on_opt ::= ON expr */
    0,  /* (122) on_opt ::= */
    0,  /* (123) indexed_opt ::= */
   -3,  /* (124) indexed_opt ::= INDEXED BY nm */
   -2,  /* (125) indexed_opt ::= NOT INDEXED */
   -4,  /* (126) using_opt ::= USING LP idlist RP */
    0,  /* (127) using_opt ::= */
    0,  /* (128) orderby_opt ::= */
   -3,  /* (129) orderby_opt ::= ORDER BY sortlist */
   -5,  /* (130) sortlist ::= sortlist COMMA expr sortorder nulls */
   -3,  /* (131) sortlist ::= expr sortorder nulls */
   -1,  /* (132) sortorder ::= ASC */
   -1,  /* (133) sortorder ::= DESC */
    0,  /* (134) sortorder ::= */
   -2,  /* (135) nulls ::= NULLS FIRST */
   -2,  /* (136) nulls ::= NULLS LAST */
    0,  /* (137) nulls ::= */
    0,  /* (138) groupby_opt ::= */
   -3,  /* (139) groupby_opt ::= GROUP BY nexprlist */
    0,  /* (140) having_opt ::= */
   -2,  /* (141) having_opt ::= HAVING expr */
    0,  /* (142) limit_opt ::= */
   -2,  /* (143) limit_opt ::= LIMIT expr */
   -4,  /* (144) limit_opt ::= LIMIT expr OFFSET expr */
   -4,  /* (145) limit_opt ::= LIMIT expr COMMA expr */
   -6,  /* (146) cmd ::= with DELETE FROM xfullname indexed_opt where_opt */
    0,  /* (147) where_opt ::= */
   -2,  /* (148) where_opt ::= WHERE expr */
   -8,  /* (149) cmd ::= with UPDATE orconf xfullname indexed_opt SET setlist where_opt */
   -5,  /* (150) setlist ::= setlist COMMA nm EQ expr */
   -7,  /* (151) setlist ::= setlist COMMA LP idlist RP EQ expr */
   -3,  /* (152) setlist ::= nm EQ expr */
   -5,  /* (153) setlist ::= LP idlist RP EQ expr */
   -7,  /* (154) cmd ::= with insert_cmd INTO xfullname idlist_opt select upsert */
   -7,  /* (155) cmd ::= with insert_cmd INTO xfullname idlist_opt DEFAULT VALUES */
    0,  /* (156) upsert ::= */
  -11,  /* (157) upsert ::= ON CONFLICT LP sortlist RP where_opt DO UPDATE SET setlist where_opt */
   -8,  /* (158) upsert ::= ON CONFLICT LP sortlist RP where_opt DO NOTHING */
   -4,  /* (159) upsert ::= ON CONFLICT DO NOTHING */
   -2,  /* (160) insert_cmd ::= INSERT orconf */
   -1,  /* (161) insert_cmd ::= REPLACE */
    0,  /* (162) idlist_opt ::= */
   -3,  /* (163) idlist_opt ::= LP idlist RP */
   -3,  /* (164) idlist ::= idlist COMMA nm */
   -1,  /* (165) idlist ::= nm */
   -3,  /* (166) expr ::= LP expr RP */
   -1,  /* (167) expr ::= ID|INDEXED */
   -1,  /* (168) expr ::= JOIN_KW */
   -3,  /* (169) expr ::= nm DOT nm */
   -5,  /* (170) expr ::= nm DOT nm DOT nm */
   -1,  /* (171) term ::= NULL|FLOAT|BLOB */
   -1,  /* (172) term ::= STRING */
   -1,  /* (173) term ::= INTEGER */
   -1,  /* (174) expr ::= VARIABLE */
   -3,  /* (175) expr ::= expr COLLATE ID|STRING */
   -6,  /* (176) expr ::= CAST LP expr AS typetoken RP */
   -5,  /* (177) expr ::= ID|INDEXED LP distinct exprlist RP */
   -4,  /* (178) expr ::= ID|INDEXED LP STAR RP */
   -6,  /* (179) expr ::= ID|INDEXED LP distinct exprlist RP filter_over */
   -5,  /* (180) expr ::= ID|INDEXED LP STAR RP filter_over */
   -1,  /* (181) term ::= CTIME_KW */
   -5,  /* (182) expr ::= LP nexprlist COMMA expr RP */
   -3,  /* (183) expr ::= expr AND expr */
   -3,  /* (184) expr ::= expr OR expr */
   -3,  /* (185) expr ::= expr LT|GT|GE|LE expr */
   -3,  /* (186) expr ::= expr EQ|NE expr */
   -3,  /* (187) expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr */
   -3,  /* (188) expr ::= expr PLUS|MINUS expr */
   -3,  /* (189) expr ::= expr STAR|SLASH|REM expr */
   -3,  /* (190) expr ::= expr CONCAT expr */
   -2,  /* (191) likeop ::= NOT LIKE_KW|MATCH */
   -3,  /* (192) expr ::= expr likeop expr */
   -5,  /* (193) expr ::= expr likeop expr ESCAPE expr */
   -2,  /* (194) expr ::= expr ISNULL|NOTNULL */
   -3,  /* (195) expr ::= expr NOT NULL */
   -3,  /* (196) expr ::= expr IS expr */
   -4,  /* (197) expr ::= expr IS NOT expr */
   -2,  /* (198) expr ::= NOT expr */
   -2,  /* (199) expr ::= BITNOT expr */
   -2,  /* (200) expr ::= PLUS|MINUS expr */
   -1,  /* (201) between_op ::= BETWEEN */
   -2,  /* (202) between_op ::= NOT BETWEEN */
   -5,  /* (203) expr ::= expr between_op expr AND expr */
   -1,  /* (204) in_op ::= IN */
   -2,  /* (205) in_op ::= NOT IN */
   -5,  /* (206) expr ::= expr in_op LP exprlist RP */
   -3,  /* (207) expr ::= LP select RP */
   -5,  /* (208) expr ::= expr in_op LP select RP */
   -5,  /* (209) expr ::= expr in_op nm dbnm paren_exprlist */
   -4,  /* (210) expr ::= EXISTS LP select RP */
   -5,  /* (211) expr ::= CASE case_operand case_exprlist case_else END */
   -5,  /* (212) case_exprlist ::= case_exprlist WHEN expr THEN expr */
   -4,  /* (213) case_exprlist ::= WHEN expr THEN expr */
   -2,  /* (214) case_else ::= ELSE expr */
    0,  /* (215) case_else ::= */
   -1,  /* (216) case_operand ::= expr */
    0,  /* (217) case_operand ::= */
    0,  /* (218) exprlist ::= */
   -3,  /* (219) nexprlist ::= nexprlist COMMA expr */
   -1,  /* (220) nexprlist ::= expr */
    0,  /* (221) paren_exprlist ::= */
   -3,  /* (222) paren_exprlist ::= LP exprlist RP */
  -12,  /* (223) cmd ::= createkw uniqueflag INDEX ifnotexists nm dbnm ON nm LP sortlist RP where_opt */
   -1,  /* (224) uniqueflag ::= UNIQUE */
    0,  /* (225) uniqueflag ::= */
    0,  /* (226) eidlist_opt ::= */
   -3,  /* (227) eidlist_opt ::= LP eidlist RP */
   -5,  /* (228) eidlist ::= eidlist COMMA nm collate sortorder */
   -3,  /* (229) eidlist ::= nm collate sortorder */
    0,  /* (230) collate ::= */
   -2,  /* (231) collate ::= COLLATE ID|STRING */
   -4,  /* (232) cmd ::= DROP INDEX ifexists fullname */
   -2,  /* (233) cmd ::= VACUUM vinto */
   -3,  /* (234) cmd ::= VACUUM nm vinto */
   -2,  /* (235) vinto ::= INTO expr */
    0,  /* (236) vinto ::= */
   -3,  /* (237) cmd ::= PRAGMA nm dbnm */
   -5,  /* (238) cmd ::= PRAGMA nm dbnm EQ nmnum */
   -6,  /* (239) cmd ::= PRAGMA nm dbnm LP nmnum RP */
   -5,  /* (240) cmd ::= PRAGMA nm dbnm EQ minus_num */
   -6,  /* (241) cmd ::= PRAGMA nm dbnm LP minus_num RP */
   -2,  /* (242) plus_num ::= PLUS INTEGER|FLOAT */
   -2,  /* (243) minus_num ::= MINUS INTEGER|FLOAT */
   -5,  /* (244) cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END */
  -11,  /* (245) trigger_decl ::= temp TRIGGER ifnotexists nm dbnm trigger_time trigger_event ON fullname foreach_clause when_clause */
   -1,  /* (246) trigger_time ::= BEFORE|AFTER */
   -2,  /* (247) trigger_time ::= INSTEAD OF */
    0,  /* (248) trigger_time ::= */
   -1,  /* (249) trigger_event ::= DELETE|INSERT */
   -1,  /* (250) trigger_event ::= UPDATE */
   -3,  /* (251) trigger_event ::= UPDATE OF idlist */
    0,  /* (252) when_clause ::= */
   -2,  /* (253) when_clause ::= WHEN expr */
   -3,  /* (254) trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI */
   -2,  /* (255) trigger_cmd_list ::= trigger_cmd SEMI */
   -3,  /* (256) trnm ::= nm DOT nm */
   -3,  /* (257) tridxby ::= INDEXED BY nm */
   -2,  /* (258) tridxby ::= NOT INDEXED */
   -8,  /* (259) trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt scanpt */
   -8,  /* (260) trigger_cmd ::= scanpt insert_cmd INTO trnm idlist_opt select upsert scanpt */
   -6,  /* (261) trigger_cmd ::= DELETE FROM trnm tridxby where_opt scanpt */
   -3,  /* (262) trigger_cmd ::= scanpt select scanpt */
   -4,  /* (263) expr ::= RAISE LP IGNORE RP */
   -6,  /* (264) expr ::= RAISE LP raisetype COMMA nm RP */
   -1,  /* (265) raisetype ::= ROLLBACK */
   -1,  /* (266) raisetype ::= ABORT */
   -1,  /* (267) raisetype ::= FAIL */
   -4,  /* (268) cmd ::= DROP TRIGGER ifexists fullname */
   -6,  /* (269) cmd ::= ATTACH database_kw_opt expr AS expr key_opt */
   -3,  /* (270) cmd ::= DETACH database_kw_opt expr */
    0,  /* (271) key_opt ::= */
   -2,  /* (272) key_opt ::= KEY expr */
   -1,  /* (273) cmd ::= REINDEX */
   -3,  /* (274) cmd ::= REINDEX nm dbnm */
   -1,  /* (275) cmd ::= ANALYZE */
   -3,  /* (276) cmd ::= ANALYZE nm dbnm */
   -6,  /* (277) cmd ::= ALTER TABLE fullname RENAME TO nm */
   -7,  /* (278) cmd ::= ALTER TABLE add_column_fullname ADD kwcolumn_opt columnname carglist */
   -1,  /* (279) add_column_fullname ::= fullname */
   -8,  /* (280) cmd ::= ALTER TABLE fullname RENAME kwcolumn_opt nm TO nm */
   -1,  /* (281) cmd ::= create_vtab */
   -4,  /* (282) cmd ::= create_vtab LP vtabarglist RP */
   -8,  /* (283) create_vtab ::= createkw VIRTUAL TABLE ifnotexists nm dbnm USING nm */
    0,  /* (284) vtabarg ::= */
   -1,  /* (285) vtabargtoken ::= ANY */
   -3,  /* (286) vtabargtoken ::= lp anylist RP */
   -1,  /* (287) lp ::= LP */
   -2,  /* (288) with ::= WITH wqlist */
   -3,  /* (289) with ::= WITH RECURSIVE wqlist */
   -6,  /* (290) wqlist ::= nm eidlist_opt AS LP select RP */
   -8,  /* (291) wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP */
   -1,  /* (292) windowdefn_list ::= windowdefn */
   -3,  /* (293) windowdefn_list ::= windowdefn_list COMMA windowdefn */
   -5,  /* (294) windowdefn ::= nm AS LP window RP */
   -5,  /* (295) window ::= PARTITION BY nexprlist orderby_opt frame_opt */
   -6,  /* (296) window ::= nm PARTITION BY nexprlist orderby_opt frame_opt */
   -4,  /* (297) window ::= ORDER BY sortlist frame_opt */
   -5,  /* (298) window ::= nm ORDER BY sortlist frame_opt */
   -1,  /* (299) window ::= frame_opt */
   -2,  /* (300) window ::= nm frame_opt */
    0,  /* (301) frame_opt ::= */
   -3,  /* (302) frame_opt ::= range_or_rows frame_bound_s frame_exclude_opt */
   -6,  /* (303) frame_opt ::= range_or_rows BETWEEN frame_bound_s AND frame_bound_e frame_exclude_opt */
   -1,  /* (304) range_or_rows ::= RANGE|ROWS|GROUPS */
   -1,  /* (305) frame_bound_s ::= frame_bound */
   -2,  /* (306) frame_bound_s ::= UNBOUNDED PRECEDING */
   -1,  /* (307) frame_bound_e ::= frame_bound */
   -2,  /* (308) frame_bound_e ::= UNBOUNDED FOLLOWING */
   -2,  /* (309) frame_bound ::= expr PRECEDING|FOLLOWING */
   -2,  /* (310) frame_bound ::= CURRENT ROW */
    0,  /* (311) frame_exclude_opt ::= */
   -2,  /* (312) frame_exclude_opt ::= EXCLUDE frame_exclude */
   -2,  /* (313) frame_exclude ::= NO OTHERS */
   -2,  /* (314) frame_exclude ::= CURRENT ROW */
   -1,  /* (315) frame_exclude ::= GROUP|TIES */
   -2,  /* (316) window_clause ::= WINDOW windowdefn_list */
   -2,  /* (317) filter_over ::= filter_clause over_clause */
   -1,  /* (318) filter_over ::= over_clause */
   -1,  /* (319) filter_over ::= filter_clause */
   -4,  /* (320) over_clause ::= OVER LP window RP */
   -2,  /* (321) over_clause ::= OVER nm */
   -5,  /* (322) filter_clause ::= FILTER LP WHERE expr RP */
   -1,  /* (323) input ::= cmdlist */
   -2,  /* (324) cmdlist ::= cmdlist ecmd */
   -1,  /* (325) cmdlist ::= ecmd */
   -1,  /* (326) ecmd ::= SEMI */
   -2,  /* (327) ecmd ::= cmdx SEMI */
   -2,  /* (328) ecmd ::= explain cmdx */
    0,  /* (329) trans_opt ::= */
   -1,  /* (330) trans_opt ::= TRANSACTION */
   -2,  /* (331) trans_opt ::= TRANSACTION nm */
   -1,  /* (332) savepoint_opt ::= SAVEPOINT */
    0,  /* (333) savepoint_opt ::= */
   -2,  /* (334) cmd ::= create_table create_table_args */
   -4,  /* (335) columnlist ::= columnlist COMMA columnname carglist */
   -2,  /* (336) columnlist ::= columnname carglist */
   -1,  /* (337) nm ::= ID|INDEXED */
   -1,  /* (338) nm ::= STRING */
   -1,  /* (339) nm ::= JOIN_KW */
   -1,  /* (340) typetoken ::= typename */
   -1,  /* (341) typename ::= ID|STRING */
   -1,  /* (342) signed ::= plus_num */
   -1,  /* (343) signed ::= minus_num */
   -2,  /* (344) carglist ::= carglist ccons */
    0,  /* (345) carglist ::= */
   -2,  /* (346) ccons ::= NULL onconf */
   -2,  /* (347) conslist_opt ::= COMMA conslist */
   -3,  /* (348) conslist ::= conslist tconscomma tcons */
   -1,  /* (349) conslist ::= tcons */
    0,  /* (350) tconscomma ::= */
   -1,  /* (351) defer_subclause_opt ::= defer_subclause */
   -1,  /* (352) resolvetype ::= raisetype */
   -1,  /* (353) selectnowith ::= oneselect */
   -1,  /* (354) oneselect ::= values */
   -2,  /* (355) sclp ::= selcollist COMMA */
   -1,  /* (356) as ::= ID|STRING */
   -1,  /* (357) expr ::= term */
   -1,  /* (358) likeop ::= LIKE_KW|MATCH */
   -1,  /* (359) exprlist ::= nexprlist */
   -1,  /* (360) nmnum ::= plus_num */
   -1,  /* (361) nmnum ::= nm */
   -1,  /* (362) nmnum ::= ON */
   -1,  /* (363) nmnum ::= DELETE */
   -1,  /* (364) nmnum ::= DEFAULT */
   -1,  /* (365) plus_num ::= INTEGER|FLOAT */
    0,  /* (366) foreach_clause ::= */
   -3,  /* (367) foreach_clause ::= FOR EACH ROW */
   -1,  /* (368) trnm ::= nm */
    0,  /* (369) tridxby ::= */
   -1,  /* (370) database_kw_opt ::= DATABASE */
    0,  /* (371) database_kw_opt ::= */
    0,  /* (372) kwcolumn_opt ::= */
   -1,  /* (373) kwcolumn_opt ::= COLUMNKW */
   -1,  /* (374) vtabarglist ::= vtabarg */
   -3,  /* (375) vtabarglist ::= vtabarglist COMMA vtabarg */
   -2,  /* (376) vtabarg ::= vtabarg vtabargtoken */
    0,  /* (377) anylist ::= */
   -4,  /* (378) anylist ::= anylist LP anylist RP */
   -2,  /* (379) anylist ::= anylist ANY */
    0,  /* (380) with ::= */
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
**
** The yyLookahead and yyLookaheadToken parameters provide reduce actions
** access to the lookahead token (if any).  The yyLookahead will be YYNOCODE
** if the lookahead token has already been consumed.  As this procedure is
** only called from one place, optimizing compilers will in-line it, which
** means that the extra parameters have no performance impact.
*/
static YYACTIONTYPE yy_reduce(
  yyParser *yypParser,         /* The parser */
  unsigned int yyruleno,       /* Number of the rule by which to reduce */
  int yyLookahead,             /* Lookahead token, or YYNOCODE if none */
  sqlite3ParserTOKENTYPE yyLookaheadToken  /* Value of the lookahead token */
  sqlite3ParserCTX_PDECL                   /* %extra_context */
){
  int yygoto;                     /* The next state */
  YYACTIONTYPE yyact;             /* The next action */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  sqlite3ParserARG_FETCH
  (void)yyLookahead;
  (void)yyLookaheadToken;
  yymsp = yypParser->yytos;
#ifndef NDEBUG
  if( yyTraceFILE && yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0])) ){
    yysize = yyRuleInfoNRhs[yyruleno];
    if( yysize ){
      fprintf(yyTraceFILE, "%sReduce %d [%s], go to state %d.\n",
        yyTracePrompt,
        yyruleno, yyRuleName[yyruleno], yymsp[yysize].stateno);
    }else{
      fprintf(yyTraceFILE, "%sReduce %d [%s].\n",
        yyTracePrompt, yyruleno, yyRuleName[yyruleno]);
    }
  }
#endif /* NDEBUG */

  /* Check that the stack is large enough to grow by a single entry
  ** if the RHS of the rule is empty.  This ensures that there is room
  ** enough on the stack to push the LHS value */
  if( yyRuleInfoNRhs[yyruleno]==0 ){
#ifdef YYTRACKMAXSTACKDEPTH
    if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
      yypParser->yyhwm++;
      assert( yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack));
    }
#endif
#if YYSTACKDEPTH>0 
    if( yypParser->yytos>=yypParser->yystackEnd ){
      yyStackOverflow(yypParser);
      /* The call to yyStackOverflow() above pops the stack until it is
      ** empty, causing the main parser loop to exit.  So the return value
      ** is never used and does not matter. */
      return 0;
    }
#else
    if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz-1] ){
      if( yyGrowStack(yypParser) ){
        yyStackOverflow(yypParser);
        /* The call to yyStackOverflow() above pops the stack until it is
        ** empty, causing the main parser loop to exit.  So the return value
        ** is never used and does not matter. */
        return 0;
      }
      yymsp = yypParser->yytos;
    }
#endif
  }

  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
/********** Begin reduce actions **********************************************/
        YYMINORTYPE yylhsminor;
      case 0: /* explain ::= EXPLAIN */
{ pParse->explain = 1; }
        break;
      case 1: /* explain ::= EXPLAIN QUERY PLAN */
{ pParse->explain = 2; }
        break;
      case 2: /* cmdx ::= cmd */
{ sqlite3FinishCoding(pParse); }
        break;
      case 3: /* cmd ::= BEGIN transtype trans_opt */
{sqlite3BeginTransaction(pParse, yymsp[-1].minor.yy32);}
        break;
      case 4: /* transtype ::= */
{yymsp[1].minor.yy32 = TK_DEFERRED;}
        break;
      case 5: /* transtype ::= DEFERRED */
      case 6: /* transtype ::= IMMEDIATE */ yytestcase(yyruleno==6);
      case 7: /* transtype ::= EXCLUSIVE */ yytestcase(yyruleno==7);
      case 304: /* range_or_rows ::= RANGE|ROWS|GROUPS */ yytestcase(yyruleno==304);
{yymsp[0].minor.yy32 = yymsp[0].major; /*A-overwrites-X*/}
        break;
      case 8: /* cmd ::= COMMIT|END trans_opt */
      case 9: /* cmd ::= ROLLBACK trans_opt */ yytestcase(yyruleno==9);
{sqlite3EndTransaction(pParse,yymsp[-1].major);}
        break;
      case 10: /* cmd ::= SAVEPOINT nm */
{
  sqlite3Savepoint(pParse, SAVEPOINT_BEGIN, &yymsp[0].minor.yy0);
}
        break;
      case 11: /* cmd ::= RELEASE savepoint_opt nm */
{
  sqlite3Savepoint(pParse, SAVEPOINT_RELEASE, &yymsp[0].minor.yy0);
}
        break;
      case 12: /* cmd ::= ROLLBACK trans_opt TO savepoint_opt nm */
{
  sqlite3Savepoint(pParse, SAVEPOINT_ROLLBACK, &yymsp[0].minor.yy0);
}
        break;
      case 13: /* create_table ::= createkw temp TABLE ifnotexists nm dbnm */
{
   sqlite3StartTable(pParse,&yymsp[-1].minor.yy0,&yymsp[0].minor.yy0,yymsp[-4].minor.yy32,0,0,yymsp[-2].minor.yy32);
}
        break;
      case 14: /* createkw ::= CREATE */
{disableLookaside(pParse);}
        break;
      case 15: /* ifnotexists ::= */
      case 18: /* temp ::= */ yytestcase(yyruleno==18);
      case 21: /* table_options ::= */ yytestcase(yyruleno==21);
      case 43: /* autoinc ::= */ yytestcase(yyruleno==43);
      case 58: /* init_deferred_pred_opt ::= */ yytestcase(yyruleno==58);
      case 68: /* defer_subclause_opt ::= */ yytestcase(yyruleno==68);
      case 77: /* ifexists ::= */ yytestcase(yyruleno==77);
      case 94: /* distinct ::= */ yytestcase(yyruleno==94);
      case 230: /* collate ::= */ yytestcase(yyruleno==230);
{yymsp[1].minor.yy32 = 0;}
        break;
      case 16: /* ifnotexists ::= IF NOT EXISTS */
{yymsp[-2].minor.yy32 = 1;}
        break;
      case 17: /* temp ::= TEMP */
      case 44: /* autoinc ::= AUTOINCR */ yytestcase(yyruleno==44);
{yymsp[0].minor.yy32 = 1;}
        break;
      case 19: /* create_table_args ::= LP columnlist conslist_opt RP table_options */
{
  sqlite3EndTable(pParse,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0,yymsp[0].minor.yy32,0);
}
        break;
      case 20: /* create_table_args ::= AS select */
{
  sqlite3EndTable(pParse,0,0,0,yymsp[0].minor.yy25);
  sqlite3SelectDelete(pParse->db, yymsp[0].minor.yy25);
}
        break;
      case 22: /* table_options ::= WITHOUT nm */
{
  if( yymsp[0].minor.yy0.n==5 && sqlite3_strnicmp(yymsp[0].minor.yy0.z,"rowid",5)==0 ){
    yymsp[-1].minor.yy32 = TF_WithoutRowid | TF_NoVisibleRowid;
  }else{
    yymsp[-1].minor.yy32 = 0;
    sqlite3ErrorMsg(pParse, "unknown table option: %.*s", yymsp[0].minor.yy0.n, yymsp[0].minor.yy0.z);
  }
}
        break;
      case 23: /* columnname ::= nm typetoken */
{sqlite3AddColumn(pParse,&yymsp[-1].minor.yy0,&yymsp[0].minor.yy0);}
        break;
      case 24: /* typetoken ::= */
      case 61: /* conslist_opt ::= */ yytestcase(yyruleno==61);
      case 100: /* as ::= */ yytestcase(yyruleno==100);
{yymsp[1].minor.yy0.n = 0; yymsp[1].minor.yy0.z = 0;}
        break;
      case 25: /* typetoken ::= typename LP signed RP */
{
  yymsp[-3].minor.yy0.n = (int)(&yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n] - yymsp[-3].minor.yy0.z);
}
        break;
      case 26: /* typetoken ::= typename LP signed COMMA signed RP */
{
  yymsp[-5].minor.yy0.n = (int)(&yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n] - yymsp[-5].minor.yy0.z);
}
        break;
      case 27: /* typename ::= typename ID|STRING */
{yymsp[-1].minor.yy0.n=yymsp[0].minor.yy0.n+(int)(yymsp[0].minor.yy0.z-yymsp[-1].minor.yy0.z);}
        break;
      case 28: /* scanpt ::= */
{
  assert( yyLookahead!=YYNOCODE );
  yymsp[1].minor.yy8 = yyLookaheadToken.z;
}
        break;
      case 29: /* scantok ::= */
{
  assert( yyLookahead!=YYNOCODE );
  yymsp[1].minor.yy0 = yyLookaheadToken;
}
        break;
      case 30: /* ccons ::= CONSTRAINT nm */
      case 63: /* tcons ::= CONSTRAINT nm */ yytestcase(yyruleno==63);
{pParse->constraintName = yymsp[0].minor.yy0;}
        break;
      case 31: /* ccons ::= DEFAULT scantok term */
{sqlite3AddDefaultValue(pParse,yymsp[0].minor.yy46,yymsp[-1].minor.yy0.z,&yymsp[-1].minor.yy0.z[yymsp[-1].minor.yy0.n]);}
        break;
      case 32: /* ccons ::= DEFAULT LP expr RP */
{sqlite3AddDefaultValue(pParse,yymsp[-1].minor.yy46,yymsp[-2].minor.yy0.z+1,yymsp[0].minor.yy0.z);}
        break;
      case 33: /* ccons ::= DEFAULT PLUS scantok term */
{sqlite3AddDefaultValue(pParse,yymsp[0].minor.yy46,yymsp[-2].minor.yy0.z,&yymsp[-1].minor.yy0.z[yymsp[-1].minor.yy0.n]);}
        break;
      case 34: /* ccons ::= DEFAULT MINUS scantok term */
{
  Expr *p = sqlite3PExpr(pParse, TK_UMINUS, yymsp[0].minor.yy46, 0);
  sqlite3AddDefaultValue(pParse,p,yymsp[-2].minor.yy0.z,&yymsp[-1].minor.yy0.z[yymsp[-1].minor.yy0.n]);
}
        break;
      case 35: /* ccons ::= DEFAULT scantok ID|INDEXED */
{
  Expr *p = tokenExpr(pParse, TK_STRING, yymsp[0].minor.yy0);
  if( p ){
    sqlite3ExprIdToTrueFalse(p);
    testcase( p->op==TK_TRUEFALSE && sqlite3ExprTruthValue(p) );
  }
    sqlite3AddDefaultValue(pParse,p,yymsp[0].minor.yy0.z,yymsp[0].minor.yy0.z+yymsp[0].minor.yy0.n);
}
        break;
      case 36: /* ccons ::= NOT NULL onconf */
{sqlite3AddNotNull(pParse, yymsp[0].minor.yy32);}
        break;
      case 37: /* ccons ::= PRIMARY KEY sortorder onconf autoinc */
{sqlite3AddPrimaryKey(pParse,0,yymsp[-1].minor.yy32,yymsp[0].minor.yy32,yymsp[-2].minor.yy32);}
        break;
      case 38: /* ccons ::= UNIQUE onconf */
{sqlite3CreateIndex(pParse,0,0,0,0,yymsp[0].minor.yy32,0,0,0,0,
                                   SQLITE_IDXTYPE_UNIQUE);}
        break;
      case 39: /* ccons ::= CHECK LP expr RP */
{sqlite3AddCheckConstraint(pParse,yymsp[-1].minor.yy46);}
        break;
      case 40: /* ccons ::= REFERENCES nm eidlist_opt refargs */
{sqlite3CreateForeignKey(pParse,0,&yymsp[-2].minor.yy0,yymsp[-1].minor.yy138,yymsp[0].minor.yy32);}
        break;
      case 41: /* ccons ::= defer_subclause */
{sqlite3DeferForeignKey(pParse,yymsp[0].minor.yy32);}
        break;
      case 42: /* ccons ::= COLLATE ID|STRING */
{sqlite3AddCollateType(pParse, &yymsp[0].minor.yy0);}
        break;
      case 45: /* refargs ::= */
{ yymsp[1].minor.yy32 = OE_None*0x0101; /* EV: R-19803-45884 */}
        break;
      case 46: /* refargs ::= refargs refarg */
{ yymsp[-1].minor.yy32 = (yymsp[-1].minor.yy32 & ~yymsp[0].minor.yy495.mask) | yymsp[0].minor.yy495.value; }
        break;
      case 47: /* refarg ::= MATCH nm */
{ yymsp[-1].minor.yy495.value = 0;     yymsp[-1].minor.yy495.mask = 0x000000; }
        break;
      case 48: /* refarg ::= ON INSERT refact */
{ yymsp[-2].minor.yy495.value = 0;     yymsp[-2].minor.yy495.mask = 0x000000; }
        break;
      case 49: /* refarg ::= ON DELETE refact */
{ yymsp[-2].minor.yy495.value = yymsp[0].minor.yy32;     yymsp[-2].minor.yy495.mask = 0x0000ff; }
        break;
      case 50: /* refarg ::= ON UPDATE refact */
{ yymsp[-2].minor.yy495.value = yymsp[0].minor.yy32<<8;  yymsp[-2].minor.yy495.mask = 0x00ff00; }
        break;
      case 51: /* refact ::= SET NULL */
{ yymsp[-1].minor.yy32 = OE_SetNull;  /* EV: R-33326-45252 */}
        break;
      case 52: /* refact ::= SET DEFAULT */
{ yymsp[-1].minor.yy32 = OE_SetDflt;  /* EV: R-33326-45252 */}
        break;
      case 53: /* refact ::= CASCADE */
{ yymsp[0].minor.yy32 = OE_Cascade;  /* EV: R-33326-45252 */}
        break;
      case 54: /* refact ::= RESTRICT */
{ yymsp[0].minor.yy32 = OE_Restrict; /* EV: R-33326-45252 */}
        break;
      case 55: /* refact ::= NO ACTION */
{ yymsp[-1].minor.yy32 = OE_None;     /* EV: R-33326-45252 */}
        break;
      case 56: /* defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt */
{yymsp[-2].minor.yy32 = 0;}
        break;
      case 57: /* defer_subclause ::= DEFERRABLE init_deferred_pred_opt */
      case 72: /* orconf ::= OR resolvetype */ yytestcase(yyruleno==72);
      case 160: /* insert_cmd ::= INSERT orconf */ yytestcase(yyruleno==160);
{yymsp[-1].minor.yy32 = yymsp[0].minor.yy32;}
        break;
      case 59: /* init_deferred_pred_opt ::= INITIALLY DEFERRED */
      case 76: /* ifexists ::= IF EXISTS */ yytestcase(yyruleno==76);
      case 202: /* between_op ::= NOT BETWEEN */ yytestcase(yyruleno==202);
      case 205: /* in_op ::= NOT IN */ yytestcase(yyruleno==205);
      case 231: /* collate ::= COLLATE ID|STRING */ yytestcase(yyruleno==231);
{yymsp[-1].minor.yy32 = 1;}
        break;
      case 60: /* init_deferred_pred_opt ::= INITIALLY IMMEDIATE */
{yymsp[-1].minor.yy32 = 0;}
        break;
      case 62: /* tconscomma ::= COMMA */
{pParse->constraintName.n = 0;}
        break;
      case 64: /* tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf */
{sqlite3AddPrimaryKey(pParse,yymsp[-3].minor.yy138,yymsp[0].minor.yy32,yymsp[-2].minor.yy32,0);}
        break;
      case 65: /* tcons ::= UNIQUE LP sortlist RP onconf */
{sqlite3CreateIndex(pParse,0,0,0,yymsp[-2].minor.yy138,yymsp[0].minor.yy32,0,0,0,0,
                                       SQLITE_IDXTYPE_UNIQUE);}
        break;
      case 66: /* tcons ::= CHECK LP expr RP onconf */
{sqlite3AddCheckConstraint(pParse,yymsp[-2].minor.yy46);}
        break;
      case 67: /* tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt */
{
    sqlite3CreateForeignKey(pParse, yymsp[-6].minor.yy138, &yymsp[-3].minor.yy0, yymsp[-2].minor.yy138, yymsp[-1].minor.yy32);
    sqlite3DeferForeignKey(pParse, yymsp[0].minor.yy32);
}
        break;
      case 69: /* onconf ::= */
      case 71: /* orconf ::= */ yytestcase(yyruleno==71);
{yymsp[1].minor.yy32 = OE_Default;}
        break;
      case 70: /* onconf ::= ON CONFLICT resolvetype */
{yymsp[-2].minor.yy32 = yymsp[0].minor.yy32;}
        break;
      case 73: /* resolvetype ::= IGNORE */
{yymsp[0].minor.yy32 = OE_Ignore;}
        break;
      case 74: /* resolvetype ::= REPLACE */
      case 161: /* insert_cmd ::= REPLACE */ yytestcase(yyruleno==161);
{yymsp[0].minor.yy32 = OE_Replace;}
        break;
      case 75: /* cmd ::= DROP TABLE ifexists fullname */
{
  sqlite3DropTable(pParse, yymsp[0].minor.yy609, 0, yymsp[-1].minor.yy32);
}
        break;
      case 78: /* cmd ::= createkw temp VIEW ifnotexists nm dbnm eidlist_opt AS select */
{
  sqlite3CreateView(pParse, &yymsp[-8].minor.yy0, &yymsp[-4].minor.yy0, &yymsp[-3].minor.yy0, yymsp[-2].minor.yy138, yymsp[0].minor.yy25, yymsp[-7].minor.yy32, yymsp[-5].minor.yy32);
}
        break;
      case 79: /* cmd ::= DROP VIEW ifexists fullname */
{
  sqlite3DropTable(pParse, yymsp[0].minor.yy609, 1, yymsp[-1].minor.yy32);
}
        break;
      case 80: /* cmd ::= select */
{
  SelectDest dest = {SRT_Output, 0, 0, 0, 0, 0};
  sqlite3Select(pParse, yymsp[0].minor.yy25, &dest);
  sqlite3SelectDelete(pParse->db, yymsp[0].minor.yy25);
}
        break;
      case 81: /* select ::= WITH wqlist selectnowith */
{
  Select *p = yymsp[0].minor.yy25;
  if( p ){
    p->pWith = yymsp[-1].minor.yy297;
    parserDoubleLinkSelect(pParse, p);
  }else{
    sqlite3WithDelete(pParse->db, yymsp[-1].minor.yy297);
  }
  yymsp[-2].minor.yy25 = p;
}
        break;
      case 82: /* select ::= WITH RECURSIVE wqlist selectnowith */
{
  Select *p = yymsp[0].minor.yy25;
  if( p ){
    p->pWith = yymsp[-1].minor.yy297;
    parserDoubleLinkSelect(pParse, p);
  }else{
    sqlite3WithDelete(pParse->db, yymsp[-1].minor.yy297);
  }
  yymsp[-3].minor.yy25 = p;
}
        break;
      case 83: /* select ::= selectnowith */
{
  Select *p = yymsp[0].minor.yy25;
  if( p ){
    parserDoubleLinkSelect(pParse, p);
  }
  yymsp[0].minor.yy25 = p; /*A-overwrites-X*/
}
        break;
      case 84: /* selectnowith ::= selectnowith multiselect_op oneselect */
{
  Select *pRhs = yymsp[0].minor.yy25;
  Select *pLhs = yymsp[-2].minor.yy25;
  if( pRhs && pRhs->pPrior ){
    SrcList *pFrom;
    Token x;
    x.n = 0;
    parserDoubleLinkSelect(pParse, pRhs);
    pFrom = sqlite3SrcListAppendFromTerm(pParse,0,0,0,&x,pRhs,0,0);
    pRhs = sqlite3SelectNew(pParse,0,pFrom,0,0,0,0,0,0);
  }
  if( pRhs ){
    pRhs->op = (u8)yymsp[-1].minor.yy32;
    pRhs->pPrior = pLhs;
    if( ALWAYS(pLhs) ) pLhs->selFlags &= ~SF_MultiValue;
    pRhs->selFlags &= ~SF_MultiValue;
    if( yymsp[-1].minor.yy32!=TK_ALL ) pParse->hasCompound = 1;
  }else{
    sqlite3SelectDelete(pParse->db, pLhs);
  }
  yymsp[-2].minor.yy25 = pRhs;
}
        break;
      case 85: /* multiselect_op ::= UNION */
      case 87: /* multiselect_op ::= EXCEPT|INTERSECT */ yytestcase(yyruleno==87);
{yymsp[0].minor.yy32 = yymsp[0].major; /*A-overwrites-OP*/}
        break;
      case 86: /* multiselect_op ::= UNION ALL */
{yymsp[-1].minor.yy32 = TK_ALL;}
        break;
      case 88: /* oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt */
{
  yymsp[-8].minor.yy25 = sqlite3SelectNew(pParse,yymsp[-6].minor.yy138,yymsp[-5].minor.yy609,yymsp[-4].minor.yy46,yymsp[-3].minor.yy138,yymsp[-2].minor.yy46,yymsp[-1].minor.yy138,yymsp[-7].minor.yy32,yymsp[0].minor.yy46);
}
        break;
      case 89: /* oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt window_clause orderby_opt limit_opt */
{
  yymsp[-9].minor.yy25 = sqlite3SelectNew(pParse,yymsp[-7].minor.yy138,yymsp[-6].minor.yy609,yymsp[-5].minor.yy46,yymsp[-4].minor.yy138,yymsp[-3].minor.yy46,yymsp[-1].minor.yy138,yymsp[-8].minor.yy32,yymsp[0].minor.yy46);
  if( yymsp[-9].minor.yy25 ){
    yymsp[-9].minor.yy25->pWinDefn = yymsp[-2].minor.yy455;
  }else{
    sqlite3WindowListDelete(pParse->db, yymsp[-2].minor.yy455);
  }
}
        break;
      case 90: /* values ::= VALUES LP nexprlist RP */
{
  yymsp[-3].minor.yy25 = sqlite3SelectNew(pParse,yymsp[-1].minor.yy138,0,0,0,0,0,SF_Values,0);
}
        break;
      case 91: /* values ::= values COMMA LP nexprlist RP */
{
  Select *pRight, *pLeft = yymsp[-4].minor.yy25;
  pRight = sqlite3SelectNew(pParse,yymsp[-1].minor.yy138,0,0,0,0,0,SF_Values|SF_MultiValue,0);
  if( ALWAYS(pLeft) ) pLeft->selFlags &= ~SF_MultiValue;
  if( pRight ){
    pRight->op = TK_ALL;
    pRight->pPrior = pLeft;
    yymsp[-4].minor.yy25 = pRight;
  }else{
    yymsp[-4].minor.yy25 = pLeft;
  }
}
        break;
      case 92: /* distinct ::= DISTINCT */
{yymsp[0].minor.yy32 = SF_Distinct;}
        break;
      case 93: /* distinct ::= ALL */
{yymsp[0].minor.yy32 = SF_All;}
        break;
      case 95: /* sclp ::= */
      case 128: /* orderby_opt ::= */ yytestcase(yyruleno==128);
      case 138: /* groupby_opt ::= */ yytestcase(yyruleno==138);
      case 218: /* exprlist ::= */ yytestcase(yyruleno==218);
      case 221: /* paren_exprlist ::= */ yytestcase(yyruleno==221);
      case 226: /* eidlist_opt ::= */ yytestcase(yyruleno==226);
{yymsp[1].minor.yy138 = 0;}
        break;
      case 96: /* selcollist ::= sclp scanpt expr scanpt as */
{
   yymsp[-4].minor.yy138 = sqlite3ExprListAppend(pParse, yymsp[-4].minor.yy138, yymsp[-2].minor.yy46);
   if( yymsp[0].minor.yy0.n>0 ) sqlite3ExprListSetName(pParse, yymsp[-4].minor.yy138, &yymsp[0].minor.yy0, 1);
   sqlite3ExprListSetSpan(pParse,yymsp[-4].minor.yy138,yymsp[-3].minor.yy8,yymsp[-1].minor.yy8);
}
        break;
      case 97: /* selcollist ::= sclp scanpt STAR */
{
  Expr *p = sqlite3Expr(pParse->db, TK_ASTERISK, 0);
  yymsp[-2].minor.yy138 = sqlite3ExprListAppend(pParse, yymsp[-2].minor.yy138, p);
}
        break;
      case 98: /* selcollist ::= sclp scanpt nm DOT STAR */
{
  Expr *pRight = sqlite3PExpr(pParse, TK_ASTERISK, 0, 0);
  Expr *pLeft = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-2].minor.yy0, 1);
  Expr *pDot = sqlite3PExpr(pParse, TK_DOT, pLeft, pRight);
  yymsp[-4].minor.yy138 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy138, pDot);
}
        break;
      case 99: /* as ::= AS nm */
      case 110: /* dbnm ::= DOT nm */ yytestcase(yyruleno==110);
      case 242: /* plus_num ::= PLUS INTEGER|FLOAT */ yytestcase(yyruleno==242);
      case 243: /* minus_num ::= MINUS INTEGER|FLOAT */ yytestcase(yyruleno==243);
{yymsp[-1].minor.yy0 = yymsp[0].minor.yy0;}
        break;
      case 101: /* from ::= */
{yymsp[1].minor.yy609 = sqlite3DbMallocZero(pParse->db, sizeof(*yymsp[1].minor.yy609));}
        break;
      case 102: /* from ::= FROM seltablist */
{
  yymsp[-1].minor.yy609 = yymsp[0].minor.yy609;
  sqlite3SrcListShiftJoinType(yymsp[-1].minor.yy609);
}
        break;
      case 103: /* stl_prefix ::= seltablist joinop */
{
   if( ALWAYS(yymsp[-1].minor.yy609 && yymsp[-1].minor.yy609->nSrc>0) ) yymsp[-1].minor.yy609->a[yymsp[-1].minor.yy609->nSrc-1].fg.jointype = (u8)yymsp[0].minor.yy32;
}
        break;
      case 104: /* stl_prefix ::= */
{yymsp[1].minor.yy609 = 0;}
        break;
      case 105: /* seltablist ::= stl_prefix nm dbnm as indexed_opt on_opt using_opt */
{
  yymsp[-6].minor.yy609 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy609,&yymsp[-5].minor.yy0,&yymsp[-4].minor.yy0,&yymsp[-3].minor.yy0,0,yymsp[-1].minor.yy46,yymsp[0].minor.yy406);
  sqlite3SrcListIndexedBy(pParse, yymsp[-6].minor.yy609, &yymsp[-2].minor.yy0);
}
        break;
      case 106: /* seltablist ::= stl_prefix nm dbnm LP exprlist RP as on_opt using_opt */
{
  yymsp[-8].minor.yy609 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-8].minor.yy609,&yymsp[-7].minor.yy0,&yymsp[-6].minor.yy0,&yymsp[-2].minor.yy0,0,yymsp[-1].minor.yy46,yymsp[0].minor.yy406);
  sqlite3SrcListFuncArgs(pParse, yymsp[-8].minor.yy609, yymsp[-4].minor.yy138);
}
        break;
      case 107: /* seltablist ::= stl_prefix LP select RP as on_opt using_opt */
{
    yymsp[-6].minor.yy609 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy609,0,0,&yymsp[-2].minor.yy0,yymsp[-4].minor.yy25,yymsp[-1].minor.yy46,yymsp[0].minor.yy406);
  }
        break;
      case 108: /* seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt */
{
    if( yymsp[-6].minor.yy609==0 && yymsp[-2].minor.yy0.n==0 && yymsp[-1].minor.yy46==0 && yymsp[0].minor.yy406==0 ){
      yymsp[-6].minor.yy609 = yymsp[-4].minor.yy609;
    }else if( yymsp[-4].minor.yy609->nSrc==1 ){
      yymsp[-6].minor.yy609 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy609,0,0,&yymsp[-2].minor.yy0,0,yymsp[-1].minor.yy46,yymsp[0].minor.yy406);
      if( yymsp[-6].minor.yy609 ){
        struct SrcList_item *pNew = &yymsp[-6].minor.yy609->a[yymsp[-6].minor.yy609->nSrc-1];
        struct SrcList_item *pOld = yymsp[-4].minor.yy609->a;
        pNew->zName = pOld->zName;
        pNew->zDatabase = pOld->zDatabase;
        pNew->pSelect = pOld->pSelect;
        if( pOld->fg.isTabFunc ){
          pNew->u1.pFuncArg = pOld->u1.pFuncArg;
          pOld->u1.pFuncArg = 0;
          pOld->fg.isTabFunc = 0;
          pNew->fg.isTabFunc = 1;
        }
        pOld->zName = pOld->zDatabase = 0;
        pOld->pSelect = 0;
      }
      sqlite3SrcListDelete(pParse->db, yymsp[-4].minor.yy609);
    }else{
      Select *pSubquery;
      sqlite3SrcListShiftJoinType(yymsp[-4].minor.yy609);
      pSubquery = sqlite3SelectNew(pParse,0,yymsp[-4].minor.yy609,0,0,0,0,SF_NestedFrom,0);
      yymsp[-6].minor.yy609 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy609,0,0,&yymsp[-2].minor.yy0,pSubquery,yymsp[-1].minor.yy46,yymsp[0].minor.yy406);
    }
  }
        break;
      case 109: /* dbnm ::= */
      case 123: /* indexed_opt ::= */ yytestcase(yyruleno==123);
{yymsp[1].minor.yy0.z=0; yymsp[1].minor.yy0.n=0;}
        break;
      case 111: /* fullname ::= nm */
{
  yylhsminor.yy609 = sqlite3SrcListAppend(pParse,0,&yymsp[0].minor.yy0,0);
  if( IN_RENAME_OBJECT && yylhsminor.yy609 ) sqlite3RenameTokenMap(pParse, yylhsminor.yy609->a[0].zName, &yymsp[0].minor.yy0);
}
  yymsp[0].minor.yy609 = yylhsminor.yy609;
        break;
      case 112: /* fullname ::= nm DOT nm */
{
  yylhsminor.yy609 = sqlite3SrcListAppend(pParse,0,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0);
  if( IN_RENAME_OBJECT && yylhsminor.yy609 ) sqlite3RenameTokenMap(pParse, yylhsminor.yy609->a[0].zName, &yymsp[0].minor.yy0);
}
  yymsp[-2].minor.yy609 = yylhsminor.yy609;
        break;
      case 113: /* xfullname ::= nm */
{yymsp[0].minor.yy609 = sqlite3SrcListAppend(pParse,0,&yymsp[0].minor.yy0,0); /*A-overwrites-X*/}
        break;
      case 114: /* xfullname ::= nm DOT nm */
{yymsp[-2].minor.yy609 = sqlite3SrcListAppend(pParse,0,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-X*/}
        break;
      case 115: /* xfullname ::= nm DOT nm AS nm */
{
   yymsp[-4].minor.yy609 = sqlite3SrcListAppend(pParse,0,&yymsp[-4].minor.yy0,&yymsp[-2].minor.yy0); /*A-overwrites-X*/
   if( yymsp[-4].minor.yy609 ) yymsp[-4].minor.yy609->a[0].zAlias = sqlite3NameFromToken(pParse->db, &yymsp[0].minor.yy0);
}
        break;
      case 116: /* xfullname ::= nm AS nm */
{  
   yymsp[-2].minor.yy609 = sqlite3SrcListAppend(pParse,0,&yymsp[-2].minor.yy0,0); /*A-overwrites-X*/
   if( yymsp[-2].minor.yy609 ) yymsp[-2].minor.yy609->a[0].zAlias = sqlite3NameFromToken(pParse->db, &yymsp[0].minor.yy0);
}
        break;
      case 117: /* joinop ::= COMMA|JOIN */
{ yymsp[0].minor.yy32 = JT_INNER; }
        break;
      case 118: /* joinop ::= JOIN_KW JOIN */
{yymsp[-1].minor.yy32 = sqlite3JoinType(pParse,&yymsp[-1].minor.yy0,0,0);  /*X-overwrites-A*/}
        break;
      case 119: /* joinop ::= JOIN_KW nm JOIN */
{yymsp[-2].minor.yy32 = sqlite3JoinType(pParse,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0,0); /*X-overwrites-A*/}
        break;
      case 120: /* joinop ::= JOIN_KW nm nm JOIN */
{yymsp[-3].minor.yy32 = sqlite3JoinType(pParse,&yymsp[-3].minor.yy0,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0);/*X-overwrites-A*/}
        break;
      case 121: /* on_opt ::= ON expr */
      case 141: /* having_opt ::= HAVING expr */ yytestcase(yyruleno==141);
      case 148: /* where_opt ::= WHERE expr */ yytestcase(yyruleno==148);
      case 214: /* case_else ::= ELSE expr */ yytestcase(yyruleno==214);
      case 235: /* vinto ::= INTO expr */ yytestcase(yyruleno==235);
{yymsp[-1].minor.yy46 = yymsp[0].minor.yy46;}
        break;
      case 122: /* on_opt ::= */
      case 140: /* having_opt ::= */ yytestcase(yyruleno==140);
      case 142: /* limit_opt ::= */ yytestcase(yyruleno==142);
      case 147: /* where_opt ::= */ yytestcase(yyruleno==147);
      case 215: /* case_else ::= */ yytestcase(yyruleno==215);
      case 217: /* case_operand ::= */ yytestcase(yyruleno==217);
      case 236: /* vinto ::= */ yytestcase(yyruleno==236);
{yymsp[1].minor.yy46 = 0;}
        break;
      case 124: /* indexed_opt ::= INDEXED BY nm */
{yymsp[-2].minor.yy0 = yymsp[0].minor.yy0;}
        break;
      case 125: /* indexed_opt ::= NOT INDEXED */
{yymsp[-1].minor.yy0.z=0; yymsp[-1].minor.yy0.n=1;}
        break;
      case 126: /* using_opt ::= USING LP idlist RP */
{yymsp[-3].minor.yy406 = yymsp[-1].minor.yy406;}
        break;
      case 127: /* using_opt ::= */
      case 162: /* idlist_opt ::= */ yytestcase(yyruleno==162);
{yymsp[1].minor.yy406 = 0;}
        break;
      case 129: /* orderby_opt ::= ORDER BY sortlist */
      case 139: /* groupby_opt ::= GROUP BY nexprlist */ yytestcase(yyruleno==139);
{yymsp[-2].minor.yy138 = yymsp[0].minor.yy138;}
        break;
      case 130: /* sortlist ::= sortlist COMMA expr sortorder nulls */
{
  yymsp[-4].minor.yy138 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy138,yymsp[-2].minor.yy46);
  sqlite3ExprListSetSortOrder(yymsp[-4].minor.yy138,yymsp[-1].minor.yy32,yymsp[0].minor.yy32);
}
        break;
      case 131: /* sortlist ::= expr sortorder nulls */
{
  yymsp[-2].minor.yy138 = sqlite3ExprListAppend(pParse,0,yymsp[-2].minor.yy46); /*A-overwrites-Y*/
  sqlite3ExprListSetSortOrder(yymsp[-2].minor.yy138,yymsp[-1].minor.yy32,yymsp[0].minor.yy32);
}
        break;
      case 132: /* sortorder ::= ASC */
{yymsp[0].minor.yy32 = SQLITE_SO_ASC;}
        break;
      case 133: /* sortorder ::= DESC */
{yymsp[0].minor.yy32 = SQLITE_SO_DESC;}
        break;
      case 134: /* sortorder ::= */
      case 137: /* nulls ::= */ yytestcase(yyruleno==137);
{yymsp[1].minor.yy32 = SQLITE_SO_UNDEFINED;}
        break;
      case 135: /* nulls ::= NULLS FIRST */
{yymsp[-1].minor.yy32 = SQLITE_SO_ASC;}
        break;
      case 136: /* nulls ::= NULLS LAST */
{yymsp[-1].minor.yy32 = SQLITE_SO_DESC;}
        break;
      case 143: /* limit_opt ::= LIMIT expr */
{yymsp[-1].minor.yy46 = sqlite3PExpr(pParse,TK_LIMIT,yymsp[0].minor.yy46,0);}
        break;
      case 144: /* limit_opt ::= LIMIT expr OFFSET expr */
{yymsp[-3].minor.yy46 = sqlite3PExpr(pParse,TK_LIMIT,yymsp[-2].minor.yy46,yymsp[0].minor.yy46);}
        break;
      case 145: /* limit_opt ::= LIMIT expr COMMA expr */
{yymsp[-3].minor.yy46 = sqlite3PExpr(pParse,TK_LIMIT,yymsp[0].minor.yy46,yymsp[-2].minor.yy46);}
        break;
      case 146: /* cmd ::= with DELETE FROM xfullname indexed_opt where_opt */
{
  sqlite3SrcListIndexedBy(pParse, yymsp[-2].minor.yy609, &yymsp[-1].minor.yy0);
  sqlite3DeleteFrom(pParse,yymsp[-2].minor.yy609,yymsp[0].minor.yy46,0,0);
}
        break;
      case 149: /* cmd ::= with UPDATE orconf xfullname indexed_opt SET setlist where_opt */
{
  sqlite3SrcListIndexedBy(pParse, yymsp[-4].minor.yy609, &yymsp[-3].minor.yy0);
  sqlite3ExprListCheckLength(pParse,yymsp[-1].minor.yy138,"set list"); 
  sqlite3Update(pParse,yymsp[-4].minor.yy609,yymsp[-1].minor.yy138,yymsp[0].minor.yy46,yymsp[-5].minor.yy32,0,0,0);
}
        break;
      case 150: /* setlist ::= setlist COMMA nm EQ expr */
{
  yymsp[-4].minor.yy138 = sqlite3ExprListAppend(pParse, yymsp[-4].minor.yy138, yymsp[0].minor.yy46);
  sqlite3ExprListSetName(pParse, yymsp[-4].minor.yy138, &yymsp[-2].minor.yy0, 1);
}
        break;
      case 151: /* setlist ::= setlist COMMA LP idlist RP EQ expr */
{
  yymsp[-6].minor.yy138 = sqlite3ExprListAppendVector(pParse, yymsp[-6].minor.yy138, yymsp[-3].minor.yy406, yymsp[0].minor.yy46);
}
        break;
      case 152: /* setlist ::= nm EQ expr */
{
  yylhsminor.yy138 = sqlite3ExprListAppend(pParse, 0, yymsp[0].minor.yy46);
  sqlite3ExprListSetName(pParse, yylhsminor.yy138, &yymsp[-2].minor.yy0, 1);
}
  yymsp[-2].minor.yy138 = yylhsminor.yy138;
        break;
      case 153: /* setlist ::= LP idlist RP EQ expr */
{
  yymsp[-4].minor.yy138 = sqlite3ExprListAppendVector(pParse, 0, yymsp[-3].minor.yy406, yymsp[0].minor.yy46);
}
        break;
      case 154: /* cmd ::= with insert_cmd INTO xfullname idlist_opt select upsert */
{
  sqlite3Insert(pParse, yymsp[-3].minor.yy609, yymsp[-1].minor.yy25, yymsp[-2].minor.yy406, yymsp[-5].minor.yy32, yymsp[0].minor.yy288);
}
        break;
      case 155: /* cmd ::= with insert_cmd INTO xfullname idlist_opt DEFAULT VALUES */
{
  sqlite3Insert(pParse, yymsp[-3].minor.yy609, 0, yymsp[-2].minor.yy406, yymsp[-5].minor.yy32, 0);
}
        break;
      case 156: /* upsert ::= */
{ yymsp[1].minor.yy288 = 0; }
        break;
      case 157: /* upsert ::= ON CONFLICT LP sortlist RP where_opt DO UPDATE SET setlist where_opt */
{ yymsp[-10].minor.yy288 = sqlite3UpsertNew(pParse->db,yymsp[-7].minor.yy138,yymsp[-5].minor.yy46,yymsp[-1].minor.yy138,yymsp[0].minor.yy46);}
        break;
      case 158: /* upsert ::= ON CONFLICT LP sortlist RP where_opt DO NOTHING */
{ yymsp[-7].minor.yy288 = sqlite3UpsertNew(pParse->db,yymsp[-4].minor.yy138,yymsp[-2].minor.yy46,0,0); }
        break;
      case 159: /* upsert ::= ON CONFLICT DO NOTHING */
{ yymsp[-3].minor.yy288 = sqlite3UpsertNew(pParse->db,0,0,0,0); }
        break;
      case 163: /* idlist_opt ::= LP idlist RP */
{yymsp[-2].minor.yy406 = yymsp[-1].minor.yy406;}
        break;
      case 164: /* idlist ::= idlist COMMA nm */
{yymsp[-2].minor.yy406 = sqlite3IdListAppend(pParse,yymsp[-2].minor.yy406,&yymsp[0].minor.yy0);}
        break;
      case 165: /* idlist ::= nm */
{yymsp[0].minor.yy406 = sqlite3IdListAppend(pParse,0,&yymsp[0].minor.yy0); /*A-overwrites-Y*/}
        break;
      case 166: /* expr ::= LP expr RP */
{yymsp[-2].minor.yy46 = yymsp[-1].minor.yy46;}
        break;
      case 167: /* expr ::= ID|INDEXED */
      case 168: /* expr ::= JOIN_KW */ yytestcase(yyruleno==168);
{yymsp[0].minor.yy46=tokenExpr(pParse,TK_ID,yymsp[0].minor.yy0); /*A-overwrites-X*/}
        break;
      case 169: /* expr ::= nm DOT nm */
{
  Expr *temp1 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-2].minor.yy0, 1);
  Expr *temp2 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[0].minor.yy0, 1);
  if( IN_RENAME_OBJECT ){
    sqlite3RenameTokenMap(pParse, (void*)temp2, &yymsp[0].minor.yy0);
    sqlite3RenameTokenMap(pParse, (void*)temp1, &yymsp[-2].minor.yy0);
  }
  yylhsminor.yy46 = sqlite3PExpr(pParse, TK_DOT, temp1, temp2);
}
  yymsp[-2].minor.yy46 = yylhsminor.yy46;
        break;
      case 170: /* expr ::= nm DOT nm DOT nm */
{
  Expr *temp1 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-4].minor.yy0, 1);
  Expr *temp2 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-2].minor.yy0, 1);
  Expr *temp3 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[0].minor.yy0, 1);
  Expr *temp4 = sqlite3PExpr(pParse, TK_DOT, temp2, temp3);
  if( IN_RENAME_OBJECT ){
    sqlite3RenameTokenMap(pParse, (void*)temp3, &yymsp[0].minor.yy0);
    sqlite3RenameTokenMap(pParse, (void*)temp2, &yymsp[-2].minor.yy0);
  }
  yylhsminor.yy46 = sqlite3PExpr(pParse, TK_DOT, temp1, temp4);
}
  yymsp[-4].minor.yy46 = yylhsminor.yy46;
        break;
      case 171: /* term ::= NULL|FLOAT|BLOB */
      case 172: /* term ::= STRING */ yytestcase(yyruleno==172);
{yymsp[0].minor.yy46=tokenExpr(pParse,yymsp[0].major,yymsp[0].minor.yy0); /*A-overwrites-X*/}
        break;
      case 173: /* term ::= INTEGER */
{
  yylhsminor.yy46 = sqlite3ExprAlloc(pParse->db, TK_INTEGER, &yymsp[0].minor.yy0, 1);
}
  yymsp[0].minor.yy46 = yylhsminor.yy46;
        break;
      case 174: /* expr ::= VARIABLE */
{
  if( !(yymsp[0].minor.yy0.z[0]=='#' && sqlite3Isdigit(yymsp[0].minor.yy0.z[1])) ){
    u32 n = yymsp[0].minor.yy0.n;
    yymsp[0].minor.yy46 = tokenExpr(pParse, TK_VARIABLE, yymsp[0].minor.yy0);
    sqlite3ExprAssignVarNumber(pParse, yymsp[0].minor.yy46, n);
  }else{
    /* When doing a nested parse, one can include terms in an expression
    ** that look like this:   #1 #2 ...  These terms refer to registers
    ** in the virtual machine.  #N is the N-th register. */
    Token t = yymsp[0].minor.yy0; /*A-overwrites-X*/
    assert( t.n>=2 );
    if( pParse->nested==0 ){
      sqlite3ErrorMsg(pParse, "near \"%T\": syntax error", &t);
      yymsp[0].minor.yy46 = 0;
    }else{
      yymsp[0].minor.yy46 = sqlite3PExpr(pParse, TK_REGISTER, 0, 0);
      if( yymsp[0].minor.yy46 ) sqlite3GetInt32(&t.z[1], &yymsp[0].minor.yy46->iTable);
    }
  }
}
        break;
      case 175: /* expr ::= expr COLLATE ID|STRING */
{
  yymsp[-2].minor.yy46 = sqlite3ExprAddCollateToken(pParse, yymsp[-2].minor.yy46, &yymsp[0].minor.yy0, 1);
}
        break;
      case 176: /* expr ::= CAST LP expr AS typetoken RP */
{
  yymsp[-5].minor.yy46 = sqlite3ExprAlloc(pParse->db, TK_CAST, &yymsp[-1].minor.yy0, 1);
  sqlite3ExprAttachSubtrees(pParse->db, yymsp[-5].minor.yy46, yymsp[-3].minor.yy46, 0);
}
        break;
      case 177: /* expr ::= ID|INDEXED LP distinct exprlist RP */
{
  yylhsminor.yy46 = sqlite3ExprFunction(pParse, yymsp[-1].minor.yy138, &yymsp[-4].minor.yy0, yymsp[-2].minor.yy32);
}
  yymsp[-4].minor.yy46 = yylhsminor.yy46;
        break;
      case 178: /* expr ::= ID|INDEXED LP STAR RP */
{
  yylhsminor.yy46 = sqlite3ExprFunction(pParse, 0, &yymsp[-3].minor.yy0, 0);
}
  yymsp[-3].minor.yy46 = yylhsminor.yy46;
        break;
      case 179: /* expr ::= ID|INDEXED LP distinct exprlist RP filter_over */
{
  yylhsminor.yy46 = sqlite3ExprFunction(pParse, yymsp[-2].minor.yy138, &yymsp[-5].minor.yy0, yymsp[-3].minor.yy32);
  sqlite3WindowAttach(pParse, yylhsminor.yy46, yymsp[0].minor.yy455);
}
  yymsp[-5].minor.yy46 = yylhsminor.yy46;
        break;
      case 180: /* expr ::= ID|INDEXED LP STAR RP filter_over */
{
  yylhsminor.yy46 = sqlite3ExprFunction(pParse, 0, &yymsp[-4].minor.yy0, 0);
  sqlite3WindowAttach(pParse, yylhsminor.yy46, yymsp[0].minor.yy455);
}
  yymsp[-4].minor.yy46 = yylhsminor.yy46;
        break;
      case 181: /* term ::= CTIME_KW */
{
  yylhsminor.yy46 = sqlite3ExprFunction(pParse, 0, &yymsp[0].minor.yy0, 0);
}
  yymsp[0].minor.yy46 = yylhsminor.yy46;
        break;
      case 182: /* expr ::= LP nexprlist COMMA expr RP */
{
  ExprList *pList = sqlite3ExprListAppend(pParse, yymsp[-3].minor.yy138, yymsp[-1].minor.yy46);
  yymsp[-4].minor.yy46 = sqlite3PExpr(pParse, TK_VECTOR, 0, 0);
  if( yymsp[-4].minor.yy46 ){
    yymsp[-4].minor.yy46->x.pList = pList;
  }else{
    sqlite3ExprListDelete(pParse->db, pList);
  }
}
        break;
      case 183: /* expr ::= expr AND expr */
{yymsp[-2].minor.yy46=sqlite3ExprAnd(pParse,yymsp[-2].minor.yy46,yymsp[0].minor.yy46);}
        break;
      case 184: /* expr ::= expr OR expr */
      case 185: /* expr ::= expr LT|GT|GE|LE expr */ yytestcase(yyruleno==185);
      case 186: /* expr ::= expr EQ|NE expr */ yytestcase(yyruleno==186);
      case 187: /* expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr */ yytestcase(yyruleno==187);
      case 188: /* expr ::= expr PLUS|MINUS expr */ yytestcase(yyruleno==188);
      case 189: /* expr ::= expr STAR|SLASH|REM expr */ yytestcase(yyruleno==189);
      case 190: /* expr ::= expr CONCAT expr */ yytestcase(yyruleno==190);
{yymsp[-2].minor.yy46=sqlite3PExpr(pParse,yymsp[-1].major,yymsp[-2].minor.yy46,yymsp[0].minor.yy46);}
        break;
      case 191: /* likeop ::= NOT LIKE_KW|MATCH */
{yymsp[-1].minor.yy0=yymsp[0].minor.yy0; yymsp[-1].minor.yy0.n|=0x80000000; /*yymsp[-1].minor.yy0-overwrite-yymsp[0].minor.yy0*/}
        break;
      case 192: /* expr ::= expr likeop expr */
{
  ExprList *pList;
  int bNot = yymsp[-1].minor.yy0.n & 0x80000000;
  yymsp[-1].minor.yy0.n &= 0x7fffffff;
  pList = sqlite3ExprListAppend(pParse,0, yymsp[0].minor.yy46);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[-2].minor.yy46);
  yymsp[-2].minor.yy46 = sqlite3ExprFunction(pParse, pList, &yymsp[-1].minor.yy0, 0);
  if( bNot ) yymsp[-2].minor.yy46 = sqlite3PExpr(pParse, TK_NOT, yymsp[-2].minor.yy46, 0);
  if( yymsp[-2].minor.yy46 ) yymsp[-2].minor.yy46->flags |= EP_InfixFunc;
}
        break;
      case 193: /* expr ::= expr likeop expr ESCAPE expr */
{
  ExprList *pList;
  int bNot = yymsp[-3].minor.yy0.n & 0x80000000;
  yymsp[-3].minor.yy0.n &= 0x7fffffff;
  pList = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy46);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[-4].minor.yy46);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[0].minor.yy46);
  yymsp[-4].minor.yy46 = sqlite3ExprFunction(pParse, pList, &yymsp[-3].minor.yy0, 0);
  if( bNot ) yymsp[-4].minor.yy46 = sqlite3PExpr(pParse, TK_NOT, yymsp[-4].minor.yy46, 0);
  if( yymsp[-4].minor.yy46 ) yymsp[-4].minor.yy46->flags |= EP_InfixFunc;
}
        break;
      case 194: /* expr ::= expr ISNULL|NOTNULL */
{yymsp[-1].minor.yy46 = sqlite3PExpr(pParse,yymsp[0].major,yymsp[-1].minor.yy46,0);}
        break;
      case 195: /* expr ::= expr NOT NULL */
{yymsp[-2].minor.yy46 = sqlite3PExpr(pParse,TK_NOTNULL,yymsp[-2].minor.yy46,0);}
        break;
      case 196: /* expr ::= expr IS expr */
{
  yymsp[-2].minor.yy46 = sqlite3PExpr(pParse,TK_IS,yymsp[-2].minor.yy46,yymsp[0].minor.yy46);
  binaryToUnaryIfNull(pParse, yymsp[0].minor.yy46, yymsp[-2].minor.yy46, TK_ISNULL);
}
        break;
      case 197: /* expr ::= expr IS NOT expr */
{
  yymsp[-3].minor.yy46 = sqlite3PExpr(pParse,TK_ISNOT,yymsp[-3].minor.yy46,yymsp[0].minor.yy46);
  binaryToUnaryIfNull(pParse, yymsp[0].minor.yy46, yymsp[-3].minor.yy46, TK_NOTNULL);
}
        break;
      case 198: /* expr ::= NOT expr */
      case 199: /* expr ::= BITNOT expr */ yytestcase(yyruleno==199);
{yymsp[-1].minor.yy46 = sqlite3PExpr(pParse, yymsp[-1].major, yymsp[0].minor.yy46, 0);/*A-overwrites-B*/}
        break;
      case 200: /* expr ::= PLUS|MINUS expr */
{
  yymsp[-1].minor.yy46 = sqlite3PExpr(pParse, yymsp[-1].major==TK_PLUS ? TK_UPLUS : TK_UMINUS, yymsp[0].minor.yy46, 0);
  /*A-overwrites-B*/
}
        break;
      case 201: /* between_op ::= BETWEEN */
      case 204: /* in_op ::= IN */ yytestcase(yyruleno==204);
{yymsp[0].minor.yy32 = 0;}
        break;
      case 203: /* expr ::= expr between_op expr AND expr */
{
  ExprList *pList = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy46);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[0].minor.yy46);
  yymsp[-4].minor.yy46 = sqlite3PExpr(pParse, TK_BETWEEN, yymsp[-4].minor.yy46, 0);
  if( yymsp[-4].minor.yy46 ){
    yymsp[-4].minor.yy46->x.pList = pList;
  }else{
    sqlite3ExprListDelete(pParse->db, pList);
  } 
  if( yymsp[-3].minor.yy32 ) yymsp[-4].minor.yy46 = sqlite3PExpr(pParse, TK_NOT, yymsp[-4].minor.yy46, 0);
}
        break;
      case 206: /* expr ::= expr in_op LP exprlist RP */
{
    if( yymsp[-1].minor.yy138==0 ){
      /* Expressions of the form
      **
      **      expr1 IN ()
      **      expr1 NOT IN ()
      **
      ** simplify to constants 0 (false) and 1 (true), respectively,
      ** regardless of the value of expr1.
      */
      sqlite3ExprUnmapAndDelete(pParse, yymsp[-4].minor.yy46);
      yymsp[-4].minor.yy46 = sqlite3Expr(pParse->db, TK_INTEGER, yymsp[-3].minor.yy32 ? "1" : "0");
    }else{
      yymsp[-4].minor.yy46 = sqlite3PExpr(pParse, TK_IN, yymsp[-4].minor.yy46, 0);
      if( yymsp[-4].minor.yy46 ){
        yymsp[-4].minor.yy46->x.pList = yymsp[-1].minor.yy138;
        sqlite3ExprSetHeightAndFlags(pParse, yymsp[-4].minor.yy46);
      }else{
        sqlite3ExprListDelete(pParse->db, yymsp[-1].minor.yy138);
      }
      if( yymsp[-3].minor.yy32 ) yymsp[-4].minor.yy46 = sqlite3PExpr(pParse, TK_NOT, yymsp[-4].minor.yy46, 0);
    }
  }
        break;
      case 207: /* expr ::= LP select RP */
{
    yymsp[-2].minor.yy46 = sqlite3PExpr(pParse, TK_SELECT, 0, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-2].minor.yy46, yymsp[-1].minor.yy25);
  }
        break;
      case 208: /* expr ::= expr in_op LP select RP */
{
    yymsp[-4].minor.yy46 = sqlite3PExpr(pParse, TK_IN, yymsp[-4].minor.yy46, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-4].minor.yy46, yymsp[-1].minor.yy25);
    if( yymsp[-3].minor.yy32 ) yymsp[-4].minor.yy46 = sqlite3PExpr(pParse, TK_NOT, yymsp[-4].minor.yy46, 0);
  }
        break;
      case 209: /* expr ::= expr in_op nm dbnm paren_exprlist */
{
    SrcList *pSrc = sqlite3SrcListAppend(pParse, 0,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0);
    Select *pSelect = sqlite3SelectNew(pParse, 0,pSrc,0,0,0,0,0,0);
    if( yymsp[0].minor.yy138 )  sqlite3SrcListFuncArgs(pParse, pSelect ? pSrc : 0, yymsp[0].minor.yy138);
    yymsp[-4].minor.yy46 = sqlite3PExpr(pParse, TK_IN, yymsp[-4].minor.yy46, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-4].minor.yy46, pSelect);
    if( yymsp[-3].minor.yy32 ) yymsp[-4].minor.yy46 = sqlite3PExpr(pParse, TK_NOT, yymsp[-4].minor.yy46, 0);
  }
        break;
      case 210: /* expr ::= EXISTS LP select RP */
{
    Expr *p;
    p = yymsp[-3].minor.yy46 = sqlite3PExpr(pParse, TK_EXISTS, 0, 0);
    sqlite3PExprAddSelect(pParse, p, yymsp[-1].minor.yy25);
  }
        break;
      case 211: /* expr ::= CASE case_operand case_exprlist case_else END */
{
  yymsp[-4].minor.yy46 = sqlite3PExpr(pParse, TK_CASE, yymsp[-3].minor.yy46, 0);
  if( yymsp[-4].minor.yy46 ){
    yymsp[-4].minor.yy46->x.pList = yymsp[-1].minor.yy46 ? sqlite3ExprListAppend(pParse,yymsp[-2].minor.yy138,yymsp[-1].minor.yy46) : yymsp[-2].minor.yy138;
    sqlite3ExprSetHeightAndFlags(pParse, yymsp[-4].minor.yy46);
  }else{
    sqlite3ExprListDelete(pParse->db, yymsp[-2].minor.yy138);
    sqlite3ExprDelete(pParse->db, yymsp[-1].minor.yy46);
  }
}
        break;
      case 212: /* case_exprlist ::= case_exprlist WHEN expr THEN expr */
{
  yymsp[-4].minor.yy138 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy138, yymsp[-2].minor.yy46);
  yymsp[-4].minor.yy138 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy138, yymsp[0].minor.yy46);
}
        break;
      case 213: /* case_exprlist ::= WHEN expr THEN expr */
{
  yymsp[-3].minor.yy138 = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy46);
  yymsp[-3].minor.yy138 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy138, yymsp[0].minor.yy46);
}
        break;
      case 216: /* case_operand ::= expr */
{yymsp[0].minor.yy46 = yymsp[0].minor.yy46; /*A-overwrites-X*/}
        break;
      case 219: /* nexprlist ::= nexprlist COMMA expr */
{yymsp[-2].minor.yy138 = sqlite3ExprListAppend(pParse,yymsp[-2].minor.yy138,yymsp[0].minor.yy46);}
        break;
      case 220: /* nexprlist ::= expr */
{yymsp[0].minor.yy138 = sqlite3ExprListAppend(pParse,0,yymsp[0].minor.yy46); /*A-overwrites-Y*/}
        break;
      case 222: /* paren_exprlist ::= LP exprlist RP */
      case 227: /* eidlist_opt ::= LP eidlist RP */ yytestcase(yyruleno==227);
{yymsp[-2].minor.yy138 = yymsp[-1].minor.yy138;}
        break;
      case 223: /* cmd ::= createkw uniqueflag INDEX ifnotexists nm dbnm ON nm LP sortlist RP where_opt */
{
  sqlite3CreateIndex(pParse, &yymsp[-7].minor.yy0, &yymsp[-6].minor.yy0, 
                     sqlite3SrcListAppend(pParse,0,&yymsp[-4].minor.yy0,0), yymsp[-2].minor.yy138, yymsp[-10].minor.yy32,
                      &yymsp[-11].minor.yy0, yymsp[0].minor.yy46, SQLITE_SO_ASC, yymsp[-8].minor.yy32, SQLITE_IDXTYPE_APPDEF);
  if( IN_RENAME_OBJECT && pParse->pNewIndex ){
    sqlite3RenameTokenMap(pParse, pParse->pNewIndex->zName, &yymsp[-4].minor.yy0);
  }
}
        break;
      case 224: /* uniqueflag ::= UNIQUE */
      case 266: /* raisetype ::= ABORT */ yytestcase(yyruleno==266);
{yymsp[0].minor.yy32 = OE_Abort;}
        break;
      case 225: /* uniqueflag ::= */
{yymsp[1].minor.yy32 = OE_None;}
        break;
      case 228: /* eidlist ::= eidlist COMMA nm collate sortorder */
{
  yymsp[-4].minor.yy138 = parserAddExprIdListTerm(pParse, yymsp[-4].minor.yy138, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy32, yymsp[0].minor.yy32);
}
        break;
      case 229: /* eidlist ::= nm collate sortorder */
{
  yymsp[-2].minor.yy138 = parserAddExprIdListTerm(pParse, 0, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy32, yymsp[0].minor.yy32); /*A-overwrites-Y*/
}
        break;
      case 232: /* cmd ::= DROP INDEX ifexists fullname */
{sqlite3DropIndex(pParse, yymsp[0].minor.yy609, yymsp[-1].minor.yy32);}
        break;
      case 233: /* cmd ::= VACUUM vinto */
{sqlite3Vacuum(pParse,0,yymsp[0].minor.yy46);}
        break;
      case 234: /* cmd ::= VACUUM nm vinto */
{sqlite3Vacuum(pParse,&yymsp[-1].minor.yy0,yymsp[0].minor.yy46);}
        break;
      case 237: /* cmd ::= PRAGMA nm dbnm */
{sqlite3Pragma(pParse,&yymsp[-1].minor.yy0,&yymsp[0].minor.yy0,0,0);}
        break;
      case 238: /* cmd ::= PRAGMA nm dbnm EQ nmnum */
{sqlite3Pragma(pParse,&yymsp[-3].minor.yy0,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0,0);}
        break;
      case 239: /* cmd ::= PRAGMA nm dbnm LP nmnum RP */
{sqlite3Pragma(pParse,&yymsp[-4].minor.yy0,&yymsp[-3].minor.yy0,&yymsp[-1].minor.yy0,0);}
        break;
      case 240: /* cmd ::= PRAGMA nm dbnm EQ minus_num */
{sqlite3Pragma(pParse,&yymsp[-3].minor.yy0,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0,1);}
        break;
      case 241: /* cmd ::= PRAGMA nm dbnm LP minus_num RP */
{sqlite3Pragma(pParse,&yymsp[-4].minor.yy0,&yymsp[-3].minor.yy0,&yymsp[-1].minor.yy0,1);}
        break;
      case 244: /* cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END */
{
  Token all;
  all.z = yymsp[-3].minor.yy0.z;
  all.n = (int)(yymsp[0].minor.yy0.z - yymsp[-3].minor.yy0.z) + yymsp[0].minor.yy0.n;
  sqlite3FinishTrigger(pParse, yymsp[-1].minor.yy527, &all);
}
        break;
      case 245: /* trigger_decl ::= temp TRIGGER ifnotexists nm dbnm trigger_time trigger_event ON fullname foreach_clause when_clause */
{
  sqlite3BeginTrigger(pParse, &yymsp[-7].minor.yy0, &yymsp[-6].minor.yy0, yymsp[-5].minor.yy32, yymsp[-4].minor.yy572.a, yymsp[-4].minor.yy572.b, yymsp[-2].minor.yy609, yymsp[0].minor.yy46, yymsp[-10].minor.yy32, yymsp[-8].minor.yy32);
  yymsp[-10].minor.yy0 = (yymsp[-6].minor.yy0.n==0?yymsp[-7].minor.yy0:yymsp[-6].minor.yy0); /*A-overwrites-T*/
}
        break;
      case 246: /* trigger_time ::= BEFORE|AFTER */
{ yymsp[0].minor.yy32 = yymsp[0].major; /*A-overwrites-X*/ }
        break;
      case 247: /* trigger_time ::= INSTEAD OF */
{ yymsp[-1].minor.yy32 = TK_INSTEAD;}
        break;
      case 248: /* trigger_time ::= */
{ yymsp[1].minor.yy32 = TK_BEFORE; }
        break;
      case 249: /* trigger_event ::= DELETE|INSERT */
      case 250: /* trigger_event ::= UPDATE */ yytestcase(yyruleno==250);
{yymsp[0].minor.yy572.a = yymsp[0].major; /*A-overwrites-X*/ yymsp[0].minor.yy572.b = 0;}
        break;
      case 251: /* trigger_event ::= UPDATE OF idlist */
{yymsp[-2].minor.yy572.a = TK_UPDATE; yymsp[-2].minor.yy572.b = yymsp[0].minor.yy406;}
        break;
      case 252: /* when_clause ::= */
      case 271: /* key_opt ::= */ yytestcase(yyruleno==271);
{ yymsp[1].minor.yy46 = 0; }
        break;
      case 253: /* when_clause ::= WHEN expr */
      case 272: /* key_opt ::= KEY expr */ yytestcase(yyruleno==272);
{ yymsp[-1].minor.yy46 = yymsp[0].minor.yy46; }
        break;
      case 254: /* trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI */
{
  assert( yymsp[-2].minor.yy527!=0 );
  yymsp[-2].minor.yy527->pLast->pNext = yymsp[-1].minor.yy527;
  yymsp[-2].minor.yy527->pLast = yymsp[-1].minor.yy527;
}
        break;
      case 255: /* trigger_cmd_list ::= trigger_cmd SEMI */
{ 
  assert( yymsp[-1].minor.yy527!=0 );
  yymsp[-1].minor.yy527->pLast = yymsp[-1].minor.yy527;
}
        break;
      case 256: /* trnm ::= nm DOT nm */
{
  yymsp[-2].minor.yy0 = yymsp[0].minor.yy0;
  sqlite3ErrorMsg(pParse, 
        "qualified table names are not allowed on INSERT, UPDATE, and DELETE "
        "statements within triggers");
}
        break;
      case 257: /* tridxby ::= INDEXED BY nm */
{
  sqlite3ErrorMsg(pParse,
        "the INDEXED BY clause is not allowed on UPDATE or DELETE statements "
        "within triggers");
}
        break;
      case 258: /* tridxby ::= NOT INDEXED */
{
  sqlite3ErrorMsg(pParse,
        "the NOT INDEXED clause is not allowed on UPDATE or DELETE statements "
        "within triggers");
}
        break;
      case 259: /* trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt scanpt */
{yylhsminor.yy527 = sqlite3TriggerUpdateStep(pParse, &yymsp[-5].minor.yy0, yymsp[-2].minor.yy138, yymsp[-1].minor.yy46, yymsp[-6].minor.yy32, yymsp[-7].minor.yy0.z, yymsp[0].minor.yy8);}
  yymsp[-7].minor.yy527 = yylhsminor.yy527;
        break;
      case 260: /* trigger_cmd ::= scanpt insert_cmd INTO trnm idlist_opt select upsert scanpt */
{
   yylhsminor.yy527 = sqlite3TriggerInsertStep(pParse,&yymsp[-4].minor.yy0,yymsp[-3].minor.yy406,yymsp[-2].minor.yy25,yymsp[-6].minor.yy32,yymsp[-1].minor.yy288,yymsp[-7].minor.yy8,yymsp[0].minor.yy8);/*yylhsminor.yy527-overwrites-yymsp[-6].minor.yy32*/
}
  yymsp[-7].minor.yy527 = yylhsminor.yy527;
        break;
      case 261: /* trigger_cmd ::= DELETE FROM trnm tridxby where_opt scanpt */
{yylhsminor.yy527 = sqlite3TriggerDeleteStep(pParse, &yymsp[-3].minor.yy0, yymsp[-1].minor.yy46, yymsp[-5].minor.yy0.z, yymsp[0].minor.yy8);}
  yymsp[-5].minor.yy527 = yylhsminor.yy527;
        break;
      case 262: /* trigger_cmd ::= scanpt select scanpt */
{yylhsminor.yy527 = sqlite3TriggerSelectStep(pParse->db, yymsp[-1].minor.yy25, yymsp[-2].minor.yy8, yymsp[0].minor.yy8); /*yylhsminor.yy527-overwrites-yymsp[-1].minor.yy25*/}
  yymsp[-2].minor.yy527 = yylhsminor.yy527;
        break;
      case 263: /* expr ::= RAISE LP IGNORE RP */
{
  yymsp[-3].minor.yy46 = sqlite3PExpr(pParse, TK_RAISE, 0, 0); 
  if( yymsp[-3].minor.yy46 ){
    yymsp[-3].minor.yy46->affExpr = OE_Ignore;
  }
}
        break;
      case 264: /* expr ::= RAISE LP raisetype COMMA nm RP */
{
  yymsp[-5].minor.yy46 = sqlite3ExprAlloc(pParse->db, TK_RAISE, &yymsp[-1].minor.yy0, 1); 
  if( yymsp[-5].minor.yy46 ) {
    yymsp[-5].minor.yy46->affExpr = (char)yymsp[-3].minor.yy32;
  }
}
        break;
      case 265: /* raisetype ::= ROLLBACK */
{yymsp[0].minor.yy32 = OE_Rollback;}
        break;
      case 267: /* raisetype ::= FAIL */
{yymsp[0].minor.yy32 = OE_Fail;}
        break;
      case 268: /* cmd ::= DROP TRIGGER ifexists fullname */
{
  sqlite3DropTrigger(pParse,yymsp[0].minor.yy609,yymsp[-1].minor.yy32);
}
        break;
      case 269: /* cmd ::= ATTACH database_kw_opt expr AS expr key_opt */
{
  sqlite3Attach(pParse, yymsp[-3].minor.yy46, yymsp[-1].minor.yy46, yymsp[0].minor.yy46);
}
        break;
      case 270: /* cmd ::= DETACH database_kw_opt expr */
{
  sqlite3Detach(pParse, yymsp[0].minor.yy46);
}
        break;
      case 273: /* cmd ::= REINDEX */
{sqlite3Reindex(pParse, 0, 0);}
        break;
      case 274: /* cmd ::= REINDEX nm dbnm */
{sqlite3Reindex(pParse, &yymsp[-1].minor.yy0, &yymsp[0].minor.yy0);}
        break;
      case 275: /* cmd ::= ANALYZE */
{sqlite3Analyze(pParse, 0, 0);}
        break;
      case 276: /* cmd ::= ANALYZE nm dbnm */
{sqlite3Analyze(pParse, &yymsp[-1].minor.yy0, &yymsp[0].minor.yy0);}
        break;
      case 277: /* cmd ::= ALTER TABLE fullname RENAME TO nm */
{
  sqlite3AlterRenameTable(pParse,yymsp[-3].minor.yy609,&yymsp[0].minor.yy0);
}
        break;
      case 278: /* cmd ::= ALTER TABLE add_column_fullname ADD kwcolumn_opt columnname carglist */
{
  yymsp[-1].minor.yy0.n = (int)(pParse->sLastToken.z-yymsp[-1].minor.yy0.z) + pParse->sLastToken.n;
  sqlite3AlterFinishAddColumn(pParse, &yymsp[-1].minor.yy0);
}
        break;
      case 279: /* add_column_fullname ::= fullname */
{
  disableLookaside(pParse);
  sqlite3AlterBeginAddColumn(pParse, yymsp[0].minor.yy609);
}
        break;
      case 280: /* cmd ::= ALTER TABLE fullname RENAME kwcolumn_opt nm TO nm */
{
  sqlite3AlterRenameColumn(pParse, yymsp[-5].minor.yy609, &yymsp[-2].minor.yy0, &yymsp[0].minor.yy0);
}
        break;
      case 281: /* cmd ::= create_vtab */
{sqlite3VtabFinishParse(pParse,0);}
        break;
      case 282: /* cmd ::= create_vtab LP vtabarglist RP */
{sqlite3VtabFinishParse(pParse,&yymsp[0].minor.yy0);}
        break;
      case 283: /* create_vtab ::= createkw VIRTUAL TABLE ifnotexists nm dbnm USING nm */
{
    sqlite3VtabBeginParse(pParse, &yymsp[-3].minor.yy0, &yymsp[-2].minor.yy0, &yymsp[0].minor.yy0, yymsp[-4].minor.yy32);
}
        break;
      case 284: /* vtabarg ::= */
{sqlite3VtabArgInit(pParse);}
        break;
      case 285: /* vtabargtoken ::= ANY */
      case 286: /* vtabargtoken ::= lp anylist RP */ yytestcase(yyruleno==286);
      case 287: /* lp ::= LP */ yytestcase(yyruleno==287);
{sqlite3VtabArgExtend(pParse,&yymsp[0].minor.yy0);}
        break;
      case 288: /* with ::= WITH wqlist */
      case 289: /* with ::= WITH RECURSIVE wqlist */ yytestcase(yyruleno==289);
{ sqlite3WithPush(pParse, yymsp[0].minor.yy297, 1); }
        break;
      case 290: /* wqlist ::= nm eidlist_opt AS LP select RP */
{
  yymsp[-5].minor.yy297 = sqlite3WithAdd(pParse, 0, &yymsp[-5].minor.yy0, yymsp[-4].minor.yy138, yymsp[-1].minor.yy25); /*A-overwrites-X*/
}
        break;
      case 291: /* wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP */
{
  yymsp[-7].minor.yy297 = sqlite3WithAdd(pParse, yymsp[-7].minor.yy297, &yymsp[-5].minor.yy0, yymsp[-4].minor.yy138, yymsp[-1].minor.yy25);
}
        break;
      case 292: /* windowdefn_list ::= windowdefn */
{ yylhsminor.yy455 = yymsp[0].minor.yy455; }
  yymsp[0].minor.yy455 = yylhsminor.yy455;
        break;
      case 293: /* windowdefn_list ::= windowdefn_list COMMA windowdefn */
{
  assert( yymsp[0].minor.yy455!=0 );
  sqlite3WindowChain(pParse, yymsp[0].minor.yy455, yymsp[-2].minor.yy455);
  yymsp[0].minor.yy455->pNextWin = yymsp[-2].minor.yy455;
  yylhsminor.yy455 = yymsp[0].minor.yy455;
}
  yymsp[-2].minor.yy455 = yylhsminor.yy455;
        break;
      case 294: /* windowdefn ::= nm AS LP window RP */
{
  if( ALWAYS(yymsp[-1].minor.yy455) ){
    yymsp[-1].minor.yy455->zName = sqlite3DbStrNDup(pParse->db, yymsp[-4].minor.yy0.z, yymsp[-4].minor.yy0.n);
  }
  yylhsminor.yy455 = yymsp[-1].minor.yy455;
}
  yymsp[-4].minor.yy455 = yylhsminor.yy455;
        break;
      case 295: /* window ::= PARTITION BY nexprlist orderby_opt frame_opt */
{
  yymsp[-4].minor.yy455 = sqlite3WindowAssemble(pParse, yymsp[0].minor.yy455, yymsp[-2].minor.yy138, yymsp[-1].minor.yy138, 0);
}
        break;
      case 296: /* window ::= nm PARTITION BY nexprlist orderby_opt frame_opt */
{
  yylhsminor.yy455 = sqlite3WindowAssemble(pParse, yymsp[0].minor.yy455, yymsp[-2].minor.yy138, yymsp[-1].minor.yy138, &yymsp[-5].minor.yy0);
}
  yymsp[-5].minor.yy455 = yylhsminor.yy455;
        break;
      case 297: /* window ::= ORDER BY sortlist frame_opt */
{
  yymsp[-3].minor.yy455 = sqlite3WindowAssemble(pParse, yymsp[0].minor.yy455, 0, yymsp[-1].minor.yy138, 0);
}
        break;
      case 298: /* window ::= nm ORDER BY sortlist frame_opt */
{
  yylhsminor.yy455 = sqlite3WindowAssemble(pParse, yymsp[0].minor.yy455, 0, yymsp[-1].minor.yy138, &yymsp[-4].minor.yy0);
}
  yymsp[-4].minor.yy455 = yylhsminor.yy455;
        break;
      case 299: /* window ::= frame_opt */
      case 318: /* filter_over ::= over_clause */ yytestcase(yyruleno==318);
{
  yylhsminor.yy455 = yymsp[0].minor.yy455;
}
  yymsp[0].minor.yy455 = yylhsminor.yy455;
        break;
      case 300: /* window ::= nm frame_opt */
{
  yylhsminor.yy455 = sqlite3WindowAssemble(pParse, yymsp[0].minor.yy455, 0, 0, &yymsp[-1].minor.yy0);
}
  yymsp[-1].minor.yy455 = yylhsminor.yy455;
        break;
      case 301: /* frame_opt ::= */
{ 
  yymsp[1].minor.yy455 = sqlite3WindowAlloc(pParse, 0, TK_UNBOUNDED, 0, TK_CURRENT, 0, 0);
}
        break;
      case 302: /* frame_opt ::= range_or_rows frame_bound_s frame_exclude_opt */
{ 
  yylhsminor.yy455 = sqlite3WindowAlloc(pParse, yymsp[-2].minor.yy32, yymsp[-1].minor.yy57.eType, yymsp[-1].minor.yy57.pExpr, TK_CURRENT, 0, yymsp[0].minor.yy118);
}
  yymsp[-2].minor.yy455 = yylhsminor.yy455;
        break;
      case 303: /* frame_opt ::= range_or_rows BETWEEN frame_bound_s AND frame_bound_e frame_exclude_opt */
{ 
  yylhsminor.yy455 = sqlite3WindowAlloc(pParse, yymsp[-5].minor.yy32, yymsp[-3].minor.yy57.eType, yymsp[-3].minor.yy57.pExpr, yymsp[-1].minor.yy57.eType, yymsp[-1].minor.yy57.pExpr, yymsp[0].minor.yy118);
}
  yymsp[-5].minor.yy455 = yylhsminor.yy455;
        break;
      case 305: /* frame_bound_s ::= frame_bound */
      case 307: /* frame_bound_e ::= frame_bound */ yytestcase(yyruleno==307);
{yylhsminor.yy57 = yymsp[0].minor.yy57;}
  yymsp[0].minor.yy57 = yylhsminor.yy57;
        break;
      case 306: /* frame_bound_s ::= UNBOUNDED PRECEDING */
      case 308: /* frame_bound_e ::= UNBOUNDED FOLLOWING */ yytestcase(yyruleno==308);
      case 310: /* frame_bound ::= CURRENT ROW */ yytestcase(yyruleno==310);
{yylhsminor.yy57.eType = yymsp[-1].major; yylhsminor.yy57.pExpr = 0;}
  yymsp[-1].minor.yy57 = yylhsminor.yy57;
        break;
      case 309: /* frame_bound ::= expr PRECEDING|FOLLOWING */
{yylhsminor.yy57.eType = yymsp[0].major; yylhsminor.yy57.pExpr = yymsp[-1].minor.yy46;}
  yymsp[-1].minor.yy57 = yylhsminor.yy57;
        break;
      case 311: /* frame_exclude_opt ::= */
{yymsp[1].minor.yy118 = 0;}
        break;
      case 312: /* frame_exclude_opt ::= EXCLUDE frame_exclude */
{yymsp[-1].minor.yy118 = yymsp[0].minor.yy118;}
        break;
      case 313: /* frame_exclude ::= NO OTHERS */
      case 314: /* frame_exclude ::= CURRENT ROW */ yytestcase(yyruleno==314);
{yymsp[-1].minor.yy118 = yymsp[-1].major; /*A-overwrites-X*/}
        break;
      case 315: /* frame_exclude ::= GROUP|TIES */
{yymsp[0].minor.yy118 = yymsp[0].major; /*A-overwrites-X*/}
        break;
      case 316: /* window_clause ::= WINDOW windowdefn_list */
{ yymsp[-1].minor.yy455 = yymsp[0].minor.yy455; }
        break;
      case 317: /* filter_over ::= filter_clause over_clause */
{
  yymsp[0].minor.yy455->pFilter = yymsp[-1].minor.yy46;
  yylhsminor.yy455 = yymsp[0].minor.yy455;
}
  yymsp[-1].minor.yy455 = yylhsminor.yy455;
        break;
      case 319: /* filter_over ::= filter_clause */
{
  yylhsminor.yy455 = (Window*)sqlite3DbMallocZero(pParse->db, sizeof(Window));
  if( yylhsminor.yy455 ){
    yylhsminor.yy455->eFrmType = TK_FILTER;
    yylhsminor.yy455->pFilter = yymsp[0].minor.yy46;
  }else{
    sqlite3ExprDelete(pParse->db, yymsp[0].minor.yy46);
  }
}
  yymsp[0].minor.yy455 = yylhsminor.yy455;
        break;
      case 320: /* over_clause ::= OVER LP window RP */
{
  yymsp[-3].minor.yy455 = yymsp[-1].minor.yy455;
  assert( yymsp[-3].minor.yy455!=0 );
}
        break;
      case 321: /* over_clause ::= OVER nm */
{
  yymsp[-1].minor.yy455 = (Window*)sqlite3DbMallocZero(pParse->db, sizeof(Window));
  if( yymsp[-1].minor.yy455 ){
    yymsp[-1].minor.yy455->zName = sqlite3DbStrNDup(pParse->db, yymsp[0].minor.yy0.z, yymsp[0].minor.yy0.n);
  }
}
        break;
      case 322: /* filter_clause ::= FILTER LP WHERE expr RP */
{ yymsp[-4].minor.yy46 = yymsp[-1].minor.yy46; }
        break;
      default:
      /* (323) input ::= cmdlist */ yytestcase(yyruleno==323);
      /* (324) cmdlist ::= cmdlist ecmd */ yytestcase(yyruleno==324);
      /* (325) cmdlist ::= ecmd (OPTIMIZED OUT) */ assert(yyruleno!=325);
      /* (326) ecmd ::= SEMI */ yytestcase(yyruleno==326);
      /* (327) ecmd ::= cmdx SEMI */ yytestcase(yyruleno==327);
      /* (328) ecmd ::= explain cmdx */ yytestcase(yyruleno==328);
      /* (329) trans_opt ::= */ yytestcase(yyruleno==329);
      /* (330) trans_opt ::= TRANSACTION */ yytestcase(yyruleno==330);
      /* (331) trans_opt ::= TRANSACTION nm */ yytestcase(yyruleno==331);
      /* (332) savepoint_opt ::= SAVEPOINT */ yytestcase(yyruleno==332);
      /* (333) savepoint_opt ::= */ yytestcase(yyruleno==333);
      /* (334) cmd ::= create_table create_table_args */ yytestcase(yyruleno==334);
      /* (335) columnlist ::= columnlist COMMA columnname carglist */ yytestcase(yyruleno==335);
      /* (336) columnlist ::= columnname carglist */ yytestcase(yyruleno==336);
      /* (337) nm ::= ID|INDEXED */ yytestcase(yyruleno==337);
      /* (338) nm ::= STRING */ yytestcase(yyruleno==338);
      /* (339) nm ::= JOIN_KW */ yytestcase(yyruleno==339);
      /* (340) typetoken ::= typename */ yytestcase(yyruleno==340);
      /* (341) typename ::= ID|STRING */ yytestcase(yyruleno==341);
      /* (342) signed ::= plus_num (OPTIMIZED OUT) */ assert(yyruleno!=342);
      /* (343) signed ::= minus_num (OPTIMIZED OUT) */ assert(yyruleno!=343);
      /* (344) carglist ::= carglist ccons */ yytestcase(yyruleno==344);
      /* (345) carglist ::= */ yytestcase(yyruleno==345);
      /* (346) ccons ::= NULL onconf */ yytestcase(yyruleno==346);
      /* (347) conslist_opt ::= COMMA conslist */ yytestcase(yyruleno==347);
      /* (348) conslist ::= conslist tconscomma tcons */ yytestcase(yyruleno==348);
      /* (349) conslist ::= tcons (OPTIMIZED OUT) */ assert(yyruleno!=349);
      /* (350) tconscomma ::= */ yytestcase(yyruleno==350);
      /* (351) defer_subclause_opt ::= defer_subclause (OPTIMIZED OUT) */ assert(yyruleno!=351);
      /* (352) resolvetype ::= raisetype (OPTIMIZED OUT) */ assert(yyruleno!=352);
      /* (353) selectnowith ::= oneselect (OPTIMIZED OUT) */ assert(yyruleno!=353);
      /* (354) oneselect ::= values */ yytestcase(yyruleno==354);
      /* (355) sclp ::= selcollist COMMA */ yytestcase(yyruleno==355);
      /* (356) as ::= ID|STRING */ yytestcase(yyruleno==356);
      /* (357) expr ::= term (OPTIMIZED OUT) */ assert(yyruleno!=357);
      /* (358) likeop ::= LIKE_KW|MATCH */ yytestcase(yyruleno==358);
      /* (359) exprlist ::= nexprlist */ yytestcase(yyruleno==359);
      /* (360) nmnum ::= plus_num (OPTIMIZED OUT) */ assert(yyruleno!=360);
      /* (361) nmnum ::= nm (OPTIMIZED OUT) */ assert(yyruleno!=361);
      /* (362) nmnum ::= ON */ yytestcase(yyruleno==362);
      /* (363) nmnum ::= DELETE */ yytestcase(yyruleno==363);
      /* (364) nmnum ::= DEFAULT */ yytestcase(yyruleno==364);
      /* (365) plus_num ::= INTEGER|FLOAT */ yytestcase(yyruleno==365);
      /* (366) foreach_clause ::= */ yytestcase(yyruleno==366);
      /* (367) foreach_clause ::= FOR EACH ROW */ yytestcase(yyruleno==367);
      /* (368) trnm ::= nm */ yytestcase(yyruleno==368);
      /* (369) tridxby ::= */ yytestcase(yyruleno==369);
      /* (370) database_kw_opt ::= DATABASE */ yytestcase(yyruleno==370);
      /* (371) database_kw_opt ::= */ yytestcase(yyruleno==371);
      /* (372) kwcolumn_opt ::= */ yytestcase(yyruleno==372);
      /* (373) kwcolumn_opt ::= COLUMNKW */ yytestcase(yyruleno==373);
      /* (374) vtabarglist ::= vtabarg */ yytestcase(yyruleno==374);
      /* (375) vtabarglist ::= vtabarglist COMMA vtabarg */ yytestcase(yyruleno==375);
      /* (376) vtabarg ::= vtabarg vtabargtoken */ yytestcase(yyruleno==376);
      /* (377) anylist ::= */ yytestcase(yyruleno==377);
      /* (378) anylist ::= anylist LP anylist RP */ yytestcase(yyruleno==378);
      /* (379) anylist ::= anylist ANY */ yytestcase(yyruleno==379);
      /* (380) with ::= */ yytestcase(yyruleno==380);
        break;
/********** End reduce actions ************************************************/
  };
  assert( yyruleno<sizeof(yyRuleInfoLhs)/sizeof(yyRuleInfoLhs[0]) );
  yygoto = yyRuleInfoLhs[yyruleno];
  yysize = yyRuleInfoNRhs[yyruleno];
  yyact = yy_find_reduce_action(yymsp[yysize].stateno,(YYCODETYPE)yygoto);

  /* There are no SHIFTREDUCE actions on nonterminals because the table
  ** generator has simplified them to pure REDUCE actions. */
  assert( !(yyact>YY_MAX_SHIFT && yyact<=YY_MAX_SHIFTREDUCE) );

  /* It is not possible for a REDUCE to be followed by an error */
  assert( yyact!=YY_ERROR_ACTION );

  yymsp += yysize+1;
  yypParser->yytos = yymsp;
  yymsp->stateno = (YYACTIONTYPE)yyact;
  yymsp->major = (YYCODETYPE)yygoto;
  yyTraceShift(yypParser, yyact, "... then shift");
  return yyact;
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH
  sqlite3ParserCTX_FETCH
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
/************ Begin %parse_failure code ***************************************/
/************ End %parse_failure code *****************************************/
  sqlite3ParserARG_STORE /* Suppress warning about unused %extra_argument variable */
  sqlite3ParserCTX_STORE
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  sqlite3ParserTOKENTYPE yyminor         /* The minor type of the error token */
){
  sqlite3ParserARG_FETCH
  sqlite3ParserCTX_FETCH
#define TOKEN yyminor
/************ Begin %syntax_error code ****************************************/

  UNUSED_PARAMETER(yymajor);  /* Silence some compiler warnings */
  if( TOKEN.z[0] ){
    sqlite3ErrorMsg(pParse, "near \"%T\": syntax error", &TOKEN);
  }else{
    sqlite3ErrorMsg(pParse, "incomplete input");
  }
/************ End %syntax_error code ******************************************/
  sqlite3ParserARG_STORE /* Suppress warning about unused %extra_argument variable */
  sqlite3ParserCTX_STORE
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH
  sqlite3ParserCTX_FETCH
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
#ifndef YYNOERRORRECOVERY
  yypParser->yyerrcnt = -1;
#endif
  assert( yypParser->yytos==yypParser->yystack );
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
/*********** Begin %parse_accept code *****************************************/
/*********** End %parse_accept code *******************************************/
  sqlite3ParserARG_STORE /* Suppress warning about unused %extra_argument variable */
  sqlite3ParserCTX_STORE
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "sqlite3ParserAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
SQLITE_PRIVATE void sqlite3Parser(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  sqlite3ParserTOKENTYPE yyminor       /* The value for the token */
  sqlite3ParserARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  YYACTIONTYPE yyact;   /* The parser action. */
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  int yyendofinput;     /* True if we are at the end of input */
#endif
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser = (yyParser*)yyp;  /* The parser */
  sqlite3ParserCTX_FETCH
  sqlite3ParserARG_STORE

  assert( yypParser->yytos!=0 );
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  yyendofinput = (yymajor==0);
#endif

  yyact = yypParser->yytos->stateno;
#ifndef NDEBUG
  if( yyTraceFILE ){
    if( yyact < YY_MIN_REDUCE ){
      fprintf(yyTraceFILE,"%sInput '%s' in state %d\n",
              yyTracePrompt,yyTokenName[yymajor],yyact);
    }else{
      fprintf(yyTraceFILE,"%sInput '%s' with pending reduce %d\n",
              yyTracePrompt,yyTokenName[yymajor],yyact-YY_MIN_REDUCE);
    }
  }
#endif

  do{
    assert( yyact==yypParser->yytos->stateno );
    yyact = yy_find_shift_action((YYCODETYPE)yymajor,yyact);
    if( yyact >= YY_MIN_REDUCE ){
      yyact = yy_reduce(yypParser,yyact-YY_MIN_REDUCE,yymajor,
                        yyminor sqlite3ParserCTX_PARAM);
    }else if( yyact <= YY_MAX_SHIFTREDUCE ){
      yy_shift(yypParser,yyact,(YYCODETYPE)yymajor,yyminor);
#ifndef YYNOERRORRECOVERY
      yypParser->yyerrcnt--;
#endif
      break;
    }else if( yyact==YY_ACCEPT_ACTION ){
      yypParser->yytos--;
      yy_accept(yypParser);
      return;
    }else{
      assert( yyact == YY_ERROR_ACTION );
      yyminorunion.yy0 = yyminor;
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminor);
      }
      yymx = yypParser->yytos->major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor, &yyminorunion);
        yymajor = YYNOCODE;
      }else{
        while( yypParser->yytos >= yypParser->yystack
            && (yyact = yy_find_reduce_action(
                        yypParser->yytos->stateno,
                        YYERRORSYMBOL)) > YY_MAX_SHIFTREDUCE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yytos < yypParser->yystack || yymajor==0 ){
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
          yypParser->yyerrcnt = -1;
#endif
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          yy_shift(yypParser,yyact,YYERRORSYMBOL,yyminor);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
      if( yymajor==YYNOCODE ) break;
      yyact = yypParser->yytos->stateno;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor, yyminor);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      break;
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor, yyminor);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
        yypParser->yyerrcnt = -1;
#endif
      }
      break;
#endif
    }
  }while( yypParser->yytos>yypParser->yystack );
#ifndef NDEBUG
  if( yyTraceFILE ){
    yyStackEntry *i;
    char cDiv = '[';
    fprintf(yyTraceFILE,"%sReturn. Stack=",yyTracePrompt);
    for(i=&yypParser->yystack[1]; i<=yypParser->yytos; i++){
      fprintf(yyTraceFILE,"%c%s", cDiv, yyTokenName[i->major]);
      cDiv = ' ';
    }
    fprintf(yyTraceFILE,"]\n");
  }
#endif
  return;
}

/*
** Return the fallback token corresponding to canonical token iToken, or
** 0 if iToken has no fallback.
*/
SQLITE_PRIVATE int sqlite3ParserFallback(int iToken){
#ifdef YYFALLBACK
  assert( iToken<(int)(sizeof(yyFallback)/sizeof(yyFallback[0])) );
  return yyFallback[iToken];
#else
  (void)iToken;
#endif
  return 0;
}

/************** End of parse.c ***********************************************/
/************** Begin file tokenize.c ****************************************/
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
** An tokenizer for SQL
**
** This file contains C code that splits an SQL input string up into
** individual tokens and sends those tokens one-by-one over to the
** parser for analysis.
*/
/* #include "sqliteInt.h" */
/* #include <stdlib.h> */

/* Character classes for tokenizing
**
** In the sqlite3GetToken() function, a switch() on aiClass[c] is implemented
** using a lookup table, whereas a switch() directly on c uses a binary search.
** The lookup table is much faster.  To maximize speed, and to ensure that
** a lookup table is used, all of the classes need to be small integers and
** all of them need to be used within the switch.
*/
#define CC_X          0    /* The letter 'x', or start of BLOB literal */
#define CC_KYWD       1    /* Alphabetics or '_'.  Usable in a keyword */
#define CC_ID         2    /* unicode characters usable in IDs */
#define CC_DIGIT      3    /* Digits */
#define CC_DOLLAR     4    /* '$' */
#define CC_VARALPHA   5    /* '@', '#', ':'.  Alphabetic SQL variables */
#define CC_VARNUM     6    /* '?'.  Numeric SQL variables */
#define CC_SPACE      7    /* Space characters */
#define CC_QUOTE      8    /* '"', '\'', or '`'.  String literals, quoted ids */
#define CC_QUOTE2     9    /* '['.   [...] style quoted ids */
#define CC_PIPE      10    /* '|'.   Bitwise OR or concatenate */
#define CC_MINUS     11    /* '-'.  Minus or SQL-style comment */
#define CC_LT        12    /* '<'.  Part of < or <= or <> */
#define CC_GT        13    /* '>'.  Part of > or >= */
#define CC_EQ        14    /* '='.  Part of = or == */
#define CC_BANG      15    /* '!'.  Part of != */
#define CC_SLASH     16    /* '/'.  / or c-style comment */
#define CC_LP        17    /* '(' */
#define CC_RP        18    /* ')' */
#define CC_SEMI      19    /* ';' */
#define CC_PLUS      20    /* '+' */
#define CC_STAR      21    /* '*' */
#define CC_PERCENT   22    /* '%' */
#define CC_COMMA     23    /* ',' */
#define CC_AND       24    /* '&' */
#define CC_TILDA     25    /* '~' */
#define CC_DOT       26    /* '.' */
#define CC_ILLEGAL   27    /* Illegal character */
#define CC_NUL       28    /* 0x00 */

static const unsigned char aiClass[] = {
#ifdef SQLITE_ASCII
/*         x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xa  xb  xc  xd  xe  xf */
/* 0x */   28, 27, 27, 27, 27, 27, 27, 27, 27,  7,  7, 27,  7,  7, 27, 27,
/* 1x */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
/* 2x */    7, 15,  8,  5,  4, 22, 24,  8, 17, 18, 21, 20, 23, 11, 26, 16,
/* 3x */    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  5, 19, 12, 14, 13,  6,
/* 4x */    5,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* 5x */    1,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  9, 27, 27, 27,  1,
/* 6x */    8,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* 7x */    1,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1, 27, 10, 27, 25, 27,
/* 8x */    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
/* 9x */    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
/* Ax */    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
/* Bx */    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
/* Cx */    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
/* Dx */    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
/* Ex */    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
/* Fx */    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2
#endif
#ifdef SQLITE_EBCDIC
/*         x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xa  xb  xc  xd  xe  xf */
/* 0x */   27, 27, 27, 27, 27,  7, 27, 27, 27, 27, 27, 27,  7,  7, 27, 27,
/* 1x */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
/* 2x */   27, 27, 27, 27, 27,  7, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
/* 3x */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
/* 4x */    7, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 26, 12, 17, 20, 10,
/* 5x */   24, 27, 27, 27, 27, 27, 27, 27, 27, 27, 15,  4, 21, 18, 19, 27,
/* 6x */   11, 16, 27, 27, 27, 27, 27, 27, 27, 27, 27, 23, 22,  1, 13,  6,
/* 7x */   27, 27, 27, 27, 27, 27, 27, 27, 27,  8,  5,  5,  5,  8, 14,  8,
/* 8x */   27,  1,  1,  1,  1,  1,  1,  1,  1,  1, 27, 27, 27, 27, 27, 27,
/* 9x */   27,  1,  1,  1,  1,  1,  1,  1,  1,  1, 27, 27, 27, 27, 27, 27,
/* Ax */   27, 25,  1,  1,  1,  1,  1,  0,  1,  1, 27, 27, 27, 27, 27, 27,
/* Bx */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27,  9, 27, 27, 27, 27, 27,
/* Cx */   27,  1,  1,  1,  1,  1,  1,  1,  1,  1, 27, 27, 27, 27, 27, 27,
/* Dx */   27,  1,  1,  1,  1,  1,  1,  1,  1,  1, 27, 27, 27, 27, 27, 27,
/* Ex */   27, 27,  1,  1,  1,  1,  1,  0,  1,  1, 27, 27, 27, 27, 27, 27,
/* Fx */    3,  3,  3,  3,  3,  3,  3,  3,  3,  3, 27, 27, 27, 27, 27, 27,
#endif
};

/*
** The charMap() macro maps alphabetic characters (only) into their
** lower-case ASCII equivalent.  On ASCII machines, this is just
** an upper-to-lower case map.  On EBCDIC machines we also need
** to adjust the encoding.  The mapping is only valid for alphabetics
** which are the only characters for which this feature is used. 
**
** Used by keywordhash.h
*/
#ifdef SQLITE_ASCII
# define charMap(X) sqlite3UpperToLower[(unsigned char)X]
#endif
#ifdef SQLITE_EBCDIC
# define charMap(X) ebcdicToAscii[(unsigned char)X]
const unsigned char ebcdicToAscii[] = {
/* 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 0x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 1x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 2x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 3x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 4x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 5x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 95,  0,  0,  /* 6x */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* 7x */
   0, 97, 98, 99,100,101,102,103,104,105,  0,  0,  0,  0,  0,  0,  /* 8x */
   0,106,107,108,109,110,111,112,113,114,  0,  0,  0,  0,  0,  0,  /* 9x */
   0,  0,115,116,117,118,119,120,121,122,  0,  0,  0,  0,  0,  0,  /* Ax */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* Bx */
   0, 97, 98, 99,100,101,102,103,104,105,  0,  0,  0,  0,  0,  0,  /* Cx */
   0,106,107,108,109,110,111,112,113,114,  0,  0,  0,  0,  0,  0,  /* Dx */
   0,  0,115,116,117,118,119,120,121,122,  0,  0,  0,  0,  0,  0,  /* Ex */
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  /* Fx */
};
#endif

/*
** The sqlite3KeywordCode function looks up an identifier to determine if
** it is a keyword.  If it is a keyword, the token code of that keyword is 
** returned.  If the input is not a keyword, TK_ID is returned.
**
** The implementation of this routine was generated by a program,
** mkkeywordhash.c, located in the tool subdirectory of the distribution.
** The output of the mkkeywordhash.c program is written into a file
** named keywordhash.h and then included into this source file by
** the #include below.
*/
/************** Include keywordhash.h in the middle of tokenize.c ************/
/************** Begin file keywordhash.h *************************************/
/***** This file contains automatically generated code ******
**
** The code in this file has been automatically generated by
**
**   sqlite/tool/mkkeywordhash.c
**
** The code in this file implements a function that determines whether
** or not a given identifier is really an SQL keyword.  The same thing
** might be implemented more directly using a hand-written hash table.
** But by using this automatically generated code, the size of the code
** is substantially reduced.  This is important for embedded applications
** on platforms with limited memory.
*/
/* Hash score: 221 */
/* zKWText[] encodes 967 bytes of keyword text in 638 bytes */
/*   REINDEXEDESCAPEACHECKEYBEFOREIGNOREGEXPLAINSTEADDATABASELECT       */
/*   ABLEFTHENDEFERRABLELSEXCLUDELETEMPORARYISNULLSAVEPOINTERSECT       */
/*   IESNOTNULLIKEXCEPTRANSACTIONATURALTERAISEXCLUSIVEXISTS             */
/*   CONSTRAINTOFFSETRIGGEREFERENCESUNIQUERYWITHOUTERELEASEATTACH       */
/*   AVINGLOBEGINNERANGEBETWEENOTHINGROUPSCASCADETACHCASECOLLATE        */
/*   CREATECURRENT_DATEIMMEDIATEJOINSERTMATCHPLANALYZEPRAGMABORT        */
/*   UPDATEVALUESVIRTUALASTWHENWHERECURSIVEAFTERENAMEANDEFAULT          */
/*   AUTOINCREMENTCASTCOLUMNCOMMITCONFLICTCROSSCURRENT_TIMESTAMP        */
/*   ARTITIONDEFERREDISTINCTDROPRECEDINGFAILIMITFILTEREPLACEFIRST       */
/*   FOLLOWINGFROMFULLIFORDERESTRICTOTHERSOVERIGHTROLLBACKROWS          */
/*   UNBOUNDEDUNIONUSINGVACUUMVIEWINDOWBYINITIALLYPRIMARY               */
static const char zKWText[637] = {
  'R','E','I','N','D','E','X','E','D','E','S','C','A','P','E','A','C','H',
  'E','C','K','E','Y','B','E','F','O','R','E','I','G','N','O','R','E','G',
  'E','X','P','L','A','I','N','S','T','E','A','D','D','A','T','A','B','A',
  'S','E','L','E','C','T','A','B','L','E','F','T','H','E','N','D','E','F',
  'E','R','R','A','B','L','E','L','S','E','X','C','L','U','D','E','L','E',
  'T','E','M','P','O','R','A','R','Y','I','S','N','U','L','L','S','A','V',
  'E','P','O','I','N','T','E','R','S','E','C','T','I','E','S','N','O','T',
  'N','U','L','L','I','K','E','X','C','E','P','T','R','A','N','S','A','C',
  'T','I','O','N','A','T','U','R','A','L','T','E','R','A','I','S','E','X',
  'C','L','U','S','I','V','E','X','I','S','T','S','C','O','N','S','T','R',
  'A','I','N','T','O','F','F','S','E','T','R','I','G','G','E','R','E','F',
  'E','R','E','N','C','E','S','U','N','I','Q','U','E','R','Y','W','I','T',
  'H','O','U','T','E','R','E','L','E','A','S','E','A','T','T','A','C','H',
  'A','V','I','N','G','L','O','B','E','G','I','N','N','E','R','A','N','G',
  'E','B','E','T','W','E','E','N','O','T','H','I','N','G','R','O','U','P',
  'S','C','A','S','C','A','D','E','T','A','C','H','C','A','S','E','C','O',
  'L','L','A','T','E','C','R','E','A','T','E','C','U','R','R','E','N','T',
  '_','D','A','T','E','I','M','M','E','D','I','A','T','E','J','O','I','N',
  'S','E','R','T','M','A','T','C','H','P','L','A','N','A','L','Y','Z','E',
  'P','R','A','G','M','A','B','O','R','T','U','P','D','A','T','E','V','A',
  'L','U','E','S','V','I','R','T','U','A','L','A','S','T','W','H','E','N',
  'W','H','E','R','E','C','U','R','S','I','V','E','A','F','T','E','R','E',
  'N','A','M','E','A','N','D','E','F','A','U','L','T','A','U','T','O','I',
  'N','C','R','E','M','E','N','T','C','A','S','T','C','O','L','U','M','N',
  'C','O','M','M','I','T','C','O','N','F','L','I','C','T','C','R','O','S',
  'S','C','U','R','R','E','N','T','_','T','I','M','E','S','T','A','M','P',
  'A','R','T','I','T','I','O','N','D','E','F','E','R','R','E','D','I','S',
  'T','I','N','C','T','D','R','O','P','R','E','C','E','D','I','N','G','F',
  'A','I','L','I','M','I','T','F','I','L','T','E','R','E','P','L','A','C',
  'E','F','I','R','S','T','F','O','L','L','O','W','I','N','G','F','R','O',
  'M','F','U','L','L','I','F','O','R','D','E','R','E','S','T','R','I','C',
  'T','O','T','H','E','R','S','O','V','E','R','I','G','H','T','R','O','L',
  'L','B','A','C','K','R','O','W','S','U','N','B','O','U','N','D','E','D',
  'U','N','I','O','N','U','S','I','N','G','V','A','C','U','U','M','V','I',
  'E','W','I','N','D','O','W','B','Y','I','N','I','T','I','A','L','L','Y',
  'P','R','I','M','A','R','Y',
};
/* aKWHash[i] is the hash value for the i-th keyword */
static const unsigned char aKWHash[127] = {
    82, 113, 130,  80, 110,  29,   0,   0,  89,   0,  83,  70,   0,
    53,  35,  84,  15,   0, 129,  92,  64, 124, 131,  19,   0,   0,
   136,   0, 134, 126,   0,  22, 100,   0,   9,   0,   0, 121,  78,
     0,  76,   6,   0,  58,  97, 143,   0, 132, 108,   0,   0,  48,
     0, 111,  24,   0,  17,   0, 137,  63,  23,  26,   5,  65, 138,
   103, 120,   0, 142, 114,  69, 141,  66, 118,  72,   0,  98,   0,
   107,  41,   0, 106,   0,   0,   0, 102,  99, 104, 109, 123,  14,
    50, 122,   0,  87,   0, 139, 119, 140,  68, 127, 135,  86,  81,
    37,  91, 117,   0,   0, 101,  51, 128, 125,   0, 133,   0,   0,
    44,   0,  93,  67,  39,   0,  20,  45, 115,  88,
};
/* aKWNext[] forms the hash collision chain.  If aKWHash[i]==0
** then the i-th keyword has no more hash collisions.  Otherwise,
** the next keyword with the same hash is aKWHash[i]-1. */
static const unsigned char aKWNext[143] = {
     0,   0,   0,   0,   4,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   2,   0,   0,   0,   0,   0,   0,  13,   0,   0,   0,   0,
     0,   0,   0,  21,   0,   0,   0,   0,  12,   0,   0,   0,   0,
     0,   0,   0,   7,   0,  36,   0,   0,  28,   0,   0,   0,  31,
     0,   0,   0,  40,   0,   0,   0,   0,   0,  60,   0,  54,   0,
     0,  38,  47,   0,   0,   0,   3,   0,   0,  74,   1,  73,   0,
     0,   0,  52,   0,   0,   0,   0,   0,   0,  57,  59,  56,  30,
     0,   0,   0,  46,   0,  16,  49,  10,   0,   0,   0,   0,   0,
     0,   0,  11,  79,  95,   0,   0,   8,   0, 112,   0, 105,   0,
    43,  62,   0,  77,   0, 116,   0,  61,   0,   0,  94,  42,  55,
     0,  75,  34,  90,  32,  33,  27,  25,  18,  96,   0,  71,  85,
};
/* aKWLen[i] is the length (in bytes) of the i-th keyword */
static const unsigned char aKWLen[143] = {
     7,   7,   5,   4,   6,   4,   5,   3,   6,   7,   3,   6,   6,
     7,   7,   3,   8,   2,   6,   5,   4,   4,   3,  10,   4,   7,
     6,   9,   4,   2,   6,   5,   9,   9,   4,   7,   3,   2,   4,
     4,   6,  11,   6,   2,   7,   5,   5,   9,   6,  10,   4,   6,
     2,   3,   7,  10,   6,   5,   7,   4,   5,   7,   6,   6,   4,
     5,   5,   5,   7,   7,   6,   5,   7,   3,   6,   4,   7,   6,
    12,   9,   4,   6,   5,   4,   7,   6,   5,   6,   6,   7,   4,
     4,   5,   9,   5,   6,   3,   7,  13,   2,   2,   4,   6,   6,
     8,   5,  17,  12,   7,   9,   8,   8,   2,   4,   9,   4,   5,
     6,   7,   5,   9,   4,   4,   2,   5,   8,   6,   4,   5,   8,
     4,   3,   9,   5,   5,   6,   4,   6,   2,   2,   9,   3,   7,
};
/* aKWOffset[i] is the index into zKWText[] of the start of
** the text for the i-th keyword. */
static const unsigned short int aKWOffset[143] = {
     0,   2,   2,   8,   9,  14,  16,  20,  23,  25,  25,  29,  33,
    36,  41,  46,  48,  53,  54,  59,  62,  65,  67,  69,  78,  81,
    86,  90,  90,  94,  99, 101, 105, 111, 119, 123, 123, 123, 126,
   129, 132, 137, 142, 146, 147, 152, 156, 160, 168, 174, 181, 184,
   184, 187, 189, 195, 205, 208, 213, 213, 217, 221, 228, 233, 238,
   241, 244, 248, 253, 259, 265, 265, 271, 272, 276, 282, 286, 293,
   299, 311, 320, 322, 328, 333, 335, 342, 347, 352, 358, 364, 370,
   374, 378, 381, 390, 394, 400, 402, 409, 411, 413, 422, 426, 432,
   438, 446, 451, 451, 451, 467, 476, 483, 484, 491, 494, 503, 506,
   511, 516, 523, 528, 537, 541, 545, 547, 551, 559, 565, 568, 573,
   581, 581, 585, 594, 599, 604, 610, 613, 616, 619, 621, 626, 630,
};
/* aKWCode[i] is the parser symbol code for the i-th keyword */
static const unsigned char aKWCode[143] = {
  TK_REINDEX,    TK_INDEXED,    TK_INDEX,      TK_DESC,       TK_ESCAPE,     
  TK_EACH,       TK_CHECK,      TK_KEY,        TK_BEFORE,     TK_FOREIGN,    
  TK_FOR,        TK_IGNORE,     TK_LIKE_KW,    TK_EXPLAIN,    TK_INSTEAD,    
  TK_ADD,        TK_DATABASE,   TK_AS,         TK_SELECT,     TK_TABLE,      
  TK_JOIN_KW,    TK_THEN,       TK_END,        TK_DEFERRABLE, TK_ELSE,       
  TK_EXCLUDE,    TK_DELETE,     TK_TEMP,       TK_TEMP,       TK_OR,         
  TK_ISNULL,     TK_NULLS,      TK_SAVEPOINT,  TK_INTERSECT,  TK_TIES,       
  TK_NOTNULL,    TK_NOT,        TK_NO,         TK_NULL,       TK_LIKE_KW,    
  TK_EXCEPT,     TK_TRANSACTION,TK_ACTION,     TK_ON,         TK_JOIN_KW,    
  TK_ALTER,      TK_RAISE,      TK_EXCLUSIVE,  TK_EXISTS,     TK_CONSTRAINT, 
  TK_INTO,       TK_OFFSET,     TK_OF,         TK_SET,        TK_TRIGGER,    
  TK_REFERENCES, TK_UNIQUE,     TK_QUERY,      TK_WITHOUT,    TK_WITH,       
  TK_JOIN_KW,    TK_RELEASE,    TK_ATTACH,     TK_HAVING,     TK_LIKE_KW,    
  TK_BEGIN,      TK_JOIN_KW,    TK_RANGE,      TK_BETWEEN,    TK_NOTHING,    
  TK_GROUPS,     TK_GROUP,      TK_CASCADE,    TK_ASC,        TK_DETACH,     
  TK_CASE,       TK_COLLATE,    TK_CREATE,     TK_CTIME_KW,   TK_IMMEDIATE,  
  TK_JOIN,       TK_INSERT,     TK_MATCH,      TK_PLAN,       TK_ANALYZE,    
  TK_PRAGMA,     TK_ABORT,      TK_UPDATE,     TK_VALUES,     TK_VIRTUAL,    
  TK_LAST,       TK_WHEN,       TK_WHERE,      TK_RECURSIVE,  TK_AFTER,      
  TK_RENAME,     TK_AND,        TK_DEFAULT,    TK_AUTOINCR,   TK_TO,         
  TK_IN,         TK_CAST,       TK_COLUMNKW,   TK_COMMIT,     TK_CONFLICT,   
  TK_JOIN_KW,    TK_CTIME_KW,   TK_CTIME_KW,   TK_CURRENT,    TK_PARTITION,  
  TK_DEFERRED,   TK_DISTINCT,   TK_IS,         TK_DROP,       TK_PRECEDING,  
  TK_FAIL,       TK_LIMIT,      TK_FILTER,     TK_REPLACE,    TK_FIRST,      
  TK_FOLLOWING,  TK_FROM,       TK_JOIN_KW,    TK_IF,         TK_ORDER,      
  TK_RESTRICT,   TK_OTHERS,     TK_OVER,       TK_JOIN_KW,    TK_ROLLBACK,   
  TK_ROWS,       TK_ROW,        TK_UNBOUNDED,  TK_UNION,      TK_USING,      
  TK_VACUUM,     TK_VIEW,       TK_WINDOW,     TK_DO,         TK_BY,         
  TK_INITIALLY,  TK_ALL,        TK_PRIMARY,    
};
/* Check to see if z[0..n-1] is a keyword. If it is, write the
** parser symbol code for that keyword into *pType.  Always
** return the integer n (the length of the token). */
static int keywordCode(const char *z, int n, int *pType){
  int i, j;
  const char *zKW;
  if( n>=2 ){
    i = ((charMap(z[0])*4) ^ (charMap(z[n-1])*3) ^ n) % 127;
    for(i=((int)aKWHash[i])-1; i>=0; i=((int)aKWNext[i])-1){
      if( aKWLen[i]!=n ) continue;
      j = 0;
      zKW = &zKWText[aKWOffset[i]];
#ifdef SQLITE_ASCII
      while( j<n && (z[j]&~0x20)==zKW[j] ){ j++; }
#endif
#ifdef SQLITE_EBCDIC
      while( j<n && toupper(z[j])==zKW[j] ){ j++; }
#endif
      if( j<n ) continue;
      testcase( i==0 ); /* REINDEX */
      testcase( i==1 ); /* INDEXED */
      testcase( i==2 ); /* INDEX */
      testcase( i==3 ); /* DESC */
      testcase( i==4 ); /* ESCAPE */
      testcase( i==5 ); /* EACH */
      testcase( i==6 ); /* CHECK */
      testcase( i==7 ); /* KEY */
      testcase( i==8 ); /* BEFORE */
      testcase( i==9 ); /* FOREIGN */
      testcase( i==10 ); /* FOR */
      testcase( i==11 ); /* IGNORE */
      testcase( i==12 ); /* REGEXP */
      testcase( i==13 ); /* EXPLAIN */
      testcase( i==14 ); /* INSTEAD */
      testcase( i==15 ); /* ADD */
      testcase( i==16 ); /* DATABASE */
      testcase( i==17 ); /* AS */
      testcase( i==18 ); /* SELECT */
      testcase( i==19 ); /* TABLE */
      testcase( i==20 ); /* LEFT */
      testcase( i==21 ); /* THEN */
      testcase( i==22 ); /* END */
      testcase( i==23 ); /* DEFERRABLE */
      testcase( i==24 ); /* ELSE */
      testcase( i==25 ); /* EXCLUDE */
      testcase( i==26 ); /* DELETE */
      testcase( i==27 ); /* TEMPORARY */
      testcase( i==28 ); /* TEMP */
      testcase( i==29 ); /* OR */
      testcase( i==30 ); /* ISNULL */
      testcase( i==31 ); /* NULLS */
      testcase( i==32 ); /* SAVEPOINT */
      testcase( i==33 ); /* INTERSECT */
      testcase( i==34 ); /* TIES */
      testcase( i==35 ); /* NOTNULL */
      testcase( i==36 ); /* NOT */
      testcase( i==37 ); /* NO */
      testcase( i==38 ); /* NULL */
      testcase( i==39 ); /* LIKE */
      testcase( i==40 ); /* EXCEPT */
      testcase( i==41 ); /* TRANSACTION */
      testcase( i==42 ); /* ACTION */
      testcase( i==43 ); /* ON */
      testcase( i==44 ); /* NATURAL */
      testcase( i==45 ); /* ALTER */
      testcase( i==46 ); /* RAISE */
      testcase( i==47 ); /* EXCLUSIVE */
      testcase( i==48 ); /* EXISTS */
      testcase( i==49 ); /* CONSTRAINT */
      testcase( i==50 ); /* INTO */
      testcase( i==51 ); /* OFFSET */
      testcase( i==52 ); /* OF */
      testcase( i==53 ); /* SET */
      testcase( i==54 ); /* TRIGGER */
      testcase( i==55 ); /* REFERENCES */
      testcase( i==56 ); /* UNIQUE */
      testcase( i==57 ); /* QUERY */
      testcase( i==58 ); /* WITHOUT */
      testcase( i==59 ); /* WITH */
      testcase( i==60 ); /* OUTER */
      testcase( i==61 ); /* RELEASE */
      testcase( i==62 ); /* ATTACH */
      testcase( i==63 ); /* HAVING */
      testcase( i==64 ); /* GLOB */
      testcase( i==65 ); /* BEGIN */
      testcase( i==66 ); /* INNER */
      testcase( i==67 ); /* RANGE */
      testcase( i==68 ); /* BETWEEN */
      testcase( i==69 ); /* NOTHING */
      testcase( i==70 ); /* GROUPS */
      testcase( i==71 ); /* GROUP */
      testcase( i==72 ); /* CASCADE */
      testcase( i==73 ); /* ASC */
      testcase( i==74 ); /* DETACH */
      testcase( i==75 ); /* CASE */
      testcase( i==76 ); /* COLLATE */
      testcase( i==77 ); /* CREATE */
      testcase( i==78 ); /* CURRENT_DATE */
      testcase( i==79 ); /* IMMEDIATE */
      testcase( i==80 ); /* JOIN */
      testcase( i==81 ); /* INSERT */
      testcase( i==82 ); /* MATCH */
      testcase( i==83 ); /* PLAN */
      testcase( i==84 ); /* ANALYZE */
      testcase( i==85 ); /* PRAGMA */
      testcase( i==86 ); /* ABORT */
      testcase( i==87 ); /* UPDATE */
      testcase( i==88 ); /* VALUES */
      testcase( i==89 ); /* VIRTUAL */
      testcase( i==90 ); /* LAST */
      testcase( i==91 ); /* WHEN */
      testcase( i==92 ); /* WHERE */
      testcase( i==93 ); /* RECURSIVE */
      testcase( i==94 ); /* AFTER */
      testcase( i==95 ); /* RENAME */
      testcase( i==96 ); /* AND */
      testcase( i==97 ); /* DEFAULT */
      testcase( i==98 ); /* AUTOINCREMENT */
      testcase( i==99 ); /* TO */
      testcase( i==100 ); /* IN */
      testcase( i==101 ); /* CAST */
      testcase( i==102 ); /* COLUMN */
      testcase( i==103 ); /* COMMIT */
      testcase( i==104 ); /* CONFLICT */
      testcase( i==105 ); /* CROSS */
      testcase( i==106 ); /* CURRENT_TIMESTAMP */
      testcase( i==107 ); /* CURRENT_TIME */
      testcase( i==108 ); /* CURRENT */
      testcase( i==109 ); /* PARTITION */
      testcase( i==110 ); /* DEFERRED */
      testcase( i==111 ); /* DISTINCT */
      testcase( i==112 ); /* IS */
      testcase( i==113 ); /* DROP */
      testcase( i==114 ); /* PRECEDING */
      testcase( i==115 ); /* FAIL */
      testcase( i==116 ); /* LIMIT */
      testcase( i==117 ); /* FILTER */
      testcase( i==118 ); /* REPLACE */
      testcase( i==119 ); /* FIRST */
      testcase( i==120 ); /* FOLLOWING */
      testcase( i==121 ); /* FROM */
      testcase( i==122 ); /* FULL */
      testcase( i==123 ); /* IF */
      testcase( i==124 ); /* ORDER */
      testcase( i==125 ); /* RESTRICT */
      testcase( i==126 ); /* OTHERS */
      testcase( i==127 ); /* OVER */
      testcase( i==128 ); /* RIGHT */
      testcase( i==129 ); /* ROLLBACK */
      testcase( i==130 ); /* ROWS */
      testcase( i==131 ); /* ROW */
      testcase( i==132 ); /* UNBOUNDED */
      testcase( i==133 ); /* UNION */
      testcase( i==134 ); /* USING */
      testcase( i==135 ); /* VACUUM */
      testcase( i==136 ); /* VIEW */
      testcase( i==137 ); /* WINDOW */
      testcase( i==138 ); /* DO */
      testcase( i==139 ); /* BY */
      testcase( i==140 ); /* INITIALLY */
      testcase( i==141 ); /* ALL */
      testcase( i==142 ); /* PRIMARY */
      *pType = aKWCode[i];
      break;
    }
  }
  return n;
}
SQLITE_PRIVATE int sqlite3KeywordCode(const unsigned char *z, int n){
  int id = TK_ID;
  keywordCode((char*)z, n, &id);
  return id;
}
#define SQLITE_N_KEYWORD 143
SQLITE_API int sqlite3_keyword_name(int i,const char **pzName,int *pnName){
  if( i<0 || i>=SQLITE_N_KEYWORD ) return SQLITE_ERROR;
  *pzName = zKWText + aKWOffset[i];
  *pnName = aKWLen[i];
  return SQLITE_OK;
}
SQLITE_API int sqlite3_keyword_count(void){ return SQLITE_N_KEYWORD; }
SQLITE_API int sqlite3_keyword_check(const char *zName, int nName){
  return TK_ID!=sqlite3KeywordCode((const u8*)zName, nName);
}

/************** End of keywordhash.h *****************************************/
/************** Continuing where we left off in tokenize.c *******************/


/*
** If X is a character that can be used in an identifier then
** IdChar(X) will be true.  Otherwise it is false.
**
** For ASCII, any character with the high-order bit set is
** allowed in an identifier.  For 7-bit characters, 
** sqlite3IsIdChar[X] must be 1.
**
** For EBCDIC, the rules are more complex but have the same
** end result.
**
** Ticket #1066.  the SQL standard does not allow '$' in the
** middle of identifiers.  But many SQL implementations do. 
** SQLite will allow '$' in identifiers for compatibility.
** But the feature is undocumented.
*/
#ifdef SQLITE_ASCII
#define IdChar(C)  ((sqlite3CtypeMap[(unsigned char)C]&0x46)!=0)
#endif
#ifdef SQLITE_EBCDIC
SQLITE_PRIVATE const char sqlite3IsEbcdicIdChar[] = {
/* x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF */
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  /* 4x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0,  /* 5x */
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0,  /* 6x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,  /* 7x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0,  /* 8x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0,  /* 9x */
    1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0,  /* Ax */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* Bx */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,  /* Cx */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,  /* Dx */
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,  /* Ex */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,  /* Fx */
};
#define IdChar(C)  (((c=C)>=0x42 && sqlite3IsEbcdicIdChar[c-0x40]))
#endif

/* Make the IdChar function accessible from ctime.c and alter.c */
SQLITE_PRIVATE int sqlite3IsIdChar(u8 c){ return IdChar(c); }

#ifndef SQLITE_OMIT_WINDOWFUNC
/*
** Return the id of the next token in string (*pz). Before returning, set
** (*pz) to point to the byte following the parsed token.
*/
static int getToken(const unsigned char **pz){
  const unsigned char *z = *pz;
  int t;                          /* Token type to return */
  do {
    z += sqlite3GetToken(z, &t);
  }while( t==TK_SPACE );
  if( t==TK_ID 
   || t==TK_STRING 
   || t==TK_JOIN_KW 
   || t==TK_WINDOW 
   || t==TK_OVER 
   || sqlite3ParserFallback(t)==TK_ID 
  ){
    t = TK_ID;
  }
  *pz = z;
  return t;
}

/*
** The following three functions are called immediately after the tokenizer
** reads the keywords WINDOW, OVER and FILTER, respectively, to determine
** whether the token should be treated as a keyword or an SQL identifier.
** This cannot be handled by the usual lemon %fallback method, due to
** the ambiguity in some constructions. e.g.
**
**   SELECT sum(x) OVER ...
**
** In the above, "OVER" might be a keyword, or it might be an alias for the 
** sum(x) expression. If a "%fallback ID OVER" directive were added to 
** grammar, then SQLite would always treat "OVER" as an alias, making it
** impossible to call a window-function without a FILTER clause.
**
** WINDOW is treated as a keyword if:
**
**   * the following token is an identifier, or a keyword that can fallback
**     to being an identifier, and
**   * the token after than one is TK_AS.
**
** OVER is a keyword if:
**
**   * the previous token was TK_RP, and
**   * the next token is either TK_LP or an identifier.
**
** FILTER is a keyword if:
**
**   * the previous token was TK_RP, and
**   * the next token is TK_LP.
*/
static int analyzeWindowKeyword(const unsigned char *z){
  int t;
  t = getToken(&z);
  if( t!=TK_ID ) return TK_ID;
  t = getToken(&z);
  if( t!=TK_AS ) return TK_ID;
  return TK_WINDOW;
}
static int analyzeOverKeyword(const unsigned char *z, int lastToken){
  if( lastToken==TK_RP ){
    int t = getToken(&z);
    if( t==TK_LP || t==TK_ID ) return TK_OVER;
  }
  return TK_ID;
}
static int analyzeFilterKeyword(const unsigned char *z, int lastToken){
  if( lastToken==TK_RP && getToken(&z)==TK_LP ){
    return TK_FILTER;
  }
  return TK_ID;
}
#endif /* SQLITE_OMIT_WINDOWFUNC */

/*
** Return the length (in bytes) of the token that begins at z[0]. 
** Store the token type in *tokenType before returning.
*/
SQLITE_PRIVATE int sqlite3GetToken(const unsigned char *z, int *tokenType){
  int i, c;
  switch( aiClass[*z] ){  /* Switch on the character-class of the first byte
                          ** of the token. See the comment on the CC_ defines
                          ** above. */
    case CC_SPACE: {
      testcase( z[0]==' ' );
      testcase( z[0]=='\t' );
      testcase( z[0]=='\n' );
      testcase( z[0]=='\f' );
      testcase( z[0]=='\r' );
      for(i=1; sqlite3Isspace(z[i]); i++){}
      *tokenType = TK_SPACE;
      return i;
    }
    case CC_MINUS: {
      if( z[1]=='-' ){
        for(i=2; (c=z[i])!=0 && c!='\n'; i++){}
        *tokenType = TK_SPACE;   /* IMP: R-22934-25134 */
        return i;
      }
      *tokenType = TK_MINUS;
      return 1;
    }
    case CC_LP: {
      *tokenType = TK_LP;
      return 1;
    }
    case CC_RP: {
      *tokenType = TK_RP;
      return 1;
    }
    case CC_SEMI: {
      *tokenType = TK_SEMI;
      return 1;
    }
    case CC_PLUS: {
      *tokenType = TK_PLUS;
      return 1;
    }
    case CC_STAR: {
      *tokenType = TK_STAR;
      return 1;
    }
    case CC_SLASH: {
      if( z[1]!='*' || z[2]==0 ){
        *tokenType = TK_SLASH;
        return 1;
      }
      for(i=3, c=z[2]; (c!='*' || z[i]!='/') && (c=z[i])!=0; i++){}
      if( c ) i++;
      *tokenType = TK_SPACE;   /* IMP: R-22934-25134 */
      return i;
    }
    case CC_PERCENT: {
      *tokenType = TK_REM;
      return 1;
    }
    case CC_EQ: {
      *tokenType = TK_EQ;
      return 1 + (z[1]=='=');
    }
    case CC_LT: {
      if( (c=z[1])=='=' ){
        *tokenType = TK_LE;
        return 2;
      }else if( c=='>' ){
        *tokenType = TK_NE;
        return 2;
      }else if( c=='<' ){
        *tokenType = TK_LSHIFT;
        return 2;
      }else{
        *tokenType = TK_LT;
        return 1;
      }
    }
    case CC_GT: {
      if( (c=z[1])=='=' ){
        *tokenType = TK_GE;
        return 2;
      }else if( c=='>' ){
        *tokenType = TK_RSHIFT;
        return 2;
      }else{
        *tokenType = TK_GT;
        return 1;
      }
    }
    case CC_BANG: {
      if( z[1]!='=' ){
        *tokenType = TK_ILLEGAL;
        return 1;
      }else{
        *tokenType = TK_NE;
        return 2;
      }
    }
    case CC_PIPE: {
      if( z[1]!='|' ){
        *tokenType = TK_BITOR;
        return 1;
      }else{
        *tokenType = TK_CONCAT;
        return 2;
      }
    }
    case CC_COMMA: {
      *tokenType = TK_COMMA;
      return 1;
    }
    case CC_AND: {
      *tokenType = TK_BITAND;
      return 1;
    }
    case CC_TILDA: {
      *tokenType = TK_BITNOT;
      return 1;
    }
    case CC_QUOTE: {
      int delim = z[0];
      testcase( delim=='`' );
      testcase( delim=='\'' );
      testcase( delim=='"' );
      for(i=1; (c=z[i])!=0; i++){
        if( c==delim ){
          if( z[i+1]==delim ){
            i++;
          }else{
            break;
          }
        }
      }
      if( c=='\'' ){
        *tokenType = TK_STRING;
        return i+1;
      }else if( c!=0 ){
        *tokenType = TK_ID;
        return i+1;
      }else{
        *tokenType = TK_ILLEGAL;
        return i;
      }
    }
    case CC_DOT: {
#ifndef SQLITE_OMIT_FLOATING_POINT
      if( !sqlite3Isdigit(z[1]) )
#endif
      {
        *tokenType = TK_DOT;
        return 1;
      }
      /* If the next character is a digit, this is a floating point
      ** number that begins with ".".  Fall thru into the next case */
    }
    case CC_DIGIT: {
      testcase( z[0]=='0' );  testcase( z[0]=='1' );  testcase( z[0]=='2' );
      testcase( z[0]=='3' );  testcase( z[0]=='4' );  testcase( z[0]=='5' );
      testcase( z[0]=='6' );  testcase( z[0]=='7' );  testcase( z[0]=='8' );
      testcase( z[0]=='9' );
      *tokenType = TK_INTEGER;
#ifndef SQLITE_OMIT_HEX_INTEGER
      if( z[0]=='0' && (z[1]=='x' || z[1]=='X') && sqlite3Isxdigit(z[2]) ){
        for(i=3; sqlite3Isxdigit(z[i]); i++){}
        return i;
      }
#endif
      for(i=0; sqlite3Isdigit(z[i]); i++){}
#ifndef SQLITE_OMIT_FLOATING_POINT
      if( z[i]=='.' ){
        i++;
        while( sqlite3Isdigit(z[i]) ){ i++; }
        *tokenType = TK_FLOAT;
      }
      if( (z[i]=='e' || z[i]=='E') &&
           ( sqlite3Isdigit(z[i+1]) 
            || ((z[i+1]=='+' || z[i+1]=='-') && sqlite3Isdigit(z[i+2]))
           )
      ){
        i += 2;
        while( sqlite3Isdigit(z[i]) ){ i++; }
        *tokenType = TK_FLOAT;
      }
#endif
      while( IdChar(z[i]) ){
        *tokenType = TK_ILLEGAL;
        i++;
      }
      return i;
    }
    case CC_QUOTE2: {
      for(i=1, c=z[0]; c!=']' && (c=z[i])!=0; i++){}
      *tokenType = c==']' ? TK_ID : TK_ILLEGAL;
      return i;
    }
    case CC_VARNUM: {
      *tokenType = TK_VARIABLE;
      for(i=1; sqlite3Isdigit(z[i]); i++){}
      return i;
    }
    case CC_DOLLAR:
    case CC_VARALPHA: {
      int n = 0;
      testcase( z[0]=='$' );  testcase( z[0]=='@' );
      testcase( z[0]==':' );  testcase( z[0]=='#' );
      *tokenType = TK_VARIABLE;
      for(i=1; (c=z[i])!=0; i++){
        if( IdChar(c) ){
          n++;
#ifndef SQLITE_OMIT_TCL_VARIABLE
        }else if( c=='(' && n>0 ){
          do{
            i++;
          }while( (c=z[i])!=0 && !sqlite3Isspace(c) && c!=')' );
          if( c==')' ){
            i++;
          }else{
            *tokenType = TK_ILLEGAL;
          }
          break;
        }else if( c==':' && z[i+1]==':' ){
          i++;
#endif
        }else{
          break;
        }
      }
      if( n==0 ) *tokenType = TK_ILLEGAL;
      return i;
    }
    case CC_KYWD: {
      for(i=1; aiClass[z[i]]<=CC_KYWD; i++){}
      if( IdChar(z[i]) ){
        /* This token started out using characters that can appear in keywords,
        ** but z[i] is a character not allowed within keywords, so this must
        ** be an identifier instead */
        i++;
        break;
      }
      *tokenType = TK_ID;
      return keywordCode((char*)z, i, tokenType);
    }
    case CC_X: {
#ifndef SQLITE_OMIT_BLOB_LITERAL
      testcase( z[0]=='x' ); testcase( z[0]=='X' );
      if( z[1]=='\'' ){
        *tokenType = TK_BLOB;
        for(i=2; sqlite3Isxdigit(z[i]); i++){}
        if( z[i]!='\'' || i%2 ){
          *tokenType = TK_ILLEGAL;
          while( z[i] && z[i]!='\'' ){ i++; }
        }
        if( z[i] ) i++;
        return i;
      }
#endif
      /* If it is not a BLOB literal, then it must be an ID, since no
      ** SQL keywords start with the letter 'x'.  Fall through */
    }
    case CC_ID: {
      i = 1;
      break;
    }
    case CC_NUL: {
      *tokenType = TK_ILLEGAL;
      return 0;
    }
    default: {
      *tokenType = TK_ILLEGAL;
      return 1;
    }
  }
  while( IdChar(z[i]) ){ i++; }
  *tokenType = TK_ID;
  return i;
}

/*
** Run the parser on the given SQL string.  The parser structure is
** passed in.  An SQLITE_ status code is returned.  If an error occurs
** then an and attempt is made to write an error message into 
** memory obtained from sqlite3_malloc() and to make *pzErrMsg point to that
** error message.
*/
SQLITE_PRIVATE int sqlite3RunParser(Parse *pParse, const char *zSql, char **pzErrMsg){
  int nErr = 0;                   /* Number of errors encountered */
  void *pEngine;                  /* The LEMON-generated LALR(1) parser */
  int n = 0;                      /* Length of the next token token */
  int tokenType;                  /* type of the next token */
  int lastTokenParsed = -1;       /* type of the previous token */
  sqlite3 *db = pParse->db;       /* The database connection */
  int mxSqlLen;                   /* Max length of an SQL string */
#ifdef sqlite3Parser_ENGINEALWAYSONSTACK
  yyParser sEngine;    /* Space to hold the Lemon-generated Parser object */
#endif
  VVA_ONLY( u8 startedWithOom = db->mallocFailed );

  assert( zSql!=0 );
  mxSqlLen = db->aLimit[SQLITE_LIMIT_SQL_LENGTH];
  if( db->nVdbeActive==0 ){
    db->u1.isInterrupted = 0;
  }
  pParse->rc = SQLITE_OK;
  pParse->zTail = zSql;
  assert( pzErrMsg!=0 );
#ifdef SQLITE_DEBUG
  if( db->flags & SQLITE_ParserTrace ){
    printf("parser: [[[%s]]]\n", zSql);
    sqlite3ParserTrace(stdout, "parser: ");
  }else{
    sqlite3ParserTrace(0, 0);
  }
#endif
#ifdef sqlite3Parser_ENGINEALWAYSONSTACK
  pEngine = &sEngine;
  sqlite3ParserInit(pEngine, pParse);
#else
  pEngine = sqlite3ParserAlloc(sqlite3Malloc, pParse);
  if( pEngine==0 ){
    sqlite3OomFault(db);
    return SQLITE_NOMEM_BKPT;
  }
#endif
  assert( pParse->pNewTable==0 );
  assert( pParse->pNewTrigger==0 );
  assert( pParse->nVar==0 );
  assert( pParse->pVList==0 );
  pParse->pParentParse = db->pParse;
  db->pParse = pParse;
  while( 1 ){
    n = sqlite3GetToken((u8*)zSql, &tokenType);
    mxSqlLen -= n;
    if( mxSqlLen<0 ){
      pParse->rc = SQLITE_TOOBIG;
      break;
    }
#ifndef SQLITE_OMIT_WINDOWFUNC
    if( tokenType>=TK_WINDOW ){
      assert( tokenType==TK_SPACE || tokenType==TK_OVER || tokenType==TK_FILTER
           || tokenType==TK_ILLEGAL || tokenType==TK_WINDOW 
      );
#else
    if( tokenType>=TK_SPACE ){
      assert( tokenType==TK_SPACE || tokenType==TK_ILLEGAL );
#endif /* SQLITE_OMIT_WINDOWFUNC */
      if( db->u1.isInterrupted ){
        pParse->rc = SQLITE_INTERRUPT;
        break;
      }
      if( tokenType==TK_SPACE ){
        zSql += n;
        continue;
      }
      if( zSql[0]==0 ){
        /* Upon reaching the end of input, call the parser two more times
        ** with tokens TK_SEMI and 0, in that order. */
        if( lastTokenParsed==TK_SEMI ){
          tokenType = 0;
        }else if( lastTokenParsed==0 ){
          break;
        }else{
          tokenType = TK_SEMI;
        }
        n = 0;
#ifndef SQLITE_OMIT_WINDOWFUNC
      }else if( tokenType==TK_WINDOW ){
        assert( n==6 );
        tokenType = analyzeWindowKeyword((const u8*)&zSql[6]);
      }else if( tokenType==TK_OVER ){
        assert( n==4 );
        tokenType = analyzeOverKeyword((const u8*)&zSql[4], lastTokenParsed);
      }else if( tokenType==TK_FILTER ){
        assert( n==6 );
        tokenType = analyzeFilterKeyword((const u8*)&zSql[6], lastTokenParsed);
#endif /* SQLITE_OMIT_WINDOWFUNC */
      }else{
        sqlite3ErrorMsg(pParse, "unrecognized token: \"%.*s\"", n, zSql);
        break;
      }
    }
    pParse->sLastToken.z = zSql;
    pParse->sLastToken.n = n;
    sqlite3Parser(pEngine, tokenType, pParse->sLastToken);
    lastTokenParsed = tokenType;
    zSql += n;
    assert( db->mallocFailed==0 || pParse->rc!=SQLITE_OK || startedWithOom );
    if( pParse->rc!=SQLITE_OK ) break;
  }
  assert( nErr==0 );
#ifdef YYTRACKMAXSTACKDEPTH
  sqlite3_mutex_enter(sqlite3MallocMutex());
  sqlite3StatusHighwater(SQLITE_STATUS_PARSER_STACK,
      sqlite3ParserStackPeak(pEngine)
  );
  sqlite3_mutex_leave(sqlite3MallocMutex());
#endif /* YYDEBUG */
#ifdef sqlite3Parser_ENGINEALWAYSONSTACK
  sqlite3ParserFinalize(pEngine);
#else
  sqlite3ParserFree(pEngine, sqlite3_free);
#endif
  if( db->mallocFailed ){
    pParse->rc = SQLITE_NOMEM_BKPT;
  }
  if( pParse->rc!=SQLITE_OK && pParse->rc!=SQLITE_DONE && pParse->zErrMsg==0 ){
    pParse->zErrMsg = sqlite3MPrintf(db, "%s", sqlite3ErrStr(pParse->rc));
  }
  assert( pzErrMsg!=0 );
  if( pParse->zErrMsg ){
    *pzErrMsg = pParse->zErrMsg;
    sqlite3_log(pParse->rc, "%s in \"%s\"", 
                *pzErrMsg, pParse->zTail);
    pParse->zErrMsg = 0;
    nErr++;
  }
  pParse->zTail = zSql;
  if( pParse->pVdbe && pParse->nErr>0 && pParse->nested==0 ){
    sqlite3VdbeDelete(pParse->pVdbe);
    pParse->pVdbe = 0;
  }
#ifndef SQLITE_OMIT_SHARED_CACHE
  if( pParse->nested==0 ){
    sqlite3DbFree(db, pParse->aTableLock);
    pParse->aTableLock = 0;
    pParse->nTableLock = 0;
  }
#endif
#ifndef SQLITE_OMIT_VIRTUALTABLE
  sqlite3_free(pParse->apVtabLock);
#endif

  if( !IN_SPECIAL_PARSE ){
    /* If the pParse->declareVtab flag is set, do not delete any table 
    ** structure built up in pParse->pNewTable. The calling code (see vtab.c)
    ** will take responsibility for freeing the Table structure.
    */
    sqlite3DeleteTable(db, pParse->pNewTable);
  }
  if( !IN_RENAME_OBJECT ){
    sqlite3DeleteTrigger(db, pParse->pNewTrigger);
  }

  if( pParse->pWithToFree ) sqlite3WithDelete(db, pParse->pWithToFree);
  sqlite3DbFree(db, pParse->pVList);
  while( pParse->pAinc ){
    AutoincInfo *p = pParse->pAinc;
    pParse->pAinc = p->pNext;
    sqlite3DbFreeNN(db, p);
  }
  while( pParse->pZombieTab ){
    Table *p = pParse->pZombieTab;
    pParse->pZombieTab = p->pNextZombie;
    sqlite3DeleteTable(db, p);
  }
  db->pParse = pParse->pParentParse;
  pParse->pParentParse = 0;
  assert( nErr==0 || pParse->rc!=SQLITE_OK );
  return nErr;
}


#ifdef SQLITE_ENABLE_NORMALIZE
/*
** Insert a single space character into pStr if the current string
** ends with an identifier
*/
static void addSpaceSeparator(sqlite3_str *pStr){
  if( pStr->nChar && sqlite3IsIdChar(pStr->zText[pStr->nChar-1]) ){
    sqlite3_str_append(pStr, " ", 1);
  }
}

/*
** Compute a normalization of the SQL given by zSql[0..nSql-1].  Return
** the normalization in space obtained from sqlite3DbMalloc().  Or return
** NULL if anything goes wrong or if zSql is NULL.
*/
SQLITE_PRIVATE char *sqlite3Normalize(
  Vdbe *pVdbe,       /* VM being reprepared */
  const char *zSql   /* The original SQL string */
){
  sqlite3 *db;       /* The database connection */
  int i;             /* Next unread byte of zSql[] */
  int n;             /* length of current token */
  int tokenType;     /* type of current token */
  int prevType = 0;  /* Previous non-whitespace token */
  int nParen;        /* Number of nested levels of parentheses */
  int iStartIN;      /* Start of RHS of IN operator in z[] */
  int nParenAtIN;    /* Value of nParent at start of RHS of IN operator */
  int j;             /* Bytes of normalized SQL generated so far */
  sqlite3_str *pStr; /* The normalized SQL string under construction */

  db = sqlite3VdbeDb(pVdbe);
  tokenType = -1;
  nParen = iStartIN = nParenAtIN = 0;
  pStr = sqlite3_str_new(db);
  assert( pStr!=0 );  /* sqlite3_str_new() never returns NULL */
  for(i=0; zSql[i] && pStr->accError==0; i+=n){
    if( tokenType!=TK_SPACE ){
      prevType = tokenType;
    }
    n = sqlite3GetToken((unsigned char*)zSql+i, &tokenType);
    if( NEVER(n<=0) ) break;
    switch( tokenType ){
      case TK_SPACE: {
        break;
      }
      case TK_NULL: {
        if( prevType==TK_IS || prevType==TK_NOT ){
          sqlite3_str_append(pStr, " NULL", 5);
          break;
        }
        /* Fall through */
      }
      case TK_STRING:
      case TK_INTEGER:
      case TK_FLOAT:
      case TK_VARIABLE:
      case TK_BLOB: {
        sqlite3_str_append(pStr, "?", 1);
        break;
      }
      case TK_LP: {
        nParen++;
        if( prevType==TK_IN ){
          iStartIN = pStr->nChar;
          nParenAtIN = nParen;
        }
        sqlite3_str_append(pStr, "(", 1);
        break;
      }
      case TK_RP: {
        if( iStartIN>0 && nParen==nParenAtIN ){
          assert( pStr->nChar>=iStartIN );
          pStr->nChar = iStartIN+1;
          sqlite3_str_append(pStr, "?,?,?", 5);
          iStartIN = 0;
        }
        nParen--;
        sqlite3_str_append(pStr, ")", 1);
        break;
      }
      case TK_ID: {
        iStartIN = 0;
        j = pStr->nChar;
        if( sqlite3Isquote(zSql[i]) ){
          char *zId = sqlite3DbStrNDup(db, zSql+i, n);
          int nId;
          int eType = 0;
          if( zId==0 ) break;
          sqlite3Dequote(zId);
          if( zSql[i]=='"' && sqlite3VdbeUsesDoubleQuotedString(pVdbe, zId) ){
            sqlite3_str_append(pStr, "?", 1);
            sqlite3DbFree(db, zId);
            break;
          }
          nId = sqlite3Strlen30(zId);
          if( sqlite3GetToken((u8*)zId, &eType)==nId && eType==TK_ID ){
            addSpaceSeparator(pStr);
            sqlite3_str_append(pStr, zId, nId);
          }else{
            sqlite3_str_appendf(pStr, "\"%w\"", zId);
          }
          sqlite3DbFree(db, zId);
        }else{
          addSpaceSeparator(pStr);
          sqlite3_str_append(pStr, zSql+i, n);
        }
        while( j<pStr->nChar ){
          pStr->zText[j] = sqlite3Tolower(pStr->zText[j]);
          j++;
        }
        break;
      }
      case TK_SELECT: {
        iStartIN = 0;
        /* fall through */
      }
      default: {
        if( sqlite3IsIdChar(zSql[i]) ) addSpaceSeparator(pStr);
        j = pStr->nChar;
        sqlite3_str_append(pStr, zSql+i, n);
        while( j<pStr->nChar ){
          pStr->zText[j] = sqlite3Toupper(pStr->zText[j]);
          j++;
        }
        break;
      }
    }
  }
  if( tokenType!=TK_SEMI ) sqlite3_str_append(pStr, ";", 1);
  return sqlite3_str_finish(pStr);
}
#endif /* SQLITE_ENABLE_NORMALIZE */

/************** End of tokenize.c ********************************************/
/************** Begin file complete.c ****************************************/
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
** An tokenizer for SQL
**
** This file contains C code that implements the sqlite3_complete() API.
** This code used to be part of the tokenizer.c source file.  But by
** separating it out, the code will be automatically omitted from
** static links that do not use it.
*/
/* #include "sqliteInt.h" */
#ifndef SQLITE_OMIT_COMPLETE

/*
** This is defined in tokenize.c.  We just have to import the definition.
*/
#ifndef SQLITE_AMALGAMATION
#ifdef SQLITE_ASCII
#define IdChar(C)  ((sqlite3CtypeMap[(unsigned char)C]&0x46)!=0)
#endif
#ifdef SQLITE_EBCDIC
SQLITE_PRIVATE const char sqlite3IsEbcdicIdChar[];
#define IdChar(C)  (((c=C)>=0x42 && sqlite3IsEbcdicIdChar[c-0x40]))
#endif
#endif /* SQLITE_AMALGAMATION */


/*
** Token types used by the sqlite3_complete() routine.  See the header
** comments on that procedure for additional information.
*/
#define tkSEMI    0
#define tkWS      1
#define tkOTHER   2
#ifndef SQLITE_OMIT_TRIGGER
#define tkEXPLAIN 3
#define tkCREATE  4
#define tkTEMP    5
#define tkTRIGGER 6
#define tkEND     7
#endif

/*
** Return TRUE if the given SQL string ends in a semicolon.
**
** Special handling is require for CREATE TRIGGER statements.
** Whenever the CREATE TRIGGER keywords are seen, the statement
** must end with ";END;".
**
** This implementation uses a state machine with 8 states:
**
**   (0) INVALID   We have not yet seen a non-whitespace character.
**
**   (1) START     At the beginning or end of an SQL statement.  This routine
**                 returns 1 if it ends in the START state and 0 if it ends
**                 in any other state.
**
**   (2) NORMAL    We are in the middle of statement which ends with a single
**                 semicolon.
**
**   (3) EXPLAIN   The keyword EXPLAIN has been seen at the beginning of 
**                 a statement.
**
**   (4) CREATE    The keyword CREATE has been seen at the beginning of a
**                 statement, possibly preceded by EXPLAIN and/or followed by
**                 TEMP or TEMPORARY
**
**   (5) TRIGGER   We are in the middle of a trigger definition that must be
**                 ended by a semicolon, the keyword END, and another semicolon.
**
**   (6) SEMI      We've seen the first semicolon in the ";END;" that occurs at
**                 the end of a trigger definition.
**
**   (7) END       We've seen the ";END" of the ";END;" that occurs at the end
**                 of a trigger definition.
**
** Transitions between states above are determined by tokens extracted
** from the input.  The following tokens are significant:
**
**   (0) tkSEMI      A semicolon.
**   (1) tkWS        Whitespace.
**   (2) tkOTHER     Any other SQL token.
**   (3) tkEXPLAIN   The "explain" keyword.
**   (4) tkCREATE    The "create" keyword.
**   (5) tkTEMP      The "temp" or "temporary" keyword.
**   (6) tkTRIGGER   The "trigger" keyword.
**   (7) tkEND       The "end" keyword.
**
** Whitespace never causes a state transition and is always ignored.
** This means that a SQL string of all whitespace is invalid.
**
** If we compile with SQLITE_OMIT_TRIGGER, all of the computation needed
** to recognize the end of a trigger can be omitted.  All we have to do
** is look for a semicolon that is not part of an string or comment.
*/
SQLITE_API int sqlite3_complete(const char *zSql){
  u8 state = 0;   /* Current state, using numbers defined in header comment */
  u8 token;       /* Value of the next token */

#ifndef SQLITE_OMIT_TRIGGER
  /* A complex statement machine used to detect the end of a CREATE TRIGGER
  ** statement.  This is the normal case.
  */
  static const u8 trans[8][8] = {
                     /* Token:                                                */
     /* State:       **  SEMI  WS  OTHER  EXPLAIN  CREATE  TEMP  TRIGGER  END */
     /* 0 INVALID: */ {    1,  0,     2,       3,      4,    2,       2,   2, },
     /* 1   START: */ {    1,  1,     2,       3,      4,    2,       2,   2, },
     /* 2  NORMAL: */ {    1,  2,     2,       2,      2,    2,       2,   2, },
     /* 3 EXPLAIN: */ {    1,  3,     3,       2,      4,    2,       2,   2, },
     /* 4  CREATE: */ {    1,  4,     2,       2,      2,    4,       5,   2, },
     /* 5 TRIGGER: */ {    6,  5,     5,       5,      5,    5,       5,   5, },
     /* 6    SEMI: */ {    6,  6,     5,       5,      5,    5,       5,   7, },
     /* 7     END: */ {    1,  7,     5,       5,      5,    5,       5,   5, },
  };
#else
  /* If triggers are not supported by this compile then the statement machine
  ** used to detect the end of a statement is much simpler
  */
  static const u8 trans[3][3] = {
                     /* Token:           */
     /* State:       **  SEMI  WS  OTHER */
     /* 0 INVALID: */ {    1,  0,     2, },
     /* 1   START: */ {    1,  1,     2, },
     /* 2  NORMAL: */ {    1,  2,     2, },
  };
#endif /* SQLITE_OMIT_TRIGGER */

#ifdef SQLITE_ENABLE_API_ARMOR
  if( zSql==0 ){
    (void)SQLITE_MISUSE_BKPT;
    return 0;
  }
#endif

  while( *zSql ){
    switch( *zSql ){
      case ';': {  /* A semicolon */
        token = tkSEMI;
        break;
      }
      case ' ':
      case '\r':
      case '\t':
      case '\n':
      case '\f': {  /* White space is ignored */
        token = tkWS;
        break;
      }
      case '/': {   /* C-style comments */
        if( zSql[1]!='*' ){
          token = tkOTHER;
          break;
        }
        zSql += 2;
        while( zSql[0] && (zSql[0]!='*' || zSql[1]!='/') ){ zSql++; }
        if( zSql[0]==0 ) return 0;
        zSql++;
        token = tkWS;
        break;
      }
      case '-': {   /* SQL-style comments from "--" to end of line */
        if( zSql[1]!='-' ){
          token = tkOTHER;
          break;
        }
        while( *zSql && *zSql!='\n' ){ zSql++; }
        if( *zSql==0 ) return state==1;
        token = tkWS;
        break;
      }
      case '[': {   /* Microsoft-style identifiers in [...] */
        zSql++;
        while( *zSql && *zSql!=']' ){ zSql++; }
        if( *zSql==0 ) return 0;
        token = tkOTHER;
        break;
      }
      case '`':     /* Grave-accent quoted symbols used by MySQL */
      case '"':     /* single- and double-quoted strings */
      case '\'': {
        int c = *zSql;
        zSql++;
        while( *zSql && *zSql!=c ){ zSql++; }
        if( *zSql==0 ) return 0;
        token = tkOTHER;
        break;
      }
      default: {
#ifdef SQLITE_EBCDIC
        unsigned char c;
#endif
        if( IdChar((u8)*zSql) ){
          /* Keywords and unquoted identifiers */
          int nId;
          for(nId=1; IdChar(zSql[nId]); nId++){}
#ifdef SQLITE_OMIT_TRIGGER
          token = tkOTHER;
#else
          switch( *zSql ){
            case 'c': case 'C': {
              if( nId==6 && sqlite3StrNICmp(zSql, "create", 6)==0 ){
                token = tkCREATE;
              }else{
                token = tkOTHER;
              }
              break;
            }
            case 't': case 'T': {
              if( nId==7 && sqlite3StrNICmp(zSql, "trigger", 7)==0 ){
                token = tkTRIGGER;
              }else if( nId==4 && sqlite3StrNICmp(zSql, "temp", 4)==0 ){
                token = tkTEMP;
              }else if( nId==9 && sqlite3StrNICmp(zSql, "temporary", 9)==0 ){
                token = tkTEMP;
              }else{
                token = tkOTHER;
              }
              break;
            }
            case 'e':  case 'E': {
              if( nId==3 && sqlite3StrNICmp(zSql, "end", 3)==0 ){
                token = tkEND;
              }else
#ifndef SQLITE_OMIT_EXPLAIN
              if( nId==7 && sqlite3StrNICmp(zSql, "explain", 7)==0 ){
                token = tkEXPLAIN;
              }else
#endif
              {
                token = tkOTHER;
              }
              break;
            }
            default: {
              token = tkOTHER;
              break;
            }
          }
#endif /* SQLITE_OMIT_TRIGGER */
          zSql += nId-1;
        }else{
          /* Operators and special symbols */
          token = tkOTHER;
        }
        break;
      }
    }
    state = trans[state][token];
    zSql++;
  }
  return state==1;
}

#ifndef SQLITE_OMIT_UTF16
/*
** This routine is the same as the sqlite3_complete() routine described
** above, except that the parameter is required to be UTF-16 encoded, not
** UTF-8.
*/
SQLITE_API int sqlite3_complete16(const void *zSql){
  sqlite3_value *pVal;
  char const *zSql8;
  int rc;

#ifndef SQLITE_OMIT_AUTOINIT
  rc = sqlite3_initialize();
  if( rc ) return rc;
#endif
  pVal = sqlite3ValueNew(0);
  sqlite3ValueSetStr(pVal, -1, zSql, SQLITE_UTF16NATIVE, SQLITE_STATIC);
  zSql8 = sqlite3ValueText(pVal, SQLITE_UTF8);
  if( zSql8 ){
    rc = sqlite3_complete(zSql8);
  }else{
    rc = SQLITE_NOMEM_BKPT;
  }
  sqlite3ValueFree(pVal);
  return rc & 0xff;
}
#endif /* SQLITE_OMIT_UTF16 */
#endif /* SQLITE_OMIT_COMPLETE */

/************** End of complete.c ********************************************/
