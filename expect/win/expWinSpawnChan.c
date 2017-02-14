/*
 * expWinSpawnChan.c --
 *
 *	Implements the Windows specific portion of the exp_spawn
 *	channel id.
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

/*
 *----------------------------------------------------------------------
 *
 * ExpPlatformSpawnOutput --
 *
 *	Write routine for exp_spawn channel
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
ExpPlatformSpawnOutput(instanceData, bufPtr, toWrite, errorPtr)
    ClientData instanceData;
    char *bufPtr;		/* (in) Ptr to buffer */
    int toWrite;		/* (in) amount to write */
    int *errorPtr;		/* (out) error code */
{
    ExpSpawnState *ssPtr = (ExpSpawnState *) instanceData;
    Tcl_Channel channelPtr = ssPtr->channelPtr;
    unsigned char lenbuf[5];
    int n;

    if (ssPtr->toWrite == 0) {
	lenbuf[0] = EXP_SLAVE_WRITE;
	lenbuf[1] = toWrite & 0xff;
	lenbuf[2] = (toWrite & 0xff00) >> 8;
	lenbuf[3] = (toWrite & 0xff0000) >> 16;
	lenbuf[4] = (toWrite & 0xff000000) >> 24;

	n = (Tcl_GetChannelType(channelPtr)->outputProc)
	    (Tcl_GetChannelInstanceData(channelPtr), lenbuf, 5, errorPtr);
	if (n < 0) {
	    return n;
	}
	if (n != 5) {
	    return 0;
	}
	ssPtr->toWrite = toWrite;
    }

    n = (Tcl_GetChannelType(channelPtr)->outputProc)
	(Tcl_GetChannelInstanceData(channelPtr), bufPtr, toWrite, errorPtr);
    if (n > 0) {
	ssPtr->toWrite -= n;
    }
    return n;
}
