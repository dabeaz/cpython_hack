/* Tuple object interface */

#ifndef Py_TUPLEOBJECT_H
#define Py_TUPLEOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

PyAPI_DATA(PyTypeObject) PyTuple_Type;
  
PyAPI_FUNC(int) PyTuple_Check(PyObject *);
PyAPI_FUNC(int) PyTuple_CheckExact(PyObject *);
PyAPI_FUNC(PyObject *) PyTuple_New(Py_ssize_t size);
PyAPI_FUNC(Py_ssize_t) PyTuple_Size(PyObject *);
PyAPI_FUNC(PyObject *) PyTuple_GetItem(PyObject *, Py_ssize_t);
PyAPI_FUNC(int) PyTuple_InitItem(PyObject *, Py_ssize_t, PyObject *);  
PyAPI_FUNC(PyObject *) PyTuple_GetSlice(PyObject *, Py_ssize_t, Py_ssize_t);
PyAPI_FUNC(PyObject *) PyTuple_Pack(Py_ssize_t, ...);
PyAPI_FUNC(PyObject **) PyTuple_Items(PyObject *);
PyAPI_FUNC(PyObject *) PyTuple_FromArray(PyObject *const *, Py_ssize_t);
  
#ifdef __cplusplus
}
#endif
#endif /* !Py_TUPLEOBJECT_H */
