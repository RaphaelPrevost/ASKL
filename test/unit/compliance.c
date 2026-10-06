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
#if (defined(_ENABLE_JSON) && defined(_ENABLE_TRIE))
/* -------------------------------------------------------------------------- */

#include "../askl_test.h"
#include "../../lib/askl_json.h"
#include "../fixtures/cts.h"

#define FIXTURES (sizeof(_cts) / sizeof(*_cts))

/* classes of a fixture */
#define C_VALID   'v' /* a valid selector, with its results */
#define C_INVALID 'n' /* a selector that must be rejected */
#define C_REGEX   'r' /* a valid selector calling match() or search() */

/* verdicts */
#define V_PASS 'P'
#define V_FAIL 'F'

/* the results of a query, each pointer after a \001, the admissible
   orderings apart by a \002 */
#define P_SEP '\001'
#define A_SEP '\002'

/* -------------------------------------------------------------------------- */

typedef struct _Results {
    char *data;
    size_t len;
    size_t size;
} _Results;

static int _add(_Results *r, const char *s, size_t len)
{
    /** @brief append bytes to a growable buffer */

    char *grown = NULL;
    size_t size = r->size;

    if (r->len + len + 1 > size) {
        size = (size) ? size : 256;
        while (r->len + len + 1 > size) size *= 2;
        if (! (grown = realloc(r->data, size)) ) return -1;
        r->data = grown; r->size = size;
    }

    memcpy(r->data + r->len, s, len);
    r->len += len;
    r->data[r->len] = '\0';

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _collect(const char *ptr, size_t len, void *arg)
{
    _Results *r = arg;
    char sep = P_SEP;

    if (_add(r, & sep, 1) == -1 || _add(r, ptr, len) == -1) return -1;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _hex(const char *s)
{
    int i = 0, v = 0;

    for (i = 0; i < 4; i ++) {
        if (s[i] >= '0' && s[i] <= '9') v = (v << 4) | (s[i] - '0');
        else if (s[i] >= 'a' && s[i] <= 'f') v = (v << 4) | (s[i] - 'a' + 10);
        else if (s[i] >= 'A' && s[i] <= 'F') v = (v << 4) | (s[i] - 'A' + 10);
        else return -1;
    }

    return v;
}

/* -------------------------------------------------------------------------- */

static int _pointer(const char *path, size_t len, _Results *out)
{
    /** @brief the RFC 6901 pointer of an RFC 9535 normalized path, such
               as $['a'][0], appended to out after a separator */

    size_t i = 1;
    int cp = 0;
    char c = 0, sep = P_SEP, tok[4];

    if (! len || path[0] != '$') return -1;
    if (_add(out, & sep, 1) == -1) return -1;

    while (i < len) {
        if (path[i ++] != '[') return -1;
        if (_add(out, "/", 1) == -1) return -1;
        if (path[i] == '\'') {
            i ++;
            while (i < len && path[i] != '\'') {
                c = path[i ++];
                if (c == '\\') {
                    if (i >= len) return -1;
                    switch ((c = path[i ++])) {
                    case 'b': c = '\b'; break;
                    case 'f': c = '\f'; break;
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    case 'u':
                        if (i + 4 > len || (cp = _hex(path + i)) == -1)
                            return -1;
                        i += 4;
                        if (cp < 0x80) { c = cp; break; }
                        if (cp < 0x800) {
                            tok[0] = 0xc0 | (cp >> 6);
                            tok[1] = 0x80 | (cp & 0x3f);
                            if (_add(out, tok, 2) == -1) return -1;
                        } else {
                            tok[0] = 0xe0 | (cp >> 12);
                            tok[1] = 0x80 | ((cp >> 6) & 0x3f);
                            tok[2] = 0x80 | (cp & 0x3f);
                            if (_add(out, tok, 3) == -1) return -1;
                        }
                        continue;
                    default: break;
                    }
                }
                if (c == '~') { if (_add(out, "~0", 2) == -1) return -1; }
                else if (c == '/') { if (_add(out, "~1", 2) == -1) return -1; }
                else if (_add(out, & c, 1) == -1) return -1;
            }
            if (i >= len) return -1;
            i ++;
        } else {
            while (i < len && path[i] >= '0' && path[i] <= '9')
                if (_add(out, & path[i ++], 1) == -1) return -1;
        }
        if (i >= len || path[i ++] != ']') return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static char _outcome(const JSONPath_Fixture *f, _Results *detail)
{
    /** @brief run a fixture: P if the results are those expected */

    JSON_Parser ctx;
    JSONPath_Query *q = NULL;
    String *z = NULL;
    _Results got;
    const char *alt = NULL, *end = NULL, *next = NULL;
    char v = V_FAIL;
    int n = 0, ret = 0;

    memset(& got, 0, sizeof(got));
    detail->len = 0;

    if (! (q = jsonpath_query_alloc(f->selector, f->slen)) ) {
        if (f->class == C_INVALID) return V_PASS;
        _add(detail, "rejected", 8);
        return V_FAIL;
    }
    if (f->class == C_INVALID) {
        jsonpath_query_free(q);
        _add(detail, "accepted", 8);
        return V_FAIL;
    }

    /* a bare number ends with the input: give it a trailing space */
    if (jsonpath_init(& ctx) == -1) goto _err_ctx;
    if (! (z = string_alloc(f->document, f->dlen)) ) goto _err_doc;
    if (! string_append_buffer(z, " ", 1)) goto _err_parse;
    ret = string_parse_json(z, JSON_STRICT, & ctx);
    while (ret > 0) ret = string_parse_json(z, JSON_STRICT, & ctx);
    if (ret == -1) {
        _add(detail, "document rejected", 17);
        goto _err_parse;
    }

    n = jsonpath_foreach(& ctx, q, _collect, & got);
    if (n == -1) {
        _add(detail, "query failed", 12);
        goto _err_parse;
    }

    /* one of the admissible orderings must match exactly */
    end = f->paths + f->plen;
    for (alt = f->paths; alt <= end; alt = next + 1) {
        next = memchr(alt, A_SEP, end - alt);
        if (! next) next = end;
        if ((size_t) (next - alt) == got.len &&
            ! memcmp(alt, got.data ? got.data : "", got.len)) v = V_PASS;
    }

    if (v == V_FAIL) {
        _add(detail, "got", 3);
        _add(detail, got.data ? got.data : "", got.len);
        _add(detail, " expected", 9);
        _add(detail, f->paths, f->plen);
    }

_err_parse:
    string_free(z);
_err_doc:
    jsonpath_free(& ctx);
_err_ctx:
    jsonpath_query_free(q);
    free(got.data);

    return v;
}

/* -------------------------------------------------------------------------- */

static void _print(const char *s, size_t len)
{
    /** @brief print results, separators shown as spaces and pipes */

    size_t i = 0;

    for (i = 0; i < len; i ++) {
        if (s[i] == P_SEP) putchar(' ');
        else if (s[i] == A_SEP) fputs(" | ", stdout);
        else putchar(s[i]);
    }
}

/* -------------------------------------------------------------------------- */

static int _class(char class)
{
    /** @brief every fixture of a class passes */

    _Results detail;
    unsigned int i = 0, n = 0;
    char v = 0;

    memset(& detail, 0, sizeof(detail));

    for (i = 0; i < FIXTURES; i ++) {
        if (_cts[i].class != class) continue;
        n ++;
        if ( (v = _outcome(& _cts[i], & detail)) == V_PASS) continue;
        printf("%s: %s: ", _cts[i].name, _cts[i].selector);
        _print(detail.data, detail.len);
        putchar('\n');
        test_fail(__FILE__, __LINE__, "%s", _cts[i].name);
    }

    free(detail.data);
    printf("%u selectors\n", n);

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _valid_selectors(void) { return _class(C_VALID); }
static int _invalid_selectors(void) { return _class(C_INVALID); }

static int _regex_selectors(void)
{
    #ifndef HAS_PCRE
    SKIP("the library was built without PCRE");
    #else
    return _class(C_REGEX);
    #endif
}

/* -------------------------------------------------------------------------- */

static void _literal(FILE *out, const char *s, size_t len)
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
    const char *commit,
    const JSONPath_Fixture *f,
    unsigned int n,
    const char *verdicts
)
{
    /** @brief write the fixtures header with the given verdicts */

    FILE *out = NULL;
    unsigned int i = 0;

    if (! (out = fopen(path, "w")) ) {
        perror(ERR(_write_header, fopen));
        return -1;
    }

    fprintf(
        out,
        "/* generated by the compliance suite from the JSONPath Compliance\n"
        "   Test Suite %s, https://github.com/jsonpath-standard/\n"
        "   jsonpath-compliance-test-suite (BSD-2 license, see\n"
        "   CTS.LICENSE); do not edit */\n\n"
        "#ifndef ASKL_CTS_H\n\n#define ASKL_CTS_H\n\n"
        "typedef struct JSONPath_Fixture {\n"
        "    const char *name;\n"
        "    char class;      /* v: valid, n: invalid, r: valid, needs"
        " PCRE */\n"
        "    char verdict;    /* pinned verdict, P or F, '-' if not"
        " pinned */\n"
        "    const char *selector;\n"
        "    size_t slen;\n"
        "    const char *document;\n"
        "    size_t dlen;\n"
        "    const char *paths;   /* the results as RFC 6901 pointers,"
        " each after\n"
        "                            a \\001, admissible orderings apart"
        " by a \\002 */\n"
        "    size_t plen;\n"
        "} JSONPath_Fixture;\n\n"
        "#define CTS_COMMIT \"%s\"\n\n"
        "static const JSONPath_Fixture _cts[] = {\n",
        commit, commit
    );

    for (i = 0; i < n; i ++) {
        fprintf(out, "    {\n");
        _literal(out, f[i].name, strlen(f[i].name));
        fprintf(out, "        , '%c', '%c',\n", f[i].class,
                (verdicts[i]) ? verdicts[i] : f[i].verdict);
        _literal(out, f[i].selector, f[i].slen);
        fprintf(out, "        , %zu,\n", f[i].slen);
        _literal(out, f[i].document, f[i].dlen);
        fprintf(out, "        , %zu,\n", f[i].dlen);
        _literal(out, f[i].paths, f[i].plen);
        fprintf(out, "        , %zu\n    },\n", f[i].plen);
    }

    fprintf(out, "};\n\n#endif\n");

    return fclose(out);
}

/* -------------------------------------------------------------------------- */

static const String *_member(const String *object, const char *name)
{
    /** @brief the value of a member of an object token, or NULL */

    unsigned int i = 0;
    size_t len = strlen(name);

    for (i = 0; i + 1 < object->count; i += 2) {
        if (object->tokens[i].len == len &&
            ! memcmp(object->tokens[i].data, name, len))
            return & object->tokens[i + 1];
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static char *_string(JSON_Parser *ctx, const char *ptr, size_t *len)
{
    /** @brief a copy of the string the index holds under a pointer */

    JSON_Value v = json_pointer_get(ctx, ptr, strlen(ptr));
    char *s = NULL;

    if (v.type != JSON_VALUE_STRING) return NULL;
    if (! (s = malloc(v.len + 1)) ) return NULL;

    memcpy(s, v.value.pointer, v.len);
    s[v.len] = '\0';

    if (len) *len = v.len;

    return s;
}

/* -------------------------------------------------------------------------- */

static int _paths(JSON_Parser *ctx, const char *ptr, _Results *out)
{
    /** @brief the pointers of the normalized paths of an array, -1 if
               the array does not exist */

    char key[128], *path = NULL;
    size_t len = 0;
    unsigned int j = 0;
    JSON_Value v = json_pointer_get(ctx, ptr, strlen(ptr));

    if (v.type == JSON_VALUE_ARRAY) return 0;

    for (j = 0; ; j ++) {
        snprintf(key, sizeof(key), "%s/%u", ptr, j);
        if (! (path = _string(ctx, key, & len)) ) break;
        if (_pointer(path, len, out) == -1) {
            free(path);
            return -1;
        }
        free(path);
    }

    return (j) ? 0 : -1;
}

/* -------------------------------------------------------------------------- */

static int _import(
    const char *file,
    JSONPath_Fixture **fixtures,
    unsigned int *count
)
{
    /** @brief the fixtures of the suite's cts.json, read through the
               tokenizer for the documents and the index for the rest */

    FILE *in = NULL;
    String *z = NULL, *tokens = NULL;
    const String *tests = NULL, *doc = NULL;
    JSON_Parser ctx;
    JSONPath_Fixture *f = NULL;
    _Results paths;
    JSON_Value v = { 0 };
    char key[128], *text = NULL, sep = A_SEP;
    size_t len = 0;
    long size = 0;
    unsigned int i = 0, k = 0;
    int ret = 0;

    memset(& paths, 0, sizeof(paths));

    if (! (in = fopen(file, "rb")) ) {
        perror(ERR(_import, fopen));
        return -1;
    }
    fseek(in, 0, SEEK_END); size = ftell(in); fseek(in, 0, SEEK_SET);
    if (size <= 0 || ! (text = malloc(size)) ||
        fread(text, 1, size, in) != (size_t) size) {
        fclose(in); free(text);
        return -1;
    }
    fclose(in);

    /* the token tree gives the text of each document */
    if (! (tokens = string_alloc(text, size)) ) goto _err_tokens;
    if (string_parse_json(tokens, JSON_STRICT, NULL) == -1 ||
        tokens->count != 1 || ! IS_OBJECT(& tokens->tokens[0]) ||
        ! (tests = _member(& tokens->tokens[0], "tests")) ) goto _err_tree;

    /* the index gives everything else, decoded */
    if (jsonpath_init(& ctx) == -1) goto _err_tree;
    if (! (z = string_alloc(text, size)) ) goto _err_index;
    ret = string_parse_json(z, JSON_STRICT, & ctx);
    while (ret > 0) ret = string_parse_json(z, JSON_STRICT, & ctx);
    if (ret == -1) goto _err_parse;

    if (! (f = calloc(tests->count, sizeof(*f))) ) goto _err_parse;

    for (i = 0; i < tests->count; i ++) {
        snprintf(key, sizeof(key), "/tests/%u/name", i);
        if (! (f[i].name = _string(& ctx, key, NULL)) ) goto _err_fixtures;
        snprintf(key, sizeof(key), "/tests/%u/selector", i);
        if (! (f[i].selector = _string(& ctx, key, & f[i].slen)) )
            goto _err_fixtures;
        f[i].verdict = '-';
        f[i].class = (strstr(f[i].selector, "match(") ||
                      strstr(f[i].selector, "search(")) ? C_REGEX : C_VALID;

        snprintf(key, sizeof(key), "/tests/%u/invalid_selector", i);
        v = json_pointer_get(& ctx, key, strlen(key));
        if (v.type == JSON_VALUE_BOOLEAN && v.value.integer) {
            f[i].class = C_INVALID;
            f[i].document = strdup("");
            f[i].paths = strdup("");
            if (! f[i].document || ! f[i].paths) goto _err_fixtures;
            continue;
        }

        if (! (doc = _member(& tests->tokens[i], "document")) )
            goto _err_fixtures;
        len = doc->len + (IS_STRING(doc) != 0) * 2;
        if (! (text = malloc(len + 1)) ) goto _err_fixtures;
        if (IS_STRING(doc)) {
            text[0] = '"'; memcpy(text + 1, doc->data, doc->len);
            text[len - 1] = '"';
        } else memcpy(text, doc->data, doc->len);
        text[len] = '\0';
        f[i].document = text; f[i].dlen = len;

        /* one ordering, or several admissible ones */
        paths.len = 0;
        snprintf(key, sizeof(key), "/tests/%u/result_paths", i);
        if (_paths(& ctx, key, & paths) == -1) {
            for (k = 0; ; k ++) {
                snprintf(key, sizeof(key), "/tests/%u/results_paths/%u",
                         i, k);
                if (k && _add(& paths, & sep, 1) == -1) goto _err_fixtures;
                if (_paths(& ctx, key, & paths) == -1) break;
            }
            if (! k) goto _err_fixtures;
            paths.len --;
        }
        if (! (f[i].paths = malloc(paths.len + 1)) ) goto _err_fixtures;
        memcpy((char *) f[i].paths, paths.data, paths.len);
        ((char *) f[i].paths)[paths.len] = '\0';
        f[i].plen = paths.len;
    }

    *fixtures = f; *count = tests->count;
    free(paths.data);
    string_free(z); jsonpath_free(& ctx); string_free(tokens);

    return 0;

_err_fixtures:
    for (i = 0; i < tests->count; i ++) {
        free((char *) f[i].name); free((char *) f[i].selector);
        free((char *) f[i].document); free((char *) f[i].paths);
    }
    free(f);
    free(paths.data);
_err_parse:
    string_free(z);
_err_index:
    jsonpath_free(& ctx);
_err_tree:
    string_free(tokens);
_err_tokens:
    return -1;
}

/* -------------------------------------------------------------------------- */

static int _profile(void)
{
    /** @brief every verdict must match the one pinned in the table;
               ASKL_TEST_CTS=cts.json imports the suite anew, and
               ASKL_TEST_PROFILE=test/fixtures/cts.h pins the verdicts */

    JSONPath_Fixture *imported = NULL;
    const JSONPath_Fixture *f = _cts;
    _Results detail;
    char *verdicts = NULL;
    const char *path = getenv("ASKL_TEST_PROFILE");
    const char *corpus = getenv("ASKL_TEST_CTS");
    const char *commit = getenv("ASKL_TEST_CTS_COMMIT");
    unsigned int i = 0, n = FIXTURES, pinned = 0, skipped = 0;
    int ret = 0;

    memset(& detail, 0, sizeof(detail));

    if (corpus) {
        if (_import(corpus, & imported, & n) == -1) FAIL("cannot import");
        f = imported;
    }

    if (! (verdicts = calloc(n, 1)) ) FAIL("out of memory");

    for (i = 0; i < n; i ++) {
        #ifndef HAS_PCRE
        if (f[i].class == C_REGEX) { skipped ++; continue; }
        #endif
        verdicts[i] = _outcome(& f[i], & detail);
        if (f[i].verdict == '-') continue;
        pinned ++;
        if (verdicts[i] != f[i].verdict) {
            test_fail(
                __FILE__, __LINE__, "%s: was %c, now %c", f[i].name,
                f[i].verdict, verdicts[i]
            );
        }
    }

    if (path && _write_header(path, (commit) ? commit : CTS_COMMIT, f, n,
                              verdicts) == -1) ret = -1;

    printf("%u of %u verdicts pinned, %u skipped\n", pinned, n, skipped);

    if (imported) {
        for (i = 0; i < n; i ++) {
            free((char *) f[i].name); free((char *) f[i].selector);
            free((char *) f[i].document); free((char *) f[i].paths);
        }
        free(imported);
    }
    free(verdicts); free(detail.data);

    if (ret == -1) return -1;
    if (! pinned) SKIP("no pinned profile, run with ASKL_TEST_PROFILE=file");

    return test_status();
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("valid_selectors", _valid_selectors),
    TEST("invalid_selectors", _invalid_selectors),
    TEST("regex_selectors", _regex_selectors),
    TEST("profile", _profile)
};

TEST_SUITE(test_suite_compliance, "compliance", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
#endif /* _ENABLE_JSON && _ENABLE_TRIE */
/* -------------------------------------------------------------------------- */
