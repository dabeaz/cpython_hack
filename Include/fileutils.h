#ifndef Py_FILEUTILS_H
#define Py_FILEUTILS_H
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    _Py_ERROR_UNKNOWN=0,
    _Py_ERROR_STRICT,
    _Py_ERROR_SURROGATEESCAPE,
    _Py_ERROR_REPLACE,
    _Py_ERROR_IGNORE,
    _Py_ERROR_BACKSLASHREPLACE,
    _Py_ERROR_SURROGATEPASS,
    _Py_ERROR_XMLCHARREFREPLACE,
    _Py_ERROR_OTHER
} _Py_error_handler;

PyAPI_FUNC(_Py_error_handler) _Py_GetErrorHandler(const char *errors);

PyAPI_FUNC(PyObject *) _Py_device_encoding(int);

#if defined(MS_WINDOWS) || defined(__APPLE__)
    /* On Windows, the count parameter of read() is an int (bpo-9015, bpo-9611).
       On macOS 10.13, read() and write() with more than INT_MAX bytes
       fail with EINVAL (bpo-24658). */
#   define _PY_READ_MAX  INT_MAX
#   define _PY_WRITE_MAX INT_MAX
#else
    /* write() should truncate the input to PY_SSIZE_T_MAX bytes,
       but it's safer to do it ourself to have a portable behaviour */
#   define _PY_READ_MAX  PY_SSIZE_T_MAX
#   define _PY_WRITE_MAX PY_SSIZE_T_MAX
#endif

#  define _Py_stat_struct stat

PyAPI_FUNC(int) _Py_fstat(
    int fd,
    struct _Py_stat_struct *status);

PyAPI_FUNC(int) _Py_fstat_noraise(
    int fd,
    struct _Py_stat_struct *status);

PyAPI_FUNC(int) _Py_stat(
    PyObject *path,
    struct stat *status);

PyAPI_FUNC(int) _Py_open(
    const char *pathname,
    int flags);

PyAPI_FUNC(int) _Py_open_noraise(
    const char *pathname,
    int flags);

PyAPI_FUNC(FILE*) _Py_fopen(
    const char *pathname,
    const char *mode);

PyAPI_FUNC(FILE*) _Py_fopen_obj(
    PyObject *path,
    const char *mode);

PyAPI_FUNC(Py_ssize_t) _Py_read(
    int fd,
    void *buf,
    size_t count);

PyAPI_FUNC(Py_ssize_t) _Py_write(
    int fd,
    const void *buf,
    size_t count);

PyAPI_FUNC(Py_ssize_t) _Py_write_noraise(
    int fd,
    const void *buf,
    size_t count);

#ifdef HAVE_READLINK
PyAPI_FUNC(int) _Py_readlink(
    const char *path,
    char *buf,
    /* Number of characters of 'buf' buffer
       including the trailing NUL character */
    size_t buflen);
#endif

#ifdef HAVE_REALPATH
PyAPI_FUNC(char*) _Py_realpath(
    const char *path,
    char *resolved_path,
    /* Number of characters of 'resolved_path' buffer
       including the trailing NUL character */
    size_t resolved_path_len);

#endif

PyAPI_FUNC(int) _Py_isabs(const char *path);

PyAPI_FUNC(int) _Py_abspath(const char *path, char **abspath_p);

PyAPI_FUNC(char*) _Py_getcwd(
    char *buf,
    /* Number of characters of 'buf' buffer
       including the trailing NUL character */
    size_t buflen);

PyAPI_FUNC(int) _Py_get_inheritable(int fd);

PyAPI_FUNC(int) _Py_set_inheritable(int fd, int inheritable,
                                    int *atomic_flag_works);

PyAPI_FUNC(int) _Py_set_inheritable_async_safe(int fd, int inheritable,
                                               int *atomic_flag_works);

PyAPI_FUNC(int) _Py_dup(int fd);

#ifndef MS_WINDOWS
PyAPI_FUNC(int) _Py_get_blocking(int fd);

PyAPI_FUNC(int) _Py_set_blocking(int fd, int blocking);
#endif   /* !MS_WINDOWS */

#ifdef __cplusplus
}
#endif
#endif /* !Py_FILEUTILS_H */
