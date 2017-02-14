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
 * Copyright (c) 1997-1998 by Gordon Chaffee
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

#include <winsock2.h>
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
    HANDLE hMaster;		/* Output when we are using pipes */
    int useSocket;
    BOOL running;
    HANDLE thread;
} PassThrough;

static HANDLE hMutex;

HANDLE hShutdown;   /* Event is set when the slave driver is shutting down. */

HANDLE ExpConsoleOut;
int    ExpDebug;

typedef struct ExpFunctionKey {
    char *sequence;
    DWORD keyval;
} ExpFunctionKey;

ExpFunctionKey VtFunctionKeys[] =
{
    {"OP",  EXP_KEY_F1},
    {"OQ",  EXP_KEY_F2},
    {"OR",  EXP_KEY_F3},
    {"OS",  EXP_KEY_F4},
    {"[A",  EXP_KEY_UP},
    {"[B",  EXP_KEY_DOWN},
    {"[C",  EXP_KEY_RIGHT},
    {"[D",  EXP_KEY_LEFT},
    {"[F",  EXP_KEY_END},
    {"[H",  EXP_KEY_HOME},
    {"[2~", EXP_KEY_INSERT},
    {"[3~", EXP_KEY_DELETE},
    {"[4~", EXP_KEY_SELECT},
    {"[5~", EXP_KEY_PAGEUP},
    {"[6~", EXP_KEY_PAGEDOWN},
    {"[11~", EXP_KEY_F1},
    {"[12~", EXP_KEY_F2},
    {"[13~", EXP_KEY_F3},
    {"[14~", EXP_KEY_F4},
    {"[15~", EXP_KEY_F5},
    {"[17~", EXP_KEY_F6},
    {"[18~", EXP_KEY_F7},
    {"[19~", EXP_KEY_F8},
    {"[20~", EXP_KEY_F9},
    {"[21~", EXP_KEY_F10},
    {"[23~", EXP_KEY_F11},
    {"[24~", EXP_KEY_F12},
    {"[25~", EXP_KEY_F13},
    {"[26~", EXP_KEY_F14},
    {"[28~", EXP_KEY_F15},
    {"[29~", EXP_KEY_F16},
    {"[31~", EXP_KEY_F17},
    {"[32~", EXP_KEY_F18},
    {"[33~", EXP_KEY_F19},
    {"[34~", EXP_KEY_F20},
    {"[39~", EXP_WIN_RESIZE},
    {NULL, 0}
};

#define EXP_MAX_QLEN 200
HANDLE ExpWaitEvent;		/* Set after modifying wait queue */
HANDLE ExpWaitMutex;		/* Grab before modifying wait queue */
DWORD  ExpWaitCount;		/* Current number of wait handles */
HANDLE ExpWaitQueue[EXP_MAX_QLEN];/* wait handles */
DWORD  ExpConsoleInputMode;	/* Current flags for the console */

static void		InitializeWaitQueue(void);
static BOOL 		PipeRespondToMaster(int useSocket, HANDLE handle,
			    DWORD value, DWORD pid);
static void		ExpProcessInput(HANDLE hMaster,
			    HANDLE hConsole, HANDLE hConsoleOut,
			    int useSocket, ExpSlaveDebugArg *debugInfo);
static void		SshdProcessInput(HANDLE sock, HANDLE hConsoleInW,
			    HANDLE hConsoleOut);
static BOOL		WriteBufferToSlave(int sshd, int useSocket,
			    int noEscapes, HANDLE hMaster,
			    HANDLE hConsoleInW, HANDLE hConsoleOut,
			    PUCHAR buf, DWORD n, LPOVERLAPPED over);
static DWORD WINAPI	PassThroughThread(LPVOID *arg);
static DWORD WINAPI	WaitQueueThread(LPVOID *arg);
static void		SetArgv(char *cmdLine, int *argcPtr, char ***argvPtr);

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
    HANDLE hConsoleInW;		/* Console, writeable input handle */
    HANDLE hConsoleOut;	/* Console, readable output handle */
    HANDLE hMaster;		/* Pipe between master and us */
    HANDLE hSlaveOut;		/* Pipe from slave's STDOUT to us */
    HANDLE hSlaveOutW;		/* Pipe from slave's STDOUT to us */
    HANDLE hProcess;		/* Current process handle */
    UCHAR cmdline[BUFSIZE];
    BOOL bRet;
    DWORD dwResult;
    HANDLE hThread;
    DWORD threadId;
    ExpSlaveDebugArg debugInfo;
    PassThrough thruSlaveOut;
    int passThrough = 0;
    int useSocket = 0;
    int n;
    int sshd = 0;

    struct sockaddr_in sin;
    WSADATA	SockData;

#if 0
    Sleep(22000);		/* XXX: For debugging purposes */
#endif

    if (argc < 2) {
	exit(1);
    }
    if (argc == 2) {
	/* This is how we use it from sshd */
	useSocket = 1;
	sshd = 1;
    } else {
	if (argv[2][0] == '1') {
	    passThrough = 1;
	}
	if (argv[3][0] == '1') {
	    ExpDebug = 1;
	}
    
	if (argv[1][0] != '\\' && argv[1][0] >= '0' && argv[1][0] <= '9') {
	    useSocket = 1;
	} else {
	    useSocket = 0;
	}
    }

    if (!useSocket) {
	hMaster = CreateFile(argv[1], GENERIC_READ | GENERIC_WRITE,
			     0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (hMaster == NULL) {
	    EXP_LOG("Unexpected error 0x%x", GetLastError());
	    Sleep(5000);
	    ExitProcess(255);
	}
    } else {
	SOCKET fdmaster;
	dwResult = WSAStartup(MAKEWORD(2, 0), &SockData);
	if (dwResult != 0) {
	    fprintf(stderr, "Unexpected error 0x%x\n", WSAGetLastError());
	    EXP_LOG("Unexpected error 0x%x", WSAGetLastError());
	    Sleep(5000);
	    ExitProcess(255);
	}

	fdmaster = WSASocket(PF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
			     WSA_FLAG_OVERLAPPED);

	/*
	 * Now attach this to a specific port.
	 */
	sin.sin_family = AF_INET;
	sin.sin_port = htons((short) strtoul(argv[1], NULL, 10));
	sin.sin_addr.s_addr = inet_addr("127.0.0.1");
	if (connect(fdmaster, (struct sockaddr *) &sin, sizeof(sin)) == SOCKET_ERROR) {
	    fprintf(stderr, "Unexpected error 0x%x\n", WSAGetLastError());
	    EXP_LOG("Unexpected error 0x%x", WSAGetLastError());
	    Sleep(5000);
	    ExitProcess(255);
	}
	hMaster = (HANDLE) fdmaster;
    }

    ExpConsoleOut = CreateFile("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, 
			  OPEN_EXISTING, 0, NULL);

    /*
     * After the subprocess is created, send back the status (success or not)
     * and the process id of the child so the master can kill it.
     */

    hShutdown = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (passThrough) {
	bRet = CreatePipe(&hSlaveOut, &hSlaveOutW, NULL, 0);
	if (bRet == FALSE) {
	    PipeRespondToMaster(useSocket, hMaster, GetLastError(), 0);
	    ExitProcess(255);
	}
    } else {
	hSlaveOutW = NULL;
    }
    hMutex = CreateMutex(NULL, FALSE, NULL);
    if (hMutex == NULL) {
	PipeRespondToMaster(useSocket, hMaster, GetLastError(), 0);
	ExitProcess(255);
    }

    InitializeWaitQueue();

    if (sshd) {
	OVERLAPPED over;
	memset(&over, 0, sizeof(over));
	over.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	bRet = ExpReadMaster(useSocket, hMaster, cmdline, BUFSIZE, &n, &over,
			     &dwResult);
	CloseHandle(over.hEvent);
	cmdline[n] = 0;
	SetArgv(cmdline, &debugInfo.argc, &debugInfo.argv);
    } else {
	debugInfo.argc = argc-4;
	debugInfo.argv = &argv[4];
    }

    /*
     * The subprocess needs to be created in the debugging thread.
     * Set all the args in debugInfo and start it up.
     */
     
    hConsoleInW = CreateFile("CONIN$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, 
			     OPEN_EXISTING, 0, NULL);
    if (hConsoleInW == NULL) {
	EXP_LOG("Unexpected error 0x%x", GetLastError());
	ExitProcess(255);
    }
    hConsoleOut = CreateFile("CONOUT$", GENERIC_READ|GENERIC_WRITE,
			      FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, 
			      OPEN_EXISTING, 0, NULL);
    if (hConsoleOut == NULL) {
	EXP_LOG("Unexpected error 0x%x", GetLastError());
	ExitProcess(255);
    }

    ExpConsoleInputMode = ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|
	ENABLE_PROCESSED_INPUT|ENABLE_MOUSE_INPUT;

    debugInfo.passThrough = passThrough;
    debugInfo.useSocket = useSocket;
    debugInfo.hConsole = hConsoleOut;
    debugInfo.hMaster = hMaster;
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

    bRet = PipeRespondToMaster(useSocket, hMaster, debugInfo.result, debugInfo.globalPid);
    if (bRet == FALSE) {
	ExitProcess(255);
    }
    if (debugInfo.result) {
	ExitProcess(0);
    }

    if (passThrough) {
	hProcess = GetCurrentProcess();
	thruSlaveOut.in = hSlaveOut;
	thruSlaveOut.useSocket = useSocket;
	thruSlaveOut.hMaster = hMaster;
	thruSlaveOut.thread = CreateThread(NULL, 8192, PassThroughThread,
	    (LPVOID) &thruSlaveOut, 0, &threadId);
	ExpAddToWaitQueue(thruSlaveOut.thread);
    }
    
    hThread = CreateThread(NULL, 8192, WaitQueueThread,
			   (LPVOID) &debugInfo.process, 0, &threadId);
    CloseHandle(hThread);

    if (sshd) {
	SshdProcessInput(hMaster, hConsoleInW, hConsoleOut);
    } else {
	ExpProcessInput(hMaster, hConsoleInW, hConsoleOut,
			useSocket, &debugInfo);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ExpProcessInput --
 *
 *	The master in this case is Expect.  Drives until everything exits.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
ExpProcessInput(HANDLE hMaster, HANDLE hConsoleInW, HANDLE hConsoleOut,
		int useSocket, ExpSlaveDebugArg *debugInfo)
{
    OVERLAPPED over;
    UCHAR buffer[BUFSIZE];
    DWORD dwState;
    DWORD dwHave;
    DWORD dwNeeded;
    DWORD dwTotalNeeded;
    BOOL bRet;
    DWORD dwResult;
    DWORD driverInCnt;		/* Number of bytes read from expect pipe */

    dwHave = 0;
    dwState = STATE_WAIT_CMD;
    dwNeeded = 1;

    memset(&over, 0, sizeof(over));
    over.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    while (1) {
	bRet = ExpReadMaster(useSocket, hMaster, &buffer[dwHave],
			     dwNeeded-dwHave, &driverInCnt, &over, &dwResult);
	if ((bRet == TRUE && driverInCnt == 0) ||
	    (bRet == FALSE && dwResult == ERROR_BROKEN_PIPE))
	{
	    if( useSocket )
	    {
		/*
		 * We should always break out because the hShutdown event got set
		 * as the wait queue thread exited.
		 */
		if( WaitForSingleObject(hShutdown, 0) == WAIT_OBJECT_0 )
		{
		    fd_set		   monitor;
		    int			   sts;
		    /*
		     * This means that all of the other threads have shut down cleanly.
		     */
		    
		    /*
		     * This will signal shutdown to the other end, by sending the
		     * FD_CLOSE.  We need to hold the connection open until the
		     * master side has closed it's end of the socket.
		     */
		    shutdown((SOCKET) hMaster, SD_SEND);
		    FD_ZERO(&monitor);
		    FD_SET((unsigned int) hMaster, &monitor);
		    sts = select(0, &monitor, NULL, NULL, NULL);
		    
		    /*
		     * Once we get the FD_CLOSE back from the master, then it is
		     * safe to close the socket.
		     */
		    closesocket((SOCKET) hMaster);
		}
		else
		{
		    EXP_LOG("Unclean shutdown 0x%x", dwResult);
		}
	    }
	    ExpKillProcessList();
	    ExitProcess(0);
	} else if (bRet == FALSE) {
	    EXP_LOG("Unexpected error 0x%x", dwResult);
	    ExpKillProcessList();
	    ExitProcess(255);
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
		GenerateConsoleCtrlEvent(CTRL_C_EVENT, debugInfo->globalPid);
	    } else if (dwResult & EXP_KILL_CTRL_BREAK) {
		GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT,debugInfo->globalPid);
	    } else if (dwResult & EXP_KILL_TERMINATE) {
		Exp_KillProcess((Tcl_Pid) debugInfo->process);
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
	case STATE_WRITE_DATA:
	    if (WriteBufferToSlave(FALSE, useSocket, FALSE, hMaster,
				   hConsoleInW, hConsoleOut,
				   buffer, dwNeeded, &over) == FALSE)
	    {
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
	    break;
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
 * SshdProcessInput --
 *
 *	The master in this case is sshd.  Drives until everything exits.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
SshdProcessInput(HANDLE hMaster, HANDLE hConsoleInW, HANDLE hConsoleOut)
{
    int n;
    UCHAR buffer[BUFSIZE];
    BOOL b;
    DWORD dwError;
    OVERLAPPED over;

    memset(&over, 0, sizeof(over));
    over.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    ExpNewConsoleSequences(TRUE, hMaster, &over);

    while (1) {
	ResetEvent(over.hEvent);
	b = ExpReadMaster(TRUE, hMaster, buffer, BUFSIZE, &n, &over, &dwError);
	/*
	 * FIXME(eric) - the changes I put in for process shutdown have probably broken
	 * this.
	 */
	if (!b) {
	    ExpKillProcessList();
	    ExitProcess(0);
	}

	if (WriteBufferToSlave(TRUE, TRUE, FALSE, hMaster, hConsoleInW,
			       hConsoleOut, buffer, n, &over) == FALSE)
	{
	    EXP_LOG("Unable to write to slave: 0x%x", GetLastError());
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
 * PipeRespondToMaster --
 *
 *	Sends back an error response to the Expect master.
 *
 * Results:
 *	TRUE if successful, FALSE if not
 *
 *----------------------------------------------------------------------
 */
static BOOL
PipeRespondToMaster(int useSocket, HANDLE handle, DWORD value, DWORD pid)
{
    UCHAR buf[8];

    buf[0] = (UCHAR) (value & 0x000000ff);
    buf[1] = (UCHAR) ((value & 0x0000ff00) >> 8);
    buf[2] = (UCHAR) ((value & 0x00ff0000) >> 16);
    buf[3] = (UCHAR) ((value & 0xff000000) >> 24);
    buf[4] = (UCHAR) (pid & 0x000000ff);
    buf[5] = (UCHAR) ((pid & 0x0000ff00) >> 8);
    buf[6] = (UCHAR) ((pid & 0x00ff0000) >> 16);
    buf[7] = (UCHAR) ((pid & 0xff000000) >> 24);

    return ExpWriteMaster(useSocket, handle, buf, sizeof(buf), NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * ExpWriteMaster --
 *
 *	Write to the Expect master using either a socket or a pipe
 *
 * Results:
 *	TRUE if successful, FALSE if not
 *
 *----------------------------------------------------------------------
 */

BOOL
ExpWriteMaster(int useSocket, HANDLE hFile, void *buf, DWORD n,
	       LPOVERLAPPED over)
{
    DWORD count, dwResult;
    BOOL bRet;
    WSABUF wsabuf[1];

    if (! useSocket) {
	bRet = WriteFile(hFile, buf, n, &count, over);
	if (!bRet && over) {
	    dwResult = GetLastError();
	    if (dwResult == ERROR_IO_PENDING) {
		bRet = GetOverlappedResult(hFile, over, &count, TRUE);
	    }
	}
	return bRet;
    } else {
	int ret;
	wsabuf[0].buf = buf;
	wsabuf[0].len = n;
	ret = WSASend((SOCKET) hFile, wsabuf, 1, &count, 0, over, NULL);
	if (ret == -1 && over) {
	    dwResult = GetLastError();
	    if (dwResult == ERROR_IO_PENDING) {
		bRet = GetOverlappedResult(hFile, over, &count, TRUE);
	    }
	}
	if (count == n) {
	    return TRUE;
	}
	return FALSE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ExpReadMaster --
 *
 *	Read from the Expect master using either a socket or a pipe
 *
 * Results:
 *	TRUE if successful, FALSE if not.  If we hit and end of file
 *	condition, returns TRUE with *pCount = 0
 *
 *----------------------------------------------------------------------
 */

BOOL
ExpReadMaster(int useSocket, HANDLE hFile, void *buf, DWORD n,
	      PDWORD pCount, LPOVERLAPPED over, PDWORD pError)
{
    int x;
    WSABUF wsabuf[1];
    HANDLE hnd[2];
    BOOL bRet;
    DWORD dwResult;
    DWORD flags;

    *pError = 0;
    if (! useSocket) {
	bRet = ReadFile(hFile, buf, n, pCount, over);
	if (!bRet) {
	    dwResult = GetLastError();
	}
    } else {
	wsabuf[0].buf = buf;
	wsabuf[0].len = n;
	flags = 0;
	bRet = TRUE;
	x = WSARecv((SOCKET) hFile, wsabuf, 1, pCount, &flags, over, NULL);
	if (x == SOCKET_ERROR) {
	    bRet = FALSE;
	    dwResult = WSAGetLastError();
	}
    }
    if (bRet == FALSE) {
	if (dwResult == ERROR_IO_PENDING) {
	    hnd[0] = hShutdown;
	    hnd[1] = over->hEvent;
	    bRet = WaitForMultipleObjects(2, hnd, FALSE, INFINITE);
	    if( bRet == WAIT_OBJECT_0 )
	    {
		/*
		 * We have been instructed to shut down.
		 */
		*pCount = 0;
		bRet = TRUE;
	    }
	    else
	    {
		bRet = GetOverlappedResult(hFile, over, pCount, TRUE);
		if (bRet == FALSE) {
		    dwResult = GetLastError();
		}
	    }
	} else if (dwResult == ERROR_HANDLE_EOF ||
		   dwResult == ERROR_BROKEN_PIPE) {
	    *pCount = 0;
	    bRet = TRUE;
	}
	*pError = dwResult;
    }
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
 *	This this exits before leaving.
 *
 * Notes:
 *	XXX: Should be using ExpReadMaster instead of ReadFile, but
 *	it is very sensitive to error conditions.
 *
 *----------------------------------------------------------------------
 */
static DWORD WINAPI
PassThroughThread(LPVOID *arg)
{
    PassThrough *thru = (PassThrough *) arg;
    HANDLE hIn = thru->in;
    HANDLE hMaster = thru->hMaster;
    int useSocket = thru->useSocket;
    OVERLAPPED over;
    DWORD nread;
    DWORD max;
    DWORD count, n;
    BOOL ret;
    UCHAR buf[BUFSIZE];
    DWORD exitVal;
    LONG err;

    n = 0;

    if (hMaster != NULL) {
	over.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    }

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
	ReleaseMutex(hMutex);
	ret = ExpWriteMaster(useSocket, hMaster, buf, n, &over);
	n = 0;
	if (ret == FALSE) {
	    break;
	}
    }

 done:
    if (n > 0) {
	if (WaitForSingleObject(hMutex, INFINITE) == WAIT_OBJECT_0) {
	    WriteFile(ExpConsoleOut, buf, n, &count, NULL);
	    ReleaseMutex(hMutex);
	    ret = ExpWriteMaster(useSocket, hMaster, buf, n, &over);
	}
    }
 error:
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
ConvertAsciiToKeyEvents(UCHAR c, KEY_EVENT_RECORD *keyRecord)
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
 * ConvertFKeyToKeyEvents --
 *
 *	Converts a function key to an KEY_EVENT_RECORD.
 *
 * Results:
 *	Number of input record that were filled in here.  Currently,
 *	this routine should never be called with less than 6 empty
 *	slots that could be filled.
 *
 *----------------------------------------------------------------------
 */
static DWORD
ConvertFKeyToKeyEvents(DWORD fk, KEY_EVENT_RECORD *keyRecord)
{
    DWORD n;

    n = 0;

    keyRecord->bKeyDown = TRUE;
    keyRecord->wRepeatCount = 1;
    keyRecord->wVirtualKeyCode = ExpFunctionToKeyArray[fk].wVirtualKeyCode;
    keyRecord->wVirtualScanCode = ExpFunctionToKeyArray[fk].wVirtualScanCode;
    keyRecord->dwControlKeyState = ExpFunctionToKeyArray[fk].dwControlKeyState;
    keyRecord->uChar.AsciiChar = 0;
    keyRecord++; n++;

    keyRecord->bKeyDown = FALSE;
    keyRecord->wRepeatCount = 1;
    keyRecord->wVirtualKeyCode = ExpFunctionToKeyArray[fk].wVirtualKeyCode;
    keyRecord->wVirtualScanCode = ExpFunctionToKeyArray[fk].wVirtualScanCode;
    keyRecord->dwControlKeyState = ExpFunctionToKeyArray[fk].dwControlKeyState;
    keyRecord->uChar.AsciiChar = 0;
    keyRecord++; n++;

    return n;
}

/*
 *----------------------------------------------------------------------
 *
 * FindEscapeKey --
 *
 *	Search for a matching escape key sequence
 *
 * Results:
 *	The matching key if found, -1 if not found, -2 if a partial match
 *
 *----------------------------------------------------------------------
 */
static int
FindEscapeKey(PUCHAR buf, DWORD buflen)
{
    DWORD len;
    int i;

    for (i = 0; VtFunctionKeys[i].sequence; i++) {
	len = strlen(VtFunctionKeys[i].sequence);
	if (len == buflen) {
	    if (strncmp(VtFunctionKeys[i].sequence, buf, buflen) == 0) {
		return VtFunctionKeys[i].keyval;
	    }
	} else {
	    if (strncmp(VtFunctionKeys[i].sequence, buf, buflen) == 0) {
		/* Partial match */
		return -2;
	    }
	}
    }
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * FlushInputRecords --
 *
 *	Takes a stack of input records and flushes them
 *
 * Results:
 *	TRUE if successful, FALSE if unsuccessful
 *
 *----------------------------------------------------------------------
 */
static BOOL
FlushInputRecords(HANDLE hConsole, INPUT_RECORD *records, DWORD pos)
{
    DWORD j = 0;
    DWORD nWritten;

    while (j != pos) {
	if (! WriteConsoleInput(hConsole, &records[j], pos-j, &nWritten)) {
	    return FALSE;
	}
	j += nWritten;
    }
    return TRUE;
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
 *	XXX: We need to be able to timeout if an escape is in
 *	the buffer and no further control sequences come in within
 *	a reasonable amount of time, say a 1/4 second.
 *
 *----------------------------------------------------------------------
 */
static BOOL
WriteBufferToSlave(int sshd, int useSocket, int noEscapes, HANDLE hMaster,
		   HANDLE hConsoleInW, HANDLE hConsoleOut,
		   PUCHAR buf, DWORD n, LPOVERLAPPED over)
{
    INPUT_RECORD ibuf[1024];
    DWORD i;
    DWORD pos;
    UCHAR masterBuf[BUFSIZE];
    PUCHAR op, p;
    int key, cols, rows;
    static UCHAR saveBuf[10];
    static int savePos = 0;

#define MAX_RECORDS 1000
    for (pos = 0, i = 0, op = masterBuf; i < n; i++) {

	if (!noEscapes && buf[i] == '\033') {
	    if (FlushInputRecords(hConsoleInW, ibuf, pos) == FALSE) {
		return FALSE;
	    }
	    pos = 0;
	    if (savePos) {
		WriteBufferToSlave(sshd, useSocket, TRUE, hMaster,
				   hConsoleInW, hConsoleOut,
				   saveBuf, savePos, over);
		savePos = 0;
	    }
	    saveBuf[savePos++] = buf[i];
	    continue;
	}

	if (!noEscapes && savePos) {
	    saveBuf[savePos++] = buf[i];
	    key = FindEscapeKey(&saveBuf[1], savePos-1);
	    /* Check for partial match */
	    if (key == -2) {
		continue;
	    } else if (key == EXP_WIN_RESIZE) {
		p = &buf[i+1];
		cols = strtoul(p, &p, 10);
		if (! p) goto flush;
		if (*p++ != ';') goto flush;
		rows = strtoul(p, &p, 10);
		if (! p) goto flush;
		if (*p++ != 'G') goto flush;
		ExpSetConsoleSize(hConsoleInW, hConsoleOut, cols, rows,
				  useSocket, hMaster, over);
		i += p - &buf[i+1];
	    } else if (key >= 0) {
		ibuf[pos].EventType = KEY_EVENT;
		pos += ConvertFKeyToKeyEvents(key, &ibuf[pos].Event.KeyEvent);
	    } else {
	    flush:
		WriteBufferToSlave(sshd, useSocket, TRUE, hMaster,
				   hConsoleInW, hConsoleOut,
				   saveBuf, savePos, over);
	    }
	    savePos = 0;
	    continue;
	}

	/*
	 * Code to echo characters back.  When echoing, convert '\r'
	 * characters into '\n'.  This first determines what mode the
	 * slave process' console is in for echoing.  Still, it isn't
	 * perfect.  The mode can be changed between the time the
	 * characters come through here and the time the characters
	 * are actually read in by the process.
	 */

	if (ExpConsoleInputMode & ENABLE_ECHO_INPUT) {
	    if (buf[i] == '\r') {
		if (sshd) {
		    *op++ = '\r';
		}
		*op++ = '\n';
	    } else {
		*op++ = buf[i];
	    }
	}
	ibuf[pos].EventType = KEY_EVENT;
	pos += ConvertAsciiToKeyEvents(buf[i], &ibuf[pos].Event.KeyEvent);
	if (pos >= MAX_RECORDS - 6) {
	    if (FlushInputRecords(hConsoleInW, ibuf, pos) == FALSE) {
		return FALSE;
	    }
	    pos = 0;
	}
    }
    if (FlushInputRecords(hConsoleInW, ibuf, pos) == FALSE) {
	return FALSE;
    }
    ResetEvent(over->hEvent);
    if (op - masterBuf) {
	ExpWriteMaster(useSocket, hMaster, masterBuf, op - masterBuf, over);
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

    /*
     * When this thread exits, the main thread will be free to return.  Just set
     * the event so that the main thread knows what's up.
     */
    SetEvent(hShutdown);

    return 0;
}

/*
 *-------------------------------------------------------------------------
 *
 * setargv --
 *
 *	Parse the Windows command line string into argc/argv.  Done here
 *	because we don't trust the builtin argument parser in crt0.  
 *	Windows applications are responsible for breaking their command
 *	line into arguments.
 *
 *	2N backslashes + quote -> N backslashes + begin quoted string
 *	2N + 1 backslashes + quote -> literal
 *	N backslashes + non-quote -> literal
 *	quote + quote in a quoted string -> single quote
 *	quote + quote not in quoted string -> empty string
 *	quote -> begin quoted string
 *
 * Results:
 *	Fills argcPtr with the number of arguments and argvPtr with the
 *	array of arguments.
 *
 * Side effects:
 *	Memory allocated.
 *
 *--------------------------------------------------------------------------
 */

static void
SetArgv(char *cmdLine, int *argcPtr, char ***argvPtr)
{
    char *p, *arg, *argSpace;
    char **argv;
    int argc, size, inquote, copy, slashes;
    
    /*
     * Precompute an overly pessimistic guess at the number of arguments
     * in the command line by counting non-space spans.
     */

    size = 2;
    for (p = cmdLine; *p != '\0'; p++) {
	if (isspace(*p)) {
	    size++;
	    while (isspace(*p)) {
		p++;
	    }
	    if (*p == '\0') {
		break;
	    }
	}
    }
    argSpace = (char *) ckalloc((unsigned) (size * sizeof(char *) 
	    + strlen(cmdLine) + 1));
    argv = (char **) argSpace;
    argSpace += size * sizeof(char *);
    size--;

    p = cmdLine;
    for (argc = 0; argc < size; argc++) {
	argv[argc] = arg = argSpace;
	while (isspace(*p)) {
	    p++;
	}
	if (*p == '\0') {
	    break;
	}

	inquote = 0;
	slashes = 0;
	while (1) {
	    copy = 1;
	    while (*p == '\\') {
		slashes++;
		p++;
	    }
	    if (*p == '"') {
		if ((slashes & 1) == 0) {
		    copy = 0;
		    if ((inquote) && (p[1] == '"')) {
			p++;
			copy = 1;
		    } else {
			inquote = !inquote;
		    }
                }
                slashes >>= 1;
            }

            while (slashes) {
		*arg = '\\';
		arg++;
		slashes--;
	    }

	    if ((*p == '\0') || (!inquote && isspace(*p))) {
		break;
	    }
	    if (copy != 0) {
		*arg = *p;
		arg++;
	    }
	    p++;
        }
	*arg = '\0';
	argSpace = arg + 1;
    }
    argv[argc] = NULL;

    *argcPtr = argc;
    *argvPtr = argv;
}

