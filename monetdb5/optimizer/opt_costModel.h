/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

#ifndef _OPT_COSTMODEL_H_
#define _OPT_COSTMODEL_H_

#include "mal.h"
#include <math.h>
#include "mal_interpreter.h"
#include "opt_support.h"
#include "opt_prelude.h"

mal_export int OPTcostModelImplementation(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci);

#define OPTDEBUGcostModel  if ( optDebug & ((lng) 1 <<DEBUG_OPT_COSTMODEL) )

#endif /* _OPT_COSTMODEL_H_ */
