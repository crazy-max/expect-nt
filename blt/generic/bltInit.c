/*
 * bltInit.c --
 *
 *	This module initializes BLT commands and variables.
 *
 * Copyright 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#define BLT_VERSION_ONLY
#include "blt.h"
#undef BLT_VERSION_ONLY

/*
 * The inclusion of contributed commands/widgets can be suppressed by
 * defining the respective preprocessor symbol.
 */

#ifndef NO_HTEXT
extern Tcl_AppInitProc Blt_HtextInit;
#endif
#ifndef NO_GRAPH
extern Tcl_AppInitProc Blt_GraphInit;
#endif
#ifndef NO_TABLE
extern Tcl_AppInitProc Blt_TableInit;
#endif
#ifndef NO_BUSY
extern Tcl_AppInitProc Blt_BusyInit;
#endif
#ifndef NO_WINOP
extern Tcl_AppInitProc Blt_WinOpInit;
#endif
#ifndef NO_BITMAP
extern Tcl_AppInitProc Blt_BitmapInit;
#endif
#ifndef NO_BGEXEC
extern Tcl_AppInitProc Blt_BgExecInit;
#endif
#ifndef NO_DRAGDROP
extern Tcl_AppInitProc Blt_DragDropInit;
#endif
#ifndef NO_DEBUG
extern Tcl_AppInitProc Blt_DebugInit;
#endif
#ifndef NO_WATCH
extern Tcl_AppInitProc Blt_WatchInit;
#endif
#ifndef NO_VECTOR
extern Tcl_AppInitProc Blt_VectorInit;
#endif
#ifndef NO_SPLINE
extern Tcl_AppInitProc Blt_SplineInit;
#endif
#ifndef NO_TILEFRAME
extern Tcl_AppInitProc Blt_FrameInit;
#endif
#ifndef NO_TILEBUTTON
extern Tcl_AppInitProc Blt_ButtonInit;
#endif
#ifndef NO_TILESCROLLBAR
extern Tcl_AppInitProc Blt_ScrollbarInit;
#endif
#ifndef NO_PIECHART
extern Tcl_AppInitProc Blt_PieInit;
#endif

static Tcl_AppInitProc *initProcArr[] =
{
#ifndef NO_HTEXT
    Blt_HtextInit,
#endif
#ifndef NO_GRAPH
    Blt_GraphInit,
#endif
#ifndef NO_TABLE
    Blt_TableInit,
#endif
#ifndef NO_BUSY
    Blt_BusyInit,
#endif
#ifndef NO_WINOP
    Blt_WinOpInit,
#endif
#ifndef NO_BITMAP
    Blt_BitmapInit,
#endif
#ifndef NO_BGEXEC
    Blt_BgExecInit,
#endif
#ifndef NO_DRAGDROP
    Blt_DragDropInit,
#endif
#ifndef NO_DEBUG
    Blt_DebugInit,
#endif
#ifndef NO_WATCH
    Blt_WatchInit,
#endif
#ifndef NO_VECTOR
    Blt_VectorInit,
#endif
#ifndef NO_SPLINE
    Blt_SplineInit,
#endif
#ifndef NO_TILEFRAME
    Blt_FrameInit,
#endif
#ifndef NO_TILEBUTTON
    Blt_ButtonInit,
#endif
#ifndef NO_TILESCROLLBAR
    Blt_ScrollbarInit,
#endif
#ifndef NO_PIECHART
    Blt_PieInit,
#endif
    (Tcl_AppInitProc *) NULL
};

#ifdef USE_TCL_NAMESPACES
static char envVar[] = "::env";
#define VERSIONS_ARRAY_VAR "::blt::blt_versions"
#else /* !USE_TCL_NAMESPACES */
static char envVar[] = "env";
#define VERSIONS_ARRAY_VAR "blt_versions"
#endif /* !USE_TCL_NAMESPACES */

#ifdef BLT_WINTERP
static Tcl_Interp *_blt_winterp;
Tcl_Interp *blt_winterp(void) {
    return _blt_winterp;
}
#endif

/*LINTLIBRARY*/
int
Blt_Init(interp)
    Tcl_Interp *interp;		/* Interpreter to add extra commands */
{
    char *libDir;
    register Tcl_AppInitProc **procPtrPtr;
#ifdef USE_TCL_NAMESPACES
    Tcl_Namespace *nspacePtr;	/* Token for "blt" namespace created, used
				 * to delete the namespace on errors. */
#endif /* USE_TCL_NAMESPACES */
#ifdef BLT_WINTERP
    _blt_winterp = interp;
#endif

    libDir = Tcl_GetVar2(interp, envVar, "BLT_LIBRARY", TCL_GLOBAL_ONLY);
    if (libDir == NULL) {
	libDir = BLT_LIBRARY;
    }
    if (Tcl_SetVar(interp, "blt_library", libDir, TCL_GLOBAL_ONLY) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_SetVar(interp, "blt_version", BLT_VERSION,
	    TCL_GLOBAL_ONLY) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_SetVar(interp, "blt_patchLevel", BLT_PATCH_LEVEL,
	    TCL_GLOBAL_ONLY) == NULL) {
	return(TCL_ERROR);
    }

#ifdef USE_TK_NAMESPACES
    if (Tcl_SetVar(interp, "blt_useTkNamespaces", "1",
	    TCL_GLOBAL_ONLY) == NULL) {
	return TCL_ERROR;
    }
#else /* !USE_TK_NAMESPACES */
    if (Tcl_SetVar(interp, "blt_useTkNamespaces", "0",
	    TCL_GLOBAL_ONLY) == NULL) {
	return TCL_ERROR;
    }
#endif /* !USE_TK_NAMESPACES */

#ifdef USE_TCL_NAMESPACES
    if (Tcl_SetVar(interp, "blt_useTclNamespaces", "1",
	    TCL_GLOBAL_ONLY) == NULL) {
	return TCL_ERROR;
    }
    if ((nspacePtr = Tcl_CreateNamespace(interp, "blt", (ClientData)0,
	    (Tcl_NamespaceDeleteProc *)0)) == NULL) {
	return TCL_ERROR;
    }
#else /* !USE_TCL_NAMESPACES */
    if (Tcl_SetVar(interp, "blt_useTclNamespaces", "0",
	    TCL_GLOBAL_ONLY) == NULL) {
	return TCL_ERROR;
    }
#endif /* !USE_TCL_NAMESPACES */

    if (Tcl_SetVar2(interp, VERSIONS_ARRAY_VAR, "BLT", BLT_VERSION,
	    TCL_GLOBAL_ONLY) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_SetVar2(interp, VERSIONS_ARRAY_VAR, "BLT_patchlevel",
	    BLT_PATCH_LEVEL, TCL_GLOBAL_ONLY) == NULL) {
	return TCL_ERROR;
    }

    for (procPtrPtr = initProcArr; *procPtrPtr != NULL; procPtrPtr++) {
	if ((**procPtrPtr) (interp) != TCL_OK) {
#ifdef USE_TCL_NAMESPACES
	    Tcl_DeleteNamespace(nspacePtr);
#endif /* USE_TCL_NAMESPACES */
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

int
Blt_SafeInit(interp)
    Tcl_Interp *interp;  
{
    return Blt_Init(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_InitCmd --
 *
 *      Given the name of a command, return a pointer to the
 *      clientData field of the command.
 *
 * Results:
 *      A standard TCL result. If the command is found, TCL_OK
 *	is returned and clientDataPtr points to the clientData
 *	field of the command (if the clientDataPtr in not NULL).
 *
 * Side effects:
 *      If the command is found, clientDataPtr is set to the address
 *	of the clientData of the command.  If not found, an error
 *	message is left in interp->result.
 *
 *----------------------------------------------------------------------
 */
int
Blt_InitCmd(interp, specPtr)
    Tcl_Interp *interp;
    Blt_CmdSpec *specPtr;
{
    Tk_Window tkwin;
    Tcl_CmdInfo cmdInfo;
    char cmdPath[200];
    ClientData clientData;

#ifdef USE_TCL_NAMESPACES
    sprintf(cmdPath, "blt::%s", specPtr->name);
#else /* !USE_TCL_NAMESPACES */
    strcpy(cmdPath, specPtr->name);
#endif /* !USE_TCL_NAMESPACES */

    if (Tcl_GetCommandInfo(interp, cmdPath, &cmdInfo)) {
	return TCL_OK;		/* Assume command was already initialized */
    }
    tkwin = Tk_MainWindow(interp);
    if (tkwin == NULL) {
	Tcl_AppendResult(interp, "\"", cmdPath, "\" requires Tk", (char *)NULL);
	return TCL_ERROR;
    }
    if (specPtr->clientData == (ClientData)0) {
	clientData = (ClientData)tkwin;
    } else {
	clientData = specPtr->clientData;
    }
    Tcl_SetVar2(interp, VERSIONS_ARRAY_VAR, specPtr->name, specPtr->version,
	TCL_GLOBAL_ONLY);
    Tcl_CreateCommand(interp, cmdPath, specPtr->cmdProc, clientData, 
	specPtr->cmdDeleteProc);

#ifdef USE_TCL_NAMESPACES
    /*
     *  mike: As of Tcl 8.0b1, the namespace API isn't yet officially
     *  available (according to Sun, this will happen in 8.1), but I've
     *  asked Brian Lewis if using Tcl_{Create|Delete}Namespace() is
     *  safe (i.e., these functions will be part of the official API)
     *  and he said "yes", so I used them in the code above. However,
     *  exporting BLT commands would require to fiddle with CallFrames
     *  and not yet exported APIs, so I decided to execute an equivalent
     *  Tcl script. Because it's very small and only executed once for
     *  each BLT command at startup time (and because access to namespaces
     *  on scripting level is likely to not being changed in the future),
     *  I don't really care to rewrite the stuff below, even when the API
     *  will be officially available. Never change a winning team...  :-)
     */
    if (Tcl_VarEval(interp, "namespace eval blt { namespace export ",
	    specPtr->name, "}", NULL) != TCL_OK) {
	return TCL_ERROR;
    }
#endif /* USE_TCL_NAMESPACES */

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_InitCmds --
 *
 *      Given the name of a command, return a pointer to the
 *      clientData field of the command.
 *
 * Results:
 *      A standard TCL result. If the command is found, TCL_OK
 *	is returned and clientDataPtr points to the clientData
 *	field of the command (if the clientDataPtr in not NULL).
 *
 * Side effects:
 *      If the command is found, clientDataPtr is set to the address
 *	of the clientData of the command.  If not found, an error
 *	message is left in interp->result.
 *
 *----------------------------------------------------------------------
 */
int
Blt_InitCmds(interp, specPtr, numCmds)
    Tcl_Interp *interp;
    Blt_CmdSpec *specPtr;
    int numCmds;
{
    register int i;

    for (i = 0; i < numCmds; i++) {
	if (Blt_InitCmd(interp, specPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	specPtr++;
    }
    return TCL_OK;
}
