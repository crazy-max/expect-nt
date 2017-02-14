/*
 * bltGrBar.c --
 *
 *	This module implements bar elements in the graph widget
 *	for the Tk toolkit.
 *
 * Copyright 1991-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef NO_GRAPH

#include "bltGraph.h"
#include "bltGrElem.h"
#include <ctype.h>

static int BarModeParseProc _ANSI_ARGS_((ClientData, Tcl_Interp *, Tk_Window,
	char *, char *, int));
static char *BarModePrintProc _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltBarModeOption =
{
    BarModeParseProc, BarModePrintProc, (ClientData)0
};

extern Tk_CustomOption bltXDataOption;
extern Tk_CustomOption bltYDataOption;
extern Tk_CustomOption bltDataPairsOption;
extern Tk_CustomOption bltMapXAxisOption;
extern Tk_CustomOption bltMapYAxisOption;
extern Tk_CustomOption bltLengthOption;

#define DEF_NUM_BARS	8	/* Default size of the static array *defSpace*.
				 * Each entry is a bar of the element. If more
				 * bars are needed for an element, they must be
				 * malloc-ed. */

typedef struct {
    Graph *graphPtr;		/* Graph widget of element*/
    ElementType type;		/* Type of element is BAR_ELEMENT */
    int flags;			/* Indicates if the entire bar is active,
				 * or if coordinates need to be calculated */
    Tk_Uid nameId;		/* Identifier to refer the element.
				 * Used in the "insert", "delete", or
				 * "show", commands. */
    int mapped;			/* If non-zero, element is currently
				 * visible.*/
    Tk_ConfigSpec *configSpecs;	/* Configuration specifications */
    char *label;		/* Label displayed in legend */
    unsigned int axisMask;	/* Indicates which axes to map element's
				 * coordinates onto */
    Vector x, y;		/* Contains array of numeric values */
    int *activeIndexArr;	/* Array of indices (malloc-ed) which
				 * indicate which data points are active
				 * (drawn with "active" colors). */
    int numActiveIndices;	/* Number of active data points. Special
				 * case: if numActiveIndices==0 and the active
				 * bit is set in "flags", then all data
				 * points are drawn active. */

    ElemConfigProc *configProc;
    ElemDestroyProc *destroyProc;
    ElemDrawProc *drawNormalProc;
    ElemDrawProc *drawActiveProc;
    ElemLimitsProc *limitsProc;
    ElemClosestProc *closestProc;
    ElemCoordsProc *coordsProc;
    ElemPrintProc *printNormalProc;
    ElemPrintProc *printActiveProc;
    ElemDrawSymbolsProc *drawSymbolsProc;
    ElemPrintSymbolsProc *printSymbolsProc;

    /*
     * Bar specific attributes
     */
    Tk_3DBorder border;		/* 3D border and background color */
    XColor *normalFg;
    GC normalGC;
    Pixmap normalPixmap;
    XColor *activeFg;
    GC activeGC;
    Pixmap activePixmap;

    int borderWidth;		/* 3D border width of bar */
    int relief;			/* Relief of the bar */

    int defIndexSpace[DEF_NUM_BARS];
    int *indexArr;
    XRectangle defSegSpace[DEF_NUM_BARS];
    XRectangle *segArr;		/* Array of rectangles comprising the bar
				 * segments of the element. By default, this
				 * is set to *defSpace*.  If more segments are
				 * needed, the array will point to malloc-ed
				 * memory */
    int numVisibleSegments;	/*  */
    int padX;			/* Spacing on either side of bar */
    double barWidth;

} Bar;

#define DEF_BAR_ACTIVE_BG_COLOR "red"
#define DEF_BAR_ACTIVE_BG_MONO	WHITE
#define DEF_BAR_ACTIVE_FG_COLOR "pink"
#define DEF_BAR_ACTIVE_FG_MONO 	BLACK
#define DEF_BAR_BG_COLOR	"navyblue"
#define DEF_BAR_BG_MONO		BLACK
#define DEF_BAR_BORDERWIDTH	"2"
#define DEF_BAR_DATA		(char *)NULL
#define DEF_BAR_FG_COLOR     	"blue"
#define DEF_BAR_FG_MONO		WHITE
#define DEF_BAR_LABEL		(char *)NULL
#define DEF_BAR_MAPPED		"1"
#define DEF_BAR_RELIEF		"raised"
#define DEF_BAR_NORMAL_STIPPLE	""
#define DEF_BAR_ACTIVE_STIPPLE	""
#define DEF_BAR_AXIS_X		"x"
#define DEF_BAR_X_DATA		(char *)NULL
#define DEF_BAR_AXIS_Y		"y"
#define DEF_BAR_Y_DATA		(char *)NULL
#define DEF_BAR_WIDTH		"0.0"

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_COLOR, "-activeforeground", "elemActiveForeground", "Foreground",
	DEF_BAR_ACTIVE_FG_COLOR, Tk_Offset(Bar, activeFg),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-activeforeground", "elemActiveForeground", "Foreground",
	DEF_BAR_ACTIVE_FG_MONO, Tk_Offset(Bar, activeFg),
	TK_CONFIG_MONO_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_BITMAP, "-activestipple", "elemActiveStipple", "Stipple",
	DEF_BAR_ACTIVE_STIPPLE, Tk_Offset(Bar, activePixmap),TK_CONFIG_NULL_OK},
    {TK_CONFIG_BORDER, "-background", "elemBackground", "Background",
	DEF_BAR_BG_COLOR, Tk_Offset(Bar, border), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "elemBackground", "Background",
	DEF_BAR_BG_COLOR, Tk_Offset(Bar, border), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_DOUBLE, "-barwidth", "elemBarWidth", "BarWidth",
	DEF_BAR_WIDTH, Tk_Offset(Bar, barWidth), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_SYNONYM, "-bd", "elemBorderwidth", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "elemBackground", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-borderwidth", "elemBorderwidth", "Borderwidth",
	DEF_BAR_BORDERWIDTH, Tk_Offset(Bar, borderWidth), 0, &bltLengthOption},
    {TK_CONFIG_SYNONYM, "-fg", "elemForeground", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-data", "elemData", "Data",
	(char *)NULL, 0, 0, &bltDataPairsOption},
    {TK_CONFIG_COLOR, "-foreground", "elemForeground", "Foreground",
	DEF_BAR_FG_COLOR, Tk_Offset(Bar, normalFg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "elemForeground", "Foreground",
	DEF_BAR_FG_COLOR, Tk_Offset(Bar, normalFg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_STRING, "-label", "elemLabel", "Label",
	DEF_BAR_LABEL, Tk_Offset(Bar, label), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-mapped", "elemMapped", "Mapped",
	DEF_BAR_MAPPED, Tk_Offset(Bar, mapped), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "elemMapX", "MapX",
	DEF_BAR_AXIS_X, Tk_Offset(Bar, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "elemMapY", "MapY",
	DEF_BAR_AXIS_Y, Tk_Offset(Bar, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapYAxisOption},
    {TK_CONFIG_RELIEF, "-relief", "elemRelief", "Relief",
	DEF_BAR_RELIEF, Tk_Offset(Bar, relief), 0},
    {TK_CONFIG_BITMAP, "-stipple", "elemStipple", "Stipple",
	DEF_BAR_NORMAL_STIPPLE, Tk_Offset(Bar, normalPixmap),TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-xdata", "elemXdata", "Xdata",
	DEF_BAR_X_DATA, 0, 0, &bltXDataOption},
    {TK_CONFIG_CUSTOM, "-ydata", "elemYdata", "Ydata",
	DEF_BAR_Y_DATA, 0, 0, &bltYDataOption},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

INLINE static double
Fabs(x)
    register double x;
{
    return ((x < 0.0) ? -x : x);
}

INLINE static int
Round(x)
    register double x;
{
    return (int) (x + ((x < 0.0) ? -0.5 : 0.5));
}

/* ----------------------------------------------------------------------
 * Custom option parse and print procedures
 * ----------------------------------------------------------------------
 */
/*
 * ----------------------------------------------------------------------
 *
 * NameOfBarMode --
 *
 *	Converts the integer representing the mode style into a string.
 *
 * ----------------------------------------------------------------------
 */
static char *
NameOfBarMode(mode)
    BarMode mode;
{
    switch (mode) {
    case MODE_NORMAL:
	return "normal";
    case MODE_STACKED:
	return "stacked";
    case MODE_ALIGNED:
	return "aligned";
    default:
	return "unknown mode value";
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ModeParseProc --
 *
 *	Converts the mode string into its numeric representation.
 *
 *	Valid mode strings are:
 *
 *      "normal"    Draw a full bar at each point in the element.
 *
 * 	"stacked"   Stack bar segments vertically. Each stack is defined
 *		    by each ordinate at a particular abscissa. The height
 *		    of each segment is represented by the sum the previous
 *		    ordinates.
 *
 *	"aligned"   Align bar segments as smaller slices one next to
 *		    the other.  Like "stacks", aligned segments are
 *		    defined by each ordinate at a particular abscissa.
 *
 * Results:
 *	A standard Tcl result.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BarModeParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* Mode style string */
    char *widgRec;		/* Cubicle structure record */
    int offset;			/* Offset of style in record */
{
    BarMode *modePtr = (BarMode *)(widgRec + offset);
    unsigned int length;
    char c;

    c = value[0];
    length = strlen(value);
    if ((c == 'n') && (strncmp(value, "normal", length) == 0)) {
	*modePtr = MODE_NORMAL;
    } else if ((c == 's') && (strncmp(value, "stacked", length) == 0)) {
	*modePtr = MODE_STACKED;
    } else if ((c == 'a') && (strncmp(value, "aligned", length) == 0)) {
	*modePtr = MODE_ALIGNED;
    } else {
	Tcl_AppendResult(interp, "bad mode argument \"", value,
	    "\": should be \"normal\", \"stacked\", or \"aligned\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * BarModePrintProc --
 *
 *	Returns the mode style string based upon the mode flags.
 *
 * Results:
 *	The mode style string is returned.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
BarModePrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Row/column structure record */
    int offset;			/* Offset of mode in Partition record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    BarMode mode = *(BarMode *)(widgRec + offset);

    return (NameOfBarMode(mode));
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureBar --
 *
 *	Sets up the appropriate configuration parameters in the GC.
 *      It is assumed the parameters have been previously set by
 *	a call to Tk_ConfigureWidget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information such as bar foreground/background
 *	color and stipple etc. get set in a new GC.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigureBar(graphPtr, elemPtr)
    Graph *graphPtr;
    register Element *elemPtr;
{
    XGCValues gcValues;
    unsigned long gcMask;
    Bar *barPtr = (Bar *)elemPtr;
    XColor *colorPtr;
    GC newGC;

    gcMask = GCForeground | GCBackground;
    colorPtr = (barPtr->activeFg != NULL) ? barPtr->activeFg : barPtr->normalFg;
    gcValues.foreground = colorPtr->pixel;
    gcValues.background = (Tk_3DBorderColor(barPtr->border))->pixel;
    if (barPtr->activePixmap != None) {
	gcValues.stipple = barPtr->activePixmap;
	gcValues.fill_style = FillOpaqueStippled;
	gcMask |= (GCStipple | GCFillStyle);
    }
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (barPtr->activeGC != NULL) {
	Tk_FreeGC(graphPtr->display, barPtr->activeGC);
    }
    barPtr->activeGC = newGC;

    gcMask = GCForeground | GCBackground;
    gcValues.foreground = barPtr->normalFg->pixel;
    if (barPtr->normalPixmap != None) {
	gcValues.stipple = barPtr->normalPixmap;
	gcValues.fill_style = FillOpaqueStippled;
	gcMask |= (GCStipple | GCFillStyle);
    }
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (barPtr->normalGC != NULL) {
	Tk_FreeGC(graphPtr->display, barPtr->normalGC);
    }
    barPtr->normalGC = newGC;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputeLimits --
 *
 *	Returns the limits of the bar data
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ComputeLimits(graphPtr, elemPtr, axisPtr, minPtr, maxPtr)
    Graph *graphPtr;
    Element *elemPtr;
    GraphAxis *axisPtr;		/* Axis information */
    double *minPtr, *maxPtr;
{
    Bar *barPtr = (Bar *)elemPtr;
    Vector *vPtr;
    double min, max;

    if (ELEMENT_LENGTH(barPtr) < 1) {
	return 0;		/* No data points */
    }
    if (!(elemPtr->axisMask & AXIS_MASK(axisPtr))) {
	return 0;		/* Bar is not mapped to this axis */
    }
    min = bltPosInfinity, max = bltNegInfinity;
    if (AXIS_MASK(axisPtr) & AXIS_MASK_X) {
	double middle, barWidth;

	barWidth = graphPtr->barWidth;
	if (barPtr->barWidth > 0.0) {
	    barWidth = barPtr->barWidth;
	}
	vPtr = &(barPtr->x);
	middle = barWidth * 0.5;
	min = vPtr->vector.min - middle;
	max = vPtr->vector.max + middle;

	/* Warning: You get what you deserve if the x-axis is logScale */
	if (axisPtr->logScale) {
	    min = Blt_FindVectorMinimum(vPtr, DBL_MIN) + middle;
	}
    } else {
	vPtr = &(barPtr->y);
	min = barPtr->y.vector.min;
	if ((min <= 0.0) && (axisPtr->logScale)) {
	    min = Blt_FindVectorMinimum(vPtr, DBL_MIN);
	} else {
	    if (min > 0.0) {
		min = 0.0;
	    }
	}
	max = barPtr->y.vector.max;
	if (max < 0.0) {
	    max = 0.0;
	}
    }
    *minPtr = min, *maxPtr = max;
    return (vPtr->vector.numValues);
}

/*
 * ----------------------------------------------------------------------
 *
 * ClosestBar --
 *
 *	Find the bar segment closest to the window coordinates	point
 *	specified.
 *
 *	Note:  This does not return the height of the stacked segment
 *	       (in graph coordinates) properly.
 *
 * Results:
 *	Returns 1 if the point is width any bar segment, otherwise 0.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
ClosestBar(graphPtr, elemPtr, infoPtr)
    Graph *graphPtr;		/* Graph widget record */
    Element *elemPtr;		/* Bar element */
    ClosestInfo *infoPtr;	/* Index of closest point in element */
{
    Bar *barPtr = (Bar *)elemPtr;
    double dummy;
    double dist, prevMin;
    XPoint pointArr[5], *pointPtr;
    register int i, side;
    register XRectangle *rectPtr;

    prevMin = infoPtr->dist;
    rectPtr = barPtr->segArr;
    for (i = 0; i < barPtr->numVisibleSegments; i++) {
	if ((infoPtr->x >= rectPtr->x) &&
	    (infoPtr->x <= (rectPtr->x + rectPtr->width)) &&
	    (infoPtr->y >= rectPtr->y) &&
	    (infoPtr->y <= (rectPtr->y + rectPtr->height))) {

	    /*
	     * The point is interior to this bar segment.
	     */
	    infoPtr->index = barPtr->indexArr[i];
	    infoPtr->dist = 0.0;
	    break;
	}
	pointArr[4].x = pointArr[3].x = pointArr[0].x = rectPtr->x;
	pointArr[4].y = pointArr[1].y = pointArr[0].y = rectPtr->y;
	pointArr[2].x = pointArr[1].x = rectPtr->x + rectPtr->width;
	pointArr[3].y = pointArr[2].y = rectPtr->y + rectPtr->height;

	pointPtr = pointArr;
	for (side = 0; side < 4; side++) {
	    dist = Blt_GetDistanceToSegment(pointPtr, infoPtr->x, infoPtr->y,
		&dummy, &dummy);
	    if (dist < infoPtr->dist) {
		infoPtr->index = barPtr->indexArr[i];
		infoPtr->dist = dist;
	    }
	    pointPtr++;
	}
	rectPtr++;
    }
    if (infoPtr->dist < prevMin) {
	infoPtr->elemId = elemPtr->nameId;
	/* Provide the segment value, not the stacked sum */
	infoPtr->closest.x = barPtr->x.vector.valueArr[infoPtr->index];
	infoPtr->closest.y = barPtr->y.vector.valueArr[infoPtr->index];
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputeBarCoordinates --
 *
 *	Calculates the actual window coordinates of the bar element.
 *	The window coordinates are saved in the bar element structure.
 *
 * Results:
 *	None.
 *
 * Notes:
 *	A bar can have multiple segments (more than one x,y pairs).
 *	In this case, the bar can be represented as either a set of
 *	non-contiguous bars or a single multi-segmented (stacked) bar.
 *
 *	The x-axis layout for a barchart may be presented in one of
 *	two ways.  If abscissas are used, the bars are placed at those
 *	coordinates.  Otherwise, the range will represent the number
 *	of values.
 *
 * ----------------------------------------------------------------------
 */
static void
ComputeBarCoordinates(graphPtr, elemPtr)
    Graph *graphPtr;
    Element *elemPtr;
{
    AxisPair axisPair;
    Bar *barPtr = (Bar *)elemPtr;
    double x1, y1, x2, y2;	/* Two opposite corners of the rectangle
				 * in graph coordinates. */
    Coordinate corner1, corner2;
    double barWidth;
    FreqKey key;
    int numDataPoints, numVisible;
    register XRectangle *rectPtr, *rectArr, *segPtr;
    register int *indexPtr;
    register int i;
    int invertBar;
    int height;
    double *xArr, *yArr;

    /* Release any storage associated with the display of the bar */
    if (barPtr->segArr != barPtr->defSegSpace) {
	ckfree((char *)barPtr->segArr);
    }
    barPtr->segArr = barPtr->defSegSpace;
    barPtr->numVisibleSegments = 0;
    if (barPtr->indexArr != barPtr->defIndexSpace) {
	ckfree((char *)barPtr->indexArr);
    }
    barPtr->indexArr = barPtr->defIndexSpace;

    numDataPoints = ELEMENT_LENGTH(barPtr);
    if (numDataPoints < 1) {
	return;			/* No data points */
    }
    barWidth = graphPtr->barWidth;
    if (barPtr->barWidth > 0.0) {
	barWidth = barPtr->barWidth;
    }
    Blt_GetAxisMapping(graphPtr, barPtr->axisMask, &axisPair);

    /*
     * Create an array of rectangles representing the screen coordinates
     * of all the segments in the bar.  Note that not all segments may be
     * visible (i.e. some may be clipped entirely).  Segments not visible
     * will have a width and height of zero.
     */
    rectArr = (XRectangle *)ckcalloc(numDataPoints, sizeof(XRectangle));
    if (rectArr == NULL) {
	return;			/* Can't allocate bar array */
    }
    xArr = barPtr->x.vector.valueArr;
    yArr = barPtr->y.vector.valueArr;
    key.dummy = 0;
    numVisible = 0;
    rectPtr = rectArr;
    for (i = 0; i < numDataPoints; i++) {
	if (((xArr[i] - barWidth) > axisPair.x->max) ||
	    ((xArr[i] + barWidth) < axisPair.x->min)) {
	    continue;		/* Abscissa is out of range of the x-axis */
	}
	x1 = xArr[i] - (barWidth * 0.5);
	x2 = x1 + barWidth;
	y1 = yArr[i];
	if (graphPtr->histmode) {
	    if (axisPair.y->logScale) {
		y2 = BLT_EXP10(axisPair.y->min);
	    } else {
		y2 = axisPair.y->min;
	    }
	} else {
	    y2 = 0.0;
	}

	if ((graphPtr->numStacks > 0) && (graphPtr->mode != MODE_NORMAL)) {
	    Tcl_HashEntry *hPtr;

	    key.value = xArr[i];
	    key.mask = barPtr->axisMask;
	    hPtr = Tcl_FindHashEntry(&(graphPtr->freqTable), (char *)&key);
	    if (hPtr != NULL) {
		FreqInfo *infoPtr;

		infoPtr = (FreqInfo *)Tcl_GetHashValue(hPtr);
		if (graphPtr->mode == MODE_STACKED) {
		    if (!graphPtr->histmode || (graphPtr->histmode
			    && (infoPtr->lastY != bltNegInfinity))) {
			y2 = infoPtr->lastY;
			y1 += y2;
		    }
		    infoPtr->lastY = y1;
		} else {	/* MODE_ALIGNED */
		    double slice;

		    slice = barWidth / (double)infoPtr->freq;
		    x1 += (slice * infoPtr->count);
		    x2 = x1 + slice;
		    infoPtr->count++;
		}
	    }
	}
	invertBar = FALSE;
	if (y1 < y2) {
	    double temp;

	    /* Handle negative bar values by swapping ordinates */
	    temp = y1, y1 = y2, y2 = temp;
	    invertBar = TRUE;
	}
#ifdef notdef
	fprintf(stderr, "x1=%g,y1=%g x2=%g,y2=%g\n", x1, y1, x2, y2);
#endif
	/*
	 * Get the two corners of the bar segment and compute the rectangle
	 */
	corner1 = Blt_TransformPt(graphPtr, x1, y1, &axisPair);
	corner2 = Blt_TransformPt(graphPtr, x2, y2, &axisPair);

	/* Clip the bars vertically at the size of the graph window */
	if (corner1.y < 0.0) {
	    corner1.y = 0.0;
	} else if (corner1.y > (double)graphPtr->height) {
	    corner1.y = (double)graphPtr->height;
	}
	if (corner2.y < 0.0) {
	    corner2.y = 0.0;
	} else if (corner2.y > (double)graphPtr->height) {
	    corner2.y = (double)graphPtr->height;
	}
	height = (int)Round(Fabs(corner1.y - corner2.y));
	if (invertBar) {
	    rectPtr->y = (int)BLT_MIN(corner1.y, corner2.y);
	} else {
	    rectPtr->y = (int)(BLT_MAX(corner1.y, corner2.y)) - height;
	}
	rectPtr->x = (int)BLT_MIN(corner1.x, corner2.x);
	rectPtr->width = (int)Round(Fabs(corner1.x - corner2.x));
	if (rectPtr->width < 1) {
	    rectPtr->width = 1;
	}
	rectPtr->height = height;
	if (rectPtr->height < 1) {
	    rectPtr->height = 1;
	}
#ifdef notdef
	fprintf(stderr, "R%d: x=%d,y=%d,w=%d,h=%d\n", i, rectPtr->x, rectPtr->y,
	    rectPtr->width, rectPtr->height);
	fprintf(stderr, "c1 = (x=%g,y=%g) c2 = (x=%g,y=%g)\n", corner1.x,
	    corner1.y, corner2.x, corner2.y);
#endif
	numVisible++;
	rectPtr++;
    }
    barPtr->numVisibleSegments = numVisible;

    if (numVisible > DEF_NUM_BARS) {
	XRectangle *segArr;
	int *indexArr;

	segArr = (XRectangle *)ckalloc(numVisible * sizeof(XRectangle));
	if (segArr == NULL) {
	    Panic("can't allocate bar array");
	}
	barPtr->segArr = segArr;
	indexArr = (int *)ckalloc(numVisible * sizeof(int));
	if (indexArr == NULL) {
	    Panic("can't allocate index array");
	}
	barPtr->indexArr = indexArr;
    }
    /* Collect the visible segments into a single array */
    rectPtr = rectArr;
    segPtr = barPtr->segArr;
    indexPtr = barPtr->indexArr;
    for (i = 0; i < numDataPoints; i++) {
	if ((rectPtr->width > 0) && (rectPtr->height > 0)) {
	    *segPtr = *rectPtr;
	    *indexPtr = i;
	    segPtr++, indexPtr++;
	    rectPtr++;
	}
    }
    ckfree((char *)rectArr);
}

/*
 * -----------------------------------------------------------------
 *
 * DrawSymbols --
 *
 * 	Draw a symbol centered at the given x,y window coordinate
 *	based upon the element symbol type and size.
 *
 * Results:
 *	None.
 *
 * Problems:
 *	Most notable is the round-off errors generated when
 *	calculating the centered position of the symbol.
 * -----------------------------------------------------------------
 */
/*ARGSUSED*/
static void
DrawSymbols(graphPtr, elemPtr, size, pointArr, numPoints, active)
    Graph *graphPtr;
    Element *elemPtr;
    int size;
    XPoint *pointArr;
    int numPoints;
    int active;			/* unused */
{
    Bar *barPtr = (Bar *)elemPtr;
    int radius;
    register int i;

    radius = (size / 2);
    size--;
    for (i = 0; i < numPoints; i++) {
	XFillRectangle(graphPtr->display, graphPtr->pixwin, barPtr->normalGC,
	    pointArr[i].x - radius, pointArr[i].y - radius, size, size);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawNormalBar --
 *
 *	Draws the rectangle representing the bar element.  If the
 *	relief option is set to "raised" or "sunken" and the bar
 *	borderwidth is set (borderwidth > 0), a 3D border is drawn
 *	around the bar.
 *
 *	Don't draw bars that aren't visible (i.e. within the limits
 *	of the axis).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	X drawing commands are output.
 *
 * ----------------------------------------------------------------------
 */
static void
DrawNormalBar(graphPtr, elemPtr)
    Graph *graphPtr;
    Element *elemPtr;
{
    Bar *barPtr = (Bar *)elemPtr;
    register XRectangle *rectPtr;
    register int i;

    XFillRectangles(graphPtr->display, graphPtr->pixwin, barPtr->normalGC,
	barPtr->segArr, barPtr->numVisibleSegments);
    if ((barPtr->borderWidth > 0) && (barPtr->relief != TK_RELIEF_FLAT)) {
	int twiceBW;

	twiceBW = (2 * barPtr->borderWidth);
	rectPtr = barPtr->segArr;
	for (i = 0; i < barPtr->numVisibleSegments; i++) {
	    if ((twiceBW < rectPtr->width) && (twiceBW < rectPtr->height)) {
		Tk_Draw3DRectangle(graphPtr->tkwin, graphPtr->pixwin,
		    barPtr->border, rectPtr->x, rectPtr->y,
		    rectPtr->width, rectPtr->height,
		    barPtr->borderWidth, barPtr->relief);
	    }
	    rectPtr++;
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * FindActiveIndex --
 *
 * 	Performs a binary search for the given index in the array
 * 	of visible bar segments.  This array contains the indices 
 * 	of the data points representing a visible (non-clipped)
 *	segment of the bar element.
 *
 * Results:
 *	Returns the index of the entry in the index array which 
 *	matches *key*.  If the index key is not found, -1 is returned.
 *
 * ----------------------------------------------------------------------
 */
static int
FindActiveIndex(barPtr, key)
    Bar *barPtr;
    int key;
{
    int median;
    int low, high;		/* Range of indices to search */

    high = barPtr->numVisibleSegments - 1;
    low = 0;
    while (low <= high) {
	median = (low + high) >> 1;
	if (key < barPtr->indexArr[median]) {
	    high = median - 1;
	} else if (key > barPtr->indexArr[median]) {
	    low = median + 1;
	} else {
	    return (median);
	}
    }
    return -1;
}


/*
 * ----------------------------------------------------------------------
 *
 * GetVisibleActiveSegments --
 *
 * 	Generates an array of rectangles representing the active, 
 *	visible segments of a bar element.  Each data point marked
 *	as active is examined to see if it's currently visible
 *	on the graph.  We simply need to test if the index is in 
 *	*indexArr* which contains the indices of the data points 
 *	representing visible bar segments.  
 *
 * Results:
 *	Returns the number of visible active segments and stores
 *	in *segPtrPtr*, the malloc-ed array of rectangles representing
 *	those segments.  The caller will be responsible for freeing 
 *	the memory.
 *
 * ----------------------------------------------------------------------
 */
static int
GetVisibleActiveSegments(barPtr, segPtrPtr)
    Bar *barPtr;
    XRectangle **segPtrPtr;
{
    register int i;
    register XRectangle *rectPtr;
    XRectangle *rectArr;
    int numActive;
    int activeIndex, segIndex;
    int numDataPoints;
    
    /* Allocate array of active bars */
    rectArr = (XRectangle *)ckalloc(sizeof(XRectangle) * 
	barPtr->numActiveIndices);
    if (rectArr == NULL) {
	Panic("can't allocate array of active points");
    }
    numActive = 0;
    rectPtr = rectArr;
    numDataPoints = ELEMENT_LENGTH(barPtr);
    for (i = 0; i < barPtr->numActiveIndices; i++) {
	activeIndex = barPtr->activeIndexArr[i];
	if ((activeIndex < 0) || (activeIndex >= numDataPoints)) {
	    continue;	/* No such index */
	}
	segIndex = FindActiveIndex(barPtr, activeIndex);
	if (segIndex >= 0) {
	    *rectPtr = barPtr->segArr[segIndex];
	    numActive++;
	    rectPtr++;
	}
    } 
    *segPtrPtr = rectArr;
    if (numActive == 0) {
	ckfree((char *)rectArr);
    }
    return (numActive);
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawActiveBar --
 *
 *	Draws rectangles representing the active segments of the
 *	bar element.  If the -relief option is set (other than "flat")
 *	and the borderwidth is greater than 0, a 3D border is drawn
 *	around the each bar segment.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	X drawing commands are output.
 *
 * ----------------------------------------------------------------------
 */
static void
DrawActiveBar(graphPtr, elemPtr)
    Graph *graphPtr;
    Element *elemPtr;
{
    Bar *barPtr = (Bar *)elemPtr;
    XRectangle *activePtr;
    int numActive;

    if (barPtr->numActiveIndices > 0) {
	numActive = GetVisibleActiveSegments(barPtr, &activePtr);
    } else {
	/* 
	 * If numActiveIndices is -1, then the entire element has been
	 * marked active.  
	 */
	activePtr = barPtr->segArr;
	numActive = barPtr->numVisibleSegments;
    } 
    if (numActive == 0) {
	return;
    }

    XFillRectangles(graphPtr->display, graphPtr->pixwin, barPtr->activeGC,
	activePtr, numActive);

    if ((barPtr->borderWidth > 0) && (barPtr->relief != TK_RELIEF_FLAT)) {
	int twiceBW;
	register int i;

	twiceBW = (2 * barPtr->borderWidth);
	for (i = 0; i < numActive; i++) {
	    /*
	     * Don't draw a 3D border if it will cover the surface of the bar
	     */
	    if ((twiceBW < activePtr->width) && (twiceBW < activePtr->height)) {
		Tk_Draw3DRectangle(graphPtr->tkwin, graphPtr->pixwin,
		    barPtr->border, activePtr->x, activePtr->y,
		    activePtr->width, activePtr->height,
		    barPtr->borderWidth, barPtr->relief);
	    }
	    activePtr++;
	}
    }
#if 0
    /*
     * mike: the following call to ckfree() causes SIGSEGVs on
     * some systems, as the memory already has been freed in
     * bltGrElem.c::ActivateOper().
     */
    if (barPtr->numActiveIndices > 0) {
	ckfree((char *)activePtr);
    }
#endif
}

/*
 * -----------------------------------------------------------------
 *
 * PrintSymbols --
 *
 * 	Draw a symbol centered at the given x,y window coordinate
 *	based upon the element symbol type and size.
 *
 * Results:
 *	None.
 *
 * Problems:
 *	Most notable is the round-off errors generated when
 *	calculating the centered position of the symbol.
 *
 * -----------------------------------------------------------------
 */
/*ARGSUSED*/
static void
PrintSymbols(graphPtr, elemPtr, size, pointArr, numPoints, active)
    Graph *graphPtr;
    Element *elemPtr;
    int size;
    XPoint *pointArr;
    int numPoints;
    int active;
{
    Bar *barPtr = (Bar *)elemPtr;
    Pixmap stipple;
    register int i;

    stipple = (active) ? barPtr->activePixmap : barPtr->normalPixmap;
    if (stipple != None) {
	int width, height;

	Blt_BackgroundToPostScript(graphPtr, Tk_3DBorderColor(barPtr->border));
	Tcl_AppendResult(graphPtr->interp, "/StippleProc {\ngsave\n",
	    (char *)NULL);
	Tk_SizeOfBitmap(graphPtr->display, stipple, &width, &height);
	Blt_StippleToPostScript(graphPtr, stipple, width, height, 1);
	Tcl_AppendResult(graphPtr->interp, "} def\n", (char *)NULL);
    } else {
	Blt_ForegroundToPostScript(graphPtr, barPtr->normalFg);
    }
    for (i = 0; i < numPoints; i++) {
	sprintf(graphPtr->scratchArr, "%d %d %d Sq\n", pointArr[i].x,
	    pointArr[i].y, size);
	Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
	if (stipple != None) {
	    Tcl_AppendResult(graphPtr->interp, "gsave\n", (char *)NULL);
	    Blt_ForegroundToPostScript(graphPtr, barPtr->normalFg);
	    Tcl_AppendResult(graphPtr->interp,
		"/StippleProc cvx exec\ngrestore\n", (char *)NULL);
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * PrintActiveBar --
 *
 *	Similar to the PrintNormalBar procedure, generates PostScript
 *	commands to display the rectangles representing the active bar
 *	segments of the element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PostScript pen width, dashes, and color settings are changed.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
PrintActiveBar(graphPtr, elemPtr)
    Graph *graphPtr;
    Element *elemPtr;
{
    Bar *barPtr = (Bar *)elemPtr;
    XRectangle *activePtr;
    register int i;
    int numActive;

    if (barPtr->numActiveIndices > 0) {
	numActive = GetVisibleActiveSegments(barPtr, &activePtr);
    } else {
	/* 
	 * If numActiveIndices is -1, then the entire element has been
	 * marked active.  
	 */
	activePtr = barPtr->segArr;
	numActive = barPtr->numVisibleSegments;
    } 
    if (numActive == 0) {
	return;
    }
    for (i = 0; i < numActive; i++) {
	if (barPtr->activePixmap != None) {
	    int width, height;

	    Blt_BackgroundToPostScript(graphPtr,
		Tk_3DBorderColor(barPtr->border));
	    Blt_RectangleToPostScript(graphPtr, activePtr->x, activePtr->y,
		(int)activePtr->width, (int)activePtr->height);
	    Tk_SizeOfBitmap(graphPtr->display, barPtr->activePixmap,
		&width, &height);
	    Blt_ForegroundToPostScript(graphPtr, barPtr->activeFg);
	    Blt_StippleToPostScript(graphPtr, barPtr->activePixmap,
		width, height, True);
	} else {
	    Blt_ForegroundToPostScript(graphPtr, barPtr->activeFg);
	    Blt_RectangleToPostScript(graphPtr, activePtr->x, activePtr->y,
		(int)activePtr->width, (int)activePtr->height);
	}
	if ((barPtr->borderWidth > 0) && (barPtr->relief != TK_RELIEF_FLAT)) {
	    Blt_Print3DRectangle(graphPtr, barPtr->border, activePtr->x,
		activePtr->y, (int)activePtr->width, (int)activePtr->height,
		barPtr->borderWidth, barPtr->relief);
	}
	activePtr++;
    }
    if (barPtr->numActiveIndices > 0) {
	ckfree((char *)activePtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * PrintNormalBar --
 *
 *	Generates PostScript commands to form the rectangles
 *	representing the segments of the bar element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PostScript pen width, dashes, and color settings are changed.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
PrintNormalBar(graphPtr, elemPtr)
    Graph *graphPtr;
    Element *elemPtr;
{
    Bar *barPtr = (Bar *)elemPtr;
    register XRectangle *rectPtr;
    register int i;

    rectPtr = barPtr->segArr;
    for (i = 0; i < barPtr->numVisibleSegments; i++) {
	if ((rectPtr->width < 1) || (rectPtr->height < 1)) {
	    continue;
	}
	if (barPtr->normalPixmap != None) {
	    int width, height;

	    Blt_BackgroundToPostScript(graphPtr,
		Tk_3DBorderColor(barPtr->border));
	    Blt_RectangleToPostScript(graphPtr, rectPtr->x, rectPtr->y,
		(int)rectPtr->width, (int)rectPtr->height);
	    Tk_SizeOfBitmap(graphPtr->display, barPtr->normalPixmap,
		&width, &height);
	    Blt_ForegroundToPostScript(graphPtr, barPtr->normalFg);
	    Blt_StippleToPostScript(graphPtr, barPtr->normalPixmap,
		width, height, True);
	} else {
	    Blt_ForegroundToPostScript(graphPtr, barPtr->normalFg);
	    Blt_RectangleToPostScript(graphPtr, rectPtr->x, rectPtr->y,
		(int)rectPtr->width, (int)rectPtr->height);
	}
	if ((barPtr->borderWidth > 0) &&
	    (barPtr->relief != TK_RELIEF_FLAT)) {
	    Blt_Print3DRectangle(graphPtr, barPtr->border, rectPtr->x,
		rectPtr->y, (int)rectPtr->width, (int)rectPtr->height,
		barPtr->borderWidth, barPtr->relief);
	}
	rectPtr++;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyBar --
 *
 *	Release memory and resources allocated for the bar element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the bar element is freed up.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyBar(graphPtr, elemPtr)
    Graph *graphPtr;
    Element *elemPtr;
{
    Bar *barPtr = (Bar *)elemPtr;

    Tk_FreeOptions(barPtr->configSpecs, (char *)barPtr, graphPtr->display, 0);

    if (barPtr->normalGC != NULL) {
	Tk_FreeGC(graphPtr->display, barPtr->normalGC);
    }
    if (barPtr->activeGC != NULL) {
	Tk_FreeGC(graphPtr->display, barPtr->activeGC);
    }
    if (barPtr->segArr != barPtr->defSegSpace) {
	ckfree((char *)barPtr->segArr);
    }
    if (barPtr->x.clientId != NULL) {
	Blt_FreeVectorId(barPtr->x.clientId);
    } else if (barPtr->x.vector.valueArr != NULL) {
	ckfree((char *)barPtr->x.vector.valueArr);
    }
    if (barPtr->y.clientId != NULL) {
	Blt_FreeVectorId(barPtr->y.clientId);
    } else if (barPtr->y.vector.valueArr != NULL) {
	ckfree((char *)barPtr->y.vector.valueArr);
    }
    if (barPtr->activeIndexArr != NULL) {
	ckfree((char *)barPtr->activeIndexArr);
    }
    if (barPtr->indexArr != barPtr->defIndexSpace) {
	ckfree((char *)barPtr->indexArr);
    }
    ckfree((char *)barPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_BarElement --
 *
 *	Allocate memory and initialize methods for the new bar element.
 *
 * Results:
 *	The pointer to the newly allocated element structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the bar element structure.
 *
 * ----------------------------------------------------------------------
 */
Element *
Blt_BarElement()
{
    register Bar *barPtr;

    barPtr = (Bar *)ckcalloc(1, sizeof(Bar));
    if (barPtr == NULL) {
	Panic("can't allocate a bar element structure");
    }
    barPtr->configSpecs = configSpecs;
    barPtr->configProc = (ElemConfigProc*)ConfigureBar;
    barPtr->destroyProc = (ElemDestroyProc*)DestroyBar;
    barPtr->drawNormalProc = (ElemDrawProc*)DrawNormalBar;
    barPtr->drawActiveProc = (ElemDrawProc*)DrawActiveBar;
    barPtr->limitsProc = (ElemLimitsProc*)ComputeLimits;
    barPtr->closestProc = (ElemClosestProc*)ClosestBar;
    barPtr->coordsProc = (ElemCoordsProc*)ComputeBarCoordinates;
    barPtr->printNormalProc = (ElemPrintProc*)PrintNormalBar;
    barPtr->printActiveProc = (ElemPrintProc*)PrintActiveBar;
    barPtr->drawSymbolsProc = (ElemDrawSymbolsProc*)DrawSymbols;
    barPtr->printSymbolsProc = (ElemPrintSymbolsProc*)PrintSymbols;
    barPtr->type = ELEM_BAR;
    barPtr->relief = TK_RELIEF_RAISED;
    barPtr->borderWidth = 2;
    barPtr->segArr = barPtr->defSegSpace;
    barPtr->indexArr = barPtr->defIndexSpace;
    return ((Element *)barPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_InitFreqTable --
 *
 *	Generate a table of abscissa frequencies.  Duplicate
 *	x-coordinates (depending upon the bar drawing mode) indicate
 *	that something special should be done with each bar segment
 *	mapped to the same abscissa (i.e. it should be stacked,
 *	aligned, or overlay-ed with other segments)
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is allocated for the bar element structure.
 *
 * ----------------------------------------------------------------------
 */
void
Blt_InitFreqTable(graphPtr)
    Graph *graphPtr;
{
    register Element *elemPtr;
    Blt_ListItem *itemPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;
    Bar *barPtr;
    int isNew, count;
    int numStacks, numSegs;
    int numDataPoints;
    FreqKey key;
    Tcl_HashTable freqTable;
    register int i;
    double *xArr;
    /*
     * Free resources associated with a previous frequency table. This
     * includes the array of frequency information and the table itself
     */
    if (graphPtr->freqArr != NULL) {
	ckfree((char *)graphPtr->freqArr);
	graphPtr->freqArr = NULL;
    }
    if (graphPtr->numStacks > 0) {
	Tcl_DeleteHashTable(&(graphPtr->freqTable));
	graphPtr->numStacks = 0;
    }
    if (graphPtr->mode == MODE_NORMAL) {
	return;			/* No frequency table is needed for
				 * normal mode */
    }
    Tcl_InitHashTable(&(graphPtr->freqTable), sizeof(FreqKey) / sizeof(int));

    /*
     * Initialize a hash table and fill it with unique abscissas.
     * Keep track of the frequency of each x-coordinate and how many
     * abscissas have duplicate mappings.
     */
    Tcl_InitHashTable(&freqTable, sizeof(FreqKey) / sizeof(int));
    numSegs = numStacks = 0;
    key.dummy = 0;
    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	if ((!elemPtr->mapped) || (elemPtr->type != ELEM_BAR)) {
	    continue;
	}
	numSegs++;
	barPtr = (Bar *)elemPtr;
	xArr = barPtr->x.vector.valueArr;
	numDataPoints = ELEMENT_LENGTH(barPtr);
	for (i = 0; i < numDataPoints; i++) {
	    key.value = xArr[i];
	    key.mask = barPtr->axisMask;
	    hPtr = Tcl_CreateHashEntry(&freqTable, (char *)&key, &isNew);
	    if (hPtr == NULL) {
		Panic("can't allocate freqTable entry");
	    }
	    if (isNew) {
		count = 1;
	    } else {
		count = (int)Tcl_GetHashValue(hPtr);
		if (count == 1) {
		    numStacks++;
		}
		count++;
	    }
	    Tcl_SetHashValue(hPtr, (ClientData)count);
	}
    }
    if (numSegs == 0) {
	return;			/* No bar elements to be displayed */
    }
    if (numStacks > 0) {
	FreqInfo *infoPtr;
	FreqKey *keyPtr;
	Tcl_HashEntry *h2Ptr;

	graphPtr->freqArr = (FreqInfo *)ckcalloc(numStacks, sizeof(FreqInfo));
	if (graphPtr->freqArr == NULL) {
	    Panic("can't allocate dup info array");
	}
	infoPtr = graphPtr->freqArr;
	for (hPtr = Tcl_FirstHashEntry(&freqTable, &cursor); hPtr != NULL;
	    hPtr = Tcl_NextHashEntry(&cursor)) {
	    count = (int)Tcl_GetHashValue(hPtr);
	    keyPtr = (FreqKey *)Tcl_GetHashKey(&freqTable, hPtr);
	    if (count > 1) {
		h2Ptr = Tcl_CreateHashEntry(&(graphPtr->freqTable),
		    (char *)keyPtr, &isNew);
		count = (int)Tcl_GetHashValue(hPtr);
		infoPtr->freq = count;
		infoPtr->mask = keyPtr->mask;
		Tcl_SetHashValue(h2Ptr, (ClientData)infoPtr);
		infoPtr++;
	    }
	}
    }
    Tcl_DeleteHashTable(&freqTable);
    graphPtr->numStacks = numStacks;
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_ComputeStacks --
 *
 *	Determine the height of each stack of bar segments.  A stack
 *	is created by designating two or more points with the same
 *	abscissa.  Each ordinate defines the height of a segment in
 *	the stack.  This procedure simply looks at all the data points
 *	summing the heights of each stacked segment. The sum is saved
 *	in the frequency information table.  This value will be used
 *	to calculate the y-axis limits (data limits aren't sufficient).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The heights of each stack is computed. Blt_CheckStacks will
 *	use this information to adjust the y-axis limits if necessary.
 *
 * ----------------------------------------------------------------------
 */
void
Blt_ComputeStacks(graphPtr)
    Graph *graphPtr;
{
    Element *elemPtr;
    Bar *barPtr;
    FreqKey key;
    Blt_ListItem *itemPtr;
    Tcl_HashEntry *hPtr;
    int numDataPoints;
    register int i;
    register FreqInfo *infoPtr;
    double *xArr, *yArr;

    if ((graphPtr->mode != MODE_STACKED) || (graphPtr->numStacks == 0)) {
	return;
    }
    /* Reset the sums for all duplicate values to zero. */

    infoPtr = graphPtr->freqArr;
    for (i = 0; i < graphPtr->numStacks; i++) {
	infoPtr->sum = 0.0;
	infoPtr++;
    }

    /* Look at each bar point, adding the ordinates of duplicate abscissas */

    key.dummy = 0;
    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	if ((!elemPtr->mapped) || (elemPtr->type != ELEM_BAR)) {
	    continue;
	}
	barPtr = (Bar *)elemPtr;
	xArr = barPtr->x.vector.valueArr;
	yArr = barPtr->y.vector.valueArr;
	numDataPoints = ELEMENT_LENGTH(barPtr);
	for (i = 0; i < numDataPoints; i++) {
	    key.value = xArr[i];
	    key.mask = barPtr->axisMask;
	    hPtr = Tcl_FindHashEntry(&(graphPtr->freqTable), (char *)&key);
	    if (hPtr == NULL) {
		continue;
	    }
	    infoPtr = (FreqInfo *)Tcl_GetHashValue(hPtr);
	    infoPtr->sum += yArr[i];
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_CheckStacks --
 *
 *	Check that the data limits are not superseded by the heights
 *	of stacked bar segments.  The heights are calculated by
 *	Blt_ComputeStacks.
 *
 * Results:
 *	If the y-axis limits need to be adjusted for stacked segments,
 *	*minPtr* or *maxPtr* are updated.
 *
 * Side effects:
 *	Autoscaling of the y-axis is affected.
 *
 * ----------------------------------------------------------------------
 */
void
Blt_CheckStacks(graphPtr, axisMask, minPtr, maxPtr)
    Graph *graphPtr;
    int axisMask;
    double *minPtr, *maxPtr;	/* Current minimum maximum for y-axis */
{
    FreqInfo *infoPtr;
    register int i;

    if ((graphPtr->mode != MODE_STACKED) || (graphPtr->numStacks == 0)) {
	return;
    }
    infoPtr = graphPtr->freqArr;
    for (i = 0; i < graphPtr->numStacks; i++) {
	if (infoPtr->mask & axisMask) {
	    /*
	     * Check if any of the y-values (because of stacking) are
	     * greater than the current limits of the graph.
	     */
	    if (infoPtr->sum < 0.0) {
		if (*minPtr > infoPtr->sum) {
		    *minPtr = infoPtr->sum;
		}
	    } else {
		if (*maxPtr < infoPtr->sum) {
		    *maxPtr = infoPtr->sum;
		}
	    }
	}
	infoPtr++;
    }
}


void
Blt_ResetStacks(graphPtr)
    Graph *graphPtr;
{
    register FreqInfo *infoPtr;
    register int i;

    infoPtr = graphPtr->freqArr;
    for (i = 0; i < graphPtr->numStacks; i++) {
	if (graphPtr->histmode) {
	    infoPtr->lastY = bltNegInfinity;
	} else {
	    infoPtr->lastY = 0.0;
	}
	infoPtr->count = 0;
	infoPtr++;
    }
}
#endif /* !NO_GRAPH */
