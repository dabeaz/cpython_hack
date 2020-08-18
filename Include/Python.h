#ifndef Py_PYTHON_H
#define Py_PYTHON_H
/* Since this is a "meta-include" file, no #ifdef __cplusplus / extern "C" { */

/* Include nearly all Python header files */

#include "patchlevel.h"
#include "pyconfig.h"
#include "pymacconfig.h"

#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

/* For size_t? */
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

/* CAUTION:  Build setups should ensure that NDEBUG is defined on the
 * compiler command line when building Python in release mode; else
 * assert() calls won't be removed.
 */
#include <assert.h>

#include "pyport.h"
#include "pymacro.h"
#include "pymath.h"
#include "pymem.h"
#include "object.h"
#include "objimpl.h"
#include "typeslots.h"
#include "pyhash.h"
#include "pydebug.h"
#include "bytearrayobject.h"
#include "bytesobject.h"
#include "unicodeobject.h"
#include "longobject.h"
#include "longintrepr.h"
#include "boolobject.h"
#include "floatobject.h"
#include "rangeobject.h"
#include "memoryobject.h"
#include "tupleobject.h"
#include "listobject.h"
#include "dictobject.h"
#include "enumobject.h"
#include "setobject.h"
#include "methodobject.h"
#include "moduleobject.h"
#include "funcobject.h"
#include "classobject.h"
#include "fileobject.h"
#include "pycapsule.h"
#include "code.h"
#include "pyframe.h"
#include "traceback.h"
#include "sliceobject.h"
#include "cellobject.h"
#include "iterobject.h"
#include "genobject.h"
#include "descrobject.h"
#include "weakrefobject.h"
#include "structseq.h"
#include "codecs.h"
#include "pyerrors.h"
#include "cpython/initconfig.h"
#include "pystate.h"
#include "modsupport.h"
#include "compile.h"
#include "pythonrun.h"
#include "pylifecycle.h"
#include "ceval.h"
#include "sysmodule.h"
#include "osmodule.h"
#include "import.h"
#include "abstract.h"
#include "bltinmodule.h"
#include "eval.h"
#include "pyctype.h"
#include "pystrtod.h"
#include "pystrcmp.h"
#include "fileutils.h"
// #include "tracemalloc.h"

#endif /* !Py_PYTHON_H */
