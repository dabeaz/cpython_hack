#include "Python.h"
#include "pycore_fileutils.h"
#include "osdefs.h"               // SEP
#include <locale.h>

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#ifdef O_CLOEXEC
/* Does open() support the O_CLOEXEC flag? Possible values:

   -1: unknown
    0: open() ignores O_CLOEXEC flag, ex: Linux kernel older than 2.6.23
    1: open() supports O_CLOEXEC flag, close-on-exec is set

   The flag is used by _Py_open(), _Py_open_noraise(), io.FileIO
   and os.open(). */
int _Py_open_cloexec_works = -1;
#endif


static int
get_surrogateescape(_Py_error_handler errors, int *surrogateescape)
{
    switch (errors)
    {
    case _Py_ERROR_STRICT:
        *surrogateescape = 0;
        return 0;
    case _Py_ERROR_SURROGATEESCAPE:
        *surrogateescape = 1;
        return 0;
    default:
        return -1;
    }
}


PyObject *
_Py_device_encoding(int fd)
{
    int valid;
    
    valid = isatty(fd);
    
    if (!valid)
        Py_RETURN_NONE;
#if defined(CODESET)
    {
        char *codeset = nl_langinfo(CODESET);
        if (codeset != NULL && codeset[0] != 0)
            return PyUnicode_FromString(codeset);
    }
#endif
    Py_RETURN_NONE;
}

#if !defined(_Py_FORCE_UTF8_FS_ENCODING)

#define USE_FORCE_ASCII

extern int _Py_normalize_encoding(const char *, char *, size_t);

/* Workaround FreeBSD and OpenIndiana locale encoding issue with the C locale
   and POSIX locale. nl_langinfo(CODESET) announces an alias of the
   ASCII encoding, whereas mbstowcs() and wcstombs() functions use the
   ISO-8859-1 encoding. The problem is that os.fsencode() and os.fsdecode() use
   locale.getpreferredencoding() codec. For example, if command line arguments
   are decoded by mbstowcs() and encoded back by os.fsencode(), we get a
   UnicodeEncodeError instead of retrieving the original byte string.

   The workaround is enabled if setlocale(LC_CTYPE, NULL) returns "C",
   nl_langinfo(CODESET) announces "ascii" (or an alias to ASCII), and at least
   one byte in range 0x80-0xff can be decoded from the locale encoding. The
   workaround is also enabled on error, for example if getting the locale
   failed.

   On HP-UX with the C locale or the POSIX locale, nl_langinfo(CODESET)
   announces "roman8" but mbstowcs() uses Latin1 in practice. Force also the
   ASCII encoding in this case.

   Values of force_ascii:

       1: the workaround is used: Py_EncodeLocale() uses
          encode_ascii_surrogateescape() and Py_DecodeLocale() uses
          decode_ascii()
       0: the workaround is not used: Py_EncodeLocale() uses wcstombs() and
          Py_DecodeLocale() uses mbstowcs()
      -1: unknown, need to call check_force_ascii() to get the value
*/
static int force_ascii = -1;

static int
check_force_ascii(void)
{
    char *loc = setlocale(LC_CTYPE, NULL);
    if (loc == NULL) {
        goto error;
    }
    if (strcmp(loc, "C") != 0 && strcmp(loc, "POSIX") != 0) {
        /* the LC_CTYPE locale is different than C and POSIX */
        return 0;
    }

#if defined(HAVE_LANGINFO_H) && defined(CODESET)
    const char *codeset = nl_langinfo(CODESET);
    if (!codeset || codeset[0] == '\0') {
        /* CODESET is not set or empty */
        goto error;
    }

    char encoding[20];   /* longest name: "iso_646.irv_1991\0" */
    if (!_Py_normalize_encoding(codeset, encoding, sizeof(encoding))) {
        goto error;
    }

#ifdef __hpux
    if (strcmp(encoding, "roman8") == 0) {
        unsigned char ch;
        wchar_t wch;
        size_t res;

        ch = (unsigned char)0xA7;
        res = mbstowcs(&wch, (char*)&ch, 1);
        if (res != (size_t)-1 && wch == L'\xA7') {
            /* On HP-UX withe C locale or the POSIX locale,
               nl_langinfo(CODESET) announces "roman8", whereas mbstowcs() uses
               Latin1 encoding in practice. Force ASCII in this case.

               Roman8 decodes 0xA7 to U+00CF. Latin1 decodes 0xA7 to U+00A7. */
            return 1;
        }
    }
#else
    const char* ascii_aliases[] = {
        "ascii",
        /* Aliases from Lib/encodings/aliases.py */
        "646",
        "ansi_x3.4_1968",
        "ansi_x3.4_1986",
        "ansi_x3_4_1968",
        "cp367",
        "csascii",
        "ibm367",
        "iso646_us",
        "iso_646.irv_1991",
        "iso_ir_6",
        "us",
        "us_ascii",
        NULL
    };

    int is_ascii = 0;
    for (const char **alias=ascii_aliases; *alias != NULL; alias++) {
        if (strcmp(encoding, *alias) == 0) {
            is_ascii = 1;
            break;
        }
    }
    if (!is_ascii) {
        /* nl_langinfo(CODESET) is not "ascii" or an alias of ASCII */
        return 0;
    }

    for (unsigned int i=0x80; i<=0xff; i++) {
        char ch[1];
        wchar_t wch[1];
        size_t res;

        unsigned uch = (unsigned char)i;
        ch[0] = (char)uch;
        res = mbstowcs(wch, ch, 1);
        if (res != (size_t)-1) {
            /* decoding a non-ASCII character from the locale encoding succeed:
               the locale encoding is not ASCII, force ASCII */
            return 1;
        }
    }
    /* None of the bytes in the range 0x80-0xff can be decoded from the locale
       encoding: the locale encoding is really ASCII */
#endif   /* !defined(__hpux) */
    return 0;
#else
    /* nl_langinfo(CODESET) is not available: always force ASCII */
    return 1;
#endif   /* defined(HAVE_LANGINFO_H) && defined(CODESET) */

error:
    /* if an error occurred, force the ASCII encoding */
    return 1;
}


int
_Py_GetForceASCII(void)
{
    if (force_ascii == -1) {
        force_ascii = check_force_ascii();
    }
    return force_ascii;
}


void
_Py_ResetForceASCII(void)
{
    force_ascii = -1;
}


static int
encode_ascii(const wchar_t *text, char **str,
             size_t *error_pos, const char **reason,
             int raw_malloc, _Py_error_handler errors)
{
    char *result = NULL, *out;
    size_t len, i;
    wchar_t ch;

    int surrogateescape;
    if (get_surrogateescape(errors, &surrogateescape) < 0) {
        return -3;
    }

    len = wcslen(text);

    /* +1 for NULL byte */
    if (raw_malloc) {
        result = PyMem_RawMalloc(len + 1);
    }
    else {
        result = PyMem_Malloc(len + 1);
    }
    if (result == NULL) {
        return -1;
    }

    out = result;
    for (i=0; i<len; i++) {
        ch = text[i];

        if (ch <= 0x7f) {
            /* ASCII character */
            *out++ = (char)ch;
        }
        else if (surrogateescape && 0xdc80 <= ch && ch <= 0xdcff) {
            /* UTF-8b surrogate */
            *out++ = (char)(ch - 0xdc00);
        }
        else {
            if (raw_malloc) {
                PyMem_RawFree(result);
            }
            else {
                PyMem_Free(result);
            }
            if (error_pos != NULL) {
                *error_pos = i;
            }
            if (reason) {
                *reason = "encoding error";
            }
            return -2;
        }
    }
    *out = '\0';
    *str = result;
    return 0;
}
#else
int
_Py_GetForceASCII(void)
{
    return 0;
}

void
_Py_ResetForceASCII(void)
{
    /* nothing to do */
}
#endif   /* !defined(_Py_FORCE_UTF8_FS_ENCODING) */


#if !defined(HAVE_MBRTOWC) || defined(USE_FORCE_ASCII)
static int
decode_ascii(const char *arg, wchar_t **wstr, size_t *wlen,
             const char **reason, _Py_error_handler errors)
{
    wchar_t *res;
    unsigned char *in;
    wchar_t *out;
    size_t argsize = strlen(arg) + 1;

    int surrogateescape;
    if (get_surrogateescape(errors, &surrogateescape) < 0) {
        return -3;
    }

    if (argsize > PY_SSIZE_T_MAX / sizeof(wchar_t)) {
        return -1;
    }
    res = PyMem_RawMalloc(argsize * sizeof(wchar_t));
    if (!res) {
        return -1;
    }

    out = res;
    for (in = (unsigned char*)arg; *in; in++) {
        unsigned char ch = *in;
        if (ch < 128) {
            *out++ = ch;
        }
        else {
            if (!surrogateescape) {
                PyMem_RawFree(res);
                if (wlen) {
                    *wlen = in - (unsigned char*)arg;
                }
                if (reason) {
                    *reason = "decoding error";
                }
                return -2;
            }
            *out++ = 0xdc00 + ch;
        }
    }
    *out = 0;

    if (wlen != NULL) {
        *wlen = out - res;
    }
    *wstr = res;
    return 0;
}
#endif   /* !HAVE_MBRTOWC */

static int
decode_current_locale(const char* arg, wchar_t **wstr, size_t *wlen,
                      const char **reason, _Py_error_handler errors)
{
    wchar_t *res;
    size_t argsize;
    size_t count;
#ifdef HAVE_MBRTOWC
    unsigned char *in;
    wchar_t *out;
    mbstate_t mbs;
#endif

    int surrogateescape;
    if (get_surrogateescape(errors, &surrogateescape) < 0) {
        return -3;
    }

#ifdef HAVE_BROKEN_MBSTOWCS
    /* Some platforms have a broken implementation of
     * mbstowcs which does not count the characters that
     * would result from conversion.  Use an upper bound.
     */
    argsize = strlen(arg);
#else
    argsize = mbstowcs(NULL, arg, 0);
#endif
    if (argsize != (size_t)-1) {
        if (argsize > PY_SSIZE_T_MAX / sizeof(wchar_t) - 1) {
            return -1;
        }
        res = (wchar_t *)PyMem_RawMalloc((argsize + 1) * sizeof(wchar_t));
        if (!res) {
            return -1;
        }

        count = mbstowcs(res, arg, argsize + 1);
        if (count != (size_t)-1) {
            wchar_t *tmp;
            /* Only use the result if it contains no
               surrogate characters. */
            for (tmp = res; *tmp != 0 &&
                         !Py_UNICODE_IS_SURROGATE(*tmp); tmp++)
                ;
            if (*tmp == 0) {
                if (wlen != NULL) {
                    *wlen = count;
                }
                *wstr = res;
                return 0;
            }
        }
        PyMem_RawFree(res);
    }

    /* Conversion failed. Fall back to escaping with surrogateescape. */
#ifdef HAVE_MBRTOWC
    /* Try conversion with mbrtwoc (C99), and escape non-decodable bytes. */

    /* Overallocate; as multi-byte characters are in the argument, the
       actual output could use less memory. */
    argsize = strlen(arg) + 1;
    if (argsize > PY_SSIZE_T_MAX / sizeof(wchar_t)) {
        return -1;
    }
    res = (wchar_t*)PyMem_RawMalloc(argsize * sizeof(wchar_t));
    if (!res) {
        return -1;
    }

    in = (unsigned char*)arg;
    out = res;
    memset(&mbs, 0, sizeof mbs);
    while (argsize) {
        size_t converted = mbrtowc(out, (char*)in, argsize, &mbs);
        if (converted == 0) {
            /* Reached end of string; null char stored. */
            break;
        }

        if (converted == (size_t)-2) {
            /* Incomplete character. This should never happen,
               since we provide everything that we have -
               unless there is a bug in the C library, or I
               misunderstood how mbrtowc works. */
            goto decode_error;
        }

        if (converted == (size_t)-1) {
            if (!surrogateescape) {
                goto decode_error;
            }

            /* Conversion error. Escape as UTF-8b, and start over
               in the initial shift state. */
            *out++ = 0xdc00 + *in++;
            argsize--;
            memset(&mbs, 0, sizeof mbs);
            continue;
        }

        if (Py_UNICODE_IS_SURROGATE(*out)) {
            if (!surrogateescape) {
                goto decode_error;
            }

            /* Surrogate character.  Escape the original
               byte sequence with surrogateescape. */
            argsize -= converted;
            while (converted--) {
                *out++ = 0xdc00 + *in++;
            }
            continue;
        }
        /* successfully converted some bytes */
        in += converted;
        argsize -= converted;
        out++;
    }
    if (wlen != NULL) {
        *wlen = out - res;
    }
    *wstr = res;
    return 0;

decode_error:
    PyMem_RawFree(res);
    if (wlen) {
        *wlen = in - (unsigned char*)arg;
    }
    if (reason) {
        *reason = "decoding error";
    }
    return -2;
#else   /* HAVE_MBRTOWC */
    /* Cannot use C locale for escaping; manually escape as if charset
       is ASCII (i.e. escape all bytes > 128. This will still roundtrip
       correctly in the locale's charset, which must be an ASCII superset. */
    return decode_ascii(arg, wstr, wlen, reason, errors);
#endif   /* HAVE_MBRTOWC */
}


/* Decode a byte string from the locale encoding.

   Use the strict error handler if 'surrogateescape' is zero.  Use the
   surrogateescape error handler if 'surrogateescape' is non-zero: undecodable
   bytes are decoded as characters in range U+DC80..U+DCFF. If a byte sequence
   can be decoded as a surrogate character, escape the bytes using the
   surrogateescape error handler instead of decoding them.

   On success, return 0 and write the newly allocated wide character string into
   *wstr (use PyMem_RawFree() to free the memory). If wlen is not NULL, write
   the number of wide characters excluding the null character into *wlen.

   On memory allocation failure, return -1.

   On decoding error, return -2. If wlen is not NULL, write the start of
   invalid byte sequence in the input string into *wlen. If reason is not NULL,
   write the decoding error message into *reason.

   Return -3 if the error handler 'errors' is not supported.

   Use the Py_EncodeLocaleEx() function to encode the character string back to
   a byte string. */
int
_Py_DecodeLocaleEx(const char* arg, wchar_t **wstr, size_t *wlen,
                   const char **reason,
                   int current_locale, _Py_error_handler errors)
{
    if (current_locale) {
#ifdef _Py_FORCE_UTF8_LOCALE
        return _Py_DecodeUTF8Ex(arg, strlen(arg), wstr, wlen, reason,
                                errors);
#else
        return decode_current_locale(arg, wstr, wlen, reason, errors);
#endif
    }

#ifdef _Py_FORCE_UTF8_FS_ENCODING
    return _Py_DecodeUTF8Ex(arg, strlen(arg), wstr, wlen, reason,
                            errors);
#else
    int use_utf8 = (Py_UTF8Mode == 1);
    if (use_utf8) {
        return _Py_DecodeUTF8Ex(arg, strlen(arg), wstr, wlen, reason,
                                errors);
    }

#ifdef USE_FORCE_ASCII
    if (force_ascii == -1) {
        force_ascii = check_force_ascii();
    }

    if (force_ascii) {
        /* force ASCII encoding to workaround mbstowcs() issue */
        return decode_ascii(arg, wstr, wlen, reason, errors);
    }
#endif

    return decode_current_locale(arg, wstr, wlen, reason, errors);
#endif   /* !_Py_FORCE_UTF8_FS_ENCODING */
}


/* Decode a byte string from the locale encoding with the
   surrogateescape error handler: undecodable bytes are decoded as characters
   in range U+DC80..U+DCFF. If a byte sequence can be decoded as a surrogate
   character, escape the bytes using the surrogateescape error handler instead
   of decoding them.

   Return a pointer to a newly allocated wide character string, use
   PyMem_RawFree() to free the memory. If size is not NULL, write the number of
   wide characters excluding the null character into *size

   Return NULL on decoding error or memory allocation error. If *size* is not
   NULL, *size is set to (size_t)-1 on memory error or set to (size_t)-2 on
   decoding error.

   Decoding errors should never happen, unless there is a bug in the C
   library.

   Use the Py_EncodeLocale() function to encode the character string back to a
   byte string. */
wchar_t*
Py_DecodeLocale(const char* arg, size_t *wlen)
{
    wchar_t *wstr;
    int res = _Py_DecodeLocaleEx(arg, &wstr, wlen,
                                 NULL, 0,
                                 _Py_ERROR_SURROGATEESCAPE);
    if (res != 0) {
        assert(res != -3);
        if (wlen != NULL) {
            *wlen = (size_t)res;
        }
        return NULL;
    }
    return wstr;
}


static int
encode_current_locale(const wchar_t *text, char **str,
                      size_t *error_pos, const char **reason,
                      int raw_malloc, _Py_error_handler errors)
{
    const size_t len = wcslen(text);
    char *result = NULL, *bytes = NULL;
    size_t i, size, converted;
    wchar_t c, buf[2];

    int surrogateescape;
    if (get_surrogateescape(errors, &surrogateescape) < 0) {
        return -3;
    }

    /* The function works in two steps:
       1. compute the length of the output buffer in bytes (size)
       2. outputs the bytes */
    size = 0;
    buf[1] = 0;
    while (1) {
        for (i=0; i < len; i++) {
            c = text[i];
            if (c >= 0xdc80 && c <= 0xdcff) {
                if (!surrogateescape) {
                    goto encode_error;
                }
                /* UTF-8b surrogate */
                if (bytes != NULL) {
                    *bytes++ = c - 0xdc00;
                    size--;
                }
                else {
                    size++;
                }
                continue;
            }
            else {
                buf[0] = c;
                if (bytes != NULL) {
                    converted = wcstombs(bytes, buf, size);
                }
                else {
                    converted = wcstombs(NULL, buf, 0);
                }
                if (converted == (size_t)-1) {
                    goto encode_error;
                }
                if (bytes != NULL) {
                    bytes += converted;
                    size -= converted;
                }
                else {
                    size += converted;
                }
            }
        }
        if (result != NULL) {
            *bytes = '\0';
            break;
        }

        size += 1; /* nul byte at the end */
        if (raw_malloc) {
            result = PyMem_RawMalloc(size);
        }
        else {
            result = PyMem_Malloc(size);
        }
        if (result == NULL) {
            return -1;
        }
        bytes = result;
    }
    *str = result;
    return 0;

encode_error:
    if (raw_malloc) {
        PyMem_RawFree(result);
    }
    else {
        PyMem_Free(result);
    }
    if (error_pos != NULL) {
        *error_pos = i;
    }
    if (reason) {
        *reason = "encoding error";
    }
    return -2;
}


/* Encode a string to the locale encoding.

   Parameters:

   * raw_malloc: if non-zero, allocate memory using PyMem_RawMalloc() instead
     of PyMem_Malloc().
   * current_locale: if non-zero, use the current LC_CTYPE, otherwise use
     Python filesystem encoding.
   * errors: error handler like "strict" or "surrogateescape".

   Return value:

    0: success, *str is set to a newly allocated decoded string.
   -1: memory allocation failure
   -2: encoding error, set *error_pos and *reason (if set).
   -3: the error handler 'errors' is not supported.
 */
static int
encode_locale_ex(const wchar_t *text, char **str, size_t *error_pos,
                 const char **reason,
                 int raw_malloc, int current_locale, _Py_error_handler errors)
{
    if (current_locale) {
#ifdef _Py_FORCE_UTF8_LOCALE
        return _Py_EncodeUTF8Ex(text, str, error_pos, reason,
                                raw_malloc, errors);
#else
        return encode_current_locale(text, str, error_pos, reason,
                                     raw_malloc, errors);
#endif
    }

#ifdef _Py_FORCE_UTF8_FS_ENCODING
    return _Py_EncodeUTF8Ex(text, str, error_pos, reason,
                            raw_malloc, errors);
#else
    int use_utf8 = (Py_UTF8Mode == 1);
    if (use_utf8) {
        return _Py_EncodeUTF8Ex(text, str, error_pos, reason,
                                raw_malloc, errors);
    }

#ifdef USE_FORCE_ASCII
    if (force_ascii == -1) {
        force_ascii = check_force_ascii();
    }

    if (force_ascii) {
        return encode_ascii(text, str, error_pos, reason,
                            raw_malloc, errors);
    }
#endif

    return encode_current_locale(text, str, error_pos, reason,
                                 raw_malloc, errors);
#endif   /* _Py_FORCE_UTF8_FS_ENCODING */
}

static char*
encode_locale(const wchar_t *text, size_t *error_pos,
              int raw_malloc, int current_locale)
{
    char *str;
    int res = encode_locale_ex(text, &str, error_pos, NULL,
                               raw_malloc, current_locale,
                               _Py_ERROR_SURROGATEESCAPE);
    if (res != -2 && error_pos) {
        *error_pos = (size_t)-1;
    }
    if (res != 0) {
        return NULL;
    }
    return str;
}

/* Encode a wide character string to the locale encoding with the
   surrogateescape error handler: surrogate characters in the range
   U+DC80..U+DCFF are converted to bytes 0x80..0xFF.

   Return a pointer to a newly allocated byte string, use PyMem_Free() to free
   the memory. Return NULL on encoding or memory allocation error.

   If error_pos is not NULL, *error_pos is set to (size_t)-1 on success, or set
   to the index of the invalid character on encoding error.

   Use the Py_DecodeLocale() function to decode the bytes string back to a wide
   character string. */
char*
Py_EncodeLocale(const wchar_t *text, size_t *error_pos)
{
    return encode_locale(text, error_pos, 0, 0);
}


/* Similar to Py_EncodeLocale(), but result must be freed by PyMem_RawFree()
   instead of PyMem_Free(). */
char*
_Py_EncodeLocaleRaw(const wchar_t *text, size_t *error_pos)
{
    return encode_locale(text, error_pos, 1, 0);
}


int
_Py_EncodeLocaleEx(const wchar_t *text, char **str,
                   size_t *error_pos, const char **reason,
                   int current_locale, _Py_error_handler errors)
{
    return encode_locale_ex(text, str, error_pos, reason, 1,
                            current_locale, errors);
}

/* Return information about a file.

   On POSIX, use fstat().

   On Windows, use GetFileType() and GetFileInformationByHandle() which support
   files larger than 2 GiB.  fstat() may fail with EOVERFLOW on files larger
   than 2 GiB because the file size type is a signed 32-bit integer: see issue
   #23152.

   On Windows, set the last Windows error and return nonzero on error. On
   POSIX, set errno and return nonzero on error. Fill status and return 0 on
   success. */
int
_Py_fstat_noraise(int fd, struct _Py_stat_struct *status)
{
    return fstat(fd, status);
}

/* Return information about a file.

   On POSIX, use fstat().

   On Windows, use GetFileType() and GetFileInformationByHandle() which support
   files larger than 2 GiB.  fstat() may fail with EOVERFLOW on files larger
   than 2 GiB because the file size type is a signed 32-bit integer: see issue
   #23152.

   Raise an exception and return -1 on error. On Windows, set the last Windows
   error on error. On POSIX, set errno on error. Fill status and return 0 on
   success.

   Release the GIL to call GetFileType() and GetFileInformationByHandle(), or
   to call fstat(). The caller must hold the GIL. */
int
_Py_fstat(int fd, struct _Py_stat_struct *status)
{
    int res;

    res = _Py_fstat_noraise(fd, status);
    

    if (res != 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    return 0;
}

/* Call _wstat() on Windows, or encode the path to the filesystem encoding and
   call stat() otherwise. Only fill st_mode attribute on Windows.

   Return 0 on success, -1 on _wstat() / stat() error, -2 if an exception was
   raised. */

int
_Py_stat(PyObject *path, struct stat *statbuf)
{
    int ret;
    PyObject *bytes;
    char *cpath;

    bytes = PyUnicode_EncodeFSDefault(path);
    if (bytes == NULL)
        return -2;

    /* check for embedded null bytes */
    if (PyBytes_AsStringAndSize(bytes, &cpath, NULL) == -1) {
        Py_DECREF(bytes);
        return -2;
    }

    ret = stat(cpath, statbuf);
    Py_DECREF(bytes);
    return ret;
}


/* This function MUST be kept async-signal-safe on POSIX when raise=0. */
static int
get_inheritable(int fd, int raise)
{
    int flags;

    flags = fcntl(fd, F_GETFD, 0);
    if (flags == -1) {
        if (raise)
            PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    return !(flags & FD_CLOEXEC);
}

/* Get the inheritable flag of the specified file descriptor.
   Return 1 if the file descriptor can be inherited, 0 if it cannot,
   raise an exception and return -1 on error. */
int
_Py_get_inheritable(int fd)
{
    return get_inheritable(fd, 1);
}


/* This function MUST be kept async-signal-safe on POSIX when raise=0. */
static int
set_inheritable(int fd, int inheritable, int raise, int *atomic_flag_works)
{
#if defined(HAVE_SYS_IOCTL_H) && defined(FIOCLEX) && defined(FIONCLEX)
    static int ioctl_works = -1;
    int request;
    int err;
#endif
    int flags, new_flags;
    int res;
    /* atomic_flag_works can only be used to make the file descriptor
       non-inheritable */
    assert(!(atomic_flag_works != NULL && inheritable));

    if (atomic_flag_works != NULL && !inheritable) {
        if (*atomic_flag_works == -1) {
            int isInheritable = get_inheritable(fd, raise);
            if (isInheritable == -1)
                return -1;
            *atomic_flag_works = !isInheritable;
        }

        if (*atomic_flag_works)
            return 0;
    }

#if defined(HAVE_SYS_IOCTL_H) && defined(FIOCLEX) && defined(FIONCLEX)
    if (ioctl_works != 0 && raise != 0) {
        /* fast-path: ioctl() only requires one syscall */
        /* caveat: raise=0 is an indicator that we must be async-signal-safe
         * thus avoid using ioctl() so we skip the fast-path. */
        if (inheritable)
            request = FIONCLEX;
        else
            request = FIOCLEX;
        err = ioctl(fd, request, NULL);
        if (!err) {
            ioctl_works = 1;
            return 0;
        }

        if (errno != ENOTTY && errno != EACCES) {
            if (raise)
                PyErr_SetFromErrno(PyExc_OSError);
            return -1;
        }
        else {
            /* Issue #22258: Here, ENOTTY means "Inappropriate ioctl for
               device". The ioctl is declared but not supported by the kernel.
               Remember that ioctl() doesn't work. It is the case on
               Illumos-based OS for example.

               Issue #27057: When SELinux policy disallows ioctl it will fail
               with EACCES. While FIOCLEX is safe operation it may be
               unavailable because ioctl was denied altogether.
               This can be the case on Android. */
            ioctl_works = 0;
        }
        /* fallback to fcntl() if ioctl() does not work */
    }
#endif

    /* slow-path: fcntl() requires two syscalls */
    flags = fcntl(fd, F_GETFD);
    if (flags < 0) {
        if (raise)
            PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    if (inheritable) {
        new_flags = flags & ~FD_CLOEXEC;
    }
    else {
        new_flags = flags | FD_CLOEXEC;
    }

    if (new_flags == flags) {
        /* FD_CLOEXEC flag already set/cleared: nothing to do */
        return 0;
    }

    res = fcntl(fd, F_SETFD, new_flags);
    if (res < 0) {
        if (raise)
            PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    return 0;
}

/* Make the file descriptor non-inheritable.
   Return 0 on success, set errno and return -1 on error. */
static int
make_non_inheritable(int fd)
{
    return set_inheritable(fd, 0, 0, NULL);
}

/* Set the inheritable flag of the specified file descriptor.
   On success: return 0, on error: raise an exception and return -1.

   If atomic_flag_works is not NULL:

    * if *atomic_flag_works==-1, check if the inheritable is set on the file
      descriptor: if yes, set *atomic_flag_works to 1, otherwise set to 0 and
      set the inheritable flag
    * if *atomic_flag_works==1: do nothing
    * if *atomic_flag_works==0: set inheritable flag to False

   Set atomic_flag_works to NULL if no atomic flag was used to create the
   file descriptor.

   atomic_flag_works can only be used to make a file descriptor
   non-inheritable: atomic_flag_works must be NULL if inheritable=1. */
int
_Py_set_inheritable(int fd, int inheritable, int *atomic_flag_works)
{
    return set_inheritable(fd, inheritable, 1, atomic_flag_works);
}

/* Same as _Py_set_inheritable() but on error, set errno and
   don't raise an exception.
   This function is async-signal-safe. */
int
_Py_set_inheritable_async_safe(int fd, int inheritable, int *atomic_flag_works)
{
    return set_inheritable(fd, inheritable, 0, atomic_flag_works);
}

static int
_Py_open_impl(const char *pathname, int flags, int gil_held)
{
    int fd;
    int async_err = 0;
    int *atomic_flag_works;

#if defined(O_CLOEXEC)
    atomic_flag_works = &_Py_open_cloexec_works;
    flags |= O_CLOEXEC;
#else
    atomic_flag_works = NULL;
#endif

    if (gil_held) {
        do {
            
            fd = open(pathname, flags);
            
        } while (fd < 0
                 && errno == EINTR); //  && !(async_err = PyErr_CheckSignals()));
        if (async_err)
            return -1;
        if (fd < 0) {
            PyErr_SetFromErrnoWithFilename(PyExc_OSError, pathname);
            return -1;
        }
    }
    else {
        fd = open(pathname, flags);
        if (fd < 0)
            return -1;
    }

    if (set_inheritable(fd, 0, gil_held, atomic_flag_works) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Open a file with the specified flags (wrapper to open() function).
   Return a file descriptor on success. Raise an exception and return -1 on
   error.

   The file descriptor is created non-inheritable.

   When interrupted by a signal (open() fails with EINTR), retry the syscall,
   except if the Python signal handler raises an exception.

   Release the GIL to call open(). The caller must hold the GIL. */
int
_Py_open(const char *pathname, int flags)
{
    /* _Py_open() must be called with the GIL held. */
    return _Py_open_impl(pathname, flags, 1);
}

/* Open a file with the specified flags (wrapper to open() function).
   Return a file descriptor on success. Set errno and return -1 on error.

   The file descriptor is created non-inheritable.

   If interrupted by a signal, fail with EINTR. */
int
_Py_open_noraise(const char *pathname, int flags)
{
    return _Py_open_impl(pathname, flags, 0);
}

/* Open a file. Use _wfopen() on Windows, encode the path to the locale
   encoding and use fopen() otherwise.

   The file descriptor is created non-inheritable.

   If interrupted by a signal, fail with EINTR. */
FILE *
_Py_wfopen(const wchar_t *path, const wchar_t *mode)
{
    FILE *f;
    char *cpath;
    char cmode[10];
    size_t r;
    r = wcstombs(cmode, mode, 10);
    if (r == (size_t)-1 || r >= 10) {
        errno = EINVAL;
        return NULL;
    }
    cpath = _Py_EncodeLocaleRaw(path, NULL);
    if (cpath == NULL) {
        return NULL;
    }
    f = fopen(cpath, cmode);
    PyMem_RawFree(cpath);
    if (f == NULL)
        return NULL;
    if (make_non_inheritable(fileno(f)) < 0) {
        fclose(f);
        return NULL;
    }
    return f;
}

/* Wrapper to fopen().

   The file descriptor is created non-inheritable.

   If interrupted by a signal, fail with EINTR. */
FILE*
_Py_fopen(const char *pathname, const char *mode)
{

    FILE *f = fopen(pathname, mode);
    if (f == NULL)
        return NULL;
    if (make_non_inheritable(fileno(f)) < 0) {
        fclose(f);
        return NULL;
    }
    return f;
}

/* Open a file. Call _wfopen() on Windows, or encode the path to the filesystem
   encoding and call fopen() otherwise.

   Return the new file object on success. Raise an exception and return NULL
   on error.

   The file descriptor is created non-inheritable.

   When interrupted by a signal (open() fails with EINTR), retry the syscall,
   except if the Python signal handler raises an exception.

   Release the GIL to call _wfopen() or fopen(). The caller must hold
   the GIL. */
FILE*
_Py_fopen_obj(PyObject *path, const char *mode)
{
    FILE *f;
    int async_err = 0;
    PyObject *bytes;
    const char *path_bytes;

    if (!PyUnicode_FSConverter(path, &bytes))
        return NULL;
    path_bytes = PyBytes_AS_STRING(bytes);
    do {
        
        f = fopen(path_bytes, mode);
        
    } while (f == NULL
             && errno == EINTR); //  && !(async_err = PyErr_CheckSignals()));

    Py_DECREF(bytes);
    if (async_err)
        return NULL;

    if (f == NULL) {
        PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path);
        return NULL;
    }

    if (set_inheritable(fileno(f), 0, 1, NULL) < 0) {
        fclose(f);
        return NULL;
    }
    return f;
}

/* Read count bytes from fd into buf.

   On success, return the number of read bytes, it can be lower than count.
   If the current file offset is at or past the end of file, no bytes are read,
   and read() returns zero.

   On error, raise an exception, set errno and return -1.

   When interrupted by a signal (read() fails with EINTR), retry the syscall.
   If the Python signal handler raises an exception, the function returns -1
   (the syscall is not retried).

   Release the GIL to call read(). The caller must hold the GIL. */
Py_ssize_t
_Py_read(int fd, void *buf, size_t count)
{
    Py_ssize_t n;
    int err;
    int async_err = 0;

    /* _Py_read() must not be called with an exception set, otherwise the
     * caller may think that read() was interrupted by a signal and the signal
     * handler raised an exception. */
    assert(!PyErr_Occurred());

    if (count > _PY_READ_MAX) {
        count = _PY_READ_MAX;
    }

    
    do {
        
        errno = 0;
        n = read(fd, buf, count);
        /* save/restore errno because PyErr_CheckSignals()
         * and PyErr_SetFromErrno() can modify it */
        err = errno;
        
	  } while (n < 0 && err == EINTR); // &&
    // !(async_err = PyErr_CheckSignals()));
    

    if (async_err) {
        /* read() was interrupted by a signal (failed with EINTR)
         * and the Python signal handler raised an exception */
        errno = err;
        assert(errno == EINTR && PyErr_Occurred());
        return -1;
    }
    if (n < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        errno = err;
        return -1;
    }

    return n;
}

static Py_ssize_t
_Py_write_impl(int fd, const void *buf, size_t count, int gil_held)
{
    Py_ssize_t n;
    int err;
    int async_err = 0;

    
    if (count > _PY_WRITE_MAX) {
        count = _PY_WRITE_MAX;
    }

    if (gil_held) {
        do {
            
            errno = 0;
            n = write(fd, buf, count);
            /* save/restore errno because PyErr_CheckSignals()
             * and PyErr_SetFromErrno() can modify it */
            err = errno;
            
	      } while (n < 0 && err == EINTR); // &&
	  //                !(async_err = PyErr_CheckSignals()));
    }
    else {
        do {
            errno = 0;
            n = write(fd, buf, count);
            err = errno;
        } while (n < 0 && err == EINTR);
    }
    

    if (async_err) {
        /* write() was interrupted by a signal (failed with EINTR)
           and the Python signal handler raised an exception (if gil_held is
           nonzero). */
        errno = err;
        assert(errno == EINTR && (!gil_held || PyErr_Occurred()));
        return -1;
    }
    if (n < 0) {
        if (gil_held)
            PyErr_SetFromErrno(PyExc_OSError);
        errno = err;
        return -1;
    }

    return n;
}

/* Write count bytes of buf into fd.

   On success, return the number of written bytes, it can be lower than count
   including 0. On error, raise an exception, set errno and return -1.

   When interrupted by a signal (write() fails with EINTR), retry the syscall.
   If the Python signal handler raises an exception, the function returns -1
   (the syscall is not retried). */

Py_ssize_t
_Py_write(int fd, const void *buf, size_t count)
{

    /* _Py_write() must not be called with an exception set, otherwise the
     * caller may think that write() was interrupted by a signal and the signal
     * handler raised an exception. */
    assert(!PyErr_Occurred());

    return _Py_write_impl(fd, buf, count, 1);
}

/* Write count bytes of buf into fd.
 *
 * On success, return the number of written bytes, it can be lower than count
 * including 0. On error, set errno and return -1.
 *
 * When interrupted by a signal (write() fails with EINTR), retry the syscall
 * without calling the Python signal handler. */
Py_ssize_t
_Py_write_noraise(int fd, const void *buf, size_t count)
{
    return _Py_write_impl(fd, buf, count, 0);
}

#ifdef HAVE_READLINK

/* Read value of symbolic link. Encode the path to the locale encoding, decode
   the result from the locale encoding.

   Return -1 on encoding error, on readlink() error, if the internal buffer is
   too short, on decoding error, or if 'buf' is too short. */
int
_Py_wreadlink(const wchar_t *path, wchar_t *buf, size_t buflen)
{
    char *cpath;
    char cbuf[MAXPATHLEN];
    size_t cbuf_len = Py_ARRAY_LENGTH(cbuf);
    wchar_t *wbuf;
    Py_ssize_t res;
    size_t r1;

    cpath = _Py_EncodeLocaleRaw(path, NULL);
    if (cpath == NULL) {
        errno = EINVAL;
        return -1;
    }
    res = readlink(cpath, cbuf, cbuf_len);
    PyMem_RawFree(cpath);
    if (res == -1) {
        return -1;
    }
    if ((size_t)res == cbuf_len) {
        errno = EINVAL;
        return -1;
    }
    cbuf[res] = '\0'; /* buf will be null terminated */
    wbuf = Py_DecodeLocale(cbuf, &r1);
    if (wbuf == NULL) {
        errno = EINVAL;
        return -1;
    }
    /* wbuf must have space to store the trailing NUL character */
    if (buflen <= r1) {
        PyMem_RawFree(wbuf);
        errno = EINVAL;
        return -1;
    }
    wcsncpy(buf, wbuf, buflen);
    PyMem_RawFree(wbuf);
    return (int)r1;
}
#endif

#ifdef HAVE_REALPATH

/* Return the canonicalized absolute pathname. Encode path to the locale
   encoding, decode the result from the locale encoding.

   Return NULL on encoding error, realpath() error, decoding error
   or if 'resolved_path' is too short. */
wchar_t*
_Py_wrealpath(const wchar_t *path,
              wchar_t *resolved_path, size_t resolved_path_len)
{
    char *cpath;
    char cresolved_path[MAXPATHLEN];
    wchar_t *wresolved_path;
    char *res;
    size_t r;
    cpath = _Py_EncodeLocaleRaw(path, NULL);
    if (cpath == NULL) {
        errno = EINVAL;
        return NULL;
    }
    res = realpath(cpath, cresolved_path);
    PyMem_RawFree(cpath);
    if (res == NULL)
        return NULL;

    wresolved_path = Py_DecodeLocale(cresolved_path, &r);
    if (wresolved_path == NULL) {
        errno = EINVAL;
        return NULL;
    }
    /* wresolved_path must have space to store the trailing NUL character */
    if (resolved_path_len <= r) {
        PyMem_RawFree(wresolved_path);
        errno = EINVAL;
        return NULL;
    }
    wcsncpy(resolved_path, wresolved_path, resolved_path_len);
    PyMem_RawFree(wresolved_path);
    return resolved_path;
}
#endif


int
_Py_isabs(const wchar_t *path)
{
    return (path[0] == SEP);
}

/* Get an absolute path.
   On error (ex: fail to get the current directory), return -1.
   On memory allocation failure, set *abspath_p to NULL and return 0.
   On success, return a newly allocated to *abspath_p to and return 0.
   The string must be freed by PyMem_RawFree(). */
int
_Py_abspath(const wchar_t *path, wchar_t **abspath_p)
{
    if (_Py_isabs(path)) {
        *abspath_p = _PyMem_RawWcsdup(path);
        return 0;
    }

    wchar_t cwd[MAXPATHLEN + 1];
    cwd[Py_ARRAY_LENGTH(cwd) - 1] = 0;
    if (!_Py_wgetcwd(cwd, Py_ARRAY_LENGTH(cwd) - 1)) {
        /* unable to get the current directory */
        return -1;
    }

    size_t cwd_len = wcslen(cwd);
    size_t path_len = wcslen(path);
    size_t len = cwd_len + 1 + path_len + 1;
    if (len <= (size_t)PY_SSIZE_T_MAX / sizeof(wchar_t)) {
        *abspath_p = PyMem_RawMalloc(len * sizeof(wchar_t));
    }
    else {
        *abspath_p = NULL;
    }
    if (*abspath_p == NULL) {
        return 0;
    }

    wchar_t *abspath = *abspath_p;
    memcpy(abspath, cwd, cwd_len * sizeof(wchar_t));
    abspath += cwd_len;

    *abspath = (wchar_t)SEP;
    abspath++;

    memcpy(abspath, path, path_len * sizeof(wchar_t));
    abspath += path_len;

    *abspath = 0;
    return 0;
}


/* Get the current directory. buflen is the buffer size in wide characters
   including the null character. Decode the path from the locale encoding.

   Return NULL on getcwd() error, on decoding error, or if 'buf' is
   too short. */
wchar_t*
_Py_wgetcwd(wchar_t *buf, size_t buflen)
{
    char fname[MAXPATHLEN];
    wchar_t *wname;
    size_t len;

    if (getcwd(fname, Py_ARRAY_LENGTH(fname)) == NULL)
        return NULL;
    wname = Py_DecodeLocale(fname, &len);
    if (wname == NULL)
        return NULL;
    /* wname must have space to store the trailing NUL character */
    if (buflen <= len) {
        PyMem_RawFree(wname);
        return NULL;
    }
    wcsncpy(buf, wname, buflen);
    PyMem_RawFree(wname);
    return buf;
}

/* Duplicate a file descriptor. The new file descriptor is created as
   non-inheritable. Return a new file descriptor on success, raise an OSError
   exception and return -1 on error.

   The GIL is released to call dup(). The caller must hold the GIL. */
int
_Py_dup(int fd)
{
#if defined(HAVE_FCNTL_H) && defined(F_DUPFD_CLOEXEC)
    
    
    fd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    
    
    if (fd < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

#else
    fd = dup(fd);
    
    
    if (fd < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    if (_Py_set_inheritable(fd, 0, NULL) < 0) {
        
        close(fd);
        
        return -1;
    }
#endif
    return fd;
}

/* Get the blocking mode of the file descriptor.
   Return 0 if the O_NONBLOCK flag is set, 1 if the flag is cleared,
   raise an exception and return -1 on error. */
int
_Py_get_blocking(int fd)
{
    int flags;
    
    flags = fcntl(fd, F_GETFL, 0);
    
    if (flags < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    return !(flags & O_NONBLOCK);
}

/* Set the blocking mode of the specified file descriptor.

   Set the O_NONBLOCK flag if blocking is False, clear the O_NONBLOCK flag
   otherwise.

   Return 0 on success, raise an exception and return -1 on error. */
int
_Py_set_blocking(int fd, int blocking)
{
#if defined(HAVE_SYS_IOCTL_H) && defined(FIONBIO)
    int arg = !blocking;
    if (ioctl(fd, FIONBIO, &arg) < 0)
        goto error;
#else
    int flags, res;

    
    flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        if (blocking)
            flags = flags & (~O_NONBLOCK);
        else
            flags = flags | O_NONBLOCK;

        res = fcntl(fd, F_SETFL, flags);
    } else {
        res = -1;
    }
    

    if (res < 0)
        goto error;
#endif
    return 0;

error:
    PyErr_SetFromErrno(PyExc_OSError);
    return -1;
}

int
_Py_GetLocaleconvNumeric(struct lconv *lc,
                         PyObject **decimal_point, PyObject **thousands_sep)
{
    assert(decimal_point != NULL);
    assert(thousands_sep != NULL);

    int change_locale = 0;
    if ((strlen(lc->decimal_point) > 1 || ((unsigned char)lc->decimal_point[0]) > 127)) {
        change_locale = 1;
    }
    if ((strlen(lc->thousands_sep) > 1 || ((unsigned char)lc->thousands_sep[0]) > 127)) {
        change_locale = 1;
    }

    /* Keep a copy of the LC_CTYPE locale */
    char *oldloc = NULL, *loc = NULL;
    if (change_locale) {
        oldloc = setlocale(LC_CTYPE, NULL);
        if (!oldloc) {
            PyErr_SetString(PyExc_RuntimeError,
                            "failed to get LC_CTYPE locale");
            return -1;
        }

        oldloc = _PyMem_Strdup(oldloc);
        if (!oldloc) {
            PyErr_NoMemory();
            return -1;
        }

        loc = setlocale(LC_NUMERIC, NULL);
        if (loc != NULL && strcmp(loc, oldloc) == 0) {
            loc = NULL;
        }

        if (loc != NULL) {
            /* Only set the locale temporarily the LC_CTYPE locale
               if LC_NUMERIC locale is different than LC_CTYPE locale and
               decimal_point and/or thousands_sep are non-ASCII or longer than
               1 byte */
            setlocale(LC_CTYPE, loc);
        }
    }

    int res = -1;

    *decimal_point = PyUnicode_DecodeLocale(lc->decimal_point, NULL);
    if (*decimal_point == NULL) {
        goto done;
    }

    *thousands_sep = PyUnicode_DecodeLocale(lc->thousands_sep, NULL);
    if (*thousands_sep == NULL) {
        goto done;
    }

    res = 0;

done:
    if (loc != NULL) {
        setlocale(LC_CTYPE, oldloc);
    }
    PyMem_Free(oldloc);
    return res;
}
