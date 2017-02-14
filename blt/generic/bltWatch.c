/*
 * bltWatch.c --
 *
 * 	This module implements watch procedure callbacks for Tcl
 *	commands and procedures.
 *
 * Copyright 1994-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#ifndef	NO_WATCH

#define WATCH_VERSION "8.0"

#define UNKNOWN_RETURN_CODE	5
static char *codeNames[] =
{
    "OK", "ERROR", "RETURN", "BREAK", "CONTINUE"
};

#ifndef TK_VERSION
/*
 *----------------------------------------------------------------------
 *
 * The following was pulled from tkGet.c so that watches could
 * be used with code which does not include the Tk library.
 *
 *----------------------------------------------------------------------
 */
typedef char *Tk_Uid;

/*
 * The hash table below is used to keep track of all the Tk_Uids created
 * so far.
 */

static Tcl_HashTable uidTable;
static int initialized = 0;

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetUid --
 *
 *	Given a string, this procedure returns a unique identifier
 *	for the string.
 *
 * Results:
 *	This procedure returns a Tk_Uid corresponding to the "string"
 *	argument.  The Tk_Uid has a string value identical to string
 *	(strcmp will return 0), but it's guaranteed that any other
 *	calls to this procedure with a string equal to "string" will
 *	return exactly the same result (i.e. can compare Tk_Uid
 *	*values* directly, without having to call strcmp on what they
 *	point to).
 *
 * Side effects:
 *	New information may be entered into the identifier table.
 *
 *----------------------------------------------------------------------
 */

Tk_Uid
Tk_GetUid(string)
    char *string;		/* String to convert. */
{
    int dummy;

    if (!initialized) {
	Tcl_InitHashTable(&uidTable, TCL_STRING_KEYS);
	initialized = 1;
    }
    return (Tk_Uid) Tcl_GetHashKey(&uidTable,
	Tcl_CreateHashEntry(&uidTable, string, &dummy));
}

#endif /* TK_VERSION */

#define WATCH_MAX_LEVEL	10000	/* Maximum depth of Tcl traces. */

enum WatchStates {
    WATCH_STATE_DONT_CARE = -1,	/* Select watch regardless of state */
    WATCH_STATE_IDLE = 0,	/*  */
    WATCH_STATE_ACTIVE = 1
};

typedef struct {
    Tcl_Interp *interp;		/* Interpreter associated with the watch */
    Tk_Uid nameId;		/* Watch identifier */

    /* User-configurable fields */
    enum WatchStates state;	/* Current state of watch: either
				 * WATCH_STATE_IDLE or WATCH_STATE_ACTIVE */
    int maxLevel;		/* Maximum depth of tracing allowed */
    char **preCmd;		/* Procedure to be invoked before the
				 * command is executed (but after
				 * substitutions have occurred). */
    char **postCmd;		/* Procedure to be invoked after the command
				 * is executed. */
    Tcl_Trace trace;		/* Trace handler which activates "pre"
				 * command procedures */
    Tcl_AsyncHandler asyncHandle;	/* Async handler which triggers the
				 * "post" command procedure (if one
				 * exists) */
    int active;			/* Indicates if a trace is currently
				 * active.  This prevents recursive
				 * tracing of the "pre" and "post"
				 * procedures. */
    int level;			/* Current level of traced command. */
    char *cmdPtr;		/* Command string before substitutions.
				 * Points to a original command buffer. */
    char *args;			/* Tcl list of the command after
				 * substitutions. List is malloc-ed by
				 * Tcl_Merge. Must be freed in handler
				 * procs */
} Watch;

typedef struct {
    Tk_Uid nameId;		/* Name identifier of the watch */
    Tcl_Interp *interp;		/* Interpreter associated with the
				 * watch */
} WatchKey;

static Tcl_HashTable watchTable;
static int initialized = 0;

/*
 *----------------------------------------------------------------------
 *
 * PreCmdProc --
 *
 *	Procedure callback for Tcl_Trace. Gets called before the
 * 	command is executed, but after substitutions have occurred.
 *	If a watch procedure is active, it evals a Tcl command.
 *	Activates the "precmd" callback, if one exists.
 *
 *	Stashes some information for the "pre" callback: command
 *	string, substituted argument list, and current level.
 *
 * 	Format of "pre" proc:
 *
 * 	proc beforeCmd { level cmdStr argList } {
 *
 * 	}
 *
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	A Tcl_AsyncHandler may be triggered, if a "post" procedure is
 *	defined.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
PreCmdProc(clientData, interp, level, command, cmdProc, cmdClientData,
    argc, argv)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* not used */
    int level;			/* Current level */
    char *command;		/* Command before substitution */
    int (*cmdProc) ();		/* not used */
    ClientData cmdClientData;	/* not used */
    int argc;
    char **argv;		/* Command after parsing, but before
				 * evaluation */
{
    Watch *watchPtr = (Watch *)clientData;

    if (((watchPtr->preCmd == NULL) && (watchPtr->postCmd == NULL)) ||
	(watchPtr->active)) {
	return;			/* Don't re-enter from Tcl_Eval below */
    }
    watchPtr->level = level;
    /*
     * There's no guarantee that the calls to PreCmdProc will match
     * up with PostCmdProc.  So free up argument lists that are still
     * hanging around before allocating a new one.
     */
    if (watchPtr->args != NULL) {
	ckfree(watchPtr->args);
    }
    watchPtr->args = Tcl_Merge(argc, argv);
    /*
     * mike: Dito for the command string.
     */
    if (watchPtr->cmdPtr != NULL) {
	ckfree(watchPtr->cmdPtr);
    }
    watchPtr->cmdPtr = ckstrdup(command);

    if (watchPtr->preCmd != NULL) {
	Tcl_DString buffer;
	char string[200];
	int status;
	register char **p;

	/* Create the "pre" command procedure call */
	Tcl_DStringInit(&buffer);
	for (p = watchPtr->preCmd; *p != NULL; p++) {
	    Tcl_DStringAppendElement(&buffer, *p);
	}
	sprintf(string, "%d", watchPtr->level);
	Tcl_DStringAppendElement(&buffer, string);
	Tcl_DStringAppendElement(&buffer, watchPtr->cmdPtr);
	Tcl_DStringAppendElement(&buffer, watchPtr->args);

	watchPtr->active = 1;
	status = Tcl_Eval(interp, Tcl_DStringValue(&buffer));
	watchPtr->active = 0;

	Tcl_DStringFree(&buffer);
	if (status != TCL_OK) {
	    fprintf(stderr, "%s failed: %s\n", watchPtr->preCmd[0],
		Tcl_GetStringResult(interp));
	}
    }
    /* Set the trigger for the "post" command procedure */
    if (watchPtr->postCmd != NULL) {
	Tcl_AsyncMark(watchPtr->asyncHandle);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PostCmdProc --
 *
 *	Procedure callback for Tcl_AsyncHandler. Gets called after
 *	the command has executed.  It tests for a "post" command, but
 *	you really can't get here, if one doesn't exist.
 *
 *	Save the current contents of interp->result before calling
 *	the "post" command, and restore it afterwards.
 *
 * 	Format of "post" proc:
 *
 * 	proc afterCmd { level cmdStr argList retCode results } {
 *
 *	}
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Memory for argument list is released.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
PostCmdProc(clientData, interp, code)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* not used */
    int code;			/* Completion code of command */
{
    Watch *watchPtr = (Watch *)clientData;

    if (watchPtr->active) {
	return code;
    }
    if (watchPtr->postCmd != NULL) {
	int status;
	Tcl_DString buffer;
	char string[200];
	char *results;
	register char **p;
	char *retCode;
	char *errorCode, *errorInfo;
	errorInfo = errorCode = NULL;

	results = "NO INTERPRETER AVAILABLE";

	/*
	 * ----------------------------------------------------
	 *
	 * Save the state of the interpreter.
	 *
	 * ----------------------------------------------------
	 */
	if (interp != NULL) {
	    results = ckstrdup(Tcl_GetStringResult(interp));
	    errorInfo = Tcl_GetVar2(interp, "errorInfo", (char *)NULL,
		TCL_GLOBAL_ONLY);
	    if (errorInfo != NULL) {
		errorInfo = ckstrdup(errorInfo);
	    }
	    errorCode = Tcl_GetVar2(interp, "errorCode", (char *)NULL,
		TCL_GLOBAL_ONLY);
	    if (errorCode != NULL) {
		errorCode = ckstrdup(errorCode);
	    }
	}
	/* Create the "post" command procedure call */
	Tcl_DStringInit(&buffer);
	for (p = watchPtr->postCmd; *p != NULL; p++) {
	    Tcl_DStringAppendElement(&buffer, *p);
	}
	sprintf(string, "%d", watchPtr->level);
	Tcl_DStringAppendElement(&buffer, string);
	Tcl_DStringAppendElement(&buffer, watchPtr->cmdPtr);
	Tcl_DStringAppendElement(&buffer, watchPtr->args);
	if (code < UNKNOWN_RETURN_CODE) {
	    retCode = codeNames[code];
	} else {
	    sprintf(string, "%d", code);
	    retCode = string;
	}
	Tcl_DStringAppendElement(&buffer, retCode);
	Tcl_DStringAppendElement(&buffer, results);

	watchPtr->active = 1;
	status = Tcl_Eval(watchPtr->interp, Tcl_DStringValue(&buffer));
	watchPtr->active = 0;

	Tcl_DStringFree(&buffer);
	ckfree(watchPtr->args);
	watchPtr->args = NULL;
	ckfree(watchPtr->cmdPtr);
	watchPtr->cmdPtr = NULL;

	if (status != TCL_OK) {
	    fprintf(stderr, "%s failed: %s\n", watchPtr->postCmd[0],
		Tcl_GetStringResult(watchPtr->interp));
	}
	/*
	 * ----------------------------------------------------
	 *
	 * Restore the state of the interpreter.
	 *
	 * ----------------------------------------------------
	 */
	if (interp != NULL) {
	    if (errorInfo != NULL) {
		Tcl_SetVar2(interp, "errorInfo", (char *)NULL, errorInfo,
		    TCL_GLOBAL_ONLY);
		ckfree((char *)errorInfo);
	    }
	    if (errorCode != NULL) {
		Tcl_SetVar2(interp, "errorCode", (char *)NULL, errorCode,
		    TCL_GLOBAL_ONLY);
		ckfree((char *)errorCode);
	    }
	    Tcl_ResetResult(interp);
	    Tcl_SetResult(interp, results, TCL_DYNAMIC);
	}
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * NewWatch --
 *
 *	Creates a new watch. Uses the nameId and interpreter
 *	address to create a unique hash key.  The new watch is
 *	registered into the "watchTable" hash table. Also creates a
 *	Tcl_AsyncHandler for triggering "post" events.
 *
 * Results:
 *	If memory for the watch could be allocated, a pointer to
 *	the new watch is returned.  Otherwise NULL, and interp->result
 *	points to an error message.
 *
 * Side Effects:
 *	A new Tcl_AsyncHandler is created. A new hash table entry
 *	is created. Memory the watch structure is allocated.
 *
 *----------------------------------------------------------------------
 */
static Watch *
NewWatch(interp, nameId)
    Tcl_Interp *interp;
    Tk_Uid nameId;
{
    Watch *watchPtr;
    WatchKey key;
    Tcl_HashEntry *hPtr;
    int dummy;

    watchPtr = (Watch *)ckcalloc(1, sizeof(Watch));
    if (watchPtr == NULL) {
	Tcl_SetResult(interp, "can't allocate watch structure", TCL_STATIC);
	return NULL;
    }
    watchPtr->state = WATCH_STATE_ACTIVE;
    watchPtr->maxLevel = WATCH_MAX_LEVEL;
    watchPtr->nameId = nameId;
    watchPtr->interp = interp;
    watchPtr->asyncHandle = Tcl_AsyncCreate((Tcl_AsyncProc*)PostCmdProc,
        (ClientData)watchPtr);
    watchPtr->cmdPtr = NULL;
    watchPtr->args = NULL;
    key.interp = interp;
    key.nameId = nameId;
    hPtr = Tcl_CreateHashEntry(&watchTable, (char *)&key, &dummy);
    Tcl_SetHashValue(hPtr, (ClientData)watchPtr);
    return watchPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyWatch --
 *
 *	Removes the watch. The resources used by the watch
 *	are released.
 *	  1) If the watch is active, its trace is deleted.
 *	  2) Memory for command strings is free-ed.
 *	  3) Entry is removed from watch registry.
 *	  4) Async handler is deleted.
 *	  5) Memory for watch itself is released.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Everything associated with the watch is freed.
 *
 *----------------------------------------------------------------------
 */
static void
DestroyWatch(watchPtr)
    Watch *watchPtr;
{
    WatchKey key;
    Tcl_HashEntry *hPtr;

    Tcl_AsyncDelete(watchPtr->asyncHandle);
    if (watchPtr->state == WATCH_STATE_ACTIVE) {
	Tcl_DeleteTrace(watchPtr->interp, watchPtr->trace);
    }
    if (watchPtr->preCmd != NULL) {
	ckfree((char *)watchPtr->preCmd);
    }
    if (watchPtr->postCmd != NULL) {
	ckfree((char *)watchPtr->postCmd);
    }
    if (watchPtr->args != NULL) {
	ckfree((char *)watchPtr->args);
    }
    key.interp = watchPtr->interp;
    key.nameId = watchPtr->nameId;
    hPtr = Tcl_FindHashEntry(&watchTable, (char *)&key);
    Tcl_DeleteHashEntry(hPtr);
    ckfree((char *)watchPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FindWatch --
 *
 *	Searches for the watch represented by the watch name and its
 *	associated interpreter in its directory.
 *
 * Results:
 *	If found, the pointer to the watch structure is returned,
 *	otherwise NULL. If requested, interp-result will be filled
 *	with an error message.
 *
 *----------------------------------------------------------------------
 */
static Watch *
FindWatch(interp, nameId, flags)
    Tcl_Interp *interp;
    Tk_Uid nameId;
    int flags;
{
    WatchKey key;
    Tcl_HashEntry *hPtr;

    key.interp = interp;
    key.nameId = nameId;
    hPtr = Tcl_FindHashEntry(&watchTable, (char *)&key);
    if (hPtr == NULL) {
	if (flags & TCL_LEAVE_ERR_MSG) {
	    Tcl_AppendResult(interp, "can't find any watch named \"", nameId,
		"\"", (char *)NULL);
	}
	return NULL;
    }
    return (Watch *) Tcl_GetHashValue(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ListWatches --
 *
 *	Creates a list of all watches in the interpreter.  The
 *	list search may be restricted to selected states by
 *	setting "state" to something other than WATCH_STATE_DONT_CARE.
 *
 * Results:
 *	A standard Tcl result.  Interp->result will contain a list
 *	of all watches matches the state criteria.
 *
 *----------------------------------------------------------------------
 */
static int
ListWatches(interp, state)
    Tcl_Interp *interp;
    enum WatchStates state;	/* Active flag */
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch cursor;
    register Watch *watchPtr;

    for (hPtr = Tcl_FirstHashEntry(&watchTable, &cursor);
	hPtr != NULL; hPtr = Tcl_NextHashEntry(&cursor)) {
	watchPtr = (Watch *)Tcl_GetHashValue(hPtr);
	if (watchPtr->interp == interp) {
	    if ((state == WATCH_STATE_DONT_CARE) ||
		(state == watchPtr->state)) {
		Tcl_AppendElement(interp, (char *)watchPtr->nameId);
	    }
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigWatch --
 *
 *	Processes argument list of switches and values, setting
 *	Watch fields.
 *
 * Results:
 *	If found, the pointer to the watch structure is returned,
 *	otherwise NULL. If requested, interp-result will be filled
 *	with an error message.
 *
 *----------------------------------------------------------------------
 */
static int
ConfigWatch(watchPtr, interp, argc, argv)
    Watch *watchPtr;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    register int i;
    char *swtch;
    unsigned int length;
    char c;

    for (i = 0; i < argc; i++) {
	length = strlen(argv[i]);
	swtch = argv[i++];

	if (*swtch != '-') {
	    goto badSwitch;
	}
	c = swtch[1];
	if (i == argc) {
	    Tcl_AppendResult(interp, "no argument for \"", swtch, "\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
	if ((c == 'p') && (length > 1) &&
	    (strncmp(swtch, "-precmd", length) == 0)) {
	    if (watchPtr->preCmd != NULL) {
		ckfree((char *)watchPtr->preCmd);
		watchPtr->preCmd = NULL;
	    }
	    if (argv[i][0] != '\0') {
		int dummy;
		char **argArr;

		if (Tcl_SplitList(interp, argv[i], &dummy, &argArr) != TCL_OK) {
		    return TCL_ERROR;
		}
		watchPtr->preCmd = argArr;
	    } else {
		watchPtr->preCmd = (char **)NULL;
	    }
	} else if ((c == 'p') && (length > 1) &&
	    (strncmp(swtch, "-postcmd", length) == 0)) {
	    if (watchPtr->postCmd != NULL) {
		ckfree((char *)watchPtr->postCmd);
		watchPtr->postCmd = NULL;
	    }
	    if (argv[i][0] != '\0') {
		int dummy;
		char **argArr;

		if (Tcl_SplitList(interp, argv[i], &dummy, &argArr) != TCL_OK) {
		    return TCL_ERROR;
		}
		watchPtr->postCmd = argArr;
	    } else {
		watchPtr->postCmd = (char **)NULL;
	    }
	} else if ((c == 'a') && (length > 1) &&
	    (strncmp(swtch, "-active", length) == 0)) {
	    int bool;

	    if (Tcl_GetBoolean(interp, argv[i], &bool) != TCL_OK) {
		return TCL_ERROR;
	    }
	    watchPtr->state = (bool) ? WATCH_STATE_ACTIVE : WATCH_STATE_IDLE;
	} else if ((c == 'm') &&
	    (strncmp(swtch, "-maxlevel", length) == 0)) {
	    int newLevel;

	    if (Tcl_GetInt(interp, argv[i], &newLevel) != TCL_OK) {
		return TCL_ERROR;
	    }
	    watchPtr->maxLevel = newLevel;
	} else {
	  badSwitch:
	    Tcl_AppendResult(interp, "bad switch \"", swtch, "\": should be ",
	        "\"-active\", \"-maxlevel\", \"-precmd\" or \"-postcmd\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
    }
    /*
     * If the watch's max depth changed or its state, reset the traces.
     */
    if (watchPtr->trace != (Tcl_Trace) 0) {
	Tcl_DeleteTrace(interp, watchPtr->trace);
	watchPtr->trace = (Tcl_Trace) 0;
    }
    if (watchPtr->state == WATCH_STATE_ACTIVE) {
	watchPtr->trace = Tcl_CreateTrace(interp, watchPtr->maxLevel,
	    (Tcl_CmdTraceProc*)PreCmdProc, (ClientData)watchPtr);
    }
    return TCL_OK;
}

/* Tcl interface routines */
/*
 *----------------------------------------------------------------------
 *
 * CreateOper --
 *
 *	Creates a new watch and processes any extra switches.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side Effects:
 *	A new watch is created.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CreateOper(clientData, interp, argc, argv)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Watch *watchPtr;
    Tk_Uid nameId;

    nameId = Tk_GetUid(argv[2]);
    watchPtr = FindWatch(interp, nameId, 0);
    if (watchPtr != NULL) {
	Tcl_AppendResult(interp, "a watch \"", argv[2], "\" already exists",
	    (char *)NULL);
	return TCL_ERROR;
    }
    watchPtr = NewWatch(interp, nameId);
    if (watchPtr == NULL) {
	return TCL_ERROR;	/* Can't create new watch */
    }
    return (ConfigWatch(watchPtr, interp, argc - 3, argv + 3));
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteOper --
 *
 *	Deletes the watch.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOper(clientData, interp, argc, argv)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Watch *watchPtr;
    Tk_Uid nameId;

    nameId = Tk_GetUid(argv[2]);
    watchPtr = FindWatch(interp, nameId, TCL_LEAVE_ERR_MSG);
    if (watchPtr == NULL) {
	return TCL_ERROR;
    }
    DestroyWatch(watchPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ActivateOper --
 *
 *	Activate/deactivates the named watch.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ActivateOper(clientData, interp, argc, argv)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Watch *watchPtr;
    Tk_Uid nameId;
    enum WatchStates state;

    state = (argv[1][0] == 'a') ? WATCH_STATE_ACTIVE : WATCH_STATE_IDLE;
    nameId = Tk_GetUid(argv[2]);
    watchPtr = FindWatch(interp, nameId, TCL_LEAVE_ERR_MSG);
    if (watchPtr == NULL) {
	return TCL_ERROR;
    }
    if (state != watchPtr->state) {
	if (watchPtr->trace == (Tcl_Trace) 0) {
	    watchPtr->trace = Tcl_CreateTrace(interp, watchPtr->maxLevel,
		(Tcl_CmdTraceProc*)PreCmdProc, (ClientData)watchPtr);
	} else {
	    Tcl_DeleteTrace(interp, watchPtr->trace);
	    watchPtr->trace = (Tcl_Trace) 0;
	}
	watchPtr->state = state;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NamesOper --
 *
 *	Returns the names of all watches in the interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
NamesOper(clientData, interp, argc, argv)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    enum WatchStates state;

    state = WATCH_STATE_DONT_CARE;
    if (argc == 3) {
	char c;
	c = argv[2][0];
	if ((c == 'a') && (strcmp(argv[2], "active") == 0)) {
	    state = WATCH_STATE_ACTIVE;
	} else if ((c == 'i') && (strcmp(argv[2], "idle") == 0)) {
	    state = WATCH_STATE_IDLE;
	} else if ((c == 'i') && (strcmp(argv[2], "ignore") == 0)) {
	    state = WATCH_STATE_DONT_CARE;
	} else {
	    Tcl_AppendResult(interp, "bad state \"", argv[2], "\" should be ",
		"\"active\", \"idle\", or \"ignore\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    return (ListWatches(interp, state));
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureOper --
 *
 *	Convert the limits of the pixel values allowed into a list.
 *
 * Results:
 *	The string representation of the limits is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigureOper(clientData, interp, argc, argv)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Watch *watchPtr;
    Tk_Uid nameId;

    nameId = Tk_GetUid(argv[2]);
    watchPtr = FindWatch(interp, nameId, TCL_LEAVE_ERR_MSG);
    if (watchPtr == NULL) {
	return TCL_ERROR;
    }
    return (ConfigWatch(watchPtr, interp, argc - 3, argv + 3));
}

/*
 *----------------------------------------------------------------------
 *
 * InfoOper --
 *
 *	Convert the limits of the pixel values allowed into a list.
 *
 * Results:
 *	The string representation of the limits is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InfoOper(clientData, interp, argc, argv)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    register Watch *watchPtr;
    Tk_Uid nameId;
    char string[200];
    register char **p;

    nameId = Tk_GetUid(argv[2]);
    watchPtr = FindWatch(interp, nameId, TCL_LEAVE_ERR_MSG);
    if (watchPtr == NULL) {
	return TCL_ERROR;
    }
    if (watchPtr->preCmd != NULL) {
	Tcl_AppendResult(interp, "-precmd", (char *)NULL);
	for (p = watchPtr->preCmd; *p != NULL; p++) {
	    Tcl_AppendResult(interp, " ", *p, (char *)NULL);
	}
    }
    if (watchPtr->postCmd != NULL) {
	Tcl_AppendResult(interp, "-postcmd", (char *)NULL);
	for (p = watchPtr->postCmd; *p != NULL; p++) {
	    Tcl_AppendResult(interp, " ", *p, (char *)NULL);
	}
    }
    sprintf(string, "%d", watchPtr->maxLevel);
    Tcl_AppendResult(interp, "-maxlevel ", string, " ", (char *)NULL);
    Tcl_AppendResult(interp, "-active ",
	(watchPtr->state == WATCH_STATE_ACTIVE)
	? "true" : "false", " ", (char *)NULL);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * WatchCmd --
 *
 *	This procedure is invoked to process the Tcl "blt_watch"
 *	command. See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static Blt_OperSpec operSpecs[] =
{
    {"activate", 1, (Blt_OperProc) ActivateOper, 3, 3, "watchName",},
    {"configure", 2, (Blt_OperProc) ConfigureOper, 3, 0,
	"watchName ?options...?"},
    {"create", 2, (Blt_OperProc) CreateOper, 3, 0, "watchName ?switches?",},
    {"deactivate", 3, (Blt_OperProc) ActivateOper, 3, 3, "watchName",},
    {"delete", 3, (Blt_OperProc) DeleteOper, 3, 3, "watchName",},
    {"info", 1, (Blt_OperProc) InfoOper, 3, 3, "watchName",},
    {"names", 1, (Blt_OperProc) NamesOper, 2, 3, "?state?",},
};
static int numSpecs = sizeof(operSpecs) / sizeof(Blt_OperSpec);

/*ARGSUSED*/
static int
WatchCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Blt_OperProc proc;
    int result;

    proc = Blt_LookupOperation(interp, numSpecs, operSpecs, BLT_OPER_ARG1,
	argc, argv);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, argc, argv);
    return (result);
}


static void
WatchDeleteCmd(clientData) 
    ClientData clientData;	/* unused */
{
    Tcl_DeleteHashTable(&watchTable);
    initialized = FALSE;
}


/* Public initialization routine */
/*
 *--------------------------------------------------------------
 *
 * Blt_WatchInit --
 *
 *	This procedure is invoked to initialize the Tcl command
 *	"blt_watch".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates the new command and adds a new entry into a
 *	global	Tcl associative array.
 *
 *--------------------------------------------------------------
 */
int
Blt_WatchInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = { 
	"watch", WATCH_VERSION, (Tcl_CmdProc*)WatchCmd,
	(Tcl_CmdDeleteProc*)WatchDeleteCmd,
    };

    if (!initialized) {
	Tcl_InitHashTable(&watchTable, sizeof(WatchKey) / sizeof(int));
	initialized = TRUE;
    }
    return (Blt_InitCmd(interp, &cmdSpec));
}
#endif /* !NO_WATCH */
