/*
 * bltGrLegd.c --
 *
 *	This module implements legends for the graph widget for
 *	the Tk toolkit.
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

#define padLeft  	padX.side1
#define padRight  	padX.side2
#define padTop		padY.side1
#define padBottom	padY.side2
#define PADDING(x)	((x).side1 + (x).side2)

#define DEF_LEGEND_ACTIVE_BG_COLOR 	STD_ACTIVE_BG_COLOR
#define DEF_LEGEND_ACTIVE_BG_MONO	STD_ACTIVE_BG_MONO
#define DEF_LEGEND_ACTIVE_BORDER_WIDTH  "2"
#define DEF_LEGEND_ACTIVE_FG_COLOR	STD_ACTIVE_FG_COLOR
#define DEF_LEGEND_ACTIVE_FG_MONO	STD_ACTIVE_FG_MONO
#define DEF_LEGEND_ACTIVE_RELIEF	"flat"
#define DEF_LEGEND_ANCHOR	   	"n"
#define DEF_LEGEND_BG_COLOR	   	(char *)NULL
#define DEF_LEGEND_BG_MONO		(char *)NULL
#define DEF_LEGEND_BORDER_WIDTH 	STD_BORDERWIDTH
#define DEF_LEGEND_FG_COLOR		STD_NORMAL_FG_COLOR
#define DEF_LEGEND_FG_MONO		STD_NORMAL_FG_MONO
#define DEF_LEGEND_FONT			STD_FONT_SMALL
#define DEF_LEGEND_IPAD_X		"1"
#define DEF_LEGEND_IPAD_Y		"1"
#define DEF_LEGEND_MAPPED       	"1"
#define DEF_LEGEND_PAD_X		"4"
#define DEF_LEGEND_PAD_Y		"4"
#define DEF_LEGEND_POSITION		"rightmargin"
#define DEF_LEGEND_RAISED       	"0"
#define DEF_LEGEND_RELIEF		"sunken"

static int PositionParseProc _ANSI_ARGS_((ClientData, Tcl_Interp *, Tk_Window,
	char *, char *, int));
static char *PositionPrintProc _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

static Tk_CustomOption legendPositionOption =
{
    PositionParseProc, PositionPrintProc, (ClientData)0
};

extern Tk_CustomOption bltPositionOption;
extern Tk_CustomOption bltLengthOption;
extern Tk_CustomOption bltPadOption;

/*
 *
 * Legend --
 *
 * 	Contains information specific to how the legend will be
 *	displayed.
 *
 */
typedef struct {
    int mapped;			/* Requested state of the legend, If non-zero,
				 * legend is displayed */
    int width, height;		/* Dimensions of the legend */
    LegendPosition anchorPos;	/* Window coordinates of legend positioning
				 * point. Used in conjunction with the anchor
				 * to determine the location of the legend. */
    int raised;			/* If non-zero, draw the legend last, above
				 * everything else. */
    LegendDrawProc *drawProc;
    LegendPrintProc *printProc;
    LegendDestroyProc *destroyProc;
    LegendGeometryProc *geomProc;

    int ipadX, ipadY;		/* # of pixels padding around legend entries */
    Pad padX, padY;		/* # of pixels padding to exterior of legend */
    int numLabels;		/* Number of labels (and symbols) to display */
    int numCols, numRows;	/* Number of columns and rows in legend */
    int entryWidth, entryHeight;

    int maxSymSize;		/* Size of largest symbol to be displayed.
				 * Used to calculate size of legend */
    Tk_Anchor anchor;		/* Anchor of legend. Used to interpret the
				 * positioning point of the legend in the
				 * graph*/

    TkFont *fontPtr;		/* Font for legend text */

    int x, y;			/* Origin of legend */

    int borderWidth;		/* Width of legend 3-D border */
    int relief;			/* 3-d effect of border around the legend:
				 * TK_RELIEF_RAISED etc. */
    XColor *textColor;		/* Foreground color for legend
				 * text. Symbols retain the color
				 * specified for the element*/
    XColor *fillColor;
    GC normalGC;		/* Normal legend entry graphics context */
    GC fillGC;			/* Fill color background GC */
    Tk_3DBorder activeBorder;	/* Background color for active legend
				 * entries. */
    int entryBorderWidth;	/* Width of border around each entry in the
				 * legend */
    int activeRelief;		/* 3-d effect: TK_RELIEF_RAISED etc. */
    XColor *activeTextColor;	/* Foreground color for active legend
				 * entries. Symbols retain the color
				 * specified for the element*/
    GC activeGC;		/* Active legend entry graphics context */

} Legend;

static Tk_ConfigSpec configSpecs[] =
{
    {TK_CONFIG_BORDER, "-activebackground", "legendActiveBackground",
	"ActiveBackground", DEF_LEGEND_ACTIVE_BG_COLOR,
	Tk_Offset(Legend, activeBorder), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "legendActiveBackground",
	"ActiveBackground", DEF_LEGEND_ACTIVE_BG_MONO,
	Tk_Offset(Legend, activeBorder), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-activeborderwidth", "legendActiveBorderWidth",
	"BorderWidth", DEF_LEGEND_BORDER_WIDTH,
	Tk_Offset(Legend, entryBorderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_COLOR, "-activeforeground", "legendActiveForeground",
	"ActiveForeground", DEF_LEGEND_ACTIVE_FG_COLOR,
	Tk_Offset(Legend, activeTextColor), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "legendActiveForeground",
	"ActiveForeground", DEF_LEGEND_ACTIVE_FG_MONO,
	Tk_Offset(Legend, activeTextColor), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_RELIEF, "-activerelief", "legendActiveRelief", "Relief",
	DEF_LEGEND_ACTIVE_RELIEF, Tk_Offset(Legend, activeRelief),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_ANCHOR, "-anchor", "legendAnchor", "Anchor",
	DEF_LEGEND_ANCHOR, Tk_Offset(Legend, anchor),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_SYNONYM, "-bg", "legendBackground", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_COLOR, "-background", "legendBackground", "Background",
	DEF_LEGEND_BG_MONO, Tk_Offset(Legend, fillColor),
	TK_CONFIG_NULL_OK | TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-background", "legendBackground", "Background",
	DEF_LEGEND_BG_COLOR, Tk_Offset(Legend, fillColor),
	TK_CONFIG_NULL_OK | TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_CUSTOM, "-borderwidth", "legendBorderWidth", "BorderWidth",
	DEF_LEGEND_BORDER_WIDTH, Tk_Offset(Legend, borderWidth),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_SYNONYM, "-bd", "legendBorderWidth", (char *)NULL,
	(char *)NULL, 0, 0},
    {TK_CONFIG_FONT, "-font", "legendFont", "Font",
	DEF_LEGEND_FONT, Tk_Offset(Legend, fontPtr), 0},
    {TK_CONFIG_SYNONYM, "-fg", "legendForeground", (char *)NULL, (char *)NULL,
	0, 0},
    {TK_CONFIG_COLOR, "-foreground", "legendForeground", "Foreground",
	DEF_LEGEND_FG_COLOR, Tk_Offset(Legend, textColor),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "legendForeground", "Foreground",
	DEF_LEGEND_FG_MONO, Tk_Offset(Legend, textColor),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_CUSTOM, "-ipadx", "legendIPadX", "Pad",
	DEF_LEGEND_IPAD_X, Tk_Offset(Legend, ipadX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_CUSTOM, "-ipady", "legendIPadY", "Pad",
	DEF_LEGEND_IPAD_Y, Tk_Offset(Legend, ipadY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-mapped", "legendMapped", "Mapped",
	DEF_LEGEND_MAPPED, Tk_Offset(Legend, mapped),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-padx", "legendPadX", "Pad",
	DEF_LEGEND_PAD_X, Tk_Offset(Legend, padX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", "legendPadY", "Pad",
	DEF_LEGEND_PAD_Y, Tk_Offset(Legend, padY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-position", "legendPosition", "Position",
	DEF_LEGEND_POSITION, Tk_Offset(Legend, anchorPos),
	TK_CONFIG_DONT_SET_DEFAULT, &legendPositionOption},
    {TK_CONFIG_BOOLEAN, "-raised", "legendRaised", "Raised",
	DEF_LEGEND_RAISED, Tk_Offset(Legend, raised),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_RELIEF, "-relief", "legendRelief", "Relief",
	DEF_LEGEND_RELIEF, Tk_Offset(Legend, relief),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};


static int
GetLegendPosition(interp, string, posPtr)
    Tcl_Interp *interp;
    char *string;
    LegendPosition *posPtr;
{
    char c;
    unsigned int length;

    if ((string == NULL) || (*string == '\0')) {
	posPtr->site = LEGEND_SITE_RIGHT;
	return TCL_OK;		/* Position marked as default */
    }
    c = string[0];
    length = strlen(string);
    if (c == '@') {
	long x, y;
	int result;
	char *comma, *tmpString;

	string++;
	/* mike: make a copy of string, so that we can safely change it. */
	tmpString = ckstrdup(string);
	comma = strchr(tmpString, ',');
	if (comma == NULL) {
	    ckfree(tmpString);
	    goto badFormat;
	}
	*comma = '\0';
	result = ((Tcl_ExprLong(interp, tmpString, &x) == TCL_OK) &&
	    (Tcl_ExprLong(interp, comma + 1, &y) == TCL_OK));
	*comma = ',';
	ckfree(tmpString);
	if (!result) {
	    return TCL_ERROR;
	}
	posPtr->x = (int)x, posPtr->y = (int)y;
	posPtr->site = LEGEND_SITE_XY;
    } else if ((c == 'l') && (strncmp(string, "leftmargin", length) == 0)) {
	posPtr->site = LEGEND_SITE_LEFT;
    } else if ((c == 'r') && (strncmp(string, "rightmargin", length) == 0)) {
	posPtr->site = LEGEND_SITE_RIGHT;
    } else if ((c == 't') && (strncmp(string, "topmargin", length) == 0)) {
	posPtr->site = LEGEND_SITE_TOP;
    } else if ((c == 'b') && (strncmp(string, "bottommargin", length) == 0)) {
	posPtr->site = LEGEND_SITE_BOTTOM;
    } else if ((c == 'p') && (strncmp(string, "plotarea", length) == 0)) {
	posPtr->site = LEGEND_SITE_PLOT;
    } else {
      badFormat:
	Tcl_AppendResult(interp, "bad position \"", string, "\": should be ",
	    "\"leftmargin\", \"rightmargin\", \"topmargin\", ",
	    "\"bottommargin\", \"plotarea\", or @x,y", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PositionParseProc --
 *
 *	Convert the string representation of a legend XY position into
 *	window coordinates.  The form of the string must be "@x,y" or
 *	none.
 *
 * Results:
 *	The return value is a standard Tcl result.  The symbol type is
 *	written into the widget record.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PositionParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* New legend position string */
    char *widgRec;		/* Widget record */
    int offset;			/* offset to XPoint structure */
{
    LegendPosition *posPtr = (LegendPosition *)(widgRec + offset);

    if (GetLegendPosition(interp, value, posPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

static char *
NameOfPosition(posPtr)
    LegendPosition *posPtr;
{
    static char position[200];

    switch (posPtr->site) {
    case LEGEND_SITE_LEFT:
	return "leftmargin";
    case LEGEND_SITE_RIGHT:
	return "rightmargin";
    case LEGEND_SITE_TOP:
	return "topmargin";
    case LEGEND_SITE_BOTTOM:
	return "bottommargin";
    case LEGEND_SITE_PLOT:
	return "plotarea";
    case LEGEND_SITE_XY:
	sprintf(position, "@%d,%d", posPtr->x, posPtr->y);
	return (position);
    default:
	return "unknown legend position";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PositionPrintProc --
 *
 *	Convert the window coordinates into a string.
 *
 * Results:
 *	The string representing the coordinate position is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
PositionPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Widget record */
    int offset;			/* offset of XPoint in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    LegendPosition *posPtr = (LegendPosition *)(widgRec + offset);

    return (NameOfPosition(posPtr));
}


static void
SetLegendOrigin(graphPtr)
    Graph *graphPtr;
{
    Legend *legendPtr = (Legend *)graphPtr->legendPtr;
    double x, y;
    Tk_Anchor anchor;
    Coordinate origin;
    int borderWidths;
    int margin;

    x = y = 0.0;			/* Suppress compiler warning */
    anchor = TK_ANCHOR_CENTER;
    borderWidths = graphPtr->borderWidth + graphPtr->plotBW;
    switch (legendPtr->anchorPos.site) {
    case LEGEND_SITE_RIGHT:
	margin = graphPtr->rightMargin - borderWidths;
	margin -= Blt_GetAxisMargin(graphPtr->rightAxis);
	x = (double)(graphPtr->width - graphPtr->borderWidth - (margin * 0.5));
	break;
    case LEGEND_SITE_LEFT:
	margin = graphPtr->leftMargin - borderWidths;
	margin -= Blt_GetAxisMargin(graphPtr->leftAxis);
	x = (double)(graphPtr->borderWidth + (margin * 0.5));
	break;
    case LEGEND_SITE_TOP:
	margin = graphPtr->topMargin - borderWidths;
	margin -= graphPtr->titleHeight + Blt_GetAxisMargin(graphPtr->topAxis);
	y = (double)(graphPtr->borderWidth + (margin * 0.5));
	y += graphPtr->titleHeight;
	break;
    case LEGEND_SITE_BOTTOM:
	margin = graphPtr->bottomMargin - borderWidths;
	margin -= Blt_GetAxisMargin(graphPtr->bottomAxis);
	y = (double)(graphPtr->height - graphPtr->borderWidth - (margin * 0.5));
	break;
    case LEGEND_SITE_PLOT:
    case LEGEND_SITE_XY:
	break;
    }

    switch (legendPtr->anchorPos.site) {
    case LEGEND_SITE_RIGHT:
    case LEGEND_SITE_LEFT:
	switch (legendPtr->anchor) {
	case TK_ANCHOR_N:
	case TK_ANCHOR_NE:
	case TK_ANCHOR_NW:
	    y = graphPtr->ury;
	    anchor = TK_ANCHOR_N;
	    break;
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_E:
	case TK_ANCHOR_W:
	    y = (double)((graphPtr->lly + graphPtr->ury) * 0.5);
	    anchor = TK_ANCHOR_CENTER;
	    break;
	case TK_ANCHOR_S:
	case TK_ANCHOR_SE:
	case TK_ANCHOR_SW:
	    y = graphPtr->lly;
	    anchor = TK_ANCHOR_S;
	    break;
	}
	break;

    case LEGEND_SITE_TOP:
    case LEGEND_SITE_BOTTOM:
	switch (legendPtr->anchor) {
	case TK_ANCHOR_E:
	case TK_ANCHOR_NE:
	case TK_ANCHOR_SE:
	    x = graphPtr->urx;
	    anchor = TK_ANCHOR_E;
	    break;
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_N:
	case TK_ANCHOR_S:
	    x = ((graphPtr->llx + graphPtr->urx) * 0.5);
	    anchor = TK_ANCHOR_CENTER;
	    break;
	case TK_ANCHOR_W:
	case TK_ANCHOR_SW:
	case TK_ANCHOR_NW:
	    x = graphPtr->llx;
	    anchor = TK_ANCHOR_W;
	    break;
	}
	break;

    case LEGEND_SITE_PLOT:
	switch (legendPtr->anchor) {
	case TK_ANCHOR_N:
	    x = ((graphPtr->llx + graphPtr->urx) * 0.5);
	    y = graphPtr->ury;
	    break;

	case TK_ANCHOR_NE:
	    x = (double)(graphPtr->width - graphPtr->rightMargin);
	    y = graphPtr->ury;
	    break;

	case TK_ANCHOR_NW:
	    x = (double)graphPtr->leftMargin;
	    y = (double)graphPtr->ury;
	    break;

	case TK_ANCHOR_CENTER:
	    x = ((graphPtr->llx + graphPtr->urx) * 0.5);
	    y = ((graphPtr->lly + graphPtr->ury) * 0.5);
	    break;

	case TK_ANCHOR_E:
	    x = (double)(graphPtr->width - graphPtr->rightMargin);
	    y = ((graphPtr->lly + graphPtr->ury) * 0.5);
	    break;

	case TK_ANCHOR_W:
	    x = (double)graphPtr->leftMargin;
	    y = ((graphPtr->lly + graphPtr->ury) * 0.5);
	    break;

	case TK_ANCHOR_S:
	    x = ((graphPtr->llx + graphPtr->urx) * 0.5);
	    y = (double)(graphPtr->height - graphPtr->bottomMargin);
	    break;

	case TK_ANCHOR_SE:
	    x = (double)(graphPtr->width - graphPtr->rightMargin);
	    y = (double)(graphPtr->height - graphPtr->bottomMargin);
	    break;

	case TK_ANCHOR_SW:
	    x = (double)graphPtr->leftMargin;
	    y = (double)(graphPtr->height - graphPtr->bottomMargin);
	    break;
	}
	anchor = legendPtr->anchor;
	break;

    case LEGEND_SITE_XY:
	if ((graphPtr->width != Tk_Width(graphPtr->tkwin)) ||
	    (graphPtr->height != Tk_Height(graphPtr->tkwin))) {
	    double scale;

	    /*
	     * Legend position was given in window coordinates so we have to
	     * scale using the current page coordinates.
	     */
	    scale = (double)graphPtr->width / Tk_Width(graphPtr->tkwin);
	    x = (legendPtr->anchorPos.x * scale);
	    scale = (double)graphPtr->height / Tk_Height(graphPtr->tkwin);
	    y = (legendPtr->anchorPos.y * scale);
	} else {
	    x = (double)legendPtr->anchorPos.x;
	    y = (double)legendPtr->anchorPos.y;
	}
	/*
	 * If negative coordinates, assume they mean offset from the
	 * opposite side of the window.
	 */
	if (x < 0) {
	    x += graphPtr->width;
	}
	if (y < 0) {
	    y += graphPtr->height;
	}
	anchor = legendPtr->anchor;
	break;
    }
    origin = Blt_TranslateBoxCoords(x, y, legendPtr->width, legendPtr->height,
	anchor);
    legendPtr->x = BLT_RND(origin.x) + legendPtr->padLeft;
    legendPtr->y = BLT_RND(origin.y) + legendPtr->padTop;
}


static int
GetLegendIndex(legendPtr, pointPtr)
    Legend *legendPtr;
    XPoint *pointPtr;		/* Point to be tested */
{
    int x, y;
    int width, height;
    int row, column;
    int entry;

    x = legendPtr->x + legendPtr->borderWidth;
    y = legendPtr->y + legendPtr->borderWidth;
    width = legendPtr->width - (2 * legendPtr->borderWidth +
	legendPtr->padLeft + legendPtr->padRight);
    height = legendPtr->height - (2 * legendPtr->borderWidth +
	legendPtr->padTop + legendPtr->padBottom);

    if ((pointPtr->x < x) || (pointPtr->x > (x + width)) ||
	(pointPtr->y < y) || (pointPtr->y > (y + height))) {
	return -1;
    }
    /* It's in the bounding box. Compute the label entry index */

    row = (pointPtr->y - y) / legendPtr->entryHeight;
    column = (pointPtr->x - x) / legendPtr->entryWidth;
    entry = (column * legendPtr->numRows) + row;
    if (entry >= legendPtr->numLabels) {
	return -1;
    }
    return (entry);
}


static Element *
LocateElement(graphPtr, legendPtr, string)
    Graph *graphPtr;
    Legend *legendPtr;
    char *string;
{
    Blt_ListItem *itemPtr;
    Element *elemPtr;

    elemPtr = NULL;
    if (string[0] == '@') {
	XPoint point;
	int count, entry;

	if (Blt_GetXYPosition(graphPtr->interp, string, &point) != TCL_OK) {
	    return NULL;
	}
	entry = GetLegendIndex(legendPtr, &point);
	if (entry < 0) {
	    return NULL;
	}
	count = 0;
	for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	    itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	    elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	    if (elemPtr->label == NULL) {
		continue;	/* Skip this label */
	    }
	    if (count == entry) {
		break;
	    }
	    count++;
	}
    }
    return (elemPtr);
}

LegendSite
Blt_LegendSite(graphPtr)
    Graph *graphPtr;
{
    return (graphPtr->legendPtr->anchorPos.site);
}

/*
 * -----------------------------------------------------------------
 *
 * ComputeLegendGeometry --
 *
 * 	Calculates the dimensions (width and height) needed for
 *	the legend.  Also determines the number of rows and columns
 *	necessary to list all the valid element labels.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *   	The following fields of the legend are calculated and set.
 *
 * 	numLabels   - number of valid labels of elements in the
 *		      display list.
 * 	numRows	    - number of rows of entries
 * 	numCols	    - number of columns of entries
 * 	entryHeight - height of each entry
 * 	entryWidth  - width of each entry
 * 	height	    - width of legend (includes borders and padding)
 * 	width	    - height of legend (includes borders and padding)
 *
 * -----------------------------------------------------------------
 */
static void
ComputeLegendGeometry(graphPtr, maxWidth, maxHeight)
    Graph *graphPtr;
    int maxWidth;		/* Maximum width available in graph window
				 * to draw the legend. Will calculate number
				 * of columns from this. */
    int maxHeight;		/* Maximum height available in graph window
				 * to draw the legend. Will calculate number
				 * of rows from this. */
{
    Legend *legendPtr = (Legend *)graphPtr->legendPtr;
    Blt_ListItem *itemPtr;
    Element *elemPtr;
    int numRows, numCols;
    int maxLabelWidth, maxLabelHeight;
    int numLabels;
    int twiceBW;
    int labelWidth, labelHeight;
    int symbolWidth;

    /* Initialize legend values to default (no legend displayed) */

    legendPtr->entryWidth = legendPtr->entryHeight = 0;
    legendPtr->numRows = legendPtr->numCols = 0;
    legendPtr->numLabels = 0;
    legendPtr->height = legendPtr->width = 0;

    if (!legendPtr->mapped) {
	return;			/* Legend is not being displayed */
    }
    /* Determine the number of labels and the width of the width label */

    numLabels = 0;
    maxLabelWidth = maxLabelHeight = 0;
    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	if (elemPtr->label == NULL) {
	    continue;		/* Skip this label */
	}
	Blt_GetTextExtents(legendPtr->fontPtr, elemPtr->label,
	    &labelWidth, &labelHeight);
	if (labelWidth > maxLabelWidth) {
	    maxLabelWidth = labelWidth;
	}
	if (labelHeight > maxLabelHeight) {
	    maxLabelHeight = labelHeight;
	}
	numLabels++;
    }

    if (numLabels == 0) {
	return;			/* No labels to display in legend */
    }
    /*
     * Calculate the space need to for the legend based upon the size
     * of a label entry and the number of rows and columns needed.
     * Bound the height of the area by *maxHeight* which is the size
     * of the plotting area.
     */

    twiceBW = (2 * legendPtr->entryBorderWidth);

    symbolWidth = TEXTHEIGHT(legendPtr->fontPtr);
    legendPtr->numLabels = numLabels;
    legendPtr->entryHeight = maxLabelHeight + twiceBW +
	(2 * legendPtr->ipadY);
    legendPtr->entryWidth = maxLabelWidth + symbolWidth + twiceBW +
	(2 * legendPtr->ipadX) + 5;

    maxHeight -= (2 * legendPtr->borderWidth) + PADDING(legendPtr->padY);
    maxWidth -= (2 * legendPtr->borderWidth) + PADDING(legendPtr->padX);
    numRows = maxHeight / legendPtr->entryHeight;
    numCols = maxWidth / legendPtr->entryWidth;

    switch (legendPtr->anchorPos.site) {
    case LEGEND_SITE_XY:
    case LEGEND_SITE_PLOT:
    case LEGEND_SITE_LEFT:
    case LEGEND_SITE_RIGHT:
	if (numRows > 0) {
	    numCols = ((numLabels - 1) / numRows) + 1;
	    if (numRows > numLabels) {
		numRows = numLabels;
	    }
	}
	break;

    case LEGEND_SITE_TOP:
    case LEGEND_SITE_BOTTOM:
	if (numCols > 0) {
	    numRows = ((numLabels - 1) / numCols) + 1;
	    if (numCols > numLabels) {
		numCols = numLabels;
	    } else {
		numCols = ((numLabels - 1) / numRows) + 1;
	    }
	}
	break;
    }
    legendPtr->height = (2 * legendPtr->borderWidth) +
	PADDING(legendPtr->padY) + (numRows * legendPtr->entryHeight);
    legendPtr->width = (2 * legendPtr->borderWidth) +
	PADDING(legendPtr->padX) + (numCols * legendPtr->entryWidth);
    legendPtr->numRows = numRows;
    legendPtr->numCols = numCols;
}

/*
 * -----------------------------------------------------------------
 *
 * DrawLegend --
 *
 * -----------------------------------------------------------------
 */
static void
DrawLegend(graphPtr)
    Graph *graphPtr;
{
    Legend *legendPtr = (Legend *)graphPtr->legendPtr;
    Blt_ListItem *itemPtr;
    TextAttributes textAttr;
    int x, y;
    int width, height;
    int labelX, startY, symbolX, symbolY;
    int counter;
    XPoint point;
    int symSize, midPoint;
    int redraw;
    register Element *elemPtr;
    GC fillGC;

    graphPtr->flags &= ~DRAW_LEGEND;
    if ((!legendPtr->mapped) || (legendPtr->numLabels == 0) ||
	(legendPtr->numRows == 0) || (legendPtr->numCols == 0)) {
	return;
    }
    SetLegendOrigin(graphPtr);

    width = legendPtr->width - PADDING(legendPtr->padX);
    height = legendPtr->height - PADDING(legendPtr->padY);
    symSize = legendPtr->fontPtr->fm.ascent;
    midPoint = (symSize / 2) + 1 + legendPtr->entryBorderWidth;

    labelX = TEXTHEIGHT(legendPtr->fontPtr) + legendPtr->entryBorderWidth +
	legendPtr->ipadX + 5;
    symbolY = midPoint + legendPtr->ipadY;
    symbolX = midPoint + legendPtr->ipadX;

    x = legendPtr->x, y = legendPtr->y;

    redraw = FALSE;
    fillGC = legendPtr->fillGC;
    if (legendPtr->fillColor == NULL) {
	fillGC = graphPtr->fillGC;
    }
    if (graphPtr->pixwin == None) {
	/*
	 * If there's no pixmap already to draw into, then this routine was
	 * called from Tcl_DoWhenIdle instead of DisplayGraph.  Create a
	 * temporary pixmap and reset the x,y coordinates to do a quick
	 * redraw of just the legend.
	 */
	graphPtr->pixwin = Tk_GetPixmap(graphPtr->display,
	    Tk_WindowId(graphPtr->tkwin), width, height,
	    Tk_Depth(graphPtr->tkwin));

	/* Clear the background of the pixmap */
	if (legendPtr->anchorPos.site > LEGEND_SITE_TOP) {
	    fillGC = graphPtr->plotFillGC;
	} else if (graphPtr->tile != NULL) {
	    fillGC = graphPtr->fillGC;
	    Blt_SetTileOrigin(graphPtr->tkwin, fillGC, legendPtr->x,
		legendPtr->y);
	}
	XFillRectangle(graphPtr->display, graphPtr->pixwin, fillGC, 0, 0, width,
	    height);
	x = y = 0;
	redraw = TRUE;
    }
    /*
     * Draw the background of the legend.
     */
    if (legendPtr->fillColor != NULL) {
	XFillRectangle(graphPtr->display, graphPtr->pixwin, fillGC, x, y, width,
	    height);
    }
    if (legendPtr->borderWidth > 0) {
	Tk_Draw3DRectangle(graphPtr->tkwin, graphPtr->pixwin, graphPtr->border,
	    x, y, width, height, legendPtr->borderWidth, legendPtr->relief);
    }
    x += legendPtr->borderWidth;
    y += legendPtr->borderWidth;

    Blt_InitTextAttrs(&textAttr, graphPtr->marginFg, (XColor *)NULL,
	legendPtr->fontPtr, 0.0, TK_ANCHOR_NW, TK_JUSTIFY_LEFT);
    textAttr.textGC = legendPtr->normalGC;

    counter = 0;
    startY = y;
    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	if (elemPtr->label == NULL) {
	    continue;		/* Skip this entry */
	}
	if (elemPtr->flags & LABEL_ACTIVE) {
	    Tk_Fill3DRectangle(graphPtr->tkwin, graphPtr->pixwin,
		legendPtr->activeBorder, x, y, legendPtr->entryWidth,
		legendPtr->entryHeight, legendPtr->entryBorderWidth,
		legendPtr->activeRelief);
	    textAttr.textGC = legendPtr->activeGC;
	} else {
	    textAttr.textGC = legendPtr->normalGC;
	}
	point.x = x + symbolX;
	point.y = y + symbolY;
	(*elemPtr->drawSymbolsProc) (graphPtr, elemPtr, symSize, &point, 1,
	    FALSE);
	Blt_DrawText(graphPtr->tkwin, graphPtr->pixwin, elemPtr->label,
	    &textAttr, x + labelX, y + legendPtr->entryBorderWidth +
	    legendPtr->ipadY);
	counter++;

	/* Check when to move to the next column */
	if ((counter % legendPtr->numRows) > 0) {
	    y += legendPtr->entryHeight;
	} else {
	    x += legendPtr->entryWidth;
	    y = startY;
	}
    }
    if (redraw) {
	(*graphPtr->crosshairs->toggleProc) (graphPtr);
	XCopyArea(graphPtr->display, graphPtr->pixwin,
	    Tk_WindowId(graphPtr->tkwin), graphPtr->drawGC, 0, 0,
	    width, height, legendPtr->x, legendPtr->y);
	(*graphPtr->crosshairs->toggleProc) (graphPtr);
	Tk_FreePixmap(graphPtr->display, graphPtr->pixwin);
	graphPtr->pixwin = None;
    }
}

/*
 * -----------------------------------------------------------------
 *
 * PrintLegend --
 *
 * -----------------------------------------------------------------
 */
static void
PrintLegend(graphPtr)
    Graph *graphPtr;
{
    Legend *legendPtr = (Legend *)graphPtr->legendPtr;
    int x, y, startY;
    Element *elemPtr;
    int labelX, symbolX, symbolY;
    int counter;
    Blt_ListItem *itemPtr;
    XPoint point;
    int symSize, midPoint;
    int width, height;
    TextAttributes textAttr;

    if ((!legendPtr->mapped) || (legendPtr->numLabels == 0) ||
	(legendPtr->numRows == 0) || (legendPtr->numCols == 0)) {
	return;
    }
    SetLegendOrigin(graphPtr);

    x = legendPtr->x, y = legendPtr->y;
    width = legendPtr->width - PADDING(legendPtr->padX);
    height = legendPtr->height - PADDING(legendPtr->padY);

    if (legendPtr->fillColor != NULL) {
	Blt_ClearBackgroundToPostScript(graphPtr);
	Blt_RectangleToPostScript(graphPtr, x, y, width, height);
    }
    if ((graphPtr->postscript->decorations) && (legendPtr->borderWidth > 0)) {
	Blt_3DRectangleToPostScript(graphPtr, graphPtr->border, x, y, width,
	    height, legendPtr->borderWidth, legendPtr->relief);
    }
    x += legendPtr->borderWidth;
    y += legendPtr->borderWidth;

    symSize = legendPtr->fontPtr->fm.ascent;
    midPoint = (symSize / 2) + 1 + legendPtr->entryBorderWidth;

    labelX = TEXTHEIGHT(legendPtr->fontPtr) + legendPtr->entryBorderWidth +
	legendPtr->ipadX + 5;
    symbolY = midPoint + legendPtr->ipadY;
    symbolX = midPoint + legendPtr->ipadX;

    Blt_InitTextAttrs(&textAttr, graphPtr->marginFg, (XColor *)NULL,
	legendPtr->fontPtr, 0.0, TK_ANCHOR_NW, TK_JUSTIFY_LEFT);
    counter = 0;
    startY = y;
    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	if (elemPtr->label == NULL) {
	    continue;		/* Skip this label */
	}
	if (elemPtr->flags & LABEL_ACTIVE) {
	    Blt_3DRectangleToPostScript(graphPtr, legendPtr->activeBorder,
		x, y, legendPtr->entryWidth, legendPtr->entryHeight,
		legendPtr->entryBorderWidth, legendPtr->activeRelief);
	    textAttr.fgColorPtr = legendPtr->activeTextColor;
	} else {
	    textAttr.fgColorPtr = legendPtr->textColor;
	}
	point.x = x + symbolX;
	point.y = y + symbolY;
	(*elemPtr->printSymbolsProc) (graphPtr, elemPtr, symSize, &point, 1,
	    FALSE);
	Blt_PrintText(graphPtr, elemPtr->label, &textAttr, x + labelX,
	    y + legendPtr->entryBorderWidth + legendPtr->ipadY);
	counter++;
	if ((counter % legendPtr->numRows) > 0) {
	    y += legendPtr->entryHeight;
	} else {
	    x += legendPtr->entryWidth;
	    y = startY;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureLegend --
 *
 * 	Routine to configure the legend.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new legend attributes.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureLegend(graphPtr, legendPtr, argc, argv, flags)
    Graph *graphPtr;
    Legend *legendPtr;
    int argc;
    char *argv[];
    int flags;
{
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;

    if (Tk_ConfigureWidget(graphPtr->interp, graphPtr->tkwin,
	    configSpecs, argc, argv, (char *)legendPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    gcMask = GCForeground | GCFont;
    gcValues.font = legendPtr->fontPtr->fid;
    gcValues.foreground = legendPtr->textColor->pixel;
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (legendPtr->normalGC != NULL) {
	Tk_FreeGC(graphPtr->display, legendPtr->normalGC);
    }
    legendPtr->normalGC = newGC;

    gcMask |= GCBackground;
    gcValues.foreground = legendPtr->activeTextColor->pixel;
    gcValues.background = Tk_3DBorderColor(legendPtr->activeBorder)->pixel;
    newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
    if (legendPtr->activeGC != NULL) {
	Tk_FreeGC(graphPtr->display, legendPtr->activeGC);
    }
    legendPtr->activeGC = newGC;

    if (legendPtr->fillColor != NULL) {
	gcMask = (GCBackground | GCForeground);
	gcValues.foreground = legendPtr->fillColor->pixel;
	gcValues.background = legendPtr->textColor->pixel;
	newGC = Tk_GetGC(graphPtr->tkwin, gcMask, &gcValues);
	if (legendPtr->fillGC != NULL) {
	    Tk_FreeGC(graphPtr->display, legendPtr->fillGC);
	}
	legendPtr->fillGC = newGC;
    }
    /*
     *  Update the layout of the graph (and redraw the elements) if
     *  any of the following legend options (all of which affect the
     *	size of the legend) have changed.
     *
     *		-activeborderwidth, -borderwidth
     *		-font
     *		-mapped
     *		-ipadx, -ipady, -padx, -pady
     *		-rows
     *
     *  If the position of the legend changed to/from the default
     *  position, also indicate that a new layout is needed.
     *
     */
    if (Blt_ConfigModified(configSpecs, "-*borderwidth", "-*pad?",
	    "-position", "-mapped", "-font", "-rows", (char *)NULL)) {
	graphPtr->flags |= COORDS_WORLD;
    }
    graphPtr->flags |= (REDRAW_WORLD | UPDATE_PIXMAP);
    Blt_RedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyLegend --
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources associated with the legend are freed.
 *
 *----------------------------------------------------------------------
 */
static void
DestroyLegend(graphPtr)
    Graph *graphPtr;
{
    Legend *legendPtr = (Legend *)graphPtr->legendPtr;

    Tk_FreeOptions(configSpecs, (char *)legendPtr, graphPtr->display, 0);
    if (legendPtr->normalGC != NULL) {
	Tk_FreeGC(graphPtr->display, legendPtr->normalGC);
    }
    if (legendPtr->activeGC != NULL) {
	Tk_FreeGC(graphPtr->display, legendPtr->activeGC);
    }
    ckfree((char *)legendPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_CreateLegend --
 *
 * 	Creates and initializes a legend structure with default settings
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_CreateLegend(graphPtr)
    Graph *graphPtr;
{
    Legend *legendPtr;

    legendPtr = (Legend *)ckcalloc(1, sizeof(Legend));
    if (legendPtr == NULL) {
	Panic("can't allocate legend structure");
    }
    legendPtr->mapped = TRUE;
    legendPtr->anchorPos.x = legendPtr->anchorPos.y = -SHRT_MAX;
    legendPtr->relief = TK_RELIEF_SUNKEN;
    legendPtr->activeRelief = TK_RELIEF_FLAT;
    legendPtr->entryBorderWidth = legendPtr->borderWidth = 2;
    legendPtr->ipadX = legendPtr->ipadY = 1;
    legendPtr->padX.side1 = legendPtr->padX.side2 = 4;
    legendPtr->padY.side1 = legendPtr->padY.side2 = 4;
    legendPtr->anchor = TK_ANCHOR_N;
    legendPtr->anchorPos.site = LEGEND_SITE_RIGHT;
    legendPtr->drawProc = (LegendDrawProc*)DrawLegend;
    legendPtr->printProc = (LegendPrintProc*)PrintLegend;
    legendPtr->destroyProc = (LegendDestroyProc*)DestroyLegend;
    legendPtr->geomProc = (LegendGeometryProc*)ComputeLegendGeometry;
    graphPtr->legendPtr = (GraphLegend *)legendPtr;
    return (ConfigureLegend(graphPtr, legendPtr, 0, (char **)NULL, 0));
}

/*
 *----------------------------------------------------------------------
 *
 * GetOper --
 *
 * 	Find the legend entry from the given argument.  The argument
 *	can be either a screen position "@x,y" or the name of an
 *	element.
 *
 *	I don't know how useful it is to test with the name of an
 *	element.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new legend attributes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
GetOper(graphPtr, legendPtr, argc, argv)
    Graph *graphPtr;
    Legend *legendPtr;
    int argc;			/* not used */
    char *argv[];
{
    register Element *elemPtr;

    if ((!legendPtr->mapped) || (legendPtr->numLabels == 0)) {
	return TCL_OK;
    }
    elemPtr = LocateElement(graphPtr, legendPtr, argv[3]);
    if (elemPtr != NULL) {
	Tcl_SetResult(graphPtr->interp, elemPtr->nameId, TCL_STATIC);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ActivateOper --
 *
 * 	Activates a particular label in the legend.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new legend attributes.
 *
 *----------------------------------------------------------------------
 */
static int
ActivateOper(graphPtr, legendPtr, argc, argv)
    Graph *graphPtr;
    Legend *legendPtr;
    int argc;
    char *argv[];
{
    Element *elemPtr;
    int active, redraw;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;
    register int i;

    active = (argv[2][0] == 'a') ? LABEL_ACTIVE : 0;
    redraw = 0;
    for (hPtr = Tcl_FirstHashEntry(&(graphPtr->elemTable), &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	elemPtr = (Element *)Tcl_GetHashValue(hPtr);
	for (i = 3; i < argc; i++) {
	    if (Tcl_StringMatch(elemPtr->nameId, argv[i])) {
		break;
	    }
	}
	if ((i < argc) && (active != (int)(elemPtr->flags & LABEL_ACTIVE))) {
	    elemPtr->flags ^= LABEL_ACTIVE;
	    if (elemPtr->label != NULL) {
		redraw++;
	    }
	}
    }
    if ((redraw) && (legendPtr->mapped)) {
	/*
	 * We need to redraw the legend. If there isn't a redraw already
	 * pending for the whole graph, we can redraw just the legend,
	 * calling the legend's display routine rather than the graph's.
	 * The window must be mapped though.
	 */
	if (legendPtr->anchorPos.site > LEGEND_SITE_TOP) {
	    graphPtr->flags |= UPDATE_PIXMAP;
	}
	if (graphPtr->flags & REDRAW_PENDING) {
	    graphPtr->flags |= REDRAW_WORLD;	/* Upgrade to entire graph */
	} else if (!(graphPtr->flags & DRAW_LEGEND)) {
	    if ((graphPtr->tkwin != NULL) && (Tk_IsMapped(graphPtr->tkwin))) {
		Tcl_DoWhenIdle((Tk_IdleProc *)DrawLegend, (ClientData)graphPtr);
		graphPtr->flags |= DRAW_LEGEND;
	    }
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CgetOper --
 *
 * 	Queries or resets options for the legend.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new legend attributes.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
CgetOper(graphPtr, legendPtr, argc, argv)
    Graph *graphPtr;
    Legend *legendPtr;
    int argc;
    char **argv;
{
    return (Tk_ConfigureValue(graphPtr->interp, graphPtr->tkwin, configSpecs,
	    (char *)legendPtr, argv[3], 0));
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOper --
 *
 * 	Queries or resets options for the legend.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new legend attributes.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOper(graphPtr, legendPtr, argc, argv)
    Graph *graphPtr;
    Legend *legendPtr;
    int argc;
    char **argv;
{
    int flags = TK_CONFIG_ARGV_ONLY;

    if (argc == 3) {
	return (Tk_ConfigureInfo(graphPtr->interp, graphPtr->tkwin,
		configSpecs, (char *)legendPtr, (char *)NULL, flags));
    } else if (argc == 4) {
	return (Tk_ConfigureInfo(graphPtr->interp, graphPtr->tkwin,
		configSpecs, (char *)legendPtr, argv[3], flags));
    }
    if (ConfigureLegend(graphPtr, legendPtr, argc - 3,
	    argv + 3, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_LegendOper --
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Legend is possibly redrawn.
 *
 *----------------------------------------------------------------------
 */

static Blt_OperSpec operSpecs[] =
{
    {"activate", 1, (Blt_OperProc) ActivateOper, 3, 0, "?pattern?...",},
    {"cget", 2, (Blt_OperProc) CgetOper, 4, 4, "option",},
    {"configure", 2, (Blt_OperProc) ConfigureOper, 3, 0, "?option value?...",},
    {"deactivate", 1, (Blt_OperProc) ActivateOper, 3, 0, "?pattern?...",},
    {"get", 1, (Blt_OperProc) GetOper, 4, 4, "index",},
};
static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

int
Blt_LegendOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_OperProc proc;
    int result;

    proc = Blt_LookupOperation(interp, numSpecs, operSpecs, BLT_OPER_ARG2,
	argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (graphPtr, graphPtr->legendPtr, argc, argv);
    return (result);
}
#endif /* !NO_GRAPH */
