
/* Interfaces to parse and execute pieces of python code */

#ifndef Py_PYTHONRUN_H
#define Py_PYTHONRUN_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_LIMITED_API
PyAPI_FUNC(int) PyRun_SimpleStringFlags(const char *, PyCompilerFlags *);
PyAPI_FUNC(int) PyRun_AnyFileExFlags(
    FILE *fp,
    const char *filename,       /* decoded from the filesystem encoding */
    int closeit,
    PyCompilerFlags *flags);
PyAPI_FUNC(int) PyRun_SimpleFileExFlags(
    FILE *fp,
    const char *filename,       /* decoded from the filesystem encoding */
    int closeit,
    PyCompilerFlags *flags);
PyAPI_FUNC(int) PyRun_InteractiveOneFlags(
    FILE *fp,
    const char *filename,       /* decoded from the filesystem encoding */
    PyCompilerFlags *flags);
PyAPI_FUNC(int) PyRun_InteractiveOneObject(
    FILE *fp,
    PyObject *filename,
    PyCompilerFlags *flags);
PyAPI_FUNC(int) PyRun_InteractiveLoopFlags(
    FILE *fp,
    const char *filename,       /* decoded from the filesystem encoding */
    PyCompilerFlags *flags);

PyAPI_FUNC(struct _mod *) PyParser_ASTFromString(
    const char *s,
    const char *filename,       /* decoded from the filesystem encoding */
    int start,
    PyCompilerFlags *flags);

PyAPI_FUNC(struct _mod *) PyParser_ASTFromStringObject(
    const char *s,
    PyObject *filename,
    int start,
    PyCompilerFlags *flags);
PyAPI_FUNC(struct _mod *) PyParser_ASTFromFile(
    FILE *fp,
    const char *filename,       /* decoded from the filesystem encoding */
    const char* enc,
    int start,
    const char *ps1,
    const char *ps2,
    PyCompilerFlags *flags,
    int *errcode);
PyAPI_FUNC(struct _mod *) PyParser_ASTFromFileObject(
    FILE *fp,
    PyObject *filename,
    const char* enc,
    int start,
    const char *ps1,
    const char *ps2,
    PyCompilerFlags *flags,
    int *errcode);
#endif

#ifndef PyParser_SimpleParseString
#define PyParser_SimpleParseString(S, B) \
    PyParser_SimpleParseStringFlags(S, B, 0)
#define PyParser_SimpleParseFile(FP, S, B) \
    PyParser_SimpleParseFileFlags(FP, S, B, 0)
#endif
PyAPI_FUNC(struct _node *) PyParser_SimpleParseStringFlags(const char *, int,
                                                           int);
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03030000
PyAPI_FUNC(struct _node *) PyParser_SimpleParseStringFlagsFilename(const char *,
                                                                   const char *,
                                                                   int, int);
#endif
PyAPI_FUNC(struct _node *) PyParser_SimpleParseFileFlags(FILE *, const char *,
                                                         int, int);

#ifndef Py_LIMITED_API
PyAPI_FUNC(PyObject *) PyRun_StringFlags(const char *, int, PyObject *,
                                         PyObject *, PyCompilerFlags *);

PyAPI_FUNC(PyObject *) PyRun_FileExFlags(
    FILE *fp,
    const char *filename,       /* decoded from the filesystem encoding */
    int start,
    PyObject *globals,
    PyObject *locals,
    int closeit,
    PyCompilerFlags *flags);
#endif

#ifdef Py_LIMITED_API
PyAPI_FUNC(PyObject *) Py_CompileString(const char *, const char *, int);
#else
#define Py_CompileString(str, p, s) Py_CompileStringExFlags(str, p, s, NULL, -1)
#define Py_CompileStringFlags(str, p, s, f) Py_CompileStringExFlags(str, p, s, f, -1)
PyAPI_FUNC(PyObject *) Py_CompileStringExFlags(
    const char *str,
    const char *filename,       /* decoded from the filesystem encoding */
    int start,
    PyCompilerFlags *flags,
    int optimize);
PyAPI_FUNC(PyObject *) Py_CompileStringObject(
    const char *str,
    PyObject *filename, int start,
    PyCompilerFlags *flags,
    int optimize);
#endif
PyAPI_FUNC(struct symtable *) Py_SymtableString(
    const char *str,
    const char *filename,       /* decoded from the filesystem encoding */
    int start);
#ifndef Py_LIMITED_API
PyAPI_FUNC(const char *) _Py_SourceAsString(
    PyObject *cmd,
    const char *funcname,
    const char *what,
    PyCompilerFlags *cf,
    PyObject **cmd_copy);

PyAPI_FUNC(struct symtable *) Py_SymtableStringObject(
    const char *str,
    PyObject *filename,
    int start);

PyAPI_FUNC(struct symtable *) _Py_SymtableStringObjectFlags(
    const char *str,
    PyObject *filename,
    int start,
    PyCompilerFlags *flags);
#endif

PyAPI_FUNC(void) PyErr_Print(void);
PyAPI_FUNC(void) PyErr_PrintEx(int);
PyAPI_FUNC(void) PyErr_Display(PyObject *, PyObject *, PyObject *);

/* Stuff with no proper home (yet) */
#ifndef Py_LIMITED_API
PyAPI_FUNC(char *) PyOS_Readline(FILE *, FILE *, const char *);
#endif
PyAPI_DATA(int) (*PyOS_InputHook)(void);
PyAPI_DATA(char) *(*PyOS_ReadlineFunctionPointer)(FILE *, FILE *, const char *);
#ifndef Py_LIMITED_API
PyAPI_DATA(PyThreadState*) _PyOS_ReadlineTState;
#endif

/* Stack size, in "pointers" (so we get extra safety margins
   on 64-bit platforms).  On a 32-bit platform, this translates
   to an 8k margin. */
#define PYOS_STACK_MARGIN 2048

#ifdef __cplusplus
}
#endif
#endif /* !Py_PYTHONRUN_H */
