/* Python interpreter main program */

#include "Python.h"
#include "pycore_initconfig.h"    // _PyArgv
#include "pycore_interp.h"        // _PyInterpreterState.sysdict
#include "pycore_pathconfig.h"    // _PyPathConfig_ComputeSysPath0()
#include "pycore_pylifecycle.h"   // _Py_PreInitializeFromPyArgv()
#include "pycore_pystate.h"       // _PyInterpreterState_GET()

/* Includes for exit_sigint() */
#include <stdio.h>                // perror()
#ifdef HAVE_SIGNAL_H
#  include <signal.h>             // SIGINT
#endif
#if defined(HAVE_GETPID) && defined(HAVE_UNISTD_H)
#  include <unistd.h>             // getpid()
#endif
/* End of includes for exit_sigint() */

#define COPYRIGHT \
    "Type \"help\", \"copyright\", \"credits\" or \"license\" " \
    "for more information."

#ifdef __cplusplus
extern "C" {
#endif

/* --- pymain_init() ---------------------------------------------- */

static PyStatus
pymain_init(const _PyArgv *args)
{
    PyStatus status;

    status = _PyRuntime_Initialize();
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    PyPreConfig preconfig;
    PyPreConfig_InitPythonConfig(&preconfig);

    status = _Py_PreInitializeFromPyArgv(&preconfig, args);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    /* pass NULL as the config: config is read from command line arguments,
       environment variables, configuration files */
    status = PyConfig_SetBytesArgv(&config, args->argc, args->bytes_argv);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    status = Py_InitializeFromConfig(&config);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }
    status = _PyStatus_OK();

done:
    PyConfig_Clear(&config);
    return status;
}


/* --- pymain_run_python() ---------------------------------------- */

/* Non-zero if filename, command (-c) or module (-m) is set
   on the command line */
static inline int config_run_code(const PyConfig *config)
{
    return (config->run_command != NULL
            || config->run_filename != NULL
            || config->run_module != NULL);
}


/* Return non-zero is stdin is a TTY or if -i command line option is used */
static int
stdin_is_interactive(const PyConfig *config)
{
    return (isatty(fileno(stdin)) || config->interactive);
}


/* Display the current Python exception and return an exitcode */
static int
pymain_err_print(int *exitcode_p)
{
    int exitcode;
    if (_Py_HandleSystemExit(&exitcode)) {
        *exitcode_p = exitcode;
        return 1;
    }

    PyErr_Print();
    return 0;
}


static int
pymain_exit_err_print(void)
{
    int exitcode = 1;
    pymain_err_print(&exitcode);
    return exitcode;
}


/* Write an exitcode into *exitcode and return 1 if we have to exit Python.
   Return 0 otherwise. */
static int
pymain_get_importer(const char*filename, PyObject **importer_p, int *exitcode)
{
    PyObject *sys_path0 = NULL, *importer;

    sys_path0 = PyUnicode_FromString(filename);
    if (sys_path0 == NULL) {
        goto error;
    }

    importer = PyImport_GetImporter(sys_path0);
    if (importer == NULL) {
        goto error;
    }

    if (importer == Py_None) {
        Py_DECREF(sys_path0);
        Py_DECREF(importer);
        return 0;
    }

    Py_DECREF(importer);
    *importer_p = sys_path0;
    return 0;

error:
    Py_XDECREF(sys_path0);

    PySys_WriteStderr("Failed checking if argv[0] is an import path entry\n");
    return pymain_err_print(exitcode);
}


static int
pymain_sys_path_add_path0(PyInterpreterState *interp, PyObject *path0)
{
    _Py_IDENTIFIER(path);
    PyObject *sys_path;
    PyObject *sysdict = interp->sysdict;
    if (sysdict != NULL) {
        sys_path = _PyDict_GetItemIdWithError(sysdict, &PyId_path);
        if (sys_path == NULL && PyErr_Occurred()) {
            return -1;
        }
    }
    else {
        sys_path = NULL;
    }
    if (sys_path == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "unable to get sys.path");
        return -1;
    }

    if (PyList_Insert(sys_path, 0, path0)) {
        return -1;
    }
    return 0;
}

const char *_warning = "You who will come after us, take warning!\n"
"    This is a message and part of system of messages.\n"
"        PAY ATTENTION TO IT!\n"
"\n"
"Sending this message was important to us we consider ourselves a\n"
"powerful CULTURE this message is a warning about danger this place\n"
"is not a place of honor no highly esteemed deed is commemorated\n"
"here nothing valued is here this place is best shunned and left\n"
"uninhabited\n"
"\n"
"This message is a warning about DANGER.\n"
"The danger is in a particular location.\n"
"The danger increases toward a center.\n"
"The center of danger is here.\n"
"The danger is present in your time as it was in ours.\n"
"\n"
"This message is a WARNING and part of a system of messages:\n"
"    Pay attention to it!\n"
"\n"
"We considered ourselves a powerful culture.  We held the stars in the\n"
"palms of our hands, but the stars are also fire and the stars burn you\n"
  "who will come after us, TAKE WARNING!\n";

static void
pymain_header(const PyConfig *config)
{
    if (config->quiet) {
        return;
    }

    if ((config_run_code(config) || !stdin_is_interactive(config))) {
        return;
    }

    fprintf(stderr, "Python %s on %s\n", Py_GetVersion(), Py_GetPlatform());
    fprintf(stderr, "%s", _warning);
}


static void
pymain_import_readline(const PyConfig *config)
{
    if (!config->inspect && config_run_code(config)) {
        return;
    }
    if (!isatty(fileno(stdin))) {
        return;
    }

    PyObject *mod = PyImport_ImportModule("readline");
    if (mod == NULL) {
        PyErr_Clear();
    }
    else {
        Py_DECREF(mod);
    }
}


static int
pymain_run_command(char *command, PyCompilerFlags *cf)
{
    int ret;

    ret = PyRun_SimpleStringFlags(command, cf);
    return (ret != 0);
}


static int
pymain_run_module(const char *modname, int set_argv0)
{
    PyObject *module, *runpy, *runmodule, *runargs, *result;
    runpy = PyImport_ImportModule("runpy");
    if (runpy == NULL) {
        fprintf(stderr, "Could not import runpy module\n");
        return pymain_exit_err_print();
    }
    runmodule = PyObject_GetAttrString(runpy, "_run_module_as_main");
    if (runmodule == NULL) {
        fprintf(stderr, "Could not access runpy._run_module_as_main\n");
        Py_DECREF(runpy);
        return pymain_exit_err_print();
    }
    module = PyUnicode_FromString(modname);
    if (module == NULL) {
        fprintf(stderr, "Could not convert module name to unicode\n");
        Py_DECREF(runpy);
        Py_DECREF(runmodule);
        return pymain_exit_err_print();
    }
    runargs = PyTuple_Pack(2, module, set_argv0 ? Py_True : Py_False);
    if (runargs == NULL) {
        fprintf(stderr,
            "Could not create arguments for runpy._run_module_as_main\n");
        Py_DECREF(runpy);
        Py_DECREF(runmodule);
        Py_DECREF(module);
        return pymain_exit_err_print();
    }
    result = PyObject_Call(runmodule, runargs, NULL);
    Py_DECREF(runpy);
    Py_DECREF(runmodule);
    Py_DECREF(module);
    Py_DECREF(runargs);
    if (result == NULL) {
        return pymain_exit_err_print();
    }
    Py_DECREF(result);
    return 0;
}


static int
pymain_run_file(const PyConfig *config, PyCompilerFlags *cf)
{
    const char *filename = config->run_filename;
    FILE *fp = _Py_fopen(filename, "rb");
    if (fp == NULL) {
        int err = errno;
        fprintf(stderr, "%s: can't open file '%s': [Errno %d] %s\n",
                config->program_name, filename, err, strerror(err));
        return 2;
    }

    if (config->skip_source_first_line) {
        int ch;
        /* Push back first newline so line numbers remain the same */
        while ((ch = getc(fp)) != EOF) {
            if (ch == '\n') {
                (void)ungetc(ch, fp);
                break;
            }
        }
    }

    struct _Py_stat_struct sb;
    if (_Py_fstat_noraise(fileno(fp), &sb) == 0 && S_ISDIR(sb.st_mode)) {
        fprintf(stderr,
                "%s: '%s' is a directory, cannot continue\n",
                config->program_name, filename);
        fclose(fp);
        return 1;
    }
    /* PyRun_AnyFileExFlags(closeit=1) calls fclose(fp) before running code */
    int run = PyRun_AnyFileExFlags(fp, filename, 1, cf);
    return (run != 0);
}



/* Write an exitcode into *exitcode and return 1 if we have to exit Python.
   Return 0 otherwise. */
static int
pymain_run_interactive_hook(int *exitcode)
{
    PyObject *sys, *hook, *result;
    sys = PyImport_ImportModule("sys");
    if (sys == NULL) {
        goto error;
    }

    hook = PyObject_GetAttrString(sys, "__interactivehook__");
    Py_DECREF(sys);
    if (hook == NULL) {
        PyErr_Clear();
        return 0;
    }
    result = _PyObject_CallNoArg(hook);
    Py_DECREF(hook);
    if (result == NULL) {
        goto error;
    }
    Py_DECREF(result);

    return 0;

error:
    PySys_WriteStderr("Failed calling sys.__interactivehook__\n");
    return pymain_err_print(exitcode);
}


static int
pymain_run_stdin(PyConfig *config, PyCompilerFlags *cf)
{
    if (stdin_is_interactive(config)) {
        config->inspect = 0;
        Py_InspectFlag = 0; /* do exit on SystemExit */

        int exitcode;
        if (pymain_run_interactive_hook(&exitcode)) {
            return exitcode;
        }
    }
    int run = PyRun_AnyFileExFlags(stdin, "<stdin>", 0, cf);
    return (run != 0);
}


static void
pymain_repl(PyConfig *config, PyCompilerFlags *cf, int *exitcode)
{
    /* Check this environment variable at the end, to give programs the
       opportunity to set it from Python. */
    if (!config->inspect && _Py_GetEnv(config->use_environment, "PYTHONINSPECT")) {
        config->inspect = 1;
        Py_InspectFlag = 1;
    }

    if (!(config->inspect && stdin_is_interactive(config) && config_run_code(config))) {
        return;
    }

    config->inspect = 0;
    Py_InspectFlag = 0;
    if (pymain_run_interactive_hook(exitcode)) {
        return;
    }

    int res = PyRun_AnyFileExFlags(stdin, "<stdin>", 0, cf);
    *exitcode = (res != 0);
}


static void
pymain_run_python(int *exitcode)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    /* pymain_run_stdin() modify the config */
    PyConfig *config = (PyConfig*)_PyInterpreterState_GetConfig(interp);

    PyObject *main_importer_path = NULL;
    if (config->run_filename != NULL) {
        /* If filename is a package (ex: directory or ZIP file) which contains
           __main__.py, main_importer_path is set to filename and will be
           prepended to sys.path.

           Otherwise, main_importer_path is left unchanged. */
        if (pymain_get_importer(config->run_filename, &main_importer_path,
                                exitcode)) {
            return;
        }
    }

    if (main_importer_path != NULL) {
        if (pymain_sys_path_add_path0(interp, main_importer_path) < 0) {
            goto error;
        }
    }
    else if (1) {
        PyObject *path0 = NULL;
        int res = _PyPathConfig_ComputeSysPath0(&config->argv, &path0);
        if (res < 0) {
            goto error;
        }

        if (res > 0) {
            if (pymain_sys_path_add_path0(interp, path0) < 0) {
                Py_DECREF(path0);
                goto error;
            }
            Py_DECREF(path0);
        }
    }

    PyCompilerFlags cf = _PyCompilerFlags_INIT;

    pymain_header(config);
    pymain_import_readline(config);

    if (config->run_command) {
        *exitcode = pymain_run_command(config->run_command, &cf);
    }
    else if (config->run_module) {
        *exitcode = pymain_run_module(config->run_module, 1);
    }
    else if (main_importer_path != NULL) {
        *exitcode = pymain_run_module("__main__", 0);
    }
    else if (config->run_filename != NULL) {
        *exitcode = pymain_run_file(config, &cf);
    }
    else {
        *exitcode = pymain_run_stdin(config, &cf);
    }

    pymain_repl(config, &cf, exitcode);
    goto done;

error:
    *exitcode = pymain_exit_err_print();

done:
    Py_XDECREF(main_importer_path);
}


/* --- pymain_main() ---------------------------------------------- */

static void
pymain_free(void)
{
    _PyImport_Fini2();

    /* Free global variables which cannot be freed in Py_Finalize():
       configuration options set before Py_Initialize() which should
       remain valid after Py_Finalize(), since
       Py_Initialize()-Py_Finalize() can be called multiple times. */
    _PyPathConfig_ClearGlobal();
    _Py_ClearStandardStreamEncoding();
    _Py_ClearArgcArgv();
    _PyRuntime_Finalize();
}


static void _Py_NO_RETURN
pymain_exit_error(PyStatus status)
{
    if (_PyStatus_IS_EXIT(status)) {
        /* If it's an error rather than a regular exit, leave Python runtime
           alive: Py_ExitStatusException() uses the current exception and use
           sys.stdout in this case. */
        pymain_free();
    }
    Py_ExitStatusException(status);
}


int
Py_RunMain(void)
{
    int exitcode = 0;

    pymain_run_python(&exitcode);

    if (Py_FinalizeEx() < 0) {
        /* Value unlikely to be confused with a non-error exit status or
           other special meaning */
        exitcode = 120;
    }

    pymain_free();
    return exitcode;
}


static int
pymain_main(_PyArgv *args)
{
    PyStatus status = pymain_init(args);
    if (_PyStatus_IS_EXIT(status)) {
        pymain_free();
        return status.exitcode;
    }
    if (_PyStatus_EXCEPTION(status)) {
        pymain_exit_error(status);
    }

    return Py_RunMain();
}

int
Py_BytesMain(int argc, char **argv)
{
    _PyArgv args = {
        .argc = argc,
        .bytes_argv = argv,
    };
    return pymain_main(&args);
}

#ifdef __cplusplus
}
#endif
