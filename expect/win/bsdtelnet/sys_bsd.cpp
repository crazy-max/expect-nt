/*
 * Copyright (c) 1988, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * From: @(#)sys_bsd.c	5.2 (Berkeley) 3/1/91
 */
char bsd_rcsid[] = 
  "$Id: sys_bsd.cpp,v 1.1.1.1 1997/08/13 05:39:36 chaffee Exp $";

/*
 * The following routines try to encapsulate what is system dependent
 * (at least between 4.x and dos) which is used in telnet.c.
 */

#ifdef __WIN32__
#include <windows.h>
#include <winsock.h>
#endif
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/telnet.h>
#ifndef __WIN32__
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#endif

#include "ring.h"

#include "defines.h"
#include "externs.h"
#include "types.h"
#include "proto.h"
#include "netlink.h"
#include "terminal.h"

#define	SIG_FUNC_RET	void


static fd_set ibits, obits, xbits;

void init_sys(void)
{
    tlink_init();
    FD_ZERO(&ibits);
    FD_ZERO(&obits);
    FD_ZERO(&xbits);

    errno = 0;
}


#ifdef	KLUDGELINEMODE
extern int kludgelinemode;
#endif
/*
 * TerminalSpecialChars()
 *
 * Look at an input character to see if it is a special character
 * and decide what to do.
 *
 * Output:
 *
 *	0	Don't add this character.
 *	1	Do add this character
 */

void intp(), sendbrk(), sendabort();

int
TerminalSpecialChars(int c)
{
    void xmitAO(), xmitEL(), xmitEC();

    if (c == termIntChar) {
	intp();
	return 0;
    } else if (c == termQuitChar) {
#ifdef	KLUDGELINEMODE
	if (kludgelinemode)
	    sendbrk();
	else
#endif
	    sendabort();
	return 0;
    } else if (c == termEofChar) {
	if (my_want_state_is_will(TELOPT_LINEMODE)) {
	    sendeof();
	    return 0;
	}
	return 1;
    } else if (c == termSuspChar) {
	sendsusp();
	return(0);
    } else if (c == termFlushChar) {
	xmitAO();		/* Transmit Abort Output */
	return 0;
    } else if (!MODE_LOCAL_CHARS(globalmode)) {
	if (c == termKillChar) {
	    xmitEL();
	    return 0;
	} else if (c == termEraseChar) {
	    xmitEC();		/* Transmit Erase Character */
	    return 0;
	}
    }
    return 1;
}



cc_t *tcval(int func) {
    switch(func) {
    case SLC_IP:	return(&termIntChar);
    case SLC_ABORT:	return(&termQuitChar);
    case SLC_EOF:	return(&termEofChar);
    case SLC_EC:	return(&termEraseChar);
    case SLC_EL:	return(&termKillChar);
    case SLC_XON:	return(&termStartChar);
    case SLC_XOFF:	return(&termStopChar);
    case SLC_FORW1:	return(&termForw1Char);
    case SLC_FORW2:	return(&termForw2Char);
#ifdef	VDISCARD
    case SLC_AO:	return(&termFlushChar);
#endif
#ifdef	VSUSP
    case SLC_SUSP:	return(&termSuspChar);
#endif
#ifdef	VWERASE
    case SLC_EW:	return(&termWerasChar);
#endif
#ifdef	VREPRINT
    case SLC_RP:	return(&termRprntChar);
#endif
#ifdef	VLNEXT
    case SLC_LNEXT:	return(&termLiteralNextChar);
#endif
#ifdef	VSTATUS
    case SLC_AYT:	return(&termAytChar);
#endif

    case SLC_SYNCH:
    case SLC_BRK:
    case SLC_EOR:
    default:
	return NULL;
    }
}

SIG_FUNC_RET ayt_status(int);

#ifdef	SIGINFO
	static SIG_FUNC_RET ayt(int);
#endif	SIGINFO

#if defined(TN3270)
void NetSigIO(int fd, int onoff) {
    ioctl(fd, FIOASYNC, (char *)&onoff);	/* hear about input */
}

void NetSetPgrp(int fd) {
    int myPid;

    myPid = getpid();
    fcntl(fd, F_SETOWN, myPid);
}
#endif	/*defined(TN3270)*/

/*
 * Various signal handling routines.
 */

static SIG_FUNC_RET deadpeer(int /*sig*/) {
    setcommandmode();
#ifdef XXX
    siglongjmp(peerdied, -1);
#endif
}

static SIG_FUNC_RET intr(int /*sig*/) {
    if (localchars) {
	intp();
    }
    else {
        setcommandmode();
#ifdef XXX
	siglongjmp(toplevel, -1);
#endif
    }
}

static SIG_FUNC_RET intr2(int /*sig*/) {
    if (localchars) {
#ifdef	KLUDGELINEMODE
	if (kludgelinemode)
	    sendbrk();
	else
#endif
	    sendabort();
	return;
    }
}

#ifdef	SIGWINCH
static SIG_FUNC_RET sendwin(int /*sig*/) {
    if (connected) {
	sendnaws();
    }
}
#endif

#ifdef	SIGINFO
static SIG_FUNC_RET ayt(int sig) {
    if (connected)
	sendayt();
    else
	ayt_status();
}
#endif

void sys_telnet_init(void) {
    signal(SIGINT, intr);
#ifdef SIGQUIT
    signal(SIGQUIT, intr2);
#endif
#ifdef  SIGPIPE
    signal(SIGPIPE, deadpeer);
#endif
#ifdef	SIGWINCH
    signal(SIGWINCH, sendwin);
#endif
#ifdef	SIGINFO
    signal(SIGINFO, ayt);
#endif

    setconnmode(0);

    nlink.nonblock(1);

#if defined(TN3270)
    if (noasynchnet == 0) {			/* DBX can't handle! */
	NetSigIO(net, 1);
	NetSetPgrp(net);
    }
#endif	/* defined(TN3270) */

    nlink.oobinline();
}


static int SelSet;
static HANDLE hSelEvent;
static HANDLE hSelSemaphore;
static int KbdSet;
static HANDLE hKbdEvent;
static HANDLE hKbdSemaphore;
static int Poll;
static struct timeval TimeValue = { 0 };
static int SelRet;
static DWORD SelErrno;
static char KbdChar;

static DWORD
SelectThread(LPDWORD arg)
{
    while (1) {
	if (WaitForSingleObject(hSelSemaphore, INFINITE) != WAIT_OBJECT_0) {
	    break;
	}
	
	SelRet = select(16, &ibits, &obits, &xbits,
			(Poll == 0)? (struct timeval *)0 : &TimeValue);
	if (SelRet == -1) {
	    SelErrno = WSAGetLastError();
	}
	SetEvent(hSelEvent);
    }

    ExitThread(0);
    return 0;
}

static DWORD
KbdThread(LPDWORD arg)
{
    HANDLE hIn;
    BOOL console;
    int done;
    INPUT_RECORD inputRecord;
    DWORD dwRead;

    hIn = GetStdHandle(STD_INPUT_HANDLE);
#if 0
    printf("KbdThread calling SetConsoleMode(0x%08x, 0x%08x)\n", hIn, 0);
#endif
    console = SetConsoleMode(hIn, 0);
    if (console) {
	done = 0;
	WaitForSingleObject(hKbdSemaphore, INFINITE);
	while (1) {
	    switch (WaitForSingleObject(hIn, INFINITE)) {
	    case WAIT_OBJECT_0:
		if (ReadConsoleInput(hIn, &inputRecord, 1, &dwRead) == FALSE) {
		    done = 1;
		    break;
		}
		switch (inputRecord.EventType) {
		case KEY_EVENT:
		    /*
		     * Check for an ASCII value.  A CTRL-@ will look
		     * like a zero, but it is a real keypress.  Check for
		     * it as well.
		     */
		    if (inputRecord.Event.KeyEvent.uChar.AsciiChar ||
			inputRecord.Event.KeyEvent.wVirtualKeyCode == '2') {
			if (inputRecord.Event.KeyEvent.bKeyDown) {
			    KbdChar = inputRecord.Event.KeyEvent.uChar.AsciiChar;
			    SetEvent(hKbdEvent);
			    WaitForSingleObject(hKbdSemaphore, INFINITE);
			}
		    }
		    break;
		}
		break;

	    default:
		done = 1;
		break;
	    }
	}
    } else {
	assert("input is not from console");
	/* XXX: This should handle a closed pipe properly */
#if 0
	while (! done) {
	    b = ReadFile(hIn, buf, sizeof(buf), &dwRead, NULL);
	    if (b == FALSE) {
		break;
	    }
	    cnt = 0;
	    while (cnt < dwRead) {
		n = send(sock, &buf[cnt], dwRead-cnt, 0);
		if (n < 0) {
		    done = 1;
		    break;
		}
	    }
	}
#endif
    }

    ExitThread(0);
    return 0;
}
/*
 * Process rings -
 *
 *	This routine tries to fill up/empty our various rings.
 *
 *	The parameter specifies whether this is a poll operation,
 *	or a block-until-something-happens operation.
 *
 *	The return value is 1 if something happened, 0 if not.
 */

int process_rings(int netin, int netout, int netex, int ttyin, int ttyout, 
		  int poll /* If 0, then block until something to do */)
{
    register int c;
		/* One wants to be a bit careful about setting returnValue
		 * to one, since a one implies we did some useful work,
		 * and therefore probably won't be called to block next
		 * time (TN3270 mode only).
		 */
    int returnValue = 0;
    DWORD tid;
    HANDLE hThread;
    static HANDLE hObjs[2];
    static int initialized = 0;

    SOCKET net = nlink.getfd();

    if (! initialized) {
	hKbdSemaphore = CreateSemaphore(NULL, 0, 1, NULL);
	hSelSemaphore = CreateSemaphore(NULL, 0, 1, NULL);
	hSelEvent     = CreateEvent(NULL, 1, 0, NULL);
	hKbdEvent     = CreateEvent(NULL, 1, 0, NULL);
	SelSet        = 0;
	KbdSet        = 0;
	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) SelectThread,
			       NULL, 0, &tid);
	assert(hThread != NULL);
	CloseHandle(hThread);
	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) KbdThread,
			       NULL, 0, &tid);
	assert(hThread != NULL);
	CloseHandle(hThread);
	hObjs[0] = hSelEvent;
	hObjs[1] = hKbdEvent;
	initialized = 1;
    }
    
#ifdef __WIN32__ /* XXX */
    /*
     * This isn't the most efficient as it tries to flush whether it
     * needs to or not, but it works.  It also could block if lots
     * of data was being written to the socket.
     */
    ttyflush(SYNCHing|flushout);
    netflush();
#else
    if (ttyout) {
	FD_SET(tout, &obits);
    }
    if (ttyin) {
	FD_SET(tin, &ibits);
    }
#endif

    if (SelSet == 0) {
	if (netin) {
	    FD_SET(net, &ibits);
	}
	if (netex) {
	    FD_SET(net, &xbits);
	}
	Poll = poll;
	
	SelSet = 1;
	ResetEvent(hSelEvent);
	ReleaseSemaphore(hSelSemaphore, 1, NULL);
    }
    if (KbdSet == 0) {
	KbdSet = 1;
	ResetEvent(hKbdEvent);
	ReleaseSemaphore(hKbdSemaphore, 1, NULL);
    }

    switch (WaitForMultipleObjects(2, hObjs, FALSE, INFINITE)) {
    case WAIT_OBJECT_0+1:
	/* Keyboard input is available */
	KbdSet = 0;
	ttyiring.write(&KbdChar, 1);
	returnValue = 1;		/* did something useful */
	break;

    case WAIT_OBJECT_0:
	/* Select is done */
	SelSet = 0;
	if (SelRet == -1) {
		    /*
		     * we can get EINTR if we are in line mode,
		     * and the user does an escape (TSTP), or
		     * some other signal generator.
		     */
	    if (SelErrno == WSAEINTR) {
		return 0;
	    }
#if defined(TN3270)
		    /*
		     * we can get EBADF if we were in transparent
		     * mode, and the transcom process died.
		    */
	    if (SelErrno == WSAEBADF) {
			/*
			 * zero the bits (even though kernel does it)
			 * to make sure we are selecting on the right
			 * ones.
			*/
		FD_ZERO(&ibits);
		FD_ZERO(&obits);
		FD_ZERO(&xbits);
		return 0;
	    }
#endif /* TN3270 */
		    /* I don't like this, does it ever happen? */
	    printf("sleep(5) from telnet, after select\r\n");
	    fflush(stdout);
	    sleep(5);
	    return 0;
	}

	/*
	 * Any urgent data?
	 */
	if (FD_ISSET(net, &xbits)) {
	    FD_CLR(net, &xbits);
	    SYNCHing = 1;
	    (void) ttyflush(1);	/* flush already enqueued data */
	}

	/*
	 * Something to read from the network...
	 */
	if (FD_ISSET(net, &ibits)) {
	    /* hacks for systems without SO_OOBINLINE removed */

	    FD_CLR(net, &ibits);
	    c = netiring.read_source();
	    if (c <= 0) {
		return -1;
	    }
	    returnValue = 1;
	}

	if (FD_ISSET(net, &obits)) {
	    FD_CLR(net, &obits);
	    returnValue |= netflush();
	}
	break;
    default:
	assert("Unexpected return from WaitForMultipleObjects()");
    }

    return returnValue;
}
