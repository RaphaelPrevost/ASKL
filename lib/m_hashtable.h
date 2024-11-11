/*******************************************************************************
 *  Concrete Server                                                            *
 *  Copyright (c) 2005-2019 Raphael Prevost <raph@el.bzh>                      *
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

#ifndef M_HASHTABLE_H

#define M_HASHTABLE_H

#ifdef _ENABLE_RANDOM

/* required build configuration */
#ifndef _ENABLE_RANDOM
#error "Concrete: the hashtable module requires the builtin PRNG."
#endif

#include "m_core_def.h"
#include "m_variant.h"
#include "m_random.h"

#define CACHE_HASHFNCOUNT         8    /* number of hash functions to use */
#define CACHE_CUCKOORETRY         4    /* retries if the bucket is full */
#define CACHE_GROWTHRATIO       1.2    /* threshold to grow the table (80%) */

#define CACHE_ASC                 0
#define CACHE_DESC                1

/** @defgroup cache module::cache */

typedef struct m_cache {
    pthread_rwlock_t *_lock;
    size_t _bucket_size;
    size_t _bucket_count;
    struct _m_bucket *_index;
    struct _m_item **_bucket;
    struct _m_item *_basket;
    void (*_freeval)(variant);
    unsigned int _seed[CACHE_HASHFNCOUNT];
} m_cache;

/**
 * @ingroup cache
 * @struct m_cache
 *
 * This structure supports the implementation of a thread safe hash table.
 *
 * Each key/value pair is stored in this structure with an overhead of
 * sizeof(uint16_t) + sizeof(char *) bytes.
 *
 * @Note Even if this structure is thread safe, it is not recommended
 * to use it in a context where concurrency is important, due to its
 * simplistic locking scheme. For good concurrency, it is better to use
 * the @ref m_hashtable structure and the related API.
 *
 */

typedef struct m_hashtable {
    m_cache *_segment[256];
} m_hashtable;

/* -------------------------------------------------------------------------- */

public m_cache *cache_alloc(void (*freeval)(variant));

/* -------------------------------------------------------------------------- */

public variant cache_push(m_cache *h, const char *k, size_t l, variant v);

/* -------------------------------------------------------------------------- */

public variant cache_add(m_cache *h, const char *k, size_t l, variant v);

/* -------------------------------------------------------------------------- */

public variant cache_lookup(m_cache *h, const char *key, size_t len,
                            variant (*function)(variant));

/* -------------------------------------------------------------------------- */

public variant cache_find(m_cache *h, const char *key, size_t len);

/* -------------------------------------------------------------------------- */

public void cache_foreach(m_cache *h,
                          int (*function)(const char *, size_t, variant));

/* -------------------------------------------------------------------------- */

public int cache_sort(m_cache *h, unsigned int order,
                      int (*cmp)(const char *, const char *, size_t,
                                 variant, variant));

/* -------------------------------------------------------------------------- */

public int cache_sort_keys(const char *key0, const char *key1, size_t l,
                           UNUSED variant val0, UNUSED variant val1);

/* -------------------------------------------------------------------------- */

public variant cache_pop(m_cache *h, const char *key, size_t len);

/* -------------------------------------------------------------------------- */

public size_t cache_footprint(m_cache *h, size_t *overhead);

/* -------------------------------------------------------------------------- */

public m_cache *cache_free(m_cache *h);

/* -------------------------------------------------------------------------- */
/* Segmented hash table */
/* -------------------------------------------------------------------------- */

public m_hashtable *hashtable_alloc(void (*freeval)(variant));

/* -------------------------------------------------------------------------- */

public size_t hashtable_footprint(m_hashtable *h, size_t *overhead);

/* -------------------------------------------------------------------------- */

public variant hashtable_insert(m_hashtable *h, const char *k, size_t l, variant v);

/* -------------------------------------------------------------------------- */

public variant hashtable_update(m_hashtable *h, const char *k, size_t l, variant v);

/* -------------------------------------------------------------------------- */

public variant hashtable_remove(m_hashtable *h, const char *key, size_t len);

/* -------------------------------------------------------------------------- */

public variant hashtable_lookup(m_hashtable *h, const char *key,
                                size_t len, variant (*function)(variant));

/* -------------------------------------------------------------------------- */

public void hashtable_foreach(m_hashtable *h,
                              int (*function)(const char *, size_t, variant));

/* -------------------------------------------------------------------------- */

public variant hashtable_find(m_hashtable *h, const char *key, size_t len);

/* -------------------------------------------------------------------------- */

public m_hashtable *hashtable_free(m_hashtable *h);

/* -------------------------------------------------------------------------- */

/* _ENABLE_HASHTABLE */
#endif

#endif
