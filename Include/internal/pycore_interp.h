#ifndef Py_INTERNAL_INTERP_H
#define Py_INTERNAL_INTERP_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_gc.h"        /* struct _gc_runtime_state */

struct _Py_parser_state {
    struct {
        int level;
        int atbol;
    } listnode;
};
  
/* fs_codec.encoding is initialized to NULL.
   Later, it is set to a non-NULL string by _PyUnicode_InitEncodings(). */
struct _Py_unicode_fs_codec {
    char *encoding;   // Filesystem encoding (encoded to UTF-8)
    int utf8;         // encoding=="utf-8"?
    char *errors;     // Filesystem errors (encoded to UTF-8)
    _Py_error_handler error_handler;
};

struct _Py_unicode_state {
    struct _Py_unicode_fs_codec fs_codec;
};

/* interpreter state */

// The PyInterpreterState typedef is in Include/pystate.h.
struct _is {
    /* Reference to the _PyRuntime global variable. This field exists
       to not have to pass runtime in addition to tstate to a function.
       Get runtime from tstate: tstate->interp->runtime. */
    struct pyruntimestate *runtime;
    int finalizing;
  
    struct _gc_runtime_state gc;

    PyObject *modules;
    PyObject *modules_by_index;
    PyObject *sysdict;
    PyObject *builtins;
    PyObject *importlib;
  
    PyObject *codec_search_path;
    PyObject *codec_search_cache;
    PyObject *codec_error_registry;
    int codecs_initialized;

    struct _Py_unicode_state unicode;
  
    PyConfig config;
    PyObject *dict;  /* Stores per-interpreter state */

    PyObject *builtins_copy;
    PyObject *import_func;
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

PyAPI_FUNC(int) _PyInterpreterState_IDInitref(struct _is *);
PyAPI_FUNC(void) _PyInterpreterState_IDIncref(struct _is *);
PyAPI_FUNC(void) _PyInterpreterState_IDDecref(struct _is *);

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_INTERP_H */

