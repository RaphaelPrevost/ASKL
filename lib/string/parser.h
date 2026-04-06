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

#ifndef ASKL_PARSER_H

#define ASKL_PARSER_H

/* -------------------------------------------------------------------------- */
#if (defined(_ENABLE_JSON))
/* -------------------------------------------------------------------------- */

#include "../askl.h"
#include "../askl_string.h"

typedef struct JSON_Parser {
    void *context;
    struct {
        const char *current;
        size_t len;
    } key;
    union {
        struct {
            uint8_t type;
            uint8_t neg;
            uint8_t rad;
            uint8_t exp;
        } current;
        uint32_t data;
    } primitive;
    int strict;
    int parent;
    int (CALLBACK *init)(int, struct JSON_Parser *);
    int (CALLBACK *data)(String *, struct JSON_Parser *);
    int (CALLBACK *exit)(int, struct JSON_Parser *);
} JSON_Parser;

#define JSON_OBJECT         0x1000
#define JSON_ARRAY          0x2000
#define JSON_STRING         0x4000
#define JSON_PRIMITIVE      0x8000
#define JSON_TYPE           0xf000

#define JSON_PRIMITIVE_NUMBER  0x1
#define JSON_PRIMITIVE_NULL    0x2
#define JSON_PRIMITIVE_BOOL    0x4
#define JSON_PRIMITIVE_TRUE   0x10
#define _JSON_PRIMITIVE_HEX   0x20

#define IS_OBJECT(x) ((x)->internal.flags & JSON_OBJECT)
#define IS_ARRAY(x) ((x)->internal.flags & JSON_ARRAY)
#define IS_STRING(x) ((x)->internal.flags & JSON_STRING)
#define IS_PRIMITIVE(x) ((x)->internal.flags & JSON_PRIMITIVE)
#define IS_TYPE(x, type) ((x)->internal.flags & (type))

#define JSON_QUIRKS         0
#define JSON_STRICT         1

/* -------------------------------------------------------------------------- */

ASKL_API int string_parse_json(String *s, char strict, JSON_Parser *ctx);

/**
 * @ingroup string
 * @fn m_string *string_parse_json(m_string *s, int strict, m_json_parser *ctx)
 * @param s the string to be parsed
 * @param strict boolean - enable or disable strict parsing
 * @param ctx optional parser context
 * @return -1 if an error occurred, 0 otherwise
 *
 * This function creates tokens for each JSON element in the provided string.
 *
 * In STRICT mode, the function rigorously follows RFC 7159 and will reject
 * any non-compliant or malformed input.
 *
 * In QUIRKS mode, the function loosely follows the JSON5 guidelines and will
 * accept several popular extensions to the JSON specification:
 * - unquoted keys in objects (ECMAScript 5.1 IdentifierName),
 * - single quoted strings,
 * - single- and multi-line comments,
 * - unsigned hexadecimal numbers,
 * - real numbers without fractional part (e.g.: 123. ) (ActionScript3),
 * - extra commas and missing values in objects and arrays,
 * - escaped and unescaped tabs and CRLF in strings.
 * Unsupported:
 * - leading decimal point,
 * - leading plus sign,
 * - NaN, Infinity, -Infinity.
 *
 * This function tokenizes the input and checks for correctness but will not
 * interpret the data. This task is left to the parser. The builtin parser
 * performs UTF-8 validation, escape sequences conversions and turns JSON
 * numbers into native IEEE754 double-precision floating-point numbers.
 *
 * The function is designed to handle streaming and partial input. If several
 * JSON messages are concatenated, each will be parsed as distinct tokens.
 * Partial input will not trigger an error, but will be flagged, so that
 * valid tokens can be processed first, and parsing resumed later when more
 * data become available.
 *
 * @note Use the macro @ref IS_ERROR to test if a token is incomplete.
 *
 */

/* -------------------------------------------------------------------------- */
#if (defined(_ENABLE_PARSER) && defined(_ENABLE_TRIE))
/* -------------------------------------------------------------------------- */

#include "../askl_cbtrie.h"

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_init(JSON_Parser *ctx);

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_print(JSON_Parser *ctx);

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_free(JSON_Parser *ctx);

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
#endif /* _ENABLE_PARSER && _ENABLE_TRIE */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
#endif /* _ENABLE_JSON */
/* -------------------------------------------------------------------------- */

#endif
