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
    #if (UINTPTR_MAX > 0xffffffffU)
    uint32_t idx; /* reclaim 32 bits of padding and cache the node index */
    #endif
} _Node;

/**
 * @ingroup trie
 * @struct _Node
 *
 * Internal branch node of the crit-bit tree.
 *
 * Internal nodes are stored as tagged pointers with their low bit set; an
 * untagged pointer denotes the inline key of a @ref Trie_Leaf. Each node tests
 * one critical bit of one key byte and selects one of its two child subtrees.
 *
 * @b private @ref child holds the two subtrees selected by the critical bit;
 *                       child[0] precedes child[1] in key order
 * @b private @ref pos is the byte offset tested by this node
 * @b private @ref bit is the encoded critical-bit selector applied to the
 *                     byte at @ref pos
 * @b private @ref val caches the byte associated with this split, used while
 *                     inserting keys to recover already established prefix
 *                     matches
 * @b private @ref idx caches the ordering index formed from @ref pos and
 *                     @ref bit on platforms where it fits in structure padding.
 *
 * This type is internal and may change at any time.
 */

struct _Trie {
    RW_Lock *_lock;
    void *_root;
    void (*_freeval)(Variant);
};

/**
 * @ingroup trie
 * @struct _Trie
 *
 * This structure holds the internal state of a @ref Trie.
 *
 * A Trie is a binary crit-bit tree indexed by byte-string keys. Internal
 * nodes encode the critical bit that distinguishes two subtrees while leaves
 * store the complete key and its associated @ref Variant value.
 *
 * Internal nodes are represented by tagged pointers with their low bit set,
 * while untagged pointers refer to the inline key of a @ref Trie_Leaf. The
 * root and every child slot can therefore refer directly to either another
 * branch node or a leaf without a separate node-type field.
 *
 * The tree maintains bytewise lexicographic key order, allowing ordered
 * traversal, prefix lookup, and subtree-restricted iteration directly from
 * the crit-bit structure.
 *
 * The implementation follows D. J. Bernstein's crit-bit tree design with
 * additional optimizations for insertion.
 *
 * Concurrency:
 * - Readers take a read lock for lookups and traversal.
 * - Writers take a write lock for insertion, removal, and updates.
 * - Iterators retain their read lock until exhausted or explicitly broken.
 * - Locking may be disabled when synchronization is provided by the owner.
 *
 * @b private @ref _lock is the reader/writer lock protecting the tree,
 *                       or NULL when internal locking has been disabled
 * @b private @ref _root is the root pointer: NULL for an empty trie, a
 *                       tagged pointer to an @ref _Node for an internal
 *                       subtree, or an untagged pointer to a @ref Trie_Leaf key
 * @b private @ref _freeval is an optional destructor callback used when
 *                          values owned by trie leaves are discarded.
 *
 * This type is internal and may change at any time; only use the public
 * @ref Trie API.
 */

typedef struct _Subtree {
    uint8_t *next;
    uint16_t divergence;
} _Subtree;

typedef struct _Trie_Iterator {
    struct Trie_Iterator interface;
    Trie *trie;
    void *_top;
    _Subtree *_stack;
    uint32_t _stack_alloc;
    uint32_t _stack_count;
    char *_next;
    uint32_t _next_alloc;
    uint16_t _prefix_len;
    uint8_t _sep;
} _Trie_Iterator;

STATIC_ASSERT(
    offsetof(_Trie_Iterator, interface) == 0,
    interface_must_be_first
);

/**
 * @ingroup trie
 * @struct _Trie_Iterator
 *
 * Internal state backing a @ref Trie_Iterator.
 *
 * The public iterator interface is the first member so that a public iterator
 * pointer can be converted to its private state.
 *
 * Traversal is non-recursive. @ref _stack contains deferred subtrees; each
 * entry also records where its keys may first differ from the preceding key.
 *
 * Child iteration keeps a fixed prefix and a scratch key for seeking past
 * descendants of the current child. @ref _top bounds those seeks to the
 * selected subtree.
 *
 * The iterator holds a read lock until it is exhausted or passed to
 * @ref trie_break().
 *
 * @b private @ref interface public iterator state; must be first
 * @b private @ref trie trie being traversed
 * @b private @ref _top subtree bounding child iteration
 * @b private @ref _stack deferred subtrees
 * @b private @ref _stack_alloc allocated entries in @ref _stack
 * @b private @ref _stack_count active entries in @ref _stack
 * @b private @ref _next scratch key used by child iteration
 * @b private @ref _next_alloc allocated size of @ref _next
 * @b private @ref _prefix_len fixed child-iteration prefix length
 * @b private @ref _sep child separator
 *
 * This type is internal and may change at any time.
 */

typedef struct _Cursor_Step {
    uint8_t *next;
    uint16_t pos;
} _Cursor_Step;

/* nearby keys to scan before seeking past the current child */
#define TRIE_CHILD_SCAN 8

/**
 * @ingroup trie
 * @struct _Cursor_Step
 *
 * One recorded branch in a prefix-lookup cursor.
 *
 * A cursor records the child selected at each tested key byte so a later
 * lookup sharing the same leading bytes can resume below the common path
 * instead of descending again from the root.
 *
 * @b private @ref next is the child subtree selected by the branch.
 * @b private @ref pos is the key byte offset tested by the branch.
 *
 * This type is internal and may change at any time.
 */

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
    uint8_t *p = NULL, val = 0;
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
            uint8_t byte = k[node->pos];
            branch = (1 + (node->bit | byte)) >> 8;

            if (likely(node->pos > prefix_len)) continue;

            if (node->val == byte) {
                bitmap |= (1ULL << _max63(node->pos));
                prefix_len = __ctzll(~bitmap);
                continue;
            }

            critbit = __msb(node->val ^ byte) ^ 0xff;

            if (likely(critbit + 1 > ((node->bit + 1) & 0xff))) {
                /* XXX bytes up to the current node position all matched
                   but the critical bit is higher for the current index.
                   the current node is therefore a suitable parent. */
                parent = current_node;
            } else if (critbit + 1 < ((node->bit + 1) & 0xff)) {
                /* XXX there was no previous divergence and the critical
                   bit is lower: new byte for this position. */
                pos = node->pos;
                val = byte;
                goto _newbyte;
            }
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

    /* one key is a prefix of the other */
    critbit = 0xff;
    val = p[pos] | (k[pos - (pos == l)] & (0 - (pos < l)));
    newbranch = (pos < l);
    n = pos << 8;

    if (0) {
_critbit:
        critbit = __msb(p[pos] ^ k[pos]) ^ 0xff;
        val = k[pos];

_newbyte:
        newbranch = (1 + (critbit | val)) >> 8;
        n = (pos << 8) | ((critbit + 1) & 0xff);
    }

    ancestor = parent;

    for (p = *parent; (uintptr_t) p & 0x1; p = *parent) {
        node = (void *) (p - 1);
        /* enforce lexicographic order */
        #if (UINTPTR_MAX > 0xffffffffU)
        if (node->idx > n) break;
        #else
        if ((((uint32_t) node->pos << 8) | ((node->bit + 1) & 0xff)) > n)
            break;
        #endif
        branch = (1 + (node->bit | k[node->pos])) >> 8;
        parent = node->child + branch;
    }

    if (! (node = malloc(sizeof(*node))) ) {
        perror(ERR(trie_insert, malloc));
        return NULL;
    }

    node->pos = pos;
    node->val = val;
    node->bit = critbit;
    #if (UINTPTR_MAX > 0xffffffffU)
    node->idx = n;
    #endif
    node->child[newbranch] = new->key;
    node->child[1 - newbranch] = *parent;

    *parent = (void *) (1 + (char *) node);

    return ancestor;
}

/* -------------------------------------------------------------------------- */

static int _detach(Trie_Leaf *leaf, Variant *out)
{
    /** @brief detach a leaf value, copying data stored inline */

    Variant ret = leaf->val;
    char *copy = NULL;

    if (leaf->own & TRIE_LEAF_INLINE) {
        if (! (copy = malloc(ret.metadata.fields.dword + 1)) ) {
            perror(ERR(trie_remove, malloc));
            return -1;
        }
        memcpy(copy, ret.value.pointer, ret.metadata.fields.dword + 1);
        ret.value.pointer = copy;
        leaf->own = 0;
    }

    *out = ret;

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API int trie_disable_lock(Trie *t)
{
    if (! t) {
        debug("trie_disable_lock(): bad parameters.\n");
        return -1;
    }

    if (lock_wrlock(t->_lock) == -1) return -1;

    lock_break(t->_lock);
    lock_destroy(t->_lock);
    t->_lock = lock_free(t->_lock);

    return 0;
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
        newleaf->len = len; newleaf->own = 0;
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
            if (t->_freeval && ! (list[0]->own & TRIE_LEAF_INLINE))
                t->_freeval(list[0]->val);
            free(list[0]);
            top = & t->_root;
        } else result = 1;

        /* try to find a safe insertion point */
        top = & t->_root;
        if (prefix_len && count > 1) {
            uint8_t *p = NULL;
            void **next = top;
            for (p = *top; (uintptr_t) p & 0x1; p = *next) {
                int branch;
                _Node *node = (void *) (p - 1);
                if (unlikely(node->pos >= prefix_len)) break;
                branch = (
                    1 + (node->bit | (uint8_t) list[1]->key[node->pos])
                ) >> 8;
                top = next; next = node->child + branch;
            }
        }

        for (i = 1; i < count; i ++) {
            if (! _insert(top, (void *) list[i]->key, list[i]->len, list[i])) {
                if (t->_freeval && ! (list[i]->own & TRIE_LEAF_INLINE))
                    t->_freeval(list[i]->val);
                free(list[i]);
            } else result ++;
        }

    lock_unlock(t->_lock);

    return result;
}

/* -------------------------------------------------------------------------- */

static inline Trie_Leaf *_find(void *from, const char *key, size_t len)
{
    const uint8_t * restrict const k = (void *) key;
    uint8_t *p = NULL;
    _Node *node = NULL;
    Trie_Leaf *leaf = NULL;
    unsigned int branch = 0;

    if (! from) return NULL;

    for (p = from; (uintptr_t) p & 0x1; p = node->child[branch]) {
        node = (void *) (p - 1);
        if (likely(node->pos < len))
            branch = (1 + (node->bit | k[node->pos])) >> 8;
        else branch = 0;
    }

    leaf = (Trie_Leaf *) (p - offsetof(Trie_Leaf, key));
    if (leaf->len != len || memcmp(leaf->key, k, len)) leaf = NULL;

    return leaf;
}

/* -------------------------------------------------------------------------- */

ASKL_API Variant trie_lookup(
    Trie *t,
    const char *key,
    size_t len,
    Variant (*function)(Variant)
)
{
    Trie_Leaf *leaf = NULL;
    Variant ret = { 0 };

    if (! t || ! key || ! len) {
        debug("trie_lookup(): bad parameters.\n");
        return ret;
    }

    if (lock_rdlock(t->_lock) == -1) return ret;

        if ( (leaf = _find(t->_root, key, len)) )
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

ASKL_API const Trie_Leaf *trie_lookup_prefix_from(
    Trie *t,
    Trie_Cursor *c,
    const char *prefix,
    size_t len
)
{
    const uint8_t * restrict const k = (void *) prefix;
    uint8_t *p = NULL;
    _Node *node = NULL;
    Trie_Leaf *leaf = NULL;
    _Cursor_Step *path = NULL, *grown = NULL;
    char *key = NULL;
    unsigned int branch = 0;
    void *anchor = NULL;
    size_t d = 0, i = 0, n = 0;

    if (! t || ! k || ! len) {
        debug("trie_lookup_prefix(): bad parameters.\n");
        return NULL;
    }

    if (lock_rdlock(t->_lock) == -1) return NULL;

    p = t->_root;

    if (c) {
        /* the nodes testing the bytes shared with the previous key led
           the same way: the descent resumes below them */
        path = c->_path;
        n = (len < c->_len) ? len : c->_len;
        while (d < n && k[d] == (uint8_t) c->_key[d]) d ++;
        for (i = 0; i < c->_depth && path[i].pos < d; i ++);
        if (i) p = path[i - 1].next;
    }

    anchor = p;

    /* the leaf the prefix leads to is the smallest key sharing it, if any
       key does: the nodes below the last one testing a byte of the prefix
       all test later bytes, so their subtree holds only keys sharing it */
    for (; (uintptr_t) p & 0x1; p = node->child[branch]) {
        node = (void *) (p - 1);
        if (likely(node->pos < len)) {
            branch = (1 + (node->bit | k[node->pos])) >> 8;
            anchor = node->child[branch];
        } else branch = 0;

        if (! c) continue;

        if (i == c->_alloc) {
            grown = realloc(path, (c->_alloc + 64) * sizeof(*path));
            if (! grown) {
                perror(ERR(trie_lookup_prefix_from, realloc));
                goto _err_path;
            }
            path = c->_path = grown; c->_alloc += 64;
        }
        path[i].pos = node->pos; path[i].next = node->child[branch]; i ++;
    }

    if (c) {
        if (c->_size < len) {
            if (! (key = realloc(c->_key, len + 64)) ) {
                perror(ERR(trie_lookup_prefix_from, realloc));
                goto _err_path;
            }
            c->_key = key; c->_size = len + 64;
        }
        memcpy(c->_key, k, len); c->_len = len; c->_depth = i;
        c->anchor = anchor;
    }

    if (likely(p)) {
        leaf = (Trie_Leaf *) (p - offsetof(Trie_Leaf, key));
        if (leaf->len < len || memcmp(leaf->key, k, len)) leaf = NULL;
    }

    lock_unlock(t->_lock);

    return leaf;

_err_path:
    c->_depth = 0; c->_len = 0; c->anchor = NULL;
    lock_unlock(t->_lock);
    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API const Trie_Leaf *trie_lookup_prefix(
    Trie *t,
    const char *prefix,
    size_t len
)
{
    return trie_lookup_prefix_from(t, NULL, prefix, len);
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Cursor *trie_cursor_free(Trie_Cursor *cursor)
{
    if (! cursor) return NULL;

    free(cursor->_path);
    free(cursor->_key);
    memset(cursor, 0, sizeof(*cursor));

    return NULL;
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
        if (_detach(leaf, & ret) == -1) goto _err;
        free(leaf);

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
    Trie_Leaf *leaf = NULL;
    Variant ret = { 0 };

    if (! t || ! key || ! len) {
        debug("trie_update(): bad parameters.\n");
        return ret;
    }

    if (lock_wrlock(t->_lock) == -1) return ret;

        if ( (leaf = _find(t->_root, key, len)) ) {
            if (_detach(leaf, & ret) == 0)
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
            if (t->_freeval && ! (leaf->own & TRIE_LEAF_INLINE))
                t->_freeval(leaf->val);
            free(leaf);
        }

        return ret[0];
    }

    return 0;

_free_node:
    free(node);
    return -(! *top);
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

static _Trie_Iterator *_iterator_alloc(Trie *t)
{
    _Trie_Iterator *it = NULL;

    if (! (it = malloc(sizeof(*it))) ) {
        perror(ERR(trie_each, malloc));
        return NULL;
    }

    it->trie = t;
    it->_stack_count = it->_stack_alloc = 0;
    it->_stack = NULL;
    it->interface.divergence = 0;

    it->interface.key = it->interface.child = NULL;
    it->interface.len = it->interface.child_len = it->_prefix_len = 0;
    it->_next = NULL; it->_next_alloc = 0;
    it->_sep = 0;

    return it;
}

/* -------------------------------------------------------------------------- */

static int _iterator_push(_Trie_Iterator *it, uint8_t *p, uint16_t divergence)
{
    if (it->_stack_count == it->_stack_alloc) {
        _Subtree *stack = NULL;
        uint32_t new_size = it->_stack_alloc + 16;

        if (unlikely(new_size < it->_stack_alloc)) {
            debug("_iterator_push(): integer overflow.\n");
            return -1;
        }

        stack = realloc(it->_stack, new_size * sizeof(*it->_stack));
        if (! stack) {
            perror(ERR(_iterator_push, realloc));
            return -1;
        }

        it->_stack = stack;
        it->_stack_alloc = new_size;
    }

    it->_stack[it->_stack_count].next = p;
    it->_stack[it->_stack_count ++].divergence = divergence;

    return 0;
}

/* -------------------------------------------------------------------------- */

static inline uint8_t *_iterator_pop(_Trie_Iterator *it)
{
    _Subtree subtree;
    if (unlikely(! it->_stack_count)) return NULL;
    subtree = it->_stack[-- it->_stack_count];
    it->interface.divergence = subtree.divergence;
    return subtree.next;
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_each(Trie *t)
{
    _Trie_Iterator *it = NULL;

    if (! t) {
        debug("trie_each(): bad parameters.\n");
        return NULL;
    }

    if (lock_rdlock(t->_lock) == -1) return NULL;

    if (! t->_root) {
        debug("trie_each(): empty trie.\n");
        goto _err;
    }

    if (! (it = _iterator_alloc(t)) ) goto _err;

    if (_iterator_push(it, t->_root, 0) == -1) goto _err_push;

    return trie_next(& it->interface);

_err_push:
    free(it);
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
    _Trie_Iterator *it = NULL;

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

    if (! (it = _iterator_alloc(t)) ) goto _err;

    if (unlikely(_iterator_push(it, top, 0) == -1)) goto _err_push;

    return trie_next(& it->interface);

_err_push:
    free(it);
_err:
    lock_unlock(t->_lock);
    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _seek(_Trie_Iterator *it, void *from, const char *key, size_t len)
{
    /** @brief position the iterator on the first key >= k in a given subtree */

    Trie *t = it->trie;
    const uint8_t * restrict const k = (void *) key;
    uint8_t *p = NULL;
    _Node *node = NULL;
    Trie_Leaf *leaf = NULL;
    unsigned int branch = 0, above = 0;
    uint32_t critbit = 0;
    size_t pos = 0, l = 0;

    if (! from) from = t->_root;
    if (! (p = from) ) return 0;

    it->_stack_count = 0;

    /* every key is >= an empty key */
    if (! len) return _iterator_push(it, p, 0);

    /* follow k to a leaf and find the divergence point */
    while ((uintptr_t) p & 0x1) {
        node = (void *) (p - 1);
        if (likely(node->pos < len))
            branch = (1 + (node->bit | k[node->pos])) >> 8;
        else branch = 0;
        p = node->child[branch];
    }

    leaf = (Trie_Leaf *) (p - offsetof(Trie_Leaf, key));
    l = (len < leaf->len) ? len : leaf->len;
    for (pos = 0; pos < l && k[pos] == (uint8_t) leaf->key[pos]; pos ++);

    if (pos < l) {
        /* encode the first differing bit in the same order as trie nodes */
        critbit = (pos << 8) | (0x100 - __msb(k[pos] ^ leaf->key[pos]));
        above = (k[pos] < (uint8_t) leaf->key[pos]);
    } else if (len != leaf->len) {
        /* when one key prefixes the other, the shorter key comes first */
        critbit = pos << 8;
        above = (len < leaf->len);
    } else {
        /* k exists: the leaf itself is the lower bound */
        critbit = UINT32_MAX; above = 1;
    }

    /* follow k again up to the divergence point to build the traversal
       stack: whenever k takes the left branch, the right subtree contains
       only greater keys and goes on the stack */
    for (p = from; (uintptr_t) p & 0x1; p = node->child[branch]) {
        node = (void *) (p - 1);

        #if (UINTPTR_MAX > 0xffffffffU)
        if (node->idx >= critbit) break;
        #else
        if ((((uint32_t) node->pos << 8) | ((node->bit + 1) & 0xff)) >= critbit)
            break;
        #endif

        if (likely(node->pos < len))
            branch = (1 + (node->bit | k[node->pos])) >> 8;
        else branch = 0;

        if (! branch && _iterator_push(it, node->child[1], 0) == -1)
            return -1;
    }

    /* at the divergence point, keep the side that sorts >= k */
    if (above && _iterator_push(it, p, 0) == -1) return -1;

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_seek(Trie *t, const char *key, size_t len)
{
    _Trie_Iterator *it = NULL;

    if (! t || (! key && len)) {
        debug("trie_seek(): bad parameters.\n");
        return NULL;
    }

    if (lock_rdlock(t->_lock) == -1) return NULL;

    if (! t->_root) {
        debug("trie_seek(): empty trie.\n");
        goto _err;
    }

    if (! (it = _iterator_alloc(t)) ) goto _err;

    if (_seek(it, NULL, key, len) == -1)
        goto _err_seek;

    return trie_next(& it->interface);

_err_seek:
    free(it->_stack);
    free(it);
_err:
    lock_unlock(t->_lock);
    return NULL;
}

/* -------------------------------------------------------------------------- */

static _Trie_Iterator *_child(_Trie_Iterator *it)
{
    /** @brief expose the immediate child in the current key,
               or stop past the prefix */

    const char *end = NULL, *from = it->interface.key + it->_prefix_len;
    size_t left = 0;

    if (it->interface.len < it->_prefix_len ||
        memcmp(it->interface.key, it->_next, it->_prefix_len))
        return (_Trie_Iterator *) trie_break(& it->interface);

    left = it->interface.len - it->_prefix_len;
    end = memchr(from, it->_sep, left);
    it->interface.child = from;
    it->interface.child_len = (end) ? (size_t) (end - from) : left;

    return it;
}

/* -------------------------------------------------------------------------- */

ASKL_API void *trie_anchor(Trie *t, void *from, const char *prefix, size_t len)
{
    /** @brief return the narrowest subtree that can contain a given prefix */

    const uint8_t * restrict const k = (void *) prefix;
    uint8_t *p = NULL, *top = NULL;
    _Node *node = NULL;
    unsigned int branch = 0;

    if (! t || (! prefix && len)) {
        debug("trie_anchor(): bad parameters.\n");
        return NULL;
    }

    if (lock_rdlock(t->_lock) == -1) return NULL;

    for (top = p = (from) ? from : t->_root; (uintptr_t) p & 0x1; p = top) {
        node = (void *) (p - 1);
        if (node->pos >= len) break;
        branch = (1 + (node->bit | k[node->pos])) >> 8;
        top = node->child[branch];
    }

    lock_unlock(t->_lock);

    return top;
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_children(
    Trie *t,
    const char *prefix,
    size_t len,
    char separator
)
{
    return trie_children_from(t, NULL, prefix, len, separator);
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_children_from(
    Trie *t,
    void *from,
    const char *prefix,
    size_t len,
    char separator
)
{
    _Trie_Iterator *it = NULL;

    if (! t || (! prefix && len)) {
        debug("trie_children_from(): bad parameters.\n");
        return NULL;
    }

    if (lock_rdlock(t->_lock) == -1) return NULL;

    if (! t->_root) {
        debug("trie_children(): empty trie.\n");
        goto _err;
    }

    if (! (it = _iterator_alloc(t)) ) goto _err;

    /* keep the prefix in the first len bytes of every scratch seek key */
    it->_next_alloc = len + 2;
    if (! (it->_next = malloc(it->_next_alloc)) ) {
        perror(ERR(trie_children, malloc));
        goto _err_next;
    }
    memcpy(it->_next, prefix, len);
    it->_prefix_len = len;
    it->_sep = separator;

    /* bound subsequent seeks to the given subtree */
    it->_top = (from) ? from : t->_root;
    if (_seek(it, it->_top, prefix, len) == -1)
        goto _err_seek;

    if (! (it = (_Trie_Iterator *) trie_next(& it->interface)) )
        return NULL;

    return (Trie_Iterator *) _child(it);

_err_seek:
    free(it->_next);
_err_next:
    free(it->_stack);
    free(it);
_err:
    lock_unlock(t->_lock);
    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_next_child(Trie_Iterator *iterator)
{
    _Trie_Iterator *it = (_Trie_Iterator *) iterator;
    char *next = NULL;
    const char *prev = NULL;
    size_t n = 0, plen = 0;
    unsigned int step = 0;

    if (! iterator) {
        debug("trie_next_child(): bad parameters.\n");
        return NULL;
    }

    do {
        n = it->_prefix_len + it->interface.child_len;

        /* scan nearby keys before trying to seek past the current child */
        prev = it->interface.child; plen = it->interface.child_len;
        for (step = 0; step < TRIE_CHILD_SCAN; step ++) {
            if (! (it = (_Trie_Iterator *) trie_next(& it->interface)) )
                return NULL;
            if (! (it = _child(it)) )
                return NULL;
            if (it->interface.child_len != plen || memcmp(it->interface.child, prev, plen))
                break;
        }

        if (step < TRIE_CHILD_SCAN) {
            n = it->_prefix_len + it->interface.child_len;
            continue;
        }

        if (it->_next_alloc < n + 2) {
            if (! (next = realloc(it->_next, n + 2)) ) {
                perror(ERR(trie_next_child, realloc));
                return trie_break(& it->interface);
            }
            it->_next = next; it->_next_alloc = n + 2;
        }

        memcpy(it->_next, it->interface.key, n);

        if (it->interface.len == n) {
            /* exact child key: seek just after the key itself */
            it->_next[n ++] = '\0';
        } else {
            /* advance the child + separator prefix past its subtree */
            it->_next[n ++] = it->_sep;
            while (n && (uint8_t) it->_next[n - 1] == 0xff) n --;
            if (! n) return trie_break(& it->interface);
            it->_next[n - 1] ++;
        }

        if (_seek(it, it->_top, it->_next, n) == -1)
            return trie_break(& it->interface);
        if (! (it = (_Trie_Iterator *) trie_next(& it->interface)) )
            return NULL;
        if (! (it = _child(it)) )
            return NULL;

        /* an existing child key was already emitted, skip duplicates */
        n = it->_prefix_len + it->interface.child_len;
    } while (it->interface.len > n && _find(it->_top, it->interface.key, n));

    return & it->interface;
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_next(Trie_Iterator *iterator)
{
    uint8_t *p = NULL;
    _Node *node = NULL;
    Trie_Leaf *leaf = NULL;
    _Trie_Iterator *it = (_Trie_Iterator *) iterator;

    for (p = _iterator_pop(it); (uintptr_t) p & 0x1; p = *node->child) {
        node = (void *) (p - 1);
        if (unlikely(_iterator_push(it, node->child[1], node->pos) == -1))
            return trie_break(& it->interface);
    }

    if (likely(p)) {
        leaf = (Trie_Leaf *) (p - offsetof(Trie_Leaf, key));
        it->interface.key = leaf->key;
        it->interface.len = leaf->len;
        it->interface.val = leaf->val;
        return & it->interface;
    }

    return trie_break(& it->interface);
}

/* -------------------------------------------------------------------------- */

ASKL_API Trie_Iterator *trie_break(Trie_Iterator *iterator)
{
    _Trie_Iterator *it = (_Trie_Iterator *) iterator;

    if (! iterator) {
        debug("trie_break(): bad parameters.\n");
        return NULL;
    }

    lock_unlock(it->trie->_lock);
    free(it->_next);
    free(it->_stack);
    free(it);

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
