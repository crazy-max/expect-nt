/*
 * bltGraph.h --
 *
 *	Declarations needed by functions implementing BLT's "graph" widget.
 *	Graph widget created by Sani Nassif and George Howlett.
 *
 * Copyright (c) 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _BLTGRAPH_H
#define _BLTGRAPH_H

/*
 * Mask values used to selectively enable GRAPH or BARCHART entries in
 * the various configuration specs.
 */
#define GRAPH_TYPE_BIT	TK_CONFIG_USER_BIT
#define XYGRAPH_MASK	GRAPH_TYPE_BIT
#define BARCHART_MASK	(GRAPH_TYPE_BIT << 1)
#define ALL_MASK	(XYGRAPH_MASK | BARCHART_MASK)

#define PADX		2	/* Padding between labels/titles */
#define PADY    	2	/* Padding between labels */

#define TITLE_PAD	5	/* Padding between titles (axis and graph) */
#define MINIMUM_MARGIN	20	/* Minimum margin size */

/*
 * -------------------------------------------------------------------
 *
 * 	Graph component structure definitions
 *
 * -------------------------------------------------------------------
 */
typedef struct Graph Graph;
typedef struct Element Element;
typedef struct GraphAxis GraphAxis;
typedef struct GraphLegend GraphLegend;
typedef struct GraphPostScript GraphPostScript;
typedef struct GraphCrosshairs GraphCrosshairs;
typedef struct GraphGrid GraphGrid;

/*
 * -------------------------------------------------------------------
 *
 * BBox --
 *
 *	Designates the lower left and upper right corners of a 
 *	rectangle representing the bounding box of a marker.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    double llx, lly, urx, ury;
} BBox;

/*
 * -------------------------------------------------------------------
 *
 * ElementType --
 *
 *	Enumerates the different types of graph elements this program
 *	produces.  An element can be either a line or a bar.
 *
 * -------------------------------------------------------------------
 */
typedef enum {
    ELEM_LINE,			/* Element represents a line curve of
				 * data values (x,y coordinate pairs) */
    ELEM_BAR			/* Element represents one or more bars
				 * of data values */
} ElementType;

/*
 * -------------------------------------------------------------------
 *
 * FreqInfo --
 *
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    int freq;			/* Number of occurrences of x-coordinate */
    int mask;			/* Axis mask.  Indicates which x and y axis
				 * are mapped to the x-value */
    double sum;			/* Sum of the ordinates of each duplicate
				 * abscissa */

    int count;
    double lastY;
} FreqInfo;

/*
 * -------------------------------------------------------------------
 *
 * FreqKey --
 *
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    double value;		/* Duplicated abscissa */
    int mask;			/* Axis mapping of element */
    int dummy;			/* Not used. Pads FreqKey structure to
				 * an even number of words. Must be
				 * initialized to zero when conducting
				 * searches of the frequency table. */
} FreqKey;

typedef enum BarModes {
    MODE_NORMAL,		/* Bars are displayed according to their
				 * x,y coordinates. If two bars have the
				 * same abscissa, the bars will overlay
				 * each other. */
    MODE_STACKED,		/* Coordinates with the same abscissa are
				 * displayed as bar segments stacked upon
				 * each other. The order of stacking is the
				 * order of the element display list and
				 * the order of the data values. */
    MODE_ALIGNED,		/* Coordinates with the same abscissa are
				 * displayed as thinner bar segments aligned
				 * one next to the other. The order of the
				 * alignment is the order of the element
				 * display list and the order of the data
				 * values. */
} BarMode;

/*
 * -------------------------------------------------------------------
 *
 * AxisTypes --
 *
 *	Enumerated type representing the types of axes
 *
 * -------------------------------------------------------------------
 */
typedef enum {
    AXIS_X1, AXIS_Y1, AXIS_X2, AXIS_Y2
} AxisType;

#define axisX1		axisArr[AXIS_X1]
#define axisX2		axisArr[AXIS_X2]
#define axisY1		axisArr[AXIS_Y1]
#define axisY2		axisArr[AXIS_Y2]

/* AxisFlags used by elements and markers */
#define AXIS_BIT_X1	(1<<AXIS_X1)
#define AXIS_BIT_Y1	(1<<AXIS_Y1)
#define AXIS_BIT_X2	(1<<AXIS_X2)
#define AXIS_BIT_Y2	(1<<AXIS_Y2)

#define AXIS_MASK(a)	(1<<((a)->type))
#define DEF_AXIS_MASK	(AXIS_BIT_X1|AXIS_BIT_Y1)
#define AXIS_MASK_X	(AXIS_BIT_X1|AXIS_BIT_X2)
#define AXIS_MASK_Y	(AXIS_BIT_Y1|AXIS_BIT_Y2)

typedef enum {
    AXIS_SITE_BOTTOM,		/* Axis is drawn in the bottom margin */
    AXIS_SITE_LEFT,		/* Axis is drawn in the left margin */
    AXIS_SITE_TOP,		/* Axis is drawn in the top margin */
    AXIS_SITE_RIGHT		/* Axis is drawn in the right margin */
} AxisSite;

/*
 * -------------------------------------------------------------------
 *
 * GraphAxis --
 *
 * 	Structure contains options controlling how the axis will be
 * 	displayed.
 *
 * -------------------------------------------------------------------
 */
typedef void (AxisDrawProc) _ANSI_ARGS_((GraphAxis *axisPtr,
	TextAttributes *attrPtr));
typedef void (AxisPrintProc) _ANSI_ARGS_((GraphAxis *axisPtr,
	TextAttributes *attrPtr));
typedef void (AxisCoordsProc) _ANSI_ARGS_((GraphAxis *axisPtr));
typedef void (AxisDestroyProc) _ANSI_ARGS_((GraphAxis *axisPtr));

struct GraphAxis {
    AxisType type;
    AxisSite site;		/* Site of the axis: right, left, etc. */
    int logScale;		/* If non-zero, scale is logarithmic */
    int mapped;			/* If non-zero, display the axis */
    double min, max;		/* Actual (includes padding) axis limits */
    int width, height;		/* Bounding box of axis */

    AxisDrawProc *drawProc;
    AxisPrintProc *printProc;
    AxisCoordsProc *coordsProc;
    AxisDestroyProc *destroyProc;
};

/*
 * -------------------------------------------------------------------
 *
 * AxisPair --
 *
 *	The pair of axes representing an element or marker.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    struct GraphAxis *x;
    struct GraphAxis *y;
} AxisPair;

typedef enum LegendSites {
    LEGEND_SITE_BOTTOM,		/* Legend is drawn in the bottom margin */
    LEGEND_SITE_LEFT,		/* Legend is drawn in the left margin */
    LEGEND_SITE_RIGHT,		/* Legend is drawn in the right margin */
    LEGEND_SITE_TOP,		/* Legend is drawn in the top margin, below
				 * the title of the graph */
    LEGEND_SITE_PLOT,		/* Legend is drawn in the plot area */
    LEGEND_SITE_XY,		/* Legend is drawn at a specified x,y
				 * window coordinate */
} LegendSite;

typedef struct {
    LegendSite site;
    int x, y;
} LegendPosition;

/*
 * -------------------------------------------------------------------
 *
 * GraphLegend --
 *
 * 	Contains information specific to how the legend will be
 *	displayed.
 *
 * -------------------------------------------------------------------
 */
typedef void (LegendDrawProc) _ANSI_ARGS_((Graph *graphPtr));
typedef void (LegendPrintProc) _ANSI_ARGS_((Graph *graphPtr));
typedef void (LegendDestroyProc) _ANSI_ARGS_((Graph *graphPtr));
typedef void (LegendGeometryProc) _ANSI_ARGS_((Graph *graphPtr, int width,
	int height));

struct GraphLegend {
    int mapped;			/* Requested state of the legend, If
				 * non-zero, legend is displayed */
    int width, height;		/* Dimensions of the legend */
    LegendPosition anchorPos;	/* Window coordinates of legend
				 * positioning point. Used in conjunction
				 * with the anchor to determine the
				 * location of the legend. */
    int raised;			/* Draw the legend last */
    LegendDrawProc *drawProc;
    LegendPrintProc *printProc;
    LegendDestroyProc *destroyProc;
    LegendGeometryProc *geomProc;
};

/*
 * -------------------------------------------------------------------
 *
 * GraphPostScript --
 *
 * 	Structure contains information specific to the outputting of
 *	PostScript commands to print the graph.
 *
 * -------------------------------------------------------------------
 */
typedef void (PostScriptDestroyProc) _ANSI_ARGS_((Graph *graphPtr));

struct GraphPostScript {
    int decorations;		/* If non-zero, print graph with
				 * color background and 3D borders */
    PostScriptDestroyProc *destroyProc;
};

/*
 * -------------------------------------------------------------------
 *
 * GraphCrosshairs --
 *
 *	Contains the line segments positions and graphics context used
 *	to simulate crosshairs (by XORing) on the graph.
 *
 * -------------------------------------------------------------------
 */
typedef void (CrosshairsToggleProc) _ANSI_ARGS_((Graph *graphPtr));
typedef void (CrosshairsUpdateProc) _ANSI_ARGS_((Graph *graphPtr));
typedef void (CrosshairsConfigProc) _ANSI_ARGS_((Graph *graphPtr));
typedef void (CrosshairsDestroyProc) _ANSI_ARGS_((Graph *graphPtr));

struct GraphCrosshairs {
    CrosshairsToggleProc *toggleProc;	/* Toggle visibility of crosshairs */
    CrosshairsUpdateProc *updateProc;	/* Update lengths of hairs */
    CrosshairsConfigProc *configProc;	/* Configure GC */
    CrosshairsDestroyProc *destroyProc;	/* Release X resources */
};

/*
 * -------------------------------------------------------------------
 *
 * GraphGrid --
 *
 * -------------------------------------------------------------------
 */
typedef void (GridDrawProc) _ANSI_ARGS_((Graph *graphPtr));
typedef void (GridPrintProc) _ANSI_ARGS_((Graph *graphPtr));
typedef void (GridDestroyProc) _ANSI_ARGS_((Graph *graphPtr));

struct GraphGrid {
    GC gc;			/* Graphics context for the grid. */
    unsigned int axisMask;	/* Axis flags */
    int mapped;			/* If non-zero, indicates that the grid
				 * is currently displayed. */
    int minorGrid;		/* If non-zero, draw grid lines for minor
				 * axis ticks too. */
    GridDrawProc *drawProc;
    GridPrintProc *printProc;
    GridDestroyProc *destroyProc;
};

/*
 * -------------------------------------------------------------------
 *
 * Graph --
 *
 *	Top level structure containing everything pertaining to
 *	the graph.
 *
 * -------------------------------------------------------------------
 */
struct Graph {
    Tcl_Interp *interp;		/* Interpreter associated with graph */
    Tk_Window tkwin;		/* Window that embodies the graph.  NULL
				 * means that the window has been
				 * destroyed but the data structures
				 * haven't yet been cleaned up. */

    Pixmap pixwin;		/* Pixmap for double buffering output */

    Display *display;		/* Display containing widget; needed,
				 * among other things, to release
				 * resources after tkwin has already gone
				 * away. */
    char *pathName;		/* Pathname of the widget. Is saved here
				 * in case the widget is destroyed and
				 * tkwin become unavailable for querying
				 * */
    Tcl_Command cmdToken;	/* Token for graph's widget command. */

    ElementType defElemType;	/* Default element type: either ELEM_LINE
				 * or ELEM_BAR */
    int flags;			/* Flags;  see below for definitions. */

    GraphAxis *axisArr[4];	/* Coordinate axis info: see bltGrAxis.c */
    GraphAxis *bottomAxis, *topAxis, *leftAxis, *rightAxis;
    GraphPostScript *postscript;/* PostScript options: see bltGrPS.c */
    GraphLegend *legendPtr;	/* Legend information: see bltGrLegd.c */
    GraphCrosshairs *crosshairs;/* Crosshairs information: see bltGrHairs.c */
    GraphGrid *gridPtr;		/* Grid attribute information */

    Tcl_HashTable elemTable;	/* Hash table containing all elements
				 * created, not just those currently
				 * displayed. */
    Blt_List elemList;		/* List describing order to draw elements */
    Tcl_HashTable markerTable;	/* Hash table of markers */
    Blt_List markerList;	/* Display list of markers */
    int nextMarkerId;		/* Tracks next marker identifier available */

    int reqWidth, reqHeight;	/* Requested size of graph window */
    int halo;			/* Maximum distance allowed between points
				 * when searching for a point */
    int inverted;		/* If non-zero, indicates the x and y axis
				 * positions should be inverted. */
    /*
     * Requested and computed sizes for margins surrounding the plotting
     * area. If non-zero, the requested margin is used, otherwise the
     * computed margin sizes below are used.
     */
    int reqLeftMargin, reqRightMargin, reqTopMargin, reqBottomMargin;
    int leftMargin, rightMargin, topMargin, bottomMargin;

    XRectangle plotArea;

    char *title;		/* Text of graph title (malloc-ed). If NULL,
				 * indicates that no overall title should be
				 * displayed. */
    int titleX, titleY;		/* Screen coordinates of graph title
				 * (anchor is center) */
    int titleWidth, titleHeight;/* Geometry of graph title */
    Tk_Justify justify;		/* Justification of the graph title. This
				 * only matters if the string is composed
				 * of multiple lines of text. */
    TkFont *fontPtr;		/* Font to use when drawing the graph title */

    Tk_Cursor cursor;

    int borderWidth;		/* Width of the exterior border */
    int relief;			/* Relief of the exterior border */
    Tk_3DBorder border;		/* 3-D border used to delineate the plot
				 * surface and outer edge of window */
    XColor *marginFg;		/* Foreground color of title and axis
				 * labels */
    int plotBW;			/* Width of interior 3-D border. */
    int plotRelief;		/* 3-d effect: TK_RELIEF_RAISED etc. */
    XColor *plotBg;		/* Color of plotting surface */

    GC plotFillGC;		/* Used to fill the plotting area with a
				 * solid background color. The fill color
				 * is stored in "plotBg". */
    GC drawGC;			/* Used for drawing on the margins. This
				 * includes the axis lines */
    GC fillGC;			/* Used to fill the background of the
				 * margins. The fill is governed by
				 * the background color or the tiled
				 * pixmap. */
    GC titleGC;			/* Used to draw the graph's title. */

    Blt_Tile tile;

    char *takeFocus;
    int width, height;		/* Size of graph window or PostScript
				 * page */

    /*
     * Barchart specific information
     */
    double barWidth;		/* Default width of each bar in graph units.
				 * The default width is 1.0 units. */
    BarMode mode;		/* Mode describing how to display bars
				 * with the same x-coordinates. Mode can
				 * be "stack", "align", or "normal" */
    int histmode;		/* If non-zero, indicates that bars are
    				 * drawn from -Inf to the actual y value,
				 * regardless of any base line */
    FreqInfo *freqArr;		/* Contains information about duplicate
				 * x-values in bar elements (malloc-ed).
				 * This information can also be accessed
				 * by the frequency hash table */
    Tcl_HashTable freqTable;	/* */
    int numStacks;		/* Number of entries in frequency array.
				 * If zero, indicates nothing special needs
				 * to be done for "stack" or "align" modes */

    double llx, lly;		/* Lower left coordinate of plot bbox */
    double urx, ury;		/* Upper right coordinate of plot bbox */

    int buffered;		/* If non-zero, cache elements by drawing
				 * them into a pixmap */
    Pixmap elemCache;		/* Pixmap used to cache elements displayed.
				 * If *buffered* is non-zero, each element
				 * is drawn into this pixmap before it is
				 * copied onto the screen.  The pixmap then
				 * acts as a cache (only the pixmap is
				 * redisplayed if the none of elements have
				 * changed). This is done so that markers can be
				 * redrawn quickly over elements without
				 * redrawing each element. */
    int cacheWidth, cacheHeight;/* Dimensions of element cache pixmap */

    char *scratchArr;		/* Utility space for building strings.
				 * Points to buffer of BUFSIZ bytes on
				 * the stack.  Currently used to
				 * create PostScript output for the
				 * "postscript" command. */
};

/*
 * Flag bits for graphs:
 * 	All kinds of state information kept here.  All these
 *	things happen when the window is available to draw into
 *	(DisplayGraph). Need the window width and height before
 *	we can calculate graph layout (i.e. the screen coordinates
 *	of the axes, elements, titles, etc). But we want to do this
 *	only when we have to, not every time the graph is redrawn.
 *
 *	Same goes for maintaining a pixmap to double buffer graph
 *	elements.  Need to mark when the pixmap needs to updated.
 *
 */


#define	COORDS_ALL_PARTS (1<<1)	/* Indicates that the layout
				 * of the axes and all elements and
				 * markers and the graph need to be
				 * recalculated. Otherwise, the layout
				 * of only those markers and elements that
				 * have changed will be reset. */


#define	GET_AXIS_GEOMETRY (1<<2)/* Indicates that the extents of the
				  * axes needs to be recalculated. */

#define RESET_AXES	(1<<3)	/* Flag to call to Blt_ComputeAxes
				 * routine.  This routine recalculates the
				 * scale offset (used for mapping
				 * coordinates) of each axis.  If an axis
				 * min or max limit has changed, then it
				 * sets flags to relayout and redraw the
				 * entire graph.  This routine also needs
				 * to be called before the axis can be
				 * used to xcompute transformations between
				 * graph and screen coordinates. */

#define LAYOUT_NEEDED	(1<<4)
#define	UPDATE_PIXMAP	(1<<5)	/* If set, redraw all elements into the
				 * pixmap used for buffering elements. */

#define REDRAW_PENDING 	(1<<6)	/* Non-zero means a DoWhenIdle
				 * handler has already been queued to
				 * redraw this window. */

#define DRAW_LEGEND     (1<<7)	/* Non-zero means redraw the legend.
				 * If this is the only DRAW_ flag, the
				 * legend display routine is called instead
				 * of the graph display routine. */

#define DRAW_PLOT_AREA	(1<<8)
#define DRAW_MARGINS	(1<<9)	/* Non-zero means that the region that
				 * borders the plotting surface of the
				 * graph needs to be redrawn. The
				 * possible reasons are:
				 *
				 * 1) an axis configuration changed
				 *
				 * 2) an axis limit changed
				 *
				 * 3) titles have changed
				 *
				 * 4) window was resized. */

#define	COORDS_WORLD 	(COORDS_ALL_PARTS|RESET_AXES|GET_AXIS_GEOMETRY|UPDATE_PIXMAP)
#define REDRAW_WORLD	(DRAW_PLOT_AREA | DRAW_MARGINS | DRAW_LEGEND)

#define SET_ALL_FLAGS	(REDRAW_WORLD | COORDS_WORLD | RESET_AXES)

/*
 * ---------------------- Forward declarations ------------------------
 */
extern void Blt_RedrawGraph _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_ComputeAxes _ANSI_ARGS_((Graph *graphPtr));

extern void Blt_GetAxisMapping _ANSI_ARGS_((Graph *graphPtr,
	unsigned int axisMask, AxisPair *axisPair));
extern Coordinate Blt_TransformPt _ANSI_ARGS_((Graph *graphPtr, double x, 
	double y, AxisPair *pairPtr));
extern Coordinate Blt_InvTransformPt _ANSI_ARGS_((Graph *graphPtr, double x,
	double y, AxisPair *pairPtr));
extern int Blt_TransformDist _ANSI_ARGS_((GraphAxis *axis, double value));

extern void Blt_StencilBitmap _ANSI_ARGS_((Display *display, Drawable draw,
	GC gc, Pixmap bitmap, int x, int y, int width, int height));
extern void Blt_GetTextExtents _ANSI_ARGS_((TkFont *fontPtr, char *text,
	int *widthPtr, int *heightPtr));
extern void Blt_PrintLine _ANSI_ARGS_((Graph *graphPtr, XPoint *pointArr,
	int numPoints));
extern void Blt_SetLineAttributes _ANSI_ARGS_((Graph *graphPtr,
	XColor *colorPtr, int lineWidth, Dashes *dashesPtr));
extern void Blt_3DRectangleToPostScript _ANSI_ARGS_((Graph *graphPtr,
	Tk_3DBorder border, int x, int y, int width, int height,
	int borderWidth, int relief));
extern void Blt_BackgroundToPostScript _ANSI_ARGS_((Graph *graphPtr,
	XColor *colorPtr));
extern void Blt_BitmapToPostScript _ANSI_ARGS_((Graph *graphPtr, Pixmap bitmap,
	int x, int y, int width, int height, double theta,
	XColor *bgColorPtr));
extern void Blt_ClearBackgroundToPostScript _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_FontToPostScript _ANSI_ARGS_((Graph *graphPtr,
	TkFont *fontPtr));
extern void Blt_ForegroundToPostScript _ANSI_ARGS_((Graph *graphPtr,
	XColor *colorPtr));
extern void Blt_LineDashesToPostScript _ANSI_ARGS_((Graph *graphPtr,
	Dashes *dashesPtr));
extern void Blt_LineWidthToPostScript _ANSI_ARGS_((Graph *graphPtr,
	int lineWidth));
extern void Blt_LinesToPostScript _ANSI_ARGS_((Graph *graphPtr,
	XPoint *pointArr, int numPoints));
extern void Blt_PolygonToPostScript _ANSI_ARGS_((Graph *graphPtr,
	XPoint *pointArr, int numPoints));
extern void Blt_Print3DRectangle _ANSI_ARGS_((Graph *graphPtr,
	Tk_3DBorder border, int x, int y, int width, int height,
	int borderWidth, int relief));
extern void Blt_RectangleToPostScript _ANSI_ARGS_((Graph *graphPtr,
	int x, int y, int width, int height));
extern void Blt_SegmentsToPostScript _ANSI_ARGS_((Graph *graphPtr,
	XSegment *segArr, int numSegs));
extern void Blt_StippleToPostScript _ANSI_ARGS_((Graph *graphPtr,
	Pixmap bitmap, int width, int height, int fgOrBg));
extern void Blt_PrintText _ANSI_ARGS_((Graph *graphPtr, char *text,
	TextAttributes *attrPtr, int x, int y));
extern LegendSite Blt_LegendSite _ANSI_ARGS_((Graph *graphPtr));

extern int Blt_GetElement _ANSI_ARGS_((Graph *graphPtr, char *name,
	Element **elemPtrPtr));
extern double Blt_GetDistanceToSegment _ANSI_ARGS_((XPoint *endPoints, int x,
	int y, double *closestXPtr, double *closestYPtr));
extern GraphAxis *Blt_XAxis _ANSI_ARGS_((Graph *graphPtr, unsigned int mask));
extern GraphAxis *Blt_YAxis _ANSI_ARGS_((Graph *graphPtr, unsigned int mask));

extern int Blt_GetAxisMargin _ANSI_ARGS_((GraphAxis *axis));

extern void Blt_InitFreqTable _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_ComputeStacks _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_CheckStacks _ANSI_ARGS_((Graph *graphPtr, int axisMask,
	double *minPtr, double *maxPtr));
extern void Blt_ResetStacks _ANSI_ARGS_((Graph *graphPtr));
extern int Blt_GetCoordinate _ANSI_ARGS_((Tcl_Interp *interp, char *expr,
	double *valuePtr));

/*
 * ---------------------- Global declarations ------------------------
 */
extern double bltNegInfinity, bltPosInfinity;

#endif /* !_BLTGRAPH_H */
