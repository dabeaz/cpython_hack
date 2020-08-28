#ifndef Py_INTERNAL_FILEUTILS_H
#define Py_INTERNAL_FILEUTILS_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "Py_BUILD_CORE must be defined to include this header"
#endif

PyAPI_DATA(int) _Py_HasFileSystemDefaultEncodeErrors;

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_FILEUTILS_H */
