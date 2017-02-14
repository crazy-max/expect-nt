/*
 * expWinTty.h --
 *
 *	tty support definitions for Windows
 *
 * Copyright (c) 1997 by Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef __EXP_UNIX_TTY_H__
#define __EXP_UNIX_TTY_H__

#include <io.h>
#include <process.h>

extern int exp_stdin_is_tty;
extern int exp_stdout_is_tty;
extern void exp_tty_break();

#endif	/* __EXP_UNIX_TTY_H__ */
