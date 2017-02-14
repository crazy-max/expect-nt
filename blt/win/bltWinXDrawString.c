/* 
 * bltWinXDrawString.c (taken from tkWinDraw.c (Tk 7.6)) --
 *
 *	This file contains the Xlib emulation functions pertaining to
 *	actually drawing objects on a window.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 1994 Software Research Associates, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinDraw.c 1.23 96/10/17 17:34:29
 */

#include "tkWinInt.h"

/*
 * Translation table between X gc functions and Win32 raster op modes.
 */

static int ropModes[] = {
    R2_BLACK,			/* GXclear */
    R2_MASKPEN,			/* GXand */
    R2_MASKPENNOT,		/* GXandReverse */
    R2_COPYPEN,			/* GXcopy */
    R2_MASKNOTPEN,		/* GXandInverted */
    R2_NOT,			/* GXnoop */
    R2_XORPEN,			/* GXxor */
    R2_MERGEPEN,		/* GXor */
    R2_NOTMERGEPEN,		/* GXnor */
    R2_NOTXORPEN,		/* GXequiv */
    R2_NOT,			/* GXinvert */
    R2_MERGEPENNOT,		/* GXorReverse */
    R2_NOTCOPYPEN,		/* GXcopyInverted */
    R2_MERGENOTPEN,		/* GXorInverted */
    R2_NOTMASKPEN,		/* GXnand */
    R2_WHITE			/* GXset */
};

/*
 * Translation table between X gc functions and Win32 BitBlt op modes.  Some
 * of the operations defined in X don't have names, so we have to construct
 * new opcodes for those functions.  This is arcane and probably not all that
 * useful, but at least it's accurate.
 */

#define NOTSRCAND	(DWORD)0x00220326 /* dest = (NOT source) AND dest */
#define NOTSRCINVERT	(DWORD)0x00990066 /* dest = (NOT source) XOR dest */
#define SRCORREVERSE	(DWORD)0x00DD0228 /* dest = source OR (NOT dest) */
#define SRCNAND		(DWORD)0x007700E6 /* dest = NOT (source AND dest) */

static int bltModes[] = {
    BLACKNESS,			/* GXclear */
    SRCAND,			/* GXand */
    SRCERASE,			/* GXandReverse */
    SRCCOPY,			/* GXcopy */
    NOTSRCAND,			/* GXandInverted */
    PATCOPY,			/* GXnoop */
    SRCINVERT,			/* GXxor */
    SRCPAINT,			/* GXor */
    NOTSRCERASE,		/* GXnor */
    NOTSRCINVERT,		/* GXequiv */
    DSTINVERT,			/* GXinvert */
    SRCORREVERSE,		/* GXorReverse */
    NOTSRCCOPY,			/* GXcopyInverted */
    MERGEPAINT,			/* GXorInverted */
    SRCNAND,			/* GXnand */
    WHITENESS			/* GXset */
};


/*
 *----------------------------------------------------------------------
 *
 * TkWinGetDrawableDC --
 *
 *	Retrieve the DC from a drawable.
 *
 * Results:
 *	Returns the window DC for windows.  Returns a new memory DC
 *	for pixmaps.
 *
 * Side effects:
 *	Sets up the palette for the device context, and saves the old
 *	device context state in the passed in TkWinDCState structure.
 *
 *----------------------------------------------------------------------
 */

HDC
TkWinGetDrawableDC(display, d, state)
    Display *display;
    Drawable d;
    TkWinDCState* state;
{
    HDC dc;
    TkWinDrawable *twdPtr = (TkWinDrawable *)d;
    Colormap cmap;

    if (twdPtr->type != TWD_BITMAP) {
	TkWindow *winPtr = twdPtr->window.winPtr;
    
 	dc = GetDC(twdPtr->window.handle);
	if (winPtr == NULL) {
	    cmap = DefaultColormap(display, DefaultScreen(display));
	} else {
	    cmap = winPtr->atts.colormap;
	}
    } else {
	HDC dcMem;
	dc = GetDC(NULL);
	dcMem = CreateCompatibleDC(dc);
	ReleaseDC(NULL, dc);
	SelectObject(dcMem, twdPtr->bitmap.handle);
	dc = dcMem;
	cmap = twdPtr->bitmap.colormap;
    }
    state->palette = TkWinSelectPalette(dc, cmap);
    return dc;
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinReleaseDrawableDC --
 *
 *	Frees the resources associated with a drawable's DC.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Restores the old bitmap handle to the memory DC for pixmaps.
 *
 *----------------------------------------------------------------------
 */

void
TkWinReleaseDrawableDC(d, dc, state)
    Drawable d;
    HDC dc;
    TkWinDCState *state;
{
    TkWinDrawable *twdPtr = (TkWinDrawable *)d;
    SelectPalette(dc, state->palette, TRUE);
    RealizePalette(dc);
    if (twdPtr->type != TWD_BITMAP) {
	ReleaseDC(TkWinGetHWND(d), dc);
    } else {
	DeleteDC(dc);
    }
}

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
    _Xconst char* string;
    int length;
{
    HDC dc;
    HFONT oldFont;
    TkWinDCState state;

    display->request++;

    if (d == None) {
	return;
    }

    dc = TkWinGetDrawableDC(display, d, &state);
    SetROP2(dc, ropModes[gc->function]);

    if ((gc->fill_style == FillStippled
	    || gc->fill_style == FillOpaqueStippled)
	    && gc->stipple != None) {
	TkWinDrawable *twdPtr = (TkWinDrawable *)gc->stipple;
	HBRUSH oldBrush, stipple;
	HBITMAP oldBitmap, bitmap;
	HDC dcMem;
	TEXTMETRIC tm;
	SIZE size;

	if (twdPtr->type != TWD_BITMAP) {
	    panic("unexpected drawable type in stipple");
	}

	/*
	 * Select stipple pattern into destination dc.
	 */
	
	dcMem = CreateCompatibleDC(dc);

	stipple = CreatePatternBrush(twdPtr->bitmap.handle);
	SetBrushOrgEx(dc, gc->ts_x_origin, gc->ts_y_origin, NULL);
	oldBrush = SelectObject(dc, stipple);

	SetTextAlign(dcMem, TA_LEFT | TA_TOP);
	SetTextColor(dcMem, gc->foreground);
	SetBkMode(dcMem, TRANSPARENT);
	SetBkColor(dcMem, RGB(0, 0, 0));

	if (gc->font != None) {
	    oldFont = SelectObject(dcMem, (HFONT)gc->font);
	}

	/*
	 * Compute the bounding box and create a compatible bitmap.
	 */

	GetTextExtentPoint(dcMem, string, length, &size);
	GetTextMetrics(dcMem, &tm);
	size.cx -= tm.tmOverhang;
	bitmap = CreateCompatibleBitmap(dc, size.cx, size.cy);
	oldBitmap = SelectObject(dcMem, bitmap);

	/*
	 * The following code is tricky because fonts are rendered in multiple
	 * colors.  First we draw onto a black background and copy the white
	 * bits.  Then we draw onto a white background and copy the black bits.
	 * Both the foreground and background bits of the font are ANDed with
	 * the stipple pattern as they are copied.
	 */

	PatBlt(dcMem, 0, 0, size.cx, size.cy, BLACKNESS);
	TextOut(dcMem, 0, 0, string, length);
	BitBlt(dc, x, y - tm.tmAscent, size.cx, size.cy, dcMem,
		0, 0, 0xEA02E9);
	PatBlt(dcMem, 0, 0, size.cx, size.cy, WHITENESS);
	TextOut(dcMem, 0, 0, string, length);
	BitBlt(dc, x, y - tm.tmAscent, size.cx, size.cy, dcMem,
		0, 0, 0x8A0E06);

	/*
	 * Destroy the temporary bitmap and restore the device context.
	 */

	if (gc->font != None) {
	    SelectObject(dcMem, oldFont);
	}
	SelectObject(dcMem, oldBitmap);
	DeleteObject(bitmap);
	DeleteDC(dcMem);
	SelectObject(dc, oldBrush);
	DeleteObject(stipple);
    } else {
	SetTextAlign(dc, TA_LEFT | TA_BASELINE);
	SetTextColor(dc, gc->foreground);
	SetBkMode(dc, TRANSPARENT);
	if (gc->font != None) {
	    oldFont = SelectObject(dc, (HFONT)gc->font);
	}
	TextOut(dc, x, y, string, length);
	if (gc->font != None) {
	    SelectObject(dc, oldFont);
	}
    }
    TkWinReleaseDrawableDC(d, dc, &state);
}
