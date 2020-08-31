#ifndef Py_OSDEFS_H
#define Py_OSDEFS_H
#ifdef __cplusplus
extern "C" {
#endif

/* Operating system dependencies */

/* Filename separator */
#ifndef SEP
#define SEP '/'
#endif

/* Max pathname length */

#ifndef MAXPATHLEN
#if defined(PATH_MAX) && PATH_MAX > 1024
#define MAXPATHLEN PATH_MAX
#else
#define MAXPATHLEN 1024
#endif
#endif

/* Search path entry delimiter */
#ifndef DELIM
#define DELIM ':'
#endif

#ifdef __cplusplus
}
#endif
#endif /* !Py_OSDEFS_H */
