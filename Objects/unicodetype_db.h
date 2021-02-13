/* this file was generated by makeunicodedata.py 3.3 */

/* a list of unique character type descriptors */
const _PyUnicode_TypeRecord _PyUnicode_TypeRecords[] = {
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 32},
    {0, 0, 0, 0, 0, 48},
    {0, 0, 0, 0, 0, 1056},
    {0, 0, 0, 0, 0, 1024},
    {0, 0, 0, 0, 0, 5120},
    {0, 0, 0, 0, 0, 3590},
    {0, 0, 0, 1, 1, 3590},
    {0, 0, 0, 2, 2, 3590},
    {0, 0, 0, 3, 3, 3590},
    {0, 0, 0, 4, 4, 3590},
    {0, 0, 0, 5, 5, 3590},
    {0, 0, 0, 6, 6, 3590},
    {0, 0, 0, 7, 7, 3590},
    {0, 0, 0, 8, 8, 3590},
    {0, 0, 0, 9, 9, 3590},
    {0, 32, 0, 0, 0, 10113},
    {0, 0, 0, 0, 0, 1536},
    {-32, 0, -32, 0, 0, 9993},
    {0, 0, 0, 0, 0, 9993},
    {0, 0, 0, 0, 0, 4096},
    {0, 0, 0, 0, 2, 3076},
    {0, 0, 0, 0, 3, 3076},
    {16777218, 17825792, 16777218, 0, 0, 26377},
    {0, 0, 0, 0, 0, 5632},
    {0, 0, 0, 0, 1, 3076},
    {0, 0, 0, 0, 0, 3072},
    {33554438, 18874371, 33554440, 0, 0, 26377},
    {121, 0, 121, 0, 0, 9993},
};

/* extended case mappings */

const Py_UCS1 _PyUnicode_ExtendedCase[] = {
    181,
    956,
    924,
    223,
    115,
    115,
    83,
    83,
    83,
    115,
};

/* type indexes */
#define SHIFT 2
static const unsigned char index1[] = {
    0, 0, 1, 2, 0, 0, 0, 3, 4, 5, 6, 7, 8, 9, 10, 6, 11, 12, 12, 12, 12, 12,
    13, 14, 15, 16, 16, 16, 16, 16, 17, 18, 0, 19, 0, 0, 0, 0, 0, 0, 20, 6,
    21, 22, 23, 24, 25, 26, 12, 12, 12, 12, 12, 13, 12, 27, 16, 16, 16, 16,
    16, 17, 16, 28,
};

static const unsigned char index2[] = {
    1, 1, 1, 1, 1, 2, 3, 3, 3, 3, 1, 1, 3, 3, 3, 2, 4, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 6, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 6, 5, 5, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 5, 5, 5, 6, 18, 6, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 5, 5, 5, 5, 1, 1, 3, 1, 1, 2, 5, 5, 5, 6, 5, 20,
    5, 5, 21, 5, 6, 5, 5, 22, 23, 6, 24, 5, 25, 6, 26, 20, 5, 27, 27, 27, 5,
    17, 17, 17, 28, 19, 19, 19, 29,
};

/* Returns the numeric value as double for Unicode characters
 * having this property, -1.0 otherwise.
 */
double _PyUnicode_ToNumeric(Py_UCS1 ch)
{
    switch (ch) {
    case 0x0030:
        return (double) 0.0;
    case 0x0031:
    case 0x00B9:
        return (double) 1.0;
    case 0x00BD:
        return (double) 1.0/2.0;
    case 0x00BC:
        return (double) 1.0/4.0;
    case 0x0032:
    case 0x00B2:
        return (double) 2.0;
    case 0x0033:
    case 0x00B3:
        return (double) 3.0;
    case 0x00BE:
        return (double) 3.0/4.0;
    case 0x0034:
        return (double) 4.0;
    case 0x0035:
        return (double) 5.0;
    case 0x0036:
        return (double) 6.0;
    case 0x0037:
        return (double) 7.0;
    case 0x0038:
        return (double) 8.0;
    case 0x0039:
        return (double) 9.0;
    }
    return -1.0;
}

/* Returns 1 for Unicode characters having the bidirectional
 * type 'WS', 'B' or 'S' or the category 'Zs', 0 otherwise.
 */
int _PyUnicode_IsWhitespace(const Py_UCS1 ch)
{
    switch (ch) {
    case 0x0009:
    case 0x000A:
    case 0x000B:
    case 0x000C:
    case 0x000D:
    case 0x001C:
    case 0x001D:
    case 0x001E:
    case 0x001F:
    case 0x0020:
    case 0x0085:
    case 0x00A0:
        return 1;
    }
    return 0;
}

/* Returns 1 for Unicode characters having the line break
 * property 'BK', 'CR', 'LF' or 'NL' or having bidirectional
 * type 'B', 0 otherwise.
 */
int _PyUnicode_IsLinebreak(const Py_UCS1 ch)
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

