
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
#include "pycore_pystate.h"       // PyThreadState_Get()
#include "pycore_tupleobject.h"

#include "osdefs.h"               // DELIM
#include <locale.h>

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
    s = arg;
    return_value = sys_intern_impl(module, s);

exit:
    return return_value;
}

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

_Py_IDENTIFIER(_);
_Py_IDENTIFIER(__sizeof__);
_Py_IDENTIFIER(builtins);
_Py_IDENTIFIER(path);
_Py_IDENTIFIER(stdout);
_Py_IDENTIFIER(stderr);
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
    PyThreadState *tstate = PyThreadState_Get();
    return sys_get_object_id(tstate, key);
}

PyObject *
PySys_GetObject(const char *name)
{
    PyThreadState *tstate = PyThreadState_Get();
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
    PyThreadState *tstate = PyThreadState_Get();
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
    PyThreadState *tstate = PyThreadState_Get();
    return sys_set_object(tstate, name, v);
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
    PyThreadState *tstate = PyThreadState_Get();

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
      return NULL;
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
    _PyErr_StackItem *err_info = _PyErr_GetTopmostException(PyThreadState_Get());
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
    PyThreadState *tstate = PyThreadState_Get();
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
    PyThreadState *tstate = PyThreadState_Get();
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

size_t
_PySys_GetSizeOf(PyObject *o)
{
    PyObject *res = NULL;
    PyObject *method;
    Py_ssize_t size;
    PyThreadState *tstate = PyThreadState_Get();

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

    return (size_t)size;
}

static PyObject *
sys_getsizeof(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"object", "default", 0};
    size_t size;
    PyObject *o, *dflt = NULL;
    PyThreadState *tstate = PyThreadState_Get();

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
    PyThreadState *tstate = PyThreadState_Get();
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


#ifdef __cplusplus
extern "C" {
#endif

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
    SYS__CLEAR_TYPE_CACHE_METHODDEF
    SYS_DISPLAYHOOK_METHODDEF
    SYS_EXC_INFO_METHODDEF
    SYS_EXCEPTHOOK_METHODDEF
    SYS_EXIT_METHODDEF
    SYS_GETDEFAULTENCODING_METHODDEF
    SYS_GETREFCOUNT_METHODDEF
    {"getsizeof",   (PyCFunction)(void(*)(void))sys_getsizeof,
     METH_VARARGS | METH_KEYWORDS, getsizeof_doc},
    SYS__GETFRAME_METHODDEF
    SYS_INTERN_METHODDEF
    SYS_IS_FINALIZING_METHODDEF
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
hexversion -- version information encoded as a single integer\n\
implementation -- Python implementation information.\n\
maxsize -- the largest supported length of containers.\n\
maxunicode -- the value of the largest Unicode code point\n\
platform -- platform identifier\n\
prefix -- prefix used to find the Python library\n\
"
)
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
getrefcount() -- return the reference count for an object (plus one :-)\n\
getsizeof() -- return the size of an object in bytes\n\
"
)
/* end of sys_doc */ ;

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
    int res;

    /* stdin/stdout/stderr are set in pylifecycle.c */

    SET_SYS_FROM_STRING_BORROW("__displayhook__",
                               PyDict_GetItemString(sysdict, "displayhook"));
    SET_SYS_FROM_STRING_BORROW("__excepthook__",
                               PyDict_GetItemString(sysdict, "excepthook"));
    SET_SYS_FROM_STRING_BORROW("__unraisablehook__",
                               PyDict_GetItemString(sysdict, "unraisablehook"));
    SET_SYS_FROM_STRING("_git",
                        Py_BuildValue("(szz)", "CPython", _Py_gitidentifier(),
                                      _Py_gitversion()));
    SET_SYS_FROM_STRING("_framework", PyUnicode_FromString(_PYTHONFRAMEWORK));
    SET_SYS_FROM_STRING("api_version",
                        PyLong_FromLong(PYTHON_API_VERSION));
    SET_SYS_FROM_STRING("platform",
                        PyUnicode_FromString(Py_GetPlatform()));
    SET_SYS_FROM_STRING("maxsize",
                        PyLong_FromSsize_t(PY_SSIZE_T_MAX));
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
    if (_PyErr_Occurred(tstate)) {
        goto err_occurred;
    }
    return _PyStatus_OK();

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


int
_PySys_InitMain(PyThreadState *tstate)
{
    PyObject *sysdict = tstate->interp->sysdict;
    const PyConfig *config = _PyInterpreterState_GetConfig(tstate->interp);
    int res;

#define COPY_LIST(KEY, VALUE) \
    do { \
        PyObject *list = _PyStringList_AsList(&(VALUE)); \
        if (list == NULL) { \
            return -1; \
        } \
        SET_SYS_FROM_STRING_BORROW(KEY, list); \
        Py_DECREF(list); \
    } while (0)

#define SET_SYS_FROM_CHAR(KEY, VALUE) \
    do { \
      PyObject *str = PyUnicode_FromString(VALUE);\
        if (str == NULL) { \
            return -1; \
        } \
        SET_SYS_FROM_STRING_BORROW(KEY, str); \
        Py_DECREF(str); \
    } while (0)

    COPY_LIST("path", config->module_search_paths);

    SET_SYS_FROM_CHAR("executable", config->executable);
    SET_SYS_FROM_CHAR("_base_executable", config->base_executable);
    SET_SYS_FROM_CHAR("prefix", config->prefix);
    SET_SYS_FROM_CHAR("base_prefix", config->base_prefix);
    SET_SYS_FROM_CHAR("exec_prefix", config->exec_prefix);
    SET_SYS_FROM_CHAR("base_exec_prefix", config->base_exec_prefix);
    SET_SYS_FROM_CHAR("platlibdir", config->platlibdir);

    COPY_LIST("argv", config->argv);

#undef COPY_LIST
#undef SET_SYS_FROM_WSTR
    
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
makepathobject(const char *path, char delim)
{
    int i, n;
    const char *p;
    PyObject *v, *w;

    n = 1;
    p = path;
    while ((p = strchr(p, delim)) != NULL) {
        n++;
        p++;
    }
    v = PyList_New(n);
    if (v == NULL)
        return NULL;
    for (i = 0; ; i++) {
        p = strchr(path, delim);
        if (p == NULL)
            p = path + strlen(path); /* End of string */
        w = PyUnicode_FromStringAndSize(path, (Py_ssize_t)(p - path));
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
PySys_SetPath(const char *path)
{
    PyObject *v;
    if ((v = makepathobject(path, DELIM)) == NULL)
        Py_FatalError("can't create sys.path");
    PyThreadState *tstate = PyThreadState_Get();
    if (sys_set_object_id(tstate, &PyId_path, v) != 0) {
        Py_FatalError("can't assign sys.path");
    }
    Py_DECREF(v);
}

static PyObject *
make_sys_argv(int argc, char * const * argv)
{
    PyObject *list = PyList_New(argc);
    if (list == NULL) {
        return NULL;
    }

    for (Py_ssize_t i = 0; i < argc; i++) {
      PyObject *v = PyUnicode_FromString(argv[i]);
        if (v == NULL) {
            Py_DECREF(list);
            return NULL;
        }
        PyList_SET_ITEM(list, i, v);
    }
    return list;
}

void
PySys_SetArgvEx(int argc, char **argv, int updatepath)
{
    char* empty_argv[1] = {""};
    PyThreadState *tstate = PyThreadState_Get();

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
        const PyStringList argv_list = {.length = argc, .items = argv};
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
PySys_SetArgv(int argc, char **argv)
{
  PySys_SetArgvEx(argc, argv, 1);
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
    PyThreadState *tstate = PyThreadState_Get();

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
    PyThreadState *tstate = PyThreadState_Get();

    _PyErr_Fetch(tstate, &error_type, &error_value, &error_traceback);
    file = sys_get_object_id(tstate, key);
    message = PyUnicode_FromFormatV(format, va);
    if (message != NULL) {
        if (sys_pyfile_write_unicode(message, file) != 0) {
            _PyErr_Clear(tstate);
            utf8 = PyUnicode_AsChar(message);
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
