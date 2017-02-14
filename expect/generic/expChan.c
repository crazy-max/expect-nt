/*
 * expChan.c --
 *
 *	Implements the exp_pair channel id.  What this really does
 *	is wrap the input and output channels into a single, duplex
 *	channel.
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

static void	ExpPairInputCloseHandler _ANSI_ARGS_((ClientData clientData));
static void	ExpPairOutputCloseHandler _ANSI_ARGS_((ClientData clientData));
static int	ExpPairBlock _ANSI_ARGS_((ClientData instanceData,
		    int mode));
static int	ExpPairInput _ANSI_ARGS_((ClientData instanceData,
		    char *bufPtr, int bufSize, int *errorPtr));
static int	ExpPairOutput _ANSI_ARGS_((ClientData instanceData,
		    char *bufPtr, int toWrite, int *errorPtr));
static int	ExpPairClose _ANSI_ARGS_((ClientData instanceData,
		    Tcl_Interp *interp));
static int	ExpPairSetOption _ANSI_ARGS_((ClientData instanceData,
		    Tcl_Interp *interp, char *nameStr, char *val));
static int	ExpPairGetOption _ANSI_ARGS_((ClientData instanceData,
		    Tcl_Interp *interp, char *nameStr, Tcl_DString *dsPtr));
static int	ExpPairGetHandle _ANSI_ARGS_((ClientData instanceData,
		    int direction, ClientData *handlePtr));
static void	ExpPairWatch _ANSI_ARGS_((ClientData instanceData,
		    int mask));
static void	ExpPairReadable _ANSI_ARGS_((ClientData clientData, int mask));
static void	ExpPairWritable _ANSI_ARGS_((ClientData clientData, int mask));

static Tcl_ChannelType ExpPairChannelType = {
    "exp_pair",
    ExpPairBlock,
    ExpPairClose,
    ExpPairInput,
    ExpPairOutput,
    NULL,         		/* Can't seek! */
    ExpPairSetOption,
    ExpPairGetOption,
    ExpPairWatch,
    ExpPairGetHandle
};

typedef struct {
    Tcl_Channel thisChannelPtr;	/* The toplevel channel */
    Tcl_Channel inChannelPtr;	/* The input child channel */
    Tcl_Channel outChannelPtr;	/* The output child channel */
    int watchMask;		/* Events that are being checked for */
    int blockingPropagate;	/* Propagate a blocking option to children */
} ExpPairState;

static int expPairCount = 0;
static int initialized = 0;

/*
 *----------------------------------------------------------------------
 *
 * ExpPairInit --
 *
 *	Initialize the pair event mechanism.  Currently, it
 *	does nothing because it may not be needed.  If it is
 *	needed, it will need to call platform specific event
 *	code.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	None currently
 *
 *----------------------------------------------------------------------
 */

static void
ExpPairInit()
{
    initialized = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpCreatePairChannel --
 *
 *	Routine that wraps an input channel and an output channel
 *	into a single channel.  By default, no translation or buffering
 *	occurs in this channel.
 *
 * Results:
 *	A Tcl_Channel.
 *    
 * Side Effects:
 *	Allocates memory.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
ExpCreatePairChannel(interp, chanInId, chanOutId, chanName)
    Tcl_Interp *interp;
    char *chanInId;
    char *chanOutId;
    char *chanName;		/* Name of resulting channel to create.
				 * If NULL, it gets created here */
{
    Tcl_Channel chanIn, chanOut, chan;
    ExpPairState *ssPtr;
    char channelNameStr[10];
    int mode;

    chanIn = Tcl_GetChannel(interp, chanInId, &mode);
    if (chanIn) {
	if ((mode & TCL_READABLE) == 0) {
	    Tcl_AppendResult(interp, chanInId, " is not a readable channel",
			     (char *) NULL);
	    return NULL;
	}
    } else {
	return NULL;
    }

    chanOut = Tcl_GetChannel(interp, chanOutId, &mode);
    if (chanOut) {
	if ((mode & TCL_WRITABLE) == 0) {
	    Tcl_AppendResult(interp, chanInId, " is not a writable channel",
			     (char *) NULL);
	    return NULL;
	}
    } else {
	return NULL;
    }

    if (chanName == NULL) {
	sprintf(channelNameStr, "exp_pair%d", expPairCount++);
	chanName = channelNameStr;
    }
    ssPtr = (ExpPairState *) ckalloc(sizeof(ExpPairState));
    ssPtr->inChannelPtr = chanIn;
    ssPtr->outChannelPtr = chanOut;

    /*
     * Setup the expect channel to always flush immediately
     */

    if (chanName == NULL) {
	sprintf(channelNameStr, "exp_pair%d", expPairCount++);
	chanName = channelNameStr;
    }
    chan = Tcl_CreateChannel(&ExpPairChannelType, chanName,
			     (ClientData) ssPtr, TCL_READABLE|TCL_WRITABLE);
    if (chan == NULL) {
	free(ssPtr);
	return NULL;
    }
    ssPtr->thisChannelPtr = chan;
    ssPtr->watchMask = 0;
    ssPtr->blockingPropagate = 0;

    Tcl_CreateCloseHandler(chanIn, ExpPairInputCloseHandler,
	(ClientData) ssPtr);
    Tcl_CreateCloseHandler(chanOut, ExpPairOutputCloseHandler,
	(ClientData) ssPtr);

    Tcl_SetChannelOption(interp, chan, "-buffering", "none");
    Tcl_SetChannelOption(interp, chan, "-translation","binary");
    Tcl_SetChannelOption(interp, chan, "-blockingpropagate", "off");
    Tcl_SetChannelOption(interp, chan, "-blocking", "off");
    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpPairInputCloseHandler --
 *
 *	This gets called when the underlying input channel is closed.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
ExpPairInputCloseHandler(clientData)
    ClientData clientData;
{
    ExpPairState *ssPtr = (ExpPairState *) clientData;
    ssPtr->inChannelPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpPairOutputCloseHandler --
 *
 *	This gets called when the underlying output channel is closed.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
ExpPairOutputCloseHandler(clientData)
    ClientData clientData;
{
    ExpPairState *ssPtr = (ExpPairState *) clientData;
    ssPtr->outChannelPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpPairBlock --
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
ExpPairBlock(instanceData, mode)
    ClientData instanceData;
    int mode;			/* (in) Block or not */
{
    ExpPairState *ssPtr = (ExpPairState *) instanceData;
    Tcl_Channel inChannelPtr = ssPtr->inChannelPtr;
    Tcl_Channel outChannelPtr = ssPtr->outChannelPtr;
    int ret;

    if (! ssPtr->blockingPropagate) {
	return TCL_OK;
    }
    if (inChannelPtr && Tcl_GetChannelType(inChannelPtr)->blockModeProc) {
	ret = (Tcl_GetChannelType(inChannelPtr)->blockModeProc)
	    (Tcl_GetChannelInstanceData(inChannelPtr), mode);
	if (ret == TCL_ERROR) {
	    return ret;
	}
    }
    if (outChannelPtr && Tcl_GetChannelType(outChannelPtr)->blockModeProc) {
	return (Tcl_GetChannelType(outChannelPtr)->blockModeProc)
	    (Tcl_GetChannelInstanceData(outChannelPtr), mode);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpPairInput --
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
ExpPairInput(instanceData, bufPtr, bufSize, errorPtr)
    ClientData instanceData;
    char *bufPtr;		/* (in) Ptr to buffer */
    int bufSize;		/* (in) sizeof buffer */
    int *errorPtr;		/* (out) error code */
{
    ExpPairState *ssPtr = (ExpPairState *)instanceData;
    Tcl_Channel channelPtr = ssPtr->inChannelPtr;

    if (! channelPtr) {
	*errorPtr = EPIPE;
	return -1;
    }

    if (channelPtr && Tcl_GetChannelType(channelPtr)->inputProc) {
	return (Tcl_GetChannelType(channelPtr)->inputProc)
	    (Tcl_GetChannelInstanceData(channelPtr), bufPtr, bufSize, errorPtr);
    }
    *errorPtr = EINVAL;
    return -1;
}


/*
 *----------------------------------------------------------------------
 *
 * ExpPairOutput --
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
ExpPairOutput(instanceData, bufPtr, toWrite, errorPtr)
    ClientData instanceData;
    char *bufPtr;		/* (in) Ptr to buffer */
    int toWrite;		/* (in) amount to write */
    int *errorPtr;		/* (out) error code */
{
    ExpPairState *ssPtr = (ExpPairState *)instanceData;
    Tcl_Channel channelPtr = ssPtr->outChannelPtr;

    if (! channelPtr) {
	*errorPtr = EPIPE;
	return -1;
    }

    if (channelPtr && (Tcl_GetChannelType(channelPtr)->outputProc)) {
	return (Tcl_GetChannelType(channelPtr)->outputProc)
	    (Tcl_GetChannelInstanceData(channelPtr), bufPtr, toWrite, errorPtr);
    }
    *errorPtr = EINVAL;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpPairClose --
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
ExpPairClose(instanceData, interp)
    ClientData instanceData;
    Tcl_Interp *interp;
{
    ExpPairState *ssPtr = (ExpPairState *) instanceData;
    if (ssPtr->inChannelPtr) {
	Tcl_DeleteCloseHandler(ssPtr->inChannelPtr, ExpPairOutputCloseHandler,
	    (ClientData) ssPtr);
    }
    if (ssPtr->outChannelPtr) {
	Tcl_DeleteCloseHandler(ssPtr->inChannelPtr, ExpPairOutputCloseHandler,
	    (ClientData) ssPtr);
    }
    ckfree((char *)ssPtr);

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ExpPairSetOption --
 *
 *	Set the value of an ExpPair channel option
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
ExpPairSetOption(instanceData, interp, nameStr, valStr)
    ClientData instanceData;
    Tcl_Interp *interp;
    char *nameStr;		/* (in) Name of option */
    char *valStr;		/* (in) New value of option */
{
    ExpPairState *ssPtr = (ExpPairState *) instanceData;
    Tcl_Channel inChannelPtr = ssPtr->inChannelPtr;
    Tcl_Channel outChannelPtr = ssPtr->outChannelPtr;
    int ret1, ret2;
    Tcl_DString dString;
    int len;
    int newMode;

    len = strlen(nameStr);
    if (strcmp(nameStr, "-blockingpropagate") == 0) {
        if (Tcl_GetBoolean(interp, valStr, &newMode) == TCL_ERROR) {
            return TCL_ERROR;
        }
	ssPtr->blockingPropagate = newMode;
	return TCL_OK;
    }

    /*
     * If the option can be applied to either channel, the result is OK.
     */
    ret1 = ret2 = TCL_OK;
    if (inChannelPtr && (Tcl_GetChannelType(inChannelPtr)->setOptionProc)) {
	ret1 = (Tcl_GetChannelType(inChannelPtr)->setOptionProc)
	    (Tcl_GetChannelInstanceData(inChannelPtr), interp, nameStr, valStr);
    }
    if (outChannelPtr && (Tcl_GetChannelType(outChannelPtr)->setOptionProc)) {
	Tcl_DStringInit(&dString);
	Tcl_DStringGetResult(interp, &dString);
	ret2 = (Tcl_GetChannelType(outChannelPtr)->setOptionProc)
	    (Tcl_GetChannelInstanceData(outChannelPtr), interp, nameStr, valStr);
	if (ret1 == TCL_OK && ret2 != TCL_OK) {
	    Tcl_DStringResult(interp, &dString);
	}
	Tcl_DStringFree(&dString);
    }

    if (ret1 == TCL_OK && ret2 == TCL_OK) {
	return TCL_OK;
    }
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * ExpPairGetOption --
 *
 *	Queries ExpPair channel for the current value of
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
ExpPairGetOption(instanceData, interp, nameStr, dsPtr)
    ClientData instanceData;
    Tcl_Interp *interp;
    char *nameStr;		/* (in) Name of option to retrieve */
    Tcl_DString *dsPtr;		/* (in) String to place value */
{
    ExpPairState *ssPtr = (ExpPairState *) instanceData;
    Tcl_Channel inChannelPtr = ssPtr->inChannelPtr;
    Tcl_Channel outChannelPtr = ssPtr->outChannelPtr;
    int ret;
    int len;

    len = nameStr ? strlen(nameStr) : 0;
	
    if (strcmp(nameStr, "-blockingpropagate") == 0) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-blockingpropagate");
        }
        Tcl_DStringAppendElement(dsPtr,
		(ssPtr->blockingPropagate) ? "0" : "1");
        if (len > 0) {
            return TCL_OK;
        }
    }

    if (inChannelPtr && (Tcl_GetChannelType(inChannelPtr)->getOptionProc)) {
	ret = (Tcl_GetChannelType(inChannelPtr)->getOptionProc)
	    (Tcl_GetChannelInstanceData(inChannelPtr), interp, nameStr, dsPtr);
	if (ret == TCL_OK) {
	    return ret;
	}
    }
    if (outChannelPtr && (Tcl_GetChannelType(outChannelPtr)->getOptionProc)) {
	return (Tcl_GetChannelType(outChannelPtr)->getOptionProc)
	    (Tcl_GetChannelInstanceData(outChannelPtr), interp, nameStr, dsPtr);
    }

    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpPairGetHandle --
 *
 *	Get the Tcl_File for the appropriate direction in from the
 *	Tcl_Channel.
 *
 * Results:
 *	NULL because ExpPair ids are handled through other channel
 *	types.
 *
 * Side Effects
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
ExpPairGetHandle(instanceData, direction, handlePtr)
    ClientData instanceData;
    int direction;
    ClientData *handlePtr;
{
    Tcl_Channel inChannelPtr = ((ExpPairState *)instanceData)->inChannelPtr;
    Tcl_Channel outChannelPtr = ((ExpPairState *)instanceData)->outChannelPtr;

    if (direction == TCL_READABLE) {
	if (inChannelPtr && (Tcl_GetChannelType(inChannelPtr)->getHandleProc)) {
	    return (Tcl_GetChannelType(inChannelPtr)->getHandleProc)
		(Tcl_GetChannelInstanceData(inChannelPtr), direction, handlePtr);
	} else {
	    *handlePtr = NULL;
	    return TCL_ERROR;
	}
    } else {
	if (outChannelPtr && (Tcl_GetChannelType(outChannelPtr)->getHandleProc)) {
	    return (Tcl_GetChannelType(outChannelPtr)->getHandleProc)
		(Tcl_GetChannelInstanceData(outChannelPtr), direction, handlePtr);
	} else {
	    *handlePtr = NULL;
	    return TCL_ERROR;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ExpPairWatch --
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
ExpPairWatch(instanceData, mask)
    ClientData instanceData;
    int mask;
{
    ExpPairState *ssPtr = (ExpPairState *) instanceData;
    Tcl_Channel inChannelPtr = ssPtr->inChannelPtr;
    Tcl_Channel outChannelPtr = ssPtr->outChannelPtr;
    int old_mask = ssPtr->watchMask;

    if (mask & TCL_READABLE) {
	if (inChannelPtr && (Tcl_GetChannelType(inChannelPtr)->watchProc)) {
	    (Tcl_GetChannelType(inChannelPtr)->watchProc)
		(Tcl_GetChannelInstanceData(inChannelPtr), mask & (~TCL_WRITABLE));
	}
	if (! (old_mask & TCL_READABLE)) {
	    Tcl_CreateChannelHandler(inChannelPtr, TCL_READABLE,
		ExpPairReadable, instanceData);
	}
    } else if (old_mask & TCL_READABLE) {
	Tcl_DeleteChannelHandler(inChannelPtr, ExpPairReadable, instanceData);
    }
    if (mask & TCL_WRITABLE) {
	if (outChannelPtr && (Tcl_GetChannelType(outChannelPtr)->watchProc)) {
	    (Tcl_GetChannelType(outChannelPtr)->watchProc)
		(Tcl_GetChannelInstanceData(outChannelPtr), mask & (~TCL_READABLE));
	}
	if (! (old_mask & TCL_WRITABLE)) {
	    Tcl_CreateChannelHandler(outChannelPtr, TCL_WRITABLE,
		ExpPairWritable, instanceData);
	}
    } else if (old_mask & TCL_WRITABLE) {
	Tcl_DeleteChannelHandler(outChannelPtr, ExpPairWritable, instanceData);
    }
    ssPtr->watchMask = mask;
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpPairReadable --
 *
 *	Callback when an event occurs in the input channel.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	An event is generated for this channel.
 *
 *----------------------------------------------------------------------
 */

static void
ExpPairReadable(ClientData instanceData, int mask)
{
    ExpPairState *ssPtr = (ExpPairState *) instanceData;
    Tcl_Channel channel = ssPtr->thisChannelPtr;

    if (! initialized) {
	initialized = 1;
	ExpPairInit();
    }
    Tcl_NotifyChannel(channel, mask);
}

/*
 *----------------------------------------------------------------------
 *
 * ExpPairWritable --
 *
 *	Callback when an event occurs in the output channel.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	An event is generated for this channel.
 *
 *----------------------------------------------------------------------
 */

static void
ExpPairWritable(ClientData instanceData, int mask)
{
    ExpPairState *ssPtr = (ExpPairState *) instanceData;
    Tcl_Channel channel = ssPtr->thisChannelPtr;

    if (!initialized) {
	initialized = 1;
	ExpPairInit();
    }
    Tcl_NotifyChannel(channel, mask);
}
