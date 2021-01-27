/* Former class object interface -- now only bound methods are here  */

/* Revealing some structures (not for general use) */

#ifndef Py_LIMITED_API
#ifndef Py_CLASSOBJECT_H
#define Py_CLASSOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif
  
PyAPI_DATA(PyTypeObject) PyMethod_Type;
PyAPI_FUNC(int) PyMethod_Check(PyObject *);
PyAPI_FUNC(PyObject *) PyMethod_New(PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyMethod_Function(PyObject *);
PyAPI_FUNC(PyObject *) PyMethod_Self(PyObject *);
  
PyAPI_DATA(PyTypeObject) PyInstanceMethod_Type;
PyAPI_FUNC(int) PyInstanceMethod_Check(PyObject *);
PyAPI_FUNC(PyObject *) PyInstanceMethod_New(PyObject *);
PyAPI_FUNC(PyObject *) PyInstanceMethod_Function(PyObject *);

#ifdef __cplusplus
}
#endif
#endif /* !Py_CLASSOBJECT_H */
#endif /* Py_LIMITED_API */
