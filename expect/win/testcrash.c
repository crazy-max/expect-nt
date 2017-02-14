/*
 * testcrash.c
 *
 *	This program is used to check stack tracebacks with Expect.
 *	It purposely crashes with several frames on the stack.
 */

#include <stdio.h>


int
crash(int a, int b)
{
    char *p = NULL;

    /* Crash occurs here */
    *p = 0;

    return a*b;
}

int
subroutine4(int a, int b)
{
    return crash(a+1, b+2);
}

int
subroutine3(int a, int b)
{
    return subroutine4(a+1,b+2);
}

int
subroutine2(int a, int b)
{
    return subroutine3(a+1,b+2);
}

int
subroutine1(int a, int b)
{
    return subroutine2(a+1,b+2);
}

int
main(int argc, char **argv)
{
    return subroutine1(100,200);
    return 0;
}
