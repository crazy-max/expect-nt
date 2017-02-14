/* exp_tty.h - Includes platform specific tty support definitions

Design and implementation of this program was paid for by U.S. tax
dollars.  Therefore it is public domain.  However, the author and NIST
would appreciate credit if this program or parts of it are used.
*/

#ifndef __EXP_TTY_H__
#define __EXP_TTY_H__

extern struct exp_f *exp_dev_tty;
extern char *exp_dev_tty_id;
extern int exp_stdin_is_tty;
extern int exp_stdout_is_tty;

#if defined(__WIN32__) || defined(_WIN32)
#   include "../win/expWinTty.h"
#else
#   if defined(MAC_TCL)
#	include "expMacTty.h"
#    else
#	include "../unix/expUnixTty.h"
#    endif
#endif


#endif	/* __EXP_TTY_H__ */
