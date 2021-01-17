/* Set object interface */

#ifndef Py_SETOBJECT_H
#define Py_SETOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif
  
PyAPI_DATA(PyTypeObject) PySet_Type;
PyAPI_DATA(PyTypeObject) PyFrozenSet_Type;
PyAPI_DATA(PyTypeObject) PySetIter_Type;

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

#define PyFrozenSet_CheckExact(ob) Py_IS_TYPE(ob, &PyFrozenSet_Type)
#define PyAnySet_CheckExact(ob) \
    (Py_IS_TYPE(ob, &PySet_Type) || Py_IS_TYPE(ob, &PyFrozenSet_Type))
#define PyAnySet_Check(ob) \
    (Py_IS_TYPE(ob, &PySet_Type) || Py_IS_TYPE(ob, &PyFrozenSet_Type) || \
      PyType_IsSubtype(Py_TYPE(ob), &PySet_Type) || \
      PyType_IsSubtype(Py_TYPE(ob), &PyFrozenSet_Type))
#define PySet_Check(ob) \
    (Py_IS_TYPE(ob, &PySet_Type) || \
    PyType_IsSubtype(Py_TYPE(ob), &PySet_Type))
#define   PyFrozenSet_Check(ob) \
    (Py_IS_TYPE(ob, &PyFrozenSet_Type) || \
      PyType_IsSubtype(Py_TYPE(ob), &PyFrozenSet_Type))

#ifdef __cplusplus
}
#endif
#endif /* !Py_SETOBJECT_H */
