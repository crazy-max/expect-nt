#include <tcl.h>
#include <stdlib.h>

#undef ckalloc
#undef ckfree
#undef ckrealloc

char *
Tcl_Alloc(size)
    unsigned int size;
{
    return (char *) malloc(size);
}

void
Tcl_Free(char *ptr)
{
    free(ptr);
}

char *
Tcl_Realloc(ptr, size)
    char *ptr;
    unsigned int size;
{
    return (char *) realloc(ptr, size);
}
