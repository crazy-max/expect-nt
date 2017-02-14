/*
 * expWinSlave.h
 *
 *	Useful definitions used by the slave driver but not useful
 *	for anybody else.
 *
 * Copyright (c) 1997 by Mitel, Inc.
 * Copyright (c) 1997 by Gordon Chaffee (chaffee@home.com)
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

typedef struct ExpSlaveDebugArg {
    HANDLE out;			/* Output handle */

    HANDLE process;		/* Handle to subprocess */
    DWORD globalPid;		/* Program identifier of slave */
    HANDLE thread;		/* Handle of debugger thread */

    HANDLE event;		/* Gets set when the process has been created */
    DWORD result;		/* Result of process being started */
    DWORD lastError;		/* GetLastError for result */
    int passThrough;		/* Pass through mode? */

    /* Args for ExpCreateProcess */
    int argc;			/* Number of args to start slave program */
    char **argv;		/* Argument list of slave program */
    HANDLE slaveStdin;		/* stdin for slave program */
    HANDLE slaveStdout;		/* stdout for slave program */
    HANDLE slaveStderr;		/* stderr for slave program */
} ExpSlaveDebugArg;

typedef struct _EXP_KEY {
    WORD wVirtualKeyCode;
    WORD wVirtualScanCode;
    DWORD dwControlKeyState;
} EXP_KEY;

#define EXP_KEY_CONTROL 0
#define EXP_KEY_SHIFT   1
#define EXP_KEY_LSHIFT  1
#define EXP_KEY_RSHIFT  2
#define EXP_KEY_ALT     3

extern EXP_KEY ExpModifierKeyArray[];
extern EXP_KEY ExpAsciiToKeyArray[];
extern DWORD   ExpConsoleInputMode;
extern HANDLE  ExpConsoleOut;

extern void			ExpAddToWaitQueue(HANDLE handle);
extern void			ExpKillProcessList();
extern DWORD WINAPI		ExpSlaveDebugThread(LPVOID *arg);
extern DWORD WINAPI		ExpGetExecutablePathA(PSTR pathInOut);
extern DWORD WINAPI		ExpGetExecutablePathW(PWSTR pathInOut);
