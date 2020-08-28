#include "Python.h"
#include "pycore_fileutils.h"
#include "osdefs.h"               // SEP

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#ifdef O_CLOEXEC
/* Does open() support the O_CLOEXEC flag? Possible values:

   -1: unknown
    0: open() ignores O_CLOEXEC flag, ex: Linux kernel older than 2.6.23
    1: open() supports O_CLOEXEC flag, close-on-exec is set

   The flag is used by _Py_open(), _Py_open_noraise(), io.FileIO
   and os.open(). */
int _Py_open_cloexec_works = -1;
#endif


static int
get_surrogateescape(_Py_error_handler errors, int *surrogateescape)
{
    switch (errors)
    {
    case _Py_ERROR_STRICT:
        *surrogateescape = 0;
        return 0;
    case _Py_ERROR_SURROGATEESCAPE:
        *surrogateescape = 1;
        return 0;
    default:
        return -1;
    }
}


PyObject *
_Py_device_encoding(int fd)
{
    int valid;
    
    valid = isatty(fd);
    
    if (!valid)
        Py_RETURN_NONE;
#if defined(CODESET)
    {
        char *codeset = nl_langinfo(CODESET);
        if (codeset != NULL && codeset[0] != 0)
            return PyUnicode_FromString(codeset);
    }
#endif
    Py_RETURN_NONE;
}

/* Decode a byte string from the locale encoding with the
   surrogateescape error handler: undecodable bytes are decoded as characters
   in range U+DC80..U+DCFF. If a byte sequence can be decoded as a surrogate
   character, escape the bytes using the surrogateescape error handler instead
   of decoding them.

   Return a pointer to a newly allocated wide character string, use
   PyMem_RawFree() to free the memory. If size is not NULL, write the number of
   wide characters excluding the null character into *size

   Return NULL on decoding error or memory allocation error. If *size* is not
   NULL, *size is set to (size_t)-1 on memory error or set to (size_t)-2 on
   decoding error.

   Decoding errors should never happen, unless there is a bug in the C
   library.

   Use the Py_EncodeLocale() function to encode the character string back to a
   byte string. */
wchar_t*
Py_DecodeLocale(const char* arg, size_t *wlen)
{
  #if 1
  wchar_t *wstr;
  Py_ssize_t n, len;
  len = strlen(arg);
  wstr = (wchar_t *) PyMem_RawMalloc((len+1)*sizeof(wchar_t));
  if (wstr) {
    for (n = 0; n < len; n++) {
      wstr[n] = arg[n];
    }
    wstr[n] = L'\0';
  }
  if (wlen) {
    *wlen = len;
  }
  return wstr;
  #endif
}


static int
encode_current_locale(const wchar_t *text, char **str,
                      size_t *error_pos, const char **reason,
                      int raw_malloc, _Py_error_handler errors)
{
    const size_t len = wcslen(text);
    char *result = NULL, *bytes = NULL;
    size_t i, size, converted;
    wchar_t c, buf[2];

    int surrogateescape;
    if (get_surrogateescape(errors, &surrogateescape) < 0) {
        return -3;
    }

    /* The function works in two steps:
       1. compute the length of the output buffer in bytes (size)
       2. outputs the bytes */
    size = 0;
    buf[1] = 0;
    while (1) {
        for (i=0; i < len; i++) {
            c = text[i];
            if (c >= 0xdc80 && c <= 0xdcff) {
                if (!surrogateescape) {
                    goto encode_error;
                }
                /* UTF-8b surrogate */
                if (bytes != NULL) {
                    *bytes++ = c - 0xdc00;
                    size--;
                }
                else {
                    size++;
                }
                continue;
            }
            else {
                buf[0] = c;
                if (bytes != NULL) {
                    converted = wcstombs(bytes, buf, size);
                }
                else {
                    converted = wcstombs(NULL, buf, 0);
                }
                if (converted == (size_t)-1) {
                    goto encode_error;
                }
                if (bytes != NULL) {
                    bytes += converted;
                    size -= converted;
                }
                else {
                    size += converted;
                }
            }
        }
        if (result != NULL) {
            *bytes = '\0';
            break;
        }

        size += 1; /* nul byte at the end */
        if (raw_malloc) {
            result = PyMem_RawMalloc(size);
        }
        else {
            result = PyMem_Malloc(size);
        }
        if (result == NULL) {
            return -1;
        }
        bytes = result;
    }
    *str = result;
    return 0;

encode_error:
    if (raw_malloc) {
        PyMem_RawFree(result);
    }
    else {
        PyMem_Free(result);
    }
    if (error_pos != NULL) {
        *error_pos = i;
    }
    if (reason) {
        *reason = "encoding error";
    }
    return -2;
}


/* Encode a string to the locale encoding.

   Parameters:

   * raw_malloc: if non-zero, allocate memory using PyMem_RawMalloc() instead
     of PyMem_Malloc().
   * current_locale: if non-zero, use the current LC_CTYPE, otherwise use
     Python filesystem encoding.
   * errors: error handler like "strict" or "surrogateescape".

   Return value:

    0: success, *str is set to a newly allocated decoded string.
   -1: memory allocation failure
   -2: encoding error, set *error_pos and *reason (if set).
   -3: the error handler 'errors' is not supported.
 */
static int
encode_locale_ex(const wchar_t *text, char **str, size_t *error_pos,
                 const char **reason,
                 int raw_malloc, int current_locale, _Py_error_handler errors)
{
  return encode_current_locale(text, str, error_pos, reason,
                                     raw_malloc, errors);
}

static char*
encode_locale(const wchar_t *text, size_t *error_pos,
              int raw_malloc, int current_locale)
{
    char *str;
    int res = encode_locale_ex(text, &str, error_pos, NULL,
                               raw_malloc, current_locale,
                               _Py_ERROR_SURROGATEESCAPE);
    if (res != -2 && error_pos) {
        *error_pos = (size_t)-1;
    }
    if (res != 0) {
        return NULL;
    }
    return str;
}

/* Encode a wide character string to the locale encoding with the
   surrogateescape error handler: surrogate characters in the range
   U+DC80..U+DCFF are converted to bytes 0x80..0xFF.

   Return a pointer to a newly allocated byte string, use PyMem_Free() to free
   the memory. Return NULL on encoding or memory allocation error.

   If error_pos is not NULL, *error_pos is set to (size_t)-1 on success, or set
   to the index of the invalid character on encoding error.

   Use the Py_DecodeLocale() function to decode the bytes string back to a wide
   character string. */
char*
Py_EncodeLocale(const wchar_t *text, size_t *error_pos)
{
    return encode_locale(text, error_pos, 0, 0);
}


/* Similar to Py_EncodeLocale(), but result must be freed by PyMem_RawFree()
   instead of PyMem_Free(). */
char*
_Py_EncodeLocaleRaw(const wchar_t *text, size_t *error_pos)
{
    return encode_locale(text, error_pos, 1, 0);
}


int
_Py_EncodeLocaleEx(const wchar_t *text, char **str,
                   size_t *error_pos, const char **reason,
                   int current_locale, _Py_error_handler errors)
{
    return encode_locale_ex(text, str, error_pos, reason, 1,
                            current_locale, errors);
}

/* Return information about a file.

   On POSIX, use fstat().

   On Windows, use GetFileType() and GetFileInformationByHandle() which support
   files larger than 2 GiB.  fstat() may fail with EOVERFLOW on files larger
   than 2 GiB because the file size type is a signed 32-bit integer: see issue
   #23152.

   On Windows, set the last Windows error and return nonzero on error. On
   POSIX, set errno and return nonzero on error. Fill status and return 0 on
   success. */
int
_Py_fstat_noraise(int fd, struct _Py_stat_struct *status)
{
    return fstat(fd, status);
}

/* Return information about a file.

   On POSIX, use fstat().

   On Windows, use GetFileType() and GetFileInformationByHandle() which support
   files larger than 2 GiB.  fstat() may fail with EOVERFLOW on files larger
   than 2 GiB because the file size type is a signed 32-bit integer: see issue
   #23152.

   Raise an exception and return -1 on error. On Windows, set the last Windows
   error on error. On POSIX, set errno on error. Fill status and return 0 on
   success.

   Release the GIL to call GetFileType() and GetFileInformationByHandle(), or
   to call fstat(). The caller must hold the GIL. */
int
_Py_fstat(int fd, struct _Py_stat_struct *status)
{
    int res;

    res = _Py_fstat_noraise(fd, status);
    

    if (res != 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    return 0;
}

/* Call _wstat() on Windows, or encode the path to the filesystem encoding and
   call stat() otherwise. Only fill st_mode attribute on Windows.

   Return 0 on success, -1 on _wstat() / stat() error, -2 if an exception was
   raised. */

int
_Py_stat(PyObject *path, struct stat *statbuf)
{
    int ret;
    PyObject *bytes;
    char *cpath;

    bytes = PyUnicode_AsBytes(path);
    if (bytes == NULL)
        return -2;

    /* check for embedded null bytes */
    if (PyBytes_AsStringAndSize(bytes, &cpath, NULL) == -1) {
        Py_DECREF(bytes);
        return -2;
    }

    ret = stat(cpath, statbuf);
    Py_DECREF(bytes);
    return ret;
}


/* This function MUST be kept async-signal-safe on POSIX when raise=0. */
static int
get_inheritable(int fd, int raise)
{
    int flags;

    flags = fcntl(fd, F_GETFD, 0);
    if (flags == -1) {
        if (raise)
            PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    return !(flags & FD_CLOEXEC);
}

/* Get the inheritable flag of the specified file descriptor.
   Return 1 if the file descriptor can be inherited, 0 if it cannot,
   raise an exception and return -1 on error. */
int
_Py_get_inheritable(int fd)
{
    return get_inheritable(fd, 1);
}


/* This function MUST be kept async-signal-safe on POSIX when raise=0. */
static int
set_inheritable(int fd, int inheritable, int raise, int *atomic_flag_works)
{
#if defined(HAVE_SYS_IOCTL_H) && defined(FIOCLEX) && defined(FIONCLEX)
    static int ioctl_works = -1;
    int request;
    int err;
#endif
    int flags, new_flags;
    int res;
    /* atomic_flag_works can only be used to make the file descriptor
       non-inheritable */
    assert(!(atomic_flag_works != NULL && inheritable));

    if (atomic_flag_works != NULL && !inheritable) {
        if (*atomic_flag_works == -1) {
            int isInheritable = get_inheritable(fd, raise);
            if (isInheritable == -1)
                return -1;
            *atomic_flag_works = !isInheritable;
        }

        if (*atomic_flag_works)
            return 0;
    }

#if defined(HAVE_SYS_IOCTL_H) && defined(FIOCLEX) && defined(FIONCLEX)
    if (ioctl_works != 0 && raise != 0) {
        /* fast-path: ioctl() only requires one syscall */
        /* caveat: raise=0 is an indicator that we must be async-signal-safe
         * thus avoid using ioctl() so we skip the fast-path. */
        if (inheritable)
            request = FIONCLEX;
        else
            request = FIOCLEX;
        err = ioctl(fd, request, NULL);
        if (!err) {
            ioctl_works = 1;
            return 0;
        }

        if (errno != ENOTTY && errno != EACCES) {
            if (raise)
                PyErr_SetFromErrno(PyExc_OSError);
            return -1;
        }
        else {
            /* Issue #22258: Here, ENOTTY means "Inappropriate ioctl for
               device". The ioctl is declared but not supported by the kernel.
               Remember that ioctl() doesn't work. It is the case on
               Illumos-based OS for example.

               Issue #27057: When SELinux policy disallows ioctl it will fail
               with EACCES. While FIOCLEX is safe operation it may be
               unavailable because ioctl was denied altogether.
               This can be the case on Android. */
            ioctl_works = 0;
        }
        /* fallback to fcntl() if ioctl() does not work */
    }
#endif

    /* slow-path: fcntl() requires two syscalls */
    flags = fcntl(fd, F_GETFD);
    if (flags < 0) {
        if (raise)
            PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    if (inheritable) {
        new_flags = flags & ~FD_CLOEXEC;
    }
    else {
        new_flags = flags | FD_CLOEXEC;
    }

    if (new_flags == flags) {
        /* FD_CLOEXEC flag already set/cleared: nothing to do */
        return 0;
    }

    res = fcntl(fd, F_SETFD, new_flags);
    if (res < 0) {
        if (raise)
            PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    return 0;
}

/* Make the file descriptor non-inheritable.
   Return 0 on success, set errno and return -1 on error. */
static int
make_non_inheritable(int fd)
{
    return set_inheritable(fd, 0, 0, NULL);
}

/* Set the inheritable flag of the specified file descriptor.
   On success: return 0, on error: raise an exception and return -1.

   If atomic_flag_works is not NULL:

    * if *atomic_flag_works==-1, check if the inheritable is set on the file
      descriptor: if yes, set *atomic_flag_works to 1, otherwise set to 0 and
      set the inheritable flag
    * if *atomic_flag_works==1: do nothing
    * if *atomic_flag_works==0: set inheritable flag to False

   Set atomic_flag_works to NULL if no atomic flag was used to create the
   file descriptor.

   atomic_flag_works can only be used to make a file descriptor
   non-inheritable: atomic_flag_works must be NULL if inheritable=1. */
int
_Py_set_inheritable(int fd, int inheritable, int *atomic_flag_works)
{
    return set_inheritable(fd, inheritable, 1, atomic_flag_works);
}

/* Same as _Py_set_inheritable() but on error, set errno and
   don't raise an exception.
   This function is async-signal-safe. */
int
_Py_set_inheritable_async_safe(int fd, int inheritable, int *atomic_flag_works)
{
    return set_inheritable(fd, inheritable, 0, atomic_flag_works);
}

static int
_Py_open_impl(const char *pathname, int flags, int gil_held)
{
    int fd;
    int async_err = 0;
    int *atomic_flag_works;

#if defined(O_CLOEXEC)
    atomic_flag_works = &_Py_open_cloexec_works;
    flags |= O_CLOEXEC;
#else
    atomic_flag_works = NULL;
#endif

    if (gil_held) {
        do {
            
            fd = open(pathname, flags);
            
        } while (fd < 0
                 && errno == EINTR); //  && !(async_err = PyErr_CheckSignals()));
        if (async_err)
            return -1;
        if (fd < 0) {
            PyErr_SetFromErrnoWithFilename(PyExc_OSError, pathname);
            return -1;
        }
    }
    else {
        fd = open(pathname, flags);
        if (fd < 0)
            return -1;
    }

    if (set_inheritable(fd, 0, gil_held, atomic_flag_works) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Open a file with the specified flags (wrapper to open() function).
   Return a file descriptor on success. Raise an exception and return -1 on
   error.

   The file descriptor is created non-inheritable.

   When interrupted by a signal (open() fails with EINTR), retry the syscall,
   except if the Python signal handler raises an exception.

   Release the GIL to call open(). The caller must hold the GIL. */
int
_Py_open(const char *pathname, int flags)
{
    /* _Py_open() must be called with the GIL held. */
    return _Py_open_impl(pathname, flags, 1);
}

/* Open a file with the specified flags (wrapper to open() function).
   Return a file descriptor on success. Set errno and return -1 on error.

   The file descriptor is created non-inheritable.

   If interrupted by a signal, fail with EINTR. */
int
_Py_open_noraise(const char *pathname, int flags)
{
    return _Py_open_impl(pathname, flags, 0);
}

/* Open a file. Use _wfopen() on Windows, encode the path to the locale
   encoding and use fopen() otherwise.

   The file descriptor is created non-inheritable.

   If interrupted by a signal, fail with EINTR. */
FILE *
_Py_wfopen(const wchar_t *path, const wchar_t *mode)
{
    FILE *f;
    char *cpath;
    char cmode[10];
    size_t r;
    r = wcstombs(cmode, mode, 10);
    if (r == (size_t)-1 || r >= 10) {
        errno = EINVAL;
        return NULL;
    }
    cpath = _Py_EncodeLocaleRaw(path, NULL);
    if (cpath == NULL) {
        return NULL;
    }
    f = fopen(cpath, cmode);
    PyMem_RawFree(cpath);
    if (f == NULL)
        return NULL;
    if (make_non_inheritable(fileno(f)) < 0) {
        fclose(f);
        return NULL;
    }
    return f;
}

/* Wrapper to fopen().

   The file descriptor is created non-inheritable.

   If interrupted by a signal, fail with EINTR. */
FILE*
_Py_fopen(const char *pathname, const char *mode)
{

    FILE *f = fopen(pathname, mode);
    if (f == NULL)
        return NULL;
    if (make_non_inheritable(fileno(f)) < 0) {
        fclose(f);
        return NULL;
    }
    return f;
}

/* Open a file. Call _wfopen() on Windows, or encode the path to the filesystem
   encoding and call fopen() otherwise.

   Return the new file object on success. Raise an exception and return NULL
   on error.

   The file descriptor is created non-inheritable.

   When interrupted by a signal (open() fails with EINTR), retry the syscall,
   except if the Python signal handler raises an exception.

   Release the GIL to call _wfopen() or fopen(). The caller must hold
   the GIL. */
FILE*
_Py_fopen_obj(PyObject *path, const char *mode)
{
    FILE *f;
    int async_err = 0;
    PyObject *bytes;
    const char *path_bytes;

    if (!PyUnicode_FSConverter(path, &bytes))
        return NULL;
    path_bytes = PyBytes_AS_STRING(bytes);
    do {
        
        f = fopen(path_bytes, mode);
        
    } while (f == NULL
             && errno == EINTR); //  && !(async_err = PyErr_CheckSignals()));

    Py_DECREF(bytes);
    if (async_err)
        return NULL;

    if (f == NULL) {
        PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path);
        return NULL;
    }

    if (set_inheritable(fileno(f), 0, 1, NULL) < 0) {
        fclose(f);
        return NULL;
    }
    return f;
}

/* Read count bytes from fd into buf.

   On success, return the number of read bytes, it can be lower than count.
   If the current file offset is at or past the end of file, no bytes are read,
   and read() returns zero.

   On error, raise an exception, set errno and return -1.

   When interrupted by a signal (read() fails with EINTR), retry the syscall.
   If the Python signal handler raises an exception, the function returns -1
   (the syscall is not retried).

   Release the GIL to call read(). The caller must hold the GIL. */
Py_ssize_t
_Py_read(int fd, void *buf, size_t count)
{
    Py_ssize_t n;
    int err;
    int async_err = 0;

    /* _Py_read() must not be called with an exception set, otherwise the
     * caller may think that read() was interrupted by a signal and the signal
     * handler raised an exception. */
    assert(!PyErr_Occurred());

    if (count > _PY_READ_MAX) {
        count = _PY_READ_MAX;
    }

    
    do {
        
        errno = 0;
        n = read(fd, buf, count);
        /* save/restore errno because PyErr_CheckSignals()
         * and PyErr_SetFromErrno() can modify it */
        err = errno;
        
	  } while (n < 0 && err == EINTR); // &&
    // !(async_err = PyErr_CheckSignals()));
    

    if (async_err) {
        /* read() was interrupted by a signal (failed with EINTR)
         * and the Python signal handler raised an exception */
        errno = err;
        assert(errno == EINTR && PyErr_Occurred());
        return -1;
    }
    if (n < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        errno = err;
        return -1;
    }

    return n;
}

static Py_ssize_t
_Py_write_impl(int fd, const void *buf, size_t count, int gil_held)
{
    Py_ssize_t n;
    int err;
    int async_err = 0;

    
    if (count > _PY_WRITE_MAX) {
        count = _PY_WRITE_MAX;
    }

    if (gil_held) {
        do {
            
            errno = 0;
            n = write(fd, buf, count);
            /* save/restore errno because PyErr_CheckSignals()
             * and PyErr_SetFromErrno() can modify it */
            err = errno;
            
	      } while (n < 0 && err == EINTR); // &&
	  //                !(async_err = PyErr_CheckSignals()));
    }
    else {
        do {
            errno = 0;
            n = write(fd, buf, count);
            err = errno;
        } while (n < 0 && err == EINTR);
    }
    

    if (async_err) {
        /* write() was interrupted by a signal (failed with EINTR)
           and the Python signal handler raised an exception (if gil_held is
           nonzero). */
        errno = err;
        assert(errno == EINTR && (!gil_held || PyErr_Occurred()));
        return -1;
    }
    if (n < 0) {
        if (gil_held)
            PyErr_SetFromErrno(PyExc_OSError);
        errno = err;
        return -1;
    }

    return n;
}

/* Write count bytes of buf into fd.

   On success, return the number of written bytes, it can be lower than count
   including 0. On error, raise an exception, set errno and return -1.

   When interrupted by a signal (write() fails with EINTR), retry the syscall.
   If the Python signal handler raises an exception, the function returns -1
   (the syscall is not retried). */

Py_ssize_t
_Py_write(int fd, const void *buf, size_t count)
{

    /* _Py_write() must not be called with an exception set, otherwise the
     * caller may think that write() was interrupted by a signal and the signal
     * handler raised an exception. */
    assert(!PyErr_Occurred());

    return _Py_write_impl(fd, buf, count, 1);
}

/* Write count bytes of buf into fd.
 *
 * On success, return the number of written bytes, it can be lower than count
 * including 0. On error, set errno and return -1.
 *
 * When interrupted by a signal (write() fails with EINTR), retry the syscall
 * without calling the Python signal handler. */
Py_ssize_t
_Py_write_noraise(int fd, const void *buf, size_t count)
{
    return _Py_write_impl(fd, buf, count, 0);
}

#ifdef HAVE_READLINK

/* Read value of symbolic link. Encode the path to the locale encoding, decode
   the result from the locale encoding.

   Return -1 on encoding error, on readlink() error, if the internal buffer is
   too short, on decoding error, or if 'buf' is too short. */
int
_Py_wreadlink(const wchar_t *path, wchar_t *buf, size_t buflen)
{
    char *cpath;
    char cbuf[MAXPATHLEN];
    size_t cbuf_len = Py_ARRAY_LENGTH(cbuf);
    wchar_t *wbuf;
    Py_ssize_t res;
    size_t r1;

    cpath = _Py_EncodeLocaleRaw(path, NULL);
    if (cpath == NULL) {
        errno = EINVAL;
        return -1;
    }
    res = readlink(cpath, cbuf, cbuf_len);
    PyMem_RawFree(cpath);
    if (res == -1) {
        return -1;
    }
    if ((size_t)res == cbuf_len) {
        errno = EINVAL;
        return -1;
    }
    cbuf[res] = '\0'; /* buf will be null terminated */
    wbuf = Py_DecodeLocale(cbuf, &r1);
    if (wbuf == NULL) {
        errno = EINVAL;
        return -1;
    }
    /* wbuf must have space to store the trailing NUL character */
    if (buflen <= r1) {
        PyMem_RawFree(wbuf);
        errno = EINVAL;
        return -1;
    }
    wcsncpy(buf, wbuf, buflen);
    PyMem_RawFree(wbuf);
    return (int)r1;
}
#endif

#ifdef HAVE_REALPATH

/* Return the canonicalized absolute pathname. Encode path to the locale
   encoding, decode the result from the locale encoding.

   Return NULL on encoding error, realpath() error, decoding error
   or if 'resolved_path' is too short. */
wchar_t*
_Py_wrealpath(const wchar_t *path,
              wchar_t *resolved_path, size_t resolved_path_len)
{
    char *cpath;
    char cresolved_path[MAXPATHLEN];
    wchar_t *wresolved_path;
    char *res;
    size_t r;
    cpath = _Py_EncodeLocaleRaw(path, NULL);
    if (cpath == NULL) {
        errno = EINVAL;
        return NULL;
    }
    res = realpath(cpath, cresolved_path);
    PyMem_RawFree(cpath);
    if (res == NULL)
        return NULL;

    wresolved_path = Py_DecodeLocale(cresolved_path, &r);
    if (wresolved_path == NULL) {
        errno = EINVAL;
        return NULL;
    }
    /* wresolved_path must have space to store the trailing NUL character */
    if (resolved_path_len <= r) {
        PyMem_RawFree(wresolved_path);
        errno = EINVAL;
        return NULL;
    }
    wcsncpy(resolved_path, wresolved_path, resolved_path_len);
    PyMem_RawFree(wresolved_path);
    return resolved_path;
}
#endif


int
_Py_isabs(const wchar_t *path)
{
    return (path[0] == SEP);
}

/* Get an absolute path.
   On error (ex: fail to get the current directory), return -1.
   On memory allocation failure, set *abspath_p to NULL and return 0.
   On success, return a newly allocated to *abspath_p to and return 0.
   The string must be freed by PyMem_RawFree(). */
int
_Py_abspath(const wchar_t *path, wchar_t **abspath_p)
{
    if (_Py_isabs(path)) {
        *abspath_p = _PyMem_RawWcsdup(path);
        return 0;
    }

    wchar_t cwd[MAXPATHLEN + 1];
    cwd[Py_ARRAY_LENGTH(cwd) - 1] = 0;
    if (!_Py_wgetcwd(cwd, Py_ARRAY_LENGTH(cwd) - 1)) {
        /* unable to get the current directory */
        return -1;
    }

    size_t cwd_len = wcslen(cwd);
    size_t path_len = wcslen(path);
    size_t len = cwd_len + 1 + path_len + 1;
    if (len <= (size_t)PY_SSIZE_T_MAX / sizeof(wchar_t)) {
        *abspath_p = PyMem_RawMalloc(len * sizeof(wchar_t));
    }
    else {
        *abspath_p = NULL;
    }
    if (*abspath_p == NULL) {
        return 0;
    }

    wchar_t *abspath = *abspath_p;
    memcpy(abspath, cwd, cwd_len * sizeof(wchar_t));
    abspath += cwd_len;

    *abspath = (wchar_t)SEP;
    abspath++;

    memcpy(abspath, path, path_len * sizeof(wchar_t));
    abspath += path_len;

    *abspath = 0;
    return 0;
}


/* Get the current directory. buflen is the buffer size in wide characters
   including the null character. Decode the path from the locale encoding.

   Return NULL on getcwd() error, on decoding error, or if 'buf' is
   too short. */
wchar_t*
_Py_wgetcwd(wchar_t *buf, size_t buflen)
{
    char fname[MAXPATHLEN];
    wchar_t *wname;
    size_t len;

    if (getcwd(fname, Py_ARRAY_LENGTH(fname)) == NULL)
        return NULL;
    wname = Py_DecodeLocale(fname, &len);
    if (wname == NULL)
        return NULL;
    /* wname must have space to store the trailing NUL character */
    if (buflen <= len) {
        PyMem_RawFree(wname);
        return NULL;
    }
    wcsncpy(buf, wname, buflen);
    PyMem_RawFree(wname);
    return buf;
}

/* Duplicate a file descriptor. The new file descriptor is created as
   non-inheritable. Return a new file descriptor on success, raise an OSError
   exception and return -1 on error.

   The GIL is released to call dup(). The caller must hold the GIL. */
int
_Py_dup(int fd)
{
#if defined(HAVE_FCNTL_H) && defined(F_DUPFD_CLOEXEC)
    
    
    fd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    
    
    if (fd < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

#else
    fd = dup(fd);
    
    
    if (fd < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    if (_Py_set_inheritable(fd, 0, NULL) < 0) {
        
        close(fd);
        
        return -1;
    }
#endif
    return fd;
}

/* Get the blocking mode of the file descriptor.
   Return 0 if the O_NONBLOCK flag is set, 1 if the flag is cleared,
   raise an exception and return -1 on error. */
int
_Py_get_blocking(int fd)
{
    int flags;
    
    flags = fcntl(fd, F_GETFL, 0);
    
    if (flags < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    return !(flags & O_NONBLOCK);
}

/* Set the blocking mode of the specified file descriptor.

   Set the O_NONBLOCK flag if blocking is False, clear the O_NONBLOCK flag
   otherwise.

   Return 0 on success, raise an exception and return -1 on error. */
int
_Py_set_blocking(int fd, int blocking)
{
#if defined(HAVE_SYS_IOCTL_H) && defined(FIONBIO)
    int arg = !blocking;
    if (ioctl(fd, FIONBIO, &arg) < 0)
        goto error;
#else
    int flags, res;

    
    flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        if (blocking)
            flags = flags & (~O_NONBLOCK);
        else
            flags = flags | O_NONBLOCK;

        res = fcntl(fd, F_SETFL, flags);
    } else {
        res = -1;
    }
    

    if (res < 0)
        goto error;
#endif
    return 0;

error:
    PyErr_SetFromErrno(PyExc_OSError);
    return -1;
}
