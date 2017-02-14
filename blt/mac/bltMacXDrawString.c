/* 
 * bltWinXDrawString.c (taken from tkWinDraw.c (Tk 7.6)) --
 *
 *	This file contains functions that preform drawing to
 *	Xlib windows.  Most of the functions simple emulate
 *	Xlib functions.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacDraw.c 1.43 96/09/05 13:38:18
 */

#include "tkInt.h"
#include "X.h"
#include "Xlib.h"
#include <stdio.h>
#include <tcl.h>

#include <Windows.h>
#include <Fonts.h>
#include <QDOffscreen.h>
#include "tkMacInt.h"

/*
 * Prototypes for functions used only in this file.
 */
static BitMapPtr 	MakeStippleMap _ANSI_ARGS_((Drawable drawable,
			    Drawable stipple));

/*
 *----------------------------------------------------------------------
 *
 * XDrawString --
 *
 *	Draw a single string in the current font.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Renders the specified string in the drawable.
 *
 *----------------------------------------------------------------------
 */

void 
XDrawString(display, d, gc, x, y, string, length)
    Display* display;
    Drawable d;
    GC gc;
    int x;
    int y;
    const char* string;
    int length;
{ 
    MacDrawable *macWin = (MacDrawable *) d;
    CGrafPtr saveWorld;
    GWorldPtr destPort;
    BitMapPtr stippleMap;
    GDHandle saveDevice;
    RGBColor macColor;
    short txFont, txFace, txSize;
    short family, style, size;


    destPort = TkMacGetDrawablePort(d);

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    TkMacSetUpClippingRgn(d);

    TkMacSetUpGraphicsPort(gc);

    TkMacFontInfo(gc->font, &family, &style, &size);
    txFont = qd.thePort->txFont;
    txFace = qd.thePort->txFace;
    txSize = qd.thePort->txSize;

    if ((gc->fill_style == FillStippled
	    || gc->fill_style == FillOpaqueStippled)
	    && gc->stipple != None) {
	Pixmap pixmap;
	GWorldPtr bufferPort;
	
	stippleMap = MakeStippleMap(d, gc->stipple);

	pixmap = Tk_GetPixmap(display, d, 	
		stippleMap->bounds.right, stippleMap->bounds.bottom, 0);
		
	bufferPort = TkMacGetDrawablePort(pixmap);
	SetGWorld(bufferPort, NULL);
	
	TextFont(family);
	TextSize(size);
	TextFace(style);
	
	if (TkSetMacColor(gc->foreground, &macColor) == true) {
	    RGBForeColor(&macColor);
	}

	ShowPen();
	MoveTo((short) 0, (short) 0);
	FillRect(&stippleMap->bounds, &qd.white);
	MoveTo((short) x, (short) y);
	DrawText(string, 0, (short) length);

	destPort = TkMacGetDrawablePort(d);
	SetGWorld(destPort, NULL);
	CopyDeepMask(&((GrafPtr) bufferPort)->portBits, stippleMap, 
		&((GrafPtr) destPort)->portBits, &stippleMap->bounds, &stippleMap->bounds,
		&((GrafPtr) destPort)->portRect /* Is this right? */, srcOr, NULL);
	/* TODO: this doesn't work quite right - it does a blend.   you can't draw white
	   text when you have a stipple.
	 */
		
	Tk_FreePixmap(display, pixmap);
	ckfree(stippleMap->baseAddr);
	ckfree((char *)stippleMap);
    } else {
	TextFont(family);
	TextSize(size);
	TextFace(style);
	
	if (TkSetMacColor(gc->foreground, &macColor) == true) {
	    RGBForeColor(&macColor);
	}

	ShowPen();
	MoveTo((short) (macWin->xOff + x), (short) (macWin->yOff + y));
	DrawText(string, 0, (short) length);
    }
    
    TextFont(txFont);
    TextSize(txSize);
    TextFace(txFace);
    SetGWorld(saveWorld, saveDevice);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacSetUpGraphicsPort --
 *
 *	Set up the graphics port from the given GC.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The current port is adjusted.
 *
 *----------------------------------------------------------------------
 */

static void
TkMacSetUpGraphicsPort(gc)
    GC gc;
{
    RGBColor macColor;

    if (gPenPat == NULL) {
	gPenPat = NewPixPat();
    }
    
    if (TkSetMacColor(gc->foreground, &macColor) == true) {
        /* TODO: cache RGBPats for preformace - measure gains...  */
	MakeRGBPat(gPenPat, &macColor);
    }
    
    PenNormal();
    if(gc->function == GXxor) {
	PenMode(patXor);
    }
    if (gc->line_width > 1) {
	PenSize(gc->line_width, gc->line_width);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacSetUpClippingRgn --
 *
 *	Set up the clipping region so that drawing only occurs on the
 *	specified X subwindow.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The clipping region in the current port is changed.
 *
 *----------------------------------------------------------------------
 */

void
TkMacSetUpClippingRgn(drawable)
    Drawable drawable;
{
    MacDrawable *macDraw = (MacDrawable *) drawable;

    if (macDraw->winPtr != NULL) {
	if (macDraw->flags & TK_CLIP_INVALID) {
	    TkMacUpdateClipRgn(macDraw->winPtr);
	}

	if (macDraw->clipRgn != NULL) {
	    SetClip(macDraw->clipRgn);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MakeStippleMap --
 *
 *	Given a drawable and a stipple pattern this function draws the
 *	pattern repeatedly over the drawable.  The drawable can then
 *	be used as a mask for bit-bliting a stipple pattern over an
 *	object.
 *
 * Results:
 *	A BitMap data structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static BitMapPtr
MakeStippleMap(drawable, stipple)
    Drawable drawable;
    Drawable stipple;
{
    MacDrawable *destDraw = (MacDrawable *) drawable;
    GWorldPtr destPort;
    BitMapPtr bitmapPtr;
    int width, height, stippleHeight, stippleWidth;
    int i, j;
    char * data;
    Rect bounds;

    destPort = TkMacGetDrawablePort(drawable);
    width = destPort->portRect.right - destPort->portRect.left;
    height = destPort->portRect.bottom - destPort->portRect.top;
    
    bitmapPtr = (BitMap *) ckalloc(sizeof(BitMap));
    data = (char *) ckalloc(height * ((width / 8) + 1));
    bitmapPtr->bounds.top = bitmapPtr->bounds.left = 0;
    bitmapPtr->bounds.right = (short) width;
    bitmapPtr->bounds.bottom = (short) height;
    bitmapPtr->baseAddr = data;
    bitmapPtr->rowBytes = (width / 8) + 1;

    destPort = TkMacGetDrawablePort(stipple);
    stippleWidth = destPort->portRect.right - destPort->portRect.left;
    stippleHeight = destPort->portRect.bottom - destPort->portRect.top;

    for (i = 0; i < height; i += stippleHeight) {
	for (j = 0; j < width; j += stippleWidth) {
	    bounds.left = j;
	    bounds.top = i;
	    bounds.right = j + stippleWidth;
	    bounds.bottom = i + stippleHeight;
	    
	    CopyBits(&((GrafPtr) destPort)->portBits, bitmapPtr, 
		&((GrafPtr) destPort)->portRect, &bounds, srcCopy, NULL);
	}
    }
    return bitmapPtr;
}
