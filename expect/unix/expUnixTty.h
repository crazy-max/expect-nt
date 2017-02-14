/* expUnixTty.h - tty support definitions for Unix

Design and implementation of this program was paid for by U.S. tax
dollars.  Therefore it is public domain.  However, the author and NIST
would appreciate credit if this program or parts of it are used.
*/

#ifndef __EXP_UNIX_TTY_H__
#define __EXP_UNIX_TTY_H__

#include "expect_cf.h"

extern int exp_ioctled_devtty;

void exp_tty_raw();
void exp_tty_echo();
void exp_tty_break();
int exp_tty_raw_noecho();
int exp_israw();
int exp_isecho();

void exp_tty_set();
int exp_tty_set_simple();
int exp_tty_get_simple();

#include "exp_tty_in.h"

#endif	/* __EXP_UNIX_TTY_H__ */
