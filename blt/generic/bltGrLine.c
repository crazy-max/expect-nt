/*
 * bltGrLine.c --
 *
 *	This module implements line elements in the graph widget
 *	for the Tk toolkit.
 *
 * Copyright 1991-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef	NO_GRAPH
#include "bltGraph.h"
#include "bltGrElem.h"
#include <ctype.h>

static int SymbolParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *SymbolPrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));

static int TraceParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *TracePrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));

static int ColorParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *ColorPrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));

extern Tk_CustomOption bltXDataOption;
extern Tk_CustomOption bltYDataOption;
extern Tk_CustomOption bltDataPairsOption;
extern Tk_CustomOption bltMapXAxisOption;
extern Tk_CustomOption bltMapYAxisOption;
extern Tk_CustomOption bltLengthOption;
extern Tk_CustomOption bltDashesOption;

static Tk_CustomOption symbolOption =
{
    SymbolParseProc, SymbolPrintProc, (ClientData)0
};

static Tk_CustomOption traceOption =
{
    TraceParseProc, TracePrintProc, (ClientData)0
};

static Tk_CustomOption colorOption =
{
    ColorParseProc, ColorPrintProc, (ClientData)0
};

typedef struct {
    int numPoints;		/* Number of points in segment */
    XPoint pointArr[1];		/* Array of points forming the trace. The
				 * array is allocated */
} Blt_Trace;

#define BROKEN_TRACE(traceDir,last,next) \
    (((((traceDir) & TRACE_DECREASING) == 0) && ((next) < (last))) || \
     ((((traceDir) & TRACE_INCREASING) == 0) && ((next) > (last))))


#define TRACE_INCREASING 1	/* Draw line segments for only those
				 * data points whose abscissas are
				 * monotonically increasing in order */
#define TRACE_DECREASING 2	/* Lines will be drawn between only
				 * those points whose abscissas are
				 * decreasing in order */
#define TRACE_BOTH	(TRACE_INCREASING | TRACE_DECREASING)
				/* Lines will be drawn between points
				 * regardless of the ordering of the
				 * abscissas */

#define COLOR_DEFAULT	(XColor *)1
#define COLOR_NONE	(XColor *)NULL

static char *symbolMacros[] =
{
    "Li", "Sq", "Ci", "Di", "Pl", "Cr", "Sp", "Sc", "Tr", (char *)NULL,
};


typedef struct {
    Graph *graphPtr;		/* Graph widget of element*/
    ElementType type;		/* Type of element is LINE_ELEMENT */
    unsigned int flags;		/* Indicates if the entire bar is active,
				 * or if coordinates need to be calculated */
    Tk_Uid nameId;		/* Identifier referring to the element.
				 * Used in the "insert", "delete", or
				 * "show", command options. */
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
				 * case: if numActivePoints==-1, then all data
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
     * Line specific configurable attributes
     */
    int symbolPixels;		/* Requested size of symbol in pixels. */
    int scaleSymbols;
    int scaleFlag;

    double xRange, yRange;	/* Requested size of symbol in graph
				 * x-coordinates. */
    int symbolSize;		/* Computed size of symbol in pixels. */
    SymbolType symbol;		/* Element symbol type */

    XColor *outlineColor;	/* Normal symbol outline color */
    XColor *fillColor;		/* Normal symbol fill color */
    XColor *traceColor;		/* Normal line color */
    XColor *gapColor;		/* Normal line gap color */
    int traceWidth;		/* Width of the line connecting the
				 * symbols together. If lineWidth >= 0,
				 * no line will be drawn, only symbols. */
    int outlineWidth;		/* Width of the outline around the symbol */

    XColor *activeOutlineColor;	/* Normal color of symbol outlines */
    XColor *activeFillColor;	/* Normal color of symbol fills */
    XColor *activeTraceColor;	/* Normal color or lines */
    int activeTraceWidth;	/* Width of the active line */

    Dashes dashes;		/* Dash on-off list value */
    int traceDir;		/* Indicates if to draw an element as
				 * separate line segments where x-coordinate
				 * values are monotonically increasing or
				 * decreasing. */
    GC normalOutlineGC;		/* Symbol outline graphics context */
    GC normalFillGC;		/* Symbol fill graphics context */
    GC normalTraceGC;		/* Line graphics context */

    GC activeOutlineGC;
    GC activeFillGC;
    GC activeTraceGC;

    /*
     * Layout and drawing related structures.
     */
    Blt_List traces;		/* List of traces formed by the data points.
				 * A trace can be formed by a line segment
				 * that either 1) runs in the wrong x-direction
				 * or 2) its last end point is clipped. */

} Line;

#define DEF_LINE_ACTIVE_FILL_COLOR 	"defcolor"
#define DEF_LINE_ACTIVE_FILL_MONO	"defcolor"
#define DEF_LINE_ACTIVE_OUTLINE_COLOR 	"defcolor"
#define DEF_LINE_ACTIVE_OUTLINE_MONO 	"defcolor"
#define DEF_LINE_ACTIVE_TRACE_COLOR	"blue"
#define DEF_LINE_ACTIVE_TRACE_MONO	BLACK
#define DEF_LINE_ACTIVE_TRACE_WIDTH 	"1"
#define DEF_LINE_AXIS_X			"x"
#define DEF_LINE_AXIS_Y			"y"
#define DEF_LINE_DASHES			(char *)NULL
#define DEF_LINE_DATA			(char *)NULL
#define DEF_LINE_FILL_COLOR    		"defcolor"
#define DEF_LINE_FILL_MONO		"defcolor"
#define DEF_LINE_LABEL			(char *)NULL
#define DEF_LINE_MAPPED			"1"
#define DEF_LINE_OFFDASH_COLOR    	(char *)NULL
#define DEF_LINE_OFFDASH_MONO		(char *)NULL
#define DEF_LINE_OUTLINE_COLOR		"defcolor"
#define DEF_LINE_OUTLINE_MONO		"defcolor"
#define DEF_LINE_OUTLINE_WIDTH 		"1"
#define DEF_LINE_PIXELS			"0.125i"
#define DEF_LINE_SCALE_SYMBOLS		"0"
#define DEF_LINE_SYMBOL			"none"
#define DEF_LINE_TRACE			"both"
#define DEF_LINE_TRACE_COLOR		"navyblue"
#define DEF_LINE_TRACE_MONO		BLACK
#define DEF_LINE_TRACE_WIDTH		"1"
#define DEF_LINE_X_DATA			(char *)NULL
#define DEF_LINE_Y_DATA			(char *)NULL

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_COLOR, "-activecolor", "elemActiveColor", "ActiveColor",
	DEF_LINE_ACTIVE_TRACE_COLOR, Tk_Offset(Line, activeTraceColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-activecolor", "elemActiveColor", "ActiveColor",
	DEF_LINE_ACTIVE_TRACE_MONO, Tk_Offset(Line, activeTraceColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-activefill", "elemActiveFill", "ActiveFill",
	DEF_LINE_ACTIVE_FILL_COLOR, Tk_Offset(Line, activeFillColor),
	TK_CONFIG_NULL_OK | TK_CONFIG_COLOR_ONLY, &colorOption},
    {TK_CONFIG_CUSTOM, "-activefill", "elemActiveFill", "ActiveFill",
	DEF_LINE_ACTIVE_FILL_MONO, Tk_Offset(Line, activeFillColor),
	TK_CONFIG_NULL_OK | TK_CONFIG_MONO_ONLY, &colorOption},
    {TK_CONFIG_CUSTOM, "-activelinewidth", "elemActiveLineWidth",
	"ActiveLineWidth",
	DEF_LINE_ACTIVE_TRACE_WIDTH, Tk_Offset(Line, activeTraceWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-activeoutline", "elemActiveOutline", "ActiveOutline",
	DEF_LINE_ACTIVE_OUTLINE_COLOR, Tk_Offset(Line, activeOutlineColor),
	TK_CONFIG_COLOR_ONLY, &colorOption},
    {TK_CONFIG_CUSTOM, "-activeoutline", "elemActiveOutline", "ActiveOutline",
	DEF_LINE_ACTIVE_OUTLINE_MONO, Tk_Offset(Line, activeOutlineColor),
	TK_CONFIG_MONO_ONLY, &colorOption},
    {TK_CONFIG_COLOR, "-color", "elemColor", "Color",
	DEF_LINE_TRACE_COLOR, Tk_Offset(Line, traceColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-color", "elemColor", "Color",
	DEF_LINE_TRACE_MONO, Tk_Offset(Line, traceColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-dashes", "elemDashes", "Dashes",
	DEF_LINE_DASHES, Tk_Offset(Line, dashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_CUSTOM, "-data", "elemData", "Data",
	DEF_LINE_DATA, 0, 0, &bltDataPairsOption},
    {TK_CONFIG_CUSTOM, "-fill", "elemFill", "Fill",
	DEF_LINE_FILL_COLOR, Tk_Offset(Line, fillColor),
	TK_CONFIG_NULL_OK | TK_CONFIG_COLOR_ONLY, &colorOption},
    {TK_CONFIG_CUSTOM, "-fill", "elemFill", "Fill",
	DEF_LINE_FILL_MONO, Tk_Offset(Line, fillColor),
	TK_CONFIG_NULL_OK | TK_CONFIG_MONO_ONLY, &colorOption},
    {TK_CONFIG_STRING, "-label", "elemLabel", "Label",
	DEF_LINE_LABEL, Tk_Offset(Line, label), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-linewidth", "elemLineWidth", "LineWidth",
	DEF_LINE_TRACE_WIDTH, Tk_Offset(Line, traceWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-mapped", "elemMapped", "Mapped",
	DEF_LINE_MAPPED, Tk_Offset(Line, mapped), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "elemMapX", "MapX",
	DEF_LINE_AXIS_X, Tk_Offset(Line, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "elemMapY", "MapY",
	DEF_LINE_AXIS_Y, Tk_Offset(Line, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapYAxisOption},
    {TK_CONFIG_CUSTOM, "-offdash", "elemOffDash", "OffDash",
	DEF_LINE_OFFDASH_COLOR, Tk_Offset(Line, gapColor),
	TK_CONFIG_NULL_OK | TK_CONFIG_COLOR_ONLY, &colorOption},
    {TK_CONFIG_CUSTOM, "-offdash", "elemOffDash", "OffDash",
	DEF_LINE_OFFDASH_MONO, Tk_Offset(Line, gapColor),
	TK_CONFIG_NULL_OK | TK_CONFIG_MONO_ONLY, &colorOption},
    {TK_CONFIG_CUSTOM, "-outline", "elemOutline", "Outline",
	DEF_LINE_OUTLINE_COLOR, Tk_Offset(Line, outlineColor),
	TK_CONFIG_COLOR_ONLY, &colorOption},
    {TK_CONFIG_CUSTOM, "-outline", "elemOutline", "Outline",
	DEF_LINE_OUTLINE_MONO, Tk_Offset(Line, outlineColor),
	TK_CONFIG_MONO_ONLY, &colorOption},
    {TK_CONFIG_CUSTOM, "-outlinewidth", "elemOutlineWidth", "OutlineWidth",
	DEF_LINE_OUTLINE_WIDTH, Tk_Offset(Line, outlineWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-pixels", "elemPixels", "Pixels",
	DEF_LINE_PIXELS, Tk_Offset(Line, symbolPixels),
	0, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-scalesymbols", "elemScaleSymbols", "ScaleSymbols",
	DEF_LINE_SCALE_SYMBOLS, Tk_Offset(Line, scaleSymbols),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-symbol", "elemSymbol", "Symbol",
	DEF_LINE_SYMBOL, Tk_Offset(Line, symbol),
	TK_CONFIG_DONT_SET_DEFAULT, &symbolOption},
    {TK_CONFIG_CUSTOM, "-trace", "elemTrace", "Trace",
	DEF_LINE_TRACE, Tk_Offset(Line, traceDir),
	TK_CONFIG_DONT_SET_DEFAULT, &traceOption},
    {TK_CONFIG_CUSTOM, "-xdata", "elemXdata", "Xdata",
	DEF_LINE_X_DATA, 0, 0, &bltXDataOption},
    {TK_CONFIG_CUSTOM, "-ydata", "elemYdata", "Ydata",
	DEF_LINE_Y_DATA, 0, 0, &bltYDataOption},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

#define LINE_INIT_SYMBOL_SCALE	(1<<10)


INLINE static int
Round(x)
    register double x;
{
    return (int) (x + ((x < 0.0) ? -0.5 : 0.5));
}

/*
 * ----------------------------------------------------------------------
 * 	Custom configuration option (parse and print) routines
 * ----------------------------------------------------------------------
 */

/*
 *----------------------------------------------------------------------
 *
 * NameOfSymbol --
 *
 *	Converts the symbol value into its string representation.
 *
 * Results:
 *	The static string representing the symbol type is returned.
 *
 *----------------------------------------------------------------------
 */
static char *
NameOfSymbol(symbol)
    SymbolType symbol;
{
    switch (symbol) {
    case SYMBOL_NONE:
	return "none";
    case SYMBOL_SQUARE:
	return "square";
    case SYMBOL_CIRCLE:
	return "circle";
    case SYMBOL_DIAMOND:
	return "diamond";
    case SYMBOL_PLUS:
	return "plus";
    case SYMBOL_CROSS:
	return "cross";
    case SYMBOL_SPLUS:
	return "splus";
    case SYMBOL_SCROSS:
	return "scross";
    case SYMBOL_TRIANGLE:
	return "triangle";
    default:
	return "unknown symbol type";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SymbolParseProc --
 *
 *	Convert the string representation of a line style or symbol name
 *	into its numeric form.
 *
 * Results:
 *	The return value is a standard Tcl result.  The symbol type is
 *	written into the widget record.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SymbolParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* String representing symbol type */
    char *widgRec;		/* Element information record */
    int offset;			/* Offset of symbol type field in record */
{
    SymbolType *symbolPtr = (SymbolType *)(widgRec + offset);
    unsigned int length;
    char c;

    c = value[0];
    length = strlen(value);
    if ((c == 'n') && (strncmp(value, "none", length) == 0)) {
	*symbolPtr = SYMBOL_NONE;
    } else if ((c == 'c') && (length > 1) &&
	(strncmp(value, "circle", length) == 0)) {
	*symbolPtr = SYMBOL_CIRCLE;
    } else if ((c == 's') && (length > 1) &&
	(strncmp(value, "square", length) == 0)) {
	*symbolPtr = SYMBOL_SQUARE;
    } else if ((c == 'd') && (strncmp(value, "diamond", length) == 0)) {
	*symbolPtr = SYMBOL_DIAMOND;
    } else if ((c == 'p') && (strncmp(value, "plus", length) == 0)) {
	*symbolPtr = SYMBOL_PLUS;
    } else if ((c == 'c') && (length > 1) &&
	(strncmp(value, "cross", length) == 0)) {
	*symbolPtr = SYMBOL_CROSS;
    } else if ((c == 's') && (length > 1) &&
	(strncmp(value, "splus", length) == 0)) {
	*symbolPtr = SYMBOL_SPLUS;
    } else if ((c == 's') && (length > 1) &&
	(strncmp(value, "scross", length) == 0)) {
	*symbolPtr = SYMBOL_SCROSS;
    } else if ((c == 't') && (strncmp(value, "triangle", length) == 0)) {
	*symbolPtr = SYMBOL_TRIANGLE;
    } else {
	Tcl_AppendResult(interp, "bad symbol name \"", value, "\": should be ",
	    "none, circle, square, diamond, plus, cross, splus, scross, or ",
	    "triangle", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SymbolPrintProc --
 *
 *	Convert the symbol value into a string.
 *
 * Results:
 *	The string representing the symbol type or line style is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
SymbolPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Element information record */
    int offset;			/* Offset of symbol type field in record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    SymbolType symbol = *(SymbolType *)(widgRec + offset);

    return (NameOfSymbol(symbol));
}

/*
 *----------------------------------------------------------------------
 *
 * TraceParseProc --
 *
 *	Convert the string representation of a line style or symbol name
 *	into its numeric form.
 *
 * Results:
 *	The return value is a standard Tcl result.  The symbol type is
 *	written into the widget record.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TraceParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* String representing symbol type */
    char *widgRec;		/* Element information record */
    int offset;			/* Offset of symbol type field in record */
{
    int *traceDirPtr = (int *)(widgRec + offset);
    unsigned int length;
    char c;

    c = value[0];
    length = strlen(value);
    if ((c == 'i') && (strncmp(value, "increasing", length) == 0)) {
	*traceDirPtr = TRACE_INCREASING;
    } else if ((c == 'd') && (strncmp(value, "decreasing", length) == 0)) {
	*traceDirPtr = TRACE_DECREASING;
    } else if ((c == 'b') && (strncmp(value, "both", length) == 0)) {
	*traceDirPtr = TRACE_BOTH;
    } else {
	Tcl_AppendResult(interp, "bad trace value \"", value,
	    "\" : should be \"increasing\", \"decreasing\", or \"both\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NameOfTrace --
 *
 *	Convert the trace option value into a string.
 *
 * Results:
 *	The static string representing the trace option is returned.
 *
 *----------------------------------------------------------------------
 */
static char *
NameOfTrace(traceDir)
    int traceDir;		/* Direction for line traces between points */
{
    switch (traceDir) {
    case TRACE_INCREASING:
	return "increasing";
    case TRACE_DECREASING:
	return "decreasing";
    case TRACE_BOTH:
	return "both";
    default:
	return "unknown trace direction";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TracePrintProc --
 *
 *	Convert the symbol value into a string.
 *
 * Results:
 *	The string representing the symbol type or line style is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
TracePrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Element information record */
    int offset;			/* Offset of symbol type field in record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    int traceDir = *(int *)(widgRec + offset);

    return (NameOfTrace(traceDir));
}

/*
 *----------------------------------------------------------------------
 *
 * ColorParseProc --
 *
 *	Convert the string representation of a color into a XColor pointer.
 *
 * Results:
 *	The return value is a standard Tcl result.  The color pointer is
 *	written into the widget record.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColorParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* String representing color */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of color field in record */
{
    XColor **colorPtrPtr = (XColor **)(widgRec + offset);
    unsigned int length;
    char c;

    if ((value == NULL) || (*value == '\0')) {
	*colorPtrPtr = NULL;
	return TCL_OK;
    }
    c = value[0];
    length = strlen(value);

    if ((c == 'd') && (strncmp(value, "defcolor", length) == 0)) {
	*colorPtrPtr = COLOR_DEFAULT;
    } else {
	*colorPtrPtr = Tk_GetColor(interp, tkwin, Tk_GetUid(value));
	if (*colorPtrPtr == NULL) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NameOfColor --
 *
 *	Convert the color option value into a string.
 *
 * Results:
 *	The static string representing the color option is returned.
 *
 *----------------------------------------------------------------------
 */
static char *
NameOfColor(colorPtr)
    XColor *colorPtr;
{
    if (colorPtr == COLOR_NONE) {
	return "";
    } else if (colorPtr == COLOR_DEFAULT) {
	return "defcolor";
    } else {
	return Tk_NameOfColor(colorPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ColorPrintProc --
 *
 *	Convert the color value into a string.
 *
 * Results:
 *	The string representing the symbol color is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
ColorPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Element information record */
    int offset;			/* Offset of symbol type field in record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    XColor *colorPtr = *(XColor **)(widgRec + offset);

    return (NameOfColor(colorPtr));
}

/*
 * ----------------------------------------------------------------------
 * 	Line element routines
 * ----------------------------------------------------------------------
 */

/*
 *----------------------------------------------------------------------
 *
 * ConfigureLine --
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
 *	Configuration information such as line width, line style, color
 *	etc. get set in a new GC.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigureLine(graphPtr, elemPtr)
    Graph *graphPtr;
    Element *elemPtr;
{
    Line *linePtr = (Line *)elemPtr;
    unsigned long gcMask;
    GC newGC;
    XGCValues gcValues;
    XColor *colorPtr;

    /* Outlines of symbols: GCForeground is outline color */

    colorPtr = linePtr->outlineColor;
    if (colorPtr == COLOR_DEFAULT) {
	colorPtr = linePtr->traceColor;
    }
    gcMask = (GCLineWidth | GCForeground);
    gcValues.foreground = colorPtr->pixel;
    gcValues.line_width = linePtr->outlineWidth;
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (linePtr->normalOutlineGC != NULL) {
	Tk_FreeGC(graphPtr->display, linePtr->normalOutlineGC);
    }
    linePtr->normalOutlineGC = newGC;

    colorPtr = linePtr->activeOutlineColor;
    if (colorPtr == COLOR_DEFAULT) {
	colorPtr = linePtr->activeTraceColor;
    }
    gcValues.foreground = colorPtr->pixel;
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (linePtr->activeOutlineGC != NULL) {
	Tk_FreeGC(graphPtr->display, linePtr->activeOutlineGC);
    }
    linePtr->activeOutlineGC = newGC;

    /* Fills for symbols: GCForeground is fill color */

    colorPtr = linePtr->fillColor;
    if (colorPtr == COLOR_DEFAULT) {
	colorPtr = linePtr->traceColor;
    }
    newGC = NULL;
    if (colorPtr != NULL) {
	gcValues.foreground = colorPtr->pixel;
	newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    }
    if (linePtr->normalFillGC != NULL) {
	Tk_FreeGC(graphPtr->display, linePtr->normalFillGC);
    }
    linePtr->normalFillGC = newGC;

    colorPtr = linePtr->activeFillColor;
    if (colorPtr == COLOR_DEFAULT) {
	colorPtr = linePtr->activeTraceColor;
    }
    newGC = NULL;
    if (colorPtr != NULL) {
	gcValues.foreground = colorPtr->pixel;
	newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    }
    if (linePtr->activeFillGC != NULL) {
	Tk_FreeGC(graphPtr->display, linePtr->activeFillGC);
    }
    linePtr->activeFillGC = newGC;

    /* Traces */

    gcMask = (GCLineWidth | GCForeground | GCLineStyle | GCCapStyle |
	GCJoinStyle);
    gcValues.cap_style = CapButt;
    gcValues.join_style = JoinRound;
    gcValues.line_style = LineSolid;
    gcValues.line_width = linePtr->traceWidth;

    colorPtr = linePtr->gapColor;
    if (colorPtr == COLOR_DEFAULT) {
	colorPtr = linePtr->traceColor;
    }
    if (colorPtr != NULL) {
	gcMask |= GCBackground;
	gcValues.background = colorPtr->pixel;
    }
    gcValues.foreground = linePtr->traceColor->pixel;
    if (linePtr->dashes.numValues > 0) {
	gcValues.line_style =
	    (colorPtr == NULL) ? LineOnOffDash : LineDoubleDash;
    }
    newGC = Blt_GetUnsharedGC(graphPtr->tkwin, gcMask, &gcValues);
    if (linePtr->normalTraceGC != NULL) {
	XFreeGC(graphPtr->display, linePtr->normalTraceGC);
    }
    if (linePtr->dashes.numValues > 0) {
	XSetDashes(graphPtr->display, newGC, 0, linePtr->dashes.valueList,
	    linePtr->dashes.numValues);
    }
    linePtr->normalTraceGC = newGC;

    gcValues.foreground = linePtr->activeTraceColor->pixel;
    gcValues.line_width = linePtr->activeTraceWidth;
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (linePtr->activeTraceGC != NULL) {
	Tk_FreeGC(graphPtr->display, linePtr->activeTraceGC);
    }
    linePtr->activeTraceGC = newGC;

    if (Blt_ConfigModified(linePtr->configSpecs, "-scalesymbols",
	    (char *)NULL)) {
	linePtr->flags |= (COORDS_NEEDED | LINE_INIT_SYMBOL_SCALE);
    }
    if (Blt_ConfigModified(linePtr->configSpecs, "-pixels", "-trace",
	    (char *)NULL)) {
	linePtr->flags |= COORDS_NEEDED;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetLineLimits --
 *
 *	Retrieves the limits of the line element
 *
 * Results:
 *	Returns the number of data points in the element.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
GetLineLimits(graphPtr, elemPtr, axisPtr, minPtr, maxPtr)
    Graph *graphPtr;		/* Graph widget record */
    Element *elemPtr;
    GraphAxis *axisPtr;		/* Axis information */
    double *minPtr, *maxPtr;
{
    Line *linePtr = (Line *)elemPtr;
    double min;
    int numPoints;

    /* Verify the element uses this axis */
    if (!(elemPtr->axisMask & AXIS_MASK(axisPtr))) {
	return 0;		/* Line is not mapped to this axis */
    }
    *minPtr = bltPosInfinity, *maxPtr = bltNegInfinity;

    numPoints = ELEMENT_LENGTH(linePtr);
    if (numPoints > 0) {
	if (AXIS_MASK(axisPtr) & AXIS_MASK_X) {
	    min = linePtr->x.vector.min;
	    if ((min <= 0.0) && (axisPtr->logScale)) {
		min = Blt_FindVectorMinimum(&(linePtr->x), DBL_MIN);
	    }
	    *minPtr = min;
	    *maxPtr = linePtr->x.vector.max;
	} else {
	    min = linePtr->y.vector.min;
	    if ((min <= 0.0) && (axisPtr->logScale)) {
		min = Blt_FindVectorMinimum(&(linePtr->y), DBL_MIN);
	    }
	    *minPtr = min;
	    *maxPtr = linePtr->y.vector.max;
	}
    }
    return (numPoints);
}

/*
 *----------------------------------------------------------------------
 *
 * ClosestEndPoint --
 *
 *	Find the data point closest to the given window x-y coordinate
 *	in the element.
 *
 * Results:
 *	If a new minimum distance is found, the information regarding
 *	it is returned via infoPtr.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
ClosestEndPoint(graphPtr, linePtr, infoPtr)
    Graph *graphPtr;		/* not used */
    Line *linePtr;		/* Line element record */
    ClosestInfo *infoPtr;	/* Info about closest point in element */
{
    double dist, prevMin;
    Blt_Trace *tracePtr;
    XPoint *pointPtr;
    int dataIndex;
    register Blt_ListItem *iPtr;
    register int i;

    prevMin = infoPtr->dist;

    for (iPtr = Blt_FirstListItem(&(linePtr->traces)); iPtr != NULL;
	    iPtr = Blt_NextItem(iPtr)) {
	tracePtr = (Blt_Trace *)Blt_GetItemValue(iPtr);
	pointPtr = tracePtr->pointArr;
	dataIndex = (int)Blt_GetItemKey(iPtr);
	for (i = 0; i < tracePtr->numPoints; i++) {
	    dist = bltHypot((double)(infoPtr->x - pointPtr->x),
		(double)(infoPtr->y - pointPtr->y));
	    if (dist < infoPtr->dist) {
		infoPtr->index = dataIndex + i;
		infoPtr->dist = dist;
	    }
	    pointPtr++;
	}
    }
    if (infoPtr->dist < prevMin) {
	infoPtr->elemId = linePtr->nameId;
	infoPtr->closest.x = linePtr->x.vector.valueArr[infoPtr->index];
	infoPtr->closest.y = linePtr->y.vector.valueArr[infoPtr->index];
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ClosestLineSegment --
 *
 *	Find the line segment closest to the given window coordinate
 *	in the element.
 *
 * Results:
 *	If a new minimum distance is found, the information regarding
 *	it is returned via infoPtr.
 *
 *----------------------------------------------------------------------
 */
static void
ClosestLineSegment(graphPtr, linePtr, infoPtr)
    Graph *graphPtr;		/* Graph widget record */
    Line *linePtr;		/* Line element record */
    ClosestInfo *infoPtr;	/* Info about closest point in element */
{
    int i;
    double dist;
    double prevMin;
    double closestX, closestY;
    double x, y;
    register Blt_ListItem *iPtr;
    Blt_Trace *tracePtr;
    XPoint *pointPtr;
    int dataIndex;

    closestX = closestY = 0;
    prevMin = infoPtr->dist;
    for (iPtr = Blt_FirstListItem(&(linePtr->traces)); iPtr != NULL;
	iPtr = Blt_NextItem(iPtr)) {
	tracePtr = (Blt_Trace *)Blt_GetItemValue(iPtr);
	pointPtr = tracePtr->pointArr;
	dataIndex = (int)Blt_GetItemKey(iPtr);
	for (i = 0; i < (tracePtr->numPoints - 1); i++) {
	    dist = Blt_GetDistanceToSegment(pointPtr, infoPtr->x, infoPtr->y,
		&x, &y);
	    if (dist < infoPtr->dist) {
		closestX = x, closestY = y;
		infoPtr->index = dataIndex + i;
		infoPtr->dist = dist;
	    }
	    pointPtr++;
	}
    }
    if (infoPtr->dist < prevMin) {
	AxisPair axisPair;
	double d1, d2;
	Coordinate p1, p2;

	/*
	 * Determine which end point on the line segment is closest.
	 */

	Blt_GetAxisMapping(graphPtr, linePtr->axisMask, &axisPair);

	p1 = Blt_TransformPt(graphPtr, 
		linePtr->x.vector.valueArr[infoPtr->index],
		linePtr->y.vector.valueArr[infoPtr->index], &axisPair);
	p2 = Blt_TransformPt(graphPtr, 
	    linePtr->x.vector.valueArr[infoPtr->index + 1],
	    linePtr->y.vector.valueArr[infoPtr->index + 1], &axisPair);

	d1 = bltHypot((double)(infoPtr->x - p1.x), (double)(infoPtr->y - p1.y));
	d2 = bltHypot((double)(infoPtr->x - p2.x), (double)(infoPtr->y - p2.y));

	if (BLT_FABS(d2) < BLT_FABS(d1)) {
	    (infoPtr->index)++;
	}
	infoPtr->elemId = linePtr->nameId;
	infoPtr->closest = Blt_InvTransformPt(graphPtr, closestX, closestY,
	    &axisPair);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ClosestLine --
 *
 *	Find the closest point or line segment (if interpolated) to
 *	the given window coordinate in the line element.
 *
 * Results:
 *	Returns the distance of the closest point among other
 *	information.
 *
 *----------------------------------------------------------------------
 */
static void
ClosestLine(graphPtr, elemPtr, infoPtr)
    Graph *graphPtr;		/* Graph widget record */
    Element *elemPtr;		/* Element to examine */
    ClosestInfo *infoPtr;	/* Info about closest point in element */
{
    Line *linePtr = (Line *)elemPtr;

    if (infoPtr->interpolate) {
	ClosestLineSegment(graphPtr, linePtr, infoPtr);
    } else {
	ClosestEndPoint(graphPtr, linePtr, infoPtr);
    }
}



static void
ComputeSymbolSize(graphPtr, linePtr, pairPtr)
    Graph *graphPtr;
    Line *linePtr;
    AxisPair *pairPtr;
{
    int symbolSize;
    int maxSize;

    symbolSize = linePtr->symbolPixels;
    if (linePtr->scaleSymbols) {
	double rangeX, rangeY;

	rangeX = (pairPtr->x->max - pairPtr->x->min);
	rangeY = (pairPtr->y->max - pairPtr->y->min);
	if ((linePtr->flags & LINE_INIT_SYMBOL_SCALE) ||
	    (linePtr->xRange < DBL_MIN) || (linePtr->yRange < DBL_MIN)) {
	    linePtr->xRange = rangeX, linePtr->yRange = rangeY;
	    linePtr->flags &= ~LINE_INIT_SYMBOL_SCALE;
	} else {
	    double scaleX, scaleY;

	    scaleX = linePtr->xRange / rangeX;
	    scaleY = linePtr->yRange / rangeY;
	    symbolSize = (int)(symbolSize*BLT_MIN(scaleX, scaleY));
	}
    }
    /*
     * Don't let the size of symbols go unbounded because the X drawing
     * routines assume coordinates to be a signed short int.
     */
    maxSize = BLT_MIN(graphPtr->plotArea.width, graphPtr->plotArea.height);
    if (linePtr->symbolSize > maxSize) {
	symbolSize = maxSize;
    }
    /* Make the symbol size odd so that its center is a single pixel. */
    symbolSize |= 0x01;
    linePtr->symbolSize = symbolSize;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateTrace --
 *
 *	Creates a new trace and inserts it into the line's
 *	list of traces.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
CreateTrace(linePtr, dataIndex, coordArr, length)
    Line *linePtr;
    int dataIndex;		/* Starting index of the trace in data point
				 * array.  Used to figure out closest point */
    Coordinate coordArr[];	/* Array of points forming the trace.  The
				 * end points of the trace may be clipped. */
    int length;			/* Number of points forming the trace */
{
    Blt_ListItem *iPtr;
    Blt_Trace *tracePtr;
    register XPoint *pointPtr;
    register int i;
    unsigned int size;

    size = sizeof(Blt_Trace) + (sizeof(XPoint) * (length - 1));
    tracePtr = (Blt_Trace *)ckcalloc(1, size);
    if (tracePtr == NULL) {
	Panic("can't allocate new trace");
    }
    /* Copy the screen coordinates of the trace into the point array */
    pointPtr = tracePtr->pointArr;
    for (i = 0; i < length; i++) {
	pointPtr->x = Round(coordArr[i].x);
	pointPtr->y = Round(coordArr[i].y);
	pointPtr++;
    }
    tracePtr->numPoints = length;

    iPtr = Blt_NewItem((char *)dataIndex);
    Blt_SetItemValue(iPtr, (ClientData)tracePtr);
    Blt_LinkAfter(&(linePtr->traces), iPtr, (Blt_ListItem *)NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteTraces --
 *
 *	Deletes all the traces for the line.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
DeleteTraces(linePtr)
    Line *linePtr;
{
    register Blt_ListItem *iPtr;
    Blt_Trace *tracePtr;

    for (iPtr = Blt_FirstListItem(&(linePtr->traces)); iPtr != NULL;
	iPtr = Blt_NextItem(iPtr)) {
	tracePtr = (Blt_Trace *)Blt_GetItemValue(iPtr);
	ckfree((char *)tracePtr);
    }
    Blt_ResetList(&(linePtr->traces));
}

#define CLIP_TOP	(1<<0)
#define CLIP_BOTTOM	(1<<1)
#define CLIP_RIGHT	(1<<2)
#define CLIP_LEFT	(1<<3)

INLINE static int
OutCode(regionPtr, coordPtr)
    BBox *regionPtr;
    Coordinate *coordPtr;
{
    int code;

    code = 0;
    if (coordPtr->x > regionPtr->urx) {
	code |= CLIP_RIGHT;
    } else if (coordPtr->x < regionPtr->llx) {
	code |= CLIP_LEFT;
    }
    if (coordPtr->y > regionPtr->lly) {
	code |= CLIP_BOTTOM;
    } else if (coordPtr->y < regionPtr->ury) {
	code |= CLIP_TOP;
    }
    return (code);
}

static int
ClipSegment(regionPtr, code1, code2, p1Ptr, p2Ptr)
    BBox *regionPtr;
    register int code1, code2;
    register Coordinate *p1Ptr, *p2Ptr;
{
    int inside, outside;

    inside = ((code1 | code2) == 0);
    outside = ((code1 & code2) != 0);

    /*
     * In the worst case, we'll clip the line segment against each of
     * the four sides of the bounding rectangle.
     */
    while ((!outside) && (!inside)) {
	if (code1 == 0) {
	    Coordinate *ptr;
	    int code;

	    ptr = p1Ptr;
	    p1Ptr = p2Ptr, p2Ptr = ptr;
	    code = code1, code1 = code2, code2 = code;
	}
	if (code1 & CLIP_LEFT) {
	    p1Ptr->y += (p2Ptr->y - p1Ptr->y) *
		(regionPtr->llx - p1Ptr->x) / (p2Ptr->x - p1Ptr->x);
	    p1Ptr->x = regionPtr->llx;
	} else if (code1 & CLIP_RIGHT) {
	    p1Ptr->y += (p2Ptr->y - p1Ptr->y) *
		(regionPtr->urx - p1Ptr->x) / (p2Ptr->x - p1Ptr->x);
	    p1Ptr->x = regionPtr->urx;
	} else if (code1 & CLIP_BOTTOM) {
	    p1Ptr->x += (p2Ptr->x - p1Ptr->x) *
		(regionPtr->lly - p1Ptr->y) / (p2Ptr->y - p1Ptr->y);
	    p1Ptr->y = regionPtr->lly;
	} else if (code1 & CLIP_TOP) {
	    p1Ptr->x += (p2Ptr->x - p1Ptr->x) *
		(regionPtr->ury - p1Ptr->y) / (p2Ptr->y - p1Ptr->y);
	    p1Ptr->y = regionPtr->ury;
	}
	code1 = OutCode(regionPtr, p1Ptr);

	inside = ((code1 | code2) == 0);
	outside = ((code1 & code2) != 0);
    }
    return (!inside);
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeLineCoordinates --
 *
 *	Calculates the actual window coordinates of the line element.
 *	The window coordinates are saved in an allocated point array.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is (re)allocated for the point array.
 *
 *----------------------------------------------------------------------
 */
static void
ComputeLineCoordinates(graphPtr, elemPtr)
    Graph *graphPtr;		/* Graph widget record */
    Element *elemPtr;		/* Element component record */
{
    Line *linePtr = (Line *)elemPtr;
    int offscreen, broken;
    AxisPair axisPair;
    Coordinate *p1Ptr, *p2Ptr;
    Coordinate *coordArr;
    Coordinate savePoint;
    BBox bbox;
    register int code1, code2;
    register int count;
    register int i;
    int numDataPoints;
    int start;

    DeleteTraces(linePtr);

    numDataPoints = ELEMENT_LENGTH(linePtr);
    if (numDataPoints < 1) {
	return;
    }
    coordArr = (Coordinate *) ckalloc(numDataPoints * sizeof(Coordinate));
    if (coordArr == NULL) {
	Panic("can't allocate coord array");
    }
    /*
     *  Transform the graph coordinates to screen coordinates.
     */
    Blt_GetAxisMapping(graphPtr, linePtr->axisMask, &axisPair);
    for (i = 0; i < numDataPoints; i++) {
	coordArr[i] = Blt_TransformPt(graphPtr, linePtr->x.vector.valueArr[i],
	    linePtr->y.vector.valueArr[i], &axisPair);
    }

    /*
     * Calculate the size of a symbol. We need to know how much
     * to add to the clip region so that we don't clip out
     * partially displayed points
     */
    ComputeSymbolSize(graphPtr, linePtr, &axisPair);

    /*
     * The clip region is the plotting area plus twice the size of the
     * element's symbol.  The reason we clip with a bounding box
     * larger than the plot area is so that symbols will be drawn even
     * if their center point isn't in the plotting area.  
     */
    bbox.llx = (double)(graphPtr->llx - linePtr->symbolSize);
    bbox.lly = (double)(graphPtr->lly + linePtr->symbolSize);
    bbox.urx = (double)(graphPtr->urx + linePtr->symbolSize);
    bbox.ury = (double)(graphPtr->ury - linePtr->symbolSize);

    count = 1;
    code1 = OutCode(&bbox, coordArr);
    p1Ptr = coordArr;
    for (i = 1; i < numDataPoints; i++) {
	p2Ptr = coordArr + i;
	code2 = OutCode(&bbox, p2Ptr);
	if (code2 != 0) {
	    /* Save the coordinates of the last point, before clipping */
	    savePoint = *p2Ptr;
	}
	offscreen = ClipSegment(&bbox, code1, code2, p1Ptr, p2Ptr);
	broken = BROKEN_TRACE(linePtr->traceDir, p1Ptr->x, p2Ptr->x);
	if (broken || offscreen) {
	    /*
	     * The last line segment is either totally clipped by the
	     * plotting area or the x-direction is wrong, breaking the
	     * trace.  Either way, save information about the last
	     * trace (if one exists), discarding the current line
	     * segment
	     */
	    if (count > 1) {
		start = i - count;
		CreateTrace(linePtr, start, coordArr + start, count);
		count = 1;
	    }
	} else {
	    count++;		/* Add the point to the trace. */
	    if (code2 != 0) {
		/*
		 * If the last point is clipped, this means that the
		 * trace is broken after this point.  Restore the
		 * original coordinate (before clipping) after saving
		 * the trace.
		 */
		start = i - (count - 1);
		CreateTrace(linePtr, start, coordArr + start, count);
		coordArr[i] = savePoint;
		count = 1;
	    }
	}
	code1 = code2;
	p1Ptr = p2Ptr;
    }
    if (count > 1) {
	start = i - count;
	CreateTrace(linePtr, start, coordArr + start, count);
    }
    ckfree((char *)coordArr);

}

/*
 * -----------------------------------------------------------------
 *
 * DrawSymbols --
 *
 * 	Draw the symbols centered at the each given x,y coordinate
 *	in the array of points.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Draws a symbol at each coordinate given.  If active,
 *	only those coordinates which are currently active are
 *	drawn.
 *
 * -----------------------------------------------------------------
 */
static void
DrawSymbols(graphPtr, elemPtr, size, pointArr, numPoints, active)
    Graph *graphPtr;		/* Graph widget record */
    Element *elemPtr;		/* Line element information */
    int size;			/* Size of element */
    XPoint *pointArr;		/* Array of x,y coordinates for line */
    int numPoints;		/* Number of coordinates in array */
    int active;
{
    Line *linePtr = (Line *)elemPtr;
    XPoint template[13];	/* Template for polygon symbols */
    GC outlineGC, traceGC, fillGC;
    int r1, r2;
    register int i, n;
    register int count;
#define SQRT_PI		1.77245385090552
#define S_RATIO		0.886226925452758

    /* Set the correct GCs, depending if the element is active  */
    if (active) {
	outlineGC = linePtr->activeOutlineGC;
	traceGC = linePtr->activeTraceGC;
	fillGC = linePtr->activeFillGC;
    } else {
	outlineGC = linePtr->normalOutlineGC;
	traceGC = linePtr->normalTraceGC;
	fillGC = linePtr->normalFillGC;
    }

    count = 0;
    r1 = (int)ceil(size * 0.5);
    r2 = (int)ceil(size * S_RATIO * 0.5);

    switch (linePtr->symbol) {
    case SYMBOL_NONE:
	{
	    /*
	     * Draw an extra line offset by one pixel from the previous to
	     * give a thicker appearance.  This is only for the legend entry.
	     * This routine is never called for drawing the actual line
	     * segments.
	     */
	    XDrawLine(graphPtr->display, graphPtr->pixwin, traceGC,
		pointArr[0].x - r1, pointArr[0].y,
		pointArr[0].x + r1, pointArr[0].y);
	    XDrawLine(graphPtr->display, graphPtr->pixwin, traceGC,
		pointArr[0].x - r1, pointArr[0].y + 1,
		pointArr[0].x + r1, pointArr[0].y + 1);
	}
	break;

    case SYMBOL_SQUARE:
	{
	    XRectangle *rectArr;/* Array of rectangles (square) */
	    register XRectangle *rectPtr;
	    int s;

	    s = r2 + r2;
	    rectArr = (XRectangle *)ckcalloc(numPoints, sizeof(XRectangle));
	    rectPtr = rectArr;
	    for (i = 0; i < numPoints; i++) {
		rectPtr->x = pointArr[i].x - r2;
		rectPtr->y = pointArr[i].y - r2;
		rectPtr->width = rectPtr->height = s;
		rectPtr++;
		count++;
	    }
	    if (fillGC != NULL) {
		XFillRectangles(graphPtr->display, graphPtr->pixwin, fillGC,
		    rectArr, count);
	    }
	    if ((outlineGC != NULL) && (linePtr->outlineWidth > 0)) {
		XDrawRectangles(graphPtr->display, graphPtr->pixwin, outlineGC,
		    rectArr, count);
	    }
	    ckfree((char *)rectArr);
	}
	break;

    case SYMBOL_CIRCLE:
	{
	    XArc *arcArr;	/* Array of arcs (circle) */
	    register XArc *arcPtr;
	    int s;

	    s = r1 + r1;
	    arcArr = (XArc *) ckcalloc(numPoints, sizeof(XArc));
	    arcPtr = arcArr;
	    for (i = 0; i < numPoints; i++) {
		arcPtr->x = pointArr[i].x - r1;
		arcPtr->y = pointArr[i].y - r1;
		arcPtr->width = arcArr[count].height = s;
		arcPtr->angle1 = 0;
		arcPtr->angle2 = 23040;
		arcPtr++;
		count++;
	    }
	    if (fillGC != NULL) {
		XFillArcs(graphPtr->display, graphPtr->pixwin, fillGC, arcArr,
		    count);
	    }
	    if ((outlineGC != NULL) && (linePtr->outlineWidth > 0)) {
		XDrawArcs(graphPtr->display, graphPtr->pixwin, outlineGC,
		    arcArr, count);
	    }
	    ckfree((char *)arcArr);
	}
	break;

    case SYMBOL_SPLUS:
    case SYMBOL_SCROSS:
	{
	    XSegment *segArr;	/* Array of line segments (splus, scross) */
	    register XSegment *segPtr;

	    if (linePtr->symbol == SYMBOL_SCROSS) {
		r2 = Round(r2 * M_SQRT1_2);
		template[2].x = template[0].x = template[0].y = -r2;
		template[2].y = r2;
		template[3].y = -(r2 + 1);
		template[3].x = r2 + 1;
		template[1].x = template[1].y = r2 + 1;
	    } else {
		template[0].y = template[1].y = template[2].x =
		    template[3].x = 0;
		template[0].x = template[2].y = -r2;
		template[1].x = template[3].y = r2 + 1;
	    }
	    segArr = (XSegment *)ckcalloc(numPoints * 2, sizeof(XSegment));
	    segPtr = segArr;
	    for (i = 0; i < numPoints; i++) {
		segPtr->x1 = template[0].x + pointArr[i].x;
		segPtr->y1 = template[0].y + pointArr[i].y;
		segPtr->x2 = template[1].x + pointArr[i].x;
		segPtr->y2 = template[1].y + pointArr[i].y;
		segPtr++;
		segPtr->x1 = template[2].x + pointArr[i].x;
		segPtr->y1 = template[2].y + pointArr[i].y;
		segPtr->x2 = template[3].x + pointArr[i].x;
		segPtr->y2 = template[3].y + pointArr[i].y;
		segPtr++;
		count += 2;
	    }
	    if ((outlineGC != NULL) && (linePtr->outlineWidth > 0)) {
		XDrawSegments(graphPtr->display, graphPtr->pixwin, outlineGC,
		    segArr, count);
	    }
	    ckfree((char *)segArr);
	}
	break;

    case SYMBOL_PLUS:
    case SYMBOL_CROSS:
	{
	    XPoint *coordArr;
	    register XPoint *coordPtr;
	    int d;		/* Small delta for cross/plus thickness */

	    d = (r2 / 3);

	    /*
	     *
	     *          2   3       The plus/cross symbol is a closed polygon
	     *                      of 12 points. The diagram to the left
	     *    0,12  1   4    5  represents the positions of the points
	     *           x,y        which are computed below. The extra
	     *     11  10   7    6  (thirteenth) point connects the first and
	     *                      last points.
	     *          9   8
	     */

	    template[0].x = template[11].x = template[12].x = -r2;
	    template[2].x = template[1].x = template[10].x = template[9].x = -d;
	    template[3].x = template[4].x = template[7].x = template[8].x = d;
	    template[5].x = template[6].x = r2;
	    template[2].y = template[3].y = -r2;
	    template[0].y = template[1].y = template[4].y = template[5].y =
		template[12].y = -d;
	    template[11].y = template[10].y = template[7].y = template[6].y = d;
	    template[9].y = template[8].y = r2;

	    if (linePtr->symbol == SYMBOL_CROSS) {
		double dx, dy;

		/* For the cross symbol, rotate the points by 45 degrees. */
		for (n = 0; n < 12; n++) {
		    dx = (double)template[n].x * M_SQRT1_2;
		    dy = (double)template[n].y * M_SQRT1_2;
		    template[n].x = Round(dx - dy);
		    template[n].y = Round(dx + dy);
		}
		template[12] = template[0];
	    }
	    coordArr = (XPoint *)ckcalloc(numPoints * 13, sizeof(XPoint));
	    coordPtr = coordArr;
	    for (i = 0; i < numPoints; i++) {
		for (n = 0; n < 13; n++) {
		    coordPtr->x = template[n].x + pointArr[i].x;
		    coordPtr->y = template[n].y + pointArr[i].y;
		    coordPtr++;
		}
		count++;
	    }

	    if (fillGC != NULL) {
		coordPtr = coordArr;
		for (i = 0; i < count; i++) {
		    XFillPolygon(graphPtr->display, graphPtr->pixwin, fillGC,
			coordPtr, 13, Complex, CoordModeOrigin);
		    coordPtr += 13;
		}
	    }
	    if ((outlineGC != NULL) && (linePtr->outlineWidth > 0)) {
		coordPtr = coordArr;
		for (i = 0; i < count; i++) {
		    XDrawLines(graphPtr->display, graphPtr->pixwin, outlineGC,
			coordPtr, 13, CoordModeOrigin);
		    coordPtr += 13;
		}
	    }
	    ckfree((char *)coordArr);
	}
	break;

    case SYMBOL_DIAMOND:
	{
	    XPoint *coordArr;

	    register XPoint *coordPtr;

	    /*
	     *
	     *                      The plus symbol is a closed polygon
	     *            1         of 4 points. The diagram to the left
	     *                      represents the positions of the points
	     *       0,4 x,y  2     which are computed below. The extra
	     *                      (fifth) point connects the first and
	     *            3         last points.
	     *
	     */
	    template[1].y = template[0].x = -r1;
	    template[2].y = template[3].x = template[0].y = template[1].x = 0;
	    template[3].y = template[2].x = r1;
	    template[4] = template[0];

	    coordArr = (XPoint *)ckcalloc(numPoints * 5, sizeof(XPoint));
	    coordPtr = coordArr;
	    for (i = 0; i < numPoints; i++) {
		for (n = 0; n < 5; n++) {
		    coordPtr->x = template[n].x + pointArr[i].x;
		    coordPtr->y = template[n].y + pointArr[i].y;
		    coordPtr++;
		}
		count++;
	    }
	    if (fillGC != NULL) {
		coordPtr = coordArr;
		for (i = 0; i < count; i++) {
		    XFillPolygon(graphPtr->display, graphPtr->pixwin, fillGC,
			coordPtr, 5, Convex, CoordModeOrigin);
		    coordPtr += 5;
		}
	    }
	    if ((outlineGC != NULL) && (linePtr->outlineWidth > 0)) {
		coordPtr = coordArr;
		for (i = 0; i < count; i++) {
		    XDrawLines(graphPtr->display, graphPtr->pixwin, outlineGC,
			coordPtr, 5, CoordModeOrigin);
		    coordPtr += 5;
		}
	    }
	    ckfree((char *)coordArr);
	}
	break;

    case SYMBOL_TRIANGLE:
	{
	    XPoint *coordArr;
	    register XPoint *coordPtr;
	    double b;
	    int b2, h1, h2;
#define H_RATIO		1.1663402261671607
#define B_RATIO		1.3467736870885982
#define TAN30		0.57735026918962573
#define COS30		0.86602540378443871

	    b = Round(size * B_RATIO * 0.7);
	    b2 = Round(b * 0.5);
	    h2 = Round(TAN30 * b * 0.5);
	    h1 = Round((b * 0.5) / COS30);
	    /*
	     *
	     *                      The triangle symbol is a closed polygon
	     *           0,3         of 3 points. The diagram to the left
	     *                      represents the positions of the points
	     *           x,y        which are computed below. The extra
	     *                      (fourth) point connects the first and
	     *      2           1   last points.
	     *
	     */

	    template[3].x = template[0].x = 0;
	    template[3].y = template[0].y = -h1;
	    template[1].x = b2;
	    template[2].y = template[1].y = h2;
	    template[2].x = -b2;

	    coordArr = (XPoint *)ckcalloc(numPoints * 4, sizeof(XPoint));
	    coordPtr = coordArr;
	    for (i = 0; i < numPoints; i++) {
		for (n = 0; n < 4; n++) {
		    coordPtr->x = template[n].x + pointArr[i].x;
		    coordPtr->y = template[n].y + pointArr[i].y;
		    coordPtr++;
		}
		count++;
	    }
	    if (fillGC) {
		coordPtr = coordArr;
		for (i = 0; i < count; i++) {
		    XFillPolygon(graphPtr->display, graphPtr->pixwin, fillGC,
			coordPtr, 4, Convex, CoordModeOrigin);
		    coordPtr += 4;
		}
	    }
	    if ((outlineGC != NULL) && (linePtr->outlineWidth > 0)) {
		coordPtr = coordArr;
		for (i = 0; i < count; i++) {
		    XDrawLines(graphPtr->display, graphPtr->pixwin, outlineGC,
			coordPtr, 4, CoordModeOrigin);
		    coordPtr += 4;
		}
	    }
	    ckfree((char *)coordArr);
	}
	break;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * GetVisibleActivePoints --
 *
 * 	Generates an array of points representing the active, 
 *	visible points of a line element.  Each data point marked
 *	as active is examined to see if it's currently visible
 *	on the graph.  
 *
 *	Two features of the "activate" operation are 1) the indices 
 *	of the data points don't have to exist when they are specified.  
 *	This means that we need to check for the index each time the 
 *	graph is redrawn; and 2) the draw/un-drawing of active points must 
 *	be cheap.  We can't recalculate and redrawn the entire set of 
 *	elements each time an active point is selected.  The trade-off 
 *	used here is to determine if the points are visible at run-time.  
 *	I'm assuming that the number of active points will be relatively 
 *	small.  So re-testing visibility of the	points shouldn't matter
 *	much in terms of performance.
 *
 * Results:
 *	Returns the number of visible active segments and stores
 *	in *pointPtrPtr*, a malloc-ed array of XPoint's.
 *	The calling routine is responsible for freeing the memory.
 *
 * ----------------------------------------------------------------------
 */
static int
GetVisibleActivePoints(linePtr, pointPtrPtr)
    Line *linePtr;
    XPoint **pointPtrPtr;
{
    Graph *graphPtr = linePtr->graphPtr;
    AxisPair axisPair;
    BBox bbox;
    XPoint *pointArr;
    register XPoint *pointPtr;
    Coordinate coord;
    int numDataPoints, numActive;
    int code;
    int activeIndex;
    double *xArr, *yArr;
    register int i;

    /*
     * The clip region is the plotting area plus twice the size of the
     * element's symbol.  The reason we clip with a bounding box
     * larger than the plot area is so that symbols will be drawn even
     * if their center point isn't in the plotting area.  
     */
    bbox.llx = (double)(graphPtr->llx - linePtr->symbolSize);
    bbox.lly = (double)(graphPtr->lly + linePtr->symbolSize);
    bbox.urx = (double)(graphPtr->urx + linePtr->symbolSize);
    bbox.ury = (double)(graphPtr->ury - linePtr->symbolSize);

    pointArr = (XPoint *)ckalloc(sizeof(XPoint) * linePtr->numActiveIndices);
    if (pointArr == NULL) {
	Panic("can't allocate array of active points");
    }

    numActive = 0;
    pointPtr = pointArr;
    numDataPoints = ELEMENT_LENGTH(linePtr);
    Blt_GetAxisMapping(graphPtr, linePtr->axisMask, &axisPair);

    xArr = linePtr->x.vector.valueArr;
    yArr = linePtr->y.vector.valueArr;
    for (i = 0; i < linePtr->numActiveIndices; i++) {
	activeIndex = linePtr->activeIndexArr[i];
	if ((activeIndex < 0) || (activeIndex >= numDataPoints)) {
	    continue;
	}
	coord = Blt_TransformPt(graphPtr, xArr[activeIndex], yArr[activeIndex],
	   &axisPair);
	code = OutCode(&bbox, &coord);
	if (code == 0) {
	    pointPtr->x = Round(coord.x), pointPtr->y = Round(coord.y);
	    pointPtr++;
	    numActive++;
	}
    }
    *pointPtrPtr = pointArr;
    if (numActive == 0) {
	ckfree((char *)pointArr);
    }
    return (numActive);
}


/*
 *----------------------------------------------------------------------
 *
 * DrawActiveLine --
 *
 *	Draws the connected line(s) representing the element. If the
 *	line is made up of non-line symbols and the line width parameter
 *	has been set (linewidth > 0), the element will also be drawn as
 *	a line (with the linewidth requested).  The line may consist of
 *	separate line segments.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	X drawing commands are output.
 *
 *----------------------------------------------------------------------
 */
static void
DrawActiveLine(graphPtr, elemPtr)
    Graph *graphPtr;		/* Graph widget record */
    Element *elemPtr;		/* Element to be drawn */
{
    Line *linePtr = (Line *)elemPtr;

    if (linePtr->numActiveIndices > 0) {
	XPoint *activeArr;
	int numActive;

	numActive = GetVisibleActivePoints(linePtr, &activeArr);
	if (numActive > 0) {
	    DrawSymbols(graphPtr, (Element *)linePtr, linePtr->symbolSize,
		activeArr, numActive, TRUE);
	    ckfree((char *)activeArr);
	}
    } else if ((linePtr->numActiveIndices < 0) && 
	       (linePtr->traces.numEntries > 0)) {
	register Blt_ListItem *iPtr;
	register Blt_Trace *tracePtr;
	
	for (iPtr = Blt_FirstListItem(&(linePtr->traces)); iPtr != NULL;
	     iPtr = Blt_NextItem(iPtr)) {
	    tracePtr = (Blt_Trace *)Blt_GetItemValue(iPtr);
	    XDrawLines(graphPtr->display, graphPtr->pixwin,
	        linePtr->activeTraceGC, tracePtr->pointArr, tracePtr->numPoints,
		CoordModeOrigin);
	}
	if (linePtr->symbol != SYMBOL_NONE) { /* Draw non-line symbols */
	    for (iPtr = Blt_FirstListItem(&(linePtr->traces)); iPtr != NULL;
		 iPtr = Blt_NextItem(iPtr)) {
		tracePtr = (Blt_Trace *)Blt_GetItemValue(iPtr);
		DrawSymbols(graphPtr, (Element *)linePtr, linePtr->symbolSize, 
			tracePtr->pointArr, tracePtr->numPoints, TRUE);
	    }
	}
    }
} 

/*
 *----------------------------------------------------------------------
 *
 * DrawNormalLine --
 *
 *	Draws the connected line(s) representing the element. If the
 *	line is made up of non-line symbols and the line width parameter
 *	has been set (linewidth > 0), the element will also be drawn as
 *	a line (with the linewidth requested).  The line may consist of
 *	separate line segments.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	X drawing commands are output.
 *
 *----------------------------------------------------------------------
 */
static void
DrawNormalLine(graphPtr, elemPtr)
    Graph *graphPtr;		/* Graph widget record */
    Element *elemPtr;		/* Element to be drawn */
{
    Line *linePtr = (Line *)elemPtr;
    register Blt_ListItem *iPtr;
    register Blt_Trace *tracePtr;

#ifdef notdef
    fprintf(stderr, "%s has #%d traces\n", linePtr->nameId,
	linePtr->traces.numEntries);
#endif
    if (linePtr->traces.numEntries < 1) {
	return;
    }
    if ((linePtr->traceWidth > 0) || (linePtr->symbol == SYMBOL_NONE)) {
	/*
	 * Draw lines if symbol is "line" or linewidth is greater than 0
	 */
	for (iPtr = Blt_FirstListItem(&(linePtr->traces)); iPtr != NULL;
	    iPtr = Blt_NextItem(iPtr)) {
	    tracePtr = (Blt_Trace *)Blt_GetItemValue(iPtr);
	    XDrawLines(graphPtr->display, graphPtr->pixwin,
		linePtr->normalTraceGC, tracePtr->pointArr, tracePtr->numPoints,
		CoordModeOrigin);
	}
    }
    if (linePtr->symbol != SYMBOL_NONE) {
	/* Draw non-line symbols */
	for (iPtr = Blt_FirstListItem(&(linePtr->traces)); iPtr != NULL;
	    iPtr = Blt_NextItem(iPtr)) {
	    tracePtr = (Blt_Trace *)Blt_GetItemValue(iPtr);
	    DrawSymbols(graphPtr, (Element *)linePtr, linePtr->symbolSize,
		tracePtr->pointArr, tracePtr->numPoints, FALSE);
	}
    }
}

/*
 * -----------------------------------------------------------------
 *
 * GetSymbolPrintInfo --
 *
 *	Set up the PostScript environment with the macros and
 *	attributes needed to draw the symbols of the element.
 *
 * Results:
 *	None.
 *
 * -----------------------------------------------------------------
 */
static void
GetSymbolPrintInfo(graphPtr, linePtr, active)
    Graph *graphPtr;
    Line *linePtr;
    int active;
{
    XColor *outlineColor, *fillColor, *defaultColor;

    /* Set line and foreground attributes */
    if (active) {
	outlineColor = linePtr->activeOutlineColor;
	fillColor = linePtr->activeFillColor;
	defaultColor = linePtr->activeTraceColor;
    } else {
	outlineColor = linePtr->outlineColor;
	fillColor = linePtr->fillColor;
	defaultColor = linePtr->traceColor;
    }
    if (fillColor == COLOR_DEFAULT) {
	fillColor = defaultColor;
    }
    if (outlineColor == COLOR_DEFAULT) {
	outlineColor = defaultColor;
    }
    if (linePtr->symbol == SYMBOL_NONE) {
	Blt_SetLineAttributes(graphPtr, defaultColor, linePtr->traceWidth + 2,
	    &(linePtr->dashes));
    } else {
	Blt_LineWidthToPostScript(graphPtr, linePtr->outlineWidth);
	Blt_LineDashesToPostScript(graphPtr, (Dashes *)NULL);
    }

    /*
     * Build a PostScript procedure to draw the fill and outline of
     * the symbols after the path of the symbol shape has been formed
     */
    Tcl_AppendResult(graphPtr->interp, "\n/DrawSymbolProc {\n  gsave\n",
	(char *)NULL);
    if (linePtr->symbol != SYMBOL_NONE) {
	if (fillColor != NULL) {
	    Tcl_AppendResult(graphPtr->interp, "    ", (char *)NULL);
	    Blt_BackgroundToPostScript(graphPtr, fillColor);
	    Tcl_AppendResult(graphPtr->interp, "    Fill\n", (char *)NULL);
	}
	if ((outlineColor != NULL) && (linePtr->outlineWidth > 0)) {
	    Tcl_AppendResult(graphPtr->interp, "    ", (char *)NULL);
	    Blt_ForegroundToPostScript(graphPtr, outlineColor);
	    Tcl_AppendResult(graphPtr->interp, "    stroke\n", (char *)NULL);
	}
    }
    Tcl_AppendResult(graphPtr->interp, "  grestore\n} def\n\n", (char *)NULL);
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
static void
PrintSymbols(graphPtr, elemPtr, size, pointArr, numPoints, active)
    Graph *graphPtr;
    Element *elemPtr;
    int size;
    XPoint *pointArr;
    int numPoints;
    int active;
{
    Line *linePtr = (Line *)elemPtr;
    double symbolSize;
    register int i;

    GetSymbolPrintInfo(graphPtr, linePtr, active);
    symbolSize = (double)size;
    if ((linePtr->symbol == SYMBOL_SQUARE) ||
	(linePtr->symbol == SYMBOL_CROSS) ||
	(linePtr->symbol == SYMBOL_PLUS) ||
	(linePtr->symbol == SYMBOL_SCROSS) ||
	(linePtr->symbol == SYMBOL_SPLUS)) {
	symbolSize = Round(size * S_RATIO);
    } else if (linePtr->symbol == SYMBOL_TRIANGLE) {
	symbolSize = Round(size * 0.7);
    } else if (linePtr->symbol == SYMBOL_DIAMOND) {
	symbolSize = Round(size * M_SQRT1_2);
    }
    for (i = 0; i < numPoints; i++) {
	sprintf(graphPtr->scratchArr, "%d %d %g %s\n", pointArr[i].x,
	    pointArr[i].y, symbolSize, symbolMacros[linePtr->symbol]);
	Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PrintActiveLine --
 *
 *	Generates PostScript commands to draw as "active" the points
 *	(symbols) and or line segments (trace) representing the
 *	element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PostScript pen width, dashes, and color settings are changed.
 *
 *----------------------------------------------------------------------
 */
static void
PrintActiveLine(graphPtr, elemPtr)
    Graph *graphPtr;
    Element *elemPtr;
{
    Line *linePtr = (Line *)elemPtr;

    /* Set line and foreground attributes */
    Blt_SetLineAttributes(graphPtr, linePtr->activeTraceColor,
	linePtr->activeTraceWidth, &(linePtr->dashes));

    if ((linePtr->dashes.numValues > 0) && (linePtr->gapColor != NULL)) {
	Tcl_AppendResult(graphPtr->interp, "/DashesProc {\n  gsave\n    ",
	    (char *)NULL);
	Blt_BackgroundToPostScript(graphPtr, linePtr->gapColor);
	Tcl_AppendResult(graphPtr->interp, "    ", (char *)NULL);
	Blt_LineDashesToPostScript(graphPtr, (Dashes *)NULL);
	Tcl_AppendResult(graphPtr->interp, "stroke\n  grestore\n} def\n",
	    (char *)NULL);
    } else {
	Tcl_AppendResult(graphPtr->interp, "/DashesProc {} def\n",
	    (char *)NULL);
    }
    if (linePtr->numActiveIndices > 0) {
	XPoint *activeArr;
	int numActive;

	numActive = GetVisibleActivePoints(linePtr, &activeArr);
	if (numActive > 0) {
	    PrintSymbols(graphPtr, (Element *)linePtr, linePtr->symbolSize,
	        activeArr, numActive, TRUE);
	    ckfree((char *)activeArr);
	}
    } else if ((linePtr->numActiveIndices < 0) && 
	       (linePtr->traces.numEntries > 0)) {
	register Blt_ListItem *iPtr;
	register Blt_Trace *tracePtr;

	for (iPtr = Blt_FirstListItem(&(linePtr->traces)); iPtr != NULL;
	    iPtr = Blt_NextItem(iPtr)) {
	    tracePtr = (Blt_Trace *)Blt_GetItemValue(iPtr);
	    Blt_PrintLine(graphPtr, tracePtr->pointArr, tracePtr->numPoints);
	}
	if (linePtr->symbol != SYMBOL_NONE) {
	    for (iPtr = Blt_FirstListItem(&(linePtr->traces)); iPtr != NULL;
		iPtr = Blt_NextItem(iPtr)) {
		tracePtr = (Blt_Trace *)Blt_GetItemValue(iPtr);
		PrintSymbols(graphPtr, (Element *)linePtr, linePtr->symbolSize,
		    tracePtr->pointArr, tracePtr->numPoints, TRUE);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PrintLine --
 *
 *	Similar to the DrawLine procedure, prints PostScript related
 *	commands to form the connected line(s) representing the element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PostScript pen width, dashes, and color settings are changed.
 *
 *----------------------------------------------------------------------
 */
static void
PrintNormalLine(graphPtr, elemPtr)
    Graph *graphPtr;
    Element *elemPtr;
{
    Line *linePtr = (Line *)elemPtr;
    register Blt_ListItem *iPtr;
    register Blt_Trace *tracePtr;

    if (linePtr->traces.numEntries < 1) {
	return;
    }
    /* Set the attributes of the line (color, dashes, linewidth) */
    Blt_SetLineAttributes(graphPtr, linePtr->traceColor, linePtr->traceWidth,
	  &(linePtr->dashes));

    if ((linePtr->dashes.numValues > 0) && (linePtr->gapColor != NULL)) {
	Tcl_AppendResult(graphPtr->interp, "/DashesProc {\n  gsave\n    ",
	    (char *)NULL);
	Blt_BackgroundToPostScript(graphPtr, linePtr->gapColor);
	Tcl_AppendResult(graphPtr->interp, "    ", (char *)NULL);
	Blt_LineDashesToPostScript(graphPtr, (Dashes *)NULL);
	Tcl_AppendResult(graphPtr->interp, "stroke\n  grestore\n} def\n",
	    (char *)NULL);
    } else {
	Tcl_AppendResult(graphPtr->interp, "/DashesProc {} def\n",
	    (char *)NULL);
    }
    if ((linePtr->traceWidth > 0) || (linePtr->symbol == SYMBOL_NONE)) {
	for (iPtr = Blt_FirstListItem(&(linePtr->traces)); iPtr != NULL;
	    iPtr = Blt_NextItem(iPtr)) {
	    tracePtr = (Blt_Trace *)Blt_GetItemValue(iPtr);
	    Blt_PrintLine(graphPtr, tracePtr->pointArr, tracePtr->numPoints);
	}
    }
    if (linePtr->symbol != SYMBOL_NONE) {
	for (iPtr = Blt_FirstListItem(&(linePtr->traces)); iPtr != NULL;
	    iPtr = Blt_NextItem(iPtr)) {
	    tracePtr = (Blt_Trace *)Blt_GetItemValue(iPtr);
	    PrintSymbols(graphPtr, (Element *)linePtr, linePtr->symbolSize,
		tracePtr->pointArr, tracePtr->numPoints, FALSE);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyLine --
 *
 *	Release memory and resources allocated for the line element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the line element is freed up.
 *
 *----------------------------------------------------------------------
 */
static void
DestroyLine(graphPtr, elemPtr)
    Graph *graphPtr;
    Element *elemPtr;
{
    Line *linePtr = (Line *)elemPtr;

    Tk_FreeOptions(linePtr->configSpecs, (char *)linePtr, graphPtr->display, 0);

    DeleteTraces(linePtr);

    if (linePtr->normalOutlineGC != NULL) {
	Tk_FreeGC(graphPtr->display, linePtr->normalOutlineGC);
    }
    if (linePtr->normalFillGC != NULL) {
	Tk_FreeGC(graphPtr->display, linePtr->normalFillGC);
    }
    if (linePtr->normalTraceGC != NULL) {
	XFreeGC(graphPtr->display, linePtr->normalTraceGC);
    }
    if (linePtr->activeOutlineGC != NULL) {
	Tk_FreeGC(graphPtr->display, linePtr->activeOutlineGC);
    }
    if (linePtr->activeFillGC != NULL) {
	Tk_FreeGC(graphPtr->display, linePtr->activeFillGC);
    }
    if (linePtr->activeTraceGC != NULL) {
	Tk_FreeGC(graphPtr->display, linePtr->activeTraceGC);
    }
    if (linePtr->x.clientId != NULL) {
	Blt_FreeVectorId(linePtr->x.clientId);
    } else if (linePtr->x.vector.valueArr != NULL) {
	ckfree((char *)linePtr->x.vector.valueArr);
    }
    if (linePtr->y.clientId != NULL) {
	Blt_FreeVectorId(linePtr->y.clientId);
    } else if (linePtr->y.vector.valueArr != NULL) {
	ckfree((char *)linePtr->y.vector.valueArr);
    }
    if (linePtr->activeIndexArr != NULL) {
	ckfree((char *)linePtr->activeIndexArr);
    }
    ckfree((char *)linePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_LineElement --
 *
 *	Allocate memory and initialize methods for the new line element.
 *
 * Results:
 *	The pointer to the newly allocated element structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the line element structure.
 *
 *----------------------------------------------------------------------
 */
Element *
Blt_LineElement()
{
    register Line *linePtr;

    linePtr = (Line *)ckcalloc(1, sizeof(Line));
    if (linePtr == NULL) {
	Panic("can't allocate a line element structure");
    }
    linePtr->configSpecs = configSpecs;
    linePtr->configProc = (ElemConfigProc*)ConfigureLine;
    linePtr->destroyProc = (ElemDestroyProc*)DestroyLine;
    linePtr->drawNormalProc = (ElemDrawProc*)DrawNormalLine;
    linePtr->drawActiveProc = (ElemDrawProc*)DrawActiveLine;
    linePtr->limitsProc = (ElemLimitsProc*)GetLineLimits;
    linePtr->closestProc = (ElemClosestProc*)ClosestLine;
    linePtr->coordsProc = (ElemCoordsProc*)ComputeLineCoordinates;
    linePtr->printNormalProc = (ElemPrintProc*)PrintNormalLine;
    linePtr->printActiveProc = (ElemPrintProc*)PrintActiveLine;
    linePtr->drawSymbolsProc = (ElemDrawSymbolsProc*)DrawSymbols;
    linePtr->printSymbolsProc = (ElemPrintSymbolsProc*)PrintSymbols;
    linePtr->outlineWidth = 1;
    linePtr->activeTraceWidth = linePtr->traceWidth = 1;
    linePtr->traceDir = TRACE_BOTH;
    linePtr->symbol = SYMBOL_NONE;
    linePtr->type = ELEM_LINE;
    linePtr->flags = LINE_INIT_SYMBOL_SCALE;
    Blt_InitList(&(linePtr->traces), TCL_ONE_WORD_KEYS);
    return ((Element *)linePtr);
}
#endif /* !NO_GRAPH */
