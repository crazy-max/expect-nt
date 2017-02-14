/*
 * bltGrElem.h --
 *
 *	Declarations of vectors and graph elements for BLT, an extension
 *	to the Tk toolkit.
 *
 * Copyright (c) 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _BLTELEMENT_H
#define _BLTELEMENT_H

/*
 * Symbol types for line elements
 */
typedef enum {
    SYMBOL_NONE, SYMBOL_SQUARE, SYMBOL_CIRCLE, SYMBOL_DIAMOND,
    SYMBOL_PLUS, SYMBOL_CROSS, SYMBOL_SPLUS, SYMBOL_SCROSS, SYMBOL_TRIANGLE
} SymbolType;

/*
 * The data structure below contains information pertaining to a line
 * vector.  It consists of an array of floating point data values and
 * for convenience, the number and minimum/maximum values.
 */
typedef struct Vector2 {
    double *data;		/* Array of values */
    int length;			/* Number of entries in the array */
    double min, max;		/* Smallest/largest values in the array */
    Blt_VectorId clientId;	/* If non-NULL, a client token identifying
				 * the external vector. */
    Element *elemPtr;		/* Element vector is associated with */
} Vector2;

typedef struct Vector {
    Blt_Vector vector;
    Blt_VectorId clientId;	/* If non-NULL, a client token identifying
				 * the external vector. */
    Element *elemPtr;		/* Element vector is associated with */
} Vector;

#define ELEMENT_LENGTH(elemPtr) \
	BLT_MIN((elemPtr)->x.vector.numValues, (elemPtr)->y.vector.numValues)

/*
 * An element has one or more vectors plus several attributes,
 * such as line style, thickness, color, and symbol type.  It
 * has an identifier which distinguishes it among the list of all
 * elements.
 */
typedef struct ClosestInfo ClosestInfo;

typedef void (ElemDrawProc) _ANSI_ARGS_((Graph *graphPtr, Element *elemPtr));
typedef void (ElemPrintProc) _ANSI_ARGS_((Graph *graphPtr, Element *elemPtr));
typedef void (ElemDestroyProc) _ANSI_ARGS_((Graph *graphPtr, Element *elemPtr));
typedef int (ElemConfigProc) _ANSI_ARGS_((Graph *graphPtr, Element *elemPtr));
typedef void (ElemCoordsProc) _ANSI_ARGS_((Graph *graphPtr, Element *elemPtr));
typedef int (ElemLimitsProc) _ANSI_ARGS_((Graph *graphPtr, Element *elemPtr,
	GraphAxis *axisPtr, double *minPtr, double *maxPtr));
typedef void (ElemClosestProc) _ANSI_ARGS_((Graph *graphPtr, Element *elemPtr,
	ClosestInfo *infoPtr));
typedef void (ElemDrawSymbolsProc) _ANSI_ARGS_((Graph *graphPtr,
	Element *elemPtr, int symbolSize, XPoint *pointArr, int numPoints,
	int active));
typedef void (ElemPrintSymbolsProc) _ANSI_ARGS_((Graph *graphPtr,
	Element *elemPtr, int symbolSize, XPoint *pointArr, int numPoints,
	int active));

struct Element {
    Graph *graphPtr;		/* Graph widget of element*/
    ElementType type;		/* Type of element; either BAR_ELEMENT or
				 * LINE_ELEMENT */
    unsigned int flags;		/* Indicates if the entire element is active,
				 * or if coordinates need to be calculated */
    Tk_Uid nameId;		/* Identifier to refer the element. Used in
				 * the "insert", "delete", or "show",
				 * commands. */
    int mapped;			/* If true, element is currently visible. */
    Tk_ConfigSpec *configSpecs;	/* Configuration specifications */
    char *label;		/* Label displayed in legend */
    unsigned int axisMask;	/* Indicates which axes to map element's
				 * coordinates onto */
    Vector x, y;		/* Contains array of floating point graph
				 * coordinate values. Also holds min/max and
				 * the number of coordinates */
    int *activeIndexArr;	/* Array of indices (malloc-ed) which
				 * indicate which data points are active
				 * (drawn with "active" colors). */
    int numActiveIndices;	/* Number of active data points. Special
				 * case: if numActivePoints==0 and the active
				 * bit is set in "flags", then all data
				 * points are drawn active. */

    ElemConfigProc *configProc;
    ElemDestroyProc *destroyProc;
    ElemDrawProc *drawNormalProc;
    ElemDrawProc *drawActiveProc;
    ElemLimitsProc *limitsProc;
    ElemClosestProc *closestProc;
    ElemCoordsProc *coordsProc;
    ElemPrintProc *printNormalProc;
    ElemPrintProc *printActiveProc;
    ElemDrawSymbolsProc *drawSymbolsProc;
    ElemPrintSymbolsProc *printSymbolsProc;
};

#define	COORDS_NEEDED 	(1<<0)	/* Indicates that the element's
				 * configuration has changed such that
				 * its layout of the element (i.e. its
				 * position in the graph window) needs
				 * to be recalculated. */

#define	ELEM_ACTIVE	(1<<8)	/* Non-zero indicates that the element
				 * should be drawn in its active
				 * foreground and background
				 * colors. */

#define	LABEL_ACTIVE 	(1<<9)	/* Non-zero indicates that the
				 * element's entry in the legend
				 * should be drawn in its active
				 * foreground and background
				 * colors. */
struct ClosestInfo {
    int halo;			/* Maximal distance a candidate point can
				 * be from the sample window coordinate */
    int interpolate;		/* If non-zero, find the closest point on the
				 * entire line segment, instead of the closest
				 * end point. */
    int x, y;			/* Screen coordinates of sample point */

    double dist;		/* Distance from the screen coordinates */
    Tk_Uid elemId;		/* Id of closest element */
    Coordinate closest;		/* Graph coordinates of closest point */
    int index;			/* Index of closest data point */
};

extern double Blt_FindVectorMinimum _ANSI_ARGS_((Vector *vecPtr,
	double minLimit));
extern void Blt_ResizeStatusArray _ANSI_ARGS_((Element *elemPtr,
	int numPoints));

#endif /* !_BLTELEMENT_H */
