/*
 * expCommand.c --
 *
 *	The bulk of the Expect commands, platform generic
 *
 * Unix version written by: Don Libes, NIST, 2/6/90
 *
 * Design and implementation of this program was paid for by U.S. tax
 * dollars.  Therefore it is public domain.  However, the author and NIST
 * would appreciate credit if this program or parts of it are used.
 *
 *
 * Windows NT port by Gordon Chaffee
 * Copyright (c) 1997 by Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include <math.h>
#include "exp_port.h"
#include "tclInt.h"
#include "tclPort.h"
#include "expect_tcl.h"
#include "exp_command.h"
#include "exp_rename.h"
#include "exp_log.h"
#include "exp_event.h"
#include "exp_prog.h"
#include "exp_tty.h"

/* Tcl needs commands in writable space (or at least used to) */
static char close_cmd[] = "close";

/*
 * exp_configure_count is incremented whenever a spawned process is closed
 * or an indirect list is modified.  This forces any (stack of) expect or
 * interact commands to reexamine the state of the world and adjust
 * accordingly.
 */
int exp_configure_count = 0;

/* this message is required because fopen sometimes fails to set errno */
/* Apparently, it "does the user a favor" and doesn't even call open */
/* if the file name is bizarre enough.  This means we can't handle fopen */
/* with the obvious trivial logic. */
static char *open_failed = "could not open - odd file name?";

/*
 * expect_key is just a source for generating a unique stamp.  As each
 * expect/interact command begins, it generates a new key and marks all
 * the spawn ids of interest with it.  Then, if someone comes along and
 * marks them with yet a newer key, the old command will recognize this
 * reexamine the state of the spawned process.
 */
int expect_key = 0;

/*
 * The table is used to map channels to exp_f structures.
 */
Tcl_HashTable *exp_f_table = NULL;

/*
 * The 'exp_any' spawn identifier
 */
struct exp_f *exp_f_any = NULL;

static void		tcl_tracer _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int level, char *command,
			    Tcl_CmdProc *cmdProc, ClientData cmdClientData,
			    int argc, char *argv[]));
static void		exp_i_add_f _ANSI_ARGS_((struct exp_i *,
			    struct exp_f *fs));
static void		exp_f_closed _ANSI_ARGS_((struct exp_f *));


/*
 *----------------------------------------------------------------------
 *
 * exp_error --
 *
 *	Formats an error message into the interp.  Do not terminate
 *	format strings with \n!!!.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	An error message is written into interp->result
 *
 *----------------------------------------------------------------------
 */

void
exp_error TCL_VARARGS_DEF(Tcl_Interp *,arg1)
{
    Tcl_Interp *interp;
    char *fmt;
    va_list args;

    interp = TCL_VARARGS_START(Tcl_Interp *,arg1,args);
    fmt = va_arg(args,char *);
    vsprintf(interp->result,fmt,args);
    va_end(args);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_wait_zero --
 *
 *	Zero out the wait status field
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */
static void
exp_wait_zero(status)
    WAIT_STATUS_TYPE *status;
{
    int i;

    for (i=0;i<sizeof(WAIT_STATUS_TYPE);i++) {
	((char *)status)[i] = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * exp_fcheck --
 *
 *	Check and possibly adjust the status of the exp_f.
 *
 * Results:
 *	0 if not valid, 1 if good.
 *
 * Side Effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
exp_fcheck(interp,f,opened,adjust,msg)
    Tcl_Interp *interp;
    struct exp_f *f;
    int opened;		/* check not closed */
    int adjust;		/* adjust buffer sizes */
    char *msg;
{
    if ((!opened) || (!f->user_closed)) {
	if (adjust) {
	    exp_adjust(f);
	}
	return 1;
    }

    exp_error(interp,"%s: invalid spawn id (%s)", msg, f->spawnId);
    return(0);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_chan2f --
 *
 *	For a given channel name, returns the exp_f structure
 *	for that channel.
 *
 * Results:
 *	An exp_f structure if found and usable, NULL if not.
 *
 * Side Effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

struct exp_f *
exp_chan2f(interp,chan,opened,adjust,msg)
    Tcl_Interp *interp;
    char *chan;			/* Channel name */
    int opened;			/* check not closed */
    int adjust;			/* adjust buffer sizes */
    char *msg;
{
    Tcl_HashEntry *hPtr;
    struct exp_f *f;

    hPtr = Tcl_FindHashEntry(exp_f_table, chan);
    if (hPtr != NULL) {
	f = (struct exp_f *) Tcl_GetHashValue(hPtr);
	if ((!opened) || !f->user_closed) {
	    if (adjust) {
		exp_adjust(f);
	    }
	    return f;
	}
    }
    exp_error(interp,"%s: invalid spawn id (%s)",msg,chan);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_close --
 *
 *	Close a connection.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A native file handle is closed
 *
 *----------------------------------------------------------------------
 */

int
exp_close(interp, f)
    Tcl_Interp *interp;
    struct exp_f *f;
{
    /*
     * Check if this is an id that should never be deleted
     */
    if (f->alwaysopen) {
	exp_error(interp, "cannot close permanent id %s", f->spawnId);
	return TCL_ERROR;
    }

    f->user_closed = TRUE;

    if (! f->leaveopen) {
	/*
	 * Tcl_UnregisterChannel() will call Tcl_Close() if needed
	 */
	if (f->channel) {
	    return Tcl_UnregisterChannel(interp, f->channel);
	}
	return TCL_OK;
    } else {
	if (--f->leaveopen >= 0) {
	    return TCL_OK;
	}
	if (f->channel) {
	    Tcl_DeleteCloseHandler(f->channel, (Tcl_CloseProc *) exp_f_closed,
				   (ClientData) f);
	}
	exp_f_closed(f);
    }

    return(TCL_OK);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_f_find --
 *
 *	Try to find an existing exp_f structure in the spawn id table.
 *
 * Results:
 *	The structure if found, NULL if not.
 *
 *----------------------------------------------------------------------
 */

struct exp_f *
exp_f_find(interp,spawnId)
    Tcl_Interp *interp;
    char *spawnId;
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(exp_f_table, spawnId);
    if (! hPtr) {
	return NULL;
    }
    return (struct exp_f *) Tcl_GetHashValue(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_f_new --
 *
 *	Create a new exp_f structure for a given channel and enter
 *	it into the spawn id hash table.  If spawnId is given, it
 *	is entered under that name.  If not, it is entered under
 *	the channel identifier.  This means that either a channel
 *	and/or a spawnId must be passed in.
 *
 * Results:
 *	The new structure if successful, NULL if not.
 *
 *----------------------------------------------------------------------
 */

struct exp_f *
exp_f_new(interp,chan,spawnId,pid)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    char *spawnId;
    int pid;
{
    struct exp_f *f;
    Tcl_HashEntry *hPtr;
    int new;

    if (!chan && !spawnId) {
	return NULL;
    }

    spawnId = spawnId ? spawnId : Tcl_GetChannelName(chan);
    hPtr = Tcl_CreateHashEntry(exp_f_table, spawnId, &new);
    if (!new) {
	panic("Exp_SpawnCmd: old entry found in table");
	f = (struct exp_f *) Tcl_GetHashValue(hPtr);
	return f;
    }

    f = (struct exp_f *) ckalloc(sizeof(struct exp_f));
    f->interp = interp;
    f->pid = pid;
    f->size = 0;
    f->msize = 0;
    f->buffer = 0;
    f->printed = 0;
    f->echoed = 0;
    f->rm_nulls = exp_default_rm_nulls;
    f->parity = exp_default_parity;
    f->key = expect_key++;
    f->force_read = FALSE;
    f->fg_armed = FALSE;
    f->umsize = exp_default_match_max;
    f->valid = TRUE;
    f->user_closed = FALSE;
    f->user_waited = (EXP_NOPID == pid) ? TRUE : FALSE;
    f->sys_waited = (EXP_NOPID == pid) ? TRUE : FALSE;
    if (f->sys_waited) {
	exp_wait_zero(&f->wait);
    }
    f->bg_interp = 0;
    f->bg_status = unarmed;
    f->bg_ecount = 0;
    f->channel = chan;
    f->leaveopen = 0;
    f->alwaysopen = 0;
    f->matched = 0;		/* Used only by expectlib */
    f->Master = NULL;
    f->event_proc = NULL;
    f->event_data = 0;
    exp_f_new_platform(f);

    Tcl_SetHashValue(hPtr, f);
    f->hashPtr = hPtr;
    f->spawnId = ckalloc(strlen(spawnId) + 1);
    strcpy(f->spawnId, spawnId);

    if (chan) {
	Tcl_CreateCloseHandler(chan, (Tcl_CloseProc *) exp_f_closed,
			       (ClientData) f);
    }

    return f;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_f_free --
 *
 *	Frees an exp_f structure.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
exp_f_free(f)
    struct exp_f *f;
{
    if (f->buffer) {
	ckfree(f->buffer);
	f->buffer = 0;
	f->msize = 0;
	f->size = 0;
	f->printed = 0;
	f->echoed = 0;
	if (f->fg_armed) {
	    exp_event_disarm(f);
	    f->fg_armed = FALSE;
	}
	ckfree(f->lower);
    }
    ckfree(f->spawnId);
    f->fg_armed = FALSE;
    Tcl_DeleteHashEntry(f->hashPtr);

    exp_f_free_platform(f);
    ckfree((char *) f);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_f_closed --
 *
 *	A channel was closed, so we need to make it unavailable
 *	for anything except the wait command.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
exp_f_closed(f)
    struct exp_f *f;
{
    exp_ecmd_remove_f_direct_and_indirect(f->interp,f);

    exp_configure_count++;

    f->fg_armed = FALSE;
    f->valid = FALSE;

    if (f->user_waited) {
	exp_f_free(f);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_ExpPidCmd --
 *
 *	Implements the "exp_pid" command
 *
 * Results:
 *	A standard Tcl result
 *
 * Side Effects:
 *	None
 *
 * Notes:
 *	OS independent
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_ExpPidCmd(clientData,interp,argc,argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    struct exp_f *f;
    char *chanId = NULL;
    char *argv0 = argv[0];
    
    argc--; argv++;
    
    for (;argc>0;argc--,argv++) {
	if (streq(*argv,"-i")) {
	    argc--; argv++;
	    if (!*argv) goto usage;
	    chanId = *argv;
	} else goto usage;
    }
    
    if (chanId == NULL) {
	f = exp_update_master(interp,0,0);
    } else {
	f = exp_chan2f(interp, chanId, 1, 0, argv0);
    }
    if (f == NULL) {
	return(TCL_ERROR);
    }

    sprintf(interp->result,"%d",f->pid);
    return TCL_OK;
 usage:
    exp_error(interp,"usage: -i spawn_id");
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_GetpidDeprecatedCmd --
 *
 *	Implements the old 'getpid' command.  This command is has
 *	been deprecated and may not be supported in the future
 *
 * Results:
 *	A standard Tcl result
 *
 * Side Effects:
 *	None
 *
 * Notes:
 *	OS independent
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_GetpidDeprecatedCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    debuglog("getpid is deprecated, use pid\r\n");
    sprintf(interp->result,"%d",exp_getpidproc());
    return(TCL_OK);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_update_master --
 *
 *	Get the current master (via out-parameter)
 *
 * Results:
 *	An exp_f structure or NULL if not found
 *	
 * Note: Since exp_chan2f calls tcl_error, this may be
 *	immediately followed by a "return(TCL_ERROR)"!!!
 *
 * Notes:
 *	OS independent
 *
 *----------------------------------------------------------------------
 */

struct exp_f *
exp_update_master(interp,opened,adjust)
    Tcl_Interp *interp;
    int opened;
    int adjust;
{
    char *s = exp_get_var(interp,EXP_SPAWN_ID_VARNAME);
    if (s == NULL) {
	s = EXP_SPAWN_ID_USER;
    }
    return exp_chan2f(interp,s,opened,adjust,s);
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_SleepCmd --
 *
 *	Implements the 'sleep' (alias 'exp_sleep') command.
 *	Can sleep for fractional seconds.
 *
 * Results:
 *	A standard Tcl result
 *
 * Side Effects:
 *	May not return immediately, and it may service other
 *	events during the sleep period
 *	
 * Notes:
 *	OS independent
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_SleepCmd(clientData,interp,argc,argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    argc--; argv++;

    if (argc != 1) {
	exp_error(interp,"must have one arg: seconds");
	return TCL_ERROR;
    }

    return(exp_dsleep(interp,(double)atof(*argv)));
}

struct slow_arg {
    int size;
    double time;
};

/*
 *----------------------------------------------------------------------
 *
 * get_slow_args --
 *
 *	Get the arguments the the 'send -s' command
 *
 * Results:
 *	0 on success, -1 on failure
 *	
 * Side Effects:
 *	The slow_arg structure is filled in
 *
 *----------------------------------------------------------------------
 */

/* returns 0 for success, -1 for failure */
static int
get_slow_args(interp,x)
    Tcl_Interp *interp;
    struct slow_arg *x;
{
    int sc;			/* return from scanf */
    char *s = exp_get_var(interp,"send_slow");
    if (!s) {
	exp_error(interp,"send -s: send_slow has no value");
	return(-1);
    }
    if (2 != (sc = sscanf(s,"%d %lf",&x->size,&x->time))) {
	exp_error(interp,"send -s: found %d value(s) in send_slow but need 2",sc);
	return(-1);
    }
    if (x->size <= 0) {
	exp_error(interp,"send -s: size (%d) in send_slow must be positive", x->size);
	return(-1);
    }
    if (x->time <= 0) {
	exp_error(interp,"send -s: time (%f) in send_slow must be larger",x->time);
	return(-1);
    }
    return(0);
}

/*
 *----------------------------------------------------------------------
 *
 * slow_write --
 *
 *	Write some bytes   s  l  o  w  l  y
 *
 * Results:
 *	0 on success, -1 on failure, positive for standard Tcl result
 *	
 * Side Effects:
 *	Data is written to an output object
 *
 * Notes:
 *	OS independent
 *
 *----------------------------------------------------------------------
 */

static int
slow_write(interp,f,buffer,rembytes,arg)
    Tcl_Interp *interp;
    struct exp_f *f;
    char *buffer;
    int rembytes;
    struct slow_arg *arg;
{
    int rc;

    while (rembytes > 0) {
	int len;

	len = (arg->size<rembytes?arg->size:rembytes);
	if (0 > exp_exact_write(f,buffer,len)) return(-1);
	rembytes -= arg->size;
	buffer += arg->size;

	/* skip sleep after last write */
	if (rembytes > 0) {
	    rc = exp_dsleep(interp,arg->time);
	    if (rc>0) return rc;
	}
    }
    return(0);
}

struct human_arg {
    float alpha;		/* average interarrival time in seconds */
    float alpha_eow;		/* as above but for eow transitions */
    float c;			/* shape */
    float min, max;
};


/*
 *----------------------------------------------------------------------
 *
 * get_human_args --
 *
 *	Get the arguments the the 'send -h' command
 *
 * Results:
 *	0 on success, -1 on failure
 *	
 * Side Effects:
 *	The human_arg structure is filled in
 *
 * Notes:
 *	OS independent
 *
 *----------------------------------------------------------------------
 */

static int
get_human_args(interp,x)
    Tcl_Interp *interp;
    struct human_arg *x;
{
    int sc;			/* return from scanf */
    char *s = exp_get_var(interp,"send_human");

    if (!s) {
	exp_error(interp,"send -h: send_human has no value");
	return(-1);
    }
    if (5 != (sc = sscanf(s,"%f %f %f %f %f",
			  &x->alpha,&x->alpha_eow,&x->c,&x->min,&x->max))) {
	if (sc == EOF) sc = 0;	/* make up for overloaded return */
	exp_error(interp,"send -h: found %d value(s) in send_human but need 5",sc);
	return(-1);
    }
    if (x->alpha < 0 || x->alpha_eow < 0) {
	exp_error(interp,"send -h: average interarrival times (%f %f) must be non-negative in send_human", x->alpha,x->alpha_eow);
	return(-1);
    }
    if (x->c <= 0) {
	exp_error(interp,"send -h: variability (%f) in send_human must be positive",x->c);
	return(-1);
    }
    x->c = 1/x->c;

    if (x->min < 0) {
	exp_error(interp,"send -h: minimum (%f) in send_human must be non-negative",x->min);
	return(-1);
    }
    if (x->max < 0) {
	exp_error(interp,"send -h: maximum (%f) in send_human must be non-negative",x->max);
	return(-1);
    }
    if (x->max < x->min) {
	exp_error(interp,"send -h: maximum (%f) must be >= minimum (%f) in send_human",x->max,x->min);
	return(-1);
    }
    return(0);
}

/*
 *----------------------------------------------------------------------
 *
 * unit_random --
 *
 *	Compute random numbers from 0 to 1, for expect's send -h
 *	This implementation sacrifices beauty for portability.
 *	Current implementation is pathetic but works
 *
 * Results:
 *	A floating point number between 0 and 1
 *
 * Side Effects:
 *	None
 *
 * Notes:
 *	OS independent
 *
 *----------------------------------------------------------------------
 */

static float
unit_random()
{
    /* 99991 is largest prime in my CRC - can't hurt, eh? */
    return((float)(1+(rand()%99991))/(float) 99991.0);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_init_unit_random --
 *
 *	Initialize the random number generator
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	None
 *
 * Notes:
 *	OS independent
 *
 *----------------------------------------------------------------------
 */

void
exp_init_unit_random()
{
    srand(exp_getpidproc());
}

/*
 *----------------------------------------------------------------------
 *
 * exp_init_unit_random --
 *
 *	This function is my implementation of the Weibull distribution.
 *	I've added a max time and an "alpha_eow" that captures the slight
 *	but noticable change in human typists when hitting end-of-word
 *	transitions.
 *
 * Results:
 *	0 for success, -1 for failure, positive for standard Tcl result
 *
 * Side Effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static int
human_write(interp,f,buffer,arg)
    Tcl_Interp *interp;
    struct exp_f *f;
    char *buffer;
    struct human_arg *arg;
{
    char *sp;
    float t;
    float alpha;
    int in_word = TRUE;
    int wc;

    debuglog("human_write: avg_arr=%f/%f  1/shape=%f  min=%f  max=%f\r\n",
	     arg->alpha,arg->alpha_eow,arg->c,arg->min,arg->max);

    for (sp = buffer;*sp;sp++) {
	/* use the end-of-word alpha at eow transitions */
	if (in_word && (ispunct(*sp) || isspace(*sp)))
	    alpha = arg->alpha_eow;
	else alpha = arg->alpha;
	in_word = !(ispunct(*sp) || isspace(*sp));

	t = alpha * (float) pow(-log((double)unit_random()),arg->c);

	/* enforce min and max times */
	if (t<arg->min) t = arg->min;
	else if (t>arg->max) t = arg->max;

	/*fprintf(stderr,"\nwriting <%c> but first sleep %f seconds\n",*sp,t);*/
	/* skip sleep before writing first character */
	if (sp != buffer) {
	    wc = exp_dsleep(interp,(double)t);
	    if (wc > 0) return wc;
	}

	wc = exp_exact_write(f, sp, 1);
	if (wc == -1) {
	    return -1;
	}
    }
    return(0);
}

struct exp_i *exp_i_pool = 0;
struct exp_fs_list *exp_fs_list_pool = 0;

#define EXP_I_INIT_COUNT	10
#define EXP_FS_INIT_COUNT	10

struct exp_i *
exp_new_i()
{
    int n;
    struct exp_i *i;

    if (!exp_i_pool) {
	/* none avail, generate some new ones */
	exp_i_pool = i = (struct exp_i *)ckalloc(
						 EXP_I_INIT_COUNT * sizeof(struct exp_i));
	for (n=0;n<EXP_I_INIT_COUNT-1;n++,i++) {
	    i->next = i+1;
	}
	i->next = 0;
    }

    /* now that we've made some, unlink one and give to user */

    i = exp_i_pool;
    exp_i_pool = exp_i_pool->next;
    i->value = 0;
    i->variable = 0;
    i->fs_list = 0;
    i->ecount = 0;
    i->next = 0;
    return i;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_new_fs --
 *
 *	Get a new exp_f structure.
 *
 * Results:
 *	A structure pointer if successful.
 *
 * Side Effects:
 *	Removes one from the pool if there are any in the pool.
 *	If none are in pool, more are allocated for the pool.
 *
 *----------------------------------------------------------------------
 */

struct exp_fs_list *
exp_new_fs(f)
    struct exp_f *f;
{
    int n;
    struct exp_fs_list *fs;

    if (!exp_fs_list_pool) {
	exp_fs_list_pool = fs = (struct exp_fs_list *)
	    ckalloc(EXP_FS_INIT_COUNT * sizeof(struct exp_fs_list));
	for (n=0;n<EXP_FS_INIT_COUNT-1;n++,fs++) {
	    fs->next = fs+1;
	}
	fs->next = 0;
    }

    fs = exp_fs_list_pool;
    exp_fs_list_pool = exp_fs_list_pool->next;
    fs->f = f;
    return fs;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_free_fs --
 *
 *	Add an exp_f structure list to the free pool.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
exp_free_fs(fs_first)
    struct exp_fs_list *fs_first;
{
    struct exp_fs_list *fs, *penultimate;

    if (!fs_first) return;

    /*
     * link entire chain back in at once by first finding last pointer
     * making that point back to pool, and then resetting pool to this
     */

    /* run to end */
    for (fs = fs_first;fs;fs=fs->next) {
	penultimate = fs;
    }
    penultimate->next = exp_fs_list_pool;
    exp_fs_list_pool = fs_first;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_free_fs_single --
 *
 *	Remove an exp_f structure from the list and put it back
 *	in the free pool
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
exp_free_fs_single(fs)
    struct exp_fs_list *fs;
{
    fs->next = exp_fs_list_pool;
    exp_fs_list_pool = fs;
}

void
exp_free_i(interp,i,updateproc)
    Tcl_Interp *interp;
    struct exp_i *i;
    Tcl_VarTraceProc *updateproc; /* proc to invoke if indirect is written */
{
    if (i->next) exp_free_i(interp,i->next,updateproc);

    exp_free_fs(i->fs_list);

    if (i->direct == EXP_INDIRECT) {
	Tcl_UntraceVar(interp,i->variable,
		       TCL_GLOBAL_ONLY|TCL_TRACE_WRITES,
		       updateproc,(ClientData)i);
    }

    /*
     * here's the long form
     * if duration & direct	free(var)  free(val)
     * PERM	  DIR	    		1
     * PERM	  INDIR	    1		1
     * TMP	  DIR
     * TMP	  INDIR			1
     * Also if i->variable was a bogus variable name, i->value might not be
     * set, so test i->value to protect this
     * TMP in this case does NOT mean from the "expect" command.  Rather
     * it means "an implicit spawn id from any expect or expect_XXX
     *  command".  In other words, there was no variable name provided.
     */
    if (i->value
	&& (((i->direct == EXP_DIRECT) && (i->duration == EXP_PERMANENT))
	    || ((i->direct == EXP_INDIRECT) && (i->duration == EXP_TEMPORARY))))
    {
	ckfree(i->value);
    } else if (i->duration == EXP_PERMANENT) {
	if (i->value) ckfree(i->value);
	if (i->variable) ckfree(i->variable);
    }

    i->next = exp_i_pool;
    exp_i_pool = i;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_new_i_complex --
 *
 *	Generate a descriptor for a "-i" flag.  This tries to
 *	be backward compatible, but it can't be perfect.  If
 *	a channel exists, it assumes that the channel is what
 *	was intended.  If it doesn't, it checks the identifier
 *	as a variable.  If the variable exists, the it uses
 *	the variable name as an indirect pointer to the channel
 *	to use.  If the variable doesn't exist, it falls back
 *	to assuming the identifier is a channel identifier.
 *
 * Results:
 *	A new descriptor structure.  Cannot fail currently.
 *
 * Side Effects:
 *	Memory is allocated and a Tcl variable trace may be setup
 *
 *----------------------------------------------------------------------
 */
 
struct exp_i *
exp_new_i_complex(interp,arg,duration,updateproc,msg)
    Tcl_Interp *interp;
    char *arg;		/* spawn id list or a variable containing a list */
    int duration;	/* if we have to copy the args */
    /* should only need do this in expect_before/after */
    Tcl_VarTraceProc *updateproc;	/* proc to invoke if indirect is written */
    char *msg;		/* Error message identifier */
{
    struct exp_i *i;
    char **stringp;
    Tcl_DString dString;
    
    if (!exp_chan2f(interp, arg, 1, 0, msg)) {
	Tcl_DStringInit(&dString);
	Tcl_DStringGetResult(interp, &dString);
	if (Tcl_GetVar(interp, arg, 0)) {
	    Tcl_DStringFree(&dString);
	    i = exp_new_i();
	    i->direct = EXP_INDIRECT;
	    stringp = &i->variable;
	} else {
	    Tcl_DStringResult(interp, &dString);
	    Tcl_DStringFree(&dString);
	    return NULL;
	}
    } else {
	i = exp_new_i();
	i->direct = EXP_DIRECT;
	stringp = &i->value;
    }

    i->duration = duration;
    if (duration == EXP_PERMANENT) {
	*stringp = ckalloc(strlen(arg)+1);
	strcpy(*stringp,arg);
    } else {
	*stringp = arg;
    }
    
    i->fs_list = 0;
    exp_i_update(interp,i);
    
    /* if indirect, ask Tcl to tell us when variable is modified */
    
    if (i->direct == EXP_INDIRECT) {
	Tcl_TraceVar(interp, i->variable, TCL_GLOBAL_ONLY|TCL_TRACE_WRITES,
		     updateproc, (ClientData) i);
    }
    
    return i;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_i_add_f --
 *
 *	Add to a list of descriptors
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A list grows
 *
 *----------------------------------------------------------------------
 */

static void
exp_i_add_f(i,f)
    struct exp_i *i;
    struct exp_f *f;
{
    struct exp_fs_list *new_fs;

    new_fs = exp_new_fs(f);
    new_fs->next = i->fs_list;
    i->fs_list = new_fs;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_i_parse_channels
 *
 *	Parses a string containing a list of channel identifiers and
 *	adds the resulting exp_f structures to a list.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A list grows
 *
 *----------------------------------------------------------------------
 */
 
void
exp_i_parse_channels(interp, i)
    Tcl_Interp *interp;
    struct exp_i *i;
{
    char *p = i->value;
    char *b;
    char s;
    struct exp_f *f;

    while (1) {
	while (isspace(*p)) {
	    p++;
	}
	if (*p == 0) break;
	b = p;
	while (*p && !isspace(*p)) {
	    p++;
	}
	s = *p;
	*p = 0;

	f = exp_chan2f(interp, b, 1, 0, "");
	if (f) {
	    exp_i_add_f(i,f);
	}

	*p = s;
    }
}
	
/*
 *----------------------------------------------------------------------
 *
 * exp_i_update --
 *
 *	updates a single exp_i struct
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
exp_i_update(interp,i)
    Tcl_Interp *interp;
    struct exp_i *i;
{
    char *p;			/* string representation of list of spawn ids*/

    if (i->direct == EXP_INDIRECT) {
	p = Tcl_GetVar(interp,i->variable,TCL_GLOBAL_ONLY);
	if (!p) {
	    p = "";
	    exp_debuglog("warning: indirect variable %s undefined",i->variable);
	}

	if (i->value) {
	    if (streq(p,i->value)) return;

	    /* replace new value with old */
	    ckfree(i->value);
	}
	i->value = ckalloc(strlen(p)+1);
	strcpy(i->value,p);

	exp_free_fs(i->fs_list);
	i->fs_list = 0;
    } else {
	/* no free, because this should only be called on */
	/* "direct" i's once */
	i->fs_list = 0;
    }
    exp_i_parse_channels(interp, i);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_new_i_simple --
 *
 *	Not quite sure what this does (GCC)
 *
 * Results:
 *	An exp_i structure
 *
 *----------------------------------------------------------------------
 */

struct exp_i *
exp_new_i_simple(f,duration)
    struct exp_f *f;
    int duration;		/* if we have to copy the args */
    /* should only need do this in expect_before/after */
{
    struct exp_i *i;

    i = exp_new_i();

    i->direct = EXP_DIRECT;
    i->duration = duration;

    exp_i_add_f(i,f);

    return i;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_exact_write --
 *
 *	Write exactly this many bytes, i.e. retry partial writes.
 *
 * Results:
 *	0 on success, -1 on failure
 *	
 * Side Effects:
 *	Data is written to an output object
 *
 *----------------------------------------------------------------------
 */

int
exp_exact_write(f,buffer,rembytes)
    struct exp_f *f;
    char *buffer;
    int rembytes;
{
    int n;
    while (rembytes) {
	n = Tcl_Write(f->channel, buffer, rembytes);
	if (-1 == n) {
	    return -1;
	}
	if (0 == n) {
	    Tcl_Sleep(1000);
	    exp_debuglog("write() failed to write anything but returned - sleeping and retrying...\n");
	}
	buffer += n;
	rembytes -= n;
    }
    return(0);
}

/*
 *----------------------------------------------------------------------
 *
 * ExpSpawnOpen --
 *
 *	Handle the 'spawn -open' command.  Called from Exp_SpawnCmd.
 *
 * Results:
 *	A standard Tcl result
 *
 *----------------------------------------------------------------------
 */

int
ExpSpawnOpen(interp, chanId, leaveopen)
    Tcl_Interp *interp;
    char *chanId;
    int leaveopen;
{
    Tcl_Channel chan;
    int mode;
    struct exp_f *f;
    
    if (!(chan = Tcl_GetChannel(interp, chanId, &mode))) {
	return TCL_ERROR;
    }
    if (!mode) {
	exp_error(interp,"%s: channel is neither readable nor writable",
		  chanId);
	return TCL_ERROR;
    }

    f = exp_f_find(interp, chanId);
    if (! f) {
	f = exp_f_new(interp, chan, NULL, EXP_NOPID);
	f->leaveopen = leaveopen;

	exp_wait_zero(&f->wait);
    } else {
	/*
	 * Reference count this thing
	 */

	f->leaveopen += leaveopen;
    }

    /* tell user id of new process */
    Tcl_SetVar(interp,EXP_SPAWN_ID_VARNAME,chanId,0);

    sprintf(interp->result,"%d",EXP_NOPID);
    debuglog("spawn: returns {%s}\r\n",interp->result);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_SendLogCmd --
 *
 *	Implements the send_log command.
 *
 * Results:
 *	A standard Tcl result
 *
 * Side Effects:
 *	Messages are written to a log file
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_SendLogCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char *string;
    int len;

    argv++;
    argc--;

    if (argc) {
	if (streq(*argv,"--")) {
	    argc--; argv++;
	}
    }

    if (argc != 1) {
	exp_error(interp,"usage: send [args] string");
	return TCL_ERROR;
    }

    string = *argv;

    len = strlen(string);

    if (debugfile) Tcl_Write(debugfile, string, len);
    if (logfile) Tcl_Write(logfile, string, len);

    return(TCL_OK);
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_SendCmd --
 *
 *	Sends data to a subprocess or over some channel
 *
 * Results:
 *	Standard Tcl result
 *
 * Notes:
 *	(Don) I've rewritten this to be unbuffered.  I did this so you
 *	could shove large files through "send".  If you are concerned
 *	about efficiency, you should quote all your send args to make
 *	them one single argument.
 *
 *	(GCC) This uses Tcl channels.  By default, the channel
 *	translation will be binary for channels created with
 *	spawn <program>.  The clientData argument, if non-NULL,
 *	holds the name of the channel to use.
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_SendCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int rc;			/* final result of this procedure */
    struct human_arg human_args;
    struct slow_arg slow_args;
#define SEND_STYLE_STRING_MASK	0x07 /* mask to detect a real string arg */
#define SEND_STYLE_PLAIN	0x01
#define SEND_STYLE_HUMAN	0x02
#define SEND_STYLE_SLOW		0x04
#define SEND_STYLE_ZERO		0x10
#define SEND_STYLE_BREAK	0x20
    int send_style = SEND_STYLE_PLAIN;
    int want_cooked = TRUE;
    char *string;		/* string to send */
    int len;			/* length of string to send */
    int zeros;			/* count of how many ascii zeros to send */
    
    char *i_masters = 0;
    struct exp_fs_list *fs;
    struct exp_i *i;
    char *arg;
    struct exp_f *f = NULL;
    char *argv0 = argv[0];
    
    argv++;
    argc--;
    while (argc) {
	arg = *argv;
	if (arg[0] != '-') break;
	arg++;
	if (exp_flageq1('-',arg)) { /* "--" */
	    argc--; argv++;
	    break;
	} else if (exp_flageq1('i',arg)) { /* "-i" */
	    argc--; argv++;
	    if (argc==0) {
		exp_error(interp,"usage: %s -i spawn_id", argv0);
		return(TCL_ERROR);
	    }
	    i_masters = *argv;
	    argc--; argv++;
	    continue;
	} else if (exp_flageq1('h',arg)) { /* "-h" */
	    argc--; argv++;
	    if (-1 == get_human_args(interp,&human_args))
		return(TCL_ERROR);
	    send_style = SEND_STYLE_HUMAN;
	    continue;
	} else if (exp_flageq1('s',arg)) { /* "-s" */
	    argc--; argv++;
	    if (-1 == get_slow_args(interp,&slow_args))
		return(TCL_ERROR);
	    send_style = SEND_STYLE_SLOW;
	    continue;
	} else if (exp_flageq("null",arg,1) || exp_flageq1('0',arg)) {
	    argc--; argv++;	/* "-null" */
	    if (!*argv) zeros = 1;
	    else {
		zeros = atoi(*argv);
		argc--; argv++;
		if (zeros < 1) return TCL_OK;
	    }
	    send_style = SEND_STYLE_ZERO;
	    string = "<zero(s)>";
	    continue;
	} else if (exp_flageq("raw",arg,1)) { /* "-raw" */
	    argc--; argv++;
	    want_cooked = FALSE;
	    continue;
	} else if (exp_flageq("break",arg,1)) {	/* "-break" */
	    argc--; argv++;
	    send_style = SEND_STYLE_BREAK;
	    string = "<break>";
	    continue;
	} else {
	    exp_error(interp,"usage: unrecognized flag <-%.80s>",arg);
	    return TCL_ERROR;
	}
    }
    
    if (send_style & SEND_STYLE_STRING_MASK) {
	if (argc != 1) {
	    exp_error(interp,"usage: %s [args] string", argv0);
	    return TCL_ERROR;
	}
	string = *argv;
    }
    len = strlen(string);
    
    if (clientData != NULL) {
	f = exp_chan2f(interp, (char *) clientData, 1, 0, argv0);
    } else if (!i_masters) {
	/*
	 * we really do want to check if it is open
	 * but since stdin could be closed, we have to first
	 * get the fs and then convert it from 0 to 1 if necessary
	 */
	f = exp_update_master(interp,0,0);
	if (f == NULL) {
	    return(TCL_ERROR);
	}
    }
    
    /*
     * if f != NULL, then it holds desired master, else i_masters does
     */
    
    if (f) {
	i = exp_new_i_simple(f,EXP_TEMPORARY);
    } else {
	i = exp_new_i_complex(interp,i_masters,FALSE,
			      (Tcl_VarTraceProc *)NULL,argv0);
	if (i == NULL) {
	    return TCL_ERROR;
	}
    }
    
    if (clientData == NULL) {
	/* This seems to be the standard send call (send_to_proc) */
	want_cooked = FALSE;
	debuglog("send: sending \"%s\" to {",dprintify(string));
	/* if closing brace doesn't appear, that's because an error */
	/* was encountered before we could send it */
    } else {
	if (debugfile) {
	    Tcl_Write(debugfile, string, len);
	}
	/* send_to_user ? */
	if (((strcmp((char *) clientData, "exp_user") == 0 ||
	      strcmp((char *) clientData, exp_dev_tty_id) == 0 ||
	      strcmp((char *) clientData, "exp_tty") == 0) && logfile_all) ||
	    logfile) {
	    if (logfile) {
		Tcl_Write(logfile, string, len);
	    }
	}
    }
    
    for (fs=i->fs_list;fs;fs=fs->next) {
	f = fs->f;

	if (clientData == NULL) {
	    /* send_to_proc */
	    debuglog(" %s ", f->spawnId);
	}

	/* check validity of each - i.e., are they open */
	if (! exp_fcheck(interp, f, 1, 0, "send")) {
	    rc = TCL_ERROR;
	    goto finish;
	}

	if (want_cooked) string = exp_cook(string,&len);

	switch (send_style) {
	case SEND_STYLE_PLAIN:
	    rc = exp_exact_write(f,string,len);
	    break;
	case SEND_STYLE_SLOW:
	    rc = slow_write(interp,f,string,len,&slow_args);
	    break;
	case SEND_STYLE_HUMAN:
	    rc = human_write(interp,f,string,&human_args);
	    break;
	case SEND_STYLE_ZERO:
	    for (;zeros>0;zeros--)
		rc = exp_exact_write(f, "", 1);
	    /* catching error on last write is sufficient */
	    break;
	case SEND_STYLE_BREAK:
	    exp_tty_break(interp,f);
	    rc = 0;
	    break;
	}

	if (rc != 0) {
	    if (rc == -1) {
		exp_error(interp,"write(spawn_id=%s): %s", f->spawnId,
			  Tcl_PosixError(interp));
		rc = TCL_ERROR;
	    }
	    goto finish;
	}
    }
    if (clientData == NULL) {
	/* send_to_proc */
	debuglog("}\r\n");
    }
    
    rc = TCL_OK;
 finish:
    exp_free_i(interp,i,(Tcl_VarTraceProc *)0);
    return rc;
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_LogFileCmd --
 *
 *	Implements the 'log_file' and 'exp_log_file' commands.
 *	Opens a logfile.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	A file may be opened, or a currently open file may be
 *	changed to unbuffered
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_LogFileCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    static Tcl_DString dstring;
    static int first_time = TRUE;
    static int current_append;	/* true if currently appending */
    static char *openarg = 0;	/* Tcl file identifier from -open */
    static int leaveopen = FALSE;	/* true if -leaveopen was used */
    
    int old_logfile_all = logfile_all;
    Tcl_Channel old_logfile = logfile;
    char *old_openarg = openarg;
    int old_leaveopen = leaveopen;
    
    int aflag = FALSE;
    int append = TRUE;
    char *filename = 0;
    char *type;
    int usage_error_occurred = FALSE;
    
    openarg = 0;
    leaveopen = FALSE;
    
    if (first_time) {
	Tcl_DStringInit(&dstring);
	first_time = FALSE;
    }
    
#define usage_error {usage_error_occurred = TRUE; goto error; }
    
    /* when this function returns, we guarantee that if logfile_all */
    /* is TRUE, then logfile is non-zero */
    
    argv++;
    argc--;
    for (;argc>0;argc--,argv++) {
	if (streq(*argv,"-open")) {
	    if (!argv[1]) usage_error;
	    openarg = ckalloc(strlen(argv[1])+1);
	    strcpy(openarg,argv[1]);
	    argc--; argv++;
	} else if (streq(*argv,"-leaveopen")) {
	    if (!argv[1]) usage_error;
	    openarg = ckalloc(strlen(argv[1])+1);
	    strcpy(openarg,argv[1]);
	    leaveopen = TRUE;
	    argc--; argv++;
	} else if (streq(*argv,"-a")) {
	    aflag = TRUE;
	} else if (streq(*argv,"-info")) {
	    if (logfile) {
		if (logfile_all) strcat(interp->result,"-a ");
		if (!current_append) strcat(interp->result,"-noappend ");
		strcat(interp->result,Tcl_DStringValue(&dstring));
	    }
	    return TCL_OK;
	} else if (streq(*argv,"-noappend")) {
	    append = FALSE;
	} else break;
    }
    
    if (argc == 1) {
	filename = argv[0];
    } else if (argc > 1) {
	/* too many arguments */
	usage_error
    } 
    
    if (openarg && filename) {
	usage_error
    }
    if (aflag && !(openarg || filename)) {
	usage_error
    }
    
    logfile = 0;
    logfile_all = aflag;
    
    current_append = append;
    
    type = (append?"a":"w");
    
    if (filename) {
	logfile = Tcl_OpenFileChannel(interp, filename, type, O_CREAT|S_IWRITE);
	if (logfile == (Tcl_Channel) NULL) {
	    exp_error(interp,"%s: %s",filename,Tcl_PosixError(interp));
	    goto error;
	}
    } else if (openarg) {
	int mode;
	
	Tcl_DStringTrunc(&dstring,0);
	
	if (!(logfile = Tcl_GetChannel(interp,openarg,&mode))) {
	    return TCL_ERROR;
	}
	if (!(mode & TCL_WRITABLE)) {
	    exp_error(interp,"channel is not writable");
	}

	if (leaveopen) {
	    Tcl_DStringAppend(&dstring,"-leaveopen ",-1);
	} else {
	    Tcl_DStringAppend(&dstring,"-open ",-1);
	}
	Tcl_DStringAppend(&dstring,openarg,-1);
	
	/*
	 * It would be convenient now to tell Tcl to close its
	 * file descriptor.  Alas, if involved in a pipeline, Tcl
	 * will be unable to complete a wait on the process.
	 * So simply remember that we meant to close it.  We will
	 * do so later in our own close routine.
	 */
    }
    if (logfile) {
	Tcl_SetChannelOption(interp, logfile, "-buffering", "none");
    }
    
    if (old_logfile) {
	if (!old_openarg || !old_leaveopen) { 
	    Tcl_Close(interp, old_logfile);
	}
	if (old_openarg) {
	    ckfree(old_openarg);
	}
    }
    
    return TCL_OK;
    
 error:
    if (old_logfile) {
	logfile = old_logfile;
	logfile_all = old_logfile_all;
    }
    
    if (openarg) ckfree(openarg);
    openarg = old_openarg;
    leaveopen = old_leaveopen;
    
    if (usage_error_occurred) {
	exp_error(interp,"usage: log_file [-info] [-noappend] [[-a] file] [-[leave]open [open ...]]");
    }
    
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_LogUserCmd --
 *
 *	Implements the 'loguser' and 'exp_loguser' commands.
 *	Can turn logging to stdout on or off, and returns the
 *	previous logging status.
 *
 * Results:
 *	A standard TCL result
 *
 * Side Effects:
 *	Logging can be enabled or disabled.
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_LogUserCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int old_loguser = loguser;
    
    if (argc == 0 || (argc == 2 && streq(argv[1],"-info"))) {
	/* do nothing */
    } else if (argc == 2) {
	if (0 == atoi(argv[1])) loguser = FALSE;
	else loguser = TRUE;
    } else {
	exp_error(interp,"usage: [-info|1|0]");
    }
    
    sprintf(interp->result,"%d",old_loguser);
    
    return(TCL_OK);
}

#ifdef TCL_DEBUGGER
/*
 *----------------------------------------------------------------------
 *
 * Exp_DebugCmd --
 *
 *	Implements the 'debug' and 'exp_debug' commands
 *
 * Results:
 *	A standard Tcl result
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_DebugCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int now = FALSE;		/* soon if FALSE, now if TRUE */
    int exp_tcl_debugger_was_available = exp_tcl_debugger_available;

    if (argc > 3) goto usage;

    if (argc == 1) {
	sprintf(interp->result,"%d",exp_tcl_debugger_available);
	return TCL_OK;
    }

    argv++;

    while (*argv) {
	if (streq(*argv,"-now")) {
	    now = TRUE;
	    argv++;
	}
	else break;
    }

    if (!*argv) {
	if (now) {
	    Dbg_On(interp,1);
	    exp_tcl_debugger_available = 1;
	} else {
	    goto usage;
	}
    } else if (streq(*argv,"0")) {
	Dbg_Off(interp);
	exp_tcl_debugger_available = 0;
    } else {
	Dbg_On(interp,now);
	exp_tcl_debugger_available = 1;
    }
    sprintf(interp->result,"%d",exp_tcl_debugger_was_available);
    return(TCL_OK);
 usage:
    exp_error(interp,"usage: [[-now] 1|0]");
    return TCL_ERROR;
}
#endif /* TCL_DEBUGGER */

/*ARGSUSED*/
static int
Exp_ExpInternalCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    static Tcl_DString dstring;
    static int first_time = TRUE;
    int fopened = FALSE;

    if (first_time) {
	Tcl_DStringInit(&dstring);
	first_time = FALSE;
    }

    if (argc > 1 && streq(argv[1],"-info")) {
	if (debugfile) {
	    sprintf(interp->result,"-f %s ",
		    Tcl_DStringValue(&dstring));
	}
	strcat(interp->result,((exp_is_debugging==0)?"0":"1"));
	return TCL_OK;
    }

    argv++;
    argc--;
    while (argc) {
	if (!streq(*argv,"-f")) break;
	argc--;argv++;
	if (argc < 1) goto usage;
	if (debugfile) {
	    Tcl_Close(interp, debugfile);
	}

	debugfile = Tcl_OpenFileChannel(interp, argv[0], "a", O_APPEND|S_IWRITE);
	if (debugfile == (Tcl_Channel) NULL) {
	    exp_error(interp,"%s: %s",argv[0],Tcl_PosixError(interp));
	    goto error;
	}
	Tcl_DStringAppend(&dstring,argv[0],-1);

	Tcl_SetChannelOption(interp, debugfile, "-buffering", "none");
	fopened = TRUE;
	argc--;argv++;
    }

    if (argc != 1) goto usage;

    /* if no -f given, close file */
    if (fopened == FALSE && debugfile) {
	Tcl_Close(interp, debugfile);
	debugfile = NULL;
	Tcl_DStringFree(&dstring);
    }

    exp_is_debugging = atoi(*argv);
    return(TCL_OK);
 usage:
    exp_error(interp,"usage: [-f file] expr");
 error:
    Tcl_DStringFree(&dstring);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_ExitCmd --
 *
 *	Called on exit to do cleanup.
 *
 * Results:
 *	A standard Tcl result
 *
 *----------------------------------------------------------------------
 */

char *exp_onexit_action = 0;

/*ARGSUSED*/
static int
Exp_ExitCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int value = 0;

    argv++;

    if (*argv) {
	if (exp_flageq(*argv,"-onexit",3)) {
	    argv++;
	    if (*argv) {
		int len = strlen(*argv);
		if (exp_onexit_action)
		    ckfree(exp_onexit_action);
		exp_onexit_action = ckalloc(len + 1);
		strcpy(exp_onexit_action,*argv);
	    } else if (exp_onexit_action) {
		Tcl_AppendResult(interp,exp_onexit_action,(char *)0);
	    }
	    return TCL_OK;
	} else if (exp_flageq(*argv,"-noexit",3)) {
	    argv++;
	    exp_exit_handlers((ClientData)interp);
	    return TCL_OK;
	}
    }

    if (*argv) {
	if (Tcl_GetInt(interp, *argv, &value) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    exp_exit(interp,value);
    /*NOTREACHED*/
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_CloseCmd --
 *
 *	Currently closes a channel or sets up handlers for
 *	when the channel closes.
 *
 * Results:
 *	A standard Tcl result
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_CloseCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char *close_onexec = NULL;
    int slave_flag = FALSE;
    char *argv0 = argv[0];
    struct exp_f *f;
#if 0
    int slave;
#endif
    char *chanId = NULL;

    int argc_orig = argc;
    char **argv_orig = argv;

    argc--; argv++;

    for (;argc>0;argc--,argv++) {
	if (streq(*argv,"-i")) {
	    argc--; argv++;
	    if (!*argv) {
		exp_error(interp,"usage: -i spawn_id");
		return(TCL_ERROR);
	    }
	    chanId = *argv;
	} else if (streq(*argv,"-slave")) {
	    slave_flag = TRUE;
	} else if (streq(*argv,"-onexec")) {
	    argc--; argv++;
	    if (!*argv) {
		exp_error(interp,"usage: -onexec channelId");
		return(TCL_ERROR);
	    }
	    close_onexec = *argv;
	} else break;
    }

    if (argc) {
	/* doesn't look like our format, it must be a Tcl-style file */
	/* handle.  Lucky that formats are easily distinguishable. */
	/* Historical note: we used "close"  long before there was a */
	/* Tcl builtin by the same name. */

	Tcl_Obj **objv;
	int i, result;
	Tcl_CmdInfo info;

	Tcl_ResetResult(interp);
	objv = (Tcl_Obj **) ckalloc((argc+1)*sizeof(Tcl_Obj *));
	objv[0] = Tcl_NewStringObj("exp_tcl_close", -1);
	for (i = 0; i < argc; i++) {
	    objv[i+1] = Tcl_NewStringObj(argv[i], -1);
	}

	if (0 == Tcl_GetCommandInfo(interp,"exp_tcl_close",&info)) {
	    info.clientData = 0;
	}
	result = info.objProc(info.objClientData,interp,argc+1,objv);
	for (i = 0; i < argc+1; i++) {
	    Tcl_DecrRefCount(objv[i]);
	}
	ckfree((char *) objv);
	return result;
    }

    if (chanId == NULL) {
	f = exp_update_master(interp, 1, 0);
    } else if (slave_flag) {
	f = exp_chan2f(interp, chanId, 1, 0, "-slave");
    } else {
	f = exp_chan2f(interp, chanId, 1, 0, argv0);
    }
    if (f == NULL) {
	return TCL_ERROR;
    }

    if (slave_flag) {
	if (f->slave_fd) {
#ifndef __WIN32__ /* XXX: This still needs some looking at */
	    close(f->slave_fd);
	    f->slave_fd = EXP_NOFD;

	    exp_slave_control(f,1);
#endif
	    return TCL_OK;
	}
	exp_error(interp,"no such slave");
	return TCL_ERROR;
    }

#ifdef XXX /* I'm not sure this is a good idea to support */
    if (onexec_flag) {
	/* heck, don't even bother to check if fd is open or a real */
	/* spawn id, nothing else depends on it */
	fcntl(m,F_SETFD,close_onexec);
	return TCL_OK;
    }
#endif

    return exp_close(interp, f);
}

/*ARGSUSED*/
static void
tcl_tracer(clientData,interp,level,command,cmdProc,cmdClientData,argc,argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int level;
    char *command;
    Tcl_CmdProc *cmdProc;
    ClientData cmdClientData;
    int argc;
    char *argv[];
{
    int i;
    
    /* come out on stderr, by using errorlog */
    errorlog("%2d",level);
    for (i = 0;i<level;i++) exp_nferrorlog("  ",0/*ignored - satisfy lint*/);
    errorlog("%s\r\n",command);
}

/*ARGSUSED*/
static int
Exp_StraceCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    static int trace_level = 0;
    static Tcl_Trace trace_handle;

    if (argc > 1 && streq(argv[1],"-info")) {
	sprintf(interp->result,"%d",trace_level);
	return TCL_OK;
    }

    if (argc != 2) {
	exp_error(interp,"usage: trace level");
	return(TCL_ERROR);
    }
    /* tracing already in effect, undo it */
    if (trace_level > 0) Tcl_DeleteTrace(interp,trace_handle);

    /* get and save new trace level */
    trace_level = atoi(argv[1]);
    if (trace_level > 0)
	trace_handle = Tcl_CreateTrace(interp,
				       trace_level,tcl_tracer,(ClientData)0);
    return(TCL_OK);
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_WaitCmd --
 *
 *	Implements the 'wait' and 'exp_wait' commands.  When a process
 *	has been spawned, the wait call must be made before it will
 *	go away.
 *
 * Results:
 *	A standard Tcl result
 *
 * Notes:
 *	XXX: This might need to go into the platform specific file.
 *	Need to make sure that we do the right thing when exp_open
 *	has been called on an identifier.
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_WaitCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int master_supplied = FALSE;
    struct exp_f *f;	/* ditto */
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    struct forked_proc *fp = 0;	/* handle to a pure forked proc */
    
#ifdef XXX
    struct exp_f ftmp;	/* temporary memory for either f or fp */
#endif
    
    int nowait = FALSE;
    int nohang = FALSE;
    char *chanId = NULL;
    char *argv0 = argv[0];
    Tcl_Pid result = 0;		/* 0 means child was successfully waited on */

    /* -1 means an error occurred */
    /* -2 means no eligible children to wait on */
#define NO_CHILD ((Tcl_Pid) -2)
    
    argv++;
    argc--;
    for (;argc>0;argc--,argv++) {
	if (streq(*argv,"-i")) {
	    argc--; argv++;
	    if (argc==0) {
		exp_error(interp,"usage: -i spawn_id");
		return(TCL_ERROR);
	    }
	    chanId = *argv;
	} else if (streq(*argv,"-nowait")) {
	    nowait = TRUE;
	} else if (streq(*argv,"-nohang")) {
	    nohang = TRUE;
	}
    }
    
    if (chanId == NULL) {
	f = exp_update_master(interp, 0, 0);
    } else {
	f = exp_chan2f(interp, chanId, 0, 0, argv0);
    }
    if (f == NULL) {
	return TCL_ERROR;
    }
    
    if (f != exp_f_any) {
	/*
	 * Check if waited on already.  Things opened by "open", set
	 * with -nowait, or are special expect ids are marked sys_waited
	 * already
	 */
	if (!f->sys_waited) {
	    if (nowait) {
		/*
		 * should probably generate an error
		 * if SIGCHLD is trapped.
		 */

		/*
		 * pass to Tcl, so it can do wait in background.
		 */
		Tcl_DetachPids(1,&f->tclPid);
		exp_wait_zero(&f->wait);
		f->sys_waited = 1;
		f->user_waited = 1;

	    } else if (nohang) {
		exp_wait_zero(&f->wait);
		result = Tcl_WaitPid(f->tclPid,&f->wait,WNOHANG);

	    } else {
		while (1) {
		    if (Tcl_AsyncReady()) {
			int rc = Tcl_AsyncInvoke(interp,TCL_OK);
			if (rc != TCL_OK) return(rc);
		    }

		    exp_wait_zero(&f->wait);
		    result = Tcl_WaitPid(f->tclPid,&f->wait,0);
		    if (result == f->tclPid) break;
		    if (result == (Tcl_Pid) -1) {
			if (errno == EINTR) continue;
			else break;
		    }
		}
	    }
	}

	/*
	 * Now have Tcl reap anything we just detached. 
	 * This also allows procs user has created with "exec &"
	 * and and associated with an "exec &" process to be reaped.
	 */

	Tcl_ReapDetachedProcs();
	exp_rearm_sigchld(interp); /* new */
    } else {
	/*
	 * Wait for any of our own spawned processes. We call waitpid
	 * rather than wait to avoid running into someone else's processes.
	 * Yes, according to Ousterhout this is the best way to do it.
	 */

	hPtr = Tcl_FirstHashEntry(exp_f_table, &search);
	while (hPtr) {
	    f = (struct exp_f *) Tcl_GetHashValue(hPtr);

	    if (!f->valid) continue;
	    if (f->pid == EXP_NOPID) continue;
	    if (f->pid == exp_getpid) continue; /* skip ourself */
	    if (f->user_waited) continue; /* one wait only! */
	    if (f->sys_waited) break;
	restart:
	    exp_wait_zero(&f->wait);
	    result = Tcl_WaitPid(f->tclPid,&f->wait,WNOHANG);
	    if (result == f->tclPid) break;
	    if (result == 0) continue; /* busy, try next */
	    if (result == (Tcl_Pid) -1) {
		if (errno == EINTR) goto restart;
		else break;
	    }
	    hPtr = Tcl_NextHashEntry(&search);
	}
	
#ifdef XXX
	/* if it's not a spawned process, maybe its a forked process */
	for (fp=forked_proc_base;fp;fp=fp->next) {
	    if (fp->link_status == not_in_use) continue;
	restart2:
	    result = waitpid(fp->pid,&fp->wait_status,WNOHANG);
	    if (result == fp->pid) {
		m = -1;		/* DOCUMENT THIS! */
		break;
	    }
	    if (result == 0) continue; /* busy, try next */
	    if (result == -1) {
		if (errno == EINTR) goto restart2;
		else break;
	    }
	}
#endif /* XXX */
	
	if (hPtr == NULL && fp == NULL) {
	    result = NO_CHILD;	/* no children */
	    Tcl_ReapDetachedProcs();
	}
	exp_rearm_sigchld(interp);
    }
    
#ifdef XXX
    /*  sigh, wedge forked_proc into an exp_f structure so we don't
     *  have to rewrite remaining code (too much)
     */
    if (fp) {
	f = &ftmp;
	f->tclPid = fp->pid;
	f->wait = fp->wait_status;
    }
#endif
    /* non-portable assumption that pid_t can be printed with %d */

    if (result == (Tcl_Pid) -1) {
	sprintf(interp->result,"%d %s -1 %d POSIX %s %s",
		f->pid,f->spawnId,errno,Tcl_ErrnoId(),Tcl_ErrnoMsg(errno));
	result = TCL_OK;
	f->sys_waited = TRUE;
	f->user_waited = TRUE;
    } else if (result == NO_CHILD) {
	interp->result = "no children";
	return TCL_ERROR;
    } else {
	sprintf(interp->result,"%d %s 0 %d",
		f->pid,f->spawnId,WEXITSTATUS(f->wait));
	if (WIFSIGNALED(f->wait)) {
	    Tcl_AppendElement(interp,"CHILDKILLED");
	    Tcl_AppendElement(interp,Tcl_SignalId((int)(WTERMSIG(f->wait))));
	    Tcl_AppendElement(interp,Tcl_SignalMsg((int) (WTERMSIG(f->wait))));
	} else if (WIFSTOPPED(f->wait)) {
	    Tcl_AppendElement(interp,"CHILDSUSP");
	    Tcl_AppendElement(interp,Tcl_SignalId((int) (WSTOPSIG(f->wait))));
	    Tcl_AppendElement(interp,Tcl_SignalMsg((int) (WSTOPSIG(f->wait))));
	}
	if (nohang && result == 0) {
	    Tcl_AppendElement(interp,"NOEXIT");
	}
	if (result > 0 && WIFEXITED(f->wait)) {
	    f->sys_waited = TRUE;
	    f->user_waited = TRUE;
	}
    }

#ifdef XXX
    if (fp) {
	fp->link_status = not_in_use;
	return ((result == -1)?TCL_ERROR:TCL_OK);		
    }
#endif
    
    /*
     * if user has already called close, make sure fd really is closed
     * and forget about this entry entirely
     */
    if (f->user_closed) {
	exp_f_free(f);
    }
    return ((result == (Tcl_Pid) -1)?TCL_ERROR:TCL_OK);
}

/*ARGSUSED*/
int
Exp_InterpreterCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (argc != 1) {
	exp_error(interp,"no arguments allowed");
	return(TCL_ERROR);
    }
    
#ifdef __WIN32__
    exp_error(interp, "not implemented on Windows NT");
    return TCL_ERROR;
#endif
    return(exp_interpreter(interp));
    /* errors and ok, are caught by exp_interpreter() and discarded */
    /* to return TCL_OK, type "return" */
}

/* this command supercede's Tcl's builtin CONTINUE command */
/*ARGSUSED*/
int
Exp_ExpContinueDeprecatedCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (argc == 1) return(TCL_CONTINUE);
    else if (argc == 2) {
	if (streq(argv[1],"-expect")) {
	    debuglog("continue -expect is deprecated, use exp_continue\r\n");
	    return(EXP_CONTINUE);
	}
    }
    exp_error(interp,"usage: continue [-expect]\n");
    return(TCL_ERROR);
}

/* this command supercede's Tcl's builtin CONTINUE command */
/*ARGSUSED*/
int
Exp_ExpContinueCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    if (argc == 1) {
	return EXP_CONTINUE;
    } else if ((argc == 2) && (streq(argv[1],"-continue_timer"))) {
	return EXP_CONTINUE_TIMER;
    }

    exp_error(interp,"usage: exp_continue [-continue_timer]\n");
    return(TCL_ERROR);
}

/* most of this is directly from Tcl's definition for return */
/*ARGSUSED*/
int
Exp_InterReturnCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    /* let Tcl's return command worry about args */
    /* if successful (i.e., TCL_RETURN is returned) */
    /* modify the result, so that we will handle it specially */

    int result;
#if TCL_MAJOR_VERSION >= 8
    Tcl_Obj **objv;
    int i;

    objv = (Tcl_Obj **) ckalloc(argc*sizeof(Tcl_Obj *));
    for (i = 0; i < argc; i++) {
	objv[i] = Tcl_NewStringObj(argv[i], -1);
    }
    result = Tcl_ReturnObjCmd(clientData,interp,argc,objv);
    for (i = 0; i < argc; i++) {
	Tcl_DecrRefCount(objv[i]);
    }
    ckfree((char *) objv);
#else
    result = Tcl_ReturnCmd(clientData,interp,argc,argv);
#endif

    if (result == TCL_RETURN)
	result = EXP_TCL_RETURN;
    return result;

}

/*
 *----------------------------------------------------------------------
 *
 * Exp_OpenCmd --
 *
 *	Implements the exp_open command.  Makes a spawn id available
 *	to Tcl.  Since this happens by default, we don't have to do
 *	anything in the -leaveopen case.  In the normal case, we
 *	need to clean up the spawn identifier and that is all.
 *
 * Results:
 *	A standard Tcl result
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
int
Exp_OpenCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    struct exp_f *f;
    int leaveopen = FALSE;
    char *chanId = NULL;
    char *argv0 = argv[0];

    argc--; argv++;

    for (;argc>0;argc--,argv++) {
	if (streq(*argv,"-i")) {
	    argc--; argv++;
	    if (!*argv) {
		exp_error(interp,"usage: -i spawn_id");
		return TCL_ERROR;
	    }
	    chanId = *argv;
	} else if (streq(*argv,"-leaveopen")) {
	    leaveopen = TRUE;
	    argc--; argv++;
	} else break;
    }

    if (chanId == NULL) {
	f = exp_update_master(interp,0,0);
    } else {
	f = exp_chan2f(interp, chanId, 1, 0, argv0);
    }
    if (f == NULL) {
	return(TCL_ERROR);
    }
    if (f->channel == NULL) {
	exp_error(interp,
		  "%s: %s is an internal spawn id that cannot be expected as a channel id",
		  argv0, chanId);
	return TCL_ERROR;
    }
    if (! leaveopen) {
	/*
	 * Don't do any reference count check on f->leaveopen.  Just force
	 * the spawn id closed
	 */
	if (f->channel) {
	    Tcl_DeleteCloseHandler(f->channel, (Tcl_CloseProc *) exp_f_free,
				   (ClientData) f);
	}
	exp_f_free(f);
    }

    Tcl_AppendResult(interp, Tcl_GetChannelName(f->channel), (char *) NULL);
    return TCL_OK;
}

/* return 1 if a string is substring of a flag */
/* this version is the code used by the macro that everyone calls */
int
exp_flageq_code(flag,string,minlen)
char *flag;
char *string;
int minlen;		/* at least this many chars must match */
{
	for (;*flag;flag++,string++,minlen--) {
		if (*string == '\0') break;
		if (*string != *flag) return 0;
	}
	if (*string == '\0' && minlen <= 0) return 1;
	return 0;
}

void
exp_create_commands(interp,c)
    Tcl_Interp *interp;
    struct exp_cmd_data *c;
{
    Interp *iPtr = (Interp *) interp;
    char cmdnamebuf[80];

    for (;c->name;c++) {
	int create = FALSE;
	/* if already defined, don't redefine */
	if (c->flags & EXP_REDEFINE) create = TRUE;
	else if (!Tcl_FindHashEntry(&iPtr->globalNsPtr->cmdTable,c->name)) {
	    create = TRUE;
	}
	if (create) {
	    sprintf(cmdnamebuf, "rename %s exp_tcl_%s", c->name, c->name);
	    Tcl_GlobalEval(interp, cmdnamebuf);
	    Tcl_CreateCommand(interp,c->name,c->proc,
			      c->data, (Tcl_CmdDeleteProc *) NULL);
	}
	if (!(c->name[0] == 'e' &&
	      c->name[1] == 'x' &&
	      c->name[2] == 'p')
	    && !(c->flags & EXP_NOPREFIX))
	{
	    sprintf(cmdnamebuf,"exp_%s",c->name);
	    Tcl_CreateCommand(interp,cmdnamebuf,c->proc,
			      c->data, (Tcl_CmdDeleteProc *) NULL);
	}
    }
}

static struct exp_cmd_data cmd_data[]  = {
{"close",	Exp_CloseCmd,	0,	EXP_REDEFINE},
#ifdef TCL_DEBUGGER
{"debug",	Exp_DebugCmd,	0,	0},
#endif
{"exp_internal",Exp_ExpInternalCmd,	0,	0},
#ifdef XXX
{"disconnect",	Exp_DisconnectCmd,	0,	0},
#endif
{"exit",	Exp_ExitCmd,	0,	EXP_REDEFINE},
{"continue",	Exp_ExpContinueDeprecatedCmd,0,EXP_NOPREFIX|EXP_REDEFINE},
{"exp_continue",Exp_ExpContinueCmd,0,	0},
#ifdef XXX
{"fork",	Exp_ForkCmd,	0,	0},
#endif
{"exp_pid",	Exp_ExpPidCmd,	0,	0},
{"getpid",	Exp_GetpidDeprecatedCmd,0,	0},
{"interpreter",	Exp_InterpreterCmd,	0,	0},
{"kill",	Exp_KillCmd,	0,	0},
{"log_file",	Exp_LogFileCmd,	0,	0},
{"log_user",	Exp_LogUserCmd,	0,	0},
{"exp_open",	Exp_OpenCmd,	0,	0},
#ifdef XXX
{"overlay",	Exp_OverlayCmd,	0,	0},
#endif
{"inter_return",Exp_InterReturnCmd,	0,	0},
{"send",	Exp_SendCmd,	(ClientData)NULL,	0},
{"send_spawn",	Exp_SendCmd,	(ClientData)NULL,	0},/*deprecat*/
{"send_error",	Exp_SendCmd,	(ClientData)"stderr",	0},
{"send_log",	Exp_SendLogCmd,	0,	0},
{"send_tty",	Exp_SendCmd,	(ClientData)"exp_tty",	0},
{"send_user",	Exp_SendCmd,	(ClientData)"exp_user",	0},
{"sleep",	Exp_SleepCmd,	0,	0},
{"spawn",	Exp_SpawnCmd,	0,	0},
{"strace",	Exp_StraceCmd,	0,	0},
{"wait",	Exp_WaitCmd,	0,	0},
{0}};

/*
 *----------------------------------------------------------------------
 *
 * exp_init_most_cmds --
 *
 *	Initialize the large majority of commands that are used
 *	in expect
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
exp_init_most_cmds(interp)
    Tcl_Interp *interp;
{
    exp_create_commands(interp,cmd_data);

    exp_close_in_child = exp_close_tcl_files;
}

void
exp_init_spawn_id_vars(interp)
    Tcl_Interp *interp;
{
    Tcl_SetVar(interp,"user_spawn_id",EXP_SPAWN_ID_USER,0);
    Tcl_SetVar(interp,"error_spawn_id",EXP_SPAWN_ID_ERROR,0);
    Tcl_SetVar(interp,"tty_spawn_id","exp_tty",0);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_init_spawn_ids --
 *
 *	Create the structures for the standard spawn ids.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
exp_init_spawn_ids(interp)
    Tcl_Interp *interp;
{
    Tcl_Channel chan, chanIn, chanOut;
    struct exp_f *f;

    exp_f_table = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(exp_f_table, TCL_STRING_KEYS);

    chan = ExpCreatePairChannel(interp, "stdin", "stdout", "exp_user");
    f = exp_f_new(interp, chan, NULL, EXP_NOPID);
    Tcl_RegisterChannel(interp, chan);

    chan = Tcl_GetStdChannel(TCL_STDIN);
    f = exp_f_new(interp, chan, NULL,
		  (isatty(0) ? exp_getpid : EXP_NOPID));
    f->leaveopen = 1;
    if (strcmp(Tcl_GetChannelName(chan), "stdin") != 0) {
	f = exp_f_new(interp, chan, "stdin",
		      (isatty(0) ? exp_getpid : EXP_NOPID));
	f->leaveopen = 1;
    }
    chanIn = chan;

    chan = Tcl_GetStdChannel(TCL_STDOUT);
    if (chan != chanIn) {
	f = exp_f_new(interp, chan, NULL,
		      (isatty(1) ? exp_getpid : EXP_NOPID));
	f->leaveopen = 1;
    }
    if (strcmp(Tcl_GetChannelName(chan), "stdout") != 0) {
	f = exp_f_new(interp, chan, "stdout",
		      (isatty(1) ? exp_getpid : EXP_NOPID));
	f->leaveopen = 1;
    }
    chanOut = chan;

    chan = Tcl_GetStdChannel(TCL_STDERR);
    if ((chan != chanIn) && (chan != chanOut)) {
	f = exp_f_new(interp, chan, NULL,
		      (isatty(2) ? exp_getpid : EXP_NOPID));
	f->leaveopen = 1;
    }
    if (strcmp(Tcl_GetChannelName(chan), "stderr") != 0) {
	f = exp_f_new(interp, chan, "stderr",
		      (isatty(2) ? exp_getpid : EXP_NOPID));
	f->leaveopen = 1;
    }

    /*
     * Create the 'exp_any' spawn id that is meant to many any spawn ids.
     */

    f = exp_f_new(interp, NULL, "exp_any", exp_getpid);
    f->alwaysopen = 1;
    exp_f_any = f;

#ifdef XXX
    /* really should be in interpreter() but silly to do on every call */
    exp_adjust(&exp_fs[0]);
#endif
}
