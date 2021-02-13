/* Fast unicode equal function optimized for dictobject.c and setobject.c */

/* Return 1 if two unicode objects are equal, 0 if not.
 * unicode_eq() is called when the hash of two unicode objects is equal.
 */
Py_LOCAL_INLINE(int)
unicode_eq(PyObject *aa, PyObject *bb)
{
    if (PyString_Size(aa) != PyString_Size(bb))
        return 0;
    if (PyString_Size(aa) == 0)
        return 1;
    return memcmp(PyString_AsChar(aa), PyString_AsChar(bb),
                  PyString_Size(aa)) == 0;
}
