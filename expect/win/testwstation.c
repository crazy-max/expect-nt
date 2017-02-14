/*
 * testwstation.c
 *
 *	Tests creation of a new Windows station and desktop.
 *
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

int
main(int argc, char **argv)
{
    HWINSTA hStation;
    HDESK hDesk;
    BOOL bRet;
    UCHAR buf[1024];
    DWORD need;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD exitStatus;

    /*
     * Create a Windows station with all access attributes but ExitWindows
     */
    hStation = CreateWindowStation(NULL, 0,
	WINSTA_ACCESSCLIPBOARD|WINSTA_ACCESSGLOBALATOMS|
	WINSTA_CREATEDESKTOP|WINSTA_ENUMDESKTOPS|
	WINSTA_ENUMERATE|WINSTA_READATTRIBUTES|
	WINSTA_READSCREEN|WINSTA_WRITEATTRIBUTES, NULL);
    if (hStation == NULL) {
	fprintf(stderr, "Unable to create new window station\n");
	exit(1);
    }
    bRet = SetProcessWindowStation(hStation);
    if (!bRet) {
	fprintf(stderr, "Unable to get access to the new Windows station\n");
	exit(1);
    }

    bRet = GetUserObjectInformation(hStation, UOI_NAME,
	buf, sizeof(buf), &need);
    if (!bRet) {
	fprintf(stderr, "Unable to find window station name\n");
	exit(1);
    }
    printf("New window station: %s\n", buf);

    strcpy(buf, "ExpectDesktop");
    hDesk = CreateDesktop(buf, NULL, NULL, 0,
	DESKTOP_CREATEMENU|	// Required to create a menu on the desktop.
	DESKTOP_CREATEWINDOW|	// Required to create a window on the desktop.
	DESKTOP_ENUMERATE|	// Required for the desktop to be enumerated.
	DESKTOP_HOOKCONTROL|	// Required to establish any of the window hooks.
	DESKTOP_JOURNALPLAYBACK|// Required to perform journal playback on the desktop.
	DESKTOP_JOURNALRECORD|	// Required to perform journal recording on the desktop.
	DESKTOP_READOBJECTS|	// Required to read objects on the desktop.
	DESKTOP_WRITEOBJECTS,	// Required to write objects on the desktop.
	NULL);

    if (hDesk == NULL) {
	fprintf(stderr, "Unable to create new desktop\n");
	exit(1);
    }

    si.cb = sizeof(si);
    si.lpReserved = NULL;
    si.lpDesktop = buf;
    si.lpTitle = NULL;
    si.dwFlags = 0;
    si.cbReserved2 = 0;
    si.lpReserved2 = NULL;
    
#if 0
    bRet = CreateProcess(NULL, "tclsh80.exe testwstation.tcl",
	NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
    if (!bRet) {
	fprintf(stderr, "Unable to create new subprocess\n");
	exit(1);
    }
#else

    bRet = CreateProcess(NULL, "testwprog.exe",
	NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!bRet) {
	fprintf(stderr, "Unable to create new subprocess\n");
	exit(1);
    }
#endif

    /*
     * Wait for the subprocess to exit
     */
    WaitForSingleObject(pi.hProcess, INFINITE);

    bRet = GetExitCodeProcess(pi.hProcess, &exitStatus);
    if (!bRet) {
	fprintf(stderr, "Unable to get exit code of subprocess\n");
	exit(1);
    }

    printf("Subprocess has exited with status 0x%08x\n", exitStatus);

    /*
     * XXX: Have the subprocess create some file that we can than check the
     * results on if it succeeded
     */

    return 0;
}
