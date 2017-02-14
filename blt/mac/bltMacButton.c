/* 
 * bltMacButton.c --
 *
 *	This file implements the Macintosh specific portion of the
 *	button widgets.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacButton.c 1.6 96/12/11 11:25:25
 */

#include "bltButton.h"
#include "tkMacInt.h"
#include <Controls.h>

/*
 * Declaration of Unix specific button structure.
 */

typedef struct MacButton {
    BltButton info;		/* Generic button info. */
    ControlRef buttonHandle;	/* Handle to the Button control struct. */
    CCTabHandle ccTabHandle;	/* Handle to color table for control. */
} MacButton;

/*
 * Some defines used to control what is drawn.
 */
#define DRAW_LABEL	0		/* Labels are treated genericly. */
#define DRAW_CONTROL	1		/* Draw using the Native control. */
#define DRAW_CUSTOM	2		/* Make our own button drawing. */

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		UpdateControlColors _ANSI_ARGS_((BltButton *butPtr,
			    RGBColor *saveColorPtr));
static void		DrawBufferedControl _ANSI_ARGS_((MacButton *macButPtr,
			    GWorldPtr destPort));
static void		ChangeBackgroundWindowColor _ANSI_ARGS_((
			    WindowRef macintoshWindow, RGBColor rgbColor, RGBColor *oldColor));			/* The old color of the background. */

/*
 * The class procedure table for the button widgets.
 */

TkClassProcs BltpButtonProcs = { 
    NULL,			/* createProc. */
    BltButtonWorldChanged,	/* geometryProc. */
    NULL			/* modalProc. */
};

/*
 *----------------------------------------------------------------------
 *
 * BltpCreateButton --
 *
 *	Allocate a new BltButton structure.
 *
 * Results:
 *	Returns a newly allocated BltButton structure.
 *
 * Side effects:
 *	Registers an event handler for the widget.
 *
 *----------------------------------------------------------------------
 */

BltButton *
BltpCreateButton(
    Tk_Window tkwin)
{
    MacButton *butPtr = (MacButton *)ckalloc(sizeof(MacButton));

    butPtr->buttonHandle = NULL;
    butPtr->ccTabHandle = NULL;
    
    return (BltButton *) butPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * BltpDisplayButton --
 *
 *	This procedure is invoked to display a button widget.  It is
 *	normally invoked as an idle handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the button in its
 *	current mode.  The REDRAW_PENDING flag is cleared.
 *
 *----------------------------------------------------------------------
 */

void
BltpDisplayButton(
    ClientData clientData)	/* Information about widget. */
{
    MacButton *macButPtr = (MacButton *) clientData;
    BltButton *butPtr = (BltButton *) clientData;
    Pixmap pixmap;
    GC gc;
    Tk_3DBorder border;
    int x = 0;			/* Initialization only needed to stop
				 * compiler warning. */
    int y, relief;
    register Tk_Window tkwin = butPtr->tkwin;
    int width, height;
    int offset;			/* 0 means this is a normal widget.  1 means
				 * it is an image button, so we offset the
				 * image to make the button appear to move
				 * up and down as the relief changes. */
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    int drawType, borderWidth;
#ifndef NO_TILEBUTTON
    Blt_Tile tile;
    GC tileGC;
#endif
    
    GetGWorld(&saveWorld, &saveDevice);

    butPtr->flags &= ~REDRAW_PENDING;
    if ((butPtr->tkwin == NULL) || !Tk_IsMapped(tkwin)) {
	return;
    }

#ifndef NO_TILEBUTTON
    tile = NULL;
    tileGC = None;
#endif
    border = butPtr->normalBorder;
    if ((butPtr->state == tkDisabledUid) && (butPtr->disabledFg != NULL)) {
	gc = butPtr->disabledGC;

    } else if ((butPtr->type == TYPE_BUTTON) && (butPtr->state == tkActiveUid)) {
	gc = butPtr->activeTextGC;
	border = butPtr->activeBorder;
#ifndef NO_TILEBUTTON
	tile = butPtr->activeTile;
	tileGC = butPtr->tileGC;
#endif
    } else {
	gc = butPtr->normalTextGC;
#ifndef NO_TILEBUTTON
	tile = butPtr->tile;
	tileGC = butPtr->tileGC;
#endif
    }
    if ((butPtr->flags & SELECTED) && (butPtr->state != tkActiveUid)
	    && (butPtr->selectBorder != NULL) && !butPtr->indicatorOn) {
	border = butPtr->selectBorder;
    }

    /*
     * Override the relief specified for the button if this is a
     * checkbutton or radiobutton and there's no indicator.
     */

    relief = butPtr->relief;
    if ((butPtr->type >= TYPE_CHECK_BUTTON) && !butPtr->indicatorOn) {
	relief = (butPtr->flags & SELECTED) ? TK_RELIEF_SUNKEN
		: TK_RELIEF_RAISED;
    }

    offset = ((butPtr->type == TYPE_BUTTON) && 
	((butPtr->image != NULL) || (butPtr->bitmap != None)));

    /*
     * In order to avoid screen flashes, this procedure redraws
     * the button in a pixmap, then copies the pixmap to the
     * screen in a single operation.  This means that there's no
     * point in time where the on-sreen image has been cleared.
     */

    pixmap = Tk_GetPixmap(butPtr->display, Tk_WindowId(tkwin),
	    Tk_Width(tkwin), Tk_Height(tkwin), Tk_Depth(tkwin));
#ifndef NO_TILEBUTTON
    if (tileGC != NULL) {
        Blt_SetTileOrigin(tkwin, tileGC, 0, 0);
        XFillRectangle(butPtr->display, pixmap, tileGC, 0, 0, Tk_Width(tkwin),
            Tk_Height(tkwin));
    } else
#endif
    Tk_Fill3DRectangle(tkwin, pixmap, butPtr->normalBorder, 0, 0,
	    Tk_Width(tkwin), Tk_Height(tkwin), 0, TK_RELIEF_FLAT);

   
    if (butPtr->type == TYPE_LABEL) {
	drawType = DRAW_LABEL;
    } else if (butPtr->type == TYPE_BUTTON) {
	if ((butPtr->image == None) && (butPtr->bitmap == None)) {
	    drawType = DRAW_CONTROL;
	} else {
	    drawType = DRAW_CUSTOM;
	}
    } else {
	if (butPtr->indicatorOn) {
	    drawType = DRAW_CONTROL;
	} else {
	    drawType = DRAW_CUSTOM;
	}
    }

    /*
     * Draw the native portion of the buttons.  Start by creating the control
     * if it doesn't already exist.  Then configure the Macintosh control from
     * the Tk info.  Finally, we call Draw1Control to draw to the screen.
     */

    if ((drawType == DRAW_CONTROL) && (macButPtr->buttonHandle == NULL)) {
	Rect r = {0, 0, 1, 1};
	SInt16 procID;
        
	switch (butPtr->type) {
	    case TYPE_BUTTON:
		procID = pushButProc;
		break;
	    case TYPE_CHECK_BUTTON:
		procID = checkBoxProc;
		break;
	    case TYPE_RADIO_BUTTON:
		procID = radioButProc;
		break;
	}
	destPort = TkMacGetDrawablePort(Tk_WindowId(tkwin));
	SetGWorld(destPort, NULL);
	macButPtr->buttonHandle = NewControl((WindowRef) destPort, &r, "\p",
		false, 1, 0, 1, procID, (SInt32) macButPtr);
    }

    if (drawType == DRAW_CONTROL) {
	MacDrawable *macDraw;
	int windowColorChanged;
	RGBColor saveBackColor, dummyColor;
	
	macDraw = (MacDrawable *) Tk_WindowId(tkwin);
	(**macButPtr->buttonHandle).contrlRect.left = macDraw->xOff + butPtr->inset;
	(**macButPtr->buttonHandle).contrlRect.top = macDraw->yOff + butPtr->inset;
	(**macButPtr->buttonHandle).contrlRect.right = macDraw->xOff + 
	    Tk_Width(tkwin) - butPtr->inset;
	(**macButPtr->buttonHandle).contrlRect.bottom = macDraw->yOff +
	    Tk_Height(tkwin) - butPtr->inset;
	if ((**macButPtr->buttonHandle).contrlVis != 255) {
	    (**macButPtr->buttonHandle).contrlVis = 255;
	}
	if (butPtr->flags & SELECTED) {
	    (**macButPtr->buttonHandle).contrlValue = 1;
	} else {
	    (**macButPtr->buttonHandle).contrlValue = 0;
	}
	if (butPtr->state == tkActiveUid) {
	    switch (butPtr->type) {
		case TYPE_BUTTON:
		    (**macButPtr->buttonHandle).contrlHilite = kControlButtonPart;
		    break;
		case TYPE_RADIO_BUTTON:
		    (**macButPtr->buttonHandle).contrlHilite = kControlRadioButtonPart;
		    break;
		case TYPE_CHECK_BUTTON:
		    (**macButPtr->buttonHandle).contrlHilite = kControlCheckBoxPart;
		    break;
	    }
	} else if (butPtr->state == tkDisabledUid) {
	    (**macButPtr->buttonHandle).contrlHilite = kControlInactivePart;
	} else {
	    (**macButPtr->buttonHandle).contrlHilite = kControlNoPart;
	}
	borderWidth = 0;
	
	/*
	 * This part uses Macintosh rather than Tk calls to draw
	 * to the screen.  Make sure the ports etc. are set correctly.
	 */
	destPort = TkMacGetDrawablePort(pixmap);
	SetGWorld(destPort, NULL);
	windowColorChanged = UpdateControlColors(butPtr, &saveBackColor);
	DrawBufferedControl(macButPtr, destPort);
	if (windowColorChanged) {
	    ChangeBackgroundWindowColor((**macButPtr->buttonHandle).contrlOwner,
		saveBackColor, &dummyColor);
	}
    }

    if ((drawType == DRAW_CUSTOM) || (drawType == DRAW_LABEL)) {
	borderWidth = butPtr->borderWidth;
    }

    /*
     * Display image or bitmap or text for button.
     */

    if (butPtr->image != None) {
	Tk_SizeOfImage(butPtr->image, &width, &height);

	imageOrBitmap:
	TkComputeAnchor(butPtr->anchor, tkwin, 0, 0,
		butPtr->indicatorSpace + width, height, &x, &y);
	x += butPtr->indicatorSpace;

	x += offset;
	y += offset;
	if (relief == TK_RELIEF_RAISED) {
	    x -= offset;
	    y -= offset;
	} else if (relief == TK_RELIEF_SUNKEN) {
	    x += offset;
	    y += offset;
	}
	if (butPtr->image != NULL) {
	    if ((butPtr->selectImage != NULL) && (butPtr->flags & SELECTED)) {
		Tk_RedrawImage(butPtr->selectImage, 0, 0, width, height, pixmap,
			x, y);
	    } else {
		Tk_RedrawImage(butPtr->image, 0, 0, width, height, pixmap,
			x, y);
	    }
	} else {
	    XSetClipOrigin(butPtr->display, gc, x, y);
	    XCopyPlane(butPtr->display, butPtr->bitmap, pixmap, gc, 0, 0,
		    (unsigned int) width, (unsigned int) height, x, y, 1);
	    XSetClipOrigin(butPtr->display, gc, 0, 0);
	}
	y += height/2;
    } else if (butPtr->bitmap != None) {
	Tk_SizeOfBitmap(butPtr->display, butPtr->bitmap, &width, &height);
	goto imageOrBitmap;
    } else {
	TkComputeAnchor(butPtr->anchor, tkwin, butPtr->padX, butPtr->padY,
		butPtr->indicatorSpace + butPtr->textWidth, butPtr->textHeight,
		&x, &y);

	x += butPtr->indicatorSpace;

	Tk_DrawTextLayout(butPtr->display, pixmap, gc, butPtr->textLayout,
		x, y, 0, -1);
	y += butPtr->textHeight/2;
    }

    /*
     * If the button is disabled with a stipple rather than a special
     * foreground color, generate the stippled effect.  If the widget
     * is selected and we use a different background color when selected,
     * must temporarily modify the GC.
     */

    if ((butPtr->state == tkDisabledUid)
	    && ((butPtr->disabledFg == NULL) || (butPtr->image != NULL))) {
	if ((butPtr->flags & SELECTED) && !butPtr->indicatorOn
		&& (butPtr->selectBorder != NULL)) {
	    XSetForeground(butPtr->display, butPtr->disabledGC,
		    Tk_3DBorderColor(butPtr->selectBorder)->pixel);
	}
	XFillRectangle(butPtr->display, pixmap, butPtr->disabledGC,
		butPtr->inset, butPtr->inset,
		(unsigned) (Tk_Width(tkwin) - 2*butPtr->inset),
		(unsigned) (Tk_Height(tkwin) - 2*butPtr->inset));
	if ((butPtr->flags & SELECTED) && !butPtr->indicatorOn
		&& (butPtr->selectBorder != NULL)) {
	    XSetForeground(butPtr->display, butPtr->disabledGC,
		    Tk_3DBorderColor(butPtr->normalBorder)->pixel);
	}
    }

    /*
     * Draw the border and traversal highlight last.  This way, if the
     * button's contents overflow they'll be covered up by the border.
     */

    if (relief != TK_RELIEF_FLAT) {
	Tk_Draw3DRectangle(tkwin, pixmap, border,
		butPtr->inset, butPtr->inset,
		Tk_Width(tkwin) - 2*butPtr->inset,
		Tk_Height(tkwin) - 2*butPtr->inset,
		borderWidth, relief);
    }

    /*
     * Copy the information from the off-screen pixmap onto the screen,
     * then delete the pixmap.
     */

    XCopyArea(butPtr->display, pixmap, Tk_WindowId(tkwin),
	    butPtr->copyGC, 0, 0, (unsigned) Tk_Width(tkwin),
	    (unsigned) Tk_Height(tkwin), 0, 0);
    Tk_FreePixmap(butPtr->display, pixmap);

    SetGWorld(saveWorld, saveDevice);
}

/*
 *----------------------------------------------------------------------
 *
 * BltpComputeButtonGeometry --
 *
 *	After changes in a button's text or bitmap, this procedure
 *	recomputes the button's geometry and passes this information
 *	along to the geometry manager for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The button's window may change size.
 *
 *----------------------------------------------------------------------
 */

void
BltpComputeButtonGeometry(
    BltButton *butPtr)	/* Button whose geometry may have changed. */
{
    int width, height, avgWidth;
    Tk_FontMetrics fm;

    if (butPtr->highlightWidth < 0) {
	butPtr->highlightWidth = 0;
    }
    if ((butPtr->type == TYPE_BUTTON) && (butPtr->image == None) && (butPtr->bitmap == None)) {
	butPtr->inset = 0;
    } else if ((butPtr->type != TYPE_LABEL) && butPtr->indicatorOn) {
	butPtr->inset = 0;
    } else {
	butPtr->inset = butPtr->borderWidth;
    }

    /*
     * The highlight width corasponds to the default ring on the Macintosh.
     * As such, the highlight width is only added if the button is the default
     * button.  The actual width of the default ring is one less than the
     * highlight width as there is also one pixel of spacing.
     */

    if (butPtr->isDefault) {
	butPtr->inset += butPtr->highlightWidth;
    }
    butPtr->indicatorSpace = 0;
    if (butPtr->image != NULL) {
	Tk_SizeOfImage(butPtr->image, &width, &height);
	imageOrBitmap:
	if (butPtr->width > 0) {
	    width = butPtr->width;
	}
	if (butPtr->height > 0) {
	    height = butPtr->height;
	}
	if ((butPtr->type >= TYPE_CHECK_BUTTON) && butPtr->indicatorOn) {
	    butPtr->indicatorSpace = height;
	    if (butPtr->type == TYPE_CHECK_BUTTON) {
		butPtr->indicatorDiameter = (65*height)/100;
	    } else {
		butPtr->indicatorDiameter = (75*height)/100;
	    }
	}
    } else if (butPtr->bitmap != None) {
	Tk_SizeOfBitmap(butPtr->display, butPtr->bitmap, &width, &height);
	goto imageOrBitmap;
    } else {
	Tk_FreeTextLayout(butPtr->textLayout);
	butPtr->textLayout = Tk_ComputeTextLayout(butPtr->tkfont,
		butPtr->text, -1, butPtr->wrapLength, butPtr->justify, 0,
		&butPtr->textWidth, &butPtr->textHeight);

	width = butPtr->textWidth;
	height = butPtr->textHeight;
	avgWidth = Tk_TextWidth(butPtr->tkfont, "0", 1);
	Tk_GetFontMetrics(butPtr->tkfont, &fm);

	if (butPtr->width > 0) {
	    width = butPtr->width * avgWidth;
	}
	if (butPtr->height > 0) {
	    height = butPtr->height * fm.linespace;
	}
	if ((butPtr->type >= TYPE_CHECK_BUTTON) && butPtr->indicatorOn) {
	    butPtr->indicatorDiameter = fm.linespace;
	    if (butPtr->type == TYPE_CHECK_BUTTON) {
		butPtr->indicatorDiameter = (80*butPtr->indicatorDiameter)/100;
	    }
	    butPtr->indicatorSpace = butPtr->indicatorDiameter + avgWidth;
	}
    }

    /*
     * When issuing the geometry request, add extra space for the indicator,
     * if any, and for the border and padding, plus if this is an image two 
     * extra pixels so the display can be offset by 1 pixel in either direction 
     * for the raised or lowered effect.
     */

    if ((butPtr->image == NULL) && (butPtr->bitmap == None)) {
	width += 2*butPtr->padX;
	height += 2*butPtr->padY;
    }
    if ((butPtr->type == TYPE_BUTTON) && 
	((butPtr->image != NULL) || (butPtr->bitmap != None))) {
	width += 2;
	height += 2;
    }
    Tk_GeometryRequest(butPtr->tkwin, (int) (width + butPtr->indicatorSpace
	    + 2*butPtr->inset), (int) (height + 2*butPtr->inset));
    Tk_SetInternalBorder(butPtr->tkwin, butPtr->inset);
}

/*
 *----------------------------------------------------------------------
 *
 * BltpDestroyButton --
 *
 *	Free data structures associated with the button control.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Restores the default control state.
 *
 *----------------------------------------------------------------------
 */

void
BltpDestroyButton(
    BltButton *butPtr)
{
    MacButton *macButPtr = (MacButton *) butPtr;

    if (macButPtr->buttonHandle != NULL) {
	DisposeControl(macButPtr->buttonHandle);
	macButPtr->buttonHandle = NULL;
    }
    if (macButPtr->ccTabHandle != NULL) {
	DisposeHandle((Handle) macButPtr->ccTabHandle);
	macButPtr->ccTabHandle = NULL;
    }
}

/*
 *--------------------------------------------------------------
 *
 * DrawBufferedControl --
 *
 *	This function draws the control to the off screen bitmap.
 *	This isn't as straight forward as it should be due to a
 *	bug in Apple's control code.  (When drawing to a GWorld
 *	the Apple control will draw checkboxes as buttons!)  This
 *	function, instead draws the control to a PICT and then
 *	draws the PICT to the GWorld.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Control is drawn to the offscreen bitmap.
 *
 *--------------------------------------------------------------
 */

static void
DrawBufferedControl(
    MacButton *macButPtr,	/* Mac Tk button. */
    GWorldPtr destPort)		/* Off screen GWorld. */
{
    Rect picFrame = (**macButPtr->buttonHandle).contrlRect;
    PicHandle picH;

    /*
     * First draw the control into a picture on the port
     * that the control belongs to.
     */
    SetPort((**macButPtr->buttonHandle).contrlOwner);
    TkMacSetUpClippingRgn(Tk_WindowId(macButPtr->info.tkwin));
    picH = OpenPicture(&picFrame);
    if (!picH) {
	panic("Failed to open Picture in DrawBufferedControl.");
    }
    
    /*
     * Draw the control.  If this is default button we need to 
     * draw the default ring for the button.
     */
    Draw1Control(macButPtr->buttonHandle);
    if ((macButPtr->info.type == TYPE_BUTTON) && macButPtr->info.isDefault) {
	Rect box = (**macButPtr->buttonHandle).contrlRect;
	RGBColor rgbColor;

	TkSetMacColor(macButPtr->info.highlightColorPtr->pixel, &rgbColor);
	RGBForeColor(&rgbColor);
	PenSize(macButPtr->info.highlightWidth - 1, macButPtr->info.highlightWidth - 1);
	InsetRect(&box, -macButPtr->info.highlightWidth, -macButPtr->info.highlightWidth);
	FrameRoundRect(&box, 16, 16);
    }

    ClosePicture();

    /*
     * Now we will draw the picture into our offscreen GWorld.
     */
    SetGWorld (destPort, NULL);
    OffsetRect(&picFrame, -picFrame.left, -picFrame.top);
    OffsetRect(&picFrame, macButPtr->info.inset, macButPtr->info.inset);
    DrawPicture(picH, &picFrame);

    KillPicture (picH);
}

/*
 *--------------------------------------------------------------
 *
 * UpdateControlColors --
 *
 *	This function will review the colors used to display
 *	a Macintosh button.  If any non-standard colors are
 *	used we create a custom palette for the button, populate
 *	with the colors for the button and install the palette.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Macintosh control may get a custom palette installed.
 *
 *--------------------------------------------------------------
 */

static int
UpdateControlColors(
    BltButton *butPtr,	/* Tk button. */
    RGBColor *saveColorPtr)
{
    MacButton *macButPtr = (MacButton *) butPtr;
    XColor *xcolor;
    CCTabHandle ccTabHandle;
    
    xcolor = Tk_3DBorderColor(butPtr->normalBorder);
    if ((macButPtr->ccTabHandle == NULL) && 
	((xcolor->pixel >> 24) == CONTROL_BODY_PIXEL) &&
	((butPtr->normalFg->pixel >> 24) == CONTROL_TEXT_PIXEL) && 
	((butPtr->highlightColorPtr->pixel >> 24) == CONTROL_FRAME_PIXEL)) {
	return false;
    }
    if (macButPtr->ccTabHandle == NULL) {
	ccTabHandle = (CCTabHandle) NewHandle(sizeof(CtlCTab));
    } else {
	ccTabHandle = macButPtr->ccTabHandle;
    }
    
    (**ccTabHandle).ccSeed = 0;
    (**ccTabHandle).ccRider = 0;
    (**ccTabHandle).ctSize = 3;
    (**ccTabHandle).ctTable[0].value = cBodyColor;
    TkSetMacColor(xcolor->pixel,
	    &(**ccTabHandle).ctTable[0].rgb);
    (**ccTabHandle).ctTable[1].value = cTextColor;
    TkSetMacColor(butPtr->normalFg->pixel,
	    &(**ccTabHandle).ctTable[1].rgb);
    (**ccTabHandle).ctTable[2].value = cFrameColor;
    TkSetMacColor(butPtr->highlightColorPtr->pixel,
	    &(**ccTabHandle).ctTable[2].rgb);

    if (macButPtr->ccTabHandle == NULL) {
	macButPtr->ccTabHandle = ccTabHandle;
	SetControlColor(macButPtr->buttonHandle, ccTabHandle);
    }
    
    if (((xcolor->pixel >> 24) != CONTROL_BODY_PIXEL) && 
	((butPtr->type == TYPE_CHECK_BUTTON) || (butPtr->type == TYPE_CHECK_BUTTON))) {
	RGBColor newColor;
	
	TkSetMacColor(xcolor->pixel, &newColor);
	ChangeBackgroundWindowColor((**macButPtr->buttonHandle).contrlOwner,
		newColor, saveColorPtr);
	return true;
    }
    
    return false;
}

/*
 *--------------------------------------------------------------
 *
 * ChangeBackgroundWindowColor --
 *
 *	Change the window's background color.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Window's color table will be adjusted.
 *
 *--------------------------------------------------------------
 */

static void
ChangeBackgroundWindowColor(
    WindowRef macintoshWindow,		/* A Mac window whose color to change. */
    RGBColor rgbColor,			/* The new RGB Color for the background. */
    RGBColor *oldColor)			/* The old color of the background. */
{
    AuxWinHandle auxWinHandle;
    WCTabHandle winCTabHandle;
    short ctIndex;
    ColorSpecPtr rgbScan;
	
    GetAuxWin(macintoshWindow, &auxWinHandle);
    winCTabHandle = (WCTabHandle) ((**auxWinHandle).awCTable);
    ctIndex = (**winCTabHandle).ctSize;
    while (ctIndex > -1) {
	rgbScan = ctIndex + (**winCTabHandle).ctTable;

	if (rgbScan->value == wContentColor) {
	    *oldColor = rgbScan->rgb;
	    rgbScan->rgb = rgbColor;
	    CTabChanged ((CTabHandle) winCTabHandle);
	}
	ctIndex--;
    }
}
/*
  TODO list for native buttons
  * image buttons
  * labels
  * test suite
  * flashing
  * disabled buttons
  * corners on buttons
 */
