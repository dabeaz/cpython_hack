
/* Return the full version string. */

#include "Python.h"

#include "patchlevel.h"

#ifndef COMPILER

// Note the __clang__ conditional has to come before the __GNUC__ one because
// clang pretends to be GCC.
#if defined(__clang__)
#define COMPILER "\n[Clang " __clang_version__ "]"
#elif defined(__GNUC__)
#define COMPILER "\n[GCC " __VERSION__ "]"
// Generic fallbacks.
#elif defined(__cplusplus)
#define COMPILER "[C++]"
#else
#define COMPILER "[C]"
#endif

#endif /* !COMPILER */

const char *
Py_GetVersion(void)
{
    static char version[250];
    PyOS_snprintf(version, sizeof(version), "%.80s (%.80s) %.80s",
                  PY_VERSION, Py_GetBuildInfo(), COMPILER);
    return version;
}
