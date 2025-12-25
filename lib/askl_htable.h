/*******************************************************************************
 *  ASKL.                                                                      *
 *  Copyright (c) 2025 Raphael Prevost <raph@el.bzh>                           *
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

#ifndef ASKL_HASHTABLE_H

#define ASKL_HASHTABLE_H

#include "askl.h"
#include "askl_variant.h"

#define MAP_ASC                 0
#define MAP_DESC                1

/** @defgroup hashtable ASKL::hashtable */

typedef struct _ASKL_LinkedMap ASKL_LinkedMap;

/**
 * @ingroup hashtable
 * @struct ASKL_LinkedMap
 *
 * This structure supports the implementation of a thread safe hash table.
 *
 * Each key/value pair is stored in this structure with an overhead of
 * sizeof(uint16_t) + sizeof(char *) bytes.
 *
 * @Note Even if this structure is thread safe, it is not recommended
 * to use it in a context where concurrency is important, due to its
 * simplistic locking scheme. For good concurrency, it is better to use
 * the @ref ASKL_HashTable structure and the related API.
 *
 */

typedef struct _ASKL_HashTable ASKL_HashTable;

/* -------------------------------------------------------------------------- */

public ASKL_LinkedMap *map_alloc(void (*freeval)(variant));

/**
 * @ingroup hashtable
 * @fn ASKL_LinkedMap *map_alloc(void (*freeval)(variant))
 * @param freeval optional callback used to destroy stored values
 * @return a pointer to a newly allocated @ref ASKL_LinkedMap, or NULL on error
 *
 * This function allocates and initializes a new concurrent linked hash map.
 *
 * If @p freeval is not NULL, it will be called once for each remaining value
 * stored in the map when @ref map_free() is called, or when entries are
 * removed via @ref map_foreach().
 *
 * The returned hashmap must be destroyed with @ref map_free() when no longer
 * needed.
 */

/* -------------------------------------------------------------------------- */

public variant map_set(ASKL_LinkedMap *h, const char *k, size_t l, variant v);

/**
 * @ingroup hashtable
 * @fn variant map_set(ASKL_LinkedMap *h, const char *k, size_t l, variant v)
 * @param h a pointer to a linked hashmap
 * @param k pointer to the key bytes (not necessarily NUL-terminated)
 * @param l length of the key in bytes
 * @param v the value to store
 * @return the previous value associated with @p k, or VARIANT_NULL if none
 *
 * This function stores the value @p v under key @p k into the map.
 * If an entry with the same key already exists, its value is replaced and the
 * previous value is returned. Otherwise, the value is inserted and
 * VARIANT_NULL is returned.
 */

/* -------------------------------------------------------------------------- */

public variant map_insert(ASKL_LinkedMap *h, const char *k, size_t l, variant v);

/**
 * @ingroup hashtable
 * @fn variant map_insert(ASKL_LinkedMap *h, const char *k, size_t l, variant v)
 * @param h a pointer to a linked hashmap
 * @param k pointer to the key bytes (not necessarily NUL-terminated)
 * @param l length of the key in bytes
 * @param v the value to store
 * @return the existing value associated with @p k if it already existed,
 *         or VARIANT_NULL if the key was newly inserted
 *
 * This function performs an insert-only operation. If no entry with the given
 * key exists, the key/value pair is inserted and VARIANT_NULL is returned.
 * If an entry already exists, the map is left unchanged and the existing
 * value is returned.
 *
 * This is useful when the caller wants to create an entry only if it does not
 * already exist, and otherwise reuse the previous value.
 */

/* -------------------------------------------------------------------------- */

public variant map_get_with(
    ASKL_LinkedMap *h,
    const char *key,
    size_t len,
    variant (*function)(variant)
);

/**
 * @ingroup hashtable
 * @fn variant map_get_with(ASKL_LinkedMap *h, const char *key, size_t len,
 *                          variant (*function)(variant))
 * @param h        a pointer to a linked hashmap
 * @param key      pointer to the key bytes
 * @param len      length of the key in bytes
 * @param function optional callback applied to the stored value
 * @return the stored value, the result of @p function, or VARIANT_NULL
 *
 * This function looks up the entry associated with @p key in @p h.
 *
 * If the key is found and @p function is NULL, the stored value is returned.
 * If @p function is non-NULL, it is called with the stored value as argument
 * and its return value is returned instead.
 *
 * If the key is not present in the map, VARIANT_NULL is returned.
 * 
 * @note The callback @p function is executed while the map's read lock is
 *       held, ensuring atomicity. This is useful for acquiring locks on
 *       stored objects, or other operations that must be atomic with the
 *       lookup. The callback must be fast and non-blocking.
 *
 * @warning The callback @p function must not free the stored value or
 *          otherwise invalidate it.
 */

/* -------------------------------------------------------------------------- */

public variant map_get(ASKL_LinkedMap *h, const char *key, size_t len);

/**
 * @ingroup hashtable
 * @fn variant map_get(ASKL_LinkedMap *h, const char *key, size_t len)
 * @param h   a pointer to a linked hashmap
 * @param key pointer to the key bytes
 * @param len length of the key in bytes
 * @return the stored value, or VARIANT_NULL if the key is not present
 *
 * This is a convenience wrapper around @ref map_get_with() with a NULL
 * callback. It simply returns the value associated with @p key, or
 * VARIANT_NULL if the key is not in the map.
 */

/* -------------------------------------------------------------------------- */

public void map_foreach(
    ASKL_LinkedMap *h,
    int (*function)(const char *, size_t, variant)
);

/**
 * @ingroup hashtable
 * @fn void map_foreach(ASKL_LinkedMap *h,
 *                      int (*function)(const char *, size_t, variant))
 * @param h        a pointer to a linked hashmap
 * @param function a callback invoked once per key/value pair
 * @return void
 *
 * This function iterates over all entries in the hashmap and calls @p function
 * for each key/value pair. The callback receives:
 *  - the key pointer (NUL-terminated),
 *  - the key length in bytes,
 *  - the associated value.
 *
 * If @p function returns -1 for an entry, that entry is removed from the
 * map. If the hashmap was created with a @p freeval callback, it is invoked on
 * the value before the entry is destroyed.
 *
 * Any other return value from @p function is ignored and the iteration
 * continues.
 *
 * @note This function acquires a write lock on the hashmap for the entire
 *       duration of the traversal.
 */

/* -------------------------------------------------------------------------- */

public int map_sort(
    ASKL_LinkedMap *h,
    unsigned int order,
    int (*cmp)(
        const char *,
        const char *,
        size_t,
        variant,
        variant
    )
);

/**
 * @ingroup hashtable
 * @fn int map_sort(ASKL_LinkedMap *h, unsigned int order,
 *                  int (*cmp)(const char *, const char *, size_t,
 *                             variant, variant))
 * @param h     a pointer to a map
 * @param order sort order: @ref MAP_ASC or @ref MAP_DESC
 * @param cmp   comparison callback
 * @return 0 on success, -1 on error
 *
 * This function sorts the internal index of @p h using the user-provided
 * comparator @p cmp and a stable merge sort.
 *
 * The comparator receives:
 *  - @p key0, @p key1: pointers to NUL-terminated keys;
 *  - @p len:           length of the shorter key in bytes;
 *  - @p value0, @p value1: associated values.
 *
 * It must return:
 *  - a negative value if (key0,value0) should come before (key1,value1),
 *  - zero if they are considered equal for ordering purposes,
 *  - a positive value if (key0,value0) should come after (key1,value1).
 *
 * The @p order argument controls whether the resulting order is ascending
 * (MAP_ASC) or descending (MAP_DESC) with respect to @p cmp.
 *
 * @note Sorting only affects the order in which @ref map_foreach() visits
 *       entries. It does not change lookup semantics.
 */

/* -------------------------------------------------------------------------- */

public int map_sort_keys(
    const char *key0,
    const char *key1,
    size_t l,
    UNUSED variant val0,
    UNUSED variant val1
);

/**
 * @ingroup hashtable
 * @fn int map_sort_keys(const char *key0, const char *key1, size_t l,
 *                       variant val0, variant val1)
 * @param key0 first key
 * @param key1 second key
 * @param l    number of bytes to compare
 * @param val0 unused
 * @param val1 unused
 * @return an integer less than, equal to, or greater than zero
 *
 * This is a convenience comparator suitable for use with @ref map_sort().
 * It performs a simple @c memcmp() of the first @p l bytes of @p key0 and
 * @p key1 and ignores the values.
 */

/* -------------------------------------------------------------------------- */

public variant map_remove(ASKL_LinkedMap *h, const char *key, size_t len);

/**
 * @ingroup hashtable
 * @fn variant map_remove(ASKL_LinkedMap *h, const char *key, size_t len)
 * @param h   a pointer to a linked hashmap
 * @param key pointer to the key bytes
 * @param len length of the key in bytes
 * @return the removed value, or VARIANT_NULL if the key was not present
 *
 * This function removes the entry associated with @p key from @p h and
 * returns its value. If the key does not exist, VARIANT_NULL is returned.
 */

/* -------------------------------------------------------------------------- */

public size_t map_footprint(ASKL_LinkedMap *h, size_t *overhead);

/**
 * @ingroup hashtable
 * @fn size_t map_footprint(ASKL_LinkedMap *h, size_t *overhead)
 * @param h        a pointer to a linked hashmap
 * @param overhead optional pointer to receive the internal overhead, in bytes
 * @return the total memory footprint of the hashmap, in bytes
 *
 * This function computes an approximate memory footprint of the hashmap,
 * including:
 *  - the structure itself,
 *  - its dynamically allocated lock,
 *  - the bucket array,
 *  - all allocated items and their keys.
 *
 * If @p overhead is non-NULL, @c *overhead is set to the portion of @p h’s
 * memory that is considered overhead (metadata, buckets, etc.) rather than
 * user payload (keys and the pointer to the value).
 */

/* -------------------------------------------------------------------------- */

public ASKL_LinkedMap *map_free(ASKL_LinkedMap *h);

/**
 * @ingroup hashtable
 * @fn ASKL_LinkedMap *map_free(ASKL_LinkedMap *h)
 * @param h a pointer to a linked hashmap
 * @return always NULL
 *
 * This function destroys the hashmap @p h and frees all associated resources.
 * If a @p freeval callback was specified at creation time, it is called once
 * for each remaining stored value before the corresponding entry is freed.
 *
 * This function always returns NULL so it can be used to clear the pointer:
 * @code
 * map = map_free(map);
 * @endcode
 */

/* -------------------------------------------------------------------------- */
/* Segmented hash table */
/* -------------------------------------------------------------------------- */

public ASKL_HashTable *htable_alloc(void (*freeval)(variant));

/**
 * @ingroup hashtable
 * @fn ASKL_HashTable *htable_alloc(void (*freeval)(variant))
 * @param freeval optional callback used to destroy stored values
 * @return a pointer to a new @ref ASKL_HashTable, or NULL on error
 *
 * This function allocates and initializes a segmented hash table composed of
 * 256 independent @ref ASKL_LinkedMap segments. The @p freeval callback is
 * passed to each segment and behaves as described in @ref map_alloc().
 *
 * The returned hash table must be destroyed with @ref htable_free().
 */

/* -------------------------------------------------------------------------- */

public variant htable_insert(
    ASKL_HashTable *h,
    const char *k,
    size_t l,
    variant v
);

/**
 * @ingroup hashtable
 * @fn variant htable_insert(ASKL_HashTable *h, const char *k,
 *                           size_t l, variant v)
 * @param h a pointer to a hash table
 * @param k pointer to the key bytes
 * @param l length of the key in bytes
 * @param v the value to store
 * @return the existing value associated with @p k if it already existed,
 *         or VARIANT_NULL if the key was newly inserted
 *
 * This function inserts the key/value pair into the hash table.
 * If no entry with this key exists, it is created and VARIANT_NULL is returned.
 * If the key already exists, the table is left unchanged and the existing
 * value is returned.
 */

/* -------------------------------------------------------------------------- */

public variant htable_set(
    ASKL_HashTable *h,
    const char *k,
    size_t l,
    variant v
);

/**
 * @ingroup hashtable
 * @fn variant htable_set(ASKL_HashTable *h, const char *k, size_t l, variant v)
 * @param h a pointer to a hash table
 * @param k pointer to the key bytes
 * @param l length of the key in bytes
 * @param v the new value to store
 * @return the previous value associated with @p k, or VARIANT_NULL if none
 *
 * This function inserts or replaces the value associated with @p k.
 * If no entry exists, a new entry is inserted and VARIANT_NULL is returned.
 * If an entry already exists, its value is replaced and the previous value is
 * returned.
 */

/* -------------------------------------------------------------------------- */

public variant htable_get_with(
    ASKL_HashTable *h,
    const char *key,
    size_t len,
    variant (*function)(variant)
);

/**
 * @ingroup hashtable
 * @fn variant htable_get_with(ASKL_HashTable *h, const char *key, size_t len,
 *                             variant (*function)(variant))
 * @param h        a pointer to a hash table
 * @param key      pointer to the key bytes
 * @param len      length of the key in bytes
 * @param function optional callback applied to the stored value
 * @return the stored value, the result of @p function, or VARIANT_NULL
 *
 * Semantics are identical to @ref map_get_with().
 */

/* -------------------------------------------------------------------------- */

public variant htable_get(ASKL_HashTable *h, const char *key, size_t len);

/**
 * @ingroup hashtable
 * @fn variant htable_get(ASKL_HashTable *h,
 *                        const char *key, size_t len)
 * @param h   a pointer to a hash table
 * @param key pointer to the key bytes
 * @param len length of the key in bytes
 * @return the stored value, or VARIANT_NULL if the key is not present
 *
 * Convenience wrapper around @ref htable_get_with() with a NULL callback.
 */

/* -------------------------------------------------------------------------- */

public void htable_foreach(
    ASKL_HashTable *h,
    int (*function)(const char *, size_t, variant)
);

/**
 * @ingroup hashtable
 * @fn void htable_foreach(ASKL_HashTable *h,
 *                         int (*function)(const char *, size_t, variant))
 * @param h        a pointer to a hash table
 * @param function a callback invoked once per key/value pair
 * @return void
 *
 * This function iterates over all entries in all segments of @p h and invokes
 * @p function for each key/value pair. The semantics of deletion (when
 * @p function returns -1) are the same as for @ref map_foreach().
 */

/* -------------------------------------------------------------------------- */

public variant htable_remove(ASKL_HashTable *h, const char *key, size_t len);

/**
 * @ingroup hashtable
 * @fn variant htable_remove(ASKL_HashTable *h, const char *key, size_t len)
 * @param h   a pointer to a hash table
 * @param key pointer to the key bytes
 * @param len length of the key in bytes
 * @return the removed value, or VARIANT_NULL if the key was not present
 *
 * This function removes the entry associated with (@p key, @p len) from the
 * hash table and returns its value. If the key does not exist,
 * VARIANT_NULL is returned.
 *
 * @note The @p freeval callback is not invoked for the removed value. The
 *       caller becomes responsible for its cleanup.
 */

/* -------------------------------------------------------------------------- */

public size_t htable_footprint(ASKL_HashTable *h, size_t *overhead);

/**
 * @ingroup hashtable
 * @fn size_t htable_footprint(ASKL_HashTable *h, size_t *overhead)
 * @param h        a pointer to a hash table
 * @param overhead optional pointer to receive the internal overhead, in bytes
 * @return the total memory footprint of the hash table, in bytes
 *
 * This function computes an approximate memory footprint for the entire hash
 * table, including the top-level structure and all underlying hashmap segments.
 *
 * If @p overhead is non-NULL, @c *overhead is set to the sum of the overhead
 * reported by each segment via @ref map_footprint().
 */

/* -------------------------------------------------------------------------- */

public ASKL_HashTable *htable_free(ASKL_HashTable *h);

/**
 * @ingroup hashtable
 * @fn ASKL_HashTable *htable_free(ASKL_HashTable *h)
 * @param h a pointer to a hash table
 * @return always NULL
 *
 * This function destroys the hash table @p h and all of its segments. If a
 * @p freeval callback was provided at allocation time, it is invoked once per
 * remaining value in each segment before freeing the corresponding entry.
 *
 * This function always returns NULL so it can be used to clear the pointer:
 * @code
 * table = htable_free(table);
 * @endcode
 */

/* -------------------------------------------------------------------------- */

/* _ENABLE_HASHTABLE */
#endif
