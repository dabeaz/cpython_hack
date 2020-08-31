#ifndef Py_INTERNAL_PATHCONFIG_H
#define Py_INTERNAL_PATHCONFIG_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

typedef struct _PyPathConfig {
    /* Full path to the Python program */
    char *program_full_path;
    char *prefix;
    char *exec_prefix;
    /* Set by Py_SetPath(), or computed by _PyConfig_InitPathConfig() */
    char *module_search_path;
    /* Python program name */
    char *program_name;
    /* Set by Py_SetPythonHome() or PYTHONHOME environment variable */
    char *home;
} _PyPathConfig;

#  define _PyPathConfig_INIT			\
      {.module_search_path = NULL}

/* Note: _PyPathConfig_INIT sets other fields to 0/NULL */

PyAPI_DATA(_PyPathConfig) _Py_path_config;

extern void _PyPathConfig_ClearGlobal(void);

extern PyStatus _PyPathConfig_Calculate(
    _PyPathConfig *pathconfig,
    const PyConfig *config);
extern int _PyPathConfig_ComputeSysPath0(
    const PyStringList *argv,
    PyObject **path0);
extern PyStatus _Py_FindEnvConfigValue(
    FILE *env_file,
    const char *key,
    char **value_p);

extern PyStatus _PyConfig_WritePathConfig(const PyConfig *config);
extern void _Py_DumpPathConfig(PyThreadState *tstate);

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_PATHCONFIG_H */
