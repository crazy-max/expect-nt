/* 
 * expWinLog.c --
 *
 *	This file logs to the NT system log.  Use the Event Viewer to
 *	see these logs.  This was predominately used to debug the
 *	slavedrv.exe process.
 *
 * Copyright (c) 1997 Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "tcl.h"
#include "tclPort.h"
#include "expWin.h"

static HANDLE hSyslog = NULL;

/*
 *----------------------------------------------------------------------
 *
 * ExpSyslog --
 *
 *	Logs error messages to the system application event log.
 *	It is normally called through the macro EXP_LOG() when
 *	errors occur in the slave driver process, but it can be
 *	used elsewhere.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
ExpSyslog TCL_VARARGS_DEF(char *,arg1)
{
    char *fmt;
    va_list args;
    char buf[4096];
    char *strings[1];

    fmt = TCL_VARARGS_START(char *,arg1,args);
    
    if (hSyslog == NULL) {
	hSyslog = OpenEventLog(NULL, "ExpectSlaveDrv");
    }
    vsprintf(buf, fmt, args);
    va_end(args);

    strings[0] = buf;
    ReportEvent(hSyslog, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 1, 0,
		strings, NULL);
}
