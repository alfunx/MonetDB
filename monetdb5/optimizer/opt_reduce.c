/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

#include "monetdb_config.h"
#include "opt_reduce.h"
#include "mal_interpreter.h"

int
OPTreduceImplementation(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr p)
{
	int actions = 0;
	(void)cntxt;
	(void)stk;
	(void) p;

	actions = mb->vtop;
	trimMalVariables(mb,0);
	actions = actions - mb->vtop;
	return actions;
}
