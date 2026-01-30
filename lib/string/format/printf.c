/*******************************************************************************
 *  ASKL.                                                                      *
 *  Copyright (c) 2026 Raphael Prevost <raph@el.bzh>                           *
 *                                                                             *
 *  This software is a computer program whose purpose is to provide a          *
 *  framework for developing and prototyping network services.                 *
 *                                                                             *
 *  This software is governed by the CeCILL  license under French law and      *
 *  abiding by the rules of distribution of free software.  You can  use,      *
 *  modify and/ or redistribute the software under the terms of the CeCILL     *
 *  license as circulated by CEA, CNRS and INRIA at the following URL          *
 *  "http://www.cecill.info".                                                  *
 *                                                                             *
 *  As a counterpart to the access to the source code and  rights to copy,     *
 *  modify and redistribute granted by the license, users are provided only    *
 *  with a limited warranty  and the software's author,  the holder of the     *
 *  economic rights,  and the successive licensors  have only  limited         *
 *  liability.                                                                 *
 *                                                                             *
 *  In this respect, the user's attention is drawn to the risks associated     *
 *  with loading,  using,  modifying and/or developing or reproducing the      *
 *  software by the user in light of its specific status of free software,     *
 *  that may mean  that it is complicated to manipulate,  and  that  also      *
 *  therefore means  that it is reserved for developers  and  experienced      *
 *  professionals having in-depth computer knowledge. Users are therefore      *
 *  encouraged to load and test the software's suitability as regards their    *
 *  requirements in conditions enabling the security of their systems and/or   *
 *  data to be ensured and,  more generally, to use and operate it in the      *
 *  same conditions as regards security.                                       *
 *                                                                             *
 *  The fact that you are presently reading this means that you have had       *
 *  knowledge of the CeCILL license and that you accept its terms.             *
 *                                                                             *
 ******************************************************************************/

/* OpenBSD vfprintf.c v. 1.48 */
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Actual printf innards.
 *
 * This code is large and complicated...
 */

#include "printf.h"

#define u_char unsigned char
#define u_long unsigned long
#define quad_t int64_t
#define u_quad_t uint64_t
#ifndef ssize_t
#define ssize_t long
#endif

union arg {
    int                intarg;
    unsigned int       uintarg;
    long               longarg;
    unsigned long      ulongarg;
    quad_t             longlongarg;
    u_quad_t           ulonglongarg;
    ptrdiff_t          ptrdiffarg;
    size_t             sizearg;
    size_t             ssizearg;
    intmax_t           intmaxarg;
    uintmax_t          uintmaxarg;
    void              *pvoidarg;
    char              *pchararg;
    signed char       *pschararg;
    short             *pshortarg;
    int               *pintarg;
    long              *plongarg;
    quad_t            *plonglongarg;
    ptrdiff_t         *pptrdiffarg;
    ssize_t           *pssizearg;
    intmax_t          *pintmaxarg;
    double             doublearg;
    long double        longdoublearg;
};

static int __find_arguments(const char *fmt0, va_list ap, union arg **argtable,
                            size_t *argtablesiz);
static int __grow_type_table(unsigned char **typetable, int *tablesize);

#ifdef FLOATING_POINT
#include <locale.h>
#include <math.h>
/* 11-bit exponent (VAX G floating point) is 308 decimal digits */
#define MAXEXP      308
/* 128 bit fraction takes up 39 decimal digits; max reasonable precision */
#define MAXFRACT    39


static int _write_int(char *out, size_t len, const u_char *b, size_t w, int flags);
static int _write_float(char *out, size_t len, const u_char *b, int flags);

#define    BUF        (MAXEXP+MAXFRACT+1)    /* + decimal point */
#define    DEFPREC        6

extern char *_m_dtoa(double, int, int, int *, int *, char **);
extern void  _m_freedtoa(char *);
static char *cvt(double, int, int, int *, int *, int, int *);
static int exponent(char *, int, int);

#else /* no FLOATING_POINT */
#define    BUF        40
#endif /* FLOATING_POINT */

#define STATIC_ARG_TBL_SIZE 8    /* Size of static argument table. */


/*
 * Macros for converting digits to letters and vice versa
 */
#define    to_digit(c)    ((c) - '0')
#define is_digit(c)    ((unsigned)to_digit(c) <= 9)
#define    to_char(n)    ((n) + '0')

/*
 * Flags used during conversion.
 */
#define ALT         0x0001        /* alternate form */
#define HEXPREFIX   0x0002        /* add 0x or 0X prefix */
#define LADJUST     0x0004        /* left adjustment */
#define LONGDBL     0x0008        /* long double; unimplemented */
#define LONGINT     0x0010        /* long integer */
#define LLONGINT    0x0020        /* long long integer */
#define SHORTINT    0x0040        /* short integer */
#define ZEROPAD     0x0080        /* zero (as opposed to blank) pad */
#define FPT         0x0100        /* Floating point number */
#define PTRINT      0x0200        /* (unsigned) ptrdiff_t */
#define SIZEINT     0x0400        /* (signed) size_t */
#define CHARINT     0x0800        /* 8 bit integer */
#define MAXINT      0x1000        /* largest integer size (intmax_t) */

/* -- Concrete Server extensions -- */

#define BINARY      0x002000      /* b: binary type, system endian */
#define BIG         0x004000      /* B: force big endian */
#define LITTLE      0x008000      /* bb: force little endian */
#define TRUNCAT     0x010000      /* t,T: truncate */
#define HIBYTES     0x020000      /* tH: truncate and get higher bytes */
#define LOBYTES     0x040000      /* tL: truncate and get lower bytes */
#define ALIGNED     0x080000      /* ta: align to the integer boundaries */
#ifdef _ENABLE_HTTP
#define URL         0x100000      /* url: urlencode the given string */
#endif

/* -- End of Concrete Server extensions -- */

/* -------------------------------------------------------------------------- */

#ifdef _ENABLE_DB
ASKL_API int m_vsnprintf_db(void *con, char *buffer, size_t bufsize,
                          const char *fmt0, va_list ap)
#else
ASKL_API int m_vsnprintf(
    char *buffer,
    size_t bufsize,
    const char *fmt0,
    va_list ap
)
#endif
{
    char *fmt;        /* format string */
    int ch;	        /* character from fmt */
    int n, n2;        /* handy integers (short term usage) */
    char *cp;        /* handy char pointer (short term usage) */
    int flags;        /* flags as above */
    int ret;        /* return value accumulator */
    int width;        /* width from format (%8d), or 0 */
    int prec;        /* precision from format (%.3d), or -1 */
    char sign;        /* sign prefix (' ', '+', '-', or \0) */
    wchar_t wc;
    mbstate_t ps;
#ifdef FLOATING_POINT
    char *decimal_point = localeconv()->decimal_point;
    int softsign;        /* temporary negative sign for floats */
    double _double = 0.0;        /* double precision arguments %[eEfgG] */
    int expt;        /* integer value of exponent */
    int expsize = MAXEXP;        /* character count for expstr */
    int ndig=0;        /* actual number of digits returned by cvt */
    char expstr[7];        /* buffer for exponent string */
    char *dtoaresult = NULL;
#endif

    uintmax_t _umax;    /* integer arguments %[diouxX] */
    enum { OCT, DEC, HEX } base;/* base for [diouxX] conversion */
    int dprec;        /* a copy of prec if [diouxX], 0 otherwise */
    int realsz;        /* field size expanded by dprec */
    int size;        /* size of converted field or string */
    char *xdigs = NULL;        /* digits for [xX] conversion */
    char buf[BUF];        /* space for %c, %[diouxX], %[eEfgG] */
    char ox[2];        /* space for 0x hex-prefix */
    union arg *argtable;    /* args, built due to positional arg */
    union arg statargtable[STATIC_ARG_TBL_SIZE];
    size_t argtablesiz;
    int nextarg;        /* 1-based argument index */
    va_list orgap;        /* original argument pointer */

    /*
     * Choose PADSIZE to trade efficiency vs. size.  If larger printf
     * fields occur frequently, increase PADSIZE and make the initialisers
     * below longer.
     */
#define    PADSIZE    16        /* pad chunk size */
    static char blanks[PADSIZE] =
     {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
    static char zeroes[PADSIZE] =
     {'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'};

    /*
     * BEWARE, these `goto error' on error, and PAD uses `n'.
         * XXX avoid an integer overflow on bufsize
     */
#define    PRINT(ptr, len) do { \
     if ((int) bufsize > 0) { \
         if (buffer) {\
             memcpy(buffer, ptr, ((size_t) len < bufsize) ? (size_t) len : bufsize); \
             buffer += len; \
         } \
         bufsize -= len; \
     } \
} while (0)

#define    PAD(howmany, with) do { \
    if ((n = (howmany)) > 0) { \
        while (n > PADSIZE) { \
            PRINT(with, PADSIZE); \
            n -= PADSIZE; \
        } \
        PRINT(with, n); \
    } \
} while (0)
#define    FLUSH()

    /*
     * To extend shorts properly, we need both signed and unsigned
     * argument extraction methods.
     */
#define    SARG() \
    ((intmax_t)(flags&MAXINT ? GETARG(intmax_t) : \
        flags&LLONGINT ? GETARG(quad_t) : \
        flags&LONGINT ? GETARG(long) : \
        flags&PTRINT ? GETARG(ptrdiff_t) : \
        flags&SIZEINT ? GETARG(ssize_t) : \
        flags&SHORTINT ? (short)GETARG(int) : \
        flags&CHARINT ? (char)GETARG(int) : \
        GETARG(int)))
#define    UARG() \
    ((uintmax_t)(flags&MAXINT ? GETARG(uintmax_t) : \
        flags&LLONGINT ? GETARG(u_quad_t) : \
        flags&LONGINT ? GETARG(unsigned long) : \
        flags&PTRINT ? (uintptr_t)GETARG(ptrdiff_t) : /* XXX */ \
        flags&SIZEINT ? GETARG(size_t) : \
        flags&SHORTINT ? (unsigned short)GETARG(int) : \
        flags&CHARINT ? (unsigned char)GETARG(int) : \
        GETARG(unsigned int)))

   /*
    * Append a digit to a value and check for overflow.
    */
#define APPEND_DIGIT(val, dig) do { \
   if ((val) > INT_MAX / 10) \
       goto overflow; \
   (val) *= 10; \
   if ((val) > INT_MAX - to_digit((dig))) \
       goto overflow; \
   (val) += to_digit((dig)); \
} while (0)


     /*
      * Get * arguments, including the form *nn$.  Preserve the nextarg
      * that the argument can be gotten once the type is determined.
      */
#define GETASTER(val) \
    n2 = 0; \
    cp = fmt; \
    while (is_digit(*cp)) { \
        APPEND_DIGIT(n2, *cp); \
        cp++; \
    } \
    if (*cp == '$') { \
        int hold = nextarg; \
        if (argtable == NULL) { \
            argtable = statargtable; \
            __find_arguments(fmt0, orgap, &argtable, &argtablesiz); \
        } \
        nextarg = n2; \
        val = GETARG(int); \
        nextarg = hold; \
        fmt = ++cp; \
    } else { \
        val = GETARG(int); \
    }

/*
* Get the argument indexed by nextarg.   If the argument table is
* built, use it to get the argument.  If its not, get the next
* argument (and arguments must be gotten sequentially).
*/
#define GETARG(type) \
    ((argtable != NULL) ? *((type *)(& argtable[nextarg ++])) : \
     (nextarg++, va_arg(ap, type)))

    fmt = (char *)fmt0;
    argtable = NULL;
    nextarg = 1;
    va_copy(orgap, ap);
    ret = 0;

    memset(&ps, 0, sizeof(ps));
    /*
     * Scan the format for conversions (`%' character).
     */
    for (;;) {
        cp = fmt;
        while ((n = mbrtowc(&wc, fmt, MB_CUR_MAX, &ps)) > 0) {
            fmt += n;
            if (wc == '%') {
                fmt--;
                break;
            }
        }
        if (fmt != cp) {
            ptrdiff_t m = fmt - cp;
            if (m < 0 || m > INT_MAX - ret)
                goto overflow;
            PRINT(cp, m);
            ret += m;
        }
        if (n <= 0)
            goto done;
        fmt++;        /* skip over '%' */

        flags = 0;
        dprec = 0;
        width = 0;
        prec = -1;
        sign = '\0';

rflag:        ch = *fmt++;
reswitch:    switch (ch) {
        case ' ':
            /*
             * ``If the space and + flags both appear, the space
             * flag will be ignored.''
             *    -- ANSI X3J11
             */
            if (!sign)
                sign = ' ';
            goto rflag;
        case '#':
            flags |= ALT;
            goto rflag;
        case '*':
            /*
             * ``A negative field width argument is taken as a
             * - flag followed by a positive field width.''
             *    -- ANSI X3J11
             * They don't exclude field widths read from args.
             */
            GETASTER(width);
            if (width >= 0)
                goto rflag;
            if (width == INT_MIN)
                goto overflow;
            width = -width;
            /* FALLTHROUGH */
        case '-':
            flags |= LADJUST;
            goto rflag;
        case '+':
            sign = '+';
            goto rflag;
        case '.':
            if ((ch = *fmt++) == '*') {
                GETASTER(n);
                prec = n < 0 ? -1 : n;
                goto rflag;
            }
            n = 0;
            while (is_digit(ch)) {
                APPEND_DIGIT(n, ch);
                ch = *fmt++;
            }
            if (ch == '$') {
                nextarg = n;
                if (argtable == NULL) {
                    argtable = statargtable;
                    __find_arguments(fmt0, orgap,
                        &argtable, &argtablesiz);
                }
                goto rflag;
            }
            prec = n;
            goto reswitch;
        case '0':
            /*
             * ``Note that 0 is taken as a flag, not as the
             * beginning of a field width.''
             *    -- ANSI X3J11
             */
            flags |= ZEROPAD;
            goto rflag;
        case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            n = 0;
            do {
                APPEND_DIGIT(n, ch);
                ch = *fmt++;
            } while (is_digit(ch));
            if (ch == '$') {
                nextarg = n;
                if (argtable == NULL) {
                    argtable = statargtable;
                    __find_arguments(fmt0, orgap,
                        &argtable, &argtablesiz);
                }
                goto rflag;
            }
            width = n;
            goto reswitch;

        case 'L':
            /* -- Concrete Server extensions -- */
            if (flags & TRUNCAT) {
                flags &= ~HIBYTES;
                flags |= LOBYTES;
            }
#ifdef FLOATING_POINT
            else {
                flags |= LONGDBL;
            }
#endif
            /* -- End of Concrete Server extensions -- */
            goto rflag;

        case 'h':
            flags |= SHORTINT;
            if (*fmt == 'h') {
                fmt++;
                flags |= CHARINT;
            } else {
                flags |= SHORTINT;
            }
            goto rflag;
        case 'j':
            flags |= MAXINT;
            goto rflag;
        case 'l':
            if (*fmt == 'l') {
                fmt++;
                flags |= LLONGINT;
            } else {
                flags |= LONGINT;
            }
            goto rflag;
        case 'q':
            flags |= LLONGINT;
            goto rflag;
        /*case 't':
            flags |= PTRINT;
            goto rflag;*/
        case 'z':
            flags |= SIZEINT;
            goto rflag;
        case 'c':
            *(cp = buf) = GETARG(int);
            size = 1;
            sign = '\0';
            break;


        /* -- Concrete Server extensions -- */
        case 'b':
            flags |= (flags & BINARY) ? LITTLE : BINARY;
            goto rflag;
        case 'B':
            flags |= BIG;
            goto rflag;
        case 't':
            if (flags & BINARY && (~flags & TRUNCAT) ) {
                flags |= TRUNCAT;
                flags |= LOBYTES;
            } else {
                flags |= PTRINT;
            }
            goto rflag;
        case 'T':
            flags |= TRUNCAT;
            flags |= HIBYTES;
            goto rflag;
        case 'H':
            if (flags & TRUNCAT) {
                flags &= ~LOBYTES;
                flags |= HIBYTES;
            }
            goto rflag;
        case 'a':
            if ( (flags & BINARY) && (flags & TRUNCAT) ) {
                /* align the truncated value on the integer boundaries */
                flags |= ALIGNED;
            }
            goto rflag;
        /* -- End of Concrete Server extensions -- */

        case 'D':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'd':
        case 'i':
            _umax = SARG();
            if ((intmax_t)_umax < 0) {
                _umax = -_umax;
                sign = '-';
            }
            base = DEC;
            goto number;
#ifdef FLOATING_POINT
        case 'e':
        case 'E':
        case 'f':
        case 'g':
        case 'G':
            if (prec == -1) {
                prec = DEFPREC;
            } else if ((ch == 'g' || ch == 'G') && prec == 0) {
                prec = 1;
            }

            if (flags & LONGDBL) {
                _double = (double) GETARG(long double);
            } else {
                _double = GETARG(double);
            }

            if (flags & BINARY) {
                int w = 0;
                float tmp = 0;
                /* from the standard, e,E,f,g,G conversions use double;
                   however, we need to be able to output floats, too.
                   you should use f for floats and the others for doubles.
                */
                if (ch == 'f') {
                    tmp = (float) _double;
                    w = _write_float(buffer, bufsize, (u_char *) & tmp, flags);
                    if (w != -1) {
                        if (buffer) buffer += w;
                        bufsize -= w; ret += w;
                        flags = 0;
                        dprec = 0;
                        width = 0;
                        prec = -1;
                        size = 0;
                        sign = '\0';
                        break;
                    } else {
                        ret = -1; errno = ENOMEM;
                        goto error;
                    }
                } else {
                    flags |= LONGINT;
                    w = _write_float(buffer, bufsize, (u_char *) & _double, flags);
                    if (w != -1) {
                        buffer += w; bufsize -= w; ret += w;
                        flags = 0;
                        dprec = 0;
                        width = 0;
                        prec = -1;
                        size = 0;
                        sign = '\0';
                        break;
                    } else {
                        ret = -1; errno = ENOMEM;
                        goto error;
                    }
                }
            }

            /* do this before tricky precision changes */
            if (isinf(_double)) {
                if (_double < 0)
                    sign = '-';
                cp = (char *) "Inf";
                size = 3;
                break;
            }
            if (isnan(_double)) {
                cp = (char *) "NaN";
                size = 3;
                break;
            }

            flags |= FPT;
            if (dtoaresult)
                _m_freedtoa(dtoaresult);
            dtoaresult = cp = cvt(_double, prec, flags, &softsign,
                &expt, ch, &ndig);
            if (ch == 'g' || ch == 'G') {
                if (expt <= -4 || expt > prec)
                    ch = (ch == 'g') ? 'e' : 'E';
                else
                    ch = 'g';
            }
            if (ch <= 'e') {    /* 'e' or 'E' fmt */
                --expt;
                expsize = exponent(expstr, expt, ch);
                size = expsize + ndig;
                if (ndig > 1 || flags & ALT)
                    ++size;
            } else if (ch == 'f') {        /* f fmt */
                if (expt > 0) {
                    size = expt;
                    if (prec || flags & ALT)
                        size += prec + 1;
                } else { /* "0.X" */
                    size = prec + 2;
                }
            } else if (expt >= ndig) {    /* fixed g fmt */
                size = expt;
                if (flags & ALT)
                    ++size;
            } else {
                size = ndig + (expt > 0 ?  1 : 2 - expt);
            }

            if (softsign)
                sign = '-';
            break;
#endif /* FLOATING_POINT */
        case 'O':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'o':
            _umax = UARG();
            base = OCT;
            goto nosign;
        case 'p':
            /*
             * ``The argument shall be a pointer to void.  The
             * value of the pointer is converted to a sequence
             * of printable characters, in an implementation-
             * defined manner.''
             *    -- ANSI X3J11
             */
            /* NOSTRICT */
            _umax = (u_long)GETARG(void *);
            base = HEX;
            xdigs = (char *) "0123456789abcdef";
            flags |= HEXPREFIX;
            ch = 'x';
            goto nosign;
        case 's':
#ifdef _ENABLE_HTTP
_str:
#endif
            if ((cp = GETARG(char *)) == NULL)
                cp = (char *) "(null)";
            if (prec >= 0) {
                if (flags & BINARY) {
                    size = prec;
                } else {
                    /*
                     * can't use strlen; can only look for the
                     * NUL in the first `prec' characters, and
                     * strlen() would go further.
                     */
                    char *p = memchr(cp, 0, prec);

                    size = p ? (p - cp) : prec;
                }
            } else {
                size_t len;

                if ((len = strlen(cp)) > INT_MAX)
                    goto overflow;
                size = (int)len;
            }

            #ifdef _ENABLE_HTTP
            if (flags & URL) {
                size_t _tmplen = 0;
                char *_tmpbuf = string_rawurlencode(cp, size, RFC1738_ESCAPE_RESERVED);
                if (_tmpbuf) {
                    _tmplen = strlen(_tmpbuf);
                    PRINT(_tmpbuf, _tmplen);
                    free(_tmpbuf);
                    ret += _tmplen;
                    continue;
                } else {
                    ret = -1; errno = EINVAL;
                    goto error;
                }
            }
            #endif

            #ifdef _ENABLE_DB
            if (con && size) {
            #ifdef _ENABLE_MYSQL
                /* allocate a temporary buffer of 2*size + 2 chars ('') */
                unsigned long escapelen = 0;
                char *_tmpbuf = malloc(((2 * size) + 2) * sizeof(*_tmpbuf));
                if (! _tmpbuf) {
                    ret = -1; errno = ENOMEM;
                    goto error;
                }
                /* quote and escape the string */
                _tmpbuf[0] = '\'';
                escapelen = mysql_real_escape_string(con, & _tmpbuf[1], cp, size);
                _tmpbuf[escapelen + 1] = '\'';
                /* write the escaped string */
                PRINT(_tmpbuf, escapelen + 2);
                free(_tmpbuf);
                ret += escapelen + 2;
                continue;
            #else
            #error "Please select a supported database engine."
            #endif
            }
            #endif
            sign = '\0';
            break;
        case 'U':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'u':

            #ifdef _ENABLE_HTTP
            /* -- Concrete Server extensions -- */
            if ( (ch = *fmt ++) == 'r') {
                if ( (ch = *fmt ++) == 'l') {
                    flags |= URL;
                    goto _str;
                } else fmt -= 2;
             } else fmt --;
             /* -- End of Concrete Server extensions -- */
             #endif

            _umax = UARG();
            base = DEC;
            goto nosign;
        case 'X':
            xdigs = (char *) "0123456789ABCDEF";
            goto hex;
        case 'x':
            xdigs = (char *) "0123456789abcdef";
hex:        _umax = UARG();
            base = HEX;
            /* leading 0x/X only if non-zero */
            if (flags & ALT && _umax != 0)
                flags |= HEXPREFIX;

            /* unsigned conversions */
nosign:     sign = '\0';
            /*
             * ``... diouXx conversions ... if a precision is
             * specified, the 0 flag will be ignored.''
             *    -- ANSI X3J11
             */
number:     if ((dprec = prec) >= 0)
                flags &= ~ZEROPAD;

            if (flags & BINARY) {
                int w = _write_int(buffer, bufsize, (u_char *) & _umax, width, flags);
                if (w != -1) {
                    if (buffer) buffer += w;
                    bufsize -= w; ret += w;
                    flags = 0;
                    dprec = 0;
                    width = 0;
                    prec = -1;
                    size = 0;
                    sign = '\0';
                    break;
                } else {
                    ret = -1; errno = ENOMEM;
                    goto error;
                }
            }


            /*
             * ``The result of converting a zero value with an
             * explicit precision of zero is no characters.''
             *    -- ANSI X3J11
             */
            cp = buf + BUF;
            if (_umax != 0 || prec != 0) {
                /*
                 * Unsigned mod is hard, and unsigned mod
                 * by a constant is easier than that by
                 * a variable; hence this switch.
                 */
                switch (base) {
                case OCT:
                    do {
                        *--cp = to_char(_umax & 7);
                        _umax >>= 3;
                    } while (_umax);
                    /* handle octal leading 0 */
                    if (flags & ALT && *cp != '0')
                        *--cp = '0';
                    break;

                case DEC:
                    /* many numbers are 1 digit */
                    while (_umax >= 10) {
                        *--cp = to_char(_umax % 10);
                        _umax /= 10;
                    }
                    *--cp = to_char(_umax);
                    break;

                case HEX:
                    do {
                        *--cp = xdigs[_umax & 15];
                        _umax >>= 4;
                    } while (_umax);
                    break;

                default:
                    cp = (char *) "bug in vfprintf: bad base";
                    size = strlen(cp);
                    goto skipsize;
                }
            }
            size = buf + BUF - cp;
        skipsize:
            break;
        default:    /* "%?" prints ?, unless ? is NUL */
            if (ch == '\0')
                goto done;
            /* pretend it was %c with argument ch */
            cp = buf;
            *cp = ch;
            size = 1;
            sign = '\0';
            break;
        }

        /*
         * All reasonable formats wind up here.  At this point, `cp'
         * points to a string which (if not flags&LADJUST) should be
         * padded out to `width' places.  If flags&ZEROPAD, it should
         * first be prefixed by any sign or other prefix; otherwise,
         * it should be blank padded before the prefix is emitted.
         * After any left-hand padding and prefixing, emit zeroes
         * required by a decimal [diouxX] precision, then print the
         * string proper, then emit zeroes required by any leftover
         * floating precision; finally, if LADJUST, pad with blanks.
         *
         * Compute actual size, so we know how much to pad.
         * size excludes decimal prec; realsz includes it.
         */
        realsz = dprec > size ? dprec : size;
        if (sign)
            realsz++;
        else if (flags & HEXPREFIX)
            realsz+= 2;

        /* right-adjusting blank padding */
        if ((flags & (LADJUST|ZEROPAD)) == 0)
            PAD(width - realsz, blanks);

        /* prefix */
        if (sign) {
            PRINT(&sign, 1);
        } else if (flags & HEXPREFIX) {
            ox[0] = '0';
            ox[1] = ch;
            PRINT(ox, 2);
        }

        /* right-adjusting zero padding */
        if ((flags & (LADJUST|ZEROPAD)) == ZEROPAD)
            PAD(width - realsz, zeroes);

        /* leading zeroes from decimal precision */
        PAD(dprec - size, zeroes);

        /* the string or number proper */
#ifdef FLOATING_POINT
        if ((flags & FPT) == 0) {
            PRINT(cp, size);
        } else {    /* glue together f_p fragments */
            if (ch >= 'f') {    /* 'f' or 'g' */
                if (_double == 0) {
                    /* kludge for __dtoa irregularity */
                    PRINT("0", 1);
                    if (expt < ndig || (flags & ALT) != 0) {
                        PRINT(decimal_point, 1);
                        PAD(ndig - 1, zeroes);
                    }
                } else if (expt <= 0) {
                    PRINT("0", 1);
                    PRINT(decimal_point, 1);
                    PAD(-expt, zeroes);
                    PRINT(cp, ndig);
                } else if (expt >= ndig) {
                    PRINT(cp, ndig);
                    PAD(expt - ndig, zeroes);
                    if (flags & ALT)
                        PRINT(".", 1);
                } else {
                    PRINT(cp, expt);
                    cp += expt;
                    PRINT(".", 1);
                    PRINT(cp, ndig-expt);
                }
            } else {    /* 'e' or 'E' */
                if (ndig > 1 || flags & ALT) {
                    ox[0] = *cp++;
                    ox[1] = '.';
                    PRINT(ox, 2);
                    if (_double) {
                        PRINT(cp, ndig-1);
                    } else {/* 0.[0..] */
                        /* __dtoa irregularity */
                        PAD(ndig - 1, zeroes);
                    }
                } else { /* XeYYY */
                    PRINT(cp, 1);
                }
                PRINT(expstr, expsize);
            }
        }
#else
        PRINT(cp, size);
#endif
        /* left-adjusting padding (always blank) */
        if (flags & LADJUST)
            PAD(width - realsz, blanks);

        /* finally, adjust ret */
        if (width < realsz)
            width = realsz;
        if (width > INT_MAX - ret)
            goto overflow;
        ret += width;

        FLUSH();    /* copy out the I/O vectors */
    }
done:
    FLUSH();

finish:
#ifdef FLOATING_POINT
    if (dtoaresult)
        _m_freedtoa(dtoaresult);
#endif
    if (argtable != NULL && argtable != statargtable) {
        munmap(argtable, argtablesiz);
        argtable = NULL;
    }

    /* terminal NUL */
    if (buffer && ((int) bufsize > 0)) *buffer = '\0';

    return ret;

overflow:
    errno = ENOMEM;
error:
    va_end(orgap);
    ret = -1;
    goto finish;
}

/* -------------------------------------------------------------------------- */

/*
 * Type ids for argument type table.
 */
#define T_UNUSED    0
#define T_SHORT        1
#define T_U_SHORT    2
#define TP_SHORT    3
#define T_INT        4
#define T_U_INT        5
#define TP_INT        6
#define T_LONG        7
#define T_U_LONG    8
#define TP_LONG        9
#define T_LLONG        10
#define T_U_LLONG    11
#define TP_LLONG    12
#define T_DOUBLE    13
#define T_LONG_DOUBLE    14
#define TP_CHAR        15
#define TP_VOID        16
#define T_PTRINT    17
#define TP_PTRINT    18
#define T_SIZEINT    19
#define T_SSIZEINT    20
#define TP_SSIZEINT    21
#define T_MAXINT    22
#define T_MAXUINT    23
#define TP_MAXINT    24
#define T_CHAR     25
#define T_U_CHAR   26

/*
 * Find all arguments when a positional parameter is encountered.  Returns a
 * table, indexed by argument number, of pointers to each arguments.  The
 * initial argument table should be an array of STATIC_ARG_TBL_SIZE entries.
 * It will be replaced with a mmap-ed one if it overflows (malloc cannot be
 * used since we are attempting to make snprintf thread safe, and alloca is
 * problematic since we have nested functions..)
 */
static int __find_arguments(const char *fmt0, va_list ap,
                            union arg **argtable, size_t *argtablesiz)
{
    char *fmt;        /* format string */
    int ch;           /* character from fmt */
    int n, n2;        /* handy integer (short term usage) */
    char *cp;        /* handy char pointer (short term usage) */
    int flags;        /* flags as above */
    unsigned char *typetable; /* table of types */
    unsigned char stattypetable[STATIC_ARG_TBL_SIZE];
    int tablesize;        /* current size of type table */
    int tablemax;        /* largest used index in table */
    int nextarg;        /* 1-based argument index */
    wchar_t wc;
    mbstate_t ps;

    /*
     * Add an argument type to the table, expanding if necessary.
     */
#define ADDTYPE(type) \
    ((nextarg >= tablesize) ? \
        __grow_type_table(&typetable, &tablesize) : 0, \
    (nextarg > tablemax) ? tablemax = nextarg : 0, \
    typetable[nextarg++] = type)

#define    ADDSARG() \
        ((flags&MAXINT) ? ADDTYPE(T_MAXINT) : \
        ((flags&PTRINT) ? ADDTYPE(T_PTRINT) : \
        ((flags&SIZEINT) ? ADDTYPE(T_SSIZEINT) : \
        ((flags&LLONGINT) ? ADDTYPE(T_LLONG) : \
        ((flags&LONGINT) ? ADDTYPE(T_LONG) : \
        ((flags&SHORTINT) ? ADDTYPE(T_SHORT) : \
        ((flags&CHARINT) ? ADDTYPE(T_CHAR) : ADDTYPE(T_INT))))))))

#define    ADDUARG() \
        ((flags&MAXINT) ? ADDTYPE(T_MAXUINT) : \
        ((flags&PTRINT) ? ADDTYPE(T_PTRINT) : \
        ((flags&SIZEINT) ? ADDTYPE(T_SIZEINT) : \
        ((flags&LLONGINT) ? ADDTYPE(T_U_LLONG) : \
        ((flags&LONGINT) ? ADDTYPE(T_U_LONG) : \
        ((flags&SHORTINT) ? ADDTYPE(T_U_SHORT) : \
        ((flags&CHARINT) ? ADDTYPE(T_U_CHAR) : ADDTYPE(T_U_INT))))))))

    /*
     * Add * arguments to the type array.
     */
#define ADDASTER() \
    n2 = 0; \
    cp = fmt; \
    while (is_digit(*cp)) { \
        APPEND_DIGIT(n2, *cp);\
        cp++; \
    } \
    if (*cp == '$') { \
        int hold = nextarg; \
        nextarg = n2; \
        ADDTYPE(T_INT); \
        nextarg = hold; \
        fmt = ++cp; \
    } else { \
        ADDTYPE(T_INT); \
    }
    fmt = (char *)fmt0;
    typetable = stattypetable;
    tablesize = STATIC_ARG_TBL_SIZE;
    tablemax = 0;
    nextarg = 1;
    memset(typetable, T_UNUSED, STATIC_ARG_TBL_SIZE);
    memset(&ps, 0, sizeof(ps));

    /*
     * Scan the format for conversions (`%' character).
     */
    for (;;) {
        cp = fmt;
        while ((n = mbrtowc(&wc, fmt, MB_CUR_MAX, &ps)) > 0) {
            fmt += n;
            if (wc == '%') {
                fmt--;
                break;
            }
        }
        if (n <= 0)
            goto done;
        fmt++;        /* skip over '%' */

        flags = 0;

rflag:        ch = *fmt++;
reswitch:    switch (ch) {
        case ' ':
        case '#':
            goto rflag;
        case '*':
            ADDASTER();
            goto rflag;
        case '-':
        case '+':
            goto rflag;
        case '.':
            if ((ch = *fmt++) == '*') {
                ADDASTER();
                goto rflag;
            }
            while (is_digit(ch)) {
                ch = *fmt++;
            }
            goto reswitch;
        case '0':
            goto rflag;
        case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            n = 0;
            do {
                APPEND_DIGIT(n ,ch);
                ch = *fmt++;
            } while (is_digit(ch));
            if (ch == '$') {
                nextarg = n;
                goto rflag;
            }
            goto reswitch;
#ifdef FLOATING_POINT
        case 'L':
            flags |= LONGDBL;
            goto rflag;
#endif
        case 'h':
            if (*fmt == 'h') {
                fmt++;
                flags |= CHARINT;
            } else {
                flags |= SHORTINT;
            }
            goto rflag;
        case 'l':
            if (*fmt == 'l') {
                fmt++;
                flags |= LLONGINT;
            } else {
                flags |= LONGINT;
            }
            goto rflag;
        case 'q':
            flags |= LLONGINT;
            goto rflag;
        case 't':
            flags |= PTRINT;
            goto rflag;
        case 'z':
            flags |= SIZEINT;
            goto rflag;
        case 'c':
            ADDTYPE(T_INT);
            break;
        case 'D':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'd':
        case 'i':
            ADDSARG();
            break;
#ifdef FLOATING_POINT
        case 'e':
        case 'E':
        case 'f':
        case 'g':
        case 'G':
            if (flags & LONGDBL)
                ADDTYPE(T_LONG_DOUBLE);
            else
                ADDTYPE(T_DOUBLE);
            break;
#endif /* FLOATING_POINT */
        case 'O':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'o':
            ADDUARG();
            break;
        case 'p':
            ADDTYPE(TP_VOID);
            break;
        case 's':
            ADDTYPE(TP_CHAR);
            break;
        case 'U':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'u':
        case 'X':
        case 'x':
            ADDUARG();
            break;
        default:    /* "%?" prints ?, unless ? is NUL */
            if (ch == '\0')
                goto done;
            break;
        }
    }
done:
    /*
     * Build the argument table.
     */
    if (tablemax >= STATIC_ARG_TBL_SIZE) {
        *argtablesiz = sizeof(union arg) * (tablemax + 1);
        *argtable = mmap(NULL, *argtablesiz,
            PROT_WRITE|PROT_READ, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (*argtable == MAP_FAILED)
            return (-1);
    }

#if 0
    /* XXX is this required? */
    (*argtable)[0].intarg = 0;
#endif
    for (n = 1; n <= tablemax; n++) {
        switch (typetable[n]) {
        case T_UNUSED:
        case T_CHAR:
        case T_U_CHAR:
        case T_SHORT:
        case T_U_SHORT:
        case T_INT:
            (*argtable)[n].intarg = va_arg(ap, int);
            break;
        case TP_SHORT:
            (*argtable)[n].pshortarg = va_arg(ap, short *);
            break;
        case T_U_INT:
            (*argtable)[n].uintarg = va_arg(ap, unsigned int);
            break;
        case TP_INT:
            (*argtable)[n].pintarg = va_arg(ap, int *);
            break;
        case T_LONG:
            (*argtable)[n].longarg = va_arg(ap, long);
            break;
        case T_U_LONG:
            (*argtable)[n].ulongarg = va_arg(ap, unsigned long);
            break;
        case TP_LONG:
            (*argtable)[n].plongarg = va_arg(ap, long *);
            break;
        case T_LLONG:
            (*argtable)[n].longlongarg = va_arg(ap, quad_t);
            break;
        case T_U_LLONG:
            (*argtable)[n].ulonglongarg = va_arg(ap, u_quad_t);
            break;
        case TP_LLONG:
            (*argtable)[n].plonglongarg = va_arg(ap, quad_t *);
            break;
        case T_DOUBLE:
            (*argtable)[n].doublearg = va_arg(ap, double);
            break;
        case T_LONG_DOUBLE:
            (*argtable)[n].longdoublearg = va_arg(ap, long double);
            break;
        case TP_CHAR:
            (*argtable)[n].pchararg = va_arg(ap, char *);
            break;
        case TP_VOID:
            (*argtable)[n].pvoidarg = va_arg(ap, void *);
            break;
        case T_PTRINT:
            (*argtable)[n].ptrdiffarg = va_arg(ap, ptrdiff_t);
            break;
        case TP_PTRINT:
            (*argtable)[n].pptrdiffarg = va_arg(ap, ptrdiff_t *);
            break;
        case T_SIZEINT:
            (*argtable)[n].sizearg = va_arg(ap, size_t);
            break;
        case T_SSIZEINT:
            (*argtable)[n].ssizearg = va_arg(ap, ssize_t);
            break;
        case TP_SSIZEINT:
            (*argtable)[n].pssizearg = va_arg(ap, ssize_t *);
            break;
        case TP_MAXINT:
            (*argtable)[n].pintmaxarg = va_arg(ap, intmax_t *);
            break;
        }
    }

    if (typetable != NULL && typetable != stattypetable) {
        munmap(typetable, *argtablesiz);
        typetable = NULL;
    }

    return 0;

overflow:
    errno = ENOMEM;
    return -1;
}

/* -------------------------------------------------------------------------- */

/*
 * Increase the size of the type table.
 */
static int __grow_type_table(unsigned char **typetable, int *tablesize)
{
    unsigned char *oldtable = *typetable;
    int newsize = *tablesize * 2;

    if (newsize < get_page_size())
        newsize = get_page_size();

    if (*tablesize == STATIC_ARG_TBL_SIZE) {
        *typetable = mmap(NULL, newsize, PROT_WRITE|PROT_READ,
            MAP_ANON|MAP_PRIVATE, -1, 0);
        if (*typetable == MAP_FAILED)
            return (-1);
        memcpy(*typetable, oldtable, *tablesize);
    } else {
        unsigned char *new = mmap(NULL, newsize, PROT_WRITE|PROT_READ,
            MAP_ANON|MAP_PRIVATE, -1, 0);
        if (new == MAP_FAILED)
            return (-1);
        memmove(new, *typetable, *tablesize);
        munmap(*typetable, *tablesize);
        *typetable = new;
    }
    memset(*typetable + *tablesize, T_UNUSED, (newsize - *tablesize));

    *tablesize = newsize;
    return (0);
}

/* -------------------------------------------------------------------------- */

#ifdef FLOATING_POINT

static char *cvt(double value, int ndigits, int flags, int *sign,
                 int *decpt, int ch, int *length)
{
    int mode;
    char *digits, *bp, *rve;

    if (ch == 'f') {
        mode = 3;        /* ndigits after the decimal point */
    } else {
        /* To obtain ndigits after the decimal point for the 'e'
         * and 'E' formats, round to ndigits + 1 significant
         * figures.
         */
        if (ch == 'e' || ch == 'E') {
            ndigits++;
        }
        mode = 2;        /* ndigits significant digits */
    }

    digits = _m_dtoa(value, mode, ndigits, decpt, sign, &rve);
    if ((ch != 'g' && ch != 'G') || flags & ALT) {/* Print trailing zeros */
        bp = digits + ndigits;
        if (ch == 'f') {
            if (*digits == '0' && value)
                *decpt = -ndigits + 1;
            bp += *decpt;
        }
        if (value == 0)    /* kludge for __dtoa irregularity */
            rve = bp;
        while (rve < bp)
            *rve++ = '0';
    }
    *length = rve - digits;
    return (digits);
}

/* -------------------------------------------------------------------------- */

static int exponent(char *p0, int exp, int fmtch)
{
    char *p, *t;
    char expbuf[MAXEXP];

    p = p0;
    *p++ = fmtch;
    if (exp < 0) {
        exp = -exp;
        *p++ = '-';
    } else
        *p++ = '+';
    t = expbuf + MAXEXP;
    if (exp > 9) {
        do {
            *--t = to_char(exp % 10);
        } while ((exp /= 10) > 9);
        *--t = to_char(exp);
        for (; t < expbuf + MAXEXP; *p++ = *t++)
            /* nothing */;
    } else {
        *p++ = '0';
        *p++ = to_char(exp);
    }
    return (p - p0);
}
#endif /* FLOATING_POINT */

/* -------------------------------------------------------------------------- */

static size_t _getsize(int flags)
{
    if (flags & SIZEINT) return sizeof(ssize_t);
    else if (flags & LLONGINT) return sizeof(quad_t);
    else if (flags & LONGINT) return sizeof(long);
    else if (flags & SHORTINT) return sizeof(short);
    else if (flags & CHARINT) return sizeof(char);
    else return sizeof(int);

    /* NOTREACHED */
}

/* -------------------------------------------------------------------------- */

static int _write_int(char *out, size_t len, const u_char *b,
                      size_t w, int flags)
{
    size_t intsize = 1;
    int sys_endian = 0; /* 0 == big endian, little endian otherwise */

    if (*(char *) & intsize == 1) sys_endian = 1;

    intsize = _getsize(flags);

    /* if we don't have a buffer (dry run) simply return the size */
    if (! out) return intsize;

    if (w && (flags & TRUNCAT)) {
        if (
            (sys_endian == 1 && (flags & HIBYTES)) ||
            (sys_endian == 0 && (flags & LOBYTES))
           ) {
            /* should begin from the MSB */
            b += intsize - w;
        }

        /* alignment was requested */
        if (
            (flags & ALIGNED) &&
            (
             ( (flags & LITTLE) && (flags & HIBYTES) ) ||
             ( (flags & BIG) && (flags & LOBYTES) )
            )
           ) {
            /* should write from the MSB */
            if (len < intsize) return -1;
            out += intsize - w;
        }
    }

    if (! w) w = intsize; else intsize = w;

    if (w > len) return -1;

    /* fetch bytes, storing them using the given endianness */
    if ( (sys_endian && (flags & BIG)) || (! sys_endian && (flags & LITTLE)) ) {
        while (w --) *out ++ = b[w];
    } else {
        memcpy(out, b, w);
    }

    return intsize;
}

/* -------------------------------------------------------------------------- */

static int _write_float(char *out, size_t len, const u_char *b, int flags)
{
    int fflags = 0;
    size_t fsize = 0;

    if (flags & BIG)
        fflags = FLOAT_BIG;
    else if (flags & LITTLE)
        fflags = FLOAT_LITTLE;

    if (flags & LONGDBL) {
        /* not implemented */
    } else if (flags & LONGINT) {
        if (! out) return sizeof(double);
        if (len < sizeof(double)) return -1;
        fsize = sizeof(double);
        float_write_double(out, b, fflags);
    } else {
        if (! out) return sizeof(float);
        if (len < sizeof(float)) return -1;
        fsize = sizeof(float);
        float_write_single(out, b, fflags);
    }

    return fsize;
}

/* -------------------------------------------------------------------------- */

#ifdef _ENABLE_DB
ASKL_API int m_vsnprintf(char *buffer, size_t bufsize, const char *fmt0,
                       va_list ap)
{
    return m_vsnprintf_db(NULL, buffer, bufsize, fmt0, ap);
}
#endif

/* -------------------------------------------------------------------------- */

ASKL_API int m_snprintf(char *buffer, size_t size, const char *fmt, ...)
{
    int ret = 0;
    va_list ap;

    va_start(ap, fmt);
    ret = m_vsnprintf(buffer, size, fmt, ap);
    va_end(ap);

    return ret;
}

/* -------------------------------------------------------------------------- */

#ifdef _ENABLE_DB
ASKL_API int m_snprintf_db(void *con, char *buffer, size_t size,
                         const char *fmt, ...)
{
    int ret = 0;
    va_list ap;

    va_start(ap, fmt);
    ret = m_vsnprintf_db(con, buffer, size, fmt, ap);
    va_end(ap);

    return ret;
}
#endif

/* -------------------------------------------------------------------------- */
