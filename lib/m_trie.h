/*******************************************************************************
 *  Concrete Server                                                            *
 *  Copyright (c) 2005-2025 Raphael Prevost <raph@el.bzh>                      *
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

#ifndef M_TRIE_H

#define M_TRIE_H

#ifdef _ENABLE_TRIE

#include "m_core_def.h"
#include "m_variant.h"

/** @defgroup trie module::trie */

typedef struct m_trie {
    pthread_rwlock_t *_lock;
    void *_root;
    void (*_freeval)(variant);
} m_trie;

/**
 * @ingroup trie
 * @struct m_trie
 *
 * @b private @ref _lock a read/write lock to ensure safe concurrency
 * @b private @ref _root a pointer to the root node of the trie
 *
 */

typedef struct m_trie_iterator {
    struct m_trie *trie;
    uint8_t **_node;
    size_t _node_alloc;
    size_t _node_count;
    const char *key;
    size_t len;
    variant val;
} m_trie_iterator;

/* -------------------------------------------------------------------------- */

public m_trie *trie_alloc(void (*freeval)(variant));

/**
 * @ingroup trie
 * @fn m_trie *trie_alloc(void (*freeval)(variant *))
 * @param freeval optional pointer to a cleanup function
 *
 * This function allocates a new crit-bit trie. If the @b freeval callback is
 * not NULL, it will be called by @ref trie_foreach() if an element should be
 * removed from the trie.
 *
 * The trie should be destroyed with @ref trie_free() after use.
 *
 */

/* -------------------------------------------------------------------------- */

public int trie_insert(m_trie *t, const char *key, size_t ulen, variant value);

/**
 * @ingroup trie
 * @fn trie_insert(m_trie *t, const char *key, size_t ulen, variant value)
 * @param t      a pointer to the trie
 * @param key    the key to associate with the stored value
 * @param ulen   the length of the key
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
 * @see trie_remove
 *
 */

/* -------------------------------------------------------------------------- */

public variant trie_lookup(m_trie *t, const char *key, size_t ulen,
                           variant (CALLBACK *f)(variant));

/**
 * @ingroup trie
 * @fn trie_lookup(m_trie *t, const char *key, size_t ulen,
 *                 void *(CALLBACK *f)(variant))
 * @param t     a pointer to the trie
 * @param key   the key used to retrieve the stored value
 * @param ulen  the length of the key
 * @param f     an optional callback that processes the retrieved value
 *
 * @return a variant containing the stored value, the return value of @b f,
 *         or a variant of type VARIANT_NULL if the key is not found
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

public variant trie_remove(m_trie *t, const char *key, size_t ulen);

/**
 * @ingroup trie
 * @fn trie_remove(m_trie *t, const char *key, size_t ulen)
 * @param t     a pointer to the trie
 * @param key   the key used to retrieve the stored value
 * @param ulen  the length of the key
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

public variant trie_update(m_trie *t, const char *key, size_t ulen, variant v);

/**
 * @ingroup trie
 * @fn trie_update(m_trie *t, const char *key, size_t ulen, variant v)
 *
 * @param t     a pointer to the trie
 * @param key   the key whose associated value should be updated
 * @param ulen  the length of the key
 * @param v     the new value to associate with the key
 *
 * @return the previous value associated with the key, or a VARIANT_NULL
 *         value if the key did not previously exist
 *
 * This function stores the value @p v under the specified @p key. If the key
 * already exists, its associated value is replaced and the previous value is
 * returned. If the key does not exist, it is inserted into the trie and
 * VARIANT_NULL is returned.
 *
 * @note If the trie was created with a @b freeval callback, that callback is
 *       not invoked for the value being replaced. The caller becomes responsible
 *       for performing any necessary cleanup on the returned value.
 */

/* -------------------------------------------------------------------------- */

public void trie_foreach(m_trie *t, int (*f)(const char *, size_t, variant));

/**
 * @ingroup trie
 * @fn trie_foreach(m_trie *t, int (*f)(const char *, size_t, variant))
 *
 * @param t   a pointer to the trie
 * @param f   a callback invoked once per leaf, receiving the key, its length,
 *            and the associated value
 *
 * This function performs a full traversal of the trie and invokes the callback
 * @p f for every key/value pair stored in it. The walk is performed in
 * depth-first order and visits every leaf in the structure.
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

public m_trie_iterator *trie_each(m_trie *t);

/**
 * @ingroup trie
 * @fn trie_each(m_trie *t)
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

public m_trie_iterator *trie_each_prefix(m_trie *t, const char *pf, size_t len);

/**
 * @ingroup trie
 * @fn trie_each_prefix(m_trie *t, const char *pf, size_t len)
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

public m_trie_iterator * CALLBACK trie_next(m_trie_iterator *iterator);

/**
 * @ingroup trie
 * @fn trie_next(m_trie_iterator *iterator)
 *
 * @param iterator  an iterator previously created with @ref trie_each or
 *                  @ref trie_each_prefix
 *
 * @return the same iterator positioned on the next leaf, or @c NULL if the end
 *         of the traversal is reached or an error occurred
 *
 * This function advances the iterator to the next leaf in depth-first order.
 * If another leaf is found, the iterator's @c key, @c len, and @c val fields
 * are updated accordingly. If there are no more leaves, the iterator is
 * automatically destroyed, its read lock is released, and @c NULL is returned.
 *
 * @note The caller must not free the iterator returned by @ref trie_next; it is
 *       freed automatically when the iteration ends. To stop early, call
 *       @ref trie_break instead.
 */

/* -------------------------------------------------------------------------- */

public m_trie_iterator *trie_break(m_trie_iterator *iterator);

/**
 * @ingroup trie
 * @fn trie_break(m_trie_iterator *iterator)
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

public m_trie *trie_free(m_trie *t);

/* -------------------------------------------------------------------------- */

/* _ENABLE_TRIE */
#endif

#endif
