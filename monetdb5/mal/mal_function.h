/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

#ifndef _MAL_FCN_H
#define _MAL_FCN_H

#include "mal_instruction.h"
#include "mal_module.h"
#include "mal_resolve.h"

typedef struct lifespan {
	int beginLifespan, endLifespan, lastUpdate;
} *Lifespan, LifespanRecord;

#define getLastUpdate(L,I)	(L[I].lastUpdate)
#define getEndLifespan(L,I)	(L[I].endLifespan)
#define getBeginLifespan(L,I)	(L[I].beginLifespan)

/* #define DEBUG_MAL_FCN */
/* #define DEBUG_CLONE */

mal_export Symbol   newFunction(str mod, str nme,int kind);
mal_export int      getPC(MalBlkPtr mb, InstrPtr p);

mal_export Symbol   getFunctionSymbol(Module scope, InstrPtr p);
mal_export void chkFlow(stream *out, MalBlkPtr mb);
mal_export void chkDeclarations(stream *out, MalBlkPtr mb);
mal_export void clrDeclarations(MalBlkPtr mb);
mal_export int isLoopBarrier(MalBlkPtr mb, int pc);
mal_export int getBlockExit(MalBlkPtr mb,int pc);
mal_export int getBlockBegin(MalBlkPtr mb,int pc);

#define newLifespan(M) (Lifespan)GDKzalloc(sizeof(LifespanRecord)*(M)->vsize)
mal_export Lifespan setLifespan(MalBlkPtr mb);

mal_export void printFunction(stream *fd, MalBlkPtr mb, MalStkPtr stk, int flg);
mal_export void debugFunction(stream *fd, MalBlkPtr mb, MalStkPtr stk, int flg, int first, int size);
mal_export void showFlowGraph(MalBlkPtr mb, MalStkPtr stk, str fname);

#include "mal_exception.h"

#define MAXDEPTH 32
#endif /*  _MAL_FCN_H*/
