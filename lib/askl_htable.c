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
 * This structure represents one key/value entry stored in a @ref ASKL_LinkedMap
 * and is actually never used alone, but always as part of a @ref _bucket.
 * 
 * The entry stores:
 * - a key (variable-length string, embedded at the end of the object),
 * - a value (as a @ref variant),
 * - a pointer field used by the map internals.
 *
 * The key is stored inline to minimize allocations: the @ref key.str field
 * is a flexible array member and holds a NUL-terminated byte string of
 * length @ref key.len (not counting the trailing NUL).
 *
 * @b private @ref ptr is an internal pointer used to link or reference entries
 *            depending on the operation (e.g. index bucket ownership, ordering,
 *            or temporary traversal state). It must not be accessed directly.
 * @b private @ref val is the value stored in the map, as a @ref variant.
 * @b private @ref key.len is the key length in bytes (excluding the NUL byte).
 * @b private @ref key.str is the inline key storage (flexible array).
 *
 * This type is internal and may change at any time.
 */

typedef struct _bucket {
    struct _bucket *next;
    _item item; 
} _bucket;

/**
 * @ingroup hashtable
 * @struct _bucket
 *
 * The map maintains an array of bucket heads (the hash index). Each element in
 * that array points to a linked list of @ref _bucket nodes that share the same
 * hash slot.
 *
 * @b private @ref next links buckets that collide in the same hash slot.
 * @b private @ref item is the embedded entry stored in this bucket.
 *
 * The @ref item member is embedded (not a pointer) so that each bucket holds
 * both the chaining node and the entry payload in one allocation.
 *
 * This type is internal and may change at any time.
 */

struct _ASKL_LinkedMap {
    struct _rwlock *_lock;
    struct _bucket *_index;
    struct _item **_bucket;
    struct _item *_basket;
    size_t _bucket_size;
    size_t _bucket_count;
    void (*_freeval)(variant);
    uint32_t _seed[HASH_COUNT];
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

struct _ASKL_HashTable {
    ASKL_LinkedMap *_segment[256];
};

/**
 * @ingroup hashtable
 * @struct _ASKL_HashTable
 *
 * This structure holds the internal state of an @ref ASKL_HashTable.
 *
 * An ASKL_HashTable is implemented as a segmented table:
 * it splits the key space into 256 independent segments, each segment being a
 * @ref ASKL_LinkedMap.
 *
 * Segmentation improves scalability under contention by reducing the scope of
 * locks: operations on different segments can proceed in parallel.
 *
 * Segment selection is performed with crc8, so that keys are spread across
 * segments.
 *
 * @b private @ref _segment is the array of 256 segment maps.
 *            Each segment is an independent @ref ASKL_LinkedMap with its own
 *            lock and seed material.
 *
 * This type is internal and may change at any time; only use the public
 * @ref ASKL_HashTable API.
 */

/* -------------------------------------------------------------------------- */

static int _set_item(ASKL_LinkedMap *h, _item *item, int replace, variant *val)
{
    unsigned int i = 0, index = 0, retry = 0;
    _item *slot = NULL;
    uint32_t mask = h->_bucket_size - 1;

    /* avoid rehashing every key */
    if (unlikely(! (index = (uintptr_t) item->ptr))) {
        index = _hash(item->key.str, item->key.len, h->_seed[0]);
        item->ptr = (void *) ((uintptr_t) index);
    }
    index &= mask; goto _loop;

    /* look for a free slot */
    for (i = 0; i < HASH_COUNT; i ++) {
        index = _hash(item->key.str, item->key.len, h->_seed[i]) & mask;

_loop:  if (! h->_bucket[index]) {
            /* this slot is free, insert */
            h->_bucket[index] = item;
            h->_bucket_count ++;
            return 0;
        }

        if (replace) {
            slot = h->_bucket[index];
            if (item->ptr == slot->ptr && item->key.len == slot->key.len) {
                if (! memcmp(item->key.str, slot->key.str, item->key.len))
                    goto _replace;
            }
        }
    }

    if (replace) {
        /* couldn't find it, look in the basket */
        for (slot = h->_basket; slot; slot = slot->ptr) {
            if (likely(item->key.len == slot->key.len)) {
                if (! memcmp(item->key.str, slot->key.str, item->key.len))
                    goto _replace;
            }
        }
    }

    /* no free slot found, try cuckoo eviction */
    for (index = (uintptr_t) item->ptr & mask; retry < HASH_RETRY; retry ++) {
        _item *tmp = h->_bucket[index];
        int loop = (tmp->ptr == item->ptr);
        h->_bucket[index] = item; item = tmp;

        /* get rid of tombstones */
        if (unlikely(! item->key.len)) return 0;

        /* skip the first seed as the slot is obviously taken */
        for (i = 1; i < HASH_COUNT; i ++) {
            index = _hash(item->key.str, item->key.len, h->_seed[i]) & mask;

            if (! h->_bucket[index]) {
                /* this slot is free, insert */
                h->_bucket[index] = item;
                h->_bucket_count ++;
                return 0;
            }
        }

        /* avoid evicting the original cuckoo */
        if (unlikely(loop)) break;
    }

    /* store the key in the overflow basket */
    item->ptr = h->_basket; h->_basket = item; h->_bucket_count ++;

    return 0;

_replace:
    if (replace == -1) *val = item->val;
    else { *val = slot->val; slot->val = item->val; }
    return 1;
}

/* -------------------------------------------------------------------------- */

static int _resize(ASKL_LinkedMap *h, size_t size)
{
    _bucket *b = NULL, *next = NULL;
    _item *item = NULL, *tmp = NULL;
    _item **new = NULL;

    if (! h) {
        debug("_resize(): bad parameters.\n");
        return -1;
    }

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
    for (b = h->_index, h->_index = NULL; b; b = next) {
        next = b->next;

        if (! b->item.key.len) { free(b); continue; }

        _set_item(h, & b->item, 0, NULL);

        b->next = h->_index; h->_index = b;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static inline variant _insert(
    ASKL_LinkedMap *h,
    const char *key,
    size_t len,
    variant val,
    int replace
)
{
    _bucket *bucket = NULL;

    if (! h || ! key || ! len) {
        debug("_insert(): bad parameters.\n");
        return val;
    }

    /* replace the key by a dynamically allocated one */
    if (! (bucket = malloc(sizeof(*bucket) + len)) ) {
        perror(ERR(_insert, malloc));
        return val;
    }

    memcpy(bucket->item.key.str, key, len);
    bucket->item.key.len = len;
    bucket->item.val = val;
    bucket->item.ptr = NULL;

    if (_map_wrlock(h->_lock) == -1) goto _err_lock;

    if (! _set_item(h, & bucket->item, replace, & val)) {
        bucket->next = h->_index; h->_index = bucket;
        val = variant_null();
    } else free(bucket);

    /* try to expand the hashtable if the load is too important */
    if (h->_basket && h->_basket->ptr)
        _resize(h, h->_bucket_size + 1);

    _map_unlock(h->_lock);

    return val;

_err_lock:
    free(bucket);
    return val;
}

/* -------------------------------------------------------------------------- */

public ASKL_LinkedMap *map_alloc(void (*freeval)(variant))
{
    unsigned int i = 0;
    ASKL_LinkedMap *h = NULL;

    if (! (h = malloc(sizeof(*h))) ) {
        perror(ERR(map_alloc, malloc));
        return NULL;
    }

    /* initialize the seeds */
    if (random_seed(h->_seed, HASH_COUNT) == -1)
        goto _err_rand;

    for (i = 0; i < HASH_COUNT; i ++) {
        /* known bad seeds */
        if ((h->_seed[i] == 0x429dacdd) ||
            (h->_seed[i] == 0x51a43a0f) ||
            (h->_seed[i] == 0x522235ae) ||
            (h->_seed[i] == 0x99ac2b20) ||
            (h->_seed[i] == 0x9a4f1376) ||
            (h->_seed[i] == 0xd637dbf3))
            h->_seed[i] ++;
    }

    /* initialize the inner semaphore */
    if (! (h->_lock = malloc(sizeof(*h->_lock))) ) {
        perror(ERR(map_alloc, malloc));
        goto _err_lock;
    }

    if (pthread_mutex_init(& h->_lock->mutex, NULL) == -1) {
        perror(ERR(map_alloc, pthread_mutex_init));
        goto _err_init;
    }

    if (pthread_cond_init(& h->_lock->cond, NULL) == -1) {
        perror(ERR(map_alloc, pthread_cond_init));
        goto _err_cond;
    }

    h->_lock->state = 1; /* unlocked */

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
    pthread_cond_destroy(& h->_lock->cond);
_err_cond:
    pthread_mutex_destroy(& h->_lock->mutex);
_err_init:
    free(h->_lock);
_err_lock:
_err_rand:
    free(h);

    return NULL;
}

/* -------------------------------------------------------------------------- */

public variant map_set(ASKL_LinkedMap *h, const char *k, size_t l, variant v)
{
    return _insert(h, k, l, v, 1);
}

/* -------------------------------------------------------------------------- */

public variant map_insert(ASKL_LinkedMap *h, const char *k, size_t l, variant v)
{
    return _insert(h, k, l, v, -1);
}

/* -------------------------------------------------------------------------- */

public variant map_get_with(
    ASKL_LinkedMap *h,
    const char *key,
    size_t len,
    variant (*function)(variant)
)
{
    unsigned int i = 0, j = 0;
    uintptr_t hash = 0;
    _item *ptr = NULL;
    variant res = { 0 };
    uint32_t mask = 0;

    if (unlikely(! h || ! key || ! len)) {
        debug("map_get_with(): bad parameters.\n");
        return res;
    }

    if (_map_rdlock(h->_lock) == -1) return res;

    mask = h->_bucket_size - 1;
    hash = _hash(key, len, h->_seed[i]);
    j = hash & mask; goto _loop;

    for (i = 0; i < HASH_COUNT; i ++) {
        j = _hash(key, len, h->_seed[i]) & mask;

        /* if an empty slot is found, no need to look further */
_loop:  if (! (ptr = h->_bucket[j]) ) break;

        if (hash == (uintptr_t) ptr->ptr && likely(ptr->key.len == len)) {
            if (likely(! memcmp(ptr->key.str, key, len))) {
                res = ptr->val; if (function) res = function(res);
                goto _result;
            }
        }
    }

    /* unlucky, scan the basket for orphan keys */
    for (ptr = h->_basket; ptr; ptr = ptr->ptr) {
        if (ptr->key.len == len && memcmp(ptr->key.str, key, len) == 0) {
            res = ptr->val; if (function) res = function(res);
            goto _result;
        }
    }

_result:
    _map_unlock(h->_lock);

    return res;
}

/* -------------------------------------------------------------------------- */

public variant map_get(ASKL_LinkedMap *h, const char *key, size_t len)
{
    return map_get_with(h, key, len, NULL);
}

/* -------------------------------------------------------------------------- */

public int map_merge(
    ASKL_LinkedMap *dest,
    ASKL_LinkedMap *src,
    variant merge(const char *key, size_t len, variant dest, variant src)
)
{
    _bucket *b = NULL, *next = NULL;

    if (! dest || ! src || ! merge) {
        debug("map_union(): bad parameters.\n");
        return -1;
    }

    /* pry both maps open */
    if (_map_wrlock(dest->_lock) == -1) return -1;
    if (_map_wrlock(src->_lock) == -1) {
        _map_unlock(dest->_lock);
        return -1;
    }

    _map_break_lock(src->_lock);

    for (b = src->_index, src->_index = NULL; b; b = next) {
        variant v = variant_null();
        next = b->next;

        if (! b->item.key.len) { free(b); continue; }

        b->item.ptr = NULL;

        /* handle conflicts with the merge helper */
        if (_set_item(dest, & b->item, 1, & v)) {
            variant ret;
            ret = merge(b->item.key.str, b->item.key.len, v, b->item.val);
            if (v.value.pointer != ret.value.pointer) {
                if (dest->_freeval) dest->_freeval(v);
                if (ret.value.pointer != b->item.val.value.pointer)
                    if (src->_freeval) src->_freeval(b->item.val);
                _set_item(dest, & b->item, 1, & ret);
            } else if (src->_freeval) src->_freeval(b->item.val);
            free(b);
        } else {
            b->next = dest->_index; dest->_index = b;
        }
    }

    /* destroy the source map */
    free(src->_bucket);
    pthread_cond_destroy(& src->_lock->cond);
    pthread_mutex_destroy(& src->_lock->mutex);
    free(src->_lock); free(src);

    _map_unlock(dest->_lock);

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

    if (_map_wrlock(h->_lock) == -1) return;

    for (bucket = h->_index; bucket; bucket = bucket->next) {
        if (bucket->item.key.len) {
            int ret = f(
                bucket->item.key.str,
                bucket->item.key.len,
                bucket->item.val
            );
            if (ret == -1) {
                /* delete the record */
                bucket->item.key.len = 0;
            }
        }
    }

    _map_unlock(h->_lock);

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

    if (_map_wrlock(h->_lock) == -1) return -1;

    /* simple merge sort */
    do {
        l[0] = h->_index; h->_index = NULL; tail = NULL;

        for (merge = 0; (l[1] = l[0]); merge ++, l[0] = l[1]) {

            /* split the table in 2 sorted lists of up to `size` buckets */
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

    _map_unlock(h->_lock);

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

public variant map_remove(ASKL_LinkedMap *h, const char *key, size_t len)
{
    unsigned int i = 0, j = 0;
    _item *tmp = NULL, *prev = NULL;
    variant result = { 0 };
    uint32_t mask = 0;

    if (! h || ! key || ! len) {
        debug("map_remove(): bad parameters.\n");
        return result;
    }

    if (_map_wrlock(h->_lock) == -1) return result;

    mask = h->_bucket_size - 1;

    for (i = 0; i < HASH_COUNT; i ++) {
        j = _hash(key, len, h->_seed[i]) & mask;

        if (! h->_bucket[j] || h->_bucket[j]->key.len != len) continue;

        if (memcmp(h->_bucket[j]->key.str, key, len) == 0) {
            /* remove from the bucket */
            result = h->_bucket[j]->val;
            /* a length of 0 indicates a tombstone */
            h->_bucket[j]->key.len = 0;
            _map_unlock(h->_lock);
            return result;
        }
    }

    /* unlucky, scan the basket for orphan keys */
    for (tmp = prev = h->_basket; tmp; prev = tmp, tmp = tmp->ptr) {
        if (tmp->key.len == len) {
            if (memcmp(tmp->key.str, key, len) == 0) {
                /* remove the orphan from the basket */
                result = tmp->val;
                if (tmp == h->_basket) h->_basket = tmp->ptr;
                else prev->ptr = tmp->ptr;

                /* XXX tombstone for garbage collection */
                tmp->key.len = 0;
                _map_unlock(h->_lock);
                return result;
            }
        }
    }

    /* garbage collection if necessary */
    _resize(h, (size_t) (h->_bucket_count * HASH_RATIO));

    _map_unlock(h->_lock);

    return variant_null();
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

    /* the lock is dynamically allocated */
    ret += sizeof(*h->_lock);

    if (_map_wrlock(h->_lock) == -1) return 0;

    if (h->_bucket_size) {
        /* segment bucket size */
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

    _map_unlock(h->_lock);

    if (overhead) *overhead = ret - key;

    return ret;
}

/* -------------------------------------------------------------------------- */

public ASKL_LinkedMap *map_free(ASKL_LinkedMap *h)
{
    _bucket *bucket = NULL, *next = NULL;

    if (! h) return NULL;

    if (_map_wrlock(h->_lock) == -1) return NULL;

    /* free the threads waiting after the linked hashmap */
    _map_break_lock(h->_lock);

    for (bucket = h->_index; bucket; bucket = next) {
        next = bucket->next;
        if (h->_freeval && bucket->item.key.len)
            h->_freeval(bucket->item.val);
        free(bucket);
    }

    free(h->_bucket);
    pthread_cond_destroy(& h->_lock->cond);
    pthread_mutex_destroy(& h->_lock->mutex);
    free(h->_lock); free(h);

    return NULL;
}

/* -------------------------------------------------------------------------- */

public ASKL_HashTable *htable_alloc(void (*freeval)(variant))
{
    unsigned int i = 0;
    ASKL_HashTable *h = malloc(sizeof(*h));

    if (! h) {
        perror(ERR(htable_alloc, malloc));
        return NULL;
    }

    for (i = 0; i < 256; i ++) h->_segment[i] = map_alloc(freeval);

    return h;
}

/* -------------------------------------------------------------------------- */

public variant htable_set(ASKL_HashTable *h, const char *k, size_t l, variant v)
{
    if (! h || ! k || ! l) {
        debug("htable_set(): bad parameters.\n");
        return v;
    }

    return _insert(h->_segment[_crc8(k, l)], k, l, v, 1);
}

/* -------------------------------------------------------------------------- */

public variant htable_insert(
    ASKL_HashTable *h,
    const char *k,
    size_t l,
    variant v
)
{
    if (! h || ! k || ! l) {
        debug("htable_insert(): bad parameters.\n");
        return v;
    }

    return _insert(h->_segment[_crc8(k, l)], k, l, v, -1);
}

/* -------------------------------------------------------------------------- */

public variant htable_get_with(
    ASKL_HashTable *h,
    const char *key,
    size_t len,
    variant (*function)(variant)
)
{
    if (! h || ! key || ! len) {
        debug("htable_get_with(): bad parameters.\n");
        return variant_null();
    }

    return map_get_with(h->_segment[_crc8(key, len)], key, len, function);
}

/* -------------------------------------------------------------------------- */

public variant htable_get(ASKL_HashTable *h, const char *key, size_t len)
{
    if (! h || ! key || ! len) {
        debug("htable_get(): bad parameters.\n");
        return variant_null();
    }

    return map_get_with(h->_segment[_crc8(key, len)], key, len, NULL);
}

/* -------------------------------------------------------------------------- */

public void htable_foreach(
    ASKL_HashTable *h,
    int (*function)(const char *, size_t, variant)
)
{
    unsigned int i = 0;

    if (! h || ! function) return;

    for (i = 0; i < 256; i ++) {
        if (h->_segment[i]->_index)
            map_foreach(h->_segment[i], function);
    }

    return;
}

/* -------------------------------------------------------------------------- */

public variant htable_remove(ASKL_HashTable *h, const char *key, size_t len)
{
    if (! h || ! key || ! len) {
        debug("htable_remove(): bad parameters.\n");
        return variant_null();
    }

    return map_remove(h->_segment[_crc8(key, len)], key, len);
}

/* -------------------------------------------------------------------------- */

public size_t htable_footprint(ASKL_HashTable *h, size_t *overhead)
{
    unsigned int i = 0;
    size_t key = 0, over = 0;
    size_t ret = sizeof(*h);

    if (! h) {
        debug("htable_footprint(): bad parameters.\n");
        return 0;
    }

    ret += 256 * sizeof(void *);

    for (i = 0; i < 256; i ++) {
        ret += map_footprint(h->_segment[i], & over);
        key += over; over = 0;
    }

    if (overhead) *overhead = key;

    return ret;
}

/* -------------------------------------------------------------------------- */

public ASKL_HashTable *htable_free(ASKL_HashTable *h)
{
    unsigned int i = 0;

    if (! h) return NULL;

    for (i = 0; i < 256; i ++) map_free(h->_segment[i]); free(h);

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
