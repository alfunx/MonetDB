/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

/*
 * This simple module unrolls the mat.pack into an incremental sequence.
 * This could speedup parallel processing and releases resources faster.
 */
#include "monetdb_config.h"
#include "opt_matpack.h"

int 
OPTmatpackImplementation(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	int v, i, j, limit, slimit;
	InstrPtr p,q;
	int actions = 0;
	InstrPtr *old;
	char *packIncrementRef = putName("packIncrement");

	(void) pci;
	(void) cntxt;
	(void) stk;		/* to fool compilers */
	old= mb->stmt;
	limit= mb->stop;
	slimit = mb->ssize;
	if ( newMalBlkStmt(mb,mb->stop) < 0)
		return 0;
	for (i = 0; i < limit; i++) {
		p = old[i];
		if( getModuleId(p) == matRef  && getFunctionId(p) == packRef && isaBatType(getArgType(mb,p,1))) {
			q = newStmt(mb, matRef, packIncrementRef);
			v = getArg(q,0);
			setVarType(mb,v,getArgType(mb,p,1));
			q = pushArgument(mb, q, getArg(p,1));
			q = pushInt(mb,q, p->argc - p->retc);

			for ( j = 2; j < p->argc; j++) {
				q = newStmt(mb,matRef, packIncrementRef);
				q = pushArgument(mb, q, v);
				q = pushArgument(mb, q, getArg(p,j));
				setVarType(mb,getArg(q,0),getVarType(mb,v));
				v = getArg(q,0);
			}
			getArg(q,0) = getArg(p,0);
			freeInstruction(p);
			actions++;
			continue;
		}
		pushInstruction(mb,p);
	} 
	for(; i<slimit; i++)
		if (old[i]) 
			freeInstruction(old[i]);
	GDKfree(old);
	return actions;
}
