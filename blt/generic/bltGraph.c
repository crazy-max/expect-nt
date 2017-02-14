/*
 * bltGraph.c --
 *
 *	This module implements a graph widget for the Tk toolkit.
 *	Graph widget created by Sani Nassif and George Howlett.
 *
 * Copyright 1991-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * To do:
 *
 * 2) Update manual pages.
 *
 * 3) Update comments.
 *
 * 5) Surface, contour, and flow graphs
 *
 * 7) Arrows for line markers
 *
 */

#include "bltInt.h"
#ifndef NO_GRAPH

#include "bltGraph.h"

#define GRAPH_VERSION "8.0"

static char *classNames[] =
{
    "Graph", "Barchart"
};

extern Tk_CustomOption bltLengthOption;
extern Tk_CustomOption bltBarModeOption;
extern Tk_CustomOption bltTileOption;

#define DEF_GRAPH_BAR_WIDTH	"0.95"
#define DEF_GRAPH_BAR_MODE	"normal"
#define DEF_GRAPH_BG_COLOR	STD_NORMAL_BG_COLOR
#define DEF_GRAPH_BG_MONO	STD_NORMAL_BG_MONO
#define DEF_GRAPH_BORDER_WIDTH	STD_BORDERWIDTH
#define DEF_GRAPH_BUFFERED	"1"
#define DEF_GRAPH_CURSOR  	"crosshair"
#define DEF_GRAPH_FG_COLOR	STD_NORMAL_FG_COLOR
#define DEF_GRAPH_FG_MONO	STD_NORMAL_FG_MONO
#define DEF_GRAPH_FONT		STD_FONT_LARGE
#define DEF_GRAPH_HALO		"0.5i"
#define DEF_GRAPH_HALO_BAR	"0.1i"
#define DEF_GRAPH_HEIGHT	"4i"
#define DEF_GRAPH_HISTMODE	"0"
#define DEF_GRAPH_INVERT_XY	"0"
#define DEF_GRAPH_JUSTIFY	"center"
#define DEF_GRAPH_MARGIN	"0"
#define DEF_GRAPH_PLOT_BG_COLOR	WHITE
#define DEF_GRAPH_PLOT_BG_MONO	WHITE
#define DEF_GRAPH_PLOT_BW_COLOR "2"
#define DEF_GRAPH_PLOT_BW_MONO  "0"
#define DEF_GRAPH_PLOT_RELIEF	"sunken"
#define DEF_GRAPH_RELIEF	"flat"
#define DEF_GRAPH_TAKE_FOCUS	(char *)NULL
#define DEF_GRAPH_TITLE		"Graph Title"
#define DEF_GRAPH_WIDTH		"5i"

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_GRAPH_BG_COLOR, Tk_Offset(Graph, border),
	TK_CONFIG_COLOR_ONLY | ALL_MASK},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_GRAPH_BG_MONO, Tk_Offset(Graph, border),
	TK_CONFIG_MONO_ONLY | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-barmode", "barMode", "BarMode",
	DEF_GRAPH_BAR_MODE, Tk_Offset(Graph, mode),
	ALL_MASK | TK_CONFIG_DONT_SET_DEFAULT, &bltBarModeOption},
    {TK_CONFIG_DOUBLE, "-barwidth", "barWidth", "BarWidth",
	DEF_GRAPH_BAR_WIDTH, Tk_Offset(Graph, barWidth), ALL_MASK},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *)NULL, (char *)NULL, 0,
	ALL_MASK},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0,
	ALL_MASK},
    {TK_CONFIG_CUSTOM, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_GRAPH_BORDER_WIDTH, Tk_Offset(Graph, borderWidth),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-bottommargin", "bottomMargin", "Margin",
	DEF_GRAPH_MARGIN, Tk_Offset(Graph, reqBottomMargin),
	ALL_MASK, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-bufferelements", "bufferElements", "BufferElements",
	DEF_GRAPH_BUFFERED, Tk_Offset(Graph, buffered),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_GRAPH_CURSOR, Tk_Offset(Graph, cursor),
	ALL_MASK | TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0,
	ALL_MASK},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_GRAPH_FONT, Tk_Offset(Graph, fontPtr), ALL_MASK},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_GRAPH_FG_COLOR, Tk_Offset(Graph, marginFg),
	TK_CONFIG_COLOR_ONLY | ALL_MASK},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_GRAPH_FG_MONO, Tk_Offset(Graph, marginFg),
	TK_CONFIG_MONO_ONLY | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-halo", "halo", "Halo",
	DEF_GRAPH_HALO_BAR, Tk_Offset(Graph, halo),
	BARCHART_MASK, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-halo", "halo", "Halo",
	DEF_GRAPH_HALO, Tk_Offset(Graph, halo),
	XYGRAPH_MASK, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-height", "height", "Height",
	DEF_GRAPH_HEIGHT, Tk_Offset(Graph, reqHeight),
	ALL_MASK, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-histmode", "histMode", "HistMode",
	DEF_GRAPH_HISTMODE, Tk_Offset(Graph, histmode),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_BOOLEAN, "-invertxy", "invertXY", "InvertXY",
	DEF_GRAPH_INVERT_XY, Tk_Offset(Graph, inverted),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_JUSTIFY, "-justify", "justify", "Justify",
	DEF_GRAPH_JUSTIFY, Tk_Offset(Graph, justify),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-leftmargin", "leftMargin", "Margin",
	DEF_GRAPH_MARGIN, Tk_Offset(Graph, reqLeftMargin),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK, &bltLengthOption},
    {TK_CONFIG_COLOR, "-plotbackground", "plotBackground", "Background",
	DEF_GRAPH_PLOT_BG_MONO, Tk_Offset(Graph, plotBg),
	TK_CONFIG_MONO_ONLY | ALL_MASK},
    {TK_CONFIG_COLOR, "-plotbackground", "plotBackground", "Background",
	DEF_GRAPH_PLOT_BG_COLOR, Tk_Offset(Graph, plotBg),
	TK_CONFIG_COLOR_ONLY | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-plotborderwidth", "plotBorderWidth", "BorderWidth",
	DEF_GRAPH_PLOT_BW_COLOR, Tk_Offset(Graph, plotBW),
	ALL_MASK | TK_CONFIG_COLOR_ONLY, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-plotborderwidth", "plotBorderWidth", "BorderWidth",
	DEF_GRAPH_PLOT_BW_MONO, Tk_Offset(Graph, plotBW),
	ALL_MASK | TK_CONFIG_MONO_ONLY, &bltLengthOption},
    {TK_CONFIG_RELIEF, "-plotrelief", "plotRelief", "Relief",
	DEF_GRAPH_PLOT_RELIEF, Tk_Offset(Graph, plotRelief),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_GRAPH_RELIEF, Tk_Offset(Graph, relief),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-rightmargin", "rightMargin", "Margin",
	DEF_GRAPH_MARGIN, Tk_Offset(Graph, reqRightMargin),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK, &bltLengthOption},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_GRAPH_TAKE_FOCUS, Tk_Offset(Graph, takeFocus),
	ALL_MASK | TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-tile", "tile", "Tile",
	(char *)NULL, Tk_Offset(Graph, tile),
	TK_CONFIG_NULL_OK | ALL_MASK, &bltTileOption},
    {TK_CONFIG_STRING, "-title", "title", "Title",
	DEF_GRAPH_TITLE, Tk_Offset(Graph, title),
	ALL_MASK | TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-topmargin", "topMargin", "Margin",
	DEF_GRAPH_MARGIN, Tk_Offset(Graph, reqTopMargin),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-width", "width", "Width",
	DEF_GRAPH_WIDTH, Tk_Offset(Graph, reqWidth),
	ALL_MASK, &bltLengthOption},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

double bltPosInfinity = (double)DBL_MAX;
double bltNegInfinity = (double)(-(DBL_MAX));

extern int Blt_CreateAxis _ANSI_ARGS_((Graph *graphPtr, AxisType type, int));
extern void Blt_UpdateAxisBackgrounds _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_LayoutGraph _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_CreateLegend _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_CreatePostScript _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_CreateCrosshairs _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_CreateGrid _ANSI_ARGS_((Graph *graphPtr));
static void DisplayGraph _ANSI_ARGS_((ClientData clientData));
static void DestroyGraph _ANSI_ARGS_((FreeProcData clientData));
static int GraphWidgetCmd _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int argc, char **argv));
extern int Blt_AxisOper _ANSI_ARGS_((GraphAxis *axisPtr, int argc, char **argv));
extern int Blt_LegendOper _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));
extern int Blt_CrosshairsOper _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));
extern int Blt_PostScriptOper _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));

extern int Blt_MarkerOper _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));
extern void Blt_DrawMarkers _ANSI_ARGS_((Graph *graphPtr, int under));
extern void Blt_GetMarkerCoordinates _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DestroyMarkers _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_GridOper _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));

extern int Blt_ElementOper _ANSI_ARGS_((Graph *graphPtr, Tcl_Interp *interp,
	int argc, char **argv));
extern void Blt_DrawElements _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DrawActiveElements _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_GetElementCoordinates _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_DestroyElements _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_GraphUpdateNeeded _ANSI_ARGS_((Graph *graphPtr));

static void TileChangedProc _ANSI_ARGS_((ClientData clientData, Blt_Tile tile,
	GC *gcPtr));

/*
 *--------------------------------------------------------------
 *
 * Blt_RedrawGraph --
 *
 *	Tell the Tk dispatcher to call the graph display routine at
 *	the next idle point.  This request is made only if the window
 *	is mapped and no other redraw request is pending.
 *
 * Results: None.
 *
 * Side effects:
 *	The window is eventually redisplayed.
 *
 *--------------------------------------------------------------
 */
void
Blt_RedrawGraph(graphPtr)
    Graph *graphPtr;		/* Graph widget record */
{
    if (Tk_IsMapped(graphPtr->tkwin) && !(graphPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(DisplayGraph, (ClientData)graphPtr);
	graphPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *--------------------------------------------------------------
 *
 * GraphEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on graphs.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, the graph is eventually
 *	redisplayed.
 *
 *--------------------------------------------------------------
 */
static void
GraphEventProc(clientData, eventPtr)
    ClientData clientData;	/* Graph widget record */
    register XEvent *eventPtr;	/* Event which triggered call to routine */
{
    Graph *graphPtr = (Graph *)clientData;

    if (eventPtr->type == Expose) {
	if (eventPtr->xexpose.count == 0) {
	    graphPtr->flags |= REDRAW_WORLD;
	    Blt_RedrawGraph(graphPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (graphPtr->tkwin != NULL) {
	    char *cmdName;

	    cmdName = Tcl_GetCommandName(graphPtr->interp, graphPtr->cmdToken);
#ifdef USE_TK_NAMESPACES
	    Itk_SetWidgetCommand(graphPtr->tkwin, (Tcl_Command) NULL);
#endif /* USE_TK_NAMESPACES */
	    graphPtr->tkwin = NULL;
	    Tcl_DeleteCommand(graphPtr->interp, cmdName);
	}
	if (graphPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayGraph, (ClientData)graphPtr);
	}
	Tcl_EventuallyFree((ClientData)graphPtr, (Tcl_FreeProc *)DestroyGraph);
    } else if (eventPtr->type == ConfigureNotify) {
	graphPtr->flags |= (COORDS_WORLD | REDRAW_WORLD);
	Blt_RedrawGraph(graphPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GraphDeleteCmdProc --
 *
 *	This procedure is invoked when a widget command is deleted.  If
 *	the widget isn't already in the process of being destroyed,
 *	this command destroys it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */
static void
GraphDeleteCmdProc(clientData)
    ClientData clientData;	/* Pointer to widget record. */
{
    Graph *graphPtr = (Graph *)clientData;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */
    if (graphPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = graphPtr->tkwin;
	graphPtr->tkwin = NULL;
#ifdef USE_TK_NAMESPACES
	Itk_SetWidgetCommand(tkwin, (Tcl_Command) NULL);
#endif /* USE_TK_NAMESPACES */
	Tk_DestroyWindow(tkwin);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * TileChangedProc
 *
 *	Rebuilds the designated GC with the new tile pixmap.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
TileChangedProc(clientData, tile, gcPtr)
    ClientData clientData;
    Blt_Tile tile;
    GC *gcPtr;
{
    Graph *graphPtr = (Graph *)clientData;

    if (graphPtr->tkwin != NULL) {
	unsigned long gcMask;
	XGCValues gcValues;
	Pixmap pixmap;
	GC newGC;

	gcMask = (GCForeground | GCBackground);
	gcValues.foreground = Tk_3DBorderColor(graphPtr->border)->pixel;
	gcValues.background = graphPtr->marginFg->pixel;
	pixmap = Blt_PixmapOfTile(tile);
	if (pixmap != None) {
	    gcMask |= (GCTile | GCFillStyle);
	    gcValues.fill_style = FillTiled;
	    gcValues.tile = pixmap;
	}
	newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
	if (*gcPtr != NULL) {
	    Tk_FreeGC(graphPtr->display, *gcPtr);
	}
	*gcPtr = newGC;
	graphPtr->flags |= REDRAW_WORLD;
	Blt_RedrawGraph(graphPtr);
    }
}


/*
 *--------------------------------------------------------------
 *
 * AdjustAxisPointers --
 *
 *	Sets the axis pointers according to whether the axis is
 *	inverted on not.  The axis sites are also reset.
 *
 * Results:
 *	None.
 *
 *--------------------------------------------------------------
 */
static void
AdjustAxisPointers(graphPtr)
    Graph *graphPtr;		/* Graph widget record */
{
    if (graphPtr->inverted) {
	graphPtr->bottomAxis = graphPtr->axisY1;
	graphPtr->leftAxis = graphPtr->axisX1;
	graphPtr->topAxis = graphPtr->axisY2;
	graphPtr->rightAxis = graphPtr->axisX2;
    } else {
	graphPtr->bottomAxis = graphPtr->axisX1;
	graphPtr->leftAxis = graphPtr->axisY1;
	graphPtr->topAxis = graphPtr->axisX2;
	graphPtr->rightAxis = graphPtr->axisY2;
    }
    graphPtr->bottomAxis->site = AXIS_SITE_BOTTOM;
    graphPtr->leftAxis->site = AXIS_SITE_LEFT;
    graphPtr->topAxis->site = AXIS_SITE_TOP;
    graphPtr->rightAxis->site = AXIS_SITE_RIGHT;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureGraph --
 *
 *	Allocates resources for the graph.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for graphPtr;  old resources get freed, if there
 *	were any.  The graph is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static void
ConfigureGraph(graphPtr)
    Graph *graphPtr;		/* Graph widget record */
{
    XColor *colorPtr;
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;

    /* Don't allow negative bar widths. Reset to an arbitrary value (0.1) */
    if (graphPtr->barWidth <= 0.0) {
	graphPtr->barWidth = 0.1;
    }
    if ((graphPtr->reqHeight != Tk_ReqHeight(graphPtr->tkwin)) ||
	(graphPtr->reqWidth != Tk_ReqWidth(graphPtr->tkwin))) {
	Tk_GeometryRequest(graphPtr->tkwin, graphPtr->reqWidth,
	    graphPtr->reqHeight);
    }
    Tk_SetInternalBorder(graphPtr->tkwin, graphPtr->borderWidth);
    colorPtr = Tk_3DBorderColor(graphPtr->border);

    /* Update background color for axis text GCs */
    if (Blt_ConfigModified(configSpecs, "-background", (char *)NULL)) {
	Blt_UpdateAxisBackgrounds(graphPtr);
    }
    if (graphPtr->title != NULL) {
	Blt_GetTextExtents(graphPtr->fontPtr, graphPtr->title,
	    &(graphPtr->titleWidth), &(graphPtr->titleHeight));
	graphPtr->titleHeight += 10;
    } else {
	graphPtr->titleWidth = graphPtr->titleHeight = 0;
    }
    /*
     * Create GCs for interior and exterior regions, and a background
     * GC for clearing the margins with XFillRectangle
     */

    /* Margin GC */

    gcValues.foreground = graphPtr->marginFg->pixel;
    gcValues.background = colorPtr->pixel;
    gcMask = (GCForeground | GCBackground);
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (graphPtr->drawGC != NULL) {
	Tk_FreeGC(graphPtr->display, graphPtr->drawGC);
    }
    graphPtr->drawGC = newGC;

    /* Title GC */

    gcValues.font = graphPtr->fontPtr->fid;
    gcMask = (GCForeground | GCBackground | GCFont);
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (graphPtr->titleGC != NULL) {
	Tk_FreeGC(graphPtr->display, graphPtr->titleGC);
    }
    graphPtr->titleGC = newGC;


    /* Plot fill GC (Background = Foreground) */

    gcValues.foreground = graphPtr->plotBg->pixel;
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (graphPtr->plotFillGC != NULL) {
	Tk_FreeGC(graphPtr->display, graphPtr->plotFillGC);
    }
    graphPtr->plotFillGC = newGC;

    /* Margin fill GC (Background = Foreground) */

    gcValues.foreground = colorPtr->pixel;
    gcValues.background = graphPtr->marginFg->pixel;
    if (graphPtr->tile != NULL) {
	Pixmap pixmap;

	Blt_SetTileChangedProc(graphPtr->tile, TileChangedProc,
	    (ClientData)graphPtr, &(graphPtr->fillGC));

	pixmap = Blt_PixmapOfTile(graphPtr->tile);
	if (pixmap != None) {
	    gcMask |= GCTile | GCFillStyle;
	    gcValues.fill_style = FillTiled;
	    gcValues.tile = pixmap;
	}
    }
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (graphPtr->fillGC != NULL) {
	Tk_FreeGC(graphPtr->display, graphPtr->fillGC);
    }
    graphPtr->fillGC = newGC;

    /*
     * If the -inverted option changed, we need to readjust the pointers
     * to the axes and recompute the their scales.
     */
    if (Blt_ConfigModified(configSpecs, "-invertxy", (char *)NULL)) {
	AdjustAxisPointers(graphPtr);
	graphPtr->flags |= RESET_AXES;
    }
    /*
     * Free the pixmap if we're not buffering the display of elements
     * anymore.
     */
    if ((!graphPtr->buffered) && (graphPtr->elemCache != None)) {
	Tk_FreePixmap(graphPtr->display, graphPtr->elemCache);
	graphPtr->elemCache = None;
    }
    /*
     * Reconfigure the crosshairs in case the plotting area's
     * background color changed
     */
    (*graphPtr->crosshairs->configProc) (graphPtr);

    /*
     *  Update the layout of the graph (and redraw the elements) if
     *  any of the following graph options which affect the size of
     *	the plotting area has changed.
     *
     *      -borderwidth, -plotborderwidth
     *	    -font, -title
     *	    -width, -height
     *	    -invertxy
     *	    -bottommargin, -leftmargin, -rightmargin, -topmargin
     *	    -barmode, -barwidth
     *      -histmode
     */
    if (Blt_ConfigModified(configSpecs, "-invertxy", "-title", "-font",
	    "-*margin", "-*width", "-height", "-barmode", "-plotbackground",
		"-histmode", (char *)NULL)) {
	graphPtr->flags |= SET_ALL_FLAGS;
    }
    graphPtr->flags |= REDRAW_WORLD;
    Blt_RedrawGraph(graphPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyGraph --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a graph at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 *----------------------------------------------------------------------
 */
static void
DestroyGraph(clientData)
    FreeProcData clientData;
{
    Graph *graphPtr = (Graph *)clientData;
    register int i;
    GraphAxis *axisPtr;

    /*
     * Destroy the individual components of the graph: elements, markers,
     * X and Y axes, legend, display lists etc.
     */
    Blt_DestroyMarkers(graphPtr);
    Blt_DestroyElements(graphPtr);

    for (i = 0; i < 4; i++) {
	axisPtr = graphPtr->axisArr[i];
	(*axisPtr->destroyProc) (axisPtr);
    }
    (*graphPtr->legendPtr->destroyProc) (graphPtr);
    (*graphPtr->postscript->destroyProc) (graphPtr);
    (*graphPtr->crosshairs->destroyProc) (graphPtr);
    (*graphPtr->gridPtr->destroyProc) (graphPtr);

    /* Release allocated X resources and memory. */
    if (graphPtr->drawGC != NULL) {
	Tk_FreeGC(graphPtr->display, graphPtr->drawGC);
    }
    if (graphPtr->fillGC != NULL) {
	Tk_FreeGC(graphPtr->display, graphPtr->fillGC);
    }
    if (graphPtr->elemCache != None) {
	Tk_FreePixmap(graphPtr->display, graphPtr->elemCache);
    }
    if (graphPtr->freqArr != NULL) {
	ckfree((char *)graphPtr->freqArr);
    }
    if (graphPtr->numStacks > 0) {
	Tcl_DeleteHashTable(&(graphPtr->freqTable));
    }
    if (graphPtr->tile != NULL) {
	Blt_FreeTile(graphPtr->tile);
    }
    Tk_FreeOptions(configSpecs, (char *)graphPtr, graphPtr->display,
	(GRAPH_TYPE_BIT << (int)graphPtr->defElemType));
    ckfree((char *)graphPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CreateGraph --
 *
 *	This procedure creates and initializes a new widget.
 *
 * Results:
 *	The return value is a pointer to a structure describing
 *	the new widget.  If an error occurred, then the return
 *	value is NULL and an error message is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated, a Tk_Window is created, etc.
 *
 *----------------------------------------------------------------------
 */

static Graph *
CreateGraph(interp, parent, argc, argv, type)
    Tcl_Interp *interp;
    Tk_Window parent;
    int argc;
    char **argv;
    ElementType type;
{
    Graph *graphPtr;
    Tk_Window tkwin;
    int flags;
    register int i;

    tkwin = Tk_CreateWindowFromPath(interp, parent, argv[1], (char *)NULL);
    if (tkwin == (Tk_Window)NULL) {
	return (Graph *) NULL;
    }
    graphPtr = (Graph *)ckcalloc(1, sizeof(Graph));
    if (graphPtr == (Graph *)NULL) {
	Panic("can't allocate graph structure");
    }
    Tk_SetClass(tkwin, classNames[type]);

    /* Initialize the data structure for the graph. */

    graphPtr->tkwin = tkwin;
    graphPtr->pathName = Tk_PathName(tkwin);
    graphPtr->display = Tk_Display(tkwin);
    graphPtr->interp = interp;
    graphPtr->defElemType = type;
    graphPtr->buffered = 1;
    graphPtr->plotRelief = TK_RELIEF_SUNKEN;
    graphPtr->relief = TK_RELIEF_FLAT;
    graphPtr->justify = TK_JUSTIFY_CENTER;
    graphPtr->flags = (SET_ALL_FLAGS);

    Tcl_InitHashTable(&(graphPtr->elemTable), TCL_STRING_KEYS);
    Blt_InitList(&(graphPtr->elemList), TCL_STRING_KEYS);
    Tcl_InitHashTable(&(graphPtr->markerTable), TCL_STRING_KEYS);
    Blt_InitList(&(graphPtr->markerList), TCL_ONE_WORD_KEYS);
    graphPtr->nextMarkerId = 1;

    flags = (GRAPH_TYPE_BIT << (int)type);
    if (Tk_ConfigureWidget(interp, tkwin, configSpecs, argc - 2, argv + 2,
	    (char *)graphPtr, flags) != TCL_OK) {
	goto error;
    }
    /*
     * To create rotated text and bitmaps (axes and markers) we may need
     * private GCs, so create the window now.
     */
    for (i = 0; i < 4; i++) {
	if (Blt_CreateAxis(graphPtr, (AxisType)i, flags) != TCL_OK) {
	    goto error;
	}
    }
    AdjustAxisPointers(graphPtr);

    if (Blt_CreatePostScript(graphPtr) != TCL_OK) {
	goto error;
    }
    if (Blt_CreateCrosshairs(graphPtr) != TCL_OK) {
	goto error;
    }
    if (Blt_CreateLegend(graphPtr) != TCL_OK) {
	goto error;
    }
    if (Blt_CreateGrid(graphPtr) != TCL_OK) {
	goto error;
    }
    /* Compute the initial axis ticks and labels */

    Tk_CreateEventHandler(graphPtr->tkwin, ExposureMask | StructureNotifyMask,
	(Tk_EventProc *)GraphEventProc, (ClientData)graphPtr);

    graphPtr->cmdToken = Tcl_CreateCommand(interp, argv[1],
	(Tcl_CmdProc*)GraphWidgetCmd,
	(ClientData)graphPtr, (Tcl_CmdDeleteProc*)GraphDeleteCmdProc);
#ifdef USE_TK_NAMESPACES
    Itk_SetWidgetCommand(graphPtr->tkwin, graphPtr->cmdToken);
#endif /* USE_TK_NAMESPACES */

    ConfigureGraph(graphPtr);
    return (graphPtr);

  error:
    if (tkwin != (Tk_Window)NULL) {
	Tk_DestroyWindow(tkwin);
    }
    return (Graph *) NULL;
}

/* Widget sub-commands */

/*ARGSUSED*/
static int
XAxisOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;		/* Not used */
    int argc;
    char **argv;
{
    return (Blt_AxisOper(graphPtr->axisX1, argc, argv));
}

/*ARGSUSED*/
static int
X2AxisOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;		/* Not used */
    int argc;
    char **argv;
{
    return (Blt_AxisOper(graphPtr->axisX2, argc, argv));
}

/*ARGSUSED*/
static int
YAxisOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;		/* Not used */
    int argc;
    char **argv;
{
    return (Blt_AxisOper(graphPtr->axisY1, argc, argv));
}

/*ARGSUSED*/
static int
Y2AxisOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;		/* Not used */
    int argc;
    char **argv;
{
    return (Blt_AxisOper(graphPtr->axisY2, argc, argv));
}

static int
ConfigureOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int flags;

    flags = ((GRAPH_TYPE_BIT << graphPtr->defElemType) | TK_CONFIG_ARGV_ONLY);
    if (argc == 2) {
	return (Tk_ConfigureInfo(interp, graphPtr->tkwin, configSpecs,
		(char *)graphPtr, (char *)NULL, flags));
    } else if (argc == 3) {
	return (Tk_ConfigureInfo(interp, graphPtr->tkwin,
		configSpecs, (char *)graphPtr, argv[2], flags));
    } else {
	if (Tk_ConfigureWidget(interp, graphPtr->tkwin, configSpecs,
		argc - 2, argv + 2, (char *)graphPtr, flags) != TCL_OK) {
	    return TCL_ERROR;
	}
	ConfigureGraph(graphPtr);
	return TCL_OK;
    }
}

/* ARGSUSED*/
static int
CgetOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    return (Tk_ConfigureValue(interp, graphPtr->tkwin, configSpecs,
	(char *)graphPtr, argv[2], GRAPH_TYPE_BIT << graphPtr->defElemType));
}

/*
 *--------------------------------------------------------------
 *
 * ExtentsOper --
 *
 *	Reports the size of one of several items within the graph.
 *	The following are valid items:
 *
 *        "plotwidth"		Width of the plot area
 *	  "plotheight"		Height of the plot area
 *	  "leftmargin"		Width of the left margin
 *	  "rightmargin"		Width of the right margin
 *	  "topmargin"		Height of the top margin
 *	  "bottommargin"	Height of the bottom margin
 *
 * Results:
 *	Always returns TCL_OK.
 *
 *--------------------------------------------------------------
 */
/* ARGSUSED*/
static int
ExtentsOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    char c;
    int length;
    int value;

    c = argv[2][0];
    length = strlen(argv[2]);
    value = 0;
    if ((c == 'p') && (length > 4) &&
	(strncmp("plotheight", argv[2], length) == 0)) {
	value = graphPtr->plotArea.height;
    } else if ((c == 'p') && (length > 4) &&
	(strncmp("plotwidth", argv[2], length) == 0)) {
	value = graphPtr->plotArea.width;
    } else if ((c == 'l') && (length > 4) &&
	(strncmp("leftmargin", argv[2], length) == 0)) {
	value = graphPtr->leftMargin;
    } else if ((c == 'r') && (length > 4) &&
	(strncmp("rightmargin", argv[2], length) == 0)) {
	value = graphPtr->rightMargin;
    } else if ((c == 't') && (length > 4) &&
	(strncmp("topmargin", argv[2], length) == 0)) {
	value = graphPtr->topMargin;
    } else if ((c == 'b') && (length > 4) &&
	(strncmp("bottommargin", argv[2], length) == 0)) {
	value = graphPtr->bottomMargin;
    } else {
	Tcl_AppendResult(interp, "bad extent item \"", argv[2],
	    "\": should be \"plotheight\", \"plotwidth\", \"leftmargin\", ",
	    "\"rightmargin\", \"topmargin\", or \"bottommargin\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    Blt_AppendInt(interp, value);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * InvtransformOper --
 *
 *	This procedure returns a list of the graph coordinate
 *	values corresponding with the given window X and Y
 *	coordinate positions.
 *
 * Results:
 *	Returns a standard Tcl result.  If an error occurred while
 *	parsing the window positions, TCL_ERROR is returned, and
 *	interp->result will contain the error message.  Otherwise
 *	interp->result will contain a Tcl list of the x and y
 *	coordinates.
 *
 *--------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InvtransformOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget record */
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    double x, y;
    Coordinate coord;
    AxisPair axisPair;

    if (Tcl_ExprDouble(interp, argv[2], &x) != TCL_OK ||
	Tcl_ExprDouble(interp, argv[3], &y) != TCL_OK) {
	return TCL_ERROR;
    }
    if (graphPtr->flags & RESET_AXES) {
	Blt_ComputeAxes(graphPtr);
    }
    /*
     * Perform the reverse transformation, converting from window
     * coordinates to graph data coordinates.  Note that the point
     * is always mapped to the X1 and Y1 axes (which may not be
     * what the user wants).
     */
    Blt_GetAxisMapping(graphPtr, DEF_AXIS_MASK, &axisPair);
    coord = Blt_InvTransformPt(graphPtr, x, y, &axisPair);
    Tcl_ResetResult(interp);
    Blt_AppendDouble(interp, coord.x);
    Blt_AppendDouble(interp, coord.y);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TransformOper --
 *
 *	This procedure returns a list of the window coordinates
 *	corresponding with the given graph x and y coordinates.
 *
 * Results:
 *	Returns a standard Tcl result.  interp->result contains
 *	the list of the graph coordinates. If an error occurred
 *	while parsing the window positions, TCL_ERROR is returned,
 *	then interp->result will contain an error message.
 *
 *--------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TransformOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget record */
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    double x, y;
    Coordinate coord;
    AxisPair axisPair;

    if (Blt_GetCoordinate(interp, argv[2], &x) != TCL_OK ||
	Blt_GetCoordinate(interp, argv[3], &y) != TCL_OK) {
	return TCL_ERROR;
    }
    if (graphPtr->flags & RESET_AXES) {
	Blt_ComputeAxes(graphPtr);
    }
    /*
     * Perform the transformation from window to graph coordinates.
     * Note that the points are always mapped onto the X1 and Y1 axes
     * (which may not be the what the user wants).
     */
    Blt_GetAxisMapping(graphPtr, DEF_AXIS_MASK, &axisPair);
    coord = Blt_TransformPt(graphPtr, x, y, &axisPair);
    Tcl_ResetResult(interp);
    Blt_AppendInt(interp, BLT_RND(coord.x));
    Blt_AppendInt(interp, BLT_RND(coord.y));
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * GraphWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
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
static Blt_OperSpec operSpecs[] =
{
    {"cget", 2, (Blt_OperProc) CgetOper, 3, 3, "option",},
    {"configure", 2, (Blt_OperProc) ConfigureOper, 2, 0, "?args?",},
    {"crosshairs", 2, (Blt_OperProc) Blt_CrosshairsOper, 2, 0, "oper ?args?",},
    {"element", 2, (Blt_OperProc) Blt_ElementOper, 2, 0, "oper ?args?",},
    {"extents", 2, (Blt_OperProc) ExtentsOper, 3, 3, "item",},
    {"grid", 1, (Blt_OperProc) Blt_GridOper, 2, 0, "oper ?args?",},
    {"invtransform", 1, (Blt_OperProc) InvtransformOper, 4, 4, "winX winY",},
    {"legend", 1, (Blt_OperProc) Blt_LegendOper, 2, 0, "oper ?args?",},
    {"marker", 1, (Blt_OperProc) Blt_MarkerOper, 2, 0, "oper ?args?",},
    {"postscript", 1, (Blt_OperProc) Blt_PostScriptOper, 3, 0, "oper ?args?",},
    {"transform", 1, (Blt_OperProc) TransformOper, 4, 4, "x y",},
    {"x2axis", 2, (Blt_OperProc) X2AxisOper, 2, 0, "oper ?args?",},
    {"xaxis", 2, (Blt_OperProc) XAxisOper, 2, 0, "oper ?args?",},
    {"y2axis", 2, (Blt_OperProc) Y2AxisOper, 2, 0, "oper ?args?",},
    {"yaxis", 2, (Blt_OperProc) YAxisOper, 2, 0, "oper ?args?",},
};
static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

static int
GraphWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_OperProc proc;
    int result;
    Graph *graphPtr = (Graph *)clientData;

    proc = Blt_LookupOperation(interp, numSpecs, operSpecs, BLT_OPER_ARG1,
	argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData)graphPtr);
    result = (*proc) (graphPtr, interp, argc, argv);
    Tcl_Release((ClientData)graphPtr);
    return (result);
}

/*
 *--------------------------------------------------------------
 *
 * GraphCmd --
 *
 *	Creates a new window and Tcl command representing an
 *	instance of a graph widget.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */
static int
GraphCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Tk_Window tkwin = (Tk_Window)clientData;
    Graph *graphPtr;
    ElementType defType;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " pathName ?option value?...\"", (char *)NULL);
	return TCL_ERROR;
    }
    defType = ELEM_LINE;
    graphPtr = CreateGraph(interp, tkwin, argc, argv, defType);
    if (graphPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, graphPtr->pathName, TCL_STATIC);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * BarchartCmd --
 *
 *	Creates a new window and Tcl command representing an
 *	instance of a barchart widget.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BarchartCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Tk_Window tkwin = (Tk_Window)clientData;
    Graph *graphPtr;
    ElementType defType;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " pathName ?option value?...\"", (char *)NULL);
	return TCL_ERROR;
    }
    defType = ELEM_BAR;
    graphPtr = CreateGraph(interp, tkwin, argc, argv, defType);
    if (graphPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, graphPtr->pathName, TCL_STATIC);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_GetGraphCoordinates --
 *
 *	Based upon the computes sizing of the plotting area, calls
 *	routines to calculate the positions for the graph components.
 *	This routine is called when the layout changes.  It is called
 *	from DisplayGraph and PrintPostScript.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Old storage is freed and new memory is allocated for holding
 *	component positions.  These positions will eventually affect
 *	where the components are drawn in the graph window.
 *
 * -----------------------------------------------------------------
 */
void
Blt_GetGraphCoordinates(graphPtr)
    Graph *graphPtr;
{
    if (graphPtr->flags & COORDS_WORLD) {
	GraphAxis **axisPtrPtr;
	register int i;

	axisPtrPtr = graphPtr->axisArr;
	for (i = 0; i < 4; i++) {
	    if ((*axisPtrPtr)->mapped) {
		(*(*axisPtrPtr)->coordsProc) (*axisPtrPtr);
	    }
	    axisPtrPtr++;
	}
    }
    Blt_GetElementCoordinates(graphPtr);
    Blt_GetMarkerCoordinates(graphPtr);

    graphPtr->flags &= ~(COORDS_ALL_PARTS);
}

/*
 * -----------------------------------------------------------------------
 *
 * DisplayExterior --
 *
 * 	Draws the exterior region of the graph (axes, ticks, titles, etc)
 *	onto a pixmap. The interior region is defined by the given
 *	rectangle structure.
 *
 *		X coordinate axis
 *		Y coordinate axis
 *		legend
 *		interior border
 *		exterior border
 *		titles (X and Y axis, graph)
 *
 * Returns:
 *	None.
 *
 * Side Effects:
 *	Exterior of graph is displayed in its window.
 *
 * -----------------------------------------------------------------------
 */
static void
DisplayExterior(graphPtr)
    Graph *graphPtr;
{
    XRectangle rectArr[4];
    GraphAxis *axisPtr;
    TextAttributes textAttr;
    register int i;
    LegendSite site;

    /*
     * Draw the four outer rectangles which encompass the plotting
     * surface. This clears the surrounding area and clips the plot.
     */
    rectArr[0].x = rectArr[0].y = rectArr[3].x = rectArr[1].x = 0;
    rectArr[0].width = rectArr[3].width = graphPtr->width;
    rectArr[0].height = graphPtr->topMargin;
    rectArr[3].y = graphPtr->plotArea.y + graphPtr->plotArea.height;
    rectArr[3].height = graphPtr->bottomMargin;
    rectArr[2].y = rectArr[1].y = graphPtr->plotArea.y;
    rectArr[1].width = graphPtr->leftMargin;
    rectArr[2].height = rectArr[1].height = graphPtr->plotArea.height;
    rectArr[2].x = graphPtr->plotArea.x + graphPtr->plotArea.width;
    rectArr[2].width = graphPtr->rightMargin;

    if (graphPtr->tile != NULL) {
	Blt_SetTileOrigin(graphPtr->tkwin, graphPtr->fillGC, 0, 0);
    }

    XFillRectangles(graphPtr->display, graphPtr->pixwin, graphPtr->fillGC,
	rectArr, 4);

    /* Draw 3D border around the plotting area */

    if (graphPtr->plotBW > 0) {
	int x, y, width, height;

	x = graphPtr->plotArea.x - graphPtr->plotBW;
	y = graphPtr->plotArea.y - graphPtr->plotBW;
	width = graphPtr->plotArea.width + (2 * graphPtr->plotBW);
	height = graphPtr->plotArea.height + (2 * graphPtr->plotBW);
	Tk_Draw3DRectangle(graphPtr->tkwin, graphPtr->pixwin, graphPtr->border,
	    x, y, width, height, graphPtr->plotBW, graphPtr->plotRelief);
    }
    /* Legend is drawn on one of th graph margins */
    site = Blt_LegendSite(graphPtr);
    if (site < LEGEND_SITE_PLOT) {
	(*graphPtr->legendPtr->drawProc) (graphPtr);
    }
    /* Initialize text attributes for graph and axis titles */

    Blt_InitTextAttrs(&textAttr, graphPtr->marginFg,
	Tk_3DBorderColor(graphPtr->border), graphPtr->fontPtr, 0.0,
	TK_ANCHOR_CENTER, graphPtr->justify);

    textAttr.fillGC = graphPtr->fillGC;
    if (graphPtr->title != NULL) {
	textAttr.textGC = graphPtr->titleGC;
	Blt_DrawText(graphPtr->tkwin, graphPtr->pixwin, graphPtr->title,
	    &textAttr, graphPtr->titleX, graphPtr->titleY);
    }

    if (graphPtr->tile != NULL) {
	textAttr.bgColorPtr = NULL;
    }

    /* Draw axes */
    for (i = 0; i < 4; i++) {
	axisPtr = graphPtr->axisArr[i];
	if (axisPtr->mapped) {
	    (*axisPtr->drawProc) (axisPtr, &textAttr);
	}
    }
    /* Exterior 3D border */
    if ((graphPtr->relief != TK_RELIEF_FLAT) &&
	(graphPtr->borderWidth > 0)) {
	Tk_Draw3DRectangle(graphPtr->tkwin, graphPtr->pixwin,
	    graphPtr->border, 0, 0, graphPtr->width, graphPtr->height,
	    graphPtr->borderWidth, graphPtr->relief);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayGraph --
 *
 *	This procedure is invoked to display a graph widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the graph in its
 *	current mode.
 *
 *----------------------------------------------------------------------
 */
static void
DisplayGraph(clientData)
    ClientData clientData;
{
    Graph *graphPtr = (Graph *)clientData;
    GraphLegend *legendPtr;
    LegendSite site;
    int x, y, width, height;

#ifdef notdef
    fprintf(stderr, "Calling DisplayGraph\n");
#endif
    graphPtr->flags &= ~REDRAW_PENDING;
    if (graphPtr->tkwin == NULL) {
	return;			/* Window was destroyed (should not get here)*/
    }
    if (Blt_GraphUpdateNeeded(graphPtr)) {
	/* 
	 * A data vector of one of the graph's elements has a
	 * notification pending.  This means that it will eventually
	 * notify the graph that data has changed.  But since the
	 * graph shares the actual data with the vector, we need to
	 * keep in sync with the vectors.  So wait for the
	 * notification before redrawing.  
	 */
	return;
    }
    graphPtr->width = Tk_Width(graphPtr->tkwin);
    graphPtr->height = Tk_Height(graphPtr->tkwin);

    if (graphPtr->flags & RESET_AXES) {
	Blt_ComputeAxes(graphPtr);
    }
    if (graphPtr->flags & LAYOUT_NEEDED) {
	Blt_LayoutGraph(graphPtr);
	graphPtr->flags &= ~LAYOUT_NEEDED;
    }
    Blt_GetGraphCoordinates(graphPtr);
    if (!Tk_IsMapped(graphPtr->tkwin)) {
	/*
	 * The graph's window isn't mapped, so don't bother drawing
	 * anything.  By getting this far, we've at least computed
	 * the coordinates of the graph's new layout.
	 */
	return;
    }
    /*
     * Create a pixmap the size of the window for double buffering.,
     */
    graphPtr->pixwin = Tk_GetPixmap(graphPtr->display,
	Tk_WindowId(graphPtr->tkwin), graphPtr->width, graphPtr->height,
	Tk_Depth(graphPtr->tkwin));

    legendPtr = graphPtr->legendPtr;
    site = Blt_LegendSite(graphPtr);
    if (graphPtr->buffered) {
	/*
	 * Create another pixmap to save elements if one doesn't
	 * already exist or the size of the window has changed.
	 */
	if ((graphPtr->cacheWidth != graphPtr->width) ||
	    (graphPtr->cacheHeight != graphPtr->height) ||
	    (graphPtr->elemCache == None)) {
	    graphPtr->cacheWidth = graphPtr->width;
	    graphPtr->cacheHeight = graphPtr->height;
	    if (graphPtr->elemCache != None) {
		Tk_FreePixmap(graphPtr->display, graphPtr->elemCache);
	    }
	    graphPtr->elemCache = Tk_GetPixmap(graphPtr->display,
		Tk_WindowId(graphPtr->tkwin), graphPtr->cacheWidth,
		graphPtr->cacheHeight, Tk_Depth(graphPtr->tkwin));
	    graphPtr->flags |= UPDATE_PIXMAP;
	}
	/*
	 * If the pixmap is out-of-date or needs to be initialized,
	 * clear the background and draw the elements into the pixmap.
	 */
	if (graphPtr->flags & UPDATE_PIXMAP) {
	    Pixmap save;

	    XFillRectangles(graphPtr->display, graphPtr->elemCache,
		graphPtr->plotFillGC, &(graphPtr->plotArea), 1);
	    save = graphPtr->pixwin;
	    graphPtr->pixwin = graphPtr->elemCache;
	    if (graphPtr->gridPtr->mapped) {
		(*graphPtr->gridPtr->drawProc) (graphPtr);
	    }
	    if (((site == LEGEND_SITE_XY) || (site == LEGEND_SITE_PLOT)) &&
		(!legendPtr->raised)) {
		(*legendPtr->drawProc) (graphPtr);
	    }
	    Blt_DrawMarkers(graphPtr, TRUE);
	    Blt_DrawElements(graphPtr);
	    graphPtr->pixwin = save;
	    graphPtr->flags &= ~UPDATE_PIXMAP;
	}
	/*
	 * Copy the pixmap to the one used for drawing the entire graph.
	 */
	XCopyArea(graphPtr->display, graphPtr->elemCache, graphPtr->pixwin,
	    graphPtr->drawGC, graphPtr->plotArea.x, graphPtr->plotArea.y,
	    graphPtr->plotArea.width, graphPtr->plotArea.height,
	    graphPtr->plotArea.x, graphPtr->plotArea.y);
    } else {
	XFillRectangles(graphPtr->display, graphPtr->pixwin,
	    graphPtr->plotFillGC, &(graphPtr->plotArea), 1);
	if (graphPtr->gridPtr->mapped) {
	    (*graphPtr->gridPtr->drawProc) (graphPtr);
	}
	if (((site == LEGEND_SITE_XY) || (site == LEGEND_SITE_PLOT)) &&
	    (!legendPtr->raised)) {
	    (*legendPtr->drawProc) (graphPtr);
	}
	Blt_DrawMarkers(graphPtr, TRUE); /* Draw markers below elements */
	Blt_DrawElements(graphPtr);
    }
    Blt_DrawMarkers(graphPtr, FALSE);	/* Draw markers above elements */
    Blt_DrawActiveElements(graphPtr);

    /* Disable crosshairs and update lengths before drawing to the display */
    (*graphPtr->crosshairs->toggleProc) (graphPtr);
    (*graphPtr->crosshairs->updateProc) (graphPtr);

    if (graphPtr->flags & DRAW_MARGINS) {
	DisplayExterior(graphPtr);
	x = y = 0;
	height = graphPtr->height;
	width = graphPtr->width;
    } else {
	x = graphPtr->plotArea.x;
	y = graphPtr->plotArea.y;
	width = graphPtr->plotArea.width;
	height = graphPtr->plotArea.height;
    }
    if (((site == LEGEND_SITE_XY) || (site == LEGEND_SITE_PLOT)) &&
	(legendPtr->raised)) {
	(*legendPtr->drawProc) (graphPtr);
    }
    XCopyArea(graphPtr->display, graphPtr->pixwin,
	Tk_WindowId(graphPtr->tkwin), graphPtr->drawGC, x, y,
	width, height, x, y);

    (*graphPtr->crosshairs->toggleProc) (graphPtr);

    Tk_FreePixmap(graphPtr->display, graphPtr->pixwin);
    graphPtr->pixwin = None;
    graphPtr->flags &= ~SET_ALL_FLAGS;
}

int
Blt_GraphInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpecs[2] =
    {
	{"graph", GRAPH_VERSION, (Tcl_CmdProc*)GraphCmd,},
	{"barchart", GRAPH_VERSION, (Tcl_CmdProc*)BarchartCmd,},
    };

    return (Blt_InitCmds(interp, cmdSpecs, 2));
}
#endif /* !NO_GRAPH */
