/*
 * bltGrMarker.c --
 *
 *	This module implements markers for the BLT graph widget.
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

static int CoordinatesParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *CoordinatesPrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,Tcl_FreeProc **freeProcPtr));

static Tk_CustomOption coordsOption =
{
    CoordinatesParseProc, CoordinatesPrintProc, (ClientData)0
};

extern Tk_CustomOption bltMapXAxisOption;
extern Tk_CustomOption bltMapYAxisOption;
extern Tk_CustomOption bltLengthOption;
extern Tk_CustomOption bltDashesOption;
extern Tk_CustomOption bltPadOption;


/*
 * Marker types:
 *
 * 	A marker can be either a text, bitmap, connected line, window,
 *	or polygon.
 */

#undef NO_WINDOW_MARKERS

typedef struct Marker Marker;

/*
 * -------------------------------------------------------------------
 *
 * MarkerType --
 *
 *	Enumerate types of the various varieties of markers.
 *
 * -------------------------------------------------------------------
 */
typedef enum {
    MARKER_TYPE_TEXT,
    MARKER_TYPE_LINE,
    MARKER_TYPE_POLYGON,
    MARKER_TYPE_BITMAP,
#ifndef NO_WINDOW_MARKERS
    MARKER_TYPE_WINDOW
#endif /* NO_WINDOW_MARKERS */
} MarkerType;

/*
 * This structure corresponds with the specific types of markers.
 * Don't change this structure without changing the individual
 * marker structures of each type below.
 */

typedef void (MarkerDrawProc) _ANSI_ARGS_((Marker * markerPtr));
typedef void (MarkerFreeProc) _ANSI_ARGS_((Graph *graphPtr, Marker *markerPtr));
typedef int (MarkerConfigProc) _ANSI_ARGS_((Marker * markerPtr));
typedef void (MarkerCoordsProc) _ANSI_ARGS_((Marker * markerPtr));
typedef void (MarkerPrintProc) _ANSI_ARGS_((Marker * markerPtr));

/*
 * -------------------------------------------------------------------
 *
 * Marker --
 *
 *	Structure defining the generic marker.  In C++ parlance
 *	this would be the base type from which all markers are
 *	derived.
 *
 * -------------------------------------------------------------------
 */
struct Marker {
    Graph *graphPtr;		/* Graph marker belongs to */
    MarkerType type;		/* Type of marker */
    int flags;
    Tk_Uid nameId;		/* Identifier for marker in list */
    Coordinate *coordArr;	/* Coordinate array to position marker */
    int numCoords;		/* Number of points in above array */
    Tk_ConfigSpec *configSpecs;	/* Marker configuration specifications */
    Tk_Uid elemId;		/* Element associated with marker */
    unsigned int axisMask;	/* Indicates which axis to map element's
				 * coordinates onto */
    int drawUnder;		/* If non-zero, draw the marker underneath
				 * any elements. This can be a performance
				 * penalty because the graph must be redraw
				 * entirely each time the marker is
				 * redrawn. */
    int mapped;			/* Indicates if the marker is currently
				 * mapped or not. */
    int clipped;		/* Indicates if the marker is totally clipped 
				 * by the plotting area. */
    int xOffset, yOffset;	/* Pixel offset from graph position */

    MarkerDrawProc *drawProc;
    MarkerFreeProc *freeProc;
    MarkerConfigProc *configProc;
    MarkerCoordsProc *coordsProc;
    MarkerPrintProc *printProc;
};

#define DEF_MARKER_ANCHOR	"center"
#define DEF_MARKER_BG_COLOR	WHITE
#define DEF_MARKER_BG_MONO	WHITE
#define DEF_MARKER_BITMAP	(char *)NULL
#define DEF_MARKER_COORDS	(char *)NULL
#define DEF_MARKER_DASHES	(char *)NULL
#define DEF_MARKER_ELEMENT	(char *)NULL
#define DEF_MARKER_FG_COLOR	BLACK
#define DEF_MARKER_FG_MONO	BLACK
#define DEF_MARKER_FILL_COLOR	RED
#define DEF_MARKER_FILL_MONO	WHITE
#define DEF_MARKER_FONT		STD_FONT
#define DEF_MARKER_HEIGHT	"0"
#define DEF_MARKER_JUSTIFY	"left"
#define DEF_MARKER_LINE_WIDTH	"1"
#define DEF_MARKER_MAPPED	"1"
#define DEF_MARKER_MAP_X	"x"
#define DEF_MARKER_MAP_Y	"y"
#define DEF_MARKER_NAME		(char *)NULL
#define DEF_MARKER_OUTLINE_COLOR BLACK
#define DEF_MARKER_OUTLINE_MONO	BLACK
#define DEF_MARKER_PAD		"4"
#define DEF_MARKER_ROTATE	"0.0"
#define DEF_MARKER_STIPPLE	(char *)NULL
#define DEF_MARKER_TEXT		(char *)NULL
#define DEF_MARKER_UNDER	"0"
#define DEF_MARKER_WIDTH	"0"
#define DEF_MARKER_WINDOW	(char *)NULL
#define DEF_MARKER_X_OFFSET	"0"
#define DEF_MARKER_Y_OFFSET	"0"

/*
 * -------------------------------------------------------------------
 *
 * TextMarker --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    Graph *graphPtr;		/* The graph this marker belongs to */
    MarkerType type;		/* Type of marker */
    int flags;
    Tk_Uid nameId;		/* Identifier for marker */
    Coordinate *coordArr;	/* Coordinate array to position marker */
    int numCoords;		/* Number of points */
    Tk_ConfigSpec *configSpecs;	/* Configuration specifications */
    Tk_Uid elemId;		/* Element associated with marker */
    unsigned int axisMask;	/* Indicates which axes to element's map
				 * coordinates onto */
    int drawUnder;		/* If non-zero, draw the marker underneath
				 * any elements. There can be a performance
				 * because the graph must be redraw entirely
				 * each time this marker is redrawn. */
    int mapped;			/* Indicates if the marker is currently
				 * mapped or not. */
    int clipped;		/* Indicates if the marker is totally clipped 
				 * by the plotting area. */
    int xOffset, yOffset;	/* pixel offset from anchor */

    MarkerDrawProc *drawProc;
    MarkerFreeProc *freeProc;
    MarkerConfigProc *configProc;
    MarkerCoordsProc *coordsProc;
    MarkerPrintProc *printProc;

    /*
     * Text specific fields and attributes
     */
    char *text;			/* Text to display in graph (malloc'ed) or
				 * NULL. */
    TextAttributes attr;	/* Attributes (font, fg, bg, anchor, etc) */
    double rotate;		/* Requested rotation of the text */
    int x, y;			/* Window x, y position of marker */
} TextMarker;


static Tk_ConfigSpec textConfigSpecs[] =
{
    {TK_CONFIG_ANCHOR, "-anchor", "textMarkerAnchor", "MarkerAnchor",
	DEF_MARKER_ANCHOR, Tk_Offset(TextMarker, attr.anchor), 0},
    {TK_CONFIG_COLOR, "-background", "textMarkerBackground", "MarkerBackground",
	DEF_MARKER_BG_COLOR, Tk_Offset(TextMarker, attr.bgColorPtr),
	TK_CONFIG_NULL_OK | TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-background", "textMarkerBackground", "MarkerBackground",
	DEF_MARKER_BG_MONO, Tk_Offset(TextMarker, attr.bgColorPtr),
	TK_CONFIG_MONO_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-bg", "textMarkerBackground", "MarkerBackground",
	(char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-coords", "textMarkerCoords", "MarkerCoords",
	DEF_MARKER_COORDS, Tk_Offset(TextMarker, coordArr),
	TK_CONFIG_NULL_OK, &coordsOption},
    {TK_CONFIG_UID, "-element", "textMarkerElement", "MarkerElement",
	DEF_MARKER_ELEMENT, Tk_Offset(TextMarker, elemId), TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-fg", "textMarkerForeground", "MarkerForeground",
	(char *)NULL, 0, 0},
    {TK_CONFIG_FONT, "-font", "textMarkerFont", "MarkerFont",
	DEF_MARKER_FONT, Tk_Offset(TextMarker, attr.fontPtr), 0},
    {TK_CONFIG_COLOR, "-foreground", "textMarkerForeground", "MarkerForeground",
	DEF_MARKER_FG_COLOR, Tk_Offset(TextMarker, attr.fgColorPtr),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "textMarkerForeground", "MarkerForeground",
	DEF_MARKER_FG_MONO, Tk_Offset(TextMarker, attr.fgColorPtr),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_JUSTIFY, "-justify", "textMarkerJustify", "MarkerJustify",
	DEF_MARKER_JUSTIFY, Tk_Offset(TextMarker, attr.justify),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-mapped", "textMarkerMapped", "MarkerMapped",
	DEF_MARKER_MAPPED, Tk_Offset(TextMarker, mapped),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "textMarkerMapX", "MarkerMapX",
	DEF_MARKER_MAP_X, Tk_Offset(TextMarker, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "textMarkerMapY", "MarkerMapY",
	DEF_MARKER_MAP_Y, Tk_Offset(TextMarker, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapYAxisOption},
    {TK_CONFIG_UID, "-name", (char *)NULL, (char *)NULL,
	DEF_MARKER_NAME, Tk_Offset(TextMarker, nameId), 0},
    {TK_CONFIG_CUSTOM, "-padx", "textMarkerPadX", "MarkerPadX",
	DEF_MARKER_PAD, Tk_Offset(TextMarker, attr.padX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", "textMarkerPadY", "MarkerPadY",
	DEF_MARKER_PAD, Tk_Offset(TextMarker, attr.padY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_DOUBLE, "-rotate", "textMarkerRotate", "MarkerRotate",
	DEF_MARKER_ROTATE, Tk_Offset(TextMarker, rotate),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_STRING, "-text", "textMarkerText", "MarkerText",
	DEF_MARKER_TEXT, Tk_Offset(TextMarker, text), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-under", "textMarkerUnder", "MarkerUnder",
	DEF_MARKER_UNDER, Tk_Offset(TextMarker, drawUnder),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-xoffset", "textMarkerXOffset", "MarkerXOffset",
	DEF_MARKER_X_OFFSET, Tk_Offset(TextMarker, xOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-yoffset", "textMarkerYOffset", "MarkerYOffset",
	DEF_MARKER_Y_OFFSET, Tk_Offset(TextMarker, yOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

#ifndef NO_WINDOW_MARKERS

/*
 * -------------------------------------------------------------------
 *
 * WindowMarker --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    Graph *graphPtr;		/* Graph marker belongs to */
    MarkerType type;		/* Type of marker */
    int flags;
    Tk_Uid nameId;		/* Identifier for marker */
    Coordinate *coordArr;	/* Coordinate array to position marker */
    int numCoords;		/* Number of points */
    Tk_ConfigSpec *configSpecs;	/* Configuration specifications */
    Tk_Uid elemId;		/* Element associated with marker */
    unsigned int axisMask;	/* Indicates which axis to element's map
				 * y-coordinates onto */
    int drawUnder;		/* If non-zero, draw the marker underneath
				 * any elements. There can be a performance
				 * because the graph must be redraw entirely
				 * each time this marker is redrawn. */
    int mapped;			/* Indicates if the marker is currently
				 * mapped or not. */
    int clipped;		/* Indicates if the marker is totally clipped 
				 * by the plotting area. */
    int xOffset, yOffset;	/* Pixel offset from anchor */

    MarkerDrawProc *drawProc;
    MarkerFreeProc *freeProc;
    MarkerConfigProc *configProc;
    MarkerCoordsProc *coordsProc;
    MarkerPrintProc *printProc;

    /*
     * Window specific attributes
     */
    char *pathName;		/* Name of child window to be displayed */
    Tk_Window child;		/* Window to display */
    int reqWidth, reqHeight;	/* Requested window extents */
    int width, height;		/* Actual window extents */
    Tk_Anchor anchor;		/* Anchor */
    int x, y;

} WindowMarker;

static Tk_ConfigSpec windowConfigSpecs[] =
{
    {TK_CONFIG_ANCHOR, "-anchor", "winMarkerAnchor", "MarkerAnchor",
	DEF_MARKER_ANCHOR, Tk_Offset(WindowMarker, anchor), 0},
    {TK_CONFIG_CUSTOM, "-coords", "winMarkerCoords", "MarkerCoords",
	DEF_MARKER_COORDS, Tk_Offset(WindowMarker, coordArr),
	TK_CONFIG_NULL_OK, &coordsOption},
    {TK_CONFIG_UID, "-element", "winMarkerElement", "MarkerElement",
	DEF_MARKER_ELEMENT, Tk_Offset(WindowMarker, elemId), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-height", "winMarkerHeight", "MarkerHeight",
	DEF_MARKER_HEIGHT, Tk_Offset(WindowMarker, reqHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-mapped", "winMarkerMapped", "MarkerMapped",
	DEF_MARKER_MAPPED, Tk_Offset(WindowMarker, mapped),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "winMarkerMapX", "MarkerMapX",
	DEF_MARKER_MAP_X, Tk_Offset(WindowMarker, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "winMarkerMapY", "MarkerMapY",
	DEF_MARKER_MAP_Y, Tk_Offset(WindowMarker, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapYAxisOption},
    {TK_CONFIG_UID, "-name", (char *)NULL, (char *)NULL,
	DEF_MARKER_NAME, Tk_Offset(WindowMarker, nameId), 0},
    {TK_CONFIG_BOOLEAN, "-under", "winMarkerUnder", "MarkerUnder",
	DEF_MARKER_UNDER, Tk_Offset(WindowMarker, drawUnder),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-width", "winMarkerWidth", "MarkerWidth",
	DEF_MARKER_WIDTH, Tk_Offset(WindowMarker, reqWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_STRING, "-window", "winMarkerWindow", "MarkerWindow",
	DEF_MARKER_WINDOW, Tk_Offset(WindowMarker, pathName), 0},
    {TK_CONFIG_PIXELS, "-xoffset", "winMarkerXOffset", "MarkerXOffset",
	DEF_MARKER_X_OFFSET, Tk_Offset(WindowMarker, xOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-yoffset", "winMarkerYOffset", "MarkerYOffset",
	DEF_MARKER_Y_OFFSET, Tk_Offset(WindowMarker, yOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

#endif /* NO_WINDOW_MARKERS */

/*
 * -------------------------------------------------------------------
 *
 * BitmapMarker --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    Graph *graphPtr;		/* Graph marker belongs to */
    MarkerType type;		/* Type of marker */
    int flags;
    Tk_Uid nameId;		/* Identifier for marker */
    Coordinate *coordArr;	/* Coordinate array to position marker */
    int numCoords;		/* Number of points */
    Tk_ConfigSpec *configSpecs;	/* Configuration specifications */
    Tk_Uid elemId;		/* Element associated with marker */
    unsigned int axisMask;	/* Indicates which axes to element's map
				 * coordinates onto */
    int drawUnder;		/* If non-zero, draw the marker underneath
				 * any elements. There can be a performance
				 * because the graph must be redraw entirely
				 * each time this marker is redrawn. */
    int mapped;			/* Indicates if the marker is currently
				 * mapped or not. */
    int clipped;		/* Indicates if the marker is totally clipped 
				 * by the plotting area. */
    int xOffset, yOffset;	/* Pixel offset from anchor */

    MarkerDrawProc *drawProc;
    MarkerFreeProc *freeProc;
    MarkerConfigProc *configProc;
    MarkerCoordsProc *coordsProc;
    MarkerPrintProc *printProc;

    /*
     * Bitmap specific attributes
     */
    Pixmap bitmap;		/* Bitmap to be displayed */
    double rotate;		/* Requested rotation of the bitmap */
    double theta;		/* Normalized rotation (0..360 degrees) */
    XColor *normalFg;		/* foreground color */
    XColor *normalBg;		/* background color */
    Tk_Anchor anchor;		/* anchor */

    GC gc;			/* Private graphic context */
    GC fillGC;			/* Shared graphic context */
    int x, y;			/* Window x,y position of the bitmap */
    Pixmap rotBitmap;		/* Rotated bitmap */
    int width, height;		/* Extents of rotated bitmap */

} BitmapMarker;

static Tk_ConfigSpec bitmapConfigSpecs[] =
{
    {TK_CONFIG_ANCHOR, "-anchor", "bmMarkerAnchor", "MarkerAnchor",
	DEF_MARKER_ANCHOR, Tk_Offset(BitmapMarker, anchor),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_COLOR, "-background", "bmMarkerBackground", "MarkerBackground",
	DEF_MARKER_BG_COLOR, Tk_Offset(BitmapMarker, normalBg),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-background", "bmMarkerBackground", "MarkerBackground",
	DEF_MARKER_BG_MONO, Tk_Offset(BitmapMarker, normalBg),
	TK_CONFIG_MONO_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-bg", "bmMarkerBackground", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_BITMAP, "-bitmap", "bmMarkerBitmap", "MarkerBitmap",
	DEF_MARKER_BITMAP, Tk_Offset(BitmapMarker, bitmap), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-coords", "bmMarkerCoords", "MarkerCoords",
	DEF_MARKER_COORDS, Tk_Offset(BitmapMarker, coordArr),
	TK_CONFIG_NULL_OK, &coordsOption},
    {TK_CONFIG_UID, "-element", "bmMarkerElement", "MarkerElement",
	DEF_MARKER_ELEMENT, Tk_Offset(BitmapMarker, elemId),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-fg", "bmMarkerForeground", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_COLOR, "-foreground", "bmMarkerForeground", "MarkerForeground",
	DEF_MARKER_FG_COLOR, Tk_Offset(BitmapMarker, normalFg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "bmMarkerForeground", "MarkerForeground",
	DEF_MARKER_FG_MONO, Tk_Offset(BitmapMarker, normalFg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BOOLEAN, "-mapped", "bitmapMarkerMapped", "MarkerMapped",
	DEF_MARKER_MAPPED, Tk_Offset(BitmapMarker, mapped),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "bmMarkerMapX", "MarkerMapX",
	DEF_MARKER_MAP_X, Tk_Offset(BitmapMarker, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "bmMarkerMapY", "MarkerMapY",
	DEF_MARKER_MAP_Y, Tk_Offset(BitmapMarker, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapYAxisOption},
    {TK_CONFIG_UID, "-name", (char *)NULL, (char *)NULL,
	DEF_MARKER_NAME, Tk_Offset(BitmapMarker, nameId), 0},
    {TK_CONFIG_DOUBLE, "-rotate", "bmMarkerRotate", "MarkerRotate",
	DEF_MARKER_ROTATE, Tk_Offset(BitmapMarker, rotate),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-under", "bmMarkerUnder", "MarkerUnder",
	DEF_MARKER_UNDER, Tk_Offset(BitmapMarker, drawUnder),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-xoffset", "bmMarkerXOffset", "MarkerXOffset",
	DEF_MARKER_X_OFFSET, Tk_Offset(BitmapMarker, xOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-yoffset", "bmMarkerYOffset", "MarkerYOffset",
	DEF_MARKER_Y_OFFSET, Tk_Offset(BitmapMarker, yOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/*
 * -------------------------------------------------------------------
 *
 * LineMarker --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    Graph *graphPtr;		/* Graph marker belongs to */
    MarkerType type;		/* Type is MARKER_TYPE_LINE */
    int flags;
    Tk_Uid nameId;		/* Identifier for marker */
    Coordinate *coordArr;	/* Coordinate array to position marker */
    int numCoords;		/* Number of points */
    Tk_ConfigSpec *configSpecs;	/* Configuration specifications */
    Tk_Uid elemId;		/* Element associated with marker */
    unsigned int axisMask;	/* Indicates which axes to element's map
				 * coordinates onto */
    int drawUnder;		/* If non-zero, draw the marker underneath
				 * any elements. There can be a performance
				 * because the graph must be redraw entirely
				 * each time this marker is redrawn. */
    int mapped;			/* Indicates if the marker is currently
				 * mapped or not. */
    int clipped;		/* Indicates if the marker is totally clipped 
				 * by the plotting area. */
    int xOffset, yOffset;	/* Pixel offset */

    MarkerDrawProc *drawProc;
    MarkerFreeProc *freeProc;
    MarkerConfigProc *configProc;
    MarkerCoordsProc *coordsProc;
    MarkerPrintProc *printProc;

    /*
     * Line specific attributes
     */
    XColor *normalFg;		/* Foreground */
    XColor *normalBg;		/* Background color */
    int lineWidth;		/* line width */
    Dashes dashes;		/* Dash list values (max 11) */
#ifdef notdef
    int arrow;			/* Indicates whether or not to draw
				 * arrowheads: "none", "first", "last", or
				 * "both". */
    float arrowShapeA;		/* Distance from tip of arrowhead to center. */
    float arrowShapeB;		/* Distance from tip of arrowhead to trailing
				 * point, measured along shaft. */
    float arrowShapeC;		/* Distance of trailing points from outside
				 * edge of shaft. */
    double *firstArrowPtr;	/* Points to array of 5 points describing
				 * polygon for arrowhead at first point in
				 * line.  First point of arrowhead is tip.
				 * Malloc'ed.  NULL means no arrowhead at
				 * first point. */
    double *lastArrowPtr;	/* Points to polygon for arrowhead at last
				 * point in line (5 points, first of which is
				 * tip).  Malloc'ed.  NULL means no arrowhead
				 * at last point. */
#endif

    GC gc;			/* Private graphic context */
    XPoint *pointArr;
    int numPoints;
} LineMarker;

static Tk_ConfigSpec lineConfigSpecs[] =
{
#ifdef notdef
    {TK_CONFIG_STRING, "-arrow", "lineMarkerArrow", "MarkerArrow",
	"none", Tk_Offset(LineMarker, arrow), TK_CONFIG_DONT_SET_DEFAULT},
#endif
    {TK_CONFIG_COLOR, "-background", "lineMarkerBackground", "MarkerBackground",
	DEF_MARKER_BG_COLOR, Tk_Offset(LineMarker, normalBg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-background", "lineMarkerBackground", "MarkerBackground",
	DEF_MARKER_BG_MONO, Tk_Offset(LineMarker, normalBg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bg", "lineMarkerBackground", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_CUSTOM, "-coords", "lineMarkerCoords", "MarkerCoords",
	DEF_MARKER_COORDS, Tk_Offset(LineMarker, coordArr),
	TK_CONFIG_NULL_OK, &coordsOption},
    {TK_CONFIG_CUSTOM, "-dashes", "lineMarkerDashes", "MarkerDashes",
	DEF_MARKER_DASHES, Tk_Offset(LineMarker, dashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_UID, "-element", "lineMarkerElement", "MarkerElement",
	DEF_MARKER_ELEMENT, Tk_Offset(LineMarker, elemId), TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-fg", "lineMarkerForeground", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_COLOR, "-foreground", "lineMarkerForeground", "MarkerForeground",
	DEF_MARKER_FG_COLOR, Tk_Offset(LineMarker, normalFg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "lineMarkerForeground", "MarkerForeground",
	DEF_MARKER_FG_MONO, Tk_Offset(LineMarker, normalFg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-linewidth", "lineMarkerLineWidth", "MarkerLineWidth",
	DEF_MARKER_LINE_WIDTH, Tk_Offset(LineMarker, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-mapped", "lineMarkerMapped", "MarkerMapped",
	DEF_MARKER_MAPPED, Tk_Offset(LineMarker, mapped),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "lineMarkerMapX", "MarkerMapX",
	DEF_MARKER_MAP_X, Tk_Offset(LineMarker, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "lineMarkerMapY", "MarkerMapY",
	DEF_MARKER_MAP_Y, Tk_Offset(LineMarker, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapYAxisOption},
    {TK_CONFIG_UID, "-name", (char *)NULL, (char *)NULL,
	DEF_MARKER_NAME, Tk_Offset(LineMarker, nameId), 0},
    {TK_CONFIG_BOOLEAN, "-under", "lineMarkerUnder", "MarkerUnder",
	DEF_MARKER_UNDER, Tk_Offset(LineMarker, drawUnder),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-xoffset", "lineMarkerXOffset", "MarkerXOffset",
	DEF_MARKER_X_OFFSET, Tk_Offset(LineMarker, xOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-yoffset", "lineMarkerYOffset", "MarkerYOffset",
	DEF_MARKER_Y_OFFSET, Tk_Offset(LineMarker, yOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/*
 * -------------------------------------------------------------------
 *
 * PolygonMarker --
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    Graph *graphPtr;		/* Graph marker belongs to */
    MarkerType type;		/* Type of marker */
    int flags;
    Tk_Uid nameId;		/* Identifier for marker */
    Coordinate *coordArr;	/* Coordinate array to position marker */
    int numCoords;		/* Number of points */
    Tk_ConfigSpec *configSpecs;	/* Configuration specifications */
    Tk_Uid elemId;		/* Element associated with marker */
    unsigned int axisMask;	/* Indicates which axis to element's map
				 * y-coordinates onto */
    int drawUnder;		/* If non-zero, draw the marker underneath
				 * any elements. There can be a performance
				 * because the graph must be redraw entirely
				 * each time this marker is redrawn. */
    int mapped;			/* Indicates if the marker is currently
				 * mapped or not. */
    int clipped;		/* Indicates if the marker is totally clipped 
				 * by the plotting area. */
    int xOffset, yOffset;	/* Pixel offset */

    MarkerDrawProc *drawProc;
    MarkerFreeProc *freeProc;
    MarkerConfigProc *configProc;
    MarkerCoordsProc *coordsProc;
    MarkerPrintProc *printProc;

    /*
     * Polygon specific attributes and fields
     */
    XColor *outlineColor;	/* foreground */
    XColor *fillColor;		/* background */
    Pixmap stipple;		/* stipple pattern */
    int lineWidth;		/* line width */
    Dashes dashes;		/* dash list value */

    GC outlineGC;		/* Private graphic context */
    GC fillGC;			/* Private graphic context */

    XPoint *pointArr;		/* Points needed to draw polygon */
    int numPoints;		/* Number of points in above array */

} PolygonMarker;

static Tk_ConfigSpec polygonConfigSpecs[] =
{
    {TK_CONFIG_CUSTOM, "-coords", "polyMarkerCoords", "MarkerCoords",
	DEF_MARKER_COORDS, Tk_Offset(PolygonMarker, coordArr),
	TK_CONFIG_NULL_OK, &coordsOption},
    {TK_CONFIG_CUSTOM, "-dashes", "polyMarkerDashes", "MarkerDashes",
	DEF_MARKER_DASHES, Tk_Offset(PolygonMarker, dashes),
	TK_CONFIG_NULL_OK, &bltDashesOption},
    {TK_CONFIG_UID, "-element", "polyMarkerElement", "MarkerElement",
	DEF_MARKER_ELEMENT, Tk_Offset(PolygonMarker, elemId),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-fill", "polyMarkerFill", "MarkerFill",
	DEF_MARKER_FILL_COLOR, Tk_Offset(PolygonMarker, fillColor),
	TK_CONFIG_COLOR_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-fill", "polyMarkerFill", "MarkerFill",
	DEF_MARKER_FILL_MONO, Tk_Offset(PolygonMarker, fillColor),
	TK_CONFIG_MONO_ONLY | TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-linewidth", "polyMarkerLineWidth", "MarkerLineWidth",
	DEF_MARKER_LINE_WIDTH, Tk_Offset(PolygonMarker, lineWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-mapped", "polyMarkerMapped", "MarkerMapped",
	DEF_MARKER_MAPPED, Tk_Offset(PolygonMarker, mapped),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-mapx", "polyMarkerMapX", "MarkerMapX",
	DEF_MARKER_MAP_X, Tk_Offset(PolygonMarker, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapXAxisOption},
    {TK_CONFIG_CUSTOM, "-mapy", "polyMarkerMapY", "MarkerMapY",
	DEF_MARKER_MAP_Y, Tk_Offset(PolygonMarker, axisMask),
	TK_CONFIG_DONT_SET_DEFAULT, &bltMapYAxisOption},
    {TK_CONFIG_UID, "-name", (char *)NULL, (char *)NULL,
	DEF_MARKER_NAME, Tk_Offset(PolygonMarker, nameId), 0},
    {TK_CONFIG_COLOR, "-outline", "polyMarkerOutline", "MarkerOutline",
	DEF_MARKER_OUTLINE_COLOR, Tk_Offset(PolygonMarker, outlineColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-outline", "polyMarkerOutline", "MarkerOutline",
	DEF_MARKER_OUTLINE_MONO, Tk_Offset(PolygonMarker, outlineColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BITMAP, "-stipple", "polyMarkerStipple", "MarkerStipple",
	DEF_MARKER_STIPPLE, Tk_Offset(PolygonMarker, stipple),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-under", "polyMarkerUnder", "MarkerUnder",
	DEF_MARKER_UNDER, Tk_Offset(PolygonMarker, drawUnder),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-xoffset", "polyMarkerXOffset", "MarkerXOffset",
	DEF_MARKER_X_OFFSET, Tk_Offset(PolygonMarker, xOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-yoffset", "polyMarkerYOffset", "MarkerYOffset",
	DEF_MARKER_Y_OFFSET, Tk_Offset(PolygonMarker, yOffset),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};


INLINE static void
BoundPoint(pointPtr)
    XPoint *pointPtr;
{
    /* Should really figure out a good offset value and test for that because
     * we could still generate bogus numbers */
    if (pointPtr->x >= SHRT_MAX) {
	pointPtr->x = SHRT_MAX - 1000;
    } else if (pointPtr->x <= SHRT_MIN) {
	pointPtr->x = SHRT_MIN + 1000;
    }
    if (pointPtr->y >= SHRT_MAX) {
	pointPtr->y = SHRT_MAX - 1000;
    } else if (pointPtr->y <= SHRT_MIN) {
	pointPtr->y = SHRT_MIN + 1000;
    }
}

INLINE static int
Inside(regionPtr, x, y)
    BBox *regionPtr;
    double x, y;
{
    if ((x > (double)regionPtr->urx) || (x < (double)regionPtr->llx)) {
	return 0;
    }
    if ((y > (double)regionPtr->lly) || (y < (double)regionPtr->ury)) {
	return 0;
    }
    return 1;
}

/*
 * ----------------------------------------------------------------------
 *
 * TestMarkerBBox --
 *
 *	Tests if the bounding box of a marker overlaps the plotting
 *	area in any way.  If so, the marker will be drawn.
 *
 *	Check if the rectangles overlay at all. Simply test the
 *	corners one rectangle against the area of the other.  If any
 *	point of one rectangle is interior to the other rectangle,
 *	then the rectangles overlap.  
 *
 * Results:
 *	Returns 0 is the marker is visible in the plotting area, and
 *	1 otherwise (marker is clipped).
 *
 * ----------------------------------------------------------------------
 */
static int 
TestMarkerBBox(graphPtr, regionPtr)
    Graph *graphPtr;
    BBox *regionPtr;
{
    BBox clipBBox;

    /* There are two rectangles: one is the plotting area, the other
     * is the bounding box of the marker. */

    clipBBox.urx = graphPtr->urx;
    clipBBox.ury = graphPtr->ury;
    clipBBox.llx = graphPtr->llx;
    clipBBox.lly = graphPtr->lly;

    if ((Inside(&clipBBox, (double)regionPtr->llx, (double)regionPtr->lly)) ||
	(Inside(&clipBBox, (double)regionPtr->urx, (double)regionPtr->ury)) ||
	(Inside(&clipBBox, (double)regionPtr->llx, (double)regionPtr->ury)) ||
	(Inside(&clipBBox, (double)regionPtr->urx, (double)regionPtr->lly)) ||
	(Inside(regionPtr, (double)clipBBox.llx, (double)clipBBox.lly)) ||
	(Inside(regionPtr, (double)clipBBox.urx, (double)clipBBox.ury)) ||
	(Inside(regionPtr, (double)clipBBox.llx, (double)clipBBox.ury)) ||
	(Inside(regionPtr, (double)clipBBox.urx, (double)clipBBox.lly))) {
	return 0;
    }
    return 1;
}


/*
 * ----------------------------------------------------------------------
 *
 * PrintCoordinate --
 *
 * 	Convert the double precision value into its string
 * 	representation.  The only reason this routine is used in
 * 	instead of sprintf, is to handle the "elastic" bounds.  That
 * 	is, convert the values DBL_MAX and -(DBL_MAX) into
 * 	"+Inf" and "-Inf" respectively.
 *
 * Results:
 *	The return value is a standard Tcl result.  The string of the
 * 	expression is passed back via string.
 *
 * ----------------------------------------------------------------------
 */
static char *
PrintCoordinate(interp, x)
    Tcl_Interp *interp;
    double x;			/* Numeric value */
{
    if (x == bltPosInfinity) {
	return "+Inf";
    } else if (x == bltNegInfinity) {
	return "-Inf";
    } else {
	static char string[TCL_DOUBLE_SPACE + 1];

	Tcl_PrintDouble(interp, x, string);
	return string;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ParseCoordinates --
 *
 *	If no coordinates are specified (coordList is NULL), this
 *	routine returns the coordinates of a given marker. Otherwise,
 *	the Tcl coordinate list is converted to their floating point
 *	values. It will then replace the current marker coordinates.
 *
 *	Since different marker types require different number of
 *	coordinates this must be checked here.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side effects:
 *	If the marker coordinates are reset, the graph is eventually redrawn
 *	with at the new marker coordinates.
 *
 * ----------------------------------------------------------------------
 */

static int
ParseCoordinates(interp, markerPtr, numExprs, exprArr)
    Tcl_Interp *interp;
    Marker *markerPtr;
    int numExprs;
    char **exprArr;
{
    int numCoords;
    int minArgs, maxArgs;
    double x, y;
    Coordinate *newArr;
    register int i;
    register Coordinate *coordPtr;

    /* Split the list of coordinates and check the values */

    if (numExprs == 0) {
	return TCL_OK;
    }
    if (numExprs & 1) {
	Tcl_SetResult(interp, "odd number of marker coordinates specified",
	    TCL_STATIC);
	return TCL_ERROR;
    }
    switch (markerPtr->type) {
    case MARKER_TYPE_LINE:
	minArgs = 4, maxArgs = 0;
	break;
    case MARKER_TYPE_POLYGON:
	minArgs = 6, maxArgs = 0;
	break;
#ifndef NO_WINDOW_MARKERS
    case MARKER_TYPE_WINDOW:
#endif /* NO_WINDOW_MARKERS */
    case MARKER_TYPE_BITMAP:
    case MARKER_TYPE_TEXT:
	minArgs = 2, maxArgs = 2;
	break;
    default:
	Tcl_SetResult(interp, "unknown marker type", TCL_STATIC);
	return TCL_ERROR;
    }

    if (numExprs < minArgs) {
	Tcl_SetResult(interp, "too few marker coordinates specified",
	    TCL_STATIC);
	return TCL_ERROR;
    }
    if ((maxArgs > 0) && (numExprs > maxArgs)) {
	Tcl_SetResult(interp, "too many marker coordinates specified",
	    TCL_STATIC);
	return TCL_ERROR;
    }
    numCoords = numExprs / 2;
    newArr = (Coordinate *)ckalloc(numCoords * sizeof(Coordinate));
    if (newArr == NULL) {
	Tcl_SetResult(interp, "can't allocate new coordinate array",
	    TCL_STATIC);
	return TCL_ERROR;
    }
    /*
     * A new coordinate array is allocated each time so that we
     * can check the coordinates without overwriting the current
     * marker coordinates.
     */
    coordPtr = newArr;
    for (i = 0; i < numExprs; i+= 2) {
	if ((Blt_GetCoordinate(interp, exprArr[i], &x) != TCL_OK) ||
	    (Blt_GetCoordinate(interp, exprArr[i+1], &y) != TCL_OK)) {
	    ckfree((char *)newArr);
	    return TCL_ERROR;
	}
	coordPtr->x = x, coordPtr->y = y;
	coordPtr++;
    }
    if (markerPtr->coordArr != NULL) {
	ckfree((char *)markerPtr->coordArr);
    }
    markerPtr->coordArr = newArr;
    markerPtr->numCoords = numCoords;
    markerPtr->flags |= COORDS_NEEDED;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * CoordinatesParseProc --
 *
 *	Given a Tcl list of numeric expression representing the element
 *	values, convert into an array of double precision values. In
 *	addition, the minimum and maximum values are saved.  Since
 *	elastic values are allow (values which translate to the
 *	min/max of the graph), we must try to get the non-elastic
 *	minimum and maximum.
 *
 * Results:
 *	The return value is a standard Tcl result.  The vector is passed
 *	back via the vecPtr.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CoordinatesParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* Tcl list of numeric expressions */
    char *widgRec;		/* Marker record */
    int offset;			/* not used */
{
    Marker *markerPtr = (Marker *) widgRec;
    int numExprs;
    char **exprArr;
    int result;

    if ((value == NULL) || (*value == '\0')) {
noCoordinates:
	if (markerPtr->coordArr != NULL) {
	    ckfree((char *)markerPtr->coordArr);
	    markerPtr->coordArr = NULL;
	}
	markerPtr->numCoords = 0;
	return TCL_OK;
    }
    if (Tcl_SplitList(interp, value, &numExprs, &exprArr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (numExprs == 0) {
	goto noCoordinates;
    }
    result = ParseCoordinates(interp, markerPtr, numExprs, exprArr);
    ckfree((char *)exprArr);
    return (result);
}

/*
 * ----------------------------------------------------------------------
 *
 * CoordinatesPrintProc --
 *
 *	Convert the vector of floating point values into a Tcl list.
 *
 * Results:
 *	The string representation of the vector is returned.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
CoordinatesPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Marker record */
    int offset;			/* not used */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Marker *markerPtr = (Marker *) widgRec;
    Tcl_DString coordList;
    char *result;
    register int i;
    register Coordinate *coordPtr;

    if (markerPtr->numCoords < 1) {
	return "";
    }
    Tcl_DStringInit(&coordList);
    coordPtr = markerPtr->coordArr;
    for (i = 0; i < markerPtr->numCoords; i++) {
	Tcl_DStringAppendElement(&coordList,
	    PrintCoordinate(markerPtr->graphPtr->interp, coordPtr->x));
	Tcl_DStringAppendElement(&coordList,
	    PrintCoordinate(markerPtr->graphPtr->interp, coordPtr->y));
	coordPtr++;
    }
    result = ckstrdup(Tcl_DStringValue(&coordList));
    Tcl_DStringFree(&coordList);
    *freeProcPtr = (Tcl_FreeProc *)BLT_ckfree;
    return (result);
}

static void
DestroyMarker(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;

    /* Free the resources allocated for the particular type of marker */
    (*markerPtr->freeProc) (graphPtr, markerPtr);
    if (markerPtr->coordArr != NULL) {
	ckfree((char *)markerPtr->coordArr);
    }
    Tk_FreeOptions(markerPtr->configSpecs, (char *)markerPtr, 
	graphPtr->display, 0);
    ckfree((char *)markerPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureBitmap --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a bitmap marker.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as bitmap pixmap, colors, rotation,
 *	etc. get set for markerPtr;  old resources get freed, if there
 *	were any.  The marker is eventually redisplayed.
 *
 * ----------------------------------------------------------------------
 */
 /* ARGSUSED */
static int
ConfigureBitmap(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    BitmapMarker *bitmapPtr = (BitmapMarker *) markerPtr;

    if (bitmapPtr->bitmap != None) {
	GC newGC;
	XGCValues gcValues;
	unsigned long gcMask;

	bitmapPtr->theta = BLT_FMOD(bitmapPtr->rotate, 360.0);
	if (bitmapPtr->theta < 0.0) {
	    bitmapPtr->theta += 360.0;
	}
	gcValues.foreground = bitmapPtr->normalFg->pixel;
	gcMask = GCForeground | GCFillStyle;
	if (bitmapPtr->normalBg != NULL) {
	    gcValues.background = bitmapPtr->normalBg->pixel;
	    gcValues.fill_style = FillSolid;
	    gcMask |= GCBackground;
	} else {
	    gcValues.stipple = bitmapPtr->bitmap;
	    gcValues.fill_style = FillStippled;
	    gcMask |= GCStipple;
	}

	/*
	 * Note that while this is a shared GC, we're going to change
	 * GCTileStipXOrigin and GCTileStipYOrigin when the bitmap
	 * is displayed anyways.  I'm assuming that any code using this
	 * GC (with the GCStipple set) is going to set the stipple
	 * origin before it draws anyway.
	 */
	newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
	if (bitmapPtr->gc != NULL) {
	    Tk_FreeGC(graphPtr->display, bitmapPtr->gc);
	}
	bitmapPtr->gc = newGC;
	/* Create background GC color */
	if (bitmapPtr->normalBg != NULL) {
	    gcValues.foreground = bitmapPtr->normalBg->pixel;
	    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
	    if (bitmapPtr->fillGC != NULL) {
		Tk_FreeGC(graphPtr->display, bitmapPtr->fillGC);
	    }
	    bitmapPtr->fillGC = newGC;
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputeBitmapCoordinates --
 *
 * 	This procedure gets called each time the layout of the graph
 *	changes.  The x, y window coordinates of the bitmap marker are
 *	saved in the marker structure.
 *
 *	Additionly, if no background color was specified, the
 *	GCTileStipXOrigin and GCTileStipYOrigin attributes are set in
 *	the private GC.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Window coordinates are saved and if no background color was
 * 	set, the GC stipple origins are changed to calculated window
 *	coordinates.
 *
 * ----------------------------------------------------------------------
 */
static void
ComputeBitmapCoordinates(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    BitmapMarker *bitmapPtr = (BitmapMarker *) markerPtr;
    int width, height;
    AxisPair axisPair;
    Coordinate bmPos, coord;
    BBox bbox;

    if (bitmapPtr->bitmap == None) {
	return;
    }
    Blt_GetAxisMapping(graphPtr, bitmapPtr->axisMask, &axisPair);
    Tk_SizeOfBitmap(graphPtr->display, bitmapPtr->bitmap, &width, &height);
    coord = Blt_TransformPt(graphPtr, bitmapPtr->coordArr[0].x,
	bitmapPtr->coordArr[0].y, &axisPair);
    coord.x += (double)bitmapPtr->xOffset;
    coord.y += (double)bitmapPtr->yOffset;

    bmPos = Blt_TranslateBoxCoords(coord.x, coord.y, width, height, 
	bitmapPtr->anchor);

    if ((bitmapPtr->rotBitmap != None) &&
	(bitmapPtr->rotBitmap != bitmapPtr->bitmap)) {
	Tk_FreePixmap(graphPtr->display, bitmapPtr->rotBitmap);
	bitmapPtr->rotBitmap = None;
    }
    if (bitmapPtr->theta == 0.0) {
	bitmapPtr->width = width;
	bitmapPtr->height = height;
	bitmapPtr->rotBitmap = bitmapPtr->bitmap;
    } else {
	GC bitmapGC;
	unsigned long gcMask;
	XGCValues gcValues;

	gcValues.foreground = 1, gcValues.background = 0;
	gcMask = (GCForeground | GCBackground);
	bitmapGC = XCreateGC(graphPtr->display, bitmapPtr->bitmap, gcMask,
	    &gcValues);
	bitmapPtr->rotBitmap = Blt_RotateBitmap(graphPtr->display,
	    Tk_WindowId(graphPtr->tkwin), bitmapGC, bitmapPtr->bitmap,
	    width, height, bitmapPtr->theta, &(bitmapPtr->width),
	    &(bitmapPtr->height));
	XFreeGC(graphPtr->display, bitmapGC);
	bmPos = Blt_TranslateBoxCoords(coord.x, coord.y, bitmapPtr->width, 
		bitmapPtr->height, bitmapPtr->anchor);
    }
    /* 
     * Determine the bounding box of the bitmap and test to see if it
     * is at least partially contained within the plotting area.
     */
    bbox.llx = bmPos.x;
    bbox.lly = bmPos.y + bitmapPtr->height;
    bbox.urx = bmPos.x + bitmapPtr->width;
    bbox.ury = bmPos.y;
    bitmapPtr->clipped = TestMarkerBBox(graphPtr, &bbox);

    bitmapPtr->x = BLT_RND(bmPos.x), bitmapPtr->y = BLT_RND(bmPos.y);
    if (bitmapPtr->normalBg == NULL) {
	XSetTSOrigin(graphPtr->display, bitmapPtr->gc, bitmapPtr->x,
	    bitmapPtr->y);
	XSetStipple(graphPtr->display, bitmapPtr->gc, bitmapPtr->rotBitmap);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawBitmap --
 *
 *	This procedure is invoked to draw a bitmap marker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	GC stipple origins are changed to current window coordinates.
 *	Commands are output to X to draw the marker in its current mode.
 *
 * ----------------------------------------------------------------------
 */
static void
DrawBitmap(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    BitmapMarker *bitmapPtr = (BitmapMarker *) markerPtr;

    if (bitmapPtr->bitmap == None) {
	return;
    }
    if (bitmapPtr->normalBg != NULL) {
	if (BLT_FMOD(bitmapPtr->theta, 90.0) == 0.0) {
	    /*
	     * Right angle rotations: Bounding box matches background area
	     */
	    XCopyPlane(graphPtr->display, bitmapPtr->rotBitmap,
		graphPtr->pixwin, bitmapPtr->gc, 0, 0, bitmapPtr->width,
		bitmapPtr->height, bitmapPtr->x, bitmapPtr->y, 1);
	    return;
	} else {
	    XPoint pointArr[4];
	    register int i;
	    int width, height;

	    Tk_SizeOfBitmap(graphPtr->display, bitmapPtr->bitmap, &width,
		&height);
	    Blt_GetBoundingBox(width, height, bitmapPtr->theta, &width, &height,
		pointArr);
	    for (i = 0; i < 4; i++) {
		pointArr[i].x += bitmapPtr->x + (bitmapPtr->width / 2);
		pointArr[i].y += bitmapPtr->y + (bitmapPtr->height / 2);
	    }
	    XFillPolygon(graphPtr->display, graphPtr->pixwin,
		bitmapPtr->fillGC, pointArr, 4, Convex, CoordModeOrigin);
	}
    }
    Blt_StencilBitmap(graphPtr->display, graphPtr->pixwin, bitmapPtr->gc,
	bitmapPtr->rotBitmap, bitmapPtr->x, bitmapPtr->y, bitmapPtr->width,
	bitmapPtr->height);
}


static void
PrintBitmap(markerPtr)
    Marker *markerPtr;		/* Marker to be printed */
{
    Graph *graphPtr = markerPtr->graphPtr;
    BitmapMarker *bitmapPtr = (BitmapMarker *) markerPtr;

    if (bitmapPtr->bitmap != None) {
	int width, height;
	int centerX, centerY;

	Tk_SizeOfBitmap(graphPtr->display, bitmapPtr->bitmap, &width, &height);

	/* Find the center of the bounding box */
	centerX = bitmapPtr->x + (bitmapPtr->width / 2);
	centerY = bitmapPtr->y + (bitmapPtr->height / 2);

	Blt_ForegroundToPostScript(graphPtr, bitmapPtr->normalFg);
	Blt_BitmapToPostScript(graphPtr, bitmapPtr->bitmap, centerX, centerY,
	    width, height, bitmapPtr->theta, bitmapPtr->normalBg);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyBitmap --
 *
 *	Destroys the structure containing the attributes of the bitmap
 * 	marker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Bitmap attributes (GCs, colors, bitmap, etc) get destroyed.
 *	Memory is released, X resources are freed, and the graph is
 *	redrawn.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyBitmap(graphPtr, markerPtr)
    Graph *graphPtr;
    Marker *markerPtr;
{
    BitmapMarker *bitmapPtr = (BitmapMarker *) markerPtr;

    if (bitmapPtr->gc != NULL) {
	Tk_FreeGC(graphPtr->display, bitmapPtr->gc);
    }
    if (bitmapPtr->fillGC != NULL) {
	Tk_FreeGC(graphPtr->display, bitmapPtr->fillGC);
    }
    if ((bitmapPtr->rotBitmap != None) &&
	(bitmapPtr->rotBitmap != bitmapPtr->bitmap)) {
	Tk_FreePixmap(graphPtr->display, bitmapPtr->rotBitmap);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * bltCreateBitmap --
 *
 *	Allocate memory and initialize methods for the new bitmap marker.
 *
 * Results:
 *	The pointer to the newly allocated marker structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the bitmap marker structure.
 *
 * ----------------------------------------------------------------------
 */
static Marker *
bltCreateBitmap()
{
    BitmapMarker *bitmapPtr;

    bitmapPtr = (BitmapMarker *) ckcalloc(1, sizeof(BitmapMarker));
    if (bitmapPtr != NULL) {
	bitmapPtr->configSpecs = bitmapConfigSpecs;
	bitmapPtr->configProc = (MarkerConfigProc*)ConfigureBitmap;
	bitmapPtr->freeProc = (MarkerFreeProc*)DestroyBitmap;
	bitmapPtr->drawProc = (MarkerDrawProc*)DrawBitmap;
	bitmapPtr->coordsProc = (MarkerCoordsProc*)ComputeBitmapCoordinates;
	bitmapPtr->printProc = (MarkerPrintProc*)PrintBitmap;
	bitmapPtr->type = MARKER_TYPE_BITMAP;
    }
    return ((Marker *) bitmapPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureText --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a text marker.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for markerPtr;  old resources get freed, if there
 *	were any.  The marker is eventually redisplayed.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigureText(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    TextMarker *textPtr = (TextMarker *) markerPtr;
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;

    textPtr->attr.theta = BLT_FMOD(textPtr->rotate, 360.0);
    if (textPtr->attr.theta < 0.0) {
	textPtr->attr.theta += 360.0;
    }
    gcValues.foreground = textPtr->attr.fgColorPtr->pixel;
    gcValues.font = textPtr->attr.fontPtr->fid;
    gcMask = (GCFont | GCForeground);
    if (textPtr->attr.bgColorPtr != NULL) {
	gcValues.background = textPtr->attr.bgColorPtr->pixel;
	gcMask |= GCBackground;
    }
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (textPtr->attr.textGC != NULL) {
	Tk_FreeGC(graphPtr->display, textPtr->attr.textGC);
    }
    textPtr->attr.textGC = newGC;
    if (textPtr->attr.fillGC != NULL) {
	Tk_FreeGC(graphPtr->display, textPtr->attr.fillGC);
	textPtr->attr.fillGC = NULL;
    }
    if (textPtr->attr.bgColorPtr != NULL) {
	gcValues.foreground = textPtr->attr.bgColorPtr->pixel;
	newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
	textPtr->attr.fillGC = newGC;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputeTextCoordinates --
 *
 *	Calculate the layout position for a text marker.  Positional
 *	information is saved in the marker.  If the text is rotated,
 *	a bitmap containing the text is created.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If no background color has been specified, the GC stipple
 *	origins are changed to current window coordinates. For both
 *	rotated and non-rotated text, if any old bitmap is leftover,
 *	it is freed.
 *
 * ----------------------------------------------------------------------
 */
static void
ComputeTextCoordinates(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    TextMarker *textPtr = (TextMarker *) markerPtr;
    Coordinate coord;		/* Upper left corner of text region */
    AxisPair axisPair;
    BBox bbox;
    int width, height;

    if (textPtr->text == NULL) {
	return;
    }
    Blt_GetAxisMapping(graphPtr, textPtr->axisMask, &axisPair);
    coord = Blt_TransformPt(graphPtr, textPtr->coordArr[0].x,
	textPtr->coordArr[0].y, &axisPair);
    coord.x += (double)textPtr->xOffset;
    coord.y += (double)textPtr->yOffset;
    textPtr->x = BLT_RND(coord.x), textPtr->y = BLT_RND(coord.y);

    /* 
     * Determine the bounding box of the text and test to see if it
     * is at least partially contained within the plotting area.
     */
    Blt_GetTextExtents(textPtr->attr.fontPtr, textPtr->text, &width, &height);
    Blt_GetBoundingBox(width, height, textPtr->attr.theta, &width, &height, 
		       (XPoint *)NULL);
    bbox.llx = textPtr->x;
    bbox.lly = textPtr->y + height;
    bbox.urx = textPtr->x + width;
    bbox.ury = textPtr->y;
    textPtr->clipped = TestMarkerBBox(graphPtr, &bbox);

}

/*
 * ----------------------------------------------------------------------
 *
 * bltDrawText --
 *
 *	Draw the text marker on the graph given. If the text is not
 *	rotated, simply use the X text drawing routines. However, if
 *	the text has been rotated, stencil the bitmap representing
 *	the text. Since stencilling is very expensive, we try to
 *	draw right angle rotations with XCopyArea.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to draw the marker in its current mode.
 *
 * ----------------------------------------------------------------------
 */
static void
bltDrawText(markerPtr)
    Marker *markerPtr;
{
    TextMarker *textPtr = (TextMarker *) markerPtr;

    if (textPtr->text != NULL) {
	Graph *graphPtr = markerPtr->graphPtr;
	
	Blt_DrawText(graphPtr->tkwin, graphPtr->pixwin, textPtr->text,
		     &(textPtr->attr), textPtr->x, textPtr->y);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * PrintText --
 *
 *	Outputs PostScript commands to draw a text marker at a given
 *	x,y coordinate, rotation, anchor, and font.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PostScript font and color settings are changed.
 *
 * ----------------------------------------------------------------------
 */
static void
PrintText(markerPtr)
    Marker *markerPtr;
{
    TextMarker *textPtr = (TextMarker *) markerPtr;

    if (textPtr->text != NULL) {
	Blt_PrintText(markerPtr->graphPtr, textPtr->text, &(textPtr->attr), 
		textPtr->x, textPtr->y);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyText --
 *
 *	Destroys the structure containing the attributes of the text
 * 	marker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Text attributes (GCs, colors, stipple, font, etc) get destroyed.
 *	Memory is released, X resources are freed, and the graph is
 *	redrawn.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyText(graphPtr, markerPtr)
    Graph *graphPtr;
    Marker *markerPtr;
{
    TextMarker *textPtr = (TextMarker *) markerPtr;

    if (textPtr->attr.textGC != NULL) {
	Tk_FreeGC(graphPtr->display, textPtr->attr.textGC);
    }
    if (textPtr->attr.fillGC != NULL) {
	Tk_FreeGC(graphPtr->display, textPtr->attr.fillGC);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * bltCreateText --
 *
 *	Allocate memory and initialize methods for the new text marker.
 *
 * Results:
 *	The pointer to the newly allocated marker structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the text marker structure.
 *
 * ----------------------------------------------------------------------
 */
static Marker *
bltCreateText()
{
    TextMarker *textPtr;

    textPtr = (TextMarker *) ckcalloc(1, sizeof(TextMarker));
    if (textPtr != NULL) {
	textPtr->configSpecs = textConfigSpecs;
	textPtr->configProc = (MarkerConfigProc*)ConfigureText;
	textPtr->freeProc = (MarkerFreeProc*)DestroyText;
	textPtr->drawProc = (MarkerDrawProc*)bltDrawText;
	textPtr->coordsProc = (MarkerCoordsProc*)ComputeTextCoordinates;
	textPtr->printProc = (MarkerPrintProc*)PrintText;
	textPtr->type = MARKER_TYPE_TEXT;
	textPtr->attr.justify = TK_JUSTIFY_CENTER;
	textPtr->attr.padLeft = textPtr->attr.padRight = 4;
	textPtr->attr.padTop = textPtr->attr.padBottom = 4;
    }
    return ((Marker *) textPtr);
}

#ifndef NO_WINDOW_MARKERS

static void ChildEventProc _ANSI_ARGS_((ClientData clientData,
	XEvent *eventPtr));
static void ChildGeometryProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));

static void ChildCustodyProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));

static Tk_GeomMgr winMarkerMgrInfo =
{
    "graph",			/* Name of geometry manager used by winfo */
    ChildGeometryProc,		/* Procedure to for new geometry requests */
    ChildCustodyProc,		/* Procedure when window is taken away */
};

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureWindow --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a window marker.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as window pathname, placement,
 *	etc. get set for markerPtr;  old resources get freed, if there
 *	were any.  The marker is eventually redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureWindow(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    WindowMarker *windowPtr = (WindowMarker *) markerPtr;
    Tk_Window tkwin;

    if (windowPtr->pathName == NULL) {
	return TCL_OK;
    }
    tkwin = Tk_NameToWindow(graphPtr->interp, windowPtr->pathName,
	graphPtr->tkwin);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    if (Tk_Parent(tkwin) != graphPtr->tkwin) {
	Tcl_AppendResult(graphPtr->interp, "\"", windowPtr->pathName,
	    "\" is not a child of ", Tk_PathName(graphPtr->tkwin),
	    (char *)NULL);
	return TCL_ERROR;
    }
    if (tkwin != windowPtr->child) {
	if (windowPtr->child != NULL) {
	    Tk_DeleteEventHandler(windowPtr->child, StructureNotifyMask,
		ChildEventProc, (ClientData)windowPtr);
	    Tk_ManageGeometry(windowPtr->child, (Tk_GeomMgr *) 0,(ClientData)0);
	    Tk_UnmapWindow(windowPtr->child);
	}
	Tk_CreateEventHandler(tkwin, StructureNotifyMask, ChildEventProc,
	    (ClientData)windowPtr);
	Tk_ManageGeometry(tkwin, &winMarkerMgrInfo, (ClientData)windowPtr);
    }
    Tk_MapWindow(tkwin);
    windowPtr->child = tkwin;
    windowPtr->width = Tk_ReqWidth(tkwin);
    windowPtr->height = Tk_ReqHeight(tkwin);
    if (windowPtr->reqWidth > 0) {
	windowPtr->width = windowPtr->reqWidth;
    }
    if (windowPtr->reqHeight > 0) {
	windowPtr->height = windowPtr->reqHeight;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputeWindowCoordinates --
 *
 *	Calculate the layout position for a window marker.  Positional
 *	information is saved in the marker.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
ComputeWindowCoordinates(markerPtr)
    Marker *markerPtr;
{
    WindowMarker *windowPtr = (WindowMarker *) markerPtr;
    Graph *graphPtr = markerPtr->graphPtr;
    Coordinate origin, coord;
    AxisPair axisPair;
    BBox bbox;

    if (windowPtr->child == (Tk_Window)NULL) {
	return;
    }
    Blt_GetAxisMapping(graphPtr, windowPtr->axisMask, &axisPair);
    coord = Blt_TransformPt(graphPtr, windowPtr->coordArr[0].x,
	    windowPtr->coordArr[0].y, &axisPair);
    origin = Blt_TranslateBoxCoords(coord.x, coord.y, windowPtr->width, 
	windowPtr->height, windowPtr->anchor);
    origin.x += windowPtr->xOffset;
    origin.y += windowPtr->yOffset;    
    /* 
     * Determine the bounding box of the window and test to see if it
     * is at least partially contained within the plotting area.
     */
    bbox.llx = origin.x;
    bbox.lly = origin.y + windowPtr->height;
    bbox.urx = origin.x + windowPtr->width;
    bbox.ury = origin.y;

    windowPtr->clipped = TestMarkerBBox(graphPtr, &bbox);
    windowPtr->x = BLT_RND(origin.x);
    windowPtr->y = BLT_RND(origin.y);
}

/*ARGSUSED*/
static void
DrawWindow(markerPtr)
    Marker *markerPtr;
{
    WindowMarker *windowPtr = (WindowMarker *) markerPtr;

    if (windowPtr->child == (Tk_Window)NULL) {
	return;
    }
    if ((windowPtr->height != Tk_Height(windowPtr->child)) ||
	(windowPtr->width != Tk_Width(windowPtr->child)) ||
	(windowPtr->x != Tk_X(windowPtr->child)) ||
	(windowPtr->y != Tk_Y(windowPtr->child))) {
	Tk_MoveResizeWindow(windowPtr->child, windowPtr->x, windowPtr->y,
			    windowPtr->width, windowPtr->height);
    }
    if (!Tk_IsMapped(windowPtr->child)) {
	Tk_MapWindow(windowPtr->child);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * bltDestroyWindow --
 *
 *	Destroys the structure containing the attributes of the window
 *      marker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Window is unmapped.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
bltDestroyWindow(graphPtr, markerPtr)
    Graph *graphPtr;
    Marker *markerPtr;
{
    WindowMarker *windowPtr = (WindowMarker *) markerPtr;

    if (windowPtr->child != NULL) {
	Tk_DeleteEventHandler(windowPtr->child, StructureNotifyMask,
	    ChildEventProc, (ClientData)windowPtr);
	Tk_ManageGeometry(windowPtr->child, (Tk_GeomMgr *) 0, (ClientData)0);
	Tk_DestroyWindow(windowPtr->child);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * bltCreateWindow --
 *
 *	Allocate memory and initialize methods for the new window marker.
 *
 * Results:
 *	The pointer to the newly allocated marker structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the window marker structure.
 *
 * ----------------------------------------------------------------------
 */
static Marker *
bltCreateWindow()
{
    WindowMarker *windowPtr;

    windowPtr = (WindowMarker *) ckcalloc(1, sizeof(WindowMarker));
    if (windowPtr != NULL) {
	windowPtr->configSpecs = windowConfigSpecs;
	windowPtr->configProc = (MarkerConfigProc*)ConfigureWindow;
	windowPtr->freeProc = (MarkerFreeProc*)bltDestroyWindow;
	windowPtr->drawProc = (MarkerDrawProc*)DrawWindow;
	windowPtr->coordsProc = (MarkerCoordsProc*)ComputeWindowCoordinates;
	windowPtr->printProc = (MarkerPrintProc*)NULL;
	windowPtr->type = MARKER_TYPE_WINDOW;
    }
    return ((Marker *) windowPtr);
}

/*
 * --------------------------------------------------------------
 *
 * ChildEventProc --
 *
 *	This procedure is invoked whenever StructureNotify events
 *	occur for a window that's managed as part of a graph window
 *	marker. This procedure's only purpose is to clean up when
 *	windows are deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is disassociated from the window item when it is
 *	deleted.
 *
 * --------------------------------------------------------------
 */
static void
ChildEventProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to record describing window item. */
    XEvent *eventPtr;		/* Describes what just happened. */
{
    WindowMarker *windowPtr = (WindowMarker *) clientData;

    if (eventPtr->type == DestroyNotify) {
	windowPtr->child = NULL;
    }
}

/*
 * --------------------------------------------------------------
 *
 * ChildGeometryProc --
 *
 *	This procedure is invoked whenever a window that's associated
 *	with a window item changes its requested dimensions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size and location on the window of the window may change,
 *	depending on the options specified for the window item.
 *
 * --------------------------------------------------------------
 */
/* ARGSUSED */
static void
ChildGeometryProc(clientData, tkwin)
    ClientData clientData;	/* Pointer to record for window item. */
    Tk_Window tkwin;		/* Window that changed its desired size. */
{
    WindowMarker *windowPtr = (WindowMarker *) clientData;

    if (windowPtr->reqWidth == 0) {
	windowPtr->width = Tk_ReqWidth(tkwin);
    }
    if (windowPtr->reqHeight == 0) {
	windowPtr->height = Tk_ReqHeight(tkwin);
    }
}

/*
 * --------------------------------------------------------------
 *
 * ChildCustodyProc --
 *
 *	This procedure is invoked when a slave window has been
 *	stolen by another geometry manager.  The information and
 *	memory associated with the slave window is released.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the widget formerly associated with the slave
 *	window to have its layout re-computed and arranged at the
 *	next idle point.
 *
 * --------------------------------------------------------------
 */
 /* ARGSUSED */
static void
ChildCustodyProc(clientData, tkwin)
    ClientData clientData;	/* Record of the former slave window. */
    Tk_Window tkwin;		/* Not used. */
{
    Marker *markerPtr = (Marker *)clientData;
    Graph *graphPtr;

    graphPtr = markerPtr->graphPtr;
    DestroyMarker(markerPtr);
    Blt_RedrawGraph(graphPtr);
}
#endif /* NO_WINDOW_MARKERS */

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureLine --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a line marker.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as line width, colors, dashes,
 *	etc. get set for markerPtr;  old resources get freed, if there
 *	were any.  The marker is eventually redisplayed.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigureLine(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    LineMarker *linePtr = (LineMarker *) markerPtr;
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;

    gcMask = (GCForeground | GCBackground | GCLineWidth | GCLineStyle |
	GCCapStyle | GCJoinStyle);
    gcValues.foreground = linePtr->normalFg->pixel;
    gcValues.background = linePtr->normalBg->pixel;
    gcValues.cap_style = CapRound;
    gcValues.join_style = JoinRound;
    gcValues.line_style = LineSolid;
    gcValues.dash_offset = 0;
    gcValues.line_width = linePtr->lineWidth;
    if (linePtr->dashes.numValues > 0) {
	gcValues.line_style = LineOnOffDash;
    }
    newGC = Blt_GetUnsharedGC(graphPtr->tkwin, gcMask, &gcValues);
    if (linePtr->gc != NULL) {
	XFreeGC(graphPtr->display, linePtr->gc);
    }
    if (linePtr->dashes.numValues > 0) {
	XSetDashes(graphPtr->display, newGC, 0, linePtr->dashes.valueList,
	    linePtr->dashes.numValues);
    }
    linePtr->gc = newGC;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputeLineCoordinates --
 *
 *	Calculate the layout position for a line marker.  Positional
 *	information is saved in the marker.  The line positions are
 *	stored in an array of points (malloc'ed).
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
ComputeLineCoordinates(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    LineMarker *linePtr = (LineMarker *) markerPtr;
    AxisPair axisPair;
    Coordinate coord;
    register XPoint *pointPtr;
    register int i;
    BBox bbox;

    linePtr->numPoints = 0;
    if (linePtr->pointArr != NULL) {
	ckfree((char *)linePtr->pointArr);
    }
    if (linePtr->numCoords < 2) {
	return;			/* Too few points */
    }
    linePtr->pointArr = (XPoint *)ckalloc(linePtr->numCoords * sizeof(XPoint));
    if (linePtr->pointArr == NULL) {
	return;			/* Can't allocate new point array */
    }
    Blt_GetAxisMapping(graphPtr, linePtr->axisMask, &axisPair);
    pointPtr = linePtr->pointArr;
    linePtr->numPoints = linePtr->numCoords;

    /* 
     * Determine the bounding box of the line and test to see if it
     * is at least partially contained within the plotting area.
     */
    bbox.ury = bbox.llx = bltPosInfinity;
    bbox.lly = bbox.urx = bltNegInfinity;
    for (i = 0; i < linePtr->numCoords; i++) {
	coord = Blt_TransformPt(graphPtr, linePtr->coordArr[i].x,
	    linePtr->coordArr[i].y, &axisPair);

	coord.x += (double)linePtr->xOffset;
	coord.y += (double)linePtr->yOffset;

	/* 
	 * Save the min and max x and y coordinates.  This will be the
	 * bounding box of the line. 
	 */
	if (coord.x > bbox.urx) {
	    bbox.urx = coord.x;
	} else if (coord.x < bbox.llx) {
	    bbox.llx = coord.x;
	}
	if (coord.y > bbox.lly) {
	    bbox.lly = coord.y;
	} else if (coord.y < bbox.ury) {
	    bbox.ury = coord.y;
	}
	pointPtr->x = BLT_RND(coord.x);
	pointPtr->y = BLT_RND(coord.y);

	BoundPoint(pointPtr);	/* Bound the points of the line to
				 * fit the size of a signed short int.
				 * We'll let X do the clipping here,
				 * although we could plug in the same
				 * clipping routines used in bltGrLine.c
				 */
	pointPtr++;
    }
    linePtr->clipped = TestMarkerBBox(graphPtr, &bbox);
}

static void
DrawLine(markerPtr)
    Marker *markerPtr;
{
    LineMarker *linePtr = (LineMarker *) markerPtr;

    if (linePtr->numPoints > 0) {
	Graph *graphPtr = markerPtr->graphPtr;

	XDrawLines(graphPtr->display, graphPtr->pixwin, linePtr->gc,
	    linePtr->pointArr, linePtr->numPoints, CoordModeOrigin);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * PrintLine --
 *
 *	Prints postscript commands to display the connect line.
 *	Dashed lines need to be handled specially, especially if a
 *	a background color is designated.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PostScript output commands are saved in the interpreter
 *	(infoPtr->interp) result field.
 *
 * ----------------------------------------------------------------------
 */
static void
PrintLine(markerPtr)
    Marker *markerPtr;
{
    LineMarker *linePtr = (LineMarker *) markerPtr;

    if (linePtr->numPoints > 1) {
	Graph *graphPtr = markerPtr->graphPtr;

	Blt_LineWidthToPostScript(graphPtr, linePtr->lineWidth);
	Blt_ForegroundToPostScript(graphPtr, linePtr->normalFg);
	if (linePtr->dashes.numValues > 0) {
	    Blt_LineDashesToPostScript(graphPtr, &(linePtr->dashes));
	    Tcl_AppendResult(graphPtr->interp, "/DashesProc {\n  gsave\n    ",
		(char *)NULL);
	    Blt_BackgroundToPostScript(graphPtr, linePtr->normalBg);
	    Tcl_AppendResult(graphPtr->interp, "    ", (char *)NULL);
	    Blt_LineDashesToPostScript(graphPtr, (Dashes *)NULL);
	    Tcl_AppendResult(graphPtr->interp, "stroke\n  grestore\n} def\n",
		(char *)NULL);
	} else {
	    Tcl_AppendResult(graphPtr->interp, "/DashesProc {} def\n",
		(char *)NULL);
	}
	Blt_PrintLine(graphPtr, linePtr->pointArr, linePtr->numPoints);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyLine --
 *
 *	Destroys the structure and attributes of a line marker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Line attributes (GCs, colors, stipple, etc) get released.
 *	Memory is deallocated, X resources are freed.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyLine(graphPtr, markerPtr)
    Graph *graphPtr;
    Marker *markerPtr;
{
    LineMarker *linePtr = (LineMarker *) markerPtr;

    if (linePtr->gc != NULL) {
	XFreeGC(graphPtr->display, linePtr->gc);
    }
    if (linePtr->pointArr != NULL) {
	ckfree((char *)linePtr->pointArr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * bltCreateLine --
 *
 *	Allocate memory and initialize methods for a new line marker.
 *
 * Results:
 *	The pointer to the newly allocated marker structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the line marker structure.
 *
 * ----------------------------------------------------------------------
 */
static Marker *
bltCreateLine()
{
    LineMarker *linePtr;

    linePtr = (LineMarker *) ckcalloc(1, sizeof(LineMarker));
    if (linePtr != NULL) {
	linePtr->configSpecs = lineConfigSpecs;
	linePtr->configProc = (MarkerConfigProc*)ConfigureLine;
	linePtr->freeProc = (MarkerFreeProc*)DestroyLine;
	linePtr->drawProc = (MarkerDrawProc*)DrawLine;
	linePtr->coordsProc = (MarkerCoordsProc*)ComputeLineCoordinates;
	linePtr->printProc = (MarkerPrintProc*)PrintLine;
	linePtr->type = MARKER_TYPE_LINE;
    }
    return ((Marker *) linePtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigurePolygon --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a polygon marker.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as polygon color, dashes, fillstyle,
 *	etc. get set for markerPtr;  old resources get freed, if there
 *	were any.  The marker is eventually redisplayed.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigurePolygon(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    PolygonMarker *polygonPtr = (PolygonMarker *) markerPtr;
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;

    gcMask = (GCForeground | GCLineWidth | GCLineStyle | GCCapStyle |
	GCJoinStyle);
    gcValues.foreground = polygonPtr->outlineColor->pixel;
    gcValues.cap_style = CapRound;
    gcValues.join_style = JoinRound;
    gcValues.line_style = LineSolid;
    gcValues.dash_offset = 0;
    gcValues.line_width = polygonPtr->lineWidth;
    if (polygonPtr->dashes.numValues > 0) {
	gcValues.line_style = LineOnOffDash;
    }
    newGC = Blt_GetUnsharedGC(graphPtr->tkwin, gcMask, &gcValues);
    if (polygonPtr->dashes.numValues > 0) {
	XSetDashes(graphPtr->display, newGC, 0, polygonPtr->dashes.valueList,
	    polygonPtr->dashes.numValues);
    }
    if (polygonPtr->outlineGC != NULL) {
	XFreeGC(graphPtr->display, polygonPtr->outlineGC);
    }
    polygonPtr->outlineGC = newGC;

    gcMask = GCForeground;
    gcValues.foreground = polygonPtr->outlineColor->pixel;
    if (polygonPtr->stipple != None) {
	gcValues.stipple = polygonPtr->stipple;
	gcValues.fill_style = FillStippled;
	if (polygonPtr->fillColor != NULL) {
	    gcValues.fill_style = FillOpaqueStippled;
	    gcValues.background = polygonPtr->fillColor->pixel;
	    gcMask |= GCBackground;
	}
	gcMask |= (GCStipple | GCFillStyle);
    } else if (polygonPtr->fillColor != NULL) {
	gcValues.foreground = polygonPtr->fillColor->pixel;
    }
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (polygonPtr->fillGC != NULL) {
	Tk_FreeGC(graphPtr->display, polygonPtr->fillGC);
    }
    polygonPtr->fillGC = newGC;

    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ComputePolygonCoordinates --
 *
 *	Calculate the layout position for a polygon marker.  Positional
 *	information is saved in the polygon in an array of points
 *	(malloc'ed).
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------
 */
static void
ComputePolygonCoordinates(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    PolygonMarker *polygonPtr = (PolygonMarker *) markerPtr;
    register int i;
    XPoint *pointArr;
    register XPoint *pointPtr;
    AxisPair axisPair;
    Coordinate coord;
    BBox bbox;
    int numPoints;

    if (polygonPtr->pointArr != NULL) {
	ckfree((char *)polygonPtr->pointArr);
	polygonPtr->pointArr = NULL;
    }
    polygonPtr->numPoints = 0;
    if (polygonPtr->numCoords < 3) {
	return;			/* Too few points */
    }
    numPoints = polygonPtr->numCoords + 1;
    pointArr = (XPoint *)ckalloc(numPoints * sizeof(XPoint));
    if (pointArr == NULL) {
	return;			/* Can't allocate point array */
    }
    Blt_GetAxisMapping(graphPtr, polygonPtr->axisMask, &axisPair);
    pointPtr = pointArr;

    /* 
     * Determine the bounding box of the polygon and test to see if it
     * is at least partially contained within the plotting area.
     */
    bbox.ury = bbox.llx = bltPosInfinity;
    bbox.lly = bbox.urx = bltNegInfinity;
    for (i = 0; i < polygonPtr->numCoords; i++) {
	coord = Blt_TransformPt(graphPtr, polygonPtr->coordArr[i].x,
	    polygonPtr->coordArr[i].y, &axisPair);
	coord.x += (double)polygonPtr->xOffset;
	coord.y += (double)polygonPtr->yOffset;
	if (coord.x > bbox.urx) {
	    bbox.urx = coord.x;
	} else if (coord.x < bbox.llx) {
	    bbox.llx = coord.x;
	}
	pointPtr->x = BLT_RND(coord.x);
	if (coord.y > bbox.lly) {
	    bbox.lly = coord.y;
	} else if (coord.y < bbox.ury) {
	    bbox.ury = coord.y;
	}
	pointPtr->y = BLT_RND(coord.y);

	BoundPoint(pointPtr);	/* Bound the points of the polygon to
				 * fit the size of a signed short int.
				 * We'll let X do the clipping here,
				 * because we really don't want to
				 * write a polygon clipping routine.
				 * Just yet. */
	pointPtr++;
    }
    *pointPtr = pointArr[0];	/* Ensure that the polygon is closed */

    polygonPtr->clipped = TestMarkerBBox(graphPtr, &bbox);
    if (polygonPtr->clipped) {
	polygonPtr->numPoints = 0;
	ckfree((char *)pointArr);
    } else {
	polygonPtr->numPoints = numPoints;
	polygonPtr->pointArr = pointArr;
    }
}


static void
DrawPolygon(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    PolygonMarker *polygonPtr = (PolygonMarker *) markerPtr;

    if (polygonPtr->numPoints < 3) {
	return;
    }
    if ((polygonPtr->fillColor != NULL) || (polygonPtr->stipple != None)) {
	XFillPolygon(graphPtr->display, graphPtr->pixwin, polygonPtr->fillGC,
	    polygonPtr->pointArr, polygonPtr->numPoints, Complex,
	    CoordModeOrigin);
    }
    if (polygonPtr->lineWidth > 0) {
	XDrawLines(graphPtr->display, graphPtr->pixwin, polygonPtr->outlineGC,
	    polygonPtr->pointArr, polygonPtr->numPoints, CoordModeOrigin);
    }
}


static void
PrintPolygon(markerPtr)
    Marker *markerPtr;
{
    Graph *graphPtr = markerPtr->graphPtr;
    PolygonMarker *polygonPtr = (PolygonMarker *) markerPtr;

    if (polygonPtr->numPoints < 3) {
	return;
    }
    /*
     * If a background color was specified, draw the polygon filled
     * with the background color.
     */
    if (polygonPtr->fillColor != NULL) {
	Blt_BackgroundToPostScript(graphPtr, polygonPtr->fillColor);
	Blt_PolygonToPostScript(graphPtr, polygonPtr->pointArr,
	    polygonPtr->numPoints);
    }
    /*
     * Draw the outline and/or stipple in the foreground color.
     */
    if ((polygonPtr->lineWidth > 0) || (polygonPtr->stipple != None)) {
	Blt_ForegroundToPostScript(graphPtr, polygonPtr->outlineColor);

	/*
	 * Create a path, regardless if there's an outline, because
	 * we'll use it for stippling.
	 */
	Blt_LinesToPostScript(graphPtr, polygonPtr->pointArr,
	    polygonPtr->numPoints);

	if (polygonPtr->lineWidth > 0) {
	    Tcl_AppendResult(graphPtr->interp, "Stroke ", (char *)NULL);
	}
	Tcl_AppendResult(graphPtr->interp, "closepath\n", (char *)NULL);
	if (polygonPtr->stipple != None) {
	    int width, height;

	    Tk_SizeOfBitmap(graphPtr->display, polygonPtr->stipple, &width,
		&height);
	    Blt_StippleToPostScript(graphPtr, polygonPtr->stipple, width,
		height, True);
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyPolygon --
 *
 *	Release memory and resources allocated for the polygon element.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the polygon element is freed up.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyPolygon(graphPtr, markerPtr)
    Graph *graphPtr;
    Marker *markerPtr;
{
    PolygonMarker *polygonPtr = (PolygonMarker *) markerPtr;

    if (polygonPtr->fillGC != NULL) {
	Tk_FreeGC(graphPtr->display, polygonPtr->fillGC);
    }
    if (polygonPtr->outlineGC != NULL) {
	XFreeGC(graphPtr->display, polygonPtr->outlineGC);
    }
    if (polygonPtr->pointArr != NULL) {
	ckfree((char *)polygonPtr->pointArr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * bltCreatePolygon --
 *
 *	Allocate memory and initialize methods for the new polygon marker.
 *
 * Results:
 *	The pointer to the newly allocated marker structure is returned.
 *
 * Side effects:
 *	Memory is allocated for the polygon marker structure.
 *
 * ----------------------------------------------------------------------
 */
static Marker *
bltCreatePolygon()
{
    PolygonMarker *polygonPtr;

    polygonPtr = (PolygonMarker *) ckcalloc(1, sizeof(PolygonMarker));
    if (polygonPtr != NULL) {
	polygonPtr->configSpecs = polygonConfigSpecs;
	polygonPtr->configProc = (MarkerConfigProc*)ConfigurePolygon;
	polygonPtr->freeProc = (MarkerFreeProc*)DestroyPolygon;
	polygonPtr->drawProc = (MarkerDrawProc*)DrawPolygon;
	polygonPtr->coordsProc = (MarkerCoordsProc*)ComputePolygonCoordinates;
	polygonPtr->printProc = (MarkerPrintProc*)PrintPolygon;
	polygonPtr->type = MARKER_TYPE_POLYGON;
    }
    return ((Marker *) polygonPtr);
}


static int
FindMarker(graphPtr, markerId, markerPtrPtr)
    Graph *graphPtr;
    char *markerId;
    Marker **markerPtrPtr;
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&(graphPtr->markerTable), markerId);
    if (hPtr == NULL) {
	Tcl_AppendResult(graphPtr->interp, "can't find marker \"", markerId,
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    *markerPtrPtr = (Marker *) Tcl_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetMarkerType --
 *
 *	Convert the marker type string value into a numeric value.
 *
 * Results:
 *	The value representing the marker type is returned.
 *
 * ----------------------------------------------------------------------
 */
static int
GetMarkerType(interp, name, typePtr)
    Tcl_Interp *interp;
    char *name;
    MarkerType *typePtr;
{
    unsigned int length;
    char c;

    c = name[0];
    length = strlen(name);
    if ((c == 't') && (strncmp(name, "text", length) == 0)) {
	*typePtr = MARKER_TYPE_TEXT;
    } else if ((c == 'l') && (strncmp(name, "line", length) == 0)) {
	*typePtr = MARKER_TYPE_LINE;
    } else if ((c == 'b') && (strncmp(name, "bitmap", length) == 0)) {
	*typePtr = MARKER_TYPE_BITMAP;
    } else if ((c == 'p') && (strncmp(name, "polygon", length) == 0)) {
	*typePtr = MARKER_TYPE_POLYGON;
#ifndef NO_WINDOW_MARKERS
    } else if ((c == 'w') && (strncmp(name, "window", length) == 0)) {
	*typePtr = MARKER_TYPE_WINDOW;
#endif /*NO_WINDOW_MARKERS*/
    } else {
#ifndef NO_WINDOW_MARKERS
	Tcl_AppendResult(interp, "unknown marker type \"", name, "\": should ",
	    "be \"text\", \"line\", \"polygon\", \"bitmap\", or \"window\"",
	    (char *)NULL);
#else
	Tcl_AppendResult(interp, "unknown marker type \"", name, "\": should ",
	    "be \"text\", \"line\", \"polygon\", or \"bitmap\"", (char *)NULL);
#endif
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * NameOfMarkerType --
 *
 *	Convert the marker type value into a string.
 *
 * Results:
 *	The string representing the marker type is returned.
 *
 * ----------------------------------------------------------------------
 */
static char *
NameOfMarkerType(type)
    MarkerType type;
{
    switch (type) {
    case MARKER_TYPE_TEXT:
	return "text";
    case MARKER_TYPE_LINE:
	return "line";
    case MARKER_TYPE_BITMAP:
	return "bitmap";
    case MARKER_TYPE_POLYGON:
	return "polygon";
    case MARKER_TYPE_WINDOW:
	return "window";
    default:
	return "unknown marker type";
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * IdsOper --
 *
 *	Returns a list of marker identifiers in interp->result;
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * ----------------------------------------------------------------------
 */
static int
IdsOper(graphPtr, argc, argv)
    Graph *graphPtr;
    int argc;
    char **argv;
{
    Marker *markerPtr;
    Blt_ListItem *itemPtr;

    for (itemPtr = Blt_FirstListItem(&(graphPtr->markerList)); itemPtr != NULL;
	itemPtr = Blt_NextItem(itemPtr)) {
	markerPtr = (Marker *) Blt_GetItemValue(itemPtr);
	/*
	 * Add the marker Id to the list if
	 * 1) the pattern matches, or
	 * 2) no pattern was provided (list all markerIds)
	 */
	if ((argc == 3) || (Tcl_StringMatch(markerPtr->nameId, argv[3]))) {
	    Tcl_AppendElement(graphPtr->interp, markerPtr->nameId);
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * CgetOper --
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOper(graphPtr, argc, argv)
    Graph *graphPtr;		/* Graph widget record */
    int argc;			/* not used */
    char *argv[];		/* Contains the markerId and the
				 * option to be queried */
{
    Marker *markerPtr;

    if (FindMarker(graphPtr, argv[3], &markerPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tk_ConfigureValue(graphPtr->interp, graphPtr->tkwin,
	    markerPtr->configSpecs, (char *)markerPtr, argv[4], 0) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureOper --
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureOper(graphPtr, argc, argv)
    Graph *graphPtr;		/* Graph widget record */
    int argc;			/* Number of options */
    char *argv[];		/* List of marker options */
{
    Marker *markerPtr;
    int result;
    int flags = TK_CONFIG_ARGV_ONLY;
    char *oldId;

    if (FindMarker(graphPtr, argv[3], &markerPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (argc == 4) {
	return (Tk_ConfigureInfo(graphPtr->interp, graphPtr->tkwin,
		markerPtr->configSpecs, (char *)markerPtr, (char *)NULL,flags));
    } else if (argc == 5) {
	return (Tk_ConfigureInfo(graphPtr->interp, graphPtr->tkwin,
		markerPtr->configSpecs, (char *)markerPtr, argv[4], flags));
    }
    oldId = markerPtr->nameId;
    if (Tk_ConfigureWidget(graphPtr->interp, graphPtr->tkwin,
	    markerPtr->configSpecs, argc - 4, argv + 4, (char *)markerPtr,
	    flags) != TCL_OK) {
	return TCL_ERROR;
    }
    result = (*markerPtr->configProc) (markerPtr);
    if (oldId != markerPtr->nameId) {
	int isNew;
	Tcl_HashEntry *hPtr;

	/*
	 * Rename the marker only if no marker already exists by that name
	 */
	hPtr = Tcl_FindHashEntry(&(graphPtr->markerTable), markerPtr->nameId);
	if (hPtr != NULL) {
	    Tcl_AppendResult(graphPtr->interp, "can't rename marker: \"",
		markerPtr->nameId, "\" already exists", (char *)NULL);
	    markerPtr->nameId = oldId;
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&(graphPtr->markerTable), oldId);
	Tcl_DeleteHashEntry(hPtr);
	hPtr = Tcl_CreateHashEntry(&(graphPtr->markerTable), markerPtr->nameId,
	    &isNew);
	Tcl_SetHashValue(hPtr, (char *)markerPtr);
    }
    markerPtr->flags |= COORDS_NEEDED;
    if (result == TCL_OK) {
	if (markerPtr->drawUnder) {
	    graphPtr->flags |= UPDATE_PIXMAP;
	}
	Blt_RedrawGraph(graphPtr);
    }
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * bltCreateOper --
 *
 *	This procedure creates and initializes a new marker.
 *
 * Results:
 *	The return value is a pointer to a structure describing
 *	the new element.  If an error occurred, then the return
 *	value is NULL and an error message is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated, etc.
 *
 * ----------------------------------------------------------------------
 */
static int
bltCreateOper(graphPtr, argc, argv)
    Graph *graphPtr;		/* Graph widget record */
    int argc;
    char *argv[];
{
    Marker *markerPtr;
    Blt_ListItem *iPtr;
    Tcl_HashEntry *hPtr;
    int isNew;
    MarkerType type;

    markerPtr = NULL;

    /* Create the new marker based upon the given type */

    if (GetMarkerType(graphPtr->interp, argv[3], &type) != TCL_OK) {
	return TCL_ERROR;
    }
    switch (type) {
    case MARKER_TYPE_LINE:
	markerPtr = bltCreateLine();
	break;
#ifndef NO_WINDOW_MARKERS
    case MARKER_TYPE_WINDOW:
	markerPtr = bltCreateWindow();
	break;
#endif /* NO_WINDOW_MARKERS */
    case MARKER_TYPE_BITMAP:
	markerPtr = bltCreateBitmap();
	break;
    case MARKER_TYPE_POLYGON:
	markerPtr = bltCreatePolygon();
	break;
    case MARKER_TYPE_TEXT:
	markerPtr = bltCreateText();
	break;
    default:
	Tcl_SetResult(graphPtr->interp, "unknown marker type", TCL_STATIC);
	return TCL_ERROR;
    }
    if (markerPtr == NULL) {
	Panic("can't allocate new marker structure");
    }
    markerPtr->graphPtr = graphPtr;
    markerPtr->type = (MarkerType) type;
    markerPtr->axisMask = DEF_AXIS_MASK;
    markerPtr->drawUnder = FALSE;
    markerPtr->flags |= COORDS_NEEDED;
    markerPtr->mapped = TRUE;

    if (Tk_ConfigureWidget(graphPtr->interp, graphPtr->tkwin,
	    markerPtr->configSpecs, argc - 4, argv + 4,
	    (char *)markerPtr, 0) != TCL_OK) {
	DestroyMarker(markerPtr);
	return TCL_ERROR;
    }
    if (markerPtr->nameId == NULL) {
	char string[200];

	/* If a marker id was provided, use it.  Otherwise generate a
	 * new one */

	sprintf(string, "MARKER%d", graphPtr->nextMarkerId++);
	markerPtr->nameId = Tk_GetUid(string);
    }
    if ((*markerPtr->configProc) (markerPtr) != TCL_OK) {
	DestroyMarker(markerPtr);
	return TCL_ERROR;
    }
    hPtr = Tcl_CreateHashEntry(&(graphPtr->markerTable), markerPtr->nameId,
	&isNew);
    if (!isNew) {
	Marker *oldPtr;

	/*
	 * Marker id already exists.  Delete the old marker and list entry.
	 */
	oldPtr = (Marker *) Tcl_GetHashValue(hPtr);
	iPtr = Blt_FindItem(&(graphPtr->markerList), (char *)oldPtr);
	Blt_DeleteItem(iPtr);
	DestroyMarker(oldPtr);
    }
    iPtr = Blt_NewItem((char *)markerPtr);
    Blt_LinkAfter(&(graphPtr->markerList), iPtr, (Blt_ListItem *)NULL);
    Blt_SetItemValue(iPtr, (ClientData)markerPtr);
    Tcl_SetHashValue(hPtr, (ClientData)markerPtr);

    Tcl_SetResult(graphPtr->interp, markerPtr->nameId, TCL_STATIC);
    if (markerPtr->drawUnder) {
	graphPtr->flags |= UPDATE_PIXMAP;
    }
    Blt_RedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * DeleteOper --
 *
 *	Deletes the marker given by markerId.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new display list.
 *
 * ----------------------------------------------------------------------
 */
static int
DeleteOper(graphPtr, argc, argv)
    Graph *graphPtr;
    int argc;
    char *argv[];
{
    Blt_ListItem *itemPtr, *deletePtr;
    Marker *markerPtr;
    register int i;

    for (itemPtr = Blt_FirstListItem(&(graphPtr->markerList)); itemPtr != NULL;
	/* empty */ ) {
	markerPtr = (Marker *) Blt_GetItemValue(itemPtr);
	deletePtr = itemPtr;
	itemPtr = Blt_NextItem(itemPtr);
	for (i = 3; i < argc; i++) {
	    if (Tcl_StringMatch(markerPtr->nameId, argv[i])) {
		break;
	    }
	}
	if (i < argc) {
	    Tcl_HashEntry *hPtr;

	    hPtr = Tcl_FindHashEntry(&(graphPtr->markerTable),
		markerPtr->nameId);
	    Tcl_DeleteHashEntry(hPtr);
	    Blt_DeleteItem(deletePtr);
	    DestroyMarker(markerPtr);
	}
    }
    Blt_RedrawGraph(graphPtr);
	/* ADD BY SEAN GILMAN */
	/* If the table is empty reset the counter to 0 so that we don't
	   keep incrementing (and allocating) forever.
	   This fixes a pseudo memory leak with markers are created
	   and destroyed over and over */
	if (Blt_FirstListItem(&(graphPtr->markerList))==NULL)
	{
		graphPtr->nextMarkerId = 1;
	}
	/* ADD BY SEAN */
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * RelinkOper --
 *
 *	Reorders the marker (given by the first markerId) before/after
 *	the another marker (given by the second markerId) in the
 *	marker display list.  If no second markerId is given, the
 *	marker is placed at the beginning/end of the list.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new display list.
 *
 * ----------------------------------------------------------------------
 */
static int
RelinkOper(graphPtr, argc, argv)
    Graph *graphPtr;
    int argc;
    char *argv[];
{
    Blt_ListItem *itemPtr, *placePtr;
    Marker *markerPtr;

    /* Find the new marker to be inserted into the display list */
    if (FindMarker(graphPtr, argv[3], &markerPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Now use the marker to find the entry in the display list */
    itemPtr = Blt_FindItem(&(graphPtr->markerList), (char *)markerPtr);

    placePtr = NULL;
    if (argc == 5) {
	if (FindMarker(graphPtr, argv[4], &markerPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	placePtr = Blt_FindItem(&(graphPtr->markerList), (char *)markerPtr);
    }
    /* Unlink the list item and relink it at the new location */
    Blt_UnlinkItem(itemPtr);

    if (argv[2][0] == 'a') {
	Blt_LinkAfter(&(graphPtr->markerList), itemPtr, placePtr);
    } else {
	Blt_LinkBefore(&(graphPtr->markerList), itemPtr, placePtr);
    }
    Blt_RedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ExistsOper --
 *
 *	Returns if marker by a given ID currently exists.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ExistsOper(graphPtr, argc, argv)
    Graph *graphPtr;
    int argc;			/* not used */
    char **argv;
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&(graphPtr->markerTable), argv[3]);
    Tcl_SetResult(graphPtr->interp, (hPtr != NULL) ? "1" : "0", TCL_STATIC);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TypeOper --
 *
 *	Returns a symbolic name for the type of the marker whose ID is
 *	given.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *	interp->result contains the symbolic type of the marker.
 *
 * ----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TypeOper(graphPtr, argc, argv)
    Graph *graphPtr;
    int argc;			/* not used */
    char **argv;
{
    Marker *markerPtr;

    if (FindMarker(graphPtr, argv[3], &markerPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetResult(graphPtr->interp, NameOfMarkerType(markerPtr->type),
	TCL_STATIC);
    return TCL_OK;
}

/* Public routines */

/*
 * --------------------------------------------------------------
 *
 * Blt_MarkerOper --
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
 * --------------------------------------------------------------
 */

static Blt_OperSpec operSpecs[] =
{
    {"after", 1, (Blt_OperProc) RelinkOper, 4, 5, "markerId ?afterMarkerId?",},
    {"before", 1,(Blt_OperProc) RelinkOper, 4, 5, "markerId ?beforeMarkerId?",},
    {"cget", 2, (Blt_OperProc) CgetOper, 5, 5, "markerId option",},
    {"configure", 2, (Blt_OperProc) ConfigureOper, 4, 0,
	"markerId ?option value?...",},
    {"create", 2, (Blt_OperProc) bltCreateOper, 3, 0,
	"markerType ?option value?...",},
    {"delete", 1, (Blt_OperProc) DeleteOper, 3, 0, "?markerId?...",},
    {"exists", 1, (Blt_OperProc) ExistsOper, 4, 4, "markerId",},
    {"names", 1, (Blt_OperProc) IdsOper, 3, 4, "?pattern?",},
    {"type", 1, (Blt_OperProc) TypeOper, 4, 4, "markerId",},
};
static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

/*ARGSUSED*/
int
Blt_MarkerOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;		/* not used */
    int argc;
    char **argv;
{
    Blt_OperProc proc;
    int result;

    proc = Blt_LookupOperation(graphPtr->interp, numSpecs, operSpecs,
	BLT_OPER_ARG2, argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (graphPtr, argc, argv);
    return (result);
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_PrintMarkers --
 *
 * -----------------------------------------------------------------
 */
void
Blt_PrintMarkers(graphPtr, under)
    Graph *graphPtr;
    int under;
{
    Blt_ListItem *itemPtr;
    register Marker *markerPtr;

    for (itemPtr = Blt_FirstListItem(&(graphPtr->markerList)); itemPtr != NULL;
	itemPtr = Blt_NextItem(itemPtr)) {
	markerPtr = (Marker *) Blt_GetItemValue(itemPtr);
	if ((markerPtr->printProc == NULL) || (markerPtr->numCoords == 0)) {
	    continue;
	}
	if (markerPtr->drawUnder != under) {
	    continue;
	}
	if (markerPtr->mapped == 0) {
	    continue;
	}
	if (markerPtr->elemId != NULL) {
	    Tcl_HashEntry *hPtr;

	    hPtr = Tcl_FindHashEntry(&(graphPtr->elemTable), markerPtr->elemId);
	    if (hPtr != NULL) {
		Element *elemPtr;

		elemPtr = (Element *)Tcl_GetHashValue(hPtr);
		if (!elemPtr->mapped) {
		    continue;
		}
	    }
	}
	Tcl_AppendResult(graphPtr->interp, "\n% Marker \"", markerPtr->nameId,
	    "\" is a ", NameOfMarkerType(markerPtr->type), ".\n\n", 
	    (char *)NULL);
	(*markerPtr->printProc) (markerPtr);
    }
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_DrawMarkers --
 *
 *	Calls the individual drawing routines (based on marker type)
 *	for each marker in the display list.
 *
 *	A marker will not be drawn if
 *
 *	1) there is an element linked with the marker (whose name is
 *	   elemId) and that element is not currently being displayed.
 *
 *	2) there are no coordinates available for the marker.
 *
 *	3) the smarkere at which we're drawing is different from how
 *	   the marker wants to be displayed (either above/below the
 *	   elements).
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Markers are drawn into the drawable (pixmap) which will eventually
 *	be displayed in the graph window.
 *
 * -----------------------------------------------------------------
 */
void
Blt_DrawMarkers(graphPtr, under)
    Graph *graphPtr;
    int under;
{
    Blt_ListItem *iPtr;
    Marker *markerPtr;

    for (iPtr = Blt_FirstListItem(&(graphPtr->markerList)); iPtr != NULL;
	iPtr = Blt_NextItem(iPtr)) {
	markerPtr = (Marker *) Blt_GetItemValue(iPtr);

	if (markerPtr->numCoords == 0) {
	    continue;
	}
	if (markerPtr->drawUnder != under) {
	    continue;
	}
	if ((!markerPtr->mapped) || (markerPtr->clipped)) {
	    continue;
	}
	if (markerPtr->elemId != NULL) {
	    Tcl_HashEntry *hPtr;

	    hPtr = Tcl_FindHashEntry(&(graphPtr->elemTable), markerPtr->elemId);
	    if (hPtr != NULL) {
		Element *elemPtr;

		elemPtr = (Element *)Tcl_GetHashValue(hPtr);
		if (!elemPtr->mapped) {
		    continue;
		}
	    }
	}
	(*markerPtr->drawProc) (markerPtr);
    }
}


void
Blt_GetMarkerCoordinates(graphPtr)
    Graph *graphPtr;
{
    Blt_ListItem *itemPtr;
    Marker *markerPtr;

    for (itemPtr = Blt_FirstListItem(&(graphPtr->markerList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	markerPtr = (Marker *) Blt_GetItemValue(itemPtr);
	if ((markerPtr->numCoords == 0) || (!markerPtr->mapped)) {
	    continue;
	}
	if ((graphPtr->flags & COORDS_ALL_PARTS) ||
	    (markerPtr->flags & COORDS_NEEDED)) {
	    (*markerPtr->coordsProc) (markerPtr);
	    markerPtr->flags &= ~COORDS_NEEDED;
	}
    }
}


void
Blt_DestroyMarkers(graphPtr)
    Graph *graphPtr;
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;
    Marker *markerPtr;

    Blt_ResetList(&(graphPtr->markerList));
    for (hPtr = Tcl_FirstHashEntry(&(graphPtr->markerTable), &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	markerPtr = (Marker *) Tcl_GetHashValue(hPtr);
	DestroyMarker(markerPtr);
    }
    Tcl_DeleteHashTable(&(graphPtr->markerTable));
}
#endif /* !NO_GRAPH */
