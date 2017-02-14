/*
 * exp_log.c --
 *
 *	logging routines and other things common to both Expect
 *	program and library.  Note that this file must NOT have any
 *	references to Tcl except for including tclInt.h
 *
 * Written by Don Libes
 *
 *
 * Windows NT port by Gordon Chaffee
 * Copyright (c) 1997 by Mitel Corporations
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "exp_port.h"
#include "tcl.h"
#include "tclInt.h"
#include "expect_comm.h"
#include "exp_int.h"
#include "exp_rename.h"
#include "exp_log.h"

#ifdef _MSC_VER
#  define vsnprintf _vsnprintf
#endif

int loguser = TRUE;		/* if TRUE, expect/spawn may write to stdout */
int logfile_all = FALSE;	/* if TRUE, write log of all interactions */
				/* despite value of loguser. */
Tcl_Channel logfile = NULL;
Tcl_Channel debugfile = NULL;

int exp_is_debugging = FALSE;

/* Following this are several functions that log the conversation. */
/* Most of them have multiple calls to printf-style functions.  */
/* At first glance, it seems stupid to reformat the same arguments again */
/* but we have no way of telling how long the formatted output will be */
/* and hence cannot allocate a buffer to do so. */
/* Fortunately, in production code, most of the duplicate reformatting */
/* will be skipped, since it is due to handling errors and debugging. */

/*
 *----------------------------------------------------------------------
 *
 * exp_log --
 *
 *	Send to the logfile if it is open.
 *	Send to stderr if debugging is enabled.
 *      use this for logging everything but the parent/child conversation
 *      (this turns out to be almost nothing)
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Messages may be written to a logfile
 *
 * Notes:
 *	Static buffer is used, so overflow can be in issue
 *
 *----------------------------------------------------------------------
 */

#define LOGUSER		(loguser || force_stdout)
/*VARARGS*/
void
exp_log TCL_VARARGS_DEF(int,arg1)
{
    int force_stdout;
    char buf[4096];
    char *p = buf;
    int len = sizeof(buf);
    int n;
    char *fmt;
    Tcl_Channel chan;
    va_list args;

    force_stdout = TCL_VARARGS_START(int,arg1,args);
    fmt = va_arg(args,char *);

    n = -1;
    while (n == -1) {
	n = vsnprintf(p, len, fmt, args);
	if (n == -1) {
	    if (p != buf) {
		free(p);
	    }
	    len *= 2;
	    p = malloc(len);
	}
    }

    if (debugfile) Tcl_Write(debugfile, buf, n);
    if (logfile_all || (LOGUSER && logfile)) Tcl_Write(logfile, buf, n);
    if (LOGUSER) {
	chan = Tcl_GetStdChannel(TCL_STDOUT);
	if (chan) {
	    Tcl_Write(chan, buf, n);
	}
    }
    if (p != buf) {
	free(p);
    }
    va_end(args);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_nflog --
 *
 *	Send to the logfile if it is open.  Just like exp_log, but
 *	it does no formatting.  Use this function for logging the
 *	parent/child conversation
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Messages may be written to a logfile
 *
 *----------------------------------------------------------------------
 */

void
exp_nflog(buf,force_stdout)
    char *buf;
    int force_stdout;	/* override value of loguser */
{
    int len = strlen(buf);
    Tcl_Channel chan;
    
    if (debugfile) Tcl_Write(debugfile, buf, len);
    if (logfile_all || (LOGUSER && logfile)) Tcl_Write(logfile, buf, len);
    if (LOGUSER) {
	chan = Tcl_GetStdChannel(TCL_STDOUT);
	if (chan) {
	    Tcl_Write(chan, buf, len);
	}
    }
}
#undef LOGUSER

/*
 *----------------------------------------------------------------------
 *
 * exp_debuglog --
 *
 *	Send to log if open and debugging enabled.
 *	send to stderr if debugging enabled.
 *	Use this function for recording unusual things in the log.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Messages may be written to a logfile
 *
 * Notes:
 *	Static buffer is used, so overflow can be in issue
 *
 *----------------------------------------------------------------------
 */

/*VARARGS*/
void
exp_debuglog TCL_VARARGS_DEF(char *,arg1)
{
    char *fmt;
    char buf[4096];
    char *p = buf;
    int len = sizeof(buf);
    int n;
    Tcl_Channel chan;
    va_list args;

    fmt = TCL_VARARGS_START(char *,arg1,args);

    n = -1;
    while (n == -1) {
	n = vsnprintf(p, len, fmt, args);
	if (n == -1) {
	    if (p != buf) {
		free(p);
	    }
	    len *= 2;
	    p = malloc(len);
	}
    }

    if (debugfile) Tcl_Write(debugfile, buf, n);
    if (is_debugging) {
	chan = Tcl_GetStdChannel(TCL_STDERR);
	if (chan) {
	    Tcl_Write(chan, buf, n);
	}
	if (logfile) Tcl_Write(logfile, buf, n);
    }
    
    if (p != buf) free(p);
    va_end(args);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_errorlog --
 *
 *	Log to stderr and to a logfile if it is open.  Also send to
 *	debuglog if debugging is enabled.  This function is used for
 *	logging error conditions.
 *
 * Results:
 *	None
 *
 * Notes:
 *	Static buffer is used, so overflow can be in issue
 *
 *----------------------------------------------------------------------
 */

/*VARARGS*/
void
exp_errorlog TCL_VARARGS_DEF(char *,arg1)
{
    char *fmt;
    char buf[4096];
    char *p = buf;
    int len = sizeof(buf);
    int n;
    Tcl_Channel chan;
    va_list args;

    fmt = TCL_VARARGS_START(char *,arg1,args);

    n = -1;
    while (n == -1) {
	n = vsnprintf(p, len, fmt, args);
	if (n == -1) {
	    if (p != buf) {
		free(p);
	    }
	    len *= 2;
	    p = malloc(len);
	}
    }

    chan = Tcl_GetStdChannel(TCL_STDERR);
    if (chan) {
	Tcl_Write(chan, buf, n);
    }
    if (debugfile) Tcl_Write(debugfile, buf, n);
    if (logfile) Tcl_Write(logfile, buf, n);
    if (p != buf) free(p);
    va_end(args);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_nferrorlog --
 *
 *	Log to stderr and to a logfile if it is open.  Also send to
 *	debuglog if debugging is enabled.  This function is used for
 *	logging the parent/child conversation.  It is just like
 *	exp_errorlog, but it does no formattting.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
void
exp_nferrorlog(buf,force_stdout)
    char *buf;
    int force_stdout;	/* not used, only declared here for compat with */
{
    int len = strlen(buf);
    Tcl_Channel chan;

    chan = Tcl_GetStdChannel(TCL_STDERR);
    if (chan) {
	Tcl_Write(chan, buf, len);
    }
    if (debugfile) Tcl_Write(debugfile, buf, len);
    if (logfile) Tcl_Write(logfile, buf, len);
}

#if 0
static int out_buffer_size;
static char *outp_last;
static char *out_buffer;
static char *outp;	/* pointer into out_buffer - static in order */
			/* to update whenever out_buffer is enlarged */


void
exp_init_log()
{
	out_buffer = ckalloc(BUFSIZ);
	out_buffer_size = BUFSIZ;
	outp_last = out_buffer + BUFSIZ - 1;
}

char *
enlarge_out_buffer()
{
	int offset = outp - out_buffer;

	int new_out_buffer_size = out_buffer_size = BUFSIZ;
	realloc(out_buffer,new_out_buffer_size);

	out_buffer_size = new_out_buffer_size;
	outp = out_buffer + offset;

	outp_last = out_buffer + out_buffer_size - 1;

	return(out_buffer);
}

/* like sprintf, but uses a static buffer enlarged as necessary */
/* currently supported are %s, %d, and %#d where # is a single-digit */
void
exp_sprintf TCL_VARARGS_DEF(char *,arg1)
/* exp_sprintf(va_alist)*/
/*va_dcl*/
{
	char *fmt;
	va_list args;
	char int_literal[20];	/* big enough for an int literal? */
	char *int_litp;		/* pointer into int_literal */
	char *width;
	char *string_arg;
	int int_arg;
	char *int_fmt;

	fmt = TCL_VARARGS_START(char *,arg1,args);
	/*va_start(args);*/
	/*fmt = va_arg(args,char *);*/

	while (*fmt != '\0') {
		if (*fmt != '%') {
			*outp++ = *fmt++;
			continue;
		}

		/* currently, only single-digit widths are used */
		if (isdigit(*fmt)) {
			width = fmt++;
		} else width = 0;

		switch (*fmt) {
		case 's':	/* interpolate string */
			string_arg = va_arg(args,char *);

			while (*string_arg) {
				if (outp == outp_last) {
					if (enlarge_out_buffer() == 0) {
						/* FAIL */
						return;
					}
				}
				*outp++ = *string_arg++;
			}
			fmt++;
			break;
		case 'd':	/* interpolate int */
			int_arg = va_arg(args,int);

			if (width) int_fmt = width;
			else int_fmt = fmt;

			sprintf(int_literal,int_fmt,int_arg);

			int_litp = int_literal;
			for (int_litp;*int_litp;) {
				if (enlarge_out_buffer() == 0) return;
				*outp++ = *int_litp++;
			}
			fmt++;
			break;
		default:	/* anything else is literal */
			if (enlarge_out_buffer() == 0) return;	/* FAIL */
			*outp++ = *fmt++;
			break;
		}
	}
}

/* copy input string to exp_output, replacing \r\n sequences by \n */
/* return length of new string */
int
exp_copy_out(char *s)
{
	outp = out_buffer;
	int count = 0;

	while (*s) {
		if ((*s == '\r') && (*(s+1) =='\n')) s++;
		if (enlarge_out_buffer() == 0) {
			/* FAIL */
			break;
		}
		*outp = *s;
		count++;
	}
	return count;
}
#endif
