#ifndef Py_CPYTHON_FILEOBJECT_H
#  error "this header file must not be included directly"
#endif

PyAPI_FUNC(char *) Py_UniversalNewlineFgets(char *, int, FILE*, PyObject *);

/* The std printer acts as a preliminary sys.stderr until the new io
   infrastructure is in place. */
PyAPI_FUNC(PyObject *) PyFile_NewStdPrinter(int);
PyAPI_DATA(PyTypeObject) PyStdPrinter_Type;

PyAPI_FUNC(PyObject *) PyFile_OpenCode(const char *utf8path);
PyAPI_FUNC(PyObject *) PyFile_OpenCodeObject(PyObject *path);
