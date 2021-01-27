
/* Execute compiled code */

/* XXX TO DO:
   XXX speed up searching for keywords by using a dictionary
   XXX document it!
   */

/* enable more aggressive intra-module optimizations, where available */

#include "Python.h"
#include "pycore_abstract.h"      // _PyIndex_Check()
#include "pycore_call.h"
#include "pycore_ceval.h"
#include "pycore_code.h"
#include "pycore_initconfig.h"
#include "pycore_object.h"
#include "pycore_pyerrors.h"
#include "pycore_pylifecycle.h"
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_sysmodule.h"

#include "code.h"
#include "dictobject.h"
#include "frameobject.h"
#include "opcode.h"
#include "setobject.h"

#include <ctype.h>

#if !defined(Py_BUILD_CORE)
#  error "ceval.c must be build with Py_BUILD_CORE define for best performance"
#endif

_Py_IDENTIFIER(__name__);

/* Forward declarations */
Py_LOCAL_INLINE(PyObject *) call_function(
    PyThreadState *tstate, PyObject ***pp_stack,
    Py_ssize_t oparg, PyObject *kwnames);
static PyObject * do_call_core(
    PyThreadState *tstate, PyObject *func,
    PyObject *callargs, PyObject *kwdict);

static PyObject * import_name(PyThreadState *, PyFrameObject *,
                              PyObject *, PyObject *, PyObject *);
static PyObject * import_from(PyThreadState *, PyObject *, PyObject *);
static int import_all_from(PyThreadState *, PyObject *, PyObject *);
static void format_exc_check_arg(PyThreadState *, PyObject *, const char *, PyObject *);
static void format_exc_unbound(PyThreadState *tstate, PyCodeObject *co, int oparg);
static PyObject * special_lookup(PyThreadState *, PyObject *, _Py_Identifier *);
static int check_args_iterable(PyThreadState *, PyObject *func, PyObject *vararg);
static void format_kwargs_error(PyThreadState *, PyObject *func, PyObject *kwargs);

#define NAME_ERROR_MSG \
    "name '%.200s' is not defined"
#define UNBOUNDLOCAL_ERROR_MSG \
    "local variable '%.200s' referenced before assignment"
#define UNBOUNDFREE_ERROR_MSG \
    "free variable '%.200s' referenced before assignment" \
    " in enclosing scope"

#ifndef NDEBUG
/* Ensure that tstate is valid: sanity check for PyEval_AcquireThread() and
   PyEval_RestoreThread(). Detect if tstate memory was freed. It can happen
   when a thread continues to run after Python finalization, especially
   daemon threads. */
static int
is_tstate_valid(PyThreadState *tstate)
{
    assert(!_PyMem_IsPtrFreed(tstate));
    assert(!_PyMem_IsPtrFreed(tstate->interp));
    return 1;
}
#endif


#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

int
_PyEval_ThreadsInitialized(_PyRuntimeState *runtime)
{
  return 1;
}

int
PyEval_ThreadsInitialized(void)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    return _PyEval_ThreadsInitialized(runtime);
}

static int do_raise(PyThreadState *tstate, PyObject *exc, PyObject *cause);
static int unpack_iterable(PyThreadState *, PyObject *, int, int, PyObject **);

#define _Py_TracingPossible(ceval) ((ceval)->tracing_possible)


PyObject *
PyEval_EvalCode(PyObject *co, PyObject *globals, PyObject *locals)
{
    return PyEval_EvalCodeEx(co,
                      globals, locals,
                      (PyObject **)NULL, 0,
                      (PyObject **)NULL, 0,
                      (PyObject **)NULL, 0,
                      NULL, NULL);
}


/* Interpreter main loop */

PyObject *
PyEval_EvalFrame(PyFrameObject *f)
{
    /* Function kept for backward compatibility */
    PyThreadState *tstate = PyThreadState_Get();
    return _PyEval_EvalFrame(tstate, f, 0);
}

PyObject *
PyEval_EvalFrameEx(PyFrameObject *f, int throwflag)
{
    PyThreadState *tstate = PyThreadState_Get();
    return _PyEval_EvalFrame(tstate, f, throwflag);
}

PyObject* _Py_HOT_FUNCTION
_PyEval_EvalFrameDefault(PyThreadState *tstate, PyFrameObject *f, int throwflag)
{
    PyObject **stack_pointer;  /* Next free slot in value stack */
    const _Py_CODEUNIT *next_instr;
    int opcode;        /* Current opcode */
    int oparg;         /* Current opcode argument, if any */
    PyObject **fastlocals, **freevars;
    PyObject *retval = NULL;            /* Return value */
    PyCodeObject *co;

    const _Py_CODEUNIT *first_instr;
    PyObject *names;
    PyObject *consts;

/* Tuple access macros */

#define GETITEM(v, i) PyTuple_GetItem((v), (i))

/* Code access macros */

/* The integer overflow is checked by an assertion below. */
#define INSTR_OFFSET()  \
    (sizeof(_Py_CODEUNIT) * (int)(next_instr - first_instr))
#define NEXTOPARG()  do { \
        _Py_CODEUNIT word = *next_instr; \
        opcode = _Py_OPCODE(word); \
        oparg = _Py_OPARG(word); \
        next_instr++; \
    } while (0)
#define JUMPTO(x)       (next_instr = first_instr + (x) / sizeof(_Py_CODEUNIT))
#define JUMPBY(x)       (next_instr += (x) / sizeof(_Py_CODEUNIT))

/* Stack manipulation macros */

/* The stack can grow at most MAXINT deep, as co_nlocals and
   co_stacksize are ints. */
#define STACK_LEVEL()     ((int)(stack_pointer - f->f_valuestack))
#define EMPTY()           (STACK_LEVEL() == 0)
#define TOP()             (stack_pointer[-1])
#define SECOND()          (stack_pointer[-2])
#define THIRD()           (stack_pointer[-3])
#define FOURTH()          (stack_pointer[-4])
#define PEEK(n)           (stack_pointer[-(n)])
#define SET_TOP(v)        (stack_pointer[-1] = (v))
#define SET_SECOND(v)     (stack_pointer[-2] = (v))
#define SET_THIRD(v)      (stack_pointer[-3] = (v))
#define SET_FOURTH(v)     (stack_pointer[-4] = (v))
#define BASIC_STACKADJ(n) (stack_pointer += n)
#define BASIC_PUSH(v)     (*stack_pointer++ = (v))
#define BASIC_POP()       (*--stack_pointer)
#define PUSH(v)                BASIC_PUSH(v)
#define POP()                  BASIC_POP()
#define STACK_GROW(n)          BASIC_STACKADJ(n)
#define STACK_SHRINK(n)        BASIC_STACKADJ(-n)
#define EXT_POP(STACK_POINTER) (*--(STACK_POINTER))

/* Local variable macros */

#define GETLOCAL(i)     (fastlocals[i])

/* The SETLOCAL() macro must not DECREF the local variable in-place and
   then store the new value; it must copy the old value to a temporary
   value, then store the new value, and then DECREF the temporary value.
   This is because it is possible that during the DECREF the frame is
   accessed by other code (e.g. a __del__ method or gc.collect()) and the
   variable would be pointing to already-freed memory. */
#define SETLOCAL(i, value)      do { PyObject *tmp = GETLOCAL(i); \
                                     GETLOCAL(i) = value; \
                                     Py_XDECREF(tmp); } while (0)


#define UNWIND_BLOCK(b) \
    while (STACK_LEVEL() > (b)->b_level) { \
        PyObject *v = POP(); \
        Py_XDECREF(v); \
    }

#define UNWIND_EXCEPT_HANDLER(b) \
    do { \
        PyObject *type, *value, *traceback; \
        _PyErr_StackItem *exc_info; \
        assert(STACK_LEVEL() >= (b)->b_level + 3); \
        while (STACK_LEVEL() > (b)->b_level + 3) { \
            value = POP(); \
            Py_XDECREF(value); \
        } \
        exc_info = tstate->exc_info; \
        type = exc_info->exc_type; \
        value = exc_info->exc_value; \
        traceback = exc_info->exc_traceback; \
        exc_info->exc_type = POP(); \
        exc_info->exc_value = POP(); \
        exc_info->exc_traceback = POP(); \
        Py_XDECREF(type); \
        Py_XDECREF(value); \
        Py_XDECREF(traceback); \
    } while(0)

/* Start of code */

    /* push frame */

    tstate->frame = f;
    co = f->f_code;
    names = co->co_names;
    consts = co->co_consts;
    fastlocals = f->f_localsplus;
    freevars = f->f_localsplus + co->co_nlocals;
    first_instr = (_Py_CODEUNIT *) PyUnicode_AsChar(co->co_code);
    /*
       f->f_lasti refers to the index of the last instruction,
       unless it's -1 in which case next_instr should be first_instr.

       YIELD_FROM sets f_lasti to itself, in order to repeatedly yield
       multiple values.
    */
    assert(f->f_lasti >= -1);
    next_instr = first_instr;
    if (f->f_lasti >= 0) {
        assert(f->f_lasti % sizeof(_Py_CODEUNIT) == 0);
        next_instr += f->f_lasti / sizeof(_Py_CODEUNIT) + 1;
    }
    stack_pointer = f->f_stacktop;
    assert(stack_pointer != NULL);
    f->f_stacktop = NULL;       /* remains NULL unless yield suspends frame */
    f->f_executing = 1;

    if (throwflag) /* support for generator.throw() */
        goto error;

main_loop:
    for (;;) {
        f->f_lasti = INSTR_OFFSET();

        /* Extract opcode and argument */

        NEXTOPARG();
    dispatch_opcode:

        switch (opcode) {

        case NOP: {
            continue;
        }

        case LOAD_FAST: {
            PyObject *value = GETLOCAL(oparg);
            if (value == NULL) {
                format_exc_check_arg(tstate, PyExc_UnboundLocalError,
                                     UNBOUNDLOCAL_ERROR_MSG,
                                     PyTuple_GetItem(co->co_varnames, oparg));
                goto error;
            }
            Py_INCREF(value);
            PUSH(value);
            continue;
        }

        case LOAD_CONST: {
            PyObject *value = GETITEM(consts, oparg);
            Py_INCREF(value);
            PUSH(value);
            continue;
        }

        case STORE_FAST: {
            PyObject *value = POP();
            SETLOCAL(oparg, value);
            continue;
        }

        case POP_TOP: {
            PyObject *value = POP();
            Py_DECREF(value);
            continue;
        }

        case ROT_TWO: {
            PyObject *top = TOP();
            PyObject *second = SECOND();
            SET_TOP(second);
            SET_SECOND(top);
            continue;
        }

        case ROT_THREE: {
            PyObject *top = TOP();
            PyObject *second = SECOND();
            PyObject *third = THIRD();
            SET_TOP(second);
            SET_SECOND(third);
            SET_THIRD(top);
            continue;
        }

        case ROT_FOUR: {
            PyObject *top = TOP();
            PyObject *second = SECOND();
            PyObject *third = THIRD();
            PyObject *fourth = FOURTH();
            SET_TOP(second);
            SET_SECOND(third);
            SET_THIRD(fourth);
            SET_FOURTH(top);
            continue;
        }

        case DUP_TOP: {
            PyObject *top = TOP();
            Py_INCREF(top);
            PUSH(top);
            continue;
        }

        case DUP_TOP_TWO: {
            PyObject *top = TOP();
            PyObject *second = SECOND();
            Py_INCREF(top);
            Py_INCREF(second);
            STACK_GROW(2);
            SET_TOP(top);
            SET_SECOND(second);
            continue;
        }

        case UNARY_POSITIVE: {
            PyObject *value = TOP();
            PyObject *res = PyNumber_Positive(value);
            Py_DECREF(value);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case UNARY_NEGATIVE: {
            PyObject *value = TOP();
            PyObject *res = PyNumber_Negative(value);
            Py_DECREF(value);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case UNARY_NOT: {
            PyObject *value = TOP();
            int err = PyObject_IsTrue(value);
            Py_DECREF(value);
            if (err == 0) {
                Py_INCREF(Py_True);
                SET_TOP(Py_True);
                continue;
            }
            else if (err > 0) {
                Py_INCREF(Py_False);
                SET_TOP(Py_False);
                continue;
            }
            STACK_SHRINK(1);
            goto error;
        }

        case UNARY_INVERT: {
            PyObject *value = TOP();
            PyObject *res = PyNumber_Invert(value);
            Py_DECREF(value);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case BINARY_POWER: {
            PyObject *exp = POP();
            PyObject *base = TOP();
            PyObject *res = PyNumber_Power(base, exp, Py_None);
            Py_DECREF(base);
            Py_DECREF(exp);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case BINARY_MULTIPLY: {
            PyObject *right = POP();
            PyObject *left = TOP();
            PyObject *res = PyNumber_Multiply(left, right);
            Py_DECREF(left);
            Py_DECREF(right);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case BINARY_MATRIX_MULTIPLY: {
            PyObject *right = POP();
            PyObject *left = TOP();
            PyObject *res = PyNumber_MatrixMultiply(left, right);
            Py_DECREF(left);
            Py_DECREF(right);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case BINARY_TRUE_DIVIDE: {
            PyObject *divisor = POP();
            PyObject *dividend = TOP();
            PyObject *quotient = PyNumber_TrueDivide(dividend, divisor);
            Py_DECREF(dividend);
            Py_DECREF(divisor);
            SET_TOP(quotient);
            if (quotient == NULL)
                goto error;
            continue;
        }

        case BINARY_FLOOR_DIVIDE: {
            PyObject *divisor = POP();
            PyObject *dividend = TOP();
            PyObject *quotient = PyNumber_FloorDivide(dividend, divisor);
            Py_DECREF(dividend);
            Py_DECREF(divisor);
            SET_TOP(quotient);
            if (quotient == NULL)
                goto error;
            continue;
        }

        case BINARY_MODULO: {
            PyObject *divisor = POP();
            PyObject *dividend = TOP();
            PyObject *res;
	    res = PyNumber_Remainder(dividend, divisor);
            Py_DECREF(divisor);
            Py_DECREF(dividend);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case BINARY_ADD: {
            PyObject *right = POP();
            PyObject *left = TOP();
            PyObject *sum;
	    sum = PyNumber_Add(left, right);
	    Py_DECREF(left);
            Py_DECREF(right);
            SET_TOP(sum);
            if (sum == NULL)
                goto error;
            continue;
        }

        case BINARY_SUBTRACT: {
            PyObject *right = POP();
            PyObject *left = TOP();
            PyObject *diff = PyNumber_Subtract(left, right);
            Py_DECREF(right);
            Py_DECREF(left);
            SET_TOP(diff);
            if (diff == NULL)
                goto error;
            continue;
        }

        case BINARY_SUBSCR: {
            PyObject *sub = POP();
            PyObject *container = TOP();
            PyObject *res = PyObject_GetItem(container, sub);
            Py_DECREF(container);
            Py_DECREF(sub);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case BINARY_LSHIFT: {
            PyObject *right = POP();
            PyObject *left = TOP();
            PyObject *res = PyNumber_Lshift(left, right);
            Py_DECREF(left);
            Py_DECREF(right);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case BINARY_RSHIFT: {
            PyObject *right = POP();
            PyObject *left = TOP();
            PyObject *res = PyNumber_Rshift(left, right);
            Py_DECREF(left);
            Py_DECREF(right);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case BINARY_AND: {
            PyObject *right = POP();
            PyObject *left = TOP();
            PyObject *res = PyNumber_And(left, right);
            Py_DECREF(left);
            Py_DECREF(right);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case BINARY_XOR: {
            PyObject *right = POP();
            PyObject *left = TOP();
            PyObject *res = PyNumber_Xor(left, right);
            Py_DECREF(left);
            Py_DECREF(right);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case BINARY_OR: {
            PyObject *right = POP();
            PyObject *left = TOP();
            PyObject *res = PyNumber_Or(left, right);
            Py_DECREF(left);
            Py_DECREF(right);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case LIST_APPEND: {
            PyObject *v = POP();
            PyObject *list = PEEK(oparg);
            int err;
            err = PyList_Append(list, v);
            Py_DECREF(v);
            if (err != 0)
                goto error;
            continue;
        }

        case SET_ADD: {
            PyObject *v = POP();
            PyObject *set = PEEK(oparg);
            int err;
            err = PySet_Add(set, v);
            Py_DECREF(v);
            if (err != 0)
                goto error;
            continue;
        }
	  
        case STORE_SUBSCR: {
            PyObject *sub = TOP();
            PyObject *container = SECOND();
            PyObject *v = THIRD();
            int err;
            STACK_SHRINK(3);
            /* container[sub] = v */
            err = PyObject_SetItem(container, sub, v);
            Py_DECREF(v);
            Py_DECREF(container);
            Py_DECREF(sub);
            if (err != 0)
                goto error;
            continue;
        }

        case DELETE_SUBSCR: {
            PyObject *sub = TOP();
            PyObject *container = SECOND();
            int err;
            STACK_SHRINK(2);
            /* del container[sub] */
            err = PyObject_DelItem(container, sub);
            Py_DECREF(container);
            Py_DECREF(sub);
            if (err != 0)
                goto error;
            continue;
        }

        case PRINT_EXPR: {
            _Py_IDENTIFIER(displayhook);
            PyObject *value = POP();
            PyObject *hook = _PySys_GetObjectId(&PyId_displayhook);
            PyObject *res;
            if (hook == NULL) {
                _PyErr_SetString(tstate, PyExc_RuntimeError,
                                 "lost sys.displayhook");
                Py_DECREF(value);
                goto error;
            }
            res = PyObject_CallOneArg(hook, value);
            Py_DECREF(value);
            if (res == NULL)
                goto error;
            Py_DECREF(res);
            continue;
        }

        case RAISE_VARARGS: {
            PyObject *cause = NULL, *exc = NULL;
            switch (oparg) {
            case 2:
                cause = POP(); /* cause */
                /* fall through */
            case 1:
                exc = POP(); /* exc */
                /* fall through */
            case 0:
                if (do_raise(tstate, exc, cause)) {
                    goto exception_unwind;
                }
                break;
            default:
                _PyErr_SetString(tstate, PyExc_SystemError,
                                 "bad RAISE_VARARGS oparg");
                break;
            }
            goto error;
        }

        case RETURN_VALUE: {
            retval = POP();
            assert(f->f_iblock == 0);
            assert(EMPTY());
            goto exiting;
        }

        case YIELD_FROM: {
            PyObject *v = POP();
            PyObject *receiver = TOP();
            int err;
            if (PyGen_CheckExact(receiver)) { // || PyCoro_CheckExact(receiver)) {
                retval = _PyGen_Send(receiver, v);
            } else {
                _Py_IDENTIFIER(send);
                if (v == Py_None)
                    retval = Py_TYPE(receiver)->tp_iternext(receiver);
                else
                    retval = _PyObject_CallMethodIdOneArg(receiver, &PyId_send, v);
            }
            Py_DECREF(v);
            if (retval == NULL) {
                PyObject *val;
                err = _PyGen_FetchStopIterationValue(&val);
                if (err < 0)
                    goto error;
                Py_DECREF(receiver);
                SET_TOP(val);
                continue;
            }
            /* receiver remains on stack, retval is value to be yielded */
            f->f_stacktop = stack_pointer;
            /* and repeat... */
            assert(f->f_lasti >= (int)sizeof(_Py_CODEUNIT));
            f->f_lasti -= sizeof(_Py_CODEUNIT);
            goto exiting;
        }

        case YIELD_VALUE: {
            retval = POP();

            f->f_stacktop = stack_pointer;
            goto exiting;
        }
	  
        case POP_EXCEPT: {
            PyObject *type, *value, *traceback;
            _PyErr_StackItem *exc_info;
            PyTryBlock *b = PyFrame_BlockPop(f);
            if (b->b_type != EXCEPT_HANDLER) {
                _PyErr_SetString(tstate, PyExc_SystemError,
                                 "popped block is not an except handler");
                goto error;
            }
            assert(STACK_LEVEL() >= (b)->b_level + 3 &&
                   STACK_LEVEL() <= (b)->b_level + 4);
            exc_info = tstate->exc_info;
            type = exc_info->exc_type;
            value = exc_info->exc_value;
            traceback = exc_info->exc_traceback;
            exc_info->exc_type = POP();
            exc_info->exc_value = POP();
            exc_info->exc_traceback = POP();
            Py_XDECREF(type);
            Py_XDECREF(value);
            Py_XDECREF(traceback);
            continue;
        }

        case POP_BLOCK: {
            PyFrame_BlockPop(f);
            continue;
        }

        case RERAISE: {
            PyObject *exc = POP();
            PyObject *val = POP();
            PyObject *tb = POP();
            assert(PyExceptionClass_Check(exc));
            _PyErr_Restore(tstate, exc, val, tb);
            goto exception_unwind;
        }

        case LOAD_ASSERTION_ERROR: {
            PyObject *value = PyExc_AssertionError;
            Py_INCREF(value);
            PUSH(value);
            continue;
        }

        case LOAD_BUILD_CLASS: {
            _Py_IDENTIFIER(__build_class__);

            PyObject *bc;
	      {
                PyObject *build_class_str = _PyUnicode_FromId(&PyId___build_class__);
                if (build_class_str == NULL)
                    goto error;
                bc = PyObject_GetItem(f->f_builtins, build_class_str);
                if (bc == NULL) {
                    if (_PyErr_ExceptionMatches(tstate, PyExc_KeyError))
                        _PyErr_SetString(tstate, PyExc_NameError,
                                         "__build_class__ not found");
                    goto error;
                }
            }
            PUSH(bc);
            continue;
        }

        case STORE_NAME: {
            PyObject *name = GETITEM(names, oparg);
            PyObject *v = POP();
            PyObject *ns = f->f_locals;
            int err;
            if (ns == NULL) {
                _PyErr_Format(tstate, PyExc_SystemError,
                              "no locals found when storing %R", name);
                Py_DECREF(v);
                goto error;
            }
	    err = PyObject_SetItem(ns, name, v);
            Py_DECREF(v);
            if (err != 0)
                goto error;
            continue;
        }

        case DELETE_NAME: {
            PyObject *name = GETITEM(names, oparg);
            PyObject *ns = f->f_locals;
            int err;
            if (ns == NULL) {
                _PyErr_Format(tstate, PyExc_SystemError,
                              "no locals when deleting %R", name);
                goto error;
            }
            err = PyObject_DelItem(ns, name);
            if (err != 0) {
                format_exc_check_arg(tstate, PyExc_NameError,
                                     NAME_ERROR_MSG,
                                     name);
                goto error;
            }
            continue;
        }

        case UNPACK_SEQUENCE: {
            PyObject *seq = POP(), *item, **items;
	    if (unpack_iterable(tstate, seq, oparg, -1,
                                       stack_pointer + oparg)) {
                STACK_GROW(oparg);
            } else {
                /* unpack_iterable() raised an exception */
                Py_DECREF(seq);
                goto error;
            }
            Py_DECREF(seq);
            continue;
        }

        case UNPACK_EX: {
            int totalargs = 1 + (oparg & 0xFF) + (oparg >> 8);
            PyObject *seq = POP();

            if (unpack_iterable(tstate, seq, oparg & 0xFF, oparg >> 8,
                                stack_pointer + totalargs)) {
                stack_pointer += totalargs;
            } else {
                Py_DECREF(seq);
                goto error;
            }
            Py_DECREF(seq);
            continue;
        }

        case STORE_ATTR: {
            PyObject *name = GETITEM(names, oparg);
            PyObject *owner = TOP();
            PyObject *v = SECOND();
            int err;
            STACK_SHRINK(2);
            err = PyObject_SetAttr(owner, name, v);
            Py_DECREF(v);
            Py_DECREF(owner);
            if (err != 0)
                goto error;
            continue;
        }

        case DELETE_ATTR: {
            PyObject *name = GETITEM(names, oparg);
            PyObject *owner = POP();
            int err;
            err = PyObject_SetAttr(owner, name, (PyObject *)NULL);
            Py_DECREF(owner);
            if (err != 0)
                goto error;
            continue;
        }

        case STORE_GLOBAL: {
            PyObject *name = GETITEM(names, oparg);
            PyObject *v = POP();
            int err;
            err = PyDict_SetItem(f->f_globals, name, v);
            Py_DECREF(v);
            if (err != 0)
                goto error;
            continue;
        }

        case DELETE_GLOBAL: {
            PyObject *name = GETITEM(names, oparg);
            int err;
            err = PyDict_DelItem(f->f_globals, name);
            if (err != 0) {
                if (_PyErr_ExceptionMatches(tstate, PyExc_KeyError)) {
                    format_exc_check_arg(tstate, PyExc_NameError,
                                         NAME_ERROR_MSG, name);
                }
                goto error;
            }
            continue;
        }

        case LOAD_NAME: {
            PyObject *name = GETITEM(names, oparg);
            PyObject *locals = f->f_locals;
            PyObject *v;
            if (locals == NULL) {
                _PyErr_Format(tstate, PyExc_SystemError,
                              "no locals when loading %R", name);
                goto error;
            }
	    {
                v = PyObject_GetItem(locals, name);
                if (v == NULL) {
                    if (!_PyErr_ExceptionMatches(tstate, PyExc_KeyError))
                        goto error;
                    _PyErr_Clear(tstate);
                }
            }
            if (v == NULL) {
                v = PyDict_GetItemWithError(f->f_globals, name);
                if (v != NULL) {
                    Py_INCREF(v);
                }
                else if (_PyErr_Occurred(tstate)) {
                    goto error;
                }
                else {
		  {
		    v = PyObject_GetItem(f->f_builtins, name);
		    if (v == NULL) {
		      if (_PyErr_ExceptionMatches(tstate, PyExc_KeyError)) {
			format_exc_check_arg(
					     tstate, PyExc_NameError,
					     NAME_ERROR_MSG, name);
		      }
		      goto error;
		    }
		  }
                }
            }
            PUSH(v);
            continue;
        }

        case LOAD_GLOBAL: {
            PyObject *name;
            PyObject *v;
	      {
                /* Slow-path if globals or builtins is not a dict */

                /* namespace 1: globals */
                name = GETITEM(names, oparg);
                v = PyObject_GetItem(f->f_globals, name);
                if (v == NULL) {
                    if (!_PyErr_ExceptionMatches(tstate, PyExc_KeyError)) {
                        goto error;
                    }
                    _PyErr_Clear(tstate);

                    /* namespace 2: builtins */
                    v = PyObject_GetItem(f->f_builtins, name);
                    if (v == NULL) {
                        if (_PyErr_ExceptionMatches(tstate, PyExc_KeyError)) {
                            format_exc_check_arg(
                                        tstate, PyExc_NameError,
                                        NAME_ERROR_MSG, name);
                        }
                        goto error;
                    }
                }
            }
            PUSH(v);
            continue;
        }

        case DELETE_FAST: {
            PyObject *v = GETLOCAL(oparg);
            if (v != NULL) {
                SETLOCAL(oparg, NULL);
                continue;
            }
            format_exc_check_arg(
                tstate, PyExc_UnboundLocalError,
                UNBOUNDLOCAL_ERROR_MSG,
                PyTuple_GetItem(co->co_varnames, oparg)
                );
            goto error;
        }

        case DELETE_DEREF: {
            PyObject *cell = freevars[oparg];
            PyObject *oldobj = PyCell_Get(cell);
            if (oldobj != NULL) {
                PyCell_Set(cell, NULL);
                Py_DECREF(oldobj);
                continue;
            }
            format_exc_unbound(tstate, co, oparg);
            goto error;
        }

        case LOAD_CLOSURE: {
            PyObject *cell = freevars[oparg];
            Py_INCREF(cell);
            PUSH(cell);
            continue;
        }

        case LOAD_CLASSDEREF: {
            PyObject *name, *value, *locals = f->f_locals;
            Py_ssize_t idx;
            assert(locals);
            assert(oparg >= PyTuple_Size(co->co_cellvars));
            idx = oparg - PyTuple_Size(co->co_cellvars);
            assert(idx >= 0 && idx < PyTuple_Size(co->co_freevars));
            name = PyTuple_GetItem(co->co_freevars, idx);
	      {
                value = PyObject_GetItem(locals, name);
                if (value == NULL) {
                    if (!_PyErr_ExceptionMatches(tstate, PyExc_KeyError)) {
                        goto error;
                    }
                    _PyErr_Clear(tstate);
                }
            }
            if (!value) {
                PyObject *cell = freevars[oparg];
                value = PyCell_Get(cell);
                if (value == NULL) {
                    format_exc_unbound(tstate, co, oparg);
                    goto error;
                }
                Py_INCREF(value);
            }
            PUSH(value);
            continue;
        }

        case LOAD_DEREF: {
            PyObject *cell = freevars[oparg];
            PyObject *value = PyCell_Get(cell);
            if (value == NULL) {
                format_exc_unbound(tstate, co, oparg);
                goto error;
            }
            Py_INCREF(value);
            PUSH(value);
            continue;
        }

        case STORE_DEREF: {
            PyObject *v = POP();
            PyObject *cell = freevars[oparg];
            PyObject *oldobj = PyCell_Get(cell);
            PyCell_Set(cell, v);
            Py_XDECREF(oldobj);
            continue;
        }

        case BUILD_STRING: {
            PyObject *str;
            PyObject *empty = PyUnicode_New(0);
            if (empty == NULL) {
                goto error;
            }
            str = _PyUnicode_JoinArray(empty, stack_pointer - oparg, oparg);
            Py_DECREF(empty);
            if (str == NULL)
                goto error;
            while (--oparg >= 0) {
                PyObject *item = POP();
                Py_DECREF(item);
            }
            PUSH(str);
            continue;
        }

        case BUILD_TUPLE: {
            PyObject *tup = PyTuple_New(oparg);
            if (tup == NULL)
                goto error;
            while (--oparg >= 0) {
                PyObject *item = POP();
                PyTuple_InitItem(tup, oparg, item);
            }
            PUSH(tup);
            continue;
        }

        case BUILD_LIST: {
            PyObject *list =  PyList_New(oparg);
            if (list == NULL)
                goto error;
            while (--oparg >= 0) {
                PyObject *item = POP();
                PyList_InitItem(list, oparg, item);
            }
            PUSH(list);
            continue;
        }

        case LIST_TO_TUPLE: {
            PyObject *list = POP();
            PyObject *tuple = PyList_AsTuple(list);
            Py_DECREF(list);
            if (tuple == NULL) {
                goto error;
            }
            PUSH(tuple);
            continue;
        }

        case LIST_EXTEND: {
            PyObject *iterable = POP();
            PyObject *list = PEEK(oparg);
            PyObject *none_val = PyList_Extend(list, iterable);
            if (none_val == NULL) {
                if (_PyErr_ExceptionMatches(tstate, PyExc_TypeError) &&
                   (Py_TYPE(iterable)->tp_iter == NULL && !PySequence_Check(iterable)))
                {
                    _PyErr_Clear(tstate);
                    _PyErr_Format(tstate, PyExc_TypeError,
                          "Value after * must be an iterable, not %.200s",
                          Py_TYPE(iterable)->tp_name);
                }
                Py_DECREF(iterable);
                goto error;
            }
            Py_DECREF(none_val);
            Py_DECREF(iterable);
            continue;
        }

        case SET_UPDATE: {
            PyObject *iterable = POP();
            PyObject *set = PEEK(oparg);
            int err = PySet_Update(set, iterable);
            Py_DECREF(iterable);
            if (err < 0) {
                goto error;
            }
            continue;
        }

        case BUILD_SET: {
            PyObject *set = PySet_New(NULL);
            int err = 0;
            int i;
            if (set == NULL)
                goto error;
            for (i = oparg; i > 0; i--) {
                PyObject *item = PEEK(i);
                if (err == 0)
                    err = PySet_Add(set, item);
                Py_DECREF(item);
            }
            STACK_SHRINK(oparg);
            if (err != 0) {
                Py_DECREF(set);
                goto error;
            }
            PUSH(set);
            continue;
        }

        case BUILD_MAP: {
            Py_ssize_t i;
            PyObject *map = _PyDict_NewPresized((Py_ssize_t)oparg);
            if (map == NULL)
                goto error;
            for (i = oparg; i > 0; i--) {
                int err;
                PyObject *key = PEEK(2*i);
                PyObject *value = PEEK(2*i - 1);
                err = PyDict_SetItem(map, key, value);
                if (err != 0) {
                    Py_DECREF(map);
                    goto error;
                }
            }

            while (oparg--) {
                Py_DECREF(POP());
                Py_DECREF(POP());
            }
            PUSH(map);
            continue;
        }

        case BUILD_CONST_KEY_MAP: {
            Py_ssize_t i;
            PyObject *map;
            PyObject *keys = TOP();
            if (!PyTuple_CheckExact(keys) ||
                PyTuple_Size(keys) != (Py_ssize_t)oparg) {
                _PyErr_SetString(tstate, PyExc_SystemError,
                                 "bad BUILD_CONST_KEY_MAP keys argument");
                goto error;
            }
            map = _PyDict_NewPresized((Py_ssize_t)oparg);
            if (map == NULL) {
                goto error;
            }
            for (i = oparg; i > 0; i--) {
                int err;
                PyObject *key = PyTuple_GetItem(keys, oparg - i);
                PyObject *value = PEEK(i + 1);
                err = PyDict_SetItem(map, key, value);
                if (err != 0) {
                    Py_DECREF(map);
                    goto error;
                }
            }

            Py_DECREF(POP());
            while (oparg--) {
                Py_DECREF(POP());
            }
            PUSH(map);
            continue;
        }

        case DICT_UPDATE: {
            PyObject *update = POP();
            PyObject *dict = PEEK(oparg);
            if (PyDict_Update(dict, update) < 0) {
                if (_PyErr_ExceptionMatches(tstate, PyExc_AttributeError)) {
                    _PyErr_Format(tstate, PyExc_TypeError,
                                    "'%.200s' object is not a mapping",
                                    Py_TYPE(update)->tp_name);
                }
                Py_DECREF(update);
                goto error;
            }
            Py_DECREF(update);
            continue;
        }

        case DICT_MERGE: {
            PyObject *update = POP();
            PyObject *dict = PEEK(oparg);

            if (PyDict_Merge(dict, update, 2) < 0) {
                format_kwargs_error(tstate, PEEK(2 + oparg), update);
                Py_DECREF(update);
                goto error;
            }
            Py_DECREF(update);
            continue;
        }

        case MAP_ADD: {
            PyObject *value = TOP();
            PyObject *key = SECOND();
            PyObject *map;
            int err;
            STACK_SHRINK(2);
            map = PEEK(oparg);                      /* dict */
            assert(PyDict_CheckExact(map));
            err = PyDict_SetItem(map, key, value);  /* map[key] = value */
            Py_DECREF(value);
            Py_DECREF(key);
            if (err != 0)
                goto error;
            continue;
        }

        case LOAD_ATTR: {
            PyObject *name = GETITEM(names, oparg);
            PyObject *owner = TOP();
            PyObject *res = PyObject_GetAttr(owner, name);
            Py_DECREF(owner);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case COMPARE_OP: {
            assert(oparg <= Py_GE);
            PyObject *right = POP();
            PyObject *left = TOP();
            PyObject *res = PyObject_RichCompare(left, right, oparg);
            SET_TOP(res);
            Py_DECREF(left);
            Py_DECREF(right);
            if (res == NULL)
                goto error;
            continue;
        }

        case IS_OP: {
            PyObject *right = POP();
            PyObject *left = TOP();
            int res = (left == right)^oparg;
            PyObject *b = res ? Py_True : Py_False;
            Py_INCREF(b);
            SET_TOP(b);
            Py_DECREF(left);
            Py_DECREF(right);
            continue;
        }

        case CONTAINS_OP: {
            PyObject *right = POP();
            PyObject *left = POP();
            int res = PySequence_Contains(right, left);
            Py_DECREF(left);
            Py_DECREF(right);
            if (res < 0) {
                goto error;
            }
            PyObject *b = (res^oparg) ? Py_True : Py_False;
            Py_INCREF(b);
            PUSH(b);
            continue;
        }

#define CANNOT_CATCH_MSG "catching classes that do not inherit from "\
                         "BaseException is not allowed"

        case JUMP_IF_NOT_EXC_MATCH: {
            PyObject *right = POP();
            PyObject *left = POP();
            if (PyTuple_Check(right)) {
                Py_ssize_t i, length;
                length = PyTuple_Size(right);
                for (i = 0; i < length; i++) {
                    PyObject *exc = PyTuple_GetItem(right, i);
                    if (!PyExceptionClass_Check(exc)) {
                        _PyErr_SetString(tstate, PyExc_TypeError,
                                        CANNOT_CATCH_MSG);
                        Py_DECREF(left);
                        Py_DECREF(right);
                        goto error;
                    }
                }
            }
            else {
                if (!PyExceptionClass_Check(right)) {
                    _PyErr_SetString(tstate, PyExc_TypeError,
                                    CANNOT_CATCH_MSG);
                    Py_DECREF(left);
                    Py_DECREF(right);
                    goto error;
                }
            }
            int res = PyErr_GivenExceptionMatches(left, right);
            Py_DECREF(left);
            Py_DECREF(right);
            if (res > 0) {
                /* Exception matches -- Do nothing */;
            }
            else if (res == 0) {
                JUMPTO(oparg);
            }
            else {
                goto error;
            }
            continue;
        }

        case IMPORT_NAME: {
            PyObject *name = GETITEM(names, oparg);
            PyObject *fromlist = POP();
            PyObject *level = TOP();
            PyObject *res;
            res = import_name(tstate, f, name, fromlist, level);
            Py_DECREF(level);
            Py_DECREF(fromlist);
            SET_TOP(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case IMPORT_STAR: {
            PyObject *from = POP(), *locals;
            int err;
            if (PyFrame_FastToLocalsWithError(f) < 0) {
                Py_DECREF(from);
                goto error;
            }

            locals = f->f_locals;
            if (locals == NULL) {
                _PyErr_SetString(tstate, PyExc_SystemError,
                                 "no locals found during 'import *'");
                Py_DECREF(from);
                goto error;
            }
            err = import_all_from(tstate, locals, from);
            PyFrame_LocalsToFast(f, 0);
            Py_DECREF(from);
            if (err != 0)
                goto error;
            continue;
        }

        case IMPORT_FROM: {
            PyObject *name = GETITEM(names, oparg);
            PyObject *from = TOP();
            PyObject *res;
            res = import_from(tstate, from, name);
            PUSH(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case JUMP_FORWARD: {
            JUMPBY(oparg);
            continue;
        }

        case POP_JUMP_IF_FALSE: {
            PyObject *cond = POP();
            int err;
            if (cond == Py_True) {
                Py_DECREF(cond);
                continue;
            }
            if (cond == Py_False) {
                Py_DECREF(cond);
                JUMPTO(oparg);
                continue;
            }
            err = PyObject_IsTrue(cond);
            Py_DECREF(cond);
            if (err > 0)
                ;
            else if (err == 0)
                JUMPTO(oparg);
            else
                goto error;
            continue;
        }

        case POP_JUMP_IF_TRUE: {
            PyObject *cond = POP();
            int err;
            if (cond == Py_False) {
                Py_DECREF(cond);
                continue;
            }
            if (cond == Py_True) {
                Py_DECREF(cond);
                JUMPTO(oparg);
                continue;
            }
            err = PyObject_IsTrue(cond);
            Py_DECREF(cond);
            if (err > 0) {
                JUMPTO(oparg);
            }
            else if (err == 0)
                ;
            else
                goto error;
            continue;
        }

        case JUMP_IF_FALSE_OR_POP: {
            PyObject *cond = TOP();
            int err;
            if (cond == Py_True) {
                STACK_SHRINK(1);
                Py_DECREF(cond);
                continue;
            }
            if (cond == Py_False) {
                JUMPTO(oparg);
                continue;
            }
            err = PyObject_IsTrue(cond);
            if (err > 0) {
                STACK_SHRINK(1);
                Py_DECREF(cond);
            }
            else if (err == 0)
                JUMPTO(oparg);
            else
                goto error;
            continue;
        }

        case JUMP_IF_TRUE_OR_POP: {
            PyObject *cond = TOP();
            int err;
            if (cond == Py_False) {
                STACK_SHRINK(1);
                Py_DECREF(cond);
                continue;
            }
            if (cond == Py_True) {
                JUMPTO(oparg);
                continue;
            }
            err = PyObject_IsTrue(cond);
            if (err > 0) {
                JUMPTO(oparg);
            }
            else if (err == 0) {
                STACK_SHRINK(1);
                Py_DECREF(cond);
            }
            else
                goto error;
            continue;
        }

        case JUMP_ABSOLUTE: {
            JUMPTO(oparg);
            continue;
        }

        case GET_ITER: {
            /* before: [obj]; after [getiter(obj)] */
            PyObject *iterable = TOP();
            PyObject *iter = PyObject_GetIter(iterable);
            Py_DECREF(iterable);
            SET_TOP(iter);
            if (iter == NULL)
                goto error;
            continue;
        }

        case GET_YIELD_FROM_ITER: {
            /* before: [obj]; after [getiter(obj)] */
            PyObject *iterable = TOP();
            PyObject *iter;

	      if (!PyGen_CheckExact(iterable)) {
                /* `iterable` is not a generator. */
                iter = PyObject_GetIter(iterable);
                Py_DECREF(iterable);
                SET_TOP(iter);
                if (iter == NULL)
                    goto error;
            }
            continue;
        }
	  
        case FOR_ITER: {
            /* before: [iter]; after: [iter, iter()] *or* [] */
            PyObject *iter = TOP();
            PyObject *next = (*Py_TYPE(iter)->tp_iternext)(iter);
            if (next != NULL) {
                PUSH(next);
                continue;
            }
            if (_PyErr_Occurred(tstate)) {
                if (!_PyErr_ExceptionMatches(tstate, PyExc_StopIteration)) {
                    goto error;
                }
                _PyErr_Clear(tstate);
            }
            /* iterator ended normally */
            STACK_SHRINK(1);
            Py_DECREF(iter);
            JUMPBY(oparg);
            continue;
        }

        case SETUP_FINALLY: {
            PyFrame_BlockSetup(f, SETUP_FINALLY, INSTR_OFFSET() + oparg,
                               STACK_LEVEL());
            continue;
        }

        case SETUP_WITH: {
            _Py_IDENTIFIER(__enter__);
            _Py_IDENTIFIER(__exit__);
            PyObject *mgr = TOP();
            PyObject *enter = special_lookup(tstate, mgr, &PyId___enter__);
            PyObject *res;
            if (enter == NULL) {
                goto error;
            }
            PyObject *exit = special_lookup(tstate, mgr, &PyId___exit__);
            if (exit == NULL) {
                Py_DECREF(enter);
                goto error;
            }
            SET_TOP(exit);
            Py_DECREF(mgr);
            res = _PyObject_CallNoArg(enter);
            Py_DECREF(enter);
            if (res == NULL)
                goto error;
            /* Setup the finally block before pushing the result
               of __enter__ on the stack. */
            PyFrame_BlockSetup(f, SETUP_FINALLY, INSTR_OFFSET() + oparg,
                               STACK_LEVEL());

            PUSH(res);
            continue;
        }

        case WITH_EXCEPT_START: {
            /* At the top of the stack are 7 values:
               - (TOP, SECOND, THIRD) = exc_info()
               - (FOURTH, FIFTH, SIXTH) = previous exception for EXCEPT_HANDLER
               - SEVENTH: the context.__exit__ bound method
               We call SEVENTH(TOP, SECOND, THIRD).
               Then we push again the TOP exception and the __exit__
               return value.
            */
            PyObject *exit_func;
            PyObject *exc, *val, *tb, *res;

            exc = TOP();
            val = SECOND();
            tb = THIRD();
            assert(exc != Py_None);
            assert(!PyLong_Check(exc));
            exit_func = PEEK(7);
            PyObject *stack[4] = {NULL, exc, val, tb};
            res = PyObject_Vectorcall(exit_func, stack + 1,
                    3 | PY_VECTORCALL_ARGUMENTS_OFFSET, NULL);
            if (res == NULL)
                goto error;

            PUSH(res);
            continue;
        }

        case LOAD_METHOD: {
            /* Designed to work in tandem with CALL_METHOD. */
            PyObject *name = GETITEM(names, oparg);
            PyObject *obj = TOP();
            PyObject *meth = NULL;

            int meth_found = _PyObject_GetMethod(obj, name, &meth);

            if (meth == NULL) {
                /* Most likely attribute wasn't found. */
                goto error;
            }

            if (meth_found) {
                /* We can bypass temporary bound method object.
                   meth is unbound method and obj is self.

                   meth | self | arg1 | ... | argN
                 */
                SET_TOP(meth);
                PUSH(obj);  // self
            }
            else {
                /* meth is not an unbound method (but a regular attr, or
                   something was returned by a descriptor protocol).  Set
                   the second element of the stack to NULL, to signal
                   CALL_METHOD that it's not a method call.

                   NULL | meth | arg1 | ... | argN
                */
                SET_TOP(NULL);
                Py_DECREF(obj);
                PUSH(meth);
            }
            continue;
        }

        case CALL_METHOD: {
            /* Designed to work in tamdem with LOAD_METHOD. */
            PyObject **sp, *res, *meth;

            sp = stack_pointer;

            meth = PEEK(oparg + 2);
            if (meth == NULL) {
                /* `meth` is NULL when LOAD_METHOD thinks that it's not
                   a method call.

                   Stack layout:

                       ... | NULL | callable | arg1 | ... | argN
                                                            ^- TOP()
                                               ^- (-oparg)
                                    ^- (-oparg-1)
                             ^- (-oparg-2)

                   `callable` will be POPed by call_function.
                   NULL will will be POPed manually later.
                */
                res = call_function(tstate, &sp, oparg, NULL);
                stack_pointer = sp;
                (void)POP(); /* POP the NULL. */
            }
            else {
                /* This is a method call.  Stack layout:

                     ... | method | self | arg1 | ... | argN
                                                        ^- TOP()
                                           ^- (-oparg)
                                    ^- (-oparg-1)
                           ^- (-oparg-2)

                  `self` and `method` will be POPed by call_function.
                  We'll be passing `oparg + 1` to call_function, to
                  make it accept the `self` as a first argument.
                */
                res = call_function(tstate, &sp, oparg + 1, NULL);
                stack_pointer = sp;
            }

            PUSH(res);
            if (res == NULL)
                goto error;
            continue;
        }

        case CALL_FUNCTION: {
            PyObject **sp, *res;
            sp = stack_pointer;
            res = call_function(tstate, &sp, oparg, NULL);
            stack_pointer = sp;
            PUSH(res);
            if (res == NULL) {
                goto error;
            }
            continue;
        }

        case CALL_FUNCTION_KW: {
            PyObject **sp, *res, *names;

            names = POP();
            assert(PyTuple_Check(names));
            assert(PyTuple_Size(names) <= oparg);
            /* We assume without checking that names contains only strings */
            sp = stack_pointer;
            res = call_function(tstate, &sp, oparg, names);
            stack_pointer = sp;
            PUSH(res);
            Py_DECREF(names);

            if (res == NULL) {
                goto error;
            }
            continue;
        }

        case CALL_FUNCTION_EX: {
            PyObject *func, *callargs, *kwargs = NULL, *result;
            if (oparg & 0x01) {
                kwargs = POP();
                if (!PyDict_CheckExact(kwargs)) {
                    PyObject *d = PyDict_New();
                    if (d == NULL)
                        goto error;
                    if (PyDict_Merge(d, kwargs, 2) < 0) {
                        Py_DECREF(d);
                        format_kwargs_error(tstate, SECOND(), kwargs);
                        Py_DECREF(kwargs);
                        goto error;
                    }
                    Py_DECREF(kwargs);
                    kwargs = d;
                }
                assert(PyDict_CheckExact(kwargs));
            }
            callargs = POP();
            func = TOP();
            if (!PyTuple_CheckExact(callargs)) {
                if (check_args_iterable(tstate, func, callargs) < 0) {
                    Py_DECREF(callargs);
                    goto error;
                }
                Py_SETREF(callargs, PySequence_Tuple(callargs));
                if (callargs == NULL) {
                    goto error;
                }
            }
            assert(PyTuple_CheckExact(callargs));

            result = do_call_core(tstate, func, callargs, kwargs);
            Py_DECREF(func);
            Py_DECREF(callargs);
            Py_XDECREF(kwargs);

            SET_TOP(result);
            if (result == NULL) {
                goto error;
            }
            continue;
        }

        case MAKE_FUNCTION: {
            PyObject *qualname = POP();
            PyObject *codeobj = POP();
            PyFunctionObject *func = (PyFunctionObject *)
                PyFunction_NewWithQualName(codeobj, f->f_globals, qualname);

            Py_DECREF(codeobj);
            Py_DECREF(qualname);
            if (func == NULL) {
                goto error;
            }

            if (oparg & 0x08) {
                assert(PyTuple_CheckExact(TOP()));
                func ->func_closure = POP();
            }
            if (oparg & 0x02) {
                assert(PyDict_CheckExact(TOP()));
                func->func_kwdefaults = POP();
            }
            if (oparg & 0x01) {
                assert(PyTuple_CheckExact(TOP()));
                func->func_defaults = POP();
            }

            PUSH((PyObject *)func);
            continue;
        }

        case BUILD_SLICE: {
            PyObject *start, *stop, *step, *slice;
            if (oparg == 3)
                step = POP();
            else
                step = NULL;
            stop = POP();
            start = TOP();
            slice = PySlice_New(start, stop, step);
            Py_DECREF(start);
            Py_DECREF(stop);
            Py_XDECREF(step);
            SET_TOP(slice);
            if (slice == NULL)
                goto error;
            continue;
        }

        case FORMAT_VALUE: {
            /* Handles f-string value formatting. */
            PyObject *result;
            PyObject *fmt_spec;
            PyObject *value;
            PyObject *(*conv_fn)(PyObject *);
            int which_conversion = oparg & FVC_MASK;
            int have_fmt_spec = (oparg & FVS_MASK) == FVS_HAVE_SPEC;

            fmt_spec = have_fmt_spec ? POP() : NULL;
            value = POP();

            /* See if any conversion is specified. */
            switch (which_conversion) {
            case FVC_NONE:  conv_fn = NULL;           break;
            case FVC_STR:   conv_fn = PyObject_Str;   break;
            case FVC_REPR:  conv_fn = PyObject_Repr;  break;
            case FVC_ASCII: conv_fn = PyObject_ASCII; break;
            default:
                _PyErr_Format(tstate, PyExc_SystemError,
                              "unexpected conversion flag %d",
                              which_conversion);
                goto error;
            }

            /* If there's a conversion function, call it and replace
               value with that result. Otherwise, just use value,
               without conversion. */
            if (conv_fn != NULL) {
                result = conv_fn(value);
                Py_DECREF(value);
                if (result == NULL) {
                    Py_XDECREF(fmt_spec);
                    goto error;
                }
                value = result;
            }

            /* If value is a unicode object, and there's no fmt_spec,
               then we know the result of format(value) is value
               itself. In that case, skip calling format(). I plan to
               move this optimization in to PyObject_Format()
               itself. */
            if (PyUnicode_CheckExact(value) && fmt_spec == NULL) {
                /* Do nothing, just transfer ownership to result. */
                result = value;
            } else {
                /* Actually call format(). */
                result = PyObject_Format(value, fmt_spec);
                Py_DECREF(value);
                Py_XDECREF(fmt_spec);
                if (result == NULL) {
                    goto error;
                }
            }

            PUSH(result);
            continue;
        }

        case EXTENDED_ARG: {
            int oldoparg = oparg;
            NEXTOPARG();
            oparg |= oldoparg << 8;
            goto dispatch_opcode;
        }

        default:
            fprintf(stderr,
                "XXX lineno: %d, opcode: %d\n",
                PyFrame_GetLineNumber(f),
                opcode);
            _PyErr_SetString(tstate, PyExc_SystemError, "unknown opcode");
            goto error;

        } /* switch */

        /* This should never be reached. Every opcode should end with DISPATCH()
           or goto error. */
        Py_UNREACHABLE();

error:
        /* Double-check exception status. */
#ifdef NDEBUG
        if (!_PyErr_Occurred(tstate)) {
            _PyErr_SetString(tstate, PyExc_SystemError,
                             "error return without exception set");
        }
#else
        assert(_PyErr_Occurred(tstate));
#endif

        /* Log traceback info. */
        PyTraceBack_Here(f);

exception_unwind:
        /* Unwind stacks if an exception occurred */
        while (f->f_iblock > 0) {
            /* Pop the current block. */
            PyTryBlock *b = &f->f_blockstack[--f->f_iblock];

            if (b->b_type == EXCEPT_HANDLER) {
                UNWIND_EXCEPT_HANDLER(b);
                continue;
            }
            UNWIND_BLOCK(b);
            if (b->b_type == SETUP_FINALLY) {
                PyObject *exc, *val, *tb;
                int handler = b->b_handler;
                _PyErr_StackItem *exc_info = tstate->exc_info;
                /* Beware, this invalidates all b->b_* fields */
                PyFrame_BlockSetup(f, EXCEPT_HANDLER, -1, STACK_LEVEL());
                PUSH(exc_info->exc_traceback);
                PUSH(exc_info->exc_value);
                if (exc_info->exc_type != NULL) {
                    PUSH(exc_info->exc_type);
                }
                else {
                    Py_INCREF(Py_None);
                    PUSH(Py_None);
                }
                _PyErr_Fetch(tstate, &exc, &val, &tb);
                /* Make the raw exception data
                   available to the handler,
                   so a program can emulate the
                   Python main loop. */
                _PyErr_NormalizeException(tstate, &exc, &val, &tb);
                if (tb != NULL)
                    PyException_SetTraceback(val, tb);
                else
                    PyException_SetTraceback(val, Py_None);
                Py_INCREF(exc);
                exc_info->exc_type = exc;
                Py_INCREF(val);
                exc_info->exc_value = val;
                exc_info->exc_traceback = tb;
                if (tb == NULL)
                    tb = Py_None;
                Py_INCREF(tb);
                PUSH(tb);
                PUSH(val);
                PUSH(exc);
                JUMPTO(handler);
                /* Resume normal execution */
		
                goto main_loop;
            }
        } /* unwind stack */

        /* End the loop as we still have an error */
        break;
    } /* main loop */

    assert(retval == NULL);
    assert(_PyErr_Occurred(tstate));

    /* Pop remaining stack entries. */
    while (!EMPTY()) {
        PyObject *o = POP();
        Py_XDECREF(o);
    }

exiting:

    /* pop frame */
    f->f_executing = 0;
    tstate->frame = f->f_back;

    return _Py_CheckFunctionResult(tstate, NULL, retval, __func__);
}

static void
format_missing(PyThreadState *tstate, const char *kind,
               PyCodeObject *co, PyObject *names, PyObject *qualname)
{
    int err;
    Py_ssize_t len = PyList_Size(names);
    PyObject *name_str, *comma, *tail, *tmp;

    assert(PyList_CheckExact(names));
    assert(len >= 1);
    /* Deal with the joys of natural language. */
    switch (len) {
    case 1:
        name_str = PyList_GetItem(names, 0);
        Py_INCREF(name_str);
        break;
    case 2:
        name_str = PyUnicode_FromFormat("%U and %U",
                                        PyList_GetItem(names, len - 2),
                                        PyList_GetItem(names, len - 1));
        break;
    default:
        tail = PyUnicode_FromFormat(", %U, and %U",
                                    PyList_GetItem(names, len - 2),
                                    PyList_GetItem(names, len - 1));
        if (tail == NULL)
            return;
        /* Chop off the last two objects in the list. This shouldn't actually
           fail, but we can't be too careful. */
        err = PyList_SetSlice(names, len - 2, len, NULL);
        if (err == -1) {
            Py_DECREF(tail);
            return;
        }
        /* Stitch everything up into a nice comma-separated list. */
        comma = PyUnicode_FromString(", ");
        if (comma == NULL) {
            Py_DECREF(tail);
            return;
        }
        tmp = PyUnicode_Join(comma, names);
        Py_DECREF(comma);
        if (tmp == NULL) {
            Py_DECREF(tail);
            return;
        }
        name_str = PyUnicode_Concat(tmp, tail);
        Py_DECREF(tmp);
        Py_DECREF(tail);
        break;
    }
    if (name_str == NULL)
        return;
    _PyErr_Format(tstate, PyExc_TypeError,
                  "%U() missing %i required %s argument%s: %U",
                  qualname,
                  len,
                  kind,
                  len == 1 ? "" : "s",
                  name_str);
    Py_DECREF(name_str);
}

static void
missing_arguments(PyThreadState *tstate, PyCodeObject *co,
                  Py_ssize_t missing, Py_ssize_t defcount,
                  PyObject **fastlocals, PyObject *qualname)
{
    Py_ssize_t i, j = 0;
    Py_ssize_t start, end;
    int positional = (defcount != -1);
    const char *kind = positional ? "positional" : "keyword-only";
    PyObject *missing_names;

    /* Compute the names of the arguments that are missing. */
    missing_names = PyList_New(missing);
    if (missing_names == NULL)
        return;
    if (positional) {
        start = 0;
        end = co->co_argcount - defcount;
    }
    else {
        start = co->co_argcount;
        end = start + co->co_kwonlyargcount;
    }
    for (i = start; i < end; i++) {
        if (GETLOCAL(i) == NULL) {
            PyObject *raw = PyTuple_GetItem(co->co_varnames, i);
            PyObject *name = PyObject_Repr(raw);
            if (name == NULL) {
                Py_DECREF(missing_names);
                return;
            }
            PyList_InitItem(missing_names, j++, name);
        }
    }
    assert(j == missing);
    format_missing(tstate, kind, co, missing_names, qualname);
    Py_DECREF(missing_names);
}

static void
too_many_positional(PyThreadState *tstate, PyCodeObject *co,
                    Py_ssize_t given, Py_ssize_t defcount,
                    PyObject **fastlocals, PyObject *qualname)
{
    int plural;
    Py_ssize_t kwonly_given = 0;
    Py_ssize_t i;
    PyObject *sig, *kwonly_sig;
    Py_ssize_t co_argcount = co->co_argcount;

    assert((co->co_flags & CO_VARARGS) == 0);
    /* Count missing keyword-only args. */
    for (i = co_argcount; i < co_argcount + co->co_kwonlyargcount; i++) {
        if (GETLOCAL(i) != NULL) {
            kwonly_given++;
        }
    }
    if (defcount) {
        Py_ssize_t atleast = co_argcount - defcount;
        plural = 1;
        sig = PyUnicode_FromFormat("from %zd to %zd", atleast, co_argcount);
    }
    else {
        plural = (co_argcount != 1);
        sig = PyUnicode_FromFormat("%zd", co_argcount);
    }
    if (sig == NULL)
        return;
    if (kwonly_given) {
        const char *format = " positional argument%s (and %zd keyword-only argument%s)";
        kwonly_sig = PyUnicode_FromFormat(format,
                                          given != 1 ? "s" : "",
                                          kwonly_given,
                                          kwonly_given != 1 ? "s" : "");
        if (kwonly_sig == NULL) {
            Py_DECREF(sig);
            return;
        }
    }
    else {
        /* This will not fail. */
        kwonly_sig = PyUnicode_FromString("");
        assert(kwonly_sig != NULL);
    }
    _PyErr_Format(tstate, PyExc_TypeError,
                  "%U() takes %U positional argument%s but %zd%U %s given",
                  qualname,
                  sig,
                  plural ? "s" : "",
                  given,
                  kwonly_sig,
                  given == 1 && !kwonly_given ? "was" : "were");
    Py_DECREF(sig);
    Py_DECREF(kwonly_sig);
}

static int
positional_only_passed_as_keyword(PyThreadState *tstate, PyCodeObject *co,
                                  Py_ssize_t kwcount, PyObject* const* kwnames,
                                  PyObject *qualname)
{
    int posonly_conflicts = 0;
    PyObject* posonly_names = PyList_New(0);

    for(int k=0; k < co->co_posonlyargcount; k++){
        PyObject* posonly_name = PyTuple_GetItem(co->co_varnames, k);

        for (int k2=0; k2<kwcount; k2++){
            /* Compare the pointers first and fallback to PyObject_RichCompareBool*/
            PyObject* kwname = kwnames[k2];
            if (kwname == posonly_name){
                if(PyList_Append(posonly_names, kwname) != 0) {
                    goto fail;
                }
                posonly_conflicts++;
                continue;
            }

            int cmp = PyObject_RichCompareBool(posonly_name, kwname, Py_EQ);

            if ( cmp > 0) {
                if(PyList_Append(posonly_names, kwname) != 0) {
                    goto fail;
                }
                posonly_conflicts++;
            } else if (cmp < 0) {
                goto fail;
            }

        }
    }
    if (posonly_conflicts) {
        PyObject* comma = PyUnicode_FromString(", ");
        if (comma == NULL) {
            goto fail;
        }
        PyObject* error_names = PyUnicode_Join(comma, posonly_names);
        Py_DECREF(comma);
        if (error_names == NULL) {
            goto fail;
        }
        _PyErr_Format(tstate, PyExc_TypeError,
                      "%U() got some positional-only arguments passed"
                      " as keyword arguments: '%U'",
                      qualname, error_names);
        Py_DECREF(error_names);
        goto fail;
    }

    Py_DECREF(posonly_names);
    return 0;

fail:
    Py_XDECREF(posonly_names);
    return 1;

}

/* This is gonna seem *real weird*, but if you put some other code between
   PyEval_EvalFrame() and _PyEval_EvalFrameDefault() you will need to adjust
   the test in the if statements in Misc/gdbinit (pystack and pystackv). */

PyObject *
_PyEval_EvalCode(PyThreadState *tstate,
           PyObject *_co, PyObject *globals, PyObject *locals,
           PyObject *const *args, Py_ssize_t argcount,
           PyObject *const *kwnames, PyObject *const *kwargs,
           Py_ssize_t kwcount, int kwstep,
           PyObject *const *defs, Py_ssize_t defcount,
           PyObject *kwdefs, PyObject *closure,
           PyObject *name, PyObject *qualname)
{
    assert(is_tstate_valid(tstate));

    PyCodeObject *co = (PyCodeObject*)_co;

    if (!name) {
        name = co->co_name;
    }
    assert(name != NULL);
    assert(PyUnicode_Check(name));

    if (!qualname) {
        qualname = name;
    }
    assert(qualname != NULL);
    assert(PyUnicode_Check(qualname));

    PyObject *retval = NULL;
    const Py_ssize_t total_args = co->co_argcount + co->co_kwonlyargcount;

    if (globals == NULL) {
        _PyErr_SetString(tstate, PyExc_SystemError,
                         "PyEval_EvalCodeEx: NULL globals");
        return NULL;
    }

    /* Create the frame */
    PyFrameObject *f = _PyFrame_New_NoTrack(tstate, co, globals, locals);
    if (f == NULL) {
        return NULL;
    }
    PyObject **fastlocals = f->f_localsplus;
    PyObject **freevars = f->f_localsplus + co->co_nlocals;

    /* Create a dictionary for keyword parameters (**kwags) */
    PyObject *kwdict;
    Py_ssize_t i;
    if (co->co_flags & CO_VARKEYWORDS) {
        kwdict = PyDict_New();
        if (kwdict == NULL)
            goto fail;
        i = total_args;
        if (co->co_flags & CO_VARARGS) {
            i++;
        }
        SETLOCAL(i, kwdict);
    }
    else {
        kwdict = NULL;
    }

    /* Copy all positional arguments into local variables */
    Py_ssize_t j, n;
    if (argcount > co->co_argcount) {
        n = co->co_argcount;
    }
    else {
        n = argcount;
    }
    for (j = 0; j < n; j++) {
        PyObject *x = args[j];
        Py_INCREF(x);
        SETLOCAL(j, x);
    }

    /* Pack other positional arguments into the *args argument */
    if (co->co_flags & CO_VARARGS) {
        PyObject *u = PyTuple_FromArray(args + n, argcount - n);
        if (u == NULL) {
            goto fail;
        }
        SETLOCAL(total_args, u);
    }

    /* Handle keyword arguments passed as two strided arrays */
    kwcount *= kwstep;
    for (i = 0; i < kwcount; i += kwstep) {
        PyObject **co_varnames;
        PyObject *keyword = kwnames[i];
        PyObject *value = kwargs[i];
        Py_ssize_t j;

        if (keyword == NULL || !PyUnicode_Check(keyword)) {
            _PyErr_Format(tstate, PyExc_TypeError,
                          "%U() keywords must be strings",
                          qualname);
            goto fail;
        }

        /* Speed hack: do raw pointer compares. As names are
           normally interned this should almost always hit. */
        co_varnames = PyTuple_Items(co->co_varnames); 
        for (j = co->co_posonlyargcount; j < total_args; j++) {
            PyObject *varname = co_varnames[j];
            if (varname == keyword) {
                goto kw_found;
            }
        }

        /* Slow fallback, just in case */
        for (j = co->co_posonlyargcount; j < total_args; j++) {
            PyObject *varname = co_varnames[j];
            int cmp = PyObject_RichCompareBool( keyword, varname, Py_EQ);
            if (cmp > 0) {
                goto kw_found;
            }
            else if (cmp < 0) {
                goto fail;
            }
        }

        assert(j >= total_args);
        if (kwdict == NULL) {

            if (co->co_posonlyargcount
                && positional_only_passed_as_keyword(tstate, co,
                                                     kwcount, kwnames,
                                                     qualname))
            {
                goto fail;
            }

            _PyErr_Format(tstate, PyExc_TypeError,
                          "%U() got an unexpected keyword argument '%S'",
                          qualname, keyword);
            goto fail;
        }

        if (PyDict_SetItem(kwdict, keyword, value) == -1) {
            goto fail;
        }
        continue;

      kw_found:
        if (GETLOCAL(j) != NULL) {
            _PyErr_Format(tstate, PyExc_TypeError,
                          "%U() got multiple values for argument '%S'",
                          qualname, keyword);
            goto fail;
        }
        Py_INCREF(value);
        SETLOCAL(j, value);
    }

    /* Check the number of positional arguments */
    if ((argcount > co->co_argcount) && !(co->co_flags & CO_VARARGS)) {
        too_many_positional(tstate, co, argcount, defcount, fastlocals,
                            qualname);
        goto fail;
    }

    /* Add missing positional arguments (copy default values from defs) */
    if (argcount < co->co_argcount) {
        Py_ssize_t m = co->co_argcount - defcount;
        Py_ssize_t missing = 0;
        for (i = argcount; i < m; i++) {
            if (GETLOCAL(i) == NULL) {
                missing++;
            }
        }
        if (missing) {
            missing_arguments(tstate, co, missing, defcount, fastlocals,
                              qualname);
            goto fail;
        }
        if (n > m)
            i = n - m;
        else
            i = 0;
        for (; i < defcount; i++) {
            if (GETLOCAL(m+i) == NULL) {
                PyObject *def = defs[i];
                Py_INCREF(def);
                SETLOCAL(m+i, def);
            }
        }
    }

    /* Add missing keyword arguments (copy default values from kwdefs) */
    if (co->co_kwonlyargcount > 0) {
        Py_ssize_t missing = 0;
        for (i = co->co_argcount; i < total_args; i++) {
            if (GETLOCAL(i) != NULL)
                continue;
            PyObject *varname = PyTuple_GetItem(co->co_varnames, i);
            if (kwdefs != NULL) {
                PyObject *def = PyDict_GetItemWithError(kwdefs, varname);
                if (def) {
                    Py_INCREF(def);
                    SETLOCAL(i, def);
                    continue;
                }
                else if (_PyErr_Occurred(tstate)) {
                    goto fail;
                }
            }
            missing++;
        }
        if (missing) {
            missing_arguments(tstate, co, missing, -1, fastlocals,
                              qualname);
            goto fail;
        }
    }

    /* Allocate and initialize storage for cell vars, and copy free
       vars into frame. */
    for (i = 0; i < PyTuple_Size(co->co_cellvars); ++i) {
        PyObject *c;
        Py_ssize_t arg;
        /* Possibly account for the cell variable being an argument. */
        if (co->co_cell2arg != NULL &&
            (arg = co->co_cell2arg[i]) != CO_CELL_NOT_AN_ARG) {
            c = PyCell_New(GETLOCAL(arg));
            /* Clear the local copy. */
            SETLOCAL(arg, NULL);
        }
        else {
            c = PyCell_New(NULL);
        }
        if (c == NULL)
            goto fail;
        SETLOCAL(co->co_nlocals + i, c);
    }

    /* Copy closure variables to free variables */
    for (i = 0; i < PyTuple_Size(co->co_freevars); ++i) {
        PyObject *o = PyTuple_GetItem(closure, i);
        Py_INCREF(o);
        freevars[PyTuple_Size(co->co_cellvars) + i] = o;
    }

    /* Handle generator/coroutine/asynchronous generator */
    if (co->co_flags & (CO_GENERATOR | CO_COROUTINE)) {
        PyObject *gen;


        /* Don't need to keep the reference to f_back, it will be set
         * when the generator is resumed. */
        Py_CLEAR(f->f_back);

        /* Create a new generator that owns the ready to run frame
         * and return that as the value. */
	gen = PyGen_NewWithQualName(f, name, qualname);
        if (gen == NULL) {
            return NULL;
        }

        return gen;
    }
    
    retval = _PyEval_EvalFrame(tstate, f, 0);

fail: /* Jump here from prelude on failure */

    /* decref'ing the frame can cause __del__ methods to get invoked,
       which can call back into Python.  While we're done with the
       current Python frame (f), the associated C stack is still in use,
       so recursion_depth must be boosted for the duration.
    */
    if (Py_REFCNT(f) > 1) {
        Py_DECREF(f);
    }
    else {
        Py_DECREF(f);
    }
    return retval;
}


PyObject *
_PyEval_EvalCodeWithName(PyObject *_co, PyObject *globals, PyObject *locals,
           PyObject *const *args, Py_ssize_t argcount,
           PyObject *const *kwnames, PyObject *const *kwargs,
           Py_ssize_t kwcount, int kwstep,
           PyObject *const *defs, Py_ssize_t defcount,
           PyObject *kwdefs, PyObject *closure,
           PyObject *name, PyObject *qualname)
{
    PyThreadState *tstate = PyThreadState_Get();
    return _PyEval_EvalCode(tstate, _co, globals, locals,
               args, argcount,
               kwnames, kwargs,
               kwcount, kwstep,
               defs, defcount,
               kwdefs, closure,
               name, qualname);
}

PyObject *
PyEval_EvalCodeEx(PyObject *_co, PyObject *globals, PyObject *locals,
                  PyObject *const *args, int argcount,
                  PyObject *const *kws, int kwcount,
                  PyObject *const *defs, int defcount,
                  PyObject *kwdefs, PyObject *closure)
{
    return _PyEval_EvalCodeWithName(_co, globals, locals,
                                    args, argcount,
                                    kws, kws != NULL ? kws + 1 : NULL,
                                    kwcount, 2,
                                    defs, defcount,
                                    kwdefs, closure,
                                    NULL, NULL);
}

static PyObject *
special_lookup(PyThreadState *tstate, PyObject *o, _Py_Identifier *id)
{
    PyObject *res;
    res = _PyObject_LookupSpecial(o, id);
    if (res == NULL && !_PyErr_Occurred(tstate)) {
        _PyErr_SetObject(tstate, PyExc_AttributeError, _PyUnicode_FromId(id));
        return NULL;
    }
    return res;
}


/* Logic for the raise statement (too complicated for inlining).
   This *consumes* a reference count to each of its arguments. */
static int
do_raise(PyThreadState *tstate, PyObject *exc, PyObject *cause)
{
    PyObject *type = NULL, *value = NULL;

    if (exc == NULL) {
        /* Reraise */
        _PyErr_StackItem *exc_info = _PyErr_GetTopmostException(tstate);
        PyObject *tb;
        type = exc_info->exc_type;
        value = exc_info->exc_value;
        tb = exc_info->exc_traceback;
        if (type == Py_None || type == NULL) {
            _PyErr_SetString(tstate, PyExc_RuntimeError,
                             "No active exception to reraise");
            return 0;
        }
        Py_XINCREF(type);
        Py_XINCREF(value);
        Py_XINCREF(tb);
        _PyErr_Restore(tstate, type, value, tb);
        return 1;
    }

    /* We support the following forms of raise:
       raise
       raise <instance>
       raise <type> */

    if (PyExceptionClass_Check(exc)) {
        type = exc;
        value = _PyObject_CallNoArg(exc);
        if (value == NULL)
            goto raise_error;
        if (!PyExceptionInstance_Check(value)) {
            _PyErr_Format(tstate, PyExc_TypeError,
                          "calling %R should have returned an instance of "
                          "BaseException, not %R",
                          type, Py_TYPE(value));
             goto raise_error;
        }
    }
    else if (PyExceptionInstance_Check(exc)) {
        value = exc;
        type = PyExceptionInstance_Class(exc);
        Py_INCREF(type);
    }
    else {
        /* Not something you can raise.  You get an exception
           anyway, just not what you specified :-) */
        Py_DECREF(exc);
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "exceptions must derive from BaseException");
        goto raise_error;
    }

    assert(type != NULL);
    assert(value != NULL);

    if (cause) {
        PyObject *fixed_cause;
        if (PyExceptionClass_Check(cause)) {
            fixed_cause = _PyObject_CallNoArg(cause);
            if (fixed_cause == NULL)
                goto raise_error;
            Py_DECREF(cause);
        }
        else if (PyExceptionInstance_Check(cause)) {
            fixed_cause = cause;
        }
        else if (cause == Py_None) {
            Py_DECREF(cause);
            fixed_cause = NULL;
        }
        else {
            _PyErr_SetString(tstate, PyExc_TypeError,
                             "exception causes must derive from "
                             "BaseException");
            goto raise_error;
        }
        PyException_SetCause(value, fixed_cause);
    }

    _PyErr_SetObject(tstate, type, value);
    /* _PyErr_SetObject incref's its arguments */
    Py_DECREF(value);
    Py_DECREF(type);
    return 0;

raise_error:
    Py_XDECREF(value);
    Py_XDECREF(type);
    Py_XDECREF(cause);
    return 0;
}

/* Iterate v argcnt times and store the results on the stack (via decreasing
   sp).  Return 1 for success, 0 if error.

   If argcntafter == -1, do a simple unpack. If it is >= 0, do an unpack
   with a variable target.
*/

static int
unpack_iterable(PyThreadState *tstate, PyObject *v,
                int argcnt, int argcntafter, PyObject **sp)
{
    int i = 0, j = 0;
    Py_ssize_t ll = 0;
    PyObject *it;  /* iter(v) */
    PyObject *w;
    PyObject *l = NULL; /* variable list */

    assert(v != NULL);

    it = PyObject_GetIter(v);
    if (it == NULL) {
        if (_PyErr_ExceptionMatches(tstate, PyExc_TypeError) &&
            Py_TYPE(v)->tp_iter == NULL && !PySequence_Check(v))
        {
            _PyErr_Format(tstate, PyExc_TypeError,
                          "cannot unpack non-iterable %.200s object",
                          Py_TYPE(v)->tp_name);
        }
        return 0;
    }

    for (; i < argcnt; i++) {
        w = PyIter_Next(it);
        if (w == NULL) {
            /* Iterator done, via error or exhaustion. */
            if (!_PyErr_Occurred(tstate)) {
                if (argcntafter == -1) {
                    _PyErr_Format(tstate, PyExc_ValueError,
                                  "not enough values to unpack "
                                  "(expected %d, got %d)",
                                  argcnt, i);
                }
                else {
                    _PyErr_Format(tstate, PyExc_ValueError,
                                  "not enough values to unpack "
                                  "(expected at least %d, got %d)",
                                  argcnt + argcntafter, i);
                }
            }
            goto Error;
        }
        *--sp = w;
    }

    if (argcntafter == -1) {
        /* We better have exhausted the iterator now. */
        w = PyIter_Next(it);
        if (w == NULL) {
            if (_PyErr_Occurred(tstate))
                goto Error;
            Py_DECREF(it);
            return 1;
        }
        Py_DECREF(w);
        _PyErr_Format(tstate, PyExc_ValueError,
                      "too many values to unpack (expected %d)",
                      argcnt);
        goto Error;
    }

    l = PySequence_List(it);
    if (l == NULL)
        goto Error;
    *--sp = l;
    i++;

    ll = PyList_Size(l);
    if (ll < argcntafter) {
        _PyErr_Format(tstate, PyExc_ValueError,
            "not enough values to unpack (expected at least %d, got %zd)",
            argcnt + argcntafter, argcnt + ll);
        goto Error;
    }

    /* Pop the "after-variable" args off the list. */
    for (j = argcntafter; j > 0; j--, i++) {
        *--sp = PyList_GetItem(l, ll - j);
    }
    /* Resize the list. */
    Py_SET_SIZE(l, ll - argcntafter);
    Py_DECREF(it);
    return 1;

Error:
    for (; i > 0; i--, sp++)
        Py_DECREF(*sp);
    Py_XDECREF(it);
    return 0;
}

PyFrameObject *
PyEval_GetFrame(void)
{
    PyThreadState *tstate = PyThreadState_Get();
    return tstate->frame;
}

PyObject *
PyEval_GetBuiltins(void)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyFrameObject *current_frame = tstate->frame;
    if (current_frame == NULL)
        return tstate->interp->builtins;
    else
        return current_frame->f_builtins;
}

/* Convenience function to get a builtin from its name */
PyObject *
_PyEval_GetBuiltinId(_Py_Identifier *name)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *attr = _PyDict_GetItemIdWithError(PyEval_GetBuiltins(), name);
    if (attr) {
        Py_INCREF(attr);
    }
    else if (!_PyErr_Occurred(tstate)) {
        _PyErr_SetObject(tstate, PyExc_AttributeError, _PyUnicode_FromId(name));
    }
    return attr;
}

PyObject *
PyEval_GetLocals(void)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyFrameObject *current_frame = tstate->frame;
    if (current_frame == NULL) {
        _PyErr_SetString(tstate, PyExc_SystemError, "frame does not exist");
        return NULL;
    }

    if (PyFrame_FastToLocalsWithError(current_frame) < 0) {
        return NULL;
    }

    assert(current_frame->f_locals != NULL);
    return current_frame->f_locals;
}

PyObject *
PyEval_GetGlobals(void)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyFrameObject *current_frame = tstate->frame;
    if (current_frame == NULL) {
        return NULL;
    }

    assert(current_frame->f_globals != NULL);
    return current_frame->f_globals;
}

int
PyEval_MergeCompilerFlags(PyCompilerFlags *cf)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyFrameObject *current_frame = tstate->frame;
    int result = cf->cf_flags != 0;

    if (current_frame != NULL) {
        const int codeflags = current_frame->f_code->co_flags;
        const int compilerflags = codeflags & PyCF_MASK;
        if (compilerflags) {
            result = 1;
            cf->cf_flags |= compilerflags;
        }
    }
    return result;
}


const char *
PyEval_GetFuncName(PyObject *func)
{
    if (PyMethod_Check(func))
        return PyEval_GetFuncName(PyMethod_Function(func));
    else if (PyFunction_Check(func))
        return PyUnicode_AsChar(((PyFunctionObject*)func)->func_name);
    else if (PyCFunction_Check(func))
        return ((PyCFunctionObject*)func)->m_ml->ml_name;
    else
        return Py_TYPE(func)->tp_name;
}

const char *
PyEval_GetFuncDesc(PyObject *func)
{
    if (PyMethod_Check(func))
        return "()";
    else if (PyFunction_Check(func))
        return "()";
    else if (PyCFunction_Check(func))
        return "()";
    else
        return " object";
}

/* Issue #29227: Inline call_function() into _PyEval_EvalFrameDefault()
   to reduce the stack consumption. */
Py_LOCAL_INLINE(PyObject *) _Py_HOT_FUNCTION
call_function(PyThreadState *tstate, PyObject ***pp_stack, Py_ssize_t oparg, PyObject *kwnames)
{
    PyObject **pfunc = (*pp_stack) - oparg - 1;
    PyObject *func = *pfunc;
    PyObject *x, *w;
    Py_ssize_t nkwargs = (kwnames == NULL) ? 0 : PyTuple_Size(kwnames);
    Py_ssize_t nargs = oparg - nkwargs;
    PyObject **stack = (*pp_stack) - nargs - nkwargs;
    x = PyObject_Vectorcall(func, stack, nargs | PY_VECTORCALL_ARGUMENTS_OFFSET, kwnames);

    assert((x != NULL) ^ (_PyErr_Occurred(tstate) != NULL));

    /* Clear the stack of the function object. */
    while ((*pp_stack) > pfunc) {
        w = EXT_POP(*pp_stack);
        Py_DECREF(w);
    }

    return x;
}

static PyObject *
do_call_core(PyThreadState *tstate, PyObject *func, PyObject *callargs, PyObject *kwdict)
{
    PyObject *result;

    if (PyCFunction_CheckExact(func) || PyCMethod_CheckExact(func)) {
        result = PyObject_Call(func, callargs, kwdict);
        return result;
    }
    return PyObject_Call(func, callargs, kwdict);
}

/* Extract a slice index from a PyLong or an object with the
   nb_index slot defined, and store in *pi.
   Silently reduce values larger than PY_SSIZE_T_MAX to PY_SSIZE_T_MAX,
   and silently boost values less than PY_SSIZE_T_MIN to PY_SSIZE_T_MIN.
   Return 0 on error, 1 on success.
*/
int
_PyEval_SliceIndex(PyObject *v, Py_ssize_t *pi)
{
    PyThreadState *tstate = PyThreadState_Get();
    if (v != Py_None) {
        Py_ssize_t x;
        if (_PyIndex_Check(v)) {
            x = PyNumber_AsSsize_t(v, NULL);
            if (x == -1 && _PyErr_Occurred(tstate))
                return 0;
        }
        else {
            _PyErr_SetString(tstate, PyExc_TypeError,
                             "slice indices must be integers or "
                             "None or have an __index__ method");
            return 0;
        }
        *pi = x;
    }
    return 1;
}

int
_PyEval_SliceIndexNotNone(PyObject *v, Py_ssize_t *pi)
{
    PyThreadState *tstate = PyThreadState_Get();
    Py_ssize_t x;
    if (_PyIndex_Check(v)) {
        x = PyNumber_AsSsize_t(v, NULL);
        if (x == -1 && _PyErr_Occurred(tstate))
            return 0;
    }
    else {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "slice indices must be integers or "
                         "have an __index__ method");
        return 0;
    }
    *pi = x;
    return 1;
}

static PyObject *
import_name(PyThreadState *tstate, PyFrameObject *f,
            PyObject *name, PyObject *fromlist, PyObject *level)
{
    PyObject *res;
    {
        int ilevel = _PyLong_AsInt(level);
        if (ilevel == -1 && _PyErr_Occurred(tstate)) {
            return NULL;
        }
        res = PyImport_ImportModuleLevelObject(
                        name,
                        f->f_globals,
                        f->f_locals == NULL ? Py_None : f->f_locals,
                        fromlist,
                        ilevel);
        return res;
    }
}

static PyObject *
import_from(PyThreadState *tstate, PyObject *v, PyObject *name)
{
    PyObject *x;
    PyObject *fullmodname, *pkgname, *pkgpath, *pkgname_or_unknown, *errmsg=NULL;

    if (_PyObject_LookupAttr(v, name, &x) != 0) {
        return x;
    }
    /* Issue #17636: in case this failed because of a circular relative
       import, try to fallback on reading the module directly from
       sys.modules. */
    pkgname = _PyObject_GetAttrId(v, &PyId___name__);
    if (pkgname == NULL) {
        goto error;
    }
    if (!PyUnicode_Check(pkgname)) {
        Py_CLEAR(pkgname);
        goto error;
    }
    fullmodname = PyUnicode_FromFormat("%U.%U", pkgname, name);
    if (fullmodname == NULL) {
        Py_DECREF(pkgname);
        return NULL;
    }
    x = PyImport_GetModule(fullmodname);
    Py_DECREF(fullmodname);
    if (x == NULL && !_PyErr_Occurred(tstate)) {
        goto error;
    }
    Py_DECREF(pkgname);
    return x;
 error:
    pkgpath = PyModule_GetFilenameObject(v);
    if (pkgname == NULL) {
        pkgname_or_unknown = PyUnicode_FromString("<unknown module name>");
        if (pkgname_or_unknown == NULL) {
            Py_XDECREF(pkgpath);
            return NULL;
        }
    } else {
        pkgname_or_unknown = pkgname;
    }

    if (pkgpath == NULL || !PyUnicode_Check(pkgpath)) {
        _PyErr_Clear(tstate);
        errmsg = PyUnicode_FromFormat(
            "cannot import name %R from %R (unknown location)",
            name, pkgname_or_unknown
        );
        /* NULL checks for errmsg and pkgname done by PyErr_SetImportError. */
        PyErr_SetImportError(errmsg, pkgname, NULL);
    }
    Py_XDECREF(errmsg);
    Py_XDECREF(pkgname_or_unknown);
    Py_XDECREF(pkgpath);
    return NULL;
}

static int
import_all_from(PyThreadState *tstate, PyObject *locals, PyObject *v)
{
    _Py_IDENTIFIER(__all__);
    _Py_IDENTIFIER(__dict__);
    PyObject *all, *dict, *name, *value;
    int skip_leading_underscores = 0;
    int pos, err;

    if (_PyObject_LookupAttrId(v, &PyId___all__, &all) < 0) {
        return -1; /* Unexpected error */
    }
    if (all == NULL) {
        if (_PyObject_LookupAttrId(v, &PyId___dict__, &dict) < 0) {
            return -1;
        }
        if (dict == NULL) {
            _PyErr_SetString(tstate, PyExc_ImportError,
                    "from-import-* object has no __dict__ and no __all__");
            return -1;
        }
        all = PyMapping_Keys(dict);
        Py_DECREF(dict);
        if (all == NULL)
            return -1;
        skip_leading_underscores = 1;
    }

    for (pos = 0, err = 0; ; pos++) {
        name = PySequence_GetItem(all, pos);
        if (name == NULL) {
            if (!_PyErr_ExceptionMatches(tstate, PyExc_IndexError)) {
                err = -1;
            }
            else {
                _PyErr_Clear(tstate);
            }
            break;
        }
        if (!PyUnicode_Check(name)) {
            PyObject *modname = _PyObject_GetAttrId(v, &PyId___name__);
            if (modname == NULL) {
                Py_DECREF(name);
                err = -1;
                break;
            }
            if (!PyUnicode_Check(modname)) {
                _PyErr_Format(tstate, PyExc_TypeError,
                              "module __name__ must be a string, not %.100s",
                              Py_TYPE(modname)->tp_name);
            }
            else {
                _PyErr_Format(tstate, PyExc_TypeError,
                              "%s in %U.%s must be str, not %.100s",
                              skip_leading_underscores ? "Key" : "Item",
                              modname,
                              skip_leading_underscores ? "__dict__" : "__all__",
                              Py_TYPE(name)->tp_name);
            }
            Py_DECREF(modname);
            Py_DECREF(name);
            err = -1;
            break;
        }
        if (skip_leading_underscores) {
            if (PyUnicode_READ_CHAR(name, 0) == '_') {
                Py_DECREF(name);
                continue;
            }
        }
        value = PyObject_GetAttr(v, name);
        if (value == NULL)
            err = -1;
        else
            err = PyObject_SetItem(locals, name, value);
        Py_DECREF(name);
        Py_XDECREF(value);
        if (err != 0)
            break;
    }
    Py_DECREF(all);
    return err;
}

static int
check_args_iterable(PyThreadState *tstate, PyObject *func, PyObject *args)
{
    if (Py_TYPE(args)->tp_iter == NULL && !PySequence_Check(args)) {
        /* check_args_iterable() may be called with a live exception:
         * clear it to prevent calling _PyObject_FunctionStr() with an
         * exception set. */
        _PyErr_Clear(tstate);
        PyObject *funcstr = _PyObject_FunctionStr(func);
        if (funcstr != NULL) {
            _PyErr_Format(tstate, PyExc_TypeError,
                          "%U argument after * must be an iterable, not %.200s",
                          funcstr, Py_TYPE(args)->tp_name);
            Py_DECREF(funcstr);
        }
        return -1;
    }
    return 0;
}

static void
format_kwargs_error(PyThreadState *tstate, PyObject *func, PyObject *kwargs)
{
    /* _PyDict_MergeEx raises attribute
     * error (percolated from an attempt
     * to get 'keys' attribute) instead of
     * a type error if its second argument
     * is not a mapping.
     */
    if (_PyErr_ExceptionMatches(tstate, PyExc_AttributeError)) {
        _PyErr_Clear(tstate);
        PyObject *funcstr = _PyObject_FunctionStr(func);
        if (funcstr != NULL) {
            _PyErr_Format(
                tstate, PyExc_TypeError,
                "%U argument after ** must be a mapping, not %.200s",
                funcstr, Py_TYPE(kwargs)->tp_name);
            Py_DECREF(funcstr);
        }
    }
    else if (_PyErr_ExceptionMatches(tstate, PyExc_KeyError)) {
        PyObject *exc, *val, *tb;
        _PyErr_Fetch(tstate, &exc, &val, &tb);
        if (val && PyTuple_Check(val) && PyTuple_Size(val) == 1) {
            _PyErr_Clear(tstate);
            PyObject *funcstr = _PyObject_FunctionStr(func);
            if (funcstr != NULL) {
                PyObject *key = PyTuple_GetItem(val, 0);
                _PyErr_Format(
                    tstate, PyExc_TypeError,
                    "%U got multiple values for keyword argument '%S'",
                    funcstr, key);
                Py_DECREF(funcstr);
            }
            Py_XDECREF(exc);
            Py_XDECREF(val);
            Py_XDECREF(tb);
        }
        else {
            _PyErr_Restore(tstate, exc, val, tb);
        }
    }
}

static void
format_exc_check_arg(PyThreadState *tstate, PyObject *exc,
                     const char *format_str, PyObject *obj)
{
    const char *obj_str;

    if (!obj)
        return;

    obj_str = PyUnicode_AsChar(obj);
    if (!obj_str)
        return;

    _PyErr_Format(tstate, exc, format_str, obj_str);
}

static void
format_exc_unbound(PyThreadState *tstate, PyCodeObject *co, int oparg)
{
    PyObject *name;
    /* Don't stomp existing exception */
    if (_PyErr_Occurred(tstate))
        return;
    if (oparg < PyTuple_Size(co->co_cellvars)) {
        name = PyTuple_GetItem(co->co_cellvars,
                                oparg);
        format_exc_check_arg(tstate,
            PyExc_UnboundLocalError,
            UNBOUNDLOCAL_ERROR_MSG,
            name);
    } else {
        name = PyTuple_GetItem(co->co_freevars, oparg -
                                PyTuple_Size(co->co_cellvars));
        format_exc_check_arg(tstate, PyExc_NameError,
                             UNBOUNDFREE_ERROR_MSG, name);
    }
}

Py_ssize_t
_PyEval_RequestCodeExtraIndex(freefunc free)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    Py_ssize_t new_index;

    if (interp->co_extra_user_count == MAX_CO_EXTRA_USERS - 1) {
        return -1;
    }
    new_index = interp->co_extra_user_count++;
    interp->co_extra_freefuncs[new_index] = free;
    return new_index;
}
