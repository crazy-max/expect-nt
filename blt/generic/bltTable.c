/*
 * bltTable.c --
 *
 *	This module implements a table geometry manager for the
 *	Tk toolkit.
 *
 * Copyright 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * To do:
 *
 * 3) No way to detect if window is already a master of another geometry
 *    manager.  This one is especially bad with toplevel windows, causing
 *    the window manager to lock-up trying to handle the myriads of resize
 *    requests.
 *
 *    Note: This problem continues in Tk 4.0.  It's possible for a window
 *	    to be a master for two different geometry managers.  Each manager
 *	    will set its own requested geometry for the master window. The
 *	    winner sets the geometry last (sometimes ad infinitum).
 *
 * 7) Relative sizing of partitions?
 *
 * 8) Change row/column allocation procedures?
 *
 */

#include "bltInt.h"
#ifndef	NO_TABLE

#include <ctype.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "bltTable.h"

#define TABLE_VERSION "8.0"

static void SlaveGeometryProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));
static void SlaveCustodyProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));

static Tk_GeomMgr tableMgrInfo =
{
    "table",			/* Name of geometry manager used by winfo */
    SlaveGeometryProc,		/* Procedure to for new geometry requests */
    SlaveCustodyProc,		/* Procedure when window is taken away */
};

/*
 * Default values for slave window attributes.
 */
#define DEF_FILL	FILL_NONE
#define DEF_COLUMN_SPAN	1
#define DEF_ROW_SPAN	1
#define DEF_ANCHOR	TK_ANCHOR_CENTER

static int initialized = 0;
static Tcl_HashTable masterDirectory, slaveDirectory;

static int LimitsParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *LimitsPrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));

static Tk_CustomOption limitsOption =
{
    LimitsParseProc, LimitsPrintProc, (ClientData)0
};

static int ResizeParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *ResizePrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));

static Tk_CustomOption resizeOption =
{
    ResizeParseProc, ResizePrintProc, (ClientData)0
};

extern Tk_CustomOption bltPadOption;
extern Tk_CustomOption bltFillOption;
extern Tk_CustomOption bltLengthOption;


#define DEF_TBL_RESIZE	"both"
#define DEF_TBL_PAD	"0"
#define DEF_TBL_MAXSPAN	"0"

static Tk_ConfigSpec rowConfigSpecs[] =
{
    {TK_CONFIG_CUSTOM, "-height", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Partition, reqSize), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_INT, "-maxspan", (char *)NULL, (char *)NULL,
	DEF_TBL_MAXSPAN, Tk_Offset(Partition, maxSpan),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-pady", (char *)NULL, (char *)NULL,
	DEF_TBL_PAD, Tk_Offset(Partition, pad),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-resize", (char *)NULL, (char *)NULL,
	DEF_TBL_RESIZE, Tk_Offset(Partition, resize),
	TK_CONFIG_DONT_SET_DEFAULT, &resizeOption},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Tk_ConfigSpec columnConfigSpecs[] =
{
    {TK_CONFIG_INT, "-maxspan", (char *)NULL, (char *)NULL,
	DEF_TBL_MAXSPAN, Tk_Offset(Partition, maxSpan),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-padx", (char *)NULL, (char *)NULL,
	DEF_TBL_PAD, Tk_Offset(Partition, pad),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-resize", (char *)NULL, (char *)NULL,
	DEF_TBL_RESIZE, Tk_Offset(Partition, resize),
	TK_CONFIG_DONT_SET_DEFAULT, &resizeOption},
    {TK_CONFIG_CUSTOM, "-width", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Partition, reqSize), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

#define DEF_TBL_FILL	"none"
#define DEF_TBL_ANCHOR	"center"
#define DEF_TBL_SPAN	"1"

static Tk_ConfigSpec cubicleConfigSpecs[] =
{
    {TK_CONFIG_ANCHOR, "-anchor", (char *)NULL, (char *)NULL,
	DEF_TBL_ANCHOR, Tk_Offset(Cubicle, anchor),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_INT, "-columnspan", "columnSpan", (char *)NULL,
	DEF_TBL_SPAN, Tk_Offset(Cubicle, colSpan),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_SYNONYM, "-cspan", "columnSpan", (char *)NULL,
	(char *)NULL, Tk_Offset(Cubicle, colSpan), 0},
    {TK_CONFIG_CUSTOM, "-fill", (char *)NULL, (char *)NULL,
	DEF_TBL_FILL, Tk_Offset(Cubicle, fill),
	TK_CONFIG_DONT_SET_DEFAULT, &bltFillOption},
    {TK_CONFIG_CUSTOM, "-padx", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Cubicle, padX), 0, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Cubicle, padY), 0, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-ipadx", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Cubicle, ipadX), 0, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-ipady", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Cubicle, ipadY), 0, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-reqheight", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Cubicle, reqHeight), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_CUSTOM, "-reqwidth", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Cubicle, reqWidth), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_INT, "-rowspan", "rowSpan", (char *)NULL,
	DEF_TBL_SPAN, Tk_Offset(Cubicle, rowSpan),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_SYNONYM, "-rspan", "rowSpan", (char *)NULL,
	(char *)NULL, Tk_Offset(Cubicle, rowSpan), 0},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

#define DEF_TBL_COLUMNS 	"0"
#define DEF_TBL_ROWS		"0"
#define DEF_TBL_PROPAGATE 	"1"

static Tk_ConfigSpec tableConfigSpecs[] =
{
    {TK_CONFIG_INT, "-columns", (char *)NULL, (char *)NULL,
	DEF_TBL_COLUMNS, Tk_Offset(Table, columns.length),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-padx", (char *)NULL, (char *)NULL,
	DEF_TBL_PAD, Tk_Offset(Table, padX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", (char *)NULL, (char *)NULL,
	DEF_TBL_PAD, Tk_Offset(Table, padY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_BOOLEAN, "-propagate", (char *)NULL, (char *)NULL,
	DEF_TBL_PROPAGATE, Tk_Offset(Table, propagate),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-reqheight", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Table, reqHeight), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_CUSTOM, "-reqwidth", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Table, reqWidth), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_INT, "-rows", (char *)NULL, (char *)NULL,
	DEF_TBL_ROWS, Tk_Offset(Table, rows.length),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/*
 * Forward declarations
 */
static void ArrangeTable _ANSI_ARGS_((ClientData clientData));
static void DestroyTable _ANSI_ARGS_((FreeProcData freeProcData));
static void DestroyCubicle _ANSI_ARGS_((Cubicle *cubiPtr));
static void TableEventProc _ANSI_ARGS_((ClientData clientData,
	XEvent *eventPtr));
static void InitPartitions _ANSI_ARGS_((Partition *partPtr, int from, int to));
static void BinCubicle _ANSI_ARGS_((Table *tablePtr, Cubicle *cubiPtr));
static int AssertRowCols _ANSI_ARGS_((Table *tablePtr, RowCol *rowColPtr,
	int partIndex));

/*
 * ----------------------------------------------------------------------------
 *
 * LimitsParseProc --
 *
 *	Converts the list of elements into zero or more pixel values which
 *	determine the range of pixel values possible.  An element can be in
 *	any form accepted by Tk_GetPixels. The list has a different meaning
 *	based upon the number of elements.
 *
 *	    # of elements:
 *
 *	    0 - the limits are reset to the defaults.
 *	    1 - the minimum and maximum values are set to this
 *		value, freezing the range at a single value.
 *	    2 - first element is the minimum, the second is the
 *		maximum.
 *	    3 - first element is the minimum, the second is the
 *		maximum, and the third is the nominal value.
 *
 *	Any element may be the empty string which indicates the default.
 *
 * Results:
 *	The return value is a standard Tcl result.  The min and max fields
 *	of the range are set.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
LimitsParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window of table */
    char *value;		/* New width list */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of limits */
{
    Limits *limitsPtr = (Limits *)(widgRec + offset);
    char **elemArr;
    int numElem;
    int limArr[3];
    Tk_Window winArr[3];
    int flags;

    elemArr = NULL;
    numElem = 0;

    /* Initialize limits to default values */
    limArr[2] = LIMITS_DEF_NOM;
    limArr[1] = LIMITS_DEF_MAX;
    limArr[0] = LIMITS_DEF_MIN;
    winArr[0] = winArr[1] = winArr[2] = NULL;
    flags = 0;

    if (value != NULL) {
	int size;
	int i;

	if (Tcl_SplitList(interp, value, &numElem, &elemArr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (numElem > 3) {
	    Tcl_AppendResult(interp, "wrong # limits \"", value, "\"",
		(char *)NULL);
	    goto error;
	}
	for (i = 0; i < numElem; i++) {
	    if (elemArr[i][0] == '\0') {
		continue;	/* Empty string: use default value */
	    }
	    flags |= (LIMITS_SET_BIT << i);
	    if ((elemArr[i][0] == '.') &&
		((elemArr[i][1] == '\0') || isalpha(elemArr[i][1]))) {
		Tk_Window tkwin2;

		/* Window specified: save pointer to window */
		tkwin2 = Tk_NameToWindow(interp, elemArr[i], tkwin);
		if (tkwin2 == NULL) {
		    goto error;
		}
		winArr[i] = tkwin2;
	    } else {
		if (Tk_GetPixels(interp, tkwin, elemArr[i], &size) != TCL_OK) {
		    goto error;
		}
		if ((size < LIMITS_DEF_MIN) || (size > LIMITS_DEF_MAX)) {
		    Tcl_AppendResult(interp, "invalid limits \"", value, "\"",
			(char *)NULL);
		    goto error;
		}
		limArr[i] = size;
	    }
	}
	ckfree((char*)elemArr);
    }
    /*
    * Check the limits specified.  We can't check the requested
    * size of windows.
    */
    switch (numElem) {
    case 1:
	flags |= (LIMITS_SET_MIN | LIMITS_SET_MAX);
	if (winArr[0] == NULL) {
	    limArr[1] = limArr[0];	/* Set minimum and maximum to value */
	} else {
	    winArr[1] = winArr[0];
	}
	break;

    case 2:
	if ((winArr[0] == NULL) && (winArr[0] == NULL) &&
	    (limArr[1] < limArr[0])) {
	    Tcl_AppendResult(interp, "invalid range \"", value, "\"",
		(char *)NULL);
	    return TCL_ERROR;	/* Minimum is greater than maximum */
	}
	break;

    case 3:
	if ((winArr[0] == NULL) && (winArr[0] == NULL)) {
	    if (limArr[1] < limArr[0]) {
		Tcl_AppendResult(interp, "invalid range \"", value, "\"",
		    (char *)NULL);
		return TCL_ERROR;	/* Minimum is greater than maximum */
	    }
	    if ((winArr[2] == NULL) &&
		((limArr[2] < limArr[0]) || (limArr[2] > limArr[1]))) {
		Tcl_AppendResult(interp, "invalid nominal \"", value, "\"",
		    (char *)NULL);
		return TCL_ERROR;	/* Nominal is outside of range defined
					 * by minimum and maximum */
	    }
	}
	break;
    }
    limitsPtr->min = limArr[0];
    limitsPtr->max = limArr[1];
    limitsPtr->nom = limArr[2];
    limitsPtr->minWin = winArr[0];
    limitsPtr->maxWin = winArr[1];
    limitsPtr->nomWin = winArr[2];
    limitsPtr->flags = flags;
    return TCL_OK;
  error:
    ckfree((char*)elemArr);
    return TCL_ERROR;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ResetLimits --
 *
 *	Resets the limits to their default values.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */
INLINE static void
ResetLimits(limitsPtr)
    Limits *limitsPtr;		/* Limits to be imposed on the value */
{
    limitsPtr->flags = 0;
    limitsPtr->min = LIMITS_DEF_MIN;
    limitsPtr->max = LIMITS_DEF_MAX;
    limitsPtr->nom = LIMITS_DEF_NOM;
    limitsPtr->nomWin = limitsPtr->maxWin = limitsPtr->minWin = NULL;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConstrainWidth --
 *
 *	Bounds a given width value to the limits described in the limit
 *	structure.  The initial starting value may be overridden by the
 *	nominal value in the limits.
 *
 * Results:
 *	Returns the constrained value.
 *
 * ----------------------------------------------------------------------------
 */
static int
ConstrainWidth(width, limitsPtr)
    int width;			/* Initial value to be constrained */
    Limits *limitsPtr;		/* Limits to be imposed on the value */
{
    /*
     * Check windows for requested width values;
     */
    if (limitsPtr->minWin != NULL) {
	limitsPtr->min = Tk_ReqWidth(limitsPtr->minWin);
    }
    if (limitsPtr->maxWin != NULL) {
	limitsPtr->max = Tk_ReqWidth(limitsPtr->maxWin);
    }
    if (limitsPtr->nomWin != NULL) {
	limitsPtr->nom = Tk_ReqWidth(limitsPtr->nomWin);
    }
    if (limitsPtr->flags & LIMITS_SET_NOM) {
	width = limitsPtr->nom;	/* Override initial value */
    }
    if (width < limitsPtr->min) {
	width = limitsPtr->min;	/* Bounded by minimum value */
    } else if (width > limitsPtr->max) {
	width = limitsPtr->max;	/* Bounded by maximum value */
    }
    return (width);
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConstrainHeight --
 *
 *	Bounds a given value to the limits described in the limit
 *	structure.  The initial starting value may be overridden by the
 *	nominal value in the limits.
 *
 * Results:
 *	Returns the constrained value.
 *
 * ----------------------------------------------------------------------------
 */
static int
ConstrainHeight(height, limitsPtr)
    int height;			/* Initial value to be constrained */
    Limits *limitsPtr;		/* Limits to be imposed on the value */
{
    /*
     * Check windows for requested height values;
     */
    if (limitsPtr->minWin != NULL) {
	limitsPtr->min = Tk_ReqHeight(limitsPtr->minWin);
    }
    if (limitsPtr->maxWin != NULL) {
	limitsPtr->max = Tk_ReqHeight(limitsPtr->maxWin);
    }
    if (limitsPtr->nomWin != NULL) {
	limitsPtr->nom = Tk_ReqHeight(limitsPtr->nomWin);
    }
    if (limitsPtr->flags & LIMITS_SET_NOM) {
	height = limitsPtr->nom;/* Override initial value */
    }
    if (height < limitsPtr->min) {
	height = limitsPtr->min;/* Bounded by minimum value */
    } else if (height > limitsPtr->max) {
	height = limitsPtr->max;/* Bounded by maximum value */
    }
    return (height);
}

/*
 * ----------------------------------------------------------------------------
 *
 * NameOfLimits --
 *
 *	Convert the values into a list representing the limits.
 *
 * Results:
 *	The static string representation of the limits is returned.
 *
 * ----------------------------------------------------------------------------
 */
static char *
NameOfLimits(limitsPtr)
    Limits *limitsPtr;
{
    Tcl_DString buffer;
#define STRING_SPACE 200
    static char string[STRING_SPACE + 1];

    Tcl_DStringInit(&buffer);

    if (limitsPtr->minWin != NULL) {
	Tcl_DStringAppendElement(&buffer, Tk_PathName(limitsPtr->minWin));
    } else if (limitsPtr->flags & LIMITS_SET_MIN) {
	sprintf(string, "%d", limitsPtr->min);
	Tcl_DStringAppendElement(&buffer, string);
    } else {
	Tcl_DStringAppendElement(&buffer, "");
    }

    if (limitsPtr->maxWin != NULL) {
	Tcl_DStringAppendElement(&buffer, Tk_PathName(limitsPtr->maxWin));
    } else if (limitsPtr->flags & LIMITS_SET_MAX) {
	sprintf(string, "%d", limitsPtr->max);
	Tcl_DStringAppendElement(&buffer, string);
    } else {
	Tcl_DStringAppendElement(&buffer, "");
    }

    if (limitsPtr->nomWin != NULL) {
	Tcl_DStringAppendElement(&buffer, Tk_PathName(limitsPtr->nomWin));
    } else if (limitsPtr->flags & LIMITS_SET_NOM) {
	sprintf(string, "%d", limitsPtr->nom);
	Tcl_DStringAppendElement(&buffer, string);
    } else {
	Tcl_DStringAppendElement(&buffer, "");
    }
    strncpy(string, Tcl_DStringValue(&buffer), STRING_SPACE);
    string[STRING_SPACE] = '\0';
    return (string);
}

/*
 * ----------------------------------------------------------------------------
 *
 * LimitsPrintProc --
 *
 *	Convert the limits of the pixel values allowed into a list.
 *
 * Results:
 *	The string representation of the limits is returned.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
LimitsPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Row/column structure record */
    int offset;			/* Offset of window Partition record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation routine */
{
    Limits *limitsPtr = (Limits *)(widgRec + offset);

    return (NameOfLimits(limitsPtr));
}

/*
 * ----------------------------------------------------------------------------
 *
 * ResizeParseProc --
 *
 *	Converts the resize mode into its numeric representation.  Valid
 *	mode strings are "none", "expand", "shrink", or "both".
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ResizeParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* Resize style string */
    char *widgRec;		/* Cubicle structure record */
    int offset;			/* Offset of style in record */
{
    Resize *resizePtr = (Resize *)(widgRec + offset);
    unsigned int length;
    char c;

    c = value[0];
    length = strlen(value);
    if ((c == 'n') && (strncmp(value, "none", length) == 0)) {
	*resizePtr = RESIZE_NONE;
    } else if ((c == 'b') && (strncmp(value, "both", length) == 0)) {
	*resizePtr = RESIZE_BOTH;
    } else if ((c == 'e') && (strncmp(value, "expand", length) == 0)) {
	*resizePtr = RESIZE_EXPAND;
    } else if ((c == 's') && (strncmp(value, "shrink", length) == 0)) {
	*resizePtr = RESIZE_SHRINK;
    } else {
	Tcl_AppendResult(interp, "bad resize argument \"", value,
	    "\": should be \"none\", \"expand\", \"shrink\", or \"both\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    return (TCL_OK);
}

/*
 * ----------------------------------------------------------------------------
 *
 * NameOfResize --
 *
 *	Converts the resize value into its string representation.
 *
 * Results:
 *	Returns a pointer to the static name string.
 *
 * ----------------------------------------------------------------------------
 */
static char *
NameOfResize(resize)
    Resize resize;
{
    switch (resize) {
    case RESIZE_NONE:
	return "none";
    case RESIZE_EXPAND:
	return "expand";
    case RESIZE_SHRINK:
	return "shrink";
    case RESIZE_BOTH:
	return "both";
    default:
	return "unknown resize value";
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * ResizePrintProc --
 *
 *	Returns resize mode string based upon the resize flags.
 *
 * Results:
 *	The resize mode string is returned.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
ResizePrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Row/column structure record */
    int offset;			/* Offset of resize in Partition record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    Resize resize = *(Resize *)(widgRec + offset);

    return (NameOfResize(resize));
}

/*
 * ----------------------------------------------------------------------------
 *
 * TableEventProc --
 *
 *	This procedure is invoked by the Tk event handler when the master
 *	window is reconfigured or destroyed.
 *
 *	The table will be rearranged at the next idle point if the master
 *	window has been resized or moved. There's a distinction made
 *	between parent and non-parent master window arrangements.  If the
 *	master window is moved and it's the parent of its slaves, the
 *	slaves are moved automatically.  If it's not the parent, the slaves
 *	need to be moved.  This can be a performance hit in rare cases
 *	where we're scrolling the master window (by moving it) and there
 *	are lots of slave windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the table associated with tkwin to have its layout
 *	re-computed and drawn at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
static void
TableEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window */
    XEvent *eventPtr;		/* Information about event */
{
    register Table *tablePtr = (Table *)clientData;

    if (eventPtr->type == ConfigureNotify) {
	if (!(tablePtr->flags & ARRANGE_PENDING) &&
	    ((tablePtr->masterWidth != Tk_Width(tablePtr->tkwin)) ||
		(tablePtr->masterHeight != Tk_Height(tablePtr->tkwin))
		|| (tablePtr->flags & NON_PARENT))) {
	    tablePtr->flags |= ARRANGE_PENDING;
	    Tcl_DoWhenIdle(ArrangeTable, (ClientData)tablePtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (tablePtr->flags & ARRANGE_PENDING) {
	    Tcl_CancelIdleCall(ArrangeTable, (ClientData)tablePtr);
	}
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&masterDirectory,
		(char *)tablePtr->tkwin));
	tablePtr->tkwin = NULL;
	Tcl_EventuallyFree((ClientData)tablePtr, (Tcl_FreeProc *)DestroyTable);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * SlaveEventProc --
 *
 *	This procedure is invoked by the Tk event handler when
 *	StructureNotify events occur in a slave window.  When a slave
 *	window is destroyed, it frees the corresponding cubicle structure
 *	and arranges for the table layout to be re-computed at the next
 *	idle point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the slave window was deleted, the Cubicle structure gets cleaned
 *	up and the table is rearranged.
 *
 * ----------------------------------------------------------------------------
 */
static void
SlaveEventProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to Slave structure for window
				 * referred to by eventPtr. */
    XEvent *eventPtr;		/* Describes what just happened. */
{
    Cubicle *cubiPtr = (Cubicle *)clientData;

    if (eventPtr->type == ConfigureNotify) {
	cubiPtr->tablePtr->flags |= REQUEST_LAYOUT;
	if (!(cubiPtr->tablePtr->flags & ARRANGE_PENDING) &&
	    (cubiPtr->extBW != Tk_Changes(cubiPtr->tkwin)->border_width)) {
	    cubiPtr->extBW = Tk_Changes(cubiPtr->tkwin)->border_width;
	    cubiPtr->tablePtr->flags |= ARRANGE_PENDING;
	    Tcl_DoWhenIdle(ArrangeTable, (ClientData)cubiPtr->tablePtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	cubiPtr->tablePtr->flags |= REQUEST_LAYOUT;
	if (!(cubiPtr->tablePtr->flags & ARRANGE_PENDING)) {
	    cubiPtr->tablePtr->flags |= ARRANGE_PENDING;
	    Tcl_DoWhenIdle(ArrangeTable, (ClientData)cubiPtr->tablePtr);
	}
	DestroyCubicle(cubiPtr);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * SlaveCustodyProc --
 *
 * 	This procedure is invoked when a slave window has been stolen by
 * 	another geometry manager.  The information and memory associated
 * 	with the slave window is released.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the table formerly associated with the slave window to
 *	have its layout re-computed and arranged at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
SlaveCustodyProc(clientData, tkwin)
    ClientData clientData;	/* Information about the slave window */
    Tk_Window tkwin;		/* Not used */
{
    Cubicle *cubiPtr = (Cubicle *)clientData;

    if (Tk_IsMapped(cubiPtr->tkwin)) {
	Tk_UnmapWindow(cubiPtr->tkwin);
    }
    Tk_UnmaintainGeometry(cubiPtr->tkwin, cubiPtr->tablePtr->tkwin);
    cubiPtr->tablePtr->flags |= REQUEST_LAYOUT;
    if (!(cubiPtr->tablePtr->flags & ARRANGE_PENDING)) {
	cubiPtr->tablePtr->flags |= ARRANGE_PENDING;
	Tcl_DoWhenIdle(ArrangeTable, (ClientData)cubiPtr->tablePtr);
    }
    DestroyCubicle(cubiPtr);
}


/*
 * ----------------------------------------------------------------------------
 *
 * SlaveGeometryProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for slave windows
 *	managed by the table geometry manager.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the table associated with the slave window to have its
 *	layout re-computed and arranged at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
SlaveGeometryProc(clientData, tkwin)
    ClientData clientData;	/* Information about window that got new
				 * preferred geometry.  */
    Tk_Window tkwin;		/* Other Tk-related information about the
			         * window. */
{
    Cubicle *cubiPtr = (Cubicle *)clientData;

    cubiPtr->tablePtr->flags |= REQUEST_LAYOUT;
    if (!(cubiPtr->tablePtr->flags & ARRANGE_PENDING)) {
	cubiPtr->tablePtr->flags |= ARRANGE_PENDING;
	Tcl_DoWhenIdle(ArrangeTable, (ClientData)cubiPtr->tablePtr);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * FixSpans --
 *
 *	Table has been shrunk.  Remove cubicles starting outside of 
 *	the table and reduce the spans of cubicles.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */
static void
FixSpans(tablePtr)
    Table *tablePtr;
{
    Blt_ListItem *iPtr;
    Cubicle *cubiPtr;
    int span, rebin;

    for(iPtr = Blt_FirstListItem(&(tablePtr->cubicles)); iPtr != NULL;
	/* empty */) {
	cubiPtr = (Cubicle *)Blt_GetItemValue(iPtr);
	iPtr = Blt_NextItem(iPtr);
	if ((cubiPtr->rowIndex >= tablePtr->numRows) || 
	    (cubiPtr->colIndex >= tablePtr->numCols)) {
	    /* Forget cubicles starting outside of the new size of the table */
	    DestroyCubicle(cubiPtr);
	    continue;
	}
	rebin = FALSE;
	span = tablePtr->numRows - cubiPtr->rowIndex;
	if (span < cubiPtr->rowSpan) {
	    rebin = TRUE;
	    cubiPtr->rowSpan = span;
	}
	span = tablePtr->numCols - cubiPtr->colIndex;
	if (span < cubiPtr->colSpan) {
	    rebin = TRUE;
	    cubiPtr->colSpan = span;
	}
	if (rebin) {
	    BinCubicle(tablePtr, cubiPtr);
	}
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * FindCubicle --
 *
 *	Searches for the Cubicle structure corresponding to the given
 *	window.
 *
 * Results:
 *	If a structure associated with the window exists, a pointer to that
 *	structure is returned. Otherwise NULL is returned and if the
 *	TCL_LEAVE_ERR_MSG flag is set, an error message is left in
 *	interp->result.
 *
 * ----------------------------------------------------------------------------
 */
static Cubicle *
FindCubicle(interp, tkwin, flags)
    Tcl_Interp *interp;
    Tk_Window tkwin;		/* Slave window associated with table entry */
    int flags;
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&slaveDirectory, (char *)tkwin);
    if (hPtr == NULL) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    Tcl_AppendResult(interp, "\"", Tk_PathName(tkwin),
		"\" is not managed by any table", (char *)NULL);
	}
	return NULL;
    }
    return ((Cubicle *)Tcl_GetHashValue(hPtr));
}

/*
 * ----------------------------------------------------------------------------
 *
 * CreateCubicle --
 *
 *	This procedure creates and initializes a new Cubicle structure to
 *	contain a slave window.  A valid slave window must have a parent
 *	window that is either a) the master window or b) a mutual ancestor
 *	of the master window.
 *
 * Results:
 *	Returns a pointer to the new structure describing the new table
 *	slave window entry.  If an error occurred, then the return value is
 *	NULL and an error message is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated and initialized for the Cubicle structure.
 *
 * ----------------------------------------------------------------------------
 */
static Cubicle *
CreateCubicle(tablePtr, tkwin)
    Table *tablePtr;
    Tk_Window tkwin;
{
    register Cubicle *cubiPtr;
    int dummy;
    Tk_Window parent, ancestor;
    Tcl_HashEntry *hPtr;
    Blt_ListItem *iPtr;
    int notParent = FALSE;	/* Indicates that the master window is not the
				 * parent of the new slave window. */

    /*
     * A valid slave window has a parent window that either
     *
     *    1) is the master window, or
     * 	  2) is a mutual ancestor of the master window.
     */
    ancestor = Tk_Parent(tkwin);
    for (parent = tablePtr->tkwin; (parent != ancestor) &&
	(!Tk_IsTopLevel(parent)); parent = Tk_Parent(parent)) {
	notParent = TRUE;
    }
    if (ancestor != parent) {
	Tcl_AppendResult(tablePtr->interp, "can't manage \"",
	    Tk_PathName(tkwin), "\" in table \"",
	    Tk_PathName(tablePtr->tkwin), "\"", (char *)NULL);
	return NULL;
    }
    cubiPtr = (Cubicle *)ckcalloc(1, sizeof(Cubicle));
    if (cubiPtr == NULL) {
	Panic("can't allocate table cubicle");
    }

    /* Initialize the cubicle structure */

    cubiPtr->tkwin = tkwin;
    cubiPtr->tablePtr = tablePtr;
    cubiPtr->extBW = Tk_Changes(tkwin)->border_width;
    cubiPtr->fill = DEF_FILL;
    cubiPtr->anchor = DEF_ANCHOR;
    cubiPtr->rowSpan = DEF_ROW_SPAN;
    cubiPtr->colSpan = DEF_COLUMN_SPAN;
    ResetLimits(&(cubiPtr->reqWidth));
    ResetLimits(&(cubiPtr->reqHeight));

    /*
     * Add the cubicle to the following data structures.
     *
     * 	1) A list of slave windows contained in each table.
     *  2) A hash table of all slave windows.
     */
    iPtr = Blt_NewItem((char *)cubiPtr->tkwin);
    Blt_SetItemValue(iPtr, (char *)cubiPtr);
    Blt_LinkAfter(&(tablePtr->cubicles), iPtr, (Blt_ListItem *)0);
    cubiPtr->slavePtr = iPtr;

    hPtr = Tcl_CreateHashEntry(&slaveDirectory, (char *)tkwin, &dummy);
    Tcl_SetHashValue(hPtr, (char *)cubiPtr);

    Tk_CreateEventHandler(tkwin, StructureNotifyMask,
	(Tk_EventProc*)SlaveEventProc, (ClientData)cubiPtr);
    Tk_ManageGeometry(tkwin, &tableMgrInfo, (ClientData)cubiPtr);

    return (cubiPtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * DestroyCubicle --
 *
 *	Removes the Cubicle structure from the hash table and frees the
 *	memory allocated by it.  If the table is still in use (i.e. was not
 *	called from DestoryTable), remove its entries from the lists of row
 *	and column sorted partitions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the cubicle is freed up.
 *
 * ----------------------------------------------------------------------------
 */
static void
DestroyCubicle(cubiPtr)
    Cubicle *cubiPtr;
{
    Tcl_HashEntry *hPtr;

    if (cubiPtr->rowPtr != NULL) {
	Blt_DeleteItem(cubiPtr->rowPtr);
    }
    if (cubiPtr->colPtr != NULL) {
	Blt_DeleteItem(cubiPtr->colPtr);
    }
    if (cubiPtr->slavePtr != NULL) {
	Blt_DeleteItem(cubiPtr->slavePtr);
    }
    Tk_DeleteEventHandler(cubiPtr->tkwin, StructureNotifyMask,
	(Tk_EventProc*)SlaveEventProc, (ClientData)cubiPtr);
    Tk_ManageGeometry(cubiPtr->tkwin, (Tk_GeomMgr *) NULL, (ClientData)cubiPtr);
    if (Tk_Parent(cubiPtr->tkwin) != cubiPtr->tablePtr->tkwin) {
	Tk_UnmaintainGeometry(cubiPtr->tkwin, cubiPtr->tablePtr->tkwin);
    }
    if (Tk_IsMapped(cubiPtr->tkwin)) {
	Tk_UnmapWindow(cubiPtr->tkwin);
    }
    hPtr = Tcl_FindHashEntry(&slaveDirectory, (char *)cubiPtr->tkwin);
    assert(hPtr != NULL);
    Tcl_DeleteHashEntry(hPtr);
    ckfree((char *)cubiPtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConfigureCubicle --
 *
 *	This procedure is called to process an argv/argc list, plus the Tk
 *	option database, in order to configure (or reconfigure) one or more
 *	cubicles associated with a slave window which is managed by the
 *	table geometry manager.
 *
 * 	Note: Currently only the one slave window can be queried while
 * 	      many can be reconfigured at a time.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	The table layout is recomputed and rearranged at the next idle
 *	point.
 *
 * ----------------------------------------------------------------------------
 */
static int
ConfigureCubicle(tablePtr, interp, pathName, argc, argv)
    Table *tablePtr;
    Tcl_Interp *interp;
    char *pathName;		/* Path name of slave window */
    int argc;			/* Option-value arguments */
    char **argv;
{
    Cubicle *cubiPtr;
    Tk_Window tkwin;
    int oldRowSpan, oldColSpan;

    tkwin = Tk_NameToWindow(interp, pathName, tablePtr->tkwin);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    cubiPtr = FindCubicle(interp, tkwin, TCL_LEAVE_ERR_MSG);
    if (cubiPtr == NULL) {
	return TCL_ERROR;
    }
    if (cubiPtr->tablePtr != tablePtr) {
	Tcl_AppendResult(interp, "slave window  \"", pathName,
	    "\" does not belong to table \"", Tk_PathName(tablePtr->tkwin),
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (argc == 0) {
	return (Tk_ConfigureInfo(interp, tkwin, cubicleConfigSpecs,
		(char *)cubiPtr, (char *)NULL, 0));
    } else if (argc == 1) {
	return (Tk_ConfigureInfo(interp, tkwin, cubicleConfigSpecs,
		(char *)cubiPtr, argv[0], 0));
    }
    oldRowSpan = cubiPtr->rowSpan;
    oldColSpan = cubiPtr->colSpan;
    if (Tk_ConfigureWidget(interp, tkwin, cubicleConfigSpecs, argc, argv,
	    (char *)cubiPtr, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((cubiPtr->colSpan < 1) || (cubiPtr->colSpan > USHRT_MAX)) {
	Tcl_AppendResult(interp, "bad column span specified for \"", pathName,
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    if ((cubiPtr->rowSpan < 1) || (cubiPtr->rowSpan > USHRT_MAX)) {
	Tcl_AppendResult(interp, "bad row span specified for \"", pathName,
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    if ((oldColSpan != cubiPtr->colSpan) || (oldRowSpan != cubiPtr->rowSpan)) {
	BinCubicle(tablePtr, cubiPtr);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * InfoCubicle --
 *
 *	Returns the name, position and options of a slave in the table.
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the slave window
 *	attributes is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InfoCubicle(tablePtr, interp, pathName)
    Table *tablePtr;
    Tcl_Interp *interp;
    char *pathName;		/* Pathname of the queried slave window */
{
    char string[200];
    Cubicle *cubiPtr;
    Tk_Window tkwin;

    tkwin = Tk_NameToWindow(interp, pathName, tablePtr->tkwin);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    cubiPtr = FindCubicle(interp, tkwin, TCL_LEAVE_ERR_MSG);
    if (cubiPtr == NULL) {
	return TCL_ERROR;
    }
    if (cubiPtr->tablePtr != tablePtr) {
	Tcl_AppendResult(interp, "slave window  \"", Tk_PathName(tkwin),
	    "\" does not belong to table \"", Tk_PathName(tablePtr->tkwin),
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    sprintf(string, "%d,%d", cubiPtr->rowIndex, cubiPtr->colIndex);
    Tcl_AppendResult(interp, " ", Tk_PathName(tablePtr->tkwin),
	" ", Tk_PathName(cubiPtr->tkwin), " ", string, (char *)NULL);
    sprintf(string, " -ipadx %d -ipady %d -rowspan %d -columnspan %d",
	cubiPtr->ipadX, cubiPtr->ipadY, cubiPtr->rowSpan, cubiPtr->colSpan);
    Tcl_AppendResult(interp, string, (char *)NULL);
    sprintf(string, "%d %d", cubiPtr->padLeft, cubiPtr->padRight);
    Tcl_AppendResult(interp, " -padx { ", string, " }", (char *)NULL);
    sprintf(string, "%d %d", cubiPtr->padTop, cubiPtr->padBottom);
    Tcl_AppendResult(interp, " -pady { ", string, " }", (char *)NULL);
    Tcl_AppendResult(interp, " -anchor ", Tk_NameOfAnchor(cubiPtr->anchor),
	" -fill ", Blt_NameOfFill(cubiPtr->fill), (char *)NULL);
    Tcl_AppendResult(interp, " -reqwidth { ",
	NameOfLimits(&(cubiPtr->reqWidth)), " }", (char *)NULL);
    Tcl_AppendResult(interp, " -reqheight { ",
	NameOfLimits(&(cubiPtr->reqHeight)), " }", (char *)NULL);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * Blt_FindTable --
 *
 *	Searches for a table associated with the window given by its
 *	pathname.  This window represents the master window of the table.
 *
 *	Errors may occur because
 *	  1) pathName does not represent a valid Tk window or
 *	  2) the window is not associated with any table as its master.
 *
 * Results:
 *	If a table entry exists, a pointer to the Table structure is
 *	returned. Otherwise NULL is returned and if the TCL_LEAVE_ERR_MSG
 *	flag is set, an error message is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
Table *
Blt_FindTable(interp, pathName, searchWin, flags)
    Tcl_Interp *interp;		/* Interpreter to report errors back to */
    char *pathName;		/* Path name of the master window */
    Tk_Window searchWin;	/* Main window: used to search for window
				 * associated with pathname */
    int flags;			/* If non-zero, don't reset interp->result */
{
    Tcl_HashEntry *hPtr;
    Tk_Window tkwin;

    tkwin = Tk_NameToWindow(interp, pathName, searchWin);
    if (tkwin == NULL) {
	if (!(flags & TCL_LEAVE_ERR_MSG)) {
	    Tcl_ResetResult(interp);
	}
	return NULL;
    }
    hPtr = Tcl_FindHashEntry(&masterDirectory, (char *)tkwin);
    if (hPtr == NULL) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    Tcl_AppendResult(interp, "no table associated with window \"",
		pathName, "\"", (char *)NULL);
	}
	return NULL;
    }
    return ((Table *)Tcl_GetHashValue(hPtr));
}

/*
 * ----------------------------------------------------------------------------
 *
 * CreateTable --
 *
 *	This procedure creates and initializes a new Table structure with
 *	tkwin as its master window. The internal structures associated with
 *	the table are initialized.
 *
 * Results:
 *	Returns the pointer to the new Table structure describing the new
 *	table geometry manager.  If an error occurred, the return value
 *	will be NULL and an error message is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated and initialized, an event handler is set up to
 *	watch tkwin, etc.
 *
 * ----------------------------------------------------------------------------
 */
static Table *
CreateTable(interp, pathName, searchWin)
    Tcl_Interp *interp;		/* Interpreter associated with table */
    char *pathName;		/* Path name of the master window to be
				 * associated with the new table */
    Tk_Window searchWin;	/* Main window */
{
    register Table *tablePtr;
    Tk_Window tkwin;
    int dummy;
    Tcl_HashEntry *hPtr;

    tkwin = Tk_NameToWindow(interp, pathName, searchWin);
    if (tkwin == NULL) {
	return NULL;
    }
    tablePtr = (Table *)ckcalloc(1, sizeof(Table));
    if (tablePtr == NULL) {
	Panic("can't allocate table structure");
    }
    tablePtr->tkwin = tkwin;
    tablePtr->searchWin = searchWin;
    tablePtr->interp = interp;
    tablePtr->rows.size = tablePtr->columns.size = DEF_ARRAY_SIZE;
    tablePtr->rows.type = "row";
    tablePtr->columns.type = "column";
    tablePtr->rows.configSpecs = rowConfigSpecs;
    tablePtr->columns.configSpecs = columnConfigSpecs;
    tablePtr->rows.array = tablePtr->rows.defSpace;
    tablePtr->columns.array = tablePtr->columns.defSpace;
    tablePtr->propagate = TRUE;

    tablePtr->arrangeProc = ArrangeTable;
    tablePtr->findCubicleProc = (CubicleSearchProc*)FindCubicle;

    ResetLimits(&(tablePtr->reqWidth));
    ResetLimits(&(tablePtr->reqHeight));

    InitPartitions(tablePtr->rows.array, 0, DEF_ARRAY_SIZE);
    InitPartitions(tablePtr->columns.array, 0, DEF_ARRAY_SIZE);

    Blt_InitList(&(tablePtr->cubicles), TCL_ONE_WORD_KEYS);
    Blt_InitList(&(tablePtr->rowSpans), TCL_ONE_WORD_KEYS);
    Blt_InitList(&(tablePtr->colSpans), TCL_ONE_WORD_KEYS);

    Tk_CreateEventHandler(tablePtr->tkwin, StructureNotifyMask,
	TableEventProc, (ClientData)tablePtr);
    hPtr = Tcl_CreateHashEntry(&masterDirectory, (char *)tkwin, &dummy);
    Tcl_SetHashValue(hPtr, (ClientData)tablePtr);
    return (tablePtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConfigureTable --
 *
 *	This procedure is called to process an argv/argc list in order to
 *	configure the table geometry manager.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Table configuration options (padx, pady, rows, columns, etc) get
 *	set.   The table is recalculated and arranged at the next idle
 *	point.
 *
 * ----------------------------------------------------------------------------
 */
static int
ConfigureTable(tablePtr, interp, argc, argv)
    Table *tablePtr;		/* Table to be configured */
    Tcl_Interp *interp;		/* Interpreter to report results back to */
    int argc;
    char **argv;		/* Option-value pairs */
{
    int oldRows, oldColumns;

    if (argc == 0) {
	return (Tk_ConfigureInfo(interp, tablePtr->tkwin, tableConfigSpecs,
		(char *)tablePtr, (char *)NULL, 0));
    } else if (argc == 1) {
	return (Tk_ConfigureInfo(interp, tablePtr->tkwin, tableConfigSpecs,
		(char *)tablePtr, argv[0], 0));
    }
    oldRows = tablePtr->numRows;
    oldColumns = tablePtr->numCols;
    if (Tk_ConfigureWidget(interp, tablePtr->tkwin, tableConfigSpecs,
	    argc, argv, (char *)tablePtr, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Extend the size of the table if necessary */
    AssertRowCols(tablePtr, &(tablePtr->rows), tablePtr->numRows - 1);
    AssertRowCols(tablePtr, &(tablePtr->columns), tablePtr->numCols - 1);

    /* Fix cubicle spans if the table is shrunk  */
    if ((tablePtr->numRows < oldRows) || (tablePtr->numCols < oldColumns)) {
	FixSpans(tablePtr);
    }
    /* Redisplay the table with the new configuration */
    tablePtr->flags |= REQUEST_LAYOUT;
    if (!(tablePtr->flags & ARRANGE_PENDING)) {
	tablePtr->flags |= ARRANGE_PENDING;
	Tcl_DoWhenIdle(ArrangeTable, (ClientData)tablePtr);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * DestroyTable --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release to
 *	clean up the Table structure at a safe time (when no-one is using
 *	it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the table geometry manager is freed up.
 *
 * ----------------------------------------------------------------------------
 */
static void
DestroyTable(freeProcData)
    FreeProcData freeProcData;	/* Table structure */
{
    Blt_ListItem *iPtr;
    Blt_List *bucket;
    Cubicle *cubiPtr;
    Table *tablePtr = (Table *)freeProcData;

    for (iPtr = Blt_FirstListItem(&(tablePtr->cubicles)); iPtr != NULL;
	iPtr = Blt_NextItem(iPtr)) {
	cubiPtr = (Cubicle *)Blt_GetItemValue(iPtr);
	cubiPtr->slavePtr = NULL;	/* Don't disrupt this list of entries */
	DestroyCubicle(cubiPtr);
    }
    Blt_ResetList(&(tablePtr->cubicles));
    /*
     *  Clear each of the lists managing the cubicles.  The entries in the
     *  lists of row and column spans are themselves lists which need to be
     *  cleared.
     */
    for (iPtr = Blt_FirstListItem(&(tablePtr->rowSpans)); iPtr != NULL; 
	iPtr = Blt_NextItem(iPtr)) {
	bucket = (Blt_List *)Blt_GetItemValue(iPtr);
	Blt_DeleteList(bucket);
    }
    Blt_ResetList(&(tablePtr->rowSpans));
    for (iPtr = Blt_FirstListItem(&(tablePtr->colSpans)); iPtr != NULL; 
	iPtr = Blt_NextItem(iPtr)) {
	bucket = (Blt_List *)Blt_GetItemValue(iPtr);
	Blt_DeleteList(bucket);
    }
    Blt_ResetList(&(tablePtr->colSpans));

    if ((tablePtr->rows.array != NULL) &&
	(tablePtr->rows.array != tablePtr->rows.defSpace)) {
	ckfree((char *)tablePtr->rows.array);
    }
    if ((tablePtr->columns.array != NULL) &&
	(tablePtr->columns.array != tablePtr->columns.defSpace)) {
	ckfree((char *)tablePtr->columns.array);
    }
    ckfree((char *)tablePtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * InitPartitions --
 *
 *	Initializes the values of the newly created elements in the
 *	partition array.  This is performed on new partitions as the array
 *	is extended.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The array of partitions is initialized.
 *
 * ----------------------------------------------------------------------------
 */
static void
InitPartitions(partArr, from, to)
    Partition partArr[];	/* Array of partitions to be initialized */
    int from, to;		/* Number of elements in array */
{
    register int i;
    register Partition *partPtr;

    partPtr = partArr + from;
    for (i = from; i < to; i++) {
	partPtr->index = i;
	partPtr->resize = RESIZE_BOTH;
	ResetLimits(&(partPtr->reqSize));
	partPtr->size = 0;
	partPtr->nomSize = LIMITS_DEF_NOM;
	partPtr->pad.side1 = partPtr->pad.side2 = 0;
	partPtr->minSpan = partPtr->maxSpan = 0;
	partPtr++;
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * ExtendRowCol --
 *
 *	Resizes the partition array to a larger size.
 *
 * Results:
 *	A pointer to the newly extended array is returned.
 *
 * ----------------------------------------------------------------------------
 */
static void
ExtendRowCol(rowColPtr, newSize)
    RowCol *rowColPtr;
    int newSize;
{
    Partition *newArr;
    int oldSize;

    oldSize = rowColPtr->size;
    newArr = (Partition *)ckalloc(newSize * sizeof(Partition));
    if (newArr == NULL) {
	Panic("can't allocate array of partitions");
    }
    if (oldSize > 0) {
	memcpy((char *)newArr, (char *)rowColPtr->array,
	    oldSize * sizeof(Partition));
    }
    InitPartitions(newArr, oldSize, newSize);

    if (rowColPtr->array != rowColPtr->defSpace) {
	ckfree((char *)rowColPtr->array);
    }
    rowColPtr->array = newArr;
    rowColPtr->size = newSize;
}

/*
 * ----------------------------------------------------------------------------
 *
 * AssertRowCols --
 *
 *	Checks the size of the column partitions and extends the size if a
 *	larger array is needed.
 *
 * Results:
 *	Returns 1 if the column exists.  Otherwise 0 is returned and
 *	interp->result contains an error message.
 *
 * Side effects:
 *	The size of the column partition array may be extended and
 *	initialized.
 *
 * ----------------------------------------------------------------------------
 */
static int
AssertRowCols(tablePtr, rowColPtr, partIndex)
    Table *tablePtr;
    RowCol *rowColPtr;
    int partIndex;
{
    if (partIndex >= rowColPtr->size) {
	int newSize;

	if (partIndex >= USHRT_MAX) {
	    Tcl_AppendResult(tablePtr->interp, "too many ", rowColPtr->type,
		"s in \"", Tk_PathName(tablePtr->tkwin), "\"", (char *)NULL);
	    return 0;
	}
	newSize = 2 * rowColPtr->size;
	while (newSize <= partIndex) {
	    newSize += newSize;
	}
	ExtendRowCol(rowColPtr, newSize);
    }
    if (partIndex >= rowColPtr->length) {
	rowColPtr->length = partIndex + 1;
    }
    return 1;
}

/*
 * ----------------------------------------------------------------------------
 *
 * InfoPartition --
 *
 *	Returns the options of a partition in the table.
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the slave window
 *	attributes is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InfoPartition(tablePtr, interp, indexStr)
    Table *tablePtr;
    Tcl_Interp *interp;
    char *indexStr;
{
    Partition *partPtr;
    char buf[BUFSIZ];
    char *format;
    RowCol *rowColPtr;
    char c;

    c = indexStr[0];
    if ((c == 'r') || (c == 'R')) {
	format = "r%d -resize %s -height {%s} -pady {%d %d} -maxspan %d";
	rowColPtr = &(tablePtr->rows);
    } else {
	format = "c%d -resize %s -width {%s} -padx {%d %d} -maxspan %d";
	rowColPtr = &(tablePtr->columns);
    }
    if ((indexStr[1] == '*') && (indexStr[2] == '\0')) {
	register int i;

	partPtr = rowColPtr->array;
	for (i = 0; i < rowColPtr->length; i++) {
	    sprintf(buf, format, partPtr->index, NameOfResize(partPtr->resize),
		NameOfLimits(&(partPtr->reqSize)), partPtr->pad.side1,
		partPtr->pad.side2, partPtr->maxSpan);
	    Tcl_AppendResult(interp, buf, (char *)NULL);
	    if ((i + 1) < rowColPtr->length) {
		Tcl_AppendResult(interp, "\n", (char *)NULL);
	    }
	    partPtr++;
	}
    } else {
	long int partNum;

	if (Tcl_ExprLong(interp, indexStr + 1, &partNum) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((partNum < 0) || (partNum >= rowColPtr->length)) {
	    Tcl_AppendResult(interp, rowColPtr->type, " index \"", indexStr,
		"\" is out of range", (char *)NULL);
	    return TCL_ERROR;
	}
	partPtr = rowColPtr->array + (int)partNum;
	sprintf(buf, format, partPtr->index, NameOfResize(partPtr->resize),
	    NameOfLimits(&(partPtr->reqSize)), partPtr->pad.side1,
	    partPtr->pad.side2, partPtr->maxSpan);
	Tcl_AppendResult(interp, buf, (char *)NULL);
    }
    return TCL_OK;
}


/*
 * ----------------------------------------------------------------------------
 *
 * BinCubicle --
 *
 *	Adds the cubicle to the lists of both row and column spans.  The
 *	layout of the table is done in order of partition spans, from
 *	shorted to longest.  The windows spanning a particular number of
 *	partitions are stored in a linked list.  Each list is in turn,
 *	contained within a master list.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cubicle is added to both the lists of row and columns spans.
 *	This will effect the layout of the windows.
 *
 * ----------------------------------------------------------------------------
 */
static void
BinCubicle(tablePtr, cubiPtr)
    Table *tablePtr;
    Cubicle *cubiPtr;
{
    Blt_ListItem *iPtr;
    Blt_List *listPtr, *bucketPtr;
    int key;

    /*
     * Remove the cubicle from both row and column lists.  It will be
     * re-inserted into the table at the new position.
     */
    if (cubiPtr->colPtr != NULL) {
	Blt_UnlinkItem(cubiPtr->colPtr);
    }
    if (cubiPtr->rowPtr != NULL) {
	Blt_UnlinkItem(cubiPtr->rowPtr);
    }
    listPtr = &(tablePtr->rowSpans);
    key = 0;			/* Initialize key to bogus span */
    for (iPtr = Blt_FirstListItem(listPtr); iPtr != NULL; 
	iPtr = Blt_NextItem(iPtr)) {
	key = (int)Blt_GetItemKey(iPtr);
	if (cubiPtr->rowSpan <= key) {
	    break;
	}
    }
    if (key != cubiPtr->rowSpan) {
	Blt_List *bucket;
	Blt_ListItem *newIPtr;

	/*
	 * Create a new list (bucket) to contain entries of that size span
	 * and and link it into the list of buckets.
	 */
	newIPtr = Blt_NewItem((char *)cubiPtr->rowSpan);
	bucket = Blt_NewList(TCL_ONE_WORD_KEYS);
	Blt_SetItemValue(newIPtr, (char *)bucket);
	if (iPtr == NULL) {
	    Blt_LinkAfter(listPtr, newIPtr, (Blt_ListItem *)NULL);
	} else {
	    Blt_LinkBefore(listPtr, newIPtr, iPtr);
	}
	iPtr = newIPtr;
    }
    bucketPtr = (Blt_List *)Blt_GetItemValue(iPtr);
    iPtr = Blt_NewItem((char *)cubiPtr->tkwin);
    Blt_SetItemValue(iPtr, (char *)cubiPtr);
    Blt_LinkAfter(bucketPtr, iPtr, (Blt_ListItem *)NULL);
    cubiPtr->rowPtr = iPtr;

    listPtr = &(tablePtr->colSpans);
    key = 0;			/* Initialize key to bogus span */
    for (iPtr = Blt_FirstListItem(listPtr); iPtr != NULL;
	iPtr = Blt_NextItem(iPtr)) {
	key = (int)Blt_GetItemKey(iPtr);
	if (cubiPtr->colSpan <= key) {
	    break;
	}
    }
    if (key != cubiPtr->colSpan) {
	Blt_List *bucket;
	Blt_ListItem *newIPtr;

	/*
	 * Create a new list (bucket) to contain entries of that size span
	 * and and link it into the list of buckets.
	 */
	newIPtr = Blt_NewItem((char *)cubiPtr->colSpan);
	bucket = Blt_NewList(TCL_ONE_WORD_KEYS);
	Blt_SetItemValue(newIPtr, (char *)bucket);
	if (iPtr == NULL) {
	    /* Empty list: add item to the head */
	    Blt_LinkAfter(listPtr, newIPtr, (Blt_ListItem *)NULL);
	} else {
	    /* Insert before the last item examined */
	    Blt_LinkBefore(listPtr, newIPtr, iPtr);
	}
	iPtr = newIPtr;
    }
    bucketPtr = (Blt_List *)Blt_GetItemValue(iPtr);
    iPtr = Blt_NewItem((char *)cubiPtr->tkwin);
    Blt_SetItemValue(iPtr, (char *)cubiPtr);

    /* Add the new cubicle to the span bucket */
    Blt_LinkAfter(bucketPtr, iPtr, (Blt_ListItem *)NULL);

    cubiPtr->colPtr = iPtr;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ParseRowCol --
 *
 *	Parse the entry index string and return the row and column numbers
 *	in their respective parameters.  The format of a table entry index
 *	is <row>,<column> where <row> is the row number and <column> is the
 *	column number.  Rows and columns are numbered starting from zero.
 *
 * Results:
 *	Returns a standard Tcl result.  If TCL_OK is returned, the row and
 *	column numbers are returned via rowPtr and columnPtr respectively.
 *
 * ----------------------------------------------------------------------------
 */
static int
ParseRowCol(interp, indexStr, rowPtr, columnPtr)
    Tcl_Interp *interp;
    char *indexStr;
    int *rowPtr;
    int *columnPtr;
{
    char *columnStr, *rowStr, *copyIndexStr;
    long row, column;

    /*
     *  mike: We need to copy indexStr in order to safely change it below
     *  (the comma is replaced by a NULL char, so that row/column can be
     *  easily parsed). This is because Tcl 8.x reuses all strings of a
     *  Tcl script, and it's generally a good idea to work on copies in
     *  case of multithreaded programs.
     */
    copyIndexStr = ckstrdup(indexStr);
    rowStr = copyIndexStr;
    columnStr = strchr(copyIndexStr, ',');
    if (columnStr == NULL) {
	Tcl_AppendResult(interp, "invalid index \"", indexStr,
	    "\": should be \"row,column\"", (char *)NULL);
	goto error;
    }
    *columnStr++ = '\0';
    if ((Tcl_ExprLong(interp, rowStr, &row) != TCL_OK) ||
	(Tcl_ExprLong(interp, columnStr, &column) != TCL_OK)) {
	/* no need to set interp->result, as Tcl_ExprLong has
	   already set an appropriate error message. */
	goto error;
    }
    if ((row < 0) || (row > USHRT_MAX)) {
	Tcl_AppendResult(interp, "row index \"", rowStr,
	    "\" is out of range", (char *)NULL);
	goto error;
    }
    if ((column < 0) || (column > USHRT_MAX)) {
	Tcl_AppendResult(interp, "column index \"", columnStr,
	    "\" is out of range", (char *)NULL);
	goto error;
    }
    *rowPtr = (int)row;
    *columnPtr = (int)column;
    ckfree(copyIndexStr);
    return TCL_OK;

error:
    ckfree(copyIndexStr);
    return TCL_ERROR;
}

/*
 * ----------------------------------------------------------------------------
 *
 * CheckIndex --
 *
 *	Checks that the given row and column indices already exist in
 *	the current table.
 *
 * Results:
 *	Returns 1 if the indices are valid and 0 otherwise.
 *
 * ----------------------------------------------------------------------------
 */
INLINE static int
CheckIndex(tablePtr, row, column)
    Table *tablePtr;
    int row, column;
{
    return ((row >= 0) && (row < tablePtr->numRows) && (column >= 0) &&
	(column < tablePtr->numCols));
}

/*
 * ----------------------------------------------------------------------------
 *
 * ManageSlave --
 *
 *	Adds the given window as a slave window into the table at a given
 *	row and column position.  The window may already exist as a slave
 *	window in the table. If tkwin is a slave of another table, it's an
 *	error.
 *
 *	The new window is inserted into both the slave window hash table
 *	(this is used to locate the information associated with the slave
 *	window without searching each table) and cubicle lists which are
 *	sorted in order of column and row spans.
 *
 * Results:
 *	Returns a standard Tcl result.  If an error occurred, TCL_ERROR is
 *	returned and an error message is left in interp->result.
 *
 * Side Effects:
 *	The table is re-computed and arranged at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
static int
ManageSlave(tablePtr, tkwin, row, column, argc, argv)
    Table *tablePtr;
    Tk_Window tkwin;
    int row, column;
    int argc;
    char **argv;
{
    register Cubicle *cubiPtr;
    int result = TCL_OK;

    cubiPtr = FindCubicle(tablePtr->interp, tkwin, 0);
    if (cubiPtr != NULL) {
	/*
	 * The cubicle for the slave window already exists. Make sure it's
	 * currently being managed by this table and not another table.
	 */
	if (cubiPtr->tablePtr != tablePtr) {
	    Tcl_AppendResult(tablePtr->interp, "\"", Tk_PathName(tkwin),
		"\" is already managed by \"", Tk_PathName(cubiPtr->tkwin),
		"\"", (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	cubiPtr = CreateCubicle(tablePtr, tkwin);
	if (cubiPtr == NULL) {
	    return TCL_ERROR;
	}
    }

    cubiPtr->colIndex = column;
    cubiPtr->rowIndex = row;
    if (argc > 0) {
	result = Tk_ConfigureWidget(tablePtr->interp, cubiPtr->tkwin,
	    cubicleConfigSpecs, argc, argv, (char *)cubiPtr,
	    TK_CONFIG_ARGV_ONLY);
    }
    if ((cubiPtr->colSpan < 1) || (cubiPtr->rowSpan < 1)) {
	Tcl_AppendResult(tablePtr->interp, "invalid span specified for \"",
	    Tk_PathName(tkwin), "\"", (char *)NULL);
	DestroyCubicle(cubiPtr);
	return TCL_ERROR;
    }
    if (!AssertRowCols(tablePtr, &(tablePtr->columns),
	    cubiPtr->colIndex + (cubiPtr->colSpan - 1)) ||
	!AssertRowCols(tablePtr, &(tablePtr->rows),
	    cubiPtr->rowIndex + (cubiPtr->rowSpan - 1))) {
	return TCL_ERROR;
    }
    /*
     * Insert the cubicle into both the row and column layout lists
     */
    BinCubicle(tablePtr, cubiPtr);

    return (result);
}

/*
 * ----------------------------------------------------------------------------
 *
 * BuildTable --
 *
 *	Processes an argv/argc list of table entries to add and configure
 *	new slave windows into the table.  A table entry consists of the
 *	window path name, table index, and optional configuration options.
 *	The first argument in the argv list is the name of the table.  If
 *	no table exists for the given window, a new one is created.
 *
 * Results:
 *	Returns a standard Tcl result.  If an error occurred, TCL_ERROR is
 *	returned and an error message is left in interp->result.
 *
 * Side Effects:
 *	Memory is allocated, a new master table is possibly created, etc.
 *	The table is re-computed and arranged at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
static int
BuildTable(tablePtr, interp, argc, argv)
    Table *tablePtr;		/* Table to manage new slave windows */
    Tcl_Interp *interp;		/* Interpreter to report errors back to */
    int argc;			/*  */
    char **argv;		/* List of slave windows, indices, and
				 * options */
{
    Tk_Window tkwin;
    int row, column;
    int nextRow, nextColumn;
    register int i;

    /* Process any options specific to the table */
    for (i = 2; i < argc; i += 2) {
	if (argv[i][0] != '-') {
	    break;
	}
    }
    if (i > argc) {
	i = argc;
    }
    if (i > 2) {
	if (ConfigureTable(tablePtr, interp, i - 2, argv + 2) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    nextRow = tablePtr->numRows;
    nextColumn = 0;
    argc -= i, argv += i;
    while (argc > 0) {
	tkwin = Tk_NameToWindow(interp, argv[0], tablePtr->tkwin);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	if ((argc == 1) || (argv[1][0] == '-')) {
	    /* No row,column index, use defaults instead */
	    row = nextRow, column = nextColumn;
	    argc--, argv++;
	} else {
	    if (ParseRowCol(interp, argv[1], &row, &column) != TCL_OK) {
		return TCL_ERROR;	/* Invalid row,column index */
	    }
	    /* Skip over the window pathname and table index. */
	    argc -= 2, argv += 2;
	}

	/* Find the end of the slave window's option-value pairs */
	for (i = 0; i < argc; i += 2) {
	    if (argv[i][0] != '-') {
		break;
	    }
	}
	if (i > argc) {
	    i = argc;
	}
	if (ManageSlave(tablePtr, tkwin, row, column, i, argv) != TCL_OK) {
	    return TCL_ERROR;
	}
	nextColumn = column + 1;
	argc -= i, argv += i;
    }
    /* Arrange for the new table layout to be calculated. */
    tablePtr->flags |= REQUEST_LAYOUT;
    if (!(tablePtr->flags & ARRANGE_PENDING)) {
	tablePtr->flags |= ARRANGE_PENDING;
	Tcl_DoWhenIdle(ArrangeTable, (ClientData)tablePtr);
    }
    Tcl_SetResult(interp, Tk_PathName(tablePtr->tkwin), TCL_STATIC);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ParseIndex --
 *
 *	Parses a string representing a table index.  The form of the
 *	index is as follows:
 *		Rn	- Row index, where n is the index of row
 *		Cn	- Column index, where n is the index of column
 *		r,c	- Cell index, where r is the row index and c
 *			  is the column index.
 *
 * Results:
 *	Returns a standard Tcl result.  If no error occurred, TCL_OK is
 *	returned.  *RowPtr* will return the row index.  *ColumnPtr* 
 *	will return the column index.  If the row or column index is
 *	not applicable, -1 is returned via *rowPtr* or *columnPtr*.
 *
 * ----------------------------------------------------------------------------
 */
static int
ParseIndex(tablePtr, string, rowPtr, columnPtr)
    Table *tablePtr;
    char *string;
    int *rowPtr, *columnPtr;
{
    char c;
    long partNum;

    c = string[0];
    *rowPtr = *columnPtr = -1;
    if ((c == 'r') || (c == 'R')) {
	if (Tcl_ExprLong(tablePtr->interp, string + 1, &partNum) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((partNum < 0) || (partNum >= tablePtr->numRows)) {
	    Tcl_AppendResult(tablePtr->interp, "row index \"", string, 
		"\" is out of range", (char *)NULL);
	    return TCL_ERROR;
	}
	*rowPtr = (int)partNum;
    } else if ((c == 'c') || (c == 'C')) {
	if (Tcl_ExprLong(tablePtr->interp, string + 1, &partNum) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((partNum < 0) || (partNum >= tablePtr->numCols)) {
	    Tcl_AppendResult(tablePtr->interp, "column index \"", string, 
		"\" is out of range", (char *)NULL);
	    return TCL_ERROR;
	}
	*columnPtr = (int)partNum;
    } else {
	if (ParseRowCol(tablePtr->interp, string, 
			rowPtr, columnPtr) != TCL_OK) {
	    return TCL_ERROR;	/* Invalid row,column index */
	}
	if (!CheckIndex(tablePtr->interp, *rowPtr, *columnPtr)) {
	    Tcl_AppendResult(tablePtr->interp, "index \"", string, 
		"\" is out of range", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ExtentsOper --
 *
 *	Returns a list of all the pathnames of the slaves window managed by
 *	a table geometry manager.  The table is given by the path name of a
 *	master window associated with the table.
 *
 * Results:
 *	Returns a standard Tcl result.  If no error occurred, TCL_OK is
 *	returned and a list of slave window path names is left in
 *	interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ExtentsOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;		/* Interpreter to return list of names to */
    int argc;			/* Number of arguments */
    char **argv;		/* Contains 1-2 arguments: pathname of master
				 * window associated with the table and search
				 * pattern */
{
    Table *tablePtr;
    Tk_Window tkwin = (Tk_Window)clientData;
    int row, column;
    Partition *c1Ptr, *r1Ptr, *c2Ptr, *r2Ptr;
    int x, y, width, height;
    char tmpString[50];

    tablePtr = Blt_FindTable(interp, argv[2], tkwin, TCL_LEAVE_ERR_MSG);
    if (tablePtr == NULL) {
	return TCL_ERROR;
    }
    if (ParseIndex(tablePtr, argv[3], &row, &column) != TCL_OK) {
	return TCL_ERROR;
    }
    if (row < 0) {
	r1Ptr = tablePtr->rowArr;
	r2Ptr = tablePtr->rowArr + (tablePtr->numRows - 1);
    } else {
	r1Ptr = r2Ptr = tablePtr->rowArr + row;
    }
    if (column < 0) {
	c1Ptr = tablePtr->colArr;
	c2Ptr = tablePtr->colArr + (tablePtr->numCols - 1);
    } else {
	c1Ptr = c2Ptr = tablePtr->colArr + column;
    }
    x = c1Ptr->offset;
    y = r1Ptr->offset;
    width = c2Ptr->offset + c2Ptr->size - x;
    height = r2Ptr->offset + r2Ptr->size - y;
    sprintf(tmpString, "%d %d %d %d", x, y, width, height);
    Tcl_SetResult(interp, tmpString, TCL_VOLATILE);
    return TCL_OK;
}


/*
 * ----------------------------------------------------------------------------
 *
 * SlavesOper --
 *
 *	Returns a list of all the pathnames of the slaves window managed by
 *	a table geometry manager.  The table is given by the path name of a
 *	master window associated with the table.
 *
 * Results:
 *	Returns a standard Tcl result.  If no error occurred, TCL_OK is
 *	returned and a list of slave window path names is left in
 *	interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SearchOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;		/* Interpreter to return list of names to */
    int argc;			/* Number of arguments */
    char **argv;		/* Contains 1-2 arguments: pathname of master
				 * window associated with the table and search
				 * pattern */
{
    Tk_Window mainWindow = (Tk_Window)clientData;
    Table *tablePtr;
    Blt_ListItem *iPtr;
    Cubicle *cubiPtr;
    int spanRow, spanCol, startRow, startCol;
    char *pattern;
    char c;
    int flags;
    register int i;
#define	MATCH_PATTERN		(1<<0)	/* Find slaves whose path names
					 * match a given pattern */
#define	MATCH_INDEX_SPAN	(1<<1)	/* Find slaves which span index  */
#define	MATCH_INDEX_START	(1<<2)	/* Find slaves which start at index */


    tablePtr = Blt_FindTable(interp, argv[2], mainWindow, TCL_LEAVE_ERR_MSG);
    if (tablePtr == NULL) {
	return TCL_ERROR;
    }
    flags = 0;
    pattern = NULL;

    /* Parse switches and arguments first */
    for (i = 3; i < argc; i += 2) {
	if (argv[i][0] == '-') {
	    unsigned int length;

	    if ((i + 1) == argc) {
		Tcl_AppendResult(interp, "switch \"", argv[i], "\" needs value",
		    (char *)NULL);
		return TCL_ERROR;
	    }
	    length = strlen(argv[i]);
	    c = argv[i][1];
	    if ((c == 'p') && (length > 1) &&
		(strncmp(argv[3], "-pattern", length) == 0)) {
		flags |= MATCH_PATTERN;
		pattern = argv[4];
	    } else if ((c == 's') && (length > 2) &&
		(strncmp(argv[i], "-start", length) == 0)) {
		flags |= MATCH_INDEX_START;
		if (ParseIndex(tablePtr, argv[i + 1],
			&startRow, &startCol) != TCL_OK) {
		    return TCL_ERROR;
		}
	    } else if ((c == 's') && (length > 2) &&
		(strncmp(argv[i], "-span", length) == 0)) {
		flags |= MATCH_INDEX_SPAN;
		if (ParseIndex(tablePtr, argv[4], &spanRow, &spanCol)!=TCL_OK) {
		    return TCL_ERROR;
		}
	    } else {
		Tcl_AppendResult(interp, "bad switch \"", argv[3], "\" : ",
		    "should be \"-pattern\", \"-span\", or \"-start\"",
		    (char *)NULL);
		return TCL_ERROR;
	    }
	} else {
	    if ((i + 1) == argc) {
		pattern = argv[i];
		flags |= MATCH_PATTERN;
	    }
	}
    }

    /* Then try to match cubicles with the search criteria */

    for (iPtr = Blt_FirstListItem(&(tablePtr->cubicles)); iPtr != NULL;
	iPtr = Blt_NextItem(iPtr)) {
	cubiPtr = (Cubicle *)Blt_GetItemValue(iPtr);
	if (flags & MATCH_PATTERN) {
	    if (pattern != NULL) {
		if (!Tcl_StringMatch(Tk_PathName(cubiPtr->tkwin), pattern)) {
		    continue;
		}
	    }
	}
	if (flags & MATCH_INDEX_SPAN) {
	    if ((spanRow >= 0) && ((cubiPtr->rowIndex > spanRow) ||
		    ((cubiPtr->rowIndex + cubiPtr->rowSpan) <= spanRow))) {
		continue;
	    }
	    if ((spanCol >= 0) && ((cubiPtr->colIndex > spanCol) ||
		    ((cubiPtr->colIndex + cubiPtr->colSpan) <= spanCol))) {
		continue;
	    }
	}
	if (flags & MATCH_INDEX_START) {
	    if ((startRow >= 0) && (cubiPtr->rowIndex != startRow)) {
		continue;
	    }
	    if ((startCol >= 0) && (cubiPtr->colIndex == startCol)) {
		continue;
	    }
	}
	Tcl_AppendElement(interp, Tk_PathName(cubiPtr->tkwin));
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * MastersOper --
 *
 *	Returns a list of all the pathnames of the master windows managed
 *	by a table geometry manager matching a given pattern.  If no
 *	pattern is present (argc == 0), all pathnames are returned.
 *
 * Results:
 *	Returns a standard Tcl result.  If no error occurred, TCL_OK is
 *	returned and a list of slave window path names is left in
 *	interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MastersOper(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter: Used to
				 * search for windows in the hierarchy */
    Tcl_Interp *interp;		/* Interpreter to return list of names to */
    int argc;
    char **argv;		/* Contains 0-1 arguments: search pattern */
{
    Tk_Window searchWin = (Tk_Window)clientData;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;
    register Table *tablePtr;
    char *pattern;

    pattern = NULL;
    if (argc > 2) {
	if (argv[2][0] == '-') {
	    unsigned int length;

	    length = strlen(argv[2]);
	    if ((length > 1) && (argv[2][1] == 'p') &&
		(strncmp(argv[2], "-pattern", length) == 0)) {
		pattern = argv[3];
		goto search;
	    } else if ((length > 1) && (argv[2][1] == 's') &&
		(strncmp(argv[2], "-slave", length) == 0)) {
		Cubicle *cubiPtr;
		Tk_Window tkwin;

		if (argc != 4) {
		    Tcl_AppendResult(interp, "needs window argument for \"",
			argv[2], "\"", (char *)NULL);
		    return TCL_ERROR;
		}
		tkwin = Tk_NameToWindow(interp, argv[3], searchWin);
		if (tkwin == NULL) {
		    return TCL_ERROR;
		}
		cubiPtr = FindCubicle(interp, tkwin, 0);
		if (cubiPtr != NULL) {
		    Tcl_SetResult(interp, Tk_PathName(cubiPtr->tablePtr->tkwin),
			TCL_VOLATILE);
		}
		return TCL_OK;
	    } else {
		Tcl_AppendResult(interp, "bad switch \"", argv[2], "\" : ",
		    "should be \"-pattern\", or \"-slave\"", (char *)NULL);
		return TCL_ERROR;
	    }
	} else {
	    pattern = argv[2];
	}
    }

  search:
    for (hPtr = Tcl_FirstHashEntry(&masterDirectory, &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	tablePtr = (Table *)Tcl_GetHashValue(hPtr);
	if (tablePtr->searchWin == searchWin) {
	    if ((pattern == NULL) ||
		(Tcl_StringMatch(Tk_PathName(tablePtr->tkwin), pattern))) {
		Tcl_AppendElement(interp, Tk_PathName(tablePtr->tkwin));
	    }
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ArrangeOper --
 *
 *	Forces layout of the table geometry manager.  This is useful
 *	to get the geometry manager to calculate the normal width and
 *	height of each row and column.  Otherwise, one needs to
 *	withdraw the master window, run "update", and then query to
 *	geometry manager.
 *
 * Results:
 *	Returns a standard Tcl result.  If no error occurred, TCL_OK
 *	is returned. Otherwise, TCL_ERROR and a error message is left in
 *	interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ArrangeOper(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter: Used to
				 * search for windows in the hierarchy */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;		/* Path name of master window associated with
				 * the table */
{
    Tk_Window searchWin = (Tk_Window)clientData;
    Table *tablePtr;

    tablePtr = Blt_FindTable(interp, argv[2], searchWin, TCL_LEAVE_ERR_MSG);
    if (tablePtr == NULL) {
	return TCL_ERROR;
    }
    tablePtr->flags |= REQUEST_LAYOUT;
    ArrangeTable((ClientData)tablePtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * InfoOper --
 *
 *	Returns the options of a slave or partition in the table.
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the slave window
 *	attributes is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InfoOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Table *tablePtr;
    Tk_Window tkwin = (Tk_Window)clientData;
    int result;
    char c;
    register int i;

    tablePtr = Blt_FindTable(interp, argv[2], tkwin, TCL_LEAVE_ERR_MSG);
    if (tablePtr == NULL) {
	return TCL_ERROR;
    }
    for (i = 3; i < argc; i++) {
	c = argv[i][0];
	if (c == '.') {		/* Cubicle information */
	    result = InfoCubicle(tablePtr, interp, argv[i]);
	} else if ((c == 'r') || (c == 'R') || (c == 'c') || (c == 'C')) {
	    result = InfoPartition(tablePtr, interp, argv[i]);
	} else {
	    Tcl_AppendResult(interp, "unknown item \"", argv[i],
		"\": should be slave, rowIndex or columnIndex", (char *)NULL);
	    return TCL_ERROR;
	}
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((i + 1) < argc) {
	    Tcl_AppendResult(interp, "\n", (char *)NULL);
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConfigureParitition --
 *
 *	Processes an argv/argc list in order to	configure a single row
 *      or column, given its index in the table geometry manager.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Row or column configuration options (bounds, resize flags, etc) get
 *	set.  New rows or columns  may be created as necessary.
 *
 * ----------------------------------------------------------------------------
 */
static int
ConfigurePartition(tablePtr, rowColPtr, partNum, argc, argv)
    Table *tablePtr;
    RowCol *rowColPtr;
    int partNum;
    int argc;
    char **argv;
{
    int result;
    Partition *partPtr;

    if (argc < 2) {
	char *name;

	if (partNum > rowColPtr->length) {
	    char tmpString[20];

	    sprintf(tmpString, "%c%d", rowColPtr->type[0], partNum);
	    Tcl_SetResult(tablePtr->interp, (char *) NULL, TCL_STATIC);
	    Tcl_AppendResult(tablePtr->interp,
		rowColPtr->type, " index \"", tmpString, "\" is out of range",
		(char *) NULL);
	    return TCL_ERROR;
	}
	partPtr = rowColPtr->array + partNum;
	name = (argc == 1) ? argv[0] : NULL;
	result = Tk_ConfigureInfo(tablePtr->interp, tablePtr->tkwin,
	    rowColPtr->configSpecs, (char *)partPtr, name, 0);
    } else {
	if (!AssertRowCols(tablePtr, rowColPtr, partNum)) {
	    return TCL_ERROR;
	}
	partPtr = rowColPtr->array + partNum;
	result = Tk_ConfigureWidget(tablePtr->interp, tablePtr->tkwin,
	    rowColPtr->configSpecs, argc, argv, (char *)partPtr,
	    TK_CONFIG_ARGV_ONLY);
	if (partPtr->maxSpan < 0) {
	    partPtr->maxSpan = 0;
	}
    }
    return (result);
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConfigureRowCol --
 *
 *	This procedure is called to process an argv/argc list in order to
 *	configure a row or column in the table geometry manager.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Partition configuration options (bounds, resize flags, etc) get
 *	set.  New partitions may be created as necessary. The table is
 *	recalculated and arranged at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
static int
ConfigureRowCol(tablePtr, rowColPtr, indexStr, argc, argv)
    Table *tablePtr;		/* Table to be configured */
    RowCol *rowColPtr;
    char *indexStr;
    int argc;
    char **argv;
{
    if ((indexStr[1] == '*') && (indexStr[2] == '\0')) {
	register int i;

	if (argc == 1) {
	    Tcl_AppendResult(tablePtr->interp, "can't query using '*'",
		(char *)NULL);
	    return TCL_ERROR;
	}
	for (i = 0; i < rowColPtr->length; i++) {
	    if (ConfigurePartition(tablePtr, rowColPtr, i,
		    argc, argv) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    } else {
	long int result;
	int partNum;

	if (Tcl_ExprLong(tablePtr->interp, indexStr + 1, &result) != TCL_OK) {
	    return TCL_ERROR;
	}
	partNum = (int)result;
	if (partNum < 0) {
	    Tcl_AppendResult(tablePtr->interp, "invalid ", rowColPtr->type,
		" index \"", indexStr, "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	if (ConfigurePartition(tablePtr, rowColPtr, partNum, argc,
		argv) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * CgetOper --
 *
 *	Returns the name, position and options of a slave in the table.
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the slave window
 *	attributes is left in interp->result.
 *
 * --------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Tk_Window searchWin = (Tk_Window)clientData;
    Table *tablePtr;
    int length;
    char c;
    int result;
    long partIndex;
    int partNum;
    RowCol *rowColPtr;

    tablePtr = Blt_FindTable(interp, argv[2], searchWin, TCL_LEAVE_ERR_MSG);
    if (tablePtr == NULL) {
	return TCL_ERROR;
    }
    if (argc == 4) {
	return (Tk_ConfigureValue(interp, tablePtr->tkwin, tableConfigSpecs,
		(char *)tablePtr, argv[3], 0));
    }
    c = argv[3][0];
    length = strlen(argv[3]);
    if (c == '.') {		/* Configure slave window */
	Cubicle *cubiPtr;
	Tk_Window tkwin;

	tkwin = Tk_NameToWindow(interp, argv[3], tablePtr->tkwin);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	cubiPtr = FindCubicle(interp, tkwin, TCL_LEAVE_ERR_MSG);
	if (cubiPtr == NULL) {
	    return TCL_ERROR;
	}
	return (Tk_ConfigureValue(interp, tkwin, cubicleConfigSpecs,
		(char *)cubiPtr, argv[4], 0));
    } else if ((c == 'm') && (strncmp(argv[3], "master", length) == 0)) {
	return (Tk_ConfigureValue(interp, tablePtr->tkwin, tableConfigSpecs,
		(char *)tablePtr, argv[4], 0));
    } else if ((c == 'r') || (c == 'R')) {
	rowColPtr = &(tablePtr->rows);
    } else if ((c == 'c') || (c == 'C')) {
	rowColPtr = &(tablePtr->columns);
    } else {
	Tcl_AppendResult(interp, "unknown item \"", argv[3],
	    "\": should be slave, row or column index, or \"master\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    /* Handle row or column configuration queries */
    if (Tcl_ExprLong(interp, argv[3] + 1, &partIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    partNum = (int)partIndex;
    if ((partNum < 0) || (partNum >= rowColPtr->length)) {
	Tcl_AppendResult(interp, "invalid ", rowColPtr->type, " index \"",
	    argv[3], "\"", (char *)NULL);
	return TCL_ERROR;
    }
    result = Tk_ConfigureValue(interp, tablePtr->tkwin, rowColPtr->configSpecs,
	(char *)(rowColPtr->array + partNum), argv[4], 0);
    return result;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConfigureOper --
 *
 *	Returns the name, position and options of a slave in the table.
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the slave window
 *	attributes is left in interp->result.
 *
 * --------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigureOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Tk_Window searchWin = (Tk_Window)clientData;
    Table *tablePtr;
    int length;
    char c;
    int count;
    int result;
    char **items;
    register int i;

    tablePtr = Blt_FindTable(interp, argv[2], searchWin, TCL_LEAVE_ERR_MSG);
    if (tablePtr == NULL) {
	return TCL_ERROR;
    }
    /*
     * Find the end of the items. Search until we see an option (-).
     */
    argc -= 3, argv += 3;
    for (count = 0; count < argc; count++) {
	if (argv[count][0] == '-') {
	    break;
	}
    }
    items = argv;		/* Save the start of the item list */
    argc -= count;		/* Move beyond the items to the options */
    argv += count;

    result = TCL_ERROR;		/* Suppress compiler warning */

    if (count == 0) {
	result = ConfigureTable(tablePtr, interp, argc, argv);
    }
    for (i = 0; i < count; i++) {
	c = items[i][0];
	length = strlen(items[i]);
	if (c == '.') {		/* Configure slave window */
	    result = ConfigureCubicle(tablePtr, interp, items[i], argc, argv);
	} else if ((c == 'r') || (c == 'R')) {
	    result = ConfigureRowCol(tablePtr, &(tablePtr->rows), items[i],
		argc, argv);
	} else if ((c == 'c') || (c == 'C')) {
	    result = ConfigureRowCol(tablePtr, &(tablePtr->columns),
		items[i], argc, argv);
	} else if ((c == 'm') && (strncmp(argv[3], "master", length) == 0)) {
	    result = ConfigureTable(tablePtr, interp, argc, argv);
	} else {
	    Tcl_AppendResult(interp, "unknown item \"", items[i],
		"\": should be slave, row or column index, or \"master\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
	if (result == TCL_ERROR) {
	    break;
	}
	if ((i + 1) < count) {
	    Tcl_AppendResult(interp, "\n", (char *)NULL);
	}
    }
    tablePtr->flags |= REQUEST_LAYOUT;
    if (!(tablePtr->flags & ARRANGE_PENDING)) {
	tablePtr->flags |= ARRANGE_PENDING;
	Tcl_DoWhenIdle(ArrangeTable, (ClientData)tablePtr);
    }
    return result;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ForgetOper --
 *
 *	Processes an argv/argc list of slave window names and purges their
 *	entries from their respective tables.  The windows are unmapped and
 *	the tables are rearranged at the next idle point.  Note that all
 *	the named slave windows do not need to exist in the same table.
 *
 * Results:
 *	Returns a standard Tcl result.  If an error occurred, TCL_ERROR is
 *	returned and an error message is left in interp->result.
 *
 * Side Effects:
 *	Memory is deallocated (the cubicle is destroyed), etc.  The
 *	affected tables are is re-computed and arranged at the next idle
 *	point.
 *
 * ----------------------------------------------------------------------------
 */
static int
ForgetOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Cubicle *cubiPtr;
    register int i;
    Tk_Window tkwin;

    for (i = 2; i < argc; i++) {
	tkwin = Tk_NameToWindow(interp, argv[i], (Tk_Window)clientData);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	cubiPtr = FindCubicle(interp, tkwin, TCL_LEAVE_ERR_MSG);
	if (cubiPtr == NULL) {
	    return TCL_ERROR;
	}
	if (Tk_IsMapped(cubiPtr->tkwin)) {
	    Tk_UnmapWindow(cubiPtr->tkwin);
	}
	/*
	 * Arrange for the call back here because not all the named slave
	 * windows may belong to the same table.
	 */
	cubiPtr->tablePtr->flags |= REQUEST_LAYOUT;
	if (!(cubiPtr->tablePtr->flags & ARRANGE_PENDING)) {
	    cubiPtr->tablePtr->flags |= ARRANGE_PENDING;
	    Tcl_DoWhenIdle(ArrangeTable, (ClientData)cubiPtr->tablePtr);
	}
	DestroyCubicle(cubiPtr);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * PartitionSearch --
 *
 * 	Performs a binary search for the line of text located at some
 * 	world y-coordinate (not screen y-coordinate). The search is
 * 	inclusive of those lines from low to high.
 *
 * Results:
 *	Returns the array index of the line found at the given
 *	y-coordinate.  If the y-coordinate is outside of the given range
 *	of lines, -1 is returned.
 *
 * ----------------------------------------------------------------------
 */
static int
PartitionSearch(partArr, dist, low, high)
    Partition partArr[];
    int dist;			/* Search y-coordinate  */
    int low, high;		/* Range of lines to search */
{
    int mid;
    int left, right;

    while (low <= high) {
	mid = (low + high) >> 1;

	left = partArr[mid].offset;
	right = partArr[mid].offset + partArr[mid].size;
	if (dist < left) {
	    high = mid - 1;
	} else if (dist > right) {
	    low = mid + 1;
	} else {
	    return (mid);
	}
    }
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * LocateOper --
 *
 *
 *	Returns the row or column index given a pixel coordinate
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
LocateOper(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int x, y;
    int rowIndex, colIndex;
    Tk_Window searchWin = (Tk_Window)clientData;
    Table *tablePtr;
    char tmpString[30];

    tablePtr = Blt_FindTable(interp, argv[2], searchWin, TCL_LEAVE_ERR_MSG);
    if (tablePtr == NULL) {
	return TCL_ERROR;
    }
    if (Blt_GetLength(interp, tablePtr->tkwin, argv[3], &x) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_GetLength(interp, tablePtr->tkwin, argv[4], &y) != TCL_OK) {
	return TCL_ERROR;
    }
    rowIndex = PartitionSearch(tablePtr->rowArr, y, 0, tablePtr->numRows - 1);
    if (rowIndex < 0) {
	return TCL_OK;
    }
    colIndex = PartitionSearch(tablePtr->colArr, x, 0, tablePtr->numCols - 1);
    if (colIndex < 0) {
	return TCL_OK;
    }
    sprintf(tmpString, "%d,%d", rowIndex, colIndex);
    Tcl_SetResult(interp, tmpString, TCL_VOLATILE);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * TranslateAnchor --
 *
 * 	Translate the coordinates of a given bounding box based upon the
 * 	anchor specified.  The anchor indicates where the given xy position
 * 	is in relation to the bounding box.
 *
 *  		nw --- n --- ne
 *  		|            |     x,y ---+
 *  		w   center   e      |     |
 *  		|            |      +-----+
 *  		sw --- s --- se
 *
 * Results:
 *	The translated coordinates of the bounding box are returned.
 *
 * ----------------------------------------------------------------------------
 */
static XPoint
TranslateAnchor(deltaX, deltaY, anchor)
    int deltaX, deltaY;		/* Difference between outer and inner regions
				 */
    Tk_Anchor anchor;		/* Direction of the anchor */
{
    XPoint newPos;

    newPos.x = newPos.y = 0;
    switch (anchor) {
    case TK_ANCHOR_NW:		/* Upper left corner */
	break;
    case TK_ANCHOR_W:		/* Left center */
	newPos.y = (deltaY / 2);
	break;
    case TK_ANCHOR_SW:		/* Lower left corner */
	newPos.y = deltaY;
	break;
    case TK_ANCHOR_N:		/* Top center */
	newPos.x = (deltaX / 2);
	break;
    case TK_ANCHOR_CENTER:	/* Centered */
	newPos.x = (deltaX / 2);
	newPos.y = (deltaY / 2);
	break;
    case TK_ANCHOR_S:		/* Bottom center */
	newPos.x = (deltaX / 2);
	newPos.y = deltaY;
	break;
    case TK_ANCHOR_NE:		/* Upper right corner */
	newPos.x = deltaX;
	break;
    case TK_ANCHOR_E:		/* Right center */
	newPos.x = deltaX;
	newPos.y = (deltaY / 2);
	break;
    case TK_ANCHOR_SE:		/* Lower right corner */
	newPos.x = deltaX;
	newPos.y = deltaY;
	break;
    }
    return (newPos);
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetReqWidth --
 *
 *	Returns the width requested by the slave window starting in the
 *	given cubicle.  The requested space also includes any internal
 *	padding which has been designated for this window.
 *
 *	The requested width of the window is always bounded by the limits
 *	set in cubiPtr->reqWidth.
 *
 * Results:
 *	Returns the requested width of the slave window.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetReqWidth(cubiPtr)
    Cubicle *cubiPtr;
{
    int width;

    width = Tk_ReqWidth(cubiPtr->tkwin) + (2 * cubiPtr->ipadX);
    width = ConstrainWidth(width, &(cubiPtr->reqWidth));
    return (width);
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetReqHeight --
 *
 *	Returns the height requested by the slave window starting in the
 *	given cubicle.  The requested space also includes any internal
 *	padding which has been designated for this window.
 *
 *	The requested height of the window is always bounded by the limits
 *	set in cubiPtr->reqHeight.
 *
 * Results:
 *	Returns the requested height of the slave window.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetReqHeight(cubiPtr)
    Cubicle *cubiPtr;
{
    int height;

    height = Tk_ReqHeight(cubiPtr->tkwin) + (2 * cubiPtr->ipadX);
    height = ConstrainHeight(height, &(cubiPtr->reqHeight));
    return (height);
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetSpan --
 *
 *	Calculates the distance of the given span of partitions.
 *
 * Results:
 *	Returns the space currently used in the span of partitions.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetSpan(tablePtr, partArr, length, withPad)
    Table *tablePtr;
    Partition *partArr;		/* Array of partitions */
    int length;			/* Number of partitions spanned */
    int withPad;		/* If non-zero, include the extra padding at
				 * the end partitions of the span in the space
				 * used */
{
    register Partition *partPtr;
    Partition *startPtr, *endPtr;
    register int spaceUsed;

    startPtr = partArr;
    endPtr = partArr + (length - 1);

    spaceUsed = 0;
    for (partPtr = startPtr; partPtr <= endPtr; partPtr++) {
	spaceUsed += partPtr->size;
    }
    /*
     * In some cases (e.g. layout of cubicles), we don't want to consider
     * the padding on either side of the span when computing the distance.
     */
    if (!withPad) {
	spaceUsed -= (startPtr->pad.side1 + endPtr->pad.side2 +
	    tablePtr->editorPartitionPad);
    }
    return (spaceUsed);
}

/*
 * ----------------------------------------------------------------------------
 *
 * GrowSpan --
 *
 *	Expand the span by the amount of the extra space needed.  This
 *	procedure is used in LayoutPartitions to grow the partitions to
 *	their minimum nominal size, starting from a zero width and height
 *	space.
 *
 *	This looks more complicated than it really is.  The idea is to make
 *	the size of the partitions correspond to the smallest cubicle
 *	spans.  For example, if window A is in column 1 and window B spans
 *	both columns 0 and 1, any extra space needed to fit window B should
 *	come from column 0.
 *
 *	On the first pass we try to add space to partitions which have not
 *	been touched yet (i.e. have no nominal size).  Since the row and
 *	column lists are sorted in ascending order of the number of rows or
 *	columns spanned, the space is distributed amongst the smallest
 *	spans first.
 *
 *	The second pass handles the case of windows which have the same
 *	span.  For example, if A and B, which span the same number of
 *	partitions are the only windows to span column 1, column 1 would
 *	grow to contain the bigger of the two slices of space.
 *
 *	If there is still extra space after the first two passes, this
 *	means that there were no partitions of with no window spans or the
 *	same order span that could be expanded. The third pass will try to
 *	remedy this by parcelling out the left over space evenly among the
 *	rest of the partitions.
 *
 *	On each pass, we have to keep iterating over the span, evenly
 *	doling out slices of extra space, because we may hit partition
 *	limits as space is donated.  In addition, if there are left over
 *	pixels because of round-off, this will distribute them as evenly as
 *	possible.  For the worst case, it will take *length* passes to
 *	expand the span.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The partitions in the span may be expanded.
 *
 * ----------------------------------------------------------------------------
 */
static void
GrowSpan(array, length, cubiPtr, extraSpace)
    Partition *array;		/* Array of (column/row) partitions  */
    int length;			/* Number of partitions in the span */
    Cubicle *cubiPtr;
    int extraSpace;		/* The amount of extra space needed to
				 * grow the span. */
{
    register Partition *partPtr;
    Partition *startPtr, *endPtr;
    int availSpace, adjustSize;
    int numAvail;		/* # of partitions with space available */

    startPtr = array;
    endPtr = array + length;

    /*
     * ------------------------------------------------------------------------
     *
     * Pass 1: Add space first to partitions which were previously empty
     *
     * ------------------------------------------------------------------------
     */

    /* Find out how many partitions have no size yet */
    numAvail = 0;
    for (partPtr = startPtr; partPtr < endPtr; partPtr++) {
	if ((partPtr->nomSize == LIMITS_DEF_NOM) &&
	    (partPtr->maxSize > partPtr->size)) {
	    numAvail++;
	}
    }
    while ((numAvail > 0) && (extraSpace > 0)) {
	adjustSize = extraSpace / numAvail;
	if (adjustSize == 0) {
	    adjustSize = 1;
	}
	for (partPtr = startPtr; (partPtr < endPtr) && (extraSpace > 0);
	    partPtr++) {
	    availSpace = partPtr->maxSize - partPtr->size;
	    if ((partPtr->nomSize == LIMITS_DEF_NOM) && (availSpace > 0)) {
		if (adjustSize < availSpace) {
		    extraSpace -= adjustSize;
		    partPtr->size += adjustSize;
		} else {
		    extraSpace -= availSpace;
		    partPtr->size += availSpace;
		    numAvail--;
		}
		partPtr->minSpan = length;
		partPtr->control = cubiPtr;
	    }
	}
    }

    /*
     * ------------------------------------------------------------------------
     *
     * Pass 2: Add space to partitions which have the same minimum span
     *
     * ------------------------------------------------------------------------
     */

    numAvail = 0;
    for (partPtr = startPtr; partPtr < endPtr; partPtr++) {
	if ((partPtr->minSpan == length) &&
	    (partPtr->maxSize > partPtr->size)) {
	    numAvail++;
	}
    }
    while ((numAvail > 0) && (extraSpace > 0)) {
	adjustSize = extraSpace / numAvail;
	if (adjustSize == 0) {
	    adjustSize = 1;
	}
	for (partPtr = startPtr; (partPtr < endPtr) && (extraSpace > 0);
	    partPtr++) {
	    availSpace = partPtr->maxSize - partPtr->size;
	    if ((partPtr->minSpan == length) && (availSpace > 0)) {
		if (adjustSize < availSpace) {
		    extraSpace -= adjustSize;
		    partPtr->size += adjustSize;
		} else {
		    extraSpace -= availSpace;
		    partPtr->size += availSpace;
		    numAvail--;
		}
		partPtr->control = cubiPtr;
	    }
	}
    }

    /*
     * ------------------------------------------------------------------------
     *
     * Pass 3: Try to expand all the partitions with space still available
     *
     * ------------------------------------------------------------------------
     */

    /* Find out how many partitions still have space available */
    numAvail = 0;
    for (partPtr = startPtr; partPtr < endPtr; partPtr++) {
	if (((partPtr->maxSpan == 0) || (length <= partPtr->maxSpan)) &&
	    (partPtr->maxSize > partPtr->size)) {
	    numAvail++;
	}
	partPtr->nomSize = partPtr->size;
    }
    while ((numAvail > 0) && (extraSpace > 0)) {
	adjustSize = extraSpace / numAvail;
	if (adjustSize == 0) {
	    adjustSize = 1;
	}
	for (partPtr = startPtr; (partPtr < endPtr) && (extraSpace > 0);
	    partPtr++) {
	    if ((partPtr->maxSpan > 0) && (length > partPtr->maxSpan)) {
		continue;	/* Ignore spans greater than designated */
	    }
	    availSpace = partPtr->maxSize - partPtr->size;
	    if (availSpace > 0) {
		if (adjustSize < availSpace) {
		    extraSpace -= adjustSize;
		    partPtr->size += adjustSize;
		} else {
		    extraSpace -= availSpace;
		    partPtr->size += availSpace;
		    numAvail--;
		}
		partPtr->nomSize = partPtr->size;
		partPtr->control = cubiPtr;
	    }
	}
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * AdjustPartitions --
 *
 *	Adjust the span by the amount of the extra space needed.  If the
 *	amount (extraSpace) is negative, shrink the span, otherwise expand
 *	it.  Size constraints on the partitions may prevent any or all of
 *	the spacing adjustments.
 *
 *	This is very much like the GrowSpan procedure, but in this case we
 *	are shrinking or expanding all the (row or column) partitions. It
 *	uses a two pass approach, first giving space to partitions which
 *	not are smaller/larger than their nominal sizes. This is because
 *	constraints on the partitions may cause resizing to be non-linear.
 *
 *	If there is still extra space, this means that all partitions are
 *	at least to their nominal sizes.  The second pass will try to
 *	add/remove the left over space evenly among all the partitions
 *	which still have space available.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The size of the partitions in the span may be increased or decreased.
 *
 * ----------------------------------------------------------------------------
 */
static void
AdjustPartitions(array, length, extraSpace)
    Partition *array;		/* Array of (column/row) partitions  */
    int length;			/* Number of partitions */
    int extraSpace;		/* The amount of extra space to grow or shrink
				 * the span. If negative, it represents the
				 * amount of space to remove */
{
    register Partition *partPtr;
    Partition *startPtr, *endPtr;
    int availSpace, adjustSize;
    int numAvail;

    startPtr = array;
    endPtr = array + length;

    /*
     * ------------------------------------------------------------------------
     *
     * Pass 1: Adjust partition's with space beyond its nominal size.
     *
     * ------------------------------------------------------------------------
     */
    numAvail = 0;
    for (partPtr = startPtr; partPtr < endPtr; partPtr++) {
	if (extraSpace < 0) {
	    availSpace = partPtr->size - partPtr->nomSize;
	} else {
	    availSpace = partPtr->nomSize - partPtr->size;
	}
	if (availSpace > 0) {
	    numAvail++;
	}
    }
    while ((numAvail > 0) && (extraSpace != 0)) {
	adjustSize = extraSpace / numAvail;
	if (adjustSize == 0) {
	    adjustSize = (extraSpace > 0) ? 1 : -1;
	}
	for (partPtr = startPtr; (partPtr < endPtr) && (extraSpace != 0);
	    partPtr++) {
	    availSpace = partPtr->nomSize - partPtr->size;
	    if (((extraSpace > 0) && (availSpace > 0)) ||
		((extraSpace < 0) && (availSpace < 0))) {
		if (BLT_ABS(adjustSize) < BLT_ABS(availSpace)) {
		    extraSpace -= adjustSize;
		    partPtr->size += adjustSize;
		} else {
		    extraSpace -= availSpace;
		    partPtr->size += availSpace;
		    numAvail--;
		}
	    }
	}
    }

    /*
     * ------------------------------------------------------------------------
     *
     * Pass 2: Adjust the partitions with space still available
     *
     * ------------------------------------------------------------------------
     */
    numAvail = 0;
    for (partPtr = startPtr; partPtr < endPtr; partPtr++) {
	if (extraSpace > 0) {
	    availSpace = partPtr->maxSize - partPtr->size;
	} else {
	    availSpace = partPtr->size - partPtr->minSize;
	}
	if (availSpace > 0) {
	    numAvail++;
	}
    }
    while ((numAvail > 0) && (extraSpace != 0)) {
	adjustSize = extraSpace / numAvail;
	if (adjustSize == 0) {
	    adjustSize = (extraSpace > 0) ? 1 : -1;
	}
	for (partPtr = startPtr; (partPtr < endPtr) && (extraSpace != 0);
	    partPtr++) {
	    if (extraSpace > 0) {
		availSpace = partPtr->maxSize - partPtr->size;
	    } else {
		availSpace = partPtr->minSize - partPtr->size;
	    }
	    if (((extraSpace > 0) && (availSpace > 0)) ||
		((extraSpace < 0) && (availSpace < 0))) {
		if (BLT_ABS(adjustSize) < BLT_ABS(availSpace)) {
		    extraSpace -= adjustSize;
		    partPtr->size += adjustSize;
		} else {
		    extraSpace -= availSpace;
		    partPtr->size += availSpace;
		    numAvail--;
		}
	    }
	}
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * ResetPartitions --
 *
 *	Sets/resets the size of each row and column partition to the
 *	minimum limit of the partition (this is usually zero). This routine
 *	gets called when new slave windows are added, deleted, or resized.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The size of each partition is re-initialized to its minimum size.
 *
 * ----------------------------------------------------------------------------
 */
static void
ResetPartitions(tablePtr, rowColPtr, limitsProc)
    Table *tablePtr;
    RowCol *rowColPtr;
    LimitsProc *limitsProc;
{
    register Partition *partPtr;
    register int i;
    int pad, size;

    partPtr = rowColPtr->array;
    for (i = 0; i < rowColPtr->length; i++) {

	/*
	 * The constraint procedure below also has the desired side effect
	 * of setting the minimum, maximum, and nominal values to the
	 * requested size of its associated window (if one exists).
	 */
	size = (*limitsProc) (0, &(partPtr->reqSize));

	pad = PADDING(partPtr->pad) + tablePtr->editorPartitionPad;
	if (partPtr->reqSize.flags & LIMITS_SET_NOM) {

	    /*
	     * This could be done more cleanly.  We want to ensure that the
	     * requested nominal size is not overridden when determining the
	     * normal sizes.  So temporarily fix min and max to the nominal
	     * size and reset them back later.
	     */
	    partPtr->minSize = partPtr->maxSize = partPtr->size =
		partPtr->nomSize = size + pad;

	} else {
	    partPtr->minSize = partPtr->reqSize.min + pad;
	    partPtr->maxSize = partPtr->reqSize.max + pad;
	    partPtr->nomSize = LIMITS_DEF_NOM;
	    partPtr->size = pad;
	}
	partPtr->minSpan = 0;
	partPtr->control = NULL;
	partPtr++;
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * SetNominalSizes
 *
 *	Sets the normal sizes for each partition in the array.  The
 *	partition size is the requested window size plus an amount of
 *	padding.  In addition, adjust the min/max bounds of the partition
 *	depending upon the resize flags (whether the partition can be
 *	expanded or shrunk from its normal size).
 *
 * Results:
 *	Returns the total space needed for the all the partitions.
 *
 * Side Effects:
 *	The nominal size of each partition in the array is set.  This is
 *	later used to determine how to shrink or grow the table if the
 *	master window cannot be resized to accommodate the exact size
 *	requirements of all the partitions.
 *
 * ----------------------------------------------------------------------------
 */
static int
SetNominalSizes(tablePtr, rowColPtr)
    Table *tablePtr;
    RowCol *rowColPtr;
{
    register Partition *partPtr;/* Row or column partition array */
    int i;
    int pad, size, total;
    int lastIndex;

    lastIndex = total = 0;

    partPtr = rowColPtr->array;
    for (i = 0; i < rowColPtr->length; i++) {
	pad = PADDING(partPtr->pad) + tablePtr->editorPartitionPad;

	/*
	 * Restore the real bounds after setting nominal size.  This is
	 * necessary if the bounds were temporarily set to the nominal value
	 * (see ResetPartitions).
	 */
	partPtr->minSize = partPtr->reqSize.min + pad;
	partPtr->maxSize = partPtr->reqSize.max + pad;

	size = partPtr->size;
	if (size > partPtr->maxSize) {
	    size = partPtr->maxSize;
	} else if (size < partPtr->minSize) {
	    size = partPtr->minSize;
	}
	if ((tablePtr->editorPartitionPad > 0) &&
	    (size < tablePtr->editPtr->minSize)) {
	    size = tablePtr->editPtr->minSize;
	}
	partPtr->nomSize = partPtr->size = size;
	total += partPtr->nomSize;

	/*
	 * If a partition can't be resized (to either expand or shrink), hold
	 * its respective limit at its normal size.
	 */
	if (!(partPtr->resize & RESIZE_EXPAND)) {
	    partPtr->maxSize = partPtr->nomSize;
	}
	if (!(partPtr->resize & RESIZE_SHRINK)) {
	    partPtr->minSize = partPtr->nomSize;
	}
	if (partPtr->size > 0) {
	    lastIndex = i;
	}
	partPtr++;
    }
    /*
     * Reset the number of entries now that we know how many partitions
     * are currently being used.  The user might have "forgot" (unpacked)
     * a slave window, or reconfigured a trailing partition to have no
     * size.
     */
    rowColPtr->length = lastIndex + 1;

    return (total);
}

/*
 * ----------------------------------------------------------------------------
 *
 * LayoutPartitions --
 *
 *	Calculates the normal space requirements for both the row and
 *	column partitions.  Each slave window is added in order of the
 *	number of rows or columns spanned, which defines the space needed
 *	among in the partitions spanned.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *
 * 	The sum of normal sizes set here will be used as the normal size
 * 	for the master window.
 *
 * ----------------------------------------------------------------------------
 */
static void
LayoutPartitions(tablePtr)
    Table *tablePtr;
{
    register Blt_ListItem *iPtr, *spanPtr;
    Blt_List *bucket;
    register Cubicle *cubiPtr;
    Partition *rowPtr, *colPtr;
    int neededSpace, usedSpace, totalSpace;
    int twiceBW;
    int cubiclePad, tablePad;

    cubiclePad = 2 * tablePtr->editorCubiclePad;
    tablePad = 2 * tablePtr->editorTablePad;
    twiceBW = (2 * Tk_InternalBorderWidth(tablePtr->tkwin));
    ResetPartitions(tablePtr, &(tablePtr->columns), ConstrainWidth);
    for (spanPtr = Blt_FirstListItem(&(tablePtr->colSpans)); spanPtr != NULL;
	spanPtr = Blt_NextItem(spanPtr)) {

	bucket = (Blt_List *)Blt_GetItemValue(spanPtr);

	for (iPtr = Blt_FirstListItem(bucket); iPtr != NULL;
	    iPtr = Blt_NextItem(iPtr)) {
	    cubiPtr = (Cubicle *)Blt_GetItemValue(iPtr);
	    neededSpace = GetReqWidth(cubiPtr) + 2 * cubiPtr->extBW +
		PADDING(cubiPtr->padX) + cubiclePad;
	    if (neededSpace > 0) {
		colPtr = tablePtr->colArr + cubiPtr->colIndex;
		usedSpace =
		    GetSpan(tablePtr, colPtr, cubiPtr->colSpan, EXCLUDE_PAD);

		if (neededSpace > usedSpace) {
		    GrowSpan(colPtr, cubiPtr->colSpan, cubiPtr->tkwin,
			neededSpace - usedSpace);
		}
	    }
	}
    }
    totalSpace = SetNominalSizes(tablePtr, &(tablePtr->columns));
    tablePtr->normalWidth = ConstrainWidth(totalSpace, &(tablePtr->reqWidth));
    tablePtr->normalWidth += (twiceBW + PADDING(tablePtr->padX) + tablePad);

    ResetPartitions(tablePtr, &(tablePtr->rows), ConstrainHeight);
    for (spanPtr = Blt_FirstListItem(&(tablePtr->rowSpans));
	spanPtr != NULL; spanPtr = Blt_NextItem(spanPtr)) {
	bucket = (Blt_List *)Blt_GetItemValue(spanPtr);
	for (iPtr = Blt_FirstListItem(bucket); iPtr != NULL;
	    iPtr = Blt_NextItem(iPtr)) {
	    cubiPtr = (Cubicle *)Blt_GetItemValue(iPtr);
	    neededSpace = GetReqHeight(cubiPtr) + 2 * cubiPtr->extBW +
		PADDING(cubiPtr->padY) + cubiclePad;
	    if (neededSpace > 0) {
		rowPtr = tablePtr->rowArr + cubiPtr->rowIndex;
		usedSpace =
		    GetSpan(tablePtr, rowPtr, cubiPtr->rowSpan, EXCLUDE_PAD);

		if (neededSpace > usedSpace) {
		    GrowSpan(rowPtr, cubiPtr->rowSpan, cubiPtr,
			neededSpace - usedSpace);
		}
	    }
	}
    }
    totalSpace = SetNominalSizes(tablePtr, &(tablePtr->rows));
    tablePtr->normalHeight =
	ConstrainHeight(totalSpace, &(tablePtr->reqHeight));
    tablePtr->normalHeight += (twiceBW + PADDING(tablePtr->padY) + tablePad);
}

/*
 * ----------------------------------------------------------------------------
 *
 * ArrangeCubicles
 *
 *	Places each slave window at its proper location.  First determines
 *	the size and position of the each window.  It then considers the
 *	following:
 *
 *	  1. translation of slave window position its parent window.
 *	  2. fill style
 *	  3. anchor
 *	  4. external and internal padding
 *	  5. window size must be greater than zero
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The size of each partition is re-initialized its minimum size.
 *
 * ----------------------------------------------------------------------------
 */
static void
ArrangeCubicles(tablePtr)
    Table *tablePtr;		/* Table widget structure */
{
    register Blt_ListItem *iPtr;
    register Cubicle *cubiPtr;
    register int width, height;
    Partition *colPtr, *rowPtr;
    register int x, y;
    int winWidth, winHeight;
    int deltaX, deltaY;
    int maxX, maxY;

    maxX = tablePtr->masterWidth - (Tk_InternalBorderWidth(tablePtr->tkwin) +
	tablePtr->padRight + tablePtr->editorTablePad);
    maxY = tablePtr->masterHeight - (Tk_InternalBorderWidth(tablePtr->tkwin) +
	tablePtr->padBottom + tablePtr->editorTablePad);

    for (iPtr = Blt_FirstListItem(&(tablePtr->cubicles)); iPtr != NULL;
	iPtr = Blt_NextItem(iPtr)) {
	cubiPtr = (Cubicle *)Blt_GetItemValue(iPtr);

	colPtr = tablePtr->colArr + cubiPtr->colIndex;
	rowPtr = tablePtr->rowArr + cubiPtr->rowIndex;

	x = colPtr->offset + colPtr->pad.side1 + cubiPtr->padLeft + 
	    cubiPtr->extBW + tablePtr->editorCubiclePad;
	y = rowPtr->offset + rowPtr->pad.side1 + cubiPtr->padTop + 
	    cubiPtr->extBW + tablePtr->editorCubiclePad;

	/*
	 * Unmap any slave windows which start outside of the master window.
	 */
	if ((x >= maxX) || (y >= maxY)) {
	    if (Tk_IsMapped(cubiPtr->tkwin)) {
		if (Tk_Parent(cubiPtr->tkwin) != tablePtr->tkwin) {
		    Tk_UnmaintainGeometry(cubiPtr->tkwin, tablePtr->tkwin);
		}
		Tk_UnmapWindow(cubiPtr->tkwin);
	    }
	    continue;
	}
	width = GetSpan(tablePtr, colPtr, cubiPtr->colSpan, EXCLUDE_PAD) -
	    ((2 * cubiPtr->extBW) + PADDING(cubiPtr->padX) +
	    (2 * tablePtr->editorCubiclePad));
	height = GetSpan(tablePtr, rowPtr, cubiPtr->rowSpan, EXCLUDE_PAD) -
	    ((2 * cubiPtr->extBW) + PADDING(cubiPtr->padY) +
	    (2 * tablePtr->editorCubiclePad));

	winWidth = GetReqWidth(cubiPtr);
	winHeight = GetReqHeight(cubiPtr);

	/*
	 *
	 * Compare the window's requested size to the size of the span.
	 *
	 * 1) If the window is too larger than the span or if the fill flag
	 *    is set, make the window the size of the span. Check that the
	 *    new size is within the bounds set for the window.
	 *
	 * 2) Otherwise, position the window in the space according to its
	 *    anchor.
	 *
	 */
	if ((width <= winWidth) || (cubiPtr->fill & FILL_X)) {
	    winWidth = width;
	    if (winWidth > cubiPtr->reqWidth.max) {
		winWidth = cubiPtr->reqWidth.max;
	    }
	}
	if ((height <= winHeight) || (cubiPtr->fill & FILL_Y)) {
	    winHeight = height;
	    if (winHeight > cubiPtr->reqHeight.max) {
		winHeight = cubiPtr->reqHeight.max;
	    }
	}
	/*
	 * If the window is too small to be interesting (i.e. it has only
	 * an external border) then unmap it.
	 */
	if ((winWidth < 1) || (winHeight < 1)) {
	    if (Tk_IsMapped(cubiPtr->tkwin)) {
		if (tablePtr->tkwin != Tk_Parent(cubiPtr->tkwin)) {
		    Tk_UnmaintainGeometry(cubiPtr->tkwin, tablePtr->tkwin);
		}
		Tk_UnmapWindow(cubiPtr->tkwin);
	    }
	    continue;
	}
	deltaX = deltaY = 0;
	if (width > winWidth) {
	    deltaX = (width - winWidth);
	}
	if (height > winHeight) {
	    deltaY = (height - winHeight);
	}
	if ((deltaX > 0) || (deltaY > 0)) {
	    XPoint newPos;

	    newPos = TranslateAnchor(deltaX, deltaY, cubiPtr->anchor);
	    x += newPos.x, y += newPos.y;
	}
	/*
	 * Clip the slave window at the bottom and/or right edge of the
	 * master
	 */
	if (winWidth > (maxX - x)) {
	    winWidth = (maxX - x);
	}
	if (winHeight > (maxY - y)) {
	    winHeight = (maxY - y);
	}
	cubiPtr->originX = x;
	cubiPtr->originY = y;

	/*
	 * Resize or move the window if necessary.
	 */

	if (tablePtr->tkwin != Tk_Parent(cubiPtr->tkwin)) {
	    Tk_MaintainGeometry(cubiPtr->tkwin, tablePtr->tkwin, x, y,
		winWidth, winHeight);
	} else {
	    if ((x != Tk_X(cubiPtr->tkwin)) || (y != Tk_Y(cubiPtr->tkwin)) ||
		(winWidth != Tk_Width(cubiPtr->tkwin)) ||
		(winHeight != Tk_Height(cubiPtr->tkwin))) {

#ifdef notdef
		fprintf(stderr, "%s at %d,%d is %dx%d\n",
		    Tk_PathName(cubiPtr->tkwin), x, y, winWidth, winHeight);
#endif
		Tk_MoveResizeWindow(cubiPtr->tkwin, x, y, winWidth, winHeight);
	    }
	    if (!Tk_IsMapped(cubiPtr->tkwin)) {
		Tk_MapWindow(cubiPtr->tkwin);
	    }
	}
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * ArrangeTable --
 *
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The slave windows in the table are possibly resized and redrawn.
 *
 * ----------------------------------------------------------------------------
 */
static void
ArrangeTable(clientData)
    ClientData clientData;
{
    Table *tablePtr = (Table *)clientData;
    int width, height;
    register int i;
    int offset;
    int padX, padY;
    int outerPad;

#ifdef notdef
    fprintf(stderr, "ArrangeTable(%s)\n", Tk_PathName(tablePtr->tkwin));
#endif
    Tcl_Preserve((ClientData)tablePtr);
    tablePtr->flags &= ~ARRANGE_PENDING;

    tablePtr->editorPartitionPad = tablePtr->editorTablePad =
	tablePtr->editorCubiclePad = 0;
    if (tablePtr->editPtr != NULL) {
	tablePtr->editorPartitionPad = tablePtr->editPtr->gridLineWidth;
	tablePtr->editorTablePad = tablePtr->editPtr->buttonHeight;
	tablePtr->editorCubiclePad = tablePtr->editPtr->cubiclePad;
    }
    /*
     * If the table has no children anymore, then don't do anything at all:
     * just leave the master window's size as-is.
     */
    if ((Blt_GetListLength(&(tablePtr->cubicles)) == 0) ||
	(tablePtr->tkwin == NULL)) {
	Tcl_Release((ClientData)tablePtr);
	return;
    }
    if (tablePtr->flags & REQUEST_LAYOUT) {
	tablePtr->flags &= ~REQUEST_LAYOUT;
	LayoutPartitions(tablePtr);
    }
    /*
     * Initially, try to fit the partitions exactly into the master window
     * by resizing the window.  If the window's requested size is
     * different, send a request to the master window's geometry manager to
     * resize.
     */
    if ((tablePtr->propagate) &&
	((tablePtr->normalWidth != Tk_ReqWidth(tablePtr->tkwin)) ||
	    (tablePtr->normalHeight != Tk_ReqHeight(tablePtr->tkwin)))) {
	Tk_GeometryRequest(tablePtr->tkwin, tablePtr->normalWidth,
	    tablePtr->normalHeight);
	tablePtr->flags |= ARRANGE_PENDING;
	Tcl_DoWhenIdle(ArrangeTable, (ClientData)tablePtr);
	Tcl_Release((ClientData)tablePtr);
	return;
    }
    /*
     * Save the width and height of the master window so we know when it
     * has changed during ConfigureNotify events.
     */
    tablePtr->masterWidth = Tk_Width(tablePtr->tkwin);
    tablePtr->masterHeight = Tk_Height(tablePtr->tkwin);
    outerPad = 2 * (Tk_InternalBorderWidth(tablePtr->tkwin) +
	tablePtr->editorTablePad) + tablePtr->editorPartitionPad;
    padX = outerPad + PADDING(tablePtr->padX);
    padY = outerPad + PADDING(tablePtr->padY);

    width = GetSpan(tablePtr, tablePtr->colArr,
	tablePtr->columns.length, INCLUDE_PAD) + padX;
    height = GetSpan(tablePtr, tablePtr->rowArr, tablePtr->numRows,
	INCLUDE_PAD) + padY;

    /*
     * If the previous geometry request was not fulfilled (i.e. the size of
     * the master window is different from partitions' space requirements),
     * try to adjust size of the partitions to fit the window.
     */
    if (tablePtr->masterWidth != width) {
	AdjustPartitions(tablePtr->colArr, tablePtr->columns.length,
	    tablePtr->masterWidth - width);
	width = GetSpan(tablePtr, tablePtr->colArr,
	    tablePtr->columns.length, INCLUDE_PAD) + padX;
    }
    if (tablePtr->masterHeight != height) {
	AdjustPartitions(tablePtr->rowArr, tablePtr->numRows,
	    tablePtr->masterHeight - height);
	height = GetSpan(tablePtr, tablePtr->rowArr,
	    tablePtr->numRows, INCLUDE_PAD) + padY;
    }
    /*
     * If after adjusting the size of the partitions the space required
     * does not equal the size of the window, do one of the following:
     *
     * 1) If it's smaller, center the table in the window.
     * 2) If it's bigger, clip the partitions that extend beyond
     *    the edge of the master window.
     *
     * Set the row and column offsets (including the master's internal
     * border width). To be used later when positioning the slave
     * windows.
     */
    offset = Tk_InternalBorderWidth(tablePtr->tkwin) + tablePtr->padLeft +
	tablePtr->editorTablePad;
    if (width < tablePtr->masterWidth) {
	offset += (tablePtr->masterWidth - width) / 2;
    }
    for (i = 0; i < tablePtr->columns.length; i++) {
	tablePtr->colArr[i].offset = offset + tablePtr->editorPartitionPad;
	offset += tablePtr->colArr[i].size;
    }

    offset = Tk_InternalBorderWidth(tablePtr->tkwin) + tablePtr->padTop +
	tablePtr->editorTablePad;
    if (height < tablePtr->masterHeight) {
	offset += (tablePtr->masterHeight - height) / 2;
    }
    for (i = 0; i < tablePtr->numRows; i++) {
	tablePtr->rowArr[i].offset = offset + tablePtr->editorPartitionPad;
	offset += tablePtr->rowArr[i].size;
    }
    ArrangeCubicles(tablePtr);
    if (tablePtr->editPtr != NULL) {
	/* Redraw the editor */
	(*tablePtr->editPtr->drawProc) (tablePtr->editPtr);
    }
    Tcl_Release((ClientData)tablePtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * Table operations.
 *
 * The fields for Blt_OperSpec are as follows:
 *
 *   - operation name
 *   - minimum number of characters required to disambiguate the operation name.
 *   - function associated with operation.
 *   - minimum number of arguments required.
 *   - maximum number of arguments allowed (0 indicates no limit).
 *   - usage string
 *
 * ----------------------------------------------------------------------------
 */
static Blt_OperSpec operSpecs[] =
{
    {"arrange", 1, (Blt_OperProc) ArrangeOper, 3, 3, "master",},
    {"cget", 2, (Blt_OperProc) CgetOper, 4, 5, "master ?item? option",},
    {"configure", 2, (Blt_OperProc) ConfigureOper, 3, 0,
	"master ?item?... ?option value?...",},
    {"extents", 1, (Blt_OperProc) ExtentsOper, 4, 4, "master item",},
    {"forget", 1, (Blt_OperProc) ForgetOper, 3, 0, "slave ?slave?...",},
    {"info", 1, (Blt_OperProc) InfoOper, 3, 0, "master ?item?...",},
    {"locate", 2, (Blt_OperProc) LocateOper, 5, 5, "master x y",},
    {"masters", 1, (Blt_OperProc) MastersOper, 2, 4, "?switch? ?arg?",},
    {"search", 1, (Blt_OperProc) SearchOper, 3, 0, "master ?switch arg?...",},
};

static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

/*
 * ----------------------------------------------------------------------------
 *
 * TableCmd --
 *
 *	This procedure is invoked to process the Tcl command that
 *	corresponds to the table geometry manager.  See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 * ----------------------------------------------------------------------------
 */
static int
TableCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_OperProc proc;
    int result;

    /* Initialize structures managing all table widgets once. */

    if (!initialized) {
	Tcl_InitHashTable(&masterDirectory, TCL_ONE_WORD_KEYS);
	Tcl_InitHashTable(&slaveDirectory, TCL_ONE_WORD_KEYS);
	initialized = TRUE;
    }
    if ((argc > 1) && (argv[1][0] == '.')) {
	Table *tablePtr;
	Tk_Window mainWindow = (Tk_Window)clientData;

	tablePtr = Blt_FindTable(interp, argv[1], mainWindow, 0);
	/*
	 * If the table doesn't exist, create one.
	 */
	if (tablePtr == NULL) {
	    tablePtr = CreateTable(interp, argv[1], mainWindow);
	    if (tablePtr == NULL) {
		return TCL_ERROR;
	    }
	}
	return (BuildTable(tablePtr, interp, argc, argv));
    }
    proc = Blt_LookupOperation(interp, numSpecs, operSpecs, BLT_OPER_ARG1,
	argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, argc, argv);
    return (result);
}

/*
 * ----------------------------------------------------------------------------
 *
 * Blt_TableInit --
 *
 *	This procedure is invoked to initialize the Tcl command that
 *	corresponds to the table geometry manager.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates the new command and adds an entry into a global Tcl
 *	associative array.
 *
 * ---------------------------------------------------------------------------
 */
int
Blt_TableInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = {
	"table", TABLE_VERSION, (Tcl_CmdProc*)TableCmd,
    };

    return (Blt_InitCmd(interp, &cmdSpec));
}
#endif /* !NO_TABLE */
