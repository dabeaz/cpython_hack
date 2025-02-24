/*
    An implementation of the I/O abstract base classes hierarchy
    as defined by PEP 3116 - "New I/O"

    Classes defined here: IOBase, IOBase.

    Written by Amaury Forgeot d'Arc and Antoine Pitrou
*/


#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "pycore_object.h"
#include <stddef.h>               // offsetof()
#include "_iomodule.h"

/*[clinic input]
module _io
class _io._IOBase "PyObject *" "&PyIOBase_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=d29a4d076c2b211c]*/

/*
 * IOBase class, an abstract class
 */

typedef struct {
    PyObject_HEAD

    PyObject *dict;
    PyObject *weakreflist;
} iobase;

PyDoc_STRVAR(iobase_doc,
    "The abstract base class for all I/O classes, acting on streams of\n"
    "bytes. There is no public constructor.\n"
    "\n"
    "This class provides dummy implementations for many methods that\n"
    "derived classes can override selectively; the default implementations\n"
    "represent a file that cannot be read, written or seeked.\n"
    "\n"
    "Even though IOBase does not declare read, readinto, or write because\n"
    "their signatures will vary, implementations and clients should\n"
    "consider those methods part of the interface. Also, implementations\n"
    "may raise UnsupportedOperation when operations they do not support are\n"
    "called.\n"
    "\n"
    "The basic type used for binary data read from or written to a file is\n"
    "bytes. Other bytes-like objects are accepted as method arguments too.\n"
    "In some cases (such as readinto), a writable object is required. Text\n"
    "I/O classes work with str data.\n"
    "\n"
    "Note that calling any method (except additional calls to close(),\n"
    "which are ignored) on a closed stream should raise a ValueError.\n"
    "\n"
    "IOBase (and its subclasses) support the iterator protocol, meaning\n"
    "that an IOBase object can be iterated over yielding the lines in a\n"
    "stream.\n"
    "\n"
    "IOBase also supports the :keyword:`with` statement. In this example,\n"
    "fp is closed after the suite of the with statement is complete:\n"
    "\n"
    "with open('spam.txt', 'r') as fp:\n"
    "    fp.write('Spam and eggs!')\n");

/* Use this macro whenever you want to check the internal `closed` status
   of the IOBase object rather than the virtual `closed` attribute as returned
   by whatever subclass. */

_Py_IDENTIFIER(__IOBase_closed);
_Py_IDENTIFIER(read);


/* Internal methods */
static PyObject *
iobase_unsupported(const char *message)
{
    _PyIO_State *state = IO_STATE();
    if (state != NULL)
        PyErr_SetString(state->unsupported_operation, message);
    return NULL;
}

/* Positioning */

PyDoc_STRVAR(iobase_seek_doc,
    "Change stream position.\n"
    "\n"
    "Change the stream position to the given byte offset. The offset is\n"
    "interpreted relative to the position indicated by whence.  Values\n"
    "for whence are:\n"
    "\n"
    "* 0 -- start of stream (the default); offset should be zero or positive\n"
    "* 1 -- current stream position; offset may be negative\n"
    "* 2 -- end of stream; offset is usually negative\n"
    "\n"
    "Return the new absolute position.");

static PyObject *
iobase_seek(PyObject *self, PyObject *args)
{
    return iobase_unsupported("seek");
}

/*[clinic input]
_io._IOBase.tell

Return current stream position.
[clinic start generated code]*/

static PyObject *
_io__IOBase_tell_impl(PyObject *self)
/*[clinic end generated code: output=89a1c0807935abe2 input=04e615fec128801f]*/
{
    _Py_IDENTIFIER(seek);

    return _PyObject_CallMethodId(self, &PyId_seek, "ii", 0, 1);
}

PyDoc_STRVAR(iobase_truncate_doc,
    "Truncate file to size bytes.\n"
    "\n"
    "File pointer is left unchanged.  Size defaults to the current IO\n"
    "position as reported by tell().  Returns the new size.");

static PyObject *
iobase_truncate(PyObject *self, PyObject *args)
{
    return iobase_unsupported("truncate");
}

static int
iobase_is_closed(PyObject *self)
{
    PyObject *res;
    int ret;
    /* This gets the derived attribute, which is *not* __IOBase_closed
       in most cases! */
    ret = _PyObject_LookupAttrId(self, &PyId___IOBase_closed, &res);
    Py_XDECREF(res);
    return ret;
}

/* Flush and close methods */

/*[clinic input]
_io._IOBase.flush

Flush write buffers, if applicable.

This is not implemented for read-only and non-blocking streams.
[clinic start generated code]*/

static PyObject *
_io__IOBase_flush_impl(PyObject *self)
/*[clinic end generated code: output=7cef4b4d54656a3b input=773be121abe270aa]*/
{
    /* XXX Should this return the number of bytes written??? */
    int closed = iobase_is_closed(self);

    if (!closed) {
        Py_RETURN_NONE;
    }
    if (closed > 0) {
        PyErr_SetString(PyExc_ValueError, "I/O operation on closed file.");
    }
    return NULL;
}

static PyObject *
iobase_closed_get(PyObject *self, void *context)
{
    int closed = iobase_is_closed(self);
    if (closed < 0) {
        return NULL;
    }
    return PyBool_FromLong(closed);
}

static int
iobase_check_closed(PyObject *self)
{
    PyObject *res;
    int closed;
    /* This gets the derived attribute, which is *not* __IOBase_closed
       in most cases! */
    closed = _PyObject_LookupAttr(self, _PyIO_str_closed, &res);
    if (closed > 0) {
        closed = PyObject_IsTrue(res);
        Py_DECREF(res);
        if (closed > 0) {
            PyErr_SetString(PyExc_ValueError, "I/O operation on closed file.");
            return -1;
        }
    }
    return closed;
}

PyObject *
_PyIOBase_check_closed(PyObject *self, PyObject *args)
{
    if (iobase_check_closed(self)) {
        return NULL;
    }
    if (args == Py_True) {
        return Py_None;
    }
    Py_RETURN_NONE;
}

/* XXX: IOBase thinks it has to maintain its own internal state in
   `__IOBase_closed` and call flush() by itself, but it is redundant with
   whatever behaviour a non-trivial derived class will implement. */

/*[clinic input]
_io._IOBase.close

Flush and close the IO object.

This method has no effect if the file is already closed.
[clinic start generated code]*/

static PyObject *
_io__IOBase_close_impl(PyObject *self)
/*[clinic end generated code: output=63c6a6f57d783d6d input=f4494d5c31dbc6b7]*/
{
    PyObject *res, *exc, *val, *tb;
    int rc, closed = iobase_is_closed(self);

    if (closed < 0) {
        return NULL;
    }
    if (closed) {
        Py_RETURN_NONE;
    }

    res = PyObject_CallMethodNoArgs(self, _PyIO_str_flush);

    PyErr_Fetch(&exc, &val, &tb);
    rc = _PyObject_SetAttrId(self, &PyId___IOBase_closed, Py_True);
    _PyErr_ChainExceptions(exc, val, tb);
    if (rc < 0) {
        Py_CLEAR(res);
    }

    if (res == NULL)
        return NULL;

    Py_DECREF(res);
    Py_RETURN_NONE;
}

/* Finalization and garbage collection support */

static void
iobase_finalize(PyObject *self)
{
    PyObject *res;
    PyObject *error_type, *error_value, *error_traceback;
    int closed;
    _Py_IDENTIFIER(_finalizing);

    /* Save the current exception, if any. */
    PyErr_Fetch(&error_type, &error_value, &error_traceback);

    /* If `closed` doesn't exist or can't be evaluated as bool, then the
       object is probably in an unusable state, so ignore. */
    if (_PyObject_LookupAttr(self, _PyIO_str_closed, &res) <= 0) {
        PyErr_Clear();
        closed = -1;
    }
    else {
        closed = PyObject_IsTrue(res);
        Py_DECREF(res);
        if (closed == -1)
            PyErr_Clear();
    }
    if (closed == 0) {
        /* Signal close() that it was called as part of the object
           finalization process. */
        if (_PyObject_SetAttrId(self, &PyId__finalizing, Py_True))
            PyErr_Clear();
        res = PyObject_CallMethodNoArgs((PyObject *)self, _PyIO_str_close);
        /* Silencing I/O errors is bad, but printing spurious tracebacks is
           equally as bad, and potentially more frequent (because of
           shutdown issues). */
        if (res == NULL) {
#ifndef Py_DEBUG
	  PyErr_Clear();
#else
	  PyErr_WriteUnraisable(self);
#endif
        }
        else {
            Py_DECREF(res);
        }
    }

    /* Restore the saved exception. */
    PyErr_Restore(error_type, error_value, error_traceback);
}

int
_PyIOBase_finalize(PyObject *self)
{
    int is_zombie;

    /* If _PyIOBase_finalize() is called from a destructor, we need to
       resurrect the object as calling close() can invoke arbitrary code. */
    is_zombie = (Py_REFCNT(self) == 0);
    if (is_zombie)
        return PyObject_CallFinalizerFromDealloc(self);
    else {
        PyObject_CallFinalizer(self);
        return 0;
    }
}

static int
iobase_clear(iobase *self)
{
    Py_CLEAR(self->dict);
    return 0;
}

/* Destructor */

static void
iobase_dealloc(iobase *self)
{
    /* NOTE: since IOBaseObject has its own dict, Python-defined attributes
       are still available here for close() to use.
       However, if the derived class declares a __slots__, those slots are
       already gone.
    */
    if (_PyIOBase_finalize((PyObject *) self) < 0) {
        /* When called from a heap type's dealloc, the type will be
           decref'ed on return (see e.g. subtype_dealloc in typeobject.c). */
        if (PyType_HasFeature(Py_TYPE(self), Py_TPFLAGS_HEAPTYPE))
            Py_INCREF(Py_TYPE(self));
        return;
    }
    if (self->weakreflist != NULL)
        PyObject_ClearWeakRefs((PyObject *) self);
    Py_CLEAR(self->dict);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

/* Inquiry methods */

/*[clinic input]
_io._IOBase.seekable

Return whether object supports random access.

If False, seek(), tell() and truncate() will raise OSError.
This method may need to do a test seek().
[clinic start generated code]*/

static PyObject *
_io__IOBase_seekable_impl(PyObject *self)
/*[clinic end generated code: output=4c24c67f5f32a43d input=b976622f7fdf3063]*/
{
    Py_RETURN_FALSE;
}

PyObject *
_PyIOBase_check_seekable(PyObject *self, PyObject *args)
{
    PyObject *res  = PyObject_CallMethodNoArgs(self, _PyIO_str_seekable);
    if (res == NULL)
        return NULL;
    if (res != Py_True) {
        Py_CLEAR(res);
        iobase_unsupported("File or stream is not seekable.");
        return NULL;
    }
    if (args == Py_True) {
        Py_DECREF(res);
    }
    return res;
}

/*[clinic input]
_io._IOBase.readable

Return whether object was opened for reading.

If False, read() will raise OSError.
[clinic start generated code]*/

static PyObject *
_io__IOBase_readable_impl(PyObject *self)
/*[clinic end generated code: output=e48089250686388b input=285b3b866a0ec35f]*/
{
    Py_RETURN_FALSE;
}

/* May be called with any object */
PyObject *
_PyIOBase_check_readable(PyObject *self, PyObject *args)
{
    PyObject *res = PyObject_CallMethodNoArgs(self, _PyIO_str_readable);
    if (res == NULL)
        return NULL;
    if (res != Py_True) {
        Py_CLEAR(res);
        iobase_unsupported("File or stream is not readable.");
        return NULL;
    }
    if (args == Py_True) {
        Py_DECREF(res);
    }
    return res;
}

/*[clinic input]
_io._IOBase.writable

Return whether object was opened for writing.

If False, write() will raise OSError.
[clinic start generated code]*/

static PyObject *
_io__IOBase_writable_impl(PyObject *self)
/*[clinic end generated code: output=406001d0985be14f input=9dcac18a013a05b5]*/
{
    Py_RETURN_FALSE;
}

/* May be called with any object */
PyObject *
_PyIOBase_check_writable(PyObject *self, PyObject *args)
{
    PyObject *res = PyObject_CallMethodNoArgs(self, _PyIO_str_writable);
    if (res == NULL)
        return NULL;
    if (res != Py_True) {
        Py_CLEAR(res);
        iobase_unsupported("File or stream is not writable.");
        return NULL;
    }
    if (args == Py_True) {
        Py_DECREF(res);
    }
    return res;
}

/* Context manager */

static PyObject *
iobase_enter(PyObject *self, PyObject *args)
{
    if (iobase_check_closed(self))
        return NULL;

    Py_INCREF(self);
    return self;
}

static PyObject *
iobase_exit(PyObject *self, PyObject *args)
{
    return PyObject_CallMethodNoArgs(self, _PyIO_str_close);
}

/* Lower-level APIs */

/* XXX Should these be present even if unimplemented? */

/*[clinic input]
_io._IOBase.fileno

Returns underlying file descriptor if one exists.

OSError is raised if the IO object does not use a file descriptor.
[clinic start generated code]*/

static PyObject *
_io__IOBase_fileno_impl(PyObject *self)
/*[clinic end generated code: output=7cc0973f0f5f3b73 input=4e37028947dc1cc8]*/
{
    return iobase_unsupported("fileno");
}

/*[clinic input]
_io._IOBase.isatty

Return whether this is an 'interactive' stream.

Return False if it can't be determined.
[clinic start generated code]*/

static PyObject *
_io__IOBase_isatty_impl(PyObject *self)
/*[clinic end generated code: output=60cab77cede41cdd input=9ef76530d368458b]*/
{
    if (iobase_check_closed(self))
        return NULL;
    Py_RETURN_FALSE;
}

/* Readline(s) and writelines */

/*[clinic input]
_io._IOBase.readline
    size as limit: Py_ssize_t(accept={int, NoneType}) = -1
    /

Read and return a line from the stream.

If size is specified, at most size bytes will be read.

The line terminator is always b'\n' for binary files; for text
files, the newlines argument to open can be used to select the line
terminator(s) recognized.
[clinic start generated code]*/

static PyObject *
_io__IOBase_readline_impl(PyObject *self, Py_ssize_t limit)
/*[clinic end generated code: output=4479f79b58187840 input=d0c596794e877bff]*/
{
    /* For backwards compatibility, a (slowish) readline(). */

    PyObject *result;
    Py_ssize_t bufsize, bufmax;
    char *buffer;

    buffer = (char *) PyMem_Malloc(100);
    bufsize = 0;
    bufmax = 100;
    
    if (buffer == NULL) {
        return NULL;
    }

    while (limit < 0 || bufsize < limit) {
        Py_ssize_t nreadahead = 1;
        PyObject *b;
	Py_ssize_t bsize;
        b = _PyObject_CallMethodId(self, &PyId_read, "n", nreadahead);
        if (b == NULL) {
            /* NOTE: PyErr_SetFromErrno() calls PyErr_CheckSignals()
               when EINTR occurs so we needn't do it ourselves. */
            if (_PyIO_trap_eintr()) {
                continue;
            }
            goto fail;
        }
	bsize = PyString_Size(b);
	if (bsize == 0) {
	  Py_DECREF(b);
	  break;
	}
	if (bufsize + bsize >= bufmax) {
	  char *newbuffer;
	  Py_ssize_t newmax = bufsize + bsize > 2*bufmax ? bufsize+bsize : 2*bufmax;
	  newbuffer = PyMem_Realloc(buffer, newmax);
	  if (newbuffer == NULL) {
	    PyMem_Free(buffer);
	    return NULL;
	  }
	  buffer = newbuffer;
	  bufmax = newmax;
	}
        memcpy(buffer+bufsize, 
               PyString_AsChar(b), bsize);
	bufsize += bsize;
        Py_DECREF(b);
        if (buffer[bufsize-1] == '\n')
            break;
    }
    result = PyString_FromStringAndSize(buffer, bufsize);
    PyMem_Free(buffer);
    return result;
  fail:
    PyMem_Free(buffer);
    return NULL;
}

static PyObject *
iobase_iter(PyObject *self)
{
    if (iobase_check_closed(self))
        return NULL;

    Py_INCREF(self);
    return self;
}

static PyObject *
iobase_iternext(PyObject *self)
{
    PyObject *line = PyObject_CallMethodNoArgs(self, _PyIO_str_readline);

    if (line == NULL)
        return NULL;

    if (PyObject_Size(line) <= 0) {
        /* Error or empty */
        Py_DECREF(line);
        return NULL;
    }

    return line;
}

/*[clinic input]
_io._IOBase.readlines
    hint: Py_ssize_t(accept={int, NoneType}) = -1
    /

Return a list of lines from the stream.

hint can be specified to control the number of lines read: no more
lines will be read if the total size (in bytes/characters) of all
lines so far exceeds hint.
[clinic start generated code]*/

static PyObject *
_io__IOBase_readlines_impl(PyObject *self, Py_ssize_t hint)
/*[clinic end generated code: output=2f50421677fa3dea input=9400c786ea9dc416]*/
{
    Py_ssize_t length = 0;
    PyObject *result, *it = NULL;

    result = PyList_New(0);
    if (result == NULL)
        return NULL;

    if (hint <= 0) {
        /* XXX special-casing this made sense in the Python version in order
           to remove the bytecode interpretation overhead, but it could
           probably be removed here. */
        _Py_IDENTIFIER(extend);
        PyObject *ret = _PyObject_CallMethodIdObjArgs(result, &PyId_extend,
                                                      self, NULL);

        if (ret == NULL) {
            goto error;
        }
        Py_DECREF(ret);
        return result;
    }

    it = PyObject_GetIter(self);
    if (it == NULL) {
        goto error;
    }

    while (1) {
        Py_ssize_t line_length;
        PyObject *line = PyIter_Next(it);
        if (line == NULL) {
            if (PyErr_Occurred()) {
                goto error;
            }
            else
                break; /* StopIteration raised */
        }

        if (PyList_Append(result, line) < 0) {
            Py_DECREF(line);
            goto error;
        }
        line_length = PyObject_Size(line);
        Py_DECREF(line);
        if (line_length < 0) {
            goto error;
        }
        if (line_length > hint - length)
            break;
        length += line_length;
    }

    Py_DECREF(it);
    return result;

 error:
    Py_XDECREF(it);
    Py_DECREF(result);
    return NULL;
}

/*[clinic input]
_io._IOBase.writelines
    lines: object
    /

Write a list of lines to stream.

Line separators are not added, so it is usual for each of the
lines provided to have a line separator at the end.
[clinic start generated code]*/

static PyObject *
_io__IOBase_writelines(PyObject *self, PyObject *lines)
/*[clinic end generated code: output=976eb0a9b60a6628 input=cac3fc8864183359]*/
{
    PyObject *iter, *res;

    if (iobase_check_closed(self))
        return NULL;

    iter = PyObject_GetIter(lines);
    if (iter == NULL)
        return NULL;

    while (1) {
        PyObject *line = PyIter_Next(iter);
        if (line == NULL) {
            if (PyErr_Occurred()) {
                Py_DECREF(iter);
                return NULL;
            }
            else
                break; /* Stop Iteration */
        }

        res = NULL;
        do {
            res = PyObject_CallMethodObjArgs(self, _PyIO_str_write, line, NULL);
        } while (res == NULL && _PyIO_trap_eintr());
        Py_DECREF(line);
        if (res == NULL) {
            Py_DECREF(iter);
            return NULL;
        }
        Py_DECREF(res);
    }
    Py_DECREF(iter);
    Py_RETURN_NONE;
}

/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(_io__IOBase_tell__doc__,
"tell($self, /)\n"
"--\n"
"\n"
"Return current stream position.");

#define _IO__IOBASE_TELL_METHODDEF    \
    {"tell", (PyCFunction)_io__IOBase_tell, METH_NOARGS, _io__IOBase_tell__doc__},

static PyObject *
_io__IOBase_tell_impl(PyObject *self);

static PyObject *
_io__IOBase_tell(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _io__IOBase_tell_impl(self);
}

PyDoc_STRVAR(_io__IOBase_flush__doc__,
"flush($self, /)\n"
"--\n"
"\n"
"Flush write buffers, if applicable.\n"
"\n"
"This is not implemented for read-only and non-blocking streams.");

#define _IO__IOBASE_FLUSH_METHODDEF    \
    {"flush", (PyCFunction)_io__IOBase_flush, METH_NOARGS, _io__IOBase_flush__doc__},

static PyObject *
_io__IOBase_flush_impl(PyObject *self);

static PyObject *
_io__IOBase_flush(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _io__IOBase_flush_impl(self);
}

PyDoc_STRVAR(_io__IOBase_close__doc__,
"close($self, /)\n"
"--\n"
"\n"
"Flush and close the IO object.\n"
"\n"
"This method has no effect if the file is already closed.");

#define _IO__IOBASE_CLOSE_METHODDEF    \
    {"close", (PyCFunction)_io__IOBase_close, METH_NOARGS, _io__IOBase_close__doc__},

static PyObject *
_io__IOBase_close_impl(PyObject *self);

static PyObject *
_io__IOBase_close(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _io__IOBase_close_impl(self);
}

PyDoc_STRVAR(_io__IOBase_seekable__doc__,
"seekable($self, /)\n"
"--\n"
"\n"
"Return whether object supports random access.\n"
"\n"
"If False, seek(), tell() and truncate() will raise OSError.\n"
"This method may need to do a test seek().");

#define _IO__IOBASE_SEEKABLE_METHODDEF    \
    {"seekable", (PyCFunction)_io__IOBase_seekable, METH_NOARGS, _io__IOBase_seekable__doc__},

static PyObject *
_io__IOBase_seekable_impl(PyObject *self);

static PyObject *
_io__IOBase_seekable(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _io__IOBase_seekable_impl(self);
}

PyDoc_STRVAR(_io__IOBase_readable__doc__,
"readable($self, /)\n"
"--\n"
"\n"
"Return whether object was opened for reading.\n"
"\n"
"If False, read() will raise OSError.");

#define _IO__IOBASE_READABLE_METHODDEF    \
    {"readable", (PyCFunction)_io__IOBase_readable, METH_NOARGS, _io__IOBase_readable__doc__},

static PyObject *
_io__IOBase_readable_impl(PyObject *self);

static PyObject *
_io__IOBase_readable(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _io__IOBase_readable_impl(self);
}

PyDoc_STRVAR(_io__IOBase_writable__doc__,
"writable($self, /)\n"
"--\n"
"\n"
"Return whether object was opened for writing.\n"
"\n"
"If False, write() will raise OSError.");

#define _IO__IOBASE_WRITABLE_METHODDEF    \
    {"writable", (PyCFunction)_io__IOBase_writable, METH_NOARGS, _io__IOBase_writable__doc__},

static PyObject *
_io__IOBase_writable_impl(PyObject *self);

static PyObject *
_io__IOBase_writable(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _io__IOBase_writable_impl(self);
}

PyDoc_STRVAR(_io__IOBase_fileno__doc__,
"fileno($self, /)\n"
"--\n"
"\n"
"Returns underlying file descriptor if one exists.\n"
"\n"
"OSError is raised if the IO object does not use a file descriptor.");

#define _IO__IOBASE_FILENO_METHODDEF    \
    {"fileno", (PyCFunction)_io__IOBase_fileno, METH_NOARGS, _io__IOBase_fileno__doc__},

static PyObject *
_io__IOBase_fileno_impl(PyObject *self);

static PyObject *
_io__IOBase_fileno(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _io__IOBase_fileno_impl(self);
}

PyDoc_STRVAR(_io__IOBase_isatty__doc__,
"isatty($self, /)\n"
"--\n"
"\n"
"Return whether this is an \'interactive\' stream.\n"
"\n"
"Return False if it can\'t be determined.");

#define _IO__IOBASE_ISATTY_METHODDEF    \
    {"isatty", (PyCFunction)_io__IOBase_isatty, METH_NOARGS, _io__IOBase_isatty__doc__},

static PyObject *
_io__IOBase_isatty_impl(PyObject *self);

static PyObject *
_io__IOBase_isatty(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _io__IOBase_isatty_impl(self);
}

PyDoc_STRVAR(_io__IOBase_readline__doc__,
"readline($self, size=-1, /)\n"
"--\n"
"\n"
"Read and return a line from the stream.\n"
"\n"
"If size is specified, at most size bytes will be read.\n"
"\n"
"The line terminator is always b\'\\n\' for binary files; for text\n"
"files, the newlines argument to open can be used to select the line\n"
"terminator(s) recognized.");

#define _IO__IOBASE_READLINE_METHODDEF    \
    {"readline", (PyCFunction)(void(*)(void))_io__IOBase_readline, METH_FASTCALL, _io__IOBase_readline__doc__},

static PyObject *
_io__IOBase_readline_impl(PyObject *self, Py_ssize_t limit);

static PyObject *
_io__IOBase_readline(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_ssize_t limit = -1;

    if (!_PyArg_CheckPositional("readline", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    if (!_Py_convert_optional_to_ssize_t(args[0], &limit)) {
        goto exit;
    }
skip_optional:
    return_value = _io__IOBase_readline_impl(self, limit);

exit:
    return return_value;
}

PyDoc_STRVAR(_io__IOBase_readlines__doc__,
"readlines($self, hint=-1, /)\n"
"--\n"
"\n"
"Return a list of lines from the stream.\n"
"\n"
"hint can be specified to control the number of lines read: no more\n"
"lines will be read if the total size (in bytes/characters) of all\n"
"lines so far exceeds hint.");

#define _IO__IOBASE_READLINES_METHODDEF    \
    {"readlines", (PyCFunction)(void(*)(void))_io__IOBase_readlines, METH_FASTCALL, _io__IOBase_readlines__doc__},

static PyObject *
_io__IOBase_readlines_impl(PyObject *self, Py_ssize_t hint);

static PyObject *
_io__IOBase_readlines(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_ssize_t hint = -1;

    if (!_PyArg_CheckPositional("readlines", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    if (!_Py_convert_optional_to_ssize_t(args[0], &hint)) {
        goto exit;
    }
skip_optional:
    return_value = _io__IOBase_readlines_impl(self, hint);

exit:
    return return_value;
}

PyDoc_STRVAR(_io__IOBase_writelines__doc__,
"writelines($self, lines, /)\n"
"--\n"
"\n"
"Write a list of lines to stream.\n"
"\n"
"Line separators are not added, so it is usual for each of the\n"
"lines provided to have a line separator at the end.");

#define _IO__IOBASE_WRITELINES_METHODDEF    \
    {"writelines", (PyCFunction)_io__IOBase_writelines, METH_O, _io__IOBase_writelines__doc__},


static PyMethodDef iobase_methods[] = {
    {"seek", iobase_seek, METH_VARARGS, iobase_seek_doc},
    _IO__IOBASE_TELL_METHODDEF
    {"truncate", iobase_truncate, METH_VARARGS, iobase_truncate_doc},
    _IO__IOBASE_FLUSH_METHODDEF
    _IO__IOBASE_CLOSE_METHODDEF

    _IO__IOBASE_SEEKABLE_METHODDEF
    _IO__IOBASE_READABLE_METHODDEF
    _IO__IOBASE_WRITABLE_METHODDEF

    {"_checkClosed",   _PyIOBase_check_closed, METH_NOARGS},
    {"_checkSeekable", _PyIOBase_check_seekable, METH_NOARGS},
    {"_checkReadable", _PyIOBase_check_readable, METH_NOARGS},
    {"_checkWritable", _PyIOBase_check_writable, METH_NOARGS},

    _IO__IOBASE_FILENO_METHODDEF
    _IO__IOBASE_ISATTY_METHODDEF

    {"__enter__", iobase_enter, METH_NOARGS},
    {"__exit__", iobase_exit, METH_VARARGS},

    _IO__IOBASE_READLINE_METHODDEF
    _IO__IOBASE_READLINES_METHODDEF
    _IO__IOBASE_WRITELINES_METHODDEF

    {NULL, NULL}
};

static PyGetSetDef iobase_getset[] = {
    {"__dict__", PyObject_GenericGetDict, NULL, NULL},
    {"closed", (getter)iobase_closed_get, NULL, NULL},
    {NULL}
};


PyTypeObject PyIOBase_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_io._IOBase",              /*tp_name*/
    sizeof(iobase),             /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor)iobase_dealloc, /*tp_dealloc*/
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
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    //        | Py_TPFLAGS_HAVE_GC,   /*tp_flags*/
    iobase_doc,                 /* tp_doc */
    0, // (traverseproc)iobase_traverse, /* tp_traverse */
    (inquiry)iobase_clear,      /* tp_clear */
    0,                          /* tp_richcompare */
    offsetof(iobase, weakreflist), /* tp_weaklistoffset */
    iobase_iter,                /* tp_iter */
    iobase_iternext,            /* tp_iternext */
    iobase_methods,             /* tp_methods */
    0,                          /* tp_members */
    iobase_getset,              /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    offsetof(iobase, dict),     /* tp_dictoffset */
    0,                          /* tp_init */
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
    iobase_finalize,            /* tp_finalize */
};
