/*
 * expWinChan.c --
 *
 *	Implements the exp_spawn channel id.  This wraps a normal
 *	file channel in another channel so we can close the file
 *	channel normally but still have another id to wait on.
 *	The file channel is not exposed in any interps.
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
#include "exp_command.h"
#include "expWin.h"

static int	ExpSpawnBlock _ANSI_ARGS_((ClientData instanceData,
		    int mode));
static int	ExpSpawnInput _ANSI_ARGS_((ClientData instanceData,
		    char *bufPtr, int bufSize, int *errorPtr));
static int	ExpSpawnOutput _ANSI_ARGS_((ClientData instanceData,
		    char *bufPtr, int toWrite, int *errorPtr));
static int	ExpSpawnClose _ANSI_ARGS_((ClientData instanceData,
		    Tcl_Interp *interp));
static int	ExpSpawnSetOption _ANSI_ARGS_((ClientData instanceData,
		    Tcl_Interp *interp, char *nameStr, char *val));
static int	ExpSpawnGetOption _ANSI_ARGS_((ClientData instanceData,
		    Tcl_Interp *interp, char *nameStr, Tcl_DString *dsPtr));
static int	ExpSpawnGetHandle _ANSI_ARGS_((ClientData instanceData,
		    int direction, ClientData *handlePtr));
static void	ExpSpawnWatch _ANSI_ARGS_((ClientData instanceData,
		    int mask));

static Tcl_ChannelType ExpSpawnChannelType = {
    "exp_spawn",
    ExpSpawnBlock,
    ExpSpawnClose,
    ExpSpawnInput,
    ExpSpawnOutput,
    NULL,         		/* Can't seek! */
    ExpSpawnSetOption,
    ExpSpawnGetOption,
    ExpSpawnWatch,
    ExpSpawnGetHandle
};

static int expSpawnCount = 0;


/*
 *----------------------------------------------------------------------
 *
 * ExpCreateSpawnChannel --
 *
 *	Create an expect spawn identifier
 *
 * Results:
 *	A Tcl channel
 *
 * Side Effects:
 *	Allocates and registers a channel
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
ExpCreateSpawnChannel(interp, chan)
    Tcl_Interp *interp;
    Tcl_Channel chan;
{
    ExpSpawnState *ssPtr;
    char channelNameStr[20];

    ssPtr = (ExpSpawnState *) ckalloc(sizeof(ExpSpawnState));
    ssPtr->channelPtr = chan;
    ssPtr->toWrite = 0;

    /*
     * Setup the expect channel to always flush immediately
     */

    sprintf(channelNameStr, "exp_spawn%d", expSpawnCount++);

    chan = Tcl_CreateChannel(&ExpSpawnChannelType, channelNameStr,
			     (ClientData) ssPtr, TCL_READABLE|TCL_WRITABLE);
    Tcl_SetChannelOption(interp, chan, "-blocking", "0");
    Tcl_SetChannelOption(interp, chan, "-buffering", "none");
    Tcl_SetChannelOption(interp, chan, "-translation","binary");
    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpSpawnBlock --
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

static int
ExpSpawnBlock(instanceData, mode)
    ClientData instanceData;
    int mode;			/* (in) Block or not */
{
    Tcl_Channel channelPtr = ((ExpSpawnState *)instanceData)->channelPtr;

    return (Tcl_GetChannelType(channelPtr)->blockModeProc)
	(Tcl_GetChannelInstanceData(channelPtr), mode);
}


/*
 *----------------------------------------------------------------------
 *
 * ExpSpawnInput --
 *
 *	Generic read routine for expect console
 *
 * Returns:
 *	Amount read or -1 with errorcode in errorPtr.
 *    
 * Side Effects:
 *	Buffer is updated. 
 *
 *----------------------------------------------------------------------
 */

static int
ExpSpawnInput(instanceData, bufPtr, bufSize, errorPtr)
    ClientData instanceData;
    char *bufPtr;		/* (in) Ptr to buffer */
    int bufSize;		/* (in) sizeof buffer */
    int *errorPtr;		/* (out) error code */
{
    Tcl_Channel channelPtr = ((ExpSpawnState *)instanceData)->channelPtr;

    return (Tcl_GetChannelType(channelPtr)->inputProc)
	(Tcl_GetChannelInstanceData(channelPtr), bufPtr, bufSize, errorPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * ExpSpawnOutput --
 *
 *	Write routine for expect console
 *
 * Results:
 *	Amount written or -1 with errorcode in errorPtr
 *    
 * Side Effects:
 *	None. 
 *
 *----------------------------------------------------------------------
 */

static int
ExpSpawnOutput(instanceData, bufPtr, toWrite, errorPtr)
    ClientData instanceData;
    char *bufPtr;		/* (in) Ptr to buffer */
    int toWrite;		/* (in) amount to write */
    int *errorPtr;		/* (out) error code */
{
    return ExpPlatformSpawnOutput(instanceData, bufPtr, toWrite, errorPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ExpSpawnClose --
 *
 *	Generic routine to close the expect console
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

static int
ExpSpawnClose(instanceData, interp)
    ClientData instanceData;
    Tcl_Interp *interp;
{
    ExpSpawnState *ssPtr = (ExpSpawnState *) instanceData;
    Tcl_Channel channelPtr = ssPtr->channelPtr;
    int ret;

    ret = Tcl_Close(interp, channelPtr);

    ckfree((char *)ssPtr);

    return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * ExpSpawnSetOption --
 *
 *	Set the value of an ExpSpawn channel option
 *
 * Results:
 *	TCL_OK and dsPtr updated with the value or TCL_ERROR.
 *
 * Side Effects
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ExpSpawnSetOption(instanceData, interp, nameStr, valStr)
    ClientData instanceData;
    Tcl_Interp *interp;
    char *nameStr;		/* (in) Name of option */
    char *valStr;		/* (in) New value of option */
{
    Tcl_Channel channelPtr = ((ExpSpawnState *)instanceData)->channelPtr;

    return (Tcl_GetChannelType(channelPtr)->setOptionProc)
	(Tcl_GetChannelInstanceData(channelPtr), interp, nameStr, valStr);
}

/*
 *----------------------------------------------------------------------
 *
 * ExpSpawnGetOption --
 *
 *	Queries ExpSpawn channel for the current value of
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

static int
ExpSpawnGetOption(instanceData, interp, nameStr, dsPtr)
    ClientData instanceData;
    Tcl_Interp *interp;
    char *nameStr;		/* (in) Name of option to retrieve */		
    Tcl_DString *dsPtr;		/* (in) String to place value */
{
    Tcl_Channel channelPtr = ((ExpSpawnState *)instanceData)->channelPtr;

    return (Tcl_GetChannelType(channelPtr)->getOptionProc)
	(Tcl_GetChannelInstanceData(channelPtr), interp, nameStr, dsPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ExpSpawnGetHandle --
 *
 *	Get the Tcl_File for the appropriate direction in from the
 *	Tcl_Channel.
 *
 * Results:
 *	NULL because ExpSpawn ids are handled through other channel
 *	types.
 *
 * Side Effects
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
ExpSpawnGetHandle(instanceData, direction, handlePtr)
    ClientData instanceData;
    int direction;
    ClientData *handlePtr;
{
    Tcl_Channel channelPtr = ((ExpSpawnState *)instanceData)->channelPtr;

    return (Tcl_GetChannelType(channelPtr)->getHandleProc)
	(Tcl_GetChannelInstanceData(channelPtr), direction, handlePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ExpSpawnWatch --
 *
 *	Sets up event handling on a expect console Tcl_Channel using
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
ExpSpawnWatch(instanceData, mask)
    ClientData instanceData;
    int mask;
{
    Tcl_Channel channelPtr = ((ExpSpawnState *)instanceData)->channelPtr;

    (Tcl_GetChannelType(channelPtr)->watchProc)
	(Tcl_GetChannelInstanceData(channelPtr), mask);
    return;
}
