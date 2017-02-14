/* 
 * tkFocus.c --
 *
 *	This file contains procedures that manage the input
 *	focus for Tk.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkFocus.c 1.45 97/08/07 14:39:47
 */

#include "tkInt.h"
#include "tkPort.h"


/*
 * For each top-level window that has ever received the focus, there
 * is a record of the following type:
 */

typedef struct TkFocusInfo {
    TkWindow *topLevelPtr;	/* Information about top-level window. */
    TkWindow *focusWinPtr;	/* The next time the focus comes to this
				 * top-level, it will be given to this
				 * window. */
    struct TkFocusInfo *nextPtr;/* Next in list of all focus records for
				 * a given application. */
} FocusInfo;

/*
 * Global used for debugging.
 */
int tclFocusDebug = 0;

/*
 * The following magic value is stored in the "send_event" field of
 * FocusIn and FocusOut events that are generated in this file.  This
 * allows us to separate "real" events coming from the server from
 * those that we generated.
 */

#define GENERATED_EVENT_MAGIC ((Bool) 0x547321ac)

/*
 * Forward declarations for procedures defined in this file:
 */


static void		FocusMapProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static TkWindow *	GetFocus _ANSI_ARGS_((TkWindow *winPtr));
static void		GenerateFocusEvents _ANSI_ARGS_((TkWindow *sourcePtr,
			    TkWindow *destPtr));
static void		SetFocus _ANSI_ARGS_((TkWindow *winPtr, int force));

/*
 *--------------------------------------------------------------
 *
 * Tk_FocusCmd --
 *
 *	This procedure is invoked to process the "focus" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
Tk_FocusCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr = (TkWindow *) clientData;
    TkWindow *newPtr, *focusWinPtr, *topLevelPtr;
    FocusInfo *focusPtr;
    char c;
    size_t length;

    /*
     * If invoked with no arguments, just return the current focus window.
     */

    if (argc == 1) {
	focusWinPtr = GetFocus(winPtr);
	if (focusWinPtr != NULL) {
	    interp->result = focusWinPtr->pathName;
	}
	return TCL_OK;
    }

    /*
     * If invoked with a single argument beginning with "." then focus
     * on that window.
     */

    if (argc == 2) {
	if (argv[1][0] == 0) {
	    return TCL_OK;
	}
	if (argv[1][0] == '.') {
	    newPtr = (TkWindow *) Tk_NameToWindow(interp, argv[1], tkwin);
	    if (newPtr == NULL) {
		return TCL_ERROR;
	    }
	    if (!(newPtr->flags & TK_ALREADY_DEAD)) {
		SetFocus(newPtr, 0);
	    }
	    return TCL_OK;
	}
    }

    length = strlen(argv[1]);
    c = argv[1][1];
    if ((c == 'd') && (strncmp(argv[1], "-displayof", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " -displayof window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	newPtr = (TkWindow *) Tk_NameToWindow(interp, argv[2], tkwin);
	if (newPtr == NULL) {
	    return TCL_ERROR;
	}
	newPtr = GetFocus(newPtr);
	if (newPtr != NULL) {
	    interp->result = newPtr->pathName;
	}
    } else if ((c == 'f') && (strncmp(argv[1], "-force", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " -force window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argv[2][0] == 0) {
	    return TCL_OK;
	}
	newPtr = (TkWindow *) Tk_NameToWindow(interp, argv[2], tkwin);
	if (newPtr == NULL) {
	    return TCL_ERROR;
	}
	SetFocus(newPtr, 1);
    } else if ((c == 'l') && (strncmp(argv[1], "-lastfor", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " -lastfor window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	newPtr = (TkWindow *) Tk_NameToWindow(interp, argv[2], tkwin);
	if (newPtr == NULL) {
	    return TCL_ERROR;
	}
	for (topLevelPtr = newPtr; topLevelPtr != NULL;
		topLevelPtr = topLevelPtr->parentPtr)  {
	    if (topLevelPtr->flags & TK_TOP_LEVEL) {
		for (focusPtr = newPtr->mainPtr->focusPtr; focusPtr != NULL;
			focusPtr = focusPtr->nextPtr) {
		    if (focusPtr->topLevelPtr == topLevelPtr) {
			interp->result = focusPtr->focusWinPtr->pathName;
			return TCL_OK;
		    }
		}
		interp->result = topLevelPtr->pathName;
		return TCL_OK;
	    }
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be -displayof, -force, or -lastfor", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TkFocusFilterEvent --
 *
 *	This procedure is invoked by Tk_HandleEvent when it encounters
 *	a FocusIn, FocusOut, Enter, or Leave event.
 *
 * Results:
 *	A return value of 1 means that Tk_HandleEvent should process
 *	the event normally (i.e. event handlers should be invoked).
 *	A return value of 0 means that this event should be ignored.
 *
 * Side effects:
 *	Additional events may be generated, and the focus may switch.
 *
 *--------------------------------------------------------------
 */

int
TkFocusFilterEvent(winPtr, eventPtr)
    TkWindow *winPtr;		/* Window that focus event is directed to. */
    XEvent *eventPtr;		/* FocusIn, FocusOut, Enter, or Leave
				 * event. */
{
    /*
     * Design notes: the window manager and X server work together to
     * transfer the focus among top-level windows.  This procedure takes
     * care of transferring the focus from a top-level or wrapper window
     * to the actual window within that top-level that has the focus. 
     * We do this by synthesizing X events to move the focus around. 
     * None of the FocusIn and FocusOut events generated by X are ever
     * used outside of this procedure;  only the synthesized events get
     * through to the rest of the application.  At one point (e.g.
     * Tk4.0b1) Tk used to call X to move the focus from a top-level to
     * one of its descendants, then just pass through the events
     * generated by X. This approach didn't work very well, for a
     * variety of reasons. For example, if X generates the events they
     * go at the back of the event queue, which could cause problems if
     * other things have already happened, such as moving the focus to
     * yet another window.
     */

    FocusInfo *focusPtr;
    TkDisplay *dispPtr = winPtr->dispPtr;
    TkWindow *newFocusPtr;
    int retValue, delta;

    /*
     * If this was a generated event, just turn off the generated
     * flag and pass the event through to Tk bindings.
     */

    if (eventPtr->xfocus.send_event == GENERATED_EVENT_MAGIC) {
	eventPtr->xfocus.send_event = 0;
	return 1;
    }

    /*
     * Check for special events generated by embedded applications to
     * request the input focus.  If this is one of those events, make
     * the change in focus and return without any additional processing
     * of the event (note: the "detail" field of the event indicates
     * whether to claim the focus even if we don't already have it).
     */

    if ((eventPtr->xfocus.mode == EMBEDDED_APP_WANTS_FOCUS)
	    && (eventPtr->type == FocusIn)) {
	SetFocus(winPtr, eventPtr->xfocus.detail);
	return 0;
    }

    /*
     * This was not a generated event.  We'll return 1 (so that the
     * event will be processed) if it's an Enter or Leave event, and
     * 0 (so that the event won't be processed) if it's a FocusIn or
     * FocusOut event.
     */

    retValue = 0;
    if (eventPtr->type == FocusIn) {
	/*
	 * Skip FocusIn events that cause confusion
	 * NotifyVirtual and NotifyNonlinearVirtual - Virtual events occur
	 *	on windows in between the origin and destination of the
	 *	focus change.  For FocusIn we may see this when focus
	 *	goes into an embedded child.  We don't care about this,
	 *	although we may end up getting a NotifyPointer later.
	 * NotifyInferior - focus is coming to us from an embedded child.
	 *	When focus is on an embeded focus, we still think we have
	 *	the focus, too, so this message doesn't change our state.
	 * NotifyPointerRoot - should never happen because this is sent
	 *	to the root window.
	 *
	 * Interesting FocusIn events are
	 * NotifyAncestor - focus is coming from our parent, probably the root.
	 * NotifyNonlinear - focus is coming from a different branch, probably
	 *	another toplevel.
	 * NotifyPointer - implicit focus because of the mouse position.
	 *	This is	only interesting on toplevels, when it means that the
	 *	focus has been set to the root window but the mouse is over
	 *	this toplevel.  We take the focus implicitly (probably no
	 *	window manager)
	 */

	if ((eventPtr->xfocus.detail == NotifyVirtual)
		|| (eventPtr->xfocus.detail == NotifyNonlinearVirtual)
		|| (eventPtr->xfocus.detail == NotifyPointerRoot)
		|| (eventPtr->xfocus.detail == NotifyInferior)) {
	    return retValue;
	}
    } else if (eventPtr->type == FocusOut) {
	/*
	 * Skip FocusOut events that cause confusion.
	 * NotifyPointer - the pointer is in us or a child, and we are loosing
	 *	focus because of an XSetInputFocus.  Other focus events
	 *	will set our state properly.
	 * NotifyPointerRoot - should never happen because this is sent
	 *	to the root window.
	 * NotifyInferior - focus leaving us for an embedded child.  We
	 *	retain a notion of focus when an embedded child has focus.
	 *
	 * Interesting events are:
	 * NotifyAncestor - focus is going to root.
	 * NotifyNonlinear - focus is going to another branch, probably
	 *	another toplevel.
	 * NotifyVirtual, NotifyNonlinearVirtual - focus is passing through,
	 *	and we need to make sure we track this.
	 */

	if ((eventPtr->xfocus.detail == NotifyPointer)
		|| (eventPtr->xfocus.detail == NotifyPointerRoot)
		|| (eventPtr->xfocus.detail == NotifyInferior)) {
	    return retValue;
	}
    } else {
	retValue = 1;
	if (eventPtr->xcrossing.detail == NotifyInferior) {
	    return retValue;
	}
    }

    /*
     * If winPtr isn't a top-level window than just ignore the event.
     */

    winPtr = TkWmFocusToplevel(winPtr);
    if (winPtr == NULL) {
	return retValue;
    }

    /*
     * If there is a grab in effect and this window is outside the
     * grabbed tree, then ignore the event.
     */

    if (TkGrabState(winPtr) == TK_GRAB_EXCLUDED)  {
	return retValue;
    }

    /*
     * It is possible that there were outstanding FocusIn and FocusOut
     * events on their way to us at the time the focus was changed
     * internally with the "focus" command.  If so, these events could
     * potentially cause us to lose the focus (switch it to the window
     * of the last FocusIn event) even though the focus change occurred
     * after those events.  The following code detects this and ignores
     * the stale events.
     *
     * Note: the focusSerial is only generated by TkpChangeFocus,
     * whereas in Tk 4.2 there was always a nop marker generated.
     */

    delta = eventPtr->xfocus.serial - winPtr->mainPtr->focusSerial;
    if (delta < 0) {
	return retValue;
    }

    /*
     * Find the FocusInfo structure for the window, and make a new one
     * if there isn't one already.
     */

    for (focusPtr = winPtr->mainPtr->focusPtr; focusPtr != NULL;
	    focusPtr = focusPtr->nextPtr) {
	if (focusPtr->topLevelPtr == winPtr) {
	    break;
	}
    }
    if (focusPtr == NULL) {
	focusPtr = (FocusInfo *) ckalloc(sizeof(FocusInfo));
	focusPtr->topLevelPtr = focusPtr->focusWinPtr = winPtr;
	focusPtr->nextPtr = winPtr->mainPtr->focusPtr;
	winPtr->mainPtr->focusPtr = focusPtr;
    }
    newFocusPtr = focusPtr->focusWinPtr;

    if (eventPtr->type == FocusIn) {
	GenerateFocusEvents(winPtr->mainPtr->focusWinPtr, newFocusPtr);
	winPtr->mainPtr->focusWinPtr = newFocusPtr;

	/*
	 * NotifyPointer gets set when the focus has been set to the root window
	 * but we have the pointer.  We'll treat this like an implicit
	 * focus in event so that upon Leave events we release focus.
	 */

	if (eventPtr->xfocus.detail == NotifyPointer) {
	    dispPtr->implicitWinPtr = winPtr;
	} else {
	    dispPtr->implicitWinPtr = NULL;
	}
    } else if (eventPtr->type == FocusOut) {
	GenerateFocusEvents(winPtr->mainPtr->focusWinPtr, (TkWindow *) NULL);
	winPtr->mainPtr->focusWinPtr = NULL;
    } else if (eventPtr->type == EnterNotify) {
	/*
	 * If there is no window manager, or if the window manager isn't
	 * moving the focus around (e.g. the disgusting "NoTitleFocus"
	 * option has been selected in twm), then we won't get FocusIn
	 * or FocusOut events.  Instead, the "focus" field will be set
	 * in an Enter event to indicate that we've already got the focus
	 * when the mouse enters the window (even though we didn't get
	 * a FocusIn event).  Watch for this and grab the focus when it
	 * happens.  Note: if this is an embedded application then don't
	 * accept the focus implicitly like this;  the container
	 * application will give us the focus explicitly if it wants us
	 * to have it.
	 */

	if (eventPtr->xcrossing.focus &&
                (winPtr->mainPtr->focusWinPtr == NULL)
		&& !(winPtr->flags & TK_EMBEDDED)) {
	    if (tclFocusDebug) {
		printf("Focussed implicitly on %s\n",
			newFocusPtr->pathName);
	    }

	    GenerateFocusEvents(winPtr->mainPtr->focusWinPtr, newFocusPtr);
	    winPtr->mainPtr->focusWinPtr = newFocusPtr;
	    dispPtr->implicitWinPtr = winPtr;
	}
    } else if (eventPtr->type == LeaveNotify) {
	/*
	 * If the pointer just left a window for which we automatically
	 * claimed the focus on enter, move the focus back to the root
	 * window, where it was before we claimed it above.  Note:
	 * dispPtr->implicitWinPtr may not be the same as
	 * winPtr->mainPtr->focusWinPtr (e.g. because the "focus"
	 * command was used to redirect the focus after it arrived at
	 * dispPtr->implicitWinPtr)!!  In addition, we generate events
	 * because the window manager won't give us a FocusOut event when
	 * we focus on the root. 
	 */

	if (dispPtr->implicitWinPtr != NULL) {
	    if (tclFocusDebug) {
		printf("Defocussed implicit Async\n");
	    }
	    GenerateFocusEvents(winPtr->mainPtr->focusWinPtr, (TkWindow *) NULL);
	    XSetInputFocus(dispPtr->display, PointerRoot, RevertToPointerRoot,
		    CurrentTime);
	    winPtr->mainPtr->focusWinPtr = NULL;
	    dispPtr->implicitWinPtr = NULL;
	}
    }
    return retValue;
}

/*
 *----------------------------------------------------------------------
 *
 * SetFocus --
 *
 *	This procedure is invoked to change the focus window for a
 *	given display in a given application.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Event handlers may be invoked to process the change of
 *	focus.
 *
 *----------------------------------------------------------------------
 */

static void
SetFocus(winPtr, force)
    TkWindow *winPtr;		/* Window that is to be the new focus for
				 * its display and application. */
    int force;			/* If non-zero, set the X focus to this
				 * window even if the application doesn't
				 * currently have the X focus. */
{
    FocusInfo *focusPtr;
    TkWindow *topLevelPtr;
    int allMapped;

    if (winPtr == winPtr->mainPtr->focusWinPtr) {
	return;
    }

    /*
     * Find the top-level window for winPtr, then find (or create)
     * a record for the top-level.  Also see whether winPtr and all its
     * ancestors are mapped.
     */

    allMapped = 1;
    for (topLevelPtr = winPtr; ; topLevelPtr = topLevelPtr->parentPtr)  {
	if (topLevelPtr == NULL) {
	    /*
	     * The window is being deleted.  No point in worrying about
	     * giving it the focus.
	     */
	    return;
	}
	if (!(topLevelPtr->flags & TK_MAPPED)) {
	    allMapped = 0;
	}
	if (topLevelPtr->flags & TK_TOP_LEVEL) {
	    break;
	}
    }

    /*
     * If the new focus window isn't mapped, then we can't focus on it
     * (X will generate an error, for example).  Instead, create an
     * event handler that will set the focus to this window once it gets
     * mapped.  At the same time, delete any old handler that might be
     * around;  it's no longer relevant.
     */

    if (winPtr->mainPtr->focusOnMapPtr != NULL) {
	Tk_DeleteEventHandler(
		(Tk_Window) winPtr->mainPtr->focusOnMapPtr,
		StructureNotifyMask, FocusMapProc,
		(ClientData) winPtr->mainPtr->focusOnMapPtr);
	winPtr->mainPtr->focusOnMapPtr = NULL;
    }
    if (!allMapped) {
	Tk_CreateEventHandler((Tk_Window) winPtr,
		VisibilityChangeMask, FocusMapProc,
		(ClientData) winPtr);
	winPtr->mainPtr->focusOnMapPtr = winPtr;
	winPtr->mainPtr->forceFocus = force;
	return;
    }

    for (focusPtr = winPtr->mainPtr->focusPtr; focusPtr != NULL;
	    focusPtr = focusPtr->nextPtr) {
	if (focusPtr->topLevelPtr == topLevelPtr) {
	    break;
	}
    }
    if (focusPtr == NULL) {
	focusPtr = (FocusInfo *) ckalloc(sizeof(FocusInfo));
	focusPtr->topLevelPtr = topLevelPtr;
	focusPtr->nextPtr = winPtr->mainPtr->focusPtr;
	winPtr->mainPtr->focusPtr = focusPtr;
    }
    focusPtr->focusWinPtr = winPtr;

    /*
     * Reset the window system's focus window and generate focus events,
     * with two special cases:
     *
     * 1. If the application is embedded and doesn't currently have the
     *    focus.  Don't set the focus directly.  Instead, see if the
     *    embedding code can claim the focus from the enclosing
     *    container.
     * 2. Otherwise, if the application doesn't currently have the
     *    focus, don't change the window system's focus unless it was
     *    already in this application or "force" was specified.
     */

    if ((topLevelPtr->flags & TK_EMBEDDED)
	    && (winPtr->mainPtr->focusWinPtr == NULL)) {
	TkpClaimFocus(topLevelPtr, force);
    } else if ((winPtr->mainPtr->focusWinPtr != NULL) || force) {
	/*
	 * Generate events to shift focus between Tk windows.
	 * We do this regardless of what TkpChangeFocus does with
	 * the real X focus so that Tk widgets track focus commands
	 * when there is no window manager.  GenerateFocusEvents will
	 * set up a serial number marker so we discard focus events
	 * that are triggered by the ChangeFocus.
	 */

	TkpChangeFocus(TkpGetWrapperWindow(topLevelPtr), force);
	GenerateFocusEvents(winPtr->mainPtr->focusWinPtr, winPtr);
	winPtr->mainPtr->focusWinPtr = winPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetFocus --
 *
 *	Given a window, this procedure returns the current focus
 *	window for its application and display.
 *
 * Results:
 *	The return value is a pointer to the window that currently
 *	has the input focus for the specified application and
 *	display, or NULL if none.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TkWindow *
GetFocus(winPtr)
    TkWindow *winPtr;		/* Window that selects an application
				 * and a display. */
{
    TkWindow *focusWinPtr;

    focusWinPtr = winPtr->mainPtr->focusWinPtr;
    if ((focusWinPtr != NULL) && (focusWinPtr->mainPtr == winPtr->mainPtr)) {
	return focusWinPtr;
    }
    return (TkWindow *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFocusKeyEvent --
 *
 *	Given a window and a key press or release event that arrived for
 *	the window, use information about the keyboard focus to compute
 *	which window should really get the event.  In addition, update
 *	the event to refer to its new window.
 *
 * Results:
 *	The return value is a pointer to the window that has the input
 *	focus in winPtr's application, or NULL if winPtr's application
 *	doesn't have the input focus.  If a non-NULL value is returned,
 *	eventPtr will be updated to refer properly to the focus window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkWindow *
TkFocusKeyEvent(winPtr, eventPtr)
    TkWindow *winPtr;		/* Window that selects an application
				 * and a display. */
    XEvent *eventPtr;		/* X event to redirect (should be KeyPress
				 * or KeyRelease). */
{
    TkWindow *focusWinPtr;
    int focusX, focusY, vRootX, vRootY, vRootWidth, vRootHeight;

    focusWinPtr = winPtr->mainPtr->focusWinPtr;
    if ((focusWinPtr != NULL) && (focusWinPtr->mainPtr == winPtr->mainPtr)) {
	/*
	 * Map the x and y coordinates to make sense in the context of
	 * the focus window, if possible (make both -1 if the map-from
	 * and map-to windows don't share the same screen).
	 */

	if ((focusWinPtr->display != winPtr->display)
		|| (focusWinPtr->screenNum != winPtr->screenNum)) {
	    eventPtr->xkey.x = -1;
	    eventPtr->xkey.y = -1;
	} else {
	    Tk_GetVRootGeometry((Tk_Window) focusWinPtr, &vRootX, &vRootY,
		    &vRootWidth, &vRootHeight);
	    Tk_GetRootCoords((Tk_Window) focusWinPtr, &focusX, &focusY);
	    eventPtr->xkey.x = eventPtr->xkey.x_root - vRootX - focusX;
	    eventPtr->xkey.y = eventPtr->xkey.y_root - vRootY - focusY;
	}
	eventPtr->xkey.window = focusWinPtr->window;
	return focusWinPtr;
    }

    /*
     * The event doesn't belong to us.  Perhaps, due to embedding, it
     * really belongs to someone else.  Give the embedding code a chance
     * to redirect the event.
     */

    TkpRedirectKeyEvent(winPtr, eventPtr);
    return (TkWindow *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFocusDeadWindow --
 *
 *	This procedure is invoked when it is determined that
 *	a window is dead.  It cleans up focus-related information
 *	about the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Various things get cleaned up and recycled.
 *
 *----------------------------------------------------------------------
 */

void
TkFocusDeadWindow(winPtr)
    register TkWindow *winPtr;		/* Information about the window
					 * that is being deleted. */
{
    FocusInfo *focusPtr, *prevPtr;
    TkDisplay *dispPtr = winPtr->dispPtr;

    /*
     * Search for focus records that refer to this window either as
     * the top-level window or the current focus window.
     */

    for (prevPtr = NULL, focusPtr = winPtr->mainPtr->focusPtr;
	    focusPtr != NULL;
	    prevPtr = focusPtr, focusPtr = focusPtr->nextPtr) {
	if (winPtr == focusPtr->topLevelPtr) {
	    /*
	     * The top-level window is the one being deleted: free
	     * the focus record and release the focus back to PointerRoot
	     * if we acquired it implicitly.
	     */

	    if (dispPtr->implicitWinPtr == winPtr) {
		if (tclFocusDebug) {
		    printf("releasing focus to root after %s died\n",
			    focusPtr->topLevelPtr->pathName);
		}
		dispPtr->implicitWinPtr = NULL;
		winPtr->mainPtr->focusWinPtr = NULL;
	    }
	    if (winPtr->mainPtr->focusWinPtr == focusPtr->focusWinPtr) {
		winPtr->mainPtr->focusWinPtr = NULL;
	    }
	    if (prevPtr == NULL) {
		winPtr->mainPtr->focusPtr = focusPtr->nextPtr;
	    } else {
		prevPtr->nextPtr = focusPtr->nextPtr;
	    }
	    ckfree((char *) focusPtr);
	    break;
	} else if (winPtr == focusPtr->focusWinPtr) {
	    /*
	     * The deleted window had the focus for its top-level:
	     * move the focus to the top-level itself.
	     */

	    focusPtr->focusWinPtr = focusPtr->topLevelPtr;
	    if ((winPtr->mainPtr->focusWinPtr == winPtr)
		    && !(focusPtr->topLevelPtr->flags & TK_ALREADY_DEAD)) {
		if (tclFocusDebug) {
		    printf("forwarding focus to %s after %s died\n",
			    focusPtr->topLevelPtr->pathName, winPtr->pathName);
		}
		GenerateFocusEvents(winPtr->mainPtr->focusWinPtr,
			focusPtr->topLevelPtr);
		winPtr->mainPtr->focusWinPtr = focusPtr->topLevelPtr;
	    }
	    break;
	}
    }

    if (winPtr->mainPtr->focusOnMapPtr == winPtr) {
	winPtr->mainPtr->focusOnMapPtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateFocusEvents --
 *
 *	This procedure is called to create FocusIn and FocusOut events to
 *	move the input focus from one window to another.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	FocusIn and FocusOut events are generated.
 *
 *----------------------------------------------------------------------
 */

static void
GenerateFocusEvents(sourcePtr, destPtr)
    TkWindow *sourcePtr;	/* Window that used to have the focus (may
				 * be NULL). */
    TkWindow *destPtr;		/* New window to have the focus (may be
				 * NULL). */

{
    XEvent event;
    TkWindow *winPtr;

    winPtr = sourcePtr;
    if (winPtr == NULL) {
	winPtr = destPtr;
	if (winPtr == NULL) {
	    return;
	}
    }

    event.xfocus.serial = LastKnownRequestProcessed(winPtr->display);
    event.xfocus.send_event = GENERATED_EVENT_MAGIC;
    event.xfocus.display = winPtr->display;
    event.xfocus.mode = NotifyNormal;
    TkInOutEvents(&event, sourcePtr, destPtr, FocusOut, FocusIn,
	    TCL_QUEUE_MARK);
}

/*
 *----------------------------------------------------------------------
 *
 * FocusMapProc --
 *
 *	This procedure is called as an event handler for VisibilityNotify
 *	events, if a window receives the focus at a time when its
 *	toplevel isn't mapped.  The procedure is needed because X
 *	won't allow the focus to be set to an unmapped window;  we
 *	detect when the toplevel is mapped and set the focus to it then.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If this is a map event, the focus gets set to the toplevel
 *	given by clientData.
 *
 *----------------------------------------------------------------------
 */

static void
FocusMapProc(clientData, eventPtr)
    ClientData clientData;	/* Toplevel window. */
    XEvent *eventPtr;		/* Information about event. */
{
    TkWindow *winPtr = (TkWindow *) clientData;

    if (eventPtr->type == VisibilityNotify) {
	if (tclFocusDebug) {
	    printf("auto-focussing on %s, force %d\n", winPtr->pathName,
		    winPtr->mainPtr->forceFocus);
	}
	Tk_DeleteEventHandler((Tk_Window) winPtr, VisibilityChangeMask,
		FocusMapProc, clientData);
	winPtr->mainPtr->focusOnMapPtr = NULL;
	SetFocus(winPtr, winPtr->mainPtr->forceFocus);
    }
}
