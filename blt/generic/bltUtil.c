/*
 * bltUtil.c --
 *
 *	This module implements utility procedures for the BLT
 *	toolkit.
 *
 * Copyright 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#include <X11/Xutil.h>

static int FillParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int flags));
static char *FillPrintProc _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltFillOption =
{
    FillParseProc, FillPrintProc, (ClientData)0
};

static int PadParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int offset));
static char *PadPrintProc _ANSI_ARGS_((ClientData clientData, Tk_Window tkwin,
	char *widgRec, int offset, Tcl_FreeProc **freeProcPtr));

Tk_CustomOption bltPadOption =
{
    PadParseProc, PadPrintProc, (ClientData)0
};

static int LengthParseProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *value, char *widgRec,
	int flags));
static char *LengthPrintProc _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltLengthOption =
{
    LengthParseProc, LengthPrintProc, (ClientData)0
};


static int DashesParseProc _ANSI_ARGS_((ClientData, Tcl_Interp *, Tk_Window,
	char *, char *, int));
static char *DashesPrintProc _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltDashesOption =
{
    DashesParseProc, DashesPrintProc, (ClientData)0
};

/*
 *----------------------------------------------------------------------
 *
 * Blt_NameOfFill --
 *
 *	Converts the integer representing the fill style into a string.
 *
 *----------------------------------------------------------------------
 */
char *
Blt_NameOfFill(fill)
    Fill fill;
{
    switch (fill) {
	case FILL_X:
	    return "x";
	case FILL_Y:
	    return "y";
	case FILL_NONE:
	    return "none";
	case FILL_BOTH:
	    return "both";
	default:
	    return "unknown fill value";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FillParseProc --
 *
 *	Converts the fill style string into its numeric representation.
 *
 *	Valid style strings are:
 *
 *	  "none"   Don't expand the window to fill the cubicle.
 * 	  "x"	   Expand only the window's width.
 *	  "y"	   Expand only the window's height.
 *	  "both"   Expand both the window's height and width.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
FillParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* Fill style string */
    char *widgRec;		/* Cubicle structure record */
    int offset;			/* Offset of style in record */
{
    Fill *fillPtr = (Fill *)(widgRec + offset);
    unsigned int length;
    char c;

    c = value[0];
    length = strlen(value);
    if ((c == 'n') && (strncmp(value, "none", length) == 0)) {
	*fillPtr = FILL_NONE;
    } else if ((c == 'x') && (strncmp(value, "x", length) == 0)) {
	*fillPtr = FILL_X;
    } else if ((c == 'y') && (strncmp(value, "y", length) == 0)) {
	*fillPtr = FILL_Y;
    } else if ((c == 'b') && (strncmp(value, "both", length) == 0)) {
	*fillPtr = FILL_BOTH;
    } else {
	Tcl_AppendResult(interp, "bad fill argument \"", value,
	    "\": should be \"none\", \"x\", \"y\", or \"both\"", (char *)NULL);
	return TCL_ERROR;
    }
    return (TCL_OK);
}

/*
 *----------------------------------------------------------------------
 *
 * FillPrintProc --
 *
 *	Returns the fill style string based upon the fill flags.
 *
 * Results:
 *	The fill style string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
FillPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Row/column structure record */
    int offset;			/* Offset of fill in Partition record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    Fill fill = *(Fill *)(widgRec + offset);

    return (Blt_NameOfFill(fill));
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetLength --
 *
 *	Like Tk_GetPixels, but doesn't allow negative pixel values.
 *
 * Results:
 *	The fill style string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_GetLength(interp, tkwin, string, valuePtr)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    char *string;
    int *valuePtr;
{
    int length;

    if (Tk_GetPixels(interp, tkwin, string, &length) != TCL_OK) {
	return TCL_ERROR;
    }
    if (length < 0) {
	Tcl_AppendResult(interp, "screen distance \"", string,
	    "\" must be non-negative value", (char *)NULL);
	return TCL_ERROR;
    }
    *valuePtr = length;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LengthParseProc --
 *
 *	Like TK_CONFIG_PIXELS, but adds an extra check for negative
 *	values.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
LengthParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window */
    char *value;		/* Pixel value string */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of pixel size in record */
{
    int *lengthPtr = (int *)(widgRec + offset);

    return (Blt_GetLength(interp, tkwin, value, lengthPtr));
}

/*
 *----------------------------------------------------------------------
 *
 * LengthPrintProc --
 *
 *	Returns the string representing the positive pixel size.
 *
 * Results:
 *	The pixel size string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
LengthPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Row/column structure record */
    int offset;			/* Offset of fill in Partition record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    int length = *(int *)(widgRec + offset);
    char *resultPtr;
    char string[200];

    sprintf(string, "%d", length);
    resultPtr = ckstrdup(string);
    if (resultPtr == NULL) {
	return (strerror(errno));
    }
    *freeProcPtr = (Tcl_FreeProc *)BLT_ckfree;
    return (resultPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * PadParseProc --
 *
 *	Convert a string into two pad values.  The string may be in one of
 *	the following forms:
 *
 *	    n    - n is a non-negative integer. This sets both
 *		   pad values to n.
 *	  {n m}  - both n and m are non-negative integers. side1
 *		   is set to n, side2 is set to m.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side Effects:
 *	The padding structure passed is updated with the new values.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PadParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window */
    char *value;		/* Pixel value string */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of pad in widget */
{
    Pad *padPtr = (Pad *)(widgRec + offset);
    int numElem;
    int pad, result;
    char **padArr;

    if (Tcl_SplitList(interp, value, &numElem, &padArr) != TCL_OK) {
	return TCL_ERROR;
    }
    result = TCL_ERROR;
    if ((numElem < 1) || (numElem > 2)) {
	Tcl_AppendResult(interp, "wrong # elements in padding list",
	    (char *)NULL);
	goto error;
    }
    if (Blt_GetLength(interp, tkwin, padArr[0], &pad) != TCL_OK) {
	goto error;
    }
    padPtr->side1 = pad;
    if ((numElem > 1) &&
	(Blt_GetLength(interp, tkwin, padArr[1], &pad) != TCL_OK)) {
	goto error;
    }
    padPtr->side2 = pad;
    result = TCL_OK;

  error:
    ckfree((char *)padArr);
    return (result);
}

/*
 *----------------------------------------------------------------------
 *
 * PadPrintProc --
 *
 *	Converts the two pad values into a Tcl list.  Each pad has two
 *	pixel values.  For vertical pads, they represent the top and bottom
 *	margins.  For horizontal pads, they're the left and right margins.
 *	All pad values are non-negative integers.
 *
 * Results:
 *	The padding list is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
PadPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Structure record */
    int offset;			/* Offset of pad in record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    Pad *padPtr = (Pad *)(widgRec + offset);
    char *result;
    char string[200];

    sprintf(string, "%d %d", padPtr->side1, padPtr->side2);
    result = ckstrdup(string);
    if (result == NULL) {
	return (strerror(errno));
    }
    *freeProcPtr = (Tcl_FreeProc *)BLT_ckfree;
    return (result);
}

/*
 *----------------------------------------------------------------------
 *
 * GetDashes --
 *
 *	Converts a Tcl list of dash values into a dash list ready for
 *	use with XSetDashes.
 *
 * 	A valid list dash values can have zero through 11 elements
 *	(PostScript limit).  Values must be between 1 and 255. Although
 *	a list of 0 (like the empty string) means no dashes.
 *
 * Results:
 *	A standard Tcl result. If the list represented a valid dash
 *	list TCL_OK is returned and *dashesPtr* will contain the
 *	valid dash list. Otherwise, TCL_ERROR is returned and
 *	interp->result will contain an error message.
 *
 *
 *----------------------------------------------------------------------
 */
static int
GetDashes(interp, string, dashesPtr)
    Tcl_Interp *interp;
    char *string;
    Dashes *dashesPtr;
{
    int numValues;
    char **strArr;
    long int value;
    register int i;

    numValues = 0;
    if ((string == NULL) || (*string == '\0')) {
	dashesPtr->numValues = 0;
	return TCL_OK;
    }
    if (Tcl_SplitList(interp, string, &numValues, &strArr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (numValues > 11) {	/* This is the postscript limit */
	Tcl_AppendResult(interp, "too many values in dash list", (char *)NULL);
	goto badDashList;
    }
    for (i = 0; i < numValues; i++) {
	if (Tcl_ExprLong(interp, strArr[i], &value) != TCL_OK) {
	    goto badDashList;
	}
	if ((value < 1) || (value > 255)) {
	    /* Backward compatibility: Allow list of 0 to turn off dashes */
	    if ((value == 0) && (numValues == 1)) {
		break;
	    }
	    Tcl_AppendResult(interp, "dash value \"", strArr[i],
		"\" is out of range", (char *)NULL);
	    goto badDashList;
	}
	dashesPtr->valueList[i] = (unsigned char)value;
    }
    dashesPtr->valueList[i] = '\0';
    dashesPtr->numValues = i;
    if (numValues > 0) {
	ckfree((char *)strArr);
    }
    return TCL_OK;

  badDashList:
    if (numValues > 0) {
	ckfree((char *)strArr);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DashesParseProc --
 *
 *	Convert the list of dash values into a dashes array.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	The Dashes structure is updated with the new dash list.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DashesParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* not used */
    char *value;		/* New dash value list */
    char *widgRec;		/* Widget record */
    int offset;			/* offset to Dashes structure */
{
    Dashes *dashesPtr = (Dashes *)(widgRec + offset);

    return (GetDashes(interp, value, dashesPtr));
}

/*
 *----------------------------------------------------------------------
 *
 * PositionPrintProc --
 *
 *	Convert the dashes array into a list of values.
 *
 * Results:
 *	The string representing the dashes returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
DashesPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Widget record */
    int offset;			/* offset of Dashes in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Dashes *dashesPtr = (Dashes *)(widgRec + offset);
    Tcl_DString dashList;
    register int i;
    char *result;
    char string[200];

    if (dashesPtr->numValues == 0) {
	return "";
    }
    Tcl_DStringInit(&dashList);
    for (i = 0; i < dashesPtr->numValues; i++) {
	sprintf(string, "%d", (int)dashesPtr->valueList[i]);
	Tcl_DStringAppendElement(&dashList, string);
    }
    *freeProcPtr = (Tcl_FreeProc *)BLT_ckfree;
    result = ckstrdup(Tcl_DStringValue(&dashList));
    Tcl_DStringFree(&dashList);
    return (result);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ConfigModified --
 *
 *      Given the specs and option argument names (terminated by a NULL),
 *	search  a Tk_ConfigSpec structure for any matching specs which
 *      has been reset.
 *
 * Results:
 *      Returns 1 if one of the options has changed, 0 otherwise.
 *
 *----------------------------------------------------------------------
 */
int
Blt_ConfigModified(specs, opt0, opt1, opt2, opt3, opt4, opt5, opt6, opt7,
    opt8, opt9)
    Tk_ConfigSpec specs[];
    char *opt0, *opt1, *opt2, *opt3, *opt4, *opt5, *opt6, *opt7, *opt8;
    char *opt9;
{
    register Tk_ConfigSpec *specPtr;
    char *argv[11];
    register char **argPtr;

    argv[0] = opt0;
    argv[1] = opt1;
    argv[2] = opt2;
    argv[3] = opt3;
    argv[4] = opt4;
    argv[5] = opt5;
    argv[6] = opt6;
    argv[7] = opt7;
    argv[8] = opt8;
    argv[9] = opt9;
    argv[10] = NULL;
    for (specPtr = specs; specPtr->type != TK_CONFIG_END; specPtr++) {
	for (argPtr = argv; *argPtr != NULL; argPtr++) {
	    if ((Tcl_StringMatch(specPtr->argvName, *argPtr)) &&
		(specPtr->specFlags & TK_CONFIG_OPTION_SPECIFIED)) {
		return 1;
	    }
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * OperSearch --
 *
 *      Performs a binary search on the array of command operation
 *      specifications to find a partial, anchored match for the
 *      given operation string.
 *
 * Results:
 *	If the string matches unambiguously the index of the specification
 *	in the array is returned.  If the string does not match, even
 *	as an abbreviation, any operation, -1 is returned.  If the string
 *	matches, but ambiguously -2 is returned.
 *
 *----------------------------------------------------------------------
 */
static int
OperSearch(specArr, numSpecs, operation)
    Blt_OperSpec specArr[];
    int numSpecs;
    char *operation;		/* Name of minor operation to search for */
{
    Blt_OperSpec *specPtr;
    char c;
    register int high, low, median;
    register int compare, length;

    low = 0;
    high = numSpecs - 1;
    c = operation[0];
    length = strlen(operation);
    while (low <= high) {
	median = (low + high) >> 1;
	specPtr = specArr + median;

	/* Test the first character */
	compare = c - specPtr->name[0];
	if (!compare) {
	    /* Now test the entire string */
	    compare = strncmp(operation, specPtr->name, length);
	    if ((compare == 0) && (length < specPtr->minChars)) {
		return -2;	/* Ambiguous operation name */
	    }
	}
	if (compare < 0) {
	    high = median - 1;
	} else if (compare > 0) {
	    low = median + 1;
	} else {
	    return (median);	/* Operation found. */
	}
    }
    return -1;			/* Can't find operation */
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_LookupOperation --
 *
 *      Find the command operation given a string name.  This is useful
 *      where a group of command operations have the same argument
 *      signature.
 *
 * Results:
 *      If found, a pointer to the procedure (function pointer) is
 *      returned.  Otherwise NULL is returned and an error message
 *      containing a list of the possible commands is returned in
 *      interp->result.
 *
 *----------------------------------------------------------------------
 */
Blt_OperProc
Blt_LookupOperation(interp, numSpecs, specArr, argIndex, numArgs, argArr)
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int numSpecs;		/* Number of specifications in array */
    Blt_OperSpec specArr[];	/* Operation specification array */
    Blt_OperIndex argIndex;	/* Index of the operation name argument */
    int numArgs;		/* Number of arguments in the argument vector.
				 * This includes any prefixed arguments */
    char *argArr[];		/* Argument vector */
{
    Blt_OperSpec *specPtr;
    char *string;
    register int i;
    register int specIndex;

    if (numArgs <= argIndex) {	/* No operation argument */
	Tcl_AppendResult(interp, "wrong # args: ", (char *)NULL);
      usage:
	Tcl_AppendResult(interp, "should be one of...",
	    (char *)NULL);
	for (specIndex = 0; specIndex < numSpecs; specIndex++) {
	    Tcl_AppendResult(interp, "\n  ", (char *)NULL);
	    for (i = 0; i < argIndex; i++) {
		Tcl_AppendResult(interp, argArr[i], " ", (char *)NULL);
	    }
	    specPtr = specArr + specIndex;
	    Tcl_AppendResult(interp, specPtr->name, " ", specPtr->usage,
		(char *)NULL);
	}
	return NULL;
    }
    string = argArr[argIndex];
    specIndex = OperSearch(specArr, numSpecs, string);

    if (specIndex == -2) {
	char c;
	int length;

	Tcl_AppendResult(interp, "ambiguous", (char *)NULL);
	if (argIndex > 2) {
	    Tcl_AppendResult(interp, " ", argArr[argIndex - 1], (char *)NULL);
	}
	Tcl_AppendResult(interp, " operation \"", string, "\" matches:",
	    (char *)NULL);

	c = string[0];
	length = strlen(string);
	for (specIndex = 0; specIndex < numSpecs; specIndex++) {
	    specPtr = specArr + specIndex;
	    if ((c == specPtr->name[0]) &&
		(strncmp(string, specPtr->name, length) == 0)) {
		Tcl_AppendResult(interp, " ", specPtr->name, (char *)NULL);
	    }
	}
	return NULL;

    } else if (specIndex == -1) {	/* Can't find operation, display help */
	Tcl_AppendResult(interp, "bad", (char *)NULL);
	if (argIndex > 2) {
	    Tcl_AppendResult(interp, " ", argArr[argIndex - 1], (char *)NULL);
	}
	Tcl_AppendResult(interp, " operation \"", string, "\": ", (char *)NULL);
	goto usage;
    }
    specPtr = specArr + specIndex;
    if ((numArgs < specPtr->minArgs) || ((specPtr->maxArgs > 0) &&
	    (numArgs > specPtr->maxArgs))) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", (char *)NULL);
	for (i = 0; i < argIndex; i++) {
	    Tcl_AppendResult(interp, argArr[i], " ", (char *)NULL);
	}
	Tcl_AppendResult(interp, specPtr->name, " ", specPtr->usage, "\"",
	    (char *)NULL);
	return NULL;
    }
    return (specPtr->proc);
}


#if (   (defined(__WIN32__) && !defined(BLT_COMPATIBLE_LIBS)) \
     || defined(TCL_MEM_DEBUG)) || !defined(HAVE_STRDUP) || USE_TCLALLOC
/*
 *----------------------------------------------------------------------
 *
 * ckstrdup --
 *
 *      Create a copy of the string from heap storage.
 *
 * Results:
 *      Returns a pointer to the need string copy.
 *
 *----------------------------------------------------------------------
 */
char *
ckstrdup(string)
    char *string;
{
    char *newPtr;

    newPtr = (char *)ckalloc(sizeof(char) * (strlen(string) + 1));
    if (newPtr != NULL) {
	strcpy(newPtr, string);
    }
    return (newPtr);
}
#endif /* TCL_MEM_DEBUG || !HAVE_STRDUP || USE_TCLALLOC */

#ifndef HAVE_STRCASECMP
static unsigned char lcase[] =
{
    '\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
    '\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
    '\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
    '\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
    '\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
    '\050', '\051', '\052', '\053', '\054', '\055', '\056', '\057',
    '\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
    '\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',
    '\100', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
    '\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
    '\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
    '\170', '\171', '\172', '\133', '\134', '\135', '\136', '\137',
    '\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
    '\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
    '\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
    '\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',
    '\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
    '\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
    '\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
    '\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
    '\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
    '\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
    '\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
    '\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
    '\300', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
    '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
    '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
    '\370', '\371', '\372', '\333', '\334', '\335', '\336', '\337',
    '\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
    '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
    '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
    '\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
};

/*
 *----------------------------------------------------------------------
 *
 * strcasecmp --
 *
 *      Compare two strings, disregarding case.
 *
 * Results:
 *      Returns a signed integer representing the following:
 *
 *	zero      - two strings are equal
 *	negative  - first string is less than second
 *	positive  - first string is greater than second
 *
 *----------------------------------------------------------------------
 */
int
strcasecmp(str1, str2)
    CONST char *str1;
    CONST char *str2;
{
    unsigned char *s = (unsigned char *)str1;
    unsigned char *t = (unsigned char *)str2;

    for ( /* empty */ ; (lcase[*s] == lcase[*t]); s++, t++) {
	if (*s == '\0') {
	    return 0;
	}
    }
    return (lcase[*s] - lcase[*t]);
}
#endif /*HAVE_STRCASECMP*/

/*
 *----------------------------------------------------------------------
 *
 * Blt_AppendDouble --
 *
 *      Convenience routine to append a double precision value to
 * 	interp->result as a separate element.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      List representation of double precision number is appended
 *	to interp->result.
 *
 *----------------------------------------------------------------------
 */
void
Blt_AppendDouble(interp, value)
    Tcl_Interp *interp;		/* Interpreter to append numeric string */
    double value;
{
    char string[TCL_DOUBLE_SPACE + 1];

    Tcl_PrintDouble(interp, value, string);
    Tcl_AppendElement(interp, string);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_AppendInt --
 *
 *      Convenience routine to append an integer value to interp->result
 *	as a separate list element.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      List representation of integer is appended to interp->result.
 *
 *----------------------------------------------------------------------
 */
void
Blt_AppendInt(interp, value)
    Tcl_Interp *interp;		/* Interpreter to append numeric string */
    int value;
{
    char string[TCL_DOUBLE_SPACE + 1];

    sprintf(string, "%d", value);
    Tcl_AppendElement(interp, string);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetUnsharedGC --
 *
 *      Like Tk_GetGC, but doesn't share the GC with any other widget.
 *	This is needed because the certain GC parameters (like dashes)
 *	can not be set via XCreateGC, therefore there is no way for
 *	Tk's hashing mechanism to recognize that two such GCs differ.
 *
 * Results:
 *      A new GC is returned.
 *
 *----------------------------------------------------------------------
 */
GC
Blt_GetUnsharedGC(tkwin, gcMask, valuePtr)
    Tk_Window tkwin;
    unsigned long gcMask;
    XGCValues *valuePtr;
{
    Drawable drawable, root;
    int isPixmap;
    int depth, defaultDepth;
    GC newGC;

    isPixmap = FALSE;
    defaultDepth = DefaultDepth(Tk_Display(tkwin), Tk_ScreenNumber(tkwin));
    root = RootWindow(Tk_Display(tkwin), Tk_ScreenNumber(tkwin));
    depth = Tk_Depth(tkwin);

    if (Tk_WindowId(tkwin) != None) {
	drawable = Tk_WindowId(tkwin);
    } else if (depth == defaultDepth) {
	drawable = root;
    } else {
	drawable = Tk_GetPixmap(Tk_Display(tkwin), root, 1, 1, depth);
	isPixmap = TRUE;
    }
    newGC = XCreateGC(Tk_Display(tkwin), drawable, gcMask, valuePtr);
    if (isPixmap) {
	Tk_FreePixmap(Tk_Display(tkwin), drawable);
    }
    return (newGC);
}

#if (   (defined(__WIN32__) && !defined(BLT_COMPATIBLE_LIBS)) \
     || defined(TCL_MEM_DEBUG)) || USE_TCLALLOC
void *
ckcalloc(n, s)
    size_t n;
    size_t s;
{
    void *p = ckalloc(n*s);

    memset(p, 0, n*s);
    return p;
}

#ifndef	ckrealloc
void *
ckrealloc(p, n)
    void *p;
    size_t n;
{
    void *q = ckalloc(n);

    memcpy(q, p, n);
    ckfree(p);
    return q;
}
#endif /* !ckrealloc */
#endif /* ((WIN32 &&!BLT_COMPATIBLE_LIBS) || TCL_MEM_DEBUG) || USE_TCLALLOC */

#ifdef _MSC_VER
#   undef HAVE_HYPOT
#endif

#ifdef NEED_HYPOT
double
bltHypot(x, y)
    double x, y;
{
    return sqrt(x*x + y*y);
}
#endif

#ifdef NEED_XDRAWARRAYS
/*
 *  draw some segments (uses XDrawLines)
 */
void
XDrawSegments(display, d, gc, segments, nsegments)
    Display *display;
    Drawable d;
    GC gc;
    XSegment *segments;
    int nsegments;
{
    int i;

    for (i = 0; i < nsegments; i++) {
	XDrawLine(display, d, gc,
	    segments[i].x1, segments[i].y1,
	    segments[i].x2, segments[i].y2);
    }
}

void
XDrawArcs(display, d, gc, arcs, narcs)
    Display *display;
    Drawable d;
    GC gc;
    XArc *arcs;
    int narcs;
{
    int i;

    for (i = 0; i < narcs; i++) {
	XDrawArc(display, d, gc,
	    arcs[i].x, arcs[i].y, arcs[i].width, arcs[i].height,
	    arcs[i].angle1, arcs[i].angle2);
    }
}

void
XFillArcs(display, d, gc, arcs, narcs)
    Display *display;
    Drawable d;
    GC gc;
    XArc *arcs;
    int narcs;
{
    int i;

    for (i = 0; i < narcs; i++) {
	XFillArc(display, d, gc,
	    arcs[i].x, arcs[i].y, arcs[i].width, arcs[i].height,
	    arcs[i].angle1, arcs[i].angle2);
    }
}

void
XDrawRectangles(display, d, gc, rectangles, nrectangles)
    Display *display;
    Drawable d;
    GC gc;
    XRectangle *rectangles;
    int nrectangles;
{
    int i;

    for (i = 0; i < nrectangles; i++) {
	XDrawRectangle(display, d, gc,
	    rectangles[i].x, rectangles[i].y,
	    rectangles[i].width, rectangles[i].height);
    }
}
#endif /* NEED_XDRAWARRAYS */

#ifdef __WIN32__
void
XSetDashes(display, gc, dash_offset, dash_list, n)
    Display *display;
    GC gc;
    int dash_offset;
    _Xconst char *dash_list;
    int n;
{
    /* empty */
}

static unsigned long
Get_Pixel(image, x, y)
    XImage *image;
    int x, y;
{
    char *destPtr = &(image->data[(y * image->bytes_per_line)
    	+ (x * (image->bits_per_pixel >> 3))]);
    return RGB(destPtr[2],destPtr[1],destPtr[0]);
}

XImage *
blt_XCreateImage(display, visual, depth, format, offset, data, width, height,
	bitmap_pad, bytes_per_line)
    Display* display;
    Visual* visual;
    unsigned int depth;
    int format;
    int offset;
    char* data;
    unsigned int width;
    unsigned int height;
    int bitmap_pad;
    int bytes_per_line;
{
#undef XCreateImage
    XImage *imagePtr =
	XCreateImage(display, visual, depth, format, offset, data, width,
	    height, bitmap_pad, bytes_per_line);

    imagePtr->f.get_pixel =
        (unsigned long (*)(struct _XImage *, int, int))Get_Pixel;
    return imagePtr;
}

#ifdef _BLT_WDEBUG
void
blt_debug(fmt, ...)
    const char *fmt;
{
    char buffer[512];
    extern Tcl_Interp *blt_winterp(void);
    Tcl_Interp *interp = blt_winterp();
    va_list ap;

#if _BLT_WDEBUG==1
    strcpy(buffer, "puts -nonewline stderr {");
#else /* _BLT_WDEBUG != 1 */
    typedef struct ConsoleInfo {
	Tcl_Interp *consoleInterp;
	Tcl_Interp *interp;
    } ConsoleInfo;
    Tcl_CmdInfo info;

    if (Tcl_GetCommandInfo(interp, "console", &info)) {
	interp = ((ConsoleInfo*)info.clientData)->consoleInterp;
    }
    strcpy(buffer,"tkConsoleOutput stderr {");
#endif /* _BLT_WDEBUG != 1 */
    va_start(ap,fmt);
    vsprintf(buffer+strlen(buffer), fmt, ap);
    va_end(ap);
    strcat(buffer, "};update idletasks");
    Tcl_Preserve((ClientData)interp);
    Tcl_Eval(interp,buffer);
    Tcl_Release((ClientData)interp);
}
#endif /* _BLT_WDEBUG */
#endif /* __WIN32__ */
