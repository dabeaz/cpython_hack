
/* Generator object interface */

#ifndef Py_LIMITED_API
#ifndef Py_GENOBJECT_H
#define Py_GENOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#include "pystate.h"   /* _PyErr_StackItem */

typedef struct {
    PyObject_HEAD
    /* Note: gi_frame can be NULL if the generator is "finished" */
    PyFrameObject *gi_frame;
    /* True if generator is being executed. */
    char gi_running;
    /* The code object backing the generator */
    PyObject *gi_code;
    /* List of weak reference. */
    PyObject *gi_weakreflist;
    /* Name of the generator. */
    PyObject *gi_name;
    /* Qualified name of the generator. */
    PyObject *gi_qualname;
    _PyErr_StackItem gi_exc_state;
} PyGenObject;

PyAPI_DATA(PyTypeObject) PyGen_Type;

#define PyGen_Check(op) PyObject_TypeCheck(op, &PyGen_Type)
#define PyGen_CheckExact(op) Py_IS_TYPE(op, &PyGen_Type)

PyAPI_FUNC(PyObject *) PyGen_New(PyFrameObject *);
PyAPI_FUNC(PyObject *) PyGen_NewWithQualName(PyFrameObject *,
    PyObject *name, PyObject *qualname);
PyAPI_FUNC(int) _PyGen_SetStopIterationValue(PyObject *);
PyAPI_FUNC(int) _PyGen_FetchStopIterationValue(PyObject **);
PyAPI_FUNC(PyObject *) _PyGen_Send(PyGenObject *, PyObject *);
PyAPI_FUNC(void) _PyGen_Finalize(PyObject *self);

#undef _PyGenObject_HEAD

#ifdef __cplusplus
}
#endif
#endif /* !Py_GENOBJECT_H */
#endif /* Py_LIMITED_API */
