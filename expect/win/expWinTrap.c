/*
 * expWinTrap.c --
 *
 *	Expect's trap command.  Not implemented under Windows.
 *	Only same placeholder functions exist.
 *
 * Copyright (c) 1997 by Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "exp_port.h"
#include "tcl.h"
#include "exp_prog.h"
#include "exp_command.h"
#include "exp_log.h"


int exp_nostack_dump = FALSE;	/* TRUE if user has requested unrolling of */
				/* stack with no trace */


void
exp_init_trap()
{
}


/*ARGSUSED*/
int
Exp_TrapCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	Tcl_AppendResult(interp, argv[0], ": not implemented", (char *) NULL);
	return TCL_ERROR;
}

static struct exp_cmd_data
cmd_data[]  = {
{"trap",	Exp_TrapCmd,	(ClientData)EXP_SPAWN_ID_BAD,	0},
{0}};

void
exp_init_trap_cmds(interp)
Tcl_Interp *interp;
{
	exp_create_commands(interp,cmd_data);
}

void
exp_rearm_sigchld(interp)
Tcl_Interp *interp;
{
}
