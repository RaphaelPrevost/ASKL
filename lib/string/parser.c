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
                if (json->parent) {
                    if (IS_OBJECT(json->parent) && (json->parent->count & 1))
                        state |= _KEY;
                }

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
#else
/* -------------------------------------------------------------------------- */

#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif /* _ENABLE_JSON */
/* -------------------------------------------------------------------------- */
