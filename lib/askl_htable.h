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

#ifndef ASKL_HASHTABLE_H

#define ASKL_HASHTABLE_H

#include "askl.h"
#include "askl_rwlock.h"
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
 */

typedef struct ASKL_MapIterator {
    ASKL_LinkedMap *map;
    struct _bucket *_current;
    const char *key;
    size_t len;
    variant val;
} ASKL_MapIterator;

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

public variant map_set_with(
    ASKL_LinkedMap *h,
    const char *k,
    size_t l,
    variant v,
    variant (*function)(const char *k, size_t l, variant old, variant new)
);

/**
 * @ingroup hashtable
 * @fn variant map_set_with(ASKL_LinkedMap *h, const char *key, size_t len,
 *                          variant value,
 *                          variant (*function)(const char *, size_t,
 *                                              variant, variant))
 * @param h        a pointer to a linked hashmap
 * @param key      pointer to the key bytes
 * @param len      length of the key in bytes
 * @param value    the new value (also available to @p function as @p new)
 * @param function optional callback used to resolve replacement
 * @return the previous value associated with @p key, or VARIANT_NULL if the
 *         key was newly inserted
 *
 * This function sets @p key to @p value, inserting a new entry if needed.
 *
 * If @p function is NULL, the stored value is replaced unconditionally and
 * the previous value is returned (or VARIANT_NULL if the key was not present).
 *
 * If @p function is non-NULL and the key already exists, it is invoked with
 * the current value (@p old) and the requested value (@p new).
 *
 * If @p function returns:
 * - @p old: the map is left unchanged and map_set_with returns @p new.
 * - @p new: the @p new value is stored in the map and map_set_with returns
 *           the previous value (@p old).
 * - any other value: the previous value (@p old) is discarded (and the
 *                    _freeval callback invoked if defined), the returned value
 *                    is stored in the map, and map_set_with returns @p new
 *                    for the caller to dispose of.
 *
 * @note The callback @p function is executed while the map's write lock is
 *       held, ensuring atomicity. The callback must be fast and non-blocking.
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

public variant map_insert_with(
    ASKL_LinkedMap *h,
    const char *key,
    size_t len,
    variant value,
    variant (*function)(const char *k, size_t l, variant new)
);

/**
 * @ingroup hashtable
 * @fn variant map_insert_with(ASKL_LinkedMap *h, const char *key, size_t len,
 *                             variant value,
 *                             variant (*function)(const char *, size_t,
 *                                                 variant))
 * @param h        a pointer to a linked hashmap
 * @param key      pointer to the key bytes
 * @param len      length of the key in bytes
 * @param value    value passed to @p function as @p new
 * @param function optional callback to compute or initialize the inserted value
 * @return the existing value associated with @p key if it already existed,
 *         or VARIANT_NULL if the key was newly inserted
 *
 * This function performs an insert-only operation with an optional callback.
 *
 * If an entry with @p key already exists, the map is left unchanged and the
 * existing value is returned.
 *
 * If the key does not exist, a new entry is inserted. If @p function is NULL,
 * @p value is stored directly. If @p function is non-NULL, it is invoked with
 * @p value as parameter and its return value is stored instead.
 *
 * @note The callback @p function is executed only when the key is newly
 *       inserted. It is executed while the map's write lock is held, ensuring
 *       atomicity. The callback must be fast and non-blocking.
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

public variant map_update_with(
    ASKL_LinkedMap *h,
    const char *k,
    size_t l,
    variant v,
    variant (*function)(const char *k, size_t l, variant old, variant new)
);

/**
 * @ingroup hashtable
 * @fn variant map_update_with(ASKL_LinkedMap *h, const char *key, size_t len,
 *                             variant value,
 *                             variant (*function)(const char *key, size_t len,
 *                                                  variant old, variant new))
 * @param h        a pointer to a linked hashmap
 * @param key      pointer to the key bytes (not necessarily NUL-terminated)
 * @param len      length of the key in bytes
 * @param value    the proposed new value (also passed to @p function as @p new)
 * @param function optional callback used to compute the replacement value
 * @return the previous value associated with @p key if it existed, or
 *         @p value if the key was not present and no update was performed
 *
 * This function performs an update-only operation with an optional callback.
 * It never inserts new keys into the map.
 *
 * If the key @p key does not exist in @p h, the map is left unchanged and
 * @p value is returned to be disposed of.
 *
 * If the key exists and @p function is NULL, the stored value is replaced
 * unconditionally with @p value and the previous value is returned.
 *
 * If the key exists and @p function is non-NULL, the callback is invoked with
 * the current value (@p old) and the proposed value (@p new). Its return
 * value determines what is stored in the map and what is returned:
 *
 * - If @p function returns @p old: the map is left unchanged and
 *   map_update_with() returns @p new.
 * - If @p function returns @p new: the returned value is stored in the map
 *   and map_update_with() returns @p old.
 * - If @p function returns any other value: the previous value (@p old) is
 *   discarded (and the map's @c _freeval callback is invoked if defined),
 *   the returned value is stored in the map, and map_update_with() returns
 *   @p new so that the caller may dispose of it if necessary.
 *
 * @note The callback @p function is executed only when the key already exists.
 *       It is executed while the map's write lock is held, ensuring atomicity.
 *       The callback must be fast and non-blocking.
 *
 * @see map_set_with()
 * @see map_insert_with()
 * @see map_update()
 */

/* -------------------------------------------------------------------------- */

public variant map_update(ASKL_LinkedMap *h, const char *k, size_t l, variant v);

/**
 * @ingroup hashtable
 * @fn variant map_update(ASKL_LinkedMap *h, const char *key, size_t len,
 *                        variant value)
 * @param h     a pointer to a linked hashmap
 * @param key   pointer to the key bytes (not necessarily NUL-terminated)
 * @param len   length of the key in bytes
 * @param value the new value to store
 * @return the previous value associated with @p key if it existed, or
 *         @p value if the key was not present and no update was performed
 *
 * This function performs a simple update-only operation. If an entry with
 * @p key exists, its value is replaced with @p value and the previous value
 * is returned.
 *
 * If the key does not exist in the map, the map is left unchanged and
 * @p value is returned to be disposed of. No new entry is created.
 *
 * This is the update-only counterpart to @ref map_insert(), and is useful
 * when the caller wants to modify an entry only if it already exists, and
 * do nothing otherwise.
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

public int map_has(ASKL_LinkedMap *h, const char *key, size_t len);

/**
 * @ingroup hashtable
 * @fn int map_has(ASKL_LinkedMap *h, const char *key, size_t len)
 * @param h   a pointer to a linked hashmap
 * @param key pointer to the key bytes
 * @param len length of the key in bytes
 * @return non-zero if @p key exists in the map, or 0 otherwise
 *
 * This function checks whether an entry with the given key exists in the map.
 *
 */

/* -------------------------------------------------------------------------- */

public int map_merge(
    ASKL_LinkedMap *dest,
    ASKL_LinkedMap *src,
    variant merge(const char *key, size_t len, variant destval, variant srcval)
);

/**
 * @ingroup hashtable
 * @fn int map_merge(ASKL_LinkedMap *dest, ASKL_LinkedMap *src,
 *                   variant (*merge)(const char *, size_t, variant, variant))
 * @param dest  destination hashmap
 * @param src   source hashmap (consumed and destroyed)
 * @param merge conflict resolution callback
 * @return 0 on success, or -1 on error
 *
 * The function transfers all entries from @p src into @p dest and resolves
 * conflicts using the user-supplied @p merge callback.
 *
 * When a key exists in both maps, the callback is invoked with the key and
 * the values from @p dest and @p src. The callback's return value replaces
 * the value stored in @p dest for that key.
 *
 * If the callback returns the original value from @p dest, the value from
 * @p src is discarded. If it returns the original value from @p src, the
 * value from @p dest is discarded. If it returns a different value, both
 * original values are discarded.
 *
 * Discarded values are released using the owning map's @p _freeval callback,
 * if defined. If no @p _freeval callback is configured for the map owning a
 * discarded value, the @p merge callback must release that value itself to
 * avoid leaks.
 *
 * After a successful call, @p src is destroyed. The @p src pointer becomes
 * invalid and must not be accessed again.
 *
 * @note The @p merge callback is executed while both maps are write-locked.
 *       It must be fast and must not attempt to access either map or perform
 *       blocking operations.
 *
 * @warning This function consumes and destroys @p src.
 *          The caller must ensure that no other threads access @p src
 *          concurrently with this call, or after it returns.
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

public variant map_remove_if(
    ASKL_LinkedMap *h,
    const char *key,
    size_t len,
    int (*condition)(const char *key, size_t len, variant val)
);

/**
 * @ingroup hashtable
 * @fn variant map_remove_if(ASKL_LinkedMap *h, const char *key, size_t len,
 *                           int (*condition)(const char *, size_t, variant))
 * @param h         a pointer to a linked hashmap
 * @param key       pointer to the key bytes
 * @param len       length of the key in bytes
 * @param condition optional predicate controlling removal
 * @return the removed value if the entry was removed, or VARIANT_NULL if the
 *         key was not present or was not removed
 *
 * This function removes the entry associated with @p key from @p h.
 *
 * If @p condition is NULL, the entry is removed unconditionally.
 *
 * If @p condition is non-NULL, it is invoked with the stored value. The entry
 * is removed only if the callback returns non-zero.
 *
 * @note The callback @p condition is executed while the map's write lock is
 *       held, ensuring atomicity. The callback must be fast and non-blocking.
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

public ASKL_MapIterator *map_each(ASKL_LinkedMap *h);

/**
 * @ingroup hashtable
 * @fn ASKL_MapIterator *map_each(ASKL_LinkedMap *h)
 * @param h   a pointer to a linked hashmap
 * @return a newly allocated iterator positioned on the first entry, or @c NULL
 *         if the map is empty or an error occurred
 *
 * This function creates an iterator that allows the caller to traverse all
 * entries of the hashmap in key order as stored in the internal index list.
 *
 * The returned iterator holds a read lock on @p h for the duration of the
 * iteration. The iterator must be advanced using @ref map_next and eventually
 * destroyed using @ref map_break (or implicitly when @ref map_next reaches
 * the end).
 *
 * @note The iterator acquires a read lock on @p h when created. This lock
 *       is automatically released when the iterator is exhausted or explicitly
 *       destroyed with @ref map_break.
 */

/* -------------------------------------------------------------------------- */

public ASKL_MapIterator * CALLBACK map_next(ASKL_MapIterator *iterator);

/**
 * @ingroup hashtable
 * @fn ASKL_MapIterator *map_next(ASKL_MapIterator *iterator)
 * @param iterator  an iterator previously created with @ref map_each
 * @return the same iterator positioned on the next entry, or @c NULL if the
 *         end of the traversal is reached or an error occurred
 *
 * This function advances the iterator to the next entry in the map. If another
 * entry is found, the iterator's @c key, @c len and @c val fields are updated
 * accordingly and @p iterator is returned.
 *
 * When there are no more entries, the iterator is automatically destroyed,
 * its read lock on the map is released, and @c NULL is returned.
 *
 * @note The caller must not free the iterator returned by @ref map_next; it is
 *       freed automatically when the iteration ends. To stop early, call
 *       @ref map_break instead.
 */

/* -------------------------------------------------------------------------- */

public variant map_set_at(ASKL_MapIterator *iterator, variant new);

/**
 * @ingroup hashtable
 * @fn variant map_set_at(ASKL_MapIterator *iterator, variant value)
 * @param iterator  a valid iterator positioned on an existing entry
 * @param value     the new value to store at the current position
 * @return the previous value stored at the iterator’s current key
 *
 * This function replaces the value associated with the entry currently pointed
 * to by @p iterator. The key is left unchanged; only the value is updated.
 *
 * Internally, the implementation upgrades the iterator's read lock to a write
 * lock for the duration of the update, then restores it back to a read lock.
 * This ensures that the update is atomic with respect to other concurrent
 * map operations and that the iterator remains valid after the call.
 *
 * @warning The iterator must currently point to a valid entry (i.e. it must
 *          be the result of a successful call to @ref map_each or
 *          @ref map_next). Calling this function on an exhausted or broken
 *          iterator results in undefined behaviour.
 */

/* -------------------------------------------------------------------------- */

public variant map_remove_at(ASKL_MapIterator *iterator);

/**
 * @ingroup hashtable
 * @fn variant map_remove_at(ASKL_MapIterator *iterator)
 * @param iterator  a valid iterator positioned on an existing entry
 * @return the value that was stored at the iterator’s current key
 *
 * This function removes the entry currently pointed to by @p iterator from
 * the map and returns its value. The key is removed from the map; subsequent
 * lookups for that key will fail as if it had never been inserted.
 *
 * Internally, the implementation upgrades the iterator's read lock to a write
 * lock for the duration of the update, then restores it back to a read lock.
 * This ensures that the update is atomic with respect to other concurrent
 * map operations and that the iterator remains valid after the call.
 *
 * After @ref map_remove_at() returns, @p iterator remains valid but its
 * current position should be considered implementation-defined. The only
 * valid operations on the iterator are to continue the traversal with
 * @ref map_next() or to stop it with @ref map_break(). The caller must not
 * attempt to reuse the previous @c key/@c len/@c val fields after the entry
 * has been removed.
 *
 * The caller is responsible for disposing of the returned value if needed.
 * In particular, if the map was configured with a @c _freeval callback, that
 * callback is **not** invoked automatically by @ref map_remove_at(); it is
 * up to the caller to free or recycle the removed value as appropriate.
 *
 * @warning The iterator must currently point to a valid entry (i.e. it must
 *          be the result of a successful call to @ref map_each or
 *          @ref map_next). Calling this function on an exhausted or broken
 *          iterator results in undefined behaviour.
 *
 * @see map_each()
 * @see map_next()
 * @see map_break()
 */

/* -------------------------------------------------------------------------- */

public ASKL_MapIterator *map_break(ASKL_MapIterator *iterator);

/**
 * @ingroup hashtable
 * @fn ASKL_MapIterator *map_break(ASKL_MapIterator *iterator)
 * @param iterator  an iterator obtained from @ref map_each
 * @return always @c NULL
 *
 * This function explicitly destroys @p iterator and releases its read lock on
 * the underlying map. After this call, @p iterator must not be used again.
 *
 * This is the manual counterpart to the implicit destruction performed by
 * @ref map_next when the end of the traversal is reached.
 *
 * @note This function always returns @c NULL so that callers can conveniently
 *       clear their iterator variables:
 * @code
 * it = map_break(it);
 * @endcode
 */

/* _ENABLE_HASHTABLE */
#endif
