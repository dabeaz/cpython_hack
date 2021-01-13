/* Author: Daniel Stutzbach */

#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "pycore_object.h"
#include "structmember.h"         // PyMemberDef
#include <stdbool.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <stddef.h> /* For offsetof */
#include "_iomodule.h"

/*
 * Known likely problems:
 *
 * - Files larger then 2**32-1
 * - Files with unicode filenames
 * - Passing numbers greater than 2**32-1 when an integer is expected
 * - Making it work on Windows and other oddball platforms
 *
 * To Do:
 *
 * - autoconfify header file inclusion
 */

#if BUFSIZ < (8*1024)
#define SMALLCHUNK (8*1024)
#elif (BUFSIZ >= (2 << 25))
#error "unreasonable BUFSIZ > 64 MiB defined"
#else
#define SMALLCHUNK BUFSIZ
#endif

/*[clinic input]
module _io
class _io.FileIO "fileio *" "&PyFileIO_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=1c77708b41fda70c]*/

typedef struct {
    PyObject_HEAD
    int fd;
    unsigned int created : 1;
    unsigned int readable : 1;
    unsigned int writable : 1;
    unsigned int appending : 1;
    signed int seekable : 2; /* -1 means unknown */
    unsigned int closefd : 1;
    char finalizing;
    unsigned int blksize;
    PyObject *weakreflist;
    PyObject *dict;
} fileio;

PyTypeObject PyFileIO_Type;

_Py_IDENTIFIER(name);

#define PyFileIO_Check(op) (PyObject_TypeCheck((op), &PyFileIO_Type))

/* Forward declarations */
static PyObject* portable_lseek(fileio *self, PyObject *posobj, int whence, bool suppress_pipe_error);

int
_PyFileIO_closed(PyObject *self)
{
    return ((fileio *)self)->fd < 0;
}

/* Because this can call arbitrary code, it shouldn't be called when
   the refcount is 0 (that is, not directly from tp_dealloc unless
   the refcount has been temporarily re-incremented). */
static PyObject *
fileio_dealloc_warn(fileio *self, PyObject *source)
{
    if (self->fd >= 0 && self->closefd) {
        PyObject *exc, *val, *tb;
        PyErr_Fetch(&exc, &val, &tb);
        PyErr_Restore(exc, val, tb);
    }
    Py_RETURN_NONE;
}

/* Returns 0 on success, -1 with exception set on failure. */
static int
internal_close(fileio *self)
{
    int err = 0;
    int save_errno = 0;
    if (self->fd >= 0) {
        int fd = self->fd;
        self->fd = -1;
        /* fd is accessible and someone else may have closed it */
        
        err = close(fd);
        if (err < 0)
            save_errno = errno;
        
    }
    if (err < 0) {
        errno = save_errno;
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    return 0;
}

/*[clinic input]
_io.FileIO.close

Close the file.

A closed file cannot be used for further I/O operations.  close() may be
called more than once without error.
[clinic start generated code]*/

static PyObject *
_io_FileIO_close_impl(fileio *self)
/*[clinic end generated code: output=7737a319ef3bad0b input=f35231760d54a522]*/
{
    PyObject *res;
    PyObject *exc, *val, *tb;
    int rc;
    _Py_IDENTIFIER(close);
    res = _PyObject_CallMethodIdOneArg((PyObject*)&PyIOBase_Type,
                                       &PyId_close, (PyObject *)self);
    if (!self->closefd) {
        self->fd = -1;
        return res;
    }
    if (res == NULL)
        PyErr_Fetch(&exc, &val, &tb);
    if (self->finalizing) {
        PyObject *r = fileio_dealloc_warn(self, (PyObject *) self);
        if (r)
            Py_DECREF(r);
        else
            PyErr_Clear();
    }
    rc = internal_close(self);
    if (res == NULL)
        _PyErr_ChainExceptions(exc, val, tb);
    if (rc < 0)
        Py_CLEAR(res);
    return res;
}

static PyObject *
fileio_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    fileio *self;

    assert(type != NULL && type->tp_alloc != NULL);

    self = (fileio *) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->fd = -1;
        self->created = 0;
        self->readable = 0;
        self->writable = 0;
        self->appending = 0;
        self->seekable = -1;
        self->blksize = 0;
        self->closefd = 1;
        self->weakreflist = NULL;
    }

    return (PyObject *) self;
}

#ifdef O_CLOEXEC
extern int _Py_open_cloexec_works;
#endif

/*[clinic input]
_io.FileIO.__init__
    file as nameobj: object
    mode: str = "r"
    closefd: bool(accept={int}) = True
    opener: object = None

Open a file.

The mode can be 'r' (default), 'w', 'x' or 'a' for reading,
writing, exclusive creation or appending.  The file will be created if it
doesn't exist when opened for writing or appending; it will be truncated
when opened for writing.  A FileExistsError will be raised if it already
exists when opened for creating. Opening a file for creating implies
writing so this mode behaves in a similar way to 'w'.Add a '+' to the mode
to allow simultaneous reading and writing. A custom opener can be used by
passing a callable as *opener*. The underlying file descriptor for the file
object is then obtained by calling opener with (*name*, *flags*).
*opener* must return an open file descriptor (passing os.open as *opener*
results in functionality similar to passing None).
[clinic start generated code]*/

static int
_io_FileIO___init___impl(fileio *self, PyObject *nameobj, const char *mode,
                         int closefd, PyObject *opener)
/*[clinic end generated code: output=23413f68e6484bbd input=1596c9157a042a39]*/
{
    const char *name = NULL;
    PyObject *stringobj = NULL;
    const char *s;
    int ret = 0;
    int rwa = 0, plus = 0;
    int flags = 0;
    int fd = -1;
    int fd_is_own = 0;
    int *atomic_flag_works = NULL;
    struct _Py_stat_struct fdfstat;
    int fstat_result;
    int async_err = 0;

    assert(PyFileIO_Check(self));
    if (self->fd >= 0) {
        if (self->closefd) {
            /* Have to close the existing file first. */
            if (internal_close(self) < 0)
                return -1;
        }
        else
            self->fd = -1;
    }

    fd = _PyLong_AsInt(nameobj);
    if (fd < 0) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_ValueError,
                            "negative file descriptor");
            return -1;
        }
        PyErr_Clear();
    }

    if (fd < 0) {
        name = PyUnicode_AsChar(nameobj);
    }

    s = mode;
    while (*s) {
        switch (*s++) {
        case 'x':
            if (rwa) {
            bad_mode:
                PyErr_SetString(PyExc_ValueError,
                                "Must have exactly one of create/read/write/append "
                                "mode and at most one plus");
                goto error;
            }
            rwa = 1;
            self->created = 1;
            self->writable = 1;
            flags |= O_EXCL | O_CREAT;
            break;
        case 'r':
            if (rwa)
                goto bad_mode;
            rwa = 1;
            self->readable = 1;
            break;
        case 'w':
            if (rwa)
                goto bad_mode;
            rwa = 1;
            self->writable = 1;
            flags |= O_CREAT | O_TRUNC;
            break;
        case 'a':
            if (rwa)
                goto bad_mode;
            rwa = 1;
            self->writable = 1;
            self->appending = 1;
            flags |= O_APPEND | O_CREAT;
            break;
        case 'b':
            break;
        case '+':
            if (plus)
                goto bad_mode;
            self->readable = self->writable = 1;
            plus = 1;
            break;
        default:
            PyErr_Format(PyExc_ValueError,
                         "invalid mode: %.200s", mode);
            goto error;
        }
    }

    if (!rwa)
        goto bad_mode;

    if (self->readable && self->writable)
        flags |= O_RDWR;
    else if (self->readable)
        flags |= O_RDONLY;
    else
        flags |= O_WRONLY;

#ifdef O_BINARY
    flags |= O_BINARY;
#endif
    flags |= O_CLOEXEC;

    if (fd >= 0) {
        self->fd = fd;
        self->closefd = closefd;
    }
    else {
        self->closefd = 1;
        if (!closefd) {
            PyErr_SetString(PyExc_ValueError,
                "Cannot use closefd=False with file name");
            goto error;
        }

        errno = 0;
        if (opener == Py_None) {
            do {
                self->fd = open(name, flags, 0666);
	    } while (self->fd < 0 && errno == EINTR);
            if (async_err)
                goto error;
        }
        else {
            PyObject *fdobj;
            /* the opener may clear the atomic flag */
            atomic_flag_works = NULL;
            fdobj = PyObject_CallFunction(opener, "Oi", nameobj, flags);
            if (fdobj == NULL)
                goto error;
            if (!PyLong_Check(fdobj)) {
                Py_DECREF(fdobj);
                PyErr_SetString(PyExc_TypeError,
                        "expected integer from opener");
                goto error;
            }

            self->fd = _PyLong_AsInt(fdobj);
            Py_DECREF(fdobj);
            if (self->fd < 0) {
                if (!PyErr_Occurred()) {
                    /* The opener returned a negative but didn't set an
                       exception.  See issue #27066 */
                    PyErr_Format(PyExc_ValueError,
                                 "opener returned %d", self->fd);
                }
                goto error;
            }
        }

        fd_is_own = 1;
        if (self->fd < 0) {
            PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, nameobj);
            goto error;
        }
        if (_Py_set_inheritable(self->fd, 0, atomic_flag_works) < 0)
            goto error;
    }

    self->blksize = DEFAULT_BUFFER_SIZE;
    fstat_result = _Py_fstat_noraise(self->fd, &fdfstat);
    
    if (fstat_result < 0) {
        /* Tolerate fstat() errors other than EBADF.  See Issue #25717, where
        an anonymous file on a Virtual Box shared folder filesystem would
        raise ENOENT. */
        if (errno == EBADF) {
            PyErr_SetFromErrno(PyExc_OSError);
            goto error;
        }
    }
    else {
#if defined(S_ISDIR) && defined(EISDIR)
        /* On Unix, open will succeed for directories.
           In Python, there should be no file objects referring to
           directories, so we need a check.  */
        if (S_ISDIR(fdfstat.st_mode)) {
            errno = EISDIR;
            PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, nameobj);
            goto error;
        }
#endif /* defined(S_ISDIR) */
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
        if (fdfstat.st_blksize > 1)
            self->blksize = fdfstat.st_blksize;
#endif /* HAVE_STRUCT_STAT_ST_BLKSIZE */
    }
    if (_PyObject_SetAttrId((PyObject *)self, &PyId_name, nameobj) < 0)
        goto error;

    if (self->appending) {
        /* For consistent behaviour, we explicitly seek to the
           end of file (otherwise, it might be done only on the
           first write()). */
        PyObject *pos = portable_lseek(self, NULL, 2, true);
        if (pos == NULL)
            goto error;
        Py_DECREF(pos);
    }

    goto done;

 error:
    ret = -1;
    if (!fd_is_own)
        self->fd = -1;
    if (self->fd >= 0)
        internal_close(self);

 done:
    Py_CLEAR(stringobj);
    return ret;
}

static int
fileio_clear(fileio *self)
{
    Py_CLEAR(self->dict);
    return 0;
}

static void
fileio_dealloc(fileio *self)
{
    self->finalizing = 1;
    if (_PyIOBase_finalize((PyObject *) self) < 0)
        return;
    if (self->weakreflist != NULL)
        PyObject_ClearWeakRefs((PyObject *) self);
    Py_CLEAR(self->dict);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
err_closed(void)
{
    PyErr_SetString(PyExc_ValueError, "I/O operation on closed file");
    return NULL;
}

static PyObject *
err_mode(const char *action)
{
    _PyIO_State *state = IO_STATE();
    if (state != NULL)
        PyErr_Format(state->unsupported_operation,
                     "File not open for %s", action);
    return NULL;
}

/*[clinic input]
_io.FileIO.fileno

Return the underlying file descriptor (an integer).
[clinic start generated code]*/

static PyObject *
_io_FileIO_fileno_impl(fileio *self)
/*[clinic end generated code: output=a9626ce5398ece90 input=0b9b2de67335ada3]*/
{
    if (self->fd < 0)
        return err_closed();
    return PyLong_FromLong((long) self->fd);
}

/*[clinic input]
_io.FileIO.readable

True if file was opened in a read mode.
[clinic start generated code]*/

static PyObject *
_io_FileIO_readable_impl(fileio *self)
/*[clinic end generated code: output=640744a6150fe9ba input=a3fdfed6eea721c5]*/
{
    if (self->fd < 0)
        return err_closed();
    return PyBool_FromLong((long) self->readable);
}

/*[clinic input]
_io.FileIO.writable

True if file was opened in a write mode.
[clinic start generated code]*/

static PyObject *
_io_FileIO_writable_impl(fileio *self)
/*[clinic end generated code: output=96cefc5446e89977 input=c204a808ca2e1748]*/
{
    if (self->fd < 0)
        return err_closed();
    return PyBool_FromLong((long) self->writable);
}

/*[clinic input]
_io.FileIO.seekable

True if file supports random-access.
[clinic start generated code]*/

static PyObject *
_io_FileIO_seekable_impl(fileio *self)
/*[clinic end generated code: output=47909ca0a42e9287 input=c8e5554d2fd63c7f]*/
{
    if (self->fd < 0)
        return err_closed();
    if (self->seekable < 0) {
        /* portable_lseek() sets the seekable attribute */
        PyObject *pos = portable_lseek(self, NULL, SEEK_CUR, false);
        assert(self->seekable >= 0);
        if (pos == NULL) {
            PyErr_Clear();
        }
        else {
            Py_DECREF(pos);
        }
    }
    return PyBool_FromLong((long) self->seekable);
}

static size_t
new_buffersize(fileio *self, size_t currentsize)
{
    size_t addend;

    /* Expand the buffer by an amount proportional to the current size,
       giving us amortized linear-time behavior.  For bigger sizes, use a
       less-than-double growth factor to avoid excessive allocation. */
    assert(currentsize <= PY_SSIZE_T_MAX);
    if (currentsize > 65536)
        addend = currentsize >> 3;
    else
        addend = 256 + currentsize;
    if (addend < SMALLCHUNK)
        /* Avoid tiny read() calls. */
        addend = SMALLCHUNK;
    return addend + currentsize;
}

/*[clinic input]
_io.FileIO.readall

Read all data from the file, returned as bytes.

In non-blocking mode, returns as much as is immediately available,
or None if no data is available.  Return an empty bytes object at EOF.
[clinic start generated code]*/

static PyObject *
_io_FileIO_readall_impl(fileio *self)
/*[clinic end generated code: output=faa0292b213b4022 input=dbdc137f55602834]*/
{
    struct _Py_stat_struct status;
    Py_off_t pos, end;
    char *buffer;
    PyObject *result;
    Py_ssize_t bytes_read = 0;
    Py_ssize_t n;
    size_t bufsize, newbufsize;
    int fstat_result;

    if (self->fd < 0)
        return err_closed();
    pos = lseek(self->fd, 0L, SEEK_CUR);
    fstat_result = _Py_fstat_noraise(self->fd, &status);

    if (fstat_result == 0)
        end = status.st_size;
    else
        end = (Py_off_t)-1;

    if (end > 0 && end >= pos && pos >= 0 && end - pos < PY_SSIZE_T_MAX) {
        /* This is probably a real file, so we try to allocate a
           buffer one byte larger than the rest of the file.  If the
           calculation is right then we should get EOF without having
           to enlarge the buffer. */
        bufsize = (size_t)(end - pos + 1);
    } else {
        bufsize = SMALLCHUNK;
    }

    buffer = PyMem_Malloc(bufsize);
    if (buffer == NULL)
        return NULL;

    while (1) {
        if (bytes_read >= (Py_ssize_t)bufsize) {
            newbufsize = new_buffersize(self, bytes_read);
            if (newbufsize > PY_SSIZE_T_MAX || newbufsize <= 0) {
                PyErr_SetString(PyExc_OverflowError,
                                "unbounded read returned more bytes "
                                "than a Python bytes object can hold");
		PyMem_Free(buffer);
                return NULL;
            }
	    if (bufsize < newbufsize) {
	      char *newbuffer = PyMem_Realloc(buffer, newbufsize);
	      if (newbuffer == NULL) {
		PyMem_Free(buffer);
		return NULL;
	      }
	      buffer = newbuffer;
	      bufsize = newbufsize;
	    }
        }
        n = _Py_read(self->fd,
                     buffer + bytes_read,
                     bufsize - bytes_read);

        if (n == 0)
            break;
        if (n == -1) {
	  PyMem_Free(buffer);
	  return NULL;
        }
        bytes_read += n;
        pos += n;
    }
    result = PyUnicode_FromStringAndSize(buffer, bytes_read);
    PyMem_Free(buffer);
    return result;
}

/*[clinic input]
_io.FileIO.read
    size: Py_ssize_t(accept={int, NoneType}) = -1
    /

Read at most size bytes, returned as bytes.

Only makes one system call, so less data may be returned than requested.
In non-blocking mode, returns None if no data is available.
Return an empty bytes object at EOF.
[clinic start generated code]*/

static PyObject *
_io_FileIO_read_impl(fileio *self, Py_ssize_t size)
/*[clinic end generated code: output=42528d39dd0ca641 input=bec9a2c704ddcbc9]*/
{
    char *ptr;
    Py_ssize_t n;
    PyObject *bytes;

    if (self->fd < 0)
        return err_closed();
    if (!self->readable)
        return err_mode("reading");

    if (size < 0)
        return _io_FileIO_readall_impl(self);

    if (size > _PY_READ_MAX) {
        size = _PY_READ_MAX;
    }

    ptr = (char *) PyMem_Malloc(size);
    if (ptr == NULL) {
      return NULL;
    }
    n = _Py_read(self->fd, ptr, size);
    if (n == -1) {
	PyMem_Free(ptr);
        return NULL;
    }
    bytes = PyUnicode_FromStringAndSize(ptr, n);
    PyMem_Free(ptr);
    return bytes;
}

/* Cribbed from posix_lseek() */
static PyObject *
portable_lseek(fileio *self, PyObject *posobj, int whence, bool suppress_pipe_error)
{
    Py_off_t pos, res;
    int fd = self->fd;

#ifdef SEEK_SET
    /* Turn 0, 1, 2 into SEEK_{SET,CUR,END} */
    switch (whence) {
#if SEEK_SET != 0
    case 0: whence = SEEK_SET; break;
#endif
#if SEEK_CUR != 1
    case 1: whence = SEEK_CUR; break;
#endif
#if SEEK_END != 2
    case 2: whence = SEEK_END; break;
#endif
    }
#endif /* SEEK_SET */

    if (posobj == NULL) {
        pos = 0;
    }
    else {
#if defined(HAVE_LARGEFILE_SUPPORT)
        pos = PyLong_AsLongLong(posobj);
#else
        pos = PyLong_AsLong(posobj);
#endif
        if (PyErr_Occurred())
            return NULL;
    }
    res = lseek(fd, pos, whence);

    if (self->seekable < 0) {
        self->seekable = (res >= 0);
    }

    if (res < 0) {
        if (suppress_pipe_error && errno == ESPIPE) {
            res = 0;
        } else {
            return PyErr_SetFromErrno(PyExc_OSError);
        }
    }

#if defined(HAVE_LARGEFILE_SUPPORT)
    return PyLong_FromLongLong(res);
#else
    return PyLong_FromLong(res);
#endif
}

/*[clinic input]
_io.FileIO.seek
    pos: object
    whence: int = 0
    /

Move to new file position and return the file position.

Argument offset is a byte count.  Optional argument whence defaults to
SEEK_SET or 0 (offset from start of file, offset should be >= 0); other values
are SEEK_CUR or 1 (move relative to current position, positive or negative),
and SEEK_END or 2 (move relative to end of file, usually negative, although
many platforms allow seeking beyond the end of a file).

Note that not all file objects are seekable.
[clinic start generated code]*/

static PyObject *
_io_FileIO_seek_impl(fileio *self, PyObject *pos, int whence)
/*[clinic end generated code: output=c976acdf054e6655 input=0439194b0774d454]*/
{
    if (self->fd < 0)
        return err_closed();

    return portable_lseek(self, pos, whence, false);
}

/*[clinic input]
_io.FileIO.tell

Current file position.

Can raise OSError for non seekable files.
[clinic start generated code]*/

static PyObject *
_io_FileIO_tell_impl(fileio *self)
/*[clinic end generated code: output=ffe2147058809d0b input=807e24ead4cec2f9]*/
{
    if (self->fd < 0)
        return err_closed();

    return portable_lseek(self, NULL, 1, false);
}

#ifdef HAVE_FTRUNCATE
/*[clinic input]
_io.FileIO.truncate
    size as posobj: object = None
    /

Truncate the file to at most size bytes and return the truncated size.

Size defaults to the current file position, as returned by tell().
The current file position is changed to the value of size.
[clinic start generated code]*/

static PyObject *
_io_FileIO_truncate_impl(fileio *self, PyObject *posobj)
/*[clinic end generated code: output=e49ca7a916c176fa input=b0ac133939823875]*/
{
    Py_off_t pos;
    int ret;
    int fd;

    fd = self->fd;
    if (fd < 0)
        return err_closed();
    if (!self->writable)
        return err_mode("writing");

    if (posobj == Py_None) {
        /* Get the current position. */
        posobj = portable_lseek(self, NULL, 1, false);
        if (posobj == NULL)
            return NULL;
    }
    else {
        Py_INCREF(posobj);
    }

#if defined(HAVE_LARGEFILE_SUPPORT)
    pos = PyLong_AsLongLong(posobj);
#else
    pos = PyLong_AsLong(posobj);
#endif
    if (PyErr_Occurred()){
        Py_DECREF(posobj);
        return NULL;
    }

    
    
    errno = 0;
    ret = ftruncate(fd, pos);
    if (ret != 0) {
        Py_DECREF(posobj);
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    return posobj;
}
#endif /* HAVE_FTRUNCATE */

static const char *
mode_string(fileio *self)
{
    if (self->created) {
        if (self->readable)
            return "xb+";
        else
            return "xb";
    }
    if (self->appending) {
        if (self->readable)
            return "ab+";
        else
            return "ab";
    }
    else if (self->readable) {
        if (self->writable)
            return "rb+";
        else
            return "rb";
    }
    else
        return "wb";
}

static PyObject *
fileio_repr(fileio *self)
{
    PyObject *nameobj, *res;

    if (self->fd < 0)
        return PyUnicode_FromFormat("<_io.FileIO [closed]>");

    if (_PyObject_LookupAttrId((PyObject *) self, &PyId_name, &nameobj) < 0) {
        return NULL;
    }
    if (nameobj == NULL) {
        res = PyUnicode_FromFormat(
            "<_io.FileIO fd=%d mode='%s' closefd=%s>",
            self->fd, mode_string(self), self->closefd ? "True" : "False");
    }
    else {
        int status = Py_ReprEnter((PyObject *)self);
        res = NULL;
        if (status == 0) {
            res = PyUnicode_FromFormat(
                "<_io.FileIO name=%R mode='%s' closefd=%s>",
                nameobj, mode_string(self), self->closefd ? "True" : "False");
            Py_ReprLeave((PyObject *)self);
        }
        else if (status > 0) {
            PyErr_Format(PyExc_RuntimeError,
                         "reentrant call inside %s.__repr__",
                         Py_TYPE(self)->tp_name);
        }
        Py_DECREF(nameobj);
    }
    return res;
}

/*[clinic input]
_io.FileIO.isatty

True if the file is connected to a TTY device.
[clinic start generated code]*/

static PyObject *
_io_FileIO_isatty_impl(fileio *self)
/*[clinic end generated code: output=932c39924e9a8070 input=cd94ca1f5e95e843]*/
{
    long res;

    if (self->fd < 0)
        return err_closed();
    
    
    res = isatty(self->fd);
    
    
    return PyBool_FromLong(res);
}

/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(_io_FileIO_close__doc__,
"close($self, /)\n"
"--\n"
"\n"
"Close the file.\n"
"\n"
"A closed file cannot be used for further I/O operations.  close() may be\n"
"called more than once without error.");

#define _IO_FILEIO_CLOSE_METHODDEF    \
    {"close", (PyCFunction)_io_FileIO_close, METH_NOARGS, _io_FileIO_close__doc__},

static PyObject *
_io_FileIO_close_impl(fileio *self);

static PyObject *
_io_FileIO_close(fileio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_FileIO_close_impl(self);
}

PyDoc_STRVAR(_io_FileIO___init____doc__,
"FileIO(file, mode=\'r\', closefd=True, opener=None)\n"
"--\n"
"\n"
"Open a file.\n"
"\n"
"The mode can be \'r\' (default), \'w\', \'x\' or \'a\' for reading,\n"
"writing, exclusive creation or appending.  The file will be created if it\n"
"doesn\'t exist when opened for writing or appending; it will be truncated\n"
"when opened for writing.  A FileExistsError will be raised if it already\n"
"exists when opened for creating. Opening a file for creating implies\n"
"writing so this mode behaves in a similar way to \'w\'.Add a \'+\' to the mode\n"
"to allow simultaneous reading and writing. A custom opener can be used by\n"
"passing a callable as *opener*. The underlying file descriptor for the file\n"
"object is then obtained by calling opener with (*name*, *flags*).\n"
"*opener* must return an open file descriptor (passing os.open as *opener*\n"
"results in functionality similar to passing None).");

static int
_io_FileIO___init___impl(fileio *self, PyObject *nameobj, const char *mode,
                         int closefd, PyObject *opener);

static int
_io_FileIO___init__(PyObject *self, PyObject *args, PyObject *kwargs)
{
    int return_value = -1;
    static const char * const _keywords[] = {"file", "mode", "closefd", "opener", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "FileIO", 0};
    PyObject *argsbuf[4];
    PyObject * const *fastargs;
    Py_ssize_t nargs = PyTuple_Size(args);
    Py_ssize_t noptargs = nargs + (kwargs ? PyDict_Size(kwargs) : 0) - 1;
    PyObject *nameobj;
    const char *mode = "r";
    int closefd = 1;
    PyObject *opener = Py_None;

    fastargs = _PyArg_UnpackKeywords(PyTuple_Items(args), nargs, kwargs, NULL, &_parser, 1, 4, 0, argsbuf);
    if (!fastargs) {
        goto exit;
    }
    nameobj = fastargs[0];
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (fastargs[1]) {
        if (!PyUnicode_Check(fastargs[1])) {
            _PyArg_BadArgument("FileIO", "argument 'mode'", "str", fastargs[1]);
            goto exit;
        }
        Py_ssize_t mode_length;
        mode = PyUnicode_AsCharAndSize(fastargs[1], &mode_length);
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
    if (fastargs[2]) {
        closefd = _PyLong_AsInt(fastargs[2]);
        if (closefd == -1 && PyErr_Occurred()) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    opener = fastargs[3];
skip_optional_pos:
    return_value = _io_FileIO___init___impl((fileio *)self, nameobj, mode, closefd, opener);

exit:
    return return_value;
}

PyDoc_STRVAR(_io_FileIO_fileno__doc__,
"fileno($self, /)\n"
"--\n"
"\n"
"Return the underlying file descriptor (an integer).");

#define _IO_FILEIO_FILENO_METHODDEF    \
    {"fileno", (PyCFunction)_io_FileIO_fileno, METH_NOARGS, _io_FileIO_fileno__doc__},

static PyObject *
_io_FileIO_fileno_impl(fileio *self);

static PyObject *
_io_FileIO_fileno(fileio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_FileIO_fileno_impl(self);
}

PyDoc_STRVAR(_io_FileIO_readable__doc__,
"readable($self, /)\n"
"--\n"
"\n"
"True if file was opened in a read mode.");

#define _IO_FILEIO_READABLE_METHODDEF    \
    {"readable", (PyCFunction)_io_FileIO_readable, METH_NOARGS, _io_FileIO_readable__doc__},

static PyObject *
_io_FileIO_readable_impl(fileio *self);

static PyObject *
_io_FileIO_readable(fileio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_FileIO_readable_impl(self);
}

PyDoc_STRVAR(_io_FileIO_writable__doc__,
"writable($self, /)\n"
"--\n"
"\n"
"True if file was opened in a write mode.");

#define _IO_FILEIO_WRITABLE_METHODDEF    \
    {"writable", (PyCFunction)_io_FileIO_writable, METH_NOARGS, _io_FileIO_writable__doc__},

static PyObject *
_io_FileIO_writable_impl(fileio *self);

static PyObject *
_io_FileIO_writable(fileio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_FileIO_writable_impl(self);
}

PyDoc_STRVAR(_io_FileIO_seekable__doc__,
"seekable($self, /)\n"
"--\n"
"\n"
"True if file supports random-access.");

#define _IO_FILEIO_SEEKABLE_METHODDEF    \
    {"seekable", (PyCFunction)_io_FileIO_seekable, METH_NOARGS, _io_FileIO_seekable__doc__},

static PyObject *
_io_FileIO_seekable_impl(fileio *self);

static PyObject *
_io_FileIO_seekable(fileio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_FileIO_seekable_impl(self);
}

PyDoc_STRVAR(_io_FileIO_readall__doc__,
"readall($self, /)\n"
"--\n"
"\n"
"Read all data from the file, returned as bytes.\n"
"\n"
"In non-blocking mode, returns as much as is immediately available,\n"
"or None if no data is available.  Return an empty bytes object at EOF.");

#define _IO_FILEIO_READALL_METHODDEF    \
    {"readall", (PyCFunction)_io_FileIO_readall, METH_NOARGS, _io_FileIO_readall__doc__},

static PyObject *
_io_FileIO_readall_impl(fileio *self);

static PyObject *
_io_FileIO_readall(fileio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_FileIO_readall_impl(self);
}

PyDoc_STRVAR(_io_FileIO_read__doc__,
"read($self, size=-1, /)\n"
"--\n"
"\n"
"Read at most size bytes, returned as bytes.\n"
"\n"
"Only makes one system call, so less data may be returned than requested.\n"
"In non-blocking mode, returns None if no data is available.\n"
"Return an empty bytes object at EOF.");

#define _IO_FILEIO_READ_METHODDEF    \
    {"read", (PyCFunction)(void(*)(void))_io_FileIO_read, METH_FASTCALL, _io_FileIO_read__doc__},

static PyObject *
_io_FileIO_read_impl(fileio *self, Py_ssize_t size);

static PyObject *
_io_FileIO_read(fileio *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_ssize_t size = -1;

    if (!_PyArg_CheckPositional("read", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    if (!_Py_convert_optional_to_ssize_t(args[0], &size)) {
        goto exit;
    }
skip_optional:
    return_value = _io_FileIO_read_impl(self, size);

exit:
    return return_value;
}

PyDoc_STRVAR(_io_FileIO_write__doc__,
"write($self, b, /)\n"
"--\n"
"\n"
"Write buffer b to file, return number of bytes written.\n"
"\n"
"Only makes one system call, so not all of the data may be written.\n"
"The number of bytes actually written is returned.  In non-blocking mode,\n"
"returns None if the write would block.");

#define _IO_FILEIO_WRITE_METHODDEF    \
    {"write", (PyCFunction)_io_FileIO_write, METH_O, _io_FileIO_write__doc__},

static PyObject *
_io_FileIO_write(fileio *self, PyObject *arg)
{
  const char *buf;
  Py_ssize_t len, n;

  if (!PyUnicode_Check(arg)) {
    _PyArg_BadArgument("write", "argument", "string", arg);
    return NULL;
  }
  buf = PyUnicode_AsCharAndSize(arg, &len);
  if (self->fd < 0)
    return err_closed();
  if (!self->writable)
    return err_mode("writing");
      
  n = _Py_write(self->fd, buf, len);
  if (n < 0) {
    return NULL;
  }
  return PyLong_FromSsize_t(n);
}

PyDoc_STRVAR(_io_FileIO_seek__doc__,
"seek($self, pos, whence=0, /)\n"
"--\n"
"\n"
"Move to new file position and return the file position.\n"
"\n"
"Argument offset is a byte count.  Optional argument whence defaults to\n"
"SEEK_SET or 0 (offset from start of file, offset should be >= 0); other values\n"
"are SEEK_CUR or 1 (move relative to current position, positive or negative),\n"
"and SEEK_END or 2 (move relative to end of file, usually negative, although\n"
"many platforms allow seeking beyond the end of a file).\n"
"\n"
"Note that not all file objects are seekable.");

#define _IO_FILEIO_SEEK_METHODDEF    \
    {"seek", (PyCFunction)(void(*)(void))_io_FileIO_seek, METH_FASTCALL, _io_FileIO_seek__doc__},

static PyObject *
_io_FileIO_seek_impl(fileio *self, PyObject *pos, int whence);

static PyObject *
_io_FileIO_seek(fileio *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *pos;
    int whence = 0;

    if (!_PyArg_CheckPositional("seek", nargs, 1, 2)) {
        goto exit;
    }
    pos = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    whence = _PyLong_AsInt(args[1]);
    if (whence == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _io_FileIO_seek_impl(self, pos, whence);

exit:
    return return_value;
}

PyDoc_STRVAR(_io_FileIO_tell__doc__,
"tell($self, /)\n"
"--\n"
"\n"
"Current file position.\n"
"\n"
"Can raise OSError for non seekable files.");

#define _IO_FILEIO_TELL_METHODDEF    \
    {"tell", (PyCFunction)_io_FileIO_tell, METH_NOARGS, _io_FileIO_tell__doc__},

static PyObject *
_io_FileIO_tell_impl(fileio *self);

static PyObject *
_io_FileIO_tell(fileio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_FileIO_tell_impl(self);
}

#if defined(HAVE_FTRUNCATE)

PyDoc_STRVAR(_io_FileIO_truncate__doc__,
"truncate($self, size=None, /)\n"
"--\n"
"\n"
"Truncate the file to at most size bytes and return the truncated size.\n"
"\n"
"Size defaults to the current file position, as returned by tell().\n"
"The current file position is changed to the value of size.");

#define _IO_FILEIO_TRUNCATE_METHODDEF    \
    {"truncate", (PyCFunction)(void(*)(void))_io_FileIO_truncate, METH_FASTCALL, _io_FileIO_truncate__doc__},

static PyObject *
_io_FileIO_truncate_impl(fileio *self, PyObject *posobj);

static PyObject *
_io_FileIO_truncate(fileio *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *posobj = Py_None;

    if (!_PyArg_CheckPositional("truncate", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    posobj = args[0];
skip_optional:
    return_value = _io_FileIO_truncate_impl(self, posobj);

exit:
    return return_value;
}

#endif /* defined(HAVE_FTRUNCATE) */

PyDoc_STRVAR(_io_FileIO_isatty__doc__,
"isatty($self, /)\n"
"--\n"
"\n"
"True if the file is connected to a TTY device.");

#define _IO_FILEIO_ISATTY_METHODDEF    \
    {"isatty", (PyCFunction)_io_FileIO_isatty, METH_NOARGS, _io_FileIO_isatty__doc__},

static PyObject *
_io_FileIO_isatty_impl(fileio *self);

static PyObject *
_io_FileIO_isatty(fileio *self, PyObject *Py_UNUSED(ignored))
{
    return _io_FileIO_isatty_impl(self);
}

#ifndef _IO_FILEIO_TRUNCATE_METHODDEF
    #define _IO_FILEIO_TRUNCATE_METHODDEF
#endif /* !defined(_IO_FILEIO_TRUNCATE_METHODDEF) */
/*[clinic end generated code: output=3479912ec0f7e029 input=a9049054013a1b77]*/


static PyMethodDef fileio_methods[] = {
    _IO_FILEIO_READ_METHODDEF
    _IO_FILEIO_READALL_METHODDEF
    _IO_FILEIO_WRITE_METHODDEF
    _IO_FILEIO_SEEK_METHODDEF
    _IO_FILEIO_TELL_METHODDEF
    _IO_FILEIO_TRUNCATE_METHODDEF
    _IO_FILEIO_CLOSE_METHODDEF
    _IO_FILEIO_SEEKABLE_METHODDEF
    _IO_FILEIO_READABLE_METHODDEF
    _IO_FILEIO_WRITABLE_METHODDEF
    _IO_FILEIO_FILENO_METHODDEF
    _IO_FILEIO_ISATTY_METHODDEF
    {"_dealloc_warn", (PyCFunction)fileio_dealloc_warn, METH_O, NULL},
    {NULL,           NULL}             /* sentinel */
};

/* 'closed' and 'mode' are attributes for backwards compatibility reasons. */

static PyObject *
get_closed(fileio *self, void *closure)
{
    return PyBool_FromLong((long)(self->fd < 0));
}

static PyObject *
get_closefd(fileio *self, void *closure)
{
    return PyBool_FromLong((long)(self->closefd));
}

static PyObject *
get_mode(fileio *self, void *closure)
{
    return PyUnicode_FromString(mode_string(self));
}

static PyGetSetDef fileio_getsetlist[] = {
    {"closed", (getter)get_closed, NULL, "True if the file is closed"},
    {"closefd", (getter)get_closefd, NULL,
        "True if the file descriptor will be closed by close()."},
    {"mode", (getter)get_mode, NULL, "String giving the file mode"},
    {NULL},
};

static PyMemberDef fileio_members[] = {
    {"_blksize", T_UINT, offsetof(fileio, blksize), 0},
    {"_finalizing", T_BOOL, offsetof(fileio, finalizing), 0},
    {NULL}
};

PyTypeObject PyFileIO_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_io.FileIO",
    sizeof(fileio),
    0,
    (destructor)fileio_dealloc,                 /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    (reprfunc)fileio_repr,                      /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, 
    // | Py_TPFLAGS_HAVE_GC,                   /* tp_flags */
    _io_FileIO___init____doc__,                 /* tp_doc */
    0, // (traverseproc)fileio_traverse,              /* tp_traverse */
    (inquiry)fileio_clear,                      /* tp_clear */
    0,                                          /* tp_richcompare */
    offsetof(fileio, weakreflist),              /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    fileio_methods,                             /* tp_methods */
    fileio_members,                             /* tp_members */
    fileio_getsetlist,                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    offsetof(fileio, dict),                     /* tp_dictoffset */
    _io_FileIO___init__,                        /* tp_init */
    PyType_GenericAlloc,                        /* tp_alloc */
    fileio_new,                                 /* tp_new */
    PyMem_Free,                            /* tp_free */
    0,                                          /* tp_is_gc */
    0,                                          /* tp_bases */
    0,                                          /* tp_mro */
    0,                                          /* tp_cache */
    0,                                          /* tp_subclasses */
    0,                                          /* tp_weaklist */
    0,                                          /* tp_del */
    0,                                          /* tp_version_tag */
    0,                                          /* tp_finalize */
};
