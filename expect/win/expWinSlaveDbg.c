/* 
 * expWinSlaveDbg.c --
 *
 *	The slave driver acts as a debugger for the slave program.  It
 *	does this so that we can determine echoing behavior of the
 *	subprocess.  This isn't perfect as the subprocess can change
 *	echoing behavior while our keystrokes are lying in its console
 *	input buffer, but it is much better than nothing.  The debugger
 *	thread sets up breakpoints on the functions we want to intercept,
 *	and it writes data that is written directly to the console to
 *	the master pipe.
 *
 * Copyright (c) 1997 by Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * TODO:
 *  * Maintain cursor information for each console screen buffer.
 *  * Intercept additional console input and output characters to better
 *    keep track of current console state.
 */

/*
 * Even though we won't have access to most of the commands, use the
 * same headers 
 */

#include <windows.h>
#include <stddef.h>
#include "tclInt.h"
#include "tclPort.h"
#include "expWin.h"
#include "expWinSlave.h"
#include <assert.h>

#define SUBPROCESS_BUFFER_LEN 1024

#define SINGLE_STEP_BIT 0x100;	/* This only works on the x86 processor */

#define EXP_FLAG_APP_NAME		0x01
#define EXP_FLAG_CMD_LINE		0x02
#define EXP_FLAG_PROC_ATTRS		0x04
#define EXP_FLAG_THREAD_ATTRS		0x08
#define EXP_FLAG_ENVIRONMENT		0x10
#define EXP_FLAG_CURR_DIR		0x20
#define EXP_FLAG_SI			0x40
#define EXP_FLAG_PI			0x80

#ifndef UNICODE
#  define ExpCreateProcessInfo	ExpCreateProcessInfoA
#  define OnWriteConsoleOutput	OnWriteConsoleOutputA
#  define ReadSubprocessString	ReadSubprocessStringA
#  define StartSubprocess	StartSubprocessW
#else
#  define ExpCreateProcessInfo	ExpCreateProcessInfoW
#  define OnWriteConsoleOutput	OnWriteConsoleOutputW
#  define ReadSubprocessString	ReadSubprocessStringW
#  define StartSubprocess	StartSubprocessA
#endif

typedef struct _ExpProcess ExpProcess;
typedef struct _ExpBreakpoint ExpBreakpoint;

typedef struct _ExpCreateProcessInfo {
    TCHAR appName[8192];
    TCHAR cmdLine[8192];
    SECURITY_ATTRIBUTES procAttrs;
    SECURITY_ATTRIBUTES threadAttrs;
    BOOL bInheritHandles;
    DWORD dwCreationFlags;
    LPVOID lpEnvironment;
    TCHAR currDir[8192];
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    PVOID piPtr;		/* Pointer to PROCESS_INFORMATION in slave */
    DWORD flags;
} ExpCreateProcessInfo;

typedef struct _CreateProcessThreadArgs {
    ExpCreateProcessInfo *cp;
    ExpProcess *proc;
    ExpSlaveDebugArg debugInfo;
} CreateProcessThreadArgs;

typedef struct _ExpThreadInfo {
    HANDLE hThread;
    DWORD dwThreadId;
    DWORD nargs;
    DWORD args[16];		/* Space for saving 16 args.  We need this
				 * space while we are waiting for the return
				 * value for the function. */
    LPCONTEXT context;		/* Current context */

    ExpCreateProcessInfo *createProcess; /* Create process structure */
    struct _ExpThreadInfo *nextPtr;
} ExpThreadInfo;

typedef void (ExpBreakProc) (ExpProcess *, ExpThreadInfo *,
    ExpBreakpoint *, DWORD ret, DWORD direction);

typedef struct _ExpBreakInfo {
    PUCHAR funcName;		/* Name of function to intercept */
    DWORD nargs;		/* Number of arguments */
    ExpBreakProc *breakProc;	/* Function to call when a breakpoint is hit */
#define EXP_BREAK_IN	1	/* Call handler on the way in */
#define EXP_BREAK_OUT	2	/* Call handler on the way out */
    DWORD dwFlags;		/* Bits for direction to call handler in */
} ExpBreakInfo;

struct _ExpBreakpoint {
    BOOL returning;		/* Is this a returning breakpoint? */
    UCHAR code;			/* Original code */
    PVOID codePtr;		/* Address of original code */
    PVOID codeReturnPtr;	/* Address of return breakpoint */
    DWORD origRetAddr;		/* Original return address */
    ExpBreakInfo *breakInfo;	/* Information about the breakpoint */
    ExpThreadInfo *threadInfo;	/* If this breakpoint is for a specific
				 * thread */
    struct _ExpBreakpoint *nextPtr;
};

#define EXP_CREATEPROCESS_MEM_OFFSET 4096
#define PAGESIZE 0x1000
#define PAGEMASK (PAGESIZE-1)

/*
 * There is one of these structures for each subprocess that we are
 * controlling.
 */
struct _ExpProcess {
    ExpThreadInfo *threadList;	/* List of threads in the subprocess */
    ExpBreakpoint *brkptList;/* List of breakpoints in the subprocess */
    ExpBreakpoint *lastBrkpt;/* Last Breakpoint Hit */
    DWORD offset;		/* Breakpoint offset in allocated mem */
    DWORD nBreakCount;		/* Number of breakpoints hit */
    DWORD consoleHandles[100];	/* A list of input console handles */
    DWORD consoleHandlesMax;
    HANDLE hProcess;		/* handle to subprocess */
    DWORD hPid;			/* Global process id */
    DWORD threadCount;		/* Number of threads in process */
    DWORD pSubprocessMemory;	/* Pointer to allocated memory in subprocess */
    DWORD pSubprocessBuffer;	/* Pointer to buffer memory in subprocess */
    DWORD pMemoryCacheBase;	/* Base address of memory cache */
    BYTE  pMemoryCache[PAGESIZE]; /* Subprocess memory cache */
    OVERLAPPED overlapped;	/* Overlapped structure for writing to master */
    Tcl_HashTable *funcTable;	/* Function table name to address mapping */
    struct _ExpProcess *nextPtr;
};

/*
 * List of processes that are being debugged
 */
static ExpProcess *ProcessList = NULL;
static HANDLE HMaster;		/* Handle to master output pipe */
static COORD CursorPosition;
static BOOL CursorKnown = FALSE;

/*
 * Static functions in this file:
 */

extern void		ExpCommonDebugger();
extern BOOL		ReadSubprocessMemory(ExpProcess *proc, LPVOID addr,
			    LPVOID buf, DWORD len);
extern int		ReadSubprocessStringA(ExpProcess *proc, PVOID base,
			    PCHAR buf, int buflen);
extern int		ReadSubprocessStringW(ExpProcess *proc, PVOID base,
			    PWCHAR buf, int buflen);
extern BOOL		WriteSubprocessMemory(ExpProcess *proc, LPVOID addr,
			    LPVOID buf, DWORD len);

static DWORD WINAPI	CreateProcessThread(LPVOID *lparg);
extern void		CreateVtSequence(ExpProcess *, COORD newPos, DWORD n);
static BOOL		SetBreakpoint(ExpProcess *, ExpBreakInfo *);
extern ExpBreakpoint *	SetBreakpointAtAddr(ExpProcess *, ExpBreakInfo *,
			    PVOID funcPtr);
static void		StartSubprocessA(ExpProcess *, ExpThreadInfo *);
static void		StartSubprocessW(ExpProcess *, ExpThreadInfo *);

extern void		OnGetStdHandle(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnOpenConsoleW(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnSetConsoleMode(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnSetConsoleCursorPosition(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnWriteConsoleA(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnWriteConsoleW(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnWriteConsoleOutputA(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnWriteConsoleOutputW(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnWriteConsoleOutputCharacterA(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnWriteConsoleOutputCharacterW(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
#if 1 /* XXX: Testing purposes only */
extern void		OnExpGetExecutablePathA(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnExpGetExecutablePathW(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnSearchPathW(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnlstrcpynW(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnlstrrchrW(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
extern void		OnGetFileAttributesW(ExpProcess *, ExpThreadInfo *,
			    ExpBreakpoint *, DWORD, DWORD);
#endif

static void		OnXBreakpoint(ExpProcess *, LPDEBUG_EVENT);
static void		OnXCreateProcess(ExpProcess *, LPDEBUG_EVENT);
static void		OnXCreateThread(ExpProcess *, LPDEBUG_EVENT);
static void		OnXDeleteThread(ExpProcess *, LPDEBUG_EVENT);
static void		OnXFirstBreakpoint(ExpProcess *, LPDEBUG_EVENT);
static void		OnXSecondBreakpoint(ExpProcess *, LPDEBUG_EVENT);
static void		OnXLoadDll(ExpProcess *, LPDEBUG_EVENT);
static void		OnXSingleStep(ExpProcess *, LPDEBUG_EVENT);

#ifndef UNICODE

/*
 * Functions where we set breakpoints
 */

ExpBreakInfo BreakArrayKernel32[] = {
    {"GetStdHandle", 1, OnGetStdHandle, EXP_BREAK_OUT},
    {"OpenConsoleW", 4, OnOpenConsoleW, EXP_BREAK_OUT},
    {"SetConsoleMode", 2, OnSetConsoleMode, EXP_BREAK_OUT},
    {"SetConsoleCursorPosition", 2, OnSetConsoleCursorPosition, EXP_BREAK_OUT},
    {"WriteConsoleA", 5, OnWriteConsoleA, EXP_BREAK_OUT},
    {"WriteConsoleW", 5, OnWriteConsoleW, EXP_BREAK_OUT},
    {"WriteConsoleOutputA", 5, OnWriteConsoleOutputA, EXP_BREAK_OUT},
    {"WriteConsoleOutputW", 5, OnWriteConsoleOutputW, EXP_BREAK_OUT},
    {"WriteConsoleOutputCharacterA", 5, OnWriteConsoleOutputCharacterA, EXP_BREAK_OUT},
    {"WriteConsoleOutputCharacterW", 5, OnWriteConsoleOutputCharacterW, EXP_BREAK_OUT},
    {NULL, 0, NULL}
};
#endif /* !UNICODE */

#ifndef UNICODE

/*
 *----------------------------------------------------------------------
 *
 * ExpProcessNew --
 *
 *	Allocates a new structure for debugging a process and
 *	initializes it.
 *
 * Results:
 *	A new structure
 *
 * Side Effects:
 *	Memory is allocated, an event is created.
 *
 * Notes:
 *	XXX: This structure is currently never freed, but it should be
 *	whenever a process exits.
 *
 *----------------------------------------------------------------------
 */

static ExpProcess *
ExpProcessNew(void)
{
    ExpProcess *proc;
    proc = malloc(sizeof(ExpProcess));
    proc->threadList = NULL;
    proc->threadCount = 0;
    proc->brkptList = NULL;
    proc->lastBrkpt = NULL;
    proc->offset = 0;
    proc->nBreakCount = 0;
    proc->consoleHandlesMax = 0;
    proc->hProcess = NULL;
    proc->pSubprocessMemory = 0;
    proc->pSubprocessBuffer = 0;
    proc->pMemoryCacheBase = 0;
    ZeroMemory(&proc->overlapped, sizeof(OVERLAPPED));
    proc->funcTable = malloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(proc->funcTable, TCL_STRING_KEYS);
    proc->nextPtr = ProcessList;
    ProcessList = proc;
    return proc;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpProcessFree --
 *
 *	Frees all allocated memory for a process and closes any
 *	open handles
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
ExpProcessFree(ExpProcess *proc)
{
    ExpThreadInfo *tcurr, *tnext;
    ExpBreakpoint *bcurr, *bnext;
    ExpProcess *pcurr, *pprev;
    
    for (tcurr = proc->threadList; tcurr != NULL; tcurr = tnext) {
	tnext = tcurr->nextPtr;
	proc->threadCount--;
	CloseHandle(tcurr->hThread);
	free(tcurr);
    }
    for (bcurr = proc->brkptList; bcurr != NULL; bcurr = bnext) {
	bnext = bcurr->nextPtr;
	free(bcurr);
    }
    Tcl_DeleteHashTable(proc->funcTable);
    free(proc->funcTable);

    for (pprev = NULL, pcurr = ProcessList; pcurr != NULL;
	 pcurr = pcurr->nextPtr)
    {
	if (pcurr == proc) {
	    if (pprev == NULL) {
		ProcessList = pcurr->nextPtr;
	    } else {
		pprev->nextPtr = pcurr->nextPtr;
	    }
	    break;
	}
    }

    free(proc);
}

/*
 *----------------------------------------------------------------------
 *
 * ExpKillProcessList --
 *
 *	Runs through the current list of slave processes and kills
 *	them all.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Processes are terminated.
 *
 *----------------------------------------------------------------------
 */

void
ExpKillProcessList()
{
    ExpProcess *proc;

    for (proc = ProcessList; proc != NULL; proc = proc->nextPtr) {
	Exp_KillProcess((Tcl_Pid) proc->hProcess);
	if (WaitForSingleObject(proc->hProcess, 10000) == WAIT_TIMEOUT) {
	    Exp_KillProcess((Tcl_Pid) proc->hProcess);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ExpSlaveDebugThread --
 *
 *	Acts as a debugger for a subprocess created by the spawn command.
 *
 * Results:
 *	None.  This thread exits with ExitThread() when the subprocess dies.
 *
 * Side Effects:
 *	A process is created.
 *
 *----------------------------------------------------------------------
 */

DWORD WINAPI
ExpSlaveDebugThread(LPVOID *lparg)
{
    ExpSlaveDebugArg *arg = (ExpSlaveDebugArg *) lparg;

    HMaster = arg->out;		/* Set the master program */

    /* Make sure the child does not ignore Ctrl-C */
    SetConsoleCtrlHandler(NULL, FALSE);
    arg->result =
	ExpCreateProcess(arg->argc, arg->argv,
			 arg->slaveStdin, arg->slaveStdout, arg->slaveStderr,
			 FALSE, FALSE,
			 arg->passThrough ? FALSE : TRUE /* debug */,
			 TRUE /* newProcessGroup */,
			 (Tcl_Pid *) &arg->process, &arg->globalPid);
    if (arg->result) {
	arg->lastError = GetLastError();
    }

    /* Make sure we ignore Ctrl-C */
    SetConsoleCtrlHandler(NULL, TRUE);
    SetEvent(arg->event);

    if (arg->result) {
	ExitThread(0);
    }

    if (! arg->passThrough) {
	ExpProcess *proc;

        ExpAddToWaitQueue(arg->process);

	proc = ExpProcessNew();
	proc->hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, arg->globalPid);
	proc->hPid = arg->globalPid;

	if (proc->hProcess == NULL) {
	  arg->lastError = GetLastError();
	  ExitThread(0);
	}

	proc->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	ExpCommonDebugger();
    } else {
	ExpProcess *proc;
	ExpAddToWaitQueue(arg->process);

	proc = ExpProcessNew();
	proc->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	proc->hProcess = arg->process;
	proc->hPid = arg->globalPid;
    }

    return 0;			/* Never executes */
}

/*
 *----------------------------------------------------------------------
 *
 * ExpCommonDebugger --
 *
 *	This is the function that is the debugger for all slave processes
 *
 * Results:
 *	None.  This thread exits with ExitThread() when the subprocess dies.
 *
 * Side Effects:
 *	Adds the process to the things being waited for by
 *	WaitForMultipleObjects
 *
 *----------------------------------------------------------------------
 */
void
ExpCommonDebugger()
{
    DEBUG_EVENT debEvent;	/* debugging event info. */
    DWORD dwContinueStatus;	/* exception continuation */
    DWORD err;
    ExpProcess *proc;

    for(;;) {
	dwContinueStatus = DBG_CONTINUE;

	/*
	 * Wait for a debugging event to occur. The second parameter
	 * indicates that the function does not return until
	 * a debugging event occurs.
	 */

	if (WaitForDebugEvent(&debEvent, INFINITE) == FALSE) {
	    err = GetLastError();
	    *((char *) NULL) = 0;
	}

	/*
	 * Find the process that is responsible for this event.
	 */
	for (proc = ProcessList; proc; proc = proc->nextPtr) {
	    if (proc->hPid == debEvent.dwProcessId) {
		break;
	    }
	}

	if (!proc && debEvent.dwDebugEventCode != CREATE_PROCESS_DEBUG_EVENT) {
	    char buf[50];
	    sprintf(buf, "%d/%d (%d)", 
		    debEvent.dwProcessId, debEvent.dwThreadId,
		    debEvent.dwDebugEventCode);
	    EXP_LOG("Unexpected debug event for %s", buf);
	    continue;
	}

	/* Process the debugging event code. */

	switch (debEvent.dwDebugEventCode) {
	case EXCEPTION_DEBUG_EVENT:
	    /*
	     * Process the exception code. When handling
	     * exceptions, remember to set the continuation
	     * status parameter (dwContinueStatus). This value
	     * is used by the ContinueDebugEvent function.
	     */

	    switch (debEvent.u.Exception.ExceptionRecord.ExceptionCode) {
	    case EXCEPTION_BREAKPOINT:
	    {
		if (proc->nBreakCount < 1000) {
		    proc->nBreakCount++;
		}
		if (proc->nBreakCount == 1) {
		    OnXFirstBreakpoint(proc, &debEvent);
		} else if (proc->nBreakCount == 2) {
		    OnXSecondBreakpoint(proc, &debEvent);
		} else {
		    OnXBreakpoint(proc, &debEvent);
		}
		break;
	    }

	    case EXCEPTION_SINGLE_STEP:
		OnXSingleStep(proc, &debEvent);
		break;

	    case DBG_CONTROL_C:
		/* fprintf(stderr, "Saw DBG_CONTROL_C event\n"); */
		dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
		break;

	    case DBG_CONTROL_BREAK:
		/* fprintf(stderr, "Saw DBG_CONTROL_BREAK event\n"); */
		dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
		break;

	    case EXCEPTION_DATATYPE_MISALIGNMENT:
	    case EXCEPTION_ACCESS_VIOLATION:
	    default:
		dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;

	    }
	    break;

	case CREATE_THREAD_DEBUG_EVENT:
#if 0
	    fprintf(stderr, "Process %d creating thread %d\n", proc->hPid, 
		    debEvent.dwThreadId);
#endif
	    OnXCreateThread(proc, &debEvent);
	    break;

	case CREATE_PROCESS_DEBUG_EVENT:
#if 0
	    fprintf(stderr, "Process %d starting...\n", debEvent.dwProcessId);
#endif
	    OnXCreateProcess(proc, &debEvent);
	    break;

	case EXIT_THREAD_DEBUG_EVENT:
#if 0
	    fprintf(stderr, "Process %d thread %d exiting\n", proc->hPid,
		    debEvent.dwThreadId);
#endif
	    OnXDeleteThread(proc, &debEvent);
	    break;

	case EXIT_PROCESS_DEBUG_EVENT:
	    CloseHandle(proc->overlapped.hEvent);
	    CloseHandle(proc->hProcess);
	    err = debEvent.u.ExitProcess.dwExitCode;
	    ExpProcessFree(proc);
	    /*
	     * When the last process exits, we exit.
	     */
	    if (ProcessList == NULL) {
		ExitThread(0);
	    }
	    break;

	case LOAD_DLL_DEBUG_EVENT:
	    OnXLoadDll(proc, &debEvent);
	    break;

	case UNLOAD_DLL_DEBUG_EVENT:
#if 0
	    fprintf(stderr, "0x%08x: Unloading\n", debEvent.u.UnloadDll.lpBaseOfDll);
#endif
	    /*
	     * Display a message that the DLL has
	     * been unloaded.
	     */
	    break;

	case OUTPUT_DEBUG_STRING_EVENT:
	    /* Display the output debugging string. */
	    break;
	}

	/* Resume executing the thread that reported the debugging event. */
	ContinueDebugEvent(debEvent.dwProcessId,
			   debEvent.dwThreadId, dwContinueStatus);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * OnXCreateProcess --
 *
 *	This routine is called when a CREATE_PROCESS_DEBUG_EVENT
 *	occurs.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
OnXCreateProcess(ExpProcess *proc, LPDEBUG_EVENT pDebEvent)
{
    ExpThreadInfo *threadInfo;

    if (proc == NULL) {
	proc = ExpProcessNew();
	proc->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	
	DuplicateHandle(GetCurrentProcess(),
			pDebEvent->u.CreateProcessInfo.hProcess,
			GetCurrentProcess(),
			&proc->hProcess, SYNCHRONIZE|PROCESS_ALL_ACCESS,
			FALSE, 0);
	proc->hPid = pDebEvent->dwProcessId;

	ExpAddToWaitQueue(proc->hProcess);
    }

    /*
     * As needed, examine or change the registers of the
     * process's initial thread with the GetThreadContext and
     * SetThreadContext functions; read from and write to the
     * process's virtual memory with the ReadProcessMemory and
     * WriteProcessMemory functions; and suspend and resume
     * thread execution with the SuspendThread and ResumeThread
     * functions.
     */

    threadInfo = (ExpThreadInfo *) malloc(sizeof(ExpThreadInfo));
    threadInfo->dwThreadId = pDebEvent->dwThreadId;
    threadInfo->hThread = pDebEvent->u.CreateProcessInfo.hThread;
    threadInfo->nextPtr = proc->threadList;
    proc->threadCount++;
    proc->threadList = threadInfo;
    
    /*
     * We don't use the file handle, so close it.
     */
    if (pDebEvent->u.CreateProcessInfo.hFile != NULL) {
	CloseHandle(pDebEvent->u.CreateProcessInfo.hFile);
    }
    CloseHandle(pDebEvent->u.CreateProcessInfo.hProcess);
}

/*
 *----------------------------------------------------------------------
 *
 * OnXCreateThread --
 *
 *	This routine is called when a CREATE_THREAD_DEBUG_EVENT
 *	occurs.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
OnXCreateThread(ExpProcess *proc, LPDEBUG_EVENT pDebEvent)
{
    /*
     * As needed, examine or change the thread's registers
     * with the GetThreadContext and SetThreadContext functions;
     * and suspend and resume thread execution with the
     * SuspendThread and ResumeThread functions.
     */
    ExpThreadInfo *threadInfo;

    threadInfo = (ExpThreadInfo *) malloc(sizeof(ExpThreadInfo));
    threadInfo->dwThreadId = pDebEvent->dwThreadId;
    threadInfo->hThread = pDebEvent->u.CreateThread.hThread;
    proc->threadCount++;
    threadInfo->nextPtr = proc->threadList;
    proc->threadList = threadInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * OnXDeleteThread --
 *
 *	This routine is called when a CREATE_THREAD_DEBUG_EVENT
 *	occurs.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
OnXDeleteThread(ExpProcess *proc, LPDEBUG_EVENT pDebEvent)
{
    /*
     * As needed, examine or change the thread's registers
     * with the GetThreadContext and SetThreadContext functions;
     * and suspend and resume thread execution with the
     * SuspendThread and ResumeThread functions.
     */
    ExpThreadInfo *threadInfo;
    ExpThreadInfo *prev;

    prev = NULL;
    for (threadInfo = proc->threadList; threadInfo;
	 prev = threadInfo, threadInfo = threadInfo->nextPtr)
    {
	if (threadInfo->dwThreadId == pDebEvent->dwThreadId) {
	    if (prev == NULL) {
		proc->threadList = threadInfo->nextPtr;
	    } else {
		prev->nextPtr = threadInfo->nextPtr;
	    }
	    proc->threadCount--;
	    CloseHandle(threadInfo->hThread);
	    free(threadInfo);
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * OnXFirstBreakpoint --
 *
 *	This routine is called when a EXCEPTION_DEBUG_EVENT with
 *	an exception code of EXCEPTION_BREAKPOINT, and it is the
 *	first one to occur in the program.  This happens when the
 *	process finally gets loaded into memory and is about to
 *	start.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static CONTEXT FirstContext;
static UCHAR   FirstPage[PAGESIZE];
static HANDLE  FirstThread;
#pragma pack(push,1)
typedef struct _InjectCode {
    UCHAR instPush1;
    DWORD argMemProtect;
    UCHAR instPush2;
    DWORD argMemType;
    UCHAR instPush3;
    DWORD argMemSize;
    UCHAR instPush4;
    DWORD argMemAddr;
    UCHAR instCall;
    DWORD argCallAddr;
    DWORD instIntr;
} InjectCode;
#pragma pack(pop)

static void
OnXFirstBreakpoint(ExpProcess *proc, LPDEBUG_EVENT pDebEvent)
{
    DWORD base;
    ExpThreadInfo *tinfo;

#if 0
    fprintf(stderr, "OnXFirstBreakpoint: proc=0x%08x\n", proc);
#endif
    for (tinfo = proc->threadList; tinfo != NULL; tinfo = tinfo->nextPtr) {
	if (pDebEvent->dwThreadId == tinfo->dwThreadId) {
	    break;
	}
    }

    /*
     * Set up the memory that will serve as the place for our
     * intercepted function return points.
     */

    {
	InjectCode code;
	Tcl_HashEntry *tclEntry;
	DWORD addr;

	FirstThread = tinfo->hThread;
	FirstContext.ContextFlags = CONTEXT_FULL;
	GetThreadContext(FirstThread, &FirstContext);

	tclEntry = Tcl_FindHashEntry(proc->funcTable, "VirtualAlloc");
	if (tclEntry == NULL) {
	    proc->nBreakCount++;	/* Don't stop at second breakpoint */
	    EXP_LOG("Unable to find entry for VirtualAlloc", NULL);
	    return;
	}
	addr = (DWORD) Tcl_GetHashValue(tclEntry);

	code.instPush1     = 0x68;
	code.argMemProtect = PAGE_EXECUTE_READWRITE;
	code.instPush2     = 0x68;
	code.argMemType    = MEM_COMMIT;
	code.instPush3     = 0x68;
	code.argMemSize    = 2048;
	code.instPush4     = 0x68;
	code.argMemAddr    = 0;
	code.instCall      = 0xe8;
	code.argCallAddr   = addr - FirstContext.Eip - offsetof(InjectCode, instCall) - 5;
	code.instIntr      = 0xCC;

	base = FirstContext.Eip;
	if (!ReadSubprocessMemory(proc, (PVOID) base, FirstPage, sizeof(InjectCode))) {
	    EXP_LOG("Error reading subprocess memory", NULL);
	}
	if (!WriteSubprocessMemory(proc, (PVOID) base, &code, sizeof(InjectCode))) {
	    EXP_LOG("Error reading subprocess memory", NULL);
	}
    }
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * OnXSecondBreakpoint --
 *
 *	This routine is called when the second breakpoint is hit.
 *	The second breakpoint is at the end of our call to GlobalAlloc().
 *	Save the returned pointer from GlobalAlloc, then restore the
 *	first page of memory and put everything back the way it was.
 *	Finally, we can start.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
OnXSecondBreakpoint(ExpProcess *proc, LPDEBUG_EVENT pDebEvent)
{
    CONTEXT context;
    UCHAR retbuf[2048];
    DWORD base;
    LPEXCEPTION_DEBUG_INFO exceptInfo;
    ExpBreakInfo *info;

    exceptInfo = &pDebEvent->u.Exception;

    context.ContextFlags = CONTEXT_FULL;
    GetThreadContext(FirstThread, &context);
    proc->pSubprocessMemory = context.Eax;

    memset(retbuf, 0xCC, sizeof(retbuf));	/* All breakpoints */
    WriteSubprocessMemory(proc, (PVOID) proc->pSubprocessMemory,
			  retbuf, sizeof(retbuf));

    base = FirstContext.Eip;
    if (!WriteSubprocessMemory(proc, (PVOID) base, FirstPage, sizeof(InjectCode))) {
	EXP_LOG("Error writing subprocess memory", NULL);
    }
    SetThreadContext(FirstThread, &FirstContext);

    /*
     * Set breakpoints in kernel32.dll
     */
    for (info = BreakArrayKernel32; info->funcName; info++) {
	SetBreakpoint(proc, info);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * OnXBreakpoint --
 *
 *	This routine is called when a EXCEPTION_DEBUG_EVENT with
 *	an exception code of EXCEPTION_BREAKPOINT.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
OnXBreakpoint(ExpProcess *proc, LPDEBUG_EVENT pDebEvent)
{
    LPEXCEPTION_DEBUG_INFO exceptInfo;
    CONTEXT context;
    ExpThreadInfo *tinfo;
    ExpBreakpoint *pbrkpt, *brkpt;
    PDWORD pdw;
    DWORD i;
    DWORD dw;

    for (tinfo = proc->threadList; tinfo != NULL; tinfo = tinfo->nextPtr) {
	if (pDebEvent->dwThreadId == tinfo->dwThreadId) {
	    break;
	}
    }
    assert(tinfo != NULL);

    exceptInfo = &pDebEvent->u.Exception;
#if 0
    fprintf(stderr, "OnXBreakpoint: proc=0x%x, count=%d, addr=0x%08x\n", proc, proc->nBreakCount, exceptInfo->ExceptionRecord.ExceptionAddress);
#endif
    pbrkpt = NULL;
    for (brkpt = proc->brkptList; brkpt != NULL;
	 pbrkpt = brkpt, brkpt = brkpt->nextPtr) {
	if (brkpt->codePtr == exceptInfo->ExceptionRecord.ExceptionAddress) {
	    if (brkpt->threadInfo == NULL) {
		break;
	    }
	    if (brkpt->threadInfo == tinfo) {
		break;
	    }
	}
    }
#if 0 /* Allow user breakpoints to be hit when we are not debugging */
    if (brkpt == NULL) {
	fprintf(stderr, "OnXBreakpoint: proc=0x%x, count=%d, addr=0x%08x\n", proc, proc->nBreakCount, exceptInfo->ExceptionRecord.ExceptionAddress);
    }
    assert(brkpt != NULL);
#endif

    context.ContextFlags = CONTEXT_FULL;
    GetThreadContext(tinfo->hThread, &context);
    if (! brkpt->returning) {
	ExpBreakpoint *bpt;
	/*
	 * Get the arguments to the function and store them in the thread
	 * specific data structure.
	 */
	for (pdw = tinfo->args, i=0; i < brkpt->breakInfo->nargs; i++, pdw++) {
	    ReadSubprocessMemory(proc, (PVOID) (context.Esp+(4*(i+1))),
				 pdw, sizeof(DWORD));
	}
	tinfo->nargs = brkpt->breakInfo->nargs;
	tinfo->context = &context;

	if (brkpt->breakInfo->dwFlags & EXP_BREAK_IN) {
	    brkpt->breakInfo->breakProc(proc, tinfo, brkpt, context.Eax, EXP_BREAK_IN);
	}

	/*
	 * Only set a return breakpoint if something is interested
	 * in the return value
	 */
	if (brkpt->breakInfo->dwFlags & EXP_BREAK_OUT) {
	    bpt = (ExpBreakpoint *) malloc(sizeof(ExpBreakpoint));
	    ReadSubprocessMemory(proc, (PVOID) context.Esp,
		&bpt->origRetAddr, sizeof(DWORD));
	    dw = (DWORD) brkpt->codeReturnPtr;
	    WriteSubprocessMemory(proc, (PVOID) context.Esp,
		&dw, sizeof(DWORD));
	    bpt->codePtr = brkpt->codeReturnPtr;
	    bpt->returning = TRUE;
	    bpt->codeReturnPtr = NULL;	/* Doesn't matter */
	    bpt->breakInfo = brkpt->breakInfo;
	    bpt->threadInfo = tinfo;
	    bpt->nextPtr = proc->brkptList;
	    proc->brkptList = bpt;

	}

	/*
	 * Now, we need to restore the original code for this breakpoint.
	 * Put the program counter back, then do a single-step and put
	 * the breakpoint back again.
	 */
	WriteSubprocessMemory(proc, brkpt->codePtr,
	    &brkpt->code, sizeof(UCHAR));

	context.EFlags |= SINGLE_STEP_BIT;
	context.Eip--;

	proc->lastBrkpt = brkpt;
    } else {
	/*
	 * Make the callback with the params and the return value
	 */
	if (brkpt->breakInfo->dwFlags & EXP_BREAK_OUT) {
	    brkpt->breakInfo->breakProc(proc, tinfo, brkpt, context.Eax, EXP_BREAK_OUT);
	}
	context.Eip = brkpt->origRetAddr;

	if (pbrkpt == NULL) {
	    proc->brkptList = brkpt->nextPtr;
	} else {
	    pbrkpt->nextPtr = brkpt->nextPtr;
	}
	free(brkpt);
    }
    SetThreadContext(tinfo->hThread, &context);
}

/*
 *----------------------------------------------------------------------
 *
 * OnXSingleStep --
 *
 *	This routine is called when a EXCEPTION_DEBUG_EVENT with
 *	an exception code of EXCEPTION_SINGLE_STEP.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
OnXSingleStep(ExpProcess *proc, LPDEBUG_EVENT pDebEvent)
{
    UCHAR code;
    /*
     * Now, we need to restore the breakpoint that we had removed.
     */
    code = 0xcc;
    WriteSubprocessMemory(proc, proc->lastBrkpt->codePtr, &code, sizeof(UCHAR));
}

/*
 *----------------------------------------------------------------------
 *
 * OnXLoadDll --
 *
 *	This routine is called when a LOAD_DLL_DEBUG_EVENT is seen
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Some information is printed
 *
 *----------------------------------------------------------------------
 */

void
OnXLoadDll(ExpProcess *proc, LPDEBUG_EVENT pDebEvent)
{
    WORD w;
    DWORD dw;
    DWORD ImageHdrOffset;
    PIMAGE_FILE_HEADER pfh;	/* File header image in subprocess memory */
    PIMAGE_SECTION_HEADER psh;
    PIMAGE_OPTIONAL_HEADER poh;
    IMAGE_DATA_DIRECTORY dataDir;
    PIMAGE_EXPORT_DIRECTORY ped;
    IMAGE_EXPORT_DIRECTORY exportDir;
    DWORD n;
    DWORD base;
    CHAR funcName[256];
    CHAR dllname[256];
    PVOID ptr, namePtr, funcPtr;
    DWORD p;
    LPLOAD_DLL_DEBUG_INFO info = &pDebEvent->u.LoadDll;
    Tcl_HashEntry *tclEntry;
    int isNew;

#if 0 /* Debugging purposes */
    int unknown = 0;

    if (info->lpImageName) {

	/*
	 * This info->lpImageName is a pointer to the name of the
	 * DLL in the process space of the subprocess
	 */
	if (ReadSubprocessMemory(proc, info->lpImageName, &ptr, sizeof(PVOID)) &&
	    ptr)
	{
	    if (info->fUnicode) {
		WCHAR name[256];
		ReadSubprocessStringW(proc, ptr, name, 256);
		printf("0x%08x: Loaded %S\n", info->lpBaseOfDll, name);
	    } else {
		CHAR name[256];
		ReadSubprocessStringA(proc, ptr, name, 256);
		printf("0x%08x: Loaded %S\n", info->lpBaseOfDll, name);
	    }
	} else {
	    printf("0x%08x: Loaded DLL, but couldn't read subprocess' memory\n", info->lpBaseOfDll);
	    unknown = 1;
	}
	if (info->dwDebugInfoFileOffset) {
	    printf(" with debugging info at offset 0x%08x\n",
		   info->dwDebugInfoFileOffset);
	}
    } else {
	printf("0x%08x: Loaded DLL with no known name\n", info->lpBaseOfDll);
    }
#endif

    base = (DWORD) info->lpBaseOfDll;

    /*
     * Check for the DOS signature
     */
    ReadSubprocessMemory(proc, info->lpBaseOfDll, &w, sizeof(WORD));
    if (w != IMAGE_DOS_SIGNATURE) return;
    
    /*
     * Skip over the DOS signature and check the NT signature
     */
    p = base;
    p += 15 * sizeof(DWORD);
    ptr = (PVOID) p;
    ReadSubprocessMemory(proc, (PVOID) p, &ImageHdrOffset, sizeof(DWORD));

    p = base;
    p += ImageHdrOffset;
    ReadSubprocessMemory(proc, (PVOID) p, &dw, sizeof(DWORD));
    if (dw != IMAGE_NT_SIGNATURE) {
	return;
    }
    ImageHdrOffset += sizeof(DWORD);
    p += sizeof(DWORD);

    pfh = (PIMAGE_FILE_HEADER) p;
    ptr = &pfh->SizeOfOptionalHeader;
    ReadSubprocessMemory(proc, ptr, &w, sizeof(WORD));

    /*
     * We want to find the exports section.  It can be found in the
     * data directory that is part of the IMAGE_OPTIONAL_HEADER
     */
    if (!w) return;
    p += sizeof(IMAGE_FILE_HEADER);
    poh = (PIMAGE_OPTIONAL_HEADER) p;

    /*
     * Find the number of entries in the data directory
     */
    ptr = &poh->NumberOfRvaAndSizes;
    ReadSubprocessMemory(proc, ptr, &dw, sizeof(DWORD));
    if (dw == 0) return;

    /*
     * Read the export data directory
     */
    ptr = &poh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    ReadSubprocessMemory(proc, ptr, &dataDir, sizeof(IMAGE_DATA_DIRECTORY));

    /*
     * This points us to the exports section
     */
    ptr = (PVOID) (base + dataDir.VirtualAddress);
    ped = (PIMAGE_EXPORT_DIRECTORY) ptr;
    ReadSubprocessMemory(proc, ptr, &exportDir, sizeof(IMAGE_EXPORT_DIRECTORY));

    /*
     * See if this is a DLL we are interested in
     */
    ptr = &ped->Name;
    ReadSubprocessMemory(proc, ptr, &dw, sizeof(DWORD));
    ptr = (PVOID) (base + dw);
    ReadSubprocessStringA(proc, ptr, dllname, sizeof(dllname));

#if 0 /* Debugging purposes */
    /*
     * We now have the DLL name, even if it was unknown.
     */
    if (unknown) {
	printf("0x%08x: Loaded %s\n", info->lpBaseOfDll, dllname);
    }
#endif

    if (stricmp(dllname, "kernel32.dll") != 0) {
	CloseHandle(info->hFile);
	return;
    }

    ptr = (PVOID) (base + (DWORD) exportDir.AddressOfNames);
    for (n = 0; n < exportDir.NumberOfNames; n++) {
	ReadSubprocessMemory(proc, ptr, &dw, sizeof(DWORD));
	namePtr = (PVOID) (base + dw);
	/*
	 * Now, we should hopefully have a pointer to the name of the
	 * function, so lets get it.
	 */
	ReadSubprocessStringA(proc, namePtr, funcName, sizeof(funcName));
	/* printf("%s\n", funcName); */

	/*
	 * Keep a list of all function names in a hash table
	 */

	funcPtr = (PVOID) (base + n*sizeof(DWORD) +
	    (DWORD) exportDir.AddressOfFunctions);
	ReadSubprocessMemory(proc, funcPtr, &dw, sizeof(DWORD));
	funcPtr = (PVOID) (base + dw);

	tclEntry = Tcl_CreateHashEntry(proc->funcTable, funcName, &isNew);
	Tcl_SetHashValue(tclEntry, funcPtr);

	ptr = (PVOID) (sizeof(DWORD) + (ULONG) ptr);
    }

    /*
     * The IMAGE_SECTION_HEADER comes after the IMAGE_OPTIONAL_HEADER
     * (if the IMAGE_OPTIONAL_HEADER exists)
     */
    p += w;

    psh = (PIMAGE_SECTION_HEADER) p;
    CloseHandle(info->hFile);
}

/*
 *----------------------------------------------------------------------
 *
 * SetBreakpoint --
 *
 *	Inserts a single breakpoint
 *
 * Results:
 *	TRUE if successful, FALSE if unsuccessful.
 *
 *----------------------------------------------------------------------
 */

static BOOL
SetBreakpoint(ExpProcess *proc, ExpBreakInfo *info)
{
    Tcl_HashEntry *tclEntry;
    PVOID funcPtr;

    tclEntry = Tcl_FindHashEntry(proc->funcTable, info->funcName);
    if (tclEntry == NULL) {
	return FALSE;
    }

#if 0
    fprintf(stderr, "%s: ", info->funcName);
#endif
    /*
     * Set a breakpoint at the function start in the subprocess and
     * save the original code at the function start.
     */
    funcPtr = Tcl_GetHashValue(tclEntry);
    SetBreakpointAtAddr(proc, info, funcPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * SetBreakpointAtAddr --
 *
 *	Inserts a single breakpoint at the given address
 *
 * Results:
 *	TRUE if successful, FALSE if unsuccessful.
 *
 *----------------------------------------------------------------------
 */

ExpBreakpoint *
SetBreakpointAtAddr(ExpProcess *proc, ExpBreakInfo *info, PVOID funcPtr)
{
    ExpBreakpoint *bpt;
    UCHAR code;

#if 0
    fprintf(stderr, "SetBreakpointAtAddr: addr=0x%08x\n", funcPtr);
#endif
    bpt = (ExpBreakpoint *) malloc(sizeof(ExpBreakpoint));
    bpt->returning = FALSE;
    bpt->codePtr = funcPtr;
    bpt->codeReturnPtr = (PVOID) (proc->offset + (DWORD) proc->pSubprocessMemory);
    bpt->origRetAddr = 0;
    bpt->breakInfo = info;
    bpt->threadInfo = NULL;
    proc->offset += 2;
    bpt->nextPtr = proc->brkptList;
    proc->brkptList = bpt;

    ReadSubprocessMemory(proc, funcPtr, &bpt->code, sizeof(UCHAR));
    code = 0xcc;	/* Breakpoint opcode on i386 */
    WriteSubprocessMemory(proc, funcPtr, &code, sizeof(UCHAR));
    return bpt;
}

/*
 *----------------------------------------------------------------------
 *
 * OnOpenConsoleW --
 *
 *	This function gets called when an OpenConsoleW breakpoint
 *	is hit.  There is one big problem with this function--it
 *	isn't documented.  However, we only really care about the
 *	return value which is a console handle.  I think this is
 *	what this function declaration should be:
 *
 *	HANDLE OpenConsoleW(LPWSTR lpFileName,
 *			    DWORD dwDesiredAccess,
 *			    DWORD dwShareMode,
 *			    LPSECURITY_ATTRIBUTES lpSecurityAttributes);
 *
 *	So why do we intercept an undocumented function while we
 *	could just intercept CreateFileW and CreateFileA?  Well,
 *	those functions are going to get called alot more than this
 *	one, so limiting the number of intercepted functions
 *	improves performance since fewer breakpoints will be hit.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Save the return value in an array of known console handles
 *	with their statuses.
 *
 *----------------------------------------------------------------------
 */

static void
OnOpenConsoleW(ExpProcess *proc, ExpThreadInfo *threadInfo,
    ExpBreakpoint *brkpt, DWORD returnValue, DWORD direction)
{
    WCHAR name[256];
    PVOID ptr;

    if (returnValue == (DWORD) INVALID_HANDLE_VALUE) {
	return;
    }

    /*
     * Save any console input handle.  No SetConsoleMode() calls will
     * succeed unless they are really attached to a console input buffer.
     */

    ptr = (PVOID) threadInfo->args[0];
    ReadSubprocessStringW(proc, ptr, name, 256);

    if (wcsicmp(name, L"CONIN$") == 0) {
	if (proc->consoleHandlesMax > 100) {
	    proc->consoleHandlesMax = 100;
	}
	proc->consoleHandles[proc->consoleHandlesMax++] = returnValue;
    }
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * OnWriteConsoleA --
 *
 *	This function gets called when an WriteConsoleA breakpoint
 *	is hit.  The data is also redirected to expect since expect
 *	normally couldn't see any output going through this interface.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Prints some output.
 *
 *----------------------------------------------------------------------
 */

static void
OnWriteConsoleA(ExpProcess *proc, ExpThreadInfo *threadInfo,
    ExpBreakpoint *brkpt, DWORD returnValue, DWORD direction)
{
    CHAR buf[1024];
    PVOID ptr;
    DWORD n, count;
    PCHAR p;
    BOOL bRet;
    DWORD dwResult;

#if 0
    fprintf(stderr, "OnWriteConsoleA\n");
#endif

    if (returnValue == 0) {
	return;
    }
    /*
     * Get number of bytes written
     */
    ptr = (PVOID) threadInfo->args[3];
    if (ptr == NULL) {
	n = threadInfo->args[2];
    } else {
	ReadSubprocessMemory(proc, ptr, &n, sizeof(DWORD));
    }
    if (n > 1024) {
	p = malloc(n * sizeof(CHAR));
    } else {
	p = buf;
    }

    ptr = (PVOID) threadInfo->args[1];
    ReadSubprocessMemory(proc, ptr, p, n * sizeof(CHAR));
    ResetEvent(proc->overlapped.hEvent);

    bRet = WriteFile(HMaster, p, n, &count, &proc->overlapped);
    if (bRet == FALSE) {
	dwResult = GetLastError();
	if (dwResult == ERROR_IO_PENDING) {
	    bRet = GetOverlappedResult(HMaster, &proc->overlapped, &count, TRUE);
	}
    }

    if (p != buf) {
	free(p);
    }
    CursorKnown = FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * OnWriteConsoleW --
 *
 *	This function gets called when an WriteConsoleW breakpoint
 *	is hit.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Prints some output.
 *
 *----------------------------------------------------------------------
 */

static void
OnWriteConsoleW(ExpProcess *proc, ExpThreadInfo *threadInfo,
    ExpBreakpoint *brkpt, DWORD returnValue, DWORD direction)
{
    WCHAR buf[1024];
    CHAR ansi[2048];
    PVOID ptr;
    DWORD n, count;
    PWCHAR p;
    PCHAR a;
    BOOL bRet;
    DWORD dwResult;
    int w;

    if (returnValue == 0) {
	return;
    }

    ptr = (PVOID) threadInfo->args[1];
    n = threadInfo->args[2];

    if (n > 1024) {
	p = malloc(n * sizeof(WCHAR));
	a = malloc(n * 2 * sizeof(CHAR));
    } else {
	p = buf;
	a = ansi;
    }
    ReadSubprocessMemory(proc, ptr, p, n * sizeof(WCHAR));
    ResetEvent(proc->overlapped.hEvent);

    /*
     * Convert to ASCI and Write the intercepted data to the pipe.
     */

    w = WideCharToMultiByte(CP_ACP, 0, buf, n, ansi, sizeof(ansi), NULL, NULL);
    bRet = WriteFile(HMaster, ansi, w, &count, &proc->overlapped);
    if (bRet == FALSE) {
	dwResult = GetLastError();
	if (dwResult == ERROR_IO_PENDING) {
	    bRet = GetOverlappedResult(HMaster, &proc->overlapped, &count, TRUE);
	}
    }

    if (p != buf) {
	free(p);
	free(a);
    }
    CursorKnown = FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateVtSequence --
 *
 *	When moving the cursor to a new location, this will create
 *	the appropriate VT100 type sequence to get the cursor there.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Characters are written to the pipe going to Expect
 *
 *----------------------------------------------------------------------
 */

void
CreateVtSequence(ExpProcess *proc, COORD newPos, DWORD n)
{
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    COORD oldPos;
    CHAR buf[2048];
    DWORD count;
    BOOL b;
    DWORD dwResult;

    if (n == 0) {
	return;
    }

    GetConsoleScreenBufferInfo(ExpConsoleOut, &consoleInfo);
    /* oldPos = consoleInfo.dwCursorPosition; */
    oldPos = CursorPosition;

    b = FALSE;
    if (CursorKnown) {
	if (newPos.Y == oldPos.Y && newPos.X <= oldPos.X) {
	    memset(buf, '\b', oldPos.X - newPos.X);
	    count = oldPos.X - newPos.X;
	    b = TRUE;
	} else if ((newPos.Y > oldPos.Y) && (newPos.X == 0)) {
	    buf[0] = '\r';
	    memset(&buf[1], '\n', newPos.Y - oldPos.Y);
	    count = 1 + newPos.Y - oldPos.Y;
	    b = TRUE;
	}
    }
    if (!b) {
	/* VT100 sequence */
	wsprintfA(buf, "\033[%d;%dH", newPos.X, newPos.Y);
	count = strlen(buf);
    }
    newPos.X += (SHORT) (n % 80); /* XXX: Want the console screen buffer width */
    newPos.Y += (SHORT) (n / 80);
    CursorPosition = newPos;
    CursorKnown = TRUE;

    b = WriteFile(HMaster, buf, count, &count, &proc->overlapped);
    if (b == FALSE) {
	dwResult = GetLastError();
	if (dwResult == ERROR_IO_PENDING) {
	    b = GetOverlappedResult(HMaster, &proc->overlapped, &count, TRUE);
	}
    }
}


/*
 *----------------------------------------------------------------------
 *
 * OnWriteConsoleOutputCharacterA --
 *
 *	This function gets called when an WriteConsoleOutputCharacterA breakpoint
 *	is hit.  The data is also redirected to expect since expect
 *	normally couldn't see any output going through this interface.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Prints some output.
 *
 *----------------------------------------------------------------------
 */

static void
OnWriteConsoleOutputCharacterA(ExpProcess *proc, ExpThreadInfo *threadInfo,
    ExpBreakpoint *brkpt, DWORD returnValue, DWORD direction)
{
    CHAR buf[1024];
    PVOID ptr;
    DWORD n, count;
    PCHAR p;
    BOOL b;
    DWORD dwResult;

    if (returnValue == 0) {
	return;
    }
    /*
     * Get number of bytes written
     */
    ptr = (PVOID) threadInfo->args[4];
    if (ptr == NULL) {
	n = threadInfo->args[2];
    } else {
	ReadSubprocessMemory(proc, ptr, &n, sizeof(DWORD));
    }

    CreateVtSequence(proc, *((PCOORD) &threadInfo->args[3]), n);

    if (n > 1024) {
	p = malloc(n * sizeof(CHAR));
    } else {
	p = buf;
    }

    ptr = (PVOID) threadInfo->args[1];
    ReadSubprocessMemory(proc, ptr, p, n * sizeof(CHAR));
    ResetEvent(proc->overlapped.hEvent);

    b = WriteFile(HMaster, p, n, &count, &proc->overlapped);
    if (b == FALSE) {
	dwResult = GetLastError();
	if (dwResult == ERROR_IO_PENDING) {
	    b = GetOverlappedResult(HMaster, &proc->overlapped, &count, TRUE);
	}
    }

    if (p != buf) {
	free(p);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * OnWriteConsoleOutputCharacterW --
 *
 *	This function gets called when an WriteConsoleOutputCharacterW breakpoint
 *	is hit.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Prints some output.
 *
 *----------------------------------------------------------------------
 */

static void
OnWriteConsoleOutputCharacterW(ExpProcess *proc, ExpThreadInfo *threadInfo,
    ExpBreakpoint *brkpt, DWORD returnValue, DWORD direction)
{
    WCHAR buf[1024];
    CHAR ansi[2048];
    PVOID ptr;
    DWORD n, count;
    PWCHAR p;
    PCHAR a;
    BOOL b;
    DWORD dwResult;
    int w;

    if (returnValue == 0) {
	return;
    }
    /*
     * Get number of bytes written
     */
    ptr = (PVOID) threadInfo->args[4];
    if (ptr == NULL) {
	n = threadInfo->args[2];
    } else {
	ReadSubprocessMemory(proc, ptr, &n, sizeof(DWORD));
    }

    CreateVtSequence(proc, *((PCOORD) &threadInfo->args[3]), n);

    if (n > 1024) {
	p = malloc(n * sizeof(WCHAR));
	a = malloc(n * 2 * sizeof(CHAR));
    } else {
	p = buf;
	a = ansi;
    }
    ReadSubprocessMemory(proc, ptr, p, n * sizeof(WCHAR));
    ResetEvent(proc->overlapped.hEvent);

    /*
     * Convert to ASCI and Write the intercepted data to the pipe.
     */

    w = WideCharToMultiByte(CP_ACP, 0, buf, n, ansi, sizeof(ansi), NULL, NULL);
    b = WriteFile(HMaster, ansi, w, &count, &proc->overlapped);
    if (b == FALSE) {
	dwResult = GetLastError();
	if (dwResult == ERROR_IO_PENDING) {
	    b = GetOverlappedResult(HMaster, &proc->overlapped, &count, TRUE);
	}
    }

    if (p != buf) {
	free(p);
	free(a);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * OnSetConsoleMode --
 *
 *	This function gets called when a SetConsoleMode breakpoint
 *	is hit.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Sets some flags that are used in determining echoing
 *	characteristics of the slave driver.
 *
 *----------------------------------------------------------------------
 */

static void
OnSetConsoleMode(ExpProcess *proc, ExpThreadInfo *threadInfo,
    ExpBreakpoint *brkpt, DWORD returnValue, DWORD direction)
{
    DWORD i;
    BOOL found;

    if (returnValue == FALSE) {
	return;
    }
    for (found = FALSE, i = 0; i < proc->consoleHandlesMax; i++) {
	if (threadInfo->args[0] == proc->consoleHandles[i]) {
	    found = TRUE;
	    break;
	}
    }
    if (found) {
	ExpConsoleInputMode = threadInfo->args[1];
    }
}

/*
 *----------------------------------------------------------------------
 *
 * OnSetConsoleCursorPosition --
 *
 *	This function gets called when a SetConsoleCursorPosition breakpoint
 *	is hit.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Updates the current console cursor position
 *
 *----------------------------------------------------------------------
 */

static void
OnSetConsoleCursorPosition(ExpProcess *proc, ExpThreadInfo *threadInfo,
    ExpBreakpoint *brkpt, DWORD returnValue, DWORD direction)
{
    BOOL b;
    CHAR buf[50];
    DWORD count;
    DWORD dwResult;

    if (returnValue == FALSE) {
	return;
    }
    CursorPosition = *((PCOORD) &threadInfo->args[1]);
    CursorKnown = TRUE;

    wsprintfA(buf, "\033[%d;%dH", CursorPosition.X, CursorPosition.Y);
    count = strlen(buf);
    b = WriteFile(HMaster, buf, count, &count, &proc->overlapped);
    if (b == FALSE) {
	dwResult = GetLastError();
	if (dwResult == ERROR_IO_PENDING) {
	    b = GetOverlappedResult(HMaster, &proc->overlapped, &count, TRUE);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * OnGetStdHandle --
 *
 *	This function gets called when a GetStdHandle breakpoint
 *	is hit.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Sets some flags that are used in determining echoing
 *	characteristics of the slave driver.
 *
 *----------------------------------------------------------------------
 */

static void
OnGetStdHandle(ExpProcess *proc, ExpThreadInfo *threadInfo,
    ExpBreakpoint *brkpt, DWORD returnValue, DWORD direction)
{
    DWORD i;
    BOOL found;

    if (returnValue == (DWORD) INVALID_HANDLE_VALUE) {
	return;
    }
    if (threadInfo->args[0] != STD_INPUT_HANDLE) {
	return;
    }
    for (found = FALSE, i = 0; i < proc->consoleHandlesMax; i++) {
	if (proc->consoleHandles[i] == returnValue) {
	    found = TRUE;
	    break;
	}
    }
    if (! found) {
	if (proc->consoleHandlesMax > 100) {
	    proc->consoleHandlesMax = 100;
	}
	proc->consoleHandles[proc->consoleHandlesMax++] = returnValue;
    }
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * ReadSubprocessMemory --
 *
 *	Reads memory from the subprocess.  Takes care of all the
 *	issues with page protection.
 *
 * Results:
 *	FALSE if unsuccessful, TRUE if successful.
 *
 * Notes:
 *	The efficient memory reading routine is disabled here
 *	because it doesn't quite work right.  I don't see the
 *	problem in the code, but there must be something there
 *	since the test suite fails when run with this code
 *	enabled.  When it works, it should be much faster than
 *	the current safe but slow implementation.
 *
 *----------------------------------------------------------------------
 */

#ifdef XXX
BOOL
ReadSubprocessMemory(ExpProcess *proc, LPVOID addr, LPVOID buf, DWORD len)
{
    DWORD oldProtection = 0;
    MEMORY_BASIC_INFORMATION mbi;
    BOOL ret = TRUE;
    DWORD offset;
    DWORD base, curr, end, n;
    HANDLE hProcess;
    PBYTE bufpos = buf;

    hProcess = proc->hProcess;

    end = len + (DWORD) addr;
    for (curr = (DWORD) addr; curr < end; ) {
	base = curr & (~PAGEMASK);
	offset = curr & PAGEMASK;
	if (offset + len > PAGESIZE) {
	    n = PAGESIZE - offset;
	} else {
	    n = len;
	}
	if (proc->pMemoryCacheBase != (curr & PAGEMASK)) {
	    /* if not committed memory abort */
	    if (!VirtualQueryEx(hProcess, (LPVOID) base, &mbi, sizeof(mbi)) ||
		(mbi.State != MEM_COMMIT))
	    {
		return FALSE;
	    }

	    /* if guarded memory, change protection temporarily */
	    if (!(mbi.Protect & PAGE_READONLY) &&
		!(mbi.Protect & PAGE_READWRITE))
	    {
		VirtualProtectEx(hProcess, (LPVOID) base, PAGESIZE,
		    PAGE_READONLY, &oldProtection);
	    }

	    if (!ReadProcessMemory(hProcess, (LPVOID) base, proc->pMemoryCache,
		PAGESIZE, NULL)) {
		ret = FALSE;
	    }
    
	    /* reset protection if changed */
	    if (oldProtection) {
		VirtualProtectEx(hProcess, (LPVOID) base, PAGESIZE,
		    oldProtection, &oldProtection);
	    }
	    if (ret == FALSE) {
		return FALSE;
	    }
	    proc->pMemoryCacheBase = base;
	}

	memcpy(bufpos, &proc->pMemoryCache[offset], n);
	bufpos += n;
	curr += n;
    }

    return ret;
}


#else
BOOL
ReadSubprocessMemory(ExpProcess *proc, LPVOID addr, LPVOID buf, DWORD len)
{
    DWORD oldProtection = 0;
    MEMORY_BASIC_INFORMATION mbi;
    BOOL ret = TRUE;

    /* if not committed memory abort */
    if (!VirtualQueryEx(proc->hProcess, addr, &mbi, sizeof(mbi)) ||
	mbi.State != MEM_COMMIT)
    {
	return FALSE;
    }
    
    /* if guarded memory, change protection temporarily */
    if (!(mbi.Protect & PAGE_READONLY) && !(mbi.Protect & PAGE_READWRITE)) {
	VirtualProtectEx(proc->hProcess, addr, len, PAGE_READONLY, &oldProtection);
    }
    
    if (!ReadProcessMemory(proc->hProcess, addr, buf, len, NULL)) {
	ret = FALSE;
    }
    
    /* reset protection if changed */
    if (oldProtection) {
	VirtualProtectEx(proc->hProcess, addr, len, oldProtection, &oldProtection);
    }
    return ret;
}
#endif /* XXX */

/*
 *----------------------------------------------------------------------
 *
 * WriteSubprocessMemory --
 *
 *	Writes memory from the subprocess.  Takes care of all the
 *	issues with page protection.
 *
 * Results:
 *	0 if unsuccessful, 1 if successful.
 *
 *----------------------------------------------------------------------
 */

BOOL
WriteSubprocessMemory(ExpProcess *proc, LPVOID addr, LPVOID buf, DWORD len)
{
    DWORD oldProtection = 0;
    MEMORY_BASIC_INFORMATION mbi;
    BOOL ret = TRUE;
    DWORD err;
    HANDLE hProcess;

    hProcess = proc->hProcess;

    /* Flush the read cache */
    proc->pMemoryCacheBase = 0;

    /* if not committed memory abort */
    if (!VirtualQueryEx(hProcess, addr, &mbi, sizeof(mbi)) ||
	mbi.State != MEM_COMMIT)
    {
	ret = FALSE;
	/* assert(ret != FALSE); */
	return ret;
    }
    
    /* if guarded memory, change protection temporarily */
    if (!(mbi.Protect & PAGE_READWRITE)) {
	if (!VirtualProtectEx(hProcess, addr, len, PAGE_READWRITE,
			      &oldProtection)) {
	    err = GetLastError();
	}
    }
    
    if (!WriteProcessMemory(hProcess, addr, buf, len, NULL)) {
	ret = FALSE;
	err = GetLastError();
    }
    
    /* reset protection if changed */
    if (oldProtection) {
	VirtualProtectEx(hProcess, addr, len, oldProtection, &oldProtection);
    }
#if 0 /* Debugging purposes only */
    if (ret == FALSE) {
	assert(ret != FALSE); 
    }
#endif
    return ret;
}
#endif /* !UNICODE */

/*
 * Everything after this point gets compiled twice, once with the UNICODE flag
 * and once without.  This gets us the Unicode and non-Unicode versions
 * of the code that we need
 */
 
/*
 *----------------------------------------------------------------------
 *
 * OnWriteConsoleOutput --
 *
 *	This function gets called when an WriteConsoleOutputA breakpoint
 *	is hit.  The data is also redirected to expect since expect
 *	normally couldn't see any output going through this interface.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Prints some output.
 *
 *----------------------------------------------------------------------
 */

void
OnWriteConsoleOutput(ExpProcess *proc, ExpThreadInfo *threadInfo,
    ExpBreakpoint *brkpt, DWORD returnValue, DWORD direction)
{
    CHAR buf[1024];
    PVOID ptr;
    DWORD n, count;
    PCHAR p;
    BOOL b;
    DWORD dwResult;
    COORD bufferSize;
    COORD bufferCoord;
    COORD curr;
    SMALL_RECT writeRegion;
    CHAR_INFO *charBuf, *pcb;
    SHORT x, y;

    if (returnValue == 0) {
	return;
    }

    bufferSize = *((PCOORD) &threadInfo->args[2]);
    bufferCoord = *((PCOORD) &threadInfo->args[3]);
    ptr = (PVOID) threadInfo->args[4]; /* Get the rectangle written */
    if (ptr == NULL) return;
    ReadSubprocessMemory(proc, ptr, &writeRegion,sizeof(SMALL_RECT));

    ptr = (PVOID) threadInfo->args[1]; /* Get character array */
    if (ptr == NULL) return;

    n = bufferSize.X * bufferSize.Y * sizeof(CHAR_INFO);
    charBuf = malloc(n);
    ReadSubprocessMemory(proc, ptr, charBuf, n);

    pcb = charBuf;
    for (y = 0; y <= writeRegion.Bottom - writeRegion.Top; y++) {
	pcb = charBuf;
	pcb += ((y + bufferCoord.Y) * bufferSize.X * sizeof(CHAR_INFO));
	pcb += (bufferCoord.X * sizeof(CHAR_INFO));
	p = buf;
	for (x = 0; x <= writeRegion.Right - writeRegion.Left; x++, pcb++) {
#ifdef UNICODE
	    *p++ = (CHAR) (pcb->Char.UnicodeChar & 0xff);
#else
	    *p++ = pcb->Char.AsciiChar;
#endif
	}
	curr.X = writeRegion.Left;
	curr.Y = writeRegion.Top + y;
	n = p - buf;
	CreateVtSequence(proc, curr, n);
	ResetEvent(proc->overlapped.hEvent);

	b = WriteFile(HMaster, buf, n, &count, &proc->overlapped);
	if (b == FALSE) {
	    dwResult = GetLastError();
	    if (dwResult == ERROR_IO_PENDING) {
		b = GetOverlappedResult(HMaster, &proc->overlapped, &count, TRUE);
	    }
	}
    }

    free(charBuf);
}

/*
 *----------------------------------------------------------------------
 *
 * ReadSubprocessString --
 *
 *	Read a character string from the subprocess
 *
 * Results:
 *	The length of the string
 *
 *----------------------------------------------------------------------
 */

int
ReadSubprocessString(ExpProcess *proc, PVOID base, PTCHAR buf, int buflen)
{
    PTCHAR ip, op;
    int i;
    
    ip = base;
    op = buf;
    i = 0;
    while (i < buflen-1) {
	if (! ReadSubprocessMemory(proc, ip, op, sizeof(TCHAR))) {
	    break;
	}
	if (*op == 0) break;
	op++; ip++; i++;
    }
    *op = 0;
    return i;
}
