#include <windows.h>
#include <stdio.h>
#include <signal.h>

static BOOL WINAPI
ctrlc(DWORD sig)
{
    static char strc[] = "Received Ctrl-C\n";
    static char strbrk[] = "Received Ctrl-Break\n";
    DWORD written;

    if (sig == CTRL_C_EVENT) {
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), strc, sizeof(strc),
		  &written, NULL);
	// signal(SIGINT, SIG_DFL);
	// signal(SIGINT, ctrlc);
	return TRUE;
    }
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), strbrk, sizeof(strbrk),
	      &written, NULL);
    ExitProcess(1);
    return FALSE;
}

static void
ctrlbreak(int sig)
{
    printf("Received Ctrl-Break\n");
    signal(SIGINT, ctrlbreak);
}

void
main()
{
    char buffer[1024];
    SetConsoleCtrlHandler(ctrlc, TRUE);
    while (1) {
	gets(buffer);
	printf("|\n");
    }
    Sleep(1000000);
}
