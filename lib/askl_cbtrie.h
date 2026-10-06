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

#ifndef ASKL_TRIE_H

#define ASKL_TRIE_H

#ifdef _ENABLE_TRIE

#include "askl.h"
#include "askl_rwlock.h"
#include "askl_variant.h"

/** @defgroup trie ASKL::trie */

typedef struct _Trie Trie;

typedef struct Trie_Leaf {
    uint16_t len;
    uint16_t own;
    Variant val;
    char key[];
} Trie_Leaf;

/**
 * @ingroup trie
 * @struct Trie_Leaf
 *
 * A trie leaf containing a complete key and its associated value.
 *
 * Leaves returned by lookup functions belong to the trie and remain valid
 * only until the trie is modified.
 */

/* the value is a string stored in the leaf after its key, with its length
   in the dword field of the variant: the trie never frees it on its own */
#define TRIE_LEAF_INLINE 0x1

typedef struct Trie_Cursor {
    void *anchor;
    void *_path;
    size_t _depth;
    size_t _alloc;
    char *_key;
    size_t _len;
    size_t _size;
} Trie_Cursor;

/**
 * @ingroup trie
 * @struct Trie_Cursor
 *
 * State reused by successive prefix lookups.
 *
 * A zero-initialized cursor allows @ref trie_lookup_prefix_from() to resume
 * below trie branches shared with the preceding lookup. @ref anchor is the
 * narrowest subtree reached while consuming the most recent prefix and may be
 * passed to @ref trie_anchor() or @ref trie_children_from().
 *
 * Cursor state, including @ref anchor, is invalidated by any trie modification.
 * Release its allocated state with @ref trie_cursor_free().
 */

typedef struct Trie_Iterator {
    const char *key;
    size_t len;
    Variant val;
    const char *child;
    uint16_t child_len;
    uint16_t divergence;
} Trie_Iterator;

/**
 * @ingroup trie
 * @struct Trie_Iterator
 *
 * State exposed by trie traversal functions.
 *
 * @ref key, @ref len and @ref val describe the current leaf. Child iteration
 * additionally sets @ref child and @ref child_len to the immediate child
 * represented by that leaf.
 *
 * @ref divergence is the earliest byte at which the current key may differ
 * from the previously returned key. Bytes before that offset are known to be
 * equal. A value of 0 means that no useful common-prefix bound is available.
 */

/* -------------------------------------------------------------------------- */

ASKL_API Trie *trie_alloc(void (*freeval)(Variant));

/**
 * @ingroup trie
 * @fn Trie *trie_alloc(void (*freeval)(Variant))
 *
 * @param freeval  optional pointer to a cleanup function taking a @ref Variant
 *
 * @return a pointer to a newly allocated empty trie, or @c NULL if an error
 *         occurs
 *
 * This function allocates a new crit-bit trie. If the @p freeval callback is
 * not @c NULL, it will be called by @ref trie_foreach() and @ref trie_free()
 * whenever an element is deleted from the trie.
 *
 * The trie should be destroyed with @ref trie_free() after use.
 */

/* -------------------------------------------------------------------------- */

ASKL_API int trie_disable_lock(Trie *t);

/**
 * @ingroup trie
 * @fn int trie_disable_lock(Trie *t)
 * @param t the trie
 * @return 0 on success, -1 on error
 *
 * Permanently disable the trie's internal locking. The caller becomes
 * responsible for synchronizing all subsequent operations.
 *
 * The function first acquires the write lock so existing operations complete
 * before locking is disabled.
 *
 * @note Call this before publishing the trie to other threads. Locking cannot
 *       be re-enabled.
 */

/* -------------------------------------------------------------------------- */

ASKL_API int trie_insert_with(
    Trie *t,
    const char *key,
    size_t len,
    Variant value,
    Variant (*function)(const char *key, size_t len, Variant new)
);

/**
 * @ingroup trie
 * @fn int trie_insert_with(Trie *t, const char *key, size_t len,
 *                          Variant value,
 *                          Variant (*function)(const char *, size_t, Variant))
 * @param t        a pointer to the trie
 * @param key      the key to associate with the stored value
 * @param len      the length of the key
 * @param value    value passed to @p function as @p new
 * @param function optional callback to compute or initialize the inserted value
 * @return 0 on success, or -1 on error
 *
 * This function performs an insert-only operation.
 *
 * If no entry with the given key exists, the key is inserted. If @p function
 * is NULL, @p value is stored directly. If @p function is non-NULL, it is
 * invoked with @p value as parameter and its return value is stored instead.
 *
 * If an entry already exists, the trie is left unchanged and -1 is returned.
 *
 * @note The callback @p function is executed only when the key is newly
 *       inserted. It is executed while the trie's write lock is held, ensuring
 *       atomicity. The callback must be fast and non-blocking.
 */

/* -------------------------------------------------------------------------- */

ASKL_API int trie_insert(Trie *t, const char *key, size_t len, Variant value);

/**
 * @ingroup trie
 * @fn trie_insert(Trie *t, const char *key, size_t len, Variant value)
 * @param t      a pointer to the trie
 * @param key    the key to associate with the stored value
 * @param len    the length of the key
 * @param value  the value to store in the trie
 *
 * @return -1 if an error occurs, 0 otherwise
 *
 * This function inserts a given variant into the trie under the specified key.
 * If the key already exists, the function fails and returns -1. Otherwise, the
 * value is stored and can later be retrieved using the same key.
 *
 * If a @b freeval callback was specified when the trie was created, it will be
 * invoked to free the value when the entry is removed or when the trie is
 * destroyed.
 *
 * @note Key lengths are limited to @c UINT16_MAX - 1 bytes. Passing a longer
 *       key causes @ref trie_insert() to fail with @c -1.
 *
 * @see trie_remove
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API int trie_insert_prefix_list(
    Trie *t,
    size_t prefix_len,
    Trie_Leaf **list,
    size_t count
);

/**
 * @ingroup trie
 * @fn int trie_insert_prefix_list(Trie *t, size_t prefix_len,
 *                                 Trie_Leaf **list, size_t count)
 * @param t trie
 * @param prefix_len number of leading key bytes shared by the list
 * @param list leaves to insert
 * @param count number of leaves in @p list
 * @return number of leaves inserted, or -1 on error
 *
 * Insert a list of preallocated leaves that share at least @p prefix_len key
 * bytes.
 *
 * Duplicate leaves are discarded. Ownership of every leaf in @p list passes
 * to this function, whether or not that leaf is inserted.
 */

/* -------------------------------------------------------------------------- */

ASKL_API Variant trie_lookup(
    Trie *t,
    const char *key,
    size_t len,
    Variant (*f)(Variant)
);

/**
 * @ingroup trie
 * @fn trie_lookup(Trie *t, const char *key, size_t len,
 *                 Variant (*f)(Variant))
 * @param t     a pointer to the trie
 * @param key   the key used to retrieve the stored value
 * @param len   the length of the key
 * @param f     an optional callback that processes the retrieved value
 *
 * @return a Variant containing the stored value, the return value of @b f,
 *         or a Variant of type VARIANT_NULL if the key is not found
 *
 * This function searches the trie for the specified key and returns its
 * associated value. If the key does not exist, a VARIANT_NULL value is
 * returned.
 *
 * If a callback function @b f is provided, it is invoked with the retrieved
 * value as its argument, and the return value of @b f is returned instead of
 * the raw stored value.
 *
 * @note The callback @b f must not modify the stored value unless such
 * modifications are safe in the presence of concurrent access.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API int trie_has(Trie *t, const char *key, size_t len);

/**
 * @ingroup trie
 * @fn int trie_has(Trie *t, const char *key, size_t len)
 * @param t   a pointer to the trie
 * @param key the key used to retrieve the stored value
 * @param len the length of the key
 * @return non-zero if @p key exists in the trie, or 0 otherwise
 *
 * This function checks whether an entry with the given key exists in the trie.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API const Trie_Leaf *trie_lookup_prefix(
    Trie *t,
    const char *prefix,
    size_t len
);

/**
 * @ingroup trie
 * @fn const Trie_Leaf *trie_lookup_prefix(Trie *t, const char *prefix,
 *                                         size_t len)
 * @param t      a pointer to the trie
 * @param prefix the prefix of the keys of interest
 * @param len    the length of the prefix
 * @return the leaf holding the smallest key starting with @p prefix, or
 *         @c NULL if no key does
 *
 * This function finds the first key of the trie, in lexicographic order,
 * starting with the given prefix, the prefix itself first when it is a
 * key, in a single descent and without allocating anything, unlike
 * @ref trie_each_prefix(). The leaf belongs to the trie: its key and
 * value are valid until the trie is modified.
 *
 * @see trie_lookup_prefix_from()
 */

/* -------------------------------------------------------------------------- */

ASKL_API const Trie_Leaf *trie_lookup_prefix_from(
    Trie *t,
    Trie_Cursor *cursor,
    const char *prefix,
    size_t len
);

/**
 * @ingroup trie
 * @fn const Trie_Leaf *trie_lookup_prefix_from(Trie *t, Trie_Cursor *cursor,
 *                                              const char *prefix,
 *                                              size_t len)
 * @param t      a pointer to the trie
 * @param cursor a cursor, zeroed before its first use, or @c NULL
 * @param prefix the prefix of the keys of interest
 * @param len    the length of the prefix
 * @return the leaf holding the smallest key starting with @p prefix, or
 *         @c NULL if no key does
 *
 * Like @ref trie_lookup_prefix(), but reuses @p cursor from the preceding
 * lookup. When successive prefixes share leading bytes, traversal resumes
 * below the corresponding cached branches instead of starting at the root.
 *
 * Initialize the cursor to zero before first use and release it with
 * @ref trie_cursor_free(). Any trie modification invalidates its cached state.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Cursor *trie_cursor_free(Trie_Cursor *cursor);

/**
 * @ingroup trie
 * @fn Trie_Cursor *trie_cursor_free(Trie_Cursor *cursor)
 * @param cursor a cursor used with @ref trie_lookup_prefix_from()
 * @return @c NULL
 *
 * This function releases what the cursor holds and zeroes it, so that it
 * can be used again.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API Variant trie_remove_if(
    Trie *t,
    const char *key,
    size_t len,
    int (*condition)(const char *key, size_t len, Variant value)
);

/**
 * @ingroup trie
 * @fn Variant trie_remove_if(Trie *t, const char *key, size_t len,
 *                            int (*condition)(const char *, size_t, Variant))
 * @param t         a pointer to the trie
 * @param key       the key used to retrieve the stored value
 * @param len       the length of the key
 * @param condition optional predicate controlling removal
 * @return the removed value if the entry was removed, or VARIANT_NULL if the
 *         key was not present or was not removed
 *
 * This function removes the entry associated with @p key from @p t.
 *
 * If @p condition is NULL, the entry is removed unconditionally.
 * If @p condition is non-NULL, it is invoked with the stored value and the
 * entry is removed only if the callback returns non-zero.
 *
 * @note The callback @p condition is executed while the trie's write lock is
 *       held, ensuring atomicity. The callback must be fast and non-blocking.
 */

/* -------------------------------------------------------------------------- */

ASKL_API Variant trie_remove(Trie *t, const char *key, size_t len);

/**
 * @ingroup trie
 * @fn trie_remove(Trie *t, const char *key, size_t len)
 * @param t     a pointer to the trie
 * @param key   the key used to retrieve the stored value
 * @param len   the length of the key
 *
 * @return the value previously associated with the key, or a VARIANT_NULL
 *         value if the key does not exist
 *
 * This function looks up the specified key in the trie, removes it if present,
 * and returns the value that was associated with it. If the key is not found,
 * a VARIANT_NULL value is returned.
 *
 * @note If the trie was created with a @b freeval callback, that callback is
 *       not invoked by this function. The caller becomes responsible for
 *       managing the returned value.
 */

/* -------------------------------------------------------------------------- */

ASKL_API Variant trie_update(Trie *t, const char *key, size_t len, Variant v);

/**
 * @ingroup trie
 * @fn trie_update(Trie *t, const char *key, size_t len, Variant v)
 *
 * @param t     a pointer to the trie
 * @param key   the key whose associated value should be updated
 * @param len   the length of the key
 * @param v     the new value to associate with the key
 *
 * @return the previous value associated with the key, or a VARIANT_NULL
 *         value if the key did not previously exist
 *
 * This function replaces the value associated with @p key by @p v and
 * returns the previous value. If the key does not exist, the trie is left
 * unchanged and VARIANT_NULL is returned.
 *
 * @note If the trie was created with a @b freeval callback, that callback is
 *       not invoked for the value being replaced. The caller becomes responsible
 *       for performing any necessary cleanup on the returned value.
 */

/* -------------------------------------------------------------------------- */

ASKL_API void trie_foreach(Trie *t, int (*f)(const char *, size_t, Variant));

/**
 * @ingroup trie
 * @fn trie_foreach(Trie *t, int (*f)(const char *, size_t, Variant))
 *
 * @param t   a pointer to the trie
 * @param f   a callback invoked once per leaf, receiving the key, its length,
 *            and the associated value
 *
 * This function performs a full traversal of the trie and invokes the callback
 * @p f for every key/value pair stored in it. The walk is performed in
 * lexicographic order and visits every leaf in the structure.
 *
 * If @p f returns @c -1, the corresponding key/value pair is removed from the
 * trie. If the trie was created with a @b freeval callback, that callback is
 * invoked on the value.
 *
 * Any other non-negative return value from @p f is ignored and the traversal
 * continues normally.
 *
 * @note This function acquires a @b write lock on the trie for the entire
 *       duration of the traversal and potential deletions.
 */

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_each(Trie *t);

/**
 * @ingroup trie
 * @fn trie_each(Trie *t)
 *
 * @param t   a pointer to the trie
 *
 * @return a newly allocated iterator positioned on the first leaf, or @c NULL
 *         if the trie is empty or an error occurred
 *
 * This function creates an iterator that allows the caller to traverse all
 * leaves of the trie. The returned iterator holds a read lock on the trie.
 *
 * The iterator must be advanced using @ref trie_next and eventually destroyed
 * using @ref trie_break (or implicitly when @ref trie_next reaches the end).
 *
 * @note The iterator acquires a read lock on the trie when created. This lock
 *       is automatically released when the iterator is exhausted or explicitly
 *       destroyed with @ref trie_break.
 */

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_each_prefix(Trie *t, const char *pf, size_t len);

/**
 * @ingroup trie
 * @fn trie_each_prefix(Trie *t, const char *pf, size_t len)
 *
 * @param t    a pointer to the trie
 * @param pf   a key prefix to restrict the traversal
 * @param len  the length of the prefix
 *
 * @return an iterator positioned at the first leaf whose key begins with the
 *         given prefix, or @c NULL if no such prefix exists or an error
 *         occurred
 *
 * This function behaves like @ref trie_each, but the traversal is limited to
 * keys that share the specified prefix @p pf.
 *
 * If the prefix does not correspond to any key, the function returns @c NULL
 * and no iterator is created.
 */

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_seek(Trie *t, const char *key, size_t len);

/**
 * @ingroup trie
 * @fn Trie_Iterator *trie_seek(Trie *t, const char *key, size_t len)
 *
 * @param t   a pointer to the trie
 * @param key the key to seek, which need not be in the trie
 * @param len the length of the key
 *
 * @return an iterator positioned on the first entry whose key is greater
 *         than or equal to @p key in lexicographic order, or @c NULL if
 *         there is none or an error occurs
 *
 * This function starts an ordered traversal at the first key that is not
 * lower than @p key: @ref trie_next() then yields the following keys in
 * lexicographic order, as it does after @ref trie_each(). An empty key
 * seeks the first entry. The iterator holds a read lock on the trie until
 * the traversal ends or @ref trie_break() is called.
 *
 * Seeking is how a range is walked without visiting what precedes it, and
 * how the children of a prefix are enumerated without visiting their
 * descendants: seek past the last byte of a child to reach the next one.
 *
 * @see trie_each(), trie_each_prefix(), trie_next(), trie_break()
 */

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_children(
    Trie *t,
    const char *prefix,
    size_t len,
    char separator
);

/**
 * @ingroup trie
 * @fn Trie_Iterator *trie_children(Trie *t, const char *prefix, size_t len,
 *                                  char separator)
 *
 * @param t         a pointer to the trie
 * @param prefix    the keys of interest start with this prefix
 * @param len       the length of the prefix
 * @param separator the byte that ends a child's name within a key
 *
 * @return an iterator positioned on the first child, or @c NULL if the
 *         prefix has no key or an error occurs
 *
 * This function enumerates the children of a prefix, where a child is the
 * part of a key that follows the prefix up to the next @p separator or the
 * end of the key: for the keys "/a/b", "/a/b/c" and "/a/d" the children of
 * "/a/" are "b" and "d". The trie itself knows nothing of separators, the
 * caller chooses one, so that a JSON Pointer index is walked with '/'.
 *
 * The iterator exposes each child in @ref Trie_Iterator::child and
 * @ref Trie_Iterator::child_len, while @ref Trie_Iterator::key holds the
 * first key of that child in traversal order: the child's own key when it
 * has one, in which case @ref Trie_Iterator::val is its value, or the key
 * of its first descendant. @ref trie_next_child() moves to the next child
 * without visiting the descendants of the current one, whatever their
 * number. The read lock is held until the enumeration ends or
 * @ref trie_break() is called.
 *
 * @see trie_next_child(), trie_seek(), trie_break()
 */

/* -------------------------------------------------------------------------- */

ASKL_API void *trie_anchor(
    Trie *t,
    void *from,
    const char *prefix,
    size_t len
);

/**
 * @ingroup trie
 * @fn void *trie_anchor(Trie *t, void *from, const char *prefix, size_t len)
 * @param t the trie
 * @param from the anchor of a shorter prefix, or NULL for the whole trie
 * @param prefix the prefix
 * @param len its length
 * @return the anchor of the prefix, NULL on error or if the trie is empty
 *
 * Return the narrowest subtree that can contain keys beginning with @p prefix.
 * If @p from is non-NULL, traversal starts from that previously obtained
 * anchor instead of the trie root.
 *
 * The returned pointer is an opaque traversal token. It may contain keys that
 * do not match @p prefix, but every matching key lies within that subtree.
 *
 * @note An anchor remains valid only until the trie is modified.
 */

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_children_from(
    Trie *t,
    void *from,
    const char *prefix,
    size_t len,
    char separator
);

/**
 * @ingroup trie
 * @fn Trie_Iterator *trie_children_from(Trie *t, void *from,
 *                                      const char *prefix, size_t len,
 *                                      char separator)
 * @param t the trie
 * @param from the anchor of the prefix or of a shorter one, see
 *             @ref trie_anchor(), NULL for the whole trie
 * @param prefix the prefix
 * @param len its length
 * @param separator the byte separating the components of the keys
 * @return an iterator on the first child, or NULL
 *
 * Like @ref trie_children(), but bounds traversal and child-to-child seeks to
 * @p from. Passing an anchor for the current prefix or one of its ancestors
 * avoids restarting those operations from the trie root.
 */

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_next_child(Trie_Iterator *iterator);

/**
 * @ingroup trie
 * @fn Trie_Iterator *trie_next_child(Trie_Iterator *iterator)
 *
 * @param iterator @param iterator an iterator returned by @ref trie_children()
 *                                 or @ref trie_children_from()
 *
 * @return the iterator positioned on the next child, or @c NULL when the
 *         children are exhausted, in which case the iterator is released
 *
 * Advance to the next immediate child of the prefix used to create the
 * iterator. Descendants of the current child are skipped.
 *
 * Returning NULL releases the iterator and its read lock.
 *
 * @see trie_children()
 */

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_next(Trie_Iterator *iterator);

/**
 * @ingroup trie
 * @fn trie_next(Trie_Iterator *iterator)
 *
 * @param iterator  an iterator previously created with @ref trie_each or
 *                  @ref trie_each_prefix
 *
 * @return the same iterator positioned on the next leaf, or @c NULL if the end
 *         of the traversal is reached or an error occurred
 *
 * This function advances the iterator to the next leaf in lexicographic order.
 * If another leaf is found, the iterator's @c key, @c len, and @c val fields
 * are updated accordingly. If there are no more leaves, the iterator is
 * automatically destroyed, its read lock is released, and @c NULL is returned.
 *
 * @note The caller must not free the iterator returned by @ref trie_next; it is
 *       freed automatically when the iteration ends. To stop early, call
 *       @ref trie_break instead.
 */

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_break(Trie_Iterator *iterator);

/**
 * @ingroup trie
 * @fn trie_break(Trie_Iterator *iterator)
 *
 * @param iterator  an iterator previously created with @ref trie_each or
 *                  @ref trie_each_prefix
 *
 * @return always @c NULL
 *
 * This function immediately terminates an iterator-based traversal. It releases
 * the read lock held by the iterator, frees its internal traversal stack, and
 * deallocates the iterator itself.
 *
 * The usual way to end a traversal is simply to let @ref trie_next reach the
 * end of the trie. @ref trie_break is used when the caller wants to stop early,
 * such as after finding a desired key.
 *
 * @note After calling this function, the @p iterator pointer must not be used.
 */

/* -------------------------------------------------------------------------- */

ASKL_API Trie *trie_free(Trie *t);

/**
 * @ingroup trie
 * @fn Trie *trie_free(Trie *t)
 *
 * @param t  a pointer to a trie
 *
 * @return always @c NULL
 *
 * This function destroys the trie and all of its contents.
 *
 * If a @p freeval callback was specified when the trie was created with
 * @ref trie_alloc(), it is invoked once for each stored value before the
 * corresponding leaf is freed.
 *
 * After calling this function, the @p t pointer must not be used. The function
 * always returns @c NULL so it can be used to clear the pointer:
 * @code
 * trie = trie_free(trie);
 * @endcode
 */

/* -------------------------------------------------------------------------- */

/* _ENABLE_TRIE */
#endif

#endif
