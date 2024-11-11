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
 * @fn trie_insert(m_trie *t, const char *key, size_t ulen, variant *value)
 * @param t a pointer to a trie
 * @param key the name which will be used to retrieve the stored data
 * @param ulen the length of the key
 * @param value the data that will be stored in the trie
 * @return -1 if an error occurs, 0 otherwise
 *
 * This function stores in a given trie the data provided, associated to the
 * key passed in parameters. If that key already exists in the trie, the
 * function will fail and return -1. Otherwise, the pointer will be stored in
 * the trie and will be retrievable by using the key.
 * 
 * If a @b freeval callback was provided when allocating the trie, the data
 * associated with the key will be automatically freed when the trie is
 * destroyed.
 * 
 * @see trie_remove
 *
 */

/* -------------------------------------------------------------------------- */

public int trie_insert_r(m_trie *t, const char *key, size_t ulen, variant val);

/**
 * @ingroup trie
 * @fn trie_insert_r(m_trie *t, const char *key, size_t ulen, variant *val)
 * @param t a pointer to a trie
 * @param key the name which will be used to retrieve the stored data
 * @param ulen the length of the key
 * @param val the data that will be stored in the trie
 * @return -1 if an error occurs, 0 otherwise
 *
 * This function is simply a thread-safe wrapper around @ref trie_insert(),
 * please see the documentation of @ref trie_insert().
 *
 */

/* -------------------------------------------------------------------------- */

public variant trie_lookup(m_trie *t, const char *key, size_t ulen,
                           variant (CALLBACK *f)(variant));

/**
 * @ingroup trie
 * @fn trie_lookup(m_trie *t, const char *key, size_t ulen,
 *                 void *(CALLBACK *f)(variant *))
 * @param t a pointer to a trie
 * @param key the name which will be used to retrieve the stored data
 * @param ulen the length of the key
 * @param f a function which will process the data associated with the key
 * @return the data associated with the key, NULL, or the return value of @b f
 *
 * This function will retrieve the data associated with the provided key if
 * it exists in the trie. If the key is not found, the function will return
 * NULL instead.
 * If a callback function @b f was provided to process the data, it will be
 * called with the retrieved data in parameter and its return value will be
 * returned by @b trie_lookup
 * 
 * @note The callback function @b f should avoid altering the data passed in
 * parameter unless it can do so in a thread-safe way.
 *
 */

/* -------------------------------------------------------------------------- */

public variant trie_remove(m_trie *t, const char *key, size_t ulen);

/**
 * @ingroup trie
 * @fn trie_remove(m_trie *t, const char *key, size_t ulen)
 * @param t a pointer to a trie
 * @param key the name which will be used to retrieve the stored data
 * @param ulen the length of the key
 * @return the data associated with the key or NULL
 *
 * This function will retrieve the data associated with the provided key if
 * it exists, remove the key from the trie, and return the data.
 * 
 * If the key is not found, this function returns NULL.
 *
 */

/* -------------------------------------------------------------------------- */

public variant trie_update(m_trie *t, const char *key, size_t ulen, variant v);

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
