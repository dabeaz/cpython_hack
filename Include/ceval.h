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

#ifndef Py_LIMITED_API
#  define Py_CPYTHON_CEVAL_H
#  include  "cpython/ceval.h"
#  undef Py_CPYTHON_CEVAL_H
#endif

#ifdef __cplusplus
}
#endif
#endif /* !Py_CEVAL_H */
