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

typedef uint8_t Py_UCS1;

#ifdef __cplusplus
extern "C" {
#endif


PyAPI_DATA(PyTypeObject) PyString_Type;
PyAPI_DATA(PyTypeObject) PyStringIter_Type;

PyAPI_FUNC(int) PyString_Check(PyObject *);
PyAPI_FUNC(int) PyString_CheckExact(PyObject *);
  
/* --- Constants ---------------------------------------------------------- */
  
/* === Public API ========================================================= */


PyAPI_FUNC(const char *) PyString_AsCharAndSize(PyObject *s, Py_ssize_t *size);
PyAPI_FUNC(const char *) PyString_AsChar(PyObject *s);
PyAPI_FUNC(Py_ssize_t) PyString_Size(PyObject *str);
PyAPI_FUNC(Py_hash_t) PyString_Hash(PyObject *s);
PyAPI_FUNC(unsigned char) PyString_ReadChar(PyObject *str, Py_ssize_t index);
PyAPI_FUNC(PyObject*) PyString_FromStringAndSize(const char *, Py_ssize_t);
PyAPI_FUNC(PyObject*) PyString_FromString(const char *);
PyAPI_FUNC(PyObject*) PyString_Substring(PyObject *str, Py_ssize_t start, Py_ssize_t end);
PyAPI_FUNC(int) PyString_Resize(PyObject **str, Py_ssize_t length);
PyAPI_FUNC(PyObject *) PyString_FromFormatV(const char *format, va_list vargs);
PyAPI_FUNC(PyObject *) PyString_FromFormat(const char *format, ...);

PyAPI_FUNC(void) PyString_InternInPlace(PyObject **);  
PyAPI_FUNC(PyObject *) PyString_InternFromString(const char *u);
  
PyAPI_FUNC(PyObject*) PyString_FromOrdinal(int ordinal);
PyAPI_FUNC(int) PyString_CheckInterned(PyObject *s);
  
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
PyAPI_FUNC(Py_ssize_t) PyString_FindChar(PyObject *str, Py_UCS1 ch, Py_ssize_t start,
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
  PyObject *buffer;               // String being built up so far
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
} _PyStringWriter ;

/* Initialize a Unicode writer.
 *
 * By default, the minimum buffer size is 0 character and overallocation is
 * disabled. Set min_length, min_char and overallocate attributes to control
 * the allocation of the buffer. */
PyAPI_FUNC(void)
_PyStringWriter_Init(_PyStringWriter *writer);

/* Prepare the buffer to write 'length' characters
   with the specified maximum character.

   Return 0 on success, raise an exception and return -1 on error. */
#define _PyStringWriter_Prepare(WRITER, LENGTH)		\
  (_PyStringWriter_PrepareInternal((WRITER), (LENGTH)))

/* Don't call this function directly, use the _PyStringWriter_Prepare() macro
   instead. */
PyAPI_FUNC(int)
_PyStringWriter_PrepareInternal(_PyStringWriter *writer,
                                 Py_ssize_t length);

/* Append a Unicode character.
   Return 0 on success, raise an exception and return -1 on error. */
PyAPI_FUNC(int)
_PyStringWriter_WriteChar(_PyStringWriter *writer,
    Py_UCS1 ch
    );

/* Append a Unicode string.
   Return 0 on success, raise an exception and return -1 on error. */
PyAPI_FUNC(int)
_PyStringWriter_WriteStr(_PyStringWriter *writer,
    PyObject *str               /* Unicode string */
    );

/* Append a substring of a Unicode string.
   Return 0 on success, raise an exception and return -1 on error. */
PyAPI_FUNC(int)
_PyStringWriter_WriteSubstring(_PyStringWriter *writer,
    PyObject *str,              /* Unicode string */
    Py_ssize_t start,
    Py_ssize_t end
    );

/* Append an ASCII-encoded byte string.
   Return 0 on success, raise an exception and return -1 on error. */
PyAPI_FUNC(int)
_PyStringWriter_WriteASCIIString(_PyStringWriter *writer,
    const char *str,           /* ASCII-encoded byte string */
    Py_ssize_t len             /* number of bytes, or -1 if unknown */
    );

/* Append a latin1-encoded byte string.
   Return 0 on success, raise an exception and return -1 on error. */
PyAPI_FUNC(int)
_PyStringWriter_WriteLatin1String(_PyStringWriter *writer,
    const char *str,           /* latin1-encoded byte string */
    Py_ssize_t len             /* length in bytes */
    );

/* Get the value of the writer as a Unicode string. Clear the
   buffer of the writer. Raise an exception and return NULL
   on error. */
PyAPI_FUNC(PyObject *)
_PyStringWriter_Finish(_PyStringWriter *writer);

/* Deallocate memory of a writer (clear its internal buffer). */
PyAPI_FUNC(void)
_PyStringWriter_Dealloc(_PyStringWriter *writer);


/* Format the object based on the format_spec, as defined in PEP 3101
   (Advanced String Formatting). */
PyAPI_FUNC(int) _PyString_FormatAdvancedWriter(
    _PyStringWriter *writer,
    PyObject *obj,
    PyObject *format_spec,
    Py_ssize_t start,
    Py_ssize_t end);

/* Coverts a Unicode object holding a decimal value to an ASCII string
   for using in int, float and complex parsers.
   Transforms code points that have decimal digit property to the
   corresponding ASCII digit code points.  Transforms spaces to ASCII.
   Transforms code points starting from the first non-ASCII code point that
   is neither a decimal digit nor a space to the end into '?'. */

PyAPI_FUNC(PyObject*) _PyString_TransformDecimalAndSpaceToASCII(
    PyObject *unicode           /* Unicode object */
    );

/* --- Methods & Slots ---------------------------------------------------- */

PyAPI_FUNC(PyObject *) _PyString_JoinArray(PyObject *separator, PyObject *const *items, Py_ssize_t seqlen);

/* Test whether a unicode is equal to ASCII identifier.  Return 1 if true,
   0 otherwise.  The right argument must be ASCII identifier.
   Any error occurs inside will be cleared before return. */

PyAPI_FUNC(int) _PyString_EqualToASCIIId(
    PyObject *left,             /* Left string */
    _Py_Identifier *right       /* Right identifier */
    );
  
/* Test whether a unicode is equal to ASCII string.  Return 1 if true,
   0 otherwise.  The right argument must be ASCII-encoded string.
   Any error occurs inside will be cleared before return. */

PyAPI_FUNC(int) _PyString_EqualToASCIIString(
    PyObject *left,
    const char *right           /* ASCII-encoded string */
    );

/* Externally visible for str.strip(unicode) */
PyAPI_FUNC(PyObject *) _PyString_XStrip(
    PyObject *self,
    int striptype,
    PyObject *sepobj
    );

/* Using explicit passed-in values, insert the thousands grouping
   into the string pointed to by buffer.  For the argument descriptions,
   see Objects/stringlib/localeutil.h */
PyAPI_FUNC(Py_ssize_t) _PyString_InsertThousandsGrouping(
    _PyStringWriter *writer,
    Py_ssize_t n_buffer,
    PyObject *digits,
    Py_ssize_t d_pos,
    Py_ssize_t n_digits,
    Py_ssize_t min_width,
    const char *grouping,
    PyObject *thousands_sep);

/* === Characters Type APIs =============================================== */
  
/* These should not be used directly. Use the Py_UNICODE_IS* and
   Py_UNICODE_TO* macros instead.

   These APIs are implemented in Objects/unicodectype.c.
*/
  
PyAPI_FUNC(int) PyString_IsWhitespace(
    const Py_UCS1 ch         /* Unicode character */
    );
  
PyAPI_FUNC(int) PyString_ToDecimalDigit(
    Py_UCS1 ch       /* Unicode character */
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
