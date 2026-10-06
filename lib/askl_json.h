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

#ifndef ASKL_JSON_H

#define ASKL_JSON_H

/* -------------------------------------------------------------------------- */
#if (defined(_ENABLE_JSON) && defined(_ENABLE_TRIE))
/* -------------------------------------------------------------------------- */

#include "string/parser.h"
#include "askl_cbtrie.h"

/** @defgroup json ASKL::json */

typedef enum JSON_Value_Type {
    JSON_VALUE_NULL    = 0,
    JSON_VALUE_INTEGER = 2,
    JSON_VALUE_BOOLEAN = 3,
    JSON_VALUE_DECIMAL = 4,
    JSON_VALUE_STRING  = 5,
    JSON_VALUE_ARRAY   = 6,
    JSON_VALUE_OBJECT  = 7
} JSON_Value_Type;

/**
 * @ingroup json
 * @enum JSON_Value_Type
 *
 * The type of a @ref JSON_Value.
 */

typedef struct JSON_Value {
    union {
        void *pointer;
        uint64_t integer;
        double decimal;
    } value;
    uint8_t type;
    uint8_t _reserved[3];
    uint32_t len;
} JSON_Value;

/**
 * @ingroup json
 * @struct JSON_Value
 *
 * A JSON value returned by the JSON Pointer and JSONPath APIs.
 *
 * @ref type determines which member of @ref value is meaningful. For strings,
 * @ref value.pointer contains the string data and @ref len its length in bytes.
 * For arrays and objects, @ref len is the number of direct members. The
 * @ref _reserved field is internal and must not be modified or inspected.
 */

typedef struct _JSONPath_Query JSONPath_Query;

/**
 * @ingroup json
 * @struct JSONPath_Query
 *
 * A compiled RFC 9535 JSONPath query.
 */

typedef struct JSONPath_Iterator {
    const char *key;
    size_t len;
    JSON_Value val;
} JSONPath_Iterator;

/**
 * @ingroup json
 * @struct JSONPath_Iterator
 *
 * This structure represents a lazy evaluation of a @ref JSONPath_Query,
 * see @ref jsonpath_each(). After each successful @ref jsonpath_next(),
 * @b public @ref key is the RFC 6901 JSON Pointer of the current node,
 * @b public @ref len its length, and @b public @ref val its value.
 */

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_init(JSON_Parser *ctx);

/**
 * @ingroup json
 * @fn int jsonpath_init(JSON_Parser *ctx)
 * @param ctx parser context
 * @return 0 on success, -1 on error
 *
 * Configure @p ctx to build an index while parsing a JSON document.
 *
 * The document is indexed by RFC 6901 JSON Pointer and can subsequently
 * be queried with @ref jsonpath_foreach() or @ref jsonpath_each() or
 * accessed by pointer. The index remains attached to @p ctx until
 * @ref jsonpath_free() is called.
 *
 * Call this function before @ref string_parse_json().
 *
 * @see jsonpath_free()
 * @see jsonpath_query()
 * @see json_pointer_get()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_print(JSON_Parser *ctx);

/**
 * @ingroup json
 * @fn int jsonpath_print(JSON_Parser *ctx)
 * @param ctx indexing context
 * @return 0
 *
 * Print the contents of the JSON Pointer index to standard output.
 *
 * This function is intended for debugging and inspection.
 */

/* -------------------------------------------------------------------------- */

ASKL_API JSON_Value json_pointer_get(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len
);

/**
 * @ingroup json
 * @fn JSON_Value json_pointer_get(JSON_Parser *ctx, const char *ptr, size_t len)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the value
 * @param len length of @p ptr
 * @return the value at @p ptr, or a zero-initialized value if the lookup fails
 *
 * Return the value identified by @p ptr.
 *
 * An empty pointer selects the document root. The index is read-locked for
 * the duration of the lookup.
 *
 * @see jsonpath_get()
 * @see json_pointer_set()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    JSON_Value value
);

/**
 * @ingroup json
 * @fn int json_pointer_set(JSON_Parser *ctx, const char *ptr, size_t len, JSON_Value value)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @param value scalar value to store
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr with @p value.
 *
 * The pointer must already identify a scalar. Containers are not replaced by
 * this function. The index is locked for writing for the duration of the
 * replacement.
 *
 * @see json_pointer_get()
 * @see jsonpath_set()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set_null(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len
);

/**
 * @ingroup json
 * @fn int json_pointer_set_null(JSON_Parser *ctx, const char *ptr, size_t len)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr with JSON null.
 *
 * The pointer must already identify a scalar. The index is locked for writing
 * for the duration of the replacement.
 *
 * @see jsonpath_set_null()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set_boolean(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    int value
);

/**
 * @ingroup json
 * @fn int json_pointer_set_boolean(JSON_Parser *ctx, const char *ptr, size_t len, int value)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @param value zero for false, nonzero for true
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr with a JSON boolean.
 *
 * The pointer must already identify a scalar. The index is locked for writing
 * for the duration of the replacement.
 *
 * @see jsonpath_set_boolean()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set_integer(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    int64_t value
);

/**
 * @ingroup json
 * @fn int json_pointer_set_integer(JSON_Parser *ctx, const char *ptr, size_t len, int64_t value)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @param value integer value to store
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr with a JSON integer.
 *
 * The pointer must already identify a scalar. The index is locked for writing
 * for the duration of the replacement.
 *
 * @see jsonpath_set_integer()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set_decimal(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    double value
);

/**
 * @ingroup json
 * @fn int json_pointer_set_decimal(JSON_Parser *ctx, const char *ptr, size_t len, double value)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @param value decimal value to store
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr with a JSON decimal.
 *
 * The pointer must already identify a scalar. The index is locked for writing
 * for the duration of the replacement.
 *
 * @see jsonpath_set_decimal()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int json_pointer_set_string(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    char *value,
    size_t value_len
);

/**
 * @ingroup json
 * @fn int json_pointer_set_string(JSON_Parser *ctx, const char *ptr, size_t len, char *value, size_t value_len)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @param value string data to store
 * @param value_len length of @p value
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr with a JSON string.
 *
 * Ownership of @p value is transferred to the index on success and remains
 * with the caller on failure. The pointer must already identify a scalar.
 * The index is locked for writing for the duration of the replacement.
 *
 * @see jsonpath_set_string()
 */

/* -------------------------------------------------------------------------- */

ASKL_API JSON_Value jsonpath_get(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len
);

/**
 * @ingroup json
 * @fn JSON_Value jsonpath_get(JSON_Parser *ctx, const char *ptr, size_t len)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the value
 * @param len length of @p ptr
 * @return the value at @p ptr, or a zero-initialized value if the lookup fails
 *
 * Return the value identified by @p ptr while a JSONPath query or iterator
 * already holds the index read lock.
 *
 * An empty pointer selects the document root. This function does not acquire
 * a lock and is intended for use from JSONPath traversal.
 *
 * @see json_pointer_get()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    JSON_Value value
);

/**
 * @ingroup json
 * @fn int jsonpath_set(JSON_Parser *ctx, const char *ptr, size_t len, JSON_Value value)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @param value scalar value to store
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr while a JSONPath query or iterator
 * holds the index read lock.
 *
 * The caller's read lock is upgraded for the replacement and restored before
 * returning. The pointer must already identify a scalar.
 *
 * @see json_pointer_set()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set_null(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len
);

/**
 * @ingroup json
 * @fn int jsonpath_set_null(JSON_Parser *ctx, const char *ptr, size_t len)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr with JSON null during JSONPath
 * traversal.
 *
 * The caller's read lock is upgraded for the replacement and restored before
 * returning.
 *
 * @see json_pointer_set_null()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set_boolean(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    int value
);

/**
 * @ingroup json
 * @fn int jsonpath_set_boolean(JSON_Parser *ctx, const char *ptr, size_t len, int value)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @param value zero for false, nonzero for true
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr with a JSON boolean during JSONPath
 * traversal.
 *
 * The caller's read lock is upgraded for the replacement and restored before
 * returning.
 *
 * @see json_pointer_set_boolean()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set_integer(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    int64_t value
);

/**
 * @ingroup json
 * @fn int jsonpath_set_integer(JSON_Parser *ctx, const char *ptr, size_t len, int64_t value)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @param value integer value to store
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr with a JSON integer during JSONPath
 * traversal.
 *
 * The caller's read lock is upgraded for the replacement and restored before
 * returning.
 *
 * @see json_pointer_set_integer()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set_decimal(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    double value
);

/**
 * @ingroup json
 * @fn int jsonpath_set_decimal(JSON_Parser *ctx, const char *ptr, size_t len, double value)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @param value decimal value to store
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr with a JSON decimal during JSONPath
 * traversal.
 *
 * The caller's read lock is upgraded for the replacement and restored before
 * returning.
 *
 * @see json_pointer_set_decimal()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_set_string(
    JSON_Parser *ctx,
    const char *ptr,
    size_t len,
    char *value,
    size_t value_len
);

/**
 * @ingroup json
 * @fn int jsonpath_set_string(JSON_Parser *ctx, const char *ptr, size_t len, char *value, size_t value_len)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @param ptr JSON Pointer identifying the scalar to replace
 * @param len length of @p ptr
 * @param value string data to store
 * @param value_len length of @p value
 * @return 0 on success, -1 on failure
 *
 * Replace the scalar identified by @p ptr with a JSON string during JSONPath
 * traversal.
 *
 * Ownership of @p value is transferred to the index on success and remains
 * with the caller on failure. The caller's read lock is upgraded for the
 * replacement and restored before returning.
 *
 * @see json_pointer_set_string()
 */

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_free(JSON_Parser *ctx);

/**
 * @ingroup json
 * @fn int jsonpath_free(JSON_Parser *ctx)
 * @param ctx indexing context initialized by @ref jsonpath_init()
 * @return 0 on success, -1 if @p ctx is invalid
 *
 * Release the JSON Pointer index and all resources associated with it,
 * and detach the indexing callbacks from @p ctx.
 *
 * Compiled @ref JSONPath_Query objects are independent of the index and
 * are not affected.
 *
 * @see jsonpath_init()
 * @see jsonpath_query_free()
 */

/* -------------------------------------------------------------------------- */

ASKL_API JSONPath_Query *jsonpath_query_alloc(const char *expr, size_t len);

/**
 * @ingroup json
 * @fn JSONPath_Query *jsonpath_query_alloc(const char *expr, size_t len)
 * @param expr RFC 9535 JSONPath expression
 * @param len length of @p expr in bytes
 * @return a compiled query, or NULL if the query cannot be compiled
 *
 * Compile an RFC 9535 JSONPath expression into a reusable query.
 *
 * All RFC 9535 segments, selectors, filter expressions, operators and
 * standard functions are supported. The match() and search() functions
 * use RFC 9485 I-Regexp and require ASKL to be built with PCRE; without
 * PCRE, queries using either function are rejected.
 *
 * The returned query is independent of any particular index and may be reused
 * with @ref jsonpath_foreach() and @ref jsonpath_each().
 *
 * Release it with @ref jsonpath_query_free().
 */

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_foreach(
    JSON_Parser *ctx,
    JSONPath_Query *query,
    int (*callback)(const char *ptr, size_t len, void *arg),
    void *arg
);

/**
 * @ingroup json
 * @fn int jsonpath_foreach(JSON_Parser *ctx, JSONPath_Query *query,
 *                          int (*callback)(const char *, size_t, void *),
 *                          void *arg)
 * @param ctx indexing context containing a successfully parsed document
 * @param query compiled JSONPath query
 * @param callback function called for each selected node
 * @param arg argument passed unchanged to @p callback
 * @return the number of nodes delivered to @p callback, or -1 on error
 *
 * Evaluate @p query against the document indexed by @p ctx.
 *
 * For each selected node, @p callback receives its RFC 6901 JSON Pointer and
 * its length. The pointer is NUL-terminated and remains valid only for the
 * duration of the callback. Use @ref jsonpath_get() to retrieve the selected
 * value while inside the callback.
 *
 * A callback return value of 0 continues evaluation. Any other value stops
 * evaluation. The function returns the number of nodes delivered before
 * evaluation completed or was stopped.
 *
 * Results follow RFC 9535 ordering where the specification defines an order.
 * Object member order is unspecified by RFC 9535; ASKL returns object members
 * deterministically by member name, with numeric components ordered
 * numerically.
 *
 * The index remains read-locked during evaluation. Writers in other threads
 * therefore wait until evaluation completes. Scalar values may be replaced
 * from the callback with the @c jsonpath_set_* functions, which temporarily
 * upgrade the traversal's read lock.
 *
 * @see jsonpath_get
 * @see jsonpath_set
 */

/* -------------------------------------------------------------------------- */

ASKL_API JSONPath_Iterator *jsonpath_each(JSON_Parser *ctx, JSONPath_Query *q);

/**
 * @ingroup json
 * @fn JSONPath_Iterator *jsonpath_each(JSON_Parser *ctx, JSONPath_Query *q)
 * @param ctx indexing context containing a successfully parsed document
 * @param q compiled JSONPath query
 * @return an iterator positioned before the first result, or NULL on error
 *
 * Start a lazy evaluation of @p q against the document indexed by @p ctx.
 *
 * Evaluation advances only as required by @ref jsonpath_next(). Results
 * selected by the current segment may be prepared together before later
 * segments are evaluated, so stopping early avoids work below unconsumed
 * results but does not necessarily avoid selecting their siblings.
 *
 * Some descendant segments require all of their results to be collected and
 * ordered before the first result can be returned. This applies to a descendant
 * segment with a single name selector and to descendant segments containing
 * filter selectors.
 *
 * Results are returned in the same order as @ref jsonpath_foreach().
 *
 * The iterator holds the index read lock until it is exhausted or passed to
 * @ref jsonpath_break(). Writers in other threads block for that duration.
 * The @c jsonpath_set_* functions may replace scalar values during traversal
 * by temporarily upgrading the iterator's read lock.
 *
 * @see jsonpath_next()
 * @see jsonpath_break()
 * @see jsonpath_foreach()
 */

/* -------------------------------------------------------------------------- */

ASKL_API JSONPath_Iterator *jsonpath_next(JSONPath_Iterator *iterator);

/**
 * @ingroup json
 * @fn JSONPath_Iterator *jsonpath_next(JSONPath_Iterator *iterator)
 * @param iterator an iterator returned by @ref jsonpath_each()
 * @return @p iterator positioned on the next result, or NULL if iteration
 *         ends or an error occurs; returning NULL also destroys the iterator
 *
 * Advance @p iterator to the next selected node.
 *
 * On success, @c key is the NUL-terminated RFC 6901 JSON Pointer of the node,
 * @c len is its length, and @c val is its value. The pointer remains valid
 * until the next call to this function or until the iterator is destroyed.
 *
 * @note Do not free the iterator directly. It is destroyed automatically when
 *       iteration ends. Use @ref jsonpath_break() to stop early.
 */

/* -------------------------------------------------------------------------- */

ASKL_API JSONPath_Iterator *jsonpath_break(JSONPath_Iterator *iterator);

/**
 * @ingroup json
 * @fn JSONPath_Iterator *jsonpath_break(JSONPath_Iterator *iterator)
 * @param iterator an iterator returned by @ref jsonpath_each(), or NULL
 * @return NULL
 *
 * Destroy an iterator before its nodes are exhausted.
 *
 * Passing NULL is allowed.
 */

/* -------------------------------------------------------------------------- */

ASKL_API JSONPath_Query *jsonpath_query_free(JSONPath_Query *query);

/**
 * @ingroup json
 * @fn JSONPath_Query *jsonpath_query_free(JSONPath_Query *query)
 * @param query compiled query, or NULL
 * @return NULL
 *
 * Release a query returned by @ref jsonpath_query_alloc().
 *
 * Passing NULL is allowed. The function always returns NULL so that a
 * query can conveniently be released and cleared in one statement.
 */

/* -------------------------------------------------------------------------- */
#endif /* _ENABLE_JSON && _ENABLE_TRIE */
/* -------------------------------------------------------------------------- */

#endif
