/*******************************************************************************
 *  Concrete Server                                                            *
 *  Copyright (c) 2005-2022 Raphael Prevost <raph@el.bzh>                      *
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

#include "m_hashtable.h"

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_HASHTABLE
/* -------------------------------------------------------------------------- */

typedef struct _m_item {
    void *ptr;
    variant val;
    struct {
        uint16_t len;
        char str[];
    } key;
} _m_item;

typedef struct _m_bucket {
    struct _m_bucket *next;
    _m_item item; 
} _m_bucket;

/* -------------------------------------------------------------------------- */
/* BITWISE OPERATIONS */
/* -------------------------------------------------------------------------- */

#include "ports/m_port_bitops.c"

/* -------------------------------------------------------------------------- */
/* wyhash32 (author: 王一 Wang Yi <godspeed_china@yeah.net>) */
/* -------------------------------------------------------------------------- */

static uint32_t _wyr32(const uint8_t *p)
{
    uint32_t result;
    memcpy(& result, p, sizeof(result));
    #if defined(BIG_ENDIAN_HOST)
    return __bswap32(result);
    #else
    return result;
    #endif
}

/* -------------------------------------------------------------------------- */

static uint32_t _wyr24(const uint8_t *p, uint32_t k)
{
    return (((uint32_t) p[0]) << 16) | (((uint32_t) p[k >> 1]) << 8) | p[k - 1];
}

/* -------------------------------------------------------------------------- */

static void _wymix32(uint32_t *a, uint32_t *b)
{
    uint64_t c = *a ^ 0x53c5ca59u;
    c *= *b ^ 0x74743c1bu;
    *a = (uint32_t) c;
    *b = (uint32_t) (c >> 32);
}

/* -------------------------------------------------------------------------- */

static uint32_t _hash(const char *key, uint16_t len, uint32_t seed)
{
    const uint8_t *p = (const uint8_t *) key;
    uint32_t i, see1 = len;

    _wymix32(& seed, & see1);

    for (i = len; i > 8; i -= 8, p += 8) {
        seed ^= _wyr32(p);
        see1 ^= _wyr32(p + 4);
        _wymix32(& seed, & see1);
    }

    if (i >= 4) {
        seed ^= _wyr32(p);
        see1 ^= _wyr32(p + i - 4);
    } else if (i) seed ^= _wyr24(p, i);

    _wymix32(& seed, & see1);
    _wymix32(& seed, & see1);

    return seed ^ see1;
}

/* -------------------------------------------------------------------------- */

static int _cache_push(m_cache *h, _m_item *item, int replace, variant *val)
{
    unsigned int i = 0;
    uintptr_t index = 0;
    unsigned int retry = 0;
    unsigned int hash_cache[CACHE_HASHFNCOUNT];
    _m_item *slot = NULL, *tmp = NULL;
    uint32_t mask = h->_bucket_size - 1;

    /* avoid rehashing every key */
    if (! (index = (uintptr_t) item->ptr) ) {
        index = _hash(item->key.str, item->key.len, h->_seed[0]);
        item->ptr = (void *) index;
    }

    index &= mask; goto _loop;

    /* look for a free slot */
    for (i = 0; i < CACHE_HASHFNCOUNT; i ++) {
        index = _hash(item->key.str, item->key.len, h->_seed[i]) & mask;

_loop:  if (! h->_bucket[index]) {
            /* this slot is free, insert */
            h->_bucket[index] = item;
            h->_bucket_count ++;
            return 0;
        }

        if (replace) {
            slot = h->_bucket[index];
            if (likely(item->key.len == slot->key.len)) {
                if (! memcmp(& item->key.str, & slot->key.str, item->key.len))
                    goto _replace;
            }
        }

        hash_cache[i] = index;
    }

    if (replace) {
        /* couldn't find it, look in the basket */
        for (slot = h->_basket; slot; slot = slot->ptr) {
            if (likely(item->key.len == slot->key.len)) {
                if (! memcmp(& item->key.str, & slot->key.str, item->key.len))
                    goto _replace;
            }
        }
    }

    /* no free slot found, try cuckoo hashing */
    for (index = hash_cache[0]; retry < CACHE_CUCKOORETRY; retry ++) {
        /* get data off the last slot and replace them */
        tmp = h->_bucket[index]; h->_bucket[index] = item; item = tmp;

        /* get rid of tombstones */
        if (! item->key.len) return 0;

        for (i = 0; i < CACHE_HASHFNCOUNT; i ++) {
            index = _hash(item->key.str, item->key.len, h->_seed[i]) & mask;

            if (! h->_bucket[index]) {
                /* this slot is free, insert */
                h->_bucket[index] = item;
                h->_bucket_count ++;
                return 0;
            } else hash_cache[i] = index;
        }
    }

    /* cuckoo hashing did not help, store the key in the basket... */
    item->ptr = h->_basket; h->_basket = item; h->_bucket_count ++;

    return 0;

_replace:
    if (replace == -1) *val = item->val;
    else { *val = slot->val; slot->val = item->val; }
    return 1;
}

/* -------------------------------------------------------------------------- */

static int _cache_resize(m_cache *h, size_t size)
{
    _m_bucket *b = NULL, *next = NULL;
    _m_item *item = NULL, *tmp = NULL;

    if (! h) {
        debug("_cache_resize(): bad parameters.\n");
        return -1;
    }

    if (h->_bucket_count * CACHE_GROWTHRATIO < h->_bucket_size) return 0;

    /* round the size to the next highest power of 2 */
    size --; size |= size >> 1; size |= size >> 2;
    size |= size >> 4; size |= size >> 8; size |= size >> 16;
    size ++; size += (size == 0);

    /* clear the basket */
    for (item = h->_basket, h->_basket = NULL; item; item = tmp) {
        tmp = item->ptr;
        item->ptr = NULL;
    }

    free(h->_bucket);

    /* try to resize the bucket array */
    if (! (h->_bucket = calloc(size, sizeof(*h->_bucket))) ) {
        perror(ERR(_cache_resize, calloc));
        return -1;
    }

    /* update the state */
    h->_bucket_size = size; h->_bucket_count = 0;

    /* rehash old buckets */
    for (b = h->_index, h->_index = NULL; b; b = next) {
        next = b->next;

        if (! b->item.key.len) { free(b); continue; }

        _cache_push(h, & b->item, 0, NULL);

        b->next = h->_index; h->_index = b;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

public m_cache *cache_alloc(void (*freeval)(variant))
{
    unsigned int i = 0;
    m_random *r = NULL;
    m_cache *h = malloc(sizeof(*h));

    if (! h) {
        perror(ERR(cache_alloc, malloc));
        return NULL;
    }

    if (! (r = random_arrayinit((void *) h, sizeof(h))) ) goto _err_rand;

    if (! (h->_lock = malloc(sizeof(*h->_lock))) ) {
        perror(ERR(cache_alloc, malloc));
        goto _err_lock;
    }

    if (pthread_rwlock_init(h->_lock, NULL) == -1) {
        perror(ERR(cache_alloc, pthread_rwlock_init));
        goto _err_init;
    }

    h->_bucket = NULL;
    h->_bucket_count = h->_bucket_size = 0; h->_basket = NULL;
    h->_index = NULL;
    h->_freeval = freeval;

    for (i = 0; i < CACHE_HASHFNCOUNT; i ++) {
        h->_seed[i] = random_uint32(r);
        /* known bad seeds */
        if ((h->_seed[i] == 0x429dacdd) ||
            (h->_seed[i] == 0x51a43a0f) ||
            (h->_seed[i] == 0x522235ae) ||
            (h->_seed[i] == 0x99ac2b20) ||
            (h->_seed[i] == 0x9a4f1376) ||
            (h->_seed[i] == 0xd637dbf3))
            h->_seed[i] ++;
    }

    if (_cache_resize(h, 4) == -1) {
        debug("cache_alloc(): cannot resize the hash table.\n");
        goto _err_size;
    }

    r = random_free(r);

    return h;

_err_size:
    free(h->_bucket);
    free(h->_basket);
    pthread_rwlock_destroy(h->_lock);
_err_init:
    free(h->_lock);
_err_lock:
    r = random_free(r);
_err_rand:
    free(h);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static variant _cache_add(m_cache *h, const char *key, size_t len, variant val,
                          int replace)
{
    _m_bucket *bucket = NULL;

    if (! h || ! key || ! len) {
        debug("_cache_add(): bad parameters.\n");
        return val;
    }

    /* replace the key by a dynamically allocated one */
    if (! (bucket = malloc(sizeof(*bucket) + len)) ) {
        perror(ERR(cache_push, malloc));
        return val;
    }

    memcpy(bucket->item.key.str, key, len);
    bucket->item.key.len = len;
    bucket->item.val = val;
    bucket->item.ptr = NULL;

    pthread_rwlock_wrlock(h->_lock);

    if (! _cache_push(h, & bucket->item, replace, & val)) {
        bucket->next = h->_index; h->_index = bucket;
        val = variant_null();
    } else free(bucket);

    /* try to expand the hashtable if the load is too important */
    if (h->_basket && h->_basket->ptr)
        _cache_resize(h, h->_bucket_size + 1);

    pthread_rwlock_unlock(h->_lock);

    return val;
}

/* -------------------------------------------------------------------------- */

public variant cache_push(m_cache *h, const char *k, size_t l, variant v)
{
    return _cache_add(h, k, l, v, 1);
}

/* -------------------------------------------------------------------------- */

public variant cache_add(m_cache *h, const char *k, size_t l, variant v)
{
    return _cache_add(h, k, l, v, -1);
}

/* -------------------------------------------------------------------------- */

public variant cache_lookup(m_cache *h, const char *key, size_t len,
                            variant (*function)(variant))
{
    unsigned int i = 0, j = 0;
    _m_item *ptr = NULL;
    variant res = { 0 };
    uint32_t mask = 0;

    if (! h || ! key || ! len) {
        debug("cache_lookup(): bad parameters.\n");
        return res;
    }

    pthread_rwlock_rdlock(h->_lock);

    mask = h->_bucket_size - 1;

    for (i = 0; i < CACHE_HASHFNCOUNT; i ++) {
        j = _hash(key, len, h->_seed[i]) & mask;

        /* if an empty slot is found, no need to look further */
        if (! (ptr = h->_bucket[j])) break;

        if (ptr->key.len == len && memcmp(ptr->key.str, key, len) == 0) {
            res = ptr->val; if (function) res = function(res);
            goto _result;
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
    pthread_rwlock_unlock(h->_lock);

    return res;
}

/* -------------------------------------------------------------------------- */

public variant cache_find(m_cache *h, const char *key, size_t len)
{
    return cache_lookup(h, key, len, NULL);
}

/* -------------------------------------------------------------------------- */

public void cache_foreach(m_cache *h, int (*f)(const char *, size_t, variant))
{
    _m_bucket *bucket = NULL;

    if (! h || ! f) {
        debug("cache_foreach(): bad parameters.\n");
        return;
    }

    pthread_rwlock_wrlock(h->_lock);

    for (bucket = h->_index; bucket; bucket = bucket->next) {
        if (bucket->item.key.len) {
            if (f(bucket->item.key.str, bucket->item.key.len, bucket->item.val) == -1) {
                /* delete the record */
                bucket->item.key.len = 0;
            }
        }
    }

    pthread_rwlock_unlock(h->_lock);

    return;
}

/* -------------------------------------------------------------------------- */

public int cache_sort(m_cache *h, unsigned int order,
                      int (*cmp)(const char *key0, const char *key1, size_t len,
                                 variant value0, variant value1))
{
    _m_bucket *l[2] = { NULL, NULL }, *bucket = NULL, *tail = NULL;
    unsigned int size = 1, merge = 0, i = 0;
    _m_item *a = NULL, *b = NULL;
    unsigned int c[2] = { 0, 0 };

    if (! h || ! cmp || (order != CACHE_ASC && order != CACHE_DESC) ) {
        debug("cache_sort(): bad parameters.\n");
        return -1;
    }

    pthread_rwlock_wrlock(h->_lock);

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

    pthread_rwlock_unlock(h->_lock);

    return 0;
}

/* -------------------------------------------------------------------------- */

public int cache_sort_keys(const char *key0, const char *key1, size_t l,
                           UNUSED variant val0, UNUSED variant val1)
{
    return memcmp(key0, key1, l);
}

/* -------------------------------------------------------------------------- */

public variant cache_pop(m_cache *h, const char *key, size_t len)
{
    unsigned int i = 0, j = 0;
    _m_item *tmp = NULL, *prev = NULL;
    variant result = { 0 };
    uint32_t mask = 0;

    if (! h || ! key || ! len) {
        debug("cache_pop(): bad parameters.\n");
        return result;
    }

    pthread_rwlock_wrlock(h->_lock);

    mask = h->_bucket_size - 1;

    for (i = 0; i < CACHE_HASHFNCOUNT; i ++) {
        j = _hash(key, len, h->_seed[i]) & mask;

        if (! h->_bucket[j] || h->_bucket[j]->key.len != len) continue;

        if (memcmp(h->_bucket[j]->key.str, key, len) == 0) {
            /* remove from the bucket */
            result = h->_bucket[j]->val;
            /* a length of 0 indicates a tombstone */
            h->_bucket[j]->key.len = 0;
            pthread_rwlock_unlock(h->_lock);
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

                pthread_rwlock_unlock(h->_lock);

                return result;
            }
        }
    }

    /* garbage collection if necessary */
    _cache_resize(h, (size_t) (h->_bucket_count * CACHE_GROWTHRATIO));

    pthread_rwlock_unlock(h->_lock);

    return variant_null();
}

/* -------------------------------------------------------------------------- */

public size_t cache_footprint(m_cache *h, size_t *overhead)
{
    _m_bucket *bucket = NULL;
    size_t key = 0;
    size_t ret = sizeof(*h);

    if (! h) {
        debug("cache_footprint(): bad parameters.\n");
        return 0;
    }

    /* the lock is dynamically allocated */
    ret += sizeof(*h->_lock);

    pthread_rwlock_wrlock(h->_lock);

    if (h->_bucket_size) {
        /* segment bucket size */
        ret += h->_bucket_size * sizeof(*h->_bucket);
        /* keys */
        for (bucket = h->_index; bucket; bucket = bucket->next) {
            if (bucket->item.key.len) {
                /* key length + key recorded size + next and value pointers */
                ret += sizeof(char *) + bucket->item.key.len + sizeof(bucket->item.key.len) +
                       sizeof(char *) + sizeof(variant);
                /* key length and value pointer are not overhead */
                key += sizeof(bucket->item.key.len) + bucket->item.key.len + sizeof(void *);
            }
        }
    }

    pthread_rwlock_unlock(h->_lock);

    if (overhead) *overhead = ret - key;

    return ret;
}

/* -------------------------------------------------------------------------- */

public m_cache *cache_free(m_cache *h)
{
    _m_bucket *bucket = NULL, *next = NULL;

    if (! h) return NULL;

    for (bucket = h->_index; bucket; bucket = next) {
        next = bucket->next;
        if (h->_freeval && bucket->item.key.len)
            h->_freeval(bucket->item.val);
        free(bucket);
    }

    free(h->_bucket);
    pthread_rwlock_destroy(h->_lock);
    free(h->_lock); free(h);

    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_hashtable *hashtable_alloc(void (*freeval)(variant))
{
    unsigned int i = 0;

    m_hashtable *h = malloc(sizeof(*h));

    if (! h) {
        perror(ERR(hash_alloc, malloc));
        return NULL;
    }

    for (i = 0; i < 256; i ++) h->_segment[i] = cache_alloc(freeval);

    return h;
}

/* -------------------------------------------------------------------------- */

public size_t hashtable_footprint(m_hashtable *h, size_t *overhead)
{
    unsigned int i = 0;
    size_t key = 0, over = 0;
    size_t ret = sizeof(*h);

    if (! h) {
        debug("hashtable_footprint(): bad parameters.\n");
        return 0;
    }

    ret += 256 * sizeof(void *);

    for (i = 0; i < 256; i ++) {
        ret += cache_footprint(h->_segment[i], & over);
        key += over; over = 0;
    }

    if (overhead) *overhead = key;

    return ret;
}

/* -------------------------------------------------------------------------- */

public variant hashtable_insert(m_hashtable *h, const char *k, size_t l, variant v)
{
    if (! k || ! l) {
        variant val = { 0 };
        return val;
    }

    if (! h) return v;

    return _cache_add(h->_segment[_crc8(k, l)], k, l, v, -1);
}

/* -------------------------------------------------------------------------- */

public variant hashtable_update(m_hashtable *h, const char *k, size_t l, variant v)
{
    if (! k || ! l) {
        variant val = { 0 };
        return val;
    }

    if (! h) return v;

    return _cache_add(h->_segment[_crc8(k, l)], k, l, v, 1);
}

/* -------------------------------------------------------------------------- */

public variant hashtable_remove(m_hashtable *h, const char *key, size_t len)
{
    if (! h || ! key || ! len) {
        variant val = { 0 };
        return val;
    }

    return cache_pop(h->_segment[_crc8(key, len)], key, len);
}

/* -------------------------------------------------------------------------- */

public variant hashtable_lookup(m_hashtable *h, const char *key,
                                size_t len, variant (*function)(variant))
{
    if (! h || ! key || ! len) {
        variant val = { 0 };
        return val;
    }

    return cache_lookup(h->_segment[_crc8(key, len)], key, len, function);
}

/* -------------------------------------------------------------------------- */

public void hashtable_foreach(m_hashtable *h,
                              int (*function)(const char *, size_t, variant))
{
    unsigned int i = 0;

    if (! h || ! function) return;

    for (i = 0; i < 256; i ++) {
        if (h->_segment[i]->_index)
            cache_foreach(h->_segment[i], function);
    }

    return;
}

/* -------------------------------------------------------------------------- */

public variant hashtable_find(m_hashtable *h, const char *key, size_t len)
{
    if (! h || ! key || ! len) {
        variant val = { 0 };
        return val;
    }

    return cache_find(h->_segment[_crc8(key, len)], key, len);
}

/* -------------------------------------------------------------------------- */

public m_hashtable *hashtable_free(m_hashtable *h)
{
    unsigned int i = 0;

    if (! h) return NULL;

    for (i = 0; i < 256; i ++) cache_free(h->_segment[i]); free(h);

    return NULL;
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

/* Hashtable support will not be compiled in the Concrete Library */
#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
