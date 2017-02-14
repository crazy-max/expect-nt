/*
 * expUnixPort.h --
 *
 *	This header file handles porting issues that occur because of
 *	differences between Windows and Unix. 
 *
 * Copyright (c) 1997 Mitel, Inc.
 * Copyright (c) 1997 Gordon Chaffee
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef _EXPUNIXPORT_H
#define _EXPUNIXPORT_H

#include "expect_cf.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
  
#endif /* _EXPUNIXPORT_H */

