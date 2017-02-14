/*
 * exp_clib.c --
 *
 *	Top-level functions in the expect C library.
 *
 * Written by: Don Libes, libes@cme.nist.gov, NIST, 12/3/90
 * Design and implementation of this program was paid for by U.S. tax
 * dollars.  Therefore it is public domain.  However, the author and NIST
 * would appreciate credit if this program or parts of it are used.
 *
 *----------------------------------------------------------------------
 *
 * Modified for Windows NT by Gordon Chaffee 4/25/97, 7/11/97
 *
 * Copyright (c) 1997 by Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 *----------------------------------------------------------------------
 */

/* Define this only in file making up expectlib */
#define __EXPECTLIB__

#include "exp_port.h"
#include "tclInt.h"
#include "tclPort.h"
#include "expect.h"

#include "expWin.h"
#include "expect_tcl.h"
#include "exp_command.h"
#include "exp_rename.h"
#include "exp_log.h"
#include "exp_int.h"
#include "exp_printify.h"

#define EXP_MATCH_MAX	2000
/* public */
char *exp_buffer = 0;
char *exp_buffer_end = 0;
char *exp_match = 0;
char *exp_match_end = 0;
int exp_match_max = EXP_MATCH_MAX;	/* bytes */
int exp_full_buffer = FALSE;		/* don't return on full buffer */
int exp_remove_nulls = TRUE;
int exp_timeout = 10;			/* seconds */
int exp_pty_timeout = 5;		/* seconds - see CRAY below */
int exp_autoallocpty = TRUE;		/* if TRUE, we do allocation */
int exp_pty[2];				/* master is [0], slave is [1] */
int exp_pid;
char *exp_stty_init = 0;		/* initial stty args */
int exp_ttycopy = TRUE;			/* copy tty parms from /dev/tty */
int exp_ttyinit = TRUE;			/* set tty parms to sane state */
int exp_console = FALSE;		/* redirect console */
void (*exp_child_exec_prelude)() = 0;
#ifdef __WIN32__
int exp_nt_debug = FALSE;		/* Display subprocess consoles */
#endif

jmp_buf exp_readenv;		/* for interruptable read() */
int exp_reading = FALSE;	/* whether we can longjmp or not */

void debuglog();
int getptymaster();
int getptyslave();
int Exp_StringMatch();

static Tcl_Interp *InterpPtr = NULL;
#define sysreturn(x)	return(errno = x, -1)

static int bufsiz = 2*EXP_MATCH_MAX;

/*
 *----------------------------------------------------------------------
 *
 * exp_strerror --
 *
 *	Returns a string with an error message in it.
 *
 * Results:
 *	Pointer to a string.  The string is only guaranteed to be
 *	valid until the next call to expectlib.
 *
 *----------------------------------------------------------------------
 */

char *
exp_strerror()
{
    return InterpPtr->result;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_perror --
 *
 *	Prints a string with an error message in it to stderr.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
exp_perror(string)
    char *string;
{
    if (string) {
	fputs(string, stderr);
	fputc(':', stderr);
	fputc(' ', stderr);
    }
    fputs(InterpPtr->result, stderr);
    fputc('\n', stderr);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_spawnv --
 *
 *	Returns a handle to a spawned channel.
 *
 * Results:
 *	NULL on failure (errno will hold the error)
 *	The handle to the spawned channel (master side of pty on Unix).
 *
 *----------------------------------------------------------------------
 */

ExpHandle
exp_spawnv(file,argv)
    char *file;
    char *argv[];	/* some compiler complains about **argv? */
{
    int cc, i;
    char *p;
    Tcl_Channel channel;
    char *chanName;
    int mode;
    char **nargv;
    int need;
    struct exp_f *f;

    if (InterpPtr == NULL) {
	InterpPtr = Tcl_CreateInterp();
	if (Expect_Init(InterpPtr) == TCL_ERROR) {
	    /* XXX: This should really return some error value */
	    fprintf(stderr, "Expect_Init failed: %s\n", InterpPtr->result);
	    return NULL;
	}
    }

#ifdef __WIN32__
    Tcl_SetVar(InterpPtr, "exp_nt_debug",
	exp_nt_debug ? "1" : "0", TCL_GLOBAL_ONLY);
#endif
    
    if (!file || !argv || !argv[0]) {
	errno = EINVAL;
	return NULL;
    }

    if (!argv[0] || strcmp(file,argv[0])) {
	debuglog("expect: warning: file (%s) != argv[0] (%s)\n",
		 file, argv[0]?argv[0]:"");
    }
    
    need = sizeof("spawn") + sizeof(char *);
    cc = 0;
    while (argv[cc]) {
	need += strlen(argv[cc]) + 1 + sizeof(char *) + sizeof(char *);
	cc++;
    }

    nargv = (char **) malloc(need);
    p = (char *) &nargv[cc+2];
    nargv[0] = p;
    memcpy(p, "spawn", sizeof("spawn"));
    p += sizeof("spawn");
    for (i = 0; i < cc; i++) {
	nargv[i+1] = p;
	strcpy(p, argv[i]);
	p += strlen(argv[i]) + 1;
    }
    cc++;
    nargv[cc] = NULL;

    /*
     * Interp->result will hold the process id.
     * The Tcl variable 'exp_spawn_id' will hold the channel name.
     * Use Tcl_GetChannel on the channel.
     */
    if (Exp_SpawnCmd(NULL, InterpPtr, cc, nargv) != TCL_OK) {
	channel = NULL;
    } else {
	chanName = exp_get_var(InterpPtr, EXP_SPAWN_ID_VARNAME);
	channel = Tcl_GetChannel(InterpPtr, chanName, &mode);

	f = exp_chan2f(InterpPtr,chanName,0,1,"");
	if (f == NULL) {
	    channel = NULL;
	} else {
	    exp_pid = f->pid;
	}

	/*
	 * Set the channel to be blocking by default
	 */
	Tcl_GetChannelType(channel)->blockModeProc(
	    Tcl_GetChannelInstanceData(channel), TCL_MODE_BLOCKING);
    }
    free(nargv);
    return (ExpHandle) channel;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_spawnl --
 *
 *	Returns a handle to a spawned channel.
 *
 * Results:
 *	NULL on failure (errno will hold the error)
 *	The handle to the spawned channel.
 *
 *----------------------------------------------------------------------
 */

/*VARARGS*/
ExpHandle
exp_spawnl TCL_VARARGS_DEF(char *,arg1)
{
    va_list args;		/* problematic line here */
    int i;
    char *arg, **argv;
    ExpHandle handle;

    arg = TCL_VARARGS_START(char *,arg1,args);
    for (i=1;;i++) {
	arg = va_arg(args,char *);
	if (!arg) break;
    }
    va_end(args);
    if (i == 0) {
	errno = EINVAL;
	return NULL;
    }
    if (!(argv = (char **)malloc((i+1)*sizeof(char *)))) {
	errno = ENOMEM;
	return NULL;
    }
    argv[0] = TCL_VARARGS_START(char *,arg1,args);
    for (i=1;;i++) {
	argv[i] = va_arg(args,char *);
	if (!argv[i]) break;
    }
    handle = exp_spawnv(argv[0],argv+1);
    free((char *)argv);
    return(handle);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_setblocking --
 *
 *	Sets the blocking mode of a handle.
 *
 * Results:
 *	0 on success, non-zero on error.
 *
 *----------------------------------------------------------------------
 */

int
exp_setblocking(handle, val)
    ExpHandle handle;
    int val;
{
    return Tcl_GetChannelType(handle)->blockModeProc(
	Tcl_GetChannelInstanceData(handle),
	val ? TCL_MODE_BLOCKING : TCL_MODE_NONBLOCKING);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_spawnfd --
 *
 *	Takes a native filehandle, wraps it in a channel, and returns
 *	the channel as a handle.
 *
 * Results:
 *	NULL on error
 *
 *----------------------------------------------------------------------
 */
ExpHandle
exp_spawnfd(filehandle)
    int filehandle;
{
    HANDLE handle;

    handle = (HANDLE) filehandle;
    return Tcl_MakeFileChannel(handle, TCL_READABLE|TCL_WRITABLE);
}

/*
 *----------------------------------------------------------------------
 *
 * rm_nulls --
 *
 *	remove nulls from s.  Initially, the number of chars in s is
 *	c, not strlen(s).  This count does not include the trailing
 *	null.
 *
 * Results:
 *	Number of nulls removed.
 *
 *----------------------------------------------------------------------
 */

static int
rm_nulls(s,c)
    char *s;
    int c;
{
    char *s2 = s;	/* points to place in original string to put */
    /* next non-null character */
    int count = 0;
    int i;

    for (i=0;i<c;i++,s++) {
	if (0 == *s) {
	    count++;
	    continue;
	}
	if (count) *s2 = *s;
	s2++;
    }
    return(count);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_expectv --
 *
 *	takes triplets of args, with a final "exp_last" arg
 *	triplets are type, pattern, and then int to return
 *
 * Results:
 *	Returns a negative value if error (or EOF/timeout) occurs.
 *	Some have an associated errno.
 *
 * Notes:
 *	I tried really hard to make the following two functions share
 *	the code that makes the ecase array, but I kept running into a
 *	brick wall when passing var args into the funcs and then again
 *	into a make_cases func I would very much appreciate it if someone
 *	showed me how to do it right
 *
 *	the key internal variables that this function depends on are:
 *		exp_buffer
 *		exp_buffer_end
 *		exp_match_end
 *
 *----------------------------------------------------------------------
 */

int
exp_expectv(handle,ecases)
    ExpHandle handle;
    struct exp_case *ecases;
{
    int cc = 0;			/* number of chars returned in a single read */
    int buf_length;		/* numbers of chars in exp_buffer */
    int old_length;		/* old buf_length */
    int first_time = TRUE;	/* force old buffer to be tested before */
				/* additional reads */
    int polled = 0;		/* true if poll has caused read() to occur */

    struct exp_case *ec;	/* points to current ecase */

    time_t current_time;	/* current time (when we last looked)*/
    time_t end_time;		/* future time at which to give up */
    int remtime;		/* remaining time in timeout */
    Tcl_Channel channel;
    int key;
    struct exp_f *f, *ignore;
    int return_val;
    int sys_error = 0;
#define return_normally(x)	{return_val = x; goto cleanup;}
#define return_errno(x)	{sys_error = x; goto cleanup;}

    channel = handle;

    f = exp_chan2f(InterpPtr, Tcl_GetChannelName(channel),0,1,"");
    if (!f) return_errno(ENOMEM);

    if (!ecases) return_errno(EINVAL);

	/* compile if necessary */
    for (ec=ecases;ec->type != exp_end;ec++) {
	if ((ec->type == exp_regexp) && !ec->re) {
	    TclRegError((char *)0);
	    if (!(ec->re = TclRegComp(ec->pattern))) {
		fprintf(stderr,"regular expression %s is bad: %s",ec->pattern,TclGetRegError());
		return_errno(EINVAL);
	    }
	}
    }

    exp_buffer = f->buffer;
    exp_buffer_end = exp_buffer + f->size;
    exp_match_end = &f->buffer[f->matched];

    buf_length = exp_buffer_end - exp_match_end;
    if (buf_length) {
	/*
	 * take end of previous match to end of buffer
	 * and copy to beginning of buffer
	 */
	memmove(exp_buffer,exp_match_end,buf_length);
	f->size = buf_length;
	f->printed = buf_length;
	f->buffer[f->size] = '\0';
    }			
    exp_buffer_end = exp_buffer + buf_length;
    *exp_buffer_end = '\0';

    /* get the latest buffer size.  Double the user input for two */
    /* reasons.  1) Need twice the space in case the match */
    /* straddles two bufferfuls, 2) easier to hack the division by */
    /* two when shifting the buffers later on */

    bufsiz = 2*exp_match_max;
    if (f->msize != bufsiz-1) {
	/* if truncated, forget about some data */
	if (buf_length > bufsiz) {
	    /* copy end of buffer down */

	    /* copy one less than what buffer can hold to avoid */
	    /* triggering buffer-full handling code below */
	    /* which will immediately dump the first half */
	    /* of the buffer */
	    memmove(exp_buffer,exp_buffer+(buf_length - bufsiz)+1,
		bufsiz-1);
	    buf_length = bufsiz-1;
	    f->size = bufsiz-1;
	    f->printed = bufsiz-1;
	    f->buffer[f->size] = '\0';
	}
	exp_buffer = realloc(exp_buffer,bufsiz);
	if (!exp_buffer) return_errno(ENOMEM);
	exp_buffer[buf_length] = '\0';
	exp_buffer_end = exp_buffer + buf_length;
	f->buffer = exp_buffer;
	f->msize = bufsiz-1;
    }

    /* remtime and current_time updated at bottom of loop */
    remtime = exp_timeout;

    time(&current_time);
restart:
    end_time = current_time + remtime;
    key = f->key + 1;

    for (;;) {
	/* when buffer fills, copy second half over first and */
	/* continue, so we can do matches over multiple buffers */
	if (buf_length == bufsiz) {
	    int first_half, second_half;

	    if (exp_full_buffer) {
		debuglog("expect: full buffer\r\n");
		exp_match = exp_buffer;
		exp_match_end = exp_buffer + buf_length;
		exp_buffer_end = exp_match_end;
		return_normally(EXP_FULLBUFFER);
	    }
	    first_half = bufsiz/2;
	    second_half = bufsiz - first_half;

	    memmove(exp_buffer,exp_buffer+first_half,second_half);
	    buf_length = second_half;
	    exp_buffer_end = exp_buffer + second_half;
	    f->size = buf_length;
	    f->printed = buf_length;
	    f->buffer[f->size] = '\0';
	}

	/*
	 * always check first if pattern is already in buffer
	 */
	if (first_time) {
	    first_time = FALSE;
	    goto after_read;
	}

	/*
	 * check for timeout
	 */
	if ((exp_timeout >= 0) && ((remtime < 0) || polled)) {
	    debuglog("expect: timeout\r\n");
	    exp_match_end = exp_buffer;
	    return_normally(EXP_TIMEOUT);
	}

	/*
	 * if timeout == 0, indicate a poll has
	 * occurred so that next time through loop causes timeout
	 */
	if (exp_timeout == 0) {
	    polled = 1;
	}


	cc = expect_read(InterpPtr,&f,1,&ignore,remtime,key);
	if (cc == EXP_EOF) {
	    /* do nothing */
	} else if (cc == EXP_TIMEOUT) {
	    debuglog("expect: timed out\r\n");
	} else if (cc == EXP_RECONFIGURE) {
	    goto restart;
	}
	if (cc < 0) {
	    return cc;
	}

	old_length = buf_length;
	buf_length += cc;
	exp_buffer_end += buf_length;

	/* remove nulls from input, so we can use C-style strings */
	/* doing it here lets them be sent to the screen, just */
	/*  in case they are involved in formatting operations */
	if (exp_remove_nulls) {
	    buf_length -= rm_nulls(exp_buffer + old_length, cc);
	}
	/* cc should be decremented as well, but since it will not */
	/* be used before being set again, there is no need */
	exp_buffer_end = exp_buffer + buf_length;
	*exp_buffer_end = '\0';
	exp_match_end = exp_buffer;

    after_read:
	debuglog("expect: does {%s} match ",exp_printify(exp_buffer));
	/* pattern supplied */
	for (ec=ecases;ec->type != exp_end;ec++) {
	    int matched = -1;

	    debuglog("{%s}? ",exp_printify(ec->pattern));
	    if (ec->type == exp_glob) {
		int offset;
		matched = Exp_StringMatch(exp_buffer,ec->pattern,&offset);
		if (matched >= 0) {
		    exp_match = exp_buffer + offset;
		    exp_match_end = exp_match + matched;
		}
	    } else if (ec->type == exp_exact) {
		char *p = strstr(exp_buffer,ec->pattern);
		if (p) {
		    matched = 1;
		    exp_match = p;
		    exp_match_end = p + strlen(ec->pattern);
		}
	    } else if (ec->type == exp_null) {
		char *p;

		for (p=exp_buffer;p<exp_buffer_end;p++) {
		    if (*p == 0) {
			matched = 1;
			exp_match = p;
			exp_match_end = p+1;
		    }
		}
	    } else {
		TclRegError((char *)0);
		if (TclRegExec(ec->re,exp_buffer,exp_buffer)) {
		    matched = 1;
		    exp_match = ec->re->startp[0];
		    exp_match_end = ec->re->endp[0];
		} else if (TclGetRegError()) {
		    fprintf(stderr,"r.e. match (pattern %s) failed: %s",ec->pattern,TclGetRegError());
		}
	    }

	    if (matched != -1) {
		debuglog("yes\nexp_buffer is {%s}\n",
		    exp_printify(exp_buffer));
		return_normally(ec->value);
	    } else debuglog("no\n");
	}

	/*
	 * Update current time and remaining time.
	 * Don't bother if we are waiting forever or polling.
	 */
	if (exp_timeout > 0) {
	    time(&current_time);
	    remtime = end_time - current_time;
	}

	/* no match was made with current data, force a read */
	f->force_read = TRUE;
    }
cleanup:
    f->matched = exp_match_end - exp_buffer;

    if (sys_error) {
	errno = sys_error;
	return -1;
    }
    return return_val;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_expectl --
 *
 *	XXX: Description needed
 *
 * Results:
 *	XXX: Description needed
 *
 *----------------------------------------------------------------------
 */

/*VARARGS*/
int
exp_expectl TCL_VARARGS_DEF(ExpHandle,arg1)
{
    va_list args;
    ExpHandle handle;
    struct exp_case *ec, *ecases;
    int i;
    enum exp_type type;

    handle = TCL_VARARGS_START(ExpHandle,arg1,args);
    for (i=0;;i++) {
	type = va_arg(args,enum exp_type);
	if (type == exp_end) break;

	/* Ultrix 4.2 compiler refuses enumerations comparison!? */
	if ((int)type < 0 || (int)type >= (int)exp_bogus) {
	    fprintf(stderr,"bad type (set %d) in exp_expectl\n",i);
	    sysreturn(EINVAL);
	}

	va_arg(args,char *);		/* COMPUTED BUT NOT USED */
	if (type == exp_compiled) {
	    va_arg(args,regexp *);	/* COMPUTED BUT NOT USED */
	}
	va_arg(args,int);		/* COMPUTED BUT NOT USED*/
    }
    va_end(args);

    if (!(ecases = (struct exp_case *)
	malloc((1+i)*sizeof(struct exp_case))))
	sysreturn(ENOMEM);

	/* now set up the actual cases */
    handle = TCL_VARARGS_START(ExpHandle,arg1,args);
    for (ec=ecases;;ec++) {
	ec->type = va_arg(args,enum exp_type);
	if (ec->type == exp_end) break;
	ec->pattern = va_arg(args,char *);
	if (ec->type == exp_compiled) {
	    ec->re = va_arg(args,regexp *);
	} else {
	    ec->re = 0;
	}
	ec->value = va_arg(args,int);
    }
    va_end(args);
    i = exp_expectv(handle,ecases);

    for (ec=ecases;ec->type != exp_end;ec++) {
	/* free only if regexp and we compiled it for user */
	if (ec->type == exp_regexp) {
	    free((char *)ec->re);
	}
    }
    free((char *)ecases);
    return(i);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_popen --
 *
 *	Sort of like popen(3) but works in both directions.
 *	This spawns a program that can be dealt with by this library.
 *
 * Results:
 *	Returns the channel.
 *
 * Notes:
 *	XXX: Work needed to setup the exp_f structure properly.
 *
 *----------------------------------------------------------------------
 */

ExpHandle
exp_popen(program)
    char *program;
{
    Tcl_Channel channel;

#ifndef __WIN32__
    channel = exp_spawnl("sh","sh","-c",program,(char *)0);
#else
    channel = exp_spawnl("cmd.exe","cmd.exe","/c",program,(char *)0);
#endif
    return channel;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_closefd --
 *
 *	Closes a channel
 *
 * Results:
 *	None?
 *
 *----------------------------------------------------------------------
 */

void
exp_closefd(handle)
    ExpHandle handle;
{
    Tcl_Channel channel = handle;
    char *name;

    name = Tcl_GetChannelName(channel);
    Tcl_VarEval(InterpPtr, "close -i ", name, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_printf --
 *
 *	Print to an expect handle
 *
 * Results:
 *	Number of characters written
 *
 *----------------------------------------------------------------------
 */

/*VARARGS*/
int
exp_printf TCL_VARARGS_DEF(ExpHandle,handle)
{
    int len, n, count;
    char *fmt;
    Tcl_Channel channel;
    va_list args;
    static char *buf = NULL;
    static int bufsize = 0;

    if (buf == NULL) {
	buf = malloc(4000);
	bufsize = 4000;
    }

    channel = (Tcl_Channel) TCL_VARARGS_START(ExpHandle,handle,args);
    fmt = va_arg(args,char *);
    
    do {
	len = _vsnprintf(buf, bufsize, fmt, args);
	if (len == -1) {
	    realloc(buf, bufsize * 2);
	    if (buf == NULL) {
		return -1;
	    }
	    bufsize *= 2;
	}
    } while (len == -1);

    n = 0;
    while (n < len) {
	count = Tcl_Write(channel, &buf[n], len-n);
	if (count == -1) {
	    if (errno == EINTR) continue;
	    break;
	}
	n += count;
    }

    va_end(args);
    return n;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_puts --
 *
 *	Writes a string to the expectlib handle
 *
 * Results:
 *	Number of characters written, -1 on error
 *
 *----------------------------------------------------------------------
 */

int
exp_puts(handle, s)
    ExpHandle handle;
    char *s;
{
    Tcl_Channel channel = handle;
    int n, len, count;

    len = strlen(s);
    n = 0;
    while (n < len) {
	count = Tcl_Write(channel, &s[n], len-n);
	if (count == -1) {
	    if (errno == EINTR) continue;
	    break;
	}
	n += count;
    }
    return n;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_putc --
 *
 *	Writes a character to an expect handle
 *
 * Results:
 *	Number of characters written, -1 on error
 *
 *----------------------------------------------------------------------
 */

int
exp_putc(handle, c)
    ExpHandle handle;
    int c;
{
    Tcl_Channel channel = handle;
    int n;
    unsigned char b;

    b = (unsigned char) c;
    while (1) {
	n = Tcl_Write(channel, &b, 1);
	if (n != 1) {
	    if (errno != EINTR) {
		break;
	    }
	}
    }
    return n;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_write --
 *
 *	Writes characters to the expectlib handle.
 *
 * Results:
 *	# of characters written.
 *	0 on EOF
 *	-1 on error.  errno holds error value
 *
 *----------------------------------------------------------------------
 */
 
int
exp_write(handle, buffer, n)
    ExpHandle handle;
    void *buffer;
    int n;
{
    return Tcl_Write((Tcl_Channel) handle, buffer, n);
}


/*
 *----------------------------------------------------------------------
 *
 * exp_read --
 *
 *	Reads characters from the expectlib handle.
 *
 * Results:
 *	# of characters written.
 *	0 on EOF
 *	-1 on error.  errno holds error value
 *
 *----------------------------------------------------------------------
 */
 
int
exp_read(handle, buffer, n)
    ExpHandle handle;
    void *buffer;
    int n;
{
    return Tcl_Read((Tcl_Channel) handle, buffer, n);
}


/*
 *----------------------------------------------------------------------
 *
 * exp_waitpid --
 *
 *	Wait for a process to exit
 *
 * Results:
 *	Sets a status through statusPtr.
 *	The process id that exited.
 *	0 if nothing exited and called with WNOHANG
 *	-1 if there was an error.
 *
 *----------------------------------------------------------------------
 */

int
exp_waitpid(pid, statusPtr, flags)
    int pid;
    int *statusPtr;
    int flags;
{
    int result;
    int status;
    int waitedPid;
    char *p;

    if (flags & WNOHANG) {
	result = Tcl_Eval(InterpPtr, "wait -nohang");
    } else {
	result = Tcl_Eval(InterpPtr, "wait");
    }

    if (result == TCL_ERROR) {
	errno = EINVAL;
	return -1;
    }
    p = InterpPtr->result;

    waitedPid = strtol(p, &p, 10);
    if (*p == ' ') p++;
    while (*p != ' ') {
	p++;
    }
    p++;

    result = strtol(p, &p, 10);
    if (*p == ' ') p++;

    status = strtol(p, &p, 10) << 8;

    if (flags & WNOHANG) {
	if (strstr(p, "NOEXIT") == NULL) {
	    result = pid;
	} else {
	    result = 0;
	}
    } else {
	result = pid;
    }

    *statusPtr = status;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_kill --
 *
 *	Kill a process
 *
 * Results:
 *	0 on success, -1 on error
 *
 *----------------------------------------------------------------------
 */

int
exp_kill(pid, sig)
    int pid;
    int sig;
{
#ifdef __WIN32__
    HANDLE hProcess;
    char buf[25];

    if (pid == exp_pid) {
	sprintf(buf, "kill %d", sig);
	Tcl_Eval(InterpPtr, buf);
	return 0;
    }
    hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (! hProcess) {
	errno = EINVAL;
	return -1;
    }

    if (! TerminateProcess(hProcess, 1)) {
	errno = EINVAL;
	return -1;
    }
    return 0;
#else
    return kill(pid, sig);
#endif
}

#ifndef __WIN32__
int
exp_disconnect()
{
	int ttyfd;

#ifndef EALREADY
#define EALREADY 37
#endif

	/* presumably, no stderr, so don't bother with error message */
	if (exp_disconnected) sysreturn(EALREADY);
	exp_disconnected = TRUE;

	freopen("/dev/null","r",stdin);
	freopen("/dev/null","w",stdout);
	freopen("/dev/null","w",stderr);

#ifdef POSIX
	setsid();
#else
#ifdef SYSV3
	/* put process in our own pgrp, and lose controlling terminal */
	setpgrp();
	signal(SIGHUP,SIG_IGN);
	if (fork()) exit(0);	/* first child exits (as per Stevens, */
	/* UNIX Network Programming, p. 79-80) */
	/* second child process continues as daemon */
#else /* !SYSV3 */
#ifdef MIPS_BSD
	/* required on BSD side of MIPS OS <jmsellen@watdragon.waterloo.edu> */
#	include <sysv/sys.s>
	syscall(SYS_setpgrp);
#endif
	setpgrp(0,getpid());	/* put process in our own pgrp */
/* Pyramid lacks this defn */
#ifdef TIOCNOTTY
	ttyfd = open("/dev/tty", O_RDWR);
	if (ttyfd >= 0) {
		/* zap controlling terminal if we had one */
		(void) ioctl(ttyfd, TIOCNOTTY, (char *)0);
		(void) close(ttyfd);
	}
#endif /* TIOCNOTTY */
#endif /* SYSV3 */
#endif /* POSIX */
	return(0);
}
#endif /* !__WIN32__ */
