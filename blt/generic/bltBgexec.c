/*
 * bltBgexec.c --
 *
 *	This module implements a background "exec" command for the
 *	Tk toolkit.
 *
 * Copyright (c) 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef NO_BGEXEC

#define BGEXEC_VERSION "8.0"

#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/types.h>

#ifdef HAVE_WAITFLAGS_H
#   include <waitflags.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#   include <sys/wait.h>
#endif

#if TCL_MAJOR_VERSION < 8
#   define BLT_TCL_PIDPTR
#   define BLT_TCL_PIDPTRPTR
#   ifdef HAVE_MALLOC_H
#	include <malloc.h>
#   endif /* HAVE_MALLOC_H */
#   undef ckfree
#   undef ckalloc
#   ifdef TCL_MEM_DEBUG
#       define ckfree Tcl_Ckfree
#       define ckalloc Tcl_Ckalloc
        extern char *ckstrdup(char *);
        extern void *ckcalloc(size_t,size_t);
#   else
#       define ckfree free
#       define ckalloc malloc
#       define ckcalloc calloc
#   endif
#   if !defined(TCL_MEM_DEBUG) && defined(HAVE_STRDUP)
#       define ckstrdup strdup
#   endif

#   if TCL_MAJOR_VERSION == 7 && TCL_MINOR_VERSION == 6
	extern int TclCreatePipeline _ANSI_ARGS_((Tcl_Interp *interp,
	    int argc, char **argv, int **pidArrayPtr,
	    Tcl_File *inPipePtr, Tcl_File *outPipePtr, Tcl_File *errFilePtr));
#   endif /* TCL7.6 */
#elif TCL80B2_OR_BETTER
#   define BLT_TCL_PIDPTR (Tcl_Pid *)
#   define BLT_TCL_PIDPTRPTR (Tcl_Pid **)
#   undef Tk_CreateFileHandler
#   define Tk_CreateFileHandler Tcl_CreateFileHandler
#   undef Tk_DeleteFileHandler
#   define Tk_DeleteFileHandler Tcl_DeleteFileHandler
#   define Tcl_File TclFile
#   define Tcl_GetFileInfo(file,typePtr) (((int)file)-1)
#   define TclpReleaseFile(file)
#   define Tcl_FreeFile(file)
#endif /* TCL80B2_OR_BETTER */

/* The wait-related definitions are taken from tclUnix.h */

/*
 * Not all systems declare the errno variable in errno.h. so this
 * file does it explicitly.  The list of system error messages also
 * isn't generally declared in a header file anywhere.
 */

extern int errno;

/*
 * The type of the status returned by wait varies from UNIX system
 * to UNIX system.  The macro below defines it:
 */

#ifdef AIX
#   define WAIT_STATUS_TYPE pid_t
#else
#   ifndef NO_UNION_WAIT
#	define WAIT_STATUS_TYPE union wait
#   else
#	define WAIT_STATUS_TYPE int
#   endif
#endif

/*
 * Supply definitions for macros to query wait status, if not already
 * defined in header files above.
 */

#ifndef WIFEXITED
#   define WIFEXITED(stat)  (((*((int *) &(stat))) & 0xff) == 0)
#endif

#ifndef WEXITSTATUS
#   define WEXITSTATUS(stat) (((*((int *) &(stat))) >> 8) & 0xff)
#endif

#ifndef WIFSIGNALED
#   define WIFSIGNALED(stat) (((*((int *) &(stat)))) \
	&& ((*((int *) &(stat))) == ((*((int *) &(stat))) & 0x00ff)))
#endif

#ifndef WTERMSIG
#   define WTERMSIG(stat)    ((*((int *) &(stat))) & 0x7f)
#endif

#ifndef WIFSTOPPED
#   define WIFSTOPPED(stat)  (((*((int *) &(stat))) & 0xff) == 0177)
#endif

#ifndef WSTOPSIG
#   define WSTOPSIG(stat)    (((*((int *) &(stat))) >> 8) & 0xff)
#endif


#define TRACE_FLAGS (TCL_TRACE_WRITES | TCL_TRACE_UNSETS | TCL_GLOBAL_ONLY)

#define BUFFER_SIZE	1000	/* Maximum number of bytes per read */
#define MAX_READS       100	/* Maximum number of successful reads
			         * before stopping to let Tk catch up
			         * on events */

/*
 * Map numeric signal codes to their stringified equivalents.
 */

#ifndef NSIG
#   define NSIG 	32	/* Number of signals available */
#endif

typedef struct {
    int number;
    char *name;
} SignalId;

static SignalId signalIds[] = {
#ifdef SIGABRT
    {SIGABRT, "SIGABRT"},
#endif
#ifdef SIGALRM
    {SIGALRM, "SIGALRM"},
#endif
#ifdef SIGBUS
    {SIGBUS, "SIGBUS"},
#endif
#ifdef SIGCHLD
    {SIGCHLD, "SIGCHLD"},
#endif
#if defined(SIGCLD) && (!defined(SIGCHLD) || (SIGCLD != SIGCHLD))
    {SIGCLD, "SIGCLD"},
#endif
#ifdef SIGCONT
    {SIGCONT, "SIGCONT"},
#endif
#if defined(SIGEMT) && (!defined(SIGXCPU) || (SIGEMT != SIGXCPU))
    {SIGEMT, "SIGEMT"},
#endif
#ifdef SIGFPE
    {SIGFPE, "SIGFPE"},
#endif
#ifdef SIGHUP
    {SIGHUP, "SIGHUP"},
#endif
#ifdef SIGILL
    {SIGILL, "SIGILL"},
#endif
#ifdef SIGINT
    {SIGINT, "SIGINT"},
#endif
#ifdef SIGIO
    {SIGIO, "SIGIO"},
#endif
#if defined(SIGIOT) && (!defined(SIGABRT) || (SIGIOT != SIGABRT))
    {SIGIOT, "SIGIOT"},
#endif
#ifdef SIGKILL
    {SIGKILL, "SIGKILL"},
#endif
#if defined(SIGLOST) && (!defined(SIGIOT) || (SIGLOST != SIGIOT)) \
	&& (!defined(SIGURG) || (SIGLOST != SIGURG))
    {SIGLOST, "SIGLOST"},
#endif
#ifdef SIGPIPE
    {SIGPIPE, "SIGPIPE"},
#endif
#if defined(SIGPOLL) && (!defined(SIGIO) || (SIGPOLL != SIGIO))
    {SIGPOLL, "SIGPOLL"},
#endif
#ifdef SIGPROF
    {SIGPROF, "SIGPROF"},
#endif
#if defined(SIGPWR) && (!defined(SIGXFSZ) || (SIGPWR != SIGXFSZ))
    {SIGPWR, "SIGPWR"},
#endif
#ifdef SIGQUIT
    {SIGQUIT, "SIGQUIT"},
#endif
#ifdef SIGSEGV
    {SIGSEGV, "SIGSEGV"},
#endif
#ifdef SIGSTOP
    {SIGSTOP, "SIGSTOP"},
#endif
#ifdef SIGSYS
    {SIGSYS, "SIGSYS"},
#endif
#ifdef SIGTERM
    {SIGTERM, "SIGTERM"},
#endif
#ifdef SIGTRAP
    {SIGTRAP, "SIGTRAP"},
#endif
#ifdef SIGTSTP
    {SIGTSTP, "SIGTSTP"},
#endif
#ifdef SIGTTIN
    {SIGTTIN, "SIGTTIN"},
#endif
#ifdef SIGTTOU
    {SIGTTOU, "SIGTTOU"},
#endif
#if defined(SIGURG) && (!defined(SIGIO) || (SIGURG != SIGIO))
    {SIGURG, "SIGURG"},
#endif
#if defined(SIGUSR1) && (!defined(SIGIO) || (SIGUSR1 != SIGIO))
    {SIGUSR1, "SIGUSR1"},
#endif
#if defined(SIGUSR2) && (!defined(SIGURG) || (SIGUSR2 != SIGURG))
    {SIGUSR2, "SIGUSR2"},
#endif
#ifdef SIGVTALRM
    {SIGVTALRM, "SIGVTALRM"},
#endif
#ifdef SIGWINCH
    {SIGWINCH, "SIGWINCH"},
#endif
#ifdef SIGXCPU
    {SIGXCPU, "SIGXCPU"},
#endif
#ifdef SIGXFSZ
    {SIGXFSZ, "SIGXFSZ"},
#endif
    {-1, "unknown signal"},
};

static int SignalParseProc _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
    int offset));
static char *SignalPrintProc _ANSI_ARGS_((ClientData clientData,
    Tk_Window tkwin, char *widgRec, int offset, Tcl_FreeProc **freeProcPtr));

static Tk_CustomOption signalOption = {
    SignalParseProc, SignalPrintProc, (ClientData)0
};

typedef struct {
    char *storage;		/* Buffer to store command output
				 * (malloc-ed): Initially points to
				 * static storage */
    int used;			/* Number of characters read into the
				 * buffer */
    int size;			/* Size of buffer allocated */
    char staticSpace[BUFFER_SIZE * 2 + 1];	/* Static buffer space */

} Buffer;

typedef struct {
    Tk_Window tkwin;		/* Main window of interpreter. Used for
				 * handling options */
    Tcl_Interp *interp;		/* Interpreter containing variable */

    char *updateVar;		/* Name of a Tcl variable (malloc'ed)
				 * to be updated when no more data is
				 * currently available for reading
				 * from the output pipe.  It's
				 * appended with the contents of the
				 * current buffer (data which has
				 * arrived since the last idle
				 * point). If it's NULL, no updates
				 * are made */

    char *outVar;		/* Name of a Tcl variable (malloc'ed)
				 * to be set with the contents of
				 * stdout after the last UNIX
				 * subprocess has completed. */

    char *errVar;		/* Name of a Tcl variable (malloc'ed)
				 * to hold any available data from
				 * stderr */

    char *statVar;		/* Name of a Tcl variable (malloc'ed)
				 * to hold exit status of the last
				 * process. Setting this variable
				 * triggers the termination of all
				 * subprocesses, regardless whether
				 * they have already completed or not */

    Tk_TimerToken timerToken;	/* Token for timer handler which polls
				 * for the exit status of each
				 * sub-process. If zero, there's no
				 * timer handler queued. */

    int outputId;		/* File descriptor for output pipe.  */
    int errorId;		/* File Descriptor for error file. */

    Buffer buffer;		/* Buffer storing subprocess' stdin/stderr */

    int numPids;		/* Number of processes created in pipeline */
    int *pidArr;		/* Array of process Ids. */

    int *exitCodePtr;		/* Pointer to a variable to hold exit code */
    int *donePtr;

    int killSignal;		/* If non-zero, indicates signal to send
				 * subprocesses when cleaning up.*/
    int keepTrailingNewLine;	/* If non-zero, indicates to set Tcl output
				 * variables with trailing newlines
				 * intact */
    int lastCount;		/* Number of bytes read the last time a
				 * buffer was retrieved */
    int fixMark;		/* Index of fixed newline character in
				 * buffer.  If -1, no fix was made. */

} BackgroundInfo;


static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_STRING, "-output", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(BackgroundInfo, outVar),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-update", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(BackgroundInfo, updateVar),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-error", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(BackgroundInfo, errVar),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-keepnewline", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(BackgroundInfo, keepTrailingNewLine), 0},
    {TK_CONFIG_CUSTOM, "-killsignal", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(BackgroundInfo, killSignal), 0, &signalOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};


/*
 *----------------------------------------------------------------------
 *
 * SignalParseProc --
 *
 *	Convert a string represent a signal number into its integer
 *	value.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SignalParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* Signal number */
    char *widgRec;		/* Background info record */
    int offset;			/* Offset of vector in Element record */
{
    int *signalPtr = (int *)(widgRec + offset);
    int killSignal;

    if ((value == NULL) || (*value == '\0')) {
	*signalPtr = 0;
	return TCL_OK;
    }
    if (isdigit(value[0])) {
	if (Tcl_GetInt(interp, value, &killSignal) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	char *name;
	register SignalId *sigPtr;

	name = value;

	/*  Clip off any "SIG" prefix from the signal name */
	if ((value[0] == 'S') && (value[1] == 'I') && (value[2] == 'G')) {
	    name += 3;
	}
	killSignal = -1;
	for (sigPtr = signalIds; sigPtr->number > 0; sigPtr++) {
	    if (strcmp(sigPtr->name + 3, name) == 0) {
		killSignal = sigPtr->number;
		break;
	    }
	}
	if (killSignal < 0) {
	    Tcl_AppendResult(interp, "unknown signal \"", value, "\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
    }
    if ((killSignal < 1) || (killSignal > NSIG)) {
	char string[200];

	/* Outside range of signals */
	sprintf(string, "bad signal number \"%s\": should be between 1 and %d",
	    value, NSIG);
	Tcl_AppendResult(interp, string, (char *)NULL);
	return TCL_ERROR;
    }
    *signalPtr = killSignal;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SignalPrintProc --
 *
 *	Convert the integer signal number into an ASCII string.
 *
 * Results:
 *	The string representation of the kill signal is returned.
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static char *
SignalPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* BackgroundInfo record */
    int offset;			/* Offset of signal number in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    int killSignal = *(int *)(widgRec + offset);

    if (killSignal == 0) {
	return "";
    } else {
	char *result;
	char string[20];

	sprintf(string, "%d", killSignal);
	*freeProcPtr = (Tcl_FreeProc *)ckfree;
	result = ckstrdup(string);
	return (result);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetBuffer --
 *
 *	Returns the output currently saved in buffer storage
 *
 *----------------------------------------------------------------------
 */
static char *
GetBuffer(bufferPtr)
    Buffer *bufferPtr;
{
    bufferPtr->storage[bufferPtr->used] = '\0';
    return (bufferPtr->storage);
}

/*
 *----------------------------------------------------------------------
 *
 * FlushBuffer --
 *
 *	Dump the contents of the output currently saved in buffer
 *	storage.  This is used when we don't want to save the entire
 *	of output from the pipeline.
 *
 *----------------------------------------------------------------------
 */

static void
FlushBuffer(bufferPtr)
    Buffer *bufferPtr;
{
    bufferPtr->used = 0;
    bufferPtr->storage[0] = '\0';
}

/*
 *----------------------------------------------------------------------
 *
 * InitBuffer --
 *
 *	Initializes the buffer storage, clearing any output that may
 *	have accumulated from previous usage.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Buffer storage is cleared.
 *
 *----------------------------------------------------------------------
 */
static void
InitBuffer(bufferPtr)
    Buffer *bufferPtr;
{
    bufferPtr->storage = bufferPtr->staticSpace;
    bufferPtr->size = BUFFER_SIZE * 2;
    bufferPtr->storage[0] = '\0';
    bufferPtr->used = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ResetBuffer --
 *
 *	Resets the buffer storage, freeing any malloc'ed space.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
ResetBuffer(bufferPtr)
    Buffer *bufferPtr;
{
    if (bufferPtr->storage != bufferPtr->staticSpace) {
	ckfree(bufferPtr->storage);
    }
    InitBuffer(bufferPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * GrowBuffer --
 *
 *	Doubles the size of the current buffer.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static int
GrowBuffer(bufferPtr)
    Buffer *bufferPtr;
{
    char *newPtr;

    /*
     * Allocate a new buffer, double the old size
     */

    bufferPtr->size += bufferPtr->size;
    newPtr = (char *)ckalloc(sizeof(char) * (bufferPtr->size + 1));
    if (newPtr == NULL) {
	return TCL_ERROR;
    }
    strcpy(newPtr, bufferPtr->storage);
    if (bufferPtr->storage != bufferPtr->staticSpace) {
	ckfree((char *)bufferPtr->storage);
    }
    bufferPtr->storage = newPtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AppendOutputToBuffer --
 *
 *	Appends any available data from a given file descriptor to the
 *	buffer.
 *
 * Results:
 *	Returns TCL_OK when EOF is found, TCL_RETURN if reading
 *	data would block, and TCL_ERROR if an error occurred.
 *
 *----------------------------------------------------------------------
 */
static int
AppendOutputToBuffer(f, bufferPtr)
    int f;
    Buffer *bufferPtr;
{
    int numBytes, bytesLeft;
    register int i, n;
    char *array;

    /*
     * ------------------------------------------------------------------
     *
     * 	Worry about indefinite postponement.
     *
     * 	Typically we want to stay in the read loop as long as it takes
     * 	to collect all the data that's currently available.  But if
     * 	it's coming in at a constant high rate, we need to arbitrarily
     * 	break out at some point. This allows for both setting the
     * 	update variable and the Tk program to handle idle events.
     *
     * ------------------------------------------------------------------
     */

    for (i = 0; i < MAX_READS; i++) {

	/*
	 * Allocate a larger buffer when the number of remaining bytes
	 * is below a threshold (BUFFER_SIZE).
	 */

	bytesLeft = bufferPtr->size - bufferPtr->used;
	if (bytesLeft < BUFFER_SIZE) {
	    if (GrowBuffer(bufferPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    bytesLeft = bufferPtr->size - bufferPtr->used;
	}
	array = bufferPtr->storage + bufferPtr->used;
	numBytes = read(f, array, bytesLeft);

	if (numBytes == 0) {	/* EOF: break out of loop. */
	    return TCL_OK;
	}
	if (numBytes < 0) {

	    /*
	     * Either an error has occurred or no more data is
	     * currently available to read.
	     */
#ifdef O_NONBLOCK
	    if (errno == EAGAIN) {
#else
	    if (errno == EWOULDBLOCK) {
#endif /*O_NONBLOCK*/
		break;
	    }
	    bufferPtr->storage[0] = '\0';
	    return TCL_ERROR;
	}
	/* Clean out NUL bytes, make spaces */
	for (n = 0; n < numBytes; n++) {
	    if (array[n] == 0) {
		array[n] = ' ';
	    }
	}
	bufferPtr->used += numBytes;
	bufferPtr->storage[bufferPtr->used] = '\0';
    }
    return TCL_RETURN;
}

/*
 *----------------------------------------------------------------------
 *
 * FixNewline --
 *
 *	Clips off the trailing newline in the buffer (if one exists).
 *	Saves the location in the buffer where the fix was made.
 *
 *----------------------------------------------------------------------
 */
static void
FixNewline(bgPtr)
    BackgroundInfo *bgPtr;
{
    Buffer *bufferPtr = &(bgPtr->buffer);

    bgPtr->fixMark = -1;
    if (bufferPtr->used > 0) {
	int mark = bufferPtr->used - 1;

	if (bufferPtr->storage[mark] == '\n') {
	    bufferPtr->storage[mark] = '\0';
	    bgPtr->fixMark = mark;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UnfixNewline --
 *
 *	Restores the previously clipped newline in the buffer.
 *	The fixMark field indicates whether one was clipped.
 *
 *----------------------------------------------------------------------
 */
static void
UnfixNewline(bgPtr)
    BackgroundInfo *bgPtr;
{
    Buffer *bufferPtr = &(bgPtr->buffer);

    if (bgPtr->fixMark >= 0) {
	bufferPtr->storage[bgPtr->fixMark] = '\n';
	bgPtr->fixMark = -1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetLastAppended --
 *
 *	Returns the output saved from the last time this routine
 *	was called.
 *
 *----------------------------------------------------------------------
 */
static char *
GetLastAppended(bgPtr)
    BackgroundInfo *bgPtr;
{
    Buffer *bufferPtr = &(bgPtr->buffer);
    char *string;

    bufferPtr->storage[bufferPtr->used] = '\0';
    string = bufferPtr->storage + bgPtr->lastCount;
    bgPtr->lastCount = bufferPtr->used;
    return (string);
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyBackgroundInfo --
 *
 * 	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 * 	to clean up the internal structure (BackgroundInfo) at a safe
 * 	time (when no-one is using it anymore).
 *
 *	Right now, our only concern is protecting bgPtr->outVar,
 *	since subsequent calls to trace procedures (via CallTraces)
 *	may still use it (as part1 and possibly part2).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The memory allocated to the BackgroundInfo structure released.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
static void
DestroyBackgroundInfo(clientData)
    ClientData clientData;	/* Background info record. */
{
    BackgroundInfo *bgPtr = (BackgroundInfo *)clientData;

    ResetBuffer(&(bgPtr->buffer));
    Tk_FreeOptions(configSpecs, (char *)bgPtr, Tk_Display(bgPtr->tkwin), 0);
    if (bgPtr->statVar != NULL) {
	ckfree(bgPtr->statVar);
    }
    if (bgPtr->pidArr != NULL) {
	if (bgPtr->numPids > 0) {
	    Tcl_DetachPids(bgPtr->numPids, BLT_TCL_PIDPTR bgPtr->pidArr);
	}
	ckfree((char *)bgPtr->pidArr);
    }
    Tcl_ReapDetachedProcs();
    ckfree((char *)bgPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * CleanupProc --
 *
 *	This procedure cleans up the BackgroundInfo data structure
 *	associated with the detached subprocesses.  It is called when
 *	the variable associated with UNIX subprocesses has been
 *	overwritten.  This usually occurs when the subprocesses have
 *	completed or an error was detected.  However, it may be used
 *	to terminate the detached processes from the Tcl program by
 *	setting the associated variable.
 *
 * Results:
 *	Always returns NULL.  Only called from a variable trace.
 *
 * Side effects:
 *	The output descriptor is closed and the variable trace is
 *	deleted.  In addition, the subprocesses are signaled for
 *	termination.
 *
 * ----------------------------------------------------------------------
 */
static char *
CleanupProc(clientData, interp, part1, part2, flags)
    ClientData clientData;	/* File output information. */
    Tcl_Interp *interp;
    char *part1, *part2;
    int flags;
{
    BackgroundInfo *bgPtr = (BackgroundInfo *)clientData;

    if ((flags & TRACE_FLAGS) == 0) {
	return NULL;
    }
    if (bgPtr->outputId != -1) {
	close(bgPtr->outputId);
	Tk_DeleteFileHandler(bgPtr->outputId);
    }
    if (bgPtr->timerToken != (Tk_TimerToken) 0) {
	Tk_DeleteTimerHandler(bgPtr->timerToken);
    }
    if (bgPtr->donePtr != NULL) {
	*bgPtr->donePtr = TRUE;
    }
    if (bgPtr->errorId >= 0) {
	if (bgPtr->errVar != NULL) {
	    /*
	     * If an error variable needs to be set, rewind the error
	     * file and read the captured stderr from the temporary file
	     */
	    if (lseek(bgPtr->errorId, 0L, 0) < 0) {
		Tcl_AppendResult(bgPtr->interp, "can't rewind stderr: ",
		    Tcl_PosixError(bgPtr->interp), (char *)NULL);
		Tk_BackgroundError(bgPtr->interp);
	    } else {
		int status;

		ResetBuffer(&(bgPtr->buffer));
		do {
		    status = AppendOutputToBuffer(bgPtr->errorId,
			&(bgPtr->buffer));
		} while (status == TCL_RETURN);
		if (status == TCL_OK) {
		    if (!bgPtr->keepTrailingNewLine) {
			FixNewline(bgPtr);
		    }
		    if (Tcl_SetVar(bgPtr->interp, bgPtr->errVar,
			    GetBuffer(&(bgPtr->buffer)), TCL_GLOBAL_ONLY)
			    == NULL) {
			Tk_BackgroundError(bgPtr->interp);
		    }
		} else if (status == TCL_ERROR) {
		    Tcl_AppendResult(bgPtr->interp,
			"can't append to error buffer: ",
			Tcl_PosixError(bgPtr->interp), (char *)NULL);
		    Tk_BackgroundError(bgPtr->interp);
		}
	    }
	}
	close(bgPtr->errorId);
    }
    Tcl_UntraceVar2(interp, part1, part2, TRACE_FLAGS, CleanupProc, clientData);

    if ((bgPtr->pidArr != NULL) && (bgPtr->killSignal > 0)) {
	register int i;

	for (i = 0; i < bgPtr->numPids; i++) {
	    kill(bgPtr->pidArr[i], (int)bgPtr->killSignal);
	}
    }
    Tcl_EventuallyFree((ClientData)bgPtr,
        (Tcl_FreeProc *)DestroyBackgroundInfo);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * StatusProc --
 *
 *	This is a timer handler procedure which gets called
 *	periodically to reap any of the sub-processes if they have
 *	terminated.  After the last process has terminated, the
 *	contents of standard output (saved in bgPtr->buffer) are
 *	stored in the output variable, which triggers the cleanup
 *	proc (using a variable trace). If the status variable is
 *	active (bgPtr->statVar != NULL), then set the status the
 *	last process to exit in the status variable.
 *
 * Results:
 *	None.  Called from the Tk event loop.
 *
 * Side effects:
 *	Many. The contents of pidArr is shifted, leaving only those
 *	sub-processes which have not yet terminated.  If there are
 *	still subprocesses left, this procedure is placed in the timer
 *	queue again. Otherwise the output and possibly the status
 *	variables are updated.  The former triggers the cleanup
 *	routine which will destroy the information and resources
 *	associated with these background processes.
 *
 *----------------------------------------------------------------------
 */
static void
StatusProc(clientData)
    ClientData clientData;
{
    BackgroundInfo *bgPtr = (BackgroundInfo *)clientData;
    register int i;
    int code;
    int lastPid;
    WAIT_STATUS_TYPE waitStatus, lastStatus;
    int numLeft;		/* Number of processes still not reaped */

    lastPid = -1;
    *((int *)&waitStatus) = 0;
    *((int *)&lastStatus) = 0;
    numLeft = 0;
    for (i = 0; i < bgPtr->numPids; i++) {
	code = waitpid(bgPtr->pidArr[i], (int *)&waitStatus, WNOHANG);
	if (code == 0) {	/*  Process has not terminated yet */
	    if (numLeft < i) {
		bgPtr->pidArr[numLeft] = bgPtr->pidArr[i];
	    }
	    numLeft++;
	} else if (code > 0) {
	    /*
	     * Save the status information associated with the subprocess.
	     * We'll use it only if this is the last subprocess to be reaped.
	     */
	    lastStatus = waitStatus;
	    lastPid = bgPtr->pidArr[i];
	}
    }
    bgPtr->numPids = numLeft;

    if (numLeft > 0) {
	/* Keep polling for the status of the children that are left */
	bgPtr->timerToken = Tk_CreateTimerHandler(1000, StatusProc,
	    (ClientData)bgPtr);
    } else {
	char *merged;		/* List formed by merging msgArr components */
	char codeStr[20];
	char pidStr[20];
	char textStr[200];
	int exitCode;
	enum StatusComponents {
	    STATUS_TOKEN, STATUS_PID, STATUS_EXITCODE, STATUS_TEXT
	};
	char *msgArr[4];
	char *result;

	/* If output is to be collected, set the output variable with
	 * the contents of the buffer */
	if (bgPtr->outVar != NULL) {
	    result = Tcl_SetVar(bgPtr->interp, bgPtr->outVar,
		GetBuffer(&(bgPtr->buffer)), TCL_GLOBAL_ONLY);
	    if (result == NULL) {
		Tk_BackgroundError(bgPtr->interp);
	    }
	}
	/*
	 * Set the status variable with the status of the last process
	 * reaped.  The status is a list of an error token, the exit
	 * status, and a message.
	 */
	exitCode = WEXITSTATUS(lastStatus);
	if (WIFEXITED(lastStatus)) {
	    msgArr[STATUS_TOKEN] = "CHILDSTATUS";
	    msgArr[STATUS_TEXT] = "child completed";
	} else if (WIFSIGNALED(lastStatus)) {
	    msgArr[STATUS_TOKEN] = "CHILDKILLED";
	    msgArr[STATUS_TEXT] = Tcl_SignalMsg((int)(WTERMSIG(lastStatus)));
	    exitCode = -1;
	} else if (WIFSTOPPED(lastStatus)) {
	    msgArr[STATUS_TOKEN] = "CHILDSUSP";
	    msgArr[STATUS_TEXT] = Tcl_SignalMsg((int)(WSTOPSIG(lastStatus)));
	    exitCode = -1;
	} else {
	    msgArr[STATUS_TOKEN] = "UNKNOWN";
	    sprintf(textStr, "child completed with unknown status 0x%x",
		*((int *)&lastStatus));
	    msgArr[STATUS_TEXT] = textStr;
	}
	sprintf(pidStr, "%d", lastPid);
	sprintf(codeStr, "%d", exitCode);
	msgArr[STATUS_PID] = pidStr;
	msgArr[STATUS_EXITCODE] = codeStr;
	if (bgPtr->exitCodePtr != NULL) {
	    *bgPtr->exitCodePtr = exitCode;
	}
	merged = Tcl_Merge(4, msgArr);
	Tcl_Preserve((ClientData)bgPtr);


	/*
	 * Setting the status variable also triggers the cleanup
	 * procedure which frees memory and destroys the variable
	 * traces.
	 */
	result = Tcl_SetVar(bgPtr->interp, bgPtr->statVar, merged,
	    TCL_GLOBAL_ONLY);

	ckfree(merged);
	if (result == NULL) {
	    Tk_BackgroundError(bgPtr->interp);
	}
	Tcl_Release((ClientData)bgPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * BackgroundProc --
 *
 *	This procedure is called when output from the detached command
 *	is available.  The output is read and saved in a buffer in the
 *	BackgroundInfo structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Data is stored in bgPtr->buffer.  This character array may
 *	be increased as more space is required to contain the output
 *	of the command.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
static void
BackgroundProc(clientData, mask)
    ClientData clientData;	/* File output information. */
    int mask;			/* Not used. */
{
    BackgroundInfo *bgPtr = (BackgroundInfo *)clientData;
    int status;
    int flags;

    /*
     * If there is no output variable (the user may be using -update
     * only), there's no need to accumulate the output in memory.
     * Reset the counters so we only keep what's last read.
     */
    if (bgPtr->outVar == NULL) {
	FlushBuffer(&(bgPtr->buffer));
	bgPtr->lastCount = 0;
    }
    flags = (TCL_GLOBAL_ONLY | TCL_APPEND_VALUE | TCL_LEAVE_ERR_MSG);
    status = AppendOutputToBuffer(bgPtr->outputId, &(bgPtr->buffer));
    if (status == TCL_RETURN) {
	if (bgPtr->updateVar != NULL) {
	    char *result;

	    if (!bgPtr->keepTrailingNewLine) {
		FixNewline(bgPtr);
		result = Tcl_SetVar(bgPtr->interp, bgPtr->updateVar,
		    GetLastAppended(bgPtr), flags);
		UnfixNewline(bgPtr);
	    } else {
		result = Tcl_SetVar(bgPtr->interp, bgPtr->updateVar,
		    GetLastAppended(bgPtr), flags);
	    }
	    if (result == NULL) {
		Tk_BackgroundError(bgPtr->interp);
	    }
	}
	return;
    }
    if (status == TCL_ERROR) {
	Tcl_AppendResult(bgPtr->interp, "error appending buffer: ",
	    Tcl_PosixError(bgPtr->interp), (char *)NULL);
	Tk_BackgroundError(bgPtr->interp);
    }
    if (!bgPtr->keepTrailingNewLine) {
	FixNewline(bgPtr);
    }
    if (bgPtr->updateVar != NULL) {
	char *result;

	result = Tcl_SetVar(bgPtr->interp, bgPtr->updateVar,
	    GetLastAppended(bgPtr), flags);
	if (result == NULL) {
	    Tk_BackgroundError(bgPtr->interp);
	}
    }
    /*
     * We're here if we've seen EOF or an error has occurred.  In
     * either case, set up a timer handler to periodically poll for
     * exit status of each process.  Initially check at the next idle
     * interval.
     */

    bgPtr->timerToken = Tk_CreateTimerHandler(0, StatusProc, (ClientData)bgPtr);

    /* Delete the file handler and close the file descriptor. */

    close(bgPtr->outputId);
    Tk_DeleteFileHandler(bgPtr->outputId);
    bgPtr->outputId = -1;
}

/*
 *----------------------------------------------------------------------
 *
 * BltCreatePipeline --
 *
 *    This function is a compatibility wrapper for TclCreatePipeline.
 *    It is only available under Unix, and may be removed from later
 *    versions.
 *
 * Results:
 *    Same as TclCreatePipeline.
 *
 * Side effects:
 *    Same as TclCreatePipeline.
 *
 *----------------------------------------------------------------------
 */

static int
BltCreatePipeline(interp, argc, argv, pidArrayPtr, inPipePtr,
      outPipePtr, errFilePtr)
    Tcl_Interp *interp;
    int argc;
    char **argv;
    int **pidArrayPtr;
    int *inPipePtr;
    int *outPipePtr;
    int *errFilePtr;
{
    Tcl_File inFile, outFile, errFile;
    int result;

    result = TclCreatePipeline(interp, argc, argv,
	BLT_TCL_PIDPTRPTR pidArrayPtr,
	(inPipePtr ? &inFile : NULL),
	(outPipePtr ? &outFile : NULL),
	(errFilePtr ? &errFile : NULL));

    if (inPipePtr) {
	if (inFile) {
	    *inPipePtr = (int) Tcl_GetFileInfo(inFile, NULL);
	    Tcl_FreeFile(inFile);
	} else {
	    *inPipePtr = -1;
	}
    }
    if (outPipePtr) {
	if (outFile) {
	    *outPipePtr = (int) Tcl_GetFileInfo(outFile, NULL);
	    Tcl_FreeFile(outFile);
	} else {
	    *outPipePtr = -1;
	}
    }
    if (errFilePtr) {
	if (errFile) {
	    *errFilePtr = (int) Tcl_GetFileInfo(errFile, NULL);
	    Tcl_FreeFile(errFile);
	} else {
	    *errFilePtr = -1;
	}
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_BgExecCmd --
 *
 *	This procedure is invoked to process the "bgexec" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

 /* ARGSUSED */
static int
BgExecCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    int *errFilePtr;
    int numPids;
    char *lastArg;
    int detach;
    BackgroundInfo *bgPtr;
    int i;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " varName ?options? command ?arg...?\"", (char *)NULL);
	return TCL_ERROR;
    }
    bgPtr = (BackgroundInfo *)ckcalloc(1, sizeof(BackgroundInfo));
    if (bgPtr == NULL) {
	Panic("can't allocate file info structure");
    }
    /* Initialize the background information record */
    bgPtr->interp = interp;
    bgPtr->killSignal = SIGKILL;
    bgPtr->numPids = -1;
    bgPtr->outputId = bgPtr->errorId = -1;
    InitBuffer(&(bgPtr->buffer));

    /*
     * Check if the command is to be run detached (indicated a '&' as
     * the last argument of the command)
     */
    lastArg = argv[argc - 1];
    detach = ((lastArg[0] == '&') && (lastArg[1] == '\0'));
    if (detach) {
	argc--;
	argv[argc] = NULL;	/* Remove the '&' argument */
    }
    for (i = 2; i < argc; i += 2) {
	/* Count the number of option-value pairs */
	if ((argv[i][0] != '-') || (argv[i][1] == '-')) {
	    break;		/* Not an option or "--" */
	}
    }
    if (i > argc) {
	i = argc;
    }
    bgPtr->tkwin = (Tk_Window)clientData;	/* Main window of interpreter */
    if (Tk_ConfigureWidget(interp, bgPtr->tkwin, configSpecs, i - 2, argv + 2,
	    (char *)bgPtr, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    if (argc <= i) {
	Tcl_AppendResult(interp, "missing command to execute: should be \"",
	    argv[0], " varName ?options? command ?arg...?\"", (char *)NULL);
      errorOnSetup:
	Tcl_ReapDetachedProcs();/* Try to clean up any detached processes */
	Tk_FreeOptions(configSpecs, (char *)bgPtr, Tk_Display(bgPtr->tkwin), 0);
	if (bgPtr != NULL) {
	    ckfree((char *)bgPtr);
	}
	return TCL_ERROR;
    }
    if (argv[i][0] == '-') {
	i++;			/* If the last option was "--", skip it. */
    }
    /* By default, stderr goes to the tty. */
    errFilePtr = NULL;
    if (bgPtr->errVar != NULL) {
	errFilePtr = &(bgPtr->errorId);	/* Error variable designated */
    }
    numPids = BltCreatePipeline(interp, argc - i, argv + i, &(bgPtr->pidArr),
	(int *)NULL, &(bgPtr->outputId), errFilePtr);

    if (numPids < 0) {
	goto errorOnSetup;
    }
    bgPtr->numPids = numPids;
    bgPtr->lastCount = 0;
    bgPtr->fixMark = -1;
    bgPtr->statVar = ckstrdup(argv[1]);

    /*
     * Put a trace on the status variable.  This also allows the
     * user to prematurely terminate the pipeline of subprocesses.
     */
    Tcl_TraceVar(interp, bgPtr->statVar, TRACE_FLAGS, CleanupProc,
	(ClientData)bgPtr);

    if (bgPtr->outputId == -1) {

	/*
	 * If output has been redirected, start polling immediately
	 * for the exit status of each process.  Normally, this is
	 * done only after stdout output has been closed by the last
	 * process.  I'm guessing about the timer interval.  Right
	 * now we poll every 1 second.
	 */

	bgPtr->timerToken = Tk_CreateTimerHandler(1000, StatusProc,
	    (ClientData)bgPtr);
    } else {

	/*
	 * Make the output descriptor non-blocking and associate it
	 * with a file handler routine.  When there is no more output
	 * available, then we'll start polling for the status of
	 * each subprocess in the pipeline.
	 */

#ifdef O_NONBLOCK
	fcntl(bgPtr->outputId, F_SETFL, O_NONBLOCK);
#else
	fcntl(bgPtr->outputId, F_SETFL, O_NDELAY);
#endif
	Tk_CreateFileHandler(bgPtr->outputId, TK_READABLE, BackgroundProc,
	    (ClientData)bgPtr);
    }

    if (detach) {
	/* Return a list of the child process ids */
	Tcl_ResetResult(interp);
	for (i = 0; i < numPids; i++) {
	    Blt_AppendInt(interp, bgPtr->pidArr[i]);
	}
    } else {
	int exitCode;
	int done;

	bgPtr->exitCodePtr = &exitCode;
	bgPtr->donePtr = &done;

	exitCode = done = 0;
	while (!done) {
	    Tk_DoOneEvent(0);
	}
	if (exitCode != 0) {
	    Tcl_AppendResult(interp, "child process exited abnormally",
		(char *)NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_BgExecInit --
 *
 *	This procedure is invoked to initialize the "bgexec" Tcl
 *	command.  See the user documentation for details on what it
 *	does.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------- */
int
Blt_BgExecInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = { "bgexec", BGEXEC_VERSION, BgExecCmd, };

    return (Blt_InitCmd(interp, &cmdSpec));
}
#endif /* !NO_BGEXEC */
