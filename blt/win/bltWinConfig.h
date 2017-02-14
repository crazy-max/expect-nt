/* src/bltConfig.h.  Generated automatically by configure.  */
/* src/bltConfig.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
/*#define HAVE_SYS_WAIT_H 1*/

/* Define to `int' if <sys/types.h> doesn't define.  */
#define pid_t int

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
/*#define TIME_WITH_SYS_TIME 1*/


/* Define if DBL_EPSILON is not defined in float.h */
/* #undef BLT_DBL_EPSILON */

/* Define if strcasecmp isn't declared in a standard header file. */
/* #undef NO_DECL_STRCASECMP */

/* Define if strdup isn't declared in a standard header file. */
/* #undef NO_DECL_STRDUP */

/* Define if compiler can't handle long strings.  */
/* #undef NO_INLINE_PS_PROLOG */

/* Define if union wait type is defined incorrectly.  */
/* #undef NO_UNION_WAIT */

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <float.h> header file.  */
#define HAVE_FLOAT_H 1

/* Define if you have the <itcl.h> header file.  */
/* #undef HAVE_ITCL_H */

/* Define if you have the <itk.h> header file.  */
/* #undef HAVE_ITK_H */

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <malloc.h> header file.  */
#define HAVE_MALLOC_H 1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/time.h> header file.  */
/*#define HAVE_SYS_TIME_H 1*/

/* Define if you have the <sys/wait.h> header file.  */
/*#define HAVE_SYS_WAIT_H 1*/

/* Define if you have the <tkInt.h> header file.  */
#define HAVE_TKINT_H 1

/* Define if you have the <unistd.h> header file.  */
/*#define HAVE_UNISTD_H 1*/

/* Define if you have the <waitflags.h> header file.  */
/*#define HAVE_WAITFLAGS_H 1*/

/* This is to avoid clashing with the Win32 API. */
#define Pie bltPie

/*  Define the NO_-flags for widgets/commands you're not interested
 *  in or which are not yet available under Win32.
 */
#define	NO_BELL		/* command is obsolete */
#define	NO_CUTBUFFER	/* command is obsolete */
#define	NO_BUSY		/* not yet available */
#undef	NO_BGEXEC	/* not yet available */
#undef	NO_DRAGDROP
#undef	NO_HTEXT
#undef	NO_GRAPH
#undef	NO_TABLE
#undef	NO_WINOP
#undef	NO_BITMAP
#undef  NO_PIECHART
#undef	NO_DEBUG
#undef	NO_WATCH
#undef	NO_VECTOR
#undef	NO_SPLINE
#undef	NO_TILEFRAME
#undef	NO_TILEBUTTON
#undef	NO_TILESCROLLBAR
#undef	NO_TED
