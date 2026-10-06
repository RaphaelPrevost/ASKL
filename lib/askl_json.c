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

#include "askl_json.h"

/* -------------------------------------------------------------------------- */
#if (defined(_ENABLE_JSON) && defined(_ENABLE_TRIE))
/* -------------------------------------------------------------------------- */

STATIC_ASSERT(
    offsetof(JSON_Value, value) == offsetof(Variant, value), value
);
STATIC_ASSERT(
    offsetof(JSON_Value, type) == offsetof(Variant, metadata.fields.type), type
);
STATIC_ASSERT(
    offsetof(JSON_Value, len) == offsetof(Variant, metadata.fields.dword), len
);

#include "arcane/bitops.c"
#include "arcane/json.c"
#include "json/pointers.c"
#include "json/compiler.c"
#include "arcane/eval.c"

/* -------------------------------------------------------------------------- */
/* RFC 9535 JSONPath                                                          */
/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_init(JSON_Parser *ctx)
{
    if (! ctx) {
        debug("jsonpath_init(): bad parameters.\n");
        return -1;
    }

    return json_index_init(ctx);
}

/* -------------------------------------------------------------------------- */

static int trie_print(const char *k, size_t len, Variant v)
{
    printf("%.*s = ", (int) len, k);

    switch (v.metadata.fields.type) {
    case VALUE_NULL: printf("<NULL>\n"); break;
    case VALUE_STRING: break;
    case VALUE_INTEGER:
        printf("0x%llx\n", (unsigned long long) variant_to_integer(v)); break;
    case VALUE_BOOLEAN:
        printf("%s\n", (variant_to_boolean(v)) ? "TRUE" : "FALSE"); break;
    case VALUE_DECIMAL: printf("%02f\n", variant_to_decimal(v)); break;
    case VALUE_POINTER: printf("0x%p\n", variant_to_pointer(v)); break;
    default: /* _VALUE_OBJECT */
    printf("%.*s\n", v.metadata.fields.dword, (char *) v.value.pointer);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_print(JSON_Parser *ctx)
{
    JSONPath_Context *context = ctx->context;

    if (lock_rdlock(context->lock) == -1) return -1;
    trie_foreach(context->tree, trie_print);
    lock_unlock(context->lock);

    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API JSONPath_Query *jsonpath_query_alloc(const char *expr, size_t len)
{
    if (! expr) {
        debug("jsonpath_query_alloc(): bad parameters.\n");
        return NULL;
    }

    return _compile(expr, len);
}

/* -------------------------------------------------------------------------- */

ASKL_API JSONPath_Query *jsonpath_query_free(JSONPath_Query *q)
{
    #ifdef HAS_PCRE
    uint32_t i = 0;
    #endif

    if (! q) return NULL;

    #ifdef HAS_PCRE
    for (i = 0; i < q->nregex; i ++) pcre_free(q->regex[i]);
    free(q->regex);
    #endif
    free(q->ops); free(q->pool); free(q);

    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_foreach(
    JSON_Parser *ctx,
    JSONPath_Query *query,
    int (*callback)(const char *ptr, size_t len, void *arg),
    void *arg
)
{
    JSONPath_Context *context = NULL;
    _Eval ev;
    int r = 0;

    if (! ctx || ! ctx->context || ! query || ! callback) {
        debug("jsonpath_foreach(): bad parameters.\n");
        return -1;
    }

    context = ctx->context;
    memset(& ev, 0, sizeof(ev));
    ev.tree = context->tree; ev.root = context->root; ev.ctx = context;
    ev.q = query; ev.callback = callback; ev.arg = arg;

    /* the index is read-locked as long as the query runs */
    if (lock_rdlock(context->lock) == -1) return -1;
    ev.sets = context->sets;

    if (_frame(& ev, M_TOP, "", 0, 0) == 0) r = _run(& ev);
    else r = -1;

    _release(& ev);
    lock_unlock(context->lock);

    return (r == -1) ? -1 : ev.total;
}

/* -------------------------------------------------------------------------- */

ASKL_API JSONPath_Iterator *jsonpath_each(JSON_Parser *ctx, JSONPath_Query *q)
{
    JSONPath_Context *context = NULL;
    _Iterator *it = NULL;

    if (! ctx || ! ctx->context || ! q) {
        debug("jsonpath_each(): bad parameters.\n");
        return NULL;
    }

    context = ctx->context;

    if (! (it = calloc(1, sizeof(*it))) ) {
        perror(ERR(jsonpath_each, calloc));
        goto _err_it;
    }

    it->ev.tree = context->tree; it->ev.root = context->root;
    it->ev.q = q; it->ev.it = & it->it; it->ev.ctx = context;

    /* the index is read-locked as long as the iterator lives */
    if (lock_rdlock(context->lock) == -1) goto _err_lock;
    it->ev.sets = context->sets;

    if (_frame(& it->ev, M_TOP, "", 0, 0) == -1) goto _err_frame;

    return & it->it;

_err_frame:
    _release(& it->ev);
    lock_unlock(context->lock);
_err_lock:
    free(it);
_err_it:
    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API JSONPath_Iterator *jsonpath_next(JSONPath_Iterator *iterator)
{
    _Iterator *it = (_Iterator *) iterator;

    if (! it) {
        debug("jsonpath_next(): bad parameters.\n");
        return NULL;
    }

    if (_run(& it->ev) == 1) return iterator;

    return jsonpath_break(iterator);
}

/* -------------------------------------------------------------------------- */

ASKL_API JSONPath_Iterator *jsonpath_break(JSONPath_Iterator *iterator)
{
    _Iterator *it = (_Iterator *) iterator;

    if (! it) return NULL;

    _release(& it->ev);
    lock_unlock(it->ev.ctx->lock);
    free(it);

    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API JSON_Value jsonpath_get(
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
        debug("jsonpath_get(): bad parameters.\n");
        return ret.json;
    }

    context = ctx->context;
    ret.val = _json_get(context, ptr, len);

    return ret.json;
}

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    JSON_Value json
)
{
    return _json_set(ctx, ptr, len, json, 1);
}

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set_null(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len
)
{
    Variant v = { 0 };

    v.metadata.fields.type = VALUE_NULL;

    return _json_pointer_set(ctx, ptr, len, v, 1);
}

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set_boolean(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    int value
)
{
    Variant v = { 0 };

    v.metadata.fields.type = VALUE_BOOLEAN;
    v.value.integer = !! value;

    return _json_pointer_set(ctx, ptr, len, v, 1);
}

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set_integer(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    int64_t value
)
{
    Variant v = { 0 };

    v.metadata.fields.type = VALUE_INTEGER;
    v.value.integer = value;

    return _json_pointer_set(ctx, ptr, len, v, 1);
}

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set_decimal(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    double value
)
{
    Variant v = { 0 };

    v.metadata.fields.type = VALUE_DECIMAL;
    v.value.decimal = value;

    return _json_pointer_set(ctx, ptr, len, v, 1);
}

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set_string(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    char *value,
    size_t value_len
)
{
    Variant v = { 0 };

    if (! value) {
        debug("jsonpath_set_string(): bad parameters.\n");
        return -1;
    }

    v.metadata.fields.type = VALUE_POINTER;
    v.metadata.fields.dword = value_len;
    v.metadata.fields.byte = value_len ? _crc7(value, value_len) : 0;
    v.value.pointer = value;

    return _json_pointer_set(ctx, ptr, len, v, 1);
}

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_free(JSON_Parser *ctx)
{
    if (! ctx) {
        debug("jsonpath_free(): bad parameters.\n");
        return -1;
    }

    return json_index_free(ctx);
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif /* _ENABLE_JSON && _ENABLE_TRIE */
/* -------------------------------------------------------------------------- */
