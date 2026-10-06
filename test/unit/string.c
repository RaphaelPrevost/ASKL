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
#include <locale.h>

/* the wide character tests need a UTF-8 locale; none is guaranteed */
static int _utf8_locale = 0;

/* -------------------------------------------------------------------------- */

static int _setup(void)
{
    static const char *candidates[] = {
        "C.UTF-8", "C.utf8", "en_US.UTF-8", "en_US.utf8", "UTF-8"
    };
    unsigned int i = 0;

    _utf8_locale = 0;

    for (i = 0; i < sizeof(candidates) / sizeof(*candidates); i ++) {
        if (setlocale(LC_CTYPE, candidates[i])) {
            _utf8_locale = 1;
            break;
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static void _teardown(void)
{
    setlocale(LC_CTYPE, "C");
}

/* -------------------------------------------------------------------------- */

static int _alloc_free(void)
{
    String *z = NULL;

    /* uninitialized buffer of a given size */
    ASSERT_NOT_NULL(z = string_alloc(NULL, 256));
    ASSERT_EQ_UINT(string_len(z), 256);
    ASSERT_TRUE(string_capacity(z) >= 256);
    ASSERT_EQ_UINT(string_available(z), string_capacity(z) - 256);
    ASSERT_NULL(z->tokens);
    ASSERT_EQ_UINT(z->count, 0);
    ASSERT_NULL(z->parent);
    ASSERT_NULL(z = string_free(z));

    /* empty string, no buffer */
    ASSERT_NOT_NULL(z = string_alloc(NULL, 0));
    ASSERT_EQ_UINT(z->len, 0);
    ASSERT_NULL(z->data);
    z = string_free(z);

    /* copy of a buffer, NUL terminated */
    ASSERT_NOT_NULL(z = string_alloc("abc", 3));
    ASSERT_EQ_STR(z, "abc");
    ASSERT_EQ_INT(z->data[3], '\0');
    z = string_free(z);

    /* embedded NUL bytes are data */
    ASSERT_NOT_NULL(z = string_alloc("a\0b", 3));
    ASSERT_EQ_UINT(z->len, 3);
    ASSERT_EQ_MEM(z->data, "a\0b", 3);
    z = string_free(z);

    /* reserve extra room */
    ASSERT_NOT_NULL(z = string_reserve("abc", 3, 100));
    ASSERT_EQ_STR(z, "abc");
    ASSERT_TRUE(string_capacity(z) >= 103);
    z = string_free(z);

    /* NULL is harmless */
    ASSERT_NULL(string_free(NULL));
    ASSERT_EQ_UINT(string_len(NULL), (size_t) -1);
    ASSERT_EQ_UINT(string_capacity(NULL), (size_t) -1);


    /* the allocation size is computed in 32 bits */
    ASSERT_NULL(string_alloc(NULL, 4294967293U));
    ASSERT_NULL(string_reserve(NULL, (size_t) -1, 2));
    return 0;
}

/* -------------------------------------------------------------------------- */

static int _resize(void)
{
    String *z = NULL;

    ASSERT_NOT_NULL(z = string_alloc(NULL, 256));

    /* growing the buffer keeps the length */
    ASSERT_EQ_INT(string_resize(z, 512), 0);
    ASSERT_TRUE(string_capacity(z) >= 512);
    ASSERT_EQ_UINT(string_len(z), 256);

    /* shrinking truncates the content */
    ASSERT_EQ_INT(string_resize(z, 100), 0);
    ASSERT_EQ_UINT(string_len(z), 100);
    ASSERT_EQ_INT(z->data[100], '\0');

    /* the wrappers only resize in one direction */
    ASSERT_EQ_INT(string_extend(z, 50), 0);
    ASSERT_EQ_UINT(string_len(z), 100);
    ASSERT_EQ_INT(string_shrink(z, 1000), 0);
    ASSERT_EQ_UINT(string_len(z), 100);
    ASSERT_EQ_INT(string_extend(z, 1000), 0);
    ASSERT_TRUE(string_capacity(z) >= 1000);
    ASSERT_EQ_INT(string_shrink(z, 10), 0);
    ASSERT_EQ_UINT(string_len(z), 10);

    /* bad parameters */
    ASSERT_EQ_INT(string_resize(z, 0), -1);
    ASSERT_EQ_INT(string_resize(NULL, 10), -1);
    ASSERT_EQ_INT(string_extend(NULL, 10), -1);
    ASSERT_EQ_INT(string_shrink(NULL, 10), -1);

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _encaps(void)
{
    char buffer[4] = "abc";
    String *z = NULL;

    ASSERT_NOT_NULL(z = string_encaps(buffer, 3));
    ASSERT_EQ_PTR(z->data, buffer);
    ASSERT_EQ_STR(z, "abc");

    /* an encapsulated buffer cannot grow */
    ASSERT_EQ_INT(string_resize(z, 10), -1);
    ASSERT_EQ_INT(string_wchar(z), -1);
    ASSERT_EQ_STR(z, "abc");

    /* it can be searched */
    ASSERT_EQ_INT(string_find(z, 0, "c", 1), 2);

    /* freeing the String leaves the buffer alone */
    z = string_free(z);
    ASSERT_TRUE(! strcmp(buffer, "abc"));

    ASSERT_NULL(string_encaps(NULL, 3));
    ASSERT_NULL(string_encaps(buffer, 0));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _static_initializer(void)
{
    String s = STRING_STATIC_INITIALIZER("abc", 3);

    ASSERT_EQ_STR(& s, "abc");
    ASSERT_EQ_INT(string_find(& s, 0, "bc", 2), 1);

    /* read-only */
    ASSERT_EQ_INT(string_upper(& s), -1);
    ASSERT_EQ_STR(& s, "abc");
    ASSERT_EQ_INT(string_resize(& s, 10), -1);

    /* string_free() must not free a static String */
    ASSERT_NULL(string_free(& s));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _fetch(void)
{
    static const char bytes[] = "\x01" "\x02\x03" "\x04\x05\x06\x07"
                                "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f" "xyz";
    char small[1] = { 'q' };
    char out[3];
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    uint64_t u64 = 0;
    String *z = NULL;

    ASSERT_NOT_NULL(z = string_alloc(bytes, 18));

    /* integers are fetched in host byte order and consumed */
    ASSERT_EQ_UINT(string_fetch_uint8(z), 1);
    ASSERT_EQ_UINT(z->len, 17);

    memcpy(& u16, bytes + 1, sizeof(u16));
    ASSERT_EQ_UINT(string_fetch_uint16(z), u16);
    ASSERT_EQ_UINT(z->len, 15);

    memcpy(& u32, bytes + 3, sizeof(u32));
    ASSERT_EQ_UINT(string_fetch_uint32(z), u32);
    ASSERT_EQ_UINT(z->len, 11);

    memcpy(& u64, bytes + 7, sizeof(u64));
    ASSERT_EQ_UINT(string_fetch_uint64(z), u64);
    ASSERT_EQ_UINT(z->len, 3);

    ASSERT_EQ_INT(string_fetch_buffer(z, out, 3), 0);
    ASSERT_EQ_MEM(out, "xyz", 3);
    ASSERT_EQ_UINT(z->len, 0);

    /* nothing left to fetch */
    ASSERT_EQ_INT(string_fetch_buffer(z, out, 1), -1);
    ASSERT_EQ_UINT(string_fetch_uint8(z), 0);

    z = string_free(z);

    /* fetching from an encapsulated buffer */
    ASSERT_NOT_NULL(z = string_encaps(small, 1));
    ASSERT_EQ_UINT(string_fetch_uint8(z), 'q');
    ASSERT_EQ_UINT(z->len, 0);
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _clone(void)
{
    String *s = NULL, *c = NULL;

    ASSERT_NOT_NULL(s = string_alloc("13-10-15:Forever", 16));
    ASSERT_EQ_INT(string_split(s, ":", 1), 0);
    ASSERT_EQ_INT(string_split(& s->tokens[0], "-", 1), 0);

    /* the clone has its own buffer and the same token tree */
    ASSERT_NOT_NULL(c = string_clone(s));
    ASSERT_TRUE(c->data != s->data);
    ASSERT_EQ_STR(c, "13-10-15:Forever");
    ASSERT_EQ_UINT(c->count, 2);
    ASSERT_EQ_STR(& c->tokens[0], "13-10-15");
    ASSERT_EQ_STR(& c->tokens[1], "Forever");
    ASSERT_EQ_PTR(c->tokens[1].parent, c);
    ASSERT_EQ_UINT(c->tokens[0].count, 3);
    ASSERT_EQ_STR(& c->tokens(0, 2), "15");
    ASSERT_EQ_PTR(c->tokens(0, 2).parent, & c->tokens[0]);
    ASSERT_TRUE(c->tokens(0, 2).data >= c->data);
    ASSERT_TRUE(c->tokens(0, 2).data < c->data + c->len);

    /* altering the clone leaves the original alone */
    ASSERT_EQ_INT(string_upper(c), 0);
    ASSERT_EQ_STR(c, "13-10-15:FOREVER");
    ASSERT_EQ_STR(s, "13-10-15:Forever");
    c = string_free(c);

    /* clone with extra room */
    ASSERT_NOT_NULL(c = string_clone_reserve(s, 100));
    ASSERT_EQ_STR(c, "13-10-15:Forever");
    ASSERT_TRUE(string_capacity(c) >= 116);
    c = string_free(c);

    s = string_free(s);

    ASSERT_NULL(string_clone(NULL));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _find(void)
{
    String *a = NULL;
    char needle[300];

    ASSERT_NOT_NULL(a = string_alloc("Random string1234", 17));

    /* short needles use the naive search, longer ones Boyer-Moore */
    ASSERT_EQ_INT(string_find(a, 0, "st", 2), 7);
    ASSERT_EQ_INT(string_find(a, 0, "Random", 6), 0);
    ASSERT_EQ_INT(string_find(a, 0, "string1234", 10), 7);
    ASSERT_EQ_INT(string_find(a, 0, "1234", 4), 13);
    ASSERT_EQ_INT(string_find(a, 0, "4", 1), 16);
    ASSERT_EQ_INT(string_find(a, 0, "34", 2), 15);
    ASSERT_EQ_INT(string_find(a, 0, "R", 1), 0);

    /* starting offset */
    ASSERT_EQ_INT(string_find(a, 1, "R", 1), -1);
    ASSERT_EQ_INT(string_find(a, 8, "st", 2), -1);
    ASSERT_EQ_INT(string_find(a, 7, "st", 2), 7);

    /* not found */
    ASSERT_EQ_INT(string_find(a, 0, "xyz", 3), -1);
    ASSERT_EQ_INT(string_find(a, 0, "Random strinG", 13), -1);
    ASSERT_EQ_INT(string_find(a, 0, "Random string12345", 18), -1);
    ASSERT_EQ_INT(string_find(a, 17, "4", 1), -1);

    /* a needle longer than 255 bytes is rejected */
    memset(needle, 'a', sizeof(needle));
    ASSERT_EQ_INT(string_find(a, 0, needle, 256), -1);
    ASSERT_EQ_INT(string_find(a, 0, needle, 0), -1);
    ASSERT_EQ_INT(string_find(a, 0, NULL, 1), -1);
    ASSERT_EQ_INT(string_find(NULL, 0, "a", 1), -1);

    a = string_free(a);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _find_pattern(void)
{
    /* XXX String_Pattern is opaque and has no public size or allocator,
       so it cannot be used outside the library without this kind of
       oversized, suitably aligned storage */
    static union { size_t align; char bytes[4096]; } storage;
    String *a = NULL;
    String_Pattern *p = (String_Pattern *) storage.bytes;
    const char *needle = "string";

    ASSERT_NOT_NULL(a = string_alloc("string 1, string 2", 18));

    /* a precompiled pattern can be reused */
    ASSERT_EQ_INT(string_pattern_compile(p, needle, 6), 0);
    ASSERT_EQ_INT(string_find_pattern(a, 0, needle, p), 0);
    ASSERT_EQ_INT(string_find_pattern(a, 1, needle, p), 10);
    ASSERT_EQ_INT(string_find_pattern(a, 11, needle, p), -1);
    ASSERT_EQ_INT(string_find_pattern(a, 18, needle, p), -1);

    ASSERT_EQ_INT(string_pattern_compile(p, needle, 0), -1);
    ASSERT_EQ_INT(string_pattern_compile(p, NULL, 3), -1);
    ASSERT_EQ_INT(string_pattern_compile(NULL, needle, 3), -1);

    a = string_free(a);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _wchar_mbyte(void)
{
    static const char utf8[] = "这个服务器有没有问题";
    String *w = NULL;

    if (! _utf8_locale) SKIP("no UTF-8 locale available");

    ASSERT_NOT_NULL(w = string_alloc(utf8, 30));

    /* ten ideograms */
    ASSERT_EQ_INT(string_wchar(w), 0);
    ASSERT_EQ_UINT(w->len, 10 * sizeof(wchar_t));
    ASSERT_EQ_UINT(((wchar_t *) w->data)[0], 0x8fd9);
    ASSERT_EQ_UINT(((wchar_t *) w->data)[9], 0x9898);

    /* and back */
    ASSERT_EQ_INT(string_mbyte(w), 0);
    ASSERT_EQ_STR(w, utf8);

    w = string_free(w);

    /* invalid multibyte sequences leave the string unchanged */
    ASSERT_NOT_NULL(w = string_alloc("\xff\xfe", 2));
    ASSERT_EQ_INT(string_wchar(w), -1);
    ASSERT_EQ_UINT(w->len, 2);
    ASSERT_EQ_MEM(w->data, "\xff\xfe", 2);
    w = string_free(w);

    ASSERT_EQ_INT(string_wchar(NULL), -1);
    ASSERT_EQ_INT(string_mbyte(NULL), -1);

    return 0;
}

/* -------------------------------------------------------------------------- */
#ifdef HAS_ICONV
/* -------------------------------------------------------------------------- */

static int _conv(void)
{
    static const char gb18030[] = "\xd5\xe2\xb8\xf6\xb7\xfe\xce\xf1\xc6\xf7"
                                  "\xd3\xd0\xc3\xbb\xd3\xd0\xce\xca\xcc\xe2";
    static const char utf8[] = "这个服务器有没有问题";
    char out[64];
    String *z = NULL;

    /* dry run: size of the converted output */
    ASSERT_EQ_UINT(string_convs(gb18030, 20, "GB18030", NULL, 0, "UTF-8"), 30);

    /* conversion to a buffer */
    ASSERT_EQ_INT(
        string_convs(gb18030, 20, "GB18030", out, sizeof(out), "UTF-8"), 0
    );
    ASSERT_EQ_MEM(out, utf8, 30);

    /* in place conversion, the string grows */
    ASSERT_NOT_NULL(z = string_alloc(gb18030, 20));
    ASSERT_EQ_INT(string_conv(z, "GB18030", "UTF-8"), 0);
    ASSERT_EQ_STR(z, utf8);

    /* and shrinks */
    ASSERT_EQ_INT(string_conv(z, "UTF-8", "GB18030"), 0);
    ASSERT_EQ_UINT(z->len, 20);
    ASSERT_EQ_MEM(z->data, gb18030, 20);
    z = string_free(z);

    /* invalid input leaves the string unchanged */
    ASSERT_NOT_NULL(z = string_alloc("\xff\xfe", 2));
    ASSERT_EQ_INT(string_conv(z, "UTF-8", "GB18030"), -1);
    ASSERT_EQ_UINT(z->len, 2);
    ASSERT_EQ_MEM(z->data, "\xff\xfe", 2);
    z = string_free(z);

    ASSERT_EQ_INT(string_conv(NULL, "UTF-8", "GB18030"), -1);
    ASSERT_EQ_UINT(
        string_convs(NULL, 20, "GB18030", NULL, 0, "UTF-8"), (size_t) -1
    );

    return 0;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

static int _append_prepend(void)
{
    static const char utf8[] = "这个服务器有没有问题";
    String *w = NULL, *a = NULL, *z = NULL;

    ASSERT_NOT_NULL(w = string_alloc(utf8, 30));
    ASSERT_NOT_NULL(a = string_alloc("Random string1234", 17));

    /* append a String */
    ASSERT_EQ_PTR(string_append(w, a), w);
    ASSERT_EQ_UINT(w->len, 47);
    ASSERT_EQ_INT(string_find(w, 0, "Random", 6), 30);
    ASSERT_EQ_INT(string_find(w, 0, "问题", 6), 24);
    ASSERT_EQ_STR(a, "Random string1234");

    /* prepend a String */
    ASSERT_EQ_PTR(string_prepend(w, a), w);
    ASSERT_EQ_UINT(w->len, 64);
    ASSERT_EQ_INT(string_find(w, 0, "Random", 6), 0);
    ASSERT_EQ_INT(string_find(w, 0, "问题", 6), 41);
    w = string_free(w); a = string_free(a);

    /* buffers */
    ASSERT_NOT_NULL(w = string_alloc("abc", 3));
    ASSERT_EQ_PTR(string_append_buffer(w, "def", 3), w);
    ASSERT_EQ_STR(w, "abcdef");
    ASSERT_EQ_PTR(string_prepend_buffer(w, "xyz", 3), w);
    ASSERT_EQ_STR(w, "xyzabcdef");
    ASSERT_EQ_INT(w->data[9], '\0');

    /* appending a string to itself doubles it */
    ASSERT_EQ_PTR(string_append(w, w), w);
    ASSERT_EQ_STR(w, "xyzabcdefxyzabcdef");

    /* appending nothing is a no-op */
    ASSERT_EQ_PTR(string_append_buffer(w, "", 0), w);
    ASSERT_EQ_STR(w, "xyzabcdefxyzabcdef");
    w = string_free(w);

    ASSERT_NULL(string_append(NULL, NULL));
    ASSERT_NULL(string_prepend(NULL, NULL));


    /* a String created without a buffer can be appended to */
    ASSERT_NOT_NULL(z = string_alloc(NULL, 0));
    ASSERT_EQ_PTR(string_append_buffer(z, "[", 1), z);
    ASSERT_EQ_STR(z, "[");
    ASSERT_EQ_PTR(string_append_buffer(z, "1]", 2), z);
    ASSERT_EQ_STR(z, "[1]");
    z = string_free(z);

    /* and prepended to */
    ASSERT_NOT_NULL(z = string_alloc(NULL, 0));
    ASSERT_EQ_PTR(string_prepend_buffer(z, "abc", 3), z);
    ASSERT_EQ_STR(z, "abc");
    z = string_free(z);

    /* a NULL destination allocates the string */
    ASSERT_NOT_NULL(w = string_append_buffer(NULL, "abc", 3));
    ASSERT_EQ_STR(w, "abc");
    w = string_free(w);

    ASSERT_NOT_NULL(w = string_splice(NULL, 0, "abc", 3));
    ASSERT_EQ_STR(w, "abc");
    w = string_free(w);

    ASSERT_NOT_NULL(w = string_prepend_buffer(NULL, "abc", 3));
    ASSERT_EQ_STR(w, "abc");
    w = string_free(w);

    /* but not at an offset into it */
    ASSERT_NULL(string_splice(NULL, 1, "abc", 3));
    ASSERT_NULL(string_append_buffer(NULL, NULL, 3));
    return 0;
}

/* -------------------------------------------------------------------------- */

static int _compare(void)
{
    String *a = NULL, *b = NULL;

    ASSERT_NOT_NULL(a = string_alloc("abc", 3));
    ASSERT_NOT_NULL(b = string_alloc("abd", 3));

    ASSERT_EQ_INT(string_compare_buffer(a, "abc", 3), 0);
    ASSERT_TRUE(string_compare_buffer(a, "abd", 3) < 0);
    ASSERT_TRUE(string_compare_buffer(a, "abb", 3) > 0);
    ASSERT_TRUE(string_compare(a, b) < 0);
    ASSERT_TRUE(string_compare(b, a) > 0);
    ASSERT_EQ_INT(string_compare(a, a), 0);

    /* only the common prefix is compared */
    ASSERT_EQ_INT(string_compare_buffer(a, "abcdef", 6), 0);
    ASSERT_EQ_INT(string_compare_buffer(a, "ab", 2), 0);

    /* bad parameters are not equal to anything */
    ASSERT_NE_INT(string_compare_buffer(a, NULL, 3), 0);
    ASSERT_NE_INT(string_compare_buffer(a, "abc", 0), 0);
    ASSERT_NE_INT(string_compare_buffer(NULL, "abc", 3), 0);
    ASSERT_NE_INT(string_compare(a, NULL), 0);

    a = string_free(a); b = string_free(b);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _upper_lower(void)
{
    String *z = NULL;

    ASSERT_NOT_NULL(z = string_alloc("One two", 7));
    ASSERT_EQ_INT(string_upper(z), 0);
    ASSERT_EQ_STR(z, "ONE TWO");
    ASSERT_EQ_INT(string_lower(z), 0);
    ASSERT_EQ_STR(z, "one two");

    /* on a token, only the token is converted and tokens are kept */
    ASSERT_EQ_INT(string_split(z, " ", 1), 0);
    ASSERT_EQ_INT(string_upper(& z->tokens[1]), 0);
    ASSERT_EQ_STR(z, "one TWO");
    ASSERT_EQ_UINT(z->count, 2);
    ASSERT_EQ_STR(& z->tokens[0], "one");
    ASSERT_EQ_STR(& z->tokens[1], "TWO");
    z = string_free(z);

    ASSERT_EQ_INT(string_upper(NULL), -1);
    ASSERT_EQ_INT(string_lower(NULL), -1);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _split_merge(void)
{
    String *w = NULL;

    ASSERT_NOT_NULL(w = string_alloc("13-10-15:Forever", 16));

    /* split, then split a token */
    ASSERT_EQ_INT(string_split(w, ":", 1), 0);
    ASSERT_EQ_UINT(w->count, 2);
    ASSERT_EQ_STR(& w->tokens[0], "13-10-15");
    ASSERT_EQ_STR(& w->tokens[1], "Forever");
    ASSERT_EQ_PTR(w->tokens[0].parent, w);

    ASSERT_EQ_INT(string_split(& w->tokens[0], "-", 1), 0);
    ASSERT_EQ_UINT(w->tokens[0].count, 3);
    ASSERT_EQ_STR(& w->tokens(0, 0), "13");
    ASSERT_EQ_STR(& w->tokens(0, 1), "10");
    ASSERT_EQ_STR(& w->tokens(0, 2), "15");
    ASSERT_EQ_PTR(w->tokens(0, 2).parent, & w->tokens[0]);

    /* prepend to a subtoken: every ancestor grows */
    ASSERT_NOT_NULL(string_prepend_buffer(& w->tokens(0, 2), "20", 2));
    ASSERT_EQ_STR(& w->tokens(0, 2), "2015");
    ASSERT_EQ_STR(& w->tokens[0], "13-10-2015");
    ASSERT_EQ_STR(& w->tokens[1], "Forever");
    ASSERT_EQ_STR(w, "13-10-2015:Forever");

    /* merge the subtokens with a new delimiter of the same length */
    ASSERT_EQ_INT(string_merge(& w->tokens[0], "/", 1), 0);
    ASSERT_EQ_STR(& w->tokens[0], "13/10/2015");
    ASSERT_EQ_STR(w, "13/10/2015:Forever");

    /* merge the tokens with a longer delimiter */
    ASSERT_EQ_INT(string_merge(w, " -> ", 4), 0);
    ASSERT_EQ_STR(w, "13/10/2015 -> Forever");
    ASSERT_EQ_STR(& w->tokens[0], "13/10/2015");
    ASSERT_EQ_STR(& w->tokens[1], "Forever");
    ASSERT_EQ_STR(& w->tokens(0, 2), "2015");

    /* shrink a token: the parent shrinks with it */
    ASSERT_EQ_INT(string_resize(& w->tokens[0], 5), 0);
    ASSERT_EQ_STR(& w->tokens[0], "13/10");
    ASSERT_EQ_STR(w, "13/10 -> Forever");
    ASSERT_EQ_STR(& w->tokens[1], "Forever");

    /* cut the delimiter out */
    ASSERT_EQ_INT(string_cut(w, w->tokens[0].len, 4, NULL), 0);
    ASSERT_EQ_STR(w, "13/10Forever");

    /* split again */
    ASSERT_EQ_INT(string_split(w, "/", 1), 0);
    ASSERT_EQ_UINT(w->count, 2);
    ASSERT_EQ_STR(& w->tokens[0], "13");
    ASSERT_EQ_STR(& w->tokens[1], "10Forever");

    w = string_free(w);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _split_fields(unsigned int delimiters)
{
    char buffer[256];
    unsigned int i = 0, fields = delimiters + 1;
    String *z = NULL;

    /* "0,1,2,...", one digit per field */
    for (i = 0; i < fields; i ++) {
        buffer[2 * i] = '0' + (char) (i % 10);
        buffer[2 * i + 1] = ',';
    }

    ASSERT_NOT_NULL(z = string_alloc(buffer, 2 * fields - 1));

    /* split, then split again to go through the token reuse path */
    for (i = 0; i < 2; i ++) {
        ASSERT_EQ_INT(string_split(z, ",", 1), 0);
        ASSERT_EQ_UINT(z->count, fields);
    }

    for (i = 0; i < fields; i ++) {
        if (z->tokens[i].len != 1 || z->tokens[i].data[0] != '0' + i % 10) {
            test_fail(
                __FILE__, __LINE__, "%u delimiters: token %u is \"%.*s\"",
                delimiters, i, (int) z->tokens[i].len, z->tokens[i].data
            );
            break;
        }
    }

    z = string_free(z);

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _split_many(void)
{
    /* the offsets are collected 16 at a time: a full batch followed by
       the tail only, a full batch followed by a real one, three batches */
    static const unsigned int delimiters[] = { 15, 16, 17, 32, 33, 50 };
    unsigned int i = 0;

    for (i = 0; i < sizeof(delimiters) / sizeof(*delimiters); i ++)
        if (_split_fields(delimiters[i]) == -1) return -1;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _split_edge(void)
{
    String *z = NULL;

    /* consecutive delimiters give an empty token */
    ASSERT_NOT_NULL(z = string_alloc("a::b", 4));
    ASSERT_EQ_INT(string_split(z, ":", 1), 0);
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_STR(& z->tokens[0], "a");
    ASSERT_EQ_UINT(z->tokens[1].len, 0);
    ASSERT_EQ_STR(& z->tokens[2], "b");
    z = string_free(z);

    /* a trailing delimiter gives an empty last token */
    ASSERT_NOT_NULL(z = string_alloc("a:b:", 4));
    ASSERT_EQ_INT(string_split(z, ":", 1), 0);
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_STR(& z->tokens[1], "b");
    ASSERT_EQ_UINT(z->tokens[2].len, 0);
    z = string_free(z);

    /* no delimiter: the whole string is one token */
    ASSERT_NOT_NULL(z = string_alloc("abcd", 4));
    ASSERT_EQ_INT(string_split(z, ":", 1), 0);
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_EQ_STR(& z->tokens[0], "abcd");
    z = string_free(z);

    /* multibyte delimiter */
    ASSERT_NOT_NULL(z = string_alloc("one----two----three", 19));
    ASSERT_EQ_INT(string_split(z, "----", 4), 0);
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_STR(& z->tokens[0], "one");
    ASSERT_EQ_STR(& z->tokens[1], "two");
    ASSERT_EQ_STR(& z->tokens[2], "three");

    /* splitting again replaces the tokens */
    ASSERT_EQ_INT(string_split(z, "t", 1), 0);
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_STR(& z->tokens[0], "one----");
    ASSERT_EQ_STR(& z->tokens[1], "wo----");
    ASSERT_EQ_STR(& z->tokens[2], "hree");
    z = string_free(z);

    /* bad parameters */
    ASSERT_NOT_NULL(z = string_alloc("abc", 3));
    ASSERT_EQ_INT(string_split(z, NULL, 1), -1);
    ASSERT_EQ_INT(string_split(z, ":", 0), -1);
    ASSERT_EQ_INT(string_split(NULL, ":", 1), -1);
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _merge_edge(void)
{
    String *z = NULL;

    z = string_alloc("Test merging with a 0-length delimiter", 38);
    ASSERT_NOT_NULL(z);
    ASSERT_EQ_INT(string_split(z, " ", 1), 0);
    ASSERT_EQ_UINT(z->count, 6);

    /* a NULL delimiter is only allowed with a zero length */
    ASSERT_EQ_INT(string_merge(z, NULL, 1), -1);
    ASSERT_EQ_STR(z, "Test merging with a 0-length delimiter");

    /* merging with an empty delimiter joins the tokens */
    ASSERT_EQ_INT(string_merge(z, NULL, 0), 0);
    ASSERT_EQ_STR(z, "Testmergingwitha0-lengthdelimiter");
    ASSERT_EQ_UINT(z->count, 6);
    ASSERT_EQ_STR(& z->tokens[0], "Test");
    ASSERT_EQ_STR(& z->tokens[5], "delimiter");
    z = string_free(z);

    /* longer delimiter, then shorter, then longer again */
    ASSERT_NOT_NULL(z = string_alloc("a,b,c", 5));
    ASSERT_EQ_INT(string_split(z, ",", 1), 0);
    ASSERT_EQ_INT(string_merge(z, "::", 2), 0);
    ASSERT_EQ_STR(z, "a::b::c");
    ASSERT_EQ_INT(string_merge(z, "-", 1), 0);
    ASSERT_EQ_STR(z, "a-b-c");
    ASSERT_EQ_INT(string_merge(z, " and ", 5), 0);
    ASSERT_EQ_STR(z, "a and b and c");
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_STR(& z->tokens[0], "a");
    ASSERT_EQ_STR(& z->tokens[1], "b");
    ASSERT_EQ_STR(& z->tokens[2], "c");
    ASSERT_EQ_INT(z->data[z->len], '\0');

    /* the same delimiter is a no-op */
    ASSERT_EQ_INT(string_merge(z, " and ", 5), 0);
    ASSERT_EQ_STR(z, "a and b and c");
    z = string_free(z);

    /* at least two tokens are required */
    ASSERT_NOT_NULL(z = string_alloc("abc", 3));
    ASSERT_EQ_INT(string_merge(z, "-", 1), -1);
    ASSERT_NOT_NULL(string_select(z, 0, 3));
    ASSERT_EQ_INT(string_merge(z, "-", 1), -1);
    z = string_free(z);

    ASSERT_EQ_INT(string_merge(NULL, "-", 1), -1);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _cut(void)
{
    char out[8];
    String *z = NULL;

    /* plain string, with and without copying the removed bytes */
    ASSERT_NOT_NULL(z = string_alloc("hello cruel world", 17));
    ASSERT_EQ_INT(string_cut(z, 5, 6, out), 0);
    ASSERT_EQ_MEM(out, " cruel", 6);
    ASSERT_EQ_STR(z, "hello world");
    ASSERT_EQ_INT(string_cut(z, 5, 1, NULL), 0);
    ASSERT_EQ_STR(z, "helloworld");
    ASSERT_EQ_INT(z->data[10], '\0');

    /* the head and the tail */
    ASSERT_EQ_INT(string_cut(z, 0, 5, NULL), 0);
    ASSERT_EQ_STR(z, "world");
    ASSERT_EQ_INT(string_cut(z, 3, 2, NULL), 0);
    ASSERT_EQ_STR(z, "wor");

    /* out of bound */
    ASSERT_EQ_INT(string_cut(z, 2, 2, NULL), -1);
    ASSERT_EQ_INT(string_cut(z, 3, 1, NULL), -1);
    ASSERT_EQ_INT(string_cut(z, -1, 1, NULL), -1);
    ASSERT_EQ_INT(string_cut(z, 0, 0, NULL), -1);
    ASSERT_EQ_STR(z, "wor");
    ASSERT_EQ_INT(string_cut(NULL, 0, 1, NULL), -1);
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _cut_tokens(void)
{
    String *z = NULL;

    /* removing a whole token drops it and shifts the following ones */
    ASSERT_NOT_NULL(z = string_alloc("aa,bb,cc,dd", 11));
    ASSERT_EQ_INT(string_split(z, ",", 1), 0);
    ASSERT_EQ_UINT(z->count, 4);
    ASSERT_EQ_INT(string_cut(z, 3, 3, NULL), 0);
    ASSERT_EQ_STR(z, "aa,cc,dd");
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_STR(& z->tokens[0], "aa");
    ASSERT_EQ_STR(& z->tokens[1], "cc");
    ASSERT_EQ_STR(& z->tokens[2], "dd");
    ASSERT_EQ_PTR(z->tokens[2].parent, z);

    /* cutting the last token through the parent drops it */
    ASSERT_EQ_INT(string_cut(z, 6, 2, NULL), 0);
    ASSERT_EQ_STR(z, "aa,cc,");
    ASSERT_EQ_UINT(z->count, 2);
    ASSERT_EQ_STR(& z->tokens[1], "cc");
    z = string_free(z);

    /* nested tokens: dropped with their parent, or updated in place */
    ASSERT_NOT_NULL(z = string_alloc("aa,bb-cc,dd", 11));
    ASSERT_EQ_INT(string_split(z, ",", 1), 0);
    ASSERT_EQ_INT(string_split(& z->tokens[1], "-", 1), 0);
    ASSERT_EQ_INT(string_cut(z, 5, 2, NULL), 0);
    ASSERT_EQ_STR(z, "aa,bbc,dd");
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_STR(& z->tokens[1], "bbc");
    ASSERT_EQ_UINT(z->tokens[1].count, 2);
    ASSERT_EQ_STR(& z->tokens(1, 0), "bb");
    ASSERT_EQ_STR(& z->tokens(1, 1), "c");
    ASSERT_EQ_PTR(z->tokens(1, 1).parent, & z->tokens[1]);
    ASSERT_EQ_STR(& z->tokens[2], "dd");
    ASSERT_EQ_INT(string_cut(z, 3, 4, NULL), 0);
    ASSERT_EQ_STR(z, "aa,dd");
    ASSERT_EQ_UINT(z->count, 2);
    ASSERT_EQ_STR(& z->tokens[1], "dd");
    ASSERT_EQ_PTR(z->tokens[1].parent, z);
    z = string_free(z);

    /* flushing a string drops its tokens */
    ASSERT_NOT_NULL(z = string_alloc("aa,bb", 5));
    ASSERT_EQ_INT(string_split(z, ",", 1), 0);
    string_flush(z);
    ASSERT_EQ_UINT(z->len, 0);
    ASSERT_EQ_UINT(z->count, 0);
    ASSERT_NULL(z->tokens);
    z = string_free(z);


    /* a cut straddling two tokens truncates both */
    ASSERT_NOT_NULL(z = string_alloc("aaaa,bbbb", 9));
    ASSERT_EQ_INT(string_split(z, ",", 1), 0);
    ASSERT_EQ_INT(string_cut(z, 2, 5, NULL), 0);
    ASSERT_EQ_STR(z, "aabb");
    ASSERT_EQ_UINT(z->count, 2);
    ASSERT_EQ_STR(& z->tokens[0], "aa");
    ASSERT_EQ_STR(& z->tokens[1], "bb");
    z = string_free(z);

    /* the only token is followed by data: cut through the parent */
    ASSERT_NOT_NULL(z = string_alloc("aa,bb", 5));
    ASSERT_NOT_NULL(string_select(z, 0, 2));
    ASSERT_EQ_INT(string_cut(z, 0, 2, NULL), 0);
    ASSERT_EQ_STR(z, ",bb");
    ASSERT_EQ_UINT(z->count, 0);
    z = string_free(z);

    /* same cut through the token: it survives, emptied */
    ASSERT_NOT_NULL(z = string_alloc("aa,bb", 5));
    ASSERT_NOT_NULL(string_select(z, 0, 2));
    ASSERT_EQ_INT(string_cut(& z->tokens[0], 0, 2, NULL), 0);
    ASSERT_EQ_STR(z, ",bb");
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_EQ_UINT(z->tokens[0].len, 0);
    ASSERT_EQ_PTR(z->tokens[0].data, z->data);
    z = string_free(z);

    ASSERT_NOT_NULL(z = string_alloc("aa,bb,cc", 8));
    ASSERT_EQ_INT(string_split(z, ",", 1), 0);

    /* cutting inside a token shrinks it, the following tokens move */
    ASSERT_EQ_INT(string_cut(& z->tokens[1], 1, 1, NULL), 0);
    ASSERT_EQ_STR(z, "aa,b,cc");
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_STR(& z->tokens[0], "aa");
    ASSERT_EQ_STR(& z->tokens[1], "b");
    ASSERT_EQ_STR(& z->tokens[2], "cc");
    z = string_free(z);
    return 0;
}

/* -------------------------------------------------------------------------- */

static int _replace_all(void)
{
    String *z = NULL;

    ASSERT_NOT_NULL(z = string_alloc("a-b-c", 5));

    /* same length, at both ends and in the middle */
    ASSERT_EQ_PTR(string_replace_all(z, "a", 1, "x", 1), z);
    ASSERT_EQ_PTR(string_replace_all(z, "c", 1, "z", 1), z);
    ASSERT_EQ_STR(z, "x-b-z");
    ASSERT_EQ_PTR(string_replace_all(z, "-", 1, "+", 1), z);
    ASSERT_EQ_STR(z, "x+b+z");
    ASSERT_EQ_INT(z->data[z->len], '\0');

    /* tokens are dropped by the transformation */
    ASSERT_EQ_INT(string_split(z, "+", 1), 0);
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_PTR(string_replace_all(z, "+", 1, "-", 1), z);
    ASSERT_EQ_STR(z, "x-b-z");
    ASSERT_EQ_UINT(z->count, 0);
    ASSERT_EQ_PTR(string_replace_all(z, "-b-", 3, "yby", 3), z);
    ASSERT_EQ_STR(z, "xybyz");

    /* nothing to replace */
    ASSERT_NULL(string_replace_all(z, "q", 1, "r", 1));
    ASSERT_EQ_STR(z, "xybyz");

    /* everything removed */
    ASSERT_EQ_PTR(string_remove_all(z, "xybyz", 5), z);
    ASSERT_EQ_UINT(z->len, 0);
    ASSERT_EQ_INT(z->data[0], '\0');
    z = string_free(z);
    ASSERT_NOT_NULL(z = string_alloc("abab", 4));
    ASSERT_EQ_PTR(string_remove_all(z, "ab", 2), z);
    ASSERT_EQ_UINT(z->len, 0);
    z = string_free(z);
    ASSERT_NOT_NULL(z = string_alloc("xybyz", 5));

    /* bad parameters */
    ASSERT_NULL(string_replace_all(z, NULL, 1, "r", 1));
    ASSERT_NULL(string_replace_all(z, "q", 0, "r", 1));
    ASSERT_NULL(string_replace_all(NULL, "q", 1, "r", 1));
    ASSERT_NULL(string_remove_all(z, NULL, 1));
    z = string_free(z);


    ASSERT_NOT_NULL(z = string_alloc("a-b-c", 5));
    ASSERT_EQ_PTR(string_replace_all(z, "-", 1, "::", 2), z);
    ASSERT_EQ_STR(z, "a::b::c");
    ASSERT_EQ_INT(z->data[z->len], '\0');
    z = string_free(z);

    ASSERT_NOT_NULL(z = string_alloc("a::b::c", 7));
    ASSERT_EQ_PTR(string_replace_all(z, "::", 2, "-", 1), z);
    ASSERT_EQ_STR(z, "a-b-c");
    ASSERT_EQ_INT(z->data[z->len], '\0');
    z = string_free(z);

    ASSERT_NOT_NULL(z = string_alloc("x-b-z", 5));

    /* tokens are dropped by the transformation */
    ASSERT_EQ_INT(string_split(z, "-", 1), 0);
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_PTR(string_remove_all(z, "-", 1), z);
    ASSERT_EQ_STR(z, "xbz");
    ASSERT_EQ_UINT(z->count, 0);
    z = string_free(z);

    ASSERT_NOT_NULL(z = string_alloc("aa,b-c,dd", 9));
    ASSERT_EQ_INT(string_split(z, ",", 1), 0);

    /* the token and its ancestors grow, the following tokens move */
    ASSERT_EQ_PTR(
        string_replace_all(& z->tokens[1], "-", 1, "::", 2), & z->tokens[1]
    );
    ASSERT_EQ_STR(z, "aa,b::c,dd");
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_STR(& z->tokens[0], "aa");
    ASSERT_EQ_STR(& z->tokens[1], "b::c");
    ASSERT_EQ_STR(& z->tokens[2], "dd");
    ASSERT_EQ_INT(z->data[z->len], '\0');

    /* and shrink */
    ASSERT_EQ_PTR(string_remove_all(& z->tokens[1], ":", 1), & z->tokens[1]);
    ASSERT_EQ_STR(z, "aa,bc,dd");
    ASSERT_EQ_STR(& z->tokens[1], "bc");
    ASSERT_EQ_STR(& z->tokens[2], "dd");
    ASSERT_EQ_INT(z->data[z->len], '\0');
    z = string_free(z);
    return 0;
}

/* -------------------------------------------------------------------------- */

static int _tokens(void)
{
    String *z = NULL, *t = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(z = string_alloc("0123456789abcdefghijklmnopqrstuv", 32));

    /* select replaces every token by a single one */
    ASSERT_NOT_NULL(t = string_select(z, 2, 3));
    ASSERT_EQ_PTR(t, & z->tokens[0]);
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_EQ_STR(t, "234");
    ASSERT_EQ_PTR(t->parent, z);
    ASSERT_NOT_NULL(string_select(z, 0, 2));
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_EQ_STR(& z->tokens[0], "01");

    /* add tokens, one with a subtoken */
    ASSERT_NOT_NULL(t = string_add_token(z, 4, 8));
    ASSERT_EQ_STR(t, "4567");
    ASSERT_NOT_NULL(string_add_token(t, 1, 3));
    ASSERT_EQ_STR(& z->tokens(1, 0), "56");
    ASSERT_EQ_PTR(z->tokens(1, 0).parent, & z->tokens[1]);

    /* growing the token array must keep the subtokens' parent pointers */
    for (i = 8; i + 1 < 32; i ++)
        ASSERT_NOT_NULL(string_add_token(z, i, i + 1));
    ASSERT_EQ_UINT(z->count, 25);
    ASSERT_EQ_STR(& z->tokens[24], "u");
    ASSERT_EQ_PTR(z->tokens(1, 0).parent, & z->tokens[1]);
    ASSERT_EQ_STR(& z->tokens(1, 0), "56");

    /* free the tokens */
    string_free_token(z);
    ASSERT_EQ_UINT(z->count, 0);
    ASSERT_NULL(z->tokens);
    ASSERT_EQ_STR(z, "0123456789abcdefghijklmnopqrstuv");
    z = string_free(z);

    ASSERT_NULL(string_select(NULL, 0, 1));
    ASSERT_NULL(string_add_token(NULL, 0, 1));


    /* string_free() on a token releases the whole string */
    ASSERT_NOT_NULL(z = string_alloc("ab-cd", 5));
    ASSERT_EQ_INT(string_split(z, "-", 1), 0);
    ASSERT_NULL(string_free(& z->tokens[1]));
    return 0;
}

/* -------------------------------------------------------------------------- */

static int _push_pop(void)
{
    String *z = NULL, *t = NULL;
    static const char *words[] = { "One", "Two", "Three", "Four", "Five" };
    unsigned int i = 0;

    ASSERT_NOT_NULL(z = string_alloc("One", 3));
    ASSERT_NOT_NULL(string_select(z, 0, 3));

    for (i = 1; i < 5; i ++)
        ASSERT_EQ_INT(string_push_token(z, words[i], strlen(words[i])), 0);

    ASSERT_EQ_UINT(z->count, 5);
    ASSERT_EQ_STR(z, "OneTwoThreeFourFive");
    ASSERT_EQ_STR(& z->tokens[4], "Five");

    /* in place transformation of a token */
    ASSERT_EQ_INT(string_upper(& z->tokens[1]), 0);
    ASSERT_EQ_STR(z, "OneTWOThreeFourFive");

    /* pop from the head */
    ASSERT_NOT_NULL(t = string_pop_token(z));
    ASSERT_EQ_STR(t, "One");
    ASSERT_NULL(t->parent);
    t = string_free(t);
    ASSERT_EQ_STR(z, "TWOThreeFourFive");
    ASSERT_EQ_UINT(z->count, 4);
    ASSERT_EQ_STR(& z->tokens[0], "TWO");

    ASSERT_NOT_NULL(t = string_pop_token(z));
    ASSERT_EQ_STR(t, "TWO");
    t = string_free(t);
    ASSERT_NOT_NULL(t = string_pop_token(z));
    ASSERT_EQ_STR(t, "Three");
    t = string_free(t);
    ASSERT_NOT_NULL(t = string_pop_token(z));
    ASSERT_EQ_STR(t, "Four");
    t = string_free(t);
    ASSERT_NOT_NULL(t = string_pop_token(z));
    ASSERT_EQ_STR(t, "Five");
    t = string_free(t);

    /* drained */
    ASSERT_EQ_UINT(z->len, 0);
    ASSERT_EQ_UINT(z->count, 0);
    ASSERT_NULL(string_pop_token(z));
    z = string_free(z);

    /* push on a string without tokens */
    ASSERT_NOT_NULL(z = string_alloc(NULL, 0));
    ASSERT_EQ_INT(string_push_token(z, "abc", 3), 0);
    ASSERT_EQ_INT(string_push_token(z, "de", 2), 0);
    ASSERT_EQ_STR(z, "abcde");
    ASSERT_EQ_UINT(z->count, 2);
    ASSERT_EQ_STR(& z->tokens[1], "de");
    z = string_free(z);

    ASSERT_EQ_INT(string_push_token(NULL, "abc", 3), -1);
    ASSERT_NULL(string_pop_token(NULL));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _suppr_token(void)
{
    String *z = NULL;

    ASSERT_NOT_NULL(z = string_alloc("aa,bb,cc,dd", 11));
    ASSERT_EQ_INT(string_split(z, ",", 1), 0);

    /* the last token, aligned with the end of the string */
    string_suppr_token(z, 3);
    ASSERT_EQ_STR(z, "aa,bb,cc,");
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_STR(& z->tokens[2], "cc");
    ASSERT_EQ_STR(& z->tokens[0], "aa");

    /* out of range */
    ASSERT_NULL(string_suppr_token(z, 3));
    ASSERT_NULL(string_suppr_token(NULL, 0));
    z = string_free(z);


    ASSERT_NOT_NULL(z = string_alloc("aa,bb,cc,dd", 11));
    ASSERT_EQ_INT(string_split(z, ",", 1), 0);

    /* a token in the middle */
    string_suppr_token(z, 1);
    ASSERT_EQ_STR(z, "aa,,cc,dd");
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_STR(& z->tokens[0], "aa");
    ASSERT_EQ_STR(& z->tokens[1], "cc");
    ASSERT_EQ_STR(& z->tokens[2], "dd");

    /* the first token */
    string_suppr_token(z, 0);
    ASSERT_EQ_STR(z, ",,cc,dd");
    ASSERT_EQ_UINT(z->count, 2);
    ASSERT_EQ_STR(& z->tokens[0], "cc");
    ASSERT_EQ_STR(& z->tokens[1], "dd");
    z = string_free(z);
    return 0;
}

/* -------------------------------------------------------------------------- */

static int _request_flow(void)
{
    /* the pattern used by the server: split a request, consume it, append */
    static const char request[] =
        "HTTP 888 TEST\r\nheader: value\r\nheader: value----1";
    String *z = NULL;

    ASSERT_NOT_NULL(z = string_alloc(request, sizeof(request) - 1));
    ASSERT_EQ_INT(string_split(z, "----", 4), 0);
    ASSERT_EQ_UINT(z->count, 2);
    ASSERT_EQ_STR(& z->tokens[1], "1");

    ASSERT_EQ_INT(string_split(& z->tokens[0], "\r\n", 2), 0);
    ASSERT_EQ_UINT(z->tokens[0].count, 3);
    ASSERT_EQ_STR(& z->tokens(0, 0), "HTTP 888 TEST");
    ASSERT_EQ_STR(& z->tokens(0, 2), "header: value");

    ASSERT_EQ_INT(string_merge(& z->tokens[0], "X", 1), 0);
    ASSERT_EQ_STR(& z->tokens[0], "HTTP 888 TESTXheader: valueXheader: value");
    ASSERT_EQ_STR(z, "HTTP 888 TESTXheader: valueXheader: value----1");
    ASSERT_EQ_STR(& z->tokens[1], "1");

    /* flush the trailing token, then append new data to it */
    string_flush(& z->tokens[1]);
    ASSERT_EQ_UINT(z->tokens[1].len, 0);
    ASSERT_EQ_STR(z, "HTTP 888 TESTXheader: valueXheader: value----");

    ASSERT_NOT_NULL(string_append_buffer(z, "incoming data", 13));
    ASSERT_EQ_STR(
        z, "HTTP 888 TESTXheader: valueXheader: value----incoming data"
    );
    ASSERT_EQ_UINT(z->count, 2);
    ASSERT_EQ_STR(& z->tokens[1], "incoming data");

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */
#ifdef HAS_PCRE
/* -------------------------------------------------------------------------- */

static int _parse_regex(void)
{
    String *a = NULL;

    ASSERT_NOT_NULL(a = string_alloc("Random string1234", 17));

    /* the match is a token, the captures are its subtokens */
    ASSERT_EQ_INT(string_parse(a, "Random string(.*)", 17), 0);
    ASSERT_EQ_UINT(a->count, 1);
    ASSERT_EQ_STR(& a->tokens[0], "Random string1234");
    ASSERT_EQ_UINT(a->tokens[0].count, 1);
    ASSERT_EQ_STR(& a->tokens(0, 0), "1234");
    ASSERT_EQ_PTR(a->tokens(0, 0).parent, & a->tokens[0]);

    ASSERT_EQ_INT(string_parse(a, "Random string(.)", 16), 0);
    ASSERT_EQ_UINT(a->count, 1);
    ASSERT_EQ_STR(& a->tokens[0], "Random string1");
    ASSERT_EQ_STR(& a->tokens(0, 0), "1");

    ASSERT_EQ_INT(string_parse(a, "(Random)", 8), 0);
    ASSERT_EQ_UINT(a->count, 1);
    ASSERT_EQ_STR(& a->tokens[0], "Random");
    ASSERT_EQ_STR(& a->tokens(0, 0), "Random");

    /* every match is a token */
    ASSERT_EQ_INT(string_parse(a, "[0-9]", 5), 0);
    ASSERT_EQ_UINT(a->count, 4);
    ASSERT_EQ_STR(& a->tokens[0], "1");
    ASSERT_EQ_STR(& a->tokens[3], "4");
    ASSERT_EQ_UINT(a->tokens[3].count, 0);

    /* no match, bad pattern */
    ASSERT_EQ_INT(string_parse(a, "test_no_match", 13), -1);
    ASSERT_EQ_INT(string_parse(a, "(unbalanced", 11), -1);
    ASSERT_EQ_INT(string_parse(a, NULL, 1), -1);
    ASSERT_EQ_INT(string_parse(NULL, "a", 1), -1);

    a = string_free(a);

    return 0;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

static int _binary_append(void)
{
    char buffer[256];
    uint32_t i = htonl(0x82);
    size_t item_size = 0;
    String *z = NULL;

    /* a string can hold arbitrary binary data */
    ASSERT_NOT_NULL(z = string_alloc((const char *) & i, sizeof(i)));
    ASSERT_EQ_UINT(z->len, 4);
    ASSERT_EQ_MEM(z->data, & i, 4);

    item_size = snprintf(
        buffer, sizeof(buffer), "%s|%d|%.3f\n", "nickname_test_1234", 350, 1.5
    );
    ASSERT_TRUE(item_size > 0);

    ASSERT_EQ_PTR(string_append_buffer(z, buffer, item_size), z);
    ASSERT_EQ_UINT(z->len, 4 + item_size);
    ASSERT_EQ_PTR(
        string_append_buffer(z, (char *) & item_size, sizeof(item_size)), z
    );
    ASSERT_EQ_UINT(z->len, 4 + item_size + sizeof(item_size));
    ASSERT_TRUE(string_capacity(z) >= z->len);
    ASSERT_EQ_MEM(z->data, & i, 4);
    ASSERT_EQ_MEM(z->data + 4, buffer, item_size);
    ASSERT_EQ_MEM(z->data + 4 + item_size, & item_size, sizeof(item_size));

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _replace_with_self(void)
{
    /* the replacement may point into the string being edited: it is
       read before the string moves or grows (D61) */

    String *z = NULL;

    ASSERT_NOT_NULL(z = string_alloc("a-b-c", 5));
    ASSERT_EQ_PTR(string_replace_all(z, "-", 1, z->data, 3), z);
    ASSERT_EQ_STR(z, "aa-bba-bc");
    z = string_free(z);

    ASSERT_NOT_NULL(z = string_alloc("a--b--c", 7));
    ASSERT_EQ_PTR(string_replace_all(z, "--", 2, z->data + 6, 1), z);
    ASSERT_EQ_STR(z, "acbcc");
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("alloc_free", _alloc_free),
    TEST("resize", _resize),
    TEST("encaps", _encaps),
    TEST("static_initializer", _static_initializer),
    TEST("fetch", _fetch),
    TEST("clone", _clone),
    TEST("find", _find),
    TEST("find_pattern", _find_pattern),
    TEST("wchar_mbyte", _wchar_mbyte),
    #ifdef HAS_ICONV
    TEST("conv", _conv),
    #endif
    TEST("append_prepend", _append_prepend),
    TEST("compare", _compare),
    TEST("upper_lower", _upper_lower),
    TEST("split_merge", _split_merge),
    TEST("split_edge", _split_edge),
    TEST("split_many", _split_many),
    TEST("merge_edge", _merge_edge),
    TEST("cut", _cut),
    TEST("cut_tokens", _cut_tokens),
    TEST("replace_all", _replace_all),
    TEST("replace_with_self", _replace_with_self),
    TEST("tokens", _tokens),
    TEST("push_pop", _push_pop),
    TEST("suppr_token", _suppr_token),
    TEST("request_flow", _request_flow),
    #ifdef HAS_PCRE
    TEST("parse_regex", _parse_regex),
    #endif
    TEST("binary_append", _binary_append)
};

TEST_SUITE(test_suite_string, "string", _setup, _teardown, _cases);

/* -------------------------------------------------------------------------- */
