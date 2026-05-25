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

#include "parser.h"

/* -------------------------------------------------------------------------- */
#if (defined(_ENABLE_JSON))
/* -------------------------------------------------------------------------- */

#include "../arcane/bitops.c"

static const unsigned char _j[] = {
     2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  2, /*  12 */
     2,  3,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, /*  24 */
     2,  2,  2,  2,  2,  2,  2,  2,  4, 15,  0, 15, /*  36 */
    15, 15, 15,  0, 15, 15, 15,  8, 11,  9,  7, 15, /*  48 */
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 12, 15, /*  60 */
    15, 15, 15, 15, 15, 15, 15, 15, 15,  6, 15, 15, /*  72 */
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, /*  84 */
    15, 15, 15, 15, 15, 15, 15, 13,  1, 14, 15, 15, /*  96 */
    15, 15, 15, 15, 15,  6,  5, 15, 15, 15, 15, 15, /* 108 */
    15, 15,  5, 15, 15, 15, 15, 15,  5, 15, 15, 15, /* 120 */
    15, 15, 15, 13, 15, 14, 15, 15, 16, 16, 16, 16, /* 128 */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 140 */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 152 */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 164 */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 176 */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 188 */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 200 */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 212 */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 224 */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 236 */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 248 */
    16, 16, 16, 16, 16, 16, 16, 16, 16              /* 256 */
};

enum {
    QUOTE,     /* ', " */
    ESCAPESEQ, /* \ */
    NON_PRINT, /* ASCII non-printing characters */
    WHITE,     /* \n, \r, \t */
    SPACE,     /* 0x20 */
    PRIMITIVE, /* f(alse), n(ull), t(rue) */
    DIGIT_EXP, /* e, E */
    DIGIT_RAD, /* . */
    DIGIT_POS, /* + */
    DIGIT_NEG, /* - */
    DIGIT,     /* 0-9 */
    COMMA,     /* , */
    COLON,     /* : */
    OBJ_START, /* {, [ */
    OBJ_CLOSE, /* }, ] */
    ASCII,     /* ASCII characters */
    EXT_ASCII, /* Extended ASCII */
};

/* tokenizer states */
#define _KEY 0x01   /* key expected */
#define _VAL 0x02   /* value expected */
#define _SIG 0x04   /* '+' or '-' sign found */
#define _RAD 0x08   /* '.' decimal separator (radix point) found */
#define _EXP 0x10   /* 'e' or 'E' exponent found */
#define _BUG 0x20   /* only used in quirks mode */
#define _EOF 0x40   /* reached the End Of File */
#define _INC 0x80   /* incomplete input */

/* -------------------------------------------------------------------------- */

ASKL_API int string_parse_json(String *s, char strict, JSON_Parser *ctx)
{
    unsigned int pos = 0;
    unsigned char class = 0;
    char *p = NULL, state = 0, leading_digit = 0;
    String *json = s, *parent = NULL;
    int callback = 0, prealloc = 2;

    if (! json) {
        debug("string_parse_json(): bad parameters.\n");
        return -1;
    }

    /* check if we should resume parsing */
    if (json->count && IS_TYPE(last_token(json), JSON_TYPE)) {
        if (IS_BUFFER(s)) {
            /* the maximum amount of tokens was reached */
            for (json = last_token(s); json->count; json = last_token(json)) {
                if (last_token(json)->count == 65535) {
                    int i = 0;

                    json = last_token(json);

                    pos = last_token(json)->data - json->data +
                          last_token(json)->len + 1;

                    for (i = 0; i < 65535; i ++)
                        string_free_token(json->tokens + i);
                    json->count = 0;

                    s->internal.flags &= ~_STRING_BUFFERING;
                    break;
                }
            }
        } else if (HAS_ERROR(last_token(json))) {
            /* input was incomplete */
            state |= _INC;

            while (json->count) {
                if (! HAS_ERROR(last_token(json))) {
                    /* restart parsing at the last known-good character */
                    pos = string_end(last_token(json)) - json->data;

                    if (IS_STRING(last_token(json))) {
                        /* last character must be a quote */
                        if (json->data[pos] == last_token(json)->data[-1])
                            pos ++;
                    }

                    /* odd count of object elements means a key is expected */
                    if (IS_OBJECT(json) && (json->count & 1)) state |= _KEY;

                    break;
                } else {
                    /* number, as bool/null only emit a token if complete */
                    if (IS_PRIMITIVE(last_token(json))) {
                        /* discard incomplete number */
                        json->count --;
                        continue;
                    }
                    json = last_token(json);
                }
            }

            if (! json->count) {
                if (IS_OBJECT(json->parent) && (json->parent->count & 1))
                    state |= _KEY;

                if (IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                    /* skip the opening bracket */
                    pos = (
                        (IS_OBJECT(json) && *json->data == '{') ||
                        (IS_ARRAY(json) && *json->data == '[')
                    );
                    state = (
                        (state & ~_KEY) | (-(IS_OBJECT(json) > 0) & _KEY) |
                        _VAL
                    );
                }
            }
        } else string_free_token(json);
    } else {
        /* skip UTF-8 BOM if present */
        if (! memcmp(json->data, "\xef\xbb\xbf", MIN(json->len, 3)))
            pos += 3;
        string_free_token(json);
    }

    if (ctx) ctx->strict = strict;

    for ( ; pos < json->len; pos ++) {

        class = _j[(uint8_t) json->data[pos]];

        if (class > 3) {
            if (unlikely(IS_STRING(json))) {
                for (++ pos; pos < json->len; pos ++) {
                    class = _j[(uint8_t) json->data[pos]];
                    if (class < 4) goto _parse;
                }
                return 0; /* EOF */
            } else p = (char *) json->data + pos;
        }

_parse: switch (class) {

        /* ', " */
        case QUOTE: {
            if (IS_STRING(json)) {
                /* check if the quotes are matching */
                if (json->data[-1] != json->data[pos])
                    break;
                json->len = json->internal.capacity = pos;
                goto _delim;
            } else if (unlikely(IS_PRIMITIVE(json))) {
                debug("string_parse_json(): unexpected quotation mark.\n");
                goto _error;
            }

            if (strict) {
                if (unlikely(json->data[pos] == '\'')) {
                    debug(
                        "string_parse_json(): strings must be enclosed "
                        "in double-quotes.\n"
                    );
                    goto _error;
                }

                if (IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                    if ((state & _VAL) == 0) {
                        debug("string_parse_json(): unexpected string.\n");
                        goto _error;
                    }
                }
            }

            if (likely(pos + 1 < json->len)) {
                json = string_add_token(json, pos + 1, json->len);
                if (unlikely(! json)) goto _nomem;

                json->internal.flags &= ~JSON_TYPE;
                json->internal.flags |= (JSON_STRING | _STRING_HAS_ERROR);
                state &= ~_VAL; pos = -1;
            } else return 0; /* EOF */

            {   /* optimize for long strings */
                uint32_t bytes;
                if (likely(pos + sizeof(bytes) < json->len)) {
                    memcpy(& bytes, json->data, sizeof(bytes));
                    if (__zero(bytes ^ (~0U / 255 * json->data[-1])))
                        break; /* " or ' */
                    if (unlikely(__less(bytes, ' ')))
                        break; /* unescaped special char */
                    if (likely(! (bytes = __zero(bytes ^ 0x5c5c5c5cU)))) {
                        pos += sizeof(bytes); break;
                    } else pos += __zero_idx(bytes); /* \ */
                } else break;
            }
        } /* FALLTHRU */

        /* \ */
        case ESCAPESEQ: { /* escape sequence */
            int z = 0;

            if (! IS_STRING(json)) {
                debug(
                    "string_parse_json(): escape sequences are only "
                    "allowed in strings.\n"
                );
                goto _error;
            }

            if (unlikely(pos + 1 == json->len)) {
                debug("string_parse_json(): incomplete escape sequence.\n");
                return 0; /* EOF */
            }

            switch (json->data[++ pos]) {
            case '\"': break;

            /* QUIRK escaped single quotes, CRLF and capital U
                     unicode escape sequences */
            case '\r':
            case '\n':
            case '\'': z = 1;
            case  'U': if (strict) goto _error; if (z)

            case  '/':
            case '\\':
            case  'b':
            case  'f':
            case  'n':
            case  'r':
            case  't': break;

            case  'u': { /* unicode escape sequence */
                uint32_t bytes;
                if (likely(pos + 1 + sizeof(bytes) < json->len)) {
                    memcpy(& bytes, json->data + pos + 1, sizeof(bytes));
                    if (! __less(bytes, '0') && ! __more(bytes, 'f'))
                    if (likely(! __between(bytes, '9', 'A')))
                    if (likely(! __between(bytes, 'F', 'a'))) {
                        pos += sizeof(bytes); break;
                    }
                } else {
                    for (pos = pos + 1; pos < json->len; pos ++) {
                        if (json->data[pos] < '0' || json->data[pos] > 'f')
                            goto _error;
                        if (json->data[pos] > '9' && json->data[pos] < 'A')
                            goto _error;
                        if (json->data[pos] > 'F' && json->data[pos] < 'a')
                            goto _error;
                    }
                    debug(
                        "string_parse_json(): incomplete Unicode "
                        "escape sequence.\n"
                    );
                    return 0; /* EOF */
                }
            }

            /* unexpected character */
            default: goto _error;
            }
        } break;

        case NON_PRINT: goto _error;

        /* \t, \r, \n, 0x20 */
        case WHITE: if (strict && IS_STRING(json)) goto _error;
        case SPACE: if (likely(IS_PRIMITIVE(json))) {
            if (state & _VAL) {
                debug("string_parse_json(): incomplete primitive.\n");
                goto _error;
            }

            /* QUIRK unquoted keys */
            json->internal.flags = (json->internal.flags & ~JSON_TYPE) |
                           (-(state & _BUG) & JSON_STRING) |
                           (-(! (state & _BUG)) & JSON_PRIMITIVE);
            state = (state & ~_KEY) | (-(IS_STRING(json) > 0) & _KEY);
            state &= ~_BUG;
            leading_digit = 0;
            json->len = json->internal.capacity = pos;
            goto _delim;
            break;
        } break;

        /* f, n, t */
        case PRIMITIVE: if (likely(! IS_PRIMITIVE(json))) {
            if (IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                if (state & _KEY) {
                    /* QUIRK unquoted keys */
                    if (! strict) {
                        json = string_add_token(json, pos, json->len);
                        if (unlikely(! json)) goto _nomem;

                        json->internal.flags &= ~JSON_TYPE;
                        json->internal.flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
                        pos = 0; state &= ~(_KEY | _VAL); state |= _BUG;
                        break;
                    }

                    debug(
                        "string_parse_json(): a key must be "
                        "enclosed in quotation marks.\n"
                    );
                    goto _error;
                }

                if ((state & _VAL) == 0) {
                    debug("string_parse_json(): unexpected primitive.\n");
                    goto _error;
                }
            }

            state |= _EOF;

            switch (*p) {
            case 'f': switch (MIN((json->len - (pos + 1)), 4)) {
                      case 4: state &= ~_EOF;
                              if (p[4] != 'e') goto _error;
                      case 3: if (p[3] != 's') goto _error;
                      case 2: if (p[2] != 'l') goto _error;
                      case 1: if (p[1] != 'a') goto _error;
                              if (ctx)
                                  ctx->primitive.current.type = (
                                      JSON_PRIMITIVE_BOOL
                                  );
                              p += 4;
                      } break;
            case 'n': switch (MIN((json->len - (pos + 1)), 3)) {
                      case 3: state &= ~_EOF;
                              if (p[3] != 'l') goto _error;
                      case 2: if (p[2] != 'l') goto _error;
                      case 1: if (p[1] != 'u') goto _error;
                              if (ctx)
                                  ctx->primitive.current.type = (
                                      JSON_PRIMITIVE_NULL
                                  );
                              p += 3;
                      } break;
            case 't': switch (MIN((json->len - (pos + 1)), 3)) {
                      case 3: state &= ~_EOF;
                              if (p[3] != 'e') goto _error;
                      case 2: if (p[2] != 'u') goto _error;
                      case 1: if (p[1] != 'r') goto _error;
                              if (ctx)
                                  ctx->primitive.current.type = (
                                      JSON_PRIMITIVE_BOOL | JSON_PRIMITIVE_TRUE
                                  );
                              p += 3;
                      } break;
            default:  goto _error;
            }

            /* EOF */
            if (unlikely(state & _EOF)) return 0;

            state &= ~(_VAL | _RAD | _EXP | _SIG);

            json = string_add_token(json, pos, json->len);
            if (unlikely(! json)) goto _nomem;

            json->internal.flags &= ~JSON_TYPE;
            json->internal.flags |= JSON_PRIMITIVE;

            pos = p - json->data;

            goto _token;
        } else if (state & _BUG) break; goto _error; /* QUIRK unquoted keys */

        /* e, E */
        case DIGIT_EXP: {
            /* exponent cannot be placed right after a radix point */
            if ((state & _RAD) && p[-1] == '.') {
                debug("string_parse_json(): a fractional part is expected.\n");
                goto _error;
            }

            if ((state & _EXP) || leading_digit == 0) {
                /* QUIRK unquoted keys */
                if (! strict) {
                    if (state & _KEY) {
                        json = string_add_token(json, pos, json->len);
                        if (unlikely(! json)) goto _nomem;

                        json->internal.flags &= ~JSON_TYPE;
                        json->internal.flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
                        pos = 0; state &= ~(_KEY | _VAL); state |= _BUG;
                        break;
                    } else if ((state & _BUG) && (IS_PRIMITIVE(json)))
                        break;
                }

                debug("string_parse_json(): unexpected exponent.\n");
                goto _error;
            }

            state &= ~_SIG;
            state |= (_EXP | _VAL);
            if (ctx) ctx->primitive.current.exp = pos;
        } break;

        /* . */
        case DIGIT_RAD: {
            if (unlikely(leading_digit == 0) || (state & (_RAD | _EXP))) {
                debug("string_parse_json(): unexpected decimal separator.\n");
                goto _error;
            }

            state &= ~_SIG; state |= _RAD;

            if (ctx) ctx->primitive.current.rad = pos;

            {   /* optimize for large numbers */
                uint32_t bytes;
                if (likely(pos + 1 + sizeof(bytes) < json->len)) {
                    memcpy(& bytes, p + 1, sizeof(bytes));
                    bytes = __more(bytes, '9') | __less(bytes, '0');
                    if ( (bytes = __zero_idx(bytes)) ) {
                        pos += bytes; break;
                    }
                }
            }

            /* QUIRK omitted fractional part */
            state |= (-(strict) & _VAL);
        } break;

        /* + */
        case DIGIT_POS: if ((state & (_SIG | _EXP | _VAL)) != (_EXP | _VAL)) {
            debug("string_parse_json(): unexpected plus sign.\n");
            goto _error;
        } else state |= (_SIG | _VAL); break;

        /* - */
        case DIGIT_NEG: {
            if (likely(! IS_PRIMITIVE(json))) {
                if (IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                    if (state & _KEY) {
                        debug(
                            "string_parse_json(): a key must be "
                            "enclosed in quotation marks.\n"
                        );
                        goto _error;
                    }

                    if ((state & _VAL) == 0) {
                        debug(
                            "string_parse_json(): unexpected "
                            "negative primitive.\n"
                        );
                        goto _error;
                    }
                }

                json = string_add_token(json, pos, json->len);
                if (unlikely(! json)) goto _nomem;

                json->internal.flags &= ~JSON_TYPE;
                json->internal.flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
                pos = 0; state &= ~(_RAD | _EXP); leading_digit = 0;
                if (ctx) {
                    ctx->primitive.current.type = JSON_PRIMITIVE_NUMBER;
                    ctx->primitive.current.neg = 1;
                    ctx->primitive.current.rad = 0;
                    ctx->primitive.current.exp = 0;
                }
            } else if ((state & (_SIG | _EXP | _VAL)) != (_EXP | _VAL)) {
                debug("string_parse_json(): unexpected minus sign.\n");
                goto _error;
            }

            state |= _SIG;
        } break;

        /* 0-9 */
        case DIGIT: if (likely(leading_digit)) {
            if (leading_digit == '0') {
                if ((state & (_RAD | _EXP)) == 0) {
                    debug(
                        "string_parse_json(): octal integers "
                        "are not supported.\n"
                    );
                    goto _error;
                }
                leading_digit = 1;
            }

            state &= ~_VAL;

            {   /* optimize for large numbers */
                uint32_t bytes;
                if (likely(pos + 1 + sizeof(bytes) < json->len)) {
                    memcpy(& bytes, p + 1, sizeof(bytes));
                    bytes = __more(bytes, '9') | __less(bytes, '0');
                    pos += __zero_idx(bytes);
                }
            }
        } else {
            leading_digit = *p;

            if (likely(! IS_PRIMITIVE(json))) {
                if (IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                    if (state & _KEY) {
                        /* QUIRK unquoted keys */
                        if (! strict) {
                            json = string_add_token(json, pos, json->len);
                            if (unlikely(! json)) goto _nomem;

                            json->internal.flags &= ~JSON_TYPE;
                            json->internal.flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
                            pos = 0; state &= ~(_KEY | _VAL); state |= _BUG;
                            break;
                        }

                        debug(
                            "string_parse_json(): a key must be a "
                            "string enclosed in quotation marks.\n"
                        );
                        goto _error;
                    }

                    if ((state & _VAL) == 0) {
                        debug(
                            "string_parse_json(): unexpected "
                            "numeric primitive.\n"
                        );
                        goto _error;
                    }
                }

                state &= ~(_VAL | _RAD | _EXP | _SIG);

                json = string_add_token(json, pos, json->len);
                if (unlikely(! json)) goto _nomem;

                json->internal.flags &= ~JSON_TYPE;
                json->internal.flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
                if (ctx) {
                    ctx->primitive.data = 0;
                    ctx->primitive.current.type = JSON_PRIMITIVE_NUMBER;
                }

                pos = 0;
            } else state &= ~_VAL;
        } break;

        /* , */
        case COMMA: {
            if (strict && unlikely(state & _VAL)) {
                debug("string_parse_json(): a value is expected.\n");
                goto _error;
            }

            if (IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                state = (state & ~_KEY) | (-(IS_OBJECT(json) > 0) & _KEY);
                state |= _VAL;

                /* skip space */
                pos += likely(p[1] == ' ');

                break;
            }

            if (IS_PRIMITIVE(json)) {
                pos --; leading_digit = 0;
                goto _token;
            }

            if (strict) {
                debug(
                    "string_parse_json(): comma-separated lists "
                    "must be enclosed in brackets.\n"
                );
                goto _error;
            }
        } break;

        /* : */
        case COLON: {
            if (unlikely((state & (_KEY | _VAL)) != _KEY)) {

                /* QUIRK unquoted keys */
                if (! strict && IS_PRIMITIVE(json)) {
                    pos --; leading_digit = 0;
                    state |= _KEY; state &= ~_BUG;
                    json->internal.flags &= ~JSON_TYPE;
                    json->internal.flags |= JSON_STRING;
                    goto _token;
                }

                if (! IS_OBJECT(json))
                    debug(
                        "string_parse_json(): key/value pairs are only "
                        "allowed in objects.\n"
                    );
                else debug("string_parse_json(): missing or malformed key.\n");

                goto _error;
            }

            state &= ~_KEY; state |= _VAL;

            /* skip space */
            pos += likely(p[1] == ' ');

            /* parser callback */
            if (ctx) {
                ctx->key.current = last_token(json)->data;
                ctx->key.len = last_token(json)->len;
            }
        } break;

        /* {, [ */
        case OBJ_START: {
            if (unlikely(state & _KEY)) {
                debug(
                    "string_parse_json(): opening bracket where "
                    "a key was expected.\n"
                );
                goto _error;
            }

            if ((state & _VAL) == 0 && json->parent) {
                debug("string_parse_json(): unexpected opening bracket.\n");
                goto _error;
            }

            json = string_add_token(json, pos, json->len);
            if (unlikely(! json)) goto _nomem;

            /* try to prealloc tokens */
            if (likely(json->tokens = malloc(prealloc * sizeof(*json->tokens))))
                json->internal.tokens_capacity = prealloc;

            pos = (*p == '{');

            json->internal.flags &= ~JSON_TYPE;
            json->internal.flags |= (-(pos) & JSON_OBJECT) | (-(! pos) & JSON_ARRAY);
            json->internal.flags |= _STRING_HAS_ERROR;

            state = (state & ~_KEY) | (-(pos) & _KEY) | _VAL;

            /* skip space */
            pos = (p[1] == ' ');

            /* parser callback */
            if (ctx && ctx->init) {
                if (json->parent)
                    ctx->parent = json->parent->internal.flags & JSON_TYPE;
                else ctx->parent = 0;

                if ( (callback = ctx->init(json->internal.flags & JSON_TYPE, ctx)) )
                    return (callback == 1) ? 0 : -1;

                ctx->key.current = NULL; ctx->key.len = 0;
            }
        } break;

        /* }, ] */
        case OBJ_CLOSE: {
            if (strict) {
                if ((state & _KEY) && json->count & 0x1) {
                    debug("string_parse_json(): a key is expected.\n");
                    goto _error;
                }

                if ((state & _VAL) && json->count) {
                    debug("string_parse_json(): a value is expected.\n");
                    goto _error;
                }
            }

            if ((state & _VAL) && ! IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                debug("string_parse_json(): a value is expected.\n");
                goto _error;
            }

            state &= ~(_KEY | _VAL);
            prealloc = json->count;

            if (IS_PRIMITIVE(json)) {
                if (IS_TYPE(json->parent, JSON_ARRAY | JSON_OBJECT)) {
                    pos --; leading_digit = 0;
                    goto _token;
                }
            } else if (IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                /* check if the brackets are matching */
                if (*json->data == *p - 2) goto _token;

                if (state & _INC) goto _token;
                debug("string_parse_json(): mismatched bracket (%c).\n",
                      *json->data);
                goto _error;
            }

            /* a closing bracket must be within an array or object */
            debug("string_parse_json(): stray closing bracket.\n");
            goto _error;
        } break;

        case ASCII: if (strict)

        default: goto _error;

        /* QUIRK comments */
        if (*p == '/') {
            if (IS_PRIMITIVE(json)) {
                leading_digit = 0; pos --;
                if (state & _BUG) {
                    state |= _KEY; state &= ~_BUG;
                    json->internal.flags &= ~JSON_TYPE;
                    json->internal.flags |= JSON_STRING;
                }
                goto _token;
            }

            if (*++ p == '/') {
                do {
                    uint32_t bytes;
                    memcpy(& bytes, p, sizeof(bytes));
                    if (__zero(bytes ^ 0x0a0a0a0aU)) {
                        while (*p ++ != '\n');
                        p --; break;
                    }
                    p += sizeof(bytes);
                } while (p < json->data + json->len);
            } else if (*p ++ == '*') {
                do {
                    uint32_t bytes;
                    memcpy(& bytes, p, sizeof(bytes));
                    if (__zero(bytes ^ 0x2a2a2a2aU)) {
                        while (*p ++ != '*');
                        if (*p == '/') break;
                    } else p += sizeof(bytes);
                } while (p < json->data + json->len);
            }

            if ((p - json->data) - pos > 2) {
                pos = p - json->data; break;
            } else goto _error;
        }

        /* QUIRK unquoted keys */
        if (state & _KEY) {
            json = string_add_token(json, pos, json->len);
            if (unlikely(! json)) goto _nomem;

            json->internal.flags &= ~JSON_TYPE;
            json->internal.flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
            pos = 0; state &= ~(_KEY | _VAL); state |= _BUG;
            break;
        } else if ((state & _BUG) && (IS_PRIMITIVE(json))) break;

        /* QUIRK hexadecimal numbers */
        if (json->data[pos] == 'x') {
            if (leading_digit != '0' || ++ pos > 2) goto _error;

            do {
                uint32_t bytes;
                memcpy(& bytes, json->data + pos, sizeof(bytes));
                if (__less(bytes, '0') || __more(bytes, 'f'))
                    break;
                if (__between(bytes, '9', 'A'))
                    break;
                if (__between(bytes, 'F', 'a'))
                    break;
                pos += sizeof(bytes);
            } while (pos < 18); /* 64 bits */

            /* check the next characters */
            while (pos < 18) {
                if (json->data[pos] < '0' || json->data[pos] > 'f')
                    break;
                if (json->data[pos] > '9' && json->data[pos] < 'A')
                    break;
                if (json->data[pos] > 'F' && json->data[pos] < 'a')
                    break;
                pos ++;
            }

            /* '0x' without digits is not allowed */
            if (-- pos == 1) goto _error;

            if (ctx) {
                ctx->primitive.data = 0;
                ctx->primitive.current.type = _JSON_PRIMITIVE_HEX;
            }

            break;
        }

        goto _error;

        }

        continue;

_token: if (likely(json->len != pos))
        json->len = json->internal.capacity = pos + 1;
_delim: json->internal.flags = (json->internal.flags & ~_STRING_HAS_ERROR) | _STRING_VALIDATED;
        parent = json->parent;

        /* parser callback */
        if (ctx) {
            int type = json->internal.flags & JSON_TYPE;

            if (likely(parent))
                ctx->parent = parent->internal.flags & JSON_TYPE;
            else ctx->parent = 0;

            if (ctx->exit && (type & (JSON_ARRAY | JSON_OBJECT))) {
                if ( (callback = ctx->exit(type, ctx)) )
                    return (callback == 1) ? 0 : -1;
                ctx->key.current = NULL;
                ctx->key.len = 0;
            } else if (ctx->data && (state & _KEY) == 0) {
                if ( (callback = ctx->data(json, ctx)) )
                    return (callback == 1) ? 0 : -1;
                ctx->primitive.data = 0;
            }
        }

        if (likely(parent)) {
            pos += json->data - parent->data;
            if (ctx && callback == 0 && (state & _KEY) == 0)
                string_free_token(json);
            json = parent;
        } else break;
    }

    return 0;

_error:
    debug("string_parse_json(): illegal character \'%c\' at %i.\n",
          json->data[pos], (int) (json->data - s->data) + pos + 1);
    string_free_token(s);
    return -1;

_nomem:
    s->internal.flags |= _STRING_BUFFERING;
    return 1;
}

#undef _KEY
#undef _VAL
#undef _SIG
#undef _RAD
#undef _EXP
#undef _BUG
#undef _EOF
#undef _INC

/* -------------------------------------------------------------------------- */
#if (defined(_ENABLE_PARSER) && defined(_ENABLE_TRIE))
/* -------------------------------------------------------------------------- */

#include "../arcane/parser.c"

typedef struct Batch {
    struct Batch *next;
    uint16_t capacity;
    uint16_t depth;
    uint16_t prefix_len;
    uint16_t count;
    Trie_Leaf *leaves[];
} Batch;

typedef struct JSONPath_Context {
    Trie *tree;
    String *path;
    uint32_t count;
    struct Batch *batches;
    struct Batch *tailptr;
    struct Batch *mempool;
    struct Batch *current;
} JSONPath_Context;

/* -------------------------------------------------------------------------- */

static char *u32toa(uint32_t u32, char *out, size_t len)
{
    uint32_t prev = 0;
    uint16_t *p = (void *) (out + 10);

    if (len < 12) {
        debug("u32toa(): bad parameters.\n");
    }

    *p = 0;

    while (u32 >= 100) {
        prev = u32; p --; u32 /= 100;
        *p = u32toa_lut[prev - (u32 * 100)];
    }

    p --; *p = u32toa_lut[u32];

    return (char *) p + (u32 < 10);
}

/* -------------------------------------------------------------------------- */

static inline uint32_t atou32(const char *in, size_t len)
{
    unsigned int i = 0;
    uint32_t ret = 0;

    for (i = 0; i < len; i ++) ret = (ret * 10) + in[i] - '0';

    return ret;
}

/* -------------------------------------------------------------------------- */

static uint32_t increment_index(String *path)
{
    uint32_t index = 0;
    char *ret = NULL, buffer[12];
    unsigned int buflen;

    /* get the index */
    index = atou32(last_token(path)->data, last_token(path)->len);

    /* increment and replace */
    ret = u32toa(++ index, buffer, sizeof(buffer));
    buflen = 10 - (ret - buffer);
    if (likely(buflen == last_token(path)->len)) {
        memcpy((char *) last_token(path)->data, ret, buflen);
    } else {
        string_suppr_token(path, path->count - 1);
        string_push_token(path, ret, buflen);
    }

    return index;
}

/* -------------------------------------------------------------------------- */

static ssize_t json_string(char *out, const char *input, size_t len, int strict)
{
    size_t i = 0, pos = 0;
    uint32_t word = 0;
    uint32_t surrogate = 0;
    uint8_t c = 0;

    while (i + 4 <= len) {
        if (unlikely(surrogate)) {
            if (unlikely(input[i ++] != '\\')) {
                debug("json_string(): unpaired high surrogate.\n");
                goto _error;
            } else goto _unesc;
        }

        memcpy(& word, input + i, sizeof(word));

        /* check for non-ASCII characters */
        if (word & 0x80808080U) {
            switch (__zero_idx(word & 0x80808080U)) {
            case 3: if ( (c = input[i ++]) == '\\') goto _unesc;
                    out[pos ++] = c;
            case 2: if ( (c = input[i ++]) == '\\') goto _unesc;
                    out[pos ++] = c;
            case 1: if ( (c = input[i ++]) == '\\') goto _unesc;
                    out[pos ++] = c;
            case 0: goto _check;
            }
        } else {
            /* check for an escape sequence */
            if ( (word = __zero(word ^ 0x5c5c5c5cU)) ) {
                switch (__zero_idx(word)) {
                case 3: out[pos ++] = input[i ++];
                case 2: out[pos ++] = input[i ++];
                case 1: out[pos ++] = input[i ++];
                case 0: i ++;
                }
            } else {
                memcpy(out + pos, input + i, 4); pos += 4; i += 4;
                continue;
            }
        }

_unesc: switch (input[i ++]) {

        case '\"': out[pos ++] = '\"'; continue;

        /* QUIRK escaped single quotes, CRLF and capital U
                 unicode escape sequences */
        case '\r':
        case '\n':
        case '\'': if (! strict) { out[pos ++] = c; continue; }
        case  'U': if (strict) goto _error; else goto _utf8;

        case  '/': out[pos ++] =  '/'; continue;
        case '\\': out[pos ++] = '\\'; continue;
        case  'b': out[pos ++] = '\b'; continue;
        case  'f': out[pos ++] = '\f'; continue;
        case  'n': out[pos ++] = '\n'; continue;
        case  'r': out[pos ++] = '\r'; continue;
        case  't': out[pos ++] = '\t'; continue;

_utf8:  case  'u': {
            /* convert the 4 hexadecimal digits to a unicode codepoint */
            uint32_t codepoint = (
                ((9 * ((input[i] >> 6)) + (input[i] & 0xf)) << 12) |
                ((9 * ((input[i + 1] >> 6)) + (input[i + 1] & 0xf)) << 8) |
                ((9 * ((input[i + 2] >> 6)) + (input[i + 2] & 0xf)) << 4) |
                 (9 * ((input[i + 3] >> 6)) + (input[i + 3] & 0xf))
            );

            i += 4;

            if (codepoint <= 0x7f) {
                /* 1-byte ASCII (0|xxxxxxx) */
                out[pos ++] = (char) codepoint;
            } else if (codepoint <= 0x7ff) {
                /* 2-byte sequence (110|xxxxx 10|xxxxxx) */
                out[pos ++] = (char) (0xc0 | (codepoint >> 6));
                out[pos ++] = (char) (0x80 | (codepoint & 0x3f));
            } else if ((codepoint & 0xfc00) == 0xd800) {
                /* high surrogate */
                if (surrogate) {
                    debug("json_string(): consecutive high surrogates.\n");
                    goto _error;
                }
                surrogate = codepoint;
            } else if ((codepoint & 0xfc00) == 0xdc00) {
                /* low surrogate */
                if (! surrogate) {
                    debug("json_string(): unpaired low surrogate.\n");
                    goto _error;
                }

                /* combine both surrogates */
                codepoint = (
                    0x10000 + ((surrogate - 0xd800) << 10) + codepoint - 0xdc00
                );
                surrogate = 0;

                /* 4-byte sequence (1110|xxxx 10|xxxxxx 10|xxxxxx 10|xxxxxx) */
                if (likely(codepoint <= 0x10ffff)) {
                    out[pos ++] = (char) (0xf0 | ((codepoint >> 18) & 0x07));
                    out[pos ++] = (char) (0x80 | ((codepoint >> 12) & 0x3f));
                    out[pos ++] = (char) (0x80 | ((codepoint >> 6) & 0x3f));
                    out[pos ++] = (char) (0x80 | (codepoint & 0x3f));
                } else {
                    debug(
                        "json_string(): invalid escaped codepoint: 0x%06x.\n",
                        codepoint
                    );
                    goto _error;
                }
            } else if (likely((codepoint & 0xf800) ^ 0xd800)) {
                /* 3-byte sequence (1110|xxxx 10|xxxxxx 10|xxxxxx) */
                out[pos ++] = (char) (0xe0 | (codepoint >> 12));
                out[pos ++] = (char) (0x80 | ((codepoint >> 6) & 0x3f));
                out[pos ++] = (char) (0x80 | (codepoint & 0x3f));
            }
        } continue;

        default: goto _error;
        }

_check: if (((c = input[i]) & 0xe0) == 0xc0) {
            if ((c & 0xfe) == 0xc0) {
                debug("json_string(): overlong 2-byte sequence.\n");
                goto _error;
            }
            if ( (i + 1 < len) && (input[i + 1] & 0xc0) == 0x80) {
                out[pos ++] = input[i ++];
                out[pos ++] = input[i ++];
            } else goto _error;
        } else if (((c = input[i]) & 0xf0) == 0xe0) {
            if ( (i + 2 < len) &&
                 ( (input[i + 1] & 0xc0) == 0x80) &&
                 ( (input[i + 2] & 0xc0) == 0x80) ) {
                if (unlikely(c == 0xe0 && (input[i + 1] & 0xe0) == 0x80)) {
                    debug("json_string(): overlong 3-byte sequence.\n");
                    goto _error;
                }
                if (unlikely(c == 0xed && (input[i + 1] & 0xe0) == 0xa0)) {
                    debug("json_string(): unescaped UTF-16 surrogate.\n");
                    goto _error;
                }
                out[pos ++] = input[i ++];
                out[pos ++] = input[i ++];
                out[pos ++] = input[i ++];
            } else goto _error;
        } else if (((c = input[i]) & 0xf8) == 0xf0) {
            if ( (i + 3 < len) &&
                 ( (input[i + 1] & 0xc0) == 0x80) &&
                 ( (input[i + 2] & 0xc0) == 0x80) &&
                 ( (input[i + 3] & 0xc0) == 0x80) ) {
                if (unlikely(c == 0xf0 && (uint8_t) input[i + 1] < 0x90)) {
                    debug("json_string(): overlong 4-byte sequence.\n");
                    goto _error;
                }
                if (unlikely(c == 0xf4 && (uint8_t) input[i + 1] > 0x8f)) {
                    debug("json_string(): invalid codepoint.\n");
                    goto _error;
                }
                out[pos ++] = input[i ++];
                out[pos ++] = input[i ++];
                out[pos ++] = input[i ++];
                out[pos ++] = input[i ++];
            }  else goto _error;
        } else goto _error;
    }

    if (surrogate) {
        debug("json_string(): lone high surrogate.\n");
        goto _error;
    }

    int continuation = 0;
    uint8_t byte = 0;

    /* remaining characters */
    switch (len - i) {
    case 3: if ( (byte = input[i ++]) == '\\') goto _unesc;
            if (byte & 0x80) {
                continuation = (
                    ((byte & 0xe0) == 0xc0) - ((byte & 0xfe) == 0xc0) +
                    ((byte & 0xf0) == 0xe0) * 2
                );
                if (! continuation) goto _error;
            }
            out[pos ++] = byte;
    case 2: if ( (c = input[i ++]) == '\\') {
                if (continuation) goto _error;
                goto _unesc;
            } else if (c & 0x80) {
                if ((c & 0xc0) == 0x80) {
                    if (! continuation --) goto _error;
                    if ( (byte == 0xe0 && (c & 0xe0) == 0x80) ||
                         (byte == 0xed && (c & 0xe0) == 0xa0) ) {
                        debug(
                            "json_string(): ill-formed multi-byte sequence.\n"
                        );
                        goto _error;
                    }
                } else {
                    continuation = ( (c & 0xe0) == 0xc0) - ((c & 0xfe) == 0xc0);
                    if (! continuation) goto _error;
                }
            }
            out[pos ++] = c;
    case 1: if ( (c = input[i ++]) == '\\') goto _error;
            if (c & 0x80) {
                if ((c & 0xc0) == 0x80) {
                    if (! continuation) goto _error;
                } else goto _error;
            }
            out[pos ++] = c;
    default:
            out[pos] = '\0';
    }

    return pos;

_error:
    debug(
        "json_string(): illegal character 0x%02x at %zu.\n",
        (uint8_t) input[i], i
    );
    return -1;
}

/* -------------------------------------------------------------------------- */

static double json_number(const char *s, size_t len, int neg, int rad, int exp)
{
    uint64_t n = 0;
    int32_t e = 0;
    int i = 0, digits = 0, l = 0, neg_exp = 0;
    double ret;

    l = (rad) ? rad : (exp) ? exp : len;

    /* read the decimal part */
    for (digits = neg; digits < l; digits ++)
        n = n * 10 + (s[digits] - '0');

    /* read the fractional part, if any */
    if (rad ++) {
        l = (exp) ? exp : len;
        for (i = rad; i < l; i ++)
            n = n * 10 + (s[i] - '0');
        e = (int32_t) rad - l;
    }

    /* zero */
    if (n == 0) return (neg) ? -0.0 : 0.0;

    /* read the exponent part, if any */
    if (exp ++) {
        int32_t exp_number = 0;

        l = len;

        /* check if there is a sign */
        if (s[exp] == '-') {
            neg_exp = 1; exp ++;
        } else if (s[exp] == '+') exp ++;

        /* check if the exponent is too long */
        if (len - exp >= 4) {
            if (neg_exp)
                return (neg) ? -0.0 : 0.0;
            else
                return (neg) ? -INFINITY : INFINITY;
        }

        for (i = exp; i < l; i ++)
            exp_number = 10 * exp_number + (s[i] - '0');

        e += (neg_exp) ? -exp_number : exp_number;

        if (unlikely(digits + e <= -324)) return (neg) ? -0.0 : 0.0;
        if (unlikely(digits + e >=  310)) return (neg) ? -INFINITY : INFINITY;
    }

    /* integer */
    if (e == 0) {
        ret = (double) n;
        return (neg) ? -ret : ret;
    }

    if ( (e >= -22) && (e <= 22) && (n <= 9007199254740991ULL) ) {
        /* losslessly convert to double (0 <= n <= 2^53 - 1) */
        ret = (double) n;

        /* multiplication will produce correctly rounded values */
        if (e < 0)
            ret *= DOUBLE_POW10_INV[-e];
        else
            ret *= DOUBLE_POW10[e];

        return (neg) ? -ret : ret;
    } else {
        /* convert to binary floating-point and check if the result is exact */
        uint64_t n2, ieee;
        int32_t e2;
        uint32_t ieee_e2;
        int trailing_zeros;

        if (e >= 0) {
            /* The length of n * 10^e in bits is:
               log2(n * 10^e) = log2(n) + e log2(10) = log2(n) + e + e * log2(5)
               We want to compute the DOUBLE_MANTISSA_BITS + 1 top-most bits
               (+1 for the implicit leading one in IEEE format).
               We therefore choose a binary output exponent of
               log2(n * 10^e) - (DOUBLE_MANTISSA_BITS + 1).
               We use floor(log2(5^e10)) so that we get at least this many bits
            */
            e2 = FLOOR_LOG2(n) + e + LOG2_POW5(e) - (DOUBLE_MANTISSA_BITS + 1);

            /* compute [n * 10^e / 2^e2] = [n * 5^e / 2^(e2 - e)]
               using the DOUBLE_POW5_SPLIT table */
            i = e2 - e - CEIL_LOG2_POW5(e) + DOUBLE_POW5_BITCOUNT;
            n2 = mul_shift64(n, DOUBLE_POW5_SPLIT[e], i);

            /* check if the result is exact */
            trailing_zeros = (
                (e2 < e) ||
                (e2 - e < 64 && __is_pow2_multiple(n, e2 - e))
            );
        } else {
            /* for negative exponent, adjust calculation with inverse powers of 5 */
            e2 = FLOOR_LOG2(n) + e - CEIL_LOG2_POW5(-e) - (DOUBLE_MANTISSA_BITS + 1);
            i = e2 - e + CEIL_LOG2_POW5(-e) - 1 + DOUBLE_POW5_INV_BITCOUNT;
            n2 = mul_shift64(n, DOUBLE_POW5_INV_SPLIT[-e], i);
            trailing_zeros = __is_pow5_multiple(n, -e);
        }

        /* compute the final IEEE exponent */
        ieee_e2 = (uint32_t) max32(0, e2 + DOUBLE_EXPONENT_BIAS + FLOOR_LOG2(n2));

        if (ieee_e2 > 0x7fe) {
            /* exponent is too large, return +/-Infinity */
            ieee = (
                (((uint64_t) neg) << (DOUBLE_EXPONENT_BITS + DOUBLE_MANTISSA_BITS)) |
                (0x7ffULL << DOUBLE_MANTISSA_BITS)
            );
            memcpy(& ret, & ieee, sizeof(ret));
        } else {
            /* compute how much n2 needs to be shifted */
            int32_t shift = (
                (ieee_e2 == 0) ? 1 : ieee_e2
            ) - e2 - DOUBLE_EXPONENT_BIAS - DOUBLE_MANTISSA_BITS;

            uint64_t last_removed_bit = (n2 >> (shift - 1)) & 1;

            /* recompute using the exact output exponent ieee_e2 */
            trailing_zeros &= (n2 & ((1ull << (shift - 1)) - 1)) == 0;

            /* rounding up is necessary if the exact value is more than 0.5
               above the value computed, which is equivalent to checking if
               the last removed bit is 1 and whether the value was not just
               trailing zeros */
            int round_up = (
                (last_removed_bit != 0) &&
                (! trailing_zeros || (((n2 >> shift) & 1) != 0))
            );

            ieee = (n2 >> shift) + round_up;
            ieee &= (1ULL << DOUBLE_MANTISSA_BITS) - 1;
            ieee_e2 += (ieee == 0 && round_up);
            ieee |= (
                (((uint64_t) neg) << DOUBLE_EXPONENT_BITS) |
                (uint64_t) ieee_e2
            ) << DOUBLE_MANTISSA_BITS;

            memcpy(& ret, & ieee, sizeof(ret));
        }
    }

    return ret;
}

/* -------------------------------------------------------------------------- */

static int path_append(String *path, const char *key, size_t len, int strict)
{
    size_t slen = 0;
    ssize_t ret = 0;

    if (path->count)
        slen = last_token(path)->data - path->data + last_token(path)->len;

    /* make room for the new data */
    if (string_extend(path, slen + len + 1) == -1) return -1;

    /* copy the data */
    if ( (ret = json_string(path->data + slen, key, len, strict)) == -1)
        return -1;
    path->len = slen + ret;

    /* append the token */
    if (! string_add_token(path, slen, slen + ret)) return -1;

    return 0;
}

/* -------------------------------------------------------------------------- */

static void trie_destructor(Variant v)
{
    if (is_pointer(v) || _is_object(v)) free(v.value.pointer);
}

/* -------------------------------------------------------------------------- */

static inline Trie_Leaf *make_leaf(const char *key, size_t len, Variant val)
{
    Trie_Leaf *leaf = NULL;

    if (! (leaf = malloc(sizeof(*leaf) + len + 1)) ) return NULL;

    memcpy(leaf->key, key, len);
    leaf->key[len] = '\0';
    leaf->len = len;
    leaf->val = val;

    return leaf;
}

/* -------------------------------------------------------------------------- */

static inline Batch *batch_open(JSONPath_Context *context)
{
    Batch *batch = context->mempool;

    if (! batch) {
        if (! (batch = malloc(sizeof(*batch) + (8 * sizeof(Trie_Leaf *))))) {
            perror(ERR(json_write, malloc));
            return NULL;
        }
        batch->capacity = 8;
    } else context->mempool = context->mempool->next;

    batch->next = NULL;
    batch->depth = context->path->count;
    batch->count = 0;
    if (context->path->count > 3) {
        batch->prefix_len = (
            context->path->tokens[2].data -
            context->path->data
        );
    } else batch->prefix_len = 0;

    return batch;
}

/* -------------------------------------------------------------------------- */

static void batch_init(JSONPath_Context *context)
{
    if (context->current) {
        /* try to reuse the current batch */
        if (context->path->count == context->current->depth) {
            trie_insert_prefix_list(
                context->tree,
                context->current->prefix_len,
                context->current->leaves,
                context->current->count
            );
            context->current->count = 0;
            context->current->next = NULL;
            return;
        }

        /* push the current batch */
        context->current->next = context->batches;
        context->batches = context->current;
        if (! context->tailptr) context->tailptr = context->current;
    }

    context->current = batch_open(context);
}

/* -------------------------------------------------------------------------- */

static void batch_exit(JSONPath_Context *context)
{
    if (context->path->count > 1) {
        context->current->next = NULL;

        /* enqueue the current batch */
        if (context->batches) {
            context->tailptr->next = context->current;
            context->tailptr = context->current;
        } else {
            context->batches = context->current;
            context->tailptr = context->current;
        }

        /* pop a batch */
        if ( (context->current = context->batches) ) {
            if (! (context->batches = context->batches->next) )
                context->tailptr = NULL;
            context->current->next = NULL;
        }
    } else if (context->path->count == 1) {
        Batch *next = NULL, *batch = context->current;
        context->current->next = context->batches;

        for (context->current = NULL; batch; batch = next) {
            trie_insert_prefix_list(
                context->tree,
                batch->prefix_len,
                batch->leaves,
                batch->count
            );
            next = batch->next;
            batch->next = context->mempool;
            context->mempool = batch;
        }

        context->batches = NULL;
        context->tailptr = NULL;
    }
}

/* -------------------------------------------------------------------------- */

static void batch_add(JSONPath_Context *context, Variant val)
{
    Trie_Leaf *leaf = NULL;

    if (unlikely(! context->current)) goto _i;

    if (context->current->count == context->current->capacity) {
        Batch *new = NULL;
        int new_capacity = 2 * context->current->capacity;
        new = realloc(
            context->current,
            sizeof(*new) + (new_capacity) * sizeof(Trie_Leaf *)
        );
        if (! new) goto _i;
        new->capacity = new_capacity;
        context->current = new;
    }

    if (! (leaf = make_leaf(context->path->data, context->path->len, val)) )
        goto _i;

    context->current->leaves[context->current->count ++] = leaf;

    return;

_i: trie_insert(context->tree, context->path->data, context->path->len, val);
}

/* -------------------------------------------------------------------------- */

INTERNAL int json_init(int type, JSON_Parser *ctx)
{
    JSONPath_Context *context = ctx->context;

    if (ctx->key.current) {
        int ret = path_append(
            context->path,
            ctx->key.current,
            ctx->key.len,
            ctx->strict
        );
        if (ret == -1) return -1;
    }

    if (type == JSON_ARRAY) {
        /* add the index token */
        string_push_token(context->path, "0", 1);
        context->count = 0;
    }

    batch_init(context);

    return 0;
}

/* -------------------------------------------------------------------------- */

INTERNAL int json_data(String *data, JSON_Parser *ctx)
{
    JSONPath_Context *context = ctx->context;
    Variant var = { 0 };
    double number = 0.0;
    char *string = NULL;

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
    } else if (IS_STRING(data)) {
        ssize_t len = 0;

        if (! (string = malloc((data->len + 1) * sizeof(*string))) ) {
            perror(ERR(json_data, malloc));
            return -1;
        }

        len = json_string(string, data->data, data->len, ctx->strict);
        if (unlikely(len == -1)) {
            free(string);
            return -1;
        }

        /* store the string CRC7 and first byte for faster retrieval/sorting */
        if (likely(var.metadata.fields.dword = len))
            var.metadata.fields.type = _crc7(string, len);
        var.metadata.fields.type |= _VALUE_OBJECT;
        var.metadata.fields.byte = *string;
        var.value.pointer = string;
    }

    if (ctx->parent == JSON_ARRAY) {
        if (unlikely(! context->count)) string_merge(context->path, "/", 1);
        batch_add(context, var);
        context->count = increment_index(context->path);
    } else if (ctx->key.current) {
        int ret = path_append(
            context->path,
            ctx->key.current,
            ctx->key.len,
            ctx->strict
        );
        if (ret == -1) return -1;
        string_merge(context->path, "/", 1);
        /* fast suffix access */
        var.metadata.fields.word = context->path->len - ctx->key.len;
        batch_add(context, var);
        if (likely(context->path->count))
            string_suppr_token(context->path, context->path->count - 1);
        if (unlikely(! context->path->count)) context->path->len = 0;
    }

    if (! ctx->parent) {
        /* TODO */
        debug("End of JSON document!\n");
    }

    return (! ctx->parent);
}

/* -------------------------------------------------------------------------- */

INTERNAL int json_exit(int type, JSON_Parser *ctx)
{
    Variant var = { 0 };
    JSONPath_Context *context = ctx->context;

    if (type == JSON_OBJECT) batch_exit(context);

    if (type == JSON_ARRAY) {
        /* remove the index token */
        string_suppr_token(context->path, context->path->count - 1);

        if (! context->count) {
            /* insert nonetheless */
            string_merge(context->path, "/", 1);
            trie_insert(
                context->tree,
                context->path->data,
                context->path->len,
                var
            );
        }
    }

    /* remove the array name */
    if (ctx->parent == JSON_OBJECT && context->path->count)
        string_suppr_token(context->path, context->path->count - 1);

    if (unlikely(! context->path->count)) context->path->len = 0;

    if (ctx->parent == JSON_ARRAY)
        context->count = increment_index(context->path);

    if (! ctx->parent) {
        /* TODO */
        debug("End of JSON document!\n");
    }

    return (! ctx->parent);
}

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_init(JSON_Parser *ctx)
{
    JSONPath_Context *private_context = NULL;

    if (! ctx) {
        debug("jsonpath_init(): bad parameters.\n");
        return -1;
    }

    if (! (private_context = malloc(sizeof(*private_context))) ) {
        perror(ERR(jsonpath_init, malloc)); goto _err_alloc;
    }

    if (! (private_context->tree = trie_alloc(trie_destructor)) )
        goto _err_trie;

    if (! (private_context->path = string_alloc(NULL, 0)) )
        goto _err_path;

    private_context->current = NULL;
    private_context->mempool = NULL;
    private_context->batches = NULL;
    private_context->tailptr = NULL;

    ctx->context = private_context;
    ctx->key.current = NULL;
    ctx->key.len = 0;
    ctx->primitive.data = 0;
    ctx->init = json_init;
    ctx->data = json_data;
    ctx->exit = json_exit;

    return 0;

_err_path:
    trie_free(private_context->tree);
_err_trie:
    free(private_context);
_err_alloc:
    return -1;
}

/* -------------------------------------------------------------------------- */

int trie_print(const char *k, size_t len, Variant v)
{
    printf("%.*s = ", (int) len, k);

    switch (v.metadata.fields.type) {
    case VALUE_NULL: printf("<NULL>\n"); break;
    case VALUE_STRING: break;
    case VALUE_INTEGER: printf("0x%llx\n", variant_to_integer(v)); break;
    case VALUE_BOOLEAN: printf("%s\n", (variant_to_boolean(v)) ? "TRUE" : "FALSE"); break;
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
    trie_foreach(context->tree, trie_print);
    return 0;
}

/* -------------------------------------------------------------------------- */

ASKL_API int jsonpath_free(JSON_Parser *ctx)
{
    JSONPath_Context *context = NULL;
    Batch *next = NULL, *batch = NULL;

    if (! ctx) {
        debug("jsonpath_free(): bad parameters.\n");
        return -1;
    }

    ctx->init = NULL;
    ctx->data = NULL;
    ctx->exit = NULL;
    context = ctx->context;
    ctx->context = NULL;

    context->tree = trie_free(context->tree);
    context->path = string_free(context->path);

    /* destroy pending batches */
    if (context->current) {
        context->current->next = context->batches;
        context->batches = context->current;
    }

    for (batch = context->batches ; batch; batch = next) {
        unsigned int i = 0;
        next = batch->next;
        for (i = 0; i < batch->count; i ++) {
            trie_destructor(batch->leaves[i]->val);
            free(batch->leaves[i]);
        }
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
#endif /* _ENABLE_PARSER && _ENABLE_TRIE */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif /* _ENABLE_JSON */
/* -------------------------------------------------------------------------- */
