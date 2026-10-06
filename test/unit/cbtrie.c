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
#ifdef _ENABLE_TRIE
/* -------------------------------------------------------------------------- */

#include "../askl_test.h"
#include "../../lib/askl_cbtrie.h"

#define KEYS 10000
#define KEYS_SLOW 800000

/* four keys shaped as two pairs: the root's children are both inner nodes */
static const char *_pairs[] = { "aa", "ab", "ba", "bb" };

/* a collected key */
typedef struct _Entry {
    char key[32];
    size_t len;
    uint64_t val;
} _Entry;

static unsigned int _freed = 0;

/* -------------------------------------------------------------------------- */

static void _count_free(UNUSED Variant v)
{
    _freed ++;
}

/* -------------------------------------------------------------------------- */

static int _delete_all(UNUSED const char *k, UNUSED size_t l, UNUSED Variant v)
{
    return -1;
}

/* -------------------------------------------------------------------------- */

static int _delete_a(const char *k, UNUSED size_t l, UNUSED Variant v)
{
    return (k[0] == 'a') ? -1 : 0;
}

/* -------------------------------------------------------------------------- */

static int _is_odd(UNUSED const char *k, UNUSED size_t l, Variant v)
{
    return variant_to_integer(v) & 1;
}

/* -------------------------------------------------------------------------- */

static int _compare(const void *a, const void *b)
{
    /** @brief lexicographic byte order, a prefix sorts first */

    const _Entry *x = a, *y = b;
    size_t n = (x->len < y->len) ? x->len : y->len;
    int ret = memcmp(x->key, y->key, n);

    return ret ? ret : (int) x->len - (int) y->len;
}

/* -------------------------------------------------------------------------- */

static int _collect(Trie *t, _Entry *out, unsigned int max)
{
    /** @brief copy every entry in traversal order, -1 if there are too many */

    Trie_Iterator *it = NULL;
    unsigned int n = 0;

    for (it = trie_each(t); it; it = trie_next(it)) {
        if (n == max || it->len >= sizeof(out->key)) {
            trie_break(it);
            return -1;
        }
        memcpy(out[n].key, it->key, it->len);
        out[n].len = it->len;
        out[n].val = variant_to_integer(it->val);
        n ++;
    }

    return n;
}

/* -------------------------------------------------------------------------- */

static int _sorted(const _Entry *e, unsigned int n)
{
    unsigned int i = 0;

    for (i = 1; i < n; i ++) if (_compare(& e[i - 1], & e[i]) >= 0) return 0;

    return 1;
}

/* -------------------------------------------------------------------------- */

static Trie_Leaf *_leaf(const char *key, size_t len, uint64_t value)
{
    /** @brief allocate a leaf for a batch, as the JSON parser does */

    Trie_Leaf *leaf = NULL;

    if (! (leaf = malloc(sizeof(*leaf) + len + 1)) ) return NULL;

    memcpy(leaf->key, key, len);
    leaf->key[len] = '\0';
    leaf->len = len; leaf->own = 0;
    leaf->val = variant_from_integer(value);

    return leaf;
}

/* -------------------------------------------------------------------------- */

static Trie *_pairs_trie(void)
{
    Trie *t = NULL;
    unsigned int i = 0;

    if (! (t = trie_alloc(NULL)) ) return NULL;

    for (i = 0; i < 4; i ++)
        if (trie_insert(t, _pairs[i], 2, variant_from_integer(i)) == -1)
            return trie_free(t);

    return t;
}

/* -------------------------------------------------------------------------- */

static int _fill(Trie *t, unsigned int n)
{
    /** @brief insert the keys "1".."n" with their number as value */

    char key[32];
    unsigned int i = 0;
    int len = 0;

    for (i = 1; i <= n; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        if (trie_insert(t, key, len, variant_from_integer(i)) == -1) return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _check(Trie *t, unsigned int n, unsigned int offset)
{
    /** @brief every key "1".."n" holds its number plus offset */

    char key[32];
    unsigned int i = 0, missing = 0;
    int len = 0;
    Variant v = { 0 };

    for (i = 1; i <= n; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        v = trie_lookup(t, key, len, NULL);
        if (! is_integer(v) || variant_to_integer(v) != i + offset) missing ++;
    }

    return missing ? -1 : 0;
}

/* -------------------------------------------------------------------------- */

static int _insert_lookup(void)
{
    Trie *t = NULL;
    Variant v = { 0 };

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_EQ_INT(_fill(t, KEYS), 0);
    ASSERT_EQ_INT(_check(t, KEYS, 0), 0);

    /* duplicates are refused, the value is kept */
    ASSERT_EQ_INT(trie_insert(t, "42", 2, variant_from_integer(0)), -1);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "42", 2, NULL)), 42);

    /* absent keys, prefixes and extensions of present keys */
    ASSERT_EQ_INT(trie_has(t, "0", 1), 0);
    ASSERT_EQ_INT(trie_has(t, "42", 2), 1);
    ASSERT_EQ_INT(trie_has(t, "4", 1), 1);
    ASSERT_EQ_INT(trie_has(t, "420", 3), 1);
    ASSERT_EQ_INT(trie_has(t, "10001", 5), 0);
    ASSERT_EQ_INT(trie_has(t, "", 0), 0);
    v = trie_lookup(t, "10001", 5, NULL);
    ASSERT_TRUE(is_null(v));
    v = trie_lookup(t, "4200", 4, NULL);
    ASSERT_TRUE(is_integer(v));
    ASSERT_EQ_UINT(variant_to_integer(v), 4200);

    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _insert_lookup_volume(void)
{
    Trie *t = NULL;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_EQ_INT(_fill(t, KEYS_SLOW), 0);
    ASSERT_EQ_INT(_check(t, KEYS_SLOW, 0), 0);
    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _update_remove(void)
{
    Trie *t = NULL;
    Variant v = { 0 };
    char key[32];
    unsigned int i = 0;
    int len = 0;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_EQ_INT(_fill(t, KEYS), 0);

    /* update returns the previous value, absent keys are not created */
    for (i = 1; i <= KEYS; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        v = trie_update(t, key, len, variant_from_integer(i + 1));
        if (! is_integer(v) || variant_to_integer(v) != i) {
            test_fail(__FILE__, __LINE__, "update of %s returned nothing", key);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_INT(_check(t, KEYS, 1), 0);
    v = trie_update(t, "10001", 5, variant_from_integer(1));
    ASSERT_TRUE(is_null(v));
    ASSERT_EQ_INT(trie_has(t, "10001", 5), 0);

    /* remove returns the value, a second time nothing */
    for (i = 1; i <= KEYS; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        v = trie_remove(t, key, len);
        if (! is_integer(v) || variant_to_integer(v) != i + 1) {
            test_fail(__FILE__, __LINE__, "remove of %s returned nothing", key);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    for (i = 1; i <= KEYS; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        if (trie_has(t, key, len)) {
            test_fail(__FILE__, __LINE__, "%s survived its removal", key);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    v = trie_remove(t, "42", 2);
    ASSERT_TRUE(is_null(v));
    ASSERT_NULL(trie_each(t));

    /* the emptied trie is reusable */
    ASSERT_EQ_INT(trie_insert(t, "42", 2, variant_from_integer(0x888)), 0);
    v = trie_update(t, "42", 2, variant_from_integer(0x8989));
    ASSERT_EQ_UINT(variant_to_integer(v), 0x888);
    v = trie_remove(t, "42", 2);
    ASSERT_EQ_UINT(variant_to_integer(v), 0x8989);
    ASSERT_EQ_INT(trie_has(t, "42", 2), 0);

    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _random_remove(void)
{
    static unsigned char present[KEYS + 1];
    Trie *t = NULL;
    Variant v = { 0 };
    char key[32];
    unsigned int i = 0, n = 0, removed = 0;
    int len = 0;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_EQ_INT(_fill(t, KEYS), 0);
    memset(present, 1, sizeof(present));

    /* remove a random third of the keys, some of them twice */
    for (i = 0; i < KEYS / 2; i ++) {
        n = 1 + rand() % KEYS;
        len = snprintf(key, sizeof(key), "%u", n);
        v = trie_remove(t, key, len);
        if (is_integer(v) != present[n]) {
            test_fail(__FILE__, __LINE__, "%s: %s", key,
                      present[n] ? "not removed" : "removed twice");
            break;
        }
        removed += present[n]; present[n] = 0;
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_TRUE(removed > KEYS / 4);

    /* the others are intact, in order */
    for (i = 1; i <= KEYS; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        v = trie_lookup(t, key, len, NULL);
        if (is_integer(v) != present[i] ||
            (present[i] && variant_to_integer(v) != i)) {
            test_fail(__FILE__, __LINE__, "%s: wrong after removals", key);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);

    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _prefix_iterator(void)
{
    Trie *t = NULL;
    Trie_Iterator *it = NULL;
    unsigned int n = 0, i = 0;
    char key[32], last[32] = "";
    int len = 0;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_EQ_INT(_fill(t, KEYS), 0);

    /* "800" and "8000".."8009", in order */
    for (it = trie_each_prefix(t, "800", 3); it; it = trie_next(it)) {
        if (it->len < 3 || memcmp(it->key, "800", 3) ||
            (n && memcmp(last, it->key, it->len) >= 0 &&
             it->len <= strlen(last))) {
            test_fail(
                __FILE__, __LINE__, "%.*s out of place", (int) it->len, it->key
            );
            trie_break(it);
            break;
        }
        memcpy(last, it->key, it->len); last[it->len] = '\0';
        n ++;
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_UINT(n, 11);

    /* an absent prefix, an exact key as prefix, an early break */
    ASSERT_NULL(trie_each_prefix(t, "800000", 6));
    ASSERT_NULL(trie_each_prefix(t, "x", 1));
    ASSERT_NOT_NULL(it = trie_each_prefix(t, "9999", 4));
    ASSERT_EQ_UINT(it->len, 4);
    ASSERT_EQ_UINT(variant_to_integer(it->val), 9999);
    ASSERT_NULL(trie_next(it));
    ASSERT_NOT_NULL(it = trie_each(t));
    ASSERT_NULL(trie_break(it));

    /* the whole trie, every key once */
    for (it = trie_each(t), n = 0; it; it = trie_next(it)) n ++;
    ASSERT_EQ_UINT(n, KEYS);
    for (i = 1; i <= 3; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        ASSERT_EQ_INT(trie_has(t, key, len), 1);
    }

    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _lexicographic_order(void)
{
    /* prefixes sort first and bytes compare unsigned */
    static const char *keys[] = {
        "b", "a", "ab", "aa", "\xff", "\x80", "\x7f", "a\xff", "abc", "abd",
        "ab\xff", "\x01", "zz", "z", "a\x80", "\xff\xff", "\xff\x01"
    };
    static _Entry expected[32], got[32];
    Trie *t = NULL;
    unsigned int i = 0, n = 0;
    size_t len = 0;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));

    for (i = 0; i < sizeof(keys) / sizeof(*keys); i ++) {
        len = strlen(keys[i]);
        ASSERT_EQ_INT(
            trie_insert(t, keys[i], len, variant_from_integer(i)), 0
        );
        memcpy(expected[n].key, keys[i], len);
        expected[n].len = len; expected[n].val = i;
        n ++;
    }

    qsort(expected, n, sizeof(*expected), _compare);
    ASSERT_EQ_INT(_collect(t, got, 32), (int) n);
    for (i = 0; i < n; i ++) {
        if (_compare(& expected[i], & got[i]) ||
            expected[i].val != got[i].val) {
            test_fail(
                __FILE__, __LINE__, "position %u: got %.*s, expected %.*s", i,
                (int) got[i].len, got[i].key,
                (int) expected[i].len, expected[i].key
            );
        }
    }

    ASSERT_NULL(trie_free(t));

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _embedded_nul_keys(void)
{
    /* keys are length-delimited: a NUL byte is data, and a key that
       extends an existing key with a NUL is a different key */
    static _Entry got[8];
    Trie *t = NULL;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_EQ_INT(trie_insert(t, "a", 1, variant_from_integer(1)), 0);
    ASSERT_EQ_INT(trie_insert(t, "a\0", 2, variant_from_integer(2)), 0);
    ASSERT_EQ_INT(trie_insert(t, "a\0b", 3, variant_from_integer(3)), 0);
    ASSERT_EQ_INT(trie_insert(t, "\0", 1, variant_from_integer(4)), 0);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "a", 1, NULL)), 1);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "a\0", 2, NULL)), 2);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "a\0b", 3, NULL)), 3);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "\0", 1, NULL)), 4);
    ASSERT_EQ_INT(_collect(t, got, 8), 4);
    ASSERT_TRUE(_sorted(got, 4));
    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _remove_if_and_freeval(void)
{
    Trie *t = NULL;
    Variant v = { 0 };

    _freed = 0;
    ASSERT_NOT_NULL(t = trie_alloc(_count_free));
    ASSERT_EQ_INT(_fill(t, 10), 0);

    /* the condition decides, the caller owns a removed value */
    v = trie_remove_if(t, "3", 1, _is_odd);
    ASSERT_EQ_UINT(variant_to_integer(v), 3);
    v = trie_remove_if(t, "4", 1, _is_odd);
    ASSERT_TRUE(is_null(v));
    ASSERT_EQ_INT(trie_has(t, "4", 1), 1);
    v = trie_remove_if(t, "4", 1, NULL);
    ASSERT_EQ_UINT(variant_to_integer(v), 4);
    v = trie_remove_if(t, "11", 2, _is_odd);
    ASSERT_TRUE(is_null(v));
    ASSERT_EQ_UINT(_freed, 0);

    /* a deleting traversal and the destruction release the rest */
    trie_foreach(t, _delete_a);
    ASSERT_EQ_UINT(_freed, 0);
    ASSERT_NULL(trie_free(t));
    ASSERT_EQ_UINT(_freed, 8);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _key_limits(void)
{
    Trie *t = NULL;
    char *key = NULL;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_NOT_NULL(key = malloc(UINT16_MAX + 1));
    memset(key, 'k', UINT16_MAX + 1);

    ASSERT_EQ_INT(trie_insert(t, key, 0, variant_from_integer(1)), -1);
    ASSERT_EQ_INT(trie_insert(t, NULL, 1, variant_from_integer(1)), -1);
    ASSERT_EQ_INT(trie_insert(NULL, key, 1, variant_from_integer(1)), -1);
    ASSERT_EQ_INT(trie_insert(t, key, UINT16_MAX, variant_from_integer(1)), -1);
    ASSERT_EQ_INT(
        trie_insert(t, key, UINT16_MAX - 1, variant_from_integer(1)), 0
    );
    ASSERT_EQ_INT(trie_has(t, key, UINT16_MAX - 1), 1);
    ASSERT_EQ_INT(trie_has(t, key, UINT16_MAX - 2), 0);
    ASSERT_EQ_INT(trie_insert(t, "k", 1, variant_from_integer(2)), 0);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "k", 1, NULL)), 2);
    ASSERT_EQ_INT(trie_has(t, "kk", 2), 0);

    free(key);
    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _key_without_terminator(void)
{
    /* a key is its len bytes: what follows them in the caller's buffer
       must not matter, and cannot be read */
    Trie *t = NULL;
    char buffer[4] = { 'k', 'k', 'k', 'k' };

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_EQ_INT(trie_insert(t, buffer, 4, variant_from_integer(4)), 0);
    ASSERT_EQ_INT(trie_insert(t, buffer, 1, variant_from_integer(1)), 0);
    ASSERT_EQ_INT(trie_insert(t, buffer, 2, variant_from_integer(2)), 0);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "k", 1, NULL)), 1);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "kk", 2, NULL)), 2);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "kkkk", 4, NULL)), 4);
    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _free_balanced(void)
{
    Trie *t = NULL;
    unsigned int i = 0;

    /* every node must be released, LeakSanitizer checks the rest */
    ASSERT_NOT_NULL(t = _pairs_trie());
    for (i = 0; i < 4; i ++) ASSERT_EQ_INT(trie_has(t, _pairs[i], 2), 1);
    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _foreach_delete_all(void)
{
    Trie *t = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(t = _pairs_trie());
    trie_foreach(t, _delete_all);

    /* the trie is empty and usable again */
    for (i = 0; i < 4; i ++) ASSERT_EQ_INT(trie_has(t, _pairs[i], 2), 0);
    ASSERT_EQ_INT(trie_insert(t, "aa", 2, variant_from_integer(7)), 0);
    ASSERT_EQ_INT(trie_has(t, "aa", 2), 1);
    ASSERT_EQ_INT(trie_has(t, "ab", 2), 0);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "aa", 2, NULL)), 7);
    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _foreach_delete_some(void)
{
    Trie *t = NULL;

    ASSERT_NOT_NULL(t = _pairs_trie());
    trie_foreach(t, _delete_a);

    /* one subtree collapsed, the other one is intact */
    ASSERT_EQ_INT(trie_has(t, "aa", 2), 0);
    ASSERT_EQ_INT(trie_has(t, "ab", 2), 0);
    ASSERT_EQ_INT(trie_has(t, "ba", 2), 1);
    ASSERT_EQ_INT(trie_has(t, "bb", 2), 1);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "bb", 2, NULL)), 3);
    ASSERT_EQ_INT(trie_insert(t, "ab", 2, variant_from_integer(9)), 0);
    ASSERT_EQ_INT(trie_has(t, "ab", 2), 1);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "ab", 2, NULL)), 9);
    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _batch(Trie *t, const char *prefix, const char **suffixes,
                  unsigned int count, uint64_t base)
{
    /** @brief build and insert a batch of prefix + suffix keys */

    Trie_Leaf *list[64];
    char key[128];
    unsigned int i = 0;
    size_t plen = strlen(prefix), len = 0;
    int ret = 0;

    for (i = 0; i < count; i ++) {
        len = snprintf(key, sizeof(key), "%s%s", prefix, suffixes[i]);
        if (! (list[i] = _leaf(key, len, base + i)) ) return -1;
    }

    ret = trie_insert_prefix_list(t, plen, list, count);

    return ret;
}

/* -------------------------------------------------------------------------- */

static int _batch_basic(void)
{
    /* the legacy shape: an existing tree, then a batch under a new prefix */
    static const char *regular[] = {
        "zero", "user/alice/name", "user/alice/age", "user/bob/name",
        "user/bob/age", "bob/secure", "bus", "bubble", "baz", "beef",
        "bob/secret", "bob/secrets", "bob/second", "bob/sales", "alpha",
        "alice/secret", "armada", "alma", "aztec", "bot/enabled",
        "config/server/host", "config/server/port", "conf", "can", "cool",
        "comb", "config/client/timeout"
    };
    static const char *suffixes[] = { "STREET", "MAPBLKLOT", "BLOCK_NUM" };
    static const char prefix[] = "features/0/properties/";
    static _Entry got[64];
    Trie *t = NULL;
    Trie_Iterator *it = NULL;
    Variant v = { 0 };
    char key[128];
    unsigned int i = 0, n = 0;
    int len = 0;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    for (i = 0; i < sizeof(regular) / sizeof(*regular); i ++) {
        ASSERT_EQ_INT(
            trie_insert(t, regular[i], strlen(regular[i]),
                        variant_from_integer(100 + i)), 0
        );
    }

    ASSERT_EQ_INT(_batch(t, prefix, suffixes, 3, 1001), 3);

    /* every key is found with its value, the batch and the rest */
    for (i = 0; i < 3; i ++) {
        len = snprintf(key, sizeof(key), "%s%s", prefix, suffixes[i]);
        v = trie_lookup(t, key, len, NULL);
        ASSERT_TRUE(is_integer(v));
        ASSERT_EQ_UINT(variant_to_integer(v), 1001 + i);
    }
    for (i = 0; i < sizeof(regular) / sizeof(*regular); i ++) {
        v = trie_lookup(t, regular[i], strlen(regular[i]), NULL);
        ASSERT_TRUE(is_integer(v));
        ASSERT_EQ_UINT(variant_to_integer(v), 100 + i);
    }

    /* the prefix traversal sees the batch, sorted, and nothing else */
    for (it = trie_each_prefix(t, prefix, strlen(prefix)); it;
         it = trie_next(it)) n ++;
    ASSERT_EQ_UINT(n, 3);
    ASSERT_EQ_INT(_collect(t, got, 64), 30);
    ASSERT_TRUE(_sorted(got, 30));

    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _batch_edges(void)
{
    static const char *one[] = { "x" };
    static const char *two[] = { "b", "a" };
    static const char *three[] = { "c", "a", "b" };
    static const char *dup[] = { "a", "a", "d" };
    static _Entry got[64];
    Trie *t = NULL;
    Trie_Leaf *list[4];

    /* into an empty trie, one, two and three leaves */
    _freed = 0;
    ASSERT_NOT_NULL(t = trie_alloc(_count_free));
    ASSERT_EQ_INT(_batch(t, "p/", one, 1, 1), 1);
    ASSERT_EQ_INT(_batch(t, "q/", two, 2, 10), 2);
    ASSERT_EQ_INT(_batch(t, "r/", three, 3, 20), 3);
    ASSERT_EQ_INT(_collect(t, got, 64), 6);
    ASSERT_TRUE(_sorted(got, 6));
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "q/a", 3, NULL)), 11);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "r/b", 3, NULL)), 22);

    /* a duplicate inside the batch and a duplicate of an existing key are
       dropped and their values released, the rest is inserted */
    ASSERT_EQ_INT(_batch(t, "r/", dup, 3, 30), 1);
    ASSERT_EQ_UINT(_freed, 2);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "r/a", 3, NULL)), 21);
    ASSERT_EQ_UINT(variant_to_integer(trie_lookup(t, "r/d", 3, NULL)), 32);
    ASSERT_EQ_INT(_collect(t, got, 64), 7);

    /* no prefix at all */
    ASSERT_NOT_NULL(list[0] = _leaf("m", 1, 40));
    ASSERT_NOT_NULL(list[1] = _leaf("a", 1, 41));
    ASSERT_NOT_NULL(list[2] = _leaf("z", 1, 42));
    ASSERT_EQ_INT(trie_insert_prefix_list(t, 0, list, 3), 3);
    ASSERT_EQ_INT(_collect(t, got, 64), 10);
    ASSERT_TRUE(_sorted(got, 10));

    /* bad parameters */
    ASSERT_EQ_INT(trie_insert_prefix_list(t, 0, NULL, 3), -1);
    ASSERT_EQ_INT(trie_insert_prefix_list(t, 0, list, 0), -1);
    ASSERT_EQ_INT(trie_insert_prefix_list(NULL, 0, list, 3), -1);

    ASSERT_NULL(trie_free(t));
    ASSERT_EQ_UINT(_freed, 12);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _batch_deep_first_leaf(void)
{
    /* the first leaf of the batch shares more than the prefix with keys
       already in the trie, so its insertion point lies below nodes that
       split inside the prefix region; the other leaves must still land
       where a lookup from the root will find them */
    static const char *existing[] = { "pre/SXa", "pre/STa", "pre/STe" };
    static const char *suffixes[] = { "STq", "MXzz", "Zed" };
    static _Entry got[64];
    Trie *t = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    for (i = 0; i < 3; i ++) {
        ASSERT_EQ_INT(
            trie_insert(t, existing[i], strlen(existing[i]),
                        variant_from_integer(i)), 0
        );
    }

    ASSERT_EQ_INT(_batch(t, "pre/", suffixes, 3, 10), 3);

    ASSERT_EQ_INT(trie_has(t, "pre/STq", 7), 1);
    ASSERT_EQ_INT(trie_has(t, "pre/MXzz", 8), 1);
    ASSERT_EQ_INT(trie_has(t, "pre/Zed", 7), 1);
    ASSERT_EQ_UINT(
        variant_to_integer(trie_lookup(t, "pre/MXzz", 8, NULL)), 11
    );
    for (i = 0; i < 3; i ++)
        ASSERT_EQ_INT(trie_has(t, existing[i], strlen(existing[i])), 1);
    ASSERT_EQ_INT(_collect(t, got, 64), 6);
    ASSERT_TRUE(_sorted(got, 6));

    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _batch_deep_ancestor(void)
{
    /* found by the differential test: the first leaf of the last batch
       diverges from an existing key below the prefix depth, and the
       insertion slot that came out of it is not where the second leaf
       belongs */
    static const char *b1[] = { "bB" };
    static const char *b2[] = { "A0", "BA", "B", "aba" };
    static const char *b3[] = { "B00", "0aa" };
    static const char *all[] = {
        "BbB", "AA0", "ABA", "AB", "Aaba", "AB00", "A0aa"
    };
    static _Entry got[16];
    Trie *t = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_EQ_INT(_batch(t, "B", b1, 1, 0), 1);
    ASSERT_EQ_INT(_batch(t, "A", b2, 4, 10), 4);
    ASSERT_EQ_INT(_batch(t, "A", b3, 2, 20), 2);

    for (i = 0; i < 7; i ++) {
        if (! trie_has(t, all[i], strlen(all[i])))
            test_fail(__FILE__, __LINE__, "%s is unreachable", all[i]);
    }
    ASSERT_EQ_INT(_collect(t, got, 16), 7);
    ASSERT_TRUE(_sorted(got, 7));
    ASSERT_EQ_INT(_batch(t, "A", b3, 2, 30), 0);
    ASSERT_EQ_INT(_collect(t, got, 16), 7);

    ASSERT_NULL(trie_free(t));

    return test_status();
}

/* -------------------------------------------------------------------------- */

static int _batch_differential(void)
{
    /* random keys under random prefixes: one trie filled one key at a
       time, another one by batches, must hold the same entries in the
       same order, and the order must be lexicographic */
    static _Entry single[512], batched[512], keys[512];
    static unsigned int batches[512];
    static const char alphabet[] = "abAB01";
    Trie *t1 = NULL, *t2 = NULL;
    Trie_Leaf *list[16];
    char prefix[8];
    unsigned int round = 0, i = 0, j = 0, n = 0, count = 0, plen = 0, ret = 0;
    unsigned int inserted = 0, b = 0, k = 0;

    for (round = 0; round < 20; round ++) {
        ASSERT_NOT_NULL(t1 = trie_alloc(NULL));
        ASSERT_NOT_NULL(t2 = trie_alloc(NULL));
        n = 0; inserted = 0; b = 0;

        while (n < 400) {
            /* a batch of 1..16 keys sharing a prefix of 0..5 characters */
            plen = rand() % 6;
            for (i = 0; i < plen; i ++) prefix[i] = alphabet[rand() % 6];
            count = 1 + rand() % 16;
            if (n + count > 400) count = 400 - n;

            for (i = 0; i < count; i ++) {
                _Entry *e = & keys[n + i];
                e->len = plen + 1 + rand() % 4;
                memcpy(e->key, prefix, plen);
                for (j = plen; j < e->len; j ++)
                    e->key[j] = alphabet[rand() % 6];
                e->val = n + i;
                ret = trie_insert(
                    t1, e->key, e->len, variant_from_integer(e->val)
                );
                inserted += (ret == 0);
                ASSERT_NOT_NULL(list[i] = _leaf(e->key, e->len, e->val));
            }

            ret = trie_insert_prefix_list(t2, plen, list, count);
            batches[b ++] = (plen << 8) | count;
            n += count;
        }

        ASSERT_EQ_INT(_collect(t1, single, 512), (int) inserted);
        if ( (ret = _collect(t2, batched, 512)) != inserted) {
            /* dump the round: each batch, then the entries in one trie only */
            for (i = k = 0; i < b; i ++) {
                printf("batch %u, prefix %u:", i, batches[i] >> 8);
                for (j = 0; j < (batches[i] & 0xff); j ++, k ++)
                    printf(" %.*s", (int) keys[k].len, keys[k].key);
                printf("\n");
            }
            for (i = 0; i < ret; i ++) {
                for (j = 0; j < inserted; j ++)
                    if (! _compare(& batched[i], & single[j])) break;
                if (j == inserted) {
                    printf("batched only: %.*s\n",
                           (int) batched[i].len, batched[i].key);
                }
            }
            for (i = 0; i < inserted; i ++) {
                for (j = 0; j < ret; j ++)
                    if (! _compare(& single[i], & batched[j])) break;
                if (j == ret) {
                    printf("single only: %.*s\n",
                           (int) single[i].len, single[i].key);
                }
            }
            for (i = 0; i < n; i ++) {
                if (! trie_has(t2, keys[i].key, keys[i].len)) {
                    printf("unreachable in the batched trie: %.*s\n",
                           (int) keys[i].len, keys[i].key);
                }
                if (! trie_has(t1, keys[i].key, keys[i].len)) {
                    printf("unreachable in the single trie: %.*s\n",
                           (int) keys[i].len, keys[i].key);
                }
            }
            test_fail(__FILE__, __LINE__, "round %u: %u batched, %u single",
                      round, ret, inserted);
            trie_free(t1); trie_free(t2);
            return -1;
        }
        ASSERT_TRUE(_sorted(single, inserted));
        for (i = 0; i < inserted; i ++) {
            if (_compare(& single[i], & batched[i]) ||
                single[i].val != batched[i].val) {
                test_fail(
                    __FILE__, __LINE__, "round %u, position %u: %.*s vs %.*s",
                    round, i, (int) single[i].len, single[i].key,
                    (int) batched[i].len, batched[i].key
                );
                break;
            }
        }
        for (i = 0; i < n; i ++) {
            if (trie_has(t1, keys[i].key, keys[i].len) !=
                trie_has(t2, keys[i].key, keys[i].len)) {
                test_fail(
                    __FILE__, __LINE__, "round %u: %.*s found in one trie only",
                    round, (int) keys[i].len, keys[i].key
                );
                break;
            }
        }

        trie_free(t1); trie_free(t2);
        if (test_status()) return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

/* a random nested document, as JSON pointers: every container is one
   batch, flushed when it closes, so nested batches come before their
   parent's, which is the order the parser feeds the trie */
typedef struct _Doc {
    _Entry keys[512];
    unsigned int count;
    unsigned int batch_start[128], batch_len[128], batch_prefix[128];
    unsigned int batches;
} _Doc;

static void _document(_Doc *d, const char *pointer, unsigned int depth)
{
    static const char *names[] = {
        "a", "ab", "b", "id", "name", "n", "x", "xy", "0", "key", "k1"
    };
    char child[64];
    unsigned int i = 0, members = 1 + rand() % 6, first = 0, len = 0;
    unsigned int plen = strlen(pointer) + 1;
    int array = rand() & 1;

    if (d->batches == 128 || d->count + members > 500) return;

    /* the containers first, they close before this one */
    for (i = 0; i < members; i ++) {
        if (depth < 3 && rand() % 4 == 0) {
            if (array) snprintf(child, sizeof(child), "%s/%u", pointer, i);
            else {
                snprintf(child, sizeof(child), "%s/%s", pointer,
                         names[rand() % 11]);
            }
            _document(d, child, depth + 1);
        }
    }

    /* then this container's own scalars, one batch */
    first = d->count;
    for (i = 0; i < members && d->count < 500; i ++) {
        _Entry *e = & d->keys[d->count];
        if (array) len = snprintf(e->key, sizeof(e->key), "%s/%u", pointer, i);
        else {
            len = snprintf(e->key, sizeof(e->key), "%s/%s", pointer,
                           names[rand() % 11]);
        }
        e->len = len; e->val = d->count;
        d->count ++;
    }
    if (d->count > first) {
        d->batch_start[d->batches] = first;
        d->batch_len[d->batches] = d->count - first;
        d->batch_prefix[d->batches] = plen;
        d->batches ++;
    }
}

/* -------------------------------------------------------------------------- */

static int _batch_json_shaped(void)
{
    static _Doc d;
    static _Entry single[512], batched[512];
    Trie *t1 = NULL, *t2 = NULL;
    Trie_Leaf *list[64];
    unsigned int round = 0, b = 0, i = 0, j = 0, n1 = 0, n2 = 0, unreachable = 0;
    int ret = 0;

    for (round = 0; round < 20; round ++) {
        memset(& d, 0, sizeof(d));
        _document(& d, "", 0);

        ASSERT_NOT_NULL(t1 = trie_alloc(NULL));
        ASSERT_NOT_NULL(t2 = trie_alloc(NULL));

        for (b = 0; b < d.batches; b ++) {
            for (i = 0; i < d.batch_len[b]; i ++) {
                _Entry *e = & d.keys[d.batch_start[b] + i];
                trie_insert(t1, e->key, e->len, variant_from_integer(e->val));
                ASSERT_NOT_NULL(list[i] = _leaf(e->key, e->len, e->val));
            }
            trie_insert_prefix_list(t2, d.batch_prefix[b], list, d.batch_len[b]);
        }

        ret = _collect(t1, single, 512); n1 = (ret < 0) ? 0 : ret;
        ret = _collect(t2, batched, 512); n2 = (ret < 0) ? 0 : ret;
        for (i = 0; i < d.count; i ++)
            unreachable += ! trie_has(t2, d.keys[i].key, d.keys[i].len);

        if (n1 != n2 || unreachable || ! _sorted(batched, n2)) {
            for (b = 0; b < d.batches; b ++) {
                printf("batch %u, prefix %u:", b, d.batch_prefix[b]);
                for (i = 0; i < d.batch_len[b]; i ++) {
                    _Entry *e = & d.keys[d.batch_start[b] + i];
                    printf(" %.*s", (int) e->len, e->key);
                }
                printf("\n");
            }
            for (i = 0; i < d.count; i ++) {
                if (! trie_has(t2, d.keys[i].key, d.keys[i].len)) {
                    printf("unreachable: %.*s\n",
                           (int) d.keys[i].len, d.keys[i].key);
                }
            }
            for (i = 1; i < n2; i ++) {
                if (_compare(& batched[i - 1], & batched[i]) >= 0) {
                    printf("out of order: %.*s then %.*s\n",
                           (int) batched[i - 1].len, batched[i - 1].key,
                           (int) batched[i].len, batched[i].key);
                }
            }
            test_fail(
                __FILE__, __LINE__,
                "round %u: %u keys, %u single, %u batched, %u unreachable, %s",
                round, d.count, n1, n2, unreachable,
                _sorted(batched, n2) ? "sorted" : "out of order"
            );
            trie_free(t1); trie_free(t2);
            return -1;
        }
        for (i = 0; i < n1; i ++) {
            if (_compare(& single[i], & batched[i]) || single[i].val != batched[i].val) {
                test_fail(__FILE__, __LINE__, "round %u: position %u differs", round, i);
                break;
            }
        }
        (void) j;
        trie_free(t1); trie_free(t2);
        if (test_status()) return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static Trie_Leaf *_inline_leaf(const char *key, size_t len, const char *value)
{
    /** @brief a leaf keeping its string value after the key, as the JSON
        parser builds them */

    Trie_Leaf *leaf = NULL;
    size_t vlen = strlen(value);
    char *string = NULL;

    if (! (leaf = malloc(sizeof(*leaf) + len + 1 + vlen + 1)) ) return NULL;

    memcpy(leaf->key, key, len);
    leaf->key[len] = '\0';
    leaf->len = len; leaf->own = TRIE_LEAF_INLINE;

    string = leaf->key + len + 1;
    memcpy(string, value, vlen + 1);
    leaf->val.value.pointer = string;
    leaf->val.metadata.fields.type = _VALUE_OBJECT;
    leaf->val.metadata.fields.dword = vlen;

    return leaf;
}

/* -------------------------------------------------------------------------- */

static int _inline_values(void)
{
    /* a leaf that keeps its string value after its key: the trie never
       frees that value on its own, refuses a duplicate without calling the
       destructor, and hands out copies on update and removal */

    static char gamma[] = "gamma";
    Trie *t = NULL;
    Trie_Leaf *list[3];
    Variant v = { 0 };

    _freed = 0;
    ASSERT_NOT_NULL(t = trie_alloc(_count_free));
    ASSERT_NOT_NULL(list[0] = _inline_leaf("p/a", 3, "alpha"));
    ASSERT_NOT_NULL(list[1] = _inline_leaf("p/b", 3, "beta"));
    ASSERT_NOT_NULL(list[2] = _inline_leaf("p/a", 3, "again"));
    ASSERT_EQ_INT(trie_insert_prefix_list(t, 2, list, 3), 2);
    ASSERT_EQ_UINT(_freed, 0);

    v = trie_lookup(t, "p/a", 3, NULL);
    ASSERT_TRUE(_is_object(v));
    ASSERT_EQ_UINT(v.metadata.fields.dword, 5);
    ASSERT_TRUE(! strcmp(v.value.pointer, "alpha"));

    /* the previous value comes out as a copy the caller owns, and the
       replacement becomes the trie's to destroy */
    v = trie_update(t, "p/a", 3, variant_from_pointer(gamma));
    ASSERT_TRUE(_is_object(v));
    ASSERT_TRUE(! strcmp(v.value.pointer, "alpha"));
    free(v.value.pointer);
    v = trie_lookup(t, "p/a", 3, NULL);
    ASSERT_EQ_PTR(v.value.pointer, gamma);

    v = trie_remove(t, "p/b", 3);
    ASSERT_TRUE(_is_object(v));
    ASSERT_TRUE(! strcmp(v.value.pointer, "beta"));
    free(v.value.pointer);
    ASSERT_EQ_INT(trie_has(t, "p/b", 3), 0);
    ASSERT_EQ_UINT(_freed, 0);

    ASSERT_NULL(trie_free(t));
    ASSERT_EQ_UINT(_freed, 1);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _before(const char *a, size_t alen, const char *b, size_t blen)
{
    /** @brief lexicographic order of two binary keys, prefix first */

    size_t l = (alen < blen) ? alen : blen;
    int c = memcmp(a, b, l);

    return c ? (c < 0) : (alen < blen);
}

/* -------------------------------------------------------------------------- */

static int _seek(void)
{
    /* seeking positions an ordered traversal on the first key not lower
       than the one sought, whether or not it is in the trie */

    static const char *keys[] = { "a", "ab", "abc", "b", "ba", "c" };
    static const struct { const char *key; size_t len; const char *found; }
    probes[] = {
        { "", 0, "a" }, { "a", 1, "a" }, { "aa", 2, "ab" },
        { "abc", 3, "abc" }, { "abc\0", 4, "b" }, { "abd", 3, "b" },
        { "b", 1, "b" }, { "b\0", 2, "ba" }, { "bz", 2, "c" },
        { "c", 1, "c" }, { "ca", 2, NULL }, { "d", 1, NULL }
    };
    static _Entry entries[KEYS];
    Trie *t = NULL;
    Trie_Iterator *it = NULL;
    unsigned int i = 0, j = 0, n = 0, len = 0, round = 0;
    char probe[8];

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    for (i = 0; i < 6; i ++)
        ASSERT_EQ_INT(trie_insert(t, keys[i], strlen(keys[i]),
                                  variant_from_integer(i)), 0);

    for (i = 0; i < sizeof(probes) / sizeof(*probes); i ++) {
        it = trie_seek(t, probes[i].key, probes[i].len);
        if (! probes[i].found) {
            if (it) {
                test_fail(__FILE__, __LINE__, "probe %u found %.*s",
                          i, (int) it->len, it->key);
                trie_break(it);
            }
            continue;
        }
        if (! it || it->len != strlen(probes[i].found) ||
            memcmp(it->key, probes[i].found, it->len)) {
            test_fail(__FILE__, __LINE__, "probe %u expected %s got %.*s", i,
                      probes[i].found, it ? (int) it->len : 0,
                      it ? it->key : "");
        }
        if (it) trie_break(it);
    }
    ASSERT_EQ_INT(test_status(), 0);

    /* the traversal continues in order after the seek */
    ASSERT_NOT_NULL(it = trie_seek(t, "ab", 2));
    for (i = 1; it && i < 6; i ++) {
        if (it->len != strlen(keys[i]) || memcmp(it->key, keys[i], it->len))
            break;
        it = trie_next(it);
    }
    ASSERT_EQ_UINT(i, 6);
    ASSERT_NULL(it);
    ASSERT_NULL(trie_free(t));

    /* random probes against the sorted entries of a larger trie */
    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_EQ_INT(_fill(t, KEYS), 0);
    n = _collect(t, entries, KEYS);
    ASSERT_EQ_UINT(n, KEYS);

    for (round = 0; round < 2000; round ++) {
        len = 1 + rand() % 6;
        for (i = 0; i < len; i ++) probe[i] = "0123456789\0"[rand() % 11];
        if (round & 1) {
            j = rand() % n;
            len = entries[j].len; memcpy(probe, entries[j].key, len);
        }
        for (j = 0; j < n; j ++)
            if (! _before(entries[j].key, entries[j].len, probe, len)) break;

        it = trie_seek(t, probe, len);
        if (j == n) {
            if (it) {
                test_fail(
                    __FILE__, __LINE__, "round %u: unexpected key", round
                );
                trie_break(it);
            }
        } else if (! it) {
            test_fail(__FILE__, __LINE__, "round %u: nothing found", round);
        } else {
            if (it->len != entries[j].len ||
                memcmp(it->key, entries[j].key, it->len)) {
                test_fail(__FILE__, __LINE__, "round %u: wrong key", round);
            } else if (j + 1 < n) {
                it = trie_next(it);
                if (! it || it->len != entries[j + 1].len ||
                    memcmp(it->key, entries[j + 1].key, it->len))
                    test_fail(
                        __FILE__, __LINE__, "round %u: wrong next", round
                    );
            }
            if (it) trie_break(it);
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _children(void)
{
    /* the children of a prefix, one per name, whatever the depth beneath
       them, with siblings sorting between a child and its descendants */

    static const char *keys[] = {
        "/book/author", "/book/author/name", "/book/author/firstname",
        "/book/author/dob", "/book/author-x", "/book/title", "/book/isbn/",
        "/book/isbn/x/y/z", "/books", "/author", "/book"
    };
    static const char *children[] = { "author", "author-x", "isbn", "title" };
    Trie *t = NULL;
    Trie_Iterator *it = NULL;
    const Trie_Leaf *leaf = NULL;
    Trie_Cursor cursor = { 0 };
    char key[200];
    unsigned int i = 0, n = 0;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    for (i = 0; i < sizeof(keys) / sizeof(*keys); i ++)
        ASSERT_EQ_INT(trie_insert(t, keys[i], strlen(keys[i]),
                                  variant_from_integer(i)), 0);

    it = trie_children(t, "/book/", 6, '/');
    for (; it; it = trie_next_child(it)) {
        if (n == 4 || it->child_len != strlen(children[n]) ||
            memcmp(it->child, children[n], it->child_len)) {
            test_fail(__FILE__, __LINE__, "child %u: got %.*s", n,
                      (int) it->child_len, it->child);
            trie_break(it);
            break;
        }
        /* a child with a key of its own carries its value */
        if (n == 0) ASSERT_EQ_UINT(variant_to_integer(it->val), 0);
        if (n == 2) ASSERT_EQ_UINT(it->len, 11);
        n ++;
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_UINT(n, 4);

    /* deeper, and a prefix with no key */
    n = 0;
    it = trie_children(t, "/book/isbn/", 11, '/');
    for (; it; it = trie_next_child(it)) n ++;
    ASSERT_EQ_UINT(n, 2);
    ASSERT_NULL(trie_children(t, "/nothing/", 9, '/'));

    /* the separator is the caller's: with '-' the children of "/book/"
       are "author", "author" (its descendants), "isbn/x/y/z" and so on,
       and duplicates are merged by name */
    n = 0;
    it = trie_children(t, "/book/", 6, '-');
    for (; it; it = trie_next_child(it)) n ++;
    ASSERT_EQ_UINT(n, 7);

    /* a child with many descendants is skipped with a seek, one with a
       few by stepping over them; either way, the descendants of a child
       with a key of its own are not a second child when a sibling whose
       name extends its own comes between */
    ASSERT_EQ_INT(trie_insert(t, "/wide/a", 7, variant_null()), 0);
    ASSERT_EQ_INT(trie_insert(t, "/wide/a-x", 9, variant_null()), 0);
    ASSERT_EQ_INT(trie_insert(t, "/wide/c", 7, variant_null()), 0);
    ASSERT_EQ_INT(trie_insert(t, "/wide/d/e", 9, variant_null()), 0);
    ASSERT_EQ_INT(trie_insert(t, "/wide/d/f", 9, variant_null()), 0);
    ASSERT_EQ_INT(trie_insert(t, "/wide/g", 7, variant_null()), 0);
    for (i = 0; i < 24; i ++) {
        char key[32];
        snprintf(key, sizeof(key), "/wide/%c/d%02u", (i < 12) ? 'a' : 'b', i);
        ASSERT_EQ_INT(trie_insert(t, key, strlen(key), variant_null()), 0);
    }
    n = 0;
    it = trie_children(t, "/wide/", 6, '/');
    for (; it; it = trie_next_child(it)) {
        static const char *wide[] = { "a", "a-x", "b", "c", "d", "g" };
        if (n == 6 || it->child_len != strlen(wide[n]) ||
            memcmp(it->child, wide[n], it->child_len)) {
            test_fail(__FILE__, __LINE__, "wide child %u: got %.*s", n,
                      (int) it->child_len, it->child);
            trie_break(it);
            break;
        }
        n ++;
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_UINT(n, 6);

    /* the smallest key under a prefix, the prefix itself when it is one */
    ASSERT_NOT_NULL(leaf = trie_lookup_prefix(t, "/book/", 6));
    ASSERT_EQ_UINT(leaf->len, 12);
    ASSERT_EQ_MEM(leaf->key, "/book/author", 12);
    ASSERT_NOT_NULL(leaf = trie_lookup_prefix(t, "/book", 5));
    ASSERT_EQ_UINT(leaf->len, 5);
    ASSERT_EQ_UINT(variant_to_integer(leaf->val), 10);
    ASSERT_NOT_NULL(leaf = trie_lookup_prefix(t, "/book/author", 12));
    ASSERT_EQ_UINT(leaf->len, 12);
    ASSERT_NOT_NULL(leaf = trie_lookup_prefix(t, "/book/i", 7));
    ASSERT_EQ_UINT(leaf->len, 11);
    ASSERT_NOT_NULL(leaf = trie_lookup_prefix(t, "/wide/b", 7));
    ASSERT_EQ_UINT(leaf->len, 11);
    ASSERT_EQ_MEM(leaf->key, "/wide/b/d12", 11);
    ASSERT_NULL(trie_lookup_prefix(t, "/nothing", 8));
    ASSERT_NULL(trie_lookup_prefix(t, "/book/authors", 13));
    ASSERT_NULL(trie_lookup_prefix(t, "/book/", 0));
    ASSERT_NULL(trie_lookup_prefix(NULL, "/book/", 6));

    /* a cursor resumes each lookup from the path of the previous one,
       whatever the two keys share, and answers as a plain lookup would */
    for (i = 0; i < 3; i ++) {
        static const char *probes[] = {
            "/wide/a/d00", "/wide/a/d01", "/wide/a/d1", "/wide/a", "/wide/",
            "/wide/b/d12", "/wide/b/d13", "/wide/b/d1x", "/book/author",
            "/book/author/", "/book/author/dob", "/book/author/dobs",
            "/book", "/books", "/booking", "/nothing", "/", "/z", "/a", "/w"
        };
        const Trie_Leaf *plain = NULL;
        unsigned int j = 0;
        for (j = 0; j < sizeof(probes) / sizeof(*probes); j ++) {
            plain = trie_lookup_prefix(t, probes[j], strlen(probes[j]));
            leaf = trie_lookup_prefix_from(t, & cursor, probes[j],
                                           strlen(probes[j]));
            if (leaf != plain) {
                test_fail(__FILE__, __LINE__, "cursor: %s", probes[j]);
                return -1;
            }
        }
        /* a key longer than the cursor's buffer, then short ones again */
        memset(key, 'k', sizeof(key) - 1); key[0] = '/';
        ASSERT_NULL(trie_lookup_prefix_from(t, & cursor, key, sizeof(key) - 1));
        ASSERT_NOT_NULL(trie_lookup_prefix_from(t, & cursor, "/wide/c", 7));
        ASSERT_NULL(trie_cursor_free(& cursor));
    }
    ASSERT_NULL(trie_cursor_free(NULL));

    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _disabled(void)
{
    /* a trie without its lock behaves as one with it */

    Trie *t = NULL;
    Trie_Iterator *it = NULL;
    unsigned int n = 0;

    ASSERT_EQ_INT(trie_disable_lock(NULL), -1);
    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_EQ_INT(trie_insert(t, "ab", 2, variant_from_integer(1)), 0);
    ASSERT_EQ_INT(trie_disable_lock(t), 0);
    ASSERT_EQ_INT(trie_insert(t, "ac", 2, variant_from_integer(2)), 0);
    ASSERT_EQ_INT(trie_insert(t, "b", 1, variant_from_integer(3)), 0);
    ASSERT_EQ_INT(trie_has(t, "ab", 2), 1);
    ASSERT_EQ_INT(trie_has(t, "a", 1), 0);
    ASSERT_EQ_INT((int) trie_lookup(t, "ac", 2, NULL).value.integer, 2);
    ASSERT_EQ_INT((int) trie_update(t, "ac", 2, variant_from_integer(4))
                  .value.integer, 2);
    for (it = trie_each_prefix(t, "a", 1); it; it = trie_next(it)) n ++;
    ASSERT_EQ_UINT(n, 2);
    ASSERT_NOT_NULL(it = trie_each(t));
    ASSERT_NULL(trie_break(it));
    ASSERT_EQ_INT((int) trie_remove(t, "b", 1).value.integer, 3);
    ASSERT_EQ_INT(trie_has(t, "b", 1), 0);
    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _anchored(void)
{
    /* children listed from an anchor: the subtree of a prefix found from
       the one of a shorter prefix, a prefix holding a single key, a prefix
       whose keys part right past it, and a prefix the trie does not hold */

    static const char *keys[] = {
        "a/b", "a/c/d", "a/c/d/e", "p/0", "p/1", "pa", "x"
    };
    Trie *t = NULL;
    Trie_Iterator *it = NULL;
    void *a = NULL, *ac = NULL, *p = NULL;
    unsigned int i = 0, n = 0;

    ASSERT_NOT_NULL(t = trie_alloc(NULL));
    ASSERT_NULL(trie_anchor(t, NULL, "a/", 2));
    for (i = 0; i < sizeof(keys) / sizeof(*keys); i ++)
        ASSERT_EQ_INT(trie_insert(t, keys[i], strlen(keys[i]),
                                  variant_from_integer(i)), 0);

    /* the anchor of a prefix is the same from the root or from a parent */
    ASSERT_NOT_NULL(a = trie_anchor(t, NULL, "a/", 2));
    ASSERT_NOT_NULL(ac = trie_anchor(t, a, "a/c/", 4));
    ASSERT_EQ_PTR(ac, trie_anchor(t, NULL, "a/c/", 4));

    /* a single key under the prefix: its leaf is the anchor */
    ASSERT_NOT_NULL(it = trie_children_from(t, ac, "a/c/d/", 6, '/'));
    ASSERT_EQ_UINT(it->child_len, 1);
    ASSERT_TRUE(it->child[0] == 'e');
    ASSERT_NULL(trie_next_child(it));

    /* keys parting at the byte right after the prefix */
    ASSERT_NOT_NULL(p = trie_anchor(t, NULL, "p/", 2));
    for (it = trie_children_from(t, p, "p/", 2, '/'); it;
         it = trie_next_child(it)) {
        ASSERT_EQ_UINT(it->child_len, 1);
        ASSERT_TRUE(it->child[0] == '0' + (char) n);
        n ++;
    }
    ASSERT_EQ_UINT(n, 2);

    /* the children of a node, from the anchor of its parent */
    n = 0;
    for (it = trie_children_from(t, a, "a/c/", 4, '/'); it;
         it = trie_next_child(it)) n ++;
    ASSERT_EQ_UINT(n, 1);
    n = 0;
    for (it = trie_children_from(t, a, "a/", 2, '/'); it;
         it = trie_next_child(it)) n ++;
    ASSERT_EQ_UINT(n, 2);

    /* a prefix the trie does not hold, under an anchor that is not its */
    ASSERT_NULL(trie_children_from(t, a, "zz/", 3, '/'));
    ASSERT_NULL(trie_children_from(t, p, "a/", 2, '/'));
    ASSERT_NULL(trie_anchor(NULL, NULL, "a/", 2));
    ASSERT_NULL(trie_children_from(NULL, a, "a/", 2, '/'));

    ASSERT_NULL(trie_free(t));

    return 0;
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("insert_lookup", _insert_lookup),
    TEST("disabled", _disabled),
    TEST("anchored", _anchored),
    TEST_SLOW("insert_lookup_volume", _insert_lookup_volume),
    TEST("update_remove", _update_remove),
    TEST("random_remove", _random_remove),
    TEST("prefix_iterator", _prefix_iterator),
    TEST("lexicographic_order", _lexicographic_order),
    TEST("embedded_nul_keys", _embedded_nul_keys),
    TEST("remove_if_and_freeval", _remove_if_and_freeval),
    TEST("key_limits", _key_limits),
    TEST("key_without_terminator", _key_without_terminator),
    TEST("free_balanced", _free_balanced),
    TEST("foreach_delete_all", _foreach_delete_all),
    TEST("foreach_delete_some", _foreach_delete_some),
    TEST("batch_basic", _batch_basic),
    TEST("batch_edges", _batch_edges),
    TEST("batch_deep_first_leaf", _batch_deep_first_leaf),
    TEST("batch_deep_ancestor", _batch_deep_ancestor),
    TEST("batch_differential", _batch_differential),
    TEST("batch_json_shaped", _batch_json_shaped),
    TEST("inline_values", _inline_values),
    TEST("seek", _seek),
    TEST("children", _children)
};

TEST_SUITE(test_suite_cbtrie, "cbtrie", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
