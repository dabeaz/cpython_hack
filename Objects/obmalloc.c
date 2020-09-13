#include "Python.h"

#include <stdbool.h>

/* Python's malloc wrappers (see pymem.h) */

void *
PyMem_Malloc(size_t size)
{
    /* PyMem_RawMalloc(0) means malloc(1). Some systems would return NULL
       for malloc(0), which would be treated as an error. Some platforms would
       return a pointer with no memory behind it, which would break pymalloc.
       To solve these problems, allocate an extra byte. */
    if (size == 0)
        size = 1;
    return malloc(size);
}

void *
PyMem_Calloc(size_t nelem, size_t elsize)
{
    /* PyMem_RawCalloc(0, 0) means calloc(1, 1). Some systems would return NULL
       for calloc(0, 0), which would be treated as an error. Some platforms
       would return a pointer with no memory behind it, which would break
       pymalloc.  To solve these problems, allocate an extra byte. */
    if (nelem == 0 || elsize == 0) {
        nelem = 1;
        elsize = 1;
    }
    return calloc(nelem, elsize);
}

void *
PyMem_Realloc(void *ptr, size_t size)
{
    if (size == 0)
        size = 1;
    return realloc(ptr, size);
}

void
PyMem_Free(void *ptr)
{
    free(ptr);
}

char *
_PyMem_Strdup(const char *str)
{
    assert(str != NULL);
    size_t size = strlen(str) + 1;
    char *copy = PyMem_Malloc(size);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, str, size);
    return copy;
}
