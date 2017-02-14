/*
 * The standard libraries startup code really seems to be messed up
 * when dealing with consoles.  Depending on which version of the 
 * standard library and which shell starts the program, behavior is
 * different.  If I link it with msvcrt40d.dll and start it from
 * cmd.exe, it doesn't echo anything.  If I link it with msvcrt.dll
 * and start it form cmd.exe, it does echo.  If started from the Korn
 * shell, it never echos anything.  If I can get it to work when linked
 * with msvcrt.dll, it will crash when linked with msvcrt40d.dll
 */

#include <windows.h>
#include <stdio.h>

extern void mainCRTStartup();

void
main(int argc, char **argv)
{
    size_t n = 0;
    char buffer[4096];
    char *p;
    static int initialized = 0;

    if (! initialized) {
	printf("Initializing\n");
	initialized = 1;
	FreeConsole();
	AllocConsole();
	mainCRTStartup();
	printf("Initialized\n");
    }

#if 0
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD count;

    WriteFile(out, "WriteFile: Entering main\n", sizeof("Entering main\n"),
	      &count, NULL);
#endif
    printf("printf: Entering main\n");
    fflush(stdout);
    do {
	p = fgets(buffer, sizeof(buffer)-1, stdin);
	if (p) {
	    fputs(p, stdout);
	}
    } while (p);
}

void specialCRTStartup(void)
{
#if 0
    HANDLE h = NULL, h1 = NULL, h2 = NULL, h3 = NULL;
    /*
     * Sleep for 30 seconds before allowing the normal initialization to
     * take place
     */

    /* Sleep(30000); */
#if 1
    h1 = GetStdHandle(STD_INPUT_HANDLE);
    if (h1 != INVALID_HANDLE_VALUE) {
	CloseHandle(h1);
    }
    h2 = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h2 != INVALID_HANDLE_VALUE && h2 != h1) {
	CloseHandle(h2);
    }
    h3 = GetStdHandle(STD_ERROR_HANDLE);
    if (h3 != INVALID_HANDLE_VALUE && h3 != h2 && h3 != h1) {
	CloseHandle(h3);
    }
#endif

    FreeConsole();
    AllocConsole();

#if 0
    h = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,
				  FILE_SHARE_READ|FILE_SHARE_WRITE,
				  NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    SetConsoleActiveScreenBuffer(h);
#endif
#if 1
    h = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE, 
		   FILE_SHARE_READ | FILE_SHARE_WRITE,
		   NULL, OPEN_EXISTING, 0, NULL);
    SetStdHandle(STD_INPUT_HANDLE, h);
    h = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, 
		   FILE_SHARE_READ | FILE_SHARE_WRITE,
		   NULL, OPEN_EXISTING, 0, NULL);
    SetStdHandle(STD_OUTPUT_HANDLE, h);
    SetStdHandle(STD_ERROR_HANDLE, h);
#endif

#if 0
    SetStdHandle(STD_INPUT_HANDLE, h);
    SetStdHandle(STD_OUTPUT_HANDLE, h);
    SetStdHandle(STD_ERROR_HANDLE, h);
#endif
#endif
    mainCRTStartup();
}
