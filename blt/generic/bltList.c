/*
 * bltList.c --
 *
 *	Generic linked list management routines.
 *
 * Copyright 1991-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"

/*
 *----------------------------------------------------------------------
 *
 * Blt_InitList --
 *
 *	Initialized a linked list.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_InitList(listPtr, type)
    Blt_List *listPtr;
    int type;
{

    listPtr->numEntries = 0;
    listPtr->headPtr = listPtr->tailPtr = (Blt_ListItem *)NULL;
    listPtr->type = type;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_NewList --
 *
 *	Creates a new linked list structure and initializes its pointers
 *
 * Results:
 *	Returns a pointer to the newly created list structure.
 *
 *----------------------------------------------------------------------
 */
Blt_List *
Blt_NewList(type)
    int type;
{
    Blt_List *listPtr;

    listPtr = (Blt_List *)ckalloc(sizeof(Blt_List));
    if (listPtr != NULL) {
	Blt_InitList(listPtr, type);
    }
    return (listPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_NewItem --
 *
 *	Creates a list entry holder.  This routine does not insert
 *	the entry into the list, nor does it no attempt to maintain
 *	consistency of the keys.  For example, more than one entry
 *	may use the same key.
 *
 * Results:
 *	The return value is the pointer to the newly created entry.
 *
 * Side Effects:
 *	The key is not copied, only the Uid is kept.  It is assumed
 *	this key will not change in the life of the entry.
 *
 *----------------------------------------------------------------------
 */
Blt_ListItem *
Blt_NewItem(key)
    char *key;			/* Unique key to reference object */
{
    register Blt_ListItem *iPtr;

    iPtr = (Blt_ListItem *)ckalloc(sizeof(Blt_ListItem));
    if (iPtr == (Blt_ListItem *)NULL) {
	Panic("can't allocate list item structure");
    }
    iPtr->keyPtr = key;
    iPtr->clientData = (ClientData)NULL;
    iPtr->nextPtr = iPtr->prevPtr = (Blt_ListItem *)NULL;
    iPtr->listPtr = (Blt_List *)NULL;
    return (iPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_FreeItem --
 *
 *	Free the memory allocated for the entry.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_FreeItem(iPtr)
    Blt_ListItem *iPtr;
{
    ckfree((char *)iPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ResetList --
 *
 *	Removes all the entries from a list, removing pointers to the
 *	objects and keys (not the objects or keys themselves).
 *	The entry counter is reset to zero.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_ResetList(listPtr)
    Blt_List *listPtr;		/* List to clear */
{
    register Blt_ListItem *oldPtr;
    register Blt_ListItem *iPtr = listPtr->headPtr;

    while (iPtr != (Blt_ListItem *)NULL) {
	oldPtr = iPtr;
	iPtr = iPtr->nextPtr;
	Blt_FreeItem(oldPtr);
    }
    Blt_InitList(listPtr, listPtr->type);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_DeleteList
 *
 *     Frees all list structures
 *
 * Results:
 *	Returns a pointer to the newly created list structure.
 *
 *----------------------------------------------------------------------
 */
void
Blt_DeleteList(listPtr)
    Blt_List *listPtr;
{
    Blt_ResetList(listPtr);
    ckfree((char *)listPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_LinkAfter --
 *
 *	Inserts an entry following a given entry.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_LinkAfter(listPtr, iPtr, afterPtr)
    Blt_List *listPtr;
    Blt_ListItem *iPtr;
    Blt_ListItem *afterPtr;
{
    /*
     * If the list keys are strings, change the key to a Tk_Uid
     */
    if (listPtr->type == TCL_STRING_KEYS) {
	iPtr->keyPtr = Tk_GetUid(iPtr->keyPtr);
    }
    if (listPtr->headPtr == (Blt_ListItem *)NULL) {
	listPtr->tailPtr = listPtr->headPtr = iPtr;
    } else {
	if (afterPtr == (Blt_ListItem *)NULL) {
	    afterPtr = listPtr->tailPtr;
	}
	iPtr->nextPtr = afterPtr->nextPtr;
	iPtr->prevPtr = afterPtr;
	if (afterPtr == listPtr->tailPtr) {
	    listPtr->tailPtr = iPtr;
	} else {
	    afterPtr->nextPtr->prevPtr = iPtr;
	}
	afterPtr->nextPtr = iPtr;
    }
    iPtr->listPtr = listPtr;
    listPtr->numEntries++;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_LinkBefore --
 *
 *	Inserts an entry preceding a given entry.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_LinkBefore(listPtr, iPtr, beforePtr)
    Blt_List *listPtr;		/* List to contain new entry */
    Blt_ListItem *iPtr;	/* New entry to be inserted */
    Blt_ListItem *beforePtr;	/* Entry to link before */
{
    /*
     * If the list keys are strings, change the key to a Tk_Uid
     */
    if (listPtr->type == TCL_STRING_KEYS) {
	iPtr->keyPtr = Tk_GetUid(iPtr->keyPtr);
    }
    if (listPtr->headPtr == (Blt_ListItem *)NULL) {
	listPtr->tailPtr = listPtr->headPtr = iPtr;
    } else {
	if (beforePtr == (Blt_ListItem *)NULL) {
	    beforePtr = listPtr->headPtr;
	}
	iPtr->prevPtr = beforePtr->prevPtr;
	iPtr->nextPtr = beforePtr;
	if (beforePtr == listPtr->headPtr) {
	    listPtr->headPtr = iPtr;
	} else {
	    beforePtr->prevPtr->nextPtr = iPtr;
	}
	beforePtr->prevPtr = iPtr;
    }
    iPtr->listPtr = listPtr;
    listPtr->numEntries++;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_UnlinkItem --
 *
 *	Unlinks an entry from the given list. The entry itself is
 *	not deallocated, but only removed from the list.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_UnlinkItem(iPtr)
    Blt_ListItem *iPtr;
{
    Blt_List *listPtr;

    listPtr = iPtr->listPtr;
    if (listPtr != NULL) {
	if (listPtr->headPtr == iPtr) {
	    listPtr->headPtr = iPtr->nextPtr;
	}
	if (listPtr->tailPtr == iPtr) {
	    listPtr->tailPtr = iPtr->prevPtr;
	}
	if (iPtr->nextPtr != NULL) {
	    iPtr->nextPtr->prevPtr = iPtr->prevPtr;
	}
	if (iPtr->prevPtr != NULL) {
	    iPtr->prevPtr->nextPtr = iPtr->nextPtr;
	}
	iPtr->listPtr = NULL;
	listPtr->numEntries--;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_FindItem --
 *
 *	Find the first entry matching the key given.
 *
 * Results:
 *	Returns the pointer to the entry.  If no entry matching
 *	the key given is found, then NULL is returned.
 *
 *----------------------------------------------------------------------
 */
Blt_ListItem *
Blt_FindItem(listPtr, searchKey)
    Blt_List *listPtr;		/* List to search */
    char *searchKey;		/* Key to match */
{
    register Blt_ListItem *iPtr;
    Tk_Uid newPtr;

    newPtr = searchKey;
    if (listPtr->type == TCL_STRING_KEYS) {
	newPtr = Tk_GetUid(searchKey);
    }
    for (iPtr = listPtr->headPtr; iPtr != NULL;
	iPtr = iPtr->nextPtr) {
	if (newPtr == iPtr->keyPtr)
	    return (iPtr);
    }
    return (Blt_ListItem *) NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_DeleteItem --
 *
 *	Find the entry and free the memory allocated for the entry.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Blt_DeleteItem(iPtr)
    Blt_ListItem *iPtr;
{
    Blt_UnlinkItem(iPtr);
    Blt_FreeItem(iPtr);
}
