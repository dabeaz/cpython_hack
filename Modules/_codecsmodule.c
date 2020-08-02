/* ------------------------------------------------------------------------

   _codecs -- Provides access to the codec registry and the builtin
              codecs.

   This module should never be imported directly. The standard library
   module "codecs" wraps this builtin module for use within Python.

   The codec registry is accessible via:

     register(search_function) -> None

     lookup(encoding) -> CodecInfo object

   The builtin Unicode codecs use the following interface:

     <encoding>_encode(Unicode_object[,errors='strict']) ->
        (string object, bytes consumed)

     <encoding>_decode(char_buffer_obj[,errors='strict']) ->
        (Unicode object, bytes consumed)

   These <encoding>s are available: utf_8, unicode_escape,
   raw_unicode_escape, latin_1, ascii (7-bit), mbcs (on win32).


Written by Marc-Andre Lemburg (mal@lemburg.com).

Copyright (c) Corporation for National Research Initiatives.

   ------------------------------------------------------------------------ */

#define PY_SSIZE_T_CLEAN
#include "Python.h"

/*[clinic input]
module _codecs
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=e1390e3da3cb9deb]*/

/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(_codecs_register__doc__,
"register($module, search_function, /)\n"
"--\n"
"\n"
"Register a codec search function.\n"
"\n"
"Search functions are expected to take one argument, the encoding name in\n"
"all lower case letters, and either return None, or a tuple of functions\n"
"(encoder, decoder, stream_reader, stream_writer) (or a CodecInfo object).");

#define _CODECS_REGISTER_METHODDEF    \
    {"register", (PyCFunction)_codecs_register, METH_O, _codecs_register__doc__},

PyDoc_STRVAR(_codecs_lookup__doc__,
"lookup($module, encoding, /)\n"
"--\n"
"\n"
"Looks up a codec tuple in the Python codec registry and returns a CodecInfo object.");

#define _CODECS_LOOKUP_METHODDEF    \
    {"lookup", (PyCFunction)_codecs_lookup, METH_O, _codecs_lookup__doc__},

static PyObject *
_codecs_lookup_impl(PyObject *module, const char *encoding);

static PyObject *
_codecs_lookup(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    const char *encoding;

    if (!PyUnicode_Check(arg)) {
        _PyArg_BadArgument("lookup", "argument", "str", arg);
        goto exit;
    }
    Py_ssize_t encoding_length;
    encoding = PyUnicode_AsUTF8AndSize(arg, &encoding_length);
    if (encoding == NULL) {
        goto exit;
    }
    if (strlen(encoding) != (size_t)encoding_length) {
        PyErr_SetString(PyExc_ValueError, "embedded null character");
        goto exit;
    }
    return_value = _codecs_lookup_impl(module, encoding);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_encode__doc__,
"encode($module, /, obj, encoding=\'utf-8\', errors=\'strict\')\n"
"--\n"
"\n"
"Encodes obj using the codec registered for encoding.\n"
"\n"
"The default encoding is \'utf-8\'.  errors may be given to set a\n"
"different error handling scheme.  Default is \'strict\' meaning that encoding\n"
"errors raise a ValueError.  Other possible values are \'ignore\', \'replace\'\n"
"and \'backslashreplace\' as well as any other name registered with\n"
"codecs.register_error that can handle ValueErrors.");

#define _CODECS_ENCODE_METHODDEF    \
    {"encode", (PyCFunction)(void(*)(void))_codecs_encode, METH_FASTCALL|METH_KEYWORDS, _codecs_encode__doc__},

static PyObject *
_codecs_encode_impl(PyObject *module, PyObject *obj, const char *encoding,
                    const char *errors);

static PyObject *
_codecs_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"obj", "encoding", "errors", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "encode", 0};
    PyObject *argsbuf[3];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    PyObject *obj;
    const char *encoding = NULL;
    const char *errors = NULL;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 3, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    obj = args[0];
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[1]) {
        if (!PyUnicode_Check(args[1])) {
            _PyArg_BadArgument("encode", "argument 'encoding'", "str", args[1]);
            goto exit;
        }
        Py_ssize_t encoding_length;
        encoding = PyUnicode_AsUTF8AndSize(args[1], &encoding_length);
        if (encoding == NULL) {
            goto exit;
        }
        if (strlen(encoding) != (size_t)encoding_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    if (!PyUnicode_Check(args[2])) {
        _PyArg_BadArgument("encode", "argument 'errors'", "str", args[2]);
        goto exit;
    }
    Py_ssize_t errors_length;
    errors = PyUnicode_AsUTF8AndSize(args[2], &errors_length);
    if (errors == NULL) {
        goto exit;
    }
    if (strlen(errors) != (size_t)errors_length) {
        PyErr_SetString(PyExc_ValueError, "embedded null character");
        goto exit;
    }
skip_optional_pos:
    return_value = _codecs_encode_impl(module, obj, encoding, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_decode__doc__,
"decode($module, /, obj, encoding=\'utf-8\', errors=\'strict\')\n"
"--\n"
"\n"
"Decodes obj using the codec registered for encoding.\n"
"\n"
"Default encoding is \'utf-8\'.  errors may be given to set a\n"
"different error handling scheme.  Default is \'strict\' meaning that encoding\n"
"errors raise a ValueError.  Other possible values are \'ignore\', \'replace\'\n"
"and \'backslashreplace\' as well as any other name registered with\n"
"codecs.register_error that can handle ValueErrors.");

#define _CODECS_DECODE_METHODDEF    \
    {"decode", (PyCFunction)(void(*)(void))_codecs_decode, METH_FASTCALL|METH_KEYWORDS, _codecs_decode__doc__},

static PyObject *
_codecs_decode_impl(PyObject *module, PyObject *obj, const char *encoding,
                    const char *errors);

static PyObject *
_codecs_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"obj", "encoding", "errors", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "decode", 0};
    PyObject *argsbuf[3];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    PyObject *obj;
    const char *encoding = NULL;
    const char *errors = NULL;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 1, 3, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    obj = args[0];
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[1]) {
        if (!PyUnicode_Check(args[1])) {
            _PyArg_BadArgument("decode", "argument 'encoding'", "str", args[1]);
            goto exit;
        }
        Py_ssize_t encoding_length;
        encoding = PyUnicode_AsUTF8AndSize(args[1], &encoding_length);
        if (encoding == NULL) {
            goto exit;
        }
        if (strlen(encoding) != (size_t)encoding_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    if (!PyUnicode_Check(args[2])) {
        _PyArg_BadArgument("decode", "argument 'errors'", "str", args[2]);
        goto exit;
    }
    Py_ssize_t errors_length;
    errors = PyUnicode_AsUTF8AndSize(args[2], &errors_length);
    if (errors == NULL) {
        goto exit;
    }
    if (strlen(errors) != (size_t)errors_length) {
        PyErr_SetString(PyExc_ValueError, "embedded null character");
        goto exit;
    }
skip_optional_pos:
    return_value = _codecs_decode_impl(module, obj, encoding, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs__forget_codec__doc__,
"_forget_codec($module, encoding, /)\n"
"--\n"
"\n"
"Purge the named codec from the internal codec lookup cache");

#define _CODECS__FORGET_CODEC_METHODDEF    \
    {"_forget_codec", (PyCFunction)_codecs__forget_codec, METH_O, _codecs__forget_codec__doc__},

static PyObject *
_codecs__forget_codec_impl(PyObject *module, const char *encoding);

static PyObject *
_codecs__forget_codec(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    const char *encoding;

    if (!PyUnicode_Check(arg)) {
        _PyArg_BadArgument("_forget_codec", "argument", "str", arg);
        goto exit;
    }
    Py_ssize_t encoding_length;
    encoding = PyUnicode_AsUTF8AndSize(arg, &encoding_length);
    if (encoding == NULL) {
        goto exit;
    }
    if (strlen(encoding) != (size_t)encoding_length) {
        PyErr_SetString(PyExc_ValueError, "embedded null character");
        goto exit;
    }
    return_value = _codecs__forget_codec_impl(module, encoding);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_escape_decode__doc__,
"escape_decode($module, data, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_ESCAPE_DECODE_METHODDEF    \
    {"escape_decode", (PyCFunction)(void(*)(void))_codecs_escape_decode, METH_FASTCALL, _codecs_escape_decode__doc__},

static PyObject *
_codecs_escape_decode_impl(PyObject *module, Py_buffer *data,
                           const char *errors);

static PyObject *
_codecs_escape_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("escape_decode", nargs, 1, 2)) {
        goto exit;
    }
    if (PyUnicode_Check(args[0])) {
        Py_ssize_t len;
        const char *ptr = PyUnicode_AsUTF8AndSize(args[0], &len);
        if (ptr == NULL) {
            goto exit;
        }
        PyBuffer_FillInfo(&data, args[0], (void *)ptr, len, 1, 0);
    }
    else { /* any bytes-like object */
        if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
            goto exit;
        }
        if (!PyBuffer_IsContiguous(&data, 'C')) {
            _PyArg_BadArgument("escape_decode", "argument 1", "contiguous buffer", args[0]);
            goto exit;
        }
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("escape_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_escape_decode_impl(module, &data, errors);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_escape_encode__doc__,
"escape_encode($module, data, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_ESCAPE_ENCODE_METHODDEF    \
    {"escape_encode", (PyCFunction)(void(*)(void))_codecs_escape_encode, METH_FASTCALL, _codecs_escape_encode__doc__},

static PyObject *
_codecs_escape_encode_impl(PyObject *module, PyObject *data,
                           const char *errors);

static PyObject *
_codecs_escape_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *data;
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("escape_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (!PyBytes_Check(args[0])) {
        _PyArg_BadArgument("escape_encode", "argument 1", "bytes", args[0]);
        goto exit;
    }
    data = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("escape_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_escape_encode_impl(module, data, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_utf_7_decode__doc__,
"utf_7_decode($module, data, errors=None, final=False, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_7_DECODE_METHODDEF    \
    {"utf_7_decode", (PyCFunction)(void(*)(void))_codecs_utf_7_decode, METH_FASTCALL, _codecs_utf_7_decode__doc__},

static PyObject *
_codecs_utf_7_decode_impl(PyObject *module, Py_buffer *data,
                          const char *errors, int final);

static PyObject *
_codecs_utf_7_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;
    int final = 0;

    if (!_PyArg_CheckPositional("utf_7_decode", nargs, 1, 3)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("utf_7_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_7_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    final = _PyLong_AsInt(args[2]);
    if (final == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_7_decode_impl(module, &data, errors, final);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_utf_8_decode__doc__,
"utf_8_decode($module, data, errors=None, final=False, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_8_DECODE_METHODDEF    \
    {"utf_8_decode", (PyCFunction)(void(*)(void))_codecs_utf_8_decode, METH_FASTCALL, _codecs_utf_8_decode__doc__},

static PyObject *
_codecs_utf_8_decode_impl(PyObject *module, Py_buffer *data,
                          const char *errors, int final);

static PyObject *
_codecs_utf_8_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;
    int final = 0;

    if (!_PyArg_CheckPositional("utf_8_decode", nargs, 1, 3)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("utf_8_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_8_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    final = _PyLong_AsInt(args[2]);
    if (final == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_8_decode_impl(module, &data, errors, final);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_utf_16_decode__doc__,
"utf_16_decode($module, data, errors=None, final=False, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_16_DECODE_METHODDEF    \
    {"utf_16_decode", (PyCFunction)(void(*)(void))_codecs_utf_16_decode, METH_FASTCALL, _codecs_utf_16_decode__doc__},

static PyObject *
_codecs_utf_16_decode_impl(PyObject *module, Py_buffer *data,
                           const char *errors, int final);

static PyObject *
_codecs_utf_16_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;
    int final = 0;

    if (!_PyArg_CheckPositional("utf_16_decode", nargs, 1, 3)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("utf_16_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_16_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    final = _PyLong_AsInt(args[2]);
    if (final == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_16_decode_impl(module, &data, errors, final);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_utf_16_le_decode__doc__,
"utf_16_le_decode($module, data, errors=None, final=False, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_16_LE_DECODE_METHODDEF    \
    {"utf_16_le_decode", (PyCFunction)(void(*)(void))_codecs_utf_16_le_decode, METH_FASTCALL, _codecs_utf_16_le_decode__doc__},

static PyObject *
_codecs_utf_16_le_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int final);

static PyObject *
_codecs_utf_16_le_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;
    int final = 0;

    if (!_PyArg_CheckPositional("utf_16_le_decode", nargs, 1, 3)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("utf_16_le_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_16_le_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    final = _PyLong_AsInt(args[2]);
    if (final == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_16_le_decode_impl(module, &data, errors, final);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_utf_16_be_decode__doc__,
"utf_16_be_decode($module, data, errors=None, final=False, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_16_BE_DECODE_METHODDEF    \
    {"utf_16_be_decode", (PyCFunction)(void(*)(void))_codecs_utf_16_be_decode, METH_FASTCALL, _codecs_utf_16_be_decode__doc__},

static PyObject *
_codecs_utf_16_be_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int final);

static PyObject *
_codecs_utf_16_be_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;
    int final = 0;

    if (!_PyArg_CheckPositional("utf_16_be_decode", nargs, 1, 3)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("utf_16_be_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_16_be_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    final = _PyLong_AsInt(args[2]);
    if (final == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_16_be_decode_impl(module, &data, errors, final);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_utf_16_ex_decode__doc__,
"utf_16_ex_decode($module, data, errors=None, byteorder=0, final=False,\n"
"                 /)\n"
"--\n"
"\n");

#define _CODECS_UTF_16_EX_DECODE_METHODDEF    \
    {"utf_16_ex_decode", (PyCFunction)(void(*)(void))_codecs_utf_16_ex_decode, METH_FASTCALL, _codecs_utf_16_ex_decode__doc__},

static PyObject *
_codecs_utf_16_ex_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int byteorder, int final);

static PyObject *
_codecs_utf_16_ex_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;
    int byteorder = 0;
    int final = 0;

    if (!_PyArg_CheckPositional("utf_16_ex_decode", nargs, 1, 4)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("utf_16_ex_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_16_ex_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    byteorder = _PyLong_AsInt(args[2]);
    if (byteorder == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (nargs < 4) {
        goto skip_optional;
    }
    final = _PyLong_AsInt(args[3]);
    if (final == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_16_ex_decode_impl(module, &data, errors, byteorder, final);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_utf_32_decode__doc__,
"utf_32_decode($module, data, errors=None, final=False, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_32_DECODE_METHODDEF    \
    {"utf_32_decode", (PyCFunction)(void(*)(void))_codecs_utf_32_decode, METH_FASTCALL, _codecs_utf_32_decode__doc__},

static PyObject *
_codecs_utf_32_decode_impl(PyObject *module, Py_buffer *data,
                           const char *errors, int final);

static PyObject *
_codecs_utf_32_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;
    int final = 0;

    if (!_PyArg_CheckPositional("utf_32_decode", nargs, 1, 3)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("utf_32_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_32_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    final = _PyLong_AsInt(args[2]);
    if (final == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_32_decode_impl(module, &data, errors, final);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_utf_32_le_decode__doc__,
"utf_32_le_decode($module, data, errors=None, final=False, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_32_LE_DECODE_METHODDEF    \
    {"utf_32_le_decode", (PyCFunction)(void(*)(void))_codecs_utf_32_le_decode, METH_FASTCALL, _codecs_utf_32_le_decode__doc__},

static PyObject *
_codecs_utf_32_le_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int final);

static PyObject *
_codecs_utf_32_le_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;
    int final = 0;

    if (!_PyArg_CheckPositional("utf_32_le_decode", nargs, 1, 3)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("utf_32_le_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_32_le_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    final = _PyLong_AsInt(args[2]);
    if (final == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_32_le_decode_impl(module, &data, errors, final);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_utf_32_be_decode__doc__,
"utf_32_be_decode($module, data, errors=None, final=False, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_32_BE_DECODE_METHODDEF    \
    {"utf_32_be_decode", (PyCFunction)(void(*)(void))_codecs_utf_32_be_decode, METH_FASTCALL, _codecs_utf_32_be_decode__doc__},

static PyObject *
_codecs_utf_32_be_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int final);

static PyObject *
_codecs_utf_32_be_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;
    int final = 0;

    if (!_PyArg_CheckPositional("utf_32_be_decode", nargs, 1, 3)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("utf_32_be_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_32_be_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    final = _PyLong_AsInt(args[2]);
    if (final == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_32_be_decode_impl(module, &data, errors, final);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_utf_32_ex_decode__doc__,
"utf_32_ex_decode($module, data, errors=None, byteorder=0, final=False,\n"
"                 /)\n"
"--\n"
"\n");

#define _CODECS_UTF_32_EX_DECODE_METHODDEF    \
    {"utf_32_ex_decode", (PyCFunction)(void(*)(void))_codecs_utf_32_ex_decode, METH_FASTCALL, _codecs_utf_32_ex_decode__doc__},

static PyObject *
_codecs_utf_32_ex_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int byteorder, int final);

static PyObject *
_codecs_utf_32_ex_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;
    int byteorder = 0;
    int final = 0;

    if (!_PyArg_CheckPositional("utf_32_ex_decode", nargs, 1, 4)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("utf_32_ex_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_32_ex_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    byteorder = _PyLong_AsInt(args[2]);
    if (byteorder == -1 && PyErr_Occurred()) {
        goto exit;
    }
    if (nargs < 4) {
        goto skip_optional;
    }
    final = _PyLong_AsInt(args[3]);
    if (final == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_32_ex_decode_impl(module, &data, errors, byteorder, final);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_unicode_escape_decode__doc__,
"unicode_escape_decode($module, data, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_UNICODE_ESCAPE_DECODE_METHODDEF    \
    {"unicode_escape_decode", (PyCFunction)(void(*)(void))_codecs_unicode_escape_decode, METH_FASTCALL, _codecs_unicode_escape_decode__doc__},

static PyObject *
_codecs_unicode_escape_decode_impl(PyObject *module, Py_buffer *data,
                                   const char *errors);

static PyObject *
_codecs_unicode_escape_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("unicode_escape_decode", nargs, 1, 2)) {
        goto exit;
    }
    if (PyUnicode_Check(args[0])) {
        Py_ssize_t len;
        const char *ptr = PyUnicode_AsUTF8AndSize(args[0], &len);
        if (ptr == NULL) {
            goto exit;
        }
        PyBuffer_FillInfo(&data, args[0], (void *)ptr, len, 1, 0);
    }
    else { /* any bytes-like object */
        if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
            goto exit;
        }
        if (!PyBuffer_IsContiguous(&data, 'C')) {
            _PyArg_BadArgument("unicode_escape_decode", "argument 1", "contiguous buffer", args[0]);
            goto exit;
        }
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("unicode_escape_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_unicode_escape_decode_impl(module, &data, errors);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_raw_unicode_escape_decode__doc__,
"raw_unicode_escape_decode($module, data, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_RAW_UNICODE_ESCAPE_DECODE_METHODDEF    \
    {"raw_unicode_escape_decode", (PyCFunction)(void(*)(void))_codecs_raw_unicode_escape_decode, METH_FASTCALL, _codecs_raw_unicode_escape_decode__doc__},

static PyObject *
_codecs_raw_unicode_escape_decode_impl(PyObject *module, Py_buffer *data,
                                       const char *errors);

static PyObject *
_codecs_raw_unicode_escape_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("raw_unicode_escape_decode", nargs, 1, 2)) {
        goto exit;
    }
    if (PyUnicode_Check(args[0])) {
        Py_ssize_t len;
        const char *ptr = PyUnicode_AsUTF8AndSize(args[0], &len);
        if (ptr == NULL) {
            goto exit;
        }
        PyBuffer_FillInfo(&data, args[0], (void *)ptr, len, 1, 0);
    }
    else { /* any bytes-like object */
        if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
            goto exit;
        }
        if (!PyBuffer_IsContiguous(&data, 'C')) {
            _PyArg_BadArgument("raw_unicode_escape_decode", "argument 1", "contiguous buffer", args[0]);
            goto exit;
        }
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("raw_unicode_escape_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_raw_unicode_escape_decode_impl(module, &data, errors);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_latin_1_decode__doc__,
"latin_1_decode($module, data, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_LATIN_1_DECODE_METHODDEF    \
    {"latin_1_decode", (PyCFunction)(void(*)(void))_codecs_latin_1_decode, METH_FASTCALL, _codecs_latin_1_decode__doc__},

static PyObject *
_codecs_latin_1_decode_impl(PyObject *module, Py_buffer *data,
                            const char *errors);

static PyObject *
_codecs_latin_1_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("latin_1_decode", nargs, 1, 2)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("latin_1_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("latin_1_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_latin_1_decode_impl(module, &data, errors);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_ascii_decode__doc__,
"ascii_decode($module, data, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_ASCII_DECODE_METHODDEF    \
    {"ascii_decode", (PyCFunction)(void(*)(void))_codecs_ascii_decode, METH_FASTCALL, _codecs_ascii_decode__doc__},

static PyObject *
_codecs_ascii_decode_impl(PyObject *module, Py_buffer *data,
                          const char *errors);

static PyObject *
_codecs_ascii_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("ascii_decode", nargs, 1, 2)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("ascii_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("ascii_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_ascii_decode_impl(module, &data, errors);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_charmap_decode__doc__,
"charmap_decode($module, data, errors=None, mapping=None, /)\n"
"--\n"
"\n");

#define _CODECS_CHARMAP_DECODE_METHODDEF    \
    {"charmap_decode", (PyCFunction)(void(*)(void))_codecs_charmap_decode, METH_FASTCALL, _codecs_charmap_decode__doc__},

static PyObject *
_codecs_charmap_decode_impl(PyObject *module, Py_buffer *data,
                            const char *errors, PyObject *mapping);

static PyObject *
_codecs_charmap_decode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;
    PyObject *mapping = Py_None;

    if (!_PyArg_CheckPositional("charmap_decode", nargs, 1, 3)) {
        goto exit;
    }
    if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
        goto exit;
    }
    if (!PyBuffer_IsContiguous(&data, 'C')) {
        _PyArg_BadArgument("charmap_decode", "argument 1", "contiguous buffer", args[0]);
        goto exit;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("charmap_decode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    mapping = args[2];
skip_optional:
    return_value = _codecs_charmap_decode_impl(module, &data, errors, mapping);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}



PyDoc_STRVAR(_codecs_readbuffer_encode__doc__,
"readbuffer_encode($module, data, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_READBUFFER_ENCODE_METHODDEF    \
    {"readbuffer_encode", (PyCFunction)(void(*)(void))_codecs_readbuffer_encode, METH_FASTCALL, _codecs_readbuffer_encode__doc__},

static PyObject *
_codecs_readbuffer_encode_impl(PyObject *module, Py_buffer *data,
                               const char *errors);

static PyObject *
_codecs_readbuffer_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_buffer data = {NULL, NULL};
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("readbuffer_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (PyUnicode_Check(args[0])) {
        Py_ssize_t len;
        const char *ptr = PyUnicode_AsUTF8AndSize(args[0], &len);
        if (ptr == NULL) {
            goto exit;
        }
        PyBuffer_FillInfo(&data, args[0], (void *)ptr, len, 1, 0);
    }
    else { /* any bytes-like object */
        if (PyObject_GetBuffer(args[0], &data, PyBUF_SIMPLE) != 0) {
            goto exit;
        }
        if (!PyBuffer_IsContiguous(&data, 'C')) {
            _PyArg_BadArgument("readbuffer_encode", "argument 1", "contiguous buffer", args[0]);
            goto exit;
        }
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("readbuffer_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_readbuffer_encode_impl(module, &data, errors);

exit:
    /* Cleanup for data */
    if (data.obj) {
       PyBuffer_Release(&data);
    }

    return return_value;
}

PyDoc_STRVAR(_codecs_utf_7_encode__doc__,
"utf_7_encode($module, str, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_7_ENCODE_METHODDEF    \
    {"utf_7_encode", (PyCFunction)(void(*)(void))_codecs_utf_7_encode, METH_FASTCALL, _codecs_utf_7_encode__doc__},

static PyObject *
_codecs_utf_7_encode_impl(PyObject *module, PyObject *str,
                          const char *errors);

static PyObject *
_codecs_utf_7_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("utf_7_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("utf_7_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_7_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_7_encode_impl(module, str, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_utf_8_encode__doc__,
"utf_8_encode($module, str, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_8_ENCODE_METHODDEF    \
    {"utf_8_encode", (PyCFunction)(void(*)(void))_codecs_utf_8_encode, METH_FASTCALL, _codecs_utf_8_encode__doc__},

static PyObject *
_codecs_utf_8_encode_impl(PyObject *module, PyObject *str,
                          const char *errors);

static PyObject *
_codecs_utf_8_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("utf_8_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("utf_8_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_8_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_8_encode_impl(module, str, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_utf_16_encode__doc__,
"utf_16_encode($module, str, errors=None, byteorder=0, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_16_ENCODE_METHODDEF    \
    {"utf_16_encode", (PyCFunction)(void(*)(void))_codecs_utf_16_encode, METH_FASTCALL, _codecs_utf_16_encode__doc__},

static PyObject *
_codecs_utf_16_encode_impl(PyObject *module, PyObject *str,
                           const char *errors, int byteorder);

static PyObject *
_codecs_utf_16_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;
    int byteorder = 0;

    if (!_PyArg_CheckPositional("utf_16_encode", nargs, 1, 3)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("utf_16_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_16_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    byteorder = _PyLong_AsInt(args[2]);
    if (byteorder == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_16_encode_impl(module, str, errors, byteorder);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_utf_16_le_encode__doc__,
"utf_16_le_encode($module, str, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_16_LE_ENCODE_METHODDEF    \
    {"utf_16_le_encode", (PyCFunction)(void(*)(void))_codecs_utf_16_le_encode, METH_FASTCALL, _codecs_utf_16_le_encode__doc__},

static PyObject *
_codecs_utf_16_le_encode_impl(PyObject *module, PyObject *str,
                              const char *errors);

static PyObject *
_codecs_utf_16_le_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("utf_16_le_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("utf_16_le_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_16_le_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_16_le_encode_impl(module, str, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_utf_16_be_encode__doc__,
"utf_16_be_encode($module, str, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_16_BE_ENCODE_METHODDEF    \
    {"utf_16_be_encode", (PyCFunction)(void(*)(void))_codecs_utf_16_be_encode, METH_FASTCALL, _codecs_utf_16_be_encode__doc__},

static PyObject *
_codecs_utf_16_be_encode_impl(PyObject *module, PyObject *str,
                              const char *errors);

static PyObject *
_codecs_utf_16_be_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("utf_16_be_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("utf_16_be_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_16_be_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_16_be_encode_impl(module, str, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_utf_32_encode__doc__,
"utf_32_encode($module, str, errors=None, byteorder=0, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_32_ENCODE_METHODDEF    \
    {"utf_32_encode", (PyCFunction)(void(*)(void))_codecs_utf_32_encode, METH_FASTCALL, _codecs_utf_32_encode__doc__},

static PyObject *
_codecs_utf_32_encode_impl(PyObject *module, PyObject *str,
                           const char *errors, int byteorder);

static PyObject *
_codecs_utf_32_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;
    int byteorder = 0;

    if (!_PyArg_CheckPositional("utf_32_encode", nargs, 1, 3)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("utf_32_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_32_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    byteorder = _PyLong_AsInt(args[2]);
    if (byteorder == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_32_encode_impl(module, str, errors, byteorder);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_utf_32_le_encode__doc__,
"utf_32_le_encode($module, str, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_32_LE_ENCODE_METHODDEF    \
    {"utf_32_le_encode", (PyCFunction)(void(*)(void))_codecs_utf_32_le_encode, METH_FASTCALL, _codecs_utf_32_le_encode__doc__},

static PyObject *
_codecs_utf_32_le_encode_impl(PyObject *module, PyObject *str,
                              const char *errors);

static PyObject *
_codecs_utf_32_le_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("utf_32_le_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("utf_32_le_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_32_le_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_32_le_encode_impl(module, str, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_utf_32_be_encode__doc__,
"utf_32_be_encode($module, str, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_UTF_32_BE_ENCODE_METHODDEF    \
    {"utf_32_be_encode", (PyCFunction)(void(*)(void))_codecs_utf_32_be_encode, METH_FASTCALL, _codecs_utf_32_be_encode__doc__},

static PyObject *
_codecs_utf_32_be_encode_impl(PyObject *module, PyObject *str,
                              const char *errors);

static PyObject *
_codecs_utf_32_be_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("utf_32_be_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("utf_32_be_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("utf_32_be_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_utf_32_be_encode_impl(module, str, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_unicode_escape_encode__doc__,
"unicode_escape_encode($module, str, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_UNICODE_ESCAPE_ENCODE_METHODDEF    \
    {"unicode_escape_encode", (PyCFunction)(void(*)(void))_codecs_unicode_escape_encode, METH_FASTCALL, _codecs_unicode_escape_encode__doc__},

static PyObject *
_codecs_unicode_escape_encode_impl(PyObject *module, PyObject *str,
                                   const char *errors);

static PyObject *
_codecs_unicode_escape_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("unicode_escape_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("unicode_escape_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("unicode_escape_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_unicode_escape_encode_impl(module, str, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_raw_unicode_escape_encode__doc__,
"raw_unicode_escape_encode($module, str, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_RAW_UNICODE_ESCAPE_ENCODE_METHODDEF    \
    {"raw_unicode_escape_encode", (PyCFunction)(void(*)(void))_codecs_raw_unicode_escape_encode, METH_FASTCALL, _codecs_raw_unicode_escape_encode__doc__},

static PyObject *
_codecs_raw_unicode_escape_encode_impl(PyObject *module, PyObject *str,
                                       const char *errors);

static PyObject *
_codecs_raw_unicode_escape_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("raw_unicode_escape_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("raw_unicode_escape_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("raw_unicode_escape_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_raw_unicode_escape_encode_impl(module, str, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_latin_1_encode__doc__,
"latin_1_encode($module, str, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_LATIN_1_ENCODE_METHODDEF    \
    {"latin_1_encode", (PyCFunction)(void(*)(void))_codecs_latin_1_encode, METH_FASTCALL, _codecs_latin_1_encode__doc__},

static PyObject *
_codecs_latin_1_encode_impl(PyObject *module, PyObject *str,
                            const char *errors);

static PyObject *
_codecs_latin_1_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("latin_1_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("latin_1_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("latin_1_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_latin_1_encode_impl(module, str, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_ascii_encode__doc__,
"ascii_encode($module, str, errors=None, /)\n"
"--\n"
"\n");

#define _CODECS_ASCII_ENCODE_METHODDEF    \
    {"ascii_encode", (PyCFunction)(void(*)(void))_codecs_ascii_encode, METH_FASTCALL, _codecs_ascii_encode__doc__},

static PyObject *
_codecs_ascii_encode_impl(PyObject *module, PyObject *str,
                          const char *errors);

static PyObject *
_codecs_ascii_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;

    if (!_PyArg_CheckPositional("ascii_encode", nargs, 1, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("ascii_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("ascii_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
skip_optional:
    return_value = _codecs_ascii_encode_impl(module, str, errors);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_charmap_encode__doc__,
"charmap_encode($module, str, errors=None, mapping=None, /)\n"
"--\n"
"\n");

#define _CODECS_CHARMAP_ENCODE_METHODDEF    \
    {"charmap_encode", (PyCFunction)(void(*)(void))_codecs_charmap_encode, METH_FASTCALL, _codecs_charmap_encode__doc__},

static PyObject *
_codecs_charmap_encode_impl(PyObject *module, PyObject *str,
                            const char *errors, PyObject *mapping);

static PyObject *
_codecs_charmap_encode(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *str;
    const char *errors = NULL;
    PyObject *mapping = Py_None;

    if (!_PyArg_CheckPositional("charmap_encode", nargs, 1, 3)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("charmap_encode", "argument 1", "str", args[0]);
        goto exit;
    }
    if (PyUnicode_READY(args[0]) == -1) {
        goto exit;
    }
    str = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    if (args[1] == Py_None) {
        errors = NULL;
    }
    else if (PyUnicode_Check(args[1])) {
        Py_ssize_t errors_length;
        errors = PyUnicode_AsUTF8AndSize(args[1], &errors_length);
        if (errors == NULL) {
            goto exit;
        }
        if (strlen(errors) != (size_t)errors_length) {
            PyErr_SetString(PyExc_ValueError, "embedded null character");
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("charmap_encode", "argument 2", "str or None", args[1]);
        goto exit;
    }
    if (nargs < 3) {
        goto skip_optional;
    }
    mapping = args[2];
skip_optional:
    return_value = _codecs_charmap_encode_impl(module, str, errors, mapping);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_charmap_build__doc__,
"charmap_build($module, map, /)\n"
"--\n"
"\n");

#define _CODECS_CHARMAP_BUILD_METHODDEF    \
    {"charmap_build", (PyCFunction)_codecs_charmap_build, METH_O, _codecs_charmap_build__doc__},

static PyObject *
_codecs_charmap_build_impl(PyObject *module, PyObject *map);

static PyObject *
_codecs_charmap_build(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    PyObject *map;

    if (!PyUnicode_Check(arg)) {
        _PyArg_BadArgument("charmap_build", "argument", "str", arg);
        goto exit;
    }
    if (PyUnicode_READY(arg) == -1) {
        goto exit;
    }
    map = arg;
    return_value = _codecs_charmap_build_impl(module, map);

exit:
    return return_value;
}



PyDoc_STRVAR(_codecs_register_error__doc__,
"register_error($module, errors, handler, /)\n"
"--\n"
"\n"
"Register the specified error handler under the name errors.\n"
"\n"
"handler must be a callable object, that will be called with an exception\n"
"instance containing information about the location of the encoding/decoding\n"
"error and must return a (replacement, new position) tuple.");

#define _CODECS_REGISTER_ERROR_METHODDEF    \
    {"register_error", (PyCFunction)(void(*)(void))_codecs_register_error, METH_FASTCALL, _codecs_register_error__doc__},

static PyObject *
_codecs_register_error_impl(PyObject *module, const char *errors,
                            PyObject *handler);

static PyObject *
_codecs_register_error(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    const char *errors;
    PyObject *handler;

    if (!_PyArg_CheckPositional("register_error", nargs, 2, 2)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("register_error", "argument 1", "str", args[0]);
        goto exit;
    }
    Py_ssize_t errors_length;
    errors = PyUnicode_AsUTF8AndSize(args[0], &errors_length);
    if (errors == NULL) {
        goto exit;
    }
    if (strlen(errors) != (size_t)errors_length) {
        PyErr_SetString(PyExc_ValueError, "embedded null character");
        goto exit;
    }
    handler = args[1];
    return_value = _codecs_register_error_impl(module, errors, handler);

exit:
    return return_value;
}

PyDoc_STRVAR(_codecs_lookup_error__doc__,
"lookup_error($module, name, /)\n"
"--\n"
"\n"
"lookup_error(errors) -> handler\n"
"\n"
"Return the error handler for the specified error handling name or raise a\n"
"LookupError, if no handler exists under this name.");

#define _CODECS_LOOKUP_ERROR_METHODDEF    \
    {"lookup_error", (PyCFunction)_codecs_lookup_error, METH_O, _codecs_lookup_error__doc__},

static PyObject *
_codecs_lookup_error_impl(PyObject *module, const char *name);

static PyObject *
_codecs_lookup_error(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    const char *name;

    if (!PyUnicode_Check(arg)) {
        _PyArg_BadArgument("lookup_error", "argument", "str", arg);
        goto exit;
    }
    Py_ssize_t name_length;
    name = PyUnicode_AsUTF8AndSize(arg, &name_length);
    if (name == NULL) {
        goto exit;
    }
    if (strlen(name) != (size_t)name_length) {
        PyErr_SetString(PyExc_ValueError, "embedded null character");
        goto exit;
    }
    return_value = _codecs_lookup_error_impl(module, name);

exit:
    return return_value;
}

#ifndef _CODECS_MBCS_DECODE_METHODDEF
    #define _CODECS_MBCS_DECODE_METHODDEF
#endif /* !defined(_CODECS_MBCS_DECODE_METHODDEF) */

#ifndef _CODECS_OEM_DECODE_METHODDEF
    #define _CODECS_OEM_DECODE_METHODDEF
#endif /* !defined(_CODECS_OEM_DECODE_METHODDEF) */

#ifndef _CODECS_CODE_PAGE_DECODE_METHODDEF
    #define _CODECS_CODE_PAGE_DECODE_METHODDEF
#endif /* !defined(_CODECS_CODE_PAGE_DECODE_METHODDEF) */

#ifndef _CODECS_MBCS_ENCODE_METHODDEF
    #define _CODECS_MBCS_ENCODE_METHODDEF
#endif /* !defined(_CODECS_MBCS_ENCODE_METHODDEF) */

#ifndef _CODECS_OEM_ENCODE_METHODDEF
    #define _CODECS_OEM_ENCODE_METHODDEF
#endif /* !defined(_CODECS_OEM_ENCODE_METHODDEF) */

#ifndef _CODECS_CODE_PAGE_ENCODE_METHODDEF
    #define _CODECS_CODE_PAGE_ENCODE_METHODDEF
#endif /* !defined(_CODECS_CODE_PAGE_ENCODE_METHODDEF) */
/*[clinic end generated code: output=eeead01414be6e42 input=a9049054013a1b77]*/


/* --- Registry ----------------------------------------------------------- */

/*[clinic input]
_codecs.register
    search_function: object
    /

Register a codec search function.

Search functions are expected to take one argument, the encoding name in
all lower case letters, and either return None, or a tuple of functions
(encoder, decoder, stream_reader, stream_writer) (or a CodecInfo object).
[clinic start generated code]*/

static PyObject *
_codecs_register(PyObject *module, PyObject *search_function)
/*[clinic end generated code: output=d1bf21e99db7d6d3 input=369578467955cae4]*/
{
    if (PyCodec_Register(search_function))
        return NULL;

    Py_RETURN_NONE;
}

/*[clinic input]
_codecs.lookup
    encoding: str
    /

Looks up a codec tuple in the Python codec registry and returns a CodecInfo object.
[clinic start generated code]*/

static PyObject *
_codecs_lookup_impl(PyObject *module, const char *encoding)
/*[clinic end generated code: output=9f0afa572080c36d input=3c572c0db3febe9c]*/
{
    return _PyCodec_Lookup(encoding);
}

/*[clinic input]
_codecs.encode
    obj: object
    encoding: str(c_default="NULL") = "utf-8"
    errors: str(c_default="NULL") = "strict"

Encodes obj using the codec registered for encoding.

The default encoding is 'utf-8'.  errors may be given to set a
different error handling scheme.  Default is 'strict' meaning that encoding
errors raise a ValueError.  Other possible values are 'ignore', 'replace'
and 'backslashreplace' as well as any other name registered with
codecs.register_error that can handle ValueErrors.
[clinic start generated code]*/

static PyObject *
_codecs_encode_impl(PyObject *module, PyObject *obj, const char *encoding,
                    const char *errors)
/*[clinic end generated code: output=385148eb9a067c86 input=cd5b685040ff61f0]*/
{
    if (encoding == NULL)
        encoding = PyUnicode_GetDefaultEncoding();

    /* Encode via the codec registry */
    return PyCodec_Encode(obj, encoding, errors);
}

/*[clinic input]
_codecs.decode
    obj: object
    encoding: str(c_default="NULL") = "utf-8"
    errors: str(c_default="NULL") = "strict"

Decodes obj using the codec registered for encoding.

Default encoding is 'utf-8'.  errors may be given to set a
different error handling scheme.  Default is 'strict' meaning that encoding
errors raise a ValueError.  Other possible values are 'ignore', 'replace'
and 'backslashreplace' as well as any other name registered with
codecs.register_error that can handle ValueErrors.
[clinic start generated code]*/

static PyObject *
_codecs_decode_impl(PyObject *module, PyObject *obj, const char *encoding,
                    const char *errors)
/*[clinic end generated code: output=679882417dc3a0bd input=7702c0cc2fa1add6]*/
{
    if (encoding == NULL)
        encoding = PyUnicode_GetDefaultEncoding();

    /* Decode via the codec registry */
    return PyCodec_Decode(obj, encoding, errors);
}

/* --- Helpers ------------------------------------------------------------ */

/*[clinic input]
_codecs._forget_codec

    encoding: str
    /

Purge the named codec from the internal codec lookup cache
[clinic start generated code]*/

static PyObject *
_codecs__forget_codec_impl(PyObject *module, const char *encoding)
/*[clinic end generated code: output=0bde9f0a5b084aa2 input=18d5d92d0e386c38]*/
{
    if (_PyCodec_Forget(encoding) < 0) {
        return NULL;
    };
    Py_RETURN_NONE;
}

static
PyObject *codec_tuple(PyObject *decoded,
                      Py_ssize_t len)
{
    if (decoded == NULL)
        return NULL;
    return Py_BuildValue("Nn", decoded, len);
}

/* --- String codecs ------------------------------------------------------ */
/*[clinic input]
_codecs.escape_decode
    data: Py_buffer(accept={str, buffer})
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_escape_decode_impl(PyObject *module, Py_buffer *data,
                           const char *errors)
/*[clinic end generated code: output=505200ba8056979a input=77298a561c90bd82]*/
{
    PyObject *decoded = PyBytes_DecodeEscape(data->buf, data->len,
                                             errors, 0, NULL);
    return codec_tuple(decoded, data->len);
}

/*[clinic input]
_codecs.escape_encode
    data: object(subclass_of='&PyBytes_Type')
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_escape_encode_impl(PyObject *module, PyObject *data,
                           const char *errors)
/*[clinic end generated code: output=4af1d477834bab34 input=8f4b144799a94245]*/
{
    Py_ssize_t size;
    Py_ssize_t newsize;
    PyObject *v;

    size = PyBytes_GET_SIZE(data);
    if (size > PY_SSIZE_T_MAX / 4) {
        PyErr_SetString(PyExc_OverflowError,
            "string is too large to encode");
            return NULL;
    }
    newsize = 4*size;
    v = PyBytes_FromStringAndSize(NULL, newsize);

    if (v == NULL) {
        return NULL;
    }
    else {
        Py_ssize_t i;
        char c;
        char *p = PyBytes_AS_STRING(v);

        for (i = 0; i < size; i++) {
            /* There's at least enough room for a hex escape */
            assert(newsize - (p - PyBytes_AS_STRING(v)) >= 4);
            c = PyBytes_AS_STRING(data)[i];
            if (c == '\'' || c == '\\')
                *p++ = '\\', *p++ = c;
            else if (c == '\t')
                *p++ = '\\', *p++ = 't';
            else if (c == '\n')
                *p++ = '\\', *p++ = 'n';
            else if (c == '\r')
                *p++ = '\\', *p++ = 'r';
            else if (c < ' ' || c >= 0x7f) {
                *p++ = '\\';
                *p++ = 'x';
                *p++ = Py_hexdigits[(c & 0xf0) >> 4];
                *p++ = Py_hexdigits[c & 0xf];
            }
            else
                *p++ = c;
        }
        *p = '\0';
        if (_PyBytes_Resize(&v, (p - PyBytes_AS_STRING(v)))) {
            return NULL;
        }
    }

    return codec_tuple(v, size);
}

/* --- Decoder ------------------------------------------------------------ */
/*[clinic input]
_codecs.utf_7_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    final: bool(accept={int}) = False
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_7_decode_impl(PyObject *module, Py_buffer *data,
                          const char *errors, int final)
/*[clinic end generated code: output=0cd3a944a32a4089 input=22c395d357815d26]*/
{
    Py_ssize_t consumed = data->len;
    PyObject *decoded = PyUnicode_DecodeUTF7Stateful(data->buf, data->len,
                                                     errors,
                                                     final ? NULL : &consumed);
    return codec_tuple(decoded, consumed);
}

/*[clinic input]
_codecs.utf_8_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    final: bool(accept={int}) = False
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_8_decode_impl(PyObject *module, Py_buffer *data,
                          const char *errors, int final)
/*[clinic end generated code: output=10f74dec8d9bb8bf input=f611b3867352ba59]*/
{
    Py_ssize_t consumed = data->len;
    PyObject *decoded = PyUnicode_DecodeUTF8Stateful(data->buf, data->len,
                                                     errors,
                                                     final ? NULL : &consumed);
    return codec_tuple(decoded, consumed);
}

/*[clinic input]
_codecs.utf_16_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    final: bool(accept={int}) = False
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_16_decode_impl(PyObject *module, Py_buffer *data,
                           const char *errors, int final)
/*[clinic end generated code: output=783b442abcbcc2d0 input=191d360bd7309180]*/
{
    int byteorder = 0;
    /* This is overwritten unless final is true. */
    Py_ssize_t consumed = data->len;
    PyObject *decoded = PyUnicode_DecodeUTF16Stateful(data->buf, data->len,
                                                      errors, &byteorder,
                                                      final ? NULL : &consumed);
    return codec_tuple(decoded, consumed);
}

/*[clinic input]
_codecs.utf_16_le_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    final: bool(accept={int}) = False
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_16_le_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int final)
/*[clinic end generated code: output=899b9e6364379dcd input=c6904fdc27fb4724]*/
{
    int byteorder = -1;
    /* This is overwritten unless final is true. */
    Py_ssize_t consumed = data->len;
    PyObject *decoded = PyUnicode_DecodeUTF16Stateful(data->buf, data->len,
                                                      errors, &byteorder,
                                                      final ? NULL : &consumed);
    return codec_tuple(decoded, consumed);
}

/*[clinic input]
_codecs.utf_16_be_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    final: bool(accept={int}) = False
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_16_be_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int final)
/*[clinic end generated code: output=49f6465ea07669c8 input=e49012400974649b]*/
{
    int byteorder = 1;
    /* This is overwritten unless final is true. */
    Py_ssize_t consumed = data->len;
    PyObject *decoded = PyUnicode_DecodeUTF16Stateful(data->buf, data->len,
                                                      errors, &byteorder,
                                                      final ? NULL : &consumed);
    return codec_tuple(decoded, consumed);
}

/* This non-standard version also provides access to the byteorder
   parameter of the builtin UTF-16 codec.

   It returns a tuple (unicode, bytesread, byteorder) with byteorder
   being the value in effect at the end of data.

*/
/*[clinic input]
_codecs.utf_16_ex_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    byteorder: int = 0
    final: bool(accept={int}) = False
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_16_ex_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int byteorder, int final)
/*[clinic end generated code: output=0f385f251ecc1988 input=5a9c19f2e6b6cf0e]*/
{
    /* This is overwritten unless final is true. */
    Py_ssize_t consumed = data->len;

    PyObject *decoded = PyUnicode_DecodeUTF16Stateful(data->buf, data->len,
                                                      errors, &byteorder,
                                                      final ? NULL : &consumed);
    if (decoded == NULL)
        return NULL;
    return Py_BuildValue("Nni", decoded, consumed, byteorder);
}

/*[clinic input]
_codecs.utf_32_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    final: bool(accept={int}) = False
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_32_decode_impl(PyObject *module, Py_buffer *data,
                           const char *errors, int final)
/*[clinic end generated code: output=2fc961807f7b145f input=fd7193965627eb58]*/
{
    int byteorder = 0;
    /* This is overwritten unless final is true. */
    Py_ssize_t consumed = data->len;
    PyObject *decoded = PyUnicode_DecodeUTF32Stateful(data->buf, data->len,
                                                      errors, &byteorder,
                                                      final ? NULL : &consumed);
    return codec_tuple(decoded, consumed);
}

/*[clinic input]
_codecs.utf_32_le_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    final: bool(accept={int}) = False
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_32_le_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int final)
/*[clinic end generated code: output=ec8f46b67a94f3e6 input=9078ec70acfe7613]*/
{
    int byteorder = -1;
    /* This is overwritten unless final is true. */
    Py_ssize_t consumed = data->len;
    PyObject *decoded = PyUnicode_DecodeUTF32Stateful(data->buf, data->len,
                                                      errors, &byteorder,
                                                      final ? NULL : &consumed);
    return codec_tuple(decoded, consumed);
}

/*[clinic input]
_codecs.utf_32_be_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    final: bool(accept={int}) = False
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_32_be_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int final)
/*[clinic end generated code: output=ff82bae862c92c4e input=f1ae1bbbb86648ff]*/
{
    int byteorder = 1;
    /* This is overwritten unless final is true. */
    Py_ssize_t consumed = data->len;
    PyObject *decoded = PyUnicode_DecodeUTF32Stateful(data->buf, data->len,
                                                      errors, &byteorder,
                                                      final ? NULL : &consumed);
    return codec_tuple(decoded, consumed);
}

/* This non-standard version also provides access to the byteorder
   parameter of the builtin UTF-32 codec.

   It returns a tuple (unicode, bytesread, byteorder) with byteorder
   being the value in effect at the end of data.

*/
/*[clinic input]
_codecs.utf_32_ex_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    byteorder: int = 0
    final: bool(accept={int}) = False
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_32_ex_decode_impl(PyObject *module, Py_buffer *data,
                              const char *errors, int byteorder, int final)
/*[clinic end generated code: output=6bfb177dceaf4848 input=e46a73bc859d0bd0]*/
{
    Py_ssize_t consumed = data->len;
    PyObject *decoded = PyUnicode_DecodeUTF32Stateful(data->buf, data->len,
                                                      errors, &byteorder,
                                                      final ? NULL : &consumed);
    if (decoded == NULL)
        return NULL;
    return Py_BuildValue("Nni", decoded, consumed, byteorder);
}

/*[clinic input]
_codecs.unicode_escape_decode
    data: Py_buffer(accept={str, buffer})
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_unicode_escape_decode_impl(PyObject *module, Py_buffer *data,
                                   const char *errors)
/*[clinic end generated code: output=3ca3c917176b82ab input=8328081a3a569bd6]*/
{
    PyObject *decoded = PyUnicode_DecodeUnicodeEscape(data->buf, data->len,
                                                      errors);
    return codec_tuple(decoded, data->len);
}

/*[clinic input]
_codecs.raw_unicode_escape_decode
    data: Py_buffer(accept={str, buffer})
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_raw_unicode_escape_decode_impl(PyObject *module, Py_buffer *data,
                                       const char *errors)
/*[clinic end generated code: output=c98eeb56028070a6 input=d2f5159ce3b3392f]*/
{
    PyObject *decoded = PyUnicode_DecodeRawUnicodeEscape(data->buf, data->len,
                                                         errors);
    return codec_tuple(decoded, data->len);
}

/*[clinic input]
_codecs.latin_1_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_latin_1_decode_impl(PyObject *module, Py_buffer *data,
                            const char *errors)
/*[clinic end generated code: output=07f3dfa3f72c7d8f input=76ca58fd6dcd08c7]*/
{
    PyObject *decoded = PyUnicode_DecodeLatin1(data->buf, data->len, errors);
    return codec_tuple(decoded, data->len);
}

/*[clinic input]
_codecs.ascii_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_ascii_decode_impl(PyObject *module, Py_buffer *data,
                          const char *errors)
/*[clinic end generated code: output=2627d72058d42429 input=e428a267a04b4481]*/
{
    PyObject *decoded = PyUnicode_DecodeASCII(data->buf, data->len, errors);
    return codec_tuple(decoded, data->len);
}

/*[clinic input]
_codecs.charmap_decode
    data: Py_buffer
    errors: str(accept={str, NoneType}) = None
    mapping: object = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_charmap_decode_impl(PyObject *module, Py_buffer *data,
                            const char *errors, PyObject *mapping)
/*[clinic end generated code: output=2c335b09778cf895 input=15b69df43458eb40]*/
{
    PyObject *decoded;

    if (mapping == Py_None)
        mapping = NULL;

    decoded = PyUnicode_DecodeCharmap(data->buf, data->len, mapping, errors);
    return codec_tuple(decoded, data->len);
}


/* --- Encoder ------------------------------------------------------------ */

/*[clinic input]
_codecs.readbuffer_encode
    data: Py_buffer(accept={str, buffer})
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_readbuffer_encode_impl(PyObject *module, Py_buffer *data,
                               const char *errors)
/*[clinic end generated code: output=c645ea7cdb3d6e86 input=aa10cfdf252455c5]*/
{
    PyObject *result = PyBytes_FromStringAndSize(data->buf, data->len);
    return codec_tuple(result, data->len);
}

/*[clinic input]
_codecs.utf_7_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_7_encode_impl(PyObject *module, PyObject *str,
                          const char *errors)
/*[clinic end generated code: output=0feda21ffc921bc8 input=2546dbbb3fa53114]*/
{
    return codec_tuple(_PyUnicode_EncodeUTF7(str, 0, 0, errors),
                       PyUnicode_GET_LENGTH(str));
}

/*[clinic input]
_codecs.utf_8_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_8_encode_impl(PyObject *module, PyObject *str,
                          const char *errors)
/*[clinic end generated code: output=02bf47332b9c796c input=a3e71ae01c3f93f3]*/
{
    return codec_tuple(_PyUnicode_AsUTF8String(str, errors),
                       PyUnicode_GET_LENGTH(str));
}

/* This version provides access to the byteorder parameter of the
   builtin UTF-16 codecs as optional third argument. It defaults to 0
   which means: use the native byte order and prepend the data with a
   BOM mark.

*/

/*[clinic input]
_codecs.utf_16_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    byteorder: int = 0
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_16_encode_impl(PyObject *module, PyObject *str,
                           const char *errors, int byteorder)
/*[clinic end generated code: output=c654e13efa2e64e4 input=68cdc2eb8338555d]*/
{
    return codec_tuple(_PyUnicode_EncodeUTF16(str, errors, byteorder),
                       PyUnicode_GET_LENGTH(str));
}

/*[clinic input]
_codecs.utf_16_le_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_16_le_encode_impl(PyObject *module, PyObject *str,
                              const char *errors)
/*[clinic end generated code: output=431b01e55f2d4995 input=83d042706eed6798]*/
{
    return codec_tuple(_PyUnicode_EncodeUTF16(str, errors, -1),
                       PyUnicode_GET_LENGTH(str));
}

/*[clinic input]
_codecs.utf_16_be_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_16_be_encode_impl(PyObject *module, PyObject *str,
                              const char *errors)
/*[clinic end generated code: output=96886a6fd54dcae3 input=6f1e9e623b03071b]*/
{
    return codec_tuple(_PyUnicode_EncodeUTF16(str, errors, +1),
                       PyUnicode_GET_LENGTH(str));
}

/* This version provides access to the byteorder parameter of the
   builtin UTF-32 codecs as optional third argument. It defaults to 0
   which means: use the native byte order and prepend the data with a
   BOM mark.

*/

/*[clinic input]
_codecs.utf_32_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    byteorder: int = 0
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_32_encode_impl(PyObject *module, PyObject *str,
                           const char *errors, int byteorder)
/*[clinic end generated code: output=5c760da0c09a8b83 input=8ec4c64d983bc52b]*/
{
    return codec_tuple(_PyUnicode_EncodeUTF32(str, errors, byteorder),
                       PyUnicode_GET_LENGTH(str));
}

/*[clinic input]
_codecs.utf_32_le_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_32_le_encode_impl(PyObject *module, PyObject *str,
                              const char *errors)
/*[clinic end generated code: output=b65cd176de8e36d6 input=f0918d41de3eb1b1]*/
{
    return codec_tuple(_PyUnicode_EncodeUTF32(str, errors, -1),
                       PyUnicode_GET_LENGTH(str));
}

/*[clinic input]
_codecs.utf_32_be_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_utf_32_be_encode_impl(PyObject *module, PyObject *str,
                              const char *errors)
/*[clinic end generated code: output=1d9e71a9358709e9 input=967a99a95748b557]*/
{
    return codec_tuple(_PyUnicode_EncodeUTF32(str, errors, +1),
                       PyUnicode_GET_LENGTH(str));
}

/*[clinic input]
_codecs.unicode_escape_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_unicode_escape_encode_impl(PyObject *module, PyObject *str,
                                   const char *errors)
/*[clinic end generated code: output=66271b30bc4f7a3c input=8c4de07597054e33]*/
{
    return codec_tuple(PyUnicode_AsUnicodeEscapeString(str),
                       PyUnicode_GET_LENGTH(str));
}

/*[clinic input]
_codecs.raw_unicode_escape_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_raw_unicode_escape_encode_impl(PyObject *module, PyObject *str,
                                       const char *errors)
/*[clinic end generated code: output=a66a806ed01c830a input=4aa6f280d78e4574]*/
{
    return codec_tuple(PyUnicode_AsRawUnicodeEscapeString(str),
                       PyUnicode_GET_LENGTH(str));
}

/*[clinic input]
_codecs.latin_1_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_latin_1_encode_impl(PyObject *module, PyObject *str,
                            const char *errors)
/*[clinic end generated code: output=2c28c83a27884e08 input=ec3ef74bf85c5c5d]*/
{
    return codec_tuple(_PyUnicode_AsLatin1String(str, errors),
                       PyUnicode_GET_LENGTH(str));
}

/*[clinic input]
_codecs.ascii_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_ascii_encode_impl(PyObject *module, PyObject *str,
                          const char *errors)
/*[clinic end generated code: output=b5e035182d33befc input=93e6e602838bd3de]*/
{
    return codec_tuple(_PyUnicode_AsASCIIString(str, errors),
                       PyUnicode_GET_LENGTH(str));
}

/*[clinic input]
_codecs.charmap_encode
    str: unicode
    errors: str(accept={str, NoneType}) = None
    mapping: object = None
    /
[clinic start generated code]*/

static PyObject *
_codecs_charmap_encode_impl(PyObject *module, PyObject *str,
                            const char *errors, PyObject *mapping)
/*[clinic end generated code: output=047476f48495a9e9 input=2a98feae73dadce8]*/
{
    if (mapping == Py_None)
        mapping = NULL;

    return codec_tuple(_PyUnicode_EncodeCharmap(str, mapping, errors),
                       PyUnicode_GET_LENGTH(str));
}

/*[clinic input]
_codecs.charmap_build
    map: unicode
    /
[clinic start generated code]*/

static PyObject *
_codecs_charmap_build_impl(PyObject *module, PyObject *map)
/*[clinic end generated code: output=bb073c27031db9ac input=d91a91d1717dbc6d]*/
{
    return PyUnicode_BuildEncodingMap(map);
}


/* --- Error handler registry --------------------------------------------- */

/*[clinic input]
_codecs.register_error
    errors: str
    handler: object
    /

Register the specified error handler under the name errors.

handler must be a callable object, that will be called with an exception
instance containing information about the location of the encoding/decoding
error and must return a (replacement, new position) tuple.
[clinic start generated code]*/

static PyObject *
_codecs_register_error_impl(PyObject *module, const char *errors,
                            PyObject *handler)
/*[clinic end generated code: output=fa2f7d1879b3067d input=5e6709203c2e33fe]*/
{
    if (PyCodec_RegisterError(errors, handler))
        return NULL;
    Py_RETURN_NONE;
}

/*[clinic input]
_codecs.lookup_error
    name: str
    /

lookup_error(errors) -> handler

Return the error handler for the specified error handling name or raise a
LookupError, if no handler exists under this name.
[clinic start generated code]*/

static PyObject *
_codecs_lookup_error_impl(PyObject *module, const char *name)
/*[clinic end generated code: output=087f05dc0c9a98cc input=4775dd65e6235aba]*/
{
    return PyCodec_LookupError(name);
}

/* --- Module API --------------------------------------------------------- */

static PyMethodDef _codecs_functions[] = {
    _CODECS_REGISTER_METHODDEF
    _CODECS_LOOKUP_METHODDEF
    _CODECS_ENCODE_METHODDEF
    _CODECS_DECODE_METHODDEF
    _CODECS_ESCAPE_ENCODE_METHODDEF
    _CODECS_ESCAPE_DECODE_METHODDEF
    _CODECS_UTF_8_ENCODE_METHODDEF
    _CODECS_UTF_8_DECODE_METHODDEF
    _CODECS_UTF_7_ENCODE_METHODDEF
    _CODECS_UTF_7_DECODE_METHODDEF
    _CODECS_UTF_16_ENCODE_METHODDEF
    _CODECS_UTF_16_LE_ENCODE_METHODDEF
    _CODECS_UTF_16_BE_ENCODE_METHODDEF
    _CODECS_UTF_16_DECODE_METHODDEF
    _CODECS_UTF_16_LE_DECODE_METHODDEF
    _CODECS_UTF_16_BE_DECODE_METHODDEF
    _CODECS_UTF_16_EX_DECODE_METHODDEF
    _CODECS_UTF_32_ENCODE_METHODDEF
    _CODECS_UTF_32_LE_ENCODE_METHODDEF
    _CODECS_UTF_32_BE_ENCODE_METHODDEF
    _CODECS_UTF_32_DECODE_METHODDEF
    _CODECS_UTF_32_LE_DECODE_METHODDEF
    _CODECS_UTF_32_BE_DECODE_METHODDEF
    _CODECS_UTF_32_EX_DECODE_METHODDEF
    _CODECS_UNICODE_ESCAPE_ENCODE_METHODDEF
    _CODECS_UNICODE_ESCAPE_DECODE_METHODDEF
    _CODECS_RAW_UNICODE_ESCAPE_ENCODE_METHODDEF
    _CODECS_RAW_UNICODE_ESCAPE_DECODE_METHODDEF
    _CODECS_LATIN_1_ENCODE_METHODDEF
    _CODECS_LATIN_1_DECODE_METHODDEF
    _CODECS_ASCII_ENCODE_METHODDEF
    _CODECS_ASCII_DECODE_METHODDEF
    _CODECS_CHARMAP_ENCODE_METHODDEF
    _CODECS_CHARMAP_DECODE_METHODDEF
    _CODECS_CHARMAP_BUILD_METHODDEF
    _CODECS_READBUFFER_ENCODE_METHODDEF
    _CODECS_MBCS_ENCODE_METHODDEF
    _CODECS_MBCS_DECODE_METHODDEF
    _CODECS_OEM_ENCODE_METHODDEF
    _CODECS_OEM_DECODE_METHODDEF
    _CODECS_CODE_PAGE_ENCODE_METHODDEF
    _CODECS_CODE_PAGE_DECODE_METHODDEF
    _CODECS_REGISTER_ERROR_METHODDEF
    _CODECS_LOOKUP_ERROR_METHODDEF
    _CODECS__FORGET_CODEC_METHODDEF
    {NULL, NULL}                /* sentinel */
};

static PyModuleDef_Slot _codecs_slots[] = {
    {0, NULL}
};

static struct PyModuleDef codecsmodule = {
        PyModuleDef_HEAD_INIT,
        "_codecs",
        NULL,
        0,
        _codecs_functions,
        _codecs_slots,
        NULL,
        NULL,
        NULL
};

PyMODINIT_FUNC
PyInit__codecs(void)
{
    return PyModuleDef_Init(&codecsmodule);
}
