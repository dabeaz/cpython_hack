/* implements the unicode (as opposed to string) version of the
   built-in formatters for string, int, float.  that is, the versions
   of int.__float__, etc., that take and return unicode objects */

#include "Python.h"
#include "pycore_fileutils.h"
#include <locale.h>

/* Raises an exception about an unknown presentation type for this
 * type. */

static void
unknown_presentation_type(Py_UCS1 presentation_type,
                          const char* type_name)
{
    /* %c might be out-of-range, hence the two cases. */
    if (presentation_type > 32 && presentation_type < 128)
        PyErr_Format(PyExc_ValueError,
                     "Unknown format code '%c' "
                     "for object of type '%.200s'",
                     (char)presentation_type,
                     type_name);
    else
        PyErr_Format(PyExc_ValueError,
                     "Unknown format code '\\x%x' "
                     "for object of type '%.200s'",
                     (unsigned int)presentation_type,
                     type_name);
}

static void
invalid_thousands_separator_type(char specifier, Py_UCS1 presentation_type)
{
    assert(specifier == ',' || specifier == '_');
    if (presentation_type > 32 && presentation_type < 128)
        PyErr_Format(PyExc_ValueError,
                     "Cannot specify '%c' with '%c'.",
                     specifier, (char)presentation_type);
    else
        PyErr_Format(PyExc_ValueError,
                     "Cannot specify '%c' with '\\x%x'.",
                     specifier, (unsigned int)presentation_type);
}

static void
invalid_comma_and_underscore(void)
{
    PyErr_Format(PyExc_ValueError, "Cannot specify both ',' and '_'.");
}

/*
    get_integer consumes 0 or more decimal digit characters from an
    input string, updates *result with the corresponding positive
    integer, and returns the number of digits consumed.

    returns -1 on error.
*/
static int
get_integer(PyObject *str, Py_ssize_t *ppos, Py_ssize_t end,
                  Py_ssize_t *result)
{
    Py_ssize_t accumulator, digitval, pos = *ppos;
    int numdigits;
    const void *data = (void *) PyString_AsChar(str);

    accumulator = numdigits = 0;
    for (; pos < end; pos++, numdigits++) {
      digitval = PyString_ToDecimalDigit(((char *) data)[pos]);
        if (digitval < 0)
            break;
        /*
           Detect possible overflow before it happens:

              accumulator * 10 + digitval > PY_SSIZE_T_MAX if and only if
              accumulator > (PY_SSIZE_T_MAX - digitval) / 10.
        */
        if (accumulator > (PY_SSIZE_T_MAX - digitval) / 10) {
            PyErr_Format(PyExc_ValueError,
                         "Too many decimal digits in format string");
            *ppos = pos;
            return -1;
        }
        accumulator = accumulator * 10 + digitval;
    }
    *ppos = pos;
    *result = accumulator;
    return numdigits;
}

/************************************************************************/
/*********** standard format specifier parsing **************************/
/************************************************************************/

/* returns true if this character is a specifier alignment token */
Py_LOCAL_INLINE(int)
is_alignment_token(Py_UCS1 c)
{
    switch (c) {
    case '<': case '>': case '=': case '^':
        return 1;
    default:
        return 0;
    }
}

/* returns true if this character is a sign element */
Py_LOCAL_INLINE(int)
is_sign_element(Py_UCS1 c)
{
    switch (c) {
    case ' ': case '+': case '-':
        return 1;
    default:
        return 0;
    }
}

/* Locale type codes. LT_NO_LOCALE must be zero. */
enum LocaleType {
    LT_NO_LOCALE = 0,
    LT_DEFAULT_LOCALE = ',',
    LT_UNDERSCORE_LOCALE = '_',
    LT_UNDER_FOUR_LOCALE,
    LT_CURRENT_LOCALE
};

typedef struct {
    Py_UCS1 fill_char;
    Py_UCS1 align;
    int alternate;
    Py_UCS1 sign;
    Py_ssize_t width;
    enum LocaleType thousands_separators;
    Py_ssize_t precision;
    Py_UCS1 type;
} InternalFormatSpec;


/*
  ptr points to the start of the format_spec, end points just past its end.
  fills in format with the parsed information.
  returns 1 on success, 0 on failure.
  if failure, sets the exception
*/
static int
parse_internal_render_format_spec(PyObject *format_spec,
                                  Py_ssize_t start, Py_ssize_t end,
                                  InternalFormatSpec *format,
                                  char default_type,
                                  char default_align)
{
    Py_ssize_t pos = start;
    const void *data = (void *) PyString_AsChar(format_spec);
    /* end-pos is used throughout this code to specify the length of
       the input string */
#define READ_spec(index) ((char *) data)[index]

    Py_ssize_t consumed;
    int align_specified = 0;
    int fill_char_specified = 0;

    format->fill_char = ' ';
    format->align = default_align;
    format->alternate = 0;
    format->sign = '\0';
    format->width = -1;
    format->thousands_separators = LT_NO_LOCALE;
    format->precision = -1;
    format->type = default_type;

    /* If the second char is an alignment token,
       then parse the fill char */
    if (end-pos >= 2 && is_alignment_token(READ_spec(pos+1))) {
        format->align = READ_spec(pos+1);
        format->fill_char = READ_spec(pos);
        fill_char_specified = 1;
        align_specified = 1;
        pos += 2;
    }
    else if (end-pos >= 1 && is_alignment_token(READ_spec(pos))) {
        format->align = READ_spec(pos);
        align_specified = 1;
        ++pos;
    }

    /* Parse the various sign options */
    if (end-pos >= 1 && is_sign_element(READ_spec(pos))) {
        format->sign = READ_spec(pos);
        ++pos;
    }

    /* If the next character is #, we're in alternate mode.  This only
       applies to integers. */
    if (end-pos >= 1 && READ_spec(pos) == '#') {
        format->alternate = 1;
        ++pos;
    }

    /* The special case for 0-padding (backwards compat) */
    if (!fill_char_specified && end-pos >= 1 && READ_spec(pos) == '0') {
        format->fill_char = '0';
        if (!align_specified) {
            format->align = '=';
        }
        ++pos;
    }

    consumed = get_integer(format_spec, &pos, end, &format->width);
    if (consumed == -1)
        /* Overflow error. Exception already set. */
        return 0;

    /* If consumed is 0, we didn't consume any characters for the
       width. In that case, reset the width to -1, because
       get_integer() will have set it to zero. -1 is how we record
       that the width wasn't specified. */
    if (consumed == 0)
        format->width = -1;

    /* Comma signifies add thousands separators */
    if (end-pos && READ_spec(pos) == ',') {
        format->thousands_separators = LT_DEFAULT_LOCALE;
        ++pos;
    }
    /* Underscore signifies add thousands separators */
    if (end-pos && READ_spec(pos) == '_') {
        if (format->thousands_separators != LT_NO_LOCALE) {
            invalid_comma_and_underscore();
            return 0;
        }
        format->thousands_separators = LT_UNDERSCORE_LOCALE;
        ++pos;
    }
    if (end-pos && READ_spec(pos) == ',') {
        invalid_comma_and_underscore();
        return 0;
    }

    /* Parse field precision */
    if (end-pos && READ_spec(pos) == '.') {
        ++pos;

        consumed = get_integer(format_spec, &pos, end, &format->precision);
        if (consumed == -1)
            /* Overflow error. Exception already set. */
            return 0;

        /* Not having a precision after a dot is an error. */
        if (consumed == 0) {
            PyErr_Format(PyExc_ValueError,
                         "Format specifier missing precision");
            return 0;
        }

    }

    /* Finally, parse the type field. */

    if (end-pos > 1) {
        /* More than one char remain, invalid format specifier. */
        PyErr_Format(PyExc_ValueError, "Invalid format specifier");
        return 0;
    }

    if (end-pos == 1) {
        format->type = READ_spec(pos);
        ++pos;
    }

    /* Do as much validating as we can, just by looking at the format
       specifier.  Do not take into account what type of formatting
       we're doing (int, float, string). */

    if (format->thousands_separators) {
        switch (format->type) {
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'E':
        case 'G':
        case '%':
        case 'F':
        case '\0':
            /* These are allowed. See PEP 378.*/
            break;
        case 'b':
        case 'o':
        case 'x':
        case 'X':
            /* Underscores are allowed in bin/oct/hex. See PEP 515. */
            if (format->thousands_separators == LT_UNDERSCORE_LOCALE) {
                /* Every four digits, not every three, in bin/oct/hex. */
                format->thousands_separators = LT_UNDER_FOUR_LOCALE;
                break;
            }
            /* fall through */
        default:
            invalid_thousands_separator_type(format->thousands_separators, format->type);
            return 0;
        }
    }

    assert (format->align <= 127);
    assert (format->sign <= 127);
    return 1;
}

/* Calculate the padding needed. */
static void
calc_padding(Py_ssize_t nchars, Py_ssize_t width, Py_UCS1 align,
             Py_ssize_t *n_lpadding, Py_ssize_t *n_rpadding,
             Py_ssize_t *n_total)
{
    if (width >= 0) {
        if (nchars > width)
            *n_total = nchars;
        else
            *n_total = width;
    }
    else {
        /* not specified, use all of the chars and no more */
        *n_total = nchars;
    }

    /* Figure out how much leading space we need, based on the
       aligning */
    if (align == '>')
        *n_lpadding = *n_total - nchars;
    else if (align == '^')
        *n_lpadding = (*n_total - nchars) / 2;
    else if (align == '<' || align == '=')
        *n_lpadding = 0;
    else {
        /* We should never have an unspecified alignment. */
        Py_UNREACHABLE();
    }

    *n_rpadding = *n_total - nchars - *n_lpadding;
}

/* Do the padding, and return a pointer to where the caller-supplied
   content goes. */
static int
fill_padding(_PyStringWriter *writer,
             Py_ssize_t nchars,
             Py_UCS1 fill_char, Py_ssize_t n_lpadding,
             Py_ssize_t n_rpadding)
{
    Py_ssize_t pos;

    /* Pad on left. */
    if (n_lpadding) {
        pos = writer->pos;
        PyString_Fill(writer->buffer, pos, n_lpadding, fill_char);
    }

    /* Pad on right. */
    if (n_rpadding) {
        pos = writer->pos + nchars + n_lpadding;
        PyString_Fill(writer->buffer, pos, n_rpadding, fill_char);
    }

    /* Pointer to the user content. */
    writer->pos += n_lpadding;
    return 0;
}

/************************************************************************/
/*********** common routines for numeric formatting *********************/
/************************************************************************/

/* Locale info needed for formatting integers and the part of floats
   before and including the decimal. Note that locales only support
   8-bit chars, not unicode. */
typedef struct {
    PyObject *decimal_point;
    PyObject *thousands_sep;
    const char *grouping;
    char *grouping_buffer;
} LocaleInfo;

#define LocaleInfo_STATIC_INIT {0, 0, 0, 0}

/* describes the layout for an integer, see the comment in
   calc_number_widths() for details */
typedef struct {
    Py_ssize_t n_lpadding;
    Py_ssize_t n_prefix;
    Py_ssize_t n_spadding;
    Py_ssize_t n_rpadding;
    char sign;
    Py_ssize_t n_sign;      /* number of digits needed for sign (0/1) */
    Py_ssize_t n_grouped_digits; /* Space taken up by the digits, including
                                    any grouping chars. */
    Py_ssize_t n_decimal;   /* 0 if only an integer */
    Py_ssize_t n_remainder; /* Digits in decimal and/or exponent part,
                               excluding the decimal itself, if
                               present. */

    /* These 2 are not the widths of fields, but are needed by
       STRINGLIB_GROUPING. */
    Py_ssize_t n_digits;    /* The number of digits before a decimal
                               or exponent. */
    Py_ssize_t n_min_width; /* The min_width we used when we computed
                               the n_grouped_digits width. */
} NumberFieldWidths;


/* Given a number of the form:
   digits[remainder]
   where ptr points to the start and end points to the end, find where
    the integer part ends. This could be a decimal, an exponent, both,
    or neither.
   If a decimal point is present, set *has_decimal and increment
    remainder beyond it.
   Results are undefined (but shouldn't crash) for improperly
    formatted strings.
*/
static void
parse_number(PyObject *s, Py_ssize_t pos, Py_ssize_t end,
             Py_ssize_t *n_remainder, int *has_decimal)
{
    Py_ssize_t remainder;
    const void *data = (void *) PyString_AsChar(s);

    while (pos<end && Py_ISDIGIT(((char *) data)[pos]))
        ++pos;
    remainder = pos;

    /* Does remainder start with a decimal point? */
    *has_decimal = pos<end && ((char *) data)[remainder] == '.';

    /* Skip the decimal point. */
    if (*has_decimal)
        remainder++;

    *n_remainder = end - remainder;
}

/* not all fields of format are used.  for example, precision is
   unused.  should this take discrete params in order to be more clear
   about what it does?  or is passing a single format parameter easier
   and more efficient enough to justify a little obfuscation?
   Return -1 on error. */
static Py_ssize_t
calc_number_widths(NumberFieldWidths *spec, Py_ssize_t n_prefix,
                   Py_UCS1 sign_char, Py_ssize_t n_start,
                   Py_ssize_t n_end, Py_ssize_t n_remainder,
                   int has_decimal, const LocaleInfo *locale,
                   const InternalFormatSpec *format)
{
    Py_ssize_t n_non_digit_non_padding;
    Py_ssize_t n_padding;

    spec->n_digits = n_end - n_start - n_remainder - (has_decimal?1:0);
    spec->n_lpadding = 0;
    spec->n_prefix = n_prefix;
    spec->n_decimal = has_decimal ? PyString_Size(locale->decimal_point) : 0;
    spec->n_remainder = n_remainder;
    spec->n_spadding = 0;
    spec->n_rpadding = 0;
    spec->sign = '\0';
    spec->n_sign = 0;

    /* the output will look like:
       |                                                                                         |
       | <lpadding> <sign> <prefix> <spadding> <grouped_digits> <decimal> <remainder> <rpadding> |
       |                                                                                         |

       sign is computed from format->sign and the actual
       sign of the number

       prefix is given (it's for the '0x' prefix)

       digits is already known

       the total width is either given, or computed from the
       actual digits

       only one of lpadding, spadding, and rpadding can be non-zero,
       and it's calculated from the width and other fields
    */

    /* compute the various parts we're going to write */
    switch (format->sign) {
    case '+':
        /* always put a + or - */
        spec->n_sign = 1;
        spec->sign = (sign_char == '-' ? '-' : '+');
        break;
    case ' ':
        spec->n_sign = 1;
        spec->sign = (sign_char == '-' ? '-' : ' ');
        break;
    default:
        /* Not specified, or the default (-) */
        if (sign_char == '-') {
            spec->n_sign = 1;
            spec->sign = '-';
        }
    }

    /* The number of chars used for non-digits and non-padding. */
    n_non_digit_non_padding = spec->n_sign + spec->n_prefix + spec->n_decimal +
        spec->n_remainder;

    /* min_width can go negative, that's okay. format->width == -1 means
       we don't care. */
    if (format->fill_char == '0' && format->align == '=')
        spec->n_min_width = format->width - n_non_digit_non_padding;
    else
        spec->n_min_width = 0;

    if (spec->n_digits == 0)
        /* This case only occurs when using 'c' formatting, we need
           to special case it because the grouping code always wants
           to have at least one character. */
        spec->n_grouped_digits = 0;
    else {

        spec->n_grouped_digits = _PyString_InsertThousandsGrouping(
            NULL, 0,
            NULL, 0, spec->n_digits,
            spec->n_min_width,
            locale->grouping, locale->thousands_sep);
        if (spec->n_grouped_digits == -1) {
            return -1;
        }
    }

    /* Given the desired width and the total of digit and non-digit
       space we consume, see if we need any padding. format->width can
       be negative (meaning no padding), but this code still works in
       that case. */
    n_padding = format->width -
                        (n_non_digit_non_padding + spec->n_grouped_digits);
    if (n_padding > 0) {
        /* Some padding is needed. Determine if it's left, space, or right. */
        switch (format->align) {
        case '<':
            spec->n_rpadding = n_padding;
            break;
        case '^':
            spec->n_lpadding = n_padding / 2;
            spec->n_rpadding = n_padding - spec->n_lpadding;
            break;
        case '=':
            spec->n_spadding = n_padding;
            break;
        case '>':
            spec->n_lpadding = n_padding;
            break;
        default:
            /* Shouldn't get here */
            Py_UNREACHABLE();
        }
    }

    return spec->n_lpadding + spec->n_sign + spec->n_prefix +
        spec->n_spadding + spec->n_grouped_digits + spec->n_decimal +
        spec->n_remainder + spec->n_rpadding;
}

/* Fill in the digit parts of a number's string representation,
   as determined in calc_number_widths().
   Return -1 on error, or 0 on success. */
static int
fill_number(_PyStringWriter *writer, const NumberFieldWidths *spec,
            PyObject *digits, Py_ssize_t d_start,
            PyObject *prefix, Py_ssize_t p_start,
            Py_UCS1 fill_char,
            LocaleInfo *locale, int toupper)
{
    /* Used to keep track of digits, decimal, and remainder. */
    Py_ssize_t d_pos = d_start;
    const void *data = writer->data;
    Py_ssize_t r;

    if (spec->n_lpadding) {
        PyString_Fill(writer->buffer,
                            writer->pos, spec->n_lpadding, fill_char);
        writer->pos += spec->n_lpadding;
    }
    if (spec->n_sign == 1) {
      ((char *) data)[writer->pos] = spec->sign;
      writer->pos++;
    }
    if (spec->n_prefix) {
        PyString_CopyCharacters(writer->buffer, writer->pos,
                                      prefix, p_start,
                                      spec->n_prefix);
        if (toupper) {
            Py_ssize_t t;
            for (t = 0; t < spec->n_prefix; t++) {
	      Py_UCS1 c = ((char *) data)[writer->pos + t];
                c = Py_TOUPPER(c);
                assert (c <= 127);
		((char *) data)[writer->pos + t] = c;
            }
        }
        writer->pos += spec->n_prefix;
    }
    if (spec->n_spadding) {
        PyString_Fill(writer->buffer,
                            writer->pos, spec->n_spadding, fill_char);
        writer->pos += spec->n_spadding;
    }

    /* Only for type 'c' special case, it has no digits. */
    if (spec->n_digits != 0) {
        /* Fill the digits with InsertThousandsGrouping. */
        r = _PyString_InsertThousandsGrouping(
                writer, spec->n_grouped_digits,
                digits, d_pos, spec->n_digits,
                spec->n_min_width,
                locale->grouping, locale->thousands_sep);
        if (r == -1)
            return -1;
        assert(r == spec->n_grouped_digits);
        d_pos += spec->n_digits;
    }
    if (toupper) {
        Py_ssize_t t;
        for (t = 0; t < spec->n_grouped_digits; t++) {
	  Py_UCS1 c = ((char *) data)[writer->pos+t];
            c = Py_TOUPPER(c);
            if (c > 127) {
                PyErr_SetString(PyExc_SystemError, "non-ascii grouped digit");
                return -1;
            }
	    ((char *) data)[writer->pos+t] = c;
        }
    }
    writer->pos += spec->n_grouped_digits;

    if (spec->n_decimal) {
        PyString_CopyCharacters(
            writer->buffer, writer->pos,
            locale->decimal_point, 0, spec->n_decimal);
        writer->pos += spec->n_decimal;
        d_pos += 1;
    }

    if (spec->n_remainder) {
        PyString_CopyCharacters(
            writer->buffer, writer->pos,
            digits, d_pos, spec->n_remainder);
        writer->pos += spec->n_remainder;
        /* d_pos += spec->n_remainder; */
    }

    if (spec->n_rpadding) {
        PyString_Fill(writer->buffer,
                            writer->pos, spec->n_rpadding,
                            fill_char);
        writer->pos += spec->n_rpadding;
    }
    return 0;
}

static const char no_grouping[1] = {CHAR_MAX};

/* Find the decimal point character(s?), thousands_separator(s?), and
   grouping description, either for the current locale if type is
   LT_CURRENT_LOCALE, a hard-coded locale if LT_DEFAULT_LOCALE or
   LT_UNDERSCORE_LOCALE/LT_UNDER_FOUR_LOCALE, or none if LT_NO_LOCALE. */
static int
get_locale_info(enum LocaleType type, LocaleInfo *locale_info)
{
  locale_info->decimal_point = PyString_FromOrdinal('.');
  locale_info->thousands_sep = PyString_New(0);
  if (!locale_info->decimal_point || !locale_info->thousands_sep)
    return -1;
  locale_info->grouping = no_grouping;
  return 0;
}

static void
free_locale_info(LocaleInfo *locale_info)
{
    Py_XDECREF(locale_info->decimal_point);
    Py_XDECREF(locale_info->thousands_sep);
    PyMem_Free(locale_info->grouping_buffer);
}

/************************************************************************/
/*********** string formatting ******************************************/
/************************************************************************/

static int
format_string_internal(PyObject *value, const InternalFormatSpec *format,
                       _PyStringWriter *writer)
{
    Py_ssize_t lpad;
    Py_ssize_t rpad;
    Py_ssize_t total;
    Py_ssize_t len;
    int result = -1;

    len = PyString_Size(value);

    /* sign is not allowed on strings */
    if (format->sign != '\0') {
        PyErr_SetString(PyExc_ValueError,
                        "Sign not allowed in string format specifier");
        goto done;
    }

    /* alternate is not allowed on strings */
    if (format->alternate) {
        PyErr_SetString(PyExc_ValueError,
                        "Alternate form (#) not allowed in string format "
                        "specifier");
        goto done;
    }

    /* '=' alignment not allowed on strings */
    if (format->align == '=') {
        PyErr_SetString(PyExc_ValueError,
                        "'=' alignment not allowed "
                        "in string format specifier");
        goto done;
    }

    if ((format->width == -1 || format->width <= len)
        && (format->precision == -1 || format->precision >= len)) {
        /* Fast path */
        return _PyStringWriter_WriteStr(writer, value);
    }

    /* if precision is specified, output no more that format.precision
       characters */
    if (format->precision >= 0 && len >= format->precision) {
        len = format->precision;
    }

    calc_padding(len, format->width, format->align, &lpad, &rpad, &total);

    /* allocate the resulting string */
    if (_PyStringWriter_Prepare(writer, total) == -1)
        goto done;

    /* Write into that space. First the padding. */
    result = fill_padding(writer, len, format->fill_char, lpad, rpad);
    if (result == -1)
        goto done;

    /* Then the source string. */
    if (len) {
        PyString_CopyCharacters(writer->buffer, writer->pos,
                                      value, 0, len);
    }
    writer->pos += (len + rpad);
    result = 0;

done:
    return result;
}


/************************************************************************/
/*********** long formatting ********************************************/
/************************************************************************/

static int
format_long_internal(PyObject *value, const InternalFormatSpec *format,
                     _PyStringWriter *writer)
{
    int result = -1;
    PyObject *tmp = NULL;
    Py_ssize_t inumeric_chars;
    Py_UCS1 sign_char = '\0';
    Py_ssize_t n_digits;       /* count of digits need from the computed
                                  string */
    Py_ssize_t n_remainder = 0; /* Used only for 'c' formatting, which
                                   produces non-digits */
    Py_ssize_t n_prefix = 0;   /* Count of prefix chars, (e.g., '0x') */
    Py_ssize_t n_total;
    Py_ssize_t prefix = 0;
    NumberFieldWidths spec;
    long x;

    /* Locale settings, either from the actual locale or
       from a hard-code pseudo-locale */
    LocaleInfo locale = LocaleInfo_STATIC_INIT;

    /* no precision allowed on integers */
    if (format->precision != -1) {
        PyErr_SetString(PyExc_ValueError,
                        "Precision not allowed in integer format specifier");
        goto done;
    }

    /* special case for character formatting */
    if (format->type == 'c') {
        /* error to specify a sign */
        if (format->sign != '\0') {
            PyErr_SetString(PyExc_ValueError,
                            "Sign not allowed with integer"
                            " format specifier 'c'");
            goto done;
        }
        /* error to request alternate format */
        if (format->alternate) {
            PyErr_SetString(PyExc_ValueError,
                            "Alternate form (#) not allowed with integer"
                            " format specifier 'c'");
            goto done;
        }

        /* taken from unicodeobject.c formatchar() */
        /* Integer input truncated to a character */
        x = PyLong_AsLong(value);
        if (x == -1 && PyErr_Occurred())
            goto done;
        if (x < 0 || x > 0x10ffff) {
            PyErr_SetString(PyExc_OverflowError,
                            "%c arg not in range(0x110000)");
            goto done;
        }
        tmp = PyString_FromOrdinal(x);
        inumeric_chars = 0;
        n_digits = 1;

        /* As a sort-of hack, we tell calc_number_widths that we only
           have "remainder" characters. calc_number_widths thinks
           these are characters that don't get formatted, only copied
           into the output string. We do this for 'c' formatting,
           because the characters are likely to be non-digits. */
        n_remainder = 1;
    }
    else {
        int base;
        int leading_chars_to_skip = 0;  /* Number of characters added by
                                           PyNumber_ToBase that we want to
                                           skip over. */

        /* Compute the base and how many characters will be added by
           PyNumber_ToBase */
        switch (format->type) {
        case 'b':
            base = 2;
            leading_chars_to_skip = 2; /* 0b */
            break;
        case 'o':
            base = 8;
            leading_chars_to_skip = 2; /* 0o */
            break;
        case 'x':
        case 'X':
            base = 16;
            leading_chars_to_skip = 2; /* 0x */
            break;
        default:  /* shouldn't be needed, but stops a compiler warning */
        case 'd':
        case 'n':
            base = 10;
            break;
        }

        if (format->sign != '+' && format->sign != ' '
            && format->width == -1
            && format->type != 'X' && format->type != 'n'
            && !format->thousands_separators
            && PyLong_CheckExact(value))
        {
            /* Fast path */
            return _PyLong_FormatWriter(writer, value, base, format->alternate);
        }

        /* The number of prefix chars is the same as the leading
           chars to skip */
        if (format->alternate)
            n_prefix = leading_chars_to_skip;

        /* Do the hard part, converting to a string in a given base */
        tmp = _PyLong_Format(value, base);
        if (tmp == NULL)
            goto done;

        inumeric_chars = 0;
        n_digits = PyString_Size(tmp);

        prefix = inumeric_chars;

        /* Is a sign character present in the output?  If so, remember it
           and skip it */
        if (PyString_ReadChar(tmp, inumeric_chars) == '-') {
            sign_char = '-';
            ++prefix;
            ++leading_chars_to_skip;
        }

        /* Skip over the leading chars (0x, 0b, etc.) */
        n_digits -= leading_chars_to_skip;
        inumeric_chars += leading_chars_to_skip;
    }

    /* Determine the grouping, separator, and decimal point, if any. */
    if (get_locale_info(format->type == 'n' ? LT_CURRENT_LOCALE :
                        format->thousands_separators,
                        &locale) == -1)
        goto done;

    /* Calculate how much memory we'll need. */
    n_total = calc_number_widths(&spec, n_prefix, sign_char, inumeric_chars,
                                 inumeric_chars + n_digits, n_remainder, 0,
                                 &locale, format);
    if (n_total == -1) {
        goto done;
    }

    /* Allocate the memory. */
    if (_PyStringWriter_Prepare(writer, n_total) == -1)
        goto done;

    /* Populate the memory. */
    result = fill_number(writer, &spec,
                         tmp, inumeric_chars,
                         tmp, prefix, format->fill_char,
                         &locale, format->type == 'X');

done:
    Py_XDECREF(tmp);
    free_locale_info(&locale);
    return result;
}

/************************************************************************/
/*********** float formatting *******************************************/
/************************************************************************/

/* much of this is taken from unicodeobject.c */
static int
format_float_internal(PyObject *value,
                      const InternalFormatSpec *format,
                      _PyStringWriter *writer)
{
    char *buf = NULL;       /* buffer returned from PyOS_double_to_string */
    Py_ssize_t n_digits;
    Py_ssize_t n_remainder;
    Py_ssize_t n_total;
    int has_decimal;
    double val;
    int precision, default_precision = 6;
    Py_UCS1 type = format->type;
    int add_pct = 0;
    Py_ssize_t index;
    NumberFieldWidths spec;
    int flags = 0;
    int result = -1;
    Py_UCS1 sign_char = '\0';
    int float_type; /* Used to see if we have a nan, inf, or regular float. */
    PyObject *unicode_tmp = NULL;

    /* Locale settings, either from the actual locale or
       from a hard-code pseudo-locale */
    LocaleInfo locale = LocaleInfo_STATIC_INIT;

    if (format->precision > INT_MAX) {
        PyErr_SetString(PyExc_ValueError, "precision too big");
        goto done;
    }
    precision = (int)format->precision;

    if (format->alternate)
        flags |= Py_DTSF_ALT;

    if (type == '\0') {
        /* Omitted type specifier.  Behaves in the same way as repr(x)
           and str(x) if no precision is given, else like 'g', but with
           at least one digit after the decimal point. */
        flags |= Py_DTSF_ADD_DOT_0;
        type = 'r';
        default_precision = 0;
    }

    if (type == 'n')
        /* 'n' is the same as 'g', except for the locale used to
           format the result. We take care of that later. */
        type = 'g';

    val = PyFloat_AsDouble(value);
    if (val == -1.0 && PyErr_Occurred())
        goto done;

    if (type == '%') {
        type = 'f';
        val *= 100;
        add_pct = 1;
    }

    if (precision < 0)
        precision = default_precision;
    else if (type == 'r')
        type = 'g';

    /* Cast "type", because if we're in unicode we need to pass an
       8-bit char. This is safe, because we've restricted what "type"
       can be. */
    buf = PyOS_double_to_string(val, (char)type, precision, flags,
                                &float_type);
    if (buf == NULL)
        goto done;
    n_digits = strlen(buf);

    if (add_pct) {
        /* We know that buf has a trailing zero (since we just called
           strlen() on it), and we don't use that fact any more. So we
           can just write over the trailing zero. */
        buf[n_digits] = '%';
        n_digits += 1;
    }

    if (format->sign != '+' && format->sign != ' '
        && format->width == -1
        && format->type != 'n'
        && !format->thousands_separators)
    {
        /* Fast path */
        result = _PyStringWriter_WriteCString(writer, buf, n_digits);
        PyMem_Free(buf);
        return result;
    }

    /* Since there is no unicode version of PyOS_double_to_string,
       just use the 8 bit version and then convert to unicode. */
    unicode_tmp = PyString_FromStringAndSize(buf, n_digits);
    PyMem_Free(buf);
    if (unicode_tmp == NULL)
        goto done;

    /* Is a sign character present in the output?  If so, remember it
       and skip it */
    index = 0;
    if (PyString_ReadChar(unicode_tmp, index) == '-') {
        sign_char = '-';
        ++index;
        --n_digits;
    }

    /* Determine if we have any "remainder" (after the digits, might include
       decimal or exponent or both (or neither)) */
    parse_number(unicode_tmp, index, index + n_digits, &n_remainder, &has_decimal);

    /* Determine the grouping, separator, and decimal point, if any. */
    if (get_locale_info(format->type == 'n' ? LT_CURRENT_LOCALE :
                        format->thousands_separators,
                        &locale) == -1)
        goto done;

    /* Calculate how much memory we'll need. */
    n_total = calc_number_widths(&spec, 0, sign_char, index,
                                 index + n_digits, n_remainder, has_decimal,
                                 &locale, format);
    if (n_total == -1) {
        goto done;
    }

    /* Allocate the memory. */
    if (_PyStringWriter_Prepare(writer, n_total) == -1)
        goto done;

    /* Populate the memory. */
    result = fill_number(writer, &spec,
                         unicode_tmp, index,
                         NULL, 0, format->fill_char,
                         &locale, 0);

done:
    Py_XDECREF(unicode_tmp);
    free_locale_info(&locale);
    return result;
}

/************************************************************************/
/*********** built in formatters ****************************************/
/************************************************************************/
static int
format_obj(PyObject *obj, _PyStringWriter *writer)
{
    PyObject *str;
    int err;

    str = PyObject_Str(obj);
    if (str == NULL)
        return -1;
    err = _PyStringWriter_WriteStr(writer, str);
    Py_DECREF(str);
    return err;
}

int
_PyString_FormatAdvancedWriter(_PyStringWriter *writer,
                                PyObject *obj,
                                PyObject *format_spec,
                                Py_ssize_t start, Py_ssize_t end)
{
    InternalFormatSpec format;

    assert(PyString_Check(obj));

    /* check for the special case of zero length format spec, make
       it equivalent to str(obj) */
    if (start == end) {
        if (PyString_CheckExact(obj))
            return _PyStringWriter_WriteStr(writer, obj);
        else
            return format_obj(obj, writer);
    }

    /* parse the format_spec */
    if (!parse_internal_render_format_spec(format_spec, start, end,
                                           &format, 's', '<'))
        return -1;

    /* type conversion? */
    switch (format.type) {
    case 's':
        /* no type conversion needed, already a string.  do the formatting */
        return format_string_internal(obj, &format, writer);
    default:
        /* unknown */
        unknown_presentation_type(format.type, Py_TYPE(obj)->tp_name);
        return -1;
    }
}

int
_PyLong_FormatAdvancedWriter(_PyStringWriter *writer,
                             PyObject *obj,
                             PyObject *format_spec,
                             Py_ssize_t start, Py_ssize_t end)
{
    PyObject *tmp = NULL;
    InternalFormatSpec format;
    int result = -1;

    /* check for the special case of zero length format spec, make
       it equivalent to str(obj) */
    if (start == end) {
        if (PyLong_CheckExact(obj))
            return _PyLong_FormatWriter(writer, obj, 10, 0);
        else
            return format_obj(obj, writer);
    }

    /* parse the format_spec */
    if (!parse_internal_render_format_spec(format_spec, start, end,
                                           &format, 'd', '>'))
        goto done;

    /* type conversion? */
    switch (format.type) {
    case 'b':
    case 'c':
    case 'd':
    case 'o':
    case 'x':
    case 'X':
    case 'n':
        /* no type conversion needed, already an int.  do the formatting */
        result = format_long_internal(obj, &format, writer);
        break;

    case 'e':
    case 'E':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
    case '%':
        /* convert to float */
        tmp = PyNumber_Float(obj);
        if (tmp == NULL)
            goto done;
        result = format_float_internal(tmp, &format, writer);
        break;

    default:
        /* unknown */
        unknown_presentation_type(format.type, Py_TYPE(obj)->tp_name);
        goto done;
    }

done:
    Py_XDECREF(tmp);
    return result;
}

int
_PyFloat_FormatAdvancedWriter(_PyStringWriter *writer,
                              PyObject *obj,
                              PyObject *format_spec,
                              Py_ssize_t start, Py_ssize_t end)
{
    InternalFormatSpec format;

    /* check for the special case of zero length format spec, make
       it equivalent to str(obj) */
    if (start == end)
        return format_obj(obj, writer);

    /* parse the format_spec */
    if (!parse_internal_render_format_spec(format_spec, start, end,
                                           &format, '\0', '>'))
        return -1;

    /* type conversion? */
    switch (format.type) {
    case '\0': /* No format code: like 'g', but with at least one decimal. */
    case 'e':
    case 'E':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
    case 'n':
    case '%':
        /* no conversion, already a float.  do the formatting */
        return format_float_internal(obj, &format, writer);

    default:
        /* unknown */
        unknown_presentation_type(format.type, Py_TYPE(obj)->tp_name);
        return -1;
    }
}
