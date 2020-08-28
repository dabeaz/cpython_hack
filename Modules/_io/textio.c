/*
    An implementation of Text I/O as defined by PEP 3116 - "New I/O"

    Classes defined here: TextIOBase, IncrementalNewlineDecoder, TextIOWrapper.

    Written by Amaury Forgeot d'Arc and Antoine Pitrou
*/

#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "pycore_interp.h"        // PyInterpreterState.fs_codec
#include "pycore_object.h"
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "structmember.h"         // PyMemberDef
#include "_iomodule.h"

/*[clinic input]
module _io
class _io.IncrementalNewlineDecoder "nldecoder_object *" "&PyIncrementalNewlineDecoder_Type"
class _io.TextIOWrapper "textio *" "&TextIOWrapper_TYpe"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=2097a4fc85670c26]*/

_Py_IDENTIFIER(close);
_Py_IDENTIFIER(_dealloc_warn);
_Py_IDENTIFIER(fileno);
_Py_IDENTIFIER(flush);
_Py_IDENTIFIER(isatty);
_Py_IDENTIFIER(mode);
_Py_IDENTIFIER(name);
_Py_IDENTIFIER(raw);
_Py_IDENTIFIER(readable);
_Py_IDENTIFIER(readline);
_Py_IDENTIFIER(seek);
_Py_IDENTIFIER(seekable);
_Py_IDENTIFIER(tell);
_Py_IDENTIFIER(writable);

/* TextIOBase */

PyDoc_STRVAR(textiobase_doc,
    "Base class for text I/O.\n"
    "\n"
    "This class provides a character and line based interface to stream\n"
    "I/O. There is no readinto method because Python's character strings\n"
    "are immutable. There is no public constructor.\n"
    );

static PyObject *
_unsupported(const char *message)
{
    _PyIO_State *state = IO_STATE();
    if (state != NULL)
        PyErr_SetString(state->unsupported_operation, message);
    return NULL;
}

PyDoc_STRVAR(textiobase_detach_doc,
    "Separate the underlying buffer from the TextIOBase and return it.\n"
    "\n"
    "After the underlying buffer has been detached, the TextIO is in an\n"
    "unusable state.\n"
    );

static PyObject *
textiobase_detach(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _unsupported("detach");
}

PyDoc_STRVAR(textiobase_read_doc,
    "Read at most n characters from stream.\n"
    "\n"
    "Read from underlying buffer until we have n characters or we hit EOF.\n"
    "If n is negative or omitted, read until EOF.\n"
    );

static PyObject *
textiobase_read(PyObject *self, PyObject *args)
{
    return _unsupported("read");
}

PyDoc_STRVAR(textiobase_readline_doc,
    "Read until newline or EOF.\n"
    "\n"
    "Returns an empty string if EOF is hit immediately.\n"
    );

static PyObject *
textiobase_readline(PyObject *self, PyObject *args)
{
    return _unsupported("readline");
}

PyDoc_STRVAR(textiobase_write_doc,
    "Write string to stream.\n"
    "Returns the number of characters written (which is always equal to\n"
    "the length of the string).\n"
    );

static PyObject *
textiobase_write(PyObject *self, PyObject *args)
{
    return _unsupported("write");
}

PyDoc_STRVAR(textiobase_encoding_doc,
    "Encoding of the text stream.\n"
    "\n"
    "Subclasses should override.\n"
    );

static PyObject *
textiobase_encoding_get(PyObject *self, void *context)
{
    Py_RETURN_NONE;
}

PyDoc_STRVAR(textiobase_newlines_doc,
    "Line endings translated so far.\n"
    "\n"
    "Only line endings translated during reading are considered.\n"
    "\n"
    "Subclasses should override.\n"
    );

static PyObject *
textiobase_newlines_get(PyObject *self, void *context)
{
    Py_RETURN_NONE;
}

PyDoc_STRVAR(textiobase_errors_doc,
    "The error setting of the decoder or encoder.\n"
    "\n"
    "Subclasses should override.\n"
    );

static PyObject *
textiobase_errors_get(PyObject *self, void *context)
{
    Py_RETURN_NONE;
}


static PyMethodDef textiobase_methods[] = {
    {"detach", textiobase_detach, METH_NOARGS, textiobase_detach_doc},
    {"read", textiobase_read, METH_VARARGS, textiobase_read_doc},
    {"readline", textiobase_readline, METH_VARARGS, textiobase_readline_doc},
    {"write", textiobase_write, METH_VARARGS, textiobase_write_doc},
    {NULL, NULL}
};

static PyGetSetDef textiobase_getset[] = {
    {"encoding", (getter)textiobase_encoding_get, NULL, textiobase_encoding_doc},
    {"newlines", (getter)textiobase_newlines_get, NULL, textiobase_newlines_doc},
    {"errors", (getter)textiobase_errors_get, NULL, textiobase_errors_doc},
    {NULL}
};

PyTypeObject PyTextIOBase_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_io._TextIOBase",          /*tp_name*/
    0,                          /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    0,                          /*tp_dealloc*/
    0,                          /*tp_vectorcall_offset*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_as_async*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash */
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /*tp_flags*/
    textiobase_doc,             /* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    textiobase_methods,         /* tp_methods */
    0,                          /* tp_members */
    textiobase_getset,          /* tp_getset */
    &PyIOBase_Type,             /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0,                          /* tp_init */
    0,                          /* tp_alloc */
    0,                          /* tp_new */
    0,                          /* tp_free */
    0,                          /* tp_is_gc */
    0,                          /* tp_bases */
    0,                          /* tp_mro */
    0,                          /* tp_cache */
    0,                          /* tp_subclasses */
    0,                          /* tp_weaklist */
    0,                          /* tp_del */
    0,                          /* tp_version_tag */
    0,                          /* tp_finalize */
};


/* TextIOWrapper */

typedef PyObject *
        (*encodefunc_t)(PyObject *, PyObject *);

typedef struct
{
    PyObject_HEAD
    int ok; /* initialized? */
    int detached;
    PyObject *buffer;
    PyObject *encoding;
    PyObject *errors;
    char seekable;
    char telling;
    char finalizing;

  /* Cache raw object if it's a FileIO object */
    PyObject *raw;
    PyObject *weakreflist;
    PyObject *dict;
} textio;

/*[clinic input]
_io.TextIOWrapper.__init__
    buffer: object
    encoding: str(accept={str, NoneType}) = None
    errors: object = None
    newline: str(accept={str, NoneType}) = None
    line_buffering: bool(accept={int}) = False
    write_through: bool(accept={int}) = False

Character and line based layer over a BufferedIOBase object, buffer.

encoding gives the name of the encoding that the stream will be
decoded or encoded with. It defaults to locale.getpreferredencoding(False).

errors determines the strictness of encoding and decoding (see
help(codecs.Codec) or the documentation for codecs.register) and
defaults to "strict".

newline controls how line endings are handled. It can be None, '',
'\n', '\r', and '\r\n'.  It works as follows:

* On input, if newline is None, universal newlines mode is
  enabled. Lines in the input can end in '\n', '\r', or '\r\n', and
  these are translated into '\n' before being returned to the
  caller. If it is '', universal newline mode is enabled, but line
  endings are returned to the caller untranslated. If it has any of
  the other legal values, input lines are only terminated by the given
  string, and the line ending is returned to the caller untranslated.

* On output, if newline is None, any '\n' characters written are
  translated to the system default line separator, os.linesep. If
  newline is '' or '\n', no translation takes place. If newline is any
  of the other legal values, any '\n' characters written are translated
  to the given string.

If line_buffering is True, a call to flush is implied when a call to
write contains a newline character.
[clinic start generated code]*/

static int
_io_TextIOWrapper___init___impl(textio *self, PyObject *buffer,
                                const char *encoding, PyObject *errors,
                                const char *newline, int line_buffering,
                                int write_through)
/*[clinic end generated code: output=72267c0c01032ed2 input=77d8696d1a1f460b]*/
{
    PyObject *raw, *codec_info = NULL;
    PyObject *res;
    int r;

    self->ok = 0;
    self->detached = 0;

    Py_CLEAR(self->buffer);
    Py_CLEAR(self->encoding);
    Py_CLEAR(self->raw);
    self->encoding = PyUnicode_FromString("latin1");
    
    /* XXX: Failures beyond this point have the potential to leak elements
     * of the partially constructed object (like self->encoding)
     */
    self->buffer = buffer;
    Py_INCREF(buffer);

    
    /* Finished sorting out the codec details */
    Py_CLEAR(codec_info);
    if (Py_IS_TYPE(buffer, &PyBufferedReader_Type) ||
        Py_IS_TYPE(buffer, &PyBufferedWriter_Type) ||
        Py_IS_TYPE(buffer, &PyBufferedRandom_Type))
    {
        if (_PyObject_LookupAttrId(buffer, &PyId_raw, &raw) < 0)
            goto error;
        /* Cache the raw FileIO object to speed up 'closed' checks */
        if (raw != NULL) {
            if (Py_IS_TYPE(raw, &PyFileIO_Type))
                self->raw = raw;
            else
                Py_DECREF(raw);
        }
    }

    res = _PyObject_CallMethodIdNoArgs(buffer, &PyId_seekable);
    if (res == NULL)
        goto error;
    r = PyObject_IsTrue(res);
    Py_DECREF(res);
    if (r < 0)
        goto error;
    self->seekable = self->telling = r;

    self->ok = 1;
    return 0;

  error:
    Py_XDECREF(codec_info);
    return -1;
}

static int
textiowrapper_clear(textio *self)
{
    self->ok = 0;
    Py_CLEAR(self->buffer);
    Py_CLEAR(self->encoding);
    Py_CLEAR(self->raw);
    Py_CLEAR(self->dict);
    return 0;
}

static void
textiowrapper_dealloc(textio *self)
{
    self->finalizing = 1;
    if (_PyIOBase_finalize((PyObject *) self) < 0)
        return;
    self->ok = 0;
    _PyObject_GC_UNTRACK(self);
    if (self->weakreflist != NULL)
        PyObject_ClearWeakRefs((PyObject *)self);
    textiowrapper_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static int
textiowrapper_traverse(textio *self, visitproc visit, void *arg)
{
    Py_VISIT(self->buffer);
    Py_VISIT(self->encoding);
    Py_VISIT(self->raw);
    Py_VISIT(self->dict);
    return 0;
}

static PyObject *
textiowrapper_closed_get(textio *self, void *context);

/* This macro takes some shortcuts to make the common case faster. */
#define CHECK_CLOSED(self) \
    do { \
        int r; \
        PyObject *_res; \
        if (Py_IS_TYPE(self, &PyTextIOWrapper_Type)) { \
            if (self->raw != NULL) \
                r = _PyFileIO_closed(self->raw); \
            else { \
                _res = textiowrapper_closed_get(self, NULL); \
                if (_res == NULL) \
                    return NULL; \
                r = PyObject_IsTrue(_res); \
                Py_DECREF(_res); \
                if (r < 0) \
                    return NULL; \
            } \
            if (r > 0) { \
                PyErr_SetString(PyExc_ValueError, \
                                "I/O operation on closed file."); \
                return NULL; \
            } \
        } \
        else if (_PyIOBase_check_closed((PyObject *)self, Py_True) == NULL) \
            return NULL; \
    } while (0)

#define CHECK_INITIALIZED(self) \
    if (self->ok <= 0) { \
        PyErr_SetString(PyExc_ValueError, \
            "I/O operation on uninitialized object"); \
        return NULL; \
    }

#define CHECK_ATTACHED(self) \
    CHECK_INITIALIZED(self); \
    if (self->detached) { \
        PyErr_SetString(PyExc_ValueError, \
             "underlying buffer has been detached"); \
        return NULL; \
    }

#define CHECK_ATTACHED_INT(self) \
    if (self->ok <= 0) { \
        PyErr_SetString(PyExc_ValueError, \
            "I/O operation on uninitialized object"); \
        return -1; \
    } else if (self->detached) { \
        PyErr_SetString(PyExc_ValueError, \
             "underlying buffer has been detached"); \
        return -1; \
    }


/*[clinic input]
_io.TextIOWrapper.detach
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_detach_impl(textio *self)
/*[clinic end generated code: output=7ba3715cd032d5f2 input=e5a71fbda9e1d9f9]*/
{
    PyObject *buffer, *res;
    CHECK_ATTACHED(self);
    res = PyObject_CallMethodNoArgs((PyObject *)self, _PyIO_str_flush);
    if (res == NULL)
        return NULL;
    Py_DECREF(res);
    buffer = self->buffer;
    self->buffer = NULL;
    self->detached = 1;
    return buffer;
}

/*[clinic input]
_io.TextIOWrapper.write
    text: unicode
    /
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_write_impl(textio *self, PyObject *text)
/*[clinic end generated code: output=d2deb0d50771fcec input=fdf19153584a0e44]*/
{
  PyObject *bytes, *result;
  CHECK_ATTACHED(self);
  CHECK_CLOSED(self);
  bytes = PyUnicode_AsBytes(text);
  if (bytes == NULL) {
    return NULL;
  }
  do {
    result = PyObject_CallMethodOneArg(self->buffer, _PyIO_str_write, bytes);
  } while (result == NULL && _PyIO_trap_eintr());
  Py_DECREF(bytes);
  return result;
}

/*[clinic input]
_io.TextIOWrapper.read
    size as n: Py_ssize_t(accept={int, NoneType}) = -1
    /
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_read_impl(textio *self, Py_ssize_t n)
/*[clinic end generated code: output=7e651ce6cc6a25a6 input=123eecbfe214aeb8]*/
{
  PyObject *bytes, *nobj;
  char *s;
  Py_ssize_t len;


  CHECK_ATTACHED(self);
  CHECK_CLOSED(self);

  nobj = PyLong_FromSsize_t(n);
  if (!nobj) {
    return NULL;
  }
  bytes = PyObject_CallMethodOneArg(self->buffer, _PyIO_str_read, nobj);
  Py_DECREF(nobj);
  if (bytes) {
    if (PyBytes_AsStringAndSize(bytes, &s, &len) < 0) {
      return NULL;
    }
    return PyUnicode_FromStringAndSize(s, len);
  }
  return NULL;
}


/* NOTE: `end` must point to the real end of the Py_UCS4 storage,
   that is to the NUL character. Otherwise the function will produce
   incorrect results. */
static const char *
find_control_char(int kind, const char *s, const char *end, Py_UCS4 ch)
{
    if (kind == PyUnicode_1BYTE_KIND) {
        assert(ch < 256);
        return (char *) memchr((const void *) s, (char) ch, end - s);
    }
    for (;;) {
        while (PyUnicode_READ(kind, s, 0) > ch)
            s += kind;
        if (PyUnicode_READ(kind, s, 0) == ch)
            return s;
        if (s == end)
            return NULL;
        s += kind;
    }
}

Py_ssize_t
_PyIO_find_line_ending(
    int translated, int universal, PyObject *readnl,
    int kind, const char *start, const char *end, Py_ssize_t *consumed)
{
    Py_ssize_t len = (end - start)/kind;

    if (1) {
        /* Newlines are already translated, only search for \n */
        const char *pos = find_control_char(kind, start, end, '\n');
        if (pos != NULL)
            return (pos - start)/kind + 1;
        else {
            *consumed = len;
            return -1;
        }
    }
}

static PyObject *
_textiowrapper_readline(textio *self, Py_ssize_t limit)
{
  PyObject *bytes;
  char *s;
  Py_ssize_t len;
  
  CHECK_ATTACHED(self);
  CHECK_CLOSED(self);
  bytes = _PyObject_CallMethodId(self->buffer, &PyId_readline, "i", limit);
  if (bytes == NULL) {
    return NULL;
  }
  if (PyBytes_AsStringAndSize(bytes, &s, &len) < 0) {
    return NULL;
  }
  return PyUnicode_FromStringAndSize(s, len);
}

/*[clinic input]
_io.TextIOWrapper.readline
    size: Py_ssize_t = -1
    /
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_readline_impl(textio *self, Py_ssize_t size)
/*[clinic end generated code: output=344afa98804e8b25 input=56c7172483b36db6]*/
{
    CHECK_ATTACHED(self);
    return _textiowrapper_readline(self, size);
}

/*[clinic input]
_io.TextIOWrapper.seek
    cookie as cookieObj: object
    whence: int = 0
    /
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_seek_impl(textio *self, PyObject *cookieObj, int whence)
/*[clinic end generated code: output=0a15679764e2d04d input=0458abeb3d7842be]*/
{
  CHECK_ATTACHED(self);
  CHECK_CLOSED(self);
  if (!self->seekable) {
    return NULL;
  }
  
  return _PyObject_CallMethodId(self->buffer, &PyId_seek, "ii", PyLong_AsLong(cookieObj), whence);
}

/*[clinic input]
_io.TextIOWrapper.tell
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_tell_impl(textio *self)
/*[clinic end generated code: output=4f168c08bf34ad5f input=9a2caf88c24f9ddf]*/
{
  PyObject *result;
  CHECK_ATTACHED(self);
  CHECK_CLOSED(self);

    if (!self->seekable) {
        _unsupported("underlying stream is not seekable");
	return NULL;
    }
    result = _PyObject_CallMethodIdNoArgs(self->buffer, &PyId_tell);
    return result;
}

/*[clinic input]
_io.TextIOWrapper.truncate
    pos: object = None
    /
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_truncate_impl(textio *self, PyObject *pos)
/*[clinic end generated code: output=90ec2afb9bb7745f input=56ec8baa65aea377]*/
{
    PyObject *res;

    CHECK_ATTACHED(self)
    res = PyObject_CallMethodNoArgs((PyObject *)self->buffer, _PyIO_str_flush);
    if (res == NULL)
        return NULL;
    Py_DECREF(res);
    return PyObject_CallMethodOneArg(self->buffer, _PyIO_str_truncate, pos);
}

static PyObject *
textiowrapper_repr(textio *self)
{
    PyObject *nameobj, *modeobj, *res, *s;
    int status;

    CHECK_INITIALIZED(self);

    res = PyUnicode_FromString("<_io.TextIOWrapper");
    if (res == NULL)
        return NULL;

    status = Py_ReprEnter((PyObject *)self);
    if (status != 0) {
        if (status > 0) {
            PyErr_Format(PyExc_RuntimeError,
                         "reentrant call inside %s.__repr__",
                         Py_TYPE(self)->tp_name);
        }
        goto error;
    }
    if (_PyObject_LookupAttrId((PyObject *) self, &PyId_name, &nameobj) < 0) {
        if (!PyErr_ExceptionMatches(PyExc_ValueError)) {
            goto error;
        }
        /* Ignore ValueError raised if the underlying stream was detached */
        PyErr_Clear();
    }
    if (nameobj != NULL) {
        s = PyUnicode_FromFormat(" name=%R", nameobj);
        Py_DECREF(nameobj);
        if (s == NULL)
            goto error;
        PyUnicode_AppendAndDel(&res, s);
        if (res == NULL)
            goto error;
    }
    if (_PyObject_LookupAttrId((PyObject *) self, &PyId_mode, &modeobj) < 0) {
        goto error;
    }
    if (modeobj != NULL) {
        s = PyUnicode_FromFormat(" mode=%R", modeobj);
        Py_DECREF(modeobj);
        if (s == NULL)
            goto error;
        PyUnicode_AppendAndDel(&res, s);
        if (res == NULL)
            goto error;
    }
    s = PyUnicode_FromFormat("%U encoding=%R>",
                             res, self->encoding);
    Py_DECREF(res);
    if (status == 0) {
        Py_ReprLeave((PyObject *)self);
    }
    return s;

  error:
    Py_XDECREF(res);
    if (status == 0) {
        Py_ReprLeave((PyObject *)self);
    }
    return NULL;
}


/* Inquiries */

/*[clinic input]
_io.TextIOWrapper.fileno
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_fileno_impl(textio *self)
/*[clinic end generated code: output=21490a4c3da13e6c input=c488ca83d0069f9b]*/
{
    CHECK_ATTACHED(self);
    return _PyObject_CallMethodIdNoArgs(self->buffer, &PyId_fileno);
}

/*[clinic input]
_io.TextIOWrapper.seekable
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_seekable_impl(textio *self)
/*[clinic end generated code: output=ab223dbbcffc0f00 input=8b005ca06e1fca13]*/
{
    CHECK_ATTACHED(self);
    return _PyObject_CallMethodIdNoArgs(self->buffer, &PyId_seekable);
}

/*[clinic input]
_io.TextIOWrapper.readable
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_readable_impl(textio *self)
/*[clinic end generated code: output=72ff7ba289a8a91b input=0704ea7e01b0d3eb]*/
{
    CHECK_ATTACHED(self);
    return _PyObject_CallMethodIdNoArgs(self->buffer, &PyId_readable);
}

/*[clinic input]
_io.TextIOWrapper.writable
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_writable_impl(textio *self)
/*[clinic end generated code: output=a728c71790d03200 input=c41740bc9d8636e8]*/
{
    CHECK_ATTACHED(self);
    return _PyObject_CallMethodIdNoArgs(self->buffer, &PyId_writable);
}

/*[clinic input]
_io.TextIOWrapper.isatty
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_isatty_impl(textio *self)
/*[clinic end generated code: output=12be1a35bace882e input=fb68d9f2c99bbfff]*/
{
    CHECK_ATTACHED(self);
    return _PyObject_CallMethodIdNoArgs(self->buffer, &PyId_isatty);
}

/*[clinic input]
_io.TextIOWrapper.flush
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_flush_impl(textio *self)
/*[clinic end generated code: output=59de9165f9c2e4d2 input=928c60590694ab85]*/
{
    CHECK_ATTACHED(self);
    CHECK_CLOSED(self);
    return _PyObject_CallMethodIdNoArgs(self->buffer, &PyId_flush);
}

/*[clinic input]
_io.TextIOWrapper.close
[clinic start generated code]*/

static PyObject *
_io_TextIOWrapper_close_impl(textio *self)
/*[clinic end generated code: output=056ccf8b4876e4f4 input=9c2114315eae1948]*/
{
    PyObject *res;
    int r;
    CHECK_ATTACHED(self);

    res = textiowrapper_closed_get(self, NULL);
    if (res == NULL)
        return NULL;
    r = PyObject_IsTrue(res);
    Py_DECREF(res);
    if (r < 0)
        return NULL;

    if (r > 0) {
        Py_RETURN_NONE; /* stream already closed */
    }
    else {
        PyObject *exc = NULL, *val, *tb;
        if (self->finalizing) {
            res = _PyObject_CallMethodIdOneArg(self->buffer,
                                              &PyId__dealloc_warn,
                                              (PyObject *)self);
            if (res)
                Py_DECREF(res);
            else
                PyErr_Clear();
        }
        res = _PyObject_CallMethodIdNoArgs((PyObject *)self, &PyId_flush);
        if (res == NULL)
            PyErr_Fetch(&exc, &val, &tb);
        else
            Py_DECREF(res);

        res = _PyObject_CallMethodIdNoArgs(self->buffer, &PyId_close);
        if (exc != NULL) {
            _PyErr_ChainExceptions(exc, val, tb);
            Py_CLEAR(res);
        }
        return res;
    }
}

static PyObject *
textiowrapper_iternext(textio *self)
{
    PyObject *line;

    CHECK_ATTACHED(self);

    self->telling = 0;
    if (Py_IS_TYPE(self, &PyTextIOWrapper_Type)) {
        /* Skip method call overhead for speed */
        line = _textiowrapper_readline(self, -1);
    }
    else {
        line = PyObject_CallMethodNoArgs((PyObject *)self,
                                          _PyIO_str_readline);
        if (line && !PyUnicode_Check(line)) {
            PyErr_Format(PyExc_OSError,
                         "readline() should have returned a str object, "
                         "not '%.200s'", Py_TYPE(line)->tp_name);
            Py_DECREF(line);
            return NULL;
        }
    }

    if (line == NULL || PyUnicode_READY(line) == -1)
        return NULL;

    if (PyUnicode_GET_LENGTH(line) == 0) {
        /* Reached EOF or would have blocked */
        Py_DECREF(line);
        self->telling = self->seekable;
        return NULL;
    }

    return line;
}

static PyObject *
textiowrapper_name_get(textio *self, void *context)
{
    CHECK_ATTACHED(self);
    return _PyObject_GetAttrId(self->buffer, &PyId_name);
}

static PyObject *
textiowrapper_closed_get(textio *self, void *context)
{
    CHECK_ATTACHED(self);
    return PyObject_GetAttr(self->buffer, _PyIO_str_closed);
}

static PyObject *
textiowrapper_newlines_get(textio *self, void *context)
{
  Py_RETURN_NONE;
}

static PyObject *
textiowrapper_errors_get(textio *self, void *context)
{
  Py_RETURN_NONE;
}


PyDoc_STRVAR(_io_TextIOWrapper___init____doc__,
"TextIOWrapper(buffer, encoding=None, errors=None, newline=None,\n"
"              line_buffering=False, write_through=False)\n"
"--\n"
"\n"
"Character and line based layer over a BufferedIOBase object, buffer.\n"
"\n"
"encoding gives the name of the encoding that the stream will be\n"
"decoded or encoded with. It defaults to locale.getpreferredencoding(False).\n"
"\n"
"errors determines the strictness of encoding and decoding (see\n"
"help(codecs.Codec) or the documentation for codecs.register) and\n"
"defaults to \"strict\".\n"
"\n"
"newline controls how line endings are handled. It can be None, \'\',\n"
"\'\\n\', \'\\r\', and \'\\r\\n\'.  It works as follows:\n"
"\n"
"* On input, if newline is None, universal newlines mode is\n"
"  enabled. Lines in the input can end in \'\\n\', \'\\r\', or \'\\r\\n\', and\n"
"  these are translated into \'\\n\' before being returned to the\n"
"  caller. If it is \'\', universal newline mode is enabled, but line\n"
"  endings are returned to the caller untranslated. If it has any of\n"
"  the other legal values, input lines are only terminated by the given\n"
"  string, and the line ending is returned to the caller untranslated.\n"
"\n"
"* On output, if newline is None, any \'\\n\' characters written are\n"
"  translated to the system default line separator, os.linesep. If\n"
"  newline is \'\' or \'\\n\', no translation takes place. If newline is any\n"
"  of the other legal values, any \'\\n\' characters written are translated\n"
"  to the given string.\n"
"\n"
"If line_buffering is True, a call to flush is implied when a call to\n"
"write contains a newline character.");

static int
_io_TextIOWrapper___init___impl(textio *self, PyObject *buffer,
                                const char *encoding, PyObject *errors,
                                const char *newline, int line_buffering,
                                int write_through);

static int
_io_TextIOWrapper___init__(PyObject *self, PyObject *args, PyObject *kwargs)
{
    int return_value = -1;
    static const char * const _keywords[] = {"buffer", "encoding", "errors", "newline", "line_buffering", "write_through", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "TextIOWrapper", 0};
    PyObject *argsbuf[6];
    PyObject * const *fastargs;
    Py_ssize_t nargs = PyTuple_GET_SIZE(args);
    Py_ssize_t noptargs = nargs + (kwargs ? PyDict_GET_SIZE(kwargs) : 0) - 1;
    PyObject *buffer;
    const char *encoding = NULL;
    PyObject *errors = Py_None;
    const char *newline = NULL;
    int line_buffering = 0;
    int write_through = 0;

    fastargs = _PyArg_UnpackKeywords(_PyTuple_CAST(args)->ob_item, nargs, kwargs, NULL, &_parser, 1, 6, 0, argsbuf);
    if (!fastargs) {
        goto exit;
    }
    buffer = fastargs[0];
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (fastargs[1]) {
        if (fastargs[1] == Py_None) {
            encoding = NULL;
        }
        else if (PyUnicode_Check(fastargs[1])) {
            Py_ssize_t encoding_length;
            encoding = PyUnicode_AsCharAndSize(fastargs[1], &encoding_length);
            if (encoding == NULL) {
                goto exit;
            }
            if (strlen(encoding) != (size_t)encoding_length) {
                PyErr_SetString(PyExc_ValueError, "embedded null character");
                goto exit;
            }
        }
        else {
            _PyArg_BadArgument("TextIOWrapper", "argument 'encoding'", "str or None", fastargs[1]);
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    if (fastargs[2]) {
        errors = fastargs[2];
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    if (fastargs[3]) {
        if (fastargs[3] == Py_None) {
            newline = NULL;
        }
        else if (PyUnicode_Check(fastargs[3])) {
            Py_ssize_t newline_length;
            newline = PyUnicode_AsCharAndSize(fastargs[3], &newline_length);
            if (newline == NULL) {
                goto exit;
            }
            if (strlen(newline) != (size_t)newline_length) {
                PyErr_SetString(PyExc_ValueError, "embedded null character");
                goto exit;
            }
        }
        else {
            _PyArg_BadArgument("TextIOWrapper", "argument 'newline'", "str or None", fastargs[3]);
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    if (fastargs[4]) {
        line_buffering = _PyLong_AsInt(fastargs[4]);
        if (line_buffering == -1 && PyErr_Occurred()) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    write_through = _PyLong_AsInt(fastargs[5]);
    if (write_through == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional_pos:
    return_value = _io_TextIOWrapper___init___impl((textio *)self, buffer, encoding, errors, newline, line_buffering, write_through);

exit:
    return return_value;
}

PyDoc_STRVAR(_io_TextIOWrapper_detach__doc__,
"detach($self, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_DETACH_METHODDEF    \
    {"detach", (PyCFunction)_io_TextIOWrapper_detach, METH_NOARGS, _io_TextIOWrapper_detach__doc__},

static PyObject *
_io_TextIOWrapper_detach_impl(textio *self);

static PyObject *
_io_TextIOWrapper_detach(textio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_TextIOWrapper_detach_impl(self);
}

PyDoc_STRVAR(_io_TextIOWrapper_write__doc__,
"write($self, text, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_WRITE_METHODDEF    \
    {"write", (PyCFunction)_io_TextIOWrapper_write, METH_O, _io_TextIOWrapper_write__doc__},

static PyObject *
_io_TextIOWrapper_write_impl(textio *self, PyObject *text);

static PyObject *
_io_TextIOWrapper_write(textio *self, PyObject *arg)
{
    PyObject *return_value = NULL;
    PyObject *text;

    if (!PyUnicode_Check(arg)) {
        _PyArg_BadArgument("write", "argument", "str", arg);
        goto exit;
    }
    if (PyUnicode_READY(arg) == -1) {
        goto exit;
    }
    text = arg;
    return_value = _io_TextIOWrapper_write_impl(self, text);

exit:
    return return_value;
}

PyDoc_STRVAR(_io_TextIOWrapper_read__doc__,
"read($self, size=-1, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_READ_METHODDEF    \
    {"read", (PyCFunction)(void(*)(void))_io_TextIOWrapper_read, METH_FASTCALL, _io_TextIOWrapper_read__doc__},

static PyObject *
_io_TextIOWrapper_read_impl(textio *self, Py_ssize_t n);

static PyObject *
_io_TextIOWrapper_read(textio *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_ssize_t n = -1;

    if (!_PyArg_CheckPositional("read", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    if (!_Py_convert_optional_to_ssize_t(args[0], &n)) {
        goto exit;
    }
skip_optional:
    return_value = _io_TextIOWrapper_read_impl(self, n);

exit:
    return return_value;
}

PyDoc_STRVAR(_io_TextIOWrapper_readline__doc__,
"readline($self, size=-1, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_READLINE_METHODDEF    \
    {"readline", (PyCFunction)(void(*)(void))_io_TextIOWrapper_readline, METH_FASTCALL, _io_TextIOWrapper_readline__doc__},

static PyObject *
_io_TextIOWrapper_readline_impl(textio *self, Py_ssize_t size);

static PyObject *
_io_TextIOWrapper_readline(textio *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_ssize_t size = -1;

    if (!_PyArg_CheckPositional("readline", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(args[0]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        size = ival;
    }
skip_optional:
    return_value = _io_TextIOWrapper_readline_impl(self, size);

exit:
    return return_value;
}

PyDoc_STRVAR(_io_TextIOWrapper_seek__doc__,
"seek($self, cookie, whence=0, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_SEEK_METHODDEF    \
    {"seek", (PyCFunction)(void(*)(void))_io_TextIOWrapper_seek, METH_FASTCALL, _io_TextIOWrapper_seek__doc__},

static PyObject *
_io_TextIOWrapper_seek_impl(textio *self, PyObject *cookieObj, int whence);

static PyObject *
_io_TextIOWrapper_seek(textio *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *cookieObj;
    int whence = 0;

    if (!_PyArg_CheckPositional("seek", nargs, 1, 2)) {
        goto exit;
    }
    cookieObj = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    whence = _PyLong_AsInt(args[1]);
    if (whence == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _io_TextIOWrapper_seek_impl(self, cookieObj, whence);

exit:
    return return_value;
}

PyDoc_STRVAR(_io_TextIOWrapper_tell__doc__,
"tell($self, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_TELL_METHODDEF    \
    {"tell", (PyCFunction)_io_TextIOWrapper_tell, METH_NOARGS, _io_TextIOWrapper_tell__doc__},

static PyObject *
_io_TextIOWrapper_tell_impl(textio *self);

static PyObject *
_io_TextIOWrapper_tell(textio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_TextIOWrapper_tell_impl(self);
}

PyDoc_STRVAR(_io_TextIOWrapper_truncate__doc__,
"truncate($self, pos=None, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_TRUNCATE_METHODDEF    \
    {"truncate", (PyCFunction)(void(*)(void))_io_TextIOWrapper_truncate, METH_FASTCALL, _io_TextIOWrapper_truncate__doc__},

static PyObject *
_io_TextIOWrapper_truncate_impl(textio *self, PyObject *pos);

static PyObject *
_io_TextIOWrapper_truncate(textio *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *pos = Py_None;

    if (!_PyArg_CheckPositional("truncate", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    pos = args[0];
skip_optional:
    return_value = _io_TextIOWrapper_truncate_impl(self, pos);

exit:
    return return_value;
}

PyDoc_STRVAR(_io_TextIOWrapper_fileno__doc__,
"fileno($self, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_FILENO_METHODDEF    \
    {"fileno", (PyCFunction)_io_TextIOWrapper_fileno, METH_NOARGS, _io_TextIOWrapper_fileno__doc__},

static PyObject *
_io_TextIOWrapper_fileno_impl(textio *self);

static PyObject *
_io_TextIOWrapper_fileno(textio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_TextIOWrapper_fileno_impl(self);
}

PyDoc_STRVAR(_io_TextIOWrapper_seekable__doc__,
"seekable($self, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_SEEKABLE_METHODDEF    \
    {"seekable", (PyCFunction)_io_TextIOWrapper_seekable, METH_NOARGS, _io_TextIOWrapper_seekable__doc__},

static PyObject *
_io_TextIOWrapper_seekable_impl(textio *self);

static PyObject *
_io_TextIOWrapper_seekable(textio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_TextIOWrapper_seekable_impl(self);
}

PyDoc_STRVAR(_io_TextIOWrapper_readable__doc__,
"readable($self, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_READABLE_METHODDEF    \
    {"readable", (PyCFunction)_io_TextIOWrapper_readable, METH_NOARGS, _io_TextIOWrapper_readable__doc__},

static PyObject *
_io_TextIOWrapper_readable_impl(textio *self);

static PyObject *
_io_TextIOWrapper_readable(textio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_TextIOWrapper_readable_impl(self);
}

PyDoc_STRVAR(_io_TextIOWrapper_writable__doc__,
"writable($self, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_WRITABLE_METHODDEF    \
    {"writable", (PyCFunction)_io_TextIOWrapper_writable, METH_NOARGS, _io_TextIOWrapper_writable__doc__},

static PyObject *
_io_TextIOWrapper_writable_impl(textio *self);

static PyObject *
_io_TextIOWrapper_writable(textio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_TextIOWrapper_writable_impl(self);
}

PyDoc_STRVAR(_io_TextIOWrapper_isatty__doc__,
"isatty($self, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_ISATTY_METHODDEF    \
    {"isatty", (PyCFunction)_io_TextIOWrapper_isatty, METH_NOARGS, _io_TextIOWrapper_isatty__doc__},

static PyObject *
_io_TextIOWrapper_isatty_impl(textio *self);

static PyObject *
_io_TextIOWrapper_isatty(textio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_TextIOWrapper_isatty_impl(self);
}

PyDoc_STRVAR(_io_TextIOWrapper_flush__doc__,
"flush($self, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_FLUSH_METHODDEF    \
    {"flush", (PyCFunction)_io_TextIOWrapper_flush, METH_NOARGS, _io_TextIOWrapper_flush__doc__},

static PyObject *
_io_TextIOWrapper_flush_impl(textio *self);

static PyObject *
_io_TextIOWrapper_flush(textio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_TextIOWrapper_flush_impl(self);
}

PyDoc_STRVAR(_io_TextIOWrapper_close__doc__,
"close($self, /)\n"
"--\n"
"\n");

#define _IO_TEXTIOWRAPPER_CLOSE_METHODDEF    \
    {"close", (PyCFunction)_io_TextIOWrapper_close, METH_NOARGS, _io_TextIOWrapper_close__doc__},

static PyObject *
_io_TextIOWrapper_close_impl(textio *self);

static PyObject *
_io_TextIOWrapper_close(textio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_TextIOWrapper_close_impl(self);
}
/*[clinic end generated code: output=2604c8f3a45b9a03 input=a9049054013a1b77]*/

static PyMethodDef textiowrapper_methods[] = {
    _IO_TEXTIOWRAPPER_DETACH_METHODDEF
    _IO_TEXTIOWRAPPER_WRITE_METHODDEF
    _IO_TEXTIOWRAPPER_READ_METHODDEF
    _IO_TEXTIOWRAPPER_READLINE_METHODDEF
    _IO_TEXTIOWRAPPER_FLUSH_METHODDEF
    _IO_TEXTIOWRAPPER_CLOSE_METHODDEF

    _IO_TEXTIOWRAPPER_FILENO_METHODDEF
    _IO_TEXTIOWRAPPER_SEEKABLE_METHODDEF
    _IO_TEXTIOWRAPPER_READABLE_METHODDEF
    _IO_TEXTIOWRAPPER_WRITABLE_METHODDEF
    _IO_TEXTIOWRAPPER_ISATTY_METHODDEF

    _IO_TEXTIOWRAPPER_SEEK_METHODDEF
    _IO_TEXTIOWRAPPER_TELL_METHODDEF
    _IO_TEXTIOWRAPPER_TRUNCATE_METHODDEF
    {NULL, NULL}
};

static PyMemberDef textiowrapper_members[] = {
    {"encoding", T_OBJECT, offsetof(textio, encoding), READONLY},
    {"buffer", T_OBJECT, offsetof(textio, buffer), READONLY},
    {"_finalizing", T_BOOL, offsetof(textio, finalizing), 0},
    {NULL}
};

static PyGetSetDef textiowrapper_getset[] = {
    {"name", (getter)textiowrapper_name_get, NULL, NULL},
    {"closed", (getter)textiowrapper_closed_get, NULL, NULL},
/*    {"mode", (getter)TextIOWrapper_mode_get, NULL, NULL},
*/
    {"newlines", (getter)textiowrapper_newlines_get, NULL, NULL},
    {"errors", (getter)textiowrapper_errors_get, NULL, NULL},
    {NULL}
};

PyTypeObject PyTextIOWrapper_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_io.TextIOWrapper",        /*tp_name*/
    sizeof(textio), /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor)textiowrapper_dealloc, /*tp_dealloc*/
    0,                          /*tp_vectorcall_offset*/
    0,                          /*tp_getattr*/
    0,                          /*tps_etattr*/
    0,                          /*tp_as_async*/
    (reprfunc)textiowrapper_repr,/*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash */
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE
        | Py_TPFLAGS_HAVE_GC,   /*tp_flags*/
    _io_TextIOWrapper___init____doc__, /* tp_doc */
    (traverseproc)textiowrapper_traverse, /* tp_traverse */
    (inquiry)textiowrapper_clear, /* tp_clear */
    0,                          /* tp_richcompare */
    offsetof(textio, weakreflist), /*tp_weaklistoffset*/
    0,                          /* tp_iter */
    (iternextfunc)textiowrapper_iternext, /* tp_iternext */
    textiowrapper_methods,      /* tp_methods */
    textiowrapper_members,      /* tp_members */
    textiowrapper_getset,       /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    offsetof(textio, dict), /*tp_dictoffset*/
    _io_TextIOWrapper___init__, /* tp_init */
    0,                          /* tp_alloc */
    PyType_GenericNew,          /* tp_new */
    0,                          /* tp_free */
    0,                          /* tp_is_gc */
    0,                          /* tp_bases */
    0,                          /* tp_mro */
    0,                          /* tp_cache */
    0,                          /* tp_subclasses */
    0,                          /* tp_weaklist */
    0,                          /* tp_del */
    0,                          /* tp_version_tag */
    0,                          /* tp_finalize */
};
