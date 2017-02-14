/*
 * bltPie.c --
 *
 *	This module implements a piechart widget for the Tk toolkit.
 *	The code is based on the Dial widget written by Francis Gardner 
 *	and Richard Dearden. Heavily mangled by Julian Anderson. Ported
 *	to Tcl/Tk 8.0 by Michael Schumacher. bltPie.c is derived from
 *	tkPie.c, which is part of the VU package, to which the following
 *	(somewhat strange :-) copyright notice applies:
 *
 * Copyright 1992 Regents of the University of Victoria, Wellington, NZ.
 * This code includes portions of code from the University of Illinois dial
 * widget so the code is copyright by that institution too.
 * [Find and paste Illinois copyright notice]
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include "bltInt.h"
#ifndef NO_PIECHART

#define PIE_CLASS_NAME		"Pie"
#undef PADDING
#define PADDING			2
#define FONTPAD			2
#define REDRAW_PENDING		1
#define CLEAR_NEEDED		2
#define KEY_CHANGED		4
#define M_DEGREE		23040
#define TWOPI			(M_PI * 2)
#define MAX_WEDGES		16
#define fontheight(f)		(TEXTHEIGHT(f)+2*FONTPAD)
#define piewidth(pie)		(  ((pie)->radius + (pie)->borderWidth \
				 + (pie)->explodelength)*2)
#define pieoffset(pie)		(  (pie)->radius + (pie)->borderWidth \
				 + (pie)->explodelength)
#define deg64_2_radians(d)	(TWOPI * ((double)(d) / M_DEGREE))
#define angle_between(d1, d2)	((d2)>(d1) ? (d2)-(d1) : (360-(d1)+(d2)))

/*
 * A data structure of the following type is kept for each
 * widget managed by this file:
 */

unsigned char bitmap[MAX_WEDGES][8] = {
  { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 }, /* Grey 1/2 */
  { 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00 }, /* Grey 1/4 */
  { 0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00 }, /* Grey 4/25 */
  { 0xff, 0x88, 0x88, 0x88, 0xff, 0x88, 0x88, 0x88 }, /* Squares */
  { 0xff, 0x80, 0x80, 0x80, 0xff, 0x08, 0x08, 0x08 }, /* Brickwork */
  { 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F }, /* Something */
  { 0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11 }, /* ne-sw slope */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* white */
  { 0x08, 0x14, 0x22, 0x41, 0x80, 0x40, 0x20, 0x10 }, /* Diag brick */
  { 0x91, 0x62, 0x02, 0x01, 0x80, 0x40, 0x46, 0x89 }, /* funny square */
  { 0x18, 0x24, 0x42, 0x81, 0x81, 0x42, 0x24, 0x18 }, /* diag squares */
  { 0x84, 0x48, 0x00, 0x00, 0x48, 0x84, 0x00, 0x00 }, /* diag dashes */
  { 0x80, 0x80, 0x41, 0x37, 0x08, 0x08, 0x14, 0x72 }, /* scales */
  { 0xCc, 0x66, 0x33, 0x99, 0xCC, 0x66, 0x33, 0x99 }, /* fat diags */
  { 0xCC, 0xCC, 0x33, 0x33, 0xCC, 0xCC, 0x33, 0x33 }, /* Fat grey 1/2 */
  { 0xCC, 0x66, 0xCC, 0x66, 0xCC, 0x66, 0xCC, 0x66 }
};

char *colors[MAX_WEDGES] = {
  "green",
  "purple",
  "moccasin",
  "red",
  "yellow",
  "SkyBlue",
  "grey75",
  "blue",
  "chocolate",
  "cyan",
  "sea green",
  "gold",
  "salmon",
  "hot pink", 
  "lawn green",
  "black"
};

char usercn[MAX_WEDGES][64];
XColor *usercp[MAX_WEDGES];

struct PieElement {
  char *label;
  char *legend;
  double value;
  GC gc;
};

typedef struct PieElement PieElt;

typedef struct Pie {
    /*
     * General stuff
     */

    Tk_Window tkwin;		/* Window that embodies the pie.  NULL means
				   that the window has been destroyed. */
    Tcl_Interp *interp;		/* Interpreter associated with button. */
    int displaybits;		/* Specifies how much of pie to display. */
    int display_depth;		/* Depth of display. */
    Tk_Cursor cursor;		/* Current cursor for window. */
    
    /*
     * Border stuff
     */

    Tk_3DBorder border;		/* Structure used to draw 3-D border. */
    int borderWidth;		/* Width of border. */
    int relief;			/* 3-d effect: TK_RELIEF_RAISED, etc. */
    XColor *backgroundColor;	/* Background color. */

    /*
     * Pie stuff
     */

    int radius;
    double total_value;		/* Sum of elements. */
    int element_count;		/* How many elements. */
    PieElt elements[MAX_WEDGES];/* The wedges themselves. */
    char *exploded;
    int explodelength;
    unsigned int centerX;	/* Pie center x position. */
    unsigned int centerY;	/* Pie center y position. */

    /*
     * Miscellaneous configuration options.
     */

    int valuewidth;
    int numdigits;
    int textwidth;
    int percentwidth;
    char *keyorder;		/* Permutations of K,L,V,P
				 * for display order. */
    int keysize;
    int origin;			/* Origin for pie elements. */
    
    /*
     * Text stuff
     */

    TkFont *fontPtr;		/* Information about text font, or NULL. */
    XColor *textColorPtr;       /* Color for drawing text. */
    GC textGC;                  /* GC for drawing text. */
    int showvalue;              /* Boolean flag for whether or not to
				   display textual value. */
    char *title;                /* Title for pie - ckalloc()ed. */
    int titleheight;		/* Height of title for geometry computations */

    /*
     *  Callback stuff
     */

    unsigned long interval;	/* Interval (mS) between callbacks. */
    char *command;		/* Command involked during the callback. */
    int continue_callback;	/* Boolean flag used to terminate the
				 * callback. */
    long userbits;		/* A place for user data. */
    char *userdata;
    int cb_key;			/* Callback data key. */
    int cb_loc[MAX_WEDGES];	/* Callback data location. */
    Tk_TimerToken cb_timer;	/* Callback timer token. */
} Pie;

#define DEF_PIE_BG_COLOR	"white"
#define DEF_PIE_BG_MONO		"white"
#define DEF_TEXT_COLOR		"black"
#define DEF_TEXT_MONO		"black"
#define DEF_PIE_BORDER_WIDTH	"2"
#define DEF_PIE_RADIUS		"100"
#define DEF_PIE_FONT		"Helvetica 12 bold"
#define DEF_PIE_RELIEF		"raised"
#define DEF_PIE_VALUEWIDTH	"50"
#define DEF_PIE_PERCENTWIDTH	"60"
#define DEF_PIE_TEXTWIDTH	"50"
#define DEF_PIE_ORDER		"KVPL" /* <K>ey,<V>alue,<P>ercent,<L>egend */
#define DEF_PIE_KEYBOX		"100"
#define DEF_PIE_ORIGIN		"0" /* Vertical */
#define DEF_PIE_EXPLODE		"10"
#define DEF_PIE_CALLBACK_INTERVAL "500"
#define DEF_PIE_NUMDIGITS	"2"

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_PIE_BG_COLOR, Tk_Offset(Pie, border),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_PIE_BG_MONO, Tk_Offset(Pie, border),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_PIE_BG_MONO, Tk_Offset(Pie, backgroundColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-background", "background", "Background",
	DEF_PIE_BG_COLOR, Tk_Offset(Pie, backgroundColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_INT, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_PIE_BORDER_WIDTH, Tk_Offset(Pie, borderWidth), 0},
    {TK_CONFIG_STRING, "-command", "command", "Command",
	(char *)NULL, Tk_Offset(Pie, command), 0},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	(char *)NULL, Tk_Offset(Pie, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-data", "data", "Data",
	(char *) NULL, Tk_Offset(Pie, userdata), 0},
    {TK_CONFIG_STRING, "-explode", "explode", "Explode",
	(char *)NULL, Tk_Offset(Pie,exploded), 0},
    {TK_CONFIG_INT, "-explodewidth", "explodewidth", "Explodewidth",
	DEF_PIE_EXPLODE, Tk_Offset(Pie,explodelength), 0},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_PIE_FONT, Tk_Offset(Pie, fontPtr), 0},
    {TK_CONFIG_INT, "-interval", "interval", "Interval",
	DEF_PIE_CALLBACK_INTERVAL, Tk_Offset(Pie, interval), 0},
    {TK_CONFIG_INT, "-keyboxsize", "keyboxsize", "Keyboxsize",
	DEF_PIE_KEYBOX, Tk_Offset(Pie,keysize), 0},
    {TK_CONFIG_STRING, "-keyorder", "keyorder", "Keyorder",
	DEF_PIE_ORDER, Tk_Offset(Pie, keyorder), 0},
    {TK_CONFIG_INT, "-numdigits", "numdigits", "Numdigits",
	DEF_PIE_NUMDIGITS, Tk_Offset(Pie,numdigits), 0},
    {TK_CONFIG_INT, "-origin", "origin", "Origin",
	DEF_PIE_ORIGIN, Tk_Offset(Pie, origin), 0},
    {TK_CONFIG_INT, "-percentwidth", "percentwidth", "Percentwidth",
	DEF_PIE_PERCENTWIDTH, Tk_Offset(Pie, percentwidth), 0},
    {TK_CONFIG_INT, "-radius", "radius", "Radius", DEF_PIE_RADIUS,
	Tk_Offset(Pie, radius), 0},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_PIE_RELIEF, Tk_Offset(Pie, relief), 0},
    {TK_CONFIG_COLOR, "-textcolor", "textcolor", "Foreground",
	DEF_TEXT_COLOR, Tk_Offset(Pie, textColorPtr),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-textcolor", "textcolor", "Foreground",
	DEF_TEXT_MONO, Tk_Offset(Pie, textColorPtr),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_INT, "-textwidth", "textwidth", "Textwidth",
	DEF_PIE_TEXTWIDTH, Tk_Offset(Pie, textwidth), 0},
    {TK_CONFIG_STRING, "-title", "title", "Title",
	(char *)NULL, Tk_Offset(Pie, title), 0},
    {TK_CONFIG_INT, "-valuewidth", "valuewidth", "Valuewidth",
	DEF_PIE_VALUEWIDTH, Tk_Offset(Pie, valuewidth), 0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations.
 */
static int ConfigurePie _ANSI_ARGS_((Tcl_Interp *interp, register Pie *piePtr,
	int argc, char **argv, int flags));

/*
 *--------------------------------------------------------------
 *
 * DisplayPie --
 *
 *	This procedure takes care of drawing a pie chart widget.
 *
 *--------------------------------------------------------------
 */
static void
DisplayPie(clientData)
    ClientData clientData;
{
    register Pie *piePtr = (Pie *)clientData;
    register Tk_Window tkwin = piePtr->tkwin;
    double foo;
    int fh = fontheight(piePtr->fontPtr);
    double perpt, start;
    double exp_start = 0, exp_diff = 0;
    char *options;
    int i;
    int pWidth, vWidth;
    int y = PADDING + piePtr->titleheight;
    Display *disp = Tk_Display(tkwin);
    Drawable draw = Tk_WindowId(tkwin);
    char explod[80];

    if (   (piePtr->exploded && piePtr->exploded[0])
        || (piePtr->displaybits & CLEAR_NEEDED)) {
	XClearWindow(Tk_Display(tkwin), Tk_WindowId(tkwin));
    }
    if (piePtr->displaybits & KEY_CHANGED) {
	XClearArea(disp, draw, piewidth(piePtr), 0,
	    Tk_Width(piePtr->tkwin) - piewidth(piePtr),
	    Tk_Height(piePtr->tkwin), 0);
    }

    if (piePtr->title && piePtr->title[0]) {
	int tlength = strlen(piePtr->title);
	int twidth = Tk_TextWidth((Tk_Font)piePtr->fontPtr, piePtr->title,
	    tlength);
	int win_width = Tk_Width(tkwin);
	int x;
      
	if (twidth < win_width - 2*piePtr->borderWidth) {
	    x = win_width / 2 - (twidth / 2);
	} else {
	    /* text wider than window - (have X) clip it */
	    x = piePtr->borderWidth + PADDING;
	}
	XDrawString(Tk_Display(tkwin), Tk_WindowId(tkwin),
	    piePtr->textGC, x, fh , piePtr->title, tlength);
    }
  
    piePtr->displaybits = 0;
    if (piePtr->total_value != 0) {
	perpt = (double)M_DEGREE / (double)piePtr->total_value;
    } else {
	perpt = (double)M_DEGREE;
    }
    piePtr->origin = piePtr->origin % 360;
    start = (double)(M_DEGREE / 4) - (double)(piePtr->origin * 64);

    if ((piePtr->element_count == 0) || (piePtr->total_value == 0)) {
	XDrawArc(disp, draw,
	    piePtr->textGC,
	    piePtr->borderWidth + piePtr->explodelength,
	    piePtr->borderWidth + piePtr->explodelength + piePtr->titleheight,
	    piePtr->radius * 2,
	    piePtr->radius * 2,
	    0,
	    M_DEGREE);
	return;
    }

    for (i = 0; i < piePtr->element_count; i++) {
	if (start < 0.0) {
	    start += M_DEGREE;
	} else if (start > M_DEGREE) {
	    start -= M_DEGREE;
	}

	if (piePtr->exploded == 0) {
	    strcpy(explod, "NO9NO9NO");
	} else {
	    strcpy(explod, piePtr->exploded);
        }

	if (strcmp(explod, piePtr->elements[i].label) == 0) {
	    int xoff, yoff;
      
	    if (piePtr->elements[i].value == 0.0) {
		continue;
	    }
	    exp_start = start;
	    exp_diff = -(double)piePtr->elements[i].value * perpt;
	    xoff = piePtr->explodelength * 
		cos(deg64_2_radians((double)(exp_start + exp_diff / 2)));
	    yoff = piePtr->explodelength * 
		sin(deg64_2_radians((double)(exp_start + exp_diff / 2)));
	    XFillArc(disp, draw,
		piePtr->elements[i].gc,
		piePtr->borderWidth + piePtr->explodelength + xoff,
		piePtr->borderWidth + piePtr->explodelength - yoff
		    + piePtr->titleheight,
		piePtr->radius * 2,
		piePtr->radius * 2,
		exp_start,
		exp_diff);
	    XDrawArc(disp, draw,
		piePtr->textGC,
		piePtr->borderWidth + piePtr->explodelength + xoff,
		piePtr->borderWidth + piePtr->explodelength - yoff
		    + piePtr->titleheight,
		piePtr->radius * 2,
		piePtr->radius * 2,
		exp_start,
		exp_diff);
	    start += exp_diff;
	} else {
	    if (piePtr->elements[i].value == 0.0) {
		continue;
	    }
	    foo = -piePtr->elements[i].value * perpt;
	    XFillArc(disp, draw,
		piePtr->elements[i].gc,
		piePtr->borderWidth + piePtr->explodelength,
		piePtr->borderWidth + piePtr->explodelength
		    + piePtr->titleheight,
		piePtr->radius * 2,
		piePtr->radius * 2,
		start,
		foo);
	    if (piePtr->element_count > 1) {
		XDrawLine(disp, draw,
		    piePtr->textGC,
		    pieoffset(piePtr),
		    pieoffset(piePtr) + piePtr->titleheight,
		    pieoffset(piePtr) +
		        (int)(piePtr->radius * cos(deg64_2_radians(start))),
		    pieoffset(piePtr) -
		        (int)(piePtr->radius * sin(deg64_2_radians(start)))
		        + piePtr->titleheight);
	    }
	    start += foo;
	}
    }

    XDrawArc(disp, draw,
	piePtr->textGC,
	piePtr->borderWidth + piePtr->explodelength,
	piePtr->borderWidth + piePtr->explodelength + piePtr->titleheight,
	piePtr->radius * 2,
	piePtr->radius * 2,
	exp_start,
	M_DEGREE + exp_diff);


    pWidth = vWidth = 0;
    for (i = 0; i < piePtr->element_count; i++) {
	int len;
	char fmt[10], str[30];

	sprintf(str, "%.2f%%",
	   100.0 * piePtr->elements[i].value / piePtr->total_value);
	if ((len = strlen (str)) > pWidth) {
	    pWidth = len;
	}
	sprintf(fmt, "%%.%df", piePtr->numdigits);
	sprintf(str, fmt, piePtr->elements[i].value);
	if ((len = strlen (str)) > vWidth) {
	    vWidth = len;
	}
    }

    for (i = 0; i < piePtr->element_count; i++) {
	int height = fontheight(piePtr->fontPtr);
	char valuestr[30], textstr[30];
	int rheight = (height * piePtr->keysize) / 100;
	int x = piewidth(piePtr);

	options = piePtr->keyorder;
	while (*options) {
	    x += PADDING * 2;
	    switch (*options) {
		case 'k': case 'K' : { /* Key element. */
		    XFillRectangle(disp, draw,
			piePtr->elements[i].gc, x, y, rheight, rheight);
		    XDrawRectangle(disp, draw,
			piePtr->textGC, x, y, rheight, rheight);
		    x += rheight;
		    break;
		}
		case 'p': case 'P': { /* Percent element. */
		    sprintf(valuestr, "%.2f%%", 100.0 *
			piePtr->elements[i].value / piePtr->total_value);
		    sprintf (textstr, "%*.*s", pWidth, pWidth, valuestr);
		    XDrawString(disp, draw,
			piePtr->textGC, x, y + rheight - PADDING * 2,
			textstr, pWidth);
		    x += piePtr->percentwidth;
		    break;
		}
		case 'v': case 'V': { /* Value element. */
		    char formatstr[10];

		    sprintf(formatstr, "%%.%df", piePtr->numdigits);
		    sprintf(valuestr, formatstr, piePtr->elements[i].value);
		    sprintf (textstr, "%*.*s", vWidth, vWidth, valuestr);
		    XDrawString(disp, draw,
			piePtr->textGC, x, y + rheight - PADDING * 2,
			textstr, vWidth);
		    x += piePtr->valuewidth;
		    break;
		}
		case 'l': case 'L' : { /* Legend element. */
		    if (piePtr->elements[i].label) {
			/* TEMPORARY FIX: label <-> legend */
			int tlength;

			tlength = strlen(piePtr->elements[i].label);
			XDrawString(disp, draw,
			    piePtr->textGC, 
			    x, y + rheight - PADDING * 2,
			    piePtr->elements[i].label, tlength);
		    }
		    x += piePtr->textwidth;
		    break;
		}
		default: { /* Ignore spurious values. */
		}
	    }
	    options++;
	}
	y += height;
    }
}

/*
 *--------------------------------------------------------------
 *
 * EventuallyRedrawPie --
 *
 *	Arrange for a pie widget to be drawn when Tk is idle.
 *
 *--------------------------------------------------------------
 */
static void
EventuallyRedrawPie(piePtr, displaybits)
    register Pie *piePtr;	/* Information about widget. */
    int displaybits;		/* Which parts of the pie are to be drawn */
{
    if (   (piePtr->tkwin == NULL) || !Tk_IsMapped(piePtr->tkwin)
	|| (piePtr->displaybits & REDRAW_PENDING)) {
	return;
    }

    piePtr->displaybits = displaybits | REDRAW_PENDING;
    Tcl_DoWhenIdle(DisplayPie, (ClientData)piePtr);
}

static void
RedrawPie(piePtr)
    Pie *piePtr;
{
    EventuallyRedrawPie(piePtr, piePtr->displaybits);
}

static void
ComputePieGeometry(piePtr)
    Pie *piePtr;  
{
    int width;
    int height, height2;
    int font = fontheight(piePtr->fontPtr);
    char *options;

    height = piewidth(piePtr);
    width = height + PADDING;
    height2 = (font * piePtr->element_count)
	+ (piePtr->borderWidth + piePtr->explodelength) * 2 + PADDING * 2;
    if (height < height2)
	height = height2;

    options = piePtr->keyorder;
    while (*options) {
	width += PADDING * 2;
	switch (*options) {
	    case 'k': case 'K': {
		width += (font * piePtr->keysize) / 100;
		break;
	    }
	    case 'p': case 'P': {
		width += piePtr->percentwidth;
		break;
	    }
	    case 'l': case 'L': {
		width += piePtr->textwidth;
		break;
	    }
	    case 'v': case 'V': {
		width += piePtr->valuewidth;
		break;
	    }
	}
	options++;
    }
  
    piePtr->titleheight = (piePtr->title && piePtr->title[0]) ? font : 0;
    height += piePtr->titleheight;
    Tk_GeometryRequest(piePtr->tkwin, width, height);
    Tk_SetInternalBorder(piePtr->tkwin, piePtr->borderWidth);
}

static int
pieInsert(piePtr, label,value)
    Pie *piePtr;
    char *label;
    double value;
{
    XGCValues gcValues;
    int i = piePtr->element_count;

    piePtr->total_value += value;
    piePtr->element_count += 1;
    piePtr->elements[i].value = value;
    piePtr->elements[i].label = (char *)ckalloc(strlen(label) + 1);
    strcpy(piePtr->elements[i].label, label);
    piePtr->elements[i].legend = NULL;	/* For the time being. */
    piePtr->displaybits |= KEY_CHANGED;
  
    /* Now a fill pattern. */
  
    if (piePtr->display_depth > 1) {
	XColor *foo;
	foo = Tk_GetColor(piePtr->interp,
	    piePtr->tkwin, colors[piePtr->element_count - 1]);
	if (foo == NULL) {
	    return TCL_ERROR;
	}
	gcValues.foreground = foo->pixel;
	piePtr->elements[i].gc =
	    Tk_GetGC(piePtr->tkwin, GCForeground, &gcValues);
    } else {
	gcValues.fill_style = FillTiled;
	gcValues.tile = Tk_GetBitmapFromData(piePtr->interp,
	    piePtr->tkwin, bitmap[piePtr->element_count-1], 8, 8);
	piePtr->elements[i].gc = Tk_GetGC(piePtr->tkwin,
	    GCTile | GCFillStyle, &gcValues);
    
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * getAllValues --
 *
 *	Return a list of values of all current elements.
 *
 *--------------------------------------------------------------
 */
static void
getAllValues(piePtr)
    Pie *piePtr;
{
    int i;

    for (i = 0; i < piePtr->element_count; i++) {
	char valuestr[20];

	sprintf(valuestr, "%d", (int)piePtr->elements[i].value);
	Tcl_AppendResult(piePtr->interp,
	    piePtr->elements[i].label, " ", valuestr, " ", NULL);
    }
}

/*
 *--------------------------------------------------------------
 *
 * getPieElements --
 *
 *	Return information on an element having the given label.
 *
 *--------------------------------------------------------------
 */
static void
getPieElements(piePtr, label)
    Pie *piePtr;
    char *label;
{
    int i;

    for (i = 0; i < piePtr->element_count; i++) {
	if (strcmp(piePtr->elements[i].label, label) == 0) {
	    char valuestr[20];

	    sprintf(valuestr, "%d", (int)piePtr->elements[i].value);
	    Tcl_AppendResult(piePtr->interp,
	        piePtr->elements[i].label, " ", valuestr, " ", NULL);
	    return;
	}      
    }
} 

/*
 *--------------------------------------------------------------
 *
 * getPieColors --
 *
 *	Return a list of current colours.
 *
 *--------------------------------------------------------------
 */
static void
getPieColors(piePtr)
    Pie *piePtr;
{
    int i;

    for (i = 0; i < piePtr->element_count; i++) {
	Tcl_AppendResult(piePtr->interp, colors[i], " ", NULL);
    }
    return;
}

/*
 *--------------------------------------------------------------
 *
 * setPieColor --
 *
 *	Set one pie colour from the array.
 *
 *--------------------------------------------------------------
 */
int
setPieColor(piePtr, si, hue) 
    Pie *piePtr;
    char *si;
    char *hue;
{
    XColor *ucol;
    XGCValues gcValues;
    GC foo2;
    int i;

    if (piePtr->display_depth <= 1) {
	Tcl_AppendResult(piePtr->interp,
	    "can't set pie color when using a mono display.", NULL);
	return TCL_ERROR;
    }

    i = atoi(si);
    strncpy(usercn[i], hue, sizeof(usercn[i]));

    foo2 = piePtr->elements[i].gc;
    if (usercp[i] != NULL) {
	Tk_FreeColor(usercp[i]);	
    }

    /* Is this a valid colour? */
    ucol = Tk_GetColor(piePtr->interp, piePtr->tkwin, usercn[i]);
    if (ucol == NULL) {
	if (foo2 != NULL) {
	    ucol = Tk_GetColor(piePtr->interp, piePtr->tkwin, usercn[i]);
	}
	usercp[i] = NULL;
	return TCL_ERROR;
    }

    usercp[i] = ucol;
    colors[i] = usercn[i];

    /* if slice is not yet inserted, no GC to update, return */
    if (foo2 == NULL) {
	return TCL_OK;
    }

    /* set foreground colour and arrange for the pie to be drawn */
    gcValues.foreground = ucol->pixel;
    Tk_FreeGC(Tk_Display(piePtr->tkwin), piePtr->elements[i].gc);
    piePtr->elements[i].gc = Tk_GetGC(piePtr->tkwin, GCForeground, &gcValues);
    RedrawPie(piePtr);
    return TCL_OK;
}

static void
alterPieIndex(piePtr, i, value)
    Pie *piePtr;
    int i;
    double value;
{
    piePtr->total_value += (value - piePtr->elements[i].value);
    piePtr->elements[i].value = value;
    piePtr->displaybits |= KEY_CHANGED;
}

static int
alterPieValue(piePtr, label, value)
    Pie *piePtr;
    char *label;
    double value;
{
    int i;

    for (i = 0; i < piePtr->element_count; i++) {
	if (strcmp(piePtr->elements[i].label, label) == 0) {
	    alterPieIndex(piePtr, i, value);
	    return TCL_OK;
	}
    }
    return pieInsert(piePtr, label, value);
}

static int
alterPieStr(piePtr, label, valuestr)
    Pie *piePtr;
    char *label;
    char *valuestr;
{
    double value;

    if (Tcl_GetDouble(piePtr->interp, valuestr, &value) != TCL_OK) {
	return TCL_ERROR;    
    }
    return alterPieValue(piePtr, label, value);
}

static void
deleteElementIndex(piePtr, index)
    Pie *piePtr;
    int index;
{
    int i;

    if (piePtr->elements[index].legend) {
	ckfree(piePtr->elements[index].legend);
    }
    if (piePtr->elements[index].label) {
	ckfree(piePtr->elements[index].label);
    }
    piePtr->total_value -= piePtr->elements[index].value;
    for (i = index; i < piePtr->element_count; i++) {
	piePtr->elements[i].label = piePtr->elements[i+1].label;
	piePtr->elements[i].legend = piePtr->elements[i+1].legend;
	piePtr->elements[i].value = piePtr->elements[i+1].value;
    }
    if (piePtr->tkwin) {
	Tk_FreeGC(Tk_Display(piePtr->tkwin),
	    piePtr->elements[piePtr->element_count-1].gc);
    }
}

static void
deleteElement(piePtr, label)
    Pie *piePtr;
    char *label;
{
    int i;
  
    for (i = 0; i < piePtr->element_count; i++) {
	if (strcmp(piePtr->elements[i].label, label) == 0) {
	    deleteElementIndex(piePtr, i);
	    piePtr->element_count--;
	    piePtr->displaybits |= KEY_CHANGED;
	    if (piePtr->element_count == 0) {
		piePtr->displaybits |= CLEAR_NEEDED;
	    }
	    return;
	}
    }
}

static void
Callback(piePtr)
    Pie *piePtr;
{
    int no_errors = TRUE;
    int result;

    if (piePtr->command && *piePtr->command) {
	result = Tcl_Eval(piePtr->interp, piePtr->command);
	if (result != TCL_OK) {
	    Tk_BackgroundError(piePtr->interp);
	    no_errors = FALSE;
	}
    }

    if (piePtr->continue_callback && no_errors) {
	piePtr->cb_timer =
	    Tk_CreateTimerHandler(piePtr->interval, (Tk_TimerProc *)Callback,
		(ClientData)piePtr);
    }
}

static int
pieWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Pie *piePtr = (Pie *)clientData;
    int result = TCL_OK;
    int length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
	    argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    Tcl_Preserve((ClientData)piePtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " cget option\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto end;
	}
	result = Tk_ConfigureValue(interp, piePtr->tkwin, configSpecs,
	    (char *)piePtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
	if (argc == 2) {
	    /* return info about all configuration options. */
	    result = Tk_ConfigureInfo(interp, piePtr->tkwin, configSpecs,
		(char *)piePtr, NULL, 0);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, piePtr->tkwin, configSpecs,
		(char *)piePtr, argv[2], 0);
	} else {
	    result = ConfigurePie(interp, piePtr, argc-2, argv+2,
		TK_CONFIG_ARGV_ONLY);
	}
	piePtr->displaybits |= CLEAR_NEEDED;
    } else if ((c == 'd') && (strncmp(argv[1], "delete", length) == 0)) {
	if (argc == 2) {
	    int i;

	    for (i = 0; i < piePtr->element_count; i++) {
		if (piePtr->elements[i].legend) {
		    ckfree(piePtr->elements[i].legend);
		}
		if (piePtr->elements[i].label) {
		    ckfree(piePtr->elements[i].label);
		}
		if (piePtr->tkwin) {
		    Tk_FreeGC(Tk_Display(piePtr->tkwin),piePtr->elements[i].gc);
		}
	    }
	    piePtr->total_value = 0;
	    piePtr->element_count = 0;
	    result = TCL_OK;
	} else {
	    if (argc > 2) {
		argv++;
		while (argc > 2) {
		    argv++;
		    argc--;
		    deleteElement(piePtr,*argv);
		}
		result = TCL_OK;
	    } else {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " delete ?label ...?\"", NULL);
		result =  TCL_ERROR;
	    }
	}
    } else if ((c == 's') && (strncmp(argv[1], "set", length) == 0)) {
	if ((argc >= 4) && ((argc % 2) == 0)) {
	  while (argc > 2) {
	    argv += 2; argc -=2;
	    result = alterPieStr(piePtr, argv[0],argv[1]);
	    if (result != TCL_OK) argc = 0;
	  }
	} else {
	  Tcl_AppendResult(interp, "wrong # args: should be \"",
			   argv[0], " set ?label value? ...\"",(char *)NULL);
	  result =  TCL_ERROR;
	}
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	if (argc == 2) {
	    result = TCL_OK;
	    getAllValues(piePtr);
	} else {
	    if (argc > 2) {
		argv++;
		result = TCL_OK;
		while (argc > 2) {
		    argv++;
		    argc--;
		    getPieElements(piePtr, *argv);
		}
	    } else {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " get ?label ...?\"", NULL);
		result =  TCL_ERROR;
	    }
	}
    } else if ((c == 's') && (strncmp(argv[1], "start", length) == 0)) {
	if (!piePtr->continue_callback) {
	    piePtr->continue_callback = TRUE;
	    Callback(piePtr);
	}
    } else if ((c == 's') && (strncmp(argv[1], "stop", length) == 0)) {
	piePtr->continue_callback = FALSE;
	if (piePtr->cb_timer) {
	    Tk_DeleteTimerHandler(piePtr->cb_timer);
	    piePtr->cb_timer = NULL;
	}
    } else if ((c == 'c') && (strncmp(argv[1], "color", length) == 0)) {
	if (argc == 2) {
	    getPieColors(piePtr);
	} else {
	    if ((argc == 4) && ((argc % 2) == 0)) {
		result = setPieColor(piePtr,argv[2],argv[3]);
	    } else {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " color ?index Xcolor? ...\"", NULL);
          	result =  TCL_ERROR;
	    }
	}
    } else {
	/* BASE CASE: Not understood. */
	Tcl_SetResult(interp, (char *) NULL, TCL_STATIC);
	Tcl_AppendResult(interp,
	    "bad option \"", argv[1],
	    "\": must be cget, configure, delete, get, set, start or stop",
	    (char *) NULL);
	result = TCL_ERROR;
    }
  
    if (result == TCL_OK) {
	ComputePieGeometry(piePtr);
	RedrawPie(piePtr);
    }

end:
    Tcl_Release((ClientData)piePtr);
    return result;
}

static void
DestroyPie(clientData)
    ClientData clientData;
{
    register Pie *piePtr = (Pie *)clientData;
    int i;

    if (piePtr->cursor != None) {
	Tk_FreeCursor(Tk_Display(piePtr->tkwin), piePtr->cursor);
    }
    if (piePtr->border != NULL) {
	Tk_Free3DBorder(piePtr->border);
    }
    if (piePtr->fontPtr != NULL) {
	Tk_FreeFont((Tk_Font)piePtr->fontPtr);
    }
    if (piePtr->textColorPtr != NULL) {
	Tk_FreeColor(piePtr->textColorPtr);
    }
    if (piePtr->backgroundColor != NULL) {
	Tk_FreeColor(piePtr->backgroundColor);
    }
    if ((piePtr->textGC != None) && piePtr->tkwin) {
	Tk_FreeGC(Tk_Display(piePtr->tkwin),piePtr->textGC);
    }
    if (piePtr->cb_timer) {
	Tk_DeleteTimerHandler(piePtr->cb_timer);
    }
    piePtr->continue_callback = FALSE;
  
    for (i = 0; i < piePtr->element_count; i++) {
	if (piePtr->elements[i].legend) {
	    ckfree(piePtr->elements[i].legend);
	}
	if (piePtr->elements[i].label) {
	    ckfree(piePtr->elements[i].label);
	}
	if (piePtr->tkwin) {
	    Tk_FreeGC(Tk_Display(piePtr->tkwin), piePtr->elements[i].gc);
	}
    }
    ckfree((char *)piePtr);
}

static void
pieEventProc(clientData, eventPtr)
    ClientData clientData;
    XEvent *eventPtr;
{
    Pie *piePtr = (Pie *)clientData;
  
    if ((eventPtr->type == Expose) &&(eventPtr->xexpose.count == 0)) {
	RedrawPie(piePtr);
    } else if (eventPtr->type == DestroyNotify) {
	Tcl_DeleteCommand(piePtr->interp, Tk_PathName(piePtr->tkwin));
	piePtr->tkwin = NULL;
	Tcl_CancelIdleCall(DisplayPie, (ClientData)piePtr);
	Tcl_EventuallyFree((ClientData)piePtr, DestroyPie);
    } else if (eventPtr->type == ConfigureNotify) {
	ComputePieGeometry(piePtr);
    }
}

/* 
 *---------------------------------------------------------------
 *
 * PieCmd --
 *
 *	Realizes a piechart widget.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *---------------------------------------------------------------
 */
static int
PieCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Tk_Window tkwin = (Tk_Window)clientData;
    register Pie *piePtr;
    Tk_Window new;
    int w;
  
    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
	    argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    piePtr = (Pie *)ckalloc(sizeof(Pie));
    memset((VOID *)piePtr, 0, sizeof(Pie));

    new = Tk_CreateWindowFromPath(interp, tkwin, argv[1], NULL);
    if (new==NULL) {
	return TCL_ERROR;
    }

    piePtr->tkwin = new;
    piePtr->interp = interp;
    piePtr->border = NULL;
    piePtr->relief = TK_RELIEF_RAISED;
    piePtr->displaybits = 0;
    piePtr->backgroundColor = NULL;
    piePtr->textColorPtr = NULL;
    piePtr->fontPtr = NULL;
    piePtr->title = NULL;
    piePtr->command = NULL;
    piePtr->exploded = NULL;
    piePtr->total_value = 0;
    piePtr->element_count = 0;
    piePtr->userdata = NULL;
    piePtr->continue_callback = FALSE;
    piePtr->cb_timer = NULL;
    piePtr->cb_key = 0;
    for (w = 0; w < MAX_WEDGES; w++) {
	piePtr->cb_loc[w] = -1;
    }
    piePtr->textGC = None;
    piePtr->cursor = None;
  
    Tk_MakeWindowExist(piePtr->tkwin);

    piePtr->display_depth = DefaultDepthOfScreen(Tk_Screen(tkwin));

    Tk_SetClass(piePtr->tkwin, PIE_CLASS_NAME);
    Tk_CreateEventHandler(piePtr->tkwin, 
	ExposureMask|StructureNotifyMask, pieEventProc, (ClientData)piePtr);
    Tcl_CreateCommand(interp,
	Tk_PathName(piePtr->tkwin), pieWidgetCmd, (ClientData)piePtr, NULL);

    if (ConfigurePie(interp, piePtr, argc-2, argv+2, 0) == TCL_OK) {
	Tcl_SetResult(interp, Tk_PathName(piePtr->tkwin), TCL_STATIC);
	return TCL_OK;
    }
    Tk_DestroyWindow(piePtr->tkwin);
    return TCL_ERROR;
}

static int
ConfigurePie(interp, piePtr, argc, argv, flags)
    Tcl_Interp *interp;
    register Pie *piePtr;
    int argc;
    char **argv;
    int flags;
{
    XGCValues gcValues;
    GC newGC;

    if (Tk_ConfigureWidget(interp, piePtr->tkwin, configSpecs, argc, argv,
	(char *)piePtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
  
    Tk_SetBackgroundFromBorder(piePtr->tkwin, piePtr->border);
    gcValues.font = piePtr->fontPtr->fid;
    gcValues.foreground = piePtr->textColorPtr->pixel;
    newGC = Tk_GetGC(piePtr->tkwin, GCForeground|GCFont, &gcValues);
    if ((piePtr->textGC != None) && piePtr->tkwin) {
	Tk_FreeGC(Tk_Display(piePtr->tkwin),piePtr->textGC);
    }
    piePtr->textGC = newGC;

    ComputePieGeometry(piePtr);
    RedrawPie(piePtr);
    return TCL_OK;
}

#ifdef __WIN32__
/*
 * XClearArea() emulation for Win32; future implementations of
 * the "piechart" widget won't probably need it anymore.
 */
void
XClearArea(display, w, x, y, width, height, dummy)
    Display* display;
    Window w;
    int x, y, width, height, dummy;
{
    RECT rc;
    HBRUSH brush;
    HPALETTE oldPalette, palette;
    TkWindow *winPtr;
    HWND hwnd = TkWinGetHWND(w);
    HDC dc = GetDC(hwnd);
 
    palette = TkWinGetPalette(display->screens[0].cmap);
    oldPalette = SelectPalette(dc, palette, FALSE);
 
    display->request++;
 
    winPtr = TkWinGetWinPtr(w);
    brush = CreateSolidBrush(winPtr->atts.background_pixel);
    rc.left = x;
    rc.right= x+width;
    rc.top = y;
    rc.bottom = y+height;
    FillRect(dc, &rc, brush);
 
    DeleteObject(brush);
    SelectPalette(dc, oldPalette, TRUE);
    ReleaseDC(hwnd, dc);
}
#endif /* __WIN32__ */

int
Blt_PieInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = { "piechart", "8.0", (Tcl_CmdProc *)PieCmd, };

    return(Blt_InitCmd(interp, &cmdSpec));
}
#endif /* !NO_PIECHART */
