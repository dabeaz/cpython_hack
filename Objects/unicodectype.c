/*
   Unicode character type helpers.

   Written by Marc-Andre Lemburg (mal@lemburg.com).
   Modified for Python 2.0 by Fredrik Lundh (fredrik@pythonware.com)

   Copyright (c) Corporation for National Research Initiatives.

*/

#include "Python.h"

PyAPI_DATA(const unsigned char) _Py_ascii_whitespace[];

int PyString_IsWhitespace(Py_UCS1 ch) {
  return _Py_ascii_whitespace[(ch)];
}

int _PyString_IsLinebreak(const Py_UCS1 ch)
{
    switch (ch) {
    case 0x000A:
    case 0x000B:
    case 0x000C:
    case 0x000D:
    case 0x001C:
    case 0x001D:
    case 0x001E:
    case 0x0085:
        return 1;
    }
    return 0;
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

int _PyString_ToDigit(Py_UCS1 ch)
{
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  } else {
    return -1;
  }
}

int _PyString_IsDigit(Py_UCS1 ch)
{
  return (ch >= '0' && ch <= '9');
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

