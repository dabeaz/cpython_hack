#ifndef Py_INTERNAL_LIFECYCLE_H
#define Py_INTERNAL_LIFECYCLE_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

/* Forward declarations */
struct _PyArgv;
struct pyruntimestate;

/* True if the main interpreter thread exited due to an unhandled
 * KeyboardInterrupt exception, suggesting the user pressed ^C. */
PyAPI_DATA(int) _Py_UnhandledKeyboardInterrupt;

PyAPI_FUNC(void) _Py_ClearStandardStreamEncoding(void);

PyAPI_FUNC(int) _Py_IsLocaleCoercionTarget(const char *ctype_loc);

/* Various one-time initializers */

extern PyStatus _PyUnicode_Init(void);
extern int _PyStructSequence_Init(void);
extern int _PyLong_Init(PyThreadState *tstate);
extern PyObject * _PyBuiltin_Init(PyThreadState *tstate);
extern PyStatus _PySys_Create(
    PyThreadState *tstate,
    PyObject **sysmod_p);
extern int _PySys_InitMain(PyThreadState *tstate);
extern PyStatus _PyExc_Init(void);
extern PyStatus _PyErr_Init(void);
extern PyStatus _PyBuiltins_AddExceptions(PyObject * bltinmod);
extern int _PyFloat_Init(void);
extern PyStatus _PyTypes_Init(void);
extern PyStatus _PyTypes_InitSlotDefs(void);

/* Various internal finalizers */

extern void _PyFrame_Fini(PyThreadState *tstate);
extern void _PyDict_Fini(void);
extern void _PyTuple_Fini(PyThreadState *tstate);
extern void _PyList_Fini(PyThreadState *tstate);
extern void _PySet_Fini(void);
extern void _PyFloat_Fini(PyThreadState *tstate);
extern void _PySlice_Fini(PyThreadState *tstate);

extern void _PyExc_Fini(void);
extern void _PyImport_Fini(void);
extern void _PyImport_Fini2(void);
extern void _PyType_Fini(void);
extern void _PyUnicode_Fini(PyThreadState *tstate);
extern void _PyLong_Fini(PyThreadState *tstate);
extern void _PyHash_Fini(void);

PyAPI_FUNC(PyStatus) _Py_PreInitializeFromPyArgv(
    const PyPreConfig *src_config,
    const struct _PyArgv *args);
PyAPI_FUNC(PyStatus) _Py_PreInitializeFromConfig(
    const PyConfig *config,
    const struct _PyArgv *args);


PyAPI_FUNC(int) _Py_HandleSystemExit(int *exitcode_p);

PyAPI_FUNC(PyObject*) _PyErr_WriteUnraisableDefaultHook(PyObject *unraisable);

PyAPI_FUNC(void) _PyErr_Print(PyThreadState *tstate);
PyAPI_FUNC(void) _PyErr_Display(PyObject *file, PyObject *exception,
                                PyObject *value, PyObject *tb);

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_LIFECYCLE_H */
