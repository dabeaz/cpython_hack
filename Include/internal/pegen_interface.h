#ifndef Py_PEGENINTERFACE
#define Py_PEGENINTERFACE
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "Python.h"
#include "Python-ast.h"

PyAPI_FUNC(mod_ty) PyPegen_ASTFromString(
    const char *str,
    const char *filename,
    int mode,
    PyCompilerFlags *flags);
PyAPI_FUNC(mod_ty) PyPegen_ASTFromStringObject(
    const char *str,
    PyObject* filename,
    int mode,
    PyCompilerFlags *flags);
PyAPI_FUNC(mod_ty) PyPegen_ASTFromFileObject(
    FILE *fp,
    PyObject *filename_ob,
    int mode,
    const char *ps1,
    const char *ps2,
    PyCompilerFlags *flags,
    int *errcode);
PyAPI_FUNC(mod_ty) PyPegen_ASTFromFilename(
    const char *filename,
    int mode,
    PyCompilerFlags *flags);


#ifdef __cplusplus
}
#endif
#endif /* !Py_PEGENINTERFACE*/
