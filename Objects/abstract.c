
/* Abstract Object Interface (many thanks to Jim Fulton) */

#include "Python.h"
#include "pycore_abstract.h"      // _PyIndex_Check()
#include "pycore_ceval.h"         // _Py_EnterRecursiveCall()
#include "pycore_pyerrors.h"
#include "pycore_pystate.h"       // PyThreadState_Get()
#include <ctype.h>
#include <stddef.h>               // offsetof()
#include "longintrepr.h"



/* Shorthands to return certain errors */

static PyObject *
type_error(const char *msg, PyObject *obj)
{
    PyErr_Format(PyExc_TypeError, msg, Py_TYPE(obj)->tp_name);
    return NULL;
}

static PyObject *
null_error(void)
{
    if (!PyErr_Occurred())
        PyErr_SetString(PyExc_SystemError,
                        "null argument to internal routine");
    return NULL;
}

/* Operations on any object */

PyObject *
PyObject_Type(PyObject *o)
{
    PyObject *v;

    if (o == NULL) {
        return null_error();
    }

    v = (PyObject *)Py_TYPE(o);
    Py_INCREF(v);
    return v;
}

Py_ssize_t
PyObject_Size(PyObject *o)
{
    PySequenceMethods *m;

    if (o == NULL) {
        null_error();
        return -1;
    }

    m = Py_TYPE(o)->tp_as_sequence;
    if (m && m->sq_length) {
        Py_ssize_t len = m->sq_length(o);
        assert(len >= 0 || PyErr_Occurred());
        return len;
    }

    return PyMapping_Size(o);
}

#undef PyObject_Length
Py_ssize_t
PyObject_Length(PyObject *o)
{
    return PyObject_Size(o);
}
#define PyObject_Length PyObject_Size

int
_PyObject_HasLen(PyObject *o) {
    return (Py_TYPE(o)->tp_as_sequence && Py_TYPE(o)->tp_as_sequence->sq_length) ||
        (Py_TYPE(o)->tp_as_mapping && Py_TYPE(o)->tp_as_mapping->mp_length);
}

/* The length hint function returns a non-negative value from o.__len__()
   or o.__length_hint__(). If those methods aren't found the defaultvalue is
   returned.  If one of the calls fails with an exception other than TypeError
   this function returns -1.
*/

Py_ssize_t
PyObject_LengthHint(PyObject *o, Py_ssize_t defaultvalue)
{
    PyObject *hint, *result;
    Py_ssize_t res;
    _Py_IDENTIFIER(__length_hint__);
    if (_PyObject_HasLen(o)) {
        res = PyObject_Length(o);
        if (res < 0) {
            assert(PyErr_Occurred());
            if (!PyErr_ExceptionMatches(PyExc_TypeError)) {
                return -1;
            }
            PyErr_Clear();
        }
        else {
            return res;
        }
    }
    hint = _PyObject_LookupSpecial(o, &PyId___length_hint__);
    if (hint == NULL) {
        if (PyErr_Occurred()) {
            return -1;
        }
        return defaultvalue;
    }
    result = _PyObject_CallNoArg(hint);
    Py_DECREF(hint);
    if (result == NULL) {
        if (PyErr_ExceptionMatches(PyExc_TypeError)) {
            PyErr_Clear();
            return defaultvalue;
        }
        return -1;
    }
    else if (result == Py_NotImplemented) {
        Py_DECREF(result);
        return defaultvalue;
    }
    if (!PyLong_Check(result)) {
        PyErr_Format(PyExc_TypeError, "__length_hint__ must be an integer, not %.100s",
            Py_TYPE(result)->tp_name);
        Py_DECREF(result);
        return -1;
    }
    res = PyLong_AsSsize_t(result);
    Py_DECREF(result);
    if (res < 0 && PyErr_Occurred()) {
        return -1;
    }
    if (res < 0) {
        PyErr_Format(PyExc_ValueError, "__length_hint__() should return >= 0");
        return -1;
    }
    return res;
}

PyObject *
PyObject_GetItem(PyObject *o, PyObject *key)
{
    PyMappingMethods *m;
    PySequenceMethods *ms;

    if (o == NULL || key == NULL) {
        return null_error();
    }

    m = Py_TYPE(o)->tp_as_mapping;
    if (m && m->mp_subscript) {
        PyObject *item = m->mp_subscript(o, key);
        assert((item != NULL) ^ (PyErr_Occurred() != NULL));
        return item;
    }

    ms = Py_TYPE(o)->tp_as_sequence;
    if (ms && ms->sq_item) {
        if (_PyIndex_Check(key)) {
            Py_ssize_t key_value;
            key_value = PyNumber_AsSsize_t(key, PyExc_IndexError);
            if (key_value == -1 && PyErr_Occurred())
                return NULL;
            return PySequence_GetItem(o, key_value);
        }
        else {
            return type_error("sequence index must "
                              "be integer, not '%.200s'", key);
        }
    }

    return type_error("'%.200s' object is not subscriptable", o);
}

int
PyObject_SetItem(PyObject *o, PyObject *key, PyObject *value)
{
    PyMappingMethods *m;

    if (o == NULL || key == NULL || value == NULL) {
        null_error();
        return -1;
    }
    m = Py_TYPE(o)->tp_as_mapping;
    if (m && m->mp_ass_subscript)
        return m->mp_ass_subscript(o, key, value);

    if (Py_TYPE(o)->tp_as_sequence) {
        if (_PyIndex_Check(key)) {
            Py_ssize_t key_value;
            key_value = PyNumber_AsSsize_t(key, PyExc_IndexError);
            if (key_value == -1 && PyErr_Occurred())
                return -1;
            return PySequence_SetItem(o, key_value, value);
        }
        else if (Py_TYPE(o)->tp_as_sequence->sq_ass_item) {
            type_error("sequence index must be "
                       "integer, not '%.200s'", key);
            return -1;
        }
    }

    type_error("'%.200s' object does not support item assignment", o);
    return -1;
}

int
PyObject_DelItem(PyObject *o, PyObject *key)
{
    PyMappingMethods *m;

    if (o == NULL || key == NULL) {
        null_error();
        return -1;
    }
    m = Py_TYPE(o)->tp_as_mapping;
    if (m && m->mp_ass_subscript)
        return m->mp_ass_subscript(o, key, (PyObject*)NULL);

    if (Py_TYPE(o)->tp_as_sequence) {
        if (_PyIndex_Check(key)) {
            Py_ssize_t key_value;
            key_value = PyNumber_AsSsize_t(key, PyExc_IndexError);
            if (key_value == -1 && PyErr_Occurred())
                return -1;
            return PySequence_DelItem(o, key_value);
        }
        else if (Py_TYPE(o)->tp_as_sequence->sq_ass_item) {
            type_error("sequence index must be "
                       "integer, not '%.200s'", key);
            return -1;
        }
    }

    type_error("'%.200s' object does not support item deletion", o);
    return -1;
}

int
PyObject_DelItemString(PyObject *o, const char *key)
{
    PyObject *okey;
    int ret;

    if (o == NULL || key == NULL) {
        null_error();
        return -1;
    }
    okey = PyUnicode_FromString(key);
    if (okey == NULL)
        return -1;
    ret = PyObject_DelItem(o, okey);
    Py_DECREF(okey);
    return ret;
}

PyObject *
PyObject_Format(PyObject *obj, PyObject *format_spec)
{
    PyObject *meth;
    PyObject *empty = NULL;
    PyObject *result = NULL;
    _Py_IDENTIFIER(__format__);

    if (format_spec != NULL && !PyUnicode_Check(format_spec)) {
        PyErr_Format(PyExc_SystemError,
                     "Format specifier must be a string, not %.200s",
                     Py_TYPE(format_spec)->tp_name);
        return NULL;
    }
    
    /* If no format_spec is provided, use an empty string */
    if (format_spec == NULL) {
        empty = PyUnicode_New(0);
        format_spec = empty;
    }

    /* Find the (unbound!) __format__ method */
    meth = _PyObject_LookupSpecial(obj, &PyId___format__);
    if (meth == NULL) {
        if (!PyErr_Occurred())
            PyErr_Format(PyExc_TypeError,
                         "Type %.100s doesn't define __format__",
                         Py_TYPE(obj)->tp_name);
        goto done;
    }

    /* And call it. */
    result = PyObject_CallOneArg(meth, format_spec);
    Py_DECREF(meth);

    if (result && !PyUnicode_Check(result)) {
        PyErr_Format(PyExc_TypeError,
             "__format__ must return a str, not %.200s",
             Py_TYPE(result)->tp_name);
        Py_DECREF(result);
        result = NULL;
        goto done;
    }

done:
    Py_XDECREF(empty);
    return result;
}
/* Operations on numbers */

int
PyNumber_Check(PyObject *o)
{
    return o && Py_TYPE(o)->tp_as_number &&
           (Py_TYPE(o)->tp_as_number->nb_index ||
            Py_TYPE(o)->tp_as_number->nb_int ||
            Py_TYPE(o)->tp_as_number->nb_float);
}

/* Binary operators */

#define NB_SLOT(x) offsetof(PyNumberMethods, x)
#define NB_BINOP(nb_methods, slot) \
        (*(binaryfunc*)(& ((char*)nb_methods)[slot]))
#define NB_TERNOP(nb_methods, slot) \
        (*(ternaryfunc*)(& ((char*)nb_methods)[slot]))

/*
  Calling scheme used for binary operations:

  Order operations are tried until either a valid result or error:
    w.op(v,w)[*], v.op(v,w), w.op(v,w)

  [*] only when Py_TYPE(v) != Py_TYPE(w) && Py_TYPE(w) is a subclass of
      Py_TYPE(v)
 */

static PyObject *
binary_op1(PyObject *v, PyObject *w, const int op_slot)
{
    PyObject *x;
    binaryfunc slotv = NULL;
    binaryfunc slotw = NULL;

    if (Py_TYPE(v)->tp_as_number != NULL)
        slotv = NB_BINOP(Py_TYPE(v)->tp_as_number, op_slot);
    if (!Py_IS_TYPE(w, Py_TYPE(v)) &&
        Py_TYPE(w)->tp_as_number != NULL) {
        slotw = NB_BINOP(Py_TYPE(w)->tp_as_number, op_slot);
        if (slotw == slotv)
            slotw = NULL;
    }
    if (slotv) {
        if (slotw && PyType_IsSubtype(Py_TYPE(w), Py_TYPE(v))) {
            x = slotw(v, w);
            if (x != Py_NotImplemented)
                return x;
            Py_DECREF(x); /* can't do it */
            slotw = NULL;
        }
        x = slotv(v, w);
        if (x != Py_NotImplemented)
            return x;
        Py_DECREF(x); /* can't do it */
    }
    if (slotw) {
        x = slotw(v, w);
        if (x != Py_NotImplemented)
            return x;
        Py_DECREF(x); /* can't do it */
    }
    Py_RETURN_NOTIMPLEMENTED;
}

static PyObject *
binop_type_error(PyObject *v, PyObject *w, const char *op_name)
{
    PyErr_Format(PyExc_TypeError,
                 "unsupported operand type(s) for %.100s: "
                 "'%.100s' and '%.100s'",
                 op_name,
                 Py_TYPE(v)->tp_name,
                 Py_TYPE(w)->tp_name);
    return NULL;
}

static PyObject *
binary_op(PyObject *v, PyObject *w, const int op_slot, const char *op_name)
{
    PyObject *result = binary_op1(v, w, op_slot);
    if (result == Py_NotImplemented) {
        Py_DECREF(result);
        return binop_type_error(v, w, op_name);
    }
    return result;
}


/*
  Calling scheme used for ternary operations:

  Order operations are tried until either a valid result or error:
    v.op(v,w,z), w.op(v,w,z), z.op(v,w,z)
 */

static PyObject *
ternary_op(PyObject *v,
           PyObject *w,
           PyObject *z,
           const int op_slot,
           const char *op_name)
{
    PyNumberMethods *mv, *mw, *mz;
    PyObject *x = NULL;
    ternaryfunc slotv = NULL;
    ternaryfunc slotw = NULL;
    ternaryfunc slotz = NULL;

    mv = Py_TYPE(v)->tp_as_number;
    mw = Py_TYPE(w)->tp_as_number;
    if (mv != NULL)
        slotv = NB_TERNOP(mv, op_slot);
    if (!Py_IS_TYPE(w, Py_TYPE(v)) && mw != NULL) {
        slotw = NB_TERNOP(mw, op_slot);
        if (slotw == slotv)
            slotw = NULL;
    }
    if (slotv) {
        if (slotw && PyType_IsSubtype(Py_TYPE(w), Py_TYPE(v))) {
            x = slotw(v, w, z);
            if (x != Py_NotImplemented)
                return x;
            Py_DECREF(x); /* can't do it */
            slotw = NULL;
        }
        x = slotv(v, w, z);
        if (x != Py_NotImplemented)
            return x;
        Py_DECREF(x); /* can't do it */
    }
    if (slotw) {
        x = slotw(v, w, z);
        if (x != Py_NotImplemented)
            return x;
        Py_DECREF(x); /* can't do it */
    }
    mz = Py_TYPE(z)->tp_as_number;
    if (mz != NULL) {
        slotz = NB_TERNOP(mz, op_slot);
        if (slotz == slotv || slotz == slotw)
            slotz = NULL;
        if (slotz) {
            x = slotz(v, w, z);
            if (x != Py_NotImplemented)
                return x;
            Py_DECREF(x); /* can't do it */
        }
    }

    if (z == Py_None)
        PyErr_Format(
            PyExc_TypeError,
            "unsupported operand type(s) for ** or pow(): "
            "'%.100s' and '%.100s'",
            Py_TYPE(v)->tp_name,
            Py_TYPE(w)->tp_name);
    else
        PyErr_Format(
            PyExc_TypeError,
            "unsupported operand type(s) for pow(): "
            "'%.100s', '%.100s', '%.100s'",
            Py_TYPE(v)->tp_name,
            Py_TYPE(w)->tp_name,
            Py_TYPE(z)->tp_name);
    return NULL;
}

#define BINARY_FUNC(func, op, op_name) \
    PyObject * \
    func(PyObject *v, PyObject *w) { \
        return binary_op(v, w, NB_SLOT(op), op_name); \
    }

BINARY_FUNC(PyNumber_Or, nb_or, "|")
BINARY_FUNC(PyNumber_Xor, nb_xor, "^")
BINARY_FUNC(PyNumber_And, nb_and, "&")
BINARY_FUNC(PyNumber_Lshift, nb_lshift, "<<")
BINARY_FUNC(PyNumber_Rshift, nb_rshift, ">>")
BINARY_FUNC(PyNumber_Subtract, nb_subtract, "-")
BINARY_FUNC(PyNumber_Divmod, nb_divmod, "divmod()")

PyObject *
PyNumber_Add(PyObject *v, PyObject *w)
{
    PyObject *result = binary_op1(v, w, NB_SLOT(nb_add));
    if (result == Py_NotImplemented) {
        PySequenceMethods *m = Py_TYPE(v)->tp_as_sequence;
        Py_DECREF(result);
        if (m && m->sq_concat) {
            return (*m->sq_concat)(v, w);
        }
        result = binop_type_error(v, w, "+");
    }
    return result;
}

static PyObject *
sequence_repeat(ssizeargfunc repeatfunc, PyObject *seq, PyObject *n)
{
    Py_ssize_t count;
    if (_PyIndex_Check(n)) {
        count = PyNumber_AsSsize_t(n, PyExc_OverflowError);
        if (count == -1 && PyErr_Occurred())
            return NULL;
    }
    else {
        return type_error("can't multiply sequence by "
                          "non-int of type '%.200s'", n);
    }
    return (*repeatfunc)(seq, count);
}

PyObject *
PyNumber_Multiply(PyObject *v, PyObject *w)
{
    PyObject *result = binary_op1(v, w, NB_SLOT(nb_multiply));
    if (result == Py_NotImplemented) {
        PySequenceMethods *mv = Py_TYPE(v)->tp_as_sequence;
        PySequenceMethods *mw = Py_TYPE(w)->tp_as_sequence;
        Py_DECREF(result);
        if  (mv && mv->sq_repeat) {
            return sequence_repeat(mv->sq_repeat, v, w);
        }
        else if (mw && mw->sq_repeat) {
            return sequence_repeat(mw->sq_repeat, w, v);
        }
        result = binop_type_error(v, w, "*");
    }
    return result;
}

PyObject *
PyNumber_MatrixMultiply(PyObject *v, PyObject *w)
{
    return binary_op(v, w, NB_SLOT(nb_matrix_multiply), "@");
}

PyObject *
PyNumber_FloorDivide(PyObject *v, PyObject *w)
{
    return binary_op(v, w, NB_SLOT(nb_floor_divide), "//");
}

PyObject *
PyNumber_TrueDivide(PyObject *v, PyObject *w)
{
    return binary_op(v, w, NB_SLOT(nb_true_divide), "/");
}

PyObject *
PyNumber_Remainder(PyObject *v, PyObject *w)
{
    return binary_op(v, w, NB_SLOT(nb_remainder), "%");
}

PyObject *
PyNumber_Power(PyObject *v, PyObject *w, PyObject *z)
{
    return ternary_op(v, w, z, NB_SLOT(nb_power), "** or pow()");
}

/* Unary operators and functions */

PyObject *
PyNumber_Negative(PyObject *o)
{
    PyNumberMethods *m;

    if (o == NULL) {
        return null_error();
    }

    m = Py_TYPE(o)->tp_as_number;
    if (m && m->nb_negative)
        return (*m->nb_negative)(o);

    return type_error("bad operand type for unary -: '%.200s'", o);
}

PyObject *
PyNumber_Positive(PyObject *o)
{
    PyNumberMethods *m;

    if (o == NULL) {
        return null_error();
    }

    m = Py_TYPE(o)->tp_as_number;
    if (m && m->nb_positive)
        return (*m->nb_positive)(o);

    return type_error("bad operand type for unary +: '%.200s'", o);
}

PyObject *
PyNumber_Invert(PyObject *o)
{
    PyNumberMethods *m;

    if (o == NULL) {
        return null_error();
    }

    m = Py_TYPE(o)->tp_as_number;
    if (m && m->nb_invert)
        return (*m->nb_invert)(o);

    return type_error("bad operand type for unary ~: '%.200s'", o);
}

PyObject *
PyNumber_Absolute(PyObject *o)
{
    PyNumberMethods *m;

    if (o == NULL) {
        return null_error();
    }

    m = Py_TYPE(o)->tp_as_number;
    if (m && m->nb_absolute)
        return m->nb_absolute(o);

    return type_error("bad operand type for abs(): '%.200s'", o);
}


int
PyIndex_Check(PyObject *obj)
{
    return _PyIndex_Check(obj);
}


/* Return a Python int from the object item.
   Can return an instance of int subclass.
   Raise TypeError if the result is not an int
   or if the object cannot be interpreted as an index.
*/
PyObject *
_PyNumber_Index(PyObject *item)
{
    PyObject *result = NULL;
    if (item == NULL) {
        return null_error();
    }

    if (PyLong_Check(item)) {
        Py_INCREF(item);
        return item;
    }
    if (!_PyIndex_Check(item)) {
        PyErr_Format(PyExc_TypeError,
                     "'%.200s' object cannot be interpreted "
                     "as an integer", Py_TYPE(item)->tp_name);
        return NULL;
    }
    result = Py_TYPE(item)->tp_as_number->nb_index(item);
    if (!result || PyLong_CheckExact(result))
        return result;
    if (!PyLong_Check(result)) {
        PyErr_Format(PyExc_TypeError,
                     "__index__ returned non-int (type %.200s)",
                     Py_TYPE(result)->tp_name);
        Py_DECREF(result);
        return NULL;
    }
    return result;
}

/* Return an exact Python int from the object item.
   Raise TypeError if the result is not an int
   or if the object cannot be interpreted as an index.
*/
PyObject *
PyNumber_Index(PyObject *item)
{
    PyObject *result = _PyNumber_Index(item);
    if (result != NULL && !PyLong_CheckExact(result)) {
        Py_SETREF(result, _PyLong_Copy((PyLongObject *)result));
    }
    return result;
}

/* Return an error on Overflow only if err is not NULL*/

Py_ssize_t
PyNumber_AsSsize_t(PyObject *item, PyObject *err)
{
    Py_ssize_t result;
    PyObject *runerr;
    PyObject *value = _PyNumber_Index(item);
    if (value == NULL)
        return -1;

    /* We're done if PyLong_AsSsize_t() returns without error. */
    result = PyLong_AsSsize_t(value);
    if (result != -1 || !(runerr = PyErr_Occurred()))
        goto finish;

    /* Error handling code -- only manage OverflowError differently */
    if (!PyErr_GivenExceptionMatches(runerr, PyExc_OverflowError))
        goto finish;

    PyErr_Clear();
    /* If no error-handling desired then the default clipping
       is sufficient.
     */
    if (!err) {
        assert(PyLong_Check(value));
        /* Whether or not it is less than or equal to
           zero is determined by the sign of ob_size
        */
        if (_PyLong_Sign(value) < 0)
            result = PY_SSIZE_T_MIN;
        else
            result = PY_SSIZE_T_MAX;
    }
    else {
        /* Otherwise replace the error with caller's error object. */
        PyErr_Format(err,
                     "cannot fit '%.200s' into an index-sized integer",
                     Py_TYPE(item)->tp_name);
    }

 finish:
    Py_DECREF(value);
    return result;
}


PyObject *
PyNumber_Long(PyObject *o)
{
    PyObject *result;
    PyNumberMethods *m;
    PyObject *trunc_func;
    _Py_IDENTIFIER(__trunc__);

    if (o == NULL) {
        return null_error();
    }

    if (PyLong_CheckExact(o)) {
        Py_INCREF(o);
        return o;
    }
    m = Py_TYPE(o)->tp_as_number;
    if (m && m->nb_int) { /* This should include subclasses of int */
        /* Convert using the nb_int slot, which should return something
           of exact type int. */
        result = m->nb_int(o);
        if (!result || PyLong_CheckExact(result))
            return result;
        if (!PyLong_Check(result)) {
            PyErr_Format(PyExc_TypeError,
                        "__int__ returned non-int (type %.200s)",
                        result->ob_type->tp_name);
            Py_DECREF(result);
            return NULL;
        }
        /* Issue #17576: warn if 'result' not of exact type int. */
        Py_SETREF(result, _PyLong_Copy((PyLongObject *)result));
        return result;
    }
    if (m && m->nb_index) {
        return PyNumber_Index(o);
    }
    trunc_func = _PyObject_LookupSpecial(o, &PyId___trunc__);
    if (trunc_func) {
        result = _PyObject_CallNoArg(trunc_func);
        Py_DECREF(trunc_func);
        if (result == NULL || PyLong_CheckExact(result)) {
            return result;
        }
        if (PyLong_Check(result)) {
            Py_SETREF(result, _PyLong_Copy((PyLongObject *)result));
            return result;
        }
        /* __trunc__ is specified to return an Integral type,
           but int() needs to return an int. */
        if (!PyIndex_Check(result)) {
            PyErr_Format(
                PyExc_TypeError,
                "__trunc__ returned non-Integral (type %.200s)",
                Py_TYPE(result)->tp_name);
            Py_DECREF(result);
            return NULL;
        }
        Py_SETREF(result, PyNumber_Index(result));
        return result;
    }
    if (PyErr_Occurred())
        return NULL;

    if (PyUnicode_Check(o))
        /* The below check is done in PyLong_FromUnicode(). */
        return PyLong_FromUnicodeObject(o, 10);

    return type_error("int() argument must be a string, a bytes-like object "
                      "or a number, not '%.200s'", o);
}

PyObject *
PyNumber_Float(PyObject *o)
{
    PyNumberMethods *m;

    if (o == NULL) {
        return null_error();
    }

    if (PyFloat_CheckExact(o)) {
        Py_INCREF(o);
        return o;
    }
    m = Py_TYPE(o)->tp_as_number;
    if (m && m->nb_float) { /* This should include subclasses of float */
        PyObject *res = m->nb_float(o);
        double val;
        if (!res || PyFloat_CheckExact(res)) {
            return res;
        }
        if (!PyFloat_Check(res)) {
            PyErr_Format(PyExc_TypeError,
                         "%.50s.__float__ returned non-float (type %.50s)",
                         Py_TYPE(o)->tp_name, Py_TYPE(res)->tp_name);
            Py_DECREF(res);
            return NULL;
        }
        val = PyFloat_AsDouble(res);
        Py_DECREF(res);
        return PyFloat_FromDouble(val);
    }
    if (m && m->nb_index) {
        PyObject *res = _PyNumber_Index(o);
        if (!res) {
            return NULL;
        }
        double val = PyLong_AsDouble(res);
        Py_DECREF(res);
        if (val == -1.0 && PyErr_Occurred()) {
            return NULL;
        }
        return PyFloat_FromDouble(val);
    }
    if (PyFloat_Check(o)) { /* A float subclass with nb_float == NULL */
        return PyFloat_FromDouble(PyFloat_AsDouble(o));
    }
    return PyFloat_FromString(o);
}


PyObject *
PyNumber_ToBase(PyObject *n, int base)
{
    if (!(base == 2 || base == 8 || base == 10 || base == 16)) {
        PyErr_SetString(PyExc_SystemError,
                        "PyNumber_ToBase: base must be 2, 8, 10 or 16");
        return NULL;
    }
    PyObject *index = _PyNumber_Index(n);
    if (!index)
        return NULL;
    PyObject *res = _PyLong_Format(index, base);
    Py_DECREF(index);
    return res;
}


/* Operations on sequences */

int
PySequence_Check(PyObject *s)
{
    if (PyDict_Check(s))
        return 0;
    return Py_TYPE(s)->tp_as_sequence &&
        Py_TYPE(s)->tp_as_sequence->sq_item != NULL;
}

Py_ssize_t
PySequence_Size(PyObject *s)
{
    PySequenceMethods *m;

    if (s == NULL) {
        null_error();
        return -1;
    }

    m = Py_TYPE(s)->tp_as_sequence;
    if (m && m->sq_length) {
        Py_ssize_t len = m->sq_length(s);
        assert(len >= 0 || PyErr_Occurred());
        return len;
    }

    if (Py_TYPE(s)->tp_as_mapping && Py_TYPE(s)->tp_as_mapping->mp_length) {
        type_error("%.200s is not a sequence", s);
        return -1;
    }
    type_error("object of type '%.200s' has no len()", s);
    return -1;
}

#undef PySequence_Length
Py_ssize_t
PySequence_Length(PyObject *s)
{
    return PySequence_Size(s);
}
#define PySequence_Length PySequence_Size

PyObject *
PySequence_Concat(PyObject *s, PyObject *o)
{
    PySequenceMethods *m;

    if (s == NULL || o == NULL) {
        return null_error();
    }

    m = Py_TYPE(s)->tp_as_sequence;
    if (m && m->sq_concat)
        return m->sq_concat(s, o);

    /* Instances of user classes defining an __add__() method only
       have an nb_add slot, not an sq_concat slot.      So we fall back
       to nb_add if both arguments appear to be sequences. */
    if (PySequence_Check(s) && PySequence_Check(o)) {
        PyObject *result = binary_op1(s, o, NB_SLOT(nb_add));
        if (result != Py_NotImplemented)
            return result;
        Py_DECREF(result);
    }
    return type_error("'%.200s' object can't be concatenated", s);
}

PyObject *
PySequence_Repeat(PyObject *o, Py_ssize_t count)
{
    PySequenceMethods *m;

    if (o == NULL) {
        return null_error();
    }

    m = Py_TYPE(o)->tp_as_sequence;
    if (m && m->sq_repeat)
        return m->sq_repeat(o, count);

    /* Instances of user classes defining a __mul__() method only
       have an nb_multiply slot, not an sq_repeat slot. so we fall back
       to nb_multiply if o appears to be a sequence. */
    if (PySequence_Check(o)) {
        PyObject *n, *result;
        n = PyLong_FromSsize_t(count);
        if (n == NULL)
            return NULL;
        result = binary_op1(o, n, NB_SLOT(nb_multiply));
        Py_DECREF(n);
        if (result != Py_NotImplemented)
            return result;
        Py_DECREF(result);
    }
    return type_error("'%.200s' object can't be repeated", o);
}

PyObject *
PySequence_GetItem(PyObject *s, Py_ssize_t i)
{
    PySequenceMethods *m;

    if (s == NULL) {
        return null_error();
    }

    m = Py_TYPE(s)->tp_as_sequence;
    if (m && m->sq_item) {
        if (i < 0) {
            if (m->sq_length) {
                Py_ssize_t l = (*m->sq_length)(s);
                if (l < 0) {
                    assert(PyErr_Occurred());
                    return NULL;
                }
                i += l;
            }
        }
        return m->sq_item(s, i);
    }

    if (Py_TYPE(s)->tp_as_mapping && Py_TYPE(s)->tp_as_mapping->mp_subscript) {
        return type_error("%.200s is not a sequence", s);
    }
    return type_error("'%.200s' object does not support indexing", s);
}

PyObject *
PySequence_GetSlice(PyObject *s, Py_ssize_t i1, Py_ssize_t i2)
{
    PyMappingMethods *mp;

    if (!s) {
        return null_error();
    }

    mp = Py_TYPE(s)->tp_as_mapping;
    if (mp && mp->mp_subscript) {
        PyObject *res;
        PyObject *slice = _PySlice_FromIndices(i1, i2);
        if (!slice)
            return NULL;
        res = mp->mp_subscript(s, slice);
        Py_DECREF(slice);
        return res;
    }

    return type_error("'%.200s' object is unsliceable", s);
}

int
PySequence_SetItem(PyObject *s, Py_ssize_t i, PyObject *o)
{
    PySequenceMethods *m;

    if (s == NULL) {
        null_error();
        return -1;
    }

    m = Py_TYPE(s)->tp_as_sequence;
    if (m && m->sq_ass_item) {
        if (i < 0) {
            if (m->sq_length) {
                Py_ssize_t l = (*m->sq_length)(s);
                if (l < 0) {
                    assert(PyErr_Occurred());
                    return -1;
                }
                i += l;
            }
        }
        return m->sq_ass_item(s, i, o);
    }

    if (Py_TYPE(s)->tp_as_mapping && Py_TYPE(s)->tp_as_mapping->mp_ass_subscript) {
        type_error("%.200s is not a sequence", s);
        return -1;
    }
    type_error("'%.200s' object does not support item assignment", s);
    return -1;
}

int
PySequence_DelItem(PyObject *s, Py_ssize_t i)
{
    PySequenceMethods *m;

    if (s == NULL) {
        null_error();
        return -1;
    }

    m = Py_TYPE(s)->tp_as_sequence;
    if (m && m->sq_ass_item) {
        if (i < 0) {
            if (m->sq_length) {
                Py_ssize_t l = (*m->sq_length)(s);
                if (l < 0) {
                    assert(PyErr_Occurred());
                    return -1;
                }
                i += l;
            }
        }
        return m->sq_ass_item(s, i, (PyObject *)NULL);
    }

    if (Py_TYPE(s)->tp_as_mapping && Py_TYPE(s)->tp_as_mapping->mp_ass_subscript) {
        type_error("%.200s is not a sequence", s);
        return -1;
    }
    type_error("'%.200s' object doesn't support item deletion", s);
    return -1;
}

int
PySequence_SetSlice(PyObject *s, Py_ssize_t i1, Py_ssize_t i2, PyObject *o)
{
    PyMappingMethods *mp;

    if (s == NULL) {
        null_error();
        return -1;
    }

    mp = Py_TYPE(s)->tp_as_mapping;
    if (mp && mp->mp_ass_subscript) {
        int res;
        PyObject *slice = _PySlice_FromIndices(i1, i2);
        if (!slice)
            return -1;
        res = mp->mp_ass_subscript(s, slice, o);
        Py_DECREF(slice);
        return res;
    }

    type_error("'%.200s' object doesn't support slice assignment", s);
    return -1;
}

int
PySequence_DelSlice(PyObject *s, Py_ssize_t i1, Py_ssize_t i2)
{
    PyMappingMethods *mp;

    if (s == NULL) {
        null_error();
        return -1;
    }

    mp = Py_TYPE(s)->tp_as_mapping;
    if (mp && mp->mp_ass_subscript) {
        int res;
        PyObject *slice = _PySlice_FromIndices(i1, i2);
        if (!slice)
            return -1;
        res = mp->mp_ass_subscript(s, slice, NULL);
        Py_DECREF(slice);
        return res;
    }
    type_error("'%.200s' object doesn't support slice deletion", s);
    return -1;
}

PyObject *
PySequence_Tuple(PyObject *v)
{
    PyObject *it;  /* iter(v) */
    Py_ssize_t n;             /* guess for result tuple size */
    PyObject *result = NULL;
    PyObject **arr = NULL;
    Py_ssize_t j;

    if (v == NULL) {
        return null_error();
    }
    /* Get iterator. */
    it = PyObject_GetIter(v);
    if (it == NULL)
        return NULL;

    /* Guess result size and allocate space. */
    n = PyObject_LengthHint(v, 10);
    if (n == -1)
        goto Fail;
    arr = (PyObject **) malloc(sizeof(PyObject *)*n);
    if (arr == NULL)
      goto Fail;
    
    /* Fill the tuple. */
    for (j = 0; ; ++j) {
        PyObject *item = PyIter_Next(it);
        if (item == NULL) {
            if (PyErr_Occurred())
                goto Fail;
            break;
        }
        if (j >= n) {
            size_t newn = (size_t)n;
            /* The over-allocation strategy can grow a bit faster
               than for lists because unlike lists the
               over-allocation isn't permanent -- we reclaim
               the excess before the end of this routine.
               So, grow by ten and then add 25%.
            */
            newn += 10u;
            newn += newn >> 2;
            if (newn > PY_SSIZE_T_MAX) {
                /* Check for overflow */
                PyErr_NoMemory();
                Py_DECREF(item);
                goto Fail;
            }
            n = (Py_ssize_t)newn;
	    {
	      PyObject **newarr = (PyObject **) realloc(arr, n*sizeof(PyObject *));
	      if (newarr == NULL) {
		Py_DECREF(item);
		goto Fail;
	      }
	      arr = newarr;
	    }
        }
	arr[j] = item;
    }

    result = PyTuple_FromArray(arr, j);
    Py_DECREF(it);
    free(arr);
    return result;

Fail:
    Py_XDECREF(result);
    Py_DECREF(it);
    if (arr != NULL) {
      free(arr);
    }
    return NULL;
}

PyObject *
PySequence_List(PyObject *v)
{
    PyObject *result;  /* result list */
    PyObject *rv;          /* return value from PyList_Extend */

    if (v == NULL) {
        return null_error();
    }

    result = PyList_New(0);
    if (result == NULL)
        return NULL;

    rv = PyList_Extend(result, v);
    if (rv == NULL) {
        Py_DECREF(result);
        return NULL;
    }
    Py_DECREF(rv);
    return result;
}

PyObject *
PySequence_Fast(PyObject *v, const char *m)
{
    PyObject *it;

    if (v == NULL) {
        return null_error();
    }

    if (PyList_CheckExact(v) || PyTuple_CheckExact(v)) {
        Py_INCREF(v);
        return v;
    }

    it = PyObject_GetIter(v);
    if (it == NULL) {
        if (PyErr_ExceptionMatches(PyExc_TypeError))
            PyErr_SetString(PyExc_TypeError, m);
        return NULL;
    }

    v = PySequence_List(it);
    Py_DECREF(it);

    return v;
}

/* Iterate over seq.  Result depends on the operation:
   PY_ITERSEARCH_COUNT:  -1 if error, else # of times obj appears in seq.
   PY_ITERSEARCH_INDEX:  0-based index of first occurrence of obj in seq;
    set ValueError and return -1 if none found; also return -1 on error.
   Py_ITERSEARCH_CONTAINS:  return 1 if obj in seq, else 0; -1 on error.
*/
Py_ssize_t
_PySequence_IterSearch(PyObject *seq, PyObject *obj, int operation)
{
    Py_ssize_t n;
    int wrapped;  /* for PY_ITERSEARCH_INDEX, true iff n wrapped around */
    PyObject *it;  /* iter(seq) */

    if (seq == NULL || obj == NULL) {
        null_error();
        return -1;
    }

    it = PyObject_GetIter(seq);
    if (it == NULL) {
        type_error("argument of type '%.200s' is not iterable", seq);
        return -1;
    }

    n = wrapped = 0;
    for (;;) {
        int cmp;
        PyObject *item = PyIter_Next(it);
        if (item == NULL) {
            if (PyErr_Occurred())
                goto Fail;
            break;
        }

        cmp = PyObject_RichCompareBool(item, obj, Py_EQ);
        Py_DECREF(item);
        if (cmp < 0)
            goto Fail;
        if (cmp > 0) {
            switch (operation) {
            case PY_ITERSEARCH_COUNT:
                if (n == PY_SSIZE_T_MAX) {
                    PyErr_SetString(PyExc_OverflowError,
                           "count exceeds C integer size");
                    goto Fail;
                }
                ++n;
                break;

            case PY_ITERSEARCH_INDEX:
                if (wrapped) {
                    PyErr_SetString(PyExc_OverflowError,
                           "index exceeds C integer size");
                    goto Fail;
                }
                goto Done;

            case PY_ITERSEARCH_CONTAINS:
                n = 1;
                goto Done;

            default:
                Py_UNREACHABLE();
            }
        }

        if (operation == PY_ITERSEARCH_INDEX) {
            if (n == PY_SSIZE_T_MAX)
                wrapped = 1;
            ++n;
        }
    }

    if (operation != PY_ITERSEARCH_INDEX)
        goto Done;

    PyErr_SetString(PyExc_ValueError,
                    "sequence.index(x): x not in sequence");
    /* fall into failure code */
Fail:
    n = -1;
    /* fall through */
Done:
    Py_DECREF(it);
    return n;

}

/* Return # of times o appears in s. */
Py_ssize_t
PySequence_Count(PyObject *s, PyObject *o)
{
    return _PySequence_IterSearch(s, o, PY_ITERSEARCH_COUNT);
}

/* Return -1 if error; 1 if ob in seq; 0 if ob not in seq.
 * Use sq_contains if possible, else defer to _PySequence_IterSearch().
 */
int
PySequence_Contains(PyObject *seq, PyObject *ob)
{
    Py_ssize_t result;
    PySequenceMethods *sqm = Py_TYPE(seq)->tp_as_sequence;
    if (sqm != NULL && sqm->sq_contains != NULL)
        return (*sqm->sq_contains)(seq, ob);
    result = _PySequence_IterSearch(seq, ob, PY_ITERSEARCH_CONTAINS);
    return Py_SAFE_DOWNCAST(result, Py_ssize_t, int);
}

/* Backwards compatibility */
#undef PySequence_In
int
PySequence_In(PyObject *w, PyObject *v)
{
    return PySequence_Contains(w, v);
}

Py_ssize_t
PySequence_Index(PyObject *s, PyObject *o)
{
    return _PySequence_IterSearch(s, o, PY_ITERSEARCH_INDEX);
}

/* Operations on mappings */

int
PyMapping_Check(PyObject *o)
{
    return o && Py_TYPE(o)->tp_as_mapping &&
        Py_TYPE(o)->tp_as_mapping->mp_subscript;
}

Py_ssize_t
PyMapping_Size(PyObject *o)
{
    PyMappingMethods *m;

    if (o == NULL) {
        null_error();
        return -1;
    }

    m = Py_TYPE(o)->tp_as_mapping;
    if (m && m->mp_length) {
        Py_ssize_t len = m->mp_length(o);
        assert(len >= 0 || PyErr_Occurred());
        return len;
    }

    if (Py_TYPE(o)->tp_as_sequence && Py_TYPE(o)->tp_as_sequence->sq_length) {
        type_error("%.200s is not a mapping", o);
        return -1;
    }
    /* PyMapping_Size() can be called from PyObject_Size(). */
    type_error("object of type '%.200s' has no len()", o);
    return -1;
}

#undef PyMapping_Length
Py_ssize_t
PyMapping_Length(PyObject *o)
{
    return PyMapping_Size(o);
}
#define PyMapping_Length PyMapping_Size

PyObject *
PyMapping_GetItemString(PyObject *o, const char *key)
{
    PyObject *okey, *r;

    if (key == NULL) {
        return null_error();
    }

    okey = PyUnicode_FromString(key);
    if (okey == NULL)
        return NULL;
    r = PyObject_GetItem(o, okey);
    Py_DECREF(okey);
    return r;
}

int
PyMapping_SetItemString(PyObject *o, const char *key, PyObject *value)
{
    PyObject *okey;
    int r;

    if (key == NULL) {
        null_error();
        return -1;
    }

    okey = PyUnicode_FromString(key);
    if (okey == NULL)
        return -1;
    r = PyObject_SetItem(o, okey, value);
    Py_DECREF(okey);
    return r;
}

int
PyMapping_HasKeyString(PyObject *o, const char *key)
{
    PyObject *v;

    v = PyMapping_GetItemString(o, key);
    if (v) {
        Py_DECREF(v);
        return 1;
    }
    PyErr_Clear();
    return 0;
}

int
PyMapping_HasKey(PyObject *o, PyObject *key)
{
    PyObject *v;

    v = PyObject_GetItem(o, key);
    if (v) {
        Py_DECREF(v);
        return 1;
    }
    PyErr_Clear();
    return 0;
}

/* This function is quite similar to PySequence_Fast(), but specialized to be
   a helper for PyMapping_Keys(), PyMapping_Items() and PyMapping_Values().
 */
static PyObject *
method_output_as_list(PyObject *o, _Py_Identifier *meth_id)
{
    PyObject *it, *result, *meth_output;

    assert(o != NULL);
    meth_output = _PyObject_CallMethodIdNoArgs(o, meth_id);
    if (meth_output == NULL || PyList_CheckExact(meth_output)) {
        return meth_output;
    }
    it = PyObject_GetIter(meth_output);
    if (it == NULL) {
        if (PyErr_ExceptionMatches(PyExc_TypeError)) {
            PyErr_Format(PyExc_TypeError,
                         "%.200s.%U() returned a non-iterable (type %.200s)",
                         Py_TYPE(o)->tp_name,
                         _PyUnicode_FromId(meth_id),
                         Py_TYPE(meth_output)->tp_name);
        }
        Py_DECREF(meth_output);
        return NULL;
    }
    Py_DECREF(meth_output);
    result = PySequence_List(it);
    Py_DECREF(it);
    return result;
}

PyObject *
PyMapping_Keys(PyObject *o)
{
    _Py_IDENTIFIER(keys);

    if (o == NULL) {
        return null_error();
    }
    return method_output_as_list(o, &PyId_keys);
}

PyObject *
PyMapping_Items(PyObject *o)
{
    _Py_IDENTIFIER(items);

    if (o == NULL) {
        return null_error();
    }
    return method_output_as_list(o, &PyId_items);
}

PyObject *
PyMapping_Values(PyObject *o)
{
    _Py_IDENTIFIER(values);

    if (o == NULL) {
      return null_error();
    }
    return method_output_as_list(o, &PyId_values);
}

/* isinstance(), issubclass() */

/* abstract_get_bases() has logically 4 return states:
 *
 * 1. getattr(cls, '__bases__') could raise an AttributeError
 * 2. getattr(cls, '__bases__') could raise some other exception
 * 3. getattr(cls, '__bases__') could return a tuple
 * 4. getattr(cls, '__bases__') could return something other than a tuple
 *
 * Only state #3 is a non-error state and only it returns a non-NULL object
 * (it returns the retrieved tuple).
 *
 * Any raised AttributeErrors are masked by clearing the exception and
 * returning NULL.  If an object other than a tuple comes out of __bases__,
 * then again, the return value is NULL.  So yes, these two situations
 * produce exactly the same results: NULL is returned and no error is set.
 *
 * If some exception other than AttributeError is raised, then NULL is also
 * returned, but the exception is not cleared.  That's because we want the
 * exception to be propagated along.
 *
 * Callers are expected to test for PyErr_Occurred() when the return value
 * is NULL to decide whether a valid exception should be propagated or not.
 * When there's no exception to propagate, it's customary for the caller to
 * set a TypeError.
 */
static PyObject *
abstract_get_bases(PyObject *cls)
{
    _Py_IDENTIFIER(__bases__);
    PyObject *bases;

    (void)_PyObject_LookupAttrId(cls, &PyId___bases__, &bases);
    if (bases != NULL && !PyTuple_Check(bases)) {
        Py_DECREF(bases);
        return NULL;
    }
    return bases;
}


static int
abstract_issubclass(PyObject *derived, PyObject *cls)
{
    PyObject *bases = NULL;
    Py_ssize_t i, n;
    int r = 0;

    while (1) {
        if (derived == cls) {
            Py_XDECREF(bases); /* See below comment */
            return 1;
        }
        /* Use XSETREF to drop bases reference *after* finishing with
           derived; bases might be the only reference to it.
           XSETREF is used instead of SETREF, because bases is NULL on the
           first iteration of the loop.
        */
        Py_XSETREF(bases, abstract_get_bases(derived));
        if (bases == NULL) {
            if (PyErr_Occurred())
                return -1;
            return 0;
        }
        n = PyTuple_Size(bases);
        if (n == 0) {
            Py_DECREF(bases);
            return 0;
        }
        /* Avoid recursivity in the single inheritance case */
        if (n == 1) {
            derived = PyTuple_GetItem(bases, 0);
            continue;
        }
        for (i = 0; i < n; i++) {
            r = abstract_issubclass(PyTuple_GetItem(bases, i), cls);
            if (r != 0)
                break;
        }
        Py_DECREF(bases);
        return r;
    }
}

static int
check_class(PyObject *cls, const char *error)
{
    PyObject *bases = abstract_get_bases(cls);
    if (bases == NULL) {
        /* Do not mask errors. */
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_TypeError, error);
        return 0;
    }
    Py_DECREF(bases);
    return -1;
}

static int
object_isinstance(PyObject *inst, PyObject *cls)
{
    PyObject *icls;
    int retval;
    _Py_IDENTIFIER(__class__);

    if (PyType_Check(cls)) {
        retval = PyObject_TypeCheck(inst, (PyTypeObject *)cls);
        if (retval == 0) {
            retval = _PyObject_LookupAttrId(inst, &PyId___class__, &icls);
            if (icls != NULL) {
                if (icls != (PyObject *)(Py_TYPE(inst)) && PyType_Check(icls)) {
                    retval = PyType_IsSubtype(
                        (PyTypeObject *)icls,
                        (PyTypeObject *)cls);
                }
                else {
                    retval = 0;
                }
                Py_DECREF(icls);
            }
        }
    }
    else {
        if (!check_class(cls,
            "isinstance() arg 2 must be a type or tuple of types"))
            return -1;
        retval = _PyObject_LookupAttrId(inst, &PyId___class__, &icls);
        if (icls != NULL) {
            retval = abstract_issubclass(icls, cls);
            Py_DECREF(icls);
        }
    }

    return retval;
}

static int
object_recursive_isinstance(PyThreadState *tstate, PyObject *inst, PyObject *cls)
{
    _Py_IDENTIFIER(__instancecheck__);

    /* Quick test for an exact match */
    if (Py_IS_TYPE(inst, (PyTypeObject *)cls)) {
        return 1;
    }

    /* We know what type's __instancecheck__ does. */
    if (PyType_CheckExact(cls)) {
        return object_isinstance(inst, cls);
    }

    if (PyTuple_Check(cls)) {
        Py_ssize_t n = PyTuple_Size(cls);
        int r = 0;
        for (Py_ssize_t i = 0; i < n; ++i) {
            PyObject *item = PyTuple_GetItem(cls, i);
            r = object_recursive_isinstance(tstate, inst, item);
            if (r != 0) {
                /* either found it, or got an error */
                break;
            }
        }
        return r;
    }

    PyObject *checker = _PyObject_LookupSpecial(cls, &PyId___instancecheck__);
    if (checker != NULL) {
        PyObject *res = PyObject_CallOneArg(checker, inst);
        Py_DECREF(checker);

        if (res == NULL) {
            return -1;
        }
        int ok = PyObject_IsTrue(res);
        Py_DECREF(res);

        return ok;
    }
    else if (_PyErr_Occurred(tstate)) {
        return -1;
    }

    /* cls has no __instancecheck__() method */
    return object_isinstance(inst, cls);
}


int
PyObject_IsInstance(PyObject *inst, PyObject *cls)
{
    PyThreadState *tstate = PyThreadState_Get();
    return object_recursive_isinstance(tstate, inst, cls);
}


static  int
recursive_issubclass(PyObject *derived, PyObject *cls)
{
    if (PyType_Check(cls) && PyType_Check(derived)) {
        /* Fast path (non-recursive) */
        return PyType_IsSubtype((PyTypeObject *)derived, (PyTypeObject *)cls);
    }
    if (!check_class(derived,
                     "issubclass() arg 1 must be a class"))
        return -1;
    if (!check_class(cls,
                    "issubclass() arg 2 must be a class"
                    " or tuple of classes"))
        return -1;

    return abstract_issubclass(derived, cls);
}

static int
object_issubclass(PyThreadState *tstate, PyObject *derived, PyObject *cls)
{
    _Py_IDENTIFIER(__subclasscheck__);
    PyObject *checker;

    /* We know what type's __subclasscheck__ does. */
    if (PyType_CheckExact(cls)) {
        /* Quick test for an exact match */
        if (derived == cls)
            return 1;
        return recursive_issubclass(derived, cls);
    }

    if (PyTuple_Check(cls)) {
        Py_ssize_t n = PyTuple_Size(cls);
        int r = 0;
        for (Py_ssize_t i = 0; i < n; ++i) {
            PyObject *item = PyTuple_GetItem(cls, i);
            r = object_issubclass(tstate, derived, item);
            if (r != 0)
                /* either found it, or got an error */
                break;
        }
        return r;
    }

    checker = _PyObject_LookupSpecial(cls, &PyId___subclasscheck__);
    if (checker != NULL) {
        int ok = -1;
        PyObject *res = PyObject_CallOneArg(checker, derived);
        Py_DECREF(checker);
        if (res != NULL) {
            ok = PyObject_IsTrue(res);
            Py_DECREF(res);
        }
        return ok;
    }
    else if (_PyErr_Occurred(tstate)) {
        return -1;
    }

    /* Probably never reached anymore. */
    return recursive_issubclass(derived, cls);
}


int
PyObject_IsSubclass(PyObject *derived, PyObject *cls)
{
    PyThreadState *tstate = PyThreadState_Get();
    return object_issubclass(tstate, derived, cls);
}


int
_PyObject_RealIsInstance(PyObject *inst, PyObject *cls)
{
    return object_isinstance(inst, cls);
}

int
_PyObject_RealIsSubclass(PyObject *derived, PyObject *cls)
{
    return recursive_issubclass(derived, cls);
}


PyObject *
PyObject_GetIter(PyObject *o)
{
    PyTypeObject *t = Py_TYPE(o);
    getiterfunc f;

    f = t->tp_iter;
    if (f == NULL) {
        if (PySequence_Check(o))
            return PySeqIter_New(o);
        return type_error("'%.200s' object is not iterable", o);
    }
    else {
        PyObject *res = (*f)(o);
        if (res != NULL && !PyIter_Check(res)) {
            PyErr_Format(PyExc_TypeError,
                         "iter() returned non-iterator "
                         "of type '%.100s'",
                         Py_TYPE(res)->tp_name);
            Py_DECREF(res);
            res = NULL;
        }
        return res;
    }
}

#undef PyIter_Check

int PyIter_Check(PyObject *obj)
{
    return Py_TYPE(obj)->tp_iternext != NULL &&
           Py_TYPE(obj)->tp_iternext != &_PyObject_NextNotImplemented;
}

/* Return next item.
 * If an error occurs, return NULL.  PyErr_Occurred() will be true.
 * If the iteration terminates normally, return NULL and clear the
 * PyExc_StopIteration exception (if it was set).  PyErr_Occurred()
 * will be false.
 * Else return the next object.  PyErr_Occurred() will be false.
 */
PyObject *
PyIter_Next(PyObject *iter)
{
    PyObject *result;
    result = (*Py_TYPE(iter)->tp_iternext)(iter);
    if (result == NULL &&
        PyErr_Occurred() &&
        PyErr_ExceptionMatches(PyExc_StopIteration))
        PyErr_Clear();
    return result;
}
