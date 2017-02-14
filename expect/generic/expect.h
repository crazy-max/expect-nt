/* expect.h - include file for using the expect library, libexpect.a
from C or C++ (i.e., without Tcl)

Written by: Don Libes, libes@cme.nist.gov, NIST, 12/3/90

Design and implementation of this program was paid for by U.S. tax
dollars.  Therefore it is public domain.  However, the author and NIST
would appreciate credit if this program or parts of it are used.
*/

#ifndef _EXPECT_H
#define _EXPECT_H

#define __EXPECTLIBUSER__

#include "expect_comm.h"

/* These macros are suggested by the autoconf documentation. */

#undef WIFEXITED
#ifndef WIFEXITED
#   define WIFEXITED(stat)  (((stat) & 0xff) == 0)
#endif

#undef WEXITSTATUS
#ifndef WEXITSTATUS
#   define WEXITSTATUS(stat) (((stat) >> 8) & 0xff)
#endif

#undef WIFSIGNALED
#ifndef WIFSIGNALED
#   define WIFSIGNALED(stat) ((stat) && ((stat) == ((stat) & 0x00ff)))
#endif

#undef WTERMSIG
#ifndef WTERMSIG
#   define WTERMSIG(stat)    ((stat) & 0x7f)
#endif

#undef WIFSTOPPED
#ifndef WIFSTOPPED
#   define WIFSTOPPED(stat)  (((stat) & 0xff) == 0177)
#endif

#undef WSTOPSIG
#ifndef WSTOPSIG
#   define WSTOPSIG(stat)    (((stat) >> 8) & 0xff)
#endif

/*
 * Define constants for waitpid() system call if they aren't defined
 * by a system header file.  Taken from Tcl include files
 */

#ifndef WNOHANG
#   define WNOHANG 1
#endif
#ifndef WUNTRACED
#   define WUNTRACED 2
#endif

enum exp_type {
	exp_end = 0,		/* placeholder - no more cases */
	exp_glob,		/* glob-style */
	exp_exact,		/* exact string */
	exp_regexp,		/* regexp-style, uncompiled */
	exp_compiled,		/* regexp-style, compiled */
	exp_null,		/* matches binary 0 */
	exp_bogus		/* aid in reporting compatibility problems */
};

struct exp_case {		/* case for expect command */
	char *pattern;
	regexp *re;
	enum exp_type type;
	int value;		/* value to be returned upon match */
};

#if defined(__WIN32__) && !defined(__EXPECTLIB__)
#define DLLIMP __declspec(dllimport)
#else
#define DLLIMP
#endif

EXTERN DLLIMP char *exp_buffer;		/* buffer of matchable chars */
EXTERN DLLIMP char *exp_buffer_end;	/* one beyond end of matchable chars */
EXTERN DLLIMP char *exp_match;		/* start of matched string */
EXTERN DLLIMP char *exp_match_end;	/* one beyond end of matched string */
EXTERN DLLIMP int exp_match_max;	/* bytes */
EXTERN DLLIMP int exp_timeout;		/* seconds */
EXTERN DLLIMP int exp_full_buffer;	/* if true, return on full buffer */
EXTERN DLLIMP int exp_remove_nulls;	/* if true, remove nulls */

EXTERN DLLIMP int exp_pty_timeout;	/* see Cray hooks in source */
EXTERN DLLIMP int exp_pid;		/* process-id of spawned process */
EXTERN DLLIMP int exp_autoallocpty;	/* if TRUE, we do allocation */
EXTERN DLLIMP int exp_pty[2];		/* master is [0], slave is [1] */
EXTERN DLLIMP char *exp_pty_slave_name;	/* name of pty slave device if we */
					/* do allocation */
EXTERN DLLIMP char *exp_stty_init;	/* initial stty args */
EXTERN DLLIMP int exp_ttycopy;		/* copy tty parms from /dev/tty */
EXTERN DLLIMP int exp_ttyinit;		/* set tty parms to sane state */
EXTERN DLLIMP int exp_console;		/* redirect console */

#ifdef __WIN32__
EXTERN DLLIMP int exp_nt_debug;		/* Expose subprocesses consoles */
#endif

EXTERN jmp_buf exp_readenv;		/* for interruptable read() */
EXTERN DLLIMP int exp_reading;		/* whether we can longjmp or not */

#undef DLLIMP

#define EXP_ABORT	1		/* abort read */
#define EXP_RESTART	2		/* restart read */

typedef void *ExpHandle;

EXTERN void      exp_closefd	_ANSI_ARGS_((ExpHandle handle));
EXTERN int       exp_disconnect	_ANSI_ARGS_((void));
EXTERN int       exp_kill	_ANSI_ARGS_((int pid, int sig));
EXTERN ExpHandle exp_popen	_ANSI_ARGS_((char *command));
EXTERN int       exp_printf	_ANSI_ARGS_(TCL_VARARGS(ExpHandle ,handle));
EXTERN int       exp_putc	_ANSI_ARGS_(TCL_VARARGS(ExpHandle ,handle));
EXTERN int       exp_puts	_ANSI_ARGS_((ExpHandle handle, char *s));
EXTERN int       exp_read	_ANSI_ARGS_((ExpHandle handle, void *buffer,
					int n));
EXTERN int       exp_waitpid	_ANSI_ARGS_((int pid, int *statusPtr,
					int flags));
EXTERN int       exp_write	_ANSI_ARGS_((ExpHandle handle, void *buffer,
					int n));
EXTERN void (*exp_child_exec_prelude) _ANSI_ARGS_((void));

#ifndef EXP_DEFINE_FNS
EXTERN ExpHandle exp_spawnl	_ANSI_ARGS_(TCL_VARARGS(char *,file));
EXTERN int exp_expectl		_ANSI_ARGS_(TCL_VARARGS(ExpHandle ,handle));
#ifndef __WIN32__
EXTERN void *exp_fexpectl	_ANSI_ARGS_(TCL_VARARGS(FILE *,fp));
#endif
#endif

EXTERN ExpHandle exp_spawnv	_ANSI_ARGS_((char *file, char *argv[]));
EXTERN int exp_expectv		_ANSI_ARGS_((ExpHandle handle,
    					struct exp_case *cases));
#ifndef __WIN32__
EXTERN void *exp_fexpectv	_ANSI_ARGS_((FILE *fp,struct exp_case *cases));
#endif

EXTERN ExpHandle exp_spawnfd	_ANSI_ARGS_((int filehandle));
#endif /* _EXPECT_H */

EXTERN void exp_perror		_ANSI_ARGS_((char *string));
EXTERN char *exp_strerror	_ANSI_ARGS_((void));
EXTERN int exp_setblocking	_ANSI_ARGS_((ExpHandle handle, int mode));
EXTERN void exp_setdebug	_ANSI_ARGS_((char *file, int mode));
