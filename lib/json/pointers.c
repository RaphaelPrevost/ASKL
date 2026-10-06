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

/* metadata byte of an indexed value: its parent container is an array */
#define JSON_IN_ARRAY 0x80
#define json_in_array(v) ((v).metadata.fields.byte & JSON_IN_ARRAY)

typedef struct JSON_Pointer {
    char *data;         /* the joined path, components separated by '/' */
    uint32_t len;       /* bytes used */
    uint32_t alloc;     /* bytes allocated */
    uint16_t *start;    /* offset of each component's first byte */
    uint32_t depth;     /* components on the stack */
    uint32_t slots;     /* stack allocated */
} JSON_Pointer;

#define BATCH_MIN 32
#define BATCH_MAX 512

typedef struct Batch {
    struct Batch *next;
    uint32_t capacity;
    uint32_t count;
    uint32_t members;
    uint16_t prefix_len;
    uint16_t indexed;
    Trie_Leaf *leaves[];
} Batch;

typedef struct JSONPath_Context {
    Trie *tree;
    JSON_Pointer *path;
    struct Batch *mempool;
    struct Batch *current;
    Variant root;
    Trie_Leaf *scalar;
    RW_Lock *lock;
    uint32_t sets;
} JSONPath_Context;

/* -------------------------------------------------------------------------- */

static void trie_destructor(Variant v)
{
    if (is_pointer(v) || _is_object(v)) free(v.value.pointer);
}

/* -------------------------------------------------------------------------- */
/* RFC 6901 JSON Pointers                                                     */
/* -------------------------------------------------------------------------- */

static JSON_Pointer *json_pointer_alloc(void)
{
    JSON_Pointer *path = NULL;

    if (! (path = malloc(sizeof(*path))) ) {
        perror(ERR(json_pointer_alloc, malloc));
        goto _err_path;
    }

    path->len = 0; path->alloc = 256;
    path->depth = 0; path->slots = 32;

    if (! (path->data = malloc(path->alloc)) ) {
        perror(ERR(json_pointer_alloc, malloc));
        goto _err_data;
    }

    if (! (path->start = malloc(path->slots * sizeof(*path->start))) ) {
        perror(ERR(json_pointer_alloc, malloc));
        goto _err_start;
    }

    return path;

_err_start:
    free(path->data);
_err_data:
    free(path);
_err_path:
    return NULL;
}

/* -------------------------------------------------------------------------- */

static int json_pointer_extend(JSON_Pointer *path, size_t need)
{
    char *data = NULL;
    size_t size = path->alloc;

    if (likely(need <= size)) return 0;

    while (size < need) size += size;

    if (! (data = realloc(path->data, size)) ) {
        perror(ERR(json_pointer_extend, realloc));
        return -1;
    }

    path->data = data; path->alloc = size;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int json_pointer_push(
    JSON_Pointer *path,
    const char *key,
    size_t len,
    int strict
)
{
    /** @brief append a component: a decoded key, or the index 0 */

    uint16_t *start = NULL;
    size_t off = path->len, i = 0, slots = 2 * path->slots;
    ssize_t ret = 1;
    uint32_t word = 0;
    unsigned int n = 0;
    char *s = NULL, *d = NULL;

    if (unlikely(path->depth == path->slots)) {
        if (! (start = realloc(path->start, slots * sizeof(*start))) ) {
            perror(ERR(json_pointer_push, realloc));
            return -1;
        }
        path->start = start; path->slots = slots;
    }

    if (json_pointer_extend(path, off + 2 * len + 3) == -1)
        return -1;

    path->data[off ++] = '/';

    if (! key) {
        path->data[off] = '0';
        goto _push;
    }

    /* decode into scratch space after the destination, then encode the
       decoded name as an RFC 6901 reference token */
    s = path->data + off + len + 1; d = path->data + off;
    if (unlikely( (ret = json_string(s, key, len, strict)) == -1)) return -1;

    while (i < (size_t) ret) {
        if (likely( (n = ret - i) >= 4)) {
            memcpy(& word, s + i, sizeof(word));
            if (likely(! (__zero(word ^ 0x7e7e7e7eU) |
                          __zero(word ^ 0x2f2f2f2fU)))) {
                memcpy(d, s + i, sizeof(word)); d += 4; i += 4;
                continue;
            }
            n = 4;
        }
        switch (n) {
        case 4: if (s[i] == '~' || s[i] == '/') {
                    d[0] = '~'; d[1] = '0' + (s[i ++] == '/'); d += 2;
                } else *d ++ = s[i ++];
        case 3: if (s[i] == '~' || s[i] == '/') {
                    d[0] = '~'; d[1] = '0' + (s[i ++] == '/'); d += 2;
                } else *d ++ = s[i ++];
        case 2: if (s[i] == '~' || s[i] == '/') {
                    d[0] = '~'; d[1] = '0' + (s[i ++] == '/'); d += 2;
                } else *d ++ = s[i ++];
        case 1: if (s[i] == '~' || s[i] == '/') {
                    d[0] = '~'; d[1] = '0' + (s[i ++] == '/'); d += 2;
                } else *d ++ = s[i ++];
        }
    }

    ret = d - (path->data + off);

_push:
    /* the trie keys and the component offsets are 16-bit */
    if (unlikely(off + ret >= UINT16_MAX)) {
        debug("json_pointer_push(): pointer too long.\n");
        return -1;
    }

    path->start[path->depth ++] = off;
    path->len = off + ret;

    return 0;
}

/* -------------------------------------------------------------------------- */

static void json_pointer_pop(JSON_Pointer *path)
{
    path->len = path->start[-- path->depth] - 1;
}

/* -------------------------------------------------------------------------- */

static int json_pointer_bump(JSON_Pointer *path)
{
    /** @brief increment the final array-index component in place */

    char *s = path->data;
    int i = path->len - 1, first = path->start[path->depth - 1];

    while (i >= first && s[i] == '9') s[i --] = '0';
    if (likely(i >= first)) { s[i] ++; return 0; }

    /* carry out: one more digit, within the 16-bit key length */
    if (unlikely(path->len + 1 >= UINT16_MAX)) {
        debug("json_pointer_bump(): pointer too long.\n");
        return -1;
    }

    if (json_pointer_extend(path, path->len + 1) == -1) return -1;

    s = path->data; s[first] = '1';
    memset(s + first + 1, '0', path->len - first);
    path->len ++;

    return 0;
}

/* -------------------------------------------------------------------------- */

static Variant _json_get(JSONPath_Context *context, const char *ptr, size_t len)
{
    Trie *t = context->tree;
    Trie_Iterator *it = NULL;
    Variant v = { 0 };
    char *key = NULL;
    uint32_t members = 0;
    int kind = 0;

    if (! len) return context->root;

    /* distinguish a stored null from a structural node without an exact leaf */
    v = trie_lookup(t, ptr, len, NULL);

    if (v.metadata.fields.type != VALUE_NULL || trie_has(t, ptr, len))
        return v;

    if (! (key = malloc(len + 2)) ) {
        perror(ERR(_json_get, malloc));
        return v;
    }

    memcpy(key, ptr, len);
    key[len] = '/';
    key[len + 1] = '\0';

    for (it = trie_children(t, key, len + 1, '/'); it; it = trie_next_child(it)) {
        members ++;
        if (! kind && it->len == len + 1 + it->child_len)
            kind = json_in_array(it->val) ? VALUE_ARRAY : VALUE_OBJECT;
    }

    free(key);

    if (kind) {
        v.metadata.fields.type = kind;
        v.metadata.fields.dword = members;
    }

    return v;
}

/* -------------------------------------------------------------------------- */

ASKL_API JSON_Value json_pointer_get(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len
)
{
    JSONPath_Context *context = NULL;
    union {
        Variant val;
        JSON_Value json;
    } ret = { 0 };

    if (! ctx || ! ctx->context || ! ptr) {
        debug("json_pointer_get(): bad parameters.\n");
        return ret.json;
    }

    context = ctx->context;

    if (lock_rdlock(context->lock) == -1)
        return ret.json;

    ret.val = _json_get(context, ptr, len);

    lock_unlock(context->lock);

    return ret.json;
}

/* -------------------------------------------------------------------------- */

static int _json_pointer_set(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    Variant new,
    unsigned int lock
)
{
    JSONPath_Context *context = NULL;
    static int (*const acquire[2])(RW_Lock *) = { lock_wrlock, lock_upgrade };
    static void (*const release[2])(RW_Lock *) = { lock_unlock, lock_restore };
    Trie_Leaf *leaf = NULL;
    Variant old = { 0 }, prev = { 0 };
    int ret = -1;

    if (! ctx || ! ctx->context || ! ptr || lock > 1) {
        debug("_json_pointer_set(): bad parameters.\n");
        return -1;
    }

    context = ctx->context;

    if (acquire[lock](context->lock) == -1)
        return -1;

    if (! len) {
        if (! (leaf = context->scalar))
            goto _container;
        old = leaf->val;
    } else {
        if (! trie_has(context->tree, ptr, len))
            goto _container;

        old = trie_lookup(context->tree, ptr, len, NULL);
        if (is_array(old) || is_object(old))
            goto _container;
    }

    /* preserve the parent marker and member-name offset */
    new.metadata.fields.byte |= old.metadata.fields.byte & JSON_IN_ARRAY;
    new.metadata.fields.word = old.metadata.fields.word;

    if (leaf) {
        if (! (leaf->own & TRIE_LEAF_INLINE))
            trie_destructor(old);

        leaf->own = 0;
        leaf->val = new;
        context->root = new;
    } else {
        prev = trie_update(context->tree, ptr, len, new);

        if (is_pointer(old) && ! is_pointer(prev))
            goto _done;

        trie_destructor(prev);
    }

    context->sets ++;
    ret = 0;

_done:
    release[lock](context->lock);
    return ret;

_container:
    debug("_json_pointer_set(): no scalar at the pointer.\n");
    goto _done;
}

/* -------------------------------------------------------------------------- */

static int _json_set(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    JSON_Value json,
    int mode
)
{
    char *copy = NULL;
    int ret = -1;
    union {
        Variant val;
        JSON_Value json;
    } v = { 0 };

    v.json = json;

    if (json.type == JSON_VALUE_ARRAY || json.type == JSON_VALUE_OBJECT)
        return -1;

    if (json.type == JSON_VALUE_STRING) {
        if (! json.value.pointer) {
            debug("_json_set(): bad value.\n");
            return -1;
        }

        if (! (copy = malloc(json.len + 1)) ) {
            perror(ERR(_json_set, malloc));
            return -1;
        }

        memcpy(copy, json.value.pointer, json.len);
        copy[json.len] = '\0';

        v.val.value.pointer = copy;
        v.val.metadata.fields.byte = (json.len) ? _crc7(copy, json.len) : 0;
    }

    if ( (ret = _json_pointer_set(ctx, ptr, len, v.val, mode)) == -1)
        free(copy);

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    JSON_Value json
)
{
    return _json_set(ctx, ptr, len, json, 0);
}

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set_null(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len
)
{
    Variant v = { 0 };

    v.metadata.fields.type = VALUE_NULL;

    return _json_pointer_set(ctx, ptr, len, v, 0);
}

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set_boolean(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    int value
)
{
    Variant v = { 0 };

    v.metadata.fields.type = VALUE_BOOLEAN;
    v.value.integer = !! value;

    return _json_pointer_set(ctx, ptr, len, v, 0);
}

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set_integer(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    int64_t value
)
{
    Variant v = { 0 };

    v.metadata.fields.type = VALUE_INTEGER;
    v.value.integer = value;

    return _json_pointer_set(ctx, ptr, len, v, 0);
}

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set_decimal(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    double value
)
{
    Variant v = { 0 };

    v.metadata.fields.type = VALUE_DECIMAL;
    v.value.decimal = value;

    return _json_pointer_set(ctx, ptr, len, v, 0);
}

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set_string(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    char *value,
    size_t value_len
)
{
    Variant v = { 0 };

    if (! value) {
        debug("json_pointer_set_string(): bad parameters.\n");
        return -1;
    }

    v.metadata.fields.type = VALUE_POINTER;
    v.metadata.fields.dword = value_len;
    v.metadata.fields.byte = value_len ? _crc7(value, value_len) : 0;
    v.value.pointer = value;

    return _json_pointer_set(ctx, ptr, len, v, 0);
}

/* -------------------------------------------------------------------------- */

static JSON_Pointer *json_pointer_free(JSON_Pointer *path)
{
    if (! path) return NULL;

    free(path->start);
    free(path->data);
    free(path);

    return NULL;
}

/* -------------------------------------------------------------------------- */
/* Batch trie insertions                                                      */
/* -------------------------------------------------------------------------- */

static inline Trie_Leaf *leaf_alloc(const char *key, size_t len, size_t extra)
{
    Trie_Leaf *leaf = NULL;

    if (! (leaf = malloc(sizeof(*leaf) + len + 1 + extra)) ) {
        perror(ERR(leaf_alloc, malloc));
        return NULL;
    }

    memcpy(leaf->key, key, len);
    leaf->key[len] = '\0';
    leaf->len = len;
    leaf->own = 0;

    return leaf;
}

/* -------------------------------------------------------------------------- */

static Trie_Leaf *leaf_free(Trie_Leaf *leaf)
{
    if (! leaf) return NULL;

    if (! (leaf->own & TRIE_LEAF_INLINE)) trie_destructor(leaf->val);
    free(leaf);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int batch_init(JSONPath_Context *context)
{
    Batch *batch = context->mempool;

    if (! batch) {
        if (! (batch = malloc(sizeof(*batch) + (8 * sizeof(Trie_Leaf *)))) ) {
            perror(ERR(batch_init, malloc));
            return -1;
        }
        batch->capacity = 8;
    } else context->mempool = batch->next;

    batch->next = context->current;
    batch->count = 0; batch->members = 0; batch->indexed = 0;

    context->current = batch;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int batch_add(JSONPath_Context *context, Trie_Leaf *leaf, size_t prefix)
{
    Variant val = leaf->val;
    char *copy = NULL;

    if (unlikely(! context->current)) goto _inline;

    if (context->current->count == context->current->capacity) {
        Batch *new = NULL;
        uint32_t new_capacity = 2 * context->current->capacity;
        new = realloc(
            context->current,
            sizeof(*new) + (new_capacity) * sizeof(Trie_Leaf *)
        );
        if (! new) goto _inline;
        new->capacity = new_capacity;
        context->current = new;
    }

    context->current->prefix_len = prefix;
    context->current->leaves[context->current->count ++] = leaf;
    context->current->indexed = 1;

    return 0;

_inline:
    if (leaf->own & TRIE_LEAF_INLINE) {
        /* detach an inline string before freeing the leaf */
        if (! (copy = malloc(val.metadata.fields.dword + 1)) ) {
            perror(ERR(json_data, malloc));
            free(leaf);
            return -1;
        }
        memcpy(copy, val.value.pointer, val.metadata.fields.dword + 1);
        val.value.pointer = copy;
    }

    trie_insert(context->tree, leaf->key, leaf->len, val);

    free(leaf);

    return 0;
}

/* -------------------------------------------------------------------------- */

static void batch_exit(JSONPath_Context *context, int type, int parent_type)
{
    Batch *batch = context->current;
    Batch *parent = batch->next;
    JSON_Pointer *path = context->path;
    size_t child, prefix_len, cost = batch->count;
    unsigned int needed, capacity;

    if (! batch->count) goto _end;

    /* estimate the benefit of merging this batch into its parent from the
       reusable path prefix; for array parents, approximate it with the child
       index length */
    if (parent && path->depth > (type == JSON_ARRAY)) {
        child = path->depth - 1 - (type == JSON_ARRAY);

        if (parent_type == JSON_ARRAY) {
            size_t end = child + 1 < path->depth
                       ? path->start[child + 1] - 1u
                       : path->len;

            cost *= end - path->start[child];
        }

        if (cost < BATCH_MIN) {
            needed = parent->count + batch->count;

            if (needed > BATCH_MAX) {
                trie_insert_prefix_list(
                    context->tree,
                    parent->prefix_len,
                    parent->leaves,
                    parent->count
                );
                parent->count = 0;
                needed = batch->count;
            }

            prefix_len = parent->count
                       ? parent->prefix_len
                       : path->start[child];

            if (prefix_len && needed > parent->capacity) {
                Batch *grown;

                capacity = 2 * parent->capacity;
                if (capacity < needed)
                    capacity = needed;

                grown = realloc(
                    parent,
                    sizeof(*parent) + capacity * sizeof(*parent->leaves)
                );

                if (grown) {
                    grown->capacity = capacity;
                    parent = batch->next = grown;
                } else {
                    prefix_len = 0;
                }
            }

            if (prefix_len) {
                memcpy(
                    parent->leaves + parent->count,
                    batch->leaves,
                    batch->count * sizeof(*batch->leaves)
                );
                parent->count += batch->count;
                parent->prefix_len = prefix_len;
                goto _end;
            }
        }
    }

    trie_insert_prefix_list(
        context->tree,
        batch->prefix_len,
        batch->leaves,
        batch->count
    );

_end:
    context->current = batch->next;
    batch->next = context->mempool;
    context->mempool = batch;
}

/* -------------------------------------------------------------------------- */
/* Parser callbacks                                                           */
/* -------------------------------------------------------------------------- */

static int json_init(int type, JSON_Parser *ctx)
{
    JSONPath_Context *context = ctx->context;

    if (batch_init(context) == -1) return -1;

    if (ctx->key.current) {
        int ret = json_pointer_push(
            context->path,
            ctx->key.current,
            ctx->key.len,
            ctx->strict
        );
        if (ret == -1) return -1;
    }

    if (type == JSON_ARRAY) {
        /* add the index */
        if (json_pointer_push(context->path, NULL, 0, 0) == -1)
            return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int json_data(String *data, JSON_Parser *ctx)
{
    JSONPath_Context *context = ctx->context;
    Trie_Leaf *leaf = NULL;
    Variant var = { 0 };
    double number = 0.0;
    size_t extra = 0, prefix = 0;

    if (IS_PRIMITIVE(data)) {
        switch (ctx->primitive.current.type) {
        case JSON_PRIMITIVE_NUMBER: {
            number = json_number(
                data->data,
                data->len,
                ctx->primitive.current.neg,
                ctx->primitive.current.rad,
                ctx->primitive.current.exp
            );

            if (! isfinite(number)) return -1;

            var = variant_from_decimal(number);
        } break;

        case JSON_PRIMITIVE_NULL: var = variant_null(); break;

        default:
            if (likely(ctx->primitive.current.type & JSON_PRIMITIVE_BOOL)) {
                var = variant_from_boolean(
                    (ctx->primitive.current.type & JSON_PRIMITIVE_TRUE)
                );
            } else if (ctx->primitive.current.type == _JSON_PRIMITIVE_HEX) {
                const uint8_t *hex = (const uint8_t *) data->data;
                uint64_t q = 0;
                unsigned int i = 0;
                for (i = 2; i < data->len; i ++)
                    q = (q << 4) | (9 * (hex[i] >> 6) + (hex[i] & 0xf));
                var = variant_from_integer(q);
            }
        }
    } else if (IS_STRING(data)) extra = data->len + 1;

    if (ctx->parent == JSON_ARRAY) {
        prefix = context->path->start[context->path->depth - 1];
    } else if (ctx->key.current) {
        int ret = json_pointer_push(
            context->path,
            ctx->key.current,
            ctx->key.len,
            ctx->strict
        );
        if (ret == -1) return -1;
        /* fast access to the value name */
        prefix = context->path->start[context->path->depth - 1];
        var.metadata.fields.word = prefix;
    }

    if (! (leaf = leaf_alloc(context->path->data, context->path->len, extra)) )
        return -1;

    if (extra) {
        /* decode the string into the leaf, after its key */
        char *string = leaf->key + context->path->len + 1;
        ssize_t len = json_string(string, data->data, data->len, ctx->strict);
        if (unlikely(len == -1)) {
            free(leaf);
            return -1;
        }
        /* store the string CRC7 for faster retrieval/sorting */
        if (likely(var.metadata.fields.dword = len))
            var.metadata.fields.byte = _crc7(string, len);
        var.metadata.fields.type = VALUE_POINTER;
        var.value.pointer = string;
        leaf->own = TRIE_LEAF_INLINE;
    }

    /* record the parent container type to be able to differentiate
       between array indices and object members later */
    var.metadata.fields.byte |= (ctx->parent == JSON_ARRAY) ? JSON_IN_ARRAY : 0;
    leaf->val = var;

    if (! ctx->parent) {
        /* the document is a scalar */
        context->root = var;
        context->scalar = leaf_free(context->scalar);
        context->scalar = leaf;
        return 1;
    }

    if (batch_add(context, leaf, prefix) == -1) return -1;
    if (likely(context->current)) context->current->members ++;

    if (ctx->parent == JSON_ARRAY) {
        if (json_pointer_bump(context->path) == -1)
            return -1;
    } else json_pointer_pop(context->path);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int json_exit(int type, JSON_Parser *ctx)
{
    Variant var = { 0 };
    JSONPath_Context *context = ctx->context;
    Trie_Leaf *leaf = NULL;
    size_t prefix = 0;
    int explicit = (! context->current->indexed);

    var.metadata.fields.type = (type == JSON_ARRAY) ? VALUE_ARRAY : VALUE_OBJECT;
    var.metadata.fields.byte = (ctx->parent == JSON_ARRAY) ? JSON_IN_ARRAY : 0;
    var.metadata.fields.dword = context->current->members;

    batch_exit(context, type, ctx->parent);

    /* remove the index */
    if (type == JSON_ARRAY) json_pointer_pop(context->path);

    /* a container with an indexed child is represented implicitly.
       otherwise, add an explicit typed leaf storing its member count. */
    if (ctx->parent && explicit) {
        prefix = context->path->start[context->path->depth - 1];
        var.metadata.fields.word = prefix;
        leaf = leaf_alloc(context->path->data, context->path->len, 0);
        if (! leaf) return -1;
        leaf->val = var;
        if (batch_add(context, leaf, prefix) == -1) return -1;
    }

    /* the closing container is a member of its parent */
    if (ctx->parent && likely(context->current)) context->current->members ++;

    /* remove the container name */
    if (ctx->parent == JSON_OBJECT && context->path->depth)
        json_pointer_pop(context->path);

    if (ctx->parent == JSON_ARRAY) {
        if (json_pointer_bump(context->path) == -1)
            return -1;
    }

    if (! ctx->parent) {
        /* the root is known by the context */
        context->root = var;
        context->scalar = leaf_free(context->scalar);
    }

    return (! ctx->parent);
}

/* -------------------------------------------------------------------------- */

static int json_index_init(JSON_Parser *ctx)
{
    JSONPath_Context *private_context = NULL;

    if (! (private_context = malloc(sizeof(*private_context))) ) {
        perror(ERR(json_index_init, malloc));
        goto _err_alloc;
    }

    if (! (private_context->tree = trie_alloc(trie_destructor)) )
        goto _err_trie;

    /* the context serialises its index with a lock of its own */
    trie_disable_lock(private_context->tree);

    if (! (private_context->lock = lock_alloc()) ) goto _err_lock;
    if (lock_init(private_context->lock) == -1) goto _err_init;

    if (! (private_context->path = json_pointer_alloc()) )
        goto _err_path;

    private_context->current = NULL;
    private_context->mempool = NULL;
    private_context->root = variant_null();
    private_context->scalar = NULL;
    private_context->sets = 0;

    ctx->context = private_context;
    ctx->key.current = NULL;
    ctx->key.len = 0;
    ctx->primitive.data = 0;
    ctx->init = json_init;
    ctx->data = json_data;
    ctx->exit = json_exit;

    return 0;

_err_path:
    lock_destroy(private_context->lock);
_err_init:
    lock_free(private_context->lock);
_err_lock:
    trie_free(private_context->tree);
_err_trie:
    free(private_context);
_err_alloc:
    return -1;
}

/* -------------------------------------------------------------------------- */

static int json_index_free(JSON_Parser *ctx)
{
    JSONPath_Context *context = NULL;
    Batch *next = NULL, *batch = NULL;

    ctx->init = NULL;
    ctx->data = NULL;
    ctx->exit = NULL;
    context = ctx->context;
    ctx->context = NULL;

    context->tree = trie_free(context->tree);
    context->path = json_pointer_free(context->path);
    context->scalar = leaf_free(context->scalar);
    lock_destroy(context->lock);
    context->lock = lock_free(context->lock);

    /* destroy pending batches */
    for (batch = context->current; batch; batch = next) {
        unsigned int i = 0;
        next = batch->next;
        for (i = 0; i < batch->count; i ++) leaf_free(batch->leaves[i]);
        free(batch);
    }

    for (batch = context->mempool; batch; batch = next) {
        next = batch->next;
        free(batch);
    }

    free(context);

    return 0;
}

/* -------------------------------------------------------------------------- */

#endif
