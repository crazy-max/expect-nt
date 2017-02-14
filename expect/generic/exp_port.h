/*
 * exp_port.h --
 *
 *	This header file handles porting issues that occur because
 *	of differences between systems.  It reads in platform specific
 *	portability files.
 *
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef _EXP_PORT_H
#define _EXP_PORT_H

#define HAVE_MEMCPY

#if defined(__WIN32__) || defined(_WIN32)
#   include "../win/expWinPort.h"
#else
#   if defined(MAC_TCL)
#	include "expPort.h"
#    else
#	include "../unix/expUnixPort.h"
#    endif
#endif

#endif /* _EXP_PORT_H */
