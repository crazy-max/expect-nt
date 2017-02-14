/* exp_event.c - event interface for Expect

Written by: Don Libes, NIST, 2/6/90

Design and implementation of this program was paid for by U.S. tax
dollars.  Therefore it is public domain.  However, the author and NIST
would appreciate credit if this program or parts of it are used.

*/

/* Notes:
I'm only a little worried because Tk does not check for errno == EBADF
after calling select.  I imagine that if the user passes in a bad file
descriptor, we'll never get called back, and thus, we'll hang forever
- it would be better to at least issue a diagnostic to the user.

Another possible problem: Tk does not do file callbacks round-robin.

Another possible problem: Calling Create/DeleteChannelHandler
before/after every Tcl_Eval... in expect/interact could be very
expensive.

*/


#include "tcl.h"
#include "tclPort.h"
#include "exp_port.h"
#include "exp_prog.h"
#include "exp_command.h"	/* for struct exp_f defs */
#include "exp_event.h"

/* Tcl_DoOneEvent will call our filehandler which will set the following */
/* vars enabling us to know where and what kind of I/O we can do */
/*#define EXP_SPAWN_ID_BAD	-1*/
/*#define EXP_SPAWN_ID_TIMEOUT	-2*/	/* really indicates a timeout */

static struct exp_f *ready_fs = NULL;
/* static int ready_fd = EXP_SPAWN_ID_BAD; */
static int ready_mask;
static int default_mask = TCL_READABLE | TCL_EXCEPTION;


/*
 * Declarations for functions used only in this file.
 */

static void		exp_timehandler _ANSI_ARGS_ ((ClientData clientData));
static void		exp_filehandler _ANSI_ARGS_ ((ClientData clientData,
			    int mask));
static void		exp_event_exit_real _ANSI_ARGS_ ((Tcl_Interp *interp));


/*
 *----------------------------------------------------------------------
 *
 * exp_event_disarm --
 *
 *	Completely remove the filehandler for this process
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Events will no longer be reported for this process
 *
 *----------------------------------------------------------------------
 */

void
exp_event_disarm(f)
    struct exp_f *f;
{
    Tcl_Channel channel;

    channel = f->Master;
    if (! channel) {
	channel = f->channel;
    }
    Tcl_DeleteChannelHandler(channel, f->event_proc, f->event_data);
    f->event_proc = NULL;

    /* remember that filehandler has been disabled so that */
    /* it can be turned on for fg expect's as well as bg */
    f->fg_armed = FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_event_disarm_fast --
 *
 *	Temporarily disable the filehandler for this process.  This
 *	is quicker than calling exp_event_disasrm as it reduces the
 *	calls to malloc() and free() inside Tcl_...FileHandler.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Events will no longer be reported for this process
 *
 *----------------------------------------------------------------------
 */

static void
exp_event_disarm_fast(f,filehandler)
    struct exp_f *f;
    Tcl_ChannelProc *filehandler;
{
    Tcl_Channel channel;

    channel = f->Master;
    if (! channel) {
	channel = f->channel;
    }
    /* Tk insists on having a valid proc here even though it isn't used */
    if (f->event_proc) {
	Tcl_DeleteChannelHandler(channel,f->event_proc,f->event_data);
    }
    Tcl_CreateChannelHandler(channel,0,filehandler,(ClientData)0);
    f->event_proc = filehandler;
    f->event_data = 0;

    /* remember that filehandler has been disabled so that */
    /* it can be turned on for fg expect's as well as bg */
    f->fg_armed = FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_arm_background_filehandler_force --
 *
 *	Always installs a background filehander
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Background events will be reported for this process
 *
 *----------------------------------------------------------------------
 */

static void
exp_arm_background_filehandler_force(f)
    struct exp_f *f;
{
    Tcl_Channel channel;

    channel = f->Master;
    if (! channel) {
	channel = f->channel;
    }
    if (f->event_proc) {
	Tcl_DeleteChannelHandler(channel,f->event_proc,f->event_data);
    }
    Tcl_CreateChannelHandler(channel, TCL_READABLE|TCL_EXCEPTION,
	exp_background_filehandler, (ClientData) f);
    f->event_proc = exp_background_filehandler;
    f->event_data = (ClientData) f;

    f->bg_status = armed;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_arm_background_filehandler --
 *
 *	Installs a background filehandler if it hasn't already been
 *	installed or if it was disabled.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Background events will be reported for this process
 *
 *----------------------------------------------------------------------
 */

void
exp_arm_background_filehandler(f)
    struct exp_f *f;
{
    switch (f->bg_status) {
    case unarmed:
	exp_arm_background_filehandler_force(f);
	break;
    case disarm_req_while_blocked:
	f->bg_status = blocked;	/* forget request */
	break;
    case armed:
    case blocked:
	/* do nothing */
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * exp_disarm_background_filehandler --
 *
 *	Removes a background filehandler if it was previously installed
 *	and armed.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Background events will no longer be reported for this process
 *
 *----------------------------------------------------------------------
 */

void
exp_disarm_background_filehandler(f)
    struct exp_f *f;
{
    switch (f->bg_status) {
    case blocked:
	f->bg_status = disarm_req_while_blocked;
	break;
    case armed:
	f->bg_status = unarmed;
	exp_event_disarm(f);
	break;
    case disarm_req_while_blocked:
    case unarmed:
	/* do nothing */
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * exp_disarm_background_filehandler_force --
 *
 *	Removes a background filehandler if it was previously installed,
 *	ignoring block status.  Called from exp_close().  After exp_close
 *	returns, we will not have an opportunity to disarm because the fd
 *	will be invalid, so we force it here.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Background events will no longer be reported for this process
 *
 *----------------------------------------------------------------------
 */

void
exp_disarm_background_filehandler_force(f)
    struct exp_f *f;
{
    switch (f->bg_status) {
    case blocked:
    case disarm_req_while_blocked:
    case armed:
	f->bg_status = unarmed;
	exp_event_disarm(f);
	break;
    case unarmed:
	/* do nothing */
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * exp_unblock_background_filehandler --
 *
 *	Unblocks the background filehandler.
 *	This can only be called at the end of the bg handler in which
 *	case we know the status is some kind of "blocked"
 *
 * Results:
 *	None
 *
 * Side Effects:
 *
 *
 *----------------------------------------------------------------------
 */

void
exp_unblock_background_filehandler(f)
    struct exp_f *f;
{
    switch (f->bg_status) {
    case blocked:
	exp_arm_background_filehandler_force(f);
	break;
    case disarm_req_while_blocked:
	exp_disarm_background_filehandler_force(f);
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * exp_block_background_filehandler --
 *
 *	Blocks the background filehandler.
 *	This can only be called at the end of the bg handler in which
 *	case we know the status is some kind of "armed"
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Temporarily removes the filehandler, so events will stop
 *	being reported for this process.
 *
 *----------------------------------------------------------------------
 */

void
exp_block_background_filehandler(f)
    struct exp_f *f;
{
    f->bg_status = blocked;
    exp_event_disarm_fast(f,exp_background_filehandler);
}


/*
 *----------------------------------------------------------------------
 *
 * exp_timehandler --
 *
 *	Tcl calls this routine when timer we have set has expired.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A flag is set.
 *
 *----------------------------------------------------------------------
 */

static void
exp_timehandler(clientData)
    ClientData clientData;
{
    *(int *)clientData = TRUE;	
}

/*
 *----------------------------------------------------------------------
 *
 * exp_filehandler --
 *
 *	Tcl calls this routine when some data is available on a
 *	channel.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Sets the global value of what process is ready.  This is
 *	checked at the return of Tcl_DoOneEvent().
 *
 *----------------------------------------------------------------------
 */

static void exp_filehandler(clientData,mask)
    ClientData clientData;
    int mask;
{
    /*
     * if input appears, record the fd on which it appeared
     */
    ready_fs = (struct exp_f *) clientData;
    /* ready_fd = *(int *)clientData; */
    ready_mask = mask;
    exp_event_disarm_fast(ready_fs,exp_filehandler);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_get_next_event --
 *
 *	Waits for the next event that expect is registered an
 *	interest in.
 *
 * Results:
 *	Returns status, one of EOF, TIMEOUT, ERROR, DATA or RECONFIGURE
 *
 * Side Effects: 
 *	Other event handlers outside of Expect may be run as well
 *
 * Notes:
 *	This still needs some work to run properly under NT
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
int exp_get_next_event(interp,masters, n,master_out,timeout,key)
    Tcl_Interp *interp;
    struct exp_f **masters;	/* Array of expect process structures */
    int n;			/* # of masters */
    struct exp_f **master_out;	/* 1st ready master, not set if none */
    int timeout;		/* seconds */
    int key;
{
    static rr = 0;	/* round robin ptr */
    int i;	/* index into in-array */
#ifdef HAVE_PTYTRAP
    struct request_info ioctl_info;
#endif
    
    int old_configure_count = exp_configure_count;
    
    int timer_created = FALSE;
    int timer_fired = FALSE;
    Tcl_TimerToken timetoken;/* handle to Tcl timehandler descriptor */
    
    for (;;) {
	struct exp_f *f;

	/* if anything has been touched by someone else, report that */
	/* an event has been received */

	for (i=0;i<n;i++) {
	    rr++;
	    if (rr >= n) rr = 0;

	    f = masters[rr];
	    if (f->key != key) {
		f->key = key;
		f->force_read = FALSE;
		*master_out = f;
		return(EXP_DATA_OLD);
	    } else if ((!f->force_read) && (f->size != 0)) {
		*master_out = f;
		return(EXP_DATA_OLD);
	    }
	}

	if (!timer_created) {
	    if (timeout >= 0) {
		timetoken = Tcl_CreateTimerHandler(1000*timeout,
						   exp_timehandler,
						   (ClientData)&timer_fired);
		timer_created = TRUE;
	    }
	}

	for (;;) {
	    int j;

	    /* make sure that all fds that should be armed are */
	    for (j=0;j<n;j++) {
		f = masters[j];

		if (!f->fg_armed) {
		    Tcl_Channel channel;

		    channel = f->Master;
		    if (! channel) {
			channel = f->channel;
		    }

		    if (f->event_proc) {
			Tcl_DeleteChannelHandler(channel,f->event_proc,
			    f->event_data);
		    }
		    Tcl_CreateChannelHandler(channel, default_mask,
			exp_filehandler, (ClientData)f);
		    f->event_proc = exp_filehandler;
		    f->event_data = (ClientData) f;
		    f->fg_armed = TRUE;
		}
	    }

	    Tcl_DoOneEvent(0);	/* do any event */

	    if (timer_fired) return(EXP_TIMEOUT);

	    if (old_configure_count != exp_configure_count) {
		if (timer_created)
		    Tcl_DeleteTimerHandler(timetoken);
		return EXP_RECONFIGURE;
	    }

	    if (ready_fs == NULL) continue;

	    /* if it was from something we're not looking for at */
	    /* the moment, ignore it */
	    for (j=0;j<n;j++) {
		if (ready_fs == masters[j]) goto found;
	    }

	    /* not found */
	    exp_event_disarm_fast(ready_fs,exp_filehandler);
	    ready_fs = NULL;
	    continue;
	found:
	    *master_out = ready_fs;
	    ready_fs = NULL;

	    if (timer_created) Tcl_DeleteTimerHandler(timetoken);

	    /* this test should be redundant but SunOS */
	    /* raises both READABLE and EXCEPTION (for no */
	    /* apparent reason) when selecting on a plain file */
	    if (ready_mask & TCL_READABLE) {
		return EXP_DATA_NEW;
	    }

	    /* ready_mask must contain TCL_EXCEPTION */
#ifndef HAVE_PTYTRAP
	    return(EXP_EOF);
#else
	    if (ioctl(*master_out,TIOCREQCHECK,&ioctl_info) < 0) {
		exp_debuglog("ioctl error on TIOCREQCHECK: %s", Tcl_PosixError(interp));
		return(EXP_TCLERROR);
	    }
	    if (ioctl_info.request == TIOCCLOSE) {
		return(EXP_EOF);
	    }
	    if (ioctl(*master_out, TIOCREQSET, &ioctl_info) < 0) {
		exp_debuglog("ioctl error on TIOCREQSET after ioctl or open on slave: %s", Tcl_ErrnoMsg(errno));
	    }
	    /* presumably, we trapped an open here */
	    continue;
#endif /* !HAVE_PTYTRAP */
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * exp_get_next_event_info --
 *
 *	Having been told there was an event for a specific fd, get it
 *	returns status, one of EOF, TIMEOUT, ERROR or DATA
 *
 * Results:
 *	Returns EXP_DATA_NEW, EXP_EOF, of EXP_TCLERROR
 *
 * Side Effects: 
 *	None on NT or most Unices.  On HPUX, it looks like there might
 *	be some.
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
int
exp_get_next_event_info(interp,f,ready_mask)
    Tcl_Interp *interp;
    struct exp_f *f;
    int ready_mask;
{
#ifdef HAVE_PTYTRAP
    struct request_info ioctl_info;
#endif
    
    if (ready_mask & TCL_READABLE) return EXP_DATA_NEW;
    
    /* ready_mask must contain TCL_EXCEPTION */
    
#ifndef HAVE_PTYTRAP
    return(EXP_EOF);
#else
    if (ioctl(f->fd,TIOCREQCHECK,&ioctl_info) < 0) {
	exp_debuglog("ioctl error on TIOCREQCHECK: %s",
		     Tcl_PosixError(interp));
	return(EXP_TCLERROR);
    }
    if (ioctl_info.request == TIOCCLOSE) {
	return(EXP_EOF);
    }
    if (ioctl(f->fd, TIOCREQSET, &ioctl_info) < 0) {
	exp_debuglog("ioctl error on TIOCREQSET after ioctl or open on slave: %s", Tcl_ErrnoMsg(errno));
    }
    /* presumably, we trapped an open here */
    /* call it an error for lack of anything more descriptive */
    /* it will be thrown away by caller anyway */
    return EXP_TCLERROR;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * exp_dsleep --
 *
 *	Waits for at least a certain amount of time.  In general, 
 *	the length of time will be a little bit longer.
 *
 * Results:
 *	Returns TCL_OK;
 *
 * Side Effects: 
 *	Event handlers can fire during this period, so other actions
 *	are taken.
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
int	/* returns TCL_XXX */
exp_dsleep(interp,sec)
    Tcl_Interp *interp;
    double sec;
{
    int timer_fired = FALSE;

    Tcl_CreateTimerHandler((int)(sec*1000),exp_timehandler,(ClientData)&timer_fired);

    while (1) {
	Tcl_DoOneEvent(0);
	if (timer_fired) return TCL_OK;

	if (ready_fs == NULL) continue;

	exp_event_disarm_fast(ready_fs,exp_filehandler);
	ready_fs = NULL;
    }
}

/*
 * Tcl used to require commands to be in writeable memory.  This
 * probably doesn't apply anymore
 */

static char destroy_cmd[] = "destroy .";

/*
 *----------------------------------------------------------------------
 *
 * exp_event_exit_real --
 *
 *	Function to call to destroy the main window, causing
 *	the program to exit.
 *
 * Results:
 *	None
 *
 * Side Effects: 
 *	Program exits.
 *
 *----------------------------------------------------------------------
 */

static void
exp_event_exit_real(interp)
    Tcl_Interp *interp;
{
    Tcl_Eval(interp,destroy_cmd);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_init_event --
 *
 *	Set things up for later calls to the event handler
 *
 * Results:
 *	None
 *
 * Side Effects: 
 *	None
 *
 *----------------------------------------------------------------------
 */

void
exp_init_event()
{
    exp_event_exit = exp_event_exit_real;
}
