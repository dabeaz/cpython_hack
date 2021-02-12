#ifndef Py_UNICODEOBJECT_H
#define Py_UNICODEOBJECT_H

#include <stdarg.h>

/*

Unicode implementation based on original code by Fredrik Lundh,
modified by Marc-Andre Lemburg (mal@lemburg.com) according to the
Unicode Integration Proposal. (See
http://www.egenix.com/files/python/unicode-proposal.txt).

Copyright (c) Corporation for National Research Initiatives.


 Original header:
 --------------------------------------------------------------------

 * Yet another Unicode string type for Python.  This type supports the
 * 16-bit Basic Multilingual Plane (BMP) only.
 *
 * Written by Fredrik Lundh, January 1999.
 *
 * Copyright (c) 1999 by Secret Labs AB.
 * Copyright (c) 1999 by Fredrik Lundh.
 *
 * fredrik@pythonware.com
 * http://www.pythonware.com
 *
 * --------------------------------------------------------------------
 * This Unicode String Type is
 *
 * Copyright (c) 1999 by Secret Labs AB
 * Copyright (c) 1999 by Fredrik Lundh
 *
 * By obtaining, using, and/or copying this software and/or its
 * associated documentation, you agree that you have read, understood,
 * and will comply with the following terms and conditions:
 *
 * Permission to use, copy, modify, and distribute this software and its
 * associated documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appears in all
 * copies, and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Secret Labs
 * AB or the author not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 *
 * SECRET LABS AB AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS.  IN NO EVENT SHALL SECRET LABS AB OR THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * -------------------------------------------------------------------- */

#include <ctype.h>

/* === Internal API ======================================================= */

/* --- Internal Unicode Format -------------------------------------------- */

/* Python 3.x requires unicode */
#define Py_USING_UNICODE

/* Py_UCS4 and Py_UCS2 are typedefs for the respective
   unicode representations. */

typedef uint32_t Py_UCS4;
typedef uint16_t Py_UCS2;
typedef uint8_t Py_UCS1;

#ifdef __cplusplus
extern "C" {
#endif


PyAPI_DATA(PyTypeObject) PyUnicode_Type;
PyAPI_DATA(PyTypeObject) PyUnicodeIter_Type;

#define PyUnicode_Check(op) \
                 PyType_HasFeature(Py_TYPE(op), Py_TPFLAGS_UNICODE_SUBCLASS)
#define PyUnicode_CheckExact(op) Py_IS_TYPE(op, &PyUnicode_Type)

/* --- Constants ---------------------------------------------------------- */
  
/* === Public API ========================================================= */

PyAPI_FUNC(Py_ssize_t) PyString_Size(PyObject *str);
PyAPI_FUNC(unsigned char) PyString_ReadChar(PyObject *str, Py_ssize_t index);
PyAPI_FUNC(PyObject*) PyString_FromStringAndSize(const char *, Py_ssize_t);
PyAPI_FUNC(PyObject*) PyString_FromString(const char *);
PyAPI_FUNC(PyObject*) PyString_Substring(PyObject *str, Py_ssize_t start, Py_ssize_t end);
PyAPI_FUNC(int) PyString_Resize(PyObject **str, Py_ssize_t length);
PyAPI_FUNC(PyObject *) PyString_FromFormatV(const char *format, va_list vargs);
PyAPI_FUNC(PyObject *) PyString_FromFormat(const char *format, ...);

PyAPI_FUNC(void) PyUnicode_InternInPlace(PyObject **);
PyAPI_FUNC(void) PyUnicode_InternImmortal(PyObject **);
PyAPI_FUNC(PyObject *) PyUnicode_InternFromString(
    const char *u              /* UTF-8 encoded string */
    );

/* Use only if you know it's a string */
#define PyUnicode_CHECK_INTERNED(op) \
    (((PyUnicodeObject *)(op))->state.interned)
 
PyAPI_FUNC(PyObject*) PyString_FromOrdinal(int ordinal);
  
  /* --- Methods & Slots ---------------------------------------------------- */

PyAPI_FUNC(PyObject*) PyString_Concat(PyObject *left, PyObject *right);
PyAPI_FUNC(void) PyString_Append(PyObject **pleft, PyObject *right);
PyAPI_FUNC(void) PyString_AppendAndDel(PyObject **pleft,  PyObject *right);
PyAPI_FUNC(PyObject*) PyString_Split(PyObject *s, PyObject *sep, Py_ssize_t maxsplit);
PyAPI_FUNC(PyObject*) PyString_Splitlines(PyObject *s, int keepends);
PyAPI_FUNC(PyObject*) PyString_Partition(PyObject *s, PyObject *sep);
PyAPI_FUNC(PyObject*) PyString_RPartition(PyObject *s, PyObject *sep);
PyAPI_FUNC(PyObject*) PyString_RSplit(PyObject *s, PyObject *sep, Py_ssize_t maxsplit);
PyAPI_FUNC(PyObject*) PyString_Join(PyObject *separator, PyObject *seq);
PyAPI_FUNC(Py_ssize_t) PyString_Tailmatch(PyObject *str, PyObject *substr, Py_ssize_t start,
					  Py_ssize_t end, int direction);
PyAPI_FUNC(Py_ssize_t) PyString_Find(PyObject *str, PyObject *substr, 
				     Py_ssize_t start, Py_ssize_t end,int direction);
PyAPI_FUNC(Py_ssize_t) PyString_FindChar(PyObject *str, Py_UCS4 ch, Py_ssize_t start,
					 Py_ssize_t end, int direction);
PyAPI_FUNC(Py_ssize_t) PyString_Count(PyObject *str, PyObject *substr,
				      Py_ssize_t start, Py_ssize_t end);

PyAPI_FUNC(PyObject *) PyString_Replace(PyObject *str, PyObject *substr, PyObject *replstr,
					Py_ssize_t maxcount);

PyAPI_FUNC(int) PyString_Compare(PyObject *left, PyObject *right);
PyAPI_FUNC(int) PyString_CompareWithASCIIString(PyObject *left, const char *right);
PyAPI_FUNC(PyObject *) PyString_RichCompare(PyObject *left, PyObject *right, int op);
PyAPI_FUNC(PyObject *) PyString_Format(PyObject *format, PyObject *args);
PyAPI_FUNC(int) PyString_Contains(PyObject *container, PyObject *element);
PyAPI_FUNC(int) PyString_IsIdentifier(PyObject *s);

/* === Characters Type APIs =============================================== */

/* --- Internal Unicode Operations ---------------------------------------- */

/* Since splitting on whitespace is an important use case, and
   whitespace in most situations is solely ASCII whitespace, we
   optimize for the common case by using a quick look-up table
   _Py_ascii_whitespace (see below) with an inlined check.

 */

#define Py_UNICODE_ISSPACE(ch) \
    ((ch) < 128U ? _Py_ascii_whitespace[(ch)] : _PyUnicode_IsWhitespace(ch))
#define Py_UNICODE_TODECIMAL(ch) _PyUnicode_ToDecimalDigit(ch)
  
/* --- Unicode Type ------------------------------------------------------- */

  #if 0
typedef struct {
    PyObject_HEAD
    Py_ssize_t length;          /* Number of code points in the string */
    Py_hash_t hash;             /* Hash value; -1 if not set */
    struct {
        unsigned int interned:2;
        unsigned int :30;
    } state;
    void *data;
} PyUnicodeObject;
  
/* Returns the deprecated Py_UNICODE representation's size in code units
   (this includes surrogate pairs as 2 units).
   If the Py_UNICODE representation is not available, it will be computed
   on request.  Use PyUnicode_GET_LENGTH() for the length in code points. */

/* Py_DEPRECATED(3.3) */

#define PyUnicode_GET_SIZE(op)                       \
  (((PyUnicodeObject*)op)->length)  

  #endif
  
/* --- Flexible String Representation Helper Macros (PEP 393) -------------- */

/* Values for PyASCIIObject.state: */

/* Interning state. */
#define SSTATE_NOT_INTERNED 0
#define SSTATE_INTERNED_MORTAL 1
#define SSTATE_INTERNED_IMMORTAL 2

  #if 0
/* Return a void pointer to the raw unicode buffer. */
#define PyUnicode_DATA(op) \
    (assert(((PyUnicodeObject*)(op))->data),        \
     ((((PyUnicodeObject *)(op))->data)))

  #endif
  
/* In the access macros below, "kind" may be evaluated more than once.
   All other macro parameters are evaluated exactly once, so it is safe
   to put side effects into them (such as increasing the index). */

  #if 0
/* Write into the canonical representation, this macro does not do any sanity
   checks and is intended for usage in loops.  The caller should cache the
   kind and data pointers obtained from other macro calls.
   index is the index in the string (starts at 0) and value is the new
   code point value which should be written to that location. */
#define PyUnicode_WRITE(data, index, value) \
    do { \
            ((unsigned char *)(data))[(index)] = (unsigned char)(value); \
    } while (0)

/* Read a code point from the string's canonical representation.  No checks
   or ready calls are performed. */
#define PyUnicode_READ(data, index) \
    ((unsigned char) \
     ((const unsigned char *)(data))[(index)])

/* PyUnicode_READ_CHAR() is less efficient than PyUnicode_READ() because it
   calls PyUnicode_KIND() and might call it twice.  For single reads, use
   PyUnicode_READ_CHAR, for multiple consecutive reads callers should
   cache kind and use PyUnicode_READ instead. */
#define PyUnicode_READ_CHAR(unicode, index) \
    (assert(PyUnicode_Check(unicode)),          \
     (unsigned char)                                  \
     ((const unsigned char *)(PyUnicode_DATA((unicode))))[(index)])

  #endif
  
  #if 0
/* Returns the length of the unicode string. The caller has to make sure that
   the string has it's canonical representation set before calling
   this macro.  Call PyUnicode_(FAST_)Ready to ensure that. */
#define PyUnicode_GET_LENGTH(op)                \
    (assert(PyUnicode_Check(op)),               \
     ((PyUnicodeObject *)(op))->length)
#endif
  

/* Return a maximum character value which is suitable for creating another
   string based on op.  This is always an approximation but more efficient
   than iterating over the string. */
#define PyUnicode_MAX_CHAR_VALUE(op) 0xffU

/* === Public API ========================================================= */

  PyAPI_FUNC(PyObject*) PyString_New(Py_ssize_t size);
  PyAPI_FUNC(PyObject*) _PyString_Copy(PyObject *);

  PyAPI_FUNC(Py_ssize_t) PyString_CopyCharacters(
    PyObject *to,
    Py_ssize_t to_start,
    PyObject *from,
    Py_ssize_t from_start,
    Py_ssize_t how_many
    );

/* Unsafe version of PyUnicode_CopyCharacters(): don't check arguments and so
   may crash if parameters are invalid (e.g. if the output string
   is too short). */
PyAPI_FUNC(void) _PyString_FastCopyCharacters(
    PyObject *to,
    Py_ssize_t to_start,
    PyObject *from,
    Py_ssize_t from_start,
    Py_ssize_t how_many
    );

/* Fill a string with a character: write fill_char into
   unicode[start:start+length].

   Fail if fill_char is bigger than the string maximum character, or if the
   string has more than 1 reference.

   Return the number of written character, or return -1 and raise an exception
   on error. */
PyAPI_FUNC(Py_ssize_t) PyString_Fill(
    PyObject *str,
    Py_ssize_t start,
    Py_ssize_t length,
    unsigned char fill_char
    );

/* Unsafe version of PyUnicode_Fill(): don't check arguments and so may crash
   if parameters are invalid (e.g. if length is longer than the string). */
PyAPI_FUNC(void) _PyString_FastFill(
    PyObject *str,
    Py_ssize_t start,
    Py_ssize_t length,
    unsigned char fill_char
    );
  
/* --- _PyUnicodeWriter API ----------------------------------------------- */

typedef struct {
  PyObject *buffer;               // Unicode String being built up so far
  void *data;
  Py_ssize_t size;
  Py_ssize_t pos;

  /* minimum number of allocated characters (default: 0) */
  Py_ssize_t min_length;
  
  /* If non-zero, overallocate the buffer (default: 0). */
  unsigned char overallocate;

  /* If readonly is 1, buffer is a shared string (cannot be modified)
     and size is set to 0. */
  unsigned char readonly;
} _PyUnicodeWriter ;

/* Initialize a Unicode writer.
 *
 * By default, the minimum buffer size is 0 character and overallocation is
 * disabled. Set min_length, min_char and overallocate attributes to control
 * the allocation of the buffer. */
PyAPI_FUNC(void)
_PyUnicodeWriter_Init(_PyUnicodeWriter *writer);

/* Prepare the buffer to write 'length' characters
   with the specified maximum character.

   Return 0 on success, raise an exception and return -1 on error. */
#define _PyUnicodeWriter_Prepare(WRITER, LENGTH)		\
  (_PyUnicodeWriter_PrepareInternal((WRITER), (LENGTH)))

/* Don't call this function directly, use the _PyUnicodeWriter_Prepare() macro
   instead. */
PyAPI_FUNC(int)
_PyUnicodeWriter_PrepareInternal(_PyUnicodeWriter *writer,
                                 Py_ssize_t length);

/* Append a Unicode character.
   Return 0 on success, raise an exception and return -1 on error. */
PyAPI_FUNC(int)
_PyUnicodeWriter_WriteChar(_PyUnicodeWriter *writer,
    Py_UCS4 ch
    );

/* Append a Unicode string.
   Return 0 on success, raise an exception and return -1 on error. */
PyAPI_FUNC(int)
_PyUnicodeWriter_WriteStr(_PyUnicodeWriter *writer,
    PyObject *str               /* Unicode string */
    );

/* Append a substring of a Unicode string.
   Return 0 on success, raise an exception and return -1 on error. */
PyAPI_FUNC(int)
_PyUnicodeWriter_WriteSubstring(_PyUnicodeWriter *writer,
    PyObject *str,              /* Unicode string */
    Py_ssize_t start,
    Py_ssize_t end
    );

/* Append an ASCII-encoded byte string.
   Return 0 on success, raise an exception and return -1 on error. */
PyAPI_FUNC(int)
_PyUnicodeWriter_WriteASCIIString(_PyUnicodeWriter *writer,
    const char *str,           /* ASCII-encoded byte string */
    Py_ssize_t len             /* number of bytes, or -1 if unknown */
    );

/* Append a latin1-encoded byte string.
   Return 0 on success, raise an exception and return -1 on error. */
PyAPI_FUNC(int)
_PyUnicodeWriter_WriteLatin1String(_PyUnicodeWriter *writer,
    const char *str,           /* latin1-encoded byte string */
    Py_ssize_t len             /* length in bytes */
    );

/* Get the value of the writer as a Unicode string. Clear the
   buffer of the writer. Raise an exception and return NULL
   on error. */
PyAPI_FUNC(PyObject *)
_PyUnicodeWriter_Finish(_PyUnicodeWriter *writer);

/* Deallocate memory of a writer (clear its internal buffer). */
PyAPI_FUNC(void)
_PyUnicodeWriter_Dealloc(_PyUnicodeWriter *writer);


/* Format the object based on the format_spec, as defined in PEP 3101
   (Advanced String Formatting). */
PyAPI_FUNC(int) _PyUnicode_FormatAdvancedWriter(
    _PyUnicodeWriter *writer,
    PyObject *obj,
    PyObject *format_spec,
    Py_ssize_t start,
    Py_ssize_t end);

PyAPI_FUNC(const char *) PyString_AsCharAndSize(PyObject *s, Py_ssize_t *size);
PyAPI_FUNC(const char *) PyString_AsChar(PyObject *s);

/* Coverts a Unicode object holding a decimal value to an ASCII string
   for using in int, float and complex parsers.
   Transforms code points that have decimal digit property to the
   corresponding ASCII digit code points.  Transforms spaces to ASCII.
   Transforms code points starting from the first non-ASCII code point that
   is neither a decimal digit nor a space to the end into '?'. */

PyAPI_FUNC(PyObject*) _PyUnicode_TransformDecimalAndSpaceToASCII(
    PyObject *unicode           /* Unicode object */
    );

/* --- Methods & Slots ---------------------------------------------------- */

PyAPI_FUNC(PyObject *) _PyString_JoinArray(PyObject *separator, PyObject *const *items, Py_ssize_t seqlen);

/* Test whether a unicode is equal to ASCII identifier.  Return 1 if true,
   0 otherwise.  The right argument must be ASCII identifier.
   Any error occurs inside will be cleared before return. */
PyAPI_FUNC(int) _PyUnicode_EqualToASCIIId(
    PyObject *left,             /* Left string */
    _Py_Identifier *right       /* Right identifier */
    );

/* Test whether a unicode is equal to ASCII string.  Return 1 if true,
   0 otherwise.  The right argument must be ASCII-encoded string.
   Any error occurs inside will be cleared before return. */
PyAPI_FUNC(int) _PyUnicode_EqualToASCIIString(
    PyObject *left,
    const char *right           /* ASCII-encoded string */
    );

/* Externally visible for str.strip(unicode) */
PyAPI_FUNC(PyObject *) _PyUnicode_XStrip(
    PyObject *self,
    int striptype,
    PyObject *sepobj
    );

/* Using explicit passed-in values, insert the thousands grouping
   into the string pointed to by buffer.  For the argument descriptions,
   see Objects/stringlib/localeutil.h */
PyAPI_FUNC(Py_ssize_t) _PyUnicode_InsertThousandsGrouping(
    _PyUnicodeWriter *writer,
    Py_ssize_t n_buffer,
    PyObject *digits,
    Py_ssize_t d_pos,
    Py_ssize_t n_digits,
    Py_ssize_t min_width,
    const char *grouping,
    PyObject *thousands_sep);

/* === Characters Type APIs =============================================== */

/* Helper array used by Py_UNICODE_ISSPACE(). */

PyAPI_DATA(const unsigned char) _Py_ascii_whitespace[];

/* These should not be used directly. Use the Py_UNICODE_IS* and
   Py_UNICODE_TO* macros instead.

   These APIs are implemented in Objects/unicodectype.c.

*/
  
PyAPI_FUNC(int) _PyUnicode_IsWhitespace(
    const Py_UCS4 ch         /* Unicode character */
    );
  
PyAPI_FUNC(int) _PyUnicode_ToDecimalDigit(
    Py_UCS4 ch       /* Unicode character */
    );
  
PyAPI_FUNC(PyObject*) _PyUnicode_FormatLong(PyObject *, int, int, int);

/* Create a copy of a unicode string ending with a nul character. Return NULL
   and raise a MemoryError exception on memory allocation failure, otherwise
   return a new allocated buffer (use PyMem_Free() to free the buffer). */

/* Return an interned Unicode object for an Identifier; may fail if there is no memory.*/
PyAPI_FUNC(PyObject*) _PyUnicode_FromId(_Py_Identifier*);

/* Fast equality check when the inputs are known to be exact unicode types
   and where the hash values are equal (i.e. a very probable match) */
PyAPI_FUNC(int) _PyUnicode_EQ(PyObject *, PyObject *);

PyAPI_FUNC(Py_ssize_t) _PyUnicode_ScanIdentifier(PyObject *);

  /* Helper for PyBytes_DecodeEscape that detects invalid escape chars. */
PyAPI_FUNC(PyObject *) _PyBytes_DecodeEscape(const char *, Py_ssize_t,
                                             const char *, const char **);

  
/* Flags used by string formatting */
#define F_LJUST (1<<0)
#define F_SIGN  (1<<1)
#define F_BLANK (1<<2)
#define F_ALT   (1<<3)
#define F_ZERO  (1<<4)

#ifdef __cplusplus
}
#endif
#endif /* !Py_UNICODEOBJECT_H */
