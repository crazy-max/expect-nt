/*
 * bltGrGrid.c --
 *
 *	This module implements grid lines for the graph widget.
 *	Graph widget created by Sani Nassif and George Howlett.
 *
 * Copyright 1995-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "bltInt.h"
#ifndef NO_GRAPH

#include "bltGraph.h"

extern Tk_CustomOption bltLengthOption;
extern Tk_CustomOption bltDashesOption;
extern Tk_CustomOption bltMapXAxisOption;
extern Tk_CustomOption bltMapYAxisOption;

/*
 * -------------------------------------------------------------------
 *
 * Grid
 *
 *	Contains attributes of describing how to draw grids (at major
 *	ticks) in the graph.  Grids may be mapped to either/both x and
 *	y axis.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    GC gc;			/* Graphics context for the grid. */
    unsigned int axisMask;	/* Specifies which x and y axes are mapped
				 * to the grid */
    int mapped;			/* If non-zero, indicates that the grid
				 * is currently displayed. */
    int minorGrid;		/* If non-zero, draw grid line for minor
				 * axis ticks too */
    GridDrawProc *drawProc;
    GridPrintProc *printProc;
    GridDestroyProc *destroyProc;

    Dashes dashes;		/* Dashstyle of the grid. This represents
				 * an array of alternatingly drawn pixel
				 * values. */
    int lineWidth;		/* Width of the grid lines */
    XColor *colorPtr;		/* Color of the grid lines */

} Grid;

#define DEF_GRID_DASHES		"2 4"
#define DEF_GRID_FG_COLOR	GRAY64
#define DEF_GRID_FG_MONO	BLACK
#define DEF_GRID_LINE_WIDTH	"0"
#define DEF_GRID_MAPPED		"0"
#define DEF_GRID_MINOR		"1"
#define DEF_GRID_MAP_X		"x"
#define DEF_GRID_MAP_Y		"y"
#define DEF_GRID_POSITION	(char *)NULL

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_COLOR, "-color", "gridColor", "GridColor",
	DEF_GRID_FG_COLOR, Tk_Offset(Grid, colorPtr),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-color", "gridColor", "GridColor",
	DEF_GRID_FG_MONO, Tk_Offset(Grid, colorPtr),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-dashes", "gridDashes", "Dashes",
	DEF_GRID_DASHES, Tk_Offset(Grid, dashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_CUSTOM, "-linewidth", "gridLineWidth", "GridLinewidth",
	DEF_GRID_LINE_WIDTH, Tk_Offset(Grid, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-mapped", "gridMapped", "GridMapped",
	DEF_GRID_MAPPED, Tk_Offset(Grid, mapped),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "gridMapX", "GridMapX",
	DEF_GRID_MAP_X, Tk_Offset(Grid, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "gridMapY", "GridMapY",
	DEF_GRID_MAP_Y, Tk_Offset(Grid, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapYAxisOption},
    {TK_CONFIG_BOOLEAN, "-minor", "gridMinor", "GridMinor",
	DEF_GRID_MINOR, Tk_Offset(Grid, minorGrid),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/* Forward declarations */

extern void Blt_GetGridSegments _ANSI_ARGS_((Graph *graphPtr,
	XSegment **segPtrPtr, int *numSegPtr));
/*
 *----------------------------------------------------------------------
 *
 * ConfigureGrid --
 *
 *	Configures attributes of the grid such as line width,
 *	dashes, and position.  The grid are first turned off
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
static int
ConfigureGrid(graphPtr, argc, argv, flags)
    Graph *graphPtr;
    int argc;
    char **argv;
    unsigned int flags;
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;
    Grid *gridPtr = (Grid *)graphPtr->gridPtr;

    if (Tk_ConfigureWidget(graphPtr->interp, graphPtr->tkwin, configSpecs,
	    argc, argv, (char *)gridPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    gcValues.background = gcValues.foreground = gridPtr->colorPtr->pixel;
    gcValues.line_width = LINEWIDTH(gridPtr->lineWidth);
    gcMask = (GCForeground | GCBackground | GCLineWidth);
    if (gridPtr->dashes.numValues > 0) {
	gcValues.line_style = LineOnOffDash;
	gcMask |= GCLineStyle;
    }
    newGC = Blt_GetUnsharedGC(graphPtr->tkwin, gcMask, &gcValues);
    if (gridPtr->dashes.numValues > 0) {
	XSetDashes(graphPtr->display, newGC, 0, gridPtr->dashes.valueList,
	    gridPtr->dashes.numValues);
    }
    if (gridPtr->gc != NULL) {
	XFreeGC(graphPtr->display, gridPtr->gc);
    }
    gridPtr->gc = newGC;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DrawGrid --
 *
 *	Draws the grid lines associated with each axis.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
DrawGrid(graphPtr)
    Graph *graphPtr;
{
    Grid *gridPtr = (Grid *)graphPtr->gridPtr;
    XSegment *segArr;
    int numSegs;

    if (!gridPtr->mapped) {
	return;
    }
    /*
     * Generate line segments to represent the grid.  Line segments
     * are calculated from the major tick intervals of each axis mapped.
     */
    Blt_GetGridSegments(graphPtr, &segArr, &numSegs);
    if (numSegs > 0) {
	XDrawSegments(graphPtr->display, graphPtr->pixwin, gridPtr->gc,
	    segArr, numSegs);
	ckfree((char *)segArr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PrintGrid --
 *
 *	Prints the grid lines associated with each axis.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
PrintGrid(graphPtr)
    Graph *graphPtr;
{
    Grid *gridPtr = (Grid *)graphPtr->gridPtr;
    XSegment *segArr;
    int numSegs;

    if (!gridPtr->mapped) {
	return;
    }
    Blt_GetGridSegments(graphPtr, &segArr, &numSegs);
    if (numSegs > 0) {
	Blt_SetLineAttributes(graphPtr, gridPtr->colorPtr, gridPtr->lineWidth,
	    &(gridPtr->dashes));
	Blt_SegmentsToPostScript(graphPtr, segArr, numSegs);
	ckfree((char *)segArr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyGrid --
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Grid GC is released.
 *
 *----------------------------------------------------------------------
 */
static void
DestroyGrid(graphPtr)
    Graph *graphPtr;
{
    Grid *gridPtr = (Grid *)graphPtr->gridPtr;

    Tk_FreeOptions(configSpecs, (char *)gridPtr, graphPtr->display, 0);
    if (gridPtr->gc != NULL) {
	XFreeGC(graphPtr->display, gridPtr->gc);
    }
    ckfree((char *)gridPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_CreateGrid --
 *
 *	Creates and initializes a new grid structure.
 *
 * Results:
 *	Returns TCL_ERROR if the configuration failed, otherwise TCL_OK.
 *
 * Side Effects:
 *	Memory for grid structure is allocated.
 *
 *----------------------------------------------------------------------
 */
int
Blt_CreateGrid(graphPtr)
    Graph *graphPtr;
{
    Grid *gridPtr;

    gridPtr = (Grid *)ckcalloc(1, sizeof(Grid));
    if (gridPtr == NULL) {
	Panic("can't allocate grid structure");
    }
    gridPtr->mapped = 0;
    gridPtr->drawProc = (GridDrawProc*)DrawGrid;
    gridPtr->printProc = (GridPrintProc*)PrintGrid;
    gridPtr->destroyProc = (GridDestroyProc*)DestroyGrid;
    gridPtr->axisMask = DEF_AXIS_MASK;
    gridPtr->minorGrid = TRUE;
    graphPtr->gridPtr = (GraphGrid *)gridPtr;
    if (ConfigureGrid(graphPtr, 0, (char **)NULL, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CgetOper --
 *
 *	Queries configuration attributes of the grid such as line
 *	width, dashes, and position.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
CgetOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Grid *gridPtr = (Grid *)graphPtr->gridPtr;

    return (Tk_ConfigureValue(interp, graphPtr->tkwin, configSpecs,
	    (char *)gridPtr, argv[3], 0));
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOper --
 *
 *	Queries or resets configuration attributes of the grid
 * 	such as line width, dashes, and position.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Grid attributes are reset.  The graph is redrawn at the
 *	next idle point.
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
    Grid *gridPtr = (Grid *)graphPtr->gridPtr;

    if (argc == 3) {
	return (Tk_ConfigureInfo(interp, graphPtr->tkwin, configSpecs,
		(char *)gridPtr, (char *)NULL, 0));
    } else if (argc == 4) {
	return (Tk_ConfigureInfo(interp, graphPtr->tkwin, configSpecs,
		(char *)gridPtr, argv[3], 0));
    }
    if (ConfigureGrid(graphPtr, argc - 3, argv + 3,
	    TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    graphPtr->flags |= UPDATE_PIXMAP;
    Blt_RedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MapOper --
 *
 *	Maps the grid.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Grid attributes are reset and the graph is redrawn if necessary.
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
    Grid *gridPtr = (Grid *)graphPtr->gridPtr;

    if (!gridPtr->mapped) {
	gridPtr->mapped = TRUE;
	graphPtr->flags |= UPDATE_PIXMAP;
	Blt_RedrawGraph(graphPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MapOper --
 *
 *	Maps or unmaps the grid (off or on).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Grid attributes are reset and the graph is redrawn if necessary.
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
    Grid *gridPtr = (Grid *)graphPtr->gridPtr;

    if (gridPtr->mapped) {
	gridPtr->mapped = FALSE;
	graphPtr->flags |= UPDATE_PIXMAP;
	Blt_RedrawGraph(graphPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ToggleOper --
 *
 *	Toggles the mapped state of the grid.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	Grid is mapped/unmapped. The graph is redrawn at the next
 *	idle time.
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
    Grid *gridPtr = (Grid *)graphPtr->gridPtr;

    gridPtr->mapped = (!gridPtr->mapped);
    Blt_RedrawGraph(graphPtr);
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
 * Blt_GridOper --
 *
 *	User routine to configure grid lines.  Grids are drawn
 *	at major tick intervals across the graph.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Grid may be drawn in the plotting area.
 *
 *----------------------------------------------------------------------
 */
int
Blt_GridOper(graphPtr, interp, argc, argv)
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
