/*
 * bltTile.c --
 *
 *	This module implements a utility to convert images into
 *	tiles.
 *
 * Copyright 1995-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "bltInt.h"
#include <X11/Xutil.h>

#define TILE_MAGIC ((unsigned int) 0x46170277)

static Tcl_HashTable tileTable;
static int initialized = 0;

typedef struct {
    Tk_Uid nameId;		/* Identifier of image from which the
				 * tile was generated. */
    Display *display;		/* Display where pixmap was created */
    int depth;			/* Depth of pixmap */
    int screenNum;		/* Screen number of pixmap */

    Pixmap pixmap;		/* Pixmap generated from image */
    Tk_Image image;		/* Token of image */
    int width, height;		/* Dimensions of the tile. */

    Blt_List clients;		/* List of clients sharing this tile */

} TileMaster;

typedef struct {
    Tk_Uid nameId;		/* Identifier of image from which the
				 * tile was generated. */
    Display *display;
} TileKey;


typedef struct {
    unsigned int magic;
    Blt_TileChangedProc *changeProc; 
				/* If non-NULL, routine to
				 * call to when tile image changes. */
    ClientData clientData;	/* Data to pass to when calling the above
				 * routine */
    GC *gcPtr;			/* Pointer to GC, to be updated when the
				 * tile changes */
    TileMaster *masterPtr;	/* Pointer to actual tile information */
    Blt_ListItem *itemPtr;	/* Pointer to client entry in the master's
				 * client list.  Used to delete the client */
} Tile;

static int TileParseProc _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	Tk_Window tkwin, char *value, char *widgRec, int flags));
static char *TilePrintProc _ANSI_ARGS_((ClientData clientData, Tk_Window tkwin,
	char *widgRec, int offset, Tcl_FreeProc **freeProcPtr));

Tk_CustomOption bltTileOption =
{
    TileParseProc, TilePrintProc, (ClientData)0
};

/*
 * ----------------------------------------------------------------------
 *
 * Tk_ImageDeleted --
 *
 *	Is there another way to determine if an image has been 
 *	deleted?  
 *
 * Results:
 *	Returns 1 if the image has been deleted, 0 otherwise.
 *
 * ----------------------------------------------------------------------
 */

/*
 * Each call to Tk_GetImage returns a pointer to one of the following
 * structures, which is used as a token by clients (widgets) that
 * display images.
 */
typedef struct Image {
    Tk_Window tkwin;		/* Window passed to Tk_GetImage (needed to
				 * "re-get" the image later if the manager
				 * changes). */
    Display *display;		/* Display for tkwin.  Needed because when
				 * the image is eventually freed tkwin may
				 * not exist anymore. */
    struct ImageMaster *masterPtr;
				/* Master for this image (identifiers image
				 * manager, for example). */
    ClientData instanceData;
				/* One word argument to pass to image manager
				 * when dealing with this image instance. */
    Tk_ImageChangedProc *changeProc;
				/* Code in widget to call when image changes
				 * in a way that affects redisplay. */
    ClientData widgetClientData;
				/* Argument to pass to changeProc. */
    struct Image *nextPtr;	/* Next in list of all image instances
				 * associated with the same name. */

} Image;

/*
 * For each image master there is one of the following structures,
 * which represents a name in the image table and all of the images
 * instantiated from it.  Entries in mainPtr->imageTable point to
 * these structures.
 */
typedef struct ImageMaster {
    Tk_ImageType *typePtr;	/* Information about image type.  NULL means
				 * that no image manager owns this image:  the
				 * image was deleted. */
    ClientData masterData;	/* One-word argument to pass to image mgr
				 * when dealing with the master, as opposed
				 * to instances. */
    int width, height;		/* Last known dimensions for image. */
    Tcl_HashTable *tablePtr;	/* Pointer to hash table containing image
				 * (the imageTable field in some TkMainInfo
				 * structure). */
    Tcl_HashEntry *hPtr;	/* Hash entry in mainPtr->imageTable for
				 * this structure (used to delete the hash
				 * entry). */
    Image *instancePtr;		/* Pointer to first in list of instances
				 * derived from this name. */
} ImageMaster;

static int
Tk_ImageDeleted(image)
    Tk_Image image;		/* Token for image. */
{
    Image *imagePtr = (Image *) image;

    if (imagePtr->masterPtr->typePtr == NULL) {
	return 1;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TileChangedProc
 *
 *	It would be better if Tk checked for NULL proc pointers.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static void
TileChangedProc(clientData, x, y, width, height, imageWidth, imageHeight)
    ClientData clientData;
    int x, y, width, height;	/* Not used */
    int imageWidth, imageHeight;
{
    TileMaster *masterPtr = (TileMaster *)clientData;
    Tile *tilePtr;
    Blt_ListItem *itemPtr;

    if (Tk_ImageDeleted(masterPtr->image)) {
	if (masterPtr->pixmap != None) {
	    Tk_FreePixmap(masterPtr->display, masterPtr->pixmap);
	}
	masterPtr->pixmap = None;
    } else { 
	/*
	 * If the size of the current image differs from the current pixmap,
	 * destroy the pixmap and create a new one of the proper size
	 */
	if ((masterPtr->width != imageWidth) || 
	    (masterPtr->height != imageHeight)) {
	    Pixmap pixmap;
	    Window root;
	    
	    if (masterPtr->pixmap != None) {
		Tk_FreePixmap(masterPtr->display, masterPtr->pixmap);
	    }
	    root = RootWindow(masterPtr->display, masterPtr->screenNum);
	    pixmap = Tk_GetPixmap(masterPtr->display, root, imageWidth, 
		imageHeight, masterPtr->depth);
	    masterPtr->width = imageWidth;
	    masterPtr->height = imageHeight;
	    masterPtr->pixmap = pixmap;
	}
	Tk_RedrawImage(masterPtr->image, 0, 0, imageWidth, imageHeight,
	    masterPtr->pixmap, 0, 0);
    }
    /*
     * Now call back each of the tile clients to notify them that the
     * pixmap has changed.
     */
    for (itemPtr = Blt_FirstListItem(&(masterPtr->clients)); itemPtr != NULL; 
	itemPtr = Blt_NextItem(itemPtr)) {
	tilePtr = (Tile *)Blt_GetItemValue(itemPtr);
	if (tilePtr->changeProc != NULL) {
	    (*tilePtr->changeProc) (tilePtr->clientData, (Blt_Tile)tilePtr,
			tilePtr->gcPtr);
	}
    }
}


static void
InitTables()
{
    Tcl_InitHashTable(&tileTable, sizeof(TileKey) / sizeof(int));
    initialized = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetTile
 *
 *	Convert the named image into a tile.
 *
 * Results:
 *	If the image is valid, a new tile is returned.  If the name
 *	does not represent a proper image, an error message is left in
 *	interp->result.
 *
 * Side Effects:
 *	Memory and X resources are allocated.  Bookkeeping is
 *	maintained on the tile (i.e. width, height, and name).
 *
 *----------------------------------------------------------------------
 */
Blt_Tile 
Blt_GetTile(interp, tkwin, imageName)
    Tcl_Interp *interp;		/* Interpreter to report results back to */
    Tk_Window tkwin;		/* Window on the same display as tile */
    char *imageName;		/* Name of image */
{
    Tcl_HashEntry *hPtr;
    Blt_ListItem *itemPtr;
    Tile *tilePtr;
    int isNew;
    TileKey key;
    TileMaster *masterPtr;

    if (!initialized) {
	InitTables();
    }
    tilePtr = (Tile *)ckcalloc(1, sizeof(Tile));
    if (tilePtr == NULL) {
	Panic("can't allocate Tile client structure");
    }
    /* Initialize client information (Remember to set the itemPtr) */
    tilePtr->magic = TILE_MAGIC;

    /* Find (or create) the master entry for the tile */
    key.nameId = Tk_GetUid(imageName);
    key.display = Tk_Display(tkwin);
    hPtr = Tcl_CreateHashEntry(&tileTable, (char *)&key, &isNew);

    if (isNew) {
	Tk_Image image;
	int width, height;
	Pixmap pixmap;
	Window root;

	masterPtr = (TileMaster *)ckalloc(sizeof(TileMaster));
	if (masterPtr == NULL) {
	    Panic("can't allocate Tile master structure");
	}

	/*
	 * Initialize the (master) bookkeeping on the tile.
	 */
	masterPtr->nameId = key.nameId;
	masterPtr->depth = Tk_Depth(tkwin);
	masterPtr->screenNum = Tk_ScreenNumber(tkwin);
	masterPtr->display = Tk_Display(tkwin);

	/*
	 * Get the image. Funnel all change notifications to a single routine.
	 */
	image = Tk_GetImage(interp, tkwin, imageName,
	    (Tk_ImageChangedProc*)TileChangedProc, (ClientData)masterPtr);
	if (image == NULL) {
	    ckfree((char *)masterPtr);
	    ckfree((char *)tilePtr);
	    return NULL;
	}

	/*
	 * Create a pixmap the same size and draw the image into it.
	 */
	Tk_SizeOfImage(image, &width, &height);
	root = RootWindow(masterPtr->display, masterPtr->screenNum);
	pixmap = Tk_GetPixmap(masterPtr->display, root, width, height,
	    masterPtr->depth);
	Tk_RedrawImage(image, 0, 0, width, height, pixmap, 0, 0);

	masterPtr->width = width;
	masterPtr->height = height;
	masterPtr->pixmap = pixmap;
	masterPtr->image = image;
	Blt_InitList(&(masterPtr->clients), TCL_ONE_WORD_KEYS);
	Tcl_SetHashValue(hPtr, (ClientData)masterPtr);
    } else {
	masterPtr = (TileMaster *)Tcl_GetHashValue(hPtr);
    }
    itemPtr = Blt_NewItem(key.nameId);
    Blt_SetItemValue(itemPtr, (ClientData)tilePtr);
    Blt_LinkAfter(&(masterPtr->clients), itemPtr, (Blt_ListItem *)NULL);
    tilePtr->itemPtr = itemPtr;
    tilePtr->masterPtr = masterPtr;
    return (Blt_Tile)tilePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_FreeTile
 *
 *	Release the resources associated with the tile.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Memory and X resources are freed.  Bookkeeping information
 *	about the tile (i.e. width, height, and name) is discarded.
 *
 *----------------------------------------------------------------------
 */
void
Blt_FreeTile(tile)
    Blt_Tile tile;		/* Tile to be deleted */
{
    Tile *tilePtr = (Tile *)tile;
    TileMaster *masterPtr;

    if (!initialized) {
	InitTables();
    }
    if ((tilePtr == NULL) || (tilePtr->magic != TILE_MAGIC)) {
	return;			/* No tile */
    }
    masterPtr = tilePtr->masterPtr;

    /* Remove the client from the master tile's list */
    Blt_DeleteItem(tilePtr->itemPtr);
    ckfree((char *)tilePtr);

    /*
     * If there are no more clients of the tile, then remove the
     * pixmap, image, and the master record.
     */
    if (masterPtr->clients.numEntries == 0) {
	Tcl_HashEntry *hPtr;
	TileKey key;

	key.nameId = masterPtr->nameId;
	key.display = masterPtr->display;
	hPtr = Tcl_FindHashEntry(&tileTable, (char *)&key);
	if (hPtr != NULL) {
	    Tcl_DeleteHashEntry(hPtr);
	}
	if (masterPtr->pixmap != None) {
	    Tk_FreePixmap(masterPtr->display, masterPtr->pixmap);
	}
	Tk_FreeImage(masterPtr->image);
	ckfree((char *)masterPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_NameOfTile
 *
 *	Returns the name of the image from which the tile was
 *	generated.
 *
 * Results:
 *	The name of the image is returned.  The name is not unique.
 *	Many tiles may use the same image.
 *
 *----------------------------------------------------------------------
 */
char *
Blt_NameOfTile(tile)
    Blt_Tile tile;		/* Tile to query */
{
    Tile *tilePtr = (Tile *)tile;

    if (tilePtr == NULL) {
	return "";
    }
    if (tilePtr->magic != TILE_MAGIC) {
	return "not a tile";
    }
    return (tilePtr->masterPtr->nameId);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_PixmapOfTile
 *
 *	Returns the pixmap of the tile.
 *
 * Results:
 *	The X pixmap used as the tile is returned.
 *
 *----------------------------------------------------------------------
 */
Pixmap
Blt_PixmapOfTile(tile)
    Blt_Tile tile;		/* Tile to query */
{
    Tile *tilePtr = (Tile *)tile;

    if ((tilePtr == NULL) || (tilePtr->magic != TILE_MAGIC)) {
	return None;
    }
    return (tilePtr->masterPtr->pixmap);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_SizeOfTile
 *
 *	Returns the width and height of the tile.
 *
 * Results:
 *	The width and height of the tile are returned.
 *
 *----------------------------------------------------------------------
 */
void
Blt_SizeOfTile(tile, widthPtr, heightPtr)
    Blt_Tile tile;		/* Tile to query */
    int *widthPtr, *heightPtr;	/* Returned dimensions of the tile (out) */
{
    Tile *tilePtr = (Tile *)tile;

    if ((tilePtr == NULL) || (tilePtr->magic != TILE_MAGIC)) {
	*widthPtr = *heightPtr = 0;
	return;			/* No tile given */
    }
    *widthPtr = tilePtr->masterPtr->width;
    *heightPtr = tilePtr->masterPtr->height;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_SetTileChangedProc
 *
 *	Sets the routine to called when an image changes.  If
 *	*changeProc* is NULL, no callback will be performed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The designated routine will be called the next time the
 *	image associated with the tile changes.
 *
 *----------------------------------------------------------------------
 */
void
Blt_SetTileChangedProc(tile, changeProc, clientData, gcPtr)
    Blt_Tile tile;		/* Tile to query */
    Blt_TileChangedProc *changeProc;
    ClientData clientData;
    GC *gcPtr;
{
    Tile *tilePtr = (Tile *)tile;

    if ((tilePtr != NULL) && (tilePtr->magic == TILE_MAGIC)) {
	tilePtr->changeProc = changeProc;
	tilePtr->clientData = clientData;
	tilePtr->gcPtr = gcPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TileParseProc --
 *
 *	Converts the name of an image into a tile.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TileParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;	/* not used */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window on same display as tile */
    char *value;		/* Name of image */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of tile in record */
{
    Tile **tilePtrPtr = (Tile **)(widgRec + offset);
    Blt_Tile tile, lastTile;

    lastTile = (Blt_Tile)*tilePtrPtr;
    tile = NULL;
    if ((value != NULL) && (*value != '\0')) {
	tile = Blt_GetTile(interp, tkwin, value);
	if (tile == NULL) {
	    return TCL_ERROR;
	}
    }	
    if (lastTile != NULL) {
	Blt_FreeTile(lastTile);
    }
    *tilePtrPtr = (Tile *)tile;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TilePrintProc --
 *
 *	Returns the name of the tile.
 *
 * Results:
 *	The name of the tile is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
TilePrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* not used */
    Tk_Window tkwin;		/* not used */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of tile in record */
    Tcl_FreeProc **freeProcPtr;	/* not used */
{
    Blt_Tile tile = *(Blt_Tile *)(widgRec + offset);

    return (Blt_NameOfTile(tile));
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_SetTileOrigin --
 *
 *	Set the pattern origin of the tile to a common point (i.e. the
 *	origin (0,0) of the top level window) so that tiles from two
 *	different widgets will match up.  This done by setting the
 *	GCTileStipOrigin field is set to the translated origin of the
 *	toplevel window in the hierarchy.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The GCTileStipOrigin is reset in the GC.  This will cause the
 *	tile origin to change when the GC is used for drawing.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
void
Blt_SetTileOrigin(tkwin, gc, x, y)
    Tk_Window tkwin;
    GC gc;
    int x, y;
{
    while (!Tk_IsTopLevel(tkwin)) {
	x += Tk_X(tkwin) + Tk_Changes(tkwin)->border_width;
	y += Tk_Y(tkwin) + Tk_Changes(tkwin)->border_width;
	tkwin = Tk_Parent(tkwin);
    }
    XSetTSOrigin(Tk_Display(tkwin), gc, -x, -y);
}
