
/* Float object interface */

/*
PyFloatObject represents a (double precision) floating point number.
*/

#ifndef Py_FLOATOBJECT_H
#define Py_FLOATOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

PyAPI_DATA(PyTypeObject) PyFloat_Type;

PyAPI_FUNC(int) PyFloat_Check(PyObject *op);
PyAPI_FUNC(int) PyFloat_CheckExact(PyObject *op);  
PyAPI_FUNC(double) PyFloat_GetMax(void);
PyAPI_FUNC(double) PyFloat_GetMin(void);

/* Return Python float from string PyObject. */
PyAPI_FUNC(PyObject *) PyFloat_FromString(PyObject*);

/* Return Python float from C double. */
PyAPI_FUNC(PyObject *) PyFloat_FromDouble(double);

/* Extract C double from Python float. */
PyAPI_FUNC(double) PyFloat_AsDouble(PyObject *);
  
/* Format the object based on the format_spec, as defined in PEP 3101
   (Advanced String Formatting). */
PyAPI_FUNC(int) _PyFloat_FormatAdvancedWriter(
    _PyUnicodeWriter *writer,
    PyObject *obj,
    PyObject *format_spec,
    Py_ssize_t start,
    Py_ssize_t end);

#ifdef __cplusplus
}
#endif
#endif /* !Py_FLOATOBJECT_H */
