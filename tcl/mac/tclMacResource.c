/*
 * tclMacResource.c --
 *
 *	This file contains several commands that manipulate or use
 *	Macintosh resources.  Included are extensions to the "source"
 *	command, the mac specific "beep" and "resource" commands, and
 *	administration for open resource file references.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacResource.c 1.23 97/06/13 18:58:59
 */

#include <FSpCompat.h>
#include <Resources.h>
#include <Sound.h>
#include <Strings.h>

#include "tcl.h"
#include "tclInt.h"
#include "tclMac.h"
#include "tclMacInt.h"
#include "tclMacPort.h"

/*
 * Hash table to track open resource files.
 */
static Tcl_HashTable nameTable;		/* Id to process number mapping. */
static Tcl_HashTable resourceTable;	/* Process number to id mapping. */
static int newId = 0;			/* Id source. */
static int initialized = 0;		/* 0 means static structures haven't 
					 * been initialized yet. */
static int osTypeInit = 0;		/* 0 means Tcl object of osType hasn't 
					 * been initialized yet. */
/*
 * Prototypes for procedures defined later in this file:
 */

static void		DupOSTypeInternalRep _ANSI_ARGS_((Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr));
static void		ResourceInit _ANSI_ARGS_((void));
static int		SetOSTypeFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
static void		UpdateStringOfOSType _ANSI_ARGS_((Tcl_Obj *objPtr));

/*
 * The structures below defines the Tcl object type defined in this file by
 * means of procedures that can be invoked by generic object code.
 */

static Tcl_ObjType osType = {
    "ostype",				/* name */
    (Tcl_FreeInternalRepProc *) NULL,   /* freeIntRepProc */
    DupOSTypeInternalRep,	        /* dupIntRepProc */
    UpdateStringOfOSType,		/* updateStringProc */
    SetOSTypeFromAny			/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResourceObjCmd --
 *
 *	This procedure is invoked to process the "resource" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ResourceObjCmd(
    ClientData clientData,		/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[])		/* Argument values. */
{
    Tcl_Obj *resultPtr, *objPtr;
    int index, result;
    long fileRef, rsrcId;
    FSSpec fileSpec;
    Tcl_DString buffer;
    char *nativeName;
    char *stringPtr;
    Tcl_HashEntry *resourceHashPtr;
    Tcl_HashEntry *nameHashPtr;
    Handle resource;
    OSErr err;
    int count, i, limitSearch = false, length;
    short id, saveRef;
    Str255 theName;
    OSType rezType;
    int new, gotInt,releaseIt = 0;
    char *resourceId = NULL;	
    long size;
    char macPermision;
    int mode;

    static char *writeSwitches[] = {"-id", "-name", "-file", (char *) NULL};
    static char *switches[] =
	    {"close", "list", "open", "read", "types", "write", (char *) NULL};

    resultPtr = Tcl_GetObjResult(interp);
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], switches, "option", 0, &index)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (!initialized) {
	ResourceInit();
    }
    result = TCL_OK;

    switch (index) {
	case 0:			/* close */
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "resourceRef");
		return TCL_ERROR;
	    }
	    stringPtr = Tcl_GetStringFromObj(objv[2], &length);
	    nameHashPtr = Tcl_FindHashEntry(&nameTable, stringPtr);
	    if (nameHashPtr == NULL) {
		Tcl_AppendStringsToObj(resultPtr,
			"invalid resource file reference \"",
			stringPtr, "\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    fileRef = (long) Tcl_GetHashValue(nameHashPtr);
	    if (fileRef == 0) {
		Tcl_AppendStringsToObj(resultPtr,
			"can't close system resource", (char *) NULL);
		return TCL_ERROR;
	    }
	    Tcl_DeleteHashEntry(nameHashPtr);
	    resourceHashPtr = Tcl_FindHashEntry(&resourceTable, (char *) fileRef);
	    if (resourceHashPtr == NULL) {
		panic("how did this happen");
	    }
	    ckfree(Tcl_GetHashValue(resourceHashPtr));
	    Tcl_DeleteHashEntry(resourceHashPtr);

	    CloseResFile((short) fileRef);
	    return TCL_OK;
	case 1:			/* list */
	    if (!((objc == 3) || (objc == 4))) {
		Tcl_WrongNumArgs(interp, 2, objv, "resourceType ?resourceRef?");
		return TCL_ERROR;
	    }
	    if (Tcl_GetOSTypeFromObj(interp, objv[2], &rezType) != TCL_OK) {
		return TCL_ERROR;
	    }

	    if (objc == 4) {
		stringPtr = Tcl_GetStringFromObj(objv[3], &length);
		nameHashPtr = Tcl_FindHashEntry(&nameTable, stringPtr);
		if (nameHashPtr == NULL) {
		    Tcl_AppendStringsToObj(resultPtr,
			"invalid resource file reference \"",
			stringPtr, "\"", (char *) NULL);
		    return TCL_ERROR;
		}
		fileRef = (long) Tcl_GetHashValue(nameHashPtr);
		saveRef = CurResFile();
		UseResFile((short) fileRef);
		limitSearch = true;
	    }

	    Tcl_ResetResult(interp);
	    if (limitSearch) {
		count = Count1Resources(rezType);
	    } else {
		count = CountResources(rezType);
	    }
	    SetResLoad(false);
	    for (i = 1; i <= count; i++) {
		if (limitSearch) {
		    resource = Get1IndResource(rezType, i);
		} else {
		    resource = GetIndResource(rezType, i);
		}
		if (resource != NULL) {
		    GetResInfo(resource, &id, (ResType *) &rezType, theName);
		    if (theName[0] != 0) {
			objPtr = Tcl_NewStringObj((char *) theName + 1, theName[0]);
		    } else {
			objPtr = Tcl_NewIntObj(id);
		    }
		    ReleaseResource(resource);
		    result = Tcl_ListObjAppendElement(interp, resultPtr, objPtr);
		    if (result != TCL_OK) {
			Tcl_DecrRefCount(objPtr);
			break;
		    }
		}
	    }
	    SetResLoad(true);
	
	    if (limitSearch) {
		UseResFile(saveRef);
	    }
	
	    return TCL_OK;
	case 2:			/* open */
	    if (!((objc == 3) || (objc == 4))) {
		Tcl_WrongNumArgs(interp, 2, objv, "fileName ?permissions?");
		return TCL_ERROR;
	    }
	    stringPtr = Tcl_GetStringFromObj(objv[2], &length);
	    nativeName = Tcl_TranslateFileName(interp, stringPtr, &buffer);
	    if (nativeName == NULL) {
		return TCL_ERROR;
	    }
	    err = FSpLocationFromPath(strlen(nativeName), nativeName,
		    &fileSpec) ;
	    Tcl_DStringFree(&buffer);
	    if (!((err == noErr) || (err == fnfErr))) {
		Tcl_AppendStringsToObj(resultPtr,
			"invalid path", (char *) NULL);
		return TCL_ERROR;
	    }

	    /*
	     * Get permissions for the file.  We really only understand
	     * read-only and shared-read-write.  If no permissions are given we
	     * default to read only.
	     */
	    
	    if (objc == 4) {
		stringPtr = Tcl_GetStringFromObj(objv[3], &length);
		mode = TclGetOpenMode(interp, stringPtr, &index);
		if (mode == -1) {
		    /* TODO: TclGetOpenMode doesn't work with Obj commands. */
		    return TCL_ERROR;
		}
		switch (mode & (O_RDONLY | O_WRONLY | O_RDWR)) {
		    case O_RDONLY:
			macPermision = fsRdPerm;
		    break;
		    case O_WRONLY:
		    case O_RDWR:
			macPermision = fsRdWrShPerm;
			break;
		    default:
			panic("Tcl_ResourceObjCmd: invalid mode value");
		    break;
		}
	    } else {
		macPermision = fsRdPerm;
	    }
	    
	    fileRef = (long) FSpOpenResFileCompat(&fileSpec, macPermision);
	    if (fileRef == -1) {
	    	err = ResError();
		if (((err == fnfErr) || (err == eofErr)) &&
			(macPermision == fsRdWrShPerm)) {
		    /*
		     * No resource fork existed for this file.  Since we are
		     * opening it for writing we will create the resource fork
		     * now.
		     */
		    HCreateResFile(fileSpec.vRefNum, fileSpec.parID,
			    fileSpec.name);
		    fileRef = (long) FSpOpenResFileCompat(&fileSpec,
			    macPermision);
		    if (fileRef == -1) {
			goto openError;
		    }
		} else if (err == fnfErr) {
		    Tcl_AppendStringsToObj(resultPtr,
			"file does not exist", (char *) NULL);
		    return TCL_ERROR;
		} else if (err == eofErr) {
		    Tcl_AppendStringsToObj(resultPtr,
			"file does not contain resource fork", (char *) NULL);
		    return TCL_ERROR;
		} else {
		    openError:
		    Tcl_AppendStringsToObj(resultPtr,
			"error opening resource file", (char *) NULL);
		    return TCL_ERROR;
		}
	    }
		
	    resourceHashPtr = Tcl_CreateHashEntry(&resourceTable,
			(char *) fileRef, &new);
	    if (!new) {
		resourceId = (char *) Tcl_GetHashValue(resourceHashPtr);
		Tcl_SetStringObj(resultPtr, resourceId, -1);
		return TCL_OK;
	    }
	  	
	    resourceId = (char *) ckalloc(15);
	    sprintf(resourceId, "resource%d", newId);
	    Tcl_SetHashValue(resourceHashPtr, resourceId);
	    newId++;

	    nameHashPtr = Tcl_CreateHashEntry(&nameTable, resourceId, &new);
	    if (!new) {
		panic("resource id has repeated itself");
	    }
	    Tcl_SetHashValue(nameHashPtr, fileRef);
		
	    Tcl_SetStringObj(resultPtr, resourceId, -1);
	    return TCL_OK;
	case 3:			/* read */
	    if (!((objc == 4) || (objc == 5))) {
		Tcl_WrongNumArgs(interp, 2, objv,
			"resourceType resourceId ?resourceRef?");
		return TCL_ERROR;
	    }

	    if (Tcl_GetOSTypeFromObj(interp, objv[2], &rezType) != TCL_OK) {
		return TCL_ERROR;
	    }
	    
	    if (Tcl_GetLongFromObj((Tcl_Interp *) NULL, objv[3], &rsrcId)
		    != TCL_OK) {
		resourceId = Tcl_GetStringFromObj(objv[3], &length);
            }

	    if (objc == 5) {
		stringPtr = Tcl_GetStringFromObj(objv[4], &length);
	    } else {
		stringPtr = NULL;
	    }
	
	    resource = Tcl_MacFindResource(interp, rezType, resourceId,
		rsrcId, stringPtr, &releaseIt);
			    
	    if (resource != NULL) {
		size = GetResourceSizeOnDisk(resource);
		Tcl_SetStringObj(resultPtr, *resource, size);

		/*
		 * Don't release the resource unless WE loaded it...
		 */
		 
		if (releaseIt) {
		    ReleaseResource(resource);
		}
		return TCL_OK;
	    } else {
		Tcl_AppendStringsToObj(resultPtr, "could not load resource",
		    (char *) NULL);
		return TCL_ERROR;
	    }
	case 4:			/* types */
	    if (!((objc == 2) || (objc == 3))) {
		Tcl_WrongNumArgs(interp, 2, objv, "?resourceRef?");
		return TCL_ERROR;
	    }

	    if (objc == 3) {
		stringPtr = Tcl_GetStringFromObj(objv[2], &length);
		nameHashPtr = Tcl_FindHashEntry(&nameTable, stringPtr);
		if (nameHashPtr == NULL) {
		    Tcl_AppendStringsToObj(resultPtr,
			"invalid resource file reference \"",
			stringPtr, "\"", (char *) NULL);
			return TCL_ERROR;
		}
		fileRef = (long) Tcl_GetHashValue(nameHashPtr);
		saveRef = CurResFile();
		UseResFile((short) fileRef);
		limitSearch = true;
	    }

	    if (limitSearch) {
		count = Count1Types();
	    } else {
		count = CountTypes();
	    }
	    for (i = 1; i <= count; i++) {
		if (limitSearch) {
		    Get1IndType((ResType *) &rezType, i);
		} else {
		    GetIndType((ResType *) &rezType, i);
		}
		objPtr = Tcl_NewOSTypeObj(rezType);
		result = Tcl_ListObjAppendElement(interp, resultPtr, objPtr);
		if (result != TCL_OK) {
		    Tcl_DecrRefCount(objPtr);
		    break;
		}
	    }
		
	    if (limitSearch) {
		UseResFile(saveRef);
	    }
		
	    return result;
	case 5:			/* write */
	    if (!((objc >= 4) && (objc <= 10) && ((objc % 2) == 0))) {
		Tcl_WrongNumArgs(interp, 2, objv, 
		    "?-id resourceId? ?-name resourceName? ?-file resourceRef? resourceType data");
		return TCL_ERROR;
	    }
	    
	    i = 2;
	    fileRef = -1;
	    gotInt = false;
	    resourceId = NULL;
	    limitSearch = false;

	    while (i < (objc - 2)) {
		if (Tcl_GetIndexFromObj(interp, objv[i], writeSwitches,
			"option", 0, &index) != TCL_OK) {
		    return TCL_ERROR;
		}

		switch (index) {
		    case 0:			/* -id */
			if (Tcl_GetLongFromObj(interp, objv[i+1], &rsrcId)
				!= TCL_OK) {
			    return TCL_ERROR;
			}
			gotInt = true;
			break;
		    case 1:			/* -name */
			resourceId = Tcl_GetStringFromObj(objv[i+1], &length);
			strcpy((char *) theName, resourceId);
			resourceId = (char *) theName;
			c2pstr(resourceId);
			break;
		    case 2:			/* -file */
			stringPtr = Tcl_GetStringFromObj(objv[i+1], &length);
			nameHashPtr = Tcl_FindHashEntry(&nameTable, stringPtr);
			if (nameHashPtr == NULL) {
			    Tcl_AppendStringsToObj(resultPtr,
				    "invalid resource file reference \"",
				    stringPtr, "\"", (char *) NULL);
			    return TCL_ERROR;
			}
			fileRef = (long) Tcl_GetHashValue(nameHashPtr);
			limitSearch = true;
			break;
		}
		i += 2;
	    }
	    if (Tcl_GetOSTypeFromObj(interp, objv[i], &rezType) != TCL_OK) {
		return TCL_ERROR;
	    }
	    stringPtr = Tcl_GetStringFromObj(objv[i+1], &length);

	    if (gotInt == false) {
		rsrcId = UniqueID(rezType);
	    }
	    if (resourceId == NULL) {
		resourceId = (char *) "\p";
	    }
	    if (limitSearch) {
		saveRef = CurResFile();
		UseResFile((short) fileRef);
	    }
	    
	    resource = NewHandle(length);
	    HLock(resource);
	    memcpy(*resource, stringPtr, length);
	    HUnlock(resource);
	    AddResource(resource, rezType, (short) rsrcId,
		(StringPtr) resourceId);
	    err = ResError();
	    if (err != noErr) {
		SysBeep(1);
	    }
	    WriteResource(resource);
	    err = ResError();
	    if (err != noErr) {
		SysBeep(1);
	    }
	    ReleaseResource(resource);
	    err = ResError();
	    if (err != noErr) {
		SysBeep(1);
	    }
	    
	    if (limitSearch) {
		UseResFile(saveRef);
	    }

	    return result;
	default:
	    return TCL_ERROR;	/* Should never be reached. */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MacSourceObjCmd --
 *
 *	This procedure is invoked to process the "source" Tcl command.
 *	See the user documentation for details on what it does.  In 
 *	addition, it supports sourceing from the resource fork of
 *	type 'TEXT'.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_MacSourceObjCmd(
    ClientData dummy,			/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[])		/* Argument objects. */
{
    char *errNum = "wrong # args: ";
    char *errBad = "bad argument: ";
    char *errStr;
    char *fileName = NULL, *rsrcName = NULL;
    long rsrcID = -1;
    char *string;
    int length;

    if (objc < 2 || objc > 4)  {
    	errStr = errNum;
    	goto sourceFmtErr;
    }
    
    if (objc == 2)  {
	string = TclGetStringFromObj(objv[1], &length);
	return Tcl_EvalFile(interp, string);
    }
    
    /*
     * The following code supports a few older forms of this command
     * for backward compatability.
     */
    string = TclGetStringFromObj(objv[1], &length);
    if (!strcmp(string, "-rsrc") || !strcmp(string, "-rsrcname")) {
	rsrcName = TclGetStringFromObj(objv[2], &length);
    } else if (!strcmp(string, "-rsrcid")) {
	if (Tcl_GetLongFromObj(interp, objv[2], &rsrcID) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
    	errStr = errBad;
    	goto sourceFmtErr;
    }
    
    if (objc == 4) {
	fileName = TclGetStringFromObj(objv[3], &length);
    }
    return Tcl_MacEvalResource(interp, rsrcName, rsrcID, fileName);
	
    sourceFmtErr:
    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), errStr, "should be \"",
		Tcl_GetStringFromObj(objv[0], (int *) NULL),
		" fileName\" or \"",
		Tcl_GetStringFromObj(objv[0], (int *) NULL),
		" -rsrc name ?fileName?\" or \"", 
		Tcl_GetStringFromObj(objv[0], (int *) NULL),
		" -rsrcid id ?fileName?\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_BeepObjCmd --
 *
 *	This procedure makes the beep sound.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Makes a beep.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_BeepObjCmd(
    ClientData dummy,			/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *CONST objv[])		/* Argument values. */
{
    Tcl_Obj *resultPtr, *objPtr;
    Handle sound;
    Str255 sndName;
    int volume = -1, length;
    char * sndArg = NULL;
    long curVolume;

    resultPtr = Tcl_GetObjResult(interp);
    if (objc == 1) {
	SysBeep(1);
	return TCL_OK;
    } else if (objc == 2) {
	if (!strcmp(Tcl_GetStringFromObj(objv[1], &length), "-list")) {
	    int count, i;
	    short id;
	    Str255 theName;
	    ResType rezType;
			
	    count = CountResources('snd ');
	    for (i = 1; i <= count; i++) {
		sound = GetIndResource('snd ', i);
		if (sound != NULL) {
		    GetResInfo(sound, &id, &rezType, theName);
		    if (theName[0] == 0) {
			continue;
		    }
		    objPtr = Tcl_NewStringObj((char *) theName + 1,
			    theName[0]);
		    Tcl_ListObjAppendElement(interp, resultPtr, objPtr);
		}
	    }
	    return TCL_OK;
	} else {
	    sndArg = Tcl_GetStringFromObj(objv[1], &length);
	}
    } else if (objc == 3) {
	if (!strcmp(Tcl_GetStringFromObj(objv[1], &length), "-volume")) {
	    Tcl_GetIntFromObj(interp, objv[2], &volume);
	} else {
	    goto beepUsage;
	}
    } else if (objc == 4) {
	if (!strcmp(Tcl_GetStringFromObj(objv[1], &length), "-volume")) {
	    Tcl_GetIntFromObj(interp, objv[2], &volume);
	    sndArg = Tcl_GetStringFromObj(objv[3], &length);
	} else {
	    goto beepUsage;
	}
    } else {
	goto beepUsage;
    }
	
    /*
     * Set Volume
     */
    if (volume >= 0) {
	GetSysBeepVolume(&curVolume);
	SetSysBeepVolume((short) volume);
    }
	
    /*
     * Play the sound
     */
    if (sndArg == NULL) {
	SysBeep(1);
    } else {
	strcpy((char *) sndName + 1, sndArg);
	sndName[0] = length;
	sound = GetNamedResource('snd ', sndName);
	if (sound != NULL) {
	    SndPlay(NULL, (SndListHandle) sound, false);
	    return TCL_OK;
	} else {
	    if (volume >= 0) {
		SetSysBeepVolume(curVolume);
	    }
	    Tcl_AppendStringsToObj(resultPtr, " \"", sndArg, 
		    "\" is not a valid sound.  (Try ",
		    Tcl_GetStringFromObj(objv[0], (int *) NULL),
		    " -list)", NULL);
	    return TCL_ERROR;
	}
    }

    /*
     * Reset Volume
     */
    if (volume >= 0) {
	SetSysBeepVolume(curVolume);
    }
    return TCL_OK;

    beepUsage:
    Tcl_WrongNumArgs(interp, 1, objv, "[-volume num] [-list | sndName]?");
    return TCL_ERROR;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Tcl_MacEvalResource --
 *
 *	Used to extend the source command.  Sources Tcl code from a Text
 *	resource.  Currently only sources the resouce by name file ID may be
 *	supported at a later date.
 *
 * Side Effects:
 *	Depends on the Tcl code in the resource.
 *
 * Results:
 *      Returns a Tcl result.
 *
 *-----------------------------------------------------------------------------
 */

int
Tcl_MacEvalResource(
    Tcl_Interp *interp,		/* Interpreter in which to process file. */
    char *resourceName,		/* Name of TEXT resource to source,
				   NULL if number should be used. */
    int resourceNumber,		/* Resource id of source. */
    char *fileName)		/* Name of file to process.
				   NULL if application resource. */
{
    Handle sourceText;
    Str255 rezName;
    char msg[200];
    int result;
    short saveRef, fileRef = -1;
    char idStr[64];
    FSSpec fileSpec;
    Tcl_DString buffer;
    char *nativeName;

    saveRef = CurResFile();
	
    if (fileName != NULL) {
	OSErr err;
	
	nativeName = Tcl_TranslateFileName(interp, fileName, &buffer);
	if (nativeName == NULL) {
	    return TCL_ERROR;
	}
	err = FSpLocationFromPath(strlen(nativeName), nativeName, &fileSpec);
	Tcl_DStringFree(&buffer);
	if (err != noErr) {
	    Tcl_AppendResult(interp, "Error finding the file: \"", 
		fileName, "\".", NULL);
	    return TCL_ERROR;
	}
		
	fileRef = FSpOpenResFileCompat(&fileSpec, fsRdPerm);
	if (fileRef == -1) {
	    Tcl_AppendResult(interp, "Error reading the file: \"", 
		fileName, "\".", NULL);
	    return TCL_ERROR;
	}
		
	UseResFile(fileRef);
    } else {
	/*
	 * The default behavior will search through all open resource files.
	 * This may not be the behavior you desire.  If you want the behavior
	 * of this call to *only* search the application resource fork, you
	 * must call UseResFile at this point to set it to the application
	 * file.  This means you must have already obtained the application's 
	 * fileRef when the application started up.
	 */
    }
	
    /*
     * Load the resource by name or ID
     */
    if (resourceName != NULL) {
	strcpy((char *) rezName + 1, resourceName);
	rezName[0] = strlen(resourceName);
	sourceText = GetNamedResource('TEXT', rezName);
    } else {
	sourceText = GetResource('TEXT', (short) resourceNumber);
    }
	
    if (sourceText == NULL) {
	result = TCL_ERROR;
    } else {
	char *sourceStr = NULL;
	
	sourceStr = Tcl_MacConvertTextResource(sourceText);
	ReleaseResource(sourceText);
		
	/*
	 * We now evaluate the Tcl source
	 */
	result = Tcl_Eval(interp, sourceStr);
	ckfree(sourceStr);
	if (result == TCL_RETURN) {
	    result = TCL_OK;
	} else if (result == TCL_ERROR) {
	    sprintf(msg, "\n    (rsrc \"%.150s\" line %d)", resourceName,
		    interp->errorLine);
	    Tcl_AddErrorInfo(interp, msg);
	}
				
	goto rezEvalCleanUp;
    }
	
    rezEvalError:
    sprintf(idStr, "ID=%d", resourceNumber);
    Tcl_AppendResult(interp, "The resource \"",
	    (resourceName != NULL ? resourceName : idStr),
	    "\" could not be loaded from ",
	    (fileName != NULL ? fileName : "application"),
	    ".", NULL);

    rezEvalCleanUp:
    if (fileRef != -1) {
	CloseResFile(fileRef);
    }

    UseResFile(saveRef);
	
    return result;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Tcl_MacConvertTextResource --
 *
 *	Converts a TEXT resource into a Tcl suitable string.
 *
 * Side Effects:
 *	Mallocs the returned memory, converts '\r' to '\n', and appends a NULL.
 *
 * Results:
 *      A new malloced string.
 *
 *-----------------------------------------------------------------------------
 */

char *
Tcl_MacConvertTextResource(
    Handle resource)		/* Handle to TEXT resource. */
{
    int i, size;
    char *resultStr;

    size = SizeResource(resource);
    
    resultStr = ckalloc(size + 1);
    
    for (i=0; i<size; i++) {
	if ((*resource)[i] == '\r') {
	    resultStr[i] = '\n';
	} else {
	    resultStr[i] = (*resource)[i];
	}
    }
    
    resultStr[size] = '\0';

    return resultStr;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Tcl_MacFindResource --
 *
 *	Higher level interface for loading resources.
 *
 * Side Effects:
 *	Attempts to load a resource.
 *
 * Results:
 *      A handle on success.
 *
 *-----------------------------------------------------------------------------
 */

Handle
Tcl_MacFindResource(
    Tcl_Interp *interp,		/* Interpreter in which to process file. */
    long resourceType,		/* Type of resource to load. */
    char *resourceName,		/* Name of resource to source,
				 * NULL if number should be used. */
    int resourceNumber,		/* Resource id of source. */
    char *resFileRef,		/* Registered resource file reference,
				 * NULL if searching all open resource files. */
    int *releaseIt)	        /* Should we release this resource when done. */
{
    Tcl_HashEntry *nameHashPtr;
    long fileRef;
    int limitSearch = false;
    short saveRef;
    Handle resource;

    if (resFileRef != NULL) {
	nameHashPtr = Tcl_FindHashEntry(&nameTable, resFileRef);
	if (nameHashPtr == NULL) {
	    Tcl_AppendResult(interp, "invalid resource file reference \"",
			     resFileRef, "\"", (char *) NULL);
	    return NULL;
	}
	fileRef = (long) Tcl_GetHashValue(nameHashPtr);
	saveRef = CurResFile();
	UseResFile((short) fileRef);
	limitSearch = true;
    }

    /* 
     * Some system resources (for example system resources) should not 
     * be released.  So we set autoload to false, and try to get the resource.
     * If the Master Pointer of the returned handle is null, then resource was 
     * not in memory, and it is safe to release it.  Otherwise, it is not.
     */
    
    SetResLoad(false);
	 
    if (resourceName == NULL) {
	if (limitSearch) {
	    resource = Get1Resource(resourceType, resourceNumber);
	} else {
	    resource = GetResource(resourceType, resourceNumber);
	}
    } else {
	c2pstr(resourceName);
	if (limitSearch) {
	    resource = Get1NamedResource(resourceType, (StringPtr) resourceName);
	} else {
	    resource = GetNamedResource(resourceType, (StringPtr) resourceName);
	}
	p2cstr((StringPtr) resourceName);
    }
    
    if (*resource == NULL) {
    	*releaseIt = 1;
    	LoadResource(resource);
    } else {
    	*releaseIt = 0;
    }
    
    SetResLoad(true);
    	

    if (limitSearch) {
	UseResFile(saveRef);
    }

    return resource;
}

/*
 *----------------------------------------------------------------------
 *
 * ResourceInit --
 *
 *	Initialize the structures used for resource management.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Read the code.
 *
 *----------------------------------------------------------------------
 */

static void
ResourceInit()
{
    Tcl_HashEntry *resourceHashPtr;
    Tcl_HashEntry *nameHashPtr;
    long fileRef;
    char * resourceId;
    int new;

    initialized = 1;
    Tcl_InitHashTable(&nameTable, TCL_STRING_KEYS);
    Tcl_InitHashTable(&resourceTable, TCL_ONE_WORD_KEYS);

    /*
     * Place the application resource file into our cache.
     */
    fileRef = CurResFile();
    resourceHashPtr = Tcl_CreateHashEntry(&resourceTable, (char *) fileRef,
	    &new);
    resourceId = (char *) ckalloc(strlen("application") + 1);
    sprintf(resourceId, "application");
    Tcl_SetHashValue(resourceHashPtr, resourceId);

    nameHashPtr = Tcl_CreateHashEntry(&nameTable, resourceId, &new);
    Tcl_SetHashValue(nameHashPtr, fileRef);

    /*
     * Place the system resource file into our cache.
     */
    fileRef = 0;
    resourceHashPtr = Tcl_CreateHashEntry(&resourceTable, (char *) fileRef,
	    &new);
    resourceId = (char *) ckalloc(strlen("system") + 1);
    sprintf(resourceId, "system");
    Tcl_SetHashValue(resourceHashPtr, resourceId);

    nameHashPtr = Tcl_CreateHashEntry(&nameTable, resourceId, &new);
    Tcl_SetHashValue(nameHashPtr, fileRef);
}
/***/

/*Tcl_RegisterObjType(typePtr) */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewOSTypeObj --
 *
 *	This procedure is used to create a new resource name type object.
 *
 * Results:
 *	The newly created object is returned. This object will have a NULL
 *	string representation. The returned object has ref count 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_NewOSTypeObj(
    OSType newOSType)		/* Int used to initialize the new object. */
{
    register Tcl_Obj *objPtr;

    if (!osTypeInit) {
	osTypeInit = 1;
	Tcl_RegisterObjType(&osType);
    }

    objPtr = Tcl_NewObj();
    objPtr->bytes = NULL;
    objPtr->internalRep.longValue = newOSType;
    objPtr->typePtr = &osType;
    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetOSTypeObj --
 *
 *	Modify an object to be a resource type and to have the 
 *	specified long value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's old string rep, if any, is freed. Also, any old
 *	internal rep is freed. 
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetOSTypeObj(
    Tcl_Obj *objPtr,		/* Object whose internal rep to init. */
    OSType newOSType)		/* Integer used to set object's value. */
{
    register Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    if (!osTypeInit) {
	osTypeInit = 1;
	Tcl_RegisterObjType(&osType);
    }

    Tcl_InvalidateStringRep(objPtr);
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    
    objPtr->internalRep.longValue = newOSType;
    objPtr->typePtr = &osType;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetOSTypeFromObj --
 *
 *	Attempt to return an int from the Tcl object "objPtr". If the object
 *	is not already an int, an attempt will be made to convert it to one.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during conversion, an error message is left in interp->objResult
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	If the object is not already an int, the conversion will free
 *	any old internal representation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetOSTypeFromObj(
    Tcl_Interp *interp, 	/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr,		/* The object from which to get a int. */
    OSType *osTypePtr)		/* Place to store resulting int. */
{
    register int result;
    
    if (!osTypeInit) {
	osTypeInit = 1;
	Tcl_RegisterObjType(&osType);
    }

    if (objPtr->typePtr == &osType) {
	*osTypePtr = objPtr->internalRep.longValue;
	return TCL_OK;
    }

    result = SetOSTypeFromAny(interp, objPtr);
    if (result == TCL_OK) {
	*osTypePtr = objPtr->internalRep.longValue;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DupOSTypeInternalRep --
 *
 *	Initialize the internal representation of an int Tcl_Obj to a
 *	copy of the internal representation of an existing int object. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	"copyPtr"s internal rep is set to the integer corresponding to
 *	"srcPtr"s internal rep.
 *
 *----------------------------------------------------------------------
 */

static void
DupOSTypeInternalRep(
    Tcl_Obj *srcPtr,	/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr)	/* Object with internal rep to set. */
{
    copyPtr->internalRep.longValue = srcPtr->internalRep.longValue;
    copyPtr->typePtr = &osType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetOSTypeFromAny --
 *
 *	Attempt to generate an integer internal form for the Tcl object
 *	"objPtr".
 *
 * Results:
 *	The return value is a standard object Tcl result. If an error occurs
 *	during conversion, an error message is left in interp->objResult
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, an int is stored as "objPtr"s internal
 *	representation. 
 *
 *----------------------------------------------------------------------
 */

static int
SetOSTypeFromAny(
    Tcl_Interp *interp,		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr)		/* The object to convert. */
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string;
    int length;
    long newOSType;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = TclGetStringFromObj(objPtr, &length);

    if (length != 4) {
	if (interp != NULL) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "expected Macintosh OS type but got \"", string, "\"",
		    (char *) NULL);
	}
	return TCL_ERROR;
    }
    newOSType =  *((long *) string);
    
    /*
     * The conversion to resource type succeeded. Free the old internalRep 
     * before setting the new one.
     */

    if ((oldTypePtr != NULL) &&	(oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    
    objPtr->internalRep.longValue = newOSType;
    objPtr->typePtr = &osType;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfOSType --
 *
 *	Update the string representation for an resource type object.
 *	Note: This procedure does not free an existing old string rep
 *	so storage will be lost if this has not already been done. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the int-to-string conversion.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfOSType(
    register Tcl_Obj *objPtr)	/* Int object whose string rep to update. */
{
    objPtr->bytes = ckalloc(5);
    sprintf(objPtr->bytes, "%-4.4s", &(objPtr->internalRep.longValue));
    objPtr->length = 4;
}
