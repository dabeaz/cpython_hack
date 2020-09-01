#ifndef Py_CEVAL_H
#define Py_CEVAL_H
#ifdef __cplusplus
extern "C" {
#endif


/* Interface to random parts in ceval.c */

PyAPI_FUNC(PyObject *) PyEval_GetBuiltins(void);
PyAPI_FUNC(PyObject *) PyEval_GetGlobals(void);
PyAPI_FUNC(PyObject *) PyEval_GetLocals(void);
PyAPI_FUNC(PyFrameObject *) PyEval_GetFrame(void);

PyAPI_FUNC(int) Py_AddPendingCall(int (*func)(void *), void *arg);
PyAPI_FUNC(int) Py_MakePendingCalls(void);
  
PyAPI_FUNC(const char *) PyEval_GetFuncName(PyObject *);
PyAPI_FUNC(const char *) PyEval_GetFuncDesc(PyObject *);

PyAPI_FUNC(PyObject *) PyEval_EvalFrame(PyFrameObject *);
PyAPI_FUNC(PyObject *) PyEval_EvalFrameEx(PyFrameObject *f, int exc);
  
/* Masks and values used by FORMAT_VALUE opcode. */
#define FVC_MASK      0x3
#define FVC_NONE      0x0
#define FVC_STR       0x1
#define FVC_REPR      0x2
#define FVC_ASCII     0x3
#define FVS_MASK      0x4
#define FVS_HAVE_SPEC 0x4

PyAPI_FUNC(void) PyEval_SetProfile(Py_tracefunc, PyObject *);
PyAPI_DATA(int) _PyEval_SetProfile(PyThreadState *tstate, Py_tracefunc func, PyObject *arg);
PyAPI_FUNC(void) PyEval_SetTrace(Py_tracefunc, PyObject *);
PyAPI_FUNC(int) _PyEval_SetTrace(PyThreadState *tstate, Py_tracefunc func, PyObject *arg);
PyAPI_FUNC(int) _PyEval_GetCoroutineOriginTrackingDepth(void);
PyAPI_FUNC(int) _PyEval_SetAsyncGenFirstiter(PyObject *);
PyAPI_FUNC(PyObject *) _PyEval_GetAsyncGenFirstiter(void);
PyAPI_FUNC(int) _PyEval_SetAsyncGenFinalizer(PyObject *);
PyAPI_FUNC(PyObject *) _PyEval_GetAsyncGenFinalizer(void);

/* Helper to look up a builtin object */
PyAPI_FUNC(PyObject *) _PyEval_GetBuiltinId(_Py_Identifier *);
/* Look at the current frame's (if any) code's co_flags, and turn on
   the corresponding compiler flags in cf->cf_flags.  Return 1 if any
   flag was set, else return 0. */
PyAPI_FUNC(int) PyEval_MergeCompilerFlags(PyCompilerFlags *cf);

PyAPI_FUNC(PyObject *) _PyEval_EvalFrameDefault(PyThreadState *tstate, PyFrameObject *f, int exc);

PyAPI_FUNC(void) _PyEval_SetSwitchInterval(unsigned long microseconds);
PyAPI_FUNC(unsigned long) _PyEval_GetSwitchInterval(void);

PyAPI_FUNC(Py_ssize_t) _PyEval_RequestCodeExtraIndex(freefunc);

PyAPI_FUNC(int) _PyEval_SliceIndex(PyObject *, Py_ssize_t *);
PyAPI_FUNC(int) _PyEval_SliceIndexNotNone(PyObject *, Py_ssize_t *);

#ifdef __cplusplus
}
#endif
#endif /* !Py_CEVAL_H */
