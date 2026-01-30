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

#include "askl_cbtrie.h"

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_TRIE
/* -------------------------------------------------------------------------- */

#include "arcane/bitops.c"

typedef struct _Node {
    void *child[2];
    uint16_t pos;
    uint8_t val;
    uint8_t bit;
} _Node;

struct _Trie {
    RW_Lock *_lock;
    void *_root;
    void (*_freeval)(Variant);
};

/* -------------------------------------------------------------------------- */

ASKL_API Trie *trie_alloc(void (*freeval)(Variant))
{
    /** @brief allocate an empty crit-bit tree */

    Trie *t = malloc(sizeof(*t));

    if (! t) {
        perror(ERR(trie_alloc, malloc));
        return NULL;
    }

    if (! (t->_lock = lock_alloc()) ) goto _err_lock;
    if (lock_init(t->_lock) == -1) goto _err_init;

    t->_root = NULL; t->_freeval = freeval;

    return t;

_err_init:
    free(t->_lock);
_err_lock:
    free(t);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static inline unsigned _max63(unsigned pos)
{
    pos |= -(pos > 63u);
    pos &= 63u;
    return pos;
}

/* -------------------------------------------------------------------------- */

static void **_insert(void **root, const uint8_t *k, size_t l, Trie_Leaf *new)
{
    uint8_t *p = NULL, byte = 0;
    int branch = 0, newbranch = 0;
    _Node *node = NULL;
    Trie_Leaf *leaf = NULL;
    unsigned prefix_len = 0, n = 0, pos = 0, critbit = 0;
    void **parent = NULL, **current_node = NULL, **ancestor = NULL;
    uint64_t bitmap = 0;

    if (unlikely(! *root)) {
        /* the tree is empty, add a new leaf */
        *root = new->key;
        return root;
    }

    parent = current_node = root;

    /* traverse the tree to find where the new node should be inserted */
    for (p = *root; (uintptr_t) p & 0x1; p = node->child[branch]) {
        node = (void *) (p - 1);
        if (likely(node->pos < l)) {
            prefix_len = __ctzll(~bitmap);
            byte = k[node->pos];
            branch = (1 + (node->bit | byte)) >> 8;

            if (likely(node->pos > prefix_len)) continue;

            if (likely(node->val != byte)) {
                critbit = __msb(node->val ^ byte) ^ 0xff;
                if (likely(critbit > node->bit)) {
                    /* XXX bytes up to the current node position all matched
                       but the critical bit is higher for the current index.
                       the current node is therefore a suitable parent. */
                    parent = current_node;
                } else if (critbit < node->bit) {
                    /* XXX there was no previous divergence and the critical
                       bit is lower: new byte for this position. */
                    pos = node->pos;
                    goto _newbyte;
                }
            } else bitmap |= (1 << _max63(node->pos));
        } else branch = 0;
        current_node = node->child + branch;
    }

    /* compute the actual divergence */
    leaf = (Trie_Leaf *) (p - offsetof(Trie_Leaf, key));
    /* skip matching bytes */
    pos = prefix_len;
    prefix_len = (leaf->len + ((l - leaf->len) & -(l < leaf->len)));

    for (n = (prefix_len - pos) & ~7u; pos < n; pos += sizeof(uint64_t)) {
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
    if (unlikely(leaf->len == l)) return NULL;

_critbit:
    critbit = __msb(p[pos] ^ k[pos]) ^ 0xff;

_newbyte:
    newbranch = (1 + (critbit | k[pos])) >> 8;
    n = (pos << 8) | critbit;

    ancestor = parent;

    for (p = *parent; (uintptr_t) p & 0x1; p = *parent) {
        node = (void *) (p - 1);
        /* enforce lexicographic order */
        if ((((uint32_t) node->pos << 8) | node->bit) > n) break;
        branch = (1 + (node->bit | k[node->pos])) >> 8;
        parent = node->child + branch;
    }

    if (! (node = malloc(sizeof(*node))) ) {
        perror(ERR(trie_insert, malloc));
        return NULL;
    }

    node->pos = pos;
    node->val = k[pos];
    node->bit = critbit;
    node->child[newbranch] = new->key;
    node->child[1 - newbranch] = *parent;

    *parent = (void *) (1 + (char *) node);

    return ancestor;
}

/* -------------------------------------------------------------------------- */

ASKL_API int trie_insert_with(
    Trie *t,
    const char *key,
    size_t len,
    Variant value,
    Variant (*function)(const char *key, size_t len, Variant new)
)
{
    Trie_Leaf *newleaf = NULL;

    if (! t || ! key || ! len) {
        debug("trie_insert(): bad parameters.\n");
        return -1;
    }

    if (len >= UINT16_MAX) {
        debug("trie_insert(): overly long key.\n");
        return -1;
    }

    if (! (newleaf = malloc(sizeof(*newleaf) + len + 1)) ) {
        perror(ERR(trie_insert, malloc));
        return -1;
    }

    if (unlikely(lock_wrlock(t->_lock) == -1)) goto _err_lock;

        if (unlikely(! _insert(& t->_root, (void *) key, len, newleaf)))
            goto _failure;

        memcpy(newleaf->key, key, len);
        newleaf->key[len] = '\0';
        newleaf->len = len;
        if (function) newleaf->val = function(key, len, value);
        else newleaf->val = value;

    lock_unlock(t->_lock);

    return 0;

_failure:
    lock_unlock(t->_lock);
_err_lock:
    free(newleaf);
    return -1;
}

/* -------------------------------------------------------------------------- */

ASKL_API int trie_insert(Trie *t, const char *key, size_t len, Variant value)
{
    return trie_insert_with(t, key, len, value, NULL);
}

/* -------------------------------------------------------------------------- */

ASKL_API int trie_insert_prefix_list(
    Trie *t,
    size_t prefix_len,
    Trie_Leaf **list,
    size_t count
)
{
    unsigned int i = 0;
    void **top = NULL;
    int result = 0;

    if (! t || ! list || ! count) {
        debug("trie_insert_batch(): bad parameters.\n");
        return -1;
    }

    if (lock_wrlock(t->_lock) == -1) return -1;

        top = _insert(& t->_root, (void *) list[0]->key, list[0]->len, list[0]);
        if (! top) {
            if (t->_freeval) t->_freeval(list[0]->val);
            free(list[0]);
            top = & t->_root;
        } else result = 1;

        /* try to find a safe insertion point */
        if (prefix_len && count > 2) {
            if (top == & t->_root) {
                uint8_t *p = NULL;
                void **next = top;
                for (p = *top; (uintptr_t) p & 0x1; p = *next) {
                    _Node *node = (void *) (p - 1);
                    int branch;
                    if (node->pos < prefix_len) {
                        top = next;
                    } else break;
                    branch = (1 + (node->bit | list[1]->key[node->pos])) >> 8;
                    next = node->child + branch;
                }
            }
        }

        for (i = 1; i < count; i ++) {
            if (! _insert(top, (void *) list[i]->key, list[i]->len, list[i])) {
                if (t->_freeval) t->_freeval(list[i]->val);
                free(list[i]);
            } else result ++;
        }

    lock_unlock(t->_lock);

    return result;
}

/* -------------------------------------------------------------------------- */

ASKL_API Variant trie_lookup(
    Trie *t,
    const char *key,
    size_t len,
    Variant (*function)(Variant)
)
{
    const uint8_t * restrict const k = (void *) key;
    uint8_t *p = NULL;
    _Node *node = NULL;
    Trie_Leaf *leaf = NULL;
    unsigned int branch = 0;
    Variant ret = { 0 };

    if (! t || ! k || ! len) {
        debug("trie_lookup(): bad parameters.\n");
        return ret;
    }

    if (lock_rdlock(t->_lock) == -1) return ret;

    if (unlikely(! t->_root)) {
        lock_unlock(t->_lock);
        return ret;
    }

    /* traverse the tree to find the node */
    for (p = t->_root; (uintptr_t) p & 0x1; p = node->child[branch]) {
        node = (void *) (p - 1);
        if (likely(node->pos < len))
            branch = (1 + (node->bit | k[node->pos])) >> 8;
        else branch = 0;
    }

    leaf = (Trie_Leaf *) (p - offsetof(Trie_Leaf, key));

    /* check for exact match */
    if (leaf->len == len && memcmp(leaf->key, k, len) == 0)
        ret = (function) ? function(leaf->val) : leaf->val;

    lock_unlock(t->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

static Variant _exists(UNUSED Variant v)
{
    return variant_true();
}

/* -------------------------------------------------------------------------- */

ASKL_API int trie_has(Trie *t, const char *key, size_t len)
{
    Variant v = trie_lookup(t, key, len, _exists);
    return (is_boolean(v) && v.value.integer);
}

/* -------------------------------------------------------------------------- */

ASKL_API Variant trie_remove_if(
    Trie *t,
    const char *key,
    size_t len,
    int (*condition)(const char *key, size_t len, Variant value)
)
{
    const uint8_t * restrict const k = (void *) key;
    uint8_t *p = NULL;
    void **ancestor = NULL, **parent = NULL;
    _Node *node = NULL;
    Trie_Leaf *leaf = NULL;
    int branch = 0;
    Variant ret = { 0 };

    if (! t || ! k || ! len) {
        debug("trie_remove(): bad parameters.\n");
        return ret;
    }

    if (lock_wrlock(t->_lock) == -1) return ret;

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

    leaf = (Trie_Leaf *) (p - offsetof(Trie_Leaf, key));

    /* check for exact match */
    if (leaf->len != len || memcmp(leaf->key, k, len)) goto _err;

    if (! condition || condition(leaf->key, leaf->len, leaf->val)) {
        /* get the associated value and free up the node */
        ret = leaf->val; free(leaf);

        if (unlikely(! ancestor)) {
            /* the tree is empty */
            t->_root = NULL; goto _err;
        } else {
            /* simplify the tree */
            *ancestor = node->child[1 - branch]; free(node);
        }
    }

_err:
    lock_unlock(t->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API Variant trie_remove(Trie *t, const char *key, size_t len)
{
    return trie_remove_if(t, key, len, NULL);
}

/* -------------------------------------------------------------------------- */

ASKL_API Variant trie_update(Trie *t, const char *key, size_t len, Variant v)
{
    const uint8_t * restrict const k = (void *) key;
    uint8_t *p = NULL;
    _Node *node = NULL;
    Trie_Leaf *leaf = NULL;
    Variant ret = { 0 };
    int branch = 0;

    if (! t || ! k || ! len) {
        debug("trie_update(): bad parameters.\n");
        return ret;
    }

    if (lock_wrlock(t->_lock) == -1) return ret;

    if (! t->_root) {
        lock_unlock(t->_lock);
        return ret;
    }

    /* traverse the tree to find the node */
    for (p = t->_root; (uintptr_t) p & 0x1; p = node->child[branch]) {
        node = (void *) (p - 1);
        if (node->pos < len)
            branch = (1 + (node->bit | k[node->pos])) >> 8;
        else branch = 0;
    }

    leaf = (Trie_Leaf *) (p - offsetof(Trie_Leaf, key));

    /* check for exact match and update the value */
    if (leaf->len == len && ! memcmp(leaf->key, k, len)) {
        ret = leaf->val;
        leaf->val = v;
    }

    lock_unlock(t->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

static int _each(
    Trie *t,
    void **top,
    int (*f)(const char *, size_t, Variant)
)
{
    uint8_t *p = NULL;
    _Node *node = NULL;
    Trie_Leaf *leaf = NULL;
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
        leaf = (Trie_Leaf *) (p - offsetof(Trie_Leaf, key));

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

ASKL_API void trie_foreach(Trie *t, int (*f)(const char *, size_t, Variant))
{
    if (! t) {
        debug("trie_foreach(): bad parameters.\n");
        return;
    }

    if (lock_wrlock(t->_lock) == -1) return;

    if (! (t->_root) ) {
        lock_unlock(t->_lock);
        return;
    }

    _each(t, & t->_root, f);

    lock_unlock(t->_lock);
}

/* -------------------------------------------------------------------------- */

static int _delete(UNUSED const char *k, UNUSED size_t l, UNUSED Variant v)
{
    return -1;
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie *trie_free(Trie *t)
{
    if (! t) return NULL;
    trie_foreach(t, _delete);
    lock_destroy(t->_lock);
    lock_free(t->_lock); free(t);
    return NULL;
}

/* -------------------------------------------------------------------------- */
/* Iterator */
/* -------------------------------------------------------------------------- */

static int _iterator_push(Trie_Iterator *iterator, uint8_t *p)
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

static inline uint8_t *_iterator_pop(Trie_Iterator *iterator)
{
    if (unlikely(! iterator->_node_count)) return NULL;
    return iterator->_node[-- iterator->_node_count];
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_each(Trie *t)
{
    Trie_Iterator *iterator = NULL;

    if (! t) {
        debug("trie_each(): bad parameters.\n");
        return NULL;
    }

    if (lock_rdlock(t->_lock) == -1) return NULL;

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
    lock_unlock(t->_lock);
    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_each_prefix(Trie *t, const char *prefix, size_t len)
{
    const uint8_t * restrict const k = (void *) prefix;
    uint8_t *p = NULL, *top = NULL;
    unsigned int branch = 0;
    Trie_Leaf *leaf = NULL;
    Trie_Iterator *iterator = NULL;

    if (! t || ! k || ! len) {
        debug("trie_each_prefix(): bad parameters.\n");
        return NULL;
    }

    if (lock_rdlock(t->_lock) == -1) return NULL;

    if (! (top = p = t->_root) ) {
        debug("trie_each_prefix(): empty trie.\n");
        goto _err;
    }

    /* find the best match for the given prefix */
    while ((uintptr_t) p & 0x1) {
        _Node *node = (void *) (p - 1);
        if (likely(node->pos < len)) {
            branch = (1 + (node->bit | k[node->pos])) >> 8;
            top = node->child[branch];
        } else branch = 0;
        p = node->child[branch];
    }

    leaf = (Trie_Leaf *) (p - offsetof(Trie_Leaf, key));

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
    lock_unlock(t->_lock);
    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_next(Trie_Iterator *iterator)
{
    uint8_t *p = NULL;
    _Node *node = NULL;
    Trie_Leaf *leaf = NULL;

    for (p = _iterator_pop(iterator); (uintptr_t) p & 0x1; p = *node->child) {
        node = (void *) (p - 1);
        if (unlikely(_iterator_push(iterator, node->child[1]) == -1))
            return trie_break(iterator);
    }

    if (likely(p)) {
        leaf = (Trie_Leaf *) (p - offsetof(Trie_Leaf, key));
        iterator->key = leaf->key;
        iterator->len = leaf->len;
        iterator->val = leaf->val;
        return iterator;
    }

    return trie_break(iterator);
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_break(Trie_Iterator *iterator)
{
    if (! iterator) {
        debug("trie_break(): bad parameters.\n");
        return NULL;
    }

    lock_unlock(iterator->trie->_lock);
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
