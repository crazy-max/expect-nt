/*
 * expUnixSpawnChan.c --
 *
 *	Implements the Unix specific portion of the exp_spawn
 *	channel id.
 */

#include "exp_port.h"
#include "tclInt.h"
#include "tclPort.h"
#include "exp_command.h"

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

    return (Tcl_GetChannelType(channelPtr)->outputProc)
	(Tcl_GetChannelInstanceData(channelPtr), bufPtr, toWrite, errorPtr);
}
