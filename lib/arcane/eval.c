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

#ifdef ASKL_JSON_H

/* -------------------------------------------------------------------------- */
/* evaluator                                                                  */
/* -------------------------------------------------------------------------- */

/* the kinds of values an expression handles */
#define V_NOTHING 0
#define V_NULL    1
#define V_BOOL    2
#define V_NUM     3
#define V_STR     4
#define V_NODE    5  /* a container: its pointer is in the value arena */
#define V_LOGICAL 6

/* the kinds of nodes */
#define K_NONE    0
#define K_SCALAR  1
#define K_ARRAY   2
#define K_OBJECT  3

/* the phases of a frame */
#define P_POP     0  /* take the next node off the stack */
#define P_SELECT  1  /* apply the selectors of its segment */
#define P_FILTER  2  /* test its children with a filter */
#define P_EXPR    3  /* evaluate the filter expression on one child */
#define P_DONE    4
#define P_PASS    5  /* run a descendant segment as a pass over the keys */

/* descendant segments scan indexed keys instead of walking nodes.
   set to 0 to use the reference walk. */
#define JSONPATH_PASS 1

/* what a frame collects */
#define M_TOP     0  /* the results of the query */
#define M_EXISTS  1
#define M_COUNT   2
#define M_VALUEOF 3

typedef struct _Value {
    uint8_t kind;
    uint8_t b;
    uint8_t crc;
    uint32_t len;
    uint32_t off;
    union {
        double d;
        const char *s;
    } u;
} _Value;

typedef struct _Entry {
    uint32_t off;
    uint32_t len;
    uint32_t pc;
    uint32_t grp;
    Variant val;
    void *top;
    uint8_t r;
    uint8_t seq;
    uint16_t rank;
    uint32_t parent;
} _Entry;

/* cached metadata for a direct member, followed by its name bytes */
typedef struct _Member {
    uint16_t len;
    uint8_t r;
    uint8_t pad[5];
    Variant val;
} _Member;

typedef struct _Buffer {
    char *data;
    uint32_t len;
    uint32_t size;
} _Buffer;

typedef struct _Frame {
    uint8_t phase;
    uint8_t mode;
    uint8_t kind;
    uint8_t known;
    uint8_t listed;
    uint8_t r;
    uint8_t cr;
    Variant val;
    Variant cval;
    uint32_t table;
    uint32_t base;
    uint32_t nbase;
    uint32_t wbase;
    uint32_t vbase;
    uint32_t sbase;
    uint32_t len;
    uint32_t pc;
    uint32_t sel;
    uint32_t left;
    uint32_t mark;
    uint32_t gbase;
    uint32_t members;
    uint32_t names;
    uint32_t next;
    uint32_t child;
    uint32_t clen;
    uint32_t j;
    uint32_t epc;
    uint32_t found;
    _Value first;
    uint16_t rank;
    uint8_t back;
    uint8_t mr;
    Trie_Iterator *pass;
    uint32_t at;
    uint32_t depth;
    uint32_t k;
    uint32_t mlen;
    uint32_t mmembers;
    uint8_t mkind;
    uint8_t mcounted;
    uint32_t tbase;
    uint32_t ebase;
    Variant mval;
    void *top;
    void *ltop;
} _Frame;

/**
 * @ingroup parser
 * @struct _Frame
 *
 * A frame evaluates one query, either the top-level query or a query nested
 * in a filter expression. Nested queries suspend their enclosing frame.
 * Nodes are processed depth-first from a stack that records the program
 * segment to apply, preserving RFC 9535 result order without materializing
 * a complete nodelist.
 *
 * @b private @ref phase what the frame is doing (P_)
 * @b private @ref mode what it collects (M_)
 * @b private @ref kind the kind of the current node, once known
 * @b private @ref known the kind and number of members of the current
 * node are known
 * @b private @ref listed its members are listed in the work arena
 * @b private @ref r probe result for the current node, 0 if unknown
 * @b private @ref cr probe result for the current child
 * @b private @ref val the value of the current node, when it has one
 * @b private @ref cval value of the current child, when available
 * @b private @ref table the members of an array, by index, in the work arena
 * @b private @ref base the base of the frame in the node stack
 * @b private @ref nbase in the node arena
 * @b private @ref wbase in the work arena: the current node, then the names
 * of its members, then the child under test
 * @b private @ref vbase in the value stack
 * @b private @ref sbase in the value arena
 * @b private @ref len the length of the current node's pointer
 * @b private @ref pc the segment applied to the current node
 * @b private @ref sel the selector being applied
 * @b private @ref left the selectors left in the segment
 * @b private @ref mark the nodes pushed by the segment start here
 * @b private @ref gbase where their pointers start in the node arena
 * @b private @ref members the number of members of the current node
 * @b private @ref names where their names start in the work arena
 * @b private @ref next the next name to read
 * @b private @ref child pointer to the current child in the work arena
 * @b private @ref clen length of the current child's pointer
 * @b private @ref j its rank
 * @b private @ref epc the expression op being evaluated
 * @b private @ref found the number of results
 * @b private @ref first the first result, for value()
 * @b private @ref rank the selector whose results are being pushed
 * @b private @ref back the phase an expression returns to
 * @b private @ref mr probe result for the node currently visited by a pass
 * @b private @ref pass iterator used by the descendant pass
 * @b private @ref at current component of the visited key
 * @b private @ref depth the number of components of the key
 * @b private @ref k the rank of the selector being applied
 * @b private @ref mlen length of the current pass node's pointer
 * @b private @ref tbase the base of the frame in the typed stack
 * @b private @ref ebase the base of the frame in the component ends
 * @b private @ref mval value of the current pass node, when available
 */

typedef struct _Eval {
    Trie *tree;
    JSONPath_Context *ctx;
    uint32_t sets;
    Trie_Cursor cursor;
    Variant root;
    JSONPath_Query *q;
    int (*callback)(const char *, size_t, void *);
    void *arg;
    JSONPath_Iterator *it;
    int total;
    _Buffer nb;
    _Buffer wb;
    _Buffer sb;
    _Entry *entries;
    uint32_t nentries;
    uint32_t aentries;
    _Frame *frames;
    uint32_t nframes;
    uint32_t aframes;
    _Value *values;
    uint32_t nvalues;
    uint32_t avalues;
    uint16_t *typed;
    uint32_t ntyped;
    uint32_t atyped;
    uint32_t *ends;
    uint32_t nends;
    uint32_t aends;
} _Eval;

typedef struct _Iterator {
    JSONPath_Iterator it;
    _Eval ev;
} _Iterator;

/* -------------------------------------------------------------------------- */

static int _room(_Buffer *b, uint32_t need)
{
    return _grow(& b->data, & b->size, b->len + need + 1, 1);
}

/* -------------------------------------------------------------------------- */

static int _append(_Buffer *b, const void *s, uint32_t len)
{
    if (_room(b, len) == -1) return -1;

    memcpy(b->data + b->len, s, len);
    b->len += len;

    return 0;
}

/* -------------------------------------------------------------------------- */

static const char _digits[] =
    "00010203040506070809101112131415161718192021222324"
    "25262728293031323334353637383940414243444546474849"
    "50515253545556575859606162636465666768697071727374"
    "75767778798081828384858687888990919293949596979899";

/* -------------------------------------------------------------------------- */

static char *u32toa(uint32_t u32, char *out)
{
    /** @brief convert a 32-bit unsigned integer to decimal text */

    uint32_t prev = 0;
    const char *d = NULL;
    char *p = out + 10;

    *p = 0;

    while (u32 >= 100) {
        prev = u32; p -= 2; u32 /= 100;
        d = _digits + 2 * (prev - u32 * 100); p[0] = d[0]; p[1] = d[1];
    }

    p -= 2; d = _digits + 2 * u32; p[0] = d[0]; p[1] = d[1];

    return p + (u32 < 10);
}

/* -------------------------------------------------------------------------- */

static int _probe(_Eval *ev, char *key, uint32_t len, Variant *v)
{
    /** @brief classify a JSON Pointer as a value, container, or missing */

    const Trie_Leaf *leaf = NULL;

    if (! len) { *v = ev->root; return 1; }

    /* The first key with this prefix may be the node itself, one of its
       descendants, or an unrelated key that merely extends the same bytes.
       '/' immediately after the prefix proves that the node is a container. */
    if (! (leaf = trie_lookup_prefix_from(ev->tree, & ev->cursor, key, len)) )
        return 0;
    if (leaf->len == len) { *v = leaf->val; return 1; }
    if ((uint8_t) leaf->key[len] >= '/') return (leaf->key[len] == '/') ? 2 : 0;

    key[len] = '/';

    return (
        (trie_lookup_prefix_from(ev->tree, & ev->cursor, key, len + 1)) ? 2 : 0
    );
}

/* -------------------------------------------------------------------------- */

static int _typed(Variant v)
{
    /** @brief the kind of a node */

    return (is_array(v)) ? K_ARRAY : (is_object(v)) ? K_OBJECT : K_SCALAR;
}

/* -------------------------------------------------------------------------- */

static int _walk(_Eval *ev, char *key, uint32_t len, uint32_t *members)
{
    /** @brief determine a container's type, optionally counting its members */

    Trie_Iterator *it = NULL;
    uint32_t n = 0;
    int kind = K_NONE;

    key[len] = '/';
    for (it = trie_children(ev->tree, key, len + 1, '/'); it;
         it = trie_next_child(it)) {
        n ++;
        if (kind == K_NONE && it->len == len + 1 + it->child_len)
            kind = (json_in_array(it->val)) ? K_ARRAY : K_OBJECT;
        if (! members && kind != K_NONE) {
            trie_break(it);
            break;
        }
    }

    if (members) *members = n;

    return kind;
}

/* -------------------------------------------------------------------------- */

static int _kind(_Eval *ev, char *key, uint32_t len, uint32_t *members)
{
    /** @brief the kind of a node, counting its members on demand */

    Variant v = { 0 };
    int kind = K_NONE, r = 0;

    if (! (r = _probe(ev, key, len, & v)) ) return K_NONE;

    if (r == 2) return _walk(ev, key, len, members);

    kind = _typed(v);

    if (members) *members = (kind == K_SCALAR) ? 0 : v.metadata.fields.dword;

    return kind;
}

/* -------------------------------------------------------------------------- */

static int _kind_of(_Eval *ev, _Frame *f)
{
    /** @brief the kind of the current node */

    if (f->known) return f->kind;
    if (! f->r) f->r = _probe(ev, ev->wb.data + f->wbase, f->len, & f->val);
    if (f->r == 1) return _typed(f->val);

    return (f->r) ? _walk(ev, ev->wb.data + f->wbase, f->len, NULL) : K_NONE;
}

/* -------------------------------------------------------------------------- */

static int _children(_Eval *ev, _Frame *f)
{
    /** @brief cache the current node's direct members in the work arena,
        determine its kind and count, and build an index table for arrays */

    char *cur = ev->wb.data + f->wbase;
    Trie_Iterator *it = NULL;
    _Member m;
    uint32_t n = 0, pos = 0, i = 0;

    if (f->listed) return 0;

    f->kind = K_NONE; f->members = 0;
    ev->wb.len = f->names = f->wbase + f->len + 1;

    if (! f->r) f->r = _probe(ev, cur, f->len, & f->val);
    if (f->r == 1) f->kind = _typed(f->val);

    if (f->r && f->kind != K_SCALAR) {
        cur[f->len] = '/';
        /* reuse the parent anchor to locate the current node's subtree */
        f->ltop = trie_anchor(ev->tree, f->top, cur, f->len + 1);
        for (it = trie_children_from(ev->tree, f->ltop, cur, f->len + 1,
                                     '/'); it; it = trie_next_child(it)) {
            memset(& m, 0, sizeof(m));
            m.len = it->child_len;
            if (it->len == f->len + 1 + m.len) { m.r = 1; m.val = it->val; }
            else m.r = 2;
            if (_append(& ev->wb, & m, sizeof(m)) == -1 ||
                _append(& ev->wb, it->child, m.len) == -1) {
                trie_break(it);
                return -1;
            }
            if (f->kind == K_NONE && m.r == 1)
                f->kind = (json_in_array(m.val)) ? K_ARRAY : K_OBJECT;
            n ++;
        }
    }

    f->members = n; f->next = f->names; f->child = f->table = ev->wb.len;
    f->known = f->listed = 1;

    if (f->kind != K_ARRAY || ! n) return 0;

    /* the members of an array come in lexicographic order, list them
       in the order of their indices instead */
    if (_room(& ev->wb, n * sizeof(pos)) == -1) return -1;
    for (pos = f->names; pos < f->table; pos += sizeof(m) + m.len) {
        memcpy(& m, ev->wb.data + pos, sizeof(m));
        for (i = 0, cur = ev->wb.data + pos + sizeof(m); cur < ev->wb.data +
             pos + sizeof(m) + m.len; cur ++) i = i * 10 + (*cur - '0');
        if (i >= n) i = 0;
        memcpy(ev->wb.data + f->table + i * sizeof(pos), & pos, sizeof(pos));
    }
    ev->wb.len += n * sizeof(pos);
    f->child = ev->wb.len;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _push_entry(_Eval *ev, const char *key, uint32_t len, uint32_t pc,
                       uint32_t grp, int r, Variant val, uint16_t rank,
                       int seq, void *top)
{
    _Entry *e = NULL;

    if (_grow(& ev->entries, & ev->aentries, ev->nentries + 1, sizeof(*e)) == -1)
        return -1;

    e = & ev->entries[ev->nentries];
    e->off = ev->nb.len; e->len = len; e->pc = pc; e->grp = grp;
    e->r = r; e->val = val; e->rank = rank; e->seq = seq; e->top = top;
    if (_append(& ev->nb, key, len) == -1) return -1;
    ev->nentries ++;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _push_child(_Eval *ev, _Frame *f, const char *n, uint32_t l,
                       uint32_t pc, int r, Variant val)
{
    /** @brief push a named child of the current node */

    _Entry *e = NULL;
    char *dst = NULL;

    if (_grow(& ev->entries, & ev->aentries, ev->nentries + 1, sizeof(*e)) == -1)
        return -1;
    if (_room(& ev->nb, f->len + 1 + l) == -1) return -1;

    e = & ev->entries[ev->nentries ++];
    e->off = ev->nb.len; e->len = f->len + 1 + l; e->pc = pc;
    e->grp = f->gbase; e->r = r; e->val = val; e->rank = f->rank; e->seq = 0;
    e->top = f->ltop;
    dst = ev->nb.data + ev->nb.len;
    memcpy(dst, ev->wb.data + f->wbase, f->len);
    dst[f->len] = '/';
    memcpy(dst + f->len + 1, n, l);
    ev->nb.len += e->len;

    return 0;
}

/* -------------------------------------------------------------------------- */

static uint32_t _token(_Eval *ev, _Op *op, char *dst)
{
    /** @brief encode a name or index selector as a JSON Pointer token,
        optionally writing it to dst, and return its length */

    char *name = NULL, buf[12];
    uint32_t len = 0;

    if (op->code == OP_INDEX) {
        name = u32toa((uint32_t) op->a, buf); len = buf + 10 - name;
        if (dst) memcpy(dst, name, len);
    } else {
        len = op->len;
        if (dst) memcpy(dst, ev->q->pool + op->a, len);
    }

    return len;
}

/* -------------------------------------------------------------------------- */

static uint32_t _names(_Eval *ev, uint32_t pc, uint32_t *after)
{
    /** @brief return the JSON Pointer length of a simple child-segment run and
        where the run ends */

    _Op *ops = ev->q->ops;
    uint32_t len = 0;

    while (ops[pc].code == OP_SEG && ! (ops[pc].flags & SEG_DESC) &&
           ops[pc].len == 1 && (ops[pc + 1].code == OP_NAME ||
           (ops[pc + 1].code == OP_INDEX && ops[pc + 1].a >= 0 &&
            ops[pc + 1].a <= UINT32_MAX))) {
        len += 1 + _token(ev, & ops[pc + 1], NULL);
        pc = ops[pc].next;
    }

    *after = pc;

    return len;
}

/* -------------------------------------------------------------------------- */

static int _verify(_Eval *ev, char *key, uint32_t len, uint32_t pc,
                   uint32_t after, uint32_t off, int r, Variant var)
{
    /** @brief verify that each index or numeric-name step in a collapsed path
        has the required parent container type */

    _Op *ops = ev->q->ops, *op = NULL;
    Variant pv = { 0 }, nv = { 0 };
    uint32_t end = 0;
    int pr = 0, nr = 0, want = K_NONE, kind = K_NONE;

    for (; pc != after; pc = ops[pc].next, off = end, pr = nr, pv = nv) {
        op = & ops[pc + 1];
        end = off + 1 + _token(ev, op, NULL);
        want = (op->code == OP_INDEX) ? K_ARRAY
             : (op->flags & NAME_INDEX) ? K_OBJECT : K_NONE;
        nr = 0;
        if (want == K_NONE) continue;
        /* an exact child leaf records its parent's container type.
           otherwise, determine the parent type from its own value or
           descendants. */
        if (end == len) { nr = r; nv = var; }
        else nr = _probe(ev, key, end, & nv);
        if (nr == 1) kind = (json_in_array(nv)) ? K_ARRAY : K_OBJECT;
        else {
            if (! pr) pr = _probe(ev, key, off, & pv);
            kind = (pr == 1) ? _typed(pv) : _walk(ev, key, off, NULL);
        }
        if (kind != want) return 0;
    }

    return 1;
}

/* -------------------------------------------------------------------------- */

static int _push_via_run(
    _Eval *ev,
    _Frame *f,
    const char *n,
    uint32_t noff,
    uint32_t l,
    uint32_t pc,
    uint32_t suffix,
    uint32_t after
)
{
    _Op *ops = ev->q->ops;
    Variant var = { 0 };
    char *key = NULL;
    void *top = NULL;
    uint32_t len = 0, off = 0, q = pc;
    int r = 0;

    if (_room(& ev->wb, f->len + 1 + l + suffix + 1) == -1)
        return -1;

    key = ev->wb.data + ev->wb.len;
    if (! n) n = ev->wb.data + noff;

    memcpy(key, ev->wb.data + f->wbase, f->len);
    len = f->len;

    key[len ++] = '/';
    memcpy(key + len, n, l);
    len += l;
    off = len;

    while (q != after) {
        key[len ++] = '/';
        len += _token(ev, & ops[q + 1], key + len);
        q = ops[q].next;
    }

    if (! (r = _probe(ev, key, len, & var)) )
        return 0;

    top = ev->cursor.anchor;

    if (off != len &&
        ! _verify(ev, key, len, pc, after, off, r, var))
        return 0;

    return _push_entry(
        ev, key, len, after, f->gbase, r, var, f->rank, 0, top
    );
}

/* -------------------------------------------------------------------------- */

static int _push_via(
    _Eval *ev,
    _Frame *f,
    const char *n,
    uint32_t noff,
    uint32_t l,
    uint32_t pc
)
{
    uint32_t after = 0;
    uint32_t suffix = _names(ev, pc, & after);

    return _push_via_run(
        ev, f, n, noff, l, pc, suffix, after
    );
}

/* -------------------------------------------------------------------------- */

static int _push_member(_Eval *ev, _Frame *f, uint32_t pos, uint32_t pc)
{
    /** @brief push the member of the current node listed at pos */

    _Member m;
    uint32_t after = 0;
    uint32_t suffix = 0;

    memcpy(& m, ev->wb.data + pos, sizeof(m));

    suffix = _names(ev, pc, & after);
    if (suffix)
        return _push_via_run(
            ev, f, NULL, pos + sizeof(m), m.len,
            pc, suffix, after
        );

    return _push_child(
        ev, f, ev->wb.data + pos + sizeof(m), m.len,
        pc, m.r, m.val
    );
}

/* -------------------------------------------------------------------------- */

static int _count(_Eval *ev, _Frame *f)
{
    /** @brief get the current node's container type and member count from its
        stored value, or by listing its children */

    if (f->known) return 0;

    if (! f->r) f->r = _probe(ev, ev->wb.data + f->wbase, f->len, & f->val);

    if (f->r == 1 && (is_array(f->val) || is_object(f->val))) {
        f->kind = _typed(f->val);
        f->members = f->val.metadata.fields.dword;
        f->names = f->table = f->child = f->wbase + f->len + 1;
        ev->wb.len = f->child;
        f->known = 1;
        ev->wb.data[f->wbase + f->len] = '/';
        f->ltop = trie_anchor(ev->tree, f->top, ev->wb.data + f->wbase, f->len + 1);
        return 0;
    }

    return _children(ev, f);
}

/* -------------------------------------------------------------------------- */

static int _push_index(_Eval *ev, _Frame *f, uint32_t i, uint32_t pc)
{
    /** @brief push a member of the current array: listed, or by index */

    Variant none = { 0 };
    char *name = NULL, buf[12];
    uint32_t pos = 0, len = 0, after = 0, suffix = 0;

    if (! f->listed) {
        name = u32toa(i, buf);
        len = buf + 10 - name;

        suffix = _names(ev, pc, & after);
        if (suffix)
            return _push_via_run(
                ev, f, name, 0, len,
                pc, suffix, after
            );

        return _push_child(ev, f, name, len, pc, 0, none);
    }

    memcpy(& pos, ev->wb.data + f->table + i * sizeof(pos), sizeof(pos));

    return _push_member(ev, f, pos, pc);
}

/* -------------------------------------------------------------------------- */

static uint32_t _member(_Eval *ev, _Frame *f, const char *name, uint32_t len)
{
    /** @brief return the cached position of a named object member,
        or 0 if absent */

    uint32_t pos = f->names;
    _Member m;

    while (pos < f->table) {
        memcpy(& m, ev->wb.data + pos, sizeof(m));
        if (m.len == len && ! memcmp(ev->wb.data + pos + sizeof(m), name, len))
            return pos;
        pos += sizeof(m) + m.len;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _push_names(_Eval *ev, _Frame *f, uint32_t pc)
{
    /** @brief push every member of the current object */

    uint32_t pos = f->names;
    _Member m;

    while (pos < f->table) {
        if (_push_member(ev, f, pos, pc) == -1) return -1;
        memcpy(& m, ev->wb.data + pos, sizeof(m));
        pos += sizeof(m) + m.len;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _push_all(_Eval *ev, _Frame *f, uint32_t pc)
{
    /** @brief push every member of the current node, in order */

    uint32_t i = 0;

    if (_count(ev, f) == -1) return -1;

    if (f->kind == K_OBJECT) {
        if (_children(ev, f) == -1) return -1;
        return _push_names(ev, f, pc);
    }

    if (f->kind == K_ARRAY) {
        for (i = 0; i < f->members; i ++)
            if (_push_index(ev, f, i, pc) == -1) return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static char *_scratch(_Eval *ev, _Frame *f, const char *n, uint32_t l,
                      uint32_t *len)
{
    /** @brief build a child JSON Pointer in the frame's scratch space */

    char *key = NULL;

    if (_room(& ev->wb, f->len + 1 + l + 1) == -1) return NULL;

    key = ev->wb.data + ev->wb.len;
    memcpy(key, ev->wb.data + f->wbase, f->len);
    key[f->len] = '/';
    memcpy(key + f->len + 1, n, l);
    *len = f->len + 1 + l;
    key[*len] = '\0';

    return key;
}

/* -------------------------------------------------------------------------- */

static char *_child_key(_Eval *ev, _Frame *f)
{
    /** @brief select the current child and build its JSON Pointer */

    _Member m;
    char *dst = NULL, *name = NULL, buf[12];
    uint32_t pos = 0;

    memset(& m, 0, sizeof(m));

    if (! f->listed) {
        /* derive the child name directly from its index. */
        name = u32toa(f->j, buf); m.len = buf + 10 - name;
    } else {
        if (f->kind == K_ARRAY)
            memcpy(& pos, ev->wb.data + f->table + f->j * sizeof(pos),
                   sizeof(pos));
        else pos = f->next;
        memcpy(& m, ev->wb.data + pos, sizeof(m));
        f->next = pos + sizeof(m) + m.len;
    }
    f->cr = m.r; f->cval = m.val;

    ev->wb.len = f->child;
    if (_room(& ev->wb, f->len + 1 + m.len + 1) == -1) return NULL;

    dst = ev->wb.data + f->child;
    memcpy(dst, ev->wb.data + f->wbase, f->len);
    dst[f->len] = '/';
    memcpy(dst + f->len + 1, (f->listed) ? ev->wb.data + pos + sizeof(m)
                                          : name, m.len);
    f->clen = f->len + 1 + m.len;
    dst[f->clen] = '\0';
    ev->wb.len = f->child + f->clen + 1;

    return dst;
}

/* -------------------------------------------------------------------------- */

static int _vpush(_Eval *ev, _Value v)
{
    if (_grow(& ev->values, & ev->avalues, ev->nvalues + 1, sizeof(v)) == -1)
        return -1;

    ev->values[ev->nvalues ++] = v;

    return 0;
}

/* -------------------------------------------------------------------------- */

static _Value _vpop(_Eval *ev)
{
    _Value v = ev->values[-- ev->nvalues];

    /* the arena of a node is released with it */
    if (v.kind == V_NODE) ev->sb.len = v.off;

    return v;
}

/* -------------------------------------------------------------------------- */

static int _vnode(_Eval *ev, const char *key, uint32_t len, _Value *v)
{
    /** @brief a container value: its pointer is copied to the value arena */

    memset(v, 0, sizeof(*v));
    v->kind = V_NODE; v->off = ev->sb.len; v->len = len;
    if (_append(& ev->sb, key, len) == -1) return -1;
    ev->sb.data[ev->sb.len ++] = '/';

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _value(_Eval *ev, Variant var, int r, const char *key, uint32_t l,
                  _Value *v)
{
    /** @brief the value of a node, as _probe() found it */

    memset(v, 0, sizeof(*v));

    if (! r) return 0;

    if (r == 2 || var.metadata.fields.type >= VALUE_ARRAY)
        return _vnode(ev, key, l, v);

    switch (var.metadata.fields.type) {
    case VALUE_NULL:
        v->kind = V_NULL;
        break;
    case VALUE_BOOLEAN:
        v->kind = V_BOOL;
        v->b = (var.value.integer != 0);
        break;
    case VALUE_INTEGER:
        v->kind = V_NUM;
        v->u.d = (double) var.value.integer;
        break;
    case VALUE_DECIMAL:
        v->kind = V_NUM;
        v->u.d = var.value.decimal;
        break;
    case VALUE_POINTER:
        v->kind = V_STR;
        v->u.s = var.value.pointer;
        v->len = var.metadata.fields.dword;
        v->crc = var.metadata.fields.byte & 0x7f;
        break;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _singular(_Eval *ev, const char *root, uint32_t rlen, uint32_t pc,
                     uint8_t mode, int rr, Variant rval)
{
    /** @brief evaluate a singular query directly and push its result */

    _Op *op = NULL;
    _Value v;
    Variant var = { 0 };
    char *key = NULL, buf[12];
    const char *n = NULL;
    uint32_t len = 0, l = 0, plen = 0, members = 0;
    int64_t i = 0;
    int r = 0, index = 0, kind = K_NONE;

    memset(& v, 0, sizeof(v));

    /* build the lookup key in scratch space above the active frames */
    key = ev->wb.data + ev->wb.len;
    memcpy(key, root, rlen); len = rlen;

    if (rr == 1 && ev->q->ops[pc].code == OP_SEG && _typed(rval) == K_SCALAR)
        goto _nothing;

    for (op = ev->q->ops + pc; op->code == OP_SEG; op += 2) {
        /* {"0"} and [0] are both represented by /0/, so such ambiguous steps
           must be checked against the parent container type */
        index = (op[1].code == OP_INDEX);
        kind = (index) ? K_ARRAY : (op[1].flags & NAME_INDEX) ? K_OBJECT
             : K_NONE;
        i = op[1].a;
        if (index && i < 0) {
            if (_kind(ev, key, len, & members) != K_ARRAY) goto _nothing;
            if ((i += members) < 0) goto _nothing;
            kind = K_NONE;
        } else if (index && i > UINT32_MAX) goto _nothing;
        else if (kind != K_NONE && op[2].code == OP_SEG &&
                 _kind(ev, key, len, NULL) != kind) goto _nothing;
        if (index) { n = u32toa((uint32_t) i, buf); l = buf + 10 - n; }
        else { n = ev->q->pool + op[1].a; l = op[1].len; }
        if (_room(& ev->wb, len + 1 + l + 1) == -1) return -1;
        key = ev->wb.data + ev->wb.len;
        plen = len;
        key[len ++] = '/';
        memcpy(key + len, n, l); len += l;
    }

    if (len == rlen && rr) { r = rr; var = rval; }
    else r = _probe(ev, key, len, & var);
    if (! r) goto _nothing;
    if (r == 1 && len > rlen && (json_in_array(var) != 0) != index)
        goto _nothing;
    if (r == 2 && kind != K_NONE && _kind(ev, key, plen, NULL) != kind)
        goto _nothing;

    if (_value(ev, var, r, key, len, & v) == -1) return -1;

_nothing:
    if (mode == QUERY_EXISTS || mode == QUERY_COUNT) {
        if (v.kind == V_NODE) ev->sb.len = v.off;
        r = (v.kind != V_NOTHING);
        memset(& v, 0, sizeof(v));
        if (mode == QUERY_EXISTS) { v.kind = V_LOGICAL; v.b = r; }
        else { v.kind = V_NUM; v.u.d = r; }
    }

    return _vpush(ev, v);
}

/* -------------------------------------------------------------------------- */

static int _same(Variant a, Variant b, int flags)
{
    /** @brief equality of two indexed values */

    uint8_t type_a = a.metadata.fields.type;
    uint8_t type_b = b.metadata.fields.type;

    if ((a.metadata.fields.byte ^ b.metadata.fields.byte) & JSON_IN_ARRAY) {
        if (flags) return 0;
    }

    if (type_a != type_b) {
        if (type_a == VALUE_INTEGER && type_b == VALUE_DECIMAL)
            return ((double) a.value.integer == b.value.decimal);
        if (type_a == VALUE_DECIMAL && type_b == VALUE_INTEGER)
            return (a.value.decimal == (double) b.value.integer);
        return 0;
    }

    switch (type_a) {
    case VALUE_NULL:
        return 1;

    case VALUE_BOOLEAN:
    case VALUE_INTEGER:
        return a.value.integer == b.value.integer;

    case VALUE_DECIMAL:
        return a.value.decimal == b.value.decimal;

    case VALUE_ARRAY:
    case VALUE_OBJECT:
        return a.metadata.fields.dword == b.metadata.fields.dword;

    case VALUE_POINTER:
        if ((a.metadata.fields.byte ^ b.metadata.fields.byte) & 0x7f)
            return 0;
        return (
            (a.metadata.fields.dword == b.metadata.fields.dword) &&
            ! memcmp(a.value.pointer, b.value.pointer, a.metadata.fields.dword)
        );

    default: return 0;
    }
}

/* -------------------------------------------------------------------------- */

static int _deep(_Eval *ev, _Value *a, _Value *b)
{
    /** @brief equality of two containers */

    char *p1 = ev->sb.data + a->off, *p2 = ev->sb.data + b->off;
    Variant v1 = { 0 }, v2 = { 0 };
    Trie_Iterator *i1 = NULL, *i2 = NULL;
    uint32_t l1 = a->len, l2 = b->len;
    int r1 = 0, r2 = 0, same = 1;

    r1 = _probe(ev, p1, l1, & v1); r2 = _probe(ev, p2, l2, & v2);
    if (r1 != r2 || (r1 == 1 && ! _same(v1, v2, 0))) return 0;
    if (r1 == 1 && ! v1.metadata.fields.dword) return 1;

    p1[l1] = '/'; p2[l2] = '/';
    i1 = trie_each_prefix(ev->tree, p1, l1 + 1);
    i2 = trie_each_prefix(ev->tree, p2, l2 + 1);

    while (i1 && i2) {
        if (i1->len - l1 != i2->len - l2 ||
            memcmp(i1->key + l1, i2->key + l2, i1->len - l1) ||
            ! _same(i1->val, i2->val, 1)) {
            same = 0;
            break;
        }
        i1 = trie_next(i1); i2 = trie_next(i2);
    }

    if ((i1 != NULL) != (i2 != NULL)) same = 0;
    if (i1) trie_break(i1);
    if (i2) trie_break(i2);

    return same;
}

/* -------------------------------------------------------------------------- */

static int _equal(_Eval *ev, _Value *a, _Value *b)
{
    if (a->kind != b->kind) return 0;

    switch (a->kind) {
    case V_NOTHING:
    case V_NULL: return 1;
    case V_BOOL: return (a->b == b->b);
    case V_NUM: return (a->u.d == b->u.d);
    case V_STR:
        return (a->crc == b->crc && a->len == b->len &&
                ! memcmp(a->u.s, b->u.s, a->len));
    case V_NODE: return _deep(ev, a, b);
    default: return 0;
    }
}

/* -------------------------------------------------------------------------- */

static int _less(_Value *a, _Value *b)
{
    uint32_t n = 0;
    int r = 0;

    if (a->kind != b->kind) return 0;
    if (a->kind == V_NUM) return (a->u.d < b->u.d);
    if (a->kind != V_STR) return 0;

    n = (a->len < b->len) ? a->len : b->len;
    r = memcmp(a->u.s, b->u.s, n);

    return (r < 0 || (! r && a->len < b->len));
}

/* -------------------------------------------------------------------------- */

static int _compare(_Eval *ev, _Value *a, _Value *b, uint8_t code)
{
    switch (code) {
    case OP_EQ: return _equal(ev, a, b);
    case OP_NE: return ! _equal(ev, a, b);
    case OP_LT: return _less(a, b);
    case OP_LE: return _less(a, b) || _equal(ev, a, b);
    case OP_GT: return _less(b, a);
    default: return _less(b, a) || _equal(ev, a, b);
    }
}

/* -------------------------------------------------------------------------- */

static _Value _length(_Eval *ev, _Value *a)
{
    _Value v;
    uint32_t i = 0, n = 0;

    memset(& v, 0, sizeof(v));

    if (a->kind == V_STR) {
        for (i = 0; i < a->len; i ++)
            if (((uint8_t) a->u.s[i] & 0xc0) != 0x80) n ++;
    } else if (a->kind == V_NODE) {
        if (_kind(ev, ev->sb.data + a->off, a->len, & n) == K_NONE) return v;
    } else return v;

    v.kind = V_NUM; v.u.d = n;

    return v;
}

/* -------------------------------------------------------------------------- */

static int _matches(_Eval *ev, _Op *op, _Value *a, _Value *b)
{
    #ifdef HAS_PCRE
    pcre *regex = NULL;
    int r = 0;

    if (a->kind != V_STR || b->kind != V_STR) return 0;

    if (op->a >= 0) regex = ev->q->regex[op->a];
    else regex = _regex(b->u.s, b->len, (op->code == OP_MATCH));
    if (! regex) return 0;

    r = pcre_exec(regex, NULL, a->u.s, a->len, 0, PCRE_NO_UTF8_CHECK, NULL, 0);

    if (op->a < 0) pcre_free(regex);

    return (r >= 0);
    #else
    (void) ev; (void) op; (void) a; (void) b;
    return 0;
    #endif
}

/* -------------------------------------------------------------------------- */

static int _frame(_Eval *ev, uint8_t mode, const char *root, uint32_t len,
                  uint32_t pc)
{
    /** @brief open a frame evaluating a query from the root node */

    _Frame *f = NULL;

    if (_grow(& ev->frames, & ev->aframes, ev->nframes + 1, sizeof(*f)) == -1)
        return -1;

    f = & ev->frames[ev->nframes ++];
    memset(f, 0, sizeof(*f));
    f->mode = mode; f->phase = P_POP;
    f->base = ev->nentries; f->nbase = ev->nb.len; f->wbase = ev->wb.len;
    f->vbase = ev->nvalues; f->sbase = ev->sb.len; f->tbase = ev->ntyped;
    f->ebase = ev->nends;

    return _push_entry(ev, root, len, pc, f->nbase, 0, f->val, 0, 0, NULL);
}

/* -------------------------------------------------------------------------- */

static int _close(_Eval *ev)
{
    /** @brief finish the top frame, restore its saved evaluator state,
        and pass its result to the parent frame */

    _Frame *f = & ev->frames[ev->nframes - 1];
    _Value v;

    memset(& v, 0, sizeof(v));

    switch (f->mode) {
    case M_EXISTS: v.kind = V_LOGICAL; v.b = (f->found > 0); break;
    case M_COUNT: v.kind = V_NUM; v.u.d = f->found; break;
    case M_VALUEOF: if (f->found == 1) v = f->first; break;
    default: break;
    }

    ev->nentries = f->base; ev->nb.len = f->nbase; ev->wb.len = f->wbase;
    ev->nvalues = f->vbase; ev->sb.len = f->sbase;
    if (v.kind == V_NODE) ev->sb.len += v.len + 1;
    if (f->pass) f->pass = trie_break(f->pass);
    ev->ntyped = f->tbase; ev->nends = f->ebase;
    ev->nframes --;

    return (f->mode == M_TOP) ? 0 : _vpush(ev, v);
}

/* -------------------------------------------------------------------------- */

static int _pop(_Eval *ev, _Frame *f, uint32_t *pc)
{
    /** @brief pop the next queued node into the current frame and return its
        program counter */

    _Entry *e = & ev->entries[-- ev->nentries];

    ev->wb.len = f->wbase;
    if (_append(& ev->wb, ev->nb.data + e->off, e->len) == -1) return -1;
    ev->wb.data[ev->wb.len ++] = '\0';
    /* reclaim the storage when the last entry in a sibling group is popped */
    if (ev->nentries == f->base || ev->entries[ev->nentries - 1].grp != e->grp)
        ev->nb.len = e->grp;
    f->len = e->len; f->known = f->listed = 0; f->r = e->r; f->val = e->val;
    f->top = e->top; f->ltop = NULL;
    *pc = e->pc;

    return 0;
}

/* -------------------------------------------------------------------------- */

static void _reverse(_Eval *ev, uint32_t from)
{
    /** @brief reverse the queued results pushed since @b from so they can be
        popped in their original order */

    _Entry tmp, *lo = ev->entries + from, *hi = ev->entries + ev->nentries;

    while (lo < -- hi) {
        tmp = *lo; *lo ++ = *hi; *hi = tmp;
    }
}

/* -------------------------------------------------------------------------- */

static int _natural(const char *a, uint32_t alen, const char *b, uint32_t blen)
{
    /** @brief compare two JSON Pointers in document order, ordering array
        indices numerically rather than lexicographically */

    uint32_t i = 0, j = 0, m = 0, n = 0;
    int r = 0;

    while (i < alen && j < blen) {
        if (a[i] >= '0' && a[i] <= '9' && b[j] >= '0' && b[j] <= '9') {
            for (m = i; m < alen && a[m] >= '0' && a[m] <= '9'; m ++);
            for (n = j; n < blen && b[n] >= '0' && b[n] <= '9'; n ++);
            if (m - i != n - j) return (m - i < n - j) ? -1 : 1;
            if ( (r = memcmp(a + i, b + j, m - i)) ) return (r < 0) ? -1 : 1;
            i = m; j = n;
            continue;
        }
        if (a[i] != b[j]) return ((uint8_t) a[i] < (uint8_t) b[j]) ? -1 : 1;
        i ++; j ++;
    }

    if (alen - i == blen - j) return 0;

    return (alen - i < blen - j) ? -1 : 1;
}

/* -------------------------------------------------------------------------- */

static int _order(const char *nb, const _Entry *x, const _Entry *y)
{
    /** @brief compare two results in RFC document order: parent node first,
        then selector rank, then the selected node when selector order does
        not already determine it */

    const char *a = nb + x->off, *b = nb + y->off;
    int r = 0;

    if ( (r = _natural(a, x->parent, b, y->parent)) )
        return r;

    if (x->rank != y->rank)
        return (x->rank < y->rank) ? -1 : 1;

    if (x->seq) return 0;

    return _natural(a, x->len, b, y->len);
}

/* -------------------------------------------------------------------------- */

static int _sort(_Eval *ev, uint32_t from, int dedupe)
{
    /** @brief put the results pushed since @b from in RFC document order,
        optionally collapsing duplicate nodes produced by a descendant pass */

    _Entry *e = ev->entries + from, *tmp = NULL, *src = NULL, *dst = NULL;
    _Entry *swap = NULL;
    const char *nb = ev->nb.data;
    uint32_t n = ev->nentries - from, width = 0, i = 0, k = 0;
    uint32_t l = 0, r = 0, lend = 0, rend = 0;

    if (n < 2) return 0;

    /* _order() compares the parent first: find it once per entry */
    for (i = 0; i < n; i ++) {
        uint32_t p = e[i].len;
        while (p && nb[e[i].off + p - 1] != '/') p --;
        e[i].parent = p;
    }

    if (! (tmp = malloc(n * sizeof(*tmp))) ) {
        perror(ERR(_sort, malloc));
        return -1;
    }

    /* bottom-up merge sort to correct the order of array indices */
    src = e; dst = tmp;
    for (width = 1; width < n; width *= 2) {
        for (i = 0; i < n; i += 2 * width) {
            l = i; lend = (i + width < n) ? i + width : n;
            r = lend; rend = (i + 2 * width < n) ? i + 2 * width : n;
            k = i;

            while (l < lend && r < rend) {
                if (_order(nb, & src[r], & src[l]) < 0)
                    dst[k ++] = src[r ++];
                else dst[k ++] = src[l ++];
            }

            while (l < lend) dst[k ++] = src[l ++];
            while (r < rend) dst[k ++] = src[r ++];
        }

        swap = src; src = dst; dst = swap;
    }

    if (src != e) memcpy(e, src, n * sizeof(*e));
    free(tmp);

    if (! dedupe) return 0;

    /* deduplicate nodes discovered both as leaves and structural containers */
    for (i = 1, k = 0; i < n; i ++) {
        if (e[i].len == e[k].len &&
            ! memcmp(nb + e[i].off, nb + e[k].off, e[i].len)) continue;
        e[++ k] = e[i];
    }

    ev->nentries = from + k + 1;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _scan(_Eval *ev, _Frame *f)
{
    /** @brief evaluate a descendant name selector directly from indexed leaves */

    _Op *op = & ev->q->ops[f->sel];
    Trie_Iterator *it = NULL;
    Variant none = { 0 };
    const char *name = ev->q->pool + op->a, *key = NULL, *prev = NULL;
    const char *p = NULL, *end = NULL;
    char *cur = ev->wb.data + f->wbase;
    uint32_t nlen = op->len, next = ev->q->ops[f->pc].next;
    uint32_t len = 0, plen = 0, cl = 0, word = 0, from = 0;

    if (f->r == 1 && _typed(f->val) == K_SCALAR) return 0;

    cur[f->len] = '/';
    for (it = trie_each_prefix(ev->tree, cur, f->len + 1); it;
        it = trie_next(it)) {
        key = it->key; len = it->len; end = key + len;

        /* resume at the component containing the first possible divergence */
        from = (it->divergence > f->len) ? it->divergence : f->len;
        if (from > f->len) {
            while (-- from > f->len && key[from] != '/');
        }

        /* emit container paths once, adjacent keys share container prefixes */
        for (p = key + from; (p = memchr(p, '/', end - p)); p ++) {
            if (p + 1 + nlen >= end || p[1 + nlen] != '/' ||
                memcmp(p + 1, name, nlen)) continue;
            cl = (p + 1 + nlen) - key;
            if (prev && plen >= cl && ! memcmp(prev, key, cl) &&
                (plen == cl || prev[cl] == '/')) continue;
            if (_push_entry(ev, key, cl, next, f->gbase, 2, none, 0, 0,
                            NULL) == -1) {
                trie_break(it);
                return -1;
            }
        }

        /* emit the leaf itself when its member name matches */
        word = it->val.metadata.fields.word;
        if (! json_in_array(it->val) && word > f->len && len - word == nlen &&
            ! memcmp(key + word, name, nlen) &&
            _push_entry(ev, key, len, next, f->gbase, 1, it->val, 0, 0,
                        NULL) == -1) {
            trie_break(it);
            return -1;
        }

        prev = key; plen = len;
    }

    return _sort(ev, f->mark, 1);
}

/* -------------------------------------------------------------------------- */

static uint32_t _component(const char *key, uint32_t from, uint32_t len)
{
    /** @brief return the end offset of the JSON Pointer component after @b from */

    const char *sep = memchr(key + from + 1, '/', len - from - 1);

    return (sep) ? (uint32_t) (sep - key) : len;
}

/* -------------------------------------------------------------------------- */

static int _ends(
    _Eval *ev, _Frame *f, const char *key, uint32_t len, uint32_t d, uint32_t *s
)
{
    /** @brief update component ends and count those preceding the divergence */

    uint32_t i = f->len, n = 0;

    while (n < f->depth && ev->ends[f->ebase + n] < d)
        i = ev->ends[f->ebase + n ++];

    *s = n;

    while (i < len) {
        i = _component(key, i, len);
        if (_grow(& ev->ends, & ev->aends, f->ebase + n + 1, sizeof(*ev->ends)) == -1)
            return -1;
        ev->ends[f->ebase + n ++] = i;
    }

    f->depth = n; ev->nends = f->ebase + n;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _typed_push(_Eval *ev, uint16_t len)
{
    if (_grow(& ev->typed, & ev->atyped, ev->ntyped + 1, sizeof(*ev->typed)) == -1)
        return -1;

    ev->typed[ev->ntyped ++] = len;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _size(_Eval *ev, _Frame *f, uint32_t *n)
{
    /** @brief return the current pass node's kind and member count */

    if (f->mr == 1) {
        *n = f->mval.metadata.fields.dword;
        return _typed(f->mval);
    }

    if (f->mcounted) {
        *n = f->mmembers;
        return f->mkind;
    }

    f->mkind = _walk(ev, ev->wb.data + f->child, f->mlen, & f->mmembers);
    f->mcounted = 1;
    *n = f->mmembers;

    return f->mkind;
}

/* -------------------------------------------------------------------------- */

static int _pass_push(_Eval *ev, _Frame *f, const char *n, uint32_t l,
                      uint32_t rank, int want, int plain)
{
    /** @brief push a named child of the current pass node if its parent type
        matches */

    Variant var = { 0 };
    char *key = NULL, *node = NULL;
    uint32_t len = 0, next = ev->q->ops[f->pc].next;
    int r = 0, kind = K_NONE;

    if (_room(& ev->wb, f->mlen + 1 + l + 1) == -1) return -1;
    node = ev->wb.data + f->child;
    key = ev->wb.data + ev->wb.len;
    memcpy(key, node, f->mlen); len = f->mlen;
    key[len ++] = '/';
    memcpy(key + len, n, l); len += l;

    if (! (r = _probe(ev, key, len, & var)) ) return 0;
    if (r == 1) kind = (json_in_array(var)) ? K_ARRAY : K_OBJECT;
    else if (plain) kind = K_OBJECT;
    else if (f->mr == 1) kind = _typed(f->mval);
    else if (f->mkind != K_NONE) kind = f->mkind;
    else kind = f->mkind = _walk(ev, node, f->mlen, NULL);
    if (kind != want) return 0;

    return _push_entry(ev, key, len, next, f->gbase, r, var, rank, 1, NULL);
}

/* -------------------------------------------------------------------------- */

static int _pass(_Eval *ev, _Frame *f)
{
    /** @brief scan the current subtree once, applying the descendant segment's
        selectors to each node */

    _Op *ops = ev->q->ops, *op = NULL;
    Trie_Iterator *it = NULL;
    const char *key = NULL;
    char *cur = ev->wb.data + f->wbase, *node = NULL;
    char *name = NULL, buf[12];
    uint32_t len = 0, n = 0, l = 0, d = 0, shared = 0, next = ops[f->pc].next;
    int64_t i = 0, lo = 0, hi = 0, step = 0, m = 0;
    int container = 0;

    if (f->at > f->depth) {
        /* advance to the next indexed leaf and update its path */
        if (f->pass) it = trie_next(f->pass);
        else {
            cur[f->len] = '/';
            it = trie_each_prefix(ev->tree, cur, f->len + 1);
        }
        if (! (f->pass = it) ) {
            if (_sort(ev, f->mark, 0) == -1) return -1;
            _reverse(ev, f->mark);
            ev->ntyped = f->tbase; ev->nends = f->ebase;
            f->phase = P_POP;
            return 0;
        }
        key = it->key; len = it->len; d = it->divergence;
        if (_ends(ev, f, key, len, d, & shared) == -1) return -1;
        f->at = shared + 1; f->k = 0;
        /* keep typed ancestors whose pointers prefix the current key */
        while (ev->ntyped > f->tbase && ev->typed[ev->ntyped - 1] > d)
            ev->ntyped --;
    }

    node = ev->wb.data + f->child;

    if (! f->k) {
        /* build the descendant node at the current path depth */
        f->sel = f->pc + 1; f->left = ops[f->pc].len;
        f->mkind = K_NONE; f->mcounted = 0;
        if (! f->at) {
            f->mlen = f->len; f->mr = f->r; f->mval = f->val;
            if (! f->mr) f->mr = _probe(ev, cur, f->len, & f->val);
            f->mval = f->val;
            memcpy(node, cur, f->mlen);
        } else {
            key = f->pass->key; len = f->pass->len;
            f->mlen = ev->ends[f->ebase + f->at - 1];
            memcpy(node, key, f->mlen);
            f->mr = 2;
            if (f->at == f->depth) {
                f->mr = 1; f->mval = f->pass->val;
                if (
                    (is_array(f->mval) || is_object(f->mval)) &&
                    _typed_push(ev, len) == -1
                ) return -1;
            } else if (ev->ntyped > f->tbase &&
                       ev->typed[ev->ntyped - 1] == f->mlen) {
                /* this node was already visited through its own leaf. */
                f->at ++;
                return 0;
            }
        }
        node[f->mlen] = '\0';
    }

    container = (f->mr == 2 || is_array(f->mval) || is_object(f->mval));

    while (f->left) {
        op = & ops[f->sel];
        switch (op->code) {
        case OP_NAME:
            if (container && _pass_push(ev, f, ev->q->pool + op->a,
                                        op->len, f->k, K_OBJECT,
                                        ! (op->flags & NAME_INDEX)) == -1)
                return -1;
            /* the work arena may have moved under the lookup */
            node = ev->wb.data + f->child;
            break;

        case OP_INDEX:
            if (! container) break;
            i = op->a;
            if (i < 0) {
                if (_size(ev, f, & n) != K_ARRAY || (i += n) < 0)
                    break;
            } else if (i > UINT32_MAX) break;
            name = u32toa((uint32_t) i, buf); l = buf + 10 - name;
            if (_pass_push(ev, f, name, l, f->k, K_ARRAY, 0) == -1) return -1;
            node = ev->wb.data + f->child;
            break;

        case OP_SLICE:
            if (! container || ! (step = op->x.c) ||
                _size(ev, f, & n) != K_ARRAY) break;
            m = n;
            lo = (op->flags & SLICE_START) ? op->a : (step > 0) ? 0 : m - 1;
            hi = (op->flags & SLICE_END) ? op->b : (step > 0) ? m : -m - 1;
            if (lo < 0) lo += m;
            if (hi < 0) hi += m;
            if (step > 0) {
                lo = (lo < 0) ? 0 : (lo > m) ? m : lo;
                hi = (hi < 0) ? 0 : (hi > m) ? m : hi;
            } else {
                lo = (lo < -1) ? -1 : (lo > m - 1) ? m - 1 : lo;
                hi = (hi < -1) ? -1 : (hi > m - 1) ? m - 1 : hi;
            }
            for (i = lo; (step > 0) ? i < hi : i > hi; i += step) {
                name = u32toa((uint32_t) i, buf); l = buf + 10 - name;
                if (_pass_push(ev, f, name, l, f->k, K_ARRAY, 0) == -1)
                    return -1;
            }
            node = ev->wb.data + f->child;
            break;

        case OP_WILD:
            if (f->at && _push_entry(ev, node, f->mlen, next, f->gbase,
                                     f->mr, f->mval, f->k, 0, NULL) == -1)
                return -1;
            break;

        default:
            /* evaluate the filter against this node, then resume the pass */
            if (! f->at) break;
            f->clen = f->mlen; f->cr = f->mr; f->cval = f->mval;
            f->rank = f->k; f->epc = f->sel + 1; f->back = P_PASS;
            f->sel = op->next; f->left --; f->k ++;
            f->phase = P_EXPR;
            return 0;
        }
        f->sel = (op->code == OP_FILTER) ? op->next : f->sel + 1;
        f->left --; f->k ++;
    }

    f->at ++; f->k = 0;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _sink(_Eval *ev, _Frame *f)
{
    /** @brief emit a completed node, or accumulate it for a nested query */

    union {
        Variant val;
        JSON_Value json;
    } v = { 0 };
    Variant val = { 0 };
    char *cur = ev->wb.data + f->wbase;
    uint32_t members = 0;
    int kind = K_NONE, r = 0;

    if (f->mode == M_TOP) {
        ev->total ++;

        if (! ev->it) {
            cur[f->len] = '\0';
            return (ev->callback(cur, f->len, ev->arg)) ? 1 : 0;
        }

        /* a lazy evaluation yields the node to its iterator */
        if (! f->len) {
            v.val = ev->ctx->root;

        } else if (f->r == 1) {
            /* replacement never changes structure */
            v.val = (ev->sets == ev->ctx->sets)
                  ? f->val
                  : trie_lookup(ev->tree, cur, f->len, NULL);
        } else if (f->r == 2) {
            /* get the kind and cardinality of the container */
            kind = _walk(ev, cur, f->len, & members);
            if (kind == K_NONE) return -1;

            v.val.metadata.fields.type =
                (kind == K_ARRAY) ? VALUE_ARRAY : VALUE_OBJECT;
            v.val.metadata.fields.dword = members;

        } else {
            /* the entry was pushed without probing the node */
            r = _probe(ev, cur, f->len, & val);

            if (r == 1) {
                v.val = val;
            } else if (r == 2) {
                kind = _walk(ev, cur, f->len, & members);
                if (kind == K_NONE) return -1;

                v.val.metadata.fields.type =
                    (kind == K_ARRAY) ? VALUE_ARRAY : VALUE_OBJECT;
                v.val.metadata.fields.dword = members;
            } else {
                return -1;
            }
        }

        cur[f->len] = '\0';

        ev->it->key = cur;
        ev->it->len = f->len;
        ev->it->val = v.json;

        return 1;
    }

    f->found ++;

    if (f->mode == M_EXISTS || (f->mode == M_VALUEOF && f->found > 1)) {
        f->phase = P_DONE;
        return 0;
    }

    if (f->mode != M_VALUEOF) return 0;

    if (! f->r) f->r = _probe(ev, cur, f->len, & f->val);

    return _value(ev, f->val, f->r, cur, f->len, & f->first);
}

/* -------------------------------------------------------------------------- */

static int _select(_Eval *ev, _Frame *f)
{
    /** @brief apply the selector at hand to the current node */

    _Op *op = & ev->q->ops[f->sel];
    Variant var = { 0 };
    char *key = NULL, *name = NULL, buf[12];
    void *top = NULL;
    uint32_t next = ev->q->ops[f->pc].next, len = 0, l = 0, pos = 0;
    int64_t i = 0, lo = 0, hi = 0, step = 0, n = 0;
    int r = 0;

    switch (op->code) {
    case OP_NAME:
        if (f->listed) {
            /* the members are listed: no need to look the name up */
            if (f->kind == K_OBJECT &&
                (pos = _member(ev, f, ev->q->pool + op->a, op->len)) &&
                _push_member(ev, f, pos, next) == -1) return -1;
            break;
        }
        if (f->known && f->kind != K_OBJECT) break;
        if (! (op->flags & NAME_INDEX)) {
            /* collapse a run of simple child selectors into one lookup */
            if (_push_via(ev, f, ev->q->pool + op->a, 0, op->len, next) == -1)
                return -1;
            break;
        }
        key = _scratch(ev, f, ev->q->pool + op->a, op->len, & len);
        if (! key) return -1;
        r = _probe(ev, key, len, & var);
        top = ev->cursor.anchor;
        if (r == 1 && ! json_in_array(var)) goto _push;
        if (r == 2 && _kind_of(ev, f) == K_OBJECT) goto _push;
        break;

    case OP_INDEX:
        i = op->a;
        if (i < 0 || f->known || f->r == 1) {
            if (_count(ev, f) == -1) return -1;
            if (f->kind != K_ARRAY || (i < 0 && (i += f->members) < 0)) break;
            if (i < f->members && _push_index(ev, f, (uint32_t) i, next) == -1)
                return -1;
            break;
        } else if (i > UINT32_MAX) break;
        name = u32toa((uint32_t) i, buf); l = buf + 10 - name;
        if (! (key = _scratch(ev, f, name, l, & len)) ) return -1;
        r = _probe(ev, key, len, & var);
        top = ev->cursor.anchor;
        if (r == 1 && json_in_array(var)) goto _push;
        if (r == 2 && _kind_of(ev, f) == K_ARRAY) goto _push;
        break;

    case OP_WILD:
        if (_push_all(ev, f, next) == -1) return -1;
        break;

    case OP_SLICE:
        if (_count(ev, f) == -1) return -1;
        if (f->kind != K_ARRAY || ! (step = op->x.c)) break;
        n = f->members;
        lo = (op->flags & SLICE_START) ? op->a : (step > 0) ? 0 : n - 1;
        hi = (op->flags & SLICE_END) ? op->b : (step > 0) ? n : -n - 1;
        if (lo < 0) lo += n;
        if (hi < 0) hi += n;
        if (step > 0) {
            lo = (lo < 0) ? 0 : (lo > n) ? n : lo;
            hi = (hi < 0) ? 0 : (hi > n) ? n : hi;
            for (i = lo; i < hi; i += step)
                if (_push_index(ev, f, (uint32_t) i, next) == -1) return -1;
        } else {
            lo = (lo < -1) ? -1 : (lo > n - 1) ? n - 1 : lo;
            hi = (hi < -1) ? -1 : (hi > n - 1) ? n - 1 : hi;
            for (i = lo; i > hi; i += step)
                if (_push_index(ev, f, (uint32_t) i, next) == -1) return -1;
        }
        break;

    default:
        /* a filter: the children are tested one by one, the members of an
           object by name */
        if (_count(ev, f) == -1) return -1;
        if (f->kind == K_OBJECT && _children(ev, f) == -1) return -1;
        f->j = 0; f->next = f->names; f->phase = P_FILTER;
        return 0;
    }

    f->sel = (op->code == OP_FILTER) ? op->next : f->sel + 1;
    f->left --;

    return 0;

_push:
    if (_push_entry(ev, key, len, next, f->gbase, r, var, f->rank, 0, top) == -1)
        return -1;
    f->sel ++; f->left --;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _filter(_Eval *ev, _Frame *f)
{
    /** @brief test the next child, or end the filter */

    _Op *op = & ev->q->ops[f->sel];

    if (f->j == f->members || f->kind == K_SCALAR) {
        f->sel = op->next; f->left --; f->phase = P_SELECT;
        return 0;
    }

    if (! _child_key(ev, f)) return -1;
    f->epc = f->sel + 1; f->back = P_FILTER; f->phase = P_EXPR;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _evaluate(_Eval *ev, _Frame *f)
{
    /** @brief run the filter expression, until a nested query needs a frame */

    _Op *op = NULL;
    _Value a, b;
    const char *root = NULL;
    uint32_t rlen = 0, pc = 0, next = ev->q->ops[f->pc].next;
    uint8_t mode = 0;

    memset(& a, 0, sizeof(a)); memset(& b, 0, sizeof(b));

    while (1) {
        op = & ev->q->ops[f->epc];

        switch (op->code) {
        case OP_END:
            a = _vpop(ev);
            if (a.b && _push_entry(ev, ev->wb.data + f->child, f->clen, next,
                                   f->gbase, f->cr, f->cval, f->rank, 0,
                                   f->ltop) == -1)
                return -1;
            if (f->back == P_FILTER) f->j ++;
            f->phase = f->back;
            return 0;

        case OP_LIT:
            switch (op->flags) {
            case LIT_NULL: a.kind = V_NULL; break;
            case LIT_FALSE: a.kind = V_BOOL; a.b = 0; break;
            case LIT_TRUE: a.kind = V_BOOL; a.b = 1; break;
            case LIT_NUM: a.kind = V_NUM; a.u.d = op->x.d; break;
            default:
                a.kind = V_STR; a.u.s = ev->q->pool + op->a; a.len = op->len;
                a.crc = op->b;
            }
            if (_vpush(ev, a) == -1) return -1;
            break;

        case OP_QUERY:
            mode = op->flags & QUERY_MODE;
            pc = (uint32_t) (op - ev->q->ops) + 1;
            f->epc = op->next;
            if (_room(& ev->wb, f->clen + 1) == -1) return -1;
            root = (op->flags & QUERY_ABS) ? "" : ev->wb.data + f->child;
            rlen = (op->flags & QUERY_ABS) ? 0 : f->clen;
            if (op->flags & QUERY_SINGULAR) {
                if (_singular(ev, root, rlen, pc, mode,
                              (op->flags & QUERY_ABS) ? 0 : f->cr,
                              f->cval) == -1) return -1;
                continue;
            }
            /* suspend this frame while the nested query runs */
            mode = (mode == QUERY_COUNT) ? M_COUNT
                 : (mode == QUERY_VALUEOF) ? M_VALUEOF : M_EXISTS;
            return _frame(ev, mode, root, rlen, pc);

        case OP_NOT:
            a = _vpop(ev); a.b = ! a.b;
            if (_vpush(ev, a) == -1) return -1;
            break;

        case OP_AND:
        case OP_OR:
            b = _vpop(ev); a = _vpop(ev);
            a.b = (op->code == OP_AND) ? (a.b && b.b) : (a.b || b.b);
            if (_vpush(ev, a) == -1) return -1;
            break;

        case OP_LENGTH:
            a = _vpop(ev);
            if (_vpush(ev, _length(ev, & a)) == -1) return -1;
            break;

        case OP_MATCH:
        case OP_SEARCH:
            b = _vpop(ev); a = _vpop(ev);
            a.b = _matches(ev, op, & a, & b); a.kind = V_LOGICAL;
            if (_vpush(ev, a) == -1) return -1;
            break;

        default:
            /* a comparison */
            b = _vpop(ev); a = _vpop(ev);
            a.b = _compare(ev, & a, & b, op->code); a.kind = V_LOGICAL;
            if (_vpush(ev, a) == -1) return -1;
        }

        f->epc ++;
    }
}

/* -------------------------------------------------------------------------- */

static int _run(_Eval *ev)
{
    _Frame *f = NULL;
    _Op *ops = ev->q->ops;
    uint32_t pc = 0;
    int r = 0;

    while (ev->nframes) {
        f = & ev->frames[ev->nframes - 1];

        switch (f->phase) {
        case P_POP:
            if (ev->nentries == f->base) {
                f->phase = P_DONE;
                break;
            }
            if (_pop(ev, f, & pc) == -1) return -1;
            if (ops[pc].code == OP_END) {
                if ( (r = _sink(ev, f)) ) return r;
            } else {
                f->pc = pc; f->sel = pc + 1; f->left = ops[pc].len;
                f->mark = ev->nentries; f->gbase = ev->nb.len;
                f->phase = P_SELECT;
                if (! (ops[pc].flags & SEG_DESC)) break;
                if (ops[pc].len == 1 && ops[pc + 1].code == OP_NAME &&
                    ! (ops[pc + 1].flags & NAME_INDEX)) {
                    /* resolve a descendant name selector directly from
                       indexed leaves */
                    if (_scan(ev, f) == -1) return -1;
                    _reverse(ev, f->mark);
                    f->phase = P_POP;
                    break;
                }
                #if JSONPATH_PASS
                if (! (ops[pc].flags & SEG_FILTER) ) {
                    if (_count(ev, f) == -1) return -1;
                    break;
                }
                /* visit the current node before scanning descendant keys */
                f->pass = NULL; f->at = 0; f->depth = 0; f->k = 0;
                f->tbase = ev->ntyped;
                ev->wb.len = f->wbase + f->len + 1;
                if (_room(& ev->wb, UINT16_MAX + 2) == -1) return -1;
                f->child = ev->wb.len; ev->wb.len += UINT16_MAX + 2;
                f->phase = P_PASS;
                break;
                #endif
                /* the reference walk needs the member list before
                   applying descendant selectors */
                if (_count(ev, f) == -1) return -1;
            }
            break;

        case P_SELECT:
            if (f->left) {
                if (_select(ev, f) == -1) return -1;
            } else {
                if ((ops[f->pc].flags & SEG_DESC) &&
                    _push_all(ev, f, f->pc) == -1) return -1;
                _reverse(ev, f->mark);
                f->phase = P_POP;
            }
            break;

        case P_FILTER:
            if (_filter(ev, f) == -1) return -1;
            break;

        case P_EXPR:
            if (_evaluate(ev, f) == -1) return -1;
            break;

        case P_PASS:
            if (_pass(ev, f) == -1) return -1;
            break;

        default:
            if (_close(ev) == -1) return -1;
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static void _release(_Eval *ev)
{
    /** @brief drop the frames and the arenas of an evaluation */

    while (ev->nframes) _close(ev);
    trie_cursor_free(& ev->cursor);
    free(ev->typed); free(ev->ends);
    free(ev->nb.data); free(ev->wb.data); free(ev->sb.data);
    free(ev->entries); free(ev->frames); free(ev->values);
}

/* -------------------------------------------------------------------------- */

#endif
