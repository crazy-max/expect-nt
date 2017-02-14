/*
 * bltGrPs.c --
 *
 *      This module implements a graph widget for
 *      the Tk toolkit.
 *
 * Copyright 1991-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef NO_GRAPH

#include "bltGraph.h"
#include "tkFont.h"

#define PS_MAXPATH	1500	/* Maximum number of components in a PostScript
				 * (level 1) path. */

#define SCRATCH_LENGTH  (BUFSIZ*2)

typedef enum {
    PS_MONO_BACKGROUND, PS_MONO_FOREGROUND
} MonoAttribute;

typedef enum {
    PS_MODE_MONOCHROME, PS_MODE_GRAYSCALE, PS_MODE_COLOR
} ColorMode;

static int ColorModeParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *ColorModePrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));

static Tk_CustomOption colorModeOption =
{
    ColorModeParseProc, ColorModePrintProc, (ClientData)0,
};

extern Tk_CustomOption bltLengthOption;
extern Tk_CustomOption bltPadOption;

/*
 * PageExtents --
 *
 * 	Convenience structure to pass information to various routines
 *	indicating how the PostScript page is arranged.
 *
 */
typedef struct {
    int width, height;		/* Dimensions of the page.  */
    int bbWidth, bbHeight;	/* Dimensions of the bounding box.
				 * This includes the graph and padding. */
    int adjustX, adjustY;	/* Distance from page edge to upper left
				 * corner of the bounding box */
    double maxScale;		/* Scale to maximize plot aspect ratio */
} PageExtents;

/*
 * PostScript --
 *
 * 	Structure contains information specific to the outputting of
 *	PostScript commands to print the graph.
 *
 */
typedef struct {
    int decorations;		/* If non-zero, print graph with
				 * color background and 3D borders */
    PostScriptDestroyProc *destroyProc;

    /* User configurable fields */

    int reqWidth, reqHeight;	/* If greater than zero, represents the
				 * requested dimensions of the printed graph */
    int reqPaperWidth;
    int reqPaperHeight;		/* Requested dimensions for the PostScript
				 * page. Can constrain the size of the graph
				 * if the graph (plus padding) is larger than
				 * the size of the page. */
    Pad padX, padY;		/* Requested padding on the exterior of the
				 * graph. This forms the bounding box for
				 * the page. */
    ColorMode colorMode;	/* Selects the color mode for PostScript page
				 * (0=monochrome, 1=greyscale, 2=color) */
    char *colorVarName;		/* If non-NULL, is the name of a Tcl array
				 * variable containing X to PostScript color
				 * translations */
    char *fontVarName;		/* If non-NULL, is the name of a Tcl array
				 * variable containing X to PostScript font
				 * translations */
    int landscape;		/* If non-zero, rotate page 90 degrees */
    int center;			/* If non-zero, center the graph on the page */
    int maxpect;		/* If non-zero, indicates to scale the graph
				 * so that it fills the page (maintaining the
				 * aspect ratio of the graph) */
} PostScript;

#define DEF_PS_CENTER		"1"
#define DEF_PS_COLOR_MAP	(char *)NULL
#define DEF_PS_COLOR_MODE	"color"
#define DEF_PS_DECORATIONS	"1"
#define DEF_PS_FONT_MAP		(char *)NULL
#define DEF_PS_LANDSCAPE	"0"
#define DEF_PS_MAXPECT		"0"
#define DEF_PS_PADX		"1.0i"
#define DEF_PS_PADY		"1.0i"
#define DEF_PS_PAPERHEIGHT	"11.0i"
#define DEF_PS_PAPERWIDTH	"8.5i"

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_BOOLEAN, "-center", "psCenter", "PsCenter",
	DEF_PS_CENTER, Tk_Offset(PostScript, center),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_STRING, "-colormap", "psColorMap", "PsColorMap",
	DEF_PS_COLOR_MAP, Tk_Offset(PostScript, colorVarName), 0},
    {TK_CONFIG_CUSTOM, "-colormode", "psColorMode", "PsColorMode",
	DEF_PS_COLOR_MODE, Tk_Offset(PostScript, colorMode),
	TK_CONFIG_DONT_SET_DEFAULT, &colorModeOption},
    {TK_CONFIG_BOOLEAN, "-decorations", "psDecorations", "PsDecorations",
	DEF_PS_DECORATIONS, Tk_Offset(PostScript, decorations),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_STRING, "-fontmap", "psFontMap", "PsFontMap",
	DEF_PS_FONT_MAP, Tk_Offset(PostScript, fontVarName), 0},
    {TK_CONFIG_CUSTOM, "-height", "psHeight", "PsHeight",
	(char *)NULL, Tk_Offset(PostScript, reqHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-landscape", "psLandscape", "PsLandscape",
	DEF_PS_LANDSCAPE, Tk_Offset(PostScript, landscape),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-maxpect", "psMaxpect", "PsMaxpect",
	DEF_PS_MAXPECT, Tk_Offset(PostScript, maxpect),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-padx", "psPadX", "PsPadX",
	DEF_PS_PADX, Tk_Offset(PostScript, padX), 0, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", "psPadY", "PsPadY",
	DEF_PS_PADY, Tk_Offset(PostScript, padY), 0, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-paperheight", "psPaperHeight", "PsPaperHeight",
	DEF_PS_PAPERHEIGHT, Tk_Offset(PostScript, reqPaperHeight),
	0, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-paperwidth", "psPaperWidth", "PsPaperWidth",
	DEF_PS_PAPERWIDTH, Tk_Offset(PostScript, reqPaperWidth),
	0, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-width", "psWidth", "PsWidth",
	(char *)NULL, Tk_Offset(PostScript, reqWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

#ifndef NO_INLINE_PS_PROLOG
#include "bltGrPs.h"
#endif /*NO_INLINE_PS_PROLOG*/

extern void Blt_GetGraphCoordinates _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_LayoutGraph _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_PrintMarkers _ANSI_ARGS_((Graph *graphPtr, int under));
extern void Blt_PrintElements _ANSI_ARGS_((Graph *graphPtr));
extern void Blt_PrintActiveElements _ANSI_ARGS_((Graph *graphPtr));

/*
 *----------------------------------------------------------------------
 *
 * ColorModeParseProc --
 *
 *	Convert the string representation of a PostScript color mode
 *	into the enumerated type representing the color level:
 *
 *	    PS_MODE_COLOR 	- Full color
 *	    PS_MODE_GRAYSCALE  	- Color converted to grayscale
 *	    PS_MODE_MONOCHROME 	- Only black and white
 *
 * Results:
 *	The return value is a standard Tcl result.  The color level is
 *	written into the page layout information structure.
 *
 * Side Effects:
 *	Future invocations of the "postscript" option will use this
 *	variable to determine how color information will be displayed
 *	in the PostScript output it produces.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColorModeParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* New legend position string */
    char *widgRec;		/* Graph widget record */
    int offset;			/* Offset of colorMode field in record */
{
    ColorMode *modePtr = (ColorMode *)(widgRec + offset);
    unsigned int length;
    char c;

    c = value[0];
    length = strlen(value);
    if ((c == 'c') && (strncmp(value, "color", length) == 0)) {
	*modePtr = PS_MODE_COLOR;
    } else if ((c == 'g') && (strncmp(value, "grayscale", length) == 0)) {
	*modePtr = PS_MODE_GRAYSCALE;
    } else if ((c == 'g') && (strncmp(value, "greyscale", length) == 0)) {
	*modePtr = PS_MODE_GRAYSCALE;
    } else if ((c == 'm') && (strncmp(value, "monochrome", length) == 0)) {
	*modePtr = PS_MODE_MONOCHROME;
    } else {
	Tcl_AppendResult(interp, "bad color mode \"", value, "\": should be ",
	    "\"color\", \"grayscale\", or \"monochrome\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NameOfColorMode --
 *
 *	Convert the PostScript mode value into the string representing
 *	a valid color mode.
 *
 * Results:
 *	The static string representing the color mode is returned.
 *
 *----------------------------------------------------------------------
 */
static char *
NameOfColorMode(colorMode)
    ColorMode colorMode;
{
    switch (colorMode) {
    case PS_MODE_COLOR:
	return "color";
    case PS_MODE_GRAYSCALE:
	return "grayscale";
    case PS_MODE_MONOCHROME:
	return "monochrome";
    default:
	return "unknown color mode";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ColorModePrintProc --
 *
 *	Convert the current color mode into the string representing a
 *	valid color mode.
 *
 * Results:
 *	The string representing the color mode is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
ColorModePrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* PostScript structure record */
    int offset;			/* field of colorMode in record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    ColorMode mode = *(ColorMode *)(widgRec + offset);

    return (NameOfColorMode(mode));
}

/*
 *----------------------------------------------------------------------
 *
 * XColorToPostScript --
 *
 *	Convert the a XColor (from its RGB values) to a PostScript
 *	command.  If a Tcl color map variable exists, it will be
 *	consulted for a PostScript translation based upon the color
 *	name.
 *
 * Results:
 *	The string representing the color mode is returned.
 *
 *----------------------------------------------------------------------
 */
static void
XColorToPostScript(graphPtr, colorPtr, attr)
    Graph *graphPtr;
    XColor *colorPtr;		/* Color value to be converted */
    MonoAttribute attr;		/* If non-zero, this represents a foreground
				 * color, otherwise a background color */
{
    PostScript *psPtr = (PostScript *)graphPtr->postscript;
    double red, green, blue;

    /* If the color name exists in Tcl array variable, use that translation */
    if (psPtr->colorVarName != NULL) {
	char *colorDesc;

	colorDesc = Tcl_GetVar2(graphPtr->interp, psPtr->colorVarName,
	    Tk_NameOfColor(colorPtr), 0);
	if (colorDesc != NULL) {
	    Tcl_AppendResult(graphPtr->interp, colorDesc, " ", (char *)NULL);
	    return;
	}
    }
    /* Otherwise convert the X color pointer to its PostScript RGB values */
    red = colorPtr->red / 65535.0;
    green = colorPtr->green / 65535.0;
    blue = colorPtr->blue / 65535.0;
    sprintf(graphPtr->scratchArr, "%g %g %g %s  ", red, green, blue,
	(attr == PS_MONO_FOREGROUND) ? "SetFgColor" : "SetBgColor");
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * ReverseBits --
 *
 *	Convert a byte from a X image into PostScript image order.
 *	This requires not only the nybbles to be reversed but also
 *	their bit values.
 *
 * Results:
 *	The converted byte is returned.
 *
 *----------------------------------------------------------------------
 */
INLINE static unsigned char
ReverseBits(byte)
    register unsigned char byte;
{
    byte = ((byte >> 1) & 0x55) | ((byte << 1) & 0xaa);
    byte = ((byte >> 2) & 0x33) | ((byte << 2) & 0xcc);
    byte = ((byte >> 4) & 0x0f) | ((byte << 4) & 0xf0);
    return (byte);
}

/*
 * -------------------------------------------------------------------------
 *
 * XBitmapToPostScript --
 *
 *      Output a PostScript image string of the given bitmap image.
 *      It is assumed the image is one bit deep and a zero value
 *      indicates an off-pixel.  To convert to PostScript, the bits
 *      need to be reversed from the X11 image order.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      The PostScript image string is appended to interp->result.
 *
 * -------------------------------------------------------------------------
 */
static void
XBitmapToPostScript(graphPtr, bitmap, width, height)
    Graph *graphPtr;
    Pixmap bitmap;
    int width, height;
{
    register unsigned int byte = 0;
    register int x, y, bitPos;
    unsigned long pixel;
    int byteCount = 0;
#ifndef __WIN32__
    XImage *imagePtr;

    imagePtr = XGetImage(graphPtr->display, bitmap, 0, 0, width, height, 1,
	ZPixmap);
#else /* __WIN32__ */
#   undef   XGetPixel
#   define  XGetPixel(P,X,Y) GetPixel(P, X, Y)
    TkWinDCState    DCs;
    HDC imagePtr=TkWinGetDrawableDC(graphPtr->display, bitmap, &DCs);
#endif /* __WIN32__ */

    Tcl_AppendResult(graphPtr->interp, "<", (char *)NULL);
    bitPos = 0;			/* Suppress compiler warning */
    for (y = 0; y < height; y++) {
	byte = 0;
	for (x = 0; x < width; x++) {
	    pixel = XGetPixel(imagePtr, x, y);
	    bitPos = x % 8;
	    byte |= (unsigned char)(pixel << bitPos);
	    if (bitPos == 7) {
		sprintf(graphPtr->scratchArr, "%02x", ReverseBits(byte));
		Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr,
		    (char *)NULL);
		byteCount++;
		byte = 0;
	    }
	    if (byteCount >= 30) {
		Tcl_AppendResult(graphPtr->interp, "\n", (char *)NULL);
		byteCount = 0;
	    }
	}
	if (bitPos != 7) {
	    sprintf(graphPtr->scratchArr, "%02x", ReverseBits(byte));
	    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr,
		(char *)NULL);
	    byteCount++;
	}
    }
    Tcl_AppendResult(graphPtr->interp, "> ", (char *)NULL);
#ifndef __WIN32__
    XDestroyImage(imagePtr);
#else
#   undef XGetPixel
    TkWinReleaseDrawableDC(bitmap,imagePtr,&DCs);
#endif
}


/*
 * -------------------------------------------------------------------
 * Routines to convert X drawing functions to PostScript commands.
 * -------------------------------------------------------------------
 */
void
Blt_ClearBackgroundToPostScript(graphPtr)
    Graph *graphPtr;
{
    Tcl_AppendResult(graphPtr->interp, " 1.0 1.0 1.0 SetBgColor\n",
	(char *)NULL);
}

void
Blt_BackgroundToPostScript(graphPtr, colorPtr)
    Graph *graphPtr;
    XColor *colorPtr;
{
    XColorToPostScript(graphPtr, colorPtr, PS_MONO_BACKGROUND);
    Tcl_AppendResult(graphPtr->interp, "\n", (char *)NULL);
}

void
Blt_ForegroundToPostScript(graphPtr, colorPtr)
    Graph *graphPtr;
    XColor *colorPtr;
{
    XColorToPostScript(graphPtr, colorPtr, PS_MONO_FOREGROUND);
    Tcl_AppendResult(graphPtr->interp, "\n", (char *)NULL);
}

void
Blt_LineWidthToPostScript(graphPtr, lineWidth)
    Graph *graphPtr;
    int lineWidth;
{
    if (lineWidth < 1) {
	lineWidth = 1;
    }
    sprintf(graphPtr->scratchArr, "%d setlinewidth\n", lineWidth);
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
}

void
Blt_LineDashesToPostScript(graphPtr, dashesPtr)
    Graph *graphPtr;
    Dashes *dashesPtr;
{
    Tcl_AppendResult(graphPtr->interp, "[ ", (char *)NULL);
    if (dashesPtr != NULL) {
	register int i;

	for (i = 0; i < dashesPtr->numValues; i++) {
	    Blt_AppendInt(graphPtr->interp, (int)dashesPtr->valueList[i]);
	}
    }
    Tcl_AppendResult(graphPtr->interp, "] 0 setdash\n", (char *)NULL);
}

void
Blt_SetLineAttributes(graphPtr, colorPtr, lineWidth, dashesPtr)
    Graph *graphPtr;
    XColor *colorPtr;
    int lineWidth;
    Dashes *dashesPtr;
{
    Blt_ForegroundToPostScript(graphPtr, colorPtr);
    Blt_LineWidthToPostScript(graphPtr, lineWidth);
    Blt_LineDashesToPostScript(graphPtr, dashesPtr);
}

void
Blt_RectangleToPostScript(graphPtr, x, y, width, height)
    Graph *graphPtr;
    int x, y;
    int width, height;
{
    sprintf(graphPtr->scratchArr, "%d %d %d %d", x, y, width, height);
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, " Box Fill\n",
	(char *)NULL);
}

void
Blt_LinesToPostScript(graphPtr, pointArr, numPoints)
    Graph *graphPtr;
    XPoint *pointArr;
    int numPoints;
{
    register int i;

    sprintf(graphPtr->scratchArr, "newpath %d %d moveto\n", pointArr[0].x,
	pointArr[0].y);
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
    for (i = 1; i < numPoints; i++) {
	sprintf(graphPtr->scratchArr, "%d %d lineto\n", pointArr[i].x,
	    pointArr[i].y);
	Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
    }
}

void
Blt_PolygonToPostScript(graphPtr, pointArr, numPoints)
    Graph *graphPtr;
    XPoint *pointArr;
    int numPoints;
{
    Blt_LinesToPostScript(graphPtr, pointArr, numPoints);
    sprintf(graphPtr->scratchArr, "%d %d lineto closepath Fill\n",
	pointArr[0].x, pointArr[0].y);
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
}

void
Blt_SegmentsToPostScript(graphPtr, segArr, numSegments)
    Graph *graphPtr;
    XSegment *segArr;
    int numSegments;
{
    register int i;

    for (i = 0; i < numSegments; i++) {
	sprintf(graphPtr->scratchArr, "%d %d %d %d Segment\n",
	    segArr[i].x1, segArr[i].y1, segArr[i].x2, segArr[i].y2);
	Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
    }
}

#ifdef notdef
void
Blt_RectanglesToPostScript(graphPtr, rectArr, numRects)
    Graph *graphPtr;
    XRectangle rectArr[];
    int numRects;
{
    register int i;

    for (i = 0; i < numRects; i++) {
	Blt_RectangleToPostScript(graphPtr, rectArr[i].x, rectArr[i].y,
	    (int)rectArr[i].width, (int)rectArr[i].height);
    }
}
#endif

/*
 * The Border structure used internally by the Tk_3D* routines.
 * The following is a copy of it from tk3d.c.
 */

typedef struct {
    Screen *screen;		/* Screen on which the border will be used. */
    Visual *visual;		/* Visual for all windows and pixmaps using
				 * the border. */
    int depth;			/* Number of bits per pixel of drawables where
				 * the border will be used. */
    Colormap colormap;		/* Colormap out of which pixels are
				 * allocated. */
    int refCount;		/* Number of different users of
				 * this border.  */
    XColor *bgColorPtr;		/* Background color (intensity
				 * between lightColorPtr and
				 * darkColorPtr). */
    XColor *darkColorPtr;	/* Color for darker areas (must free when
				 * deleting structure). NULL means shadows
				 * haven't been allocated yet.*/
    XColor *lightColorPtr;	/* Color used for lighter areas of border
				 * (must free this when deleting structure).
				 * NULL means shadows haven't been allocated
				 * yet. */
    Pixmap shadow;		/* Stipple pattern to use for drawing
				 * shadows areas.  Used for displays with
				 * <= 64 colors or where colormap has filled
				 * up. */
    GC bgGC;			/* Used (if necessary) to draw areas in
				 * the background color. */
    GC darkGC;			/* Used to draw darker parts of the
				 * border. None means the shadow colors
				 * haven't been allocated yet.*/
    GC lightGC;			/* Used to draw lighter parts of
				 * the border. None means the shadow colors
				 * haven't been allocated yet. */
    Tcl_HashEntry *hashPtr;	/* Entry in borderTable (needed in
				 * order to delete structure). */
} Border;


void
Blt_Print3DRectangle(graphPtr, border, x, y, width, height, borderWidth,
    relief)
    Graph *graphPtr;		/* File pointer to write PS output. */
    Tk_3DBorder border;		/* Token for border to draw. */
    int x, y;			/* Coordinates of rectangle */
    int width, height;		/* Region to be drawn. */
    int borderWidth;		/* Desired width for border, in pixels. */
    int relief;			/* Should be either TK_RELIEF_RAISED or
                                 * TK_RELIEF_SUNKEN;  indicates position of
                                 * interior of window relative to exterior. */
{
    Border *borderPtr = (Border *)border;
    XColor lightColor, darkColor;
    XColor *lightColorPtr, *darkColorPtr;
    XColor *topColor, *bottomColor;
    XPoint points[7];
    int deall;
    Tk_Window tkwin = graphPtr->tkwin;
    int twiceWidth = (borderWidth * 2);

    if ((width < twiceWidth) || (height < twiceWidth)) {
	return;
    }
    if ((deall = (   (borderPtr->lightColorPtr == NULL)
		  || (borderPtr->darkColorPtr == NULL)))) {
	Screen *screenPtr;
#ifndef	__WIN32__
	Colormap colorMap;
#endif

	lightColor.pixel = borderPtr->bgColorPtr->pixel;
	screenPtr = Tk_Screen(graphPtr->tkwin);
	if (lightColor.pixel == WhitePixelOfScreen(screenPtr)) {
	    darkColor.pixel = BlackPixelOfScreen(screenPtr);
	} else {
	    darkColor.pixel = WhitePixelOfScreen(screenPtr);
	}
#ifndef	__WIN32__
	colorMap = borderPtr->colormap;
	XQueryColor(graphPtr->display, colorMap, &lightColor);
	lightColorPtr = &lightColor;
	XQueryColor(Tk_Display(tkwin), colorMap, &darkColor);
	darkColorPtr = &darkColor;
#else
	lightColorPtr = Tk_GetColorByValue(tkwin,&lightColor);
	darkColorPtr = Tk_GetColorByValue(tkwin,&darkColor);
#endif
    } else {
	lightColorPtr = borderPtr->lightColorPtr;
	darkColorPtr = borderPtr->darkColorPtr;
    }

    /*
     * Handle grooves and ridges with recursive calls.
     */

    if ((relief == TK_RELIEF_GROOVE) || (relief == TK_RELIEF_RIDGE)) {
	int halfWidth, insideOffset;

	halfWidth = borderWidth / 2;
	insideOffset = borderWidth - halfWidth;
	Blt_Print3DRectangle(graphPtr, border, x, y, width, height, halfWidth,
	    (relief == TK_RELIEF_GROOVE) ? TK_RELIEF_SUNKEN :
	    TK_RELIEF_RAISED);
	Blt_Print3DRectangle(graphPtr, border, x + insideOffset,
	    y + insideOffset, width - insideOffset * 2,
	    height - insideOffset * 2, halfWidth,
	    (relief == TK_RELIEF_GROOVE) ? TK_RELIEF_RAISED :
	    TK_RELIEF_SUNKEN);
	return;
    }
    if (relief == TK_RELIEF_RAISED) {
	topColor = lightColorPtr;
	bottomColor = darkColorPtr;
    } else if (relief == TK_RELIEF_SUNKEN) {
	topColor = darkColorPtr;
	bottomColor = lightColorPtr;
    } else {
	topColor = bottomColor = borderPtr->bgColorPtr;
    }
    Blt_BackgroundToPostScript(graphPtr, bottomColor);
    Blt_RectangleToPostScript(graphPtr, x, y + height - borderWidth,
	width, borderWidth);
    Blt_RectangleToPostScript(graphPtr, x + width - borderWidth, y,
	borderWidth, height);
    points[0].x = points[1].x = points[6].x = x;
    points[0].y = points[6].y = y + height;
    points[1].y = points[2].y = y;
    points[2].x = x + width;
    points[3].x = x + width - borderWidth;
    points[3].y = points[4].y = y + borderWidth;
    points[4].x = points[5].x = x + borderWidth;
    points[5].y = y + height - borderWidth;
    if (relief != TK_RELIEF_FLAT) {
	Blt_BackgroundToPostScript(graphPtr, topColor);
    }
    Blt_PolygonToPostScript(graphPtr, points, 7);

    if (deall) {
	Tk_FreeColor(lightColorPtr);
	Tk_FreeColor(darkColorPtr);
    }
}


void
Blt_3DRectangleToPostScript(graphPtr, border, x, y, width, height,
    borderWidth, relief)
    Graph *graphPtr;
    Tk_3DBorder border;		/* Token for border to draw. */
    int x, y;			/* Coordinates of top-left of border area */
    int width, height;		/* Dimension of border to be drawn. */
    int borderWidth;		/* Desired width for border, in pixels. */
    int relief;			/* Should be either TK_RELIEF_RAISED or
                                 * TK_RELIEF_SUNKEN;  indicates position of
                                 * interior of window relative to exterior. */
{
    Border *borderPtr = (Border *)border;

    /*
     * I'm assuming that the rectangle is to be drawn as a background.
     * Setting the pen color as foreground or background only affects
     * the plot when the colormode option is "monochrome".
     */
    Blt_BackgroundToPostScript(graphPtr, borderPtr->bgColorPtr);
    Blt_RectangleToPostScript(graphPtr, x, y, width, height);
    Blt_Print3DRectangle(graphPtr, border, x, y, width, height, borderWidth,
	relief);
}

void
Blt_StippleToPostScript(graphPtr, bitmap, width, height, fillOrStroke)
    Graph *graphPtr;
    Pixmap bitmap;
    int width, height;
    int fillOrStroke;
{
    sprintf(graphPtr->scratchArr, "%d %d\n", width, height);
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
    XBitmapToPostScript(graphPtr, bitmap, width, height);
    Tcl_AppendResult(graphPtr->interp, (fillOrStroke) ? "true" : "false",
	" StippleFill\n", (char *)NULL);
}

void
Blt_BitmapToPostScript(graphPtr, bitmap, centerX, centerY,
    width, height, theta, bgColorPtr)
    Graph *graphPtr;
    Pixmap bitmap;		/* Bitmap to be converted to PostScript */
    int centerX, centerY;	/* Bitmap's center coordinates */
    int width, height;		/* Extents of bitmap */
    double theta;		/* Degrees to rotate bitmap */
    XColor *bgColorPtr;		/* Background color of bitmap: NULL indicates
				 * no background color */
{
    if (bgColorPtr != NULL) {
	Tcl_AppendResult(graphPtr->interp, "{ ", (char *)NULL);
	XColorToPostScript(graphPtr, bgColorPtr, PS_MONO_BACKGROUND);
	Tcl_AppendResult(graphPtr->interp, "} true ", (char *)NULL);
    } else {
	Tcl_AppendResult(graphPtr->interp, "false ", (char *)NULL);
    }
    sprintf(graphPtr->scratchArr, "%d %d %d %d %g\n", centerX, centerY,
	width, height, theta);
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
    XBitmapToPostScript(graphPtr, bitmap, width, height);
    Tcl_AppendResult(graphPtr->interp, " DrawBitmap\n", (char *)NULL);
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_FontToPostScript --
 *
 *      Map the Tk font to a PostScript font and point size.
 *
 *	If a Tcl array variable was specified, each element should be
 *	indexed by the X11 font name and contain a list of 1-2
 *	elements; the PostScript font name and the desired point size.
 *	The point size may be omitted and the X font point size will
 *	be used.
 *
 *	Otherwise, if the foundry is "Adobe", we try to do a plausible
 *	mapping looking at the full name of the font and building a
 *	string in the form of "Family-TypeFace".
 *
 * Returns:
 *      None.
 *
 * Side Effects:
 *      PostScript commands are output to change the type and the
 *      point size of the current font.
 *
 * -----------------------------------------------------------------
 */
void
Blt_FontToPostScript(graphPtr, fontPtr)
    Graph *graphPtr;
    TkFont *fontPtr;		/* Tk font to query about */
{
    PostScript *psPtr = (PostScript *)graphPtr->postscript;
    char *fontName;
    int pointSize;
    Tcl_DString dstr;

    fontName = "Helvetica-Bold";/* Default font */
    pointSize = fontPtr->fa.pointsize;

    if (psPtr->fontVarName != NULL) {
	char *fontInfo;

	fontInfo = Tcl_GetVar2(graphPtr->interp, psPtr->fontVarName,
	    Tk_NameOfFont((Tk_Font)fontPtr), 0);
	if (fontInfo != NULL) {
	    int numProps;
	    char **propArr = NULL;

	    if (Tcl_SplitList(graphPtr->interp, fontInfo, &numProps,
		    &propArr) == TCL_OK) {
		fontName = propArr[0];
		if (numProps == 2) {
		    Tcl_GetInt(graphPtr->interp, propArr[1], &pointSize);
		}
	    }
	    sprintf(graphPtr->scratchArr, "%d /%s SetFont\n", pointSize,
		fontName);
	    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr,
		(char *)NULL);
	    if (propArr != (char **)NULL) {
		ckfree((char *)propArr);
	    }
	    return;
	}
    }
    /*
     * Otherwise, try to query the X font for its properties
     */
    Tcl_DStringInit(&dstr);
    pointSize = Tk_PostscriptFontName((Tk_Font)fontPtr, &dstr);
    sprintf(graphPtr->scratchArr, "%d /%s SetFont\n",
	pointSize, Tcl_DStringValue(&dstr));
    Tcl_DStringFree(&dstr);
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_PrintText --
 *
 *      Output PostScript commands to print a text string. The string
 *      may be rotated at any arbitrary angle, and placed according
 *      the anchor type given. The anchor indicates how to interpret
 *      the window coordinates as an anchor for the text bounding box.
 *      If a background color is specified (i.e. bgColorPtr != NULL),
 *      output a filled rectangle the size of the bounding box in the
 *      given background color.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Text string is drawn using the given font and GC on the graph
 *      window at the given coordinates, anchor, and rotation
 *
 * -----------------------------------------------------------------
 */
void
Blt_PrintText(graphPtr, text, attrPtr, x, y)
    Graph *graphPtr;
    char *text;			/* Text to convert to PostScript */
    TextAttributes *attrPtr;	/* Text attribute information */
    int x, y;			/* Window coordinates to draw text */
{
    Coordinate center;		/* Upper left corner of the text region */
    double theta;
    int width, height;
    int bbWidth, bbHeight;
    int lineHeight;
    register char *p;
    int length;

    if ((text == NULL) || (*text == '\0')) {	/* Empty string, do nothing */
	return;
    }
    theta = BLT_FMOD(attrPtr->theta, (double)360.0);
    Blt_GetTextExtents(attrPtr->fontPtr, text, &width, &height);
    attrPtr->regionWidth = width;
    Blt_GetBoundingBox(width, height, theta, &bbWidth, &bbHeight,
	(XPoint *)NULL);
    /*
     * Find the center of the bounding box
     */
    center = Blt_TranslateBoxCoords((double)x, (double)y, bbWidth, bbHeight, 
	attrPtr->anchor);
    center.x += (bbWidth * 0.5);
    center.y += (bbHeight * 0.5);

    /* Initialize text (sets translation and rotation) */
    sprintf(graphPtr->scratchArr, "%d %d %g %g %g BeginText\n", width, height,
	attrPtr->theta, center.x, center.y);
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);

    /* If a background is needed, draw a rectangle first before the text */
    if (attrPtr->bgColorPtr != (XColor *)NULL) {
	Blt_BackgroundToPostScript(graphPtr, attrPtr->bgColorPtr);
	Blt_RectangleToPostScript(graphPtr, 0, 0, width, height);
    }
    Blt_FontToPostScript(graphPtr, attrPtr->fontPtr);
    Blt_ForegroundToPostScript(graphPtr, attrPtr->fgColorPtr);
    y = attrPtr->fontPtr->fm.ascent;
    lineHeight = TEXTHEIGHT(attrPtr->fontPtr);
    length = 0;
    for (p = text; *p != '\0'; p++) {
	if (*p == '\n') {
	    if (length > 0) {
		Blt_PrintJustified(graphPtr->interp, graphPtr->scratchArr,
		    attrPtr, text, 0, y, length);
	    }
	    y += lineHeight;
	    text = p + 1;	/* Start the text on the next line */
	    length = 0;		/* Reset to indicate the start of a new line */
	    continue;
	}
	length++;
    }
    if ((length > 0) && (*(p - 1) != '\n')) {
	Blt_PrintJustified(graphPtr->interp, graphPtr->scratchArr, attrPtr,
	    text, 0, y, length);
    }
    /* End text mode */
    Tcl_AppendResult(graphPtr->interp, "EndText\n", (char *)NULL);
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_PrintLine --
 *
 *      Outputs PostScript commands to print a multi-segmented line.
 *      It assumes a procedure DashesProc was previously defined.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Line is printed.
 *
 * -----------------------------------------------------------------
 */
void
Blt_PrintLine(graphPtr, pointArr, numPoints)
    Graph *graphPtr;
    XPoint *pointArr;
    int numPoints;
{
    register int i;
    register XPoint *pointPtr;

    if (numPoints <= 0) {
	return;
    }
    pointPtr = pointArr;
    sprintf(graphPtr->scratchArr, " newpath %d %d moveto\n", pointPtr->x,
	pointPtr->y);
    pointPtr++;
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
    for (i = 1; i < (numPoints - 1); i++) {
	if (i % PS_MAXPATH) {
	    sprintf(graphPtr->scratchArr, " %d %d lineto\n", pointPtr->x,
		pointPtr->y);
	} else {
	    sprintf(graphPtr->scratchArr,
		" %d %d lineto\nDashesProc stroke\n newpath %d %d moveto\n",
		pointPtr->x, pointPtr->y, pointPtr->x, pointPtr->y);
	}
	Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
	pointPtr++;
    }
    /*
     * Note: It's assumed that i is numPoints - 1 after finishing loop
     */
    sprintf(graphPtr->scratchArr, " %d %d lineto\n DashesProc stroke\n",
	pointPtr->x, pointPtr->y);
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigurePS --
 *
 *      Configure the PostScript generation of the graph widget.
 *
 * Results:
 *      A standard TCL result.
 *
 * Side effects:
 *      A new PostScript file is created.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigurePS(interp, tkwin, psPtr, argc, argv, flags)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    PostScript *psPtr;
    int argc;			/* Number of options in argv vector */
    char **argv;		/* Option vector */
    int flags;
{
    if (Tk_ConfigureWidget(interp, tkwin, configSpecs, argc, argv,
	    (char *)psPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}


static void
DestroyPostScript(graphPtr)
    Graph *graphPtr;
{
    Tk_FreeOptions(configSpecs, (char *)graphPtr->postscript,
	graphPtr->display, 0);
    ckfree((char *)graphPtr->postscript);
}

/*
 *----------------------------------------------------------------------
 *
 * CgetOper --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    PostScript *psPtr = (PostScript *)graphPtr->postscript;

    if (Tk_ConfigureValue(interp, graphPtr->tkwin, configSpecs, (char *)psPtr,
	    argv[4], 0) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureOper --
 *
 *      This procedure is invoked to print the graph in a file.
 *
 * Results:
 *      A standard TCL result.
 *
 * Side effects:
 *      A new PostScript file is created.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;			/* Number of options in argv vector */
    char **argv;		/* Option vector */
{
    int flags = TK_CONFIG_ARGV_ONLY;
    PostScript *psPtr = (PostScript *)graphPtr->postscript;

    if (argc == 3) {
	return (Tk_ConfigureInfo(interp, graphPtr->tkwin, configSpecs,
		(char *)psPtr, (char *)NULL, flags));
    } else if (argc == 4) {
	return (Tk_ConfigureInfo(interp, graphPtr->tkwin, configSpecs,
		(char *)psPtr, argv[3], flags));
    }
    if (ConfigurePS(interp, graphPtr->tkwin, psPtr, argc - 3, argv + 3,
	    flags) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * --------------------------------------------------------------------------
 *
 * GetPageExtents --
 *
 * 	Calculate the bounding box required for the plot and its
 * 	padding.  First get the size of the plot (by default, it's the
 * 	size of graph's X window).  If the plot is bigger than the
 * 	designated paper size, or if the "-maxpect" option is turned
 * 	on, make the bounding box same size as the page.  The bounding
 * 	box will still maintain its padding, therefore the plot area
 * 	will grow or shrink.
 *
 * 	Since the values set are reliant upon the width and height of the
 *	graph, this must be called each time PostScript is generated.
 *
 * Results: None.
 *
 * Side Effects:
 *	graph->width and graph->height are set to the postscript plot
 *	Extents.
 *
 * --------------------------------------------------------------------------
 */
static void
GetPageExtents(graphPtr, pagePtr)
    Graph *graphPtr;
    PageExtents *pagePtr;
{
    PostScript *psPtr = (PostScript *)graphPtr->postscript;

    if (psPtr->reqWidth > 0) {
	graphPtr->width = psPtr->reqWidth;
    }
    if (psPtr->reqHeight > 0) {
	graphPtr->height = psPtr->reqHeight;
    }
    /*
     * Calculate dimension of bounding box (plot + padding).  The
     * bounding box is always aligned with the page, so correct for
     * orientation.
     */
    if (psPtr->landscape) {
	pagePtr->bbWidth = graphPtr->height;
	pagePtr->bbHeight = graphPtr->width;
    } else {
	pagePtr->bbWidth = graphPtr->width;
	pagePtr->bbHeight = graphPtr->height;
    }
    pagePtr->bbWidth += PADDING(psPtr->padX);
    pagePtr->bbHeight += PADDING(psPtr->padY);

    /*
     * Calculate the size of the page.  If no requested size was made
     * (i.e. the request is zero), default the paper size to the size
     * of the bounding box.
     */
    if (psPtr->reqPaperWidth > 0) {
	pagePtr->width = psPtr->reqPaperWidth;
    } else {
	pagePtr->width = pagePtr->bbWidth;
    }
    if (psPtr->reqPaperHeight > 0) {
	pagePtr->height = psPtr->reqPaperHeight;
    } else {
	pagePtr->height = pagePtr->bbHeight;
    }
    /*
     * Reset the size of the bounding box if it's bigger than the page.
     */
    if (pagePtr->bbHeight > pagePtr->height) {
	if (psPtr->landscape) {
	    graphPtr->width = pagePtr->bbHeight - PADDING(psPtr->padY);
	} else {
	    graphPtr->height = pagePtr->bbHeight - PADDING(psPtr->padY);
	}
	pagePtr->bbHeight = pagePtr->height;
    }
    if (pagePtr->bbWidth > pagePtr->width) {
	if (psPtr->landscape) {
	    graphPtr->height = pagePtr->bbWidth - PADDING(psPtr->padX);
	} else {
	    graphPtr->width = pagePtr->bbWidth - PADDING(psPtr->padX);
	}
	pagePtr->bbWidth = pagePtr->width;
    }
    pagePtr->maxScale = 1.0;
    if (psPtr->maxpect) {
	double xScale, yScale, scale;
	int borderX, borderY;
	int plotWidth, plotHeight;

	borderX = PADDING(psPtr->padX);
	borderY = PADDING(psPtr->padY);
	plotWidth = (pagePtr->bbWidth - borderX);
	plotHeight = (pagePtr->bbHeight - borderY);
	xScale = (double)(pagePtr->width - borderX) / plotWidth;
	yScale = (double)(pagePtr->height - borderY) / plotHeight;
	scale = BLT_MIN(xScale, yScale);
	pagePtr->bbWidth = (int)(plotWidth * scale + 0.5) + borderX;
	pagePtr->bbHeight = (int)(plotHeight * scale + 0.5) + borderY;
	pagePtr->maxScale = scale;
    }
    pagePtr->adjustX = pagePtr->adjustY = 0;
    if (psPtr->center) {
	if (pagePtr->width > pagePtr->bbWidth) {
	    pagePtr->adjustX = (pagePtr->width - pagePtr->bbWidth) / 2;
	}
	if (pagePtr->height > pagePtr->bbHeight) {
	    pagePtr->adjustY = (pagePtr->height - pagePtr->bbHeight) / 2;
	}
    }
}

/*
 * --------------------------------------------------------------------------
 *
 * PrintPreamble
 *
 *    	The PostScript preamble calculates the needed translation and scaling
 *    	to make X11 coordinates compatible with PostScript.
 *
 * ---------------------------------------------------------------------
 */

#ifdef TIME_WITH_SYS_TIME
#   include <sys/time.h>
#   include <time.h>
#else
#   ifdef HAVE_SYS_TIME_H
#	include <sys/time.h>
#   else
#	include <time.h>
#   endif /* HAVE_SYS_TIME_H */
#endif /* TIME_WITH_SYS_TIME */

static int
PrintPreamble(graphPtr, pagePtr, fileName)
    Graph *graphPtr;
    PageExtents *pagePtr;
    char *fileName;
{
    PostScript *psPtr = (PostScript *)graphPtr->postscript;
    long date;
    char *version;
    double llx, lly, urx, ury;
    double dpiX, dpiY;
    double yScale, xScale;	/* Scales to convert pixels to pica */
    Screen *screenPtr;
#ifdef NO_INLINE_PS_PROLOG
    Tcl_DString prologPath;
    char *libDir;
    FILE *f;
#endif /* NO_INLINE_PS_PROLOG */

    if (fileName == NULL) {
	fileName = Tk_PathName(graphPtr->tkwin);
    }
    Tcl_AppendResult(graphPtr->interp,
	"%!PS-Adobe-3.0 EPSF-3.0\n%%Pages: 1\n",
	"%%Title: (", fileName, ")\n",
	"%%DocumentNeededResources: font Helvetica Courier\n",
	(char *)NULL);
    /*
     * Compute the scale factors to convert PostScript to X11 coordinates.
     * Round the pixels per inch (dpi) to an integral value before computing
     * the scale.
     */
#define MM_INCH (double)25.4
#define PICA_INCH (double)72.0
    screenPtr = Tk_Screen(graphPtr->tkwin);
    dpiX = (WidthOfScreen(screenPtr) * MM_INCH) / WidthMMOfScreen(screenPtr);
    dpiY = (HeightOfScreen(screenPtr) * MM_INCH) / HeightMMOfScreen(screenPtr);
    xScale = PICA_INCH / BLT_RND(dpiX);
    yScale = PICA_INCH / BLT_RND(dpiY);

    /*
     * Calculate the bounding box for the plot.  The bounding box
     * contains the graph and any designated padding (-padx or -pady
     * options).  Compute the lower left and upper right coordinates
     * of the bounding box.
     */
    llx = (double)(pagePtr->adjustX) * xScale;
    lly = (double)(pagePtr->adjustY) * yScale;
    urx = (double)(pagePtr->adjustX + pagePtr->bbWidth) * xScale;
    ury = (double)(pagePtr->adjustY + pagePtr->bbHeight) * yScale;
    sprintf(graphPtr->scratchArr, "%%%%BoundingBox: %5.0f %5.0f %5.0f %5.0f\n",
	llx, lly, urx, ury);

    version = Tcl_GetVar2(graphPtr->interp, "blt_versions", "graph",
	TCL_GLOBAL_ONLY);
    if (version == NULL) {
	version = "???";
    }
    date = time((time_t *) NULL);
    Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr,
	"%%Creator: ", Tk_Class(graphPtr->tkwin), " (version ", version, ")\n",
	"%%CreationDate: ", ctime(&date),
	"%%EndComments\n", (char *)NULL);

#ifndef NO_INLINE_PS_PROLOG
    {	int i;

	/*
	 *  Just copy the lines from the variable containing the prologue
	 *  to the interpreter's result variable.
	 */
	for (i = 0; postScriptPrologue[i]; i++) {
	    Tcl_AppendResult(graphPtr->interp, postScriptPrologue[i],
		(char *)NULL);
	}
    }
#else /* NO_INLINE_PS_PROLOG */
    /*
     * Read a standard prolog file from file and append it to the
     * generated PostScript output stored in interp->result.
     */
    libDir = Tcl_GetVar(graphPtr->interp, "blt_library", TCL_GLOBAL_ONLY);
    if (libDir == NULL) {
	Tcl_ResetResult(graphPtr->interp);
	Tcl_AppendResult(graphPtr->interp, "couldn't find BLT script library:",
	    "global variable \"blt_library\" doesn't exist", (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_DStringInit(&prologPath);
    Tcl_DStringAppend(&prologPath, libDir, -1);
    Tcl_DStringAppend(&prologPath, "/bltGraph.pro", -1);
    f = fopen(Tcl_DStringValue(&prologPath), "r");
    if (f == NULL) {
	Tcl_ResetResult(graphPtr->interp);
	Tcl_AppendResult(graphPtr->interp, "couldn't open prologue file \"",
	    Tcl_DStringValue(&prologPath), "\": ",
	    Tcl_PosixError(graphPtr->interp), (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_AppendResult(graphPtr->interp, "\n% including file \"",
	Tcl_DStringValue(&prologPath), "\"\n\n", (char *)NULL);
    while (fgets(graphPtr->scratchArr, SCRATCH_LENGTH, f) != NULL) {
	Tcl_AppendResult(graphPtr->interp, graphPtr->scratchArr, (char *)NULL);
    }
    if (ferror(f)) {
	fclose(f);
	Tcl_ResetResult(graphPtr->interp);
	Tcl_AppendResult(graphPtr->interp, "error reading prologue file \"",
	    Tcl_DStringValue(&prologPath), "\": ",
	    Tcl_PosixError(graphPtr->interp), (char *)NULL);
	Tcl_DStringFree(&prologPath);
	return TCL_ERROR;
    }
    Tcl_DStringFree(&prologPath);
    fclose(f);
#endif /* NO_INLINE_PS_PROLOG */

    /*
     * Set the conversion from PostScript to X11 coordinates.  Scale
     * pica to pixels and flip the y-axis (the origin is the upperleft
     * corner).
     */
    sprintf(graphPtr->scratchArr, "%f -%f scale\n0 %d translate\n\n",
	xScale, yScale, -pagePtr->height);
    Tcl_AppendResult(graphPtr->interp,
	"% Transform coordinate system to use X11 coordinates\n\n",
	"% Flip the y-axis by changing the origin and reversing the scale,\n",
	"% making the origin to the upper left corner\n",
	graphPtr->scratchArr,
	"% User defined page layout\n\n",
	(char *)NULL);
    sprintf(graphPtr->scratchArr, "/CL %d def\n\n", psPtr->colorMode);
    Tcl_AppendResult(graphPtr->interp,
	"% Set color level\n",
	graphPtr->scratchArr,
	(char *)NULL);
    sprintf(graphPtr->scratchArr, "%d %d translate\n\n",
	pagePtr->adjustX + psPtr->padLeft,
	pagePtr->adjustY + psPtr->padTop);
    Tcl_AppendResult(graphPtr->interp,
	"% Set origin\n",
	graphPtr->scratchArr,
	(char *)NULL);
    if (psPtr->landscape) {
	sprintf(graphPtr->scratchArr, "0 %g translate\n-90 rotate\n",
	    (double)(graphPtr->width * pagePtr->maxScale));
	Tcl_AppendResult(graphPtr->interp, "% Landscape orientation\n",
	    graphPtr->scratchArr, (char *)NULL);
    }
    if (psPtr->maxpect) {
	sprintf(graphPtr->scratchArr, "%g %g scale\n", pagePtr->maxScale,
	    pagePtr->maxScale);
	Tcl_AppendResult(graphPtr->interp,
	    "% Set to max aspect ratio\n",
	    graphPtr->scratchArr,
	    (char *)NULL);
    }
    Tcl_AppendResult(graphPtr->interp, "%%%%EndSetup\n\n", (char *)NULL);
    return TCL_OK;
}


static void
PrintExterior(graphPtr, psPtr)
    Graph *graphPtr;
    PostScript *psPtr;
{
    register int i;
    XRectangle rectArr[4];
    TextAttributes textAttr;
    GraphAxis *axisPtr;

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

    /* Clear the surrounding margins and clip the plotting surface */
    if (psPtr->decorations) {
	Blt_BackgroundToPostScript(graphPtr,
	    Tk_3DBorderColor(graphPtr->border));
    } else {
	Blt_ClearBackgroundToPostScript(graphPtr);
    }
    for (i = 0; i < 4; i++) {
	Blt_RectangleToPostScript(graphPtr, rectArr[i].x, rectArr[i].y,
	    (int)rectArr[i].width, (int)rectArr[i].height);
    }
    /* Interior 3D border */
    if ((psPtr->decorations) && (graphPtr->plotBW > 0)) {
	int x, y, width, height;

	x = graphPtr->plotArea.x - graphPtr->plotBW;
	y = graphPtr->plotArea.y - graphPtr->plotBW;
	width = graphPtr->plotArea.width + (2 * graphPtr->plotBW);
	height = graphPtr->plotArea.height + (2 * graphPtr->plotBW);
	Blt_Print3DRectangle(graphPtr, graphPtr->border, x, y, width, height,
	    graphPtr->plotBW, graphPtr->plotRelief);
    }
    if (Blt_LegendSite(graphPtr) < LEGEND_SITE_PLOT) {
	/*
	 * Print the legend if we're using a site which lies in one
	 * of the margins (left, right, top, or bottom) of the graph.
	 */
	(*graphPtr->legendPtr->printProc) (graphPtr);
    }
    Blt_InitTextAttrs(&textAttr, graphPtr->marginFg, (XColor *)NULL,
	graphPtr->fontPtr, 0.0, TK_ANCHOR_CENTER, graphPtr->justify);

    if (graphPtr->title != NULL) {
	Blt_PrintText(graphPtr, graphPtr->title, &textAttr, graphPtr->titleX,
	    graphPtr->titleY);
    }
    textAttr.fontPtr = graphPtr->fontPtr;
    for (i = 0; i < 4; i++) {	/* Print axes */
	axisPtr = graphPtr->axisArr[i];
	if (axisPtr->mapped) {
	    (*axisPtr->printProc) (axisPtr, &textAttr);
	}
    }
}


/*
 *----------------------------------------------------------------------
 *
 * OutputOper --
 *
 *      This procedure is invoked to print the graph in a file.
 *
 * Results:
 *      Standard TCL result.  TCL_OK if plot was successfully printed,
 *	TCL_ERROR otherwise.
 *
 * Side effects:
 *      A new PostScript file is created.
 *
 *----------------------------------------------------------------------
 */
static int
OutputOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget record */
    Tcl_Interp *interp;
    int argc;			/* Number of options in argv vector */
    char **argv;		/* Option vector */
{
    PostScript *psPtr = (PostScript *)graphPtr->postscript;
    GraphLegend *legendPtr = graphPtr->legendPtr;
    int x, y, width, height;
    int result = TCL_ERROR;
    PageExtents pageExtents;
    LegendSite site;
    char scratchSpace[SCRATCH_LENGTH + 1];
    FILE *f = NULL;
    char *fileName;		/* Name of file to write PostScript output
                                 * If NULL, output is returned via
                                 * interp->result. */

    if (graphPtr->flags & RESET_AXES) {
	Blt_ComputeAxes(graphPtr);
    }
    fileName = NULL;
    graphPtr->scratchArr = scratchSpace;
    if (argc > 3) {
	if (argv[3][0] != '-') {
	    fileName = argv[3];	/* First argument is the file name. */
	    argv++, argc--;
	}
	if (ConfigurePS(interp, graphPtr->tkwin, psPtr, argc - 3, argv + 3,
		TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (fileName != NULL) {
	    f = fopen(fileName, "w");
	    if (f == NULL) {
		Tcl_AppendResult(interp, "can't create \"", fileName, "\": ",
		    Tcl_PosixError(interp), (char *)NULL);
		return TCL_ERROR;
	    }
	}
    }
    GetPageExtents(graphPtr, &pageExtents);
    Blt_LayoutGraph(graphPtr);
    graphPtr->flags |= COORDS_WORLD;
    Blt_GetGraphCoordinates(graphPtr);
    Tcl_ResetResult(interp);

    result = PrintPreamble(graphPtr, &pageExtents, fileName);
    if (result != TCL_OK) {
	goto error;
    }
    /*
     * Determine rectangle of the plotting area for the graph window
     */
    x = graphPtr->plotArea.x - graphPtr->plotBW;
    y = graphPtr->plotArea.y - graphPtr->plotBW;
    width = graphPtr->plotArea.width + (2 * graphPtr->plotBW);
    height = graphPtr->plotArea.height + (2 * graphPtr->plotBW);

    Blt_FontToPostScript(graphPtr, graphPtr->fontPtr);
    if (psPtr->decorations) {
	Blt_BackgroundToPostScript(graphPtr, graphPtr->plotBg);
    } else {
	Blt_ClearBackgroundToPostScript(graphPtr);
    }
    Blt_RectangleToPostScript(graphPtr, x, y, width, height);
    Tcl_AppendResult(interp, "gsave clip\n\n", (char *)NULL);
    /*
     * Draw the grid, elements, and markers in the interior of the graph
     * (plotting surface).
     */
    site = Blt_LegendSite(graphPtr);

    if ((site >= LEGEND_SITE_PLOT) && (!legendPtr->raised)) {
	/* Draw legend underneath elements and markers */
	(*legendPtr->drawProc) (graphPtr);
    }
    if (graphPtr->gridPtr->mapped) {
	(*graphPtr->gridPtr->printProc) (graphPtr);
    }
    Blt_PrintMarkers(graphPtr, TRUE);
    Blt_PrintElements(graphPtr);
    if ((site >= LEGEND_SITE_PLOT) && (legendPtr->raised)) {
	/* Draw legend above elements (but not markers) */
	(*legendPtr->drawProc) (graphPtr);
    }
    Blt_PrintMarkers(graphPtr, FALSE);
    Blt_PrintActiveElements(graphPtr);
    Tcl_AppendResult(interp, "\n% Unset clipping\ngrestore\n\n", (char *)NULL);
    PrintExterior(graphPtr, psPtr);
    Tcl_AppendResult(interp, "showpage\n%Trailer\ngrestore\nend\n%EOF\n",
	(char *)NULL);
    /*
     * If a file name was given, write the results to that file
     */
    if (f != NULL) {
	fputs(Tcl_GetStringResult(interp), f);
	Tcl_ResetResult(interp);
	if (ferror(f)) {
	    Tcl_AppendResult(interp, "error writing file \"", fileName, "\": ",
		Tcl_PosixError(interp), (char *)NULL);
	    goto error;
	}
    }
    result = TCL_OK;

  error:
    if (f != NULL) {
	fclose(f);
    }
    /* Reset height and width of graph window */
    graphPtr->width = Tk_Width(graphPtr->tkwin);
    graphPtr->height = Tk_Height(graphPtr->tkwin);
    graphPtr->flags = COORDS_WORLD;
    /*
     * Redraw the graph in order to re-calculate the layout as soon as
     * possible. This is in the case the crosshairs are active.
     */
    Blt_RedrawGraph(graphPtr);
    return (result);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_CreatePostScript --
 *
 *      Creates a postscript structure.
 *
 * Results:
 *      Always TCL_OK.
 *
 * Side effects:
 *      A new PostScript structure is created.
 *
 *----------------------------------------------------------------------
 */
int
Blt_CreatePostScript(graphPtr)
    Graph *graphPtr;
{
    PostScript *psPtr;

    psPtr = (PostScript *)ckcalloc(1, sizeof(PostScript));
    if (psPtr == NULL) {
	Panic("can't allocate postscript structure");
    }
    psPtr->colorMode = PS_MODE_COLOR;
    psPtr->destroyProc = (PostScriptDestroyProc*)DestroyPostScript;
    psPtr->center = TRUE;
    psPtr->decorations = TRUE;
    graphPtr->postscript = (GraphPostScript *)psPtr;
    return (ConfigurePS(graphPtr->interp, graphPtr->tkwin, psPtr, 0,
	    (char **)NULL, 0));
}

/*
 *--------------------------------------------------------------
 *
 * Blt_PostScriptOper --
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
    {"cget", 2, (Blt_OperProc) CgetOper, 4, 4, "option",},
    {"configure", 2, (Blt_OperProc) ConfigureOper, 2, 0, "?option value?...",},
    {"output", 1, (Blt_OperProc) OutputOper, 2, 0,
	"?fileName? ?option value?...",},
};

static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

int
Blt_PostScriptOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget record */
    Tcl_Interp *interp;
    int argc;			/* # arguments */
    char **argv;		/* Argument list */
{
    Blt_OperProc proc;
    int result;

    proc = Blt_LookupOperation(interp, numSpecs, operSpecs, BLT_OPER_ARG2,
	argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (graphPtr, interp, argc, argv);
    return (result);
}
#endif /* !NO_GRAPH */
