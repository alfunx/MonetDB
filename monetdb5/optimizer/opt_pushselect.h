/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

#ifndef OPT_PUSHSELECT_H
#define OPT_PUSHSELECT_H
#include "opt_support.h"
#include "opt_prelude.h"

mal_export int OPTpushselectImplementation(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci);

#define OPTDEBUGpushselect  if ( optDebug & (LL_CONSTANT(1) <<DEBUG_OPT_PUSHSELECT) )

#endif
