#ifndef Py_STRCMP_H
#define Py_STRCMP_H

#ifdef __cplusplus
extern "C" {
#endif

PyAPI_FUNC(int) PyOS_mystrnicmp(const char *, const char *, Py_ssize_t);
PyAPI_FUNC(int) PyOS_mystricmp(const char *, const char *);

#define PyOS_strnicmp PyOS_mystrnicmp
#define PyOS_stricmp PyOS_mystricmp

#ifdef __cplusplus
}
#endif

#endif /* !Py_STRCMP_H */
