/*
 * bltGrElem.c --
 *
 *	This module implements elements in the graph widget
 *	for the Tk toolkit.
 *
 * Copyright 1991-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef NO_GRAPH

#include "bltGraph.h"
#include <ctype.h>

static int DataParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *DataPrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,Tcl_FreeProc **freeProcPtr));
static int DataPairsParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *DataPairsPrintProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));

#include "bltGrElem.h"

Tk_CustomOption bltXDataOption =
{
    DataParseProc, DataPrintProc, (ClientData)AXIS_MASK_X
};
Tk_CustomOption bltYDataOption =
{
    DataParseProc, DataPrintProc, (ClientData)AXIS_MASK_Y
};

Tk_CustomOption bltDataPairsOption =
{
    DataPairsParseProc, DataPairsPrintProc, (ClientData)0
};

extern Element *Blt_BarElement();
extern Element *Blt_LineElement();

extern Tk_CustomOption bltLengthOption;


/* ----------------------------------------------------------------------
 * Custom option parse and print procedures
 * ----------------------------------------------------------------------
 */

/*
 *----------------------------------------------------------------------
 *
 * FindLimits --
 *
 *	Find the minimum, positive minimum, and maximum values in a
 *	given vector and store the results in the vector structure.
 *
 * Results:
 *     	None.
 *
 * Side Effects:
 *	Minimum, positive minimum, and maximum values are stored in
 *	the vector.
 *
 *----------------------------------------------------------------------
 */
static void
FindLimits(vPtr)
    Vector *vPtr;
{
    register int i;
    register double *dataPtr;
    register double min, max;

    if ((vPtr->vector.numValues < 1) || (vPtr->vector.valueArr == NULL)) {
	return;			/* This shouldn't ever happen */
    }
    dataPtr = vPtr->vector.valueArr;

    /*  Initialize values to track the vector limits */
    min = max = *dataPtr;
    dataPtr++;
    for (i = 1; i < vPtr->vector.numValues; i++) {
	if (*dataPtr < min) {
	    min = *dataPtr;
	} else if (*dataPtr > max) {
	    max = *dataPtr;
	}
	dataPtr++;
    }
    vPtr->vector.min = min;
    vPtr->vector.max = max;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_FindVectorMinimum --
 *
 *	Find the minimum, positive minimum, and maximum values in a
 *	given vector and store the results in the vector structure.
 *
 * Results:
 *     	None.
 *
 * Side Effects:
 *	Minimum, positive minimum, and maximum values are stored in
 *	the vector.
 *
 *----------------------------------------------------------------------
 */
double
Blt_FindVectorMinimum(vPtr, minLimit)
    Vector *vPtr;
    double minLimit;
{
    register int i;
    register double *dataPtr;
    register double min;

    min = bltPosInfinity;
    dataPtr = vPtr->vector.valueArr;
    for (i = 0; i < vPtr->vector.numValues; i++) {
	if ((*dataPtr > minLimit) && (min > *dataPtr)) {
	    min = *dataPtr;
	}
	dataPtr++;
    }
    if (min == bltPosInfinity) {
	min = minLimit;
    }
    return (min);
}

/*
 *----------------------------------------------------------------------
 *
 * SetVectorSize --
 *
 *	Sets the size of the given vector, allocating the an array
 *	of the given number of elements.
 *
 * Results:
 *     	Always TCL_OK.
 *
 *----------------------------------------------------------------------
 */
static int
SetVectorSize(vPtr, size)
    Vector *vPtr;
    int size;
{
    /*
     * Release any memory associated with the current vector.  If it's
     * malloc-ed, free it.  If it's using a BLT vector, free the vector.
     */
    if ((vPtr->clientId == NULL) && (vPtr->vector.valueArr != NULL)) {
	ckfree((char *)vPtr->vector.valueArr);
    }
    vPtr->vector.valueArr = NULL;
    vPtr->vector.numValues = 0;

    if (vPtr->clientId != NULL) {
	Blt_FreeVectorId(vPtr->clientId);
	vPtr->clientId = NULL;
    }
    if (size > 0) {
	double *newArr;

	newArr = (double *)ckcalloc(sizeof(double), size);
	if (newArr == NULL) {
	    Panic("can't allocate new vector array");
	}
	vPtr->vector.valueArr = newArr;
	vPtr->vector.numValues = size;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * VectorChangedProc --
 *
 *
 * Results:
 *     	None.
 *
 * Side Effects:
 *	Graph is redrawn.
 *
 *----------------------------------------------------------------------
 */
static void
VectorChangedProc(interp, clientData, notify)
    Tcl_Interp *interp;
    ClientData clientData;
    Blt_VectorNotify notify;
{
    Vector *vPtr = (Vector *)clientData;
    Element *elemPtr = vPtr->elemPtr;
    Graph *graphPtr = elemPtr->graphPtr;

    switch (notify) {
    case BLT_VECTOR_NOTIFY_DESTROY:
	vPtr->clientId = NULL;
	vPtr->vector.valueArr = NULL;
	vPtr->vector.numValues = 0;
	break;

    default:
    case BLT_VECTOR_NOTIFY_UPDATE:
	Blt_GetVectorById(interp, vPtr->clientId, &(vPtr->vector));
	break;

    }
    graphPtr->flags |= RESET_AXES;
    elemPtr->flags |= COORDS_NEEDED;
    if (elemPtr->mapped) {
	graphPtr->flags |= UPDATE_PIXMAP;
	Blt_RedrawGraph(graphPtr);
    }
}

static int
EvalExprList(interp, list, numElemPtr, dataPtrPtr)
    Tcl_Interp *interp;
    char *list;
    int *numElemPtr;
    double **dataPtrPtr;
{
    int numElem;
    char **elemArr;
    double *dataArr;
    int result;

    result = TCL_ERROR;
    elemArr = NULL;
    if (Tcl_SplitList(interp, list, &numElem, &elemArr) != TCL_OK) {
	return TCL_ERROR;
    }
    dataArr = NULL;
    if (numElem > 0) {
	register double *dataPtr;
	register int i;

	dataArr = (double *)ckalloc(sizeof(double) * numElem);
	if (dataArr == NULL) {
	    Tcl_SetResult(interp, "can't allocate new vector", TCL_STATIC);
	    goto badList;
	}
	dataPtr = dataArr;
	for (i = 0; i < numElem; i++) {
	    if (Tcl_ExprDouble(interp, elemArr[i], dataPtr) != TCL_OK) {
		goto badList;
	    }
	    dataPtr++;
	}
    }
    result = TCL_OK;
  badList:
    ckfree((char *)elemArr);
    *dataPtrPtr = dataArr;
    *numElemPtr = numElem;
    if (result != TCL_OK) {
	ckfree((char *)dataArr);
    }
    return (result);
}

/*
 *----------------------------------------------------------------------
 *
 * DataParseProc --
 *
 *	Given a Tcl list of numeric expression representing the element
 *	values, convert into an array of double precision values. In
 *	addition, the minimum and maximum values are saved.  Since
 *	elastic values are allow (values which translate to the
 *	min/max of the graph), we must try to get the non-elastic
 *	minimum and maximum.
 *
 * Results:
 *	The return value is a standard Tcl result.  The vector is passed
 *	back via the vPtr.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DataParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* Type of axis vector to fill */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* Tcl list of expressions */
    char *widgRec;		/* Element record */
    int offset;			/* Offset of vector in Element record */
{
    Element *elemPtr = (Element *)(widgRec + offset);
    Vector *vPtr;
    int flags = (int)clientData;

    vPtr = (flags & AXIS_MASK_X) ? &(elemPtr->x) : &(elemPtr->y);

    if (Blt_VectorExists(interp, value)) {
	Blt_VectorId clientId;

	clientId = Blt_AllocVectorId(interp, value);
	SetVectorSize(vPtr, 0);	/* Free any memory associated with the
				 * vector. */
	if (Blt_GetVectorById(interp, clientId, &(vPtr->vector)) != TCL_OK) {
	    return TCL_ERROR;
	}
	Blt_SetVectorChangedProc(clientId,
	    (Blt_VectorChangedProc*)VectorChangedProc, (ClientData)vPtr);
	vPtr->elemPtr = elemPtr;
	vPtr->clientId = clientId;

    } else {
	double *newArr;
	int numValues;

	if (EvalExprList(interp, value, &numValues, &newArr) != TCL_OK) {
	    return TCL_ERROR;
	}
	SetVectorSize(vPtr, numValues);
	if (numValues > 0) {
	    memcpy((char *)vPtr->vector.valueArr, (char *)newArr,
		sizeof(double) * numValues);
	    FindLimits(vPtr);
	    ckfree((char *)newArr);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DataPrintProc --
 *
 *	Convert the vector of floating point values into a Tcl list.
 *
 * Results:
 *	The string representation of the vector is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
DataPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Type of axis vector to print */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Element record */
    int offset;			/* Offset of vector in Element record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Element *elemPtr = (Element *)(widgRec + offset);
    Vector *vPtr;
    int flags = (int)clientData;
    Tcl_DString valueList;
    char string[TCL_DOUBLE_SPACE + 1];
    char *result;
    register int i;

    vPtr = (flags & AXIS_MASK_X) ? &(elemPtr->x) : &(elemPtr->y);
    if (vPtr->clientId != NULL) {
	return Blt_NameOfVectorId(vPtr->clientId);
    }
    if (vPtr->vector.numValues == 0) {
	return "";
    }
    Tcl_DStringInit(&valueList);
    for (i = 0; i < vPtr->vector.numValues; i++) {
	Tcl_PrintDouble(elemPtr->graphPtr->interp, vPtr->vector.valueArr[i], 
		string);
	Tcl_DStringAppendElement(&valueList, string);
    }
    *freeProcPtr = (Tcl_FreeProc *)BLT_ckfree;
    result = ckstrdup(Tcl_DStringValue(&valueList));
    Tcl_DStringFree(&valueList);
    return (result);
}

/*
 *----------------------------------------------------------------------
 *
 * DataPairsParseProc --
 *
 *	This procedure is like DataParseProc except that it
 *	interprets the list of numeric expressions as X Y coordinate
 *	pairs.  The minimum and maximum for both the X and Y vectors are
 *	determined.
 *
 * Results:
 *	The return value is a standard Tcl result.  The vectors are passed
 *	back via the widget record (elemPtr).
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DataPairsParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* Tcl list of numeric expressions */
    char *widgRec;		/* Element record */
    int offset;			/* not used */
{
    Element *elemPtr = (Element *)widgRec;
    int numElem;
    double *newArr;
    int newSize;

    if (EvalExprList(interp, value, &numElem, &newArr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (numElem & 1) {
	Tcl_SetResult(interp, "odd number of data points", TCL_STATIC);
	ckfree((char *)newArr);
	return TCL_ERROR;
    }
    newSize = numElem / 2;
    SetVectorSize(&(elemPtr->x), newSize);
    SetVectorSize(&(elemPtr->y), newSize);
    if (newSize > 0) {
	register int i;
	register int count;

	count = 0;
	for (i = 0; i < numElem; i += 2) {
	    elemPtr->x.vector.valueArr[count] = newArr[i];
	    elemPtr->y.vector.valueArr[count] = newArr[i + 1];
	    count++;
	}
	ckfree((char *)newArr);
	FindLimits(&(elemPtr->x));
	FindLimits(&(elemPtr->y));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DataPairsPrintProc --
 *
 *	Convert pairs of floating point values in the X and Y arrays
 *	into a Tcl list.
 *
 * Results:
 *	The return value is a string (Tcl list).
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
DataPairsPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Element information record */
    int offset;			/* not used */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Element *elemPtr = (Element *)widgRec;
    Tcl_Interp *interp = elemPtr->graphPtr->interp;
    int i;
    int length;
    char *result;
    char string[TCL_DOUBLE_SPACE + 1];
    Tcl_DString valueList;

    length = ELEMENT_LENGTH(elemPtr);
    if (length < 1) {
	return "";
    }
    Tcl_DStringInit(&valueList);
    for (i = 0; i < length; i++) {
	Tcl_PrintDouble(interp, elemPtr->x.vector.valueArr[i], string);
	Tcl_DStringAppendElement(&valueList, string);
	Tcl_PrintDouble(interp, elemPtr->y.vector.valueArr[i], string);
	Tcl_DStringAppendElement(&valueList, string);
    }
    *freeProcPtr = (Tcl_FreeProc *)BLT_ckfree;
    result = ckstrdup(Tcl_DStringValue(&valueList));
    Tcl_DStringFree(&valueList);
    return (result);
}

/*
 * Generic element routines:
 */
static char *
NameOfElementType(elemPtr)
    Element *elemPtr;
{
    switch (elemPtr->type) {
    case ELEM_LINE:
	return "line";
    case ELEM_BAR:
	return "bar";
    default:
	return "unknown element type";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetIndex --
 *
 *	Given a string representing the index of a pair of x,y
 *	coordinates, return the numeric index.
 *
 * Results:
 *     	A standard TCL result.
 *
 *----------------------------------------------------------------------
 */
static int
GetIndex(interp, elemPtr, string, indexPtr)
    Tcl_Interp *interp;
    Element *elemPtr;
    char *string;
    int *indexPtr;
{
    long elemIndex;
    int last;

    last = ELEMENT_LENGTH(elemPtr) - 1;
    if ((*string == 'e') && (strcmp("end", string) == 0)) {
	elemIndex = last;
    } else if (Tcl_ExprLong(interp, string, &elemIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((elemIndex < 0) || (elemIndex > last)) {
	Tcl_AppendResult(interp, "index \"", string,
	    "\" is out of range for element", (char *)NULL);
	return TCL_ERROR;
    }
    *indexPtr = (int)elemIndex;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetElement --
 *
 *	Find the element represented the given name,  returning
 *	a pointer to its data structure in *elemPtrPtr*.
 *
 * Results:
 *     	A standard TCL result.
 *
 *----------------------------------------------------------------------
 */
int
Blt_GetElement(graphPtr, name, elemPtrPtr)
    Graph *graphPtr;
    char *name;
    Element **elemPtrPtr;
{
    Tcl_HashEntry *hPtr;

    /* Find the named element */
    hPtr = Tcl_FindHashEntry(&(graphPtr->elemTable), name);
    if (hPtr == NULL) {
	Tcl_AppendResult(graphPtr->interp, "can't find element \"", name,
	    "\" in \"", Tk_PathName(graphPtr->tkwin), (char *)NULL);
	return TCL_ERROR;
    }
    *elemPtrPtr = (Element *)Tcl_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateElement --
 *
 *	Add a new element to the graph.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static int
CreateElement(graphPtr, interp, argc, argv, type)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
    ElementType type;
{
    Element *elemPtr;
    Tcl_HashEntry *hPtr;
    int dummy;

    if (Blt_GetElement(graphPtr, argv[3], &elemPtr) == TCL_OK) {
	Tcl_AppendResult(interp, "element \"", argv[3],
	    "\" already exists in \"", argv[0], "\"", (char *)NULL);
	return TCL_ERROR;
    }
    /* Clear error message left by Blt_GetElement() */
    Tcl_ResetResult(interp);

    if (type == ELEM_BAR) {
	elemPtr = Blt_BarElement();
    } else if (type == ELEM_LINE) {
	elemPtr = Blt_LineElement();
    }
    if (elemPtr == NULL) {
	Tcl_AppendResult(interp, "can't create element \"", argv[3], "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    elemPtr->graphPtr = graphPtr;
    elemPtr->axisMask = DEF_AXIS_MASK;
    elemPtr->nameId = Tk_GetUid(argv[3]);
    elemPtr->mapped = 1;

    /* Allocate memory for the label, since the label may change */
    elemPtr->label = ckstrdup(argv[3]);

    if (Tk_ConfigureWidget(interp, graphPtr->tkwin, elemPtr->configSpecs,
	    argc - 4, argv + 4, (char *)elemPtr, 0) != TCL_OK) {
	(*elemPtr->destroyProc) (graphPtr, elemPtr);
	return TCL_ERROR;
    }
    hPtr = Tcl_CreateHashEntry(&(graphPtr->elemTable), (char *)elemPtr->nameId,
	&dummy);
    Tcl_SetHashValue(hPtr, (ClientData)elemPtr);

    (*elemPtr->configProc) (graphPtr, elemPtr);

    elemPtr->flags |= COORDS_NEEDED;
    graphPtr->flags |= RESET_AXES | DRAW_MARGINS;
    if (elemPtr->mapped) {
	Blt_ListItem *itemPtr;

	itemPtr = Blt_NewItem(elemPtr->nameId);
	Blt_SetItemValue(itemPtr, elemPtr);
	Blt_LinkAfter(&(graphPtr->elemList), itemPtr, (Blt_ListItem *)NULL);
	graphPtr->flags |= UPDATE_PIXMAP;
	Blt_RedrawGraph(graphPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RebuildDisplayList --
 *
 *	Given a Tcl list of element names, this procedure rebuilds the
 *	display list, ignoring invalid element names. This list describes
 *	not only only which elements to draw, but in what order.  This is
 *	only important for bar and pie charts.
 *
 * Results:
 *	The return value is a standard Tcl result.  Only if the Tcl list
 *	cannot be split, a TCL_ERROR is returned and interp->result contains
 *	an error message.
 *
 * Side effects:
 *	The graph is eventually redrawn using the new display list.
 *
 *----------------------------------------------------------------------
 */
static int
RebuildDisplayList(graphPtr, newList)
    Graph *graphPtr;		/* Graph widget record */
    char *newList;		/* Tcl list of element names */
{
    int numNames;		/* Number of names found in Tcl name list */
    char **nameArr;		/* Broken out array of element names */
    Blt_ListItem *itemPtr;
    Tcl_HashSearch cursor;
    register int i;
    register Tcl_HashEntry *hPtr;
    Element *elemPtr;		/* Element information record */

    if (Tcl_SplitList(graphPtr->interp, newList, &numNames,
	    &nameArr) != TCL_OK) {
	Tcl_AppendResult(graphPtr->interp, "can't split name list \"", newList,
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    /*
     * Clear the display list and mark all elements as unmapped.
     */
    Blt_ResetList(&(graphPtr->elemList));
    for (hPtr = Tcl_FirstHashEntry(&(graphPtr->elemTable), &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	elemPtr = (Element *)Tcl_GetHashValue(hPtr);
	elemPtr->mapped = FALSE;
    }

    /*
     * Rebuild the display list, checking that each name it exists
     * (currently ignoring invalid element names).
     */
    for (i = 0; i < numNames; i++) {
	if (Blt_GetElement(graphPtr, nameArr[i], &elemPtr) != TCL_OK) {
	    continue;
	}
	elemPtr->mapped = TRUE;
	itemPtr = Blt_NewItem(elemPtr->nameId);
	Blt_SetItemValue(itemPtr, elemPtr);
	Blt_LinkAfter(&(graphPtr->elemList), itemPtr, (Blt_ListItem *)NULL);
    }
    ckfree((char *)nameArr);
    graphPtr->flags |= SET_ALL_FLAGS;
    Blt_RedrawGraph(graphPtr);
    Tcl_ResetResult(graphPtr->interp);
    return TCL_OK;
}

void
Blt_DestroyElements(graphPtr)
    Graph *graphPtr;
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;
    Element *elemPtr;

    Blt_ResetList(&(graphPtr->elemList));
    for (hPtr = Tcl_FirstHashEntry(&(graphPtr->elemTable), &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	elemPtr = (Element *)Tcl_GetHashValue(hPtr);
	(*elemPtr->destroyProc) (graphPtr, elemPtr);
    }
    Tcl_DeleteHashTable(&(graphPtr->elemTable));
}

void
Blt_GetElementCoordinates(graphPtr)
    Graph *graphPtr;
{
    Blt_ListItem *itemPtr;
    Element *elemPtr;

    if (graphPtr->mode != MODE_NORMAL) {
	Blt_ResetStacks(graphPtr);
    }
    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	if ((graphPtr->flags & COORDS_ALL_PARTS) ||
	    (elemPtr->flags & COORDS_NEEDED)) {
	    (*elemPtr->coordsProc) (graphPtr, elemPtr);
	    elemPtr->flags &= ~COORDS_NEEDED;
	}
    }
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_DrawElements --
 *
 *	Calls the individual element drawing routines for each
 *	element.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Elements are drawn into the drawable (pixmap) which will
 *	eventually be displayed in the graph window.
 *
 * -----------------------------------------------------------------
 */
void
Blt_DrawElements(graphPtr)
    Graph *graphPtr;
{
    Blt_ListItem *itemPtr;
    register Element *elemPtr;

    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	(*elemPtr->drawNormalProc) (graphPtr, elemPtr);
    }
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_DrawActiveElements --
 *
 *	Calls the individual element drawing routines to display
 *	the active colors for each element.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Elements are drawn into the drawable (pixmap) which will
 *	eventually be displayed in the graph window.
 *
 * -----------------------------------------------------------------
 */
void
Blt_DrawActiveElements(graphPtr)
    Graph *graphPtr;
{
    Blt_ListItem *itemPtr;
    register Element *elemPtr;

    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	if (elemPtr->flags & ELEM_ACTIVE) {
	    (*elemPtr->drawActiveProc) (graphPtr, elemPtr);
	}
    }
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_PrintElements --
 *
 *	Generates PostScript output for each graph element in the 
 *	element display list. 
 *
 * -----------------------------------------------------------------
 */
void
Blt_PrintElements(graphPtr)
    Graph *graphPtr;
{
    Blt_ListItem *itemPtr;
    Element *elemPtr;

    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	/* Comment the PostScript to indicate the start of the element */
	Tcl_AppendResult(graphPtr->interp, "\n% Element \"", elemPtr->nameId, 
		"\"\n\n", (char *)NULL);
	(*elemPtr->printNormalProc) (graphPtr, elemPtr);
    }
}

/*
 * -----------------------------------------------------------------
 *
 * Blt_PrintActiveElements --
 *
 * -----------------------------------------------------------------
 */
void
Blt_PrintActiveElements(graphPtr)
    Graph *graphPtr;
{
    Blt_ListItem *itemPtr;
    register Element *elemPtr;

    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	if ((elemPtr->mapped) && (elemPtr->flags & ELEM_ACTIVE)) {
	    Tcl_AppendResult(graphPtr->interp, "\n% Active Element \"",
		elemPtr->nameId, "\"\n\n", (char *)NULL);
	    (*elemPtr->printActiveProc) (graphPtr, elemPtr);
	}
    }
}


int
Blt_GraphUpdateNeeded(graphPtr)
    Graph *graphPtr;
{
    Blt_ListItem *itemPtr;
    register Element *elemPtr;

    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	/* Check if the x or y vectors have notifications pending */
	if (((elemPtr->x.clientId != NULL) &&
	    (Blt_VectorNotifyPending(elemPtr->x.clientId))) ||
	    ((elemPtr->y.clientId != NULL) && 
	    (Blt_VectorNotifyPending(elemPtr->y.clientId)))) {
	    return 1;
	}
    }
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * ActivateOper --
 *
 *	Find the element closest to the window coordinates specified.
 *
 * Results:
 *	Returns TCL_OK if no errors occurred. The result field of the
 *	interpreter may contain a list containing the element name,
 *	the index of the closest point, and the x and y graph coordinates
 *	of the point is stored.  If an error occurred, returns TCL_ERROR
 *	and an error message is left in interp->result.
 *
 *----------------------------------------------------------------------
 */
static int
ActivateOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;			/* Number of element names */
    char **argv;		/* List of element names */
{
    Element *elemPtr;
    register int i;
    int *activeArr;
    int numActiveIndices;

    if (Blt_GetElement(graphPtr, argv[3], &elemPtr) != TCL_OK) {
	return TCL_ERROR;	/* Can't find named element */
    }
    elemPtr->flags |= ELEM_ACTIVE;

    activeArr = NULL;
    numActiveIndices = -1;
    if (argc > 4) {
	register int *activePtr;

	numActiveIndices = argc - 4;
	activeArr = (int *)ckalloc(sizeof(int) * numActiveIndices);
	if (activeArr == NULL) {
	    Panic("can't allocate active element index array");
	}
	activePtr = activeArr;
	for (i = 4; i < argc; i++) {
	    if (GetIndex(interp, elemPtr, argv[i], activePtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    activePtr++;
	}
    }
    if (elemPtr->activeIndexArr != NULL) {
	ckfree((char *)elemPtr->activeIndexArr);
    }
    elemPtr->numActiveIndices = numActiveIndices;
    elemPtr->activeIndexArr = activeArr;
    Blt_RedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * BarOper --
 *
 *	Add a new bar element to the graph.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static int
BarOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    return CreateElement(graphPtr, interp, argc, argv, ELEM_BAR);
}

/*
 *----------------------------------------------------------------------
 *
 * CreateOper --
 *
 *	Add a new element to the graph (using the default type of the graph).
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static int
CreateOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    return CreateElement(graphPtr, interp, argc, argv, graphPtr->defElemType);
}

/*
 *----------------------------------------------------------------------
 *
 * CgetOper --
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    Element *elemPtr;

    if (Blt_GetElement(graphPtr, argv[3], &elemPtr) != TCL_OK) {
	return TCL_ERROR;	/* Can't find named element */
    }
    if (Tk_ConfigureValue(interp, graphPtr->tkwin, elemPtr->configSpecs,
	    (char *)elemPtr, argv[4], 0) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ClosestOper --
 *
 *	Find the element closest to the window coordinates specified.
 *
 * Results:
 *	Returns TCL_OK if no errors occurred. The result field of the
 *	interpreter may contain a list containing the element name,
 *	the index of the closest point, and the x and y graph coordinates
 *	of the point is stored.  If an error occurred, returns TCL_ERROR
 *	and an error message is left in interp->result.
 *
 *----------------------------------------------------------------------
 */

static Tk_ConfigSpec closestSpecs[] =
{
    {TK_CONFIG_CUSTOM, "-halo", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(ClosestInfo, halo), 0, &bltLengthOption},
    {TK_CONFIG_BOOLEAN, "-interpolate", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(ClosestInfo, interpolate), 0},
    {TK_CONFIG_END, (char *)NULL, (char *)NULL, (char *)NULL,
	(char *)NULL, 0, 0}
};

static int
ClosestOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget */
    Tcl_Interp *interp;		/* Interpreter to report results to */
    int argc;			/* Number of element names */
    char **argv;		/* List of element names */
{
    Element *elemPtr;
    ClosestInfo info;
    int i;

    if (graphPtr->flags & RESET_AXES) {
	Blt_ComputeAxes(graphPtr);
    }
    if (Tk_GetPixels(interp, graphPtr->tkwin, argv[3], &(info.x)) != TCL_OK) {
	Tcl_AppendResult(interp, ": bad window x-coordinate", (char *)NULL);
	return TCL_ERROR;
    }
    if (Tk_GetPixels(interp, graphPtr->tkwin, argv[4], &(info.y)) != TCL_OK) {
	Tcl_AppendResult(interp, ": bad window y-coordinate", (char *)NULL);
	return TCL_ERROR;
    }
    if (graphPtr->inverted) {
	int hold;

	hold = info.x, info.x = info.y, info.y = hold;
    }
    for (i = 6; i < argc; i += 2) {	/* Count switches-value pairs */
	if ((argv[i][0] != '-') ||
	    ((argv[i][1] == '-') && (argv[i][2] == '\0'))) {
	    break;
	}
    }
    if (i > argc) {
	i = argc;
    }
    info.interpolate = 0;
    info.halo = graphPtr->halo;
    info.index = -1;

    if (Tk_ConfigureWidget(interp, graphPtr->tkwin, closestSpecs, i - 6,
	    argv + 6, (char *)&info, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;	/* Error processing option */
    }
    if ((i < argc) && (argv[i][0] == '-')) {
	i++;			/* Skip "--" */
    }
    info.dist = (double)(info.halo + 1);

    if (i < argc) {
	for ( /* empty */ ; i < argc; i++) {
	    if (Blt_GetElement(graphPtr, argv[i], &elemPtr) != TCL_OK) {
		return TCL_ERROR;	/* Can't find named element */
	    }
	    if (!elemPtr->mapped) {
		Tcl_AppendResult(interp, "element \"", argv[i],
		    "\" isn't mapped", (char *)NULL);
		return TCL_ERROR;	/* Only examine visible elements */
	    }
	    (*elemPtr->closestProc) (graphPtr, elemPtr, &info);
	}
    } else {
	Blt_ListItem *itemPtr;
	/*
	 * Find the closest point in set of displayed elements,
	 * searching the display list from back to front.  That
	 * if two points (from two different elements) are on top
	 * of each other, the one that's visible is returned.
	 */
	for (itemPtr = Blt_LastListItem(&(graphPtr->elemList));
	    itemPtr != NULL; itemPtr = Blt_PrevItem(itemPtr)) {
	    elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	    if (elemPtr->mapped) {
		(*elemPtr->closestProc) (graphPtr, elemPtr, &info);
	    }
	}
    }
    if (info.dist < (double)info.halo) {
	char string[200];
	/*
	 *  Return an array of 5 elements
	 */
	if (Tcl_SetVar2(interp, argv[5], "name", info.elemId, 0) == NULL) {
	    return TCL_ERROR;
	}
	sprintf(string, "%d", info.index);
	if (Tcl_SetVar2(interp, argv[5], "index", string, 0) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_PrintDouble(interp, info.closest.x, string);
	if (Tcl_SetVar2(interp, argv[5], "x", string, 0) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_PrintDouble(interp, info.closest.y, string);
	if (Tcl_SetVar2(interp, argv[5], "y", string, 0) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_PrintDouble(interp, info.dist, string);
	if (Tcl_SetVar2(interp, argv[5], "dist", string, 0) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetResult(interp, "1", TCL_STATIC);
    } else {
	if (Tcl_SetVar2(interp, argv[5], "name", "", 0) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetResult(interp, "0", TCL_STATIC);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOper --
 *
 *	Sets the element specifications by the given the command line
 *	arguments and calls the element specification configuration
 *	routine. If zero or one command line options are given, only
 *	information about the option(s) is returned in interp->result.
 *      If the element configuration has changed and the element is
 *	currently displayed, the axis limits are updated and recomputed.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	Graph will be redrawn to reflect the new display list.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigureOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    Element *elemPtr;
    int flags = TK_CONFIG_ARGV_ONLY;
    int result;

    if (Blt_GetElement(graphPtr, argv[3], &elemPtr) != TCL_OK) {
	return TCL_ERROR;	/* Can't find named element */
    }
    if (argc == 4) {
	return (Tk_ConfigureInfo(interp, graphPtr->tkwin, elemPtr->configSpecs,
		(char *)elemPtr, (char *)NULL, flags));
    } else if (argc == 5) {
	return (Tk_ConfigureInfo(interp, graphPtr->tkwin, elemPtr->configSpecs,
		(char *)elemPtr, argv[4], flags));
    }
    result = Tk_ConfigureWidget(interp, graphPtr->tkwin, elemPtr->configSpecs,
	argc - 4, argv + 4, (char *)elemPtr, flags);
    if ((*elemPtr->configProc) (graphPtr, elemPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_ConfigModified(elemPtr->configSpecs, "-mapped", (char *)NULL)) {
	Blt_ListItem *iPtr;

	/* If the legend is being displayed will need to redraw it. */
	graphPtr->flags |= DRAW_MARGINS;

	iPtr = Blt_FindItem(&(graphPtr->elemList), elemPtr->nameId);
	if ((elemPtr->mapped) != (iPtr != NULL)) {
	    /*
	     * The element's "mapped" variable is out of sync with the
	     * display list.  (That's what you get for having two ways
	     * to do the same thing.) Update the display list by
	     * either by adding or removing the element.
	     */
	    if (iPtr == NULL) {
		iPtr = Blt_NewItem(elemPtr->nameId);
		Blt_SetItemValue(iPtr, elemPtr);
		Blt_LinkAfter(&(graphPtr->elemList), iPtr,(Blt_ListItem *)NULL);
	    } else {
		Blt_DeleteItem(iPtr);
	    }
	}
    }

    /* The following options may affect the appearance of the legend.
     * They are in two groups because Blt_ConfigModified() can only
     * accept a limited number of arguments (see bltUtil.c for details).
     */
    if (Blt_ConfigModified(elemPtr->configSpecs, "-foreground", "-background",
	    "-stipple", "-outline", "-color", "-symbol", (char *)NULL) ||
	    Blt_ConfigModified(elemPtr->configSpecs, "-outlinewidth",
	    "-dashes", "-linewidth", "-fill", "-offdash", (char *)NULL)) {
	graphPtr->flags |= DRAW_MARGINS;
    }

    if (Blt_ConfigModified(elemPtr->configSpecs, "-*data", "-map*",
	    "-barwidth", (char *)NULL)) {
	/*
	 * Data points, the width of bars, or axes have changed: Reset
	 * the axes (may affect autoscaling) and recalculate the screen
	 * points of the element.
	 */
	graphPtr->flags |= RESET_AXES;
	elemPtr->flags |= COORDS_NEEDED;
    }
    if (Blt_ConfigModified(elemPtr->configSpecs, "-label", (char *)NULL)) {
	/*
	 * The new label may change the size of the legend, affecting
	 * margins.
	 */
	graphPtr->flags |= (COORDS_WORLD | REDRAW_WORLD);
    }
    /* Update the pixmap if any configuration option changed */
    graphPtr->flags |= UPDATE_PIXMAP;
    Blt_RedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DeactivateOper --
 *
 *	Clears the active bit for the named elements.
 *
 * Results:
 *	Returns TCL_OK if no errors occurred.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeactivateOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget */
    Tcl_Interp *interp;		/* not used */
    int argc;			/* Number of element names */
    char **argv;		/* List of element names */
{
    Element *elemPtr;
    Tcl_HashSearch cursor;
    register Tcl_HashEntry *hPtr;
    register int i;

    for (hPtr = Tcl_FirstHashEntry(&(graphPtr->elemTable), &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	elemPtr = (Element *)Tcl_GetHashValue(hPtr);
	for (i = 3; i < argc; i++) {
	    if (Tcl_StringMatch(elemPtr->nameId, argv[i])) {
		break;
	    }
	}
	if (i < argc) {
	    elemPtr->flags &= ~ELEM_ACTIVE;
	    if (elemPtr->activeIndexArr != NULL) {
		ckfree((char *)elemPtr->activeIndexArr);
		elemPtr->activeIndexArr = NULL;
	    }
	    elemPtr->numActiveIndices = 0;
	}
    }
    Blt_RedrawGraph(graphPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteOper --
 *
 *	Delete the named elements from the graph.
 *
 * Results:
 *	TCL_ERROR is returned if any of the named elements can not be found.
 *	Otherwise TCL_OK is returned;
 *
 * Side Effects:
 *	If the element is currently mapped, the plotting area of the
 *	graph is redrawn. Memory and resources allocated by the elements
 *	are released.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget */
    Tcl_Interp *interp;		/* not used */
    int argc;			/* Number of element names */
    char **argv;		/* List of element names */
{
    Element *elemPtr;
    Blt_List deleteList;
    Tcl_HashSearch cursor;
    register int i;
    register Tcl_HashEntry *hPtr;
    register Blt_ListItem *itemPtr;

    /*
     * Make a list of the elements matching one of the patterns.
     * We'll use this list to prune both the display list and the
     * element hash table of their entries.
     */
    Blt_InitList(&deleteList, TCL_ONE_WORD_KEYS);
    for (hPtr = Tcl_FirstHashEntry(&(graphPtr->elemTable), &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	elemPtr = (Element *)Tcl_GetHashValue(hPtr);
	for (i = 3; i < argc; i++) {
	    if (Tcl_StringMatch(elemPtr->nameId, argv[i])) {
		break;
	    }
	}
	if (i < argc) {
	    itemPtr = Blt_NewItem((char *)elemPtr);
	    Blt_SetItemValue(itemPtr, (ClientData)hPtr);
	    Blt_LinkAfter(&deleteList, itemPtr, (Blt_ListItem *)0);
	}
    }
    if (deleteList.numEntries > 0) {
	int redraw;
	Blt_ListItem *deletePtr;

	redraw = 0;
	for (itemPtr = Blt_FirstListItem(&deleteList); itemPtr != NULL;
	    itemPtr = Blt_NextItem(itemPtr)) {
	    elemPtr = (Element *)Blt_GetItemKey(itemPtr);
	    Tcl_DeleteHashEntry((Tcl_HashEntry *) Blt_GetItemValue(itemPtr));
	    deletePtr = Blt_FindItem(&(graphPtr->elemList), elemPtr->nameId);
	    if (deletePtr != NULL) {
		Blt_DeleteItem(deletePtr);
		redraw++;
	    }
	    /*
	     * Call the element's own destructor to release the memory and
	     * resources allocated for it.
	     */
	    (*elemPtr->destroyProc) (graphPtr, elemPtr);
	}
	if (redraw) {
	    graphPtr->flags |= SET_ALL_FLAGS;
	    Blt_RedrawGraph(graphPtr);
	}
    }
    Blt_ResetList(&deleteList);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExistsOper --
 *
 *	Runs through the given list of element entries and builds a
 *	Tcl list of element names.  This procedure is used in the
 *	"names" and "show" commands.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *	interp->result contains the list of element names.
 *
 *----------------------------------------------------------------------
 */
static int
ExistsOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;			/* unused */
    char **argv;
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&(graphPtr->elemTable), argv[3]);
    Tcl_SetResult(interp, (hPtr != NULL) ? "1" : "0", TCL_STATIC);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LineOper --
 *
 *	Add a new line element to the graph.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static int
LineOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    return (CreateElement(graphPtr, interp, argc, argv, ELEM_LINE));
}

/*
 *----------------------------------------------------------------------
 *
 * NamesOper --
 *
 *	Runs through the given list of element entries and builds a
 *	Tcl list of element names.  This procedure is used in the
 *	"names" and "show" commands.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *	interp->result contains the list of element names.
 *
 *----------------------------------------------------------------------
 */
static int
NamesOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Element *elemPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;

    for (hPtr = Tcl_FirstHashEntry(&(graphPtr->elemTable), &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	elemPtr = (Element *)Tcl_GetHashValue(hPtr);
	if ((argc == 3) || Tcl_StringMatch(elemPtr->nameId, argv[3])) {
	    Tcl_AppendElement(interp, elemPtr->nameId);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ShowOper --
 *
 *	Displays or rebuilds the element display list.
 *
 * Results:
 *	The return value is a standard Tcl result. interp->result
 *	will contain the new display list of element names.
 *
 *----------------------------------------------------------------------
 */
static int
ShowOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Element *elemPtr;
    Blt_ListItem *itemPtr;

    if (argc == 4) {
	if (RebuildDisplayList(graphPtr, argv[3]) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    for (itemPtr = Blt_FirstListItem(&(graphPtr->elemList));
	itemPtr != NULL; itemPtr = Blt_NextItem(itemPtr)) {
	elemPtr = (Element *)Blt_GetItemValue(itemPtr);
	Tcl_AppendElement(interp, (char *)elemPtr->nameId);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TypeOper --
 *
 *	Returns the name of the type of the element given by some
 *	element name.
 *
 * Results:
 *	A standard Tcl result. Returns the type of the element in
 *	interp->result. If the identifier given doesn't represent
 *	an element, then an error message is left in interp->result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TypeOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget */
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;		/* Element name */
{
    Element *elemPtr;

    if (Blt_GetElement(graphPtr, argv[3], &elemPtr) != TCL_OK) {
	return TCL_ERROR;	/* Can't find named element */
    }
    Tcl_SetResult(interp, NameOfElementType(elemPtr), TCL_STATIC);
    return TCL_OK;
}

/*
 * Global routines:
 */

static Blt_OperSpec operSpecs[] =
{
    {"activate", 1, (Blt_OperProc) ActivateOper, 4, 0, "elemName ?index...?",},
    {"bar", 1, (Blt_OperProc) BarOper, 4, 0, "elemName ?option value?...",},
    {"cget", 2, (Blt_OperProc) CgetOper, 5, 5, "elemName option",},
    {"closest", 2, (Blt_OperProc) ClosestOper, 6, 0,
	"x y varName ?option value?... ?elemName?...",},
    {"configure", 2, (Blt_OperProc) ConfigureOper, 4, 0,
	"elemName ?options...?",},
    {"create", 2, (Blt_OperProc) CreateOper, 4, 0,
	"elemName ?option value?...",},
    {"deactivate", 3, (Blt_OperProc) DeactivateOper, 3, 0, "?pattern?...",},
    {"delete", 3, (Blt_OperProc) DeleteOper, 3, 0, "?pattern?...",},
    {"exists", 1, (Blt_OperProc) ExistsOper, 4, 4, "elemName",},
    {"line", 1, (Blt_OperProc) LineOper, 4, 0, "elemName ?option value?...",},
    {"names", 1, (Blt_OperProc) NamesOper, 3, 4, "?pattern?",},
    {"show", 1, (Blt_OperProc) ShowOper, 3, 4, "?elemList?",},
    {"type", 1, (Blt_OperProc) TypeOper, 4, 4, "elemName",},
};
static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);


/*
 *--------------------------------------------------------------
 *
 * Blt_ElementOper --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */
int
Blt_ElementOper(graphPtr, interp, argc, argv)
    Graph *graphPtr;		/* Graph widget record */
    Tcl_Interp *interp;
    int argc;			/* # arguments */
    char **argv;		/* Argument list */
{
    Blt_OperProc proc;
    int result;

    proc = Blt_LookupOperation(interp, numSpecs, operSpecs, BLT_OPER_ARG2,
	argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (graphPtr, interp, argc, argv);
    return (result);
}
#endif /* !NO_GRAPH */
