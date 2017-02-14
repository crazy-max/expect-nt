/*
 * bltList.h --
 *
 *	This file contains declarations needed to implement BLT's
 *	list structures.
 *
 * Copyright (c) 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _BLTLIST_H
#define _BLTLIST_H

struct Blt_List;
/*
 * A Blt_ListItem is the container structure for the Blt_List.
 */
typedef struct Blt_ListItem {
    struct Blt_ListItem *prevPtr;	/* Link to the previous item */
    struct Blt_ListItem *nextPtr;	/* Link to the next item */
    Tk_Uid keyPtr;		/* Pointer to the (character string) key */
    ClientData clientData;	/* Pointer to the data object */
    struct Blt_List *listPtr;
} Blt_ListItem;

/*
 * A Blt_List is a doubly chained list structure.
 */
typedef struct Blt_List {
    Blt_ListItem *headPtr;	/* Pointer to first element in list */
    Blt_ListItem *tailPtr;	/* Pointer to last element in list */
    int numEntries;		/* Number of elements in list */
    int type;			/* Type of keys in list */
} Blt_List;

EXTERN void Blt_InitList _ANSI_ARGS_((Blt_List *listPtr, int type));
EXTERN Blt_List *Blt_NewList _ANSI_ARGS_((int type));
EXTERN void Blt_DeleteList _ANSI_ARGS_((Blt_List *listPtr));
EXTERN Blt_ListItem *Blt_NewItem _ANSI_ARGS_((char *key));
EXTERN void Blt_FreeItem _ANSI_ARGS_((Blt_ListItem *itemPtr));
EXTERN void Blt_ResetList _ANSI_ARGS_((Blt_List *listPtr));
EXTERN void Blt_LinkAfter _ANSI_ARGS_((Blt_List *listPtr,
	Blt_ListItem *itemPtr, Blt_ListItem *afterPtr));
EXTERN void Blt_LinkBefore _ANSI_ARGS_((Blt_List *listPtr,
	Blt_ListItem *itemPtr, Blt_ListItem *beforePtr));
EXTERN void Blt_UnlinkItem _ANSI_ARGS_((Blt_ListItem *itemPtr));
EXTERN Blt_ListItem *Blt_FindItem _ANSI_ARGS_((Blt_List *listPtr,
	char *name));
EXTERN void Blt_DeleteItem _ANSI_ARGS_((Blt_ListItem *itemPtr));

#define Blt_GetListLength(listPtr)	((listPtr)->numEntries)
#define Blt_FirstListItem(listPtr)	((listPtr)->headPtr)
#define Blt_LastListItem(listPtr)	((listPtr)->tailPtr)
#define Blt_PrevItem(itemPtr)		((itemPtr)->prevPtr)
#define Blt_NextItem(itemPtr) 		((itemPtr)->nextPtr)
#define Blt_GetItemKey(itemPtr)    	((itemPtr)->keyPtr)
#define Blt_GetItemValue(itemPtr)  	((itemPtr)->clientData)
#define Blt_SetItemValue(itemPtr, valuePtr) \
	((itemPtr)->clientData = (ClientData)(valuePtr))

#endif /* !_BLTLIST_H */
