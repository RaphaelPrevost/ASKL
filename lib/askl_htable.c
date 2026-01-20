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

#include "askl_htable.h"

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_HASHTABLE
/* -------------------------------------------------------------------------- */

#include "arcane/bitops.c"
#include "arcane/htable.c"

typedef struct _item {
    void *ptr;
    variant val;
    struct {
        uint16_t len;
        char str[];
    } key;
} _item;

/**
 * @ingroup hashtable
 * @struct _item
 *
 * Internal representation of a single key/value entry stored in an
 * @ref ASKL_LinkedMap. An @ref _item is never allocated on its own;
 * it is always embedded inside a @ref _bucket.
 *
 * @b private @ref key.len is the key length in bytes (excluding the NUL byte).
 * @b private @ref key.str is the inline key storage (flexible array), always
 *                         terminated with a NUL character.
 * @b private @ref val is the value stored in the map, as a @ref variant.
 * @b private @ref ptr has two roles depending on where the item is stored:
 *   - in the main hash table (@ref _ASKL_LinkedMap::_bucket), it caches the
 *     primary hash value (hash0) as a uintptr_t
 *   - in the overflow basket (@ref _ASKL_LinkedMap::_basket), @ref ptr is
 *     used as the "next" link in the basket's singly linked list.
 *
 * This type is internal and may change at any time.
 */

typedef struct _bucket {
    struct _bucket *next;
    _item item; 
} _bucket;

#if (TAG_SHIFT > 0)
STATIC_ASSERT(
    ((ALIGNOF(_bucket) | offsetof(_bucket, item)) & TAG_MASK) == 0,
    pointer_alignment_unsuitable_for_tagging
);
#endif

/**
 * @ingroup hashtable
 * @struct _bucket
 *
 * Internal node used to maintain the map's traversal order.
 *
 * While the hash index is stored separately as an array of @ref _item pointers
 * (@ref _ASKL_LinkedMap::_bucket), the map also keeps a singly linked list of
 * all entries to support stable iteration and sorting. Each node of that list
 * is a @ref _bucket that embeds an @ref _item.
 *
 * The head of this list is stored in @ref _ASKL_LinkedMap::_index, and
 * functions such as @ref map_each(), @ref map_next() and @ref map_sort()
 * operate on this list rather than on the main hash table.
 *
 * @b private @ref next links nodes in the traversal list.
 * @b private @ref item is the embedded entry payload for this position in the
 *                      list. The same @ref _item is also referenced from the
 *                      hash table and/or the overflow basket.
 *
 * This type is internal and may change at any time.
 */

struct _ASKL_LinkedMap {
    ASKL_RWLock *_lock;
    struct _bucket *_index;
    struct _item **_bucket;
    struct _item *_basket;
    size_t _bucket_size;
    size_t _bucket_count;
    void (*_freeval)(variant);
    uintptr_t _seed[HASH_COUNT];
};

/**
 * @ingroup linkedmap
 * @struct _ASKL_LinkedMap
 *
 * This structure holds the internal state of a @ref ASKL_LinkedMap.
 *
 * An ASKL_LinkedMap is a hash-indexed associative container that also maintains
 * a stable traversal order (typically insertion order, and optionally sortable)
 * It combines cuckoo hashing for O(1) expected lookup with an overflow basket
 * for guaranteed insertion when cuckoo displacement fails.
 *
 * Concurrency:
 * - Readers hold a read lock to allow concurrent lookups/traversals.
 * - Writers take a write lock to insert/remove/resize.
 *
 * @b private @ref _lock is a reader/writer lock protecting the whole map.
 * @b private @ref _bucket_size is the current capacity of the map
 * @b private @ref _bucket_count is the number of entries in the map.
 * @b private @ref _index keeps track of all the entries in a stable order.
 *                 Each node points to an @ref _item in either @ref _bucket or
 *                 @ref _basket. This enables O(n) ordered traversal.
 * @b private @ref _bucket is the hash index table
 * @b private @ref _basket is the overflow chain head. Points to the first
 *                 @ref _bucket in the linked list of items that failed cuckoo
 *                 placement after HASH_RETRY displacement attempts.
 * @b private @ref _freeval is an optional destructor callback used when
 *                 removing entries from the map.
 * @b private @ref _seed is the per-map hash seed material (HASH_COUNT words)
 *
 * This type is internal and may change at any time; only use the public
 * @ref ASKL_LinkedMap API.
 */

/* -------------------------------------------------------------------------- */

static int _probe(ASKL_LinkedMap *h, unsigned int i, _item *new, int replace)
{
    if (! h->_bucket[i]) {
        if (unlikely(replace == MODIFY_ONLY)) return -1;
        h->_bucket[i] = TAG_PTR(new, new->ptr);
        h->_bucket_count ++;
        /* insert */
        return 0;
    }

    if (replace >= 0 && unlikely(GET_TAG(h->_bucket[i]) == HASHTAG(new->ptr))) {
        _item *slot = GET_PTR(h->_bucket[i]);
        if (new->ptr == slot->ptr) {
            /* XXX tombstones have a zero length but we can rely on the
               NUL terminator of the key to inspect them */
            if (! memcmp(new->key.str, slot->key.str, new->key.len + 1)) {
                if (unlikely(! slot->key.len)) {
                    if (unlikely(replace == MODIFY_ONLY)) return -1;
                    /* resurrect the key */
                    slot->key.len = new->key.len;
                    slot->val = variant_null();
                }
                /* replace */
                return 1;
            }
        }
    }

    /* continue */
    return INT_MAX;
}

/* -------------------------------------------------------------------------- */

static int _set_item(
    ASKL_LinkedMap *h,
    _item *item,
    int replace,
    variant *val,
    variant (*on_insert)(const char *k, size_t l, variant new),
    variant (*on_update)(const char *k, size_t l, variant old, variant new)
)
{
    unsigned int i = 0, index = 0, retry = 0;
    _item *slot = NULL;
    uintptr_t hash = 0, mask = h->_bucket_size - 1;

    /* avoid rehashing every key */
    if (unlikely(! (hash = (uintptr_t) item->ptr))) {
        hash = _hash(item->key.str, item->key.len, h->_seed[0]);
        item->ptr = (void *) hash;
    }

    #if (UINTPTR_MAX == 0xffffffffffffffffULL)
    PREFETCH(& h->_bucket[(hash >> 32) & mask], 1, L1_CACHE);
    #endif

    goto _loop;

    /* look for a free slot */
    for (i = 0; i < HASH_COUNT; i ++) {
        int probe = 0;

        hash = _hash(item->key.str, item->key.len, h->_seed[i]);
        #if (UINTPTR_MAX == 0xffffffffffffffffULL)
        PREFETCH(& h->_bucket[(hash >> 32) & mask], 1, L2_CACHE);
        #endif

_loop:  index = hash & mask;
        if ( (probe = _probe(h, index, item, replace)) == 0) {
            if (on_insert)
                item->val = on_insert(item->key.str, item->key.len, item->val);
            return 0;
        } else if (unlikely(probe == -1)) goto _failure;

        if (probe == 1) {
            slot = GET_PTR(h->_bucket[index]);
            goto _replace;
        }

        #if (UINTPTR_MAX == 0xffffffffffffffffULL)
        /* second probe on 64 bits systems */
        index = (hash >> 32) & mask;
        if ( (probe = _probe(h, index, item, replace)) == 0) {
            if (on_insert)
                item->val = on_insert(item->key.str, item->key.len, item->val);
            return 0;
        } else if (unlikely(probe == -1)) goto _failure;

        if (probe == 1) {
            slot = GET_PTR(h->_bucket[index]);
            goto _replace;
        }
        #endif
    }

    /* couldn't find it, look in the basket */
    if (replace != REHASH_ONLY) {
        for (slot = h->_basket; slot; slot = slot->ptr) {
            if (likely(item->key.len == slot->key.len)) {
                if (! memcmp(item->key.str, slot->key.str, item->key.len))
                    goto _replace;
            }
        }

        if (replace == MODIFY_ONLY) goto _failure;
    }

    /* no free slot found, the new item will be forcefully inserted */
    if (on_insert)
        item->val = on_insert(item->key.str, item->key.len, item->val);

    /* try cuckoo eviction */
    for (index = (uintptr_t) item->ptr & mask; retry < HASH_RETRY; retry ++) {
        _item *tmp = GET_PTR(h->_bucket[index]);
        int loop = (tmp->ptr == item->ptr);

        h->_bucket[index] = TAG_PTR(item, item->ptr); item = tmp;

        /* get rid of tombstones */
        if (unlikely(! item->key.len)) return 0;

        #if (UINTPTR_MAX == 0xffffffffffffffffULL)
        index = (((uintptr_t) item->ptr) >> 32) & mask;
        if (_probe(h, index, item, REHASH_ONLY) == 0)
            return 0;
        #endif

        for (i = 1; i < HASH_COUNT; i ++) {
            hash = _hash(item->key.str, item->key.len, h->_seed[i]);
            index = hash & mask;
            if (_probe(h, index, item, REHASH_ONLY) == 0)
                return 0;
            #if (UINTPTR_MAX == 0xffffffffffffffffULL)
            /* second probe on 64 bits systems */
            index = (hash >> 32) & mask;
            if (_probe(h, index, item, REHASH_ONLY) == 0)
                return 0;
            #endif
        }

        /* avoid evicting the original cuckoo */
        if (unlikely(loop)) break;
    }

    /* store the key in the overflow basket */
    item->ptr = h->_basket; h->_basket = item; h->_bucket_count ++;

    return 0;

_replace:
    if (replace) {
        variant new = item->val, rejected = slot->val;
        if (on_update) {
            new = on_update(item->key.str, item->key.len, slot->val, item->val);
            if (! variant_equal(new, item->val)) {
                if (! variant_equal(new, slot->val)) {
                    /* the function returned an entirely new value */
                    if (h->_freeval) h->_freeval(slot->val);
                }
                /* the item value was unused */
                rejected = item->val;
            }
        }

        if (replace == MODIFY_ONLY)
            if (lock_upgrade(h->_lock) == -1) goto _failure;

            *val = rejected;
            slot->val = new;

        if (replace == MODIFY_ONLY)
            lock_restore(h->_lock);
    } else *val = slot->val; /* return the existing value */

    return 1;

_failure:
    *val = item->val; /* return the rejected value */
    return -1;
}

/* -------------------------------------------------------------------------- */

static int _resize(ASKL_LinkedMap *h, size_t size)
{
    _bucket *b = NULL, *next = NULL;
    _bucket **prev = NULL;
    _item *item = NULL, *tmp = NULL;
    _item **new = NULL;

    if (h->_bucket_count * HASH_RATIO < h->_bucket_size) return 0;

    /* round the size to the next highest power of 2 */
    size --; size |= size >> 1; size |= size >> 2;
    size |= size >> 4; size |= size >> 8; size |= size >> 16;
    size ++; size += (size == 0);

    /* try to allocate a new bucket array */
    if (! (new = calloc(size, sizeof(*h->_bucket))) ) {
        perror(ERR(_resize, calloc));
        return -1;
    }

    /* clear the basket */
    for (item = h->_basket, h->_basket = NULL; item; item = tmp) {
        tmp = item->ptr;
        item->ptr = NULL;
    }

    /* replace the bucket array */
    free(h->_bucket); h->_bucket = new;

    /* update the state */
    h->_bucket_size = size; h->_bucket_count = 0;

    /* rehash old buckets */
    for (prev = & h->_index, b = h->_index; b; b = next) {
        next = b->next;

        if (unlikely(! b->item.key.len)) {
            *prev = next;
            free(b);
            continue;
        }

        prev = & b->next;

        _set_item(h, & b->item, REHASH_ONLY, NULL, NULL, NULL);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static inline variant _insert(
    ASKL_LinkedMap *h,
    const char *key,
    size_t len,
    variant val,
    int replace,
    variant (*on_insert)(const char *k, size_t l, variant new),
    variant (*on_update)(const char *k, size_t l, variant old, variant new)
)
{
    _bucket *bucket = NULL;
    int (*lockfn[3])(ASKL_RWLock *) = {
        lock_wrlock, lock_wrlock, lock_rdlock
    };

    /* replace the key by a dynamically allocated one */
    if (! (bucket = malloc(sizeof(*bucket) + len + 1)) ) {
        perror(ERR(_insert, malloc));
        return val;
    }

    memcpy(bucket->item.key.str, key, len);
    bucket->item.key.str[len] = '\0';
    bucket->item.key.len = len;
    bucket->item.val = val;
    bucket->item.ptr = NULL;

    if (lockfn[replace](h->_lock) == -1) goto _err_lock;

    if (! _set_item(h, & bucket->item, replace, & val, on_insert, on_update)) {
        bucket->next = h->_index; h->_index = bucket;
        val = variant_null();
    } else free(bucket);

    /* try to expand the hashtable if the load is too important */
    if (h->_basket && h->_basket->ptr)
        _resize(h, h->_bucket_size + 1);

    lock_unlock(h->_lock);

    return val;

_err_lock:
    free(bucket);
    return val;
}

/* -------------------------------------------------------------------------- */

public ASKL_LinkedMap *map_alloc(void (*freeval)(variant))
{
    ASKL_LinkedMap *h = NULL;

    if (! (h = malloc(sizeof(*h))) ) {
        perror(ERR(map_alloc, malloc));
        return NULL;
    }

    if (random_seed((uint32_t *) h->_seed, sizeof(h->_seed) / 4) == -1)
        goto _err_rand;

    #if (UINTPTR_MAX == 0xffffffffU)
    for (int i = 0; i < HASH_COUNT; i ++) {
        /* wyhash32 known bad seeds */
        if ((h->_seed[i] == 0x429dacdd) ||
            (h->_seed[i] == 0x51a43a0f) ||
            (h->_seed[i] == 0x522235ae) ||
            (h->_seed[i] == 0x99ac2b20) ||
            (h->_seed[i] == 0x9a4f1376) ||
            (h->_seed[i] == 0xd637dbf3))
            h->_seed[i] ++;
    }
    #endif

    if (! (h->_lock = lock_alloc()) ) goto _err_lock;
    if (lock_init(h->_lock) == -1) goto _err_init;

    h->_bucket = NULL;
    h->_bucket_count = h->_bucket_size = 0; h->_basket = NULL;
    h->_index = NULL;
    h->_freeval = freeval;

    if (_resize(h, 4) == -1) {
        debug("map_alloc(): cannot resize the hash table.\n");
        goto _err_size;
    }

    return h;

_err_size:
    free(h->_bucket);
    free(h->_basket);
    lock_destroy(h->_lock);
_err_init:
    lock_free(h->_lock);
_err_lock:
_err_rand:
    free(h);

    return NULL;
}

/* -------------------------------------------------------------------------- */

public variant map_set_with(
    ASKL_LinkedMap *h,
    const char *k,
    size_t l,
    variant v,
    variant (*function)(const char *k, size_t l, variant old, variant new)
)
{
    if (unlikely(! h || ! k || ! l)) {
        debug("map_set_with(): bad parameters.\n");
        return v;
    }

    return _insert(h, k, l, v, STORE_VALUE, NULL, function);
}

/* -------------------------------------------------------------------------- */

public variant map_set(ASKL_LinkedMap *h, const char *k, size_t l, variant v)
{
    if (unlikely(! h || ! k || ! l)) {
        debug("map_set(): bad parameters.\n");
        return v;
    }

    return _insert(h, k, l, v, STORE_VALUE, NULL, NULL);
}

/* -------------------------------------------------------------------------- */

public variant map_insert_with(
    ASKL_LinkedMap *h,
    const char *k,
    size_t l,
    variant v,
    variant (*function)(const char *k, size_t l, variant new)
)
{
    if (unlikely(! h || ! k || ! l)) {
        debug("map_insert_with(): bad parameters.\n");
        return v;
    }

    return _insert(h, k, l, v, CREATE_ONLY, function, NULL);
}

/* -------------------------------------------------------------------------- */

public variant map_insert(ASKL_LinkedMap *h, const char *k, size_t l, variant v)
{
    if (unlikely(! h || ! k || ! l)) {
        debug("map_insert(): bad parameters.\n");
        return v;
    }

    return _insert(h, k, l, v, CREATE_ONLY, NULL, NULL);
}

/* -------------------------------------------------------------------------- */

public variant map_update_with(
    ASKL_LinkedMap *h,
    const char *k,
    size_t l,
    variant v,
    variant (*function)(const char *k, size_t l, variant old, variant new)
)
{
    if (unlikely(! h || ! k || ! l)) {
        debug("map_update_with(): bad parameters.\n");
        return v;
    }

    return _insert(h, k, l, v, MODIFY_ONLY, NULL, function);
}

/* -------------------------------------------------------------------------- */

public variant map_update(ASKL_LinkedMap *h, const char *k, size_t l, variant v)
{
    if (unlikely(! h || ! k || ! l)) {
        debug("map_update(): bad parameters.\n");
        return v;
    }

    return _insert(h, k, l, v, MODIFY_ONLY, NULL, NULL);
}

/* -------------------------------------------------------------------------- */

static _item *_get_item(ASKL_LinkedMap *h, const char *k, size_t l, variant *v)
{
    unsigned int i = 0;
    uintptr_t h0, hash, mask = h->_bucket_size - 1;
    _item *ptr = NULL;

    h0 = hash = _hash(k, l, h->_seed[0]);
    #if (UINTPTR_MAX == 0xffffffffffffffffULL)
    PREFETCH(& h->_bucket[(hash >> 32) & mask], 0, L1_CACHE);
    #endif
    goto _loop;

    for (i = 0; i < HASH_COUNT; i ++) {
        hash = _hash(k, l, h->_seed[i]);
        #if (UINTPTR_MAX == 0xffffffffffffffffULL)
        PREFETCH(& h->_bucket[(hash >> 32) & mask], 0, L2_CACHE);
        #endif

        #define _MAP_GET(index) \
        /* if an empty slot is found, no need to look further */ \
        if (! (ptr = h->_bucket[(index)]) ) break; \
        if (GET_TAG(ptr) == HASHTAG(h0)) { \
            ptr = GET_PTR(ptr); \
            if ((uintptr_t) ptr->ptr == h0 && likely(ptr->key.len == l)) { \
                if (likely(memcmp(ptr->key.str, k, l) == 0)) { \
                    *v = ptr->val; \
                    return ptr; \
                } \
            } \
        }

_loop:  _MAP_GET(hash & mask);

        #if (UINTPTR_MAX == 0xffffffffffffffffULL)
        /* second probe on 64 bits systems */
        _MAP_GET((hash >> 32) & mask);
        #endif

        #undef _MAP_GET
    }

    /* scan the overflow basket */
    for (ptr = h->_basket; ptr; ptr = ptr->ptr) {
        if (ptr->key.len == l && memcmp(ptr->key.str, k, l) == 0) {
            *v = ptr->val;
            return ptr;
        }
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

public variant map_get_with(
    ASKL_LinkedMap *h,
    const char *key,
    size_t len,
    variant (*function)(variant)
)
{
    variant res = { 0 };

    if (unlikely(! h || ! key || ! len)) {
        debug("map_get_with(): bad parameters.\n");
        return res;
    }

    if (lock_rdlock(h->_lock) == -1) return res;

        if (_get_item(h, key, len, & res) && function)
            res = function(res);

    lock_unlock(h->_lock);

    return res;
}

/* -------------------------------------------------------------------------- */

public variant map_get(ASKL_LinkedMap *h, const char *key, size_t len)
{
    variant res = { 0 };

    if (unlikely(! h || ! key || ! len)) {
        debug("map_get(): bad parameters.\n");
        return res;
    }

    if (lock_rdlock(h->_lock) == -1) return res;

        _get_item(h, key, len, & res);

    lock_unlock(h->_lock);

    return res;
}

/* -------------------------------------------------------------------------- */

static variant _exists(UNUSED variant v)
{
    return variant_true();
}

/* -------------------------------------------------------------------------- */

public int map_has(ASKL_LinkedMap *h, const char *key, size_t len)
{
    variant v = map_get_with(h, key, len, _exists);
    return (is_boolean(v) && variant_to_boolean(v));
}

/* -------------------------------------------------------------------------- */

public int map_merge(
    ASKL_LinkedMap *dest,
    ASKL_LinkedMap *src,
    variant merge(const char *key, size_t len, variant destval, variant srcval)
)
{
    _bucket *b = NULL, *next = NULL;

    if (! dest || ! src || ! merge) {
        debug("map_merge(): bad parameters.\n");
        return -1;
    }

    /* pry both maps open */
    if (lock_wrlock(dest->_lock) == -1) return -1;
    if (lock_wrlock(src->_lock) == -1) {
        lock_unlock(dest->_lock);
        return -1;
    }

    lock_break(src->_lock);

    for (b = src->_index, src->_index = NULL; b; b = next) {
        variant v = variant_null();
        next = b->next;

        if (! b->item.key.len) { free(b); continue; }

        b->item.ptr = NULL;

        /* handle conflicts with the merge helper */
        if (_set_item(dest, & b->item, 1, & v, NULL, merge)) {
            if (variant_equal(v, b->item.val)) {
                if (src->_freeval) src->_freeval(v);
            } else if (dest->_freeval) dest->_freeval(v);
            free(b);
        } else {
            b->next = dest->_index; dest->_index = b;
        }
    }

    /* destroy the source map */
    free(src->_bucket);
    lock_destroy(src->_lock);
    lock_free(src->_lock); free(src);

    lock_unlock(dest->_lock);

    return 0;
}

/* -------------------------------------------------------------------------- */

public void map_foreach(
    ASKL_LinkedMap *h,
    int (*f)(const char *, size_t, variant)
)
{
    _bucket *bucket = NULL;

    if (! h || ! f) {
        debug("map_foreach(): bad parameters.\n");
        return;
    }

    if (lock_wrlock(h->_lock) == -1) return;

    for (bucket = h->_index; bucket; bucket = bucket->next) {
        if (bucket->item.key.len) {
            int ret = f(
                bucket->item.key.str,
                bucket->item.key.len,
                bucket->item.val
            );
            if (ret == -1) {
                /* delete the record */
                if (h->_freeval)
                    h->_freeval(bucket->item.val);
                bucket->item.key.len = 0;
            }
        }
    }

    lock_unlock(h->_lock);

    return;
}

/* -------------------------------------------------------------------------- */

public int map_sort(
    ASKL_LinkedMap *h,
    unsigned int order,
    int (*cmp)(
        const char *key0,
        const char *key1,
        size_t len,
        variant value0,
        variant value1
    )
)
{
    _bucket *l[2] = { NULL, NULL }, *bucket = NULL, *tail = NULL;
    unsigned int size = 1, merge = 0, i = 0;
    _item *a = NULL, *b = NULL;
    unsigned int c[2] = { 0, 0 };

    if (! h || ! cmp || (order != MAP_ASC && order != MAP_DESC) ) {
        debug("map_sort(): bad parameters.\n");
        return -1;
    }

    if (lock_wrlock(h->_lock) == -1) return -1;

    /* simple merge sort */
    do {
        l[0] = h->_index; h->_index = NULL; tail = NULL;

        for (merge = 0; (l[1] = l[0]); merge ++, l[0] = l[1]) {

            /* split the table in 2 sorted lists */
            for (c[0] = 0; c[0] < size; c[0] ++) {
                if (! (l[1] = l[1]->next) ) { c[0] ++; break; }
            }

            /* merge these lists */
            for (c[1] = size; c[0] || (l[1] && c[1]); tail = bucket) {

                if (l[1] && c[1]) {

                    if (c[0]) {
                        a = & l[0]->item;
                        b = & l[1]->item;
                        i = (a->key.len < b->key.len) ? a->key.len : b->key.len;
                        if (cmp(a->key.str, b->key.str, i, a->val, b->val) <= 0)
                            i = 0 + order;
                        else
                            i = 1 - order;
                    } else i = 1; /* 1st list is empty */

                } else i = 0; /* 2nd list is empty */

                bucket = l[i];

                if (tail) tail->next = bucket;
                else h->_index = bucket;

                l[i] = l[i]->next; c[i] --;
            }
        }

        if (tail) tail->next = NULL; size <<= 1;

    } while (merge > 1);

    lock_unlock(h->_lock);

    return 0;
}

/* -------------------------------------------------------------------------- */

public int map_sort_keys(
    const char *key0,
    const char *key1,
    size_t l,
    UNUSED variant val0,
    UNUSED variant val1
)
{
    return memcmp(key0, key1, l);
}

/* -------------------------------------------------------------------------- */

public variant map_remove_if(
    ASKL_LinkedMap *h,
    const char *key,
    size_t len,
    int (*condition)(const char *key, size_t len, variant val)
)
{
    unsigned int i = 0;
    _item *tmp = NULL, *prev = NULL;
    variant result = { 0 };
    uintptr_t h0 = 0, hash = 0, mask = 0;

    if (! h || ! key || ! len) {
        debug("map_remove(): bad parameters.\n");
        return result;
    }

    if (lock_wrlock(h->_lock) == -1) return result;

    mask = h->_bucket_size - 1;
    h0 = hash = _hash(key, len, h->_seed[i]);
    goto _loop;

    for (i = 0; i < HASH_COUNT; i ++) {
        hash = _hash(key, len, h->_seed[i]);

        #define _MAP_REMOVE(index) \
        if ( (tmp = h->_bucket[(index)]) && GET_TAG(tmp) == HASHTAG(h0)) { \
            tmp = GET_PTR(tmp); \
            if ((uintptr_t) tmp->ptr == h0 && likely(tmp->key.len == len)) { \
                if (likely(memcmp(tmp->key.str, key, len) == 0)) { \
                    if (! condition || condition(key, len, tmp->val)) { \
                        /* remove from the bucket */ \
                        result = tmp->val; \
                        /* a length of 0 indicates a tombstone */ \
                        tmp->key.len = 0; \
                    } \
                    goto _result; \
                } \
            } \
        }

_loop:  _MAP_REMOVE(hash & mask);

        #if (UINTPTR_MAX == 0xffffffffffffffffULL)
        /* second probe on 64 bits systems */
        _MAP_REMOVE((hash >> 32) & mask);
        #endif

        #undef _MAP_REMOVE
    }

    /* scan the overflow basket */
    for (tmp = prev = h->_basket; tmp; prev = tmp, tmp = tmp->ptr) {
        if (tmp->key.len == len) {
            if (memcmp(tmp->key.str, key, len) == 0) {
                if (! condition || condition(key, len, tmp->val)) {
                    /* remove from the basket */
                    result = tmp->val;
                    if (tmp == h->_basket) h->_basket = tmp->ptr;
                    else prev->ptr = tmp->ptr;

                    /* tombstone */
                    tmp->key.len = 0;
                }
                goto _result;
            }
        }
    }

_result:
    /* garbage collection if necessary */
    _resize(h, (size_t) (h->_bucket_count * HASH_RATIO));
    lock_unlock(h->_lock);

    return result;
}

/* -------------------------------------------------------------------------- */

public variant map_remove(ASKL_LinkedMap *h, const char *key, size_t len)
{
    return map_remove_if(h, key, len, NULL);
}

/* -------------------------------------------------------------------------- */

public size_t map_footprint(ASKL_LinkedMap *h, size_t *overhead)
{
    _bucket *bucket = NULL;
    size_t key = 0;
    size_t ret = sizeof(*h);

    if (! h) {
        debug("map_footprint(): bad parameters.\n");
        return 0;
    }

    if (lock_wrlock(h->_lock) == -1) return 0;

    if (h->_bucket_size) {
        /* bucket size */
        ret += h->_bucket_size * sizeof(*h->_bucket);
        /* keys */
        for (bucket = h->_index; bucket; bucket = bucket->next) {
            if (bucket->item.key.len) {
                /* key length + key recorded size + next and value pointers */
                ret += (
                    sizeof(char *) + bucket->item.key.len +
                    sizeof(bucket->item.key.len) +
                    sizeof(char *) + sizeof(variant)
                );
                /* key length and value pointer are not overhead */
                key += (
                    sizeof(bucket->item.key.len) +
                    bucket->item.key.len + sizeof(void *)
                );
            }
        }
    }

    lock_unlock(h->_lock);

    if (overhead) *overhead = ret - key;

    return ret;
}

/* -------------------------------------------------------------------------- */

public ASKL_LinkedMap *map_free(ASKL_LinkedMap *h)
{
    _bucket *bucket = NULL, *next = NULL;

    if (! h) return NULL;

    if (lock_wrlock(h->_lock) == -1) return NULL;

    /* free the threads waiting after the linked hashmap */
    lock_break(h->_lock);

    for (bucket = h->_index; bucket; bucket = next) {
        next = bucket->next;
        if (h->_freeval && bucket->item.key.len)
            h->_freeval(bucket->item.val);
        free(bucket);
    }

    free(h->_bucket);
    lock_destroy(h->_lock);
    lock_free(h->_lock); free(h);

    return NULL;
}

/* -------------------------------------------------------------------------- */
/* Iterator */
/* -------------------------------------------------------------------------- */

public ASKL_MapIterator *map_each(ASKL_LinkedMap *h)
{
    ASKL_MapIterator *iterator = NULL;

    if (! h) {
        debug("map_each(): bad parameters.\n");
        return NULL;
    }

    if (lock_rdlock(h->_lock) == -1) goto _err_lock;

    if (! h->_index) {
        debug("map_each(): empty map.\n");
        goto _err_init;
    }

    if (! (iterator = malloc(sizeof(*iterator)))) {
        perror(ERR(map_each, malloc));
        goto _err_init;
    }

    iterator->map = h;
    iterator->_current = h->_index;
    if (likely(iterator->_current->item.key.len)) {
        iterator->key = h->_index->item.key.str;
        iterator->len = h->_index->item.key.len;
        iterator->val = h->_index->item.val;
    } else return map_next(iterator);

    return iterator;

_err_init:
    lock_unlock(h->_lock);
_err_lock:
    return NULL;
}

/* -------------------------------------------------------------------------- */

public ASKL_MapIterator *map_at(ASKL_LinkedMap *h, const char *key, size_t len)
{
    ASKL_MapIterator *iterator = NULL;
    uint8_t *ptr = NULL;

    if (! h) {
        debug("map_at(): bad parameters.\n");
        return NULL;
    }

    if (! (iterator = malloc(sizeof(*iterator)))) {
        perror(ERR(map_at, malloc));
        return NULL;
    }

    iterator->map = h;

    if (lock_rdlock(h->_lock) == -1) goto _err_lock;

    if (! (ptr = (uint8_t *) _get_item(h, key, len, & iterator->val))) {
        debug("map_at(): key not found.\n");
        goto _err_item;
    }

    /* find the bucket from the item address */
    iterator->_current = (_bucket *) (ptr - offsetof(_bucket, item));
    iterator->key = iterator->_current->item.key.str;
    iterator->len = iterator->_current->item.key.len;

    return iterator;

_err_item:
    lock_unlock(h->_lock);
_err_lock:
    free(iterator);
    return NULL;
}

/* -------------------------------------------------------------------------- */

public ASKL_MapIterator * CALLBACK map_next(ASKL_MapIterator *iterator)
{
    _bucket *bucket = NULL;

    for (bucket = iterator->_current->next; bucket; bucket = bucket->next) {
        if (likely(bucket->item.key.len)) {
            iterator->_current = bucket;
            iterator->key = bucket->item.key.str;
            iterator->len = bucket->item.key.len;
            iterator->val = bucket->item.val;
            return iterator;
        }
    }

    return map_break(iterator);
}

/* -------------------------------------------------------------------------- */

public variant map_set_at(ASKL_MapIterator *iterator, variant new)
{
    variant old;

    if (lock_upgrade(iterator->map->_lock) == -1) return new;
        old = iterator->_current->item.val;
        iterator->_current->item.val = new;
        iterator->val = new;
    lock_restore(iterator->map->_lock);

    return old;
}

/* -------------------------------------------------------------------------- */

public variant map_remove_at(ASKL_MapIterator *iterator)
{
    variant ret = { 0 };
    unsigned int len = 0;

    if (lock_upgrade(iterator->map->_lock) == -1) return ret;
        /* XXX another thread may have deleted the entry during upgrade */
        if (likely(len = iterator->_current->item.key.len)) {
            ret = iterator->_current->item.val;
            iterator->_current->item.key.len = 0;
        }
    lock_restore(iterator->map->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

public ASKL_MapIterator *map_break(ASKL_MapIterator *iterator)
{
    if (! iterator) {
        debug("map_break(): bad parameters.\n");
        return NULL;
    }

    lock_unlock(iterator->map->_lock);
    free(iterator);

    return NULL;
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
