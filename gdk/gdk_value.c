/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

/*
 * @a Martin L. Kersten & Peter Boncz
 * @v 2.0
 * @+ Value representation
 *
 *
 * When manipulating values, MonetDB puts them into value records.
 * The built-in types have a direct entry in the union. Others should
 * be represented as a pointer of memory in pval or as a string, which
 * is basically the same. In such cases the len field indicates the
 * size of this piece of memory.
 *
 * MonetDB extenders will use value records for passing parameters to
 * their new operators. MonetDB algebraic commands receive an (argc,
 * argv) combination, where argc is an integer indicating the size of
 * the the argv array of value records. On call, the first record,
 * argv[0], is always empty. The routine must place its return value -
 * if any - there. The other values are the parameters.
 *
 * Actually, the gdk value type defined here should become a built-in
 * type in the kernel. Next step will be to define the corresponding
 * extension module.
 *
 * @+ Value operations
 * The following primitives are required to manipulate value records.
 * Note that binding a BAT requires upgrading its reference count.
 * The receiver of the value should have been cleared or represent
 * free space.
 */
#include "monetdb_config.h"
#include "gdk.h"
#include "gdk_private.h"

/* Set V to the type/value combination in T/P.  Also see VALinit.  In
 * this version, if P refers to an external type, no new memory is
 * allocated, but instead the pointer P is given to V. */
ValPtr
VALset(ValPtr v, int t, ptr p)
{
	switch (ATOMstorage(v->vtype = t)) {
	case TYPE_void:
		v->val.oval = *(oid *) p;
		break;
	case TYPE_bte:
		v->val.btval = *(bte *) p;
		break;
	case TYPE_sht:
		v->val.shval = *(sht *) p;
		break;
	case TYPE_int:
		v->val.ival = *(int *) p;
		break;
	case TYPE_flt:
		v->val.fval = *(flt *) p;
		break;
	case TYPE_dbl:
		v->val.dval = *(dbl *) p;
		break;
	case TYPE_lng:
		v->val.lval = *(lng *) p;
		break;
#ifdef HAVE_HGE
	case TYPE_hge:
		v->val.hval = *(hge *) p;
		break;
#endif
	case TYPE_str:
		v->val.sval = (str) p;
		v->len = ATOMlen(t, p);
		break;
	case TYPE_ptr:
		v->val.pval = *(ptr *) p;
		v->len = ATOMlen(t, *(ptr *) p);
		break;
	default:
		v->val.pval = p;
		v->len = ATOMlen(t, p);
		break;
	}
	return v;
}

/* Return a pointer to the value contained in V.  Also see VALptr
 * which returns a const void *. */
void *
VALget(ValPtr v)
{
	switch (ATOMstorage(v->vtype)) {
	case TYPE_void: return (void *) &v->val.oval;
	case TYPE_bte: return (void *) &v->val.btval;
	case TYPE_sht: return (void *) &v->val.shval;
	case TYPE_int: return (void *) &v->val.ival;
	case TYPE_flt: return (void *) &v->val.fval;
	case TYPE_dbl: return (void *) &v->val.dval;
	case TYPE_lng: return (void *) &v->val.lval;
#ifdef HAVE_HGE
	case TYPE_hge: return (void *) &v->val.hval;
#endif
	case TYPE_str: return (void *) v->val.sval;
	default:       return (void *) v->val.pval;
	}
}

/* Clear V to an empty value (type void, value nil), freeing any
 * memory allocated for external types.  See VALempty for when V does
 * not yet contain a value. */
void
VALclear(ValPtr v)
{
	if (v->vtype == TYPE_str || ATOMextern(v->vtype)) {
		if (v->val.pval && v->val.pval != str_nil)
			GDKfree(v->val.pval);
	}
	VALempty(v);
}

/* Initialize V to an empty value (type void, value nil).  See
 * VALclear for when V already contains a value. */
void
VALempty(ValPtr v)
{
	v->len = 0;
	v->val.oval = oid_nil;
	v->vtype = TYPE_void;
}

/* Create a copy of S into D, allocating space for external values
 * (non-fixed sized values).  See VALinit for a version where the
 * source is not in a VALRecord. */
ValPtr
VALcopy(ValPtr d, const ValRecord *s)
{
	if (!ATOMextern(s->vtype)) {
		*d = *s;
	} else if (s->val.pval == 0) {
		d->val.pval = ATOMnil(s->vtype);
		d->vtype = s->vtype;
	} else if (s->vtype == TYPE_str) {
		d->vtype = TYPE_str;
		d->val.sval = GDKstrdup(s->val.sval);
		d->len = strLen(d->val.sval);
	} else if (s->vtype == TYPE_bit) {
		d->vtype = s->vtype;
		d->len = 1;
		d->val.btval = s->val.btval;
	} else {
		ptr p = s->val.pval;

		d->vtype = s->vtype;
		d->len = ATOMlen(d->vtype, p);
		d->val.pval = GDKmalloc(d->len);
		if (d->val.pval)
			memcpy(d->val.pval, p, d->len);
	}
	return d;
}

/* Create a copy of the type value combination in TPE/S, allocating
 * space for external values (non-fixed sized values).  See VALcopy
 * for a version where the source is in a ValRecord. */
ValPtr
VALinit(ValPtr d, int tpe, const void *s)
{
	if (!ATOMextern(tpe)) {
		d->vtype = tpe;
		memcpy(&d->val.ival, s, ATOMlen(tpe, s));
	} else if (s == 0) {
		GDKerror("VALinit:unsupported init\n");
		d->vtype = TYPE_int;
	} else if (tpe >= TYPE_str && ATOMstorage(tpe) == TYPE_str) {
		d->vtype = tpe;
		d->val.sval = GDKstrdup(s);
		d->len = strLen(s);
	} else {
		d->vtype = tpe;
		d->len = ATOMlen(tpe, s);
		d->val.pval = GDKmalloc(d->len);
		if (d->val.pval)
			memcpy(d->val.pval, s, d->len);
	}
	return d;
}

/* Format the value in RES in the standard way for the type of RES
 * into a newly allocated buffer which is returned through BUF. */
int
VALformat(char **buf, const ValRecord *res)
{
	*buf = 0;
	return ATOMformat(res->vtype, VALptr(res), buf);
}

/* Convert (cast) the value in T to the type TYP, do this in place.
 * Return a pointer to the converted value, or NULL if the conversion
 * didn't succeed.  Also see VARconvert. */
ptr
VALconvert(int typ, ValPtr t)
{
	int src_tpe = t->vtype;
	ValRecord dst;

	dst.vtype = typ;
	/* use base types for user types */
	if (src_tpe > TYPE_str)
		src_tpe = ATOMstorage(src_tpe);
	if (dst.vtype > TYPE_str)
		dst.vtype = ATOMstorage(dst.vtype);
	else if (dst.vtype == TYPE_void)
		dst.vtype = TYPE_oid;

	/* first convert into a new location */
	if (VARconvert(&dst, t, 0) != GDK_SUCCEED)
		return NULL;

	/* then maybe free the old */
	if (src_tpe != dst.vtype &&
	    t->vtype != typ &&
	    dst.vtype != TYPE_void &&
	    (src_tpe >= TYPE_str || dst.vtype >= TYPE_str))
		VALclear(t);
	/* and finally copy the result */
	*t = dst;
	/* make sure we return the correct type (not the storage type) */
	t->vtype = typ;
	return VALget(t);
}

/* Compare two values in P and Q and return -1/0/1 depending on
 * whether P is less than, equal to, or larger than Q. Also return -1
 * if P or Q is NULL or NIL, or if the types of P and Q are not
 * equal. */
int
VALcmp(const ValRecord *p, const ValRecord *q)
{

	int (*cmp)(const void *, const void *);
	int tpe;
	const void *nilptr, *pp, *pq;

	if (p == 0 || q == 0)
		return -1;
	if ((tpe = p->vtype) != q->vtype)
		return -1;

	if (tpe == TYPE_ptr)
		return 0;	/* ignore comparing C pointers */
	cmp = ATOMcompare(tpe);
	nilptr = ATOMnilptr(tpe);
	pp = VALptr(p);
	pq = VALptr(q);
	if ((*cmp)(pp, nilptr) == 0 && (*cmp)(pq, nilptr) == 0)
		return 0;	/* eq nil val */
	if ((*cmp)(pp, nilptr) == 0 || (*cmp)(pq, nilptr) == 0)
		return -1;
	return (*cmp)(pp, pq);

}

/* Return TRUE if the value in V is NIL. */
int
VALisnil(const ValRecord *v)
{
	switch (v->vtype) {
	case TYPE_void:
		return 1;
	case TYPE_bte:
		return v->val.btval == bte_nil;
	case TYPE_sht:
		return v->val.shval == sht_nil;
	case TYPE_int:
		return v->val.ival == int_nil;
	case TYPE_wrd:
		return v->val.wval == wrd_nil;
	case TYPE_lng:
		return v->val.lval == lng_nil;
#ifdef HAVE_HGE
	case TYPE_hge:
		return v->val.hval == hge_nil;
#endif
	case TYPE_flt:
		return v->val.fval == flt_nil;
	case TYPE_dbl:
		return v->val.dval == dbl_nil;
	case TYPE_oid:
		return v->val.oval == oid_nil;
	case TYPE_bat:
		return v->val.bval == bat_nil || v->val.bval == 0;
	default:
		break;
	}
	return (*ATOMcompare(v->vtype))(VALptr(v), ATOMnilptr(v->vtype)) == 0;
}
