
/* Top level execution of Python code (including in __main__) */

/* To help control the interfaces between the startup, execution and
 * shutdown code, the phases are split across separate modules (boostrap,
 * pythonrun, shutdown)
 */

/* TODO: Cull includes following phase split */

#include "Python.h"

#include "Python-ast.h"
#undef Yield   /* undefine macro conflicting with <winbase.h> */

#include "pycore_interp.h"        // PyInterpreterState.importlib
#include "pycore_object.h"        // _PyDebug_PrintTotalRefs()
#include "pycore_pyerrors.h"      // _PyErr_Fetch
#include "pycore_pylifecycle.h"   // _Py_UnhandledKeyboardInterrupt
#include "pycore_pystate.h"       // _PyInterpreterState_GET()

#include "node.h"                 // node
#include "token.h"                // INDENT
#include "parsetok.h"             // perrdetail
#include "errcode.h"              // E_EOF
#include "code.h"                 // PyCodeObject
#include "symtable.h"             // PySymtable_BuildObject()
#include "ast.h"                  // PyAST_FromNodeObject()
#include "marshal.h"              // PyMarshal_ReadLongFromFile()

#include "pegen_interface.h"      // PyPegen_ASTFrom*

_Py_IDENTIFIER(builtins);
_Py_IDENTIFIER(excepthook);
_Py_IDENTIFIER(flush);
_Py_IDENTIFIER(last_traceback);
_Py_IDENTIFIER(last_type);
_Py_IDENTIFIER(last_value);
_Py_IDENTIFIER(ps1);
_Py_IDENTIFIER(ps2);
_Py_IDENTIFIER(stdin);
_Py_IDENTIFIER(stdout);
_Py_IDENTIFIER(stderr);
_Py_static_string(PyId_string, "<string>");

#ifdef __cplusplus
extern "C" {
#endif

/* Forward */
static void flush_io(void);
static PyObject *run_mod(mod_ty, PyObject *, PyObject *, PyObject *,
			 PyCompilerFlags *);
static int PyRun_InteractiveOneObjectEx(FILE *, PyObject *, PyCompilerFlags *);

/* Parse input from a file and execute it */
int
PyRun_AnyFileExFlags(FILE *fp, const char *filename, int closeit,
                     PyCompilerFlags *flags)
{
    if (filename == NULL)
        filename = "???";
    if (Py_FdIsInteractive(fp, filename)) {
        int err = PyRun_InteractiveLoopFlags(fp, filename, flags);
        if (closeit)
            fclose(fp);
        return err;
    }
    else
        return PyRun_SimpleFileExFlags(fp, filename, closeit, flags);
}

int
PyRun_InteractiveLoopFlags(FILE *fp, const char *filename_str, PyCompilerFlags *flags)
{
    PyObject *filename, *v;
    int ret, err;
    PyCompilerFlags local_flags = _PyCompilerFlags_INIT;
    int nomem_count = 0;

    filename = PyUnicode_FromString(filename_str);
    
    if (filename == NULL) {
        PyErr_Print();
        return -1;
    }

    if (flags == NULL) {
        flags = &local_flags;
    }
    v = _PySys_GetObjectId(&PyId_ps1);
    if (v == NULL) {
        _PySys_SetObjectId(&PyId_ps1, v = PyUnicode_FromString(">>> "));
        Py_XDECREF(v);
    }
    v = _PySys_GetObjectId(&PyId_ps2);
    if (v == NULL) {
        _PySys_SetObjectId(&PyId_ps2, v = PyUnicode_FromString("... "));
        Py_XDECREF(v);
    }
    err = 0;
    do {
        ret = PyRun_InteractiveOneObjectEx(fp, filename, flags);
        if (ret == -1 && PyErr_Occurred()) {
            /* Prevent an endless loop after multiple consecutive MemoryErrors
             * while still allowing an interactive command to fail with a
             * MemoryError. */
            if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
                if (++nomem_count > 16) {
                    PyErr_Clear();
                    err = -1;
                    break;
                }
            } else {
                nomem_count = 0;
            }
            PyErr_Print();
            flush_io();
        } else {
            nomem_count = 0;
        }
    } while (ret != E_EOF);
    Py_DECREF(filename);
    return err;
}

/* A PyRun_InteractiveOneObject() auxiliary function that does not print the
 * error on failure. */
static int
PyRun_InteractiveOneObjectEx(FILE *fp, PyObject *filename,
                             PyCompilerFlags *flags)
{
    PyObject *m, *d, *v, *w, *oenc = NULL, *mod_name;
    mod_ty mod;
    const char *ps1 = "", *ps2 = "", *enc = NULL;
    int errcode = 0;
    _Py_IDENTIFIER(encoding);
    _Py_IDENTIFIER(__main__);

    mod_name = _PyUnicode_FromId(&PyId___main__); /* borrowed */
    if (mod_name == NULL) {
        return -1;
    }

    if (fp == stdin) {
        /* Fetch encoding from sys.stdin if possible. */
        v = _PySys_GetObjectId(&PyId_stdin);
        if (v && v != Py_None) {
            oenc = _PyObject_GetAttrId(v, &PyId_encoding);
            if (oenc)
                enc = PyUnicode_AsChar(oenc);
            if (!enc)
                PyErr_Clear();
        }
    }
    v = _PySys_GetObjectId(&PyId_ps1);
    if (v != NULL) {
        v = PyObject_Str(v);
        if (v == NULL)
            PyErr_Clear();
        else if (PyUnicode_Check(v)) {
            ps1 = PyUnicode_AsChar(v);
            if (ps1 == NULL) {
                PyErr_Clear();
                ps1 = "";
            }
        }
    }
    w = _PySys_GetObjectId(&PyId_ps2);
    if (w != NULL) {
        w = PyObject_Str(w);
        if (w == NULL)
            PyErr_Clear();
        else if (PyUnicode_Check(w)) {
            ps2 = PyUnicode_AsChar(w);
            if (ps2 == NULL) {
                PyErr_Clear();
                ps2 = "";
            }
        }
    }
    mod = PyPegen_ASTFromFileObject(fp, filename, Py_single_input,
                                    ps1, ps2, flags, &errcode);

    Py_XDECREF(v);
    Py_XDECREF(w);
    Py_XDECREF(oenc);
    if (mod == NULL) {
        if (errcode == E_EOF) {
            PyErr_Clear();
            return E_EOF;
        }
        return -1;
    }
    m = PyImport_AddModuleObject(mod_name);
    if (m == NULL) {
        return -1;
    }
    d = PyModule_GetDict(m);
    v = run_mod(mod, filename, d, d, flags);
    if (v == NULL) {
        return -1;
    }
    Py_DECREF(v);
    flush_io();
    return 0;
}

int
PyRun_InteractiveOneObject(FILE *fp, PyObject *filename, PyCompilerFlags *flags)
{
    int res;

    res = PyRun_InteractiveOneObjectEx(fp, filename, flags);
    if (res == -1) {
        PyErr_Print();
        flush_io();
    }
    return res;
}

int
PyRun_InteractiveOneFlags(FILE *fp, const char *filename_str, PyCompilerFlags *flags)
{
    PyObject *filename;
    int res;

    filename = PyUnicode_FromString(filename_str);
    
    if (filename == NULL) {
        PyErr_Print();
        return -1;
    }
    res = PyRun_InteractiveOneObject(fp, filename, flags);
    Py_DECREF(filename);
    return res;
}

static int
set_main_loader(PyObject *d, const char *filename, const char *loader_name)
{
    PyObject *filename_obj, *bootstrap, *loader_type = NULL, *loader;
    int result = 0;

    filename_obj = PyUnicode_FromString(filename);
    
    if (filename_obj == NULL)
        return -1;
    PyInterpreterState *interp = _PyInterpreterState_GET();
    bootstrap = PyObject_GetAttrString(interp->importlib,
                                       "_bootstrap_external");
    if (bootstrap != NULL) {
        loader_type = PyObject_GetAttrString(bootstrap, loader_name);
        Py_DECREF(bootstrap);
    }
    if (loader_type == NULL) {
        Py_DECREF(filename_obj);
        return -1;
    }
    loader = PyObject_CallFunction(loader_type, "sN", "__main__", filename_obj);
    Py_DECREF(loader_type);
    if (loader == NULL) {
        return -1;
    }
    if (PyDict_SetItemString(d, "__loader__", loader) < 0) {
        result = -1;
    }
    Py_DECREF(loader);
    return result;
}

int
PyRun_SimpleFileExFlags(FILE *fp, const char *filename, int closeit,
                        PyCompilerFlags *flags)
{
    PyObject *m, *d, *v;
    const char *ext;
    int set_file_name = 0, ret = -1;
    size_t len;

    m = PyImport_AddModule("__main__");
    if (m == NULL)
        return -1;
    Py_INCREF(m);
    d = PyModule_GetDict(m);
    if (PyDict_GetItemString(d, "__file__") == NULL) {
        PyObject *f;
	f = PyUnicode_FromString(filename);
	
        if (f == NULL)
            goto done;
        if (PyDict_SetItemString(d, "__file__", f) < 0) {
            Py_DECREF(f);
            goto done;
        }
        if (PyDict_SetItemString(d, "__cached__", Py_None) < 0) {
            Py_DECREF(f);
            goto done;
        }
        set_file_name = 1;
        Py_DECREF(f);
    }
    len = strlen(filename);
    ext = filename + len - (len > 4 ? 4 : 0);
    /* When running from stdin, leave __main__.__loader__ alone */
    if (strcmp(filename, "<stdin>") != 0 &&
	set_main_loader(d, filename, "SourceFileLoader") < 0) {
      fprintf(stderr, "python: failed to set __main__.__loader__\n");
      ret = -1;
      goto done;
    }
    v = PyRun_FileExFlags(fp, filename, Py_file_input, d, d,
                              closeit, flags);
    flush_io();
    if (v == NULL) {
        Py_CLEAR(m);
        PyErr_Print();
        goto done;
    }
    Py_DECREF(v);
    ret = 0;
  done:
    if (set_file_name) {
        if (PyDict_DelItemString(d, "__file__")) {
            PyErr_Clear();
        }
        if (PyDict_DelItemString(d, "__cached__")) {
            PyErr_Clear();
        }
    }
    Py_XDECREF(m);
    return ret;
}

int
PyRun_SimpleStringFlags(const char *command, PyCompilerFlags *flags)
{
    PyObject *m, *d, *v;
    m = PyImport_AddModule("__main__");
    if (m == NULL)
        return -1;
    d = PyModule_GetDict(m);
    v = PyRun_StringFlags(command, Py_file_input, d, d, flags);
    if (v == NULL) {
        PyErr_Print();
        return -1;
    }
    Py_DECREF(v);
    return 0;
}

static int
parse_syntax_error(PyObject *err, PyObject **message, PyObject **filename,
                   Py_ssize_t *lineno, Py_ssize_t *offset, PyObject **text)
{
    Py_ssize_t hold;
    PyObject *v;
    _Py_IDENTIFIER(msg);
    _Py_IDENTIFIER(filename);
    _Py_IDENTIFIER(lineno);
    _Py_IDENTIFIER(offset);
    _Py_IDENTIFIER(text);

    *message = NULL;
    *filename = NULL;

    /* new style errors.  `err' is an instance */
    *message = _PyObject_GetAttrId(err, &PyId_msg);
    if (!*message)
        goto finally;

    v = _PyObject_GetAttrId(err, &PyId_filename);
    if (!v)
        goto finally;
    if (v == Py_None) {
        Py_DECREF(v);
        *filename = _PyUnicode_FromId(&PyId_string);
        if (*filename == NULL)
            goto finally;
        Py_INCREF(*filename);
    }
    else {
        *filename = v;
    }

    v = _PyObject_GetAttrId(err, &PyId_lineno);
    if (!v)
        goto finally;
    hold = PyLong_AsSsize_t(v);
    Py_DECREF(v);
    if (hold < 0 && PyErr_Occurred())
        goto finally;
    *lineno = hold;

    v = _PyObject_GetAttrId(err, &PyId_offset);
    if (!v)
        goto finally;
    if (v == Py_None) {
        *offset = -1;
        Py_DECREF(v);
    } else {
        hold = PyLong_AsSsize_t(v);
        Py_DECREF(v);
        if (hold < 0 && PyErr_Occurred())
            goto finally;
        *offset = hold;
    }

    v = _PyObject_GetAttrId(err, &PyId_text);
    if (!v)
        goto finally;
    if (v == Py_None) {
        Py_DECREF(v);
        *text = NULL;
    }
    else {
        *text = v;
    }
    return 1;

finally:
    Py_XDECREF(*message);
    Py_XDECREF(*filename);
    return 0;
}

static void
print_error_text(PyObject *f, Py_ssize_t offset, PyObject *text_obj)
{
    /* Convert text to a char pointer; return if error */
    const char *text = PyUnicode_AsChar(text_obj);
    if (text == NULL)
        return;

    /* Convert offset from 1-based to 0-based */
    offset--;

    /* Strip leading whitespace from text, adjusting offset as we go */
    while (*text == ' ' || *text == '\t' || *text == '\f') {
        text++;
        offset--;
    }

    /* Calculate text length excluding trailing newline */
    Py_ssize_t len = strlen(text);
    if (len > 0 && text[len-1] == '\n') {
        len--;
    }

    /* Clip offset to at most len */
    if (offset > len) {
        offset = len;
    }

    /* Skip past newlines embedded in text */
    for (;;) {
        const char *nl = strchr(text, '\n');
        if (nl == NULL) {
            break;
        }
        Py_ssize_t inl = nl - text;
        if (inl >= offset) {
            break;
        }
        inl += 1;
        text += inl;
        len -= inl;
        offset -= (int)inl;
    }

    /* Print text */
    PyFile_WriteString("    ", f);
    PyFile_WriteString(text, f);

    /* Make sure there's a newline at the end */
    if (text[len] != '\n') {
        PyFile_WriteString("\n", f);
    }

    /* Don't print caret if it points to the left of the text */
    if (offset < 0)
        return;

    /* Write caret line */
    PyFile_WriteString("    ", f);
    while (--offset >= 0) {
        PyFile_WriteString(" ", f);
    }
    PyFile_WriteString("^\n", f);
}


int
_Py_HandleSystemExit(int *exitcode_p)
{
    int inspect = _Py_GetConfig()->inspect;
    if (inspect) {
        /* Don't exit if -i flag was given. This flag is set to 0
         * when entering interactive mode for inspecting. */
        return 0;
    }

    if (!PyErr_ExceptionMatches(PyExc_SystemExit)) {
        return 0;
    }

    PyObject *exception, *value, *tb;
    PyErr_Fetch(&exception, &value, &tb);

    fflush(stdout);

    int exitcode = 0;
    if (value == NULL || value == Py_None) {
        goto done;
    }

    if (PyExceptionInstance_Check(value)) {
        /* The error code should be in the `code' attribute. */
        _Py_IDENTIFIER(code);
        PyObject *code = _PyObject_GetAttrId(value, &PyId_code);
        if (code) {
            Py_DECREF(value);
            value = code;
            if (value == Py_None)
                goto done;
        }
        /* If we failed to dig out the 'code' attribute,
           just let the else clause below print the error. */
    }

    if (PyLong_Check(value)) {
        exitcode = (int)PyLong_AsLong(value);
    }
    else {
        PyObject *sys_stderr = _PySys_GetObjectId(&PyId_stderr);
        /* We clear the exception here to avoid triggering the assertion
         * in PyObject_Str that ensures it won't silently lose exception
         * details.
         */
        PyErr_Clear();
        if (sys_stderr != NULL && sys_stderr != Py_None) {
            PyFile_WriteObject(value, sys_stderr, Py_PRINT_RAW);
        } else {
            PyObject_Print(value, stderr, Py_PRINT_RAW);
            fflush(stderr);
        }
        PySys_WriteStderr("\n");
        exitcode = 1;
    }

 done:
    /* Restore and clear the exception info, in order to properly decref
     * the exception, value, and traceback.      If we just exit instead,
     * these leak, which confuses PYTHONDUMPREFS output, and may prevent
     * some finalizers from running.
     */
    PyErr_Restore(exception, value, tb);
    PyErr_Clear();
    *exitcode_p = exitcode;
    return 1;
}


static void
handle_system_exit(void)
{
    int exitcode;
    if (_Py_HandleSystemExit(&exitcode)) {
        Py_Exit(exitcode);
    }
}


static void
_PyErr_PrintEx(PyThreadState *tstate, int set_sys_last_vars)
{
    PyObject *exception, *v, *tb, *hook;

    handle_system_exit();

    _PyErr_Fetch(tstate, &exception, &v, &tb);
    if (exception == NULL) {
        goto done;
    }

    _PyErr_NormalizeException(tstate, &exception, &v, &tb);
    if (tb == NULL) {
        tb = Py_None;
        Py_INCREF(tb);
    }
    PyException_SetTraceback(v, tb);
    if (exception == NULL) {
        goto done;
    }

    /* Now we know v != NULL too */
    if (set_sys_last_vars) {
        if (_PySys_SetObjectId(&PyId_last_type, exception) < 0) {
            _PyErr_Clear(tstate);
        }
        if (_PySys_SetObjectId(&PyId_last_value, v) < 0) {
            _PyErr_Clear(tstate);
        }
        if (_PySys_SetObjectId(&PyId_last_traceback, tb) < 0) {
            _PyErr_Clear(tstate);
        }
    }
    hook = _PySys_GetObjectId(&PyId_excepthook);
    if (hook) {
        PyObject* stack[3];
        PyObject *result;

        stack[0] = exception;
        stack[1] = v;
        stack[2] = tb;
        result = _PyObject_FastCall(hook, stack, 3);
        if (result == NULL) {
            handle_system_exit();

            PyObject *exception2, *v2, *tb2;
            _PyErr_Fetch(tstate, &exception2, &v2, &tb2);
            _PyErr_NormalizeException(tstate, &exception2, &v2, &tb2);
            /* It should not be possible for exception2 or v2
               to be NULL. However PyErr_Display() can't
               tolerate NULLs, so just be safe. */
            if (exception2 == NULL) {
                exception2 = Py_None;
                Py_INCREF(exception2);
            }
            if (v2 == NULL) {
                v2 = Py_None;
                Py_INCREF(v2);
            }
            fflush(stdout);
            PySys_WriteStderr("Error in sys.excepthook:\n");
            PyErr_Display(exception2, v2, tb2);
            PySys_WriteStderr("\nOriginal exception was:\n");
            PyErr_Display(exception, v, tb);
            Py_DECREF(exception2);
            Py_DECREF(v2);
            Py_XDECREF(tb2);
        }
        Py_XDECREF(result);
    }
    else {
        PySys_WriteStderr("sys.excepthook is missing\n");
        PyErr_Display(exception, v, tb);
    }

done:
    Py_XDECREF(exception);
    Py_XDECREF(v);
    Py_XDECREF(tb);
}

void
_PyErr_Print(PyThreadState *tstate)
{
    _PyErr_PrintEx(tstate, 1);
}

void
PyErr_PrintEx(int set_sys_last_vars)
{
    PyThreadState *tstate = PyThreadState_Get();
    _PyErr_PrintEx(tstate, set_sys_last_vars);
}

void
PyErr_Print(void)
{
    PyErr_PrintEx(1);
}

static void
print_exception(PyObject *f, PyObject *value)
{
    int err = 0;
    PyObject *type, *tb;
    _Py_IDENTIFIER(print_file_and_line);

    if (!PyExceptionInstance_Check(value)) {
        err = PyFile_WriteString("TypeError: print_exception(): Exception expected for value, ", f);
        err += PyFile_WriteString(Py_TYPE(value)->tp_name, f);
        err += PyFile_WriteString(" found\n", f);
        if (err)
            PyErr_Clear();
        return;
    }

    Py_INCREF(value);
    fflush(stdout);
    type = (PyObject *) Py_TYPE(value);
    tb = PyException_GetTraceback(value);
    if (tb && tb != Py_None)
        err = PyTraceBack_Print(tb, f);
    if (err == 0 &&
        _PyObject_HasAttrId(value, &PyId_print_file_and_line))
    {
        PyObject *message, *filename, *text;
        Py_ssize_t lineno, offset;
        if (!parse_syntax_error(value, &message, &filename,
                                &lineno, &offset, &text))
            PyErr_Clear();
        else {
            PyObject *line;

            Py_DECREF(value);
            value = message;

            line = PyUnicode_FromFormat("  File \"%S\", line %zd\n",
                                          filename, lineno);
            Py_DECREF(filename);
            if (line != NULL) {
                PyFile_WriteObject(line, f, Py_PRINT_RAW);
                Py_DECREF(line);
            }

            if (text != NULL) {
                print_error_text(f, offset, text);
                Py_DECREF(text);
            }

            /* Can't be bothered to check all those
               PyFile_WriteString() calls */
            if (PyErr_Occurred())
                err = -1;
        }
    }
    if (err) {
        /* Don't do anything else */
    }
    else {
        PyObject* moduleName;
        const char *className;
        _Py_IDENTIFIER(__module__);
        assert(PyExceptionClass_Check(type));
        className = PyExceptionClass_Name(type);
        if (className != NULL) {
            const char *dot = strrchr(className, '.');
            if (dot != NULL)
                className = dot+1;
        }

        moduleName = _PyObject_GetAttrId(type, &PyId___module__);
        if (moduleName == NULL || !PyUnicode_Check(moduleName))
        {
            Py_XDECREF(moduleName);
            err = PyFile_WriteString("<unknown>", f);
        }
        else {
            if (!_PyUnicode_EqualToASCIIId(moduleName, &PyId_builtins))
            {
                err = PyFile_WriteObject(moduleName, f, Py_PRINT_RAW);
                err += PyFile_WriteString(".", f);
            }
            Py_DECREF(moduleName);
        }
        if (err == 0) {
            if (className == NULL)
                      err = PyFile_WriteString("<unknown>", f);
            else
                      err = PyFile_WriteString(className, f);
        }
    }
    if (err == 0 && (value != Py_None)) {
        PyObject *s = PyObject_Str(value);
        /* only print colon if the str() of the
           object is not the empty string
        */
        if (s == NULL) {
            PyErr_Clear();
            err = -1;
            PyFile_WriteString(": <exception str() failed>", f);
        }
        else if (!PyUnicode_Check(s) ||
            PyUnicode_GetLength(s) != 0)
            err = PyFile_WriteString(": ", f);
        if (err == 0)
          err = PyFile_WriteObject(s, f, Py_PRINT_RAW);
        Py_XDECREF(s);
    }
    /* try to write a newline in any case */
    if (err < 0) {
        PyErr_Clear();
    }
    err += PyFile_WriteString("\n", f);
    Py_XDECREF(tb);
    Py_DECREF(value);
    /* If an error happened here, don't show it.
       XXX This is wrong, but too many callers rely on this behavior. */
    if (err != 0)
        PyErr_Clear();
}

static const char cause_message[] =
    "\nThe above exception was the direct cause "
    "of the following exception:\n\n";

static const char context_message[] =
    "\nDuring handling of the above exception, "
    "another exception occurred:\n\n";

static void
print_exception_recursive(PyObject *f, PyObject *value, PyObject *seen)
{
    int err = 0, res;
    PyObject *cause, *context;

    if (seen != NULL) {
        /* Exception chaining */
        PyObject *value_id = PyLong_FromVoidPtr(value);
        if (value_id == NULL || PySet_Add(seen, value_id) == -1)
            PyErr_Clear();
        else if (PyExceptionInstance_Check(value)) {
            PyObject *check_id = NULL;
            cause = PyException_GetCause(value);
            context = PyException_GetContext(value);
            if (cause) {
                check_id = PyLong_FromVoidPtr(cause);
                if (check_id == NULL) {
                    res = -1;
                } else {
                    res = PySet_Contains(seen, check_id);
                    Py_DECREF(check_id);
                }
                if (res == -1)
                    PyErr_Clear();
                if (res == 0) {
                    print_exception_recursive(
                        f, cause, seen);
                    err |= PyFile_WriteString(
                        cause_message, f);
                }
            }
            else if (context &&
                !((PyBaseExceptionObject *)value)->suppress_context) {
                check_id = PyLong_FromVoidPtr(context);
                if (check_id == NULL) {
                    res = -1;
                } else {
                    res = PySet_Contains(seen, check_id);
                    Py_DECREF(check_id);
                }
                if (res == -1)
                    PyErr_Clear();
                if (res == 0) {
                    print_exception_recursive(
                        f, context, seen);
                    err |= PyFile_WriteString(
                        context_message, f);
                }
            }
            Py_XDECREF(context);
            Py_XDECREF(cause);
        }
        Py_XDECREF(value_id);
    }
    print_exception(f, value);
    if (err != 0)
        PyErr_Clear();
}

void
_PyErr_Display(PyObject *file, PyObject *exception, PyObject *value, PyObject *tb)
{
    assert(file != NULL && file != Py_None);

    PyObject *seen;
    if (PyExceptionInstance_Check(value)
        && tb != NULL && PyTraceBack_Check(tb)) {
        /* Put the traceback on the exception, otherwise it won't get
           displayed.  See issue #18776. */
        PyObject *cur_tb = PyException_GetTraceback(value);
        if (cur_tb == NULL)
            PyException_SetTraceback(value, tb);
        else
            Py_DECREF(cur_tb);
    }

    /* We choose to ignore seen being possibly NULL, and report
       at least the main exception (it could be a MemoryError).
    */
    seen = PySet_New(NULL);
    if (seen == NULL) {
        PyErr_Clear();
    }
    print_exception_recursive(file, value, seen);
    Py_XDECREF(seen);

    /* Call file.flush() */
    PyObject *res = _PyObject_CallMethodIdNoArgs(file, &PyId_flush);
    if (!res) {
        /* Silently ignore file.flush() error */
        PyErr_Clear();
    }
    else {
        Py_DECREF(res);
    }
}

void
PyErr_Display(PyObject *exception, PyObject *value, PyObject *tb)
{
    PyObject *file = _PySys_GetObjectId(&PyId_stderr);
    if (file == NULL) {
        _PyObject_Dump(value);
        fprintf(stderr, "lost sys.stderr\n");
        return;
    }
    if (file == Py_None) {
        return;
    }

    _PyErr_Display(file, exception, value, tb);
}

PyObject *
PyRun_StringFlags(const char *str, int start, PyObject *globals,
                  PyObject *locals, PyCompilerFlags *flags)
{
    PyObject *ret = NULL;
    mod_ty mod;
    PyObject *filename;

    filename = _PyUnicode_FromId(&PyId_string); /* borrowed */
    if (filename == NULL)
        return NULL;

    mod = PyPegen_ASTFromStringObject(str, filename, start, flags);

    if (mod != NULL)
        ret = run_mod(mod, filename, globals, locals, flags);
    return ret;
}

PyObject *
PyRun_FileExFlags(FILE *fp, const char *filename_str, int start, PyObject *globals,
                  PyObject *locals, int closeit, PyCompilerFlags *flags)
{
    PyObject *ret = NULL;
    mod_ty mod;
    PyObject *filename;

    filename = PyUnicode_FromString(filename_str);
    
    if (filename == NULL)
        goto exit;


    mod = PyPegen_ASTFromFileObject(fp, filename, start, NULL, NULL, 
                                        flags, NULL);

    if (closeit)
        fclose(fp);
    if (mod == NULL) {
        goto exit;
    }
    ret = run_mod(mod, filename, globals, locals, flags);

exit:
    Py_XDECREF(filename);
    return ret;
}

static void
flush_io(void)
{
    PyObject *f, *r;
    PyObject *type, *value, *traceback;

    /* Save the current exception */
    PyErr_Fetch(&type, &value, &traceback);

    f = _PySys_GetObjectId(&PyId_stderr);
    if (f != NULL) {
        r = _PyObject_CallMethodIdNoArgs(f, &PyId_flush);
        if (r)
            Py_DECREF(r);
        else
            PyErr_Clear();
    }
    f = _PySys_GetObjectId(&PyId_stdout);
    if (f != NULL) {
        r = _PyObject_CallMethodIdNoArgs(f, &PyId_flush);
        if (r)
            Py_DECREF(r);
        else
            PyErr_Clear();
    }

    PyErr_Restore(type, value, traceback);
}

static PyObject *
run_eval_code_obj(PyThreadState *tstate, PyCodeObject *co, PyObject *globals, PyObject *locals)
{
    PyObject *v;
    /*
     * We explicitly re-initialize _Py_UnhandledKeyboardInterrupt every eval
     * _just in case_ someone is calling into an embedded Python where they
     * don't care about an uncaught KeyboardInterrupt exception (why didn't they
     * leave config.install_signal_handlers set to 0?!?) but then later call
     * Py_Main() itself (which _checks_ this flag and dies with a signal after
     * its interpreter exits).  We don't want a previous embedded interpreter's
     * uncaught exception to trigger an unexplained signal exit from a future
     * Py_Main() based one.
     */
    _Py_UnhandledKeyboardInterrupt = 0;

    /* Set globals['__builtins__'] if it doesn't exist */
    if (globals != NULL && PyDict_GetItemString(globals, "__builtins__") == NULL) {
        if (PyDict_SetItemString(globals, "__builtins__",
                                 tstate->interp->builtins) < 0) {
            return NULL;
        }
    }

    v = PyEval_EvalCode((PyObject*)co, globals, locals);
    if (!v && _PyErr_Occurred(tstate) == PyExc_KeyboardInterrupt) {
        _Py_UnhandledKeyboardInterrupt = 1;
    }
    return v;
}

static PyObject *
run_mod(mod_ty mod, PyObject *filename, PyObject *globals, PyObject *locals,
            PyCompilerFlags *flags)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyCodeObject *co = PyAST_CompileObject(mod, filename, flags, -1);
    if (co == NULL)
        return NULL;

    PyObject *v = run_eval_code_obj(tstate, co, globals, locals);
    Py_DECREF(co);
    return v;
}

PyObject *
Py_CompileStringObject(const char *str, PyObject *filename, int start,
                       PyCompilerFlags *flags, int optimize)
{
    PyCodeObject *co;
    mod_ty mod;

    mod = PyPegen_ASTFromStringObject(str, filename, start, flags);
    if (mod == NULL) {
        return NULL;
    }
    if (flags && (flags->cf_flags & PyCF_ONLY_AST)) {
        PyObject *result = PyAST_mod2obj(mod);
        return result;
    }
    co = PyAST_CompileObject(mod, filename, flags, optimize);
    return (PyObject *)co;
}

PyObject *
Py_CompileStringExFlags(const char *str, const char *filename_str, int start,
                        PyCompilerFlags *flags, int optimize)
{
    PyObject *filename, *co;
    filename = PyUnicode_FromString(filename_str);
    
    if (filename == NULL)
        return NULL;
    co = Py_CompileStringObject(str, filename, start, flags, optimize);
    Py_DECREF(filename);
    return co;
}

/* For use in Py_LIMITED_API */
#undef Py_CompileString
PyObject *
PyCompileString(const char *str, const char *filename, int start)
{
    return Py_CompileStringFlags(str, filename, start, NULL);
}

const char *
_Py_SourceAsString(PyObject *cmd, const char *funcname, const char *what, PyCompilerFlags *cf, PyObject **cmd_copy)
{
    const char *str;
    Py_ssize_t size;

    *cmd_copy = NULL;
    if (PyUnicode_Check(cmd)) {
        cf->cf_flags |= PyCF_IGNORE_COOKIE;
        str = PyUnicode_AsCharAndSize(cmd, &size);
        if (str == NULL)
            return NULL;
    }
    else {
        PyErr_Format(PyExc_TypeError,
            "%s() arg 1 must be a %s object",
            funcname, what);
        return NULL;
    }

    if (strlen(str) != (size_t)size) {
        PyErr_SetString(PyExc_ValueError,
            "source code string cannot contain null bytes");
        Py_CLEAR(*cmd_copy);
        return NULL;
    }
    return str;
}

struct symtable *
Py_SymtableStringObject(const char *str, PyObject *filename, int start)
{
    PyCompilerFlags flags = _PyCompilerFlags_INIT;
    return _Py_SymtableStringObjectFlags(str, filename, start, &flags);
}

struct symtable *
_Py_SymtableStringObjectFlags(const char *str, PyObject *filename, int start, PyCompilerFlags *flags)
{
    struct symtable *st;
    mod_ty mod;
    mod = PyPegen_ASTFromStringObject(str, filename, start, flags);
    if (mod == NULL) {
        return NULL;
    }
    st = PySymtable_BuildObject(mod, filename);
    return st;
}

struct symtable *
Py_SymtableString(const char *str, const char *filename_str, int start)
{
    PyObject *filename;
    struct symtable *st;

    filename = PyUnicode_FromString(filename_str);
    
    if (filename == NULL)
        return NULL;
    st = Py_SymtableStringObject(str, filename, start);
    Py_DECREF(filename);
    return st;
}

#ifdef __cplusplus
}
#endif
