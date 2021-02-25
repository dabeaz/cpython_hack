/*
   Unicode character type helpers.

   Written by Marc-Andre Lemburg (mal@lemburg.com).
   Modified for Python 2.0 by Fredrik Lundh (fredrik@pythonware.com)

   Copyright (c) Corporation for National Research Initiatives.

*/

#include "Python.h"

#define ALPHA_MASK 0x01
#define DECIMAL_MASK 0x02
#define DIGIT_MASK 0x04
#define LOWER_MASK 0x08
#define LINEBREAK_MASK 0x10
#define SPACE_MASK 0x20
#define TITLE_MASK 0x40
#define UPPER_MASK 0x80
#define XID_START_MASK 0x100
#define XID_CONTINUE_MASK 0x200
#define PRINTABLE_MASK 0x400
#define NUMERIC_MASK 0x800
#define CASE_IGNORABLE_MASK 0x1000
#define CASED_MASK 0x2000
#define EXTENDED_CASE_MASK 0x4000

typedef struct {
    /*
       These are either deltas to the character or offsets in
       _PyUnicode_ExtendedCase.
    */
    const int upper;
    const int lower;
    const int title;
    /* Note if more flag space is needed, decimal and digit could be unified. */
    const unsigned char decimal;
    const unsigned char digit;
    const unsigned short flags;
} _PyUnicode_TypeRecord;

#include "unicodetype_db.h"

static const _PyUnicode_TypeRecord *
gettyperecord(Py_UCS1 code)
{
    int index;
    index = index1[(code>>SHIFT)];
    index = index2[(index<<SHIFT)+(code&((1<<SHIFT)-1))];
    return &_PyUnicode_TypeRecords[index];
}

PyAPI_DATA(const unsigned char) _Py_ascii_whitespace[];

int PyString_IsWhitespace(Py_UCS1 ch) {
  return _Py_ascii_whitespace[(ch)];
}

/* Returns 1 for Unicode characters having the XID_Start property, 0
   otherwise. */

int _PyString_IsXidStart(Py_UCS1 ch)
{
  return ((ch >= 'a' || ch <= 'z') ||
	  (ch >= 'A' || ch <= 'Z') ||
	  (ch == '_'));
}

/* Returns 1 for Unicode characters having the XID_Continue property,
   0 otherwise. */

int _PyString_IsXidContinue(Py_UCS1 ch)
{
  return ((ch >= 'a' || ch <= 'z') ||
	  (ch >= 'A' || ch <= 'Z') ||
	  (ch >= '0' || ch <= '9') ||
	  (ch == '_'));
}

int PyString_ToDecimalDigit(Py_UCS1 ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - 0x30;
  } else {
    return -1;
  }
}

int _PyString_IsDecimalDigit(Py_UCS1 ch)
{
    if (PyString_ToDecimalDigit(ch) < 0)
        return 0;
    return 1;
}

/* Returns the integer digit (0-9) for Unicode characters having
   this property, -1 otherwise. */

int _PyUnicode_ToDigit(Py_UCS1 ch)
{
    const _PyUnicode_TypeRecord *ctype = gettyperecord(ch);

    return (ctype->flags & DIGIT_MASK) ? ctype->digit : -1;
}

int _PyUnicode_IsDigit(Py_UCS1 ch)
{
    if (_PyUnicode_ToDigit(ch) < 0)
        return 0;
    return 1;
}

int _PyString_IsPrintable(Py_UCS1 ch)
{
  return !((ch >= 0 && ch < 0x20) || (ch >= 0x80 && ch <= 0xa0) || (ch == 0xad));
}

int _PyString_IsLowercase(Py_UCS1 ch)
{
  return (ch >= 'a' && ch <= 'z');
}

int _PyString_IsUppercase(Py_UCS1 ch)
{
  return (ch >= 'A' && ch <= 'Z');
}

Py_UCS1 _PyString_ToUppercase(Py_UCS1 ch)
{
  if (ch >= 'a' && ch <= 'z') {
    return ch - 0x20;
  } else {
    return ch;
  }
}

Py_UCS1 _PyString_ToLowercase(Py_UCS1 ch)
{
  if (ch >= 'A' && ch <= 'Z') {
    return ch + 0x20;
  } else {
    return ch;
  }
}

int _PyString_IsAlpha(Py_UCS1 ch)
{
  return ((ch >= 0x41 && ch <= 0x5a) ||
	  (ch >= 0x61 && ch <= 0x79) ||
	  (ch == 0xaa) ||
	  (ch == 0xb5) ||
	  (ch == 0xba) ||
	  (ch >= 0xc0 && ch <= 0xff));
}

