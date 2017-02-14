/*
 * blt.h --
 *
 *	Public interface definitions for BLT, an extension to the
 *	Tk toolkit.
 *
 * Copyright 1993-1997 by AT&T Bell Laboratories.
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the
 * names of AT&T Bell Laboratories any of their entities not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * AT&T disclaims all warranties with regard to this software, including
 * all implied warranties of merchantability and fitness.  In no event
 * shall AT&T be liable for any special, indirect or consequential
 * damages or any damages whatsoever resulting from loss of use, data
 * or profits, whether in an action of contract, negligence or other
 * tortuous action, arising out of or in connection with the use or
 * performance of this software.
 *
 */

#ifndef _BLT_H
#define _BLT_H

/*
 * The release level should be 0 for alpha, 1 for beta, and 2 for
 * final/patch.  The release serial value is the number that follows the
 * "a", "b", or "p" in the patch level; for example, if the patch level
 * is 8.0b2, BLT_RELEASE_SERIAL is 2.  It restarts at 1 whenever the
 * release level is changed, except for the final release which is 0
 * (the first patch will start at 1).
 */

#define BLT_MAJOR_VERSION 	8
#define BLT_MINOR_VERSION 	0
#define BLT_RELEASE_LEVEL	2
#define BLT_RELEASE_SERIAL	0
#define BLT_VERSION		"8.0"
#define BLT_LIB_VERSION		"8.0"
#define BLT_PATCH_LEVEL		"8.0-unoff"

#ifndef BLT_VERSION_ONLY

#ifdef _ANSI_ARGS_
#   undef _ANSI_ARGS_
#endif

#if ((defined(__STDC__) || defined(SABER)) && !defined(NO_PROTOTYPE)) \
   || defined(__cplusplus) || defined(USE_PROTOTYPE)
#   define _ANSI_ARGS_(x)     x
#else
#   define _ANSI_ARGS_(x)     ()
#endif

#ifdef EXTERN
#   undef EXTERN
#endif

#ifdef __cplusplus
#   define EXTERN extern "C"
#else
#   define EXTERN extern
#endif

typedef enum {
    BLT_VECTOR_NOTIFY_UPDATE=1, /* The vector's values has been updated */
    BLT_VECTOR_NOTIFY_DESTROY=2	/* The vector has been destroyed and the client
				 * should no longer use its data (calling 
				 * Blt_FreeVectorId) */
} Blt_VectorNotify;

typedef void (Blt_VectorChangedProc) _ANSI_ARGS_((Tcl_Interp *interp,
	ClientData clientData, Blt_VectorNotify notify));

#ifdef __cplusplus
typedef void *Blt_VectorId;
#else
typedef struct Blt_VectorId *Blt_VectorId;
#endif

typedef struct {
    double *valueArr;		/* Array of values (possibly malloc-ed) */
    int numValues;		/* Number of values in the array */
    int arraySize;		/* Size (in values) of the allocated space */
    double min, max;		/* Minimum and maximum values in the vector */
    int reserved;		/* Reserved for future use */
} Blt_Vector;

EXTERN Blt_VectorId Blt_AllocVectorId _ANSI_ARGS_((Tcl_Interp *interp, 
	char *vecName));
EXTERN void Blt_SetVectorChangedProc _ANSI_ARGS_((Blt_VectorId clientId,
	Blt_VectorChangedProc * proc, ClientData clientData));
EXTERN void Blt_FreeVectorId _ANSI_ARGS_((Blt_VectorId clientId));
EXTERN int Blt_GetVectorById _ANSI_ARGS_((Tcl_Interp *interp,
	Blt_VectorId clientId, Blt_Vector *vecPtr));
EXTERN char *Blt_NameOfVectorId _ANSI_ARGS_((Blt_VectorId clientId));
EXTERN int Blt_VectorNotifyPending _ANSI_ARGS_((Blt_VectorId clientId));

EXTERN int Blt_CreateVector _ANSI_ARGS_((Tcl_Interp *interp, char *vecName,
	int size, Blt_Vector *vecPtr));
EXTERN int Blt_GetVector _ANSI_ARGS_((Tcl_Interp *interp, char *vecName,
	Blt_Vector *vecPtr));
EXTERN int Blt_VectorExists _ANSI_ARGS_((Tcl_Interp *interp, char *vecName));
EXTERN int Blt_ResetVector _ANSI_ARGS_((Tcl_Interp *interp,  char *vecName,
	Blt_Vector *vecPtr, Tcl_FreeProc *freeProc));
EXTERN int Blt_ResizeVector _ANSI_ARGS_((Tcl_Interp *interp, char *vecName,
	int newSize));
EXTERN int Blt_DeleteVector _ANSI_ARGS_((Tcl_Interp *interp, char *vecName));

EXTERN int Blt_Init _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Blt_SafeInit _ANSI_ARGS_((Tcl_Interp *interp));

#endif /* !BLT_VERSION_ONLY */
#endif /* !_BLT_H*/
