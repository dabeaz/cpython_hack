
/* Module object implementation */

#include "Python.h"
#include "pycore_interp.h"        // PyInterpreterState.importlib
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "structmember.h"         // PyMemberDef

static Py_ssize_t max_module_number;

_Py_IDENTIFIER(__doc__);
_Py_IDENTIFIER(__name__);

typedef struct {
    PyObject_HEAD
    PyObject *md_dict;
    struct PyModuleDef *md_def;
    void *md_state;
    PyObject *md_weaklist;
    PyObject *md_name;  /* for logging purposes after md_dict is cleared */
} PyModuleObject;

static PyMemberDef module_members[] = {
    {"__dict__", T_OBJECT, offsetof(PyModuleObject, md_dict), READONLY},
    {0}
};


PyTypeObject PyModuleDef_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "moduledef",                                /* tp_name */
    sizeof(struct PyModuleDef),                 /* tp_basicsize */
    0,                                          /* tp_itemsize */
};


PyObject*
PyModuleDef_Init(struct PyModuleDef* def)
{
    if (PyType_Ready(&PyModuleDef_Type) < 0)
         return NULL;
    if (def->m_base.m_index == 0) {
        max_module_number++;
        Py_SET_REFCNT(def, 1);
        Py_SET_TYPE(def, &PyModuleDef_Type);
        def->m_base.m_index = max_module_number;
    }
    return (PyObject*)def;
}

static int
module_init_dict(PyModuleObject *mod, PyObject *md_dict,
                 PyObject *name, PyObject *doc)
{
    _Py_IDENTIFIER(__package__);
    _Py_IDENTIFIER(__loader__);

    if (md_dict == NULL)
        return -1;
    if (doc == NULL)
        doc = Py_None;

    if (_PyDict_SetItemId(md_dict, &PyId___name__, name) != 0)
        return -1;
    if (_PyDict_SetItemId(md_dict, &PyId___doc__, doc) != 0)
        return -1;
    if (_PyDict_SetItemId(md_dict, &PyId___package__, Py_None) != 0)
        return -1;
    if (_PyDict_SetItemId(md_dict, &PyId___loader__, Py_None) != 0)
        return -1;
    if (PyString_CheckExact(name)) {
        Py_INCREF(name);
        Py_XSETREF(mod->md_name, name);
    }

    return 0;
}


PyObject *
PyModule_NewObject(PyObject *name)
{
    PyModuleObject *m;
    m = PyObject_New(PyModuleObject, &PyModule_Type);
    if (m == NULL)
        return NULL;
    m->md_def = NULL;
    m->md_state = NULL;
    m->md_weaklist = NULL;
    m->md_name = NULL;
    m->md_dict = PyDict_New();
    if (module_init_dict(m, m->md_dict, name, NULL) != 0)
        goto fail;
    return (PyObject *)m;

 fail:
    Py_DECREF(m);
    return NULL;
}

PyObject *
PyModule_New(const char *name)
{
    PyObject *nameobj, *module;
    nameobj = PyString_FromString(name);
    if (nameobj == NULL)
        return NULL;
    module = PyModule_NewObject(nameobj);
    Py_DECREF(nameobj);
    return module;
}

/* Check API/ABI version
 * Issues a warning on mismatch, which is usually not fatal.
 * Returns 0 if an exception is raised.
 */
static int
check_api_version(const char *name, int module_api_version)
{
    return 1;
}

static int
_add_methods_to_object(PyObject *module, PyObject *name, PyMethodDef *functions)
{
    PyObject *func;
    PyMethodDef *fdef;

    for (fdef = functions; fdef->ml_name != NULL; fdef++) {
        if ((fdef->ml_flags & METH_CLASS) ||
            (fdef->ml_flags & METH_STATIC)) {
            PyErr_SetString(PyExc_ValueError,
                            "module functions cannot set"
                            " METH_CLASS or METH_STATIC");
            return -1;
        }
        func = PyCFunction_NewEx(fdef, (PyObject*)module, name);
        if (func == NULL) {
            return -1;
        }
        if (PyObject_SetAttrString(module, fdef->ml_name, func) != 0) {
            Py_DECREF(func);
            return -1;
        }
        Py_DECREF(func);
    }

    return 0;
}

PyObject *
PyModule_Create2(struct PyModuleDef* module, int module_api_version)
{
    if (!_PyImport_IsInitialized(_PyInterpreterState_GET())) {
        PyErr_SetString(PyExc_SystemError,
                        "Python import machinery not initialized");
        return NULL;
    }
    return _PyModule_CreateInitialized(module, module_api_version);
}

PyObject *
_PyModule_CreateInitialized(struct PyModuleDef* module, int module_api_version)
{
    const char* name;
    PyModuleObject *m;

    if (!PyModuleDef_Init(module))
        return NULL;
    name = module->m_name;
    if (!check_api_version(name, module_api_version)) {
        return NULL;
    }
    if (module->m_slots) {
        PyErr_Format(
            PyExc_SystemError,
            "module %s: PyModule_Create is incompatible with m_slots", name);
        return NULL;
    }
    /* Make sure name is fully qualified.

       This is a bit of a hack: when the shared library is loaded,
       the module name is "package.module", but the module calls
       PyModule_Create*() with just "module" for the name.  The shared
       library loader squirrels away the true name of the module in
       _Py_PackageContext, and PyModule_Create*() will substitute this
       (if the name actually matches).
    */
    if (_Py_PackageContext != NULL) {
        const char *p = strrchr(_Py_PackageContext, '.');
        if (p != NULL && strcmp(module->m_name, p+1) == 0) {
            name = _Py_PackageContext;
            _Py_PackageContext = NULL;
        }
    }
    if ((m = (PyModuleObject*)PyModule_New(name)) == NULL)
        return NULL;

    if (module->m_size > 0) {
        m->md_state = PyMem_Malloc(module->m_size);
        if (!m->md_state) {
            PyErr_NoMemory();
            Py_DECREF(m);
            return NULL;
        }
        memset(m->md_state, 0, module->m_size);
    }

    if (module->m_methods != NULL) {
        if (PyModule_AddFunctions((PyObject *) m, module->m_methods) != 0) {
            Py_DECREF(m);
            return NULL;
        }
    }
    if (module->m_doc != NULL) {
        if (PyModule_SetDocString((PyObject *) m, module->m_doc) != 0) {
            Py_DECREF(m);
            return NULL;
        }
    }
    m->md_def = module;
    return (PyObject*)m;
}

PyObject *
PyModule_FromDefAndName2(struct PyModuleDef* def, PyObject *nameobj, int module_api_version)
{
    PyModuleDef_Slot* cur_slot;
    PyObject *(*create)(PyObject *, PyModuleDef*) = NULL;
    PyObject *m = NULL;
    int has_execution_slots = 0;
    const char *name;
    int ret;

    PyModuleDef_Init(def);
    name = PyString_AsChar(nameobj);
    if (name == NULL) {
        goto error;
    }

    if (!check_api_version(name, module_api_version)) {
        goto error;
    }

    if (def->m_size < 0) {
        PyErr_Format(
            PyExc_SystemError,
            "module %s: m_size may not be negative for multi-phase initialization",
            name);
        goto error;
    }

    for (cur_slot = def->m_slots; cur_slot && cur_slot->slot; cur_slot++) {
        if (cur_slot->slot == Py_mod_create) {
            if (create) {
                PyErr_Format(
                    PyExc_SystemError,
                    "module %s has multiple create slots",
                    name);
                goto error;
            }
            create = cur_slot->value;
        } else if (cur_slot->slot < 0 || cur_slot->slot > _Py_mod_LAST_SLOT) {
            PyErr_Format(
                PyExc_SystemError,
                "module %s uses unknown slot ID %i",
                name, cur_slot->slot);
            goto error;
        } else {
            has_execution_slots = 1;
        }
    }
      {
        m = PyModule_NewObject(nameobj);
        if (m == NULL) {
            goto error;
        }
    }

    if (PyModule_Check(m)) {
        ((PyModuleObject*)m)->md_state = NULL;
        ((PyModuleObject*)m)->md_def = def;
    } else {
        if (def->m_size > 0 || def->m_traverse || def->m_clear || def->m_free) {
            PyErr_Format(
                PyExc_SystemError,
                "module %s is not a module object, but requests module state",
                name);
            goto error;
        }
        if (has_execution_slots) {
            PyErr_Format(
                PyExc_SystemError,
                "module %s specifies execution slots, but did not create "
                    "a ModuleType instance",
                name);
            goto error;
        }
    }

    if (def->m_methods != NULL) {
        ret = _add_methods_to_object(m, nameobj, def->m_methods);
        if (ret != 0) {
            goto error;
        }
    }

    if (def->m_doc != NULL) {
        ret = PyModule_SetDocString(m, def->m_doc);
        if (ret != 0) {
            goto error;
        }
    }
    return m;

error:
    Py_XDECREF(m);
    return NULL;
}


int
PyModule_ExecDef(PyObject *module, PyModuleDef *def)
{
    PyModuleDef_Slot *cur_slot;
    const char *name;
    int ret;

    name = PyModule_GetName(module);
    if (name == NULL) {
        return -1;
    }

    if (def->m_size >= 0) {
        PyModuleObject *md = (PyModuleObject*)module;
        if (md->md_state == NULL) {
            /* Always set a state pointer; this serves as a marker to skip
             * multiple initialization (importlib.reload() is no-op) */
            md->md_state = PyMem_Malloc(def->m_size);
            if (!md->md_state) {
                PyErr_NoMemory();
                return -1;
            }
            memset(md->md_state, 0, def->m_size);
        }
    }

    if (def->m_slots == NULL) {
        return 0;
    }

    for (cur_slot = def->m_slots; cur_slot && cur_slot->slot; cur_slot++) {
        switch (cur_slot->slot) {
            case Py_mod_create:
                /* handled in PyModule_FromDefAndSpec2 */
                break;
            case Py_mod_exec:
                ret = ((int (*)(PyObject *))cur_slot->value)(module);
                if (ret != 0) {
                    if (!PyErr_Occurred()) {
                        PyErr_Format(
                            PyExc_SystemError,
                            "execution of module %s failed without setting an exception",
                            name);
                    }
                    return -1;
                }
                if (PyErr_Occurred()) {
                    PyErr_Format(
                        PyExc_SystemError,
                        "execution of module %s raised unreported exception",
                        name);
                    return -1;
                }
                break;
            default:
                PyErr_Format(
                    PyExc_SystemError,
                    "module %s initialized with unknown slot %i",
                    name, cur_slot->slot);
                return -1;
        }
    }
    return 0;
}

int
PyModule_AddFunctions(PyObject *m, PyMethodDef *functions)
{
    int res;
    PyObject *name = PyModule_GetNameObject(m);
    if (name == NULL) {
        return -1;
    }

    res = _add_methods_to_object(m, name, functions);
    Py_DECREF(name);
    return res;
}

int
PyModule_SetDocString(PyObject *m, const char *doc)
{
    PyObject *v;

    v = PyString_FromString(doc);
    if (v == NULL || _PyObject_SetAttrId(m, &PyId___doc__, v) != 0) {
        Py_XDECREF(v);
        return -1;
    }
    Py_DECREF(v);
    return 0;
}

PyObject *
PyModule_GetDict(PyObject *m)
{
    PyObject *d;
    if (!PyModule_Check(m)) {
        PyErr_BadInternalCall();
        return NULL;
    }
    d = ((PyModuleObject *)m) -> md_dict;
    assert(d != NULL);
    return d;
}

PyObject*
PyModule_GetNameObject(PyObject *m)
{
    PyObject *d;
    PyObject *name;
    if (!PyModule_Check(m)) {
        PyErr_BadArgument();
        return NULL;
    }
    d = ((PyModuleObject *)m)->md_dict;
    if (d == NULL ||
        (name = _PyDict_GetItemId(d, &PyId___name__)) == NULL ||
        !PyString_Check(name))
    {
        PyErr_SetString(PyExc_SystemError, "nameless module");
        return NULL;
    }
    Py_INCREF(name);
    return name;
}

const char *
PyModule_GetName(PyObject *m)
{
    PyObject *name = PyModule_GetNameObject(m);
    if (name == NULL)
        return NULL;
    Py_DECREF(name);   /* module dict has still a reference */
    return PyString_AsChar(name);
}

PyObject*
PyModule_GetFilenameObject(PyObject *m)
{
    _Py_IDENTIFIER(__file__);
    PyObject *d;
    PyObject *fileobj;
    if (!PyModule_Check(m)) {
        PyErr_BadArgument();
        return NULL;
    }
    d = ((PyModuleObject *)m)->md_dict;
    if (d == NULL ||
        (fileobj = _PyDict_GetItemId(d, &PyId___file__)) == NULL ||
        !PyString_Check(fileobj))
    {
        PyErr_SetString(PyExc_SystemError, "module filename missing");
        return NULL;
    }
    Py_INCREF(fileobj);
    return fileobj;
}

PyModuleDef*
PyModule_GetDef(PyObject* m)
{
    if (!PyModule_Check(m)) {
        PyErr_BadArgument();
        return NULL;
    }
    return ((PyModuleObject *)m)->md_def;
}

void*
PyModule_GetState(PyObject* m)
{
    if (!PyModule_Check(m)) {
        PyErr_BadArgument();
        return NULL;
    }
    return ((PyModuleObject *)m)->md_state;
}

void
_PyModule_Clear(PyObject *m)
{
    PyObject *d = ((PyModuleObject *)m)->md_dict;
    if (d != NULL)
        _PyModule_ClearDict(d);
}

void
_PyModule_ClearDict(PyObject *d)
{
    /* To make the execution order of destructors for global
       objects a bit more predictable, we first zap all objects
       whose name starts with a single underscore, before we clear
       the entire dictionary.  We zap them by replacing them with
       None, rather than deleting them from the dictionary, to
       avoid rehashing the dictionary (to some extent). */

    Py_ssize_t pos;
    PyObject *key, *value;

    /* First, clear only names starting with a single underscore */
    pos = 0;
    while (PyDict_Next(d, &pos, &key, &value)) {
        if (value != Py_None && PyString_Check(key)) {
            if (PyString_ReadChar(key, 0) == '_' &&
                PyString_ReadChar(key, 1) != '_') {
                if (PyDict_SetItem(d, key, Py_None) != 0) {
                    PyErr_WriteUnraisable(NULL);
                }
            }
        }
    }

    /* Next, clear all names except for __builtins__ */
    pos = 0;
    while (PyDict_Next(d, &pos, &key, &value)) {
        if (value != Py_None && PyString_Check(key)) {
            if (PyString_ReadChar(key, 0) != '_' ||
                !_PyString_EqualToASCIIString(key, "__builtins__"))
            {
                if (PyDict_SetItem(d, key, Py_None) != 0) {
                    PyErr_WriteUnraisable(NULL);
                }
            }
        }
    }

    /* Note: we leave __builtins__ in place, so that destructors
       of non-global objects defined in this module can still use
       builtins, in particularly 'None'. */

}

/*[clinic input]
class module "PyModuleObject *" "&PyModule_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=3e35d4f708ecb6af]*/

/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(module___init____doc__,
"module(name, doc=None)\n"
"--\n"
"\n"
"Create a module object.\n"
"\n"
"The name must be a string; the optional doc argument can have any type.");

static int
module___init___impl(PyModuleObject *self, PyObject *name, PyObject *doc);

static int
module___init__(PyObject *self, PyObject *args, PyObject *kwargs)
{
    int return_value = -1;
    static const char * const _keywords[] = {"name", "doc", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "module", 0};
    PyObject *argsbuf[2];
    PyObject * const *fastargs;
    Py_ssize_t nargs = PyTuple_Size(args);
    Py_ssize_t noptargs = nargs + (kwargs ? PyDict_Size(kwargs) : 0) - 1;
    PyObject *name;
    PyObject *doc = Py_None;

    fastargs = _PyArg_UnpackKeywords(PyTuple_Items(args), nargs, kwargs, NULL, &_parser, 1, 2, 0, argsbuf);
    if (!fastargs) {
        goto exit;
    }
    if (!PyString_Check(fastargs[0])) {
        _PyArg_BadArgument("module", "argument 'name'", "str", fastargs[0]);
        goto exit;
    }
    name = fastargs[0];
    if (!noptargs) {
        goto skip_optional_pos;
    }
    doc = fastargs[1];
skip_optional_pos:
    return_value = module___init___impl((PyModuleObject *)self, name, doc);

exit:
    return return_value;
}
/*[clinic end generated code: output=680276bc3a496d7a input=a9049054013a1b77]*/


/* Methods */

/*[clinic input]
module.__init__
    name: unicode
    doc: object = None

Create a module object.

The name must be a string; the optional doc argument can have any type.
[clinic start generated code]*/

static int
module___init___impl(PyModuleObject *self, PyObject *name, PyObject *doc)
/*[clinic end generated code: output=e7e721c26ce7aad7 input=57f9e177401e5e1e]*/
{
    PyObject *dict = self->md_dict;
    if (dict == NULL) {
        dict = PyDict_New();
        if (dict == NULL)
            return -1;
        self->md_dict = dict;
    }
    if (module_init_dict(self, dict, name, doc) < 0)
        return -1;
    return 0;
}

static void
module_dealloc(PyModuleObject *m)
{
    if (m->md_weaklist != NULL)
        PyObject_ClearWeakRefs((PyObject *) m);
    /* bpo-39824: Don't call m_free() if m_size > 0 and md_state=NULL */
    if (m->md_def && m->md_def->m_free
        && (m->md_def->m_size <= 0 || m->md_state != NULL))
    {
        m->md_def->m_free(m);
    }
    Py_XDECREF(m->md_dict);
    Py_XDECREF(m->md_name);
    if (m->md_state != NULL)
        PyMem_Free(m->md_state);
    Py_TYPE(m)->tp_free((PyObject *)m);
}

static PyObject *
module_repr(PyModuleObject *m)
{
  PyObject *r;
  char *temp;
  int size = PyString_Size(m->md_name);
  temp = PyMem_Malloc(size + 50);
  sprintf(temp, "<module '%s'>", PyString_AsChar(m->md_name));
  r = PyString_FromString(temp);
  PyMem_Free(temp);
  return r;
}

static PyObject*
module_getattro(PyModuleObject *m, PyObject *name)
{
    PyObject *attr, *mod_name, *getattr;
    attr = PyObject_GenericGetAttr((PyObject *)m, name);
    if (attr || !PyErr_ExceptionMatches(PyExc_AttributeError)) {
        return attr;
    }
    PyErr_Clear();
    if (m->md_dict) {
        _Py_IDENTIFIER(__getattr__);
        getattr = _PyDict_GetItemId(m->md_dict, &PyId___getattr__);
        if (getattr) {
            return PyObject_CallOneArg(getattr, name);
        }
        mod_name = _PyDict_GetItemId(m->md_dict, &PyId___name__);
    }
    PyErr_Format(PyExc_AttributeError,
                "module has no attribute '%U'", name);
    return NULL;
}

static int
module_clear(PyModuleObject *m)
{
    /* bpo-39824: Don't call m_clear() if m_size > 0 and md_state=NULL */
    if (m->md_def && m->md_def->m_clear
        && (m->md_def->m_size <= 0 || m->md_state != NULL))
    {
        int res = m->md_def->m_clear((PyObject*)m);
        if (PyErr_Occurred()) {
            PySys_FormatStderr("Exception ignored in m_clear of module%s%V\n",
                               m->md_name ? " " : "",
                               m->md_name, "");
            PyErr_WriteUnraisable(NULL);
        }
        if (res)
            return res;
    }
    Py_CLEAR(m->md_dict);
    return 0;
}

static PyObject *
module_dir(PyObject *self, PyObject *args)
{
    _Py_IDENTIFIER(__dict__);
    _Py_IDENTIFIER(__dir__);
    PyObject *result = NULL;
    PyObject *dict = _PyObject_GetAttrId(self, &PyId___dict__);

    if (dict != NULL) {
        if (PyDict_Check(dict)) {
            PyObject *dirfunc = _PyDict_GetItemIdWithError(dict, &PyId___dir__);
            if (dirfunc) {
                result = _PyObject_CallNoArg(dirfunc);
            }
            else if (!PyErr_Occurred()) {
                result = PyDict_Keys(dict);
            }
        }
        else {
            const char *name = PyModule_GetName(self);
            if (name)
                PyErr_Format(PyExc_TypeError,
                             "%.200s.__dict__ is not a dictionary",
                             name);
        }
    }

    Py_XDECREF(dict);
    return result;
}

static PyMethodDef module_methods[] = {
    {"__dir__", module_dir, METH_NOARGS,
     PyDoc_STR("__dir__() -> list\nspecialized dir() implementation")},
    {0}
};

PyTypeObject PyModule_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "module",                                   /* tp_name */
    sizeof(PyModuleObject),                     /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)module_dealloc,                 /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    (reprfunc)module_repr,                      /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    (getattrofunc)module_getattro,              /* tp_getattro */
    PyObject_GenericSetAttr,                    /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | //Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,                    /* tp_flags */
    module___init____doc__,                     /* tp_doc */
    0,               /* tp_traverse */
    (inquiry)module_clear,                      /* tp_clear */
    0,                                          /* tp_richcompare */
    offsetof(PyModuleObject, md_weaklist),      /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    module_methods,                             /* tp_methods */
    module_members,                             /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    offsetof(PyModuleObject, md_dict),          /* tp_dictoffset */
    module___init__,                            /* tp_init */
    PyType_GenericAlloc,                        /* tp_alloc */
    PyType_GenericNew,                          /* tp_new */
    PyMem_Free,                            /* tp_free */
};
