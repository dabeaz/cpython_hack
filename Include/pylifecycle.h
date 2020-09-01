
/* Interfaces to configure, query, create & destroy the Python runtime */

#ifndef Py_PYLIFECYCLE_H
#define Py_PYLIFECYCLE_H
#ifdef __cplusplus
extern "C" {
#endif


/* Initialization and finalization */
PyAPI_FUNC(void) Py_Initialize(void);
PyAPI_FUNC(void) Py_InitializeEx(int);
PyAPI_FUNC(void) Py_Finalize(void);
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03060000
PyAPI_FUNC(int) Py_FinalizeEx(void);
#endif
PyAPI_FUNC(int) Py_IsInitialized(void);

/* Subinterpreter support */
PyAPI_FUNC(PyThreadState *) Py_NewInterpreter(void);
PyAPI_FUNC(void) Py_EndInterpreter(PyThreadState *);


/* Py_PyAtExit is for the atexit module, Py_AtExit is for low-level
 * exit functions.
 */
PyAPI_FUNC(int) Py_AtExit(void (*func)(void));

PyAPI_FUNC(void) _Py_NO_RETURN Py_Exit(int);

/* Bootstrap __main__ (defined in Modules/main.c) */
PyAPI_FUNC(int) Py_BytesMain(int argc, char **argv);

/* In pathconfig.c */
PyAPI_FUNC(void) Py_SetProgramName(const char *);
PyAPI_FUNC(char *) Py_GetProgramName(void);

PyAPI_FUNC(void) Py_SetPythonHome(const char *);
PyAPI_FUNC(char *) Py_GetPythonHome(void);

PyAPI_FUNC(char *) Py_GetProgramFullPath(void);

PyAPI_FUNC(char *) Py_GetPrefix(void);
PyAPI_FUNC(char *) Py_GetExecPrefix(void);
PyAPI_FUNC(char *) Py_GetPath(void);
PyAPI_FUNC(void)      Py_SetPath(const char *);

/* In their own files */
PyAPI_FUNC(const char *) Py_GetVersion(void);
PyAPI_FUNC(const char *) Py_GetPlatform(void);
PyAPI_FUNC(const char *) Py_GetCopyright(void);
PyAPI_FUNC(const char *) Py_GetCompiler(void);
PyAPI_FUNC(const char *) Py_GetBuildInfo(void);

/* Signals */
typedef void (*PyOS_sighandler_t)(int);
PyAPI_FUNC(PyOS_sighandler_t) PyOS_getsig(int);
PyAPI_FUNC(PyOS_sighandler_t) PyOS_setsig(int, PyOS_sighandler_t);

/* Only used by applications that embed the interpreter and need to
 * override the standard encoding determination mechanism
 */
PyAPI_FUNC(int) Py_SetStandardStreamEncoding(const char *encoding,
                                             const char *errors);

/* PEP 432 Multi-phase initialization API (Private while provisional!) */

PyAPI_FUNC(PyStatus) Py_PreInitialize(
    const PyPreConfig *src_config);
PyAPI_FUNC(PyStatus) Py_PreInitializeFromBytesArgs(
    const PyPreConfig *src_config,
    Py_ssize_t argc,
    char **argv);

PyAPI_FUNC(int) _Py_IsCoreInitialized(void);


/* Initialization and finalization */

PyAPI_FUNC(PyStatus) Py_InitializeFromConfig(
    const PyConfig *config);
PyAPI_FUNC(PyStatus) _Py_InitializeMain(void);

PyAPI_FUNC(int) Py_RunMain(void);


PyAPI_FUNC(void) _Py_NO_RETURN Py_ExitStatusException(PyStatus err);

/* Py_PyAtExit is for the atexit module, Py_AtExit is for low-level
 * exit functions.
 */
PyAPI_FUNC(void) _Py_PyAtExit(void (*func)(PyObject *), PyObject *);

/* Restore signals that the interpreter has called SIG_IGN on to SIG_DFL. */
PyAPI_FUNC(void) _Py_RestoreSignals(void);

PyAPI_FUNC(int) Py_FdIsInteractive(FILE *, const char *);

PyAPI_FUNC(void) _Py_SetProgramFullPath(const char *);

PyAPI_FUNC(const char *) _Py_gitidentifier(void);
PyAPI_FUNC(const char *) _Py_gitversion(void);

PyAPI_FUNC(int) _Py_IsFinalizing(void);

/* Random */
PyAPI_FUNC(int) _PyOS_URandom(void *buffer, Py_ssize_t size);
PyAPI_FUNC(int) _PyOS_URandomNonblock(void *buffer, Py_ssize_t size);

/* Legacy locale support */
PyAPI_FUNC(int) _Py_CoerceLegacyLocale(int warn);
PyAPI_FUNC(int) _Py_LegacyLocaleDetected(int warn);
PyAPI_FUNC(char *) _Py_SetLocaleFromEnv(int category);

PyAPI_FUNC(PyThreadState *) _Py_NewInterpreter(int isolated_subinterpreter);

#ifdef __cplusplus
}
#endif
#endif /* !Py_PYLIFECYCLE_H */
