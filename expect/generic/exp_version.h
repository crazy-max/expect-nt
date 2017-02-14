#ifndef _EXP_VERSION_H
#define _EXP_VERSION_H

#define EXP_MAJOR_VERSION   5
#define EXP_MINOR_VERSION   21
#define EXP_MICRO_VERSION   7
#define EXP_RELEASE_LEVEL   0
#define EXP_RELEASE_SERIAL  1

#define EXP_VERSION	   "5.21"
#define EXP_PATCH_LEVEL    "5.21.7 (NT a1)"

#ifndef __WIN32__
#   if defined(_WIN32) || defined(WIN32)
#	define __WIN32__
#   endif
#endif

#ifdef __WIN32__
#   ifndef STRINGIFY
#	define STRINGIFY(x)	    STRINGIFY1(x)
#	define STRINGIFY1(x)	    #x
#   endif
#endif

#endif
