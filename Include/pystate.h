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

#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03090000
/* New in 3.9 */
/* Get the current interpreter state.

   Issue a fatal error if there no current Python thread state or no current
   interpreter. It cannot return NULL.

   The caller must hold the GIL. */
PyAPI_FUNC(PyInterpreterState *) PyInterpreterState_Get(void);
#endif

#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03080000
/* New in 3.8 */
PyAPI_FUNC(PyObject *) PyInterpreterState_GetDict(PyInterpreterState *);
#endif

#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03070000
/* New in 3.7 */
PyAPI_FUNC(int64_t) PyInterpreterState_GetID(PyInterpreterState *);
#endif
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03030000

/* State unique per thread */

/* New in 3.3 */
PyAPI_FUNC(int) PyState_AddModule(PyObject*, struct PyModuleDef*);
PyAPI_FUNC(int) PyState_RemoveModule(struct PyModuleDef*);
#endif
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

/* Get the current Python thread state.

   Macro using PyThreadState_Get() or _PyThreadState_GET() depending if
   pycore_pystate.h is included or not (this header redefines the macro).

   If PyThreadState_Get() is used, issue a fatal error if the current thread
   state is NULL.

   See also PyThreadState_Get() and _PyThreadState_GET(). */
#define PyThreadState_GET() PyThreadState_Get()

PyAPI_FUNC(PyThreadState *) PyThreadState_Swap(PyThreadState *);
PyAPI_FUNC(PyObject *) PyThreadState_GetDict(void);
PyAPI_FUNC(int) PyThreadState_SetAsyncExc(unsigned long, PyObject *);

#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03090000
/* New in 3.9 */
PyAPI_FUNC(PyInterpreterState*) PyThreadState_GetInterpreter(PyThreadState *tstate);
PyAPI_FUNC(PyFrameObject*) PyThreadState_GetFrame(PyThreadState *tstate);
PyAPI_FUNC(uint64_t) PyThreadState_GetID(PyThreadState *tstate);
#endif

#ifndef Py_LIMITED_API
#  define Py_CPYTHON_PYSTATE_H
#  include  "cpython/pystate.h"
#  undef Py_CPYTHON_PYSTATE_H
#endif

#ifdef __cplusplus
}
#endif
#endif /* !Py_PYSTATE_H */
