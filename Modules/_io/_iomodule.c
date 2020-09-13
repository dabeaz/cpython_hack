/*
    An implementation of the new I/O lib as defined by PEP 3116 - "New I/O"

    Classes defined here: UnsupportedOperation, BlockingIOError.
    Functions defined here: open().

    Mostly written by Amaury Forgeot d'Arc
*/

#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "_iomodule.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */

/* Various interned strings */

PyObject *_PyIO_str_close = NULL;
PyObject *_PyIO_str_closed = NULL;
PyObject *_PyIO_str_decode = NULL;
PyObject *_PyIO_str_encode = NULL;
PyObject *_PyIO_str_fileno = NULL;
PyObject *_PyIO_str_flush = NULL;
PyObject *_PyIO_str_getstate = NULL;
PyObject *_PyIO_str_isatty = NULL;
PyObject *_PyIO_str_newlines = NULL;
PyObject *_PyIO_str_nl = NULL;
PyObject *_PyIO_str_peek = NULL;
PyObject *_PyIO_str_read = NULL;
PyObject *_PyIO_str_read1 = NULL;
PyObject *_PyIO_str_readable = NULL;
PyObject *_PyIO_str_readall = NULL;
PyObject *_PyIO_str_readline = NULL;
PyObject *_PyIO_str_reset = NULL;
PyObject *_PyIO_str_seek = NULL;
PyObject *_PyIO_str_seekable = NULL;
PyObject *_PyIO_str_setstate = NULL;
PyObject *_PyIO_str_tell = NULL;
PyObject *_PyIO_str_truncate = NULL;
PyObject *_PyIO_str_writable = NULL;
PyObject *_PyIO_str_write = NULL;

PyObject *_PyIO_empty_str = NULL;
PyObject *_PyIO_empty_bytes = NULL;

PyDoc_STRVAR(module_doc,
"The io module provides the Python interfaces to stream handling. The\n"
"builtin open function is defined in this module.\n"
    );


/*
 * The main open() function
 */
/*[clinic input]
module _io

_io.open
    file: object
    mode: str = "r"

Open file and return a stream.  Raise OSError upon failure.

file is either a text or byte string giving the name (and the path
if the file isn't in the current working directory) of the file to
be opened or an integer file descriptor of the file to be
wrapped. (If a file descriptor is given, it is closed when the
returned I/O object is closed, unless closefd is set to False.)

mode is an optional string that specifies the mode in which the file
is opened. It defaults to 'r' which means open for reading in text
mode.  Other common values are 'w' for writing (truncating the file if
it already exists), 'x' for creating and writing to a new file, and
'a' for appending (which on some Unix systems, means that all writes
append to the end of the file regardless of the current seek position).
In text mode, if encoding is not specified the encoding used is platform
dependent: locale.getpreferredencoding(False) is called to get the
current locale encoding. (For reading and writing raw bytes use binary
mode and leave encoding unspecified.) The available modes are:

========= ===============================================================
Character Meaning
--------- ---------------------------------------------------------------
'r'       open for reading (default)
'w'       open for writing, truncating the file first
'x'       create a new file and open it for writing
'a'       open for writing, appending to the end of the file if it exists
'b'       binary mode
't'       text mode (default)
'+'       open a disk file for updating (reading and writing)
'U'       universal newline mode (deprecated)
========= ===============================================================
*/

static PyObject *
_io_open_impl(PyObject *module, PyObject *file, const char *mode) 
/*[clinic end generated code: output=aefafc4ce2b46dc0 input=7295902222e6b311]*/
{
    unsigned i;
    int creating = 0, reading = 0, writing = 0, appending = 0, updating = 0;
    int text = 0, binary = 0;

    int is_number;
    char rawmode[6], *m;
    PyObject *raw, *result = NULL, *path_or_fd = NULL;

    is_number = PyNumber_Check(file);

    if (is_number) {
        path_or_fd = file;
        Py_INCREF(path_or_fd);
    } else {
      path_or_fd = file;
      Py_INCREF(path_or_fd);
        if (path_or_fd == NULL) {
            return NULL;
        }
    }

    if (!is_number &&
        !PyUnicode_Check(path_or_fd))
      {
        PyErr_Format(PyExc_TypeError, "invalid file: %R", file);
        goto error;
    }

    /* Decode mode */
    for (i = 0; i < strlen(mode); i++) {
        char c = mode[i];

        switch (c) {
        case 'x':
            creating = 1;
            break;
        case 'r':
            reading = 1;
            break;
        case 'w':
            writing = 1;
            break;
        case 'a':
            appending = 1;
            break;
        case '+':
            updating = 1;
            break;
        case 't':
            text = 1;
            break;
        case 'b':
            binary = 1;
            break;
        default:
            goto invalid_mode;
        }
        /* c must not be duplicated */
        if (strchr(mode+i+1, c)) {
          invalid_mode:
            PyErr_Format(PyExc_ValueError, "invalid mode: '%s'", mode);
	    goto error;
        }

    }

    m = rawmode;
    if (creating)  *(m++) = 'x';
    if (reading)   *(m++) = 'r';
    if (writing)   *(m++) = 'w';
    if (appending) *(m++) = 'a';
    if (updating)  *(m++) = '+';
    *m = '\0';

    if (creating + reading + writing + appending > 1) {
        PyErr_SetString(PyExc_ValueError,
                        "must have exactly one of create/read/write/append mode");
	goto error;
    }
    /* Create the Raw file stream */
    {
        PyObject *RawIO_class = (PyObject *)&PyFileIO_Type;
        raw = PyObject_CallFunction(RawIO_class, "Os",
                                    path_or_fd, rawmode);
    }

    if (raw == NULL)
        goto error;
    result = raw;
 error:
    Py_DECREF(path_or_fd);
    return result;
}

/*
 * Private helpers for the io module.
 */


Py_off_t
PyNumber_AsOff_t(PyObject *item, PyObject *err)
{
    Py_off_t result;
    PyObject *runerr;
    PyObject *value = _PyNumber_Index(item);
    if (value == NULL)
        return -1;

    /* We're done if PyLong_AsSsize_t() returns without error. */
    result = PyLong_AsOff_t(value);
    if (result != -1 || !(runerr = PyErr_Occurred()))
        goto finish;

    /* Error handling code -- only manage OverflowError differently */
    if (!PyErr_GivenExceptionMatches(runerr, PyExc_OverflowError))
        goto finish;

    PyErr_Clear();
    /* If no error-handling desired then the default clipping
       is sufficient.
     */
    if (!err) {
        assert(PyLong_Check(value));
        /* Whether or not it is less than or equal to
           zero is determined by the sign of ob_size
        */
        if (_PyLong_Sign(value) < 0)
            result = PY_OFF_T_MIN;
        else
            result = PY_OFF_T_MAX;
    }
    else {
        /* Otherwise replace the error with caller's error object. */
        PyErr_Format(err,
                     "cannot fit '%.200s' into an offset-sized integer",
                     Py_TYPE(item)->tp_name);
    }

 finish:
    Py_DECREF(value);
    return result;
}

static inline _PyIO_State*
get_io_state(PyObject *module)
{
    void *state = PyModule_GetState(module);
    assert(state != NULL);
    return (_PyIO_State *)state;
}

_PyIO_State *
_PyIO_get_module_state(void)
{
    PyObject *mod = PyState_FindModule(&_PyIO_Module);
    _PyIO_State *state;
    if (mod == NULL || (state = get_io_state(mod)) == NULL) {
        PyErr_SetString(PyExc_RuntimeError,
                        "could not find io module state "
                        "(interpreter shutdown?)");
        return NULL;
    }
    return state;
}

PyObject *
_PyIO_get_locale_module(_PyIO_State *state)
{
    PyObject *mod;
    if (state->locale_module != NULL) {
        assert(PyWeakref_CheckRef(state->locale_module));
        mod = PyWeakref_GET_OBJECT(state->locale_module);
        if (mod != Py_None) {
            Py_INCREF(mod);
            return mod;
        }
        Py_CLEAR(state->locale_module);
    }
    mod = PyImport_ImportModule("_bootlocale");
    if (mod == NULL)
        return NULL;
    state->locale_module = PyWeakref_NewRef(mod, NULL);
    if (state->locale_module == NULL) {
        Py_DECREF(mod);
        return NULL;
    }
    return mod;
}

static int
iomodule_clear(PyObject *mod) {
    _PyIO_State *state = get_io_state(mod);
    if (!state->initialized)
        return 0;
    if (state->locale_module != NULL)
        Py_CLEAR(state->locale_module);
    Py_CLEAR(state->unsupported_operation);
    return 0;
}

static void
iomodule_free(PyObject *mod) {
    iomodule_clear(mod);
}


/*
 * Module definition
 */

/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(_io_open__doc__,
"open($module, /, file, mode=\'r\')\n"
"--\n"
"\n"
"Open file and return a stream.  Raise OSError upon failure.\n"
"\n"
"file is either a text or byte string giving the name (and the path\n"
"if the file isn\'t in the current working directory) of the file to\n"
"be opened or an integer file descriptor of the file to be\n"
"wrapped. (If a file descriptor is given, it is closed when the\n"
"returned I/O object is closed, unless closefd is set to False.)\n"
"\n"
"mode is an optional string that specifies the mode in which the file\n"
"is opened. It defaults to \'r\' which means open for reading in text\n"
"mode.  Other common values are \'w\' for writing (truncating the file if\n"
"it already exists), \'x\' for creating and writing to a new file, and\n"
"\'a\' for appending (which on some Unix systems, means that all writes\n"
"append to the end of the file regardless of the current seek position).\n"
"In text mode, if encoding is not specified the encoding used is platform\n"
"dependent: locale.getpreferredencoding(False) is called to get the\n"
"current locale encoding. (For reading and writing raw bytes use binary\n"
"mode and leave encoding unspecified.) The available modes are:\n"
"\n"
"========= ===============================================================\n"
"Character Meaning\n"
"--------- ---------------------------------------------------------------\n"
"\'r\'       open for reading (default)\n"
"\'w\'       open for writing, truncating the file first\n"
"\'x\'       create a new file and open it for writing\n"
"\'a\'       open for writing, appending to the end of the file if it exists\n"
"\'b\'       binary mode\n"
"\'t\'       text mode (default)\n"
"\'+\'       open a disk file for updating (reading and writing)\n"
"========= ===============================================================\n");

#define _IO_OPEN_METHODDEF    \
    {"open", (PyCFunction)(void(*)(void))_io_open, METH_FASTCALL|METH_KEYWORDS, _io_open__doc__},

static PyObject *
_io_open_impl(PyObject *module, PyObject *file, const char *mode);

static PyObject *
_io_open(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"file", "mode", NULL };
    static _PyArg_Parser _parser = {NULL, _keywords, "open", 0};
    PyObject *argsbuf[2];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    PyObject *file;
    const char *mode = "r";

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 2, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    file = args[0];
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[1]) {
        if (!PyUnicode_Check(args[1])) {
            _PyArg_BadArgument("open", "argument 'mode'", "str", args[1]);
            goto exit;
        }
        Py_ssize_t mode_length;
        mode = PyUnicode_AsCharAndSize(args[1], &mode_length);
        if (mode == NULL) {
            goto exit;
        }
        if (strlen(mode) != (size_t)mode_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
skip_optional_pos:
    return_value = _io_open_impl(module, file, mode);

exit:
    return return_value;
}

static PyMethodDef module_methods[] = {
    _IO_OPEN_METHODDEF
    {NULL, NULL}
};

struct PyModuleDef _PyIO_Module = {
    PyModuleDef_HEAD_INIT,
    "io",
    module_doc,
    sizeof(_PyIO_State),
    module_methods,
    NULL,
    NULL, 
    iomodule_clear,
    (freefunc)iomodule_free,
};

PyMODINIT_FUNC
PyInit__io(void)
{
    PyObject *m = PyModule_Create(&_PyIO_Module);
    _PyIO_State *state = NULL;
    if (m == NULL)
        return NULL;
    state = get_io_state(m);
    state->initialized = 0;

#define ADD_TYPE(type) \
    if (PyModule_AddType(m, type) < 0) {  \
        goto fail; \
    }

    /* DEFAULT_BUFFER_SIZE */
    if (PyModule_AddIntMacro(m, DEFAULT_BUFFER_SIZE) < 0)
        goto fail;

    /* UnsupportedOperation inherits from ValueError and OSError */
    state->unsupported_operation = PyObject_CallFunction(
        (PyObject *)&PyType_Type, "s(OO){}",
        "UnsupportedOperation", PyExc_OSError, PyExc_ValueError);
    if (state->unsupported_operation == NULL)
        goto fail;
    Py_INCREF(state->unsupported_operation);
    if (PyModule_AddObject(m, "UnsupportedOperation",
                           state->unsupported_operation) < 0)
        goto fail;

    /* BlockingIOError, for compatibility */
    Py_INCREF(PyExc_BlockingIOError);
    if (PyModule_AddObject(m, "BlockingIOError",
                           (PyObject *) PyExc_BlockingIOError) < 0)
        goto fail;

    /* Concrete base types of the IO ABCs.
       (the ABCs themselves are declared through inheritance in io.py)
    */
    ADD_TYPE(&PyIOBase_Type);
    
    /* Implementation of concrete IO objects. */
    /* FileIO */
    PyFileIO_Type.tp_base = &PyIOBase_Type;
    ADD_TYPE(&PyFileIO_Type);
    
    /* Interned strings */
#define ADD_INTERNED(name) \
    if (!_PyIO_str_ ## name && \
        !(_PyIO_str_ ## name = PyUnicode_InternFromString(# name))) \
        goto fail;

    ADD_INTERNED(close)
    ADD_INTERNED(closed)
    ADD_INTERNED(decode)
    ADD_INTERNED(encode)
    ADD_INTERNED(fileno)
    ADD_INTERNED(flush)
    ADD_INTERNED(getstate)
    ADD_INTERNED(isatty)
    ADD_INTERNED(newlines)
    ADD_INTERNED(peek)
    ADD_INTERNED(read)
    ADD_INTERNED(read1)
    ADD_INTERNED(readable)
    ADD_INTERNED(readall)
    ADD_INTERNED(readline)
    ADD_INTERNED(reset)
    ADD_INTERNED(seek)
    ADD_INTERNED(seekable)
    ADD_INTERNED(setstate)
    ADD_INTERNED(tell)
    ADD_INTERNED(truncate)
    ADD_INTERNED(write)
    ADD_INTERNED(writable)

    if (!_PyIO_str_nl &&
        !(_PyIO_str_nl = PyUnicode_InternFromString("\n")))
        goto fail;

    if (!_PyIO_empty_str &&
        !(_PyIO_empty_str = PyUnicode_FromStringAndSize(NULL, 0)))
        goto fail;
    state->initialized = 1;

    return m;

  fail:
    Py_XDECREF(state->unsupported_operation);
    Py_DECREF(m);
    return NULL;
}


/* Return 1 if an OSError with errno == EINTR is set (and then
   clears the error indicator), 0 otherwise.
   Should only be called when PyErr_Occurred() is true.
*/
int
_PyIO_trap_eintr(void)
{
    static PyObject *eintr_int = NULL;
    PyObject *typ, *val, *tb;
    PyOSErrorObject *env_err;

    if (eintr_int == NULL) {
        eintr_int = PyLong_FromLong(EINTR);
        assert(eintr_int != NULL);
    }
    if (!PyErr_ExceptionMatches(PyExc_OSError))
        return 0;
    PyErr_Fetch(&typ, &val, &tb);
    PyErr_NormalizeException(&typ, &val, &tb);
    env_err = (PyOSErrorObject *) val;
    assert(env_err != NULL);
    if (env_err->myerrno != NULL &&
        PyObject_RichCompareBool(env_err->myerrno, eintr_int, Py_EQ) > 0) {
        Py_DECREF(typ);
        Py_DECREF(val);
        Py_XDECREF(tb);
        return 1;
    }
    /* This silences any error set by PyObject_RichCompareBool() */
    PyErr_Restore(typ, val, tb);
    return 0;
}
