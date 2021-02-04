/* Python interpreter top-level routines, including init/exit */

#include "Python.h"

#include "Python-ast.h"
#undef Yield   /* undefine macro conflicting with <winbase.h> */

#include "pycore_ceval.h"         // _PyEval_FiniGIL()
#include "pycore_fileutils.h"     // _Py_ResetForceASCII()
#include "pycore_import.h"        // _PyImport_Cleanup()
#include "pycore_initconfig.h"    // _PyStatus_OK()
#include "pycore_object.h"        // _PyDebug_PrintTotalRefs()
#include "pycore_pathconfig.h"    // _PyConfig_WritePathConfig()
#include "pycore_pyerrors.h"      // _PyErr_Occurred()
#include "pycore_pylifecycle.h"   // _PyErr_Print()
#include "pycore_pystate.h"       // PyThreadState_Get()
#include "pycore_traceback.h"     // _Py_DumpTracebackThreads()

_Py_IDENTIFIER(flush);
_Py_IDENTIFIER(name);
_Py_IDENTIFIER(stdin);
_Py_IDENTIFIER(stdout);
_Py_IDENTIFIER(stderr);
_Py_IDENTIFIER(threading);

#ifdef __cplusplus
extern "C" {
#endif


/* Forward declarations */
static PyStatus add_main_module(PyInterpreterState *interp);
static PyStatus init_set_builtins_open(void);
static PyStatus init_sys_streams(PyThreadState *tstate);
static void call_py_exitfuncs(PyThreadState *tstate);
static void wait_for_thread_shutdown(PyThreadState *tstate);
static void call_ll_exitfuncs(_PyRuntimeState *runtime);

int _Py_UnhandledKeyboardInterrupt = 0;
_PyRuntimeState _PyRuntime = _PyRuntimeState_INIT;
static int runtime_initialized = 0;

PyStatus
_PyRuntime_Initialize(void)
{
    /* XXX We only initialize once in the process, which aligns with
       the static initialization of the former globals now found in
       _PyRuntime.  However, _PyRuntime *should* be initialized with
       every Py_Initialize() call, but doing so breaks the runtime.
       This is because the runtime state is not properly finalized
       currently. */
    if (runtime_initialized) {
        return _PyStatus_OK();
    }
    runtime_initialized = 1;

    return _PyRuntimeState_Init(&_PyRuntime);
}

void
_PyRuntime_Finalize(void)
{
    _PyRuntimeState_Fini(&_PyRuntime);
    runtime_initialized = 0;
}

int
_Py_IsFinalizing(void)
{
    return _PyRuntimeState_GetFinalizing(&_PyRuntime) != NULL;
}

/* Hack to force loading of object files */
int (*_PyOS_mystrnicmp_hack)(const char *, const char *, Py_ssize_t) = \
    PyOS_mystrnicmp; /* Python/pystrcmp.o */

/* APIs to access the initialization flags
 *
 * Can be called prior to Py_Initialize.
 */

int
_Py_IsCoreInitialized(void)
{
    return _PyRuntime.core_initialized;
}

int
Py_IsInitialized(void)
{
    return _PyRuntime.initialized;
}

  
static PyStatus
pyinit_core_reconfigure(_PyRuntimeState *runtime,
                        PyThreadState **tstate_p,
                        const PyConfig *config)
{
    PyStatus status;
    PyThreadState *tstate = PyThreadState_Get();
    if (!tstate) {
        return _PyStatus_ERR("failed to read thread state");
    }
    *tstate_p = tstate;

    PyInterpreterState *interp = tstate->interp;
    if (interp == NULL) {
        return _PyStatus_ERR("can't make main interpreter");
    }

    status = _PyConfig_Write(config, runtime);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    status = _PyInterpreterState_SetConfig(interp, config);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    config = _PyInterpreterState_GetConfig(interp);

    if (config->_install_importlib) {
        status = _PyConfig_WritePathConfig(config);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }
    return _PyStatus_OK();
}


static PyStatus
pycore_init_runtime(_PyRuntimeState *runtime,
                    const PyConfig *config)
{
    if (runtime->initialized) {
        return _PyStatus_ERR("main interpreter already initialized");
    }

    PyStatus status = _PyConfig_Write(config, runtime);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    /* Py_Finalize leaves _Py_Finalizing set in order to help daemon
     * threads behave a little more gracefully at interpreter shutdown.
     * We clobber it here so the new interpreter can start with a clean
     * slate.
     *
     * However, this may still lead to misbehaviour if there are daemon
     * threads still hanging around from a previous Py_Initialize/Finalize
     * pair :(
     */
    _PyRuntimeState_SetFinalizing(runtime, NULL);
    return _PyStatus_OK();
}



static PyStatus
pycore_create_interpreter(_PyRuntimeState *runtime,
                          const PyConfig *config,
                          PyThreadState **tstate_p)
{
  extern PyThreadState *_PyCurrent;
  
    PyInterpreterState *interp = PyInterpreterState_New();
    if (interp == NULL) {
        return _PyStatus_ERR("can't make main interpreter");
    }

    PyStatus status = _PyInterpreterState_SetConfig(interp, config);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    PyThreadState *tstate = PyThreadState_New(interp);
    if (tstate == NULL) {
        return _PyStatus_ERR("can't make first thread");
    }
    _PyCurrent = tstate;
    *tstate_p = tstate;
    return _PyStatus_OK();
}


static PyStatus
pycore_init_types(PyThreadState *tstate)
{
    PyStatus status;
    int is_main_interp = 1;
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    if (is_main_interp) {
        status = _PyTypes_Init();
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }


    if (!_PyLong_Init(tstate)) {
        return _PyStatus_ERR("can't init longs");
    }

    if (is_main_interp) {
        status = _PyUnicode_Init();
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    status = _PyExc_Init();
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    if (is_main_interp) {
        if (!_PyFloat_Init()) {
            return _PyStatus_ERR("can't init float");
        }

        if (_PyStructSequence_Init() < 0) {
            return _PyStatus_ERR("can't initialize structseq");
        }
    }

    status = _PyErr_Init();
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    
    return _PyStatus_OK();
}


static PyStatus
pycore_init_builtins(PyThreadState *tstate)
{
    assert(!_PyErr_Occurred(tstate));

    PyObject *bimod = _PyBuiltin_Init(tstate);
    if (bimod == NULL) {
        goto error;
    }

    PyInterpreterState *interp = tstate->interp;
    if (_PyImport_FixupBuiltin(bimod, "builtins", interp->modules) < 0) {
        goto error;
    }

    PyObject *builtins_dict = PyModule_GetDict(bimod);
    if (builtins_dict == NULL) {
        goto error;
    }
    Py_INCREF(builtins_dict);
    interp->builtins = builtins_dict;

    PyStatus status = _PyBuiltins_AddExceptions(bimod);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    interp->builtins_copy = PyDict_Copy(interp->builtins);
    if (interp->builtins_copy == NULL) {
        goto error;
    }
    Py_DECREF(bimod);

    assert(!_PyErr_Occurred(tstate));

    return _PyStatus_OK();

error:
    Py_XDECREF(bimod);
    return _PyStatus_ERR("can't initialize builtins module");
}

static PyStatus
pycore_interp_init(PyThreadState *tstate)
{
    PyStatus status;
    PyObject *sysmod = NULL;

    status = pycore_init_types(tstate);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    status = _PySys_Create(tstate, &sysmod);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    status = pycore_init_builtins(tstate);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }
    
done:
    /* sys.modules['sys'] contains a strong reference to the module */
    Py_XDECREF(sysmod);
    return status;
}


static PyStatus
pyinit_config(_PyRuntimeState *runtime,
              PyThreadState **tstate_p,
              const PyConfig *config)
{
    PyStatus status = pycore_init_runtime(runtime, config);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    PyThreadState *tstate;
    status = pycore_create_interpreter(runtime, config, &tstate);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    *tstate_p = tstate;

    status = pycore_interp_init(tstate);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    /* Only when we get here is the runtime core fully initialized */
    runtime->core_initialized = 1;
    return _PyStatus_OK();
}


PyStatus
_Py_PreInitializeFromPyArgv(const PyPreConfig *src_config, const _PyArgv *args)
{
    PyStatus status;

    if (src_config == NULL) {
        return _PyStatus_ERR("preinitialization config is NULL");
    }

    status = _PyRuntime_Initialize();
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    _PyRuntimeState *runtime = &_PyRuntime;

    if (runtime->preinitialized) {
        /* If it's already configured: ignored the new configuration */
        return _PyStatus_OK();
    }

    /* Note: preinitialized remains 1 on error, it is only set to 0
       at exit on success. */
    runtime->preinitializing = 1;

    PyPreConfig config;

    status = _PyPreConfig_InitFromPreConfig(&config, src_config);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    status = _PyPreConfig_Read(&config, args);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    status = _PyPreConfig_Write(&config);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    runtime->preinitializing = 0;
    runtime->preinitialized = 1;
    return _PyStatus_OK();
}


PyStatus
Py_PreInitializeFromBytesArgs(const PyPreConfig *src_config, Py_ssize_t argc, char **argv)
{
    _PyArgv args = {.argc = argc, .bytes_argv = argv};
    return _Py_PreInitializeFromPyArgv(src_config, &args);
}

PyStatus
Py_PreInitialize(const PyPreConfig *src_config)
{
    return _Py_PreInitializeFromPyArgv(src_config, NULL);
}


PyStatus
_Py_PreInitializeFromConfig(const PyConfig *config,
                            const _PyArgv *args)
{
    assert(config != NULL);

    PyStatus status = _PyRuntime_Initialize();
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    _PyRuntimeState *runtime = &_PyRuntime;

    if (runtime->preinitialized) {
        /* Already initialized: do nothing */
        return _PyStatus_OK();
    }

    PyPreConfig preconfig;

    _PyPreConfig_InitFromConfig(&preconfig, config);

    if (!config->parse_argv) {
        return Py_PreInitialize(&preconfig);
    }
    else if (args == NULL) {
        _PyArgv config_args = {
            .argc = config->argv.length,
            .bytes_argv = config->argv.items};
        return _Py_PreInitializeFromPyArgv(&preconfig, &config_args);
    }
    else {
        return _Py_PreInitializeFromPyArgv(&preconfig, args);
    }
}


/* Begin interpreter initialization
 *
 * On return, the first thread and interpreter state have been created,
 * but the compiler, signal handling, multithreading and
 * multiple interpreter support, and codec infrastructure are not yet
 * available.
 *
 * The import system will support builtin and frozen modules only.
 * The only supported io is writing to sys.stderr
 *
 * If any operation invoked by this function fails, a fatal error is
 * issued and the function does not return.
 *
 * Any code invoked from this function should *not* assume it has access
 * to the Python C API (unless the API is explicitly listed as being
 * safe to call without calling Py_Initialize first)
 */
static PyStatus
pyinit_core(_PyRuntimeState *runtime,
            const PyConfig *src_config,
            PyThreadState **tstate_p)
{
    PyStatus status;

    status = _Py_PreInitializeFromConfig(src_config, NULL);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    PyConfig config;
    _PyConfig_InitCompatConfig(&config);

    status = _PyConfig_Copy(&config, src_config);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    status = PyConfig_Read(&config);
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

    if (!runtime->core_initialized) {
        status = pyinit_config(runtime, tstate_p, &config);
    }
    else {
        status = pyinit_core_reconfigure(runtime, tstate_p, &config);
    }
    if (_PyStatus_EXCEPTION(status)) {
        goto done;
    }

done:
    PyConfig_Clear(&config);
    return status;
}


/* Py_Initialize() has already been called: update the main interpreter
   configuration. Example of bpo-34008: Py_Main() called after
   Py_Initialize(). */
static PyStatus
_Py_ReconfigureMainInterpreter(PyThreadState *tstate)
{
    const PyConfig *config = _PyInterpreterState_GetConfig(tstate->interp);

    PyObject *argv = _PyStringList_AsList(&config->argv);
    if (argv == NULL) {
        return _PyStatus_NO_MEMORY(); \
    }

    int res = PyDict_SetItemString(tstate->interp->sysdict, "argv", argv);
    Py_DECREF(argv);
    if (res < 0) {
        return _PyStatus_ERR("fail to set sys.argv");
    }
    return _PyStatus_OK();
}


static PyStatus
init_interp_main(PyThreadState *tstate)
{
    assert(!_PyErr_Occurred(tstate));

    PyStatus status;
    int is_main_interp = 1;
    PyInterpreterState *interp = tstate->interp;
    const PyConfig *config = _PyInterpreterState_GetConfig(interp);

    if (!config->_install_importlib) {
        /* Special mode for freeze_importlib: run with no import system
         *
         * This means anything which needs support from extension modules
         * or pure Python code in the standard library won't work.
         */
        if (is_main_interp) {
            interp->runtime->initialized = 1;
        }
        return _PyStatus_OK();
    }

    if (_PySys_InitMain(tstate) < 0) {
        return _PyStatus_ERR("can't finish initializing sys");
    }
    
    status = init_sys_streams(tstate);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    status = init_set_builtins_open();
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    status = add_main_module(interp);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }


    
    if (is_main_interp) {
        /* Initialize warnings. */
        PyObject *warnoptions = PySys_GetObject("warnoptions");
        if (warnoptions != NULL && PyList_Size(warnoptions) > 0)
        {
            PyObject *warnings_module = PyImport_ImportModule("warnings");
            if (warnings_module == NULL) {
                fprintf(stderr, "'import warnings' failed; traceback:\n");
                _PyErr_Print(tstate);
            }
            Py_XDECREF(warnings_module);
        }

        interp->runtime->initialized = 1;
    }

    assert(!_PyErr_Occurred(tstate));

    return _PyStatus_OK();
}


/* Update interpreter state based on supplied configuration settings
 *
 * After calling this function, most of the restrictions on the interpreter
 * are lifted. The only remaining incomplete settings are those related
 * to the main module (sys.argv[0], __main__ metadata)
 *
 * Calling this when the interpreter is not initializing, is already
 * initialized or without a valid current thread state is a fatal error.
 * Other errors should be reported as normal Python exceptions with a
 * non-zero return code.
 */
static PyStatus
pyinit_main(PyThreadState *tstate)
{
    PyInterpreterState *interp = tstate->interp;
    if (!interp->runtime->core_initialized) {
        return _PyStatus_ERR("runtime core not initialized");
    }

    if (interp->runtime->initialized) {
        return _Py_ReconfigureMainInterpreter(tstate);
    }

    PyStatus status = init_interp_main(tstate);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    return _PyStatus_OK();
}


PyStatus
_Py_InitializeMain(void)
{
    PyStatus status = _PyRuntime_Initialize();
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    PyThreadState *tstate = PyThreadState_Get();
    return pyinit_main(tstate);
}


PyStatus
Py_InitializeFromConfig(const PyConfig *config)
{
    if (config == NULL) {
        return _PyStatus_ERR("initialization config is NULL");
    }

    PyStatus status;

    status = _PyRuntime_Initialize();
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    _PyRuntimeState *runtime = &_PyRuntime;

    PyThreadState *tstate = NULL;
    status = pyinit_core(runtime, config, &tstate);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }
    config = _PyInterpreterState_GetConfig(tstate->interp);

    if (config->_init_main) {
        status = pyinit_main(tstate);
        if (_PyStatus_EXCEPTION(status)) {
            return status;
        }
    }

    return _PyStatus_OK();
}


void
Py_InitializeEx(int install_sigs)
{
    PyStatus status;

    status = _PyRuntime_Initialize();
    if (_PyStatus_EXCEPTION(status)) {
        Py_ExitStatusException(status);
    }
    _PyRuntimeState *runtime = &_PyRuntime;

    if (runtime->initialized) {
        /* bpo-33932: Calling Py_Initialize() twice does nothing. */
        return;
    }

    PyConfig config;
    _PyConfig_InitCompatConfig(&config);

    status = Py_InitializeFromConfig(&config);
    if (_PyStatus_EXCEPTION(status)) {
        Py_ExitStatusException(status);
    }
}

void
Py_Initialize(void)
{
    Py_InitializeEx(1);
}


/* Flush stdout and stderr */

static int
file_is_closed(PyObject *fobj)
{
    int r;
    PyObject *tmp = PyObject_GetAttrString(fobj, "closed");
    if (tmp == NULL) {
        PyErr_Clear();
        return 0;
    }
    r = PyObject_IsTrue(tmp);
    Py_DECREF(tmp);
    if (r < 0)
        PyErr_Clear();
    return r > 0;
}

static int
flush_std_files(void)
{
    PyObject *fout = _PySys_GetObjectId(&PyId_stdout);
    PyObject *ferr = _PySys_GetObjectId(&PyId_stderr);
    PyObject *tmp;
    int status = 0;

    if (fout != NULL && fout != Py_None && !file_is_closed(fout)) {
        tmp = _PyObject_CallMethodIdNoArgs(fout, &PyId_flush);
        if (tmp == NULL) {
            PyErr_WriteUnraisable(fout);
            status = -1;
        }
        else
            Py_DECREF(tmp);
    }

    if (ferr != NULL && ferr != Py_None && !file_is_closed(ferr)) {
        tmp = _PyObject_CallMethodIdNoArgs(ferr, &PyId_flush);
        if (tmp == NULL) {
            PyErr_Clear();
            status = -1;
        }
        else
            Py_DECREF(tmp);
    }

    return status;
}

/* Undo the effect of Py_Initialize().

   Beware: if multiple interpreter and/or thread states exist, these
   are not wiped out; only the current thread and interpreter state
   are deleted.  But since everything else is deleted, those other
   interpreter and thread states should no longer be used.

   (XXX We should do better, e.g. wipe out all interpreters and
   threads.)

   Locking: as above.

*/


static void
finalize_interp_types(PyThreadState *tstate, int is_main_interp)
{
    _PyFrame_Fini(tstate);

    if (is_main_interp) {
        _PySet_Fini();
    }
    if (is_main_interp) {
        _PyDict_Fini();
    }
    _PyList_Fini(tstate);
    _PyTuple_Fini(tstate);

    _PySlice_Fini(tstate);
    _PyUnicode_Fini(tstate);
    _PyFloat_Fini(tstate);
    _PyLong_Fini(tstate);
}


static void
finalize_interp_clear(PyThreadState *tstate)
{
  int is_main_interp = 1;

    /* Clear interpreter state and all thread states */
    PyInterpreterState_Clear(tstate->interp);
    
    if (is_main_interp) {
        _PyArg_Fini();
    }

    if (is_main_interp) {
        _PyExc_Fini();
    }

    finalize_interp_types(tstate, is_main_interp);
}


static void
finalize_interp_delete(PyThreadState *tstate)
{
    PyInterpreterState_Delete(tstate->interp);
}


int
Py_FinalizeEx(void)
{
    int status = 0;

    _PyRuntimeState *runtime = &_PyRuntime;
    if (!runtime->initialized) {
        return status;
    }

    /* Get current thread state and interpreter pointer */
    PyThreadState *tstate = PyThreadState_Get();

    // Wrap up existing "threading"-module-created, non-daemon threads.
    wait_for_thread_shutdown(tstate);

    /* The interpreter is still entirely intact at this point, and the
     * exit funcs may be relying on that.  In particular, if some thread
     * or exit func is still waiting to do an import, the import machinery
     * expects Py_IsInitialized() to return true.  So don't say the
     * runtime is uninitialized until after the exit funcs have run.
     * Note that Threading.py uses an exit func to do a join on all the
     * threads created thru it, so this also protects pending imports in
     * the threads created via Threading.
     */

    call_py_exitfuncs(tstate);

    /* Copy the core config, PyInterpreterState_Delete() free
       the core config memory */

    /* Remaining daemon threads will automatically exit
       when they attempt to take the GIL (ex: PyEval_RestoreThread()). */
    _PyRuntimeState_SetFinalizing(runtime, tstate);
    runtime->initialized = 0;
    runtime->core_initialized = 0;

    /* Flush sys.stdout and sys.stderr */
    if (flush_std_files() < 0) {
        status = -1;
    }

    /* Destroy all modules */
    _PyImport_Cleanup(tstate);

    /* Flush sys.stdout and sys.stderr (again, in case more was printed) */
    if (flush_std_files() < 0) {
        status = -1;
    }

    /* Destroy the database used by _PyImport_{Fixup,Find}Extension */
    _PyImport_Fini();

    /* Cleanup typeobject.c's internal caches. */
    _PyType_Fini();

    /* dump hash stats */
    _PyHash_Fini();

    finalize_interp_clear(tstate);
    finalize_interp_delete(tstate);

    call_ll_exitfuncs(runtime);

    _PyRuntime_Finalize();
    return status;
}

void
Py_Finalize(void)
{
    Py_FinalizeEx();
}

/* Add the __main__ module */

static PyStatus
add_main_module(PyInterpreterState *interp)
{
  PyObject *m, *d;
    m = PyImport_AddModule("__main__");
    if (m == NULL)
        return _PyStatus_ERR("can't create __main__ module");

    d = PyModule_GetDict(m);
    if (PyDict_GetItemString(d, "__builtins__") == NULL) {
        PyObject *bimod = PyImport_ImportModule("builtins");
        if (bimod == NULL) {
            return _PyStatus_ERR("Failed to retrieve builtins module");
        }
        if (PyDict_SetItemString(d, "__builtins__", bimod) < 0) {
            return _PyStatus_ERR("Failed to initialize __main__.__builtins__");
        }
        Py_DECREF(bimod);
    }
    
    return _PyStatus_OK();
}

/* Check if a file descriptor is valid or not.
   Return 0 if the file descriptor is invalid, return non-zero otherwise. */
static int
is_valid_fd(int fd)
{
    struct stat st;
    return (fstat(fd, &st) == 0);
}

/* returns Py_None if the fd is not valid */
static PyObject*
create_stdio(const PyConfig *config, PyObject* io,
    int fd, int write_mode, const char* name,
    const char* encoding, const char* errors)
{
    PyObject *text = NULL, *raw = NULL, *res;
    const char* mode;
    int buffering, isatty;
    _Py_IDENTIFIER(open);
    _Py_IDENTIFIER(isatty);
    const int buffered_stdio = config->buffered_stdio;

    if (!is_valid_fd(fd))
        Py_RETURN_NONE;

    /* stdin is always opened in buffered mode, first because it shouldn't
       make a difference in common use cases, second because TextIOWrapper
       depends on the presence of a read1() method which only exists on
       buffered streams.
    */
    if (!buffered_stdio && write_mode)
        buffering = 0;
    else
        buffering = -1;
    if (write_mode)
        mode = "wb";
    else
        mode = "rb";
    raw = _PyObject_CallMethodId(io, &PyId_open, "is",
                                 fd, mode, buffering);
    if (raw == NULL)
        goto error;
    
    text = PyString_FromString(name);
    if (text == NULL || _PyObject_SetAttrId(raw, &PyId_name, text) < 0)
        goto error;
    res = _PyObject_CallMethodIdNoArgs(raw, &PyId_isatty);
    if (res == NULL)
        goto error;
    isatty = PyObject_IsTrue(res);
    Py_DECREF(res);
    if (isatty == -1)
        goto error;
    return raw;

error:
    Py_XDECREF(raw);
    if (PyErr_ExceptionMatches(PyExc_OSError) && !is_valid_fd(fd)) {
        /* Issue #24891: the file descriptor was closed after the first
           is_valid_fd() check was called. Ignore the OSError and set the
           stream to None. */
        PyErr_Clear();
        Py_RETURN_NONE;
    }
    return NULL;
}

/* Set builtins.open to io.OpenWrapper */
static PyStatus
init_set_builtins_open(void)
{
    PyObject *iomod = NULL, *wrapper;
    PyObject *bimod = NULL;
    PyStatus res = _PyStatus_OK();

    if (!(iomod = PyImport_ImportModule("io"))) {
        goto error;
    }

    if (!(bimod = PyImport_ImportModule("builtins"))) {
        goto error;
    }

    if (!(wrapper = PyObject_GetAttrString(iomod, "OpenWrapper"))) {
        goto error;
    }

    /* Set builtins.open */
    if (PyObject_SetAttrString(bimod, "open", wrapper) == -1) {
        Py_DECREF(wrapper);
        goto error;
    }
    Py_DECREF(wrapper);
    goto done;

error:
    res = _PyStatus_ERR("can't initialize io.open");

done:
    Py_XDECREF(bimod);
    Py_XDECREF(iomod);
    return res;
}


/* Initialize sys.stdin, stdout, stderr and builtins.open */
static PyStatus
init_sys_streams(PyThreadState *tstate)
{
    PyObject *iomod = NULL;
    PyObject *std = NULL;
    int fd;

    PyStatus res = _PyStatus_OK();
    const PyConfig *config = _PyInterpreterState_GetConfig(tstate->interp);

    /* Check that stdin is not a directory
       Using shell redirection, you can redirect stdin to a directory,
       crashing the Python interpreter. Catch this common mistake here
       and output a useful error message. Note that under MS Windows,
       the shell already prevents that. */
    struct _Py_stat_struct sb;
    if (_Py_fstat_noraise(fileno(stdin), &sb) == 0 &&
        S_ISDIR(sb.st_mode)) {
        return _PyStatus_ERR("<stdin> is a directory, cannot continue");
    }

    if (!(iomod = PyImport_ImportModule("io"))) {
        goto error;
    }

    /* Set sys.stdin */
    fd = fileno(stdin);
    /* Under some conditions stdin, stdout and stderr may not be connected
     * and fileno() may point to an invalid file descriptor. For example
     * GUI apps don't have valid standard streams by default.
     */
    std = create_stdio(config, iomod, fd, 0, "<stdin>",
                       NULL, NULL);
    if (std == NULL)
        goto error;
    PySys_SetObject("__stdin__", std);
    _PySys_SetObjectId(&PyId_stdin, std);
    Py_DECREF(std);

    /* Set sys.stdout */
    fd = fileno(stdout);
    std = create_stdio(config, iomod, fd, 1, "<stdout>",
                       NULL, NULL);
    if (std == NULL)
        goto error;
    PySys_SetObject("__stdout__", std);
    _PySys_SetObjectId(&PyId_stdout, std);
    Py_DECREF(std);

    goto done;

error:
    res = _PyStatus_ERR("can't initialize sys standard streams");

done:
    _Py_ClearStandardStreamEncoding();
    Py_XDECREF(iomod);
    return res;
}


static void
_Py_FatalError_DumpTracebacks(int fd, PyInterpreterState *interp,
                              PyThreadState *tstate)
{
    fputc('\n', stderr);
    fflush(stderr);

    /* display the current Python stack */
    _Py_DumpTracebackThreads(fd, interp, tstate);
}

/* Print the current exception (if an exception is set) with its traceback,
   or display the current Python stack.

   Don't call PyErr_PrintEx() and the except hook, because Py_FatalError() is
   called on catastrophic cases.

   Return 1 if the traceback was displayed, 0 otherwise. */

static int
_Py_FatalError_PrintExc(PyThreadState *tstate)
{
    PyObject *ferr, *res;
    PyObject *exception, *v, *tb;
    int has_tb;

    _PyErr_Fetch(tstate, &exception, &v, &tb);
    if (exception == NULL) {
        /* No current exception */
        return 0;
    }

    ferr = _PySys_GetObjectId(&PyId_stderr);
    if (ferr == NULL || ferr == Py_None) {
        /* sys.stderr is not set yet or set to None,
           no need to try to display the exception */
        return 0;
    }

    _PyErr_NormalizeException(tstate, &exception, &v, &tb);
    if (tb == NULL) {
        tb = Py_None;
        Py_INCREF(tb);
    }
    PyException_SetTraceback(v, tb);
    if (exception == NULL) {
        /* PyErr_NormalizeException() failed */
        return 0;
    }

    has_tb = (tb != Py_None);
    PyErr_Display(exception, v, tb);
    Py_XDECREF(exception);
    Py_XDECREF(v);
    Py_XDECREF(tb);

    /* sys.stderr may be buffered: call sys.stderr.flush() */
    res = _PyObject_CallMethodIdNoArgs(ferr, &PyId_flush);
    if (res == NULL) {
        _PyErr_Clear(tstate);
    }
    else {
        Py_DECREF(res);
    }

    return has_tb;
}

/* Print fatal error message and abort */

static void
fatal_error_dump_runtime(FILE *stream, _PyRuntimeState *runtime)
{
    fprintf(stream, "Python runtime state: ");
    PyThreadState *finalizing = _PyRuntimeState_GetFinalizing(runtime);
    if (finalizing) {
        fprintf(stream, "finalizing (tstate=%p)", finalizing);
    }
    else if (runtime->initialized) {
        fprintf(stream, "initialized");
    }
    else if (runtime->core_initialized) {
        fprintf(stream, "core initialized");
    }
    else if (runtime->preinitialized) {
        fprintf(stream, "preinitialized");
    }
    else if (runtime->preinitializing) {
        fprintf(stream, "preinitializing");
    }
    else {
        fprintf(stream, "unknown");
    }
    fprintf(stream, "\n");
    fflush(stream);
}


static inline void _Py_NO_RETURN
fatal_error_exit(int status)
{
    if (status < 0) {
        abort();
    }
    else {
        exit(status);
    }
}


static void _Py_NO_RETURN
fatal_error(FILE *stream, int header, const char *prefix, const char *msg,
            int status)
{
    const int fd = fileno(stream);
    static int reentrant = 0;

    if (reentrant) {
        /* Py_FatalError() caused a second fatal error.
           Example: flush_std_files() raises a recursion error. */
        fatal_error_exit(status);
    }
    reentrant = 1;

    if (header) {
        fprintf(stream, "Fatal Python error: ");
        if (prefix) {
            fputs(prefix, stream);
            fputs(": ", stream);
        }
        if (msg) {
            fputs(msg, stream);
        }
        else {
            fprintf(stream, "<message not set>");
        }
        fputs("\n", stream);
        fflush(stream);
    }

    _PyRuntimeState *runtime = &_PyRuntime;
    fatal_error_dump_runtime(stream, runtime);

    PyThreadState *tstate = PyThreadState_Get();
    PyInterpreterState *interp = NULL;
    if (tstate != NULL) {
        interp = tstate->interp;
    }

    /* Check if the current thread has a Python thread state
       and holds the GIL.

       tss_tstate is NULL if Py_FatalError() is called from a C thread which
       has no Python thread state.

       tss_tstate != tstate if the current Python thread does not hold the GIL.
       */
    /* If an exception is set, print the exception with its traceback */
    if (!_Py_FatalError_PrintExc(tstate)) {
      /* No exception is set, or an exception is set without traceback */
      _Py_FatalError_DumpTracebacks(fd, interp, tstate);
    }
    flush_std_files();
    fatal_error_exit(status);
}


#undef Py_FatalError

void _Py_NO_RETURN
Py_FatalError(const char *msg)
{
    fatal_error(stderr, 1, NULL, msg, -1);
}


void _Py_NO_RETURN
_Py_FatalErrorFunc(const char *func, const char *msg)
{
    fatal_error(stderr, 1, func, msg, -1);
}


void _Py_NO_RETURN
_Py_FatalErrorFormat(const char *func, const char *format, ...)
{
    static int reentrant = 0;
    if (reentrant) {
        /* _Py_FatalErrorFormat() caused a second fatal error */
        fatal_error_exit(-1);
    }
    reentrant = 1;

    FILE *stream = stderr;
    fprintf(stream, "Fatal Python error: ");
    if (func) {
        fputs(func, stream);
        fputs(": ", stream);
    }
    fflush(stream);

    va_list vargs;
    va_start(vargs, format);
    vfprintf(stream, format, vargs);
    va_end(vargs);

    fputs("\n", stream);
    fflush(stream);

    fatal_error(stream, 0, NULL, NULL, -1);
}


void _Py_NO_RETURN
Py_ExitStatusException(PyStatus status)
{
    if (_PyStatus_IS_EXIT(status)) {
        exit(status.exitcode);
    }
    else if (_PyStatus_IS_ERROR(status)) {
        fatal_error(stderr, 1, status.func, status.err_msg, 1);
    }
    else {
        Py_FatalError("Py_ExitStatusException() must not be called on success");
    }
}

/* Clean up and exit */

/* For the atexit module. */
void _Py_PyAtExit(void (*func)(PyObject *), PyObject *module)
{
    PyInterpreterState *is = _PyInterpreterState_GET();

    /* Guard against API misuse (see bpo-17852) */
    assert(is->pyexitfunc == NULL || is->pyexitfunc == func);

    is->pyexitfunc = func;
    is->pyexitmodule = module;
}

static void
call_py_exitfuncs(PyThreadState *tstate)
{
    PyInterpreterState *interp = tstate->interp;
    if (interp->pyexitfunc == NULL)
        return;

    (*interp->pyexitfunc)(interp->pyexitmodule);
    _PyErr_Clear(tstate);
}

/* Wait until threading._shutdown completes, provided
   the threading module was imported in the first place.
   The shutdown routine will wait until all non-daemon
   "threading" threads have completed. */
static void
wait_for_thread_shutdown(PyThreadState *tstate)
{
    _Py_IDENTIFIER(_shutdown);
    PyObject *result;
    PyObject *threading = _PyImport_GetModuleId(&PyId_threading);
    if (threading == NULL) {
        if (_PyErr_Occurred(tstate)) {
            PyErr_WriteUnraisable(NULL);
        }
        /* else: threading not imported */
        return;
    }
    result = _PyObject_CallMethodIdNoArgs(threading, &PyId__shutdown);
    if (result == NULL) {
        PyErr_WriteUnraisable(threading);
    }
    else {
        Py_DECREF(result);
    }
    Py_DECREF(threading);
}

#define NEXITFUNCS 32
int Py_AtExit(void (*func)(void))
{
    if (_PyRuntime.nexitfuncs >= NEXITFUNCS)
        return -1;
    _PyRuntime.exitfuncs[_PyRuntime.nexitfuncs++] = func;
    return 0;
}

static void
call_ll_exitfuncs(_PyRuntimeState *runtime)
{
    while (runtime->nexitfuncs > 0) {
        /* pop last function from the list */
        runtime->nexitfuncs--;
        void (*exitfunc)(void) = runtime->exitfuncs[runtime->nexitfuncs];
        runtime->exitfuncs[runtime->nexitfuncs] = NULL;

        exitfunc();
    }

    fflush(stdout);
    fflush(stderr);
}

void _Py_NO_RETURN
Py_Exit(int sts)
{
    if (Py_FinalizeEx() < 0) {
        sts = 120;
    }

    exit(sts);
}

/*
 * The file descriptor fd is considered ``interactive'' if either
 *   a) isatty(fd) is TRUE, or
 *   b) the -i flag was given, and the filename associated with
 *      the descriptor is NULL or "<stdin>" or "???".
 */
int
Py_FdIsInteractive(FILE *fp, const char *filename)
{
    if (isatty((int)fileno(fp)))
        return 1;
    if (!Py_InteractiveFlag)
        return 0;
    return (filename == NULL) ||
           (strcmp(filename, "<stdin>") == 0) ||
           (strcmp(filename, "???") == 0);
}


#ifdef __cplusplus
}
#endif
