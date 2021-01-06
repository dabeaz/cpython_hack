/*

Unicode implementation based on original code by Fredrik Lundh,
modified by Marc-Andre Lemburg <mal@lemburg.com>.

Major speed upgrades to the method implementations at the Reykjavik
NeedForSpeed sprint, by Fredrik Lundh and Andrew Dalke.

Copyright (c) Corporation for National Research Initiatives.

--------------------------------------------------------------------
The original string type implementation is:

  Copyright (c) 1999 by Secret Labs AB
  Copyright (c) 1999 by Fredrik Lundh

By obtaining, using, and/or copying this software and/or its
associated documentation, you agree that you have read, understood,
and will comply with the following terms and conditions:

Permission to use, copy, modify, and distribute this software and its
associated documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies, and that both that copyright notice and this permission notice
appear in supporting documentation, and that the name of Secret Labs
AB or the author not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

SECRET LABS AB AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS.  IN NO EVENT SHALL SECRET LABS AB OR THE AUTHOR BE LIABLE FOR
ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
--------------------------------------------------------------------

*/

#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "pycore_abstract.h"       // _PyIndex_Check()
#include "pycore_fileutils.h"
#include "pycore_initconfig.h"
#include "pycore_interp.h"         // PyInterpreterState.fs_codec
#include "pycore_object.h"
#include "pycore_pathconfig.h"
#include "pycore_pylifecycle.h"
#include "pycore_pystate.h"        // _PyInterpreterState_GET()
#include "stringlib/eq.h"


const char *Py_hexdigits = "0123456789abcdef";

/*[clinic input]
class str "PyObject *" "&PyUnicode_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=4884c934de622cf6]*/

/*[python input]
class Py_UCS4_converter(CConverter):
    type = 'Py_UCS4'
    converter = 'convert_uc'

    def converter_init(self):
        if self.default is not unspecified:
            self.c_default = ascii(self.default)
            if len(self.c_default) > 4 or self.c_default[0] != "'":
                self.c_default = hex(ord(self.default))

[python start generated code]*/
/*[python end generated code: output=da39a3ee5e6b4b0d input=88f5dd06cd8e7a61]*/

/* --- Globals ------------------------------------------------------------

NOTE: In the interpreter's initialization phase, some globals are currently
      initialized dynamically as needed. In the process Unicode objects may
      be created before the Unicode type is ready.

*/


#ifdef __cplusplus
extern "C" {
#endif

#  define _PyUnicode_CHECK(op) PyUnicode_Check(op)
  
#define _PyUnicode_LENGTH(op)                           \
    (((PyUnicodeObject *)(op))->length)
#define _PyUnicode_STATE(op)                            \
    (((PyUnicodeObject *)(op))->state)
#define _PyUnicode_HASH(op)                             \
    (((PyUnicodeObject *)(op))->hash)
#define _PyUnicode_GET_LENGTH(op)                       \
    (assert(_PyUnicode_CHECK(op)),                      \
     ((PyUnicodeObject *)(op))->length)
#define _PyUnicode_DATA_ANY(op)                         \
    (((PyUnicodeObject*)(op))->data)
  
/* Generic helper macro to convert characters of different types.
   from_type and to_type have to be valid type names, begin and end
   are pointers to the source characters which should be of type
   "from_type *".  to is a pointer of type "to_type *" and points to the
   buffer where the result characters are written to. */
#define _PyUnicode_CONVERT_BYTES(from_type, to_type, begin, end, to) \
    do {                                                \
        to_type *_to = (to_type *)(to);                \
        const from_type *_iter = (const from_type *)(begin);\
        const from_type *_end = (const from_type *)(end);\
        Py_ssize_t n = (_end) - (_iter);                \
        const from_type *_unrolled_end =                \
            _iter + _Py_SIZE_ROUND_DOWN(n, 4);          \
        while (_iter < (_unrolled_end)) {               \
            _to[0] = (to_type) _iter[0];                \
            _to[1] = (to_type) _iter[1];                \
            _to[2] = (to_type) _iter[2];                \
            _to[3] = (to_type) _iter[3];                \
            _iter += 4; _to += 4;                       \
        }                                               \
        while (_iter < (_end))                          \
            *_to++ = (to_type) *_iter++;                \
    } while (0)

   /* On Linux, overallocate by 25% is the best factor */
#  define OVERALLOCATE_FACTOR 4

/* bpo-40521: Interned strings are shared by all interpreters. */
#  define INTERNED_STRINGS

/* This dictionary holds all interned unicode strings.  Note that references
   to strings in this dictionary are *not* counted in the string's ob_refcnt.
   When the interned string reaches a refcnt of 0 the string deallocation
   function will delete the reference from this dictionary.

   Another way to look at this is that to say that the actual reference
   count of a string is:  s->ob_refcnt + (s->state ? 2 : 0)
*/
#ifdef INTERNED_STRINGS
static PyObject *interned = NULL;
#endif

/* The empty Unicode object is shared to improve performance. */
static PyObject *unicode_empty = NULL;

#define _Py_INCREF_UNICODE_EMPTY()                      \
    do {                                                \
        if (unicode_empty != NULL)                      \
            Py_INCREF(unicode_empty);                   \
        else {                                          \
            unicode_empty = PyUnicode_New(0);        \
            if (unicode_empty != NULL) {                \
                Py_INCREF(unicode_empty);               \
                assert(_PyUnicode_CheckConsistency(unicode_empty, 1)); \
            }                                           \
        }                                               \
    } while (0)

#define _Py_RETURN_UNICODE_EMPTY()                      \
    do {                                                \
        _Py_INCREF_UNICODE_EMPTY();                     \
        return unicode_empty;                           \
    } while (0)

static inline void
unicode_fill(void *data, unsigned char value,
             Py_ssize_t start, Py_ssize_t length)
{
    char *to = (char *)data + start;
    memset(to, value, length);
}


/* Forward declaration */
static inline int
_PyUnicodeWriter_WriteCharInline(_PyUnicodeWriter *writer, Py_UCS4 ch);

/* List of static strings. */
static _Py_Identifier *static_strings = NULL;

/* bpo-40521: Latin1 singletons are shared by all interpreters. */
#  define LATIN1_SINGLETONS

#ifdef LATIN1_SINGLETONS
/* Single character Unicode strings in the Latin-1 range are being
   shared as well. */
static PyObject *unicode_latin1[256] = {NULL};
#endif

/* Fast detection of the most frequent whitespace characters */
const unsigned char _Py_ascii_whitespace[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
/*     case 0x0009: * CHARACTER TABULATION */
/*     case 0x000A: * LINE FEED */
/*     case 0x000B: * LINE TABULATION */
/*     case 0x000C: * FORM FEED */
/*     case 0x000D: * CARRIAGE RETURN */
    0, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
/*     case 0x001C: * FILE SEPARATOR */
/*     case 0x001D: * GROUP SEPARATOR */
/*     case 0x001E: * RECORD SEPARATOR */
/*     case 0x001F: * UNIT SEPARATOR */
    0, 0, 0, 0, 1, 1, 1, 1,
/*     case 0x0020: * SPACE */
    1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

/* forward */
static PyUnicodeObject *_PyUnicode_New(Py_ssize_t length);
static PyObject* get_latin1_char(unsigned char ch);
static int unicode_modifiable(PyObject *unicode);


static PyObject *
_PyUnicode_FromUCS1(const Py_UCS1 *s, Py_ssize_t size);

/* Same for linebreaks */
static const unsigned char ascii_linebreak[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
/*         0x000A, * LINE FEED */
/*         0x000B, * LINE TABULATION */
/*         0x000C, * FORM FEED */
/*         0x000D, * CARRIAGE RETURN */
    0, 0, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
/*         0x001C, * FILE SEPARATOR */
/*         0x001D, * GROUP SEPARATOR */
/*         0x001E, * RECORD SEPARATOR */
    0, 0, 0, 0, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static int convert_uc(PyObject *obj, void *addr);

/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(unicode_title__doc__,
"title($self, /)\n"
"--\n"
"\n"
"Return a version of the string where each word is titlecased.\n"
"\n"
"More specifically, words start with uppercased characters and all remaining\n"
"cased characters have lower case.");

#define UNICODE_TITLE_METHODDEF    \
    {"title", (PyCFunction)unicode_title, METH_NOARGS, unicode_title__doc__},

static PyObject *
unicode_title_impl(PyObject *self);

static PyObject *
unicode_title(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_title_impl(self);
}

PyDoc_STRVAR(unicode_capitalize__doc__,
"capitalize($self, /)\n"
"--\n"
"\n"
"Return a capitalized version of the string.\n"
"\n"
"More specifically, make the first character have upper case and the rest lower\n"
"case.");

#define UNICODE_CAPITALIZE_METHODDEF    \
    {"capitalize", (PyCFunction)unicode_capitalize, METH_NOARGS, unicode_capitalize__doc__},

static PyObject *
unicode_capitalize_impl(PyObject *self);

static PyObject *
unicode_capitalize(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_capitalize_impl(self);
}

PyDoc_STRVAR(unicode_casefold__doc__,
"casefold($self, /)\n"
"--\n"
"\n"
"Return a version of the string suitable for caseless comparisons.");

#define UNICODE_CASEFOLD_METHODDEF    \
    {"casefold", (PyCFunction)unicode_casefold, METH_NOARGS, unicode_casefold__doc__},

static PyObject *
unicode_casefold_impl(PyObject *self);

static PyObject *
unicode_casefold(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_casefold_impl(self);
}

PyDoc_STRVAR(unicode_center__doc__,
"center($self, width, fillchar=\' \', /)\n"
"--\n"
"\n"
"Return a centered string of length width.\n"
"\n"
"Padding is done using the specified fill character (default is a space).");

#define UNICODE_CENTER_METHODDEF    \
    {"center", (PyCFunction)(void(*)(void))unicode_center, METH_FASTCALL, unicode_center__doc__},

static PyObject *
unicode_center_impl(PyObject *self, Py_ssize_t width, Py_UCS4 fillchar);

static PyObject *
unicode_center(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_ssize_t width;
    Py_UCS4 fillchar = ' ';

    if (!_PyArg_CheckPositional("center", nargs, 1, 2)) {
        goto exit;
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(args[0]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        width = ival;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (!convert_uc(args[1], &fillchar)) {
        goto exit;
    }
skip_optional:
    return_value = unicode_center_impl(self, width, fillchar);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_expandtabs__doc__,
"expandtabs($self, /, tabsize=8)\n"
"--\n"
"\n"
"Return a copy where all tab characters are expanded using spaces.\n"
"\n"
"If tabsize is not given, a tab size of 8 characters is assumed.");

#define UNICODE_EXPANDTABS_METHODDEF    \
    {"expandtabs", (PyCFunction)(void(*)(void))unicode_expandtabs, METH_FASTCALL|METH_KEYWORDS, unicode_expandtabs__doc__},

static PyObject *
unicode_expandtabs_impl(PyObject *self, int tabsize);

static PyObject *
unicode_expandtabs(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"tabsize", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "expandtabs", 0};
    PyObject *argsbuf[1];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_Size(kwnames) : 0) - 0;
    int tabsize = 8;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 0, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    tabsize = _PyLong_AsInt(args[0]);
    if (tabsize == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional_pos:
    return_value = unicode_expandtabs_impl(self, tabsize);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_isascii__doc__,
"isascii($self, /)\n"
"--\n"
"\n"
"Return True if all characters in the string are ASCII, False otherwise.\n"
"\n"
"ASCII characters have code points in the range U+0000-U+007F.\n"
"Empty string is ASCII too.");

#define UNICODE_ISASCII_METHODDEF    \
    {"isascii", (PyCFunction)unicode_isascii, METH_NOARGS, unicode_isascii__doc__},

static PyObject *
unicode_isascii_impl(PyObject *self);

static PyObject *
unicode_isascii(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_isascii_impl(self);
}

PyDoc_STRVAR(unicode_islower__doc__,
"islower($self, /)\n"
"--\n"
"\n"
"Return True if the string is a lowercase string, False otherwise.\n"
"\n"
"A string is lowercase if all cased characters in the string are lowercase and\n"
"there is at least one cased character in the string.");

#define UNICODE_ISLOWER_METHODDEF    \
    {"islower", (PyCFunction)unicode_islower, METH_NOARGS, unicode_islower__doc__},

static PyObject *
unicode_islower_impl(PyObject *self);

static PyObject *
unicode_islower(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_islower_impl(self);
}

PyDoc_STRVAR(unicode_isupper__doc__,
"isupper($self, /)\n"
"--\n"
"\n"
"Return True if the string is an uppercase string, False otherwise.\n"
"\n"
"A string is uppercase if all cased characters in the string are uppercase and\n"
"there is at least one cased character in the string.");

#define UNICODE_ISUPPER_METHODDEF    \
    {"isupper", (PyCFunction)unicode_isupper, METH_NOARGS, unicode_isupper__doc__},

static PyObject *
unicode_isupper_impl(PyObject *self);

static PyObject *
unicode_isupper(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_isupper_impl(self);
}

PyDoc_STRVAR(unicode_istitle__doc__,
"istitle($self, /)\n"
"--\n"
"\n"
"Return True if the string is a title-cased string, False otherwise.\n"
"\n"
"In a title-cased string, upper- and title-case characters may only\n"
"follow uncased characters and lowercase characters only cased ones.");

#define UNICODE_ISTITLE_METHODDEF    \
    {"istitle", (PyCFunction)unicode_istitle, METH_NOARGS, unicode_istitle__doc__},

static PyObject *
unicode_istitle_impl(PyObject *self);

static PyObject *
unicode_istitle(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_istitle_impl(self);
}

PyDoc_STRVAR(unicode_isspace__doc__,
"isspace($self, /)\n"
"--\n"
"\n"
"Return True if the string is a whitespace string, False otherwise.\n"
"\n"
"A string is whitespace if all characters in the string are whitespace and there\n"
"is at least one character in the string.");

#define UNICODE_ISSPACE_METHODDEF    \
    {"isspace", (PyCFunction)unicode_isspace, METH_NOARGS, unicode_isspace__doc__},

static PyObject *
unicode_isspace_impl(PyObject *self);

static PyObject *
unicode_isspace(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_isspace_impl(self);
}

PyDoc_STRVAR(unicode_isalpha__doc__,
"isalpha($self, /)\n"
"--\n"
"\n"
"Return True if the string is an alphabetic string, False otherwise.\n"
"\n"
"A string is alphabetic if all characters in the string are alphabetic and there\n"
"is at least one character in the string.");

#define UNICODE_ISALPHA_METHODDEF    \
    {"isalpha", (PyCFunction)unicode_isalpha, METH_NOARGS, unicode_isalpha__doc__},

static PyObject *
unicode_isalpha_impl(PyObject *self);

static PyObject *
unicode_isalpha(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_isalpha_impl(self);
}

PyDoc_STRVAR(unicode_isalnum__doc__,
"isalnum($self, /)\n"
"--\n"
"\n"
"Return True if the string is an alpha-numeric string, False otherwise.\n"
"\n"
"A string is alpha-numeric if all characters in the string are alpha-numeric and\n"
"there is at least one character in the string.");

#define UNICODE_ISALNUM_METHODDEF    \
    {"isalnum", (PyCFunction)unicode_isalnum, METH_NOARGS, unicode_isalnum__doc__},

static PyObject *
unicode_isalnum_impl(PyObject *self);

static PyObject *
unicode_isalnum(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_isalnum_impl(self);
}

PyDoc_STRVAR(unicode_isdecimal__doc__,
"isdecimal($self, /)\n"
"--\n"
"\n"
"Return True if the string is a decimal string, False otherwise.\n"
"\n"
"A string is a decimal string if all characters in the string are decimal and\n"
"there is at least one character in the string.");

#define UNICODE_ISDECIMAL_METHODDEF    \
    {"isdecimal", (PyCFunction)unicode_isdecimal, METH_NOARGS, unicode_isdecimal__doc__},

static PyObject *
unicode_isdecimal_impl(PyObject *self);

static PyObject *
unicode_isdecimal(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_isdecimal_impl(self);
}

PyDoc_STRVAR(unicode_isdigit__doc__,
"isdigit($self, /)\n"
"--\n"
"\n"
"Return True if the string is a digit string, False otherwise.\n"
"\n"
"A string is a digit string if all characters in the string are digits and there\n"
"is at least one character in the string.");

#define UNICODE_ISDIGIT_METHODDEF    \
    {"isdigit", (PyCFunction)unicode_isdigit, METH_NOARGS, unicode_isdigit__doc__},

static PyObject *
unicode_isdigit_impl(PyObject *self);

static PyObject *
unicode_isdigit(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_isdigit_impl(self);
}

PyDoc_STRVAR(unicode_isnumeric__doc__,
"isnumeric($self, /)\n"
"--\n"
"\n"
"Return True if the string is a numeric string, False otherwise.\n"
"\n"
"A string is numeric if all characters in the string are numeric and there is at\n"
"least one character in the string.");

#define UNICODE_ISNUMERIC_METHODDEF    \
    {"isnumeric", (PyCFunction)unicode_isnumeric, METH_NOARGS, unicode_isnumeric__doc__},

static PyObject *
unicode_isnumeric_impl(PyObject *self);

static PyObject *
unicode_isnumeric(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_isnumeric_impl(self);
}

PyDoc_STRVAR(unicode_isidentifier__doc__,
"isidentifier($self, /)\n"
"--\n"
"\n"
"Return True if the string is a valid Python identifier, False otherwise.\n"
"\n"
"Call keyword.iskeyword(s) to test whether string s is a reserved identifier,\n"
"such as \"def\" or \"class\".");

#define UNICODE_ISIDENTIFIER_METHODDEF    \
    {"isidentifier", (PyCFunction)unicode_isidentifier, METH_NOARGS, unicode_isidentifier__doc__},

static PyObject *
unicode_isidentifier_impl(PyObject *self);

static PyObject *
unicode_isidentifier(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_isidentifier_impl(self);
}

PyDoc_STRVAR(unicode_isprintable__doc__,
"isprintable($self, /)\n"
"--\n"
"\n"
"Return True if the string is printable, False otherwise.\n"
"\n"
"A string is printable if all of its characters are considered printable in\n"
"repr() or if it is empty.");

#define UNICODE_ISPRINTABLE_METHODDEF    \
    {"isprintable", (PyCFunction)unicode_isprintable, METH_NOARGS, unicode_isprintable__doc__},

static PyObject *
unicode_isprintable_impl(PyObject *self);

static PyObject *
unicode_isprintable(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_isprintable_impl(self);
}

PyDoc_STRVAR(unicode_join__doc__,
"join($self, iterable, /)\n"
"--\n"
"\n"
"Concatenate any number of strings.\n"
"\n"
"The string whose method is called is inserted in between each given string.\n"
"The result is returned as a new string.\n"
"\n"
"Example: \'.\'.join([\'ab\', \'pq\', \'rs\']) -> \'ab.pq.rs\'");

#define UNICODE_JOIN_METHODDEF    \
    {"join", (PyCFunction)unicode_join, METH_O, unicode_join__doc__},

PyDoc_STRVAR(unicode_ljust__doc__,
"ljust($self, width, fillchar=\' \', /)\n"
"--\n"
"\n"
"Return a left-justified string of length width.\n"
"\n"
"Padding is done using the specified fill character (default is a space).");

#define UNICODE_LJUST_METHODDEF    \
    {"ljust", (PyCFunction)(void(*)(void))unicode_ljust, METH_FASTCALL, unicode_ljust__doc__},

static PyObject *
unicode_ljust_impl(PyObject *self, Py_ssize_t width, Py_UCS4 fillchar);

static PyObject *
unicode_ljust(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_ssize_t width;
    Py_UCS4 fillchar = ' ';

    if (!_PyArg_CheckPositional("ljust", nargs, 1, 2)) {
        goto exit;
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(args[0]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        width = ival;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (!convert_uc(args[1], &fillchar)) {
        goto exit;
    }
skip_optional:
    return_value = unicode_ljust_impl(self, width, fillchar);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_lower__doc__,
"lower($self, /)\n"
"--\n"
"\n"
"Return a copy of the string converted to lowercase.");

#define UNICODE_LOWER_METHODDEF    \
    {"lower", (PyCFunction)unicode_lower, METH_NOARGS, unicode_lower__doc__},

static PyObject *
unicode_lower_impl(PyObject *self);

static PyObject *
unicode_lower(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_lower_impl(self);
}

PyDoc_STRVAR(unicode_strip__doc__,
"strip($self, chars=None, /)\n"
"--\n"
"\n"
"Return a copy of the string with leading and trailing whitespace removed.\n"
"\n"
"If chars is given and not None, remove characters in chars instead.");

#define UNICODE_STRIP_METHODDEF    \
    {"strip", (PyCFunction)(void(*)(void))unicode_strip, METH_FASTCALL, unicode_strip__doc__},

static PyObject *
unicode_strip_impl(PyObject *self, PyObject *chars);

static PyObject *
unicode_strip(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *chars = Py_None;

    if (!_PyArg_CheckPositional("strip", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    chars = args[0];
skip_optional:
    return_value = unicode_strip_impl(self, chars);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_lstrip__doc__,
"lstrip($self, chars=None, /)\n"
"--\n"
"\n"
"Return a copy of the string with leading whitespace removed.\n"
"\n"
"If chars is given and not None, remove characters in chars instead.");

#define UNICODE_LSTRIP_METHODDEF    \
    {"lstrip", (PyCFunction)(void(*)(void))unicode_lstrip, METH_FASTCALL, unicode_lstrip__doc__},

static PyObject *
unicode_lstrip_impl(PyObject *self, PyObject *chars);

static PyObject *
unicode_lstrip(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *chars = Py_None;

    if (!_PyArg_CheckPositional("lstrip", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    chars = args[0];
skip_optional:
    return_value = unicode_lstrip_impl(self, chars);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_rstrip__doc__,
"rstrip($self, chars=None, /)\n"
"--\n"
"\n"
"Return a copy of the string with trailing whitespace removed.\n"
"\n"
"If chars is given and not None, remove characters in chars instead.");

#define UNICODE_RSTRIP_METHODDEF    \
    {"rstrip", (PyCFunction)(void(*)(void))unicode_rstrip, METH_FASTCALL, unicode_rstrip__doc__},

static PyObject *
unicode_rstrip_impl(PyObject *self, PyObject *chars);

static PyObject *
unicode_rstrip(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *chars = Py_None;

    if (!_PyArg_CheckPositional("rstrip", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    chars = args[0];
skip_optional:
    return_value = unicode_rstrip_impl(self, chars);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_replace__doc__,
"replace($self, old, new, count=-1, /)\n"
"--\n"
"\n"
"Return a copy with all occurrences of substring old replaced by new.\n"
"\n"
"  count\n"
"    Maximum number of occurrences to replace.\n"
"    -1 (the default value) means replace all occurrences.\n"
"\n"
"If the optional argument count is given, only the first count occurrences are\n"
"replaced.");

#define UNICODE_REPLACE_METHODDEF    \
    {"replace", (PyCFunction)(void(*)(void))unicode_replace, METH_FASTCALL, unicode_replace__doc__},

static PyObject *
unicode_replace_impl(PyObject *self, PyObject *old, PyObject *new,
                     Py_ssize_t count);

static PyObject *
unicode_replace(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *old;
    PyObject *new;
    Py_ssize_t count = -1;

    if (!_PyArg_CheckPositional("replace", nargs, 2, 3)) {
        goto exit;
    }
    if (!PyUnicode_Check(args[0])) {
        _PyArg_BadArgument("replace", "argument 1", "str", args[0]);
        goto exit;
    }
    old = args[0];
    if (!PyUnicode_Check(args[1])) {
        _PyArg_BadArgument("replace", "argument 2", "str", args[1]);
        goto exit;
    }
    new = args[1];
    if (nargs < 3) {
        goto skip_optional;
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(args[2]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        count = ival;
    }
skip_optional:
    return_value = unicode_replace_impl(self, old, new, count);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_removeprefix__doc__,
"removeprefix($self, prefix, /)\n"
"--\n"
"\n"
"Return a str with the given prefix string removed if present.\n"
"\n"
"If the string starts with the prefix string, return string[len(prefix):].\n"
"Otherwise, return a copy of the original string.");

#define UNICODE_REMOVEPREFIX_METHODDEF    \
    {"removeprefix", (PyCFunction)unicode_removeprefix, METH_O, unicode_removeprefix__doc__},

static PyObject *
unicode_removeprefix_impl(PyObject *self, PyObject *prefix);

static PyObject *
unicode_removeprefix(PyObject *self, PyObject *arg)
{
    PyObject *return_value = NULL;
    PyObject *prefix;

    if (!PyUnicode_Check(arg)) {
        _PyArg_BadArgument("removeprefix", "argument", "str", arg);
        goto exit;
    }
    prefix = arg;
    return_value = unicode_removeprefix_impl(self, prefix);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_removesuffix__doc__,
"removesuffix($self, suffix, /)\n"
"--\n"
"\n"
"Return a str with the given suffix string removed if present.\n"
"\n"
"If the string ends with the suffix string and that suffix is not empty,\n"
"return string[:-len(suffix)]. Otherwise, return a copy of the original\n"
"string.");

#define UNICODE_REMOVESUFFIX_METHODDEF    \
    {"removesuffix", (PyCFunction)unicode_removesuffix, METH_O, unicode_removesuffix__doc__},

static PyObject *
unicode_removesuffix_impl(PyObject *self, PyObject *suffix);

static PyObject *
unicode_removesuffix(PyObject *self, PyObject *arg)
{
    PyObject *return_value = NULL;
    PyObject *suffix;

    if (!PyUnicode_Check(arg)) {
        _PyArg_BadArgument("removesuffix", "argument", "str", arg);
        goto exit;
    }
    suffix = arg;
    return_value = unicode_removesuffix_impl(self, suffix);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_rjust__doc__,
"rjust($self, width, fillchar=\' \', /)\n"
"--\n"
"\n"
"Return a right-justified string of length width.\n"
"\n"
"Padding is done using the specified fill character (default is a space).");

#define UNICODE_RJUST_METHODDEF    \
    {"rjust", (PyCFunction)(void(*)(void))unicode_rjust, METH_FASTCALL, unicode_rjust__doc__},

static PyObject *
unicode_rjust_impl(PyObject *self, Py_ssize_t width, Py_UCS4 fillchar);

static PyObject *
unicode_rjust(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_ssize_t width;
    Py_UCS4 fillchar = ' ';

    if (!_PyArg_CheckPositional("rjust", nargs, 1, 2)) {
        goto exit;
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(args[0]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        width = ival;
    }
    if (nargs < 2) {
        goto skip_optional;
    }
    if (!convert_uc(args[1], &fillchar)) {
        goto exit;
    }
skip_optional:
    return_value = unicode_rjust_impl(self, width, fillchar);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_split__doc__,
"split($self, /, sep=None, maxsplit=-1)\n"
"--\n"
"\n"
"Return a list of the words in the string, using sep as the delimiter string.\n"
"\n"
"  sep\n"
"    The delimiter according which to split the string.\n"
"    None (the default value) means split according to any whitespace,\n"
"    and discard empty strings from the result.\n"
"  maxsplit\n"
"    Maximum number of splits to do.\n"
"    -1 (the default value) means no limit.");

#define UNICODE_SPLIT_METHODDEF    \
    {"split", (PyCFunction)(void(*)(void))unicode_split, METH_FASTCALL|METH_KEYWORDS, unicode_split__doc__},

static PyObject *
unicode_split_impl(PyObject *self, PyObject *sep, Py_ssize_t maxsplit);

static PyObject *
unicode_split(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"sep", "maxsplit", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "split", 0};
    PyObject *argsbuf[2];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_Size(kwnames) : 0) - 0;
    PyObject *sep = Py_None;
    Py_ssize_t maxsplit = -1;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 0, 2, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[0]) {
        sep = args[0];
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(args[1]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        maxsplit = ival;
    }
skip_optional_pos:
    return_value = unicode_split_impl(self, sep, maxsplit);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_partition__doc__,
"partition($self, sep, /)\n"
"--\n"
"\n"
"Partition the string into three parts using the given separator.\n"
"\n"
"This will search for the separator in the string.  If the separator is found,\n"
"returns a 3-tuple containing the part before the separator, the separator\n"
"itself, and the part after it.\n"
"\n"
"If the separator is not found, returns a 3-tuple containing the original string\n"
"and two empty strings.");

#define UNICODE_PARTITION_METHODDEF    \
    {"partition", (PyCFunction)unicode_partition, METH_O, unicode_partition__doc__},

PyDoc_STRVAR(unicode_rpartition__doc__,
"rpartition($self, sep, /)\n"
"--\n"
"\n"
"Partition the string into three parts using the given separator.\n"
"\n"
"This will search for the separator in the string, starting at the end. If\n"
"the separator is found, returns a 3-tuple containing the part before the\n"
"separator, the separator itself, and the part after it.\n"
"\n"
"If the separator is not found, returns a 3-tuple containing two empty strings\n"
"and the original string.");

#define UNICODE_RPARTITION_METHODDEF    \
    {"rpartition", (PyCFunction)unicode_rpartition, METH_O, unicode_rpartition__doc__},

PyDoc_STRVAR(unicode_rsplit__doc__,
"rsplit($self, /, sep=None, maxsplit=-1)\n"
"--\n"
"\n"
"Return a list of the words in the string, using sep as the delimiter string.\n"
"\n"
"  sep\n"
"    The delimiter according which to split the string.\n"
"    None (the default value) means split according to any whitespace,\n"
"    and discard empty strings from the result.\n"
"  maxsplit\n"
"    Maximum number of splits to do.\n"
"    -1 (the default value) means no limit.\n"
"\n"
"Splits are done starting at the end of the string and working to the front.");

#define UNICODE_RSPLIT_METHODDEF    \
    {"rsplit", (PyCFunction)(void(*)(void))unicode_rsplit, METH_FASTCALL|METH_KEYWORDS, unicode_rsplit__doc__},

static PyObject *
unicode_rsplit_impl(PyObject *self, PyObject *sep, Py_ssize_t maxsplit);

static PyObject *
unicode_rsplit(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"sep", "maxsplit", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "rsplit", 0};
    PyObject *argsbuf[2];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_Size(kwnames) : 0) - 0;
    PyObject *sep = Py_None;
    Py_ssize_t maxsplit = -1;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 0, 2, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[0]) {
        sep = args[0];
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(args[1]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        maxsplit = ival;
    }
skip_optional_pos:
    return_value = unicode_rsplit_impl(self, sep, maxsplit);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_splitlines__doc__,
"splitlines($self, /, keepends=False)\n"
"--\n"
"\n"
"Return a list of the lines in the string, breaking at line boundaries.\n"
"\n"
"Line breaks are not included in the resulting list unless keepends is given and\n"
"true.");

#define UNICODE_SPLITLINES_METHODDEF    \
    {"splitlines", (PyCFunction)(void(*)(void))unicode_splitlines, METH_FASTCALL|METH_KEYWORDS, unicode_splitlines__doc__},

static PyObject *
unicode_splitlines_impl(PyObject *self, int keepends);

static PyObject *
unicode_splitlines(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"keepends", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "splitlines", 0};
    PyObject *argsbuf[1];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_Size(kwnames) : 0) - 0;
    int keepends = 0;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 0, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    keepends = _PyLong_AsInt(args[0]);
    if (keepends == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional_pos:
    return_value = unicode_splitlines_impl(self, keepends);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_swapcase__doc__,
"swapcase($self, /)\n"
"--\n"
"\n"
"Convert uppercase characters to lowercase and lowercase characters to uppercase.");

#define UNICODE_SWAPCASE_METHODDEF    \
    {"swapcase", (PyCFunction)unicode_swapcase, METH_NOARGS, unicode_swapcase__doc__},

static PyObject *
unicode_swapcase_impl(PyObject *self);

static PyObject *
unicode_swapcase(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_swapcase_impl(self);
}

PyDoc_STRVAR(unicode_upper__doc__,
"upper($self, /)\n"
"--\n"
"\n"
"Return a copy of the string converted to uppercase.");

#define UNICODE_UPPER_METHODDEF    \
    {"upper", (PyCFunction)unicode_upper, METH_NOARGS, unicode_upper__doc__},

static PyObject *
unicode_upper_impl(PyObject *self);

static PyObject *
unicode_upper(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_upper_impl(self);
}

PyDoc_STRVAR(unicode_zfill__doc__,
"zfill($self, width, /)\n"
"--\n"
"\n"
"Pad a numeric string with zeros on the left, to fill a field of the given width.\n"
"\n"
"The string is never truncated.");

#define UNICODE_ZFILL_METHODDEF    \
    {"zfill", (PyCFunction)unicode_zfill, METH_O, unicode_zfill__doc__},

static PyObject *
unicode_zfill_impl(PyObject *self, Py_ssize_t width);

static PyObject *
unicode_zfill(PyObject *self, PyObject *arg)
{
    PyObject *return_value = NULL;
    Py_ssize_t width;

    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(arg);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        width = ival;
    }
    return_value = unicode_zfill_impl(self, width);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode___format____doc__,
"__format__($self, format_spec, /)\n"
"--\n"
"\n"
"Return a formatted version of the string as described by format_spec.");

#define UNICODE___FORMAT___METHODDEF    \
    {"__format__", (PyCFunction)unicode___format__, METH_O, unicode___format____doc__},

static PyObject *
unicode___format___impl(PyObject *self, PyObject *format_spec);

static PyObject *
unicode___format__(PyObject *self, PyObject *arg)
{
    PyObject *return_value = NULL;
    PyObject *format_spec;

    if (!PyUnicode_Check(arg)) {
        _PyArg_BadArgument("__format__", "argument", "str", arg);
        goto exit;
    }
    format_spec = arg;
    return_value = unicode___format___impl(self, format_spec);

exit:
    return return_value;
}

PyDoc_STRVAR(unicode_sizeof__doc__,
"__sizeof__($self, /)\n"
"--\n"
"\n"
"Return the size of the string in memory, in bytes.");

#define UNICODE_SIZEOF_METHODDEF    \
    {"__sizeof__", (PyCFunction)unicode_sizeof, METH_NOARGS, unicode_sizeof__doc__},

static PyObject *
unicode_sizeof_impl(PyObject *self);

static PyObject *
unicode_sizeof(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return unicode_sizeof_impl(self);
}
/*[clinic end generated code: output=c5eb21e314da78b8 input=a9049054013a1b77]*/

int
_PyUnicode_CheckConsistency(PyObject *op, int check_content)
{
#define CHECK(expr) \
    do { if (!(expr)) { _PyObject_ASSERT_FAILED_MSG(op, Py_STRINGIFY(expr)); } } while (0)

    PyUnicodeObject *ascii;

    assert(op != NULL);
    CHECK(PyUnicode_Check(op));

    ascii = (PyUnicodeObject *)op;

    {
        void *data;
       {
            PyUnicodeObject *unicode = (PyUnicodeObject *)op;

            data = unicode->data;
	    CHECK(data != NULL);
	}
    }
    return 1;

#undef CHECK
}

static PyObject*
unicode_result_ready(PyObject *unicode)
{
    Py_ssize_t length;

    length = PyUnicode_GET_LENGTH(unicode);
    if (length == 0) {
        if (unicode != unicode_empty) {
            Py_DECREF(unicode);
            _Py_RETURN_UNICODE_EMPTY();
        }
        return unicode_empty;
    }

#ifdef LATIN1_SINGLETONS
    if (length == 1) {
        const void *data = PyUnicode_DATA(unicode);

        Py_UCS4 ch = PyUnicode_READ(data, 0);
        if (ch < 256) {
            PyObject *latin1_char = unicode_latin1[ch];
            if (latin1_char != NULL) {
                if (unicode != latin1_char) {
                    Py_INCREF(latin1_char);
                    Py_DECREF(unicode);
                }
                return latin1_char;
            }
            else {
                assert(_PyUnicode_CheckConsistency(unicode, 1));
                Py_INCREF(unicode);
                unicode_latin1[ch] = unicode;
                return unicode;
            }
        }
    }
#endif

    assert(_PyUnicode_CheckConsistency(unicode, 1));
    return unicode;
}

static PyObject*
unicode_result(PyObject *unicode)
{
    assert(_PyUnicode_CHECK(unicode));
    return unicode_result_ready(unicode);
}

static PyObject*
unicode_result_unchanged(PyObject *unicode)
{
    if (PyUnicode_CheckExact(unicode)) {
        Py_INCREF(unicode);
        return unicode;
    }
    else
        /* Subtype -- return genuine unicode string with the same value. */
        return _PyUnicode_Copy(unicode);
}

/* --- Bloom Filters ----------------------------------------------------- */

/* stuff to implement simple "bloom filters" for Unicode characters.
   to keep things simple, we use a single bitmask, using the least 5
   bits from each unicode characters as the bit index. */

/* the linebreak mask is set up by Unicode_Init below */

#if LONG_BIT >= 128
#define BLOOM_WIDTH 128
#elif LONG_BIT >= 64
#define BLOOM_WIDTH 64
#elif LONG_BIT >= 32
#define BLOOM_WIDTH 32
#else
#error "LONG_BIT is smaller than 32"
#endif

#define BLOOM_MASK unsigned long

static BLOOM_MASK bloom_linebreak = ~(BLOOM_MASK)0;

#define BLOOM(mask, ch)     ((mask &  (1UL << ((ch) & (BLOOM_WIDTH - 1)))))

#define BLOOM_LINEBREAK(ch)                                             \
    ((ch) < 128U ? ascii_linebreak[(ch)] :                              \
     (BLOOM(bloom_linebreak, (ch)) && Py_UNICODE_ISLINEBREAK(ch)))

static inline BLOOM_MASK
make_bloom_mask(const void* ptr, Py_ssize_t len)
{
#define BLOOM_UPDATE(TYPE, MASK, PTR, LEN)             \
    do {                                               \
        TYPE *data = (TYPE *)PTR;                      \
        TYPE *end = data + LEN;                        \
        unsigned char ch;                                    \
        for (; data != end; data++) {                  \
            ch = *data;                                \
            MASK |= (1UL << (ch & (BLOOM_WIDTH - 1))); \
        }                                              \
        break;                                         \
    } while (0)

    /* calculate simple bloom-style bitmask for a given unicode string */

    BLOOM_MASK mask;

    mask = 0;
    BLOOM_UPDATE(Py_UCS1, mask, ptr, len);
    return mask;

#undef BLOOM_UPDATE
}

static int
ensure_unicode(PyObject *obj)
{
    if (!PyUnicode_Check(obj)) {
        PyErr_Format(PyExc_TypeError,
                     "must be str, not %.100s",
                     Py_TYPE(obj)->tp_name);
        return -1;
    }
    return 0;
}

/* Compilation of templated routines */

#include "stringlib/ucs1lib.h"
#include "stringlib/fastsearch.h"
#include "stringlib/partition.h"
#include "stringlib/split.h"
#include "stringlib/count.h"
#include "stringlib/find.h"
#include "stringlib/replace.h"
#include "stringlib/find_max_char.h"
#include "stringlib/undef.h"

/* --- Unicode Object ----------------------------------------------------- */

static inline Py_ssize_t
findchar(const void *s, 
         Py_ssize_t size, unsigned char ch,
         int direction)
{
        if (direction > 0)
            return ucs1lib_find_char((const Py_UCS1 *) s, size, (Py_UCS1) ch);
        else
            return ucs1lib_rfind_char((const Py_UCS1 *) s, size, (Py_UCS1) ch);
}

static int
resize_inplace(PyObject *unicode, Py_ssize_t length)
{
    Py_ssize_t new_size;
    assert(Py_REFCNT(unicode) == 1);

    {
        void *data;

        data = _PyUnicode_DATA_ANY(unicode);
	
        if (length > (PY_SSIZE_T_MAX - 1)) {
            PyErr_NoMemory();
            return -1;
        }
        new_size = (length + 1);
	
        data = (PyObject *)PyMem_Realloc(data, new_size);
        if (data == NULL) {
            PyErr_NoMemory();
            return -1;
        }
        _PyUnicode_DATA_ANY(unicode) = data;
        _PyUnicode_LENGTH(unicode) = length;
        PyUnicode_WRITE(data, length, 0);
    }
    assert(_PyUnicode_CheckConsistency(unicode, 0));
    return 0;
}

static PyObject*
resize_copy(PyObject *unicode, Py_ssize_t length)
{
    Py_ssize_t copy_length;
    if (1) {
        PyObject *copy;
        copy = PyUnicode_New(length);
        if (copy == NULL)
            return NULL;

        copy_length = Py_MIN(length, PyUnicode_GET_LENGTH(unicode));
        _PyUnicode_FastCopyCharacters(copy, 0, unicode, 0, copy_length);
        return copy;
    }
}

/* We allocate one more byte to make sure the string is
   Ux0000 terminated; some code (e.g. new_identifier)
   relies on that.

   XXX This allocator could further be enhanced by assuring that the
   free list never reduces its size below 1.

*/

static PyUnicodeObject *
_PyUnicode_New(Py_ssize_t length)
{
    PyUnicodeObject *unicode;
    size_t new_size;

    /* Optimization for empty strings */
    if (length == 0 && unicode_empty != NULL) {
        Py_INCREF(unicode_empty);
        return (PyUnicodeObject*)unicode_empty;
    }

    /* Ensure we won't overflow the size. */
    if (length > (PY_SSIZE_T_MAX - 1)) {
        return (PyUnicodeObject *)PyErr_NoMemory();
    }
    if (length < 0) {
        PyErr_SetString(PyExc_SystemError,
                        "Negative size passed to _PyUnicode_New");
        return NULL;
    }

    unicode = PyObject_New(PyUnicodeObject, &PyUnicode_Type);
    if (unicode == NULL)
        return NULL;
    new_size = ((size_t)length + 1);

    _PyUnicode_LENGTH(unicode) = length;
    _PyUnicode_HASH(unicode) = -1;
    _PyUnicode_STATE(unicode).interned = 0;
    _PyUnicode_DATA_ANY(unicode) = (void *) PyMem_Malloc(new_size);
    if (!_PyUnicode_DATA_ANY(unicode)) {
        Py_DECREF(unicode);
        PyErr_NoMemory();
        return NULL;
    }
    ((char *)_PyUnicode_DATA_ANY(unicode))[0] = 0;
    ((char *)_PyUnicode_DATA_ANY(unicode))[length] = 0;

    assert(_PyUnicode_CheckConsistency((PyObject *)unicode, 0));
    return unicode;
}

PyObject *
PyUnicode_New(Py_ssize_t size)
{
    PyObject *obj;
    PyUnicodeObject *unicode;
    void *data;
    int is_sharing, is_ascii;
    Py_ssize_t struct_size;

    /* Optimization for empty strings */
    if (size == 0 && unicode_empty != NULL) {
        Py_INCREF(unicode_empty);
        return unicode_empty;
    }

    is_ascii = 0;
    is_sharing = 0;
    struct_size = sizeof(PyUnicodeObject);
    /* Ensure we won't overflow the size. */
    if (size < 0) {
        PyErr_SetString(PyExc_SystemError,
                        "Negative size passed to PyUnicode_New");
        return NULL;
    }
    if (size > (PY_SSIZE_T_MAX - 1))
        return PyErr_NoMemory();

    /* Duplicated allocation code from _PyObject_New() instead of a call to
     * PyObject_New() so we are able to allocate space for the object and
     * it's data buffer.
     */
    obj = (PyObject *) PyMem_Malloc(struct_size);
    if (obj == NULL)
        return PyErr_NoMemory();
    obj = PyObject_INIT(obj, &PyUnicode_Type);
    if (obj == NULL)
        return NULL;

    unicode = (PyUnicodeObject *)obj;
    _PyUnicode_DATA_ANY(unicode) = (void *) PyMem_Malloc(size+1);
    if (!_PyUnicode_DATA_ANY(unicode)) {
        Py_DECREF(unicode);
        PyErr_NoMemory();
        return NULL;
    }
    data = _PyUnicode_DATA_ANY(unicode);
    _PyUnicode_LENGTH(unicode) = size;
    _PyUnicode_HASH(unicode) = -1;
    _PyUnicode_STATE(unicode).interned = 0;
    
    ((char*)data)[size] = 0;
    assert(_PyUnicode_CheckConsistency((PyObject*)unicode, 0));
    return obj;
}


static int
unicode_check_modifiable(PyObject *unicode)
{
    if (!unicode_modifiable(unicode)) {
        PyErr_SetString(PyExc_SystemError,
                        "Cannot modify a string currently used");
        return -1;
    }
    return 0;
}

static int
_copy_characters(PyObject *to, Py_ssize_t to_start,
                 PyObject *from, Py_ssize_t from_start,
                 Py_ssize_t how_many)
{
    const void *from_data;
    void *to_data;

    assert(0 <= how_many);
    assert(0 <= from_start);
    assert(0 <= to_start);
    assert(PyUnicode_Check(from));
    assert(from_start + how_many <= PyUnicode_GET_LENGTH(from));

    assert(PyUnicode_Check(to));
    assert(to_start + how_many <= PyUnicode_GET_LENGTH(to));

    if (how_many == 0)
        return 0;

    from_data = PyUnicode_DATA(from);
    to_data = PyUnicode_DATA(to);
    memcpy((char*)to_data + to_start,
	   (const char*)from_data + from_start,
	   how_many);
    return 0;
}

void
_PyUnicode_FastCopyCharacters(
    PyObject *to, Py_ssize_t to_start,
    PyObject *from, Py_ssize_t from_start, Py_ssize_t how_many)
{
    (void)_copy_characters(to, to_start, from, from_start, how_many);
}

Py_ssize_t
PyUnicode_CopyCharacters(PyObject *to, Py_ssize_t to_start,
                         PyObject *from, Py_ssize_t from_start,
                         Py_ssize_t how_many)
{
    int err;

    if (!PyUnicode_Check(from) || !PyUnicode_Check(to)) {
        PyErr_BadInternalCall();
        return -1;
    }
    if ((size_t)from_start > (size_t)PyUnicode_GET_LENGTH(from)) {
        PyErr_SetString(PyExc_IndexError, "string index out of range");
        return -1;
    }
    if ((size_t)to_start > (size_t)PyUnicode_GET_LENGTH(to)) {
        PyErr_SetString(PyExc_IndexError, "string index out of range");
        return -1;
    }
    if (how_many < 0) {
        PyErr_SetString(PyExc_SystemError, "how_many cannot be negative");
        return -1;
    }
    how_many = Py_MIN(PyUnicode_GET_LENGTH(from)-from_start, how_many);
    if (to_start + how_many > PyUnicode_GET_LENGTH(to)) {
        PyErr_Format(PyExc_SystemError,
                     "Cannot write %zi characters at %zi "
                     "in a string of %zi characters",
                     how_many, to_start, PyUnicode_GET_LENGTH(to));
        return -1;
    }

    if (how_many == 0)
        return 0;

    if (unicode_check_modifiable(to))
        return -1;

    err = _copy_characters(to, to_start, from, from_start, how_many);
    return how_many;
}

static void
unicode_dealloc(PyObject *unicode)
{
    switch (PyUnicode_CHECK_INTERNED(unicode)) {
    case SSTATE_NOT_INTERNED:
        break;

    case SSTATE_INTERNED_MORTAL:
        /* revive dead object temporarily for DelItem */
        Py_SET_REFCNT(unicode, 3);
#ifdef INTERNED_STRINGS
        if (PyDict_DelItem(interned, unicode) != 0) {
            _PyErr_WriteUnraisableMsg("deletion of interned string failed",
                                      NULL);
        }
#endif
        break;

    case SSTATE_INTERNED_IMMORTAL:
        _PyObject_ASSERT_FAILED_MSG(unicode, "Immortal interned string died");
        break;

    default:
        Py_UNREACHABLE();
    }
    if (_PyUnicode_DATA_ANY(unicode)) {
      PyMem_Free(_PyUnicode_DATA_ANY(unicode));
    }

    Py_TYPE(unicode)->tp_free(unicode);
}

static int
unicode_modifiable(PyObject *unicode)
{
    assert(_PyUnicode_CHECK(unicode));
    if (Py_REFCNT(unicode) != 1)
        return 0;
    if (_PyUnicode_HASH(unicode) != -1)
        return 0;
    if (PyUnicode_CHECK_INTERNED(unicode))
        return 0;
    if (!PyUnicode_CheckExact(unicode))
        return 0;
    return 1;
}

static int
unicode_resize(PyObject **p_unicode, Py_ssize_t length)
{
    PyObject *unicode;
    Py_ssize_t old_length;

    assert(p_unicode != NULL);
    unicode = *p_unicode;

    assert(unicode != NULL);
    assert(PyUnicode_Check(unicode));
    assert(0 <= length);
    old_length = PyUnicode_GET_LENGTH(unicode);
    if (old_length == length)
        return 0;

    if (length == 0) {
        _Py_INCREF_UNICODE_EMPTY();
        if (!unicode_empty)
            return -1;
        Py_SETREF(*p_unicode, unicode_empty);
        return 0;
    }

    if (!unicode_modifiable(unicode)) {
        PyObject *copy = resize_copy(unicode, length);
        if (copy == NULL)
            return -1;
        Py_SETREF(*p_unicode, copy);
        return 0;
    }
    return resize_inplace(unicode, length);
}

int
PyUnicode_Resize(PyObject **p_unicode, Py_ssize_t length)
{
    PyObject *unicode;
    if (p_unicode == NULL) {
        PyErr_BadInternalCall();
        return -1;
    }
    unicode = *p_unicode;
    if (unicode == NULL || !PyUnicode_Check(unicode) || length < 0)
    {
        PyErr_BadInternalCall();
        return -1;
    }
    return unicode_resize(p_unicode, length);
}

/* Copy an ASCII or latin1 char* string into a Python Unicode string.

   WARNING: The function doesn't copy the terminating null character and
   doesn't check the maximum character (may write a latin1 character in an
   ASCII string). */
static void
unicode_write_cstr(PyObject *unicode, Py_ssize_t index,
                   const char *str, Py_ssize_t len)
{
    const void *data = PyUnicode_DATA(unicode);

    assert(index + len <= PyUnicode_GET_LENGTH(unicode));
    memcpy((char *) data + index, str, len);
}

static PyObject*
get_latin1_char(unsigned char ch)
{
    PyObject *unicode;

#ifdef LATIN1_SINGLETONS
    unicode = unicode_latin1[ch];
    if (unicode) {
        Py_INCREF(unicode);
        return unicode;
    }
#endif

    unicode = PyUnicode_New(1);
    if (!unicode) {
        return NULL;
    }

    PyUnicode_1BYTE_DATA(unicode)[0] = ch;
    assert(_PyUnicode_CheckConsistency(unicode, 1));

#ifdef LATIN1_SINGLETONS
    Py_INCREF(unicode);
    unicode_latin1[ch] = unicode;
#endif
    return unicode;
}

static PyObject*
unicode_char(Py_UCS4 ch)
{
    assert(ch < 256);
    return get_latin1_char(ch);
}

PyObject *
PyUnicode_FromStringAndSize(const char *u, Py_ssize_t size)
{
    if (size < 0) {
        PyErr_SetString(PyExc_SystemError,
                        "Negative size passed to PyUnicode_FromStringAndSize");
        return NULL;
    }
    if (u != NULL)
      return _PyUnicode_FromUCS1((const unsigned char *)u, size);
    else
        return (PyObject *)_PyUnicode_New(size);
}

PyObject *
PyUnicode_FromString(const char *u)
{
    size_t size = strlen(u);
    if (size > PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError, "input too long");
        return NULL;
    }
    return _PyUnicode_FromUCS1((unsigned char *) u, (Py_ssize_t) size);
}

PyObject *
_PyUnicode_FromId(_Py_Identifier *id)
{
    if (id->object) {
        return id->object;
    }

    PyObject *obj;
    obj = PyUnicode_FromStringAndSize(id->string,
				      strlen(id->string));
    if (!obj) {
        return NULL;
    }
    PyUnicode_InternInPlace(&obj);

    assert(!id->next);
    id->object = obj;
    id->next = static_strings;
    static_strings = id;
    return id->object;
}

static void
unicode_clear_static_strings(void)
{
    _Py_Identifier *tmp, *s = static_strings;
    while (s) {
        Py_CLEAR(s->object);
        tmp = s->next;
        s->next = NULL;
        s = tmp;
    }
    static_strings = NULL;
}

static PyObject*
_PyUnicode_FromUCS1(const Py_UCS1* u, Py_ssize_t size)
{
    PyObject *res;
    unsigned char max_char;

    if (size == 0)
        _Py_RETURN_UNICODE_EMPTY();
    assert(size > 0);
    if (size == 1)
        return get_latin1_char(u[0]);

    max_char = ucs1lib_find_max_char(u, u + size);
    res = PyUnicode_New(size);
    if (!res)
        return NULL;
    memcpy(PyUnicode_1BYTE_DATA(res), u, size);
    assert(_PyUnicode_CheckConsistency(res, 1));
    return res;
}

PyObject*
_PyUnicode_Copy(PyObject *unicode)
{
    Py_ssize_t length;
    PyObject *copy;

    if (!PyUnicode_Check(unicode)) {
        PyErr_BadInternalCall();
        return NULL;
    }
    length = PyUnicode_GET_LENGTH(unicode);
    copy = PyUnicode_New(length);
    if (!copy)
        return NULL;
    memcpy(PyUnicode_DATA(copy), PyUnicode_DATA(unicode), length);
    assert(_PyUnicode_CheckConsistency(copy, 1));
    return copy;
}

/* maximum number of characters required for output of %lld or %p.
   We need at most ceil(log10(256)*SIZEOF_LONG_LONG) digits,
   plus 1 for the sign.  53/22 is an upper bound for log10(256). */
#define MAX_LONG_LONG_CHARS (2 + (SIZEOF_LONG_LONG*53-1) / 22)

static int
unicode_fromformat_write_str(_PyUnicodeWriter *writer, PyObject *str,
                             Py_ssize_t width, Py_ssize_t precision)
{
    Py_ssize_t length, fill, arglen;

    length = PyUnicode_GET_LENGTH(str);
    if ((precision == -1 || precision >= length)
        && width <= length)
        return _PyUnicodeWriter_WriteStr(writer, str);

    if (precision != -1)
        length = Py_MIN(precision, length);

    arglen = Py_MAX(length, width);
    
    if (_PyUnicodeWriter_Prepare(writer, arglen) == -1)
        return -1;

    if (width > length) {
        fill = width - length;
        if (PyUnicode_Fill(writer->buffer, writer->pos, fill, ' ') == -1)
            return -1;
        writer->pos += fill;
    }

    _PyUnicode_FastCopyCharacters(writer->buffer, writer->pos,
                                  str, 0, length);
    writer->pos += length;
    return 0;
}

static int
unicode_fromformat_write_cstr(_PyUnicodeWriter *writer, const char *str,
                              Py_ssize_t width, Py_ssize_t precision)
{
    /* UTF-8 */
    Py_ssize_t length;
    PyObject *unicode;
    int res;

    if (precision == -1) {
        length = strlen(str);
    }
    else {
        length = 0;
        while (length < precision && str[length]) {
            length++;
        }
    }
    unicode = PyUnicode_FromStringAndSize(str, length);
    if (unicode == NULL)
        return -1;

    res = unicode_fromformat_write_str(writer, unicode, width, -1);
    Py_DECREF(unicode);
    return res;
}

static const char*
unicode_fromformat_arg(_PyUnicodeWriter *writer,
                       const char *f, va_list *vargs)
{
    const char *p;
    Py_ssize_t len;
    int zeropad;
    Py_ssize_t width;
    Py_ssize_t precision;
    int longflag;
    int longlongflag;
    int size_tflag;
    Py_ssize_t fill;

    p = f;
    f++;
    zeropad = 0;
    if (*f == '0') {
        zeropad = 1;
        f++;
    }

    /* parse the width.precision part, e.g. "%2.5s" => width=2, precision=5 */
    width = -1;
    if (Py_ISDIGIT((unsigned)*f)) {
        width = *f - '0';
        f++;
        while (Py_ISDIGIT((unsigned)*f)) {
            if (width > (PY_SSIZE_T_MAX - ((int)*f - '0')) / 10) {
                PyErr_SetString(PyExc_ValueError,
                                "width too big");
                return NULL;
            }
            width = (width * 10) + (*f - '0');
            f++;
        }
    }
    precision = -1;
    if (*f == '.') {
        f++;
        if (Py_ISDIGIT((unsigned)*f)) {
            precision = (*f - '0');
            f++;
            while (Py_ISDIGIT((unsigned)*f)) {
                if (precision > (PY_SSIZE_T_MAX - ((int)*f - '0')) / 10) {
                    PyErr_SetString(PyExc_ValueError,
                                    "precision too big");
                    return NULL;
                }
                precision = (precision * 10) + (*f - '0');
                f++;
            }
        }
        if (*f == '%') {
            /* "%.3%s" => f points to "3" */
            f--;
        }
    }
    if (*f == '\0') {
        /* bogus format "%.123" => go backward, f points to "3" */
        f--;
    }

    /* Handle %ld, %lu, %lld and %llu. */
    longflag = 0;
    longlongflag = 0;
    size_tflag = 0;
    if (*f == 'l') {
        if (f[1] == 'd' || f[1] == 'u' || f[1] == 'i') {
            longflag = 1;
            ++f;
        }
        else if (f[1] == 'l' &&
                 (f[2] == 'd' || f[2] == 'u' || f[2] == 'i')) {
            longlongflag = 1;
            f += 2;
        }
    }
    /* handle the size_t flag. */
    else if (*f == 'z' && (f[1] == 'd' || f[1] == 'u' || f[1] == 'i')) {
        size_tflag = 1;
        ++f;
    }

    if (f[1] == '\0')
        writer->overallocate = 0;

    switch (*f) {
    case 'c':
    {
        int ordinal = va_arg(*vargs, int);
        if (ordinal < 0 || ordinal > 255) {
            PyErr_SetString(PyExc_OverflowError,
                            "character argument not in range(0x100)");
            return NULL;
        }
        if (_PyUnicodeWriter_WriteCharInline(writer, ordinal) < 0)
            return NULL;
        break;
    }

    case 'i':
    case 'd':
    case 'u':
    case 'x':
    {
        /* used by sprintf */
        char buffer[MAX_LONG_LONG_CHARS];
        Py_ssize_t arglen;

        if (*f == 'u') {
            if (longflag) {
                len = sprintf(buffer, "%lu", va_arg(*vargs, unsigned long));
            }
            else if (longlongflag) {
                len = sprintf(buffer, "%llu", va_arg(*vargs, unsigned long long));
            }
            else if (size_tflag) {
                len = sprintf(buffer, "%zu", va_arg(*vargs, size_t));
            }
            else {
                len = sprintf(buffer, "%u", va_arg(*vargs, unsigned int));
            }
        }
        else if (*f == 'x') {
            len = sprintf(buffer, "%x", va_arg(*vargs, int));
        }
        else {
            if (longflag) {
                len = sprintf(buffer, "%li", va_arg(*vargs, long));
            }
            else if (longlongflag) {
                len = sprintf(buffer, "%lli", va_arg(*vargs, long long));
            }
            else if (size_tflag) {
                len = sprintf(buffer, "%zi", va_arg(*vargs, Py_ssize_t));
            }
            else {
                len = sprintf(buffer, "%i", va_arg(*vargs, int));
            }
        }
        assert(len >= 0);

        if (precision < len)
            precision = len;

        arglen = Py_MAX(precision, width);
        if (_PyUnicodeWriter_Prepare(writer, arglen) == -1)
            return NULL;

        if (width > precision) {
            Py_UCS4 fillchar;
            fill = width - precision;
            fillchar = zeropad?'0':' ';
            if (PyUnicode_Fill(writer->buffer, writer->pos, fill, fillchar) == -1)
                return NULL;
            writer->pos += fill;
        }
        if (precision > len) {
            fill = precision - len;
            if (PyUnicode_Fill(writer->buffer, writer->pos, fill, '0') == -1)
                return NULL;
            writer->pos += fill;
        }

        if (_PyUnicodeWriter_WriteASCIIString(writer, buffer, len) < 0)
            return NULL;
        break;
    }

    case 'p':
    {
        char number[MAX_LONG_LONG_CHARS];

        len = sprintf(number, "%p", va_arg(*vargs, void*));
        assert(len >= 0);

        /* %p is ill-defined:  ensure leading 0x. */
        if (number[1] == 'X')
            number[1] = 'x';
        else if (number[1] != 'x') {
            memmove(number + 2, number,
                    strlen(number) + 1);
            number[0] = '0';
            number[1] = 'x';
            len += 2;
        }

        if (_PyUnicodeWriter_WriteASCIIString(writer, number, len) < 0)
            return NULL;
        break;
    }

    case 's':
    {
        /* UTF-8 */
        const char *s = va_arg(*vargs, const char*);
        if (unicode_fromformat_write_cstr(writer, s, width, precision) < 0)
            return NULL;
        break;
    }

    case 'U':
    {
        PyObject *obj = va_arg(*vargs, PyObject *);
        assert(obj && _PyUnicode_CHECK(obj));

        if (unicode_fromformat_write_str(writer, obj, width, precision) == -1)
            return NULL;
        break;
    }

    case 'V':
    {
        PyObject *obj = va_arg(*vargs, PyObject *);
        const char *str = va_arg(*vargs, const char *);
        if (obj) {
            assert(_PyUnicode_CHECK(obj));
            if (unicode_fromformat_write_str(writer, obj, width, precision) == -1)
                return NULL;
        }
        else {
            assert(str != NULL);
            if (unicode_fromformat_write_cstr(writer, str, width, precision) < 0)
                return NULL;
        }
        break;
    }

    case 'S':
    {
        PyObject *obj = va_arg(*vargs, PyObject *);
        PyObject *str;
        assert(obj);
        str = PyObject_Str(obj);
        if (!str)
            return NULL;
        if (unicode_fromformat_write_str(writer, str, width, precision) == -1) {
            Py_DECREF(str);
            return NULL;
        }
        Py_DECREF(str);
        break;
    }

    case 'R':
    {
        PyObject *obj = va_arg(*vargs, PyObject *);
        PyObject *repr;
        assert(obj);
        repr = PyObject_Repr(obj);
        if (!repr)
            return NULL;
        if (unicode_fromformat_write_str(writer, repr, width, precision) == -1) {
            Py_DECREF(repr);
            return NULL;
        }
        Py_DECREF(repr);
        break;
    }

    case 'A':
    {
        PyObject *obj = va_arg(*vargs, PyObject *);
        PyObject *ascii;
        assert(obj);
        ascii = PyObject_ASCII(obj);
        if (!ascii)
            return NULL;
        if (unicode_fromformat_write_str(writer, ascii, width, precision) == -1) {
            Py_DECREF(ascii);
            return NULL;
        }
        Py_DECREF(ascii);
        break;
    }

    case '%':
        if (_PyUnicodeWriter_WriteCharInline(writer, '%') < 0)
            return NULL;
        break;

    default:
        /* if we stumble upon an unknown formatting code, copy the rest
           of the format string to the output string. (we cannot just
           skip the code, since there's no way to know what's in the
           argument list) */
        len = strlen(p);
        if (_PyUnicodeWriter_WriteLatin1String(writer, p, len) == -1)
            return NULL;
        f = p+len;
        return f;
    }

    f++;
    return f;
}

PyObject *
PyUnicode_FromFormatV(const char *format, va_list vargs)
{
    va_list vargs2;
    const char *f;
    _PyUnicodeWriter writer;

    _PyUnicodeWriter_Init(&writer);
    writer.min_length = strlen(format) + 100;
    writer.overallocate = 1;

    // Copy varags to be able to pass a reference to a subfunction.
    va_copy(vargs2, vargs);

    for (f = format; *f; ) {
        if (*f == '%') {
            f = unicode_fromformat_arg(&writer, f, &vargs2);
            if (f == NULL)
                goto fail;
        }
        else {
            const char *p;
            Py_ssize_t len;

            p = f;
            do
            {
                if ((unsigned char)*p > 127) {
                    PyErr_Format(PyExc_ValueError,
                        "PyUnicode_FromFormatV() expects an ASCII-encoded format "
                        "string, got a non-ASCII byte: 0x%02x",
                        (unsigned char)*p);
                    goto fail;
                }
                p++;
            }
            while (*p != '\0' && *p != '%');
            len = p - f;

            if (*p == '\0')
                writer.overallocate = 0;

            if (_PyUnicodeWriter_WriteASCIIString(&writer, f, len) < 0)
                goto fail;

            f = p;
        }
    }
    va_end(vargs2);
    return _PyUnicodeWriter_Finish(&writer);

  fail:
    va_end(vargs2);
    _PyUnicodeWriter_Dealloc(&writer);
    return NULL;
}

PyObject *
PyUnicode_FromFormat(const char *format, ...)
{
    PyObject* ret;
    va_list vargs;

#ifdef HAVE_STDARG_PROTOTYPES
    va_start(vargs, format);
#else
    va_start(vargs);
#endif
    ret = PyUnicode_FromFormatV(format, vargs);
    va_end(vargs);
    return ret;
}

PyObject *
PyUnicode_FromOrdinal(int ordinal)
{
    if (ordinal < 0 || ordinal > 0xff) {
        PyErr_SetString(PyExc_ValueError,
                        "chr() arg not in range(0x100)");
        return NULL;
    }

    return unicode_char((Py_UCS4)ordinal);
}

PyObject *
PyUnicode_FromObject(PyObject *obj)
{
    /* XXX Perhaps we should make this API an alias of
       PyObject_Str() instead ?! */
    if (PyUnicode_CheckExact(obj)) {
        Py_INCREF(obj);
        return obj;
    }
    if (PyUnicode_Check(obj)) {
        /* For a Unicode subtype that's not a Unicode object,
           return a true Unicode object with the same data. */
        return _PyUnicode_Copy(obj);
    }
    PyErr_Format(PyExc_TypeError,
                 "Can't convert '%.100s' object to str implicitly",
                 Py_TYPE(obj)->tp_name);
    return NULL;
}

const char *
PyUnicode_AsCharAndSize(PyObject *unicode, Py_ssize_t *psize)
{
  if (psize)
    *psize = PyUnicode_GET_LENGTH(unicode);
  return (char *) PyUnicode_DATA(unicode);
}

const char *
PyUnicode_AsChar(PyObject *unicode)
{
    return PyUnicode_AsCharAndSize(unicode, NULL);
}

Py_ssize_t
PyUnicode_GetLength(PyObject *unicode)
{
    if (!PyUnicode_Check(unicode)) {
        PyErr_BadArgument();
        return -1;
    }
    return PyUnicode_GET_LENGTH(unicode);
}

Py_UCS4
PyUnicode_ReadChar(PyObject *unicode, Py_ssize_t index)
{
    const void *data;

    if (!PyUnicode_Check(unicode)) {
        PyErr_BadArgument();
        return (Py_UCS4)-1;
    }
    if (index < 0 || index >= PyUnicode_GET_LENGTH(unicode)) {
        PyErr_SetString(PyExc_IndexError, "string index out of range");
        return (Py_UCS4)-1;
    }
    data = PyUnicode_DATA(unicode);
    return PyUnicode_READ(data, index);
}

int
PyUnicode_WriteChar(PyObject *unicode, Py_ssize_t index, Py_UCS4 ch)
{
  if (!PyUnicode_Check(unicode)) {
        PyErr_BadArgument();
        return -1;
    }
    if (index < 0 || index >= PyUnicode_GET_LENGTH(unicode)) {
        PyErr_SetString(PyExc_IndexError, "string index out of range");
        return -1;
    }
    if (unicode_check_modifiable(unicode))
        return -1;
    if (ch > PyUnicode_MAX_CHAR_VALUE(unicode)) {
        PyErr_SetString(PyExc_ValueError, "character out of range");
        return -1;
    }
    PyUnicode_WRITE(PyUnicode_DATA(unicode),index, ch);
    return 0;
}

/* --- UTF-8 Codec -------------------------------------------------------- */

#include "stringlib/ucs1lib.h"
#include "stringlib/undef.h"


/* --- 7-bit ASCII Codec -------------------------------------------------- */
PyObject *
_PyUnicode_TransformDecimalAndSpaceToASCII(PyObject *unicode)
{
    if (!PyUnicode_Check(unicode)) {
        PyErr_BadInternalCall();
        return NULL;
    }
    Py_ssize_t len = PyUnicode_GET_LENGTH(unicode);
    PyObject *result = PyUnicode_New(len);
    if (result == NULL) {
        return NULL;
    }

    Py_UCS1 *out = PyUnicode_1BYTE_DATA(result);

    const void *data = PyUnicode_DATA(unicode);
    Py_ssize_t i;
    for (i = 0; i < len; ++i) {
        Py_UCS4 ch = PyUnicode_READ(data, i);
        if (ch < 127) {
            out[i] = ch;
        }
        else if (Py_UNICODE_ISSPACE(ch)) {
            out[i] = ' ';
        }
        else {
            int decimal = Py_UNICODE_TODECIMAL(ch);
            if (decimal < 0) {
                out[i] = '?';
                out[i+1] = '\0';
                _PyUnicode_LENGTH(result) = i + 1;
                break;
            }
            out[i] = '0' + decimal;
        }
    }

    assert(_PyUnicode_CheckConsistency(result, 1));
    return result;
}

/* --- Decimal Encoder ---------------------------------------------------- */

/* --- Helpers ------------------------------------------------------------ */

/* helper macro to fixup start/end slice values */
#define ADJUST_INDICES(start, end, len)         \
    if (end > len)                              \
        end = len;                              \
    else if (end < 0) {                         \
        end += len;                             \
        if (end < 0)                            \
            end = 0;                            \
    }                                           \
    if (start < 0) {                            \
        start += len;                           \
        if (start < 0)                          \
            start = 0;                          \
    }

static Py_ssize_t
any_find_slice(PyObject* s1, PyObject* s2,
               Py_ssize_t start,
               Py_ssize_t end,
               int direction)
{
    const void *buf1, *buf2;
    Py_ssize_t len1, len2, result;

    len1 = PyUnicode_GET_LENGTH(s1);
    len2 = PyUnicode_GET_LENGTH(s2);
    ADJUST_INDICES(start, end, len1);
    if (end - start < len2)
        return -1;

    buf1 = PyUnicode_DATA(s1);
    buf2 = PyUnicode_DATA(s2);
    if (len2 == 1) {
        Py_UCS4 ch = PyUnicode_READ(buf2, 0);
        result = findchar((const char *)buf1 + start,
                          end - start, ch, direction);
        if (result == -1)
            return -1;
        else
            return start + result;
    }
    
    if (direction > 0) {
      result = ucs1lib_find_slice(buf1, len1, buf2, len2, start, end);
    }  else {
      result = ucs1lib_rfind_slice(buf1, len1, buf2, len2, start, end);
    }

    return result;
}

/* _PyUnicode_InsertThousandsGrouping() helper functions */
#include "stringlib/localeutil.h"

/**
 * InsertThousandsGrouping:
 * @writer: Unicode writer.
 * @n_buffer: Number of characters in @buffer.
 * @digits: Digits we're reading from. If count is non-NULL, this is unused.
 * @d_pos: Start of digits string.
 * @n_digits: The number of digits in the string, in which we want
 *            to put the grouping chars.
 * @min_width: The minimum width of the digits in the output string.
 *             Output will be zero-padded on the left to fill.
 * @grouping: see definition in localeconv().
 * @thousands_sep: see definition in localeconv().
 *
 * There are 2 modes: counting and filling. If @writer is NULL,
 *  we are in counting mode, else filling mode.
 * If counting, the required buffer size is returned.
 * If filling, we know the buffer will be large enough, so we don't
 *  need to pass in the buffer size.
 * Inserts thousand grouping characters (as defined by grouping and
 *  thousands_sep) into @writer.
 *
 * Return value: -1 on error, number of characters otherwise.
 **/
Py_ssize_t
_PyUnicode_InsertThousandsGrouping(
    _PyUnicodeWriter *writer,
    Py_ssize_t n_buffer,
    PyObject *digits,
    Py_ssize_t d_pos,
    Py_ssize_t n_digits,
    Py_ssize_t min_width,
    const char *grouping,
    PyObject *thousands_sep)
{
    min_width = Py_MAX(0, min_width);
    if (writer) {
        assert(digits != NULL);
    }
    else {
        assert(digits == NULL);
    }
    assert(0 <= d_pos);
    assert(0 <= n_digits);
    assert(grouping != NULL);

    Py_ssize_t count = 0;
    Py_ssize_t n_zeros;
    int loop_broken = 0;
    int use_separator = 0; /* First time through, don't append the
                              separator. They only go between
                              groups. */
    Py_ssize_t buffer_pos;
    Py_ssize_t digits_pos;
    Py_ssize_t len;
    Py_ssize_t n_chars;
    Py_ssize_t remaining = n_digits; /* Number of chars remaining to
                                        be looked at */
    /* A generator that returns all of the grouping widths, until it
       returns 0. */
    GroupGenerator groupgen;
    GroupGenerator_init(&groupgen, grouping);
    const Py_ssize_t thousands_sep_len = PyUnicode_GET_LENGTH(thousands_sep);

    /* if digits are not grouped, thousands separator
       should be an empty string */
    assert(!(grouping[0] == CHAR_MAX && thousands_sep_len != 0));

    digits_pos = d_pos + n_digits;
    if (writer) {
        buffer_pos = writer->pos + n_buffer;
        assert(buffer_pos <= PyUnicode_GET_LENGTH(writer->buffer));
        assert(digits_pos <= PyUnicode_GET_LENGTH(digits));
    }
    else {
        buffer_pos = n_buffer;
    }

    while ((len = GroupGenerator_next(&groupgen)) > 0) {
        len = Py_MIN(len, Py_MAX(Py_MAX(remaining, min_width), 1));
        n_zeros = Py_MAX(0, len - remaining);
        n_chars = Py_MAX(0, Py_MIN(remaining, len));

        /* Use n_zero zero's and n_chars chars */

        /* Count only, don't do anything. */
        count += (use_separator ? thousands_sep_len : 0) + n_zeros + n_chars;

        /* Copy into the writer. */
        InsertThousandsGrouping_fill(writer, &buffer_pos,
                                     digits, &digits_pos,
                                     n_chars, n_zeros,
                                     use_separator ? thousands_sep : NULL,
                                     thousands_sep_len);

        /* Use a separator next time. */
        use_separator = 1;

        remaining -= n_chars;
        min_width -= len;

        if (remaining <= 0 && min_width <= 0) {
            loop_broken = 1;
            break;
        }
        min_width -= thousands_sep_len;
    }
    if (!loop_broken) {
        /* We left the loop without using a break statement. */

        len = Py_MAX(Py_MAX(remaining, min_width), 1);
        n_zeros = Py_MAX(0, len - remaining);
        n_chars = Py_MAX(0, Py_MIN(remaining, len));

        /* Use n_zero zero's and n_chars chars */
        count += (use_separator ? thousands_sep_len : 0) + n_zeros + n_chars;

        /* Copy into the writer. */
        InsertThousandsGrouping_fill(writer, &buffer_pos,
                                     digits, &digits_pos,
                                     n_chars, n_zeros,
                                     use_separator ? thousands_sep : NULL,
                                     thousands_sep_len);
    }
    return count;
}


Py_ssize_t
PyUnicode_Count(PyObject *str,
                PyObject *substr,
                Py_ssize_t start,
                Py_ssize_t end)
{
    Py_ssize_t result;
    const void *buf1 = NULL, *buf2 = NULL;
    Py_ssize_t len1, len2;

    if (ensure_unicode(str) < 0 || ensure_unicode(substr) < 0)
        return -1;

    len1 = PyUnicode_GET_LENGTH(str);
    len2 = PyUnicode_GET_LENGTH(substr);
    ADJUST_INDICES(start, end, len1);
    if (end - start < len2)
        return 0;

    buf1 = PyUnicode_DATA(str);
    buf2 = PyUnicode_DATA(substr);
    result = ucs1lib_count(
                ((const Py_UCS1*)buf1) + start, end - start,
                buf2, len2, PY_SSIZE_T_MAX
                );
    return result;
}

Py_ssize_t
PyUnicode_Find(PyObject *str,
               PyObject *substr,
               Py_ssize_t start,
               Py_ssize_t end,
               int direction)
{
    if (ensure_unicode(str) < 0 || ensure_unicode(substr) < 0)
        return -2;

    return any_find_slice(str, substr, start, end, direction);
}

Py_ssize_t
PyUnicode_FindChar(PyObject *str, Py_UCS4 ch,
                   Py_ssize_t start, Py_ssize_t end,
                   int direction)
{
    Py_ssize_t len, result;
    len = PyUnicode_GET_LENGTH(str);
    ADJUST_INDICES(start, end, len);
    if (end - start < 1)
        return -1;
    result = findchar(PyUnicode_1BYTE_DATA(str) + start,
                      end-start, ch, direction);
    if (result == -1)
        return -1;
    else
        return start + result;
}

static int
tailmatch(PyObject *self,
          PyObject *substring,
          Py_ssize_t start,
          Py_ssize_t end,
          int direction)
{
    const void *data_self;
    const void *data_sub;
    Py_ssize_t offset;
    Py_ssize_t i;
    Py_ssize_t end_sub;


    ADJUST_INDICES(start, end, PyUnicode_GET_LENGTH(self));
    end -= PyUnicode_GET_LENGTH(substring);
    if (end < start)
        return 0;

    if (PyUnicode_GET_LENGTH(substring) == 0)
        return 1;

    data_self = PyUnicode_DATA(self);
    data_sub = PyUnicode_DATA(substring);
    end_sub = PyUnicode_GET_LENGTH(substring) - 1;

    if (direction > 0)
        offset = end;
    else
        offset = start;

    if (PyUnicode_READ(data_self, offset) ==
        PyUnicode_READ(data_sub, 0) &&
        PyUnicode_READ(data_self, offset + end_sub) ==
        PyUnicode_READ(data_sub, end_sub)) {
      return ! memcmp((char *)data_self +
		      (offset),
		      data_sub,
		      PyUnicode_GET_LENGTH(substring));
    } 
        /* otherwise we have to compare each character by first accessing it */
        else {
            /* We do not need to compare 0 and len(substring)-1 because
               the if statement above ensured already that they are equal
               when we end up here. */
            for (i = 1; i < end_sub; ++i) {
                if (PyUnicode_READ(data_self, offset + i) !=
                    PyUnicode_READ(data_sub, i))
                    return 0;
            }
            return 1;
        }
    return 0;
}

Py_ssize_t
PyUnicode_Tailmatch(PyObject *str,
                    PyObject *substr,
                    Py_ssize_t start,
                    Py_ssize_t end,
                    int direction)
{
    if (ensure_unicode(str) < 0 || ensure_unicode(substr) < 0)
        return -1;

    return tailmatch(str, substr, start, end, direction);
}

static int
lower_ucs4(const void *data, Py_ssize_t length, Py_ssize_t i,
           Py_UCS4 c, Py_UCS4 *mapped)
{
    return _PyUnicode_ToLowerFull(c, mapped);
}

static Py_ssize_t
do_capitalize(const void *data, Py_ssize_t length, Py_UCS4 *res)
{
    Py_ssize_t i, k = 0;
    int n_res, j;
    Py_UCS4 c, mapped[3];

    c = PyUnicode_READ(data, 0);
    n_res = _PyUnicode_ToTitleFull(c, mapped);
    for (j = 0; j < n_res; j++) {
        res[k++] = mapped[j];
    }
    for (i = 1; i < length; i++) {
        c = PyUnicode_READ(data, i);
        n_res = lower_ucs4(data, length, i, c, mapped);
        for (j = 0; j < n_res; j++) {
            res[k++] = mapped[j];
        }
    }
    return k;
}

static Py_ssize_t
do_swapcase(const void *data, Py_ssize_t length, Py_UCS4 *res) {
    Py_ssize_t i, k = 0;

    for (i = 0; i < length; i++) {
        Py_UCS4 c = PyUnicode_READ(data, i), mapped[3];
        int n_res, j;
        if (Py_UNICODE_ISUPPER(c)) {
            n_res = lower_ucs4(data, length, i, c, mapped);
        }
        else if (Py_UNICODE_ISLOWER(c)) {
            n_res = _PyUnicode_ToUpperFull(c, mapped);
        }
        else {
            n_res = 1;
            mapped[0] = c;
        }
        for (j = 0; j < n_res; j++) {
            res[k++] = mapped[j];
        }
    }
    return k;
}

static Py_ssize_t
do_upper_or_lower(const void *data, Py_ssize_t length, Py_UCS4 *res,
                  int lower)
{
    Py_ssize_t i, k = 0;

    for (i = 0; i < length; i++) {
        Py_UCS4 c = PyUnicode_READ(data, i), mapped[3];
        int n_res, j;
        if (lower)
            n_res = lower_ucs4(data, length, i, c, mapped);
        else
            n_res = _PyUnicode_ToUpperFull(c, mapped);
        for (j = 0; j < n_res; j++) {
            res[k++] = mapped[j];
        }
    }
    return k;
}

static Py_ssize_t
do_upper(const void *data, Py_ssize_t length, Py_UCS4 *res)
{
    return do_upper_or_lower(data, length, res, 0);
}

static Py_ssize_t
do_lower(const void *data, Py_ssize_t length, Py_UCS4 *res)
{
    return do_upper_or_lower(data, length, res, 1);
}

static Py_ssize_t
do_casefold(const void *data, Py_ssize_t length, Py_UCS4 *res)
{
    Py_ssize_t i, k = 0;

    for (i = 0; i < length; i++) {
        Py_UCS4 c = PyUnicode_READ(data, i);
        Py_UCS4 mapped[3];
        int j, n_res = _PyUnicode_ToFoldedFull(c, mapped);
        for (j = 0; j < n_res; j++) {
            res[k++] = mapped[j];
        }
    }
    return k;
}

static Py_ssize_t
do_title(const void *data, Py_ssize_t length, Py_UCS4 *res)
{
    Py_ssize_t i, k = 0;
    int previous_is_cased;

    previous_is_cased = 0;
    for (i = 0; i < length; i++) {
        const Py_UCS4 c = PyUnicode_READ(data, i);
        Py_UCS4 mapped[3];
        int n_res, j;

        if (previous_is_cased)
            n_res = lower_ucs4(data, length, i, c, mapped);
        else
            n_res = _PyUnicode_ToTitleFull(c, mapped);

        for (j = 0; j < n_res; j++) {
            res[k++] = mapped[j];
        }

        previous_is_cased = _PyUnicode_IsCased(c);
    }
    return k;
}

static PyObject *
case_operation(PyObject *self,
               Py_ssize_t (*perform)(const void *, Py_ssize_t, Py_UCS4 *))
{
    PyObject *res = NULL;
    Py_ssize_t length, newlength = 0;
    const void *data;
    void *outdata;
    Py_UCS4 *tmp, *tmpend;

    data = PyUnicode_DATA(self);
    length = PyUnicode_GET_LENGTH(self);
    if ((size_t) length > PY_SSIZE_T_MAX / (3 * sizeof(Py_UCS4))) {
        PyErr_SetString(PyExc_OverflowError, "string is too long");
        return NULL;
    }
    tmp = PyMem_Malloc(sizeof(Py_UCS4) * 3 * length);
    if (tmp == NULL)
        return PyErr_NoMemory();
    newlength = perform(data, length, tmp);
    res = PyUnicode_New(newlength);
    if (res == NULL)
        goto leave;
    tmpend = tmp + newlength;
    outdata = PyUnicode_DATA(res);
    _PyUnicode_CONVERT_BYTES(Py_UCS4, Py_UCS1, tmp, tmpend, outdata);
  leave:
    PyMem_Free(tmp);
    return res;
}

PyObject *
PyUnicode_Join(PyObject *separator, PyObject *seq)
{
    PyObject *res;
    PyObject *fseq;
    Py_ssize_t seqlen;
    PyObject **items;

    fseq = PySequence_Fast(seq, "can only join an iterable");
    if (fseq == NULL) {
        return NULL;
    }

    /* NOTE: the following code can't call back into Python code,
     * so we are sure that fseq won't be mutated.
     */

    items = PySequence_Fast_ITEMS(fseq);
    seqlen = PySequence_Size(fseq);
    res = _PyUnicode_JoinArray(separator, items, seqlen);
    Py_DECREF(fseq);
    return res;
}

PyObject *
_PyUnicode_JoinArray(PyObject *separator, PyObject *const *items, Py_ssize_t seqlen)
{
    PyObject *res = NULL; /* the result */
    PyObject *sep = NULL;
    Py_ssize_t seplen;
    PyObject *item;
    Py_ssize_t sz, i, res_offset;
    int use_memcpy;
    unsigned char *res_data = NULL, *sep_data = NULL;
    PyObject *last_obj;

    /* If empty sequence, return u"". */
    if (seqlen == 0) {
        _Py_RETURN_UNICODE_EMPTY();
    }

    /* If singleton sequence with an exact Unicode, return that. */
    last_obj = NULL;
    if (seqlen == 1) {
        if (PyUnicode_CheckExact(items[0])) {
            res = items[0];
            Py_INCREF(res);
            return res;
        }
        seplen = 0;
    }
    else {
        /* Set up sep and seplen */
        if (separator == NULL) {
            /* fall back to a blank space separator */
            sep = PyUnicode_FromOrdinal(' ');
            if (!sep)
                goto onError;
            seplen = 1;
        }
        else {
            if (!PyUnicode_Check(separator)) {
                PyErr_Format(PyExc_TypeError,
                             "separator: expected str instance,"
                             " %.80s found",
                             Py_TYPE(separator)->tp_name);
                goto onError;
            }
            sep = separator;
            seplen = PyUnicode_GET_LENGTH(separator);
            /* inc refcount to keep this code path symmetric with the
               above case of a blank separator */
            Py_INCREF(sep);
        }
        last_obj = sep;
    }

    /* There are at least two things to join, or else we have a subclass
     * of str in the sequence.
     * Do a pre-pass to figure out the total amount of space we'll
     * need (sz), and see whether all argument are strings.
     */
    sz = 0;
    use_memcpy = 1;
    for (i = 0; i < seqlen; i++) {
        size_t add_sz;
        item = items[i];
        if (!PyUnicode_Check(item)) {
            PyErr_Format(PyExc_TypeError,
                         "sequence item %zd: expected str instance,"
                         " %.80s found",
                         i, Py_TYPE(item)->tp_name);
            goto onError;
        }
        add_sz = PyUnicode_GET_LENGTH(item);
        if (i != 0) {
            add_sz += seplen;
        }
        if (add_sz > (size_t)(PY_SSIZE_T_MAX - sz)) {
            PyErr_SetString(PyExc_OverflowError,
                            "join() result is too long for a Python string");
            goto onError;
        }
        sz += add_sz;
        last_obj = item;
    }

    res = PyUnicode_New(sz);
    if (res == NULL)
        goto onError;

    /* Catenate everything. */
    if (use_memcpy) {
        res_data = PyUnicode_1BYTE_DATA(res);
        if (seplen != 0)
            sep_data = PyUnicode_1BYTE_DATA(sep);
    }
    if (use_memcpy) {
        for (i = 0; i < seqlen; ++i) {
            Py_ssize_t itemlen;
            item = items[i];

            /* Copy item, and maybe the separator. */
            if (i && seplen != 0) {
                memcpy(res_data,
                          sep_data,
                          seplen);
                res_data += seplen;
            }

            itemlen = PyUnicode_GET_LENGTH(item);
            if (itemlen != 0) {
                memcpy(res_data,
                          PyUnicode_DATA(item),
                          itemlen);
                res_data += itemlen;
            }
        }
        assert(res_data == PyUnicode_1BYTE_DATA(res)
                           + PyUnicode_GET_LENGTH(res));
    }
    else {
        for (i = 0, res_offset = 0; i < seqlen; ++i) {
            Py_ssize_t itemlen;
            item = items[i];

            /* Copy item, and maybe the separator. */
            if (i && seplen != 0) {
                _PyUnicode_FastCopyCharacters(res, res_offset, sep, 0, seplen);
                res_offset += seplen;
            }

            itemlen = PyUnicode_GET_LENGTH(item);
            if (itemlen != 0) {
                _PyUnicode_FastCopyCharacters(res, res_offset, item, 0, itemlen);
                res_offset += itemlen;
            }
        }
        assert(res_offset == PyUnicode_GET_LENGTH(res));
    }

    Py_XDECREF(sep);
    assert(_PyUnicode_CheckConsistency(res, 1));
    return res;

  onError:
    Py_XDECREF(sep);
    Py_XDECREF(res);
    return NULL;
}

void
_PyUnicode_FastFill(PyObject *unicode, Py_ssize_t start, Py_ssize_t length,
                    unsigned char fill_char)
{
    void *data = PyUnicode_DATA(unicode);
    assert(unicode_modifiable(unicode));
    assert(fill_char <= PyUnicode_MAX_CHAR_VALUE(unicode));
    assert(start >= 0);
    assert(start + length <= PyUnicode_GET_LENGTH(unicode));
    unicode_fill(data, fill_char, start, length);
}

Py_ssize_t
PyUnicode_Fill(PyObject *unicode, Py_ssize_t start, Py_ssize_t length,
               unsigned char fill_char)
{
    Py_ssize_t maxlen;

    if (!PyUnicode_Check(unicode)) {
        PyErr_BadInternalCall();
        return -1;
    }
    if (unicode_check_modifiable(unicode))
        return -1;

    if (start < 0) {
        PyErr_SetString(PyExc_IndexError, "string index out of range");
        return -1;
    }
    if (fill_char > PyUnicode_MAX_CHAR_VALUE(unicode)) {
        PyErr_SetString(PyExc_ValueError,
                         "fill character is bigger than "
                         "the string maximum character");
        return -1;
    }

    maxlen = PyUnicode_GET_LENGTH(unicode) - start;
    length = Py_MIN(maxlen, length);
    if (length <= 0)
        return 0;

    _PyUnicode_FastFill(unicode, start, length, fill_char);
    return length;
}

static PyObject *
pad(PyObject *self,
    Py_ssize_t left,
    Py_ssize_t right,
    unsigned char fill)
{
    PyObject *u;
    void *data;

    if (left < 0)
        left = 0;
    if (right < 0)
        right = 0;

    if (left == 0 && right == 0)
        return unicode_result_unchanged(self);

    if (left > PY_SSIZE_T_MAX - _PyUnicode_LENGTH(self) ||
        right > PY_SSIZE_T_MAX - (left + _PyUnicode_LENGTH(self))) {
        PyErr_SetString(PyExc_OverflowError, "padded string is too long");
        return NULL;
    }
    u = PyUnicode_New(left + _PyUnicode_LENGTH(self) + right);
    if (!u)
        return NULL;

    data = PyUnicode_DATA(u);
    if (left)
        unicode_fill(data, fill, 0, left);
    if (right)
        unicode_fill(data, fill, left + _PyUnicode_LENGTH(self), right);
    _PyUnicode_FastCopyCharacters(u, left, self, 0, _PyUnicode_LENGTH(self));
    assert(_PyUnicode_CheckConsistency(u, 1));
    return u;
}

PyObject *
PyUnicode_Splitlines(PyObject *string, int keepends)
{
    PyObject *list;

    if (ensure_unicode(string) < 0)
        return NULL;
    list = ucs1lib_splitlines(
			      string, PyUnicode_1BYTE_DATA(string),
			      PyUnicode_GET_LENGTH(string), keepends);
    return list;
}

static PyObject *
split(PyObject *self,
      PyObject *substring,
      Py_ssize_t maxcount)
{
    const void *buf1, *buf2;
    Py_ssize_t len1, len2;
    PyObject* out;

    if (maxcount < 0)
        maxcount = PY_SSIZE_T_MAX;

    if (substring == NULL)
                return ucs1lib_split_whitespace(
                    self,  PyUnicode_1BYTE_DATA(self),
                    PyUnicode_GET_LENGTH(self), maxcount
                    );

    len1 = PyUnicode_GET_LENGTH(self);
    len2 = PyUnicode_GET_LENGTH(substring);
    if (len1 < len2) {
        out = PyList_New(1);
        if (out == NULL)
            return NULL;
        Py_INCREF(self);
        PyList_SET_ITEM(out, 0, self);
        return out;
    }
    buf1 = PyUnicode_DATA(self);
    buf2 = PyUnicode_DATA(substring);
    
    out = ucs1lib_split(
			self,  buf1, len1, buf2, len2, maxcount);
    return out;
}

static PyObject *
rsplit(PyObject *self,
       PyObject *substring,
       Py_ssize_t maxcount)
{
    const void *buf1, *buf2;
    Py_ssize_t len1, len2;
    PyObject* out;

    if (maxcount < 0)
        maxcount = PY_SSIZE_T_MAX;


    if (substring == NULL)
      return ucs1lib_rsplit_whitespace(
                    self,  PyUnicode_1BYTE_DATA(self),
                    PyUnicode_GET_LENGTH(self), maxcount
                    );

    len1 = PyUnicode_GET_LENGTH(self);
    len2 = PyUnicode_GET_LENGTH(substring);
    if (len1 < len2) {
        out = PyList_New(1);
        if (out == NULL)
            return NULL;
        Py_INCREF(self);
        PyList_SET_ITEM(out, 0, self);
        return out;
    }
    buf1 = PyUnicode_DATA(self);
    buf2 = PyUnicode_DATA(substring);
    
    out = ucs1lib_rsplit(
			 self,  buf1, len1, buf2, len2, maxcount);
    return out;
}

static Py_ssize_t
anylib_find(PyObject *str1, const void *buf1, Py_ssize_t len1,
            PyObject *str2, const void *buf2, Py_ssize_t len2, Py_ssize_t offset)
{
  return ucs1lib_find(buf1, len1, buf2, len2, offset);
}

static Py_ssize_t
anylib_count(PyObject *sstr, const void* sbuf, Py_ssize_t slen,
             PyObject *str1, const void *buf1, Py_ssize_t len1, Py_ssize_t maxcount)
{
  return ucs1lib_count(sbuf, slen, buf1, len1, maxcount);
}

static void
replace_1char_inplace(PyObject *u, Py_ssize_t pos,
                      Py_UCS4 u1, Py_UCS4 u2, Py_ssize_t maxcount)
{

    void *data = PyUnicode_DATA(u);
    Py_ssize_t len = PyUnicode_GET_LENGTH(u);
    ucs1lib_replace_1char_inplace((Py_UCS1 *)data + pos,
				  (Py_UCS1 *)data + len,
				  u1, u2, maxcount);
}

static PyObject *
replace(PyObject *self, PyObject *str1,
        PyObject *str2, Py_ssize_t maxcount)
{
    PyObject *u;
    const char *sbuf = PyUnicode_DATA(self);
    const void *buf1 = PyUnicode_DATA(str1);
    const void *buf2 = PyUnicode_DATA(str2);
    int srelease = 0, release1 = 0, release2 = 0;

    Py_ssize_t slen = PyUnicode_GET_LENGTH(self);
    Py_ssize_t len1 = PyUnicode_GET_LENGTH(str1);
    Py_ssize_t len2 = PyUnicode_GET_LENGTH(str2);

    if (slen < len1)
        goto nothing;

    if (maxcount < 0)
        maxcount = PY_SSIZE_T_MAX;
    else if (maxcount == 0)
        goto nothing;

    if (str1 == str2)
        goto nothing;

    if (len1 == len2) {
        /* same length */
        if (len1 == 0)
            goto nothing;
        if (len1 == 1) {
            /* replace characters */
            Py_UCS4 u1, u2;
            Py_ssize_t pos;

            u1 = PyUnicode_READ(buf1, 0);
            pos = findchar(sbuf, slen, u1, 1);
            if (pos < 0)
                goto nothing;
            u2 = PyUnicode_READ(buf2, 0);
            u = PyUnicode_New(slen);
            if (!u)
                goto error;

            _PyUnicode_FastCopyCharacters(u, 0, self, 0, slen);
            replace_1char_inplace(u, pos, u1, u2, maxcount);
        }
        else {
            char *res;
            Py_ssize_t i;
            i = anylib_find(self, sbuf, slen, str1, buf1, len1, 0);
            if (i < 0)
                goto nothing;
            u = PyUnicode_New(slen);
            if (!u)
                goto error;
            res = PyUnicode_DATA(u);

            memcpy(res, sbuf, slen);
            /* change everything in-place, starting with this one */
            memcpy(res + i,
                   buf2,
                   len2);
            i += len1;

            while ( --maxcount > 0) {
                i = anylib_find(self,
                                sbuf+i, slen-i,
                                str1, buf1, len1, i);
                if (i == -1)
                    break;
                memcpy(res + i,
                       buf2,
                       len2);
                i += len1;
            }
        }
    }
    else {
        Py_ssize_t n, i, j, ires;
        Py_ssize_t new_size;
        char *res;
        n = anylib_count(self, sbuf, slen, str1, buf1, len1, maxcount);
        if (n == 0)
            goto nothing;
        /* new_size = PyUnicode_GET_LENGTH(self) + n * (PyUnicode_GET_LENGTH(str2) -
           PyUnicode_GET_LENGTH(str1))); */
        if (len1 < len2 && len2 - len1 > (PY_SSIZE_T_MAX - slen) / n) {
                PyErr_SetString(PyExc_OverflowError,
                                "replace string is too long");
                goto error;
        }
        new_size = slen + n * (len2 - len1);
        if (new_size == 0) {
            _Py_INCREF_UNICODE_EMPTY();
            if (!unicode_empty)
                goto error;
            u = unicode_empty;
            goto done;
        }
        if (new_size > (PY_SSIZE_T_MAX)) {
            PyErr_SetString(PyExc_OverflowError,
                            "replace string is too long");
            goto error;
        }
        u = PyUnicode_New(new_size);
        if (!u)
            goto error;
        res = PyUnicode_DATA(u);
        ires = i = 0;
        if (len1 > 0) {
            while (n-- > 0) {
                /* look for next match */
                j = anylib_find(self,
                                sbuf +  i, slen-i,
                                str1, buf1, len1, i);
                if (j == -1)
                    break;
                else if (j > i) {
                    /* copy unchanged part [i:j] */
                    memcpy(res + ires,
                           sbuf +  i,
                           (j-i));
                    ires += j - i;
                }
                /* copy substitution string */
                if (len2 > 0) {
                    memcpy(res + ires,
                           buf2,
                           len2);
                    ires += len2;
                }
                i = j + len1;
            }
            if (i < slen)
                /* copy tail [i:] */
                memcpy(res + ires,
                       sbuf + i,
                       (slen-i));
        }
        else {
            /* interleave */
            while (n > 0) {
                memcpy(res + ires,
                       buf2,
                       len2);
                ires += len2;
                if (--n <= 0)
                    break;
                memcpy(res + ires,
                       sbuf + i,
                       1);
                ires++;
                i++;
            }
            memcpy(res + ires,
                   sbuf + i,
                   (slen-i));
        }
    }
    
  done:
    assert(srelease == (sbuf != PyUnicode_DATA(self)));
    assert(release1 == (buf1 != PyUnicode_DATA(str1)));
    assert(release2 == (buf2 != PyUnicode_DATA(str2)));
    if (srelease)
        PyMem_Free((void *)sbuf);
    if (release1)
        PyMem_Free((void *)buf1);
    if (release2)
        PyMem_Free((void *)buf2);
    assert(_PyUnicode_CheckConsistency(u, 1));
    return u;

  nothing:
    /* nothing to replace; return original string (when possible) */
    assert(srelease == (sbuf != PyUnicode_DATA(self)));
    assert(release1 == (buf1 != PyUnicode_DATA(str1)));
    assert(release2 == (buf2 != PyUnicode_DATA(str2)));
    if (srelease)
        PyMem_Free((void *)sbuf);
    if (release1)
        PyMem_Free((void *)buf1);
    if (release2)
        PyMem_Free((void *)buf2);
    return unicode_result_unchanged(self);

  error:
    assert(srelease == (sbuf != PyUnicode_DATA(self)));
    assert(release1 == (buf1 != PyUnicode_DATA(str1)));
    assert(release2 == (buf2 != PyUnicode_DATA(str2)));
    if (srelease)
        PyMem_Free((void *)sbuf);
    if (release1)
        PyMem_Free((void *)buf1);
    if (release2)
        PyMem_Free((void *)buf2);
    return NULL;
}

/* --- Unicode Object Methods --------------------------------------------- */

/*[clinic input]
str.title as unicode_title

Return a version of the string where each word is titlecased.

More specifically, words start with uppercased characters and all remaining
cased characters have lower case.
[clinic start generated code]*/

static PyObject *
unicode_title_impl(PyObject *self)
/*[clinic end generated code: output=c75ae03809574902 input=fa945d669b26e683]*/
{
    return case_operation(self, do_title);
}

/*[clinic input]
str.capitalize as unicode_capitalize

Return a capitalized version of the string.

More specifically, make the first character have upper case and the rest lower
case.
[clinic start generated code]*/

static PyObject *
unicode_capitalize_impl(PyObject *self)
/*[clinic end generated code: output=e49a4c333cdb7667 input=f4cbf1016938da6d]*/
{
    if (PyUnicode_GET_LENGTH(self) == 0)
        return unicode_result_unchanged(self);
    return case_operation(self, do_capitalize);
}

/*[clinic input]
str.casefold as unicode_casefold

Return a version of the string suitable for caseless comparisons.
[clinic start generated code]*/

static PyObject *
unicode_casefold_impl(PyObject *self)
/*[clinic end generated code: output=0120daf657ca40af input=384d66cc2ae30daf]*/
{
    return case_operation(self, do_casefold);
}


/* Argument converter. Accepts a single Unicode character. */

static int
convert_uc(PyObject *obj, void *addr)
{
    Py_UCS4 *fillcharloc = (Py_UCS4 *)addr;

    if (!PyUnicode_Check(obj)) {
        PyErr_Format(PyExc_TypeError,
                     "The fill character must be a unicode character, "
                     "not %.100s", Py_TYPE(obj)->tp_name);
        return 0;
    }
    if (PyUnicode_GET_LENGTH(obj) != 1) {
        PyErr_SetString(PyExc_TypeError,
                        "The fill character must be exactly one character long");
        return 0;
    }
    *fillcharloc = PyUnicode_READ_CHAR(obj, 0);
    return 1;
}

/*[clinic input]
str.center as unicode_center

    width: Py_ssize_t
    fillchar: Py_UCS4 = ' '
    /

Return a centered string of length width.

Padding is done using the specified fill character (default is a space).
[clinic start generated code]*/

static PyObject *
unicode_center_impl(PyObject *self, Py_ssize_t width, Py_UCS4 fillchar)
/*[clinic end generated code: output=420c8859effc7c0c input=b42b247eb26e6519]*/
{
    Py_ssize_t marg, left;

    if (PyUnicode_GET_LENGTH(self) >= width)
        return unicode_result_unchanged(self);

    marg = width - PyUnicode_GET_LENGTH(self);
    left = marg / 2 + (marg & width & 1);

    return pad(self, left, marg - left, fillchar);
}

/* This function assumes that str1 and str2 are readied by the caller. */

static int
unicode_compare(PyObject *str1, PyObject *str2)
{
#define COMPARE(TYPE1, TYPE2) \
    do { \
        TYPE1* p1 = (TYPE1 *)data1; \
        TYPE2* p2 = (TYPE2 *)data2; \
        TYPE1* end = p1 + len; \
        Py_UCS4 c1, c2; \
        for (; p1 != end; p1++, p2++) { \
            c1 = *p1; \
            c2 = *p2; \
            if (c1 != c2) \
                return (c1 < c2) ? -1 : 1; \
        } \
    } \
    while (0)

    const void *data1, *data2;
    Py_ssize_t len1, len2, len;

    data1 = PyUnicode_DATA(str1);
    data2 = PyUnicode_DATA(str2);
    len1 = PyUnicode_GET_LENGTH(str1);
    len2 = PyUnicode_GET_LENGTH(str2);
    len = Py_MIN(len1, len2);
    {
      int cmp = memcmp(data1, data2, len);
      if (cmp < 0)
	return -1;
      if (cmp > 0)
	return 1;
    }
    if (len1 == len2)
        return 0;
    if (len1 < len2)
        return -1;
    else
        return 1;

#undef COMPARE
}

static int
unicode_compare_eq(PyObject *str1, PyObject *str2)
{
    const void *data1, *data2;
    Py_ssize_t len;
    int cmp;

    len = PyUnicode_GET_LENGTH(str1);
    if (PyUnicode_GET_LENGTH(str2) != len)
        return 0;
    data1 = PyUnicode_DATA(str1);
    data2 = PyUnicode_DATA(str2);

    cmp = memcmp(data1, data2, len);
    return (cmp == 0);
}


int
PyUnicode_Compare(PyObject *left, PyObject *right)
{
    if (PyUnicode_Check(left) && PyUnicode_Check(right)) {

        /* a string is equal to itself */
        if (left == right)
            return 0;

        return unicode_compare(left, right);
    }
    PyErr_Format(PyExc_TypeError,
                 "Can't compare %.100s and %.100s",
                 Py_TYPE(left)->tp_name,
                 Py_TYPE(right)->tp_name);
    return -1;
}

int
PyUnicode_CompareWithASCIIString(PyObject* uni, const char* str)
{
    assert(_PyUnicode_CHECK(uni));
    {
        const void *data = PyUnicode_1BYTE_DATA(uni);
        size_t len1 = (size_t)PyUnicode_GET_LENGTH(uni);
        size_t len, len2 = strlen(str);
        int cmp;

        len = Py_MIN(len1, len2);
        cmp = memcmp(data, str, len);
        if (cmp != 0) {
            if (cmp < 0)
                return -1;
            else
                return 1;
        }
        if (len1 > len2)
            return 1; /* uni is longer */
        if (len1 < len2)
            return -1; /* str is longer */
        return 0;
    }
}

int
_PyUnicode_EqualToASCIIString(PyObject *unicode, const char *str)
{
    size_t len;
    assert(_PyUnicode_CHECK(unicode));
    assert(str);
    len = (size_t)PyUnicode_GET_LENGTH(unicode);
    return strlen(str) == len &&
           memcmp(PyUnicode_1BYTE_DATA(unicode), str, len) == 0;
}

int
_PyUnicode_EqualToASCIIId(PyObject *left, _Py_Identifier *right)
{
    PyObject *right_uni;

    assert(_PyUnicode_CHECK(left));
    assert(right->string);
#ifndef NDEBUG
    for (const char *p = right->string; *p; p++) {
        assert((unsigned char)*p < 128);
    }
#endif
    right_uni = _PyUnicode_FromId(right);       /* borrowed */
    if (right_uni == NULL) {
        /* memory error or bad data */
        PyErr_Clear();
        return _PyUnicode_EqualToASCIIString(left, right->string);
    }

    if (left == right_uni)
        return 1;

    if (PyUnicode_CHECK_INTERNED(left))
        return 0;

#ifdef INTERNED_STRINGS
    assert(_PyUnicode_HASH(right_uni) != -1);
    Py_hash_t hash = _PyUnicode_HASH(left);
    if (hash != -1 && hash != _PyUnicode_HASH(right_uni))
        return 0;
#endif

    return unicode_compare_eq(left, right_uni);
}

PyObject *
PyUnicode_RichCompare(PyObject *left, PyObject *right, int op)
{
    int result;

    if (!PyUnicode_Check(left) || !PyUnicode_Check(right))
        Py_RETURN_NOTIMPLEMENTED;

    if (left == right) {
        switch (op) {
        case Py_EQ:
        case Py_LE:
        case Py_GE:
            /* a string is equal to itself */
            Py_RETURN_TRUE;
        case Py_NE:
        case Py_LT:
        case Py_GT:
            Py_RETURN_FALSE;
        default:
            PyErr_BadArgument();
            return NULL;
        }
    }
    else if (op == Py_EQ || op == Py_NE) {
        result = unicode_compare_eq(left, right);
        result ^= (op == Py_NE);
        return PyBool_FromLong(result);
    }
    else {
        result = unicode_compare(left, right);
        Py_RETURN_RICHCOMPARE(result, 0, op);
    }
}

int
_PyUnicode_EQ(PyObject *aa, PyObject *bb)
{
    return unicode_eq(aa, bb);
}

int
PyUnicode_Contains(PyObject *str, PyObject *substr)
{
    const void *buf1, *buf2;
    Py_ssize_t len1, len2;
    int result;

    if (!PyUnicode_Check(substr)) {
        PyErr_Format(PyExc_TypeError,
                     "'in <string>' requires string as left operand, not %.100s",
                     Py_TYPE(substr)->tp_name);
        return -1;
    }
    if (ensure_unicode(str) < 0)
        return -1;

    len1 = PyUnicode_GET_LENGTH(str);
    len2 = PyUnicode_GET_LENGTH(substr);
    if (len1 < len2)
        return 0;
    buf1 = PyUnicode_DATA(str);
    buf2 = PyUnicode_DATA(substr);
    if (len2 == 1) {
        Py_UCS4 ch = PyUnicode_READ(buf2, 0);
        result = findchar((const char *)buf1, len1, ch, 1) != -1;
        return result;
    }
    result = ucs1lib_find(buf1, len1, buf2, len2, 0) != -1;
    return result;
}

/* Concat to string or Unicode object giving a new Unicode object. */

PyObject *
PyUnicode_Concat(PyObject *left, PyObject *right)
{
    PyObject *result;
    Py_ssize_t left_len, right_len, new_len;

    if (ensure_unicode(left) < 0)
        return NULL;

    if (!PyUnicode_Check(right)) {
        PyErr_Format(PyExc_TypeError,
                     "can only concatenate str (not \"%.200s\") to str",
                     Py_TYPE(right)->tp_name);
        return NULL;
    }

    /* Shortcuts */
    if (left == unicode_empty)
        return PyUnicode_FromObject(right);
    if (right == unicode_empty)
        return PyUnicode_FromObject(left);

    left_len = PyUnicode_GET_LENGTH(left);
    right_len = PyUnicode_GET_LENGTH(right);
    if (left_len > PY_SSIZE_T_MAX - right_len) {
        PyErr_SetString(PyExc_OverflowError,
                        "strings are too large to concat");
        return NULL;
    }
    new_len = left_len + right_len;

    /* Concat the two Unicode strings */
    result = PyUnicode_New(new_len);
    if (result == NULL)
        return NULL;
    _PyUnicode_FastCopyCharacters(result, 0, left, 0, left_len);
    _PyUnicode_FastCopyCharacters(result, left_len, right, 0, right_len);
    assert(_PyUnicode_CheckConsistency(result, 1));
    return result;
}

void
PyUnicode_Append(PyObject **p_left, PyObject *right)
{
    PyObject *left, *res;
    Py_ssize_t left_len, right_len, new_len;

    if (p_left == NULL) {
        if (!PyErr_Occurred())
            PyErr_BadInternalCall();
        return;
    }
    left = *p_left;
    if (right == NULL || left == NULL
        || !PyUnicode_Check(left) || !PyUnicode_Check(right)) {
        if (!PyErr_Occurred())
            PyErr_BadInternalCall();
        goto error;
    }

    /* Shortcuts */
    if (left == unicode_empty) {
        Py_DECREF(left);
        Py_INCREF(right);
        *p_left = right;
        return;
    }
    if (right == unicode_empty)
        return;

    left_len = PyUnicode_GET_LENGTH(left);
    right_len = PyUnicode_GET_LENGTH(right);
    if (left_len > PY_SSIZE_T_MAX - right_len) {
        PyErr_SetString(PyExc_OverflowError,
                        "strings are too large to concat");
        goto error;
    }
    new_len = left_len + right_len;

    if (unicode_modifiable(left)
        && PyUnicode_CheckExact(right)
        /* Don't resize for ascii += latin1. Convert ascii to latin1 requires
           to change the structure size, but characters are stored just after
           the structure, and so it requires to move all characters which is
           not so different than duplicating the string. */
	)
    {
        /* append inplace */
        if (unicode_resize(p_left, new_len) != 0)
            goto error;

        /* copy 'right' into the newly allocated area of 'left' */
        _PyUnicode_FastCopyCharacters(*p_left, left_len, right, 0, right_len);
    }
    else {
        /* Concat the two Unicode strings */
        res = PyUnicode_New(new_len);
        if (res == NULL)
            goto error;
        _PyUnicode_FastCopyCharacters(res, 0, left, 0, left_len);
        _PyUnicode_FastCopyCharacters(res, left_len, right, 0, right_len);
        Py_DECREF(left);
        *p_left = res;
    }
    assert(_PyUnicode_CheckConsistency(*p_left, 1));
    return;

error:
    Py_CLEAR(*p_left);
}

void
PyUnicode_AppendAndDel(PyObject **pleft, PyObject *right)
{
    PyUnicode_Append(pleft, right);
    Py_XDECREF(right);
}

/*
Wraps stringlib_parse_args_finds() and additionally ensures that the
first argument is a unicode object.
*/

static inline int
parse_args_finds_unicode(const char * function_name, PyObject *args,
                         PyObject **substring,
                         Py_ssize_t *start, Py_ssize_t *end)
{
    if(ucs1lib_parse_args_finds(function_name, args, substring,
                                  start, end)) {
        if (ensure_unicode(*substring) < 0)
            return 0;
        return 1;
    }
    return 0;
}

PyDoc_STRVAR(count__doc__,
             "S.count(sub[, start[, end]]) -> int\n\
\n\
Return the number of non-overlapping occurrences of substring sub in\n\
string S[start:end].  Optional arguments start and end are\n\
interpreted as in slice notation.");

static PyObject *
unicode_count(PyObject *self, PyObject *args)
{
    PyObject *substring = NULL;   /* initialize to fix a compiler warning */
    Py_ssize_t start = 0;
    Py_ssize_t end = PY_SSIZE_T_MAX;
    PyObject *result;
    const void *buf1, *buf2;
    Py_ssize_t len1, len2, iresult;

    if (!parse_args_finds_unicode("count", args, &substring, &start, &end))
        return NULL;

    len1 = PyUnicode_GET_LENGTH(self);
    len2 = PyUnicode_GET_LENGTH(substring);
    ADJUST_INDICES(start, end, len1);
    if (end - start < len2)
        return PyLong_FromLong(0);

    buf1 = PyUnicode_DATA(self);
    buf2 = PyUnicode_DATA(substring);
    iresult = ucs1lib_count(
			    ((const Py_UCS1*)buf1) + start, end - start,
			    buf2, len2, PY_SSIZE_T_MAX
			    );

    result = PyLong_FromSsize_t(iresult);
    return result;
}

/*[clinic input]
str.expandtabs as unicode_expandtabs

    tabsize: int = 8

Return a copy where all tab characters are expanded using spaces.

If tabsize is not given, a tab size of 8 characters is assumed.
[clinic start generated code]*/

static PyObject *
unicode_expandtabs_impl(PyObject *self, int tabsize)
/*[clinic end generated code: output=3457c5dcee26928f input=8a01914034af4c85]*/
{
    Py_ssize_t i, j, line_pos, src_len, incr;
    Py_UCS4 ch;
    PyObject *u;
    const void *src_data;
    void *dest_data;
    int found;


    /* First pass: determine size of output string */
    src_len = PyUnicode_GET_LENGTH(self);
    i = j = line_pos = 0;
    src_data = PyUnicode_DATA(self);
    found = 0;
    for (; i < src_len; i++) {
        ch = PyUnicode_READ(src_data, i);
        if (ch == '\t') {
            found = 1;
            if (tabsize > 0) {
                incr = tabsize - (line_pos % tabsize); /* cannot overflow */
                if (j > PY_SSIZE_T_MAX - incr)
                    goto overflow;
                line_pos += incr;
                j += incr;
            }
        }
        else {
            if (j > PY_SSIZE_T_MAX - 1)
                goto overflow;
            line_pos++;
            j++;
            if (ch == '\n' || ch == '\r')
                line_pos = 0;
        }
    }
    if (!found)
        return unicode_result_unchanged(self);

    /* Second pass: create output string and fill it */
    u = PyUnicode_New(j);
    if (!u)
        return NULL;
    dest_data = PyUnicode_DATA(u);

    i = j = line_pos = 0;

    for (; i < src_len; i++) {
        ch = PyUnicode_READ(src_data, i);
        if (ch == '\t') {
            if (tabsize > 0) {
                incr = tabsize - (line_pos % tabsize);
                line_pos += incr;
                unicode_fill(dest_data, ' ', j, incr);
                j += incr;
            }
        }
        else {
            line_pos++;
            PyUnicode_WRITE(dest_data, j, ch);
            j++;
            if (ch == '\n' || ch == '\r')
                line_pos = 0;
        }
    }
    assert (j == PyUnicode_GET_LENGTH(u));
    return unicode_result(u);

  overflow:
    PyErr_SetString(PyExc_OverflowError, "new string is too long");
    return NULL;
}

PyDoc_STRVAR(find__doc__,
             "S.find(sub[, start[, end]]) -> int\n\
\n\
Return the lowest index in S where substring sub is found,\n\
such that sub is contained within S[start:end].  Optional\n\
arguments start and end are interpreted as in slice notation.\n\
\n\
Return -1 on failure.");

static PyObject *
unicode_find(PyObject *self, PyObject *args)
{
    /* initialize variables to prevent gcc warning */
    PyObject *substring = NULL;
    Py_ssize_t start = 0;
    Py_ssize_t end = 0;
    Py_ssize_t result;

    if (!parse_args_finds_unicode("find", args, &substring, &start, &end))
        return NULL;

    result = any_find_slice(self, substring, start, end, 1);

    if (result == -2)
        return NULL;

    return PyLong_FromSsize_t(result);
}

static PyObject *
unicode_getitem(PyObject *self, Py_ssize_t index)
{
    const void *data;
    Py_UCS4 ch;

    if (!PyUnicode_Check(self)) {
        PyErr_BadArgument();
        return NULL;
    }
    if (index < 0 || index >= PyUnicode_GET_LENGTH(self)) {
        PyErr_SetString(PyExc_IndexError, "string index out of range");
        return NULL;
    }
    data = PyUnicode_DATA(self);
    ch = PyUnicode_READ(data, index);
    return unicode_char(ch);
}

/* Believe it or not, this produces the same value for ASCII strings
   as bytes_hash(). */
static Py_hash_t
unicode_hash(PyObject *self)
{
    Py_uhash_t x;  /* Unsigned for defined overflow behavior. */

    if (_PyUnicode_HASH(self) != -1)
        return _PyUnicode_HASH(self);
    x = _Py_HashBytes(PyUnicode_DATA(self),
                      PyUnicode_GET_LENGTH(self));
    _PyUnicode_HASH(self) = x;
    return x;
}

PyDoc_STRVAR(index__doc__,
             "S.index(sub[, start[, end]]) -> int\n\
\n\
Return the lowest index in S where substring sub is found,\n\
such that sub is contained within S[start:end].  Optional\n\
arguments start and end are interpreted as in slice notation.\n\
\n\
Raises ValueError when the substring is not found.");

static PyObject *
unicode_index(PyObject *self, PyObject *args)
{
    /* initialize variables to prevent gcc warning */
    Py_ssize_t result;
    PyObject *substring = NULL;
    Py_ssize_t start = 0;
    Py_ssize_t end = 0;

    if (!parse_args_finds_unicode("index", args, &substring, &start, &end))
        return NULL;

    result = any_find_slice(self, substring, start, end, 1);

    if (result == -2)
        return NULL;

    if (result < 0) {
        PyErr_SetString(PyExc_ValueError, "substring not found");
        return NULL;
    }

    return PyLong_FromSsize_t(result);
}

/*[clinic input]
str.isascii as unicode_isascii

Return True if all characters in the string are ASCII, False otherwise.

ASCII characters have code points in the range U+0000-U+007F.
Empty string is ASCII too.
[clinic start generated code]*/

static PyObject *
unicode_isascii_impl(PyObject *self)
/*[clinic end generated code: output=c5910d64b5a8003f input=5a43cbc6399621d5]*/
{
    return PyBool_FromLong(1);
}

/*[clinic input]
str.islower as unicode_islower

Return True if the string is a lowercase string, False otherwise.

A string is lowercase if all cased characters in the string are lowercase and
there is at least one cased character in the string.
[clinic start generated code]*/

static PyObject *
unicode_islower_impl(PyObject *self)
/*[clinic end generated code: output=dbd41995bd005b81 input=acec65ac6821ae47]*/
{
    Py_ssize_t i, length;
    const void *data;
    int cased;
    length = PyUnicode_GET_LENGTH(self);
    data = PyUnicode_DATA(self);

    /* Shortcut for single character strings */
    if (length == 1)
        return PyBool_FromLong(
            Py_UNICODE_ISLOWER(PyUnicode_READ(data, 0)));

    /* Special case for empty strings */
    if (length == 0)
        Py_RETURN_FALSE;

    cased = 0;
    for (i = 0; i < length; i++) {
        const Py_UCS4 ch = PyUnicode_READ(data, i);

        if (Py_UNICODE_ISUPPER(ch) || Py_UNICODE_ISTITLE(ch))
            Py_RETURN_FALSE;
        else if (!cased && Py_UNICODE_ISLOWER(ch))
            cased = 1;
    }
    return PyBool_FromLong(cased);
}

/*[clinic input]
str.isupper as unicode_isupper

Return True if the string is an uppercase string, False otherwise.

A string is uppercase if all cased characters in the string are uppercase and
there is at least one cased character in the string.
[clinic start generated code]*/

static PyObject *
unicode_isupper_impl(PyObject *self)
/*[clinic end generated code: output=049209c8e7f15f59 input=e9b1feda5d17f2d3]*/
{
    Py_ssize_t i, length;
    const void *data;
    int cased;

    length = PyUnicode_GET_LENGTH(self);
    data = PyUnicode_DATA(self);

    /* Shortcut for single character strings */
    if (length == 1)
        return PyBool_FromLong(
            Py_UNICODE_ISUPPER(PyUnicode_READ(data, 0)) != 0);

    /* Special case for empty strings */
    if (length == 0)
        Py_RETURN_FALSE;

    cased = 0;
    for (i = 0; i < length; i++) {
        const Py_UCS4 ch = PyUnicode_READ(data, i);

        if (Py_UNICODE_ISLOWER(ch) || Py_UNICODE_ISTITLE(ch))
            Py_RETURN_FALSE;
        else if (!cased && Py_UNICODE_ISUPPER(ch))
            cased = 1;
    }
    return PyBool_FromLong(cased);
}

/*[clinic input]
str.istitle as unicode_istitle

Return True if the string is a title-cased string, False otherwise.

In a title-cased string, upper- and title-case characters may only
follow uncased characters and lowercase characters only cased ones.
[clinic start generated code]*/

static PyObject *
unicode_istitle_impl(PyObject *self)
/*[clinic end generated code: output=e9bf6eb91f5d3f0e input=98d32bd2e1f06f8c]*/
{
    Py_ssize_t i, length;
    const void *data;
    int cased, previous_is_cased;

    length = PyUnicode_GET_LENGTH(self);
    data = PyUnicode_DATA(self);

    /* Shortcut for single character strings */
    if (length == 1) {
        Py_UCS4 ch = PyUnicode_READ(data, 0);
        return PyBool_FromLong((Py_UNICODE_ISTITLE(ch) != 0) ||
                               (Py_UNICODE_ISUPPER(ch) != 0));
    }

    /* Special case for empty strings */
    if (length == 0)
        Py_RETURN_FALSE;

    cased = 0;
    previous_is_cased = 0;
    for (i = 0; i < length; i++) {
        const Py_UCS4 ch = PyUnicode_READ(data, i);

        if (Py_UNICODE_ISUPPER(ch) || Py_UNICODE_ISTITLE(ch)) {
            if (previous_is_cased)
                Py_RETURN_FALSE;
            previous_is_cased = 1;
            cased = 1;
        }
        else if (Py_UNICODE_ISLOWER(ch)) {
            if (!previous_is_cased)
                Py_RETURN_FALSE;
            previous_is_cased = 1;
            cased = 1;
        }
        else
            previous_is_cased = 0;
    }
    return PyBool_FromLong(cased);
}

/*[clinic input]
str.isspace as unicode_isspace

Return True if the string is a whitespace string, False otherwise.

A string is whitespace if all characters in the string are whitespace and there
is at least one character in the string.
[clinic start generated code]*/

static PyObject *
unicode_isspace_impl(PyObject *self)
/*[clinic end generated code: output=163a63bfa08ac2b9 input=fe462cb74f8437d8]*/
{
    Py_ssize_t i, length;
    const void *data;

    length = PyUnicode_GET_LENGTH(self);
    data = PyUnicode_DATA(self);

    /* Shortcut for single character strings */
    if (length == 1)
        return PyBool_FromLong(
            Py_UNICODE_ISSPACE(PyUnicode_READ(data, 0)));

    /* Special case for empty strings */
    if (length == 0)
        Py_RETURN_FALSE;

    for (i = 0; i < length; i++) {
        const Py_UCS4 ch = PyUnicode_READ(data, i);
        if (!Py_UNICODE_ISSPACE(ch))
            Py_RETURN_FALSE;
    }
    Py_RETURN_TRUE;
}

/*[clinic input]
str.isalpha as unicode_isalpha

Return True if the string is an alphabetic string, False otherwise.

A string is alphabetic if all characters in the string are alphabetic and there
is at least one character in the string.
[clinic start generated code]*/

static PyObject *
unicode_isalpha_impl(PyObject *self)
/*[clinic end generated code: output=cc81b9ac3883ec4f input=d0fd18a96cbca5eb]*/
{
    Py_ssize_t i, length;
    const void *data;

    length = PyUnicode_GET_LENGTH(self);
    data = PyUnicode_DATA(self);

    /* Shortcut for single character strings */
    if (length == 1)
        return PyBool_FromLong(
            Py_UNICODE_ISALPHA(PyUnicode_READ(data, 0)));

    /* Special case for empty strings */
    if (length == 0)
        Py_RETURN_FALSE;

    for (i = 0; i < length; i++) {
        if (!Py_UNICODE_ISALPHA(PyUnicode_READ(data, i)))
            Py_RETURN_FALSE;
    }
    Py_RETURN_TRUE;
}

/*[clinic input]
str.isalnum as unicode_isalnum

Return True if the string is an alpha-numeric string, False otherwise.

A string is alpha-numeric if all characters in the string are alpha-numeric and
there is at least one character in the string.
[clinic start generated code]*/

static PyObject *
unicode_isalnum_impl(PyObject *self)
/*[clinic end generated code: output=a5a23490ffc3660c input=5c6579bf2e04758c]*/
{
    const void *data;
    Py_ssize_t len, i;

    data = PyUnicode_DATA(self);
    len = PyUnicode_GET_LENGTH(self);

    /* Shortcut for single character strings */
    if (len == 1) {
        const Py_UCS4 ch = PyUnicode_READ(data, 0);
        return PyBool_FromLong(Py_UNICODE_ISALNUM(ch));
    }

    /* Special case for empty strings */
    if (len == 0)
        Py_RETURN_FALSE;

    for (i = 0; i < len; i++) {
        const Py_UCS4 ch = PyUnicode_READ(data, i);
        if (!Py_UNICODE_ISALNUM(ch))
            Py_RETURN_FALSE;
    }
    Py_RETURN_TRUE;
}

/*[clinic input]
str.isdecimal as unicode_isdecimal

Return True if the string is a decimal string, False otherwise.

A string is a decimal string if all characters in the string are decimal and
there is at least one character in the string.
[clinic start generated code]*/

static PyObject *
unicode_isdecimal_impl(PyObject *self)
/*[clinic end generated code: output=fb2dcdb62d3fc548 input=336bc97ab4c8268f]*/
{
    Py_ssize_t i, length;
    const void *data;

    length = PyUnicode_GET_LENGTH(self);
    data = PyUnicode_DATA(self);

    /* Shortcut for single character strings */
    if (length == 1)
        return PyBool_FromLong(
            Py_UNICODE_ISDECIMAL(PyUnicode_READ(data, 0)));

    /* Special case for empty strings */
    if (length == 0)
        Py_RETURN_FALSE;

    for (i = 0; i < length; i++) {
        if (!Py_UNICODE_ISDECIMAL(PyUnicode_READ(data, i)))
            Py_RETURN_FALSE;
    }
    Py_RETURN_TRUE;
}

/*[clinic input]
str.isdigit as unicode_isdigit

Return True if the string is a digit string, False otherwise.

A string is a digit string if all characters in the string are digits and there
is at least one character in the string.
[clinic start generated code]*/

static PyObject *
unicode_isdigit_impl(PyObject *self)
/*[clinic end generated code: output=10a6985311da6858 input=901116c31deeea4c]*/
{
    Py_ssize_t i, length;
    const void *data;

    length = PyUnicode_GET_LENGTH(self);
    data = PyUnicode_DATA(self);

    /* Shortcut for single character strings */
    if (length == 1) {
        const Py_UCS4 ch = PyUnicode_READ(data, 0);
        return PyBool_FromLong(Py_UNICODE_ISDIGIT(ch));
    }

    /* Special case for empty strings */
    if (length == 0)
        Py_RETURN_FALSE;

    for (i = 0; i < length; i++) {
        if (!Py_UNICODE_ISDIGIT(PyUnicode_READ(data, i)))
            Py_RETURN_FALSE;
    }
    Py_RETURN_TRUE;
}

/*[clinic input]
str.isnumeric as unicode_isnumeric

Return True if the string is a numeric string, False otherwise.

A string is numeric if all characters in the string are numeric and there is at
least one character in the string.
[clinic start generated code]*/

static PyObject *
unicode_isnumeric_impl(PyObject *self)
/*[clinic end generated code: output=9172a32d9013051a input=722507db976f826c]*/
{
    Py_ssize_t i, length;
    const void *data;

    length = PyUnicode_GET_LENGTH(self);
    data = PyUnicode_DATA(self);

    /* Shortcut for single character strings */
    if (length == 1)
        return PyBool_FromLong(
            Py_UNICODE_ISNUMERIC(PyUnicode_READ(data, 0)));

    /* Special case for empty strings */
    if (length == 0)
        Py_RETURN_FALSE;

    for (i = 0; i < length; i++) {
        if (!Py_UNICODE_ISNUMERIC(PyUnicode_READ(data, i)))
            Py_RETURN_FALSE;
    }
    Py_RETURN_TRUE;
}

Py_ssize_t
_PyUnicode_ScanIdentifier(PyObject *self)
{
    Py_ssize_t i;

    Py_ssize_t len = PyUnicode_GET_LENGTH(self);
    if (len == 0) {
        /* an empty string is not a valid identifier */
        return 0;
    }


    const void *data = PyUnicode_DATA(self);
    Py_UCS4 ch = PyUnicode_READ(data, 0);
    /* PEP 3131 says that the first character must be in
       XID_Start and subsequent characters in XID_Continue,
       and for the ASCII range, the 2.x rules apply (i.e
       start with letters and underscore, continue with
       letters, digits, underscore). However, given the current
       definition of XID_Start and XID_Continue, it is sufficient
       to check just for these, except that _ must be allowed
       as starting an identifier.  */
    if (!_PyUnicode_IsXidStart(ch) && ch != 0x5F /* LOW LINE */) {
        return 0;
    }

    for (i = 1; i < len; i++) {
        ch = PyUnicode_READ(data, i);
        if (!_PyUnicode_IsXidContinue(ch)) {
            return i;
        }
    }
    return i;
}

int
PyUnicode_IsIdentifier(PyObject *self)
{
  Py_ssize_t i = _PyUnicode_ScanIdentifier(self);
  Py_ssize_t len = PyUnicode_GET_LENGTH(self);
  /* an empty string is not a valid identifier */
  return len && i == len;
}

/*[clinic input]
str.isidentifier as unicode_isidentifier

Return True if the string is a valid Python identifier, False otherwise.

Call keyword.iskeyword(s) to test whether string s is a reserved identifier,
such as "def" or "class".
[clinic start generated code]*/

static PyObject *
unicode_isidentifier_impl(PyObject *self)
/*[clinic end generated code: output=fe585a9666572905 input=2d807a104f21c0c5]*/
{
    return PyBool_FromLong(PyUnicode_IsIdentifier(self));
}

/*[clinic input]
str.isprintable as unicode_isprintable

Return True if the string is printable, False otherwise.

A string is printable if all of its characters are considered printable in
repr() or if it is empty.
[clinic start generated code]*/

static PyObject *
unicode_isprintable_impl(PyObject *self)
/*[clinic end generated code: output=3ab9626cd32dd1a0 input=98a0e1c2c1813209]*/
{
    Py_ssize_t i, length;
    const void *data;

    length = PyUnicode_GET_LENGTH(self);
    data = PyUnicode_DATA(self);

    /* Shortcut for single character strings */
    if (length == 1)
        return PyBool_FromLong(
            Py_UNICODE_ISPRINTABLE(PyUnicode_READ( data, 0)));

    for (i = 0; i < length; i++) {
        if (!Py_UNICODE_ISPRINTABLE(PyUnicode_READ( data, i))) {
            Py_RETURN_FALSE;
        }
    }
    Py_RETURN_TRUE;
}

/*[clinic input]
str.join as unicode_join

    iterable: object
    /

Concatenate any number of strings.

The string whose method is called is inserted in between each given string.
The result is returned as a new string.

Example: '.'.join(['ab', 'pq', 'rs']) -> 'ab.pq.rs'
[clinic start generated code]*/

static PyObject *
unicode_join(PyObject *self, PyObject *iterable)
/*[clinic end generated code: output=6857e7cecfe7bf98 input=2f70422bfb8fa189]*/
{
    return PyUnicode_Join(self, iterable);
}

static Py_ssize_t
unicode_length(PyObject *self)
{
    return PyUnicode_GET_LENGTH(self);
}

/*[clinic input]
str.ljust as unicode_ljust

    width: Py_ssize_t
    fillchar: Py_UCS4 = ' '
    /

Return a left-justified string of length width.

Padding is done using the specified fill character (default is a space).
[clinic start generated code]*/

static PyObject *
unicode_ljust_impl(PyObject *self, Py_ssize_t width, Py_UCS4 fillchar)
/*[clinic end generated code: output=1cce0e0e0a0b84b3 input=3ab599e335e60a32]*/
{
    if (PyUnicode_GET_LENGTH(self) >= width)
        return unicode_result_unchanged(self);

    return pad(self, 0, width - PyUnicode_GET_LENGTH(self), fillchar);
}

/*[clinic input]
str.lower as unicode_lower

Return a copy of the string converted to lowercase.
[clinic start generated code]*/

static PyObject *
unicode_lower_impl(PyObject *self)
/*[clinic end generated code: output=84ef9ed42efad663 input=60a2984b8beff23a]*/
{
    return case_operation(self, do_lower);
}

#define LEFTSTRIP 0
#define RIGHTSTRIP 1
#define BOTHSTRIP 2

/* Arrays indexed by above */
static const char *stripfuncnames[] = {"lstrip", "rstrip", "strip"};

#define STRIPNAME(i) (stripfuncnames[i])

/* externally visible for str.strip(unicode) */
PyObject *
_PyUnicode_XStrip(PyObject *self, int striptype, PyObject *sepobj)
{
    const void *data;
    Py_ssize_t i, j, len;
    BLOOM_MASK sepmask;
    Py_ssize_t seplen;

    data = PyUnicode_DATA(self);
    len = PyUnicode_GET_LENGTH(self);
    seplen = PyUnicode_GET_LENGTH(sepobj);
    sepmask = make_bloom_mask(PyUnicode_DATA(sepobj),
                              seplen);

    i = 0;
    if (striptype != RIGHTSTRIP) {
        while (i < len) {
            Py_UCS4 ch = PyUnicode_READ( data, i);
            if (!BLOOM(sepmask, ch))
                break;
            if (PyUnicode_FindChar(sepobj, ch, 0, seplen, 1) < 0)
                break;
            i++;
        }
    }

    j = len;
    if (striptype != LEFTSTRIP) {
        j--;
        while (j >= i) {
            Py_UCS4 ch = PyUnicode_READ( data, j);
            if (!BLOOM(sepmask, ch))
                break;
            if (PyUnicode_FindChar(sepobj, ch, 0, seplen, 1) < 0)
                break;
            j--;
        }

        j++;
    }

    return PyUnicode_Substring(self, i, j);
}

PyObject*
PyUnicode_Substring(PyObject *self, Py_ssize_t start, Py_ssize_t end)
{
    const unsigned char *data;
    Py_ssize_t length;

    length = PyUnicode_GET_LENGTH(self);
    end = Py_MIN(end, length);

    if (start == 0 && end == length)
        return unicode_result_unchanged(self);

    if (start < 0 || end < 0) {
        PyErr_SetString(PyExc_IndexError, "string index out of range");
        return NULL;
    }
    if (start >= length || end < start)
        _Py_RETURN_UNICODE_EMPTY();

    length = end - start;
      {
        data = PyUnicode_1BYTE_DATA(self);
        return PyUnicode_FromStringAndSize((char *) (data + start), length);
    }
}

static PyObject *
do_strip(PyObject *self, int striptype)
{
    Py_ssize_t len, i, j;

    len = PyUnicode_GET_LENGTH(self);
    {

        const void *data = PyUnicode_DATA(self);

        i = 0;
        if (striptype != RIGHTSTRIP) {
            while (i < len) {
                Py_UCS4 ch = PyUnicode_READ( data, i);
                if (!Py_UNICODE_ISSPACE(ch))
                    break;
                i++;
            }
        }

        j = len;
        if (striptype != LEFTSTRIP) {
            j--;
            while (j >= i) {
                Py_UCS4 ch = PyUnicode_READ( data, j);
                if (!Py_UNICODE_ISSPACE(ch))
                    break;
                j--;
            }
            j++;
        }
    }

    return PyUnicode_Substring(self, i, j);
}


static PyObject *
do_argstrip(PyObject *self, int striptype, PyObject *sep)
{
    if (sep != Py_None) {
        if (PyUnicode_Check(sep))
            return _PyUnicode_XStrip(self, striptype, sep);
        else {
            PyErr_Format(PyExc_TypeError,
                         "%s arg must be None or str",
                         STRIPNAME(striptype));
            return NULL;
        }
    }

    return do_strip(self, striptype);
}


/*[clinic input]
str.strip as unicode_strip

    chars: object = None
    /

Return a copy of the string with leading and trailing whitespace removed.

If chars is given and not None, remove characters in chars instead.
[clinic start generated code]*/

static PyObject *
unicode_strip_impl(PyObject *self, PyObject *chars)
/*[clinic end generated code: output=ca19018454345d57 input=385289c6f423b954]*/
{
    return do_argstrip(self, BOTHSTRIP, chars);
}


/*[clinic input]
str.lstrip as unicode_lstrip

    chars: object = None
    /

Return a copy of the string with leading whitespace removed.

If chars is given and not None, remove characters in chars instead.
[clinic start generated code]*/

static PyObject *
unicode_lstrip_impl(PyObject *self, PyObject *chars)
/*[clinic end generated code: output=3b43683251f79ca7 input=529f9f3834448671]*/
{
    return do_argstrip(self, LEFTSTRIP, chars);
}


/*[clinic input]
str.rstrip as unicode_rstrip

    chars: object = None
    /

Return a copy of the string with trailing whitespace removed.

If chars is given and not None, remove characters in chars instead.
[clinic start generated code]*/

static PyObject *
unicode_rstrip_impl(PyObject *self, PyObject *chars)
/*[clinic end generated code: output=4a59230017cc3b7a input=62566c627916557f]*/
{
    return do_argstrip(self, RIGHTSTRIP, chars);
}


static PyObject*
unicode_repeat(PyObject *str, Py_ssize_t len)
{
    PyObject *u;
    Py_ssize_t nchars, n;

    if (len < 1)
        _Py_RETURN_UNICODE_EMPTY();

    /* no repeat, return original string */
    if (len == 1)
        return unicode_result_unchanged(str);

    if (PyUnicode_GET_LENGTH(str) > PY_SSIZE_T_MAX / len) {
        PyErr_SetString(PyExc_OverflowError,
                        "repeated string is too long");
        return NULL;
    }
    nchars = len * PyUnicode_GET_LENGTH(str);

    u = PyUnicode_New(nchars);
    if (!u)
        return NULL;

    if (PyUnicode_GET_LENGTH(str) == 1) {
        Py_UCS4 fill_char = PyUnicode_READ( PyUnicode_DATA(str), 0);
	void *to = PyUnicode_DATA(u);
	memset(to, (unsigned char)fill_char, len);
    }
    else {
        /* number of characters copied this far */
        Py_ssize_t done = PyUnicode_GET_LENGTH(str);
        char *to = (char *) PyUnicode_DATA(u);
        memcpy(to, PyUnicode_DATA(str),
	       PyUnicode_GET_LENGTH(str));
        while (done < nchars) {
            n = (done <= nchars-done) ? done : nchars-done;
            memcpy(to + done, to, n);
            done += n;
        }
    }

    assert(_PyUnicode_CheckConsistency(u, 1));
    return u;
}

PyObject *
PyUnicode_Replace(PyObject *str,
                  PyObject *substr,
                  PyObject *replstr,
                  Py_ssize_t maxcount)
{
    if (ensure_unicode(str) < 0 || ensure_unicode(substr) < 0 ||
            ensure_unicode(replstr) < 0)
        return NULL;
    return replace(str, substr, replstr, maxcount);
}

/*[clinic input]
str.replace as unicode_replace

    old: unicode
    new: unicode
    count: Py_ssize_t = -1
        Maximum number of occurrences to replace.
        -1 (the default value) means replace all occurrences.
    /

Return a copy with all occurrences of substring old replaced by new.

If the optional argument count is given, only the first count occurrences are
replaced.
[clinic start generated code]*/

static PyObject *
unicode_replace_impl(PyObject *self, PyObject *old, PyObject *new,
                     Py_ssize_t count)
/*[clinic end generated code: output=b63f1a8b5eebf448 input=147d12206276ebeb]*/
{
    return replace(self, old, new, count);
}

/*[clinic input]
str.removeprefix as unicode_removeprefix

    prefix: unicode
    /

Return a str with the given prefix string removed if present.

If the string starts with the prefix string, return string[len(prefix):].
Otherwise, return a copy of the original string.
[clinic start generated code]*/

static PyObject *
unicode_removeprefix_impl(PyObject *self, PyObject *prefix)
/*[clinic end generated code: output=f1e5945e9763bcb9 input=27ec40b99a37eb88]*/
{
    int match = tailmatch(self, prefix, 0, PY_SSIZE_T_MAX, -1);
    if (match == -1) {
        return NULL;
    }
    if (match) {
        return PyUnicode_Substring(self, PyUnicode_GET_LENGTH(prefix),
                                   PyUnicode_GET_LENGTH(self));
    }
    return unicode_result_unchanged(self);
}

/*[clinic input]
str.removesuffix as unicode_removesuffix

    suffix: unicode
    /

Return a str with the given suffix string removed if present.

If the string ends with the suffix string and that suffix is not empty,
return string[:-len(suffix)]. Otherwise, return a copy of the original
string.
[clinic start generated code]*/

static PyObject *
unicode_removesuffix_impl(PyObject *self, PyObject *suffix)
/*[clinic end generated code: output=d36629e227636822 input=12cc32561e769be4]*/
{
    int match = tailmatch(self, suffix, 0, PY_SSIZE_T_MAX, +1);
    if (match == -1) {
        return NULL;
    }
    if (match) {
        return PyUnicode_Substring(self, 0, PyUnicode_GET_LENGTH(self)
                                            - PyUnicode_GET_LENGTH(suffix));
    }
    return unicode_result_unchanged(self);
}

static PyObject *
unicode_repr(PyObject *unicode)
{
    PyObject *repr;
    Py_ssize_t isize;
    Py_ssize_t osize, squote, dquote, i, o;
    Py_UCS4 max, quote;
    int unchanged;
    const void *idata;
    void *odata;

    isize = PyUnicode_GET_LENGTH(unicode);
    idata = PyUnicode_DATA(unicode);

    /* Compute length of output, quote characters, and
       maximum character */
    osize = 0;
    max = 127;
    squote = dquote = 0;
    for (i = 0; i < isize; i++) {
        Py_UCS4 ch = PyUnicode_READ(idata, i);
        Py_ssize_t incr = 1;
        switch (ch) {
        case '\'': squote++; break;
        case '"':  dquote++; break;
        case '\\': case '\t': case '\r': case '\n':
            incr = 2;
            break;
        default:
            /* Fast-path ASCII */
            if (ch < ' ' || ch == 0x7f)
                incr = 4; /* \xHH */
            else if (ch < 0x7f)
                ;
            else if (Py_UNICODE_ISPRINTABLE(ch))
                max = ch > max ? ch : max;
            else if (ch < 0x100)
                incr = 4; /* \xHH */
            else if (ch < 0x10000)
                incr = 6; /* \uHHHH */
            else
                incr = 10; /* \uHHHHHHHH */
        }
        if (osize > PY_SSIZE_T_MAX - incr) {
            PyErr_SetString(PyExc_OverflowError,
                            "string is too long to generate repr");
            return NULL;
        }
        osize += incr;
    }

    quote = '\'';
    unchanged = (osize == isize);
    if (squote) {
        unchanged = 0;
        if (dquote)
            /* Both squote and dquote present. Use squote,
               and escape them */
            osize += squote;
        else
            quote = '"';
    }
    osize += 2;   /* quotes */

    repr = PyUnicode_New(osize);
    if (repr == NULL)
        return NULL;
    odata = PyUnicode_DATA(repr);

    PyUnicode_WRITE(odata, 0, quote);
    PyUnicode_WRITE(odata, osize-1, quote);
    if (unchanged) {
        _PyUnicode_FastCopyCharacters(repr, 1,
                                      unicode, 0,
                                      isize);
    }
    else {
        for (i = 0, o = 1; i < isize; i++) {
            Py_UCS4 ch = PyUnicode_READ(idata, i);

            /* Escape quotes and backslashes */
            if ((ch == quote) || (ch == '\\')) {
                PyUnicode_WRITE(odata, o++, '\\');
                PyUnicode_WRITE(odata, o++, ch);
                continue;
            }

            /* Map special whitespace to '\t', \n', '\r' */
            if (ch == '\t') {
                PyUnicode_WRITE(odata, o++, '\\');
                PyUnicode_WRITE(odata, o++, 't');
            }
            else if (ch == '\n') {
                PyUnicode_WRITE(odata, o++, '\\');
                PyUnicode_WRITE(odata, o++, 'n');
            }
            else if (ch == '\r') {
                PyUnicode_WRITE(odata, o++, '\\');
                PyUnicode_WRITE(odata, o++, 'r');
            }

            /* Map non-printable US ASCII to '\xhh' */
            else if (ch < ' ' || ch == 0x7F) {
                PyUnicode_WRITE(odata, o++, '\\');
                PyUnicode_WRITE(odata, o++, 'x');
                PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 4) & 0x000F]);
                PyUnicode_WRITE(odata, o++, Py_hexdigits[ch & 0x000F]);
            }

            /* Copy ASCII characters as-is */
            else if (ch < 0x7F) {
                PyUnicode_WRITE(odata, o++, ch);
            }

            /* Non-ASCII characters */
            else {
                /* Map Unicode whitespace and control characters
                   (categories Z* and C* except ASCII space)
                */
                if (!Py_UNICODE_ISPRINTABLE(ch)) {
                    PyUnicode_WRITE(odata, o++, '\\');
                    /* Map 8-bit characters to '\xhh' */
                    if (ch <= 0xff) {
                        PyUnicode_WRITE(odata, o++, 'x');
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 4) & 0x000F]);
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[ch & 0x000F]);
                    }
                    /* Map 16-bit characters to '\uxxxx' */
                    else if (ch <= 0xffff) {
                        PyUnicode_WRITE(odata, o++, 'u');
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 12) & 0xF]);
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 8) & 0xF]);
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 4) & 0xF]);
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[ch & 0xF]);
                    }
                    /* Map 21-bit characters to '\U00xxxxxx' */
                    else {
                        PyUnicode_WRITE(odata, o++, 'U');
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 28) & 0xF]);
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 24) & 0xF]);
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 20) & 0xF]);
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 16) & 0xF]);
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 12) & 0xF]);
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 8) & 0xF]);
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[(ch >> 4) & 0xF]);
                        PyUnicode_WRITE(odata, o++, Py_hexdigits[ch & 0xF]);
                    }
                }
                /* Copy characters as-is */
                else {
                    PyUnicode_WRITE(odata, o++, ch);
                }
            }
        }
    }
    /* Closing quote already added at the beginning */
    assert(_PyUnicode_CheckConsistency(repr, 1));
    return repr;
}

PyDoc_STRVAR(rfind__doc__,
             "S.rfind(sub[, start[, end]]) -> int\n\
\n\
Return the highest index in S where substring sub is found,\n\
such that sub is contained within S[start:end].  Optional\n\
arguments start and end are interpreted as in slice notation.\n\
\n\
Return -1 on failure.");

static PyObject *
unicode_rfind(PyObject *self, PyObject *args)
{
    /* initialize variables to prevent gcc warning */
    PyObject *substring = NULL;
    Py_ssize_t start = 0;
    Py_ssize_t end = 0;
    Py_ssize_t result;

    if (!parse_args_finds_unicode("rfind", args, &substring, &start, &end))
        return NULL;

    result = any_find_slice(self, substring, start, end, -1);

    if (result == -2)
        return NULL;

    return PyLong_FromSsize_t(result);
}

PyDoc_STRVAR(rindex__doc__,
             "S.rindex(sub[, start[, end]]) -> int\n\
\n\
Return the highest index in S where substring sub is found,\n\
such that sub is contained within S[start:end].  Optional\n\
arguments start and end are interpreted as in slice notation.\n\
\n\
Raises ValueError when the substring is not found.");

static PyObject *
unicode_rindex(PyObject *self, PyObject *args)
{
    /* initialize variables to prevent gcc warning */
    PyObject *substring = NULL;
    Py_ssize_t start = 0;
    Py_ssize_t end = 0;
    Py_ssize_t result;

    if (!parse_args_finds_unicode("rindex", args, &substring, &start, &end))
        return NULL;

    result = any_find_slice(self, substring, start, end, -1);

    if (result == -2)
        return NULL;

    if (result < 0) {
        PyErr_SetString(PyExc_ValueError, "substring not found");
        return NULL;
    }

    return PyLong_FromSsize_t(result);
}

/*[clinic input]
str.rjust as unicode_rjust

    width: Py_ssize_t
    fillchar: Py_UCS4 = ' '
    /

Return a right-justified string of length width.

Padding is done using the specified fill character (default is a space).
[clinic start generated code]*/

static PyObject *
unicode_rjust_impl(PyObject *self, Py_ssize_t width, Py_UCS4 fillchar)
/*[clinic end generated code: output=804a1a57fbe8d5cf input=d05f550b5beb1f72]*/
{

    if (PyUnicode_GET_LENGTH(self) >= width)
        return unicode_result_unchanged(self);

    return pad(self, width - PyUnicode_GET_LENGTH(self), 0, fillchar);
}

PyObject *
PyUnicode_Split(PyObject *s, PyObject *sep, Py_ssize_t maxsplit)
{
    if (ensure_unicode(s) < 0 || (sep != NULL && ensure_unicode(sep) < 0))
        return NULL;

    return split(s, sep, maxsplit);
}

/*[clinic input]
str.split as unicode_split

    sep: object = None
        The delimiter according which to split the string.
        None (the default value) means split according to any whitespace,
        and discard empty strings from the result.
    maxsplit: Py_ssize_t = -1
        Maximum number of splits to do.
        -1 (the default value) means no limit.

Return a list of the words in the string, using sep as the delimiter string.
[clinic start generated code]*/

static PyObject *
unicode_split_impl(PyObject *self, PyObject *sep, Py_ssize_t maxsplit)
/*[clinic end generated code: output=3a65b1db356948dc input=606e750488a82359]*/
{
    if (sep == Py_None)
        return split(self, NULL, maxsplit);
    if (PyUnicode_Check(sep))
        return split(self, sep, maxsplit);

    PyErr_Format(PyExc_TypeError,
                 "must be str or None, not %.100s",
                 Py_TYPE(sep)->tp_name);
    return NULL;
}

PyObject *
PyUnicode_Partition(PyObject *str_obj, PyObject *sep_obj)
{
    PyObject* out;
    const void *buf1, *buf2;
    Py_ssize_t len1, len2;

    if (ensure_unicode(str_obj) < 0 || ensure_unicode(sep_obj) < 0)
        return NULL;

    len1 = PyUnicode_GET_LENGTH(str_obj);
    len2 = PyUnicode_GET_LENGTH(sep_obj);
    if (len1 < len2) {
        _Py_INCREF_UNICODE_EMPTY();
        if (!unicode_empty)
            out = NULL;
        else {
            out = PyTuple_Pack(3, str_obj, unicode_empty, unicode_empty);
            Py_DECREF(unicode_empty);
        }
        return out;
    }
    buf1 = PyUnicode_DATA(str_obj);
    buf2 = PyUnicode_DATA(sep_obj);
    out = ucs1lib_partition(str_obj, buf1, len1, sep_obj, buf2, len2);
    return out;
}


PyObject *
PyUnicode_RPartition(PyObject *str_obj, PyObject *sep_obj)
{
    PyObject* out;
    const void *buf1, *buf2;
    Py_ssize_t len1, len2;

    if (ensure_unicode(str_obj) < 0 || ensure_unicode(sep_obj) < 0)
        return NULL;

    len1 = PyUnicode_GET_LENGTH(str_obj);
    len2 = PyUnicode_GET_LENGTH(sep_obj);
    if (len1 < len2) {
        _Py_INCREF_UNICODE_EMPTY();
        if (!unicode_empty)
            out = NULL;
        else {
            out = PyTuple_Pack(3, unicode_empty, unicode_empty, str_obj);
            Py_DECREF(unicode_empty);
        }
        return out;
    }
    buf1 = PyUnicode_DATA(str_obj);
    buf2 = PyUnicode_DATA(sep_obj);
    out = ucs1lib_rpartition(str_obj, buf1, len1, sep_obj, buf2, len2);
    return out;
}

/*[clinic input]
str.partition as unicode_partition

    sep: object
    /

Partition the string into three parts using the given separator.

This will search for the separator in the string.  If the separator is found,
returns a 3-tuple containing the part before the separator, the separator
itself, and the part after it.

If the separator is not found, returns a 3-tuple containing the original string
and two empty strings.
[clinic start generated code]*/

static PyObject *
unicode_partition(PyObject *self, PyObject *sep)
/*[clinic end generated code: output=e4ced7bd253ca3c4 input=f29b8d06c63e50be]*/
{
    return PyUnicode_Partition(self, sep);
}

/*[clinic input]
str.rpartition as unicode_rpartition = str.partition

Partition the string into three parts using the given separator.

This will search for the separator in the string, starting at the end. If
the separator is found, returns a 3-tuple containing the part before the
separator, the separator itself, and the part after it.

If the separator is not found, returns a 3-tuple containing two empty strings
and the original string.
[clinic start generated code]*/

static PyObject *
unicode_rpartition(PyObject *self, PyObject *sep)
/*[clinic end generated code: output=1aa13cf1156572aa input=c4b7db3ef5cf336a]*/
{
    return PyUnicode_RPartition(self, sep);
}

PyObject *
PyUnicode_RSplit(PyObject *s, PyObject *sep, Py_ssize_t maxsplit)
{
    if (ensure_unicode(s) < 0 || (sep != NULL && ensure_unicode(sep) < 0))
        return NULL;

    return rsplit(s, sep, maxsplit);
}

/*[clinic input]
str.rsplit as unicode_rsplit = str.split

Return a list of the words in the string, using sep as the delimiter string.

Splits are done starting at the end of the string and working to the front.
[clinic start generated code]*/

static PyObject *
unicode_rsplit_impl(PyObject *self, PyObject *sep, Py_ssize_t maxsplit)
/*[clinic end generated code: output=c2b815c63bcabffc input=12ad4bf57dd35f15]*/
{
    if (sep == Py_None)
        return rsplit(self, NULL, maxsplit);
    if (PyUnicode_Check(sep))
        return rsplit(self, sep, maxsplit);

    PyErr_Format(PyExc_TypeError,
                 "must be str or None, not %.100s",
                 Py_TYPE(sep)->tp_name);
    return NULL;
}

/*[clinic input]
str.splitlines as unicode_splitlines

    keepends: bool(accept={int}) = False

Return a list of the lines in the string, breaking at line boundaries.

Line breaks are not included in the resulting list unless keepends is given and
true.
[clinic start generated code]*/

static PyObject *
unicode_splitlines_impl(PyObject *self, int keepends)
/*[clinic end generated code: output=f664dcdad153ec40 input=b508e180459bdd8b]*/
{
    return PyUnicode_Splitlines(self, keepends);
}

static
PyObject *unicode_str(PyObject *self)
{
    return unicode_result_unchanged(self);
}

/*[clinic input]
str.swapcase as unicode_swapcase

Convert uppercase characters to lowercase and lowercase characters to uppercase.
[clinic start generated code]*/

static PyObject *
unicode_swapcase_impl(PyObject *self)
/*[clinic end generated code: output=5d28966bf6d7b2af input=3f3ef96d5798a7bb]*/
{
    return case_operation(self, do_swapcase);
}

/*[clinic input]
str.upper as unicode_upper

Return a copy of the string converted to uppercase.
[clinic start generated code]*/

static PyObject *
unicode_upper_impl(PyObject *self)
/*[clinic end generated code: output=1b7ddd16bbcdc092 input=db3d55682dfe2e6c]*/
{
    return case_operation(self, do_upper);
}

/*[clinic input]
str.zfill as unicode_zfill

    width: Py_ssize_t
    /

Pad a numeric string with zeros on the left, to fill a field of the given width.

The string is never truncated.
[clinic start generated code]*/

static PyObject *
unicode_zfill_impl(PyObject *self, Py_ssize_t width)
/*[clinic end generated code: output=e13fb6bdf8e3b9df input=c6b2f772c6f27799]*/
{
    Py_ssize_t fill;
    PyObject *u;
    const void *data;
    Py_UCS4 chr;

    if (PyUnicode_GET_LENGTH(self) >= width)
        return unicode_result_unchanged(self);

    fill = width - PyUnicode_GET_LENGTH(self);

    u = pad(self, fill, 0, '0');

    if (u == NULL)
        return NULL;

    data = PyUnicode_DATA(u);
    chr = PyUnicode_READ( data, fill);

    if (chr == '+' || chr == '-') {
        /* move sign to beginning of string */
        PyUnicode_WRITE(data, 0, chr);
        PyUnicode_WRITE(data, fill, '0');
    }

    assert(_PyUnicode_CheckConsistency(u, 1));
    return u;
}

PyDoc_STRVAR(startswith__doc__,
             "S.startswith(prefix[, start[, end]]) -> bool\n\
\n\
Return True if S starts with the specified prefix, False otherwise.\n\
With optional start, test S beginning at that position.\n\
With optional end, stop comparing S at that position.\n\
prefix can also be a tuple of strings to try.");

static PyObject *
unicode_startswith(PyObject *self,
                   PyObject *args)
{
    PyObject *subobj;
    PyObject *substring;
    Py_ssize_t start = 0;
    Py_ssize_t end = PY_SSIZE_T_MAX;
    int result;

    if (!ucs1lib_parse_args_finds("startswith", args, &subobj, &start, &end))
        return NULL;
    
    if (PyTuple_Check(subobj)) {
        Py_ssize_t i;
        for (i = 0; i < PyTuple_Size(subobj); i++) {
            substring = PyTuple_GetItem(subobj, i);
            if (!PyUnicode_Check(substring)) {
                PyErr_Format(PyExc_TypeError,
                             "tuple for startswith must only contain str, "
                             "not %.100s",
                             Py_TYPE(substring)->tp_name);
                return NULL;
            }
            result = tailmatch(self, substring, start, end, -1);
            if (result == -1)
                return NULL;
            if (result) {
                Py_RETURN_TRUE;
            }
        }
        /* nothing matched */
        Py_RETURN_FALSE;
    }
    if (!PyUnicode_Check(subobj)) {
        PyErr_Format(PyExc_TypeError,
                     "startswith first arg must be str or "
                     "a tuple of str, not %.100s", Py_TYPE(subobj)->tp_name);
        return NULL;
    }
    result = tailmatch(self, subobj, start, end, -1);
    if (result == -1)
        return NULL;
    return PyBool_FromLong(result);
}


PyDoc_STRVAR(endswith__doc__,
             "S.endswith(suffix[, start[, end]]) -> bool\n\
\n\
Return True if S ends with the specified suffix, False otherwise.\n\
With optional start, test S beginning at that position.\n\
With optional end, stop comparing S at that position.\n\
suffix can also be a tuple of strings to try.");

static PyObject *
unicode_endswith(PyObject *self,
                 PyObject *args)
{
    PyObject *subobj;
    PyObject *substring;
    Py_ssize_t start = 0;
    Py_ssize_t end = PY_SSIZE_T_MAX;
    int result;
    if (!ucs1lib_parse_args_finds("endswith", args, &subobj, &start, &end))
        return NULL;
    if (PyTuple_Check(subobj)) {
        Py_ssize_t i;
        for (i = 0; i < PyTuple_Size(subobj); i++) {
            substring = PyTuple_GetItem(subobj, i);
            if (!PyUnicode_Check(substring)) {
                PyErr_Format(PyExc_TypeError,
                             "tuple for endswith must only contain str, "
                             "not %.100s",
                             Py_TYPE(substring)->tp_name);
                return NULL;
            }
            result = tailmatch(self, substring, start, end, +1);
            if (result == -1)
                return NULL;
            if (result) {
                Py_RETURN_TRUE;
            }
        }
        Py_RETURN_FALSE;
    }
    if (!PyUnicode_Check(subobj)) {
        PyErr_Format(PyExc_TypeError,
                     "endswith first arg must be str or "
                     "a tuple of str, not %.100s", Py_TYPE(subobj)->tp_name);
        return NULL;
    }
    result = tailmatch(self, subobj, start, end, +1);
    if (result == -1)
        return NULL;
    return PyBool_FromLong(result);
}

static inline void
_PyUnicodeWriter_Update(_PyUnicodeWriter *writer)
{
    writer->data = PyUnicode_DATA(writer->buffer);

    if (!writer->readonly) {
        writer->size = PyUnicode_GET_LENGTH(writer->buffer);
    }
    else {
        writer->size = 0;
    }
}

void
_PyUnicodeWriter_Init(_PyUnicodeWriter *writer)
{
    memset(writer, 0, sizeof(*writer));
}

int
_PyUnicodeWriter_PrepareInternal(_PyUnicodeWriter *writer,
                                 Py_ssize_t length)
{
    Py_ssize_t newlen;

    /* ensure that the _PyUnicodeWriter_Prepare macro was used */
    if (length > PY_SSIZE_T_MAX - writer->pos) {
        PyErr_NoMemory();
        return -1;
    }
    newlen = writer->pos + length;
    if (writer->buffer == NULL) {
        assert(!writer->readonly);
        if (writer->overallocate
            && newlen <= (PY_SSIZE_T_MAX - newlen / OVERALLOCATE_FACTOR)) {
            /* overallocate to limit the number of realloc() */
            newlen += newlen / OVERALLOCATE_FACTOR;
        }
        if (newlen < writer->min_length)
            newlen = writer->min_length;

        writer->buffer = PyUnicode_New(newlen);
        if (writer->buffer == NULL)
            return -1;
    }
    else if (newlen > writer->size) {
        if (writer->overallocate
            && newlen <= (PY_SSIZE_T_MAX - newlen / OVERALLOCATE_FACTOR)) {
            /* overallocate to limit the number of realloc() */
            newlen += newlen / OVERALLOCATE_FACTOR;
        }
        if (newlen < writer->min_length)
            newlen = writer->min_length;
	  {
	    if (resize_inplace(writer->buffer, newlen) < -1) {
	      return -1;
	    }
	  }
    }
    _PyUnicodeWriter_Update(writer);
    return 0;

#undef OVERALLOCATE_FACTOR
}

static inline int
_PyUnicodeWriter_WriteCharInline(_PyUnicodeWriter *writer, Py_UCS4 ch)
{
    assert(ch <= MAX_UNICODE);
    if (_PyUnicodeWriter_Prepare(writer, 1) < 0)
        return -1;
    PyUnicode_WRITE(writer->data, writer->pos, ch);
    writer->pos++;
    return 0;
}

int
_PyUnicodeWriter_WriteChar(_PyUnicodeWriter *writer, Py_UCS4 ch)
{
    return _PyUnicodeWriter_WriteCharInline(writer, ch);
}

int
_PyUnicodeWriter_WriteStr(_PyUnicodeWriter *writer, PyObject *str)
{
    Py_ssize_t len;

    len = PyUnicode_GET_LENGTH(str);
    if (len == 0)
        return 0;
    if (writer->buffer == NULL && !writer->overallocate) {
            assert(_PyUnicode_CheckConsistency(str, 1));
            writer->readonly = 1;
            Py_INCREF(str);
            writer->buffer = str;
            _PyUnicodeWriter_Update(writer);
            writer->pos += len;
            return 0;
    }
    if (_PyUnicodeWriter_PrepareInternal(writer, len) == -1)
      return -1;
	
    _PyUnicode_FastCopyCharacters(writer->buffer, writer->pos,
                                  str, 0, len);
    writer->pos += len;
    return 0;
}

int
_PyUnicodeWriter_WriteSubstring(_PyUnicodeWriter *writer, PyObject *str,
                                Py_ssize_t start, Py_ssize_t end)
{
    Py_ssize_t len;
    assert(0 <= start);
    assert(end <= PyUnicode_GET_LENGTH(str));
    assert(start <= end);

    if (end == 0)
        return 0;

    if (start == 0 && end == PyUnicode_GET_LENGTH(str))
        return _PyUnicodeWriter_WriteStr(writer, str);

    len = end - start;

    if (_PyUnicodeWriter_Prepare(writer, len) < 0)
        return -1;

    _PyUnicode_FastCopyCharacters(writer->buffer, writer->pos,
                                  str, start, len);
    writer->pos += len;
    return 0;
}

int
_PyUnicodeWriter_WriteASCIIString(_PyUnicodeWriter *writer,
                                  const char *ascii, Py_ssize_t len)
{
    if (len == -1)
        len = strlen(ascii);

    assert(ucs1lib_find_max_char((const Py_UCS1*)ascii, (const Py_UCS1*)ascii + len) < 128);

    if (writer->buffer == NULL && !writer->overallocate) {
        PyObject *str;

        str = PyUnicode_FromStringAndSize(ascii, len);
        if (str == NULL)
            return -1;

        writer->readonly = 1;
        writer->buffer = str;
        _PyUnicodeWriter_Update(writer);
        writer->pos += len;
        return 0;
    }

    if (_PyUnicodeWriter_Prepare(writer, len) == -1)
        return -1;
    {
        const Py_UCS1 *str = (const Py_UCS1 *)ascii;
        Py_UCS1 *data = writer->data;

        memcpy(data + writer->pos, str, len);
    }
    writer->pos += len;
    return 0;
}

int
_PyUnicodeWriter_WriteLatin1String(_PyUnicodeWriter *writer,
                                   const char *str, Py_ssize_t len)
{
    if (_PyUnicodeWriter_Prepare(writer, len) == -1)
        return -1;
    unicode_write_cstr(writer->buffer, writer->pos, str, len);
    writer->pos += len;
    return 0;
}

PyObject *
_PyUnicodeWriter_Finish(_PyUnicodeWriter *writer)
{
    PyObject *str;

    if (writer->pos == 0) {
        Py_CLEAR(writer->buffer);
        _Py_RETURN_UNICODE_EMPTY();
    }

    str = writer->buffer;
    writer->buffer = NULL;

    if (writer->readonly) {
        assert(PyUnicode_GET_LENGTH(str) == writer->pos);
        return str;
    }

    if (PyUnicode_GET_LENGTH(str) != writer->pos) {
	if (resize_inplace(str, writer->pos) < 0) {
	  Py_DECREF(str);
	  return NULL;
	}
    }

    assert(_PyUnicode_CheckConsistency(str, 1));
    return unicode_result_ready(str);
}

void
_PyUnicodeWriter_Dealloc(_PyUnicodeWriter *writer)
{
    Py_CLEAR(writer->buffer);
}

#include "stringlib/unicode_format.h"

PyDoc_STRVAR(format__doc__,
             "S.format(*args, **kwargs) -> str\n\
\n\
Return a formatted version of S, using substitutions from args and kwargs.\n\
The substitutions are identified by braces ('{' and '}').");

PyDoc_STRVAR(format_map__doc__,
             "S.format_map(mapping) -> str\n\
\n\
Return a formatted version of S, using substitutions from mapping.\n\
The substitutions are identified by braces ('{' and '}').");

/*[clinic input]
str.__format__ as unicode___format__

    format_spec: unicode
    /

Return a formatted version of the string as described by format_spec.
[clinic start generated code]*/

static PyObject *
unicode___format___impl(PyObject *self, PyObject *format_spec)
/*[clinic end generated code: output=45fceaca6d2ba4c8 input=5e135645d167a214]*/
{
    _PyUnicodeWriter writer;
    int ret;

    _PyUnicodeWriter_Init(&writer);
    ret = _PyUnicode_FormatAdvancedWriter(&writer,
                                          self, format_spec, 0,
                                          PyUnicode_GET_LENGTH(format_spec));
    if (ret == -1) {
        _PyUnicodeWriter_Dealloc(&writer);
        return NULL;
    }
    return _PyUnicodeWriter_Finish(&writer);
}

/*[clinic input]
str.__sizeof__ as unicode_sizeof

Return the size of the string in memory, in bytes.
[clinic start generated code]*/

static PyObject *
unicode_sizeof_impl(PyObject *self)
/*[clinic end generated code: output=6dbc2f5a408b6d4f input=6dd011c108e33fb0]*/
{
    Py_ssize_t size;

    /* If it's a compact object, account for base structure +
       character data. */
    {
        /* If it is a two-block object, account for base object, and
           for character block if present. */
        size = sizeof(PyUnicodeObject);
        if (_PyUnicode_DATA_ANY(self))
	  size += (PyUnicode_GET_LENGTH(self) + 1);
    }
    return PyLong_FromSsize_t(size);
}

static PyObject *
unicode_getnewargs(PyObject *v, PyObject *Py_UNUSED(ignored))
{
    PyObject *copy = _PyUnicode_Copy(v);
    if (!copy)
        return NULL;
    return Py_BuildValue("(N)", copy);
}

static PyMethodDef unicode_methods[] = {
    UNICODE_REPLACE_METHODDEF
    UNICODE_SPLIT_METHODDEF
    UNICODE_RSPLIT_METHODDEF
    UNICODE_JOIN_METHODDEF
    UNICODE_CAPITALIZE_METHODDEF
    UNICODE_CASEFOLD_METHODDEF
    UNICODE_TITLE_METHODDEF
    UNICODE_CENTER_METHODDEF
    {"count", (PyCFunction) unicode_count, METH_VARARGS, count__doc__},
    UNICODE_EXPANDTABS_METHODDEF
    {"find", (PyCFunction) unicode_find, METH_VARARGS, find__doc__},
    UNICODE_PARTITION_METHODDEF
    {"index", (PyCFunction) unicode_index, METH_VARARGS, index__doc__},
    UNICODE_LJUST_METHODDEF
    UNICODE_LOWER_METHODDEF
    UNICODE_LSTRIP_METHODDEF
    {"rfind", (PyCFunction) unicode_rfind, METH_VARARGS, rfind__doc__},
    {"rindex", (PyCFunction) unicode_rindex, METH_VARARGS, rindex__doc__},
    UNICODE_RJUST_METHODDEF
    UNICODE_RSTRIP_METHODDEF
    UNICODE_RPARTITION_METHODDEF
    UNICODE_SPLITLINES_METHODDEF
    UNICODE_STRIP_METHODDEF
    UNICODE_SWAPCASE_METHODDEF
    UNICODE_UPPER_METHODDEF
    {"startswith", (PyCFunction) unicode_startswith, METH_VARARGS, startswith__doc__},
    {"endswith", (PyCFunction) unicode_endswith, METH_VARARGS, endswith__doc__},
    UNICODE_REMOVEPREFIX_METHODDEF
    UNICODE_REMOVESUFFIX_METHODDEF
    UNICODE_ISASCII_METHODDEF
    UNICODE_ISLOWER_METHODDEF
    UNICODE_ISUPPER_METHODDEF
    UNICODE_ISTITLE_METHODDEF
    UNICODE_ISSPACE_METHODDEF
    UNICODE_ISDECIMAL_METHODDEF
    UNICODE_ISDIGIT_METHODDEF
    UNICODE_ISNUMERIC_METHODDEF
    UNICODE_ISALPHA_METHODDEF
    UNICODE_ISALNUM_METHODDEF
    UNICODE_ISIDENTIFIER_METHODDEF
    UNICODE_ISPRINTABLE_METHODDEF
    UNICODE_ZFILL_METHODDEF
    {"format", (PyCFunction)(void(*)(void)) do_string_format, METH_VARARGS | METH_KEYWORDS, format__doc__},
    {"format_map", (PyCFunction) do_string_format_map, METH_O, format_map__doc__},
    UNICODE___FORMAT___METHODDEF
    UNICODE_SIZEOF_METHODDEF
    {"__getnewargs__",  unicode_getnewargs, METH_NOARGS},
    {NULL, NULL}
};

static PyObject *
unicode_mod(PyObject *v, PyObject *w)
{
    if (!PyUnicode_Check(v))
        Py_RETURN_NOTIMPLEMENTED;
    return PyUnicode_Format(v, w);
}

static PyNumberMethods unicode_as_number = {
    0,              /*nb_add*/
    0,              /*nb_subtract*/
    0,              /*nb_multiply*/
    unicode_mod,            /*nb_remainder*/
};

static PySequenceMethods unicode_as_sequence = {
    (lenfunc) unicode_length,       /* sq_length */
    PyUnicode_Concat,           /* sq_concat */
    (ssizeargfunc) unicode_repeat,  /* sq_repeat */
    (ssizeargfunc) unicode_getitem,     /* sq_item */
    0,                  /* sq_slice */
    0,                  /* sq_ass_item */
    0,                  /* sq_ass_slice */
    PyUnicode_Contains,         /* sq_contains */
};

static PyObject*
unicode_subscript(PyObject* self, PyObject* item)
{

    if (_PyIndex_Check(item)) {
        Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
        if (i == -1 && PyErr_Occurred())
            return NULL;
        if (i < 0)
            i += PyUnicode_GET_LENGTH(self);
        return unicode_getitem(self, i);
    } else if (PySlice_Check(item)) {
        Py_ssize_t start, stop, step, slicelength, i;
        size_t cur;
        PyObject *result;
        const void *src_data;
        void *dest_data;

        if (PySlice_Unpack(item, &start, &stop, &step) < 0) {
            return NULL;
        }
        slicelength = PySlice_AdjustIndices(PyUnicode_GET_LENGTH(self),
                                            &start, &stop, step);

        if (slicelength <= 0) {
            _Py_RETURN_UNICODE_EMPTY();
        } else if (start == 0 && step == 1 &&
                   slicelength == PyUnicode_GET_LENGTH(self)) {
            return unicode_result_unchanged(self);
        } else if (step == 1) {
            return PyUnicode_Substring(self,
                                       start, start + slicelength);
        }
        /* General case */
        src_data = PyUnicode_DATA(self);
        result = PyUnicode_New(slicelength);
        if (result == NULL)
            return NULL;
        dest_data = PyUnicode_DATA(result);

        for (cur = start, i = 0; i < slicelength; cur += step, i++) {
            Py_UCS4 ch = PyUnicode_READ(src_data, cur);
            PyUnicode_WRITE(dest_data, i, ch);
        }
        assert(_PyUnicode_CheckConsistency(result, 1));
        return result;
    } else {
        PyErr_SetString(PyExc_TypeError, "string indices must be integers");
        return NULL;
    }
}

static PyMappingMethods unicode_as_mapping = {
    (lenfunc)unicode_length,        /* mp_length */
    (binaryfunc)unicode_subscript,  /* mp_subscript */
    (objobjargproc)0,           /* mp_ass_subscript */
};


/* Helpers for PyUnicode_Format() */

struct unicode_formatter_t {
    PyObject *args;
    int args_owned;
    Py_ssize_t arglen, argidx;
    PyObject *dict;

    Py_ssize_t fmtcnt, fmtpos;
    const void *fmtdata;
    PyObject *fmtstr;

    _PyUnicodeWriter writer;
};

struct unicode_format_arg_t {
    Py_UCS4 ch;
    int flags;
    Py_ssize_t width;
    int prec;
    int sign;
};

static PyObject *
unicode_format_getnextarg(struct unicode_formatter_t *ctx)
{
    Py_ssize_t argidx = ctx->argidx;

    if (argidx < ctx->arglen) {
        ctx->argidx++;
        if (ctx->arglen < 0)
            return ctx->args;
        else
            return PyTuple_GetItem(ctx->args, argidx);
    }
    PyErr_SetString(PyExc_TypeError,
                    "not enough arguments for format string");
    return NULL;
}

/* Returns a new reference to a PyUnicode object, or NULL on failure. */

/* Format a float into the writer if the writer is not NULL, or into *p_output
   otherwise.

   Return 0 on success, raise an exception and return -1 on error. */
static int
formatfloat(PyObject *v, struct unicode_format_arg_t *arg,
            PyObject **p_output,
            _PyUnicodeWriter *writer)
{
    char *p;
    double x;
    Py_ssize_t len;
    int prec;
    int dtoa_flags;

    x = PyFloat_AsDouble(v);
    if (x == -1.0 && PyErr_Occurred())
        return -1;

    prec = arg->prec;
    if (prec < 0)
        prec = 6;

    if (arg->flags & F_ALT)
        dtoa_flags = Py_DTSF_ALT;
    else
        dtoa_flags = 0;
    p = PyOS_double_to_string(x, arg->ch, prec, dtoa_flags, NULL);
    if (p == NULL)
        return -1;
    len = strlen(p);
    if (writer) {
        if (_PyUnicodeWriter_WriteASCIIString(writer, p, len) < 0) {
            PyMem_Free(p);
            return -1;
        }
    }
    else
        *p_output = PyUnicode_FromStringAndSize(p, len);
    PyMem_Free(p);
    return 0;
}

/* formatlong() emulates the format codes d, u, o, x and X, and
 * the F_ALT flag, for Python's long (unbounded) ints.  It's not used for
 * Python's regular ints.
 * Return value:  a new PyUnicodeObject*, or NULL if error.
 *     The output string is of the form
 *         "-"? ("0x" | "0X")? digit+
 *     "0x"/"0X" are present only for x and X conversions, with F_ALT
 *         set in flags.  The case of hex digits will be correct,
 *     There will be at least prec digits, zero-filled on the left if
 *         necessary to get that many.
 * val          object to be converted
 * flags        bitmask of format flags; only F_ALT is looked at
 * prec         minimum number of digits; 0-fill on left if needed
 * type         a character in [duoxX]; u acts the same as d
 *
 * CAUTION:  o, x and X conversions on regular ints can never
 * produce a '-' sign, but can for Python's unbounded ints.
 */
PyObject *
_PyUnicode_FormatLong(PyObject *val, int alt, int prec, int type)
{
    PyObject *result = NULL;
    char *buf;
    Py_ssize_t i;
    int sign;           /* 1 if '-', else 0 */
    int len;            /* number of characters */
    Py_ssize_t llen;
    int numdigits;      /* len == numnondigits + numdigits */
    int numnondigits = 0;

    /* Avoid exceeding SSIZE_T_MAX */
    if (prec > INT_MAX-3) {
        PyErr_SetString(PyExc_OverflowError,
                        "precision too large");
        return NULL;
    }

    assert(PyLong_Check(val));

    switch (type) {
    default:
        Py_UNREACHABLE();
    case 'd':
    case 'i':
    case 'u':
        /* int and int subclasses should print numerically when a numeric */
        /* format code is used (see issue18780) */
        result = PyNumber_ToBase(val, 10);
        break;
    case 'o':
        numnondigits = 2;
        result = PyNumber_ToBase(val, 8);
        break;
    case 'x':
    case 'X':
        numnondigits = 2;
        result = PyNumber_ToBase(val, 16);
        break;
    }
    if (!result)
        return NULL;

    assert(unicode_modifiable(result));

    /* To modify the string in-place, there can only be one reference. */
    if (Py_REFCNT(result) != 1) {
        Py_DECREF(result);
        PyErr_BadInternalCall();
        return NULL;
    }
    buf = PyUnicode_DATA(result);
    llen = PyUnicode_GET_LENGTH(result);
    if (llen > INT_MAX) {
        Py_DECREF(result);
        PyErr_SetString(PyExc_ValueError,
                        "string too large in _PyUnicode_FormatLong");
        return NULL;
    }
    len = (int)llen;
    sign = buf[0] == '-';
    numnondigits += sign;
    numdigits = len - numnondigits;
    assert(numdigits > 0);

    /* Get rid of base marker unless F_ALT */
    if (((alt) == 0 &&
        (type == 'o' || type == 'x' || type == 'X'))) {
        assert(buf[sign] == '0');
        assert(buf[sign+1] == 'x' || buf[sign+1] == 'X' ||
               buf[sign+1] == 'o');
        numnondigits -= 2;
        buf += 2;
        len -= 2;
        if (sign)
            buf[0] = '-';
        assert(len == numnondigits + numdigits);
        assert(numdigits > 0);
    }

    /* Fill with leading zeroes to meet minimum width. */
    if (prec > numdigits) {
        PyObject *r1 = PyUnicode_FromStringAndSize(NULL,
                                numnondigits + prec);
        char *b1;
        if (!r1) {
            Py_DECREF(result);
            return NULL;
        }
        b1 = (char *) PyUnicode_AsChar(r1);
        for (i = 0; i < numnondigits; ++i)
            *b1++ = *buf++;
        for (i = 0; i < prec - numdigits; i++)
            *b1++ = '0';
        for (i = 0; i < numdigits; i++)
            *b1++ = *buf++;
        *b1 = '\0';
        Py_DECREF(result);
        result = r1;
        buf = (char *) PyUnicode_AsChar(result);
        len = numnondigits + prec;
    }

    /* Fix up case for hex conversions. */
    if (type == 'X') {
        /* Need to convert all lower case letters to upper case.
           and need to convert 0x to 0X (and -0x to -0X). */
        for (i = 0; i < len; i++)
            if (buf[i] >= 'a' && buf[i] <= 'x')
                buf[i] -= 'a'-'A';
    }
    if (!PyUnicode_Check(result)
        || buf != PyUnicode_DATA(result)) {
        PyObject *unicode;
        unicode = PyUnicode_FromStringAndSize(buf, len);
        Py_DECREF(result);
        result = unicode;
    }
    else if (len != PyUnicode_GET_LENGTH(result)) {
        if (PyUnicode_Resize(&result, len) < 0)
            Py_CLEAR(result);
    }
    return result;
}

/* Format an integer or a float as an integer.
 * Return 1 if the number has been formatted into the writer,
 *        0 if the number has been formatted into *p_output
 *       -1 and raise an exception on error */
static int
mainformatlong(PyObject *v,
               struct unicode_format_arg_t *arg,
               PyObject **p_output,
               _PyUnicodeWriter *writer)
{
    PyObject *iobj, *res;
    char type = (char)arg->ch;

    if (!PyNumber_Check(v))
        goto wrongtype;

    /* make sure number is a type of integer for o, x, and X */
    if (!PyLong_Check(v)) {
        if (type == 'o' || type == 'x' || type == 'X') {
            iobj = _PyNumber_Index(v);
            if (iobj == NULL) {
                if (PyErr_ExceptionMatches(PyExc_TypeError))
                    goto wrongtype;
                return -1;
            }
        }
        else {
            iobj = PyNumber_Long(v);
            if (iobj == NULL ) {
                if (PyErr_ExceptionMatches(PyExc_TypeError))
                    goto wrongtype;
                return -1;
            }
        }
        assert(PyLong_Check(iobj));
    }
    else {
        iobj = v;
        Py_INCREF(iobj);
    }

    if (PyLong_CheckExact(v)
        && arg->width == -1 && arg->prec == -1
        && !(arg->flags & (F_SIGN | F_BLANK))
        && type != 'X')
    {
        /* Fast path */
        int alternate = arg->flags & F_ALT;
        int base;

        switch(type)
        {
            default:
                Py_UNREACHABLE();
            case 'd':
            case 'i':
            case 'u':
                base = 10;
                break;
            case 'o':
                base = 8;
                break;
            case 'x':
            case 'X':
                base = 16;
                break;
        }

        if (_PyLong_FormatWriter(writer, v, base, alternate) == -1) {
            Py_DECREF(iobj);
            return -1;
        }
        Py_DECREF(iobj);
        return 1;
    }

    res = _PyUnicode_FormatLong(iobj, arg->flags & F_ALT, arg->prec, type);
    Py_DECREF(iobj);
    if (res == NULL)
        return -1;
    *p_output = res;
    return 0;

wrongtype:
    switch(type)
    {
        case 'o':
        case 'x':
        case 'X':
            PyErr_Format(PyExc_TypeError,
                    "%%%c format: an integer is required, "
                    "not %.200s",
                    type, Py_TYPE(v)->tp_name);
            break;
        default:
            PyErr_Format(PyExc_TypeError,
                    "%%%c format: a number is required, "
                    "not %.200s",
                    type, Py_TYPE(v)->tp_name);
            break;
    }
    return -1;
}

static Py_UCS4
formatchar(PyObject *v)
{
    /* presume that the buffer is at least 3 characters long */
    if (PyUnicode_Check(v)) {
        if (PyUnicode_GET_LENGTH(v) == 1) {
            return PyUnicode_READ_CHAR(v, 0);
        }
        goto onError;
    }
    else {
        PyObject *iobj;
        long x;
        /* make sure number is a type of integer */
        if (!PyLong_Check(v)) {
            iobj = PyNumber_Index(v);
            if (iobj == NULL) {
                goto onError;
            }
            x = PyLong_AsLong(iobj);
            Py_DECREF(iobj);
        }
        else {
            x = PyLong_AsLong(v);
        }
        if (x == -1 && PyErr_Occurred())
            goto onError;

        if (x < 0 || x > 0xff) {
            PyErr_SetString(PyExc_OverflowError,
                            "%c arg not in range(0x100)");
            return (Py_UCS4) -1;
        }

        return (Py_UCS4) x;
    }

  onError:
    PyErr_SetString(PyExc_TypeError,
                    "%c requires int or char");
    return (Py_UCS4) -1;
}

/* Parse options of an argument: flags, width, precision.
   Handle also "%(name)" syntax.

   Return 0 if the argument has been formatted into arg->str.
   Return 1 if the argument has been written into ctx->writer,
   Raise an exception and return -1 on error. */
static int
unicode_format_arg_parse(struct unicode_formatter_t *ctx,
                         struct unicode_format_arg_t *arg)
{
#define FORMAT_READ(ctx) \
        PyUnicode_READ((ctx)->fmtdata, (ctx)->fmtpos)

    PyObject *v;

    if (arg->ch == '(') {
        /* Get argument value from a dictionary. Example: "%(name)s". */
        Py_ssize_t keystart;
        Py_ssize_t keylen;
        PyObject *key;
        int pcount = 1;

        if (ctx->dict == NULL) {
            PyErr_SetString(PyExc_TypeError,
                            "format requires a mapping");
            return -1;
        }
        ++ctx->fmtpos;
        --ctx->fmtcnt;
        keystart = ctx->fmtpos;
        /* Skip over balanced parentheses */
        while (pcount > 0 && --ctx->fmtcnt >= 0) {
            arg->ch = FORMAT_READ(ctx);
            if (arg->ch == ')')
                --pcount;
            else if (arg->ch == '(')
                ++pcount;
            ctx->fmtpos++;
        }
        keylen = ctx->fmtpos - keystart - 1;
        if (ctx->fmtcnt < 0 || pcount > 0) {
            PyErr_SetString(PyExc_ValueError,
                            "incomplete format key");
            return -1;
        }
        key = PyUnicode_Substring(ctx->fmtstr,
                                  keystart, keystart + keylen);
        if (key == NULL)
            return -1;
        if (ctx->args_owned) {
            ctx->args_owned = 0;
            Py_DECREF(ctx->args);
        }
        ctx->args = PyObject_GetItem(ctx->dict, key);
        Py_DECREF(key);
        if (ctx->args == NULL)
            return -1;
        ctx->args_owned = 1;
        ctx->arglen = -1;
        ctx->argidx = -2;
    }

    /* Parse flags. Example: "%+i" => flags=F_SIGN. */
    while (--ctx->fmtcnt >= 0) {
        arg->ch = FORMAT_READ(ctx);
        ctx->fmtpos++;
        switch (arg->ch) {
        case '-': arg->flags |= F_LJUST; continue;
        case '+': arg->flags |= F_SIGN; continue;
        case ' ': arg->flags |= F_BLANK; continue;
        case '#': arg->flags |= F_ALT; continue;
        case '0': arg->flags |= F_ZERO; continue;
        }
        break;
    }

    /* Parse width. Example: "%10s" => width=10 */
    if (arg->ch == '*') {
        v = unicode_format_getnextarg(ctx);
        if (v == NULL)
            return -1;
        if (!PyLong_Check(v)) {
            PyErr_SetString(PyExc_TypeError,
                            "* wants int");
            return -1;
        }
        arg->width = PyLong_AsSsize_t(v);
        if (arg->width == -1 && PyErr_Occurred())
            return -1;
        if (arg->width < 0) {
            arg->flags |= F_LJUST;
            arg->width = -arg->width;
        }
        if (--ctx->fmtcnt >= 0) {
            arg->ch = FORMAT_READ(ctx);
            ctx->fmtpos++;
        }
    }
    else if (arg->ch >= '0' && arg->ch <= '9') {
        arg->width = arg->ch - '0';
        while (--ctx->fmtcnt >= 0) {
            arg->ch = FORMAT_READ(ctx);
            ctx->fmtpos++;
            if (arg->ch < '0' || arg->ch > '9')
                break;
            /* Since arg->ch is unsigned, the RHS would end up as unsigned,
               mixing signed and unsigned comparison. Since arg->ch is between
               '0' and '9', casting to int is safe. */
            if (arg->width > (PY_SSIZE_T_MAX - ((int)arg->ch - '0')) / 10) {
                PyErr_SetString(PyExc_ValueError,
                                "width too big");
                return -1;
            }
            arg->width = arg->width*10 + (arg->ch - '0');
        }
    }

    /* Parse precision. Example: "%.3f" => prec=3 */
    if (arg->ch == '.') {
        arg->prec = 0;
        if (--ctx->fmtcnt >= 0) {
            arg->ch = FORMAT_READ(ctx);
            ctx->fmtpos++;
        }
        if (arg->ch == '*') {
            v = unicode_format_getnextarg(ctx);
            if (v == NULL)
                return -1;
            if (!PyLong_Check(v)) {
                PyErr_SetString(PyExc_TypeError,
                                "* wants int");
                return -1;
            }
            arg->prec = _PyLong_AsInt(v);
            if (arg->prec == -1 && PyErr_Occurred())
                return -1;
            if (arg->prec < 0)
                arg->prec = 0;
            if (--ctx->fmtcnt >= 0) {
                arg->ch = FORMAT_READ(ctx);
                ctx->fmtpos++;
            }
        }
        else if (arg->ch >= '0' && arg->ch <= '9') {
            arg->prec = arg->ch - '0';
            while (--ctx->fmtcnt >= 0) {
                arg->ch = FORMAT_READ(ctx);
                ctx->fmtpos++;
                if (arg->ch < '0' || arg->ch > '9')
                    break;
                if (arg->prec > (INT_MAX - ((int)arg->ch - '0')) / 10) {
                    PyErr_SetString(PyExc_ValueError,
                                    "precision too big");
                    return -1;
                }
                arg->prec = arg->prec*10 + (arg->ch - '0');
            }
        }
    }

    /* Ignore "h", "l" and "L" format prefix (ex: "%hi" or "%ls") */
    if (ctx->fmtcnt >= 0) {
        if (arg->ch == 'h' || arg->ch == 'l' || arg->ch == 'L') {
            if (--ctx->fmtcnt >= 0) {
                arg->ch = FORMAT_READ(ctx);
                ctx->fmtpos++;
            }
        }
    }
    if (ctx->fmtcnt < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "incomplete format");
        return -1;
    }
    return 0;

#undef FORMAT_READ
}

/* Format one argument. Supported conversion specifiers:

   - "s", "r", "a": any type
   - "i", "d", "u": int or float
   - "o", "x", "X": int
   - "e", "E", "f", "F", "g", "G": float
   - "c": int or str (1 character)

   When possible, the output is written directly into the Unicode writer
   (ctx->writer). A string is created when padding is required.

   Return 0 if the argument has been formatted into *p_str,
          1 if the argument has been written into ctx->writer,
         -1 on error. */
static int
unicode_format_arg_format(struct unicode_formatter_t *ctx,
                          struct unicode_format_arg_t *arg,
                          PyObject **p_str)
{
    PyObject *v;
    _PyUnicodeWriter *writer = &ctx->writer;

    if (ctx->fmtcnt == 0)
        ctx->writer.overallocate = 0;

    v = unicode_format_getnextarg(ctx);
    if (v == NULL)
        return -1;


    switch (arg->ch) {
    case 's':
    case 'r':
    case 'a':
        if (PyLong_CheckExact(v) && arg->width == -1 && arg->prec == -1) {
            /* Fast path */
            if (_PyLong_FormatWriter(writer, v, 10, arg->flags & F_ALT) == -1)
                return -1;
            return 1;
        }

        if (PyUnicode_CheckExact(v) && arg->ch == 's') {
            *p_str = v;
            Py_INCREF(*p_str);
        }
        else {
            if (arg->ch == 's')
                *p_str = PyObject_Str(v);
            else if (arg->ch == 'r')
                *p_str = PyObject_Repr(v);
            else
                *p_str = PyObject_ASCII(v);
        }
        break;

    case 'i':
    case 'd':
    case 'u':
    case 'o':
    case 'x':
    case 'X':
    {
        int ret = mainformatlong(v, arg, p_str, writer);
        if (ret != 0)
            return ret;
        arg->sign = 1;
        break;
    }

    case 'e':
    case 'E':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
        if (arg->width == -1 && arg->prec == -1
            && !(arg->flags & (F_SIGN | F_BLANK)))
        {
            /* Fast path */
            if (formatfloat(v, arg, NULL, writer) == -1)
                return -1;
            return 1;
        }

        arg->sign = 1;
        if (formatfloat(v, arg, p_str, NULL) == -1)
            return -1;
        break;

    case 'c':
    {
        Py_UCS4 ch = formatchar(v);
        if (ch == (Py_UCS4) -1)
            return -1;
        if (arg->width == -1 && arg->prec == -1) {
            /* Fast path */
            if (_PyUnicodeWriter_WriteCharInline(writer, ch) < 0)
                return -1;
            return 1;
        }
        *p_str = PyUnicode_FromOrdinal(ch);
        break;
    }

    default:
        PyErr_Format(PyExc_ValueError,
                     "unsupported format character '%c' (0x%x) "
                     "at index %zd",
                     (31<=arg->ch && arg->ch<=126) ? (char)arg->ch : '?',
                     (int)arg->ch,
                     ctx->fmtpos - 1);
        return -1;
    }
    if (*p_str == NULL)
        return -1;
    assert (PyUnicode_Check(*p_str));
    return 0;
}

static int
unicode_format_arg_output(struct unicode_formatter_t *ctx,
                          struct unicode_format_arg_t *arg,
                          PyObject *str)
{
    Py_ssize_t len;
    const void *pbuf;
    Py_ssize_t pindex;
    Py_UCS4 signchar;
    Py_ssize_t buflen;
    Py_ssize_t sublen;
    _PyUnicodeWriter *writer = &ctx->writer;
    Py_UCS4 fill;

    fill = ' ';
    if (arg->sign && arg->flags & F_ZERO)
        fill = '0';

    len = PyUnicode_GET_LENGTH(str);
    if ((arg->width == -1 || arg->width <= len)
        && (arg->prec == -1 || arg->prec >= len)
        && !(arg->flags & (F_SIGN | F_BLANK)))
    {
        /* Fast path */
        if (_PyUnicodeWriter_WriteStr(writer, str) == -1)
            return -1;
        return 0;
    }

    /* Truncate the string for "s", "r" and "a" formats
       if the precision is set */
    if (arg->ch == 's' || arg->ch == 'r' || arg->ch == 'a') {
        if (arg->prec >= 0 && len > arg->prec)
            len = arg->prec;
    }

    /* Adjust sign and width */
    pbuf = PyUnicode_DATA(str);
    pindex = 0;
    signchar = '\0';
    if (arg->sign) {
        Py_UCS4 ch = PyUnicode_READ( pbuf, pindex);
        if (ch == '-' || ch == '+') {
            signchar = ch;
            len--;
            pindex++;
        }
        else if (arg->flags & F_SIGN)
            signchar = '+';
        else if (arg->flags & F_BLANK)
            signchar = ' ';
        else
            arg->sign = 0;
    }
    if (arg->width < len)
        arg->width = len;

    /* Prepare the writer */
    buflen = arg->width;
    if (arg->sign && len == arg->width)
        buflen++;
    if (_PyUnicodeWriter_Prepare(writer, buflen) == -1)
        return -1;

    /* Write the sign if needed */
    if (arg->sign) {
        if (fill != ' ') {
            PyUnicode_WRITE(writer->data, writer->pos, signchar);
            writer->pos += 1;
        }
        if (arg->width > len)
            arg->width--;
    }

    /* Write the numeric prefix for "x", "X" and "o" formats
       if the alternate form is used.
       For example, write "0x" for the "%#x" format. */
    if ((arg->flags & F_ALT) && (arg->ch == 'x' || arg->ch == 'X' || arg->ch == 'o')) {
        if (fill != ' ') {
            PyUnicode_WRITE(writer->data, writer->pos, '0');
            PyUnicode_WRITE(writer->data, writer->pos+1, arg->ch);
            writer->pos += 2;
            pindex += 2;
        }
        arg->width -= 2;
        if (arg->width < 0)
            arg->width = 0;
        len -= 2;
    }

    /* Pad left with the fill character if needed */
    if (arg->width > len && !(arg->flags & F_LJUST)) {
        sublen = arg->width - len;
        unicode_fill(writer->data, fill, writer->pos, sublen);
        writer->pos += sublen;
        arg->width = len;
    }

    /* If padding with spaces: write sign if needed and/or numeric prefix if
       the alternate form is used */
    if (fill == ' ') {
        if (arg->sign) {
            PyUnicode_WRITE(writer->data, writer->pos, signchar);
            writer->pos += 1;
        }
        if ((arg->flags & F_ALT) && (arg->ch == 'x' || arg->ch == 'X' || arg->ch == 'o')) {
            assert(PyUnicode_READ( pbuf, pindex) == '0');
            assert(PyUnicode_READ( pbuf, pindex+1) == arg->ch);
            PyUnicode_WRITE(writer->data, writer->pos, '0');
            PyUnicode_WRITE(writer->data, writer->pos+1, arg->ch);
            writer->pos += 2;
            pindex += 2;
        }
    }

    /* Write characters */
    if (len) {
        _PyUnicode_FastCopyCharacters(writer->buffer, writer->pos,
                                      str, pindex, len);
        writer->pos += len;
    }

    /* Pad right with the fill character if needed */
    if (arg->width > len) {
        sublen = arg->width - len;
        unicode_fill(writer->data, ' ', writer->pos, sublen);
        writer->pos += sublen;
    }
    return 0;
}

/* Helper of PyUnicode_Format(): format one arg.
   Return 0 on success, raise an exception and return -1 on error. */
static int
unicode_format_arg(struct unicode_formatter_t *ctx)
{
    struct unicode_format_arg_t arg;
    PyObject *str;
    int ret;

    arg.ch = PyUnicode_READ(ctx->fmtdata, ctx->fmtpos);
    if (arg.ch == '%') {
        ctx->fmtpos++;
        ctx->fmtcnt--;
        if (_PyUnicodeWriter_WriteCharInline(&ctx->writer, '%') < 0)
            return -1;
        return 0;
    }
    arg.flags = 0;
    arg.width = -1;
    arg.prec = -1;
    arg.sign = 0;
    str = NULL;

    ret = unicode_format_arg_parse(ctx, &arg);
    if (ret == -1)
        return -1;

    ret = unicode_format_arg_format(ctx, &arg, &str);
    if (ret == -1)
        return -1;

    if (ret != 1) {
        ret = unicode_format_arg_output(ctx, &arg, str);
        Py_DECREF(str);
        if (ret == -1)
            return -1;
    }

    if (ctx->dict && (ctx->argidx < ctx->arglen)) {
        PyErr_SetString(PyExc_TypeError,
                        "not all arguments converted during string formatting");
        return -1;
    }
    return 0;
}

PyObject *
PyUnicode_Format(PyObject *format, PyObject *args)
{
    struct unicode_formatter_t ctx;

    if (format == NULL || args == NULL) {
        PyErr_BadInternalCall();
        return NULL;
    }

    if (ensure_unicode(format) < 0)
        return NULL;

    ctx.fmtstr = format;
    ctx.fmtdata = PyUnicode_DATA(ctx.fmtstr);
    ctx.fmtcnt = PyUnicode_GET_LENGTH(ctx.fmtstr);
    ctx.fmtpos = 0;

    _PyUnicodeWriter_Init(&ctx.writer);
    ctx.writer.min_length = ctx.fmtcnt + 100;
    ctx.writer.overallocate = 1;

    if (PyTuple_Check(args)) {
        ctx.arglen = PyTuple_Size(args);
        ctx.argidx = 0;
    }
    else {
        ctx.arglen = -1;
        ctx.argidx = -2;
    }
    ctx.args_owned = 0;
    if (PyMapping_Check(args) && !PyTuple_Check(args) && !PyUnicode_Check(args))
        ctx.dict = args;
    else
        ctx.dict = NULL;
    ctx.args = args;

    while (--ctx.fmtcnt >= 0) {
        if (PyUnicode_READ(ctx.fmtdata, ctx.fmtpos) != '%') {
            Py_ssize_t nonfmtpos;

            nonfmtpos = ctx.fmtpos++;
            while (ctx.fmtcnt >= 0 &&
                   PyUnicode_READ(ctx.fmtdata, ctx.fmtpos) != '%') {
                ctx.fmtpos++;
                ctx.fmtcnt--;
            }
            if (ctx.fmtcnt < 0) {
                ctx.fmtpos--;
                ctx.writer.overallocate = 0;
            }

            if (_PyUnicodeWriter_WriteSubstring(&ctx.writer, ctx.fmtstr,
                                                nonfmtpos, ctx.fmtpos) < 0)
                goto onError;
        }
        else {
            ctx.fmtpos++;
            if (unicode_format_arg(&ctx) == -1)
                goto onError;
        }
    }

    if (ctx.argidx < ctx.arglen && !ctx.dict) {
        PyErr_SetString(PyExc_TypeError,
                        "not all arguments converted during string formatting");
        goto onError;
    }

    if (ctx.args_owned) {
        Py_DECREF(ctx.args);
    }
    return _PyUnicodeWriter_Finish(&ctx.writer);

  onError:
    _PyUnicodeWriter_Dealloc(&ctx.writer);
    if (ctx.args_owned) {
        Py_DECREF(ctx.args);
    }
    return NULL;
}

static PyObject *
unicode_subtype_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

static PyObject *
unicode_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *x = NULL;
    static char *kwlist[] = {"object", "encoding", "errors", 0};
    char *encoding = NULL;
    char *errors = NULL;

    if (type != &PyUnicode_Type)
        return unicode_subtype_new(type, args, kwds);
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Oss:str",
                                     kwlist, &x, &encoding, &errors))
        return NULL;
    if (x == NULL)
        _Py_RETURN_UNICODE_EMPTY();
        return PyObject_Str(x);
}

static PyObject *
unicode_subtype_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *unicode, *self;
    Py_ssize_t length;
    void *data;

    assert(PyType_IsSubtype(type, &PyUnicode_Type));

    unicode = unicode_new(&PyUnicode_Type, args, kwds);
    if (unicode == NULL)
        return NULL;
    assert(_PyUnicode_CHECK(unicode));
    self = type->tp_alloc(type, 0);
    if (self == NULL) {
        Py_DECREF(unicode);
        return NULL;
    }
    length = PyUnicode_GET_LENGTH(unicode);

    _PyUnicode_LENGTH(self) = length;
    _PyUnicode_HASH(self) = _PyUnicode_HASH(unicode);
    _PyUnicode_STATE(self).interned = 0;
    _PyUnicode_DATA_ANY(self) = NULL;

    /* Ensure we won't overflow the length. */
    if (length > (PY_SSIZE_T_MAX - 1)) {
        PyErr_NoMemory();
        goto onError;
    }
    data = PyMem_Malloc((length + 1));
    if (data == NULL) {
        PyErr_NoMemory();
        goto onError;
    }

    _PyUnicode_DATA_ANY(self) = data;
    
    memcpy(data, PyUnicode_DATA(unicode), (length + 1));
    assert(_PyUnicode_CheckConsistency(self, 1));
    Py_DECREF(unicode);
    return self;

onError:
    Py_DECREF(unicode);
    Py_DECREF(self);
    return NULL;
}

PyDoc_STRVAR(unicode_doc,
"str(object='') -> str\n\
str(bytes_or_buffer[, encoding[, errors]]) -> str\n\
\n\
Create a new string object from the given object. If encoding or\n\
errors is specified, then the object must expose a data buffer\n\
that will be decoded using the given encoding and error handler.\n\
Otherwise, returns the result of object.__str__() (if defined)\n\
or repr(object).\n\
encoding defaults to sys.getdefaultencoding().\n\
errors defaults to 'strict'.");

static PyObject *unicode_iter(PyObject *seq);

PyTypeObject PyUnicode_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "str",                        /* tp_name */
    sizeof(PyUnicodeObject),      /* tp_basicsize */
    0,                            /* tp_itemsize */
    /* Slots */
    (destructor)unicode_dealloc,  /* tp_dealloc */
    0,                            /* tp_vectorcall_offset */
    0,                            /* tp_getattr */
    0,                            /* tp_setattr */
    0,                            /* tp_as_async */
    unicode_repr,                 /* tp_repr */
    &unicode_as_number,           /* tp_as_number */
    &unicode_as_sequence,         /* tp_as_sequence */
    &unicode_as_mapping,          /* tp_as_mapping */
    (hashfunc) unicode_hash,      /* tp_hash*/
    0,                            /* tp_call*/
    (reprfunc) unicode_str,       /* tp_str */
    PyObject_GenericGetAttr,      /* tp_getattro */
    0,                            /* tp_setattro */
    0,                            /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE |
    Py_TPFLAGS_UNICODE_SUBCLASS,   /* tp_flags */
    unicode_doc,                  /* tp_doc */
    0,                            /* tp_traverse */
    0,                            /* tp_clear */
    PyUnicode_RichCompare,        /* tp_richcompare */
    0,                            /* tp_weaklistoffset */
    unicode_iter,                 /* tp_iter */
    0,                            /* tp_iternext */
    unicode_methods,              /* tp_methods */
    0,                            /* tp_members */
    0,                            /* tp_getset */
    &PyBaseObject_Type,           /* tp_base */
    0,                            /* tp_dict */
    0,                            /* tp_descr_get */
    0,                            /* tp_descr_set */
    0,                            /* tp_dictoffset */
    0,                            /* tp_init */
    0,                            /* tp_alloc */
    unicode_new,                  /* tp_new */
    PyMem_Free,                 /* tp_free */
};

/* Initialize the Unicode implementation */

PyStatus
_PyUnicode_Init(void)
{

    /* Init the implementation */
    _Py_INCREF_UNICODE_EMPTY();
    if (!unicode_empty) {
        return _PyStatus_ERR("Can't create empty string");
    }
    Py_DECREF(unicode_empty);

    if (PyType_Ready(&PyUnicode_Type) < 0) {
        return _PyStatus_ERR("Can't initialize unicode type");
    }

    /* initialize the linebreak bloom filter */
    if (PyType_Ready(&PyFieldNameIter_Type) < 0) {
        return _PyStatus_ERR("Can't initialize field name iterator type");
    }
    if (PyType_Ready(&PyFormatterIter_Type) < 0) {
        return _PyStatus_ERR("Can't initialize formatter iter type");
    }
    return _PyStatus_OK();
}


void
PyUnicode_InternInPlace(PyObject **p)
{
    PyObject *s = *p;
    if (s == NULL || !PyUnicode_Check(s)) {
        return;
    }

    /* If it's a subclass, we don't really know what putting
       it in the interned dict might do. */
    if (!PyUnicode_CheckExact(s)) {
        return;
    }

    if (PyUnicode_CHECK_INTERNED(s)) {
        return;
    }

#ifdef INTERNED_STRINGS
    if (interned == NULL) {
        interned = PyDict_New();
        if (interned == NULL) {
            PyErr_Clear(); /* Don't leave an exception */
            return;
        }
    }

    PyObject *t;
    t = PyDict_SetDefault(interned, s, s);

    if (t == NULL) {
        PyErr_Clear();
        return;
    }

    if (t != s) {
        Py_INCREF(t);
        Py_SETREF(*p, t);
        return;
    }

    /* The two references in interned are not counted by refcnt.
       The deallocator will take care of this */
    Py_SET_REFCNT(s, Py_REFCNT(s) - 2);
    _PyUnicode_STATE(s).interned = SSTATE_INTERNED_MORTAL;
#endif
}

void
PyUnicode_InternImmortal(PyObject **p)
{
    PyUnicode_InternInPlace(p);
    if (PyUnicode_CHECK_INTERNED(*p) != SSTATE_INTERNED_IMMORTAL) {
        _PyUnicode_STATE(*p).interned = SSTATE_INTERNED_IMMORTAL;
        Py_INCREF(*p);
    }
}

PyObject *
PyUnicode_InternFromString(const char *cp)
{
    PyObject *s = PyUnicode_FromString(cp);
    if (s == NULL)
        return NULL;
    PyUnicode_InternInPlace(&s);
    return s;
}

/********************* Unicode Iterator **************************/

typedef struct {
    PyObject_HEAD
    Py_ssize_t it_index;
    PyObject *it_seq;    /* Set to NULL when iterator is exhausted */
} unicodeiterobject;

static void
unicodeiter_dealloc(unicodeiterobject *it)
{
    Py_XDECREF(it->it_seq);
    PyMem_Free(it);
}

static PyObject *
unicodeiter_next(unicodeiterobject *it)
{
    PyObject *seq, *item;

    assert(it != NULL);
    seq = it->it_seq;
    if (seq == NULL)
        return NULL;
    assert(_PyUnicode_CHECK(seq));

    if (it->it_index < PyUnicode_GET_LENGTH(seq)) {
        const void *data = PyUnicode_DATA(seq);
        Py_UCS4 chr = PyUnicode_READ(data, it->it_index);
        item = PyUnicode_FromOrdinal(chr);
        if (item != NULL)
            ++it->it_index;
        return item;
    }

    it->it_seq = NULL;
    Py_DECREF(seq);
    return NULL;
}

static PyObject *
unicodeiter_len(unicodeiterobject *it, PyObject *Py_UNUSED(ignored))
{
    Py_ssize_t len = 0;
    if (it->it_seq)
        len = PyUnicode_GET_LENGTH(it->it_seq) - it->it_index;
    return PyLong_FromSsize_t(len);
}

PyDoc_STRVAR(length_hint_doc, "Private method returning an estimate of len(list(it)).");

static PyObject *
unicodeiter_reduce(unicodeiterobject *it, PyObject *Py_UNUSED(ignored))
{
    _Py_IDENTIFIER(iter);
    if (it->it_seq != NULL) {
        return Py_BuildValue("N(O)n", _PyEval_GetBuiltinId(&PyId_iter),
                             it->it_seq, it->it_index);
    } else {
        PyObject *u = (PyObject *)_PyUnicode_New(0);
        if (u == NULL)
            return NULL;
        return Py_BuildValue("N(N)", _PyEval_GetBuiltinId(&PyId_iter), u);
    }
}

PyDoc_STRVAR(reduce_doc, "Return state information for pickling.");

static PyObject *
unicodeiter_setstate(unicodeiterobject *it, PyObject *state)
{
    Py_ssize_t index = PyLong_AsSsize_t(state);
    if (index == -1 && PyErr_Occurred())
        return NULL;
    if (it->it_seq != NULL) {
        if (index < 0)
            index = 0;
        else if (index > PyUnicode_GET_LENGTH(it->it_seq))
            index = PyUnicode_GET_LENGTH(it->it_seq); /* iterator truncated */
        it->it_index = index;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(setstate_doc, "Set state information for unpickling.");

static PyMethodDef unicodeiter_methods[] = {
    {"__length_hint__", (PyCFunction)unicodeiter_len, METH_NOARGS,
     length_hint_doc},
    {"__reduce__",      (PyCFunction)unicodeiter_reduce, METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)unicodeiter_setstate, METH_O,
     setstate_doc},
    {NULL,      NULL}       /* sentinel */
};

PyTypeObject PyUnicodeIter_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "str_iterator",         /* tp_name */
    sizeof(unicodeiterobject),      /* tp_basicsize */
    0,                  /* tp_itemsize */
    /* methods */
    (destructor)unicodeiter_dealloc,    /* tp_dealloc */
    0,                  /* tp_vectorcall_offset */
    0,                  /* tp_getattr */
    0,                  /* tp_setattr */
    0,                  /* tp_as_async */
    0,                  /* tp_repr */
    0,                  /* tp_as_number */
    0,                  /* tp_as_sequence */
    0,                  /* tp_as_mapping */
    0,                  /* tp_hash */
    0,                  /* tp_call */
    0,                  /* tp_str */
    PyObject_GenericGetAttr,        /* tp_getattro */
    0,                  /* tp_setattro */
    0,                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT, // | Py_TPFLAGS_HAVE_GC,/* tp_flags */
    0,                  /* tp_doc */
    0, // (traverseproc)unicodeiter_traverse, /* tp_traverse */
    0,                  /* tp_clear */
    0,                  /* tp_richcompare */
    0,                  /* tp_weaklistoffset */
    PyObject_SelfIter,          /* tp_iter */
    (iternextfunc)unicodeiter_next,     /* tp_iternext */
    unicodeiter_methods,            /* tp_methods */
    0,
};

static PyObject *
unicode_iter(PyObject *seq)
{
    unicodeiterobject *it;

    if (!PyUnicode_Check(seq)) {
        PyErr_BadInternalCall();
        return NULL;
    }
    it = PyObject_New(unicodeiterobject, &PyUnicodeIter_Type);
    if (it == NULL)
        return NULL;
    it->it_index = 0;
    Py_INCREF(seq);
    it->it_seq = seq;
    return (PyObject *)it;
}

PyStatus
_PyUnicode_InitEncodings(PyThreadState *tstate)
{
    return _PyStatus_OK();
}

void
_PyUnicode_Fini(PyThreadState *tstate)
{
  if (1) {
        Py_CLEAR(unicode_empty);

#ifdef LATIN1_SINGLETONS
        for (Py_ssize_t i = 0; i < 256; i++) {
            Py_CLEAR(unicode_latin1[i]);
        }
#endif
        unicode_clear_static_strings();
    }
}


/* A _string module, to export formatter_parser and formatter_field_name_split
   to the string.Formatter class implemented in Python. */

static PyMethodDef _string_methods[] = {
    {"formatter_field_name_split", (PyCFunction) formatter_field_name_split,
     METH_O, PyDoc_STR("split the argument as a field name")},
    {"formatter_parser", (PyCFunction) formatter_parser,
     METH_O, PyDoc_STR("parse the argument as a format string")},
    {NULL, NULL}
};

static struct PyModuleDef _string_module = {
    PyModuleDef_HEAD_INIT,
    "_string",
    PyDoc_STR("string helper module"),
    0,
    _string_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
PyInit__string(void)
{
    return PyModule_Create(&_string_module);
}


/* Unescape a backslash-escaped string. */
PyObject *_PyBytes_DecodeEscape(const char *s,
                                Py_ssize_t len,
                                const char *errors,
                                const char **first_invalid_escape)
{
    int c;
    char *p;
    const char *end;
    PyObject *temp;
    temp = PyUnicode_New(len);
    p = (char *) PyUnicode_AsChar(temp);

    *first_invalid_escape = NULL;

    end = s + len;
    while (s < end) {
        if (*s != '\\') {
            *p++ = *s++;
            continue;
        }

        s++;
        if (s == end) {
            PyErr_SetString(PyExc_ValueError,
                            "Trailing \\ in string");
            goto failed;
        }

        switch (*s++) {
        /* XXX This assumes ASCII! */
        case '\n': break;
        case '\\': *p++ = '\\'; break;
        case '\'': *p++ = '\''; break;
        case '\"': *p++ = '\"'; break;
        case 'b': *p++ = '\b'; break;
        case 'f': *p++ = '\014'; break; /* FF */
        case 't': *p++ = '\t'; break;
        case 'n': *p++ = '\n'; break;
        case 'r': *p++ = '\r'; break;
        case 'v': *p++ = '\013'; break; /* VT */
        case 'a': *p++ = '\007'; break; /* BEL, not classic C */
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
            c = s[-1] - '0';
            if (s < end && '0' <= *s && *s <= '7') {
                c = (c<<3) + *s++ - '0';
                if (s < end && '0' <= *s && *s <= '7')
                    c = (c<<3) + *s++ - '0';
            }
            *p++ = c;
            break;
        case 'x':
            if (s+1 < end) {
                int digit1, digit2;
                digit1 = _PyLong_DigitValue[Py_CHARMASK(s[0])];
                digit2 = _PyLong_DigitValue[Py_CHARMASK(s[1])];
                if (digit1 < 16 && digit2 < 16) {
                    *p++ = (unsigned char)((digit1 << 4) + digit2);
                    s += 2;
                    break;
                }
            }
            /* invalid hexadecimal digits */

            if (!errors || strcmp(errors, "strict") == 0) {
                PyErr_Format(PyExc_ValueError,
                             "invalid \\x escape at position %zd",
                             s - 2 - (end - len));
                goto failed;
            }
            if (strcmp(errors, "replace") == 0) {
                *p++ = '?';
            } else if (strcmp(errors, "ignore") == 0)
                /* do nothing */;
            else {
                PyErr_Format(PyExc_ValueError,
                             "decoding error; unknown "
                             "error handling code: %.400s",
                             errors);
                goto failed;
            }
            /* skip \x */
            if (s < end && Py_ISXDIGIT(s[0]))
                s++; /* and a hexdigit */
            break;

        default:
            if (*first_invalid_escape == NULL) {
                *first_invalid_escape = s-1; /* Back up one char, since we've
                                                already incremented s. */
            }
            *p++ = '\\';
            s--;
        }
    }
    PyUnicode_Resize(&temp, (p - PyUnicode_AsChar(temp)));
    return temp;

  failed:
    Py_DECREF(temp);
    return NULL;
}

PyObject *PyBytes_DecodeEscape(const char *s,
                                Py_ssize_t len,
                                const char *errors,
                                Py_ssize_t Py_UNUSED(unicode),
                                const char *Py_UNUSED(recode_encoding))
{
    const char* first_invalid_escape;
    PyObject *result = _PyBytes_DecodeEscape(s, len, errors,
                                             &first_invalid_escape);
    if (result == NULL)
        return NULL;
    return result;

}

#ifdef __cplusplus
}
#endif
