/* Return the initial module search path. */

#include "Python.h"
#include "pycore_fileutils.h"
#include "pycore_initconfig.h"
#include "pycore_pathconfig.h"
#include "osdefs.h"               // DELIM

#include <sys/types.h>
#include <string.h>

/* Search in some common locations for the associated Python libraries.
 *
 * Two directories must be found, the platform independent directory
 * (prefix), containing the common .py and .pyc files, and the platform
 * dependent directory (exec_prefix), containing the shared library
 * modules.  Note that prefix and exec_prefix can be the same directory,
 * but for some installations, they are different.
 *
 * Py_GetPath() carries out separate searches for prefix and exec_prefix.
 * Each search tries a number of different locations until a ``landmark''
 * file or directory is found.  If no prefix or exec_prefix is found, a
 * warning message is issued and the preprocessor defined PREFIX and
 * EXEC_PREFIX are used (even though they will not work); python carries on
 * as best as is possible, but most imports will fail.
 *
 * Before any searches are done, the location of the executable is
 * determined.  If argv[0] has one or more slashes in it, it is used
 * unchanged.  Otherwise, it must have been invoked from the shell's path,
 * so we search $PATH for the named executable and use that.  If the
 * executable was not found on $PATH (or there was no $PATH environment
 * variable), the original argv[0] string is used.
 *
 * Next, the executable location is examined to see if it is a symbolic
 * link.  If so, the link is chased (correctly interpreting a relative
 * pathname if one is found) and the directory of the link target is used.
 *
 * Finally, argv0_path is set to the directory containing the executable
 * (i.e. the last component is stripped).
 *
 * With argv0_path in hand, we perform a number of steps.  The same steps
 * are performed for prefix and for exec_prefix, but with a different
 * landmark.
 *
 * Step 1. Are we running python out of the build directory?  This is
 * checked by looking for a different kind of landmark relative to
 * argv0_path.  For prefix, the landmark's path is derived from the VPATH
 * preprocessor variable (taking into account that its value is almost, but
 * not quite, what we need).  For exec_prefix, the landmark is
 * pybuilddir.txt.  If the landmark is found, we're done.
 *
 * For the remaining steps, the prefix landmark will always be
 * lib/python$VERSION/os.py and the exec_prefix will always be
 * lib/python$VERSION/lib-dynload, where $VERSION is Python's version
 * number as supplied by the Makefile.  Note that this means that no more
 * build directory checking is performed; if the first step did not find
 * the landmarks, the assumption is that python is running from an
 * installed setup.
 *
 * Step 2. See if the $PYTHONHOME environment variable points to the
 * installed location of the Python libraries.  If $PYTHONHOME is set, then
 * it points to prefix and exec_prefix.  $PYTHONHOME can be a single
 * directory, which is used for both, or the prefix and exec_prefix
 * directories separated by a colon.
 *
 * Step 3. Try to find prefix and exec_prefix relative to argv0_path,
 * backtracking up the path until it is exhausted.  This is the most common
 * step to succeed.  Note that if prefix and exec_prefix are different,
 * exec_prefix is more likely to be found; however if exec_prefix is a
 * subdirectory of prefix, both will be found.
 *
 * Step 4. Search the directories pointed to by the preprocessor variables
 * PREFIX and EXEC_PREFIX.  These are supplied by the Makefile but can be
 * passed in as options to the configure script.
 *
 * That's it!
 *
 * Well, almost.  Once we have determined prefix and exec_prefix, the
 * preprocessor variable PYTHONPATH is used to construct a path.  Each
 * relative path on PYTHONPATH is prefixed with prefix.  Then the directory
 * containing the shared library modules is appended.  The environment
 * variable $PYTHONPATH is inserted in front of it all.  Finally, the
 * prefix and exec_prefix globals are tweaked so they reflect the values
 * expected by other code, by stripping the "lib/python$VERSION/..." stuff
 * off.  If either points to the build directory, the globals are reset to
 * the corresponding preprocessor variables (so sys.prefix will reflect the
 * installation location, even though sys.path points into the build
 * directory).  This seems to make more sense given that currently the only
 * known use of sys.prefix and sys.exec_prefix is for the ILU installation
 * process to find the installed Python tree.
 *
 * An embedding application can use Py_SetPath() to override all of
 * these automatic path computations.
 *
 * NOTE: Windows MSVC builds use PC/getpathp.c instead!
 */

#ifdef __cplusplus
extern "C" {
#endif


#if (!defined(PREFIX) || !defined(EXEC_PREFIX) \
        || !defined(VERSION) || !defined(VPATH))
#error "PREFIX, EXEC_PREFIX, VERSION and VPATH macros must be defined"
#endif

#ifndef LANDMARK
#define LANDMARK "os.py"
#endif

#define BUILD_LANDMARK "Modules/Setup.local"

#define DECODE_LOCALE_ERR(NAME, LEN) \
    ((LEN) == (size_t)-2) \
     ? _PyStatus_ERR("cannot decode " NAME) \
     : _PyStatus_NO_MEMORY()

#define PATHLEN_ERR() _PyStatus_ERR("path configuration: path too long")

typedef struct {
    char *path_env;                 /* PATH environment variable */

    char *pythonpath_macro;         /* PYTHONPATH macro */
    char *prefix_macro;             /* PREFIX macro */
    char *exec_prefix_macro;        /* EXEC_PREFIX macro */
    char *vpath_macro;              /* VPATH macro */

    char *lib_python;               /* <platlibdir> / "pythonX.Y" */

    int prefix_found;         /* found platform independent libraries? */
    int exec_prefix_found;    /* found the platform dependent libraries? */

    int warnings;
    const char *pythonpath_env;
    const char *platlibdir;

    char *argv0_path;
    char *zip_path;
    char *prefix;
    char *exec_prefix;
} PyCalculatePath;

static const char delimiter[2] = {DELIM, '\0'};
static const char separator[2] = {SEP, '\0'};


/* Get file status. Encode the path to the locale encoding. */
static int
_Py_cstat(const char* path, struct stat *buf)
{
  return stat(path, buf);
}


static void
reduce(char *dir)
{
    size_t i = strlen(dir);
    while (i > 0 && dir[i] != SEP) {
        --i;
    }
    dir[i] = '\0';
}


/* Is file, not directory */
static int
isfile(const char *filename)
{
    struct stat buf;
    if (_Py_cstat(filename, &buf) != 0) {
        return 0;
    }
    if (!S_ISREG(buf.st_mode)) {
        return 0;
    }
    return 1;
}


/* Is executable file */
static int
isxfile(const char *filename)
{
    struct stat buf;
    if (_Py_cstat(filename, &buf) != 0) {
        return 0;
    }
    if (!S_ISREG(buf.st_mode)) {
        return 0;
    }
    if ((buf.st_mode & 0111) == 0) {
        return 0;
    }
    return 1;
}


/* Is directory */
static int
isdir(const char *filename)
{
    struct stat buf;
    if (_Py_cstat(filename, &buf) != 0) {
        return 0;
    }
    if (!S_ISDIR(buf.st_mode)) {
        return 0;
    }
    return 1;
}


/* Add a path component, by appending stuff to buffer.
   buflen: 'buffer' length in characters including trailing NUL.

   If path2 is empty:

   - if path doesn't end with SEP and is not empty, add SEP to path
   - otherwise, do nothing. */
static PyStatus
joinpath(char *path, const char *path2, size_t path_len)
{
    size_t n;
    if (!_Py_isabs(path2)) {
        n = strlen(path);
        if (n >= path_len) {
            return PATHLEN_ERR();
        }

        if (n > 0 && path[n-1] != SEP) {
            path[n++] = SEP;
        }
    }
    else {
        n = 0;
    }

    size_t k = strlen(path2);
    if (n + k >= path_len) {
        return PATHLEN_ERR();
    }
    strncpy(path + n, path2, k);
    path[n + k] = '\0';

    return _PyStatus_OK();
}


static char*
substring(const char *str, size_t len)
{
  char *substr = PyMem_Malloc((len + 1));
    if (substr == NULL) {
        return NULL;
    }

    if (len) {
      memcpy(substr, str, len);
    }
    substr[len] = '\0';
    return substr;
}


static char*
joinpath2(const char *path, const char *path2)
{
    if (_Py_isabs(path2)) {
        return strdup(path2);
    }

    size_t len = strlen(path);
    int add_sep = (len > 0 && path[len - 1] != SEP);
    len += add_sep;
    len += strlen(path2);

    char *new_path = PyMem_Malloc((len + 1));
    if (new_path == NULL) {
        return NULL;
    }

    strcpy(new_path, path);
    if (add_sep) {
        strcat(new_path, separator);
    }
    strcat(new_path, path2);
    return new_path;
}


static inline int
safe_strcpy(char *dst, const char *src, size_t n)
{
    size_t srclen = strlen(src);
    if (n <= srclen) {
        dst[0] = '\0';
        return -1;
    }
    memcpy(dst, src, (srclen + 1));
    return 0;
}


/* copy_absolute requires that path be allocated at least
   'abs_path_len' characters (including trailing NUL). */
static PyStatus
copy_absolute(char *abs_path, const char *path, size_t abs_path_len)
{
    if (_Py_isabs(path)) {
        if (safe_strcpy(abs_path, path, abs_path_len) < 0) {
            return PATHLEN_ERR();
        }
    }
    else {
        if (!_Py_getcwd(abs_path, abs_path_len)) {
            /* unable to get the current directory */
            if (safe_strcpy(abs_path, path, abs_path_len) < 0) {
                return PATHLEN_ERR();
            }
            return _PyStatus_OK();
        }
        if (path[0] == '.' && path[1] == SEP) {
            path += 2;
        }
        PyStatus status = joinpath(abs_path, path, abs_path_len);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }
    return _PyStatus_OK();
}


/* path_len: path length in characters including trailing NUL */
static PyStatus
absolutize(char **path_p)
{
    assert(!_Py_isabs(*path_p));

    char abs_path[MAXPATHLEN+1];
    char *path = *path_p;

    PyStatus status = copy_absolute(abs_path, path, Py_ARRAY_LENGTH(abs_path));
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    PyMem_Free(*path_p);
    *path_p = strdup(abs_path);
    if (*path_p == NULL) {
        return _PyStatus_NO_MEMORY();
    }
    return _PyStatus_OK();
}


/* Is module -- check for .pyc too */
static PyStatus
ismodule(const char *path, int *result)
{
    char *filename = joinpath2(path, LANDMARK);
    if (filename == NULL) {
        return _PyStatus_NO_MEMORY();
    }

    if (isfile(filename)) {
        PyMem_Free(filename);
        *result = 1;
        return _PyStatus_OK();
    }

    /* Check for the compiled version of prefix. */
    size_t len = strlen(filename);
    char *pyc = PyMem_Malloc((len + 2));
    if (pyc == NULL) {
        PyMem_Free(filename);
        return _PyStatus_NO_MEMORY();
    }

    memcpy(pyc, filename, len);
    pyc[len] = 'c';
    pyc[len + 1] = '\0';
    *result = isfile(pyc);

    PyMem_Free(filename);
    PyMem_Free(pyc);

    return _PyStatus_OK();
}

/* search_for_prefix requires that argv0_path be no more than MAXPATHLEN
   bytes long.
*/
static PyStatus
search_for_prefix(PyCalculatePath *calculate, _PyPathConfig *pathconfig,
                  char *prefix, size_t prefix_len, int *found)
{
    PyStatus status;

    /* If PYTHONHOME is set, we believe it unconditionally */
    if (pathconfig->home) {
        /* Path: <home> / <lib_python> */
        if (safe_strcpy(prefix, pathconfig->home, prefix_len) < 0) {
            return PATHLEN_ERR();
        }
        char *delim = strchr(prefix, DELIM);
        if (delim) {
            *delim = '\0';
        }
        status = joinpath(prefix, calculate->lib_python, prefix_len);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
        *found = 1;
        return _PyStatus_OK();
    }

    /* Check to see if argv0_path is in the build directory

       Path: <argv0_path> / <BUILD_LANDMARK define> */
    char *path = joinpath2(calculate->argv0_path, BUILD_LANDMARK);
    if (path == NULL) {
        return _PyStatus_NO_MEMORY();
    }

    int is_build_dir = isfile(path);
    PyMem_Free(path);

    if (is_build_dir) {
        /* argv0_path is the build directory (BUILD_LANDMARK exists),
           now also check LANDMARK using ismodule(). */

        /* Path: <argv0_path> / <VPATH macro> / Lib */
        /* or if VPATH is empty: <argv0_path> / Lib */
        if (safe_strcpy(prefix, calculate->argv0_path, prefix_len) < 0) {
            return PATHLEN_ERR();
        }

        status = joinpath(prefix, calculate->vpath_macro, prefix_len);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }

        status = joinpath(prefix, "Lib", prefix_len);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }

        int module;
        status = ismodule(prefix, &module);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
        if (module) {
            /* BUILD_LANDMARK and LANDMARK found */
            *found = -1;
            return _PyStatus_OK();
        }
    }

    /* Search from argv0_path, until root is found */
    status = copy_absolute(prefix, calculate->argv0_path, prefix_len);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    do {
        /* Path: <argv0_path or substring> / <lib_python> / LANDMARK */
        size_t n = strlen(prefix);
        status = joinpath(prefix, calculate->lib_python, prefix_len);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }

        int module;
        status = ismodule(prefix, &module);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
        if (module) {
            *found = 1;
            return _PyStatus_OK();
        }
        prefix[n] = '\0';
        reduce(prefix);
    } while (prefix[0]);

    /* Look at configure's PREFIX.
       Path: <PREFIX macro> / <lib_python> / LANDMARK */
    if (safe_strcpy(prefix, calculate->prefix_macro, prefix_len) < 0) {
        return PATHLEN_ERR();
    }
    status = joinpath(prefix, calculate->lib_python, prefix_len);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    int module;
    status = ismodule(prefix, &module);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    if (module) {
        *found = 1;
        return _PyStatus_OK();
    }

    /* Fail */
    *found = 0;
    return _PyStatus_OK();
}


static PyStatus
calculate_prefix(PyCalculatePath *calculate, _PyPathConfig *pathconfig)
{
    char prefix[MAXPATHLEN+1];
    memset(prefix, 0, sizeof(prefix));
    size_t prefix_len = Py_ARRAY_LENGTH(prefix);

    PyStatus status;
    status = search_for_prefix(calculate, pathconfig,
                               prefix, prefix_len,
                               &calculate->prefix_found);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    if (!calculate->prefix_found) {
        if (calculate->warnings) {
            fprintf(stderr,
                "Could not find platform independent libraries <prefix>\n");
        }

        calculate->prefix = joinpath2(calculate->prefix_macro,
                                      calculate->lib_python);
    }
    else {
        calculate->prefix = strdup(prefix);
    }

    if (calculate->prefix == NULL) {
        return _PyStatus_NO_MEMORY();
    }
    return _PyStatus_OK();
}


static PyStatus
calculate_set_prefix(PyCalculatePath *calculate, _PyPathConfig *pathconfig)
{
    /* Reduce prefix and exec_prefix to their essence,
     * e.g. /usr/local/lib/python1.5 is reduced to /usr/local.
     * If we're loading relative to the build directory,
     * return the compiled-in defaults instead.
     */
    if (calculate->prefix_found > 0) {
        char *prefix = strdup(calculate->prefix);
        if (prefix == NULL) {
            return _PyStatus_NO_MEMORY();
        }

        reduce(prefix);
        reduce(prefix);
        if (prefix[0]) {
            pathconfig->prefix = prefix;
        }
        else {
            PyMem_Free(prefix);

            /* The prefix is the root directory, but reduce() chopped
               off the "/". */
            pathconfig->prefix = strdup(separator);
            if (pathconfig->prefix == NULL) {
                return _PyStatus_NO_MEMORY();
            }
        }
    }
    else {
        pathconfig->prefix = strdup(calculate->prefix_macro);
        if (pathconfig->prefix == NULL) {
            return _PyStatus_NO_MEMORY();
        }
    }
    return _PyStatus_OK();
}


/* search_for_exec_prefix requires that argv0_path be no more than
   MAXPATHLEN bytes long.
*/
static PyStatus
search_for_exec_prefix(PyCalculatePath *calculate, _PyPathConfig *pathconfig,
                       char *exec_prefix, size_t exec_prefix_len,
                       int *found)
{
    PyStatus status;

    /* If PYTHONHOME is set, we believe it unconditionally */
    if (pathconfig->home) {
        /* Path: <home> / <lib_python> / "lib-dynload" */
        char *delim = strchr(pathconfig->home, DELIM);
        if (delim) {
            if (safe_strcpy(exec_prefix, delim+1, exec_prefix_len) < 0) {
                return PATHLEN_ERR();
            }
        }
        else {
            if (safe_strcpy(exec_prefix, pathconfig->home, exec_prefix_len) < 0) {
                return PATHLEN_ERR();
            }
        }
        status = joinpath(exec_prefix, calculate->lib_python, exec_prefix_len);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
        status = joinpath(exec_prefix, "lib-dynload", exec_prefix_len);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
        *found = 1;
        return _PyStatus_OK();
    }
    
    /* Search from argv0_path, until root is found */
    status = copy_absolute(exec_prefix, calculate->argv0_path, exec_prefix_len);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    do {
        /* Path: <argv0_path or substring> / <lib_python> / "lib-dynload" */
        size_t n = strlen(exec_prefix);
        status = joinpath(exec_prefix, calculate->lib_python, exec_prefix_len);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
        status = joinpath(exec_prefix, "lib-dynload", exec_prefix_len);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
        if (isdir(exec_prefix)) {
            *found = 1;
            return _PyStatus_OK();
        }
        exec_prefix[n] = '\0';
        reduce(exec_prefix);
    } while (exec_prefix[0]);

    /* Look at configure's EXEC_PREFIX.

       Path: <EXEC_PREFIX macro> / <lib_python> / "lib-dynload" */
    if (safe_strcpy(exec_prefix, calculate->exec_prefix_macro, exec_prefix_len) < 0) {
        return PATHLEN_ERR();
    }
    status = joinpath(exec_prefix, calculate->lib_python, exec_prefix_len);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    status = joinpath(exec_prefix, "lib-dynload", exec_prefix_len);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    if (isdir(exec_prefix)) {
        *found = 1;
        return _PyStatus_OK();
    }

    /* Fail */
    *found = 0;
    return _PyStatus_OK();
}


static PyStatus
calculate_exec_prefix(PyCalculatePath *calculate, _PyPathConfig *pathconfig)
{
    PyStatus status;
    char exec_prefix[MAXPATHLEN+1];
    memset(exec_prefix, 0, sizeof(exec_prefix));
    size_t exec_prefix_len = Py_ARRAY_LENGTH(exec_prefix);

    status = search_for_exec_prefix(calculate, pathconfig,
                                    exec_prefix, exec_prefix_len,
                                    &calculate->exec_prefix_found);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    if (!calculate->exec_prefix_found) {
        if (calculate->warnings) {
            fprintf(stderr,
                "Could not find platform dependent libraries <exec_prefix>\n");
        }

        /* <platlibdir> / "lib-dynload" */
        char *lib_dynload = joinpath2(calculate->platlibdir,
                                         "lib-dynload");
        if (lib_dynload == NULL) {
            return _PyStatus_NO_MEMORY();
        }

        calculate->exec_prefix = joinpath2(calculate->exec_prefix_macro,
                                           lib_dynload);
        PyMem_Free(lib_dynload);

        if (calculate->exec_prefix == NULL) {
            return _PyStatus_NO_MEMORY();
        }
    }
    else {
        /* If we found EXEC_PREFIX do *not* reduce it!  (Yet.) */
        calculate->exec_prefix = strdup(exec_prefix);
        if (calculate->exec_prefix == NULL) {
            return _PyStatus_NO_MEMORY();
        }
    }
    return _PyStatus_OK();
}


static PyStatus
calculate_set_exec_prefix(PyCalculatePath *calculate,
                          _PyPathConfig *pathconfig)
{
    if (calculate->exec_prefix_found > 0) {
        char *exec_prefix = strdup(calculate->exec_prefix);
        if (exec_prefix == NULL) {
            return _PyStatus_NO_MEMORY();
        }

        reduce(exec_prefix);
        reduce(exec_prefix);
        reduce(exec_prefix);

        if (exec_prefix[0]) {
            pathconfig->exec_prefix = exec_prefix;
        }
        else {
            /* empty string: use SEP instead */
            PyMem_Free(exec_prefix);

            /* The exec_prefix is the root directory, but reduce() chopped
               off the "/". */
            pathconfig->exec_prefix = strdup(separator);
            if (pathconfig->exec_prefix == NULL) {
                return _PyStatus_NO_MEMORY();
            }
        }
    }
    else {
        pathconfig->exec_prefix = strdup(calculate->exec_prefix_macro);
        if (pathconfig->exec_prefix == NULL) {
            return _PyStatus_NO_MEMORY();
        }
    }
    return _PyStatus_OK();
}


/* Similar to shutil.which().
   If found, write the path into *abs_path_p. */
static PyStatus
calculate_which(const char *path_env, char *program_name,
                char **abs_path_p)
{
    while (1) {
        char *delim = strchr(path_env, DELIM);
        char *abs_path;

        if (delim) {
            char *path = substring(path_env, delim - path_env);
            if (path == NULL) {
                return _PyStatus_NO_MEMORY();
            }
            abs_path = joinpath2(path, program_name);
            PyMem_Free(path);
        }
        else {
            abs_path = joinpath2(path_env, program_name);
        }

        if (abs_path == NULL) {
            return _PyStatus_NO_MEMORY();
        }

        if (isxfile(abs_path)) {
            *abs_path_p = abs_path;
            return _PyStatus_OK();
        }
        PyMem_Free(abs_path);

        if (!delim) {
            break;
        }
        path_env = delim + 1;
    }

    /* not found */
    return _PyStatus_OK();
}


static PyStatus
calculate_program_impl(PyCalculatePath *calculate, _PyPathConfig *pathconfig)
{
    assert(pathconfig->program_full_path == NULL);

    PyStatus status;

    /* If there is no slash in the argv0 path, then we have to
     * assume python is on the user's $PATH, since there's no
     * other way to find a directory to start the search from.  If
     * $PATH isn't exported, you lose.
     */
    if (strchr(pathconfig->program_name, SEP)) {
        pathconfig->program_full_path = strdup(pathconfig->program_name);
        if (pathconfig->program_full_path == NULL) {
            return _PyStatus_NO_MEMORY();
        }
        return _PyStatus_OK();
    }

    if (calculate->path_env) {
        char *abs_path = NULL;
        status = calculate_which(calculate->path_env, pathconfig->program_name,
                                 &abs_path);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
        if (abs_path) {
            pathconfig->program_full_path = abs_path;
            return _PyStatus_OK();
        }
    }

    /* In the last resort, use an empty string */
    pathconfig->program_full_path = strdup("");
    if (pathconfig->program_full_path == NULL) {
        return _PyStatus_NO_MEMORY();
    }
    return _PyStatus_OK();
}


/* Calculate pathconfig->program_full_path */
static PyStatus
calculate_program(PyCalculatePath *calculate, _PyPathConfig *pathconfig)
{
    PyStatus status;

    status = calculate_program_impl(calculate, pathconfig);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    if (pathconfig->program_full_path[0] != '\0') {
        /* program_full_path is not empty */

        /* Make sure that program_full_path is an absolute path */
        if (!_Py_isabs(pathconfig->program_full_path)) {
            status = absolutize(&pathconfig->program_full_path);
            if (_PyStatus_EXCEPTION(status)) {
                return status;
            }
        }
    }
    return _PyStatus_OK();
}


#if HAVE_READLINK
static PyStatus
resolve_symlinks(char **path_p)
{
    char new_path[MAXPATHLEN + 1];
    const size_t new_path_len = Py_ARRAY_LENGTH(new_path);
    unsigned int nlink = 0;

    while (1) {
        int linklen = _Py_readlink(*path_p, new_path, new_path_len);
        if (linklen == -1) {
            /* not a symbolic link: we are done */
            break;
        }

        if (_Py_isabs(new_path)) {
            PyMem_Free(*path_p);
            *path_p = strdup(new_path);
            if (*path_p == NULL) {
                return _PyStatus_NO_MEMORY();
            }
        }
        else {
            /* new_path is relative to path */
            reduce(*path_p);

            char *abs_path = joinpath2(*path_p, new_path);
            if (abs_path == NULL) {
                return _PyStatus_NO_MEMORY();
            }

            PyMem_Free(*path_p);
            *path_p = abs_path;
        }

        nlink++;
        /* 40 is the Linux kernel 4.2 limit */
        if (nlink >= 40) {
            return _PyStatus_ERR("maximum number of symbolic links reached");
        }
    }
    return _PyStatus_OK();
}
#endif /* HAVE_READLINK */


static PyStatus
calculate_argv0_path(PyCalculatePath *calculate,
                     _PyPathConfig *pathconfig)
{
    PyStatus status;

    calculate->argv0_path = strdup(pathconfig->program_full_path);
    if (calculate->argv0_path == NULL) {
        return _PyStatus_NO_MEMORY();
    }
    status = resolve_symlinks(&calculate->argv0_path);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    reduce(calculate->argv0_path);
    return _PyStatus_OK();
}

static PyStatus
calculate_zip_path(PyCalculatePath *calculate)
{
    PyStatus res;

    /* Path: <platlibdir> / "pythonXY.zip" */
    char *path = joinpath2(calculate->platlibdir,
                              "python" Py_STRINGIFY(PY_MAJOR_VERSION) Py_STRINGIFY(PY_MINOR_VERSION)
                              ".zip");
    if (path == NULL) {
        return _PyStatus_NO_MEMORY();
    }

    if (calculate->prefix_found > 0) {
        /* Use the reduced prefix returned by Py_GetPrefix()

           Path: <basename(basename(prefix))> / <platlibdir> / "pythonXY.zip" */
        char *parent = strdup(calculate->prefix);
        if (parent == NULL) {
            res = _PyStatus_NO_MEMORY();
            goto done;
        }
        reduce(parent);
        reduce(parent);
        calculate->zip_path = joinpath2(parent, path);
        PyMem_Free(parent);
    }
    else {
        calculate->zip_path = joinpath2(calculate->prefix_macro, path);
    }

    if (calculate->zip_path == NULL) {
        res = _PyStatus_NO_MEMORY();
        goto done;
    }

    res = _PyStatus_OK();

done:
    PyMem_Free(path);
    return res;
}
  

static PyStatus
calculate_module_search_path(PyCalculatePath *calculate,
                             _PyPathConfig *pathconfig)
{
    /* Calculate size of return buffer */
    size_t bufsz = 0;
    if (calculate->pythonpath_env != NULL) {
        bufsz += strlen(calculate->pythonpath_env) + 1;
    }

    char *defpath = calculate->pythonpath_macro;
    size_t prefixsz = strlen(calculate->prefix) + 1;
    while (1) {
        char *delim = strchr(defpath, DELIM);

        if (!_Py_isabs(defpath)) {
            /* Paths are relative to prefix */
            bufsz += prefixsz;
        }

        if (delim) {
            bufsz += delim - defpath + 1;
        }
        else {
            bufsz += strlen(defpath) + 1;
            break;
        }
        defpath = delim + 1;
    }

    bufsz += strlen(calculate->zip_path) + 1;
    bufsz += strlen(calculate->exec_prefix) + 1;

    /* Allocate the buffer */
    char *buf = PyMem_Malloc(bufsz);
    if (buf == NULL) {
        return _PyStatus_NO_MEMORY();
    }
    buf[0] = '\0';

    /* Run-time value of $PYTHONPATH goes first */
    if (calculate->pythonpath_env) {
        strcpy(buf, calculate->pythonpath_env);
        strcat(buf, delimiter);
    }

    /* Next is the default zip path */
    strcat(buf, calculate->zip_path);
    strcat(buf, delimiter);

    /* Next goes merge of compile-time $PYTHONPATH with
     * dynamically located prefix.
     */
    defpath = calculate->pythonpath_macro;
    while (1) {
        char *delim = strchr(defpath, DELIM);

        if (!_Py_isabs(defpath)) {
            strcat(buf, calculate->prefix);
            if (prefixsz >= 2 && calculate->prefix[prefixsz - 2] != SEP &&
                defpath[0] != (delim ? DELIM : '\0'))
            {
                /* not empty */
                strcat(buf, separator);
            }
        }

        if (delim) {
            size_t len = delim - defpath + 1;
            size_t end = strlen(buf) + len;
            strncat(buf, defpath, len);
            buf[end] = '\0';
        }
        else {
            strcat(buf, defpath);
            break;
        }
        defpath = delim + 1;
    }
    strcat(buf, delimiter);

    /* Finally, on goes the directory for dynamic-load modules */
    strcat(buf, calculate->exec_prefix);

    pathconfig->module_search_path = buf;
    return _PyStatus_OK();
}


static PyStatus
calculate_init(PyCalculatePath *calculate, const PyConfig *config)
{
    calculate->warnings = config->pathconfig_warnings;
    calculate->pythonpath_env = config->pythonpath_env;
    calculate->platlibdir = config->platlibdir;

    const char *path = getenv("PATH");
    if (path) {
      calculate->path_env = strdup(path);
    }

    /* Decode macros */
    calculate->pythonpath_macro = strdup(PYTHONPATH);
    if (!calculate->pythonpath_macro) {
        return _PyStatus_NO_MEMORY();      
    }
    calculate->prefix_macro = strdup(PREFIX);
    if (!calculate->prefix_macro) {
        return _PyStatus_NO_MEMORY();      
    }
    calculate->exec_prefix_macro = strdup(EXEC_PREFIX);
    if (!calculate->exec_prefix_macro) {
        return _PyStatus_NO_MEMORY();      
    }
    calculate->vpath_macro = strdup(VPATH);
    if (!calculate->vpath_macro) {
        return _PyStatus_NO_MEMORY();      
    }

    // <platlibdir> / "pythonX.Y"
    char *pyversion = strdup("python" VERSION);
    if (!pyversion) {
        return _PyStatus_NO_MEMORY();      
    }
    calculate->lib_python = joinpath2(config->platlibdir, pyversion);
    PyMem_Free(pyversion);
    if (calculate->lib_python == NULL) {
        return _PyStatus_NO_MEMORY();
    }

    return _PyStatus_OK();
}


static void
calculate_free(PyCalculatePath *calculate)
{
    PyMem_Free(calculate->pythonpath_macro);
    PyMem_Free(calculate->prefix_macro);
    PyMem_Free(calculate->exec_prefix_macro);
    PyMem_Free(calculate->vpath_macro);
    PyMem_Free(calculate->lib_python);
    PyMem_Free(calculate->path_env);
    PyMem_Free(calculate->zip_path);
    PyMem_Free(calculate->argv0_path);
    PyMem_Free(calculate->prefix);
    PyMem_Free(calculate->exec_prefix);
}


static PyStatus
calculate_path(PyCalculatePath *calculate, _PyPathConfig *pathconfig)
{
    PyStatus status;

    if (pathconfig->program_full_path == NULL) {
        status = calculate_program(calculate, pathconfig);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    status = calculate_argv0_path(calculate, pathconfig);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    status = calculate_prefix(calculate, pathconfig);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    status = calculate_zip_path(calculate);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    
    status = calculate_exec_prefix(calculate, pathconfig);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    if ((!calculate->prefix_found || !calculate->exec_prefix_found)
        && calculate->warnings)
    {
        fprintf(stderr,
                "Consider setting $PYTHONHOME to <prefix>[:<exec_prefix>]\n");
    }

    if (pathconfig->module_search_path == NULL) {
        status = calculate_module_search_path(calculate, pathconfig);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    if (pathconfig->prefix == NULL) {
        status = calculate_set_prefix(calculate, pathconfig);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    if (pathconfig->exec_prefix == NULL) {
        status = calculate_set_exec_prefix(calculate, pathconfig);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }
    return _PyStatus_OK();
}


/* Calculate the Python path configuration.

   Inputs:

   - PATH environment variable
   - Macros: PYTHONPATH, PREFIX, EXEC_PREFIX, VERSION (ex: "3.9").
     PREFIX and EXEC_PREFIX are generated by the configure script.
     PYTHONPATH macro is the default search path.
   - pybuilddir.txt file
   - pyvenv.cfg configuration file
   - PyConfig fields ('config' function argument):

     - pathconfig_warnings
     - pythonpath_env (PYTHONPATH environment variable)

   - _PyPathConfig fields ('pathconfig' function argument):

     - program_name: see config_init_program_name()
     - home: Py_SetPythonHome() or PYTHONHOME environment variable

   - current working directory: see copy_absolute()

   Outputs, 'pathconfig' fields:

   - program_full_path
   - module_search_path
   - prefix
   - exec_prefix

   If a field is already set (non NULL), it is left unchanged. */
PyStatus
_PyPathConfig_Calculate(_PyPathConfig *pathconfig, const PyConfig *config)
{
    PyStatus status;
    PyCalculatePath calculate;
    memset(&calculate, 0, sizeof(calculate));

    status = calculate_init(&calculate, config);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    status = calculate_path(&calculate, pathconfig);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    /* program_full_path must an either an empty string or an absolute path */
    assert(strlen(pathconfig->program_full_path) == 0
           || _Py_isabs(pathconfig->program_full_path));

    status = _PyStatus_OK();

done:
    calculate_free(&calculate);
    return status;
}

#ifdef __cplusplus
}
#endif
