/*
 * Declarations shared between the different parts of the io module
 */

#include "exports.h"

/* ABCs */
extern PyTypeObject PyIOBase_Type;

/* Concrete classes */
extern PyTypeObject PyFileIO_Type;

/* These functions are used as METH_NOARGS methods, are normally called
 * with args=NULL, and return a new reference.
 * BUT when args=Py_True is passed, they return a borrowed reference.
 */
extern PyObject* _PyIOBase_check_readable(PyObject *self, PyObject *args);
extern PyObject* _PyIOBase_check_writable(PyObject *self, PyObject *args);
extern PyObject* _PyIOBase_check_seekable(PyObject *self, PyObject *args);
extern PyObject* _PyIOBase_check_closed(PyObject *self, PyObject *args);

/* Helper for finalization.
   This function will revive an object ready to be deallocated and try to
   close() it. It returns 0 if the object can be destroyed, or -1 if it
   is alive again. */
extern int _PyIOBase_finalize(PyObject *self);

/* Returns true if the given FileIO object is closed.
   Doesn't check the argument type, so be careful! */
extern int _PyFileIO_closed(PyObject *self);

/* Return 1 if an OSError with errno == EINTR is set (and then
   clears the error indicator), 0 otherwise.
   Should only be called when PyErr_Occurred() is true.
*/
extern int _PyIO_trap_eintr(void);

#define DEFAULT_BUFFER_SIZE (8 * 1024)  /* bytes */

/*
 * Offset type for positioning.
 */

/* Other platforms use off_t */
typedef off_t Py_off_t;
#if (SIZEOF_OFF_T == SIZEOF_SIZE_T)
# define PyLong_AsOff_t     PyLong_AsSsize_t
# define PyLong_FromOff_t   PyLong_FromSsize_t
# define PY_OFF_T_MAX       PY_SSIZE_T_MAX
# define PY_OFF_T_MIN       PY_SSIZE_T_MIN
# define PY_OFF_T_COMPAT    Py_ssize_t
# define PY_PRIdOFF         "zd"
#elif (SIZEOF_OFF_T == SIZEOF_LONG_LONG)
# define PyLong_AsOff_t     PyLong_AsLongLong
# define PyLong_FromOff_t   PyLong_FromLongLong
# define PY_OFF_T_MAX       LLONG_MAX
# define PY_OFF_T_MIN       LLONG_MIN
# define PY_OFF_T_COMPAT    long long
# define PY_PRIdOFF         "lld"
#elif (SIZEOF_OFF_T == SIZEOF_LONG)
# define PyLong_AsOff_t     PyLong_AsLong
# define PyLong_FromOff_t   PyLong_FromLong
# define PY_OFF_T_MAX       LONG_MAX
# define PY_OFF_T_MIN       LONG_MIN
# define PY_OFF_T_COMPAT    long
# define PY_PRIdOFF         "ld"
#else
# error off_t does not match either size_t, long, or long long!
#endif

extern Py_off_t PyNumber_AsOff_t(PyObject *item, PyObject *err);

/* Implementation details */

/* IO module structure */

extern PyModuleDef _PyIO_Module;

typedef struct {
    int initialized;
    PyObject *locale_module;

    PyObject *unsupported_operation;
} _PyIO_State;

#define IO_MOD_STATE(mod) ((_PyIO_State *)PyModule_GetState(mod))
#define IO_STATE() _PyIO_get_module_state()

extern _PyIO_State *_PyIO_get_module_state(void);
extern PyObject *_PyIO_get_locale_module(_PyIO_State *);

extern PyObject *_PyIO_str_close;
extern PyObject *_PyIO_str_closed;
extern PyObject *_PyIO_str_decode;
extern PyObject *_PyIO_str_encode;
extern PyObject *_PyIO_str_fileno;
extern PyObject *_PyIO_str_flush;
extern PyObject *_PyIO_str_getstate;
extern PyObject *_PyIO_str_isatty;
extern PyObject *_PyIO_str_newlines;
extern PyObject *_PyIO_str_nl;
extern PyObject *_PyIO_str_peek;
extern PyObject *_PyIO_str_read;
extern PyObject *_PyIO_str_read1;
extern PyObject *_PyIO_str_readable;
extern PyObject *_PyIO_str_readall;
extern PyObject *_PyIO_str_readinto;
extern PyObject *_PyIO_str_readline;
extern PyObject *_PyIO_str_reset;
extern PyObject *_PyIO_str_seek;
extern PyObject *_PyIO_str_seekable;
extern PyObject *_PyIO_str_setstate;
extern PyObject *_PyIO_str_tell;
extern PyObject *_PyIO_str_truncate;
extern PyObject *_PyIO_str_writable;
extern PyObject *_PyIO_str_write;

extern PyObject *_PyIO_empty_str;
extern PyObject *_PyIO_empty_bytes;
