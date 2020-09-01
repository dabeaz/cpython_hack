#ifndef Py_PYCORECONFIG_H
#define Py_PYCORECONFIG_H
#ifndef Py_LIMITED_API

/* --- PyStatus ----------------------------------------------- */

typedef struct {
    enum {
        _PyStatus_TYPE_OK=0,
        _PyStatus_TYPE_ERROR=1,
        _PyStatus_TYPE_EXIT=2
    } _type;
    const char *func;
    const char *err_msg;
    int exitcode;
} PyStatus;

PyAPI_FUNC(PyStatus) PyStatus_Ok(void);
PyAPI_FUNC(PyStatus) PyStatus_Error(const char *err_msg);
PyAPI_FUNC(PyStatus) PyStatus_NoMemory(void);
PyAPI_FUNC(PyStatus) PyStatus_Exit(int exitcode);
PyAPI_FUNC(int) PyStatus_IsError(PyStatus err);
PyAPI_FUNC(int) PyStatus_IsExit(PyStatus err);
PyAPI_FUNC(int) PyStatus_Exception(PyStatus err);

/* --- PyStringList ------------------------------------------------ */

typedef struct {
    /* If length is greater than zero, items must be non-NULL
       and all items strings must be non-NULL */
    Py_ssize_t length;
    char **items;
} PyStringList;

PyAPI_FUNC(PyStatus) PyStringList_Append(PyStringList *list,
    const char *item);
PyAPI_FUNC(PyStatus) PyStringList_Insert(PyStringList *list,
    Py_ssize_t index, const char *item);


/* --- PyPreConfig ----------------------------------------------- */

typedef struct {
    int _config_init;     /* _PyConfigInitEnum value */

    /* Parse Py_PreInitializeFromBytesArgs() arguments?
       See PyConfig.parse_argv */
    int parse_argv;

    /* If greater than 0, enable isolated mode: sys.path contains
       neither the script's directory nor the user's site-packages directory.

       Set to 1 by the -I command line option. If set to -1 (default), inherit
       Py_IsolatedFlag value. */
    int isolated;

    /* If greater than 0: use environment variables.
       Set to 0 by -E command line option. If set to -1 (default), it is
       set to !Py_IgnoreEnvironmentFlag. */
    int use_environment;

    /* Set the LC_CTYPE locale to the user preferred locale? If equals to 0,
       set coerce_c_locale and coerce_c_locale_warn to 0. */
  /*
    int configure_locale;
  */
    /* Coerce the LC_CTYPE locale if it's equal to "C"? (PEP 538)

       Set to 0 by PYTHONCOERCECLOCALE=0. Set to 1 by PYTHONCOERCECLOCALE=1.
       Set to 2 if the user preferred LC_CTYPE locale is "C".

       If it is equal to 1, LC_CTYPE locale is read to decide if it should be
       coerced or not (ex: PYTHONCOERCECLOCALE=1). Internally, it is set to 2
       if the LC_CTYPE locale must be coerced.

       Disable by default (set to 0). Set it to -1 to let Python decide if it
       should be enabled or not. */
  /*
    int coerce_c_locale;
  */
    /* Emit a warning if the LC_CTYPE locale is coerced?

       Set to 1 by PYTHONCOERCECLOCALE=warn.

       Disable by default (set to 0). Set it to -1 to let Python decide if it
       should be enabled or not. */
  /*
    int coerce_c_locale_warn;
  */

    /* Enable UTF-8 mode? (PEP 540)

       Disabled by default (equals to 0).

       Set to 1 by "-X utf8" and "-X utf8=1" command line options.
       Set to 1 by PYTHONUTF8=1 environment variable.

       Set to 0 by "-X utf8=0" and PYTHONUTF8=0.

       If equals to -1, it is set to 1 if the LC_CTYPE locale is "C" or
       "POSIX", otherwise it is set to 0. Inherit Py_UTF8Mode value value. */

  /*
    int utf8_mode;
  */
    /* If non-zero, enable the Python Development Mode.

       Set to 1 by the -X dev command line option. Set by the PYTHONDEVMODE
       environment variable. */
    int dev_mode;

    /* Memory allocator: PYTHONMALLOC env var.
       See PyMemAllocatorName for valid values. */
    int allocator;
} PyPreConfig;

PyAPI_FUNC(void) PyPreConfig_InitPythonConfig(PyPreConfig *config);
PyAPI_FUNC(void) PyPreConfig_InitIsolatedConfig(PyPreConfig *config);


/* --- PyConfig ---------------------------------------------- */

typedef struct {
    int _config_init;     /* _PyConfigInitEnum value */
    int use_environment;  /* Use environment variables? see PyPreConfig.use_environment */

    int use_hash_seed;      /* PYTHONHASHSEED=x */
    unsigned long hash_seed;

    int parse_argv;           /* Parse argv command line arguments? */

    /* Command line arguments (sys.argv).

       Set parse_argv to 1 to parse argv as Python command line arguments
       and then strip Python arguments from argv.

       If argv is empty, an empty string is added to ensure that sys.argv
       always exists and is never empty. */
    PyStringList argv;

    /* Program name:

       - If Py_SetProgramName() was called, use its value.
       - On macOS, use PYTHONEXECUTABLE environment variable if set.
       - If WITH_NEXT_FRAMEWORK macro is defined, use __PYVENV_LAUNCHER__
         environment variable is set.
       - Use argv[0] if available and non-empty.
       - Use "python" on Windows, or "python3 on other platforms. */
    char *program_name;

    /* If greater than 0, enable inspect: when a script is passed as first
       argument or the -c option is used, enter interactive mode after
       executing the script or the command, even when sys.stdin does not appear
       to be a terminal.

       Incremented by the -i command line option. Set to 1 if the PYTHONINSPECT
       environment variable is non-empty. If set to -1 (default), inherit
       Py_InspectFlag value. */
    int inspect;

    /* If greater than 0: enable the interactive mode (REPL).

       Incremented by the -i command line option. If set to -1 (default),
       inherit Py_InteractiveFlag value. */
    int interactive;

    /* If greater than 0, enable the quiet mode: Don't display the copyright
       and version messages even in interactive mode.

       Incremented by the -q option. If set to -1 (default), inherit
       Py_QuietFlag value. */
    int quiet;

    /* If non-zero, configure C standard steams (stdio, stdout,
       stderr):

       - Set O_BINARY mode on Windows.
       - If buffered_stdio is equal to zero, make streams unbuffered.
         Otherwise, enable streams buffering if interactive is non-zero. */
    int configure_c_stdio;

    /* If equal to 0, enable unbuffered mode: force the stdout and stderr
       streams to be unbuffered.

       Set to 0 by the -u option. Set by the PYTHONUNBUFFERED environment
       variable.
       If set to -1 (default), it is set to !Py_UnbufferedStdioFlag. */
    int buffered_stdio;
  
    /* --- Path configuration inputs ------------ */

    /* If greater than 0, suppress _PyPathConfig_Calculate() warnings on Unix.
       The parameter has no effect on Windows.

       If set to -1 (default), inherit !Py_FrozenFlag value. */
    int pathconfig_warnings;

    char *pythonpath_env; /* PYTHONPATH environment variable */
    char *home;          /* PYTHONHOME environment variable,
                               see also Py_SetPythonHome(). */

    /* --- Path configuration outputs ----------- */

    int module_search_paths_set;  /* If non-zero, use module_search_paths */
    PyStringList module_search_paths;  /* sys.path paths. Computed if
                                       module_search_paths_set is equal
                                       to zero. */

    char *executable;        /* sys.executable */
    char *base_executable;   /* sys._base_executable */
    char *prefix;            /* sys.prefix */
    char *base_prefix;       /* sys.base_prefix */
    char *exec_prefix;       /* sys.exec_prefix */
    char *base_exec_prefix;  /* sys.base_exec_prefix */
    char *platlibdir;        /* sys.platlibdir */

    /* --- Parameter only used by Py_Main() ---------- */

    /* Skip the first line of the source ('run_filename' parameter), allowing use of non-Unix forms of
       "#!cmd".  This is intended for a DOS specific hack only.

       Set by the -x command line option. */
    int skip_source_first_line;

    char *run_command;   /* -c command line argument */
    char *run_module;    /* -m command line argument */
    char *run_filename;  /* Trailing command line argument without -c or -m */

    /* --- Private fields ---------------------------- */

    /* Install importlib? If set to 0, importlib is not initialized at all.
       Needed by freeze_importlib. */
    int _install_importlib;

    /* If equal to 0, stop Python initialization before the "main" phase */
    int _init_main;

    /* Original command line arguments. If _orig_argv is empty and _argv is
       not equal to [''], PyConfig_Read() copies the configuration 'argv' list
       into '_orig_argv' list before modifying 'argv' list (if parse_argv
       is non-zero).

       _PyConfig_Write() initializes Py_GetArgcArgv() to this list. */
    PyStringList _orig_argv;
} PyConfig;

PyAPI_FUNC(void) PyConfig_InitPythonConfig(PyConfig *config);
PyAPI_FUNC(void) PyConfig_InitIsolatedConfig(PyConfig *config);
PyAPI_FUNC(void) PyConfig_Clear(PyConfig *);

PyAPI_FUNC(PyStatus) PyConfig_SetChar(
    PyConfig *config,
    char **config_str,
    const char *str);

PyAPI_FUNC(PyStatus) PyConfig_SetBytesString(
    PyConfig *config,
    char **config_str,
    const char *str);
PyAPI_FUNC(PyStatus) PyConfig_Read(PyConfig *config);
PyAPI_FUNC(PyStatus) PyConfig_SetBytesArgv(
    PyConfig *config,
    Py_ssize_t argc,
    char * const *argv);
PyAPI_FUNC(PyStatus) PyConfig_SetArgv(PyConfig *config,
    Py_ssize_t argc,
    char * const *argv);
PyAPI_FUNC(PyStatus) PyConfig_SetStringList(PyConfig *config,
    PyStringList *list,
    Py_ssize_t length, char **items);


/* --- Helper functions --------------------------------------- */

/* Get the original command line arguments, before Python modified them.

   See also PyConfig._orig_argv. */
PyAPI_FUNC(void) Py_GetArgcArgv(int *argc, char ***argv);

#endif /* !Py_LIMITED_API */
#endif /* !Py_PYCORECONFIG_H */
