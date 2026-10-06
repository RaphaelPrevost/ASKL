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
#include "../fixtures/jsontestsuite.h"

#define FIXTURES (sizeof(_jsontestsuite) / sizeof(*_jsontestsuite))

/* verdicts of the tokenizer on a document */
#define V_ACCEPT     'A' /* one complete root value */
#define V_REJECT     'R' /* string_parse_json() returned -1 */
#define V_INCOMPLETE 'I' /* the root value is flagged incomplete */
#define V_NUMBER     'N' /* a bare number, which a stream cannot end */
#define V_TRAILING   'T' /* more than one root value */
#define V_EMPTY      'E' /* no value at all */

/* the recursive release of a token tree overflows an instrumented stack */
#if defined(__SANITIZE_THREAD__)
#define SKIP_ABOVE 50000
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define SKIP_ABOVE 50000
#endif
#endif
#ifndef SKIP_ABOVE
#define SKIP_ABOVE ((size_t) -1)
#endif

/* -------------------------------------------------------------------------- */

static size_t _length(const JSON_Fixture *f)
{
    return f->plen * f->repeat + f->tlen;
}

/* -------------------------------------------------------------------------- */

static char _verdict(const JSON_Fixture *f, int parse)
{
    /** @brief tokenize, or parse, a fixture and classify the outcome */

    String *z = NULL;
    unsigned int i = 0;
    int ret = 0;
    char v = 0;
    const char *end = NULL;
    #if defined(_ENABLE_JSON) && defined(_ENABLE_TRIE)
    JSON_Parser ctx;
    #else
    if (parse) return '-';
    #endif

    if (! (z = string_reserve(NULL, _length(f), 1)) ) return 0;

    for (i = 0; i < f->repeat; i ++)
        memcpy(z->data + i * f->plen, f->pattern, f->plen);
    memcpy(z->data + f->repeat * f->plen, f->tail, f->tlen);

    #if defined(_ENABLE_JSON) && defined(_ENABLE_TRIE)
    if (parse) {
        if (jsonpath_init(& ctx) == -1) return string_free(z), 0;
        ret = string_parse_json(z, JSON_STRICT, & ctx);
        jsonpath_free(& ctx);
    } else
    #endif
    ret = string_parse_json(z, JSON_STRICT, NULL);

    if (ret == -1) v = V_REJECT;
    else if (! z->count) v = V_EMPTY;
    else if (z->count > 1) v = V_TRAILING;
    else if (! HAS_ERROR(& z->tokens[0])) v = V_ACCEPT;
    else if (IS_PRIMITIVE(& z->tokens[0])) v = V_NUMBER;
    else v = V_INCOMPLETE;

    /* nothing but white space may follow a complete value; the parser
       returns after the first one without looking further */
    if (v == V_ACCEPT) {
        end = string_end(& z->tokens[0]) + (IS_STRING(& z->tokens[0]) != 0);
        while (end < string_end(z) && strchr(" \t\r\n", *end)) end ++;
        if (end < string_end(z)) v = V_TRAILING;
    }

    string_free(z);

    return v;
}

/* -------------------------------------------------------------------------- */

static void _write_literal(FILE *out, const char *s, size_t len)
{
    /** @brief write a C string literal, as lines of at most 68 columns */

    char line[80], escaped[8];
    size_t i = 0, n = 0, k = 0;
    unsigned char c = 0;

    for (i = 0; i < len; i ++) {
        c = s[i];
        if (c == '"' || c == '?' || c == '\\')
            k = snprintf(escaped, sizeof(escaped), "\\%c", c);
        else if (c >= 0x20 && c < 0x7f)
            k = snprintf(escaped, sizeof(escaped), "%c", c);
        else k = snprintf(escaped, sizeof(escaped), "\\%03o", c);
        if (n + k > 68) {
            fprintf(out, "        \"%s\"\n", line);
            n = 0;
        }
        memcpy(line + n, escaped, k); n += k; line[n] = '\0';
    }

    line[n] = '\0';
    fprintf(out, "        \"%s\"\n", line);
}

/* -------------------------------------------------------------------------- */

static int _write_header(
    const char *path,
    const char *verdicts,
    const char *parsed
)
{
    /** @brief rewrite the fixtures header with the given verdicts */

    FILE *out = NULL;
    unsigned int i = 0;
    const JSON_Fixture *f = NULL;

    if (! (out = fopen(path, "w")) ) {
        perror(ERR(_write_header, fopen));
        return -1;
    }

    fprintf(
        out,
        "/* generated by the conformance suite from JSONTestSuite %s,\n"
        "   https://github.com/nst/JSONTestSuite (MIT license,\n"
        "   see JSONTestSuite.LICENSE); do not edit */\n\n"
        "#ifndef ASKL_JSONTESTSUITE_H\n\n#define ASKL_JSONTESTSUITE_H\n\n"
        "typedef struct JSON_Fixture {\n"
        "    const char *name;\n"
        "    char class;      /* y: valid, n: invalid, i: implementation"
        " defined */\n"
        "    char verdict;    /* pinned tokenizer verdict, '-' if not"
        " pinned */\n"
        "    char parsed;     /* same, with the parser context */\n"
        "    const char *pattern;\n"
        "    size_t plen;\n"
        "    unsigned int repeat;\n"
        "    const char *tail;\n"
        "    size_t tlen;\n"
        "} JSON_Fixture;\n\n"
        "#define JSONTESTSUITE_COMMIT \"%s\"\n\n"
        "static const JSON_Fixture _jsontestsuite[] = {\n",
        JSONTESTSUITE_COMMIT, JSONTESTSUITE_COMMIT
    );

    for (i = 0; i < FIXTURES; i ++) {
        f = & _jsontestsuite[i];
        fprintf(
            out, "    {\n        \"%s\", '%c', '%c', '%c',\n",
            f->name, f->class, verdicts[i] ? verdicts[i] : f->verdict,
            parsed[i] ? parsed[i] : f->parsed
        );
        _write_literal(out, f->pattern, f->plen);
        fprintf(out, "        , %zu, %u,\n", f->plen, f->repeat);
        _write_literal(out, f->tail, f->tlen);
        fprintf(out, "        , %zu\n    },\n", f->tlen);
    }

    fprintf(out, "};\n\n#endif\n");

    return fclose(out);
}

/* -------------------------------------------------------------------------- */

static int _valid(int parse)
{
    unsigned int i = 0, n = 0;
    char v = 0;

    for (i = 0; i < FIXTURES; i ++) {
        if (_jsontestsuite[i].class != 'y') continue;
        if (_length(& _jsontestsuite[i]) > SKIP_ABOVE) continue;
        n ++;
        if ( (v = _verdict(& _jsontestsuite[i], parse)) == '-')
            SKIP("the parser is not built");
        if (v == V_ACCEPT) continue;
        /* a bare number at the end of the input cannot be complete */
        if (v == V_NUMBER) {
            printf("%s: bare number, incomplete\n", _jsontestsuite[i].name);
            continue;
        }
        test_fail(__FILE__, __LINE__, "%s: %c", _jsontestsuite[i].name, v);
    }

    printf("%u valid documents\n", n);

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _invalid(int parse)
{
    unsigned int i = 0, n = 0;
    char v = 0;

    for (i = 0; i < FIXTURES; i ++) {
        if (_jsontestsuite[i].class != 'n') continue;
        if (_length(& _jsontestsuite[i]) > SKIP_ABOVE) continue;
        n ++;
        /* a stream may hold an invalid document as incomplete, never as
           a complete value */
        if ( (v = _verdict(& _jsontestsuite[i], parse)) == '-')
            SKIP("the parser is not built");
        if (v == V_ACCEPT) {
            test_fail(
                __FILE__, __LINE__, "%s: accepted", _jsontestsuite[i].name
            );
        }
    }

    printf("%u invalid documents\n", n);

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _implementation_defined(void)
{
    unsigned int i = 0, n = 0;

    /* no verdict is right or wrong, the profile pins the current one */
    for (i = 0; i < FIXTURES; i ++) {
        if (_jsontestsuite[i].class != 'i') continue;
        if (_length(& _jsontestsuite[i]) > SKIP_ABOVE) continue;
        n ++;
        ASSERT_NE_INT(_verdict(& _jsontestsuite[i], 0), 0);
        ASSERT_NE_INT(_verdict(& _jsontestsuite[i], 1), 0);
    }

    printf("%u implementation defined documents\n", n);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _valid_documents(void) { return _valid(0); }
static int _invalid_documents(void) { return _invalid(0); }
static int _parser_valid_documents(void) { return _valid(1); }
static int _parser_invalid_documents(void) { return _invalid(1); }

/* -------------------------------------------------------------------------- */

static int _profile(void)
{
    /** @brief every verdict must match the one pinned in the table */

    static char verdicts[FIXTURES], parsed[FIXTURES];
    unsigned int i = 0, pinned = 0;
    const char *path = getenv("ASKL_TEST_PROFILE");
    const JSON_Fixture *f = NULL;

    for (i = 0; i < FIXTURES; i ++) {
        f = & _jsontestsuite[i];
        if (_length(f) > SKIP_ABOVE) continue;
        verdicts[i] = _verdict(f, 0);
        parsed[i] = _verdict(f, 1);
        if (f->verdict != '-') {
            pinned ++;
            if (verdicts[i] != f->verdict) {
                test_fail(
                    __FILE__, __LINE__, "%s: tokenizer was %c, now %c",
                    f->name, f->verdict, verdicts[i]
                );
            }
        }
        if (f->parsed != '-' && parsed[i] != '-') {
            pinned ++;
            if (parsed[i] != f->parsed) {
                test_fail(
                    __FILE__, __LINE__, "%s: parser was %c, now %c",
                    f->name, f->parsed, parsed[i]
                );
            }
        }
    }

    /* ASKL_TEST_PROFILE=test/fixtures/jsontestsuite.h pins the verdicts */
    if (path && _write_header(path, verdicts, parsed) == -1) return -1;

    printf("%u of %u verdicts pinned\n", pinned, 2 * (unsigned int) FIXTURES);

    if (! pinned) SKIP("no pinned profile, run with ASKL_TEST_PROFILE=file");

    return test_status();
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("valid_documents", _valid_documents),
    TEST("invalid_documents", _invalid_documents),
    TEST("parser_valid_documents", _parser_valid_documents),
    TEST("parser_invalid_documents", _parser_invalid_documents),
    TEST("implementation_defined", _implementation_defined),
    TEST("profile", _profile)
};

TEST_SUITE(test_suite_conformance, "conformance", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
