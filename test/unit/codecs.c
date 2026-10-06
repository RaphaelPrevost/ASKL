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
#include "../../lib/string/codecs.h"
#include "../../lib/string/crypto.h"

/* -------------------------------------------------------------------------- */

static void _random_bytes(char *out, size_t len)
{
    size_t i = 0;

    for (i = 0; i < len; i ++) out[i] = (char) (rand() & 0xff);
}

/* -------------------------------------------------------------------------- */

static int _b58_encode(void)
{
    String *z = NULL, *s = NULL;

    ASSERT_NOT_NULL(z = string_b58s("test string", 11));
    ASSERT_EQ_STR(z, "Vs5LyRhXt9nUp14");
    z = string_free(z);

    /* reference vector from the bitcoin test suite */
    ASSERT_NOT_NULL(z = string_b58s("Hello World!", 12));
    ASSERT_EQ_STR(z, "2NEpo7TZRRrLZSi2U");
    z = string_free(z);

    /* leading zero bytes are encoded as leading '1' */
    ASSERT_NOT_NULL(z = string_b58s("\0\0abc", 5));
    ASSERT_EQ_STR(z, "11ZiCa");
    z = string_free(z);

    /* String wrapper */
    ASSERT_NOT_NULL(s = string_alloc("test string", 11));
    ASSERT_NOT_NULL(z = string_b58(s));
    ASSERT_EQ_STR(z, "Vs5LyRhXt9nUp14");
    z = string_free(z); s = string_free(s);

    /* bad parameters */
    ASSERT_NULL(string_b58s(NULL, 3));
    ASSERT_NULL(string_b58s("abc", 0));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _b58_decode(void)
{
    String *z = NULL, *s = NULL;

    ASSERT_NOT_NULL(z = string_deb58s("Vs5LyRhXt9nUp14", 15));
    ASSERT_EQ_STR(z, "test string");
    z = string_free(z);

    ASSERT_NOT_NULL(z = string_deb58s("2NEpo7TZRRrLZSi2U", 17));
    ASSERT_EQ_STR(z, "Hello World!");
    z = string_free(z);

    ASSERT_NOT_NULL(z = string_deb58s("11ZiCa", 6));
    ASSERT_EQ_UINT(z->len, 5);
    ASSERT_EQ_MEM(z->data, "\0\0abc", 5);
    z = string_free(z);

    /* String wrapper */
    ASSERT_NOT_NULL(s = string_alloc("Vs5LyRhXt9nUp14", 15));
    ASSERT_NOT_NULL(z = string_deb58(s));
    ASSERT_EQ_STR(z, "test string");
    z = string_free(z); s = string_free(s);

    /* bad parameters */
    ASSERT_NULL(string_deb58s(NULL, 3));
    ASSERT_NULL(string_deb58s("abc", 0));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _b58_decode_invalid(void)
{
    /* characters outside the alphabet are rejected */
    ASSERT_NULL(string_deb58s("Vs5LyRhXt0nUp14", 15));
    ASSERT_NULL(string_deb58s("Vs5LyRhXtOnUp14", 15));
    ASSERT_NULL(string_deb58s("Vs5LyRhXtInUp14", 15));
    ASSERT_NULL(string_deb58s("Vs5LyRhXtlnUp14", 15));
    ASSERT_NULL(string_deb58s("Vs5LyRhXt nUp14", 15));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _b58_decode_short(void)
{
    static const char *inputs[] = { "a", "ab", "abc", "\xff", "\x01\x02" };
    unsigned int i = 0;
    String *encoded = NULL, *decoded = NULL;

    /* inputs shorter than 4 bytes */
    for (i = 0; i < sizeof(inputs) / sizeof(*inputs); i ++) {
        ASSERT_NOT_NULL(encoded = string_b58s(inputs[i], strlen(inputs[i])));
        if (! (decoded = string_deb58(encoded)) ) {
            test_fail(
                __FILE__, __LINE__, "cannot decode %.*s (%zu bytes)",
                (int) encoded->len, encoded->data, strlen(inputs[i])
            );
            return -1;
        }
        ASSERT_EQ_STR(decoded, inputs[i]);
        encoded = string_free(encoded); decoded = string_free(decoded);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _b58_roundtrip(void)
{
    char buffer[64];
    String *encoded = NULL, *decoded = NULL;
    unsigned int i = 0;
    size_t len = 0;

    /* every length up to 20 bytes with the largest and a typical value,
       then random lengths and contents */
    for (i = 0; i < 40 + 64; i ++) {
        if (i < 40) {
            len = 1 + i / 2;
            memset(buffer, (i & 1) ? 'A' : 0xff, len);
        } else {
            len = 1 + (rand() % sizeof(buffer));
            _random_bytes(buffer, len);
        }

        ASSERT_NOT_NULL(encoded = string_b58s(buffer, len));
        if (! (decoded = string_deb58(encoded)) ) {
            test_fail(
                __FILE__, __LINE__, "cannot decode %.*s (%zu bytes)",
                (int) encoded->len, encoded->data, len
            );
            return -1;
        }
        ASSERT_EQ_UINT(decoded->len, len);
        ASSERT_EQ_MEM(decoded->data, buffer, len);

        encoded = string_free(encoded); decoded = string_free(decoded);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _b64_encode(void)
{
    /* RFC 4648 section 10 test vectors */
    static const struct { const char *in; const char *out; } vectors[] = {
        { "f", "Zg==" },
        { "fo", "Zm8=" },
        { "foo", "Zm9v" },
        { "foob", "Zm9vYg==" },
        { "fooba", "Zm9vYmE=" },
        { "foobar", "Zm9vYmFy" },
        { "test string", "dGVzdCBzdHJpbmc=" }
    };
    static const char tail[] = { 'f', 'o', '\xff', '\xff' };
    unsigned int i = 0;
    String *z = NULL, *s = NULL;

    for (i = 0; i < sizeof(vectors) / sizeof(*vectors); i ++) {
        z = string_b64s(vectors[i].in, strlen(vectors[i].in), 0);
        ASSERT_NOT_NULL(z);
        ASSERT_EQ_STR(z, vectors[i].out);
        z = string_free(z);
    }

    /* String wrapper */
    ASSERT_NOT_NULL(s = string_alloc("foobar", 6));
    ASSERT_NOT_NULL(z = string_b64(s, 0));
    ASSERT_EQ_STR(z, "Zm9vYmFy");
    z = string_free(z); s = string_free(s);

    /* the bytes following the input must not leak into the padding */
    ASSERT_NOT_NULL(z = string_b64s(tail, 1, 0));
    ASSERT_EQ_STR(z, "Zg==");
    z = string_free(z);
    ASSERT_NOT_NULL(z = string_b64s(tail, 2, 0));
    ASSERT_EQ_STR(z, "Zm8=");
    z = string_free(z);

    /* bad parameters */
    ASSERT_NULL(string_b64s(NULL, 3, 0));
    ASSERT_NULL(string_b64s("abc", 0, 0));
    /* a line size must be a multiple of 4, no longer than a MIME line */
    ASSERT_NULL(string_b64s("abc", 3, 5));
    ASSERT_NULL(string_b64s("abc", 3, 80));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _b64_encode_lines(void)
{
    /* 57 bytes fill exactly one 76 characters MIME line */
    static const char line[] =
        "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
        "YWFhYWFhYWFh";
    char input[60];
    char expected[128];
    String *z = NULL;

    memset(input, 'a', sizeof(input));

    ASSERT_NOT_NULL(z = string_b64s(input, 57, 76));
    snprintf(expected, sizeof(expected), "%s\r\n", line);
    ASSERT_EQ_STR(z, expected);
    z = string_free(z);

    ASSERT_NOT_NULL(z = string_b64s(input, 60, 76));
    snprintf(expected, sizeof(expected), "%s\r\nYWFh\r\n", line);
    ASSERT_EQ_STR(z, expected);
    z = string_free(z);

    /* the output of a short input does not need to be wrapped */
    ASSERT_NOT_NULL(z = string_b64s("foobar", 6, 76));
    ASSERT_EQ_STR(z, "Zm9vYmFy\r\n");
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _b64_decode(void)
{
    static const struct { const char *in; const char *out; } vectors[] = {
        { "Zg==", "f" },
        { "Zm8=", "fo" },
        { "Zm9v", "foo" },
        { "Zm9vYg==", "foob" },
        { "Zm9vYmE=", "fooba" },
        { "Zm9vYmFy", "foobar" },
        { "dGVzdCBzdHJpbmc=", "test string" },
        { "Zg==\r\n", "f" },
        { "Zm8=\n", "fo" },
        { "Zm9v\r\n", "foo" }
    };
    unsigned int i = 0;
    String *z = NULL, *s = NULL;

    for (i = 0; i < sizeof(vectors) / sizeof(*vectors); i ++) {
        z = string_deb64s(vectors[i].in, strlen(vectors[i].in));
        ASSERT_NOT_NULL(z);
        ASSERT_EQ_STR(z, vectors[i].out);
        z = string_free(z);
    }

    /* String wrapper */
    ASSERT_NOT_NULL(s = string_alloc("Zm9vYmFy", 8));
    ASSERT_NOT_NULL(z = string_deb64(s));
    ASSERT_EQ_STR(z, "foobar");
    z = string_free(z); s = string_free(s);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _b64_decode_mime_url(void)
{
    /* base64url alphabet ('-' and '_'), wrapped with CRLF, no padding */
    static const char mime[] =
        "5Lul5ZGC5rOi6ICz5pys6YOo5q2iCuWNg-WIqeWltOa1geS5juWSjOWKoArppJjl"
        "pJrpgKPmm73m\r\n"
        "tKXnpaLpgqMK6Imv54mf5pyJ54K66IO95pa85LmFCuiAtuS4h-ioiOS4jeW3seih"
        "o-WkqQrpmL_k\r\n"
        "vZDkvI7llqnlpbPnvo7kuYsK5oG15q-U5q-b5Yui6aCI";
    /* the iroha poem, one verse per line */
    static const char expected[] =
        "以呂波耳本部止\n千利奴流乎和加\n餘多連曽津祢那\n良牟有為能於久\n"
        "耶万計不己衣天\n阿佐伎喩女美之\n恵比毛勢須";
    String *z = NULL;

    ASSERT_EQ_UINT(sizeof(mime) - 1, 200);
    ASSERT_NOT_NULL(z = string_deb64s(mime, sizeof(mime) - 1));
    ASSERT_EQ_STR(z, expected);
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _b64_decode_invalid(void)
{
    String *z = NULL;

    /* a single character cannot encode a byte */
    if ( (z = string_deb64s("A", 1)) ) {
        string_free(z);
        FAIL("an incomplete base64 quantum must be rejected");
    }

    /* characters outside the alphabet */
    if ( (z = string_deb64s("dGVzd!ee", 8)) ) {
        string_free(z);
        FAIL("a character outside the base64 alphabet must be rejected");
    }

    /* nothing to decode */
    if ( (z = string_deb64s("\r\n  \t", 5)) ) {
        string_free(z);
        FAIL("a whitespace-only input must be rejected");
    }

    ASSERT_NULL(string_deb64s(NULL, 3));
    ASSERT_NULL(string_deb64s("abc", 0));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _b64_roundtrip(void)
{
    char buffer[256];
    String *encoded = NULL, *decoded = NULL;
    unsigned int i = 0;
    size_t len = 0;

    for (i = 0; i < 64; i ++) {
        len = 1 + (rand() % sizeof(buffer));
        _random_bytes(buffer, len);

        ASSERT_NOT_NULL(encoded = string_b64s(buffer, len, (i & 1) ? 76 : 0));
        if (! (decoded = string_deb64(encoded)) ) {
            test_fail(
                __FILE__, __LINE__, "cannot decode %.*s (%zu bytes)",
                (int) encoded->len, encoded->data, len
            );
            return -1;
        }
        if (decoded->len != len) {
            test_fail(
                __FILE__, __LINE__, "%zu bytes -> %.*s -> %u bytes",
                len, (int) encoded->len, encoded->data, decoded->len
            );
            return -1;
        }
        ASSERT_EQ_MEM(decoded->data, buffer, len);

        encoded = string_free(encoded); decoded = string_free(decoded);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _urlencode_raw(void)
{
    char *out = NULL;

    /* unreserved characters are copied as is */
    ASSERT_NOT_NULL(out = string_rawurlencode("AZaz09-_.", 9, 0));
    ASSERT_TRUE(! strcmp(out, "AZaz09-_."));
    free(out);

    /* unsafe characters are escaped in lower case hexadecimal */
    ASSERT_NOT_NULL(out = string_rawurlencode("a b\"c<d>e", 9, 0));
    ASSERT_TRUE(! strcmp(out, "a%20b%22c%3cd%3ee"));
    free(out);

    /* control characters and DEL */
    ASSERT_NOT_NULL(out = string_rawurlencode("\x01\x1f\x7f", 3, 0));
    ASSERT_TRUE(! strcmp(out, "%01%1f%7f"));
    free(out);

    /* reserved characters are escaped on request */
    out = string_rawurlencode("/?", 2, RFC1738_ESCAPE_RESERVED);
    ASSERT_NOT_NULL(out);
    ASSERT_TRUE(! strcmp(out, "%2f%3f"));
    free(out);

    /* the overflow guard from the original test */
    ASSERT_NULL(string_rawurlencode("http://overflow.net", 1431655765, 0));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _urlencode_non_ascii(void)
{
    char *out = NULL;

    /* non US-ASCII bytes are escaped one by one */
    ASSERT_NOT_NULL(out = string_rawurlencode("\xc3\xa9\xff", 3, 0));
    ASSERT_TRUE(! strcmp(out, "%c3%a9%ff"));
    free(out);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _urlencode_string(void)
{
    String *s = NULL;

    /* the encoded data must replace the content, length included */
    ASSERT_NOT_NULL(s = string_alloc("a b", 3));
    ASSERT_EQ_INT(string_urlencode(s, 0), 0);
    ASSERT_EQ_STR(s, "a%20b");
    ASSERT_EQ_INT(s->data[s->len], '\0');
    s = string_free(s);

    /* nothing to escape, and stale tokens are dropped */
    ASSERT_NOT_NULL(s = string_alloc("a-b", 3));
    ASSERT_EQ_INT(string_split(s, "-", 1), 0);
    ASSERT_EQ_INT(string_urlencode(s, 0), 0);
    ASSERT_EQ_STR(s, "a-b");
    ASSERT_EQ_UINT(s->count, 0);
    s = string_free(s);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _urlencode_token(void)
{
    String *s = NULL;

    /* on a token, the parent grows with it */
    ASSERT_NOT_NULL(s = string_alloc("a b|c", 5));
    ASSERT_EQ_INT(string_split(s, "|", 1), 0);
    ASSERT_EQ_INT(string_urlencode(& s->tokens[0], 0), 0);
    ASSERT_EQ_STR(& s->tokens[0], "a%20b");
    ASSERT_EQ_STR(s, "a%20b|c");
    s = string_free(s);

    ASSERT_EQ_INT(string_urlencode(NULL, 0), -1);

    return 0;
}

/* -------------------------------------------------------------------------- */
#ifdef HAS_ZLIB
/* -------------------------------------------------------------------------- */

static int _deflate_inflate(void)
{
    char buffer[2048];
    unsigned int i = 0;
    String *s = NULL, *z = NULL, *back = NULL;

    /* highly compressible data */
    for (i = 0; i < sizeof(buffer); i ++) buffer[i] = "hello " [i % 6];

    ASSERT_NOT_NULL(s = string_alloc(buffer, sizeof(buffer)));
    ASSERT_NOT_NULL(z = string_deflate(s));
    ASSERT_TRUE(z->len < s->len);
    ASSERT_NOT_NULL(back = string_inflate(z, s->len));
    ASSERT_EQ_UINT(back->len, s->len);
    ASSERT_EQ_MEM(back->data, s->data, s->len);
    back = string_free(back); z = string_free(z); s = string_free(s);

    /* incompressible data survives the round trip too */
    _random_bytes(buffer, sizeof(buffer));
    ASSERT_NOT_NULL(s = string_alloc(buffer, sizeof(buffer)));
    ASSERT_NOT_NULL(z = string_deflate(s));
    ASSERT_NOT_NULL(back = string_inflate(z, s->len));
    ASSERT_EQ_UINT(back->len, s->len);
    ASSERT_EQ_MEM(back->data, s->data, s->len);
    back = string_free(back); z = string_free(z); s = string_free(s);

    ASSERT_NULL(string_deflate(NULL));
    ASSERT_NULL(string_inflate(NULL, 10));

    return 0;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

static int _sha1(void)
{
    /* FIPS 180-1 and block boundary vectors */
    static const struct {
        const char *in; size_t len; const char *hex;
    } v[] = {
        { "abc", 3, "a9993e364706816aba3e25717850c26c9cd0d89d" },
        { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
          "84983e441c3bd26ebaae4aa1f95129e5e54670f1" },
        { "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 55,
          "cef734ba81a024479e09eb5a75b6ddae62e6abf1" },
        { "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 56,
          "901305367c259952f4e7af8323f480d59f81335b" },
        { "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
          "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 64,
          "bb2fa3ee7afb9f54c6dfb5d021f14b1ffe40c163" }
    };
    unsigned int i = 0;
    String *z = NULL, *s = NULL;
    char *million = NULL;

    for (i = 0; i < sizeof(v) / sizeof(*v); i ++) {
        ASSERT_NOT_NULL(z = string_sha1s(v[i].in, v[i].len));
        ASSERT_EQ_STR(z, v[i].hex);
        z = string_free(z);
    }

    /* one million 'a' */
    ASSERT_NOT_NULL(million = malloc(1000000));
    memset(million, 'a', 1000000);
    z = string_sha1s(million, 1000000);
    free(million);
    ASSERT_NOT_NULL(z);
    ASSERT_EQ_STR(z, "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    z = string_free(z);

    /* String wrapper, on a token */
    ASSERT_NOT_NULL(s = string_alloc("xyz|abc", 7));
    ASSERT_EQ_INT(string_split(s, "|", 1), 0);
    ASSERT_NOT_NULL(z = string_sha1(& s->tokens[1]));
    ASSERT_EQ_STR(z, "a9993e364706816aba3e25717850c26c9cd0d89d");
    z = string_free(z); s = string_free(s);

    ASSERT_NULL(string_sha1s(NULL, 3));
    ASSERT_NULL(string_sha1s("abc", 0));

    return 0;
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("b58_encode", _b58_encode),
    TEST("b58_decode", _b58_decode),
    TEST("b58_decode_invalid", _b58_decode_invalid),
    TEST("b58_decode_short", _b58_decode_short),
    TEST("b58_roundtrip", _b58_roundtrip),
    TEST("b64_encode", _b64_encode),
    TEST("b64_encode_lines", _b64_encode_lines),
    TEST("b64_decode", _b64_decode),
    TEST("b64_decode_mime_url", _b64_decode_mime_url),
    TEST("b64_decode_invalid", _b64_decode_invalid),
    TEST("b64_roundtrip", _b64_roundtrip),
    TEST("urlencode_raw", _urlencode_raw),
    TEST("urlencode_non_ascii", _urlencode_non_ascii),
    TEST("urlencode_string", _urlencode_string),
    TEST("urlencode_token", _urlencode_token),
    #ifdef HAS_ZLIB
    TEST("deflate_inflate", _deflate_inflate),
    #endif
    TEST("sha1", _sha1)
};

TEST_SUITE(test_suite_codecs, "codecs", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
