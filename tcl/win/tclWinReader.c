/* 
 * tclWinReader.c --
 *
 *	This file implements the Windows-specific functionality of
 *	reading blocking files on threads so event driven I/O can
 *	be done.
 *
 * Copyright (c) 1997 by Mitel, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "tclWinInt.h"

/*
 * The following define declares a new user message for use on the
 * reader window.
 */

#define READER_MESSAGE	WM_USER+20

/*
 * This defines the buffersize we use for reading readers
 */

#define READER_BUFFER_SIZE 8192

/*
 * Declarations for functions used only in this file.
 */


/*
 *----------------------------------------------------------------------
 *
 * TclWinReaderNew --
 *
 *	Allocates a new TclWinReaderInfo structure
 *
 * Results:
 *	A TclWinReaderInfo structure
 *
 * Side Effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

TclWinReaderInfo *
TclWinReaderNew(postWin, postMsg, file)
    HANDLE postWin;
    DWORD postMsg;
    HANDLE file;
{
    TclWinReaderInfo *infoPtr;
    DCB dcb;
    DWORD dw;

    infoPtr = (TclWinReaderInfo *) ckalloc(sizeof(TclWinReaderInfo));

    infoPtr->postWin = postWin;
    infoPtr->postMsg = postMsg;
    infoPtr->rHandle = file;
    if (TclWinGetPlatformId() == VER_PLATFORM_WIN32s) {
	infoPtr->rType = FILE_TYPE_DISK;
    } else {
	dcb.DCBlength = sizeof( DCB ) ;
	if (GetCommState(file, &dcb)) {
	    infoPtr->rType = FILE_TYPE_COM;
	} else {
	    infoPtr->rType = GetFileType(file);
	    if (infoPtr->rType == FILE_TYPE_CHAR) {
		if (GetConsoleMode(file, &dw) == TRUE) {
		    infoPtr->rType = FILE_TYPE_CONSOLE;
		}
	    }
	}
    }

    /*
     * overlappedIO flag is set via the SetOption interface.  Ugly,
     * but it works.
     */
    infoPtr->overlappedIO = 0;
    infoPtr->eof = 0;
    infoPtr->async = 0;
    infoPtr->readyMask =
	(infoPtr->rType == FILE_TYPE_DISK) ? TCL_READABLE|TCL_WRITABLE : 0;

    ZeroMemory(&infoPtr->rOverResult, sizeof(infoPtr->rOverResult));
    infoPtr->rSemaphore = NULL;
    infoPtr->rData = NULL;
    infoPtr->rDataLen = 0;
    infoPtr->rDataPos = 0;
    infoPtr->rThread = NULL;
    infoPtr->rEvent = NULL;
    infoPtr->rGoEvent = NULL;
    infoPtr->rHandleClosed = FALSE;

    infoPtr->refCount = 1;

    return infoPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinReaderFree --
 *
 *	Free a reader structure.  If a thread is active, it will instead
 *	be freed by the thread.
 *
 * Results:
 *	None
 *
 * Side Effects.
 *	A semaphore may be released.
 *
 *----------------------------------------------------------------------
 */

void
TclWinReaderFree(infoPtr)
    TclWinReaderInfo *infoPtr;
{
    if (--infoPtr->refCount <= 0) {
	CloseHandle(infoPtr->rSemaphore);
	CloseHandle(infoPtr->rOverResult.hEvent);
	CloseHandle(infoPtr->rEvent);
	CloseHandle(infoPtr->rGoEvent);
	CloseHandle(infoPtr->rThread);

	if (infoPtr->rData) {
	    ckfree(infoPtr->rData);
	}
	ckfree((char *) infoPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinReaderStart --
 *
 *	Create a reader thread.
 *
 * Results:
 *	1 on success, 0 on failure
 *
 * Side Effects:
 *	A thread is created and synchronization objects are created
 *
 *----------------------------------------------------------------------
 */
int
TclWinReaderStart(infoPtr, threadProc)
    TclWinReaderInfo *infoPtr;
    LPTHREAD_START_ROUTINE threadProc;
{
    DWORD threadId;

    if (infoPtr->overlappedIO) {
	infoPtr->rOverResult.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (infoPtr->rOverResult.hEvent == NULL) {
	    goto error;
	}
    }
    infoPtr->rSemaphore = CreateSemaphore(NULL, 0, 1, NULL);
    if (infoPtr->rSemaphore == NULL) {
	goto error;
    }
    infoPtr->rEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (infoPtr->rEvent == NULL) {
	goto error;
    }
    infoPtr->rGoEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (infoPtr->rGoEvent == NULL) {
	goto error;
    }
    infoPtr->rData = ckalloc(READER_BUFFER_SIZE);

    infoPtr->refCount++;
    infoPtr->rThread = CreateThread(NULL, 0, threadProc, infoPtr, 0,
				     &threadId);
    if (infoPtr->rThread == NULL) {
	--infoPtr->refCount;
	goto error;
    }

    return 1;

  error:
    if (infoPtr->rSemaphore) {
	CloseHandle(infoPtr->rSemaphore);
	infoPtr->rSemaphore = NULL;
    }
    if (infoPtr->rOverResult.hEvent) {
	CloseHandle(infoPtr->rOverResult.hEvent);
	infoPtr->rOverResult.hEvent = NULL;
    }
    if (infoPtr->rEvent) {
	CloseHandle(infoPtr->rEvent);
	infoPtr->rEvent = NULL;
    }
    if (infoPtr->rGoEvent) {
	CloseHandle(infoPtr->rGoEvent);
	infoPtr->rGoEvent = NULL;
    }
    if (infoPtr->rData) {
	ckfree(infoPtr->rData);
	infoPtr->rData = NULL;
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinReaderThread --
 *
 *	This routine reads from a file and places data into a buffer
 *	for the main thread to read from.
 *
 * Results:
 *	This thread exits when the child is done.
 *
 * Side Effects:
 *	This buffer should not be changed until the data has been copied.
 *
 *----------------------------------------------------------------------
 */
DWORD WINAPI
TclWinReaderThread(arg)
    LPVOID *arg;
{
    TclWinReaderInfo *infoPtr = (TclWinReaderInfo *) arg;
    DWORD nread;
    int n, cnt, nloopcnt;
    DWORD nmax;
    HANDLE handle;
    LPOVERLAPPED overResult;
    BOOL b;
    DWORD err;

    handle = infoPtr->rHandle;
    overResult = infoPtr->overlappedIO ? &infoPtr->rOverResult: NULL;

    n = 0;
    while (1) {
	nloopcnt = 0;
	if (infoPtr->rHandleClosed) break;
	if (infoPtr->rType == FILE_TYPE_PIPE) {
	    nread = 0;
	    PeekNamedPipe(handle, NULL, 0, NULL, &nread, NULL);
	    if (nread == 0) {
		nread = 1;
	    }
	} else if (infoPtr->rType == FILE_TYPE_CONSOLE) {
	    nread = READER_BUFFER_SIZE;
	} else {
	    nread = 1;
	}
	do {
	    nmax = READER_BUFFER_SIZE - n;
	    nread = min(nread, nmax);
	    if (overResult) {
		ResetEvent(overResult->hEvent);
	    }
	    if (infoPtr->rHandleClosed) goto done;
	    b = ReadFile(handle, &infoPtr->rData[n], nread,
			 &cnt, overResult);
	    if (b == FALSE) {
		err = GetLastError();
		if (err == ERROR_IO_PENDING) {
		    if (infoPtr->rHandleClosed) goto done;
		    b = GetOverlappedResult(handle, overResult,
					    &cnt, TRUE);
		    if (b == FALSE) {
			err = GetLastError();
			if (n == 0) {
			    goto done;
			}
		    }
		} else {
		    if (nloopcnt == 0) {
			goto done;
		    }
		}
	    }
	    if (cnt > 0) {
		n += cnt;
		nloopcnt += cnt;
		if ((n >= READER_BUFFER_SIZE) ||
		    (infoPtr->rType == FILE_TYPE_CONSOLE))
		{
		    break;
		}
	    } else {
		break;
	    }
	    if (infoPtr->rHandleClosed) goto done;
	    if (infoPtr->rType == FILE_TYPE_PIPE) {
		PeekNamedPipe(handle, NULL, 0, NULL, &nread, NULL);
		/*
		 * Allow subprocess to do something if we get all the
		 * processing time
		 */
		if (cnt == 1 && nread == 0) {
		    Sleep(40);
		    PeekNamedPipe(handle, NULL, 0, NULL, &nread, NULL);
		}
	    }
	} while (nread > 0);

	infoPtr->rData[n] = 0;
	infoPtr->rDataLen = n;
	if (n > 0) {
	    if (infoPtr->rType == FILE_TYPE_CONSOLE) {
		ResetEvent(infoPtr->rGoEvent);
	    }
	    SetEvent(infoPtr->rEvent);
	    PostMessage(infoPtr->postWin, infoPtr->postMsg, (UINT) handle, TCL_READABLE);
	    if (n == READER_BUFFER_SIZE || infoPtr->rType == FILE_TYPE_CONSOLE) {
		WaitForSingleObject(infoPtr->rSemaphore, INFINITE);
		n = 0;
		if (infoPtr->rType == FILE_TYPE_CONSOLE) {
		    /*
		     * For reading from the console, do not actually start
		     * reading until input has been requested.  The only
		     * reason this is necessary is that if a an extension
		     * (i.e. Expect) needs to set the console mode, it only
		     * take affect if ReadFile has not yet been called for
		     * console input or it will wait until the next line.
		     */
		    WaitForSingleObject(infoPtr->rGoEvent, INFINITE);
		}
	    }
	    nloopcnt = 0;
	} else if (cnt <= 0 && n <= 0) {
	    infoPtr->rDataLen = 0;
	    infoPtr->rData[0] = 0;
	    break;
	}
    }

 done:
    infoPtr->eof = 1;
    PostMessage(infoPtr->postWin, READER_MESSAGE, (UINT) handle, TCL_READABLE);
    SetEvent(infoPtr->rEvent);

    WaitForSingleObject(infoPtr->rSemaphore, INFINITE);

    TclWinReaderFree(infoPtr);
    ExitThread(0);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinReaderInput --
 *
 *	Reads input from the IO channel into the buffer given. Returns
 *	count of how many bytes were actually read, and an error indication.
 *
 * Results:
 *	A count of how many bytes were read is returned and an error
 *	indication is returned in an output argument.
 *
 * Side effects:
 *	Reads input from the actual channel.
 *
 *----------------------------------------------------------------------
 */

int
TclWinReaderInput(infoPtr, buf, bufSize, errorCode)
    TclWinReaderInfo *infoPtr;		/* Reader state. */
    char *buf;				/* Where to store data read. */
    int bufSize;			/* How much space is available
                                         * in the buffer? */
    int *errorCode;			/* Where to store error code. */
{
    int n, pos, len;

    /*
     * Readers will block until the requested number of bytes has been
     * read.  To avoid blocking unnecessarily, we look ahead and only
     * read as much as is available.
     */

    if (infoPtr->eof) {
	if (infoPtr->rDataLen - infoPtr->rDataPos == 0) {
	    ReleaseSemaphore(infoPtr->rSemaphore, 1, NULL);
	    return 0;
	}
    }
    if (infoPtr->rThread == NULL) {
	TclWinReaderStart(infoPtr, TclWinReaderThread);
    }
    if (infoPtr->rDataLen - infoPtr->rDataPos == 0) {
	if (infoPtr->rType == FILE_TYPE_CONSOLE) {
	    /*
	     * For reading from the console, do not actually start
	     * reading until input has been requested.  The only reason
	     * this is necessary is that if a an extension (i.e. Expect)
	     * needs to set the console mode, it only take affect if
	     * ReadFile has not yet been called for console input
	     */
	    SetEvent(infoPtr->rGoEvent);
	}
	if (infoPtr->async) {
	    infoPtr->readyMask &= ~TCL_READABLE;
	    errno = *errorCode = EAGAIN;
	    return -1;
	}
	while (1) {
	    if (WaitForSingleObject(infoPtr->rEvent, INFINITE) != WAIT_OBJECT_0) {
		goto error;
	    }
	    if (infoPtr->rDataLen - infoPtr->rDataPos > 0) {
		break;
	    }
	    if (infoPtr->eof == 1) {
		break;
	    }
	    ResetEvent(infoPtr->rEvent);
	}
    }
    if (infoPtr->rDataLen - infoPtr->rDataPos > 0) {
	len = infoPtr->rDataLen;
	pos = infoPtr->rDataPos;
	n = min(bufSize, len - pos);
	memcpy(buf, infoPtr->rData + pos, n);
	infoPtr->rDataPos += n;
	if (infoPtr->rDataPos == infoPtr->rDataLen) {
	    infoPtr->readyMask &= ~TCL_READABLE;
	}
	/*
	 * Console input gets read a line at a time and then waits.
	 * Pipe input gets read until the entire buffer has been
	 * filled and then it waits.
	 */
	if (infoPtr->rDataPos == READER_BUFFER_SIZE ||
	    infoPtr->rType == FILE_TYPE_CONSOLE)
	{
	    infoPtr->rDataPos = 0;
	    infoPtr->rDataLen = 0;
	    ResetEvent(infoPtr->rEvent);
	    ReleaseSemaphore(infoPtr->rSemaphore, 1, NULL);
	}
	return n;
    }
    ReleaseSemaphore(infoPtr->rSemaphore, 1, NULL);
    return 0;
    
    error:
    TclWinConvertError(GetLastError());
    if (errno == EPIPE) {
	ReleaseSemaphore(infoPtr->rSemaphore, 1, NULL);
	return 0;
    }
    *errorCode = errno;
    return -1;
}
