/*
 * bltVector.c --
 *
 *	This module implements simple vectors.
 *
 * Copyright 1995-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef	NO_VECTOR

#include <ctype.h>

#define VECTOR_VERSION "8.0"

#define TRACE_ALL	(TCL_TRACE_WRITES | TCL_TRACE_READS | TCL_TRACE_UNSETS)
#define VECTOR_MAGIC	((unsigned int) 0x46170277)
#define DEF_ARRAY_SIZE  64
#define MAX_INDEX	-1
#define MIN_INDEX	-2

/* These defines allow parsing of different types of indices */
#define ALLOW_SPECIAL	(1<<0)	/* Recognize "min", "max", and "++end" as
				 * valid indices */
#define ALLOW_COLON	(1<<1)	/* Also recognize a range of indices separated
				 * by a colon */
#define CHECK_RANGE	(1<<2)	/* Verify that the specified index or range
				 * of indices are within limits */

/*
 * Vector --
 *
 *	A vector is simple array of double precision values.  It can
 *	be queried or modified by either a Tcl command and array
 *	variable which are created when the array is created.  The
 *	memory storage for the array of values is initially static,
 *	but space is malloc-ed if more is necessary.
 *
 *	Vectors can be modified from C code too. Furthermore, the same
 *	vector can be used in multiple instances (for example, by two
 *	different graph widgets).  All the clients of the vector will
 *	share the data associated with the vector.  Therefore, when a
 *	client wants to use a vector, it allocates a vector
 *	identifier.  A vector identifier uniquely identifies the
 *	client.  The client then uses this id to specify a callback
 *	routine to be invoked whenever the vector is modified or
 *	destroyed.  Whenever the vector is updated or destroyed, each
 *	client is notified of the change by their callback routine.
 *
 */
typedef struct {
    /*
     * If you change these fields, make sure you change the definition
     * of Blt_Vector in bltInt.h and blt.h too.
     */
    double *valueArr;		/* Array of values (malloc-ed) */
    int numValues;		/* Number of values in the array */
    int arraySize;		/* Size of the allocated space */
    double min, max;		/* Minimum and maximum values in the vector */
    int reserved;

    /* The following fields are local to this module  */

    Tcl_FreeProc *freeProc;	/* Address of procedure to call to release
				 * storage for the value array, Optionally
				 * can be one of the following: TCL_STATIC,
				 * TCL_DYNAMIC, or TCL_VOLATILE. */
    char *arrayName;		/* The name of the variable representing
				 * the vector */
    int global;			/* Indicates if Tcl array variable is
				 * global or not. Will be set to
				 * TCL_GLOBAL_ONLY if variable is
				 * global or 0 otherwise. */
    int offset;			/* Offset from zero of the starting
				 * index for array variable */

    Tcl_Command cmdToken;	/* Token for vector's Tcl command. */

    double staticSpace[DEF_ARRAY_SIZE];
    Blt_List clients;		/* List of clients using this vector */
    Tcl_Interp *interp;		/* Interpreter associated with the vector */
    int flags;			/* Notification flags. See definitions below */

} Vector;

#define NOTIFY_UPDATED		((int)BLT_VECTOR_NOTIFY_UPDATE)
#define NOTIFY_DESTROYED	((int)BLT_VECTOR_NOTIFY_DESTROY)

#define NOTIFY_NEVER		(1<<2)	/* Never notify clients of updates to
					* the vector */
#define NOTIFY_ALWAYS		(1<<3)	/* Notify clients after each update
					* of the vector is made */
#define NOTIFY_WHENIDLE		(1<<4)	/* Notify clients at the next idle point
					* that the vector has been updated. */
#define NOTIFY_PENDING		(1<<5)	/* A do-when-idle notification of the
					* vector's clients is pending. */
#define NOTIFY_NOW		(1<<6)	/* Notify clients of changes once
					 * immediately */

#define NOTIFY_WHEN_MASK	(NOTIFY_NEVER|NOTIFY_ALWAYS|NOTIFY_WHENIDLE)

/*
 * ClientInfo --
 *
 *	A vector can be shared by several applications.  Each client
 *	of the vector allocates memory for this structure.  It
 *	contains a pointer to the master information pertaining to the
 *	vector.  It also contains pointers to a routine the call when
 *	the vector changes and data to pass with this routine.
 */
typedef struct {
    unsigned int magic;		/* Magic value designating whether this
				 * really is a vector token or not */
    Vector *master;		/* Pointer to the master record of the vector
				 * If NULL, indicates that vector has been
				 * destroyed. */
    Blt_VectorChangedProc *proc;/* Routine to call when the contents of
				 * the vector change */
    ClientData clientData;	/* Data to pass on ChangedProc call */

} ClientInfo;


static Tcl_HashTable vectorTable;
static int initialized = 0;

static char *VariableProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, char *part1, char *part2, int flags));
static int VectorInstCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	int argc, char **argv));

#if defined(__WIN32__) && defined(_BC)
/* ec: I prototyped these static functions to avoid to generate too many
 *     warning messages by the Borland Compiler and pass the compilation.
 */
static Vector *FindVector _ANSI_ARGS_((Tcl_Interp *interp, char *vecName,
	unsigned int flags));
static void FreeVector _ANSI_ARGS_((Vector *vPtr));
static int GetIndex _ANSI_ARGS_((Vector *vPtr, char *string, int *indexPtr,
	int flags));
static char *NameOfVector _ANSI_ARGS_((Vector *vPtr));
static void NotifyClients _ANSI_ARGS_((ClientData clientData));
static int GlobalVariable _ANSI_ARGS_((Tcl_Interp *interp, char *name));
static intGetIndex2 _ANSI_ARGS_((Vector *vPtr, char *string, int *firstPtr,
	int *lastPtr, int flags));
static int ResizeVector _ANSI_ARGS_((Vector *vPtr, int size));
#endif /* __WIN32__ && _BC */


INLINE static char *
NameOfVector(vPtr) 
    Vector *vPtr;
{
    return(Tcl_GetCommandName(vPtr->interp, vPtr->cmdToken));
}

/*
 * ----------------------------------------------------------------------
 *
 * GlobalVariable --
 *
 *	Determines if a variable is global or not.  This is needed
 *	to determine how to set, unset and reset traces on a variable.
 *	There may be a local variable by the same name.  In that case
 *	we want to reset the trace on the global variable only.
 *
 *      Note: This routine uses private information internal to the
 *	      interpreter.  The following structure must be kept
 *	      up-to-date with the structure Interp in tclInt.h. This
 *	      ought to be a library-interface routine. I really don't
 *	      want to do something like the following:
 *
 *            result = Tcl_VarEval(interp, "info globals ", arrayName,
 * 			 (char *)NULL);
 *            if (result == NULL) {
 * 	          return NULL;
 *            }
 *            if (strcmp(interp->result, "1") == 0) {
 * 	          global = TCL_GLOBAL_ONLY;
 *            }
 *            Tcl_ResetResult(interp);
 *
 *	mike: Now that Tcl is aware of namespaces, we do not need to
 *	access an interpreter's private information any longer, as it
 *	is available through an official API (well, not in 8.0, but
 *	in 8.1; Brian Lewis suggested to use Tcl_FindNamespaceVar(),
 *	so I guess it's safe to use it even for 8.0).
 *
 * Results:
 *	Return 1 if the variable is global and 0 if it is not.  Note
 *	that the variable need not exist first.
 *
 * ----------------------------------------------------------------------
 */

#if defined(USE_TCL_NAMESPACES) || TCL80B2_OR_BETTER

static int
GlobalVariable(interp, name)
    Tcl_Interp *interp;
    char *name;
{
    Tcl_Var varToken;

    varToken = Tcl_FindNamespaceVar(interp, name, NULL, TCL_GLOBAL_ONLY);
    return (varToken != (Tcl_Var)NULL);
}

#else /* !USE_TCL_NAMESPACES && !TCL80B2_OR_BETTER */

static int
GlobalVariable(interp, name)
    Tcl_Interp *interp;
    char *name;
{
    struct Interp *iPtr = (struct Interp *) interp;

    return (Tcl_FindHashEntry(&(iPtr->globalTable), name) != NULL);
}

#endif /* !USE_TCL_NAMESPACES && !TCL80B2_OR_BETTER */

/*
 * ----------------------------------------------------------------------
 *
 * GetIndex --
 *
 *	Converts the string representing an index in the vector, to
 *	its numeric value.  A valid index may be an numeric string of
 *	the string "end" (indicating the last element in the string).
 *
 * Results:
 *	A standard Tcl result.  If the string is a valid index, TCL_OK
 *	is returned.  Otherwise TCL_ERROR is returned and interp->result
 *	will contain an error message.
 *
 * ----------------------------------------------------------------------
 */
static int
GetIndex(vPtr, string, indexPtr, flags)
    Vector *vPtr;
    char *string;
    int *indexPtr;
    int flags;
{
    char c;

    c = string[0];
    if ((c == 'e') && (strcmp(string, "end") == 0)) {
	*indexPtr = vPtr->numValues - 1;
    } else if ((flags & ALLOW_SPECIAL) && (c == '+') &&
	(strcmp(string, "++end") == 0)) {
	*indexPtr = vPtr->numValues;
    } else if ((flags & ALLOW_SPECIAL) && (c == 'm') &&
	strcmp(string, "min") == 0) {
	*indexPtr = MIN_INDEX;
    } else if ((flags & ALLOW_SPECIAL) && (c == 'm') &&
	strcmp(string, "max") == 0) {
	*indexPtr = MAX_INDEX;
    } else {
	long int value;

	if (Tcl_ExprLong(vPtr->interp, string, &value) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_ResetResult(vPtr->interp);
	value -= vPtr->offset;	/* Fix the value by the current index offset */
	if ((value < 0) || ((flags & CHECK_RANGE) &&
		(value >= vPtr->numValues))) {
	    Tcl_AppendResult(vPtr->interp, "index \"", string,
		"\" is out of range", (char *)NULL);
	    return TCL_ERROR;
	}
	*indexPtr = (int)value;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetIndex2 --
 *
 *	Converts the string representing an index in the vector, to
 *	its numeric value.  A valid index may be an numeric string of
 *	the string "end" (indicating the last element in the string).
 *
 * Results:
 *	A standard Tcl result.  If the string is a valid index, TCL_OK
 *	is returned.  Otherwise TCL_ERROR is returned and interp->result
 *	will contain an error message.
 *
 * ----------------------------------------------------------------------
 */
static int
GetIndex2(vPtr, string, firstPtr, lastPtr, flags)
    Vector *vPtr;
    char *string;
    int *firstPtr, *lastPtr;
    int flags;
{
    char *colon, *tmpString;

    colon = NULL;
    /* mike: make a copy of string, so that we can safely change it. */
    tmpString = ckstrdup(string);
    if (flags & ALLOW_COLON) {
	colon = strchr(tmpString, ':');
    }
    if (colon != NULL) {
	flags &= ~ALLOW_SPECIAL; /* Don't allow special indices when
				  * processing ranges */
	if (tmpString == colon) {
	    *firstPtr = 0;
	} else {
	    int result;

	    *colon = '\0';
	    result = GetIndex(vPtr, tmpString, firstPtr, flags);
	    *colon = ':';
	    if (result != TCL_OK) {
	        ckfree(tmpString);
		return TCL_ERROR;
	    }
	}
	if (*(colon+1) == '\0') {
	    *lastPtr = vPtr->numValues - 1;
	} else {
	    if(GetIndex(vPtr, colon+1, lastPtr, flags) != TCL_OK) {
	        ckfree(tmpString);
		return TCL_ERROR;
	    }
	}
	if ((*firstPtr > *lastPtr) && (*firstPtr != 0)) {
	    Tcl_AppendResult(vPtr->interp, "invalid range \"",
		     tmpString, "\" (first > last)", (char *)NULL);
	    ckfree(tmpString);
	    return TCL_ERROR;
	}
    } else {
	if(GetIndex(vPtr, tmpString, firstPtr, flags) != TCL_OK) {
	    ckfree(tmpString);
	    return TCL_ERROR;
	}
	*lastPtr = *firstPtr;
    }
    ckfree(tmpString);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * FindLimits --
 *
 *	Determines the minimum and maximum values in the vector.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The min and max fields of the vector are updated.
 *
 * ----------------------------------------------------------------------
 */
static void
FindLimits(vPtr)
    Vector *vPtr;
{
    register double min, max;

    min = max = 0.0;
    if (vPtr->numValues > 0) {
	register int i;
	register double *valuePtr;

	min = max = vPtr->valueArr[0];
	valuePtr = vPtr->valueArr + 1;
	for (i = 1; i < vPtr->numValues; i++) {
	    if (min > *valuePtr) {
		min = *valuePtr;
	    } else if (max < *valuePtr) {
		max = *valuePtr;
	    }
	    valuePtr++;
	}
    }
    vPtr->min = min, vPtr->max = max;
}

/*
 * ----------------------------------------------------------------------
 *
 * NotifyClients --
 *
 *	Notifies each client of the vector that the vector has changed
 *	(updated or destroyed) by calling the provided function back.
 *	The function pointer may be NULL, in that case the client is
 *	not notified.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The results depend upon what actions the client callbacks
 *	take.
 *
 * ----------------------------------------------------------------------
 */
static void
NotifyClients(clientData)
    ClientData clientData;
{
    Vector *vPtr = (Vector *)clientData;
    Blt_ListItem *itemPtr;
    ClientInfo *clientPtr;
    Blt_VectorNotify notify;

    notify = (vPtr->flags & NOTIFY_DESTROYED)
	? BLT_VECTOR_NOTIFY_DESTROY : BLT_VECTOR_NOTIFY_UPDATE;
    vPtr->flags &= ~(NOTIFY_UPDATED | NOTIFY_DESTROYED | NOTIFY_PENDING);

    for (itemPtr = Blt_FirstListItem(&(vPtr->clients)); itemPtr != NULL;
	itemPtr = Blt_NextItem(itemPtr)) {
	clientPtr = (ClientInfo *)Blt_GetItemValue(itemPtr);
	if (clientPtr->proc != NULL) {
	    (*clientPtr->proc) (vPtr->interp, clientPtr->clientData, notify);
	}
    }
    /*
     * Some clients may not handle the "destroy" callback properly
     * (they should call Blt_FreeVectorId to release the client
     * identifier), so we need to mark any remaining clients to
     * indicate that the master vector has gone away.
     */
    if (notify == BLT_VECTOR_NOTIFY_DESTROY) {
	for (itemPtr = Blt_FirstListItem(&(vPtr->clients)); itemPtr != NULL;
	    itemPtr = Blt_NextItem(itemPtr)) {
	    clientPtr = (ClientInfo *)Blt_GetItemValue(itemPtr);
	    clientPtr->master = NULL;
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * UpdateClients --
 *
 *	Notifies each client of the vector that the vector has changed
 *	(updated or destroyed) by calling the provided function back.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The results depend upon what actions the client callbacks
 *	take.
 *
 * ----------------------------------------------------------------------
 */
static void
UpdateClients(vPtr)
    Vector *vPtr;
{
    if (vPtr->flags & NOTIFY_NEVER) {
	return;
    }
    vPtr->flags |= NOTIFY_UPDATED;
    if (vPtr->flags & NOTIFY_ALWAYS) {
	NotifyClients((ClientData)vPtr);
	return;
    }
    if (!(vPtr->flags & NOTIFY_PENDING)) {
	vPtr->flags |= NOTIFY_PENDING;
	Tcl_DoWhenIdle((Tcl_IdleProc*)NotifyClients, (ClientData)vPtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * FlushCache --
 *
 *	Unsets all the elements of the Tcl array variable associated
 *	with the vector, freeing memory in associated with the
 *	variable.  This includes both the hash table and the hash
 *	keys.  The down side is that this effectively flushes the
 *	caching of vector elements in the array.  This means that the
 *	subsequent reads of the array will require a decimal to string
 *	conversion.
 *
 *	This is needed when the vector changes its values, making
 *	the array variable out-of-sync.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All elements of array variable (except one) are unset, freeing
 *	the memory associated with the variable.
 *
 * ----------------------------------------------------------------------
 */
static void
FlushCache(vPtr)
    Vector *vPtr;
{
    /*
     * Turn off the trace temporarily so that we can unset all the
     * elements in the array variable.
     */
    Tcl_UntraceVar2(vPtr->interp, vPtr->arrayName, (char *)NULL,
	TRACE_ALL | vPtr->global, VariableProc, (ClientData)vPtr);

    /* Unset the entire array */
    Tcl_UnsetVar2(vPtr->interp, vPtr->arrayName, (char *)NULL, vPtr->global);

    /* Restore the "end" index by default and the trace on the entire array */
    Tcl_SetVar2(vPtr->interp, vPtr->arrayName, "end", "", vPtr->global);
    Tcl_TraceVar2(vPtr->interp, vPtr->arrayName, (char *)NULL,
	TRACE_ALL | vPtr->global, VariableProc, (ClientData)vPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * FindVector --
 *
 *	Searches for the vector associated with the name given.
 *
 * Results:
 *	Returns a pointer to the vector if found, otherwise NULL.
 *	If the name is not associated with a vector and the
 *	TCL_LEAVE_ERR_MSG flag is set, and interp->result will
 *	contain an error message.
 *
 * ----------------------------------------------------------------------
 */
static Vector *
FindVector(interp, vecName, flags)
    Tcl_Interp *interp;
    char *vecName;
    unsigned int flags;
{
    Tcl_HashEntry *hPtr;
    Vector *vPtr;
    Tcl_CmdInfo info;

    if (!Tcl_GetCommandInfo(interp, vecName, &info)) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    Tcl_AppendResult(interp, "can't find a vector \"", vecName, "\"",
		(char *)NULL);
	}
	return NULL;
    }
    vPtr = (Vector *)info.clientData;
    hPtr = Tcl_FindHashEntry(&vectorTable, (char *)vPtr);
    if (hPtr == NULL) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    Tcl_AppendResult(interp, "\"", vecName, "\" is not a vector", 
		(char *)NULL);
	}
	return NULL;
    }
    return (vPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * ResizeVector --
 *
 *	Resizes the vector to the new size.
 *
 * Results:
 *	A standard Tcl result.  If the reallocation is successful,
 *	TCL_OK is returned, otherwise TCL_ERROR.
 *
 * Side effects:
 *	Memory for the array is reallocated.
 *
 * ----------------------------------------------------------------------
 */
static int
ResizeVector(vPtr, size)
    Vector *vPtr;
    int size;
{
    if (size > DEF_ARRAY_SIZE) {
	int newSize;

	newSize = DEF_ARRAY_SIZE;
	while (newSize < size) {
	    newSize += newSize;
	}

	/* Bigger or smaller reallocate the buffer */

	if (vPtr->arraySize != newSize) {
	    double *newArr;

	    if (vPtr->freeProc == TCL_STATIC) {
		newArr = (double *)ckcalloc(sizeof(double), newSize);
		if (vPtr->valueArr != NULL) {

		    /* Copy the contents of the (old) static array */

		    memcpy((char *)newArr, (char *)vPtr->valueArr,
			sizeof(double) * BLT_MIN(newSize, vPtr->numValues));
		}
	    } else {
		newArr = (double *)ckrealloc((char *)vPtr->valueArr,
		    sizeof(double) * newSize);
	    }
	    if (newArr == NULL) {
		return TCL_ERROR;
	    }
	    vPtr->valueArr = newArr;
	    vPtr->freeProc = TCL_DYNAMIC;
	    vPtr->arraySize = newSize;
	}
    } else {
	if ((vPtr->valueArr != vPtr->staticSpace) &&
		(vPtr->freeProc != TCL_STATIC)) {
	    memcpy((char *)vPtr->staticSpace, (char *)vPtr->valueArr,
		sizeof(double) * BLT_MIN(size, vPtr->numValues));
	    if (vPtr->freeProc == TCL_DYNAMIC) {
		ckfree((char *)vPtr->valueArr);
	    } else {
		(*vPtr->freeProc) ((char *)vPtr->valueArr);
	    }
	    vPtr->valueArr = vPtr->staticSpace;
	    vPtr->arraySize = DEF_ARRAY_SIZE;
	    vPtr->freeProc = TCL_STATIC;
	}
    }
    vPtr->numValues = size;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * DestroyVector --
 *
 *	Removes the memory and frees resources associated with the
 *	vector.
 *
 *	o Removes the trace and the Tcl array variable and unsets
 *	  the variable.
 *	o Notifies clients of the vector that the vector is being
 *	  destroyed.
 *	o Removes any clients that are left after notification.
 *	o Frees the memory (if necessary) allocated for the array.
 *	o Removes the entry from the hash table of vectors.
 *	o Frees the memory allocated for the name.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *
 * ----------------------------------------------------------------------
 */
static void
DestroyVector(clientData)
    ClientData clientData;
{
    Vector *vPtr = (Vector *)clientData;
    Tcl_HashEntry *hPtr;
    Blt_ListItem *itemPtr;
    ClientInfo *clientPtr;

    Tcl_UntraceVar2(vPtr->interp, vPtr->arrayName, (char *)NULL,
	TRACE_ALL | vPtr->global, VariableProc, (ClientData)vPtr);
    Tcl_UnsetVar2(vPtr->interp, vPtr->arrayName, (char *)NULL, vPtr->global);

    vPtr->numValues = 0;

    /* Immediately notify clients that vector is going away */
    if (vPtr->flags & NOTIFY_PENDING) {
	vPtr->flags &= ~NOTIFY_PENDING;
	Tcl_CancelIdleCall((Tcl_IdleProc*)NotifyClients, (ClientData)vPtr);
    }
    vPtr->flags |= NOTIFY_DESTROYED;
    NotifyClients((ClientData)vPtr);

    for (itemPtr = Blt_FirstListItem(&(vPtr->clients)); itemPtr != NULL;
	itemPtr = Blt_NextItem(itemPtr)) {
	clientPtr = (ClientInfo *)Blt_GetItemValue(itemPtr);
	ckfree((char *)clientPtr);
    }
    Blt_ResetList(&(vPtr->clients));
    if ((vPtr->valueArr != vPtr->staticSpace) &&
	(vPtr->freeProc != TCL_STATIC)) {
	if (vPtr->freeProc == TCL_DYNAMIC) {
	    ckfree((char *)vPtr->valueArr);
	} else {
	    (*vPtr->freeProc) ((char *)vPtr->valueArr);
	}
    }
    hPtr = Tcl_FindHashEntry(&vectorTable, (char *)vPtr);
    if (hPtr != NULL) {
	Tcl_DeleteHashEntry(hPtr);
    }
    if (vPtr->arrayName != NULL) {
	ckfree((char *)vPtr->arrayName);
    }
    ckfree((char *)vPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * FreeVector --
 *
 *	Deletes the command associated with the vector.  As a side
 *	effect, a Tcl_DeleteCmdProc routine is called to free up
 *	resource with the vector.
 *
 * Results:
 *	A standard Tcl result.  If the reallocation is successful,
 *	TCL_OK is returned, otherwise TCL_ERROR.
 *
 * Side effects:
 *	Memory for the array is reallocated.
 *
 * ----------------------------------------------------------------------
 */
static void
FreeVector(vPtr)
    Vector *vPtr;
{
    /*
     * Deleting the command associated with the vector will trigger a
     * cleanup of the resources used by it.
     */
    Tcl_DeleteCommand(vPtr->interp, NameOfVector(vPtr));
}

/*
 * ----------------------------------------------------------------------
 *
 * CreateVector --
 *
 *	Creates a vector structure and the following items:
 *
 *	o Tcl command
 *	o Tcl array variable and establishes traces on the variable
 *	o Adds a  new entry in the vector hash table
 *
 * Results:
 *	A pointer to the new vector structure.  If an error occurred
 *	NULL is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	A new Tcl command and array variable is added to the
 *	interpreter.
 *
 * ----------------------------------------------------------------------
 */
static Vector *
CreateVector(interp, varName, newPtr)
    Tcl_Interp *interp;
    char *varName;
    int *newPtr;
{
    Tcl_CmdInfo info;
    Tcl_HashEntry *hPtr;
    Vector *vPtr;
    int dummy, exists;

    vPtr = NULL;		/* Initialize vector pointer */
    *newPtr = 1;

    /* 
     * See if a command *vecName* exists, and check if its clientData
     * really represents a vector.  
     */
    if (Tcl_GetCommandInfo(interp, varName, &info)) {
	vPtr = (Vector *)info.clientData;

	hPtr = Tcl_FindHashEntry(&vectorTable, (char *)vPtr);
	if (hPtr != NULL) {
	    *newPtr = 0;
	    return (vPtr);
	} else {
	    Tcl_AppendResult(interp, "command \"", varName, "\" already exists",
			     (char *)NULL);
	    return NULL;
	}
    }

    /*
     * There's no way in the current C API to determine if a Tcl
     * variable already exists.  Can't use Tcl_GetVar because the
     * variable may be an array.  We must invoke "info" instead.
     */

    if (Tcl_VarEval(interp, "info exists ", varName, (char *)NULL) != TCL_OK) {
	return NULL;
    }
    exists = ((Tcl_GetStringResult(interp))[0] == '1');
    Tcl_ResetResult(interp);
    if (exists) {
	Tcl_AppendResult(interp, "variable \"", varName, "\" already exists",
	    (char *)NULL);
	return NULL;
    }
    vPtr = (Vector *)ckcalloc(1, sizeof(Vector));
    if (vPtr == NULL) {
	Panic("can't allocate vector structure");
    }
    vPtr->flags = NOTIFY_WHENIDLE;
    vPtr->freeProc = TCL_STATIC;
    vPtr->arrayName = ckstrdup(varName);
    vPtr->valueArr = vPtr->staticSpace;
    vPtr->arraySize = DEF_ARRAY_SIZE;
    vPtr->interp = interp;
    Blt_InitList(&(vPtr->clients), TCL_ONE_WORD_KEYS);
    hPtr = Tcl_CreateHashEntry(&vectorTable, (char *)vPtr, &dummy);
    Tcl_SetHashValue(hPtr, (char *)vPtr);

    vPtr->cmdToken = Tcl_CreateCommand(interp, varName,
        (Tcl_CmdProc*)VectorInstCmd,
	(ClientData)vPtr, (Tcl_CmdDeleteProc*)DestroyVector);

    /*
     * The index "end" designates the last element in the array.
     * We don't know whether this is a global variable or not until
     * the variable is set.
     */
    if (Tcl_SetVar2(interp, varName, "end", "", TCL_LEAVE_ERR_MSG) == NULL) {
	Tcl_DeleteHashEntry(hPtr);
	if (vPtr != NULL) {
	    ckfree((char *)vPtr);
	}
	return NULL;
    }
    /* Determine if the variable is global or not */
    if (GlobalVariable(interp, varName)) {
	vPtr->global = TCL_GLOBAL_ONLY;
    }
    /* Trace the array on reads, writes, and unsets */
    Tcl_TraceVar2(interp, varName, (char *)NULL, TRACE_ALL | vPtr->global,
	VariableProc, (ClientData)vPtr);
    return (vPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * VariableProc --
 *
 * Results:
 *	Always returns NULL.  Only called from a variable trace.
 *
 * Side effects:
 *
 * ----------------------------------------------------------------------
 */
static char *
VariableProc(clientData, interp, part1, part2, flags)
    ClientData clientData;	/* File output information. */
    Tcl_Interp *interp;
    char *part1, *part2;
    int flags;
{
    Vector *vPtr = (Vector *)clientData;
    int first, last;		/* Indices */
    char string[TCL_DOUBLE_SPACE + 1];
    if (part2 == NULL) {
	FreeVector(vPtr);	/* Unsetting the entire array. */
	return NULL;
    }
    if (GetIndex2(vPtr, part2, &first, &last,
	    (ALLOW_SPECIAL | CHECK_RANGE | ALLOW_COLON)) != TCL_OK) {
	static char errMsg[200];

	sprintf(errMsg, Tcl_GetStringResult(vPtr->interp));
	return (errMsg);
    }
    if (flags & TCL_TRACE_WRITES) {
	double value;
	register int i;
	char *newValue;

	if (first < 0) {
	    return NULL;	/* Tried to set "min" or "max" */
	}
	newValue = Tcl_GetVar2(interp, part1, part2, vPtr->global);
	if (newValue == NULL) {
	    return "can't read current vector value";
	}
	if (Tcl_ExprDouble(interp, newValue, &value) != TCL_OK) {
	    if (first == last) {
		/* Reset the array element to its old value on errors */
		Tcl_PrintDouble(interp, vPtr->valueArr[first], string);
		Tcl_SetVar2(interp, part1, part2, string, vPtr->global);
	    }
	    return "bad value for vector element";
	}
	if (first == vPtr->numValues) {
	    if (ResizeVector(vPtr, vPtr->numValues + 1) != TCL_OK) {
		return "error resizing vector";
	    }
	}
	/* Set possibly an entire range of values */
	for(i = first; i <= last; i++) {
	    vPtr->valueArr[i] = value;
	}
	FindLimits(vPtr);
    } else if (flags & TCL_TRACE_READS) {
	double value;

	if (first == last) {
	    if (first == vPtr->numValues) {
		return NULL;	/* index was "++end" */
	    }
	    if (first == MIN_INDEX) {
		value = vPtr->min;
	    } else if (first == MAX_INDEX) {
		value = vPtr->max;
	    } else {
		value = vPtr->valueArr[first];
	    }
	    Tcl_PrintDouble(interp, value, string);
	    if (Tcl_SetVar2(interp, part1, part2, string, 
			vPtr->global) == NULL) {
		return "error setting vector element (read)";
	    }
	} else {
	    Tcl_DString buffer;
	    char *result;
	    register int i;

	    Tcl_DStringInit(&buffer);
	    for (i = first; i <= last; i++) {
		Tcl_PrintDouble(interp, vPtr->valueArr[i], string);
		Tcl_DStringAppendElement(&buffer, string);
	    }
	    result = Tcl_SetVar2(interp, part1, part2, 
		Tcl_DStringValue(&buffer), vPtr->global);
	    Tcl_DStringFree(&buffer);
	    if (result == NULL) {
		return "error setting vector element (read)";
	    }
	}
    } else if (flags & TCL_TRACE_UNSETS) {
	register int i, j;

	if (first == vPtr->numValues) {
	    return "can't unset array element \"++end\"";
	} else if (first == MIN_INDEX) {
	    return "can't unset array element \"min\"";
	} else if (first == MAX_INDEX) {
	    return "can't unset array element \"max\"";
	}
	/*
	 * Collapse the vector from the point of the first unset element.
	 * Also flush any array variable entries so that the shift is
	 * reflected when the array variable is read.
	 */
	for (i = first, j = last + 1; j < vPtr->numValues; i++, j++) {
	    vPtr->valueArr[i] = vPtr->valueArr[j];
	}
	vPtr->numValues -= ((last - first) + 1);
	FlushCache(vPtr);
	FindLimits(vPtr);
    } else {
	return "unknown variable flags";
    }
    if (flags & (TCL_TRACE_UNSETS | TCL_TRACE_WRITES)) {
	UpdateClients(vPtr);
    }
    return NULL;
}

/*ARGSUSED*/
static int
SetList(vPtr, numElem, elemArr)
    Vector *vPtr;
    int numElem;
    char **elemArr;
{
    register int i;
    double value;

    if (ResizeVector(vPtr, numElem) != TCL_OK) {
	Tcl_AppendResult(vPtr->interp, "can't resize vector \"",
	    NameOfVector(vPtr), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    for (i = 0; i < numElem; i++) {
	if (Tcl_ExprDouble(vPtr->interp, elemArr[i], &value) != TCL_OK) {
	    vPtr->numValues = i;
	    return TCL_ERROR;
	}
	vPtr->valueArr[i] = value;
    }
    return TCL_OK;
}

static int
SetVector(destPtr, srcPtr)
    Vector *destPtr, *srcPtr;
{
    int numBytes;

    if (ResizeVector(destPtr, srcPtr->numValues) != TCL_OK) {
	Tcl_AppendResult(destPtr->interp, "can't resize vector \"",
	    NameOfVector(destPtr), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    numBytes = srcPtr->numValues * sizeof(double);
    memcpy(destPtr->valueArr, srcPtr->valueArr, numBytes);
    destPtr->min = srcPtr->min;
    destPtr->max = srcPtr->max;
    destPtr->offset = srcPtr->offset;
    return TCL_OK;
}

static int
AppendVector(destPtr, srcPtr)
    Vector *destPtr, *srcPtr;
{
    int numBytes;
    int oldSize, newSize;

    oldSize = destPtr->numValues;
    newSize = oldSize + srcPtr->numValues;
    if (ResizeVector(destPtr, newSize) != TCL_OK) {
	Tcl_AppendResult(destPtr->interp, "can't resize vector \"",
	    NameOfVector(destPtr), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    numBytes = (newSize - oldSize) * sizeof(double);
    memcpy((char *)&(destPtr->valueArr[oldSize]), srcPtr->valueArr, numBytes);
    FindLimits(destPtr);
    return TCL_OK;
}

static int
AppendList(vPtr, numElem, elemArr)
    Vector *vPtr;
    int numElem;
    char **elemArr;
{
    int count;
    register int i;
    double value;
    int oldSize;

    oldSize = vPtr->numValues;
    if (ResizeVector(vPtr, vPtr->numValues + numElem) != TCL_OK) {
	Tcl_AppendResult(vPtr->interp, "can't resize vector \"",
	    NameOfVector(vPtr), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    count = oldSize;
    for (i = 0; i < numElem; i++) {
	if (Tcl_ExprDouble(vPtr->interp, elemArr[i], &value) != TCL_OK) {
	    vPtr->numValues = count;
	    return TCL_ERROR;
	}
	vPtr->valueArr[count++] = value;
    }
    FindLimits(vPtr);
    return TCL_OK;
}

/* Vector instance option commands */

/*
 * -----------------------------------------------------------------------
 *
 * AppendOper --
 *
 *	Appends one of more Tcl lists of values, or vector objects
 *	onto the end of the current vector object.
 *
 * Results:
 *	A standard Tcl result.  If a current vector can't be created,
 *      resized, any of the named vectors can't be found, or one of
 *	lists of values is invalid, TCL_ERROR is returned.
 *
 * Side Effects:
 *	Clients of current vector will be notified of the change.
 *
 * -----------------------------------------------------------------------
 */
static int
AppendOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register int i;
    int result;

    for (i = 2; i < argc; i++) {
	if (isalpha(argv[i][0])) {
	    Vector *v2Ptr;

	    v2Ptr = FindVector(interp, argv[i], TCL_LEAVE_ERR_MSG);
	    if (v2Ptr == NULL) {
		return TCL_ERROR;
	    }
	    result = AppendVector(vPtr, v2Ptr);
	} else {
	    int numElem;
	    char **elemArr;

	    if (Tcl_SplitList(interp, argv[i], &numElem, &elemArr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    result = AppendList(vPtr, numElem, elemArr);
	    ckfree((char *)elemArr);
	}
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (argc > 2) {
	FlushCache(vPtr);
	UpdateClients(vPtr);
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * ClearOper --
 *
 *	Deletes all the accumulated array indices for the Tcl array
 *	associated will the vector.  This routine can be used to
 *	free excess memory from a large vector.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side Effects:
 *	Memory used for the entries of the Tcl array variable is freed.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ClearOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    FlushCache(vPtr);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * DeleteOper --
 *
 *	Deletes the given indices from the vector.  If no indices are
 *	provided the entire vector is deleted.
 *
 * Results:
 *	A standard Tcl result.  If any of the given indices is invalid,
 *	interp->result will an error message and TCL_ERROR is returned.
 *
 * Side Effects:
 *	The clients of the vector will be notified of the vector
 *	deletions.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    unsigned char *unsetArr;
    register int i, j;
    register int count;
    int first, last;

    if (argc == 2) {
	FreeVector(vPtr);
	return TCL_OK;
    }
    /*
     * Allocate an "unset" bitmap the size of the vector.  We should
     * try to use bit fields instead of a character array, since
     * memory may be an issue if the vector is large.
     */
    unsetArr = (unsigned char *)ckcalloc(sizeof(unsigned char),vPtr->numValues);
    if (unsetArr == NULL) {
	Panic("can't allocate unset array");
    }
    for (i = 2; i < argc; i++) {
	if (GetIndex2(vPtr, argv[i], &first, &last, 
		(ALLOW_COLON | CHECK_RANGE)) != TCL_OK) {
	    ckfree((char *)unsetArr);
	    return TCL_ERROR;
	}
	for(j = first; j <= last; j++) {
	    unsetArr[j] = TRUE;
	}
    }
    count = 0;
    for (i = 0; i < vPtr->numValues; i++) {
	if (unsetArr[i]) {
	    continue;
	}
	if (count < i) {
	    vPtr->valueArr[count] = vPtr->valueArr[i];
	}
	count++;
    }
    ckfree((char *)unsetArr);
    vPtr->numValues = count;
    FlushCache(vPtr);
    FindLimits(vPtr);
    UpdateClients(vPtr);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * DupOper --
 *
 *	Creates one or more duplicates of the vector object.
 *
 * Results:
 *	A standard Tcl result.  If a new vector can't be created,
 *      or and existing vector resized, TCL_ERROR is returned.
 *
 * Side Effects:
 *	Clients of existing vectors will be notified of the change.
 *
 * -----------------------------------------------------------------------
 */
static int
DupOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Vector *v2Ptr;
    int isNew;
    register int i;

    for (i = 2; i < argc; i++) {
	v2Ptr = CreateVector(interp, argv[i], &isNew);
	if (v2Ptr == NULL) {
	    return TCL_ERROR;
	}
	if (v2Ptr == vPtr) {
	    continue;
	}
	if (SetVector(v2Ptr, vPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!isNew) {
	    FlushCache(v2Ptr);
	    UpdateClients(v2Ptr);
	}
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * LengthOper --
 *
 *	Returns the length of the vector.  If a new size is given, the
 *	vector is resized to the new vector.
 *
 * Results:
 *	A standard Tcl result.  If the new length is invalid,
 *	interp->result will an error message and TCL_ERROR is returned.
 *	Otherwise interp->result will contain the length of the vector.
 *
 * -----------------------------------------------------------------------
 */
static int
LengthOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char tmpString[10];

    if (argc == 3) {
	int size;

	if (Tcl_GetInt(interp, argv[2], &size) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (size < 0) {
	    Tcl_AppendResult(interp, "bad vector size \"", argv[3], "\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
	if (ResizeVector(vPtr, size) != TCL_OK) {
	    Tcl_AppendResult(vPtr->interp, "can't resize vector \"",
		NameOfVector(vPtr), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	FlushCache(vPtr);
	UpdateClients(vPtr);
    }
    sprintf(tmpString, "%d", vPtr->numValues);
    Tcl_SetResult(interp, tmpString, TCL_VOLATILE);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * MergeOper --
 *
 *	Merges the values from the given vectors to the current vector.
 *
 * Results:
 *	A standard Tcl result.  If any of the given vectors differ in size,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and the
 *	vector data will contain merged values of the given vectors.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
MergeOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Vector *v2Ptr;
    Vector **vecArr;
    register Vector **vPtrPtr;
    int refSize;
    register int i;

    /*
     * Allocate an array of vector pointers of each vector
     * to be merged in the current vector.
     */
    vecArr = (Vector **)ckalloc(sizeof(Vector *) * argc);
    if (vecArr == NULL) {
	Panic("can't allocate vector pointer array");
    }
    vPtrPtr = vecArr;
    *vPtrPtr = vPtr;		/* Initialize the list with the first vector */
    vPtrPtr++;

    refSize = vPtr->numValues;
    for (i = 2; i < argc; i++) {
	v2Ptr = FindVector(interp, argv[i], TCL_LEAVE_ERR_MSG);
	if (v2Ptr == NULL) {
	    ckfree((char *)vecArr);
	    return TCL_ERROR;
	}
	/* Check that all the vectors are the same length */

	if (v2Ptr->numValues != refSize) {
	    Tcl_AppendResult(vPtr->interp, "vectors \"", NameOfVector(vPtr),
		"\" and \"", NameOfVector(v2Ptr), "\" differ in length",
		(char *)NULL);
	    ckfree((char *)vecArr);
	    return TCL_ERROR;
	}
	*vPtrPtr = v2Ptr;
	vPtrPtr++;
    }
    *vPtrPtr = NULL;

    /* Merge the values from each of the vectors into the current vector */
    for (i = 0; i < refSize; i++) {
	for (vPtrPtr = vecArr; *vPtrPtr != NULL; vPtrPtr++) {
	    Blt_AppendDouble(interp, (*vPtrPtr)->valueArr[i]);
	}
    }
    ckfree((char *)vecArr);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * MergeOper --
 *
 *	Merges the values from the given vectors to the current vector.
 *
 * Results:
 *	A standard Tcl result.  If any of the given vectors differ in size,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and the
 *	vector data will contain merged values of the given vectors.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
NotifyOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    char c;
    int length;

    c = argv[2][0];
    length = strlen(argv[2]);
    if ((c == 'a') && (length > 1)
	    && (strncmp(argv[2], "always", length) == 0)) {
	vPtr->flags &= ~NOTIFY_WHEN_MASK;
	vPtr->flags |= NOTIFY_ALWAYS;
    } else if ((c == 'n') && (length > 2)
	    && (strncmp(argv[2], "never", length) == 0)) {
	vPtr->flags &= ~NOTIFY_WHEN_MASK;
	vPtr->flags |= NOTIFY_NEVER;
    } else if ((c == 'w') && (length > 1)
	    && (strncmp(argv[2], "whenidle", length) == 0)) {
	vPtr->flags &= ~NOTIFY_WHEN_MASK;
	vPtr->flags |= NOTIFY_WHENIDLE;
    } else if ((c == 'n') && (length > 2)
	    && (strncmp(argv[2], "now", length) == 0)) {
	/* How does this play when an update is pending? */
	NotifyClients(vPtr);
    } else if ((c == 'c') && (length > 1)
	    && (strncmp(argv[2], "cancel", length) == 0)) {
	if (vPtr->flags & NOTIFY_PENDING) {
	    vPtr->flags &= ~NOTIFY_PENDING;
	    Tcl_CancelIdleCall((Tcl_IdleProc*)NotifyClients, (ClientData)vPtr);
	}
    } else if ((c == 'p') && (length > 1)
	    && (strncmp(argv[2], "pending", length) == 0)) {
	Tcl_SetResult(interp, (vPtr->flags & NOTIFY_PENDING) ? "1" : "0",
	    TCL_STATIC);
    } else {
	Tcl_AppendResult(interp, "bad qualifier \"", argv[2], "\": should be ",
	    "\"always\", \"never\", \"whenidle\", \"now\", \"cancel\", or ",
	    "\"pending\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * PopulateOper --
 *
 *	Creates or resizes a new vector based upon the density specified.
 *
 * Results:
 *	A standard Tcl result.  If the density is invalid, TCL_ERROR
 *	is returned.  Otherwise TCL_OK is returned.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PopulateOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Vector *v2Ptr;
    int size, density;
    int isNew;
    register int i, j;
    double slice, range;
    register double *valuePtr;
    int count;

    v2Ptr = CreateVector(interp, argv[2], &isNew);
    if (v2Ptr == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_GetInt(interp, argv[3], &density) != TCL_OK) {
	return TCL_ERROR;
    }
    if (density < 1) {
	Tcl_AppendResult(interp, "invalid density \"", argv[3], "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    size = (vPtr->numValues - 1) * (density + 1) + 1;
    if (ResizeVector(v2Ptr, size) != TCL_OK) {
	Tcl_AppendResult(v2Ptr->interp, "can't resize vector \"",
	    NameOfVector(v2Ptr), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    count = 0;
    valuePtr = v2Ptr->valueArr;
    for (i = 0; i < (vPtr->numValues - 1); i++) {
	range = vPtr->valueArr[i + 1] - vPtr->valueArr[i];
	slice = range / (double)(density + 1);
	for (j = 0; j <= density; j++) {
	    *valuePtr = vPtr->valueArr[i] + (slice * (double)j);
	    valuePtr++;
	    count++;
	}
    }
    count++;
    *valuePtr = vPtr->valueArr[i];
    assert(count == v2Ptr->numValues);
    FindLimits(v2Ptr);
    if (!isNew) {
	FlushCache(v2Ptr);
	UpdateClients(v2Ptr);
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * RangeOper --
 *
 *	Returns a Tcl list of the range of vector values specified.
 *
 * Results:
 *	A standard Tcl result.  If the given range is invalid, TCL_ERROR
 *	is returned.  Otherwise TCL_OK is returned and interp->result
 *	will contain the list of values.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
RangeOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    int first, last;
    register int i;

    if ((GetIndex(vPtr, argv[2], &first, CHECK_RANGE) != TCL_OK) ||
	(GetIndex(vPtr, argv[3], &last, CHECK_RANGE) != TCL_OK)) {
	return TCL_ERROR;
    }
    Tcl_ResetResult(interp);
    if (first > last) {
	/* Return the list reversed */
	for (i = last; i <= first; i++) {
	    Blt_AppendDouble(interp, vPtr->valueArr[i]);
	}
    } else {
	for (i = first; i <= last; i++) {
	    Blt_AppendDouble(interp, vPtr->valueArr[i]);
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * OutOfRange --
 *
 *	Determines if a value does not lie within a given range.  
 *
 *	The value is normalized and compared against the interval 
 *	[0..1], where 0.0 is the minimum and 1.0 is the maximum.
 *	DBL_EPSILON is the smallest number that can be represented
 *	on the host machine, such that (1.0 + epsilon) != 1.0.
 *
 *	Please note, *max* can't equal *min*.
 *
 * Results:
 *	Returns whether the value lies outside of the given range.
 *	If value is outside of the interval [min..max], 1 is returned; 
 *	0 otherwise.
 *
 * ---------------------------------------------------------------------- 
 */
INLINE static int
OutOfRange(value, min, max)
    register double value, min, max;
{
    register double norm;

    norm = (value - min) / (max - min);
    return(((norm - 1.0) > DBL_EPSILON) || (((1.0 - norm) - 1.0) >DBL_EPSILON));
}

/*
 * -----------------------------------------------------------------------
 *
 * SearchOper --
 *
 *	Searchs for a value in the vector. Returns the indices of all
 *	vector elements matching a particular value.
 *
 * Results:
 *	Always returns TCL_OK.  interp->result will contain a list
 *	of the indices of array elements matching value. If no elements
 *	match, interp->result will contain the empty string.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SearchOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    double min, max;
    register int i;

    if (Tcl_ExprDouble(interp, argv[2], &min) != TCL_OK) {
	return TCL_ERROR;
    }
    max = min;
    if ((argc > 3) && (Tcl_ExprDouble(interp, argv[3], &max) != TCL_OK)) {
	return TCL_ERROR;
    }
    Tcl_ResetResult(interp);
    if (min != max) {
	for (i = 0; i < vPtr->numValues; i++) {
	    if (!OutOfRange(vPtr->valueArr[i], min, max)) {
		Blt_AppendInt(interp, i + vPtr->offset);
	    }
	}
    } else {
	for (i = 0; i < vPtr->numValues; i++) {
	    if (vPtr->valueArr[i] == min) {
		Blt_AppendInt(interp, i + vPtr->offset);
	    }
	}
    } 
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * OffsetOper --
 *
 *	Queries or sets the offset of the array index from the base
 *	address of the data array of values.
 *
 * Results:
 *	A standard Tcl result.  If the source vector doesn't exist
 *	or the source list is not a valid list of numbers, TCL_ERROR
 *	returned.  Otherwise TCL_OK is returned.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
OffsetOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    char tmpString[10];

    if (argc == 3) {
	int newOffset;

	if (Tcl_GetInt(interp, argv[2], &newOffset) != TCL_OK) {
	    return TCL_ERROR;
	}
	vPtr->offset = newOffset;
    }
    sprintf(tmpString, "%d", vPtr->offset);
    Tcl_SetResult(interp, tmpString, TCL_VOLATILE);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * SetOper --
 *
 *	Sets the data of the vector object from a list of values.
 *
 * Results:
 *	A standard Tcl result.  If the source vector doesn't exist
 *	or the source list is not a valid list of numbers, TCL_ERROR
 *	returned.  Otherwise TCL_OK is returned.
 *
 * Side Effects:
 *	The vector data is reset.  Clients of the vector are notified.
 *	Any cached array indices are flushed.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SetOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    int result;
    Vector *v2Ptr;
    int numElem;
    char **elemArr;

    /* 
     * The source can be either a list of expressions of another vector.
     */
    if (Tcl_SplitList(interp, argv[2], &numElem, &elemArr) != TCL_OK) {
	return TCL_ERROR;
    }
    /* 
     * If there's only one element, check to see whether it's the name
     * of a vector.  Otherwise, treat it as a single numeric expression.  
     */
    if ((numElem == 1) && ((v2Ptr = FindVector(interp, argv[2], 0)) != NULL)) {
	result = SetVector(vPtr, v2Ptr);
    } else {
	result = SetList(vPtr, numElem, elemArr);
    }
    ckfree((char *)elemArr);

    if (result == TCL_OK) {
	/* 
	 * The vector has changed; so flush the array indices (they're
	 * wrong now), find the new limits of the data, and notify
	 * the vector's clients that it's been modified.
	 */
	FlushCache(vPtr);
	FindLimits(vPtr);
	UpdateClients(vPtr);
    }
    return (result);
}

/*
 * -----------------------------------------------------------------------
 *
 * SortOper --
 *
 *	Sorts the vector object and any other vectors according to
 *	sorting order of the vector object.
 *
 * Results:
 *	A standard Tcl result.  If any of the auxiliary vectors are
 *	a different size than the sorted vector object, TCL_ERROR is
 *	returned.  Otherwise TCL_OK is returned.
 *
 * Side Effects:
 *	The vectors are sorted.
 *
 * -----------------------------------------------------------------------
 */
static double *sortArr;		/* Pointer to the array of values currently
				 * being sorted. */
static int reverse;		/* Indicates the ordering of the sort. If
				 * non-zero, the vectors are sorted in
				 * decreasing order */

static int
CompareVector(a, b)
    void *a;
    void *b;
{
    double delta;
    int result;

    delta = sortArr[*(int *)a] - sortArr[*(int *)b];
    if (delta < 0.0) {
	result = -1;
    } else if (delta > 0.0) {
	result = 1;
    } else {
	return 0;
    }
    if (reverse) {
	result = -result;
    }
    return (result);
}

static int
SortOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int *indexArr;
    double *mergeArr;
    Vector *v2Ptr;
    int refSize, numBytes;
    int result;
    register int i, n;

    reverse = FALSE;
    if ((argc > 2) && (argv[2][0] == '-')) {
	int length;

	length = strlen(argv[2]);
	if ((length > 1) && (strncmp(argv[2], "-reverse", length) == 0)) {
	    reverse = TRUE;
	} else {
	    Tcl_AppendResult(interp, "unknown flag \"", argv[2],
		"\": should be \"-reverse\"", (char *)NULL);
	    return TCL_ERROR;
	}
	argc--, argv++;
    }
    refSize = vPtr->numValues;
    /*
     * Create and initialize an array of indices.  This array will be
     * then sorted based upon the current values in the vector in
     * ascending order.  We'll use this array as a guide for sorting
     * the actual values in the vector and any other vectors listed.
     */
    indexArr = (int *)ckalloc(sizeof(int) * refSize);
    if (indexArr == NULL) {
	Panic("can't allocate sort index array");
    }
    for (i = 0; i < refSize; i++) {
	indexArr[i] = i;
    }
    sortArr = vPtr->valueArr;
    qsort((char *)indexArr, refSize, sizeof(int),
        (int (*)(const void *,const void *))CompareVector);

    /*
     * Create an array to store a copy of the current values of the
     * vector. We'll merge the values back into the vector based upon
     * the indices found in the index array.
     */
    numBytes = sizeof(double) * refSize;
    mergeArr = (double *)ckalloc(numBytes);
    if (mergeArr == NULL) {
	Panic("can't allocate sort merge array");
    }
    memcpy((char *)mergeArr, (char *)vPtr->valueArr, numBytes);
    for (n = 0; n < refSize; n++) {
	vPtr->valueArr[n] = mergeArr[indexArr[n]];
    }
    FlushCache(vPtr);
    UpdateClients(vPtr);

    /*
     * Now sort any other vectors in the same fashion.  The vectors
     * must be the same size as the indexArr though.
     */
    result = TCL_ERROR;
    for (i = 2; i < argc; i++) {
	v2Ptr = FindVector(interp, argv[i], TCL_LEAVE_ERR_MSG);
	if (v2Ptr == NULL) {
	    goto error;
	}
	if (v2Ptr->numValues != refSize) {
	    Tcl_AppendResult(interp, "vector \"", NameOfVector(v2Ptr),
		"\" is not the same size as \"", NameOfVector(vPtr), "\"",
		(char *)NULL);
	    goto error;
	}
	memcpy((char *)mergeArr, (char *)v2Ptr->valueArr, numBytes);
	for (n = 0; n < refSize; n++) {
	    v2Ptr->valueArr[n] = mergeArr[indexArr[n]];
	}
	UpdateClients(v2Ptr);
	FlushCache(v2Ptr);
    }
    result = TCL_OK;
  error:
    ckfree((char *)mergeArr);
    ckfree((char *)indexArr);
    return result;
}

/*
 * -----------------------------------------------------------------------
 *
 * ArithOper --
 *
 * Results:
 *	A standard Tcl result.  If the source vector doesn't exist
 *	or the source list is not a valid list of numbers, TCL_ERROR
 *	returned.  Otherwise TCL_OK is returned.
 *
 * Side Effects:
 *	The vector data is reset.  Clients of the vector are notified.
 *	Any cached array indices are flushed.
 *
 * -----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ArithOper(vPtr, interp, argc, argv)
    Vector *vPtr;
    Tcl_Interp *interp;
    int argc;			/* not used */
    char **argv;
{
    register double value;
    register int i;

    if (isalpha(argv[2][0])) {
	Vector *v2Ptr;

	v2Ptr = FindVector(interp, argv[2], TCL_LEAVE_ERR_MSG);
	if (v2Ptr == NULL) {
	    return TCL_ERROR;
	}
	if (v2Ptr->numValues != vPtr->numValues) {
	    Tcl_AppendResult(interp, "vectors \"", argv[0], "\" and \"",
		argv[2], "\" are not the same length", (char *)NULL);
	    return TCL_ERROR;
	}
	Tcl_ResetResult(interp);
	switch (argv[1][0]) {
	    case '*':
		for (i = 0; i < vPtr->numValues; i++) {
		    value = vPtr->valueArr[i] * v2Ptr->valueArr[i];
		    Blt_AppendDouble(interp, value);
		}
		break;

	    case '/':
		for (i = 0; i < vPtr->numValues; i++) {
		    value = vPtr->valueArr[i] / v2Ptr->valueArr[i];
		    Blt_AppendDouble(interp, value);
		}
		break;

	    case '-':
		for (i = 0; i < vPtr->numValues; i++) {
		    value = vPtr->valueArr[i] - v2Ptr->valueArr[i];
		    Blt_AppendDouble(interp, value);
		}
		break;

	    case '+':
		for (i = 0; i < vPtr->numValues; i++) {
		    value = vPtr->valueArr[i] + v2Ptr->valueArr[i];
		    Blt_AppendDouble(interp, value);
		}
		break;
	}
    } else {
	double scalar;

	if (Tcl_ExprDouble(interp, argv[2], &scalar) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_ResetResult(interp);
	switch (argv[1][0]) {
	    case '*':
		for (i = 0; i < vPtr->numValues; i++) {
		    value = vPtr->valueArr[i] * scalar;
		    Blt_AppendDouble(interp, value);
		}
	    break;

	    case '/':
		for (i = 0; i < vPtr->numValues; i++) {
		    value = vPtr->valueArr[i] / scalar;
		    Blt_AppendDouble(interp, value);
		}
	    break;

	    case '-':
		for (i = 0; i < vPtr->numValues; i++) {
		    value = vPtr->valueArr[i] - scalar;
		    Blt_AppendDouble(interp, value);
		}
		break;

	    case '+':
		for (i = 0; i < vPtr->numValues; i++) {
		    value = vPtr->valueArr[i] + scalar;
		    Blt_AppendDouble(interp, value);
		}
		break;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * VectorInstCmd --
 *
 *	Parses and invokes the appropriate vector instance command
 *	option.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
static Blt_OperSpec operSpecs[] =
{
    {"*", 1, (Blt_OperProc) ArithOper,  3, 3, "item",},
    {"+", 1, (Blt_OperProc) ArithOper,  3, 3, "item",},
    {"-", 1, (Blt_OperProc) ArithOper,  3, 3, "item",},
    {"/", 1, (Blt_OperProc) ArithOper,  3, 3, "item",},
    {"append", 1, (Blt_OperProc) AppendOper, 3, 0, "item ?item...?",},
    {"clear", 1, (Blt_OperProc) ClearOper, 2, 2, "",},
    {"delete", 2, (Blt_OperProc) DeleteOper, 2, 0, "index ?index...?",},
    {"dup", 2, (Blt_OperProc) DupOper, 3, 0, "vecName",},
    {"length", 1, (Blt_OperProc) LengthOper, 2, 3, "?newSize?",},
    {"merge", 1, (Blt_OperProc) MergeOper, 3, 0, "vecName ?vecName...?",},
    {"notify", 1, (Blt_OperProc) NotifyOper, 3, 3, "keyword",},
    {"offset", 2, (Blt_OperProc) OffsetOper, 2, 3, "?offset?",},
    {"populate", 1, (Blt_OperProc) PopulateOper, 4, 4, "vecName density",},
    {"range", 1, (Blt_OperProc) RangeOper, 4, 4, "firstIndex lastIndex",},
    {"search", 3, (Blt_OperProc) SearchOper, 3, 4, "value ?value?",},
    {"set", 3, (Blt_OperProc) SetOper, 3, 3, "list",},
    {"sort", 2, (Blt_OperProc) SortOper, 2, 0, "?-reverse? ?vecName...?",},
};
static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

static int
VectorInstCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_OperProc proc;
    Vector *vPtr = (Vector *)clientData;

    proc = Blt_LookupOperation(interp, numSpecs, operSpecs, BLT_OPER_ARG1,
	argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    return ((*proc) (vPtr, interp, argc, argv));
}


/*
 *----------------------------------------------------------------------
 *
 * VectorCmd --
 *
 *	Creates a Tcl command, and array variable representing an
 *	instance of a vector.
 *
 *	vector a 
 *	vector b(20)
 *	vector c(-5:14)
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */
/* ARGUSED */
static int
VectorCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Vector *vPtr;
    char *leftParen, *rightParen, *tmpString;
    register int i;
    int isNew, size, first, last;

    if (argc == 1) {
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch cursor;

	for (hPtr = Tcl_FirstHashEntry(&vectorTable, &cursor);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	    vPtr = (Vector *)Tcl_GetHashValue(hPtr);
	    Tcl_AppendElement(interp, NameOfVector(vPtr));
	}
	return TCL_OK;
    }
    for (i = 1; i < argc; i++) {
	/* mike: make a copy of argv[i], so that we can safely change it. */
	tmpString = ckstrdup(argv[i]);
	size = first = last = 0;
	leftParen = strchr(tmpString, '(');
	rightParen = strchr(tmpString, ')');
	if (((leftParen != NULL) && (rightParen == NULL)) ||
	    ((leftParen == NULL) && (rightParen != NULL)) ||
	    (leftParen > rightParen)) {
	    Tcl_AppendResult(interp, "bad vector specification \"", argv[i],
		"\"", (char *)NULL);
	    ckfree(tmpString);
	    return TCL_ERROR;
	}
	if (leftParen != NULL) {
	    int result;
	    char *colon;

	    *rightParen = '\0';
	    colon = strchr(leftParen + 1, ':');
	    if (colon != NULL) {

		/* Specification is in the form vecName(first:last) */
		*colon = '\0';
		result = Tcl_GetInt(interp, leftParen + 1, &first);
		if ((*(colon+1) != '\0') && (result == TCL_OK)) {
		    result = Tcl_GetInt(interp, colon + 1, &last);
		    if (first > last) {
			Tcl_AppendResult(interp, "bad vector range \"", 
				 tmpString, "\"", (char *)NULL);
			result = TCL_ERROR;
		    }
		    size = (last - first) + 1;
		}
		*colon = ':';
	    } else {
		/* Specification is in the form vecName(size) */
		result = Tcl_GetInt(interp, leftParen + 1, &size);
	    }
	    *rightParen = ')';
	    if (result != TCL_OK) {
	        ckfree(tmpString);
		return TCL_ERROR;
	    }
	    if (size < 0) {
		Tcl_AppendResult(interp, "bad vector size \"", tmpString,
		    "\"", (char *)NULL);
	        ckfree(tmpString);
		return TCL_ERROR;
	    }
	}
	if (leftParen != NULL) {
	    *leftParen = '\0';
	}
	vPtr = CreateVector(interp, tmpString, &isNew);
	if (leftParen != NULL) {
	    *leftParen = '(';
	}
	if (vPtr == NULL) {
	    ckfree(tmpString);
	    return TCL_ERROR;
	}
	if (!isNew) {
	    Tcl_AppendResult(interp, "vector \"", tmpString,
	        "\" already exists", (char *)NULL);
	    ckfree(tmpString);
	    return TCL_ERROR;
	}
	ckfree(tmpString);
	vPtr->offset = first;
	if (size > 0) {
	    if (ResizeVector(vPtr, size) != TCL_OK) {
		Tcl_AppendResult(vPtr->interp, "can't resize vector \"",
		    NameOfVector(vPtr), "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;
}

static void
VectorDeleteCmd(clientData)
    ClientData clientData;	/* unused */
{
    Tcl_DeleteHashTable(&vectorTable);
    initialized = FALSE;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_VectorInit --
 *
 *	This procedure is invoked to initialize the Tcl command
 *	"blt_vector".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates the new command and adds a new entry into a global Tcl
 *	associative array.
 *
 * ------------------------------------------------------------------------
 */
int
Blt_VectorInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = { 
	"vector", VECTOR_VERSION, (Tcl_CmdProc*)VectorCmd,
	(Tcl_CmdDeleteProc*)VectorDeleteCmd,
    };

    /* Flag this so this routine can be run more than once */
    if (!initialized) {
	Tcl_InitHashTable(&vectorTable, TCL_ONE_WORD_KEYS);
	initialized = TRUE;
    }
    return (Blt_InitCmd(interp, &cmdSpec));
}

/* Public C interface to vectors */

/*
 * -----------------------------------------------------------------------
 *
 * Blt_CreateVector --
 *
 *	Creates a new vector by the name and size.
 *
 * Results:
 *	A standard Tcl result.  If the new array size is invalid or a
 *	vector already exists by that name, TCL_ERROR is returned.
 *	Otherwise TCL_OK is returned and the new vector is created.
 *
 * Side Effects:
 *	Memory will be allocated for the new vector.  A new Tcl command
 *	and Tcl array variable will be created.
 *
 * -----------------------------------------------------------------------
 */
int
Blt_CreateVector(interp, vecName, initialSize, vecPtr)
    Tcl_Interp *interp;
    char *vecName;
    int initialSize;
    Blt_Vector *vecPtr;
{
    Vector *vPtr;
    int isNew;

    if (!initialized) {
	Tcl_InitHashTable(&vectorTable, TCL_ONE_WORD_KEYS);
	initialized = TRUE;
    }
    if (initialSize < 0) {
        char tmpString[10];

	sprintf(tmpString, "%d", initialSize);
	Tcl_SetResult(interp, (char *) NULL, TCL_STATIC);
	Tcl_AppendResult(interp, "bad vector size \"", tmpString, "\"",
	    (char *) NULL);
	return TCL_ERROR;
    }
    vPtr = CreateVector(interp, vecName, &isNew);
    if (vPtr == NULL) {
	return TCL_ERROR;
    }
#ifdef notdef
    if (!isNew) {
	Tcl_AppendResult(interp, "vector \"", vecName, "\" already exists",
	    (char *)NULL);
	return TCL_ERROR;
    }
#endif
    if (initialSize > 0) {
	if (ResizeVector(vPtr, initialSize) != TCL_OK) {
	    Tcl_AppendResult(vPtr->interp, "can't resize vector \"",
		NameOfVector(vPtr), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    if (vecPtr != NULL) {
	vecPtr->numValues = vPtr->numValues;
	vecPtr->arraySize = vPtr->arraySize;
	vecPtr->valueArr = vPtr->valueArr;
	vecPtr->min = vPtr->min;
	vecPtr->max = vPtr->max;
    }
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_DeleteVector --
 *
 *	Deletes the vector of the given name.  All clients with
 *	designated callback routines will be notified.
 *
 * Results:
 *	A standard Tcl result.  If no vector exists by that name,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and
 *	vector is deleted.
 *
 * Side Effects:
 *	Memory will be released for the new vector.  Both the Tcl
 *	command and array variable will be deleted.  All clients which
 *	set call back procedures will be notified.
 *
 * -----------------------------------------------------------------------
 */
int
Blt_DeleteVector(interp, vecName)
    Tcl_Interp *interp;
    char *vecName;
{
    Vector *vPtr;

    if (!initialized) {
	Tcl_InitHashTable(&vectorTable, TCL_ONE_WORD_KEYS);
	initialized = TRUE;
    }
    vPtr = FindVector(interp, vecName, TCL_LEAVE_ERR_MSG);
    if (vPtr == NULL) {
	return TCL_ERROR;
    }
    FreeVector(vPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * Blt_VectorExists --
 *
 *	Returns whether the vector associated with the client token
 *	still exists.
 *
 * Results:
 *	Returns 1 is the vector still exists, 0 otherwise.
 *
 * ----------------------------------------------------------------------
 */
int
Blt_VectorExists(interp, vecName)
    Tcl_Interp *interp;
    char *vecName;
{
    Vector *vPtr;

    if (!initialized) {
	Tcl_InitHashTable(&vectorTable, TCL_ONE_WORD_KEYS);
	initialized = TRUE;
    }
    vPtr = FindVector(interp, vecName, 0);
    return (vPtr != NULL);
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_GetVector --
 *
 *	Returns a pointer to the vector associated with the given name.
 *
 * Results:
 *	A standard Tcl result.  If there is no vector "name", TCL_ERROR
 *	is returned.  Otherwise TCL_OK is returned and vecPtrPtr will
 *	point to the vector.
 *
 * -----------------------------------------------------------------------
 */
int
Blt_GetVector(interp, vecName, vecPtr)
    Tcl_Interp *interp;
    char *vecName;
    Blt_Vector *vecPtr;
{
    Vector *vPtr;

    if (!initialized) {
	Tcl_InitHashTable(&vectorTable, TCL_ONE_WORD_KEYS);
	initialized = TRUE;
    }
    vPtr = FindVector(interp, vecName, TCL_LEAVE_ERR_MSG);
    if (vPtr == NULL) {
	return TCL_ERROR;
    }
    vecPtr->numValues = vPtr->numValues;
    vecPtr->arraySize = vPtr->arraySize;
    vecPtr->valueArr = vPtr->valueArr;
    vecPtr->min = vPtr->min;
    vecPtr->max = vPtr->max;

    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_ResetVector --
 *
 *	Resets the vector data.  This is called by a client to
 *	indicate that the vector data has changed.  The vector does
 *	not need to point to different memory.  Any clients of the
 *	vector will be notified of the change.
 *
 * Results:
 *	A standard Tcl result.  If the new array size is invalid,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and the
 *	new vector data is recorded.
 *
 * Side Effects:
 *	Any client designated callbacks will be posted.  Memory may
 *	be changed for the vector array.
 *
 * -----------------------------------------------------------------------
 */
int
Blt_ResetVector(interp, vecName, vecPtr, freeProc)
    Tcl_Interp *interp;		/* Interpreter to report errors back to */
    char *vecName;		/* Name of vector to be reset */
    Blt_Vector *vecPtr;		/* Information about the vector consisting
				 * of array, size, and number of values.
				 * Really points to a Vector structure. */
    Tcl_FreeProc *freeProc;	/* Address of memory deallocation routine
				 * for the array of values.  Can also be
				 * TCL_STATIC, TCL_DYNAMIC, or TCL_VOLATILE. */
{
    int numValues, arraySize;
    double *valueArr;
    Vector *vPtr;

    vPtr = FindVector(interp, vecName, TCL_LEAVE_ERR_MSG);
    if (vPtr == NULL) {
	return TCL_ERROR;
    }
    valueArr = vecPtr->valueArr;
    arraySize = vecPtr->arraySize;
    numValues = vecPtr->numValues;

    if (arraySize < 0) {
	Tcl_SetResult(interp, "invalid array size", TCL_STATIC);
	return TCL_ERROR;
    }
    if (vPtr->valueArr != vecPtr->valueArr) {

	/*
	 * New array of values is in different memory than the current
	 * vector.
	 */

	if ((vecPtr->valueArr != NULL) && (vecPtr->arraySize > 0)) {
	    if (freeProc == TCL_VOLATILE) {
		double *newArr;
		/*
		 * Data is volatile. Make a copy of the value array.
		 */
		newArr = (double *)ckalloc(sizeof(double) * vecPtr->arraySize);
		memcpy((char *)newArr, (char *)vecPtr->valueArr,
		    sizeof(double) * vecPtr->numValues);
		valueArr = newArr;
		freeProc = TCL_DYNAMIC;
	    }
	} else {
	    /* Empty array. Set up default values */
	    freeProc = TCL_STATIC;
	    valueArr = vPtr->staticSpace;
	    arraySize = DEF_ARRAY_SIZE;
	    numValues = 0;
	}

	if ((vPtr->valueArr != vPtr->staticSpace) &&
	    (vPtr->freeProc != TCL_STATIC)) {
	    if (vPtr->freeProc == TCL_DYNAMIC) {
		ckfree((char *)vPtr->valueArr);
	    } else {
		(*freeProc) ((char *)vPtr->valueArr);
	    }
	}
	vPtr->freeProc = freeProc;
	vPtr->valueArr = valueArr;
	vPtr->arraySize = arraySize;
    }
    vPtr->numValues = numValues;
    FlushCache(vPtr);
    FindLimits(vPtr);
    UpdateClients(vPtr);
    return TCL_OK;
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_ResizeVector --
 *
 *	Changes the size of the vector.  All clients with designated
 *	callback routines will be notified of the size change.
 *
 * Results:
 *	A standard Tcl result.  If no vector exists by that name,
 *	TCL_ERROR is returned.  Otherwise TCL_OK is returned and
 *	vector is resized.
 *
 * Side Effects:
 *	Memory may be reallocated for the new vector size.  All clients
 *	which set call back procedures will be notified.
 *
 * -----------------------------------------------------------------------
 */
int
Blt_ResizeVector(interp, vecName, newSize)
    Tcl_Interp *interp;
    char *vecName;
    int newSize;
{
    Vector *vPtr;

    vPtr = FindVector(interp, vecName, TCL_LEAVE_ERR_MSG);
    if (vPtr == NULL) {
	return TCL_ERROR;
    }
    if (ResizeVector(vPtr, newSize) != TCL_OK) {
	Tcl_AppendResult(interp, "can't resize vector \"", NameOfVector(vPtr),
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    FlushCache(vPtr);
    UpdateClients(vPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Blt_AllocVectorId --
 *
 *	Creates an identifier token for an existing vector.  The
 *	identifier is used by the client routines to get call backs
 *	when (and if) the vector changes.
 *
 * Results:
 *	A standard Tcl result.  If "varName" is not associated with
 *	a vector, TCL_ERROR is returned and interp->result is filled
 *	with an error message.
 *
 *--------------------------------------------------------------
 */
Blt_VectorId
Blt_AllocVectorId(interp, varName)
    Tcl_Interp *interp;
    char *varName;
{
    Vector *vPtr;
    Blt_ListItem *itemPtr;
    ClientInfo *clientPtr;
    Blt_VectorId clientId;

    if (!initialized) {
	Tcl_InitHashTable(&vectorTable, TCL_ONE_WORD_KEYS);
	initialized = TRUE;
    }
    vPtr = FindVector(interp, varName, TCL_LEAVE_ERR_MSG);
    if (vPtr == NULL) {
	return (Blt_VectorId)0;
    }
    /* Allocate a new client structure */
    clientPtr = (ClientInfo *)ckcalloc(1, sizeof(ClientInfo));
    if (clientPtr == NULL) {
	Panic("can't allocate client vector structure");
    }
    clientPtr->magic = VECTOR_MAGIC;
    /*
     * Add the pointer to the master list of clients
     */
    itemPtr = Blt_NewItem((char *)clientPtr);
    Blt_LinkAfter(&(vPtr->clients), itemPtr, (Blt_ListItem *)NULL);
    Blt_SetItemValue(itemPtr, clientPtr);
    clientPtr->master = vPtr;

    clientId = (Blt_VectorId)clientPtr;
    return (clientId);
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_SetVectorChangedProc --
 *
 *	Sets the routine to be called back when the vector is changed
 *	or deleted.  *clientData* will be provided as an argument. If
 *	*proc* is NULL, no callback will be made.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The designated routine will be called when the vector is changed
 *	or deleted.
 *
 * -----------------------------------------------------------------------
 */
void
Blt_SetVectorChangedProc(clientId, proc, clientData)
    Blt_VectorId clientId;	/* Client token identifying the vector */
    Blt_VectorChangedProc *proc;/* Address of routine to call when the contents
				 * of the vector change. If NULL, no routine
				 * will be called */
    ClientData clientData;	/* One word of information to pass along when
				 * the above routine is called */
{
    ClientInfo *clientPtr = (ClientInfo *)clientId;

    if (clientPtr->magic != VECTOR_MAGIC) {
	return;			/* Not a valid token */
    }
    clientPtr->clientData = clientData;
    clientPtr->proc = proc;
}

/*
 *--------------------------------------------------------------
 *
 * Blt_FreeVectorId --
 *
 *	Releases the token for an existing vector.  This indicates
 *	that the client is no longer interested the vector.  Any
 *	previously specified callback routine will no longer be
 *	invoked when (and if) the vector changes.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Any previously specified callback routine will no longer be
 *	invoked when (and if) the vector changes.
 *
 *--------------------------------------------------------------
 */
void
Blt_FreeVectorId(clientId)
    Blt_VectorId clientId;	/* Client token identifying the vector */
{
    ClientInfo *clientPtr = (ClientInfo *)clientId;
    Blt_ListItem *itemPtr;

    if (clientPtr->magic != VECTOR_MAGIC) {
	return;			/* Not a valid token */
    }
    if (clientPtr->master != NULL) {
	Vector *vPtr = clientPtr->master;

	/* Remove the client from the master list */
	itemPtr = Blt_FindItem(&(vPtr->clients), (char *)clientPtr);
	if (itemPtr != NULL) {
	    Blt_DeleteItem(itemPtr);
	}
    }
    ckfree((char *)clientPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Blt_NameOfVectorId --
 *
 *	Returns the name of the vector (and array variable).
 *
 * Results:
 *	The name of the array variable is returned.
 *
 *--------------------------------------------------------------
 */
char *
Blt_NameOfVectorId(clientId)
    Blt_VectorId clientId;	/* Client token identifying the vector */
{
    ClientInfo *clientPtr = (ClientInfo *)clientId;

    if ((clientPtr->magic != VECTOR_MAGIC) || (clientPtr->master == NULL)) {
	return NULL;
    }
    return (NameOfVector(clientPtr->master));
}

/*
 *--------------------------------------------------------------
 *
 * Blt_VectorNotifyPending --
 *
 *	Returns the name of the vector (and array variable).
 *
 * Results:
 *	The name of the array variable is returned.
 *
 *--------------------------------------------------------------
 */
int
Blt_VectorNotifyPending(clientId)
    Blt_VectorId clientId;	/* Client token identifying the vector */
{
    ClientInfo *clientPtr = (ClientInfo *)clientId;

    if ((clientPtr->magic != VECTOR_MAGIC) || (clientPtr->master == NULL)) {
	return 0;
    }
    return (clientPtr->master->flags & NOTIFY_PENDING);
}

/*
 * -----------------------------------------------------------------------
 *
 * Blt_GetVectorById --
 *
 *	Returns a pointer to the vector associated with the client
 *	token.
 *
 * Results:
 *	A standard Tcl result.  If the client token is not associated
 *	with a vector any longer, TCL_ERROR is returned. Otherwise,
 *	TCL_OK is returned and vecPtrPtr will point to vector.
 *
 * -----------------------------------------------------------------------
 */
int
Blt_GetVectorById(interp, clientId, vecPtr)
    Tcl_Interp *interp;
    Blt_VectorId clientId;	/* Client token identifying the vector */
    Blt_Vector *vecPtr;
{
    ClientInfo *clientPtr = (ClientInfo *)clientId;
    Vector *vPtr;

    if (clientPtr->magic != VECTOR_MAGIC) {
	Tcl_SetResult(interp, "invalid vector token", TCL_STATIC);
	return TCL_ERROR;
    }
    if (clientPtr->master == NULL) {
	Tcl_SetResult(interp, "vector no longer exists", TCL_STATIC);
	return TCL_ERROR;
    }
    vPtr = clientPtr->master;
    vecPtr->numValues = vPtr->numValues;
    vecPtr->arraySize = vPtr->arraySize;
    vecPtr->valueArr = vPtr->valueArr;
    vecPtr->min = vPtr->min;
    vecPtr->max = vPtr->max;

    return TCL_OK;
}
#endif /* !NO_VECTOR */
