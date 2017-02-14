/*
 * expWinCLib.c --
 *
 *	Top-level functions in the expect C library, expectlib.dll,
 *	on Windows NT
 *
 * Written by: Don Libes, libes@cme.nist.gov, NIST, 12/3/90
 * Design and implementation of this program was paid for by U.S. tax
 * dollars.  Therefore it is public domain.  However, the author and NIST
 * would appreciate credit if this program or parts of it are used.
 *
 *----------------------------------------------------------------------
 * Modified for Windows NT by Gordon Chaffee 4/25/97.  Not yet fully
 * implemented.
 *
 * Copyright (c) 1997 by Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 *
 * XXX: There seem to be two approaches to porting this library.
 * 1. Try to keep out Tcl as much as possible.
 * 2. Resign myself to the fact that Tcl will be used.  Create
 *    an interpreter that will be used to handle things such as
 *    events, etc.  This would allow the channels to possibly be
 *    used for doing I/O.  This seems to be a better approach on
 *    Windows NT since the hard work is then done for us in the
 *    normal expect DLL.
 *----------------------------------------------------------------------
 */

#include "exp_port.h"
#include "tclInt.h"
#include "tclPort.h"
#include "expWin.h"
#include "exp_command.h"
#include "exp_rename.h"
#include "expect.h"
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

jmp_buf exp_readenv;		/* for interruptable read() */
int exp_reading = FALSE;	/* whether we can longjmp or not */

void debuglog();
int getptymaster();
int getptyslave();
int Exp_StringMatch();

static Tcl_Interp *InterpPtr = NULL;
#define sysreturn(x)	return(errno = x, -1)

void exp_init_pty();

static unsigned int bufsiz = 2*EXP_MATCH_MAX;

static struct f {
	int valid;

	char *buffer;		/* buffer of matchable chars */
	char *buffer_end;	/* one beyond end of matchable chars */
	/*char *match;		/* start of matched string */
	char *match_end;	/* one beyond end of matched string */
	int msize;		/* size of allocate space */
				/* actual size is one larger for null */
} *fs = 0;

static int fd_alloc_max = -1;	/* max fd allocated */

/* translate fd or fp to fd */
static struct f *
fdfp2f(fd,fp)
int fd;
FILE *fp;
{
	if (fd == -1) return(fs + fileno(fp));
	else return(fs + fd);
}

#if 0 /* XXX */
struct exp_f *
exp_f_new(interp,chan,spawnId,pid)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    char *spawnId;
    int pid;
#endif

static struct f *
fd_new(fd)
int fd;
{
	int i, low;
	struct f *fp;
	struct f *newfs;	/* temporary, so we don't lose old fs */

	if (fd > fd_alloc_max) {
		if (!fs) {	/* no fd's yet allocated */
			newfs = (struct f *)malloc(sizeof(struct f)*(fd+1));
			low = 0;
		} else {		/* enlarge fd table */
			newfs = (struct f *)realloc((char *)fs,sizeof(struct f)*(fd+1));
			low = fd_alloc_max+1;
		}
		fs = newfs;
		fd_alloc_max = fd;
		for (i = low; i <= fd_alloc_max; i++) { /* init new entries */
			fs[i].valid = FALSE;
		}
	}

	fp = fs+fd;

	if (!fp->valid) {
		/* initialize */
		fp->buffer = malloc((unsigned)(bufsiz+1));
		if (!fp->buffer) return 0;
		fp->msize = bufsiz;
		fp->valid = TRUE;
	}
	fp->buffer_end = fp->buffer;
	fp->match_end = fp->buffer;
	return fp;

}

/* returns fd of master side of pty */
/*
 *----------------------------------------------------------------------
 *
 * exp_spawnv --
 *
 *	Returns a handle to a spawned channel.
 *
 * Results:
 *	NULL on failure (errno will hold the error)
 *	The handle to the spawned channel.
 *
 *----------------------------------------------------------------------
 */

void *
exp_spawnv(file,argv)
    char *file;
    char *argv[];	/* some compiler complains about **argv? */
{
    int cc;
    Tcl_Channel channel;
    char *chanName;
    int mode;
    
    if (InterpPtr == NULL) {
	InterpPtr = Tcl_CreateInterp();
	exp_init_pty(InterpPtr);
	exp_init_tty(InterpPtr);
    }
    
    if (!file || !argv) {
	errno = EINVAL;
	return NULL;
    }

    if (!argv[0] || strcmp(file,argv[0])) {
	debuglog("expect: warning: file (%s) != argv[0] (%s)\n",
		 file, argv[0]?argv[0]:"");
    }
    
    cc = 0;
    while (argv[cc]) {
	cc++;
    }
    /*
     * Interp->result will hold the process id.
     * The Tcl variable 'exp_spawn_id' will hold the channel name.
     * Use Tcl_GetChannel on the channel.
     */
    if (Exp_SpawnCmd(NULL, InterpPtr, cc, argv) != TCL_OK) {
	return NULL;
    }
    chanName = exp_get_var(InterpPtr, EXP_SPAWN_ID_VARNAME);
    channel = Tcl_GetChannel(InterpPtr, chanName, &mode);

    return (void *) channel;
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
void *
exp_spawnl TCL_VARARGS_DEF(char *,arg1)
{
    va_list args;		/* problematic line here */
    int i;
    char *arg, **argv;
    void *handle;

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

/* allow user-provided fd to be passed to expect funcs */
int
exp_spawnfd(fd)
int fd;
{
	if (!fd_new(fd)) {
		errno = ENOMEM;
		return -1;
	}
	return fd;	
}

/* remove nulls from s.  Initially, the number of chars in s is c, */
/* not strlen(s).  This count does not include the trailing null. */
/* returns number of nulls removed. */
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

/* I tried really hard to make the following two functions share the code */
/* that makes the ecase array, but I kept running into a brick wall when */
/* passing var args into the funcs and then again into a make_cases func */
/* I would very much appreciate it if someone showed me how to do it right */

/* takes triplets of args, with a final "exp_last" arg */
/* triplets are type, pattern, and then int to return */
/* returns negative value if error (or EOF/timeout) occurs */
/* some negative values can also have an associated errno */

/* the key internal variables that this function depends on are:
	exp_buffer
	exp_buffer_end
	exp_match_end
*/
static int
expectv(fd,fp,ecases)
int fd;
FILE *fp;
struct exp_case *ecases;
{
	int cc = 0;		/* number of chars returned in a single read */
	int buf_length;		/* numbers of chars in exp_buffer */
	int old_length;		/* old buf_length */
	int first_time = TRUE;	/* force old buffer to be tested before */
				/* additional reads */
	int polled = 0;		/* true if poll has caused read() to occur */

	struct exp_case *ec;	/* points to current ecase */

	time_t current_time;	/* current time (when we last looked)*/
	time_t end_time;	/* future time at which to give up */
	int remtime;		/* remaining time in timeout */

	struct f *f;
	int return_val;
	int sys_error = 0;
#define return_normally(x)	{return_val = x; goto cleanup;}
#define return_errno(x)	{sys_error = x; goto cleanup;}

	f = fdfp2f(fd,fp);
	if (!f) return_errno(ENOMEM);

	exp_buffer = f->buffer;
	exp_buffer_end = f->buffer_end;
	exp_match_end = f->match_end;

	buf_length = exp_buffer_end - exp_match_end;
	if (buf_length) {
		/*
		 * take end of previous match to end of buffer
		 * and copy to beginning of buffer
		 */
		memmove(exp_buffer,exp_match_end,buf_length);
	}			
	exp_buffer_end = exp_buffer + buf_length;
	*exp_buffer_end = '\0';

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

	/* get the latest buffer size.  Double the user input for two */
	/* reasons.  1) Need twice the space in case the match */
	/* straddles two bufferfuls, 2) easier to hack the division by */
	/* two when shifting the buffers later on */

	bufsiz = 2*exp_match_max;
	if (f->msize != bufsiz) {
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
		}
		exp_buffer = realloc(exp_buffer,bufsiz+1);
		if (!exp_buffer) return_errno(ENOMEM);
		exp_buffer[buf_length] = '\0';
		exp_buffer_end = exp_buffer + buf_length;
		f->msize = bufsiz;
	}

	/* some systems (i.e., Solaris) require fp be flushed when switching */
	/* directions - do this again afterwards */
	if (fd == -1) fflush(fp);

	if (exp_timeout != -1) signal(SIGALRM,sigalarm_handler);

	/* remtime and current_time updated at bottom of loop */
	remtime = exp_timeout;

	time(&current_time);
	end_time = current_time + remtime;

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

			memcpy(exp_buffer,exp_buffer+first_half,second_half);
			buf_length = second_half;
			exp_buffer_end = exp_buffer + second_half;
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

		cc = i_read(fd,fp,
				exp_buffer_end,
				bufsiz - buf_length,
				remtime);

		if (cc == 0) {
			debuglog("expect: eof\r\n");
			return_normally(EXP_EOF);	/* normal EOF */
		} else if (cc == -1) {			/* abnormal EOF */
			/* ptys produce EIO upon EOF - sigh */
			if (i_read_errno == EIO) {
				/* convert to EOF indication */
				debuglog("expect: eof\r\n");
				return_normally(EXP_EOF);
			}
			debuglog("expect: error (errno = %d)\r\n",i_read_errno);
			return_errno(i_read_errno);
		} else if (cc == -2) {
			debuglog("expect: timeout\r\n");
			exp_match_end = exp_buffer;
			return_normally(EXP_TIMEOUT);
		}

		old_length = buf_length;
		buf_length += cc;
		exp_buffer_end += buf_length;

		if (logfile_all || (loguser && logfile)) {
			fwrite(exp_buffer + old_length,1,cc,logfile);
		}
		if (loguser) fwrite(exp_buffer + old_length,1,cc,stdout);
		if (debugfile) fwrite(exp_buffer + old_length,1,cc,debugfile);

		/* if we wrote to any logs, flush them */
		if (debugfile) fflush(debugfile);
		if (loguser) {
			fflush(stdout);
			if (logfile) fflush(logfile);
		}

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
	}
 cleanup:
	f->buffer     = exp_buffer;
	f->buffer_end = exp_buffer_end;
	f->match_end  = exp_match_end;

	/* some systems (i.e., Solaris) require fp be flushed when switching */
	/* directions - do this before as well */
	if (fd == -1) fflush(fp);

	if (sys_error) {
		errno = sys_error;
		return -1;
	}
	return return_val;
}

int
exp_fexpectv(fp,ecases)
FILE *fp;
struct exp_case *ecases;
{
	return(expectv(-1,fp,ecases));
}

int
exp_expectv(fd,ecases)
int fd;
struct exp_case *ecases;
{
	return(expectv(fd,(FILE *)0,ecases));
}

/*VARARGS*/
int
exp_expectl TCL_VARARGS_DEF(int,arg1)
/*exp_expectl(va_alist)*/
/*va_dcl*/
{
	va_list args;
	int fd;
	struct exp_case *ec, *ecases;
	int i;
	enum exp_type type;

	fd = TCL_VARARGS_START(int,arg1,args);
	/* va_start(args);*/
	/* fd = va_arg(args,int);*/
	/* first just count the arg sets */
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
	fd = TCL_VARARGS_START(int,arg1,args);
	/*va_start(args);*/
	/*va_arg(args,int);*/		/*COMPUTED BUT NOT USED*/
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
	i = expectv(fd,(FILE *)0,ecases);

	for (ec=ecases;ec->type != exp_end;ec++) {
		/* free only if regexp and we compiled it for user */
		if (ec->type == exp_regexp) {
			free((char *)ec->re);
		}
	}
	free((char *)ecases);
	return(i);
}

int
exp_fexpectl TCL_VARARGS_DEF(FILE *,arg1)
/*exp_fexpectl(va_alist)*/
/*va_dcl*/
{
	va_list args;
	FILE *fp;
	struct exp_case *ec, *ecases;
	int i;
	enum exp_type type;

	fp = TCL_VARARGS_START(FILE *,arg1,args);
	/*va_start(args);*/
	/*fp = va_arg(args,FILE *);*/
	/* first just count the arg-pairs */
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

#if 0
	va_start(args);
	va_arg(args,FILE *);		/*COMPUTED, BUT NOT USED*/
#endif
	(void) TCL_VARARGS_START(FILE *,arg1,args);

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
	i = expectv(-1,fp,ecases);

	for (ec=ecases;ec->type != exp_end;ec++) {
		/* free only if regexp and we compiled it for user */
		if (ec->type == exp_regexp) {
			free((char *)ec->re);
		}
	}
	free((char *)ecases);
	return(i);
}

/* like popen(3) but works in both directions */
FILE *
exp_popen(program)
char *program;
{
	FILE *fp;
	int ec;

	if (0 > (ec = exp_spawnl("sh","sh","-c",program,(char *)0))) return(0);
	if (!(fp = fdopen(ec,"r+"))) return(0);
	setbuf(fp,(char *)0);
	return(fp);
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
