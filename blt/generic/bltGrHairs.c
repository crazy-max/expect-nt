/*
 * bltGrHairs.c --
 *
 *	This module implements a crosshair cursor for the graph widget.
 *	Graph widget created by Sani Nassif and George Howlett.
 *
 * Copyright 1991-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef NO_GRAPH

#include "bltGraph.h"

extern Tk_CustomOption bltPositionOption;
extern Tk_CustomOption bltLengthOption;
extern Tk_CustomOption bltDashesOption;

/*
 * -------------------------------------------------------------------
 *
 * Crosshairs
 *
 *	Contains the line segments positions and graphics context used
 *	to simulate crosshairs (by XORing) on the graph.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    CrosshairsToggleProc *toggleProc;
    CrosshairsUpdateProc *updateProc;
    CrosshairsConfigProc *configProc;
    CrosshairsDestroyProc *destroyProc;

    XPoint anchorPos;		/* Hot spot for crosshairs */
    int state;			/* Internal state of crosshairs. If non-zero,
				 * crosshairs are displayed. */
    int mapped;			/* Requested state of crosshairs (on/off). This
				 * is not necessarily consistent with the
				 * internal state variable.  This is true when
				 * the hot spot is off the graph.  */
    Dashes dashes;		/* Dashstyle of the crosshairs. This represents
				 * an array of alternatingly drawn pixel
				 * values. If NULL, the hairs are drawn as a
				 * solid line */
    int lineWidth;		/* Width of the simulated crosshair lines */
    XSegment segArr[2];		/* Positions of line segments representing the
				 * simulated crosshairs. */
    XColor *colorPtr;		/* Foreground color of crosshairs */
    GC gc;			/* Graphics context for crosshairs. Set to
				 * GXxor to not require redraws of graph */
} Crosshairs;

#define DEF_HAIRS_DASHES	(char *)NULL
#define DEF_HAIRS_FG_COLOR	BLACK
#define DEF_HAIRS_FG_MONO	BLACK
#define DEF_HAIRS_LINE_WIDTH	"0"
#define DEF_HAIRS_MAPPED	"0"
#define DEF_HAIRS_POSITION	(char *)NULL

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_COLOR, "-color", "color", "Color",
	DEF_HAIRS_FG_COLOR, Tk_Offset(Crosshairs, colorPtr),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-color", "color", "Color",
	DEF_HAIRS_FG_MONO, Tk_Offset(Crosshairs, colorPtr),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-dashes", "xhairsDashes", "Dashes",
	DEF_HAIRS_DASHES, Tk_Offset(Crosshairs, dashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_CUSTOM, "-linewidth", "xhairsLineWidth", "XhairsLinewidth",
	DEF_HAIRS_LINE_WIDTH, Tk_Offset(Crosshairs, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-mapped", "xhairsMapped", "XhairsMapped",
	DEF_HAIRS_MAPPED, Tk_Offset(Crosshairs, mapped),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-position", "xhairsPosition", "XhairsPosition",
	DEF_HAIRS_POSITION, Tk_Offset(Crosshairs, anchorPos),
	0, &bltPositionOption},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

extern int Blt_PointOnGraph _ANSI_ARGS_((Graph *, int, int));

/*
 *----------------------------------------------------------------------
 *
 * TurnOffHairs --
 *
 *	XOR's the existing line segments (representing the crosshairs),
 *	thereby erasing them.  The internal state of the crosshairs is
 *	tracked.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Crosshairs are erased.
 *
 *----------------------------------------------------------------------
 */
static void
TurnOffHairs(tkwin, hairsPtr)
    Tk_Window tkwin;
    Crosshairs *hairsPtr;
{
    if (Tk_IsMapped(tkwin) && (hairsPtr->state)) {
	XDrawSegments(Tk_Display(tkwin), Tk_WindowId(tkwin), hairsPtr->gc,
	    hairsPtr->segArr, 2);
	hairsPtr->state = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TurnOnHairs --
 *
 *	Draws (by XORing) new line segments, creating the effect of
 *	crosshairs. The internal state of the crosshairs is tracked.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Crosshairs are displayed.
 *
 *----------------------------------------------------------------------
 */
static void
TurnOnHairs(tkwin, hairsPtr)
    Tk_Window tkwin;
    Crosshairs *hairsPtr;
{
    if (Tk_IsMapped(tkwin) && (!hairsPtr->state)) {
	XDrawSegments(Tk_Display(tkwin), Tk_WindowId(tkwin), hairsPtr->gc,
	    hairsPtr->segArr, 2);
	hairsPtr->state = 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureCrosshairs --
 *
 *	Configures attributes of the crosshairs such as line width,
 *	dashes, and position.  The crosshairs are first turned off
 *	before any of the attributes changes.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Crosshair GC is allocated.
 *
 *----------------------------------------------------------------------
 */
static void
ConfigureCrosshairs(graphPtr)
    Graph *graphPtr;
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;
    long colorValue;
    Crosshairs *hairsPtr = (Crosshairs *)graphPtr->crosshairs;

    /*
     * Turn off the crosshairs temporarily. This is in case the new
     * configuration changes the size, style, or position of the lines.
     */
    TurnOffHairs(graphPtr->tkwin, hairsPtr);

    gcValues.function = GXxor;

    /* The graph's color option may not have been set yet */
    if (graphPtr->plotBg == NULL) {
	colorValue = WhitePixelOfScreen(Tk_Screen(graphPtr->tkwin));
    } else {
	colorValue = graphPtr->plotBg->pixel;
    }
    gcValues.background = colorValue;
    gcValues.foreground = (colorValue ^ hairsPtr->colorPtr->pixel);

    gcValues.line_width = LINEWIDTH(hairsPtr->lineWidth);
    gcMask = (GCForeground | GCBackground | GCFunction | GCLineWidth);
    if (hairsPtr->dashes.numValues > 0) {
	gcValues.line_style = LineOnOffDash;
	gcMask |= GCLineStyle;
    }
    newGC = Blt_GetUnsharedGC(graphPtr->tkwin, gcMask, &gcValues);
    if (hairsPtr->dashes.numValues > 0) {
	XSetDashes(graphPtr->display, newGC, 0, hairsPtr->dashes.valueList,
	    hairsPtr->dashes.numValues);
    }
    if (hairsPtr->gc != NULL) {
	XFreeGC(graphPtr->display, hairsPtr->gc);
    }
    hairsPtr->gc = newGC;

    /*
     * Are the new coordinates on the graph?
     */
    if (!Blt_PointOnGraph(graphPtr, hairsPtr->anchorPos.x,
	    hairsPtr->anchorPos.y)) {
	return;			/* Coordinates are off the graph */
    }
    hairsPtr->segArr[0].x2 = hairsPtr->segArr[0].x1 = hairsPtr->anchorPos.x;
    hairsPtr->segArr[0].y1 = (int)graphPtr->lly;
    hairsPtr->segArr[0].y2 = (int)graphPtr->ury;
    hairsPtr->segArr[1].y2 = hairsPtr->segArr[1].y1 = hairsPtr->anchorPos.y;
    hairsPtr->segArr[1].x1 = (int)graphPtr->llx;
    hairsPtr->segArr[1].x2 = (int)graphPtr->urx;

    if (hairsPtr->mapped) {
	TurnOnHairs(graphPtr->tkwin, hairsPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ToggleCrosshairs --
 *
 *	Turn on or off crosshair simulation if it has been requested
 *	and the current crosshair position is in the plotting area.
 *	This routine is used to erase the crosshairs before the
 *	graph is redrawn (invalidating the XOR'ed old crosshairs).
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Crosshairs may be drawn or erased from the plotting area.
 *
 *----------------------------------------------------------------------
 */
static void
ToggleCrosshairs(graphPtr)
    Graph *graphPtr;
{
    Crosshairs *hairsPtr = (Crosshairs *)graphPtr->crosshairs;

    if ((hairsPtr->mapped) && (hairsPtr->state)) {
	XDrawSegments(graphPtr->display, Tk_WindowId(graphPtr->tkwin),
	    hairsPtr->gc, hairsPtr->segArr, 2);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateCrosshairs --
 *
 *	Update the length of the hairs (not the hot spot).
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
UpdateCrosshairs(graphPtr)
    Graph *graphPtr;
{
    Crosshairs *hairsPtr = (Crosshairs *)graphPtr->crosshairs;

    hairsPtr->segArr[0].y1 = (int)graphPtr->lly;
    hairsPtr->segArr[0].y2 = (int)graphPtr->ury;
    hairsPtr->segArr[1].x1 = (int)graphPtr->llx;
    hairsPtr->segArr[1].x2 = (int)graphPtr->urx;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyCrosshairs --
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Crosshair GC is allocated.
 *
 *----------------------------------------------------------------------
 */
static void
DestroyCrosshairs(graphPtr)
    Graph *graphPtr;
{
    Crosshairs *hairsPtr = (Crosshairs *)graphPtr->crosshairs;

    Tk_FreeOptions(configSpecs, (char *)hairsPtr, graphPtr->display, 0);
    if (hairsPtr->gc != NULL) {
	XFreeGC(graphPtr->display, hairsPtr->gc);
    }
    ckfree((char *)hairsPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_CreateCrosshairs --
 *
 *	Creates and initializes a new crosshair structure.
 *
 * Results:
 *	Returns TCL_ERROR if the crosshair structure cannot be created,
 *	otherwise TCL_OK.
 *
 * Side Effects:
 *	Crosshair GC is allocated.
 *
 *----------------------------------------------------------------------
 */
int
Blt_CreateCrosshairs(graphPtr)
    Graph *graphPtr;
{
    Crosshairs *hairsPtr;

    hairsPtr = (Crosshairs *)ckcalloc(1, sizeof(Crosshairs));
    if (hairsPtr == NULL) {
	Panic("can't allocate crosshairs structure");
    }
    hairsPtr->mapped = hairsPtr->state = 0;
    hairsPtr->destroyProc = (CrosshairsDestroyProc*)DestroyCrosshairs;
    hairsPtr->toggleProc = (CrosshairsToggleProc*)ToggleCrosshairs;
    hairsPtr->updateProc = (CrosshairsUpdateProc*)UpdateCrosshairs;
    hairsPtr->configProc = (CrosshairsConfigProc*)ConfigureCrosshairs;
    graphPtr->crosshairs = (GraphCrosshairs *)hairsPtr;
    if (Tk_ConfigureWidget(graphPtr->interp, graphPtr->tkwin, configSpecs,
	    0, (char **)NULL, (char *)hairsPtr, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CgetOper --
 *
 *	Queries configuration attributes of the crosshairs such as
 *	line width, dashes, and position.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static int
CgetOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;			/* unused */
    char **argv;
{
    Crosshairs *hairsPtr = (Crosshairs *)graphPtr->crosshairs;

    return (Tk_ConfigureValue(interp, graphPtr->tkwin, configSpecs,
	    (char *)hairsPtr, argv[3], 0));
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOper --
 *
 *	Queries or resets configuration attributes of the crosshairs
 * 	such as line width, dashes, and position.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Crosshairs are reset.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Crosshairs *hairsPtr = (Crosshairs *)graphPtr->crosshairs;

    if (argc == 3) {
	return (Tk_ConfigureInfo(interp, graphPtr->tkwin, configSpecs,
		(char *)hairsPtr, (char *)NULL, 0));
    } else if (argc == 4) {
	return (Tk_ConfigureInfo(interp, graphPtr->tkwin, configSpecs,
		(char *)hairsPtr, argv[3], 0));
    }
    if (Tk_ConfigureWidget(interp, graphPtr->tkwin, configSpecs, argc - 3,
	    argv + 3, (char *)hairsPtr, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    ConfigureCrosshairs(graphPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MapOper --
 *
 *	Maps the crosshairs.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Crosshairs are reset if necessary.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MapOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Crosshairs *hairsPtr = (Crosshairs *)graphPtr->crosshairs;

    if (!hairsPtr->mapped) {
	hairsPtr->mapped = TRUE;
	TurnOnHairs(graphPtr->tkwin, hairsPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UnmapOper --
 *
 *	Unmaps the crosshairs.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Crosshairs are reset if necessary.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
UnmapOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Crosshairs *hairsPtr = (Crosshairs *)graphPtr->crosshairs;

    if (hairsPtr->mapped) {
	hairsPtr->mapped = FALSE;
	TurnOffHairs(graphPtr->tkwin, hairsPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ToggleOper --
 *
 *	Toggles the state of the crosshairs.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Crosshairs are reset.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ToggleOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Crosshairs *hairsPtr = (Crosshairs *)graphPtr->crosshairs;

    hairsPtr->mapped = (!hairsPtr->mapped);
    if (hairsPtr->mapped) {
	TurnOnHairs(graphPtr->tkwin, hairsPtr);
    } else {
	TurnOffHairs(graphPtr->tkwin, hairsPtr);
    }
    return TCL_OK;
}


static Blt_OperSpec operSpecs[] =
{
    {"cget", 2, (Blt_OperProc) CgetOper, 4, 4, "option",},
    {"configure", 2, (Blt_OperProc) ConfigureOper, 3, 0, "?options...?",},
    {"off", 2, (Blt_OperProc) UnmapOper, 3, 3, "",},
    {"on", 2, (Blt_OperProc) MapOper, 3, 3, "",},
    {"toggle", 1, (Blt_OperProc) ToggleOper, 3, 3, "",},
};
static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

/*
 *----------------------------------------------------------------------
 *
 * Blt_CrosshairsOper --
 *
 *	User routine to configure crosshair simulation.  Crosshairs
 *	are simulated by drawing line segments parallel to both axes
 *	using the XOR drawing function. The allows the lines to be
 *	erased (by drawing them again) without redrawing the entire
 *	graph.  Care must be taken to erase crosshairs before redrawing
 *	the graph and redraw them after the graph is redraw.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Crosshairs may be drawn in the plotting area.
 *
 *----------------------------------------------------------------------
 */
int
Blt_CrosshairsOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_OperProc proc;

    proc = Blt_LookupOperation(interp, numSpecs, operSpecs, BLT_OPER_ARG2,
	argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return ((*proc) (graphPtr, interp, argc, argv));
}
#endif /* !NO_GRAPH */
