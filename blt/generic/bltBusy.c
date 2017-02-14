/*
 * bltBusy.c --
 *
 *	This module implements busy windows for the Tk toolkit.
 *
 *	Dieses Modul erfuellt arbeitsreiche Fenster fuer die Tk
 *	Werzeugausruestung.
 *
 * Copyright (c) 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef	NO_BUSY

#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define BUSY_VERSION "8.0"

typedef struct {
    Display *display;		/* Display of busy window */
    Tk_Window inputWin;		/* Busy window: InputOnly class window */
    Tk_Window mainWin;		/* Main window of interpreter where "blt_busy"
				 * command was created. It's used to key the
				 * searches in the window hierarchy. See the
				 * "windows" command. */
    Tk_Window master;		/* Master window of the busy window. It is used
				 * to manage the size and position of the busy
				 * window. */
    Tk_Window parent;		/* Parent window of the busy window. It may
				 * be the master window (if the master is a
				 * toplevel) or a mutual ancestor of the
				 * master window */
    int width, height;		/* Size of the master window. Retained to know
				 * if the master window has been reconfigured
				 * to a new size. */
    int isBusy;			/* Indicates whether the InputOnly window
				 * should be mapped. This can be different
				 * from what Tk_IsMapped says because the
				 * master may be forcing the InputOnly window
				 * to be unmapped. */
    int masterX, masterY;	/* Position of the master window */
    Tk_Cursor cursor;		/* Cursor for the busy window */

} Busy;

#define DEF_BUSY_CURSOR "watch"

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_CURSOR, "-cursor", "busyCursor", "BusyCursor",
	DEF_BUSY_CURSOR, Tk_Offset(Busy, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static int initialized = 0;	/* If non-zero, indicates to
				 * initialize the hash table */
static Tcl_HashTable busyTable;	/* Hash table of busy window
				 * structures keyed by the address of
				 * the master Tk window */

static void BusyGeometryProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));
static void BusyCustodyProc _ANSI_ARGS_((ClientData clientData, Tk_Window tkwin));

static Tk_GeomMgr busyMgrInfo =
{
    "busy",			/* Name of geometry manager used by winfo */
    BusyGeometryProc,		/* Procedure to for new geometry requests */
    BusyCustodyProc,		/* Procedure when window is taken away */
};

/*
 *Forward declarations
 */
static void DestroyBusy _ANSI_ARGS_((FreeProcData freeProcData));
static void BusyEventProc _ANSI_ARGS_((ClientData clientData,XEvent *eventPtr));

/*
 *----------------------------------------------------------------------
 *
 * BusyEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for events on
 *	the busy window itself.  We're only concerned with destroy
 *	events.
 *
 *	It might be necessary (someday) to watch resize events.  Right
 *	now, I don't think there's any point in it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When a busy window is destroyed, all internal structures
 * 	associated with it released at the next idle point.
 *
 *----------------------------------------------------------------------
 */
static void
BusyEventProc(clientData, eventPtr)
    ClientData clientData;	/* Busy window record */
    XEvent *eventPtr;		/* Event which triggered call to routine */
{
    Busy *busyPtr = (Busy *) clientData;

    if (eventPtr->type == DestroyNotify) {
	busyPtr->inputWin = NULL;
	Tcl_EventuallyFree((ClientData)busyPtr, (Tcl_FreeProc *)DestroyBusy);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * BusyCustodyProc --
 *
 *	This procedure is invoked when the busy window has been stolen
 *	by another geometry manager.  The information and memory
 *	associated with the busy window is released. I don't know why
 *	anyone would try to pack a busy window, but this should keep
 *	everything sane, if it is.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Busy structure is freed at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
BusyCustodyProc(clientData, tkwin)
    ClientData clientData;	/* Information about the slave window */
    Tk_Window tkwin;		/* Not used */
{
    Busy *busyPtr = (Busy *) clientData;

    Tk_DeleteEventHandler(busyPtr->inputWin, StructureNotifyMask, BusyEventProc,
	(ClientData)busyPtr);
    if (busyPtr->inputWin != NULL) {
	Tk_UnmapWindow(busyPtr->inputWin);
	busyPtr->inputWin = NULL;
    }
    busyPtr->isBusy = FALSE;
    Tcl_EventuallyFree((ClientData)busyPtr, (Tcl_FreeProc *)DestroyBusy);
}

/*
 * ----------------------------------------------------------------------------
 *
 * BusyGeometryProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for busy
 *	windows.  Busy windows never request geometry, so it's
 *	unlikely that this routine will ever be called.  The routine
 *	exists simply as a place holder for the GeomProc in the
 *	Geometry Manager structure.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
BusyGeometryProc(clientData, tkwin)
    ClientData clientData;	/* Information about window that got new
				 * preferred geometry.  */
    Tk_Window tkwin;		/* Other Tk-related information about the
			         * window. */
{
    /* Should never get here */
}


/*
 * ------------------------------------------------------------------
 *
 * MasterEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for the
 *	following events on the master window.  If the master and
 *	parent windows are the same, only the first event is
 *	important.
 *
 *	   1) ConfigureNotify  - The master window has been resized or
 *				 moved.  Move and resize the busy window
 *				 to be the same size and position of the
 *				 master window.
 *
 *	   2) DestroyNotify    - The master window was destroyed. Destroy
 *				 the busy window and the free resources
 *				 used.
 *
 *	   3) MapNotify	       - The master window was (re)mapped. Map the
 *				 busy window again.
 *
 *	   4) UnmapNotify      - The master window was unmapped. Unmap the
 *				 busy window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the master window gets deleted, internal structures get
 *	cleaned up.  When it gets resized, the busy window is resized
 *	accordingly. If it's mapped, the busy window is mapped. And
 *	when it's unmapped, the busy window is unmapped.
 *
 * -------------------------------------------------------------------
 */
static void
MasterEventProc(clientData, eventPtr)
    ClientData clientData;	/* Busy window record */
    register XEvent *eventPtr;	/* Event which triggered call to routine */
{
    register Busy *busyPtr = (Busy *) clientData;

    switch (eventPtr->type) {
    case DestroyNotify:

	/*
	 * Arrange for the busy structure to be removed at a proper time.
	 */

	Tcl_EventuallyFree((ClientData)busyPtr, (Tcl_FreeProc *)DestroyBusy);
	break;

    case ConfigureNotify:
	if ((busyPtr->width != Tk_Width(busyPtr->master)) ||
	    (busyPtr->height != Tk_Height(busyPtr->master)) ||
	    (busyPtr->masterX != Tk_X(busyPtr->master)) ||
	    (busyPtr->masterY != Tk_Y(busyPtr->master))) {
	    int x, y;

	    busyPtr->width = Tk_Width(busyPtr->master);
	    busyPtr->height = Tk_Height(busyPtr->master);
	    busyPtr->masterX = Tk_X(busyPtr->master);
	    busyPtr->masterY = Tk_Y(busyPtr->master);

	    x = y = 0;
	    if (busyPtr->parent != busyPtr->master) {
		Tk_Window ancestor;

		for (ancestor = busyPtr->master; ancestor != busyPtr->parent;
		    ancestor = Tk_Parent(ancestor)) {
		    x += Tk_X(ancestor) + Tk_Changes(ancestor)->border_width;
		    y += Tk_Y(ancestor) + Tk_Changes(ancestor)->border_width;
		}
	    }
	    if (busyPtr->inputWin != NULL) {
		Tk_MoveResizeWindow(busyPtr->inputWin, x, y, busyPtr->width,
		    busyPtr->height);
	    }
	}
	break;

    case MapNotify:
	if ((busyPtr->parent != busyPtr->master) && (busyPtr->isBusy)) {
	    Tk_MapWindow(busyPtr->inputWin);
	}
	break;

    case UnmapNotify:
	if (busyPtr->parent != busyPtr->master) {
	    Tk_UnmapWindow(busyPtr->inputWin);
	}
	break;
    }
}

/*
 * ------------------------------------------------------------------
 *
 * ConfigureBusy --
 *
 *	This procedure is called from the Tk event dispatcher. It
 * 	releases X resources and memory used by the busy window and
 *	updates the internal hash table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory and resources are released and the Tk event handler
 *	is removed.
 *
 * -------------------------------------------------------------------
 */
static int
ConfigureBusy(interp, busyPtr, argc, argv)
    Tcl_Interp *interp;
    Busy *busyPtr;
    int argc;
    char **argv;
{
    Tk_Cursor oldCursor;

    oldCursor = busyPtr->cursor;
    if (Tk_ConfigureWidget(interp, busyPtr->master, configSpecs, argc, argv,
	    (char *)busyPtr, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    if (busyPtr->cursor != oldCursor) {
	if (busyPtr->cursor == None) {
	    Tk_UndefineCursor(busyPtr->inputWin);
	} else {
	    Tk_DefineCursor(busyPtr->inputWin, busyPtr->cursor);
	}
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * CreateBusy --
 *
 *	Creates a child InputOnly window which obscures the parent
 *	window thereby effectively blocking device events.  The size
 *	and position of the busy window is exactly that of the master
 *	window.
 *
 * Results:
 *	Returns a pointer to the new busy window structure.
 *
 * Side effects:
 *	When the busy window is eventually mapped, it will screen
 *	device events (in the area of the master window) from reaching
 *	its parent window and its children.  User feed back can be
 *	achieved by changing the cursor.
 *
 * -------------------------------------------------------------------
 */
static Busy *
CreateBusy(interp, master, mainWin)
    Tcl_Interp *interp;		/* Interpreter to report error to */
    Tk_Window master;		/* Window hosting the busy window */
    Tk_Window mainWin;		/* Main window of interpreter. Used to
				 * search for a parent window by its
				 * pathname */
{
    Busy *busyPtr;
    int length;
    char *fmt, *name;
    Tk_Window tkwin;
    Tk_Window parent;
    int x, y;

    busyPtr = (Busy *) ckcalloc(1, sizeof(Busy));
    if (busyPtr == NULL) {
	Panic("can't allocate busy window structure");
    }
    x = y = 0;
    length = strlen(Tk_Name(master));
    name = (char *)ckalloc(length + 6);
    if (Tk_IsTopLevel(master)) {
	fmt = "_Busy";		/* Child */
	parent = master;
    } else {
	Tk_Window ancestor;

	fmt = "%s_Busy";	/* Sibling */
	parent = Tk_Parent(master);
	for (ancestor = master; ancestor != parent;
	    ancestor = Tk_Parent(ancestor)) {
	    x += Tk_X(ancestor) + Tk_Changes(ancestor)->border_width;
	    y += Tk_Y(ancestor) + Tk_Changes(ancestor)->border_width;
	    if (Tk_IsTopLevel(ancestor)) {
		break;
	    }
	}
    }
    sprintf(name, fmt, Tk_Name(master));
    tkwin = Tk_CreateWindow(interp, parent, name, (char *)NULL);
    ckfree((char *)name);

    if (tkwin == NULL) {
	return NULL;
    }
    busyPtr->display = Tk_Display(master);
    busyPtr->master = master;
    busyPtr->parent = parent;
    busyPtr->mainWin = mainWin;
    busyPtr->width = Tk_Width(master);
    busyPtr->height = Tk_Height(master);
    busyPtr->masterX = Tk_X(master);
    busyPtr->masterY = Tk_Y(master);
    busyPtr->cursor = None;
    busyPtr->inputWin = tkwin;

    Tk_SetClass(tkwin, "Busy");
    Blt_MakeInputOnlyWindowExist(tkwin);

    Tk_MoveResizeWindow(tkwin, x, y, busyPtr->width, busyPtr->height);
    Tk_RestackWindow(tkwin, Above, (Tk_Window)NULL);

    /*
     * Only worry if the busy window is destroyed.
     */
    Tk_CreateEventHandler(tkwin, StructureNotifyMask, BusyEventProc,
	(ClientData)busyPtr);

    /*
     * Indicate that the busy window's geometry is being managed.
     * This will also notify us if the busy window is ever packed.
     */
    Tk_ManageGeometry(tkwin, &busyMgrInfo, (ClientData)busyPtr);

    if (busyPtr->cursor != None) {
	Tk_DefineCursor(tkwin, busyPtr->cursor);
    }
    /*
     * Track events in the master window to see if it is resized or destroyed.
     */
    Tk_CreateEventHandler(master, StructureNotifyMask, MasterEventProc,
	(ClientData)busyPtr);
    return (busyPtr);
}

/*
 * ------------------------------------------------------------------
 *
 * DestroyBusy --
 *
 *	This procedure is called from the Tk event dispatcher. It
 *	releases X resources and memory used by the busy window and
 *	updates the internal hash table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory and resources are released and the Tk event handler
 *	is removed.
 *
 * -------------------------------------------------------------------
 */
static void
DestroyBusy(freeProcData)
    FreeProcData freeProcData;	/* Busy window structure record */
{
    Busy *busyPtr = (Busy *) freeProcData;
    Tcl_HashEntry *hPtr;

    Tk_FreeOptions(configSpecs, (char *)busyPtr, busyPtr->display, 0);
    hPtr = Tcl_FindHashEntry(&busyTable, (char *)busyPtr->master);
    if (hPtr != NULL) {
	Tcl_DeleteHashEntry(hPtr);
    }
    if (busyPtr->inputWin != NULL) {
	Tk_DeleteEventHandler(busyPtr->inputWin, StructureNotifyMask,
	    BusyEventProc, (ClientData)busyPtr);
	Tk_ManageGeometry(busyPtr->inputWin, (Tk_GeomMgr *)NULL,
	    (ClientData)busyPtr);
	Tk_DestroyWindow(busyPtr->inputWin);
    }
    Tk_DeleteEventHandler(busyPtr->master, StructureNotifyMask,
	MasterEventProc, (ClientData)busyPtr);
    ckfree((char *)busyPtr);
}

/*
 * ------------------------------------------------------------------
 *
 * GetBusy --
 *
 *	Returns the busy window structure associated with the master
 *	window, keyed by its path name.  The clientData argument is
 *	the main window of the interpreter, used to search for the
 *	master window in its own window hierarchy.
 *
 * Results:
 *	If path name represents a master window with a busy window, a
 *	pointer to the busy window structure is returned. Otherwise,
 *	NULL is returned and an error message is left in
 *	interp->result.
 *
 * -------------------------------------------------------------------
 */
static int
GetBusy(clientData, interp, pathName, busyPtrPtr)
    ClientData clientData;	/* Window used to reference search  */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    char *pathName;		/* Path name of parent window */
    Busy **busyPtrPtr;		/* Will contain address of busy window if
				 * found. */
{
    Tcl_HashEntry *hPtr;
    Tk_Window mainWin = (Tk_Window)clientData;
    Tk_Window master;

    master = Tk_NameToWindow(interp, pathName, mainWin);
    if (master == NULL) {
	return TCL_ERROR;
    }
    hPtr = Tcl_FindHashEntry(&busyTable, (char *)master);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "can't find busy window \"", pathName, "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    *busyPtrPtr = ((Busy *) Tcl_GetHashValue(hPtr));
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * HoldBusy --
 *
 *	Creates (if necessary) and maps a busy window, thereby
 *	preventing device events from being be received by the parent
 *	window and its children.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy
 *	window, it is unmapped and TCL_OK is returned. Otherwise,
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	The busy window is created and mapped, blocking events from
 *	the parent window and its children.
 *
 * -------------------------------------------------------------------
 */
static int
HoldBusy(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;		/* Window name and option pairs */
{
    Tk_Window mainWin = (Tk_Window)clientData;
    Tk_Window master;
    Tcl_HashEntry *hPtr;
    Busy *busyPtr;
    int isNew;
    int result;

    master = Tk_NameToWindow(interp, argv[0], mainWin);
    if (master == NULL) {
	return TCL_ERROR;
    }
    hPtr = Tcl_CreateHashEntry(&busyTable, (char *)master, &isNew);
    if (isNew) {
	busyPtr = (Busy *) CreateBusy(interp, master, mainWin);
	if (busyPtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetHashValue(hPtr, (char *)busyPtr);
    } else {
	busyPtr = (Busy *) Tcl_GetHashValue(hPtr);

	/*
	 * Raise and re-map the busy window whenever hold is reasserted.
	 */

	Tk_RestackWindow(busyPtr->inputWin, Above, (Tk_Window)NULL);
    }

    /*
     * Don't map the window unless the master window is also mapped
     */
    if (Tk_IsMapped(busyPtr->master)) {
	Tk_MapWindow(busyPtr->inputWin);
    } else {
	Tk_UnmapWindow(busyPtr->inputWin);
    }
    busyPtr->isBusy = TRUE;

    Tcl_Preserve((ClientData)busyPtr);
    result = ConfigureBusy(interp, busyPtr, argc - 1, argv + 1);
    Tcl_Release((ClientData)busyPtr);
    return result;
}

/*
 * ------------------------------------------------------------------
 *
 * StatusOper --
 *
 *	Returns the status of the busy window; whether it's blocking
 *	events or not.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy
 *	window, the status is returned via interp->result and TCL_OK
 *	is returned. Otherwise, TCL_ERROR is returned and an error
 *	message is left in interp->result.
 *
 * -------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StatusOper(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of interpreter */
    Tcl_Interp *interp;		/* Interpreter to report error to */
    int argc;			/* not used */
    char **argv;
{
    Busy *busyPtr;

    if (GetBusy(clientData, interp, argv[2], &busyPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData)busyPtr);
    Tcl_SetResult(interp, (busyPtr->isBusy) ? "1" : "0", TCL_STATIC);
    Tcl_Release((ClientData)busyPtr);
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * ForgetOper --
 *
 *	Destroys the busy window associated with the master window and
 *	arranges for internal resources to the released when they're
 *	not being used anymore.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy
 *	window, it is destroyed and TCL_OK is returned. Otherwise,
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	The busy window is removed.  Other related memory and resources
 *	are eventually released by the Tk dispatcher.
 *
 * -------------------------------------------------------------------
 */
static int
ForgetOper(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;
{
    Busy *busyPtr;
    register int i;
    Tcl_HashEntry *hPtr;
    Tk_Window mainWin = (Tk_Window)clientData;
    Tk_Window master;

    for (i = 2; i < argc; i++) {
	master = Tk_NameToWindow(interp, argv[i], mainWin);
	if (master == NULL) {
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&busyTable, (char *)master);
	if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "can't find busy window \"", argv[i],
		"\"", (char *)NULL);
	    return TCL_ERROR;
	}
	busyPtr = (Busy *) Tcl_GetHashValue(hPtr);

	/* Unmap the window even though it will be soon destroyed */
	if (busyPtr->inputWin != NULL) {
	    Tk_UnmapWindow(busyPtr->inputWin);
	}
	busyPtr->isBusy = FALSE;

	Tcl_EventuallyFree((ClientData)busyPtr, (Tcl_FreeProc *)DestroyBusy);
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * ReleaseOper --
 *
 *	Unmaps the busy window, thereby permitting device events
 *	to be received by the parent window and its children.
 *
 * Results:
 *	Returns a standard TCL result. If path name represents a busy
 *	window, it is unmapped and TCL_OK is returned. Otherwise,
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	The busy window is unmapped, allowing the parent window and
 *	its children to receive events again.
 *
 * -------------------------------------------------------------------
 */
static int
ReleaseOper(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;
{
    Busy *busyPtr;
    int i;

    for (i = 2; i < argc; i++) {
	if (GetBusy(clientData, interp, argv[i], &busyPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_Preserve((ClientData)busyPtr);
	if (busyPtr->inputWin != NULL) {
	    Tk_UnmapWindow(busyPtr->inputWin);
	}
	busyPtr->isBusy = FALSE;
	Tcl_Release((ClientData)busyPtr);
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * WindowsOper --
 *
 *	Reports the names of all widgets with busy windows attached to
 *	them, matching a given pattern.  If no pattern is given, all
 *	busy widgets are listed.
 *
 * Results:
 *	Returns a TCL list of the names of the widget with busy windows
 *	attached to them, regardless if the widget is currently busy
 *	or not.
 *
 * -------------------------------------------------------------------
 */
static int
WindowsOper(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;
{
    Tk_Window mainWin = (Tk_Window)clientData;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;
    Busy *busyPtr;

    for (hPtr = Tcl_FirstHashEntry(&busyTable, &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	busyPtr = (Busy *) Tcl_GetHashValue(hPtr);
	if (busyPtr->mainWin == mainWin) {
	    if ((argc != 3) ||
		(Tcl_StringMatch(Tk_PathName(busyPtr->master), argv[2]))) {
		Tcl_AppendElement(interp, Tk_PathName(busyPtr->master));
	    }
	}
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * IsBusyOper --
 *
 *	Reports the names of all widgets with busy windows attached to
 *	them, matching a given pattern.  If no pattern is given, all
 *	busy widgets are listed.
 *
 * Results:
 *	Returns a TCL list of the names of the widget with busy windows
 *	attached to them, regardless if the widget is currently busy
 *	or not.
 *
 * -------------------------------------------------------------------
 */
static int
IsBusyOper(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;
{
    Tk_Window mainWin = (Tk_Window)clientData;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;
    Busy *busyPtr;

    for (hPtr = Tcl_FirstHashEntry(&busyTable, &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	busyPtr = (Busy *) Tcl_GetHashValue(hPtr);
	if (busyPtr->mainWin == mainWin) {
	    if ((busyPtr->isBusy) && ((argc != 3) ||
		    (Tcl_StringMatch(Tk_PathName(busyPtr->master), argv[2])))) {
		Tcl_AppendElement(interp, Tk_PathName(busyPtr->master));
	    }
	}
    }
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------
 *
 * HoldOper --
 *
 *	Creates (if necessary) and maps a busy window, thereby
 *	preventing device events from being be received by the parent
 *      window and its children. The argument vector may contain
 *	option-value pairs of configuration options to be set.
 *
 * Results:
 *	Returns a standard TCL result.
 *
 * Side effects:
 *	The busy window is created and mapped, blocking events from the
 *	parent window and its children.
 *
 * -------------------------------------------------------------------
 */
static int
HoldOper(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;		/* Window name and option pairs */
{
    register int i, count;

    if (argv[1][0] == 'h') {	/* Command used "hold" keyword */
	argc--, argv++;
    }
    for (i = 1; i < argc; i++) {
	/*
	 * Find the end of the option-value pairs for this window.
	 */
	for (count = i + 1; count < argc; count += 2) {
	    if (argv[count][0] != '-') {
		break;
	    }
	}
	if (count > argc) {
	    count = argc;
	}
	if (HoldBusy(clientData, interp, count - i, argv + i) != TCL_OK) {
	    return TCL_ERROR;
	}
	i = count;
    }
    return TCL_OK;
}

/* ARGSUSED*/
static int
CgetOper(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;		/* Widget pathname and option switch */
{
    Busy *busyPtr;
    int result;

    if (GetBusy(clientData, interp, argv[2], &busyPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData)busyPtr);
    result = Tk_ConfigureValue(interp, busyPtr->master, configSpecs,
	(char *)busyPtr, argv[3], 0);
    Tcl_Release((ClientData)busyPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOper --
 *
 *	This procedure is called to process an argv/argc list in order
 *	to configure (or reconfigure) a busy window.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information get set for busyPtr; old resources
 *	get freed, if there were any.  The busy window destroyed and
 *	recreated in a new parent window.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOper(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;		/* Master window path name and options */
{
    Busy *busyPtr;
    int result;

    if (GetBusy(clientData, interp, argv[2], &busyPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData)busyPtr);
    if (argc == 3) {
	result = Tk_ConfigureInfo(interp, busyPtr->master, configSpecs,
	    (char *)busyPtr, (char *)NULL, 0);
    } else if (argc == 4) {
	result = Tk_ConfigureInfo(interp, busyPtr->master, configSpecs,
	    (char *)busyPtr, argv[3], 0);
    } else {
	result = ConfigureBusy(interp, busyPtr, argc - 3, argv + 3);
    }
    Tcl_Release((ClientData)busyPtr);
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Busy Sub-command specification:
 *
 *	- Name of the sub-command.
 *	- Minimum number of characters needed to unambiguously
 *        recognize the sub-command.
 *	- Pointer to the function to be called for the sub-command.
 *	- Minimum number of arguments accepted.
 *	- Maximum number of arguments accepted.
 *	- String to be displayed for usage (arguments only).
 *
 *--------------------------------------------------------------
 */
static Blt_OperSpec operSpecs[] =
{
    {"cget", 2, (Blt_OperProc)CgetOper, 4, 4, "window option",},
    {"configure", 2, (Blt_OperProc)ConfigureOper, 3, 0,
	"window ?options?...",},
    {"forget", 1, (Blt_OperProc)ForgetOper, 2, 0, "?window?...",},
    {"hold", 3, (Blt_OperProc)HoldOper, 3, 0,
	"window ?options?... ?window options?...",},
    {"isbusy", 1, (Blt_OperProc)IsBusyOper, 2, 3, "?pattern?",},
    {"release", 1, (Blt_OperProc)ReleaseOper, 2, 0, "?window?...",},
    {"status", 1, (Blt_OperProc)StatusOper, 3, 3, "window",},
    {"windows", 1, (Blt_OperProc)WindowsOper, 2, 3, "?pattern?",},
};
static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

/*
 *----------------------------------------------------------------------
 *
 * BusyCmd --
 *
 *	This procedure is invoked to process the "busy" Tcl command.
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
static int
BusyCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter */
    Tcl_Interp *interp;		/* Interpreter associated with command */
    int argc;
    char **argv;
{
    Blt_OperProc proc;
    int result;

    if (!initialized) {
	Tcl_InitHashTable(&busyTable, TCL_ONE_WORD_KEYS);
	initialized = 1;
    }
    if ((argc > 1) && (argv[1][0] == '.')) {
	return (HoldOper(clientData, interp, argc, argv));
    }
    proc = Blt_LookupOperation(interp, numSpecs, operSpecs, BLT_OPER_ARG1,
	argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, argc, argv);
    return (result);
}

int
Blt_BusyInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = { "busy", BUSY_VERSION, BusyCmd, };

    return (Blt_InitCmd(interp, &cmdSpec));
}

#endif /* !NO_BUSY */
