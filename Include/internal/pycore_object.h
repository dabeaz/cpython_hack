#ifndef Py_INTERNAL_OBJECT_H
#define Py_INTERNAL_OBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif
  
#include "pycore_interp.h"     // PyInterpreterState.gc
#include "pycore_pystate.h"    // _PyThreadState_GET()

PyAPI_FUNC(int) _PyType_CheckConsistency(PyTypeObject *type);
PyAPI_FUNC(int) _PyDict_CheckConsistency(PyObject *mp, int check_content);
  
static inline PyObject **
_PyObject_GET_WEAKREFS_LISTPTR(PyObject *op)
{
    Py_ssize_t offset = Py_TYPE(op)->tp_weaklistoffset;
    return (PyObject **)((char *)op + offset);
}

// Fast inlined version of PyType_HasFeature()
static inline int
_PyType_HasFeature(PyTypeObject *type, unsigned long feature) {
    return ((type->tp_flags & feature) != 0);
}
  
#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_OBJECT_H */
