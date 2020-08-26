/* ------------------------------------------------------------------------

   Python Codec Registry and support functions

Written by Marc-Andre Lemburg (mal@lemburg.com).

Copyright (c) Corporation for National Research Initiatives.

   ------------------------------------------------------------------------ */

#include "Python.h"
#include "pycore_interp.h"        // PyInterpreterState.codec_search_path
#include "pycore_pystate.h"       // _PyInterpreterState_GET()
#include "ucnhash.h"
#include <ctype.h>

const char *Py_hexdigits = "0123456789abcdef";

/* --- Codec Registry ----------------------------------------------------- */

/* Import the standard encodings package which will register the first
   codec search function.

   This is done in a lazy way so that the Unicode implementation does
   not downgrade startup time of scripts not needing it.

   ImportErrors are silently ignored by this function. Only one try is
   made.

*/

static int _PyCodecRegistry_Init(void); /* Forward */

int PyCodec_Register(PyObject *search_function)
{
    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (interp->codec_search_path == NULL && _PyCodecRegistry_Init())
        goto onError;
    if (search_function == NULL) {
        PyErr_BadArgument();
        goto onError;
    }
    if (!PyCallable_Check(search_function)) {
        PyErr_SetString(PyExc_TypeError, "argument must be callable");
        goto onError;
    }
    return PyList_Append(interp->codec_search_path, search_function);

 onError:
    return -1;
}

extern int _Py_normalize_encoding(const char *, char *, size_t);

/* Convert a string to a normalized Python string(decoded from UTF-8): all characters are
   converted to lower case, spaces and hyphens are replaced with underscores. */

static
PyObject *normalizestring(const char *string)
{
    size_t len = strlen(string);
    char *encoding;
    PyObject *v;

    if (len > PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError, "string is too large");
        return NULL;
    }

    encoding = PyMem_Malloc(len + 1);
    if (encoding == NULL)
        return PyErr_NoMemory();

    if (!_Py_normalize_encoding(string, encoding, len + 1))
    {
        PyErr_SetString(PyExc_RuntimeError, "_Py_normalize_encoding() failed");
        PyMem_Free(encoding);
        return NULL;
    }

    v = PyUnicode_FromString(encoding);
    PyMem_Free(encoding);
    return v;
}

/* Lookup the given encoding and return a tuple providing the codec
   facilities.

   The encoding string is looked up converted to all lower-case
   characters. This makes encodings looked up through this mechanism
   effectively case-insensitive.

   If no codec is found, a LookupError is set and NULL returned.

   As side effect, this tries to load the encodings package, if not
   yet done. This is part of the lazy load strategy for the encodings
   package.

*/

PyObject *_PyCodec_Lookup(const char *encoding)
{
  encoding = "latin1";
  
    if (encoding == NULL) {
        PyErr_BadArgument();
        return NULL;
    }

    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (interp->codec_search_path == NULL && _PyCodecRegistry_Init()) {
        return NULL;
    }

    /* Convert the encoding to a normalized Python string: all
       characters are converted to lower case, spaces and hyphens are
       replaced with underscores. */
    PyObject *v = normalizestring(encoding);
    if (v == NULL) {
        return NULL;
    }
    PyUnicode_InternInPlace(&v);

    /* First, try to lookup the name in the registry dictionary */
    PyObject *result = PyDict_GetItemWithError(interp->codec_search_cache, v);
    if (result != NULL) {
        Py_INCREF(result);
        Py_DECREF(v);
        return result;
    }
    else if (PyErr_Occurred()) {
        goto onError;
    }

    /* Next, scan the search functions in order of registration */
    const Py_ssize_t len = PyList_Size(interp->codec_search_path);
    if (len < 0)
        goto onError;
    if (len == 0) {
        PyErr_SetString(PyExc_LookupError,
                        "no codec search functions registered: "
                        "can't find encoding");
        goto onError;
    }

    Py_ssize_t i;
    for (i = 0; i < len; i++) {
        PyObject *func;

        func = PyList_GetItem(interp->codec_search_path, i);
        if (func == NULL)
            goto onError;
        result = PyObject_CallOneArg(func, v);
        if (result == NULL)
            goto onError;
        if (result == Py_None) {
            Py_DECREF(result);
            continue;
        }
        if (!PyTuple_Check(result) || PyTuple_GET_SIZE(result) != 4) {
            PyErr_SetString(PyExc_TypeError,
                            "codec search functions must return 4-tuples");
            Py_DECREF(result);
            goto onError;
        }
        break;
    }
    if (i == len) {
        /* XXX Perhaps we should cache misses too ? */
        PyErr_Format(PyExc_LookupError,
                     "unknown encoding: %s", encoding);
        goto onError;
    }

    /* Cache and return the result */
    if (PyDict_SetItem(interp->codec_search_cache, v, result) < 0) {
        Py_DECREF(result);
        goto onError;
    }
    Py_DECREF(v);
    return result;

 onError:
    Py_DECREF(v);
    return NULL;
}

int _PyCodec_Forget(const char *encoding)
{
    PyObject *v;
    int result;

    PyInterpreterState *interp = _PyInterpreterState_GET();
    if (interp->codec_search_path == NULL) {
        return -1;
    }

    /* Convert the encoding to a normalized Python string: all
       characters are converted to lower case, spaces and hyphens are
       replaced with underscores. */
    v = normalizestring(encoding);
    if (v == NULL) {
        return -1;
    }

    /* Drop the named codec from the internal cache */
    result = PyDict_DelItem(interp->codec_search_cache, v);
    Py_DECREF(v);

    return result;
}

/* Codec registry encoding check API. */

int PyCodec_KnownEncoding(const char *encoding)
{
    PyObject *codecs;

    codecs = _PyCodec_Lookup(encoding);
    if (!codecs) {
        PyErr_Clear();
        return 0;
    }
    else {
        Py_DECREF(codecs);
        return 1;
    }
}

static
PyObject *args_tuple(PyObject *object,
                     const char *errors)
{
    PyObject *args;

    args = PyTuple_New(1 + (errors != NULL));
    if (args == NULL)
        return NULL;
    Py_INCREF(object);
    PyTuple_SET_ITEM(args,0,object);
    if (errors) {
        PyObject *v;

        v = PyUnicode_FromString(errors);
        if (v == NULL) {
            Py_DECREF(args);
            return NULL;
        }
        PyTuple_SET_ITEM(args, 1, v);
    }
    return args;
}

/* Helper function to get a codec item */

static
PyObject *codec_getitem(const char *encoding, int index)
{
    PyObject *codecs;
    PyObject *v;

    codecs = _PyCodec_Lookup(encoding);
    if (codecs == NULL)
        return NULL;
    v = PyTuple_GET_ITEM(codecs, index);
    Py_DECREF(codecs);
    Py_INCREF(v);
    return v;
}

/* Helper functions to create an incremental codec. */
static
PyObject *codec_makeincrementalcodec(PyObject *codec_info,
                                     const char *errors,
                                     const char *attrname)
{
    PyObject *ret, *inccodec;

    inccodec = PyObject_GetAttrString(codec_info, attrname);
    if (inccodec == NULL)
        return NULL;
    if (errors)
        ret = PyObject_CallFunction(inccodec, "s", errors);
    else
        ret = _PyObject_CallNoArg(inccodec);
    Py_DECREF(inccodec);
    return ret;
}

static
PyObject *codec_getincrementalcodec(const char *encoding,
                                    const char *errors,
                                    const char *attrname)
{
    PyObject *codec_info, *ret;

    codec_info = _PyCodec_Lookup(encoding);
    if (codec_info == NULL)
        return NULL;
    ret = codec_makeincrementalcodec(codec_info, errors, attrname);
    Py_DECREF(codec_info);
    return ret;
}

/* Helper function to create a stream codec. */

static
PyObject *codec_getstreamcodec(const char *encoding,
                               PyObject *stream,
                               const char *errors,
                               const int index)
{
    PyObject *codecs, *streamcodec, *codeccls;

    codecs = _PyCodec_Lookup(encoding);
    if (codecs == NULL)
        return NULL;

    codeccls = PyTuple_GET_ITEM(codecs, index);
    if (errors != NULL)
        streamcodec = PyObject_CallFunction(codeccls, "Os", stream, errors);
    else
        streamcodec = PyObject_CallOneArg(codeccls, stream);
    Py_DECREF(codecs);
    return streamcodec;
}

/* Helpers to work with the result of _PyCodec_Lookup

 */
PyObject *_PyCodecInfo_GetIncrementalDecoder(PyObject *codec_info,
                                             const char *errors)
{
    return codec_makeincrementalcodec(codec_info, errors,
                                      "incrementaldecoder");
}

PyObject *_PyCodecInfo_GetIncrementalEncoder(PyObject *codec_info,
                                             const char *errors)
{
    return codec_makeincrementalcodec(codec_info, errors,
                                      "incrementalencoder");
}


/* Convenience APIs to query the Codec registry.

   All APIs return a codec object with incremented refcount.

 */

PyObject *PyCodec_Encoder(const char *encoding)
{
    return codec_getitem(encoding, 0);
}

PyObject *PyCodec_Decoder(const char *encoding)
{
    return codec_getitem(encoding, 1);
}

PyObject *PyCodec_IncrementalEncoder(const char *encoding,
                                     const char *errors)
{
    return codec_getincrementalcodec(encoding, errors, "incrementalencoder");
}

PyObject *PyCodec_IncrementalDecoder(const char *encoding,
                                     const char *errors)
{
    return codec_getincrementalcodec(encoding, errors, "incrementaldecoder");
}

PyObject *PyCodec_StreamReader(const char *encoding,
                               PyObject *stream,
                               const char *errors)
{
    return codec_getstreamcodec(encoding, stream, errors, 2);
}

PyObject *PyCodec_StreamWriter(const char *encoding,
                               PyObject *stream,
                               const char *errors)
{
    return codec_getstreamcodec(encoding, stream, errors, 3);
}

/* Helper that tries to ensure the reported exception chain indicates the
 * codec that was invoked to trigger the failure without changing the type
 * of the exception raised.
 */
static void
wrap_codec_error(const char *operation,
                 const char *encoding)
{
    /* TrySetFromCause will replace the active exception with a suitably
     * updated clone if it can, otherwise it will leave the original
     * exception alone.
     */
    _PyErr_TrySetFromCause("%s with '%s' codec failed",
                           operation, encoding);
}

/* Encode an object (e.g. a Unicode object) using the given encoding
   and return the resulting encoded object (usually a Python string).

   errors is passed to the encoder factory as argument if non-NULL. */

static PyObject *
_PyCodec_EncodeInternal(PyObject *object,
                        PyObject *encoder,
                        const char *encoding,
                        const char *errors)
{
    PyObject *args = NULL, *result = NULL;
    PyObject *v = NULL;

    args = args_tuple(object, errors);
    if (args == NULL)
        goto onError;

    result = PyObject_Call(encoder, args, NULL);
    if (result == NULL) {
        wrap_codec_error("encoding", encoding);
        goto onError;
    }

    if (!PyTuple_Check(result) ||
        PyTuple_GET_SIZE(result) != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "encoder must return a tuple (object, integer)");
        goto onError;
    }
    v = PyTuple_GET_ITEM(result,0);
    Py_INCREF(v);
    /* We don't check or use the second (integer) entry. */

    Py_DECREF(args);
    Py_DECREF(encoder);
    Py_DECREF(result);
    return v;

 onError:
    Py_XDECREF(result);
    Py_XDECREF(args);
    Py_XDECREF(encoder);
    return NULL;
}

/* Decode an object (usually a Python string) using the given encoding
   and return an equivalent object (e.g. a Unicode object).

   errors is passed to the decoder factory as argument if non-NULL. */

static PyObject *
_PyCodec_DecodeInternal(PyObject *object,
                        PyObject *decoder,
                        const char *encoding,
                        const char *errors)
{
    PyObject *args = NULL, *result = NULL;
    PyObject *v;

    args = args_tuple(object, errors);
    if (args == NULL)
        goto onError;

    result = PyObject_Call(decoder, args, NULL);
    if (result == NULL) {
        wrap_codec_error("decoding", encoding);
        goto onError;
    }
    if (!PyTuple_Check(result) ||
        PyTuple_GET_SIZE(result) != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "decoder must return a tuple (object,integer)");
        goto onError;
    }
    v = PyTuple_GET_ITEM(result,0);
    Py_INCREF(v);
    /* We don't check or use the second (integer) entry. */

    Py_DECREF(args);
    Py_DECREF(decoder);
    Py_DECREF(result);
    return v;

 onError:
    Py_XDECREF(args);
    Py_XDECREF(decoder);
    Py_XDECREF(result);
    return NULL;
}

/* Generic encoding/decoding API */
PyObject *PyCodec_Encode(PyObject *object,
                         const char *encoding,
                         const char *errors)
{
  PyObject *encoder = NULL;
    encoder = PyCodec_Encoder(encoding);
    if (encoder == NULL)
        return NULL;
    return _PyCodec_EncodeInternal(object, encoder, encoding, errors);
}

PyObject *PyCodec_Decode(PyObject *object,
                         const char *encoding,
                         const char *errors)
{
  PyObject *decoder = NULL;
    decoder = PyCodec_Decoder(encoding);
    if (decoder == NULL)
        return NULL;
    return _PyCodec_DecodeInternal(object, decoder, encoding, errors);
}

/* Text encoding/decoding API */
PyObject * _PyCodec_LookupTextEncoding(const char *encoding,
                                       const char *alternate_command)
{
    _Py_IDENTIFIER(_is_text_encoding);
    PyObject *codec;
    PyObject *attr;
    int is_text_codec;

    codec = _PyCodec_Lookup(encoding);
    if (codec == NULL)
        return NULL;

    /* Backwards compatibility: assume any raw tuple describes a text
     * encoding, and the same for anything lacking the private
     * attribute.
     */
    if (!PyTuple_CheckExact(codec)) {
        if (_PyObject_LookupAttrId(codec, &PyId__is_text_encoding, &attr) < 0) {
            Py_DECREF(codec);
            return NULL;
        }
        if (attr != NULL) {
            is_text_codec = PyObject_IsTrue(attr);
            Py_DECREF(attr);
            if (is_text_codec <= 0) {
                Py_DECREF(codec);
                if (!is_text_codec)
                    PyErr_Format(PyExc_LookupError,
                                 "'%.400s' is not a text encoding; "
                                 "use %s to handle arbitrary codecs",
                                 encoding, alternate_command);
                return NULL;
            }
        }
    }

    /* This appears to be a valid text encoding */
    return codec;
}


static
PyObject *codec_getitem_checked(const char *encoding,
                                const char *alternate_command,
                                int index)
{
    PyObject *codec;
    PyObject *v;

    codec = _PyCodec_LookupTextEncoding(encoding, alternate_command);
    if (codec == NULL)
        return NULL;

    v = PyTuple_GET_ITEM(codec, index);
    Py_INCREF(v);
    Py_DECREF(codec);
    return v;
}

static PyObject * _PyCodec_TextEncoder(const char *encoding)
{
    return codec_getitem_checked(encoding, "codecs.encode()", 0);
}

static PyObject * _PyCodec_TextDecoder(const char *encoding)
{
    return codec_getitem_checked(encoding, "codecs.decode()", 1);
}

PyObject *_PyCodec_EncodeText(PyObject *object,
                              const char *encoding,
                              const char *errors)
{
  PyObject *encoder = NULL;
    encoder = _PyCodec_TextEncoder(encoding);
    if (encoder == NULL)
        return NULL;
    return _PyCodec_EncodeInternal(object, encoder, encoding, errors);
}

PyObject *_PyCodec_DecodeText(PyObject *object,
                              const char *encoding,
                              const char *errors)
{
    PyObject *decoder = NULL;
    decoder = _PyCodec_TextDecoder(encoding);
    if (decoder == NULL)
        return NULL;
    return _PyCodec_DecodeInternal(object, decoder, encoding, errors);
}


PyObject *PyCodec_StrictErrors(PyObject *exc)
{
    if (PyExceptionInstance_Check(exc))
        PyErr_SetObject(PyExceptionInstance_Class(exc), exc);
    else
        PyErr_SetString(PyExc_TypeError, "codec must pass exception instance");
    return NULL;
}

static int _PyCodecRegistry_Init(void)
{
    
    PyInterpreterState *interp = _PyInterpreterState_GET();
    PyObject *mod;

    if (interp->codec_search_path != NULL)
        return 0;

    interp->codec_search_path = PyList_New(0);
    if (interp->codec_search_path == NULL) {
        return -1;
    }

    interp->codec_search_cache = PyDict_New();
    if (interp->codec_search_cache == NULL) {
        return -1;
    }
    
    mod = PyImport_ImportModuleNoBlock("encodings");
    if (mod == NULL) {
        return -1;
    }
    Py_DECREF(mod);
    interp->codecs_initialized = 1;
    return 0;
}
