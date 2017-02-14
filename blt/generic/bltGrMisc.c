/*
 * bltGrMisc.c --
 *
 *	This module implements a graph widget for the
 *	Tk toolkit.
 *
 * Copyright 1991-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef NO_GRAPH

#include "bltGraph.h"

static int PositionParseProc _ANSI_ARGS_((ClientData, Tcl_Interp *, Tk_Window,
	char *, char *, int));
static char *PositionPrintProc _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltPositionOption =
{
    PositionParseProc, PositionPrintProc, (ClientData)0
};

static int MapAxisParseProc _ANSI_ARGS_((ClientData, Tcl_Interp *, Tk_Window,
	char *, char *, int));
static char *MapAxisPrintProc _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltMapXAxisOption =
{
    MapAxisParseProc, MapAxisPrintProc, (ClientData)AXIS_MASK_X
};

Tk_CustomOption bltMapYAxisOption =
{
    MapAxisParseProc, MapAxisPrintProc, (ClientData)AXIS_MASK_Y
};

static INLINE double
Fabs(x)
    register double x;
{
    return ((x < 0.0) ? -x : x);
}

/* ----------------------------------------------------------------------
 * Custom option parse and print procedures
 * ----------------------------------------------------------------------
 */

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetXYPosition --
 *
 *	Converts a string in the form "@x,y" into an XPoint structure
 *	of the x and y coordinates.
 *
 * Results:
 *	A standard Tcl result. If the string represents a valid position
 *	*pointPtr* will contain the converted x and y coordinates and
 *	TCL_OK is returned.  Otherwise,	TCL_ERROR is returned and
 *	interp->result will contain an error message.
 *
 *----------------------------------------------------------------------
 */
int
Blt_GetXYPosition(interp, string, pointPtr)
    Tcl_Interp *interp;
    char *string;
    XPoint *pointPtr;
{
    char *comma, *tmpString;
    int result;
    int x, y;

    if ((string == NULL) || (*string == '\0')) {
	pointPtr->x = pointPtr->y = -SHRT_MAX;
	return TCL_OK;
    }
    if (*string != '@') {
	goto badFormat;
    }
    string++;
    /* mike: make a copy of string, so that we can safely change it. */
    tmpString = ckstrdup(string);
    comma = strchr(tmpString, ',');
    if (comma == NULL) {
	ckfree(tmpString);
	goto badFormat;
    }
    *comma = '\0';
    result = ((Tcl_GetInt(interp, tmpString, &x) == TCL_OK) &&
	(Tcl_GetInt(interp, comma + 1, &y) == TCL_OK));
    *comma = ',';
    ckfree(tmpString);
    if (!result) {
	return TCL_ERROR;
    }
    pointPtr->x = x, pointPtr->y = y;
    return TCL_OK;

  badFormat:
    Tcl_AppendResult(interp, "bad position \"", string,
	"\": should be formatted as \"@x,y\"", (char *)NULL);
    return TCL_ERROR;
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
    XPoint *pointPtr = (XPoint *)(widgRec + offset);

    return (Blt_GetXYPosition(interp, value, pointPtr));
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
    char *result;
    XPoint *pointPtr = (XPoint *)(widgRec + offset);

    result = "";
    if ((pointPtr->x != -SHRT_MAX) && (pointPtr->y != -SHRT_MAX)) {
	char string[200];

	sprintf(string, "@%d,%d", pointPtr->x, pointPtr->y);
	result = ckstrdup(string);
	if (result == NULL) {
	    Panic("can't allocate string for position");
	}
	*freeProcPtr = (Tcl_FreeProc *)BLT_ckfree;
    }
    return (result);
}

/*
 *----------------------------------------------------------------------
 *
 * MapAxisParseProc --
 *
 *	Convert the string indicating which axis flags to use.
 *
 * Results:
 *	The return value is a standard Tcl result.  The axis flags are
 *	written into the widget record.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MapAxisParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* Axis type */
    char *widgRec;		/* Widget record */
    int offset;			/* offset of AxisType field */
{
    int *flagsPtr = (int *)(widgRec + offset);
    int mask = (int)clientData;
    int flags;

    flags = 0;
    if (mask == AXIS_MASK_X) {
	if (strcmp(value, "x2") == 0) {
	    flags = AXIS_BIT_X2;
	} else if (strcmp(value, "x1") == 0) {
	    flags = AXIS_BIT_X1;
	} else if (strcmp(value, "x") == 0) {
	    flags = AXIS_BIT_X1;
	} else if (strcmp(value, "both") == 0) {
	    flags = (AXIS_BIT_X1 | AXIS_BIT_X2);
	} else {
	    Tcl_AppendResult(interp, "bad x-axis type \"", value,
		"\": should be \"x\", \"x2\", or \"both\"", (char *)NULL);
	    return TCL_ERROR;
	}
    } else {
	if (strcmp(value, "y2") == 0) {
	    flags = AXIS_BIT_Y2;
	} else if (strcmp(value, "y1") == 0) {
	    flags = AXIS_BIT_Y1;
	} else if (strcmp(value, "y") == 0) {
	    flags = AXIS_BIT_Y1;
	} else if (strcmp(value, "both") == 0) {
	    flags = (AXIS_BIT_Y1 | AXIS_BIT_Y2);
	} else {
	    Tcl_AppendResult(interp, "bad y-axis type \"", value,
		"\": should be \"y\", \"y2\", or \"both\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    *flagsPtr &= ~mask;
    *flagsPtr |= flags;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NameOfMapAxis --
 *
 *	Convert the axis mask into a string.
 *
 * Results:
 *	The string representing the axis mapping is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
NameOfMapAxis(mask)
    int mask;
{
    switch (mask) {
    case AXIS_BIT_X1:
	return "x";
    case AXIS_BIT_X2:
	return "x2";
    case AXIS_BIT_Y1:
	return "y";
    case AXIS_BIT_Y2:
	return "y2";
    case (AXIS_BIT_X1 | AXIS_BIT_X2):
    case (AXIS_BIT_Y1 | AXIS_BIT_Y2):
	return "both";
    default:
	return "unknown axis mask";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MapAxisPrintProc --
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
MapAxisPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Widget record */
    int offset;			/* offset of AxisType field */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    int flags = *(int *)(widgRec + offset);
    int mask = (int)clientData;

    return (NameOfMapAxis(flags & mask));
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_GetCoordinate --
 *
 * 	Convert the expression string into a floating point value. The
 *	only reason we use this routine instead of Tcl_ExprDouble is to
 *	handle "elastic" bounds.  That is, convert the strings "-Inf",
 *	"Inf" into -(DBL_MAX) and DBL_MAX respectively.
 *
 * Results:
 *	The return value is a standard Tcl result.  The value of the
 * 	expression is passed back via valuePtr.
 *
 * ----------------------------------------------------------------------
 */
int
Blt_GetCoordinate(interp, expr, valuePtr)
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    char *expr;			/* Numeric expression string to parse */
    double *valuePtr;		/* Real-valued result of expression */
{
    char c;

    c = expr[0];
    if ((c == 'I') && (strcmp(expr, "Inf") == 0)) {
	*valuePtr = bltPosInfinity;	/* Elastic upper bound */
    } else if ((c == '-') && (expr[1] == 'I') && (strcmp(expr, "-Inf") == 0)) {
	*valuePtr = bltNegInfinity;	/* Elastic lower bound */
    } else if ((c == '+') && (expr[1] == 'I') && (strcmp(expr, "+Inf") == 0)) {
	*valuePtr = bltPosInfinity;	/* Elastic upper bound */
    } else if (Tcl_ExprDouble(interp, expr, valuePtr) != TCL_OK) {
	Tcl_AppendResult(interp, "bad expression \"", expr, "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetDistanceToSegment --
 *
 *	Calculates the closest point on a line segment to a given
 *	point.
 *
 * Results:
 *	Returns the distance from the given screen coordinate to the
 *	closest point on the line segment.
 *
 *----------------------------------------------------------------------
 */
double
Blt_GetDistanceToSegment(endPoints, x, y, closestXPtr, closestYPtr)
    XPoint endPoints[];		/* Window coordinates of the line segment */
    int x, y;			/* Screen coordinates of the sample point */
    double *closestXPtr;	/* Calculated screen coordinates of
				 * the point */
    double *closestYPtr;	/* in the line segment closest to the sample
				 * point */
{
    double dx, dy;
    double tx, ty;
    double dist;
    double x1, x2, y1, y2;

    x1 = (double)endPoints[0].x;
    y1 = (double)endPoints[0].y;
    x2 = (double)endPoints[1].x;
    y2 = (double)endPoints[1].y;

    dx = (x2 - x1);
    dy = (y2 - y1);

    /* Find the slope and y-intercept of the line segment */
    if (Fabs(dx) <= DBL_MIN) {	/* Horizontal line */
	tx = x1;
	ty = y;
    } else if (Fabs(dy) <= DBL_MIN) {	/* Vertical line */
	ty = y1;
	tx = x;
    } else {
	double m1, m2;		/* Slope of both lines */
	double b1, b2;		/* y-intercepts */

	m1 = (dy / dx);
	b1 = y1 - (x1 * m1);
	m2 = -(m1);		/* Line tangent to the segment will
				 * have a mirrored slope */
	b2 = y - (x * m2);

	/*
	 * Given the equations of two lines which contain the same point,
	 *
	 *    y = m1 * x + b1
	 *    y = m2 * x + b2
	 *
	 * solve for the intersection.
	 *
	 *    x = (b2 - b1) / (m1 - m2)
	 *    y = m1 * x + b1
	 *
	 */
	tx = (b2 - b1) / (m1 - m2);
	ty = m1 * tx + b1;
    }
    /*
     * Check if the point of intersection is outside the line segment
     * Remember, we don't know the order of the coordinates in the
     * line segment.
     */
    if ((((tx < x1) && (tx < x2)) || ((tx > x1) && (tx > x2))) ||
	(((ty < y1) && (ty < y2)) || ((ty > y1) && (ty > y2)))) {
	double d1, d2;		/* Distances to each end point */

	/* Compute distance to each end point and return the closest */
	d1 = bltHypot((double)x - x1, (double)y - y1);
	d2 = bltHypot((double)x - x2, (double)y - y2);
	if (d1 < d2) {
	    dist = d1;
	    tx = x1, ty = y1;
	} else {
	    dist = d2;
	    tx = x2, ty = y2;
	}
    } else {
	dist = bltHypot((double)x - tx, (double)y - ty);
    }
    *closestXPtr = tx;
    *closestYPtr = ty;
    return (dist);
}
#endif /* !NO_GRAPH */
