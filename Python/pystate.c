
/* Thread and interpreter state structures and their interfaces */

#include "Python.h"
#include "pycore_ceval.h"
#include "pycore_initconfig.h"
#include "pycore_pyerrors.h"
#include "pycore_pylifecycle.h"
#include "pycore_pymem.h"         // _PyMem_SetDefaultAllocator()
#include "pycore_pystate.h"       // _PyThreadState_GET()
#include "pycore_sysmodule.h"

/* --------------------------------------------------------------------------
CAUTION

Always use PyMem_RawMalloc() and PyMem_RawFree() directly in this file.  A
number of these functions are advertised as safe to call when the GIL isn't
held, and in a debug build Python redirects (e.g.) PyMem_NEW (etc) to Python's
debugging obmalloc functions.  Those aren't thread-safe (they rely on the GIL
to avoid the expense of doing their own locking).
-------------------------------------------------------------------------- */

#ifdef HAVE_DLOPEN
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#if !HAVE_DECL_RTLD_LAZY
#define RTLD_LAZY 1
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define _PyRuntimeGILState_GetThreadState(gilstate) \
    ((PyThreadState*) gilstate->tstate_current)
#define _PyRuntimeGILState_SetThreadState(gilstate, value) \
    gilstate->tstate_current = (uintptr_t)(value)

/* Forward declarations */
static void _PyThreadState_Delete(PyThreadState *tstate, int check_current);

static PyStatus
_PyRuntimeState_Init_impl(_PyRuntimeState *runtime)
{
    memset(runtime, 0, sizeof(*runtime));
    _PyEval_InitRuntimeState(&runtime->ceval);
    PyPreConfig_InitPythonConfig(&runtime->preconfig);
    runtime->gilstate.check_enabled = 1;
    runtime->interpreters.next_id = -1;
    return _PyStatus_OK();
}

PyStatus
_PyRuntimeState_Init(_PyRuntimeState *runtime)
{
    /* Force default allocator, since _PyRuntimeState_Fini() must
       use the same allocator than this function. */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    PyStatus status = _PyRuntimeState_Init_impl(runtime);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
    return status;
}

void
_PyRuntimeState_Fini(_PyRuntimeState *runtime)
{
    /* Force the allocator used by _PyRuntimeState_Init(). */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
}

/* Forward declaration */

PyStatus
_PyInterpreterState_Enable(_PyRuntimeState *runtime)
{
    struct pyinterpreters *interpreters = &runtime->interpreters;
    interpreters->next_id = 0;

    /* Py_Finalize() calls _PyRuntimeState_Fini() which clears the mutex.
       Create a new mutex if needed. */

        /* Force default allocator, since _PyRuntimeState_Fini() must
           use the same allocator than this function. */
        PyMemAllocatorEx old_alloc;
        _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
        PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    return _PyStatus_OK();
}

PyInterpreterState *
PyInterpreterState_New(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    /* tstate is NULL when Py_InitializeFromConfig() calls
       PyInterpreterState_New() to create the main interpreter. */

    PyInterpreterState *interp = PyMem_RawCalloc(1, sizeof(PyInterpreterState));
    if (interp == NULL) {
        return NULL;
    }

    interp->id_refcount = -1;

    /* Don't get runtime from tstate since tstate can be NULL */
    _PyRuntimeState *runtime = &_PyRuntime;
    interp->runtime = runtime;

    if (_PyEval_InitState(&interp->ceval) < 0) {
        goto out_of_memory;
    }

    _PyGC_InitState(&interp->gc);
    PyConfig_InitPythonConfig(&interp->config);

    interp->eval_frame = _PyEval_EvalFrameDefault;

    struct pyinterpreters *interpreters = &runtime->interpreters;

    if (interpreters->next_id < 0) {
        /* overflow or Py_Initialize() not called! */
        if (tstate != NULL) {
            _PyErr_SetString(tstate, PyExc_RuntimeError,
                             "failed to get an interpreter ID");
        }
        PyMem_RawFree(interp);
        interp = NULL;
    }
    else {
        interp->id = interpreters->next_id;
        interpreters->next_id += 1;
        interp->next = interpreters->head;
        if (interpreters->main == NULL) {
            interpreters->main = interp;
        }
        interpreters->head = interp;
    }

    if (interp == NULL) {
        return NULL;
    }

    interp->tstate_next_unique_id = 0;
    return interp;

out_of_memory:
    if (tstate != NULL) {
        _PyErr_NoMemory(tstate);
    }

    PyMem_RawFree(interp);
    return NULL;
}


void
PyInterpreterState_Clear(PyInterpreterState *interp)
{
    _PyRuntimeState *runtime = interp->runtime;

    for (PyThreadState *p = interp->tstate_head; p != NULL; p = p->next) {
        PyThreadState_Clear(p);
    }
    PyConfig_Clear(&interp->config);
    Py_CLEAR(interp->codec_search_path);
    Py_CLEAR(interp->codec_search_cache);
    Py_CLEAR(interp->codec_error_registry);
    Py_CLEAR(interp->modules);
    Py_CLEAR(interp->modules_by_index);
    Py_CLEAR(interp->sysdict);
    Py_CLEAR(interp->builtins);
    Py_CLEAR(interp->builtins_copy);
    Py_CLEAR(interp->importlib);
    Py_CLEAR(interp->import_func);
    Py_CLEAR(interp->dict);
    if (_PyRuntimeState_GetFinalizing(runtime) == NULL) {
      /* _PyWarnings_Fini(interp); */
    }
    // XXX Once we have one allocator per interpreter (i.e.
    // per-interpreter GC) we must ensure that all of the interpreter's
    // objects have been cleaned up at the point.
}


static void
zapthreads(PyInterpreterState *interp, int check_current)
{
    PyThreadState *tstate;
    /* No need to lock the mutex here because this should only happen
       when the threads are all really dead (XXX famous last words). */
    while ((tstate = interp->tstate_head) != NULL) {
        _PyThreadState_Delete(tstate, check_current);
    }
}


void
PyInterpreterState_Delete(PyInterpreterState *interp)
{
    _PyRuntimeState *runtime = interp->runtime;
    struct pyinterpreters *interpreters = &runtime->interpreters;
    zapthreads(interp, 0);

    _PyEval_FiniState(&interp->ceval);

    /* Delete current thread. After this, many C API calls become crashy. */
    _PyThreadState_Swap(&runtime->gilstate, NULL);

    PyInterpreterState **p;
    for (p = &interpreters->head; ; p = &(*p)->next) {
        if (*p == NULL) {
            Py_FatalError("NULL interpreter");
        }
        if (*p == interp) {
            break;
        }
    }
    if (interp->tstate_head != NULL) {
        Py_FatalError("remaining threads");
    }
    *p = interp->next;

    if (interpreters->main == interp) {
        interpreters->main = NULL;
        if (interpreters->head != NULL) {
            Py_FatalError("remaining subinterpreters");
        }
    }
    PyMem_RawFree(interp);
}


PyInterpreterState *
PyInterpreterState_Get(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _Py_EnsureTstateNotNULL(tstate);
    PyInterpreterState *interp = tstate->interp;
    if (interp == NULL) {
        Py_FatalError("no current interpreter");
    }
    return interp;
}


int64_t
PyInterpreterState_GetID(PyInterpreterState *interp)
{
    if (interp == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "no interpreter provided");
        return -1;
    }
    return interp->id;
}


static PyInterpreterState *
interp_look_up_id(_PyRuntimeState *runtime, int64_t requested_id)
{
    PyInterpreterState *interp = runtime->interpreters.head;
    while (interp != NULL) {
        int64_t id = PyInterpreterState_GetID(interp);
        if (id < 0) {
            return NULL;
        }
        if (requested_id == id) {
            return interp;
        }
        interp = PyInterpreterState_Next(interp);
    }
    return NULL;
}

PyInterpreterState *
_PyInterpreterState_LookUpID(int64_t requested_id)
{
    PyInterpreterState *interp = NULL;
    if (requested_id >= 0) {
        _PyRuntimeState *runtime = &_PyRuntime;
        interp = interp_look_up_id(runtime, requested_id);
    }
    if (interp == NULL && !PyErr_Occurred()) {
        PyErr_Format(PyExc_RuntimeError,
                     "unrecognized interpreter ID %lld", requested_id);
    }
    return interp;
}


int
_PyInterpreterState_IDInitref(PyInterpreterState *interp)
{
  interp->id_refcount = 0;
  return 0;
}


void
_PyInterpreterState_IDIncref(PyInterpreterState *interp)
{
    interp->id_refcount += 1;
}


void
_PyInterpreterState_IDDecref(PyInterpreterState *interp)
{
    struct _gilstate_runtime_state *gilstate = &_PyRuntime.gilstate;
    assert(interp->id_refcount != 0);
    interp->id_refcount -= 1;
    int64_t refcount = interp->id_refcount;

    if (refcount == 0 && interp->requires_idref) {
        // XXX Using the "head" thread isn't strictly correct.
        PyThreadState *tstate = PyInterpreterState_ThreadHead(interp);
        // XXX Possible GILState issues?
        PyThreadState *save_tstate = _PyThreadState_Swap(gilstate, tstate);
        Py_EndInterpreter(tstate);
        _PyThreadState_Swap(gilstate, save_tstate);
    }
}

int
_PyInterpreterState_RequiresIDRef(PyInterpreterState *interp)
{
    return interp->requires_idref;
}

void
_PyInterpreterState_RequireIDRef(PyInterpreterState *interp, int required)
{
    interp->requires_idref = required ? 1 : 0;
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
    PyThreadState *tstate = (PyThreadState *)PyMem_RawMalloc(sizeof(PyThreadState));
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

    tstate->trash_delete_nesting = 0;
    tstate->trash_delete_later = NULL;
    tstate->on_delete = NULL;
    tstate->on_delete_data = NULL;
    tstate->context = NULL;
    tstate->context_ver = 1;

    if (init) {
        _PyThreadState_Init(tstate);
    }

    tstate->id = ++interp->tstate_next_unique_id;
    tstate->prev = NULL;
    tstate->next = interp->tstate_head;
    if (tstate->next)
        tstate->next->prev = tstate;
    interp->tstate_head = tstate;

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
    if (index >= PyList_GET_SIZE(state->modules_by_index))
        return NULL;
    res = PyList_GET_ITEM(state->modules_by_index, index);
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

    while (PyList_GET_SIZE(interp->modules_by_index) <= def->m_base.m_index) {
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

    PyThreadState *tstate = _PyThreadState_GET();
    PyInterpreterState *interp = tstate->interp;
    Py_ssize_t index = def->m_base.m_index;
    if (interp->modules_by_index &&
        index < PyList_GET_SIZE(interp->modules_by_index) &&
        module == PyList_GET_ITEM(interp->modules_by_index, index))
    {
        _Py_FatalErrorFormat(__func__, "module %p already added", module);
        return -1;
    }
    return _PyState_AddModule(tstate, module, def);
}

int
PyState_RemoveModule(struct PyModuleDef* def)
{
    PyThreadState *tstate = _PyThreadState_GET();
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
    if (index > PyList_GET_SIZE(interp->modules_by_index)) {
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
    for (i = 0; i < PyList_GET_SIZE(interp->modules_by_index); i++) {
        PyObject *m = PyList_GET_ITEM(interp->modules_by_index, i);
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
                        0, PyList_GET_SIZE(interp->modules_by_index),
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

    if (tstate->on_delete != NULL) {
        tstate->on_delete(tstate->on_delete_data);
    }
}


/* Common code for PyThreadState_Delete() and PyThreadState_DeleteCurrent() */
static void
tstate_delete_common(PyThreadState *tstate,
                     struct _gilstate_runtime_state *gilstate)
{
    _Py_EnsureTstateNotNULL(tstate);
    PyInterpreterState *interp = tstate->interp;
    if (interp == NULL) {
        Py_FatalError("NULL interpreter");
    }
    if (tstate->prev) {
        tstate->prev->next = tstate->next;
    }
    else {
        interp->tstate_head = tstate->next;
    }
    if (tstate->next) {
        tstate->next->prev = tstate->prev;
    }
}


static void
_PyThreadState_Delete(PyThreadState *tstate, int check_current)
{
    struct _gilstate_runtime_state *gilstate = &tstate->interp->runtime->gilstate;
    if (check_current) {
        if (tstate == _PyRuntimeGILState_GetThreadState(gilstate)) {
            _Py_FatalErrorFormat(__func__, "tstate %p is still current", tstate);
        }
    }
    tstate_delete_common(tstate, gilstate);
    PyMem_RawFree(tstate);
}


void
PyThreadState_Delete(PyThreadState *tstate)
{
    _PyThreadState_Delete(tstate, 1);
}


void
_PyThreadState_DeleteCurrent(PyThreadState *tstate)
{
    _Py_EnsureTstateNotNULL(tstate);
    struct _gilstate_runtime_state *gilstate = &tstate->interp->runtime->gilstate;
    tstate_delete_common(tstate, gilstate);
    _PyRuntimeGILState_SetThreadState(gilstate, NULL);
    PyMem_RawFree(tstate);
}

void
PyThreadState_DeleteCurrent(void)
{
    struct _gilstate_runtime_state *gilstate = &_PyRuntime.gilstate;
    PyThreadState *tstate = _PyRuntimeGILState_GetThreadState(gilstate);
    _PyThreadState_DeleteCurrent(tstate);
}


/*
 * Delete all thread states except the one passed as argument.
 * Note that, if there is a current thread state, it *must* be the one
 * passed as argument.  Also, this won't touch any other interpreters
 * than the current one, since we don't know which thread state should
 * be kept in those other interpreters.
 */
void
_PyThreadState_DeleteExcept(_PyRuntimeState *runtime, PyThreadState *tstate)
{
    PyInterpreterState *interp = tstate->interp;

    /* Remove all thread states, except tstate, from the linked list of
       thread states.  This will allow calling PyThreadState_Clear()
       without holding the lock. */
    PyThreadState *list = interp->tstate_head;
    if (list == tstate) {
        list = tstate->next;
    }
    if (tstate->prev) {
        tstate->prev->next = tstate->next;
    }
    if (tstate->next) {
        tstate->next->prev = tstate->prev;
    }
    tstate->prev = tstate->next = NULL;
    interp->tstate_head = tstate;

    /* Clear and deallocate all stale thread states.  Even if this
       executes Python code, we should be safe since it executes
       in the current thread, not one of the stale threads. */
    PyThreadState *p, *next;
    for (p = list; p; p = next) {
        next = p->next;
        PyThreadState_Clear(p);
        PyMem_RawFree(p);
    }
}



PyThreadState *
_PyThreadState_UncheckedGet(void)
{
    return _PyThreadState_GET();
}


PyThreadState *
PyThreadState_Get(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _Py_EnsureTstateNotNULL(tstate);
    return tstate;
}


PyThreadState *
_PyThreadState_Swap(struct _gilstate_runtime_state *gilstate, PyThreadState *newts)
{
    PyThreadState *oldts = _PyRuntimeGILState_GetThreadState(gilstate);

    _PyRuntimeGILState_SetThreadState(gilstate, newts);
    /* It should not be possible for more than one thread state
       to be used for a thread.  Check this the best we can in debug
       builds.
    */
    return oldts;
}

PyThreadState *
PyThreadState_Swap(PyThreadState *newts)
{
    return _PyThreadState_Swap(&_PyRuntime.gilstate, newts);
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
    PyThreadState *tstate = _PyThreadState_GET();
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

/* Routines for advanced debuggers, requested by David Beazley.
   Don't use unless you know what you are doing! */

PyInterpreterState *
PyInterpreterState_Head(void)
{
    return _PyRuntime.interpreters.head;
}

PyInterpreterState *
PyInterpreterState_Main(void)
{
    return _PyRuntime.interpreters.main;
}

PyInterpreterState *
PyInterpreterState_Next(PyInterpreterState *interp) {
    return interp->next;
}

PyThreadState *
PyInterpreterState_ThreadHead(PyInterpreterState *interp) {
    return interp->tstate_head;
}

PyThreadState *
PyThreadState_Next(PyThreadState *tstate) {
    return tstate->next;
}

/* The implementation of sys._current_frames().  This is intended to be
   called with the GIL held, as it will be when called via
   sys._current_frames().  It's possible it would work fine even without
   the GIL held, but haven't thought enough about that.
*/
PyObject *
_PyThread_CurrentFrames(void)
{
    PyThreadState *tstate = _PyThreadState_GET();

    PyObject *result = PyDict_New();
    if (result == NULL) {
        return NULL;
    }

    /* for i in all interpreters:
     *     for t in all of i's thread states:
     *          if t's frame isn't NULL, map t's id to its frame
     * Because these lists can mutate even when the GIL is held, we
     * need to grab head_mutex for the duration.
     */
    _PyRuntimeState *runtime = tstate->interp->runtime;
    PyInterpreterState *i;
    for (i = runtime->interpreters.head; i != NULL; i = i->next) {
        PyThreadState *t;
        for (t = i->tstate_head; t != NULL; t = t->next) {
            PyFrameObject *frame = t->frame;
            if (frame == NULL) {
                continue;
            }
            PyObject *id = PyLong_FromUnsignedLong(0);
            if (id == NULL) {
                goto fail;
            }
            int stat = PyDict_SetItem(result, id, (PyObject *)frame);
            Py_DECREF(id);
            if (stat < 0) {
                goto fail;
            }
        }
    }
    goto done;

fail:
    Py_CLEAR(result);

done:
    return result;
}

/* Python "auto thread state" API. */

/* Keep this as a static, as it is not reliable!  It can only
   ever be compared to the state for the *current* thread.
   * If not equal, then it doesn't matter that the actual
     value may change immediately after comparison, as it can't
     possibly change to the current thread's state.
   * If equal, then the current thread holds the lock, so the value can't
     change until we yield the lock.
*/

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
    PyThreadState *tstate = _PyThreadState_GET();
    return _PyInterpreterState_GetConfig(tstate->interp);
}

#ifdef __cplusplus
}
#endif
