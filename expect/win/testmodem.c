/*
 * testmodem.c
 *
 *	Test of Expect's handling of redirected serial handles.
 *
 *	Usage: testmodem comfile scriptname
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
    char *nativeName, *scriptName;
    HANDLE handle;
    DWORD accessMode, flags;
    COMMTIMEOUTS cto;
    DCB dcb;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    BOOL b;
    char buffer[4096];

    if (argc >= 2) {
	nativeName = argv[1];
    } else {
	nativeName = "com2:";
    }
    if (argc >= 3) {
	scriptName = argv[2];
    } else {
	scriptName = "tests\\modemtest.exp";
    }

    printf("Debug 1\n");
    flags = GetFileAttributes(nativeName);
    if (flags == 0xFFFFFFFF) {
	flags = 0;
    }
    accessMode = (GENERIC_READ | GENERIC_WRITE);
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    handle = CreateFile(nativeName, accessMode, 0, &sa, OPEN_EXISTING,
	flags | FILE_FLAG_OVERLAPPED, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
	fprintf(stderr, "Unable to open %s, exiting...\n", nativeName);
	exit(1);
    }

    printf("Debug 2\n");

    /*
     * FileInit the com port.
     */

    SetCommMask(handle, EV_RXCHAR);
    SetupComm(handle, 4096, 4096);
    PurgeComm(handle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR
	| PURGE_RXCLEAR);
    cto.ReadIntervalTimeout = MAXDWORD;
    cto.ReadTotalTimeoutMultiplier = 0;
    cto.ReadTotalTimeoutConstant = 0;
    cto.WriteTotalTimeoutMultiplier = 0;
    cto.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts(handle, &cto);

    GetCommState(handle, &dcb);
    SetCommState(handle, &dcb);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = handle;
    si.hStdInput = handle;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    printf("Debug 3\n");
    sprintf(buffer, "tclsh80.exe %s", scriptName);
    printf("Debug 4: Command Line=%s\n", buffer);
    b = CreateProcess(NULL, buffer, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    printf("Debug 5\n");
    if (b == FALSE) {
	fprintf(stderr, "Unable to create process tclsh80.exe: error=%08x\n",
	    GetLastError());
	exit(1);
    }
    printf("Debug 6\n");

    return 0;
}
