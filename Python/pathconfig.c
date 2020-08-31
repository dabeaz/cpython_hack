/* Path configuration like module_search_path (sys.path) */

#include "Python.h"
#include "osdefs.h"               // DELIM
#include "pycore_initconfig.h"
#include "pycore_fileutils.h"
#include "pycore_pathconfig.h"
#include "pycore_pymem.h"         // _PyMem_SetDefaultAllocator()

#ifdef __cplusplus
extern "C" {
#endif


_PyPathConfig _Py_path_config = _PyPathConfig_INIT;


static int
copy_str(char **dst, const char *src)
{
    assert(*dst == NULL);
    if (src != NULL) {
      *dst = strdup(src);
      if (*dst == NULL) {
	return -1;
      }
    } else {
      *dst = NULL;
    }
    return 0;
}


static void
pathconfig_clear(_PyPathConfig *config)
{
    /* _PyMem_SetDefaultAllocator() is needed to get a known memory allocator,
       since Py_SetPath(), Py_SetPythonHome() and Py_SetProgramName() can be
       called before Py_Initialize() which can changes the memory allocator. */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

#define CLEAR(ATTR) \
    do { \
        PyMem_RawFree(ATTR); \
        ATTR = NULL; \
    } while (0)

    CLEAR(config->program_full_path);
    CLEAR(config->prefix);
    CLEAR(config->exec_prefix);
    CLEAR(config->module_search_path);
    CLEAR(config->program_name);
    CLEAR(config->home);

#undef CLEAR

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
}


static PyStatus
pathconfig_copy(_PyPathConfig *config, const _PyPathConfig *config2)
{
    pathconfig_clear(config);

#define COPY_ATTR(ATTR) \
    do { \
        if (copy_str(&config->ATTR, config2->ATTR) < 0) { \
            return _PyStatus_NO_MEMORY(); \
        } \
    } while (0)

    COPY_ATTR(program_full_path);
    COPY_ATTR(prefix);
    COPY_ATTR(exec_prefix);
    COPY_ATTR(module_search_path);
    COPY_ATTR(program_name);
    COPY_ATTR(home);

#undef COPY_ATTR

    return _PyStatus_OK();
}


void
_PyPathConfig_ClearGlobal(void)
{
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    pathconfig_clear(&_Py_path_config);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
}


static char*
_PyStringList_Join(const PyStringList *list, char sep)
{
    size_t len = 1;   /* NUL terminator */
    for (Py_ssize_t i=0; i < list->length; i++) {
        if (i != 0) {
            len++;
        }
        len += strlen(list->items[i]);
    }

    char *text = PyMem_RawMalloc(len * sizeof(char));
    if (text == NULL) {
        return NULL;
    }
    char *str = text;
    for (Py_ssize_t i=0; i < list->length; i++) {
        char *path = list->items[i];
        if (i != 0) {
            *str++ = sep;
        }
        len = strlen(path);
        memcpy(str, path, len * sizeof(char));
        str += len;
    }
    *str = '\0';
    return text;
}

static PyStatus
pathconfig_set_from_config(_PyPathConfig *pathconfig, const PyConfig *config)
{
    PyStatus status;
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    if (config->module_search_paths_set) {
        PyMem_RawFree(pathconfig->module_search_path);
        pathconfig->module_search_path = _PyStringList_Join(&config->module_search_paths, DELIM);
        if (pathconfig->module_search_path == NULL) {
            goto no_memory;
        }
    }

#define COPY_CONFIG(PATH_ATTR, CONFIG_ATTR) \
        if (config->CONFIG_ATTR) { \
            PyMem_RawFree(pathconfig->PATH_ATTR); \
            pathconfig->PATH_ATTR = NULL; \
            if (copy_str(&pathconfig->PATH_ATTR, config->CONFIG_ATTR) < 0) { \
                goto no_memory; \
            } \
        }

    COPY_CONFIG(program_full_path, executable);
    COPY_CONFIG(prefix, prefix);
    COPY_CONFIG(exec_prefix, exec_prefix);
    COPY_CONFIG(program_name, program_name);
    COPY_CONFIG(home, home);

#undef COPY_CONFIG

    status = _PyStatus_OK();
    goto done;

no_memory:
    status = _PyStatus_NO_MEMORY();

done:
    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
    return status;
}


PyStatus
_PyConfig_WritePathConfig(const PyConfig *config)
{
    return pathconfig_set_from_config(&_Py_path_config, config);
}


static PyStatus
config_init_module_search_paths(PyConfig *config, _PyPathConfig *pathconfig)
{
    assert(!config->module_search_paths_set);

    _PyStringList_Clear(&config->module_search_paths);

    const char *sys_path = pathconfig->module_search_path;
    const char delim = DELIM;
    while (1) {
        const char *p = strchr(sys_path, delim);
        if (p == NULL) {
            p = sys_path + strlen(sys_path); /* End of string */
        }

        size_t path_len = (p - sys_path);
        char *path = PyMem_RawMalloc((path_len + 1));
        if (path == NULL) {
            return _PyStatus_NO_MEMORY();
        }
        memcpy(path, sys_path, path_len);
        path[path_len] = '\0';

        PyStatus status = PyStringList_Append(&config->module_search_paths, path);
        PyMem_RawFree(path);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }

        if (*p == '\0') {
            break;
        }
        sys_path = p + 1;
    }
    config->module_search_paths_set = 1;
    return _PyStatus_OK();
}


/* Calculate the path configuration:

   - exec_prefix
   - module_search_path
   - prefix
   - program_full_path

   On Windows, more fields are calculated:

   - base_executable
   - isolated
   - site_import

   On other platforms, isolated and site_import are left unchanged, and
   _PyConfig_InitPathConfig() copies executable to base_executable (if it's not
   set).

   Priority, highest to lowest:

   - PyConfig
   - _Py_path_config: set by Py_SetPath(), Py_SetPythonHome()
     and Py_SetProgramName()
   - _PyPathConfig_Calculate()
*/
static PyStatus
pathconfig_calculate(_PyPathConfig *pathconfig, const PyConfig *config)
{
    PyStatus status;

    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    status = pathconfig_copy(pathconfig, &_Py_path_config);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    status = pathconfig_set_from_config(pathconfig, config);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    if (_Py_path_config.module_search_path == NULL) {
        status = _PyPathConfig_Calculate(pathconfig, config);
    }
    else {
        /* Py_SetPath() has been called: avoid _PyPathConfig_Calculate() */
    }

done:
    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
    return status;
}


static PyStatus
config_calculate_pathconfig(PyConfig *config)
{
    _PyPathConfig pathconfig = _PyPathConfig_INIT;
    PyStatus status;

    status = pathconfig_calculate(&pathconfig, config);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    if (!config->module_search_paths_set) {
        status = config_init_module_search_paths(config, &pathconfig);
        if (_PyStatus_EXCEPTION(status)) {
            goto done;
        }
    }

#define COPY_ATTR(PATH_ATTR, CONFIG_ATTR) \
        if (config->CONFIG_ATTR == NULL) { \
            if (copy_str(&config->CONFIG_ATTR, pathconfig.PATH_ATTR) < 0) { \
                goto no_memory; \
            } \
        }


    COPY_ATTR(program_full_path, executable);
    COPY_ATTR(prefix, prefix);
    COPY_ATTR(exec_prefix, exec_prefix);

#undef COPY_ATTR


    status = _PyStatus_OK();
    goto done;

no_memory:
    status = _PyStatus_NO_MEMORY();

done:
    pathconfig_clear(&pathconfig);
    return status;
}


PyStatus
_PyConfig_InitPathConfig(PyConfig *config)
{
    /* Do we need to calculate the path? */
    if (!config->module_search_paths_set
        || config->executable == NULL
        || config->prefix == NULL
        || config->exec_prefix == NULL)
    {
        PyStatus status = config_calculate_pathconfig(config);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    if (config->base_prefix == NULL) {
        if (copy_str(&config->base_prefix, config->prefix) < 0) {
            return _PyStatus_NO_MEMORY();
        }
    }

    if (config->base_exec_prefix == NULL) {
        if (copy_str(&config->base_exec_prefix,
                      config->exec_prefix) < 0) {
            return _PyStatus_NO_MEMORY();
        }
    }

    if (config->base_executable == NULL) {
        if (copy_str(&config->base_executable,
                      config->executable) < 0) {
            return _PyStatus_NO_MEMORY();
        }
    }

    return _PyStatus_OK();
}


static PyStatus
pathconfig_global_read(_PyPathConfig *pathconfig)
{
    PyConfig config;
    _PyConfig_InitCompatConfig(&config);

    /* Call _PyConfig_InitPathConfig() */
    PyStatus status = PyConfig_Read(&config);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    status = pathconfig_set_from_config(pathconfig, &config);

done:
    PyConfig_Clear(&config);
    return status;
}


static void
pathconfig_global_init(void)
{
    PyStatus status;


    if (_Py_path_config.module_search_path == NULL) {
        status = pathconfig_global_read(&_Py_path_config);
        if (_PyStatus_EXCEPTION(status)) {
            Py_ExitStatusException(status);
        }
    }
    else {
        /* Global configuration already initialized */
    }

    assert(_Py_path_config.program_full_path != NULL);
    assert(_Py_path_config.prefix != NULL);
    assert(_Py_path_config.exec_prefix != NULL);
    assert(_Py_path_config.module_search_path != NULL);
    assert(_Py_path_config.program_name != NULL);
    /* home can be NULL */
}


/* External interface */

static void _Py_NO_RETURN
path_out_of_memory(const char *func)
{
    _Py_FatalErrorFunc(func, "out of memory");
}

void
Py_SetPath(const char *path)
{
    if (path == NULL) {
        pathconfig_clear(&_Py_path_config);
        return;
    }

    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    /* Getting the program full path calls pathconfig_global_init() */
    char *program_full_path = strdup(Py_GetProgramFullPath());

    PyMem_RawFree(_Py_path_config.program_full_path);
    PyMem_RawFree(_Py_path_config.prefix);
    PyMem_RawFree(_Py_path_config.exec_prefix);
    PyMem_RawFree(_Py_path_config.module_search_path);

    _Py_path_config.program_full_path = program_full_path;
    _Py_path_config.prefix = strdup("");
    _Py_path_config.exec_prefix = strdup("");
    _Py_path_config.module_search_path = strdup(path);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    if (_Py_path_config.program_full_path == NULL
        || _Py_path_config.prefix == NULL
        || _Py_path_config.exec_prefix == NULL
        || _Py_path_config.module_search_path == NULL)
    {
        path_out_of_memory(__func__);
    }
}


void
Py_SetPythonHome(const char *home)
{
    if (home == NULL) {
        return;
    }

    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    PyMem_RawFree(_Py_path_config.home);
    _Py_path_config.home = strdup(home);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    if (_Py_path_config.home == NULL) {
        path_out_of_memory(__func__);
    }
}


void
Py_SetProgramName(const char *program_name)
{
    if (program_name == NULL || program_name[0] == '\0') {
        return;
    }

    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    PyMem_RawFree(_Py_path_config.program_name);
    _Py_path_config.program_name = strdup(program_name);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    if (_Py_path_config.program_name == NULL) {
        path_out_of_memory(__func__);
    }
}

void
_Py_SetProgramFullPath(const char *program_full_path)
{
    if (program_full_path == NULL || program_full_path[0] == '\0') {
        return;
    }

    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    PyMem_RawFree(_Py_path_config.program_full_path);
    _Py_path_config.program_full_path = strdup(program_full_path);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    if (_Py_path_config.program_full_path == NULL) {
        path_out_of_memory(__func__);
    }
}


char *
Py_GetPath(void)
{
    pathconfig_global_init();
    return _Py_path_config.module_search_path;
}


char *
Py_GetPrefix(void)
{
    pathconfig_global_init();
    return _Py_path_config.prefix;
}


char *
Py_GetExecPrefix(void)
{
    pathconfig_global_init();
    return _Py_path_config.exec_prefix;
}


char *
Py_GetProgramFullPath(void)
{
    pathconfig_global_init();
    return _Py_path_config.program_full_path;
}


char*
Py_GetPythonHome(void)
{
    pathconfig_global_init();
    return _Py_path_config.home;
}


char *
Py_GetProgramName(void)
{
    pathconfig_global_init();
    return _Py_path_config.program_name;
}

/* Compute module search path from argv[0] or the current working
   directory ("-m module" case) which will be prepended to sys.argv:
   sys.path[0].

   Return 1 if the path is correctly resolved and written into *path0_p.

   Return 0 if it fails to resolve the full path. For example, return 0 if the
   current working directory has been removed (bpo-36236) or if argv is empty.

   Raise an exception and return -1 on error.
   */
int
_PyPathConfig_ComputeSysPath0(const PyStringList *argv, PyObject **path0_p)
{
    assert(_PyStringList_CheckConsistency(argv));

    if (argv->length == 0) {
        /* Leave sys.path unchanged if sys.argv is empty */
        return 0;
    }

    char *argv0 = argv->items[0];
    int have_module_arg = (strcmp(argv0, "-m") == 0);
    int have_script_arg = (!have_module_arg && (strcmp(argv0, "-c") != 0));

    char *path0 = argv0;
    Py_ssize_t n = 0;

#ifdef HAVE_REALPATH
    char fullpath[MAXPATHLEN];
#endif

    if (have_module_arg) {
#if defined(HAVE_REALPATH)
        if (!_Py_getcwd(fullpath, Py_ARRAY_LENGTH(fullpath))) {
            return 0;
        }
        path0 = fullpath;
#else
        path0 = ".";
#endif
        n = strlen(path0);
    }

#ifdef HAVE_READLINK
    char link[MAXPATHLEN + 1];
    int nr = 0;

    if (have_script_arg) {
        nr = _Py_readlink(path0, link, Py_ARRAY_LENGTH(link));
    }
    if (nr > 0) {
        /* It's a symlink */
        link[nr] = '\0';
        if (link[0] == SEP) {
            path0 = link; /* Link to absolute path */
        }
        else if (strchr(link, SEP) == NULL) {
            /* Link without path */
        }
        else {
            /* Must join(dirname(path0), link) */
            char *q = strrchr(path0, SEP);
            if (q == NULL) {
                /* path0 without path */
                path0 = link;
            }
            else {
                /* Must make a copy, path0copy has room for 2 * MAXPATHLEN */
                char path0copy[2 * MAXPATHLEN + 1];
                strncpy(path0copy, path0, MAXPATHLEN);
                q = strrchr(path0copy, SEP);
                strncpy(q+1, link, MAXPATHLEN);
                q[MAXPATHLEN + 1] = '\0';
                path0 = path0copy;
            }
        }
    }
#endif /* HAVE_READLINK */
    
    char *p = NULL;

#if SEP == '\\'
    /* Special case for Microsoft filename syntax */
    if (have_script_arg) {
        char *q;
        p = strrchr(path0, SEP);
        /* Test for alternate separator */
        q = strrchr(p ? p : path0, '/');
        if (q != NULL)
            p = q;
        if (p != NULL) {
            n = p + 1 - path0;
            if (n > 1 && p[-1] != ':')
                n--; /* Drop trailing separator */
        }
    }
#else
    /* All other filename syntaxes */
    if (have_script_arg) {
#if defined(HAVE_REALPATH)
        if (_Py_realpath(path0, fullpath, Py_ARRAY_LENGTH(fullpath))) {
            path0 = fullpath;
        }
#endif
        p = strrchr(path0, SEP);
    }
    if (p != NULL) {
        n = p + 1 - path0;
#if SEP == '/' /* Special case for Unix filename syntax */
        if (n > 1) {
            /* Drop trailing separator */
            n--;
        }
#endif /* Unix */
    }
#endif /* All others */

    PyObject *path0_obj = PyUnicode_FromString(path0);
    if (path0_obj == NULL) {
        return -1;
    }

    *path0_p = path0_obj;
    return 1;
}


#ifdef __cplusplus
}
#endif
