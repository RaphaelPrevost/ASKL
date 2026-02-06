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

/* OpenBSD vfscanf.c v. 1.19 */

/*-
 * Copyright (c) 1990, 1993
 * The Regents of the University of California.  All rights reserved.
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

#include "scanf.h"

/* 11-bit exponent (VAX G floating point) is 308 decimal digits */
#define MAXEXP      308
/* 128 bit fraction takes up 39 decimal digits; max reasonable precision */
#define MAXFRACT    39

#define BUF     513 /* Maximum length of numeric string. */

/*
 * Flags used during conversion.
 */
#define LONG        0x0001  /* l: long or double */
#define LONGDBL     0x0002  /* L: long double; unimplemented */
#define SHORT       0x0004  /* h: short */
#define QUAD        0x0008  /* q: quad */
#define SUPPRESS    0x0010  /* suppress assignment */
#define POINTER     0x0020  /* weird %p pointer (`fake hex') */
#define NOSKIP      0x0040  /* do not skip blanks */
#define SHORTSHORT  0x0080  /* hh: 8 bit integer */
#define SIZEINT     0x0100  /* z: (signed) size_t */

/* -- Concrete Server extensions -- */
#define TIMEINT     0x02000  /* t: time_t */
#define BINARY      0x04000  /* b: binary type, system endian */
#define BIG         0x08000  /* B: force big endian */
#define LITTLE      0x10000  /* bb: force little endian */
#define TRUNK       0x20000  /* t,T: truncate binary (get lower/higher bytes) */
#define HIBYTES     0x40000  /* tH: truncate and get higher bytes */
#define LOBYTES     0x80000  /* tL: truncate and get lower bytes */

/* -- End of Concrete Server extensions -- */

/*
 * The following are used in numeric conversions only:
 * SIGNOK, NDIGITS, DPTOK, and EXPOK are for floating point;
 * SIGNOK, NDIGITS, PFXOK, and NZDIGITS are for integral.
 */
#define SIGNOK      0x0200  /* +/- is (still) legal */
#define HAVESIGN    0x0400
#define NDIGITS     0x0800  /* no digits detected */

#define DPTOK       0x1000  /* (float) decimal point is still legal */
#define EXPOK       0x2000  /* (float) exponent (e+3, etc) still legal */

#define PFXOK       0x1000  /* 0x prefix is (still) legal */
#define NZDIGITS    0x2000  /* no zero digits detected */

/*
 * Conversion types.
 */
#define CT_CHAR     0   /* %c conversion */
#define CT_CCL      1   /* %[...] conversion */
#define CT_STRING   2   /* %s conversion */
#define CT_INT      3   /* integer, i.e., strtoq or strtouq */
#define CT_FLOAT    4   /* floating, i.e., strtod */

#define u_char unsigned char
#define u_long unsigned long
#define quad_t int64_t
#define u_quad_t uint64_t
#ifndef ssize_t
#define ssize_t long
#endif

/*--------------------------[ FUNCTIONS PROTOTYPES ]--------------------------*/

static u_char *__sccl(char *, u_char *);
static u_quad_t _fetch_int(const u_char *, size_t, int);
static double _fetch_float(const u_char *, size_t, int);

/* -------------------------------------------------------------------------- */

ASKL_API int m_vsnscanf(
    const char *buffer,
    size_t bufsize,
    const char *fmt0,
    va_list ap
)
{
    u_char *fmt = (u_char *)fmt0;
    int c;                  /* character from format, or conversion */
    size_t width;           /* field width, or 0 */
    char *p = NULL;         /* points into all kinds of strings */
    int n;                  /* handy integer */
    int flags;              /* flags as defined above */
    char *p0;               /* saves original value of p when necessary */
    int nread;              /* number of characters consumed from fp */
    int base;               /* base argument to strtoq/strtouq */
    char ccltab[256];       /* character class table for %[...] */
    char buf[BUF];          /* buffer for numeric conversions */
    /* conversion function (strtoq/strtouq) */
    u_quad_t (*ccfn)(const char *, char **, int);
    u_char *b = (u_char *) buffer;

    /* `basefix' is used to avoid `if' tests in the integer scanner */
    static short basefix[17] =
    { 10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

    nread = 0;
    base = 0;       /* XXX just to keep gcc happy */
    ccfn = NULL;    /* XXX just to keep gcc happy */
    for (;;) {
        c = *fmt++;
        if (c == 0)
            return nread;
        if (isspace(c)) {
            while ((bufsize > 0) &&
                isspace(*b))
                nread++, bufsize--, b++;
            continue;
        }
        if (c != '%')
            goto literal;
        width = 0;
        flags = 0;
        /*
         * switch on the format.  continue if done;
         * break once format type is derived.
         */
again:  c = *fmt++;
        switch (c) {
        case '%':
literal:
            if (bufsize <= 0)
                goto input_failure;
            if (*b != c)
                goto match_failure;
            bufsize--, b++;
            nread++;
            continue;

        case '*':
            flags |= SUPPRESS;
            goto again;
        case 'L':
            /* -- Concrete Server extensions -- */
            if (flags & TRUNK) {
                flags &= ~HIBYTES;
                flags |= LOBYTES;
            } else {
                flags |= LONGDBL;
            }
            goto again;
            /* -- End of Concrete Server extensions -- */
        case 'h':
            if (*fmt == 'h') {
                fmt++;
                flags |= SHORTSHORT;
            } else {
                flags |= SHORT;
            }
            goto again;
        case 'l':
            if (*fmt == 'l') {
                fmt++;
                flags |= QUAD;
            } else {
                flags |= LONG;
            }
            goto again;
        case 'q':
            flags |= QUAD;
            goto again;
        case 'z':
            flags |= SIZEINT;
            goto again;
        /* -- Concrete Server extensions -- */
        case 'b':
            flags |= (flags & BINARY) ? LITTLE : BINARY;
            goto again;
        case 'B':
            flags |= BIG;
            goto again;
        case 't':
            if (flags & BINARY) {
                flags |= TRUNK;
                flags |= LOBYTES;
                goto again;
            } else {
                /* time_t */
                flags |= TIMEINT;
                c = CT_INT;
                /* time_t may be 32 or 64 bits, so we
                    use u_quad_t to avoid partial reads */
                ccfn = (u_quad_t (*)(const char *, char **, int)) strtoll;
                base = 0;
                break;
            }
        case 'T':
            flags |= TRUNK;
            flags |= HIBYTES;
            goto again;
        case 'H':
            if (flags & TRUNK) {
                flags &= ~LOBYTES;
                flags |= HIBYTES;
            }
            goto again;
        case '$':
            width = va_arg(ap, size_t);
            goto again;
        /* -- End of Concrete Server extensions -- */

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            width = width * 10 + c - '0';
            goto again;

        /*
         * Conversions.
         * Those marked `compat' are for 4.[123]BSD compatibility.
         *
         * (According to ANSI, E and X formats are supposed
         * to be the same as e and x.  Sorry about that.)
         */
        case 'D': /* compat */
            flags |= LONG;
            /* FALLTHROUGH */
        case 'd':
            c = CT_INT;
            ccfn = (u_quad_t (*)(const char *, char **, int)) strtoll;
            base = 10;
            break;

        case 'i':
            c = CT_INT;
            ccfn = (u_quad_t (*)(const char *, char **, int)) strtoll;
            base = 0;
            break;

        case 'O': /* compat */
            flags |= LONG;
            /* FALLTHROUGH */
        case 'o':
            c = CT_INT;
            ccfn = (u_quad_t (*)(const char *, char **, int)) strtoull;
            base = 8;
            break;

        case 'u':
            c = CT_INT;
            ccfn = (u_quad_t (*)(const char *, char **, int)) strtoull;
            base = 10;
            break;

        case 'X':
        case 'x':
            flags |= PFXOK; /* enable 0x prefixing */
            c = CT_INT;
            ccfn = (u_quad_t (*)(const char *, char **, int)) strtoull;
            base = 16;
            break;

        case 'E':
        case 'G':
        case 'e':
        case 'f':
        case 'g':
            c = CT_FLOAT;
            break;

        case 's':
            c = CT_STRING;
            break;

        case '[':
            fmt = __sccl(ccltab, fmt);
            flags |= NOSKIP;
            c = CT_CCL;
            break;

        case 'c':
            flags |= NOSKIP;
            c = CT_CHAR;
            break;

        case 'p': /* pointer format is like hex */
            flags |= POINTER | PFXOK;
            c = CT_INT;
            ccfn = (u_quad_t (*)(const char *, char **, int)) strtoull;
            base = 16;
            break;
        /*
         * Disgusting backwards compatibility hacks.	XXX
         */
        case '\0': /* compat */
            return (EOF);

        default: /* compat */
            if (isupper(c))
                flags |= LONG;
            c = CT_INT;
            ccfn = (u_quad_t (*)(const char *, char **, int)) strtoll;
            base = 10;
            break;
        }

        /*
         * We have a conversion that requires input.
         */
        if (bufsize <= 0)
            goto input_failure;

        /*
         * Consume leading white space, except for formats
         * that suppress this.
         */
        if ((flags & NOSKIP) == 0) {
            while (isspace(*b)) {
                nread++;
                if (--bufsize > 0)
                    b++;
                else
                    goto input_failure;
            }
            /*
             * Note that there is at least one character in
             * the buffer, so conversions that do not set NOSKIP
             * ca no longer result in an input failure.
             */
        }

        /*
         * Do the conversion.
         */
        switch (c) {

        case CT_CHAR:
            /* scan arbitrary characters (sets NOSKIP) */
            if (width == 0)
                width = 1;
            if (flags & SUPPRESS) {
                size_t sum = 0;
                for (;;) {
                    if ((size_t) (n = bufsize) < width) {
                        sum += n;
                        width -= n;
                        b += n;
                        if (sum == 0)
                            goto input_failure;
                        break;
                    } else {
                        sum += width;
                        bufsize -= width;
                        b += width;
                        break;
                    }
                }
                nread += sum;
            } else {
                if (bufsize < width)
                    goto input_failure;

                memcpy((void *) va_arg(ap, char *), b, width);

                nread += width; b += width;
            }
            break;

        case CT_CCL:
            /* scan a (nonempty) character class (sets NOSKIP) */
            if (width == 0)
                width = (size_t)~0;	/* `infinity' */
            /* take only those things in the class */
            if (flags & SUPPRESS) {
                n = 0;
                while (ccltab[*b]) {
                    n++, bufsize--, b++;
                    if (--width == 0)
                        break;
                    if (bufsize <= 0) {
                        if (n == 0)
                            goto input_failure;
                        break;
                    }
                }
                if (n == 0)
                    goto match_failure;
            } else {
                p0 = p = va_arg(ap, char *);
                while (ccltab[*b]) {
                    bufsize--;
                    *p++ = *b++;
                    if (--width == 0)
                        break;
                    if (bufsize <= 0) {
                        if (p == p0)
                            goto input_failure;
                        break;
                    }
                }
                n = p - p0;
                if (n == 0)
                    goto match_failure;
                *p = '\0';
            }
            nread += n;
            break;

        case CT_STRING:
            /* like CCL, but zero-length string OK, & no NOSKIP */
            if (width == 0)
                width = (size_t)~0;
            if (flags & SUPPRESS) {
                n = 0;
                while (!isspace(*b)) {
                    n++, bufsize--, b++;
                    if (--width == 0)
                        break;
                    if (bufsize <= 0)
                        break;
                }
                nread += n;
            } else {
                p0 = p = va_arg(ap, char *);
                while (!isspace(*b)) {
                    bufsize--;
                    *p++ = *b++;
                    if (--width == 0)
                        break;
                    if (bufsize <= 0)
                        break;
                }
                *p = '\0';
                nread += p - p0;
            }
            continue;

        case CT_INT:
            if (flags & BINARY) {
                /* no scan, but check the buffer length */
                if (! width || width >= sizeof(long double)) {
                    if (flags & POINTER)
                        width = sizeof(int *);
                    else if (flags & SIZEINT)
                        width = sizeof(ssize_t);
                    else if (flags & TIMEINT)
                        width = sizeof(time_t);
                    else if (flags & QUAD)
                        width = sizeof(quad_t);
                    else if (flags & LONG)
                        width = sizeof(long);
                    else if (flags & SHORT)
                        width = sizeof(short);
                    else if (flags & SHORTSHORT)
                        width = sizeof(char);
                    else
                        width = sizeof(int);
                }

                if ((int) (bufsize - width) < 0) goto input_failure;
            } else {
                /* scan an integer as if by strtoq/strtouq */
#ifdef hardway
                if (width == 0 || width > sizeof(buf) - 1)
                    width = sizeof(buf) - 1;
#else
                /* size_t is unsigned, hence this optimisation */
                if (--width > sizeof(buf) - 2)
                    width = sizeof(buf) - 2;
                width++;
#endif
                flags |= SIGNOK | NDIGITS | NZDIGITS;
                for (p = buf; width; width--) {
                    c = *b;
                /*
                * Switch on the character; `goto ok'
                * if we accept it as a part of number.
                */
                switch (c) {

                   /*
                    * The digit 0 is always legal, but is
                    * special.  For %i conversions, if no
                    * digits (zero or nonzero) have been
                    * scanned (only signs), we will have
                    * base==0.  In that case, we should set
                    * it to 8 and enable 0x prefixing.
                    * Also, if we have not scanned zero digits
                    * before this, do not turn off prefixing
                    * (someone else will turn it off if we
                    * have scanned any nonzero digits).
                    */
                    case '0':
                        if (base == 0) {
                            base = 8;
                            flags |= PFXOK;
                        }
                        if (flags & NZDIGITS)
                            flags &= ~(SIGNOK|NZDIGITS|NDIGITS);
                        else
                            flags &= ~(SIGNOK|PFXOK|NDIGITS);
                        goto ok;

                    /* 1 through 7 always legal */
                    case '1': case '2': case '3':
                    case '4': case '5': case '6': case '7':
                        base = basefix[base];
                        flags &= ~(SIGNOK | PFXOK | NDIGITS);
                        goto ok;

                    /* digits 8 and 9 ok iff decimal or hex */
                    case '8': case '9':
                        base = basefix[base];
                        if (base <= 8)
                            break;	/* not legal here */
                        flags &= ~(SIGNOK | PFXOK | NDIGITS);
                        goto ok;

                    /* letters ok iff hex */
                    case 'A': case 'B': case 'C':
                    case 'D': case 'E': case 'F':
                    case 'a': case 'b': case 'c':
                    case 'd': case 'e': case 'f':
                        /* no need to fix base here */
                        if (base <= 10)
                            break;	/* not legal here */
                        flags &= ~(SIGNOK | PFXOK | NDIGITS);
                        goto ok;

                    /* sign ok only as first character */
                    case '+': case '-':
                        if (flags & SIGNOK) {
                            flags &= ~SIGNOK;
                            flags |= HAVESIGN;
                            goto ok;
                        }
                        break;

                    /* x ok iff flag still set & 2nd char
                     * (or 3rd char if we have a sign)
                     */
                    case 'x': case 'X':
                        if (flags & PFXOK && p == buf + 1 + !!(flags & HAVESIGN)) {
                            base = 16;	/* if %i */
                            flags &= ~PFXOK;
                            goto ok;
                        }
                        break;
                    }

                    /*
                    * If we got here, c is not a legal character
                    * for a number.  Stop accumulating digits.
                    */
                    break;
            ok:
                    /*
                    * c is legal: store it and look at the next.
                    */
                    *p++ = c;
                    if (--bufsize > 0)
                        b++;
                    else
                        break;		/* EOF */
                }
                /*
                * If we had only a sign, it is no good; push
                * back the sign.  If the number ends in `x',
                * it was [sign] '0' 'x', so push back the x
                * and treat it as [sign] '0'.
                */
                if (flags & NDIGITS) {
                    goto match_failure;
                }
                c = ((u_char *)p)[-1];
                if (c == 'x' || c == 'X') --p;
            }

            if ((flags & SUPPRESS) == 0) {
                u_quad_t res;

                if (flags & BINARY) {
                    res = _fetch_int(b, width, flags);
                    b += width; bufsize -= width;
                } else {
                    *p = '\0';
                    res = (*ccfn)(buf, (char **) NULL, base);
                    if (flags & TIMEINT) {
                        /* XXX check the size of the time_t type */
                        if ((quad_t) res != (time_t) res) {
                            debug("m_vsnscanf(): local time_t type "
                                  "is not large enough to read this"
                                  " timestamp.\n");
                            goto input_failure;
                        }
                    }
                }

                if (flags & POINTER)
                    *va_arg(ap, void **) =
                        (void *)(long)res;
                else if (flags & SIZEINT)
                    *va_arg(ap, ssize_t *) = (ssize_t) res;
                else if (flags & TIMEINT)
                    *va_arg(ap, time_t *) = (time_t) res;
                else if (flags & QUAD)
                    *va_arg(ap, quad_t *) = res;
                else if (flags & LONG)
                    *va_arg(ap, long *) = (long) res;
                else if (flags & SHORT)
                    *va_arg(ap, short *) = (short) res;
                else if (flags & SHORTSHORT)
                    *va_arg(ap, char *) = (char) res;
                else
                    *va_arg(ap, int *) = (int) res;
            }

            nread += (flags & BINARY) ? width : (size_t) (p - buf);
            break;

        case CT_FLOAT:
            if (flags & BINARY) {
                /* no scan, but check the buffer length */
                if (! width || width >= sizeof(long double)) {
                    if (flags & LONGDBL)
                        width = sizeof(long double);
                    else if (flags & LONG)
                        width = sizeof(double);
                    else
                        width = sizeof(float);
                }

                if ((int) (bufsize - width) < 0) goto input_failure;
            } else {
                /* scan a floating point number as if by strtod */
#ifdef hardway
                if (width == 0 || width > sizeof(buf) - 1)
                    width = sizeof(buf) - 1;
#else
                /* size_t is unsigned, hence this optimisation */
                if (--width > sizeof(buf) - 2)
                    width = sizeof(buf) - 2;
                width++;
#endif
                flags |= SIGNOK | NDIGITS | DPTOK | EXPOK;
                for (p = buf; width; width--) {
                    c = *b;
                    /*
                    * This code mimicks the integer conversion
                    * code, but is much simpler.
                    */
                    switch (c) {

                    case '0': case '1': case '2': case '3':
                    case '4': case '5': case '6': case '7':
                    case '8': case '9':
                        flags &= ~(SIGNOK | NDIGITS);
                        goto fok;

                    case '+': case '-':
                        if (flags & SIGNOK) {
                            flags &= ~SIGNOK;
                            goto fok;
                        }
                        break;
                    case '.':
                        if (flags & DPTOK) {
                            flags &= ~(SIGNOK | DPTOK);
                            goto fok;
                        }
                        break;
                    case 'e': case 'E':
                        /* no exponent without some digits */
                        if ((flags&(NDIGITS|EXPOK)) == EXPOK) {
                            flags =
                                (flags & ~(EXPOK|DPTOK)) |
                                SIGNOK | NDIGITS;
                            goto fok;
                        }
                        break;
                    }
                    break;
            fok:
                    *p++ = c;
                    if (--bufsize > 0)
                        b++;
                    else
                        break;	/* EOF */
                }
                /*
                * If no digits, might be missing exponent digits
                * (just give back the exponent) or might be missing
                * regular digits, but had sign and/or decimal point.
                */
                if (flags & NDIGITS) {
                    if (flags & EXPOK) {
                        /* no digits at all */
                        goto match_failure;
                    }
                    /* just a bad exponent (e and maybe sign) */
                    c = *(u_char *)--p;
                    if (c != 'e' && c != 'E') {
                        /* sign */
                        c = *(u_char *) --p;
                    }
                }
            }

            if ((flags & SUPPRESS) == 0) {
                double res;

                if (flags & BINARY) {
                    res = _fetch_float(b, width, flags);
                    b += width; bufsize -= width;
                } else {
                    *p = '\0';
                    res = strtod(buf, (char **) NULL);
                }

                if (flags & LONGDBL)
                    *va_arg(ap, long double *) = res;
                else if (flags & LONG)
                    *va_arg(ap, double *) = res;
                else
                    *va_arg(ap, float *) = res;
            }

            nread += (flags & BINARY) ? width : (size_t) (p - buf);
            break;
        }
    }
input_failure:
    errno = ENOMEM;
    return -1;
match_failure:
    errno = EAGAIN;
    return -1;
}

/* -------------------------------------------------------------------------- */

/*
 * Fill in the given table from the scanset at the given format
 * (just after `[').  Return a pointer to the character past the
 * closing `]'.  The table has a 1 wherever characters should be
 * considered part of the scanset.
 */
static u_char *__sccl(char *tab, u_char *fmt)
{
    int c, n, v;

    /* first `clear' the whole table */
    c = *fmt++;             /* first char hat => negated scanset */
    if (c == '^') {
        v = 1;              /* default => accept */
        c = *fmt++;         /* get new first char */
    } else
        v = 0;              /* default => reject */
    /* should probably use memset here */
    for (n = 0; n < 256; n++)
        tab[n] = v;
    if (c == 0)
        return (fmt - 1);   /* format ended before closing ] */

    /*
     * Now set the entries corresponding to the actual scanset
     * to the opposite of the above.
     *
     * The first character may be ']' (or '-') without being special;
     * the last character may be '-'.
     */
    v = 1 - v;
    for (;;) {
        tab[c] = v;     /* take character c */
doswitch:
        n = *fmt++;     /* and examine the next */
        switch (n) {

        case 0:         /* format ended too soon */
            return (fmt - 1);

        case '-':
            /*
             * A scanset of the form
             * [01+-]
             * is defined as `the digit 0, the digit 1,
             * the character +, the character -', but
             * the effect of a scanset such as
             * [a-zA-Z0-9]
             * is implementation defined.  The V7 Unix
             * scanf treats `a-z' as `the letters a through
             * z', but treats `a-a' as `the letter a, the
             * character -, and the letter a'.
             *
             * For compatibility, the `-' is not considerd
             * to define a range if the character following
             * it is either a close bracket (required by ANSI)
             * or is not numerically greater than the character
             * we just stored in the table (c).
             */
            n = *fmt;
            if (n == ']' || n < c) {
                c = '-';
                break;  /* resume the for(;;) */
            }
            fmt++;
            do {        /* fill in the range */
                tab[++c] = v;
            } while (c < n);
#if 1  /* XXX another disgusting compatibility hack */
            /*
             * Alas, the V7 Unix scanf also treats formats
             * such as [a-c-e] as `the letters a through e'.
             * This too is permitted by the standard....
             */
            goto doswitch;
#else
            c = *fmt++;
            if (c == 0)
                return (fmt - 1);
            if (c == ']')
                return (fmt);
#endif
            break;

        case ']':       /* end of scanset */
            return (fmt);

        default:        /* just another character */
            c = n;
            break;
        }
    }
    /* NOTREACHED */
}

/* -------------------------------------------------------------------------- */

static u_quad_t _fetch_int(const u_char *b, size_t w, int flags)
{
    u_quad_t ret = 1;
    int sys_endian = 0; /* 0 == big endian, little endian otherwise */

    if (*(char *) & ret == 1) sys_endian = 1;

    if (flags & BIG) sys_endian = 0; else if (flags & LITTLE) sys_endian = 1;

    if (flags & TRUNK) {
        if (
            (sys_endian == 1 && (flags & HIBYTES)) ||
            (sys_endian == 0 && (flags & LOBYTES))
           ) {
            /* should begin from the MSB */
            if (flags & POINTER) b += sizeof(int *);
            else if (flags & SIZEINT) b += sizeof(ssize_t);
            else if (flags & QUAD) b += sizeof(quad_t);
            else if (flags & LONG) b += sizeof(long);
            else if (flags & SHORT) b += sizeof(short);
            else if (flags & SHORTSHORT) b += sizeof(char);
            else b += sizeof(int);
            b -= w;
        }
    }

    ret = 0;

    /* fetch bytes, storing them using the given endianness */
    if (sys_endian) {
        while (w --) ret += ((u_quad_t) b[w]) << (w * CHAR_BIT);
    } else {
        size_t i = 0;
        while (w --) ret += ((u_quad_t) b[w]) << (i ++ * CHAR_BIT);
    }

    return ret;
}

/* -------------------------------------------------------------------------- */

static double _fetch_float(const u_char *b, UNUSED size_t w, int flags)
{
    double ret = 0;
    int float_flags = 0;

    if (flags & BIG)
        float_flags = FLOAT_BIG;
    else if (flags & LITTLE)
        float_flags = FLOAT_LITTLE;

    if (flags & LONGDBL)
        /* not supported */;
    else if (flags & LONG)
        ret = float_read_double((const char *) b, float_flags);
    else
        ret = float_read_single((const char *) b, float_flags);

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API int m_snscanf(const char *buffer, size_t size, const char *fmt, ...)
{
    int ret = 0;
    va_list ap;

    va_start(ap, fmt);
    ret = m_vsnscanf(buffer, size, fmt, ap);
    va_end(ap);

    return ret;
}

/* -------------------------------------------------------------------------- */
