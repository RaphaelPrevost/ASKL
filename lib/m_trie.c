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

#include "m_trie.h"

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_TRIE
/* -------------------------------------------------------------------------- */

typedef struct _m_node {
    void *child[2];
    uint16_t byte;
    uint8_t val;
    uint8_t otherbits;
} _m_node;

typedef struct _m_leaf {
    variant val;
    uint16_t len;
    char key[];
} _m_leaf;

/* -------------------------------------------------------------------------- */
/* BITWISE OPERATIONS */
/* -------------------------------------------------------------------------- */

#include "ports/m_port_bitops.c"

/* -------------------------------------------------------------------------- */

public m_trie *trie_alloc(void (*freeval)(variant))
{
    /** @brief allocate an empty crit-bit tree */

    m_trie *t = malloc(sizeof(*t));

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

public int trie_insert_r(m_trie *t, const char *key, size_t ulen, variant val)
{
    int ret = 0;

    if (! t) {
        debug("trie_insert(): bad parameters.\n");
        return -1;
    }

    pthread_rwlock_wrlock(t->_lock);

    ret = trie_insert(t, key, ulen, val);

    pthread_rwlock_unlock(t->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

public int trie_insert(m_trie *t, const char *key, size_t ulen, variant value)
{
    const uint8_t * restrict const ubytes = (void *) key;
    uint8_t *p = NULL, c = 0;
    unsigned int direction = 0, newdirection = 0;
    _m_node *node = NULL, *n = NULL;
    _m_leaf *leaf = NULL;
    unsigned int prefix_len = 0, newbyte = 0, newotherbits = 0;
    int32_t allocsize = 0;
    void **children = NULL, **wherep = NULL;

    if (! t || ! key || ! ulen) {
        debug("trie_insert(): bad parameters.\n");
        return -1;
    }

    allocsize = sizeof(*leaf) + ulen + 1;
    if ((size_t) allocsize < ulen) {
        debug("trie_insert(): integer overflow.\n");
        return -1;
    }

    if (! (p = t->_root) ) {
        /* the tree is empty, allocate a new leaf node */
        if (! (leaf = malloc(allocsize)) ) {
            perror(ERR(trie_insert, malloc));
            return -1;
        }

        leaf->val = value; leaf->len = ulen;
        memcpy(leaf->key, key, ulen);
        leaf->key[ulen] = '\0';

        t->_root = leaf->key;

        return 0;
    }

    /* traverse the tree to find where the new node should be inserted */
    for (wherep = & t->_root; (intptr_t) p & 0x1; p = children[direction]) {
        node = (void *) (p - 1);
        children = node->child;

        if (likely(node->byte < ulen)) {
            c = ubytes[node->byte];
            direction = (1 + (node->otherbits | c)) >> 8;
            if (likely(node->val != c)) {
                newotherbits = __msb(node->val ^ c) ^ 0xff;
                if (likely(node->otherbits > newotherbits)) {
                    newbyte = node->byte;
                    c = node->val;
                    goto different_byte_found;
                }
            } else wherep = node->child + direction;
        } else direction = (1 + node->otherbits) >> 8;
    }

    /* found a leaf, compute the divergence */
    leaf = (_m_leaf *) (p - offsetof(_m_leaf, key));
    prefix_len = (leaf->len + ((ulen - leaf->len) & -(ulen < leaf->len)));

    if (prefix_len >= 16) {
        unsigned int index, len64 = prefix_len & ~7u;
        uint64_t bytes;

        for (newbyte = 0; newbyte < len64; newbyte += sizeof(bytes)) {
            memcpy(& bytes, ubytes + newbyte, sizeof(bytes));
            index = __zero_idx64(*(uint64_t *)(p + newbyte) ^ bytes);
            if (likely(index < sizeof(bytes))) {
                newbyte += index;
                goto _newotherbits;
            }
        }
    }

    for ( ; newbyte < prefix_len; newbyte ++) {
        if (likely(p[newbyte] != ubytes[newbyte]))
            goto _newotherbits;
    }

    if (unlikely(prefix_len == ulen)) return -1;

_newotherbits:
    c = p[newbyte];
    newotherbits = __msb(p[newbyte] ^ ubytes[newbyte]) ^ 0xff;

different_byte_found:

    newdirection = (1 + (newotherbits | c)) >> 8;

    if (! (node = malloc(sizeof(*node))) ) {
        perror(ERR(trie_insert, malloc));
        return -1;
    }

    node->byte = newbyte;
    node->val = ubytes[newbyte];
    node->otherbits = newotherbits;

    if (! (leaf = malloc(allocsize)) ) {
        perror(ERR(trie_insert, malloc));
        free(node);
        return -1;
    }

    leaf->val = value; leaf->len = ulen;
    memcpy(leaf->key, key, ulen);
    leaf->key[ulen] = '\0';

    while ( (p = *wherep) ) {
        if (! ((intptr_t) p & 0x1)) break;
        n = (void *) (p - 1);
        if (n->byte > newbyte) break;
        if (n->byte == newbyte && n->otherbits > newotherbits) break;
        c = (likely(n->byte < ulen)) ? ubytes[n->byte] : 0;
        direction = (1 + (n->otherbits | c)) >> 8;
        wherep = n->child + direction;
    }

    node->child[1 - newdirection] = leaf->key;
    node->child[newdirection] = *wherep;
    *wherep = (void *) (1 + (char *) node);

    return 0;
}

/* -------------------------------------------------------------------------- */

public variant trie_lookup(m_trie *t, const char *key, size_t ulen,
                           variant (CALLBACK *function)(variant))
{
    uint8_t *p = NULL, c = 0;
    _m_node *node = NULL;
    _m_leaf *leaf = NULL;
    unsigned int direction = 0;
    variant ret = { 0 };
    const uint8_t *ubytes = (void *) key;

    if (! t || ! key || ! ulen) {
        debug("trie_lookup(): bad parameters.\n");
        return ret;
    }

    pthread_rwlock_rdlock(t->_lock);

    if (! (p = t->_root) ) {
        pthread_rwlock_unlock(t->_lock);
        return ret;
    }

    /* traverse the tree to find the node */
    while ((intptr_t) p & 0x1) {
        node = (void *) (p - 1);
        c = (node->byte < ulen) ? ubytes[node->byte] : 0;
        direction = (1 + (node->otherbits | c)) >> 8;
        p = node->child[direction];
    }

    leaf = (_m_leaf *) (p - offsetof(_m_leaf, key));

    /* check for exact match */
    if (leaf->len == ulen && memcmp(leaf->key, key, ulen) == 0)
        ret = (function) ? function(leaf->val) : leaf->val;

    pthread_rwlock_unlock(t->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

public variant trie_remove(m_trie *t, const char *key, size_t ulen)
{
    uint8_t *p = NULL, c = 0;
    void **wherep = NULL, **whereq = NULL;
    _m_node *node = NULL;
    _m_leaf *leaf = NULL;
    int direction = 0;
    variant ret = { 0 };
    const uint8_t *ubytes = (void *) key;

    if (! t || ! key || ! ulen) {
        debug("trie_remove(): bad parameters.\n");
        return ret;
    }

    pthread_rwlock_wrlock(t->_lock);

    if (unlikely(! (p = t->_root)))
        goto _err;
    else wherep = & t->_root;

    /* traverse the tree to find the node */
    while ((intptr_t) p & 0x1) {
        whereq = wherep;
        node = (void *) (p - 1);
        c = (node->byte < ulen) ? ubytes[node->byte] : 0;
        direction = (1 + (node->otherbits | c)) >> 8;
        wherep = node->child + direction;
        p = *wherep;
    }

    leaf = (_m_leaf *) (p - offsetof(_m_leaf, key));

    /* check for exact match */
    if (leaf->len != ulen || memcmp(leaf->key, key, ulen)) goto _err;

    /* get the associated value and free up the node */
    ret = leaf->val; free(leaf);

    if (unlikely(! whereq)) {
        /* the tree is empty */
        t->_root = NULL; goto _err;
    } else {
        /* simplify the tree */
        *whereq = node->child[1 - direction]; free(node);
    }

_err:
    pthread_rwlock_unlock(t->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

public variant trie_update(m_trie *t, const char *key, size_t ulen, variant v)
{
    uint8_t *p = NULL, c = 0;
    _m_node *node = NULL;
    _m_leaf *leaf = NULL;
    variant ret = { 0 };
    int direction = 0;
    const uint8_t *ubytes = (void *) key;

    if (! t || ! key || ! ulen) {
        debug("trie_update(): bad parameters.\n");
        return ret;
    }

    pthread_rwlock_wrlock(t->_lock);

    if (! (p = t->_root) ) {
        pthread_rwlock_unlock(t->_lock);
        return ret;
    }

    /* traverse the tree to find the node */
    while ((intptr_t) p & 0x1) {
        node = (void *) (p - 1);
        c = (node->byte < ulen) ? ubytes[node->byte] : 0;
        direction = (1 + (node->otherbits | c)) >> 8;
        p = node->child[direction];
    }

    leaf = (_m_leaf *) (p - offsetof(_m_leaf, key));

    /* check for exact match and update the value */
    if (leaf->len == ulen && ! memcmp(leaf->key, key, ulen)) {
        ret = leaf->val;
        leaf->val = v;
    }

    pthread_rwlock_unlock(t->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

static int _each(m_trie *t, void **top, int (*f)(const char *, size_t, variant))
{
    uint8_t *p = NULL;
    _m_node *node = NULL;
    _m_leaf *leaf = NULL;
    int ret[2] = { 0, 0 };

    if (! (p = *top) ) return -1;

    if ((intptr_t) p & 0x1) {
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
        leaf = (_m_leaf *) (p - offsetof(_m_leaf, key));

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

public void trie_foreach(m_trie *t, int (*f)(const char *, size_t, variant))
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

public m_trie *trie_free(m_trie *t)
{
    if (! t) return NULL;
    trie_foreach(t, _delete);
    pthread_rwlock_destroy(t->_lock);
    free(t->_lock); free(t);
    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _each_prefix(uint8_t *top, int (*f)(const char *, size_t, variant))
{
    _m_node *node = NULL;
    _m_leaf *leaf = NULL;
    int direction = 0, ret = 1;

    if ((intptr_t) top & 0x1) {
        node = (void *) (top - 1);

        for (direction = 0; direction < 2; ++ direction) {
            switch (_each_prefix(node->child[direction], f)) {
                case 1: break;
                case 0: return 0;
                default: return -1;
            }
        }

        return 1;
    }

    leaf = (_m_leaf *) (top - offsetof(_m_leaf, key));
    ret = f(leaf->key, leaf->len, leaf->val);

    return ret;
}

/* -------------------------------------------------------------------------- */

public void trie_foreach_prefix(m_trie *t, const char *prefix, size_t ulen,
                                int (*f)(const char *, size_t, variant))
{
    const uint8_t *ubytes = (void *) prefix;
    uint8_t *p = NULL, *top = NULL, c = 0;
    int direction = 0;
    _m_node *node = NULL;
    _m_leaf *leaf = NULL;

    if (! t || ! prefix || ! ulen) {
        debug("trie_foreach_prefix(): bad parameters.\n");
        return;
    }

    pthread_rwlock_rdlock(t->_lock);

    if (! (top = p = t->_root) ) {
        pthread_rwlock_unlock(t->_lock);
        return;
    }

    /* find the best match for the given prefix */
    while ((intptr_t) p & 0x1) {
        node = (void *)(p - 1);
        c = (node->byte < ulen) ? ubytes[node->byte] : 0;
        direction = (1 + (node->otherbits | c)) >> 8;
        p = node->child[direction];
        if (node->byte < ulen) top = p;
    }

    leaf = (_m_leaf *) (p - offsetof(_m_leaf, key));

    /* check if the best match is ok */
    if (leaf->len != ulen || memcmp(leaf->key, prefix, ulen)) {
        pthread_rwlock_unlock(t->_lock);
        return;
    }

    /* start iterating through the child nodes */
    _each_prefix(top, f);

    pthread_rwlock_unlock(t->_lock);
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

/* Trie support will not be compiled in the Concrete Library */
#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
