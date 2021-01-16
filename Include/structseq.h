
/* Named tuple object interface */

#ifndef Py_STRUCTSEQ_H
#define Py_STRUCTSEQ_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct PyStructSequence_Field {
    const char *name;
    const char *doc;
} PyStructSequence_Field;

typedef struct PyStructSequence_Desc {
    const char *name;
    const char *doc;
    struct PyStructSequence_Field *fields;
    int n_in_sequence;
} PyStructSequence_Desc;

extern const char * const PyStructSequence_UnnamedField;

PyAPI_FUNC(int) PyStructSequence_InitType(PyTypeObject *type,
                                           PyStructSequence_Desc *desc);
PyAPI_FUNC(PyTypeObject*) PyStructSequence_NewType(PyStructSequence_Desc *desc);
PyAPI_FUNC(PyObject *) PyStructSequence_New(PyTypeObject* type);
PyAPI_FUNC(void) PyStructSequence_InitItem(PyObject *, Py_ssize_t, PyObject *);
PyAPI_FUNC(PyObject*) PyStructSequence_GetItem(PyObject*, Py_ssize_t);
PyAPI_FUNC(PyObject **) PyStructSequence_Items(PyObject *);
  
#ifdef __cplusplus
}
#endif
#endif /* !Py_STRUCTSEQ_H */
