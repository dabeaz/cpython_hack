/* POSIX module implementation */

/* This file is also used for Windows NT/MS-Win.  In that case the
   module actually calls itself 'nt', not 'posix', and a few
   functions are either unimplemented or implemented differently.  The source
   assumes that for Windows NT, the macro 'MS_WINDOWS' is defined independent
   of the compiler used.  Different compilers define their own feature
   test macro, e.g. '_MSC_VER'. */

#ifdef __APPLE__
   /*
    * Step 1 of support for weak-linking a number of symbols existing on
    * OSX 10.4 and later, see the comment in the #ifdef __APPLE__ block
    * at the end of this file for more information.
    */
#  pragma weak lchown
#  pragma weak statvfs
#  pragma weak fstatvfs

#endif /* __APPLE__ */

#define PY_SSIZE_T_CLEAN

#include "Python.h"
#ifdef MS_WINDOWS
   /* include <windows.h> early to avoid conflict with pycore_condvar.h:

        #define WIN32_LEAN_AND_MEAN
        #include <windows.h>

      FSCTL_GET_REPARSE_POINT is not exported with WIN32_LEAN_AND_MEAN. */
#  include <windows.h>
#endif

#include "pycore_ceval.h"         // _PyEval_ReInitThreads()
#include "pycore_import.h"        // _PyImport_ReInitLock()
#include "pycore_initconfig.h"    // _PyStatus_EXCEPTION()
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "structmember.h"         // PyMemberDef
#ifndef MS_WINDOWS
#  include "posixmodule.h"
#else
#  include "winreparse.h"
#endif

/* On android API level 21, 'AT_EACCESS' is not declared although
 * HAVE_FACCESSAT is defined. */
#ifdef __ANDROID__
#  undef HAVE_FACCESSAT
#endif

#include <stdio.h>  /* needed for ctermid() */

#ifdef __cplusplus
extern "C" {
#endif

PyDoc_STRVAR(posix__doc__,
"This module provides access to operating system functionality that is\n\
standardized by the C Standard and the POSIX standard (a thinly\n\
disguised Unix interface).  Refer to the library manual and\n\
corresponding Unix manual entries for more information on calls.");


#ifdef HAVE_SYS_UIO_H
#  include <sys/uio.h>
#endif

#ifdef HAVE_SYS_SYSMACROS_H
/* GNU C Library: major(), minor(), makedev() */
#  include <sys/sysmacros.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>           // WNOHANG
#endif
#ifdef HAVE_LINUX_WAIT_H
#  include <linux/wait.h>         // P_PIDFD
#endif

#ifdef HAVE_SIGNAL_H
#  include <signal.h>
#endif

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#ifdef HAVE_GRP_H
#  include <grp.h>
#endif

#ifdef HAVE_SYSEXITS_H
#  include <sysexits.h>
#endif

#ifdef HAVE_SYS_LOADAVG_H
#  include <sys/loadavg.h>
#endif

#ifdef HAVE_SYS_SENDFILE_H
#  include <sys/sendfile.h>
#endif

#if defined(__APPLE__)
#  include <copyfile.h>
#endif


#if defined(HAVE_SYS_XATTR_H) && defined(__GLIBC__) && !defined(__FreeBSD_kernel__) && !defined(__GNU__)
#  define USE_XATTRS
#endif

#ifdef USE_XATTRS
#  include <sys/xattr.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__APPLE__)
#  ifdef HAVE_SYS_SOCKET_H
#    include <sys/socket.h>
#  endif
#endif

#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>
#endif

#ifdef __hpux
#  include <sys/mpctl.h>
#endif

#if defined(__DragonFly__) || \
    defined(__OpenBSD__)   || \
    defined(__FreeBSD__)   || \
    defined(__NetBSD__)    || \
    defined(__APPLE__)
#  include <sys/sysctl.h>
#endif

#ifdef HAVE_LINUX_RANDOM_H
#  include <linux/random.h>
#endif
#ifdef HAVE_GETRANDOM_SYSCALL
#  include <sys/syscall.h>
#endif

#if defined(MS_WINDOWS)
#  define TERMSIZE_USE_CONIO
#elif defined(HAVE_SYS_IOCTL_H)
#  include <sys/ioctl.h>
#  if defined(HAVE_TERMIOS_H)
#    include <termios.h>
#  endif
#  if defined(TIOCGWINSZ)
#    define TERMSIZE_USE_IOCTL
#  endif
#endif /* MS_WINDOWS */

/* Various compilers have only certain posix functions */
/* XXX Gosh I wish these were all moved into pyconfig.h */
#if defined(__WATCOMC__) && !defined(__QNX__)           /* Watcom compiler */
#  define HAVE_OPENDIR    1
#  define HAVE_SYSTEM     1
#  include <process.h>
#else
#  ifdef _MSC_VER
     /* Microsoft compiler */
#    define HAVE_GETPPID    1
#    define HAVE_GETLOGIN   1
#    define HAVE_SPAWNV     1
#    define HAVE_EXECV      1
#    define HAVE_WSPAWNV    1
#    define HAVE_WEXECV     1
#    define HAVE_PIPE       1
#    define HAVE_SYSTEM     1
#    define HAVE_CWAIT      1
#    define HAVE_FSYNC      1
#    define fsync _commit
#  else
     /* Unix functions that the configure script doesn't check for */
#    ifndef __VXWORKS__
#      define HAVE_EXECV      1
#      define HAVE_FORK       1
#      if defined(__USLC__) && defined(__SCO_VERSION__)       /* SCO UDK Compiler */
#        define HAVE_FORK1      1
#      endif
#    endif
#    define HAVE_GETEGID    1
#    define HAVE_GETEUID    1
#    define HAVE_GETGID     1
#    define HAVE_GETPPID    1
#    define HAVE_GETUID     1
#    define HAVE_KILL       1
#    define HAVE_OPENDIR    1
#    define HAVE_PIPE       1
#    define HAVE_SYSTEM     1
#    define HAVE_WAIT       1
#    define HAVE_TTYNAME    1
#  endif  /* _MSC_VER */
#endif  /* ! __WATCOMC__ || __QNX__ */

_Py_IDENTIFIER(__fspath__);

/*[clinic input]
# one of the few times we lie about this name!
module os
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=94a0f0f978acae17]*/

#ifndef _MSC_VER

#if defined(__sgi)&&_COMPILER_VERSION>=700
/* declare ctermid_r if compiling with MIPSPro 7.x in ANSI C mode
   (default) */
extern char        *ctermid_r(char *);
#endif

#endif /* !_MSC_VER */

#if defined(__VXWORKS__)
#  include <vxCpuLib.h>
#  include <rtpLib.h>
#  include <wait.h>
#  include <taskLib.h>
#  ifndef _P_WAIT
#    define _P_WAIT          0
#    define _P_NOWAIT        1
#    define _P_NOWAITO       1
#  endif
#endif /* __VXWORKS__ */

#ifdef HAVE_POSIX_SPAWN
#  include <spawn.h>
#endif

#ifdef HAVE_UTIME_H
#  include <utime.h>
#endif /* HAVE_UTIME_H */

#ifdef HAVE_SYS_UTIME_H
#  include <sys/utime.h>
#  define HAVE_UTIME_H /* pretend we do for the rest of this file */
#endif /* HAVE_SYS_UTIME_H */

#ifdef HAVE_SYS_TIMES_H
#  include <sys/times.h>
#endif /* HAVE_SYS_TIMES_H */

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */

#ifdef HAVE_SYS_UTSNAME_H
#  include <sys/utsname.h>
#endif /* HAVE_SYS_UTSNAME_H */

#ifdef HAVE_DIRENT_H
#  include <dirent.h>
#  define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#  if defined(__WATCOMC__) && !defined(__QNX__)
#    include <direct.h>
#    define NAMLEN(dirent) strlen((dirent)->d_name)
#  else
#    define dirent direct
#    define NAMLEN(dirent) (dirent)->d_namlen
#  endif
#  ifdef HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif
#  ifdef HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif
#  ifdef HAVE_NDIR_H
#    include <ndir.h>
#  endif
#endif

#ifdef _MSC_VER
#  ifdef HAVE_DIRECT_H
#    include <direct.h>
#  endif
#  ifdef HAVE_IO_H
#    include <io.h>
#  endif
#  ifdef HAVE_PROCESS_H
#    include <process.h>
#  endif
#  ifndef IO_REPARSE_TAG_SYMLINK
#    define IO_REPARSE_TAG_SYMLINK (0xA000000CL)
#  endif
#  ifndef IO_REPARSE_TAG_MOUNT_POINT
#    define IO_REPARSE_TAG_MOUNT_POINT (0xA0000003L)
#  endif
#  include "osdefs.h"             // SEP
#  include <malloc.h>
#  include <windows.h>
#  include <shellapi.h>           // ShellExecute()
#  include <lmcons.h>             // UNLEN
#  define HAVE_SYMLINK
#endif /* _MSC_VER */

#ifndef MAXPATHLEN
#  if defined(PATH_MAX) && PATH_MAX > 1024
#    define MAXPATHLEN PATH_MAX
#  else
#    define MAXPATHLEN 1024
#  endif
#endif /* MAXPATHLEN */

#ifdef UNION_WAIT
   /* Emulate some macros on systems that have a union instead of macros */
#  ifndef WIFEXITED
#    define WIFEXITED(u_wait) (!(u_wait).w_termsig && !(u_wait).w_coredump)
#  endif
#  ifndef WEXITSTATUS
#    define WEXITSTATUS(u_wait) (WIFEXITED(u_wait)?((u_wait).w_retcode):-1)
#  endif
#  ifndef WTERMSIG
#    define WTERMSIG(u_wait) ((u_wait).w_termsig)
#  endif
#  define WAIT_TYPE union wait
#  define WAIT_STATUS_INT(s) (s.w_status)
#else
   /* !UNION_WAIT */
#  define WAIT_TYPE int
#  define WAIT_STATUS_INT(s) (s)
#endif /* UNION_WAIT */

/* Don't use the "_r" form if we don't need it (also, won't have a
   prototype for it, at least on Solaris -- maybe others as well?). */
#if defined(HAVE_CTERMID_R)
#  define USE_CTERMID_R
#endif

/* choose the appropriate stat and fstat functions and return structs */
#undef STAT
#undef FSTAT
#undef STRUCT_STAT
#ifdef MS_WINDOWS
#  define STAT win32_stat
#  define LSTAT win32_lstat
#  define FSTAT _Py_fstat_noraise
#  define STRUCT_STAT struct _Py_stat_struct
#else
#  define STAT stat
#  define LSTAT lstat
#  define FSTAT fstat
#  define STRUCT_STAT struct stat
#endif

#if defined(MAJOR_IN_MKDEV)
#  include <sys/mkdev.h>
#else
#  if defined(MAJOR_IN_SYSMACROS)
#    include <sys/sysmacros.h>
#  endif
#  if defined(HAVE_MKNOD) && defined(HAVE_SYS_MKDEV_H)
#    include <sys/mkdev.h>
#  endif
#endif

#ifdef MS_WINDOWS
#  define INITFUNC PyInit_nt
#  define MODNAME "nt"
#else
#  define INITFUNC PyInit_posix
#  define MODNAME "posix"
#endif

#if defined(__sun)
/* Something to implement in autoconf, not present in autoconf 2.69 */
#  define HAVE_STRUCT_STAT_ST_FSTYPE 1
#endif

/* memfd_create is either defined in sys/mman.h or sys/memfd.h
 * linux/memfd.h defines additional flags
 */
#ifdef HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#endif
#ifdef HAVE_SYS_MEMFD_H
#  include <sys/memfd.h>
#endif
#ifdef HAVE_LINUX_MEMFD_H
#  include <linux/memfd.h>
#endif

#ifdef _Py_MEMORY_SANITIZER
#  include <sanitizer/msan_interface.h>
#endif


/* Legacy wrapper */
void
PyOS_AfterFork(void)
{
}


#ifndef MS_WINDOWS
PyObject *
_PyLong_FromUid(uid_t uid)
{
    if (uid == (uid_t)-1)
        return PyLong_FromLong(-1);
    return PyLong_FromUnsignedLong(uid);
}

PyObject *
_PyLong_FromGid(gid_t gid)
{
    if (gid == (gid_t)-1)
        return PyLong_FromLong(-1);
    return PyLong_FromUnsignedLong(gid);
}

int
_Py_Uid_Converter(PyObject *obj, void *p)
{
    uid_t uid;
    PyObject *index;
    int overflow;
    long result;
    unsigned long uresult;

    index = _PyNumber_Index(obj);
    if (index == NULL) {
        PyErr_Format(PyExc_TypeError,
                     "uid should be integer, not %.200s",
                     _PyType_Name(Py_TYPE(obj)));
        return 0;
    }

    /*
     * Handling uid_t is complicated for two reasons:
     *  * Although uid_t is (always?) unsigned, it still
     *    accepts -1.
     *  * We don't know its size in advance--it may be
     *    bigger than an int, or it may be smaller than
     *    a long.
     *
     * So a bit of defensive programming is in order.
     * Start with interpreting the value passed
     * in as a signed long and see if it works.
     */

    result = PyLong_AsLongAndOverflow(index, &overflow);

    if (!overflow) {
        uid = (uid_t)result;

        if (result == -1) {
            if (PyErr_Occurred())
                goto fail;
            /* It's a legitimate -1, we're done. */
            goto success;
        }

        /* Any other negative number is disallowed. */
        if (result < 0)
            goto underflow;

        /* Ensure the value wasn't truncated. */
        if (sizeof(uid_t) < sizeof(long) &&
            (long)uid != result)
            goto underflow;
        goto success;
    }

    if (overflow < 0)
        goto underflow;

    /*
     * Okay, the value overflowed a signed long.  If it
     * fits in an *unsigned* long, it may still be okay,
     * as uid_t may be unsigned long on this platform.
     */
    uresult = PyLong_AsUnsignedLong(index);
    if (PyErr_Occurred()) {
        if (PyErr_ExceptionMatches(PyExc_OverflowError))
            goto overflow;
        goto fail;
    }

    uid = (uid_t)uresult;

    /*
     * If uid == (uid_t)-1, the user actually passed in ULONG_MAX,
     * but this value would get interpreted as (uid_t)-1  by chown
     * and its siblings.   That's not what the user meant!  So we
     * throw an overflow exception instead.   (We already
     * handled a real -1 with PyLong_AsLongAndOverflow() above.)
     */
    if (uid == (uid_t)-1)
        goto overflow;

    /* Ensure the value wasn't truncated. */
    if (sizeof(uid_t) < sizeof(long) &&
        (unsigned long)uid != uresult)
        goto overflow;
    /* fallthrough */

success:
    Py_DECREF(index);
    *(uid_t *)p = uid;
    return 1;

underflow:
    PyErr_SetString(PyExc_OverflowError,
                    "uid is less than minimum");
    goto fail;

overflow:
    PyErr_SetString(PyExc_OverflowError,
                    "uid is greater than maximum");
    /* fallthrough */

fail:
    Py_DECREF(index);
    return 0;
}

int
_Py_Gid_Converter(PyObject *obj, void *p)
{
    gid_t gid;
    PyObject *index;
    int overflow;
    long result;
    unsigned long uresult;

    index = _PyNumber_Index(obj);
    if (index == NULL) {
        PyErr_Format(PyExc_TypeError,
                     "gid should be integer, not %.200s",
                     _PyType_Name(Py_TYPE(obj)));
        return 0;
    }

    /*
     * Handling gid_t is complicated for two reasons:
     *  * Although gid_t is (always?) unsigned, it still
     *    accepts -1.
     *  * We don't know its size in advance--it may be
     *    bigger than an int, or it may be smaller than
     *    a long.
     *
     * So a bit of defensive programming is in order.
     * Start with interpreting the value passed
     * in as a signed long and see if it works.
     */

    result = PyLong_AsLongAndOverflow(index, &overflow);

    if (!overflow) {
        gid = (gid_t)result;

        if (result == -1) {
            if (PyErr_Occurred())
                goto fail;
            /* It's a legitimate -1, we're done. */
            goto success;
        }

        /* Any other negative number is disallowed. */
        if (result < 0) {
            goto underflow;
        }

        /* Ensure the value wasn't truncated. */
        if (sizeof(gid_t) < sizeof(long) &&
            (long)gid != result)
            goto underflow;
        goto success;
    }

    if (overflow < 0)
        goto underflow;

    /*
     * Okay, the value overflowed a signed long.  If it
     * fits in an *unsigned* long, it may still be okay,
     * as gid_t may be unsigned long on this platform.
     */
    uresult = PyLong_AsUnsignedLong(index);
    if (PyErr_Occurred()) {
        if (PyErr_ExceptionMatches(PyExc_OverflowError))
            goto overflow;
        goto fail;
    }

    gid = (gid_t)uresult;

    /*
     * If gid == (gid_t)-1, the user actually passed in ULONG_MAX,
     * but this value would get interpreted as (gid_t)-1  by chown
     * and its siblings.   That's not what the user meant!  So we
     * throw an overflow exception instead.   (We already
     * handled a real -1 with PyLong_AsLongAndOverflow() above.)
     */
    if (gid == (gid_t)-1)
        goto overflow;

    /* Ensure the value wasn't truncated. */
    if (sizeof(gid_t) < sizeof(long) &&
        (unsigned long)gid != uresult)
        goto overflow;
    /* fallthrough */

success:
    Py_DECREF(index);
    *(gid_t *)p = gid;
    return 1;

underflow:
    PyErr_SetString(PyExc_OverflowError,
                    "gid is less than minimum");
    goto fail;

overflow:
    PyErr_SetString(PyExc_OverflowError,
                    "gid is greater than maximum");
    /* fallthrough */

fail:
    Py_DECREF(index);
    return 0;
}
#endif /* MS_WINDOWS */


#define _PyLong_FromDev PyLong_FromLongLong

#ifdef AT_FDCWD
/*
 * Why the (int) cast?  Solaris 10 defines AT_FDCWD as 0xffd19553 (-3041965);
 * without the int cast, the value gets interpreted as uint (4291925331),
 * which doesn't play nicely with all the initializer lines in this file that
 * look like this:
 *      int dir_fd = DEFAULT_DIR_FD;
 */
#define DEFAULT_DIR_FD (int)AT_FDCWD
#else
#define DEFAULT_DIR_FD (-100)
#endif

static int
_fd_converter(PyObject *o, int *p)
{
    int overflow;
    long long_value;

    PyObject *index = _PyNumber_Index(o);
    if (index == NULL) {
        return 0;
    }

    assert(PyLong_Check(index));
    long_value = PyLong_AsLongAndOverflow(index, &overflow);
    Py_DECREF(index);
    assert(!PyErr_Occurred());
    if (overflow > 0 || long_value > INT_MAX) {
        PyErr_SetString(PyExc_OverflowError,
                        "fd is greater than maximum");
        return 0;
    }
    if (overflow < 0 || long_value < INT_MIN) {
        PyErr_SetString(PyExc_OverflowError,
                        "fd is less than minimum");
        return 0;
    }

    *p = (int)long_value;
    return 1;
}

static int
dir_fd_converter(PyObject *o, void *p)
{
    if (o == Py_None) {
        *(int *)p = DEFAULT_DIR_FD;
        return 1;
    }
    else if (PyIndex_Check(o)) {
        return _fd_converter(o, (int *)p);
    }
    else {
        PyErr_Format(PyExc_TypeError,
                     "argument should be integer or None, not %.200s",
                     _PyType_Name(Py_TYPE(o)));
        return 0;
    }
}

typedef struct {
    PyObject *billion;
    PyObject *DirEntryType;
    PyObject *ScandirIteratorType;
    PyObject *StatResultType;
    PyObject *StatVFSResultType;
    PyObject *st_mode;
} _posixstate;


static inline _posixstate*
get_posix_state(PyObject *module)
{
    void *state = PyModule_GetState(module);
    assert(state != NULL);
    return (_posixstate *)state;
}

/*
 * A PyArg_ParseTuple "converter" function
 * that handles filesystem paths in the manner
 * preferred by the os module.
 *
 * path_converter accepts (Unicode) strings and their
 * subclasses, and bytes and their subclasses.  What
 * it does with the argument depends on the platform:
 *
 *   * On Windows, if we get a (Unicode) string we
 *     extract the wchar_t * and return it; if we get
 *     bytes we decode to wchar_t * and return that.
 *
 *   * On all other platforms, strings are encoded
 *     to bytes using PyUnicode_FSConverter, then we
 *     extract the char * from the bytes object and
 *     return that.
 *
 * path_converter also optionally accepts signed
 * integers (representing open file descriptors) instead
 * of path strings.
 *
 * Input fields:
 *   path.nullable
 *     If nonzero, the path is permitted to be None.
 *   path.allow_fd
 *     If nonzero, the path is permitted to be a file handle
 *     (a signed int) instead of a string.
 *   path.function_name
 *     If non-NULL, path_converter will use that as the name
 *     of the function in error messages.
 *     (If path.function_name is NULL it omits the function name.)
 *   path.argument_name
 *     If non-NULL, path_converter will use that as the name
 *     of the parameter in error messages.
 *     (If path.argument_name is NULL it uses "path".)
 *
 * Output fields:
 *   path.wide
 *     Points to the path if it was expressed as Unicode
 *     and was not encoded.  (Only used on Windows.)
 *   path.narrow
 *     Points to the path if it was expressed as bytes,
 *     or it was Unicode and was encoded to bytes. (On Windows,
 *     is a non-zero integer if the path was expressed as bytes.
 *     The type is deliberately incompatible to prevent misuse.)
 *   path.fd
 *     Contains a file descriptor if path.accept_fd was true
 *     and the caller provided a signed integer instead of any
 *     sort of string.
 *
 *     WARNING: if your "path" parameter is optional, and is
 *     unspecified, path_converter will never get called.
 *     So if you set allow_fd, you *MUST* initialize path.fd = -1
 *     yourself!
 *   path.length
 *     The length of the path in characters, if specified as
 *     a string.
 *   path.object
 *     The original object passed in (if get a PathLike object,
 *     the result of PyOS_FSPath() is treated as the original object).
 *     Own a reference to the object.
 *   path.cleanup
 *     For internal use only.  May point to a temporary object.
 *     (Pay no attention to the man behind the curtain.)
 *
 *   At most one of path.wide or path.narrow will be non-NULL.
 *   If path was None and path.nullable was set,
 *     or if path was an integer and path.allow_fd was set,
 *     both path.wide and path.narrow will be NULL
 *     and path.length will be 0.
 *
 *   path_converter takes care to not write to the path_t
 *   unless it's successful.  However it must reset the
 *   "cleanup" field each time it's called.
 *
 * Use as follows:
 *      path_t path;
 *      memset(&path, 0, sizeof(path));
 *      PyArg_ParseTuple(args, "O&", path_converter, &path);
 *      // ... use values from path ...
 *      path_cleanup(&path);
 *
 * (Note that if PyArg_Parse fails you don't need to call
 * path_cleanup().  However it is safe to do so.)
 */
typedef struct {
    const char *function_name;
    const char *argument_name;
    int nullable;
    int allow_fd;
    const wchar_t *wide;
#ifdef MS_WINDOWS
    BOOL narrow;
#else
    const char *narrow;
#endif
    int fd;
    Py_ssize_t length;
    PyObject *object;
    PyObject *cleanup;
} path_t;

#ifdef MS_WINDOWS
#define PATH_T_INITIALIZE(function_name, argument_name, nullable, allow_fd) \
    {function_name, argument_name, nullable, allow_fd, NULL, FALSE, -1, 0, NULL, NULL}
#else
#define PATH_T_INITIALIZE(function_name, argument_name, nullable, allow_fd) \
    {function_name, argument_name, nullable, allow_fd, NULL, NULL, -1, 0, NULL, NULL}
#endif

static void
path_cleanup(path_t *path)
{
    Py_CLEAR(path->object);
    Py_CLEAR(path->cleanup);
}

static int
path_converter(PyObject *o, void *p)
{
    path_t *path = (path_t *)p;
    PyObject *bytes = NULL;
    Py_ssize_t length = 0;
    int is_index, is_buffer, is_bytes, is_unicode;
    const char *narrow;

#define FORMAT_EXCEPTION(exc, fmt) \
    PyErr_Format(exc, "%s%s" fmt, \
        path->function_name ? path->function_name : "", \
        path->function_name ? ": "                : "", \
        path->argument_name ? path->argument_name : "path")

    /* Py_CLEANUP_SUPPORTED support */
    if (o == NULL) {
        path_cleanup(path);
        return 1;
    }

    /* Ensure it's always safe to call path_cleanup(). */
    path->object = path->cleanup = NULL;
    /* path->object owns a reference to the original object */
    Py_INCREF(o);

    if ((o == Py_None) && path->nullable) {
        path->wide = NULL;
        path->narrow = NULL;
        path->fd = -1;
        goto success_exit;
    }

    /* Only call this here so that we don't treat the return value of
       os.fspath() as an fd or buffer. */
    is_index = path->allow_fd && PyIndex_Check(o);
    is_buffer = PyObject_CheckBuffer(o);
    is_bytes = PyBytes_Check(o);
    is_unicode = PyUnicode_Check(o);

    if (!is_index && !is_buffer && !is_unicode && !is_bytes) {
        /* Inline PyOS_FSPath() for better error messages. */
        PyObject *func, *res;

        func = _PyObject_LookupSpecial(o, &PyId___fspath__);
        if (NULL == func) {
            goto error_format;
        }
        res = _PyObject_CallNoArg(func);
        Py_DECREF(func);
        if (NULL == res) {
            goto error_exit;
        }
        else if (PyUnicode_Check(res)) {
            is_unicode = 1;
        }
        else if (PyBytes_Check(res)) {
            is_bytes = 1;
        }
        else {
            PyErr_Format(PyExc_TypeError,
                 "expected %.200s.__fspath__() to return str or bytes, "
                 "not %.200s", _PyType_Name(Py_TYPE(o)),
                 _PyType_Name(Py_TYPE(res)));
            Py_DECREF(res);
            goto error_exit;
        }

        /* still owns a reference to the original object */
        Py_DECREF(o);
        o = res;
    }

    if (is_unicode) {
        if (!PyUnicode_FSConverter(o, &bytes)) {
            goto error_exit;
        }
    }
    else if (is_bytes) {
        bytes = o;
        Py_INCREF(bytes);
    }
    else if (is_buffer) {
        /* XXX Replace PyObject_CheckBuffer with PyBytes_Check in other code
           after removing support of non-bytes buffer objects. */
        bytes = PyBytes_FromObject(o);
        if (!bytes) {
            goto error_exit;
        }
    }
    else if (is_index) {
        if (!_fd_converter(o, &path->fd)) {
            goto error_exit;
        }
        path->wide = NULL;
        path->narrow = NULL;
        goto success_exit;
    }
    else {
 error_format:
        PyErr_Format(PyExc_TypeError, "%s%s%s should be %s, not %.200s",
            path->function_name ? path->function_name : "",
            path->function_name ? ": "                : "",
            path->argument_name ? path->argument_name : "path",
            path->allow_fd && path->nullable ? "string, bytes, os.PathLike, "
                                               "integer or None" :
            path->allow_fd ? "string, bytes, os.PathLike or integer" :
            path->nullable ? "string, bytes, os.PathLike or None" :
                             "string, bytes or os.PathLike",
            _PyType_Name(Py_TYPE(o)));
        goto error_exit;
    }

    length = PyBytes_GET_SIZE(bytes);
    narrow = PyBytes_AS_STRING(bytes);
    if ((size_t)length != strlen(narrow)) {
        FORMAT_EXCEPTION(PyExc_ValueError, "embedded null character in %s");
        goto error_exit;
    }

    path->wide = NULL;
    path->narrow = narrow;
    if (bytes == o) {
        /* Still a reference owned by path->object, don't have to
           worry about path->narrow is used after free. */
        Py_DECREF(bytes);
    }
    else {
        path->cleanup = bytes;
    }
    path->fd = -1;

 success_exit:
    path->length = length;
    path->object = o;
    return Py_CLEANUP_SUPPORTED;

 error_exit:
    Py_XDECREF(o);
    Py_XDECREF(bytes);
    return 0;
}

static void
argument_unavailable_error(const char *function_name, const char *argument_name)
{
    PyErr_Format(PyExc_NotImplementedError,
        "%s%s%s unavailable on this platform",
        (function_name != NULL) ? function_name : "",
        (function_name != NULL) ? ": ": "",
        argument_name);
}

static int
dir_fd_unavailable(PyObject *o, void *p)
{
    int dir_fd;
    if (!dir_fd_converter(o, &dir_fd))
        return 0;
    if (dir_fd != DEFAULT_DIR_FD) {
        argument_unavailable_error(NULL, "dir_fd");
        return 0;
    }
    *(int *)p = dir_fd;
    return 1;
}

static int
fd_specified(const char *function_name, int fd)
{
    if (fd == -1)
        return 0;

    argument_unavailable_error(function_name, "fd");
    return 1;
}

static int
follow_symlinks_specified(const char *function_name, int follow_symlinks)
{
    if (follow_symlinks)
        return 0;

    argument_unavailable_error(function_name, "follow_symlinks");
    return 1;
}

static int
path_and_dir_fd_invalid(const char *function_name, path_t *path, int dir_fd)
{
    if (!path->wide && (dir_fd != DEFAULT_DIR_FD)
        && !path->narrow
    ) {
        PyErr_Format(PyExc_ValueError,
                     "%s: can't specify dir_fd without matching path",
                     function_name);
        return 1;
    }
    return 0;
}

static int
dir_fd_and_fd_invalid(const char *function_name, int dir_fd, int fd)
{
    if ((dir_fd != DEFAULT_DIR_FD) && (fd != -1)) {
        PyErr_Format(PyExc_ValueError,
                     "%s: can't specify both dir_fd and fd",
                     function_name);
        return 1;
    }
    return 0;
}

static int
fd_and_follow_symlinks_invalid(const char *function_name, int fd,
                               int follow_symlinks)
{
    if ((fd > 0) && (!follow_symlinks)) {
        PyErr_Format(PyExc_ValueError,
                     "%s: cannot use fd and follow_symlinks together",
                     function_name);
        return 1;
    }
    return 0;
}

static int
dir_fd_and_follow_symlinks_invalid(const char *function_name, int dir_fd,
                                   int follow_symlinks)
{
    if ((dir_fd != DEFAULT_DIR_FD) && (!follow_symlinks)) {
        PyErr_Format(PyExc_ValueError,
                     "%s: cannot use dir_fd and follow_symlinks together",
                     function_name);
        return 1;
    }
    return 0;
}

#ifdef MS_WINDOWS
    typedef long long Py_off_t;
#else
    typedef off_t Py_off_t;
#endif

static int
Py_off_t_converter(PyObject *arg, void *addr)
{
#ifdef HAVE_LARGEFILE_SUPPORT
    *((Py_off_t *)addr) = PyLong_AsLongLong(arg);
#else
    *((Py_off_t *)addr) = PyLong_AsLong(arg);
#endif
    if (PyErr_Occurred())
        return 0;
    return 1;
}

static PyObject *
PyLong_FromPy_off_t(Py_off_t offset)
{
#ifdef HAVE_LARGEFILE_SUPPORT
    return PyLong_FromLongLong(offset);
#else
    return PyLong_FromLong(offset);
#endif
}

#ifdef HAVE_SIGSET_T
/* Convert an iterable of integers to a sigset.
   Return 1 on success, return 0 and raise an exception on error. */
int
_Py_Sigset_Converter(PyObject *obj, void *addr)
{
    sigset_t *mask = (sigset_t *)addr;
    PyObject *iterator, *item;
    long signum;
    int overflow;

    // The extra parens suppress the unreachable-code warning with clang on MacOS
    if (sigemptyset(mask) < (0)) {
        /* Probably only if mask == NULL. */
        PyErr_SetFromErrno(PyExc_OSError);
        return 0;
    }

    iterator = PyObject_GetIter(obj);
    if (iterator == NULL) {
        return 0;
    }

    while ((item = PyIter_Next(iterator)) != NULL) {
        signum = PyLong_AsLongAndOverflow(item, &overflow);
        Py_DECREF(item);
        if (signum <= 0 || signum >= NSIG) {
            if (overflow || signum != -1 || !PyErr_Occurred()) {
                PyErr_Format(PyExc_ValueError,
                             "signal number %ld out of range", signum);
            }
            goto error;
        }
        if (sigaddset(mask, (int)signum)) {
            if (errno != EINVAL) {
                /* Probably impossible */
                PyErr_SetFromErrno(PyExc_OSError);
                goto error;
            }
            /* For backwards compatibility, allow idioms such as
             * `range(1, NSIG)` but warn about invalid signal numbers
             */
        }
    }
    if (!PyErr_Occurred()) {
        Py_DECREF(iterator);
        return 1;
    }

error:
    Py_DECREF(iterator);
    return 0;
}
#endif /* HAVE_SIGSET_T */

/* Return a dictionary corresponding to the POSIX environment table */
#if defined(WITH_NEXT_FRAMEWORK) || (defined(__APPLE__) && defined(Py_ENABLE_SHARED))
/* On Darwin/MacOSX a shared library or framework has no access to
** environ directly, we must obtain it with _NSGetEnviron(). See also
** man environ(7).
*/
#include <crt_externs.h>
#elif !defined(_MSC_VER) && (!defined(__WATCOMC__) || defined(__QNX__) || defined(__VXWORKS__))
extern char **environ;
#endif /* !_MSC_VER */

static PyObject *
convertenviron(void)
{
    PyObject *d;
#ifdef MS_WINDOWS
    wchar_t **e;
#else
    char **e;
#endif

    d = PyDict_New();
    if (d == NULL)
        return NULL;
#ifdef MS_WINDOWS
    /* _wenviron must be initialized in this way if the program is started
       through main() instead of wmain(). */
    _wgetenv(L"");
    e = _wenviron;
#elif defined(WITH_NEXT_FRAMEWORK) || (defined(__APPLE__) && defined(Py_ENABLE_SHARED))
    /* environ is not accessible as an extern in a shared object on OSX; use
       _NSGetEnviron to resolve it. The value changes if you add environment
       variables between calls to Py_Initialize, so don't cache the value. */
    e = *_NSGetEnviron();
#else
    e = environ;
#endif
    if (e == NULL)
        return d;
    for (; *e != NULL; e++) {
        PyObject *k;
        PyObject *v;
#ifdef MS_WINDOWS
        const wchar_t *p = wcschr(*e, L'=');
#else
        const char *p = strchr(*e, '=');
#endif
        if (p == NULL)
            continue;
#ifdef MS_WINDOWS
        k = PyUnicode_FromWideChar(*e, (Py_ssize_t)(p-*e));
#else
        k = PyBytes_FromStringAndSize(*e, (int)(p-*e));
#endif
        if (k == NULL) {
            Py_DECREF(d);
            return NULL;
        }
#ifdef MS_WINDOWS
        v = PyUnicode_FromWideChar(p+1, wcslen(p+1));
#else
        v = PyBytes_FromStringAndSize(p+1, strlen(p+1));
#endif
        if (v == NULL) {
            Py_DECREF(k);
            Py_DECREF(d);
            return NULL;
        }
        if (PyDict_GetItemWithError(d, k) == NULL) {
            if (PyErr_Occurred() || PyDict_SetItem(d, k, v) != 0) {
                Py_DECREF(v);
                Py_DECREF(k);
                Py_DECREF(d);
                return NULL;
            }
        }
        Py_DECREF(k);
        Py_DECREF(v);
    }
    return d;
}

/* Set a POSIX-specific error from errno, and return NULL */

static PyObject *
posix_error(void)
{
    return PyErr_SetFromErrno(PyExc_OSError);
}

static PyObject *
posix_path_object_error(PyObject *path)
{
    return PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path);
}

static PyObject *
path_object_error(PyObject *path)
{
    return posix_path_object_error(path);
}

static PyObject *
path_object_error2(PyObject *path, PyObject *path2)
{
    return PyErr_SetFromErrnoWithFilenameObjects(PyExc_OSError, path, path2);
}

static PyObject *
path_error(path_t *path)
{
    return path_object_error(path->object);
}

static PyObject *
posix_path_error(path_t *path)
{
    return posix_path_object_error(path->object);
}

static PyObject *
path_error2(path_t *path, path_t *path2)
{
    return path_object_error2(path->object, path2->object);
}


/* POSIX generic methods */

static int
fildes_converter(PyObject *o, void *p)
{
    int fd;
    int *pointer = (int *)p;
    fd = PyObject_AsFileDescriptor(o);
    if (fd < 0)
        return 0;
    *pointer = fd;
    return 1;
}

static PyObject *
posix_fildes_fd(int fd, int (*func)(int))
{
    int res;
    int async_err = 0;

    do {
        
        
        res = (*func)(fd);
        
        
	  } while (res != 0 && errno == EINTR); // && !(async_err = PyErr_CheckSignals()));
    if (res != 0)
        return (!async_err) ? posix_error() : NULL;
    Py_RETURN_NONE;
}

PyDoc_STRVAR(stat_result__doc__,
"stat_result: Result from stat, fstat, or lstat.\n\n\
This object may be accessed either as a tuple of\n\
  (mode, ino, dev, nlink, uid, gid, size, atime, mtime, ctime)\n\
or via the attributes st_mode, st_ino, st_dev, st_nlink, st_uid, and so on.\n\
\n\
Posix/windows: If your platform supports st_blksize, st_blocks, st_rdev,\n\
or st_flags, they are available as attributes only.\n\
\n\
See os.stat for more information.");

static PyStructSequence_Field stat_result_fields[] = {
    {"st_mode",    "protection bits"},
    {"st_ino",     "inode"},
    {"st_dev",     "device"},
    {"st_nlink",   "number of hard links"},
    {"st_uid",     "user ID of owner"},
    {"st_gid",     "group ID of owner"},
    {"st_size",    "total size, in bytes"},
    /* The NULL is replaced with PyStructSequence_UnnamedField later. */
    {NULL,   "integer time of last access"},
    {NULL,   "integer time of last modification"},
    {NULL,   "integer time of last change"},
    {"st_atime",   "time of last access"},
    {"st_mtime",   "time of last modification"},
    {"st_ctime",   "time of last change"},
    {"st_atime_ns",   "time of last access in nanoseconds"},
    {"st_mtime_ns",   "time of last modification in nanoseconds"},
    {"st_ctime_ns",   "time of last change in nanoseconds"},
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    {"st_blksize", "blocksize for filesystem I/O"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    {"st_blocks",  "number of blocks allocated"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_RDEV
    {"st_rdev",    "device type (if inode device)"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_FLAGS
    {"st_flags",   "user defined flags for file"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_GEN
    {"st_gen",    "generation number"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_BIRTHTIME
    {"st_birthtime",   "time of creation"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_FILE_ATTRIBUTES
    {"st_file_attributes", "Windows file attribute bits"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_FSTYPE
    {"st_fstype",  "Type of filesystem"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_REPARSE_TAG
    {"st_reparse_tag", "Windows reparse tag"},
#endif
    {0}
};

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
#define ST_BLKSIZE_IDX 16
#else
#define ST_BLKSIZE_IDX 15
#endif

#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
#define ST_BLOCKS_IDX (ST_BLKSIZE_IDX+1)
#else
#define ST_BLOCKS_IDX ST_BLKSIZE_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_RDEV
#define ST_RDEV_IDX (ST_BLOCKS_IDX+1)
#else
#define ST_RDEV_IDX ST_BLOCKS_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_FLAGS
#define ST_FLAGS_IDX (ST_RDEV_IDX+1)
#else
#define ST_FLAGS_IDX ST_RDEV_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_GEN
#define ST_GEN_IDX (ST_FLAGS_IDX+1)
#else
#define ST_GEN_IDX ST_FLAGS_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_BIRTHTIME
#define ST_BIRTHTIME_IDX (ST_GEN_IDX+1)
#else
#define ST_BIRTHTIME_IDX ST_GEN_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_FILE_ATTRIBUTES
#define ST_FILE_ATTRIBUTES_IDX (ST_BIRTHTIME_IDX+1)
#else
#define ST_FILE_ATTRIBUTES_IDX ST_BIRTHTIME_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_FSTYPE
#define ST_FSTYPE_IDX (ST_FILE_ATTRIBUTES_IDX+1)
#else
#define ST_FSTYPE_IDX ST_FILE_ATTRIBUTES_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_REPARSE_TAG
#define ST_REPARSE_TAG_IDX (ST_FSTYPE_IDX+1)
#else
#define ST_REPARSE_TAG_IDX ST_FSTYPE_IDX
#endif

static PyStructSequence_Desc stat_result_desc = {
    "stat_result", /* name */
    stat_result__doc__, /* doc */
    stat_result_fields,
    10
};

PyDoc_STRVAR(statvfs_result__doc__,
"statvfs_result: Result from statvfs or fstatvfs.\n\n\
This object may be accessed either as a tuple of\n\
  (bsize, frsize, blocks, bfree, bavail, files, ffree, favail, flag, namemax),\n\
or via the attributes f_bsize, f_frsize, f_blocks, f_bfree, and so on.\n\
\n\
See os.statvfs for more information.");

static PyStructSequence_Field statvfs_result_fields[] = {
    {"f_bsize",  },
    {"f_frsize", },
    {"f_blocks", },
    {"f_bfree",  },
    {"f_bavail", },
    {"f_files",  },
    {"f_ffree",  },
    {"f_favail", },
    {"f_flag",   },
    {"f_namemax",},
    {"f_fsid",   },
    {0}
};

static PyStructSequence_Desc statvfs_result_desc = {
    "statvfs_result", /* name */
    statvfs_result__doc__, /* doc */
    statvfs_result_fields,
    10
};

static newfunc structseq_new;

static PyObject *
statresult_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyStructSequence *result;
    int i;

    result = (PyStructSequence*)structseq_new(type, args, kwds);
    if (!result)
        return NULL;
    /* If we have been initialized from a tuple,
       st_?time might be set to None. Initialize it
       from the int slots.  */
    for (i = 7; i <= 9; i++) {
        if (result->ob_item[i+3] == Py_None) {
            Py_DECREF(Py_None);
            Py_INCREF(result->ob_item[i]);
            result->ob_item[i+3] = result->ob_item[i];
        }
    }
    return (PyObject*)result;
}

static int
_posix_clear(PyObject *module)
{
    _posixstate *state = get_posix_state(module);
    Py_CLEAR(state->billion);
    Py_CLEAR(state->DirEntryType);
    Py_CLEAR(state->ScandirIteratorType);
    Py_CLEAR(state->StatResultType);
    Py_CLEAR(state->StatVFSResultType);
    Py_CLEAR(state->st_mode);
    return 0;
}

static int
_posix_traverse(PyObject *module, visitproc visit, void *arg)
{
    _posixstate *state = get_posix_state(module);
    Py_VISIT(state->billion);
    Py_VISIT(state->DirEntryType);
    Py_VISIT(state->ScandirIteratorType);
    Py_VISIT(state->StatResultType);
    Py_VISIT(state->StatVFSResultType);
    Py_VISIT(state->st_mode);
    return 0;
}

static void
_posix_free(void *module)
{
   _posix_clear((PyObject *)module);
}

static void
fill_time(PyObject *module, PyObject *v, int index, time_t sec, unsigned long nsec)
{
    PyObject *s = _PyLong_FromTime_t(sec);
    PyObject *ns_fractional = PyLong_FromUnsignedLong(nsec);
    PyObject *s_in_ns = NULL;
    PyObject *ns_total = NULL;
    PyObject *float_s = NULL;

    if (!(s && ns_fractional))
        goto exit;

    s_in_ns = PyNumber_Multiply(s, get_posix_state(module)->billion);
    if (!s_in_ns)
        goto exit;

    ns_total = PyNumber_Add(s_in_ns, ns_fractional);
    if (!ns_total)
        goto exit;

    float_s = PyFloat_FromDouble(sec + 1e-9*nsec);
    if (!float_s) {
        goto exit;
    }

    PyStructSequence_SET_ITEM(v, index, s);
    PyStructSequence_SET_ITEM(v, index+3, float_s);
    PyStructSequence_SET_ITEM(v, index+6, ns_total);
    s = NULL;
    float_s = NULL;
    ns_total = NULL;
exit:
    Py_XDECREF(s);
    Py_XDECREF(ns_fractional);
    Py_XDECREF(s_in_ns);
    Py_XDECREF(ns_total);
    Py_XDECREF(float_s);
}

/* pack a system stat C structure into the Python stat tuple
   (used by posix_stat() and posix_fstat()) */
static PyObject*
_pystat_fromstructstat(PyObject *module, STRUCT_STAT *st)
{
    unsigned long ansec, mnsec, cnsec;
    PyObject *StatResultType = get_posix_state(module)->StatResultType;
    PyObject *v = PyStructSequence_New((PyTypeObject *)StatResultType);
    if (v == NULL)
        return NULL;

    PyStructSequence_SET_ITEM(v, 0, PyLong_FromLong((long)st->st_mode));
    Py_BUILD_ASSERT(sizeof(unsigned long long) >= sizeof(st->st_ino));
    PyStructSequence_SET_ITEM(v, 1, PyLong_FromUnsignedLongLong(st->st_ino));
    PyStructSequence_SET_ITEM(v, 2, _PyLong_FromDev(st->st_dev));
    PyStructSequence_SET_ITEM(v, 3, PyLong_FromLong((long)st->st_nlink));
    PyStructSequence_SET_ITEM(v, 4, _PyLong_FromUid(st->st_uid));
    PyStructSequence_SET_ITEM(v, 5, _PyLong_FromGid(st->st_gid));
    Py_BUILD_ASSERT(sizeof(long long) >= sizeof(st->st_size));
    PyStructSequence_SET_ITEM(v, 6, PyLong_FromLongLong(st->st_size));

#if defined(HAVE_STAT_TV_NSEC)
    ansec = st->st_atim.tv_nsec;
    mnsec = st->st_mtim.tv_nsec;
    cnsec = st->st_ctim.tv_nsec;
#elif defined(HAVE_STAT_TV_NSEC2)
    ansec = st->st_atimespec.tv_nsec;
    mnsec = st->st_mtimespec.tv_nsec;
    cnsec = st->st_ctimespec.tv_nsec;
#elif defined(HAVE_STAT_NSEC)
    ansec = st->st_atime_nsec;
    mnsec = st->st_mtime_nsec;
    cnsec = st->st_ctime_nsec;
#else
    ansec = mnsec = cnsec = 0;
#endif
    fill_time(module, v, 7, st->st_atime, ansec);
    fill_time(module, v, 8, st->st_mtime, mnsec);
    fill_time(module, v, 9, st->st_ctime, cnsec);

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    PyStructSequence_SET_ITEM(v, ST_BLKSIZE_IDX,
                              PyLong_FromLong((long)st->st_blksize));
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    PyStructSequence_SET_ITEM(v, ST_BLOCKS_IDX,
                              PyLong_FromLong((long)st->st_blocks));
#endif
#ifdef HAVE_STRUCT_STAT_ST_RDEV
    PyStructSequence_SET_ITEM(v, ST_RDEV_IDX,
                              PyLong_FromLong((long)st->st_rdev));
#endif
#ifdef HAVE_STRUCT_STAT_ST_GEN
    PyStructSequence_SET_ITEM(v, ST_GEN_IDX,
                              PyLong_FromLong((long)st->st_gen));
#endif
#ifdef HAVE_STRUCT_STAT_ST_BIRTHTIME
    {
      PyObject *val;
      unsigned long bsec,bnsec;
      bsec = (long)st->st_birthtime;
#ifdef HAVE_STAT_TV_NSEC2
      bnsec = st->st_birthtimespec.tv_nsec;
#else
      bnsec = 0;
#endif
      val = PyFloat_FromDouble(bsec + 1e-9*bnsec);
      PyStructSequence_SET_ITEM(v, ST_BIRTHTIME_IDX,
                                val);
    }
#endif
#ifdef HAVE_STRUCT_STAT_ST_FLAGS
    PyStructSequence_SET_ITEM(v, ST_FLAGS_IDX,
                              PyLong_FromLong((long)st->st_flags));
#endif
#ifdef HAVE_STRUCT_STAT_ST_FILE_ATTRIBUTES
    PyStructSequence_SET_ITEM(v, ST_FILE_ATTRIBUTES_IDX,
                              PyLong_FromUnsignedLong(st->st_file_attributes));
#endif
#ifdef HAVE_STRUCT_STAT_ST_FSTYPE
   PyStructSequence_SET_ITEM(v, ST_FSTYPE_IDX,
                              PyUnicode_FromString(st->st_fstype));
#endif
#ifdef HAVE_STRUCT_STAT_ST_REPARSE_TAG
    PyStructSequence_SET_ITEM(v, ST_REPARSE_TAG_IDX,
                              PyLong_FromUnsignedLong(st->st_reparse_tag));
#endif

    if (PyErr_Occurred()) {
        Py_DECREF(v);
        return NULL;
    }

    return v;
}

/* POSIX methods */


static PyObject *
posix_do_stat(PyObject *module, const char *function_name, path_t *path,
              int dir_fd, int follow_symlinks)
{
    STRUCT_STAT st;
    int result;

#if !defined(MS_WINDOWS) && !defined(HAVE_FSTATAT) && !defined(HAVE_LSTAT)
    if (follow_symlinks_specified(function_name, follow_symlinks))
        return NULL;
#endif

    if (path_and_dir_fd_invalid("stat", path, dir_fd) ||
        dir_fd_and_fd_invalid("stat", dir_fd, path->fd) ||
        fd_and_follow_symlinks_invalid("stat", path->fd, follow_symlinks))
        return NULL;

    
    if (path->fd != -1)
        result = FSTAT(path->fd, &st);
    else
#if defined(HAVE_LSTAT)
    if ((!follow_symlinks) && (dir_fd == DEFAULT_DIR_FD))
        result = LSTAT(path->narrow, &st);
    else
#endif /* HAVE_LSTAT */
#ifdef HAVE_FSTATAT
    if ((dir_fd != DEFAULT_DIR_FD) || !follow_symlinks)
        result = fstatat(dir_fd, path->narrow, &st,
                         follow_symlinks ? 0 : AT_SYMLINK_NOFOLLOW);
    else
#endif /* HAVE_FSTATAT */
        result = STAT(path->narrow, &st);
    if (result != 0) {
        return path_error(path);
    }

    return _pystat_fromstructstat(module, &st);
}

/*[python input]

for s in """

FACCESSAT
FCHMODAT
FCHOWNAT
FSTATAT
LINKAT
MKDIRAT
MKFIFOAT
MKNODAT
OPENAT
READLINKAT
SYMLINKAT
UNLINKAT

""".strip().split():
    s = s.strip()
    print("""
#ifdef HAVE_{s}
    #define {s}_DIR_FD_CONVERTER dir_fd_converter
#else
    #define {s}_DIR_FD_CONVERTER dir_fd_unavailable
#endif
""".rstrip().format(s=s))

for s in """

FCHDIR
FCHMOD
FCHOWN
FDOPENDIR
FEXECVE
FPATHCONF
FSTATVFS
FTRUNCATE

""".strip().split():
    s = s.strip()
    print("""
#ifdef HAVE_{s}
    #define PATH_HAVE_{s} 1
#else
    #define PATH_HAVE_{s} 0
#endif

""".rstrip().format(s=s))
[python start generated code]*/

#ifdef HAVE_FACCESSAT
    #define FACCESSAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define FACCESSAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_FCHMODAT
    #define FCHMODAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define FCHMODAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_FCHOWNAT
    #define FCHOWNAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define FCHOWNAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_FSTATAT
    #define FSTATAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define FSTATAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_LINKAT
    #define LINKAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define LINKAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_MKDIRAT
    #define MKDIRAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define MKDIRAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_MKFIFOAT
    #define MKFIFOAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define MKFIFOAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_MKNODAT
    #define MKNODAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define MKNODAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_OPENAT
    #define OPENAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define OPENAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_READLINKAT
    #define READLINKAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define READLINKAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_SYMLINKAT
    #define SYMLINKAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define SYMLINKAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_UNLINKAT
    #define UNLINKAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define UNLINKAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_FCHDIR
    #define PATH_HAVE_FCHDIR 1
#else
    #define PATH_HAVE_FCHDIR 0
#endif

#ifdef HAVE_FCHMOD
    #define PATH_HAVE_FCHMOD 1
#else
    #define PATH_HAVE_FCHMOD 0
#endif

#ifdef HAVE_FCHOWN
    #define PATH_HAVE_FCHOWN 1
#else
    #define PATH_HAVE_FCHOWN 0
#endif

#ifdef HAVE_FDOPENDIR
    #define PATH_HAVE_FDOPENDIR 1
#else
    #define PATH_HAVE_FDOPENDIR 0
#endif

#ifdef HAVE_FEXECVE
    #define PATH_HAVE_FEXECVE 1
#else
    #define PATH_HAVE_FEXECVE 0
#endif

#ifdef HAVE_FPATHCONF
    #define PATH_HAVE_FPATHCONF 1
#else
    #define PATH_HAVE_FPATHCONF 0
#endif

#ifdef HAVE_FSTATVFS
    #define PATH_HAVE_FSTATVFS 1
#else
    #define PATH_HAVE_FSTATVFS 0
#endif

#ifdef HAVE_FTRUNCATE
    #define PATH_HAVE_FTRUNCATE 1
#else
    #define PATH_HAVE_FTRUNCATE 0
#endif
/*[python end generated code: output=4bd4f6f7d41267f1 input=80b4c890b6774ea5]*/

/*[python input]

class path_t_converter(CConverter):

    type = "path_t"
    impl_by_reference = True
    parse_by_reference = True

    converter = 'path_converter'

    def converter_init(self, *, allow_fd=False, nullable=False):
        # right now path_t doesn't support default values.
        # to support a default value, you'll need to override initialize().
        if self.default not in (unspecified, None):
            fail("Can't specify a default to the path_t converter!")

        if self.c_default not in (None, 'Py_None'):
            raise RuntimeError("Can't specify a c_default to the path_t converter!")

        self.nullable = nullable
        self.allow_fd = allow_fd

    def pre_render(self):
        def strify(value):
            if isinstance(value, str):
                return value
            return str(int(bool(value)))

        # add self.py_name here when merging with posixmodule conversion
        self.c_default = 'PATH_T_INITIALIZE("{}", "{}", {}, {})'.format(
            self.function.name,
            self.name,
            strify(self.nullable),
            strify(self.allow_fd),
            )

    def cleanup(self):
        return "path_cleanup(&" + self.name + ");\n"


class dir_fd_converter(CConverter):
    type = 'int'

    def converter_init(self, requires=None):
        if self.default in (unspecified, None):
            self.c_default = 'DEFAULT_DIR_FD'
        if isinstance(requires, str):
            self.converter = requires.upper() + '_DIR_FD_CONVERTER'
        else:
            self.converter = 'dir_fd_converter'

class fildes_converter(CConverter):
    type = 'int'
    converter = 'fildes_converter'

class uid_t_converter(CConverter):
    type = "uid_t"
    converter = '_Py_Uid_Converter'

class gid_t_converter(CConverter):
    type = "gid_t"
    converter = '_Py_Gid_Converter'

class dev_t_converter(CConverter):
    type = 'dev_t'
    converter = '_Py_Dev_Converter'

class dev_t_return_converter(unsigned_long_return_converter):
    type = 'dev_t'
    conversion_fn = '_PyLong_FromDev'
    unsigned_cast = '(dev_t)'

class FSConverter_converter(CConverter):
    type = 'PyObject *'
    converter = 'PyUnicode_FSConverter'
    def converter_init(self):
        if self.default is not unspecified:
            fail("FSConverter_converter does not support default values")
        self.c_default = 'NULL'

    def cleanup(self):
        return "Py_XDECREF(" + self.name + ");\n"

class pid_t_converter(CConverter):
    type = 'pid_t'
    format_unit = '" _Py_PARSE_PID "'

class idtype_t_converter(int_converter):
    type = 'idtype_t'

class id_t_converter(CConverter):
    type = 'id_t'
    format_unit = '" _Py_PARSE_PID "'

class intptr_t_converter(CConverter):
    type = 'intptr_t'
    format_unit = '" _Py_PARSE_INTPTR "'

class Py_off_t_converter(CConverter):
    type = 'Py_off_t'
    converter = 'Py_off_t_converter'

class Py_off_t_return_converter(long_return_converter):
    type = 'Py_off_t'
    conversion_fn = 'PyLong_FromPy_off_t'

class path_confname_converter(CConverter):
    type="int"
    converter="conv_path_confname"

class confstr_confname_converter(path_confname_converter):
    converter='conv_confstr_confname'

class sysconf_confname_converter(path_confname_converter):
    converter="conv_sysconf_confname"

[python start generated code]*/
/*[python end generated code: output=da39a3ee5e6b4b0d input=f1c8ae8d744f6c8b]*/

/*[clinic input]

os.stat

    path : path_t(allow_fd=True)
        Path to be examined; can be string, bytes, a path-like object or
        open-file-descriptor int.

    *

    dir_fd : dir_fd(requires='fstatat') = None
        If not None, it should be a file descriptor open to a directory,
        and path should be a relative string; path will then be relative to
        that directory.

    follow_symlinks: bool = True
        If False, and the last element of the path is a symbolic link,
        stat will examine the symbolic link itself instead of the file
        the link points to.

Perform a stat system call on the given path.

dir_fd and follow_symlinks may not be implemented
  on your platform.  If they are unavailable, using them will raise a
  NotImplementedError.

It's an error to use dir_fd or follow_symlinks when specifying path as
  an open file descriptor.

[clinic start generated code]*/

static PyObject *
os_stat_impl(PyObject *module, path_t *path, int dir_fd, int follow_symlinks)
/*[clinic end generated code: output=7d4976e6f18a59c5 input=01d362ebcc06996b]*/
{
    return posix_do_stat(module, "stat", path, dir_fd, follow_symlinks);
}


/*[clinic input]
os.lstat

    path : path_t

    *

    dir_fd : dir_fd(requires='fstatat') = None

Perform a stat system call on the given path, without following symbolic links.

Like stat(), but do not follow symbolic links.
Equivalent to stat(path, follow_symlinks=False).
[clinic start generated code]*/

static PyObject *
os_lstat_impl(PyObject *module, path_t *path, int dir_fd)
/*[clinic end generated code: output=ef82a5d35ce8ab37 input=0b7474765927b925]*/
{
    int follow_symlinks = 0;
    return posix_do_stat(module, "lstat", path, dir_fd, follow_symlinks);
}


/*[clinic input]
os.access -> bool

    path: path_t
        Path to be tested; can be string, bytes, or a path-like object.

    mode: int
        Operating-system mode bitfield.  Can be F_OK to test existence,
        or the inclusive-OR of R_OK, W_OK, and X_OK.

    *

    dir_fd : dir_fd(requires='faccessat') = None
        If not None, it should be a file descriptor open to a directory,
        and path should be relative; path will then be relative to that
        directory.

    effective_ids: bool = False
        If True, access will use the effective uid/gid instead of
        the real uid/gid.

    follow_symlinks: bool = True
        If False, and the last element of the path is a symbolic link,
        access will examine the symbolic link itself instead of the file
        the link points to.

Use the real uid/gid to test for access to a path.

{parameters}
dir_fd, effective_ids, and follow_symlinks may not be implemented
  on your platform.  If they are unavailable, using them will raise a
  NotImplementedError.

Note that most operations will use the effective uid/gid, therefore this
  routine can be used in a suid/sgid environment to test if the invoking user
  has the specified access to the path.

[clinic start generated code]*/

static int
os_access_impl(PyObject *module, path_t *path, int mode, int dir_fd,
               int effective_ids, int follow_symlinks)
/*[clinic end generated code: output=cf84158bc90b1a77 input=3ffe4e650ee3bf20]*/
{
    int return_value;
    int result;

#ifndef HAVE_FACCESSAT
    if (follow_symlinks_specified("access", follow_symlinks))
        return -1;

    if (effective_ids) {
        argument_unavailable_error("access", "effective_ids");
        return -1;
    }
#endif
    
#ifdef HAVE_FACCESSAT
    if ((dir_fd != DEFAULT_DIR_FD) ||
        effective_ids ||
        !follow_symlinks) {
        int flags = 0;
        if (!follow_symlinks)
            flags |= AT_SYMLINK_NOFOLLOW;
        if (effective_ids)
            flags |= AT_EACCESS;
        result = faccessat(dir_fd, path->narrow, mode, flags);
    }
    else
#endif
        result = access(path->narrow, mode);
    
    return_value = !result;
    return return_value;
}

#ifndef F_OK
#define F_OK 0
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 1
#endif



/*[clinic input]
os.chdir

    path: path_t(allow_fd='PATH_HAVE_FCHDIR')

Change the current working directory to the specified path.

path may always be specified as a string.
On some platforms, path may also be specified as an open file descriptor.
  If this functionality is unavailable, using it raises an exception.
[clinic start generated code]*/

static PyObject *
os_chdir_impl(PyObject *module, path_t *path)
/*[clinic end generated code: output=3be6400eee26eaae input=1a4a15b4d12cb15d]*/
{
    int result;

    if (PySys_Audit("os.chdir", "(O)", path->object) < 0) {
        return NULL;
    }

#ifdef HAVE_FCHDIR
    if (path->fd != -1)
        result = fchdir(path->fd);
    else
#endif
        result = chdir(path->narrow);

    if (result) {
        return path_error(path);
    }

    Py_RETURN_NONE;
}


#ifdef HAVE_FCHDIR
/*[clinic input]
os.fchdir

    fd: fildes

Change to the directory of the given file descriptor.

fd must be opened on a directory, not a file.
Equivalent to os.chdir(fd).

[clinic start generated code]*/

static PyObject *
os_fchdir_impl(PyObject *module, int fd)
/*[clinic end generated code: output=42e064ec4dc00ab0 input=18e816479a2fa985]*/
{
    if (PySys_Audit("os.chdir", "(i)", fd) < 0) {
        return NULL;
    }
    return posix_fildes_fd(fd, fchdir);
}
#endif /* HAVE_FCHDIR */



static PyObject *
posix_getcwd(int use_bytes)
{
    const size_t chunk = 1024;

    char *buf = NULL;
    char *cwd = NULL;
    size_t buflen = 0;

    
    do {
        char *newbuf;
        if (buflen <= PY_SSIZE_T_MAX - chunk) {
            buflen += chunk;
            newbuf = PyMem_RawRealloc(buf, buflen);
        }
        else {
            newbuf = NULL;
        }
        if (newbuf == NULL) {
            PyMem_RawFree(buf);
            buf = NULL;
            break;
        }
        buf = newbuf;

        cwd = getcwd(buf, buflen);
    } while (cwd == NULL && errno == ERANGE);
    

    if (buf == NULL) {
        return PyErr_NoMemory();
    }
    if (cwd == NULL) {
        PyMem_RawFree(buf);
        return posix_error();
    }

    PyObject *obj;
    if (use_bytes) {
        obj = PyBytes_FromStringAndSize(buf, strlen(buf));
    }
    else {
        obj = PyUnicode_DecodeFSDefault(buf);
    }
    PyMem_RawFree(buf);

    return obj;
}


/*[clinic input]
os.getcwd

Return a unicode string representing the current working directory.
[clinic start generated code]*/

static PyObject *
os_getcwd_impl(PyObject *module)
/*[clinic end generated code: output=21badfae2ea99ddc input=f069211bb70e3d39]*/
{
    return posix_getcwd(0);
}


/*[clinic input]
os.getcwdb

Return a bytes string representing the current working directory.
[clinic start generated code]*/

static PyObject *
os_getcwdb_impl(PyObject *module)
/*[clinic end generated code: output=3dd47909480e4824 input=f6f6a378dad3d9cb]*/
{
    return posix_getcwd(1);
}


#if ((!defined(HAVE_LINK)) && defined(MS_WINDOWS))
#define HAVE_LINK 1
#endif

#ifdef HAVE_LINK
/*[clinic input]

os.link

    src : path_t
    dst : path_t
    *
    src_dir_fd : dir_fd = None
    dst_dir_fd : dir_fd = None
    follow_symlinks: bool = True

Create a hard link to a file.

If either src_dir_fd or dst_dir_fd is not None, it should be a file
  descriptor open to a directory, and the respective path string (src or dst)
  should be relative; the path will then be relative to that directory.
If follow_symlinks is False, and the last element of src is a symbolic
  link, link will create a link to the symbolic link itself instead of the
  file the link points to.
src_dir_fd, dst_dir_fd, and follow_symlinks may not be implemented on your
  platform.  If they are unavailable, using them will raise a
  NotImplementedError.
[clinic start generated code]*/

static PyObject *
os_link_impl(PyObject *module, path_t *src, path_t *dst, int src_dir_fd,
             int dst_dir_fd, int follow_symlinks)
/*[clinic end generated code: output=7f00f6007fd5269a input=b0095ebbcbaa7e04]*/
{
#ifdef MS_WINDOWS
    BOOL result = FALSE;
#else
    int result;
#endif

#ifndef HAVE_LINKAT
    if ((src_dir_fd != DEFAULT_DIR_FD) || (dst_dir_fd != DEFAULT_DIR_FD)) {
        argument_unavailable_error("link", "src_dir_fd and dst_dir_fd");
        return NULL;
    }
#endif

#ifndef MS_WINDOWS
    if ((src->narrow && dst->wide) || (src->wide && dst->narrow)) {
        PyErr_SetString(PyExc_NotImplementedError,
                        "link: src and dst must be the same type");
        return NULL;
    }
#endif

    if (PySys_Audit("os.link", "OOii", src->object, dst->object,
                    src_dir_fd == DEFAULT_DIR_FD ? -1 : src_dir_fd,
                    dst_dir_fd == DEFAULT_DIR_FD ? -1 : dst_dir_fd) < 0) {
        return NULL;
    }

#ifdef MS_WINDOWS
    
    result = CreateHardLinkW(dst->wide, src->wide, NULL);
    

    if (!result)
        return path_error2(src, dst);
#else
    
#ifdef HAVE_LINKAT
    if ((src_dir_fd != DEFAULT_DIR_FD) ||
        (dst_dir_fd != DEFAULT_DIR_FD) ||
        (!follow_symlinks))
        result = linkat(src_dir_fd, src->narrow,
            dst_dir_fd, dst->narrow,
            follow_symlinks ? AT_SYMLINK_FOLLOW : 0);
    else
#endif /* HAVE_LINKAT */
        result = link(src->narrow, dst->narrow);
    

    if (result)
        return path_error2(src, dst);
#endif /* MS_WINDOWS */

    Py_RETURN_NONE;
}
#endif


#if defined(MS_WINDOWS) && !defined(HAVE_OPENDIR)
static PyObject *
_listdir_windows_no_opendir(path_t *path, PyObject *list)
{
    PyObject *v;
    HANDLE hFindFile = INVALID_HANDLE_VALUE;
    BOOL result;
    wchar_t namebuf[MAX_PATH+4]; /* Overallocate for "\*.*" */
    /* only claim to have space for MAX_PATH */
    Py_ssize_t len = Py_ARRAY_LENGTH(namebuf)-4;
    wchar_t *wnamebuf = NULL;

    WIN32_FIND_DATAW wFileData;
    const wchar_t *po_wchars;

    if (!path->wide) { /* Default arg: "." */
        po_wchars = L".";
        len = 1;
    } else {
        po_wchars = path->wide;
        len = wcslen(path->wide);
    }
    /* The +5 is so we can append "\\*.*\0" */
    wnamebuf = PyMem_New(wchar_t, len + 5);
    if (!wnamebuf) {
        PyErr_NoMemory();
        goto exit;
    }
    wcscpy(wnamebuf, po_wchars);
    if (len > 0) {
        wchar_t wch = wnamebuf[len-1];
        if (wch != SEP && wch != ALTSEP && wch != L':')
            wnamebuf[len++] = SEP;
        wcscpy(wnamebuf + len, L"*.*");
    }
    if ((list = PyList_New(0)) == NULL) {
        goto exit;
    }
    
    hFindFile = FindFirstFileW(wnamebuf, &wFileData);
    
    if (hFindFile == INVALID_HANDLE_VALUE) {
        int error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND)
            goto exit;
        Py_DECREF(list);
        list = path_error(path);
        goto exit;
    }
    do {
        /* Skip over . and .. */
        if (wcscmp(wFileData.cFileName, L".") != 0 &&
            wcscmp(wFileData.cFileName, L"..") != 0) {
            v = PyUnicode_FromWideChar(wFileData.cFileName,
                                       wcslen(wFileData.cFileName));
            if (path->narrow && v) {
                Py_SETREF(v, PyUnicode_EncodeFSDefault(v));
            }
            if (v == NULL) {
                Py_DECREF(list);
                list = NULL;
                break;
            }
            if (PyList_Append(list, v) != 0) {
                Py_DECREF(v);
                Py_DECREF(list);
                list = NULL;
                break;
            }
            Py_DECREF(v);
        }
        
        result = FindNextFileW(hFindFile, &wFileData);
        
        /* FindNextFile sets error to ERROR_NO_MORE_FILES if
           it got to the end of the directory. */
        if (!result && GetLastError() != ERROR_NO_MORE_FILES) {
            Py_DECREF(list);
            list = path_error(path);
            goto exit;
        }
    } while (result == TRUE);

exit:
    if (hFindFile != INVALID_HANDLE_VALUE) {
        if (FindClose(hFindFile) == FALSE) {
            if (list != NULL) {
                Py_DECREF(list);
                list = path_error(path);
            }
        }
    }
    PyMem_Free(wnamebuf);

    return list;
}  /* end of _listdir_windows_no_opendir */

#else  /* thus POSIX, ie: not (MS_WINDOWS and not HAVE_OPENDIR) */

static PyObject *
_posix_listdir(path_t *path, PyObject *list)
{
    PyObject *v;
    DIR *dirp = NULL;
    struct dirent *ep;
    int return_str; /* if false, return bytes */
#ifdef HAVE_FDOPENDIR
    int fd = -1;
#endif

    errno = 0;
#ifdef HAVE_FDOPENDIR
    if (path->fd != -1) {
        /* closedir() closes the FD, so we duplicate it */
        fd = _Py_dup(path->fd);
        if (fd == -1)
            return NULL;

        return_str = 1;

        
        dirp = fdopendir(fd);
        
    }
    else
#endif
    {
        const char *name;
        if (path->narrow) {
            name = path->narrow;
            /* only return bytes if they specified a bytes-like object */
            return_str = !PyObject_CheckBuffer(path->object);
        }
        else {
            name = ".";
            return_str = 1;
        }

        
        dirp = opendir(name);
        
    }

    if (dirp == NULL) {
        list = path_error(path);
#ifdef HAVE_FDOPENDIR
        if (fd != -1) {
            
            close(fd);
            
        }
#endif
        goto exit;
    }
    if ((list = PyList_New(0)) == NULL) {
        goto exit;
    }
    for (;;) {
        errno = 0;
        
        ep = readdir(dirp);
        
        if (ep == NULL) {
            if (errno == 0) {
                break;
            } else {
                Py_DECREF(list);
                list = path_error(path);
                goto exit;
            }
        }
        if (ep->d_name[0] == '.' &&
            (NAMLEN(ep) == 1 ||
             (ep->d_name[1] == '.' && NAMLEN(ep) == 2)))
            continue;
        if (return_str)
            v = PyUnicode_DecodeFSDefaultAndSize(ep->d_name, NAMLEN(ep));
        else
            v = PyBytes_FromStringAndSize(ep->d_name, NAMLEN(ep));
        if (v == NULL) {
            Py_CLEAR(list);
            break;
        }
        if (PyList_Append(list, v) != 0) {
            Py_DECREF(v);
            Py_CLEAR(list);
            break;
        }
        Py_DECREF(v);
    }

exit:
    if (dirp != NULL) {
        
#ifdef HAVE_FDOPENDIR
        if (fd > -1)
            rewinddir(dirp);
#endif
        closedir(dirp);
        
    }

    return list;
}  /* end of _posix_listdir */
#endif  /* which OS */


/*[clinic input]
os.listdir

    path : path_t(nullable=True, allow_fd='PATH_HAVE_FDOPENDIR') = None

Return a list containing the names of the files in the directory.

path can be specified as either str, bytes, or a path-like object.  If path is bytes,
  the filenames returned will also be bytes; in all other circumstances
  the filenames returned will be str.
If path is None, uses the path='.'.
On some platforms, path may also be specified as an open file descriptor;\
  the file descriptor must refer to a directory.
  If this functionality is unavailable, using it raises NotImplementedError.

The list is in arbitrary order.  It does not include the special
entries '.' and '..' even if they are present in the directory.


[clinic start generated code]*/

static PyObject *
os_listdir_impl(PyObject *module, path_t *path)
/*[clinic end generated code: output=293045673fcd1a75 input=e3f58030f538295d]*/
{
    if (PySys_Audit("os.listdir", "O",
                    path->object ? path->object : Py_None) < 0) {
        return NULL;
    }
#if defined(MS_WINDOWS) && !defined(HAVE_OPENDIR)
    return _listdir_windows_no_opendir(path, NULL);
#else
    return _posix_listdir(path, NULL);
#endif
}

/*[clinic input]
os.mkdir

    path : path_t

    mode: int = 0o777

    *

    dir_fd : dir_fd(requires='mkdirat') = None

# "mkdir(path, mode=0o777, *, dir_fd=None)\n\n\

Create a directory.

If dir_fd is not None, it should be a file descriptor open to a directory,
  and path should be relative; path will then be relative to that directory.
dir_fd may not be implemented on your platform.
  If it is unavailable, using it will raise a NotImplementedError.

The mode argument is ignored on Windows.
[clinic start generated code]*/

static PyObject *
os_mkdir_impl(PyObject *module, path_t *path, int mode, int dir_fd)
/*[clinic end generated code: output=a70446903abe821f input=e965f68377e9b1ce]*/
{
    int result;

    if (PySys_Audit("os.mkdir", "Oii", path->object, mode,
                    dir_fd == DEFAULT_DIR_FD ? -1 : dir_fd) < 0) {
        return NULL;
    }
    
#if HAVE_MKDIRAT
    if (dir_fd != DEFAULT_DIR_FD)
        result = mkdirat(dir_fd, path->narrow, mode);
    else
#endif
#if defined(__WATCOMC__) && !defined(__QNX__)
        result = mkdir(path->narrow);
#else
        result = mkdir(path->narrow, mode);
#endif
    
    if (result < 0)
        return path_error(path);
    Py_RETURN_NONE;
}


/* sys/resource.h is needed for at least: wait3(), wait4(), broken nice. */
#if defined(HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif

static PyObject *
internal_rename(path_t *src, path_t *dst, int src_dir_fd, int dst_dir_fd, int is_replace)
{
    const char *function_name = is_replace ? "replace" : "rename";
    int dir_fd_specified;
    int result;

    dir_fd_specified = (src_dir_fd != DEFAULT_DIR_FD) ||
                       (dst_dir_fd != DEFAULT_DIR_FD);
#ifndef HAVE_RENAMEAT
    if (dir_fd_specified) {
        argument_unavailable_error(function_name, "src_dir_fd and dst_dir_fd");
        return NULL;
    }
#endif

    if (PySys_Audit("os.rename", "OOii", src->object, dst->object,
                    src_dir_fd == DEFAULT_DIR_FD ? -1 : src_dir_fd,
                    dst_dir_fd == DEFAULT_DIR_FD ? -1 : dst_dir_fd) < 0) {
        return NULL;
    }
    if ((src->narrow && dst->wide) || (src->wide && dst->narrow)) {
        PyErr_Format(PyExc_ValueError,
                     "%s: src and dst must be the same type", function_name);
        return NULL;
    }
    
#ifdef HAVE_RENAMEAT
    if (dir_fd_specified)
        result = renameat(src_dir_fd, src->narrow, dst_dir_fd, dst->narrow);
    else
#endif
    result = rename(src->narrow, dst->narrow);
    

    if (result)
        return path_error2(src, dst);
    Py_RETURN_NONE;
}


/*[clinic input]
os.rename

    src : path_t
    dst : path_t
    *
    src_dir_fd : dir_fd = None
    dst_dir_fd : dir_fd = None

Rename a file or directory.

If either src_dir_fd or dst_dir_fd is not None, it should be a file
  descriptor open to a directory, and the respective path string (src or dst)
  should be relative; the path will then be relative to that directory.
src_dir_fd and dst_dir_fd, may not be implemented on your platform.
  If they are unavailable, using them will raise a NotImplementedError.
[clinic start generated code]*/

static PyObject *
os_rename_impl(PyObject *module, path_t *src, path_t *dst, int src_dir_fd,
               int dst_dir_fd)
/*[clinic end generated code: output=59e803072cf41230 input=faa61c847912c850]*/
{
    return internal_rename(src, dst, src_dir_fd, dst_dir_fd, 0);
}


/*[clinic input]
os.replace = os.rename

Rename a file or directory, overwriting the destination.

If either src_dir_fd or dst_dir_fd is not None, it should be a file
  descriptor open to a directory, and the respective path string (src or dst)
  should be relative; the path will then be relative to that directory.
src_dir_fd and dst_dir_fd, may not be implemented on your platform.
  If they are unavailable, using them will raise a NotImplementedError.
[clinic start generated code]*/

static PyObject *
os_replace_impl(PyObject *module, path_t *src, path_t *dst, int src_dir_fd,
                int dst_dir_fd)
/*[clinic end generated code: output=1968c02e7857422b input=c003f0def43378ef]*/
{
    return internal_rename(src, dst, src_dir_fd, dst_dir_fd, 1);
}


/*[clinic input]
os.rmdir

    path: path_t
    *
    dir_fd: dir_fd(requires='unlinkat') = None

Remove a directory.

If dir_fd is not None, it should be a file descriptor open to a directory,
  and path should be relative; path will then be relative to that directory.
dir_fd may not be implemented on your platform.
  If it is unavailable, using it will raise a NotImplementedError.
[clinic start generated code]*/

static PyObject *
os_rmdir_impl(PyObject *module, path_t *path, int dir_fd)
/*[clinic end generated code: output=080eb54f506e8301 input=38c8b375ca34a7e2]*/
{
    int result;

    if (PySys_Audit("os.rmdir", "Oi", path->object,
                    dir_fd == DEFAULT_DIR_FD ? -1 : dir_fd) < 0) {
        return NULL;
    }
#ifdef HAVE_UNLINKAT
    if (dir_fd != DEFAULT_DIR_FD)
        result = unlinkat(dir_fd, path->narrow, AT_REMOVEDIR);
    else
#endif
      result = rmdir(path->narrow);

    if (result)
        return path_error(path);

    Py_RETURN_NONE;
}


#ifdef HAVE_SYSTEM
#ifdef MS_WINDOWS
/*[clinic input]
os.system -> long

    command: Py_UNICODE

Execute the command in a subshell.
[clinic start generated code]*/

static long
os_system_impl(PyObject *module, const Py_UNICODE *command)
/*[clinic end generated code: output=5b7c3599c068ca42 input=303f5ce97df606b0]*/
{
    long result;

    if (PySys_Audit("os.system", "(u)", command) < 0) {
        return -1;
    }

    
    
    result = _wsystem(command);
    
    
    return result;
}
#else /* MS_WINDOWS */
/*[clinic input]
os.system -> long

    command: FSConverter

Execute the command in a subshell.
[clinic start generated code]*/

static long
os_system_impl(PyObject *module, PyObject *command)
/*[clinic end generated code: output=290fc437dd4f33a0 input=86a58554ba6094af]*/
{
    long result;
    const char *bytes = PyBytes_AsString(command);

    if (PySys_Audit("os.system", "(O)", command) < 0) {
        return -1;
    }

    
    result = system(bytes);
    
    return result;
}
#endif
#endif /* HAVE_SYSTEM */


/*[clinic input]
os.unlink

    path: path_t
    *
    dir_fd: dir_fd(requires='unlinkat')=None

Remove a file (same as remove()).

If dir_fd is not None, it should be a file descriptor open to a directory,
  and path should be relative; path will then be relative to that directory.
dir_fd may not be implemented on your platform.
  If it is unavailable, using it will raise a NotImplementedError.

[clinic start generated code]*/

static PyObject *
os_unlink_impl(PyObject *module, path_t *path, int dir_fd)
/*[clinic end generated code: output=621797807b9963b1 input=d7bcde2b1b2a2552]*/
{
    int result;

    if (PySys_Audit("os.remove", "Oi", path->object,
                    dir_fd == DEFAULT_DIR_FD ? -1 : dir_fd) < 0) {
        return NULL;
    }

    
    
#ifdef HAVE_UNLINKAT
    if (dir_fd != DEFAULT_DIR_FD)
        result = unlinkat(dir_fd, path->narrow, 0);
    else
#endif /* HAVE_UNLINKAT */
        result = unlink(path->narrow);

    if (result)
        return path_error(path);

    Py_RETURN_NONE;
}


/*[clinic input]
os.remove = os.unlink

Remove a file (same as unlink()).

If dir_fd is not None, it should be a file descriptor open to a directory,
  and path should be relative; path will then be relative to that directory.
dir_fd may not be implemented on your platform.
  If it is unavailable, using it will raise a NotImplementedError.
[clinic start generated code]*/

static PyObject *
os_remove_impl(PyObject *module, path_t *path, int dir_fd)
/*[clinic end generated code: output=a8535b28f0068883 input=e05c5ab55cd30983]*/
{
    return os_unlink_impl(module, path, dir_fd);
}

typedef struct {
    int    now;
    time_t atime_s;
    long   atime_ns;
    time_t mtime_s;
    long   mtime_ns;
} utime_t;

/* Process operations */


/*[clinic input]
os._exit

    status: int

Exit to the system with specified status, without normal exit processing.
[clinic start generated code]*/

static PyObject *
os__exit_impl(PyObject *module, int status)
/*[clinic end generated code: output=116e52d9c2260d54 input=5e6d57556b0c4a62]*/
{
    _exit(status);
    return NULL; /* Make gcc -Wall happy */
}


/* AIX uses /dev/ptc but is otherwise the same as /dev/ptmx */
/* IRIX has both /dev/ptc and /dev/ptmx, use ptmx */
#if defined(HAVE_DEV_PTC) && !defined(HAVE_DEV_PTMX)
#define DEV_PTY_FILE "/dev/ptc"
#define HAVE_DEV_PTMX
#else
#define DEV_PTY_FILE "/dev/ptmx"
#endif

#if defined(HAVE_OPENPTY) || defined(HAVE_FORKPTY) || defined(HAVE_DEV_PTMX)
#ifdef HAVE_PTY_H
#include <pty.h>
#else
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#else
#ifdef HAVE_UTIL_H
#include <util.h>
#endif /* HAVE_UTIL_H */
#endif /* HAVE_LIBUTIL_H */
#endif /* HAVE_PTY_H */
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif
#endif /* defined(HAVE_OPENPTY) || defined(HAVE_FORKPTY) || defined(HAVE_DEV_PTMX) */



#ifdef NGROUPS_MAX
#define MAX_GROUPS NGROUPS_MAX
#else
    /* defined to be 16 on Solaris7, so this should be a small number */
#define MAX_GROUPS 64
#endif



#ifdef HAVE_PLOCK
#ifdef HAVE_SYS_LOCK_H
#include <sys/lock.h>
#endif

/*[clinic input]
os.plock
    op: int
    /

Lock program segments into memory.");
[clinic start generated code]*/

static PyObject *
os_plock_impl(PyObject *module, int op)
/*[clinic end generated code: output=81424167033b168e input=e6e5e348e1525f60]*/
{
    if (plock(op) == -1)
        return posix_error();
    Py_RETURN_NONE;
}
#endif /* HAVE_PLOCK */


#if defined(HAVE_READLINK) || defined(MS_WINDOWS)
/*[clinic input]
os.readlink

    path: path_t
    *
    dir_fd: dir_fd(requires='readlinkat') = None

Return a string representing the path to which the symbolic link points.

If dir_fd is not None, it should be a file descriptor open to a directory,
and path should be relative; path will then be relative to that directory.

dir_fd may not be implemented on your platform.  If it is unavailable,
using it will raise a NotImplementedError.
[clinic start generated code]*/

static PyObject *
os_readlink_impl(PyObject *module, path_t *path, int dir_fd)
/*[clinic end generated code: output=d21b732a2e814030 input=113c87e0db1ecaf2]*/
{
#if defined(HAVE_READLINK)
    char buffer[MAXPATHLEN+1];
    ssize_t length;

    
#ifdef HAVE_READLINKAT
    if (dir_fd != DEFAULT_DIR_FD)
        length = readlinkat(dir_fd, path->narrow, buffer, MAXPATHLEN);
    else
#endif
        length = readlink(path->narrow, buffer, MAXPATHLEN);
    

    if (length < 0) {
        return path_error(path);
    }
    buffer[length] = '\0';

    if (PyUnicode_Check(path->object))
        return PyUnicode_DecodeFSDefaultAndSize(buffer, length);
    else
        return PyBytes_FromStringAndSize(buffer, length);
#elif defined(MS_WINDOWS)
    DWORD n_bytes_returned;
    DWORD io_result = 0;
    HANDLE reparse_point_handle;
    char target_buffer[_Py_MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
    _Py_REPARSE_DATA_BUFFER *rdb = (_Py_REPARSE_DATA_BUFFER *)target_buffer;
    PyObject *result = NULL;

    /* First get a handle to the reparse point */
    
    reparse_point_handle = CreateFileW(
        path->wide,
        0,
        0,
        0,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,
        0);
    if (reparse_point_handle != INVALID_HANDLE_VALUE) {
        /* New call DeviceIoControl to read the reparse point */
        io_result = DeviceIoControl(
            reparse_point_handle,
            FSCTL_GET_REPARSE_POINT,
            0, 0, /* in buffer */
            target_buffer, sizeof(target_buffer),
            &n_bytes_returned,
            0 /* we're not using OVERLAPPED_IO */
            );
        CloseHandle(reparse_point_handle);
    }
    

    if (io_result == 0) {
        return path_error(path);
    }

    wchar_t *name = NULL;
    Py_ssize_t nameLen = 0;
    if (rdb->ReparseTag == IO_REPARSE_TAG_SYMLINK)
    {
        name = (wchar_t *)((char*)rdb->SymbolicLinkReparseBuffer.PathBuffer +
                           rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset);
        nameLen = rdb->SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(wchar_t);
    }
    else if (rdb->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
    {
        name = (wchar_t *)((char*)rdb->MountPointReparseBuffer.PathBuffer +
                           rdb->MountPointReparseBuffer.SubstituteNameOffset);
        nameLen = rdb->MountPointReparseBuffer.SubstituteNameLength / sizeof(wchar_t);
    }
    else
    {
        PyErr_SetString(PyExc_ValueError, "not a symbolic link");
    }
    if (name) {
        if (nameLen > 4 && wcsncmp(name, L"\\??\\", 4) == 0) {
            /* Our buffer is mutable, so this is okay */
            name[1] = L'\\';
        }
        result = PyUnicode_FromWideChar(name, nameLen);
        if (result && path->narrow) {
            Py_SETREF(result, PyUnicode_EncodeFSDefault(result));
        }
    }
    return result;
#endif
}
#endif /* defined(HAVE_READLINK) || defined(MS_WINDOWS) */

#ifdef HAVE_SYMLINK

/*[clinic input]
os.symlink
    src: path_t
    dst: path_t
    target_is_directory: bool = False
    *
    dir_fd: dir_fd(requires='symlinkat')=None

# "symlink(src, dst, target_is_directory=False, *, dir_fd=None)\n\n\

Create a symbolic link pointing to src named dst.

target_is_directory is required on Windows if the target is to be
  interpreted as a directory.  (On Windows, symlink requires
  Windows 6.0 or greater, and raises a NotImplementedError otherwise.)
  target_is_directory is ignored on non-Windows platforms.

If dir_fd is not None, it should be a file descriptor open to a directory,
  and path should be relative; path will then be relative to that directory.
dir_fd may not be implemented on your platform.
  If it is unavailable, using it will raise a NotImplementedError.

[clinic start generated code]*/

static PyObject *
os_symlink_impl(PyObject *module, path_t *src, path_t *dst,
                int target_is_directory, int dir_fd)
/*[clinic end generated code: output=08ca9f3f3cf960f6 input=e820ec4472547bc3]*/
{
    int result;

    if (PySys_Audit("os.symlink", "OOi", src->object, dst->object,
                    dir_fd == DEFAULT_DIR_FD ? -1 : dir_fd) < 0) {
        return NULL;
    }
    if ((src->narrow && dst->wide) || (src->wide && dst->narrow)) {
        PyErr_SetString(PyExc_ValueError,
            "symlink: src and dst must be the same type");
        return NULL;
    }

    
#if HAVE_SYMLINKAT
    if (dir_fd != DEFAULT_DIR_FD)
        result = symlinkat(src->narrow, dir_fd, dst->narrow);
    else
#endif
        result = symlink(src->narrow, dst->narrow);
    

    if (result)
        return path_error2(src, dst);

    Py_RETURN_NONE;
}
#endif /* HAVE_SYMLINK */


/* Functions acting on file descriptors */

#ifdef O_CLOEXEC
extern int _Py_open_cloexec_works;
#endif


/*[clinic input]
os.open -> int
    path: path_t
    flags: int
    mode: int = 0o777
    *
    dir_fd: dir_fd(requires='openat') = None

# "open(path, flags, mode=0o777, *, dir_fd=None)\n\n\

Open a file for low level IO.  Returns a file descriptor (integer).

If dir_fd is not None, it should be a file descriptor open to a directory,
  and path should be relative; path will then be relative to that directory.
dir_fd may not be implemented on your platform.
  If it is unavailable, using it will raise a NotImplementedError.
[clinic start generated code]*/

static int
os_open_impl(PyObject *module, path_t *path, int flags, int mode, int dir_fd)
/*[clinic end generated code: output=abc7227888c8bc73 input=ad8623b29acd2934]*/
{
    int fd;
    int async_err = 0;

#ifdef O_CLOEXEC
    int *atomic_flag_works = &_Py_open_cloexec_works;
#elif !defined(MS_WINDOWS)
    int *atomic_flag_works = NULL;
#endif

#ifdef MS_WINDOWS
    flags |= O_NOINHERIT;
#elif defined(O_CLOEXEC)
    flags |= O_CLOEXEC;
#endif

    if (PySys_Audit("open", "OOi", path->object, Py_None, flags) < 0) {
        return -1;
    }

    
    do {
        
#ifdef MS_WINDOWS
        fd = _wopen(path->wide, flags, mode);
#else
#ifdef HAVE_OPENAT
        if (dir_fd != DEFAULT_DIR_FD)
            fd = openat(dir_fd, path->narrow, flags, mode);
        else
#endif /* HAVE_OPENAT */
            fd = open(path->narrow, flags, mode);
#endif /* !MS_WINDOWS */
        
	  } while (fd < 0 && errno == EINTR); // && !(async_err = PyErr_CheckSignals()));
    

    if (fd < 0) {
        if (!async_err)
            PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path->object);
        return -1;
    }

#ifndef MS_WINDOWS
    if (_Py_set_inheritable(fd, 0, atomic_flag_works) < 0) {
        close(fd);
        return -1;
    }
#endif

    return fd;
}


/*[clinic input]
os.close

    fd: int

Close a file descriptor.
[clinic start generated code]*/

static PyObject *
os_close_impl(PyObject *module, int fd)
/*[clinic end generated code: output=2fe4e93602822c14 input=2bc42451ca5c3223]*/
{
    int res;
    /* We do not want to retry upon EINTR: see http://lwn.net/Articles/576478/
     * and http://linux.derkeiler.com/Mailing-Lists/Kernel/2005-09/3000.html
     * for more details.
     */
    
    
    res = close(fd);
    
    
    if (res < 0)
        return posix_error();
    Py_RETURN_NONE;
}


#ifdef HAVE_FDWALK
static int
_fdwalk_close_func(void *lohi, int fd)
{
    int lo = ((int *)lohi)[0];
    int hi = ((int *)lohi)[1];

    if (fd >= hi) {
        return 1;
    }
    else if (fd >= lo) {
        /* Ignore errors */
        (void)close(fd);
    }
    return 0;
}
#endif /* HAVE_FDWALK */

#ifdef HAVE_LOCKF
/*[clinic input]
os.lockf

    fd: int
        An open file descriptor.
    command: int
        One of F_LOCK, F_TLOCK, F_ULOCK or F_TEST.
    length: Py_off_t
        The number of bytes to lock, starting at the current position.
    /

Apply, test or remove a POSIX lock on an open file descriptor.

[clinic start generated code]*/

static PyObject *
os_lockf_impl(PyObject *module, int fd, int command, Py_off_t length)
/*[clinic end generated code: output=af7051f3e7c29651 input=65da41d2106e9b79]*/
{
    int res;

    if (PySys_Audit("os.lockf", "iiL", fd, command, length) < 0) {
        return NULL;
    }

    
    res = lockf(fd, command, length);
    

    if (res < 0)
        return posix_error();

    Py_RETURN_NONE;
}
#endif /* HAVE_LOCKF */


/*[clinic input]
os.lseek -> Py_off_t

    fd: int
    position: Py_off_t
    how: int
    /

Set the position of a file descriptor.  Return the new position.

Return the new cursor position in number of bytes
relative to the beginning of the file.
[clinic start generated code]*/

static Py_off_t
os_lseek_impl(PyObject *module, int fd, Py_off_t position, int how)
/*[clinic end generated code: output=971e1efb6b30bd2f input=902654ad3f96a6d3]*/
{
    Py_off_t result;

#ifdef SEEK_SET
    /* Turn 0, 1, 2 into SEEK_{SET,CUR,END} */
    switch (how) {
        case 0: how = SEEK_SET; break;
        case 1: how = SEEK_CUR; break;
        case 2: how = SEEK_END; break;
    }
#endif /* SEEK_END */

    
    
#ifdef MS_WINDOWS
    result = _lseeki64(fd, position, how);
#else
    result = lseek(fd, position, how);
#endif
    
    
    if (result < 0)
        posix_error();

    return result;
}


/*[clinic input]
os.read
    fd: int
    length: Py_ssize_t
    /

Read from a file descriptor.  Returns a bytes object.
[clinic start generated code]*/

static PyObject *
os_read_impl(PyObject *module, int fd, Py_ssize_t length)
/*[clinic end generated code: output=dafbe9a5cddb987b input=1df2eaa27c0bf1d3]*/
{
    Py_ssize_t n;
    PyObject *buffer;

    if (length < 0) {
        errno = EINVAL;
        return posix_error();
    }

    length = Py_MIN(length, _PY_READ_MAX);

    buffer = PyBytes_FromStringAndSize((char *)NULL, length);
    if (buffer == NULL)
        return NULL;

    n = _Py_read(fd, PyBytes_AS_STRING(buffer), length);
    if (n == -1) {
        Py_DECREF(buffer);
        return NULL;
    }

    if (n != length)
        _PyBytes_Resize(&buffer, n);

    return buffer;
}


/*[clinic input]
os.write -> Py_ssize_t

    fd: int
    data: Py_buffer
    /

Write a bytes object to a file descriptor.
[clinic start generated code]*/

static Py_ssize_t
os_write_impl(PyObject *module, int fd, Py_buffer *data)
/*[clinic end generated code: output=e4ef5bc904b58ef9 input=3207e28963234f3c]*/
{
    return _Py_write(fd, data->buf, data->len);
}

/*[clinic input]
os.fstat

    fd : int

Perform a stat system call on the given file descriptor.

Like stat(), but for an open file descriptor.
Equivalent to os.stat(fd).
[clinic start generated code]*/

static PyObject *
os_fstat_impl(PyObject *module, int fd)
/*[clinic end generated code: output=efc038cb5f654492 input=27e0e0ebbe5600c9]*/
{
    STRUCT_STAT st;
    int res;
    int async_err = 0;

    do {
        
        res = FSTAT(fd, &st);
        
    } while (res != 0 && errno == EINTR); // && !(async_err = PyErr_CheckSignals()));
    if (res != 0) {
#ifdef MS_WINDOWS
        return PyErr_SetFromWindowsErr(0);
#else
        return (!async_err) ? posix_error() : NULL;
#endif
    }

    return _pystat_fromstructstat(module, &st);
}


/*[clinic input]
os.isatty -> bool
    fd: int
    /

Return True if the fd is connected to a terminal.

Return True if the file descriptor is an open file descriptor
connected to the slave end of a terminal.
[clinic start generated code]*/

static int
os_isatty_impl(PyObject *module, int fd)
/*[clinic end generated code: output=6a48c8b4e644ca00 input=08ce94aa1eaf7b5e]*/
{
    int return_value;
    
    return_value = isatty(fd);
    
    return return_value;
}


#if defined HAVE_FTRUNCATE || defined MS_WINDOWS
/*[clinic input]
os.ftruncate

    fd: int
    length: Py_off_t
    /

Truncate a file, specified by file descriptor, to a specific length.
[clinic start generated code]*/

static PyObject *
os_ftruncate_impl(PyObject *module, int fd, Py_off_t length)
/*[clinic end generated code: output=fba15523721be7e4 input=63b43641e52818f2]*/
{
    int result;
    int async_err = 0;

    if (PySys_Audit("os.truncate", "in", fd, length) < 0) {
        return NULL;
    }

    do {
        
        
#ifdef MS_WINDOWS
        result = _chsize_s(fd, length);
#else
        result = ftruncate(fd, length);
#endif
        
        
	  } while (result != 0 && errno == EINTR); // &&
    //             !(async_err = PyErr_CheckSignals()));
    if (result != 0)
        return (!async_err) ? posix_error() : NULL;
    Py_RETURN_NONE;
}
#endif /* HAVE_FTRUNCATE || MS_WINDOWS */


#if defined HAVE_TRUNCATE || defined MS_WINDOWS
/*[clinic input]
os.truncate
    path: path_t(allow_fd='PATH_HAVE_FTRUNCATE')
    length: Py_off_t

Truncate a file, specified by path, to a specific length.

On some platforms, path may also be specified as an open file descriptor.
  If this functionality is unavailable, using it raises an exception.
[clinic start generated code]*/

static PyObject *
os_truncate_impl(PyObject *module, path_t *path, Py_off_t length)
/*[clinic end generated code: output=43009c8df5c0a12b input=77229cf0b50a9b77]*/
{
    int result;
#ifdef MS_WINDOWS
    int fd;
#endif

    if (path->fd != -1)
        return os_ftruncate_impl(module, path->fd, length);

    if (PySys_Audit("os.truncate", "On", path->object, length) < 0) {
        return NULL;
    }

    
    
#ifdef MS_WINDOWS
    fd = _wopen(path->wide, _O_WRONLY | _O_BINARY | _O_NOINHERIT);
    if (fd < 0)
        result = -1;
    else {
        result = _chsize_s(fd, length);
        close(fd);
        if (result < 0)
            errno = result;
    }
#else
    result = truncate(path->narrow, length);
#endif
    
    
    if (result < 0)
        return posix_path_error(path);

    Py_RETURN_NONE;
}
#endif /* HAVE_TRUNCATE || MS_WINDOWS */

/*[clinic input]
os.putenv

    name: FSConverter
    value: FSConverter
    /

Change or add an environment variable.
[clinic start generated code]*/

static PyObject *
os_putenv_impl(PyObject *module, PyObject *name, PyObject *value)
/*[clinic end generated code: output=d29a567d6b2327d2 input=a97bc6152f688d31]*/
{
    const char *name_string = PyBytes_AS_STRING(name);
    const char *value_string = PyBytes_AS_STRING(value);

    if (strchr(name_string, '=') != NULL) {
        PyErr_SetString(PyExc_ValueError, "illegal environment variable name");
        return NULL;
    }

    if (PySys_Audit("os.putenv", "OO", name, value) < 0) {
        return NULL;
    }

    if (setenv(name_string, value_string, 1)) {
        return posix_error();
    }
    Py_RETURN_NONE;
}
/*[clinic input]
os.unsetenv
    name: FSConverter
    /

Delete an environment variable.
[clinic start generated code]*/

static PyObject *
os_unsetenv_impl(PyObject *module, PyObject *name)
/*[clinic end generated code: output=54c4137ab1834f02 input=2bb5288a599c7107]*/
{
    if (PySys_Audit("os.unsetenv", "(O)", name) < 0) {
        return NULL;
    }
#ifdef HAVE_BROKEN_UNSETENV
    unsetenv(PyBytes_AS_STRING(name));
#else
    int err = unsetenv(PyBytes_AS_STRING(name));
    if (err) {
        return posix_error();
    }
#endif

    Py_RETURN_NONE;
}

/*[clinic input]
os.strerror

    code: int
    /

Translate an error code to a message string.
[clinic start generated code]*/

static PyObject *
os_strerror_impl(PyObject *module, int code)
/*[clinic end generated code: output=baebf09fa02a78f2 input=75a8673d97915a91]*/
{
    char *message = strerror(code);
    if (message == NULL) {
        PyErr_SetString(PyExc_ValueError,
                        "strerror() argument out of range");
        return NULL;
    }
    return PyUnicode_DecodeLocale(message, "surrogateescape");
}


#if defined(HAVE_FSTATVFS) && defined(HAVE_SYS_STATVFS_H)
#ifdef _SCO_DS
/* SCO OpenServer 5.0 and later requires _SVID3 before it reveals the
   needed definitions in sys/statvfs.h */
#define _SVID3
#endif
#include <sys/statvfs.h>

static PyObject*
_pystatvfs_fromstructstatvfs(PyObject *module, struct statvfs st) {
    PyObject *StatVFSResultType = get_posix_state(module)->StatVFSResultType;
    PyObject *v = PyStructSequence_New((PyTypeObject *)StatVFSResultType);
    if (v == NULL)
        return NULL;

#if !defined(HAVE_LARGEFILE_SUPPORT)
    PyStructSequence_SET_ITEM(v, 0, PyLong_FromLong((long) st.f_bsize));
    PyStructSequence_SET_ITEM(v, 1, PyLong_FromLong((long) st.f_frsize));
    PyStructSequence_SET_ITEM(v, 2, PyLong_FromLong((long) st.f_blocks));
    PyStructSequence_SET_ITEM(v, 3, PyLong_FromLong((long) st.f_bfree));
    PyStructSequence_SET_ITEM(v, 4, PyLong_FromLong((long) st.f_bavail));
    PyStructSequence_SET_ITEM(v, 5, PyLong_FromLong((long) st.f_files));
    PyStructSequence_SET_ITEM(v, 6, PyLong_FromLong((long) st.f_ffree));
    PyStructSequence_SET_ITEM(v, 7, PyLong_FromLong((long) st.f_favail));
    PyStructSequence_SET_ITEM(v, 8, PyLong_FromLong((long) st.f_flag));
    PyStructSequence_SET_ITEM(v, 9, PyLong_FromLong((long) st.f_namemax));
#else
    PyStructSequence_SET_ITEM(v, 0, PyLong_FromLong((long) st.f_bsize));
    PyStructSequence_SET_ITEM(v, 1, PyLong_FromLong((long) st.f_frsize));
    PyStructSequence_SET_ITEM(v, 2,
                              PyLong_FromLongLong((long long) st.f_blocks));
    PyStructSequence_SET_ITEM(v, 3,
                              PyLong_FromLongLong((long long) st.f_bfree));
    PyStructSequence_SET_ITEM(v, 4,
                              PyLong_FromLongLong((long long) st.f_bavail));
    PyStructSequence_SET_ITEM(v, 5,
                              PyLong_FromLongLong((long long) st.f_files));
    PyStructSequence_SET_ITEM(v, 6,
                              PyLong_FromLongLong((long long) st.f_ffree));
    PyStructSequence_SET_ITEM(v, 7,
                              PyLong_FromLongLong((long long) st.f_favail));
    PyStructSequence_SET_ITEM(v, 8, PyLong_FromLong((long) st.f_flag));
    PyStructSequence_SET_ITEM(v, 9, PyLong_FromLong((long) st.f_namemax));
#endif
/* The _ALL_SOURCE feature test macro defines f_fsid as a structure
 * (issue #32390). */
#if defined(_AIX) && defined(_ALL_SOURCE)
    PyStructSequence_SET_ITEM(v, 10, PyLong_FromUnsignedLong(st.f_fsid.val[0]));
#else
    PyStructSequence_SET_ITEM(v, 10, PyLong_FromUnsignedLong(st.f_fsid));
#endif
    if (PyErr_Occurred()) {
        Py_DECREF(v);
        return NULL;
    }

    return v;
}


/*[clinic input]
os.fstatvfs
    fd: int
    /

Perform an fstatvfs system call on the given fd.

Equivalent to statvfs(fd).
[clinic start generated code]*/

static PyObject *
os_fstatvfs_impl(PyObject *module, int fd)
/*[clinic end generated code: output=53547cf0cc55e6c5 input=d8122243ac50975e]*/
{
    int result;
    int async_err = 0;
    struct statvfs st;

    do {
        
        result = fstatvfs(fd, &st);
        
	  } while (result != 0 && errno == EINTR); // &&
    //             !(async_err = PyErr_CheckSignals()));
    if (result != 0)
        return (!async_err) ? posix_error() : NULL;

    return _pystatvfs_fromstructstatvfs(module, st);
}
#endif /* defined(HAVE_FSTATVFS) && defined(HAVE_SYS_STATVFS_H) */


#if defined(HAVE_STATVFS) && defined(HAVE_SYS_STATVFS_H)
#include <sys/statvfs.h>
/*[clinic input]
os.statvfs

    path: path_t(allow_fd='PATH_HAVE_FSTATVFS')

Perform a statvfs system call on the given path.

path may always be specified as a string.
On some platforms, path may also be specified as an open file descriptor.
  If this functionality is unavailable, using it raises an exception.
[clinic start generated code]*/

static PyObject *
os_statvfs_impl(PyObject *module, path_t *path)
/*[clinic end generated code: output=87106dd1beb8556e input=3f5c35791c669bd9]*/
{
    int result;
    struct statvfs st;

    
#ifdef HAVE_FSTATVFS
    if (path->fd != -1) {
#ifdef __APPLE__
        /* handle weak-linking on Mac OS X 10.3 */
        if (fstatvfs == NULL) {
            fd_specified("statvfs", path->fd);
            return NULL;
        }
#endif
        result = fstatvfs(path->fd, &st);
    }
    else
#endif
        result = statvfs(path->narrow, &st);
    

    if (result) {
        return path_error(path);
    }

    return _pystatvfs_fromstructstatvfs(module, st);
}
#endif /* defined(HAVE_STATVFS) && defined(HAVE_SYS_STATVFS_H) */


#ifdef MS_WINDOWS
/*[clinic input]
os._getdiskusage

    path: path_t

Return disk usage statistics about the given path as a (total, free) tuple.
[clinic start generated code]*/

static PyObject *
os__getdiskusage_impl(PyObject *module, path_t *path)
/*[clinic end generated code: output=3bd3991f5e5c5dfb input=6af8d1b7781cc042]*/
{
    BOOL retval;
    ULARGE_INTEGER _, total, free;
    DWORD err = 0;

    
    retval = GetDiskFreeSpaceExW(path->wide, &_, &total, &free);
    
    if (retval == 0) {
        if (GetLastError() == ERROR_DIRECTORY) {
            wchar_t *dir_path = NULL;

            dir_path = PyMem_New(wchar_t, path->length + 1);
            if (dir_path == NULL) {
                return PyErr_NoMemory();
            }

            wcscpy_s(dir_path, path->length + 1, path->wide);

            if (_dirnameW(dir_path) != -1) {
                
                retval = GetDiskFreeSpaceExW(dir_path, &_, &total, &free);
                
            }
            /* Record the last error in case it's modified by PyMem_Free. */
            err = GetLastError();
            PyMem_Free(dir_path);
            if (retval) {
                goto success;
            }
        }
        return PyErr_SetFromWindowsErr(err);
    }

success:
    return Py_BuildValue("(LL)", total.QuadPart, free.QuadPart);
}
#endif /* MS_WINDOWS */


/* This is used for fpathconf(), pathconf(), confstr() and sysconf().
 * It maps strings representing configuration variable names to
 * integer values, allowing those functions to be called with the
 * magic names instead of polluting the module's namespace with tons of
 * rarely-used constants.  There are three separate tables that use
 * these definitions.
 *
 * This code is always included, even if none of the interfaces that
 * need it are included.  The #if hackery needed to avoid it would be
 * sufficiently pervasive that it's not worth the loss of readability.
 */
struct constdef {
    const char *name;
    int value;
};

static int
conv_confname(PyObject *arg, int *valuep, struct constdef *table,
              size_t tablesize)
{
    if (PyLong_Check(arg)) {
        int value = _PyLong_AsInt(arg);
        if (value == -1 && PyErr_Occurred())
            return 0;
        *valuep = value;
        return 1;
    }
    else {
        /* look up the value in the table using a binary search */
        size_t lo = 0;
        size_t mid;
        size_t hi = tablesize;
        int cmp;
        const char *confname;
        if (!PyUnicode_Check(arg)) {
            PyErr_SetString(PyExc_TypeError,
                "configuration names must be strings or integers");
            return 0;
        }
        confname = PyUnicode_AsUTF8(arg);
        if (confname == NULL)
            return 0;
        while (lo < hi) {
            mid = (lo + hi) / 2;
            cmp = strcmp(confname, table[mid].name);
            if (cmp < 0)
                hi = mid;
            else if (cmp > 0)
                lo = mid + 1;
            else {
                *valuep = table[mid].value;
                return 1;
            }
        }
        PyErr_SetString(PyExc_ValueError, "unrecognized configuration name");
        return 0;
    }
}


#if defined(HAVE_FPATHCONF) || defined(HAVE_PATHCONF)
static struct constdef  posix_constants_pathconf[] = {
#ifdef _PC_ABI_AIO_XFER_MAX
    {"PC_ABI_AIO_XFER_MAX",     _PC_ABI_AIO_XFER_MAX},
#endif
#ifdef _PC_ABI_ASYNC_IO
    {"PC_ABI_ASYNC_IO", _PC_ABI_ASYNC_IO},
#endif
#ifdef _PC_ASYNC_IO
    {"PC_ASYNC_IO",     _PC_ASYNC_IO},
#endif
#ifdef _PC_CHOWN_RESTRICTED
    {"PC_CHOWN_RESTRICTED",     _PC_CHOWN_RESTRICTED},
#endif
#ifdef _PC_FILESIZEBITS
    {"PC_FILESIZEBITS", _PC_FILESIZEBITS},
#endif
#ifdef _PC_LAST
    {"PC_LAST", _PC_LAST},
#endif
#ifdef _PC_LINK_MAX
    {"PC_LINK_MAX",     _PC_LINK_MAX},
#endif
#ifdef _PC_MAX_CANON
    {"PC_MAX_CANON",    _PC_MAX_CANON},
#endif
#ifdef _PC_MAX_INPUT
    {"PC_MAX_INPUT",    _PC_MAX_INPUT},
#endif
#ifdef _PC_NAME_MAX
    {"PC_NAME_MAX",     _PC_NAME_MAX},
#endif
#ifdef _PC_NO_TRUNC
    {"PC_NO_TRUNC",     _PC_NO_TRUNC},
#endif
#ifdef _PC_PATH_MAX
    {"PC_PATH_MAX",     _PC_PATH_MAX},
#endif
#ifdef _PC_PIPE_BUF
    {"PC_PIPE_BUF",     _PC_PIPE_BUF},
#endif
#ifdef _PC_PRIO_IO
    {"PC_PRIO_IO",      _PC_PRIO_IO},
#endif
#ifdef _PC_SOCK_MAXBUF
    {"PC_SOCK_MAXBUF",  _PC_SOCK_MAXBUF},
#endif
#ifdef _PC_SYNC_IO
    {"PC_SYNC_IO",      _PC_SYNC_IO},
#endif
#ifdef _PC_VDISABLE
    {"PC_VDISABLE",     _PC_VDISABLE},
#endif
#ifdef _PC_ACL_ENABLED
    {"PC_ACL_ENABLED",  _PC_ACL_ENABLED},
#endif
#ifdef _PC_MIN_HOLE_SIZE
    {"PC_MIN_HOLE_SIZE",    _PC_MIN_HOLE_SIZE},
#endif
#ifdef _PC_ALLOC_SIZE_MIN
    {"PC_ALLOC_SIZE_MIN",   _PC_ALLOC_SIZE_MIN},
#endif
#ifdef _PC_REC_INCR_XFER_SIZE
    {"PC_REC_INCR_XFER_SIZE",   _PC_REC_INCR_XFER_SIZE},
#endif
#ifdef _PC_REC_MAX_XFER_SIZE
    {"PC_REC_MAX_XFER_SIZE",    _PC_REC_MAX_XFER_SIZE},
#endif
#ifdef _PC_REC_MIN_XFER_SIZE
    {"PC_REC_MIN_XFER_SIZE",    _PC_REC_MIN_XFER_SIZE},
#endif
#ifdef _PC_REC_XFER_ALIGN
    {"PC_REC_XFER_ALIGN",   _PC_REC_XFER_ALIGN},
#endif
#ifdef _PC_SYMLINK_MAX
    {"PC_SYMLINK_MAX",  _PC_SYMLINK_MAX},
#endif
#ifdef _PC_XATTR_ENABLED
    {"PC_XATTR_ENABLED",    _PC_XATTR_ENABLED},
#endif
#ifdef _PC_XATTR_EXISTS
    {"PC_XATTR_EXISTS", _PC_XATTR_EXISTS},
#endif
#ifdef _PC_TIMESTAMP_RESOLUTION
    {"PC_TIMESTAMP_RESOLUTION", _PC_TIMESTAMP_RESOLUTION},
#endif
};

static int
conv_path_confname(PyObject *arg, int *valuep)
{
    return conv_confname(arg, valuep, posix_constants_pathconf,
                         sizeof(posix_constants_pathconf)
                           / sizeof(struct constdef));
}
#endif


#ifdef HAVE_FPATHCONF
/*[clinic input]
os.fpathconf -> long

    fd: int
    name: path_confname
    /

Return the configuration limit name for the file descriptor fd.

If there is no limit, return -1.
[clinic start generated code]*/

static long
os_fpathconf_impl(PyObject *module, int fd, int name)
/*[clinic end generated code: output=d5b7042425fc3e21 input=5942a024d3777810]*/
{
    long limit;

    errno = 0;
    limit = fpathconf(fd, name);
    if (limit == -1 && errno != 0)
        posix_error();

    return limit;
}
#endif /* HAVE_FPATHCONF */


#ifdef HAVE_PATHCONF
/*[clinic input]
os.pathconf -> long
    path: path_t(allow_fd='PATH_HAVE_FPATHCONF')
    name: path_confname

Return the configuration limit name for the file or directory path.

If there is no limit, return -1.
On some platforms, path may also be specified as an open file descriptor.
  If this functionality is unavailable, using it raises an exception.
[clinic start generated code]*/

static long
os_pathconf_impl(PyObject *module, path_t *path, int name)
/*[clinic end generated code: output=5bedee35b293a089 input=bc3e2a985af27e5e]*/
{
    long limit;

    errno = 0;
#ifdef HAVE_FPATHCONF
    if (path->fd != -1)
        limit = fpathconf(path->fd, name);
    else
#endif
        limit = pathconf(path->narrow, name);
    if (limit == -1 && errno != 0) {
        if (errno == EINVAL)
            /* could be a path or name problem */
            posix_error();
        else
            path_error(path);
    }

    return limit;
}
#endif /* HAVE_PATHCONF */

#ifdef HAVE_CONFSTR
static struct constdef posix_constants_confstr[] = {
#ifdef _CS_ARCHITECTURE
    {"CS_ARCHITECTURE", _CS_ARCHITECTURE},
#endif
#ifdef _CS_GNU_LIBC_VERSION
    {"CS_GNU_LIBC_VERSION",     _CS_GNU_LIBC_VERSION},
#endif
#ifdef _CS_GNU_LIBPTHREAD_VERSION
    {"CS_GNU_LIBPTHREAD_VERSION",       _CS_GNU_LIBPTHREAD_VERSION},
#endif
#ifdef _CS_HOSTNAME
    {"CS_HOSTNAME",     _CS_HOSTNAME},
#endif
#ifdef _CS_HW_PROVIDER
    {"CS_HW_PROVIDER",  _CS_HW_PROVIDER},
#endif
#ifdef _CS_HW_SERIAL
    {"CS_HW_SERIAL",    _CS_HW_SERIAL},
#endif
#ifdef _CS_INITTAB_NAME
    {"CS_INITTAB_NAME", _CS_INITTAB_NAME},
#endif
#ifdef _CS_LFS64_CFLAGS
    {"CS_LFS64_CFLAGS", _CS_LFS64_CFLAGS},
#endif
#ifdef _CS_LFS64_LDFLAGS
    {"CS_LFS64_LDFLAGS",        _CS_LFS64_LDFLAGS},
#endif
#ifdef _CS_LFS64_LIBS
    {"CS_LFS64_LIBS",   _CS_LFS64_LIBS},
#endif
#ifdef _CS_LFS64_LINTFLAGS
    {"CS_LFS64_LINTFLAGS",      _CS_LFS64_LINTFLAGS},
#endif
#ifdef _CS_LFS_CFLAGS
    {"CS_LFS_CFLAGS",   _CS_LFS_CFLAGS},
#endif
#ifdef _CS_LFS_LDFLAGS
    {"CS_LFS_LDFLAGS",  _CS_LFS_LDFLAGS},
#endif
#ifdef _CS_LFS_LIBS
    {"CS_LFS_LIBS",     _CS_LFS_LIBS},
#endif
#ifdef _CS_LFS_LINTFLAGS
    {"CS_LFS_LINTFLAGS",        _CS_LFS_LINTFLAGS},
#endif
#ifdef _CS_MACHINE
    {"CS_MACHINE",      _CS_MACHINE},
#endif
#ifdef _CS_PATH
    {"CS_PATH", _CS_PATH},
#endif
#ifdef _CS_RELEASE
    {"CS_RELEASE",      _CS_RELEASE},
#endif
#ifdef _CS_SRPC_DOMAIN
    {"CS_SRPC_DOMAIN",  _CS_SRPC_DOMAIN},
#endif
#ifdef _CS_SYSNAME
    {"CS_SYSNAME",      _CS_SYSNAME},
#endif
#ifdef _CS_VERSION
    {"CS_VERSION",      _CS_VERSION},
#endif
#ifdef _CS_XBS5_ILP32_OFF32_CFLAGS
    {"CS_XBS5_ILP32_OFF32_CFLAGS",      _CS_XBS5_ILP32_OFF32_CFLAGS},
#endif
#ifdef _CS_XBS5_ILP32_OFF32_LDFLAGS
    {"CS_XBS5_ILP32_OFF32_LDFLAGS",     _CS_XBS5_ILP32_OFF32_LDFLAGS},
#endif
#ifdef _CS_XBS5_ILP32_OFF32_LIBS
    {"CS_XBS5_ILP32_OFF32_LIBS",        _CS_XBS5_ILP32_OFF32_LIBS},
#endif
#ifdef _CS_XBS5_ILP32_OFF32_LINTFLAGS
    {"CS_XBS5_ILP32_OFF32_LINTFLAGS",   _CS_XBS5_ILP32_OFF32_LINTFLAGS},
#endif
#ifdef _CS_XBS5_ILP32_OFFBIG_CFLAGS
    {"CS_XBS5_ILP32_OFFBIG_CFLAGS",     _CS_XBS5_ILP32_OFFBIG_CFLAGS},
#endif
#ifdef _CS_XBS5_ILP32_OFFBIG_LDFLAGS
    {"CS_XBS5_ILP32_OFFBIG_LDFLAGS",    _CS_XBS5_ILP32_OFFBIG_LDFLAGS},
#endif
#ifdef _CS_XBS5_ILP32_OFFBIG_LIBS
    {"CS_XBS5_ILP32_OFFBIG_LIBS",       _CS_XBS5_ILP32_OFFBIG_LIBS},
#endif
#ifdef _CS_XBS5_ILP32_OFFBIG_LINTFLAGS
    {"CS_XBS5_ILP32_OFFBIG_LINTFLAGS",  _CS_XBS5_ILP32_OFFBIG_LINTFLAGS},
#endif
#ifdef _CS_XBS5_LP64_OFF64_CFLAGS
    {"CS_XBS5_LP64_OFF64_CFLAGS",       _CS_XBS5_LP64_OFF64_CFLAGS},
#endif
#ifdef _CS_XBS5_LP64_OFF64_LDFLAGS
    {"CS_XBS5_LP64_OFF64_LDFLAGS",      _CS_XBS5_LP64_OFF64_LDFLAGS},
#endif
#ifdef _CS_XBS5_LP64_OFF64_LIBS
    {"CS_XBS5_LP64_OFF64_LIBS", _CS_XBS5_LP64_OFF64_LIBS},
#endif
#ifdef _CS_XBS5_LP64_OFF64_LINTFLAGS
    {"CS_XBS5_LP64_OFF64_LINTFLAGS",    _CS_XBS5_LP64_OFF64_LINTFLAGS},
#endif
#ifdef _CS_XBS5_LPBIG_OFFBIG_CFLAGS
    {"CS_XBS5_LPBIG_OFFBIG_CFLAGS",     _CS_XBS5_LPBIG_OFFBIG_CFLAGS},
#endif
#ifdef _CS_XBS5_LPBIG_OFFBIG_LDFLAGS
    {"CS_XBS5_LPBIG_OFFBIG_LDFLAGS",    _CS_XBS5_LPBIG_OFFBIG_LDFLAGS},
#endif
#ifdef _CS_XBS5_LPBIG_OFFBIG_LIBS
    {"CS_XBS5_LPBIG_OFFBIG_LIBS",       _CS_XBS5_LPBIG_OFFBIG_LIBS},
#endif
#ifdef _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS
    {"CS_XBS5_LPBIG_OFFBIG_LINTFLAGS",  _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS},
#endif
#ifdef _MIPS_CS_AVAIL_PROCESSORS
    {"MIPS_CS_AVAIL_PROCESSORS",        _MIPS_CS_AVAIL_PROCESSORS},
#endif
#ifdef _MIPS_CS_BASE
    {"MIPS_CS_BASE",    _MIPS_CS_BASE},
#endif
#ifdef _MIPS_CS_HOSTID
    {"MIPS_CS_HOSTID",  _MIPS_CS_HOSTID},
#endif
#ifdef _MIPS_CS_HW_NAME
    {"MIPS_CS_HW_NAME", _MIPS_CS_HW_NAME},
#endif
#ifdef _MIPS_CS_NUM_PROCESSORS
    {"MIPS_CS_NUM_PROCESSORS",  _MIPS_CS_NUM_PROCESSORS},
#endif
#ifdef _MIPS_CS_OSREL_MAJ
    {"MIPS_CS_OSREL_MAJ",       _MIPS_CS_OSREL_MAJ},
#endif
#ifdef _MIPS_CS_OSREL_MIN
    {"MIPS_CS_OSREL_MIN",       _MIPS_CS_OSREL_MIN},
#endif
#ifdef _MIPS_CS_OSREL_PATCH
    {"MIPS_CS_OSREL_PATCH",     _MIPS_CS_OSREL_PATCH},
#endif
#ifdef _MIPS_CS_OS_NAME
    {"MIPS_CS_OS_NAME", _MIPS_CS_OS_NAME},
#endif
#ifdef _MIPS_CS_OS_PROVIDER
    {"MIPS_CS_OS_PROVIDER",     _MIPS_CS_OS_PROVIDER},
#endif
#ifdef _MIPS_CS_PROCESSORS
    {"MIPS_CS_PROCESSORS",      _MIPS_CS_PROCESSORS},
#endif
#ifdef _MIPS_CS_SERIAL
    {"MIPS_CS_SERIAL",  _MIPS_CS_SERIAL},
#endif
#ifdef _MIPS_CS_VENDOR
    {"MIPS_CS_VENDOR",  _MIPS_CS_VENDOR},
#endif
};

static int
conv_confstr_confname(PyObject *arg, int *valuep)
{
    return conv_confname(arg, valuep, posix_constants_confstr,
                         sizeof(posix_constants_confstr)
                           / sizeof(struct constdef));
}


/*[clinic input]
os.confstr

    name: confstr_confname
    /

Return a string-valued system configuration variable.
[clinic start generated code]*/

static PyObject *
os_confstr_impl(PyObject *module, int name)
/*[clinic end generated code: output=bfb0b1b1e49b9383 input=18fb4d0567242e65]*/
{
    PyObject *result = NULL;
    char buffer[255];
    size_t len;

    errno = 0;
    len = confstr(name, buffer, sizeof(buffer));
    if (len == 0) {
        if (errno) {
            posix_error();
            return NULL;
        }
        else {
            Py_RETURN_NONE;
        }
    }

    if (len >= sizeof(buffer)) {
        size_t len2;
        char *buf = PyMem_Malloc(len);
        if (buf == NULL)
            return PyErr_NoMemory();
        len2 = confstr(name, buf, len);
        assert(len == len2);
        result = PyUnicode_DecodeFSDefaultAndSize(buf, len2-1);
        PyMem_Free(buf);
    }
    else
        result = PyUnicode_DecodeFSDefaultAndSize(buffer, len-1);
    return result;
}
#endif /* HAVE_CONFSTR */



/* This code is used to ensure that the tables of configuration value names
 * are in sorted order as required by conv_confname(), and also to build
 * the exported dictionaries that are used to publish information about the
 * names available on the host platform.
 *
 * Sorting the table at runtime ensures that the table is properly ordered
 * when used, even for platforms we're not able to test on.  It also makes
 * it easier to add additional entries to the tables.
 */

static int
cmp_constdefs(const void *v1,  const void *v2)
{
    const struct constdef *c1 =
    (const struct constdef *) v1;
    const struct constdef *c2 =
    (const struct constdef *) v2;

    return strcmp(c1->name, c2->name);
}

static int
setup_confname_table(struct constdef *table, size_t tablesize,
                     const char *tablename, PyObject *module)
{
    PyObject *d = NULL;
    size_t i;

    qsort(table, tablesize, sizeof(struct constdef), cmp_constdefs);
    d = PyDict_New();
    if (d == NULL)
        return -1;

    for (i=0; i < tablesize; ++i) {
        PyObject *o = PyLong_FromLong(table[i].value);
        if (o == NULL || PyDict_SetItemString(d, table[i].name, o) == -1) {
            Py_XDECREF(o);
            Py_DECREF(d);
            return -1;
        }
        Py_DECREF(o);
    }
    return PyModule_AddObject(module, tablename, d);
}

/* Return -1 on failure, 0 on success. */
static int
setup_confname_tables(PyObject *module)
{
#if defined(HAVE_FPATHCONF) || defined(HAVE_PATHCONF)
    if (setup_confname_table(posix_constants_pathconf,
                             sizeof(posix_constants_pathconf)
                               / sizeof(struct constdef),
                             "pathconf_names", module))
        return -1;
#endif
#ifdef HAVE_CONFSTR
    if (setup_confname_table(posix_constants_confstr,
                             sizeof(posix_constants_confstr)
                               / sizeof(struct constdef),
                             "confstr_names", module))
        return -1;
#endif
    return 0;
}


/*[clinic input]
os.abort

Abort the interpreter immediately.

This function 'dumps core' or otherwise fails in the hardest way possible
on the hosting operating system.  This function never returns.
[clinic start generated code]*/

static PyObject *
os_abort_impl(PyObject *module)
/*[clinic end generated code: output=dcf52586dad2467c input=cf2c7d98bc504047]*/
{
    abort();
    /*NOTREACHED*/
#ifndef __clang__
    /* Issue #28152: abort() is declared with __attribute__((__noreturn__)).
       GCC emits a warning without "return NULL;" (compiler bug?), but Clang
       is smarter and emits a warning on the return. */
    Py_FatalError("abort() called from Python code didn't abort!");
    return NULL;
#endif
}

/*[clinic input]
os.device_encoding
    fd: int

Return a string describing the encoding of a terminal's file descriptor.

The file descriptor must be attached to a terminal.
If the device is not a terminal, return None.
[clinic start generated code]*/

static PyObject *
os_device_encoding_impl(PyObject *module, int fd)
/*[clinic end generated code: output=e0d294bbab7e8c2b input=9e1d4a42b66df312]*/
{
    return _Py_device_encoding(fd);
}

/*[clinic input]
os.get_inheritable -> bool

    fd: int
    /

Get the close-on-exe flag of the specified file descriptor.
[clinic start generated code]*/

static int
os_get_inheritable_impl(PyObject *module, int fd)
/*[clinic end generated code: output=0445e20e149aa5b8 input=89ac008dc9ab6b95]*/
{
    int return_value;
    
    return_value = _Py_get_inheritable(fd);
    
    return return_value;
}


/*[clinic input]
os.set_inheritable
    fd: int
    inheritable: int
    /

Set the inheritable flag of the specified file descriptor.
[clinic start generated code]*/

static PyObject *
os_set_inheritable_impl(PyObject *module, int fd, int inheritable)
/*[clinic end generated code: output=f1b1918a2f3c38c2 input=9ceaead87a1e2402]*/
{
    int result;

    
    result = _Py_set_inheritable(fd, inheritable, NULL);
    
    if (result < 0)
        return NULL;
    Py_RETURN_NONE;
}


#ifdef MS_WINDOWS
/*[clinic input]
os.get_handle_inheritable -> bool
    handle: intptr_t
    /

Get the close-on-exe flag of the specified file descriptor.
[clinic start generated code]*/

static int
os_get_handle_inheritable_impl(PyObject *module, intptr_t handle)
/*[clinic end generated code: output=36be5afca6ea84d8 input=cfe99f9c05c70ad1]*/
{
    DWORD flags;

    if (!GetHandleInformation((HANDLE)handle, &flags)) {
        PyErr_SetFromWindowsErr(0);
        return -1;
    }

    return flags & HANDLE_FLAG_INHERIT;
}


/*[clinic input]
os.set_handle_inheritable
    handle: intptr_t
    inheritable: bool
    /

Set the inheritable flag of the specified handle.
[clinic start generated code]*/

static PyObject *
os_set_handle_inheritable_impl(PyObject *module, intptr_t handle,
                               int inheritable)
/*[clinic end generated code: output=021d74fe6c96baa3 input=7a7641390d8364fc]*/
{
    DWORD flags = inheritable ? HANDLE_FLAG_INHERIT : 0;
    if (!SetHandleInformation((HANDLE)handle, HANDLE_FLAG_INHERIT, flags)) {
        PyErr_SetFromWindowsErr(0);
        return NULL;
    }
    Py_RETURN_NONE;
}
#endif /* MS_WINDOWS */

#ifndef MS_WINDOWS
/*[clinic input]
os.get_blocking -> bool
    fd: int
    /

Get the blocking mode of the file descriptor.

Return False if the O_NONBLOCK flag is set, True if the flag is cleared.
[clinic start generated code]*/

static int
os_get_blocking_impl(PyObject *module, int fd)
/*[clinic end generated code: output=336a12ad76a61482 input=f4afb59d51560179]*/
{
    int blocking;

    
    blocking = _Py_get_blocking(fd);
    
    return blocking;
}

/*[clinic input]
os.set_blocking
    fd: int
    blocking: bool(accept={int})
    /

Set the blocking mode of the specified file descriptor.

Set the O_NONBLOCK flag if blocking is False,
clear the O_NONBLOCK flag otherwise.
[clinic start generated code]*/

static PyObject *
os_set_blocking_impl(PyObject *module, int fd, int blocking)
/*[clinic end generated code: output=384eb43aa0762a9d input=bf5c8efdc5860ff3]*/
{
    int result;

    
    result = _Py_set_blocking(fd, blocking);
    
    if (result < 0)
        return NULL;
    Py_RETURN_NONE;
}
#endif   /* !MS_WINDOWS */


/*[clinic input]
class os.DirEntry "DirEntry *" "DirEntryType"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=3c18c7a448247980]*/

typedef struct {
    PyObject_HEAD
    PyObject *name;
    PyObject *path;
    PyObject *stat;
    PyObject *lstat;
#ifdef MS_WINDOWS
    struct _Py_stat_struct win32_lstat;
    uint64_t win32_file_index;
    int got_file_index;
#else /* POSIX */
#ifdef HAVE_DIRENT_D_TYPE
    unsigned char d_type;
#endif
    ino_t d_ino;
    int dir_fd;
#endif
} DirEntry;

static PyObject *
_disabled_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyErr_Format(PyExc_TypeError,
        "cannot create '%.100s' instances", _PyType_Name(type));
    return NULL;
}

static void
DirEntry_dealloc(DirEntry *entry)
{
    PyTypeObject *tp = Py_TYPE(entry);
    Py_XDECREF(entry->name);
    Py_XDECREF(entry->path);
    Py_XDECREF(entry->stat);
    Py_XDECREF(entry->lstat);
    freefunc free_func = PyType_GetSlot(tp, Py_tp_free);
    free_func(entry);
    Py_DECREF(tp);
}

/* Forward reference */
static int
DirEntry_test_mode(PyTypeObject *defining_class, DirEntry *self,
                   int follow_symlinks, unsigned short mode_bits);

/*[clinic input]
os.DirEntry.is_symlink -> bool
    defining_class: defining_class
    /

Return True if the entry is a symbolic link; cached per entry.
[clinic start generated code]*/

static int
os_DirEntry_is_symlink_impl(DirEntry *self, PyTypeObject *defining_class)
/*[clinic end generated code: output=293096d589b6d47c input=e9acc5ee4d511113]*/
{
#ifdef MS_WINDOWS
    return (self->win32_lstat.st_mode & S_IFMT) == S_IFLNK;
#elif defined(HAVE_DIRENT_D_TYPE)
    /* POSIX */
    if (self->d_type != DT_UNKNOWN)
        return self->d_type == DT_LNK;
    else
        return DirEntry_test_mode(defining_class, self, 0, S_IFLNK);
#else
    /* POSIX without d_type */
    return DirEntry_test_mode(defining_class, self, 0, S_IFLNK);
#endif
}

static PyObject *
DirEntry_fetch_stat(PyObject *module, DirEntry *self, int follow_symlinks)
{
    int result;
    STRUCT_STAT st;
    PyObject *ub;

#ifdef MS_WINDOWS
    if (!PyUnicode_FSDecoder(self->path, &ub))
        return NULL;
    const wchar_t *path = PyUnicode_AsUnicode(ub);
#else /* POSIX */
    if (!PyUnicode_FSConverter(self->path, &ub))
        return NULL;
    const char *path = PyBytes_AS_STRING(ub);
    if (self->dir_fd != DEFAULT_DIR_FD) {
#ifdef HAVE_FSTATAT
        result = fstatat(self->dir_fd, path, &st,
                         follow_symlinks ? 0 : AT_SYMLINK_NOFOLLOW);
#else
        PyErr_SetString(PyExc_NotImplementedError, "can't fetch stat");
        return NULL;
#endif /* HAVE_FSTATAT */
    }
    else
#endif
    {
        if (follow_symlinks)
            result = STAT(path, &st);
        else
            result = LSTAT(path, &st);
    }
    Py_DECREF(ub);

    if (result != 0)
        return path_object_error(self->path);

    return _pystat_fromstructstat(module, &st);
}

static PyObject *
DirEntry_get_lstat(PyTypeObject *defining_class, DirEntry *self)
{
    if (!self->lstat) {
        PyObject *module = PyType_GetModule(defining_class);
#ifdef MS_WINDOWS
        self->lstat = _pystat_fromstructstat(module, &self->win32_lstat);
#else /* POSIX */
        self->lstat = DirEntry_fetch_stat(module, self, 0);
#endif
    }
    Py_XINCREF(self->lstat);
    return self->lstat;
}

/*[clinic input]
os.DirEntry.stat
    defining_class: defining_class
    /
    *
    follow_symlinks: bool = True

Return stat_result object for the entry; cached per entry.
[clinic start generated code]*/

static PyObject *
os_DirEntry_stat_impl(DirEntry *self, PyTypeObject *defining_class,
                      int follow_symlinks)
/*[clinic end generated code: output=23f803e19c3e780e input=e816273c4e67ee98]*/
{
    if (!follow_symlinks) {
        return DirEntry_get_lstat(defining_class, self);
    }

    if (!self->stat) {
        int result = os_DirEntry_is_symlink_impl(self, defining_class);
        if (result == -1) {
            return NULL;
        }
        if (result) {
            PyObject *module = PyType_GetModule(defining_class);
            self->stat = DirEntry_fetch_stat(module, self, 1);
        }
        else {
            self->stat = DirEntry_get_lstat(defining_class, self);
        }
    }

    Py_XINCREF(self->stat);
    return self->stat;
}

/* Set exception and return -1 on error, 0 for False, 1 for True */
static int
DirEntry_test_mode(PyTypeObject *defining_class, DirEntry *self,
                   int follow_symlinks, unsigned short mode_bits)
{
    PyObject *stat = NULL;
    PyObject *st_mode = NULL;
    long mode;
    int result;
#if defined(MS_WINDOWS) || defined(HAVE_DIRENT_D_TYPE)
    int is_symlink;
    int need_stat;
#endif
#ifdef MS_WINDOWS
    unsigned long dir_bits;
#endif

#ifdef MS_WINDOWS
    is_symlink = (self->win32_lstat.st_mode & S_IFMT) == S_IFLNK;
    need_stat = follow_symlinks && is_symlink;
#elif defined(HAVE_DIRENT_D_TYPE)
    is_symlink = self->d_type == DT_LNK;
    need_stat = self->d_type == DT_UNKNOWN || (follow_symlinks && is_symlink);
#endif

#if defined(MS_WINDOWS) || defined(HAVE_DIRENT_D_TYPE)
    if (need_stat) {
#endif
        stat = os_DirEntry_stat_impl(self, defining_class, follow_symlinks);
        if (!stat) {
            if (PyErr_ExceptionMatches(PyExc_FileNotFoundError)) {
                /* If file doesn't exist (anymore), then return False
                   (i.e., say it's not a file/directory) */
                PyErr_Clear();
                return 0;
            }
            goto error;
        }
        _posixstate* state = get_posix_state(PyType_GetModule(defining_class));
        st_mode = PyObject_GetAttr(stat, state->st_mode);
        if (!st_mode)
            goto error;

        mode = PyLong_AsLong(st_mode);
        if (mode == -1 && PyErr_Occurred())
            goto error;
        Py_CLEAR(st_mode);
        Py_CLEAR(stat);
        result = (mode & S_IFMT) == mode_bits;
#if defined(MS_WINDOWS) || defined(HAVE_DIRENT_D_TYPE)
    }
    else if (is_symlink) {
        assert(mode_bits != S_IFLNK);
        result = 0;
    }
    else {
        assert(mode_bits == S_IFDIR || mode_bits == S_IFREG);
#ifdef MS_WINDOWS
        dir_bits = self->win32_lstat.st_file_attributes & FILE_ATTRIBUTE_DIRECTORY;
        if (mode_bits == S_IFDIR)
            result = dir_bits != 0;
        else
            result = dir_bits == 0;
#else /* POSIX */
        if (mode_bits == S_IFDIR)
            result = self->d_type == DT_DIR;
        else
            result = self->d_type == DT_REG;
#endif
    }
#endif

    return result;

error:
    Py_XDECREF(st_mode);
    Py_XDECREF(stat);
    return -1;
}

/*[clinic input]
os.DirEntry.is_dir -> bool
    defining_class: defining_class
    /
    *
    follow_symlinks: bool = True

Return True if the entry is a directory; cached per entry.
[clinic start generated code]*/

static int
os_DirEntry_is_dir_impl(DirEntry *self, PyTypeObject *defining_class,
                        int follow_symlinks)
/*[clinic end generated code: output=0cd453b9c0987fdf input=1a4ffd6dec9920cb]*/
{
    return DirEntry_test_mode(defining_class, self, follow_symlinks, S_IFDIR);
}

/*[clinic input]
os.DirEntry.is_file -> bool
    defining_class: defining_class
    /
    *
    follow_symlinks: bool = True

Return True if the entry is a file; cached per entry.
[clinic start generated code]*/

static int
os_DirEntry_is_file_impl(DirEntry *self, PyTypeObject *defining_class,
                         int follow_symlinks)
/*[clinic end generated code: output=f7c277ab5ba80908 input=0a64c5a12e802e3b]*/
{
    return DirEntry_test_mode(defining_class, self, follow_symlinks, S_IFREG);
}

/*[clinic input]
os.DirEntry.inode

Return inode of the entry; cached per entry.
[clinic start generated code]*/

static PyObject *
os_DirEntry_inode_impl(DirEntry *self)
/*[clinic end generated code: output=156bb3a72162440e input=3ee7b872ae8649f0]*/
{
#ifdef MS_WINDOWS
    if (!self->got_file_index) {
        PyObject *unicode;
        const wchar_t *path;
        STRUCT_STAT stat;
        int result;

        if (!PyUnicode_FSDecoder(self->path, &unicode))
            return NULL;
        path = PyUnicode_AsUnicode(unicode);
        result = LSTAT(path, &stat);
        Py_DECREF(unicode);

        if (result != 0)
            return path_object_error(self->path);

        self->win32_file_index = stat.st_ino;
        self->got_file_index = 1;
    }
    Py_BUILD_ASSERT(sizeof(unsigned long long) >= sizeof(self->win32_file_index));
    return PyLong_FromUnsignedLongLong(self->win32_file_index);
#else /* POSIX */
    Py_BUILD_ASSERT(sizeof(unsigned long long) >= sizeof(self->d_ino));
    return PyLong_FromUnsignedLongLong(self->d_ino);
#endif
}

static PyObject *
DirEntry_repr(DirEntry *self)
{
    return PyUnicode_FromFormat("<DirEntry %R>", self->name);
}

/*[clinic input]
os.DirEntry.__fspath__

Returns the path for the entry.
[clinic start generated code]*/

static PyObject *
os_DirEntry___fspath___impl(DirEntry *self)
/*[clinic end generated code: output=6dd7f7ef752e6f4f input=3c49d0cf38df4fac]*/
{
    Py_INCREF(self->path);
    return self->path;
}

static PyMemberDef DirEntry_members[] = {
    {"name", T_OBJECT_EX, offsetof(DirEntry, name), READONLY,
     "the entry's base filename, relative to scandir() \"path\" argument"},
    {"path", T_OBJECT_EX, offsetof(DirEntry, path), READONLY,
     "the entry's full path name; equivalent to os.path.join(scandir_path, entry.name)"},
    {NULL}
};

/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(os_stat__doc__,
"stat($module, /, path, *, dir_fd=None, follow_symlinks=True)\n"
"--\n"
"\n"
"Perform a stat system call on the given path.\n"
"\n"
"  path\n"
"    Path to be examined; can be string, bytes, a path-like object or\n"
"    open-file-descriptor int.\n"
"  dir_fd\n"
"    If not None, it should be a file descriptor open to a directory,\n"
"    and path should be a relative string; path will then be relative to\n"
"    that directory.\n"
"  follow_symlinks\n"
"    If False, and the last element of the path is a symbolic link,\n"
"    stat will examine the symbolic link itself instead of the file\n"
"    the link points to.\n"
"\n"
"dir_fd and follow_symlinks may not be implemented\n"
"  on your platform.  If they are unavailable, using them will raise a\n"
"  NotImplementedError.\n"
"\n"
"It\'s an error to use dir_fd or follow_symlinks when specifying path as\n"
"  an open file descriptor.");

#define OS_STAT_METHODDEF    \
    {"stat", (PyCFunction)(void(*)(void))os_stat, METH_FASTCALL|METH_KEYWORDS, os_stat__doc__},

static PyObject *
os_stat_impl(PyObject *module, path_t *path, int dir_fd, int follow_symlinks);

static PyObject *
os_stat(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", "dir_fd", "follow_symlinks", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "stat", 0};
    PyObject *argsbuf[3];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    path_t path = PATH_T_INITIALIZE("stat", "path", 0, 1);
    int dir_fd = DEFAULT_DIR_FD;
    int follow_symlinks = 1;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (args[1]) {
        if (!FSTATAT_DIR_FD_CONVERTER(args[1], &dir_fd)) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    follow_symlinks = PyObject_IsTrue(args[2]);
    if (follow_symlinks < 0) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_stat_impl(module, &path, dir_fd, follow_symlinks);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

PyDoc_STRVAR(os_lstat__doc__,
"lstat($module, /, path, *, dir_fd=None)\n"
"--\n"
"\n"
"Perform a stat system call on the given path, without following symbolic links.\n"
"\n"
"Like stat(), but do not follow symbolic links.\n"
"Equivalent to stat(path, follow_symlinks=False).");

#define OS_LSTAT_METHODDEF    \
    {"lstat", (PyCFunction)(void(*)(void))os_lstat, METH_FASTCALL|METH_KEYWORDS, os_lstat__doc__},

static PyObject *
os_lstat_impl(PyObject *module, path_t *path, int dir_fd);

static PyObject *
os_lstat(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", "dir_fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "lstat", 0};
    PyObject *argsbuf[2];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    path_t path = PATH_T_INITIALIZE("lstat", "path", 0, 0);
    int dir_fd = DEFAULT_DIR_FD;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (!FSTATAT_DIR_FD_CONVERTER(args[1], &dir_fd)) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_lstat_impl(module, &path, dir_fd);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

PyDoc_STRVAR(os_access__doc__,
"access($module, /, path, mode, *, dir_fd=None, effective_ids=False,\n"
"       follow_symlinks=True)\n"
"--\n"
"\n"
"Use the real uid/gid to test for access to a path.\n"
"\n"
"  path\n"
"    Path to be tested; can be string, bytes, or a path-like object.\n"
"  mode\n"
"    Operating-system mode bitfield.  Can be F_OK to test existence,\n"
"    or the inclusive-OR of R_OK, W_OK, and X_OK.\n"
"  dir_fd\n"
"    If not None, it should be a file descriptor open to a directory,\n"
"    and path should be relative; path will then be relative to that\n"
"    directory.\n"
"  effective_ids\n"
"    If True, access will use the effective uid/gid instead of\n"
"    the real uid/gid.\n"
"  follow_symlinks\n"
"    If False, and the last element of the path is a symbolic link,\n"
"    access will examine the symbolic link itself instead of the file\n"
"    the link points to.\n"
"\n"
"dir_fd, effective_ids, and follow_symlinks may not be implemented\n"
"  on your platform.  If they are unavailable, using them will raise a\n"
"  NotImplementedError.\n"
"\n"
"Note that most operations will use the effective uid/gid, therefore this\n"
"  routine can be used in a suid/sgid environment to test if the invoking user\n"
"  has the specified access to the path.");

#define OS_ACCESS_METHODDEF    \
    {"access", (PyCFunction)(void(*)(void))os_access, METH_FASTCALL|METH_KEYWORDS, os_access__doc__},

static int
os_access_impl(PyObject *module, path_t *path, int mode, int dir_fd,
               int effective_ids, int follow_symlinks);

static PyObject *
os_access(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", "mode", "dir_fd", "effective_ids", "follow_symlinks", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "access", 0};
    PyObject *argsbuf[5];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 2;
    path_t path = PATH_T_INITIALIZE("access", "path", 0, 0);
    int mode;
    int dir_fd = DEFAULT_DIR_FD;
    int effective_ids = 0;
    int follow_symlinks = 1;
    int _return_value;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 2, 2, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    mode = _PyLong_AsInt(args[1]);
    if (mode == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (args[2]) {
        if (!FACCESSAT_DIR_FD_CONVERTER(args[2], &dir_fd)) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    if (args[3]) {
        effective_ids = PyObject_IsTrue(args[3]);
        if (effective_ids < 0) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    follow_symlinks = PyObject_IsTrue(args[4]);
    if (follow_symlinks < 0) {
        goto exit;
    }
skip_optional_kwonly:
    _return_value = os_access_impl(module, &path, mode, dir_fd, effective_ids, follow_symlinks);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

PyDoc_STRVAR(os_chdir__doc__,
"chdir($module, /, path)\n"
"--\n"
"\n"
"Change the current working directory to the specified path.\n"
"\n"
"path may always be specified as a string.\n"
"On some platforms, path may also be specified as an open file descriptor.\n"
"  If this functionality is unavailable, using it raises an exception.");

#define OS_CHDIR_METHODDEF    \
    {"chdir", (PyCFunction)(void(*)(void))os_chdir, METH_FASTCALL|METH_KEYWORDS, os_chdir__doc__},

static PyObject *
os_chdir_impl(PyObject *module, path_t *path);

static PyObject *
os_chdir(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "chdir", 0};
    PyObject *argsbuf[1];
    path_t path = PATH_T_INITIALIZE("chdir", "path", 0, PATH_HAVE_FCHDIR);

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    return_value = os_chdir_impl(module, &path);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

#if defined(HAVE_FCHDIR)

PyDoc_STRVAR(os_fchdir__doc__,
"fchdir($module, /, fd)\n"
"--\n"
"\n"
"Change to the directory of the given file descriptor.\n"
"\n"
"fd must be opened on a directory, not a file.\n"
"Equivalent to os.chdir(fd).");

#define OS_FCHDIR_METHODDEF    \
    {"fchdir", (PyCFunction)(void(*)(void))os_fchdir, METH_FASTCALL|METH_KEYWORDS, os_fchdir__doc__},

static PyObject *
os_fchdir_impl(PyObject *module, int fd);

static PyObject *
os_fchdir(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "fchdir", 0};
    PyObject *argsbuf[1];
    int fd;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!fildes_converter(args[0], &fd)) {
        goto exit;
    }
    return_value = os_fchdir_impl(module, fd);

exit:
    return return_value;
}

#endif /* defined(HAVE_FCHDIR) */

PyDoc_STRVAR(os_getcwd__doc__,
"getcwd($module, /)\n"
"--\n"
"\n"
"Return a unicode string representing the current working directory.");

#define OS_GETCWD_METHODDEF    \
    {"getcwd", (PyCFunction)os_getcwd, METH_NOARGS, os_getcwd__doc__},

static PyObject *
os_getcwd_impl(PyObject *module);

static PyObject *
os_getcwd(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return os_getcwd_impl(module);
}

PyDoc_STRVAR(os_getcwdb__doc__,
"getcwdb($module, /)\n"
"--\n"
"\n"
"Return a bytes string representing the current working directory.");

#define OS_GETCWDB_METHODDEF    \
    {"getcwdb", (PyCFunction)os_getcwdb, METH_NOARGS, os_getcwdb__doc__},

static PyObject *
os_getcwdb_impl(PyObject *module);

static PyObject *
os_getcwdb(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return os_getcwdb_impl(module);
}

#if defined(HAVE_LINK)

PyDoc_STRVAR(os_link__doc__,
"link($module, /, src, dst, *, src_dir_fd=None, dst_dir_fd=None,\n"
"     follow_symlinks=True)\n"
"--\n"
"\n"
"Create a hard link to a file.\n"
"\n"
"If either src_dir_fd or dst_dir_fd is not None, it should be a file\n"
"  descriptor open to a directory, and the respective path string (src or dst)\n"
"  should be relative; the path will then be relative to that directory.\n"
"If follow_symlinks is False, and the last element of src is a symbolic\n"
"  link, link will create a link to the symbolic link itself instead of the\n"
"  file the link points to.\n"
"src_dir_fd, dst_dir_fd, and follow_symlinks may not be implemented on your\n"
"  platform.  If they are unavailable, using them will raise a\n"
"  NotImplementedError.");

#define OS_LINK_METHODDEF    \
    {"link", (PyCFunction)(void(*)(void))os_link, METH_FASTCALL|METH_KEYWORDS, os_link__doc__},

static PyObject *
os_link_impl(PyObject *module, path_t *src, path_t *dst, int src_dir_fd,
             int dst_dir_fd, int follow_symlinks);

static PyObject *
os_link(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"src", "dst", "src_dir_fd", "dst_dir_fd", "follow_symlinks", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "link", 0};
    PyObject *argsbuf[5];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 2;
    path_t src = PATH_T_INITIALIZE("link", "src", 0, 0);
    path_t dst = PATH_T_INITIALIZE("link", "dst", 0, 0);
    int src_dir_fd = DEFAULT_DIR_FD;
    int dst_dir_fd = DEFAULT_DIR_FD;
    int follow_symlinks = 1;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 2, 2, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &src)) {
        goto exit;
    }
    if (!path_converter(args[1], &dst)) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (args[2]) {
        if (!dir_fd_converter(args[2], &src_dir_fd)) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    if (args[3]) {
        if (!dir_fd_converter(args[3], &dst_dir_fd)) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    follow_symlinks = PyObject_IsTrue(args[4]);
    if (follow_symlinks < 0) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_link_impl(module, &src, &dst, src_dir_fd, dst_dir_fd, follow_symlinks);

exit:
    /* Cleanup for src */
    path_cleanup(&src);
    /* Cleanup for dst */
    path_cleanup(&dst);

    return return_value;
}

#endif /* defined(HAVE_LINK) */

PyDoc_STRVAR(os_listdir__doc__,
"listdir($module, /, path=None)\n"
"--\n"
"\n"
"Return a list containing the names of the files in the directory.\n"
"\n"
"path can be specified as either str, bytes, or a path-like object.  If path is bytes,\n"
"  the filenames returned will also be bytes; in all other circumstances\n"
"  the filenames returned will be str.\n"
"If path is None, uses the path=\'.\'.\n"
"On some platforms, path may also be specified as an open file descriptor;\\\n"
"  the file descriptor must refer to a directory.\n"
"  If this functionality is unavailable, using it raises NotImplementedError.\n"
"\n"
"The list is in arbitrary order.  It does not include the special\n"
"entries \'.\' and \'..\' even if they are present in the directory.");

#define OS_LISTDIR_METHODDEF    \
    {"listdir", (PyCFunction)(void(*)(void))os_listdir, METH_FASTCALL|METH_KEYWORDS, os_listdir__doc__},

static PyObject *
os_listdir_impl(PyObject *module, path_t *path);

static PyObject *
os_listdir(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "listdir", 0};
    PyObject *argsbuf[1];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 0;
    path_t path = PATH_T_INITIALIZE("listdir", "path", 1, PATH_HAVE_FDOPENDIR);

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 0, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
skip_optional_pos:
    return_value = os_listdir_impl(module, &path);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

PyDoc_STRVAR(os_mkdir__doc__,
"mkdir($module, /, path, mode=511, *, dir_fd=None)\n"
"--\n"
"\n"
"Create a directory.\n"
"\n"
"If dir_fd is not None, it should be a file descriptor open to a directory,\n"
"  and path should be relative; path will then be relative to that directory.\n"
"dir_fd may not be implemented on your platform.\n"
"  If it is unavailable, using it will raise a NotImplementedError.\n"
"\n"
"The mode argument is ignored on Windows.");

#define OS_MKDIR_METHODDEF    \
    {"mkdir", (PyCFunction)(void(*)(void))os_mkdir, METH_FASTCALL|METH_KEYWORDS, os_mkdir__doc__},

static PyObject *
os_mkdir_impl(PyObject *module, path_t *path, int mode, int dir_fd);

static PyObject *
os_mkdir(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", "mode", "dir_fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "mkdir", 0};
    PyObject *argsbuf[3];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    path_t path = PATH_T_INITIALIZE("mkdir", "path", 0, 0);
    int mode = 511;
    int dir_fd = DEFAULT_DIR_FD;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 2, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[1]) {
        mode = _PyLong_AsInt(args[1]);
        if (mode == -1 && PyErr_Occurred()) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
skip_optional_pos:
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (!MKDIRAT_DIR_FD_CONVERTER(args[2], &dir_fd)) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_mkdir_impl(module, &path, mode, dir_fd);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

PyDoc_STRVAR(os_rename__doc__,
"rename($module, /, src, dst, *, src_dir_fd=None, dst_dir_fd=None)\n"
"--\n"
"\n"
"Rename a file or directory.\n"
"\n"
"If either src_dir_fd or dst_dir_fd is not None, it should be a file\n"
"  descriptor open to a directory, and the respective path string (src or dst)\n"
"  should be relative; the path will then be relative to that directory.\n"
"src_dir_fd and dst_dir_fd, may not be implemented on your platform.\n"
"  If they are unavailable, using them will raise a NotImplementedError.");

#define OS_RENAME_METHODDEF    \
    {"rename", (PyCFunction)(void(*)(void))os_rename, METH_FASTCALL|METH_KEYWORDS, os_rename__doc__},

static PyObject *
os_rename_impl(PyObject *module, path_t *src, path_t *dst, int src_dir_fd,
               int dst_dir_fd);

static PyObject *
os_rename(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"src", "dst", "src_dir_fd", "dst_dir_fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "rename", 0};
    PyObject *argsbuf[4];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 2;
    path_t src = PATH_T_INITIALIZE("rename", "src", 0, 0);
    path_t dst = PATH_T_INITIALIZE("rename", "dst", 0, 0);
    int src_dir_fd = DEFAULT_DIR_FD;
    int dst_dir_fd = DEFAULT_DIR_FD;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 2, 2, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &src)) {
        goto exit;
    }
    if (!path_converter(args[1], &dst)) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (args[2]) {
        if (!dir_fd_converter(args[2], &src_dir_fd)) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    if (!dir_fd_converter(args[3], &dst_dir_fd)) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_rename_impl(module, &src, &dst, src_dir_fd, dst_dir_fd);

exit:
    /* Cleanup for src */
    path_cleanup(&src);
    /* Cleanup for dst */
    path_cleanup(&dst);

    return return_value;
}

PyDoc_STRVAR(os_replace__doc__,
"replace($module, /, src, dst, *, src_dir_fd=None, dst_dir_fd=None)\n"
"--\n"
"\n"
"Rename a file or directory, overwriting the destination.\n"
"\n"
"If either src_dir_fd or dst_dir_fd is not None, it should be a file\n"
"  descriptor open to a directory, and the respective path string (src or dst)\n"
"  should be relative; the path will then be relative to that directory.\n"
"src_dir_fd and dst_dir_fd, may not be implemented on your platform.\n"
"  If they are unavailable, using them will raise a NotImplementedError.");

#define OS_REPLACE_METHODDEF    \
    {"replace", (PyCFunction)(void(*)(void))os_replace, METH_FASTCALL|METH_KEYWORDS, os_replace__doc__},

static PyObject *
os_replace_impl(PyObject *module, path_t *src, path_t *dst, int src_dir_fd,
                int dst_dir_fd);

static PyObject *
os_replace(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"src", "dst", "src_dir_fd", "dst_dir_fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "replace", 0};
    PyObject *argsbuf[4];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 2;
    path_t src = PATH_T_INITIALIZE("replace", "src", 0, 0);
    path_t dst = PATH_T_INITIALIZE("replace", "dst", 0, 0);
    int src_dir_fd = DEFAULT_DIR_FD;
    int dst_dir_fd = DEFAULT_DIR_FD;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 2, 2, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &src)) {
        goto exit;
    }
    if (!path_converter(args[1], &dst)) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (args[2]) {
        if (!dir_fd_converter(args[2], &src_dir_fd)) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    if (!dir_fd_converter(args[3], &dst_dir_fd)) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_replace_impl(module, &src, &dst, src_dir_fd, dst_dir_fd);

exit:
    /* Cleanup for src */
    path_cleanup(&src);
    /* Cleanup for dst */
    path_cleanup(&dst);

    return return_value;
}

PyDoc_STRVAR(os_rmdir__doc__,
"rmdir($module, /, path, *, dir_fd=None)\n"
"--\n"
"\n"
"Remove a directory.\n"
"\n"
"If dir_fd is not None, it should be a file descriptor open to a directory,\n"
"  and path should be relative; path will then be relative to that directory.\n"
"dir_fd may not be implemented on your platform.\n"
"  If it is unavailable, using it will raise a NotImplementedError.");

#define OS_RMDIR_METHODDEF    \
    {"rmdir", (PyCFunction)(void(*)(void))os_rmdir, METH_FASTCALL|METH_KEYWORDS, os_rmdir__doc__},

static PyObject *
os_rmdir_impl(PyObject *module, path_t *path, int dir_fd);

static PyObject *
os_rmdir(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", "dir_fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "rmdir", 0};
    PyObject *argsbuf[2];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    path_t path = PATH_T_INITIALIZE("rmdir", "path", 0, 0);
    int dir_fd = DEFAULT_DIR_FD;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (!UNLINKAT_DIR_FD_CONVERTER(args[1], &dir_fd)) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_rmdir_impl(module, &path, dir_fd);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

#if defined(HAVE_SYSTEM) && defined(MS_WINDOWS)

PyDoc_STRVAR(os_system__doc__,
"system($module, /, command)\n"
"--\n"
"\n"
"Execute the command in a subshell.");

#define OS_SYSTEM_METHODDEF    \
    {"system", (PyCFunction)(void(*)(void))os_system, METH_FASTCALL|METH_KEYWORDS, os_system__doc__},

static long
os_system_impl(PyObject *module, const Py_UNICODE *command);

static PyObject *
os_system(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"command", NULL};
    static _PyArg_Parser _parser = {"u:system", _keywords, 0};
    const Py_UNICODE *command;
    long _return_value;

    if (!_PyArg_ParseStackAndKeywords(args, nargs, kwnames, &_parser,
        &command)) {
        goto exit;
    }
    _return_value = os_system_impl(module, command);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyLong_FromLong(_return_value);

exit:
    return return_value;
}

#endif /* defined(HAVE_SYSTEM) && defined(MS_WINDOWS) */

#if defined(HAVE_SYSTEM) && !defined(MS_WINDOWS)

PyDoc_STRVAR(os_system__doc__,
"system($module, /, command)\n"
"--\n"
"\n"
"Execute the command in a subshell.");

#define OS_SYSTEM_METHODDEF    \
    {"system", (PyCFunction)(void(*)(void))os_system, METH_FASTCALL|METH_KEYWORDS, os_system__doc__},

static long
os_system_impl(PyObject *module, PyObject *command);

static PyObject *
os_system(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"command", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "system", 0};
    PyObject *argsbuf[1];
    PyObject *command = NULL;
    long _return_value;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!PyUnicode_FSConverter(args[0], &command)) {
        goto exit;
    }
    _return_value = os_system_impl(module, command);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyLong_FromLong(_return_value);

exit:
    /* Cleanup for command */
    Py_XDECREF(command);

    return return_value;
}

#endif /* defined(HAVE_SYSTEM) && !defined(MS_WINDOWS) */

PyDoc_STRVAR(os_unlink__doc__,
"unlink($module, /, path, *, dir_fd=None)\n"
"--\n"
"\n"
"Remove a file (same as remove()).\n"
"\n"
"If dir_fd is not None, it should be a file descriptor open to a directory,\n"
"  and path should be relative; path will then be relative to that directory.\n"
"dir_fd may not be implemented on your platform.\n"
"  If it is unavailable, using it will raise a NotImplementedError.");

#define OS_UNLINK_METHODDEF    \
    {"unlink", (PyCFunction)(void(*)(void))os_unlink, METH_FASTCALL|METH_KEYWORDS, os_unlink__doc__},

static PyObject *
os_unlink_impl(PyObject *module, path_t *path, int dir_fd);

static PyObject *
os_unlink(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", "dir_fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "unlink", 0};
    PyObject *argsbuf[2];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    path_t path = PATH_T_INITIALIZE("unlink", "path", 0, 0);
    int dir_fd = DEFAULT_DIR_FD;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (!UNLINKAT_DIR_FD_CONVERTER(args[1], &dir_fd)) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_unlink_impl(module, &path, dir_fd);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

PyDoc_STRVAR(os_remove__doc__,
"remove($module, /, path, *, dir_fd=None)\n"
"--\n"
"\n"
"Remove a file (same as unlink()).\n"
"\n"
"If dir_fd is not None, it should be a file descriptor open to a directory,\n"
"  and path should be relative; path will then be relative to that directory.\n"
"dir_fd may not be implemented on your platform.\n"
"  If it is unavailable, using it will raise a NotImplementedError.");

#define OS_REMOVE_METHODDEF    \
    {"remove", (PyCFunction)(void(*)(void))os_remove, METH_FASTCALL|METH_KEYWORDS, os_remove__doc__},

static PyObject *
os_remove_impl(PyObject *module, path_t *path, int dir_fd);

static PyObject *
os_remove(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", "dir_fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "remove", 0};
    PyObject *argsbuf[2];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    path_t path = PATH_T_INITIALIZE("remove", "path", 0, 0);
    int dir_fd = DEFAULT_DIR_FD;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (!UNLINKAT_DIR_FD_CONVERTER(args[1], &dir_fd)) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_remove_impl(module, &path, dir_fd);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

PyDoc_STRVAR(os__exit__doc__,
"_exit($module, /, status)\n"
"--\n"
"\n"
"Exit to the system with specified status, without normal exit processing.");

#define OS__EXIT_METHODDEF    \
    {"_exit", (PyCFunction)(void(*)(void))os__exit, METH_FASTCALL|METH_KEYWORDS, os__exit__doc__},

static PyObject *
os__exit_impl(PyObject *module, int status);

static PyObject *
os__exit(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"status", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "_exit", 0};
    PyObject *argsbuf[1];
    int status;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    status = _PyLong_AsInt(args[0]);
    if (status == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = os__exit_impl(module, status);

exit:
    return return_value;
}

#if defined(HAVE_PLOCK)

PyDoc_STRVAR(os_plock__doc__,
"plock($module, op, /)\n"
"--\n"
"\n"
"Lock program segments into memory.\");");

#define OS_PLOCK_METHODDEF    \
    {"plock", (PyCFunction)os_plock, METH_O, os_plock__doc__},

static PyObject *
os_plock_impl(PyObject *module, int op);

static PyObject *
os_plock(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int op;

    op = _PyLong_AsInt(arg);
    if (op == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = os_plock_impl(module, op);

exit:
    return return_value;
}

#endif /* defined(HAVE_PLOCK) */


#if (defined(HAVE_READLINK) || defined(MS_WINDOWS))

PyDoc_STRVAR(os_readlink__doc__,
"readlink($module, /, path, *, dir_fd=None)\n"
"--\n"
"\n"
"Return a string representing the path to which the symbolic link points.\n"
"\n"
"If dir_fd is not None, it should be a file descriptor open to a directory,\n"
"and path should be relative; path will then be relative to that directory.\n"
"\n"
"dir_fd may not be implemented on your platform.  If it is unavailable,\n"
"using it will raise a NotImplementedError.");

#define OS_READLINK_METHODDEF    \
    {"readlink", (PyCFunction)(void(*)(void))os_readlink, METH_FASTCALL|METH_KEYWORDS, os_readlink__doc__},

static PyObject *
os_readlink_impl(PyObject *module, path_t *path, int dir_fd);

static PyObject *
os_readlink(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", "dir_fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "readlink", 0};
    PyObject *argsbuf[2];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    path_t path = PATH_T_INITIALIZE("readlink", "path", 0, 0);
    int dir_fd = DEFAULT_DIR_FD;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (!READLINKAT_DIR_FD_CONVERTER(args[1], &dir_fd)) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_readlink_impl(module, &path, dir_fd);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

#endif /* (defined(HAVE_READLINK) || defined(MS_WINDOWS)) */

#if defined(HAVE_SYMLINK)

PyDoc_STRVAR(os_symlink__doc__,
"symlink($module, /, src, dst, target_is_directory=False, *, dir_fd=None)\n"
"--\n"
"\n"
"Create a symbolic link pointing to src named dst.\n"
"\n"
"target_is_directory is required on Windows if the target is to be\n"
"  interpreted as a directory.  (On Windows, symlink requires\n"
"  Windows 6.0 or greater, and raises a NotImplementedError otherwise.)\n"
"  target_is_directory is ignored on non-Windows platforms.\n"
"\n"
"If dir_fd is not None, it should be a file descriptor open to a directory,\n"
"  and path should be relative; path will then be relative to that directory.\n"
"dir_fd may not be implemented on your platform.\n"
"  If it is unavailable, using it will raise a NotImplementedError.");

#define OS_SYMLINK_METHODDEF    \
    {"symlink", (PyCFunction)(void(*)(void))os_symlink, METH_FASTCALL|METH_KEYWORDS, os_symlink__doc__},

static PyObject *
os_symlink_impl(PyObject *module, path_t *src, path_t *dst,
                int target_is_directory, int dir_fd);

static PyObject *
os_symlink(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"src", "dst", "target_is_directory", "dir_fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "symlink", 0};
    PyObject *argsbuf[4];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 2;
    path_t src = PATH_T_INITIALIZE("symlink", "src", 0, 0);
    path_t dst = PATH_T_INITIALIZE("symlink", "dst", 0, 0);
    int target_is_directory = 0;
    int dir_fd = DEFAULT_DIR_FD;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 2, 3, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &src)) {
        goto exit;
    }
    if (!path_converter(args[1], &dst)) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[2]) {
        target_is_directory = PyObject_IsTrue(args[2]);
        if (target_is_directory < 0) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
skip_optional_pos:
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (!SYMLINKAT_DIR_FD_CONVERTER(args[3], &dir_fd)) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_symlink_impl(module, &src, &dst, target_is_directory, dir_fd);

exit:
    /* Cleanup for src */
    path_cleanup(&src);
    /* Cleanup for dst */
    path_cleanup(&dst);

    return return_value;
}

#endif /* defined(HAVE_SYMLINK) */

PyDoc_STRVAR(os_open__doc__,
"open($module, /, path, flags, mode=511, *, dir_fd=None)\n"
"--\n"
"\n"
"Open a file for low level IO.  Returns a file descriptor (integer).\n"
"\n"
"If dir_fd is not None, it should be a file descriptor open to a directory,\n"
"  and path should be relative; path will then be relative to that directory.\n"
"dir_fd may not be implemented on your platform.\n"
"  If it is unavailable, using it will raise a NotImplementedError.");

#define OS_OPEN_METHODDEF    \
    {"open", (PyCFunction)(void(*)(void))os_open, METH_FASTCALL|METH_KEYWORDS, os_open__doc__},

static int
os_open_impl(PyObject *module, path_t *path, int flags, int mode, int dir_fd);

static PyObject *
os_open(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", "flags", "mode", "dir_fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "open", 0};
    PyObject *argsbuf[4];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 2;
    path_t path = PATH_T_INITIALIZE("open", "path", 0, 0);
    int flags;
    int mode = 511;
    int dir_fd = DEFAULT_DIR_FD;
    int _return_value;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 2, 3, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    flags = _PyLong_AsInt(args[1]);
    if (flags == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[2]) {
        mode = _PyLong_AsInt(args[2]);
        if (mode == -1 && PyErr_Occurred()) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
skip_optional_pos:
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (!OPENAT_DIR_FD_CONVERTER(args[3], &dir_fd)) {
        goto exit;
    }
skip_optional_kwonly:
    _return_value = os_open_impl(module, &path, flags, mode, dir_fd);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyLong_FromLong((long)_return_value);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

PyDoc_STRVAR(os_close__doc__,
"close($module, /, fd)\n"
"--\n"
"\n"
"Close a file descriptor.");

#define OS_CLOSE_METHODDEF    \
    {"close", (PyCFunction)(void(*)(void))os_close, METH_FASTCALL|METH_KEYWORDS, os_close__doc__},

static PyObject *
os_close_impl(PyObject *module, int fd);

static PyObject *
os_close(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "close", 0};
    PyObject *argsbuf[1];
    int fd;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    fd = _PyLong_AsInt(args[0]);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = os_close_impl(module, fd);

exit:
    return return_value;
}

#if defined(HAVE_LOCKF)

PyDoc_STRVAR(os_lockf__doc__,
"lockf($module, fd, command, length, /)\n"
"--\n"
"\n"
"Apply, test or remove a POSIX lock on an open file descriptor.\n"
"\n"
"  fd\n"
"    An open file descriptor.\n"
"  command\n"
"    One of F_LOCK, F_TLOCK, F_ULOCK or F_TEST.\n"
"  length\n"
"    The number of bytes to lock, starting at the current position.");

#define OS_LOCKF_METHODDEF    \
    {"lockf", (PyCFunction)(void(*)(void))os_lockf, METH_FASTCALL, os_lockf__doc__},

static PyObject *
os_lockf_impl(PyObject *module, int fd, int command, Py_off_t length);

static PyObject *
os_lockf(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int fd;
    int command;
    Py_off_t length;

    if (!_PyArg_CheckPositional("lockf", nargs, 3, 3)) {
        goto exit;
    }
    fd = _PyLong_AsInt(args[0]);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    command = _PyLong_AsInt(args[1]);
    if (command == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (!Py_off_t_converter(args[2], &length)) {
        goto exit;
    }
    return_value = os_lockf_impl(module, fd, command, length);

exit:
    return return_value;
}

#endif /* defined(HAVE_LOCKF) */

PyDoc_STRVAR(os_lseek__doc__,
"lseek($module, fd, position, how, /)\n"
"--\n"
"\n"
"Set the position of a file descriptor.  Return the new position.\n"
"\n"
"Return the new cursor position in number of bytes\n"
"relative to the beginning of the file.");

#define OS_LSEEK_METHODDEF    \
    {"lseek", (PyCFunction)(void(*)(void))os_lseek, METH_FASTCALL, os_lseek__doc__},

static Py_off_t
os_lseek_impl(PyObject *module, int fd, Py_off_t position, int how);

static PyObject *
os_lseek(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int fd;
    Py_off_t position;
    int how;
    Py_off_t _return_value;

    if (!_PyArg_CheckPositional("lseek", nargs, 3, 3)) {
        goto exit;
    }
    fd = _PyLong_AsInt(args[0]);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (!Py_off_t_converter(args[1], &position)) {
        goto exit;
    }
    how = _PyLong_AsInt(args[2]);
    if (how == -1 && PyErr_Occurred()) {
        goto exit;
    }
    _return_value = os_lseek_impl(module, fd, position, how);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyLong_FromPy_off_t(_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(os_read__doc__,
"read($module, fd, length, /)\n"
"--\n"
"\n"
"Read from a file descriptor.  Returns a bytes object.");

#define OS_READ_METHODDEF    \
    {"read", (PyCFunction)(void(*)(void))os_read, METH_FASTCALL, os_read__doc__},

static PyObject *
os_read_impl(PyObject *module, int fd, Py_ssize_t length);

static PyObject *
os_read(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int fd;
    Py_ssize_t length;

    if (!_PyArg_CheckPositional("read", nargs, 2, 2)) {
        goto exit;
    }
    fd = _PyLong_AsInt(args[0]);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(args[1]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        length = ival;
    }
    return_value = os_read_impl(module, fd, length);

exit:
    return return_value;
}

PyDoc_STRVAR(os_write__doc__,
"write($module, fd, data, /)\n"
"--\n"
"\n"
"Write a bytes object to a file descriptor.");

#define OS_WRITE_METHODDEF    \
    {"write", (PyCFunction)(void(*)(void))os_write, METH_FASTCALL, os_write__doc__},

static Py_ssize_t
os_write_impl(PyObject *module, int fd, Py_buffer *data);

static PyObject *
os_write(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int fd;
    Py_buffer data = {NULL, NULL};
    Py_ssize_t _return_value;

    if (!_PyArg_CheckPositional("write", nargs, 2, 2)) {
        goto exit;
    }
    fd = _PyLong_AsInt(args[0]);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[1], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("write", "argument 2", "contiguous buffer", args[1]);
        goto exit;
    }
    _return_value = os_write_impl(module, fd, &data);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyLong_FromSsize_t(_return_value);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(os_fstat__doc__,
"fstat($module, /, fd)\n"
"--\n"
"\n"
"Perform a stat system call on the given file descriptor.\n"
"\n"
"Like stat(), but for an open file descriptor.\n"
"Equivalent to os.stat(fd).");

#define OS_FSTAT_METHODDEF    \
    {"fstat", (PyCFunction)(void(*)(void))os_fstat, METH_FASTCALL|METH_KEYWORDS, os_fstat__doc__},

static PyObject *
os_fstat_impl(PyObject *module, int fd);

static PyObject *
os_fstat(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "fstat", 0};
    PyObject *argsbuf[1];
    int fd;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    fd = _PyLong_AsInt(args[0]);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = os_fstat_impl(module, fd);

exit:
    return return_value;
}

PyDoc_STRVAR(os_isatty__doc__,
"isatty($module, fd, /)\n"
"--\n"
"\n"
"Return True if the fd is connected to a terminal.\n"
"\n"
"Return True if the file descriptor is an open file descriptor\n"
"connected to the slave end of a terminal.");

#define OS_ISATTY_METHODDEF    \
    {"isatty", (PyCFunction)os_isatty, METH_O, os_isatty__doc__},

static int
os_isatty_impl(PyObject *module, int fd);

static PyObject *
os_isatty(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int fd;
    int _return_value;

    fd = _PyLong_AsInt(arg);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    _return_value = os_isatty_impl(module, fd);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}

#if (defined HAVE_FTRUNCATE || defined MS_WINDOWS)

PyDoc_STRVAR(os_ftruncate__doc__,
"ftruncate($module, fd, length, /)\n"
"--\n"
"\n"
"Truncate a file, specified by file descriptor, to a specific length.");

#define OS_FTRUNCATE_METHODDEF    \
    {"ftruncate", (PyCFunction)(void(*)(void))os_ftruncate, METH_FASTCALL, os_ftruncate__doc__},

static PyObject *
os_ftruncate_impl(PyObject *module, int fd, Py_off_t length);

static PyObject *
os_ftruncate(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int fd;
    Py_off_t length;

    if (!_PyArg_CheckPositional("ftruncate", nargs, 2, 2)) {
        goto exit;
    }
    fd = _PyLong_AsInt(args[0]);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (!Py_off_t_converter(args[1], &length)) {
        goto exit;
    }
    return_value = os_ftruncate_impl(module, fd, length);

exit:
    return return_value;
}

#endif /* (defined HAVE_FTRUNCATE || defined MS_WINDOWS) */

#if (defined HAVE_TRUNCATE || defined MS_WINDOWS)

PyDoc_STRVAR(os_truncate__doc__,
"truncate($module, /, path, length)\n"
"--\n"
"\n"
"Truncate a file, specified by path, to a specific length.\n"
"\n"
"On some platforms, path may also be specified as an open file descriptor.\n"
"  If this functionality is unavailable, using it raises an exception.");

#define OS_TRUNCATE_METHODDEF    \
    {"truncate", (PyCFunction)(void(*)(void))os_truncate, METH_FASTCALL|METH_KEYWORDS, os_truncate__doc__},

static PyObject *
os_truncate_impl(PyObject *module, path_t *path, Py_off_t length);

static PyObject *
os_truncate(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", "length", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "truncate", 0};
    PyObject *argsbuf[2];
    path_t path = PATH_T_INITIALIZE("truncate", "path", 0, PATH_HAVE_FTRUNCATE);
    Py_off_t length;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 2, 2, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    if (!Py_off_t_converter(args[1], &length)) {
        goto exit;
    }
    return_value = os_truncate_impl(module, &path, length);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

#endif /* (defined HAVE_TRUNCATE || defined MS_WINDOWS) */

#if defined(MS_WINDOWS)

PyDoc_STRVAR(os_putenv__doc__,
"putenv($module, name, value, /)\n"
"--\n"
"\n"
"Change or add an environment variable.");

#define OS_PUTENV_METHODDEF    \
    {"putenv", (PyCFunction)(void(*)(void))os_putenv, METH_FASTCALL, os_putenv__doc__},

static PyObject *
os_putenv_impl(PyObject *module, PyObject *name, PyObject *value);

static PyObject *
os_putenv(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *name;
    PyObject *value;

    if (!_PyArg_CheckPositional("putenv", nargs, 2, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("putenv", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    name = args[0];
    if (!PyUnicode_Check(args[1])) {
        _PyArg_BadArgument("putenv", "argument 2", "str", args[1]);
        goto exit;
    }
    if (PyUnicode_READY(args[1]) == -1) {
        goto exit;
    }
    value = args[1];
    return_value = os_putenv_impl(module, name, value);

exit:
    return return_value;
}

#endif /* defined(MS_WINDOWS) */

#if !defined(MS_WINDOWS)

PyDoc_STRVAR(os_putenv__doc__,
"putenv($module, name, value, /)\n"
"--\n"
"\n"
"Change or add an environment variable.");

#define OS_PUTENV_METHODDEF    \
    {"putenv", (PyCFunction)(void(*)(void))os_putenv, METH_FASTCALL, os_putenv__doc__},

static PyObject *
os_putenv_impl(PyObject *module, PyObject *name, PyObject *value);

static PyObject *
os_putenv(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *name = NULL;
    PyObject *value = NULL;

    if (!_PyArg_CheckPositional("putenv", nargs, 2, 2)) {
        goto exit;
    }
    if (!PyUnicode_FSConverter(args[0], &name)) {
        goto exit;
    }
    if (!PyUnicode_FSConverter(args[1], &value)) {
        goto exit;
    }
    return_value = os_putenv_impl(module, name, value);

exit:
    /* Cleanup for name */
    Py_XDECREF(name);
    /* Cleanup for value */
    Py_XDECREF(value);

    return return_value;
}

#endif /* !defined(MS_WINDOWS) */

#if defined(MS_WINDOWS)

PyDoc_STRVAR(os_unsetenv__doc__,
"unsetenv($module, name, /)\n"
"--\n"
"\n"
"Delete an environment variable.");

#define OS_UNSETENV_METHODDEF    \
    {"unsetenv", (PyCFunction)os_unsetenv, METH_O, os_unsetenv__doc__},

static PyObject *
os_unsetenv_impl(PyObject *module, PyObject *name);

static PyObject *
os_unsetenv(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    PyObject *name;

    if (!PyUnicode_Check(arg)) {
        _PyArg_BadArgument("unsetenv", "argument", "str", arg);
        goto exit;
    }
    if (PyUnicode_READY(arg) == -1) {
        goto exit;
    }
    name = arg;
    return_value = os_unsetenv_impl(module, name);

exit:
    return return_value;
}

#endif /* defined(MS_WINDOWS) */

#if !defined(MS_WINDOWS)

PyDoc_STRVAR(os_unsetenv__doc__,
"unsetenv($module, name, /)\n"
"--\n"
"\n"
"Delete an environment variable.");

#define OS_UNSETENV_METHODDEF    \
    {"unsetenv", (PyCFunction)os_unsetenv, METH_O, os_unsetenv__doc__},

static PyObject *
os_unsetenv_impl(PyObject *module, PyObject *name);

static PyObject *
os_unsetenv(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    PyObject *name = NULL;

    if (!PyUnicode_FSConverter(arg, &name)) {
        goto exit;
    }
    return_value = os_unsetenv_impl(module, name);

exit:
    /* Cleanup for name */
    Py_XDECREF(name);

    return return_value;
}

#endif /* !defined(MS_WINDOWS) */

PyDoc_STRVAR(os_strerror__doc__,
"strerror($module, code, /)\n"
"--\n"
"\n"
"Translate an error code to a message string.");

#define OS_STRERROR_METHODDEF    \
    {"strerror", (PyCFunction)os_strerror, METH_O, os_strerror__doc__},

static PyObject *
os_strerror_impl(PyObject *module, int code);

static PyObject *
os_strerror(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int code;

    code = _PyLong_AsInt(arg);
    if (code == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = os_strerror_impl(module, code);

exit:
    return return_value;
}

#if (defined(HAVE_FSTATVFS) && defined(HAVE_SYS_STATVFS_H))

PyDoc_STRVAR(os_fstatvfs__doc__,
"fstatvfs($module, fd, /)\n"
"--\n"
"\n"
"Perform an fstatvfs system call on the given fd.\n"
"\n"
"Equivalent to statvfs(fd).");

#define OS_FSTATVFS_METHODDEF    \
    {"fstatvfs", (PyCFunction)os_fstatvfs, METH_O, os_fstatvfs__doc__},

static PyObject *
os_fstatvfs_impl(PyObject *module, int fd);

static PyObject *
os_fstatvfs(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int fd;

    fd = _PyLong_AsInt(arg);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = os_fstatvfs_impl(module, fd);

exit:
    return return_value;
}

#endif /* (defined(HAVE_FSTATVFS) && defined(HAVE_SYS_STATVFS_H)) */

#if (defined(HAVE_STATVFS) && defined(HAVE_SYS_STATVFS_H))

PyDoc_STRVAR(os_statvfs__doc__,
"statvfs($module, /, path)\n"
"--\n"
"\n"
"Perform a statvfs system call on the given path.\n"
"\n"
"path may always be specified as a string.\n"
"On some platforms, path may also be specified as an open file descriptor.\n"
"  If this functionality is unavailable, using it raises an exception.");

#define OS_STATVFS_METHODDEF    \
    {"statvfs", (PyCFunction)(void(*)(void))os_statvfs, METH_FASTCALL|METH_KEYWORDS, os_statvfs__doc__},

static PyObject *
os_statvfs_impl(PyObject *module, path_t *path);

static PyObject *
os_statvfs(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "statvfs", 0};
    PyObject *argsbuf[1];
    path_t path = PATH_T_INITIALIZE("statvfs", "path", 0, PATH_HAVE_FSTATVFS);

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    return_value = os_statvfs_impl(module, &path);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

#endif /* (defined(HAVE_STATVFS) && defined(HAVE_SYS_STATVFS_H)) */

#if defined(HAVE_FPATHCONF)

PyDoc_STRVAR(os_fpathconf__doc__,
"fpathconf($module, fd, name, /)\n"
"--\n"
"\n"
"Return the configuration limit name for the file descriptor fd.\n"
"\n"
"If there is no limit, return -1.");

#define OS_FPATHCONF_METHODDEF    \
    {"fpathconf", (PyCFunction)(void(*)(void))os_fpathconf, METH_FASTCALL, os_fpathconf__doc__},

static long
os_fpathconf_impl(PyObject *module, int fd, int name);

static PyObject *
os_fpathconf(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int fd;
    int name;
    long _return_value;

    if (!_PyArg_CheckPositional("fpathconf", nargs, 2, 2)) {
        goto exit;
    }
    fd = _PyLong_AsInt(args[0]);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (!conv_path_confname(args[1], &name)) {
        goto exit;
    }
    _return_value = os_fpathconf_impl(module, fd, name);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyLong_FromLong(_return_value);

exit:
    return return_value;
}

#endif /* defined(HAVE_FPATHCONF) */

#if defined(HAVE_PATHCONF)

PyDoc_STRVAR(os_pathconf__doc__,
"pathconf($module, /, path, name)\n"
"--\n"
"\n"
"Return the configuration limit name for the file or directory path.\n"
"\n"
"If there is no limit, return -1.\n"
"On some platforms, path may also be specified as an open file descriptor.\n"
"  If this functionality is unavailable, using it raises an exception.");

#define OS_PATHCONF_METHODDEF    \
    {"pathconf", (PyCFunction)(void(*)(void))os_pathconf, METH_FASTCALL|METH_KEYWORDS, os_pathconf__doc__},

static long
os_pathconf_impl(PyObject *module, path_t *path, int name);

static PyObject *
os_pathconf(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", "name", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "pathconf", 0};
    PyObject *argsbuf[2];
    path_t path = PATH_T_INITIALIZE("pathconf", "path", 0, PATH_HAVE_FPATHCONF);
    int name;
    long _return_value;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 2, 2, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
    if (!conv_path_confname(args[1], &name)) {
        goto exit;
    }
    _return_value = os_pathconf_impl(module, &path, name);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyLong_FromLong(_return_value);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

#endif /* defined(HAVE_PATHCONF) */

#if defined(HAVE_CONFSTR)

PyDoc_STRVAR(os_confstr__doc__,
"confstr($module, name, /)\n"
"--\n"
"\n"
"Return a string-valued system configuration variable.");

#define OS_CONFSTR_METHODDEF    \
    {"confstr", (PyCFunction)os_confstr, METH_O, os_confstr__doc__},

static PyObject *
os_confstr_impl(PyObject *module, int name);

static PyObject *
os_confstr(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int name;

    if (!conv_confstr_confname(arg, &name)) {
        goto exit;
    }
    return_value = os_confstr_impl(module, name);

exit:
    return return_value;
}

#endif /* defined(HAVE_CONFSTR) */

PyDoc_STRVAR(os_abort__doc__,
"abort($module, /)\n"
"--\n"
"\n"
"Abort the interpreter immediately.\n"
"\n"
"This function \'dumps core\' or otherwise fails in the hardest way possible\n"
"on the hosting operating system.  This function never returns.");

#define OS_ABORT_METHODDEF    \
    {"abort", (PyCFunction)os_abort, METH_NOARGS, os_abort__doc__},

static PyObject *
os_abort_impl(PyObject *module);

static PyObject *
os_abort(PyObject *module, PyObject *Py_UNUSED(ignored))
{
    return os_abort_impl(module);
}

PyDoc_STRVAR(os_device_encoding__doc__,
"device_encoding($module, /, fd)\n"
"--\n"
"\n"
"Return a string describing the encoding of a terminal\'s file descriptor.\n"
"\n"
"The file descriptor must be attached to a terminal.\n"
"If the device is not a terminal, return None.");

#define OS_DEVICE_ENCODING_METHODDEF    \
    {"device_encoding", (PyCFunction)(void(*)(void))os_device_encoding, METH_FASTCALL|METH_KEYWORDS, os_device_encoding__doc__},

static PyObject *
os_device_encoding_impl(PyObject *module, int fd);

static PyObject *
os_device_encoding(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"fd", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "device_encoding", 0};
    PyObject *argsbuf[1];
    int fd;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    fd = _PyLong_AsInt(args[0]);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = os_device_encoding_impl(module, fd);

exit:
    return return_value;
}

PyDoc_STRVAR(os_get_inheritable__doc__,
"get_inheritable($module, fd, /)\n"
"--\n"
"\n"
"Get the close-on-exe flag of the specified file descriptor.");

#define OS_GET_INHERITABLE_METHODDEF    \
    {"get_inheritable", (PyCFunction)os_get_inheritable, METH_O, os_get_inheritable__doc__},

static int
os_get_inheritable_impl(PyObject *module, int fd);

static PyObject *
os_get_inheritable(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int fd;
    int _return_value;

    fd = _PyLong_AsInt(arg);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    _return_value = os_get_inheritable_impl(module, fd);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(os_set_inheritable__doc__,
"set_inheritable($module, fd, inheritable, /)\n"
"--\n"
"\n"
"Set the inheritable flag of the specified file descriptor.");

#define OS_SET_INHERITABLE_METHODDEF    \
    {"set_inheritable", (PyCFunction)(void(*)(void))os_set_inheritable, METH_FASTCALL, os_set_inheritable__doc__},

static PyObject *
os_set_inheritable_impl(PyObject *module, int fd, int inheritable);

static PyObject *
os_set_inheritable(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int fd;
    int inheritable;

    if (!_PyArg_CheckPositional("set_inheritable", nargs, 2, 2)) {
        goto exit;
    }
    fd = _PyLong_AsInt(args[0]);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    inheritable = _PyLong_AsInt(args[1]);
    if (inheritable == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = os_set_inheritable_impl(module, fd, inheritable);

exit:
    return return_value;
}

#if defined(MS_WINDOWS)

PyDoc_STRVAR(os_get_handle_inheritable__doc__,
"get_handle_inheritable($module, handle, /)\n"
"--\n"
"\n"
"Get the close-on-exe flag of the specified file descriptor.");

#define OS_GET_HANDLE_INHERITABLE_METHODDEF    \
    {"get_handle_inheritable", (PyCFunction)os_get_handle_inheritable, METH_O, os_get_handle_inheritable__doc__},

static int
os_get_handle_inheritable_impl(PyObject *module, intptr_t handle);

static PyObject *
os_get_handle_inheritable(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    intptr_t handle;
    int _return_value;

    if (!PyArg_Parse(arg, "" _Py_PARSE_INTPTR ":get_handle_inheritable", &handle)) {
        goto exit;
    }
    _return_value = os_get_handle_inheritable_impl(module, handle);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}

#endif /* defined(MS_WINDOWS) */

#if defined(MS_WINDOWS)

PyDoc_STRVAR(os_set_handle_inheritable__doc__,
"set_handle_inheritable($module, handle, inheritable, /)\n"
"--\n"
"\n"
"Set the inheritable flag of the specified handle.");

#define OS_SET_HANDLE_INHERITABLE_METHODDEF    \
    {"set_handle_inheritable", (PyCFunction)(void(*)(void))os_set_handle_inheritable, METH_FASTCALL, os_set_handle_inheritable__doc__},

static PyObject *
os_set_handle_inheritable_impl(PyObject *module, intptr_t handle,
                               int inheritable);

static PyObject *
os_set_handle_inheritable(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    intptr_t handle;
    int inheritable;

    if (!_PyArg_ParseStack(args, nargs, "" _Py_PARSE_INTPTR "p:set_handle_inheritable",
        &handle, &inheritable)) {
        goto exit;
    }
    return_value = os_set_handle_inheritable_impl(module, handle, inheritable);

exit:
    return return_value;
}

#endif /* defined(MS_WINDOWS) */

#if !defined(MS_WINDOWS)

PyDoc_STRVAR(os_get_blocking__doc__,
"get_blocking($module, fd, /)\n"
"--\n"
"\n"
"Get the blocking mode of the file descriptor.\n"
"\n"
"Return False if the O_NONBLOCK flag is set, True if the flag is cleared.");

#define OS_GET_BLOCKING_METHODDEF    \
    {"get_blocking", (PyCFunction)os_get_blocking, METH_O, os_get_blocking__doc__},

static int
os_get_blocking_impl(PyObject *module, int fd);

static PyObject *
os_get_blocking(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int fd;
    int _return_value;

    fd = _PyLong_AsInt(arg);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    _return_value = os_get_blocking_impl(module, fd);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}

#endif /* !defined(MS_WINDOWS) */

#if !defined(MS_WINDOWS)

PyDoc_STRVAR(os_set_blocking__doc__,
"set_blocking($module, fd, blocking, /)\n"
"--\n"
"\n"
"Set the blocking mode of the specified file descriptor.\n"
"\n"
"Set the O_NONBLOCK flag if blocking is False,\n"
"clear the O_NONBLOCK flag otherwise.");

#define OS_SET_BLOCKING_METHODDEF    \
    {"set_blocking", (PyCFunction)(void(*)(void))os_set_blocking, METH_FASTCALL, os_set_blocking__doc__},

static PyObject *
os_set_blocking_impl(PyObject *module, int fd, int blocking);

static PyObject *
os_set_blocking(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int fd;
    int blocking;

    if (!_PyArg_CheckPositional("set_blocking", nargs, 2, 2)) {
        goto exit;
    }
    fd = _PyLong_AsInt(args[0]);
    if (fd == -1 && PyErr_Occurred()) {
        goto exit;
    }
    blocking = _PyLong_AsInt(args[1]);
    if (blocking == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = os_set_blocking_impl(module, fd, blocking);

exit:
    return return_value;
}

#endif /* !defined(MS_WINDOWS) */

PyDoc_STRVAR(os_DirEntry_is_symlink__doc__,
"is_symlink($self, /)\n"
"--\n"
"\n"
"Return True if the entry is a symbolic link; cached per entry.");

#define OS_DIRENTRY_IS_SYMLINK_METHODDEF    \
    {"is_symlink", (PyCFunction)(void(*)(void))os_DirEntry_is_symlink, METH_METHOD|METH_FASTCALL|METH_KEYWORDS, os_DirEntry_is_symlink__doc__},

static int
os_DirEntry_is_symlink_impl(DirEntry *self, PyTypeObject *defining_class);

static PyObject *
os_DirEntry_is_symlink(DirEntry *self, PyTypeObject *defining_class, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = { NULL};
    static _PyArg_Parser _parser = {":is_symlink", _keywords, 0};
    int _return_value;

    if (!_PyArg_ParseStackAndKeywords(args, nargs, kwnames, &_parser
        )) {
        goto exit;
    }
    _return_value = os_DirEntry_is_symlink_impl(self, defining_class);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(os_DirEntry_stat__doc__,
"stat($self, /, *, follow_symlinks=True)\n"
"--\n"
"\n"
"Return stat_result object for the entry; cached per entry.");

#define OS_DIRENTRY_STAT_METHODDEF    \
    {"stat", (PyCFunction)(void(*)(void))os_DirEntry_stat, METH_METHOD|METH_FASTCALL|METH_KEYWORDS, os_DirEntry_stat__doc__},

static PyObject *
os_DirEntry_stat_impl(DirEntry *self, PyTypeObject *defining_class,
                      int follow_symlinks);

static PyObject *
os_DirEntry_stat(DirEntry *self, PyTypeObject *defining_class, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"follow_symlinks", NULL};
    static _PyArg_Parser _parser = {"|$p:stat", _keywords, 0};
    int follow_symlinks = 1;

    if (!_PyArg_ParseStackAndKeywords(args, nargs, kwnames, &_parser,
        &follow_symlinks)) {
        goto exit;
    }
    return_value = os_DirEntry_stat_impl(self, defining_class, follow_symlinks);

exit:
    return return_value;
}

PyDoc_STRVAR(os_DirEntry_is_dir__doc__,
"is_dir($self, /, *, follow_symlinks=True)\n"
"--\n"
"\n"
"Return True if the entry is a directory; cached per entry.");

#define OS_DIRENTRY_IS_DIR_METHODDEF    \
    {"is_dir", (PyCFunction)(void(*)(void))os_DirEntry_is_dir, METH_METHOD|METH_FASTCALL|METH_KEYWORDS, os_DirEntry_is_dir__doc__},

static int
os_DirEntry_is_dir_impl(DirEntry *self, PyTypeObject *defining_class,
                        int follow_symlinks);

static PyObject *
os_DirEntry_is_dir(DirEntry *self, PyTypeObject *defining_class, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"follow_symlinks", NULL};
    static _PyArg_Parser _parser = {"|$p:is_dir", _keywords, 0};
    int follow_symlinks = 1;
    int _return_value;

    if (!_PyArg_ParseStackAndKeywords(args, nargs, kwnames, &_parser,
        &follow_symlinks)) {
        goto exit;
    }
    _return_value = os_DirEntry_is_dir_impl(self, defining_class, follow_symlinks);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(os_DirEntry_is_file__doc__,
"is_file($self, /, *, follow_symlinks=True)\n"
"--\n"
"\n"
"Return True if the entry is a file; cached per entry.");

#define OS_DIRENTRY_IS_FILE_METHODDEF    \
    {"is_file", (PyCFunction)(void(*)(void))os_DirEntry_is_file, METH_METHOD|METH_FASTCALL|METH_KEYWORDS, os_DirEntry_is_file__doc__},

static int
os_DirEntry_is_file_impl(DirEntry *self, PyTypeObject *defining_class,
                         int follow_symlinks);

static PyObject *
os_DirEntry_is_file(DirEntry *self, PyTypeObject *defining_class, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"follow_symlinks", NULL};
    static _PyArg_Parser _parser = {"|$p:is_file", _keywords, 0};
    int follow_symlinks = 1;
    int _return_value;

    if (!_PyArg_ParseStackAndKeywords(args, nargs, kwnames, &_parser,
        &follow_symlinks)) {
        goto exit;
    }
    _return_value = os_DirEntry_is_file_impl(self, defining_class, follow_symlinks);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(os_DirEntry_inode__doc__,
"inode($self, /)\n"
"--\n"
"\n"
"Return inode of the entry; cached per entry.");

#define OS_DIRENTRY_INODE_METHODDEF    \
    {"inode", (PyCFunction)os_DirEntry_inode, METH_NOARGS, os_DirEntry_inode__doc__},

static PyObject *
os_DirEntry_inode_impl(DirEntry *self);

static PyObject *
os_DirEntry_inode(DirEntry *self, PyObject *Py_UNUSED(ignored))
{
    return os_DirEntry_inode_impl(self);
}

PyDoc_STRVAR(os_DirEntry___fspath____doc__,
"__fspath__($self, /)\n"
"--\n"
"\n"
"Returns the path for the entry.");

#define OS_DIRENTRY___FSPATH___METHODDEF    \
    {"__fspath__", (PyCFunction)os_DirEntry___fspath__, METH_NOARGS, os_DirEntry___fspath____doc__},

static PyObject *
os_DirEntry___fspath___impl(DirEntry *self);

static PyObject *
os_DirEntry___fspath__(DirEntry *self, PyObject *Py_UNUSED(ignored))
{
    return os_DirEntry___fspath___impl(self);
}

PyDoc_STRVAR(os_scandir__doc__,
"scandir($module, /, path=None)\n"
"--\n"
"\n"
"Return an iterator of DirEntry objects for given path.\n"
"\n"
"path can be specified as either str, bytes, or a path-like object.  If path\n"
"is bytes, the names of yielded DirEntry objects will also be bytes; in\n"
"all other circumstances they will be str.\n"
"\n"
"If path is None, uses the path=\'.\'.");

#define OS_SCANDIR_METHODDEF    \
    {"scandir", (PyCFunction)(void(*)(void))os_scandir, METH_FASTCALL|METH_KEYWORDS, os_scandir__doc__},

static PyObject *
os_scandir_impl(PyObject *module, path_t *path);

static PyObject *
os_scandir(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "scandir", 0};
    PyObject *argsbuf[1];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 0;
    path_t path = PATH_T_INITIALIZE("scandir", "path", 1, PATH_HAVE_FDOPENDIR);

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 0, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
skip_optional_pos:
    return_value = os_scandir_impl(module, &path);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}

PyDoc_STRVAR(os_fspath__doc__,
"fspath($module, /, path)\n"
"--\n"
"\n"
"Return the file system path representation of the object.\n"
"\n"
"If the object is str or bytes, then allow it to pass through as-is. If the\n"
"object defines __fspath__(), then return the result of that method. All other\n"
"types raise a TypeError.");

#define OS_FSPATH_METHODDEF    \
    {"fspath", (PyCFunction)(void(*)(void))os_fspath, METH_FASTCALL|METH_KEYWORDS, os_fspath__doc__},

static PyObject *
os_fspath_impl(PyObject *module, PyObject *path);

static PyObject *
os_fspath(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "fspath", 0};
    PyObject *argsbuf[1];
    PyObject *path;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    path = args[0];
    return_value = os_fspath_impl(module, path);

exit:
    return return_value;
}

#ifndef OS_FCHDIR_METHODDEF
    #define OS_FCHDIR_METHODDEF
#endif /* !defined(OS_FCHDIR_METHODDEF) */

#ifndef OS_LINK_METHODDEF
    #define OS_LINK_METHODDEF
#endif /* !defined(OS_LINK_METHODDEF) */

#ifndef OS_SYSTEM_METHODDEF
    #define OS_SYSTEM_METHODDEF
#endif /* !defined(OS_SYSTEM_METHODDEF) */

#ifndef OS_PLOCK_METHODDEF
    #define OS_PLOCK_METHODDEF
#endif /* !defined(OS_PLOCK_METHODDEF) */

#ifndef OS_READLINK_METHODDEF
    #define OS_READLINK_METHODDEF
#endif /* !defined(OS_READLINK_METHODDEF) */

#ifndef OS_SYMLINK_METHODDEF
    #define OS_SYMLINK_METHODDEF
#endif /* !defined(OS_SYMLINK_METHODDEF) */

#ifndef OS_TIMES_METHODDEF
    #define OS_TIMES_METHODDEF
#endif /* !defined(OS_TIMES_METHODDEF) */

#ifndef OS_LOCKF_METHODDEF
    #define OS_LOCKF_METHODDEF
#endif /* !defined(OS_LOCKF_METHODDEF) */

#ifndef OS_MAJOR_METHODDEF
    #define OS_MAJOR_METHODDEF
#endif /* !defined(OS_MAJOR_METHODDEF) */

#ifndef OS_MINOR_METHODDEF
    #define OS_MINOR_METHODDEF
#endif /* !defined(OS_MINOR_METHODDEF) */

#ifndef OS_MAKEDEV_METHODDEF
    #define OS_MAKEDEV_METHODDEF
#endif /* !defined(OS_MAKEDEV_METHODDEF) */

#ifndef OS_FTRUNCATE_METHODDEF
    #define OS_FTRUNCATE_METHODDEF
#endif /* !defined(OS_FTRUNCATE_METHODDEF) */

#ifndef OS_TRUNCATE_METHODDEF
    #define OS_TRUNCATE_METHODDEF
#endif /* !defined(OS_TRUNCATE_METHODDEF) */

#ifndef OS_PUTENV_METHODDEF
    #define OS_PUTENV_METHODDEF
#endif /* !defined(OS_PUTENV_METHODDEF) */

#ifndef OS_UNSETENV_METHODDEF
    #define OS_UNSETENV_METHODDEF
#endif /* !defined(OS_UNSETENV_METHODDEF) */

#ifndef OS_FSTATVFS_METHODDEF
    #define OS_FSTATVFS_METHODDEF
#endif /* !defined(OS_FSTATVFS_METHODDEF) */

#ifndef OS_STATVFS_METHODDEF
    #define OS_STATVFS_METHODDEF
#endif /* !defined(OS_STATVFS_METHODDEF) */

#ifndef OS_FPATHCONF_METHODDEF
    #define OS_FPATHCONF_METHODDEF
#endif /* !defined(OS_FPATHCONF_METHODDEF) */

#ifndef OS_PATHCONF_METHODDEF
    #define OS_PATHCONF_METHODDEF
#endif /* !defined(OS_PATHCONF_METHODDEF) */

#ifndef OS_CONFSTR_METHODDEF
    #define OS_CONFSTR_METHODDEF
#endif /* !defined(OS_CONFSTR_METHODDEF) */

#ifndef OS_GET_HANDLE_INHERITABLE_METHODDEF
    #define OS_GET_HANDLE_INHERITABLE_METHODDEF
#endif /* !defined(OS_GET_HANDLE_INHERITABLE_METHODDEF) */

#ifndef OS_SET_HANDLE_INHERITABLE_METHODDEF
    #define OS_SET_HANDLE_INHERITABLE_METHODDEF
#endif /* !defined(OS_SET_HANDLE_INHERITABLE_METHODDEF) */

#ifndef OS_GET_BLOCKING_METHODDEF
    #define OS_GET_BLOCKING_METHODDEF
#endif /* !defined(OS_GET_BLOCKING_METHODDEF) */

#ifndef OS_SET_BLOCKING_METHODDEF
    #define OS_SET_BLOCKING_METHODDEF
#endif /* !defined(OS_SET_BLOCKING_METHODDEF) */


static PyMethodDef DirEntry_methods[] = {
    OS_DIRENTRY_IS_DIR_METHODDEF
    OS_DIRENTRY_IS_FILE_METHODDEF
    OS_DIRENTRY_IS_SYMLINK_METHODDEF
    OS_DIRENTRY_STAT_METHODDEF
    OS_DIRENTRY_INODE_METHODDEF
    OS_DIRENTRY___FSPATH___METHODDEF
    {NULL}
};

static PyType_Slot DirEntryType_slots[] = {
    {Py_tp_new, _disabled_new},
    {Py_tp_dealloc, DirEntry_dealloc},
    {Py_tp_repr, DirEntry_repr},
    {Py_tp_methods, DirEntry_methods},
    {Py_tp_members, DirEntry_members},
    {0, 0},
};

static PyType_Spec DirEntryType_spec = {
    MODNAME ".DirEntry",
    sizeof(DirEntry),
    0,
    Py_TPFLAGS_DEFAULT,
    DirEntryType_slots
};


#ifdef MS_WINDOWS

static wchar_t *
join_path_filenameW(const wchar_t *path_wide, const wchar_t *filename)
{
    Py_ssize_t path_len;
    Py_ssize_t size;
    wchar_t *result;
    wchar_t ch;

    if (!path_wide) { /* Default arg: "." */
        path_wide = L".";
        path_len = 1;
    }
    else {
        path_len = wcslen(path_wide);
    }

    /* The +1's are for the path separator and the NUL */
    size = path_len + 1 + wcslen(filename) + 1;
    result = PyMem_New(wchar_t, size);
    if (!result) {
        PyErr_NoMemory();
        return NULL;
    }
    wcscpy(result, path_wide);
    if (path_len > 0) {
        ch = result[path_len - 1];
        if (ch != SEP && ch != ALTSEP && ch != L':')
            result[path_len++] = SEP;
        wcscpy(result + path_len, filename);
    }
    return result;
}

static PyObject *
DirEntry_from_find_data(PyObject *module, path_t *path, WIN32_FIND_DATAW *dataW)
{
    DirEntry *entry;
    BY_HANDLE_FILE_INFORMATION file_info;
    ULONG reparse_tag;
    wchar_t *joined_path;

    PyObject *DirEntryType = get_posix_state(module)->DirEntryType;
    entry = PyObject_New(DirEntry, (PyTypeObject *)DirEntryType);
    if (!entry)
        return NULL;
    entry->name = NULL;
    entry->path = NULL;
    entry->stat = NULL;
    entry->lstat = NULL;
    entry->got_file_index = 0;

    entry->name = PyUnicode_FromWideChar(dataW->cFileName, -1);
    if (!entry->name)
        goto error;
    if (path->narrow) {
        Py_SETREF(entry->name, PyUnicode_EncodeFSDefault(entry->name));
        if (!entry->name)
            goto error;
    }

    joined_path = join_path_filenameW(path->wide, dataW->cFileName);
    if (!joined_path)
        goto error;

    entry->path = PyUnicode_FromWideChar(joined_path, -1);
    PyMem_Free(joined_path);
    if (!entry->path)
        goto error;
    if (path->narrow) {
        Py_SETREF(entry->path, PyUnicode_EncodeFSDefault(entry->path));
        if (!entry->path)
            goto error;
    }

    find_data_to_file_info(dataW, &file_info, &reparse_tag);
    _Py_attribute_data_to_stat(&file_info, reparse_tag, &entry->win32_lstat);

    return (PyObject *)entry;

error:
    Py_DECREF(entry);
    return NULL;
}

#else /* POSIX */

static char *
join_path_filename(const char *path_narrow, const char* filename, Py_ssize_t filename_len)
{
    Py_ssize_t path_len;
    Py_ssize_t size;
    char *result;

    if (!path_narrow) { /* Default arg: "." */
        path_narrow = ".";
        path_len = 1;
    }
    else {
        path_len = strlen(path_narrow);
    }

    if (filename_len == -1)
        filename_len = strlen(filename);

    /* The +1's are for the path separator and the NUL */
    size = path_len + 1 + filename_len + 1;
    result = PyMem_New(char, size);
    if (!result) {
        PyErr_NoMemory();
        return NULL;
    }
    strcpy(result, path_narrow);
    if (path_len > 0 && result[path_len - 1] != '/')
        result[path_len++] = '/';
    strcpy(result + path_len, filename);
    return result;
}

static PyObject *
DirEntry_from_posix_info(PyObject *module, path_t *path, const char *name,
                         Py_ssize_t name_len, ino_t d_ino
#ifdef HAVE_DIRENT_D_TYPE
                         , unsigned char d_type
#endif
                         )
{
    DirEntry *entry;
    char *joined_path;

    PyObject *DirEntryType = get_posix_state(module)->DirEntryType;
    entry = PyObject_New(DirEntry, (PyTypeObject *)DirEntryType);
    if (!entry)
        return NULL;
    entry->name = NULL;
    entry->path = NULL;
    entry->stat = NULL;
    entry->lstat = NULL;

    if (path->fd != -1) {
        entry->dir_fd = path->fd;
        joined_path = NULL;
    }
    else {
        entry->dir_fd = DEFAULT_DIR_FD;
        joined_path = join_path_filename(path->narrow, name, name_len);
        if (!joined_path)
            goto error;
    }

    if (!path->narrow || !PyObject_CheckBuffer(path->object)) {
        entry->name = PyUnicode_DecodeFSDefaultAndSize(name, name_len);
        if (joined_path)
            entry->path = PyUnicode_DecodeFSDefault(joined_path);
    }
    else {
        entry->name = PyBytes_FromStringAndSize(name, name_len);
        if (joined_path)
            entry->path = PyBytes_FromString(joined_path);
    }
    PyMem_Free(joined_path);
    if (!entry->name)
        goto error;

    if (path->fd != -1) {
        entry->path = entry->name;
        Py_INCREF(entry->path);
    }
    else if (!entry->path)
        goto error;

#ifdef HAVE_DIRENT_D_TYPE
    entry->d_type = d_type;
#endif
    entry->d_ino = d_ino;

    return (PyObject *)entry;

error:
    Py_XDECREF(entry);
    return NULL;
}

#endif


typedef struct {
    PyObject_HEAD
    path_t path;
#ifdef MS_WINDOWS
    HANDLE handle;
    WIN32_FIND_DATAW file_data;
    int first_time;
#else /* POSIX */
    DIR *dirp;
#endif
#ifdef HAVE_FDOPENDIR
    int fd;
#endif
} ScandirIterator;

#ifdef MS_WINDOWS

static int
ScandirIterator_is_closed(ScandirIterator *iterator)
{
    return iterator->handle == INVALID_HANDLE_VALUE;
}

static void
ScandirIterator_closedir(ScandirIterator *iterator)
{
    HANDLE handle = iterator->handle;

    if (handle == INVALID_HANDLE_VALUE)
        return;

    iterator->handle = INVALID_HANDLE_VALUE;
    
    FindClose(handle);
    
}

static PyObject *
ScandirIterator_iternext(ScandirIterator *iterator)
{
    WIN32_FIND_DATAW *file_data = &iterator->file_data;
    BOOL success;
    PyObject *entry;

    /* Happens if the iterator is iterated twice, or closed explicitly */
    if (iterator->handle == INVALID_HANDLE_VALUE)
        return NULL;

    while (1) {
        if (!iterator->first_time) {
            
            success = FindNextFileW(iterator->handle, file_data);
            
            if (!success) {
                /* Error or no more files */
                if (GetLastError() != ERROR_NO_MORE_FILES)
                    path_error(&iterator->path);
                break;
            }
        }
        iterator->first_time = 0;

        /* Skip over . and .. */
        if (wcscmp(file_data->cFileName, L".") != 0 &&
            wcscmp(file_data->cFileName, L"..") != 0)
        {
            PyObject *module = PyType_GetModule(Py_TYPE(iterator));
            entry = DirEntry_from_find_data(module, &iterator->path, file_data);
            if (!entry)
                break;
            return entry;
        }

        /* Loop till we get a non-dot directory or finish iterating */
    }

    /* Error or no more files */
    ScandirIterator_closedir(iterator);
    return NULL;
}

#else /* POSIX */

static int
ScandirIterator_is_closed(ScandirIterator *iterator)
{
    return !iterator->dirp;
}

static void
ScandirIterator_closedir(ScandirIterator *iterator)
{
    DIR *dirp = iterator->dirp;

    if (!dirp)
        return;

    iterator->dirp = NULL;
    
#ifdef HAVE_FDOPENDIR
    if (iterator->path.fd != -1)
        rewinddir(dirp);
#endif
    closedir(dirp);
    
    return;
}

static PyObject *
ScandirIterator_iternext(ScandirIterator *iterator)
{
    struct dirent *direntp;
    Py_ssize_t name_len;
    int is_dot;
    PyObject *entry;

    /* Happens if the iterator is iterated twice, or closed explicitly */
    if (!iterator->dirp)
        return NULL;

    while (1) {
        errno = 0;
        
        direntp = readdir(iterator->dirp);
        

        if (!direntp) {
            /* Error or no more files */
            if (errno != 0)
                path_error(&iterator->path);
            break;
        }

        /* Skip over . and .. */
        name_len = NAMLEN(direntp);
        is_dot = direntp->d_name[0] == '.' &&
                 (name_len == 1 || (direntp->d_name[1] == '.' && name_len == 2));
        if (!is_dot) {
            PyObject *module = PyType_GetModule(Py_TYPE(iterator));
            entry = DirEntry_from_posix_info(module,
                                             &iterator->path, direntp->d_name,
                                             name_len, direntp->d_ino
#ifdef HAVE_DIRENT_D_TYPE
                                             , direntp->d_type
#endif
                                            );
            if (!entry)
                break;
            return entry;
        }

        /* Loop till we get a non-dot directory or finish iterating */
    }

    /* Error or no more files */
    ScandirIterator_closedir(iterator);
    return NULL;
}

#endif

static PyObject *
ScandirIterator_close(ScandirIterator *self, PyObject *args)
{
    ScandirIterator_closedir(self);
    Py_RETURN_NONE;
}

static PyObject *
ScandirIterator_enter(PyObject *self, PyObject *args)
{
    Py_INCREF(self);
    return self;
}

static PyObject *
ScandirIterator_exit(ScandirIterator *self, PyObject *args)
{
    ScandirIterator_closedir(self);
    Py_RETURN_NONE;
}

static void
ScandirIterator_finalize(ScandirIterator *iterator)
{
    PyObject *error_type, *error_value, *error_traceback;

    /* Save the current exception, if any. */
    PyErr_Fetch(&error_type, &error_value, &error_traceback);

    if (!ScandirIterator_is_closed(iterator)) {
        ScandirIterator_closedir(iterator);
    }

    path_cleanup(&iterator->path);

    /* Restore the saved exception. */
    PyErr_Restore(error_type, error_value, error_traceback);
}

static void
ScandirIterator_dealloc(ScandirIterator *iterator)
{
    PyTypeObject *tp = Py_TYPE(iterator);
    if (PyObject_CallFinalizerFromDealloc((PyObject *)iterator) < 0)
        return;

    freefunc free_func = PyType_GetSlot(tp, Py_tp_free);
    free_func(iterator);
    Py_DECREF(tp);
}

static PyMethodDef ScandirIterator_methods[] = {
    {"__enter__", (PyCFunction)ScandirIterator_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)ScandirIterator_exit, METH_VARARGS},
    {"close", (PyCFunction)ScandirIterator_close, METH_NOARGS},
    {NULL}
};

static PyType_Slot ScandirIteratorType_slots[] = {
    {Py_tp_new, _disabled_new},
    {Py_tp_dealloc, ScandirIterator_dealloc},
    {Py_tp_finalize, ScandirIterator_finalize},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, ScandirIterator_iternext},
    {Py_tp_methods, ScandirIterator_methods},
    {0, 0},
};

static PyType_Spec ScandirIteratorType_spec = {
    MODNAME ".ScandirIterator",
    sizeof(ScandirIterator),
    0,
    // bpo-40549: Py_TPFLAGS_BASETYPE should not be used, since
    // PyType_GetModule(Py_TYPE(self)) doesn't work on a subclass instance.
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_FINALIZE,
    ScandirIteratorType_slots
};

/*[clinic input]
os.scandir

    path : path_t(nullable=True, allow_fd='PATH_HAVE_FDOPENDIR') = None

Return an iterator of DirEntry objects for given path.

path can be specified as either str, bytes, or a path-like object.  If path
is bytes, the names of yielded DirEntry objects will also be bytes; in
all other circumstances they will be str.

If path is None, uses the path='.'.
[clinic start generated code]*/

static PyObject *
os_scandir_impl(PyObject *module, path_t *path)
/*[clinic end generated code: output=6eb2668b675ca89e input=6bdd312708fc3bb0]*/
{
    ScandirIterator *iterator;
#ifdef MS_WINDOWS
    wchar_t *path_strW;
#else
    const char *path_str;
#ifdef HAVE_FDOPENDIR
    int fd = -1;
#endif
#endif

    if (PySys_Audit("os.scandir", "O",
                    path->object ? path->object : Py_None) < 0) {
        return NULL;
    }

    PyObject *ScandirIteratorType = get_posix_state(module)->ScandirIteratorType;
    iterator = PyObject_New(ScandirIterator, (PyTypeObject *)ScandirIteratorType);
    if (!iterator)
        return NULL;

#ifdef MS_WINDOWS
    iterator->handle = INVALID_HANDLE_VALUE;
#else
    iterator->dirp = NULL;
#endif

    memcpy(&iterator->path, path, sizeof(path_t));
    /* Move the ownership to iterator->path */
    path->object = NULL;
    path->cleanup = NULL;

#ifdef MS_WINDOWS
    iterator->first_time = 1;

    path_strW = join_path_filenameW(iterator->path.wide, L"*.*");
    if (!path_strW)
        goto error;

    
    iterator->handle = FindFirstFileW(path_strW, &iterator->file_data);
    

    PyMem_Free(path_strW);

    if (iterator->handle == INVALID_HANDLE_VALUE) {
        path_error(&iterator->path);
        goto error;
    }
#else /* POSIX */
    errno = 0;
#ifdef HAVE_FDOPENDIR
    if (path->fd != -1) {
        /* closedir() closes the FD, so we duplicate it */
        fd = _Py_dup(path->fd);
        if (fd == -1)
            goto error;

        
        iterator->dirp = fdopendir(fd);
        
    }
    else
#endif
    {
        if (iterator->path.narrow)
            path_str = iterator->path.narrow;
        else
            path_str = ".";

        
        iterator->dirp = opendir(path_str);
        
    }

    if (!iterator->dirp) {
        path_error(&iterator->path);
#ifdef HAVE_FDOPENDIR
        if (fd != -1) {
            
            close(fd);
            
        }
#endif
        goto error;
    }
#endif

    return (PyObject *)iterator;

error:
    Py_DECREF(iterator);
    return NULL;
}

/*
    Return the file system path representation of the object.

    If the object is str or bytes, then allow it to pass through with
    an incremented refcount. If the object defines __fspath__(), then
    return the result of that method. All other types raise a TypeError.
*/
PyObject *
PyOS_FSPath(PyObject *path)
{
    /* For error message reasons, this function is manually inlined in
       path_converter(). */
    PyObject *func = NULL;
    PyObject *path_repr = NULL;

    if (PyUnicode_Check(path) || PyBytes_Check(path)) {
        Py_INCREF(path);
        return path;
    }

    func = _PyObject_LookupSpecial(path, &PyId___fspath__);
    if (NULL == func) {
        return PyErr_Format(PyExc_TypeError,
                            "expected str, bytes or os.PathLike object, "
                            "not %.200s",
                            _PyType_Name(Py_TYPE(path)));
    }

    path_repr = _PyObject_CallNoArg(func);
    Py_DECREF(func);
    if (NULL == path_repr) {
        return NULL;
    }

    if (!(PyUnicode_Check(path_repr) || PyBytes_Check(path_repr))) {
        PyErr_Format(PyExc_TypeError,
                     "expected %.200s.__fspath__() to return str or bytes, "
                     "not %.200s", _PyType_Name(Py_TYPE(path)),
                     _PyType_Name(Py_TYPE(path_repr)));
        Py_DECREF(path_repr);
        return NULL;
    }

    return path_repr;
}

/*[clinic input]
os.fspath

    path: object

Return the file system path representation of the object.

If the object is str or bytes, then allow it to pass through as-is. If the
object defines __fspath__(), then return the result of that method. All other
types raise a TypeError.
[clinic start generated code]*/

static PyObject *
os_fspath_impl(PyObject *module, PyObject *path)
/*[clinic end generated code: output=c3c3b78ecff2914f input=e357165f7b22490f]*/
{
    return PyOS_FSPath(path);
}

static PyMethodDef posix_methods[] = {

    OS_STAT_METHODDEF
    OS_ACCESS_METHODDEF
    OS_CHDIR_METHODDEF
    OS_GETCWD_METHODDEF
    OS_GETCWDB_METHODDEF
    OS_LINK_METHODDEF
    OS_LISTDIR_METHODDEF
    OS_LSTAT_METHODDEF
    OS_MKDIR_METHODDEF
    OS_READLINK_METHODDEF
    OS_RENAME_METHODDEF
    OS_REPLACE_METHODDEF
    OS_RMDIR_METHODDEF
    OS_SYMLINK_METHODDEF
    OS_SYSTEM_METHODDEF
    OS_UNLINK_METHODDEF
    OS_REMOVE_METHODDEF
    OS__EXIT_METHODDEF
    OS_PLOCK_METHODDEF
    OS_OPEN_METHODDEF
    OS_CLOSE_METHODDEF
    OS_DEVICE_ENCODING_METHODDEF
    OS_LOCKF_METHODDEF
    OS_LSEEK_METHODDEF
    OS_READ_METHODDEF
    OS_WRITE_METHODDEF
    OS_FSTAT_METHODDEF
    OS_ISATTY_METHODDEF
    OS_FTRUNCATE_METHODDEF
    OS_TRUNCATE_METHODDEF
    OS_PUTENV_METHODDEF
    OS_UNSETENV_METHODDEF
    OS_STRERROR_METHODDEF
    OS_FCHDIR_METHODDEF
    OS_FSTATVFS_METHODDEF
    OS_STATVFS_METHODDEF
    OS_CONFSTR_METHODDEF
    OS_FPATHCONF_METHODDEF
    OS_PATHCONF_METHODDEF
    OS_ABORT_METHODDEF
    OS_GET_INHERITABLE_METHODDEF
    OS_SET_INHERITABLE_METHODDEF
    OS_GET_HANDLE_INHERITABLE_METHODDEF
    OS_SET_HANDLE_INHERITABLE_METHODDEF
    OS_GET_BLOCKING_METHODDEF
    OS_SET_BLOCKING_METHODDEF
    OS_SCANDIR_METHODDEF
    OS_FSPATH_METHODDEF
    {NULL,              NULL}            /* Sentinel */
};

static int
all_ins(PyObject *m)
{
#ifdef F_OK
    if (PyModule_AddIntMacro(m, F_OK)) return -1;
#endif
#ifdef R_OK
    if (PyModule_AddIntMacro(m, R_OK)) return -1;
#endif
#ifdef W_OK
    if (PyModule_AddIntMacro(m, W_OK)) return -1;
#endif
#ifdef X_OK
    if (PyModule_AddIntMacro(m, X_OK)) return -1;
#endif
#ifdef NGROUPS_MAX
    if (PyModule_AddIntMacro(m, NGROUPS_MAX)) return -1;
#endif
#ifdef TMP_MAX
    if (PyModule_AddIntMacro(m, TMP_MAX)) return -1;
#endif
#ifdef O_RDONLY
    if (PyModule_AddIntMacro(m, O_RDONLY)) return -1;
#endif
#ifdef O_WRONLY
    if (PyModule_AddIntMacro(m, O_WRONLY)) return -1;
#endif
#ifdef O_RDWR
    if (PyModule_AddIntMacro(m, O_RDWR)) return -1;
#endif
#ifdef O_NDELAY
    if (PyModule_AddIntMacro(m, O_NDELAY)) return -1;
#endif
#ifdef O_NONBLOCK
    if (PyModule_AddIntMacro(m, O_NONBLOCK)) return -1;
#endif
#ifdef O_APPEND
    if (PyModule_AddIntMacro(m, O_APPEND)) return -1;
#endif
#ifdef O_DSYNC
    if (PyModule_AddIntMacro(m, O_DSYNC)) return -1;
#endif
#ifdef O_RSYNC
    if (PyModule_AddIntMacro(m, O_RSYNC)) return -1;
#endif
#ifdef O_SYNC
    if (PyModule_AddIntMacro(m, O_SYNC)) return -1;
#endif
#ifdef O_NOCTTY
    if (PyModule_AddIntMacro(m, O_NOCTTY)) return -1;
#endif
#ifdef O_CREAT
    if (PyModule_AddIntMacro(m, O_CREAT)) return -1;
#endif
#ifdef O_EXCL
    if (PyModule_AddIntMacro(m, O_EXCL)) return -1;
#endif
#ifdef O_TRUNC
    if (PyModule_AddIntMacro(m, O_TRUNC)) return -1;
#endif
#ifdef O_BINARY
    if (PyModule_AddIntMacro(m, O_BINARY)) return -1;
#endif
#ifdef O_TEXT
    if (PyModule_AddIntMacro(m, O_TEXT)) return -1;
#endif
#ifdef O_XATTR
    if (PyModule_AddIntMacro(m, O_XATTR)) return -1;
#endif
#ifdef O_LARGEFILE
    if (PyModule_AddIntMacro(m, O_LARGEFILE)) return -1;
#endif
#ifndef __GNU__
#ifdef O_SHLOCK
    if (PyModule_AddIntMacro(m, O_SHLOCK)) return -1;
#endif
#ifdef O_EXLOCK
    if (PyModule_AddIntMacro(m, O_EXLOCK)) return -1;
#endif
#endif
#ifdef O_EXEC
    if (PyModule_AddIntMacro(m, O_EXEC)) return -1;
#endif
#ifdef O_SEARCH
    if (PyModule_AddIntMacro(m, O_SEARCH)) return -1;
#endif
#ifdef O_PATH
    if (PyModule_AddIntMacro(m, O_PATH)) return -1;
#endif
#ifdef O_TTY_INIT
    if (PyModule_AddIntMacro(m, O_TTY_INIT)) return -1;
#endif
#ifdef O_TMPFILE
    if (PyModule_AddIntMacro(m, O_TMPFILE)) return -1;
#endif
#ifdef PRIO_PROCESS
    if (PyModule_AddIntMacro(m, PRIO_PROCESS)) return -1;
#endif
#ifdef PRIO_PGRP
    if (PyModule_AddIntMacro(m, PRIO_PGRP)) return -1;
#endif
#ifdef PRIO_USER
    if (PyModule_AddIntMacro(m, PRIO_USER)) return -1;
#endif
#ifdef O_CLOEXEC
    if (PyModule_AddIntMacro(m, O_CLOEXEC)) return -1;
#endif
#ifdef O_ACCMODE
    if (PyModule_AddIntMacro(m, O_ACCMODE)) return -1;
#endif


#ifdef SEEK_HOLE
    if (PyModule_AddIntMacro(m, SEEK_HOLE)) return -1;
#endif
#ifdef SEEK_DATA
    if (PyModule_AddIntMacro(m, SEEK_DATA)) return -1;
#endif

/* MS Windows */
#ifdef O_NOINHERIT
    /* Don't inherit in child processes. */
    if (PyModule_AddIntMacro(m, O_NOINHERIT)) return -1;
#endif
#ifdef _O_SHORT_LIVED
    /* Optimize for short life (keep in memory). */
    /* MS forgot to define this one with a non-underscore form too. */
    if (PyModule_AddIntConstant(m, "O_SHORT_LIVED", _O_SHORT_LIVED)) return -1;
#endif
#ifdef O_TEMPORARY
    /* Automatically delete when last handle is closed. */
    if (PyModule_AddIntMacro(m, O_TEMPORARY)) return -1;
#endif
#ifdef O_RANDOM
    /* Optimize for random access. */
    if (PyModule_AddIntMacro(m, O_RANDOM)) return -1;
#endif
#ifdef O_SEQUENTIAL
    /* Optimize for sequential access. */
    if (PyModule_AddIntMacro(m, O_SEQUENTIAL)) return -1;
#endif

/* GNU extensions. */
#ifdef O_ASYNC
    /* Send a SIGIO signal whenever input or output
       becomes available on file descriptor */
    if (PyModule_AddIntMacro(m, O_ASYNC)) return -1;
#endif
#ifdef O_DIRECT
    /* Direct disk access. */
    if (PyModule_AddIntMacro(m, O_DIRECT)) return -1;
#endif
#ifdef O_DIRECTORY
    /* Must be a directory.      */
    if (PyModule_AddIntMacro(m, O_DIRECTORY)) return -1;
#endif
#ifdef O_NOFOLLOW
    /* Do not follow links.      */
    if (PyModule_AddIntMacro(m, O_NOFOLLOW)) return -1;
#endif
#ifdef O_NOLINKS
    /* Fails if link count of the named file is greater than 1 */
    if (PyModule_AddIntMacro(m, O_NOLINKS)) return -1;
#endif
#ifdef O_NOATIME
    /* Do not update the access time. */
    if (PyModule_AddIntMacro(m, O_NOATIME)) return -1;
#endif

    /* These come from sysexits.h */
#ifdef EX_OK
    if (PyModule_AddIntMacro(m, EX_OK)) return -1;
#endif /* EX_OK */
#ifdef EX_USAGE
    if (PyModule_AddIntMacro(m, EX_USAGE)) return -1;
#endif /* EX_USAGE */
#ifdef EX_DATAERR
    if (PyModule_AddIntMacro(m, EX_DATAERR)) return -1;
#endif /* EX_DATAERR */
#ifdef EX_NOINPUT
    if (PyModule_AddIntMacro(m, EX_NOINPUT)) return -1;
#endif /* EX_NOINPUT */
#ifdef EX_NOUSER
    if (PyModule_AddIntMacro(m, EX_NOUSER)) return -1;
#endif /* EX_NOUSER */
#ifdef EX_NOHOST
    if (PyModule_AddIntMacro(m, EX_NOHOST)) return -1;
#endif /* EX_NOHOST */
#ifdef EX_UNAVAILABLE
    if (PyModule_AddIntMacro(m, EX_UNAVAILABLE)) return -1;
#endif /* EX_UNAVAILABLE */
#ifdef EX_SOFTWARE
    if (PyModule_AddIntMacro(m, EX_SOFTWARE)) return -1;
#endif /* EX_SOFTWARE */
#ifdef EX_OSERR
    if (PyModule_AddIntMacro(m, EX_OSERR)) return -1;
#endif /* EX_OSERR */
#ifdef EX_OSFILE
    if (PyModule_AddIntMacro(m, EX_OSFILE)) return -1;
#endif /* EX_OSFILE */
#ifdef EX_CANTCREAT
    if (PyModule_AddIntMacro(m, EX_CANTCREAT)) return -1;
#endif /* EX_CANTCREAT */
#ifdef EX_IOERR
    if (PyModule_AddIntMacro(m, EX_IOERR)) return -1;
#endif /* EX_IOERR */
#ifdef EX_TEMPFAIL
    if (PyModule_AddIntMacro(m, EX_TEMPFAIL)) return -1;
#endif /* EX_TEMPFAIL */
#ifdef EX_PROTOCOL
    if (PyModule_AddIntMacro(m, EX_PROTOCOL)) return -1;
#endif /* EX_PROTOCOL */
#ifdef EX_NOPERM
    if (PyModule_AddIntMacro(m, EX_NOPERM)) return -1;
#endif /* EX_NOPERM */
#ifdef EX_CONFIG
    if (PyModule_AddIntMacro(m, EX_CONFIG)) return -1;
#endif /* EX_CONFIG */
#ifdef EX_NOTFOUND
    if (PyModule_AddIntMacro(m, EX_NOTFOUND)) return -1;
#endif /* EX_NOTFOUND */

    /* statvfs */
#ifdef ST_RDONLY
    if (PyModule_AddIntMacro(m, ST_RDONLY)) return -1;
#endif /* ST_RDONLY */
#ifdef ST_NOSUID
    if (PyModule_AddIntMacro(m, ST_NOSUID)) return -1;
#endif /* ST_NOSUID */

       /* GNU extensions */
#ifdef ST_NODEV
    if (PyModule_AddIntMacro(m, ST_NODEV)) return -1;
#endif /* ST_NODEV */
#ifdef ST_NOEXEC
    if (PyModule_AddIntMacro(m, ST_NOEXEC)) return -1;
#endif /* ST_NOEXEC */
#ifdef ST_SYNCHRONOUS
    if (PyModule_AddIntMacro(m, ST_SYNCHRONOUS)) return -1;
#endif /* ST_SYNCHRONOUS */
#ifdef ST_MANDLOCK
    if (PyModule_AddIntMacro(m, ST_MANDLOCK)) return -1;
#endif /* ST_MANDLOCK */
#ifdef ST_WRITE
    if (PyModule_AddIntMacro(m, ST_WRITE)) return -1;
#endif /* ST_WRITE */
#ifdef ST_APPEND
    if (PyModule_AddIntMacro(m, ST_APPEND)) return -1;
#endif /* ST_APPEND */
#ifdef ST_NOATIME
    if (PyModule_AddIntMacro(m, ST_NOATIME)) return -1;
#endif /* ST_NOATIME */
#ifdef ST_NODIRATIME
    if (PyModule_AddIntMacro(m, ST_NODIRATIME)) return -1;
#endif /* ST_NODIRATIME */
#ifdef ST_RELATIME
    if (PyModule_AddIntMacro(m, ST_RELATIME)) return -1;
#endif /* ST_RELATIME */

    /* constants for lockf */
#ifdef F_LOCK
    if (PyModule_AddIntMacro(m, F_LOCK)) return -1;
#endif
#ifdef F_TLOCK
    if (PyModule_AddIntMacro(m, F_TLOCK)) return -1;
#endif
#ifdef F_ULOCK
    if (PyModule_AddIntMacro(m, F_ULOCK)) return -1;
#endif
#ifdef F_TEST
    if (PyModule_AddIntMacro(m, F_TEST)) return -1;
#endif

#ifdef RWF_DSYNC
    if (PyModule_AddIntConstant(m, "RWF_DSYNC", RWF_DSYNC)) return -1;
#endif
#ifdef RWF_HIPRI
    if (PyModule_AddIntConstant(m, "RWF_HIPRI", RWF_HIPRI)) return -1;
#endif
#ifdef RWF_SYNC
    if (PyModule_AddIntConstant(m, "RWF_SYNC", RWF_SYNC)) return -1;
#endif
#ifdef RWF_NOWAIT
    if (PyModule_AddIntConstant(m, "RWF_NOWAIT", RWF_NOWAIT)) return -1;
#endif
#ifdef RWF_APPEND
    if (PyModule_AddIntConstant(m, "RWF_APPEND", RWF_APPEND)) return -1;
#endif

#if HAVE_DECL_RTLD_LAZY
    if (PyModule_AddIntMacro(m, RTLD_LAZY)) return -1;
#endif
#if HAVE_DECL_RTLD_NOW
    if (PyModule_AddIntMacro(m, RTLD_NOW)) return -1;
#endif
#if HAVE_DECL_RTLD_GLOBAL
    if (PyModule_AddIntMacro(m, RTLD_GLOBAL)) return -1;
#endif
#if HAVE_DECL_RTLD_LOCAL
    if (PyModule_AddIntMacro(m, RTLD_LOCAL)) return -1;
#endif
#if HAVE_DECL_RTLD_NODELETE
    if (PyModule_AddIntMacro(m, RTLD_NODELETE)) return -1;
#endif
#if HAVE_DECL_RTLD_NOLOAD
    if (PyModule_AddIntMacro(m, RTLD_NOLOAD)) return -1;
#endif
#if HAVE_DECL_RTLD_DEEPBIND
    if (PyModule_AddIntMacro(m, RTLD_DEEPBIND)) return -1;
#endif
#if HAVE_DECL_RTLD_MEMBER
    if (PyModule_AddIntMacro(m, RTLD_MEMBER)) return -1;
#endif

#if defined(__APPLE__)
    if (PyModule_AddIntConstant(m, "_COPYFILE_DATA", COPYFILE_DATA)) return -1;
#endif

    return 0;
}


static int
posixmodule_exec(PyObject *m)
{
    _posixstate *state = get_posix_state(m);

    /* Initialize environ dictionary */
    PyObject *v = convertenviron();
    Py_XINCREF(v);
    if (v == NULL || PyModule_AddObject(m, "environ", v) != 0)
        return -1;
    Py_DECREF(v);

    if (all_ins(m))
        return -1;

    if (setup_confname_tables(m))
        return -1;

    Py_INCREF(PyExc_OSError);
    PyModule_AddObject(m, "error", PyExc_OSError);

    stat_result_desc.name = "os.stat_result"; /* see issue #19209 */
    stat_result_desc.fields[7].name = PyStructSequence_UnnamedField;
    stat_result_desc.fields[8].name = PyStructSequence_UnnamedField;
    stat_result_desc.fields[9].name = PyStructSequence_UnnamedField;
    PyObject *StatResultType = (PyObject *)PyStructSequence_NewType(&stat_result_desc);
    if (StatResultType == NULL) {
        return -1;
    }
    Py_INCREF(StatResultType);
    PyModule_AddObject(m, "stat_result", StatResultType);
    state->StatResultType = StatResultType;
    structseq_new = ((PyTypeObject *)StatResultType)->tp_new;
    ((PyTypeObject *)StatResultType)->tp_new = statresult_new;

    statvfs_result_desc.name = "os.statvfs_result"; /* see issue #19209 */
    PyObject *StatVFSResultType = (PyObject *)PyStructSequence_NewType(&statvfs_result_desc);
    if (StatVFSResultType == NULL) {
        return -1;
    }
    Py_INCREF(StatVFSResultType);
    PyModule_AddObject(m, "statvfs_result", StatVFSResultType);
    state->StatVFSResultType = StatVFSResultType;
#ifdef NEED_TICKS_PER_SECOND
#  if defined(HZ)
    ticks_per_second = HZ;
#  else
    ticks_per_second = 60; /* magic fallback value; may be bogus */
#  endif
#endif

    /* initialize scandir types */
    PyObject *ScandirIteratorType = PyType_FromModuleAndSpec(m, &ScandirIteratorType_spec, NULL);
    if (ScandirIteratorType == NULL) {
        return -1;
    }
    state->ScandirIteratorType = ScandirIteratorType;

    PyObject *DirEntryType = PyType_FromModuleAndSpec(m, &DirEntryType_spec, NULL);
    if (DirEntryType == NULL) {
        return -1;
    }
    Py_INCREF(DirEntryType);
    PyModule_AddObject(m, "DirEntry", DirEntryType);
    state->DirEntryType = DirEntryType;

#ifdef __APPLE__
    /*
     * Step 2 of weak-linking support on Mac OS X.
     *
     * The code below removes functions that are not available on the
     * currently active platform.
     *
     * This block allow one to use a python binary that was build on
     * OSX 10.4 on OSX 10.3, without losing access to new APIs on
     * OSX 10.4.
     */
#ifdef HAVE_FSTATVFS
    if (fstatvfs == NULL) {
        if (PyObject_DelAttrString(m, "fstatvfs") == -1) {
            return -1;
        }
    }
#endif /* HAVE_FSTATVFS */

#ifdef HAVE_STATVFS
    if (statvfs == NULL) {
        if (PyObject_DelAttrString(m, "statvfs") == -1) {
            return -1;
        }
    }
#endif /* HAVE_STATVFS */

# ifdef HAVE_LCHOWN
    if (lchown == NULL) {
        if (PyObject_DelAttrString(m, "lchown") == -1) {
            return -1;
        }
    }
#endif /* HAVE_LCHOWN */


#endif /* __APPLE__ */

    if ((state->billion = PyLong_FromLong(1000000000)) == NULL)
        return -1;
    state->st_mode = PyUnicode_InternFromString("st_mode");
    if (state->st_mode == NULL)
        return -1;

    /* suppress "function not used" warnings */
    {
    int ignored;
    fd_specified("", -1);
    follow_symlinks_specified("", 1);
    dir_fd_and_follow_symlinks_invalid("chmod", DEFAULT_DIR_FD, 1);
    dir_fd_converter(Py_None, &ignored);
    dir_fd_unavailable(Py_None, &ignored);
    }

    return 0;
}


static PyModuleDef_Slot posixmodile_slots[] = {
    {Py_mod_exec, posixmodule_exec},
    {0, NULL}
};

static struct PyModuleDef posixmodule = {
    PyModuleDef_HEAD_INIT,
    .m_name = MODNAME,
    .m_doc = posix__doc__,
    .m_size = sizeof(_posixstate),
    .m_methods = posix_methods,
    .m_slots = posixmodile_slots,
    .m_traverse = _posix_traverse,
    .m_clear = _posix_clear,
    .m_free = _posix_free,
};

PyMODINIT_FUNC
INITFUNC(void)
{
    return PyModuleDef_Init(&posixmodule);
}

#ifdef __cplusplus
}
#endif
