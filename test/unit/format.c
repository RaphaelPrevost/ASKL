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

#include "../askl_test.h"
#include "../../lib/string/format/format.h"
#include <float.h>
#include <math.h>

/* format with the library and with the C library, compare both outputs */
#define CHECK_FMT(fmt, ...) do { \
    char _got[512], _want[512]; \
    int _g = m_snprintf(_got, sizeof(_got), fmt, __VA_ARGS__); \
    int _w = snprintf(_want, sizeof(_want), fmt, __VA_ARGS__); \
    if (_g != _w || strcmp(_got, _want)) { \
        test_fail( \
            __FILE__, __LINE__, "\"%s\": got \"%s\" (%d), libc \"%s\" (%d)", \
            fmt, _got, _g, _want, _w \
        ); \
    } \
} while (0)

/* -------------------------------------------------------------------------- */

static int _integers(void)
{
    CHECK_FMT("%d", 0);
    CHECK_FMT("%d", -1);
    CHECK_FMT("%d", INT_MIN);
    CHECK_FMT("%d", INT_MAX);
    CHECK_FMT("%i", 42);
    CHECK_FMT("%u", 0U);
    CHECK_FMT("%u", UINT_MAX);
    CHECK_FMT("%x", 0xdeadbeefU);
    CHECK_FMT("%X", 0xdeadbeefU);
    CHECK_FMT("%o", 0755U);
    CHECK_FMT("%#x", 255U);
    CHECK_FMT("%#o", 8U);
    CHECK_FMT("%hd", (short) -12345);
    CHECK_FMT("%ld", LONG_MIN);
    CHECK_FMT("%lu", ULONG_MAX);
    CHECK_FMT("%lld", LLONG_MAX);
    CHECK_FMT("%lld", LLONG_MIN);
    CHECK_FMT("%llu", ULLONG_MAX);
    CHECK_FMT("%llx", 0x0123456789abcdefULL);
    CHECK_FMT("%zu", (size_t) 123456789);
    CHECK_FMT("%c", 'x');
    CHECK_FMT("%%|%d", 1);

    /* flags, width, precision */
    CHECK_FMT("%5d|", 42);
    CHECK_FMT("%-5d|", 42);
    CHECK_FMT("%05d|", 42);
    CHECK_FMT("%05d|", -42);
    CHECK_FMT("%+d|", 42);
    CHECK_FMT("% d|", 42);
    CHECK_FMT("%.5d|", 42);
    CHECK_FMT("%8.5d|", -42);
    CHECK_FMT("%*d|", 6, 42);
    CHECK_FMT("%-*d|", 6, 42);
    CHECK_FMT("%.*d|", 4, 42);

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _strings(void)
{
    CHECK_FMT("%s", "");
    CHECK_FMT("%s", "hello");
    CHECK_FMT("%10s|", "hello");
    CHECK_FMT("%-10s|", "hello");
    CHECK_FMT("%.3s|", "hello");
    CHECK_FMT("%.0s|", "hello");
    CHECK_FMT("%.*s|", 2, "hello");
    CHECK_FMT("%s=%d;%s=%x", "a", 1, "b", 2);
    CHECK_FMT("%c%c%c", 'a', 'b', 'c');

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _floats(void)
{
    static const double values[] = {
        0.0, 1.0, -1.0, 0.5, 0.1, 0.2, 0.3, 2.5, 1.0 / 3.0, 123456789.0,
        1e15, 1e16, 1e21, 1e22, 1e100, 1e300, 1e-5, 1e-100, 1e-300,
        DBL_MAX, DBL_MIN, DBL_EPSILON, 5e-324, 4.9406564584124654e-324,
        1.7976931348623157e308, 2.2250738585072014e-308, 9007199254740993.0
    };
    unsigned int i = 0;

    for (i = 0; i < sizeof(values) / sizeof(*values); i ++) {
        CHECK_FMT("%f", values[i]);
        CHECK_FMT("%e", values[i]);
        CHECK_FMT("%g", values[i]);
        CHECK_FMT("%E", values[i]);
        CHECK_FMT("%G", values[i]);
        CHECK_FMT("%.0f", values[i]);
        CHECK_FMT("%.10f", values[i]);
        CHECK_FMT("%.17g", values[i]);
        CHECK_FMT("%.1e", values[i]);
        CHECK_FMT("%12.3f|", values[i]);
        CHECK_FMT("%-12.3f|", values[i]);
        CHECK_FMT("%+.2e|", -values[i]);
    }

    CHECK_FMT("%g", -0.0);
    CHECK_FMT("%f", -0.0);
    CHECK_FMT("%#.0f", 1.0);
    CHECK_FMT("%#g", 1.0);
    CHECK_FMT("%010.3f|", 3.14159);

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _special_floats(void)
{
    /* C99 7.19.6.1: "inf", "nan" for %g, "INF", "NAN" for %G */
    CHECK_FMT("%g", INFINITY);
    CHECK_FMT("%g", -INFINITY);
    CHECK_FMT("%f", INFINITY);
    CHECK_FMT("%G", INFINITY);
    CHECK_FMT("%g", NAN);
    CHECK_FMT("%E", NAN);

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _dtoa_roundtrip(void)
{
    /* the shortest exact representation must read back bit for bit, and
       agree with the C library digit for digit */
    char got[64], want[64], *end = NULL;
    double d = 0.0, back = 0.0;
    uint64_t bits = 0;
    unsigned int i = 0;

    for (i = 0; i < 2000; i ++) {
        bits = ((uint64_t) rand() << 40) ^ ((uint64_t) rand() << 20) ^ rand();
        memcpy(& d, & bits, sizeof(d));
        if (isnan(d) || isinf(d)) continue;

        m_snprintf(got, sizeof(got), "%.17g", d);
        snprintf(want, sizeof(want), "%.17g", d);
        if (strcmp(got, want)) {
            test_fail(__FILE__, __LINE__, "%%.17g: got %s, libc %s", got, want);
            return -1;
        }

        back = strtod(got, & end);
        if (*end || memcmp(& back, & d, sizeof(d))) {
            test_fail(__FILE__, __LINE__, "%s does not read back", got);
            return -1;
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _sizing(void)
{
    char buf[8];
    int n = 0, ret = 0;

    /* a NULL buffer only measures */
    ASSERT_EQ_INT(m_snprintf(NULL, 0, "%d-%s", 12345, "abc"), 9);

    /* the output may be binary: a short buffer is filled exactly, without
       a terminator, and the full length is returned; an output that fits
       is terminated */
    for (n = 1; n <= 6; n ++) {
        memset(buf, 'X', sizeof(buf));
        ret = m_snprintf(buf, n, "%s", "abcdef");
        if (ret != 6 || strncmp(buf, "abcdef", n) || buf[n] != 'X') {
            test_fail(
                __FILE__, __LINE__, "size %d: ret %d, buffer \"%.8s\"",
                n, ret, buf
            );
        }
    }

    memset(buf, 'X', sizeof(buf));
    ASSERT_EQ_INT(m_snprintf(buf, 7, "%s", "abcdef"), 6);
    ASSERT_TRUE(! strcmp(buf, "abcdef"));
    ASSERT_EQ_INT(buf[7], 'X');

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _binary(void)
{
    /* the documented examples: 'b' writes raw bytes, 'B'/'b' force the
       endianness, 't'/'T' select the lower or higher bytes */
    unsigned char buf[16];
    unsigned int u = 0xdeadbeefU;
    unsigned long long ull = 0x0123456789abcdefULL;
    unsigned char native[sizeof(u)];

    memcpy(native, & u, sizeof(u));
    memset(buf, 0, sizeof(buf));
    ASSERT_EQ_INT(m_snprintf((char *) buf, sizeof(buf), "%bu", u), 4);
    ASSERT_EQ_MEM(buf, native, 4);

    ASSERT_EQ_INT(m_snprintf((char *) buf, sizeof(buf), "%bBu", u), 4);
    ASSERT_EQ_MEM(buf, "\xde\xad\xbe\xef", 4);

    ASSERT_EQ_INT(m_snprintf((char *) buf, sizeof(buf), "%bbu", u), 4);
    ASSERT_EQ_MEM(buf, "\xef\xbe\xad\xde", 4);

    ASSERT_EQ_INT(m_snprintf((char *) buf, sizeof(buf), "%3bBtu", u), 3);
    ASSERT_EQ_MEM(buf, "\xad\xbe\xef", 3);

    ASSERT_EQ_INT(m_snprintf((char *) buf, sizeof(buf), "%4bbTllu", ull), 4);
    ASSERT_EQ_MEM(buf, "\x67\x45\x23\x01", 4);

    ASSERT_EQ_INT(m_snprintf((char *) buf, sizeof(buf), "%bBllu", ull), 8);
    ASSERT_EQ_MEM(buf, "\x01\x23\x45\x67\x89\xab\xcd\xef", 8);

    ASSERT_EQ_INT(m_snprintf((char *) buf, sizeof(buf), "%bBhu", 0x0102), 2);
    ASSERT_EQ_MEM(buf, "\x01\x02", 2);

    /* and back */
    u = 0;
    ASSERT_EQ_INT(m_snscanf("\xde\xad\xbe\xef", 4, "%bBu", & u), 4);
    ASSERT_EQ_UINT(u, 0xdeadbeefU);
    u = 0;
    ASSERT_EQ_INT(m_snscanf("\xef\xbe\xad\xde", 4, "%bbu", & u), 4);
    ASSERT_EQ_UINT(u, 0xdeadbeefU);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _binary_signed(void)
{
    /* a negative value is written in two's complement, like any other */
    unsigned char buf[16];
    int i = -2;
    unsigned char native[sizeof(i)];

    memcpy(native, & i, sizeof(i));
    ASSERT_EQ_INT(m_snprintf((char *) buf, sizeof(buf), "%bd", i), 4);
    ASSERT_EQ_MEM(buf, native, 4);
    ASSERT_EQ_INT(m_snprintf((char *) buf, sizeof(buf), "%bBi", i), 4);
    ASSERT_EQ_MEM(buf, "\xff\xff\xff\xfe", 4);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _scan(void)
{
    int d = 0;
    unsigned int x = 0;
    char word[16] = "";

    /* the return value is the number of bytes consumed */
    ASSERT_EQ_INT(
        m_snscanf("42 abc ff|rest", 14, "%d %s %x", & d, word, & x), 9
    );
    ASSERT_EQ_INT(d, 42);
    ASSERT_TRUE(! strcmp(word, "abc"));
    ASSERT_EQ_UINT(x, 0xff);

    /* no match */
    ASSERT_EQ_INT(m_snscanf("nope", 4, "%d", & d), -1);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _scan_width(void)
{
    char *small = NULL;

    /* the '$' width takes the size of the output buffer, terminator included */
    ASSERT_NOT_NULL(small = malloc(4));
    ASSERT_EQ_INT(m_snscanf("hello", 5, "%$s", (size_t) 4, small), 3);
    ASSERT_TRUE(! strcmp(small, "hel"));
    ASSERT_EQ_INT(m_snscanf("hello", 5, "%$[a-z]", (size_t) 4, small), 3);
    ASSERT_TRUE(! strcmp(small, "hel"));
    ASSERT_EQ_INT(m_snscanf("hello", 5, "%$c", (size_t) 4, small), 4);
    ASSERT_EQ_MEM(small, "hell", 4);
    ASSERT_EQ_INT(m_snscanf("hello", 5, "%$s", (size_t) 1, small), -1);
    free(small);

    return 0;
}

/* -------------------------------------------------------------------------- */

static String *_vfmt(String *s, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    s = string_vfmt(s, fmt, args);
    va_end(args);

    return s;
}

/* -------------------------------------------------------------------------- */

static int _string_fmt(void)
{
    String *z = NULL;
    String ro = STRING_STATIC_INITIALIZER("abc", 3);

    /* append formatted output */
    ASSERT_NOT_NULL(z = string_alloc("abc", 3));
    ASSERT_EQ_PTR(string_catfmt(z, "%s%i", "-formatted-", 1234), z);
    ASSERT_EQ_STR(z, "abc-formatted-1234");
    z = string_free(z);

    /* to an empty string */
    ASSERT_NOT_NULL(z = string_alloc(NULL, 0));
    ASSERT_EQ_PTR(
        string_catfmt(z, "%lli,0,\"/%s\",%li,0\n", 0LL, "filename", 123456789L),
        z
    );
    ASSERT_EQ_STR(z, "0,0,\"/filename\",123456789,0\n");
    z = string_free(z);

    /* overwrite, or allocate */
    ASSERT_NOT_NULL(z = string_fmt(NULL, "%05.1f|%-4s|%x", 3.14159, "ab", 255));
    ASSERT_EQ_STR(z, "003.1|ab  |ff");
    ASSERT_EQ_PTR(string_fmt(z, "%c", 'z'), z);
    ASSERT_EQ_STR(z, "z");
    ASSERT_EQ_PTR(_vfmt(z, "%d+%d", 1, 2), z);
    ASSERT_EQ_STR(z, "1+2");
    z = string_free(z);

    /* a NULL string is allocated */
    ASSERT_NOT_NULL(z = string_catfmt(NULL, "%s", "new"));
    ASSERT_EQ_STR(z, "new");
    z = string_free(z);

    /* a bad format or a read-only string */
    ASSERT_NULL(string_fmt(NULL, NULL));
    ASSERT_NULL(string_fmt(& ro, "%d", 1));
    ASSERT_NULL(string_catfmt(& ro, "%d", 1));
    ASSERT_EQ_STR(& ro, "abc");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _peek_fetch(void)
{
    String *z = NULL;
    int d = 0;
    char word[16] = "";

    ASSERT_NOT_NULL(z = string_alloc("42 abc rest", 11));

    /* peek reads, fetch consumes */
    ASSERT_EQ_INT(string_peek_fmt(z, "%d %s", & d, word), 0);
    ASSERT_EQ_INT(d, 42);
    ASSERT_TRUE(! strcmp(word, "abc"));
    ASSERT_EQ_STR(z, "42 abc rest");

    ASSERT_EQ_INT(string_fetch_fmt(z, "%d", & d), 0);
    ASSERT_EQ_INT(d, 42);
    ASSERT_EQ_STR(z, " abc rest");

    ASSERT_EQ_INT(string_fetch_fmt(z, " %s", word), 0);
    ASSERT_TRUE(! strcmp(word, "abc"));
    ASSERT_EQ_STR(z, " rest");

    /* nothing matches */
    ASSERT_EQ_INT(string_peek_fmt(z, "%d", & d), -1);
    ASSERT_EQ_STR(z, " rest");
    ASSERT_EQ_INT(string_fetch_fmt(NULL, "%d", & d), -1);
    ASSERT_EQ_INT(string_peek_fmt(z, NULL, & d), -1);

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("integers", _integers),
    TEST("strings", _strings),
    TEST("floats", _floats),
    TEST_TODO("special_floats", _special_floats,
        "prints Inf and NaN where C99 says inf and nan"),
    TEST("dtoa_roundtrip", _dtoa_roundtrip),
    TEST("sizing", _sizing),
    TEST("binary", _binary),
    TEST_TODO("binary_signed", _binary_signed,
        "%bd writes the magnitude of a negative value"),
    TEST("scan", _scan),
    TEST("scan_width", _scan_width),
    TEST("string_fmt", _string_fmt),
    TEST("peek_fetch", _peek_fetch)
};

TEST_SUITE(test_suite_format, "format", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
