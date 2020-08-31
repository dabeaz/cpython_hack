/*---------------------------------------------------------------------------*
 * <RCS keywords>
 *
 * C++ Library
 *
 * Copyright 1992-1994, David Gottner
 *
 *                    All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice, this permission notice and
 * the following disclaimer notice appear unmodified in all copies.
 *
 * I DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL I
 * BE LIABLE FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA, OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *---------------------------------------------------------------------------*/

/* Modified to support --help and --version, as well as /? on Windows
 * by Georg Brandl. */

#include <Python.h>
#include <stdio.h>
#include <string.h>
#include "pycore_getopt.h"

#ifdef __cplusplus
extern "C" {
#endif

int _PyOS_opterr = 1;                 /* generate error messages */
Py_ssize_t _PyOS_optind = 1;          /* index into argv array   */
const char *_PyOS_optarg = NULL;   /* optional argument       */

static const char *opt_ptr = "";

/* Python command line short and long options */

#define SHORT_OPTS "bBc:dEhiIJm:OqRsStuvVW:xX:?"

static const _PyOS_LongOption longopts[] = {
    {"check-hash-based-pycs", 1, 0},
    {NULL, 0, 0},
};


void _PyOS_ResetGetOpt(void)
{
    _PyOS_opterr = 1;
    _PyOS_optind = 1;
    _PyOS_optarg = NULL;
    opt_ptr = "";
}

int _PyOS_GetOpt(Py_ssize_t argc, char * const *argv, int *longindex)
{
    char *ptr;
    char option;

    if (*opt_ptr == '\0') {

        if (_PyOS_optind >= argc)
            return -1;

        else if (argv[_PyOS_optind][0] != '-' ||
                 argv[_PyOS_optind][1] == '\0' /* lone dash */ )
            return -1;

        else if (strcmp(argv[_PyOS_optind], "--") == 0) {
            ++_PyOS_optind;
            return -1;
        }

        else if (strcmp(argv[_PyOS_optind], "--help") == 0) {
            ++_PyOS_optind;
            return 'h';
        }

        else if (strcmp(argv[_PyOS_optind], "--version") == 0) {
            ++_PyOS_optind;
            return 'V';
        }

        opt_ptr = &argv[_PyOS_optind++][1];
    }

    if ((option = *opt_ptr++) == '\0')
        return -1;

    if (option == '-') {
        // Parse long option.
        if (*opt_ptr == '\0') {
            if (_PyOS_opterr) {
                fprintf(stderr, "expected long option\n");
            }
            return -1;
        }
        *longindex = 0;
        const _PyOS_LongOption *opt;
        for (opt = &longopts[*longindex]; opt->name; opt = &longopts[++(*longindex)]) {
            if (!strcmp(opt->name, opt_ptr))
                break;
        }
        if (!opt->name) {
            if (_PyOS_opterr) {
                fprintf(stderr, "unknown option %s\n", argv[_PyOS_optind - 1]);
            }
            return '_';
        }
        opt_ptr = "";
        if (!opt->has_arg) {
            return opt->val;
        }
        if (_PyOS_optind >= argc) {
            if (_PyOS_opterr) {
                fprintf(stderr, "Argument expected for the %s options\n",
                        argv[_PyOS_optind - 1]);
            }
            return '_';
        }
        _PyOS_optarg = argv[_PyOS_optind++];
        return opt->val;
    }

    if (option == 'J') {
        if (_PyOS_opterr) {
            fprintf(stderr, "-J is reserved for Jython\n");
        }
        return '_';
    }

    if ((ptr = strchr(SHORT_OPTS, option)) == NULL) {
        if (_PyOS_opterr) {
            fprintf(stderr, "Unknown option: -%c\n", (char)option);
        }
        return '_';
    }

    if (*(ptr + 1) == ':') {
        if (*opt_ptr != '\0') {
            _PyOS_optarg  = opt_ptr;
            opt_ptr = "";
        }

        else {
            if (_PyOS_optind >= argc) {
                if (_PyOS_opterr) {
                    fprintf(stderr,
                        "Argument expected for the -%c option\n", (char)option);
                }
                return '_';
            }

            _PyOS_optarg = argv[_PyOS_optind++];
        }
    }

    return option;
}

#ifdef __cplusplus
}
#endif

