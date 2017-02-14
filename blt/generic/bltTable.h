/*
 * bltTable.h --
 *
 *	This file contains declarations for BLT's "table" geometry manager.
 *
 * Copyright (c) 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _BLTTABLE_H
#define _BLTTABLE_H

typedef struct Editor Editor;
typedef void (EditorDrawProc) _ANSI_ARGS_((Editor *editor));
typedef void (EditorDestroyProc) _ANSI_ARGS_((Editor *editor));

struct Editor {
    int gridLineWidth;
    int buttonHeight;
    int cubiclePad;
    int minSize;		/* Minimum size to allow any partition */

    EditorDrawProc	*drawProc;
    EditorDestroyProc *destroyProc;
};

#define numRows		rows.length
#define rowArr		rows.array
#define numCols		columns.length
#define colArr		columns.array

/*
 * Limits --
 *
 * 	Defines the bounding of a size (width or height) in the table.
 * 	It may be related to the partition, cubicle, or table size.  The
 * 	window pointers are used to associate * sizes with the requested
 * 	size of other windows.
 *
 */
typedef struct {
    int flags;			/* Flags indicate whether using default
				 * values for limits or not. See flags
				 * below. */
    int max, min;		/* Values for respective limits */
    int nom;			/* Nominal starting value */
    Tk_Window maxWin, minWin;	/* If non-NULL, represents windows whose
				 * requested sizes will be set as limits */
    Tk_Window nomWin;		/* If non-NULL represents window whose
				 * requested size will be the nominal size */
} Limits;

#define LIMITS_SET_BIT	1
#define LIMITS_SET_MIN  (LIMITS_SET_BIT<<0)
#define LIMITS_SET_MAX  (LIMITS_SET_BIT<<1)
#define LIMITS_SET_NOM  (LIMITS_SET_BIT<<2)

#define LIMITS_DEF_MIN	0	/* Default minimum limit  */
#define LIMITS_DEF_MAX	SHRT_MAX/* Default maximum limit */
#define LIMITS_DEF_NOM	-1000	/* Default nomimal value.  Indicates if a
				 * partition has received any space yet */

typedef int (LimitsProc) _ANSI_ARGS_((int value, Limits *limitsPtr));

/*
 * Resize --
 *
 * 	These flags indicate in what ways each partition in a table
 *	can be resized from its normal dimensions.  The normal size
 *	of a partition is the amount of space needed to contain the
 *	slave windows that span it.  The table may then be stretched
 *	or shrunk depending if the master window is larger or smaller
 *	than the table. This can occur if 1) the user resizes the
 *	toplevel window, or 2) the master window is in turn packed
 *	into a larger window and the "fill" option is set.
 *
 * 	  RESIZE_NONE 	  - No resizing from normal size.
 *	  RESIZE_EXPAND   - Do not allow the size to decrease.
 *			    The size may increase however.
 *        RESIZE_SHRINK   - Do not allow the size to increase.
 *			    The size may decrease however.
 *	  RESIZE_BOTH     - Allow the size to increase or
 *			    decrease from the normal size.
 *
 */
typedef enum {
    RESIZE_NONE, 
    RESIZE_EXPAND, 
    RESIZE_SHRINK, 
    RESIZE_BOTH
} Resize;

#define EXCLUDE_PAD 	0
#define INCLUDE_PAD	1

/*
 * Default values for slave window attributes.
 */
#define DEF_FILL	FILL_NONE
#define DEF_COLUMN_SPAN	1
#define DEF_ROW_SPAN	1
#define DEF_ANCHOR	TK_ANCHOR_CENTER


struct Table;

/*
 * Cubicle --
 *
 * 	A cubicle is the frame which contains a slave window and its
 *	padding. It may span multiple partitions and have requested
 *	limits which constrain the size of it.  Multiple cubicles can
 *	start at any location in the table, but a cubicle contains only
 *	one window.
 */

typedef struct {
    Tk_Window tkwin;		/* Slave window */
    struct Table *tablePtr;	/* Table managing this window */
    int extBW;			/* Last known border width of slave window */


    Limits reqWidth, reqHeight;	/* Configurable bounds for width and height
				 * requests made by the slave window */
    int rowSpan;		/* Number of rows spanned by slave */
    int rowIndex;		/* Starting row of span */
    int colSpan;		/* Number of columns spanned by slave */
    int colIndex;		/* Starting column of span */

    Tk_Anchor anchor;		/* Anchor type: indicates how the window is
				 * positioned if extra space is available in
				 * the cubicle */
    Pad padX;			/* Extra horizontal padding around the slave */
    Pad padY;			/* Extra vertical padding around the slave  */
    int ipadX, ipadY;		/* Extra padding inside of the slave window
				 * (in addition to the requested size of
				 * the window) */
    Fill fill;			/* Indicates how the slave window should fill
				 * the cubicle */
    int originX, originY;	/* Origin of the slave window wrt master
				 * window */
    /*
     * The following are the saved locations of the cubicle in each of the
     * three lists which are used by the table to track the cubicle.  The
     * item pointers are used to cleanup when the cubicle is no longer
     * needed.
     */
    Blt_ListItem *rowPtr;	/* Pointer into list of row spans.*/
    Blt_ListItem *colPtr;	/* Pointer into list of column spans. */
    Blt_ListItem *slavePtr;	/* Pointer into list of slave windows. */

} Cubicle;

/*
 * Partition --
 *
 * 	A partition creates a definable space (row or column) in the
 * 	table. It may have both requested minimum or maximum values which
 * 	constrain the size of it.
 *
 */
typedef struct {
    int index;			/* Index of row or column */

    int size;			/* Current size of the partition. This size
				 * is bounded by minSize and maxSize. */
    /* 
     * nomSize and size perform similar duties.  I need to keep track of 
     * the amount of space allocated to the partition (using size).  But 
     * at the same time, I need to indicate that space can be parcelled 
     * out to this partition.  If a nominal size was set for this 
     * partition, I don't want to add space.
     */

    int nomSize;		/* The nominal size (neither expanded nor
				 * shrunk) of the partition based upon the
				 * requested sizes of the slave windows
				 * spanning this partition. */

    int minSize, maxSize;	/* Size constraints on the partition */

    int offset;			/* Offset of the partition (in pixels) from
				 * the origin of the master window */

    int minSpan;		/* Minimum spanning window in partition. Used
				 * for bookkeeping when growing a span of
				 * partitions */

    Cubicle *control;		/* Pointer to the cubicle containing the slave
				 * window spanning this partition which
				 * controls its size.  This is used to
				 * know when a partition is used or not. */

    Resize resize;		/* Indicates if the partition should shrink
				 * or expand from its nominal size. */

    Pad pad;			/* Pads the partition beyond its nominal
				 * size */

    Limits reqSize;		/* Requested bounds for the size of the
				 * partition. The partition will not expand
				 * or shrink beyond these limits, regardless
				 * of how it was specified (max slave size).
				 * This includes any extra padding which may
				 * be specified. */

    int maxSpan;		/* Maximum spanning window to consider when
				 * growing a span of partitions. A value of
				 * zero indicates that all spans should be
				 * considered. */

} Partition;

#define DEF_TBL_RESIZE	"both"
#define DEF_TBL_PAD	"0"
#define DEF_TBL_MAXSPAN	"0"


/*
 * This is the default number of elements in the statically pre-allocated
 * column and row arrays.  This number should reflect a useful number of
 * row and columns, which fit most applications.
 */
#define DEF_ARRAY_SIZE	32

typedef Cubicle *(CubicleSearchProc) _ANSI_ARGS_((Tcl_Interp *interp,
	Tk_Window tkwin, int flags));

/*
 * RowCol --
 *
 * 	Manages an array of partitions.  The array of partitions represents
 *	either the rows or the columns in the table.
 *
 */
typedef struct {
    char *type;			/* String identifying the type of partition:
				 * "row" or "column". */
    int length;			/* Number of partitions currently used */
    int size;			/* Number of partitions allocated */

    Partition *array;		/* Pointer to array of partition information:
				 * Initially points to defSpace */

    /* Pre-allocated array of partitions. */

    Partition defSpace[DEF_ARRAY_SIZE];  

    Tk_ConfigSpec *configSpecs;

} RowCol;

/*
 * Table structure
 */
typedef struct Table {
    int flags;			/* See the flags definitions below. */
    Tk_Window searchWin;	/* Main window of window hierarchy */
    Tk_Window tkwin;		/* The master window in which the slave
				 * windows are arranged. */
    Tcl_Interp *interp;		/* Interpreter associated with all windows */

    Blt_List cubicles;		/* List of cubicles in the table. The
				 * key is the address of the slave tkwin */
    Blt_List rowSpans;		/* Linked list of bins of windows of
				 * increasing row span. */
    Blt_List colSpans;		/* Linked list of bins of windows of
				 * increasing column span. */
    Pad padX, padY;

    int propagate;		/* If non-zero, the table will make a
				 * geometry request on behalf of the
				 * master window. */

    int editorTablePad, editorCubiclePad, editorPartitionPad;

    RowCol columns, rows;	/* Manages row and column partitions */

    int masterWidth;		/* Last known width of the master window */
    int masterHeight;		/* Last known height of the master window */
    int normalWidth;		/* Normal width of the table */
    int normalHeight;		/* Normal height of the table */
    Limits reqWidth, reqHeight;	/* Constraints on the table's normal
				 * width and height */
    Editor *editPtr;		/* If non-NULL, indicates that the table
				 * is currently being edited */
    Tk_IdleProc *arrangeProc;
    CubicleSearchProc *findCubicleProc;

} Table;

/*
 * Table flags definitions
 */
#define ARRANGE_PENDING (1<<0)	/* A call to ArrangeTable is pending. This
				 * flag allows multiple layout changes to
				 * be requested before the table is
				 * actually reconfigured. */
#define REQUEST_LAYOUT 	(1<<1)	/* Get the requested sizes of the slave
				 * windows before expanding/shrinking the
				 * size of the master window.  It's
				 * necessary to recompute the layout every
				 * time a partition or cubicle is added,
				 * reconfigured, or deleted, but not when
				 * the master window is resized. */
#define NON_PARENT	(1<<2)	/* The table is managing slaves which are
				 * not children of the master window. This
				 * requires that they are moved when the
				 * master window is moved (a definite
				 * performance hit). */
/*
 * Forward declarations
 */

extern Table *Blt_FindTable _ANSI_ARGS_((Tcl_Interp *interp, char *pathName, 
	Tk_Window searchWin, int flags));

#endif /* _BLTTABLE_H */
