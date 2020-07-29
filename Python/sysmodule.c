
/* System module */

/*
Various bits of information used by the interpreter are collected in
module 'sys'.
Function member:
- exit(sts): raise SystemExit
Data members:
- stdin, stdout, stderr: standard file objects
- modules: the table of modules (dictionary)
- path: module search path (list of strings)
- argv: script arguments (list of strings)
- ps1, ps2: optional primary and secondary prompts (strings)
*/

#include "Python.h"
#include "code.h"
#include "frameobject.h"          // PyFrame_GetBack()
#include "pycore_ceval.h"         // _Py_RecursionLimitLowerWaterMark()
#include "pycore_initconfig.h"
#include "pycore_object.h"
#include "pycore_pathconfig.h"
#include "pycore_pyerrors.h"
#include "pycore_pylifecycle.h"
#include "pycore_pymem.h"         // _PyMem_SetDefaultAllocator()
#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "pycore_tupleobject.h"

#include "pydtrace.h"
#include "osdefs.h"               // DELIM
#include <locale.h>

#ifdef MS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif /* MS_WINDOWS */

#ifdef MS_COREDLL
extern void *PyWin_DLLhModule;
/* A string loaded from the DLL at startup: */
extern const char *PyWin_DLLVersionString;
#endif

/*[clinic input]
module sys
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=3726b388feee8cea]*/

/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(sys_displayhook__doc__,
"displayhook($module, object, /)\n"
"--\n"
"\n"
"Print an object to sys.stdout and also save it in builtins._");

#define SYS_DISPLAYHOOK_METHODDEF    \
    {"displayhook", (PyCFunction)sys_displayhook, METH_O, sys_displayhook__doc__},

PyDoc_STRVAR(sys_excepthook__doc__,
"excepthook($module, exctype, value, traceback, /)\n"
"--\n"
"\n"
"Handle an exception by displaying it with a traceback on sys.stderr.");

#define SYS_EXCEPTHOOK_METHODDEF    \
    {"excepthook", (PyCFunction)(void(*)(void))sys_excepthook, METH_FASTCALL, sys_excepthook__doc__},

static PyObject *
sys_excepthook_impl(PyObject *module, PyObject *exctype, PyObject *value,
                    PyObject *traceback);

static PyObject *
sys_excepthook(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *exctype;
    PyObject *value;
    PyObject *traceback;

    if (!_PyArg_CheckPositional("excepthook", nargs, 3, 3)) {
        goto exit;
    }
    exctype = args[0];
    value = args[1];
    traceback = args[2];
    return_value = sys_excepthook_impl(module, exctype, value, traceback);

exit:
    return return_value;
}

PyDoc_STRVAR(sys_exc_info__doc__,
"exc_info($module, /)\n"
"--\n"
"\n"
"Return current exception information: (type, value, traceback).\n"
"\n"
"Return information about the most recent exception caught by an except\n"
"clause in the current stack frame or in an older stack frame.");

#define SYS_EXC_INFO_METHODDEF    \
    {"exc_info", (PyCFunction)sys_exc_info, METH_NOARGS, sys_exc_info__doc__},

static PyObject *
sys_exc_info_impl(PyObject *module);

static PyObject *
sys_exc_info(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_exc_info_impl(module);
}

PyDoc_STRVAR(sys_unraisablehook__doc__,
"unraisablehook($module, unraisable, /)\n"
"--\n"
"\n"
"Handle an unraisable exception.\n"
"\n"
"The unraisable argument has the following attributes:\n"
"\n"
"* exc_type: Exception type.\n"
"* exc_value: Exception value, can be None.\n"
"* exc_traceback: Exception traceback, can be None.\n"
"* err_msg: Error message, can be None.\n"
"* object: Object causing the exception, can be None.");

#define SYS_UNRAISABLEHOOK_METHODDEF    \
    {"unraisablehook", (PyCFunction)sys_unraisablehook, METH_O, sys_unraisablehook__doc__},

PyDoc_STRVAR(sys_exit__doc__,
"exit($module, status=None, /)\n"
"--\n"
"\n"
"Exit the interpreter by raising SystemExit(status).\n"
"\n"
"If the status is omitted or None, it defaults to zero (i.e., success).\n"
"If the status is an integer, it will be used as the system exit status.\n"
"If it is another kind of object, it will be printed and the system\n"
"exit status will be one (i.e., failure).");

#define SYS_EXIT_METHODDEF    \
    {"exit", (PyCFunction)(void(*)(void))sys_exit, METH_FASTCALL, sys_exit__doc__},

static PyObject *
sys_exit_impl(PyObject *module, PyObject *status);

static PyObject *
sys_exit(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *status = Py_None;

    if (!_PyArg_CheckPositional("exit", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    status = args[0];
skip_optional:
    return_value = sys_exit_impl(module, status);

exit:
    return return_value;
}

PyDoc_STRVAR(sys_getdefaultencoding__doc__,
"getdefaultencoding($module, /)\n"
"--\n"
"\n"
"Return the current default encoding used by the Unicode implementation.");

#define SYS_GETDEFAULTENCODING_METHODDEF    \
    {"getdefaultencoding", (PyCFunction)sys_getdefaultencoding, METH_NOARGS, sys_getdefaultencoding__doc__},

static PyObject *
sys_getdefaultencoding_impl(PyObject *module);

static PyObject *
sys_getdefaultencoding(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_getdefaultencoding_impl(module);
}

PyDoc_STRVAR(sys_getfilesystemencoding__doc__,
"getfilesystemencoding($module, /)\n"
"--\n"
"\n"
"Return the encoding used to convert Unicode filenames to OS filenames.");

#define SYS_GETFILESYSTEMENCODING_METHODDEF    \
    {"getfilesystemencoding", (PyCFunction)sys_getfilesystemencoding, METH_NOARGS, sys_getfilesystemencoding__doc__},

static PyObject *
sys_getfilesystemencoding_impl(PyObject *module);

static PyObject *
sys_getfilesystemencoding(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_getfilesystemencoding_impl(module);
}

PyDoc_STRVAR(sys_getfilesystemencodeerrors__doc__,
"getfilesystemencodeerrors($module, /)\n"
"--\n"
"\n"
"Return the error mode used Unicode to OS filename conversion.");

#define SYS_GETFILESYSTEMENCODEERRORS_METHODDEF    \
    {"getfilesystemencodeerrors", (PyCFunction)sys_getfilesystemencodeerrors, METH_NOARGS, sys_getfilesystemencodeerrors__doc__},

static PyObject *
sys_getfilesystemencodeerrors_impl(PyObject *module);

static PyObject *
sys_getfilesystemencodeerrors(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_getfilesystemencodeerrors_impl(module);
}

PyDoc_STRVAR(sys_intern__doc__,
"intern($module, string, /)\n"
"--\n"
"\n"
"``Intern\'\' the given string.\n"
"\n"
"This enters the string in the (global) table of interned strings whose\n"
"purpose is to speed up dictionary lookups. Return the string itself or\n"
"the previously interned string object with the same value.");

#define SYS_INTERN_METHODDEF    \
    {"intern", (PyCFunction)sys_intern, METH_O, sys_intern__doc__},

static PyObject *
sys_intern_impl(PyObject *module, PyObject *s);

static PyObject *
sys_intern(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    PyObject *s;

    if (!PyUnicode_Check(arg)) {
        _PyArg_BadArgument("intern", "argument", "str", arg);
        goto exit;
    }
    if (PyUnicode_READY(arg) == -1) {
        goto exit;
    }
    s = arg;
    return_value = sys_intern_impl(module, s);

exit:
    return return_value;
}

PyDoc_STRVAR(sys_gettrace__doc__,
"gettrace($module, /)\n"
"--\n"
"\n"
"Return the global debug tracing function set with sys.settrace.\n"
"\n"
"See the debugger chapter in the library manual.");

#define SYS_GETTRACE_METHODDEF    \
    {"gettrace", (PyCFunction)sys_gettrace, METH_NOARGS, sys_gettrace__doc__},

static PyObject *
sys_gettrace_impl(PyObject *module);

static PyObject *
sys_gettrace(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_gettrace_impl(module);
}

PyDoc_STRVAR(sys_getprofile__doc__,
"getprofile($module, /)\n"
"--\n"
"\n"
"Return the profiling function set with sys.setprofile.\n"
"\n"
"See the profiler chapter in the library manual.");

#define SYS_GETPROFILE_METHODDEF    \
    {"getprofile", (PyCFunction)sys_getprofile, METH_NOARGS, sys_getprofile__doc__},

static PyObject *
sys_getprofile_impl(PyObject *module);

static PyObject *
sys_getprofile(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_getprofile_impl(module);
}

PyDoc_STRVAR(sys_setrecursionlimit__doc__,
"setrecursionlimit($module, limit, /)\n"
"--\n"
"\n"
"Set the maximum depth of the Python interpreter stack to n.\n"
"\n"
"This limit prevents infinite recursion from causing an overflow of the C\n"
"stack and crashing Python.  The highest possible limit is platform-\n"
"dependent.");

#define SYS_SETRECURSIONLIMIT_METHODDEF    \
    {"setrecursionlimit", (PyCFunction)sys_setrecursionlimit, METH_O, sys_setrecursionlimit__doc__},

static PyObject *
sys_setrecursionlimit_impl(PyObject *module, int new_limit);

static PyObject *
sys_setrecursionlimit(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int new_limit;

    new_limit = _PyLong_AsInt(arg);
    if (new_limit == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = sys_setrecursionlimit_impl(module, new_limit);

exit:
    return return_value;
}

PyDoc_STRVAR(sys_set_coroutine_origin_tracking_depth__doc__,
"set_coroutine_origin_tracking_depth($module, /, depth)\n"
"--\n"
"\n"
"Enable or disable origin tracking for coroutine objects in this thread.\n"
"\n"
"Coroutine objects will track \'depth\' frames of traceback information\n"
"about where they came from, available in their cr_origin attribute.\n"
"\n"
"Set a depth of 0 to disable.");

#define SYS_SET_COROUTINE_ORIGIN_TRACKING_DEPTH_METHODDEF    \
    {"set_coroutine_origin_tracking_depth", (PyCFunction)(void(*)(void))sys_set_coroutine_origin_tracking_depth, METH_FASTCALL|METH_KEYWORDS, sys_set_coroutine_origin_tracking_depth__doc__},

static PyObject *
sys_set_coroutine_origin_tracking_depth_impl(PyObject *module, int depth);

static PyObject *
sys_set_coroutine_origin_tracking_depth(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"depth", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "set_coroutine_origin_tracking_depth", 0};
    PyObject *argsbuf[1];
    int depth;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    depth = _PyLong_AsInt(args[0]);
    if (depth == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = sys_set_coroutine_origin_tracking_depth_impl(module, depth);

exit:
    return return_value;
}

PyDoc_STRVAR(sys_get_coroutine_origin_tracking_depth__doc__,
"get_coroutine_origin_tracking_depth($module, /)\n"
"--\n"
"\n"
"Check status of origin tracking for coroutine objects in this thread.");

#define SYS_GET_COROUTINE_ORIGIN_TRACKING_DEPTH_METHODDEF    \
    {"get_coroutine_origin_tracking_depth", (PyCFunction)sys_get_coroutine_origin_tracking_depth, METH_NOARGS, sys_get_coroutine_origin_tracking_depth__doc__},

static int
sys_get_coroutine_origin_tracking_depth_impl(PyObject *module);

static PyObject *
sys_get_coroutine_origin_tracking_depth(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    PyObject *return_value = NULL;
    int _return_value;

    _return_value = sys_get_coroutine_origin_tracking_depth_impl(module);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyLong_FromLong((long)_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(sys_get_asyncgen_hooks__doc__,
"get_asyncgen_hooks($module, /)\n"
"--\n"
"\n"
"Return the installed asynchronous generators hooks.\n"
"\n"
"This returns a namedtuple of the form (firstiter, finalizer).");

#define SYS_GET_ASYNCGEN_HOOKS_METHODDEF    \
    {"get_asyncgen_hooks", (PyCFunction)sys_get_asyncgen_hooks, METH_NOARGS, sys_get_asyncgen_hooks__doc__},

static PyObject *
sys_get_asyncgen_hooks_impl(PyObject *module);

static PyObject *
sys_get_asyncgen_hooks(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_get_asyncgen_hooks_impl(module);
}

PyDoc_STRVAR(sys_getrecursionlimit__doc__,
"getrecursionlimit($module, /)\n"
"--\n"
"\n"
"Return the current value of the recursion limit.\n"
"\n"
"The recursion limit is the maximum depth of the Python interpreter\n"
"stack.  This limit prevents infinite recursion from causing an overflow\n"
"of the C stack and crashing Python.");

#define SYS_GETRECURSIONLIMIT_METHODDEF    \
    {"getrecursionlimit", (PyCFunction)sys_getrecursionlimit, METH_NOARGS, sys_getrecursionlimit__doc__},

static PyObject *
sys_getrecursionlimit_impl(PyObject *module);

static PyObject *
sys_getrecursionlimit(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_getrecursionlimit_impl(module);
}

#if defined(MS_WINDOWS)

PyDoc_STRVAR(sys_getwindowsversion__doc__,
"getwindowsversion($module, /)\n"
"--\n"
"\n"
"Return info about the running version of Windows as a named tuple.\n"
"\n"
"The members are named: major, minor, build, platform, service_pack,\n"
"service_pack_major, service_pack_minor, suite_mask, product_type and\n"
"platform_version. For backward compatibility, only the first 5 items\n"
"are available by indexing. All elements are numbers, except\n"
"service_pack and platform_type which are strings, and platform_version\n"
"which is a 3-tuple. Platform is always 2. Product_type may be 1 for a\n"
"workstation, 2 for a domain controller, 3 for a server.\n"
"Platform_version is a 3-tuple containing a version number that is\n"
"intended for identifying the OS rather than feature detection.");

#define SYS_GETWINDOWSVERSION_METHODDEF    \
    {"getwindowsversion", (PyCFunction)sys_getwindowsversion, METH_NOARGS, sys_getwindowsversion__doc__},

static PyObject *
sys_getwindowsversion_impl(PyObject *module);

static PyObject *
sys_getwindowsversion(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_getwindowsversion_impl(module);
}

#endif /* defined(MS_WINDOWS) */

#if defined(MS_WINDOWS)

PyDoc_STRVAR(sys__enablelegacywindowsfsencoding__doc__,
"_enablelegacywindowsfsencoding($module, /)\n"
"--\n"
"\n"
"Changes the default filesystem encoding to mbcs:replace.\n"
"\n"
"This is done for consistency with earlier versions of Python. See PEP\n"
"529 for more information.\n"
"\n"
"This is equivalent to defining the PYTHONLEGACYWINDOWSFSENCODING\n"
"environment variable before launching Python.");

#define SYS__ENABLELEGACYWINDOWSFSENCODING_METHODDEF    \
    {"_enablelegacywindowsfsencoding", (PyCFunction)sys__enablelegacywindowsfsencoding, METH_NOARGS, sys__enablelegacywindowsfsencoding__doc__},

static PyObject *
sys__enablelegacywindowsfsencoding_impl(PyObject *module);

static PyObject *
sys__enablelegacywindowsfsencoding(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys__enablelegacywindowsfsencoding_impl(module);
}

#endif /* defined(MS_WINDOWS) */

#if defined(HAVE_DLOPEN)

PyDoc_STRVAR(sys_setdlopenflags__doc__,
"setdlopenflags($module, flags, /)\n"
"--\n"
"\n"
"Set the flags used by the interpreter for dlopen calls.\n"
"\n"
"This is used, for example, when the interpreter loads extension\n"
"modules. Among other things, this will enable a lazy resolving of\n"
"symbols when importing a module, if called as sys.setdlopenflags(0).\n"
"To share symbols across extension modules, call as\n"
"sys.setdlopenflags(os.RTLD_GLOBAL).  Symbolic names for the flag\n"
"modules can be found in the os module (RTLD_xxx constants, e.g.\n"
"os.RTLD_LAZY).");

#define SYS_SETDLOPENFLAGS_METHODDEF    \
    {"setdlopenflags", (PyCFunction)sys_setdlopenflags, METH_O, sys_setdlopenflags__doc__},

static PyObject *
sys_setdlopenflags_impl(PyObject *module, int new_val);

static PyObject *
sys_setdlopenflags(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int new_val;

    new_val = _PyLong_AsInt(arg);
    if (new_val == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = sys_setdlopenflags_impl(module, new_val);

exit:
    return return_value;
}

#endif /* defined(HAVE_DLOPEN) */

#if defined(HAVE_DLOPEN)

PyDoc_STRVAR(sys_getdlopenflags__doc__,
"getdlopenflags($module, /)\n"
"--\n"
"\n"
"Return the current value of the flags that are used for dlopen calls.\n"
"\n"
"The flag constants are defined in the os module.");

#define SYS_GETDLOPENFLAGS_METHODDEF    \
    {"getdlopenflags", (PyCFunction)sys_getdlopenflags, METH_NOARGS, sys_getdlopenflags__doc__},

static PyObject *
sys_getdlopenflags_impl(PyObject *module);

static PyObject *
sys_getdlopenflags(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_getdlopenflags_impl(module);
}

#endif /* defined(HAVE_DLOPEN) */

#if defined(USE_MALLOPT)

PyDoc_STRVAR(sys_mdebug__doc__,
"mdebug($module, flag, /)\n"
"--\n"
"\n");

#define SYS_MDEBUG_METHODDEF    \
    {"mdebug", (PyCFunction)sys_mdebug, METH_O, sys_mdebug__doc__},

static PyObject *
sys_mdebug_impl(PyObject *module, int flag);

static PyObject *
sys_mdebug(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int flag;

    flag = _PyLong_AsInt(arg);
    if (flag == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = sys_mdebug_impl(module, flag);

exit:
    return return_value;
}

#endif /* defined(USE_MALLOPT) */

PyDoc_STRVAR(sys_getrefcount__doc__,
"getrefcount($module, object, /)\n"
"--\n"
"\n"
"Return the reference count of object.\n"
"\n"
"The count returned is generally one higher than you might expect,\n"
"because it includes the (temporary) reference as an argument to\n"
"getrefcount().");

#define SYS_GETREFCOUNT_METHODDEF    \
    {"getrefcount", (PyCFunction)sys_getrefcount, METH_O, sys_getrefcount__doc__},

static Py_ssize_t
sys_getrefcount_impl(PyObject *module, PyObject *object);

static PyObject *
sys_getrefcount(PyObject *module, PyObject *object)
{
    PyObject *return_value = NULL;
    Py_ssize_t _return_value;

    _return_value = sys_getrefcount_impl(module, object);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyLong_FromSsize_t(_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(sys_getallocatedblocks__doc__,
"getallocatedblocks($module, /)\n"
"--\n"
"\n"
"Return the number of memory blocks currently allocated.");

#define SYS_GETALLOCATEDBLOCKS_METHODDEF    \
    {"getallocatedblocks", (PyCFunction)sys_getallocatedblocks, METH_NOARGS, sys_getallocatedblocks__doc__},

static Py_ssize_t
sys_getallocatedblocks_impl(PyObject *module);

static PyObject *
sys_getallocatedblocks(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    PyObject *return_value = NULL;
    Py_ssize_t _return_value;

    _return_value = sys_getallocatedblocks_impl(module);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyLong_FromSsize_t(_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(sys__getframe__doc__,
"_getframe($module, depth=0, /)\n"
"--\n"
"\n"
"Return a frame object from the call stack.\n"
"\n"
"If optional integer depth is given, return the frame object that many\n"
"calls below the top of the stack.  If that is deeper than the call\n"
"stack, ValueError is raised.  The default for depth is zero, returning\n"
"the frame at the top of the call stack.\n"
"\n"
"This function should be used for internal and specialized purposes\n"
"only.");

#define SYS__GETFRAME_METHODDEF    \
    {"_getframe", (PyCFunction)(void(*)(void))sys__getframe, METH_FASTCALL, sys__getframe__doc__},

static PyObject *
sys__getframe_impl(PyObject *module, int depth);

static PyObject *
sys__getframe(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int depth = 0;

    if (!_PyArg_CheckPositional("_getframe", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    depth = _PyLong_AsInt(args[0]);
    if (depth == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = sys__getframe_impl(module, depth);

exit:
    return return_value;
}

PyDoc_STRVAR(sys__current_frames__doc__,
"_current_frames($module, /)\n"
"--\n"
"\n"
"Return a dict mapping each thread\'s thread id to its current stack frame.\n"
"\n"
"This function should be used for specialized purposes only.");

#define SYS__CURRENT_FRAMES_METHODDEF    \
    {"_current_frames", (PyCFunction)sys__current_frames, METH_NOARGS, sys__current_frames__doc__},

static PyObject *
sys__current_frames_impl(PyObject *module);

static PyObject *
sys__current_frames(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys__current_frames_impl(module);
}

PyDoc_STRVAR(sys_call_tracing__doc__,
"call_tracing($module, func, args, /)\n"
"--\n"
"\n"
"Call func(*args), while tracing is enabled.\n"
"\n"
"The tracing state is saved, and restored afterwards.  This is intended\n"
"to be called from a debugger from a checkpoint, to recursively debug\n"
"some other code.");

#define SYS_CALL_TRACING_METHODDEF    \
    {"call_tracing", (PyCFunction)(void(*)(void))sys_call_tracing, METH_FASTCALL, sys_call_tracing__doc__},

static PyObject *
sys_call_tracing_impl(PyObject *module, PyObject *func, PyObject *funcargs);

static PyObject *
sys_call_tracing(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *func;
    PyObject *funcargs;

    if (!_PyArg_CheckPositional("call_tracing", nargs, 2, 2)) {
        goto exit;
    }
    func = args[0];
    if (!PyTuple_Check(args[1])) {
        _PyArg_BadArgument("call_tracing", "argument 2", "tuple", args[1]);
        goto exit;
    }
    funcargs = args[1];
    return_value = sys_call_tracing_impl(module, func, funcargs);

exit:
    return return_value;
}

PyDoc_STRVAR(sys__debugmallocstats__doc__,
"_debugmallocstats($module, /)\n"
"--\n"
"\n"
"Print summary info to stderr about the state of pymalloc\'s structures.\n"
"\n"
"In Py_DEBUG mode, also perform some expensive internal consistency\n"
"checks.");

#define SYS__DEBUGMALLOCSTATS_METHODDEF    \
    {"_debugmallocstats", (PyCFunction)sys__debugmallocstats, METH_NOARGS, sys__debugmallocstats__doc__},

static PyObject *
sys__debugmallocstats_impl(PyObject *module);

static PyObject *
sys__debugmallocstats(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys__debugmallocstats_impl(module);
}

PyDoc_STRVAR(sys__clear_type_cache__doc__,
"_clear_type_cache($module, /)\n"
"--\n"
"\n"
"Clear the internal type lookup cache.");

#define SYS__CLEAR_TYPE_CACHE_METHODDEF    \
    {"_clear_type_cache", (PyCFunction)sys__clear_type_cache, METH_NOARGS, sys__clear_type_cache__doc__},

static PyObject *
sys__clear_type_cache_impl(PyObject *module);

static PyObject *
sys__clear_type_cache(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys__clear_type_cache_impl(module);
}

PyDoc_STRVAR(sys_is_finalizing__doc__,
"is_finalizing($module, /)\n"
"--\n"
"\n"
"Return True if Python is exiting.");

#define SYS_IS_FINALIZING_METHODDEF    \
    {"is_finalizing", (PyCFunction)sys_is_finalizing, METH_NOARGS, sys_is_finalizing__doc__},

static PyObject *
sys_is_finalizing_impl(PyObject *module);

static PyObject *
sys_is_finalizing(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_is_finalizing_impl(module);
}

#if defined(ANDROID_API_LEVEL)

PyDoc_STRVAR(sys_getandroidapilevel__doc__,
"getandroidapilevel($module, /)\n"
"--\n"
"\n"
"Return the build time API version of Android as an integer.");

#define SYS_GETANDROIDAPILEVEL_METHODDEF    \
    {"getandroidapilevel", (PyCFunction)sys_getandroidapilevel, METH_NOARGS, sys_getandroidapilevel__doc__},

static PyObject *
sys_getandroidapilevel_impl(PyObject *module);

static PyObject *
sys_getandroidapilevel(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return sys_getandroidapilevel_impl(module);
}

#endif /* defined(ANDROID_API_LEVEL) */

#ifndef SYS_GETWINDOWSVERSION_METHODDEF
    #define SYS_GETWINDOWSVERSION_METHODDEF
#endif /* !defined(SYS_GETWINDOWSVERSION_METHODDEF) */

#ifndef SYS__ENABLELEGACYWINDOWSFSENCODING_METHODDEF
    #define SYS__ENABLELEGACYWINDOWSFSENCODING_METHODDEF
#endif /* !defined(SYS__ENABLELEGACYWINDOWSFSENCODING_METHODDEF) */

#ifndef SYS_SETDLOPENFLAGS_METHODDEF
    #define SYS_SETDLOPENFLAGS_METHODDEF
#endif /* !defined(SYS_SETDLOPENFLAGS_METHODDEF) */

#ifndef SYS_GETDLOPENFLAGS_METHODDEF
    #define SYS_GETDLOPENFLAGS_METHODDEF
#endif /* !defined(SYS_GETDLOPENFLAGS_METHODDEF) */

#ifndef SYS_MDEBUG_METHODDEF
    #define SYS_MDEBUG_METHODDEF
#endif /* !defined(SYS_MDEBUG_METHODDEF) */

#ifndef SYS_GETTOTALREFCOUNT_METHODDEF
    #define SYS_GETTOTALREFCOUNT_METHODDEF
#endif /* !defined(SYS_GETTOTALREFCOUNT_METHODDEF) */

#ifndef SYS_GETANDROIDAPILEVEL_METHODDEF
    #define SYS_GETANDROIDAPILEVEL_METHODDEF
#endif /* !defined(SYS_GETANDROIDAPILEVEL_METHODDEF) */
/*[clinic end generated code: output=87baa3357293ea65 input=a9049054013a1b77]*/


_Py_IDENTIFIER(_);
_Py_IDENTIFIER(__sizeof__);
_Py_IDENTIFIER(_xoptions);
_Py_IDENTIFIER(buffer);
_Py_IDENTIFIER(builtins);
_Py_IDENTIFIER(encoding);
_Py_IDENTIFIER(path);
_Py_IDENTIFIER(stdout);
_Py_IDENTIFIER(stderr);
_Py_IDENTIFIER(warnoptions);
_Py_IDENTIFIER(write);

static PyObject *
sys_get_object_id(PyThreadState *tstate, _Py_Identifier *key)
{
    PyObject *sd = tstate->interp->sysdict;
    if (sd == NULL) {
        return NULL;
    }
    return _PyDict_GetItemId(sd, key);
}

PyObject *
_PySys_GetObjectId(_Py_Identifier *key)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return sys_get_object_id(tstate, key);
}

PyObject *
PySys_GetObject(const char *name)
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *sd = tstate->interp->sysdict;
    if (sd == NULL) {
        return NULL;
    }
    return PyDict_GetItemString(sd, name);
}

static int
sys_set_object_id(PyThreadState *tstate, _Py_Identifier *key, PyObject *v)
{
    PyObject *sd = tstate->interp->sysdict;
    if (v == NULL) {
        if (_PyDict_GetItemId(sd, key) == NULL) {
            return 0;
        }
        else {
            return _PyDict_DelItemId(sd, key);
        }
    }
    else {
        return _PyDict_SetItemId(sd, key, v);
    }
}

int
_PySys_SetObjectId(_Py_Identifier *key, PyObject *v)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return sys_set_object_id(tstate, key, v);
}

static int
sys_set_object(PyThreadState *tstate, const char *name, PyObject *v)
{
    PyObject *sd = tstate->interp->sysdict;
    if (v == NULL) {
        if (PyDict_GetItemString(sd, name) == NULL) {
            return 0;
        }
        else {
            return PyDict_DelItemString(sd, name);
        }
    }
    else {
        return PyDict_SetItemString(sd, name, v);
    }
}

int
PySys_SetObject(const char *name, PyObject *v)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return sys_set_object(tstate, name, v);
}

static PyObject *
sys_breakpointhook(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *keywords)
{
    PyThreadState *tstate = _PyThreadState_GET();
    assert(!_PyErr_Occurred(tstate));
    char *envar = Py_GETENV("PYTHONBREAKPOINT");

    if (envar == NULL || strlen(envar) == 0) {
        envar = "pdb.set_trace";
    }
    else if (!strcmp(envar, "0")) {
        /* The breakpoint is explicitly no-op'd. */
        Py_RETURN_NONE;
    }
    /* According to POSIX the string returned by getenv() might be invalidated
     * or the string content might be overwritten by a subsequent call to
     * getenv().  Since importing a module can performs the getenv() calls,
     * we need to save a copy of envar. */
    envar = _PyMem_RawStrdup(envar);
    if (envar == NULL) {
        _PyErr_NoMemory(tstate);
        return NULL;
    }
    const char *last_dot = strrchr(envar, '.');
    const char *attrname = NULL;
    PyObject *modulepath = NULL;

    if (last_dot == NULL) {
        /* The breakpoint is a built-in, e.g. PYTHONBREAKPOINT=int */
        modulepath = PyUnicode_FromString("builtins");
        attrname = envar;
    }
    else if (last_dot != envar) {
        /* Split on the last dot; */
        modulepath = PyUnicode_FromStringAndSize(envar, last_dot - envar);
        attrname = last_dot + 1;
    }
    else {
        goto warn;
    }
    if (modulepath == NULL) {
        PyMem_RawFree(envar);
        return NULL;
    }

    PyObject *module = PyImport_Import(modulepath);
    Py_DECREF(modulepath);

    if (module == NULL) {
        if (_PyErr_ExceptionMatches(tstate, PyExc_ImportError)) {
            goto warn;
        }
        PyMem_RawFree(envar);
        return NULL;
    }

    PyObject *hook = PyObject_GetAttrString(module, attrname);
    Py_DECREF(module);

    if (hook == NULL) {
        if (_PyErr_ExceptionMatches(tstate, PyExc_AttributeError)) {
            goto warn;
        }
        PyMem_RawFree(envar);
        return NULL;
    }
    PyMem_RawFree(envar);
    PyObject *retval = PyObject_Vectorcall(hook, args, nargs, keywords);
    Py_DECREF(hook);
    return retval;

  warn:
    /* If any of the imports went wrong, then warn and ignore. */
    _PyErr_Clear(tstate);
    PyMem_RawFree(envar);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(breakpointhook_doc,
"breakpointhook(*args, **kws)\n"
"\n"
"This hook function is called by built-in breakpoint().\n"
);

/* Write repr(o) to sys.stdout using sys.stdout.encoding and 'backslashreplace'
   error handler. If sys.stdout has a buffer attribute, use
   sys.stdout.buffer.write(encoded), otherwise redecode the string and use
   sys.stdout.write(redecoded).

   Helper function for sys_displayhook(). */
static int
sys_displayhook_unencodable(PyObject *outf, PyObject *o)
{
    PyObject *stdout_encoding = NULL;
    PyObject *encoded, *escaped_str, *repr_str, *buffer, *result;
    const char *stdout_encoding_str;
    int ret;

    stdout_encoding = _PyObject_GetAttrId(outf, &PyId_encoding);
    if (stdout_encoding == NULL)
        goto error;
    stdout_encoding_str = PyUnicode_AsUTF8(stdout_encoding);
    if (stdout_encoding_str == NULL)
        goto error;

    repr_str = PyObject_Repr(o);
    if (repr_str == NULL)
        goto error;
    encoded = PyUnicode_AsEncodedString(repr_str,
                                        stdout_encoding_str,
                                        "backslashreplace");
    Py_DECREF(repr_str);
    if (encoded == NULL)
        goto error;

    if (_PyObject_LookupAttrId(outf, &PyId_buffer, &buffer) < 0) {
        Py_DECREF(encoded);
        goto error;
    }
    if (buffer) {
        result = _PyObject_CallMethodIdOneArg(buffer, &PyId_write, encoded);
        Py_DECREF(buffer);
        Py_DECREF(encoded);
        if (result == NULL)
            goto error;
        Py_DECREF(result);
    }
    else {
        escaped_str = PyUnicode_FromEncodedObject(encoded,
                                                  stdout_encoding_str,
                                                  "strict");
        Py_DECREF(encoded);
        if (PyFile_WriteObject(escaped_str, outf, Py_PRINT_RAW) != 0) {
            Py_DECREF(escaped_str);
            goto error;
        }
        Py_DECREF(escaped_str);
    }
    ret = 0;
    goto finally;

error:
    ret = -1;
finally:
    Py_XDECREF(stdout_encoding);
    return ret;
}

/*[clinic input]
sys.displayhook

    object as o: object
    /

Print an object to sys.stdout and also save it in builtins._
[clinic start generated code]*/

static PyObject *
sys_displayhook(PyObject *module, PyObject *o)
/*[clinic end generated code: output=347477d006df92ed input=08ba730166d7ef72]*/
{
    PyObject *outf;
    PyObject *builtins;
    static PyObject *newline = NULL;
    PyThreadState *tstate = _PyThreadState_GET();

    builtins = _PyImport_GetModuleId(&PyId_builtins);
    if (builtins == NULL) {
        if (!_PyErr_Occurred(tstate)) {
            _PyErr_SetString(tstate, PyExc_RuntimeError,
                             "lost builtins module");
        }
        return NULL;
    }
    Py_DECREF(builtins);

    /* Print value except if None */
    /* After printing, also assign to '_' */
    /* Before, set '_' to None to avoid recursion */
    if (o == Py_None) {
        Py_RETURN_NONE;
    }
    if (_PyObject_SetAttrId(builtins, &PyId__, Py_None) != 0)
        return NULL;
    outf = sys_get_object_id(tstate, &PyId_stdout);
    if (outf == NULL || outf == Py_None) {
        _PyErr_SetString(tstate, PyExc_RuntimeError, "lost sys.stdout");
        return NULL;
    }
    if (PyFile_WriteObject(o, outf, 0) != 0) {
        if (_PyErr_ExceptionMatches(tstate, PyExc_UnicodeEncodeError)) {
            int err;
            /* repr(o) is not encodable to sys.stdout.encoding with
             * sys.stdout.errors error handler (which is probably 'strict') */
            _PyErr_Clear(tstate);
            err = sys_displayhook_unencodable(outf, o);
            if (err) {
                return NULL;
            }
        }
        else {
            return NULL;
        }
    }
    if (newline == NULL) {
        newline = PyUnicode_FromString("\n");
        if (newline == NULL)
            return NULL;
    }
    if (PyFile_WriteObject(newline, outf, Py_PRINT_RAW) != 0)
        return NULL;
    if (_PyObject_SetAttrId(builtins, &PyId__, o) != 0)
        return NULL;
    Py_RETURN_NONE;
}


/*[clinic input]
sys.excepthook

    exctype:   object
    value:     object
    traceback: object
    /

Handle an exception by displaying it with a traceback on sys.stderr.
[clinic start generated code]*/

static PyObject *
sys_excepthook_impl(PyObject *module, PyObject *exctype, PyObject *value,
                    PyObject *traceback)
/*[clinic end generated code: output=18d99fdda21b6b5e input=ecf606fa826f19d9]*/
{
    PyErr_Display(exctype, value, traceback);
    Py_RETURN_NONE;
}


/*[clinic input]
sys.exc_info

Return current exception information: (type, value, traceback).

Return information about the most recent exception caught by an except
clause in the current stack frame or in an older stack frame.
[clinic start generated code]*/

static PyObject *
sys_exc_info_impl(PyObject *module)
/*[clinic end generated code: output=3afd0940cf3a4d30 input=b5c5bf077788a3e5]*/
{
    _PyErr_StackItem *err_info = _PyErr_GetTopmostException(_PyThreadState_GET());
    return Py_BuildValue(
        "(OOO)",
        err_info->exc_type != NULL ? err_info->exc_type : Py_None,
        err_info->exc_value != NULL ? err_info->exc_value : Py_None,
        err_info->exc_traceback != NULL ?
            err_info->exc_traceback : Py_None);
}


/*[clinic input]
sys.unraisablehook

    unraisable: object
    /

Handle an unraisable exception.

The unraisable argument has the following attributes:

* exc_type: Exception type.
* exc_value: Exception value, can be None.
* exc_traceback: Exception traceback, can be None.
* err_msg: Error message, can be None.
* object: Object causing the exception, can be None.
[clinic start generated code]*/

static PyObject *
sys_unraisablehook(PyObject *module, PyObject *unraisable)
/*[clinic end generated code: output=bb92838b32abaa14 input=ec3af148294af8d3]*/
{
    return _PyErr_WriteUnraisableDefaultHook(unraisable);
}


/*[clinic input]
sys.exit

    status: object = None
    /

Exit the interpreter by raising SystemExit(status).

If the status is omitted or None, it defaults to zero (i.e., success).
If the status is an integer, it will be used as the system exit status.
If it is another kind of object, it will be printed and the system
exit status will be one (i.e., failure).
[clinic start generated code]*/

static PyObject *
sys_exit_impl(PyObject *module, PyObject *status)
/*[clinic end generated code: output=13870986c1ab2ec0 input=b86ca9497baa94f2]*/
{
    /* Raise SystemExit so callers may catch it or clean up. */
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_SetObject(tstate, PyExc_SystemExit, status);
    return NULL;
}



/*[clinic input]
sys.getdefaultencoding

Return the current default encoding used by the Unicode implementation.
[clinic start generated code]*/

static PyObject *
sys_getdefaultencoding_impl(PyObject *module)
/*[clinic end generated code: output=256d19dfcc0711e6 input=d416856ddbef6909]*/
{
    return PyUnicode_FromString(PyUnicode_GetDefaultEncoding());
}

/*[clinic input]
sys.getfilesystemencoding

Return the encoding used to convert Unicode filenames to OS filenames.
[clinic start generated code]*/

static PyObject *
sys_getfilesystemencoding_impl(PyObject *module)
/*[clinic end generated code: output=1dc4bdbe9be44aa7 input=8475f8649b8c7d8c]*/
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    const PyConfig *config = _PyInterpreterState_GetConfig(interp);
    return PyUnicode_FromWideChar(config->filesystem_encoding, -1);
}

/*[clinic input]
sys.getfilesystemencodeerrors

Return the error mode used Unicode to OS filename conversion.
[clinic start generated code]*/

static PyObject *
sys_getfilesystemencodeerrors_impl(PyObject *module)
/*[clinic end generated code: output=ba77b36bbf7c96f5 input=22a1e8365566f1e5]*/
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    const PyConfig *config = _PyInterpreterState_GetConfig(interp);
    return PyUnicode_FromWideChar(config->filesystem_errors, -1);
}

/*[clinic input]
sys.intern

    string as s: unicode
    /

``Intern'' the given string.

This enters the string in the (global) table of interned strings whose
purpose is to speed up dictionary lookups. Return the string itself or
the previously interned string object with the same value.
[clinic start generated code]*/

static PyObject *
sys_intern_impl(PyObject *module, PyObject *s)
/*[clinic end generated code: output=be680c24f5c9e5d6 input=849483c006924e2f]*/
{
    PyThreadState *tstate = _PyThreadState_GET();
    if (PyUnicode_CheckExact(s)) {
        Py_INCREF(s);
        PyUnicode_InternInPlace(&s);
        return s;
    }
    else {
        _PyErr_Format(tstate, PyExc_TypeError,
                      "can't intern %.400s", Py_TYPE(s)->tp_name);
        return NULL;
    }
}


/*
 * Cached interned string objects used for calling the profile and
 * trace functions.  Initialized by trace_init().
 */
static PyObject *whatstrings[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

static int
trace_init(void)
{
    static const char * const whatnames[8] = {
        "call", "exception", "line", "return",
        "c_call", "c_exception", "c_return",
        "opcode"
    };
    PyObject *name;
    int i;
    for (i = 0; i < 8; ++i) {
        if (whatstrings[i] == NULL) {
            name = PyUnicode_InternFromString(whatnames[i]);
            if (name == NULL)
                return -1;
            whatstrings[i] = name;
        }
    }
    return 0;
}


static PyObject *
call_trampoline(PyThreadState *tstate, PyObject* callback,
                PyFrameObject *frame, int what, PyObject *arg)
{
    if (PyFrame_FastToLocalsWithError(frame) < 0) {
        return NULL;
    }

    PyObject *stack[3];
    stack[0] = (PyObject *)frame;
    stack[1] = whatstrings[what];
    stack[2] = (arg != NULL) ? arg : Py_None;

    /* call the Python-level function */
    PyObject *result = _PyObject_FastCallTstate(tstate, callback, stack, 3);

    PyFrame_LocalsToFast(frame, 1);
    if (result == NULL) {
        PyTraceBack_Here(frame);
    }

    return result;
}

static int
profile_trampoline(PyObject *self, PyFrameObject *frame,
                   int what, PyObject *arg)
{
    if (arg == NULL) {
        arg = Py_None;
    }

    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *result = call_trampoline(tstate, self, frame, what, arg);
    if (result == NULL) {
        _PyEval_SetProfile(tstate, NULL, NULL);
        return -1;
    }

    Py_DECREF(result);
    return 0;
}

static int
trace_trampoline(PyObject *self, PyFrameObject *frame,
                 int what, PyObject *arg)
{
    PyObject *callback;
    if (what == PyTrace_CALL) {
        callback = self;
    }
    else {
        callback = frame->f_trace;
    }
    if (callback == NULL) {
        return 0;
    }

    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *result = call_trampoline(tstate, callback, frame, what, arg);
    if (result == NULL) {
        _PyEval_SetTrace(tstate, NULL, NULL);
        Py_CLEAR(frame->f_trace);
        return -1;
    }

    if (result != Py_None) {
        Py_XSETREF(frame->f_trace, result);
    }
    else {
        Py_DECREF(result);
    }
    return 0;
}

static PyObject *
sys_settrace(PyObject *self, PyObject *args)
{
    if (trace_init() == -1) {
        return NULL;
    }

    PyThreadState *tstate = _PyThreadState_GET();
    if (args == Py_None) {
        if (_PyEval_SetTrace(tstate, NULL, NULL) < 0) {
            return NULL;
        }
    }
    else {
        if (_PyEval_SetTrace(tstate, trace_trampoline, args) < 0) {
            return NULL;
        }
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(settrace_doc,
"settrace(function)\n\
\n\
Set the global debug tracing function.  It will be called on each\n\
function call.  See the debugger chapter in the library manual."
);

/*[clinic input]
sys.gettrace

Return the global debug tracing function set with sys.settrace.

See the debugger chapter in the library manual.
[clinic start generated code]*/

static PyObject *
sys_gettrace_impl(PyObject *module)
/*[clinic end generated code: output=e97e3a4d8c971b6e input=373b51bb2147f4d8]*/
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *temp = tstate->c_traceobj;

    if (temp == NULL)
        temp = Py_None;
    Py_INCREF(temp);
    return temp;
}

static PyObject *
sys_setprofile(PyObject *self, PyObject *args)
{
    if (trace_init() == -1) {
        return NULL;
    }

    PyThreadState *tstate = _PyThreadState_GET();
    if (args == Py_None) {
        if (_PyEval_SetProfile(tstate, NULL, NULL) < 0) {
            return NULL;
        }
    }
    else {
        if (_PyEval_SetProfile(tstate, profile_trampoline, args) < 0) {
            return NULL;
        }
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(setprofile_doc,
"setprofile(function)\n\
\n\
Set the profiling function.  It will be called on each function call\n\
and return.  See the profiler chapter in the library manual."
);

/*[clinic input]
sys.getprofile

Return the profiling function set with sys.setprofile.

See the profiler chapter in the library manual.
[clinic start generated code]*/

static PyObject *
sys_getprofile_impl(PyObject *module)
/*[clinic end generated code: output=579b96b373448188 input=1b3209d89a32965d]*/
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *temp = tstate->c_profileobj;

    if (temp == NULL)
        temp = Py_None;
    Py_INCREF(temp);
    return temp;
}


/*[clinic input]
sys.setrecursionlimit

    limit as new_limit: int
    /

Set the maximum depth of the Python interpreter stack to n.

This limit prevents infinite recursion from causing an overflow of the C
stack and crashing Python.  The highest possible limit is platform-
dependent.
[clinic start generated code]*/

static PyObject *
sys_setrecursionlimit_impl(PyObject *module, int new_limit)
/*[clinic end generated code: output=35e1c64754800ace input=b0f7a23393924af3]*/
{
    int mark;
    PyThreadState *tstate = _PyThreadState_GET();

    if (new_limit < 1) {
        _PyErr_SetString(tstate, PyExc_ValueError,
                         "recursion limit must be greater or equal than 1");
        return NULL;
    }

    /* Issue #25274: When the recursion depth hits the recursion limit in
       _Py_CheckRecursiveCall(), the overflowed flag of the thread state is
       set to 1 and a RecursionError is raised. The overflowed flag is reset
       to 0 when the recursion depth goes below the low-water mark: see
       Py_LeaveRecursiveCall().

       Reject too low new limit if the current recursion depth is higher than
       the new low-water mark. Otherwise it may not be possible anymore to
       reset the overflowed flag to 0. */
    mark = _Py_RecursionLimitLowerWaterMark(new_limit);
    if (tstate->recursion_depth >= mark) {
        _PyErr_Format(tstate, PyExc_RecursionError,
                      "cannot set the recursion limit to %i at "
                      "the recursion depth %i: the limit is too low",
                      new_limit, tstate->recursion_depth);
        return NULL;
    }

    Py_SetRecursionLimit(new_limit);
    Py_RETURN_NONE;
}

/*[clinic input]
sys.set_coroutine_origin_tracking_depth

  depth: int

Enable or disable origin tracking for coroutine objects in this thread.

Coroutine objects will track 'depth' frames of traceback information
about where they came from, available in their cr_origin attribute.

Set a depth of 0 to disable.
[clinic start generated code]*/

static PyObject *
sys_set_coroutine_origin_tracking_depth_impl(PyObject *module, int depth)
/*[clinic end generated code: output=0a2123c1cc6759c5 input=a1d0a05f89d2c426]*/
{
    PyThreadState *tstate = _PyThreadState_GET();
    if (depth < 0) {
        _PyErr_SetString(tstate, PyExc_ValueError, "depth must be >= 0");
        return NULL;
    }
    _PyEval_SetCoroutineOriginTrackingDepth(tstate, depth);
    Py_RETURN_NONE;
}

/*[clinic input]
sys.get_coroutine_origin_tracking_depth -> int

Check status of origin tracking for coroutine objects in this thread.
[clinic start generated code]*/

static int
sys_get_coroutine_origin_tracking_depth_impl(PyObject *module)
/*[clinic end generated code: output=3699f7be95a3afb8 input=335266a71205b61a]*/
{
    return _PyEval_GetCoroutineOriginTrackingDepth();
}

static PyTypeObject AsyncGenHooksType;

PyDoc_STRVAR(asyncgen_hooks_doc,
"asyncgen_hooks\n\
\n\
A named tuple providing information about asynchronous\n\
generators hooks.  The attributes are read only.");

static PyStructSequence_Field asyncgen_hooks_fields[] = {
    {"firstiter", "Hook to intercept first iteration"},
    {"finalizer", "Hook to intercept finalization"},
    {0}
};

static PyStructSequence_Desc asyncgen_hooks_desc = {
    "asyncgen_hooks",          /* name */
    asyncgen_hooks_doc,        /* doc */
    asyncgen_hooks_fields ,    /* fields */
    2
};

static PyObject *
sys_set_asyncgen_hooks(PyObject *self, PyObject *args, PyObject *kw)
{
    static char *keywords[] = {"firstiter", "finalizer", NULL};
    PyObject *firstiter = NULL;
    PyObject *finalizer = NULL;
    PyThreadState *tstate = _PyThreadState_GET();

    if (!PyArg_ParseTupleAndKeywords(
            args, kw, "|OO", keywords,
            &firstiter, &finalizer)) {
        return NULL;
    }

    if (finalizer && finalizer != Py_None) {
        if (!PyCallable_Check(finalizer)) {
            _PyErr_Format(tstate, PyExc_TypeError,
                          "callable finalizer expected, got %.50s",
                          Py_TYPE(finalizer)->tp_name);
            return NULL;
        }
        if (_PyEval_SetAsyncGenFinalizer(finalizer) < 0) {
            return NULL;
        }
    }
    else if (finalizer == Py_None && _PyEval_SetAsyncGenFinalizer(NULL) < 0) {
        return NULL;
    }

    if (firstiter && firstiter != Py_None) {
        if (!PyCallable_Check(firstiter)) {
            _PyErr_Format(tstate, PyExc_TypeError,
                          "callable firstiter expected, got %.50s",
                          Py_TYPE(firstiter)->tp_name);
            return NULL;
        }
        if (_PyEval_SetAsyncGenFirstiter(firstiter) < 0) {
            return NULL;
        }
    }
    else if (firstiter == Py_None && _PyEval_SetAsyncGenFirstiter(NULL) < 0) {
        return NULL;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(set_asyncgen_hooks_doc,
"set_asyncgen_hooks(* [, firstiter] [, finalizer])\n\
\n\
Set a finalizer for async generators objects."
);

/*[clinic input]
sys.get_asyncgen_hooks

Return the installed asynchronous generators hooks.

This returns a namedtuple of the form (firstiter, finalizer).
[clinic start generated code]*/

static PyObject *
sys_get_asyncgen_hooks_impl(PyObject *module)
/*[clinic end generated code: output=53a253707146f6cf input=3676b9ea62b14625]*/
{
    PyObject *res;
    PyObject *firstiter = _PyEval_GetAsyncGenFirstiter();
    PyObject *finalizer = _PyEval_GetAsyncGenFinalizer();

    res = PyStructSequence_New(&AsyncGenHooksType);
    if (res == NULL) {
        return NULL;
    }

    if (firstiter == NULL) {
        firstiter = Py_None;
    }

    if (finalizer == NULL) {
        finalizer = Py_None;
    }

    Py_INCREF(firstiter);
    PyStructSequence_SET_ITEM(res, 0, firstiter);

    Py_INCREF(finalizer);
    PyStructSequence_SET_ITEM(res, 1, finalizer);

    return res;
}


static PyTypeObject Hash_InfoType;

PyDoc_STRVAR(hash_info_doc,
"hash_info\n\
\n\
A named tuple providing parameters used for computing\n\
hashes. The attributes are read only.");

static PyStructSequence_Field hash_info_fields[] = {
    {"width", "width of the type used for hashing, in bits"},
    {"modulus", "prime number giving the modulus on which the hash "
                "function is based"},
    {"inf", "value to be used for hash of a positive infinity"},
    {"nan", "value to be used for hash of a nan"},
    {"imag", "multiplier used for the imaginary part of a complex number"},
    {"algorithm", "name of the algorithm for hashing of str, bytes and "
                  "memoryviews"},
    {"hash_bits", "internal output size of hash algorithm"},
    {"seed_bits", "seed size of hash algorithm"},
    {"cutoff", "small string optimization cutoff"},
    {NULL, NULL}
};

static PyStructSequence_Desc hash_info_desc = {
    "sys.hash_info",
    hash_info_doc,
    hash_info_fields,
    9,
};

static PyObject *
get_hash_info(PyThreadState *tstate)
{
    PyObject *hash_info;
    int field = 0;
    PyHash_FuncDef *hashfunc;
    hash_info = PyStructSequence_New(&Hash_InfoType);
    if (hash_info == NULL)
        return NULL;
    hashfunc = PyHash_GetFuncDef();
    PyStructSequence_SET_ITEM(hash_info, field++,
                              PyLong_FromLong(8*sizeof(Py_hash_t)));
    PyStructSequence_SET_ITEM(hash_info, field++,
                              PyLong_FromSsize_t(_PyHASH_MODULUS));
    PyStructSequence_SET_ITEM(hash_info, field++,
                              PyLong_FromLong(_PyHASH_INF));
    PyStructSequence_SET_ITEM(hash_info, field++,
                              PyLong_FromLong(_PyHASH_NAN));
    PyStructSequence_SET_ITEM(hash_info, field++,
                              PyLong_FromLong(_PyHASH_IMAG));
    PyStructSequence_SET_ITEM(hash_info, field++,
                              PyUnicode_FromString(hashfunc->name));
    PyStructSequence_SET_ITEM(hash_info, field++,
                              PyLong_FromLong(hashfunc->hash_bits));
    PyStructSequence_SET_ITEM(hash_info, field++,
                              PyLong_FromLong(hashfunc->seed_bits));
    PyStructSequence_SET_ITEM(hash_info, field++,
                              PyLong_FromLong(Py_HASH_CUTOFF));
    if (_PyErr_Occurred(tstate)) {
        Py_CLEAR(hash_info);
        return NULL;
    }
    return hash_info;
}
/*[clinic input]
sys.getrecursionlimit

Return the current value of the recursion limit.

The recursion limit is the maximum depth of the Python interpreter
stack.  This limit prevents infinite recursion from causing an overflow
of the C stack and crashing Python.
[clinic start generated code]*/

static PyObject *
sys_getrecursionlimit_impl(PyObject *module)
/*[clinic end generated code: output=d571fb6b4549ef2e input=1c6129fd2efaeea8]*/
{
    return PyLong_FromLong(Py_GetRecursionLimit());
}

#ifdef MS_WINDOWS

static PyTypeObject WindowsVersionType = {0, 0, 0, 0, 0, 0};

static PyStructSequence_Field windows_version_fields[] = {
    {"major", "Major version number"},
    {"minor", "Minor version number"},
    {"build", "Build number"},
    {"platform", "Operating system platform"},
    {"service_pack", "Latest Service Pack installed on the system"},
    {"service_pack_major", "Service Pack major version number"},
    {"service_pack_minor", "Service Pack minor version number"},
    {"suite_mask", "Bit mask identifying available product suites"},
    {"product_type", "System product type"},
    {"platform_version", "Diagnostic version number"},
    {0}
};

static PyStructSequence_Desc windows_version_desc = {
    "sys.getwindowsversion",       /* name */
    sys_getwindowsversion__doc__,  /* doc */
    windows_version_fields,        /* fields */
    5                              /* For backward compatibility,
                                      only the first 5 items are accessible
                                      via indexing, the rest are name only */
};

/* Disable deprecation warnings about GetVersionEx as the result is
   being passed straight through to the caller, who is responsible for
   using it correctly. */
#pragma warning(push)
#pragma warning(disable:4996)

/*[clinic input]
sys.getwindowsversion

Return info about the running version of Windows as a named tuple.

The members are named: major, minor, build, platform, service_pack,
service_pack_major, service_pack_minor, suite_mask, product_type and
platform_version. For backward compatibility, only the first 5 items
are available by indexing. All elements are numbers, except
service_pack and platform_type which are strings, and platform_version
which is a 3-tuple. Platform is always 2. Product_type may be 1 for a
workstation, 2 for a domain controller, 3 for a server.
Platform_version is a 3-tuple containing a version number that is
intended for identifying the OS rather than feature detection.
[clinic start generated code]*/

static PyObject *
sys_getwindowsversion_impl(PyObject *module)
/*[clinic end generated code: output=1ec063280b932857 input=73a228a328fee63a]*/
{
    PyObject *version;
    int pos = 0;
    OSVERSIONINFOEXW ver;
    DWORD realMajor, realMinor, realBuild;
    HANDLE hKernel32;
    wchar_t kernel32_path[MAX_PATH];
    LPVOID verblock;
    DWORD verblock_size;
    PyThreadState *tstate = _PyThreadState_GET();

    ver.dwOSVersionInfoSize = sizeof(ver);
    if (!GetVersionExW((OSVERSIONINFOW*) &ver))
        return PyErr_SetFromWindowsErr(0);

    version = PyStructSequence_New(&WindowsVersionType);
    if (version == NULL)
        return NULL;

    PyStructSequence_SET_ITEM(version, pos++, PyLong_FromLong(ver.dwMajorVersion));
    PyStructSequence_SET_ITEM(version, pos++, PyLong_FromLong(ver.dwMinorVersion));
    PyStructSequence_SET_ITEM(version, pos++, PyLong_FromLong(ver.dwBuildNumber));
    PyStructSequence_SET_ITEM(version, pos++, PyLong_FromLong(ver.dwPlatformId));
    PyStructSequence_SET_ITEM(version, pos++, PyUnicode_FromWideChar(ver.szCSDVersion, -1));
    PyStructSequence_SET_ITEM(version, pos++, PyLong_FromLong(ver.wServicePackMajor));
    PyStructSequence_SET_ITEM(version, pos++, PyLong_FromLong(ver.wServicePackMinor));
    PyStructSequence_SET_ITEM(version, pos++, PyLong_FromLong(ver.wSuiteMask));
    PyStructSequence_SET_ITEM(version, pos++, PyLong_FromLong(ver.wProductType));

    realMajor = ver.dwMajorVersion;
    realMinor = ver.dwMinorVersion;
    realBuild = ver.dwBuildNumber;

    // GetVersion will lie if we are running in a compatibility mode.
    // We need to read the version info from a system file resource
    // to accurately identify the OS version. If we fail for any reason,
    // just return whatever GetVersion said.
    hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (hKernel32 && GetModuleFileNameW(hKernel32, kernel32_path, MAX_PATH) &&
        (verblock_size = GetFileVersionInfoSizeW(kernel32_path, NULL)) &&
        (verblock = PyMem_RawMalloc(verblock_size))) {
        VS_FIXEDFILEINFO *ffi;
        UINT ffi_len;

        if (GetFileVersionInfoW(kernel32_path, 0, verblock_size, verblock) &&
            VerQueryValueW(verblock, L"", (LPVOID)&ffi, &ffi_len)) {
            realMajor = HIWORD(ffi->dwProductVersionMS);
            realMinor = LOWORD(ffi->dwProductVersionMS);
            realBuild = HIWORD(ffi->dwProductVersionLS);
        }
        PyMem_RawFree(verblock);
    }
    PyStructSequence_SET_ITEM(version, pos++, Py_BuildValue("(kkk)",
        realMajor,
        realMinor,
        realBuild
    ));

    if (_PyErr_Occurred(tstate)) {
        Py_DECREF(version);
        return NULL;
    }

    return version;
}

#pragma warning(pop)

/*[clinic input]
sys._enablelegacywindowsfsencoding

Changes the default filesystem encoding to mbcs:replace.

This is done for consistency with earlier versions of Python. See PEP
529 for more information.

This is equivalent to defining the PYTHONLEGACYWINDOWSFSENCODING
environment variable before launching Python.
[clinic start generated code]*/

static PyObject *
sys__enablelegacywindowsfsencoding_impl(PyObject *module)
/*[clinic end generated code: output=f5c3855b45e24fe9 input=2bfa931a20704492]*/
{
    if (_PyUnicode_EnableLegacyWindowsFSEncoding() < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

#endif /* MS_WINDOWS */

#ifdef HAVE_DLOPEN

/*[clinic input]
sys.setdlopenflags

    flags as new_val: int
    /

Set the flags used by the interpreter for dlopen calls.

This is used, for example, when the interpreter loads extension
modules. Among other things, this will enable a lazy resolving of
symbols when importing a module, if called as sys.setdlopenflags(0).
To share symbols across extension modules, call as
sys.setdlopenflags(os.RTLD_GLOBAL).  Symbolic names for the flag
modules can be found in the os module (RTLD_xxx constants, e.g.
os.RTLD_LAZY).
[clinic start generated code]*/

static PyObject *
sys_setdlopenflags_impl(PyObject *module, int new_val)
/*[clinic end generated code: output=ec918b7fe0a37281 input=4c838211e857a77f]*/
{
    PyThreadState *tstate = _PyThreadState_GET();
    tstate->interp->dlopenflags = new_val;
    Py_RETURN_NONE;
}


/*[clinic input]
sys.getdlopenflags

Return the current value of the flags that are used for dlopen calls.

The flag constants are defined in the os module.
[clinic start generated code]*/

static PyObject *
sys_getdlopenflags_impl(PyObject *module)
/*[clinic end generated code: output=e92cd1bc5005da6e input=dc4ea0899c53b4b6]*/
{
    PyThreadState *tstate = _PyThreadState_GET();
    return PyLong_FromLong(tstate->interp->dlopenflags);
}

#endif  /* HAVE_DLOPEN */

#ifdef USE_MALLOPT
/* Link with -lmalloc (or -lmpc) on an SGI */
#include <malloc.h>

/*[clinic input]
sys.mdebug

    flag: int
    /
[clinic start generated code]*/

static PyObject *
sys_mdebug_impl(PyObject *module, int flag)
/*[clinic end generated code: output=5431d545847c3637 input=151d150ae1636f8a]*/
{
    int flag;
    mallopt(M_DEBUG, flag);
    Py_RETURN_NONE;
}
#endif /* USE_MALLOPT */

size_t
_PySys_GetSizeOf(PyObject *o)
{
    PyObject *res = NULL;
    PyObject *method;
    Py_ssize_t size;
    PyThreadState *tstate = _PyThreadState_GET();

    /* Make sure the type is initialized. float gets initialized late */
    if (PyType_Ready(Py_TYPE(o)) < 0) {
        return (size_t)-1;
    }

    method = _PyObject_LookupSpecial(o, &PyId___sizeof__);
    if (method == NULL) {
        if (!_PyErr_Occurred(tstate)) {
            _PyErr_Format(tstate, PyExc_TypeError,
                          "Type %.100s doesn't define __sizeof__",
                          Py_TYPE(o)->tp_name);
        }
    }
    else {
        res = _PyObject_CallNoArg(method);
        Py_DECREF(method);
    }

    if (res == NULL)
        return (size_t)-1;

    size = PyLong_AsSsize_t(res);
    Py_DECREF(res);
    if (size == -1 && _PyErr_Occurred(tstate))
        return (size_t)-1;

    if (size < 0) {
        _PyErr_SetString(tstate, PyExc_ValueError,
                          "__sizeof__() should return >= 0");
        return (size_t)-1;
    }

    /* add gc_head size */
    if (_PyObject_IS_GC(o))
        return ((size_t)size) + sizeof(PyGC_Head);
    return (size_t)size;
}

static PyObject *
sys_getsizeof(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"object", "default", 0};
    size_t size;
    PyObject *o, *dflt = NULL;
    PyThreadState *tstate = _PyThreadState_GET();

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:getsizeof",
                                     kwlist, &o, &dflt)) {
        return NULL;
    }

    size = _PySys_GetSizeOf(o);

    if (size == (size_t)-1 && _PyErr_Occurred(tstate)) {
        /* Has a default value been given */
        if (dflt != NULL && _PyErr_ExceptionMatches(tstate, PyExc_TypeError)) {
            _PyErr_Clear(tstate);
            Py_INCREF(dflt);
            return dflt;
        }
        else
            return NULL;
    }

    return PyLong_FromSize_t(size);
}

PyDoc_STRVAR(getsizeof_doc,
"getsizeof(object [, default]) -> int\n\
\n\
Return the size of object in bytes.");

/*[clinic input]
sys.getrefcount -> Py_ssize_t

    object:  object
    /

Return the reference count of object.

The count returned is generally one higher than you might expect,
because it includes the (temporary) reference as an argument to
getrefcount().
[clinic start generated code]*/

static Py_ssize_t
sys_getrefcount_impl(PyObject *module, PyObject *object)
/*[clinic end generated code: output=5fd477f2264b85b2 input=bf474efd50a21535]*/
{
    return Py_REFCNT(object);
}

/*[clinic input]
sys.getallocatedblocks -> Py_ssize_t

Return the number of memory blocks currently allocated.
[clinic start generated code]*/

static Py_ssize_t
sys_getallocatedblocks_impl(PyObject *module)
/*[clinic end generated code: output=f0c4e873f0b6dcf7 input=dab13ee346a0673e]*/
{
    return _Py_GetAllocatedBlocks();
}


/*[clinic input]
sys._getframe

    depth: int = 0
    /

Return a frame object from the call stack.

If optional integer depth is given, return the frame object that many
calls below the top of the stack.  If that is deeper than the call
stack, ValueError is raised.  The default for depth is zero, returning
the frame at the top of the call stack.

This function should be used for internal and specialized purposes
only.
[clinic start generated code]*/

static PyObject *
sys__getframe_impl(PyObject *module, int depth)
/*[clinic end generated code: output=d438776c04d59804 input=c1be8a6464b11ee5]*/
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyFrameObject *f = PyThreadState_GetFrame(tstate);

    while (depth > 0 && f != NULL) {
        PyFrameObject *back = PyFrame_GetBack(f);
        Py_DECREF(f);
        f = back;
        --depth;
    }
    if (f == NULL) {
        _PyErr_SetString(tstate, PyExc_ValueError,
                         "call stack is not deep enough");
        return NULL;
    }
    return (PyObject*)f;
}

/*[clinic input]
sys._current_frames

Return a dict mapping each thread's thread id to its current stack frame.

This function should be used for specialized purposes only.
[clinic start generated code]*/

static PyObject *
sys__current_frames_impl(PyObject *module)
/*[clinic end generated code: output=d2a41ac0a0a3809a input=2a9049c5f5033691]*/
{
    return _PyThread_CurrentFrames();
}

/*[clinic input]
sys.call_tracing

    func: object
    args as funcargs: object(subclass_of='&PyTuple_Type')
    /

Call func(*args), while tracing is enabled.

The tracing state is saved, and restored afterwards.  This is intended
to be called from a debugger from a checkpoint, to recursively debug
some other code.
[clinic start generated code]*/

static PyObject *
sys_call_tracing_impl(PyObject *module, PyObject *func, PyObject *funcargs)
/*[clinic end generated code: output=7e4999853cd4e5a6 input=5102e8b11049f92f]*/
{
    return _PyEval_CallTracing(func, funcargs);
}


#ifdef __cplusplus
extern "C" {
#endif

/*[clinic input]
sys._debugmallocstats

Print summary info to stderr about the state of pymalloc's structures.

In Py_DEBUG mode, also perform some expensive internal consistency
checks.
[clinic start generated code]*/

static PyObject *
sys__debugmallocstats_impl(PyObject *module)
/*[clinic end generated code: output=ec3565f8c7cee46a input=33c0c9c416f98424]*/
{
#ifdef WITH_PYMALLOC
    if (_PyObject_DebugMallocStats(stderr)) {
        fputc('\n', stderr);
    }
#endif
    _PyObject_DebugTypeStats(stderr);

    Py_RETURN_NONE;
}

#ifdef DYNAMIC_EXECUTION_PROFILE
/* Defined in ceval.c because it uses static globals if that file */
extern PyObject *_Py_GetDXProfile(PyObject *,  PyObject *);
#endif

#ifdef __cplusplus
}
#endif


/*[clinic input]
sys._clear_type_cache

Clear the internal type lookup cache.
[clinic start generated code]*/

static PyObject *
sys__clear_type_cache_impl(PyObject *module)
/*[clinic end generated code: output=20e48ca54a6f6971 input=127f3e04a8d9b555]*/
{
    PyType_ClearCache();
    Py_RETURN_NONE;
}

/*[clinic input]
sys.is_finalizing

Return True if Python is exiting.
[clinic start generated code]*/

static PyObject *
sys_is_finalizing_impl(PyObject *module)
/*[clinic end generated code: output=735b5ff7962ab281 input=f0df747a039948a5]*/
{
    return PyBool_FromLong(_Py_IsFinalizing());
}

#ifdef ANDROID_API_LEVEL
/*[clinic input]
sys.getandroidapilevel

Return the build time API version of Android as an integer.
[clinic start generated code]*/

static PyObject *
sys_getandroidapilevel_impl(PyObject *module)
/*[clinic end generated code: output=214abf183a1c70c1 input=3e6d6c9fcdd24ac6]*/
{
    return PyLong_FromLong(ANDROID_API_LEVEL);
}
#endif   /* ANDROID_API_LEVEL */



static PyMethodDef sys_methods[] = {
    /* Might as well keep this in alphabetic order */
    {"breakpointhook",  (PyCFunction)(void(*)(void))sys_breakpointhook,
     METH_FASTCALL | METH_KEYWORDS, breakpointhook_doc},
    SYS__CLEAR_TYPE_CACHE_METHODDEF
    SYS__CURRENT_FRAMES_METHODDEF
    SYS_DISPLAYHOOK_METHODDEF
    SYS_EXC_INFO_METHODDEF
    SYS_EXCEPTHOOK_METHODDEF
    SYS_EXIT_METHODDEF
    SYS_GETDEFAULTENCODING_METHODDEF
    SYS_GETDLOPENFLAGS_METHODDEF
    SYS_GETALLOCATEDBLOCKS_METHODDEF
#ifdef DYNAMIC_EXECUTION_PROFILE
    {"getdxp",          _Py_GetDXProfile, METH_VARARGS},
#endif
    SYS_GETFILESYSTEMENCODING_METHODDEF
    SYS_GETFILESYSTEMENCODEERRORS_METHODDEF
    SYS_GETTOTALREFCOUNT_METHODDEF
    SYS_GETREFCOUNT_METHODDEF
    SYS_GETRECURSIONLIMIT_METHODDEF
    {"getsizeof",   (PyCFunction)(void(*)(void))sys_getsizeof,
     METH_VARARGS | METH_KEYWORDS, getsizeof_doc},
    SYS__GETFRAME_METHODDEF
    SYS_GETWINDOWSVERSION_METHODDEF
    SYS__ENABLELEGACYWINDOWSFSENCODING_METHODDEF
    SYS_INTERN_METHODDEF
    SYS_IS_FINALIZING_METHODDEF
    SYS_MDEBUG_METHODDEF
    SYS_SETDLOPENFLAGS_METHODDEF
    {"setprofile",      sys_setprofile, METH_O, setprofile_doc},
    SYS_GETPROFILE_METHODDEF
    SYS_SETRECURSIONLIMIT_METHODDEF
    {"settrace",        sys_settrace, METH_O, settrace_doc},
    SYS_GETTRACE_METHODDEF
    SYS_CALL_TRACING_METHODDEF
    SYS__DEBUGMALLOCSTATS_METHODDEF
    SYS_SET_COROUTINE_ORIGIN_TRACKING_DEPTH_METHODDEF
    SYS_GET_COROUTINE_ORIGIN_TRACKING_DEPTH_METHODDEF
    {"set_asyncgen_hooks", (PyCFunction)(void(*)(void))sys_set_asyncgen_hooks,
     METH_VARARGS | METH_KEYWORDS, set_asyncgen_hooks_doc},
    SYS_GET_ASYNCGEN_HOOKS_METHODDEF
    SYS_GETANDROIDAPILEVEL_METHODDEF
    SYS_UNRAISABLEHOOK_METHODDEF
    {NULL,              NULL}           /* sentinel */
};

static PyObject *
list_builtin_module_names(void)
{
    PyObject *list = PyList_New(0);
    int i;
    if (list == NULL)
        return NULL;
    for (i = 0; PyImport_Inittab[i].name != NULL; i++) {
        PyObject *name = PyUnicode_FromString(
            PyImport_Inittab[i].name);
        if (name == NULL)
            break;
        PyList_Append(list, name);
        Py_DECREF(name);
    }
    if (PyList_Sort(list) != 0) {
        Py_DECREF(list);
        list = NULL;
    }
    if (list) {
        PyObject *v = PyList_AsTuple(list);
        Py_DECREF(list);
        list = v;
    }
    return list;
}

/* Pre-initialization support for sys.warnoptions and sys._xoptions
 *
 * Modern internal code paths:
 *   These APIs get called after _Py_InitializeCore and get to use the
 *   regular CPython list, dict, and unicode APIs.
 *
 * Legacy embedding code paths:
 *   The multi-phase initialization API isn't public yet, so embedding
 *   apps still need to be able configure sys.warnoptions and sys._xoptions
 *   before they call Py_Initialize. To support this, we stash copies of
 *   the supplied wchar * sequences in linked lists, and then migrate the
 *   contents of those lists to the sys module in _PyInitializeCore.
 *
 */

struct _preinit_entry {
    wchar_t *value;
    struct _preinit_entry *next;
};

typedef struct _preinit_entry *_Py_PreInitEntry;

static _Py_PreInitEntry _preinit_warnoptions = NULL;
static _Py_PreInitEntry _preinit_xoptions = NULL;

static _Py_PreInitEntry
_alloc_preinit_entry(const wchar_t *value)
{
    /* To get this to work, we have to initialize the runtime implicitly */
    _PyRuntime_Initialize();

    /* Force default allocator, so we can ensure that it also gets used to
     * destroy the linked list in _clear_preinit_entries.
     */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    _Py_PreInitEntry node = PyMem_RawCalloc(1, sizeof(*node));
    if (node != NULL) {
        node->value = _PyMem_RawWcsdup(value);
        if (node->value == NULL) {
            PyMem_RawFree(node);
            node = NULL;
        };
    };

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
    return node;
}

static int
_append_preinit_entry(_Py_PreInitEntry *optionlist, const wchar_t *value)
{
    _Py_PreInitEntry new_entry = _alloc_preinit_entry(value);
    if (new_entry == NULL) {
        return -1;
    }
    /* We maintain the linked list in this order so it's easy to play back
     * the add commands in the same order later on in _Py_InitializeCore
     */
    _Py_PreInitEntry last_entry = *optionlist;
    if (last_entry == NULL) {
        *optionlist = new_entry;
    } else {
        while (last_entry->next != NULL) {
            last_entry = last_entry->next;
        }
        last_entry->next = new_entry;
    }
    return 0;
}

static void
_clear_preinit_entries(_Py_PreInitEntry *optionlist)
{
    _Py_PreInitEntry current = *optionlist;
    *optionlist = NULL;
    /* Deallocate the nodes and their contents using the default allocator */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
    while (current != NULL) {
        _Py_PreInitEntry next = current->next;
        PyMem_RawFree(current->value);
        PyMem_RawFree(current);
        current = next;
    }
    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
}


PyStatus
_PySys_ReadPreinitWarnOptions(PyWideStringList *options)
{
    PyStatus status;
    _Py_PreInitEntry entry;

    for (entry = _preinit_warnoptions; entry != NULL; entry = entry->next) {
        status = PyWideStringList_Append(options, entry->value);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    _clear_preinit_entries(&_preinit_warnoptions);
    return _PyStatus_OK();
}


PyStatus
_PySys_ReadPreinitXOptions(PyConfig *config)
{
    PyStatus status;
    _Py_PreInitEntry entry;

    for (entry = _preinit_xoptions; entry != NULL; entry = entry->next) {
        status = PyWideStringList_Append(&config->xoptions, entry->value);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    _clear_preinit_entries(&_preinit_xoptions);
    return _PyStatus_OK();
}


static PyObject *
get_warnoptions(PyThreadState *tstate)
{
    PyObject *warnoptions = sys_get_object_id(tstate, &PyId_warnoptions);
    if (warnoptions == NULL || !PyList_Check(warnoptions)) {
        /* PEP432 TODO: we can reach this if warnoptions is NULL in the main
        *  interpreter config. When that happens, we need to properly set
         * the `warnoptions` reference in the main interpreter config as well.
         *
         * For Python 3.7, we shouldn't be able to get here due to the
         * combination of how _PyMainInterpreter_ReadConfig and _PySys_EndInit
         * work, but we expect 3.8+ to make the _PyMainInterpreter_ReadConfig
         * call optional for embedding applications, thus making this
         * reachable again.
         */
        warnoptions = PyList_New(0);
        if (warnoptions == NULL) {
            return NULL;
        }
        if (sys_set_object_id(tstate, &PyId_warnoptions, warnoptions)) {
            Py_DECREF(warnoptions);
            return NULL;
        }
        Py_DECREF(warnoptions);
    }
    return warnoptions;
}

void
PySys_ResetWarnOptions(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    if (tstate == NULL) {
        _clear_preinit_entries(&_preinit_warnoptions);
        return;
    }

    PyObject *warnoptions = sys_get_object_id(tstate, &PyId_warnoptions);
    if (warnoptions == NULL || !PyList_Check(warnoptions))
        return;
    PyList_SetSlice(warnoptions, 0, PyList_GET_SIZE(warnoptions), NULL);
}

static int
_PySys_AddWarnOptionWithError(PyThreadState *tstate, PyObject *option)
{
    PyObject *warnoptions = get_warnoptions(tstate);
    if (warnoptions == NULL) {
        return -1;
    }
    if (PyList_Append(warnoptions, option)) {
        return -1;
    }
    return 0;
}

void
PySys_AddWarnOptionUnicode(PyObject *option)
{
    PyThreadState *tstate = _PyThreadState_GET();
    if (_PySys_AddWarnOptionWithError(tstate, option) < 0) {
        /* No return value, therefore clear error state if possible */
        if (tstate) {
            _PyErr_Clear(tstate);
        }
    }
}

void
PySys_AddWarnOption(const wchar_t *s)
{
    PyThreadState *tstate = _PyThreadState_GET();
    if (tstate == NULL) {
        _append_preinit_entry(&_preinit_warnoptions, s);
        return;
    }
    PyObject *unicode;
    unicode = PyUnicode_FromWideChar(s, -1);
    if (unicode == NULL)
        return;
    PySys_AddWarnOptionUnicode(unicode);
    Py_DECREF(unicode);
}

int
PySys_HasWarnOptions(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *warnoptions = sys_get_object_id(tstate, &PyId_warnoptions);
    return (warnoptions != NULL && PyList_Check(warnoptions)
            && PyList_GET_SIZE(warnoptions) > 0);
}

static PyObject *
get_xoptions(PyThreadState *tstate)
{
    PyObject *xoptions = sys_get_object_id(tstate, &PyId__xoptions);
    if (xoptions == NULL || !PyDict_Check(xoptions)) {
        /* PEP432 TODO: we can reach this if xoptions is NULL in the main
        *  interpreter config. When that happens, we need to properly set
         * the `xoptions` reference in the main interpreter config as well.
         *
         * For Python 3.7, we shouldn't be able to get here due to the
         * combination of how _PyMainInterpreter_ReadConfig and _PySys_EndInit
         * work, but we expect 3.8+ to make the _PyMainInterpreter_ReadConfig
         * call optional for embedding applications, thus making this
         * reachable again.
         */
        xoptions = PyDict_New();
        if (xoptions == NULL) {
            return NULL;
        }
        if (sys_set_object_id(tstate, &PyId__xoptions, xoptions)) {
            Py_DECREF(xoptions);
            return NULL;
        }
        Py_DECREF(xoptions);
    }
    return xoptions;
}

static int
_PySys_AddXOptionWithError(const wchar_t *s)
{
    PyObject *name = NULL, *value = NULL;

    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *opts = get_xoptions(tstate);
    if (opts == NULL) {
        goto error;
    }

    const wchar_t *name_end = wcschr(s, L'=');
    if (!name_end) {
        name = PyUnicode_FromWideChar(s, -1);
        value = Py_True;
        Py_INCREF(value);
    }
    else {
        name = PyUnicode_FromWideChar(s, name_end - s);
        value = PyUnicode_FromWideChar(name_end + 1, -1);
    }
    if (name == NULL || value == NULL) {
        goto error;
    }
    if (PyDict_SetItem(opts, name, value) < 0) {
        goto error;
    }
    Py_DECREF(name);
    Py_DECREF(value);
    return 0;

error:
    Py_XDECREF(name);
    Py_XDECREF(value);
    return -1;
}

void
PySys_AddXOption(const wchar_t *s)
{
    PyThreadState *tstate = _PyThreadState_GET();
    if (tstate == NULL) {
        _append_preinit_entry(&_preinit_xoptions, s);
        return;
    }
    if (_PySys_AddXOptionWithError(s) < 0) {
        /* No return value, therefore clear error state if possible */
        _PyErr_Clear(tstate);
    }
}

PyObject *
PySys_GetXOptions(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return get_xoptions(tstate);
}

/* XXX This doc string is too long to be a single string literal in VC++ 5.0.
   Two literals concatenated works just fine.  If you have a K&R compiler
   or other abomination that however *does* understand longer strings,
   get rid of the !!! comment in the middle and the quotes that surround it. */
PyDoc_VAR(sys_doc) =
PyDoc_STR(
"This module provides access to some objects used or maintained by the\n\
interpreter and to functions that interact strongly with the interpreter.\n\
\n\
Dynamic objects:\n\
\n\
argv -- command line arguments; argv[0] is the script pathname if known\n\
path -- module search path; path[0] is the script directory, else ''\n\
modules -- dictionary of loaded modules\n\
\n\
displayhook -- called to show results in an interactive session\n\
excepthook -- called to handle any uncaught exception other than SystemExit\n\
  To customize printing in an interactive session or to install a custom\n\
  top-level exception handler, assign other functions to replace these.\n\
\n\
stdin -- standard input file object; used by input()\n\
stdout -- standard output file object; used by print()\n\
stderr -- standard error object; used for error messages\n\
  By assigning other file objects (or objects that behave like files)\n\
  to these, it is possible to redirect all of the interpreter's I/O.\n\
\n\
last_type -- type of last uncaught exception\n\
last_value -- value of last uncaught exception\n\
last_traceback -- traceback of last uncaught exception\n\
  These three are only available in an interactive session after a\n\
  traceback has been printed.\n\
"
)
/* concatenating string here */
PyDoc_STR(
"\n\
Static objects:\n\
\n\
builtin_module_names -- tuple of module names built into this interpreter\n\
copyright -- copyright notice pertaining to this interpreter\n\
exec_prefix -- prefix used to find the machine-specific Python library\n\
executable -- absolute path of the executable binary of the Python interpreter\n\
float_info -- a named tuple with information about the float implementation.\n\
float_repr_style -- string indicating the style of repr() output for floats\n\
hash_info -- a named tuple with information about the hash algorithm.\n\
hexversion -- version information encoded as a single integer\n\
implementation -- Python implementation information.\n\
int_info -- a named tuple with information about the int implementation.\n\
maxsize -- the largest supported length of containers.\n\
maxunicode -- the value of the largest Unicode code point\n\
platform -- platform identifier\n\
prefix -- prefix used to find the Python library\n\
thread_info -- a named tuple with information about the thread implementation.\n\
version -- the version of this interpreter as a string\n\
version_info -- version information as a named tuple\n\
"
)
#ifdef MS_COREDLL
/* concatenating string here */
PyDoc_STR(
"dllhandle -- [Windows only] integer handle of the Python DLL\n\
winver -- [Windows only] version number of the Python DLL\n\
"
)
#endif /* MS_COREDLL */
#ifdef MS_WINDOWS
/* concatenating string here */
PyDoc_STR(
"_enablelegacywindowsfsencoding -- [Windows only]\n\
"
)
#endif
PyDoc_STR(
"__stdin__ -- the original stdin; don't touch!\n\
__stdout__ -- the original stdout; don't touch!\n\
__stderr__ -- the original stderr; don't touch!\n\
__displayhook__ -- the original displayhook; don't touch!\n\
__excepthook__ -- the original excepthook; don't touch!\n\
\n\
Functions:\n\
\n\
displayhook() -- print an object to the screen, and save it in builtins._\n\
excepthook() -- print an exception and its traceback to sys.stderr\n\
exc_info() -- return thread-safe information about the current exception\n\
exit() -- exit the interpreter by raising SystemExit\n\
getdlopenflags() -- returns flags to be used for dlopen() calls\n\
getprofile() -- get the global profiling function\n\
getrefcount() -- return the reference count for an object (plus one :-)\n\
getrecursionlimit() -- return the max recursion depth for the interpreter\n\
getsizeof() -- return the size of an object in bytes\n\
gettrace() -- get the global debug tracing function\n\
setdlopenflags() -- set the flags to be used for dlopen() calls\n\
setprofile() -- set the global profiling function\n\
setrecursionlimit() -- set the max recursion depth for the interpreter\n\
settrace() -- set the global debug tracing function\n\
"
)
/* end of sys_doc */ ;


PyDoc_STRVAR(flags__doc__,
"sys.flags\n\
\n\
Flags provided through command line arguments or environment vars.");

static PyTypeObject FlagsType;

static PyStructSequence_Field flags_fields[] = {
    {"debug",                   "-d"},
    {"inspect",                 "-i"},
    {"interactive",             "-i"},
    /*    {"optimize",                "-O or -OO"},*/
    {"dont_write_bytecode",     "-B"},
    {"no_user_site",            "-s"},
    {"no_site",                 "-S"},
    {"ignore_environment",      "-E"},
    {"verbose",                 "-v"},
    /* {"unbuffered",                   "-u"}, */
    /* {"skip_first",                   "-x"}, */
    {"bytes_warning",           "-b"},
    {"quiet",                   "-q"},
    {"hash_randomization",      "-R"},
    {"isolated",                "-I"},
    {"dev_mode",                "-X dev"},
    {"utf8_mode",               "-X utf8"},
    {0}
};

static PyStructSequence_Desc flags_desc = {
    "sys.flags",        /* name */
    flags__doc__,       /* doc */
    flags_fields,       /* fields */
    14
};

static PyObject*
make_flags(PyThreadState *tstate)
{
    PyInterpreterState *interp = tstate->interp;
    const PyPreConfig *preconfig = &interp->runtime->preconfig;
    const PyConfig *config = _PyInterpreterState_GetConfig(interp);

    PyObject *seq = PyStructSequence_New(&FlagsType);
    if (seq == NULL) {
        return NULL;
    }

    int pos = 0;
#define SetFlag(flag) \
    PyStructSequence_SET_ITEM(seq, pos++, PyLong_FromLong(flag))

    SetFlag(config->parser_debug);
    SetFlag(config->inspect);
    SetFlag(config->interactive);
    SetFlag(!config->write_bytecode);
    SetFlag(!config->user_site_directory);
    SetFlag(!config->site_import);
    SetFlag(!config->use_environment);
    SetFlag(config->verbose);
    /* SetFlag(saw_unbuffered_flag); */
    /* SetFlag(skipfirstline); */
    SetFlag(config->bytes_warning);
    SetFlag(config->quiet);
    SetFlag(config->use_hash_seed == 0 || config->hash_seed != 0);
    SetFlag(config->isolated);
    PyStructSequence_SET_ITEM(seq, pos++, PyBool_FromLong(config->dev_mode));
    SetFlag(preconfig->utf8_mode);
#undef SetFlag

    if (_PyErr_Occurred(tstate)) {
        Py_DECREF(seq);
        return NULL;
    }
    return seq;
}

PyDoc_STRVAR(version_info__doc__,
"sys.version_info\n\
\n\
Version information as a named tuple.");

static PyTypeObject VersionInfoType;

static PyStructSequence_Field version_info_fields[] = {
    {"major", "Major release number"},
    {"minor", "Minor release number"},
    {"micro", "Patch release number"},
    {"releaselevel", "'alpha', 'beta', 'candidate', or 'final'"},
    {"serial", "Serial release number"},
    {0}
};

static PyStructSequence_Desc version_info_desc = {
    "sys.version_info",     /* name */
    version_info__doc__,    /* doc */
    version_info_fields,    /* fields */
    5
};

static PyObject *
make_version_info(PyThreadState *tstate)
{
    PyObject *version_info;
    char *s;
    int pos = 0;

    version_info = PyStructSequence_New(&VersionInfoType);
    if (version_info == NULL) {
        return NULL;
    }

    /*
     * These release level checks are mutually exclusive and cover
     * the field, so don't get too fancy with the pre-processor!
     */
#if PY_RELEASE_LEVEL == PY_RELEASE_LEVEL_ALPHA
    s = "alpha";
#elif PY_RELEASE_LEVEL == PY_RELEASE_LEVEL_BETA
    s = "beta";
#elif PY_RELEASE_LEVEL == PY_RELEASE_LEVEL_GAMMA
    s = "candidate";
#elif PY_RELEASE_LEVEL == PY_RELEASE_LEVEL_FINAL
    s = "final";
#endif

#define SetIntItem(flag) \
    PyStructSequence_SET_ITEM(version_info, pos++, PyLong_FromLong(flag))
#define SetStrItem(flag) \
    PyStructSequence_SET_ITEM(version_info, pos++, PyUnicode_FromString(flag))

    SetIntItem(PY_MAJOR_VERSION);
    SetIntItem(PY_MINOR_VERSION);
    SetIntItem(PY_MICRO_VERSION);
    SetStrItem(s);
    SetIntItem(PY_RELEASE_SERIAL);
#undef SetIntItem
#undef SetStrItem

    if (_PyErr_Occurred(tstate)) {
        Py_CLEAR(version_info);
        return NULL;
    }
    return version_info;
}


/* sys.implementation values */
#define NAME "cpython"
const char *_PySys_ImplName = NAME;
#define MAJOR Py_STRINGIFY(PY_MAJOR_VERSION)
#define MINOR Py_STRINGIFY(PY_MINOR_VERSION)
/* #define TAG NAME "-" MAJOR MINOR */
#define TAG NAME
const char *_PySys_ImplCacheTag = TAG;
#undef NAME
#undef MAJOR
#undef MINOR
#undef TAG

#if 0
static PyObject *
make_impl_info(PyObject *version_info)
{
    int res;
    PyObject *impl_info, *value, *ns;

    impl_info = PyDict_New();
    if (impl_info == NULL)
        return NULL;

    /* populate the dict */

    value = PyUnicode_FromString(_PySys_ImplName);
    if (value == NULL)
        goto error;
    res = PyDict_SetItemString(impl_info, "name", value);
    Py_DECREF(value);
    if (res < 0)
        goto error;

    value = PyUnicode_FromString(_PySys_ImplCacheTag);
    if (value == NULL)
        goto error;
    res = PyDict_SetItemString(impl_info, "cache_tag", value);
    Py_DECREF(value);
    if (res < 0)
        goto error;

    res = PyDict_SetItemString(impl_info, "version", version_info);
    if (res < 0)
        goto error;

    value = PyLong_FromLong(PY_VERSION_HEX);
    if (value == NULL)
        goto error;
    res = PyDict_SetItemString(impl_info, "hexversion", value);
    Py_DECREF(value);
    if (res < 0)
        goto error;

#ifdef MULTIARCH
    value = PyUnicode_FromString(MULTIARCH);
    if (value == NULL)
        goto error;
    res = PyDict_SetItemString(impl_info, "_multiarch", value);
    Py_DECREF(value);
    if (res < 0)
        goto error;
#endif

    /* dict ready */

    ns = _PyNamespace_New(impl_info);
    Py_DECREF(impl_info);
    return ns;

error:
    Py_CLEAR(impl_info);
    return NULL;
}

#endif

static struct PyModuleDef sysmodule = {
    PyModuleDef_HEAD_INIT,
    "sys",
    sys_doc,
    -1, /* multiple "initialization" just copies the module dict. */
    sys_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

/* Updating the sys namespace, returning NULL pointer on error */
#define SET_SYS_FROM_STRING_BORROW(key, value)             \
    do {                                                   \
        PyObject *v = (value);                             \
        if (v == NULL) {                                   \
            goto err_occurred;                             \
        }                                                  \
        res = PyDict_SetItemString(sysdict, key, v);       \
        if (res < 0) {                                     \
            goto err_occurred;                             \
        }                                                  \
    } while (0)
#define SET_SYS_FROM_STRING(key, value)                    \
    do {                                                   \
        PyObject *v = (value);                             \
        if (v == NULL) {                                   \
            goto err_occurred;                             \
        }                                                  \
        res = PyDict_SetItemString(sysdict, key, v);       \
        Py_DECREF(v);                                      \
        if (res < 0) {                                     \
            goto err_occurred;                             \
        }                                                  \
    } while (0)

static PyStatus
_PySys_InitCore(PyThreadState *tstate, PyObject *sysdict)
{
    PyObject *version_info;
    int res;

    /* stdin/stdout/stderr are set in pylifecycle.c */

    SET_SYS_FROM_STRING_BORROW("__displayhook__",
                               PyDict_GetItemString(sysdict, "displayhook"));
    SET_SYS_FROM_STRING_BORROW("__excepthook__",
                               PyDict_GetItemString(sysdict, "excepthook"));
    SET_SYS_FROM_STRING_BORROW(
        "__breakpointhook__",
        PyDict_GetItemString(sysdict, "breakpointhook"));
    SET_SYS_FROM_STRING_BORROW("__unraisablehook__",
                               PyDict_GetItemString(sysdict, "unraisablehook"));

    SET_SYS_FROM_STRING("version",
                         PyUnicode_FromString(Py_GetVersion()));
    SET_SYS_FROM_STRING("hexversion",
                         PyLong_FromLong(PY_VERSION_HEX));
    SET_SYS_FROM_STRING("_git",
                        Py_BuildValue("(szz)", "CPython", _Py_gitidentifier(),
                                      _Py_gitversion()));
    SET_SYS_FROM_STRING("_framework", PyUnicode_FromString(_PYTHONFRAMEWORK));
    SET_SYS_FROM_STRING("api_version",
                        PyLong_FromLong(PYTHON_API_VERSION));
    SET_SYS_FROM_STRING("copyright",
                        PyUnicode_FromString(Py_GetCopyright()));
    SET_SYS_FROM_STRING("platform",
                        PyUnicode_FromString(Py_GetPlatform()));
    SET_SYS_FROM_STRING("maxsize",
                        PyLong_FromSsize_t(PY_SSIZE_T_MAX));
    SET_SYS_FROM_STRING("float_info",
                        PyFloat_GetInfo());
    SET_SYS_FROM_STRING("int_info",
                        PyLong_GetInfo());
    /* initialize hash_info */
    if (Hash_InfoType.tp_name == NULL) {
        if (PyStructSequence_InitType2(&Hash_InfoType, &hash_info_desc) < 0) {
            goto type_init_failed;
        }
    }
    SET_SYS_FROM_STRING("hash_info",
                        get_hash_info(tstate));
    SET_SYS_FROM_STRING("maxunicode",
                        PyLong_FromLong(0x10FFFF));
    SET_SYS_FROM_STRING("builtin_module_names",
                        list_builtin_module_names());
#if PY_BIG_ENDIAN
    SET_SYS_FROM_STRING("byteorder",
                        PyUnicode_FromString("big"));
#else
    SET_SYS_FROM_STRING("byteorder",
                        PyUnicode_FromString("little"));
#endif

#ifdef MS_COREDLL
    SET_SYS_FROM_STRING("dllhandle",
                        PyLong_FromVoidPtr(PyWin_DLLhModule));
    SET_SYS_FROM_STRING("winver",
                        PyUnicode_FromString(PyWin_DLLVersionString));
#endif
#ifdef ABIFLAGS
    SET_SYS_FROM_STRING("abiflags",
                        PyUnicode_FromString(ABIFLAGS));
#endif

    /* version_info */
    if (VersionInfoType.tp_name == NULL) {
        if (PyStructSequence_InitType2(&VersionInfoType,
                                       &version_info_desc) < 0) {
            goto type_init_failed;
        }
    }
    version_info = make_version_info(tstate);
    SET_SYS_FROM_STRING("version_info", version_info);
    /* prevent user from creating new instances */
    VersionInfoType.tp_init = NULL;
    VersionInfoType.tp_new = NULL;
    res = PyDict_DelItemString(VersionInfoType.tp_dict, "__new__");
    if (res < 0 && _PyErr_ExceptionMatches(tstate, PyExc_KeyError)) {
        _PyErr_Clear(tstate);
    }

    /* implementation */
    /*
    SET_SYS_FROM_STRING("implementation", make_impl_info(version_info));
    */
    /* flags */
    if (FlagsType.tp_name == 0) {
        if (PyStructSequence_InitType2(&FlagsType, &flags_desc) < 0) {
            goto type_init_failed;
        }
    }
    /* Set flags to their default values (updated by _PySys_InitMain()) */
    SET_SYS_FROM_STRING("flags", make_flags(tstate));

#if defined(MS_WINDOWS)
    /* getwindowsversion */
    if (WindowsVersionType.tp_name == 0)
        if (PyStructSequence_InitType2(&WindowsVersionType,
                                       &windows_version_desc) < 0) {
            goto type_init_failed;
        }
    /* prevent user from creating new instances */
    WindowsVersionType.tp_init = NULL;
    WindowsVersionType.tp_new = NULL;
    assert(!_PyErr_Occurred(tstate));
    res = PyDict_DelItemString(WindowsVersionType.tp_dict, "__new__");
    if (res < 0 && _PyErr_ExceptionMatches(tstate, PyExc_KeyError)) {
        _PyErr_Clear(tstate);
    }
#endif

    /* float repr style: 0.03 (short) vs 0.029999999999999999 (legacy) */
#ifndef PY_NO_SHORT_FLOAT_REPR
    SET_SYS_FROM_STRING("float_repr_style",
                        PyUnicode_FromString("short"));
#else
    SET_SYS_FROM_STRING("float_repr_style",
                        PyUnicode_FromString("legacy"));
#endif

    SET_SYS_FROM_STRING("thread_info", PyThread_GetInfo());

    /* initialize asyncgen_hooks */
    if (AsyncGenHooksType.tp_name == NULL) {
        if (PyStructSequence_InitType2(
                &AsyncGenHooksType, &asyncgen_hooks_desc) < 0) {
            goto type_init_failed;
        }
    }

    if (_PyErr_Occurred(tstate)) {
        goto err_occurred;
    }
    return _PyStatus_OK();

type_init_failed:
    return _PyStatus_ERR("failed to initialize a type");

err_occurred:
    return _PyStatus_ERR("can't initialize sys module");
}

/* Updating the sys namespace, returning integer error codes */
#define SET_SYS_FROM_STRING_INT_RESULT(key, value)         \
    do {                                                   \
        PyObject *v = (value);                             \
        if (v == NULL)                                     \
            return -1;                                     \
        res = PyDict_SetItemString(sysdict, key, v);       \
        Py_DECREF(v);                                      \
        if (res < 0) {                                     \
            return res;                                    \
        }                                                  \
    } while (0)


static int
sys_add_xoption(PyObject *opts, const wchar_t *s)
{
    PyObject *name, *value;

    const wchar_t *name_end = wcschr(s, L'=');
    if (!name_end) {
        name = PyUnicode_FromWideChar(s, -1);
        value = Py_True;
        Py_INCREF(value);
    }
    else {
        name = PyUnicode_FromWideChar(s, name_end - s);
        value = PyUnicode_FromWideChar(name_end + 1, -1);
    }
    if (name == NULL || value == NULL) {
        goto error;
    }
    if (PyDict_SetItem(opts, name, value) < 0) {
        goto error;
    }
    Py_DECREF(name);
    Py_DECREF(value);
    return 0;

error:
    Py_XDECREF(name);
    Py_XDECREF(value);
    return -1;
}


static PyObject*
sys_create_xoptions_dict(const PyConfig *config)
{
    Py_ssize_t nxoption = config->xoptions.length;
    wchar_t * const * xoptions = config->xoptions.items;
    PyObject *dict = PyDict_New();
    if (dict == NULL) {
        return NULL;
    }

    for (Py_ssize_t i=0; i < nxoption; i++) {
        const wchar_t *option = xoptions[i];
        if (sys_add_xoption(dict, option) < 0) {
            Py_DECREF(dict);
            return NULL;
        }
    }

    return dict;
}


int
_PySys_InitMain(PyThreadState *tstate)
{
    PyObject *sysdict = tstate->interp->sysdict;
    const PyConfig *config = _PyInterpreterState_GetConfig(tstate->interp);
    int res;

#define COPY_LIST(KEY, VALUE) \
    do { \
        PyObject *list = _PyWideStringList_AsList(&(VALUE)); \
        if (list == NULL) { \
            return -1; \
        } \
        SET_SYS_FROM_STRING_BORROW(KEY, list); \
        Py_DECREF(list); \
    } while (0)

#define SET_SYS_FROM_WSTR(KEY, VALUE) \
    do { \
        PyObject *str = PyUnicode_FromWideChar(VALUE, -1); \
        if (str == NULL) { \
            return -1; \
        } \
        SET_SYS_FROM_STRING_BORROW(KEY, str); \
        Py_DECREF(str); \
    } while (0)

    COPY_LIST("path", config->module_search_paths);

    SET_SYS_FROM_WSTR("executable", config->executable);
    SET_SYS_FROM_WSTR("_base_executable", config->base_executable);
    SET_SYS_FROM_WSTR("prefix", config->prefix);
    SET_SYS_FROM_WSTR("base_prefix", config->base_prefix);
    SET_SYS_FROM_WSTR("exec_prefix", config->exec_prefix);
    SET_SYS_FROM_WSTR("base_exec_prefix", config->base_exec_prefix);
    SET_SYS_FROM_WSTR("platlibdir", config->platlibdir);

    if (config->pycache_prefix != NULL) {
        SET_SYS_FROM_WSTR("pycache_prefix", config->pycache_prefix);
    } else {
        PyDict_SetItemString(sysdict, "pycache_prefix", Py_None);
    }

    COPY_LIST("argv", config->argv);
    COPY_LIST("warnoptions", config->warnoptions);

    PyObject *xoptions = sys_create_xoptions_dict(config);
    if (xoptions == NULL) {
        return -1;
    }
    SET_SYS_FROM_STRING_BORROW("_xoptions", xoptions);
    Py_DECREF(xoptions);

#undef COPY_LIST
#undef SET_SYS_FROM_WSTR


    /* Set flags to their final values */
    SET_SYS_FROM_STRING_INT_RESULT("flags", make_flags(tstate));
    /* prevent user from creating new instances */
    FlagsType.tp_init = NULL;
    FlagsType.tp_new = NULL;
    res = PyDict_DelItemString(FlagsType.tp_dict, "__new__");
    if (res < 0) {
        if (!_PyErr_ExceptionMatches(tstate, PyExc_KeyError)) {
            return res;
        }
        _PyErr_Clear(tstate);
    }

    SET_SYS_FROM_STRING_INT_RESULT("dont_write_bytecode",
                         PyBool_FromLong(!config->write_bytecode));

    if (get_warnoptions(tstate) == NULL) {
        return -1;
    }

    if (get_xoptions(tstate) == NULL)
        return -1;

    if (_PyErr_Occurred(tstate)) {
        goto err_occurred;
    }

    return 0;

err_occurred:
    return -1;
}

#undef SET_SYS_FROM_STRING
#undef SET_SYS_FROM_STRING_BORROW
#undef SET_SYS_FROM_STRING_INT_RESULT


/* Set up a preliminary stderr printer until we have enough
   infrastructure for the io module in place.

   Use UTF-8/surrogateescape and ignore EAGAIN errors. */
static PyStatus
_PySys_SetPreliminaryStderr(PyObject *sysdict)
{
    PyObject *pstderr = PyFile_NewStdPrinter(fileno(stderr));
    if (pstderr == NULL) {
        goto error;
    }
    if (_PyDict_SetItemId(sysdict, &PyId_stderr, pstderr) < 0) {
        goto error;
    }
    if (PyDict_SetItemString(sysdict, "__stderr__", pstderr) < 0) {
        goto error;
    }
    Py_DECREF(pstderr);
    return _PyStatus_OK();

error:
    Py_XDECREF(pstderr);
    return _PyStatus_ERR("can't set preliminary stderr");
}


/* Create sys module without all attributes: _PySys_InitMain() should be called
   later to add remaining attributes. */
PyStatus
_PySys_Create(PyThreadState *tstate, PyObject **sysmod_p)
{
    assert(!_PyErr_Occurred(tstate));

    PyInterpreterState *interp = tstate->interp;

    PyObject *modules = PyDict_New();
    if (modules == NULL) {
        goto error;
    }
    interp->modules = modules;

    PyObject *sysmod = _PyModule_CreateInitialized(&sysmodule, PYTHON_API_VERSION);
    if (sysmod == NULL) {
        return _PyStatus_ERR("failed to create a module object");
    }

    PyObject *sysdict = PyModule_GetDict(sysmod);
    if (sysdict == NULL) {
        goto error;
    }
    Py_INCREF(sysdict);
    interp->sysdict = sysdict;

    if (PyDict_SetItemString(sysdict, "modules", interp->modules) < 0) {
        goto error;
    }

    PyStatus status = _PySys_SetPreliminaryStderr(sysdict);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    status = _PySys_InitCore(tstate, sysdict);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    if (_PyImport_FixupBuiltin(sysmod, "sys", interp->modules) < 0) {
        goto error;
    }

    assert(!_PyErr_Occurred(tstate));

    *sysmod_p = sysmod;
    return _PyStatus_OK();

error:
    return _PyStatus_ERR("can't initialize sys module");
}


static PyObject *
makepathobject(const wchar_t *path, wchar_t delim)
{
    int i, n;
    const wchar_t *p;
    PyObject *v, *w;

    n = 1;
    p = path;
    while ((p = wcschr(p, delim)) != NULL) {
        n++;
        p++;
    }
    v = PyList_New(n);
    if (v == NULL)
        return NULL;
    for (i = 0; ; i++) {
        p = wcschr(path, delim);
        if (p == NULL)
            p = path + wcslen(path); /* End of string */
        w = PyUnicode_FromWideChar(path, (Py_ssize_t)(p - path));
        if (w == NULL) {
            Py_DECREF(v);
            return NULL;
        }
        PyList_SET_ITEM(v, i, w);
        if (*p == '\0')
            break;
        path = p+1;
    }
    return v;
}

void
PySys_SetPath(const wchar_t *path)
{
    PyObject *v;
    if ((v = makepathobject(path, DELIM)) == NULL)
        Py_FatalError("can't create sys.path");
    PyThreadState *tstate = _PyThreadState_GET();
    if (sys_set_object_id(tstate, &PyId_path, v) != 0) {
        Py_FatalError("can't assign sys.path");
    }
    Py_DECREF(v);
}

static PyObject *
make_sys_argv(int argc, wchar_t * const * argv)
{
    PyObject *list = PyList_New(argc);
    if (list == NULL) {
        return NULL;
    }

    for (Py_ssize_t i = 0; i < argc; i++) {
        PyObject *v = PyUnicode_FromWideChar(argv[i], -1);
        if (v == NULL) {
            Py_DECREF(list);
            return NULL;
        }
        PyList_SET_ITEM(list, i, v);
    }
    return list;
}

void
PySys_SetArgvEx(int argc, wchar_t **argv, int updatepath)
{
    wchar_t* empty_argv[1] = {L""};
    PyThreadState *tstate = _PyThreadState_GET();

    if (argc < 1 || argv == NULL) {
        /* Ensure at least one (empty) argument is seen */
        argv = empty_argv;
        argc = 1;
    }

    PyObject *av = make_sys_argv(argc, argv);
    if (av == NULL) {
        Py_FatalError("no mem for sys.argv");
    }
    if (sys_set_object(tstate, "argv", av) != 0) {
        Py_DECREF(av);
        Py_FatalError("can't assign sys.argv");
    }
    Py_DECREF(av);

    if (updatepath) {
        /* If argv[0] is not '-c' nor '-m', prepend argv[0] to sys.path.
           If argv[0] is a symlink, use the real path. */
        const PyWideStringList argv_list = {.length = argc, .items = argv};
        PyObject *path0 = NULL;
        if (_PyPathConfig_ComputeSysPath0(&argv_list, &path0)) {
            if (path0 == NULL) {
                Py_FatalError("can't compute path0 from argv");
            }

            PyObject *sys_path = sys_get_object_id(tstate, &PyId_path);
            if (sys_path != NULL) {
                if (PyList_Insert(sys_path, 0, path0) < 0) {
                    Py_DECREF(path0);
                    Py_FatalError("can't prepend path0 to sys.path");
                }
            }
            Py_DECREF(path0);
        }
    }
}

void
PySys_SetArgv(int argc, wchar_t **argv)
{
    PySys_SetArgvEx(argc, argv, Py_IsolatedFlag == 0);
}

/* Reimplementation of PyFile_WriteString() no calling indirectly
   PyErr_CheckSignals(): avoid the call to PyObject_Str(). */

static int
sys_pyfile_write_unicode(PyObject *unicode, PyObject *file)
{
    if (file == NULL)
        return -1;
    assert(unicode != NULL);
    PyObject *result = _PyObject_CallMethodIdOneArg(file, &PyId_write, unicode);
    if (result == NULL) {
        return -1;
    }
    Py_DECREF(result);
    return 0;
}

static int
sys_pyfile_write(const char *text, PyObject *file)
{
    PyObject *unicode = NULL;
    int err;

    if (file == NULL)
        return -1;

    unicode = PyUnicode_FromString(text);
    if (unicode == NULL)
        return -1;

    err = sys_pyfile_write_unicode(unicode, file);
    Py_DECREF(unicode);
    return err;
}

/* APIs to write to sys.stdout or sys.stderr using a printf-like interface.
   Adapted from code submitted by Just van Rossum.

   PySys_WriteStdout(format, ...)
   PySys_WriteStderr(format, ...)

      The first function writes to sys.stdout; the second to sys.stderr.  When
      there is a problem, they write to the real (C level) stdout or stderr;
      no exceptions are raised.

      PyErr_CheckSignals() is not called to avoid the execution of the Python
      signal handlers: they may raise a new exception whereas sys_write()
      ignores all exceptions.

      Both take a printf-style format string as their first argument followed
      by a variable length argument list determined by the format string.

      *** WARNING ***

      The format should limit the total size of the formatted output string to
      1000 bytes.  In particular, this means that no unrestricted "%s" formats
      should occur; these should be limited using "%.<N>s where <N> is a
      decimal number calculated so that <N> plus the maximum size of other
      formatted text does not exceed 1000 bytes.  Also watch out for "%f",
      which can print hundreds of digits for very large numbers.

 */

static void
sys_write(_Py_Identifier *key, FILE *fp, const char *format, va_list va)
{
    PyObject *file;
    PyObject *error_type, *error_value, *error_traceback;
    char buffer[1001];
    int written;
    PyThreadState *tstate = _PyThreadState_GET();

    _PyErr_Fetch(tstate, &error_type, &error_value, &error_traceback);
    file = sys_get_object_id(tstate, key);
    written = PyOS_vsnprintf(buffer, sizeof(buffer), format, va);
    if (sys_pyfile_write(buffer, file) != 0) {
        _PyErr_Clear(tstate);
        fputs(buffer, fp);
    }
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        const char *truncated = "... truncated";
        if (sys_pyfile_write(truncated, file) != 0)
            fputs(truncated, fp);
    }
    _PyErr_Restore(tstate, error_type, error_value, error_traceback);
}

void
PySys_WriteStdout(const char *format, ...)
{
    va_list va;

    va_start(va, format);
    sys_write(&PyId_stdout, stdout, format, va);
    va_end(va);
}

void
PySys_WriteStderr(const char *format, ...)
{
    va_list va;

    va_start(va, format);
    sys_write(&PyId_stderr, stderr, format, va);
    va_end(va);
}

static void
sys_format(_Py_Identifier *key, FILE *fp, const char *format, va_list va)
{
    PyObject *file, *message;
    PyObject *error_type, *error_value, *error_traceback;
    const char *utf8;
    PyThreadState *tstate = _PyThreadState_GET();

    _PyErr_Fetch(tstate, &error_type, &error_value, &error_traceback);
    file = sys_get_object_id(tstate, key);
    message = PyUnicode_FromFormatV(format, va);
    if (message != NULL) {
        if (sys_pyfile_write_unicode(message, file) != 0) {
            _PyErr_Clear(tstate);
            utf8 = PyUnicode_AsUTF8(message);
            if (utf8 != NULL)
                fputs(utf8, fp);
        }
        Py_DECREF(message);
    }
    _PyErr_Restore(tstate, error_type, error_value, error_traceback);
}

void
PySys_FormatStdout(const char *format, ...)
{
    va_list va;

    va_start(va, format);
    sys_format(&PyId_stdout, stdout, format, va);
    va_end(va);
}

void
PySys_FormatStderr(const char *format, ...)
{
    va_list va;

    va_start(va, format);
    sys_format(&PyId_stderr, stderr, format, va);
    va_end(va);
}
