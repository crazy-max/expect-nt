/* 
 * expWinSlaveDrv.c --
 *
 *	This file implements the Windows NT specific expect slave driver.
 *	The slave driver is used to control a subprocess, but it does it
 *	in an unshared console.  Because a process can only be attached
 *	to a single console, we need to have a separate executable 
 *	driving the slave process.  Hence, a slave driver.
 *
 * Copyright (c) 1997 by Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 *
 * XXX: Make sure to check at initialization time in Expect that we are
 * not trying to run on Win32s or Windows 95.  Named pipes will fail
 * rather quickly on Win95 anyway.
 *
 */

/*
 *----------------------------------------------------------------------
 * Communication Protocol Between Master and Slave Driver
 *
 * The master sends over a single byte command.  Depending on
 * the command, further data may follow.  Not all of this is
 * implemented, and some may have changed.
 *
 * EXP_SLAVE_WRITE:
 *	Further Data: Followed by binary 4 bytes specifying length data
 *		to write to slave.
 *	Response: None
 *
 * EXP_SLAVE_KEY:
 *	Further Data: Not defined yet.  Will correspond to the
 *	   KEY_EVENT_RECORD structure.
 *		BOOL bKeyDown; 
 *		WORD wRepeatCount; 
 *		WORD wVirtualKeyCode; 
 *		WORD wVirtualScanCode; 
 *		union { 
 *			WCHAR UnicodeChar; 
 *			CHAR  AsciiChar; 
 *		} uChar; 
 *		DWORD dwControlKeyState; 
 *	Response: None
 *
 *----------------------------------------------------------------------
 */

/*
 * Even though we won't have access to most of the commands, use the
 * normal headers 
 */

#include "tcl.h"
#include "tclPort.h"
#include "expWin.h"
#include "expWinSlave.h"

#define STATE_WAIT_CMD   0	/* Waiting for the next command */
#define STATE_CREATE     1	/* Doesn't happen currently */
#define STATE_KEY        2	/* Waiting for key params */
#define STATE_KILL       3	/* 1 Param: type of kill to do */
#define STATE_MOUSE      4	/*  */
#define STATE_WRITE      5	/* Wait for the length of the data */
#define STATE_WRITE_DATA 6	/* Wait for the data itself */

#define BUFSIZE 4096

typedef struct PassThrough {
    HANDLE in;
    HANDLE out;
    HANDLE mutex;		/* Mutex to get before writing to out */
    BOOL running;
    HANDLE thread;
} PassThrough;

static HANDLE hMutex;
HANDLE ExpConsoleOut;

#define EXP_MAX_QLEN 200
HANDLE ExpWaitEvent;		/* Set after modifying wait queue */
HANDLE ExpWaitMutex;		/* Grab before modifying wait queue */
DWORD  ExpWaitCount;		/* Current number of wait handles */
HANDLE ExpWaitQueue[EXP_MAX_QLEN];/* wait handles */
DWORD  ExpConsoleInputMode;	/* Current flags for the console */

static void		InitializeWaitQueue(void);
static BOOL 		RespondToMaster(HANDLE handle, DWORD value, DWORD pid);
static BOOL		WriteBufferToSlave(HANDLE handle, PUCHAR buf, DWORD n);
static DWORD WINAPI	PassThroughThread(LPVOID *arg);
static DWORD WINAPI	WaitQueueThread(LPVOID *arg);

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	Main entry point from Windows.  The arguments to this program
 *	are used to create the subprocess that this will control.
 *	However, it will not execute the subprocess immediately.
 *	It waits to hear from the expect process about setting
 *	things like the CTRL-C handler and such.
 *
 *	argv[1] is the named pipe base that we need to connect to.
 *	argv[2] Flags:	bit 0: Use pipes or not.
 *	argv[3] is the program we will run, and all the following
 *	  are its arguments.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The launch of this program will have caused a console to be
 *	allocated.  This will then send commands to subprocesses.
 *
 * Notes:
 *	Because we are dealing with anonymous pipes, all actions
 *	on pipes are blocking.  To deal with this, after a process
 *	is created, we need to create two additional threads that
 *	read from the child's STDOUT and STDERR.  Anything read
 *	from the pipe will be forwarded directly to the master on
 *	the appropriate pipes.  This should also take care of the
 *	problems associated with blocking pipes.
 *
 *----------------------------------------------------------------------
 */

void
main(argc, argv)
    int argc;
    char **argv;
{
    HANDLE hMaster;		/* Pipe between master and us */
    HANDLE hSlaveOut;		/* Pipe from slave's STDOUT to us */
    HANDLE hSlaveOutW;		/* Pipe from slave's STDOUT to us */
    HANDLE hProcess;		/* Current process handle */
    DWORD driverInCnt;		/* Number of bytes read from expect pipe */
    BOOL bRet;
    DWORD dwResult;
    UCHAR buffer[BUFSIZE];
    UCHAR copied[BUFSIZE];
    DWORD dwState;
    DWORD dwHave;
    DWORD dwNeeded;
    DWORD dwTotalNeeded;
    HANDLE hThread;
    DWORD threadId;
    int len;
    OVERLAPPED over;
    HANDLE hConsole;		/* Console input handle */
    ExpSlaveDebugArg debugInfo;
    PassThrough thruSlaveOut;
    int passThrough = 0;

    if (argc < 3) {
	exit(1);
    }

#if 0
    Sleep(22000);		/* XXX: For debugging purposes */
#endif

    len = strlen(argv[1]);
    
    if (argv[2][0] == '1') {
	passThrough = 1;
    }
    
    hMaster = CreateFile(argv[1], GENERIC_READ | GENERIC_WRITE,
			 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (hMaster == NULL) {
	EXP_LOG("Unexpected error 0x%x", GetLastError());
	ExitProcess(255);
    }

    hConsole = CreateFile("CONIN$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, 
			  OPEN_EXISTING, 0, NULL);
    if (hConsole == NULL) {
	EXP_LOG("Unexpected error 0x%x", GetLastError());
	ExitProcess(255);
    }

    ExpConsoleOut = CreateFile("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, 
			  OPEN_EXISTING, 0, NULL);

    /*
     * After the subprocess is created, send back the status (success or not)
     * and the process id of the child so the master can kill it.
     */

    if (passThrough) {
	bRet = CreatePipe(&hSlaveOut, &hSlaveOutW, NULL, 0);
	if (bRet == FALSE) {
	    RespondToMaster(hMaster, GetLastError(), 0);
	    ExitProcess(255);
	}
    } else {
	hSlaveOutW = NULL;
    }
    hMutex = CreateMutex(NULL, FALSE, NULL);
    if (hMutex == NULL) {
	RespondToMaster(hMaster, GetLastError(), 0);
	ExitProcess(255);
    }

    InitializeWaitQueue();

    /*
     * The subprocess needs to be created in the debugging thread.
     * Set all the args in debugInfo and start it up.
     */
     
    ExpConsoleInputMode = ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|
	ENABLE_PROCESSED_INPUT|ENABLE_MOUSE_INPUT;

    debugInfo.passThrough = passThrough;
    debugInfo.out = hMaster;
    debugInfo.argc = argc-3;
    debugInfo.argv = &argv[3];
    debugInfo.slaveStdin = NULL;
    debugInfo.slaveStdout = hSlaveOutW;
    debugInfo.slaveStderr = hSlaveOutW;
    debugInfo.event = CreateEvent(NULL, TRUE, FALSE, NULL);
    debugInfo.thread = CreateThread(NULL, 65536, ExpSlaveDebugThread,
	(LPVOID) &debugInfo, 0, &threadId);
    ExpAddToWaitQueue(debugInfo.thread);

    WaitForSingleObject(debugInfo.event, INFINITE);
    CloseHandle(debugInfo.event);
    if (passThrough) {
	CloseHandle(hSlaveOutW);
    }

    bRet = RespondToMaster(hMaster, debugInfo.result, debugInfo.globalPid);
    if (bRet == FALSE) {
	ExitProcess(255);
    }
    if (debugInfo.result) {
	ExitProcess(0);
    }

    if (passThrough) {
	hProcess = GetCurrentProcess();
	thruSlaveOut.in = hSlaveOut;
	thruSlaveOut.out = hMaster;
	thruSlaveOut.thread = CreateThread(NULL, 8192, PassThroughThread,
	    (LPVOID) &thruSlaveOut, 0, &threadId);
	ExpAddToWaitQueue(thruSlaveOut.thread);
    }
    
    hThread = CreateThread(NULL, 8192, WaitQueueThread,
			   (LPVOID) &debugInfo.process, 0, &threadId);
    CloseHandle(hThread);

    dwHave = 0;
    dwState = STATE_WAIT_CMD;
    dwNeeded = 1;

    over.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    while (1) {
	bRet = ReadFile(hMaster, &buffer[dwHave],
			dwNeeded-dwHave, &driverInCnt, &over);
	if (bRet == FALSE) {
	    dwResult = GetLastError();
	    if (dwResult == ERROR_IO_PENDING) {
		bRet = GetOverlappedResult(hMaster, &over, &driverInCnt, TRUE);
		if (bRet == FALSE) {
		    dwResult = GetLastError();
		}
	    }
	}
	if (bRet == FALSE) {
	    if (dwResult == ERROR_HANDLE_EOF || dwResult == ERROR_BROKEN_PIPE)
	    {
		ExpKillProcessList();
		ExitProcess(0);
	    } else {
		EXP_LOG("Unexpected error 0x%x", dwResult);
		ExpKillProcessList();
		ExitProcess(255);
	    }
	}

	dwHave += driverInCnt;
	if (dwHave != dwNeeded) {
	    continue;
	}
	dwHave = 0;
	switch (dwState) {
	case STATE_WAIT_CMD:
	    switch (buffer[0]) {
	    case EXP_SLAVE_KILL:
		dwState = STATE_KILL;
		dwNeeded = 1;
		break;
	    case EXP_SLAVE_KEY:
		/* XXX: To be implemented */
		dwState = STATE_KEY;
		dwNeeded = 13;
		break;
	    case EXP_SLAVE_MOUSE:
		/* XXX: To be implemented */
		dwState = STATE_MOUSE;
		dwNeeded = 13 /* XXX */;
		break;
	    case EXP_SLAVE_WRITE:
		dwNeeded = 4;
		dwState = STATE_WRITE;
		break;
	    }
	    break;
	case STATE_KILL:
	    dwResult = buffer[0];
	    if (dwResult & EXP_KILL_CTRL_C) {
		GenerateConsoleCtrlEvent(CTRL_C_EVENT, debugInfo.globalPid);
	    } else if (dwResult & EXP_KILL_CTRL_BREAK) {
		GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT,debugInfo.globalPid);
	    } else if (dwResult & EXP_KILL_TERMINATE) {
		Exp_KillProcess((Tcl_Pid) debugInfo.process);
	    }
	    dwState = STATE_WAIT_CMD;
	    dwNeeded = 1;
	    break;
	case STATE_WRITE:
	    dwTotalNeeded = buffer[0] | (buffer[1] << 8) |
		(buffer[2] << 16) | (buffer[3] << 24);
	    dwNeeded = (dwTotalNeeded > BUFSIZE) ? BUFSIZE : dwTotalNeeded;
	    dwState = STATE_WRITE_DATA;
	    break;
	case STATE_WRITE_DATA: {
	    DWORD count;
	    PUCHAR ip, op, end;

	    /*
	     * Code to echo characters back.  When echoing, convert '\r'
	     * characters into '\n'.  This should first determine
	     * what mode the slave process' console is in for echoing,
	     * but that requires making this process a debugger process.
	     * XXX: Later on, that is a likely course of action.  For now,
	     * don't even think about it.
	     */

	    if (ExpConsoleInputMode & ENABLE_ECHO_INPUT) {
		ip = buffer;
		op = copied;
		end = ip + dwNeeded;
		while (ip != end) {
		    if (*ip == '\r') {
			*op = '\n';
		    } else {
			*op = *ip;
		    }
		    op++; ip++;
		}
		if (WaitForSingleObject(hMutex, INFINITE) == WAIT_OBJECT_0) {
		    ResetEvent(over.hEvent);
		    bRet = WriteFile(hMaster, copied, dwNeeded, &count, &over);
		    if (bRet == FALSE) {
			dwResult = GetLastError();
			if (dwResult == ERROR_IO_PENDING) {
			    bRet = GetOverlappedResult(hMaster, &over, &count,
						       TRUE);
			}
		    }
		    ReleaseMutex(hMutex);
		}
	    }

	    if (WriteBufferToSlave(hConsole, buffer, dwNeeded) == FALSE) {
		EXP_LOG("Unable to write to slave: 0x%x", GetLastError());
	    }
	    dwTotalNeeded -= dwNeeded;
	    if (dwTotalNeeded) {
		dwNeeded = (dwTotalNeeded > BUFSIZE) ?
		    BUFSIZE : dwTotalNeeded;
	    } else {
		dwNeeded = 1;
		dwState = STATE_WAIT_CMD;
	    }
	    break; }
	case STATE_KEY:
	case STATE_MOUSE:
	    /* XXX: To be implemented */
	    break;
	default:
	    /* If we ever get here, there is a problem */
	    EXP_LOG("Unexpected state\n", 0);
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InitializeWaitQueue --
 *
 *	Set up the initial wait queue
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */
static void
InitializeWaitQueue(void)
{
    int i;
    ExpWaitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    ExpWaitMutex = CreateMutex(NULL, FALSE, NULL);
    ExpWaitCount = 0;
    for (i = 0; i < EXP_MAX_QLEN; i++) {
	ExpWaitQueue[i] = INVALID_HANDLE_VALUE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ExpAddToWaitQueue --
 *
 *	Adds a handle to the wait queue.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */
void
ExpAddToWaitQueue(HANDLE handle)
{
    DWORD i;

    WaitForSingleObject(ExpWaitMutex, INFINITE);
    for (i = 0; i < EXP_MAX_QLEN; i++) {
	if (ExpWaitQueue[i] == INVALID_HANDLE_VALUE) {
	    ExpWaitQueue[i] = handle;
	    ExpWaitCount++;
	    break;
	}
    }
    ReleaseMutex(ExpWaitMutex);
    SetEvent(ExpWaitEvent);
}

/*
 *----------------------------------------------------------------------
 *
 * RespondToMaster --
 *
 *	Sends back an error response to the Expect master.
 *
 * Results:
 *	TRUE if successful, FALSE if not
 *
 *----------------------------------------------------------------------
 */
static BOOL
RespondToMaster(handle, value, pid)
    HANDLE handle;
    DWORD value;
    DWORD pid;
{
    UCHAR buf[8];
    DWORD count;
    BOOL bRet;

    buf[0] = (UCHAR) (value & 0x000000ff);
    buf[1] = (UCHAR) ((value & 0x0000ff00) >> 8);
    buf[2] = (UCHAR) ((value & 0x00ff0000) >> 16);
    buf[3] = (UCHAR) ((value & 0xff000000) >> 24);
    buf[4] = (UCHAR) (pid & 0x000000ff);
    buf[5] = (UCHAR) ((pid & 0x0000ff00) >> 8);
    buf[6] = (UCHAR) ((pid & 0x00ff0000) >> 16);
    buf[7] = (UCHAR) ((pid & 0xff000000) >> 24);

    bRet = WriteFile(handle, buf, sizeof(buf), &count, NULL);
    return bRet;
}

/*
 *----------------------------------------------------------------------
 *
 * PassThroughThread --
 *
 *	Reads everything that is currently available from one pipe
 *	and sends it to the other pipe.  Since anonymous pipes are
 *	synchronous, we wait for a read on one pipe and then send
 *	to the next pipe.
 *
 * Results:
 *	This is the 
 *
 *----------------------------------------------------------------------
 */
static DWORD WINAPI
PassThroughThread(LPVOID *arg)
{
    PassThrough *thru = (PassThrough *) arg;
    HANDLE hIn = thru->in;
    HANDLE hOut = thru->out;
    DWORD count, n, nread, max;
    BOOL ret;
    UCHAR buf[BUFSIZE];
    DWORD exitVal;
    LONG err;
    OVERLAPPED over;

    over.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    while (1) {
	ret = PeekNamedPipe(hIn, NULL, 0, NULL, &nread, NULL);
	if (ret == FALSE) {
	    break;
	}
	if (nread == 0) {
	    nread = 1;
	}
	do {
	    max = sizeof(buf) - 1 - n;
	    nread = min(nread, max);
	    ret = ReadFile(hIn, &buf[n], nread, &count, NULL);
	    if (ret == FALSE) {
		err = GetLastError();
		goto done;
	    }
	    if (count > 0) {
		n += count;
		if (n - 1 >= sizeof(buf)) {
		    break;
		}
	    } else {
		break;
	    }
	    ret = PeekNamedPipe(hIn, NULL, 0, NULL, &nread, NULL);
	    if (ret == FALSE) {
		err = GetLastError();
		goto done;
	    }
	    /*
	     * To allow subprocess to do something without continuous
	     * process switching, give it a bit of processing time before
	     * we check for more data.
	     */
	    if (count == 1 && nread == 0) {
		Sleep(40);
		ret = PeekNamedPipe(hIn, NULL, 0, NULL, &nread, NULL);
		if (ret == FALSE) {
		    err = GetLastError();
		    goto done;
		}
	    }
	} while (nread > 0);

	if (WaitForSingleObject(hMutex, INFINITE) != WAIT_OBJECT_0) {
	    goto error;
	}
	WriteFile(ExpConsoleOut, buf, n, &count, NULL);
	ResetEvent(over.hEvent);
	ret = WriteFile(hOut, buf, n, &count, &over);
	if (ret == FALSE) {
	    err = GetLastError();
	    if (err == ERROR_IO_PENDING) {
		ret = GetOverlappedResult(hOut, &over, &count, TRUE);
		if (ret == FALSE) {
		    err = GetLastError();
		} else {
		    n -= count;
		}
	    }
	} else {
	    n -= count;
	}
	ReleaseMutex(hMutex);
	if (ret == FALSE) {
	    n = 0;
	    break;
	}
    }

 done:
    if (n > 0) {
	if (WaitForSingleObject(hMutex, INFINITE) == WAIT_OBJECT_0) {
	    WriteFile(ExpConsoleOut, buf, n, &count, NULL);
	    ret = WriteFile(hOut, buf, n, &count, NULL);
	    if (ret == FALSE) {
		err = GetLastError();
		if (err == ERROR_IO_PENDING) {
		    ret = GetOverlappedResult(hOut, &over, &count, TRUE);
		}
	    }
	    
	    ReleaseMutex(hMutex);
	}
    }
 error:
    CloseHandle(hOut);
    CloseHandle(hIn);
    thru->running = 0;
    if (err == ERROR_HANDLE_EOF || err == ERROR_BROKEN_PIPE) {
	exitVal = 0;
    } else {
	exitVal = err;
    }
    ExitThread(exitVal);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertAsciiToKeyEvents --
 *
 *	Converts an ASCII character to an KEY_EVENT_RECORD.
 *
 * Results:
 *	Number of input record that were filled in here.  Currently,
 *	this routine should never be called with less than 6 empty
 *	slots that could be filled.
 *
 *----------------------------------------------------------------------
 */
static DWORD
ConvertAsciiToKeyEvents(c, keyRecord)
    UCHAR c;
    KEY_EVENT_RECORD *keyRecord;
{
    UCHAR lc;
    DWORD mods;
    DWORD n;

    n = 0;
    lc = c < 128 ? c : c - 128;
    mods = ExpAsciiToKeyArray[lc].dwControlKeyState;

#if 0
    if (mods & RIGHT_CTRL_PRESSED) {
	/* First, generate a control key press */
	keyRecord->bKeyDown = TRUE;
	keyRecord->wRepeatCount = 1;
	keyRecord->wVirtualKeyCode =
	    ExpModifierKeyArray[EXP_KEY_CONTROL].wVirtualKeyCode;
	keyRecord->wVirtualScanCode =
	    ExpModifierKeyArray[EXP_KEY_CONTROL].wVirtualScanCode;
	keyRecord->uChar.AsciiChar = 0;
	keyRecord->dwControlKeyState = RIGHT_CTRL_PRESSED;
	keyRecord++; n++;
    }
    if (mods & SHIFT_PRESSED) {
	/* First, generate a control key press */
	keyRecord->bKeyDown = TRUE;
	keyRecord->wRepeatCount = 1;
	keyRecord->wVirtualKeyCode =
	    ExpModifierKeyArray[EXP_KEY_SHIFT].wVirtualKeyCode;
	keyRecord->wVirtualScanCode =
	    ExpModifierKeyArray[EXP_KEY_SHIFT].wVirtualScanCode;
	keyRecord->uChar.AsciiChar = 0;
	keyRecord->dwControlKeyState = mods;
	keyRecord++; n++;
    }
#endif

    keyRecord->bKeyDown = TRUE;
    keyRecord->wRepeatCount = 1;
    keyRecord->wVirtualKeyCode = ExpAsciiToKeyArray[lc].wVirtualKeyCode;
    keyRecord->wVirtualScanCode = ExpAsciiToKeyArray[lc].wVirtualScanCode;
    keyRecord->dwControlKeyState = ExpAsciiToKeyArray[lc].dwControlKeyState;
    keyRecord->uChar.AsciiChar = c;
    keyRecord++; n++;

    keyRecord->bKeyDown = FALSE;
    keyRecord->wRepeatCount = 1;
    keyRecord->wVirtualKeyCode = ExpAsciiToKeyArray[lc].wVirtualKeyCode;
    keyRecord->wVirtualScanCode = ExpAsciiToKeyArray[lc].wVirtualScanCode;
    keyRecord->dwControlKeyState = ExpAsciiToKeyArray[lc].dwControlKeyState;
    keyRecord->uChar.AsciiChar = c;
    keyRecord++; n++;

#if 0
    if (mods & SHIFT_PRESSED) {
	/* First, generate a control key press */
	keyRecord->bKeyDown = FALSE;
	keyRecord->wRepeatCount = 1;
	keyRecord->wVirtualKeyCode =
	    ExpModifierKeyArray[EXP_KEY_SHIFT].wVirtualKeyCode;
	keyRecord->wVirtualScanCode =
	    ExpModifierKeyArray[EXP_KEY_SHIFT].wVirtualScanCode;
	keyRecord->uChar.AsciiChar = 0;
	keyRecord->dwControlKeyState = mods & ~SHIFT_PRESSED;
	keyRecord++; n++;
    }
    if (mods & RIGHT_CTRL_PRESSED) {
	/* First, generate a control key press */
	keyRecord->bKeyDown = FALSE;
	keyRecord->wRepeatCount = 1;
	keyRecord->wVirtualKeyCode =
	    ExpModifierKeyArray[EXP_KEY_CONTROL].wVirtualKeyCode;
	keyRecord->wVirtualScanCode =
	    ExpModifierKeyArray[EXP_KEY_CONTROL].wVirtualScanCode;
	keyRecord->uChar.AsciiChar = 0;
	keyRecord->dwControlKeyState = 0;
	keyRecord++; n++;
    }
#endif
    return n;
}

/*
 *----------------------------------------------------------------------
 *
 * WriteBufferToSlave --
 *
 *	Takes an input buffer, and it generates key commands to drive
 *	the slave program.
 *
 * Results:
 *	TRUE if successful, FALSE if unsuccessful.
 *
 * Side Effects:
 *	Characters are entered into the console input buffer.
 *
 * Notes:
 *	Might want to change this to be able to right to either
 *	a pipe or the console.
 *
 *----------------------------------------------------------------------
 */
static BOOL
WriteBufferToSlave(handle, buf, n)
    HANDLE handle;
    PUCHAR buf;
    DWORD n;
{
    BOOL bRet;
    INPUT_RECORD ibuf[1024];
    DWORD i, j;
    DWORD nWritten;
    DWORD pos;

    /*
     * XXX: MAX_RECORDS used to be 1000, but I was seeing a missing character
     * when running telnet.  Even running from the command line, I could
     * occasionally see it.  This is an attempted workaround since it doesn't
     * seem to show up when writing a character at a time.
     */

#define MAX_RECORDS 1000
    for (pos = 0, i = 0; i < n; i++) {
	ibuf[pos].EventType = KEY_EVENT;
	pos += ConvertAsciiToKeyEvents(buf[i], &ibuf[pos].Event.KeyEvent);
	if (pos > MAX_RECORDS) {
	    j = 0;
	    while (j != pos) {
		bRet = WriteConsoleInput(handle, &ibuf[j], pos-j, &nWritten);
		if (bRet == FALSE) {
		    return FALSE;
		}
		j += nWritten;
	    }
	    pos = 0;
	}
    }
    if (pos > 0) {
	j = 0;
	while (j != pos) {
	    bRet = WriteConsoleInput(handle, &ibuf[j], pos-j, &nWritten);
	    if (bRet == FALSE) {
		return FALSE;
	    }
	    j += nWritten;
	}
    }
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * WaitQueueThread --
 *
 *	Wait for all subprocesses to exit before exiting this process.
 *	Exit with the value of the primary child.
 *
 * Results:
 *	None really.  This exits the process.  A zero is return to make
 *	the compiler happy.
 *
 *----------------------------------------------------------------------
 */
static DWORD WINAPI
WaitQueueThread(LPVOID *arg)
{
    Tcl_Pid slavePid = *((Tcl_Pid *) arg);
    DWORD exitVal;
    HANDLE hEvents[EXP_MAX_QLEN+1];
    DWORD  posEvents[EXP_MAX_QLEN+1];
    DWORD n, val, err;
    int i;

    ResetEvent(ExpWaitEvent);
    hEvents[0] = ExpWaitEvent;
    while (ExpWaitCount > 0) {
	n = 1;
	WaitForSingleObject(ExpWaitMutex, INFINITE);
	for (i = 0; i < EXP_MAX_QLEN; i++) {
	    if (ExpWaitQueue[i] != INVALID_HANDLE_VALUE) {
		hEvents[n] = ExpWaitQueue[i];
		posEvents[n] = i;
		n++;
	    }
	}
	ReleaseMutex(ExpWaitMutex);
	val = WaitForMultipleObjects(n, hEvents, FALSE, INFINITE);
	if (val == WAIT_FAILED) {
	    err = GetLastError();
	    printf("WAIT_FAILED: 0x%x\n", err);
	} else if (val >= WAIT_ABANDONED_0 && val < WAIT_ABANDONED_0 + n) {
	    val -= WAIT_ABANDONED_0;
	    err = GetLastError();
	    printf("WAIT_ABANDONED %d: 0x%x\n", val, err);
	} else {
	    val -= WAIT_OBJECT_0;
	    if (val > 0) {
		if (hEvents[val] == (HANDLE) slavePid) {
		    Exp_WaitPid(slavePid, &exitVal, 0);
		} else {
		    CloseHandle(hEvents[val]);
		}
		ExpWaitCount--;
		WaitForSingleObject(ExpWaitMutex, INFINITE);
		ExpWaitQueue[posEvents[val]] = INVALID_HANDLE_VALUE;
		ReleaseMutex(ExpWaitMutex);
	    }
	}
    }

    CloseHandle(hMutex);
    CloseHandle(ExpWaitEvent);
    CloseHandle(ExpWaitMutex);

    ExitProcess(exitVal >> 8);
    return 0;
}
