/* 
 * debugger.c --
 *
 *	This is sample code that was used for testing.  The code has
 *	been migrated into expWinSlaveDrv.c, but it is still very useful
 *	on it own.
 *
 * Copyright (c) 1997 by Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Notes:
 * 	This code does not keep track of console handles.  It does prints
 *	mode changes.  expWinSlaveDrv.c keeps track of changes.
 */

/*
 * Even though we won't have access to most of the commands, use the
 * same headers 
 */

#include "tcl.h"
#include "tclPort.h"
#include "expWin.h"
#include <assert.h>

HANDLE HProcess;		/* handle to subprocess */
DWORD PSubprocessMemory;	/* Pointer to allocated memory in subprocess */

#define SINGLE_STEP_BIT 0x100;	/* This only works on the x86 processor */

typedef struct _ExpThreadInfo {
    HANDLE hThread;
    DWORD dwThreadId;
    DWORD nargs;
    DWORD args[16];		/* Space for saving 16 args.  We need this
				 * space while we are waiting for the return
				 * value for the function. */
    struct _ExpThreadInfo *nextPtr;
} ExpThreadInfo;

/*
 * List of threads in the subprocess
 */
static ExpThreadInfo *ThreadList = NULL;


typedef void (ExpDebBreakProc) (ExpThreadInfo *threadInfo, DWORD ret);

typedef struct _ExpDebBreakInfo {
    PUCHAR funcName;		/* Name of function to intercept */
    DWORD nargs;		/* Number of arguments */
    ExpDebBreakProc *breakProc;	/* Function to call when a breakpoint is hit */
} ExpDebBreakInfo;

typedef struct _ExpDebBreakpoint {
    BOOL returning;		/* Is this a returning breakpoint? */
    UCHAR code;			/* Original code */
    PVOID codePtr;		/* Address of original code */
    UINT retCodeOffset;		/* Offset in allocated page for breakpoint
				 * on return */
    DWORD origRetAddr;		/* Original return address */
    ExpDebBreakInfo *breakInfo;	/* Information about the breakpoint */
    ExpThreadInfo *threadInfo;	/* If this breakpoint is for a specific
				 * thread */
    struct _ExpDebBreakpoint *nextPtr;
} ExpDebBreakpoint;

/*
 * List of breakpoints in the subprocess
 */
static ExpDebBreakpoint *BreakpointList = NULL;

static PVOID		AllocProcessMemory(HANDLE hProcess, DWORD numBytes);
static ExpDebBreakInfo *CheckAddBreakpoint(PCHAR funcName);
static void		OnBreakpoint(LPDEBUG_EVENT);
static void		OnCreateProcess(LPDEBUG_EVENT);
static void		OnCreateThread(LPDEBUG_EVENT);
static void		OnFirstBreakpoint(LPDEBUG_EVENT);
static void		OnGetStdHandle(ExpThreadInfo *threadInfo, DWORD ret);
static void		OnLoadDll(LPDEBUG_EVENT);
static void		OnOpenConsoleW(ExpThreadInfo *threadInfo, DWORD ret);
static void		OnWriteConsoleA(ExpThreadInfo *threadInfo, DWORD ret);
static void		OnWriteConsoleW(ExpThreadInfo *threadInfo, DWORD ret);
static void		OnSingleStep(LPDEBUG_EVENT);
static void		OnSetConsoleMode(ExpThreadInfo *threadInfo, DWORD ret);
static BOOL		ReadSubprocessMemory(LPVOID addr, LPVOID buf,
			    DWORD len);
static int		ReadSubprocessStringA(PVOID base, PCHAR buf,
			    int buflen);
static int		ReadSubprocessStringW(PVOID base, PWCHAR buf,
			    int buflen);
static BOOL		WriteSubprocessMemory(LPVOID addr, LPVOID buf,
			    DWORD len);

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	Main entry point from Windows.  The arguments to this program
 *	are used to create the subprocess that this will control.
 *
 *	argv[1] is the program we will run, and all the following
 *	  are its arguments.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The launch of this program will have caused a console to be
 *	allocated.  This will then send commands to subprocesses.
 *
 *----------------------------------------------------------------------
 */

void
main(int argc, char **argv)
{
    DWORD dwResult;
    Tcl_Pid slavePid;		/* Process identifier of slave */
    DWORD globalPid;		/* Globally unique identifier of slave */
    DEBUG_EVENT debEvent;	/* debugging event info. */
    DWORD dwContinueStatus;	/* exception continuation */
  
    if (argc < 2) {
	fprintf(stderr, "Usage: %s progname ...\n");
	exit(1);
    }

    dwResult = ExpCreateProcess(argc-1, &argv[1], NULL, NULL, NULL, FALSE,
				FALSE, TRUE, FALSE, &slavePid, &globalPid);
    if (dwResult) {
	fprintf(stderr, "Unable to create process %s\n", argv[1]);
	exit(1);
    }

    HProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, globalPid);
    if (HProcess == NULL) {
	fprintf(stderr, "Unable to debug process %s\n", argv[1]);
	exit(1);
    }

    dwContinueStatus = DBG_CONTINUE;
    for(;;) {

	/*
	 * Wait for a debugging event to occur. The second parameter
	 * indicates that the function does not return until
	 * a debugging event occurs.
	 */

	WaitForDebugEvent(&debEvent, INFINITE);

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
	    case EXCEPTION_ACCESS_VIOLATION:
		/*
		 * First chance: Pass this on to the kernel.
		 * Last chance: Display an appropriate error.
		 */
		dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
		break;

	    case EXCEPTION_BREAKPOINT:
	    {
		static BOOL firstBreakpoint = 1;

		if (firstBreakpoint) {
		    firstBreakpoint = 0;
		    OnFirstBreakpoint(&debEvent);
		} else {
		    OnBreakpoint(&debEvent);
		}
		break;
	    }

	    case EXCEPTION_SINGLE_STEP:
		OnSingleStep(&debEvent);
		break;

	    case EXCEPTION_DATATYPE_MISALIGNMENT:
		/*
		 * First chance: Pass this on to the kernel.
		 * Last chance: Display an appropriate error.
		 */
		dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
		break;

	    case DBG_CONTROL_C:
		/*
		 * First chance: Pass this on to the kernel.
		 * Last chance: Display an appropriate error.
		 */
		dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
		break;

	    default:
		dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;

	    }
	    break;

	case CREATE_THREAD_DEBUG_EVENT:
	    OnCreateThread(&debEvent);
	    break;

	case CREATE_PROCESS_DEBUG_EVENT:
	    OnCreateProcess(&debEvent);
	    break;

	case EXIT_THREAD_DEBUG_EVENT:
	    /* Display the thread's exit code. */
	    break;

	case EXIT_PROCESS_DEBUG_EVENT:
	    /* Display the process's exit code. */
	    exit(0);
	    break;

	case LOAD_DLL_DEBUG_EVENT:
	    OnLoadDll(&debEvent);
	    break;

	case UNLOAD_DLL_DEBUG_EVENT:
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
    exit(0);
}

/*
 *----------------------------------------------------------------------
 *
 * OnCreateProcess --
 *
 *	This routine is called when a CREATE_PROCESS_DEBUG_EVENT
 *	occurs.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */
void
OnCreateProcess(LPDEBUG_EVENT pDebEvent)
{
    /*
     * As needed, examine or change the registers of the
     * process's initial thread with the GetThreadContext and
     * SetThreadContext functions; read from and write to the
     * process's virtual memory with the ReadProcessMemory and
     * WriteProcessMemory functions; and suspend and resume
     * thread execution with the SuspendThread and ResumeThread
     * functions.
     */
    ExpThreadInfo *threadInfo;
    LPCREATE_PROCESS_DEBUG_INFO info = &pDebEvent->u.CreateProcessInfo;
    PVOID ptr;

    threadInfo = (ExpThreadInfo *) malloc(sizeof(ExpThreadInfo));
    threadInfo->dwThreadId = pDebEvent->dwThreadId;
    threadInfo->hThread = info->hThread;
    threadInfo->nextPtr = ThreadList;
    ThreadList = threadInfo;

    if (info->lpImageName) {

	/*
	 * This info->lpImageName is a pointer to the name of the
	 * DLL in the process space of the subprocess
	 */
	if (ReadSubprocessMemory(info->lpImageName, &ptr, sizeof(PVOID)) &&
	    ptr)
	{
	    if (info->fUnicode) {
		WCHAR name[256];
		ReadSubprocessStringW(ptr, name, 256);
		printf("0x%08x: Loaded %S\n", info->lpBaseOfImage, name);
	    } else {
		CHAR name[256];
		ReadSubprocessStringA(ptr, name, 256);
		printf("0x%08x: Loaded %S\n", info->lpBaseOfImage, name);
	    }
	} else {
	    printf("Loaded unknown Executable");
	}
	if (info->dwDebugInfoFileOffset) {
	    printf(" with debugging info at offset 0x%08x\n",
		   info->dwDebugInfoFileOffset);
	}
    } else {
	printf("Starting unknown process\n");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * OnCreateThread --
 *
 *	This routine is called when a CREATE_THREAD_DEBUG_EVENT
 *	occurs.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */
void
OnCreateThread(LPDEBUG_EVENT pDebEvent)
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
    threadInfo->nextPtr = ThreadList;
    ThreadList = threadInfo;
}

/*
 *----------------------------------------------------------------------
 *
 * OnFirstBreakpoint --
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
static void
OnFirstBreakpoint(LPDEBUG_EVENT pDebEvent)
{
    DWORD i;
    UCHAR buf[4];
    DWORD base;

    printf("Hit first breakpoint\n");
    
    /*
     * Set up the memory that will serve as the place for our
     * intercepted function return
     */
    PSubprocessMemory = (DWORD) AllocProcessMemory(HProcess, 4096);
    base = PSubprocessMemory;
    buf[0] = 0xcc;		/* INT 3 */
    buf[1] = 0xc3;		/* RET */
    buf[2] = 0x00;		/* Unused */
    buf[3] = 0x00;		/* Unused */
    for (i = 0; i < 4096; i += sizeof(DWORD)) {
	WriteSubprocessMemory((PVOID) (base + i), buf, sizeof(DWORD));
    }
    return;
}

static ExpDebBreakpoint *LastBreakpoint = NULL;
/*
 *----------------------------------------------------------------------
 *
 * OnBreakpoint --
 *
 *	This routine is called when a EXCEPTION_DEBUG_EVENT with
 *	an exception code of EXCEPTION_BREAKPOINT.
 *
 * Results:
 *	None
 *
 * Notes:
 *	XXX: Maybe should only deal with this on a first chance
 *	exception.
 *
 *----------------------------------------------------------------------
 */
static void
OnBreakpoint(LPDEBUG_EVENT pDebEvent)
{
    LPEXCEPTION_DEBUG_INFO exceptInfo;
    CONTEXT context;
    ExpThreadInfo *tinfo;
    ExpDebBreakpoint *pbinfo, *binfo;
    PDWORD pdw;
    DWORD i;
    DWORD dw;

    for (tinfo = ThreadList; tinfo != NULL; tinfo = tinfo->nextPtr) {
	if (pDebEvent->dwThreadId == tinfo->dwThreadId) {
	    break;
	}
    }
    assert(tinfo != NULL);

    exceptInfo = &pDebEvent->u.Exception;
    pbinfo = NULL;
    for (binfo = BreakpointList; binfo != NULL;
	 pbinfo = binfo, binfo = binfo->nextPtr) {
	if (binfo->codePtr == exceptInfo->ExceptionRecord.ExceptionAddress) {
	    if (binfo->threadInfo == NULL) {
		break;
	    }
	    if (binfo->threadInfo == tinfo) {
		break;
	    }
	}
    }
    assert(binfo != NULL);

    context.ContextFlags = CONTEXT_FULL;
    GetThreadContext(tinfo->hThread, &context);
    if (! binfo->returning) {
	ExpDebBreakpoint *bpt;
	/*
	 * Get the arguments to the function and store them in the thread
	 * specific data structure.
	 */
	for (pdw = tinfo->args, i=0; i < binfo->breakInfo->nargs; i++, pdw++) {
	    ReadSubprocessMemory((PVOID) (context.Esp+(4*(i+1))),
				 pdw, sizeof(DWORD));
	}
	tinfo->nargs = binfo->breakInfo->nargs;

#if 0
	/*
	 * Add an additional return address on the stack.  This is to our
	 * return area.
	 */
	context.Esp -= 4;
#endif
	bpt = (ExpDebBreakpoint *) malloc(sizeof(ExpDebBreakpoint));
	ReadSubprocessMemory((PVOID) context.Esp, &bpt->origRetAddr,
			     sizeof(DWORD));
	dw = PSubprocessMemory + binfo->retCodeOffset;
	WriteSubprocessMemory((PVOID) context.Esp, &dw, sizeof(DWORD));

	bpt->returning = TRUE;
	bpt->codePtr = (PVOID) (PSubprocessMemory + binfo->retCodeOffset);
	bpt->retCodeOffset = 0;	/* Doesn't matter */
	bpt->breakInfo = binfo->breakInfo;
	bpt->threadInfo = tinfo;
	bpt->nextPtr = BreakpointList;
	BreakpointList = bpt;

	/*
	 * Now, we need to restore the original code for this breakpoint.
	 * Put the program counter back, then do a single-step and put
	 * the breakpoint back again.
	 */
	WriteSubprocessMemory(binfo->codePtr, &binfo->code, sizeof(UCHAR));

	context.EFlags |= SINGLE_STEP_BIT;
	context.Eip--;

	LastBreakpoint = binfo;
    } else {
	/*
	 * Make the callback with the params and the return value
	 */
	binfo->breakInfo->breakProc(tinfo, context.Eax);
	/*
	 * Set the instruction pointer to the original return address
	 * and continue.
	 */
	context.Eip = binfo->origRetAddr;

	if (pbinfo == NULL) {
	    BreakpointList = binfo->nextPtr;
	} else {
	    pbinfo->nextPtr = binfo->nextPtr;
	}
	free(binfo);
    }
    SetThreadContext(tinfo->hThread, &context);
}

/*
 *----------------------------------------------------------------------
 *
 * OnSingleStep --
 *
 *	This routine is called when a EXCEPTION_DEBUG_EVENT with
 *	an exception code of EXCEPTION_SINGLE_STEP.
 *
 * Results:
 *	None
 *
 * Notes:
 *	XXX: Maybe should only deal with this on a first chance
 *	exception.
 *
 *----------------------------------------------------------------------
 */
static void
OnSingleStep(LPDEBUG_EVENT pDebEvent)
{
    UCHAR code;
    /*
     * Now, we need to restore the breakpoint that we had removed.
     */
    code = 0xcc;
    WriteSubprocessMemory(LastBreakpoint->codePtr, &code, sizeof(UCHAR));
}

/*
 *----------------------------------------------------------------------
 *
 * OnLoadDll --
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
OnLoadDll(LPDEBUG_EVENT pDebEvent)
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
    ExpDebBreakInfo *debBreakInfo;
    int unknown = 0;

    if (info->lpImageName) {

	/*
	 * This info->lpImageName is a pointer to the name of the
	 * DLL in the process space of the subprocess
	 */
	if (ReadSubprocessMemory(info->lpImageName, &ptr, sizeof(PVOID)) &&
	    ptr)
	{
	    if (info->fUnicode) {
		WCHAR name[256];
		ReadSubprocessStringW(ptr, name, 256);
		printf("0x%08x: Loaded %S\n", info->lpBaseOfDll, name);
	    } else {
		CHAR name[256];
		ReadSubprocessStringA(ptr, name, 256);
		printf("0x%08x: Loaded %S\n", info->lpBaseOfDll, name);
	    }
	} else {
	    /* printf("Loaded unknown DLL"); */
	    unknown = 1;
	}
	if (info->dwDebugInfoFileOffset) {
	    printf(" with debugging info at offset 0x%08x\n",
		   info->dwDebugInfoFileOffset);
	}
    }

    base = (DWORD) info->lpBaseOfDll;

    /*
     * Check for the DOS signature
     */
    ReadSubprocessMemory(info->lpBaseOfDll, &w, sizeof(WORD));
    if (w != IMAGE_DOS_SIGNATURE) return;
    
    /*
     * Skip over the DOS signature and check the NT signature
     */
    p = base;
    p += 15 * sizeof(DWORD);
    ptr = (PVOID) p;
    ReadSubprocessMemory((PVOID) p, &ImageHdrOffset, sizeof(DWORD));

    p = base;
    p += ImageHdrOffset;
    ReadSubprocessMemory((PVOID) p, &dw, sizeof(DWORD));
    if (dw != IMAGE_NT_SIGNATURE) {
	return;
    }
    ImageHdrOffset += sizeof(DWORD);
    p += sizeof(DWORD);

    pfh = (PIMAGE_FILE_HEADER) p;
    ptr = &pfh->SizeOfOptionalHeader;
    ReadSubprocessMemory(ptr, &w, sizeof(WORD));

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
    ReadSubprocessMemory(ptr, &dw, sizeof(DWORD));
    if (dw == 0) return;

    /*
     * Read the export data directory
     */
    ptr = &poh->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    ReadSubprocessMemory(ptr, &dataDir, sizeof(IMAGE_DATA_DIRECTORY));

    /*
     * This points us to the exports section
     */
    ptr = (PVOID) (base + dataDir.VirtualAddress);
    ped = (PIMAGE_EXPORT_DIRECTORY) ptr;
    ReadSubprocessMemory(ptr, &exportDir, sizeof(IMAGE_EXPORT_DIRECTORY));

    /*
     * See if this is a DLL we are interested in
     */
    ptr = &ped->Name;
    ReadSubprocessMemory(ptr, &dw, sizeof(DWORD));
    ptr = (PVOID) (base + dw);
    ReadSubprocessStringA(ptr, dllname, sizeof(dllname));

    /*
     * We now have the DLL name, even if it was unknown.
     */
    if (unknown) {
	printf("0x%08x: Loaded %s\n", info->lpBaseOfDll, dllname);
    }

    if (stricmp(dllname, "kernel32.dll") != 0) {
	return;
    }

    ptr = (PVOID) (base + (DWORD) exportDir.AddressOfNames);
    for (n = 0; n < exportDir.NumberOfNames; n++) {
	/*
	 * Look for SetConsoleMode() so we can set a breakpoint on
	 * it.
	 */
	ReadSubprocessMemory(ptr, &dw, sizeof(DWORD));
	namePtr = (PVOID) (base + dw);
	/*
	 * Now, we should hopefully have a pointer to the name of the
	 * function, so lets get it.
	 */
	ReadSubprocessStringA(namePtr, funcName, sizeof(funcName));
	/* printf("%s\n", funcName); */

	debBreakInfo = CheckAddBreakpoint(funcName);
	if (debBreakInfo) {
	    /*
	     * Set a breakpoint and save the original code.
	     */
	    ExpDebBreakpoint *bpt;
	    UCHAR code;
	    static DWORD offset = 0;

	    funcPtr = (PVOID) (base + n*sizeof(DWORD) +
			       (DWORD) exportDir.AddressOfFunctions);
	    ReadSubprocessMemory(funcPtr, &dw, sizeof(DWORD));
	    funcPtr = (PVOID) (base + dw);
	    /*
	     * Now, we have a pointer to the function in subprocess.
	     */
	    bpt = (ExpDebBreakpoint *) malloc(sizeof(ExpDebBreakpoint));
	    bpt->returning = FALSE;
	    bpt->codePtr = funcPtr;
	    bpt->retCodeOffset = offset;
	    bpt->breakInfo = debBreakInfo;
	    bpt->threadInfo = NULL;
	    offset += sizeof(DWORD);
	    bpt->nextPtr = BreakpointList;
	    BreakpointList = bpt;

	    ReadSubprocessMemory(funcPtr, &bpt->code, sizeof(UCHAR));
	    code = 0xcc;	/* Breakpoint opcode on i386 */
	    WriteSubprocessMemory(funcPtr, &code, sizeof(UCHAR));
	}
	    
	ptr = (PVOID) (sizeof(DWORD) + (ULONG) ptr);
    }

    /*
     * The IMAGE_SECTION_HEADER comes after the IMAGE_OPTIONAL_HEADER
     * (if the IMAGE_OPTIONAL_HEADER exists)
     */
    p += w;

    psh = (PIMAGE_SECTION_HEADER) p;
}

#if 0
typedef void (ExpDebBreakProc) (HANDLE hThread, DWORD nargs, PDWORD args,
	DWORD ret, PCONTEXT);

typedef struct _ExpDebBreakFunc {
    PUCHAR funcName;		/* Name of function to intercept */
    DWORD nargs;		/* Number of arguments, 0 if we only care
				 * about the return value */
    ExpDebBreakProc *breakProc;	/* Function to call when a breakpoint is hit */
} ExpDebBreakFunc;
#endif

ExpDebBreakInfo BreakArray[] = {
    {"GetStdHandle", 1, OnGetStdHandle},
    {"OpenConsoleW", 4, OnOpenConsoleW},
    {"SetConsoleMode", 2, OnSetConsoleMode},
    {"WriteConsoleA", 5, OnWriteConsoleA},
    {"WriteConsoleW", 5, OnWriteConsoleW},
    {NULL, 0, NULL}
};

/*
 *----------------------------------------------------------------------
 *
 * CheckAddBreakpoint --
 *
 *	Read through an ?alphabetized? list of names and
 *	add a breakpoint if the function name matches one
 *	of them.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static ExpDebBreakInfo *
CheckAddBreakpoint(PCHAR funcName)
{
    ExpDebBreakInfo *info;

    info = BreakArray;
    while (info->funcName) {
	if (strcmp(funcName, info->funcName) == 0) {
	    break;
	}
	info++;
    }
    if (info->funcName) return info;
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * OnOpenConsoleW --
 *
 *	This function gets called when an OpenConsoleW breakpoint
 *	is hit.  There is one big problem with this function--we
 *	don't really know what the arguments are since there is
 *	no documented use of it.  It takes four arguments, so the
 *	current guess is:
 *
 *	HANDLE OpenConsoleW(DWORD DWORD dwDesiredAccess,
 *	    DWORD dwShareMode,
 *	    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
 *	    DWORD dwFlagsAndAttributes);
 *
 *	Need to write a small program that calls CreateFile("CONIN$", ...);
 *	to test this theory.
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
OnOpenConsoleW(ExpThreadInfo *threadInfo, DWORD returnValue)
{
    WCHAR name[256];
    PVOID ptr;
    ptr = (PVOID) threadInfo->args[0];

    ReadSubprocessStringW(ptr, name, 256);
    if (wcsicmp(name, L"CONIN$") == 0) {
	/*
	 * Add to our list of known console handles
	 */
    } else if (wcsicmp(name, L"CONOUT$") == 0) {
	/*
	 * Add to our list of known console handles
	 */
    }

    printf("OpenConsoleW(0x%08x, 0x%08x, 0x%08x, 0x%08x) ==> 0x%x\n",
	   threadInfo->args[0], threadInfo->args[1],
	   threadInfo->args[2], threadInfo->args[3], returnValue);
#if 0
    printf("OpenConsoleW(name=%S)\n", name);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * OnWriteConsoleA --
 *
 *	This function gets called when an WriteConsoleA breakpoint
 *	is hit.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Prints some output.
 *
 * Notes:
 *	XXX: If this is using a handle to a known console handle,
 *	then redirect the output to expect.
 *
 *----------------------------------------------------------------------
 */

static void
OnWriteConsoleA(ExpThreadInfo *threadInfo, DWORD returnValue)
{
    CHAR name[256];
    PVOID ptr;
    DWORD n;
    PCHAR p;

    ptr = (PVOID) threadInfo->args[1];
    n = threadInfo->args[2];

    if (n > 256) {
	if (n > 32768) n = 32768;
	p = malloc(n * sizeof(CHAR));
    } else {
	p = name;
    }
    ReadSubprocessMemory(ptr, p, n * sizeof(CHAR));
    p[n] = 0;

    printf("WriteConsoleA(0x%08x, 0x%08x, 0x%08x, 0x%08x) ==> 0x%x\n",
	   threadInfo->args[0], threadInfo->args[1],
	   threadInfo->args[2], threadInfo->args[3], returnValue);
    printf("WriteConsoleA(buf=%s)\n", p);
    if (p != name) {
	free(name);
    }
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
OnWriteConsoleW(ExpThreadInfo *threadInfo, DWORD returnValue)
{
    WCHAR name[256];
    PVOID ptr;
    DWORD n;
    PWCHAR p;

    ptr = (PVOID) threadInfo->args[1];
    n = threadInfo->args[2];

    if (n > 256) {
	if (n > 32768) n = 32768;
	p = malloc(n * sizeof(WCHAR));
    } else {
	p = name;
    }
    ReadSubprocessMemory(ptr, p, n * sizeof(WCHAR));
    p[n] = 0;

    printf("WriteConsoleW(0x%08x, 0x%08x, 0x%08x, 0x%08x) ==> 0x%x\n",
	   threadInfo->args[0], threadInfo->args[1],
	   threadInfo->args[2], threadInfo->args[3], returnValue);
    printf("WriteConsoleW(buf=%S)\n", p);

    if (p != name) {
	free(name);
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
OnSetConsoleMode(ExpThreadInfo *threadInfo, DWORD returnValue)
{
    printf("SetConsoleMode(0x%08x, 0x%08x) ==> 0x%x\n",
	   threadInfo->args[0], threadInfo->args[1], returnValue);
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
OnGetStdHandle(ExpThreadInfo *threadInfo, DWORD returnValue)
{
    printf("GetStdHandle(0x%08x) ==> 0x%x\n",
	   threadInfo->args[0], returnValue);
}

/*
 *----------------------------------------------------------------------
 *
 * ReadSubprocessStringW --
 *
 *	Read a wide character string from the subprocess
 *
 * Results:
 *	The length of the string in wide characters.
 *
 *----------------------------------------------------------------------
 */
static int
ReadSubprocessStringW(PVOID base, PWCHAR buf, int buflen)
{
    PWCHAR ip, op;
    int i;
    
    ip = base;
    op = buf;
    i = 0;
    while (i < buflen-1) {
	if (! ReadSubprocessMemory(ip, op, sizeof(WCHAR))) {
	    break;
	}
	if (*op == 0) break;
	op++; ip++; i++;
    }
    *op = 0;
    return i;
}

/*
 *----------------------------------------------------------------------
 *
 * ReadSubprocessStringA --
 *
 *	Read an ANSI character string from the subprocess
 *
 * Results:
 *	The length of the string in characters.
 *
 *----------------------------------------------------------------------
 */
static int
ReadSubprocessStringA(PVOID base, PCHAR buf, int buflen)
{
    PCHAR ip, op;
    int i;
    
    ip = base;
    op = buf;
    i = 0;
    while (i < buflen-1) {
	if (! ReadSubprocessMemory(ip, op, sizeof(CHAR))) {
	    break;
	}
	if (*op == 0) break;
	op++; ip++; i++;
    }
    *op = 0;
    return i;
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
 *----------------------------------------------------------------------
 */

static BOOL
ReadSubprocessMemory(LPVOID addr, LPVOID buf, DWORD len)
{
    DWORD oldProtection = 0;
    MEMORY_BASIC_INFORMATION mbi;
    BOOL ret = TRUE;

    /* if not committed memory abort */
    if (!VirtualQueryEx(HProcess, addr, &mbi, sizeof(mbi)) ||
	mbi.State != MEM_COMMIT)
    {
	return FALSE;
    }
    
    /* if guarded memory, change protection temporarily */
    if (!(mbi.Protect & PAGE_READONLY) && !(mbi.Protect & PAGE_READWRITE)) {
	VirtualProtectEx(HProcess, addr, len, PAGE_READONLY, &oldProtection);
    }
    
    if (!ReadProcessMemory(HProcess, addr, buf, len, NULL)) {
	ret = FALSE;
    }
    
    /* reset protection if changed */
    if (oldProtection) {
	VirtualProtectEx(HProcess, addr, len, oldProtection, &oldProtection);
    }
    return ret;
}

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

static BOOL
WriteSubprocessMemory(LPVOID addr, LPVOID buf, DWORD len)
{
    DWORD oldProtection = 0;
    MEMORY_BASIC_INFORMATION mbi;
    BOOL ret = TRUE;
    DWORD err;

    /* if not committed memory abort */
    if (!VirtualQueryEx(HProcess, addr, &mbi, sizeof(mbi)) ||
	mbi.State != MEM_COMMIT)
    {
	ret = FALSE;
	assert(ret != FALSE);
	return ret;
    }
    
    /* if guarded memory, change protection temporarily */
    if (!(mbi.Protect & PAGE_READWRITE)) {
	if (!VirtualProtectEx(HProcess, addr, len, PAGE_READWRITE,
			      &oldProtection)) {
	    err = GetLastError();
	}
    }
    
    if (!WriteProcessMemory(HProcess, addr, buf, len, NULL)) {
	ret = FALSE;
	err = GetLastError();
    }
    
    /* reset protection if changed */
    if (oldProtection) {
	VirtualProtectEx(HProcess, addr, len, oldProtection, &oldProtection);
    }
    if (ret == FALSE) {
	assert(ret != FALSE);
    }
    return ret;
}

/*
 *----------------------------------------------------------------------
 *
 * AllocProcessMemory --
 *
 *	Allocates memory in the subprocess.  Makes the memory readable,
 *	writable, and executable.  Allocation is done by calling
 *	CreateRemoteThread(), making it unsuitable for use on Windows 95.
 *	The alternative is to use some special injection code to load
 *	a DLL that then calls AllocMemory() and stores the pointer
 *	somewhere where we can get at it.
 *
 * Results:
 *	A pointer that is valid in the other process
 *
 *----------------------------------------------------------------------
 */

static PVOID
AllocProcessMemory(HANDLE hProcess, DWORD numBytes)
{
    HANDLE hThread;
    DWORD threadId;
    CONTEXT context;
    MEMORY_BASIC_INFORMATION mbi;
    PVOID pMem;
    DWORD oldProtection;

    hThread = CreateRemoteThread(HProcess, NULL, numBytes,
				 (LPTHREAD_START_ROUTINE) NULL, 0,
				 CREATE_SUSPENDED, &threadId);
    context.ContextFlags = CONTEXT_CONTROL;
    GetThreadContext(hThread, &context);

    /*
     * Address of top of stack is in stack pointer register
     */
    VirtualQueryEx(hProcess, (PDWORD) context.Esp - 1, &mbi, sizeof(mbi));
    pMem = (PVOID) mbi.BaseAddress;
    VirtualProtectEx(hProcess, pMem, numBytes, PAGE_EXECUTE_READWRITE,
		     &oldProtection);
    return pMem;
}
