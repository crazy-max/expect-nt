/*
 * bltText.c --
 *
 *	This module implements text drawing procedures for the BLT
 *	toolkit.
 *
 * Copyright 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#include <X11/Xutil.h>

/*
 * -----------------------------------------------------------------
 *
 * Blt_GetTextExtents --
 *
 *	Get the extents of a possibly multiple-lined text string.
 *
 * Results:
 *	Returns via *widthPtr* and *heightPtr* the dimensions of
 *	the text string.
 *
 * -----------------------------------------------------------------
 */
void
Blt_GetTextExtents(fontPtr, string, widthPtr, heightPtr)
    TkFont *fontPtr;
    char string[];
    int *widthPtr, *heightPtr;
{
    Tk_TextLayout layout;

    layout = Tk_ComputeTextLayout((Tk_Font)fontPtr, string, -1,
	0, TK_JUSTIFY_CENTER, 0, widthPtr, heightPtr);
    Tk_FreeTextLayout(layout);
    BLT_WDEBUG(("Extents=%d x %d\n", *widthPtr, *heightPtr);)
}

static int
JustifyOffset(attrPtr, text, length, widthPtr)
    TextAttributes *attrPtr;	/* Attributes of the text string. Specifically,
				 * holds the desired justification of the text
				 * and width of the entire text block (text
				 * may be more than one line) to calculate the
				 * needed offset. */
    char *text;			/* Text string. May not be NUL terminated. */
    int length;			/* # of characters in the text string. */
    int *widthPtr;		/* (out) If non-NULL, the contents of this
				 * variable will be set to width of the text
				 * string in pixels. */
{
    int width;
    int dummy;			/* unused return value from ComputeTextLayout */
    Tk_TextLayout layout;

    layout = Tk_ComputeTextLayout((Tk_Font)attrPtr->fontPtr, text,
	length, 0, attrPtr->justify, 0, &width, &dummy);
    Tk_FreeTextLayout(layout);
    if (widthPtr != NULL) {
	*widthPtr = width;	/* Save the width (for printing PostScript) */
    }
    switch (attrPtr->justify) {
	default:
	case TK_JUSTIFY_LEFT:
	    return 0;		/* No offset for left justified text strings */
	case TK_JUSTIFY_RIGHT:
	    return (attrPtr->regionWidth - width);
	case TK_JUSTIFY_CENTER:
	    return (attrPtr->regionWidth - width) / 2;
    }
}

/*
 * -----------------------------------------------------------------
 *
 * DrawCompoundText --
 *
 *	Draw a possibly multi-lined text string.
 *
 * Results:
 *	None.
 *
 * -----------------------------------------------------------------
 */
static void
DrawCompoundText(display, draw, gc, attrPtr, text, x, y)
    Display *display;
    Drawable draw;
    GC gc;
    TextAttributes *attrPtr;
    char *text;
    int x, y;			/* Window coordinates to draw text */
{
    int length;			/* # of characters in the current line */
    int lineHeight;
    register char *p;
    int justifyX;

    BLT_WDEBUG(("DrawCompoundText: x=%d y=%d gc=%08lX fg=%08lxX bg=%08lxX %s\n",
        x,y, gc, gc->foreground, gc->background, text);)
    y += attrPtr->fontPtr->fm.ascent;
    lineHeight = TEXTHEIGHT(attrPtr->fontPtr);
    length = 0;
    for (p = text; *p != '\0'; p++) {
	if (*p == '\n') {
	    if (length > 0) {
		justifyX = x + JustifyOffset(attrPtr, text, length,
		    (int *)NULL);
		XDrawString(display, draw, gc, justifyX, y, text, length);
	    }
	    y += lineHeight;
	    text = p + 1;	/* Start the text on the next line */
	    length = 0;		/* Reset to indicate the start of a new line */
	    continue;
	}
	length++;
    }
    if ((length > 0) && (*(p - 1) != '\n')) {
	justifyX = x + JustifyOffset(attrPtr, text, length, (int *)NULL);
	XDrawString(display, draw, gc, justifyX, y, text, length);
    }
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_GetBoundingBox
 *
 * 	Computes the size the bounding box of a rotated rectangle, given
 *	by the width, height, and rotation.  The rectangle corners are
 *	simply rotated and the min and max x,y coordinates are determined.
 *      The rectangle is centered at 0,0.  Point 0 is at -w/2, -h/2.
 *	Point 1 is at w/2, -h/2, etc.
 *
 *  		0 ------- 1
 *  		|         |
 *  		|    x    |
 *  		|         |
 *  		3 ------- 2
 *
 * Results:
 *	The width and height of the bounding box containing the
 *	rotated rectangle are returned.
 *
 * -----------------------------------------------------------------
 */
void
Blt_GetBoundingBox(width, height, theta, rotWPtr, rotHPtr, pointArr)
    int width, height;		/* Unrotated region */
    double theta;		/* Rotation of box */
    int *rotWPtr, *rotHPtr;	/* Rotated bounding box region */
    XPoint *pointArr;		/* Points of the rotated box */
{
    register int i;
    double sinTheta, cosTheta;
    double maxX, maxY;
    register double x, y;
    Coordinate corner[4];

    /*
     * Set the four corners of the rectangle whose center is the origin
     */
    corner[1].x = corner[2].x = (double)width * 0.5;
    corner[0].x = corner[3].x = -corner[1].x;
    corner[2].y = corner[3].y = (double)height * 0.5;
    corner[0].y = corner[1].y = -corner[2].y;

    theta = (-theta / 180.0) * M_PI;
    sinTheta = sin(theta), cosTheta = cos(theta);
    maxX = maxY = 0.0;

    /*
     * Rotate the four corners and find the maximum X and Y coordinates
     */
    for (i = 0; i < 4; i++) {
	x = (corner[i].x * cosTheta) - (corner[i].y * sinTheta);
	y = (corner[i].x * sinTheta) + (corner[i].y * cosTheta);
	if (x > maxX) {
	    maxX = x;
	}
	if (y > maxY) {
	    maxY = y;
	}
	if (pointArr != NULL) {
	    pointArr[i].x = BLT_RND(x);
	    pointArr[i].y = BLT_RND(y);
	}
    }

    /*
     * By symmetry, the width and height of the bounding box are
     * twice the maximum x and y coordinates.
     */
    *rotWPtr = (int)((maxX + maxX) + 0.5);
    *rotHPtr = (int)((maxY + maxY) + 0.5);
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_TranslateBoxCoords --
 *
 * 	Translate the coordinates of a given bounding box based
 *	upon the anchor specified.  The anchor indicates where
 *	the given xy position is in relation to the bounding box.
 *
 *  		nw --- n --- ne
 *  		|            |
 *  		w   center   e
 *  		|            |
 *  		sw --- s --- se
 *
 * 	The coordinates returned are translated to the origin of the
 * 	bounding box (suitable for giving to XCopyArea, etc.)
 *
 * Results:
 *	The translated coordinates of the bounding box are returned.
 *
 * -----------------------------------------------------------------
 */
Coordinate
Blt_TranslateBoxCoords(x, y, width, height, anchor)
    double x, y;		/* Window coordinates of anchor */
    int width, height;		/* Extents of the bounding box */
    Tk_Anchor anchor;		/* Direction of the anchor */
{
    Coordinate newPos;
    double w, h;

    w = (double)width, h = (double)height;
    newPos.x = x, newPos.y = y;
    switch (anchor) {
	case TK_ANCHOR_NW:		/* Upper left corner */
	    break;
	case TK_ANCHOR_W:		/* Left center */
	    newPos.y -= (h * 0.5);
	    break;
	case TK_ANCHOR_SW:		/* Lower left corner */
	    newPos.y -= h;
	    break;
	case TK_ANCHOR_N:		/* Top center */
	    newPos.x -= (w * 0.5);
	    break;
	case TK_ANCHOR_CENTER:		/* Centered */
	    newPos.x -= (w * 0.5);
	    newPos.y -= (h * 0.5);
	    break;
	case TK_ANCHOR_S:		/* Bottom center */
	    newPos.x -= (w * 0.5);
	    newPos.y -= h;
	    break;
	case TK_ANCHOR_NE:		/* Upper right corner */
	    newPos.x -= w;
	    break;
	case TK_ANCHOR_E:		/* Right center */
	    newPos.x -= w;
	    newPos.y -= (h * 0.5);
	    break;
	case TK_ANCHOR_SE:		/* Lower right corner */
	    newPos.x -= w;
	    newPos.y -= h;
	    break;
    }

    return (newPos);
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_RotateBitmap --
 *
 *	Creates a new bitmap containing the rotated image of the given
 *	bitmap.  We also need a special GC of depth 1, so that we do
 *	not need to rotate more than one plane of the bitmap.
 *
 * Results:
 *	Returns a new bitmap containing the rotated image.
 *
 * -----------------------------------------------------------------
 */
Pixmap
Blt_RotateBitmap(display, draw, bitmapGC, srcBitmap, srcWidth, srcHeight,
    theta, rotWPtr, rotHPtr)
    Display *display;		/* X display */
    Drawable draw;		/* Root window drawable */
    GC bitmapGC;		/* GC created from bitmap where fg=1,bg=0 */
    Pixmap srcBitmap;		/* Source bitmap to be rotated */
    int srcWidth, srcHeight;	/* Width and height of the source bitmap */
    double theta;		/* Right angle rotation to perform */
    int *rotWPtr, *rotHPtr;
{
    Pixmap destBitmap;
    int destWidth, destHeight;
    register int dx, dy;	/* Destination bitmap coordinates */
    register int sx, sy;	/* Source bitmap coordinates */
    unsigned long pixel;
#ifndef	__WIN32__
    XImage *src, *dest;
#else
#   undef XPutPixel
#   undef XGetPixel
#   define XGetPixel(P,X,Y) GetPixel(P, X, Y)
#   define XPutPixel(d,x,y,c) SetPixel(d,x,y,c)
    unsigned long fg = bitmapGC->foreground;
    unsigned long bg = bitmapGC->background;
    TkWinDCState DCs, DCd;
    HDC src, dest;
#endif /* !__WIN32__ */

    /*
     * Create a bitmap and image big enough to contain the rotated text
     */
    Blt_GetBoundingBox(srcWidth, srcHeight, theta, &destWidth, &destHeight,
	(XPoint *)NULL);
    destBitmap = Tk_GetPixmap(display, draw, destWidth, destHeight, 1);
    XSetForeground(display, bitmapGC, 0);
    XFillRectangle(display, destBitmap, bitmapGC, 0, 0, destWidth, destHeight);
#ifndef	__WIN32__
    src = XGetImage(display, srcBitmap, 0, 0, srcWidth, srcHeight, 1, ZPixmap);
    dest = XGetImage(display, destBitmap, 0, 0, destWidth, destHeight, 1,
	ZPixmap);
#else
    src = TkWinGetDrawableDC(display, srcBitmap, &DCs);
    dest = TkWinGetDrawableDC(display, destBitmap, &DCd);
#endif

    theta = BLT_FMOD(theta, 360.0);
    BLT_WDEBUG(("RotateBitmap: theta=%g gc=%08lxX fg=%08xX bg=%08xX\n",
	theta, bitmapGC, fg, bitmapGC->background);)
    if (BLT_FMOD(theta, (double)90.0) == 0.0) {
	int quadrant;

	/*
	 * Handle right-angle rotations specially
	 */
	quadrant = (int)(theta / 90.0);
	for (dx = 0; dx < destWidth; dx++) {
	    for (dy = 0; dy < destHeight; dy++) {
		switch (quadrant) {
		    case 3:			/* 270 degrees */
			sx = dy;
			sy = destWidth - dx - 1;
			break;
		    case 2:			/* 180 degrees */
			sx = destWidth - dx - 1;
			sy = destHeight - dy - 1;
			break;
		    case 1:			/* 90 degrees */
			sx = destHeight - dy - 1;
			sy = dx;
			break;
		    default:
		    case 0:			/* 0 degrees */
			sx = dx;
			sy = dy;
			break;
		}
		pixel = XGetPixel(src, sx, sy);
		if (pixel) {
		    XPutPixel(dest, dx, dy, pixel);
		}
	    }
	}
    } else {
	double radians, sinTheta, cosTheta;
	double srcX, srcY;	/* Center of source rectangle */
	double destX, destY;	/* Center of destination rectangle */
	double transX, transY;
	double rotX, rotY;

	radians = (theta / 180.0) * M_PI;
	sinTheta = sin(radians);
	cosTheta = cos(radians);

	/*
	 * Coordinates of the centers of the source and destination rectangles
	 */
	srcX = srcWidth * 0.5;
	srcY = srcHeight * 0.5;
	destX = destWidth * 0.5;
	destY = destHeight * 0.5;

	/*
	 * Rotate each pixel of dest image, placing results in source image
	 */
	for (dx = 0; dx < destWidth; dx++) {
	    for (dy = 0; dy < destHeight; dy++) {

		/* Translate origin to center of destination image */
		transX = dx - destX;
		transY = dy - destY;

		/* Rotate the coordinates about the origin */
		rotX = (transX * cosTheta) - (transY * sinTheta);
		rotY = (transX * sinTheta) + (transY * cosTheta);

		/* Translate back to the center of the source image */
		rotX += srcX;
		rotY += srcY;

		sx = BLT_RND(rotX);
		sy = BLT_RND(rotY);

		/*
		 * Verify the coordinates, since the destination image can be
		 * bigger than the source
		 */

		if (   (sx >= srcWidth) || (sx < 0)
		    || (sy >= srcHeight) || (sy < 0)) {
		    continue;
		}
		pixel = XGetPixel(src, sx, sy);
		if (pixel) {
		    XPutPixel(dest, dx, dy, pixel);
		}
	    }
	}
    }

#ifndef	__WIN32__
    /* Write the rotated image into the destination bitmap */
    XPutImage(display, destBitmap, bitmapGC, dest, 0, 0, 0, 0, destWidth,
	destHeight);

    /* Clean up temporary resources used */
    XDestroyImage(src);
    XDestroyImage(dest);
#else /* __WIN32__ */
#   undef XPutPixel
#   undef XGetPixel
    TkWinReleaseDrawableDC(srcBitmap, src, &DCs);
    TkWinReleaseDrawableDC(destBitmap, dest, &DCd);
#endif /* __WIN32__ */

    *rotWPtr = destWidth;
    *rotHPtr = destHeight;

    return destBitmap;
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_CreateTextBitmap --
 *
 *	Draw a bitmap, using the the given window coordinates
 *	as an anchor for the text bounding box.
 *
 * Results:
 *	Returns the bitmap representing the text string.
 *
 * Side Effects:
 *	Bitmap is drawn using the given font and GC in the
 *	drawable at the given coordinates, anchor, and rotation.
 *
 * -----------------------------------------------------------------
 */
Pixmap
Blt_CreateTextBitmap(tkwin, text, attrPtr, bmWidthPtr, bmHeightPtr)
    Tk_Window tkwin;
    char *text;			/* Text string to draw */
    TextAttributes *attrPtr;
    int *bmWidthPtr, *bmHeightPtr;
{
    int width, height;		/* Width and height of text bounding box */
    Pixmap bitmap;
    XGCValues gcValues;
    unsigned long gcMask;
    GC bitmapGC;
    Window root;
    Display *display;
#ifdef	__WIN32__
#   define BITMAPFGCOL PALETTERGB(255,255,255)
#   define BITMAPBGCOL PALETTERGB(0,0,0)
#   ifdef _WINNT
#	define TEXTMASKDEPTH Tk_Depth(tkwin)
#   else
#	define TEXTMASKDEPTH 1
#   endif
#else
#   define TEXTMASKDEPTH 1
#   define BITMAPFGCOL 1
#   define BITMAPBGCOL 0
#endif

    Blt_GetTextExtents(attrPtr->fontPtr, text, &width, &height);
    root = RootWindow(Tk_Display(tkwin), Tk_ScreenNumber(tkwin));
    display = Tk_Display(tkwin);
    BLT_WDEBUG(("Blt_CreateTextBitmap: w=%d h=%d theta=%gi text=%s\n",
	width, height, attrPtr->theta, text);)

    /*
     * Create a temporary bitmap and draw the text string into it.
     */
    attrPtr->regionWidth = width;
    width += PADDING(attrPtr->padX);
    height += PADDING(attrPtr->padY);
    bitmap = Tk_GetPixmap(display, root, width, height, TEXTMASKDEPTH);
    gcValues.font = attrPtr->fontPtr->fid;
	gcValues.foreground = gcValues.background = BITMAPBGCOL;

    gcMask = (GCFont | GCForeground | GCBackground);
    bitmapGC = XCreateGC(display, bitmap, gcMask, &gcValues);

    /*
     * Clear the pixmap and draw the text string into it
     */
    XFillRectangle(display, bitmap, bitmapGC, 0, 0, width, height);
    XSetForeground(display, bitmapGC, BITMAPFGCOL);
    BLT_WDEBUG(("bm gc/f/bg=%08lxX %08lxX %08lxX\ntxt gc/f/bg=%08lxX %08lxX "
	"%08lxX\n", bitmapGC, bitmapGC->foreground, bitmapGC->background,
	attrPtr->textGC, CONDPTR(attrPtr->textGC,foreground),
	CONDPTR(attrPtr->textGC,background));)
    DrawCompoundText(display, bitmap, bitmapGC, attrPtr, text,
	attrPtr->padLeft, attrPtr->padTop);
    if (attrPtr->theta != 0.0) {
	Pixmap rotBitmap;

	rotBitmap = Blt_RotateBitmap(display, root, bitmapGC, bitmap, width,
	    height, attrPtr->theta, bmWidthPtr, bmHeightPtr);
	Tk_FreePixmap(display, bitmap);
	bitmap = rotBitmap;
    } else {
	*bmWidthPtr = width, *bmHeightPtr = height;
    }
    XFreeGC(display, bitmapGC);

    return bitmap;
#   undef TEXTMASKDEPTH
#   undef BITMAPFGCOL
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_StencilBitmap --
 *
 *	Stencil (i.e. draw foreground only) a bitmap at the given
 *	window coordinates.  This routine should be used sparingly
 *	since it is very slow on most X servers.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Bitmap is stenciled at the given window coordinates.
 *
 *      The Stipple, FillStyle, and TSOrigin fields of the GC are
 *      modified.  This assumes the GC is private, *not* shared (via
 *      Tk_GetGC)
 *
 * -----------------------------------------------------------------
 */
void
Blt_StencilBitmap(display, draw, gc, bitmap, x, y, width, height)
    Display *display;
    Drawable draw;
    GC gc;			/* Private graphic context to use */
    Pixmap bitmap;		/* Bitmap to be displayed */
    int x, y;			/* x, y position of bitmap */
    int width, height;
{
    BLT_WDEBUG(("Stencil: x=%d y=%d w=%d h=%d\n",x, y, width, height);)
    XSetStipple(display, gc, bitmap);
    XSetTSOrigin(display, gc, x, y);
    XSetFillStyle(display, gc, FillStippled);
    XFillRectangle(display, draw, gc, x, y, width, height);
    XSetFillStyle(display, gc, FillSolid);
}

void
Blt_InitTextAttrs(attrPtr, fgPtr, bgPtr, fontPtr, theta, anchor, justify)
    TextAttributes *attrPtr;
    XColor *fgPtr, *bgPtr;
    TkFont *fontPtr;
    double theta;
    Tk_Anchor anchor;
    Tk_Justify justify;
{
    attrPtr->fgColorPtr = fgPtr;
    attrPtr->bgColorPtr = bgPtr;
    attrPtr->fontPtr = fontPtr;
    if (theta != 0.0) {
	theta = BLT_FMOD(theta, 360.0);
	if (theta < 0.0) {
	    theta += 360.0;
	}
    }
    attrPtr->theta = theta;
    attrPtr->anchor = anchor;
    attrPtr->justify = justify;
    /* Initialize these to zero */
    attrPtr->padLeft = attrPtr->padRight = 0;
    attrPtr->padTop = attrPtr->padBottom = 0;
    attrPtr->regionWidth = 0;
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_DrawText --
 *
 *	Draw a text string, possibly rotated,  using the the given
 *	window coordinates as an anchor for the text bounding box.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Text string is drawn using the given font and GC at the
 *	the given window coordinates.
 *
 *      The Stipple, FillStyle, and TSOrigin fields of the GC are
 *      modified for rotated text.  This assumes the GC is private,
 *      *not* shared (via Tk_GetGC)
 *
 * -----------------------------------------------------------------
 */
void
Blt_DrawText(tkwin, draw, text, attrPtr, x, y)
    Tk_Window tkwin;
    Drawable draw;
    char *text;
    TextAttributes *attrPtr;	/* Text attribute information */
    int x, y;			/* Window coordinates to draw text */
{
    Coordinate origin;
    double theta;
    Pixmap bitmap;
    Display *display;
    int width, height;
    int bmWidth, bmHeight;

    if ((text == NULL) || (*text == '\0')) {	/* Empty string, do nothing */
	return;
    }
    display = Tk_Display(tkwin);
    theta = BLT_FMOD(attrPtr->theta, (double)360.0);
    if (theta < 0.0) {
	theta += 360.0;
    }
    Blt_GetTextExtents(attrPtr->fontPtr, text, &width, &height);
    attrPtr->regionWidth = width;
    width += PADDING(attrPtr->padX);
    height += PADDING(attrPtr->padY);

    if (theta == 0.0) {
	/*
	 * This is the easy case of no rotation. Simply draw the text
	 * using the standard drawing routines.  If a background color
	 * was specified (both bgColorPtr and fillGC fields will be
	 * set), then fill a rectangle with the background color.
	 */
	origin = Blt_TranslateBoxCoords((double)x, (double)y, width, height, 
		attrPtr->anchor);
	x = BLT_RND(origin.x), y = BLT_RND(origin.y);
	if (attrPtr->bgColorPtr != NULL) {
	    BLT_WDEBUG(("bgptr=%08lxX fillGC=%08lxX fillBG=%08lxX\n",
		attrPtr->bgColorPtr, attrPtr->fillGC,
		attrPtr->fillGC ? attrPtr->fillGC->background : 0);)
	    XFillRectangle(display, draw, attrPtr->fillGC, x, y, width - 1, 
		height - 1);
	}
	/*
	 * attrPtr->width must be set to the width of the widest line
	 * before calling DrawCompoundText.
	 */
	DrawCompoundText(display, draw, attrPtr->textGC, attrPtr, text,
	    x + attrPtr->padLeft, y + attrPtr->padTop);
	return;
    }
    /*
     * Rotate the text by writing the text into a bitmap and rotating
     * the bitmap.  If the text is at a right angle and a background
     * color is set, we can use the faster XCopyPlane/Area operation.
     * Otherwise, we're left to draw a background polygon and/or
     * stipple the bitmap which can be slow.
     */
    attrPtr->theta = theta;
    bitmap = Blt_CreateTextBitmap(tkwin, text, attrPtr, &bmWidth, &bmHeight);
    /* attrPtr->width and attrPtr->height are the rotated bitmap dimensions */
    origin = Blt_TranslateBoxCoords((double)x, (double)y, bmWidth, bmHeight, 
	attrPtr->anchor);
    x = BLT_RND(origin.x), y = BLT_RND(origin.y);
    
    theta = BLT_FMOD(theta, (double)90.0);
    BLT_WDEBUG(("DrawText: xy=%d %d thetaf=%g\n",x,y,theta);)
    if ((attrPtr->bgColorPtr != NULL && theta == 0.0)) {
#ifdef	__WIN32__
	attrPtr->textGC->clip_mask = bitmap;
#endif
	/* Right angle, non-zero degree rotation */
	XCopyPlane(display, bitmap, draw, attrPtr->textGC, 0, 0, bmWidth,
	    bmHeight, x, y,1);
#ifdef	__WIN32__
	attrPtr->textGC->clip_mask = None;
#endif
    } else {
	if (attrPtr->bgColorPtr != NULL) {
	    XPoint pointArr[4];
	    register int i;
	    int centerX, centerY;
	    int dummy;
	    BLT_WDEBUG(("Rotate XFillPoly\n");)
	    /*
	     * If a background color was specified, calculate the
	     * bounding polygon and fill it with the background color.
	     */
	    /* Translate the anchor to the center of the bitmap */
	    origin.x += (bmWidth * 0.5);
	    origin.y += (bmHeight * 0.5);
	    centerX = BLT_RND(origin.x), centerY = BLT_RND(origin.y);
	    /* Determine the polygon bounding the rotated text region */
	    Blt_GetBoundingBox(width, height, attrPtr->theta, &dummy, &dummy,
		pointArr);

	    for (i = 0; i < 4; i++) {
		pointArr[i].x += centerX;
		pointArr[i].y += centerY;
	    }
	    XFillPolygon(display, draw, attrPtr->fillGC, pointArr, 4, Convex,
		CoordModeOrigin);
	}
	Blt_StencilBitmap(display, draw, attrPtr->textGC, bitmap, x, y, bmWidth,
		bmHeight);
    }
    Tk_FreePixmap(display, bitmap);
}

void
Blt_PrintJustified(interp, buffer, attrPtr, text, x, y, length)
    Tcl_Interp *interp;
    char *buffer;
    TextAttributes *attrPtr;
    char *text;
    int x, y;
    int length;
{
    int justifyX;
    int textLength;
    register int i;
    register int count;

    textLength = 0;
    justifyX = x + JustifyOffset(attrPtr, text, length, &textLength);
    count = 0;
    Tcl_AppendResult(interp, "(", (char *)NULL);
    for (i = 0; i < length; i++) {
	if (count > BUFSIZ) {
	    buffer[count] = '\0';
	    Tcl_AppendResult(interp, buffer, (char *)NULL);
	    count = 0;
	}
	/*
	 * On the off chance that special PostScript characters are
	 * embedded in the text string, escape the following
	 * characters: "\", "(", and ")"
	 */
	if ((*text == '\\') || (*text == '(') || (*text == ')')) {
	    buffer[count++] = '\\';
	}
	buffer[count++] = *text++;
    }
    buffer[count] = '\0';
    Tcl_AppendResult(interp, buffer, (char *)NULL);
    sprintf(buffer, ") %d %d %d DrawLine\n", textLength, justifyX, y);
    Tcl_AppendResult(interp, buffer, (char *)NULL);
}
