/*
 * expWin.h --
 *
 *	Useful definitions for Expect on NT.
 *
 * Copyright (c) 1997 by Mitel, Inc.
 * Copyright (c) 1997 by Gordon Chaffee (chaffee@home.com)
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#define EXP_SLAVE_CREATE 'c'
#define EXP_SLAVE_KEY    'k'
#define EXP_SLAVE_MOUSE  'm'
#define EXP_SLAVE_WRITE  'w'
#define EXP_SLAVE_KILL   'x'

/*
 * Define the types of attempts to use to kill the subprocess
 */
#define EXP_KILL_TERMINATE  0x1
#define EXP_KILL_CTRL_C     0x2
#define EXP_KILL_CTRL_BREAK 0x4

#define EXP_LOG(format, args) \
    ExpSyslog("Expect SlaveDriver %s: %d" format, __FILE__, __LINE__, args)

/*
 * The following defines identify the various types of applications that 
 * run under windows.  There is special case code for the various types.
 */

#define EXP_APPL_NONE	0
#define EXP_APPL_DOS	1
#define EXP_APPL_WIN3X	2
#define EXP_APPL_WIN32	3

typedef struct {
    Tcl_Channel channelPtr;
    int toWrite;
} ExpSpawnState;

extern DWORD		ExpApplicationType(const char *originalName,
			    char *fullPath, char *imageName);
extern DWORD		ExpCreateProcess(int argc, char **argv,
			    HANDLE inputHandle, HANDLE outputHandle,
			    HANDLE errorHandle, int allocConsole,
			    int hideConsole, int debug, int newProcessGroup,
			    Tcl_Pid *pidPtr, PDWORD globalPidPtr);
EXTERN Tcl_Channel	ExpCreateSpawnChannel _ANSI_ARGS_((Tcl_Interp *,
			    Tcl_Channel chan));
extern void		ExpSyslog TCL_VARARGS(char *,fmt);
extern Tcl_Pid		Exp_WaitPid(Tcl_Pid pid, int *statPtr, int options);
extern void		Exp_KillProcess(Tcl_Pid pid);
