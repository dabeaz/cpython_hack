/* Cell object interface */
#ifndef Py_LIMITED_API
#ifndef Py_CELLOBJECT_H
#define Py_CELLOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

PyAPI_FUNC(int) PyCell_Check(PyObject *);
PyAPI_FUNC(PyObject *) PyCell_New(PyObject *);
PyAPI_FUNC(PyObject *) PyCell_Get(PyObject *);
PyAPI_FUNC(int) PyCell_Set(PyObject *, PyObject *);
  
#ifdef __cplusplus
}
#endif
#endif /* !Py_CELLOBJECT_H */
#endif /* Py_LIMITED_API */
