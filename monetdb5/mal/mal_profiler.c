/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

/* (c) M.L. Kersten
 * Performance tracing
 * The stethoscope/tachograph and tomograph performance monitors have exclusive access 
 * to a single event stream, which avoids concurrency conflicts amongst clients. 
 * It also avoid cluthered event records on the stream. Since this event stream is owned
 * by a client, we should ensure that the profiler is automatically 
 * reset once the owner leaves. 
 */
#include "monetdb_config.h"
#include "mal_function.h"
#include "mal_listing.h"
#include "mal_profiler.h"
#include "mal_runtime.h"
#include "mal_debugger.h"
#include "mal_resource.h"

static void cachedProfilerEvent(MalBlkPtr mb, MalStkPtr stk, InstrPtr pci);

stream *eventstream = 0;

static int sqlProfiling = FALSE;
static str myname = 0;	// avoid tracing the profiler module
static int eventcounter = 0;
static str prettify = "\n"; /* or ' ' for single line json output */

static int TRACE_init = 0;
int malProfileMode = 0;     /* global flag to indicate profiling mode */

static volatile ATOMIC_TYPE hbdelay = 0;

#ifdef HAVE_SYS_RESOURCE_H
struct rusage infoUsage;
static struct rusage prevUsage;
#endif

static struct{
	lng user, nice, system, idle, iowait;
	double load;
} corestat[256];

/* the heartbeat process produces a ping event once every X milliseconds */
//#ifdef ATOMIC_LOCK
//static MT_Lock mal_beatLock MT_LOCK_INITIALIZER("beatLock");
//#endif

#define LOGLEN 8192
#define lognew()  loglen = 0; logbase = logbuffer; *logbase = 0;

#define logadd(...) {														\
	do {																\
		loglen += snprintf(logbase+loglen, LOGLEN -1 - loglen, __VA_ARGS__); \
		assert(loglen < LOGLEN); \
	} while (0);}


// The heart beat events should be sent to all outstanding channels.
static void logjsonInternal(char *logbuffer)
{	
	size_t len;

	len = strlen(logbuffer);

	MT_lock_set(&mal_profileLock);
	if (eventstream) {
	// upon request the log record is sent over the profile stream
		(void) mnstr_write(eventstream, logbuffer, 1, len);
		(void) mnstr_flush(eventstream);
	}
	MT_lock_unset(&mal_profileLock);
}

/* JSON rendering method of performance data. 
 * The eventparser may assume this layout for ease of parsing
EXAMPLE:
{
"event":6        ,
"time":"15:37:13.799706",
"thread":3,
"function":"user.s3_1",
"pc":1,
"tag":10397,
"state":"start",
"usec":0,
"rss":215,
"size":0,
"oublock":8,
"stmt":"X_41=0@0:void := querylog.define(\"select count(*) from tables;\":str,\"default_pipe\":str,30:int);",
"short":"define( \"select count(*) from tables;\",\"default_pipe\",30 )",
"prereq":[]
}
*/
static void
renderProfilerEvent(MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, int start, str usrname)
{
	char logbuffer[LOGLEN], *logbase;
	int loglen;
	char ctm[26];
	time_t clk;
	struct timeval clock;
	str stmt, c;
	char *tbuf;
	str stmtq;
	lng usec= GDKusec();


	if( start) // show when instruction was started
		clock = pci->clock;
	else 
		gettimeofday(&clock, NULL);
	clk = clock.tv_sec;

	/* make profile event tuple  */
	lognew();
	logadd("{%s",prettify); // fill in later with the event counter

#ifdef HAVE_CTIME_R3
	tbuf = ctime_r(&clk, ctm, sizeof(ctm));
#else
#ifdef HAVE_CTIME_R
	tbuf = ctime_r(&clk, ctm);
#else
	tbuf = ctime(&clk);
#endif
#endif
	tbuf[19]=0;
	/* there should be less than 10^6 == 1M usecs in 1 sec */
	assert(clock.tv_usec >= 0 && clock.tv_usec < 1000000);
	if( usrname)
		logadd("\"user\":\"%s\",%s",usrname, prettify);
	logadd("\"clk\":"LLFMT",%s",usec,prettify);
	logadd("\"ctime\":\"%s.%06ld\",%s", tbuf+11, (long)clock.tv_usec, prettify);
	logadd("\"thread\":%d,%s", THRgettid(),prettify);

	logadd("\"function\":\"%s.%s\",%s", getModuleId(getInstrPtr(mb, 0)), getFunctionId(getInstrPtr(mb, 0)), prettify);
	logadd("\"pc\":%d,%s", mb?getPC(mb,pci):0, prettify);
	logadd("\"tag\":%d,%s", stk?stk->tag:0, prettify);

	if( start){
		logadd("\"state\":\"start\",%s", prettify);
		// determine the Estimated Time of Completion
		if ( pci->calls){
			logadd("\"usec\":"LLFMT",%s", pci->totticks/pci->calls, prettify);
		} else{
			logadd("\"usec\":"LLFMT",%s", pci->ticks, prettify);
		}
	} else {
		logadd("\"state\":\"done\",%s", prettify);
		logadd("\"usec\":"LLFMT",%s", pci->ticks, prettify);
	}
	logadd("\"rss\":"SZFMT ",%s", MT_getrss()/1024/1024, prettify);
	logadd("\"size\":"LLFMT ",%s", pci? pci->wbytes/1024/1024:0, prettify);	// result size

#ifdef NUMAprofiling
		logadd("\"numa\":[");
		if(mb)
		for( i= pci->retc ; i < pci->argc; i++)
		if( !isVarConstant(mb, getArg(pci,i)) && mb->var[getArg(pci,i)]->worker)
			logadd("%c %d", (i?',':' '), mb->var[getArg(pci,i)]->worker);
		logadd("],%s", prettify);
#endif

#ifdef HAVE_SYS_RESOURCE_H
	getrusage(RUSAGE_SELF, &infoUsage);
	if(infoUsage.ru_inblock - prevUsage.ru_inblock)
		logadd("\"inblock\":%ld,%s", infoUsage.ru_inblock - prevUsage.ru_inblock, prettify);
	if(infoUsage.ru_oublock - prevUsage.ru_oublock)
		logadd("\"oublock\":%ld,%s", infoUsage.ru_oublock - prevUsage.ru_oublock, prettify);
	if(infoUsage.ru_majflt - prevUsage.ru_majflt)
		logadd("\"majflt\":%ld,%s", infoUsage.ru_majflt - prevUsage.ru_majflt, prettify);
	if(infoUsage.ru_nswap - prevUsage.ru_nswap)
		logadd("\"nswap\":%ld,s%sn", infoUsage.ru_nswap - prevUsage.ru_nswap, prettify);
	if(infoUsage.ru_nvcsw - prevUsage.ru_nvcsw)
		logadd("\"nvcsw\":%ld,%s", infoUsage.ru_nvcsw - prevUsage.ru_nvcsw +infoUsage.ru_nivcsw - prevUsage.ru_nivcsw, prettify);
	prevUsage = infoUsage;
#endif

	if( mb){
		char prereq[BUFSIZ];
		size_t len;
		int i,j,k,comma;
		InstrPtr q;

		/* generate actual call statement */
		stmt = instruction2str(mb, stk, pci, LIST_MAL_ALL);
		if (stmt) {
			c = stmt;

			while (*c && isspace((int)*c))
				c++;
			if( *c){
				stmtq = mal_quote(c, strlen(c));
				if (stmtq != NULL) {
					logadd("\"stmt\":\"%s\",%s", stmtq,prettify);
					GDKfree(stmtq);
				}
			}
			GDKfree(stmt);
		}

		// ship the beautified version as well

		stmt = shortStmtRendering(mb, stk, pci);
		stmtq = mal_quote(stmt, strlen(stmt));
		if (stmtq != NULL) {
			logadd("\"short\":\"%s\",%s", stmtq, prettify);
			GDKfree(stmtq);
		} 
		GDKfree(stmt);


		// collect the prerequisite pre-requisite statements
		prereq[0]='[';
		prereq[1]=0;
		len = 1;
		comma=0;
		for(i= pci->retc; i < pci->argc; i++){
			for( j = pci->pc-1; j > 0; j--){
				q= getInstrPtr(mb,j);
				for( k=0; k < q->retc; k++)
					if( getArg(q,k) == getArg(pci,i))
						break;
				if( k < q->retc){
					snprintf(prereq + len, BUFSIZ-len,"%s%d", (comma?",":""), j);
					len = strlen(prereq);
					comma++;
					break;
				}
			}
		}
#define MALARGUMENTDETAILS
#ifdef MALARGUMENTDETAILS
		logadd("\"prereq\":%s],%s", prereq, prettify);
#else
		logadd("\"prereq\":%s]%s", prereq, prettify);
#endif
		
/* EXAMPLE MAL statement argument decomposition
 * The eventparser may assume this layout for ease of parsing
{
... as above ...
"result":{"clk":"173297139,"pc":1,"index":0,,"name":"X_6","type":"void","value":"0@0","eol":0}
...
"argument":{"clk":173297139,"pc":1,"index":"2","type":"str","value":"\"default_pipe\"","eol":0},
}
This information can be used to determine memory footprint and variable life times.
 */
#ifdef MALARGUMENTDETAILS
		// Also show details of the arguments for modelling
		if(mb){
			logadd("\"ret\":[");
			for( j=0; j< pci->argc; j++){
				int tpe = getVarType(mb, getArg(pci,j));
				str tname = 0, cv;
				lng total = 0;
				BUN cnt = 0;
				bat bid=0;
				str pret = ""; // or prettify
				int p = getPC(mb,pci);

				if( j == pci->retc ){
					logadd("],%s\"arg\":[",prettify);
				} 
				logadd("{");
				logadd("\"index\":\"%d\",%s", j,pret);
				logadd("\"name\":\"%s\",%s", getVarName(mb, getArg(pci,j)), pret);
				if( isaBatType(tpe) ){
					BAT *d= BATdescriptor( bid = abs(stk->stk[getArg(pci,j)].val.ival));
					tname = getTypeName(getColumnType(tpe));
					logadd("\"type\":\"bat[:%s]\",%s", tname,pret);
					if( d) {
						//if( isVIEW(d))
							//bid = abs(VIEWtparent(d));
						cnt = BATcount(d);
						total += cnt * d->T->width;
						total += heapinfo(d->T->vheap, abs(d->batCacheid)); 
						total += hashinfo(d->T->hash, abs(d->batCacheid)); 
						total += IMPSimprintsize(d);
						BBPunfix(d->batCacheid);
					} 
					logadd("\"bid\":\"%d\",%s", bid,pret);
					logadd("\"count\":\""BUNFMT"\",%s",cnt,pret);
					logadd("\"size\":" LLFMT",%s", total,pret);
				} else{
					tname = getTypeName(tpe);
					logadd("\"type\":\"%s\",%s", tname,pret);
					cv = 0;
					VALformat(&cv, &stk->stk[getArg(pci,j)]);
					stmtq = mal_quote(cv, strlen(cv));
					logadd("\"value\":\"%s\",%s", stmtq,pret);
					GDKfree(cv);
					GDKfree(stmtq);
				}
				logadd("\"eol\":%d%s", p == getEndOfLife(mb,getArg(pci,j)) , pret);
				GDKfree(tname);
				logadd("}%s%s", (j< pci->argc-1 && j != pci->retc -1?",":""), pret);
			}
			logadd("] %s",prettify); // end marker for arguments
		}
	}
#endif
	logadd("}\n"); // end marker
	logjsonInternal(logbuffer);
}

static int
getCPULoad(char cpuload[BUFSIZ]){
    int cpu, len = 0, i;
	lng user, nice, system, idle, iowait;
	size_t n;
    char buf[BUFSIZ+1], *s;
	static FILE *proc= NULL;
	lng newload;

	if ( proc == NULL || ferror(proc))
		proc = fopen("/proc/stat","r");
	else rewind(proc);
	if ( proc == NULL) {
		/* unexpected */
		return -1;
	}
	/* read complete file to avoid concurrent write issues */
	if ((n = fread(buf, 1, BUFSIZ,proc)) == 0 )
		return -1;
	buf[n] = 0;
	for ( s= buf; *s; s++) {
		if ( strncmp(s,"cpu",3)== 0){
			s +=3;
			if ( *s == ' ') {
				s++;
				cpu = 255; // the cpu totals stored here
			}  else {
				cpu = atoi(s);
				if (cpu < 0 || cpu > 255)
					cpu = 255;
			}
			s= strchr(s,' ');
			if (s == NULL)		/* unexpected format of file */
				break;

			while( *s && isspace((int)*s)) s++;
			i= sscanf(s,LLFMT" "LLFMT" "LLFMT" "LLFMT" "LLFMT,  &user, &nice, &system, &idle, &iowait);
			if ( i != 5 )
				goto skip;
			newload = (user - corestat[cpu].user + nice - corestat[cpu].nice + system - corestat[cpu].system);
			if (  newload)
				corestat[cpu].load = (double) newload / (newload + idle - corestat[cpu].idle + iowait - corestat[cpu].iowait);
			corestat[cpu].user = user;
			corestat[cpu].nice = nice;
			corestat[cpu].system = system;
			corestat[cpu].idle = idle;
			corestat[cpu].iowait = iowait;
		} 
	  skip:
		while (*s && *s != '\n')
			s++;
	}

	if( cpuload == 0)
		return 0;
	// identify core processing
	len += snprintf(cpuload, BUFSIZ, "[ ");
	for ( cpu = 0; cpuload && cpu < 255 && corestat[cpu].user; cpu++) {
		len +=snprintf(cpuload + len, BUFSIZ - len, "%c %.2f", (cpu?',':' '), corestat[cpu].load);
	}
	(void) snprintf(cpuload + len, BUFSIZ - len, "]");
	return 0;
}

void
profilerHeartbeatEvent(char *alter)
{
	char cpuload[BUFSIZ];
	char logbuffer[LOGLEN], *logbase;
	int loglen;
	char ctm[26];
	time_t clk;
	struct timeval clock;
	char *tbuf;

	if (ATOMIC_GET(hbdelay, mal_beatLock) == 0 || eventstream  == NULL)
		return;

	/* get CPU load on beat boundaries only */
	if ( getCPULoad(cpuload) )
		return;
	gettimeofday(&clock, NULL);
	clk = clock.tv_sec;
	
	lognew();
	logadd("{%s",prettify); // fill in later with the event counter
#ifdef HAVE_CTIME_R3
	tbuf = ctime_r(&clk, ctm, sizeof(ctm));
#else
#ifdef HAVE_CTIME_R
	tbuf = ctime_r(&clk, ctm);
#else
	tbuf = ctime(&clk);
#endif
#endif
	tbuf[19]=0;
	logadd("\"user\":\"heartbeat\",%s", prettify);
	logadd("\"ctime\":\"%s.%06ld\",%s",tbuf+11, (long)clock.tv_usec, prettify);
	logadd("\"rss\":"SZFMT ",%s", MT_getrss()/1024/1024, prettify);
#ifdef HAVE_SYS_RESOURCE_H
	getrusage(RUSAGE_SELF, &infoUsage);
	if(infoUsage.ru_inblock - prevUsage.ru_inblock)
		logadd("\"inblock\":%ld,%s", infoUsage.ru_inblock - prevUsage.ru_inblock, prettify);
	if(infoUsage.ru_oublock - prevUsage.ru_oublock)
		logadd("\"oublock\":%ld,%s", infoUsage.ru_oublock - prevUsage.ru_oublock, prettify);
	if(infoUsage.ru_majflt - prevUsage.ru_majflt)
		logadd("\"majflt\":%ld,%s", infoUsage.ru_majflt - prevUsage.ru_majflt, prettify);
	if(infoUsage.ru_nswap - prevUsage.ru_nswap)
		logadd("\"nswap\":%ld,%s", infoUsage.ru_nswap - prevUsage.ru_nswap, prettify);
	if(infoUsage.ru_nvcsw - prevUsage.ru_nvcsw)
		logadd("\"nvcsw\":%ld,%s", infoUsage.ru_nvcsw - prevUsage.ru_nvcsw +infoUsage.ru_nivcsw - prevUsage.ru_nivcsw, prettify);
	prevUsage = infoUsage;
#endif
	logadd("\"state\":\"%s\",%s",alter,prettify);
	logadd("\"cpuload\":\"%s\",%s",cpuload,prettify);
	logadd("}\n"); // end marker
	logjsonInternal(logbuffer);
}

void
profilerEvent(MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, int start, str usrname)
{
	if (stk == NULL) return;
	if (pci == NULL) return;
	if (getModuleId(pci) == myname) // ignore profiler commands from monitoring
		return;

	if( sqlProfiling && !start )
		cachedProfilerEvent(mb, stk, pci);
		
	if( eventstream) {
		renderProfilerEvent(mb, stk, pci, start, usrname);
		if ( start && pci->pc ==0)
			profilerHeartbeatEvent("ping");
		if ( !start && pci->token == ENDsymbol)
			profilerHeartbeatEvent("ping");
	}
}

/* The first scheme dumps the events
 * on a stream (and in the pool)
 * The mode encodes two flags: 
 * - showing all running instructions
 * - single line json
 */
#define PROFSHOWRUNNING	1
#define PROFSINGLELINE 2
str
openProfilerStream(stream *fd, int mode)
{
	int i,j;
	Client c;

#ifdef HAVE_SYS_RESOURCE_H
	getrusage(RUSAGE_SELF, &infoUsage);
	prevUsage = infoUsage;
#endif
	if (myname == 0){
		myname = putName("profiler");
		eventcounter = 0;
		logjsonInternal(monet_characteristics);
	}
	if( eventstream)
		closeProfilerStream();
	malProfileMode = -1;
	eventstream = fd;
	prettify = (mode & PROFSINGLELINE) ? " ": "\n";

	/* show all in progress instructions for stethoscope startup */
	if( (mode & PROFSHOWRUNNING) > 0){
		for (i = 0; i < MAL_MAXCLIENTS; i++) {
			c = mal_clients+i;
			if ( c->active ) 
				for(j = 0; j <THREADS; j++)
				if( c->inprogress[j].mb)
				/* show the event */
					profilerEvent(c->inprogress[j].mb, c->inprogress[j].stk, c->inprogress[j].pci, 1, c->username);
		}
	}
	return MAL_SUCCEED;
}

str
closeProfilerStream(void)
{
	eventstream = NULL;
	malProfileMode = 0;
	return MAL_SUCCEED;
}

/* the second scheme is to collect the profile
 * events in a local table for direct SQL inspection
 */
str
startProfiler(void)
{
#ifdef HAVE_SYS_RESOURCE_H
	getrusage(RUSAGE_SELF, &infoUsage);
	prevUsage = infoUsage;
#endif

	if( eventstream){
		throw(MAL,"profiler.start","Profiler already running, stream not available");
	}
	MT_lock_set(&mal_profileLock );
	if (myname == 0){
		myname = putName("profiler");
		eventcounter = 0;
	}
	malProfileMode = 1;
	MT_lock_unset(&mal_profileLock);
	logjsonInternal(monet_characteristics);
	// reset the trace table
	clearTrace();

	return MAL_SUCCEED;
}

/* SQL queries can be traced without obstructing the stream */
str
startTrace(void)
{
	malProfileMode = 1;
	sqlProfiling = TRUE;
	clearTrace();
	return MAL_SUCCEED;
}

str
stopTrace(void)
{
	malProfileMode = eventstream != NULL;
	sqlProfiling = FALSE;
	return MAL_SUCCEED;
}

str
stopProfiler(void)
{
	MT_lock_set(&mal_profileLock);
	malProfileMode = 0;
	setHeartbeat(0); // stop heartbeat
	closeProfilerStream();
	MT_lock_unset(&mal_profileLock);
	return MAL_SUCCEED;
}

/*
 * The resetProfiler is called when the owner of the event stream
 * leaves the scene. (Unclear if parallelism may cause errors)
 */
void
MPresetProfiler(stream *fdout)
{
	if (fdout != eventstream)
		return;
	if (mal_trace) // already traced on console
		return;
	MT_lock_set(&mal_profileLock);
	eventstream = 0;
	MT_lock_unset(&mal_profileLock);
}

/*
 * Offline tracing
 * The events being captured are stored in separate BATs.
 * They are made persistent to accumate information over
 * multiple sessions. This means it has to be explicitly reset
 * to avoid disc overflow using profiler.reset().
 *
 * All properties identified below are maintained, because this allows
 * for easy integration with SQL.
 */
static int TRACE_event = 0;
static BAT *TRACE_id_event = 0;
static BAT *TRACE_id_time = 0;
static BAT *TRACE_id_pc = 0;
static BAT *TRACE_id_thread = 0;
static BAT *TRACE_id_ticks = 0;
static BAT *TRACE_id_inblock = 0;
static BAT *TRACE_id_oublock = 0;
static BAT *TRACE_id_rssMB = 0;
static BAT *TRACE_id_tmpspace = 0;
static BAT *TRACE_id_minflt = 0;
static BAT *TRACE_id_majflt = 0;
static BAT *TRACE_id_nvcsw = 0;
static BAT *TRACE_id_stmt = 0;

int
TRACEtable(BAT **r)
{
	MT_lock_set(&mal_profileLock);
	if (TRACE_init == 0) {
		MT_lock_unset(&mal_profileLock);
		return -1;       /* not initialized */
	}
	r[0] = COLcopy(TRACE_id_event, TRACE_id_event->ttype, 0, TRANSIENT);
	r[1] = COLcopy(TRACE_id_time, TRACE_id_time->ttype, 0, TRANSIENT);
	r[2] = COLcopy(TRACE_id_pc, TRACE_id_pc->ttype, 0, TRANSIENT);
	r[3] = COLcopy(TRACE_id_thread, TRACE_id_thread->ttype, 0, TRANSIENT);
	r[4] = COLcopy(TRACE_id_ticks, TRACE_id_ticks->ttype, 0, TRANSIENT);
	r[5] = COLcopy(TRACE_id_rssMB, TRACE_id_rssMB->ttype, 0, TRANSIENT);
	r[6] = COLcopy(TRACE_id_tmpspace, TRACE_id_tmpspace->ttype, 0, TRANSIENT);
	r[7] = COLcopy(TRACE_id_inblock, TRACE_id_inblock->ttype, 0, TRANSIENT);
	r[8] = COLcopy(TRACE_id_oublock, TRACE_id_oublock->ttype, 0, TRANSIENT);
	r[9] = COLcopy(TRACE_id_minflt, TRACE_id_minflt->ttype, 0, TRANSIENT);
	r[10] = COLcopy(TRACE_id_majflt, TRACE_id_majflt->ttype, 0, TRANSIENT);
	r[11] = COLcopy(TRACE_id_nvcsw, TRACE_id_nvcsw->ttype, 0, TRANSIENT);
	r[12] = COLcopy(TRACE_id_stmt, TRACE_id_stmt->ttype, 0, TRANSIENT);
	MT_lock_unset(&mal_profileLock);
	return 13;
}

BAT *
getTrace(const char *nme)
{
	BAT *bn = NULL;

	MT_lock_set(&mal_profileLock);
	if (TRACE_init) {
		if (strcmp(nme, "event") == 0)
			bn = COLcopy(TRACE_id_event, TRACE_id_event->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "time") == 0)
			bn = COLcopy(TRACE_id_time, TRACE_id_time->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "pc") == 0)
			bn = COLcopy(TRACE_id_pc, TRACE_id_pc->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "thread") == 0)
			bn = COLcopy(TRACE_id_thread, TRACE_id_thread->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "usec") == 0)
			bn = COLcopy(TRACE_id_ticks, TRACE_id_ticks->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "rssMB") == 0)
			bn = COLcopy(TRACE_id_rssMB, TRACE_id_rssMB->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "tmpspace") == 0)
			bn = COLcopy(TRACE_id_tmpspace, TRACE_id_tmpspace->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "reads") == 0)
			bn = COLcopy(TRACE_id_inblock, TRACE_id_inblock->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "writes") == 0)
			bn = COLcopy(TRACE_id_oublock, TRACE_id_oublock->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "minflt") == 0)
			bn = COLcopy(TRACE_id_minflt, TRACE_id_minflt->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "majflt") == 0)
			bn = COLcopy(TRACE_id_majflt, TRACE_id_majflt->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "nvcsw") == 0)
			bn = COLcopy(TRACE_id_nvcsw, TRACE_id_nvcsw->ttype, 0, TRANSIENT);
		else if (strcmp(nme, "stmt") == 0)
			bn = COLcopy(TRACE_id_stmt, TRACE_id_stmt->ttype, 0, TRANSIENT);
	}
	MT_lock_unset(&mal_profileLock);
	return bn;
}

static BAT *
TRACEcreate(const char *hnme, const char *tnme, int tt)
{
	BAT *b;
	char buf[BUFSIZ];

	snprintf(buf, BUFSIZ, "trace_%s_%s", hnme, tnme);

	b = BATnew(TYPE_void, tt, 1 << 16, TRANSIENT);
	if (b == NULL)
		return NULL;
	BBPrename(b->batCacheid, buf);
	return b;
}


#define CLEANUPprofile(X)  if (X) { BBPunfix((X)->batCacheid); } (X) = NULL;

static void
_cleanupProfiler(void)
{
	CLEANUPprofile(TRACE_id_event);
	CLEANUPprofile(TRACE_id_time);
	CLEANUPprofile(TRACE_id_pc);
	CLEANUPprofile(TRACE_id_rssMB);
	CLEANUPprofile(TRACE_id_tmpspace);
	CLEANUPprofile(TRACE_id_inblock);
	CLEANUPprofile(TRACE_id_oublock);
	CLEANUPprofile(TRACE_id_minflt);
	CLEANUPprofile(TRACE_id_majflt);
	CLEANUPprofile(TRACE_id_nvcsw);
	CLEANUPprofile(TRACE_id_thread);
	CLEANUPprofile(TRACE_id_stmt);
	TRACE_init = 0;
}

int
initTrace(void)
{
	int ret = -1;

	MT_lock_set(&mal_profileLock);
	if (TRACE_init) {
		MT_lock_unset(&mal_profileLock);
		return 0;       /* already initialized */
	}
	TRACE_id_event = TRACEcreate("id", "event", TYPE_int);
	TRACE_id_time = TRACEcreate("id", "time", TYPE_str);
	// TODO split pc into its components fcn,pc,tag
	TRACE_id_pc = TRACEcreate("id", "pc", TYPE_str);
	TRACE_id_thread = TRACEcreate("id", "thread", TYPE_int);
	TRACE_id_ticks = TRACEcreate("id", "usec", TYPE_lng);
	TRACE_id_rssMB = TRACEcreate("id", "rssMB", TYPE_lng);
	// rename to size
	TRACE_id_tmpspace = TRACEcreate("id", "tmpspace", TYPE_lng);
	TRACE_id_inblock = TRACEcreate("id", "read", TYPE_lng);
	TRACE_id_oublock = TRACEcreate("id", "write", TYPE_lng);
	TRACE_id_minflt = TRACEcreate("id", "minflt", TYPE_lng);
	TRACE_id_majflt = TRACEcreate("id", "majflt", TYPE_lng);
	TRACE_id_nvcsw = TRACEcreate("id", "nvcsw", TYPE_lng);
	TRACE_id_stmt = TRACEcreate("id", "stmt", TYPE_str);
	if (TRACE_id_event == NULL ||
		TRACE_id_time == NULL ||
		TRACE_id_ticks == NULL ||
		TRACE_id_pc == NULL ||
		TRACE_id_stmt == NULL ||
		TRACE_id_rssMB == NULL ||
		TRACE_id_tmpspace == NULL ||
		TRACE_id_inblock == NULL ||
		TRACE_id_oublock == NULL ||
		TRACE_id_minflt == NULL ||
		TRACE_id_majflt == NULL ||
		TRACE_id_nvcsw == NULL ||
		TRACE_id_thread == NULL 
	)
		_cleanupProfiler();
	else
		TRACE_init = 1;
	ret = TRACE_init;
	MT_lock_unset(&mal_profileLock);
	return ret;
}

void
clearTrace(void)
{
	MT_lock_set(&mal_profileLock);
	if (TRACE_init == 0) {
		MT_lock_unset(&mal_profileLock);
		return;     /* not initialized */
	}
	/* drop all trace tables */
	_cleanupProfiler();
	TRACE_init = 0;
	MT_lock_unset(&mal_profileLock);
	initTrace();
}

str
cleanupTraces(void)
{
	clearTrace();
	return MAL_SUCCEED;
}

void
cachedProfilerEvent(MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	/* static struct Mallinfo prevMalloc; */
	char buf[BUFSIZ]= {0};
	char ctm[27]={0}, *ct= ctm+10;
	int tid = (int)THRgettid();
	lng v1 = 0, v2= 0, v3=0, v4=0, v5=0;
	str stmt, c;
	time_t clk;
	struct timeval clock;
	lng rssMB = MT_getrss()/1024/1024;
	lng tmpspace = pci->wbytes/1024/1024;
	int errors = 0;

#ifdef HAVE_TIMES
	struct tms newTms;
#endif

	/* struct Mallinfo infoMalloc; */
	gettimeofday(&clock, NULL);
	clk= clock.tv_sec;
#ifdef HAVE_TIMES
	times(&newTms);
#endif
	/* infoMalloc = MT_mallinfo(); */
#ifdef HAVE_SYS_RESOURCE_H
	getrusage(RUSAGE_SELF, &infoUsage);
#endif
	if (TRACE_init == 0)
		return;

	/* update the Trace tables */
	snprintf(buf, BUFSIZ, "%s.%s[%d]%d",
	getModuleId(getInstrPtr(mb, 0)),
	getFunctionId(getInstrPtr(mb, 0)), getPC(mb, pci), stk->tag);

#ifdef HAVE_CTIME_R3
	if (ctime_r(&clk, ctm, sizeof(ctm)) == NULL)
		strncpy(ctm, "", sizeof(ctm));
#else
#ifdef HAVE_CTIME_R
	if (ctime_r(&clk, ctm) == NULL)
		strncpy(ctm, "", sizeof(ctm));
#else
	{
		char *tbuf = ctime(&clk);
		strncpy(ctm, tbuf ? tbuf : "", sizeof(ctm));
	}
#endif
#endif
	/* sneakily overwrite year with second fraction */
	snprintf(ctm + 19, 6, ".%03d", (int)(clock.tv_usec / 1000));

	/* generate actual call statement */
	stmt = instruction2str(mb, stk, pci, LIST_MAL_ALL);
	c = stmt;

	while (c && *c && (isspace((int)*c) || *c == '!'))
		c++;

#ifdef HAVE_SYS_RESOURCE_H
	getrusage(RUSAGE_SELF, &infoUsage);
	v1= infoUsage.ru_inblock - prevUsage.ru_inblock;
	v2= infoUsage.ru_oublock - prevUsage.ru_oublock;
	v3= infoUsage.ru_majflt - prevUsage.ru_majflt;
	v4= infoUsage.ru_nswap - prevUsage.ru_nswap;
	v5= infoUsage.ru_nvcsw - prevUsage.ru_nvcsw +infoUsage.ru_nivcsw - prevUsage.ru_nivcsw;
	prevUsage = infoUsage;
#endif

	// keep it a short transaction
	MT_lock_set(&mal_profileLock);
 	if (TRACE_init == 0) {
		MT_lock_unset(&mal_profileLock);
		return;
	}
	errors += BUNappend(TRACE_id_event, &TRACE_event, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_time, ct, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_pc, buf, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_thread, &tid, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_ticks, &pci->ticks, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_rssMB, &rssMB, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_tmpspace, &tmpspace, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_inblock, &v1, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_oublock, &v2, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_minflt, &v3, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_majflt, &v4, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_nvcsw, &v5, FALSE) != GDK_SUCCEED;
	errors += BUNappend(TRACE_id_stmt, c, FALSE) != GDK_SUCCEED;
	TRACE_event++;
	eventcounter++;
	MT_lock_unset(&mal_profileLock);
	GDKfree(stmt);
}

lng
getDiskWrites(void)
{
#ifdef HAVE_SYS_RESOURCE_H
	struct rusage infoUsage;
	getrusage(RUSAGE_SELF, &infoUsage);
	return infoUsage.ru_oublock;
#else
	return 0;
#endif
}

lng
getDiskReads(void)
{
#ifdef HAVE_SYS_RESOURCE_H
	struct rusage infoUsage;
	getrusage(RUSAGE_SELF, &infoUsage);
	return infoUsage.ru_inblock;
#else
	return 0;
#endif
}

lng
getUserTime(void)
{
#ifdef HAVE_TIMES
	struct tms newTms;
	times(&newTms);
	return newTms.tms_utime;
#else
	return 0;
#endif
}

lng
getSystemTime(void)
{
#ifdef HAVE_TIMES
	struct tms newTms;
	times(&newTms);
	return newTms.tms_stime;
#else
	return 0;
#endif
}

lng
getDiskSpace(void)
{
	BAT *b;
	bat i;
	lng size = 0;

	for (i = 1; i < getBBPsize(); i++)
		if (BBP_logical(i) && (BBP_refs(i) || BBP_lrefs(i))) {
			b = BATdescriptor(i);
			if (b) {
				size += sizeof(BAT);
				if (!isVIEW(b)) {
					BUN cnt = BATcount(b);

					size += tailsize(b, cnt);
					/* the upperbound is used for the heaps */
					if (b->T->vheap)
						size += b->T->vheap->size;
					if (b->T->hash)
						size += sizeof(BUN) * cnt;
				}
				BBPunfix(i);
			}
		}
	return size;
}

//
// Retrieve the io statistics for the complete process group
// This information can only be obtained using root-permissions.
//
#ifdef GETIOSTAT
static str getIOactivity(void){
	Thread t,s;
	FILE *fd;
	char fnme[BUFSIZ], *buf;
	int n,i=0;
	size_t len=0;

	buf= GDKzalloc(BUFSIZ);
	if ( buf == NULL)
		return 0;
	buf[len++]='"';
	//MT_lock_set(&GDKthreadLock);
	for (t = GDKthreads, s = t + THREADS; t < s; t++, i++)
		if (t->pid ){
			(void) snprintf(fnme,BUFSIZ,"/proc/"SZFMT"/io",t->pid);
			fd = fopen(fnme,"r");
			if ( fd == NULL)
				return buf;
			(void) snprintf(buf+len, BUFSIZ-len-2,"thr %d ",i);
			if ((n = fread(buf+len, 1, BUFSIZ-len-2,fd)) == 0 )
				return  buf;
			// extract the properties
			mnstr_printf(GDKout,"#got io stat:%s\n",buf);
			(void)fclose (fd);
		 }
	//MT_lock_unset(&GDKthreadLock);
	buf[len++]='"';
	return buf;
}
#endif

void profilerGetCPUStat(lng *user, lng *nice, lng *sys, lng *idle, lng *iowait)
{
	(void) getCPULoad(0);
	*user = corestat[255].user;
	*nice = corestat[255].nice;
	*sys = corestat[255].system;
	*idle = corestat[255].idle;
	*iowait = corestat[255].iowait;
}

static MT_Id hbthread;
static volatile ATOMIC_TYPE hbrunning;

static void profilerHeartbeat(void *dummy)
{
	int t;
	const int timeout = GDKdebug & FORCEMITOMASK ? 10 : 25;

	(void) dummy;
	for (;;) {
		/* wait until you need this info */
		while (ATOMIC_GET(hbdelay, mal_beatLock) == 0 || eventstream == NULL) {
			if (GDKexiting() || !ATOMIC_GET(hbrunning, mal_beatLock))
				return;
			MT_sleep_ms(timeout);
		}
		for (t = (int) ATOMIC_GET(hbdelay, mal_beatLock); t > 0; t -= timeout) {
			if (GDKexiting() || !ATOMIC_GET(hbrunning, mal_beatLock))
				return;
			MT_sleep_ms(t > timeout ? timeout : t);
		}
		profilerHeartbeatEvent("ping");
	}
	ATOMIC_SET(hbdelay, 0, mal_beatLock);
}

void setHeartbeat(int delay)
{
	if (delay < 0 ){
		ATOMIC_SET(hbrunning, 0, mal_beatLock);
		if (hbthread)
			MT_join_thread(hbthread);
		return;
	}
	if ( delay > 0 &&  delay <= 10)
		delay = 10;
	ATOMIC_SET(hbdelay, (ATOMIC_TYPE) delay, mal_beatLock);
}

void initProfiler(void)
{
	if( mal_trace)
		openProfilerStream(mal_clients[0].fdout,0);
}

void initHeartbeat(void)
{
#ifdef NEED_MT_LOCK_INIT
	ATOMIC_INIT(mal_beatLock, "beatLock");
#endif
	ATOMIC_SET(hbrunning, 1, mal_beatLock);
	if (MT_create_thread(&hbthread, profilerHeartbeat, NULL, MT_THR_JOINABLE) < 0) {
		/* it didn't happen */
		hbthread = 0;
		ATOMIC_SET(hbrunning, 0, mal_beatLock);
	}
}

