
/* Thread and interpreter state structures and their interfaces */

#include "Python.h"
#include "pycore_ceval.h"
#include "pycore_initconfig.h"
#include "pycore_pyerrors.h"
#include "pycore_pylifecycle.h"
#include "pycore_pystate.h"       // PyThreadState_Get()
#include "pycore_sysmodule.h"

/* --------------------------------------------------------------------------
CAUTION

Always use PyMem_Malloc() and PyMem_Free() directly in this file.  A
number of these functions are advertised as safe to call when the GIL isn't
held, and in a debug build Python redirects (e.g.) PyMem_NEW (etc) to Python's
debugging obmalloc functions.  Those aren't thread-safe (they rely on the GIL
to avoid the expense of doing their own locking).
-------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

  /* Thread state for currently running code */
  
  PyThreadState *_PyCurrent = NULL;
  
#define _PyRuntimeGILState_GetThreadState()	\
  (_PyCurrent)
#define _PyRuntimeGILState_SetThreadState(value) \
  _PyCurrent = (value)

/* Forward declarations */
static void _PyThreadState_Delete(PyThreadState *tstate, int check_current);

static PyStatus
_PyRuntimeState_Init_impl(_PyRuntimeState *runtime)
{
    memset(runtime, 0, sizeof(*runtime));
    PyPreConfig_InitPythonConfig(&runtime->preconfig);
    return _PyStatus_OK();
}

PyStatus
_PyRuntimeState_Init(_PyRuntimeState *runtime)
{
    PyStatus status = _PyRuntimeState_Init_impl(runtime);
    return status;
}

void
_PyRuntimeState_Fini(_PyRuntimeState *runtime)
{
}

PyInterpreterState *
PyInterpreterState_New(void)
{

    /* tstate is NULL when Py_InitializeFromConfig() calls
       PyInterpreterState_New() to create the main interpreter. */

    PyInterpreterState *interp = PyMem_Calloc(1, sizeof(PyInterpreterState));
    if (interp == NULL) {
        return NULL;
    }
    
    /* Don't get runtime from tstate since tstate can be NULL */
    _PyRuntimeState *runtime = &_PyRuntime;
    interp->runtime = runtime;

    PyConfig_InitPythonConfig(&interp->config);

    interp->eval_frame = _PyEval_EvalFrameDefault;
    return interp;
}


void
PyInterpreterState_Clear(PyInterpreterState *interp)
{
    _PyRuntimeState *runtime = interp->runtime;
    PyConfig_Clear(&interp->config);
    Py_CLEAR(interp->modules);
    Py_CLEAR(interp->modules_by_index);
    Py_CLEAR(interp->sysdict);
    Py_CLEAR(interp->builtins);
    Py_CLEAR(interp->builtins_copy);
    Py_CLEAR(interp->dict);
    if (_PyRuntimeState_GetFinalizing(runtime) == NULL) {
      /* _PyWarnings_Fini(interp); */
    }
    // XXX Once we have one allocator per interpreter (i.e.
    // per-interpreter GC) we must ensure that all of the interpreter's
    // objects have been cleaned up at the point.
}

void
PyInterpreterState_Delete(PyInterpreterState *interp)
{
    PyMem_Free(interp);
}


PyInterpreterState *
PyInterpreterState_Get(void)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyInterpreterState *interp = tstate->interp;
    if (interp == NULL) {
        Py_FatalError("no current interpreter");
    }
    return interp;
}

PyObject *
_PyInterpreterState_GetMainModule(PyInterpreterState *interp)
{
    if (interp->modules == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "interpreter not initialized");
        return NULL;
    }
    return PyMapping_GetItemString(interp->modules, "__main__");
}

PyObject *
PyInterpreterState_GetDict(PyInterpreterState *interp)
{
    if (interp->dict == NULL) {
        interp->dict = PyDict_New();
        if (interp->dict == NULL) {
            PyErr_Clear();
        }
    }
    /* Returning NULL means no per-interpreter dict is available. */
    return interp->dict;
}

static PyThreadState *
new_threadstate(PyInterpreterState *interp, int init)
{
    PyThreadState *tstate = (PyThreadState *)PyMem_Malloc(sizeof(PyThreadState));
    if (tstate == NULL) {
        return NULL;
    }

    tstate->interp = interp;

    tstate->frame = NULL;
    tstate->async_exc = NULL;

    tstate->dict = NULL;

    tstate->curexc_type = NULL;
    tstate->curexc_value = NULL;
    tstate->curexc_traceback = NULL;

    tstate->exc_state.exc_type = NULL;
    tstate->exc_state.exc_value = NULL;
    tstate->exc_state.exc_traceback = NULL;
    tstate->exc_state.previous_item = NULL;
    tstate->exc_info = &tstate->exc_state;
    tstate->context = NULL;
    tstate->context_ver = 1;

    if (init) {
        _PyThreadState_Init(tstate);
    }

    tstate->id = 0;
    tstate->prev = NULL;
    tstate->next = NULL;
    return tstate;
}

PyThreadState *
PyThreadState_New(PyInterpreterState *interp)
{
    return new_threadstate(interp, 1);
}

PyThreadState *
_PyThreadState_Prealloc(PyInterpreterState *interp)
{
    return new_threadstate(interp, 0);
}

void
_PyThreadState_Init(PyThreadState *tstate)
{
  /*    _PyGILState_NoteThreadState(&tstate->interp->runtime->gilstate, tstate);*/
}

PyObject*
PyState_FindModule(struct PyModuleDef* module)
{
    Py_ssize_t index = module->m_base.m_index;
    PyInterpreterState *state = _PyInterpreterState_GET();
    PyObject *res;
    if (module->m_slots) {
        return NULL;
    }
    if (index == 0)
        return NULL;
    if (state->modules_by_index == NULL)
        return NULL;
    if (index >= PyList_Size(state->modules_by_index))
        return NULL;
    res = PyList_GetItem(state->modules_by_index, index);
    return res==Py_None ? NULL : res;
}

int
_PyState_AddModule(PyThreadState *tstate, PyObject* module, struct PyModuleDef* def)
{
    if (!def) {
        assert(_PyErr_Occurred(tstate));
        return -1;
    }
    if (def->m_slots) {
        _PyErr_SetString(tstate,
                         PyExc_SystemError,
                         "PyState_AddModule called on module with slots");
        return -1;
    }

    PyInterpreterState *interp = tstate->interp;
    if (!interp->modules_by_index) {
        interp->modules_by_index = PyList_New(0);
        if (!interp->modules_by_index) {
            return -1;
        }
    }

    while (PyList_Size(interp->modules_by_index) <= def->m_base.m_index) {
        if (PyList_Append(interp->modules_by_index, Py_None) < 0) {
            return -1;
        }
    }

    Py_INCREF(module);
    return PyList_SetItem(interp->modules_by_index,
                          def->m_base.m_index, module);
}

int
PyState_AddModule(PyObject* module, struct PyModuleDef* def)
{
    if (!def) {
        Py_FatalError("module definition is NULL");
        return -1;
    }

    PyThreadState *tstate = PyThreadState_Get();
    PyInterpreterState *interp = tstate->interp;
    Py_ssize_t index = def->m_base.m_index;
    if (interp->modules_by_index &&
        index < PyList_Size(interp->modules_by_index) &&
        module == PyList_GetItem(interp->modules_by_index, index))
    {
        _Py_FatalErrorFormat(__func__, "module %p already added", module);
        return -1;
    }
    return _PyState_AddModule(tstate, module, def);
}

int
PyState_RemoveModule(struct PyModuleDef* def)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyInterpreterState *interp = tstate->interp;

    if (def->m_slots) {
        _PyErr_SetString(tstate,
                         PyExc_SystemError,
                         "PyState_RemoveModule called on module with slots");
        return -1;
    }

    Py_ssize_t index = def->m_base.m_index;
    if (index == 0) {
        Py_FatalError("invalid module index");
    }
    if (interp->modules_by_index == NULL) {
        Py_FatalError("Interpreters module-list not accessible.");
    }
    if (index > PyList_Size(interp->modules_by_index)) {
        Py_FatalError("Module index out of bounds.");
    }

    Py_INCREF(Py_None);
    return PyList_SetItem(interp->modules_by_index, index, Py_None);
}

/* Used by PyImport_Cleanup() */
void
_PyInterpreterState_ClearModules(PyInterpreterState *interp)
{
    if (!interp->modules_by_index) {
        return;
    }

    Py_ssize_t i;
    for (i = 0; i < PyList_Size(interp->modules_by_index); i++) {
        PyObject *m = PyList_GetItem(interp->modules_by_index, i);
        if (PyModule_Check(m)) {
            /* cleanup the saved copy of module dicts */
            PyModuleDef *md = PyModule_GetDef(m);
            if (md) {
                Py_CLEAR(md->m_base.m_copy);
            }
        }
    }

    /* Setting modules_by_index to NULL could be dangerous, so we
       clear the list instead. */
    if (PyList_SetSlice(interp->modules_by_index,
                        0, PyList_Size(interp->modules_by_index),
                        NULL)) {
        PyErr_WriteUnraisable(interp->modules_by_index);
    }
}

void
PyThreadState_Clear(PyThreadState *tstate)
{
    /* Don't clear tstate->frame: it is a borrowed reference */

    Py_CLEAR(tstate->dict);
    Py_CLEAR(tstate->async_exc);

    Py_CLEAR(tstate->curexc_type);
    Py_CLEAR(tstate->curexc_value);
    Py_CLEAR(tstate->curexc_traceback);

    Py_CLEAR(tstate->exc_state.exc_type);
    Py_CLEAR(tstate->exc_state.exc_value);
    Py_CLEAR(tstate->exc_state.exc_traceback);
    Py_CLEAR(tstate->context);
}


static void
_PyThreadState_Delete(PyThreadState *tstate, int check_current)
{
    if (check_current) {
        if (tstate == _PyRuntimeGILState_GetThreadState()) {
            _Py_FatalErrorFormat(__func__, "tstate %p is still current", tstate);
        }
    }
    PyMem_Free(tstate);
}


void
PyThreadState_Delete(PyThreadState *tstate)
{
    _PyThreadState_Delete(tstate, 1);
}


void
_PyThreadState_DeleteCurrent(PyThreadState *tstate)
{
    _PyRuntimeGILState_SetThreadState(NULL);
    PyMem_Free(tstate);
}

void
PyThreadState_DeleteCurrent(void)
{
    PyThreadState *tstate = _PyRuntimeGILState_GetThreadState();
    _PyThreadState_DeleteCurrent(tstate);
}

PyThreadState *
_PyThreadState_UncheckedGet(void)
{
    return PyThreadState_Get();
}


PyThreadState *
PyThreadState_Get(void)
{
  return _PyCurrent;
}

/* An extension mechanism to store arbitrary additional per-thread state.
   PyThreadState_GetDict() returns a dictionary that can be used to hold such
   state; the caller should pick a unique key and store its state there.  If
   PyThreadState_GetDict() returns NULL, an exception has *not* been raised
   and the caller should assume no per-thread state is available. */

PyObject *
_PyThreadState_GetDict(PyThreadState *tstate)
{
    assert(tstate != NULL);
    if (tstate->dict == NULL) {
        tstate->dict = PyDict_New();
        if (tstate->dict == NULL) {
            _PyErr_Clear(tstate);
        }
    }
    return tstate->dict;
}


PyObject *
PyThreadState_GetDict(void)
{
    PyThreadState *tstate = PyThreadState_Get();
    if (tstate == NULL) {
        return NULL;
    }
    return _PyThreadState_GetDict(tstate);
}

PyInterpreterState *
PyThreadState_GetInterpreter(PyThreadState *tstate)
{
    assert(tstate != NULL);
    return tstate->interp;
}


PyFrameObject*
PyThreadState_GetFrame(PyThreadState *tstate)
{
    assert(tstate != NULL);
    PyFrameObject *frame = tstate->frame;
    Py_XINCREF(frame);
    return frame;
}


uint64_t
PyThreadState_GetID(PyThreadState *tstate)
{
    assert(tstate != NULL);
    return tstate->id;
}

_PyFrameEvalFunction
_PyInterpreterState_GetEvalFrameFunc(PyInterpreterState *interp)
{
    return interp->eval_frame;
}


void
_PyInterpreterState_SetEvalFrameFunc(PyInterpreterState *interp,
                                     _PyFrameEvalFunction eval_frame)
{
    interp->eval_frame = eval_frame;
}


const PyConfig*
_PyInterpreterState_GetConfig(PyInterpreterState *interp)
{
    return &interp->config;
}


PyStatus
_PyInterpreterState_SetConfig(PyInterpreterState *interp,
                              const PyConfig *config)
{
    return _PyConfig_Copy(&interp->config, config);
}


const PyConfig*
_Py_GetConfig(void)
{
    PyThreadState *tstate = PyThreadState_Get();
    return _PyInterpreterState_GetConfig(tstate->interp);
}

#ifdef __cplusplus
}
#endif
