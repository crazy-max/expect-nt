/*
 * Testcase a2
 *
 *	This testcase may depend on the behavior of a particular
 *	C library.  MSVCRT seems to buffer stderr, if it isn't
 *	attached to a console.
 */

#include <windows.h>
#include <stdio.h>

main()
{
    fprintf(stderr,"Msg1 to stderr\n");
    fflush(stderr);
    fprintf(stdout,"Msg1 to stdout\n");
    fflush(stdout);

    fprintf(stderr,"Msg2 to stderr\n");
    fprintf(stdout,"Msg2 to stdout\n");
    fflush(stdout);

    setbuf(stderr, 0);
    fprintf(stderr,"Msg3 to stderr\n");
    fprintf(stdout,"Msg3 to stdout\n");
    fflush(stdout);

    return 0;
}
