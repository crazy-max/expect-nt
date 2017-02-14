#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

void
specialCRTStartup()
{
    HANDLE console;
    COORD coord;
    DWORD n;

#define ESTR "String at x=1, y=2\n\nAfter two \\n's.  Lots of additonal characters and stuff.  Want to see what happens when we wrap"

    console = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, 
	FILE_SHARE_READ | FILE_SHARE_WRITE,
	NULL, OPEN_EXISTING, 0, NULL);

    coord.X = 1;
    coord.Y = 5;
    WriteConsoleOutputCharacterA(console, ESTR, lstrlenA(ESTR),
	coord, &n);
    // Sleep(20000);
}
