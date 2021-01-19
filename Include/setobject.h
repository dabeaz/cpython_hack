/* Set object interface */

#ifndef Py_SETOBJECT_H
#define Py_SETOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif
  
PyAPI_DATA(PyTypeObject) PySet_Type;
PyAPI_DATA(PyTypeObject) PyFrozenSet_Type;
  
PyAPI_FUNC(PyObject *) PySet_New(PyObject *);
PyAPI_FUNC(PyObject *) PyFrozenSet_New(PyObject *);

PyAPI_FUNC(int) PySet_Add(PyObject *set, PyObject *key);
PyAPI_FUNC(int) PySet_Clear(PyObject *set);
PyAPI_FUNC(int) PySet_Contains(PyObject *anyset, PyObject *key);
PyAPI_FUNC(int) PySet_Discard(PyObject *set, PyObject *key);
PyAPI_FUNC(PyObject *) PySet_Pop(PyObject *set);
PyAPI_FUNC(Py_ssize_t) PySet_Size(PyObject *anyset);
PyAPI_FUNC(int) PySet_Update(PyObject *set, PyObject *iterable);

/* Internal function. Used to iterate over set key/hash values */
PyAPI_FUNC(int) PySet_NextEntry(PyObject *set, Py_ssize_t *pos, PyObject **key, Py_hash_t *hash);

PyAPI_FUNC(int) PySet_Check(PyObject *ob);
PyAPI_FUNC(int) PyFrozenSet_Check(PyObject *ob);
PyAPI_FUNC(int) PyAnySet_CheckExact(PyObject *ob);
PyAPI_FUNC(int) PyAnySet_Check(PyObject *ob);
PyAPI_FUNC(int) PyFrozenSet_CheckExact(PyObject *ob);  
  
#ifdef __cplusplus
}
#endif
#endif /* !Py_SETOBJECT_H */
