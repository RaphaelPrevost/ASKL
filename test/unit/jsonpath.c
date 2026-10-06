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

/* the RFC 9535 bookstore */
static const char _store[] =
    "{ \"store\": {"
    "    \"book\": ["
    "      { \"category\": \"reference\", \"author\": \"Nigel Rees\","
    "        \"title\": \"Sayings of the Century\", \"price\": 8.95 },"
    "      { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\","
    "        \"title\": \"Sword of Honour\", \"price\": 12.99 },"
    "      { \"category\": \"fiction\", \"author\": \"Herman Melville\","
    "        \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\","
    "        \"price\": 8.99 },"
    "      { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\","
    "        \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\","
    "        \"price\": 22.99 }"
    "    ],"
    "    \"bicycle\": { \"color\": \"red\", \"price\": 399 }"
    "  }"
    "}";

/* -------------------------------------------------------------------------- */

static JSON_Parser _ctx;
static String *_doc = NULL;
static char _buf[4096];

static int _index(const char *doc)
{
    /** @brief index a document into the shared context */

    _doc = string_free(_doc);
    if (_ctx.context) jsonpath_free(& _ctx);
    memset(& _ctx, 0, sizeof(_ctx));

    if (jsonpath_init(& _ctx) == -1) return -1;
    if (! (_doc = string_alloc(doc, strlen(doc))) ) return -1;

    return string_parse_json(_doc, JSON_STRICT, & _ctx);
}

/* -------------------------------------------------------------------------- */

static void _teardown(void)
{
    _doc = string_free(_doc);
    if (_ctx.context) jsonpath_free(& _ctx);
    memset(& _ctx, 0, sizeof(_ctx));
}

/* -------------------------------------------------------------------------- */

static int _collect(const char *ptr, size_t len, void *arg)
{
    /** @brief append "[pointer=value]" to the result buffer */

    int values = * (int *) arg;
    size_t n = strlen(_buf);
    char *out = _buf + n;
    size_t size = sizeof(_buf) - n;
    JSON_Value v = { 0 };

    if (strlen(ptr) != len) return -1;

    n = snprintf(out, size, "[%s", ptr);
    if (n >= size) return -1;
    out += n; size -= n;

    if (values) {
        v = jsonpath_get(&_ctx, ptr, len);

        switch (v.type) {
        case JSON_VALUE_NULL:
            n = snprintf(out, size, "=null");
            break;

        case JSON_VALUE_BOOLEAN:
            n = snprintf(out, size, "=%s",
                         v.value.integer ? "true" : "false");
            break;

        case JSON_VALUE_INTEGER:
            n = snprintf(out, size, "=%lld",
                         (long long) (int64_t) v.value.integer);
            break;

        case JSON_VALUE_DECIMAL:
            n = snprintf(out, size, "=%g", v.value.decimal);
            break;

        case JSON_VALUE_STRING:
            n = snprintf(out, size, "=\"%.*s\"", (int) v.len,
                         (const char *) v.value.pointer);
            break;

        case JSON_VALUE_ARRAY:
            n = snprintf(out, size, "=[%u]", v.len);
            break;

        case JSON_VALUE_OBJECT:
            n = snprintf(out, size, "={%u}", v.len);
            break;

        default:
            n = snprintf(out, size, "=?");
        }

        if (n >= size) return -1;
        out += n; size -= n;
    }

    return (snprintf(out, size, "]") >= (int) size) ? -1 : 0;
}

/* -------------------------------------------------------------------------- */

static int _run(const char *expr, int values)
{
    /** @brief the number of nodes, -1 on error, -2 if the query is rejected */

    JSONPath_Query *q = NULL;
    int n = 0;

    _buf[0] = '\0';
    if (! (q = jsonpath_query_alloc(expr, strlen(expr))) ) return -2;
    n = jsonpath_foreach(& _ctx, q, _collect, & values);
    jsonpath_query_free(q);

    return n;
}

/* -------------------------------------------------------------------------- */

#define EXPECT(expr, want) do { \
    int _n = _run(expr, 0); \
    if (_n < 0 || strcmp(_buf, want)) { \
        test_fail( \
            __FILE__, __LINE__, "%s -> %d %s\n    expected %s", \
            expr, _n, _buf, want \
        ); \
        return -1; \
    } \
} while (0)

#define EXPECT_VALUES(expr, want) do { \
    int _n = _run(expr, 1); \
    if (_n < 0 || strcmp(_buf, want)) { \
        test_fail( \
            __FILE__, __LINE__, "%s -> %d %s\n    expected %s", \
            expr, _n, _buf, want \
        ); \
        return -1; \
    } \
} while (0)

#define REJECT(expr) do { \
    JSONPath_Query *_q = jsonpath_query_alloc(expr, strlen(expr)); \
    if (_q) { \
        jsonpath_query_free(_q); \
        test_fail(__FILE__, __LINE__, "accepted: %s", expr); \
        return -1; \
    } \
} while (0)

#define ACCEPT(expr) do { \
    JSONPath_Query *_q = jsonpath_query_alloc(expr, strlen(expr)); \
    if (! _q) { \
        test_fail(__FILE__, __LINE__, "rejected: %s", expr); \
        return -1; \
    } \
    jsonpath_query_free(_q); \
} while (0)

/* -------------------------------------------------------------------------- */

static int _bookstore(void)
{
    /* the examples of RFC 9535, section 1.5, table 2 */

    ASSERT_EQ_INT(_index(_store), 0);

    EXPECT("$.store.book[*].author",
           "[/store/book/0/author][/store/book/1/author]"
           "[/store/book/2/author][/store/book/3/author]");
    EXPECT("$..author",
           "[/store/book/0/author][/store/book/1/author]"
           "[/store/book/2/author][/store/book/3/author]");
    EXPECT("$.store.*", "[/store/bicycle][/store/book]");
    EXPECT("$.store..price",
           "[/store/bicycle/price][/store/book/0/price][/store/book/1/price]"
           "[/store/book/2/price][/store/book/3/price]");
    EXPECT("$..book[2]", "[/store/book/2]");
    EXPECT("$..book[2].author", "[/store/book/2/author]");
    EXPECT("$..book[2].publisher", "");
    EXPECT("$..book[-1]", "[/store/book/3]");
    EXPECT("$..book[0,1]", "[/store/book/0][/store/book/1]");
    EXPECT("$..book[:2]", "[/store/book/0][/store/book/1]");
    EXPECT("$..book[?@.isbn]", "[/store/book/2][/store/book/3]");
    EXPECT("$..book[?@.price<10]", "[/store/book/0][/store/book/2]");
    ASSERT_EQ_INT(_run("$..*", 0), 27);

    /* values come along with the pointers */
    EXPECT_VALUES("$.store.book[0].price", "[/store/book/0/price=8.95]");
    EXPECT_VALUES("$.store.bicycle.color", "[/store/bicycle/color=\"red\"]");
    EXPECT_VALUES("$.store.book", "[/store/book=[4]]");
    EXPECT_VALUES("$.store", "[/store={2}]");
    EXPECT_VALUES("$", "[={1}]");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _descendants(void)
{
    /* the descendant segment visits a node, then each child and its own
       descendants in turn; the members of objects come in the order of
       their names, the members of arrays in the order of their indices */

    ASSERT_EQ_INT(_index(
        "{\"o\": {\"j\": 1, \"k\": 2}, \"a\": [5, 3, [{\"j\": 4}, {\"k\": 6}]]}"
    ), 0);

    EXPECT("$..[*]",
           "[/a][/o][/a/0][/a/1][/a/2][/a/2/0][/a/2/1][/a/2/0/j][/a/2/1/k]"
           "[/o/j][/o/k]");
    EXPECT("$..[1]", "[/a/1][/a/2/1]");
    EXPECT("$..j", "[/a/2/0/j][/o/j]");
    EXPECT("$..[0]", "[/a/0][/a/2/0]");
    EXPECT("$.a..[?@ == 4]", "[/a/2/0/j]");
    EXPECT("$..*.*", "[/a/0][/a/1][/a/2][/o/j][/o/k][/a/2/0][/a/2/1]"
                     "[/a/2/0/j][/a/2/1/k]");

    /* indices are ordered as numbers, not as strings */
    ASSERT_EQ_INT(_index("[[0,1,2,3,4,5,6,7,8,9,10,11],{\"b\":1,\"a\":2}]"),
                  0);
    EXPECT("$[0][*]", "[/0/0][/0/1][/0/2][/0/3][/0/4][/0/5][/0/6][/0/7][/0/8]"
                      "[/0/9][/0/10][/0/11]");
    EXPECT("$..[?@ > 8]", "[/0/9][/0/10][/0/11]");
    EXPECT("$[1].*", "[/1/a][/1/b]");
    EXPECT("$[0][9:]", "[/0/9][/0/10][/0/11]");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _names(void)
{
    /* member names and indices, however they look, are told apart */

    ASSERT_EQ_INT(_index(
        "{\"a\": {\"0\": \"n\", \"1\": [10, 20], \"x~y\": 1, \"p/q\": 2,"
        " \"\": {\"\": 7}, \"\\u00e9\": 3, \"q'\": 4, \"q\\\"\": 5},"
        " \"b\": [[1, 2], [3, [4, 5]], []], \"0\": 6}"
    ), 0);

    EXPECT_VALUES("$.a[\"0\"]", "[/a/0=\"n\"]");
    EXPECT("$.a[0]", "");
    EXPECT("$[\"0\"]", "[/0]");
    EXPECT("$[0]", "");
    EXPECT_VALUES("$.a[\"1\"][0]", "[/a/1/0=10]");
    EXPECT_VALUES("$.a[\"1\"]", "[/a/1=[2]]");
    EXPECT_VALUES("$.a['x~y']", "[/a/x~0y=1]");
    EXPECT_VALUES("$.a['p/q']", "[/a/p~1q=2]");
    EXPECT_VALUES("$.a[\"\"]", "[/a/={1}]");
    EXPECT_VALUES("$.a[\"\"][\"\"]", "[/a//=7]");
    EXPECT_VALUES("$.a.\xc3\xa9", "[/a/\xc3\xa9=3]");
    EXPECT_VALUES("$.a[\"\\u00e9\"]", "[/a/\xc3\xa9=3]");
    EXPECT_VALUES("$.a['q\\'']", "[/a/q'=4]");
    EXPECT_VALUES("$.a[\"q\\\"\"]", "[/a/q\"=5]");
    EXPECT_VALUES("$.b[1][1][0]", "[/b/1/1/0=4]");
    EXPECT_VALUES("$.b[*][*]", "[/b/0/0=1][/b/0/1=2][/b/1/0=3][/b/1/1=[2]]");
    EXPECT_VALUES("$.b[2]", "[/b/2=[0]]");
    EXPECT_VALUES("$.b[-1]", "[/b/2=[0]]");
    EXPECT("$.b[-4]", "");
    EXPECT("$.b[3]", "");
    EXPECT("$.b[\"0\"]", "");
    EXPECT("$.b.*.*", "[/b/0/0][/b/0/1][/b/1/0][/b/1/1]");
    EXPECT("$['a','b']", "[/a][/b]");
    EXPECT("$['b','a','zz','b']", "[/b][/a][/b]");

    /* a run of names and indices is looked up whole, then the kind of
       every parent an index or a name that looks like one stands under
       is checked, through its own leaf, its child's, or its members */
    ASSERT_EQ_INT(_index(
        "{\"a\": [{\"0\": [1]}, [2], {\"0\": 4, \"x\": 5}],"
        " \"o\": {\"0\": {\"0\": 3}, \"1\": [6]}}"
    ), 0);
    EXPECT("$.a[0][\"0\"][0]", "[/a/0/0/0]");
    EXPECT("$.a[0][0][0]", "");
    EXPECT("$.a[0][\"0\"][\"0\"]", "");
    EXPECT("$.a[1][0]", "[/a/1/0]");
    EXPECT("$.a[1][\"0\"]", "");
    EXPECT("$.a[2][\"0\"]", "[/a/2/0]");
    EXPECT("$.a[2][0]", "");
    EXPECT("$.a[*][0]", "[/a/1/0]");
    EXPECT("$.a[*][\"0\"]", "[/a/0/0][/a/2/0]");
    EXPECT("$.a[*][\"0\"][0]", "[/a/0/0/0]");
    EXPECT("$.a[0:3][0]", "[/a/1/0]");
    EXPECT("$.o[\"0\"][\"0\"]", "[/o/0/0]");
    EXPECT("$.o[0][\"0\"]", "");
    EXPECT("$.o[\"0\"][0]", "");
    EXPECT("$.o[\"1\"][0]", "[/o/1/0]");
    EXPECT("$.o[1][0]", "");
    EXPECT("$[\"a\"][0][\"0\"][0]", "[/a/0/0/0]");
    EXPECT("$[\"o\"][\"0\"][\"0\"]", "[/o/0/0]");
    EXPECT("$[\"o\"][0][0]", "");
    EXPECT("$.a[0][\"0\"][1]", "");
    EXPECT("$.a[0][\"x\"]", "");

    ASSERT_EQ_INT(_index(
        "{\"a\": {\"0\": \"n\", \"1\": [10, 20], \"x~y\": 1, \"p/q\": 2,"
        " \"\": {\"\": 7}, \"\\u00e9\": 3, \"q'\": 4, \"q\\\"\": 5},"
        " \"b\": [[1, 2], [3, [4, 5]], []], \"0\": 6}"
    ), 0);

    /* the ambiguity is checked along singular queries too */
    EXPECT("$[?@[\"1\"][0] == 10]", "[/a]");
    EXPECT("$[?@[1][0] == 10]", "");
    EXPECT("$[?@[0][0] == 1]", "[/b]");
    EXPECT("$[?@[\"0\"][0] == 1]", "");
    EXPECT("$[?@[\"0\"] == \"n\"]", "[/a]");
    EXPECT("$[?@[0] == \"n\"]", "");
    EXPECT("$[?@[-1][0] == 1]", "");
    EXPECT("$[?@[-3][0] == 1]", "[/b]");
    EXPECT("$[?@[-4]]", "");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _slices(void)
{
    /* the examples of RFC 9535, section 2.3.4, and the edges */

    ASSERT_EQ_INT(_index("[0, 1, 2, 3, 4, 5, 6]"), 0);

    EXPECT("$[1:3]", "[/1][/2]");
    EXPECT("$[5:]", "[/5][/6]");
    EXPECT("$[1:5:2]", "[/1][/3]");
    EXPECT("$[5:1:-2]", "[/5][/3]");
    EXPECT("$[::-1]", "[/6][/5][/4][/3][/2][/1][/0]");
    EXPECT("$[:]", "[/0][/1][/2][/3][/4][/5][/6]");
    EXPECT("$[::]", "[/0][/1][/2][/3][/4][/5][/6]");
    EXPECT("$[::0]", "");
    EXPECT("$[-2:]", "[/5][/6]");
    EXPECT("$[:-5]", "[/0][/1]");
    EXPECT("$[-100:100]", "[/0][/1][/2][/3][/4][/5][/6]");
    EXPECT("$[100:]", "");
    EXPECT("$[3:3]", "");
    EXPECT("$[3:2]", "");
    EXPECT("$[2:3:-1]", "");
    EXPECT("$[-1:-100:-3]", "[/6][/3][/0]");
    EXPECT("$[ 1 : 3 ]", "[/1][/2]");
    EXPECT("$[1: 3 :1]", "[/1][/2]");
    EXPECT("$[0, 1:3, -1]", "[/0][/1][/2][/6]");

    /* a slice selects nothing from an object */
    ASSERT_EQ_INT(_index("{\"0\": 0, \"1\": 1, \"2\": 2}"), 0);
    EXPECT("$[:]", "");
    EXPECT("$[*]", "[/0][/1][/2]");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _filters(void)
{
    /* comparisons, logical operators, existence, nested queries */

    ASSERT_EQ_INT(_index(
        "{\"nums\": [3, 1, 2, \"x\", null, true, false, [1], {\"a\": 1}, -0.5,"
        " 1e3, \"h\\u00e9llo\\nworld\"],"
        " \"arr\": [{\"v\": [1, 2]}, {\"v\": [1, 2]}, {\"v\": [2, 1]},"
        " {\"v\": {\"0\": 1, \"1\": 2}}, {\"v\": []}, {\"v\": {}},"
        " {\"v\": [[]]}],"
        " \"c\": [{\"k\": 1}, {\"k\": 2}], \"d\": {}, \"e\": [],"
        " \"s\": \"str\"}"
    ), 0);

    /* values of every kind */
    EXPECT("$.nums[?@ > 1]", "[/nums/0][/nums/2][/nums/10]");
    EXPECT("$.nums[?@ < 1]", "[/nums/9]");
    EXPECT("$.nums[?@ >= 3]", "[/nums/0][/nums/10]");
    EXPECT("$.nums[?@ <= 1]", "[/nums/1][/nums/9]");
    EXPECT("$.nums[?@ == 1000]", "[/nums/10]");
    EXPECT("$.nums[?@ == -0.5]", "[/nums/9]");
    EXPECT("$.nums[?@ != -0.5]",
           "[/nums/0][/nums/1][/nums/2][/nums/3][/nums/4][/nums/5][/nums/6]"
           "[/nums/7][/nums/8][/nums/10][/nums/11]");
    EXPECT("$.nums[?@ == \"x\"]", "[/nums/3]");
    EXPECT("$.nums[?@ >= \"x\"]", "[/nums/3]");
    EXPECT("$.nums[?@ > \"x\"]", "");
    EXPECT("$.nums[?@ < \"i\"]", "[/nums/11]");
    EXPECT("$.nums[?@ == \"h\\u00e9llo\\nworld\"]", "[/nums/11]");
    EXPECT("$.nums[?@ == 'h\xc3\xa9llo\\nworld']", "[/nums/11]");
    EXPECT("$.nums[?@ == null]", "[/nums/4]");
    EXPECT("$.nums[?@ == true]", "[/nums/5]");
    EXPECT("$.nums[?@ == false]", "[/nums/6]");
    EXPECT("$.nums[?@ == 1]", "[/nums/1]");
    EXPECT("$.nums[?true == @]", "[/nums/5]");
    EXPECT("$.nums[?1 == @]", "[/nums/1]");

    /* existence */
    ASSERT_EQ_INT(_run("$.nums[?@]", 0), 12);
    EXPECT("$.nums[?@.a]", "[/nums/8]");
    EXPECT("$.nums[?@[0]]", "[/nums/7]");
    EXPECT("$.nums[?@.a == 1 || @[0] == 1]", "[/nums/7][/nums/8]");
    EXPECT("$.nums[?!@.a && !@[0] && @ > 2]", "[/nums/0][/nums/10]");
    EXPECT("$.nums[?@.*]", "[/nums/7][/nums/8]");
    EXPECT("$.nums[?@..a]", "[/nums/8]");
    EXPECT("$[?@.*]", "[/arr][/c][/nums]");
    EXPECT("$[?!@.*]", "[/d][/e][/s]");

    /* Nothing compares equal to Nothing only, and null is a value */
    EXPECT("$.c[?@.missing == @.other]", "[/c/0][/c/1]");
    EXPECT("$.c[?@.missing == null]", "");
    EXPECT("$.c[?@.missing != null]", "[/c/0][/c/1]");
    EXPECT("$.c[?@.missing < 1]", "");
    EXPECT("$.c[?@.k != @.missing]", "[/c/0][/c/1]");
    EXPECT("$.nums[?@ == @.missing]", "");

    /* containers compare deeply */
    EXPECT("$.arr[?@.v == $.arr[0].v]", "[/arr/0][/arr/1]");
    EXPECT("$.arr[?@.v != $.arr[0].v]",
           "[/arr/2][/arr/3][/arr/4][/arr/5][/arr/6]");
    EXPECT("$.arr[?@.v == $.arr[4].v]", "[/arr/4]");
    EXPECT("$.arr[?@.v == $.arr[5].v]", "[/arr/5]");
    EXPECT("$.arr[?@.v == $.arr[6].v]", "[/arr/6]");
    EXPECT("$.arr[?@.v == @.v]",
           "[/arr/0][/arr/1][/arr/2][/arr/3][/arr/4][/arr/5][/arr/6]");
    EXPECT("$.arr[?@.v == 1]", "");
    EXPECT("$.arr[?@.v < $.arr[0].v]", "");
    EXPECT("$[?@ == $.nums[7]]", "");
    EXPECT("$.nums[?@ == $.nums[7]]", "[/nums/7]");
    EXPECT("$.nums[?@ == $.nums[8]]", "[/nums/8]");
    EXPECT("$[?@.k == $.c[0].k]", "");
    EXPECT("$.c[?@ == $.c[0]]", "[/c/0]");

    /* precedence and parentheses */
    EXPECT("$.c[?@.k == 1 || @.k == 2 && @.k == 3]", "[/c/0]");
    EXPECT("$.c[?(@.k == 1 || @.k == 2) && @.k == 2]", "[/c/1]");
    EXPECT("$.c[?!(@.k == 1)]", "[/c/1]");
    EXPECT("$.c[?!(@.k == 1) && (@.k)]", "[/c/1]");
    EXPECT("$.c[?((@.k == 2))]", "[/c/1]");
    EXPECT("$.c[?@.k==1||@.k==2]", "[/c/0][/c/1]");

    /* nested filters and absolute queries: a filter tests the members of
       an object as well as those of an array */
    EXPECT("$.arr[?@.v[?@ == 2]]", "[/arr/0][/arr/1][/arr/2][/arr/3]");
    EXPECT("$.arr[?@.v[?@ == 2] && @.v[0] == 1]", "[/arr/0][/arr/1]");
    EXPECT("$.arr[?@.v[?@ == 2] && @.v[\"0\"] == 1]", "[/arr/3]");
    EXPECT("$.arr[?$.c[?@.k == 2]]",
           "[/arr/0][/arr/1][/arr/2][/arr/3][/arr/4][/arr/5][/arr/6]");
    EXPECT("$.arr[?$.c[?@.k == 3]]", "");
    EXPECT("$.arr[?@.v[?@ == $.c[1].k]]", "[/arr/0][/arr/1][/arr/2][/arr/3]");
    EXPECT("$..[?@.k == 2]", "[/c/1]");
    EXPECT("$.c[?@.k > 1].k", "[/c/1/k]");
    EXPECT("$.c[?@.k == 1 || @.k == 2][?@ > 1]", "[/c/1/k]");
    /* several filters in one segment test the members again, each */
    EXPECT("$.arr[0][?@, ?@]", "[/arr/0/v][/arr/0/v]");
    EXPECT("$.c[?@.k, ?@.k == 2]", "[/c/0][/c/1][/c/1]");
    EXPECT("$.arr[3][?@, ?@ == 2]", "[/arr/3/v]");
    EXPECT("$.arr[3].v[?@, ?@ == 2]", "[/arr/3/v/0][/arr/3/v/1][/arr/3/v/1]");
    EXPECT("$.arr[3].v[?@ == 2, ?@]", "[/arr/3/v/1][/arr/3/v/0][/arr/3/v/1]");
    EXPECT("$.arr[5][?@, ?@]", "[/arr/5/v][/arr/5/v]");
    EXPECT("$.arr[5].v[?@, ?@]", "");
    EXPECT("$[?@.k]", "");

    /* a filter on a scalar or an empty container selects nothing */
    EXPECT("$.s[?@]", "");
    EXPECT("$.d[?@]", "");
    EXPECT("$.e[?@]", "");
    EXPECT("$.s[*]", "");
    EXPECT("$.s[0]", "");
    EXPECT("$.s.s", "");
    EXPECT("$.s..s", "");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _functions(void)
{
    /* length(), count() and value() */

    ASSERT_EQ_INT(_index(
        "{\"s\": \"h\\u00e9llo\", \"e\": \"\", \"n\": 5, \"t\": true,"
        " \"z\": null, \"a\": [1, 2, 3], \"o\": {\"x\": 1, \"y\": 2},"
        " \"d\": {}, \"l\": [], \"c\": [[1], [2]], \"f\": [{\"g\": 1}]}"
    ), 0);

    EXPECT("$[?length(@) == 5]", "[/s]");
    EXPECT("$[?length(@) == 0]", "[/d][/e][/l]");
    EXPECT("$[?length(@) == 3]", "[/a]");
    EXPECT("$[?length(@) == 2]", "[/c][/o]");
    EXPECT("$[?length(@) == 1]", "[/f]");
    EXPECT("$[?length(@) == length(@)]",
           "[/a][/c][/d][/e][/f][/l][/n][/o][/s][/t][/z]");
    EXPECT("$[?length(@.x) == length(@.y)]",
           "[/a][/c][/d][/e][/f][/l][/n][/o][/s][/t][/z]");
    EXPECT("$[?length(@.x) == 0]", "");
    EXPECT("$[?length(\"abc\") == 3]",
           "[/a][/c][/d][/e][/f][/l][/n][/o][/s][/t][/z]");
    EXPECT("$[?length(5) == 1]", "");

    EXPECT("$[?count(@.*) == 2]", "[/c][/o]");
    EXPECT("$[?count(@.*) == 0]",
           "[/d][/e][/l][/n][/s][/t][/z]");
    EXPECT("$[?count(@..*) == 4]", "[/c]");
    EXPECT("$[?count(@[0]) == 1]", "[/a][/c][/f]");
    EXPECT("$[?count(@.x) == count(@.y)]",
           "[/a][/c][/d][/e][/f][/l][/n][/o][/s][/t][/z]");
    EXPECT("$[?count($.*) == 11]",
           "[/a][/c][/d][/e][/f][/l][/n][/o][/s][/t][/z]");
    EXPECT("$[?count($..*) > 11]",
           "[/a][/c][/d][/e][/f][/l][/n][/o][/s][/t][/z]");
    EXPECT("$[?count($[*][*]) == 8]",
           "[/a][/c][/d][/e][/f][/l][/n][/o][/s][/t][/z]");

    EXPECT("$[?value(@..g) == 1]", "[/f]");
    EXPECT("$[?value(@.*) == 1]", "");
    EXPECT("$[?value(@.*) == $.f[0]]", "[/f]");
    EXPECT("$.f[?value(@.*) == 1]", "[/f/0]");
    EXPECT("$[?value(@[*]) == 1]", "");
    EXPECT("$[?value(@.x) == 1]", "[/o]");
    EXPECT("$[?value(@[0]) == $.a[0]]", "[/a]");
    EXPECT("$[?value(@..*) == value(@.*)]", "[/a][/c][/d][/e][/l][/n][/o]"
                                             "[/s][/t][/z]");
    EXPECT("$[?value($.c[*]) == 1]", "");
    EXPECT("$[?value($.a[*]) == 1]", "");
    EXPECT("$[?value($.a[0]) == 1]",
           "[/a][/c][/d][/e][/f][/l][/n][/o][/s][/t][/z]");
    EXPECT("$[?length(value(@.*)) == 1]", "[/f]");
    EXPECT("$[?length(value(@.*)) == 2]", "");
    EXPECT("$[?count(@.*) > 1 && length(@) < 3]", "[/c][/o]");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _regex(void)
{
    /* match() and search() take I-Regexp patterns, through PCRE */

    ASSERT_EQ_INT(_index(
        "{\"a\": \"Nigel Rees\", \"b\": \"Evelyn Waugh\","
        " \"c\": \"h\\u00e9llo\\nworld\", \"d\": \"0-553-21311-3\","
        " \"e\": 1, \"f\": \"1\", \"g\": [\"x\"]}"
    ), 0);

    #ifndef HAS_PCRE
    REJECT("$[?match(@, \"N.*\")]");
    REJECT("$[?search(@, \"N\")]");
    SKIP("the library was built without PCRE");
    #else
    EXPECT("$[?match(@, \"N.*\")]", "[/a]");
    EXPECT("$[?search(@, \"N\")]", "[/a]");
    EXPECT("$[?match(@, \"N\")]", "");
    EXPECT("$[?search(@, \"e\")]", "[/a][/b]");
    EXPECT("$[?search(@, \"^e\")]", "");
    EXPECT("$[?search(@, \"ees$\")]", "[/a]");
    EXPECT("$[?match(@, \"[0-9]-[0-9]{3}-[0-9]{5}-[0-9]\")]", "[/d]");
    EXPECT("$[?match(@, \"\\\\d-\\\\d{3}-\\\\d{5}-\\\\d\")]", "[/d]");
    EXPECT("$[?search(@, \"\\\\d\")]", "[/d][/f]");
    EXPECT("$[?match(@, \"\\\\p{Lu}.*\")]", "[/a][/b]");
    EXPECT("$[?match(@, \"[A-Z].*\")]", "[/a][/b]");
    EXPECT("$[?match(@, \"[^A-Z].*\")]", "[/d][/f]");
    EXPECT("$[?search(@, \"[^A-Z].*\")]", "[/a][/b][/c][/d][/f]");
    EXPECT("$[?match(@, \"h\\u00e9llo\\\\nworld\")]", "[/c]");
    EXPECT("$[?match(@, \"h\\\\wllo\\\\sworld\")]", "[/c]");
    EXPECT("$[?match(@, \"h.llo\\\\nworld\")]", "[/c]");
    EXPECT("$[?match(@, \"h.llo.world\")]", "");
    EXPECT("$[?search(@, \"o.w\")]", "");
    EXPECT("$[?search(@, \"o\\\\nw\")]", "[/c]");
    EXPECT("$[?match(@, \"h[^l]llo\\\\sworld\")]", "[/c]");
    EXPECT("$[?match(@, \"h[^\\\\]]llo\\\\sworld\")]", "[/c]");
    EXPECT("$[?match(@, \"h[]l]llo\\\\sworld\")]", "");
    EXPECT("$[?match(@, \"(?:h|x)\\u00e9llo\\\\nworld\")]", "[/c]");
    EXPECT("$[?match(@, \"h\\u00e9llo\\\\nworld|1\")]", "[/c][/f]");

    /* patterns need not be constant, non-strings never match */
    EXPECT("$[?match(@, @)]", "[/a][/b][/c][/d][/f]");
    EXPECT("$[?match(@, $.f)]", "[/f]");
    EXPECT("$[?search(@, $.e)]", "");
    EXPECT("$[?match(@, $.g)]", "");
    EXPECT("$[?match(@.x, \"1\")]", "");
    EXPECT("$[?match(1, \"1\")]", "");
    EXPECT("$[?!match(@, \"1\")]", "[/a][/b][/c][/d][/e][/g]");
    EXPECT("$[?match(@, \"1\") || search(@, \"Waugh\")]", "[/b][/f]");

    /* a bad pattern is a rejected query, or a failed match at run time */
    REJECT("$[?match(@, \"(\")]");
    REJECT("$[?search(@, \"[\")]");
    EXPECT("$[?match(@, $.d)]", "[/d]");
    ASSERT_EQ_INT(_index("[\"(\", \"a\"]"), 0);
    EXPECT("$[?match(@, $[0])]", "");
    EXPECT("$[?match(@, $[1])]", "[/1]");

    return 0;
    #endif
}

/* -------------------------------------------------------------------------- */

static int _roots(void)
{
    /* scalar roots, empty roots, roots and members made of containers */

    ASSERT_EQ_INT(_index("\"hello\""), 0);
    EXPECT_VALUES("$", "[=\"hello\"]");
    EXPECT("$.a", "");
    EXPECT("$[0]", "");
    EXPECT("$[*]", "");
    EXPECT("$..*", "");
    EXPECT("$[?@]", "");
    EXPECT("$[?$ == \"hello\"]", "");

    /* a top-level number is only complete once something follows it */
    ASSERT_EQ_INT(_index("42 "), 0);
    EXPECT_VALUES("$", "[=42]");
    ASSERT_EQ_INT(_index("null"), 0);
    EXPECT_VALUES("$", "[=null]");
    ASSERT_EQ_INT(_index("false"), 0);
    EXPECT_VALUES("$", "[=false]");

    ASSERT_EQ_INT(_index("{}"), 0);
    EXPECT_VALUES("$", "[={0}]");
    EXPECT("$.*", "");
    EXPECT("$..*", "");
    ASSERT_EQ_INT(_index("[]"), 0);
    EXPECT_VALUES("$", "[=[0]]");
    EXPECT("$[0]", "");
    EXPECT("$[-1]", "");
    EXPECT("$[:]", "");

    /* containers holding containers only are typed by the index */
    ASSERT_EQ_INT(_index("[[[]]]"), 0);
    EXPECT_VALUES("$", "[=[1]]");
    EXPECT_VALUES("$[0]", "[/0=[1]]");
    EXPECT_VALUES("$[0][0]", "[/0/0=[0]]");
    EXPECT("$[0][0][0]", "");
    EXPECT("$[\"0\"]", "");
    EXPECT("$..*", "[/0][/0/0]");
    EXPECT("$..[0]", "[/0][/0/0]");
    EXPECT("$[?@[0]]", "[/0]");
    EXPECT("$[?@ == $[0]]", "[/0]");
    EXPECT("$[?@[0] == $[0][0]]", "[/0]");
    EXPECT("$[?length(@) == 1]", "[/0]");
    EXPECT("$[?length(@[0]) == 0]", "[/0]");

    ASSERT_EQ_INT(_index("{\"a\": {\"b\": {\"c\": [{\"d\": [1]}]}}}"), 0);
    EXPECT_VALUES("$.a", "[/a={1}]");
    EXPECT_VALUES("$.a.b", "[/a/b={1}]");
    EXPECT_VALUES("$.a.b.c", "[/a/b/c=[1]]");
    EXPECT_VALUES("$.a.b.c[0]", "[/a/b/c/0={1}]");
    EXPECT_VALUES("$.a.b.c[0].d", "[/a/b/c/0/d=[1]]");
    EXPECT_VALUES("$.a.b.c[0].d[0]", "[/a/b/c/0/d/0=1]");
    EXPECT("$.a.b[0]", "");
    EXPECT("$.a.b.c.d", "");
    EXPECT("$.a.b.c[\"0\"]", "");
    EXPECT("$..d", "[/a/b/c/0/d]");
    EXPECT("$..[0]", "[/a/b/c/0][/a/b/c/0/d/0]");
    EXPECT("$[?@.b.c[0].d[0] == 1]", "[/a]");
    EXPECT("$[?@.b.c[\"0\"].d[0] == 1]", "");
    EXPECT("$[?@.b.c[0].d[\"0\"] == 1]", "");
    EXPECT("$..[?@.d]", "[/a/b/c/0]");
    EXPECT("$..[?count(@.*) == 1]", "[/a][/a/b][/a/b/c][/a/b/c/0]"
                                    "[/a/b/c/0/d]");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _syntax(void)
{
    /* well formed and malformed queries */

    ACCEPT("$");
    ACCEPT("$ .a");
    ACCEPT("$.a .b");
    ACCEPT("$.a\t[0]");
    ACCEPT("$[ 'a' , 'b' ]");
    ACCEPT("$[?@.a==1&&@.b==2||@.c==3]");
    ACCEPT("$[? @.a == 1 ]");
    ACCEPT("$[?(@.a)]");
    ACCEPT("$[?!(@.a)]");
    ACCEPT("$[?! @.a]");
    ACCEPT("$[?@.a == -0]");
    ACCEPT("$[?@.a == 1.5e-3]");
    ACCEPT("$[?@.a == 1E3]");
    ACCEPT("$[?@.a == \"\\u00e9\"]");
    ACCEPT("$[?@.a == \"\\ud83d\\ude00\"]");
    ACCEPT("$[?@.a == '\\/\\\\\\b\\f\\n\\r\\t']");
    ACCEPT("$[\"a'b\"]");
    ACCEPT("$['a\"b']");
    ACCEPT("$[9007199254740991]");
    ACCEPT("$[-9007199254740991]");
    ACCEPT("$[?count(@.a) == 1]");
    ACCEPT("$[?value(@.a) == 1]");
    ACCEPT("$[?length(@.a) == 1]");
    ACCEPT("$[?length(\"a\") == 1]");
    ACCEPT("$[?length(count(@.*)) == 1]");
    ACCEPT("$.\xc3\xa9\xe2\x82\xac_9");
    ACCEPT("$.a..b..[0]..*");
    ACCEPT("$[0:1:]");
    ACCEPT("$[:]");
    ACCEPT("$[ : ]");

    REJECT("");
    REJECT("$.");
    REJECT("$..");
    REJECT("$.a..");
    REJECT(" $");
    REJECT("$ ");
    REJECT("$.a ");
    REJECT("$. a");
    REJECT("$.. a");
    REJECT("a");
    REJECT("$$");
    REJECT("$[");
    REJECT("$[0");
    REJECT("$[]");
    REJECT("$[0,]");
    REJECT("$[,0]");
    REJECT("$.1");
    REJECT("$.-a");
    REJECT("$[01]");
    REJECT("$[-0]");
    REJECT("$[1.5]");
    REJECT("$[9007199254740992]");
    REJECT("$[-9007199254740992]");
    REJECT("$[0:1:2:3]");
    REJECT("$['a\"]");
    REJECT("$[\"a\\'b\"]");
    REJECT("$['a\\\"b']");
    REJECT("$[\"a\\x\"]");
    REJECT("$[\"a\\u12\"]");
    REJECT("$[\"a\\ud83d\"]");
    REJECT("$[\"\\ude00\"]");
    REJECT("$[\"\t\"]");
    REJECT("$[\"\n\"]");
    REJECT("$[\"a]");
    REJECT("$[?]");
    REJECT("$[?@.a == ]");
    REJECT("$[?== @.a]");
    REJECT("$[?@.a ==]");
    REJECT("$[?@.a = 1]");
    REJECT("$[?@.a === 1]");
    REJECT("$[?@.a =! 1]");
    REJECT("$[?@.a & @.b]");
    REJECT("$[?@.a | @.b]");
    REJECT("$[?@.a &&]");
    REJECT("$[?&& @.a]");
    REJECT("$[?@.a == 1 == true]");
    REJECT("$[?@.a == 1 < 2]");
    REJECT("$[?!@.a == 1]");
    REJECT("$[?@.a]]");
    REJECT("$[?(@.a]");
    REJECT("$[?@.a == 1)]");
    REJECT("$[?()]");
    REJECT("$[?(1)]");
    REJECT("$[?(@.a == 1) == true]");
    REJECT("$[?1]");
    REJECT("$[?\"a\"]");
    REJECT("$[?@.* == 1]");
    REJECT("$[?@..a == 1]");
    REJECT("$[?@[*] == 1]");
    REJECT("$[?@[0:1] == 1]");
    REJECT("$[?@[0,1] == 1]");
    REJECT("$[?@[?@] == 1]");
    REJECT("$[?length(@.*)]");
    REJECT("$[?length(@.*) == 1]");
    REJECT("$[?length(@.a)]");
    REJECT("$[?count(@.a)]");
    REJECT("$[?value(@.a)]");
    REJECT("$[?length()]");
    REJECT("$[?length(@.a, 1)]");
    REJECT("$[?length(@.a,)]");
    REJECT("$[?length(1 == 1)]");
    REJECT("$[?count(1)]");
    REJECT("$[?count(@.a == 1)]");
    REJECT("$[?value(\"a\")]");
    REJECT("$[?foo(@.a)]");
    REJECT("$[?Length(@.a)]");
    REJECT("$[?length (@.a)]");
    REJECT("$[?@.a == 01]");
    REJECT("$[?@.a == .5]");
    REJECT("$[?@.a == 1.]");
    REJECT("$[?@.a == 1e]");
    REJECT("$[?@.a == +1]");
    REJECT("$[?@.a == True]");
    REJECT("$[?@.a == nil]");
    REJECT("$[?@.a == \"a\" \"b\"]");
    ACCEPT("$[?@ .a]");
    ACCEPT("$[?@ [0]]");
    REJECT("$[?@. a]");
    REJECT("$[?@a]");
    REJECT("$[?$$]");
    /* a query is not checked to be UTF-8: broken names match nothing */
    ACCEPT("$.a\xff");
    ACCEPT("$['\xc0\x80']");
    ACCEPT("$['\xed\xa0\x80']");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _early_stop(const char *ptr, size_t len, void *arg)
{
    int *n = arg;

    (void) ptr; (void) len;

    return (++ (*n) == 2);
}

static int _api(void)
{
    /* parameters, stopping a query, reusing a query */

    JSONPath_Query *q = NULL;
    int n = 0, i = 0;

    ASSERT_EQ_INT(_index(_store), 0);
    ASSERT_NOT_NULL(q = jsonpath_query_alloc("$..price", 8));

    ASSERT_NULL(jsonpath_query_alloc(NULL, 1));
    ASSERT_EQ_INT(jsonpath_foreach(NULL, q, _early_stop, & n), -1);
    ASSERT_EQ_INT(jsonpath_foreach(& _ctx, NULL, _early_stop, & n), -1);
    ASSERT_EQ_INT(jsonpath_foreach(& _ctx, q, NULL, & n), -1);
    ASSERT_NULL(jsonpath_query_free(NULL));

    /* the callback stops the query after the second node */
    ASSERT_EQ_INT(jsonpath_foreach(& _ctx, q, _early_stop, & n), 2);
    ASSERT_EQ_INT(n, 2);

    /* a query runs again and again */
    for (i = 0; i < 3; i ++) {
        n = 0;
        ASSERT_EQ_INT(jsonpath_foreach(& _ctx, q, _collect, & n), 5);
    }

    ASSERT_NULL(jsonpath_query_free(q));

    /* the same query on another document */
    ASSERT_EQ_INT(_index("{\"price\": 1, \"x\": {\"price\": 2}}"), 0);
    EXPECT("$..price", "[/price][/x/price]");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _same_value(JSON_Value a, JSON_Value b)
{
    if (a.type != b.type || a.len != b.len) return 0;

    switch (a.type) {
    case JSON_VALUE_NULL:
    case JSON_VALUE_ARRAY:
    case JSON_VALUE_OBJECT:
        return 1;

    case JSON_VALUE_BOOLEAN:
    case JSON_VALUE_INTEGER:
        return a.value.integer == b.value.integer;

    case JSON_VALUE_DECIMAL:
        return a.value.decimal == b.value.decimal;

    case JSON_VALUE_STRING:
        return ! a.len ||
               ! memcmp(a.value.pointer, b.value.pointer, a.len);

    default:
        return 0;
    }
}

/* -------------------------------------------------------------------------- */

static int _iterator(void)
{
    /* a lazy evaluation yields the nodes of the callback one, in order,
       with their values; stopping early and bad parameters */

    static const char *queries[] = {
        "$..price", "$.store.book[*].author", "$..*", "$..[?@.price < 10]",
        "$.store.book[::-1].title", "$..book[0]", "$.store.book[-1]", "$",
        "$.nothing", "$..[?@.isbn].price", "$.store[?@.color].price"
    };
    JSONPath_Query *q = NULL;
    JSONPath_Iterator *it = NULL;
    JSON_Value v = { 0 };
    char expect[sizeof(_buf)];
    unsigned int i = 0;
    int n = 0, zero = 0;

    ASSERT_EQ_INT(_index(_store), 0);

    for (i = 0; i < sizeof(queries) / sizeof(*queries); i ++) {
        ASSERT_TRUE((n = _run(queries[i], 0)) >= 0);
        memcpy(expect, _buf, sizeof(expect));
        _buf[0] = '\0';
        ASSERT_NOT_NULL(q = jsonpath_query_alloc(queries[i],
                                                 strlen(queries[i])));
        ASSERT_NOT_NULL(it = jsonpath_each(& _ctx, q));
        while ( (it = jsonpath_next(it)) ) {
            ASSERT_EQ_UINT(strlen(it->key), it->len);

            v = jsonpath_get(&_ctx, it->key, it->len);
            ASSERT_TRUE(_same_value(v, it->val));

            ASSERT_EQ_INT(_collect(it->key, it->len, & zero), 0);
            n --;
        }
        ASSERT_EQ_INT(n, 0);
        ASSERT_TRUE(! strcmp(_buf, expect));
        ASSERT_NULL(jsonpath_query_free(q));
    }

    /* an iteration stopped early */
    ASSERT_NOT_NULL(q = jsonpath_query_alloc("$..*", 4));
    ASSERT_NOT_NULL(it = jsonpath_each(& _ctx, q));
    ASSERT_NOT_NULL(it = jsonpath_next(it));
    ASSERT_TRUE(! strcmp(it->key, "/store"));
    ASSERT_NOT_NULL(it = jsonpath_next(it));
    ASSERT_TRUE(! strcmp(it->key, "/store/bicycle"));
    ASSERT_NULL(jsonpath_break(it));

    /* parameters */
    ASSERT_NULL(jsonpath_each(NULL, q));
    ASSERT_NULL(jsonpath_each(& _ctx, NULL));
    ASSERT_NULL(jsonpath_next(NULL));
    ASSERT_NULL(jsonpath_break(NULL));
    ASSERT_NULL(jsonpath_query_free(q));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _double(const char *ptr, size_t len, void *arg)
{
    /** @brief replace the number delivered by its double */

    JSON_Value v = jsonpath_get(&_ctx, ptr, len);
    double n = 0;

    (void) arg;

    if (v.type == JSON_VALUE_INTEGER)
        n = (double) (int64_t) v.value.integer;
    else if (v.type == JSON_VALUE_DECIMAL)
        n = v.value.decimal;
    else return -1;

    return jsonpath_set_decimal(&_ctx, ptr, len, n * 2);
}

/* -------------------------------------------------------------------------- */

static char *_owned_string(const char *s, size_t len)
{
    char *copy = NULL;

    if (! (copy = malloc(len + 1)) ) return NULL;

    memcpy(copy, s, len);
    copy[len] = '\0';

    return copy;
}

/* -------------------------------------------------------------------------- */

static int _set(void)
{
    /* values replaced in place, by pointer and through queries */

    JSONPath_Query *q = NULL;
    JSONPath_Iterator *it = NULL;
    JSON_Value v = { 0 };
    char *s = NULL;
    double sum = 0;
    int n = 0;

    ASSERT_EQ_INT(_index(_store), 0);

    /* every scalar kind, the type free to change, the place kept */
    ASSERT_NOT_NULL(s = _owned_string("blue", 4));
    ASSERT_EQ_INT(json_pointer_set_string(
        &_ctx, "/store/bicycle/color", 20, s, 4
    ), 0);
    s = NULL;

    EXPECT_VALUES(
        "$.store.bicycle.color",
        "[/store/bicycle/color=\"blue\"]"
    );

    ASSERT_EQ_INT(json_pointer_set_null(
        &_ctx, "/store/bicycle/color", 20
    ), 0);

    EXPECT_VALUES(
        "$.store.bicycle.color",
        "[/store/bicycle/color=null]"
    );

    ASSERT_EQ_INT(json_pointer_set_boolean(
        &_ctx, "/store/bicycle/color", 20, 1
    ), 0);

    EXPECT_VALUES(
        "$.store[?@.color == true]",
        "[/store/bicycle={2}]"
    );

    ASSERT_NOT_NULL(s = _owned_string("a much longer colour name", 25));
    ASSERT_EQ_INT(json_pointer_set_string(
        &_ctx, "/store/bicycle/color", 20, s, 25
    ), 0);
    s = NULL;

    EXPECT_VALUES(
        "$.store.bicycle.color",
        "[/store/bicycle/color=\"a much longer colour name\"]"
    );

    v = json_pointer_get(&_ctx, "/store/bicycle/color", 20);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_STRING);
    ASSERT_EQ_UINT(v.len, 25);

    /*
     * A value copied from another node: the generic setter copies its
     * string storage.
     */
    v = json_pointer_get(&_ctx, "/store/book/1/title", 19);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_STRING);

    ASSERT_EQ_INT(json_pointer_set(
        &_ctx, "/store/book/0/title", 19, v
    ), 0);

    EXPECT(
        "$.store.book[?@.title == \"Sword of Honour\"]",
        "[/store/book/0][/store/book/1]"
    );

    /* replace from under an iterator */
    ASSERT_NOT_NULL(q = jsonpath_query_alloc(
        "$..[?@.category == 'fiction'].category", 38
    ));

    ASSERT_NOT_NULL(it = jsonpath_each(&_ctx, q));

    while ( (it = jsonpath_next(it)) ) {
        ASSERT_NOT_NULL(s = _owned_string("novel", 5));

        if (jsonpath_set_string(&_ctx, it->key, it->len, s, 5) == -1) {
            free(s);
            jsonpath_break(it);
            return -1;
        }

        s = NULL;
        n ++;
    }

    ASSERT_EQ_INT(n, 3);
    ASSERT_NULL(jsonpath_query_free(q));

    ASSERT_EQ_INT(_run("$..[?@.category == 'fiction']", 0), 0);
    ASSERT_EQ_INT(_run("$..[?@.category == 'novel']", 0), 3);

    #ifdef HAS_PCRE
    ASSERT_EQ_INT(_run("$..[?match(@.category, 'nov.*')]", 0), 3);
    #endif

    /* numbers doubled by a query, read back by another */
    ASSERT_NOT_NULL(q = jsonpath_query_alloc("$..price", 8));
    ASSERT_EQ_INT(jsonpath_foreach(&_ctx, q, _double, NULL), 5);
    ASSERT_NOT_NULL(it = jsonpath_each(&_ctx, q));

    while ( (it = jsonpath_next(it)) )
        sum += it->val.value.decimal;

    ASSERT_WITHIN(sum, 2 * (8.95 + 12.99 + 8.99 + 22.99 + 399), 1e-9);
    ASSERT_EQ_INT(_run("$..[?@.price > 40]", 0), 2);
    ASSERT_NULL(jsonpath_query_free(q));

    /* no container, no missing node, no container as a value */
    ASSERT_EQ_INT(json_pointer_set_null(&_ctx, "/store", 6), -1);
    ASSERT_EQ_INT(json_pointer_set_null(&_ctx, "/store/book", 11), -1);
    ASSERT_EQ_INT(json_pointer_set_null(&_ctx, "/nothing", 8), -1);
    ASSERT_EQ_INT(json_pointer_set_null(&_ctx, "", 0), -1);

    v = json_pointer_get(&_ctx, "/store/book", 11);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_ARRAY);
    ASSERT_EQ_UINT(v.len, 4);

    ASSERT_EQ_INT(json_pointer_set(
        &_ctx, "/store/book/0/price", 19, v
    ), -1);

    ASSERT_EQ_INT(json_pointer_set_null(NULL, "/store", 6), -1);
    ASSERT_EQ_INT(json_pointer_set_null(&_ctx, NULL, 6), -1);

    /* a scalar document, replaced twice */
    ASSERT_EQ_INT(_index("\"hello\""), 0);

    ASSERT_EQ_INT(json_pointer_set_decimal(&_ctx, "", 0, 42), 0);

    v = json_pointer_get(&_ctx, "", 0);
    ASSERT_EQ_UINT(v.type, JSON_VALUE_DECIMAL);
    ASSERT_TRUE(v.value.decimal == 42);

    ASSERT_EQ_INT(_run("$[?@ == 42]", 0), 0);

    ASSERT_NOT_NULL(s = _owned_string("world", 5));
    ASSERT_EQ_INT(json_pointer_set_string(&_ctx, "", 0, s, 5), 0);
    s = NULL;

    EXPECT_VALUES("$", "[=\"world\"]");

    return 0;
}

/* -------------------------------------------------------------------------- */

static void *_reader(void *arg)
{
    /** @brief read the strings of the document, over and over */

    JSONPath_Iterator *it = NULL;
    unsigned int i = 0;
    long bad = 0;

    for (i = 0; i < 100; i ++) {
        if (! (it = jsonpath_each(&_ctx, arg)) ) return (void *) 1;

        while ( (it = jsonpath_next(it)) ) {
            if (it->val.type != JSON_VALUE_STRING ||
                strlen((const char *) it->val.value.pointer) != it->val.len)
                bad ++;
        }
    }

    return (void *) bad;
}

/* -------------------------------------------------------------------------- */

static void *_replacer(void *arg)
{
    /** @brief replace the strings of the document, from under a query */

    JSONPath_Iterator *it = NULL;
    char *s = NULL;
    char key[32], value[64];
    size_t len = 0;
    unsigned int i = 0;
    long bad = 0;

    for (i = 0; i < 1000; i ++) {
        snprintf(key, sizeof(key), "/a/%u/s", i % 60);
        snprintf(value, sizeof(value), "value %u%s", i,
                 (i & 1) ? " of the longer kind" : "");

        len = strlen(value);
        if (! (s = _owned_string(value, len)) )
            return (void *) 1;

        if (! (it = jsonpath_each(&_ctx, arg)) ) {
            free(s);
            return (void *) 1;
        }

        if (! (it = jsonpath_next(it)) ) {
            free(s);
            return (void *) 1;
        }

        if (jsonpath_set_string(&_ctx, key, strlen(key), s, len) == -1) {
            free(s);
            bad ++;
        } else {
            s = NULL;
        }

        jsonpath_break(it);
    }

    return (void *) bad;
}

/* -------------------------------------------------------------------------- */

static int _threads(void)
{
    /* a query holds the index for reading: a replacement from another
       thread waits for it, and no query sees a torn or freed value */

    JSONPath_Query *qr = NULL, *qw = NULL;
    pthread_t r, w;
    void *rr = NULL, *wr = NULL;
    char doc[4096];
    size_t len = 0;
    unsigned int i = 0;

    len = snprintf(doc, sizeof(doc), "{\"a\": [");
    for (i = 0; i < 60; i ++)
        len += snprintf(doc + len, sizeof(doc) - len, "%s{\"s\": \"v%u\"}",
                        i ? ", " : "", i);
    len += snprintf(doc + len, sizeof(doc) - len, "]}");
    ASSERT_EQ_INT(_index(doc), 0);
    ASSERT_NOT_NULL(qr = jsonpath_query_alloc("$.a[*].s", 8));
    ASSERT_NOT_NULL(qw = jsonpath_query_alloc("$", 1));

    ASSERT_EQ_INT(pthread_create(& r, NULL, _reader, qr), 0);
    ASSERT_EQ_INT(pthread_create(& w, NULL, _replacer, qw), 0);
    pthread_join(r, & rr);
    pthread_join(w, & wr);
    ASSERT_EQ_INT((int) (intptr_t) rr, 0);
    ASSERT_EQ_INT((int) (intptr_t) wr, 0);

    ASSERT_EQ_INT(_run("$.a[*].s", 0), 60);
    ASSERT_EQ_INT(_run("$.a[?@.s == 'value 999 of the longer kind']", 0), 1);
    ASSERT_NULL(jsonpath_query_free(qr));
    ASSERT_NULL(jsonpath_query_free(qw));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _wide(void)
{
    /* arrays past ten members keep their order through every selector */

    char doc[8192];
    size_t len = 0;
    unsigned int i = 0;

    len = snprintf(doc, sizeof(doc), "{\"a\": [");
    for (i = 0; i < 120; i ++)
        len += snprintf(doc + len, sizeof(doc) - len, "%s{\"i\": %u}",
                        i ? ", " : "", i);
    len += snprintf(doc + len, sizeof(doc) - len, "]}");
    ASSERT_EQ_INT(_index(doc), 0);

    ASSERT_EQ_INT(_run("$.a[*].i", 0), 120);
    ASSERT_EQ_INT(_run("$..i", 0), 120);
    EXPECT("$.a[8:12].i", "[/a/8/i][/a/9/i][/a/10/i][/a/11/i]");
    EXPECT("$.a[-3:].i", "[/a/117/i][/a/118/i][/a/119/i]");
    EXPECT("$.a[?@.i > 117].i", "[/a/118/i][/a/119/i]");
    EXPECT("$.a[?@.i == 100]", "[/a/100]");
    EXPECT("$.a[100, 99, 9]", "[/a/100][/a/99][/a/9]");
    EXPECT("$.a[100:97:-1].i", "[/a/100/i][/a/99/i][/a/98/i]");
    EXPECT("$[?count(@[*]) == 120]", "[/a]");
    EXPECT("$[?length(@) == 120]", "[/a]");
    EXPECT("$[?value(@[119].i) == 119]", "[/a]");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _scan(void)
{
    /* a descendant segment selecting a member name reads the leaves under
       the node instead of walking it: every bearer of the name, leaf or
       container, once, in document order */

    ASSERT_EQ_INT(_index(
        "{\"name\": 1, \"a\": {\"name\": {\"x\": {\"name\": 2}}, \"name-x\": 3,"
        " \"zz\": [{\"name\": 4}, {\"q\": 5}, {\"name\": {}},"
        " {\"name\": [[]]}]},"
        " \"b\": [0,1,2,3,4,5,6,7,8,9,{\"name\": 10},{\"name\": 11}],"
        " \"0\": {\"name\": 12}, \"name-y\": {\"name\": 13}, \"s\": \"name\"}"
    ), 0);

    /* the root is visited before its descendants: its own member first */
    EXPECT_VALUES("$..name",
                  "[/name=1][/0/name=12][/a/name={1}][/a/name/x/name=2]"
                  "[/a/zz/0/name=4][/a/zz/2/name={0}][/a/zz/3/name=[1]]"
                  "[/b/10/name=10][/b/11/name=11][/name-y/name=13]");
    EXPECT("$..[\"name\"]",
           "[/name][/0/name][/a/name][/a/name/x/name][/a/zz/0/name]"
           "[/a/zz/2/name][/a/zz/3/name][/b/10/name][/b/11/name]"
           "[/name-y/name]");
    EXPECT("$.a..name", "[/a/name][/a/name/x/name][/a/zz/0/name]"
                        "[/a/zz/2/name][/a/zz/3/name]");
    EXPECT("$.b..name", "[/b/10/name][/b/11/name]");
    EXPECT("$.s..name", "");
    EXPECT("$..name.x", "[/a/name/x]");
    EXPECT("$..x..name", "[/a/name/x/name]");
    EXPECT("$..name[0]", "[/a/zz/3/name/0]");
    EXPECT("$..q", "[/a/zz/1/q]");
    EXPECT("$..zz[?@.name].name", "[/a/zz/0/name][/a/zz/2/name]"
                                  "[/a/zz/3/name]");
    EXPECT("$[?count(@..name) == 5]", "[/a]");
    EXPECT("$[?count(@..name) == 2]", "[/b]");
    EXPECT("$[?@..name]", "[/0][/a][/b][/name-y]");
    EXPECT("$[?value(@..name) == 13]", "[/name-y]");
    EXPECT("$..[?@.name == 12]", "[/0]");

    /* a member name that could be an index takes the general walk */
    EXPECT("$..[\"0\"]", "[/0]");
    EXPECT("$..[0]", "[/a/zz/0][/a/zz/3/name/0][/b/0]");

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _tally(const char *ptr, size_t len, void *arg)
{
    (void) ptr; (void) len;
    (* (int *) arg) ++;

    return 0;
}

static int _huge(void)
{
    /* pointers of 60K, near the limit of a key: the work arena grows
       under the lookups of a segment, whose later selectors must not keep
       a pointer into the old one; a filter puts the segment on the pass */

    static char names[60001], doc[70000], expr[40000];
    JSONPath_Query *q = NULL;
    size_t len = 0;
    int n = 0;

    memset(names, 'a', 30000); memset(names + 30000, 'b', 30000);
    names[60000] = '\0';
    snprintf(doc, sizeof(doc), "{\"%.30000s\": {\"%.30000s\": [10, 20, 30]}}",
             names, names + 30000);
    ASSERT_EQ_INT(_index(doc), 0);

    /* root: its member; the outer object: its member, twice; the array:
       three members, the last, the first two */
    len = snprintf(expr, sizeof(expr), "$..[?@, \"%s\", -1, 0:2]",
                   names + 30000);
    ASSERT_NOT_NULL(q = jsonpath_query_alloc(expr, len));
    ASSERT_EQ_INT(jsonpath_foreach(& _ctx, q, _tally, & n), 9);
    ASSERT_EQ_INT(n, 9);
    jsonpath_query_free(q);

    /* the same through the walk, then a name run folded in one lookup */
    n = 0;
    len = snprintf(expr, sizeof(expr), "$..[*, \"%s\", -1, 0:2]",
                   names + 30000);
    ASSERT_NOT_NULL(q = jsonpath_query_alloc(expr, len));
    ASSERT_EQ_INT(jsonpath_foreach(& _ctx, q, _tally, & n), 9);
    jsonpath_query_free(q);
    n = 0;
    len = snprintf(expr, sizeof(expr), "$[*].%s[*]", names + 30000);
    ASSERT_NOT_NULL(q = jsonpath_query_alloc(expr, len));
    ASSERT_EQ_INT(jsonpath_foreach(& _ctx, q, _tally, & n), 3);
    jsonpath_query_free(q);

    return 0;
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("bookstore", _bookstore),
    TEST("huge", _huge),
    TEST("scan", _scan),
    TEST("descendants", _descendants),
    TEST("names", _names),
    TEST("slices", _slices),
    TEST("filters", _filters),
    TEST("functions", _functions),
    TEST("regex", _regex),
    TEST("roots", _roots),
    TEST("syntax", _syntax),
    TEST("api", _api),
    TEST("iterator", _iterator),
    TEST("set", _set),
    TEST("threads", _threads),
    TEST("wide", _wide)
};

TEST_SUITE(test_suite_jsonpath, "jsonpath", NULL, _teardown, _cases);

/* -------------------------------------------------------------------------- */
#endif /* _ENABLE_JSON && _ENABLE_TRIE */
/* -------------------------------------------------------------------------- */
