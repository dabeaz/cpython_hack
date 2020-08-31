#include "Python.h"
#include "pycore_getopt.h"        // _PyOS_GetOpt()
#include "pycore_initconfig.h"    // _PyArgv
#include "pycore_pymem.h"         // _PyMem_GetAllocatorName()
#include "pycore_runtime.h"       // _PyRuntime_Initialize()

#define DECODE_LOCALE_ERR(NAME, LEN) \
    (((LEN) == -2) \
     ? _PyStatus_ERR("cannot decode " NAME) \
     : _PyStatus_NO_MEMORY())


/* Forward declarations */
static void
preconfig_copy(PyPreConfig *config, const PyPreConfig *config2);


/* --- _PyArgv ---------------------------------------------------- */

/* Decode bytes_argv using Py_DecodeLocale() */
PyStatus
_PyArgv_AsCharList(const _PyArgv *args, PyStringList *list)
{
    PyStringList wargv = _PyStringList_INIT;
    { 
        size_t size = sizeof(char *) * args->argc;
        wargv.items = (char **)PyMem_RawMalloc(size);
        if (wargv.items == NULL) {
            return _PyStatus_NO_MEMORY();
        }

        for (Py_ssize_t i = 0; i < args->argc; i++) {
	    char *arg;
	    arg = strdup(args->bytes_argv[i]);
            if (arg == NULL) {
                _PyStringList_Clear(&wargv);
		return _PyStatus_NO_MEMORY();
            }
            wargv.items[i] = arg;
            wargv.length++;
        }
        _PyStringList_Clear(list);
        *list = wargv;
    }
    return _PyStatus_OK();
}


/* --- _PyPreCmdline ------------------------------------------------- */

void
_PyPreCmdline_Clear(_PyPreCmdline *cmdline)
{
    _PyStringList_Clear(&cmdline->argv);
    _PyStringList_Clear(&cmdline->xoptions);
}


PyStatus
_PyPreCmdline_SetArgv(_PyPreCmdline *cmdline, const _PyArgv *args)
{
    return _PyArgv_AsCharList(args, &cmdline->argv);
}


static void
precmdline_get_preconfig(_PyPreCmdline *cmdline, const PyPreConfig *config)
{
#define COPY_ATTR(ATTR) \
    if (config->ATTR != -1) { \
        cmdline->ATTR = config->ATTR; \
    }

    COPY_ATTR(use_environment);

#undef COPY_ATTR
}


static void
precmdline_set_preconfig(const _PyPreCmdline *cmdline, PyPreConfig *config)
{
#define COPY_ATTR(ATTR) \
    config->ATTR = cmdline->ATTR

    COPY_ATTR(use_environment);

#undef COPY_ATTR
}


PyStatus
_PyPreCmdline_SetConfig(const _PyPreCmdline *cmdline, PyConfig *config)
{
#define COPY_ATTR(ATTR) \
    config->ATTR = cmdline->ATTR
    COPY_ATTR(use_environment);
    return _PyStatus_OK();

#undef COPY_ATTR
}


/* Parse the command line arguments */
static PyStatus
precmdline_parse_cmdline(_PyPreCmdline *cmdline)
{
    const PyStringList *argv = &cmdline->argv;

    _PyOS_ResetGetOpt();
    /* Don't log parsing errors into stderr here: PyConfig_Read()
       is responsible for that */
    _PyOS_opterr = 0;
    do {
        int longindex = -1;
        int c = _PyOS_GetOpt(argv->length, argv->items, &longindex);

        if (c == EOF || c == 'c' || c == 'm') {
            break;
        }

        switch (c) {
        case 'E':
            cmdline->use_environment = 0;
            break;

        case 'X':
        {
            PyStatus status = PyStringList_Append(&cmdline->xoptions,
                                                      _PyOS_optarg);
            if (_PyStatus_EXCEPTION(status)) {
                return status;
            }
            break;
        }

        default:
            /* ignore other argument:
               handled by PyConfig_Read() */
            break;
        }
    } while (1);

    return _PyStatus_OK();
}


PyStatus
_PyPreCmdline_Read(_PyPreCmdline *cmdline, const PyPreConfig *preconfig)
{
    precmdline_get_preconfig(cmdline, preconfig);

    if (preconfig->parse_argv) {
        PyStatus status = precmdline_parse_cmdline(cmdline);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }
    return _PyStatus_OK();
}


/* --- PyPreConfig ----------------------------------------------- */


void
_PyPreConfig_InitCompatConfig(PyPreConfig *config)
{
    memset(config, 0, sizeof(*config));

    config->_config_init = (int)_PyConfig_INIT_COMPAT;
    config->parse_argv = 0;
    config->use_environment = -1;
    config->allocator = PYMEM_ALLOCATOR_NOT_SET;
}


void
PyPreConfig_InitPythonConfig(PyPreConfig *config)
{
    _PyPreConfig_InitCompatConfig(config);

    config->_config_init = (int)_PyConfig_INIT_PYTHON;
    config->parse_argv = 1;
    config->use_environment = 1;
}


PyStatus
_PyPreConfig_InitFromPreConfig(PyPreConfig *config,
                               const PyPreConfig *config2)
{
    PyPreConfig_InitPythonConfig(config);
    preconfig_copy(config, config2);
    return _PyStatus_OK();
}


void
_PyPreConfig_InitFromConfig(PyPreConfig *preconfig, const PyConfig *config)
{
    _PyConfigInitEnum config_init = (_PyConfigInitEnum)config->_config_init;
    switch (config_init) {
    case _PyConfig_INIT_PYTHON:
        PyPreConfig_InitPythonConfig(preconfig);
        break;
    case _PyConfig_INIT_COMPAT:
    default:
        _PyPreConfig_InitCompatConfig(preconfig);
    }

    _PyPreConfig_GetConfig(preconfig, config);
}


static void
preconfig_copy(PyPreConfig *config, const PyPreConfig *config2)
{
#define COPY_ATTR(ATTR) config->ATTR = config2->ATTR

    COPY_ATTR(_config_init);
    COPY_ATTR(parse_argv);
    COPY_ATTR(use_environment);
    COPY_ATTR(allocator);

#undef COPY_ATTR
}


PyObject*
_PyPreConfig_AsDict(const PyPreConfig *config)
{
    PyObject *dict;

    dict = PyDict_New();
    if (dict == NULL) {
        return NULL;
    }

#define SET_ITEM_INT(ATTR) \
        do { \
            PyObject *obj = PyLong_FromLong(config->ATTR); \
            if (obj == NULL) { \
                goto fail; \
            } \
            int res = PyDict_SetItemString(dict, #ATTR, obj); \
            Py_DECREF(obj); \
            if (res < 0) { \
                goto fail; \
            } \
        } while (0)

    SET_ITEM_INT(_config_init);
    SET_ITEM_INT(parse_argv);
    SET_ITEM_INT(use_environment);
    SET_ITEM_INT(allocator);
    return dict;

fail:
    Py_DECREF(dict);
    return NULL;

#undef SET_ITEM_INT
}


void
_PyPreConfig_GetConfig(PyPreConfig *preconfig, const PyConfig *config)
{
#define COPY_ATTR(ATTR) \
    if (config->ATTR != -1) { \
        preconfig->ATTR = config->ATTR; \
    }

    COPY_ATTR(parse_argv);
    COPY_ATTR(use_environment);

#undef COPY_ATTR
}


static void
preconfig_get_global_vars(PyPreConfig *config)
{
    if (config->_config_init != _PyConfig_INIT_COMPAT) {
        /* Python and Isolated configuration ignore global variables */
        return;
    }

#define COPY_FLAG(ATTR, VALUE) \
    if (config->ATTR < 0) { \
        config->ATTR = VALUE; \
    }
#define COPY_NOT_FLAG(ATTR, VALUE) \
    if (config->ATTR < 0) { \
        config->ATTR = !(VALUE); \
    }

    COPY_NOT_FLAG(use_environment, Py_IgnoreEnvironmentFlag);

#undef COPY_FLAG
#undef COPY_NOT_FLAG
}


static void
preconfig_set_global_vars(const PyPreConfig *config)
{
#define COPY_FLAG(ATTR, VAR) \
    if (config->ATTR >= 0) { \
        VAR = config->ATTR; \
    }
#define COPY_NOT_FLAG(ATTR, VAR) \
    if (config->ATTR >= 0) { \
        VAR = !config->ATTR; \
    }

    COPY_NOT_FLAG(use_environment, Py_IgnoreEnvironmentFlag);

#undef COPY_FLAG
#undef COPY_NOT_FLAG
}


const char*
_Py_GetEnv(int use_environment, const char *name)
{
    assert(use_environment >= 0);

    if (!use_environment) {
        return NULL;
    }

    const char *var = getenv(name);
    if (var && var[0] != '\0') {
        return var;
    }
    else {
        return NULL;
    }
}


int
_Py_str_to_int(const char *str, int *result)
{
    const char *endptr = str;
    errno = 0;
    long value = strtol(str, (char **)&endptr, 10);
    if (*endptr != '\0' || errno == ERANGE) {
        return -1;
    }
    if (value < INT_MIN || value > INT_MAX) {
        return -1;
    }

    *result = (int)value;
    return 0;
}


void
_Py_get_env_flag(int use_environment, int *flag, const char *name)
{
    const char *var = _Py_GetEnv(use_environment, name);
    if (!var) {
        return;
    }
    int value;
    if (_Py_str_to_int(var, &value) < 0 || value < 0) {
        /* PYTHONDEBUG=text and PYTHONDEBUG=-2 behave as PYTHONDEBUG=1 */
        value = 1;
    }
    if (*flag < value) {
        *flag = value;
    }
}

static PyStatus
preconfig_init_allocator(PyPreConfig *config)
{
    if (config->allocator == PYMEM_ALLOCATOR_NOT_SET) {
        /* bpo-34247. The PYTHONMALLOC environment variable has the priority
           over PYTHONDEV env var and "-X dev" command line option.
           For example, PYTHONMALLOC=malloc PYTHONDEVMODE=1 sets the memory
           allocators to "malloc" (and not to "debug"). */
        const char *envvar = _Py_GetEnv(config->use_environment, "PYTHONMALLOC");
        if (envvar) {
            PyMemAllocatorName name;
            if (_PyMem_GetAllocatorName(envvar, &name) < 0) {
                return _PyStatus_ERR("PYTHONMALLOC: unknown allocator");
            }
            config->allocator = (int)name;
        }
    }
    return _PyStatus_OK();
}


static PyStatus
preconfig_read(PyPreConfig *config, _PyPreCmdline *cmdline)
{
    PyStatus status;

    status = _PyPreCmdline_Read(cmdline, config);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    precmdline_set_preconfig(cmdline, config);

    /* allocator */
    status = preconfig_init_allocator(config);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    assert(config->use_environment >= 0);

    return _PyStatus_OK();
}


/* Read the configuration from:

   - command line arguments
   - environment variables
   - Py_xxx global configuration variables
   - the LC_CTYPE locale */
PyStatus
_PyPreConfig_Read(PyPreConfig *config, const _PyArgv *args)
{
    PyStatus status;

    status = _PyRuntime_Initialize();
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    preconfig_get_global_vars(config);

    /* Save the config to be able to restore it if encodings change */
    PyPreConfig save_config;

    status = _PyPreConfig_InitFromPreConfig(&save_config, config);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    _PyPreCmdline cmdline = _PyPreCmdline_INIT;
    int loops = 0;

    while (1) {
        /* Watchdog to prevent an infinite loop */
        loops++;
        if (loops == 3) {
            status = _PyStatus_ERR("Encoding changed twice while "
                                   "reading the configuration");
            goto done;
        }
        if (args) {
            // Set command line arguments at each iteration. If they are bytes
            // strings, they are decoded from the new encoding.
            status = _PyPreCmdline_SetArgv(&cmdline, args);
            if (_PyStatus_EXCEPTION(status)) {
                goto done;
            }
        }

        status = preconfig_read(config, &cmdline);
        if (_PyStatus_EXCEPTION(status)) {
            goto done;
        }
	break;
    }
    status = _PyStatus_OK();

done:
    _PyPreCmdline_Clear(&cmdline);
    return status;
}


/* Write the pre-configuration:

   - set the memory allocators
   - set Py_xxx global configuration variables
   - set the LC_CTYPE locale (coerce C locale, PEP 538) and set the UTF-8 mode
     (PEP 540)

   The applied configuration is written into _PyRuntime.preconfig.
   If the C locale cannot be coerced, set coerce_c_locale to 0.

   Do nothing if called after Py_Initialize(): ignore the new
   pre-configuration. */
PyStatus
_PyPreConfig_Write(const PyPreConfig *src_config)
{
    PyPreConfig config;

    PyStatus status = _PyPreConfig_InitFromPreConfig(&config, src_config);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    if (_PyRuntime.core_initialized) {
        /* bpo-34008: Calling this functions after Py_Initialize() ignores
           the new configuration. */
        return _PyStatus_OK();
    }

    PyMemAllocatorName name = (PyMemAllocatorName)config.allocator;
    if (name != PYMEM_ALLOCATOR_NOT_SET) {
        if (_PyMem_SetupAllocators(name) < 0) {
            return _PyStatus_ERR("Unknown PYTHONMALLOC allocator");
        }
    }

    preconfig_set_global_vars(&config);

    /* Write the new pre-configuration into _PyRuntime */
    preconfig_copy(&_PyRuntime.preconfig, &config);

    return _PyStatus_OK();
}
