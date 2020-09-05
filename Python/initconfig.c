#include "Python.h"
#include "pycore_fileutils.h"     // _Py_HasFileSystemDefaultEncodeErrors
#include "pycore_getopt.h"        // _PyOS_GetOpt()
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_interp.h"        // _PyInterpreterState.runtime
#include "pycore_pathconfig.h"    // _Py_path_config
#include "pycore_pyerrors.h"      // _PyErr_Fetch()
#include "pycore_pylifecycle.h"   // _Py_PreInitializeFromConfig()
#include "pycore_pymem.h"         // _PyMem_SetDefaultAllocator()
#include "pycore_pystate.h"       // PyThreadState_Get()

#include "osdefs.h"               // DELIM
#ifdef HAVE_LANGINFO_H
#  include <langinfo.h>           // nl_langinfo(CODESET)
#endif

#ifndef PLATLIBDIR
#  error "PLATLIBDIR macro must be defined"
#endif


/* --- Command line options --------------------------------------- */

/* Short usage message (with %s for argv0) */
static const char usage_line[] =
"usage: %s [option] ... [-c cmd | -m mod | file | -] [arg] ...\n";

/* Long usage message, split into parts < 512 bytes */
static const char usage_1[] = "\
Options and arguments (and corresponding environment variables):\n\
-b     : issue warnings about str(bytes_instance), str(bytearray_instance)\n\
         and comparing bytes/bytearray with str. (-bb: issue errors)\n\
-B     : don't write .pyc files on import; also PYTHONDONTWRITEBYTECODE=x\n\
-c cmd : program passed in as string (terminates option list)\n\
-d     : debug output from parser; also PYTHONDEBUG=x\n\
-E     : ignore PYTHON* environment variables (such as PYTHONPATH)\n\
-h     : print this help message and exit (also --help)\n\
";
static const char usage_2[] = "\
-i     : inspect interactively after running script; forces a prompt even\n\
         if stdin does not appear to be a terminal; also PYTHONINSPECT=x\n\
-m mod : run library module as a script (terminates option list)\n\
-O     : remove assert and __debug__-dependent statements; add .opt-1 before\n\
         .pyc extension; also PYTHONOPTIMIZE=x\n\
-OO    : do -O changes and also discard docstrings; add .opt-2 before\n\
         .pyc extension\n\
-q     : don't print version and copyright messages on interactive startup\n\
-s     : don't add user site directory to sys.path; also PYTHONNOUSERSITE\n\
-S     : don't imply 'import site' on initialization\n\
";
static const char usage_3[] = "\
-u     : force the stdout and stderr streams to be unbuffered;\n\
         this option has no effect on stdin; also PYTHONUNBUFFERED=x\n\
-V     : print the Python version number and exit (also --version)\n\
         when given twice, print more information about the build\n\
-W arg : warning control; arg is action:message:category:module:lineno\n\
         also PYTHONWARNINGS=arg\n\
-x     : skip first line of source, allowing use of non-Unix forms of #!cmd\n\
-X opt : set implementation-specific option. The following options are available:\n\
\n\
         -X dev: enable CPython’s “development mode”, introducing additional runtime\n\
             checks which are too expensive to be enabled by default. Effect of the\n\
             developer mode:\n\
                * Add default warning filter, as -W default\n\
                * Install debug hooks on memory allocators: see the PyMem_SetupDebugHooks() C function\n\
                * Enable the faulthandler module to dump the Python traceback on a crash\n\
                * Enable asyncio debug mode\n\
                * Set the dev_mode attribute of sys.flags to True\n\
                * io.IOBase destructor logs close() exceptions\n\
\n\
--check-hash-based-pycs always|default|never:\n\
    control how Python invalidates hash-based .pyc files\n\
";
static const char usage_4[] = "\
file   : program read from script file\n\
-      : program read from stdin (default; interactive mode if a tty)\n\
arg ...: arguments passed to program in sys.argv[1:]\n\n\
Other environment variables:\n\
PYTHONSTARTUP: file executed on interactive startup (no default)\n\
PYTHONPATH   : '%lc'-separated list of directories prefixed to the\n\
               default module search path.  The result is sys.path.\n\
";
static const char usage_5[] =
"PYTHONHOME   : alternate <prefix> directory (or <prefix>%lc<exec_prefix>).\n"
"               The default module search path uses %s.\n"
"PYTHONPLATLIBDIR : override sys.platlibdir.\n"
"PYTHONCASEOK : ignore case in 'import' statements (Windows).\n"
"PYTHONUTF8: if set to 1, enable the UTF-8 mode.\n"
  "PYTHONIOENCODING: Encoding[:errors] used for stdin/stdout/stderr.\n";
  
static const char usage_6[] =
"PYTHONHASHSEED: if this variable is set to 'random', a random value is used\n"
"   to seed the hashes of str and bytes objects.  It can also be set to an\n"
"   integer in the range [0,4294967295] to get hash values with a\n"
"   predictable seed.\n"
"PYTHONMALLOC: set the Python memory allocators and/or install debug hooks\n"
"   on Python memory allocators. Use PYTHONMALLOC=debug to install debug\n"
"   hooks.\n"
"PYTHONBREAKPOINT: if this variable is set to 0, it disables the default\n"
"   debugger. It can be set to the callable of your debugger of choice.\n"
"PYTHONDEVMODE: enable the development mode.\n";

#define PYTHONHOMEHELP "<prefix>/lib/pythonX.X"

/* --- Global configuration variables ----------------------------- */

/* UTF-8 mode (PEP 540): if equals to 1, use the UTF-8 encoding, and change
   stdin and stdout error handler to "surrogateescape". */
int Py_UTF8Mode = 0;
int Py_DebugFlag = 0; /* Needed by parser.c */
int Py_QuietFlag = 0; /* Needed by sysmodule.c */
int Py_InteractiveFlag = 0; /* Needed by Py_FdIsInteractive() below */
int Py_InspectFlag = 0; /* Needed to determine whether to exit at SystemExit */
int Py_OptimizeFlag = 0; /* Needed by compile.c */
int Py_BytesWarningFlag = 0; /* Warn on str(bytes) and str(buffer) */
int Py_FrozenFlag = 0; /* Needed by getpath.c */
int Py_IgnoreEnvironmentFlag = 0; /* e.g. PYTHONPATH, PYTHONHOME */
int Py_NoUserSiteDirectory = 0; /* for -s and site.py */
int Py_UnbufferedStdioFlag = 0; /* Unbuffered binary std{in,out,err} */
int Py_HashRandomizationFlag = 0; /* for -R and PYTHONHASHSEED */

static PyObject *
_Py_GetGlobalVariablesAsDict(void)
{
    PyObject *dict, *obj;

    dict = PyDict_New();
    if (dict == NULL) {
        return NULL;
    }

#define SET_ITEM(KEY, EXPR) \
        do { \
            obj = (EXPR); \
            if (obj == NULL) { \
                return NULL; \
            } \
            int res = PyDict_SetItemString(dict, (KEY), obj); \
            Py_DECREF(obj); \
            if (res < 0) { \
                goto fail; \
            } \
        } while (0)
#define SET_ITEM_INT(VAR) \
    SET_ITEM(#VAR, PyLong_FromLong(VAR))
#define FROM_STRING(STR) \
    ((STR != NULL) ? \
        PyUnicode_FromString(STR) \
        : (Py_INCREF(Py_None), Py_None))
#define SET_ITEM_STR(VAR) \
    SET_ITEM(#VAR, FROM_STRING(VAR))

    SET_ITEM_INT(Py_UTF8Mode);
    SET_ITEM_INT(Py_DebugFlag);
    SET_ITEM_INT(Py_QuietFlag);
    SET_ITEM_INT(Py_InteractiveFlag);
    SET_ITEM_INT(Py_InspectFlag);

    SET_ITEM_INT(Py_OptimizeFlag);
    SET_ITEM_INT(Py_BytesWarningFlag);
    SET_ITEM_INT(Py_FrozenFlag);
    SET_ITEM_INT(Py_IgnoreEnvironmentFlag);
    SET_ITEM_INT(Py_UnbufferedStdioFlag);
    SET_ITEM_INT(Py_HashRandomizationFlag);

    return dict;

fail:
    Py_DECREF(dict);
    return NULL;

#undef FROM_STRING
#undef SET_ITEM
#undef SET_ITEM_INT
#undef SET_ITEM_STR
}


/* --- PyStatus ----------------------------------------------- */

PyStatus PyStatus_Ok(void)
{ return _PyStatus_OK(); }

PyStatus PyStatus_Error(const char *err_msg)
{
    return (PyStatus){._type = _PyStatus_TYPE_ERROR,
                          .err_msg = err_msg};
}

PyStatus PyStatus_NoMemory(void)
{ return PyStatus_Error("memory allocation failed"); }

PyStatus PyStatus_Exit(int exitcode)
{ return _PyStatus_EXIT(exitcode); }


int PyStatus_IsError(PyStatus status)
{ return _PyStatus_IS_ERROR(status); }

int PyStatus_IsExit(PyStatus status)
{ return _PyStatus_IS_EXIT(status); }

int PyStatus_Exception(PyStatus status)
{ return _PyStatus_EXCEPTION(status); }


/* --- StringList */


#ifndef NDEBUG
int
_PyStringList_CheckConsistency(const PyStringList *list)
{
    assert(list->length >= 0);
    if (list->length != 0) {
        assert(list->items != NULL);
    }
    for (Py_ssize_t i = 0; i < list->length; i++) {
        assert(list->items[i] != NULL);
    }
    return 1;
}
#endif   /* Py_DEBUG */

void
_PyStringList_Clear(PyStringList *list)
{
    assert(_PyStringList_CheckConsistency(list));
    for (Py_ssize_t i=0; i < list->length; i++) {
        PyMem_RawFree(list->items[i]);
    }
    PyMem_RawFree(list->items);
    list->length = 0;
    list->items = NULL;
}


int
_PyStringList_Copy(PyStringList *list, const PyStringList *list2)
{
    assert(_PyStringList_CheckConsistency(list));
    assert(_PyStringList_CheckConsistency(list2));

    if (list2->length == 0) {
        _PyStringList_Clear(list);
        return 0;
    }

    PyStringList copy = _PyStringList_INIT;

    size_t size = list2->length * sizeof(list2->items[0]);
    copy.items = PyMem_RawMalloc(size);
    if (copy.items == NULL) {
        return -1;
    }

    for (Py_ssize_t i=0; i < list2->length; i++) {
      char *item = strdup(list2->items[i]);
      if (item == NULL) {
	_PyStringList_Clear(&copy);
	return -1;
      }
      copy.items[i] = item;
      copy.length = i + 1;
    }
    _PyStringList_Clear(list);
    *list = copy;
    return 0;
}


PyStatus
PyStringList_Insert(PyStringList *list,
                        Py_ssize_t index, const char *item)
{
    Py_ssize_t len = list->length;
    if (len == PY_SSIZE_T_MAX) {
        /* length+1 would overflow */
        return _PyStatus_NO_MEMORY();
    }
    if (index < 0) {
        return _PyStatus_ERR("PyStringList_Insert index must be >= 0");
    }
    if (index > len) {
        index = len;
    }

    char *item2 = strdup(item);
    if (item2 == NULL) {
        return _PyStatus_NO_MEMORY();
    }

    size_t size = (len + 1) * sizeof(list->items[0]);
    char **items2 = (char **)PyMem_RawRealloc(list->items, size);
    if (items2 == NULL) {
        PyMem_RawFree(item2);
        return _PyStatus_NO_MEMORY();
    }

    if (index < len) {
        memmove(&items2[index + 1],
                &items2[index],
                (len - index) * sizeof(items2[0]));
    }

    items2[index] = item2;
    list->items = items2;
    list->length++;
    return _PyStatus_OK();
}


PyStatus
PyStringList_Append(PyStringList *list, const char *item)
{
    return PyStringList_Insert(list, list->length, item);
}


PyStatus
_PyStringList_Extend(PyStringList *list, const PyStringList *list2)
{
    for (Py_ssize_t i = 0; i < list2->length; i++) {
        PyStatus status = PyStringList_Append(list, list2->items[i]);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }
    return _PyStatus_OK();
}


PyObject*
_PyStringList_AsList(const PyStringList *list)
{
    assert(_PyStringList_CheckConsistency(list));

    PyObject *pylist = PyList_New(list->length);
    if (pylist == NULL) {
        return NULL;
    }

    for (Py_ssize_t i = 0; i < list->length; i++) {
      PyObject *item = PyUnicode_FromString(list->items[i]);
      if (item == NULL) {
	Py_DECREF(pylist);
	return NULL;
      }
      PyList_SET_ITEM(pylist, i, item);
    }
    return pylist;
}

/* --- Py_SetStandardStreamEncoding() ----------------------------- */

/* Helper to allow an embedding application to override the normal
 * mechanism that attempts to figure out an appropriate IO encoding
 */

static char *_Py_StandardStreamEncoding = NULL;
static char *_Py_StandardStreamErrors = NULL;

int
Py_SetStandardStreamEncoding(const char *encoding, const char *errors)
{
    if (Py_IsInitialized()) {
        /* This is too late to have any effect */
        return -1;
    }

    int res = 0;

    /* Py_SetStandardStreamEncoding() can be called before Py_Initialize(),
       but Py_Initialize() can change the allocator. Use a known allocator
       to be able to release the memory later. */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    /* Can't call PyErr_NoMemory() on errors, as Python hasn't been
     * initialised yet.
     *
     * However, the raw memory allocators are initialised appropriately
     * as C static variables, so _PyMem_RawStrdup is OK even though
     * Py_Initialize hasn't been called yet.
     */
    if (encoding) {
        PyMem_RawFree(_Py_StandardStreamEncoding);
        _Py_StandardStreamEncoding = _PyMem_RawStrdup(encoding);
        if (!_Py_StandardStreamEncoding) {
            res = -2;
            goto done;
        }
    }
    if (errors) {
        PyMem_RawFree(_Py_StandardStreamErrors);
        _Py_StandardStreamErrors = _PyMem_RawStrdup(errors);
        if (!_Py_StandardStreamErrors) {
            PyMem_RawFree(_Py_StandardStreamEncoding);
            _Py_StandardStreamEncoding = NULL;
            res = -3;
            goto done;
        }
    }

done:
    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    return res;
}


void
_Py_ClearStandardStreamEncoding(void)
{
    /* Use the same allocator than Py_SetStandardStreamEncoding() */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    /* We won't need them anymore. */
    if (_Py_StandardStreamEncoding) {
        PyMem_RawFree(_Py_StandardStreamEncoding);
        _Py_StandardStreamEncoding = NULL;
    }
    if (_Py_StandardStreamErrors) {
        PyMem_RawFree(_Py_StandardStreamErrors);
        _Py_StandardStreamErrors = NULL;
    }

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
}


/* --- Py_GetArgcArgv() ------------------------------------------- */

/* For Py_GetArgcArgv(); set by _Py_SetArgcArgv() */
static PyStringList orig_argv = {.length = 0, .items = NULL};


void
_Py_ClearArgcArgv(void)
{
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    _PyStringList_Clear(&orig_argv);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
}


static int
_Py_SetArgcArgv(Py_ssize_t argc, char * const *argv)
{
    const PyStringList argv_list = {.length = argc, .items = (char **)argv};
    int res;

    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    res = _PyStringList_Copy(&orig_argv, &argv_list);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
    return res;
}


void
Py_GetArgcArgv(int *argc, char ***argv)
{
    *argc = (int)orig_argv.length;
    *argv = orig_argv.items;
}


/* --- PyConfig ---------------------------------------------- */

#define DECODE_LOCALE_ERR(NAME, LEN) \
    (((LEN) == -2) \
     ? _PyStatus_ERR("cannot decode " NAME) \
     : _PyStatus_NO_MEMORY())


/* Free memory allocated in config, but don't clear all attributes */
void
PyConfig_Clear(PyConfig *config)
{
#define CLEAR(ATTR) \
    do { \
        PyMem_RawFree(ATTR); \
        ATTR = NULL; \
    } while (0)

    CLEAR(config->pythonpath_env);
    CLEAR(config->home);
    CLEAR(config->program_name);

    _PyStringList_Clear(&config->argv);
    _PyStringList_Clear(&config->module_search_paths);
    config->module_search_paths_set = 0;

    CLEAR(config->executable);
    CLEAR(config->base_executable);
    CLEAR(config->prefix);
    CLEAR(config->base_prefix);
    CLEAR(config->exec_prefix);
    CLEAR(config->base_exec_prefix);
    CLEAR(config->platlibdir);
    CLEAR(config->run_command);
    CLEAR(config->run_module);
    CLEAR(config->run_filename);
#undef CLEAR
}


void
_PyConfig_InitCompatConfig(PyConfig *config)
{
    memset(config, 0, sizeof(*config));

    config->_config_init = (int)_PyConfig_INIT_COMPAT;
    config->use_environment = -1;
    config->use_hash_seed = -1;
    config->module_search_paths_set = 0;
    config->parse_argv = 0;
    config->inspect = -1;
    config->interactive = -1;
    config->quiet = -1;
    config->configure_c_stdio = 0;
    config->buffered_stdio = -1;
    config->_install_importlib = 1;
    config->pathconfig_warnings = -1;
    config->_init_main = 1;
}


static void
config_init_defaults(PyConfig *config)
{
    _PyConfig_InitCompatConfig(config);

    config->use_environment = 1;
    config->inspect = 0;
    config->interactive = 0;
    config->quiet = 0;
    config->buffered_stdio = 1;
    config->pathconfig_warnings = 1;
}


void
PyConfig_InitPythonConfig(PyConfig *config)
{
    config_init_defaults(config);

    config->_config_init = (int)_PyConfig_INIT_PYTHON;
    config->configure_c_stdio = 1;
    config->parse_argv = 1;
}


void
PyConfig_InitIsolatedConfig(PyConfig *config)
{
    config_init_defaults(config);

    config->_config_init = (int)_PyConfig_INIT_ISOLATED;
    config->use_environment = 0;
    config->use_hash_seed = 0;
    config->pathconfig_warnings = 0;
}

/* Copy str into *config_str (duplicate the string) */
PyStatus
PyConfig_SetChar(PyConfig *config, char **config_str, const char *str)
{
    PyStatus status = _Py_PreInitializeFromConfig(config, NULL);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    char *str2;
    if (str != NULL) {
      str2 = strdup(str);
      if (str2 == NULL) {
	return _PyStatus_NO_MEMORY();
      }
    }
    else {
        str2 = NULL;
    }
    PyMem_RawFree(*config_str);
    *config_str = str2;
    return _PyStatus_OK();
}


static PyStatus
config_set_bytes_string(PyConfig *config, char **config_str,
                        const char *str, const char *decode_err_msg)
{
  char *str2;
    PyStatus status = _Py_PreInitializeFromConfig(config, NULL);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    str2 = strdup(str);
    if (str2 == NULL) {
      return _PyStatus_NO_MEMORY();
    }
    PyMem_RawFree(*config_str);
    *config_str = str2;
    return _PyStatus_OK();
}


#define CONFIG_SET_BYTES_STR(config, config_str, str, NAME) \
    config_set_bytes_string(config, config_str, str, "cannot decode " NAME)


/* Decode str using Py_DecodeLocale() and set the result into *config_str.
   Pre-initialize Python if needed to ensure that encodings are properly
   configured. */
PyStatus
PyConfig_SetBytesString(PyConfig *config, char **config_str,
                           const char *str)
{
    return CONFIG_SET_BYTES_STR(config, config_str, str, "string");
}


PyStatus
_PyConfig_Copy(PyConfig *config, const PyConfig *config2)
{
    PyStatus status;

    PyConfig_Clear(config);

#define COPY_ATTR(ATTR) config->ATTR = config2->ATTR
#define COPY_WSTR_ATTR(ATTR) \
    do { \
        status = PyConfig_SetString(config, &config->ATTR, config2->ATTR); \
        if (_PyStatus_EXCEPTION(status)) { \
            return status; \
        } \
    } while (0)

#define COPY_CHAR_ATTR(ATTR) \
    do { \
        status = PyConfig_SetChar(config, &config->ATTR, config2->ATTR); \
        if (_PyStatus_EXCEPTION(status)) { \
            return status; \
        } \
    } while (0)
    
#define COPY_WSTRLIST(LIST) \
    do { \
        if (_PyWideStringList_Copy(&config->LIST, &config2->LIST) < 0) { \
            return _PyStatus_NO_MEMORY(); \
        } \
    } while (0)

#define COPY_CHARLIST(LIST) \
    do { \
        if (_PyStringList_Copy(&config->LIST, &config2->LIST) < 0) { \
            return _PyStatus_NO_MEMORY(); \
        } \
    } while (0)
    
    COPY_ATTR(_config_init);
    COPY_ATTR(use_environment);
    COPY_ATTR(use_hash_seed);
    COPY_ATTR(hash_seed);
    COPY_ATTR(_install_importlib);

    COPY_CHAR_ATTR(pythonpath_env);
    COPY_CHAR_ATTR(home);
    COPY_CHAR_ATTR(program_name);

    COPY_ATTR(parse_argv);
    COPY_CHARLIST(argv);
    COPY_CHARLIST(module_search_paths);
    COPY_ATTR(module_search_paths_set);

    COPY_CHAR_ATTR(executable);
    COPY_CHAR_ATTR(base_executable);
    COPY_CHAR_ATTR(prefix);
    COPY_CHAR_ATTR(base_prefix);
    COPY_CHAR_ATTR(exec_prefix);
    COPY_CHAR_ATTR(base_exec_prefix);
    COPY_CHAR_ATTR(platlibdir);

    COPY_ATTR(inspect);
    COPY_ATTR(interactive);
    COPY_ATTR(quiet);
    COPY_ATTR(configure_c_stdio);
    COPY_ATTR(buffered_stdio);
    COPY_ATTR(skip_source_first_line);
    COPY_CHAR_ATTR(run_command);
    COPY_CHAR_ATTR(run_module);
    COPY_CHAR_ATTR(run_filename);
    COPY_ATTR(pathconfig_warnings);
    COPY_ATTR(_init_main);
    COPY_CHARLIST(_orig_argv);

#undef COPY_ATTR
#undef COPY_WSTR_ATTR
#undef COPY_WSTRLIST
#undef COPY_CHAR_ATTR
#undef COPY_CHARLIST
    return _PyStatus_OK();
}


static PyObject *
config_as_dict(const PyConfig *config)
{
    PyObject *dict;

    dict = PyDict_New();
    if (dict == NULL) {
        return NULL;
    }

#define SET_ITEM(KEY, EXPR) \
        do { \
            PyObject *obj = (EXPR); \
            if (obj == NULL) { \
                goto fail; \
            } \
            int res = PyDict_SetItemString(dict, (KEY), obj); \
            Py_DECREF(obj); \
            if (res < 0) { \
                goto fail; \
            } \
        } while (0)
#define SET_ITEM_INT(ATTR) \
    SET_ITEM(#ATTR, PyLong_FromLong(config->ATTR))
#define SET_ITEM_UINT(ATTR) \
    SET_ITEM(#ATTR, PyLong_FromUnsignedLong(config->ATTR))
#define FROM_WSTRING(STR) \
    ((STR != NULL) ? \
        PyUnicode_FromWideChar(STR, -1) \
        : (Py_INCREF(Py_None), Py_None))

#define FROM_CHAR(STR) \
    ((STR != NULL) ? \
        PyUnicode_FromString(STR) \
        : (Py_INCREF(Py_None), Py_None))
    
#define SET_ITEM_WSTR(ATTR) \
    SET_ITEM(#ATTR, FROM_WSTRING(config->ATTR))

#define SET_ITEM_CHAR(ATTR) \
    SET_ITEM(#ATTR, FROM_CHAR(config->ATTR))
    
#define SET_ITEM_WSTRLIST(LIST) \
    SET_ITEM(#LIST, _PyWideStringList_AsList(&config->LIST))

#define SET_ITEM_CHARLIST(LIST) \
    SET_ITEM(#LIST, _PyStringList_AsList(&config->LIST))
    
    SET_ITEM_INT(_config_init);
    SET_ITEM_INT(use_environment);
    SET_ITEM_INT(use_hash_seed);
    SET_ITEM_UINT(hash_seed);
    SET_ITEM_CHAR(program_name);
    SET_ITEM_INT(parse_argv);
    SET_ITEM_CHARLIST(argv);
    SET_ITEM_CHAR(pythonpath_env);
    SET_ITEM_CHAR(home);
    SET_ITEM_CHARLIST(module_search_paths);
    SET_ITEM_CHAR(executable);
    SET_ITEM_CHAR(base_executable);
    SET_ITEM_CHAR(prefix);
    SET_ITEM_CHAR(base_prefix);
    SET_ITEM_CHAR(exec_prefix);
    SET_ITEM_CHAR(base_exec_prefix);
    SET_ITEM_CHAR(platlibdir);
    SET_ITEM_INT(inspect);
    SET_ITEM_INT(interactive);
    SET_ITEM_INT(quiet);
    SET_ITEM_INT(configure_c_stdio);
    SET_ITEM_INT(buffered_stdio);
    SET_ITEM_INT(skip_source_first_line);
    SET_ITEM_CHAR(run_command);
    SET_ITEM_CHAR(run_module);
    SET_ITEM_CHAR(run_filename);
    SET_ITEM_INT(_install_importlib);
    SET_ITEM_INT(pathconfig_warnings);
    SET_ITEM_INT(_init_main);
    SET_ITEM_CHARLIST(_orig_argv);

    return dict;

fail:
    Py_DECREF(dict);
    return NULL;

#undef FROM_WSTRING
#undef SET_ITEM
#undef SET_ITEM_INT
#undef SET_ITEM_UINT
#undef SET_ITEM_WSTR
#undef SET_ITEM_WSTRLIST
#undef SET_ITEM_CHAR
#undef SET_ITEM_CHARLIST
}


static const char*
config_get_env(const PyConfig *config, const char *name)
{
    return _Py_GetEnv(config->use_environment, name);
}


/* Get a copy of the environment variable as wchar_t*.
   Return 0 on success, but *dest can be NULL.
   Return -1 on memory allocation failure. Return -2 on decoding error. */
static PyStatus
config_get_env_dup(PyConfig *config,
                   char **dest,
                   char *wname, char *name,
                   const char *decode_err_msg)
{
    assert(*dest == NULL);
    assert(config->use_environment >= 0);

    if (!config->use_environment) {
        *dest = NULL;
        return _PyStatus_OK();
    }

    const char *var = getenv(name);
    if (!var || var[0] == '\0') {
        *dest = NULL;
        return _PyStatus_OK();
    }

    return config_set_bytes_string(config, dest, var, decode_err_msg);
}


#define CONFIG_GET_ENV_DUP(CONFIG, DEST, WNAME, NAME) \
    config_get_env_dup(CONFIG, DEST, WNAME, NAME, "cannot decode " NAME)


static void
config_get_global_vars(PyConfig *config)
{
    if (config->_config_init != _PyConfig_INIT_COMPAT) {
        /* Python and Isolated configuration ignore global variables */
        return;
    }

#define COPY_FLAG(ATTR, VALUE) \
        if (config->ATTR == -1) { \
            config->ATTR = VALUE; \
        }
#define COPY_NOT_FLAG(ATTR, VALUE) \
        if (config->ATTR == -1) { \
            config->ATTR = !(VALUE); \
        }

    COPY_NOT_FLAG(use_environment, Py_IgnoreEnvironmentFlag);
    COPY_FLAG(inspect, Py_InspectFlag);
    COPY_FLAG(interactive, Py_InteractiveFlag);
    COPY_FLAG(quiet, Py_QuietFlag);
    COPY_NOT_FLAG(pathconfig_warnings, Py_FrozenFlag);

    COPY_NOT_FLAG(buffered_stdio, Py_UnbufferedStdioFlag);

#undef COPY_FLAG
#undef COPY_NOT_FLAG
}


/* Set Py_xxx global configuration variables from 'config' configuration. */
static void
config_set_global_vars(const PyConfig *config)
{
#define COPY_FLAG(ATTR, VAR) \
        if (config->ATTR != -1) { \
            VAR = config->ATTR; \
        }
#define COPY_NOT_FLAG(ATTR, VAR) \
        if (config->ATTR != -1) { \
            VAR = !config->ATTR; \
        }

    COPY_NOT_FLAG(use_environment, Py_IgnoreEnvironmentFlag);
    COPY_FLAG(inspect, Py_InspectFlag);
    COPY_FLAG(interactive, Py_InteractiveFlag);
    COPY_FLAG(quiet, Py_QuietFlag);
    COPY_NOT_FLAG(pathconfig_warnings, Py_FrozenFlag);

    COPY_NOT_FLAG(buffered_stdio, Py_UnbufferedStdioFlag);

    /* Random or non-zero hash seed */
    Py_HashRandomizationFlag = (config->use_hash_seed == 0 ||
                                config->hash_seed != 0);

#undef COPY_FLAG
#undef COPY_NOT_FLAG
}


/* Get the program name: use PYTHONEXECUTABLE and __PYVENV_LAUNCHER__
   environment variables on macOS if available. */
static PyStatus
config_init_program_name(PyConfig *config)
{
    PyStatus status;

    /* If Py_SetProgramName() was called, use its value */
    const char *program_name = _Py_path_config.program_name;
    if (program_name != NULL) {
      config->program_name = strdup(program_name);
        if (config->program_name == NULL) {
            return _PyStatus_NO_MEMORY();
        }
        return _PyStatus_OK();
    }

    /* Use argv[0] if available and non-empty */
    const PyStringList *argv = &config->argv;
    if (argv->length >= 1 && argv->items[0][0] != L'\0') {
        config->program_name = strdup(argv->items[0]);
        if (config->program_name == NULL) {
            return _PyStatus_NO_MEMORY();
        }
        return _PyStatus_OK();
    }

    /* Last fall back: hardcoded name */
    const char *default_program_name = "python3";
    status = PyConfig_SetChar(config, &config->program_name,
                                default_program_name);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    return _PyStatus_OK();
}

static PyStatus
config_init_executable(PyConfig *config)
{
    assert(config->executable == NULL);

    /* If Py_SetProgramFullPath() was called, use its value */
    const char *program_full_path = _Py_path_config.program_full_path;
    if (program_full_path != NULL) {
        PyStatus status = PyConfig_SetChar(config,
                                             &config->executable,
                                             program_full_path);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
        return _PyStatus_OK();
    }
    return _PyStatus_OK();
}


static PyStatus
config_init_home(PyConfig *config)
{
    assert(config->home == NULL);

    /* If Py_SetPythonHome() was called, use its value */
    char *home = _Py_path_config.home;
    if (home) {
        PyStatus status = PyConfig_SetChar(config, &config->home, home);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
        return _PyStatus_OK();
    }

    return CONFIG_GET_ENV_DUP(config, &config->home,
                              "PYTHONHOME", "PYTHONHOME");
}


static PyStatus
config_init_hash_seed(PyConfig *config)
{
    const char *seed_text = config_get_env(config, "PYTHONHASHSEED");

    Py_BUILD_ASSERT(sizeof(_Py_HashSecret_t) == sizeof(_Py_HashSecret.uc));
    /* Convert a text seed to a numeric one */
    if (seed_text && strcmp(seed_text, "random") != 0) {
        const char *endptr = seed_text;
        unsigned long seed;
        errno = 0;
        seed = strtoul(seed_text, (char **)&endptr, 10);
        if (*endptr != '\0'
            || seed > 4294967295UL
            || (errno == ERANGE && seed == ULONG_MAX))
        {
            return _PyStatus_ERR("PYTHONHASHSEED must be \"random\" "
                                "or an integer in range [0; 4294967295]");
        }
        /* Use a specific hash */
        config->use_hash_seed = 1;
        config->hash_seed = seed;
    }
    else {
        /* Use a random hash */
        config->use_hash_seed = 0;
        config->hash_seed = 0;
    }
    return _PyStatus_OK();
}


static PyStatus
config_read_env_vars(PyConfig *config)
{
    PyStatus status;
    int use_env = config->use_environment;

    /* Get environment variables */
    _Py_get_env_flag(use_env, &config->inspect, "PYTHONINSPECT");

    int unbuffered_stdio = 0;
    _Py_get_env_flag(use_env, &unbuffered_stdio, "PYTHONUNBUFFERED");
    if (unbuffered_stdio) {
        config->buffered_stdio = 0;
    }

    if (config->pythonpath_env == NULL) {
        status = CONFIG_GET_ENV_DUP(config, &config->pythonpath_env,
                                    "PYTHONPATH", "PYTHONPATH");
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    if(config->platlibdir == NULL) {
        status = CONFIG_GET_ENV_DUP(config, &config->platlibdir,
                                    "PYTHONPLATLIBDIR", "PYTHONPLATLIBDIR");
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    if (config->use_hash_seed < 0) {
        status = config_init_hash_seed(config);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    return _PyStatus_OK();
}

static PyStatus
config_read(PyConfig *config)
{
    PyStatus status;

    if (config->use_environment) {
        status = config_read_env_vars(config);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    if (config->home == NULL) {
        status = config_init_home(config);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    if (config->executable == NULL) {
        status = config_init_executable(config);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    if(config->platlibdir == NULL) {
        status = CONFIG_SET_BYTES_STR(config, &config->platlibdir, PLATLIBDIR,
                                      "PLATLIBDIR macro");
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    if (config->_install_importlib) {
        status = _PyConfig_InitPathConfig(config);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    /* default values */
    if (config->use_hash_seed < 0) {
        config->use_hash_seed = 0;
        config->hash_seed = 0;
    }
    if (config->argv.length < 1) {
        /* Ensure at least one (empty) argument is seen */
        status = PyStringList_Append(&config->argv, "");
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    if (config->configure_c_stdio < 0) {
        config->configure_c_stdio = 1;
    }

    return _PyStatus_OK();
}


static void
config_init_stdio(const PyConfig *config)
{

    if (!config->buffered_stdio) {
#ifdef HAVE_SETVBUF
        setvbuf(stdin,  (char *)NULL, _IONBF, BUFSIZ);
        setvbuf(stdout, (char *)NULL, _IONBF, BUFSIZ);
        setvbuf(stderr, (char *)NULL, _IONBF, BUFSIZ);
#else /* !HAVE_SETVBUF */
        setbuf(stdin,  (char *)NULL);
        setbuf(stdout, (char *)NULL);
        setbuf(stderr, (char *)NULL);
#endif /* !HAVE_SETVBUF */
    }
    else if (config->interactive) {
#ifdef HAVE_SETVBUF
        setvbuf(stdin,  (char *)NULL, _IOLBF, BUFSIZ);
        setvbuf(stdout, (char *)NULL, _IOLBF, BUFSIZ);
#endif /* HAVE_SETVBUF */
    }
}


/* Write the configuration:

   - set Py_xxx global configuration variables
   - initialize C standard streams (stdin, stdout, stderr) */
PyStatus
_PyConfig_Write(const PyConfig *config, _PyRuntimeState *runtime)
{
    config_set_global_vars(config);

    if (config->configure_c_stdio) {
        config_init_stdio(config);
    }

    /* Write the new pre-configuration into _PyRuntime */
    PyPreConfig *preconfig = &runtime->preconfig;
    preconfig->use_environment = config->use_environment;

    if (_Py_SetArgcArgv(config->_orig_argv.length,
                        config->_orig_argv.items) < 0)
    {
        return _PyStatus_NO_MEMORY();
    }
    return _PyStatus_OK();
}


/* --- PyConfig command line parser -------------------------- */

static void
config_usage(int error, const char* program)
{
    FILE *f = error ? stderr : stdout;

    fprintf(f, usage_line, program);
    if (error)
        fprintf(f, "Try `python -h' for more information.\n");
    else {
        fputs(usage_1, f);
        fputs(usage_2, f);
        fputs(usage_3, f);
        fprintf(f, usage_4, (wint_t)DELIM);
        fprintf(f, usage_5, (wint_t)DELIM, PYTHONHOMEHELP);
        fputs(usage_6, f);
    }
}


/* Parse the command line arguments */
static PyStatus
config_parse_cmdline(PyConfig *config, 
                     Py_ssize_t *opt_index)
{
    const PyStringList *argv = &config->argv;
    int print_version = 0;
    const char* program = config->program_name;

    _PyOS_ResetGetOpt();
    do {
        int longindex = -1;
        int c = _PyOS_GetOpt(argv->length, argv->items, &longindex);
        if (c == EOF) {
            break;
        }

        if (c == 'c') {
            if (config->run_command == NULL) {
                /* -c is the last option; following arguments
                   that look like options are left for the
                   command to interpret. */
                size_t len = strlen(_PyOS_optarg) + 1 + 1;
                char *command = PyMem_RawMalloc(sizeof(char) * len);
                if (command == NULL) {
                    return _PyStatus_NO_MEMORY();
                }
                memcpy(command, _PyOS_optarg, (len - 2) * sizeof(char));
                command[len - 2] = '\n';
                command[len - 1] = 0;
                config->run_command = command;
            }
            break;
        }

        if (c == 'm') {
            /* -m is the last option; following arguments
               that look like options are left for the
               module to interpret. */
            if (config->run_module == NULL) {
                config->run_module = strdup(_PyOS_optarg);
                if (config->run_module == NULL) {
                    return _PyStatus_NO_MEMORY();
                }
            }
            break;
        }

        switch (c) {
        case 0:
            // Handle long option.
            assert(longindex == 0); // Only one long option now.
            if (strcmp(_PyOS_optarg, "always") == 0
                || strcmp(_PyOS_optarg, "never") == 0
                || strcmp(_PyOS_optarg, "default") == 0)
            {
            } else {
                fprintf(stderr, "--check-hash-based-pycs must be one of "
                        "'default', 'always', or 'never'\n");
                config_usage(1, program);
                return _PyStatus_EXIT(2);
            }
            break;

        case 'i':
            config->inspect++;
            config->interactive++;
            break;

        case 'E':
        case 'I':
        case 'X':
            /* option handled by _PyPreCmdline_Read() */
            break;

        /* case 'J': reserved for Jython */

        case 't':
            /* ignored for backwards compatibility */
            break;

        case 'u':
            config->buffered_stdio = 0;
            break;

        case 'x':
            config->skip_source_first_line = 1;
            break;

        case 'h':
        case '?':
            config_usage(0, program);
            return _PyStatus_EXIT(0);

        case 'V':
            print_version++;
            break;

        case 'q':
            config->quiet++;
            break;

        case 'R':
            config->use_hash_seed = 0;
            break;

        /* This space reserved for other options */

        default:
            /* unknown argument: parsing failed */
            config_usage(1, program);
            return _PyStatus_EXIT(2);
        }
    } while (1);

    if (print_version) {
        printf("Python %s\n",
                (print_version >= 2) ? Py_GetVersion() : PY_VERSION);
        return _PyStatus_EXIT(0);
    }

    if (config->run_command == NULL && config->run_module == NULL
        && _PyOS_optind < argv->length
        && strcmp(argv->items[_PyOS_optind], "-") != 0
        && config->run_filename == NULL)
    {
        config->run_filename = strdup(argv->items[_PyOS_optind]);
        if (config->run_filename == NULL) {
            return _PyStatus_NO_MEMORY();
        }
    }

    if (config->run_command != NULL || config->run_module != NULL) {
        /* Backup _PyOS_optind */
        _PyOS_optind--;
    }

    *opt_index = _PyOS_optind;

    return _PyStatus_OK();
}


#  define WCSTOK wcstok

static PyStatus
config_update_argv(PyConfig *config, Py_ssize_t opt_index)
{
    const PyStringList *cmdline_argv = &config->argv;
    PyStringList config_argv = _PyStringList_INIT;

    /* Copy argv to be able to modify it (to force -c/-m) */
    if (cmdline_argv->length <= opt_index) {
        /* Ensure at least one (empty) argument is seen */
        PyStatus status = PyStringList_Append(&config_argv, "");
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }
    else {
        PyStringList slice;
        slice.length = cmdline_argv->length - opt_index;
        slice.items = &cmdline_argv->items[opt_index];
        if (_PyStringList_Copy(&config_argv, &slice) < 0) {
            return _PyStatus_NO_MEMORY();
        }
    }
    assert(config_argv.length >= 1);

    char *arg0 = NULL;
    if (config->run_command != NULL) {
        /* Force sys.argv[0] = '-c' */
        arg0 = "-c";
    }
    else if (config->run_module != NULL) {
        /* Force sys.argv[0] = '-m'*/
        arg0 = "-m";
    }

    if (arg0 != NULL) {
        arg0 = strdup(arg0);
        if (arg0 == NULL) {
            _PyStringList_Clear(&config_argv);
            return _PyStatus_NO_MEMORY();
        }

        PyMem_RawFree(config_argv.items[0]);
        config_argv.items[0] = arg0;
    }

    _PyStringList_Clear(&config->argv);
    config->argv = config_argv;
    return _PyStatus_OK();
}


static PyStatus
core_read_precmdline(PyConfig *config, _PyPreCmdline *precmdline)
{
    PyStatus status;

    if (config->parse_argv) {
        if (_PyStringList_Copy(&precmdline->argv, &config->argv) < 0) {
            return _PyStatus_NO_MEMORY();
        }
    }

    PyPreConfig preconfig;

    status = _PyPreConfig_InitFromPreConfig(&preconfig, &_PyRuntime.preconfig);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    _PyPreConfig_GetConfig(&preconfig, config);

    status = _PyPreCmdline_Read(precmdline, &preconfig);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    status = _PyPreCmdline_SetConfig(precmdline, config);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    return _PyStatus_OK();
}


/* Get run_filename absolute path */
static PyStatus
config_run_filename_abspath(PyConfig *config)
{
    if (!config->run_filename) {
        return _PyStatus_OK();
    }

    if (_Py_isabs(config->run_filename)) {
        /* path is already absolute */
        return _PyStatus_OK();
    }

    char *abs_filename;
    if (_Py_abspath(config->run_filename, &abs_filename) < 0) {
        /* failed to get the absolute path of the command line filename:
           ignore the error, keep the relative path */
        return _PyStatus_OK();
    }
    if (abs_filename == NULL) {
        return _PyStatus_NO_MEMORY();
    }

    PyMem_RawFree(config->run_filename);
    config->run_filename = abs_filename;
    return _PyStatus_OK();
}


static PyStatus
config_read_cmdline(PyConfig *config)
{
    PyStatus status;

    if (config->parse_argv < 0) {
        config->parse_argv = 1;
    }

    if (config->program_name == NULL) {
        status = config_init_program_name(config);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    if (config->parse_argv) {
        Py_ssize_t opt_index;
        status = config_parse_cmdline(config, &opt_index);
        if (_PyStatus_EXCEPTION(status)) {
            goto done;
        }

        status = config_run_filename_abspath(config);
        if (_PyStatus_EXCEPTION(status)) {
            goto done;
        }

        status = config_update_argv(config, opt_index);
        if (_PyStatus_EXCEPTION(status)) {
            goto done;
        }
    }
    else {
        status = config_run_filename_abspath(config);
        if (_PyStatus_EXCEPTION(status)) {
            goto done;
        }
    }
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    status = _PyStatus_OK();

done:
    return status;
}


PyStatus
_PyConfig_SetPyArgv(PyConfig *config, const _PyArgv *args)
{
    PyStatus status = _Py_PreInitializeFromConfig(config, args);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    return _PyArgv_AsCharList(args, &config->argv);
}


/* Set config.argv: decode argv using Py_DecodeLocale(). Pre-initialize Python
   if needed to ensure that encodings are properly configured. */
PyStatus
PyConfig_SetBytesArgv(PyConfig *config, Py_ssize_t argc, char * const *argv)
{
    _PyArgv args = {
        .argc = argc,
        .bytes_argv = argv};
    return _PyConfig_SetPyArgv(config, &args);
}

PyStatus
PyConfig_SetStringList(PyConfig *config, PyStringList *list,
                           Py_ssize_t length, char **items)
{
    PyStatus status = _Py_PreInitializeFromConfig(config, NULL);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    PyStringList list2 = {.length = length, .items = items};
    if (_PyStringList_Copy(list, &list2) < 0) {
        return _PyStatus_NO_MEMORY();
    }
    return _PyStatus_OK();
}


/* Read the configuration into PyConfig from:

   * Command line arguments
   * Environment variables
   * Py_xxx global configuration variables

   The only side effects are to modify config and to call _Py_SetArgcArgv(). */
PyStatus
PyConfig_Read(PyConfig *config)
{
    PyStatus status;

    status = _Py_PreInitializeFromConfig(config, NULL);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    config_get_global_vars(config);

    if (config->_orig_argv.length == 0
        && !(config->argv.length == 1
             && strcmp(config->argv.items[0], "") == 0))
    {
        if (_PyStringList_Copy(&config->_orig_argv, &config->argv) < 0) {
            return _PyStatus_NO_MEMORY();
        }
    }

    _PyPreCmdline precmdline = _PyPreCmdline_INIT;
    status = core_read_precmdline(config, &precmdline);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    status = config_read_cmdline(config);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    status = config_read(config);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    /* Check config consistency */
    assert(config->use_environment >= 0);
    assert(config->use_hash_seed >= 0);
    assert(config->inspect >= 0);
    assert(config->interactive >= 0);
    assert(config->quiet >= 0);
    assert(config->parse_argv >= 0);
    assert(config->configure_c_stdio >= 0);
    assert(config->buffered_stdio >= 0);
    assert(config->program_name != NULL);
    assert(_PyStringList_CheckConsistency(&config->argv));
    /* sys.argv must be non-empty: empty argv is replaced with [''] */
    assert(config->argv.length >= 1);
    assert(_PyStringList_CheckConsistency(&config->module_search_paths));
    if (config->_install_importlib) {
        assert(config->module_search_paths_set != 0);
        /* don't check config->module_search_paths */
        assert(config->executable != NULL);
        assert(config->base_executable != NULL);
        assert(config->prefix != NULL);
        assert(config->base_prefix != NULL);
        assert(config->exec_prefix != NULL);
        assert(config->base_exec_prefix != NULL);
    }
    assert(config->platlibdir != NULL);
    assert(config->filesystem_encoding != NULL);
    assert(config->filesystem_errors != NULL);
    /* -c and -m options are exclusive */
    assert(!(config->run_command != NULL && config->run_module != NULL));
    assert(config->_install_importlib >= 0);
    assert(config->pathconfig_warnings >= 0);
    assert(_PyStringList_CheckConsistency(&config->_orig_argv));
    status = _PyStatus_OK();

done:
    _PyPreCmdline_Clear(&precmdline);
    return status;
}


PyObject*
_Py_GetConfigsAsDict(void)
{
    PyObject *result = NULL;
    PyObject *dict = NULL;

    result = PyDict_New();
    if (result == NULL) {
        goto error;
    }

    /* global result */
    dict = _Py_GetGlobalVariablesAsDict();
    if (dict == NULL) {
        goto error;
    }
    if (PyDict_SetItemString(result, "global_config", dict) < 0) {
        goto error;
    }
    Py_CLEAR(dict);

    /* pre config */
    PyThreadState *tstate = PyThreadState_Get();
    const PyPreConfig *pre_config = &tstate->interp->runtime->preconfig;
    dict = _PyPreConfig_AsDict(pre_config);
    if (dict == NULL) {
        goto error;
    }
    if (PyDict_SetItemString(result, "pre_config", dict) < 0) {
        goto error;
    }
    Py_CLEAR(dict);

    /* core config */
    const PyConfig *config = _PyInterpreterState_GetConfig(tstate->interp);
    dict = config_as_dict(config);
    if (dict == NULL) {
        goto error;
    }
    if (PyDict_SetItemString(result, "config", dict) < 0) {
        goto error;
    }
    Py_CLEAR(dict);

    return result;

error:
    Py_XDECREF(result);
    Py_XDECREF(dict);
    return NULL;
}

/* Dump the Python path configuration into sys.stderr */
void
_Py_DumpPathConfig(PyThreadState *tstate)
{
    PyObject *exc_type, *exc_value, *exc_tb;
    _PyErr_Fetch(tstate, &exc_type, &exc_value, &exc_tb);

    PySys_WriteStderr("Python path configuration:\n");

#define DUMP_CONFIG(NAME, FIELD) \
        do { \
            PySys_WriteStderr("  " NAME " = "); \
	    PySys_WriteStderr("%s", config->FIELD);	\
            PySys_WriteStderr("\n"); \
        } while (0)

    const PyConfig *config = _PyInterpreterState_GetConfig(tstate->interp);
    DUMP_CONFIG("PYTHONHOME", home);
    DUMP_CONFIG("PYTHONPATH", pythonpath_env);
    DUMP_CONFIG("program name", program_name);
    PySys_WriteStderr("  environment = %i\n", config->use_environment);
#undef DUMP_CONFIG

#define DUMP_SYS(NAME) \
        do { \
            obj = PySys_GetObject(#NAME); \
            PySys_FormatStderr("  sys.%s = ", #NAME); \
            if (obj != NULL) { \
                PySys_FormatStderr("%A", obj); \
            } \
            else { \
                PySys_WriteStderr("(not set)"); \
            } \
            PySys_FormatStderr("\n"); \
        } while (0)

    PyObject *obj;
    DUMP_SYS(_base_executable);
    DUMP_SYS(base_prefix);
    DUMP_SYS(base_exec_prefix);
    DUMP_SYS(platlibdir);
    DUMP_SYS(executable);
    DUMP_SYS(prefix);
    DUMP_SYS(exec_prefix);
#undef DUMP_SYS

    PyObject *sys_path = PySys_GetObject("path");  /* borrowed reference */
    if (sys_path != NULL && PyList_Check(sys_path)) {
        PySys_WriteStderr("  sys.path = [\n");
        Py_ssize_t len = PyList_GET_SIZE(sys_path);
        for (Py_ssize_t i=0; i < len; i++) {
            PyObject *path = PyList_GET_ITEM(sys_path, i);
            PySys_FormatStderr("    %A,\n", path);
        }
        PySys_WriteStderr("  ]\n");
    }

    _PyErr_Restore(tstate, exc_type, exc_value, exc_tb);
}
