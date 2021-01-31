/* Weak references objects for Python. */

#ifndef Py_WEAKREFOBJECT_H
#define Py_WEAKREFOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif


typedef struct _PyWeakReference PyWeakReference;

/* PyWeakReference is the base struct for the Python ReferenceType, ProxyType,
 * and CallableProxyType.
 */
  
PyAPI_DATA(PyTypeObject) _PyWeakref_RefType;
PyAPI_DATA(PyTypeObject) _PyWeakref_ProxyType;
PyAPI_DATA(PyTypeObject) _PyWeakref_CallableProxyType;

PyAPI_FUNC(int) PyWeakref_CheckRef(PyObject *op);
PyAPI_FUNC(int) PyWeakref_CheckRefExact(PyObject *op);  
PyAPI_FUNC(int) PyWeakref_CheckProxy(PyObject *op);  
PyAPI_FUNC(int) PyWeakref_Check(PyObject *op);

PyAPI_FUNC(PyObject *) PyWeakref_NewRef(PyObject *ob, PyObject *callback);
PyAPI_FUNC(PyObject *) PyWeakref_NewProxy(PyObject *ob, PyObject *callback);
PyAPI_FUNC(PyObject *) PyWeakref_GetObject(PyObject *ref);
  
PyAPI_FUNC(Py_ssize_t) _PyWeakref_GetWeakrefCount(PyObject *head);
PyAPI_FUNC(void) _PyWeakref_ClearRef(PyObject *self);


#ifdef __cplusplus
}
#endif
#endif /* !Py_WEAKREFOBJECT_H */
