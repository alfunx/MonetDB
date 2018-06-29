/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

/*
 * author Martin Kersten
 * Inspection
 * This module introduces a series of commands that provide access
 * to information stored within the interpreter data structures.
 * It's primary use is debugging.
 * In all cases, the pseudo BAT operation is returned that
 * should be garbage collected after being used.
 *
 * The main performance drain would be to use a pseudo BAT directly to
 * successively access it components. This can be avoided by first assigning
 * the pseudo BAT to a variable.
 */
#include "monetdb_config.h"
#include "inspect.h"

static void
pseudo(bat *ret, BAT *b, str X1,str X2, str X3) {
	char buf[BUFSIZ];
	snprintf(buf,BUFSIZ,"%s_%s_%s", X1,X2,X3);
	if (BBPindex(buf) <= 0)
		BATname(b,buf);
	BATroles(b,X1,X2);
	BATmode(b,TRANSIENT);
	BATfakeCommit(b);
	*ret = b->batCacheid;
	BBPkeepref(*ret);
}

/*
 * Symbol table
 * Mal symbol table and environment analysis.
 *
 * Collect symbol table information in a series of BATs for analysis
 * and display. Note, the elements are aligned using a counter,
 * which makes it susceptable for intermediate updates
 */

str
INSPECTgetAllFunctions(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	Module s;
	Symbol t;
	int i;
	BAT *b = BATnew(TYPE_void, TYPE_str, 256, TRANSIENT);
	bat *ret = getArgReference_bat(stk,pci,0);

	(void) mb;
	if (b == 0)
		throw(MAL, "inspect.getgetFunctionId", MAL_MALLOC_FAIL );
	BATseqbase(b, 0);
	s = cntxt->nspace;
	while (s) {
		for (i = 0; s && i < MAXSCOPE; i++)
			if (s->subscope[i]) {
				for (t = s->subscope[i]; t; t = t->peer) {
					InstrPtr sig = getSignature(t);
					BUNappend(b, getFunctionId(sig), FALSE);
				}
			}
		s = s->outer;
	}
	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view","symbol","function");

	return MAL_SUCCEED;
}

str
INSPECTgetAllModules(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	Module s;
	Symbol t;
	int i;
	BAT *b = BATnew(TYPE_void, TYPE_str, 256, TRANSIENT);
	bat *ret = getArgReference_bat(stk,pci,0);

	(void) mb;
	if (b == 0)
		throw(MAL, "inspect.getmodule", MAL_MALLOC_FAIL);
	BATseqbase(b, 0);
	s = cntxt->nspace;
	while (s) {
		for (i = 0; s && i < MAXSCOPE; i++)
			if (s->subscope[i]) {
				for (t = s->subscope[i]; t; t = t->peer) {
					InstrPtr sig = getSignature(t);

					BUNappend(b, getModuleId(sig), FALSE);
				}
			}
		s = s->outer;
	}
	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view","symbol","module");

	return MAL_SUCCEED;
}

str
INSPECTgetkind(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	Module s;
	Symbol t;
	int i;
	BAT *b = BATnew(TYPE_void, TYPE_str, 256, TRANSIENT);
	bat *ret = getArgReference_bat(stk,pci,0);

	(void)mb;
	if (b == 0)
		throw(MAL, "inspect.get", MAL_MALLOC_FAIL);
	BATseqbase(b, 0);
	s = cntxt->nspace;
	while (s) {
		for (i = 0; s && i < MAXSCOPE; i++)
			if (s->subscope[i]) {
				for (t = s->subscope[i]; t; t = t->peer) {
					InstrPtr sig = getSignature(t);
					str kind = operatorName(sig->token);
					BUNappend(b, kind, FALSE);
				}
			}
		s = s->outer;
	}
	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view","symbol","kind");

	return MAL_SUCCEED;
}


str
INSPECTgetAllSignatures(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	Module s;
	Symbol t;
	int i;
	BAT *b = BATnew(TYPE_void, TYPE_str, 256, TRANSIENT);
	char sig[BLOCK],*a;
	bat *ret = getArgReference_bat(stk,pci,0);

	(void)mb;
	if (b == 0)
		throw(MAL, "inspect.get", MAL_MALLOC_FAIL);
	BATseqbase(b, 0);
	s = cntxt->nspace;
	while (s) {
		for (i = 0; s && i < MAXSCOPE; i++)
			if (s->subscope[i]) {
				for (t = s->subscope[i]; t; t = t->peer) {
					fcnDefinition(t->def, getSignature(t), sig, 0,sig,BLOCK);
					a= strstr(sig,"address");
					if(a) *a = 0;
					BUNappend(b, (a = strchr(sig, '(')) ? a : "", FALSE);
				}
			}
		s = s->outer;
	}
	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view"," symbol","address");

	return MAL_SUCCEED;
}
str
INSPECTgetAllAddresses(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	Module s;
	Symbol t;
	int i;
	BAT *b = BATnew(TYPE_void, TYPE_str, 256, TRANSIENT);
	char sig[BLOCK],*a;
	bat *ret = getArgReference_bat(stk,pci,0);

	(void)mb;

	if (b == 0)
		throw(MAL, "inspect.get", MAL_MALLOC_FAIL);
	BATseqbase(b, 0);
	s = cntxt->nspace;
	while (s) {
		for (i = 0; s && i < MAXSCOPE; i++)
			if (s->subscope[i]) {
				for (t = s->subscope[i]; t; t = t->peer) {
					fcnDefinition(t->def, getSignature(t), sig, 0,sig,BLOCK);
					a= strstr(sig,"address");
					if( a)
						for( a=a+7; isspace((int) *a); a++)
							;
					BUNappend(b, (a? a: "nil"), FALSE);
				}
			}
		s = s->outer;
	}
	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view"," symbol","address");

	return MAL_SUCCEED;
}

str
INSPECTgetDefinition(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	bat *ret = getArgReference_bat(stk,pci,0);
	str *mod = getArgReference_str(stk,pci,1);
	str *fcn = getArgReference_str(stk,pci,2);
	Symbol s;
	BAT *b;
	(void)mb;

	s = findSymbol(cntxt->nspace, putName(*mod), putName(*fcn));
	if (s == 0)
		throw(MAL, "inspect.getDefinition", RUNTIME_SIGNATURE_MISSING);

	b = BATnew(TYPE_void, TYPE_str, 256, TRANSIENT);
	if (b == 0)
		throw(MAL, "inspect.getDefinition", MAL_MALLOC_FAIL);
	BATseqbase(b,0);

	while (s) {
		int i;
		str ps;

		for (i = 0; i < s->def->stop; i++) {
			ps = instruction2str(s->def,0, getInstrPtr(s->def, i), 0);
			BUNappend(b, ps + 1, FALSE);
			GDKfree(ps);
		}
		s = s->peer;
	}
	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view","fcn","stmt");

	return MAL_SUCCEED;
}

str
INSPECTgetSignature(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	bat *ret = getArgReference_bat(stk,pci,0);
	str *mod = getArgReference_str(stk,pci,1);
	str *fcn = getArgReference_str(stk,pci,2);
	Symbol s;
	str ps, tail;
	BAT *b;
	(void) mb;

	s = findSymbol(cntxt->nspace, getName(*mod), putName(*fcn));
	if (s == 0)
		throw(MAL, "inspect.getSignature", RUNTIME_SIGNATURE_MISSING);
	b = BATnew(TYPE_void, TYPE_str, 12, TRANSIENT);
	if (b == 0)
		throw(MAL, "inspect.getSignature", MAL_MALLOC_FAIL);
	BATseqbase(b,0);

	while (s != NULL) {
		if (idcmp(s->name, *fcn) == 0) {
			char *c, *w;

			ps = instruction2str(s->def, 0, getSignature(s), 0);
			c = strchr(ps, '(');
			if (c == 0) {
				GDKfree(ps);
				continue;
			}
			tail= strstr(c,"address");
			if( tail)
				*tail = 0;
			if (tail && (w=strchr(tail, ';')) )
				*w = 0;
			BUNappend(b, c, FALSE);
			GDKfree(ps);
		}
		s = s->peer;
	}

	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view","input","result");
	return MAL_SUCCEED;
}

str
INSPECTgetAddress(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	bat *ret = getArgReference_bat(stk,pci,0);
	str *mod = getArgReference_str(stk,pci,1);
	str *fcn = getArgReference_str(stk,pci,2);
	Symbol s;
	str ps, tail;
	BAT *b;
	(void) mb;

	s = findSymbol(cntxt->nspace, getName(*mod), putName(*fcn));
	if (s == 0)
		throw(MAL, "inspect.getAddress", RUNTIME_SIGNATURE_MISSING);
	b = BATnew(TYPE_void, TYPE_str, 12, TRANSIENT);
	if (b == 0)
		throw(MAL, "inspect.getAddress", MAL_MALLOC_FAIL);
	BATseqbase(b,0);

	while (s != NULL) {
		if (idcmp(s->name, *fcn) == 0) {
			char *c,*w;

			ps = instruction2str(s->def, 0, getSignature(s), 0);
			if(ps == NULL)
				continue;
			c = strchr(ps, '(');
			if (c == 0) {
				GDKfree(ps);
				continue;
			}
			tail= strstr(c,"address");
			if( tail){
				*tail = 0;
				for( tail=tail+7; isspace((int) *tail); tail++)  ;
			}
			if (tail && (w=strchr(tail, ';')) )
				*w = 0;
			BUNappend(b, (tail? tail: "nil"), FALSE);
			GDKfree(ps);
		}
		s = s->peer;
	}

	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view","input","result");
	return MAL_SUCCEED;
}
str
INSPECTgetComment(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	bat *ret = getArgReference_bat(stk,pci,0);
	str *mod = getArgReference_str(stk,pci,1);
	str *fcn = getArgReference_str(stk,pci,2);
	Symbol s;
	BAT *b;
	(void) mb;

	s = findSymbol(cntxt->nspace, getName(*mod), putName(*fcn));
	if (s == 0)
		throw(MAL, "inspect.getComment", RUNTIME_SIGNATURE_MISSING);
	b = BATnew(TYPE_void, TYPE_str, 12, TRANSIENT);
	if (b == 0)
		throw(MAL, "inspect.getComment", MAL_MALLOC_FAIL);
	BATseqbase(b,0);

	while (s != NULL) {
		if (idcmp(s->name, *fcn) == 0) {
			BUNappend(b, s->def->help, FALSE);
		}
		s = s->peer;
	}

	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view","input","result");
	return MAL_SUCCEED;
}

str
INSPECTgetSource(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	str *ret = getArgReference_str(stk,pci,0);
	str *mod = getArgReference_str(stk,pci,1);
	str *fcn = getArgReference_str(stk,pci,2);
	Symbol s;
	char *buf;
	size_t len,lim;
	(void) mb;

	s = findSymbol( cntxt->nspace, getName(*mod), putName(*fcn));
	if (s == 0)
		throw(MAL, "inspect.getSource", RUNTIME_SIGNATURE_MISSING);

	buf= (char*) GDKmalloc(BUFSIZ);
	if ( buf == NULL)
		throw(MAL, "inspect.getSource", MAL_MALLOC_FAIL);
	snprintf(buf,BUFSIZ,"%s.%s",*mod,*fcn);
	buf[0]=0;
	len= 0;
	lim= BUFSIZ;

	while (s) {
		int i;
		str ps;

		for (i = 0; i < s->def->stop; i++) {
			ps = instruction2str(s->def, 0, getInstrPtr(s->def, i), LIST_MAL_NAME );
			if( strlen(ps) >= lim-len){
				/* expand the buffer */
				char *bn;
				bn= GDKrealloc(buf, lim+BUFSIZ);
				if ( bn == NULL) {
					GDKfree(ps);
					GDKfree(buf);
					throw(MAL, "inspect.getSource", MAL_MALLOC_FAIL);
				}
				buf=bn;
				lim+= BUFSIZ;
			}
			strcat(buf+len,ps);
			len+= strlen(ps);
			buf[len++]='\n';
			buf[len]=0;
			GDKfree(ps);
		}
		s = s->peer;
	}
	*ret= buf;
	return MAL_SUCCEED;
}

str
INSPECTatom_names(bat *ret)
{
	int i;
	BAT *b = BATnew(TYPE_void, TYPE_str, 256, TRANSIENT);

	if (b == 0)
		throw(MAL, "inspect.getAtomNames", MAL_MALLOC_FAIL);
	BATseqbase(b,0);

	for (i = 0; i < GDKatomcnt; i++)
		BUNappend(b, ATOMname(i), FALSE);

	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view","atom","name");

	return MAL_SUCCEED;
}
str
INSPECTgetEnvironment(bat *ret, bat *ret2)
{
	BAT *b, *bn;

	b = COLcopy(GDKkey, GDKkey->ttype, 0, TRANSIENT);
	if (b == 0)
		throw(MAL, "inspect.getEnvironment", MAL_MALLOC_FAIL);
	bn = COLcopy(GDKval, GDKval->ttype, 0, TRANSIENT);
	if (bn == 0){
		BBPunfix(b->batCacheid);
		throw(MAL, "inspect.getEnvironment", MAL_MALLOC_FAIL);
 	}
	BATseqbase(b,0);
	BATseqbase(bn,0);

	BBPkeepref(*ret = b->batCacheid);
	BBPkeepref(*ret2 = bn->batCacheid);
	return MAL_SUCCEED;
}

str
INSPECTgetEnvironmentKey(str *ret, str *key)
{
	str s;
	*ret = 0;

	s= GDKgetenv(*key);
	if (s == 0)
		s= getenv(*key);
	if (s == 0)
		throw(MAL, "inspect.getEnvironment", "environment variable '%s' not found", *key);
	*ret = GDKstrdup(s);
	return MAL_SUCCEED;
}

str
INSPECTatom_sup_names(bat *ret)
{
	int i, k;
	BAT *b = BATnew(TYPE_void, TYPE_str, 256, TRANSIENT);

	if (b == 0)
		throw(MAL, "inspect.getAtomSuper", MAL_MALLOC_FAIL);
	BATseqbase(b,0);

	for (i = 0; i < GDKatomcnt; i++) {
		for (k = ATOMstorage(i); k > TYPE_str; k = ATOMstorage(k))
			;
		BUNappend(b, ATOMname(k), FALSE);
	}

	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view","atom","sup_name");

	return MAL_SUCCEED;
}

str
INSPECTatom_sizes(bat *ret)
{
	int i;
	int s;
	BAT *b = BATnew(TYPE_void, TYPE_int, 256, TRANSIENT);

	if (b == 0)
		throw(MAL, "inspect.getAtomSizes", MAL_MALLOC_FAIL);
	BATseqbase(b,0);

	for (i = 0; i < GDKatomcnt; i++) {
		s = ATOMsize(i);
		BUNappend(b, &s, FALSE);
	}

	if (!(b->batDirty&2)) BATsetaccess(b, BAT_READ);
	pseudo(ret,b,"view","atom","size");

	return MAL_SUCCEED;
}

/* calculate to trimmed storage space */
static lng
INSPECTcalcSize(MalBlkPtr mb){
	lng size,args=0,i;
	InstrPtr p;

	for(i=0;i<mb->stop; i++){
		p= getInstrPtr(mb,i);
		args += (p->argc-1)* sizeof(*p->argv);
	}
	size = (offsetof(InstrRecord, argv) +sizeof(InstrPtr)) * mb->stop;
	size += sizeof(VarRecord) * mb->vtop;
	size += args;
	return size;
}

str
INSPECTgetSize(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr p){
	lng *ret = getArgReference_lng(stk,p,0);


	*ret= INSPECTcalcSize(mb);
	(void) cntxt;
	(void) mb;
	return MAL_SUCCEED;
}

str
INSPECTgetFunctionSize(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	lng *ret = getArgReference_lng(stk,pci,0);
	str *mod = getArgReference_str(stk,pci,1);
	str *fcn = getArgReference_str(stk,pci,2);
	Symbol s;
	(void) mb;

	s = findSymbol(cntxt->nspace, getName(*mod), putName(*fcn));
	if (s == 0)
		throw(MAL, "inspect.getSize", RUNTIME_SIGNATURE_MISSING);
	*ret= INSPECTcalcSize(s->def);
	return MAL_SUCCEED;
}
/*
 * Display routines
 */
str
INSPECTshowFunction(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr p)
{
	(void) p;
	printFunction(cntxt->fdout, mb, stk, LIST_INPUT);
	return MAL_SUCCEED;
}

str
INSPECTshowFunction3(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr p)
{
	str modnme = getArgName(mb, p, 1);
	str fcnnme = getArgName(mb, p, 2);
	Symbol s = NULL;

	s = findSymbol(cntxt->nspace,getName(modnme), putName(fcnnme));

	if (s == NULL){
		char buf[BUFSIZ];
		snprintf(buf,BUFSIZ,"%s.%s", modnme, fcnnme);
		throw(MAL, "inspect.showSource",RUNTIME_SIGNATURE_MISSING "%s",buf);
	} else
		printFunction(cntxt->fdout, s->def, stk, LIST_INPUT);
	return MAL_SUCCEED;
}

str
INSPECTequalType(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	bit *ret;
	(void) stk;
	(void) cntxt;
	ret = getArgReference_bit(stk, pci, 0);
	*ret = getArgType(mb,pci,1)== getArgType(mb,pci,2);
	return MAL_SUCCEED;
}

str
INSPECTtypeName(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	str *hn, *tn =0;

	hn = getArgReference_str(stk, pci, 0);

	(void) cntxt;
	if( pci->retc== 2){
		tn = getArgReference_str(stk, pci, 1);
		*hn = getTypeName(TYPE_oid);
		*tn = getTypeName(getColumnType(getArgType(mb, pci, 2)));
	} else if (isaBatType(getArgType(mb,pci,1) ) ){
		bat *bid= getArgReference_bat(stk,pci,1);
		BAT *b;
		if ((b = BATdescriptor(*bid)) ) {
			*hn = getTypeName(newBatType(TYPE_oid,b->ttype));
			BBPunfix(b->batCacheid);
		} else
			*hn = getTypeName(getArgType(mb, pci, 1));
	} else
		*hn = getTypeName(getArgType(mb, pci, 1));
	return MAL_SUCCEED;
}
