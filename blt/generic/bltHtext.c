/*
 * bltHtext.c --
 *
 *	This module implements a hypertext widget for the
 *	Tk toolkit.
 *
 * Copyright 1991-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * To do:
 *
 * 1) Fix scroll unit round off errors.
 *
 * 2) Better error checking.
 *
 * 3) Use html format.
 *
 * 4) The dimension of cavities using -relwidth and -relheight
 *    should be 0 when computing initial estimates for the size
 *    of the virtual text.
 */

#include "bltInt.h"
#ifndef	NO_HTEXT

#include <ctype.h>
#include <sys/stat.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define HTEXT_VERSION  "8.0"

#define DEF_LINES_ALLOC 512	/* Default block of lines allocated */

/*
 * Justify option values
 */
typedef enum {
    JUSTIFY_CENTER, JUSTIFY_TOP, JUSTIFY_BOTTOM
} Justify;

extern Tk_CustomOption bltFillOption;
extern Tk_CustomOption bltPadOption;
extern Tk_CustomOption bltLengthOption;
extern Tk_CustomOption bltTileOption;

static int WidthParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int flags));
static int HeightParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int flags));
static char *WidthHeightPrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset, Tcl_FreeProc **freeProc));

static Tk_CustomOption widthOption =
{
    WidthParseProc, WidthHeightPrintProc, (ClientData)0
};

static Tk_CustomOption heightOption =
{
    HeightParseProc, WidthHeightPrintProc, (ClientData)0
};

static int JustifyParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *JustifyPrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset, Tcl_FreeProc **freeProcPtr));

static Tk_CustomOption justifyOption =
{
    JustifyParseProc, JustifyPrintProc, (ClientData)0
};

static void SlaveGeometryProc _ANSI_ARGS_((ClientData, Tk_Window));
static void SlaveCustodyProc _ANSI_ARGS_((ClientData, Tk_Window));

static Tk_GeomMgr htextMgrInfo =
{
    "htext",			/* Name of geometry manager used by winfo */
    SlaveGeometryProc,		/* Procedure to for new geometry requests */
    SlaveCustodyProc,		/* Procedure when window is taken away */
};

typedef struct Slave {
    struct HText *textPtr;	/* Pointer to parent's Htext structure */
    Tk_Window tkwin;		/* Widget window */
    int flags;

    int x, y;			/* Origin of slave window in text */

    /* Dimensions of the cavity surrounding the slave window */

    int cavityWidth, cavityHeight;

    /*
     *  Dimensions of the slave window.  Compared against actual
     *	slave window sizes when checking for resizing.
     */
    int winWidth, winHeight;

    int precedingTextEnd;	/* Index (in charArr) of the the last character
				 * immediatedly preceding the slave window */
    int precedingTextWidth;	/* Width of normal text preceding slave */

    Tk_Anchor anchor;
    Justify justify;		/* Justification of region wrt to line */


    /*
     * Requested dimensions of the cavity (includes padding). If non-zero,
     * it overrides the calculated dimension of the cavity.
     */
    int reqCavityWidth, reqCavityHeight;

    /*
     * Relative dimensions of cavity wrt the size of the viewport. If
     * greater than 0.0.
     */
    double relCavityWidth, relCavityHeight;

    /*
     * If non-zero, overrides the requested dimension of the slave window
     */
    int reqSlaveWidth, reqSlaveHeight;

    /*
     * Relative dimensions of slave window wrt the size of the viewport
     */
    double relSlaveWidth, relSlaveHeight;

    Pad padX, padY;		/* Extra padding to frame around */
    int ipadX, ipadY;		/* internal padding for window */

    Fill fill;			/* Fill style flag */

} Slave;

/*
 * Flag bits slaves windows:
 */
#define SLAVE_VISIBLE	(1<<2)	/* Slave window is currently visible in the
				 * viewport. */
#define SLAVE_NOT_CHILD	(1<<3)	/* Slave window is not a child of hypertext
				 * widget */
/*
 * Defaults for slaves:
 */
#define DEF_SLAVE_ANCHOR        "center"
#define DEF_SLAVE_FILL		"none"
#define DEF_SLAVE_HEIGHT	"0"
#define DEF_SLAVE_JUSTIFY	"center"
#define DEF_SLAVE_PAD_X		"0"
#define DEF_SLAVE_PAD_Y		"0"
#define DEF_SLAVE_REL_HEIGHT	"0.0"
#define DEF_SLAVE_REL_WIDTH  	"0.0"
#define DEF_SLAVE_WIDTH  	"0"

static Tk_ConfigSpec slaveConfigSpecs[] =
{
    {TK_CONFIG_ANCHOR, "-anchor", (char *)NULL, (char *)NULL,
	DEF_SLAVE_ANCHOR, Tk_Offset(Slave, anchor),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-fill", (char *)NULL, (char *)NULL,
	DEF_SLAVE_FILL, Tk_Offset(Slave, fill),
	TK_CONFIG_DONT_SET_DEFAULT, &bltFillOption},
    {TK_CONFIG_CUSTOM, "-cavityheight", (char *)NULL, (char *)NULL,
	DEF_SLAVE_HEIGHT, Tk_Offset(Slave, reqCavityHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-cavitywidth", (char *)NULL, (char *)NULL,
	DEF_SLAVE_WIDTH, Tk_Offset(Slave, reqCavityWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-height", (char *)NULL, (char *)NULL,
	DEF_SLAVE_HEIGHT, Tk_Offset(Slave, reqSlaveHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-justify", (char *)NULL, (char *)NULL,
	DEF_SLAVE_JUSTIFY, Tk_Offset(Slave, justify),
	TK_CONFIG_DONT_SET_DEFAULT, &justifyOption},
    {TK_CONFIG_CUSTOM, "-padx", (char *)NULL, (char *)NULL,
	DEF_SLAVE_PAD_X, Tk_Offset(Slave, padX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", (char *)NULL, (char *)NULL,
	DEF_SLAVE_PAD_Y, Tk_Offset(Slave, padY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_DOUBLE, "-relcavityheight", (char *)NULL, (char *)NULL,
	DEF_SLAVE_REL_HEIGHT, Tk_Offset(Slave, relCavityHeight),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_DOUBLE, "-relcavitywidth", (char *)NULL, (char *)NULL,
	DEF_SLAVE_REL_WIDTH, Tk_Offset(Slave, relCavityWidth),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_DOUBLE, "-relheight", (char *)NULL, (char *)NULL,
	DEF_SLAVE_REL_HEIGHT, Tk_Offset(Slave, relSlaveHeight),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_DOUBLE, "-relwidth", (char *)NULL, (char *)NULL,
	DEF_SLAVE_REL_WIDTH, Tk_Offset(Slave, relSlaveWidth),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-width", (char *)NULL, (char *)NULL,
	DEF_SLAVE_WIDTH, Tk_Offset(Slave, reqSlaveWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

/*
 * Line --
 *
 *	Structure to contain the contents of a single line of text and
 *	the slaves on that line.
 *
 * 	Individual lines are not configurable, although changes to the
 * 	size of slaves do effect its values.
 */
typedef struct {
    int offset;			/* Offset of line from y-origin (0) in
				 * world coordinates */
    int baseline;		/* Baseline y-coordinate of the text */
    short int width, height;	/* Dimensions of the line */
    int textStart, textEnd;	/* Start and end indices of characters
				 * forming the line in the text array */
    Blt_List winList;		/* List of slave windows on the line of text */
} Line;

typedef struct {
    int textStart;
    int textEnd;
} TextSegment;

typedef struct {
    int width, height;
} Dimension;

typedef struct {
    int x, y;
} Position;

/*
 * Hypertext widget.
 */
typedef struct HText {
    Tk_Window tkwin;		/* Window that embodies the widget.
                                 * NULL means that the window has been
                                 * destroyed but the data structures
                                 * haven't yet been cleaned up.*/
    Display *display;		/* Display containing widget; needed,
                                 * among other things, to release
                                 * resources after tkwin has already
                                 * gone away. */
    Tcl_Interp *interp;		/* Interpreter associated with widget. */

    Tcl_Command cmdToken;	/* Token for htext's widget command. */

    int flags;

    /* User-configurable fields */

    XColor *normalFg, *normalBg;
    TkFont *fontPtr;		/* Font for normal text. May affect the size
				 * of the viewport if the width/height is
				 * specified in columns/rows */
    GC drawGC;			/* Graphics context for normal text */
    Blt_Tile tile;
    int tileOffsetPage;		/* Set tile offset to top of page instead
				 * of toplevel window */
    GC fillGC;			/* GC for clearing the window in the
				 * designated background color. The
				 * background color is the foreground
				 * attribute in GC.  */

    int numRows, numColumns;	/* # of characters of the current font
				 * for a row or column of the viewport.
				 * Used to determine the width and height
				 * of the text window (i.e. viewport) */
    int reqWidth, reqHeight;	/* Requested dimensions of the viewport */
    int maxWidth, maxHeight;	/* Maximum dimensions allowed for the viewport,
				 * regardless of the size of the text */

    Tk_Cursor cursor;		/* X Cursor */

    char *fileName;		/* If non-NULL, indicates the name of a
				 * hypertext file to be read into the widget.
				 * If NULL, the *text* field is considered
				 * instead */
    char *text;			/* Hypertext to be loaded into the widget. This
				 * value is ignored if *fileName* is non-NULL */
    int specChar;		/* Special character designating a TCL
			         * command block in a hypertext file. */
    int lineSpacing;		/* # of pixels between lines */

    char *yScrollCmd;		/* Name of vertical scrollbar to invoke */
    int scrollY;		/* # of pixels per vertical scroll */
    char *xScrollCmd;		/* Name of horizontal scroll bar to invoke */
    int scrollX;		/* # of pixels per horizontal scroll */

    int reqLineNum;		/* Line requested by "goto" command */

    /*
     * The view port is the width and height of the window and the
     * origin of the viewport (upper left corner) in world coordinates.
     */
    Dimension virtText;		/* Size of view text in world coordinates */
    Position viewOrigin;	/* Position of viewport in virt. coordinates */

    int pendingX, pendingY;	/* New upper-left corner (origin) of
				 * the viewport (not yet posted) */

    int first, last;		/* Range of lines displayed */

    int lastWidth, lastHeight;
    /* Last known size of the window: saved to
				 * recognize when the viewport is resized. */

    Tcl_HashTable slaves;	/* Table of slave windows */

    /*
     * Selection display information:
     */
    Tk_3DBorder selectBorder;	/* Border and background color */
    int selectBW;		/* Border width */
    XColor *selectFg;		/* Text foreground color */
    GC selectGC;		/* GC for drawing selected text */
    int selectAnchor;		/* Fixed end of selection
			         * (i.e. "selection to" operation will
			         * use this as one end of the selection).*/
    int selectFirst;		/* The index of first character in the
				 * text array selected */
    int selectLast;		/* The index of the last character selected */
    int exportSelection;	/* Non-zero means to export the internal text
				 * selection to the X server. */
    char *takeFocus;

    /*
     * Scanning information:
     */
    XPoint scanMark;		/* Anchor position of scan */
    XPoint scanPt;		/* x,y position where the scan started. */

    char *charArr;		/* Pool of characters representing the text
				 * to be displayed */
    int numChars;		/* Length of the text pool */

    Line *lineArr;		/* Array of pointers to text lines */
    int numLines;		/* # of line entered into array. */
    int arraySize;		/* Size of array allocated. */

} HText;

/*
 * Bit flags for the hypertext widget:
 */
#define REDRAW_PENDING	 (1<<0)	/* A DoWhenIdle handler has already
				 * been queued to redraw the window */
#define IGNORE_EXPOSURES (1<<1)	/* Ignore exposure events in the text
				 * window.  Potentially many expose
				 * events can occur while rearranging
				 * slave windows during a single call to
				 * the DisplayText.  */

#define REQUEST_LAYOUT 	(1<<4)	/* Something has happened which
				 * requires the layout of text and
				 * slave window positions to be
				 * recalculated.  The following
				 * actions may cause this:
				 *
				 * 1) the contents of the hypertext
				 *    has changed by either the -file or
				 *    -text options.
				 *
				 * 2) a text attribute has changed
				 *    (line spacing, font, etc)
				 *
				 * 3) a slave window has been resized or
				 *    moved.
				 *
				 * 4) a slave configuration option has
				 *    changed.
				 */
#define TEXT_DIRTY 	(1<<5)	/* The layout was recalculated and the
				 * size of the world (text layout) has
				 * changed. */
#define GOTO_PENDING 	(1<<6)	/* Indicates the starting text line
				 * number has changed. To be reflected
				 * the next time the widget is redrawn. */
#define SLAVE_APPENDED	(1<<7)	/* Indicates a slave window has just
				 * been appended to the text.  This is
				 * used to determine when to add a
				 * space to the text array */

#define DEF_HTEXT_BG_COLOR		STD_NORMAL_BG_COLOR
#define DEF_HTEXT_BG_MONO		STD_NORMAL_BG_MONO
#define DEF_HTEXT_CURSOR		"pencil"
#define DEF_HTEXT_EXPORT_SELECTION	"1"

#define DEF_HTEXT_FG_COLOR		STD_NORMAL_FG_COLOR
#define DEF_HTEXT_FG_MONO		STD_NORMAL_FG_MONO
#define DEF_HTEXT_FILE_NAME		(char *)NULL
#define DEF_HTEXT_FONT			STD_FONT
#define DEF_HTEXT_HEIGHT		"0"
#define DEF_HTEXT_LINE_SPACING		"1"
#define DEF_HTEXT_MAX_HEIGHT		(char *)NULL
#define DEF_HTEXT_MAX_WIDTH 		(char *)NULL
#define DEF_HTEXT_SCROLL_UNITS		"10"
#define DEF_HTEXT_SPEC_CHAR		"0x25"
#define DEF_HTEXT_SELECT_BORDER_WIDTH 	STD_SELECT_BORDERWIDTH
#define DEF_HTEXT_SELECT_BG_COLOR 	STD_SELECT_BG_COLOR
#define DEF_HTEXT_SELECT_BG_MONO  	STD_SELECT_BG_MONO
#define DEF_HTEXT_SELECT_FG_COLOR 	STD_SELECT_FG_COLOR
#define DEF_HTEXT_SELECT_FG_MONO  	STD_SELECT_FG_MONO
#define DEF_HTEXT_TAKE_FOCUS		(char *)NULL
#define DEF_HTEXT_TEXT			(char *)NULL
#define DEF_HTEXT_TILE_OFFSET		"1"
#define DEF_HTEXT_WIDTH			"0"

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_HTEXT_BG_COLOR, Tk_Offset(HText, normalBg), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_HTEXT_BG_MONO, Tk_Offset(HText, normalBg), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_HTEXT_CURSOR, Tk_Offset(HText, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-exportselection", "exportSelection","ExportSelection",
	DEF_HTEXT_EXPORT_SELECTION, Tk_Offset(HText, exportSelection), 0},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *)NULL, (char *)NULL, 0, 0},
    {TK_CONFIG_STRING, "-file", "file", "File",
	DEF_HTEXT_FILE_NAME, Tk_Offset(HText, fileName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_HTEXT_FONT, Tk_Offset(HText, fontPtr), 0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_HTEXT_FG_COLOR, Tk_Offset(HText, normalFg), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_HTEXT_FG_MONO, Tk_Offset(HText, normalFg), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-height", "height", "Height",
	DEF_HTEXT_HEIGHT, Tk_Offset(HText, reqHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &heightOption},
    {TK_CONFIG_CUSTOM, "-linespacing", "lineSpacing", "LineSpacing",
	DEF_HTEXT_LINE_SPACING, Tk_Offset(HText, lineSpacing),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-maxheight", "maxHeight", "MaxHeight",
	DEF_HTEXT_MAX_HEIGHT, Tk_Offset(HText, maxHeight),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-maxwidth", "maxWidth", "MaxWidth",
	DEF_HTEXT_MAX_WIDTH, Tk_Offset(HText, maxWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Background",
	DEF_HTEXT_SELECT_BG_MONO, Tk_Offset(HText, selectBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Background",
	DEF_HTEXT_SELECT_BG_COLOR, Tk_Offset(HText, selectBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_CUSTOM, "-selectborderwidth", "selectBorderWidth", "BorderWidth",
	DEF_HTEXT_SELECT_BORDER_WIDTH, Tk_Offset(HText, selectBW),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Foreground",
	DEF_HTEXT_SELECT_FG_MONO, Tk_Offset(HText, selectFg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Foreground",
	DEF_HTEXT_SELECT_FG_COLOR, Tk_Offset(HText, selectFg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_INT, "-specialchar", "specialChar", "SpecialChar",
	DEF_HTEXT_SPEC_CHAR, Tk_Offset(HText, specChar), 0},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_HTEXT_TAKE_FOCUS, Tk_Offset(HText, takeFocus),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-tile", "tile", "Tile",
	(char *)NULL, Tk_Offset(HText, tile), TK_CONFIG_NULL_OK, 
	&bltTileOption},
    {TK_CONFIG_BOOLEAN, "-tileoffset", "tileOffset", "TileOffset",
	DEF_HTEXT_TILE_OFFSET, Tk_Offset(HText, tileOffsetPage), 0},
    {TK_CONFIG_STRING, "-text", "text", "Text",
	DEF_HTEXT_TEXT, Tk_Offset(HText, text), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-width", "width", "Width",
	DEF_HTEXT_WIDTH, Tk_Offset(HText, reqWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &widthOption},
    {TK_CONFIG_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
	(char *)NULL, Tk_Offset(HText, xScrollCmd), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-xscrollunits", "xScrollUnits", "ScrollUnits",
	DEF_HTEXT_SCROLL_UNITS, Tk_Offset(HText, scrollX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
	(char *)NULL, Tk_Offset(HText, yScrollCmd), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-yscrollunits", "yScrollUnits", "yScrollUnits",
	DEF_HTEXT_SCROLL_UNITS, Tk_Offset(HText, scrollY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

/* Forward Declarations */
static void DestroyText _ANSI_ARGS_((FreeProcData freeProcData));
static void SlaveGeometryProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));
static void SlaveEventProc _ANSI_ARGS_((ClientData clientdata,
	XEvent *eventPtr));
static void DisplayText _ANSI_ARGS_((ClientData clientData));

static void TextDeleteCmdProc _ANSI_ARGS_((ClientData clientdata));
static void TileChangedProc _ANSI_ARGS_((ClientData clientData, Blt_Tile tile,
	GC *gcPtr));

/* end of Forward Declarations */


 /* Custom options */
/*
 *----------------------------------------------------------------------
 *
 * JustifyParseProc --
 *
 * 	Converts the justification string into its numeric
 * 	representation. This configuration option affects how the
 *	slave window is positioned with respect to the line on which
 *	it sits.
 *
 *	Valid style strings are:
 *
 *	"top"      Uppermost point of region is top of the line's
 *		   text
 * 	"center"   Center point of region is line's baseline.
 *	"bottom"   Lowermost point of region is bottom of the
 *		   line's text
 *
 * Returns:
 *	A standard Tcl result.  If the value was not valid
 *
 *---------------------------------------------------------------------- */
/*ARGSUSED*/
static int
JustifyParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* Justification string */
    char *widgRec;		/* Structure record */
    int offset;			/* Offset of justify in record */
{
    Justify *justPtr = (Justify *)(widgRec + offset);
    unsigned int length;
    char c;

    c = value[0];
    length = strlen(value);
    if ((c == 'c') && (strncmp(value, "center", length) == 0)) {
	*justPtr = JUSTIFY_CENTER;
    } else if ((c == 't') && (strncmp(value, "top", length) == 0)) {
	*justPtr = JUSTIFY_TOP;
    } else if ((c == 'b') && (strncmp(value, "bottom", length) == 0)) {
	*justPtr = JUSTIFY_BOTTOM;
    } else {
	Tcl_AppendResult(interp, "bad justification argument \"", value,
	    "\": should be \"center\", \"top\", or \"bottom\"", (char *)NULL);
	return TCL_ERROR;
    }
    return (TCL_OK);
}

/*
 *----------------------------------------------------------------------
 *
 * NameOfJustify --
 *
 *	Returns the justification style string based upon the value.
 *
 * Results:
 *	The static justification style string is returned.
 *
 *----------------------------------------------------------------------
 */
static char *
NameOfJustify(justify)
    Justify justify;
{
    switch (justify) {
    case JUSTIFY_CENTER:
	return "center";
    case JUSTIFY_TOP:
	return "top";
    case JUSTIFY_BOTTOM:
	return "bottom";
    default:
	return "unknown justification value";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * JustifyPrintProc --
 *
 *	Returns the justification style string based upon the value.
 *
 * Results:
 *	The justification style string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
JustifyPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Structure record */
    int offset;			/* Offset of justify record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    Justify justify = *(Justify *)(widgRec + offset);

    return (NameOfJustify(justify));
}

/*
 *----------------------------------------------------------------------
 *
 * GetScreenDistance --
 *
 *	Converts the given string into the screen distance or number
 *	of characters.  The valid formats are
 *
 *	    N	- pixels	Nm - millimeters
 *	    Ni  - inches        Np - pica
 *          Nc  - centimeters   N# - number of characters
 *
 *	where N is a non-negative decimal number.
 *
 * Results:
 *	A standard Tcl result.  The screen distance and the number of
 *	characters are returned.  If the string can't be converted,
 *	TCL_ERROR is returned and interp->result will contain an error
 *	message.
 *
 *----------------------------------------------------------------------
 */
static int
GetScreenDistance(interp, tkwin, string, sizePtr, countPtr)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    char *string;
    int *sizePtr;
    int *countPtr;
{
    int numPixels, numChars;
    char *endPtr;		/* Pointer to last character scanned */
    double value;
    int rounded;

    value = strtod(string, &endPtr);
    if (endPtr == string) {
	Tcl_AppendResult(interp, "bad screen distance \"", string, "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    if (value < 0.0) {
	Tcl_AppendResult(interp, "screen distance \"", string,
	    "\" must be non-negative value", (char *)NULL);
	return TCL_ERROR;
    }
    while (isspace(*endPtr)) {
	if (*endPtr == '\0') {
	    break;
	}
	endPtr++;
    }
    numPixels = numChars = 0;
    rounded = BLT_RND(value);
    switch (*endPtr) {
    case '\0':			/* Distance in pixels */
	numPixels = rounded;
	break;
    case '#':			/* Number of characters */
	numChars = rounded;
	break;
    default:			/* cm, mm, pica, inches */
	if (Tk_GetPixels(interp, tkwin, string, &rounded) != TCL_OK) {
	    return TCL_ERROR;
	}
	numPixels = rounded;
	break;
    }
    *sizePtr = numPixels;
    *countPtr = numChars;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * HeightParseProc --
 *
 *	Like TK_CONFIG_PIXELS, but adds an extra check for negative
 *	values.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
HeightParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window */
    char *value;		/* Pixel value string */
    char *widgRec;		/* Widget record */
    int offset;			/* not used */
{
    HText *textPtr = (HText *)widgRec;
    int height, numRows;

    if (GetScreenDistance(interp, tkwin, value, &height, &numRows) != TCL_OK) {
	return TCL_ERROR;
    }
    textPtr->numRows = numRows;
    textPtr->reqHeight = height;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WidthParseProc --
 *
 *	Like TK_CONFIG_PIXELS, but adds an extra check for negative
 *	values.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
WidthParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window */
    char *value;		/* Pixel value string */
    char *widgRec;		/* Widget record */
    int offset;			/* not used */
{
    HText *textPtr = (HText *)widgRec;
    int width, numColumns;

    if (GetScreenDistance(interp, tkwin, value, &width, &numColumns) != TCL_OK) {
	return TCL_ERROR;
    }
    textPtr->numColumns = numColumns;
    textPtr->reqWidth = width;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WidthHeightPrintProc --
 *
 *	Returns the string representing the positive pixel size.
 *
 * Results:
 *	The pixel size string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
WidthHeightPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Row/column structure record */
    int offset;			/* Offset of fill in Partition record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    int pixels = *(int *)(widgRec + offset);
    char *resultPtr;
    char string[200];

    sprintf(string, "%d", pixels);
    resultPtr = ckstrdup(string);
    if (resultPtr == NULL) {
	return (strerror(errno));
    }
    *freeProcPtr = (Tcl_FreeProc *)BLT_ckfree;
    return (resultPtr);
}

/* General routines */
/*
 *----------------------------------------------------------------------
 *
 * EventuallyRedraw --
 *
 *	Queues a request to redraw the text window at the next idle
 *	point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets redisplayed.  Right now we don't do selective
 *	redisplays:  the whole window will be redrawn.  This doesn't
 *	seem to hurt performance noticeably, but if it does then this
 *	could be changed.
 *
 *----------------------------------------------------------------------
 */
static void
EventuallyRedraw(textPtr)
    HText *textPtr;		/* Information about widget. */
{
    if ((textPtr->tkwin != NULL) && !(textPtr->flags & REDRAW_PENDING)) {
	textPtr->flags |= REDRAW_PENDING;
	Tcl_DoWhenIdle(DisplayText, (ClientData)textPtr);
    }
}

/*
 * --------------------------------------------------------------------
 *
 * ResizeArray --
 *
 *	Reallocates memory to the new size given.  New memory
 *	is also cleared (zeros).
 *
 * Results:
 *	Returns a pointer to the new object or NULL if an error occurred.
 *
 * Side Effects:
 *	Memory is re/allocated.
 *
 * --------------------------------------------------------------------
 */
static int
ResizeArray(arrayPtr, elemSize, newSize, prevSize)
    char **arrayPtr;
    int elemSize;
    int newSize;
    int prevSize;
{
    char *newPtr;

    if (newSize == prevSize) {
	return TCL_OK;
    }
    if (newSize == 0) {		/* Free entire array */
	ckfree(*arrayPtr);
	*arrayPtr = NULL;
	return TCL_OK;
    }
    newPtr = (char *)ckcalloc(elemSize, newSize);
    if (newPtr == NULL) {
	return TCL_ERROR;
    }
    if ((prevSize > 0) && (*arrayPtr != NULL)) {
	int size;

	size = BLT_MIN(prevSize, newSize) * elemSize;
	if (size > 0) {
	    memcpy(newPtr, *arrayPtr, size);
	}
	ckfree(*arrayPtr);
    }
    *arrayPtr = newPtr;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * LineSearch --
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
LineSearch(textPtr, yCoord, low, high)
    HText *textPtr;		/* HText widget */
    int yCoord;			/* Search y-coordinate  */
    int low, high;		/* Range of lines to search */
{
    int median;
    Line *linePtr;

    while (low <= high) {
	median = (low + high) >> 1;
	linePtr = textPtr->lineArr + median;
	if (yCoord < linePtr->offset) {
	    high = median - 1;
	} else if (yCoord >= (linePtr->offset + linePtr->height)) {
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
 * IndexSearch --
 *
 *	Try to find what line contains a given text index. Performs
 *	a binary search for the text line which contains the given index.
 *	The search is inclusive of those lines from low and high.
 *
 * Results:
 *	Returns the line number containing the given index. If the index
 *	is outside the range of lines, -1 is returned.
 *
 * ----------------------------------------------------------------------
 */
static int
IndexSearch(textPtr, textIndex, low, high)
    HText *textPtr;		/* HText widget */
    int textIndex;		/* Search index */
    int low, high;		/* Range of lines to search */
{
    int median;
    Line *linePtr;

    while (low <= high) {
	median = (low + high) >> 1;
	linePtr = textPtr->lineArr + median;
	if (   (linePtr->textEnd == (linePtr->textStart-1))
	    && (textIndex == linePtr->textStart)) {
	    return(median);
	}

	if (textIndex < linePtr->textStart) {
	    high = median - 1;
	} else if (textIndex > linePtr->textEnd) {
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
 * GetXYPosIndex --
 *
 * 	Converts a string in the form "@x,y", where x and y are
 *	window coordinates, to a text index.
 *
 *	Window coordinates are first translated into world coordinates.
 *	Any coordinate outside of the bounds of the virtual text is
 *	silently set the nearest boundary.
 *
 * Results:
 *	A standard Tcl result.  If "string" is a valid index, then
 *	*indexPtr is filled with the numeric index corresponding.
 *	Otherwise an error message is left in interp->result.
 *
 * ----------------------------------------------------------------------
 */
static int
GetXYPosIndex(textPtr, string, indexPtr)
    HText *textPtr;
    char *string;
    int *indexPtr;
{
    int x, y, curX, dummy;
    int textLength, textStart;
    int charPos, lineNum;
    Line *linePtr;
    XPoint winPos;

    if (Blt_GetXYPosition(textPtr->interp, string, &winPos) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Locate the line corresponding to the window y-coordinate position */

    y = winPos.y + textPtr->viewOrigin.y;
    if (y < 0) {
	lineNum = textPtr->first;
    } else if (y >= textPtr->virtText.height) {
	lineNum = textPtr->last;
    } else {
	lineNum = LineSearch(textPtr, y, 0, textPtr->numLines - 1);
    }
    if (lineNum < 0) {
	Tcl_AppendResult(textPtr->interp, "can't find line at \"", string, "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    x = winPos.x + textPtr->viewOrigin.x;
    if (x < 0) {
	x = 0;
    } else if (x > textPtr->virtText.width) {
	x = textPtr->virtText.width;
    }
    linePtr = textPtr->lineArr + lineNum;
    curX = 0;
    textStart = linePtr->textStart;
    textLength = linePtr->textEnd - linePtr->textStart;
    if (linePtr->winList.numEntries > 0) {
	Blt_ListItem *itemPtr;
	int deltaX;
	Slave *slavePtr;

	for (itemPtr = Blt_FirstListItem(&(linePtr->winList));
	    itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	    slavePtr = (Slave *)Blt_GetItemValue(itemPtr);
	    deltaX = slavePtr->precedingTextWidth + slavePtr->cavityWidth;
	    if ((curX + deltaX) > x) {
		textLength = (slavePtr->precedingTextEnd - textStart);
		break;
	    }
	    curX += deltaX;
	    /*
	     * Skip over the trailing space. It designates the position of
	     * a slave window in the text
	     */
	    textStart = slavePtr->precedingTextEnd + 1;
	}
    }
    charPos = Tk_MeasureChars((Tk_Font)textPtr->fontPtr,
        textPtr->charArr + textStart, textLength, x, NO_FLAGS, &dummy);
    *indexPtr = textStart + charPos;

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ParseIndex --
 *
 *	Parse a string representing a text index into numeric
 *	value.  A text index can be in one of the following forms.
 *
 *	  "anchor"	- anchor position of the selection.
 *	  "sel.first"   - index of the first character in the selection.
 *	  "sel.last"	- index of the last character in the selection.
 *	  "page.top"  	- index of the first character on the page.
 *	  "page.bottom"	- index of the last character on the page.
 *	  "@x,y"	- x and y are window coordinates.
 * 	  "number	- raw index of text
 *	  "line.char"	- line number and character position
 *
 * Results:
 *	A standard Tcl result.  If "string" is a valid index, then
 *	*indexPtr is filled with the corresponding numeric index.
 *	Otherwise an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */
static int
ParseIndex(textPtr, string, indexPtr)
    HText *textPtr;		/* Text for which the index is being
				 * specified. */
    char *string;		/* Numerical index into textPtr's element
				 * list, or "end" to refer to last element. */
    int *indexPtr;		/* Where to store converted relief. */
{
    unsigned int length;
    char c;
    Tcl_Interp *interp = textPtr->interp;

    length = strlen(string);
    c = string[0];

    if ((c == 'a') && (strncmp(string, "anchor", length) == 0)) {
	*indexPtr = textPtr->selectAnchor;
    } else if ((c == 's') && (length > 4)) {
	if (strncmp(string, "sel.first", length) == 0) {
	    *indexPtr = textPtr->selectFirst;
	} else if (strncmp(string, "sel.last", length) == 0) {
	    *indexPtr = textPtr->selectLast;
	} else {
	    goto badIndex;	/* Not a valid index */
	}
	if (*indexPtr < 0) {
	    Tcl_AppendResult(interp, "bad index \"", string,
		"\": nothing selected in \"",
		Tk_PathName(textPtr->tkwin), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
    } else if ((c == 'p') && (length > 5) &&
	(strncmp(string, "page.top", length) == 0)) {
	int first;

	first = textPtr->first;
	if (first < 0) {
	    first = 0;
	}
	*indexPtr = textPtr->lineArr[first].textStart;
    } else if ((c == 'p') && (length > 5) &&
	(strncmp(string, "page.bottom", length) == 0)) {
	*indexPtr = textPtr->lineArr[textPtr->last].textEnd;
    } else if (c == '@') {	/* Screen position */
	if (GetXYPosIndex(textPtr, string, indexPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {	
	char *period;

	period = strchr(string, '.');
	if (period == NULL) {	/* Raw index */
	    int textIndex;

	    if ((string[0] == 'e') && (strcmp(string, "end") == 0)) {
		textIndex = textPtr->numChars - 1;
	    } else if (Tcl_GetInt(interp, string, &textIndex) != TCL_OK) {
		goto badIndex;
	    }
	    if (textIndex < 0) {
		textIndex = 0;
	    } else if (textIndex > (textPtr->numChars - 1)) {
		textIndex = textPtr->numChars - 1;
	    }
	    *indexPtr = textIndex;
	} else {
	    int lineNum, charPos, offset;
	    Line *linePtr;
	    int result;

	    *period = '\0';
	    result = TCL_OK;
	    if ((string[0] == 'e') && (strcmp(string, "end") == 0)) {
		lineNum = textPtr->numLines - 1;
	    } else {
		result = Tcl_GetInt(interp, string, &lineNum);
	    }
	    *period = '.';	/* Repair index string before returning */
	    if (result != TCL_OK) {
		goto badIndex;	/* Bad line number */
	    }
	    if (lineNum < 0) {
		lineNum = 0;	/* Silently repair bad line numbers */
	    }
	    if (textPtr->numChars == 0) {
		*indexPtr = 0;
		return TCL_OK;
	    }
	    if (lineNum >= textPtr->numLines) {
		lineNum = textPtr->numLines - 1;
	    }
	    linePtr = textPtr->lineArr + lineNum;
	    charPos = 0;
	    if ((*(period + 1) != '\0')) {
		string = period + 1;
		if ((string[0] == 'e') && (strcmp(string, "end") == 0)) {
		    charPos = linePtr->textEnd - linePtr->textStart;
		} else if (Tcl_GetInt(interp, string, &charPos) != TCL_OK) {
		    goto badIndex;
		}
	    }
	    if (charPos < 0) {
		charPos = 0;	/* Silently fix bogus indices */
	    }
	    offset = 0;
	    if (textPtr->numChars > 0) {
		offset = linePtr->textStart + charPos;
		if (offset > linePtr->textEnd) {
		    offset = linePtr->textEnd;
		}
	    }
	    *indexPtr = offset;
	}
    }
    if (textPtr->numChars == 0) {
	*indexPtr = 0;
    }
    return TCL_OK;

  badIndex:

    /*
     * Some of the paths here leave messages in interp->result, so we
     * have to clear it out before storing our own message.
     */
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "invalid index \"", string, "\": should be ",
        "one of the following: anchor, sel.first, sel.last, page.bottom, ",
	"page.top, @x,y, index, line.char", (char *)NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * GetIndex --
 *
 *	Get the index from a string representing a text index.
 *
 *
 * Results:
 *	A standard Tcl result.  If "string" is a valid index, then
 *	*indexPtr is filled with the numeric index corresponding.
 *	Otherwise an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */
static int
GetIndex(textPtr, string, indexPtr)
    HText *textPtr;		/* Text for which the index is being
				 * specified. */
    char *string;		/* Numerical index into textPtr's element
				 * list, or "end" to refer to last element. */
    int *indexPtr;		/* Where to store converted relief. */
{
    int textIndex;

    if (ParseIndex(textPtr, string, &textIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    *indexPtr = textIndex;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetTextPosition --
 *
 * 	Performs a binary search for the index located on line in
 *	the text. The search is limited to those lines between
 *	low and high inclusive.
 *
 * Results:
 *	Returns the line number at the given Y coordinate. If position
 *	does not correspond to any of the lines in the given the set,
 *	-1 is returned.
 *
 * ----------------------------------------------------------------------
 */
static int
GetTextPosition(textPtr, textIndex, lineNumPtr, charPosPtr)
    HText *textPtr;
    int textIndex;
    int *lineNumPtr;
    int *charPosPtr;
{
    int lineNum, charPos;

    lineNum = charPos = 0;
    if (textPtr->numChars > 0) {
	Line *linePtr;

	lineNum = IndexSearch(textPtr, textIndex, 0, textPtr->numLines - 1);
	if (lineNum < 0) {
	    char string[200];

	    sprintf(string, "can't determine line number from index \"%d\"",
		textIndex);
	    Tcl_AppendResult(textPtr->interp, string, (char *)NULL);
	    return TCL_ERROR;
	}
	linePtr = textPtr->lineArr + lineNum;
	if (textIndex > linePtr->textEnd) {
	    textIndex = linePtr->textEnd;
	}
	charPos = textIndex - linePtr->textStart;
    }
    *lineNumPtr = lineNum;
    *charPosPtr = charPos;
    return TCL_OK;
}

/* Slave Procedures */
/*
 *----------------------------------------------------------------------
 *
 * GetSlaveWidth --
 *
 *	Returns the width requested by the slave window. The requested
 *	space also includes any internal padding which has been designated
 *	for this window.
 *
 * Results:
 *	Returns the requested width of the slave window.
 *
 *----------------------------------------------------------------------
 */
static int
GetSlaveWidth(slavePtr)
    Slave *slavePtr;
{
    int width;

    if (slavePtr->reqSlaveWidth > 0) {
	width = slavePtr->reqSlaveWidth;
    } else if (slavePtr->relSlaveWidth > 0.0) {
	width = (int)((double)Tk_Width(slavePtr->textPtr->tkwin) *
	    slavePtr->relSlaveWidth + 0.5);
    } else {
	width = Tk_ReqWidth(slavePtr->tkwin);
    }
    width += (2 * slavePtr->ipadX);
    return (width);
}

/*
 *----------------------------------------------------------------------
 *
 * GetSlaveHeight --
 *
 *	Returns the height requested by the slave window. The requested
 *	space also includes any internal padding which has been designated
 *	for this window.
 *
 * Results:
 *	Returns the requested height of the slave window.
 *
 *----------------------------------------------------------------------
 */
static int
GetSlaveHeight(slavePtr)
    Slave *slavePtr;
{
    int height;

    if (slavePtr->reqSlaveHeight > 0) {
	height = slavePtr->reqSlaveHeight;
    } else if (slavePtr->relSlaveHeight > 0.0) {
	height = (int)((double)Tk_Height(slavePtr->textPtr->tkwin) *
	    slavePtr->relSlaveHeight + 0.5);
    } else {
	height = Tk_ReqHeight(slavePtr->tkwin);
    }
    height += (2 * slavePtr->ipadY);
    return (height);
}

/*
 * --------------------------------------------------------------
 *
 * SlaveEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on hypertext widgets.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 * --------------------------------------------------------------
 */
static void
SlaveEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about the slave window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Slave *slavePtr = (Slave *)clientData;
    HText *textPtr;

    if ((slavePtr == NULL) || (slavePtr->tkwin == NULL)) {
	return;
    }
    textPtr = slavePtr->textPtr;

    if (eventPtr->type == DestroyNotify) {
	/*
	 * Mark the slave as deleted by dereferencing the Tk window
	 * pointer.  Zero out the height and width to collapse the area
	 * used by the slave.  Redraw the window only if the slave is
	 * currently visible and mapped.
	 */
	slavePtr->textPtr->flags |= REQUEST_LAYOUT;
	if (Tk_IsMapped(slavePtr->tkwin) && (slavePtr->flags & SLAVE_VISIBLE)) {
	    EventuallyRedraw(textPtr);
	}
	Tk_DeleteEventHandler(slavePtr->tkwin, StructureNotifyMask,
	    SlaveEventProc, (ClientData)slavePtr);
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&(textPtr->slaves),
		(char *)slavePtr->tkwin));
	slavePtr->cavityWidth = slavePtr->cavityHeight = 0;
	slavePtr->tkwin = NULL;

    } else if (eventPtr->type == ConfigureNotify) {
	/*
	 * Slaves can't request new positions. Worry only about resizing.
	 */
	if (slavePtr->winWidth != Tk_Width(slavePtr->tkwin) ||
	    slavePtr->winHeight != Tk_Height(slavePtr->tkwin)) {
	    EventuallyRedraw(textPtr);
	    textPtr->flags |= REQUEST_LAYOUT;
	}
    }
}


/*
 *--------------------------------------------------------------
 *
 * SlaveCustodyProc --
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
 *--------------------------------------------------------------
 */
 /* ARGSUSED */
static void
SlaveCustodyProc(clientData, tkwin)
    ClientData clientData;	/* Information about the former slave window. */
    Tk_Window tkwin;		/* Not used. */
{
    Slave *slavePtr = (Slave *)clientData;
    /*
     * Mark the slave as deleted by dereferencing the Tk window
     * pointer.  Zero out the height and width to collapse the area
     * used by the slave.  Redraw the window only if the slave is
     * currently visible and mapped.
     */
    slavePtr->textPtr->flags |= REQUEST_LAYOUT;
    if (Tk_IsMapped(slavePtr->tkwin) && (slavePtr->flags & SLAVE_VISIBLE)) {
	EventuallyRedraw(slavePtr->textPtr);
    }
    Tk_DeleteEventHandler(slavePtr->tkwin, StructureNotifyMask,
	SlaveEventProc, (ClientData)slavePtr);
    Tcl_DeleteHashEntry(Tcl_FindHashEntry(&(slavePtr->textPtr->slaves),
	    (char *)slavePtr->tkwin));
    slavePtr->cavityWidth = slavePtr->cavityHeight = 0;
    slavePtr->tkwin = NULL;
}


/*
 *--------------------------------------------------------------
 *
 * SlaveGeometryProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for
 *	slave windows managed by the hypertext widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for tkwin, and all its managed siblings, to
 *	be repacked and drawn at the next idle point.
 *
 *--------------------------------------------------------------
 */
 /* ARGSUSED */
static void
SlaveGeometryProc(clientData, tkwin)
    ClientData clientData;	/* Information about window that got new
			         * preferred geometry.  */
    Tk_Window tkwin;		/* Other Tk-related information about the
			         * window. */
{
    Slave *slavePtr = (Slave *)clientData;

    slavePtr->textPtr->flags |= REQUEST_LAYOUT;
    EventuallyRedraw(slavePtr->textPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * FindSlave --
 *
 *	Searches for a slave widget matching the path name given
 *	If found, the pointer to the slave structure is returned,
 *	otherwise NULL.
 *
 * Results:
 *	The pointer to the slave structure. If not found, NULL.
 *
 * ----------------------------------------------------------------------
 */
static Slave *
FindSlave(textPtr, tkwin)
    HText *textPtr;		/* Hypertext widget structure */
    Tk_Window tkwin;		/* Path name of slave window  */
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&(textPtr->slaves), (char *)tkwin);
    if (hPtr != NULL) {
	return (Slave *) Tcl_GetHashValue(hPtr);
    }
    return NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateSlave --
 *
 * 	This procedure creates and initializes a new slave window
 *	in the hyper text widget.
 *
 * Results:
 *	The return value is a pointer to a structure describing the
 *	new slave.  If an error occurred, then the return value is
 *      NULL and an error message is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated. Slave window is mapped. Callbacks are set
 *	up for slave window resizes and geometry requests.
 *
 * ----------------------------------------------------------------------
 */
static Slave *
CreateSlave(textPtr, name)
    HText *textPtr;		/* Hypertext widget */
    char *name;			/* Name of slave window */
{
    Slave *slavePtr;
    Tk_Window tkwin;
    Tcl_HashEntry *hPtr;
    int isNew;

    tkwin = Tk_NameToWindow(textPtr->interp, name, textPtr->tkwin);
    if (tkwin == NULL) {
	return NULL;
    }
    if (Tk_Parent(tkwin) != textPtr->tkwin) {
	Tcl_AppendResult(textPtr->interp, "parent window of \"", name,
	    "\" must be \"", Tk_PathName(textPtr->tkwin), "\"", (char *)NULL);
	return NULL;
    }
    hPtr = Tcl_CreateHashEntry(&(textPtr->slaves), (char *)tkwin, &isNew);
    /* Check is the window is already a slave of this widget */
    if (!isNew) {
	Tcl_AppendResult(textPtr->interp, "\"", name,
	    "\" is already appended to ", Tk_PathName(textPtr->tkwin),
	    (char *)NULL);
	return NULL;
    }
    slavePtr = (Slave *)ckcalloc(1, sizeof(Slave));
    if (slavePtr == NULL) {
	Panic("can't allocate hypertext slave structure");
    }
    slavePtr->flags = 0;
    slavePtr->tkwin = tkwin;
    slavePtr->textPtr = textPtr;
    slavePtr->x = slavePtr->y = 0;
    slavePtr->fill = FILL_NONE;
    slavePtr->justify = JUSTIFY_CENTER;
    slavePtr->anchor = TK_ANCHOR_CENTER;
    Tcl_SetHashValue(hPtr, (ClientData)slavePtr);

    Tk_ManageGeometry(tkwin, &htextMgrInfo, (ClientData)slavePtr);
    Tk_CreateEventHandler(tkwin, StructureNotifyMask, SlaveEventProc,
	(ClientData)slavePtr);
    return (slavePtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroySlave --
 *
 * 	This procedure is invoked by DestroyLine to clean up the
 * 	internal structure of a slave.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroySlave(slavePtr)
    Slave *slavePtr;
{
    /* Destroy the slave window if it still exists */
    if (slavePtr->tkwin != NULL) {
	Tcl_HashEntry *hPtr;

	Tk_DeleteEventHandler(slavePtr->tkwin, StructureNotifyMask,
	    SlaveEventProc, (ClientData)slavePtr);
	hPtr = Tcl_FindHashEntry(&(slavePtr->textPtr->slaves),
	    (char *)slavePtr->tkwin);
	Tcl_DeleteHashEntry(hPtr);
	Tk_DestroyWindow(slavePtr->tkwin);
    }
    ckfree((char *)slavePtr);
}

/* Line Procedures */
/*
 * ----------------------------------------------------------------------
 *
 * CreateLine --
 *
 * 	This procedure creates and initializes a new line of text.
 *
 * Results:
 *	The return value is a pointer to a structure describing the new
 * 	line of text.  If an error occurred, then the return value is NULL
 *	and an error message is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated.
 *
 * ----------------------------------------------------------------------
 */
static Line *
CreateLine(textPtr)
    HText *textPtr;
{
    Line *linePtr;

    if (textPtr->numLines >= textPtr->arraySize) {
	if (textPtr->arraySize == 0) {
	    textPtr->arraySize = DEF_LINES_ALLOC;
	} else {
	    textPtr->arraySize += textPtr->arraySize;
	}
	if (ResizeArray((char **)&(textPtr->lineArr), sizeof(Line),
		textPtr->arraySize, textPtr->numLines) != TCL_OK) {
	    return NULL;
	}
    }
    /* Initialize values in the new entry */

    linePtr = textPtr->lineArr + textPtr->numLines;
    linePtr->offset = 0;
    linePtr->height = linePtr->width = 0;
    linePtr->textStart = 0;
    linePtr->textEnd = -1;
    linePtr->baseline = 0;
    Blt_InitList(&(linePtr->winList), TCL_ONE_WORD_KEYS);

    textPtr->numLines++;
    return (linePtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyLine --
 *
 * 	This procedure is invoked to clean up the internal structure
 *	of a line.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the line (text and slaves) is
 *	freed up.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyLine(linePtr)
    Line *linePtr;
{
    Blt_ListItem *itemPtr;
    Slave *slavePtr;

    /* Free the list of slave structures */

    for (itemPtr = Blt_FirstListItem(&(linePtr->winList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	slavePtr = (Slave *)Blt_GetItemValue(itemPtr);
	DestroySlave(slavePtr);
    }
    Blt_ResetList(&(linePtr->winList));
}

static void
FreeText(textPtr)
    HText *textPtr;
{
    int i;

    for (i = 0; i < textPtr->numLines; i++) {
	DestroyLine(textPtr->lineArr + i);
    }
    textPtr->numLines = 0;
    textPtr->numChars = 0;
    if (textPtr->charArr != NULL) {
	ckfree(textPtr->charArr);
	textPtr->charArr = NULL;
    }
}

/* Text Procedures */
/*
 * ----------------------------------------------------------------------
 *
 * DestroyText --
 *
 * 	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a HText at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyText(freeProcData)
    FreeProcData freeProcData;	/* Info about hypertext widget. */
{
    HText *textPtr = (HText *)freeProcData;

    if (textPtr->drawGC != NULL) {
	Tk_FreeGC(textPtr->display, textPtr->drawGC);
    }
    if (textPtr->fillGC != NULL) {
	Tk_FreeGC(textPtr->display, textPtr->fillGC);
    }
    if (textPtr->tile != NULL) {
	Blt_FreeTile(textPtr->tile);
    }
    if (textPtr->selectGC != NULL) {
	Tk_FreeGC(textPtr->display, textPtr->selectGC);
    }
    FreeText(textPtr);
    if (textPtr->lineArr != NULL) {
	ckfree((char *)textPtr->lineArr);
    }
    Tk_FreeOptions(configSpecs, (char *)textPtr, textPtr->display, 0);
    Tcl_DeleteHashTable(&(textPtr->slaves));
    ckfree((char *)textPtr);
}

/*
 * --------------------------------------------------------------
 *
 * TextEventProc --
 *
 * 	This procedure is invoked by the Tk dispatcher for various
 * 	events on hypertext widgets.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 * --------------------------------------------------------------
 */
static void
TextEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    HText *textPtr = (HText *)clientData;

    if (eventPtr->type == ConfigureNotify) {
	if ((textPtr->lastWidth != Tk_Width(textPtr->tkwin)) ||
	    (textPtr->lastHeight != Tk_Height(textPtr->tkwin))) {
	    textPtr->flags |= (REQUEST_LAYOUT | TEXT_DIRTY);
	    EventuallyRedraw(textPtr);
	}
    } else if (eventPtr->type == Expose) {

	/*
	 * If the Expose event was synthetic (i.e. we manufactured it
	 * ourselves during a redraw operation), toggle the bit flag
	 * which controls redraws.
	 */

	if (eventPtr->xexpose.send_event) {
	    textPtr->flags ^= IGNORE_EXPOSURES;
	    return;
	}
	if ((eventPtr->xexpose.count == 0) &&
	    !(textPtr->flags & IGNORE_EXPOSURES)) {
	    textPtr->flags |= TEXT_DIRTY;
	    EventuallyRedraw(textPtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (textPtr->tkwin != NULL) {
	    char *cmdName;

	    cmdName = Tcl_GetCommandName(textPtr->interp, textPtr->cmdToken);
#ifdef USE_TK_NAMESPACES
	    Itk_SetWidgetCommand(textPtr->tkwin, (Tcl_Command)NULL);
#endif /* USE_TK_NAMESPACES */
	    textPtr->tkwin = NULL;
	    Tcl_DeleteCommand(textPtr->interp, cmdName);
	}
	if (textPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayText, (ClientData)textPtr);
	}
	Tcl_EventuallyFree((ClientData)textPtr, (Tcl_FreeProc *)DestroyText);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TextDeleteCmdProc --
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
TextDeleteCmdProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    HText *textPtr = (HText *)clientData;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (textPtr->tkwin != NULL) {
	Tk_Window tkwin;

	tkwin = textPtr->tkwin;
	textPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
#ifdef USE_TK_NAMESPACES
	Itk_SetWidgetCommand(tkwin, (Tcl_Command)NULL);
#endif /* USE_TK_NAMESPACES */
    }
}


/*
 *----------------------------------------------------------------------
 *
 * TileChangedProc
 *
 *	Stub for image change notifications.  Since we immediately draw
 *	the image into a pixmap, we don't care about image changes.
 *
 *	It would be better if Tk checked for NULL proc pointers.
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
    HText *textPtr = (HText *)clientData;

    if (textPtr->tkwin != NULL) {
	unsigned long gcMask;
	XGCValues gcValues;
	GC newGC;
	Pixmap pixmap;

	gcMask = GCBackground;
	gcValues.foreground = textPtr->normalBg->pixel;
	pixmap = Blt_PixmapOfTile(tile);
	if (pixmap != None) {
	    gcMask |= (GCTile | GCFillStyle);
	    gcValues.fill_style = FillTiled;
	    gcValues.tile = pixmap;
	} 
	newGC = Tk_GetGC(textPtr->tkwin, gcMask, &gcValues);
	if (*gcPtr != NULL) {
	    Tk_FreeGC(textPtr->display, *gcPtr);
	}
	*gcPtr = newGC;
	EventuallyRedraw(textPtr);
    }
}


/* Configuration Procedures */
static void
ResetTextInfo(textPtr)
    HText *textPtr;
{
    textPtr->first = 0;
    textPtr->last = textPtr->numLines - 1;
    textPtr->selectFirst = textPtr->selectLast = -1;
    textPtr->selectAnchor = 0;
    textPtr->pendingX = textPtr->pendingY = 0;
    textPtr->virtText.width = textPtr->virtText.height = 0;
    textPtr->viewOrigin.x = textPtr->viewOrigin.y = 0;
}

static Line *
GetLastLine(textPtr)
    HText *textPtr;
{
    if (textPtr->numLines == 0) {
	return (CreateLine(textPtr));
    }
    return (textPtr->lineArr + (textPtr->numLines - 1));
}

/*
 * ----------------------------------------------------------------------
 *
 * bltReadFile --
 *
 * 	Read the named file into a newly allocated buffer.
 *
 * Results:
 *	Returns the size of the allocated buffer if the file was
 *	read correctly.  Otherwise -1 is returned and "interp->result"
 *	will contain an error message.
 *
 * Side Effects:
 *	If successful, the contents of "bufferPtr" will point
 *	to the allocated buffer.
 *
 * ----------------------------------------------------------------------
 */
static int
bltReadFile(interp, fileName, bufferPtr)
    Tcl_Interp *interp;
    char *fileName;
    char **bufferPtr;
{
    FILE *f;
    int numRead, fileSize;
    int count, bytesLeft;
    char *buffer;
    int result = -1;
    struct stat fileInfo;

    f = fopen(fileName, "r");
    if (f == NULL) {
	Tcl_AppendResult(interp, "can't open \"", fileName,
	    "\" for reading: ", Tcl_PosixError(interp), (char *)NULL);
	return -1;
    }
    if (fstat(fileno(f), &fileInfo) < 0) {
	Tcl_AppendResult(interp, "can't stat \"", fileName, "\": ",
	    Tcl_PosixError(interp), (char *)NULL);
	fclose(f);
	return -1;
    }
    fileSize = fileInfo.st_size + 1;
    buffer = (char *)ckalloc(sizeof(char) * fileSize);
    if (buffer == NULL) {
	fclose(f);
	return -1;		/* Can't allocate memory for file buffer */
    }
    count = 0;
    for (bytesLeft = fileInfo.st_size; bytesLeft > 0; bytesLeft -= numRead) {
	numRead = fread(buffer + count, sizeof(char), bytesLeft, f);
	if (numRead < 0) {
	    Tcl_AppendResult(interp, "error reading \"", fileName, "\": ",
		Tcl_PosixError(interp), (char *)NULL);
	    fclose(f);
	    ckfree(buffer);
	    return -1;
	} else if (numRead == 0) {
	    break;
	}
	count += numRead;
    }
    fclose(f);
    buffer[count] = '\0';
    result = count;
    *bufferPtr = buffer;
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * CollectCommand --
 *
 * 	Collect the characters representing a Tcl command into a
 *	given buffer.
 *
 * Results:
 *	Returns the number of bytes examined.  If an error occurred,
 *	-1 is returned and "interp->result" will contain an error
 *	message.
 *
 * Side Effects:
 *	If successful, the "cmdArr" will be filled with the string
 *	representing the Tcl command.
 *
 * ----------------------------------------------------------------------
 */

static int
CollectCommand(textPtr, inputArr, maxBytes, cmdArr)
    HText *textPtr;		/* Widget record */
    char inputArr[];		/* Array of bytes representing the htext input*/
    int maxBytes;		/* Maximum number of bytes left in input */
    char cmdArr[];		/* Output buffer to be filled with the Tcl
				 * command */
{
    int c;
    int i;
    int state, count;

    /* Simply collect the all the characters until %% into a buffer */

    state = count = 0;
    for (i = 0; i < maxBytes; i++) {
	c = inputArr[i];
	if (c == textPtr->specChar) {
	    state++;
	} else if ((state == 0) && (c == '\\')) {
	    state = 3;
	} else {
	    state = 0;
	}
	switch (state) {
	case 2:		/* End of command block found */
	    cmdArr[count - 1] = '\0';
	    return (i);

	case 4:		/* Escaped block designator */
	    cmdArr[count] = c;
	    state = 0;
	    break;

	default:		/* Add to command buffer */
	    cmdArr[count++] = c;
	    break;
	}
    }
    Tcl_SetResult(textPtr->interp, "premature end of TCL command block",
        TCL_STATIC);
    return -1;
}

/*
 * ----------------------------------------------------------------------
 *
 * ParseInput --
 *
 * 	Parse the input to the HText structure into an array of lines.
 *	Each entry contains the beginning index and end index of the
 *	characters in the text array which comprise the line.
 *
 *	|*|*|*|\n|T|h|i|s| |a| |l|i|n|e| |o|f| |t|e|x|t|.|\n|*|*|*|
 *                ^					^
 *	          textStart				textEnd
 *
 *	Note that the end index does *not* contain the '\n' for Tk 8.x.
 *
 * Results:
 *	Returns TCL_OK or error depending if the file was read correctly.
 *
 * ----------------------------------------------------------------------
 */
static int
ParseInput(interp, textPtr, input, numBytes)
    Tcl_Interp *interp;
    HText *textPtr;
    char input[];
    int numBytes;
{
    int c;
    int i;
    char *textArr;
    char *cmdArr;
    int count, numLines;
    int numTabs;
    char *inputPtr;
    int length;
    int state;
    Line *linePtr;

    linePtr = CreateLine(textPtr);
    if (linePtr == NULL) {
	return TCL_ERROR;	/* Error allocating the line structure */
    }
    /*  Right now, we replace the text array instead of appending to it */

    linePtr->textStart = 0;

    /* The MeasureChars/DrawChars functions of Tk 8.0 will treat tabs as
       "usual" special characters and replace them with \t instead of
       indenting the following chars. Therefore, we have to replace them
       ourselves with an appropriate number of spaces.
    */
    numTabs = 0;
    for (inputPtr = input; inputPtr < &input[numBytes]; /* empty */) {
	if (*inputPtr++ == '\t') {
	    ++numTabs;
	}
    }
    /* In the worst case, assume the entire input could be Tcl commands
       and all tabs are replaced with 8 spaces */
    cmdArr = ckalloc(sizeof(char) * ((numBytes + 1) + (numTabs * 7)));

    textArr = ckalloc(sizeof(char) * (numBytes + 1));
    if (textPtr->charArr != NULL) {
	ckfree((char *)textPtr->charArr);
    }
    textPtr->charArr = textArr;
    textPtr->numChars = 0;

    numLines = count = state = 0;
    textPtr->flags &= ~SLAVE_APPENDED;

    for (i = 0; i < numBytes; i++) {
	c = input[i];
	if (c == textPtr->specChar) {
	    state++;
	} else if (c == '\n') {
	    state = -1;
	} else if (c == '\t') {
	    state = -2;
	} else if ((state == 0) && (c == '\\')) {
	    state = 3;
	} else {
	    state = 0;
	}

	switch (state) {
	case 2:		/* Block of Tcl commands found */
	    count--, i++;
	    length = CollectCommand(textPtr, input + i, numBytes - i, cmdArr);
	    if (length < 0) {
		goto error;
	    }
	    i += length;
	    linePtr->textEnd = count;
	    textPtr->numChars = count + 1;
	    if (Tcl_Eval(interp, cmdArr) != TCL_OK) {
		goto error;
	    }
	    if (textPtr->flags & SLAVE_APPENDED) {
		/* Indicates the location a slave window in the text array */
		textArr[count++] = ' ';
		textPtr->flags &= ~SLAVE_APPENDED;
	    }
	    state = 0;
	    break;

	case 4:		/* Escaped block designator */
	    textArr[count - 1] = c;
	    state = 0;
	    break;

	case -1:		/* End of line or input */
	    linePtr->textEnd = count-1;
	    textArr[count++] = '\n';
	    numLines++;
	    linePtr = CreateLine(textPtr);
	    if (linePtr == NULL) {
		goto error;
	    }
	    linePtr->textStart = count;
	    state = 0;
	    break;

	case -2:		/* Tabulator, replace with spaces */
	    numTabs = 8 - ((count - linePtr->textStart) % 8);
	    while (numTabs--) {
	        textArr[count++] = ' ';
	    }
	    state = 0;
	    break;

	default:		/* Default action, add to text buffer */
	    textArr[count++] = c;
	    break;
	}
    }
    if (count > linePtr->textStart) {
	linePtr->textEnd = count-1;
	textArr[count++] = '\n'; /* Every line must end with a '\n' */
	numLines++;
    }
    ckfree((char *)cmdArr);
    /* Reset number of lines allocated */
    if (ResizeArray((char **)&(textPtr->lineArr), sizeof(Line), numLines,
	    textPtr->arraySize) != TCL_OK) {
	Tcl_SetResult(interp, "can't reallocate array of lines", TCL_STATIC);
	return TCL_ERROR;
    }
    textPtr->numLines = textPtr->arraySize = numLines;
    /*  and the size of the character array */
    if (ResizeArray(&(textPtr->charArr), sizeof(char), count,
	    numBytes) != TCL_OK) {
	Tcl_SetResult(interp, "can't reallocate text character buffer",
	    TCL_STATIC);
	return TCL_ERROR;
    }
    textPtr->numChars = count;
    return TCL_OK;

  error:
    ckfree((char *)cmdArr);
    return TCL_ERROR;
}

static int
IncludeText(interp, textPtr, fileName)
    Tcl_Interp *interp;
    HText *textPtr;
    char *fileName;
{
    char *buffer;
    int result;
    int numBytes;

    if ((textPtr->text == NULL) && (fileName == NULL)) {
	return TCL_OK;		/* Empty text string */
    }
    if (fileName != NULL) {
	numBytes = bltReadFile(interp, fileName, &buffer);
	if (numBytes < 0) {
	    return TCL_ERROR;
	}
    } else {
	buffer = textPtr->text;
	numBytes = strlen(textPtr->text);
    }
    result = ParseInput(interp, textPtr, buffer, numBytes);
    if (fileName != NULL) {
	ckfree(buffer);
    }
    return (result);
}

/* ARGSUSED */
static char *
TextVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about widget. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    HText *textPtr = (HText *)clientData;
    HText *lasttextPtr;

    /* Check to see of this is the most recent trace */
    lasttextPtr = (HText *)Tcl_VarTraceInfo2(interp, name1, name2, flags,
	(Tcl_VarTraceProc*)TextVarProc, (ClientData)NULL);
    if (lasttextPtr != textPtr) {
	return NULL;		/* Ignore all but most current trace */
    }
    if (flags & TCL_TRACE_READS) {
	char c;

	c = name2[0];
	if ((c == 'w') && (strcmp(name2, "widget") == 0)) {
	    Tcl_SetVar2(interp, name1, name2, Tk_PathName(textPtr->tkwin),
		flags);
	} else if ((c == 'l') && (strcmp(name2, "line") == 0)) {
	    char buf[80];
	    int lineNum;

	    lineNum = textPtr->numLines - 1;
	    if (lineNum < 0) {
		lineNum = 0;
	    }
	    sprintf(buf, "%d", lineNum);
	    Tcl_SetVar2(interp, name1, name2, buf, flags);
	} else if ((c == 'i') && (strcmp(name2, "index") == 0)) {
	    char buf[80];

	    sprintf(buf, "%d", textPtr->numChars - 1);
	    Tcl_SetVar2(interp, name1, name2, buf, flags);
	} else if ((c == 'f') && (strcmp(name2, "file") == 0)) {
	    char *fileName;

	    fileName = textPtr->fileName;
	    if (fileName == NULL) {
		fileName = "";
	    }
	    Tcl_SetVar2(interp, name1, name2, fileName, flags);
	} else {
	    return ("??");
	}
    }
    return NULL;
}

static char *varNames[] =
{
    "widget", "line", "file", "index", (char *)NULL
};

static void
CreateTraces(textPtr)
    HText *textPtr;
{
    char **ptr;
    static char globalCmd[] = "global htext";

    /*
     * Make the traced variables global to the widget
     */
    Tcl_Eval(textPtr->interp, globalCmd);
    for (ptr = varNames; *ptr != NULL; ptr++) {
	Tcl_TraceVar2(textPtr->interp, "htext", *ptr,
	    (TCL_GLOBAL_ONLY | TCL_TRACE_READS), (Tcl_VarTraceProc*)TextVarProc,
	    (ClientData)textPtr);
    }
}

static void
DeleteTraces(textPtr)
    HText *textPtr;
{
    char **ptr;

    for (ptr = varNames; *ptr != NULL; ptr++) {
	Tcl_UntraceVar2(textPtr->interp, "htext", *ptr,
	    (TCL_GLOBAL_ONLY | TCL_TRACE_READS), (Tcl_VarTraceProc*)TextVarProc,
	    (ClientData)textPtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ConfigureText --
 *
 * 	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a hypertext widget.
 *
 * 	The layout of the text must be calculated (by ComputeLayout)
 *	whenever particular options change; -font, -file, -linespacing
 *	and -text options. If the user has changes one of these options,
 *	it must be detected so that the layout can be recomputed. Since the
 *	coordinates of the layout are virtual, there is no need to adjust
 *	them if physical window attributes (window size, etc.)
 *	change.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 * 	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 * 	etc. get set for textPtr;  old resources get freed, if there were any.
 * 	The hypertext is redisplayed.
 *
 * ----------------------------------------------------------------------
 */
static int
ConfigureText(interp, textPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    HText *textPtr;		/* Information about widget; may or may not
			         * already have values for some fields. */
{
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;

    if (Blt_ConfigModified(configSpecs, "-font", "-linespacing", "-file",
	    "-text", "-width", "-height", (char *)NULL)) {
	/*
	 * These options change the layout of the text.  Width/height
	 * and rows/columns may change a relatively sized window or cavity.
	 */
	textPtr->flags |= (REQUEST_LAYOUT | TEXT_DIRTY); /* Mark for update */
    }
    gcMask = GCForeground | GCFont;
    gcValues.font = textPtr->fontPtr->fid;
    gcValues.foreground = textPtr->normalFg->pixel;
    newGC = Tk_GetGC(textPtr->tkwin, gcMask, &gcValues);
    if (textPtr->drawGC != NULL) {
	Tk_FreeGC(textPtr->display, textPtr->drawGC);
    }
    textPtr->drawGC = newGC;

    gcValues.foreground = textPtr->selectFg->pixel;
    newGC = Tk_GetGC(textPtr->tkwin, gcMask, &gcValues);
    if (textPtr->selectGC != NULL) {
	Tk_FreeGC(textPtr->display, textPtr->selectGC);
    }
    textPtr->selectGC = newGC;

    if (textPtr->scrollX < 1) {
	textPtr->scrollX = 1;
    }
    if (textPtr->scrollY < 1) {
	textPtr->scrollY = 1;
    }

    if (textPtr->tile != NULL) {
	Pixmap pixmap;

	Blt_SetTileChangedProc(textPtr->tile, TileChangedProc,
	    (ClientData)textPtr, &(textPtr->fillGC));
	pixmap = Blt_PixmapOfTile(textPtr->tile);
	if (pixmap != None) {
	    gcMask |= (GCTile | GCFillStyle);
	    gcValues.fill_style = FillTiled;
	    gcValues.tile = pixmap;
	} 
    }

    gcValues.foreground = textPtr->normalBg->pixel;
    newGC = Tk_GetGC(textPtr->tkwin, gcMask, &gcValues);
    if (textPtr->fillGC != NULL) {
	Tk_FreeGC(textPtr->display, textPtr->fillGC);
    }
    textPtr->fillGC = newGC;

    if (textPtr->numColumns > 0) {
	textPtr->reqWidth =
	    textPtr->numColumns * Tk_TextWidth((Tk_Font)textPtr->fontPtr,
	    				"0", 1);
    }
    if (textPtr->numRows > 0) {
	textPtr->reqHeight = textPtr->numRows * TEXTHEIGHT(textPtr->fontPtr);
    }
    /*
     * If the either the -text or -file option changed, read in the
     * new text.  The -text option supersedes any -file option.
     */
    if (Blt_ConfigModified(configSpecs, "-file", "-text", (char *)NULL)) {
	int result;

	FreeText(textPtr);
	CreateTraces(textPtr);	/* Create variable traces */

	result = IncludeText(interp, textPtr, textPtr->fileName);

	DeleteTraces(textPtr);
	if (result == TCL_ERROR) {
	    FreeText(textPtr);
	    return TCL_ERROR;
	}
	ResetTextInfo(textPtr);
    }
    EventuallyRedraw(textPtr);
    return TCL_OK;
}

/* Layout Procedures */
/*
 * -----------------------------------------------------------------
 *
 * TranslateAnchor --
 *
 * 	Translate the coordinates of a given bounding box based
 *	upon the anchor specified.  The anchor indicates where
 *	the given xy position is in relation to the bounding box.
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
 * -----------------------------------------------------------------
 */
static XPoint
TranslateAnchor(deltaX, deltaY, anchor)
    int deltaX, deltaY;		/* Difference between outer and inner regions
				 */
    Tk_Anchor anchor;		/* Direction of the anchor */
{
    XPoint anchorPos;

    anchorPos.x = anchorPos.y = 0;
    switch (anchor) {
    case TK_ANCHOR_NW:		/* Upper left corner */
	break;
    case TK_ANCHOR_W:		/* Left center */
	anchorPos.y = (deltaY / 2);
	break;
    case TK_ANCHOR_SW:		/* Lower left corner */
	anchorPos.y = deltaY;
	break;
    case TK_ANCHOR_N:		/* Top center */
	anchorPos.x = (deltaX / 2);
	break;
    case TK_ANCHOR_CENTER:	/* Centered */
	anchorPos.x = (deltaX / 2);
	anchorPos.y = (deltaY / 2);
	break;
    case TK_ANCHOR_S:		/* Bottom center */
	anchorPos.x = (deltaX / 2);
	anchorPos.y = deltaY;
	break;
    case TK_ANCHOR_NE:		/* Upper right corner */
	anchorPos.x = deltaX;
	break;
    case TK_ANCHOR_E:		/* Right center */
	anchorPos.x = deltaX;
	anchorPos.y = (deltaY / 2);
	break;
    case TK_ANCHOR_SE:		/* Lower right corner */
	anchorPos.x = deltaX;
	anchorPos.y = deltaY;
	break;
    }
    return (anchorPos);
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeCavitySize --
 *
 *	Sets the width and height of the cavity based upon the
 *	requested size of the slave window.  The requested space also
 *	includes any external padding which has been designated for
 *	this window.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The size of the cavity is set in the slave information
 *	structure.  These values can effect how the slave window is
 *	packed into the master window.
 *
 *----------------------------------------------------------------------
 */
static void
ComputeCavitySize(slavePtr)
    Slave *slavePtr;
{
    int width, height;
    int twiceBW;

    twiceBW = 2 * Tk_Changes(slavePtr->tkwin)->border_width;
    if (slavePtr->reqCavityWidth > 0) {
	width = slavePtr->reqCavityWidth;
    } else if (slavePtr->relCavityWidth > 0.0) {
	width = (int)((double)Tk_Width(slavePtr->textPtr->tkwin) *
	    slavePtr->relCavityWidth + 0.5);
    } else {
	width = GetSlaveWidth(slavePtr) + PADDING(slavePtr->padX) + twiceBW;
    }
    slavePtr->cavityWidth = width;

    if (slavePtr->reqCavityHeight > 0) {
	height = slavePtr->reqCavityHeight;
    } else if (slavePtr->relCavityHeight > 0.0) {
	height = (int)((double)Tk_Height(slavePtr->textPtr->tkwin) *
	    slavePtr->relCavityHeight + 0.5);
    } else {
	height = GetSlaveHeight(slavePtr) + PADDING(slavePtr->padY) + twiceBW;
    }
    slavePtr->cavityHeight = height;
}

/*
 *----------------------------------------------------------------------
 *
 * LayoutLine --
 *
 *	This procedure computes the total width and height needed
 *      to contain the text and slaves for a particular line.
 *      It also calculates the baseline of the text on the line with
 *	respect to the other slaves on the line.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
LayoutLine(textPtr, linePtr)
    HText *textPtr;
    Line *linePtr;
{
    Slave *slavePtr;
    int textStart, textLength;
    int maxAscent, maxDescent, maxHeight;
    int ascent, descent;
    int median;			/* Difference of font ascent/descent values */
    Blt_ListItem *itemPtr;
    int x, y;
    int newX;

    /* Initialize line defaults */

    maxAscent = textPtr->fontPtr->fm.ascent;
    maxDescent = textPtr->fontPtr->fm.descent;
    median = textPtr->fontPtr->fm.ascent - textPtr->fontPtr->fm.descent;
    ascent = descent = 0;	/* Suppress compiler warnings */

    /*
     * Pass 1: Determine the maximum ascent (baseline) and descent
     * needed for the line.  We'll need this for figuring the top,
     * bottom, and center anchors.
     */
    for (itemPtr = Blt_FirstListItem(&(linePtr->winList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	slavePtr = (Slave *)Blt_GetItemValue(itemPtr);
	if (slavePtr->tkwin == NULL) {
	    continue;
	}
	ComputeCavitySize(slavePtr);

	switch (slavePtr->justify) {
	case JUSTIFY_TOP:
	    ascent = textPtr->fontPtr->fm.ascent + slavePtr->padTop;
	    descent = slavePtr->cavityHeight - textPtr->fontPtr->fm.ascent;
	    break;
	case JUSTIFY_CENTER:
	    ascent = (slavePtr->cavityHeight + median) / 2;
	    descent = (slavePtr->cavityHeight - median) / 2;
	    break;
	case JUSTIFY_BOTTOM:
	    ascent = slavePtr->cavityHeight - textPtr->fontPtr->fm.descent;
	    descent = textPtr->fontPtr->fm.descent;
	    break;
	}
	if (descent > maxDescent) {
	    maxDescent = descent;
	}
	if (ascent > maxAscent) {
	    maxAscent = ascent;
	}
    }

    maxHeight = maxAscent + maxDescent + textPtr->lineSpacing;
    x = 0;			/* Always starts from x=0 */
    y = 0;			/* Suppress compiler warning */
    textStart = linePtr->textStart;

    /*
     * Pass 2: Find the placements of the text and slaves along each
     * line.
     */
    for (itemPtr = Blt_FirstListItem(&(linePtr->winList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	slavePtr = (Slave *)Blt_GetItemValue(itemPtr);
	if (slavePtr->tkwin == NULL) {
	    continue;
	}
	/* Get the width of the text leading to the slave */
	textLength = (slavePtr->precedingTextEnd - textStart);
	if (textLength > 0) {
	    Tk_MeasureChars((Tk_Font)textPtr->fontPtr,
	        textPtr->charArr + textStart,
		textLength, 10000, TK_AT_LEAST_ONE, &newX);
	    slavePtr->precedingTextWidth = newX;
	    x += newX;
	}
	switch (slavePtr->justify) {
	case JUSTIFY_TOP:
	    y = maxAscent - textPtr->fontPtr->fm.ascent;
	    break;
	case JUSTIFY_CENTER:
	    y = maxAscent - (slavePtr->cavityHeight + median) / 2;
	    break;
	case JUSTIFY_BOTTOM:
	    y = maxAscent + textPtr->fontPtr->fm.descent-slavePtr->cavityHeight;
	    break;
	}
	slavePtr->x = x, slavePtr->y = y;

	/* Skip over trailing space */
	textStart = slavePtr->precedingTextEnd + 1;

	x += slavePtr->cavityWidth;
    }

    /*
     * This can be either the trailing piece of a line after the last slave
     * or the entire line if no slaves windows are embedded in it.
     */
    textLength = (linePtr->textEnd - textStart) + 1;
    if (textLength > 0) {
	Tk_MeasureChars((Tk_Font)textPtr->fontPtr,
	    textPtr->charArr + textStart, textLength, -1, NO_FLAGS, &newX);
	x += newX;
    }
    /* Update line parameters */
    if ((linePtr->width != x) || (linePtr->height != maxHeight) ||
	(linePtr->baseline != maxAscent)) {
	textPtr->flags |= TEXT_DIRTY;
    }
    linePtr->width = x;
    linePtr->height = maxHeight;
    linePtr->baseline = maxAscent;
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeLayout --
 *
 *	This procedure computes the total width and height needed
 *      to contain the text and slaves from all the lines of text.
 *      It merely sums the heights and finds the maximum width of
 *	all the lines.  The width and height are needed for scrolling.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
ComputeLayout(textPtr)
    HText *textPtr;
{
    int count;
    Line *linePtr;
    int height, width;

    width = height = 0;
    for (count = 0; count < textPtr->numLines; count++) {
	linePtr = textPtr->lineArr + count;

	linePtr->offset = height;
	LayoutLine(textPtr, linePtr);
	height += linePtr->height;
	if (linePtr->width > width) {
	    width = linePtr->width;
	}
    }
    /*
     * Set changed flag if new layout changed size of virtual text.
     */
    if ((height != textPtr->virtText.height) ||
	(width != textPtr->virtText.width)) {
	textPtr->virtText.height = height;
	textPtr->virtText.width = width;
	textPtr->flags |= TEXT_DIRTY;
    }
}

/* Display Procedures */
/*
 * ----------------------------------------------------------------------
 *
 * GetVisibleLines --
 *
 * 	Calculates which lines are visible using the height
 *      of the viewport and y offset from the top of the text.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Only those line between first and last inclusive are
 * 	redrawn.
 *
 * ----------------------------------------------------------------------
 */
static int
GetVisibleLines(textPtr)
    HText *textPtr;
{
    int topLine, bottomLine;
    int firstY, lastY;
    int lastLine;

    if (textPtr->numLines == 0) {
	textPtr->first = 0;
	textPtr->last = -1;
	return TCL_OK;
    }
    firstY = textPtr->pendingY;
    lastLine = textPtr->numLines - 1;

    /* First line */
    topLine = LineSearch(textPtr, firstY, 0, lastLine);
    if (topLine < 0) {
	/*
	 * This can't be. The y-coordinate offset must be corrupted.
	 *
	 * mike: Unfortunately, it *can* be: just scroll down one more
	 * line if the last line is displayed on the top line of the
	 * htext widget. I guess this has to do with the new scrollbar
	 * protocol, as I haven't seen such behaviour with the old one.
	 * So, instead of griping, we just return TCL_ERROR (this seems
	 * to be safe, but it makes for "interesting" effects when
	 * scanning (<B2-Motion>) past the end of a text -> FIXME).
	 *
	 * fprintf(stderr, "internal error: First position not found `%d'\n",
	 *     firstY);
	 */
	return TCL_ERROR;
    }
    textPtr->first = topLine;

    /*
     * If there is less text than window space, the bottom line is the
     * last line of text.  Otherwise search for the line at the bottom
     * of the window.
     */
    lastY = firstY + Tk_Height(textPtr->tkwin) - 1;
    if (lastY < textPtr->virtText.height) {
	bottomLine = LineSearch(textPtr, lastY, topLine, lastLine);
    } else {
	bottomLine = lastLine;
    }
    if (bottomLine < 0) {
	/*
	 * This can't be. The newY offset must be corrupted.
	 */
	fprintf(stderr, "internal error: Last position not found `%d'\n",
	    lastY);
#ifdef notdef
	fprintf(stderr,"virtText.height=%d,height=%d,top=%d,first=%d,last=%d\n",
	    textPtr->virtText.height, Tk_Height(textPtr->tkwin), firstY,
	    textPtr->lineArr[topLine].offset,
	    textPtr->lineArr[lastLine].offset);
#endif
	return TCL_ERROR;
    }
    textPtr->last = bottomLine;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * UpdateScrollbar --
 *
 * 	Invoke a Tcl command to the scrollbar, defining the new
 *	position and length of the scroll. See the Tk documentation
 *	for further information on the scrollbar.  It is assumed the
 *	scrollbar command prefix is valid.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Scrollbar is commanded to change position and/or size.
 *
 * ----------------------------------------------------------------------
 */
static void
UpdateScrollbar(interp, command, total, window, first, units)
    Tcl_Interp *interp;
    char *command;		/* scrollbar command */
    int total;			/* Total distance */
    int window;			/* Window distance */
    int first;			/* Position of viewport */
    int units;			/* Unit distance */
{
    char scrollArgs[1000];
    int totalUnits, windowUnits;
    int firstUnit, lastUnit;

    totalUnits = (total / units) + 1;
    windowUnits = window / units;
    firstUnit = first / units;
    lastUnit = (firstUnit + windowUnits);

    /* Keep scrolling within range. */
    if (firstUnit >= totalUnits) {
	firstUnit = totalUnits;
    }
    if (lastUnit > totalUnits) {
	lastUnit = totalUnits;
    }
    sprintf(scrollArgs, " %d %d %d %d", totalUnits, windowUnits,
	firstUnit, lastUnit);
    if (Tcl_VarEval(interp, command, scrollArgs, (char *)NULL) != TCL_OK) {
	Tk_BackgroundError(interp);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawSegment --
 *
 * 	Draws a line segment, designated by the segment structure.
 *	This routine handles the display of selected text by drawing
 *	a raised 3D border underneath the selected text.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The line segment is drawn on *draw*.
 *
 * ----------------------------------------------------------------------
 */
static void
DrawSegment(textPtr, draw, linePtr, x, y, segPtr)
    HText *textPtr;
    Drawable draw;
    Line *linePtr;
    int x, y;
    TextSegment *segPtr;
{
    int lastX, curPos, numChars;
    int textLength;
    int selectStart, selectEnd, selectLength;
    int tabOrigin;

#ifdef notdef
    fprintf(stderr, "DS select: first=%d,last=%d text: first=%d,last=%d\n",
	textPtr->selectFirst, textPtr->selectLast,
	segPtr->textStart, segPtr->textEnd);
#endif
    textLength = (segPtr->textEnd - segPtr->textStart) + 1;
    if (textLength < 1) {
	return;
    }
    tabOrigin = -(textPtr->viewOrigin.x);
    if ((segPtr->textEnd < textPtr->selectFirst) ||
	(segPtr->textStart > textPtr->selectLast)) {	/* No selected text */
	Tk_DrawChars(textPtr->display, draw, textPtr->drawGC,
	    (Tk_Font)textPtr->fontPtr, textPtr->charArr + segPtr->textStart,
	    textLength, x, y + linePtr->baseline);
	return;
    }
    /*
     *	Text in a segment (with selected text) may have
     *	up to three regions:
     *
     *	1) the text before the start the selection
     *	2) the selected text itself (drawn in a raised border)
     *	3) the text following the selection.
     */

    selectStart = segPtr->textStart;
    selectEnd = segPtr->textEnd;
    if (textPtr->selectFirst > segPtr->textStart) {
	selectStart = textPtr->selectFirst;
    }
    if (textPtr->selectLast < segPtr->textEnd) {
	selectEnd = textPtr->selectLast;
    }
    selectLength = (selectEnd - selectStart) + 1;
    lastX = x;
    curPos = segPtr->textStart;

    if (selectStart > segPtr->textStart) {	/* Text preceding selection */
	numChars = (selectStart - segPtr->textStart);
	Tk_MeasureChars((Tk_Font)textPtr->fontPtr,
	    textPtr->charArr + segPtr->textStart,
	    numChars, -1, NO_FLAGS, &lastX);
	lastX += x;
	Tk_DrawChars(textPtr->display, draw, textPtr->drawGC,
	    (Tk_Font)textPtr->fontPtr, textPtr->charArr + segPtr->textStart,
	    numChars, x, y + linePtr->baseline);
	curPos = selectStart;
    }
    if (selectLength > 0) {	/* The selection itself */
	int width, nextX;

	Tk_MeasureChars((Tk_Font)textPtr->fontPtr,
	    textPtr->charArr + selectStart, selectLength, -1, NO_FLAGS, &nextX);
	nextX += lastX;
	width = (selectEnd == linePtr->textEnd)
	    ? textPtr->virtText.width - textPtr->viewOrigin.x - lastX :
	    nextX - lastX;
	Tk_Fill3DRectangle(textPtr->tkwin, draw, textPtr->selectBorder,
	    lastX, y + linePtr->baseline - textPtr->fontPtr->fm.ascent,
	    width, TEXTHEIGHT(textPtr->fontPtr),
	    textPtr->selectBW, TK_RELIEF_RAISED);
	Tk_DrawChars(textPtr->display, draw, textPtr->selectGC,
	    (Tk_Font)textPtr->fontPtr, textPtr->charArr + selectStart,
	    selectLength, lastX, y + linePtr->baseline);
	lastX = nextX;
	curPos = selectStart + selectLength;
    }
    numChars = segPtr->textEnd - curPos;
    if (numChars > 0) {		/* Text following the selection */
	Tk_DrawChars(textPtr->display, draw, textPtr->drawGC,
	    (Tk_Font)textPtr->fontPtr, textPtr->charArr + curPos, numChars,
	    lastX, y + linePtr->baseline);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * MoveSlave --
 *
 * 	Move a slave window to a new location in the hypertext
 *	parent window.  If the window has no geometry (i.e. width,
 *	or height is 0), simply unmap to window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Each slave window is moved to its new location, generating
 *      Expose events in the parent for each slave window moved.
 *
 * ----------------------------------------------------------------------
 */
static void
MoveSlave(slavePtr, offset)
    Slave *slavePtr;
    int offset;
{
    int winWidth, winHeight;
    int width, height;
    int deltaX, deltaY;
    int x, y;
    int intBW;

    winWidth = GetSlaveWidth(slavePtr);
    winHeight = GetSlaveHeight(slavePtr);
    if ((winWidth < 1) || (winHeight < 1)) {
	if (Tk_IsMapped(slavePtr->tkwin)) {
	    Tk_UnmapWindow(slavePtr->tkwin);
	}
	return;
    }
    intBW = Tk_Changes(slavePtr->tkwin)->border_width;
    x = (slavePtr->x + intBW + slavePtr->padLeft) -
	slavePtr->textPtr->viewOrigin.x;
    y = offset + (slavePtr->y + intBW + slavePtr->padTop) -
	slavePtr->textPtr->viewOrigin.y;

    width = slavePtr->cavityWidth - (2 * intBW + PADDING(slavePtr->padX));
    if ((width < winWidth) || (slavePtr->fill & FILL_X)) {
	winWidth = width;
    }
    deltaX = width - winWidth;

    height = slavePtr->cavityHeight - (2 * intBW + PADDING(slavePtr->padY));
    if ((height < winHeight) || (slavePtr->fill & FILL_Y)) {
	winHeight = height;
    }
    deltaY = height - winHeight;

    if ((deltaX > 0) || (deltaY > 0)) {
	XPoint anchorPos;

	anchorPos = TranslateAnchor(deltaX, deltaY, slavePtr->anchor);
	x += anchorPos.x, y += anchorPos.y;
    }
    slavePtr->winWidth = winWidth;
    slavePtr->winHeight = winHeight;

    if ((x != Tk_X(slavePtr->tkwin)) || (y != Tk_Y(slavePtr->tkwin)) ||
	(winWidth != Tk_Width(slavePtr->tkwin)) ||
	(winHeight != Tk_Height(slavePtr->tkwin))) {
	Tk_MoveResizeWindow(slavePtr->tkwin, x, y, winWidth, winHeight);
	if (!Tk_IsMapped(slavePtr->tkwin)) {
	    Tk_MapWindow(slavePtr->tkwin);
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DrawPage --
 *
 * 	This procedure displays the lines of text and moves the slave
 *      windows to their new positions.  It draws lines with regard to
 *	the direction of the scrolling.  The idea here is to make the
 *	text and buttons appear to move together. Otherwise you will
 *	get a "jiggling" effect where the windows appear to bump into
 *	the next line before that line is moved.  In the worst case, where
 *	every line has at least one widget, you can get an aquarium effect
 *      (lines appear to ripple up).
 *
 * 	The text area may start between line boundaries (to accommodate
 *	both variable height lines and constant scrolling). Subtract the
 *	difference of the page offset and the line offset from the starting
 *	coordinates. For horizontal scrolling, simply subtract the offset
 *	of the viewport. The window will clip the top of the first line,
 *	the bottom of the last line, whatever text extends to the left
 *	or right of the viewport on any line.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the line in its current
 * 	mode.
 *
 * ----------------------------------------------------------------------
 */
static void
DrawPage(textPtr, deltaY)
    HText *textPtr;
    int deltaY;			/* Change from previous Y coordinate */
{
    Line *linePtr;
    Slave *slavePtr;
    Tk_Window tkwin = textPtr->tkwin;
    TextSegment sgmt;
    Pixmap pixmap;
    int forceCopy = 0;
    int i;
    int lineNum;
    int x, y, lastY;
    Blt_ListItem *itemPtr;

    /* Create an off-screen pixmap for semi-smooth scrolling. */
    pixmap = Tk_GetPixmap(textPtr->display, Tk_WindowId(tkwin), Tk_Width(tkwin),
	Tk_Height(tkwin), Tk_Depth(tkwin));

    x = -(textPtr->viewOrigin.x);
    y = -(textPtr->viewOrigin.y);


    if (textPtr->tile != NULL) {
	if (textPtr->tileOffsetPage) {
	    XSetTSOrigin(textPtr->display, textPtr->fillGC, x, y);
	} else {
	    Blt_SetTileOrigin(textPtr->tkwin, textPtr->fillGC, 0, 0);
	}
    }

    XFillRectangle(textPtr->display, pixmap, textPtr->fillGC, 0, 0,
	Tk_Width(tkwin), Tk_Height(tkwin));


    if (deltaY >= 0) {
	y += textPtr->lineArr[textPtr->first].offset;
	lineNum = textPtr->first;
	lastY = 0;
    } else {
	y += textPtr->lineArr[textPtr->last].offset;
	lineNum = textPtr->last;
	lastY = Tk_Height(tkwin);
    }
    forceCopy = 0;

    /* Draw each line */
    for (i = textPtr->first; i <= textPtr->last; i++) {

	/* Initialize character position in text buffer to start */
	linePtr = textPtr->lineArr + lineNum;
	sgmt.textStart = linePtr->textStart;
	sgmt.textEnd = linePtr->textEnd;

	/* Initialize X position */
	x = -(textPtr->viewOrigin.x);
	for (itemPtr = Blt_FirstListItem(&(linePtr->winList));
	    itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	    slavePtr = (Slave *)Blt_GetItemValue(itemPtr);

	    if (slavePtr->tkwin != NULL) {
		slavePtr->flags |= SLAVE_VISIBLE;
		MoveSlave(slavePtr, linePtr->offset);
	    }
	    sgmt.textEnd = slavePtr->precedingTextEnd - 1;
	    if (sgmt.textEnd >= sgmt.textStart) {
		DrawSegment(textPtr, pixmap, linePtr, x, y, &sgmt);
		x += slavePtr->precedingTextWidth;
	    }
	    /* Skip over the extra trailing space which designates the slave */
	    sgmt.textStart = slavePtr->precedingTextEnd + 1;
	    x += slavePtr->cavityWidth;
	    forceCopy++;
	}

	/*
	 * This may be the text trailing the last slave or the entire
	 * line if no slaves occur on it.
	 */
	sgmt.textEnd = linePtr->textEnd;
	if (sgmt.textEnd >= sgmt.textStart) {
	    DrawSegment(textPtr, pixmap, linePtr, x, y, &sgmt);
	}
	/* Go to the top of the next line */
	if (deltaY >= 0) {
	    y += textPtr->lineArr[lineNum].height;
	    lineNum++;
	}
	if ((forceCopy > 0) && !(textPtr->flags & TEXT_DIRTY)) {
	    if (deltaY >= 0) {
		XCopyArea(textPtr->display, pixmap, Tk_WindowId(tkwin),
		    textPtr->drawGC, 0, lastY, Tk_Width(tkwin), y - lastY, 0,
		    lastY);
	    } else {
		XCopyArea(textPtr->display, pixmap, Tk_WindowId(tkwin),
		    textPtr->drawGC, 0, y, Tk_Width(tkwin), lastY - y, 0, y);
	    }
	    forceCopy = 0;	/* Reset drawing flag */
	    lastY = y;		/* Record last Y position */
	}
	if ((deltaY < 0) && (lineNum > 0)) {
	    --lineNum;
	    y -= textPtr->lineArr[lineNum].height;
	}
    }
    /*
     * If the viewport was resized, draw the page in one operation.
     * Otherwise draw any left-over block of text (either at the top
     * or bottom of the page)
     */
    if (textPtr->flags & TEXT_DIRTY) {
	XCopyArea(textPtr->display, pixmap, Tk_WindowId(tkwin),
	    textPtr->drawGC, 0, 0, Tk_Width(tkwin), Tk_Height(tkwin), 0, 0);
    } else if (lastY != y) {
	if (deltaY >= 0) {
	    XCopyArea(textPtr->display, pixmap, Tk_WindowId(tkwin),
		textPtr->drawGC, 0, lastY, Tk_Width(tkwin),
		Tk_Height(tkwin) - lastY, 0, lastY);
	} else {
	    XCopyArea(textPtr->display, pixmap, Tk_WindowId(tkwin),
		textPtr->drawGC, 0, 0, Tk_Width(tkwin), lastY, 0, 0);
	}
    }
    Tk_FreePixmap(textPtr->display, pixmap);
}


static void
SendBogusEvent(tkwin)
    Tk_Window tkwin;
{
#define DONTPROPAGATE 0
    XEvent event;

    event.type = event.xexpose.type = Expose;
    event.xexpose.window = Tk_WindowId(tkwin);
    event.xexpose.display = Tk_Display(tkwin);
    event.xexpose.count = 0;
    event.xexpose.x = event.xexpose.y = 0;
    event.xexpose.width = Tk_Width(tkwin);
    event.xexpose.height = Tk_Height(tkwin);

    XSendEvent(Tk_Display(tkwin), Tk_WindowId(tkwin), DONTPROPAGATE,
	ExposureMask, &event);
}

/*
 * ----------------------------------------------------------------------
 *
 * DisplayText --
 *
 * 	This procedure is invoked to display a hypertext widget.
 *	Many of the operations which might ordinarily be performed
 *	elsewhere (e.g. in a configuration routine) are done here
 *	because of the somewhat unusual interactions occurring between
 *	the parent and slave windows.
 *
 *      Recompute the layout of the text if necessary. This is
 *	necessary if the world coordinate system has changed.
 *	Specifically, the following may have occurred:
 *
 *	  1.  a text attribute has changed (font, linespacing, etc.).
 *	  2.  slave option changed (anchor, width, height).
 *        3.  actual slave window was resized.
 *	  4.  new text string or file.
 *
 *      This is deferred to the display routine since potentially
 *      many of these may occur (especially slave window changes).
 *
 *	Set the vertical and horizontal scrollbars (if they are
 *	designated) by issuing a Tcl command.  Done here since
 *	the text window width and height are needed.
 *
 *	If the viewport position or contents have changed in the
 *	vertical direction,  the now out-of-view slave windows
 *	must be moved off the viewport.  Since slave windows will
 *	obscure the text window, it is imperative that the slaves
 *	are moved off before we try to redraw text in the same area.
 *      This is necessary only for vertical movements.  Horizontal
 *	slave window movements are handled automatically in the
 *	page drawing routine.
 *
 *      Get the new first and last line numbers for the viewport.
 *      These line numbers may have changed because either a)
 *      the viewport changed size or position, or b) the text
 *	(slave window sizes or text attributes) have changed.
 *
 *	If the viewport has changed vertically (i.e. the first or
 *      last line numbers have changed), move the now out-of-view
 *	slave windows off the viewport.
 *
 *      Potentially many expose events may be generated when the
 *	the individual slave windows are moved and/or resized.
 *	These events need to be ignored.  Since (I think) expose
 * 	events are guaranteed to happen in order, we can bracket
 *	them by sending phony events (via XSendEvent). The phony
 *      events turn on and off flags indicating which events
 *	should be ignored.
 *
 *	Finally, the page drawing routine is called.
 *
 * Results:
 *	None.
 *
 * Side effects:
 * 	Commands are output to X to display the hypertext in its
 *	current mode.
 *
 * ----------------------------------------------------------------------
 */
static void
DisplayText(clientData)
    ClientData clientData;	/* Information about widget. */
{
    HText *textPtr = (HText *)clientData;
    Tk_Window tkwin = textPtr->tkwin;
    int oldFirst;		/* First line of old viewport */
    int oldLast;		/* Last line of old viewport */
    int deltaY;			/* Change in viewport in Y direction */
    int reqWidth, reqHeight;

    textPtr->flags &= ~REDRAW_PENDING;
    if (tkwin == NULL) {
	return;			/* Window has been destroyed */
    }
    if (textPtr->flags & REQUEST_LAYOUT) {
	/*
	 * Recompute the layout when slaves are created, deleted,
	 * moved, or resized.  Also when text attributes (such as
	 * font, linespacing) have changed.
	 */
	ComputeLayout(textPtr);
    }
    textPtr->lastWidth = Tk_Width(tkwin);
    textPtr->lastHeight = Tk_Height(tkwin);

    /*
     * Check the requested width and height.  We allow two modes:
     * 	1) If the user requested value is greater than zero, use it.
     *  2) Otherwise, let the window be as big as the virtual text.
     *	   This could be too large to display, so constrain it by
     *	   the maxWidth and maxHeight values.
     *
     * In any event, we need to calculate the size of the virtual
     * text and then make a geometry request.  This is so that slave
     * windows whose size is relative to the master, will be set
     * once.
     */
    if (textPtr->reqWidth > 0) {
	reqWidth = textPtr->reqWidth;
    } else {
	reqWidth = BLT_MIN(textPtr->virtText.width, textPtr->maxWidth);
	if (reqWidth < 1) {
	    reqWidth = 1;
	}
    }
    if (textPtr->reqHeight > 0) {
	reqHeight = textPtr->reqHeight;
    } else {
	reqHeight = BLT_MIN(textPtr->virtText.height, textPtr->maxHeight);
	if (reqHeight < 1) {
	    reqHeight = 1;
	}
    }
    if ((reqWidth != Tk_ReqWidth(tkwin)) || (reqHeight != Tk_ReqHeight(tkwin))) {
#ifdef notdef
	fprintf(stderr,"geometry request %dx%d => %dx%d\n", reqWidth, reqHeight,
	    Tk_ReqWidth(tkwin), Tk_ReqHeight(tkwin));
#endif
	Tk_GeometryRequest(tkwin, reqWidth, reqHeight);

	EventuallyRedraw(textPtr);
	return;			/* Try again with new geometry */
    }
    if (!Tk_IsMapped(tkwin)) {
	return;
    }
    /*
     * Turn off layout requests here, after the text window has been
     * mapped.  Otherwise, relative slave window size requests wrt
     * to the size of parent text window will be wrong.
     */
    textPtr->flags &= ~REQUEST_LAYOUT;

    /* Is there a pending goto request? */
    if (textPtr->flags & GOTO_PENDING) {
	textPtr->pendingY = textPtr->lineArr[textPtr->reqLineNum].offset;
	textPtr->flags &= ~GOTO_PENDING;
    }
    deltaY = textPtr->pendingY - textPtr->viewOrigin.y;
    oldFirst = textPtr->first, oldLast = textPtr->last;

    /*
     * If the viewport has changed size or position, or the text
     * and/or slave windows have changed, adjust the scrollbars to
     * new positions.
     */
    if (textPtr->flags & TEXT_DIRTY) {
	/* Reset viewport origin and world extents */
	textPtr->viewOrigin.x = textPtr->pendingX;
	textPtr->viewOrigin.y = textPtr->pendingY;
	if (textPtr->xScrollCmd != NULL)
	    UpdateScrollbar(textPtr->interp, textPtr->xScrollCmd,
		textPtr->virtText.width, Tk_Width(tkwin), textPtr->viewOrigin.x,
		textPtr->scrollX);
	if (textPtr->yScrollCmd != NULL)
	    UpdateScrollbar(textPtr->interp, textPtr->yScrollCmd,
		textPtr->virtText.height, Tk_Height(tkwin),
		textPtr->viewOrigin.y, textPtr->scrollY);
	/*
	 * Given a new viewport or text height, find the first and
	 * last line numbers of the new viewport.
	 */
	if (GetVisibleLines(textPtr) != TCL_OK) {
	    return;
	}
    }
    /*
     * 	This is a kludge: Send an expose event before and after
     * 	drawing the page of text.  Since moving and resizing of the
     * 	slave windows will cause redundant expose events in the parent
     * 	window, the phony events will bracket them indicating no
     * 	action should be taken.
     */
    SendBogusEvent(tkwin);

    /*
     * If either the position of the viewport has changed or the size
     * of width or height of the entire text have changed, move the
     * slaves from the previous viewport out of the current
     * viewport. Worry only about the vertical slave window movements.
     * The page is always draw at full width and the viewport will clip
     * the text.
     */
    if ((textPtr->first != oldFirst) || (textPtr->last != oldLast)) {
	int offset;
	int i;
	int first, last;
	Blt_ListItem *itemPtr;
	Slave *slavePtr;

	/* Figure out which lines are now out of the viewport */

	if ((textPtr->first > oldFirst) && (textPtr->first <= oldLast)) {
	    first = oldFirst, last = textPtr->first;
	} else if ((textPtr->last < oldLast) && (textPtr->last >= oldFirst)) {
	    first = textPtr->last, last = oldLast;
	} else {
	    first = oldFirst, last = oldLast;
	}

	for (i = first; i <= last; i++) {
	    offset = textPtr->lineArr[i].offset;
	    for (itemPtr = Blt_FirstListItem(&(textPtr->lineArr[i].winList));
		itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
		slavePtr = (Slave *)Blt_GetItemValue(itemPtr);
		if (slavePtr->tkwin != NULL) {
		    MoveSlave(slavePtr, offset);
		    slavePtr->flags &= ~SLAVE_VISIBLE;
		}
	    }
	}
    }
    DrawPage(textPtr, deltaY);
    SendBogusEvent(tkwin);

    /* Reset flags */
    textPtr->flags &= ~TEXT_DIRTY;
}

/* Selection Procedures */
/*
 *----------------------------------------------------------------------
 *
 * TextSelectionProc --
 *
 *	This procedure is called back by Tk when the selection is
 *	requested by someone.  It returns part or all of the selection
 *	in a buffer provided by the caller.
 *
 * Results:
 *	The return value is the number of non-NULL bytes stored
 *	at buffer.  Buffer is filled (or partially filled) with a
 *	NULL-terminated string containing part or all of the selection,
 *	as given by offset and maxBytes.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static int
TextSelectionProc(clientData, offset, buffer, maxBytes)
    ClientData clientData;	/* Information about Text widget. */
    int offset;			/* Offset within selection of first
				 * character to be returned. */
    char *buffer;		/* Location in which to place
				 * selection. */
    int maxBytes;		/* Maximum number of bytes to place
				 * at buffer, not including terminating
				 * NULL character. */
{
    HText *textPtr = (HText *)clientData;
    int size;

    if ((textPtr->selectFirst < 0) || (!textPtr->exportSelection)) {
	return -1;
    }
    size = (textPtr->selectLast - textPtr->selectFirst) + 1 - offset;
    if (size > maxBytes) {
	size = maxBytes;
    }
    if (size <= 0) {
	return 0;		/* huh? */
    }
    strncpy(buffer, textPtr->charArr + textPtr->selectFirst + offset, size);
    buffer[size] = '\0';
    return (size);
}

/*
 *----------------------------------------------------------------------
 *
 * TextLostSelection --
 *
 *	This procedure is called back by Tk when the selection is
 *	grabbed away from a Text widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The existing selection is unhighlighted, and the window is
 *	marked as not containing a selection.
 *
 *----------------------------------------------------------------------
 */
static void
TextLostSelection(clientData)
    ClientData clientData;	/* Information about Text widget. */
{
    HText *textPtr = (HText *)clientData;

    if ((textPtr->selectFirst >= 0) && (textPtr->exportSelection)) {
	textPtr->selectFirst = textPtr->selectLast = -1;
	EventuallyRedraw(textPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SelectLine --
 *
 *	Modify the selection by moving both its anchored and un-anchored
 *	ends.  This could make the selection either larger or smaller.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
static int
SelectLine(textPtr, textIndex)
    HText *textPtr;		/* Information about widget. */
    int textIndex;		/* Index of element that is to
				 * become the "other" end of the
				 * selection. */
{
    int selectFirst, selectLast;
    int lineNum;
    Line *linePtr;

    lineNum = IndexSearch(textPtr, textIndex, 0, textPtr->numLines - 1);
    if (lineNum < 0) {
	char string[200];

	sprintf(string, "can't determine line number from index \"%d\"",
	    textIndex);
	Tcl_AppendResult(textPtr->interp, string, (char *)NULL);
	return TCL_ERROR;
    }
    linePtr = textPtr->lineArr + lineNum;
    /*
     * Grab the selection if we don't own it already.
     */
    if ((textPtr->exportSelection) && (textPtr->selectFirst == -1)) {
	Tk_OwnSelection(textPtr->tkwin, XA_PRIMARY,
	    (Tk_LostSelProc*)TextLostSelection, (ClientData)textPtr);
    }
    selectFirst = linePtr->textStart;
    selectLast = linePtr->textEnd;
    textPtr->selectAnchor = textIndex;
#ifdef notdef
    fprintf(stderr, "selection first=%d,last=%d\n", selectFirst, selectLast);
#endif
    if ((textPtr->selectFirst != selectFirst) ||
	(textPtr->selectLast != selectLast)) {
	textPtr->selectFirst = selectFirst;
	textPtr->selectLast = selectLast;
	EventuallyRedraw(textPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectWord --
 *
 *	Modify the selection by moving both its anchored and un-anchored
 *	ends.  This could make the selection either larger or smaller.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
static int
SelectWord(textPtr, textIndex)
    HText *textPtr;		/* Information about widget. */
    int textIndex;		/* Index of element that is to
				 * become the "other" end of the
				 * selection. */
{
    int selectFirst, selectLast;
    int i;

    for (i = textIndex; i < textPtr->numChars; i++) {
	if (isspace(textPtr->charArr[i])) {
	    break;
	}
    }
    selectLast = i - 1;
    for (i = textIndex; i >= 0; i--) {
	if (isspace(textPtr->charArr[i])) {
	    break;
	}
    }
    selectFirst = i + 1;
    if (selectFirst > selectLast) {
	selectFirst = selectLast = textIndex;
    }
    /*
     * Grab the selection if we don't own it already.
     */
    if ((textPtr->exportSelection) && (textPtr->selectFirst == -1)) {
	Tk_OwnSelection(textPtr->tkwin, XA_PRIMARY,
	    (Tk_LostSelProc*)TextLostSelection, (ClientData)textPtr);
    }
    textPtr->selectAnchor = textIndex;
#ifdef notdef
    fprintf(stderr, "selection first=%d,last=%d\n", selectFirst, selectLast);
#endif
    if ((textPtr->selectFirst != selectFirst) ||
	(textPtr->selectLast != selectLast)) {
	textPtr->selectFirst = selectFirst;
	textPtr->selectLast = selectLast;
	EventuallyRedraw(textPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectTextBlock --
 *
 *	Modify the selection by moving its un-anchored end.  This
 *	could make the selection either larger or smaller.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
static int
SelectTextBlock(textPtr, textIndex)
    HText *textPtr;		/* Information about widget. */
    int textIndex;		/* Index of element that is to
				 * become the "other" end of the
				 * selection. */
{
    int selectFirst, selectLast;

    /*
     * Grab the selection if we don't own it already.
     */

    if ((textPtr->exportSelection) && (textPtr->selectFirst == -1)) {
	Tk_OwnSelection(textPtr->tkwin, XA_PRIMARY,
	    (Tk_LostSelProc*)TextLostSelection, (ClientData)textPtr);
    }
    /*  If the anchor hasn't been set yet, assume the beginning of the text*/
    if (textPtr->selectAnchor < 0) {
	textPtr->selectAnchor = 0;
    }
    if (textPtr->selectAnchor <= textIndex) {
	selectFirst = textPtr->selectAnchor;
	selectLast = textIndex;
    } else {
	selectFirst = textIndex;
	selectLast = textPtr->selectAnchor;
    }
#ifdef notdef
    fprintf(stderr, "selection first=%d,last=%d\n", selectFirst, selectLast);
#endif
    if ((textPtr->selectFirst != selectFirst) ||
	(textPtr->selectLast != selectLast)) {
	textPtr->selectFirst = selectFirst;
	textPtr->selectLast = selectLast;
	EventuallyRedraw(textPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectOper --
 *
 *	This procedure handles the individual options for text
 *	selections.  The selected text is designated by start and end
 *	indices into the text pool.  The selected segment has both a
 *	anchored and unanchored ends.  The following selection
 *	operations are implemented:
 *
 *	  "adjust"	- resets either the first or last index
 *			  of the selection.
 *	  "clear"	- clears the selection. Sets first/last
 *			  indices to -1.
 *	  "from"	- sets the index of the selection anchor.
 *	  "line"	- sets the first of last indices to the
 *			  start and end of the line at the
 *			  designated point.
 *	  "present"	- return "1" if a selection is available,
 *			  "0" otherwise.
 *	  "range"	- sets the first and last indices.
 *	  "to"		- sets the index of the un-anchored end.
 *	  "word"	- sets the first of last indices to the
 *			  start and end of the word at the
 *			  designated point.
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */
static int
SelectOper(textPtr, interp, argc, argv)
    HText *textPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int selectIndex;
    unsigned int length;
    int result = TCL_OK;
    char c;

    length = strlen(argv[2]);
    c = argv[2][0];
    if ((c == 'c') && (strncmp(argv[2], "clear", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " selection clear\"", (char *)NULL);
	    return TCL_ERROR;
	}
	if (textPtr->selectFirst != -1) {
	    textPtr->selectFirst = textPtr->selectLast = -1;
	    EventuallyRedraw(textPtr);
	}
	return TCL_OK;
    } else if ((c == 'p') && (strncmp(argv[2], "present", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " selection present\"", (char *)NULL);
	    return TCL_ERROR;
	}
	Tcl_AppendResult(interp, (textPtr->selectFirst != -1) ? "0" : "1",
	    (char *)NULL);
	return TCL_OK;
    } else if ((c == 'r') && (strncmp(argv[2], "range", length) == 0)) {
	int selectFirst, selectLast;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " selection range first last\"", (char *)NULL);
	    return TCL_ERROR;
	}
	if (GetIndex(textPtr, argv[3], &selectFirst) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (GetIndex(textPtr, argv[4], &selectLast) != TCL_OK) {
	    return TCL_ERROR;
	}
	textPtr->selectAnchor = selectFirst;
	result = SelectTextBlock(textPtr, selectLast);
	return TCL_OK;
    }
    if (argc != 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
	    argv[0], " selection ", argv[2], " index\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (GetIndex(textPtr, argv[3], &selectIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((c == 'f') && (strncmp(argv[2], "from", length) == 0)) {
	textPtr->selectAnchor = selectIndex;
    } else if ((c == 'a') && (strncmp(argv[2], "adjust", length) == 0)) {
	int half1, half2;

	half1 = (textPtr->selectFirst + textPtr->selectLast) / 2;
	half2 = (textPtr->selectFirst + textPtr->selectLast + 1) / 2;
	if (selectIndex < half1) {
	    textPtr->selectAnchor = textPtr->selectLast;
	} else if (selectIndex > half2) {
	    textPtr->selectAnchor = textPtr->selectFirst;
	}
	result = SelectTextBlock(textPtr, selectIndex);
    } else if ((c == 't') && (strncmp(argv[2], "to", length) == 0)) {
	result = SelectTextBlock(textPtr, selectIndex);
    } else if ((c == 'w') && (strncmp(argv[2], "word", length) == 0)) {
	result = SelectWord(textPtr, selectIndex);
    } else if ((c == 'l') && (strncmp(argv[2], "line", length) == 0)) {
	result = SelectLine(textPtr, selectIndex);
    } else {
	Tcl_AppendResult(interp, "bad selection operation \"", argv[2],
	    "\": should be \"adjust\", \"clear\", \"from\", \"line\", ",
	    "\"present\", \"range\", \"to\", or \"word\"", (char *)NULL);
	return TCL_ERROR;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GotoOper --
 *
 *	Move the top line of the viewport to the new location based
 *	upon the given line number.  Force out-of-range requests to the
 *	top or bottom of text.
 *
 * Results:
 *	A standard Tcl result. If TCL_OK, interp->result contains the
 *	current line number.
 *
 * Side effects:
 *	At the next idle point, the text viewport will be move to the
 *	new line.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
GotoOper(textPtr, interp, argc, argv)
    HText *textPtr;
    Tcl_Interp *interp;		/* not used */
    int argc;
    char **argv;
{
    int line;
    char tmpString[10];

    line = textPtr->first;
    if (argc == 3) {
	int textIndex;

	if (GetIndex(textPtr, argv[2], &textIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	line = IndexSearch(textPtr, textIndex, 0, textPtr->numLines - 1);
	if (line < 0) {
	    char string[200];

	    sprintf(string, "can't determine line number from index \"%d\"",
		textIndex);
	    Tcl_AppendResult(textPtr->interp, string, (char *)NULL);
	    return TCL_ERROR;
	}
	textPtr->reqLineNum = line;
	textPtr->flags |= TEXT_DIRTY;

	/*
	 * Make only a request for a change in the viewport.  Defer
	 * the actual scrolling until the text layout is adjusted at
	 * the next idle point.
	 */
	if (line != textPtr->first) {
	    textPtr->flags |= GOTO_PENDING;
	    EventuallyRedraw(textPtr);
	}
    }
    sprintf(tmpString, "%d", line);
    Tcl_SetResult(textPtr->interp, tmpString, TCL_VOLATILE);
    return TCL_OK;
}

static int
XViewOper(textPtr, interp, argc, argv)
    HText *textPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int x;
    int type, count;
    double fraction;
    char tmpString[10];

    x = textPtr->viewOrigin.x;
    if (argc == 3) {
	if (Tk_GetPixels(interp, textPtr->tkwin, argv[2], &x) != TCL_OK) {
	    return TCL_ERROR;
	}
	x *= textPtr->scrollX;	/* Convert to pixels */
    } else if (argc >= 4) {
	type = Tk_GetScrollInfo(interp, argc, argv, &fraction, &count);
	switch (type) {
	    case TK_SCROLL_ERROR:
	        return TCL_ERROR;
	    case TK_SCROLL_MOVETO:
	        if (fraction > 1.0) {
		    fraction = 1.0;
		} else if (fraction < 0) {
		    fraction = 0;
		}
		x = (int) (0.5 +
		    fraction * (textPtr->virtText.width + textPtr->scrollX));
		break;
	    case TK_SCROLL_PAGES:
	        x += count * Tk_Width(textPtr->tkwin);
		break;
	    case TK_SCROLL_UNITS:
	    	x += count * textPtr->scrollX;
		break;
	}
    }
    if (argc != 2) {
	if (x > textPtr->virtText.width) {
	    x = textPtr->virtText.width - 1;
	} else if (x < 0) {
	    x = 0;
	}
	if (x != textPtr->viewOrigin.x) {
	    textPtr->pendingX = x;
	    textPtr->flags |= TEXT_DIRTY;
	    EventuallyRedraw(textPtr);
	}
    }

    sprintf(tmpString, "%d", x / textPtr->scrollX);
    Tcl_SetResult(interp, tmpString, TCL_VOLATILE);
    return TCL_OK;
}


static int
YViewOper(textPtr, interp, argc, argv)
    HText *textPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int y;
    int type, count;
    double fraction;
    char tmpString[10];

    y = textPtr->viewOrigin.y;
    if (argc == 3) {
	if (Tk_GetPixels(interp, textPtr->tkwin, argv[2], &y) != TCL_OK) {
	    return TCL_ERROR;
	}
	y *= textPtr->scrollY;	/* Convert to pixels */
    } else if (argc >= 4) {
	type = Tk_GetScrollInfo(interp, argc, argv, &fraction, &count);
	switch (type) {
	    case TK_SCROLL_ERROR:
	        return TCL_ERROR;
	    case TK_SCROLL_MOVETO:
	        if (fraction > 1.0) {
		    fraction = 1.0;
		} else if (fraction < 0) {
		    fraction = 0;
		}
		y = (int) (0.5 +
		    fraction * (textPtr->virtText.height + textPtr->scrollY));
		break;
	    case TK_SCROLL_PAGES:
	        y += count * Tk_Height(textPtr->tkwin);
		break;
	    case TK_SCROLL_UNITS:
	    	y += count * textPtr->scrollY;
		break;
	}
    }
    if (argc != 2) {
	if (y > textPtr->virtText.height) {
	    y = textPtr->virtText.height - 1;
	} else if (y < 0) {
	    y = 0;
	}
	if (y != textPtr->viewOrigin.y) {
	    textPtr->pendingY = y;
	    textPtr->flags |= TEXT_DIRTY;
	    EventuallyRedraw(textPtr);
	}
    }

    sprintf(tmpString, "%d", y / textPtr->scrollY);
    Tcl_SetResult(interp, tmpString, TCL_VOLATILE);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * AppendOper --
 *
 * 	This procedure creates and initializes a new hyper text slave.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side effects:
 *	Memory is allocated.  Slave gets configured.
 *
 * ----------------------------------------------------------------------
 */
static int
AppendOper(textPtr, interp, argc, argv)
    HText *textPtr;		/* Hypertext widget */
    Tcl_Interp *interp;		/* Interpreter associated with widget */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Line *linePtr;
    Slave *slavePtr;
    Blt_ListItem *itemPtr;

    slavePtr = CreateSlave(textPtr, argv[2]);
    if (slavePtr == NULL) {
	return TCL_ERROR;
    }
    if (Tk_ConfigureWidget(interp, textPtr->tkwin, slaveConfigSpecs,
	    argc - 3, argv + 3, (char *)slavePtr, 0) != TCL_OK) {
	return TCL_ERROR;
    }
    /*
     * Append slave to list of slave windows of the last line.
     */
    linePtr = GetLastLine(textPtr);
    if (linePtr == NULL) {
	Tcl_SetResult(textPtr->interp, "can't allocate line structure",
	    TCL_STATIC);
	return TCL_ERROR;
    }
    itemPtr = Blt_NewItem((char *)slavePtr->tkwin);
    Blt_LinkAfter(&(linePtr->winList), itemPtr, (Blt_ListItem *)NULL);
    Blt_SetItemValue(itemPtr, (ClientData)slavePtr);
    linePtr->width += slavePtr->cavityWidth;
    slavePtr->precedingTextEnd = linePtr->textEnd;

    textPtr->flags |= (SLAVE_APPENDED | REQUEST_LAYOUT);
    EventuallyRedraw(textPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WindowsOper --
 *
 *	Returns a list of all the pathNames of slave windows of the
 *	HText widget.  If a pattern argument is given, only the names
 *	of windows matching it will be placed into the list.
 *
 * Results:
 *	Standard Tcl result.  If TCL_OK, interp->result will contain
 *	the list of the slave window pathnames.  Otherwise it will
 *	contain an error message.
 *
 *----------------------------------------------------------------------
 */
static int
WindowsOper(textPtr, interp, argc, argv)
    HText *textPtr;		/* Hypertext widget record */
    Tcl_Interp *interp;		/* Interpreter associated with widget */
    int argc;
    char **argv;
{
    Slave *slavePtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;
    char *name;

    for (hPtr = Tcl_FirstHashEntry(&(textPtr->slaves), &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	slavePtr = (Slave *)Tcl_GetHashValue(hPtr);
	if (slavePtr->tkwin == NULL) {
	    fprintf(stderr, "window `%s' is null\n",
		Tk_PathName(Tcl_GetHashKey(&(textPtr->slaves), hPtr)));
	    continue;
	}
	name = Tk_PathName(slavePtr->tkwin);
	if ((argc == 2) || (Tcl_StringMatch(name, argv[2]))) {
	    Tcl_AppendElement(interp, name);
	}
    }
    return TCL_OK;
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
CgetOper(textPtr, interp, argc, argv)
    HText *textPtr;
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    char *itemPtr;
    Tk_ConfigSpec *specsPtr;

    if ((argc > 2) && (argv[2][0] == '.')) {
	Tk_Window tkwin;
	Slave *slavePtr;

	/* Slave window to be configured */
	tkwin = Tk_NameToWindow(interp, argv[2], textPtr->tkwin);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	slavePtr = FindSlave(textPtr, tkwin);
	if (slavePtr == NULL) {
	    Tcl_AppendResult(interp, "window \"", argv[2],
		"\" is not managed by \"", argv[0], "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	specsPtr = slaveConfigSpecs;
	itemPtr = (char *)slavePtr;
	argv++;
	argc--;
    } else {
	specsPtr = configSpecs;
	itemPtr = (char *)textPtr;
    }
    return (Tk_ConfigureValue(interp, textPtr->tkwin, specsPtr, itemPtr,
	    argv[2], 0));
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOper --
 *
 * 	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a hypertext widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 * 	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 * 	etc. get set for textPtr;  old resources get freed, if there were any.
 * 	The hypertext is redisplayed.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOper(textPtr, interp, argc, argv)
    HText *textPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char *itemPtr;
    Tk_ConfigSpec *specsPtr;

    if ((argc > 2) && (argv[2][0] == '.')) {
	Tk_Window tkwin;
	Slave *slavePtr;

	/* Slave window to be configured */
	tkwin = Tk_NameToWindow(interp, argv[2], textPtr->tkwin);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	slavePtr = FindSlave(textPtr, tkwin);
	if (slavePtr == NULL) {
	    Tcl_AppendResult(interp, "window \"", argv[2],
		"\" is not managed by \"", argv[0], "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	specsPtr = slaveConfigSpecs;
	itemPtr = (char *)slavePtr;
	argv++;
	argc--;
    } else {
	specsPtr = configSpecs;
	itemPtr = (char *)textPtr;
    }
    if (argc == 2) {
	return (Tk_ConfigureInfo(interp, textPtr->tkwin, specsPtr, itemPtr,
		(char *)NULL, 0));
    } else if (argc == 3) {
	return (Tk_ConfigureInfo(interp, textPtr->tkwin, specsPtr, itemPtr,
		argv[2], 0));
    }
    if (Tk_ConfigureWidget(interp, textPtr->tkwin, specsPtr, argc - 2,
	    argv + 2, itemPtr, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    if (itemPtr == (char *)textPtr) {
	/* Reconfigure the master */
	if (ConfigureText(interp, textPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	textPtr->flags |= REQUEST_LAYOUT;
    }
    EventuallyRedraw(textPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ScanOper --
 *
 *	Implements the quick scan for hypertext widgets.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ScanOper(textPtr, interp, argc, argv)
    HText *textPtr;
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    XPoint winPos;
    char c;
    unsigned int length;


    if (Blt_GetXYPosition(interp, argv[3], &winPos) != TCL_OK) {
	return TCL_ERROR;
    }
    c = argv[2][0];
    length = strlen(argv[2]);
    if ((c == 'm') && (strncmp(argv[2], "mark", length) == 0)) {
	textPtr->scanMark.x = winPos.x, textPtr->scanMark.y = winPos.y;
	textPtr->scanPt.x = textPtr->viewOrigin.x;
	textPtr->scanPt.y = textPtr->viewOrigin.y;

    } else if ((c == 'd') && (strncmp(argv[2], "dragto", length) == 0)) {
	int x, y;

	x = textPtr->scanPt.x - (10 * (winPos.x - textPtr->scanMark.x));
	y = textPtr->scanPt.y - (10 * (winPos.y - textPtr->scanMark.y));

	if (x < 0) {
	    x = textPtr->scanPt.x = 0;
	    textPtr->scanMark.x = winPos.x;
	} else if (x >= textPtr->virtText.width) {
	    x = textPtr->scanPt.x = textPtr->virtText.width - textPtr->scrollX;
	    textPtr->scanMark.x = winPos.x;
	}
	if (y < 0) {
	    y = textPtr->scanPt.y = 0;
	    textPtr->scanMark.y = winPos.y;
	} else if (y >= textPtr->virtText.height) {
	    y = textPtr->scanPt.y = textPtr->virtText.height - textPtr->scrollY;
	    textPtr->scanMark.y = winPos.y;
	}
	if ((y != textPtr->pendingY) || (x != textPtr->pendingX)) {
	    textPtr->pendingX = x, textPtr->pendingY = y;
	    textPtr->flags |= TEXT_DIRTY;
	    EventuallyRedraw(textPtr);
	}
    } else {
	Tcl_AppendResult(interp, "bad scan operation \"", argv[2],
	    "\": should be either \"mark\" or \"dragto\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SearchOper --
 *
 *	Returns the linenumber of the next line matching the given
 *	pattern within the range of lines provided.  If the first
 *	line number is greater than the last, the search is done in
 *	reverse.
 *
 *----------------------------------------------------------------------
 */
static int
SearchOper(textPtr, interp, argc, argv)
    HText *textPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char *startPtr, *endPtr;
    char saved;
    Tcl_RegExp regExpToken;
    int searchFirst, searchLast;
    int matchStart, matchEnd;
    int match;

    regExpToken = Tcl_RegExpCompile(interp, argv[2]);
    if (regExpToken == NULL) {
	return TCL_ERROR;
    }
    searchFirst = 0;
    searchLast = textPtr->numChars;
    if (argc > 3) {
	if (GetIndex(textPtr, argv[3], &searchFirst) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (argc == 4) {
	if (GetIndex(textPtr, argv[4], &searchLast) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (searchLast < searchFirst) {
	return TCL_ERROR;
    }
    matchStart = matchEnd = -1;
    startPtr = textPtr->charArr + searchFirst;
    endPtr = textPtr->charArr + searchLast + 1;
    saved = *endPtr;
    *endPtr = '\0';		/* Make the line a string by changing the
				 * '\n' into a NUL byte before searching */
    match = Tcl_RegExpExec(interp, regExpToken, startPtr, startPtr);
    *endPtr = saved;
    if (match < 0) {
	return TCL_ERROR;
    } else if (match > 0) {
	Tcl_RegExpRange(regExpToken, 0, &startPtr, &endPtr);
	if ((startPtr != NULL) || (endPtr != NULL)) {
	    matchStart = startPtr - textPtr->charArr;
	    matchEnd = (endPtr - textPtr->charArr - 1);
	}
    }
    if (match > 0) {
	char tmpString[30];

	sprintf(tmpString, "%d %d", matchStart, matchEnd);
	Tcl_SetResult(interp, tmpString, TCL_VOLATILE);
    } else {
	Tcl_ResetResult(interp);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RangeOper --
 *
 *	Returns the characters designated by the range of elements.
 *
 *----------------------------------------------------------------------
 */
static int
RangeOper(textPtr, interp, argc, argv)
    HText *textPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char *startPtr, *endPtr;
    char saved;
    int textFirst, textLast;

    textFirst = textPtr->selectFirst;
    textLast = textPtr->selectLast;
    if (textFirst < 0) {
	textFirst = 0;
	textLast = textPtr->numChars - 1;
    }
    if (argc > 2) {
	if (GetIndex(textPtr, argv[2], &textFirst) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (argc == 4) {
	if (GetIndex(textPtr, argv[3], &textLast) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (textLast < textFirst) {
	Tcl_AppendResult(interp, "first index is greater than last",
	    (char *)NULL);
	return TCL_ERROR;
    }
    startPtr = textPtr->charArr + textFirst;
    endPtr = textPtr->charArr + textLast + 1;
    saved = *endPtr;
    *endPtr = '\0';		/* Make the line into a string by
				 * changing the * '\n' into a '\0'
				 * before copying */
    Tcl_SetResult(interp, startPtr, TCL_VOLATILE);
    *endPtr = saved;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IndexOper --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
IndexOper(textPtr, interp, argc, argv)
    HText *textPtr;
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    int textIndex;
    char tmpString[10];

    if (GetIndex(textPtr, argv[2], &textIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    sprintf(tmpString, "%d", textIndex);
    Tcl_SetResult(interp, tmpString, TCL_VOLATILE);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LinePosOper --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
LinePosOper(textPtr, interp, argc, argv)
    HText *textPtr;
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    int lineNum, charPos, textIndex;
    char tmpString[30];

    if (GetIndex(textPtr, argv[2], &textIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    if (GetTextPosition(textPtr, textIndex, &lineNum, &charPos) != TCL_OK) {
	return TCL_ERROR;
    }
    sprintf(tmpString, "%d.%d", lineNum, charPos);
    Tcl_SetResult(interp, tmpString, TCL_VOLATILE);
    return TCL_OK;
}

/*
 * --------------------------------------------------------------
 *
 * TextWidgetCmd --
 *
 * 	This procedure is invoked to process the Tcl command that
 *	corresponds to a widget managed by this module. See the user
 * 	documentation for details on what it does.
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
    {"append", 1, (Blt_OperProc) AppendOper, 3, 0, "window ?option value?...",},
    {"cget", 2, (Blt_OperProc) CgetOper, 3, 3, "?window? option",},
    {"configure", 2, (Blt_OperProc) ConfigureOper, 2, 0,
	"?window? ?option value?...",},
    {"gotoline", 2, (Blt_OperProc) GotoOper, 2, 3, "?line?",},
    {"index", 1, (Blt_OperProc) IndexOper, 3, 3, "string",},
    {"linepos", 1, (Blt_OperProc) LinePosOper, 3, 3, "string",},
    {"range", 2, (Blt_OperProc) RangeOper, 2, 4, "?from? ?to?",},
    {"scan", 2, (Blt_OperProc) ScanOper, 4, 4, "oper @x,y",},
    {"search", 3, (Blt_OperProc) SearchOper, 3, 5, "pattern ?from? ?to?",},
    {"selection", 3, (Blt_OperProc) SelectOper, 3, 5, "oper ?index?",},
    {"windows", 6, (Blt_OperProc) WindowsOper, 2, 3, "?pattern?",},
    {"xview", 1, (Blt_OperProc) XViewOper, 2, 5, "?pos?",},
    {"yview", 1, (Blt_OperProc) YViewOper, 2, 5, "?pos?",},
};
static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

static int
TextWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about hypertext widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Blt_OperProc proc;
    int result;
    HText *textPtr = (HText *)clientData;

    proc = Blt_LookupOperation(interp, numSpecs, operSpecs, BLT_OPER_ARG1,
	argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData)textPtr);
    result = (*proc) (textPtr, interp, argc, argv);
    Tcl_Release((ClientData)textPtr);
    return (result);
}

/*
 * --------------------------------------------------------------
 *
 * TextCmd --
 *
 * 	This procedure is invoked to process the "htext" Tcl command.
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
/* ARGSUSED */
static int
TextCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    HText *textPtr;
    Tk_Window mainWindow = (Tk_Window)clientData;
    Screen *screenPtr;
    Tk_Window tkwin;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " pathName ?option value?...\"", (char *)NULL);
	return TCL_ERROR;
    }
    textPtr = (HText *)ckcalloc(1, sizeof(HText));
    if (textPtr == NULL) {
	Panic("can't allocate htext structure");
	return TCL_ERROR;
    }
    tkwin = Tk_CreateWindowFromPath(interp, mainWindow, argv[1], (char *)NULL);
    if (tkwin == NULL) {
	ckfree((char*)textPtr);
	return TCL_ERROR;
    }
    /* Initialize the new hypertext widget */

    Tk_SetClass(tkwin, "Htext");
    textPtr->tkwin = tkwin;
    textPtr->display = Tk_Display(tkwin);
    textPtr->interp = interp;
    textPtr->numLines = textPtr->arraySize = 0;
    textPtr->lineSpacing = 1;
    textPtr->scrollX = textPtr->scrollY = 10;
    textPtr->numRows = textPtr->numColumns = 0;
    textPtr->selectFirst = textPtr->selectLast = -1;
    textPtr->selectAnchor = 0;
    textPtr->exportSelection = TRUE;
    textPtr->selectBW = 2;
    screenPtr = Tk_Screen(textPtr->tkwin);
    textPtr->maxWidth = WidthOfScreen(screenPtr);
    textPtr->maxHeight = HeightOfScreen(screenPtr);
    Tcl_InitHashTable(&(textPtr->slaves), TCL_ONE_WORD_KEYS);

    Tk_CreateSelHandler(tkwin, XA_PRIMARY, XA_STRING,
	(Tk_SelectionProc *)TextSelectionProc, (ClientData)textPtr, XA_STRING);
    Tk_CreateEventHandler(tkwin, ExposureMask | StructureNotifyMask,
	(Tk_EventProc*)TextEventProc, (ClientData)textPtr);

    /*
     * -----------------------------------------------------------------
     *
     *  Create the widget command before configuring the widget. This
     *  is because the "-file" and "-text" options may have embedded
     *  commands that self-reference the widget through the
     *  "$blt_htext(widget)" variable.
     *
     * ------------------------------------------------------------------
     */
    textPtr->cmdToken = Tcl_CreateCommand(interp, argv[1],
	(Tcl_CmdProc*)TextWidgetCmd, (ClientData)textPtr,
	(Tcl_CmdDeleteProc*)TextDeleteCmdProc);
#ifdef USE_TK_NAMESPACES
    Itk_SetWidgetCommand(textPtr->tkwin, textPtr->cmdToken);
#endif /* USE_TK_NAMESPACES */
    if ((Tk_ConfigureWidget(interp, textPtr->tkwin, configSpecs, argc - 2,
		argv + 2, (char *)textPtr, 0) != TCL_OK) ||
	(ConfigureText(interp, textPtr) != TCL_OK)) {
	Tk_DestroyWindow(textPtr->tkwin);
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, Tk_PathName(textPtr->tkwin), TCL_STATIC);

    return TCL_OK;
}

int
Blt_HtextInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = {
	"htext", HTEXT_VERSION, (Tcl_CmdProc*)TextCmd,
    };

    return (Blt_InitCmd(interp, &cmdSpec));
}
#endif /* !NO_HTEXT */
