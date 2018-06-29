/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

/*
 * (c) Martin Kersten
 *  MonetDB Basic Definitions
 * Definitions that need to included in every file of the Monet system, as well as in user defined module implementations.
 */
#ifndef _MAL_H
#define _MAL_H

#include <gdk.h>

#ifdef WIN32
#if !defined(LIBMAL) && !defined(LIBATOMS) && !defined(LIBKERNEL) && !defined(LIBMAL) && !defined(LIBOPTIMIZER) && !defined(LIBSCHEDULER) && !defined(LIBMONETDB5)
#define mal_export extern __declspec(dllimport)
#else
#define mal_export extern __declspec(dllexport)
#endif
#else
#define mal_export extern
#endif

#ifdef HAVE_SYS_TIMES_H
# include <sys/times.h>
#endif

#include <setjmp.h>
/*
 * MonetDB Calling Options
 * The number of invocation arguments is kept to a minimum.
 * See `man mserver5` or tools/mserver/mserver5.1
 * for additional system variable settings.
 */
#define MAXSCRIPT 64

/*
 * MonetDB assumes it can use most of the machines memory,
 * leaving a small portion for other programs.
 */
#define GB (((lng)1024)*1024*1024)
#define MEMORY_THRESHOLD  (0.2 * monet_memory > 8 * GB?  monet_memory - 8 * GB: 0.8 * monet_memory)

mal_export char     monet_cwd[PATHLENGTH];
mal_export size_t	monet_memory;
mal_export char 	monet_characteristics[PATHLENGTH];
mal_export lng 		memorypool;      /* memory claimed by concurrent threads */
mal_export int 		memoryclaims;    /* number of threads active with expensive operations */
mal_export int		mal_trace;		/* enable profile events on console */
#ifdef HAVE_HGE
mal_export int have_hge;
#endif

/*
   See gdk/gdk.mx for the definition of all debug masks.
   See `man mserver5` or tools/mserver/mserver5.1
   for a documentation of all debug options.
*/
#define GRPthreads (THRDMASK | PARMASK)
#define GRPmemory (MEMMASK | ALLOCMASK )
#define GRPproperties (CHECKMASK | PROPMASK | BATMASK )
#define GRPio (IOMASK | PERFMASK )
#define GRPheaps (HEAPMASK)
#define GRPtransactions (TMMASK | DELTAMASK | TEMMASK)
#define GRPmodules (LOADMASK)
#define GRPalgorithms (ALGOMASK | ESTIMASK)
#define GRPperformance (JOINPROPMASK | DEADBEEFMASK)
#define GRPoptimizers  (OPTMASK)
#define GRPforcemito (FORCEMITOMASK)
#define GRPrecycler (1<<30)

mal_export MT_Lock  mal_contextLock;
mal_export MT_Lock  mal_remoteLock;
mal_export MT_Lock  mal_profileLock ;
mal_export MT_Lock  mal_copyLock ;
mal_export MT_Lock  mal_delayLock ;
mal_export MT_Lock  mal_beatLock ;


mal_export int mal_init(void);
mal_export void mal_exit(void);
mal_export void mserver_reset(void);

/* This should be here, but cannot, as "Client" isn't known, yet ... |-(
 * For now, we move the prototype declaration to src/mal/mal_client.c,
 * the only place where it is currently used. Maybe, we should concider
 * also moving the implementation there...
 */


/* Listing modes are globally known */
#define LIST_INPUT      1       /* echo original input */
#define LIST_MAL_NAME   2       /* show variable name */
#define LIST_MAL_TYPE   4       /* show type resolutoin */
#define LIST_MAL_VALUE  8		/* list bat tuple count */
#define LIST_MAL_PROPS 16       /* show variable properties */
#define LIST_MAL_MAPI  32       /* output Mapi compatible output */
#define LIST_MAL_REMOTE  64       /* output MAL for remote execution */
#define LIST_MAL_CALL  (LIST_MAL_NAME | LIST_MAL_VALUE )
#define LIST_MAL_DEBUG (LIST_MAL_NAME | LIST_MAL_VALUE | LIST_MAL_TYPE | LIST_MAL_PROPS)
#define LIST_MAL_ALL   (LIST_MAL_NAME | LIST_MAL_VALUE | LIST_MAL_TYPE | LIST_MAL_PROPS | LIST_MAL_MAPI)

#ifndef WORDS_BIGENDIAN
#define STRUCT_ALIGNED
#endif

/* The MAL instruction block type definitions */
/* Variable properties */
#define VAR_CONSTANT 	1
#define VAR_TYPEVAR	2
#define VAR_FIXTYPE	4
#define VAR_UDFTYPE	8
#define VAR_CLEANUP	16
#define VAR_INIT	32
#define VAR_USED	64
#define VAR_CLIST 	128	/* Candidate list variable */
#define VAR_DISABLED	256		/* used for comments and scheduler */

/* type check status is kept around to improve type checking efficiency */
#define TYPE_ERROR      -1
#define TYPE_UNKNOWN     0
#define TYPE_RESOLVED    2
#define GARBAGECONTROL   3

#define VARARGS 1				/* deal with variable arguments */
#define VARRETS 2

#define SERVERSHUTDOWNDELAY 5 /* seconds */

typedef int malType;
typedef str (*MALfcn) ();

typedef struct SYMDEF {
	struct SYMDEF *peer;		/* where to look next */
	struct SYMDEF *skip;		/* skip to next different symbol */
	str name;
	int kind;
	struct MALBLK *def;			/* the details of the MAL fcn */
} *Symbol, SymRecord;

typedef struct VARRECORD {
	str name;					/* argname or lexical value repr */
	malType type;				/* internal type signature */
	int flags;					/* see below, reserve some space */
	int tmpindex;				/* temporary variable */
	ValRecord value;
	int eolife;					/* pc index when it should be garbage collected */
	int worker;					/* tread id of last worker producing it */
	str stc;					/* rendering schema.table.column */
	BUN rowcnt;					/* estimated row count*/
} *VarPtr, VarRecord;

/* For performance analysis we keep track of the number of calls and
 * the total time spent while executing the instruction. (See
 * mal_profiler.mx) 
 */

typedef struct {
	bit token;					/* instruction type */
	bit barrier;				/* flow of control modifier takes:
								   BARRIER, LEAVE, REDO, EXIT, CATCH, RAISE */
	bit typechk;				/* type check status */
	bit gc;						/* garbage control flags */
	bit polymorphic;			/* complex type analysis */
	bit varargs;				/* variable number of arguments */
	int recycle;				/* <0 or index into recycle cache */
	int jump;					/* controlflow program counter */
	int pc;						/* location in MAL plan for profiler*/
	MALfcn fcn;					/* resolved function address */
	struct MALBLK *blk;			/* resolved MAL function address */
	int mitosis;				/* old mtProp value */
	/* inline statistics */
	struct timeval clock;		/* when the last call was started */
	lng ticks;					/* total micro seconds spent in last call */
	int calls;					/* number of calls made to this instruction */
	lng totticks;				/* total time spent on this instruction. */
	lng rbytes;					/* accumulated number of bytes read, currently ignored */
	lng wbytes;					/* accumulated number of bytes produced */
	/* the core admin */
	str modname;				/* module context */
	str fcnname;				/* function name */
	int argc, retc, maxarg;		/* total and result argument count */
	int argv[FLEXIBLE_ARRAY_MEMBER]; /* at least a few entries */
} *InstrPtr, InstrRecord;

typedef struct MALBLK {
	str binding;				/* related C-function */
	str help;					/* supportive commentary */
	oid tag;					/* unique block tag */
	struct MALBLK *alternative;
	int vtop;					/* next free slot */
	int vsize;					/* size of variable arena */
	VarRecord **var;			/* Variable table */
	int stop;					/* next free slot */
	int ssize;					/* byte size of arena */
	InstrPtr *stmt;				/* Instruction location */
	int ptop;					/* next free slot */
	int psize;					/* byte size of arena */
	int inlineProp;				/* inline property */
	int unsafeProp;				/* unsafe property */

	int errors;					/* left over errors */
	int typefixed;				/* no undetermined instruction */
	int flowfixed;				/* all flow instructions are fixed */
	struct MALBLK *history;		/* of optimizer actions */
	short keephistory;			/* do we need the history at all */
	short dotfile;				/* send dot file to stethoscope? */
	str marker;					/* history points are marked for backtracking */
	int maxarg;					/* keep track on the maximal arguments used */
	ptr replica;				/* for the replicator tests */
	sht recycle;				/* execution subject to recycler control */
	lng recid;					/* Recycler identifier */
	sht trap;					/* call debugger when called */
	lng starttime;				/* track when the query started, for resource management */
	lng runtime;				/* average execution time of block in ticks */
	int calls;					/* number of calls */
	lng optimize;				/* total optimizer time */
	int activeClients;			/* load during mitosis optimization */
} *MalBlkPtr, MalBlkRecord;

#define STACKINCR   128
#define MAXGLOBALS  (4 * STACKINCR)
#define MAXSHARES   8

typedef int (*DFhook) (void *, void *, void *, void *);

typedef struct MALSTK {
	int stksize;
	int stktop;
	int stkbot;			/* the first variable to be initialized */
	int stkdepth;		/* to protect against runtime stack overflow */
	int calldepth;		/* to protect against runtime stack overflow */
	short keepAlive;	/* do not garbage collect when set */
	short garbageCollect; /* stack needs garbage collection */
	lng tmpspace;		/* amount of temporary space produced */
	/*
	 * Parallel processing is mostly driven by dataflow, but within this context
	 * there may be different schemes to take instructions into execution.
	 * The admission scheme (and wrapup) are the necessary scheduler hooks.
	 */
	DFhook admit;
	DFhook wrapup;
	MT_Lock stklock;	/* used for parallel processing */

/*
 * It is handy to administer the timing in the stack frame
 * for use in profiling and recylcing instructions.
 */
	struct timeval clock;		/* time this stack was created */
	char cmd;		/* debugger and runtime communication */
	char status;	/* srunning 'R' uspended 'S', quiting 'Q' */
	int pcup;		/* saved pc upon a recursive all */
	int tag;		/* unique invocation call tag */
	struct MALSTK *up;	/* stack trace list */
	struct MALBLK *blk;	/* associated definition */
	ValRecord stk[FLEXIBLE_ARRAY_MEMBER];
} MalStack, *MalStkPtr;

#endif /*  _MAL_H*/
