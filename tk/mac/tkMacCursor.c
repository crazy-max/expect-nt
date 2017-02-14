/* 
 * tkMacCursor.c --
 *
 *	This file contains Macintosh specific cursor related routines.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacCursor.c 1.17 96/12/17 15:23:07
 */

#include "tkPort.h"
#include "tkInt.h"
#include "tkMacInt.h"

#include <Resources.h>
#include <ToolUtils.h>

/*
 * There are three different ways to set the cursor on the Mac.
 */
#define ARROW	0	/* The arrow cursor. */
#define COLOR	1	/* Cursors of type crsr. */
#define NORMAL	2	/* Cursors of type CURS. */

/*
 * The following data structure contains the system specific data
 * necessary to control Windows cursors.
 */

typedef struct {
    TkCursor info;		/* Generic cursor info used by tkCursor.c */
    Handle macCursor;		/* Resource containing Macintosh cursor. */
    int type;			/* Type of Mac cursor: arrow, crsr, CURS */
} TkMacCursor;

/*
 * The table below is used to map from the name of a predefined cursor
 * to its resource identifier.
 */

static struct CursorName {
    char *name;
    int id;
} cursorNames[] = {
    {"ibeam",		1},
    {"text",		1},
    {"xterm",		1},
    {"cross",		2},
    {"crosshair",	2},
    {"cross-hair",	2},
    {"plus",		3},
    {"watch",		4},
    {"arrow",		5},
    {NULL,		0}
};

/*
 * Declarations of static variables used in this file.
 */

static TkMacCursor *	gCurrentCursor = NULL;	 /* A pointer to the current
						  * cursor. */
static int		gResizeOverride = false; /* A boolean indicating wether
						  * we should use the resize
						  * cursor during installations. */

/*
 *----------------------------------------------------------------------
 *
 * TkGetCursorByName --
 *
 *	Retrieve a system cursor by name.  
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.  
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkGetCursorByName(
    Tcl_Interp *interp,		/* Interpreter to use for error reporting. */
    Tk_Window tkwin,		/* Window in which cursor will be used. */
    Tk_Uid string)		/* Description of cursor.  See manual entry
				 * for details on legal syntax. */
{
    struct CursorName *namePtr;
    TkMacCursor *macCursorPtr;

    /*
     * Check for the cursor in the system cursor set.
     */

    for (namePtr = cursorNames; namePtr->name != NULL; namePtr++) {
	if (strcmp(namePtr->name, string) == 0) {
	    break;
	}
    }

    macCursorPtr = (TkMacCursor *) ckalloc(sizeof(TkMacCursor));
    macCursorPtr->info.cursor = (Tk_Cursor) macCursorPtr;
    if (namePtr->name != NULL) {
    	if (namePtr->id == 5) {
	    macCursorPtr->macCursor = (Handle) -1;
	    macCursorPtr->type = ARROW;
    	} else {
	    macCursorPtr->macCursor = (Handle) GetCursor(namePtr->id);
	    macCursorPtr->type = NORMAL;
	}
    } else {
    	Handle resource;
	Str255 curName;
    	
	strcpy((char *) curName + 1, string);
	curName[0] = strlen(string);
	resource = GetNamedResource('crsr', curName);
	if (resource != NULL) {
	    short id;
	    Str255 theName;
	    ResType	theType;

	    HLock(resource);
	    GetResInfo(resource, &id, &theType, theName);
	    HUnlock(resource);
	    macCursorPtr->macCursor = (Handle) GetCCursor(id);
	    macCursorPtr->type = COLOR;
	}

	if (resource == NULL) {
	    macCursorPtr->macCursor = GetNamedResource('CURS', curName);
	    macCursorPtr->type = NORMAL;
	}
    }
    if (macCursorPtr->macCursor == NULL) {
	ckfree((char *)macCursorPtr);
	Tcl_AppendResult(interp, "bad cursor spec \"", string, "\"",
		(char *) NULL);
	return NULL;
    } else {
	return (TkCursor *) macCursorPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkCreateCursorFromData --
 *
 *	Creates a cursor from the source and mask bits.
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkCreateCursorFromData(
    Tk_Window tkwin,		/* Window in which cursor will be used. */
    char *source,		/* Bitmap data for cursor shape. */
    char *mask,			/* Bitmap data for cursor mask. */
    int width, int height,	/* Dimensions of cursor. */
    int xHot, int yHot,		/* Location of hot-spot in cursor. */
    XColor fgColor,		/* Foreground color for cursor. */
    XColor bgColor)		/* Background color for cursor. */
{
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFreeCursor --
 *
 *	This procedure is called to release a cursor allocated by
 *	TkGetCursorByName.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor data structure is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TkFreeCursor(
    TkCursor *cursorPtr)
{
    TkMacCursor *macCursorPtr = (TkMacCursor *) cursorPtr;

    switch (macCursorPtr->type) {
	case COLOR:
	    DisposeCCursor((CCrsrHandle) macCursorPtr->macCursor);
	    break;
	case NORMAL:
	    ReleaseResource(macCursorPtr->macCursor);
	    break;
    }

    if (macCursorPtr == gCurrentCursor) {
	gCurrentCursor = NULL;
    }
    
    ckfree((char *) macCursorPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacInstallCursor --
 *
 *	Installs either the current cursor as defined by TkpSetCursor
 *	or a resize cursor as the cursor the Macintosh should currently
 *	display.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the Macintosh mouse cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkMacInstallCursor(
    int resizeOverride)
{
    TkMacCursor *macCursorPtr = gCurrentCursor;
    CCrsrHandle ccursor;
    CursHandle cursor;

    gResizeOverride = resizeOverride;
    
    if (resizeOverride) {
	cursor = (CursHandle) GetNamedResource('CURS', "\presize");
	SetCursor(*cursor);
    } else if (macCursorPtr == NULL || macCursorPtr->type == ARROW) {
	SetCursor(&tcl_macQdPtr->arrow);
    } else {
	switch (macCursorPtr->type) {
	    case COLOR:
		ccursor = (CCrsrHandle) macCursorPtr->macCursor;
		SetCCursor(ccursor);
		break;
	    case NORMAL:
		cursor = (CursHandle) macCursorPtr->macCursor;
		SetCursor(*cursor);
		break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkpSetCursor --
 *
 *	Set the current cursor and install it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the current cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkpSetCursor(
    TkpCursor cursor)
{
    if (cursor == None) {
	gCurrentCursor = NULL;
    } else {
	gCurrentCursor = (TkMacCursor *) cursor;
    }

    if (tkMacAppInFront) {
	TkMacInstallCursor(gResizeOverride);
    }
}
