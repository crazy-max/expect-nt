/*
 * bltInt.h --
 *
 *	All sorts of internal declarations for the BLT extension to
 *	the Tk toolkit.
 *
 * Copyright (c) 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _BLTINT_H
#define _BLTINT_H

#ifdef	__WIN32__
#   include "bltWinConfig.h"
#   include "tclInt.h"
#   include "tkWinInt.h"
#   undef Pie
#else
#   include "bltConfig.h"
#   include "tclInt.h"
#   include "tkInt.h"
#endif

#define TCL80B2_OR_BETTER				\
    (   (TCL_MAJOR_VERSION > 8)				\
     || (   (TCL_MAJOR_VERSION == 8)			\
	 && (   (TCL_MINOR_VERSION > 0)			\
	     || (   (TCL_RELEASE_LEVEL > 1)		\
		 || (   (TCL_RELEASE_LEVEL == 1)	\
		     && (TCL_RELEASE_SERIAL >= 2))))))

#if !TCL80B2_OR_BETTER
#   error "This version of BLT requires Tcl/Tk 8.0b2 or better."
#endif

#include "tkFont.h"
#include <default.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif /* HAVE_MEMORY_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef	__WIN32__
#   ifndef	_MSC_VER
#	define _BC
#   endif
#   undef NEED_HYPOT
#   define NEED_XDRAWARRAYS
#   ifndef BLT_COMPATIBLE_LIBS
#	ifndef ckalloc
#	    undef ckfree
#	    define ckalloc Tcl_Ckalloc
#	    define ckfree Tcl_Ckfree
#	endif
#   endif
#   ifndef TCL_MEM_DEBUG
#	ifndef BLT_COMPATIBLE_LIBS
#	    undef ckrealloc
	    extern void *ckrealloc(void *,size_t);
#	endif
#   endif
#   if defined(BLT_COMPATIBLE_LIBS) && !defined(TCL_MEM_DEBUG) \
        && USE_TCLALLOC==0
#	define ckstrdup strdup
#	define ckcalloc calloc
#	undef ckfree
#	define ckfree free
#   else
	extern char *ckstrdup(char *);
	extern void *ckcalloc(size_t,size_t);
#   endif
    extern XImage *blt_XCreateImage(Display *, Visual *, unsigned, int, int,
        char *, unsigned, unsigned, int, int);
#   define XCreateImage blt_XCreateImage
#   undef BLT_WINTERP
#   ifdef _BLT_WDEBUG
#	define CONDPTR(P,F) ((P)?P->F:0)
#	define BLT_WINTERP
	extern void blt_debug(const char *, ...);
#	define BLT_WDEBUG(A) blt_debug A
#   else
#	define BLT_WDEBUG(A)		
#   endif
#   ifdef BLT_WINTERP
	extern Tcl_Interp *blt_winterp(void);
#   endif

#else /* !__WIN32__ */

#   ifdef HAVE_MALLOC_H
#	include <malloc.h>
#   endif /* HAVE_MALLOC_H */
#   undef ckfree
#   undef ckalloc
#   ifdef TCL_MEM_DEBUG
#	define ckfree Tcl_Ckfree
#	define ckalloc Tcl_Ckalloc
	extern char *ckstrdup(char *);
	extern void *ckcalloc(size_t,size_t);
#   else
#	define ckfree free
#	define ckalloc malloc
#	define ckcalloc calloc
#   endif
#   if !defined(TCL_MEM_DEBUG) && defined(HAVE_STRDUP)
#	define ckstrdup strdup
#   endif
#   define BLT_WDEBUG(A)
#endif /* !__WIN32__ */


#ifdef NEED_HYPOT
    extern double bltHypot(double, double);
#else
#   ifdef _MSC_VER
#	define bltHypot _hypot
#   else
#	define bltHypot hypot
#   endif
#endif

#ifdef NEED_XDRAWARRAYS
    void XDrawSegments(Display *, Drawable, GC, XSegment *, int);
#endif

#if USE_TCLALLOC
#   define BLT_ckfree Tcl_Ckfree
#else
#   define BLT_ckfree free
#endif

#ifdef HAVE_FLOAT_H
#   include <float.h>
#endif

#ifdef HAVE_LIMITS_H
#   include <limits.h>
#endif

#include "bltList.h"

#ifndef M_PI
#   define M_PI		3.14159265358979323846
#endif /* M_PI */

#ifndef M_SQRT2
#   define M_SQRT2	1.41421356237309504880
#endif /* M_SQRT2 */

#ifndef M_SQRT1_2
#   define M_SQRT1_2	0.70710678118654752440
#endif /* M_SQRT1_2 */

#ifndef SHRT_MAX
#   define SHRT_MAX	0x7FFF
#endif /* SHRT_MAX */

#ifndef USHRT_MAX
#   define USHRT_MAX	0xFFFF
#endif /* USHRT_MAX */

#if !defined(HAVE_FLOAT_H) || defined(_BC)
/*
 * ----------------------------------------------------------------------
 *
 * DBL_MIN, DBL_MAX --
 *
 * 	DBL_MAX and DBL_MIN are the largest and smaller double
 * 	precision numbers that can be represented by the floating
 * 	point hardware. If the compiler is ANSI, they can be found in
 * 	float.h.  Otherwise, we use HUGE_VAL or HUGE to determine
 * 	them.
 *
 * ---------------------------------------------------------------------- */
/*
 * Don't want to include __infinity (definition of HUGE_VAL (SC1.x))
 */
#   if defined(sun)
#	define DBL_MAX		1.7976931348623157E+308
#	define DBL_MIN		2.2250738585072014E-308
#	define DBL_EPSILON	2.2204460492503131e-16
#   elseif defined(_BC)	
#	undef  DBL_MAX
#	define DBL_MAX		0.85E+154
#   else
#	ifndef DBL_EPSILON
#	    define DBL_EPSILON	BLT_DBL_EPSILON
#	endif
#	ifdef HUGE_VAL
#	    define DBL_MAX	HUGE_VAL
#	    define DBL_MIN	(1/HUGE_VAL)
#	else
#	    ifdef HUGE
#		define DBL_MAX	HUGE
#		define DBL_MIN	(1/HUGE)
#	    else
		/* Punt: Assume values simple and relatively small */
#		define DBL_MAX	3.40282347E+38
#		define DBL_MIN	1.17549435E-38
#	    endif /*HUGE*/
#	endif /*HUGE_VAL*/
#   endif /* !(sun || _BC) */
#endif /* !HAVE_FLOAT_H || _BC */

#ifdef __GNUC__
#   define INLINE __inline__
#else
#   define INLINE
#endif

#undef BLT_MIN
#define BLT_MIN(a,b)	(((a)<(b))?(a):(b))

#undef BLT_MAX
#define BLT_MAX(a,b)	(((a)>(b))?(a):(b))

/*
 * ----------------------------------------------------------------------
 *
 *  	The following are macros replacing math library functions:
 *  	"fabs", "fmod", "abs", "rint", and "exp10".
 *
 *  	Although many of these routines may be in your math library,
 *  	they aren't used in libtcl.a or libtk.a.  This makes it
 *  	difficult to dynamically load the BLT library as a shared
 *  	object unless the math library is also shared (which isn't
 *  	true on several systems).  We can avoid the problem by
 *  	replacing the "exotic" math routines with macros.
 *
 * ----------------------------------------------------------------------
 */
#undef BLT_ABS
#define BLT_ABS(x) 	(((x)<0)?(-(x)):(x))

#undef BLT_EXP10
#define BLT_EXP10(x)	(pow(10.0,(x)))

#undef BLT_FABS
#define BLT_FABS(x) 	(((x)<0.0)?(-(x)):(x))

#undef BLT_FMOD
#define BLT_FMOD(x,y) 	((x)-(((int)((x)/(y)))*y))

#undef BLT_RND
#define BLT_RND(x) 	((int)((x) + (((x)<0.0) ? -0.5 : 0.5)))

#define TRUE 	1
#define FALSE 	0

#ifdef hpux
#   define BLACK		"black"
#   define WHITE		"white"
#   define BISQUE1		"bisque1"
#   define BISQUE2		"bisque2"
#   define BISQUE3		"bisque3"
#   define ANTIQUEWHITE1	"antiquewhite1"
#   define GRAY			"grey"
#   define LIGHTBLUE2		"lightblue2"
#   define RED			"red"
#   define PINK			"pink"
#   define BLUE			"blue"
#   define NAVYBLUE		"navyblue"
#   define GRAY85		"gray85"
#   define GRAY92		"gray92"
#   define GRAY77		"gray92"
#   define GRAY64		"gray64"
#   define MAROON		"maroon"
#else /* !hpux */
#   ifndef __WIN32__
#	ifndef BLACK
#	    define BLACK	"#000000"
#	endif
#	ifndef WHITE
#	    define WHITE	"#ffffff"
#	endif
#	define BISQUE1		"#ffe4c4"
#	define BISQUE2		"#eed5b7"
#	define BISQUE3     	"#cdb79e"
#	define ANTIQUEWHITE1	"#ffefdb"
#	define GRAY		"#b0b0b0"
#	define LIGHTBLUE2	"#b2dfee"
#	define GRAY85		"#d9d9d9"
#	define GRAY92		"#ececec"
#	define GRAY77		"#c3c3c3"
#	define MAROON		"#b03060"
#	define BLUE		"#0000ff"
#	define GREEN		"#00ff00"
#   endif /* !__WIN32 */
#   define RED			"#ff0000"
#   define GRAY64		"#a3a3a3"
#endif /* !hpux */

#if defined(__WIN32__) || !defined(INDICATOR)
#   define NORMAL_BG		"SystemButtonFace"
#   ifdef ACTIVE_BG
#	undef ACTIVE_BG
#   endif
#   define ACTIVE_BG		"SystemButtonHighlight"
#   ifdef SELECT_BG
#	undef SELECT_BG
#   endif
#   define SELECT_BG		"SystemHighlight"
#   ifdef SELECT_FG
#	undef SELECT_FG
#   endif
#   define SELECT_FG		"SystemHighlightText"
#   ifdef TROUGH
#	undef TROUGH
#   endif
#   define TROUGH		"SystemScrollbar"
#   ifdef INDICATOR
#	undef INDICATOR
#   endif
#   define INDICATOR		"#b03060"
#   ifdef DISABLED
#	undef DISABLED
#   endif
#   define DISABLED		"SystemDisabledText"
#endif

#define STD_NORMAL_BG_COLOR	NORMAL_BG
#define STD_ACTIVE_BG_COLOR	ACTIVE_BG
#define STD_SELECT_BG_COLOR	SELECT_BG
#define STD_DISABLE_FG_COLOR	DISABLED
#define STD_INDICATOR_COLOR	MAROON
#define STD_NORMAL_FG_COLOR	BLACK
#define STD_NORMAL_BG_MONO	WHITE
#define STD_NORMAL_FG_MONO	BLACK
#define STD_ACTIVE_BG_MONO	BLACK
#define STD_ACTIVE_FG_COLOR	BLACK
#define STD_ACTIVE_FG_MONO	WHITE
#define STD_SELECT_FG_COLOR	BLACK
#define STD_SELECT_FG_MONO	WHITE
#define STD_SELECT_BG_MONO	BLACK
#define STD_SELECT_BORDERWIDTH	"2"
#define STD_BORDERWIDTH 	"2"
#define STD_FONT_HUGE		"Helvetica 18 bold"
#define STD_FONT_LARGE		"Helvetica 14 bold"
#define STD_FONT		"Helvetica 12 bold"
#define STD_FONT_SMALL		"Helvetica 10 bold"

#define TEXTHEIGHT(f)		((f)->fm.ascent + (f)->fm.descent)
#define LINEWIDTH(w)		(((w) > 1) ? (w) : 0)

#define NO_FLAGS		0
#define NO_TED			1

#undef VARARGS
#ifdef __cplusplus
#   define ANYARGS		(...)
#   define VARARGS(first)	(first, ...)
#else
#   define ANYARGS		()
#   define VARARGS(first)	()
#endif

#define Panic(mesg)		panic("%s:%d %s", __FILE__, __LINE__, (mesg));

/*
 * ----------------------------------------------------------------------
 *
 * Blt_CmdSpec --
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    char *name;			/* Name of command */
    char *version;		/* Version string of command */
    Tcl_CmdProc *cmdProc;
    Tcl_CmdDeleteProc *cmdDeleteProc;
    ClientData clientData;
} Blt_CmdSpec;

/*
 * ----------------------------------------------------------------------
 *
 * Blt_OperProc --
 *
 * 	Generic function prototype of CmdOptions.
 *
 * ----------------------------------------------------------------------
 */
typedef int (*Blt_OperProc) _ANSI_ARGS_(ANYARGS);

/*
 * ----------------------------------------------------------------------
 *
 * Blt_OperSpec --
 *
 * 	Structure to specify a CmdOption for a Tcl command.  This is 
 *	passed in the Blt_LookupOperation procedure to search for a 
 *	Blt_OperProc procedure.
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    char *name;			/* Name of minor command */
    int minChars;		/* Minimum # characters to disambiguate */
    Blt_OperProc proc;
    int minArgs;		/* Minimum # args required */
    int maxArgs;		/* Maximum # args required */
    char *usage;		/* Usage message */

} Blt_OperSpec;

typedef enum {
    BLT_OPER_ARG0,		/* Operation name is the first argument */
    BLT_OPER_ARG1,		/* Operation name is the second argument */
    BLT_OPER_ARG2,		/* Operation name is the third argument */
    BLT_OPER_ARG3,		/* Operation name is the fourth argument */
    BLT_OPER_ARG4,		/* Operation name is the fifth argument */

} Blt_OperIndex;

extern Blt_OperProc Blt_LookupOperation _ANSI_ARGS_((Tcl_Interp *interp,
	int numSpecs, Blt_OperSpec *specArr, Blt_OperIndex argIndex, 
	int numArgs, char **argArr));

/*
 * ----------------------------------------------------------------------
 *
 * Pad --
 *
 * 	Structure to specify vertical and horizontal padding.
 *	This allows padding to be specified on a per side basis.
 *	Vertically, side1 and side2 refer to top and bottom sides.
 *	Horizontally, they refer to left and right sides.
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    int side1, side2;
} Pad;

#define padLeft  	padX.side1
#define padRight  	padX.side2
#define padTop		padY.side1
#define padBottom	padY.side2
#define PADDING(x)	((x).side1 + (x).side2)

/*
 * ----------------------------------------------------------------------
 *
 * The following enumerated values are used as bit flags.
 *
 *
 * ----------------------------------------------------------------------
 */
typedef enum {
    FILL_NONE,			/* No filling is specified */
    FILL_X,			/* Fill horizontally */
    FILL_Y,			/* Fill vertically */
    FILL_BOTH			/* Fill both vertically and horizontally */
} Fill;

/*
 * ----------------------------------------------------------------------
 *
 * Dashes --
 *
 * 	List of dash values (maximum 11 based upon PostScript limit).
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    unsigned char valueList[12];
    int numValues;
} Dashes;

/*
 * ----------------------------------------------------------------------
 *
 * TextAttributes --
 *
 * 	Represents a convenient structure to hold text attributes
 *	which determine how a text string is to be displayed on the
 *	window, or drawn with PostScript commands.  The alternative
 *	is to pass lots of parameters to the drawing and printing
 *	routines. This seems like a more efficient and less cumbersome
 *	way of passing parameters.
 *
 * ----------------------------------------------------------------------
 */
typedef struct {
    XColor *bgColorPtr;		/* Text background color */
    XColor *fgColorPtr;		/* Text foreground color */
    double theta;		/* Rotation of text in degrees. */
    TkFont *fontPtr;		/* Font to use to draw text */
    Tk_Justify justify;		/* Justification of the text string. This
				 * only matters if the text is composed
				 * of multiple lines. */
    Tk_Anchor anchor;		/* Indicates how the text is anchored around
				 * its x and y coordinates. */
    GC textGC;			/* GC used to draw the text */
    GC fillGC;			/* GC used to fill the background of the
				 * text. bgColorPtr must be set too. */
    int regionWidth;		/* Width of the region where the text will
				 * be drawn. Used for justification. */
    Pad padX, padY;		/* Specifies padding of around text region */
} TextAttributes;

/*
 * -------------------------------------------------------------------
 *
 * Coordinate --
 *
 *	Represents a single coordinate.
 *
 * -------------------------------------------------------------------
 */
typedef struct {
    double x, y;
} Coordinate;


typedef enum {
    BLT_VECTOR_NOTIFY_UPDATE=1, /* The vector's values has been updated */
    BLT_VECTOR_NOTIFY_DESTROY=2	/* The vector has been destroyed and the client
				 * should no longer use its data (calling 
				 * Blt_FreeVectorId) */
} Blt_VectorNotify;

typedef void (Blt_VectorChangedProc) _ANSI_ARGS_((Tcl_Interp *interp,
	ClientData clientData, Blt_VectorNotify notify));

typedef struct Blt_VectorId *Blt_VectorId;
typedef struct {
    double *valueArr;		/* Array of values (possibly malloc-ed) */
    int numValues;		/* Number of values in the array */
    int arraySize;		/* Size of the allocated space */
    double min, max;		/* Minimum and maximum values in the vector */
    int reserved;		/* Reserved for future use */
} Blt_Vector;

extern Blt_VectorId Blt_AllocVectorId _ANSI_ARGS_((Tcl_Interp *interp, 
	char *vecName));
extern void Blt_SetVectorChangedProc _ANSI_ARGS_((Blt_VectorId clientId,
	Blt_VectorChangedProc * proc, ClientData clientData));
extern void Blt_FreeVectorId _ANSI_ARGS_((Blt_VectorId clientId));
extern int Blt_GetVectorById _ANSI_ARGS_((Tcl_Interp *interp,
	Blt_VectorId clientId, Blt_Vector *vecPtr));
extern char *Blt_NameOfVectorId _ANSI_ARGS_((Blt_VectorId clientId));
extern int Blt_VectorNotifyPending _ANSI_ARGS_((Blt_VectorId clientId));

extern int Blt_CreateVector _ANSI_ARGS_((Tcl_Interp *interp, char *vecName,
	int size, Blt_Vector *vecPtr));
extern int Blt_GetVector _ANSI_ARGS_((Tcl_Interp *interp, char *vecName,
	Blt_Vector *vecPtr));
extern int Blt_VectorExists _ANSI_ARGS_((Tcl_Interp *interp, char *vecName));
extern int Blt_ResetVector _ANSI_ARGS_((Tcl_Interp *interp,  char *vecName,
	Blt_Vector *vecPtr, Tcl_FreeProc *freeProc));
extern int Blt_ResizeVector _ANSI_ARGS_((Tcl_Interp *interp, char *vecName,
	int newSize));
extern int Blt_DeleteVector _ANSI_ARGS_((Tcl_Interp *interp, char *vecName));


/*
 * ----------------------------------------------------------------------
 *
 * 	X11/Xosdefs.h requires XNOSTDHDRS be set for some systems.
 *	This is a guess.  If I can't find STDC headers or unistd.h,
 *	assume that this is non-POSIX and non-STDC environment.
 *	(needed for Encore Umax 3.4 ?)
 *
 * ----------------------------------------------------------------------
 */
#if !defined(STDC_HEADERS) && !defined(HAVE_UNISTD_H)
#   define XNOSTDHDRS 	1
#endif

/*
 * ----------------------------------------------------------------------
 *
 *	Assume we need to declare free if there's no stdlib.h or malloc.h
 *
 * ----------------------------------------------------------------------
 */
#if !defined(HAVE_STDLIB_H) && !defined(HAVE_MALLOC_H)
extern void free _ANSI_ARGS_((void *));
#endif

/*
 * ----------------------------------------------------------------------
 *
 *	If strerror isn't in the C library (we'll get it from
 *	libtcl.a) assume that we also need a forward declaration.
 *
 * ----------------------------------------------------------------------
 */
#ifndef HAVE_STRERROR
extern char *strerror _ANSI_ARGS_((int));
#endif

/*
 * ----------------------------------------------------------------------
 *
 *	Make sure we have a declaration for strdup and strcasecmp
 *
 * ----------------------------------------------------------------------
 */
#ifdef NO_DECL_STRDUP
extern char *strdup _ANSI_ARGS_((CONST char *s));
#endif

#ifdef NO_DECL_STRCASECMP
extern int strcasecmp _ANSI_ARGS_((CONST char *s1, CONST char *s2));
#endif

extern GC Blt_GetUnsharedGC _ANSI_ARGS_((Tk_Window tkwin, unsigned long gcMask,
	XGCValues *valuePtr));

extern int Blt_GetLength _ANSI_ARGS_((Tcl_Interp *interp, Tk_Window tkwin,
	char *string, int *valuePtr));

extern char *Blt_NameOfFill _ANSI_ARGS_((Fill fill));

extern int Blt_GetXYPosition _ANSI_ARGS_((Tcl_Interp *interp, char *string,
	XPoint *pointPtr));

extern void Blt_AppendDouble _ANSI_ARGS_((Tcl_Interp *interp, double value));
extern void Blt_AppendInt _ANSI_ARGS_((Tcl_Interp *interp, int value));

extern int Blt_InitCmd _ANSI_ARGS_((Tcl_Interp *interp, Blt_CmdSpec *specPtr));
extern int Blt_InitCmds _ANSI_ARGS_((Tcl_Interp *interp, Blt_CmdSpec *specPtr,
	int numCmds));
extern int Blt_ConfigModified _ANSI_ARGS_(VARARGS(Tk_ConfigSpec *specs));

extern void Blt_MakeInputOnlyWindowExist _ANSI_ARGS_((Tk_Window tkwin));

extern void Blt_GetBoundingBox _ANSI_ARGS_((int width, int height,
	double theta, int *widthPtr, int *heightPtr, XPoint *pointArr));

extern void Blt_InitTextAttrs _ANSI_ARGS_((TextAttributes *attrPtr,
	XColor *fgColorPtr, XColor *bgColorPtr, TkFont *fontPtr,
	double theta, Tk_Anchor anchor, Tk_Justify justify));

extern void Blt_DrawText _ANSI_ARGS_((Tk_Window tkwin, Drawable draw,
	char *text, TextAttributes *attrPtr, int x, int y));

extern void Blt_PrintJustified _ANSI_ARGS_((Tcl_Interp *interp, char *buffer,
	TextAttributes *attrPtr, char *text, int x, int y, int length));

extern Pixmap Blt_CreateTextBitmap _ANSI_ARGS_((Tk_Window tkwin, char *textStr,
	TextAttributes *attrPtr, int *bmWidthPtr, int *bmHeightPtr));

extern Pixmap Blt_RotateBitmap _ANSI_ARGS_((Display *display, Drawable draw,
	GC gc, Pixmap bitmap, int width, int height, double theta,
	int *rotWPtr, int *rotHPtr));

extern Coordinate Blt_TranslateBoxCoords _ANSI_ARGS_((double x, double y, 
	int width, int height, Tk_Anchor anchor));

typedef int *Blt_Tile;		/* Opaque type for tiles */

typedef void (Blt_TileChangedProc) _ANSI_ARGS_((ClientData clientData,
	Blt_Tile tile, GC *gcPtr));

extern Blt_Tile Blt_GetTile _ANSI_ARGS_((Tcl_Interp *interp, Tk_Window tkwin,
	char *imageName));
extern void Blt_FreeTile _ANSI_ARGS_((Blt_Tile tile));
extern char *Blt_NameOfTile _ANSI_ARGS_((Blt_Tile tile));
extern void Blt_SetTileChangedProc _ANSI_ARGS_((Blt_Tile tile,
	Blt_TileChangedProc * changeProc, ClientData clientData, GC *gcPtr));
extern Pixmap Blt_PixmapOfTile _ANSI_ARGS_((Blt_Tile tile));
extern void Blt_SizeOfTile _ANSI_ARGS_((Blt_Tile tile, int *widthPtr,
	int *heightPtr));
extern void Blt_SetTileOrigin _ANSI_ARGS_((Tk_Window tkwin, GC gc, int x,
	int y));

typedef char *FreeProcData;

extern void panic _ANSI_ARGS_(VARARGS(char *fmt));

/* 
 * [incr tcl] 2.0 Namespaces
 *
 * BLT will automatically use namespaces if you compile with with the
 * itcl-2.x versions of Tcl and Tk headers and libraries.  BLT will
 * reside in its own namespace called "blt". All the normal BLT
 * commands and variables will reside there.
 * 
 * If you have [incr Tcl] 2.x, try to use it instead of the vanilla
 * Tcl and Tk headers and libraries.  Itcl namespaces are the reason
 * why the prefix "blt_" is no longer in front of every command.
 * There's no reason to "uglify" code when the right solution is
 * available.  Nuff said.
 *
 * But if you insist, you can disable the automatic selection of itcl
 * by uncommenting the following #undef statement.
 *
 * mike: Since Tcl 8.0b1 introduced a namespace mechanism based on
 * Michael McLennan's [incr Tcl] implementation, I changed the macro
 * below from ITCL_NAMESPACES to USE_TCL_NAMESPACES, and introduced
 * another macro USE_TK_NAMESPACES, which should only be defined if
 * Tcl's namespaces are extended to also cover widgets (this is not
 * the case currently, but it is required for building MegaWidgets,
 * so chances are that Tcl will be aware of widget namespaces someday).
 */

#define USE_TCL_NAMESPACES
#undef USE_TK_NAMESPACES

#endif /* !_BLTINT_H*/
