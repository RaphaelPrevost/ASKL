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

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_JSON
/* -------------------------------------------------------------------------- */

#include "../askl_test.h"
#include "../../lib/askl_json.h"
#include <unistd.h>

/* shorthand for the n-th token of a String */
#define T(s, n) (& (s)->tokens[n])

/* -------------------------------------------------------------------------- */

static String *_parse(const char *doc, char strict)
{
    /** @brief tokenize a document, NULL if it was rejected */

    String *z = NULL;

    if (! (z = string_alloc(doc, strlen(doc))) ) return NULL;

    if (string_parse_json(z, strict, NULL) == -1) return string_free(z);

    return z;
}

/* -------------------------------------------------------------------------- */

static int _strict_tokenizes_document(void)
{
    static const char doc[] =
        "{\"obj\":{\"b\":false,\"z\":[\"\"]},\"unicode\":\"\\u611b\","
        "\"matrix\":[[[1,2],[2,3]]],\"empty\":{}}";
    String *z = NULL, *root = NULL, *obj = NULL, *m = NULL;

    ASSERT_NOT_NULL(z = string_alloc(doc, sizeof(doc) - 1));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);

    /* one root value, an object whose members alternate keys and values */
    ASSERT_EQ_UINT(z->count, 1);
    root = T(z, 0);
    ASSERT_TRUE(IS_OBJECT(root));
    ASSERT_FALSE(HAS_ERROR(root));
    ASSERT_EQ_STR(root, doc);
    ASSERT_EQ_UINT(root->count, 8);

    ASSERT_TRUE(IS_STRING(T(root, 0)));
    ASSERT_EQ_STR(T(root, 0), "obj");
    obj = T(root, 1);
    ASSERT_TRUE(IS_OBJECT(obj));
    ASSERT_EQ_UINT(obj->count, 4);
    ASSERT_EQ_STR(T(obj, 0), "b");
    ASSERT_TRUE(IS_PRIMITIVE(T(obj, 1)));
    ASSERT_EQ_STR(T(obj, 1), "false");
    ASSERT_EQ_STR(T(obj, 2), "z");
    ASSERT_TRUE(IS_ARRAY(T(obj, 3)));
    ASSERT_EQ_UINT(T(obj, 3)->count, 1);
    ASSERT_TRUE(IS_STRING(& obj->tokens(3, 0)));
    ASSERT_EQ_UINT(obj->tokens(3, 0).len, 0);

    /* string tokens are views on the raw, still escaped, data */
    ASSERT_EQ_STR(T(root, 2), "unicode");
    ASSERT_TRUE(IS_STRING(T(root, 3)));
    ASSERT_EQ_STR(T(root, 3), "\\u611b");

    /* nested arrays */
    ASSERT_EQ_STR(T(root, 4), "matrix");
    m = T(root, 5);
    ASSERT_TRUE(IS_ARRAY(m));
    ASSERT_EQ_STR(m, "[[[1,2],[2,3]]]");
    ASSERT_EQ_UINT(m->count, 1);
    ASSERT_TRUE(IS_ARRAY(T(m, 0)));
    ASSERT_EQ_UINT(T(m, 0)->count, 2);
    ASSERT_EQ_STR(& m->tokens(0, 0), "[1,2]");
    ASSERT_EQ_STR(& m->tokens(0, 1), "[2,3]");
    ASSERT_EQ_UINT(m->tokens(0, 1).count, 2);
    ASSERT_TRUE(IS_PRIMITIVE(& m->tokens(0, 1, 1)));
    ASSERT_EQ_STR(& m->tokens(0, 1, 1), "3");
    ASSERT_EQ_PTR(m->tokens(0, 1, 1).parent, & m->tokens(0, 1));

    /* empty object */
    ASSERT_EQ_STR(T(root, 6), "empty");
    ASSERT_TRUE(IS_OBJECT(T(root, 7)));
    ASSERT_EQ_UINT(T(root, 7)->count, 0);

    z = string_free(z);

    /* a UTF-8 byte order mark is skipped */
    ASSERT_NOT_NULL(z = _parse("\xef\xbb\xbf{\"bom\":1}", JSON_STRICT));
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_TRUE(IS_OBJECT(T(z, 0)));
    ASSERT_FALSE(HAS_ERROR(T(z, 0)));
    ASSERT_EQ_STR(& z->tokens(0, 0), "bom");
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _top_level_number_flagged(void)
{
    String *z = NULL;

    /* a number ending the input may continue in the next chunk: it is
       tokenized but flagged, unlike a number followed by a delimiter */
    ASSERT_NOT_NULL(z = _parse("1", JSON_STRICT));
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_TRUE(IS_PRIMITIVE(T(z, 0)));
    ASSERT_TRUE(HAS_ERROR(T(z, 0)));
    z = string_free(z);

    ASSERT_NOT_NULL(z = _parse("-0.5e+10 ", JSON_STRICT));
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_FALSE(HAS_ERROR(T(z, 0)));
    ASSERT_EQ_STR(T(z, 0), "-0.5e+10");
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _top_level_number_resumed(void)
{
    String *z = NULL;

    /* the rest of a top-level number arrives in the next chunk */
    ASSERT_NOT_NULL(z = _parse("1", JSON_STRICT));
    ASSERT_TRUE(HAS_ERROR(T(z, 0)));
    ASSERT_NOT_NULL(string_append_buffer(z, "2 ", 2));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_FALSE(HAS_ERROR(T(z, 0)));
    ASSERT_EQ_STR(T(z, 0), "12");
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _incomplete_flagged_and_resumed(void)
{
    static const char part1[] = "{\"0\": {}, \"a\":[1,2,3],\"b\":[4,5";
    static const char part2[] = ",6],\"c\":[7,8,9]}";
    String *z = NULL, *root = NULL;

    ASSERT_NOT_NULL(z = string_alloc(part1, sizeof(part1) - 1));

    /* a document cut short is tokenized as far as possible and flagged */
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_UINT(z->count, 1);
    root = T(z, 0);
    ASSERT_TRUE(IS_OBJECT(root));
    ASSERT_TRUE(HAS_ERROR(root));
    ASSERT_EQ_UINT(root->count, 6);
    ASSERT_EQ_STR(T(root, 0), "0");
    ASSERT_TRUE(IS_OBJECT(T(root, 1)));
    ASSERT_FALSE(HAS_ERROR(T(root, 1)));
    ASSERT_EQ_STR(T(root, 3), "[1,2,3]");
    ASSERT_FALSE(HAS_ERROR(T(root, 3)));
    ASSERT_TRUE(IS_ARRAY(T(root, 5)));
    ASSERT_TRUE(HAS_ERROR(T(root, 5)));
    ASSERT_EQ_UINT(T(root, 5)->count, 2);
    ASSERT_FALSE(HAS_ERROR(& root->tokens(5, 0)));
    /* the number may continue in the next chunk */
    ASSERT_TRUE(HAS_ERROR(& root->tokens(5, 1)));

    /* discard the complete values, keep the incomplete tail */
    ASSERT_EQ_INT(
        string_cut(z, 0, last_token(last_token(z))->data - z->data, NULL), 0
    );
    ASSERT_EQ_STR(z, "[4,5");

    /* append the rest and resume */
    ASSERT_NOT_NULL(string_append_buffer(z, part2, sizeof(part2) - 1));
    ASSERT_EQ_STR(z, "[4,5,6],\"c\":[7,8,9]}");
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);

    ASSERT_EQ_UINT(z->count, 1);
    root = T(z, 0);
    ASSERT_TRUE(IS_OBJECT(root));
    ASSERT_FALSE(HAS_ERROR(root));
    ASSERT_EQ_UINT(root->count, 3);
    ASSERT_TRUE(IS_ARRAY(T(root, 0)));
    ASSERT_EQ_STR(T(root, 0), "[4,5,6]");
    ASSERT_EQ_UINT(T(root, 0)->count, 3);
    ASSERT_EQ_STR(& root->tokens(0, 2), "6");
    ASSERT_EQ_STR(T(root, 1), "c");
    ASSERT_EQ_STR(T(root, 2), "[7,8,9]");
    ASSERT_EQ_UINT(T(root, 2)->count, 3);
    ASSERT_FALSE(HAS_ERROR(& root->tokens(2, 2)));

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _stream_nested_resumed(void)
{
    static const char part1[] = "{\"a\": \"stream_part\"}[[[[{\"b\"";
    static const char part2[] = ": \"partial stream part\"}]]]]";
    String *z = NULL, *t = NULL;
    unsigned int depth = 0;

    ASSERT_NOT_NULL(z = string_alloc(part1, sizeof(part1) - 1));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);

    /* first document complete, second one flagged down to the object */
    ASSERT_EQ_UINT(z->count, 2);
    ASSERT_TRUE(IS_OBJECT(T(z, 0)));
    ASSERT_FALSE(HAS_ERROR(T(z, 0)));
    ASSERT_EQ_STR(& z->tokens(0, 1), "stream_part");
    ASSERT_TRUE(IS_ARRAY(T(z, 1)));
    ASSERT_TRUE(HAS_ERROR(T(z, 1)));
    for (t = T(z, 1), depth = 0; IS_ARRAY(t); t = T(t, 0), depth ++) {
        ASSERT_TRUE(HAS_ERROR(t));
        ASSERT_EQ_UINT(t->count, 1);
    }
    ASSERT_EQ_UINT(depth, 4);
    ASSERT_TRUE(IS_OBJECT(t));
    ASSERT_TRUE(HAS_ERROR(t));
    ASSERT_EQ_UINT(t->count, 1);
    ASSERT_EQ_STR(T(t, 0), "b");

    /* resume with the rest of the stream */
    ASSERT_NOT_NULL(string_append_buffer(z, part2, sizeof(part2) - 1));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_UINT(z->count, 2);
    ASSERT_FALSE(HAS_ERROR(T(z, 0)));
    ASSERT_FALSE(HAS_ERROR(T(z, 1)));
    ASSERT_EQ_STR(T(z, 1), "[[[[{\"b\": \"partial stream part\"}]]]]");
    for (t = T(z, 1), depth = 0; IS_ARRAY(t); t = T(t, 0), depth ++)
        ASSERT_FALSE(HAS_ERROR(t));
    ASSERT_EQ_UINT(depth, 4);
    ASSERT_TRUE(IS_OBJECT(t));
    ASSERT_EQ_UINT(t->count, 2);
    ASSERT_EQ_STR(T(t, 1), "partial stream part");

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _stream_concatenated_documents(void)
{
    static const char doc1[] = "{\"a\": [1, true] }";
    static const char doc2[] = "{\"b\": [0, false] }";
    String *z = NULL;

    ASSERT_NOT_NULL(z = string_alloc(doc1, sizeof(doc1) - 1));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_FALSE(HAS_ERROR(T(z, 0)));
    ASSERT_EQ_STR(& z->tokens(0, 1, 1), "true");

    /* a second document appended to the buffer is a second root token */
    ASSERT_NOT_NULL(string_append_buffer(z, doc2, sizeof(doc2) - 1));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_UINT(z->count, 2);
    ASSERT_TRUE(IS_OBJECT(T(z, 0)));
    ASSERT_TRUE(IS_OBJECT(T(z, 1)));
    ASSERT_FALSE(HAS_ERROR(T(z, 0)));
    ASSERT_FALSE(HAS_ERROR(T(z, 1)));
    ASSERT_EQ_STR(T(z, 0), doc1);
    ASSERT_EQ_STR(T(z, 1), doc2);
    ASSERT_EQ_STR(& z->tokens(1, 0), "b");
    ASSERT_EQ_STR(& z->tokens(1, 1, 1), "false");

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _incomplete_string_resumed(void)
{
    String *z = NULL;

    ASSERT_NOT_NULL(z = string_alloc("\"incomplete, ", 13));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_TRUE(IS_STRING(T(z, 0)));
    ASSERT_TRUE(HAS_ERROR(T(z, 0)));
    ASSERT_EQ_STR(T(z, 0), "incomplete, ");

    ASSERT_NOT_NULL(string_append_buffer(z, "string\"", 7));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_TRUE(IS_STRING(T(z, 0)));
    ASSERT_FALSE(HAS_ERROR(T(z, 0)));
    ASSERT_EQ_STR(T(z, 0), "incomplete, string");

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _quirks_extensions(void)
{
    static const char doc[] =
        "{ // single-line comment\n"
        "unquoted_key: 'single quoted string w/ nested \"double quoted "
        "string\"',\n"
        "'single quoted key'/* multi-line\ncomment */: \"stray CRLF\\\n"
        "\tunescaped tab\","
        "\"hex\": [0xA, 0xBAD, 0xc0ffee, 0x00000000064B175]/* max 64 bits */,\n"
        "\"no fractional part\": 123.,\n"
        ", \"trailing commas in object\":\n"
        "[\"and\",,\"in array\", ],\n"
        "}";
    String *z = NULL, *root = NULL;

    /* rejected in strict mode */
    ASSERT_NULL(_parse(doc, JSON_STRICT));

    ASSERT_NOT_NULL(z = _parse(doc, JSON_QUIRKS));
    ASSERT_EQ_UINT(z->count, 1);
    root = T(z, 0);
    ASSERT_TRUE(IS_OBJECT(root));
    ASSERT_FALSE(HAS_ERROR(root));
    ASSERT_EQ_UINT(root->count, 10);

    ASSERT_EQ_STR(T(root, 0), "unquoted_key");
    ASSERT_TRUE(IS_STRING(T(root, 1)));
    ASSERT_EQ_STR(
        T(root, 1), "single quoted string w/ nested \"double quoted string\""
    );
    ASSERT_EQ_STR(T(root, 2), "single quoted key");
    ASSERT_EQ_STR(T(root, 3), "stray CRLF\\\n\tunescaped tab");
    ASSERT_EQ_STR(T(root, 4), "hex");
    ASSERT_TRUE(IS_ARRAY(T(root, 5)));
    ASSERT_EQ_UINT(T(root, 5)->count, 4);
    ASSERT_TRUE(IS_PRIMITIVE(& root->tokens(5, 3)));
    ASSERT_EQ_STR(& root->tokens(5, 3), "0x00000000064B175");
    ASSERT_EQ_STR(T(root, 6), "no fractional part");
    ASSERT_TRUE(IS_PRIMITIVE(T(root, 7)));
    ASSERT_EQ_STR(T(root, 7), "123.");
    ASSERT_EQ_STR(T(root, 8), "trailing commas in object");
    ASSERT_TRUE(IS_ARRAY(T(root, 9)));
    ASSERT_EQ_UINT(T(root, 9)->count, 2);
    ASSERT_EQ_STR(& root->tokens(9, 0), "and");
    ASSERT_EQ_STR(& root->tokens(9, 1), "in array");

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _nest(unsigned int levels)
{
    String *z = NULL, *t = NULL;
    unsigned int i = 0, depth = 0;

    /* the tokenizer is iterative: hostile nesting must not overflow
       the stack, and the token tree must be freed without recursion */
    ASSERT_NOT_NULL(z = string_alloc(NULL, levels));
    memset(z->data, '[', levels);
    ASSERT_NE_INT(string_parse_json(z, JSON_STRICT, NULL), -1);
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_TRUE(HAS_ERROR(T(z, 0)));
    z = string_free(z);

    /* balanced */
    ASSERT_NOT_NULL(z = string_alloc(NULL, 2 * levels));
    memset(z->data, '[', levels);
    memset(z->data + levels, ']', levels);
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_FALSE(HAS_ERROR(T(z, 0)));
    for (t = T(z, 0), depth = 1; t->count; t = T(t, 0), depth ++) {
        if (t->count != 1 || ! IS_ARRAY(t)) {
            test_fail(__FILE__, __LINE__, "bad node at depth %u", depth);
            break;
        }
    }
    ASSERT_EQ_UINT(depth, levels);
    for (i = 0; i < 10; i ++) {
        ASSERT_TRUE(IS_ARRAY(t));
        t = t->parent;
    }
    z = string_free(z);

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _deep_nesting(void)
{
    return _nest(10000);
}

/* -------------------------------------------------------------------------- */

static int _hostile_nesting(void)
{
    /* where RapidJSON segfaults; freeing the tree is the weak point */
    return _nest(100000);
}

/* -------------------------------------------------------------------------- */

static int _many_siblings(void)
{
    String *z = NULL, *t = NULL;
    unsigned int i = 0, n = 10000;
    char *doc = NULL;

    /* an array of 10000 numbers */
    ASSERT_NOT_NULL(doc = malloc(2 * n + 2));
    doc[0] = '[';
    for (i = 0; i < n; i ++) {
        doc[1 + 2 * i] = '1';
        doc[2 + 2 * i] = ',';
    }
    doc[2 * n] = ']';
    z = string_alloc(doc, 2 * n + 1);
    free(doc);
    ASSERT_NOT_NULL(z);

    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_UINT(z->count, 1);
    t = T(z, 0);
    ASSERT_TRUE(IS_ARRAY(t));
    ASSERT_FALSE(HAS_ERROR(t));
    ASSERT_EQ_UINT(t->count, n);
    ASSERT_EQ_STR(T(t, n - 1), "1");
    ASSERT_EQ_PTR(T(t, n - 1)->parent, t);

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _reparse_replaces_tokens(void)
{
    String *z = NULL;

    /* parsing a complete document twice gives the same tree */
    ASSERT_NOT_NULL(z = _parse("[1,[2,3]]", JSON_STRICT));
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_EQ_UINT(T(z, 0)->count, 2);
    ASSERT_EQ_STR(& z->tokens(0, 1, 1), "3");

    /* tokens from a previous split are discarded */
    ASSERT_EQ_INT(string_split(z, ",", 1), 0);
    ASSERT_EQ_UINT(z->count, 3);
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_TRUE(IS_ARRAY(T(z, 0)));
    z = string_free(z);

    ASSERT_EQ_INT(string_parse_json(NULL, JSON_STRICT, NULL), -1);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _resume(const char *part1, const char *part2, char strict)
{
    /** @brief tokenize part1, append part2, resume: 0 if complete */

    String *z = NULL;
    int ret = 0;

    if (! (z = string_alloc(part1, strlen(part1))) ) return -1;
    if (string_parse_json(z, strict, NULL) == -1)
        return string_free(z), -1;
    if (! string_append_buffer(z, part2, strlen(part2)))
        return string_free(z), -1;
    ret = string_parse_json(z, strict, NULL);
    if (ret == 0 && (z->count != 1 || HAS_ERROR(T(z, 0)))) ret = -2;
    string_free(z);

    return ret;
}

/* -------------------------------------------------------------------------- */

static int _resume_at_boundaries(void)
{
    /* the input ends right after an opening bracket, a key, a colon or
       a comma: resuming there is where the bookkeeping is most delicate */
    static const char *splits[][2] = {
        { "[", "1]" },
        { "{", "\"a\":1}" },
        { "{\"a\"", ":1}" },
        { "{\"a\":", "1}" },
        { "{\"a\":1,", "\"b\":2}" },
        { "{\"a\":[", "1]}" },
        { "{\"a\":{", "\"b\":1}}" },
        { "[[", "1],[2]]" },
        { "[1,", "2]" },
        { "[1,[2,", "3]]" },
        { "{\"a\":1}", "" },
        { "[\"x\",", "\"y\"]" },
        { "{\"a\":\"b\",\"c\":", "\"d\"}" }
    };
    unsigned int i = 0;
    int ret = 0;

    for (i = 0; i < sizeof(splits) / sizeof(*splits); i ++) {
        if ( (ret = _resume(splits[i][0], splits[i][1], JSON_STRICT)) ) {
            test_fail(
                __FILE__, __LINE__, "%s | %s: %s", splits[i][0], splits[i][1],
                (ret == -2) ? "incomplete" : "rejected"
            );
        }
    }

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _quirks_incomplete_primitives(void)
{
    String *z = NULL;

    /* an unquoted key, a trailing decimal point and a hexadecimal number
       cut by the end of the input are flagged and resumed */
    ASSERT_NOT_NULL(z = _parse("{abc", JSON_QUIRKS));
    ASSERT_TRUE(HAS_ERROR(T(z, 0)));
    ASSERT_NOT_NULL(string_append_buffer(z, "def:1}", 6));
    ASSERT_EQ_INT(string_parse_json(z, JSON_QUIRKS, NULL), 0);
    ASSERT_FALSE(HAS_ERROR(T(z, 0)));
    ASSERT_EQ_STR(& z->tokens(0, 0), "abcdef");
    ASSERT_EQ_STR(& z->tokens(0, 1), "1");
    z = string_free(z);

    /* an unquoted identifier is not a value */
    ASSERT_NULL(_parse("[abc]", JSON_QUIRKS));

    ASSERT_NOT_NULL(z = _parse("[123.", JSON_QUIRKS));
    ASSERT_TRUE(HAS_ERROR(T(z, 0)));
    ASSERT_NOT_NULL(string_append_buffer(z, "5]", 2));
    ASSERT_EQ_INT(string_parse_json(z, JSON_QUIRKS, NULL), 0);
    ASSERT_EQ_STR(& z->tokens(0, 0), "123.5");
    z = string_free(z);

    ASSERT_NOT_NULL(z = _parse("[0x1A", JSON_QUIRKS));
    ASSERT_TRUE(HAS_ERROR(T(z, 0)));
    ASSERT_NOT_NULL(string_append_buffer(z, "F]", 2));
    ASSERT_EQ_INT(string_parse_json(z, JSON_QUIRKS, NULL), 0);
    ASSERT_TRUE(IS_PRIMITIVE(& z->tokens(0, 0)));
    ASSERT_EQ_STR(& z->tokens(0, 0), "0x1AF");
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _string_edges(void)
{
    String *z = NULL;

    /* strings are scanned four bytes at a time: a raw control character
       inside a long string, and the input ending inside one */
    ASSERT_NULL(_parse("[\"abcdefgh\tij\"]", JSON_STRICT));
    ASSERT_NULL(_parse("[\"\x01" "abcdefgh\"]", JSON_STRICT));
    ASSERT_NULL(_parse("[\"abcdefghijklmnop\x1f\"]", JSON_STRICT));
    ASSERT_NULL(_parse("[\"abc\"\"]", JSON_STRICT));

    ASSERT_NOT_NULL(z = _parse("[\"abcdefghijkl", JSON_STRICT));
    ASSERT_TRUE(HAS_ERROR(T(z, 0)));
    ASSERT_NOT_NULL(string_append_buffer(z, "mnop\"]", 6));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_FALSE(HAS_ERROR(T(z, 0)));
    ASSERT_EQ_STR(& z->tokens(0, 0), "abcdefghijklmnop");
    z = string_free(z);

    ASSERT_NOT_NULL(z = _parse("[\"abcdefghijklmno", JSON_STRICT));
    ASSERT_NOT_NULL(string_append_buffer(z, "\"]", 2));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_STR(& z->tokens(0, 0), "abcdefghijklmno");
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _number_edges(void)
{
    String *z = NULL;

    /* an exponent without digits */
    ASSERT_NULL(_parse("[1e]", JSON_STRICT));
    ASSERT_NULL(_parse("[1e+]", JSON_STRICT));
    ASSERT_NULL(_parse("[1e-]", JSON_STRICT));
    ASSERT_NULL(_parse("[1E]", JSON_STRICT));
    ASSERT_NULL(_parse("[-]", JSON_STRICT));
    ASSERT_NULL(_parse("[-e5]", JSON_STRICT));

    /* cut inside the exponent, then completed */
    ASSERT_NOT_NULL(z = _parse("[1e", JSON_STRICT));
    ASSERT_TRUE(HAS_ERROR(T(z, 0)));
    ASSERT_NOT_NULL(string_append_buffer(z, "+5]", 3));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 0);
    ASSERT_EQ_STR(& z->tokens(0, 0), "1e+5");
    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _token_limit(void)
{
    /* a String holds at most 65535 tokens: the tokenizer returns 1 and
       flags the String when the limit is reached; carrying on is the
       parser's business, see parser_token_limit */
    const unsigned int n = 70000;
    char *doc = NULL;
    String *z = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(doc = malloc(2 * n + 2));
    doc[0] = '[';
    for (i = 0; i < n; i ++) {
        doc[1 + 2 * i] = '1';
        doc[2 + 2 * i] = ',';
    }
    doc[2 * n] = ']';
    z = string_alloc(doc, 2 * n + 1);
    free(doc);
    ASSERT_NOT_NULL(z);

    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, NULL), 1);
    ASSERT_TRUE(IS_BUFFER(z));
    ASSERT_EQ_UINT(z->count, 1);
    ASSERT_EQ_UINT(T(z, 0)->count, 65535);

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */
#if defined(_ENABLE_JSON) && defined(_ENABLE_TRIE)
/* -------------------------------------------------------------------------- */

static int _parse_indexed(const char *doc, char strict)
{
    /** @brief parse a document through the indexing parser context */

    JSON_Parser ctx;
    String *z = NULL;
    int ret = 0;

    if (jsonpath_init(& ctx) == -1) return -1;

    if (! (z = string_alloc(doc, strlen(doc))) ) {
        jsonpath_free(& ctx);
        return -1;
    }

    ret = string_parse_json(z, strict, & ctx);

    string_free(z);
    jsonpath_free(& ctx);

    return ret;
}

/* -------------------------------------------------------------------------- */

static int _parser_validates_strings(void)
{
    /* the tokenizer only delimits strings; UTF-8 and escape sequences are
       validated by the parser, when a context is given */
    static const char *invalid[] = {
        "[\"\xff\"]",                 /* stray continuation byte */
        "[\"\xc0\x80\"]",             /* overlong NUL */
        "[\"\xed\xa0\x80\"]",         /* UTF-8 encoded surrogate */
        "[\"\xf4\x90\x80\x80\"]",     /* above U+10FFFF */
        "[\"\xe4\xbd\"]",             /* truncated sequence */
        "[\"\\ud800\"]",              /* lone high surrogate */
        "[\"\\udc00\"]",              /* lone low surrogate */
        "[\"\\ud83d\\u0041\"]"        /* high surrogate, no low one */
    };
    static const char *valid[] = {
        "[\"\xe4\xbd\xa0\xe5\xa5\xbd\"]",
        "[\"\\ud83d\\ude00\"]",
        "[\"\\u00e9\\u0000\"]",
        "{\"\xf0\x9f\x98\x80\": \"\\\"\\\\\\/\\b\\f\\n\\r\\t\"}"
    };
    unsigned int i = 0;

    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i ++)
        if (_parse_indexed(invalid[i], JSON_STRICT) != -1)
            test_fail(__FILE__, __LINE__, "accepted: %s", invalid[i]);

    for (i = 0; i < sizeof(valid) / sizeof(*valid); i ++)
        if (_parse_indexed(valid[i], JSON_STRICT) == -1)
            test_fail(__FILE__, __LINE__, "rejected: %s", valid[i]);

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _parser_scalars_and_failed_keys(void)
{
    /* a top-level scalar has no place in the index and a failing key
       aborts the document: neither may leak the decoded value */
    ASSERT_EQ_INT(_parse_indexed("\"asd\"", JSON_STRICT), 0);
    ASSERT_EQ_INT(_parse_indexed("\"\"", JSON_STRICT), 0);
    ASSERT_EQ_INT(_parse_indexed("42 ", JSON_STRICT), 0);
    ASSERT_EQ_INT(_parse_indexed("{\"\xb9\":\"0\"}", JSON_STRICT), -1);
    ASSERT_EQ_INT(_parse_indexed("{\"a\":{\"\xb9\":\"0\"}}", JSON_STRICT), -1);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _parser_wide_and_deep(void)
{
    /* index batches grow, array indices reach three digits, nested
       batches queue up */
    char doc[1024];
    unsigned int i = 0, len = 0;

    len = snprintf(doc, sizeof(doc), "[");
    for (i = 0; i < 150; i ++)
        len += snprintf(doc + len, sizeof(doc) - len, "%s%u", i ? "," : "", i);
    snprintf(doc + len, sizeof(doc) - len, "]");
    ASSERT_EQ_INT(_parse_indexed(doc, JSON_STRICT), 0);

    ASSERT_EQ_INT(_parse_indexed("[[[1]],[[2]],[[3]]]", JSON_STRICT), 0);
    ASSERT_EQ_INT(
        _parse_indexed(
            "{\"a\":{\"b\":{\"c\":1}},\"d\":{\"e\":{\"f\":2}}}", JSON_STRICT
        ),
        0
    );
    ASSERT_EQ_INT(
        _parse_indexed(
            "[{\"a\":[1,2,{\"b\":[3]}]},{\"c\":{}},[],[[]]]", JSON_STRICT
        ),
        0
    );

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _parser_hex_and_print(void)
{
    JSON_Parser ctx;
    String *z = NULL;

    /* QUIRKS hexadecimal numbers become integers in the index */
    ASSERT_EQ_INT(jsonpath_init(& ctx), 0);
    ASSERT_NOT_NULL(z = string_alloc("[0xFF, 0x10, 0xdeadbeef]", 24));
    ASSERT_EQ_INT(string_parse_json(z, JSON_QUIRKS, & ctx), 0);
    ASSERT_EQ_INT(jsonpath_print(& ctx), 0);
    z = string_free(z);
    ASSERT_EQ_INT(jsonpath_free(& ctx), 0);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _parser_string_edges(void)
{
    /* a two-byte character at each of the four word alignments */
    ASSERT_EQ_INT(_parse_indexed("[\"\xc3\xa9\"]", JSON_STRICT), 0);
    ASSERT_EQ_INT(_parse_indexed("[\"a\xc3\xa9\"]", JSON_STRICT), 0);
    ASSERT_EQ_INT(_parse_indexed("[\"ab\xc3\xa9\"]", JSON_STRICT), 0);
    ASSERT_EQ_INT(_parse_indexed("[\"abc\xc3\xa9\"]", JSON_STRICT), 0);
    ASSERT_EQ_INT(_parse_indexed("[\"abcd\xc3\xa9\xc3\xa9\"]", JSON_STRICT), 0);

    /* escapes the tokenizer lets through and the parser must reject */
    ASSERT_EQ_INT(_parse_indexed("[\"\\ud83dx\"]", JSON_STRICT), -1);
    ASSERT_EQ_INT(_parse_indexed("[\"\\u12G4\"]", JSON_STRICT), -1);
    ASSERT_EQ_INT(_parse_indexed("[\"\\ud83d\\ud83d\"]", JSON_STRICT), -1);

    /* overlong and surrogate encodings, at every alignment */
    ASSERT_EQ_INT(_parse_indexed("[\"\xc0\x80\"]", JSON_STRICT), -1);
    ASSERT_EQ_INT(_parse_indexed("[\"a\xc1\xbf\"]", JSON_STRICT), -1);
    ASSERT_EQ_INT(_parse_indexed("[\"\xe0\x80\x80\"]", JSON_STRICT), -1);
    ASSERT_EQ_INT(_parse_indexed("[\"ab\xe0\x9f\xbf\"]", JSON_STRICT), -1);
    ASSERT_EQ_INT(_parse_indexed("[\"\xed\xa0\x80\"]", JSON_STRICT), -1);
    ASSERT_EQ_INT(_parse_indexed("[\"abc\xed\xbf\xbf\"]", JSON_STRICT), -1);
    ASSERT_EQ_INT(_parse_indexed("[\"\xf0\x80\x80\x80\"]", JSON_STRICT), -1);
    ASSERT_EQ_INT(_parse_indexed("[\"a\xf0\x8f\xbf\xbf\"]", JSON_STRICT), -1);

    /* QUIRKS: an escaped single quote and a raw carriage return */
    ASSERT_EQ_INT(_parse_indexed("['it\\'s']", JSON_QUIRKS), 0);
    ASSERT_EQ_INT(_parse_indexed("[\"a\rb\"]", JSON_QUIRKS), 0);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _parser_number_edges(void)
{
    /* a number outside the double range is rejected, not saturated */
    ASSERT_EQ_INT(_parse_indexed("[2e308]", JSON_STRICT), -1);
    ASSERT_EQ_INT(_parse_indexed("[-2e308]", JSON_STRICT), -1);

    /* underflow, the extremes and round-to-even ties go through */
    ASSERT_EQ_INT(_parse_indexed("[1e-400]", JSON_STRICT), 0);
    ASSERT_EQ_INT(_parse_indexed("[9007199254740993]", JSON_STRICT), 0);
    ASSERT_EQ_INT(_parse_indexed("[9007199254740992.5]", JSON_STRICT), 0);
    ASSERT_EQ_INT(
        _parse_indexed(
            "[0.1e1, 1.5, -0, 0e0, 123456789012345678901234567890]", JSON_STRICT
        ),
        0
    );
    ASSERT_EQ_INT(
        _parse_indexed(
            "[1.7976931348623157e308, 2.2250738585072014e-308, 4.9e-324]",
            JSON_STRICT
        ),
        0
    );

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _parser_token_limit(void)
{
    /* the canonical loop, as in json_checker: call again while the
       tokenizer returns 1, the context consumes the tokens in between */
    const unsigned int n = 70000;
    char *doc = NULL;
    String *z = NULL;
    JSON_Parser ctx;
    unsigned int i = 0, calls = 0;
    int ret = 0;

    ASSERT_NOT_NULL(doc = malloc(2 * n + 2));
    doc[0] = '[';
    for (i = 0; i < n; i ++) {
        doc[1 + 2 * i] = '1';
        doc[2 + 2 * i] = ',';
    }
    doc[2 * n] = ']';
    z = string_alloc(doc, 2 * n + 1);
    free(doc);
    ASSERT_NOT_NULL(z);

    ASSERT_EQ_INT(jsonpath_init(& ctx), 0);
    do {
        ret = string_parse_json(z, JSON_STRICT, & ctx);
        calls ++;
    } while (ret > 0 && calls < 10);
    ASSERT_EQ_INT(ret, 0);
    ASSERT_EQ_INT(calls, 2);
    ASSERT_EQ_INT(jsonpath_free(& ctx), 0);

    z = string_free(z);

    return 0;
}

/* -------------------------------------------------------------------------- */

static unsigned int _generate(
    char *out,
    size_t size,
    unsigned int depth,
    unsigned int *entries,
    int *own
)
{
    /** @brief a random nested document, returns its number of scalars;
               entries counts the keys the index should hold, own tells
               whether the container gets a key of its own */

    static const char *names[] = {
        "a", "ab", "abc", "b", "id", "name", "n", "x", "xy", "STREET",
        "STREAM", "ST", "k1", "k10", "k2", "0", "00"
    };
    unsigned int i = 0, members = 1 + rand() % 7, scalars = 0, len = 0;
    unsigned int used = 0, pick = 0, direct = 0;
    int array = rand() & 1, child = 0;

    if (size < 64) return 0;

    len = snprintf(out, size, array ? "[" : "{");
    for (i = 0; i < members && size - len > 64; i ++) {
        if (i) len += snprintf(out + len, size - len, ",");
        if (! array) {
            /* member names are unique within an object */
            do pick = rand() % 17; while (used & (1U << pick));
            used |= 1U << pick;
            len += snprintf(out + len, size - len, "\"%s\":", names[pick]);
        }
        if (depth < 4 && rand() % 3 == 0) {
            scalars += _generate(out + len, size - len, depth + 1, entries,
                                 & child);
            len += strlen(out + len);
            direct += child;
        } else {
            len += snprintf(out + len, size - len, "%u", scalars);
            scalars ++; direct ++; (*entries) ++;
        }
    }
    len += snprintf(out + len, size - len, array ? "]" : "}");

    /* a container of containers is indexed on its own, the root is not */
    *own = (! direct);
    if (! direct && depth) (*entries) ++;

    return scalars;
}

/* -------------------------------------------------------------------------- */

static int _parser_index_order(void)
{
    /* index random documents through the real batch flow, read the index
       back through jsonpath_print(), and check that every key comes out
       once, in lexicographic order */
    static char doc[8192], prev[256], key[256];
    static char line[512];
    JSON_Parser ctx;
    String *z = NULL;
    FILE *out = NULL;
    unsigned int round = 0, scalars = 0, seen = 0, prevlen = 0, len = 0;
    unsigned int entries = 0;
    int saved = -1, ret = 0, own = 0;
    char *eq = NULL;

    for (round = 0; round < 50; round ++) {
        entries = 0;
        scalars = _generate(doc, sizeof(doc), 0, & entries, & own);
        if (! scalars) continue;

        ASSERT_EQ_INT(jsonpath_init(& ctx), 0);
        ASSERT_NOT_NULL(z = string_alloc(doc, strlen(doc)));
        ret = string_parse_json(z, JSON_STRICT, & ctx);
        while (ret > 0) ret = string_parse_json(z, JSON_STRICT, & ctx);
        ASSERT_EQ_INT(ret, 0);

        /* capture the printed index */
        ASSERT_NOT_NULL(out = tmpfile());
        fflush(stdout);
        saved = dup(STDOUT_FILENO);
        dup2(fileno(out), STDOUT_FILENO);
        jsonpath_print(& ctx);
        fflush(stdout);
        dup2(saved, STDOUT_FILENO); close(saved);
        rewind(out);

        seen = 0; prevlen = 0; prev[0] = '\0';
        while (fgets(line, sizeof(line), out)) {
            if (! (eq = strstr(line, " = ")) ) continue;
            len = eq - line;
            if (len >= sizeof(key)) len = sizeof(key) - 1;
            memcpy(key, line, len); key[len] = '\0';
            if (seen &&
                memcmp(prev, key, (prevlen < len ? prevlen : len) + 1) >= 0) {
                if (! strcmp(prev, key))
                    printf("duplicate: %s\n", key);
                else printf("out of order: %s then %s\n", prev, key);
                test_fail(__FILE__, __LINE__, "round %u: %s", round, doc);
            }
            memcpy(prev, key, len + 1); prevlen = len;
            seen ++;
        }
        fclose(out);
        if (seen != entries) {
            test_fail(
                __FILE__, __LINE__, "round %u: %u entries, %u indexed: %s",
                round, entries, seen, doc
            );
        }

        z = string_free(z);
        ASSERT_EQ_INT(jsonpath_free(& ctx), 0);
        if (test_status()) return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _parser_index_digits(void)
{
    /* an array index that grows a digit, 9 to 10 or 99 to 100, keeps its
       separator from the enclosing container (D50) */

    static const char doc[] =
        "{\"a\":[0,1,2,3,4,5,6,7,8,9,10,11],"
        "\"b\":[[0,0,0,0,0,0,0,0,0,0,0,0]],\"c\":1}";
    static char text[4096], line[64];
    JSON_Parser ctx;
    String *z = NULL;
    FILE *out = NULL;
    unsigned int i = 0;
    int saved = -1;

    ASSERT_EQ_INT(jsonpath_init(& ctx), 0);
    ASSERT_NOT_NULL(z = string_alloc(doc, sizeof(doc) - 1));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, & ctx), 0);

    /* capture the printed index */
    ASSERT_NOT_NULL(out = tmpfile());
    fflush(stdout);
    saved = dup(STDOUT_FILENO);
    dup2(fileno(out), STDOUT_FILENO);
    jsonpath_print(& ctx);
    fflush(stdout);
    dup2(saved, STDOUT_FILENO); close(saved);
    rewind(out);
    text[fread(text, 1, sizeof(text) - 1, out)] = '\0';
    fclose(out);

    z = string_free(z);
    ASSERT_EQ_INT(jsonpath_free(& ctx), 0);

    for (i = 0; i < 12; i ++) {
        snprintf(line, sizeof(line), "/a/%u = %u.", i, i);
        ASSERT_NOT_NULL(strstr(text, line));
        snprintf(line, sizeof(line), "/b/0/%u = 0.", i);
        ASSERT_NOT_NULL(strstr(text, line));
    }
    ASSERT_NULL(strstr(text, "/a10"));
    ASSERT_NULL(strstr(text, "/b/010"));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _parser_pointer_limit(void)
{
    /* a member name whose escaped form pushes the pointer past 64K bytes
       is refused, one that stays below is indexed (D58) */

    static char doc[80000];
    JSON_Parser ctx;
    String *z = NULL;
    int ret = 0;

    memset(doc, '/', sizeof(doc));
    memcpy(doc, "{\"", 2); memcpy(doc + 33002, "\":1}", 4);
    ASSERT_EQ_INT(jsonpath_init(& ctx), 0);
    ASSERT_NOT_NULL(z = string_alloc(doc, 33006));
    ret = string_parse_json(z, JSON_STRICT, & ctx);
    ASSERT_EQ_INT(ret, -1);
    z = string_free(z);
    ASSERT_EQ_INT(jsonpath_free(& ctx), 0);

    memset(doc, 'a', sizeof(doc));
    memcpy(doc, "{\"", 2); memcpy(doc + 60002, "\":1}", 4);
    ASSERT_EQ_INT(jsonpath_init(& ctx), 0);
    ASSERT_NOT_NULL(z = string_alloc(doc, 60006));
    ret = string_parse_json(z, JSON_STRICT, & ctx);
    ASSERT_EQ_INT(ret, 0);
    z = string_free(z);
    ASSERT_EQ_INT(jsonpath_free(& ctx), 0);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _parser_pointer_carry(void)
{
    /* an array index growing a digit next to the 64K limit is refused
       rather than truncated into the trie key (D66) */

    static char doc[70000];
    JSON_Parser ctx;
    String *z = NULL;
    size_t len = 0;
    int ret = 0, n = 0;

    for (n = 50; n <= 100; n += 50) {
        memcpy(doc, "{\"", 2); memset(doc + 2, 'a', 65530); len = 65532;
        memcpy(doc + len, "\":[", 3); len += 3;
        for (ret = 0; ret < n; ret ++) { doc[len ++] = '0'; doc[len ++] = ','; }
        memcpy(doc + len - 1, "]}", 2); len += 1;

        ASSERT_EQ_INT(jsonpath_init(& ctx), 0);
        ASSERT_NOT_NULL(z = string_alloc(doc, len));
        ret = string_parse_json(z, JSON_STRICT, & ctx);
        ASSERT_EQ_INT(ret, (n == 50) ? 0 : -1);
        z = string_free(z);
        ASSERT_EQ_INT(jsonpath_free(& ctx), 0);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _parser_indexes_document(void)
{
    static const char doc[] =
        "{\"a\":{\"b\":[1,2.5,{\"c\":\"d\"}],\"e\":true},\"f\":null,"
        "\"g\":-1.5e3,\"h\":[[],{}],\"i\":\"\\u611b\"}";
    JSON_Parser ctx;
    JSON_Value v = { 0 };
    String *z = NULL;

    /* XXX the index is private to the context: the parser can only be
       checked for acceptance and for not crashing, until an accessor
       for the JSON Pointer index is exposed */
    ASSERT_EQ_INT(jsonpath_init(& ctx), 0);
    ASSERT_NOT_NULL(z = string_alloc(doc, sizeof(doc) - 1));
    ASSERT_EQ_INT(string_parse_json(z, JSON_STRICT, & ctx), 0);
    ASSERT_EQ_INT(jsonpath_print(& ctx), 0);
    z = string_free(z);

    v = json_pointer_get(&ctx, "/a/b/0", 6);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_DECIMAL);
    ASSERT_TRUE(v.value.decimal == 1.0);

    v = json_pointer_get(& ctx, "/a/b/1", 6);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_DECIMAL);
    ASSERT_TRUE(v.value.decimal == 2.5);

    v = json_pointer_get(& ctx, "/a/b/2/c", 8);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_STRING);
    ASSERT_EQ_UINT(v.len, 1);
    ASSERT_TRUE(! memcmp(v.value.pointer, "d", 1));

    v = json_pointer_get(& ctx, "/a/e", 4);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_BOOLEAN);
    ASSERT_TRUE(v.value.integer);

    v = json_pointer_get(& ctx, "/f", 2);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_NULL);

    v = json_pointer_get(& ctx, "/g", 2);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_DECIMAL);
    ASSERT_TRUE(v.value.decimal == -1500.0);

    v = json_pointer_get(& ctx, "/h/0", 4);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_ARRAY);

    v = json_pointer_get(& ctx, "/h/1", 4);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_OBJECT);

    v = json_pointer_get(& ctx, "/i", 2);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_STRING);
    ASSERT_EQ_UINT(v.len, 3);
    ASSERT_TRUE(!memcmp(v.value.pointer, "\xe6\x84\x9b", 3));

    ASSERT_EQ_INT(jsonpath_free(& ctx), 0);

    ASSERT_EQ_INT(jsonpath_init(NULL), -1);
    ASSERT_EQ_INT(jsonpath_free(NULL), -1);

    return 0;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("strict_tokenizes_document", _strict_tokenizes_document),
    TEST("top_level_number_flagged", _top_level_number_flagged),
    TEST("top_level_number_resumed", _top_level_number_resumed),
    TEST("incomplete_flagged_and_resumed", _incomplete_flagged_and_resumed),
    TEST("stream_nested_resumed", _stream_nested_resumed),
    TEST("stream_concatenated_documents", _stream_concatenated_documents),
    TEST("incomplete_string_resumed", _incomplete_string_resumed),
    TEST("quirks_extensions", _quirks_extensions),
    TEST("deep_nesting", _deep_nesting),
    TEST_TODO("hostile_nesting", _hostile_nesting,
        "string_free_token() recurses to token depth (overflows under TSan)"),
    TEST("many_siblings", _many_siblings),
    TEST("reparse_replaces_tokens", _reparse_replaces_tokens),
    TEST("resume_at_boundaries", _resume_at_boundaries),
    TEST("quirks_incomplete_primitives", _quirks_incomplete_primitives),
    TEST("string_edges", _string_edges),
    TEST("number_edges", _number_edges),
    TEST("token_limit", _token_limit),
    #if defined(_ENABLE_JSON) && defined(_ENABLE_TRIE)
    TEST("parser_validates_strings", _parser_validates_strings),
    TEST("parser_wide_and_deep", _parser_wide_and_deep),
    TEST("parser_hex_and_print", _parser_hex_and_print),
    TEST("parser_string_edges", _parser_string_edges),
    TEST("parser_number_edges", _parser_number_edges),
    TEST("parser_token_limit", _parser_token_limit),
    TEST("parser_index_order", _parser_index_order),
    TEST("parser_index_digits", _parser_index_digits),
    TEST("parser_pointer_limit", _parser_pointer_limit),
    TEST("parser_pointer_carry", _parser_pointer_carry),
    TEST("parser_scalars_and_failed_keys", _parser_scalars_and_failed_keys),
    TEST("parser_indexes_document", _parser_indexes_document)
    #endif
};

TEST_SUITE(test_suite_json, "json", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
