#ifndef Py_TRACEBACK_H
#define Py_TRACEBACK_H
#ifdef __cplusplus
extern "C" {
#endif

/* Traceback interface */

PyAPI_FUNC(int) PyTraceBack_Here(PyFrameObject *);
PyAPI_FUNC(int) PyTraceBack_Print(PyObject *, PyObject *);

/* Reveal traceback type so we can typecheck traceback objects */
PyAPI_DATA(PyTypeObject) PyTraceBack_Type;
#define PyTraceBack_Check(v) Py_IS_TYPE(v, &PyTraceBack_Type)

typedef struct _traceback {
    PyObject_HEAD
    struct _traceback *tb_next;
    PyFrameObject *tb_frame;
    int tb_lasti;
    int tb_lineno;
} PyTracebackObject;

PyAPI_FUNC(int) _Py_DisplaySourceLine(PyObject *, PyObject *, int, int);
PyAPI_FUNC(void) _PyTraceback_Add(const char *, const char *, int);

#ifdef __cplusplus
}
#endif
#endif /* !Py_TRACEBACK_H */
