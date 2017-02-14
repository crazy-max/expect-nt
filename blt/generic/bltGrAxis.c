/*
 * bltGrAxis.c --
 *
 *	This module implements coordinate axes for a graph
 *	widget in the Tk toolkit.
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

#define DEF_NUM_TICKS	4	/* Each minor tick is 20% */
#define STATIC_TICK_SPACE  10

#define MAX_TICK_LABEL	200

#ifndef SHRT_MIN
#define SHRT_MIN        -0x8000
#endif

#define Vertical(a)		((a)->site&1)
#define Horizontal(a)    	(!((a)->site&1))
#define TitleHeight(a)		(((Axis *)a)->titleHeight)


/*
 * Round x in terms of units
 */
#define UROUND(x,u)		(Round((x)/(u))*(u))
#define UCEIL(x,u)		(ceil((x)/(u))*(u))
#define UFLOOR(x,u)		(floor((x)/(u))*(u))

#define LENGTH_MAJOR_TICK 	0.030	/* Length of a major tick */
#define LENGTH_MINOR_TICK 	0.015	/* Length of a minor (sub)tick */
#define LENGTH_LABEL_TICK 	0.040	/* Distance from graph to start of the
					 * label */

#ifdef __linux__
#define NUMDIGITS	14	/* Specifies the number of digits of accuracy
				 * used when outputting axis tick labels. */
#else
#define NUMDIGITS	15	/* Specifies the number of digits of accuracy
				 * used when outputting axis tick labels. */
#endif
typedef enum AxisComponents {
    MAJOR_TICK, MINOR_TICK, TICK_LABEL, AXIS_LINE
} AxisComponent;

#define AVG_TICK_NUM_CHARS	16	/* Assumed average tick label size */

enum LimitIndices {
    LMIN, LMAX
};

#define minLimit limits[LMIN]
#define maxLimit limits[LMAX]

/* Note: The following array is ordered according to enum AxisTypes */
static char *axisNames[] =
{
    "x", "y", "x2", "y2"
};

/* Map normalized coordinates to window coordinates */
#define WINX(X,x)    	(Round((x)*(X)->scale)+(X)->offset)
#define WINY(Y,y)    	((Y)->offset-Round((y)*(Y)->scale))

/* Map graph coordinates to normalized coordinates [0..1] */
#define NORMALIZE(a,x) 	(((x) - (a)->min) / (a)->range)

static int AxisLimitParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *AxisLimitPrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset, Tcl_FreeProc **freeProcPtr));

static Tk_CustomOption minOption =
{
    AxisLimitParseProc, AxisLimitPrintProc, (ClientData)LMIN,
};
static Tk_CustomOption maxOption =
{
    AxisLimitParseProc, AxisLimitPrintProc, (ClientData)LMAX,
};

static int TicksParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *TicksPrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));
static Tk_CustomOption ticksOption =
{
    TicksParseProc, TicksPrintProc, (ClientData)0,
};

extern Tk_CustomOption bltLengthOption;

/*
 * ----------------------------------------------------------------------
 *
 * TickLabel --
 *
 * 	Structure containing the formatted label and the screen
 * 	coordinates position of the tick label (anchored at its
 * 	center).
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    char *text;			/* Label for tick on axis */
    short int x, y;		/* Window position of tick on graph */
} TickLabel;

/*
 * ----------------------------------------------------------------------
 *
 * Ticks --
 *
 * 	Structure containing information where the ticks (major or
 *	minor) will be displayed on the graph.
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    int reqNumTicks;		/* If non-zero, indicates the number of major
				 * ticks.  This supersedes the calculation of
				 * ticks using autoscaling, but not if a
				 * stepsize is also requested. */
    int numTicks;		/* # of ticks on axis */
    int genTicks;
    double *valueArr;		/* Array of tick values, initially points to
				 * defSpace.  Malloc-ed if more ticks than
				 * STATIC_TICK_SPACE are needed. */
    double defSpace[STATIC_TICK_SPACE];
} Ticks;

/*
 * ----------------------------------------------------------------------
 *
 * Axis --
 *
 * 	Structure contains options controlling how the axis will be
 * 	displayed.
 *
 * ----------------------------------------------------------------------
 */

typedef struct {
    AxisType type;		/* Type of axis: X1, Y1, X2, or Y2 */
    AxisSite site;		/* Site of the axis relative to plotting
				 * surface: right, left, etc. */
    int logScale;		/* If non-zero, scale values logarithmically */
    int mapped;			/* If non-zero, display the axis */
    double min, max;		/* Actual (includes padding) axis limits */
    int width, height;		/* Bounding box of axis */

    AxisDrawProc *drawProc;
    AxisPrintProc *printProc;
    AxisCoordsProc *coordsProc;
    AxisDestroyProc *destroyProc;

    Graph *graphPtr;		/* The graph this axis is attached to. */

    /* User-definable fields */
    char *title;		/* Axis title */
    Tk_Justify justify;		/* Specifies how the axis title is justified.
				 * This only matters for strings which compose
				 * multiple lines of text. */
    int tickLength;		/* Length of major ticks. */
    TkFont *titleFontPtr;	/* Font used to the axis title. */

    int showTicks;		/* If non-zero, display (both major and minor)
				 * ticks on the axis */
    int loose;			/* If non-zero, autoscale limits loosely */
    int descending;		/* In non-zero, axis values are descending
				 * instead of monotonically increasing. */
    double limits[2];		/* Limits for scaling of axis */

    double prevMin, prevMax;	/* Last limits for of the axis. Used for
				 * detecting if the limits have changed since
				 * the last update (e.g. by the addition or
				 * subtraction of element data) */

    TkFont *tickFontPtr;	/* Font used to draw tick labels. */
    XColor *fgColorPtr;		/* Foreground color for ticks, labels,
				 * and axis */
    int lineWidth;		/* Line thickness of axis and ticks */
    double rotate;		/* Rotation of tick labels in degrees. */
    double theta;		/* Rotation normalized to a 0 to 360 degree
				 * interval. */
    char *formatCmd;		/* If non-NULL, indicates a Tcl proc to call
				 * when formatting tick labels. See the manual
				 * for its usage. */

    /* Calculated values */
    int flags;

    Tk_Anchor anchor;		/* Anchor of tick labels */
    int posArr[4];		/* Screen location of axis, major tick,
				 * minor tick, and tick label */
    int titleX, titleY;		/* Coordinates of axis title */
    int titleWidth, titleHeight;/* Geometry of axis title */

    double tickMin, tickMax;	/* Smallest and largest possible major ticks
				 * on the plot */
    double range;		/* Range of values (max-min) */
    double scale;		/* Scale factor to convert values to pixels */
    int offset;			/* Offset of plotting region from window
				 * origin */
    double reqStep;		/* Requested interval between ticks.
				 * However, if zero or less, the graph
				 * automatically calculates a "best" step size
				 * based on the range of data values for the
				 * axis. The default value is 0. */
    double majorStep;

    GC gc;			/* Graphics context for axis and tick labels */
    GC titleGC;			/* Graphics context for the axis title */
    int numSegments;		/* Size of the above segment array */
    XSegment *segArr;		/* Array of computed tick line segments.  Also
				 * includes the axis line */
    int numLabels;		/* Size of the above label array */
    TickLabel *labelArr;	/* Array of computed tick labels: See the
				 * description of the Label structure. */
    Ticks majorTicks;
    Ticks minorTicks;

} Axis;

/* Axis flags: */
#define AXIS_CONFIG_DIRTY	(1<<8)
#define AXIS_CONFIG_USER_BIT	(1<<9)
#define AXIS_CONFIG_MIN_MASK	(AXIS_CONFIG_USER_BIT << LMIN)
#define AXIS_CONFIG_MAX_MASK	(AXIS_CONFIG_USER_BIT << LMAX)
#define AXIS_CONFIG_MIN_SET(a) 	((a)->flags & AXIS_CONFIG_MIN_MASK)
#define AXIS_CONFIG_MAX_SET(a) 	((a)->flags & AXIS_CONFIG_MAX_MASK)

#define DEF_AXIS_ALT_MAPPED 	"false"
#define DEF_AXIS_COMMAND	(char *)NULL
#define DEF_AXIS_DESCENDING 	"0"
#define DEF_AXIS_FG_COLOR	BLACK
#define DEF_AXIS_FG_MONO	BLACK
#define DEF_AXIS_TICK_FONT	"Courier 8 bold"
#define DEF_AXIS_JUSTIFY	"center"
#define DEF_AXIS_LINE_WIDTH	"0"
#define DEF_AXIS_LOG_SCALE 	"0"
#define DEF_AXIS_LOOSE 		"0"
#define DEF_AXIS_MAX		(char *)NULL
#define DEF_AXIS_MIN		(char *)NULL
#define DEF_AXIS_ROTATE		"0.0"
#define DEF_AXIS_STD_MAPPED 	"1"
#define DEF_AXIS_STEPSIZE	"0.0"
#define DEF_AXIS_SUBDIVISIONS	"2"
#define DEF_AXIS_TICKS		"0"
#define DEF_AXIS_TICK_LENGTH	"0.1i"
#define DEF_AXIS_TICK_VALUES	(char *)NULL
#define DEF_AXIS_TITLE		(char *)NULL
#define DEF_AXIS_TITLE_FONT	STD_FONT
#define DEF_AXIS_X_STEPSIZE_BARCHART	"1.0"
#define DEF_AXIS_X_SUBDIVISIONS_BARCHART	"0"

static Tk_ConfigSpec xAxisConfigSpecs[] =
{
    {TK_CONFIG_COLOR, "-color", "xColor", "AxisColor",
	DEF_AXIS_FG_COLOR, Tk_Offset(Axis, fgColorPtr),
	TK_CONFIG_COLOR_ONLY | ALL_MASK},
    {TK_CONFIG_COLOR, "-color", "xColor", "AxisColor",
	DEF_AXIS_FG_MONO, Tk_Offset(Axis, fgColorPtr),
	TK_CONFIG_MONO_ONLY | ALL_MASK},
    {TK_CONFIG_STRING, "-command", "xCommand", "AxisCommand",
	DEF_AXIS_COMMAND, Tk_Offset(Axis, formatCmd),
	ALL_MASK | TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-descending", "xDescending", "AxisDescending",
	DEF_AXIS_DESCENDING, Tk_Offset(Axis, descending),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_JUSTIFY, "-justify", "xJustify", "AxisJustify",
	DEF_AXIS_JUSTIFY, Tk_Offset(Axis, justify), ALL_MASK},
    {TK_CONFIG_CUSTOM, "-linewidth", "xLineWidth", "AxisLineWidth",
	DEF_AXIS_LINE_WIDTH, Tk_Offset(Axis, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-logscale", "xLogScale", "AxisLogScale",
	DEF_AXIS_LOG_SCALE, Tk_Offset(Axis, logScale),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_BOOLEAN, "-loose", "xLoose", "AxisLoose",
	DEF_AXIS_LOOSE, Tk_Offset(Axis, loose),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-majorticks", "xMajorTicks", "AxisMajorTicks",
	DEF_AXIS_TICK_VALUES, Tk_Offset(Axis, majorTicks),
	ALL_MASK | TK_CONFIG_NULL_OK, &ticksOption},
    {TK_CONFIG_BOOLEAN, "-mapped", "xMapped", "AxisMapped",
	DEF_AXIS_STD_MAPPED, Tk_Offset(Axis, mapped),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-max", "xMax", "AxisMax",
	DEF_AXIS_MAX, 0, ALL_MASK | TK_CONFIG_NULL_OK, &maxOption},
    {TK_CONFIG_CUSTOM, "-min", "xMin", "AxisMin",
	DEF_AXIS_MIN, 0, ALL_MASK | TK_CONFIG_NULL_OK, &minOption},
    {TK_CONFIG_CUSTOM, "-minorticks", "xMinorTicks", "AxisMinorTicks",
	DEF_AXIS_TICK_VALUES, Tk_Offset(Axis, minorTicks),
	ALL_MASK | TK_CONFIG_NULL_OK, &ticksOption},
    {TK_CONFIG_DOUBLE, "-rotate", "xRotate", "AxisRotate",
	DEF_AXIS_ROTATE, Tk_Offset(Axis, rotate),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_BOOLEAN, "-showticks", "xShowTicks", "AxisShowTicks",
	DEF_AXIS_TICKS, Tk_Offset(Axis, showTicks),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_DOUBLE, "-stepsize", "xStepsize", "AxisStepsize",
	DEF_AXIS_STEPSIZE, Tk_Offset(Axis, reqStep), XYGRAPH_MASK},
    {TK_CONFIG_DOUBLE, "-stepsize", "xStepsize", "AxisStepsize",
	DEF_AXIS_X_STEPSIZE_BARCHART, Tk_Offset(Axis, reqStep),
	BARCHART_MASK},
    {TK_CONFIG_INT, "-subdivisions", "xSubdivisions", "AxisSubdivisions",
	DEF_AXIS_SUBDIVISIONS, Tk_Offset(Axis, minorTicks.reqNumTicks),
	XYGRAPH_MASK},
    {TK_CONFIG_INT, "-subdivisions", "xSubdivisions", "AxisSubdivisions",
	DEF_AXIS_X_SUBDIVISIONS_BARCHART,
	Tk_Offset(Axis, minorTicks.reqNumTicks), BARCHART_MASK},
    {TK_CONFIG_FONT, "-tickfont", "xTickFont", "AxisTickFont",
	DEF_AXIS_TICK_FONT, Tk_Offset(Axis, tickFontPtr), ALL_MASK},
    {TK_CONFIG_PIXELS, "-ticklength", "xTickLength", "AxisTickLength",
	DEF_AXIS_TICK_LENGTH, Tk_Offset(Axis, tickLength), ALL_MASK},
    {TK_CONFIG_STRING, "-title", "xTitle", "AxisTitle",
	"X", Tk_Offset(Axis, title), ALL_MASK | TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-titlefont", "xTitleFont", "AxisTitleFont",
	DEF_AXIS_TITLE_FONT, Tk_Offset(Axis, titleFontPtr), ALL_MASK},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};


static Tk_ConfigSpec x2AxisConfigSpecs[] =
{
    {TK_CONFIG_COLOR, "-color", "x2Color", "AxisColor",
	DEF_AXIS_FG_COLOR, Tk_Offset(Axis, fgColorPtr),
	TK_CONFIG_COLOR_ONLY | ALL_MASK},
    {TK_CONFIG_COLOR, "-color", "x2Color", "AxisColor",
	DEF_AXIS_FG_MONO, Tk_Offset(Axis, fgColorPtr),
	TK_CONFIG_MONO_ONLY | ALL_MASK},
    {TK_CONFIG_STRING, "-command", "x2Command", "AxisCommand",
	DEF_AXIS_COMMAND, Tk_Offset(Axis, formatCmd),
	ALL_MASK | TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-descending", "x2Descending", "AxisDescending",
	DEF_AXIS_DESCENDING, Tk_Offset(Axis, descending),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_JUSTIFY, "-justify", "x2Justify", "AxisJustify",
	DEF_AXIS_JUSTIFY, Tk_Offset(Axis, justify), ALL_MASK},
    {TK_CONFIG_CUSTOM, "-linewidth", "x2LineWidth", "AxisLineWidth",
	DEF_AXIS_LINE_WIDTH, Tk_Offset(Axis, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-logscale", "x2Logscale", "AxisLogscale",
	DEF_AXIS_LOG_SCALE, Tk_Offset(Axis, logScale),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_BOOLEAN, "-loose", "x2Loose", "AxisLoose",
	DEF_AXIS_LOOSE, Tk_Offset(Axis, loose),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-majorticks", "x2MajorTicks", "AxisMajorTicks",
	DEF_AXIS_TICK_VALUES, Tk_Offset(Axis, majorTicks),
	ALL_MASK | TK_CONFIG_NULL_OK, &ticksOption},
    {TK_CONFIG_BOOLEAN, "-mapped", "x2Mapped", "AxisMapped",
	DEF_AXIS_ALT_MAPPED, Tk_Offset(Axis, mapped),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-max", "x2Max", "AxisMax",
	DEF_AXIS_MAX, 0, ALL_MASK | TK_CONFIG_NULL_OK, &maxOption},
    {TK_CONFIG_CUSTOM, "-min", "x2Min", "AxisMin",
	DEF_AXIS_MIN, 0, ALL_MASK | TK_CONFIG_NULL_OK, &minOption},
    {TK_CONFIG_CUSTOM, "-minorticks", "x2MinorTicks", "AxisMinorTicks",
	DEF_AXIS_TICK_VALUES, Tk_Offset(Axis, minorTicks),
	ALL_MASK | TK_CONFIG_NULL_OK, &ticksOption},
    {TK_CONFIG_DOUBLE, "-rotate", "x2Rotate", "AxisRotate",
	DEF_AXIS_ROTATE, Tk_Offset(Axis, rotate),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_BOOLEAN, "-showticks", "x2ShowTicks", "AxisShowTicks",
	DEF_AXIS_TICKS, Tk_Offset(Axis, showTicks), ALL_MASK},
    {TK_CONFIG_DOUBLE, "-stepsize", "x2Stepsize", "AxisStepsize",
	DEF_AXIS_STEPSIZE, Tk_Offset(Axis, reqStep),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_INT, "-subdivisions", "x2Subdivisions", "AxisSubdivisions",
	DEF_AXIS_SUBDIVISIONS, Tk_Offset(Axis, minorTicks.reqNumTicks),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_FONT, "-tickfont", "x2TickFont", "AxisTickFont",
	DEF_AXIS_TICK_FONT, Tk_Offset(Axis, tickFontPtr), ALL_MASK},
    {TK_CONFIG_PIXELS, "-ticklength", "x2TickLength", "AxisTickLength",
	DEF_AXIS_TICK_LENGTH, Tk_Offset(Axis, tickLength), ALL_MASK},
    {TK_CONFIG_STRING, "-title", "x2Title", "AxisTitle",
	"X2", Tk_Offset(Axis, title), ALL_MASK | TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-titlefont", "x2TitleFont", "AxisTitleFont",
	DEF_AXIS_TITLE_FONT, Tk_Offset(Axis, titleFontPtr), ALL_MASK},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Tk_ConfigSpec yAxisConfigSpecs[] =
{
    {TK_CONFIG_COLOR, "-color", "yColor", "AxisColor",
	DEF_AXIS_FG_COLOR, Tk_Offset(Axis, fgColorPtr),
	TK_CONFIG_COLOR_ONLY | ALL_MASK},
    {TK_CONFIG_COLOR, "-color", "yColor", "AxisColor",
	DEF_AXIS_FG_MONO, Tk_Offset(Axis, fgColorPtr),
	TK_CONFIG_MONO_ONLY | ALL_MASK},
    {TK_CONFIG_STRING, "-command", "yCommand", "AxisCommand",
	(char *)NULL, Tk_Offset(Axis, formatCmd),
	ALL_MASK | TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-descending", "yDescending", "AxisDescending",
	DEF_AXIS_DESCENDING, Tk_Offset(Axis, descending),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_JUSTIFY, "-justify", "yJustify", "AxisJustify",
	DEF_AXIS_JUSTIFY, Tk_Offset(Axis, justify), ALL_MASK},
    {TK_CONFIG_BOOLEAN, "-loose", "yLoose", "AxisLoose",
	DEF_AXIS_LOOSE, Tk_Offset(Axis, loose),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-linewidth", "yLineWidth", "AxisLineWidth",
	DEF_AXIS_LINE_WIDTH, Tk_Offset(Axis, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-logscale", "yLogscale", "AxisLogscale",
	DEF_AXIS_LOG_SCALE, Tk_Offset(Axis, logScale),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-majorticks", "yMajorTicks", "AxisMajorTicks",
	DEF_AXIS_TICK_VALUES, Tk_Offset(Axis, majorTicks),
	ALL_MASK | TK_CONFIG_NULL_OK, &ticksOption},
    {TK_CONFIG_BOOLEAN, "-mapped", "yMapped", "AxisMapped",
	DEF_AXIS_STD_MAPPED, Tk_Offset(Axis, mapped),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-max", "yMax", "AxisMax",
	DEF_AXIS_MAX, 0, ALL_MASK | TK_CONFIG_NULL_OK, &maxOption},
    {TK_CONFIG_CUSTOM, "-min", "yMin", "AxisMin",
	DEF_AXIS_MIN, 0, ALL_MASK | TK_CONFIG_NULL_OK, &minOption},
    {TK_CONFIG_CUSTOM, "-minorticks", "yMinorTicks", "AxisMinorTicks",
	DEF_AXIS_TICK_VALUES, Tk_Offset(Axis, minorTicks),
	ALL_MASK | TK_CONFIG_NULL_OK, &ticksOption},
    {TK_CONFIG_DOUBLE, "-rotate", "yRotate", "AxisRotate",
	DEF_AXIS_ROTATE, Tk_Offset(Axis, rotate),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_BOOLEAN, "-showticks", "yShowTicks", "AxisShowTicks",
	DEF_AXIS_TICKS, Tk_Offset(Axis, showTicks),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_DOUBLE, "-stepsize", "yStepsize", "AxisStepsize",
	DEF_AXIS_STEPSIZE, Tk_Offset(Axis, reqStep),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_INT, "-subdivisions", "ySubdivisions", "AxisSubdivisions",
	DEF_AXIS_SUBDIVISIONS,
	Tk_Offset(Axis, minorTicks.reqNumTicks), ALL_MASK},
    {TK_CONFIG_FONT, "-tickfont", "yTickFont", "AxisTickFont",
	DEF_AXIS_TICK_FONT, Tk_Offset(Axis, tickFontPtr), ALL_MASK},
    {TK_CONFIG_PIXELS, "-ticklength", "yTickLength", "AxisTickLength",
	DEF_AXIS_TICK_LENGTH, Tk_Offset(Axis, tickLength), ALL_MASK},
    {TK_CONFIG_STRING, "-title", "yTitle", "AxisTitle",
	"Y", Tk_Offset(Axis, title), ALL_MASK | TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-titlefont", "yTitleFont", "AxisTitleFont",
	DEF_AXIS_TITLE_FONT, Tk_Offset(Axis, titleFontPtr), ALL_MASK},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Tk_ConfigSpec y2AxisConfigSpecs[] =
{
    {TK_CONFIG_COLOR, "-color", "y2Color", "AxisColor",
	DEF_AXIS_FG_COLOR, Tk_Offset(Axis, fgColorPtr),
	TK_CONFIG_COLOR_ONLY | ALL_MASK},
    {TK_CONFIG_COLOR, "-color", "y2Color", "AxisColor",
	DEF_AXIS_FG_MONO, Tk_Offset(Axis, fgColorPtr),
	TK_CONFIG_MONO_ONLY | ALL_MASK},
    {TK_CONFIG_STRING, "-command", "y2Command", "AxisCommand",
	DEF_AXIS_COMMAND, Tk_Offset(Axis, formatCmd),
	ALL_MASK | TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-descending", "y2Descending", "AxisDescending",
	DEF_AXIS_DESCENDING, Tk_Offset(Axis, descending),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_JUSTIFY, "-justify", "y2Justify", "AxisJustify",
	DEF_AXIS_JUSTIFY, Tk_Offset(Axis, justify), ALL_MASK},
    {TK_CONFIG_CUSTOM, "-linewidth", "y2LineWidth", "AxisLineWidth",
	DEF_AXIS_LINE_WIDTH, Tk_Offset(Axis, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-loose", "y2Loose", "AxisLoose",
	DEF_AXIS_LOOSE, Tk_Offset(Axis, loose),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_BOOLEAN, "-logscale", "y2Logscale", "AxisLogscale",
	DEF_AXIS_LOG_SCALE, Tk_Offset(Axis, logScale),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-majorticks", "y2MajorTicks", "AxisMajorTicks",
	DEF_AXIS_TICK_VALUES, Tk_Offset(Axis, majorTicks),
	ALL_MASK | TK_CONFIG_NULL_OK, &ticksOption},
    {TK_CONFIG_BOOLEAN, "-mapped", "y2Mapped", "AxisMapped",
	DEF_AXIS_ALT_MAPPED, Tk_Offset(Axis, mapped),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_CUSTOM, "-max", "y2Max", "AxisMax",
	DEF_AXIS_MAX, 0, ALL_MASK | TK_CONFIG_NULL_OK, &maxOption},
    {TK_CONFIG_CUSTOM, "-min", "y2Min", "AxisMin",
	DEF_AXIS_MIN, 0, ALL_MASK | TK_CONFIG_NULL_OK, &minOption},
    {TK_CONFIG_CUSTOM, "-minorticks", "y2MinorTicks", "AxisMinorTicks",
	DEF_AXIS_TICK_VALUES, Tk_Offset(Axis, minorTicks),
	ALL_MASK | TK_CONFIG_NULL_OK, &ticksOption},
    {TK_CONFIG_DOUBLE, "-rotate", "y2Rotate", "AxisRotate",
	DEF_AXIS_ROTATE, Tk_Offset(Axis, rotate),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_BOOLEAN, "-showticks", "y2ShowTicks", "AxisShowTicks",
	DEF_AXIS_TICKS, Tk_Offset(Axis, showTicks),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_DOUBLE, "-stepsize", "y2Stepsize", "AxisStepsize",
	DEF_AXIS_STEPSIZE, Tk_Offset(Axis, reqStep),
	TK_CONFIG_DONT_SET_DEFAULT | ALL_MASK},
    {TK_CONFIG_INT, "-subdivisions", "y2Subdivisions", "AxisSubdivisions",
	DEF_AXIS_SUBDIVISIONS,
	Tk_Offset(Axis, minorTicks.reqNumTicks), ALL_MASK},
    {TK_CONFIG_FONT, "-tickfont", "y2TickFont", "AxisTickFont",
	DEF_AXIS_TICK_FONT, Tk_Offset(Axis, tickFontPtr), ALL_MASK},
    {TK_CONFIG_PIXELS, "-ticklength", "y2TickLength", "AxisTickLength",
	DEF_AXIS_TICK_LENGTH, Tk_Offset(Axis, tickLength), ALL_MASK},
    {TK_CONFIG_STRING, "-title", "y2Title", "AxisTitle",
	"Y2", Tk_Offset(Axis, title), ALL_MASK | TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-titlefont", "y2TitleFont", "AxisTitleFont",
	DEF_AXIS_TITLE_FONT, Tk_Offset(Axis, titleFontPtr), ALL_MASK},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Tk_ConfigSpec *axisConfigSpecs[4] =
{
    xAxisConfigSpecs, yAxisConfigSpecs,		/* Normal x-axis and y-axis */
    x2AxisConfigSpecs, y2AxisConfigSpecs	/* Alternate x and y axis */
};


INLINE static int
Round(x)
    register double x;
{
    return (int) (x + ((x < 0.0) ? -0.5 : 0.5));
}

INLINE static double
Fabs(x)
    register double x;
{
    return ((x < 0.0) ? -x : x);
}

/*
 * ----------------------------------------------------------------------
 *
 * OutOfRange --
 *
 *	Determines if a value does not lie within a given range.  
 *
 *	The value is normalized and compared against the interval 
 *	[0..1], where 0.0 is the minimum and 1.0 is the maximum.
 *	DBL_EPSILON is the smallest number that can be represented
 *	on the host machine, such that (1.0 + epsilon) != 1.0.
 *
 *	Please note, *max* can't equal *min*.
 *
 * Results:
 *	Returns whether the value lies outside of the given range.
 *	If value is outside of the interval [min..max], 1 is returned; 
 *	0 otherwise.
 *
 * ---------------------------------------------------------------------- 
 */
INLINE static int
OutOfRange(value, min, max)
    register double value, min, max;
{
    register double norm;

    norm = (value - min) / (max - min);
    return (((norm - 1.0) > DBL_EPSILON) || (((1.0 - norm) - 1.0) > DBL_EPSILON));
}

/* ----------------------------------------------------------------------
 * Custom option parse and print procedures
 * ----------------------------------------------------------------------
 */
/*
 * ----------------------------------------------------------------------
 *
 * AxisLimitParseProc --
 *
 *	Convert the string representation of an axis limit into its numeric
 *	form.
 *
 * Results:
 *	The return value is a standard Tcl result.  The symbol type is
 *	written into the widget record.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
AxisLimitParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* Either LMIN or LMAX */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* */
    char *widgRec;		/* Axis structure */
    int offset;			/* Offset of limit */
{
    Axis *axisPtr = (Axis *)(widgRec);
    int whichLimit = (int)clientData;
    int mask;

    mask = (AXIS_CONFIG_USER_BIT << whichLimit);
    if ((value == NULL) || (*value == '\0')) {
	axisPtr->flags &= ~mask;
    } else {
	double newLimit;

	if (Tcl_ExprDouble(interp, value, &newLimit) != TCL_OK) {
	    return TCL_ERROR;
	}
	axisPtr->limits[whichLimit] = newLimit;
	axisPtr->flags |= mask;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * AxisLimitPrintProc --
 *
 *	Convert the floating point axis limit into a string.
 *
 * Results:
 *	The string representation of the limit is returned.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
AxisLimitPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Either LMIN or LMAX */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* */
    int offset;
    Tcl_FreeProc **freeProcPtr;
{
    Axis *axisPtr = (Axis *)(widgRec);
    int whichLimit = (int)clientData;
    int mask;
    char *result;

    result = "";
    mask = (AXIS_CONFIG_USER_BIT << whichLimit);
    if (axisPtr->flags & mask) {
	char string[TCL_DOUBLE_SPACE + 1];

	Tcl_PrintDouble(axisPtr->graphPtr->interp, axisPtr->limits[whichLimit],
		string);
	result = ckstrdup(string);
	if (result == NULL) {
	    return "";
	}
	*freeProcPtr = (Tcl_FreeProc *)BLT_ckfree;
    }
    return result;
}


static int
FillTickArray(interp, tickPtr, numExprs, exprArr)
    Tcl_Interp *interp;
    Ticks *tickPtr;
    int numExprs;
    char **exprArr;
{
    double *valueArr;
    double tickValue;
    register int i;

    valueArr = tickPtr->defSpace;
    tickPtr->numTicks = 0;
    if (numExprs > STATIC_TICK_SPACE) {
	valueArr = (double *)ckalloc(sizeof(double) * numExprs);
	if (valueArr == NULL) {
	    Panic("can't allocation tick array");
	}
    }
    for (i = 0; i < numExprs; i++) {
	if (Tcl_ExprDouble(interp, exprArr[i], &tickValue) != TCL_OK) {
	    ckfree((char *)valueArr);
	    return TCL_ERROR;
	}
	valueArr[i] = tickValue;
    }
    if (tickPtr->valueArr != tickPtr->defSpace) {
	ckfree((char *)tickPtr->valueArr);
    }
    tickPtr->valueArr = valueArr;
    tickPtr->numTicks = numExprs;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TicksParseProc --
 *
 *
 * Results:
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TicksParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* List of expressions representing axis
				 * values indicating where to display ticks */
    char *widgRec;		/* Axis structure */
    int offset;			/* not used */
{
    Ticks *ticksPtr = (Ticks *) (widgRec + offset);
    int numExprs;
    char **exprArr;
    int result;

    if ((value == NULL) || (*value == '\0')) {
      noTicks:
	ticksPtr->genTicks = TRUE;
	FillTickArray(interp, ticksPtr, 0, (char **)NULL);
	return TCL_OK;
    }
    if (Tcl_SplitList(interp, value, &numExprs, &exprArr) != TCL_OK) {
	return TCL_ERROR;
    }
    ticksPtr->genTicks = TRUE;	/* Revert to computed ticks on errors. */
    if (numExprs == 0) {
	goto noTicks;
    }
    result = FillTickArray(interp, ticksPtr, numExprs, exprArr);
    ckfree((char *)exprArr);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    ticksPtr->genTicks = FALSE;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TicksPrintProc --
 *
 *	Convert array of tick coordinates to a list.
 *
 * Results:
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
TicksPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* */
    int offset;
    Tcl_FreeProc **freeProcPtr;
{
    Axis *axisPtr = (Axis *)(widgRec);
    char string[TCL_DOUBLE_SPACE + 1];
    register int i;
    char *result;
    Tcl_DString valueList;

    if (axisPtr->majorTicks.genTicks) {
	return "";
    }
    Tcl_DStringInit(&valueList);
    for (i = 0; i < axisPtr->majorTicks.numTicks; i++) {
	Tcl_PrintDouble(axisPtr->graphPtr->interp, 
		axisPtr->majorTicks.valueArr[i], string);
	Tcl_DStringAppendElement(&valueList, string);

    }
    *freeProcPtr = (Tcl_FreeProc *)BLT_ckfree;
    result = ckstrdup(Tcl_DStringValue(&valueList));
    Tcl_DStringFree(&valueList);
    return (result);
}

/*
 * ----------------------------------------------------------------------
 *
 * MakeLabel --
 *
 *	Converts a floating point tick value to a string to be used as its
 *	label.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Returns a new label in the string character buffer.  The formatted
 *	tick label will be displayed on the graph.
 *
 * ---------------------------------------------------------------------- */
static void
MakeLabel(axisPtr, value, string)
    Axis *axisPtr;		/* Axis structure */
    double value;		/* Value to be convert to a decimal string */
    char string[];		/* String (length is MAX_TICK_LABEL+1)
				 * containing the formatted label */
{
    /*
     * Generate the default tick label based upon the tick value.
     */
    if (axisPtr->logScale) {
	sprintf(string, "1E%d", Round(value));
    } else {
	sprintf(string, "%.*g", NUMDIGITS, value);
    }

    if (axisPtr->formatCmd != NULL) {
	Tcl_Interp *interp = axisPtr->graphPtr->interp; 
	Tk_Window tkwin = axisPtr->graphPtr->tkwin;

	/*
	 * A Tcl proc was designated to format tick labels. Append the path
	 * name of the widget and the default tick label as arguments when
	 * invoking it. Copy and save the new label from interp->result.
	 */
	Tcl_ResetResult(interp);
	if (Tcl_VarEval(interp, axisPtr->formatCmd, " ", Tk_PathName(tkwin), 
		" ", string, (char *)NULL) != TCL_OK) {
	    Tk_BackgroundError(interp);
	    return;		/* Error in format proc */
	}
	/*
	 * The proc could return a string of any length, so limit its size to
	 * what will fit in the return string.
	 */
	strncpy(string, Tcl_GetStringResult(interp), MAX_TICK_LABEL);
	string[MAX_TICK_LABEL] = 0;

	/* Make sure to clear the interpreter's result */
	Tcl_ResetResult(interp);
    }
}


GraphAxis *
Blt_XAxis(graphPtr, mask)
    Graph *graphPtr;
    unsigned int mask;
{
    switch (mask & AXIS_MASK_X) {
    case AXIS_BIT_X1:
    case (AXIS_BIT_X1 | AXIS_BIT_X2):	/* mapped to both x-axes */
	return (graphPtr->axisX1);
    case AXIS_BIT_X2:
	return (graphPtr->axisX2);
    default:
	return NULL;
    }
}

GraphAxis *
Blt_YAxis(graphPtr, mask)
    Graph *graphPtr;
    unsigned int mask;
{
    switch (mask & AXIS_MASK_Y) {
    case AXIS_BIT_Y1:
    case (AXIS_BIT_Y1 | AXIS_BIT_Y2):	/* mapped to both y-axes */
	return (graphPtr->axisY1);
    case AXIS_BIT_Y2:
	return (graphPtr->axisY2);
    default:
	return NULL;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * InvTransform --
 *
 *	Maps the given window y-coordinate back to a graph coordinate
 *	value. Called by the graph locater routine.
 *
 * Results:
 *	Returns the graph coordinate value at the given window
 *	y-coordinate.
 *
 * ----------------------------------------------------------------------
 */
static double
InvTransform(axis, coord)
    GraphAxis *axis;
    double coord;
{
    double norm, value;
    Axis *axisPtr = (Axis *)axis;

    if (Horizontal(axisPtr)) {
	coord = (coord - axisPtr->offset);
    } else {
	coord = (axisPtr->offset - coord);
    }
    norm = coord / axisPtr->scale;
    if (axisPtr->descending) {
	norm = 1.0 - norm;
    }
    value = (norm * axisPtr->range) + axisPtr->min;
    if (axisPtr->logScale) {
	value = BLT_EXP10(value);
    }
    return (value);
}

/*
 * ----------------------------------------------------------------------
 *
 * Transform --
 *
 *	Map the given graph coordinate value to its axis, returning a window
 *	position.
 *
 * Results:
 *	Returns the window coordinate position on the given axis.
 *
 * Note:
 *	Since line and polygon clipping is performed by the X server, we must
 *	be careful about coordinates which are outside of the range of a
 *	signed short int.
 *
 * ----------------------------------------------------------------------
 */
static double
Transform(axis, value)
    GraphAxis *axis;
    register double value;
{
    Axis *axisPtr = (Axis *)axis;
    register double norm;
    register double coord;

    if (value == bltPosInfinity) {
	norm = 1.0;
    } else if (value == bltNegInfinity) {
	norm = 0.0;
    } else {
	if (axisPtr->logScale) {
	    if (value > 0.0) {
		value = log10(value);
	    } else if (value < 0.0) {
		value = 0.0;
	    }
	}
	norm = NORMALIZE(axisPtr, value);
    }
    if (axisPtr->descending) {
	norm = 1.0 - norm;
    }
    norm *= axisPtr->scale;
    if (Horizontal(axisPtr)) {
	coord = norm + (double)axisPtr->offset;
    } else {
	coord = (double)axisPtr->offset - norm;
    }
    return (coord);
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_GetAxisMapping --
 *
 *	Returns the x and y axes indicated by the axis flag mask.
 *
 * Results:
 *	Returns via pairPtr, the x and y axes designated.
 *
 * ----------------------------------------------------------------------
 */
void
Blt_GetAxisMapping(graphPtr, mask, pairPtr)
    Graph *graphPtr;
    unsigned int mask;
    AxisPair *pairPtr;
{
    pairPtr->x = Blt_XAxis(graphPtr, mask);
    pairPtr->y = Blt_YAxis(graphPtr, mask);
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_TransformPt --
 *
 *	Maps the given graph x,y coordinate values to a window position.
 *
 * Results:
 *	Returns a XPoint structure containing the window coordinates of
 *	the given graph x,y coordinate.
 *
 * ----------------------------------------------------------------------
 */
Coordinate
Blt_TransformPt(graphPtr, x, y, pairPtr)
    Graph *graphPtr;
    double x, y;		/* Graph x and y coordinates */
    AxisPair *pairPtr;		/* Specifies which axes to use */
{
    Coordinate coord;

    x = Transform(pairPtr->x, x);
    y = Transform(pairPtr->y, y);
    if (graphPtr->inverted) {	/* Swap x and y coordinates */
	double hold;

	hold = y, y = x, x = hold;
    }
    coord.x = x;
    coord.y = y;
    return (coord);
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_InvTransformPt --
 *
 *	Maps the given graph x,y coordinate values to a window position.
 *
 * Results:
 *	Returns a XPoint structure containing the window coordinates of
 *	the given graph x,y coordinate.
 *
 * ----------------------------------------------------------------------
 */
Coordinate
Blt_InvTransformPt(graphPtr, x, y, pairPtr)
    Graph *graphPtr;
    double x, y;		/* Window x and y coordinates */
    AxisPair *pairPtr;		/* Specifies which axes to use */
{
    Coordinate coord;

    if (graphPtr->inverted) {	/* Swap x and y coordinates */
	double hold;

	hold = y, y = x, x = hold;
    }
    coord.x = InvTransform(pairPtr->x, x);
    coord.y = InvTransform(pairPtr->y, y);
    return (coord);
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_PointOnGraph --
 *
 *	Determines if the window coordinates given represent a point
 *	on the graph (within the bounds of the graph axes).
 *
 * Results:
 *	Returns 1 is the point is within the bounds of the graph,
 *	0 otherwise.
 *
 * ----------------------------------------------------------------------
 */
int
Blt_PointOnGraph(graphPtr, x, y)
    Graph *graphPtr;
    int x, y;
{
    double norm;
    Axis *axisPtr;

    /*
     * It doesn't make a difference which x-axis or y-axis we use (the point
     * is referenced by window coordinates). We're just checking if the
     * point's within the range of axis' normalized coordinates [0..1].
     */
    axisPtr = (Axis *)graphPtr->bottomAxis;
    if (axisPtr->scale == 0.0) {
	return 0;		/* Axis layout hasn't been calculated yet */
    }
    norm = (x - axisPtr->offset) / axisPtr->scale;
    if ((norm < 0.0) || (norm > 1.0)) {
	return 0;		/* x-coordinates are off the graph */
    }
    axisPtr = (Axis *)graphPtr->leftAxis;
    norm = (axisPtr->offset - y) / axisPtr->scale;
    return ((norm >= 0.0) && (norm <= 1.0));
}

/*
 * ----------------------------------------------------------------------
 *
 * GetDataLimits --
 *
 *	Updates the min and max values for each axis as determined by
 *	the data elements currently to be displayed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Minimum, maximum data limit fields for both the X and Y axes
 *	in the graph widget record are updated.
 *
 * ----------------------------------------------------------------------
 */
static void
GetDataLimits(axisPtr)
    Axis *axisPtr;
{
    Graph *graphPtr = axisPtr->graphPtr; 
    int result;

    if ((!AXIS_CONFIG_MAX_SET(axisPtr)) || (!AXIS_CONFIG_MIN_SET(axisPtr))) {
	register Element *elemPtr;
	Blt_ListItem *itemPtr;
	double min, max;
	double elemMin, elemMax;
	double value;
	int mask;

	/* Find the minimum and maximum values for all the elements displayed */
	min = bltPosInfinity, max = bltNegInfinity;
	for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	    itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	    elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	    if (!elemPtr->mapped) {
		continue;
	    }
	    if ((*elemPtr->limitsProc) (graphPtr, elemPtr,
		    (GraphAxis *)axisPtr, &elemMin, &elemMax) > 0) {
		if (min > elemMin) {
		    min = elemMin;
		}
		if (max < elemMax) {
		    max = elemMax;
		}
	    }
	}
	mask = AXIS_MASK(axisPtr);
	if ((AXIS_MASK_Y & mask) && (graphPtr->mode == MODE_STACKED) &&
	    (graphPtr->numStacks > 0)) {
	    Blt_CheckStacks(graphPtr, mask, &min, &max);
	}
	/*
	 * When auto-scaling, the axis limits are the bounds of the element
	 * data.  If no data exists, set arbitrary limits (wrt to log/linear
	 * scale).
	 */
	if (min == bltPosInfinity) {
	    min = (axisPtr->logScale) ? 0.001 : -10.0;
	}
	if (max == bltNegInfinity) {
	    max = 10.0;
	}
	/*
	 * Handle situations where only one limit is set.
	 */
	value = min;
	if (AXIS_CONFIG_MIN_SET(axisPtr)) {
	    min = value = axisPtr->minLimit;
	} else if (AXIS_CONFIG_MAX_SET(axisPtr)) {
	    max = value = axisPtr->maxLimit;
	}
	/* If there's no range of data (min >= max), manufacture one */
	if (min >= max) {
	    if (value == 0.0) {
		min = -0.1, max = 0.1;
	    } else {
		double x;

		x = Fabs(value * 0.1);
		min = value - x;
		max = value + x;
	    }
	}
	if (!AXIS_CONFIG_MIN_SET(axisPtr)) {
	    axisPtr->minLimit = min;
	}
	if (!AXIS_CONFIG_MAX_SET(axisPtr)) {
	    axisPtr->maxLimit = max;
	}
    }
    /* Indicate if the axis limits have changed */
    result = (axisPtr->maxLimit != axisPtr->prevMax) ||
	(axisPtr->minLimit != axisPtr->prevMin);
    /* and save the previous minimum and maximum values */
    axisPtr->prevMin = axisPtr->minLimit;
    axisPtr->prevMax = axisPtr->maxLimit;
    if (result) {
	axisPtr->flags |= AXIS_CONFIG_DIRTY;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * NiceNum --
 *
 * 	Taken from Paul Heckbert's "Nice Numbers for Graph Labels" in
 * 	Graphics Gems (pp 61-63).  Finds a "nice" number approximately
 * 	equal to x.
 *
 * ----------------------------------------------------------------------
 */
static double
NiceNum(x, round)
    double x;
    int round;			/* If non-zero, round. Otherwise take ceiling
				 * of value. */
{
    double exponX;		/* exponent of x */
    double fractX;		/* fractional part of x */
    double nf;			/* nice, rounded fraction */

    exponX = floor(log10(x));
    fractX = x / BLT_EXP10(exponX);	/* between 1 and 10 */
    if (round) {
	if (fractX < 1.5) {
	    nf = 1.0;
	} else if (fractX < 3.0) {
	    nf = 2.0;
	} else if (fractX < 7.0) {
	    nf = 5.0;
	} else {
	    nf = 10.0;
	}
    } else if (fractX <= 1.0) {
	nf = 1.0;
    } else if (fractX <= 2.0) {
	nf = 2.0;
    } else if (fractX <= 5.0) {
	nf = 5.0;
    } else {
	nf = 10.0;
    }
    return (nf * BLT_EXP10(exponX));
}

static void
GenerateTicks(ticksPtr, startValue, step)
    Ticks *ticksPtr;
    double startValue;
    double step;
{
    if (ticksPtr->numTicks > 0) {
	register int i;
	double *valueArr;
	double value;

	if (ticksPtr->numTicks > STATIC_TICK_SPACE) {
	    valueArr = (double *)ckalloc(sizeof(double) * ticksPtr->numTicks);
	    if (valueArr == NULL) {
		Panic("can't allocate tick value array");
	    }
	} else {
	    valueArr = ticksPtr->defSpace;
	}
	value = startValue;	/* Start from smallest axis tick */
	for (i = 0; i < ticksPtr->numTicks; i++) {
	    valueArr[i] = Round(value / step) * step;
	    value = valueArr[i] + step;
	}
	if (ticksPtr->valueArr != ticksPtr->defSpace) {
	    ckfree((char *)ticksPtr->valueArr);
	}
	ticksPtr->valueArr = valueArr;
    } else {
	if (ticksPtr->valueArr != ticksPtr->defSpace) {
	    ckfree((char *)ticksPtr->valueArr);
	    ticksPtr->valueArr = ticksPtr->defSpace;
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * LogAxis --
 *
 * 	Determine the range and units of a log scaled axis.
 *
 * 	Unless the axis limits are specified, the axis is scaled
 * 	automatically, where the smallest and largest major ticks encompass
 * 	the range of actual data values.  When an axis limit is specified,
 * 	that value represents the smallest(min)/largest(max) value in the
 * 	displayed range of values.
 *
 * 	Both manual and automatic scaling are affected by the step used.  By
 * 	default, the step is the largest power of ten to divide the range in
 * 	more than one piece.
 *
 *	Automatic scaling:
 *	Find the smallest number of units which contain the range of values.
 *	The minimum and maximum major tick values will be represent the
 *	range of values for the axis. This greatest number of major ticks
 *	possible is 10.
 *
 * 	Manual scaling:
 *   	Make the minimum and maximum data values the represent the range of
 *   	the values for the axis.  The minimum and maximum major ticks will be
 *   	inclusive of this range.  This provides the largest area for plotting
 *   	and the expected results when the axis min and max values have be set
 *   	by the user (.e.g zooming).  The maximum number of major ticks is 20.
 *
 *   	For log scale, there is always the possibility that the minimum and
 *   	maximum data values are the same magnitude.  To represent the points
 *   	properly, at least one full decade should be shown.  However, if you
 *   	zoom a log scale plot, the results should be predictable. Therefore,
 *   	in that case, show only minor ticks.  Lastly, there should be an
 *   	appropriate way to handle numbers <=0.
 *
 *          maxY
 *            |    units = magnitude (of least significant digit)
 *            |    high  = largest unit tick < max axis value
 *      high _|    low   = smallest unit tick > min axis value
 *            |
 *            |    range = high - low
 *            |    # ticks = greatest factor of range/units
 *           _|
 *        U   |
 *        n   |
 *        i   |
 *        t  _|
 *            |
 *            |
 *            |
 *       low _|
 *            |
 *            |_minX________________maxX__
 *            |   |       |      |       |
 *     minY  low                        high
 *           minY
 *
 *
 * 	numTicks = Number of ticks
 * 	min = Minimum value of axis
 * 	max = Maximum value of axis
 * 	range    = Range of values (max - min)
 *
 * 	If the number of decades is greater than ten, it is assumed
 *	that the full set of log-style ticks can't be drawn properly.
 *
 * Results:
 *	None
 *
 * ----------------------------------------------------------------------
 */
static void
LogAxis(axisPtr)
    Axis *axisPtr;
{
    double range;
    double min, max;
    double majorStep, minorStep;
    int numMajor, numMinor;
    static float logTable[] =	/* Precomputed log10 values [1..10] */
    {
	0.0f, 0.301f, 0.477f, 0.602f, 0.699f,
	0.778f, 0.845f, 0.903f, 0.954f, 1.0f
    };

    min = axisPtr->minLimit;
    max = axisPtr->maxLimit;
    if (min > 0.0) {
	min = floor(log10(min));
    } else {
	min = 0.0;
    }
    if (max > 0.0) {
	max = ceil(log10(max));
    } else {
	max = 1.0;
    }
    range = max - min;
    if (range > 10) {
	/*
	 * There are too many decades to display every major tick.
	 * Instead, treat the axis as a linear scale.
	 */
	range = NiceNum(range, 0);
	majorStep = NiceNum(range / DEF_NUM_TICKS, 1);

	/* Find the outer limits in terms of the step. */
	min = UFLOOR(min, majorStep);
	max = UCEIL(max, majorStep);
	numMajor = (int)((max - min) / majorStep) + 1;
	minorStep = BLT_EXP10(floor(log10(majorStep)));
	if (minorStep == majorStep) {
	    numMinor = 4;
	    minorStep = 0.2;
	} else {
	    numMinor = Round(majorStep / minorStep) - 1;
	}
    } else {
	if (min == max) {
	    max++;
	}
	majorStep = 1.0;
	/* CHECK THIS. */
	numMajor = (int)((max - min) + 1);
	minorStep = 0.0;
	numMinor = 9;
    }
    axisPtr->min = axisPtr->tickMin = min;
    axisPtr->max = axisPtr->tickMax = max;
    axisPtr->range = (max - min);
    axisPtr->majorStep = majorStep;
    if (axisPtr->majorTicks.genTicks) {
	axisPtr->majorTicks.numTicks = numMajor;
	GenerateTicks(&(axisPtr->majorTicks), min, majorStep);
    }
    if (axisPtr->minorTicks.genTicks) {
	axisPtr->minorTicks.numTicks = numMinor;
	if (minorStep == 0.0) {
	    register int i;

	    for (i = 0; i < 10; i++) {
		axisPtr->minorTicks.valueArr[i] = (double)logTable[i];
	    }
	} else {
	    GenerateTicks(&(axisPtr->minorTicks), minorStep, minorStep);
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * LinearAxis --
 *
 * 	Determine the units of a linear scaled axis.
 *
 * 	Unless the axis limits are specified, the axis is scaled
 * 	automatically, where the smallest and largest major ticks
 * 	encompass the range of actual data values.  When an axis
 * 	limit is specified, that value represents the
 * 	smallest(min)/largest(max) value in the displayed range of
 * 	values.
 *
 * 	Both manual and automatic scaling are affected by the
 * 	step used.  By default, the step is the largest
 * 	power of ten to divide the range in more than one piece.
 *
 * 	Automatic scaling:
 *   	Find the smallest number of units which contain the range of
 *   	values.  The minimum and maximum major tick values will be
 *   	represent the range of values for the axis. This greatest
 *   	number of major ticks possible is 10.
 *
 * 	Manual scaling:
 *   	Make the minimum and maximum data values the represent the
 *   	range of the values for the axis.  The minimum and maximum
 *   	major ticks will be inclusive of this range.  This provides
 *   	the largest area for plotting and the expected results when
 *   	the axis min and max values have be set by the user (.e.g zooming).
 *   	The maximum number of major ticks is 20.
 *
 *   	For log scale, there is always the possibility that the minimum
 *   	and maximum data values are the same magnitude.  To represent
 *   	the points properly, at least one full decade should be shown.
 *   	However, if you zoom a log scale plot, the results should be
 *   	predictable. Therefore, in that case, show only minor ticks.
 *   	Lastly, there should be an appropriate way to handle numbers <=0.
 *
 *          maxY
 *            |    units = magnitude (of least significant digit)
 *            |    high  = largest unit tick < max axis value
 *      high _|    low   = smallest unit tick > min axis value
 *            |
 *            |    range = high - low
 *            |    # ticks = greatest factor of range/units
 *           _|
 *        U   |
 *        n   |
 *        i   |
 *        t  _|
 *            |
 *            |
 *            |
 *       low _|
 *            |
 *            |_minX________________maxX__
 *            |   |       |      |       |
 *     minY  low                        high
 *           minY
 *
 *
 * 	numTicks = Number of ticks
 * 	min = Minimum value of axis
 * 	max = Maximum value of axis
 * 	range    = Range of values (max - min)
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
LinearAxis(axisPtr)
    Axis *axisPtr;
{
    Ticks *ticksPtr;
    double range, pad;
    double min, max;
    double majorStep, minorStep;
    int numMajor, numMinor;

    min = axisPtr->minLimit;
    max = axisPtr->maxLimit;
    range = max - min;

    /*
     * Calculate the major step.  If the user provided a step, use that value,
     * but only if it fits within the current range of the axis.
     */
    if ((axisPtr->reqStep > 0.0) && (axisPtr->reqStep < range)) {
	majorStep = axisPtr->reqStep;
    } else {
	range = NiceNum(range, 0);
	majorStep = NiceNum(range / DEF_NUM_TICKS, 1);
    }

    /*
     * Find the outer tick values in terms of the major step interval.  Add
     * +0.0 to preclude the possibility of an IEEE -0.0.
     */
    axisPtr->tickMin = UFLOOR(min, majorStep) + 0.0;
    axisPtr->tickMax = UCEIL(max, majorStep) + 0.0;
    range = axisPtr->tickMax - axisPtr->tickMin;
    numMajor = Round(range / majorStep) + 1;
    /*
     * If the axis is "loose", the range is between the two outermost ticks.
     * Otherwise if it's "tight", the range is between the data min and max.
     */
    if (axisPtr->loose) {
	axisPtr->min = axisPtr->tickMin, axisPtr->max = axisPtr->tickMax;
    } else {
	axisPtr->min = min, axisPtr->max = max;
    }
    /*
     * If is a limit is auto-scaled, add some padding so that the symbols
     * representing data points at the extremes aren't clipped in half by the
     * edge of the plot.  Two percent is an arbitrary guess.
     */
    pad = (axisPtr->max - axisPtr->min) * 0.02;
    if (!AXIS_CONFIG_MIN_SET(axisPtr)) {
	axisPtr->min -= pad;
    }
    if (!AXIS_CONFIG_MAX_SET(axisPtr)) {
	axisPtr->max += pad;
    }
    axisPtr->range = axisPtr->max - axisPtr->min;
    axisPtr->majorStep = majorStep;
    ticksPtr = &axisPtr->majorTicks;
    if (ticksPtr->genTicks) {
	ticksPtr->numTicks = numMajor;
	GenerateTicks(ticksPtr, axisPtr->tickMin, majorStep);
    }
    /* Now calculate the minor tick step and number. */

    if ((axisPtr->minorTicks.reqNumTicks > 0) &&
	(axisPtr->majorTicks.genTicks)) {
	numMinor = axisPtr->minorTicks.reqNumTicks - 1;
	minorStep = 1.0 / (numMinor + 1);
    } else {
	minorStep = 0.2;
	numMinor = 0;		/* (1.0 / minorStep) - 1; */
    }
    if (axisPtr->minorTicks.genTicks) {
	axisPtr->minorTicks.numTicks = numMinor;
	GenerateTicks(&(axisPtr->minorTicks), minorStep, minorStep);
    }
}

/*
 * -----------------------------------------------------------------
 *
 * SetAxisLimits  --
 *
 * -----------------------------------------------------------------
 */
static void
SetAxisLimits(axisPtr)
    Axis *axisPtr;
{
    GetDataLimits(axisPtr);	/* Get element limits */

    if (axisPtr->flags & AXIS_CONFIG_DIRTY) {
	Graph *graphPtr = axisPtr->graphPtr; 

	/* Calculate min/max tick (major/minor) layouts */
	if (axisPtr->logScale) {
	    LogAxis(axisPtr);
	} else {
	    LinearAxis(axisPtr);
	}
	axisPtr->flags &= ~AXIS_CONFIG_DIRTY;
	/* 
	 * When any axis changes, we need to layout the entire graph. 
	 */
	graphPtr->flags |= (COORDS_WORLD | REDRAW_WORLD);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_ComputeAxes --
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
void
Blt_ComputeAxes(graphPtr)
    Graph *graphPtr;
{
    register int i;

    /* FIXME: This should be called whenever the display list of
     *	      elements change. Maybe yet another flag INIT_STACKS to
     *	      indicate that the element display list has changed.
     *	      Needs to be done before the axis limits are set.  
     */
    Blt_InitFreqTable(graphPtr);
    if ((graphPtr->mode == MODE_STACKED) && (graphPtr->numStacks > 0)) {
	Blt_ComputeStacks(graphPtr);
    }
    for (i = 0; i < 4; i++) {
	SetAxisLimits((Axis *)graphPtr->axisArr[i]);
    }
    graphPtr->flags &= ~RESET_AXES;
    graphPtr->flags |= LAYOUT_NEEDED;
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureAxis --
 *
 *	Configures axis attributes (font, line width, label, etc) and
 *	allocates a new (possibly shared) graphics context.  Line cap
 *	style is projecting.  This is for the problem of when a tick
 *	sits directly at the end point of the axis.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Axis resources are allocated (GC, font). Axis layout is
 *	deferred until the height and width of the window are known.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureAxis(axisPtr, argc, argv, flags)
    Axis *axisPtr;
    int argc;
    char *argv[];
    int flags;
{
    Graph *graphPtr = axisPtr->graphPtr; 
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;
    Tk_ConfigSpec *configSpecs;

    configSpecs = axisConfigSpecs[axisPtr->type];
    if (Tk_ConfigureWidget(graphPtr->interp, graphPtr->tkwin, configSpecs,
	    argc, argv, (char *)axisPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    /*
     * Check requested X and Y axis limits. Can't allow min to be greater than
     * max, or have undefined log scale limits.
     */
    if ((AXIS_CONFIG_MIN_SET(axisPtr)) && (AXIS_CONFIG_MAX_SET(axisPtr)) &&
	    (axisPtr->minLimit >= axisPtr->maxLimit)) {
	char tmpString1[30], tmpString2[30];

	sprintf(tmpString1, "%g", axisPtr->minLimit);
	sprintf(tmpString2, "%g", axisPtr->maxLimit);
	Tcl_SetResult(graphPtr->interp, (char *) NULL, TCL_STATIC);
	Tcl_AppendResult(graphPtr->interp,
	    "impossible ", axisNames[axisPtr->type],
	    "-axis limits (min ", tmpString1, " >= max ", tmpString2, ")",
	    (char *) NULL);
	return TCL_ERROR;
    }
    if ((axisPtr->logScale) && (AXIS_CONFIG_MIN_SET(axisPtr)) &&
	    (axisPtr->minLimit <= 0.0)) {
	char tmpString1[30], tmpString2[30];

	sprintf(tmpString1, "%g", axisPtr->minLimit);
	sprintf(tmpString2, "%g", axisPtr->maxLimit);
	Tcl_SetResult(graphPtr->interp, (char *) NULL, TCL_STATIC);
	Tcl_AppendResult(graphPtr->interp,
	    "invalid ", axisNames[axisPtr->type],
	    "-axis limits (min=", tmpString1, ",max=", tmpString2,
	    ") for log scale", (char *)NULL);
	return TCL_ERROR;
    }
    gcMask = (GCForeground | GCFont | GCBackground);
    gcValues.font = axisPtr->titleFontPtr->fid;
    gcValues.foreground = axisPtr->fgColorPtr->pixel;
    gcValues.background = Tk_3DBorderColor(graphPtr->border)->pixel;
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (axisPtr->titleGC != NULL) {
	Tk_FreeGC(graphPtr->display, axisPtr->titleGC);
    }
    axisPtr->titleGC = newGC;

    if ((axisPtr->mapped) && (axisPtr->title != NULL)) {
	Blt_GetTextExtents(axisPtr->titleFontPtr, axisPtr->title,
	    &(axisPtr->titleWidth), &(axisPtr->titleHeight));
	axisPtr->titleHeight += 2 * TITLE_PAD;	/* Pad title height */
    } else {
	axisPtr->titleWidth = axisPtr->titleHeight = 0;
    }

    gcMask |= (GCLineWidth | GCCapStyle);
    gcValues.font = axisPtr->tickFontPtr->fid;
    gcValues.line_width = axisPtr->lineWidth;
    gcValues.cap_style = CapProjecting;

    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (axisPtr->gc != NULL) {
	Tk_FreeGC(graphPtr->display, axisPtr->gc);
    }
    axisPtr->gc = newGC;

    axisPtr->theta = BLT_FMOD(axisPtr->rotate, 360.0);
    if (axisPtr->theta < 0.0) {
	axisPtr->theta += 360.0;
    }
    /*
     * Don't bother to check what options have changed.  Almost every axis
     * configuration option changes the size of the plotting area (except for
     * -foreground).
     *
     * Recompute the scale and offset of the axis in case -min, -max options
     * have changed.
     */
    graphPtr->flags |= (REDRAW_WORLD | COORDS_WORLD | RESET_AXES);
    axisPtr->flags |= AXIS_CONFIG_DIRTY;
    Blt_RedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyAxis --
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources (font, color, gc, labels, etc.) associated with the
 *	axis are deallocated.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyAxis(axis)
    GraphAxis *axis;
{
    Axis *axisPtr = (Axis *)axis;
    Graph *graphPtr = axisPtr->graphPtr; 

    Tk_FreeOptions(axisConfigSpecs[axisPtr->type], (char *)axisPtr,
	graphPtr->display, 0);

    if (axisPtr->titleGC != NULL) {
	Tk_FreeGC(graphPtr->display, axisPtr->titleGC);
    }
    if (axisPtr->gc != NULL) {
	Tk_FreeGC(graphPtr->display, axisPtr->gc);
    }
    if (axisPtr->labelArr != NULL) {
	ckfree((char *)axisPtr->labelArr);
    }
    if (axisPtr->segArr != NULL) {
	ckfree((char *)axisPtr->segArr);
    }
    if (axisPtr->majorTicks.valueArr != axisPtr->majorTicks.defSpace) {
	ckfree((char *)axisPtr->majorTicks.valueArr);
    }
    if (axisPtr->minorTicks.valueArr != axisPtr->minorTicks.defSpace) {
	ckfree((char *)axisPtr->minorTicks.valueArr);
    }
    ckfree((char *)axisPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * CalculateOffsets --
 *
 *	Determines the sites of the axis, major and minor ticks,
 *	and title of the axis.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
CalculateOffsets(axisPtr)
    Axis *axisPtr;
{
    Graph *graphPtr = axisPtr->graphPtr; 
    int pad;			/* Offset of axis from interior region. This
				 * includes a possible border and the axis
				 * line width. */
    int innerPos, outerPos;
    int majorOffset, minorOffset, labelOffset;

    majorOffset = BLT_ABS(axisPtr->tickLength);
    minorOffset = Round(majorOffset * 0.5);
    labelOffset = Round(majorOffset * 1.4) + axisPtr->lineWidth / 2;

    /* Adjust offset for the interior border width and the line width */
    pad = graphPtr->plotBW + axisPtr->lineWidth + 2;
    if (graphPtr->plotBW > 0) {
	pad++;
    }
    if ((axisPtr->site == AXIS_SITE_LEFT) ||
	(axisPtr->site == AXIS_SITE_TOP)) {
	majorOffset = -majorOffset;
	minorOffset = -minorOffset;
	labelOffset = -labelOffset;
    }
    /*
     * Pre-calculate the x-coordinate positions of the axis, tick labels, and
     * the individual major and minor ticks.
     */
    innerPos = 0;		/* Suppress compiler warning */
    switch (axisPtr->site) {
    case AXIS_SITE_BOTTOM:
	innerPos = Round(graphPtr->lly + (double)pad);
	axisPtr->titleX = Round((graphPtr->urx + graphPtr->llx) * 0.5);
	axisPtr->titleY =
	    Round(graphPtr->lly + (double)(axisPtr->height + TITLE_PAD));
	axisPtr->anchor = TK_ANCHOR_N;
	break;

    case AXIS_SITE_LEFT:
	innerPos = Round(graphPtr->llx - (double)pad);
	axisPtr->titleX = Round(graphPtr->llx - (axisPtr->width + TITLE_PAD));
	axisPtr->titleY = Round((graphPtr->lly + graphPtr->ury) * 0.5);
	axisPtr->anchor = TK_ANCHOR_E;
	break;

    case AXIS_SITE_TOP:
	innerPos = Round(graphPtr->ury - (double)pad);
	axisPtr->titleX = Round((graphPtr->urx + graphPtr->llx) * 0.5);
	axisPtr->titleY = Round(graphPtr->ury - 
				(double)(axisPtr->height + TITLE_PAD));
	axisPtr->anchor = TK_ANCHOR_S;
	break;

    case AXIS_SITE_RIGHT:
	innerPos = Round(graphPtr->urx + (double)pad);
	axisPtr->titleX =
	    Round(graphPtr->urx + (double)(axisPtr->width + TITLE_PAD));
	axisPtr->titleY = Round((graphPtr->lly + graphPtr->ury) * 0.5);
	axisPtr->anchor = TK_ANCHOR_W;
	break;

    }
    outerPos = innerPos + majorOffset;
    axisPtr->posArr[MAJOR_TICK] = outerPos;
    axisPtr->posArr[AXIS_LINE] = innerPos - (axisPtr->lineWidth / 2);
    axisPtr->posArr[MINOR_TICK] = innerPos + minorOffset;
    axisPtr->posArr[TICK_LABEL] = innerPos + labelOffset;
    if (axisPtr->tickLength < 0) {
	int hold;

	hold = axisPtr->posArr[MAJOR_TICK];
	axisPtr->posArr[MAJOR_TICK] = axisPtr->posArr[AXIS_LINE];
	axisPtr->posArr[AXIS_LINE] = hold;
    }
}

static XSegment
AxisLine(axisPtr, min, max)
    Axis *axisPtr;		/* Axis information */
    double min, max;		/* Limits of axis in graph coordinates */
{
    double normMin, normMax;
    XSegment segment;

    normMax = NORMALIZE(axisPtr, min);
    if (axisPtr->descending) {
	normMax = 1.0 - normMax;
    }
    normMin = NORMALIZE(axisPtr, max);
    if (axisPtr->descending) {
	normMin = 1.0 - normMin;
    }
    if (Horizontal(axisPtr)) {
	segment.y1 = segment.y2 = axisPtr->posArr[AXIS_LINE];
	segment.x1 = WINX(axisPtr, normMin);
	segment.x2 = WINX(axisPtr, normMax);
    } else {
	segment.x1 = segment.x2 = axisPtr->posArr[AXIS_LINE];
	segment.y1 = WINY(axisPtr, normMin);
	segment.y2 = WINY(axisPtr, normMax);
    }
    return (segment);
}


static XSegment
Tick(axisPtr, value, whichTick)
    Axis *axisPtr;
    double value;
    AxisComponent whichTick;	/* MAJOR_TICK or MINOR_TICK */
{
    double norm;
    XSegment segment;
    int tick;

    norm = NORMALIZE(axisPtr, value);
    if (axisPtr->descending) {
	norm = 1.0 - norm;
    }
    tick = axisPtr->posArr[(int)whichTick];
    if (Horizontal(axisPtr)) {
	segment.y1 = axisPtr->posArr[AXIS_LINE];
	segment.y2 = tick;
	segment.x1 = segment.x2 = WINX(axisPtr, norm);
    } else {
	segment.x1 = axisPtr->posArr[AXIS_LINE];
	segment.x2 = tick;
	segment.y1 = segment.y2 = WINY(axisPtr, norm);
    }
    return (segment);
}

/*
 * -----------------------------------------------------------------
 *
 * ComputeAxisCoordinates --
 *
 *	Pre-calculate the x-coordinate positions of the axis, ticks
 *	and labels to be used later when displaying the X axis.  Ticks
 *	(minor and major) will be saved in an array of XSegments so
 *	that they can be drawn in one XDrawSegments call. The strings
 *	representing the tick labels and the corresponding window
 *	positions are saved in an array of Label's.
 *
 *      Calculates the values for each major and minor tick and checks to
 *	see if they are in range (the outer ticks may be outside of the
 *	range of plotted values).
 *
 * Results:
 *	None.
 *
 * SideEffects:
 *	Line segments and tick labels saved will be used to draw
 *	the X axis.
 * -----------------------------------------------------------------
 */
static void
ComputeAxisCoordinates(axis)
    GraphAxis *axis;
{
    Axis *axisPtr = (Axis *)axis;
    int arraySize;
    double minAxis, maxAxis;
    register int sgmts, labels;

    CalculateOffsets(axisPtr);

    /* Save all line coordinates in an array of line segments. */

    if (axisPtr->segArr != NULL) {
	ckfree((char *)axisPtr->segArr);
    }
    arraySize = 1 + (axisPtr->majorTicks.numTicks *
	(axisPtr->minorTicks.numTicks + 1));
    axisPtr->segArr = (XSegment *)ckalloc(arraySize * sizeof(XSegment));
    if (axisPtr->segArr == NULL) {
	return;			/* Can't allocate array of segments */
    }
    if ((axisPtr->logScale) || (axisPtr->loose) ||
	(axisPtr->minLimit == axisPtr->maxLimit)) {
	minAxis = axisPtr->tickMin, maxAxis = axisPtr->tickMax;
    } else {
	minAxis = axisPtr->minLimit, maxAxis = axisPtr->maxLimit;
    }

    /* Axis baseline */
    axisPtr->segArr[0] = AxisLine(axisPtr, minAxis, maxAxis);

    sgmts = 1, labels = 0;
    if (axisPtr->showTicks) {
	double value, subValue;
	short int labelPos;
	register int i, j;

	assert(axisPtr->majorTicks.valueArr != NULL);
	for (i = 0; i < axisPtr->majorTicks.numTicks; i++) {
	    value = axisPtr->majorTicks.valueArr[i];

	    /* Minor ticks */
	    for (j = 0; j < axisPtr->minorTicks.numTicks; j++) {
		subValue = value + (axisPtr->majorStep *
		    axisPtr->minorTicks.valueArr[j]);
		if (OutOfRange(subValue, minAxis, maxAxis)) {
		    continue;
		}
		axisPtr->segArr[sgmts] = Tick(axisPtr, subValue, MINOR_TICK);
		sgmts++;
	    }
	    if (OutOfRange(value, minAxis, maxAxis)) {
		continue;
	    }
	    /* Major tick and label position */

	    axisPtr->segArr[sgmts] = Tick(axisPtr, value, MAJOR_TICK);
	    labelPos = (short int)axisPtr->posArr[TICK_LABEL];

	    /* Save tick label position */

	    if (Horizontal(axisPtr)) {
		axisPtr->labelArr[labels].x = axisPtr->segArr[sgmts].x1;
		axisPtr->labelArr[labels].y = labelPos;
	    } else {
		axisPtr->labelArr[labels].x = labelPos;
		axisPtr->labelArr[labels].y = axisPtr->segArr[sgmts].y1;
	    }
	    sgmts++, labels++;
	}
    }
    assert(sgmts <= arraySize);
    assert(labels <= axisPtr->numLabels);
    axisPtr->numSegments = sgmts;
}

/*
 * -----------------------------------------------------------------
 *
 * DrawAxis --
 *
 *	Draws the axis, ticks, and labels onto the canvas.
 *
 *	Initializes and passes text attribute information through
 *	TextAttributes structure.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Axis gets drawn on window.
 *
 * -----------------------------------------------------------------
 */

static float titleRot[4] =	/* Rotation for each axis title */
{
    0.0f, 90.0f, 0.0f, 270.0f
};

static void
DrawAxis(axis, attrPtr)
    GraphAxis *axis;
    TextAttributes *attrPtr;
{
    Axis *axisPtr = (Axis *)axis;
    Graph *graphPtr = axisPtr->graphPtr; 

    if (axisPtr->title != NULL) {
	attrPtr->theta = (double)titleRot[axisPtr->site];
	attrPtr->justify = axisPtr->justify;
	attrPtr->anchor = axisPtr->anchor;
	attrPtr->textGC = axisPtr->titleGC;
	attrPtr->fontPtr = axisPtr->titleFontPtr;
	Blt_DrawText(graphPtr->tkwin, graphPtr->pixwin, axisPtr->title,
	    attrPtr, axisPtr->titleX, axisPtr->titleY);
    }
    if (axisPtr->showTicks) {
	register int i;
	TextAttributes textAttr;

	Blt_InitTextAttrs(&textAttr, axisPtr->fgColorPtr,
	    Tk_3DBorderColor(graphPtr->border), axisPtr->tickFontPtr,
	    axisPtr->theta, axisPtr->anchor, TK_JUSTIFY_CENTER);

	textAttr.fillGC = graphPtr->fillGC;
	textAttr.textGC = axisPtr->gc;
	if (graphPtr->tile != NULL) {
	    textAttr.bgColorPtr = NULL;
	}

	/* Draw each major tick label */

	for (i = 0; i < axisPtr->numLabels; i++) {
	    Blt_DrawText(graphPtr->tkwin, graphPtr->pixwin,
		axisPtr->labelArr[i].text, &textAttr,
		axisPtr->labelArr[i].x, axisPtr->labelArr[i].y);
	}
    }
    if (axisPtr->numSegments > 0) {
	/* Draw the tick marks */
	XDrawSegments(graphPtr->display, graphPtr->pixwin, axisPtr->gc,
	    axisPtr->segArr, axisPtr->numSegments);
    }
}

/*
 * -----------------------------------------------------------------
 *
 * PrintAxis --
 *
 *	Generates PostScript output to draw the axis, ticks, and
 *	labels.
 *
 *	Initializes and passes text attribute information through
 *	TextAttributes structure.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	PostScript output is left in graphPtr->interp->result;
 *
 * -----------------------------------------------------------------
 */
static void
PrintAxis(axis, attrPtr)
    GraphAxis *axis;
    TextAttributes *attrPtr;
{
    Axis *axisPtr = (Axis *)axis;
    Graph *graphPtr = axisPtr->graphPtr; 

    if (axisPtr->title != NULL) {
	attrPtr->theta = (double)titleRot[axisPtr->site];
	attrPtr->justify = axisPtr->justify;
	attrPtr->anchor = axisPtr->anchor;
	Blt_PrintText(graphPtr, axisPtr->title, attrPtr, axisPtr->titleX,
	    axisPtr->titleY);
    }
    if (axisPtr->showTicks) {
	TextAttributes textAttr;
	register int i;

	Blt_InitTextAttrs(&textAttr, axisPtr->fgColorPtr, (XColor *)NULL,
	    axisPtr->tickFontPtr, axisPtr->theta, axisPtr->anchor,
	    TK_JUSTIFY_CENTER);
	for (i = 0; i < axisPtr->numLabels; i++) {
	    Blt_PrintText(graphPtr, axisPtr->labelArr[i].text, &textAttr,
		axisPtr->labelArr[i].x, axisPtr->labelArr[i].y);
	}
    }
    if (axisPtr->numSegments > 0) {
	Blt_SetLineAttributes(graphPtr, axisPtr->fgColorPtr,
	    axisPtr->lineWidth, (Dashes *)NULL);
	Blt_SegmentsToPostScript(graphPtr, axisPtr->segArr,
	    axisPtr->numSegments);
    }
}

static XSegment
GridLine(axisPtr, value)
    Axis *axisPtr;
    double value;
{
    Graph *graphPtr = axisPtr->graphPtr; 
    XSegment segment;
    double norm;

    norm = NORMALIZE(axisPtr, value);
    if (axisPtr->descending) {
	norm = 1.0 - norm;
    }
    if (Horizontal(axisPtr)) {
	segment.y1 = Round(graphPtr->lly);
	segment.y2 = Round(graphPtr->ury);
	segment.x1 = segment.x2 = WINX(axisPtr, norm);
    } else {
	segment.x1 = Round(graphPtr->llx);
	segment.x2 = Round(graphPtr->urx);
	segment.y1 = segment.y2 = WINY(axisPtr, norm);
    }
    return (segment);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetGridSegments --
 *
 *	Draws the grid lines associated with each axis.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_GetGridSegments(graphPtr, segPtrPtr, numSegPtr)
    Graph *graphPtr;
    XSegment **segPtrPtr;
    int *numSegPtr;
{
    register int count;
    register int n;
    Axis *axisPtr;
    int needed;

    count = needed = 0;
    for (n = 0; n < 4; n++) {
	axisPtr = (Axis *)graphPtr->axisArr[n];
	needed += axisPtr->majorTicks.numTicks;
	if (graphPtr->gridPtr->minorGrid) {
	    needed += (axisPtr->majorTicks.numTicks *
		axisPtr->minorTicks.numTicks);
	}
    }
    if (needed > 0) {
	register int i;
	double minAxis, maxAxis;
	double value;
	XSegment *segArr;

	segArr = (XSegment *)ckalloc(sizeof(XSegment) * needed);
	if (segArr == NULL) {
	    Panic("can't allocate grid segments");
	}
	for (n = 0; n < 4; n++) {
	    axisPtr = (Axis *)graphPtr->axisArr[n];
	    if (!(graphPtr->gridPtr->axisMask & AXIS_MASK(axisPtr))) {
		continue;
	    }
	    if ((axisPtr->logScale) || (axisPtr->loose) ||
		(axisPtr->minLimit == axisPtr->maxLimit)) {
		minAxis = axisPtr->tickMin, maxAxis = axisPtr->tickMax;
	    } else {
		minAxis = axisPtr->minLimit, maxAxis = axisPtr->maxLimit;
	    }
	    for (i = 0; i < axisPtr->majorTicks.numTicks; i++) {
		value = axisPtr->majorTicks.valueArr[i];

		if (graphPtr->gridPtr->minorGrid) {
		    register int j;
		    double subValue;

		    for (j = 0; j < axisPtr->minorTicks.numTicks; j++) {
			subValue = value + (axisPtr->majorStep *
			    axisPtr->minorTicks.valueArr[j]);
			if (OutOfRange(subValue, minAxis, maxAxis)) {
			    continue;
			}
			segArr[count] = GridLine(axisPtr, subValue);
			count++;
		    }
		}
		if (OutOfRange(value, minAxis, maxAxis)) {
		    continue;
		}
		segArr[count] = GridLine(axisPtr, value);
		count++;
	    }
	}
	if (count == 0) {
	    ckfree((char *)segArr);
	} else {
	    *segPtrPtr = segArr;
	}
    }
    assert(count <= needed);
    *numSegPtr = count;
}

/*
 *----------------------------------------------------------------------
 *
 * GetAxisGeometry --
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
GetAxisGeometry(axisPtr)
    Axis *axisPtr;
{
    Graph *graphPtr = axisPtr->graphPtr; 
    register int i, numTickLabels;
    int numTicks;
    char label[MAX_TICK_LABEL + 1];
    int length;
    int arraySize, poolSize, used;
    char *strPool;
    int textWidth, textHeight;
    int bbWidth, bbHeight;
    int maxWidth, maxHeight;
    double value;
    double minAxis, maxAxis;
    TickLabel *newArr;
    int pad;

    /* Release any previously generated ticks labels */
    if (axisPtr->labelArr != NULL) {
	ckfree((char *)axisPtr->labelArr);
	axisPtr->numLabels = 0, axisPtr->labelArr = NULL;
    }
    axisPtr->width = axisPtr->height = 0;

    /*
     * First handle the two special cases where the axis is either unmapped
     * (-mapped false) or no ticks are being shown (-showticks false).
     */
    if (!axisPtr->mapped) {
	return;			/* Axis is unmapped, dimensions are zero */
    }
    if (!axisPtr->showTicks) {
	if (Horizontal(axisPtr)) {
	    axisPtr->height = axisPtr->lineWidth + TITLE_PAD;
	} else {
	    axisPtr->width = axisPtr->lineWidth + TITLE_PAD;
	}
	return;			/* Leave room for axis baseline (and pad) */
    }

    /*
     * Determine the limits of the axis, considering how the axis is the be
     * layed out (loose or tight).  We'll use this information to generate
     * ticks (and subticks) within this range.
     */
    if ((axisPtr->logScale) || (axisPtr->loose) ||
	(axisPtr->minLimit == axisPtr->maxLimit)) {
	minAxis = axisPtr->tickMin, maxAxis = axisPtr->tickMax;
    } else {
	minAxis = axisPtr->minLimit, maxAxis = axisPtr->maxLimit;
    }

    numTicks = axisPtr->majorTicks.numTicks;
    /* Hey Sani, does this check fail under AIX? */
    assert((numTicks >= 0) && (numTicks < 100000));

    /* Create an array of labels with an attached string of characters */
    arraySize = numTicks * sizeof(TickLabel);
    poolSize = numTicks * AVG_TICK_NUM_CHARS * sizeof(char);
    newArr = (TickLabel *)ckalloc(arraySize + poolSize);
    if (newArr == NULL) {
	Panic("can't allocate tick label array");
    }
    strPool = (char *)newArr + arraySize;

    maxHeight = maxWidth = 0;
    used = numTickLabels = 0;
    for (i = 0; i < numTicks; i++) {
	value = axisPtr->majorTicks.valueArr[i];
	if (OutOfRange(value, minAxis, maxAxis)) {
	    continue;
	}
	MakeLabel(axisPtr, value, label);
	length = strlen(label);

	if (poolSize <= (used + length)) {
	    int newSize;
	    /*
	     * Resize the label array when we overflow its string pool
	     */
	    newSize = poolSize;
	    do {
		newSize += newSize;
	    } while (newSize <= (used + length));
	    newArr = (TickLabel *)ckrealloc((char *)newArr,arraySize + newSize);
	    if (newArr == NULL) {
		Panic("can't reallocate tick label array");
	    }
	    strPool = (char *)newArr + arraySize;
	    poolSize = newSize;
	}
	newArr[numTickLabels].text = strPool + used;
	strcpy(newArr[numTickLabels].text, label);
	used += (length + 1);
	numTickLabels++;

	/*
	 * Compute the dimensions of the tick label.  Remember that the
	 * tick label can be multi-lined and/or rotated.
	 */
	Blt_GetTextExtents(axisPtr->tickFontPtr, label, &textWidth,&textHeight);
	if (axisPtr->theta == 0.0) {
	    bbWidth = textWidth, bbHeight = textHeight;
	} else {
	    Blt_GetBoundingBox(textWidth, textHeight, axisPtr->theta,
		&bbWidth, &bbHeight, (XPoint *)NULL);
	}
	if (bbWidth > maxWidth) {
	    maxWidth = bbWidth;
	}
	if (bbHeight > maxHeight) {
	    maxHeight = bbHeight;
	}
    }
    axisPtr->labelArr = newArr;
    axisPtr->numLabels = numTickLabels;
    assert(axisPtr->numLabels <= numTicks);

    /*
     * Because the axis cap style is "CapProjecting", there's an extra 1.5
     * linewidth to be accounted for at the ends of each line.
     */
    pad = ((axisPtr->lineWidth * 15) / 10) + 2;
    axisPtr->width = maxWidth + pad;
    axisPtr->height = maxHeight + pad;

    value = BLT_ABS(axisPtr->tickLength) * 1.4;
    pad = Round(value) + graphPtr->plotBW + 1;
    if (graphPtr->plotBW > 0) {
	pad++;
    }
    if (Horizontal(axisPtr)) {
	axisPtr->height += pad;
    } else {
	axisPtr->width += pad;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_UpdateAxisBackgrounds --
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_UpdateAxisBackgrounds(graphPtr)
    Graph *graphPtr;
{
    Axis *axisPtr;
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;
    int i;

    gcValues.background = Tk_3DBorderColor(graphPtr->border)->pixel;
    gcValues.cap_style = CapProjecting;
    gcMask = (GCForeground | GCBackground | GCFont | GCLineWidth | GCCapStyle);
    /*
     * For each axis, update the text GC with the current background.
     */
    for (i = 0; i < 4; i++) {
	axisPtr = (Axis *)graphPtr->axisArr[i];

	gcValues.foreground = axisPtr->fgColorPtr->pixel;
	gcValues.line_width = axisPtr->lineWidth;

	gcValues.font = axisPtr->tickFontPtr->fid;
	newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
	if (axisPtr->gc != NULL) {
	    Tk_FreeGC(graphPtr->display, axisPtr->gc);
	}
	axisPtr->gc = newGC;

	gcValues.font = axisPtr->titleFontPtr->fid;
	newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
	if (axisPtr->titleGC != NULL) {
	    Tk_FreeGC(graphPtr->display, axisPtr->titleGC);
	}
	axisPtr->titleGC = newGC;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetAxisMargin --
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int
Blt_GetAxisMargin(axis)
    GraphAxis *axis;
{
    Axis *axisPtr = (Axis *)axis;
    int margin;

    margin = axisPtr->titleHeight +
	(Horizontal(axisPtr) ? axisPtr->height : axisPtr->width + PADX);
    return (margin);
}

/*
 *----------------------------------------------------------------------
 *
 * DefaultMargin --
 *
 *	Computes the size of the margin along the axis.  The axis
 *	geometry is computed if necessary.
 *
 * Results:
 *	Returns the default size of the margin (left, right, top,
 *	or bottom).
 *
 *----------------------------------------------------------------------
 */
static int
DefaultMargin(axis)
    GraphAxis *axis;
{
    int margin;
    Axis *axisPtr = (Axis *)axis;
    Graph *graphPtr = axisPtr->graphPtr; 

    if (graphPtr->flags & GET_AXIS_GEOMETRY) {
	GetAxisGeometry(axisPtr);
    }
    margin = (Horizontal(axisPtr) ? axisPtr->height : axisPtr->width + PADX);
    return (margin);
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeMargins --
 *
 * Results:
 *
 *----------------------------------------------------------------------
 */
static void
ComputeMargins(graphPtr)
    Graph *graphPtr;
{
    int left, right, top, bottom;
    int defLeft, defRight, defTop, defBot;
    int margin, width, height;
    int borderWidths;

    defTop = DefaultMargin(graphPtr->topAxis);
    defBot = DefaultMargin(graphPtr->bottomAxis);
    defLeft = DefaultMargin(graphPtr->leftAxis);
    defRight = DefaultMargin(graphPtr->rightAxis);

    top = BLT_MAX(defTop + TitleHeight(graphPtr->topAxis), defBot);
    bottom = BLT_MAX(defBot + TitleHeight(graphPtr->bottomAxis), defTop);
    left = BLT_MAX(defLeft + TitleHeight(graphPtr->leftAxis), defRight);
    right = BLT_MAX(defRight + TitleHeight(graphPtr->rightAxis), defLeft);

    top = BLT_MAX(top, MINIMUM_MARGIN);
    bottom = BLT_MAX(bottom, MINIMUM_MARGIN);
    left = BLT_MAX(left, MINIMUM_MARGIN);
    right = BLT_MAX(right, MINIMUM_MARGIN);

    if (graphPtr->title != NULL) {
	if (graphPtr->topAxis->mapped) {
	    top += graphPtr->titleHeight;
	} else {
	    top = BLT_MAX(top, graphPtr->titleHeight);
	}
    }
    borderWidths = graphPtr->borderWidth + graphPtr->plotBW;
    width = graphPtr->width - ((2 * borderWidths) + left + right);
    height = graphPtr->height - ((2 * borderWidths) + top + bottom);
    (*graphPtr->legendPtr->geomProc) (graphPtr, width, height);

    if (graphPtr->legendPtr->mapped) {
	width = graphPtr->legendPtr->width + TITLE_PAD;
	height = graphPtr->legendPtr->height + TITLE_PAD;
	switch (Blt_LegendSite(graphPtr)) {
	case LEGEND_SITE_RIGHT:
	    if (graphPtr->rightAxis->mapped) {
		right += width;
	    } else {
		right = BLT_MAX(right, width);
	    }
	    break;
	case LEGEND_SITE_LEFT:
	    if (graphPtr->leftAxis->mapped) {
		left += width;
	    } else {
		left = BLT_MAX(left, width);
	    }
	    break;
	case LEGEND_SITE_TOP:
	    margin = height + graphPtr->titleHeight +
		Blt_GetAxisMargin(graphPtr->topAxis);
	    top = BLT_MAX(top, margin);
	    break;
	case LEGEND_SITE_BOTTOM:
	    margin = height + Blt_GetAxisMargin(graphPtr->bottomAxis);
	    bottom = BLT_MAX(bottom, margin);
	    break;
	case LEGEND_SITE_XY:
	case LEGEND_SITE_PLOT:
	    break;
	}
    }
    /* Override calculated values if user specified margins */

    if (graphPtr->reqLeftMargin > 0) {
	left = graphPtr->reqLeftMargin;
    }
    if (graphPtr->reqTopMargin > 0) {
	top = graphPtr->reqTopMargin;
    }
    if (graphPtr->reqBottomMargin > 0) {
	bottom = graphPtr->reqBottomMargin;
    }
    if (graphPtr->reqRightMargin > 0) {
	right = graphPtr->reqRightMargin;
    }
    graphPtr->topMargin = top + borderWidths;
    graphPtr->leftMargin = left + borderWidths;
    graphPtr->rightMargin = right + borderWidths;
    graphPtr->bottomMargin = bottom + borderWidths;
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_LayoutGraph --
 *
 * 	Calculate the layout of the graph.  Based upon the data,
 *	axis limits, X and Y titles, and title height, determine
 *	the cavity left which is the plotting surface.  The first
 *	step get the data and axis limits for calculating the space
 *	needed for the top, bottom, left, and right margins.
 *
 * 	1) The LEFT margin is the area from the left border to the
 *	   Y axis (not including ticks). It composes the border
 *	   width, the width an optional Y axis label and its padding,
 *	   and the tick numeric labels. The Y axis label is rotated
 *	   90 degrees so that the width is the font height.
 *
 * 	2) The RIGHT margin is the area from the end of the graph
 *	   to the right window border. It composes the border width,
 *	   some padding, the font height (this may be dubious. It
 *	   appears to provide a more even border), the max of the
 *	   legend width and 1/2 max X tick number. This last part is
 *	   so that the last tick label is not clipped.
 *
 *           Area Width
 *      ___________________________________________________________
 *      |          |                               |               |
 *      |          |   TOP  height of title        |               |
 *      |          |                               |               |
 *      |          |           x2 title            |               |
 *      |          |                               |               |
 *      |          |        height of x2-axis      |               |
 *      |__________|_______________________________|_______________|  A
 *      |          |                        extreme|               |  r
 *      |          |                               |               |  e
 *      |   LEFT   |                               |     RIGHT     |  a
 *      |          |                               |               |
 *      | y        |     Free area = 104%          |      y2       |  H
 *      |          |     Plotting surface = 100%   |               |  e
 *      | t        |     Tick length = 2 + 2%      |      t        |  i
 *      | i        |                               |      i        |  g
 *      | t        |                               |      t  legend|  h
 *      | l        |                               |      l   width|  t
 *      | e        |                               |      e        |
 *      |    height|                               |height         |
 *      |       of |                               | of            |
 *      |    y-axis|                               |y2-axis        |
 *      |          |                               |               |
 *      |          |origin                         |               |
 *      |__________|_______________________________|_______________|
 *      |          | (xoffset, yoffset)            |               |
 *      |          |                               |               |
 *      |          |       height of x-axis        |               |
 *      |          |                               |               |
 *      |          |   BOTTOM   x title            |               |
 *      |__________|_______________________________|_______________|
 *
 * 3) The TOP margin is the area from the top window border to the top
 *    of the graph. It composes the border width, twice the height of
 *    the title font (if one is given) and some padding between the
 *    title.
 *
 * 4) The BOTTOM margin is area from the bottom window border to the
 *    X axis (not including ticks). It composes the border width, the height
 *    an optional X axis label and its padding, the height of the font
 *    of the tick labels.
 *
 * The plotting area is between the margins which includes the X and Y axes
 * including the ticks but not the tick numeric labels. The length of
 * the ticks and its padding is 5% of the entire plotting area.  Hence the
 * entire plotting area is scaled as 105% of the width and height of the
 * area.
 *
 * The axis labels, ticks labels, title, and legend may or may not be
 * displayed which must be taken into account.
 *
 *
 * -----------------------------------------------------------------
 */
void
Blt_LayoutGraph(graphPtr)
    Graph *graphPtr;
{
    int leftOver;
    int titleY;
    int borderWidths;
    Axis *x1, *x2, *y1, *y2;

    ComputeMargins(graphPtr);

    x1 = (Axis *)graphPtr->bottomAxis;
    x2 = (Axis *)graphPtr->topAxis;
    y1 = (Axis *)graphPtr->leftAxis;
    y2 = (Axis *)graphPtr->rightAxis;

    /* Based upon the margins, calculate the space left for the graph. */
    borderWidths = graphPtr->borderWidth + graphPtr->plotBW;

    x1->offset = graphPtr->leftMargin;
    y1->offset = graphPtr->height - graphPtr->bottomMargin;
    leftOver = graphPtr->width -
	(graphPtr->leftMargin + graphPtr->rightMargin);
    if (leftOver < 1) {
	leftOver = 1;
    }
    x1->scale = (double)leftOver;	/* Pixels per X unit */
    leftOver = graphPtr->height -
	(graphPtr->topMargin + graphPtr->bottomMargin);
    if (leftOver < 1) {
	leftOver = 1;
    }
    y1->scale = (double)leftOver;	/* Pixels per Y unit */

    x2->scale = x1->scale, x2->offset = x1->offset;
    y2->scale = y1->scale, y2->offset = y1->offset;

    graphPtr->llx = (double)WINX(x1, 0.0);
    graphPtr->lly = (double)WINY(y1, 0.0);
    graphPtr->urx = (double)WINX(x1, 1.0);
    graphPtr->ury = (double)WINY(y1, 1.0);

    /*
     * Calculate the placement of the graph title so it is centered within the
     * space provided for it in the top margin
     */
    titleY = graphPtr->topMargin -
	(borderWidths + Blt_GetAxisMargin(graphPtr->topAxis));
    if ((graphPtr->legendPtr->mapped) &&
	(Blt_LegendSite(graphPtr) == LEGEND_SITE_TOP)) {
	titleY -= graphPtr->legendPtr->height;
    }
    graphPtr->titleY = (titleY / 2) + graphPtr->borderWidth;
    graphPtr->titleX = Round((graphPtr->urx + graphPtr->llx) * 0.5);

    graphPtr->plotArea.x = (int)graphPtr->llx;
    graphPtr->plotArea.y = (int)graphPtr->ury;
    graphPtr->plotArea.width = (int)(graphPtr->urx - graphPtr->llx);
    graphPtr->plotArea.height = (int)(graphPtr->lly - graphPtr->ury);
}

/*
 * ----------------------------------------------------------------------
 *
 * CgetOper --
 *
 *	Queries axis attributes (font, line width, label, etc).
 *
 * Results:
 *	Return value is a standard Tcl result.  If querying configuration
 *	values, interp->result will contain the results.
 *
 * ----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
CgetOper(axisPtr, argc, argv)
    Axis *axisPtr;
    int argc;
    char *argv[];
{
    Graph *graphPtr = axisPtr->graphPtr; 
    int flags;
    Tk_ConfigSpec *configSpecs;

    configSpecs = axisConfigSpecs[axisPtr->type];
    flags = (GRAPH_TYPE_BIT << graphPtr->defElemType);
    return (Tk_ConfigureValue(graphPtr->interp, graphPtr->tkwin, configSpecs,
	    (char *)axisPtr, argv[3], flags));
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureOper --
 *
 *	Queries or resets axis attributes (font, line width, label, etc).
 *
 * Results:
 *	Return value is a standard Tcl result.  If querying configuration
 *	values, interp->result will contain the results.
 *
 * Side Effects:
 *	Axis resources are possibly allocated (GC, font). Axis layout is
 *	deferred until the height and width of the window are known.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureOper(axisPtr, argc, argv)
    Axis *axisPtr;
    int argc;
    char *argv[];
{
    Graph *graphPtr = axisPtr->graphPtr; 
    Tk_ConfigSpec *configSpecs;
    int flags;

    flags = ((GRAPH_TYPE_BIT << graphPtr->defElemType) | TK_CONFIG_ARGV_ONLY);
    configSpecs = axisConfigSpecs[axisPtr->type];
    if (argc == 3) {
	return (Tk_ConfigureInfo(graphPtr->interp, graphPtr->tkwin,
		configSpecs, (char *)axisPtr, (char *)NULL, flags));
    } else if (argc == 4) {
	return (Tk_ConfigureInfo(graphPtr->interp, graphPtr->tkwin,
		configSpecs, (char *)axisPtr, argv[3], flags));
    }
    return (ConfigureAxis(axisPtr, argc - 3, argv + 3, flags));
}

/*
 *--------------------------------------------------------------
 *
 * LimitsOper --
 *
 *	This procedure returns a string representing the axis limits
 *	of the graph.  The format of the string is { xmin ymin xmax ymax}.
 *
 * Results:
 *	Always returns TCL_OK.  The interp->result field is
 *	a list of the graph axis limits.
 *
 *--------------------------------------------------------------
 */
/*ARGSUSED*/
static int
LimitsOper(axisPtr, argc, argv)
    Axis *axisPtr;
    int argc;			/* not used */
    char **argv;		/* not used */

{
    Graph *graphPtr = axisPtr->graphPtr; 
    double min, max;

    if (graphPtr->flags & RESET_AXES) {
	Blt_ComputeAxes(graphPtr);
    }
    min = axisPtr->min, max = axisPtr->max;
    if (axisPtr->logScale) {
	min = BLT_EXP10(min), max = BLT_EXP10(max);
    }
    Tcl_ResetResult(graphPtr->interp);
    Blt_AppendDouble(graphPtr->interp, min);
    Blt_AppendDouble(graphPtr->interp, max);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InvTransformOper --
 *
 *	Maps the given window coordinate into an axis-value.
 *
 * Results:
 *	Returns a standard Tcl result.  interp->result contains
 *	the axis value. If an error occurred, TCL_ERROR is returned
 *	and interp->result will contain an error message.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InvTransformOper(axisPtr, argc, argv)
    Axis *axisPtr;
    int argc;			/* not used */
    char **argv;
{
    Graph *graphPtr = axisPtr->graphPtr; 
    int coord;			/* Integer window coordinate*/
    double value;

    if (graphPtr->flags & RESET_AXES) {
	Blt_ComputeAxes(graphPtr);
    }
    if (Tcl_GetInt(graphPtr->interp, argv[3], &coord) != TCL_OK) {
	return TCL_ERROR;
    }
    value = InvTransform((GraphAxis *)axisPtr, (double)coord);
    Tcl_ResetResult(graphPtr->interp);
    Blt_AppendDouble(graphPtr->interp, value);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TransformOper --
 *
 *	Maps the given axis-value to a window coordinate.
 *
 * Results:
 *	Returns a standard Tcl result.  interp->result contains
 *	the window coordinate. If an error occurred, TCL_ERROR
 *	is returned and interp->result will contain an error
 *	message.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TransformOper(axisPtr, argc, argv)
    Axis *axisPtr;		/* Axis */
    int argc;			/* not used */
    char **argv;
{
    Graph *graphPtr = axisPtr->graphPtr; 
    double value;
    int coord;
    char tmpString[10];

    if (graphPtr->flags & RESET_AXES) {
	Blt_ComputeAxes(graphPtr);
    }
    if (Blt_GetCoordinate(graphPtr->interp, argv[3], &value) != TCL_OK) {
	return TCL_ERROR;
    }
    coord = (int)Transform((GraphAxis *)axisPtr, value);
    sprintf(tmpString, "%d", coord);
    Tcl_SetResult(graphPtr->interp, tmpString, TCL_VOLATILE);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_CreateAxis --
 *
 *	Create and initialize a structure containing information to
 * 	display a graph axis.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * ----------------------------------------------------------------------
 */
int
Blt_CreateAxis(graphPtr, type, flags)
    Graph *graphPtr;
    AxisType type;
    int flags;			/* Configuration flags */
{
    Axis *axisPtr;
    AxisSite site;

    axisPtr = (Axis *)ckcalloc(1, sizeof(Axis));
    if (axisPtr == NULL) {
	Panic("can't allocate axis structure");
    }
    site = (AxisSite)type;	/* For now, there's a 1-1 correspondence
				 * between axis types and sites */
    axisPtr->type = type;
    axisPtr->site = site;
    axisPtr->graphPtr = graphPtr; /* Needed for Tcl_PrintDouble */
    axisPtr->showTicks = TRUE;
    axisPtr->majorTicks.genTicks = TRUE;
    axisPtr->minorTicks.genTicks = TRUE;
    axisPtr->majorTicks.valueArr = axisPtr->majorTicks.defSpace;
    axisPtr->minorTicks.valueArr = axisPtr->minorTicks.defSpace;
    axisPtr->minorTicks.reqNumTicks = 2;
    axisPtr->destroyProc = (AxisDestroyProc*)DestroyAxis;
    axisPtr->drawProc = (AxisDrawProc*)DrawAxis;
    axisPtr->coordsProc = (AxisCoordsProc*)ComputeAxisCoordinates;
    axisPtr->printProc = (AxisPrintProc*)PrintAxis;
    axisPtr->justify = TK_JUSTIFY_CENTER;
    /*
     * The actual axis min and max can't be the same, so initializing
     * the previous limits to zero is OK because it will always cause
     * the axis to be recalculated.  
     */
    axisPtr->prevMin = axisPtr->prevMax = 0.0;
    axisPtr->mapped = ((site == AXIS_SITE_BOTTOM) || (site == AXIS_SITE_LEFT));

    graphPtr->axisArr[type] = (GraphAxis *)axisPtr;
    if (ConfigureAxis(axisPtr, 0, (char **)NULL, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

static Blt_OperSpec operSpecs[] =
{
    {"cget", 2, (Blt_OperProc) CgetOper, 4, 4, "option",},
    {"configure", 2, (Blt_OperProc) ConfigureOper, 3, 0, "?args?",},
    {"invtransform", 1, (Blt_OperProc) InvTransformOper, 4, 4, "screenCoord",},
    {"limits", 1, (Blt_OperProc) LimitsOper, 3, 3, "",},
    {"transform", 1, (Blt_OperProc) TransformOper, 4, 4, "graphCoord",},
};
static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

int
Blt_AxisOper(axis, argc, argv)
    GraphAxis *axis;
    int argc;
    char **argv;
{
    Axis *axisPtr = (Axis *)axis;
    Blt_OperProc proc;
    int result;

    proc = Blt_LookupOperation(axisPtr->graphPtr->interp, numSpecs, operSpecs,
	BLT_OPER_ARG2, argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (axisPtr, argc, argv);
    return (result);
}
#endif /* !NO_GRAPH */
