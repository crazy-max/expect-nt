/* exp_poll.c - This file contains UNIX specific procedures for
 * poll-based notifier, which is the lowest-level part of the Tcl
 * event loop.  This file works together with ../generic/tclNotify.c.
 *
 * Design and implementation of this program was paid for by U.S. tax
 * dollars.  Therefore it is public domain.  However, the author and
 * NIST would appreciate credit if this program or parts of it are
 * used.
 *
 * Written by Don Libes, NIST, 2/6/90
 * Rewritten by Don Libes, 2/96 for new Tcl notifier paradigm.
 *
 */

#include "tclInt.h"
#include "tclPort.h"
#include <signal.h> 

#include <poll.h>
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

/*
 * The information below is used to provide read, write, and
 * exception masks to select during calls to Tcl_DoOneEvent.
 */

/* Some systems require that the poll array be non-empty.  Alas, Tcl
 * provides no initialization opportunity to dynamically allocate it,
 * so provide a 1-elt array for starters.  It will be ignored as soon
 * as it grows larger.
 */

static struct pollfd initialFdArray;
static struct pollfd *fdArray = &initialFdArray;
static int fdsInUse = 0;	/* space in use */
static int fdsMaxSpace = 1;	/* space that has actually been allocated */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WatchFile --
 *
 *	Arrange for Tcl_DoOneEvent to include this file in the masks
 *	for the next call to select.  This procedure is invoked by
 *	event sources, which are in turn invoked by Tcl_DoOneEvent
 *	before it invokes select.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	
 *	The notifier will generate a file event when the I/O channel
 *	given by fd next becomes ready in the way indicated by mask.
 *	If fd is already registered then the old mask will be replaced
 *	with the new one.  Once the event is sent, the notifier will
 *	not send any more events about the fd until the next call to
 *	Tcl_NotifyFile. 
 *
 * Assumption for poll implementation: Tcl_WatchFile is presumed NOT
 * to be called on the same file descriptior without intervening calls
 * to Tcl_DoOneEvent.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_WatchFile(file, mask)
    Tcl_File file;	/* Generic file handle for a stream. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions to wait for
				 * in select. */
{
    int fd, type;
    int cur_fd_index = fdsInUse;

    fd = (int) Tcl_GetFileInfo(file, &type);

    if (type != TCL_UNIX_FD) {
	panic("Tcl_WatchFile: unexpected file type");
    }

    fdsInUse++;
    if (fdsInUse > fdsMaxSpace) {
	if (fdArray != &initialFdArray) ckfree((char *)fdArray);
	fdArray = (struct pollfd *)ckalloc(fdsInUse*sizeof(struct pollfd));
	fdsMaxSpace = fdsInUse;
    }

    fdArray[cur_fd_index].fd = fd;

    /* I know that POLLIN/OUT is right.  But I have no idea if POLLPRI
     * corresponds well to TCL_EXCEPTION.
     */

    if (mask & TCL_READABLE) {
        fdArray[cur_fd_index].events = POLLIN;
    }
    if (mask & TCL_WRITABLE) {
        fdArray[cur_fd_index].events = POLLOUT;
    }
    if (mask & TCL_EXCEPTION) {
        fdArray[cur_fd_index].events = POLLPRI;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_FileReady --
 *
 *	Indicates what conditions (readable, writable, etc.) were
 *	present on a file the last time the notifier invoked select.
 *	This procedure is typically invoked by event sources to see
 *	if they should queue events.
 *
 * Results:
 *	The return value is 0 if none of the conditions specified by mask
 *	was true for fd the last time the system checked.  If any of the
 *	conditions were true, then the return value is a mask of those
 *	that were true.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FileReady(file, mask)
    Tcl_File file;	/* Generic file handle for a stream. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions caller cares about. */
{
    int index, result, type, fd;
    fd_mask bit;

    fd = (int) Tcl_GetFileInfo(file, &type);
    if (type != TCL_UNIX_FD) {
	panic("Tcl_FileReady: unexpected file type");
    }

    result = 0;
    if ((mask & TCL_READABLE) && (fdArray[fd].revents & POLLIN)) {
	result |= TCL_READABLE;
    }
    if ((mask & TCL_WRITABLE) && (fdArray[fd].revents & POLLOUT)) {
	result |= TCL_WRITABLE;
    }
    /* I have no idea if this is right ... */
    if ((mask & TCL_EXCEPTION) &&
		(fdArray[fd].revents & (POLLPRI|POLLERR|POLLHUP|POLLNVAL))) {
	result |= TCL_EXCEPTION;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitForEvent --
 *
 *	This procedure does the lowest level wait for events in a
 *	platform-specific manner.  It uses information provided by
 *	previous calls to Tcl_WatchFile, plus the timePtr argument,
 *	to determine what to wait for and how long to wait.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May put the process to sleep for a while, depending on timePtr.
 *	When this procedure returns, an event of interest to the application
 *	has probably, but not necessarily, occurred.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_WaitForEvent(timePtr)
    Tcl_Time *timePtr;		/* Specifies the maximum amount of time
				 * that this procedure should block before
				 * returning.  The time is given as an
				 * interval, not an absolute wakeup time.
				 * NULL means block forever. */
{
    int timeout;
    struct timeval *timeoutPtr;

    /* no need to clear revents */
    if (timePtr == NULL) {
	timeout = -1;
    } else {
        timeout = timePtr->sec*1000 + timePtr->usec/1000;
    }
    poll(fdsInUse,fdArray,timeout);

    fdsInUse = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Sleep --
 *
 *	Delay execution for the specified number of milliseconds.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Time passes.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Sleep(ms)
    int ms;			/* Number of milliseconds to sleep. */
{
    static struct timeval delay;
    Tcl_Time before, after;

    /*
     * The only trick here is that select appears to return early
     * under some conditions, so we have to check to make sure that
     * the right amount of time really has elapsed.  If it's too
     * early, go back to sleep again.
     */

    TclGetTime(&before);
    after = before;
    after.sec += ms/1000;
    after.usec += (ms%1000)*1000;
    if (after.usec > 1000000) {
	after.usec -= 1000000;
	after.sec += 1;
    }
    while (1) {
	delay.tv_sec = after.sec - before.sec;
	delay.tv_usec = after.usec - before.usec;
	if (delay.tv_usec < 0) {
	    delay.tv_usec += 1000000;
	    delay.tv_sec -= 1;
	}

	/*
	 * Special note:  must convert delay.tv_sec to int before comparing
	 * to zero, since delay.tv_usec is unsigned on some platforms.
	 */

	if ((((int) delay.tv_sec) < 0)
		|| ((delay.tv_usec == 0) && (delay.tv_sec == 0))) {
	    break;
	}

	/* poll understands milliseconds, sigh */
	poll(0,fdArray,delay.tv_sec*1000 + delay.tv_usec/1000);
	TclGetTime(&before);
    }
}
