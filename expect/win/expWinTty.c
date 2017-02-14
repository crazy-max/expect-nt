/*
 * expWinTty.c --
 *
 *	Implements some tty related functions.  Handles interaction with
 *	the console
 *
 * Copyright (c) 1997 by Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "tcl.h"
#include "tclPort.h"
#include "expect_tcl.h"
#include "exp_command.h"
#include "exp_tty.h"
#include "exp_log.h"
#include "exp_prog.h"

struct exp_f *exp_dev_tty = NULL;
char *exp_dev_tty_id = "exp_tty";

int exp_stdin_is_tty;
int exp_stdout_is_tty;

static DWORD originalConsoleOutMode;
static DWORD originalConsoleInMode;
static DWORD consoleOutMode;
static DWORD consoleInMode;
static HANDLE hConsoleOut;
static HANDLE hConsoleIn;

int
exp_israw()
{
    return !(consoleOutMode & ENABLE_PROCESSED_OUTPUT);
}

int
exp_isecho()
{
    return consoleInMode & ENABLE_ECHO_INPUT;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_tty_raw --
 *
 *	Set the tty status.  If 1, set to raw.  Otherwise, set
 *	to normal.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	May change the current console settings
 *
 *----------------------------------------------------------------------
 */

void
exp_tty_raw(set)
    int set;
{
    if (set == 1) {
	if (consoleOutMode & ENABLE_PROCESSED_OUTPUT) {
	    consoleOutMode &= ~ENABLE_PROCESSED_OUTPUT;
	    SetConsoleMode(hConsoleOut, consoleOutMode);
	}
    } else {
	if (! (consoleOutMode & ENABLE_PROCESSED_OUTPUT)) {
	    consoleOutMode |= ENABLE_PROCESSED_OUTPUT;
	    SetConsoleMode(hConsoleOut, consoleOutMode);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * exp_tty_echo --
 *
 *	Set the tty status.  If 1, set to echo.  Otherwise, set
 *	to no echo.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	May change the current console settings
 *
 *----------------------------------------------------------------------
 */

void
exp_tty_echo(set)
    int set;
{
    if (set == 1) {
	if (! (consoleInMode & ENABLE_ECHO_INPUT)) {
	    consoleInMode |= ENABLE_ECHO_INPUT;
	    SetConsoleMode(hConsoleIn, consoleInMode);
	}
    } else {
	if (consoleInMode & ENABLE_ECHO_INPUT) {
	    consoleInMode &= ~ENABLE_ECHO_INPUT;
	    SetConsoleMode(hConsoleIn, consoleInMode);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * exp_cook --
 *
 *	take strings with newlines and insert carriage-returns.
 *	This allows user to write send_user strings without always
 *	putting in \r.  If len == 0, use strlen to compute it
 *	NB: if terminal is not in raw mode, nothing is done.
 *
 * Results:
 *	The same string if in raw mode, the modified string otherwise
 *
 *----------------------------------------------------------------------
 */

char *
exp_cook(s,len)
    char *s;
    int *len;	/* current and new length of s */
{
    static unsigned int destlen = 0;
    static char *dest = 0;
    char *d;		/* ptr into dest */
    unsigned int need;

    if (s == 0) return("<null>");

    /* worst case is every character takes 2 to represent */
    need = 1 + 2*(len?*len:strlen(s));
    if (need > destlen) {
	if (dest) ckfree(dest);
	dest = ckalloc(need);
	destlen = need;
    }

    for (d = dest;*s;s++) {
	if (*s == '\n') {
	    *d++ = '\r';
	    *d++ = '\n';
	} else {
	    *d++ = *s;
	}
    }
    *d = '\0';
    if (len) *len = d-dest;
    return(dest);
}


void
exp_init_stdio()
{
    exp_stdin_is_tty = isatty(0);
    exp_stdout_is_tty = isatty(1);

    setbuf(stdout,(char *)0);	/* unbuffer stdout */
}

/*
 *----------------------------------------------------------------------
 *
 * exp_tty_break --
 *
 *	Send a BREAK to the controlled subprocess
 *
 * Results:
 *	XXX: None, so we can't propagate an error back up
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
void
exp_tty_break(interp,f)
    Tcl_Interp *interp;
    struct exp_f *f;
{
#ifdef POSIX
    tcsendbreak(fd,0);
#endif
    Tcl_AppendResult(interp, "exp_tty_break not implemented", NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_SttyCmd --
 *
 *	Implements the 'stty' and 'exp_stty' command.
 *
 * Results:
 *	A standard Tcl result
 *
 * Side Effects:
 *	Can change the characteristics of the console
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_SttyCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
#if 1
    /* redirection symbol is not counted as a stty arg in terms */
    /* of recognition. */
    int saw_unknown_stty_arg = FALSE;
    int saw_known_stty_arg = FALSE;
    int no_args = TRUE;

    int rc = TCL_OK;
    int cooked = FALSE;
    int was_raw, was_echo;

    char **argv0 = argv;

    for (argv=argv0+1;*argv;argv++) {
	if (argv[0][0] == '<') {
	    Tcl_AppendResult(interp, argv0,
		": redirection not supported on Windows NT", NULL);
	    return TCL_ERROR;
	}
    }

    was_raw = exp_israw();
    was_echo = exp_isecho();

    for (argv=argv0+1;*argv;argv++) {
	if (streq(*argv,"raw") ||
	    streq(*argv,"-cooked")) {
	    exp_tty_raw(1);
	    saw_known_stty_arg = TRUE;
	    no_args = FALSE;
	} else if (streq(*argv,"-raw") ||
	    streq(*argv,"cooked")) {
	    cooked = TRUE;
	    exp_tty_raw(-1);
	    saw_known_stty_arg = TRUE;
	    no_args = FALSE;
	} else if (streq(*argv,"echo")) {
	    exp_tty_echo(1);
	    saw_known_stty_arg = TRUE;
	    no_args = FALSE;
	} else if (streq(*argv,"-echo")) {
	    exp_tty_echo(-1);
	    saw_known_stty_arg = TRUE;
	    no_args = FALSE;
#ifdef XXX /* This can be implemented, but it isn't right now */
	} else if (streq(*argv,"rows")) {
	    if (*(argv+1)) {
		exp_win_rows_set(*(argv+1));
		argv++;
		no_args = FALSE;
	    } else {
		exp_win_rows_get(interp->result);
		return TCL_OK;
	    }
	} else if (streq(*argv,"columns")) {
	    if (*(argv+1)) {
		exp_win_columns_set(*(argv+1));
		argv++;
		no_args = FALSE;
	    } else {
		exp_win_columns_get(interp->result);
		return TCL_OK;
	    }
#endif
	} else {
	    saw_unknown_stty_arg = TRUE;
	}
    }

    /* if no result, make a crude one */
    if (interp->result[0] == '\0') {
	sprintf(interp->result,"%sraw %secho",
	    (was_raw?"":"-"),
	    (was_echo?"":"-"));
    }

    return rc;
#else
    Tcl_AppendResult(interp, argv[0], ": not implemented", NULL);
    return TCL_ERROR;
#endif
}

/*ARGSUSED*/
static int
Exp_SystemCmd(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Tcl_AppendResult(interp, argv[0], ": not implemented", NULL);
    return TCL_ERROR;
}

static struct exp_cmd_data
cmd_data[]  = {
{"stty",	Exp_SttyCmd,	0,	0},
{"system",	Exp_SystemCmd,	0,	0},
{0}};

/*
 *----------------------------------------------------------------------
 *
 * exp_init_tty_cmds --
 *
 *	Adds tty related commands to the interpreter
 *
 * Results:
 *	A standard TCL result
 *
 * Side Effects:
 *	Commands are added to the interpreter.
 *
 *----------------------------------------------------------------------
 */

int
exp_init_tty_cmds(interp)
    struct Tcl_Interp *interp;
{
    exp_create_commands(interp,cmd_data);
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_init_pty --
 *
 *	This is where the console device is initialized.  We wrap
 *	it in a channel (if it is available).
 *
 * Results:
 *	None at the moment.
 *
 * Side Effects:
 *	A channel name is set and an exp_f structure is created.
 *
 * Notes:
 *	XXX: GetCurrentProcessId() might not be the appropriate thing
 *	as exp_getpid was previously used.  We might want to have
 *	a separate console input channel.
 *
 *----------------------------------------------------------------------
 */

void
exp_init_pty(interp)
    Tcl_Interp *interp;
{
    HANDLE hOut, hIn;
    Tcl_Channel channel, inChannel, outChannel;
    struct exp_f *f;
    char *inId, *outId;
    int mode;
    DWORD dw;

    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(hOut, &dw) == FALSE) {
	hOut = CreateFile("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE,
	    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOut == INVALID_HANDLE_VALUE) {
	    return;
	}
	outChannel = Tcl_MakeFileChannel(hOut, TCL_WRITABLE);
	f = exp_f_new(interp, outChannel, NULL, GetCurrentProcessId());
	f->channel = outChannel;
	Tcl_RegisterChannel(interp, outChannel);
    } else {
	outChannel = Tcl_GetChannel(interp, "stdout", &mode);
    }

    hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (GetConsoleMode(hIn, &dw) == FALSE) {
	hIn = CreateFile("CONIN$", GENERIC_READ, FILE_SHARE_READ,
	    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
	    if (hOut != GetStdHandle(STD_OUTPUT_HANDLE)) {
		CloseHandle(hOut);
		return;
	    }
	}
	inChannel = Tcl_MakeFileChannel(hIn, TCL_READABLE);
	f = exp_f_new(interp, inChannel, NULL, GetCurrentProcessId());
	f->channel = inChannel;
	Tcl_RegisterChannel(interp, inChannel);
    } else {
	inChannel = Tcl_GetChannel(interp, "stdin", &mode);
    }

    inId = Tcl_GetChannelName(inChannel);
    outId = Tcl_GetChannelName(outChannel);

    /*
     * Create an alias channel
     */

    channel = ExpCreatePairChannel(interp, inId, outId, "exp_tty");
    Tcl_SetChannelOption(interp, channel, "-buffering", "none");
    Tcl_SetChannelOption(interp, channel, "-translation", "binary");
    exp_dev_tty = exp_f_new(interp, channel, NULL, EXP_NOPID);
    Tcl_RegisterChannel(interp, channel);

    /*
     * Get the original console modes
     */
    GetConsoleMode(hOut, &originalConsoleOutMode);
    consoleOutMode = originalConsoleOutMode;
    hConsoleOut = hOut;

    GetConsoleMode(hIn, &originalConsoleInMode);
    consoleInMode = originalConsoleInMode;
    hConsoleIn = hIn;
}

/*
 *----------------------------------------------------------------------
 *
 * exp_pty_exit --
 *
 *	Routine to set the terminal mode back to normal on exit.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Console mode is set to what is was when we entered Expect.
 *
 *----------------------------------------------------------------------
 */

void
exp_pty_exit()
{
    SetConsoleMode(hConsoleIn, originalConsoleInMode);
    SetConsoleMode(hConsoleOut, originalConsoleOutMode);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_init_tty --
 *
 *	Placeholder routine in library
 *
 *----------------------------------------------------------------------
 */

void
exp_init_tty(interp)
    Tcl_Interp *interp;
{
}
