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

#include "askl_cbtrie.h"

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_TRIE
/* -------------------------------------------------------------------------- */

#include "arcane/bitops.c"

typedef struct _node {
    void *child[2];
    uint16_t pos;
    uint8_t val;
    uint8_t bit;
} _node;

typedef struct _leaf {
    uint16_t len;
    uint16_t pad;
    variant val;
    char key[];
} _leaf;

struct _ASKL_Trie {
    pthread_rwlock_t *_lock;
    void *_root;
    void (*_freeval)(variant);
};

/* -------------------------------------------------------------------------- */

public ASKL_Trie *trie_alloc(void (*freeval)(variant))
{
    /** @brief allocate an empty crit-bit tree */

    ASKL_Trie *t = malloc(sizeof(*t));

    if (! t) {
        perror(ERR(trie_alloc, malloc));
        return NULL;
    }

    if (! (t->_lock = malloc(sizeof(*t->_lock))) ) {
        perror(ERR(trie_alloc, malloc));
        goto _err_lock;
    }

    if (pthread_rwlock_init(t->_lock, NULL) == -1) {
        perror(ERR(trie_alloc, pthread_rwlock_init));
        goto _err_init;
    }

    t->_root = NULL; t->_freeval = freeval;

    return t;

_err_init:
    free(t->_lock);
_err_lock:
    free(t);

    return NULL;
}

/* -------------------------------------------------------------------------- */

public int trie_insert(ASKL_Trie *t, const char *key, size_t len, variant value)
{
    const uint8_t * restrict const k = (void *) key;
    uint8_t *p = NULL, byte = 0;
    int branch = 0, newbranch = 0;
    _node *node = NULL;
    _leaf *leaf = NULL, *newleaf = NULL;
    unsigned prefix_len = 0, n = 0, pos = 0, critbit = 0;
    void **parent = NULL, **last_diff = NULL, **prev_node = NULL;

    if (! t || ! k || ! len) {
        debug("trie_insert(): bad parameters.\n");
        return -1;
    }

    if (len >= UINT16_MAX) {
        debug("trie_insert(): overly long key.\n");
        return -1;
    }

    if (! (newleaf = malloc(sizeof(*newleaf) + len + 1)) ) {
        perror(ERR(trie_insert, malloc));
        goto _err_leaf;
    }

    newleaf->val = value; newleaf->len = len;

    pthread_rwlock_wrlock(t->_lock);

    if (unlikely(! t->_root)) {
        /* the tree is empty, add a new leaf */
        t->_root = newleaf->key;
        goto _success;
    } else parent = prev_node = & t->_root;

    /* traverse the tree to find where the new node should be inserted */
    for (p = t->_root; (uintptr_t) p & 0x1; p = node->child[branch]) {
        node = (void *) (p - 1);
        if (likely(node->pos < len)) {
            byte = k[node->pos];
            branch = (1 + (node->bit | byte)) >> 8;
            if (likely(node->val != byte)) {
                critbit = __msb(node->val ^ byte) ^ 0xff;
                if (node->bit > critbit) {
                    pos = node->pos;
                    byte = node->val;
                    if (last_diff) parent = last_diff;
                    goto _newbyte;
                }
                last_diff = node->child + branch;
            } else parent = prev_node;
            prev_node = node->child + branch;
            PREFETCH((char *) node->child[branch] - 1, 0, NTACCESS);
        } else branch = 0;
    }

    /* found a leaf, compute the divergence */
    leaf = (_leaf *) (p - offsetof(_leaf, key));
    prefix_len = (leaf->len + ((len - leaf->len) & -(len < leaf->len)));

    for (n = prefix_len & ~7u; pos < n; pos += sizeof(uint64_t)) {
        uint64_t bytes, leaf64;
        memcpy(& bytes, k + pos, sizeof(bytes));
        memcpy(& leaf64, p + pos, sizeof(leaf64));
        if (likely(bytes ^= leaf64)) {
            pos += __zero_idx64(bytes);
            goto _critbit;
        }
    }

    switch (prefix_len - pos) {
    case 7: if (p[pos] ^ k[pos]) goto _critbit; pos ++;
    case 6: if (p[pos] ^ k[pos]) goto _critbit; pos ++;
    case 5: if (p[pos] ^ k[pos]) goto _critbit; pos ++;
    case 4: {
        uint32_t bytes, leaf32;
        memcpy(& bytes, k + pos, sizeof(bytes));
        memcpy(& leaf32, p + pos, sizeof(leaf32));
        if (likely(bytes ^= leaf32)) {
            pos += __zero_idx(bytes);
            goto _critbit;
        }
        pos += sizeof(bytes);
    } break;
    case 3: if (p[pos] ^ k[pos]) goto _critbit; pos ++;
    case 2: if (p[pos] ^ k[pos]) goto _critbit; pos ++;
    case 1: if (p[pos] ^ k[pos]) goto _critbit; pos ++;
    }

    /* duplicate key */
    if (unlikely(prefix_len == len)) goto _failure;

_critbit:
    critbit = __msb(p[pos] ^ k[pos]) ^ 0xff;
    byte = p[pos];

_newbyte:
    newbranch = (1 + (critbit | byte)) >> 8;
    n = (pos << 8) | critbit;

    for (p = *parent; (uintptr_t) p & 0x1; p = *parent) {
        node = (void *) (p - 1);
        /* enforce lexicographic order */
        if ((((uint32_t) node->pos << 8) | node->bit) > n) break;
        branch = (1 + (node->bit | k[node->pos])) >> 8;
        parent = node->child + branch;
    }

    if (! (node = malloc(sizeof(*node))) ) {
        perror(ERR(trie_insert, malloc));
        goto _failure;
    }

    node->pos = pos;
    node->val = k[pos];
    node->bit = critbit;
    node->child[1 - newbranch] = newleaf->key;
    node->child[newbranch] = *parent;

    *parent = (void *) (1 + (char *) node);

_success:
    memcpy(newleaf->key, k, len);
    newleaf->key[len] = '\0';
    pthread_rwlock_unlock(t->_lock);
    return 0;

_failure:
    pthread_rwlock_unlock(t->_lock);
    free(newleaf);
_err_leaf:
    return -1;
}

/* -------------------------------------------------------------------------- */

public variant trie_lookup(ASKL_Trie *t, const char *key, size_t len,
                           variant (CALLBACK *function)(variant))
{
    const uint8_t * restrict const k = (void *) key;
    uint8_t *p = NULL;
    _node *node = NULL;
    _leaf *leaf = NULL;
    unsigned int branch = 0;
    variant ret = { 0 };

    if (! t || ! k || ! len) {
        debug("trie_lookup(): bad parameters.\n");
        return ret;
    }

    pthread_rwlock_rdlock(t->_lock);

    if (unlikely(! t->_root)) {
        pthread_rwlock_unlock(t->_lock);
        return ret;
    }

    /* traverse the tree to find the node */
    for (p = t->_root; (uintptr_t) p & 0x1; p = node->child[branch]) {
        node = (void *) (p - 1);
        if (likely(node->pos < len))
            branch = (1 + (node->bit | k[node->pos])) >> 8;
        else branch = 0;
    }

    leaf = (_leaf *) (p - offsetof(_leaf, key));

    /* check for exact match */
    if (leaf->len == len && memcmp(leaf->key, k, len) == 0)
        ret = (function) ? function(leaf->val) : leaf->val;

    pthread_rwlock_unlock(t->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

public variant trie_remove(ASKL_Trie *t, const char *key, size_t len)
{
    const uint8_t * restrict const k = (void *) key;
    uint8_t *p = NULL;
    void **ancestor = NULL, **parent = NULL;
    _node *node = NULL;
    _leaf *leaf = NULL;
    int branch = 0;
    variant ret = { 0 };

    if (! t || ! k || ! len) {
        debug("trie_remove(): bad parameters.\n");
        return ret;
    }

    pthread_rwlock_wrlock(t->_lock);

    if (unlikely(! t->_root))
        goto _err;
    else parent = & t->_root;

    /* traverse the tree to find the node */
    for (p = *parent; (uintptr_t) p & 0x1; p = *parent) {
        ancestor = parent;
        node = (void *) (p - 1);
        if (node->pos < len)
            branch = (1 + (node->bit | k[node->pos])) >> 8;
        else branch = 0;
        parent = node->child + branch;
    }

    leaf = (_leaf *) (p - offsetof(_leaf, key));

    /* check for exact match */
    if (leaf->len != len || memcmp(leaf->key, k, len)) goto _err;

    /* get the associated value and free up the node */
    ret = leaf->val; free(leaf);

    if (unlikely(! ancestor)) {
        /* the tree is empty */
        t->_root = NULL; goto _err;
    } else {
        /* simplify the tree */
        *ancestor = node->child[1 - branch]; free(node);
    }

_err:
    pthread_rwlock_unlock(t->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

public variant trie_update(ASKL_Trie *t, const char *key, size_t len, variant v)
{
    const uint8_t * restrict const k = (void *) key;
    uint8_t *p = NULL;
    _node *node = NULL;
    _leaf *leaf = NULL;
    variant ret = { 0 };
    int branch = 0;

    if (! t || ! k || ! len) {
        debug("trie_update(): bad parameters.\n");
        return ret;
    }

    pthread_rwlock_wrlock(t->_lock);

    if (! t->_root) {
        pthread_rwlock_unlock(t->_lock);
        return ret;
    }

    /* traverse the tree to find the node */
    for (p = t->_root; (uintptr_t) p & 0x1; p = node->child[branch]) {
        node = (void *) (p - 1);
        if (node->pos < len)
            branch = (1 + (node->bit | k[node->pos])) >> 8;
        else branch = 0;
    }

    leaf = (_leaf *) (p - offsetof(_leaf, key));

    /* check for exact match and update the value */
    if (leaf->len == len && ! memcmp(leaf->key, k, len)) {
        ret = leaf->val;
        leaf->val = v;
    }

    pthread_rwlock_unlock(t->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

static int _each(
    ASKL_Trie *t,
    void **top,
    int (*f)(const char *, size_t, variant)
)
{
    uint8_t *p = NULL;
    _node *node = NULL;
    _leaf *leaf = NULL;
    int ret[2] = { 0, 0 };

    if (! (p = *top) ) return -1;

    if ((uintptr_t) p & 0x1) {
        node = (void *) (p - 1);

        ret[0] = _each(t, & node->child[0], f);
        ret[1] = _each(t, & node->child[1], f);

        if (ret[0] == -1) {
            *top = (ret[1] == -1) ? NULL : node->child[1];
            goto _free_node;
        } else if (ret[1] == -1) {
            *top = node->child[0];
            goto _free_node;
        }
    } else {
        leaf = (_leaf *) (p - offsetof(_leaf, key));

        if ( (ret[0] = f(leaf->key, leaf->len, leaf->val)) == -1) {
            if (t->_freeval) t->_freeval(leaf->val);
            free(leaf);
        }

        return ret[0];
    }

    return 0;

_free_node:
    free(node);
    return 0;
}

/* -------------------------------------------------------------------------- */

public void trie_foreach(ASKL_Trie *t, int (*f)(const char *, size_t, variant))
{
    if (! t) {
        debug("trie_foreach(): bad parameters.\n");
        return;
    }

    pthread_rwlock_wrlock(t->_lock);

    if (! (t->_root) ) {
        pthread_rwlock_unlock(t->_lock);
        return;
    }

    _each(t, & t->_root, f);

    pthread_rwlock_unlock(t->_lock);
}

/* -------------------------------------------------------------------------- */

static int _delete(UNUSED const char *k, UNUSED size_t l, UNUSED variant v)
{
    return -1;
}

/* -------------------------------------------------------------------------- */

public ASKL_Trie *trie_free(ASKL_Trie *t)
{
    if (! t) return NULL;
    trie_foreach(t, _delete);
    pthread_rwlock_destroy(t->_lock);
    free(t->_lock); free(t);
    return NULL;
}

/* -------------------------------------------------------------------------- */
/* Iterator */
/* -------------------------------------------------------------------------- */

static int _iterator_push(ASKL_TrieIterator *iterator, uint8_t *p)
{
    if (iterator->_node_count == iterator->_node_alloc) {
        uint8_t **nodes = NULL;
        size_t new_size = iterator->_node_alloc + 16;

        if (unlikely(new_size < iterator->_node_alloc)) {
            debug("_iterator_push(): integer overflow.\n");
            return -1;
        }

        nodes = realloc(iterator->_node, new_size * sizeof(*iterator->_node));
        if (! nodes) {
            perror(ERR(_iterator_push, realloc));
            return -1;
        }

        iterator->_node = nodes;
        iterator->_node_alloc = new_size;
    }

    iterator->_node[iterator->_node_count ++] = p;

    return 0;
}

/* -------------------------------------------------------------------------- */

static inline uint8_t *_iterator_pop(ASKL_TrieIterator *iterator)
{
    if (unlikely(! iterator->_node_count)) return NULL;
    return iterator->_node[-- iterator->_node_count];
}

/* -------------------------------------------------------------------------- */

public ASKL_TrieIterator *trie_each(ASKL_Trie *t)
{
    ASKL_TrieIterator *iterator = NULL;

    if (! t) {
        debug("trie_each(): bad parameters.\n");
        return NULL;
    }

    pthread_rwlock_rdlock(t->_lock);

    if (! t->_root) {
        debug("trie_each(): empty trie.\n");
        goto _err;
    }

    if (! (iterator = malloc(sizeof(*iterator)))) {
        perror(ERR(trie_each, malloc));
        goto _err;
    }

    iterator->trie = t;
    iterator->_node_count = iterator->_node_alloc = 0;
    iterator->_node = NULL;

    if (_iterator_push(iterator, t->_root) == -1) goto _err_push;

    return trie_next(iterator);

_err_push:
    free(iterator);
_err:
    pthread_rwlock_unlock(t->_lock);
    return NULL;
}

/* -------------------------------------------------------------------------- */

public ASKL_TrieIterator *trie_each_prefix(
    ASKL_Trie *t,
    const char *prefix,
    size_t len
)
{
    const uint8_t * restrict const k = (void *) prefix;
    uint8_t *p = NULL, *top = NULL;
    unsigned int branch = 0;
    _leaf *leaf = NULL;
    ASKL_TrieIterator *iterator = NULL;

    if (! t || ! k || ! len) {
        debug("trie_each_prefix(): bad parameters.\n");
        return NULL;
    }

    pthread_rwlock_rdlock(t->_lock);

    if (! (top = p = t->_root) ) {
        debug("trie_each_prefix(): empty trie.\n");
        goto _err;
    }

    /* find the best match for the given prefix */
    while ((uintptr_t) p & 0x1) {
        _node *node = (void *) (p - 1);
        if (likely(node->pos < len)) {
            branch = (1 + (node->bit | k[node->pos])) >> 8;
            top = node->child[branch];
        } else branch = 0;
        p = node->child[branch];
    }

    leaf = (_leaf *) (p - offsetof(_leaf, key));

    /* check if the best match is correct */
    if (leaf->len < len || memcmp(leaf->key, k, len)) {
        debug("trie_each_prefix(): prefix not found.\n");
        goto _err;
    }

    if (! (iterator = malloc(sizeof(*iterator)))) {
        perror(ERR(trie_each_prefix, malloc));
        goto _err;
    }

    iterator->trie = t;
    iterator->_node_count = iterator->_node_alloc = 0;
    iterator->_node = NULL;

    if (unlikely(_iterator_push(iterator, top) == -1)) goto _err_push;

    return trie_next(iterator);

_err_push:
    free(iterator);
_err:
    pthread_rwlock_unlock(t->_lock);
    return NULL;
}

/* -------------------------------------------------------------------------- */

public ASKL_TrieIterator * CALLBACK trie_next(ASKL_TrieIterator *iterator)
{
    uint8_t *p = NULL;
    _node *node = NULL;
    _leaf *leaf = NULL;

    for (p = _iterator_pop(iterator); (uintptr_t) p & 0x1; p = *node->child) {
        node = (void *) (p - 1);
        if (unlikely(_iterator_push(iterator, node->child[1]) == -1))
            return trie_break(iterator);
    }

    if (likely(p)) {
        leaf = (_leaf *) (p - offsetof(_leaf, key));
        iterator->key = leaf->key;
        iterator->len = leaf->len;
        iterator->val = leaf->val;
        return iterator;
    }

    return trie_break(iterator);
}

/* -------------------------------------------------------------------------- */

public ASKL_TrieIterator *trie_break(ASKL_TrieIterator *iterator)
{
    if (! iterator) {
        debug("trie_break(): bad parameters.\n");
        return NULL;
    }

    pthread_rwlock_unlock(iterator->trie->_lock);
    free(iterator->_node);
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
