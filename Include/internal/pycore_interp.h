#ifndef Py_INTERNAL_INTERP_H
#define Py_INTERNAL_INTERP_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif
  
struct _Py_parser_state {
    struct {
        int level;
        int atbol;
    } listnode;
};
  
/* interpreter state */

// The PyInterpreterState typedef is in Include/pystate.h.
struct _is {
    /* Reference to the _PyRuntime global variable. This field exists
       to not have to pass runtime in addition to tstate to a function.
       Get runtime from tstate: tstate->interp->runtime. */
    struct pyruntimestate *runtime;
    int finalizing;
  
    PyObject *modules;
    PyObject *modules_by_index;
    PyObject *sysdict;
    PyObject *builtins;
    PyConfig config;
    PyObject *dict;  /* Stores per-interpreter state */
    PyObject *builtins_copy;
  
    /* Initialized to PyEval_EvalFrameDefault(). */
    _PyFrameEvalFunction eval_frame;

    Py_ssize_t co_extra_user_count;
    freefunc co_extra_freefuncs[MAX_CO_EXTRA_USERS];
  
    /* AtExit module */
    void (*pyexitfunc)(PyObject *);
    PyObject *pyexitmodule;
  
    struct _Py_parser_state parser;
};

/* Used by _PyImport_Cleanup() */
extern void _PyInterpreterState_ClearModules(PyInterpreterState *interp);

extern PyStatus _PyInterpreterState_SetConfig(
    PyInterpreterState *interp,
    const PyConfig *config);

PyAPI_FUNC(struct _is*) _PyInterpreterState_LookUpID(int64_t);
  
#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_INTERP_H */

