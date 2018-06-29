/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

#include "monetdb_config.h"
#include "gdk.h"
#include "gdk_private.h"

static gdk_return
BATcross1(BAT **r1p, BAT **r2p, BAT *l, BAT *r)
{
	BAT *bn1, *bn2;
	BUN i, j;
	oid *restrict p1, *restrict p2;

	assert(BAThdense(l));
	assert(BAThdense(r));
	bn1 = BATnew(TYPE_void, TYPE_oid, BATcount(l) * BATcount(r), TRANSIENT);
	bn2 = BATnew(TYPE_void, TYPE_oid, BATcount(l) * BATcount(r), TRANSIENT);
	if (bn1 == NULL || bn2 == NULL) {
		BBPreclaim(bn1);
		BBPreclaim(bn2);
		return GDK_FAIL;
	}
	BATseqbase(bn1, 0);
	BATseqbase(bn2, 0);
	p1 = (oid *) Tloc(bn1, BUNfirst(bn1));
	p2 = (oid *) Tloc(bn2, BUNfirst(bn2));
	for (i = 0; i < BATcount(l); i++) {
		for (j = 0; j < BATcount(r); j++) {
			*p1++ = i + l->hseqbase;
			*p2++ = j + r->hseqbase;
		}
	}
	BATsetcount(bn1, BATcount(l) * BATcount(r));
	BATsetcount(bn2, BATcount(l) * BATcount(r));
	bn1->tsorted = 1;
	bn1->trevsorted = BATcount(l) <= 1;
	bn1->tkey = BATcount(r) <= 1;
	bn1->tdense = bn1->tkey != 0;
	bn1->T->nil = 0;
	bn1->T->nonil = 1;
	bn2->tsorted = BATcount(l) <= 1;
	bn2->trevsorted = BATcount(bn2) <= 1;
	bn2->tkey = BATcount(l) <= 1;
	bn2->tdense = bn2->tkey != 0;
	bn2->T->nil = 0;
	bn2->T->nonil = 1;
	BATseqbase(BATmirror(bn1), l->hseqbase);
	BATseqbase(BATmirror(bn2), r->hseqbase);
	*r1p = bn1;
	*r2p = bn2;
	return GDK_SUCCEED;
}

/* Calculate a cross product between bats l and r with optional
 * candidate lists sl for l and sr for r.
 * The result is two bats r1 and r2 which contain the OID (head
 * values) of the input bats l and r. */
gdk_return
BATsubcross(BAT **r1p, BAT **r2p, BAT *l, BAT *r, BAT *sl, BAT *sr)
{
	BAT *bn1, *bn2, *t;

	if (BATcross1(&bn1, &bn2, sl ? sl : l, sr ? sr : r) != GDK_SUCCEED)
		return GDK_FAIL;
	if (sl) {
		t = BATproject(bn1, sl);
		BBPunfix(bn1->batCacheid);
		if (t == NULL) {
			BBPunfix(bn2->batCacheid);
			return GDK_FAIL;
		}
		bn1 = t;
	}
	if (sr) {
		t = BATproject(bn2, sr);
		BBPunfix(bn2->batCacheid);
		if (t == NULL) {
			BBPunfix(bn1->batCacheid);
			return GDK_FAIL;
		}
		bn2 = t;
	}
	*r1p = bn1;
	*r2p = bn2;
	return GDK_SUCCEED;
}
