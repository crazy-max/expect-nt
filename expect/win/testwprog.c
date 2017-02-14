#include <windows.h>
#include <stdio.h>

int
APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hPrev,
		 LPSTR lpComdLine, int nCmdShow)
{
    BOOL bRet;
    FILE *fp;

    Sleep(5000);
    bRet = AllocConsole();

    fp = fopen("d:\\temp\\testwprog.log", "w+");
    if (fp) {
	fprintf(fp, "Result from AllocConsole() was %s\n",
	    bRet ? "TRUE" : "FALSE");
	if (!bRet) {
	    fprintf(fp, "Error was %d (0x%08x)\n",
		GetLastError(), GetLastError());
	}
    }
    fclose(fp);
    return 0;
}
