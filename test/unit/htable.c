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
 *  data to be ensured and,  more generally, to use and operate in the         *
 *  same conditions as regards security.                                       *
 *                                                                             *
 *  The fact that you are presently reading this means that you have had       *
 *  knowledge of the CeCILL license and that you accept its terms.             *
 *                                                                             *
 ******************************************************************************/

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_HASHMAP
/* -------------------------------------------------------------------------- */

#include "../askl_test.h"
#include "../../lib/askl_htable.h"
#include "../../lib/arcane/bitops.c"

#ifdef HAS_ATOMICS
#define _STOP_GET(a)    _atomic_ldr(& (a)->stop)
#define _STOP_SET(a, v) _atomic_stlr(& (a)->stop, (v))
#else
#define _STOP_GET(a)    ((a)->stop)
#define _STOP_SET(a, v) do { (a)->stop = (v); } while (0)
#endif

#define KEYS 20000

/* a reference entry: keys may hold NUL bytes, so they carry their length */
typedef struct _Entry {
    unsigned char key[16];
    size_t len;
    uint64_t val;
    int alive;
} _Entry;

#define SORT_KEYS   0
#define SORT_VALUES 1

/* how the reference entries are sorted by qsort(3) */
static int _sort_mode = SORT_KEYS;
static int _sort_desc = 0;

static unsigned int _freed = 0;

/* thread start switch, so that the workers contend from the first access */
static pthread_mutex_t _mx_switch = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t _cd_switch = PTHREAD_COND_INITIALIZER;
static int _start_switch = 0;

/* -------------------------------------------------------------------------- */

static void _count_free(UNUSED Variant v)
{
    _freed ++;
}

/* -------------------------------------------------------------------------- */

static int _bytes_cmp(
    const unsigned char *a,
    size_t alen,
    const unsigned char *b,
    size_t blen
)
{
    size_t n = (alen < blen) ? alen : blen;
    int r = memcmp(a, b, n);

    if (r) return r;

    return (alen > blen) - (alen < blen);
}

/* -------------------------------------------------------------------------- */

static int _key_cmp(
    const char *key0,
    size_t len0,
    UNUSED Variant val0,
    const char *key1,
    size_t len1,
    UNUSED Variant val1
)
{
    return _bytes_cmp(
        (const unsigned char *) key0, len0,
        (const unsigned char *) key1, len1
    );
}

/* -------------------------------------------------------------------------- */

static int _value_cmp(
    const char *key0,
    size_t len0,
    Variant val0,
    const char *key1,
    size_t len1,
    Variant val1
)
{
    uint64_t v0 = variant_to_integer(val0), v1 = variant_to_integer(val1);

    if (v0 < v1) return -1;
    if (v0 > v1) return 1;

    return _key_cmp(key0, len0, val0, key1, len1, val1);
}

/* -------------------------------------------------------------------------- */

static int _entry_cmp(const void *pa, const void *pb)
{
    const _Entry *a = *(const _Entry * const *) pa;
    const _Entry *b = *(const _Entry * const *) pb;
    int r = 0;

    if (_sort_mode == SORT_VALUES) r = (a->val > b->val) - (a->val < b->val);
    if (! r) r = _bytes_cmp(a->key, a->len, b->key, b->len);

    return (_sort_desc) ? -r : r;
}

/* -------------------------------------------------------------------------- */

static void _universe(_Entry *u, unsigned int n, char base)
{
    /** @brief build keys with shared prefixes, NUL bytes and a unique tail */

    unsigned int i = 0, j = 0;

    for (i = 0; i < n; i ++) {
        u[i].len = 4 + rand() % (sizeof(u[i].key) - 3);
        u[i].key[0] = base + (i % 8);
        u[i].key[1] = (i % 5) ? 0 : 'x';
        for (j = 2; j < u[i].len - 2; j ++)
            u[i].key[j] = (rand() % 5) ? rand() & 0xff : 0;
        u[i].key[u[i].len - 2] = i & 0xff;
        u[i].key[u[i].len - 1] = (i >> 8) & 0xff;
        u[i].val = 0;
        u[i].alive = 0;
    }
}

/* -------------------------------------------------------------------------- */

static unsigned int _live(
    _Entry *u,
    unsigned int n,
    _Entry **out,
    int mode,
    int desc
)
{
    /** @brief collect the live entries of a universe, in the given order */

    unsigned int i = 0, count = 0;

    for (i = 0; i < n; i ++) if (u[i].alive) out[count ++] = & u[i];

    _sort_mode = mode; _sort_desc = desc;
    qsort(out, count, sizeof(*out), _entry_cmp);

    return count;
}

/* -------------------------------------------------------------------------- */

static int _check_order(
    Map *h,
    _Entry *u,
    unsigned int n,
    int mode,
    int desc,
    const char *where
)
{
    /** @brief a fresh traversal must match the sorted live entries */

    _Entry *live[256];
    Map_Iterator *it = NULL;
    unsigned int count = _live(u, n, live, mode, desc), i = 0;

    for (it = map_each(h); it; it = map_next(it), i ++) {
        if (i >= count) {
            test_fail(__FILE__, __LINE__, "%s: traversal too long", where);
            map_break(it);
            return -1;
        }
        if (it->len != live[i]->len ||
            memcmp(it->key, live[i]->key, it->len)) {
            test_fail(__FILE__, __LINE__, "%s: key %u out of order",
                      where, i);
            map_break(it);
            return -1;
        }
        if (! is_integer(it->val) ||
            variant_to_integer(it->val) != live[i]->val) {
            test_fail(__FILE__, __LINE__, "%s: wrong value at %u",
                      where, i);
            map_break(it);
            return -1;
        }
    }

    if (i != count) {
        test_fail(__FILE__, __LINE__, "%s: traversal too short (%u of %u)",
                  where, i, count);
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _check_from_at(
    Map *h,
    _Entry *u,
    unsigned int n,
    int mode,
    int desc,
    unsigned int seed,
    const char *where
)
{
    /** @brief map_at() must continue the sorted order from its key */

    _Entry *live[256];
    Map_Iterator *it = NULL;
    unsigned int count = _live(u, n, live, mode, desc), start = 0, i = 0;

    if (! count) return 0;

    start = seed % count;
    it = map_at(h, (const char *) live[start]->key, live[start]->len);
    if (! it) {
        test_fail(__FILE__, __LINE__, "%s: map_at() found nothing", where);
        return -1;
    }

    for (i = start; it; it = map_next(it), i ++) {
        if (i >= count ||
            it->len != live[i]->len ||
            memcmp(it->key, live[i]->key, it->len) ||
            variant_to_integer(it->val) != live[i]->val) {
            test_fail(__FILE__, __LINE__, "%s: mismatch at %u", where, i);
            map_break(it);
            return -1;
        }
    }

    if (i != count) {
        test_fail(__FILE__, __LINE__, "%s: stopped at %u of %u",
                  where, i, count);
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _check_membership(Map *h, _Entry *u, unsigned int n)
{
    /** @brief the map holds exactly the live entries, by traversal and
        by lookup */

    static unsigned char seen[256];
    Map_Iterator *it = NULL;
    Variant v = { 0 };
    unsigned int i = 0, count = 0, live = 0;

    memset(seen, 0, n);

    for (it = map_each(h); it; it = map_next(it)) {
        for (i = 0; i < n; i ++) {
            if (u[i].alive && u[i].len == it->len &&
                ! memcmp(u[i].key, it->key, it->len)) break;
        }
        if (i == n || seen[i]) {
            test_fail(__FILE__, __LINE__, "%s key in traversal",
                      (i == n) ? "unknown" : "duplicate");
            map_break(it);
            return -1;
        }
        seen[i] = 1; count ++;
        if (! is_integer(it->val) || variant_to_integer(it->val) != u[i].val) {
            test_fail(__FILE__, __LINE__, "wrong value for key %u", i);
            map_break(it);
            return -1;
        }
    }

    for (i = 0; i < n; i ++) {
        live += u[i].alive;
        v = map_get(h, (const char *) u[i].key, u[i].len);
        if (map_has(h, (const char *) u[i].key, u[i].len) != u[i].alive ||
            is_integer(v) != u[i].alive ||
            (u[i].alive && variant_to_integer(v) != u[i].val)) {
            test_fail(__FILE__, __LINE__, "lookup of key %u: wrong", i);
            return -1;
        }
    }

    if (count != live) {
        test_fail(__FILE__, __LINE__, "%u entries traversed, %u live",
                  count, live);
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static unsigned int _count(Map *h)
{
    Map_Iterator *it = NULL;
    unsigned int count = 0;

    for (it = map_each(h); it; it = map_next(it)) count ++;

    return count;
}

/* -------------------------------------------------------------------------- */

static void _fill(Map *h, unsigned int n)
{
    char key[32];
    unsigned int i = 0;
    int len = 0;

    for (i = 1; i <= n; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        map_set(h, key, len, variant_from_integer(i));
    }
}

/* -------------------------------------------------------------------------- */

static int _set_get_remove(void)
{
    Map *h = NULL;
    Variant v = { 0 };
    size_t footprint = 0, overhead = 0;

    ASSERT_NOT_NULL(h = map_alloc(NULL));

    /* empty map */
    ASSERT_TRUE(is_null(map_get(h, "key", 3)));
    ASSERT_EQ_INT(map_has(h, "key", 3), 0);
    ASSERT_TRUE(is_null(map_remove(h, "key", 3)));
    ASSERT_NULL(map_each(h));
    ASSERT_NULL(map_at(h, "key", 3));

    /* insert, remove, insert again, read back */
    ASSERT_TRUE(is_null(map_insert(h, "key", 3, variant_from_integer(86))));
    ASSERT_EQ_UINT(variant_to_integer(map_remove(h, "key", 3)), 86);
    ASSERT_TRUE(is_null(map_insert(h, "key", 3, variant_from_integer(68))));
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "key", 3)), 68);
    ASSERT_EQ_INT(map_has(h, "key", 3), 1);

    /* overwrite: the displaced value comes back */
    v = map_set(h, "key", 3, variant_from_integer(0xc0ffee));
    ASSERT_EQ_UINT(variant_to_integer(v), 68);
    v = map_set(h, "key", 3, variant_from_integer(0xcafe));
    ASSERT_EQ_UINT(variant_to_integer(v), 0xc0ffee);
    v = map_remove(h, "key", 3);
    ASSERT_EQ_UINT(variant_to_integer(v), 0xcafe);
    ASSERT_EQ_INT(map_has(h, "key", 3), 0);
    ASSERT_TRUE(is_null(map_get(h, "key", 3)));

    /* the key is compared by length, not as a C string */
    map_set(h, "key", 3, variant_from_integer(1));
    ASSERT_TRUE(is_null(map_get(h, "ke", 2)));
    ASSERT_TRUE(is_null(map_get(h, "keys", 4)));
    ASSERT_EQ_INT(map_has(h, "key", 3), 1);

    /* reserving capacity keeps the content */
    map_reserve(h, 4096);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "key", 3)), 1);
    ASSERT_EQ_UINT(_count(h), 1);

    footprint = map_footprint(h, & overhead);
    ASSERT_TRUE(footprint > overhead);
    ASSERT_TRUE(overhead > 0);

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

static Variant _keep_old(
    UNUSED const char *k,
    UNUSED size_t l,
    Variant old,
    UNUSED Variant new
)
{
    return old;
}

/* -------------------------------------------------------------------------- */

static Variant _take_new(
    UNUSED const char *k,
    UNUSED size_t l,
    UNUSED Variant old,
    Variant new
)
{
    return new;
}

/* -------------------------------------------------------------------------- */

static Variant _third_value(
    UNUSED const char *k,
    UNUSED size_t l,
    UNUSED Variant old,
    UNUSED Variant new
)
{
    return variant_from_integer(333);
}

/* -------------------------------------------------------------------------- */

static Variant _double(UNUSED const char *k, UNUSED size_t l, Variant new)
{
    return variant_from_integer(2 * variant_to_integer(new));
}

/* -------------------------------------------------------------------------- */

static Variant _plus_one(Variant v)
{
    return variant_from_integer(variant_to_integer(v) + 1);
}

/* -------------------------------------------------------------------------- */

static int _is_even(UNUSED const char *k, UNUSED size_t l, Variant v)
{
    return ! (variant_to_integer(v) & 1);
}

/* -------------------------------------------------------------------------- */

static int _return_contracts(void)
{
    /* who owns what after each call: the map keeps one value, the caller
       gets the other one back, and nothing is returned twice */

    Map *h = NULL;
    Variant r = { 0 };

    ASSERT_NOT_NULL(h = map_alloc(NULL));

    /* insert: null on success, the proposed value on a duplicate */
    r = map_insert(h, "a", 1, variant_from_integer(1));
    ASSERT_TRUE(is_null(r));
    r = map_insert(h, "a", 1, variant_from_integer(2));
    ASSERT_EQ_UINT(variant_to_integer(r), 2);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "a", 1)), 1);

    /* set: the displaced value, null when nothing changed */
    r = map_set(h, "a", 1, variant_from_integer(2));
    ASSERT_EQ_UINT(variant_to_integer(r), 1);
    r = map_set(h, "a", 1, variant_from_integer(2));
    ASSERT_TRUE(is_null(r));

    /* update: the proposed value on a missing key, the old one otherwise */
    r = map_update(h, "missing", 7, variant_from_integer(9));
    ASSERT_EQ_UINT(variant_to_integer(r), 9);
    ASSERT_EQ_INT(map_has(h, "missing", 7), 0);
    r = map_update(h, "a", 1, variant_from_integer(10));
    ASSERT_EQ_UINT(variant_to_integer(r), 2);
    r = map_update(h, "a", 1, variant_from_integer(10));
    ASSERT_TRUE(is_null(r));

    /* set_with: the callback decides what is kept */
    r = map_set_with(h, "a", 1, variant_from_integer(20), _keep_old);
    ASSERT_EQ_UINT(variant_to_integer(r), 20);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "a", 1)), 10);
    r = map_set_with(h, "a", 1, variant_from_integer(30), _take_new);
    ASSERT_EQ_UINT(variant_to_integer(r), 10);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "a", 1)), 30);
    r = map_set_with(h, "a", 1, variant_from_integer(40), _third_value);
    ASSERT_EQ_UINT(variant_to_integer(r), 40);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "a", 1)), 333);

    /* update_with: same contract, never inserts */
    r = map_update_with(h, "a", 1, variant_from_integer(50), _keep_old);
    ASSERT_EQ_UINT(variant_to_integer(r), 50);
    r = map_update_with(h, "a", 1, variant_from_integer(50), _take_new);
    ASSERT_EQ_UINT(variant_to_integer(r), 333);
    r = map_update_with(h, "a", 1, variant_from_integer(60), _third_value);
    ASSERT_EQ_UINT(variant_to_integer(r), 60);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "a", 1)), 333);
    r = map_update_with(h, "b", 1, variant_from_integer(70), _take_new);
    ASSERT_EQ_UINT(variant_to_integer(r), 70);
    ASSERT_EQ_INT(map_has(h, "b", 1), 0);

    /* insert_with: the callback only runs for a new key */
    r = map_insert_with(h, "b", 1, variant_from_integer(4), _double);
    ASSERT_TRUE(is_null(r));
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "b", 1)), 8);
    r = map_insert_with(h, "b", 1, variant_from_integer(5), _double);
    ASSERT_EQ_UINT(variant_to_integer(r), 5);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "b", 1)), 8);

    /* get_with: the callback's result is returned, the value stays */
    r = map_get_with(h, "b", 1, _plus_one);
    ASSERT_EQ_UINT(variant_to_integer(r), 9);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "b", 1)), 8);
    ASSERT_TRUE(is_null(map_get_with(h, "c", 1, _plus_one)));

    /* remove_if: the predicate must agree */
    r = map_remove_if(h, "a", 1, _is_even);
    ASSERT_TRUE(is_null(r));
    ASSERT_EQ_INT(map_has(h, "a", 1), 1);
    r = map_remove_if(h, "b", 1, _is_even);
    ASSERT_EQ_UINT(variant_to_integer(r), 8);
    ASSERT_EQ_INT(map_has(h, "b", 1), 0);
    ASSERT_TRUE(is_null(map_remove_if(h, "b", 1, _is_even)));

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _volume(void)
{
    static unsigned char present[KEYS + 1];
    Map *h = NULL;
    Map_Iterator *it = NULL;
    Variant v = { 0 };
    char key[32], last[32];
    unsigned int i = 0, n = 0, removed = 0, count = 0;
    int len = 0, lastlen = 0;

    ASSERT_NOT_NULL(h = map_alloc(NULL));
    _fill(h, KEYS);
    memset(present, 1, sizeof(present));

    for (i = 1; i <= KEYS; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        v = map_get(h, key, len);
        if (! is_integer(v) || variant_to_integer(v) != i) {
            test_fail(__FILE__, __LINE__, "%s: missing after fill", key);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);

    /* remove a random share of the keys, some of them twice */
    for (i = 0; i < KEYS / 2; i ++) {
        n = 1 + rand() % KEYS;
        len = snprintf(key, sizeof(key), "%u", n);
        v = map_remove(h, key, len);
        if (is_integer(v) != present[n]) {
            test_fail(__FILE__, __LINE__, "%s: %s", key,
                      present[n] ? "not removed" : "removed twice");
            break;
        }
        removed += present[n]; present[n] = 0;
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_TRUE(removed > KEYS / 4);

    /* a persistent key order: the traversal is sorted and complete */
    ASSERT_EQ_INT(map_sort(h, MAP_ASC, map_sort_keys), 0);
    lastlen = 0;
    for (it = map_each(h); it; it = map_next(it), count ++) {
        if (lastlen && _bytes_cmp((unsigned char *) last, lastlen,
                                  (unsigned char *) it->key, it->len) >= 0) {
            test_fail(__FILE__, __LINE__, "%.*s after %.*s",
                      (int) it->len, it->key, lastlen, last);
            map_break(it);
            break;
        }
        memcpy(last, it->key, it->len); lastlen = it->len;
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_UINT(count, KEYS - removed);

    /* replace every value: absent keys are inserted back */
    for (i = 1; i <= KEYS; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        v = map_set(h, key, len, variant_from_integer(i + 1));
        if (is_integer(v) != present[i] ||
            (present[i] && variant_to_integer(v) != i)) {
            test_fail(__FILE__, __LINE__, "%s: wrong value replaced", key);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_UINT(_count(h), KEYS);

    /* remove everything, then nothing is left */
    for (i = 1; i <= KEYS; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        v = map_remove(h, key, len);
        if (! is_integer(v) || variant_to_integer(v) != i + 1) {
            test_fail(__FILE__, __LINE__, "%s: wrong value removed", key);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    for (i = 1; i <= KEYS; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        if (map_has(h, key, len)) {
            test_fail(__FILE__, __LINE__, "%s: phantom key", key);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_NULL(map_each(h));

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _delete_all(
    UNUSED const char *k,
    UNUSED size_t l,
    UNUSED Variant v,
    void *context
)
{
    (*(unsigned int *) context) ++;
    return -1;
}

/* -------------------------------------------------------------------------- */

static int _stop_at_second(
    UNUSED const char *k,
    UNUSED size_t l,
    UNUSED Variant v,
    void *context
)
{
    return (++ (*(unsigned int *) context) == 2);
}

/* -------------------------------------------------------------------------- */

static int _collect_first(
    const char *k,
    UNUSED size_t l,
    UNUSED Variant v,
    void *context
)
{
    *(unsigned int *) context = (unsigned char) k[0];
    return 1;
}

/* -------------------------------------------------------------------------- */

static int _sort_and_foreach(void)
{
    static const char *keys[] = { "zzzzz", "tedst", "testa", "btest", "tcest" };
    static const char *asc[] = { "btest", "tcest", "tedst", "testa", "zzzzz" };
    Map *h = NULL;
    Map_Iterator *it = NULL;
    unsigned int i = 0, calls = 0;

    ASSERT_NOT_NULL(h = map_alloc(_count_free));
    _freed = 0;

    for (i = 0; i < 5; i ++)
        map_set(h, keys[i], 5, variant_from_integer(i));

    /* ascending, then descending, each a fresh traversal */
    ASSERT_EQ_INT(map_sort(h, MAP_ASC, map_sort_keys), 0);
    for (it = map_each(h), i = 0; it; it = map_next(it), i ++) {
        if (i >= 5 || it->len != 5 || memcmp(it->key, asc[i], 5)) {
            test_fail(__FILE__, __LINE__, "ascending: %.*s at %u",
                      (int) it->len, it->key, i);
            map_break(it);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_UINT(i, 5);

    ASSERT_EQ_INT(map_sort(h, MAP_DESC, map_sort_keys), 0);
    for (it = map_each(h), i = 0; it; it = map_next(it), i ++) {
        if (i >= 5 || it->len != 5 || memcmp(it->key, asc[4 - i], 5)) {
            test_fail(__FILE__, __LINE__, "descending: %.*s at %u",
                      (int) it->len, it->key, i);
            map_break(it);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_UINT(i, 5);

    /* a callback returning 1 stops the traversal */
    calls = 0;
    map_foreach(h, _stop_at_second, & calls);
    ASSERT_EQ_UINT(calls, 2);
    ASSERT_EQ_UINT(_count(h), 5);

    /* a callback returning -1 deletes the entry, through the destructor */
    calls = 0;
    map_foreach(h, _delete_all, & calls);
    ASSERT_EQ_UINT(calls, 5);
    ASSERT_EQ_UINT(_freed, 5);
    ASSERT_NULL(map_each(h));

    /* the persistent order survives an empty map and new entries, and
       map_foreach() restores it as well */
    map_set(h, "btest", 5, variant_from_integer(4));
    map_set(h, "tcest", 5, variant_from_integer(8));
    calls = 0;
    map_foreach(h, _collect_first, & calls);
    ASSERT_EQ_UINT(calls, 't');
    ASSERT_NOT_NULL(it = map_each(h));
    ASSERT_EQ_MEM(it->key, "tcest", 5);
    ASSERT_NOT_NULL(it = map_next(it));
    ASSERT_EQ_MEM(it->key, "btest", 5);
    ASSERT_NULL(map_next(it));

    /* a run appended past the sorted ones is merged after them */
    map_set(h, "aaaaa", 5, variant_from_integer(1));
    map_set(h, "AAAAA", 5, variant_from_integer(2));
    ASSERT_NOT_NULL(it = map_each(h));
    ASSERT_EQ_MEM(it->key, "tcest", 5);
    ASSERT_NOT_NULL(it = map_next(it));
    ASSERT_EQ_MEM(it->key, "btest", 5);
    ASSERT_NOT_NULL(it = map_next(it));
    ASSERT_EQ_MEM(it->key, "aaaaa", 5);
    ASSERT_NOT_NULL(it = map_next(it));
    ASSERT_EQ_MEM(it->key, "AAAAA", 5);
    ASSERT_NULL(map_next(it));

    /* a stale order with no live entry left */
    map_set(h, "AAAAA", 5, variant_from_integer(3));
    map_remove(h, "tcest", 5);
    map_remove(h, "btest", 5);
    map_remove(h, "aaaaa", 5);
    map_remove(h, "AAAAA", 5);
    ASSERT_NULL(map_each(h));
    map_set(h, "btest", 5, variant_from_integer(4));
    map_set(h, "tcest", 5, variant_from_integer(8));

    /* the destructor runs for what is left */
    ASSERT_NULL(map_free(h));
    ASSERT_EQ_UINT(_freed, 7);

    return 0;
}

/* -------------------------------------------------------------------------- */

static Variant _overwrite(
    UNUSED const char *key,
    UNUSED size_t len,
    UNUSED Variant dest,
    Variant src
)
{
    return src;
}

/* -------------------------------------------------------------------------- */

static Variant _keep_dest(
    UNUSED const char *key,
    UNUSED size_t len,
    Variant dest,
    UNUSED Variant src
)
{
    return dest;
}

/* -------------------------------------------------------------------------- */

static int _merge_and_at(void)
{
    static const char *merged[] = { "A", "B", "C", "D", "E", "F", "G", "H" };
    Map *h = NULL, *h2 = NULL;
    Map_Iterator *it = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(h = map_alloc(NULL));
    ASSERT_NOT_NULL(h2 = map_alloc(NULL));

    for (i = 0; i < 4; i ++)
        map_set(h, merged[i], 1, variant_from_integer(i + 1));
    map_set(h2, "D", 1, variant_from_integer(0x44));
    for (i = 4; i < 8; i ++)
        map_set(h2, merged[i], 1, variant_from_integer(i + 1));

    /* the source is consumed, the conflict goes to the callback */
    ASSERT_EQ_INT(map_merge(h, h2, _overwrite), 0);
    ASSERT_EQ_UINT(_count(h), 8);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "D", 1)), 0x44);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "A", 1)), 1);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "H", 1)), 8);

    /* map_at() continues the traversal from its key */
    ASSERT_EQ_INT(map_sort(h, MAP_ASC, map_sort_keys), 0);
    ASSERT_NULL(map_at(h, "Z", 1));
    for (it = map_at(h, "D", 1), i = 3; it; it = map_next(it), i ++) {
        if (i >= 8 || it->len != 1 || it->key[0] != merged[i][0]) {
            test_fail(__FILE__, __LINE__, "from D: %.*s at %u",
                      (int) it->len, it->key, i);
            map_break(it);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_UINT(i, 8);

    /* a discarded source value goes through the source's destructor */
    ASSERT_NOT_NULL(h2 = map_alloc(_count_free));
    map_set(h2, "D", 1, variant_from_integer(0x55));
    map_set(h2, "I", 1, variant_from_integer(9));
    _freed = 0;
    ASSERT_EQ_INT(map_merge(h, h2, _keep_dest), 0);
    ASSERT_EQ_UINT(_freed, 1);
    ASSERT_EQ_UINT(_count(h), 9);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "D", 1)), 0x44);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, "I", 1)), 9);

    /* breaking an iterator releases the map for writers */
    ASSERT_NOT_NULL(it = map_each(h));
    ASSERT_NULL(map_break(it));
    map_set(h, "J", 1, variant_from_integer(10));
    ASSERT_EQ_UINT(_count(h), 10);

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _embedded_nul_keys(void)
{
    /* keys are byte strings: a prefix, a NUL extension and a NUL start are
       all distinct entries */

    static const unsigned char k1[] = { 'a', '\0', 'x' };
    static const unsigned char k2[] = { 'a', '\0', 'y' };
    static const unsigned char k3[] = { 'a' };
    static const unsigned char k4[] = { 'a', '\0' };
    static const unsigned char k5[] = { '\0', 'z' };
    static const unsigned char *keys[] = { k1, k2, k3, k4, k5 };
    static const size_t lens[] = { 3, 3, 1, 2, 2 };
    Map *h = NULL;
    Map_Iterator *it = NULL;
    Variant v = { 0 };
    unsigned int i = 0;

    ASSERT_NOT_NULL(h = map_alloc(NULL));

    for (i = 0; i < 5; i ++)
        map_set(h, (const char *) keys[i], lens[i], variant_from_integer(i));
    ASSERT_EQ_UINT(_count(h), 5);

    for (i = 0; i < 5; i ++) {
        v = map_get(h, (const char *) keys[i], lens[i]);
        if (! is_integer(v) || variant_to_integer(v) != i) {
            test_fail(__FILE__, __LINE__, "key %u: wrong value", i);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);

    /* removing one leaves its neighbours intact */
    v = map_remove(h, (const char *) k2, sizeof(k2));
    ASSERT_EQ_UINT(variant_to_integer(v), 1);
    ASSERT_EQ_INT(map_has(h, (const char *) k2, sizeof(k2)), 0);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, (const char *) k1, 3)), 0);
    ASSERT_EQ_UINT(variant_to_integer(map_get(h, (const char *) k4, 2)), 3);

    /* the byte order puts the NUL start first and the prefix before
       its extensions */
    ASSERT_EQ_INT(map_sort(h, MAP_ASC, map_sort_keys), 0);
    ASSERT_NOT_NULL(it = map_each(h));
    ASSERT_EQ_UINT(it->len, 2);
    ASSERT_EQ_MEM(it->key, k5, 2);
    ASSERT_NOT_NULL(it = map_next(it));
    ASSERT_EQ_UINT(it->len, 1);
    ASSERT_EQ_MEM(it->key, k3, 1);
    ASSERT_NOT_NULL(it = map_next(it));
    ASSERT_EQ_UINT(it->len, 2);
    ASSERT_EQ_MEM(it->key, k4, 2);
    ASSERT_NOT_NULL(it = map_next(it));
    ASSERT_EQ_UINT(it->len, 3);
    ASSERT_EQ_MEM(it->key, k1, 3);
    ASSERT_NULL(map_next(it));

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _random_differential(void)
{
    /* random operations on a universe of 256 keys against a reference
       model, with the traversal, lookups and sort order checked along
       the way */

    static _Entry u[256];
    Map *h = NULL;
    Variant got = { 0 };
    uint64_t val = 0;
    unsigned int op = 0, which = 0, ret = 0;

    _universe(u, 256, 'A');
    ASSERT_NOT_NULL(h = map_alloc(NULL));

    for (op = 0; op < 50000; op ++) {
        which = rand() % 256;
        val = ((uint64_t) op << 16) ^ which;

        switch (rand() % 10) {
        case 0: case 1: case 2:
            map_set(h, (const char *) u[which].key, u[which].len,
                    variant_from_integer(val));
            u[which].alive = 1; u[which].val = val;
            break;
        case 3:
            got = map_insert(h, (const char *) u[which].key, u[which].len,
                             variant_from_integer(val));
            if (u[which].alive) {
                ASSERT_EQ_UINT(variant_to_integer(got), val);
            } else {
                ASSERT_TRUE(is_null(got));
                u[which].alive = 1; u[which].val = val;
            }
            break;
        case 4:
            got = map_update(h, (const char *) u[which].key, u[which].len,
                             variant_from_integer(val));
            if (u[which].alive) {
                if (u[which].val == val) ASSERT_TRUE(is_null(got));
                else ASSERT_EQ_UINT(variant_to_integer(got), u[which].val);
                u[which].val = val;
            } else ASSERT_EQ_UINT(variant_to_integer(got), val);
            break;
        case 5: case 6:
            got = map_remove(h, (const char *) u[which].key, u[which].len);
            if (u[which].alive) {
                ASSERT_EQ_UINT(variant_to_integer(got), u[which].val);
                u[which].alive = 0;
            } else ASSERT_TRUE(is_null(got));
            break;
        case 7: case 8:
            got = map_get(h, (const char *) u[which].key, u[which].len);
            ASSERT_EQ_INT(is_integer(got), u[which].alive);
            if (u[which].alive)
                ASSERT_EQ_UINT(variant_to_integer(got), u[which].val);
            ASSERT_EQ_INT(map_has(h, (const char *) u[which].key,
                                  u[which].len), u[which].alive);
            break;
        default:
            ret = rand() & 1;
            ASSERT_EQ_INT(map_sort(h, ret ? MAP_DESC : MAP_ASC,
                                   map_sort_keys), 0);
            ASSERT_EQ_INT(_check_order(h, u, 256, SORT_KEYS, ret, "sort"), 0);
            break;
        }

        if (! (op % 250)) ASSERT_EQ_INT(_check_membership(h, u, 256), 0);
    }

    ASSERT_EQ_INT(_check_membership(h, u, 256), 0);
    ASSERT_EQ_INT(map_sort(h, MAP_ASC, map_sort_keys), 0);
    ASSERT_EQ_INT(_check_order(h, u, 256, SORT_KEYS, 0, "final asc"), 0);
    ASSERT_EQ_INT(map_sort(h, MAP_DESC, map_sort_keys), 0);
    ASSERT_EQ_INT(_check_order(h, u, 256, SORT_KEYS, 1, "final desc"), 0);

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _sticky_sort(void)
{
    /* a persistent order is restored lazily before the next traversal, a
       one-shot sort does not replace it */

    static _Entry u[128];
    Map *h = NULL;
    uint64_t val = 0;
    unsigned int op = 0, i = 0;

    _universe(u, 128, 'a');
    ASSERT_NOT_NULL(h = map_alloc(NULL));

    for (i = 0; i < 128; i += 2) {
        val = 1000 + i;
        map_set(h, (const char *) u[i].key, u[i].len,
                variant_from_integer(val));
        u[i].alive = 1; u[i].val = val;
    }

    ASSERT_EQ_INT(map_sort(h, MAP_ASC, _key_cmp), 0);
    ASSERT_EQ_INT(_check_order(h, u, 128, SORT_KEYS, 0, "keys asc"), 0);

    /* mutations without traversal, then the order must be back */
    for (op = 0; op < 1000; op ++) {
        i = rand() % 128;
        val = ((uint64_t) op << 16) ^ i;
        if (rand() % 4 == 3) {
            map_remove(h, (const char *) u[i].key, u[i].len);
            u[i].alive = 0;
        } else {
            map_set(h, (const char *) u[i].key, u[i].len,
                    variant_from_integer(val));
            u[i].alive = 1; u[i].val = val;
        }
    }
    ASSERT_EQ_INT(_check_from_at(h, u, 128, SORT_KEYS, 0, 17,
                                 "keys asc, map_at"), 0);
    ASSERT_EQ_INT(_check_order(h, u, 128, SORT_KEYS, 0,
                               "keys asc, mutated"), 0);

    /* a new persistent policy, by value, then value updates */
    ASSERT_EQ_INT(map_sort(h, MAP_DESC, _value_cmp), 0);
    ASSERT_EQ_INT(_check_order(h, u, 128, SORT_VALUES, 1, "values desc"), 0);
    for (op = 0; op < 1000; op ++) {
        i = rand() % 128;
        val = ((uint64_t) rand() << 32) ^ op;
        map_set(h, (const char *) u[i].key, u[i].len,
                variant_from_integer(val));
        u[i].alive = 1; u[i].val = val;
    }
    ASSERT_EQ_INT(_check_from_at(h, u, 128, SORT_VALUES, 1, 23,
                                 "values desc, map_at"), 0);
    ASSERT_EQ_INT(_check_order(h, u, 128, SORT_VALUES, 1,
                               "values desc, updated"), 0);

    /* a one-shot sort by key, kept across removals */
    ASSERT_EQ_INT(map_sort(h, MAP_DESC | MAP_SORT_ONCE, _key_cmp), 0);
    ASSERT_EQ_INT(_check_order(h, u, 128, SORT_KEYS, 1, "one-shot"), 0);
    for (op = 0; op < 32; op ++) {
        i = rand() % 128;
        map_remove(h, (const char *) u[i].key, u[i].len);
        u[i].alive = 0;
    }
    ASSERT_EQ_INT(_check_order(h, u, 128, SORT_KEYS, 1,
                               "one-shot, removals"), 0);

    /* an insertion makes the persistent policy take over again */
    for (i = 0; i < 128 && u[i].alive; i ++);
    ASSERT_TRUE(i < 128);
    val = UINT64_MAX - i;
    map_set(h, (const char *) u[i].key, u[i].len, variant_from_integer(val));
    u[i].alive = 1; u[i].val = val;
    ASSERT_EQ_INT(_check_order(h, u, 128, SORT_VALUES, 1,
                               "values desc, restored"), 0);

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _sticky_sort_active_iterator(void)
{
    /* changing a value under an active iterator does not relink it; the
       next fresh traversal sees the new order */

    static const char *tail[] = { "B", "C", "D", "E" };
    static const char *fresh[] = { "B", "C", "D", "E", "A" };
    Map *h = NULL;
    Map_Iterator *it = NULL;
    Variant old = { 0 };
    unsigned int i = 0;

    ASSERT_NOT_NULL(h = map_alloc(NULL));
    for (i = 0; i < 5; i ++)
        map_set(h, fresh[(i + 4) % 5], 1, variant_from_integer(i + 1));

    ASSERT_EQ_INT(map_sort(h, MAP_ASC, _value_cmp), 0);
    ASSERT_NOT_NULL(it = map_each(h));
    ASSERT_EQ_MEM(it->key, "A", 1);

    old = map_set_at(it, variant_from_integer(100));
    ASSERT_EQ_UINT(variant_to_integer(old), 1);
    ASSERT_EQ_UINT(variant_to_integer(it->val), 100);

    for (it = map_next(it), i = 0; it; it = map_next(it), i ++) {
        if (i >= 4 || it->len != 1 || it->key[0] != tail[i][0]) {
            test_fail(__FILE__, __LINE__, "active: %.*s at %u",
                      (int) it->len, it->key, i);
            map_break(it);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_UINT(i, 4);

    for (it = map_each(h), i = 0; it; it = map_next(it), i ++) {
        if (i >= 5 || it->len != 1 || it->key[0] != fresh[i][0]) {
            test_fail(__FILE__, __LINE__, "fresh: %.*s at %u",
                      (int) it->len, it->key, i);
            map_break(it);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_UINT(i, 5);

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

static void _wait_for_start(void)
{
    pthread_mutex_lock(& _mx_switch);
    while (! _start_switch) pthread_cond_wait(& _cd_switch, & _mx_switch);
    pthread_mutex_unlock(& _mx_switch);
}

/* -------------------------------------------------------------------------- */

static void _start(void)
{
    pthread_mutex_lock(& _mx_switch);
    _start_switch = 1;
    pthread_cond_broadcast(& _cd_switch);
    pthread_mutex_unlock(& _mx_switch);
}

/* -------------------------------------------------------------------------- */

typedef struct _Worker {
    Map *map;
    unsigned int id;
    unsigned int ops;
    unsigned int errors;
    _ATOMIC int stop;
    uint64_t max_latency;
    uint64_t timeouts;
} _Worker;

#define WRITERS 8
#define WRITES  5000

/* -------------------------------------------------------------------------- */

static void *_writer(void *arg)
{
    _Worker *w = arg;
    char key[32];
    unsigned int op = 0;
    int len = 0;

    _wait_for_start();

    for (op = 0; op < WRITES; op ++) {
        len = snprintf(key, sizeof(key), "key_%u_%u", w->id, op);
        map_set(w->map, key, len,
                variant_from_integer((uint64_t) w->id * 1000000 + op));
        w->ops ++;
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _write_stress(void)
{
    /* concurrent writers on disjoint keys: every write lands */

    static _Worker w[WRITERS];
    pthread_t threads[WRITERS];
    Map *h = NULL;
    unsigned int i = 0, ops = 0;

    ASSERT_NOT_NULL(h = map_alloc(NULL));
    _start_switch = 0;

    for (i = 0; i < WRITERS; i ++) {
        memset(& w[i], 0, sizeof(w[i]));
        w[i].map = h; w[i].id = i;
        ASSERT_EQ_INT(pthread_create(& threads[i], NULL, _writer, & w[i]), 0);
    }

    _start();

    for (i = 0; i < WRITERS; i ++) {
        pthread_join(threads[i], NULL);
        ops += w[i].ops;
    }

    ASSERT_EQ_UINT(ops, WRITERS * WRITES);
    ASSERT_EQ_UINT(_count(h), WRITERS * WRITES);

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

#define ITEMS 10000

static void *_iterator_reader(void *arg)
{
    _Worker *w = arg;
    Map_Iterator *it = NULL;
    unsigned int pass = 0, count = 0;

    _wait_for_start();

    for (pass = 0; pass < 3; pass ++) {
        for (it = map_each(w->map), count = 0; it; it = map_next(it)) {
            if (! is_integer(it->val)) {
                w->errors ++;
                map_break(it);
                return NULL;
            }
            if (! (++ count % 500)) usleep(10);
        }
        w->ops += count;
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static void *_iterator_updater(void *arg)
{
    _Worker *w = arg;
    Map_Iterator *it = NULL;
    Variant old = { 0 };
    uint64_t value = 0;
    unsigned int pass = 0, count = 0;

    _wait_for_start();

    for (pass = 0; pass < 2; pass ++) {
        for (it = map_each(w->map), count = 0; it; it = map_next(it)) {
            count ++;
            if (! is_integer(it->val)) {
                w->errors ++;
                map_break(it);
                return NULL;
            }
            if (count % 10 != w->id) continue;

            /* the iterator must see its own update */
            value = variant_to_integer(it->val);
            old = map_set_at(it, variant_from_integer(value + 2000000));
            if (! is_integer(old) || variant_to_integer(old) != value ||
                variant_to_integer(it->val) != value + 2000000)
                w->errors ++;
            w->ops ++;

            if (! (count % 100)) usleep(1);
        }
        if (count != ITEMS) w->errors ++;
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _iterator_concurrency(void)
{
    /* two readers and two updaters traverse the same map at once */

    static _Worker w[4];
    pthread_t threads[4];
    Map *h = NULL;
    unsigned int i = 0, errors = 0;

    ASSERT_NOT_NULL(h = map_alloc(NULL));
    _fill(h, ITEMS);
    _start_switch = 0;

    for (i = 0; i < 4; i ++) {
        memset(& w[i], 0, sizeof(w[i]));
        w[i].map = h; w[i].id = i;
        ASSERT_EQ_INT(pthread_create(
            & threads[i], NULL,
            (i < 2) ? _iterator_reader : _iterator_updater, & w[i]
        ), 0);
    }

    _start();

    for (i = 0; i < 4; i ++) {
        pthread_join(threads[i], NULL);
        errors += w[i].errors;
    }

    ASSERT_EQ_UINT(errors, 0);
    ASSERT_EQ_UINT(w[0].ops, 3 * ITEMS);
    ASSERT_EQ_UINT(w[1].ops, 3 * ITEMS);
    ASSERT_EQ_UINT(w[2].ops + w[3].ops, 2 * (ITEMS / 10) * 2);
    ASSERT_EQ_UINT(_count(h), ITEMS);

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

static void *_remover(void *arg)
{
    _Worker *w = arg;
    Map_Iterator *it = NULL;
    unsigned int count = 0;

    _wait_for_start();

    /* remove every third entry this iterator sees */
    for (it = map_each(w->map); it; it = map_next(it), count ++) {
        if (count % 3 != w->id % 3) continue;
        if (! is_null(map_remove_at(it))) w->ops ++;
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _remove_at_concurrent(void)
{
    /* three iterators remove entries at once: nothing is lost or removed
       twice, and the map is usable afterwards */

    static _Worker w[3];
    pthread_t threads[3];
    Map *h = NULL;
    Variant v = { 0 };
    char key[32];
    unsigned int i = 0, removed = 0;
    int len = 0;

    ASSERT_NOT_NULL(h = map_alloc(NULL));
    _fill(h, 1000);
    _start_switch = 0;

    for (i = 0; i < 3; i ++) {
        memset(& w[i], 0, sizeof(w[i]));
        w[i].map = h; w[i].id = i;
        ASSERT_EQ_INT(pthread_create(& threads[i], NULL, _remover, & w[i]), 0);
    }

    _start();

    for (i = 0; i < 3; i ++) {
        pthread_join(threads[i], NULL);
        removed += w[i].ops;
    }

    ASSERT_TRUE(removed > 0);
    ASSERT_EQ_UINT(removed + _count(h), 1000);

    for (i = 1; i <= 1000; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        map_set(h, key, len, variant_from_integer(i + 1));
    }
    for (i = 1; i <= 1000; i ++) {
        len = snprintf(key, sizeof(key), "%u", i);
        v = map_get(h, key, len);
        if (! is_integer(v) || variant_to_integer(v) != i + 1) {
            test_fail(__FILE__, __LINE__, "%s: wrong after refill", key);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

#define READERS  16
#define DURATION 2

static uint64_t _now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, & ts);

    return (uint64_t) ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/* -------------------------------------------------------------------------- */

static void *_starving_reader(void *arg)
{
    _Worker *w = arg;
    char key[32];
    int len = 0;

    while (! _STOP_GET(w)) {
        len = snprintf(key, sizeof(key), "%04u", rand() % ITEMS);
        map_get(w->map, key, len);
        w->ops ++;
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static void *_starving_writer(void *arg)
{
    _Worker *w = arg;
    char key[32];
    uint64_t start = 0, latency = 0;
    int len = 0;

    while (! _STOP_GET(w)) {
        len = snprintf(key, sizeof(key), "%04u", rand() % ITEMS);
        start = _now();
        map_set(w->map, key, len, variant_from_integer(w->ops));
        latency = _now() - start;
        if (latency > w->max_latency) w->max_latency = latency;
        w->timeouts += (latency > 100000000ULL);
        w->ops ++;
        usleep(1000);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _writer_starvation(void)
{
    /* writers keep getting the lock under a continuous read load: no
       write waits more than 100 ms */

    static _Worker r[READERS], w[2];
    pthread_t readers[READERS], writers[2];
    Map *h = NULL;
    char key[32];
    unsigned int i = 0;
    uint64_t reads = 0, writes = 0, timeouts = 0, max_latency = 0;
    int len = 0;

    ASSERT_NOT_NULL(h = map_alloc(NULL));
    for (i = 0; i < ITEMS; i ++) {
        len = snprintf(key, sizeof(key), "%04u", i);
        map_set(h, key, len, variant_from_integer(i));
    }

    for (i = 0; i < READERS; i ++) {
        memset(& r[i], 0, sizeof(r[i]));
        r[i].map = h; r[i].id = i;
        ASSERT_EQ_INT(pthread_create(
            & readers[i], NULL, _starving_reader, & r[i]
        ), 0);
    }
    for (i = 0; i < 2; i ++) {
        memset(& w[i], 0, sizeof(w[i]));
        w[i].map = h; w[i].id = i;
        ASSERT_EQ_INT(pthread_create(
            & writers[i], NULL, _starving_writer, & w[i]
        ), 0);
    }

    sleep(DURATION);

    for (i = 0; i < READERS; i ++) _STOP_SET(& r[i], 1);
    for (i = 0; i < 2; i ++) _STOP_SET(& w[i], 1);
    for (i = 0; i < READERS; i ++) {
        pthread_join(readers[i], NULL);
        reads += r[i].ops;
    }
    for (i = 0; i < 2; i ++) {
        pthread_join(writers[i], NULL);
        writes += w[i].ops;
        timeouts += w[i].timeouts;
        if (w[i].max_latency > max_latency) max_latency = w[i].max_latency;
    }

    printf("%llu reads, %llu writes, worst write %.2f ms\n",
           (unsigned long long) reads, (unsigned long long) writes,
           max_latency / 1000000.0);

    ASSERT_EQ_UINT(timeouts, 0);
    ASSERT_TRUE(writes >= DURATION * 250);

    ASSERT_NULL(map_free(h));

    return 0;
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("set_get_remove", _set_get_remove),
    TEST("return_contracts", _return_contracts),
    TEST("volume", _volume),
    TEST("sort_and_foreach", _sort_and_foreach),
    TEST("merge_and_at", _merge_and_at),
    TEST("embedded_nul_keys", _embedded_nul_keys),
    TEST("random_differential", _random_differential),
    TEST("sticky_sort", _sticky_sort),
    TEST("sticky_sort_active_iterator", _sticky_sort_active_iterator),
    TEST("write_stress", _write_stress),
    TEST("iterator_concurrency", _iterator_concurrency),
    TEST("remove_at_concurrent", _remove_at_concurrent),
    TEST_SLOW("writer_starvation", _writer_starvation)
};

TEST_SUITE(test_suite_htable, "htable", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
