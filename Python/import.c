/* Module definition and import implementation */

#include "Python.h"

#include "Python-ast.h"
#undef Yield   /* undefine macro conflicting with <winbase.h> */
#include "pycore_initconfig.h"
#include "pycore_pyerrors.h"
#include "pycore_pyhash.h"
#include "pycore_pylifecycle.h"
#include "pycore_pymem.h"         // _PyMem_SetDefaultAllocator()
#include "pycore_interp.h"        // _PyInterpreterState_ClearModules()
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "pycore_sysmodule.h"
#include "errcode.h"
#include "code.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* Forward references */
static PyObject *import_add_module(PyThreadState *tstate, PyObject *name);

/* See _PyImport_FixupExtensionObject() below */
static PyObject *extensions = NULL;

/* This table is defined in config.c: */
extern struct _inittab _PyImport_Inittab[];

struct _inittab *PyImport_Inittab = _PyImport_Inittab;
static struct _inittab *inittab_copy = NULL;
  
void
_PyImport_Fini(void)
{
}

void
_PyImport_Fini2(void)
{
    /* Use the same memory allocator than PyImport_ExtendInittab(). */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    /* Free memory allocated by PyImport_ExtendInittab() */
    PyMem_RawFree(inittab_copy);
    inittab_copy = NULL;

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
}

/* Helper for sys */

PyObject *
PyImport_GetModuleDict(void)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (interp->modules == NULL) {
        Py_FatalError("interpreter has no modules dictionary");
    }
    return interp->modules;
}

/* In some corner cases it is important to be sure that the import
   machinery has been initialized (or not cleaned up yet).  For
   example, see issue #4236 and PyModule_Create2(). */

int
_PyImport_IsInitialized(PyInterpreterState *interp)
{
    if (interp->modules == NULL)
        return 0;
    return 1;
}

PyObject *
_PyImport_GetModuleId(struct _Py_Identifier *nameid)
{
    PyObject *name = _PyUnicode_FromId(nameid); /* borrowed */
    if (name == NULL) {
        return NULL;
    }
    return PyImport_GetModule(name);
}

int
_PyImport_SetModule(PyObject *name, PyObject *m)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *modules = tstate->interp->modules;
    return PyObject_SetItem(modules, name, m);
}

int
_PyImport_SetModuleString(const char *name, PyObject *m)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *modules = tstate->interp->modules;
    return PyMapping_SetItemString(modules, name, m);
}

static PyObject *
import_get_module(PyThreadState *tstate, PyObject *name)
{
    PyObject *modules = tstate->interp->modules;
    if (modules == NULL) {
        _PyErr_SetString(tstate, PyExc_RuntimeError,
                         "unable to get sys.modules");
        return NULL;
    }

    PyObject *m;
    Py_INCREF(modules);
    if (PyDict_CheckExact(modules)) {
        m = PyDict_GetItemWithError(modules, name);  /* borrowed */
        Py_XINCREF(m);
    }
    else {
        m = PyObject_GetItem(modules, name);
        if (m == NULL && _PyErr_ExceptionMatches(tstate, PyExc_KeyError)) {
            _PyErr_Clear(tstate);
        }
    }
    Py_DECREF(modules);
    return m;
}

/* List of names to clear in sys */
static const char * const sys_deletes[] = {
    "path", "argv", "ps1", "ps2",
    "last_type", "last_value", "last_traceback",
    "path_hooks", "path_importer_cache", "meta_path",
    "__interactivehook__",
    NULL
};

static const char * const sys_files[] = {
    "stdin", "__stdin__",
    "stdout", "__stdout__",
    "stderr", "__stderr__",
    NULL
};

/* Un-initialize things, as good as we can */

void
_PyImport_Cleanup(PyThreadState *tstate)
{
    PyInterpreterState *interp = tstate->interp;
    PyObject *modules = interp->modules;
    if (modules == NULL) {
        /* Already done */
        return;
    }

    /* Delete some special variables first.  These are common
       places where user values hide and people complain when their
       destructors fail.  Since the modules containing them are
       deleted *last* of all, they would come too late in the normal
       destruction order.  Sigh. */

    /* XXX Perhaps these precautions are obsolete. Who knows? */
    if (PyDict_SetItemString(interp->builtins, "_", Py_None) < 0) {
        PyErr_WriteUnraisable(NULL);
    }

    const char * const *p;
    for (p = sys_deletes; *p != NULL; p++) {
        if (PyDict_SetItemString(interp->sysdict, *p, Py_None) < 0) {
            PyErr_WriteUnraisable(NULL);
        }
    }
    for (p = sys_files; *p != NULL; p+=2) {
        PyObject *value = _PyDict_GetItemStringWithError(interp->sysdict,
                                                         *(p+1));
        if (value == NULL) {
            if (_PyErr_Occurred(tstate)) {
                PyErr_WriteUnraisable(NULL);
            }
            value = Py_None;
        }
        if (PyDict_SetItemString(interp->sysdict, *p, value) < 0) {
            PyErr_WriteUnraisable(NULL);
        }
    }

    /* We prepare a list which will receive (name, weakref) tuples of
       modules when they are removed from sys.modules.  The name is used
       for diagnosis messages (in verbose mode), while the weakref helps
       detect those modules which have been held alive. */
    PyObject *weaklist = PyList_New(0);
    if (weaklist == NULL) {
        PyErr_WriteUnraisable(NULL);
    }

#define STORE_MODULE_WEAKREF(name, mod) \
    if (weaklist != NULL) { \
        PyObject *wr = PyWeakref_NewRef(mod, NULL); \
        if (wr) { \
            PyObject *tup = PyTuple_Pack(2, name, wr); \
            if (!tup || PyList_Append(weaklist, tup) < 0) { \
                PyErr_WriteUnraisable(NULL); \
            } \
            Py_XDECREF(tup); \
            Py_DECREF(wr); \
        } \
        else { \
            PyErr_WriteUnraisable(NULL); \
        } \
    }
#define CLEAR_MODULE(name, mod) \
    if (PyModule_Check(mod)) { \
        STORE_MODULE_WEAKREF(name, mod); \
        if (PyObject_SetItem(modules, name, Py_None) < 0) { \
            PyErr_WriteUnraisable(NULL); \
        } \
    }

    /* Remove all modules from sys.modules, hoping that garbage collection
       can reclaim most of them. */
    if (PyDict_CheckExact(modules)) {
        Py_ssize_t pos = 0;
        PyObject *key, *value;
        while (PyDict_Next(modules, &pos, &key, &value)) {
            CLEAR_MODULE(key, value);
        }
    }
    else {
        PyObject *iterator = PyObject_GetIter(modules);
        if (iterator == NULL) {
            PyErr_WriteUnraisable(NULL);
        }
        else {
            PyObject *key;
            while ((key = PyIter_Next(iterator))) {
                PyObject *value = PyObject_GetItem(modules, key);
                if (value == NULL) {
                    PyErr_WriteUnraisable(NULL);
                    continue;
                }
                CLEAR_MODULE(key, value);
                Py_DECREF(value);
                Py_DECREF(key);
            }
            if (PyErr_Occurred()) {
                PyErr_WriteUnraisable(NULL);
            }
            Py_DECREF(iterator);
        }
    }

    /* Clear the modules dict. */
    if (PyDict_CheckExact(modules)) {
        PyDict_Clear(modules);
    }
    else {
        _Py_IDENTIFIER(clear);
        if (_PyObject_CallMethodIdNoArgs(modules, &PyId_clear) == NULL) {
            PyErr_WriteUnraisable(NULL);
        }
    }
    /* Restore the original builtins dict, to ensure that any
       user data gets cleared. */
    PyObject *dict = PyDict_Copy(interp->builtins);
    if (dict == NULL) {
        PyErr_WriteUnraisable(NULL);
    }
    PyDict_Clear(interp->builtins);
    if (PyDict_Update(interp->builtins, interp->builtins_copy)) {
        _PyErr_Clear(tstate);
    }
    Py_XDECREF(dict);
    /* Collect references */
    
    /* Now, if there are any modules left alive, clear their globals to
       minimize potential leaks.  All C extension modules actually end
       up here, since they are kept alive in the interpreter state.

       The special treatment of "builtins" here is because even
       when it's not referenced as a module, its dictionary is
       referenced by almost every module's __builtins__.  Since
       deleting a module clears its dictionary (even if there are
       references left to it), we need to delete the "builtins"
       module last.  Likewise, we don't delete sys until the very
       end because it is implicitly referenced (e.g. by print). */
    if (weaklist != NULL) {
        Py_ssize_t i;
        /* Since dict is ordered in CPython 3.6+, modules are saved in
           importing order.  First clear modules imported later. */
        for (i = PyList_GET_SIZE(weaklist) - 1; i >= 0; i--) {
            PyObject *tup = PyList_GET_ITEM(weaklist, i);
            PyObject *mod = PyWeakref_GET_OBJECT(PyTuple_GET_ITEM(tup, 1));
            if (mod == Py_None)
                continue;
            assert(PyModule_Check(mod));
            dict = PyModule_GetDict(mod);
            if (dict == interp->builtins || dict == interp->sysdict)
                continue;
            Py_INCREF(mod);
            _PyModule_Clear(mod);
            Py_DECREF(mod);
        }
        Py_DECREF(weaklist);
    }

    /* Next, delete sys and builtins (in that order) */
    _PyModule_ClearDict(interp->sysdict);
    _PyModule_ClearDict(interp->builtins);

    /* Clear module dict copies stored in the interpreter state */
    _PyInterpreterState_ClearModules(interp);

    /* Clear and delete the modules directory.  Actual modules will
       still be there only if imported during the execution of some
       destructor. */
    interp->modules = NULL;
    Py_DECREF(modules);

#undef CLEAR_MODULE
#undef STORE_MODULE_WEAKREF
}


/* Helper for pythonrun.c -- return magic number and tag. */

/* Magic for extension modules (built-in as well as dynamically
   loaded).  To prevent initializing an extension module more than
   once, we keep a static dictionary 'extensions' keyed by the tuple
   (module name, module name)  (for built-in modules) or by
   (filename, module name) (for dynamically loaded modules), containing these
   modules.  A copy of the module's dictionary is stored by calling
   _PyImport_FixupExtensionObject() immediately after the module initialization
   function succeeds.  A copy can be retrieved from there by calling
   _PyImport_FindExtensionObject().

   Modules which do support multiple initialization set their m_size
   field to a non-negative number (indicating the size of the
   module-specific state). They are still recorded in the extensions
   dictionary, to avoid loading shared libraries twice.
*/

int
_PyImport_FixupExtensionObject(PyObject *mod, PyObject *name,
                               PyObject *filename, PyObject *modules)
{
    if (mod == NULL || !PyModule_Check(mod)) {
        PyErr_BadInternalCall();
        return -1;
    }

    struct PyModuleDef *def = PyModule_GetDef(mod);
    if (!def) {
        PyErr_BadInternalCall();
        return -1;
    }

    PyThreadState *tstate = PyThreadState_Get();
    if (PyObject_SetItem(modules, name, mod) < 0) {
        return -1;
    }
    if (_PyState_AddModule(tstate, mod, def) < 0) {
        PyMapping_DelItem(modules, name);
        return -1;
    }

    if (1) {
        if (def->m_size == -1) {
            if (def->m_base.m_copy) {
                /* Somebody already imported the module,
                   likely under a different name.
                   XXX this should really not happen. */
                Py_CLEAR(def->m_base.m_copy);
            }
            PyObject *dict = PyModule_GetDict(mod);
            if (dict == NULL) {
                return -1;
            }
            def->m_base.m_copy = PyDict_Copy(dict);
            if (def->m_base.m_copy == NULL) {
                return -1;
            }
        }

        if (extensions == NULL) {
            extensions = PyDict_New();
            if (extensions == NULL) {
                return -1;
            }
        }

        PyObject *key = PyTuple_Pack(2, filename, name);
        if (key == NULL) {
            return -1;
        }
        int res = PyDict_SetItem(extensions, key, (PyObject *)def);
        Py_DECREF(key);
        if (res < 0) {
            return -1;
        }
    }

    return 0;
}

int
_PyImport_FixupBuiltin(PyObject *mod, const char *name, PyObject *modules)
{
    int res;
    PyObject *nameobj;
    nameobj = PyUnicode_InternFromString(name);
    if (nameobj == NULL)
        return -1;
    res = _PyImport_FixupExtensionObject(mod, nameobj, nameobj, modules);
    Py_DECREF(nameobj);
    return res;
}

static PyObject *
import_find_extension(PyThreadState *tstate, PyObject *name,
                      PyObject *filename)
{
    if (extensions == NULL) {
        return NULL;
    }

    PyObject *key = PyTuple_Pack(2, filename, name);
    if (key == NULL) {
        return NULL;
    }
    PyModuleDef* def = (PyModuleDef *)PyDict_GetItemWithError(extensions, key);
    Py_DECREF(key);
    if (def == NULL) {
        return NULL;
    }

    PyObject *mod, *mdict;
    PyObject *modules = tstate->interp->modules;

    if (def->m_size == -1) {
        /* Module does not support repeated initialization */
        if (def->m_base.m_copy == NULL)
            return NULL;
        mod = import_add_module(tstate, name);
        if (mod == NULL)
            return NULL;
        mdict = PyModule_GetDict(mod);
        if (mdict == NULL)
            return NULL;
        if (PyDict_Update(mdict, def->m_base.m_copy))
            return NULL;
    }
    else {
        if (def->m_base.m_init == NULL)
            return NULL;
        mod = def->m_base.m_init();
        if (mod == NULL)
            return NULL;
        if (PyObject_SetItem(modules, name, mod) == -1) {
            Py_DECREF(mod);
            return NULL;
        }
        Py_DECREF(mod);
    }
    if (_PyState_AddModule(tstate, mod, def) < 0) {
        PyMapping_DelItem(modules, name);
        return NULL;
    }
    return mod;
}

PyObject *
_PyImport_FindExtensionObject(PyObject *name, PyObject *filename)
{
    PyThreadState *tstate = PyThreadState_Get();
    return import_find_extension(tstate, name, filename);
}


PyObject *
_PyImport_FindBuiltin(PyThreadState *tstate, const char *name)
{
    PyObject *res, *nameobj;
    nameobj = PyUnicode_InternFromString(name);
    if (nameobj == NULL)
        return NULL;
    res = import_find_extension(tstate, nameobj, nameobj);
    Py_DECREF(nameobj);
    return res;
}

/* Get the module object corresponding to a module name.
   First check the modules dictionary if there's one there,
   if not, create a new one and insert it in the modules dictionary.
   Because the former action is most common, THIS DOES NOT RETURN A
   'NEW' REFERENCE! */

static PyObject *
import_add_module(PyThreadState *tstate, PyObject *name)
{
    PyObject *modules = tstate->interp->modules;
    if (modules == NULL) {
        _PyErr_SetString(tstate, PyExc_RuntimeError,
                         "no import module dictionary");
        return NULL;
    }

    PyObject *m;
    if (PyDict_CheckExact(modules)) {
        m = PyDict_GetItemWithError(modules, name);
    }
    else {
        m = PyObject_GetItem(modules, name);
        // For backward-compatibility we copy the behavior
        // of PyDict_GetItemWithError().
        if (_PyErr_ExceptionMatches(tstate, PyExc_KeyError)) {
            _PyErr_Clear(tstate);
        }
    }
    if (_PyErr_Occurred(tstate)) {
        return NULL;
    }
    if (m != NULL && PyModule_Check(m)) {
        return m;
    }
    m = PyModule_NewObject(name);
    if (m == NULL)
        return NULL;
    if (PyObject_SetItem(modules, name, m) != 0) {
        Py_DECREF(m);
        return NULL;
    }
    Py_DECREF(m); /* Yes, it still exists, in modules! */

    return m;
}

PyObject *
PyImport_AddModuleObject(PyObject *name)
{
    PyThreadState *tstate = PyThreadState_Get();
    return import_add_module(tstate, name);
}


PyObject *
PyImport_AddModule(const char *name)
{
    PyObject *nameobj = PyUnicode_FromString(name);
    if (nameobj == NULL) {
        return NULL;
    }
    PyObject *module = PyImport_AddModuleObject(nameobj);
    Py_DECREF(nameobj);
    return module;
}


/* Remove name from sys.modules, if it's there. */
static void
remove_module(PyThreadState *tstate, PyObject *name)
{
    PyObject *type, *value, *traceback;
    _PyErr_Fetch(tstate, &type, &value, &traceback);

    PyObject *modules = tstate->interp->modules;
    if (!PyMapping_HasKey(modules, name)) {
        goto out;
    }
    if (PyMapping_DelItem(modules, name) < 0) {
        _PyErr_SetString(tstate, PyExc_RuntimeError,
                         "deleting key in sys.modules failed");
        _PyErr_ChainExceptions(type, value, traceback);
        return;
    }

out:
    _PyErr_Restore(tstate, type, value, traceback);
}

/* Helper to test for built-in module */

static int
is_builtin(PyObject *name)
{
    int i;
    for (i = 0; PyImport_Inittab[i].name != NULL; i++) {
        if (_PyUnicode_EqualToASCIIString(name, PyImport_Inittab[i].name)) {
            if (PyImport_Inittab[i].initfunc == NULL)
                return -1;
            else
                return 1;
        }
    }
    return 0;
}


/* Return a finder object for a sys.path/pkg.__path__ item 'p',
   possibly by fetching it from the path_importer_cache dict. If it
   wasn't yet cached, traverse path_hooks until a hook is found
   that can handle the path item. Return None if no hook could;
   this tells our caller that the path based finder could not find
   a finder for this path item. Cache the result in
   path_importer_cache.
   Returns a borrowed reference. */

static PyObject *
get_path_importer(PyThreadState *tstate, PyObject *path_importer_cache,
                  PyObject *path_hooks, PyObject *p)
{
    PyObject *importer;
    Py_ssize_t j, nhooks;

    /* These conditions are the caller's responsibility: */
    assert(PyList_Check(path_hooks));
    assert(PyDict_Check(path_importer_cache));

    nhooks = PyList_Size(path_hooks);
    if (nhooks < 0)
        return NULL; /* Shouldn't happen */

    importer = PyDict_GetItemWithError(path_importer_cache, p);
    if (importer != NULL || _PyErr_Occurred(tstate))
        return importer;

    /* set path_importer_cache[p] to None to avoid recursion */
    if (PyDict_SetItem(path_importer_cache, p, Py_None) != 0)
        return NULL;

    for (j = 0; j < nhooks; j++) {
        PyObject *hook = PyList_GetItem(path_hooks, j);
        if (hook == NULL)
            return NULL;
        importer = PyObject_CallOneArg(hook, p);
        if (importer != NULL)
            break;

        if (!_PyErr_ExceptionMatches(tstate, PyExc_ImportError)) {
            return NULL;
        }
        _PyErr_Clear(tstate);
    }
    if (importer == NULL) {
        return Py_None;
    }
    if (importer != NULL) {
        int err = PyDict_SetItem(path_importer_cache, p, importer);
        Py_DECREF(importer);
        if (err != 0)
            return NULL;
    }
    return importer;
}

PyObject *
PyImport_GetImporter(PyObject *path)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *importer=NULL, *path_importer_cache=NULL, *path_hooks=NULL;

    path_importer_cache = PySys_GetObject("path_importer_cache");
    path_hooks = PySys_GetObject("path_hooks");
    if (path_importer_cache != NULL && path_hooks != NULL) {
        importer = get_path_importer(tstate, path_importer_cache,
                                     path_hooks, path);
    }
    Py_XINCREF(importer); /* get_path_importer returns a borrowed reference */
    return importer;
}

/* Import a module, either built-in, frozen, or external, and return
   its module object WITH INCREMENTED REFERENCE COUNT */

PyObject *
PyImport_ImportModule(const char *name)
{
    PyObject *pname;
    PyObject *result;

    pname = PyUnicode_FromString(name);
    if (pname == NULL)
        return NULL;
    result = PyImport_Import(pname);
    Py_DECREF(pname);
    return result;
}


/* Import a module without blocking
 *
 * At first it tries to fetch the module from sys.modules. If the module was
 * never loaded before it loads it with PyImport_ImportModule() unless another
 * thread holds the import lock. In the latter case the function raises an
 * ImportError instead of blocking.
 *
 * Returns the module object with incremented ref count.
 */
PyObject *
PyImport_ImportModuleNoBlock(const char *name)
{
    return PyImport_ImportModule(name);
}


/* Remove importlib frames from the traceback,
 * except in Verbose mode. */
static void
remove_importlib_frames(PyThreadState *tstate)
{
    const char *importlib_filename = "<frozen importlib._bootstrap>";
    const char *external_filename = "<frozen importlib._bootstrap_external>";
    const char *remove_frames = "_call_with_frames_removed";
    int always_trim = 0;
    int in_importlib = 0;
    PyObject *exception, *value, *base_tb, *tb;
    PyObject **prev_link, **outer_link = NULL;

    /* Synopsis: if it's an ImportError, we trim all importlib chunks
       from the traceback. We always trim chunks
       which end with a call to "_call_with_frames_removed". */

    _PyErr_Fetch(tstate, &exception, &value, &base_tb);
    if (!exception) {
        goto done;
    }

    if (PyType_IsSubtype((PyTypeObject *) exception,
                         (PyTypeObject *) PyExc_ImportError))
        always_trim = 1;

    prev_link = &base_tb;
    tb = base_tb;
    while (tb != NULL) {
        PyTracebackObject *traceback = (PyTracebackObject *)tb;
        PyObject *next = (PyObject *) traceback->tb_next;
        PyFrameObject *frame = traceback->tb_frame;
        PyCodeObject *code = PyFrame_GetCode(frame);
        int now_in_importlib;

        assert(PyTraceBack_Check(tb));
        now_in_importlib = _PyUnicode_EqualToASCIIString(code->co_filename, importlib_filename) ||
                           _PyUnicode_EqualToASCIIString(code->co_filename, external_filename);
        if (now_in_importlib && !in_importlib) {
            /* This is the link to this chunk of importlib tracebacks */
            outer_link = prev_link;
        }
        in_importlib = now_in_importlib;

        if (in_importlib &&
            (always_trim ||
             _PyUnicode_EqualToASCIIString(code->co_name, remove_frames))) {
            Py_XINCREF(next);
            Py_XSETREF(*outer_link, next);
            prev_link = outer_link;
        }
        else {
            prev_link = (PyObject **) &traceback->tb_next;
        }
        Py_DECREF(code);
        tb = next;
    }
done:
    _PyErr_Restore(tstate, exception, value, base_tb);
}


#include "osdefs.h"

static PyObject *
import_find_and_load(PyThreadState *tstate, PyObject *abs_name)
{
    PyObject *mod = NULL;
    PyObject *sys_path = PySys_GetObject("path");

    /* Replacement for the import statement */
    /* See if the module is in the module cache */
    mod = import_get_module(tstate, abs_name);
    if (mod) {
      return mod;
    }
    PyErr_Clear();
    
    /* Built-in modules */
    if (is_builtin(abs_name)) {
      struct _inittab *p;
      for (p = PyImport_Inittab; p->name != NULL; p++) {
	if (_PyUnicode_EqualToASCIIString(abs_name, p->name)) {
	  if (p->initfunc == 0) {
	    mod = PyImport_AddModule(PyUnicode_AsChar(abs_name));
	    return mod;
	  }
	  mod = (*p->initfunc)();
	  if (mod == NULL) {
	    return NULL;
	  }
	  /* If it's actually a moduleDef object. */
	  if (PyObject_TypeCheck(mod, &PyModuleDef_Type)) {
	    PyModuleDef *def = (PyModuleDef *) mod;
	    mod = PyModule_FromDefAndName((PyModuleDef *) mod, abs_name);
	    PyModule_ExecDef(mod, def);
	    return mod;
	  } else {
	    PyModuleDef *def = PyModule_GetDef(mod);
	    PyModule_ExecDef(mod, def);
	    return mod;
	  }
	}
      }
    }
    /* Python modules */
    {
      Py_ssize_t n = 0;
      Py_ssize_t len = PyList_GET_SIZE(sys_path);
      for (n = 0; n < len; n++) {
	char name[MAXPATHLEN];
	PyObject *p = PySequence_GetItem(sys_path, n);
	if (PyUnicode_GET_SIZE(p) < MAXPATHLEN - PyUnicode_GET_SIZE(abs_name) - 5) {
	  int fd;
	  struct _Py_stat_struct stat;
	  if (PyUnicode_GET_SIZE(p) > 0) {
	    strcpy(name, PyUnicode_AsChar(p));
	    strcat(name, "/");
	  } else {
	    strcpy(name, "");
	  }
	  strcat(name, PyUnicode_AsChar(abs_name));
	  strcat(name, ".py");
	  fd = _Py_open(name, O_RDONLY);
	  if (fd > 0) {
	    PyObject *src;
	    PyObject *dict;
	    PyObject *v;
	    _Py_fstat(fd, &stat);
	    src = PyUnicode_New(stat.st_size);
	    _Py_read(fd, (char *) PyUnicode_AsChar(src), stat.st_size);
	    close(fd);
	    mod = PyImport_AddModuleObject(abs_name);
	    if (mod == NULL) {
	      return NULL;
	    }
	    dict = PyModule_GetDict(mod);
	    PyDict_SetItemString(dict, "__file__", PyUnicode_FromString(name));
	    v = PyRun_String(PyUnicode_AsChar(src), Py_file_input, dict, dict);
	    if (v == NULL) {
	      return NULL;
	    }
	    return mod;
	  } else {
	    _PyErr_Clear(tstate);
	  }
	  
	}
	Py_XDECREF(p);
      }
    }
    _PyErr_SetObject(tstate, PyExc_ImportError, abs_name);
    return NULL;
}

PyObject *
PyImport_GetModule(PyObject *name)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *mod;

    mod = import_get_module(tstate, name);
    return mod;
}

PyObject *
PyImport_ImportModuleLevelObject(PyObject *name, PyObject *globals,
                                 PyObject *locals, PyObject *fromlist,
                                 int level)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *abs_name = NULL;
    PyObject *final_mod = NULL;
    PyObject *mod = NULL;
    PyObject *package = NULL;
    int has_from;


    if (name == NULL) {
        _PyErr_SetString(tstate, PyExc_ValueError, "Empty module name");
        goto error;
    }
    /* The below code is importlib.__import__() & _gcd_import(), ported to C
       for added performance. */

    if (!PyUnicode_Check(name)) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "module name must be a string");
        goto error;
    }
    if (level < 0) {
        _PyErr_SetString(tstate, PyExc_ValueError, "level must be >= 0");
        goto error;
    }

    if (level > 0) {
        _PyErr_SetString(tstate, PyExc_ValueError, "level must be == 0");
	goto error;
    }
    else {  /* level == 0 */
        if (PyUnicode_GET_LENGTH(name) == 0) {
            _PyErr_SetString(tstate, PyExc_ValueError, "Empty module name");
            goto error;
        }
        abs_name = name;
        Py_INCREF(abs_name);
    }


    
    mod = import_get_module(tstate, abs_name);
    if (mod == NULL && _PyErr_Occurred(tstate)) {
        goto error;
    }
    Py_XDECREF(mod);


    mod = import_find_and_load(tstate, abs_name);
    if (mod == NULL) {
      goto error;
    }

    has_from = 0;
    if (fromlist != NULL && fromlist != Py_None) {
        has_from = PyObject_IsTrue(fromlist);
        if (has_from < 0)
            goto error;
    }

    if (!has_from) {
        Py_ssize_t len = PyUnicode_GET_LENGTH(name);
        if (level == 0 || len > 0) {
            Py_ssize_t dot;
            dot = PyUnicode_FindChar(name, '.', 0, len, 1);
            if (dot == -2) {
                goto error;
            }

            if (dot == -1) {
                /* No dot in module name, simple exit */
                final_mod = mod;
                Py_INCREF(mod);
                goto error;
            }


	
            if (level == 0) {
                PyObject *front = PyUnicode_Substring(name, 0, dot);
                if (front == NULL) {
                    goto error;
                }


                final_mod = PyImport_ImportModuleLevelObject(front, NULL, NULL, NULL, 0);
                Py_DECREF(front);
            }
            else {
                Py_ssize_t cut_off = len - dot;
                Py_ssize_t abs_name_len = PyUnicode_GET_LENGTH(abs_name);
                PyObject *to_return = PyUnicode_Substring(abs_name, 0,
                                                        abs_name_len - cut_off);
                if (to_return == NULL) {
                    goto error;
                }

                final_mod = import_get_module(tstate, to_return);
                Py_DECREF(to_return);
                if (final_mod == NULL) {
                    if (!_PyErr_Occurred(tstate)) {
                        _PyErr_Format(tstate, PyExc_KeyError,
                                      "%R not in sys.modules as expected",
                                      to_return);
                    }
                    goto error;
                }
            }
        }
        else {
            final_mod = mod;
            Py_INCREF(mod);
        }
    }
    else {
      final_mod = mod;
      Py_INCREF(mod);
    }

  error:
    Py_XDECREF(abs_name);
    Py_XDECREF(mod);
    Py_XDECREF(package);
    if (final_mod == NULL) {
        remove_importlib_frames(tstate);
    }
    return final_mod;
}

PyObject *
PyImport_ImportModuleLevel(const char *name, PyObject *globals, PyObject *locals,
                           PyObject *fromlist, int level)
{
    PyObject *nameobj, *mod;
    nameobj = PyUnicode_FromString(name);
    if (nameobj == NULL)
        return NULL;
    mod = PyImport_ImportModuleLevelObject(nameobj, globals, locals,
                                           fromlist, level);

    Py_DECREF(nameobj);
    return mod;
}

/* Higher-level import emulator which emulates the "import" statement
   more accurately -- it invokes the __import__() function from the
   builtins of the current globals.  This means that the import is
   done using whatever import hooks are installed in the current
   environment.
   A dummy list ["__doc__"] is passed as the 4th argument so that
   e.g. PyImport_Import(PyUnicode_FromString("win32com.client.gencache"))
   will return <module "gencache"> instead of <module "win32com">. */

PyObject *
PyImport_Import(PyObject *module_name)
{
    PyThreadState *tstate = PyThreadState_Get();
    PyObject *r = NULL;

    r = PyImport_ImportModuleLevelObject(module_name, NULL, NULL, NULL, 0);
    if (r == NULL) {
      return NULL;
    }
    r = import_get_module(tstate, module_name);
    if (r == NULL && !_PyErr_Occurred(tstate)) {
        _PyErr_SetObject(tstate, PyExc_KeyError, module_name);
    }
    return r;
}

#ifdef __cplusplus
}
#endif
