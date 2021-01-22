#ifndef Py_ITEROBJECT_H
#define Py_ITEROBJECT_H
/* Iterators (the basic kind, over a sequence) */
#ifdef __cplusplus
extern "C" {
#endif
  
PyAPI_FUNC(PyObject *) PySeqIter_New(PyObject *);
PyAPI_FUNC(PyObject *) PyCallIter_New(PyObject *, PyObject *);

#ifdef __cplusplus
}
#endif
#endif /* !Py_ITEROBJECT_H */

