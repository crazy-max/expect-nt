/*
 * bltWinOp.c --
 *
 *	This module implements simple window commands for
 *	the Tk toolkit.
 *
 * Copyright 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef	NO_WINOP

#define WINOP_VERSION "8.0"

static Tk_Window
bltGetWindow(interp, pathName, mainWindow)
    Tcl_Interp *interp;
    char *pathName;
    Tk_Window mainWindow;
{
    Tk_Window tkwin;

    tkwin = Tk_NameToWindow(interp, pathName, mainWindow);
    if (tkwin != NULL) {
	Tk_MakeWindowExist(tkwin);
    }
    return (tkwin);
}

static int
LowerOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register int i;
    Tk_Window tkwin;

    for (i = 2; i < argc; i++) {
	tkwin = bltGetWindow(interp, argv[i], (Tk_Window)clientData);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
#ifdef __WIN32__
	Tk_RestackWindow(tkwin, Below, NULL);
#else
	XLowerWindow(Tk_Display(tkwin), Tk_WindowId(tkwin));
#endif
    }
    return TCL_OK;
}

static int
RaiseOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register int i;
    Tk_Window tkwin;

    for (i = 2; i < argc; i++) {
	tkwin = bltGetWindow(interp, argv[i], (Tk_Window)clientData);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
#ifdef __WIN32__
	Tk_RestackWindow(tkwin, Above, NULL);
#else
	XRaiseWindow(Tk_Display(tkwin), Tk_WindowId(tkwin));
#endif
    }
    return TCL_OK;
}

static int
MapOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register int i;
    Tk_Window tkwin;

    for (i = 2; i < argc; i++) {
	tkwin = bltGetWindow(interp, argv[i], (Tk_Window)clientData);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	XMapWindow(Tk_Display(tkwin), Tk_WindowId(tkwin));
    }
    return TCL_OK;
}

static int
UnmapOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register int i;
    Tk_Window tkwin;

    for (i = 2; i < argc; i++) {
	tkwin = bltGetWindow(interp, argv[i], (Tk_Window)clientData);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	XUnmapWindow(Tk_Display(tkwin), Tk_WindowId(tkwin));
    }
    return TCL_OK;
}

/* ARGSUSED */
static int
QueryOper(interp, tkwin)
    Tcl_Interp *interp;
    Tk_Window tkwin;
{
    int rootX, rootY, childX, childY;
    Window root, child;
    unsigned int mask;

    if (XQueryPointer(Tk_Display(tkwin), Tk_WindowId(tkwin), &root,
	    &child, &rootX, &rootY, &childX, &childY, &mask)) {
	char string[200];

	sprintf(string, "@%d,%d", rootX, rootY);
	Tcl_SetResult(interp, string, TCL_VOLATILE);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
WarpToOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;			/* Not used */
    char **argv;
{
#ifndef	__WIN32__
    Tk_Window tkwin;

    if (argc == 3) {
	if (argv[2][0] == '@') {
	    XPoint point;
	    Window root;
	    
	    
	    if (Blt_GetXYPosition(interp, argv[2], &point) != TCL_OK) {
		return TCL_ERROR;
	    }
	    tkwin = (Tk_Window)clientData;
	    root = RootWindow(Tk_Display(tkwin), Tk_ScreenNumber(tkwin));
	    XWarpPointer(Tk_Display(tkwin), None, root, 0, 0, 0, 0,
			 point.x, point.y);
	} else {
	    tkwin = bltGetWindow(interp, argv[2], (Tk_Window)clientData);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	    if (!Tk_IsMapped(tkwin)) {
		Tcl_AppendResult(interp, "can't warp to unmapped window \"",
				 Tk_PathName(tkwin), "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    XWarpPointer(Tk_Display(tkwin), None, Tk_WindowId(tkwin), 
		0, 0, 0, 0, Tk_Width(tkwin) / 2, Tk_Height(tkwin) / 2);
	}
    }
    return (QueryOper(interp, (Tk_Window)clientData));
#else /* __WIN32__ */
    Tcl_AppendResult(interp, "Command not implemented for this platform",
	(char *)NULL);
    return TCL_ERROR;
#endif /* __WIN32__ */
}

static Blt_OperSpec operSpecs[] =
{
    {"lower", 1, (Blt_OperProc)LowerOper, 2, 0, "window ?window?...",},
    {"map", 1, (Blt_OperProc)MapOper, 2, 0, "window ?window?...",},
    {"raise", 1, (Blt_OperProc)RaiseOper, 2, 0, "window ?window?...",},
    {"unmap", 1, (Blt_OperProc)UnmapOper, 2, 0, "window ?window?...",},
    {"warpto", 1, (Blt_OperProc)WarpToOper, 2, 3, "?window?",},
};
static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

/* ARGSUSED */
static int
WindowCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Blt_OperProc proc;
    int result;

    proc = Blt_LookupOperation(interp, numSpecs, operSpecs, BLT_OPER_ARG1,
	argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, argc, argv);
    return (result);
}

int
Blt_WinOpInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec =
    {
	"winop", WINOP_VERSION, (Tcl_CmdProc*)WindowCmd,
    };

    return (Blt_InitCmd(interp, &cmdSpec));
}
#endif /* !NO_WINOP */
