
/* Error handling */

#include "Python.h"
#include "pycore_initconfig.h"
#include "pycore_pyerrors.h"
#include "pycore_pystate.h"    // PyThreadState_Get()
#include "pycore_sysmodule.h"
#include "pycore_traceback.h"

#ifndef __STDC__
extern char *strerror(int);
#endif

#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

_Py_IDENTIFIER(__module__);
_Py_IDENTIFIER(builtins);
_Py_IDENTIFIER(stderr);
_Py_IDENTIFIER(flush);

/* Forward declarations */
static PyObject *
_PyErr_FormatV(PyThreadState *tstate, PyObject *exception,
               const char *format, va_list vargs);


void
_PyErr_Restore(PyThreadState *tstate, PyObject *type, PyObject *value,
               PyObject *traceback)
{
    PyObject *oldtype, *oldvalue, *oldtraceback;

    if (traceback != NULL && !PyTraceBack_Check(traceback)) {
        /* XXX Should never happen -- fatal error instead? */
        /* Well, it could be None. */
        Py_DECREF(traceback);
        traceback = NULL;
    }

    /* Save these in locals to safeguard against recursive
       invocation through Py_XDECREF */
    oldtype = tstate->curexc_type;
    oldvalue = tstate->curexc_value;
    oldtraceback = tstate->curexc_traceback;

    tstate->curexc_type = type;
    tstate->curexc_value = value;
    tstate->curexc_traceback = traceback;

    Py_XDECREF(oldtype);
    Py_XDECREF(oldvalue);
    Py_XDECREF(oldtraceback);
}

void
PyErr_Restore(PyObject *type, PyObject *value, PyObject *traceback)
{
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_Restore(tstate, type, value, traceback);
}


_PyErr_StackItem *
_PyErr_GetTopmostException(PyThreadState *tstate)
{
    _PyErr_StackItem *exc_info = tstate->exc_info;
    while ((exc_info->exc_type == NULL || exc_info->exc_type == Py_None) &&
           exc_info->previous_item != NULL)
    {
        exc_info = exc_info->previous_item;
    }
    return exc_info;
}

static PyObject*
_PyErr_CreateException(PyObject *exception, PyObject *value)
{
    if (value == NULL || value == Py_None) {
        return _PyObject_CallNoArg(exception);
    }
    else if (PyTuple_Check(value)) {
        return PyObject_Call(exception, value, NULL);
    }
    else {
        return PyObject_CallOneArg(exception, value);
    }
}

void
_PyErr_SetObject(PyThreadState *tstate, PyObject *exception, PyObject *value)
{
    PyObject *exc_value;
    PyObject *tb = NULL;

    if (exception != NULL &&
        !PyExceptionClass_Check(exception)) {
        _PyErr_Format(tstate, PyExc_SystemError,
                      "_PyErr_SetObject: "
                      "exception %R is not a BaseException subclass",
                      exception);
        return;
    }

    Py_XINCREF(value);
    exc_value = _PyErr_GetTopmostException(tstate)->exc_value;
    if (exc_value != NULL && exc_value != Py_None) {
        /* Implicit exception chaining */
        Py_INCREF(exc_value);
        if (value == NULL || !PyExceptionInstance_Check(value)) {
            /* We must normalize the value right now */
            PyObject *fixed_value;

            /* Issue #23571: functions must not be called with an
               exception set */
            _PyErr_Clear(tstate);

            fixed_value = _PyErr_CreateException(exception, value);
            Py_XDECREF(value);
            if (fixed_value == NULL) {
                Py_DECREF(exc_value);
                return;
            }

            value = fixed_value;
        }

        /* Avoid reference cycles through the context chain.
           This is O(chain length) but context chains are
           usually very short. Sensitive readers may try
           to inline the call to PyException_GetContext. */
        if (exc_value != value) {
            PyObject *o = exc_value, *context;
            while ((context = PyException_GetContext(o))) {
                Py_DECREF(context);
                if (context == value) {
                    PyException_SetContext(o, NULL);
                    break;
                }
                o = context;
            }
            PyException_SetContext(value, exc_value);
        }
        else {
            Py_DECREF(exc_value);
        }
    }
    if (value != NULL && PyExceptionInstance_Check(value))
        tb = PyException_GetTraceback(value);
    Py_XINCREF(exception);
    _PyErr_Restore(tstate, exception, value, tb);
}

void
PyErr_SetObject(PyObject *exception, PyObject *value)
{
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_SetObject(tstate, exception, value);
}

/* Set a key error with the specified argument, wrapping it in a
 * tuple automatically so that tuple keys are not unpacked as the
 * exception arguments. */
void
_PyErr_SetKeyError(PyObject *arg)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *tup = PyTuple_Pack(1, arg);
    if (!tup) {
        /* caller will expect error to be set anyway */
        return;
    }
    _PyErr_SetObject(tstate, PyExc_KeyError, tup);
    Py_DECREF(tup);
}

void
_PyErr_SetNone(PyThreadState *tstate, PyObject *exception)
{
    _PyErr_SetObject(tstate, exception, (PyObject *)NULL);
}


void
PyErr_SetNone(PyObject *exception)
{
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_SetNone(tstate, exception);
}


void
_PyErr_SetString(PyThreadState *tstate, PyObject *exception,
                 const char *string)
{
    PyObject *value = PyString_FromString(string);
    _PyErr_SetObject(tstate, exception, value);
    Py_XDECREF(value);
}

void
PyErr_SetString(PyObject *exception, const char *string)
{
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_SetString(tstate, exception, string);
}


PyObject* _Py_HOT_FUNCTION
PyErr_Occurred(void)
{
    PyThreadState *tstate = PyThreadState_Get();
    return _PyErr_Occurred(tstate);
}


int
PyErr_GivenExceptionMatches(PyObject *err, PyObject *exc)
{
    if (err == NULL || exc == NULL) {
        /* maybe caused by "import exceptions" that failed early on */
        return 0;
    }
    if (PyTuple_Check(exc)) {
        Py_ssize_t i, n;
        n = PyTuple_Size(exc);
        for (i = 0; i < n; i++) {
            /* Test recursively */
             if (PyErr_GivenExceptionMatches(
                 err, PyTuple_GetItem(exc, i)))
             {
                 return 1;
             }
        }
        return 0;
    }
    /* err might be an instance, so check its class. */
    if (PyExceptionInstance_Check(err))
        err = PyExceptionInstance_Class(err);

    if (PyExceptionClass_Check(err) && PyExceptionClass_Check(exc)) {
        return PyType_IsSubtype((PyTypeObject *)err, (PyTypeObject *)exc);
    }

    return err == exc;
}


int
_PyErr_ExceptionMatches(PyThreadState *tstate, PyObject *exc)
{
    return PyErr_GivenExceptionMatches(_PyErr_Occurred(tstate), exc);
}


int
PyErr_ExceptionMatches(PyObject *exc)
{
    PyThreadState *tstate = PyThreadState_Get();
    return _PyErr_ExceptionMatches(tstate, exc);
}


#ifndef Py_NORMALIZE_RECURSION_LIMIT
#define Py_NORMALIZE_RECURSION_LIMIT 32
#endif

/* Used in many places to normalize a raised exception, including in
   eval_code2(), do_raise(), and PyErr_Print()

   XXX: should PyErr_NormalizeException() also call
            PyException_SetTraceback() with the resulting value and tb?
*/
void
_PyErr_NormalizeException(PyThreadState *tstate, PyObject **exc,
                          PyObject **val, PyObject **tb)
{
    int recursion_depth = 0;
    PyObject *type, *value, *initial_tb;

  restart:
    type = *exc;
    if (type == NULL) {
        /* There was no exception, so nothing to do. */
        return;
    }

    value = *val;
    /* If PyErr_SetNone() was used, the value will have been actually
       set to NULL.
    */
    if (!value) {
        value = Py_None;
        Py_INCREF(value);
    }

    /* Normalize the exception so that if the type is a class, the
       value will be an instance.
    */
    if (PyExceptionClass_Check(type)) {
        PyObject *inclass = NULL;
        int is_subclass = 0;

        if (PyExceptionInstance_Check(value)) {
            inclass = PyExceptionInstance_Class(value);
            is_subclass = PyObject_IsSubclass(inclass, type);
            if (is_subclass < 0) {
                goto error;
            }
        }

        /* If the value was not an instance, or is not an instance
           whose class is (or is derived from) type, then use the
           value as an argument to instantiation of the type
           class.
        */
        if (!is_subclass) {
            PyObject *fixed_value = _PyErr_CreateException(type, value);
            if (fixed_value == NULL) {
                goto error;
            }
            Py_DECREF(value);
            value = fixed_value;
        }
        /* If the class of the instance doesn't exactly match the
           class of the type, believe the instance.
        */
        else if (inclass != type) {
            Py_INCREF(inclass);
            Py_DECREF(type);
            type = inclass;
        }
    }
    *exc = type;
    *val = value;
    return;

  error:
    Py_DECREF(type);
    Py_DECREF(value);
    recursion_depth++;
    
    /* If the new exception doesn't set a traceback and the old
       exception had a traceback, use the old traceback for the
       new exception.  It's better than nothing.
    */
    initial_tb = *tb;
    _PyErr_Fetch(tstate, exc, val, tb);
    assert(*exc != NULL);
    if (initial_tb != NULL) {
        if (*tb == NULL)
            *tb = initial_tb;
        else
            Py_DECREF(initial_tb);
    }
    /* Abort when Py_NORMALIZE_RECURSION_LIMIT has been exceeded, and the
       corresponding RecursionError could not be normalized, and the
       MemoryError raised when normalize this RecursionError could not be
       normalized. */
    if (recursion_depth >= Py_NORMALIZE_RECURSION_LIMIT + 2) {
        if (PyErr_GivenExceptionMatches(*exc, PyExc_MemoryError)) {
            Py_FatalError("Cannot recover from MemoryErrors "
                          "while normalizing exceptions.");
        }
        else {
            Py_FatalError("Cannot recover from the recursive normalization "
                          "of an exception.");
        }
    }
    goto restart;
}


void
PyErr_NormalizeException(PyObject **exc, PyObject **val, PyObject **tb)
{
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_NormalizeException(tstate, exc, val, tb);
}


void
_PyErr_Fetch(PyThreadState *tstate, PyObject **p_type, PyObject **p_value,
             PyObject **p_traceback)
{
    *p_type = tstate->curexc_type;
    *p_value = tstate->curexc_value;
    *p_traceback = tstate->curexc_traceback;

    tstate->curexc_type = NULL;
    tstate->curexc_value = NULL;
    tstate->curexc_traceback = NULL;
}


void
PyErr_Fetch(PyObject **p_type, PyObject **p_value, PyObject **p_traceback)
{
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_Fetch(tstate, p_type, p_value, p_traceback);
}


void
_PyErr_Clear(PyThreadState *tstate)
{
    _PyErr_Restore(tstate, NULL, NULL, NULL);
}


void
PyErr_Clear(void)
{
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_Clear(tstate);
}


void
_PyErr_GetExcInfo(PyThreadState *tstate,
                  PyObject **p_type, PyObject **p_value, PyObject **p_traceback)
{
    _PyErr_StackItem *exc_info = _PyErr_GetTopmostException(tstate);
    *p_type = exc_info->exc_type;
    *p_value = exc_info->exc_value;
    *p_traceback = exc_info->exc_traceback;

    Py_XINCREF(*p_type);
    Py_XINCREF(*p_value);
    Py_XINCREF(*p_traceback);
}


void
PyErr_GetExcInfo(PyObject **p_type, PyObject **p_value, PyObject **p_traceback)
{
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_GetExcInfo(tstate, p_type, p_value, p_traceback);
}

void
PyErr_SetExcInfo(PyObject *p_type, PyObject *p_value, PyObject *p_traceback)
{
    PyObject *oldtype, *oldvalue, *oldtraceback;
    PyThreadState *tstate = PyThreadState_Get();

    oldtype = tstate->exc_info->exc_type;
    oldvalue = tstate->exc_info->exc_value;
    oldtraceback = tstate->exc_info->exc_traceback;

    tstate->exc_info->exc_type = p_type;
    tstate->exc_info->exc_value = p_value;
    tstate->exc_info->exc_traceback = p_traceback;

    Py_XDECREF(oldtype);
    Py_XDECREF(oldvalue);
    Py_XDECREF(oldtraceback);
}

/* Like PyErr_Restore(), but if an exception is already set,
   set the context associated with it.

   The caller is responsible for ensuring that this call won't create
   any cycles in the exception context chain. */
void
_PyErr_ChainExceptions(PyObject *exc, PyObject *val, PyObject *tb)
{
    if (exc == NULL)
        return;

    PyThreadState *tstate = PyThreadState_Get();

    if (!PyExceptionClass_Check(exc)) {
        _PyErr_Format(tstate, PyExc_SystemError,
                      "_PyErr_ChainExceptions: "
                      "exception %R is not a BaseException subclass",
                      exc);
        return;
    }

    if (_PyErr_Occurred(tstate)) {
        PyObject *exc2, *val2, *tb2;
        _PyErr_Fetch(tstate, &exc2, &val2, &tb2);
        _PyErr_NormalizeException(tstate, &exc, &val, &tb);
        if (tb != NULL) {
            PyException_SetTraceback(val, tb);
            Py_DECREF(tb);
        }
        Py_DECREF(exc);
        _PyErr_NormalizeException(tstate, &exc2, &val2, &tb2);
        PyException_SetContext(val2, val);
        _PyErr_Restore(tstate, exc2, val2, tb2);
    }
    else {
        _PyErr_Restore(tstate, exc, val, tb);
    }
}

/* Set the currently set exception's context to the given exception.

   If the provided exc_info is NULL, then the current Python thread state's
   exc_info will be used for the context instead.

   This function can only be called when _PyErr_Occurred() is true.
   Also, this function won't create any cycles in the exception context
   chain to the extent that _PyErr_SetObject ensures this. */
void
_PyErr_ChainStackItem(_PyErr_StackItem *exc_info)
{
    PyThreadState *tstate = PyThreadState_Get();
    assert(_PyErr_Occurred(tstate));

    int exc_info_given;
    if (exc_info == NULL) {
        exc_info_given = 0;
        exc_info = tstate->exc_info;
    } else {
        exc_info_given = 1;
    }
    if (exc_info->exc_type == NULL || exc_info->exc_type == Py_None) {
        return;
    }

    _PyErr_StackItem *saved_exc_info;
    if (exc_info_given) {
        /* Temporarily set the thread state's exc_info since this is what
           _PyErr_SetObject uses for implicit exception chaining. */
        saved_exc_info = tstate->exc_info;
        tstate->exc_info = exc_info;
    }

    PyObject *exc, *val, *tb;
    _PyErr_Fetch(tstate, &exc, &val, &tb);

    PyObject *exc2, *val2, *tb2;
    exc2 = exc_info->exc_type;
    val2 = exc_info->exc_value;
    tb2 = exc_info->exc_traceback;
    _PyErr_NormalizeException(tstate, &exc2, &val2, &tb2);
    if (tb2 != NULL) {
        PyException_SetTraceback(val2, tb2);
    }

    /* _PyErr_SetObject sets the context from PyThreadState. */
    _PyErr_SetObject(tstate, exc, val);
    Py_DECREF(exc);  // since _PyErr_Occurred was true
    Py_XDECREF(val);
    Py_XDECREF(tb);

    if (exc_info_given) {
        tstate->exc_info = saved_exc_info;
    }
}

static PyObject *
_PyErr_FormatVFromCause(PyThreadState *tstate, PyObject *exception,
                        const char *format, va_list vargs)
{
    PyObject *exc, *val, *val2, *tb;

    assert(_PyErr_Occurred(tstate));
    _PyErr_Fetch(tstate, &exc, &val, &tb);
    _PyErr_NormalizeException(tstate, &exc, &val, &tb);
    if (tb != NULL) {
        PyException_SetTraceback(val, tb);
        Py_DECREF(tb);
    }
    Py_DECREF(exc);
    assert(!_PyErr_Occurred(tstate));

    _PyErr_FormatV(tstate, exception, format, vargs);

    _PyErr_Fetch(tstate, &exc, &val2, &tb);
    _PyErr_NormalizeException(tstate, &exc, &val2, &tb);
    Py_INCREF(val);
    PyException_SetCause(val2, val);
    PyException_SetContext(val2, val);
    _PyErr_Restore(tstate, exc, val2, tb);

    return NULL;
}

PyObject *
_PyErr_FormatFromCauseTstate(PyThreadState *tstate, PyObject *exception,
                             const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    _PyErr_FormatVFromCause(tstate, exception, format, vargs);
    va_end(vargs);
    return NULL;
}

PyObject *
_PyErr_FormatFromCause(PyObject *exception, const char *format, ...)
{
    PyThreadState *tstate = PyThreadState_Get();
    va_list vargs;
    va_start(vargs, format);
    _PyErr_FormatVFromCause(tstate, exception, format, vargs);
    va_end(vargs);
    return NULL;
}

/* Convenience functions to set a type error exception and return 0 */

int
PyErr_BadArgument(void)
{
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_SetString(tstate, PyExc_TypeError,
                     "bad argument type for built-in operation");
    return 0;
}

PyObject *
_PyErr_NoMemory(PyThreadState *tstate)
{
    if (Py_IS_TYPE(PyExc_MemoryError, NULL)) {
        /* PyErr_NoMemory() has been called before PyExc_MemoryError has been
           initialized by _PyExc_Init() */
        Py_FatalError("Out of memory and PyExc_MemoryError is not "
                      "initialized yet");
    }
    _PyErr_SetNone(tstate, PyExc_MemoryError);
    return NULL;
}

PyObject *
PyErr_NoMemory(void)
{
    PyThreadState *tstate = PyThreadState_Get();
    return _PyErr_NoMemory(tstate);
}

PyObject *
PyErr_SetFromErrnoWithFilenameObject(PyObject *exc, PyObject *filenameObject)
{
    return PyErr_SetFromErrnoWithFilenameObjects(exc, filenameObject, NULL);
}

PyObject *
PyErr_SetFromErrnoWithFilenameObjects(PyObject *exc, PyObject *filenameObject, PyObject *filenameObject2)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *message;
    PyObject *v, *args;
    int i = errno;
    
    if (i != 0) {
        const char *s = strerror(i);
	message = PyString_FromString(s);
    }
    else {
        /* Sometimes errno didn't get set */
        message = PyString_FromString("Error");
    }

    if (message == NULL)
    {
        return NULL;
    }

    if (filenameObject != NULL) {
        if (filenameObject2 != NULL)
            args = Py_BuildValue("(iOOiO)", i, message, filenameObject, 0, filenameObject2);
        else
            args = Py_BuildValue("(iOO)", i, message, filenameObject);
    } else {
        assert(filenameObject2 == NULL);
        args = Py_BuildValue("(iO)", i, message);
    }
    Py_DECREF(message);

    if (args != NULL) {
        v = PyObject_Call(exc, args, NULL);
        Py_DECREF(args);
        if (v != NULL) {
            _PyErr_SetObject(tstate, (PyObject *) Py_TYPE(v), v);
            Py_DECREF(v);
        }
    }
    return NULL;
}

PyObject *
PyErr_SetFromErrnoWithFilename(PyObject *exc, const char *filename)
{
    PyObject *name = filename ? PyString_FromString(filename) : NULL;
    PyObject *result = PyErr_SetFromErrnoWithFilenameObjects(exc, name, NULL);
    Py_XDECREF(name);
    return result;
}

PyObject *
PyErr_SetFromErrno(PyObject *exc)
{
    return PyErr_SetFromErrnoWithFilenameObjects(exc, NULL, NULL);
}

PyObject *
PyErr_SetImportErrorSubclass(PyObject *exception, PyObject *msg,
    PyObject *name, PyObject *path)
{
    PyThreadState *tstate = PyThreadState_Get();
    int issubclass;
    PyObject *kwargs, *error;

    issubclass = PyObject_IsSubclass(exception, PyExc_ImportError);
    if (issubclass < 0) {
        return NULL;
    }
    else if (!issubclass) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "expected a subclass of ImportError");
        return NULL;
    }

    if (msg == NULL) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "expected a message argument");
        return NULL;
    }

    if (name == NULL) {
        name = Py_None;
    }
    if (path == NULL) {
        path = Py_None;
    }

    kwargs = PyDict_New();
    if (kwargs == NULL) {
        return NULL;
    }
    if (PyDict_SetItemString(kwargs, "name", name) < 0) {
        goto done;
    }
    if (PyDict_SetItemString(kwargs, "path", path) < 0) {
        goto done;
    }

    error = PyObject_VectorcallDict(exception, &msg, 1, kwargs);
    if (error != NULL) {
        _PyErr_SetObject(tstate, (PyObject *)Py_TYPE(error), error);
        Py_DECREF(error);
    }

done:
    Py_DECREF(kwargs);
    return NULL;
}

PyObject *
PyErr_SetImportError(PyObject *msg, PyObject *name, PyObject *path)
{
    return PyErr_SetImportErrorSubclass(PyExc_ImportError, msg, name, path);
}

void
_PyErr_BadInternalCall(const char *filename, int lineno)
{
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_Format(tstate, PyExc_SystemError,
                  "%s:%d: bad argument to internal function",
                  filename, lineno);
}

/* Remove the preprocessor macro for PyErr_BadInternalCall() so that we can
   export the entry point for existing object code: */
#undef PyErr_BadInternalCall
void
PyErr_BadInternalCall(void)
{
    assert(0 && "bad argument to internal function");
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_SetString(tstate, PyExc_SystemError,
                     "bad argument to internal function");
}
#define PyErr_BadInternalCall() _PyErr_BadInternalCall(__FILE__, __LINE__)


static PyObject *
_PyErr_FormatV(PyThreadState *tstate, PyObject *exception,
               const char *format, va_list vargs)
{
    PyObject* string;

    /* Issue #23571: PyString_FromFormatV() must not be called with an
       exception set, it calls arbitrary Python code like PyObject_Repr() */
    _PyErr_Clear(tstate);

    string = PyString_FromFormatV(format, vargs);

    _PyErr_SetObject(tstate, exception, string);
    Py_XDECREF(string);
    return NULL;
}


PyObject *
PyErr_FormatV(PyObject *exception, const char *format, va_list vargs)
{
    PyThreadState *tstate = PyThreadState_Get();
    return _PyErr_FormatV(tstate, exception, format, vargs);
}


PyObject *
_PyErr_Format(PyThreadState *tstate, PyObject *exception,
              const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    _PyErr_FormatV(tstate, exception, format, vargs);
    va_end(vargs);
    return NULL;
}


PyObject *
PyErr_Format(PyObject *exception, const char *format, ...)
{
    PyThreadState *tstate = PyThreadState_Get();
    va_list vargs;
    va_start(vargs, format);
    _PyErr_FormatV(tstate, exception, format, vargs);
    va_end(vargs);
    return NULL;
}


PyObject *
PyErr_NewException(const char *name, PyObject *base, PyObject *dict)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *modulename = NULL;
    PyObject *classname = NULL;
    PyObject *mydict = NULL;
    PyObject *bases = NULL;
    PyObject *result = NULL;

    const char *dot = strrchr(name, '.');
    if (dot == NULL) {
        _PyErr_SetString(tstate, PyExc_SystemError,
                         "PyErr_NewException: name must be module.class");
        return NULL;
    }
    if (base == NULL) {
        base = PyExc_Exception;
    }
    if (dict == NULL) {
        dict = mydict = PyDict_New();
        if (dict == NULL)
            goto failure;
    }

    if (_PyDict_GetItemIdWithError(dict, &PyId___module__) == NULL) {
        if (_PyErr_Occurred(tstate)) {
            goto failure;
        }
        modulename = PyString_FromStringAndSize(name,
                                             (Py_ssize_t)(dot-name));
        if (modulename == NULL)
            goto failure;
        if (_PyDict_SetItemId(dict, &PyId___module__, modulename) != 0)
            goto failure;
    }
    if (PyTuple_Check(base)) {
        bases = base;
        /* INCREF as we create a new ref in the else branch */
        Py_INCREF(bases);
    } else {
        bases = PyTuple_Pack(1, base);
        if (bases == NULL)
            goto failure;
    }
    /* Create a real class. */
    result = PyObject_CallFunction((PyObject *)&PyType_Type, "sOO",
                                   dot+1, bases, dict);
  failure:
    Py_XDECREF(bases);
    Py_XDECREF(mydict);
    Py_XDECREF(classname);
    Py_XDECREF(modulename);
    return result;
}


/* Create an exception with docstring */
PyObject *
PyErr_NewExceptionWithDoc(const char *name, const char *doc,
                          PyObject *base, PyObject *dict)
{
    int result;
    PyObject *ret = NULL;
    PyObject *mydict = NULL; /* points to the dict only if we create it */
    PyObject *docobj;

    if (dict == NULL) {
        dict = mydict = PyDict_New();
        if (dict == NULL) {
            return NULL;
        }
    }

    if (doc != NULL) {
        docobj = PyString_FromString(doc);
        if (docobj == NULL)
            goto failure;
        result = PyDict_SetItemString(dict, "__doc__", docobj);
        Py_DECREF(docobj);
        if (result < 0)
            goto failure;
    }

    ret = PyErr_NewException(name, base, dict);
  failure:
    Py_XDECREF(mydict);
    return ret;
}


PyDoc_STRVAR(UnraisableHookArgs__doc__,
"UnraisableHookArgs\n\
\n\
Type used to pass arguments to sys.unraisablehook.");

static PyTypeObject UnraisableHookArgsType;

static PyStructSequence_Field UnraisableHookArgs_fields[] = {
    {"exc_type", "Exception type"},
    {"exc_value", "Exception value"},
    {"exc_traceback", "Exception traceback"},
    {"err_msg", "Error message"},
    {"object", "Object causing the exception"},
    {0}
};

static PyStructSequence_Desc UnraisableHookArgs_desc = {
    .name = "UnraisableHookArgs",
    .doc = UnraisableHookArgs__doc__,
    .fields = UnraisableHookArgs_fields,
    .n_in_sequence = 5
};


PyStatus
_PyErr_Init(void)
{
    if (UnraisableHookArgsType.tp_name == NULL) {
        if (PyStructSequence_InitType(&UnraisableHookArgsType,
                                       &UnraisableHookArgs_desc) < 0) {
            return _PyStatus_ERR("failed to initialize UnraisableHookArgs type");
        }
    }
    return _PyStatus_OK();
}


static PyObject *
make_unraisable_hook_args(PyThreadState *tstate, PyObject *exc_type,
                          PyObject *exc_value, PyObject *exc_tb,
                          PyObject *err_msg, PyObject *obj)
{
    PyObject *args = PyStructSequence_New(&UnraisableHookArgsType);
    if (args == NULL) {
        return NULL;
    }

    Py_ssize_t pos = 0;
#define ADD_ITEM(exc_type) \
        do { \
            if (exc_type == NULL) { \
                exc_type = Py_None; \
            } \
            Py_INCREF(exc_type); \
            PyStructSequence_InitItem(args, pos++, exc_type); \
        } while (0)


    ADD_ITEM(exc_type);
    ADD_ITEM(exc_value);
    ADD_ITEM(exc_tb);
    ADD_ITEM(err_msg);
    ADD_ITEM(obj);
#undef ADD_ITEM

    if (_PyErr_Occurred(tstate)) {
        Py_DECREF(args);
        return NULL;
    }
    return args;
}



/* Default implementation of sys.unraisablehook.

   It can be called to log the exception of a custom sys.unraisablehook.

   Do nothing if sys.stderr attribute doesn't exist or is set to None. */
static int
write_unraisable_exc_file(PyThreadState *tstate, PyObject *exc_type,
                          PyObject *exc_value, PyObject *exc_tb,
                          PyObject *err_msg, PyObject *obj, PyObject *file)
{
    if (obj != NULL && obj != Py_None) {
        if (err_msg != NULL && err_msg != Py_None) {
            if (PyFile_WriteObject(err_msg, file, Py_PRINT_RAW) < 0) {
                return -1;
            }
            if (PyFile_WriteString(": ", file) < 0) {
                return -1;
            }
        }
        else {
            if (PyFile_WriteString("Exception ignored in: ", file) < 0) {
                return -1;
            }
        }

        if (PyFile_WriteObject(obj, file, 0) < 0) {
            _PyErr_Clear(tstate);
            if (PyFile_WriteString("<object repr() failed>", file) < 0) {
                return -1;
            }
        }
        if (PyFile_WriteString("\n", file) < 0) {
            return -1;
        }
    }
    else if (err_msg != NULL && err_msg != Py_None) {
        if (PyFile_WriteObject(err_msg, file, Py_PRINT_RAW) < 0) {
            return -1;
        }
        if (PyFile_WriteString(":\n", file) < 0) {
            return -1;
        }
    }

    if (exc_tb != NULL && exc_tb != Py_None) {
        if (PyTraceBack_Print(exc_tb, file) < 0) {
            /* continue even if writing the traceback failed */
            _PyErr_Clear(tstate);
        }
    }

    if (exc_type == NULL || exc_type == Py_None) {
        return -1;
    }

    assert(PyExceptionClass_Check(exc_type));
    const char *className = PyExceptionClass_Name(exc_type);
    if (className != NULL) {
        const char *dot = strrchr(className, '.');
        if (dot != NULL) {
            className = dot+1;
        }
    }

    PyObject *moduleName = _PyObject_GetAttrId(exc_type, &PyId___module__);
    if (moduleName == NULL || !PyString_Check(moduleName)) {
        Py_XDECREF(moduleName);
        _PyErr_Clear(tstate);
        if (PyFile_WriteString("<unknown>", file) < 0) {
            return -1;
        }
    }
    else {
        if (!_PyUnicode_EqualToASCIIId(moduleName, &PyId_builtins)) {
            if (PyFile_WriteObject(moduleName, file, Py_PRINT_RAW) < 0) {
                Py_DECREF(moduleName);
                return -1;
            }
            Py_DECREF(moduleName);
            if (PyFile_WriteString(".", file) < 0) {
                return -1;
            }
        }
        else {
            Py_DECREF(moduleName);
        }
    }
    if (className == NULL) {
        if (PyFile_WriteString("<unknown>", file) < 0) {
            return -1;
        }
    }
    else {
        if (PyFile_WriteString(className, file) < 0) {
            return -1;
        }
    }

    if (exc_value && exc_value != Py_None) {
        if (PyFile_WriteString(": ", file) < 0) {
            return -1;
        }
        if (PyFile_WriteObject(exc_value, file, Py_PRINT_RAW) < 0) {
            _PyErr_Clear(tstate);
            if (PyFile_WriteString("<exception str() failed>", file) < 0) {
                return -1;
            }
        }
    }

    if (PyFile_WriteString("\n", file) < 0) {
        return -1;
    }

    /* Explicitly call file.flush() */
    PyObject *res = _PyObject_CallMethodIdNoArgs(file, &PyId_flush);
    if (!res) {
        return -1;
    }
    Py_DECREF(res);

    return 0;
}


static int
write_unraisable_exc(PyThreadState *tstate, PyObject *exc_type,
                     PyObject *exc_value, PyObject *exc_tb, PyObject *err_msg,
                     PyObject *obj)
{
    PyObject *file = _PySys_GetObjectId(&PyId_stderr);
    if (file == NULL || file == Py_None) {
        return 0;
    }

    /* Hold a strong reference to ensure that sys.stderr doesn't go away
       while we use it */
    Py_INCREF(file);
    int res = write_unraisable_exc_file(tstate, exc_type, exc_value, exc_tb,
                                        err_msg, obj, file);
    Py_DECREF(file);

    return res;
}


PyObject*
_PyErr_WriteUnraisableDefaultHook(PyObject *args)
{
    PyThreadState *tstate = PyThreadState_Get();

    if (!Py_IS_TYPE(args, &UnraisableHookArgsType)) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "sys.unraisablehook argument type "
                         "must be UnraisableHookArgs");
        return NULL;
    }

    /* Borrowed references */
    PyObject *exc_type = PyStructSequence_GetItem(args, 0);
    PyObject *exc_value = PyStructSequence_GetItem(args, 1);
    PyObject *exc_tb = PyStructSequence_GetItem(args, 2);
    PyObject *err_msg = PyStructSequence_GetItem(args, 3);
    PyObject *obj = PyStructSequence_GetItem(args, 4);

    if (write_unraisable_exc(tstate, exc_type, exc_value, exc_tb, err_msg, obj) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}


/* Call sys.unraisablehook().

   This function can be used when an exception has occurred but there is no way
   for Python to handle it. For example, when a destructor raises an exception
   or during garbage collection (gc.collect()).

   If err_msg_str is non-NULL, the error message is formatted as:
   "Exception ignored %s" % err_msg_str. Otherwise, use "Exception ignored in"
   error message.

   An exception must be set when calling this function. */
void
_PyErr_WriteUnraisableMsg(const char *err_msg_str, PyObject *obj)
{
    PyThreadState *tstate = PyThreadState_Get();

    PyObject *err_msg = NULL;
    PyObject *exc_type, *exc_value, *exc_tb;
    _PyErr_Fetch(tstate, &exc_type, &exc_value, &exc_tb);

    assert(exc_type != NULL);

    if (exc_type == NULL) {
        /* sys.unraisablehook requires that at least exc_type is set */
        goto default_hook;
    }

    if (exc_tb == NULL) {
        PyFrameObject *frame = tstate->frame;
        if (frame != NULL) {
            exc_tb = _PyTraceBack_FromFrame(NULL, frame);
            if (exc_tb == NULL) {
                _PyErr_Clear(tstate);
            }
        }
    }

    _PyErr_NormalizeException(tstate, &exc_type, &exc_value, &exc_tb);

    if (exc_tb != NULL && exc_tb != Py_None && PyTraceBack_Check(exc_tb)) {
        if (PyException_SetTraceback(exc_value, exc_tb) < 0) {
            _PyErr_Clear(tstate);
        }
    }

    if (err_msg_str != NULL) {
        err_msg = PyString_FromFormat("Exception ignored %s", err_msg_str);
        if (err_msg == NULL) {
            PyErr_Clear();
        }
    }

    PyObject *hook_args = make_unraisable_hook_args(
        tstate, exc_type, exc_value, exc_tb, err_msg, obj);
    if (hook_args == NULL) {
        err_msg_str = ("Exception ignored on building "
                       "sys.unraisablehook arguments");
        goto error;
    }

    _Py_IDENTIFIER(unraisablehook);
    PyObject *hook = _PySys_GetObjectId(&PyId_unraisablehook);
    if (hook == NULL) {
        Py_DECREF(hook_args);
        goto default_hook;
    }

    if (hook == Py_None) {
        Py_DECREF(hook_args);
        goto default_hook;
    }

    PyObject *res = PyObject_CallOneArg(hook, hook_args);
    Py_DECREF(hook_args);
    if (res != NULL) {
        Py_DECREF(res);
        goto done;
    }

    /* sys.unraisablehook failed: log its error using default hook */
    obj = hook;
    err_msg_str = NULL;

error:
    /* err_msg_str and obj have been updated and we have a new exception */
    Py_XSETREF(err_msg, PyString_FromString(err_msg_str ?
        err_msg_str : "Exception ignored in sys.unraisablehook"));
    Py_XDECREF(exc_type);
    Py_XDECREF(exc_value);
    Py_XDECREF(exc_tb);
    _PyErr_Fetch(tstate, &exc_type, &exc_value, &exc_tb);

default_hook:
    /* Call the default unraisable hook (ignore failure) */
    (void)write_unraisable_exc(tstate, exc_type, exc_value, exc_tb,
                               err_msg, obj);

done:
    Py_XDECREF(exc_type);
    Py_XDECREF(exc_value);
    Py_XDECREF(exc_tb);
    Py_XDECREF(err_msg);
    _PyErr_Clear(tstate); /* Just in case */
}


void
PyErr_WriteUnraisable(PyObject *obj)
{
    _PyErr_WriteUnraisableMsg(NULL, obj);
}


extern PyObject *PyModule_GetWarningsModule(void);


void
PyErr_SyntaxLocation(const char *filename, int lineno)
{
    PyErr_SyntaxLocationEx(filename, lineno, -1);
}


/* Set file and line information for the current exception.
   If the exception is not a SyntaxError, also sets additional attributes
   to make printing of exceptions believe it is a syntax error. */

void
PyErr_SyntaxLocationObject(PyObject *filename, int lineno, int col_offset)
{
    PyObject *exc, *v, *tb, *tmp;
    _Py_IDENTIFIER(filename);
    _Py_IDENTIFIER(lineno);
    _Py_IDENTIFIER(msg);
    _Py_IDENTIFIER(offset);
    _Py_IDENTIFIER(print_file_and_line);
    _Py_IDENTIFIER(text);
    PyThreadState *tstate = PyThreadState_Get();

    /* add attributes for the line number and filename for the error */
    _PyErr_Fetch(tstate, &exc, &v, &tb);
    _PyErr_NormalizeException(tstate, &exc, &v, &tb);
    /* XXX check that it is, indeed, a syntax error. It might not
     * be, though. */
    tmp = PyLong_FromLong(lineno);
    if (tmp == NULL)
        _PyErr_Clear(tstate);
    else {
        if (_PyObject_SetAttrId(v, &PyId_lineno, tmp)) {
            _PyErr_Clear(tstate);
        }
        Py_DECREF(tmp);
    }
    tmp = NULL;
    if (col_offset >= 0) {
        tmp = PyLong_FromLong(col_offset);
        if (tmp == NULL) {
            _PyErr_Clear(tstate);
        }
    }
    if (_PyObject_SetAttrId(v, &PyId_offset, tmp ? tmp : Py_None)) {
        _PyErr_Clear(tstate);
    }
    Py_XDECREF(tmp);
    if (filename != NULL) {
        if (_PyObject_SetAttrId(v, &PyId_filename, filename)) {
            _PyErr_Clear(tstate);
        }

        tmp = PyErr_ProgramTextObject(filename, lineno);
        if (tmp) {
            if (_PyObject_SetAttrId(v, &PyId_text, tmp)) {
                _PyErr_Clear(tstate);
            }
            Py_DECREF(tmp);
        }
    }
    if (exc != PyExc_SyntaxError) {
        if (!_PyObject_HasAttrId(v, &PyId_msg)) {
            tmp = PyObject_Str(v);
            if (tmp) {
                if (_PyObject_SetAttrId(v, &PyId_msg, tmp)) {
                    _PyErr_Clear(tstate);
                }
                Py_DECREF(tmp);
            }
            else {
                _PyErr_Clear(tstate);
            }
        }
        if (!_PyObject_HasAttrId(v, &PyId_print_file_and_line)) {
            if (_PyObject_SetAttrId(v, &PyId_print_file_and_line,
                                    Py_None)) {
                _PyErr_Clear(tstate);
            }
        }
    }
    _PyErr_Restore(tstate, exc, v, tb);
}

void
PyErr_SyntaxLocationEx(const char *filename, int lineno, int col_offset)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *fileobj;
    if (filename != NULL) {
	fileobj = PyString_FromString(filename);
        if (fileobj == NULL) {
            _PyErr_Clear(tstate);
        }
    }
    else {
        fileobj = NULL;
    }
    PyErr_SyntaxLocationObject(fileobj, lineno, col_offset);
    Py_XDECREF(fileobj);
}

/* Attempt to load the line of text that the exception refers to.  If it
   fails, it will return NULL but will not set an exception.

   XXX The functionality of this function is quite similar to the
   functionality in tb_displayline() in traceback.c. */

static PyObject *
err_programtext(PyThreadState *tstate, FILE *fp, int lineno)
{
    int i;
    char linebuf[1000];

    if (fp == NULL)
        return NULL;
    for (i = 0; i < lineno; i++) {
        char *pLastChar = &linebuf[sizeof(linebuf) - 2];
        do {
            *pLastChar = '\0';
            if (Py_UniversalNewlineFgets(linebuf, sizeof linebuf,
                                         fp, NULL) == NULL)
                break;
            /* fgets read *something*; if it didn't get as
               far as pLastChar, it must have found a newline
               or hit the end of the file; if pLastChar is \n,
               it obviously found a newline; else we haven't
               yet seen a newline, so must continue */
        } while (*pLastChar != '\0' && *pLastChar != '\n');
    }
    fclose(fp);
    if (i == lineno) {
        PyObject *res;
        res = PyString_FromString(linebuf);
        if (res == NULL)
            _PyErr_Clear(tstate);
        return res;
    }
    return NULL;
}

PyObject *
PyErr_ProgramText(const char *filename, int lineno)
{
    FILE *fp;
    if (filename == NULL || *filename == '\0' || lineno <= 0) {
        return NULL;
    }
    PyThreadState *tstate = PyThreadState_Get();
    fp = _Py_fopen(filename, "r" PY_STDIOTEXTMODE);
    return err_programtext(tstate, fp, lineno);
}

PyObject *
PyErr_ProgramTextObject(PyObject *filename, int lineno)
{
    if (filename == NULL || lineno <= 0) {
        return NULL;
    }

    PyThreadState *tstate = PyThreadState_Get();
    FILE *fp = _Py_fopen_obj(filename, "r" PY_STDIOTEXTMODE);
    if (fp == NULL) {
        _PyErr_Clear(tstate);
        return NULL;
    }
    return err_programtext(tstate, fp, lineno);
}

#ifdef __cplusplus
}
#endif
