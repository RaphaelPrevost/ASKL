/*******************************************************************************
 *  Concrete Server                                                            *
 *  Copyright (c) 2005-2024 Raphael Prevost <raph@el.bzh>                      *
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

/* -------------------------------------------------------------------------- */

public void trie_foreach_prefix(m_trie *t, const char *prefix, size_t ulen,
                                int (*function)(const char *, size_t, variant));

/* -------------------------------------------------------------------------- */

public m_trie *trie_free(m_trie *t);

/* -------------------------------------------------------------------------- */

/* _ENABLE_TRIE */
#endif

#endif
