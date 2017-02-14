/*
 * bltInput.c --
 *
 *	This module implements InputOnly windows for the BLT toolkit.
 *
 * Copyright 1995-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef	NO_BUSY

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#ifdef XNQueryInputStyle
#   define TK_USE_INPUT_METHODS
#endif


/*
 *----------------------------------------------------------------------
 *
 * DoConfigureNotify --
 *
 *	Generate a ConfigureNotify event describing the current
 *	configuration of a window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An event is generated and processed by Tk_HandleEvent.
 *
 *----------------------------------------------------------------------
 */
static void
DoConfigureNotify(winPtr)
    Tk_FakeWin *winPtr;		/* Window whose configuration was just changed. */
{
    XEvent event;

    event.type = ConfigureNotify;
    event.xconfigure.serial = LastKnownRequestProcessed(winPtr->display);
    event.xconfigure.send_event = False;
    event.xconfigure.display = winPtr->display;
    event.xconfigure.event = winPtr->window;
    event.xconfigure.window = winPtr->window;
    event.xconfigure.x = winPtr->changes.x;
    event.xconfigure.y = winPtr->changes.y;
    event.xconfigure.width = winPtr->changes.width;
    event.xconfigure.height = winPtr->changes.height;
    event.xconfigure.border_width = winPtr->changes.border_width;
    if (winPtr->changes.stack_mode == Above) {
	event.xconfigure.above = winPtr->changes.sibling;
    } else {
	event.xconfigure.above = None;
    }
    event.xconfigure.override_redirect = winPtr->atts.override_redirect;
    Tk_HandleEvent(&event);
}

/*
 *--------------------------------------------------------------
 *
 * Blt_MakeInputOnlyWindowExist --
 *
 *	Ensure that a particular window actually exists.  This
 *	procedure shouldn't normally need to be invoked from
 *	outside the Tk package, but may be needed if someone
 *	wants to manipulate a window before mapping it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the procedure returns, the X window associated with
 *	tkwin is guaranteed to exist.  This may require the
 *	window's ancestors to be created also.
 *
 *--------------------------------------------------------------
 */

void
Blt_MakeInputOnlyWindowExist(tkwin)
    Tk_Window tkwin;		/* Token for window. */
{
    Tk_FakeWin *winPtr = (Tk_FakeWin *) tkwin;
    Tk_FakeWin *winPtr2;
    Window parent;
    Tcl_HashEntry *hPtr;
    int notUsed;
    TkDisplay *dispPtr;

#define nextPtr		dummy4
#define dirtyChanges 	dummy6
#define dirtyAtts 	dummy7
#define inputContext 	dummy9

    if (winPtr->window != None) {
	return;
    }
    if ((winPtr->parentPtr == NULL) || (winPtr->flags & TK_TOP_LEVEL)) {
	parent = XRootWindow(winPtr->display, winPtr->screenNum);
    } else {
	if (Tk_WindowId(winPtr->parentPtr) == None) {
	    Tk_MakeWindowExist(winPtr->parentPtr);
	}
	parent = Tk_WindowId(winPtr->parentPtr);
    }

    /*
     * Ignore the important events while the window is mapped.
     */
    winPtr->atts.do_not_propagate_mask = (KeyPressMask | KeyReleaseMask |
	ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    winPtr->atts.event_mask = (KeyPressMask | KeyReleaseMask |
	ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    winPtr->changes.border_width = 0;
    winPtr->depth = 0;
    winPtr->window = XCreateWindow(winPtr->display, parent,
	winPtr->changes.x, winPtr->changes.y,
	(unsigned)winPtr->changes.width,	/* width */
	(unsigned)winPtr->changes.height,	/* height */
	0,			/* border_width */
	0,			/* depth */
	InputOnly,		/* class */
	CopyFromParent,		/* visual */
	(CWDontPropagate | CWEventMask),	/* valuemask */
	&(winPtr->atts) /* attributes */ );

    dispPtr = (TkDisplay *)winPtr->dummy1;
    hPtr = Tcl_CreateHashEntry(&(dispPtr->winTable), (char *)winPtr->window,
	&notUsed);
    Tcl_SetHashValue(hPtr, winPtr);

    winPtr->dirtyAtts = 0;
    winPtr->dirtyChanges = 0;
#ifdef TK_USE_INPUT_METHODS
    winPtr->inputContext = NULL;
#endif /* TK_USE_INPUT_METHODS */

    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	/*
	 * If any siblings higher up in the stacking order have already
	 * been created then move this window to its rightful position
	 * in the stacking order.
	 *
	 * NOTE: this code ignores any changes anyone might have made
	 * to the sibling and stack_mode field of the window's attributes,
	 * so it really isn't safe for these to be manipulated except
	 * by calling Tk_RestackWindow.
	 */

	for (winPtr2 = (Tk_FakeWin *) winPtr->nextPtr; winPtr2 != NULL;
	    winPtr2 = (Tk_FakeWin *) winPtr2->nextPtr) {
	    if ((winPtr2->window != None) && !(winPtr2->flags & TK_TOP_LEVEL)) {
		XWindowChanges changes;
		changes.sibling = winPtr2->window;
		changes.stack_mode = Below;
		XConfigureWindow(winPtr->display, winPtr->window,
		    CWSibling | CWStackMode, &changes);
		break;
	    }
	}
    }
    /*
     * Issue a ConfigureNotify event if there were deferred configuration
     * changes (but skip it if the window is being deleted;  the
     * ConfigureNotify event could cause problems if we're being called
     * from Tk_DestroyWindow under some conditions).
     */

    if ((winPtr->flags & TK_NEED_CONFIG_NOTIFY)
	&& !(winPtr->flags & TK_ALREADY_DEAD)) {
	winPtr->flags &= ~TK_NEED_CONFIG_NOTIFY;
	DoConfigureNotify(tkwin);
    }
}
#endif /* !NO_BUSY */
