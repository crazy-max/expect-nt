/*
 * bltDebug.c --
 *
 *	This module implements a simple debugging aid for Tcl scripts.
 *
 * Copyright (c) 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef	NO_DEBUG

#define DEBUG_VERSION "8.0"

/*ARGSUSED*/
static void
DebugProc(clientData, interp, level, command, proc, cmdClientData,
    argc, argv)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* not used */
    int level;			/* Current level */
    char *command;		/* Command before substitution */
    int (*proc) ();		/* not used */
    ClientData cmdClientData;	/* not used */
    int argc;
    char **argv;		/* Command after parsing, but before
				 * evaluation */
{
    register int i;
    char buff[32];
    Tcl_Channel chan = Tcl_GetStdChannel(TCL_STDERR);
    if (chan == NULL) return;

    sprintf(buff, "%d>\t", level);
    Tcl_Write(chan, buff, -1);
    /* mike: Tcl 8.0a2 always passes a NULL pointer for "command"; as
     * soon as this is fixed, "debug" will automatically go back to SNAFU.
     */
    if (command) {
        Tcl_Write(chan, command, -1);
        Tcl_Write(chan, "\n\t", 2);
    }
    for (i = 0; i < argc; i++) {
	Tcl_Write(chan, argv[i], -1);
	Tcl_Write(chan, " ", 1);
    }
    Tcl_Write(chan, "\n", 1);
}

/*ARGSUSED*/
static int
DebugCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    static Tcl_Trace token;
    static int level;
    int newLevel;
    char tmpString[10];

    if (argc > 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " ?level?\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (argc == 1) {
	sprintf(tmpString, "%d", level);
	Tcl_SetResult(interp, tmpString, TCL_VOLATILE);
	return TCL_OK;
    }
    if (Tcl_GetInt(interp, argv[1], &newLevel) != TCL_OK) {
	return TCL_ERROR;
    }
    if (newLevel < 0) {
	newLevel = 0;
    }
    if (token != 0) {
	Tcl_DeleteTrace(interp, token);
    }
    if (newLevel > 0) {
	token = Tcl_CreateTrace(interp, newLevel,
	    (Tcl_CmdTraceProc*)DebugProc, (ClientData)0);
    }
    level = newLevel;
    return TCL_OK;
}

int
Blt_DebugInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = { "bltdebug", DEBUG_VERSION,
	(Tcl_CmdProc*)DebugCmd, };

    return (Blt_InitCmd(interp, &cmdSpec));
}

#endif /* !NO_DEBUG */
