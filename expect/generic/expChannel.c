/*
 * expChannel.c --
 *
 *	Implements the Expect_Channel
 *
 *	XXX: This has not been implemented yet but the idea is this:
 *	Change expect to use channel ids instead of just expect ids.
 *	This allows more flexibility.
 *
 * Copyright (c) 1997 by Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "exp_port.h"
#include "tclInt.h"
#include "tclPort.h"

int		 ExpectBlock _ANSI_ARGS_((ClientData instanceData,
                                int mode));
int              ExpectInput _ANSI_ARGS_((ClientData instanceData,
                                char *bufPtr, int bufSize, int *errorPtr));
int              ExpectOutput _ANSI_ARGS_((ClientData instanceData,
				char *bufPtr, int toWrite, int *errorPtr));
int              ExpectClose _ANSI_ARGS_((ClientData instanceData,
                                Tcl_Interp *interp));
int              ExpectSetOption _ANSI_ARGS_((ClientData instanceData,
                                Tcl_Interp *interp, char *nameStr, char *val));
int              ExpectGetOption _ANSI_ARGS_((ClientData instanceData,
                                char *nameStr, Tcl_DString *dsPtr));
Tcl_File         ExpectGetFile _ANSI_ARGS_((ClientData instanceData,
                                int direction));
int              ExpectReady _ANSI_ARGS_((ClientData instanceData,
                                int direction));
void             ExpectWatch _ANSI_ARGS_((ClientData instanceData,
                                int mask));

static Tcl_ChannelType expectChannelType = {
    "expect",
    ExpectBlock,
    ExpectClose,
    ExpectInput,
    ExpectOutput,
    NULL,         		/* Can't seek! */
    ExpectSetOption,
    ExpectGetOption,
    ExpectWatch,
    ExpectReady,
    ExpectGetFile
};


/*
 *----------------------------------------------------------------------
 *
 * ExpOpenExpectChannel --
 *
 *	Generic routine to open a expect channel
 *
 * Results:
 *	A Tcl_Channel.
 *    
 * Side Effects:
 *	Allocates memory.
 *
 * Notes:
 *	XXX: This will be called from Exp_SpawnCmd() to create a new
 *	channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
ExpOpenExpectChannel(interp, argc, argv)
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Tcl_Channel chan;
    ExpectState *ssPtr;
    char devStr[15];
    char channelNameStr[10];

    /*
     * XXX: A bunch of other stuff should be done here first
     */

    ssPtr = (ExpectState *) ckalloc(sizeof(ExpectState));
    if (ExppOpenExpectChannel(interp, (ClientData)ssPtr, devStr, flags)
	    != TCL_OK) {
	ckfree((char *)ssPtr);
	return NULL;
    }
    ssPtr->theFile = Tcl_GetFile((ClientData)ssPtr->fd, EXPECT_HANDLE);

    /*
     * Setup the expect channel to always flush immediately
     */

    sprintf(channelNameStr, "expect%d", expectCount++);
    chan = Tcl_CreateChannel(&expectChannelType, channelNameStr,
	    (ClientData) ssPtr, mode);
     
    if (Tcl_SetChannelOption(interp, chan, "-buffering", "none")
	    != TCL_OK) {
        ExpClose(interp, chan);
	return NULL;
    }

    return chan;

arg_missing:
    Tcl_AppendResult(interp, "Value for \"", argv[i],
    	"\" missing", NULL);
    return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * ExpectBlock --
 *
 *	Generic routine to set I/O to blocking or non-blocking.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *    
 * Side Effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
ExpectBlock(instanceData, mode)
    ClientData instanceData;
    int mode;			/* (in) Block or not */
{
    return ExppExpectBlock(instanceData, mode);
}


/*
 *----------------------------------------------------------------------
 *
 * ExpectInput --
 *
 *	Generic read routine for expect ports
 *
 * Returns:
 *	Amount read or -1 with errorcode in errorPtr.
 *    
 * Side Effects:
 *	Buffer is updated. 
 *
 *----------------------------------------------------------------------
 */

int
ExpectInput(instanceData, bufPtr, bufSize, errorPtr)
    ClientData instanceData;
    char *bufPtr;		/* (in) Ptr to buffer */
    int bufSize;		/* (in) sizeof buffer */
    int *errorPtr;		/* (out) error code */
{
    Tcl_Channel channelPtr = ((PlugFInfo *)instanceData)->channelPtr;

    return (Tcl_GetChannelType(channelPtr)->inputProc)
	(Tcl_GetChannelInstanceData(channelPtr), bufPtr, bufSize, errorPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * ExpectOutput --
 *
 *	Generic write routine for expect ports
 *
 * Results:
 *	Amount written or -1 with errorcode in errorPtr
 *    
 * Side Effects:
 *	None. 
 *
 *----------------------------------------------------------------------
 */

int
ExpectOutput(instanceData, bufPtr, toWrite, errorPtr)
    ClientData instanceData;
    char *bufPtr;		/* (in) Ptr to buffer */
    int toWrite;		/* (in) amount to write */
    int *errorPtr;		/* (out) error code */
{
    Tcl_Channel channelPtr = ((PlugFInfo *)instanceData)->channelPtr;

    return (Tcl_GetChannelType(channelPtr)->outputProc)
	(Tcl_GetChannelInstanceData(channelPtr), bufPtr, toWrite, errorPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ExpectClose --
 *
 *	Generic routine to close the expect port
 *
 * Results:
 *      0 if successful or a POSIX errorcode with
 *      interp updated.
 *    
 * Side Effects:
 *	Channel is deleted.
 *
 *----------------------------------------------------------------------
 */

int
ExpectClose(instanceData, interp)
    ClientData instanceData;
    Tcl_Interp *interp;
{
    ExpectState *ssPtr = (ExpectState *) instanceData;
    int rc = TCL_OK;

    rc = ExppExpectClose(instanceData);
    if ((rc != 0) && (interp != NULL)) {
    	Tcl_SetErrno(rc);
    	Tcl_SetResult(interp, Tcl_PosixError(interp), TCL_VOLATILE);
    }
    Tcl_FreeFile(ssPtr->theFile);
    ckfree((char *)ssPtr);
    return rc; 
}


/*
 *----------------------------------------------------------------------
 *
 * ExpectSetOption --
 *
 *	Set the value of an expect channel option
 *
 * Results:
 *	TCL_OK and dsPtr updated with the value or TCL_ERROR.
 *
 * Side Effects
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
ExpectSetOption(instanceData, interp, nameStr, valStr)
    ClientData instanceData;
    Tcl_Interp *interp;
    char *nameStr;		/* (in) Name of option */
    char *valStr;		/* (in) New value of option */
{
    ExpectState *ssPtr = (ExpectState *) instanceData;
    int optVal, option;
    char errorStr[80];
    int optBool;

    Tcl_AppendResult (interp, "Illegal option \"", nameStr,
		      "\" -- must be a standard channel option", NULL);
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * ExpectGetOption --
 *
 *	Queries expect channel for the current value of
 *      the given option.
 *
 * Results:
 *	TCL_OK and dsPtr updated with the value or TCL_ERROR.
 *
 * Side Effects
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
ExpectGetOption(instanceData, nameStr, dsPtr)
    ClientData instanceData;
    char *nameStr;		/* (in) Name of option to retrieve */		
    Tcl_DString *dsPtr;		/* (in) String to place value */
{
    ExpectState *ssPtr = (ExpectState *) instanceData;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpectGetFile --
 *
 *	Get the Tcl_File for the appropriate direction in from the
 *	Tcl_Channel.
 *
 * Results:
 *	NULL because expect ids are handled through other channel
 *	types.
 *
 * Side Effects
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_File
ExpectGetFile(instanceData, direction)
    ClientData instanceData;
    int direction;
{
    return (Tcl_File)NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpectReady --
 *
 *	Determines whether expect port has data to be
 *	read or is OK for writing.
 *
 * Results:
 *	A bitmask of the events that were found by checking the
 *	underlying channel.
 *
 * Side Effects
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
ExpectReady(instanceData, direction)
    ClientData instanceData;
    int direction;
{
    Tcl_Channel channelPtr = ((PlugFInfo *)instanceData)->channelPtr;

    return (Tcl_GetChannelType(channelPtr)->channelReadyProc)
                         (Tcl_GetChannelInstanceData(channelPtr), mask);
}

/*
 *----------------------------------------------------------------------
 *
 * ExpectWatch --
 *
 *	Sets up event handling on a expect port Tcl_Channel using
 *	the underlying channel type.
 *
 * Results:
 *	Nothing
 *
 * Side Effects
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
ExpectWatch(instanceData, mask)
    ClientData instanceData;
    int mask;
{
    Tcl_Channel channelPtr = ((PlugFInfo *)instanceData)->channelPtr;

    (Tcl_GetChannelType(channelPtr)->watchChannelProc) 
                         (Tcl_GetChannelInstanceData(channelPtr), mask);
    return;
}

