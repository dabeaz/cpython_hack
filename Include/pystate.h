/* Thread and interpreter state structures and their interfaces */


#ifndef Py_PYSTATE_H
#define Py_PYSTATE_H
#ifdef __cplusplus
extern "C" {
#endif

/* This limitation is for performance and simplicity. If needed it can be
removed (with effort). */
#define MAX_CO_EXTRA_USERS 255

/* Forward declarations for PyFrameObject, PyThreadState
   and PyInterpreterState */
struct _ts;
struct _is;

/* struct _ts is defined in cpython/pystate.h */
typedef struct _ts PyThreadState;
/* struct _is is defined in internal/pycore_interp.h */
typedef struct _is PyInterpreterState;

PyAPI_FUNC(PyInterpreterState *) PyInterpreterState_New(void);
PyAPI_FUNC(void) PyInterpreterState_Clear(PyInterpreterState *);
PyAPI_FUNC(void) PyInterpreterState_Delete(PyInterpreterState *);
PyAPI_FUNC(PyInterpreterState *) PyInterpreterState_Get(void);
PyAPI_FUNC(PyObject *) PyInterpreterState_GetDict(PyInterpreterState *);
PyAPI_FUNC(int64_t) PyInterpreterState_GetID(PyInterpreterState *);

/* State unique per thread */

/* New in 3.3 */
PyAPI_FUNC(int) PyState_AddModule(PyObject*, struct PyModuleDef*);
PyAPI_FUNC(int) PyState_RemoveModule(struct PyModuleDef*);
PyAPI_FUNC(PyObject*) PyState_FindModule(struct PyModuleDef*);

PyAPI_FUNC(PyThreadState *) PyThreadState_New(PyInterpreterState *);
PyAPI_FUNC(void) PyThreadState_Clear(PyThreadState *);
PyAPI_FUNC(void) PyThreadState_Delete(PyThreadState *);

/* Get the current thread state.

   When the current thread state is NULL, this issues a fatal error (so that
   the caller needn't check for NULL).

   The caller must hold the GIL.

   See also PyThreadState_GET() and _PyThreadState_GET(). */
PyAPI_FUNC(PyThreadState *) PyThreadState_Get(void);

PyAPI_FUNC(PyObject *) PyThreadState_GetDict(void);
  
PyAPI_FUNC(int) PyThreadState_SetAsyncExc(unsigned long, PyObject *);

/* New in 3.9 */
PyAPI_FUNC(PyInterpreterState*) PyThreadState_GetInterpreter(PyThreadState *tstate);
PyAPI_FUNC(PyFrameObject*) PyThreadState_GetFrame(PyThreadState *tstate);
PyAPI_FUNC(uint64_t) PyThreadState_GetID(PyThreadState *tstate);

#include "initconfig.h"

PyAPI_FUNC(int) _PyInterpreterState_RequiresIDRef(PyInterpreterState *);
PyAPI_FUNC(void) _PyInterpreterState_RequireIDRef(PyInterpreterState *, int);

PyAPI_FUNC(PyObject *) _PyInterpreterState_GetMainModule(PyInterpreterState *);

/* State unique per thread */

/* Py_tracefunc return -1 when raising an exception, or 0 for success. */
typedef int (*Py_tracefunc)(PyObject *, PyFrameObject *, int, PyObject *);

/* The following values are used for 'what' for tracefunc functions
 *
 * To add a new kind of trace event, also update "trace_init" in
 * Python/sysmodule.c to define the Python level event name
 */
#define PyTrace_CALL 0
#define PyTrace_EXCEPTION 1
#define PyTrace_LINE 2
#define PyTrace_RETURN 3
#define PyTrace_C_CALL 4
#define PyTrace_C_EXCEPTION 5
#define PyTrace_C_RETURN 6
#define PyTrace_OPCODE 7


typedef struct _err_stackitem {
    /* This struct represents an entry on the exception stack, which is a
     * per-coroutine state. (Coroutine in the computer science sense,
     * including the thread and generators).
     * This ensures that the exception state is not impacted by "yields"
     * from an except handler.
     */
    PyObject *exc_type, *exc_value, *exc_traceback;

    struct _err_stackitem *previous_item;

} _PyErr_StackItem;


// The PyThreadState typedef is in Include/pystate.h.
struct _ts {
    /* See Python/ceval.c for comments explaining most fields */

    struct _ts *prev;
    struct _ts *next;
    PyInterpreterState *interp;

    /* Borrowed reference to the current frame (it can be NULL) */
    PyFrameObject *frame;
  
    /* The exception currently being raised */
    PyObject *curexc_type;
    PyObject *curexc_value;
    PyObject *curexc_traceback;

    /* The exception currently being handled, if no coroutines/generators
     * are present. Always last element on the stack referred to be exc_info.
     */
    _PyErr_StackItem exc_state;

    /* Pointer to the top of the stack of the exceptions currently
     * being handled */
    _PyErr_StackItem *exc_info;

    PyObject *dict;  /* Stores per-thread state */

    PyObject *async_exc; /* Asynchronous exception to raise */
   /* unsigned long thread_id; Thread id where this tstate was created */
  
    PyObject *context;
    uint64_t context_ver;

    /* Unique thread state id. */
    uint64_t id;

    /* XXX signal handlers should also be here */

};

// Alias for backward compatibility with Python 3.8
#define _PyInterpreterState_Get PyInterpreterState_Get

PyAPI_FUNC(PyThreadState *) _PyThreadState_Prealloc(PyInterpreterState *);

/* Similar to PyThreadState_Get(), but don't issue a fatal error
 * if it is NULL. */
PyAPI_FUNC(PyThreadState *) _PyThreadState_UncheckedGet(void);

PyAPI_FUNC(PyObject *) _PyThreadState_GetDict(PyThreadState *tstate);


/* Get the single PyInterpreterState used by this process' GILState
   implementation.

   This function doesn't check for error. Return NULL before _PyGILState_Init()
   is called and after _PyGILState_Fini() is called.

   See also _PyInterpreterState_Get() and _PyInterpreterState_GET(). */
PyAPI_FUNC(PyInterpreterState *) _PyGILState_GetInterpreterStateUnsafe(void);

/* The implementation of sys._current_frames()  Returns a dict mapping
   thread id to that thread's current frame.
*/
PyAPI_FUNC(PyObject *) _PyThread_CurrentFrames(void);

/* Routines for advanced debuggers, requested by David Beazley.
   Don't use unless you know what you are doing! */
PyAPI_FUNC(PyInterpreterState *) PyInterpreterState_Main(void);
PyAPI_FUNC(PyInterpreterState *) PyInterpreterState_Head(void);
PyAPI_FUNC(PyInterpreterState *) PyInterpreterState_Next(PyInterpreterState *);
PyAPI_FUNC(PyThreadState *) PyInterpreterState_ThreadHead(PyInterpreterState *);
PyAPI_FUNC(PyThreadState *) PyThreadState_Next(PyThreadState *);
PyAPI_FUNC(void) PyThreadState_DeleteCurrent(void);

/* Frame evaluation API */

typedef PyObject* (*_PyFrameEvalFunction)(PyThreadState *tstate, PyFrameObject *, int);

PyAPI_FUNC(_PyFrameEvalFunction) _PyInterpreterState_GetEvalFrameFunc(
    PyInterpreterState *interp);
PyAPI_FUNC(void) _PyInterpreterState_SetEvalFrameFunc(
    PyInterpreterState *interp,
    _PyFrameEvalFunction eval_frame);

PyAPI_FUNC(const PyConfig*) _PyInterpreterState_GetConfig(PyInterpreterState *interp);

// Get the configuration of the currrent interpreter.
// The caller must hold the GIL.
PyAPI_FUNC(const PyConfig*) _Py_GetConfig(void);


/* cross-interpreter data */

struct _xid;

// _PyCrossInterpreterData is similar to Py_buffer as an effectively
// opaque struct that holds data outside the object machinery.  This
// is necessary to pass safely between interpreters in the same process.
typedef struct _xid {
    // data is the cross-interpreter-safe derivation of a Python object
    // (see _PyObject_GetCrossInterpreterData).  It will be NULL if the
    // new_object func (below) encodes the data.
    void *data;
    // obj is the Python object from which the data was derived.  This
    // is non-NULL only if the data remains bound to the object in some
    // way, such that the object must be "released" (via a decref) when
    // the data is released.  In that case the code that sets the field,
    // likely a registered "crossinterpdatafunc", is responsible for
    // ensuring it owns the reference (i.e. incref).
    PyObject *obj;
    // interp is the ID of the owning interpreter of the original
    // object.  It corresponds to the active interpreter when
    // _PyObject_GetCrossInterpreterData() was called.  This should only
    // be set by the cross-interpreter machinery.
    //
    // We use the ID rather than the PyInterpreterState to avoid issues
    // with deleted interpreters.  Note that IDs are never re-used, so
    // each one will always correspond to a specific interpreter
    // (whether still alive or not).
    int64_t interp;
    // new_object is a function that returns a new object in the current
    // interpreter given the data.  The resulting object (a new
    // reference) will be equivalent to the original object.  This field
    // is required.
    PyObject *(*new_object)(struct _xid *);
    // free is called when the data is released.  If it is NULL then
    // nothing will be done to free the data.  For some types this is
    // okay (e.g. bytes) and for those types this field should be set
    // to NULL.  However, for most the data was allocated just for
    // cross-interpreter use, so it must be freed when
    // _PyCrossInterpreterData_Release is called or the memory will
    // leak.  In that case, at the very least this field should be set
    // to PyMem_RawFree (the default if not explicitly set to NULL).
    // The call will happen with the original interpreter activated.
    void (*free)(void *);
} _PyCrossInterpreterData;

PyAPI_FUNC(int) _PyObject_GetCrossInterpreterData(PyObject *, _PyCrossInterpreterData *);
PyAPI_FUNC(PyObject *) _PyCrossInterpreterData_NewObject(_PyCrossInterpreterData *);
PyAPI_FUNC(void) _PyCrossInterpreterData_Release(_PyCrossInterpreterData *);

PyAPI_FUNC(int) _PyObject_CheckCrossInterpreterData(PyObject *);

/* cross-interpreter data registry */

typedef int (*crossinterpdatafunc)(PyObject *, struct _xid *);

PyAPI_FUNC(int) _PyCrossInterpreterData_RegisterClass(PyTypeObject *, crossinterpdatafunc);
PyAPI_FUNC(crossinterpdatafunc) _PyCrossInterpreterData_Lookup(PyObject *);

#ifdef __cplusplus
}
#endif
#endif /* !Py_PYSTATE_H */
