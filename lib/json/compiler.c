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

/* a query is compiled into a flat program: a sequence of segments, each
   followed by its selectors, ended by OP_END; a filter selector is followed
   by its expression in postfix order, ended by OP_END, and a query nested
   in an expression is laid out inline the same way */
#define OP_END       0
#define OP_SEG       1  /* len: selectors, next: past them, flags: SEG_DESC */
#define OP_NAME      2  /* a: pool offset, len: escaped name, flags */
#define OP_INDEX     3  /* a: index */
#define OP_WILD      4
#define OP_SLICE     5  /* a: start, b: end, c: step, flags */
#define OP_FILTER    6  /* next: past the expression */
#define OP_LIT       7  /* flags: kind, d: number, a/len: string */
#define OP_QUERY     8  /* flags: root, singularity, mode, next: past it */
#define OP_NOT       9
#define OP_AND      10
#define OP_OR       11
#define OP_EQ       12
#define OP_NE       13
#define OP_LT       14
#define OP_LE       15
#define OP_GT       16
#define OP_GE       17
#define OP_LENGTH   18
#define OP_MATCH    19  /* a: compiled regex, or -1 */
#define OP_SEARCH   20
#define OP_PAREN  0xff  /* operator stack only */

#define SEG_DESC       0x1
#define SEG_FILTER     0x2  /* every selector of the segment is a filter */
#define NAME_INDEX     0x1  /* the name could be an index */
#define SLICE_START    0x1
#define SLICE_END      0x2

#define LIT_NULL         0
#define LIT_FALSE        1
#define LIT_TRUE         2
#define LIT_NUM          3
#define LIT_STR          4

#define QUERY_ABS      0x1
#define QUERY_SINGULAR 0x2
#define QUERY_MODE     0xc
#define QUERY_EXISTS   0x0  /* a test: does the query select anything */
#define QUERY_VALUE    0x4  /* the value of a singular query */
#define QUERY_COUNT    0x8  /* count(): the number of nodes selected */
#define QUERY_VALUEOF  0xc  /* value(): the value of the only node */

#define NONE UINT32_MAX

typedef struct _Op {
    uint8_t code;
    uint8_t flags;
    uint16_t len;
    uint32_t next;
    int64_t a;
    int64_t b;
    union {
        int64_t c;
        double d;
    } x;
} _Op;

struct _JSONPath_Query {
    _Op *ops;
    uint32_t count;
    uint32_t size;
    char *pool;
    uint32_t plen;
    uint32_t psize;
    #ifdef HAS_PCRE
    pcre **regex;
    uint32_t nregex;
    uint32_t sregex;
    #endif
};

/**
 * @ingroup json
 * @struct _JSONPath_Query
 *
 * Compiled JSONPath program and its literal storage.
 *
 * @b private @ref ops compiled instructions
 * @b private @ref count number of instructions
 * @b private @ref size allocated instruction capacity
 * @b private @ref pool NUL-terminated names and string literals
 * @b private @ref plen used bytes in @ref pool
 * @b private @ref psize allocated bytes in @ref pool
 * @b private @ref regex the regular expressions compiled with the query
 * @b private @ref nregex number of compiled regular expressions
 * @b private @ref sregex allocated regular expression capacity
 */

/* -------------------------------------------------------------------------- */
/* compiler                                                                   */
/* -------------------------------------------------------------------------- */

/* context types */
#define C_SEGS    0  /* the segments of a query */
#define C_BRACKET 1  /* the selectors of a bracketed segment */
#define C_EXPR    2  /* a logical expression: a filter or an argument */
#define C_ARGS    3  /* the arguments of a function */

/* compiler operand types */
#define T_LIT     0
#define T_QUERY   1
#define T_VALUE   2
#define T_LOGICAL 3

/* required expression result types */
#define X_LOGICAL 0
#define X_VALUE   1
#define X_NODES   2

#define FN_LENGTH 0
#define FN_COUNT  1
#define FN_VALUE  2
#define FN_MATCH  3
#define FN_SEARCH 4

typedef struct _Type {
    uint8_t type;
    uint32_t op;
} _Type;

typedef struct _Ctx {
    uint8_t kind;
    uint8_t state;
    uint8_t flags;
    uint8_t last;
    uint32_t op;
    uint32_t count;
    uint32_t filters;
    uint32_t base;
    uint32_t tbase;
} _Ctx;

/**
 * @ingroup json
 * @struct _Ctx
 *
 * One context on the compiler's non-recursive parse stack.
 *
 * @b private @ref kind context type (C_)
 * @b private @ref state context-specific parser state
 * @b private @ref flags C_SEGS: whether the query is still singular;
 *                       C_EXPR: required result type (X_)
 * @b private @ref last C_BRACKET: last selector type;
 *                      C_EXPR and C_ARGS: function being called
 * @b private @ref op C_SEGS: enclosing OP_QUERY;
 *                    C_BRACKET: OP_SEG;
 *                    C_EXPR: OP_FILTER, or NONE
 * @b private @ref count C_BRACKET: selector count;
 *                       C_ARGS: argument count;
 *                       C_EXPR: open-parenthesis count
 * @b private @ref filters number of filter selectors in C_BRACKET
 * @b private @ref base base of the operator stack for C_EXPR
 * @b private @ref tbase base of the type stack for C_EXPR
 */

typedef struct _Compiler {
    const char *s;
    size_t len;
    size_t pos;
    JSONPath_Query *q;
    _Ctx *ctx;
    uint32_t nctx;
    uint32_t actx;
    uint8_t *stack;
    uint32_t nstack;
    uint32_t astack;
    _Type *types;
    uint32_t ntypes;
    uint32_t atypes;
} _Compiler;

/**
 * @ingroup json
 * @struct _Compiler
 *
 * State of the non-recursive JSONPath compiler.
 *
 * Parsing contexts replace recursive descent; @ref stack holds pending
 * operators and @ref types tracks expression operand types. Instructions and
 * literals are emitted directly into @ref q.
 */

static const struct {
    const char *name;
    uint8_t arity;
    uint8_t param[2];
    uint8_t result;
} _fn[] = {
    { "length", 1, { X_VALUE, X_VALUE }, T_VALUE },
    { "count",  1, { X_NODES, X_NODES }, T_VALUE },
    { "value",  1, { X_NODES, X_NODES }, T_VALUE },
    { "match",  2, { X_VALUE, X_VALUE }, T_LOGICAL },
    { "search", 2, { X_VALUE, X_VALUE }, T_LOGICAL }
};

#define SYNTAX(c, m) \
    debug("jsonpath_query_alloc(): " m " at %u.\n", (unsigned) (c)->pos)

#define AT(c) ((c)->pos < (c)->len ? (c)->s[(c)->pos] : '\0')

/* -------------------------------------------------------------------------- */

static int _growth(void *array, uint32_t *size, uint32_t need, size_t item)
{
    /** @brief grow an array to hold at least @p need items */

    void *new = NULL, **slot = array;
    uint32_t n = (*size) ? *size : 16;

    while (n < need) {
        if (n > UINT32_MAX / 2) {
            debug("jsonpath: too much to hold.\n");
            return -1;
        }
        n *= 2;
    }

    if (! (new = realloc(*slot, (size_t) n * item)) ) {
        perror(ERR(jsonpath, realloc));
        return -1;
    }

    *slot = new; *size = n;

    return 0;
}

/* -------------------------------------------------------------------------- */

static inline int _grow(void *array, uint32_t *size, uint32_t need,
                        size_t item)
{
    return (need <= *size) ? 0 : _growth(array, size, need, item);
}

/* -------------------------------------------------------------------------- */

static _Op *_emit(_Compiler *c, uint8_t code)
{
    _Op *op = NULL;

    if (_grow(& c->q->ops, & c->q->size, c->q->count + 1, sizeof(*op)) == -1)
        return NULL;

    op = & c->q->ops[c->q->count ++];
    memset(op, 0, sizeof(*op));
    op->code = code;

    return op;
}

/* -------------------------------------------------------------------------- */

static int _pool(_Compiler *c, const char *s, uint32_t len)
{
    if (_grow(& c->q->pool, & c->q->psize, c->q->plen + len, 1) == -1)
        return -1;

    memcpy(c->q->pool + c->q->plen, s, len);
    c->q->plen += len;

    return 0;
}

/* -------------------------------------------------------------------------- */

static _Ctx *_open(_Compiler *c, uint8_t kind)
{
    _Ctx *ctx = NULL;

    if (_grow(& c->ctx, & c->actx, c->nctx + 1, sizeof(*ctx)) == -1)
        return NULL;

    ctx = & c->ctx[c->nctx ++];
    memset(ctx, 0, sizeof(*ctx));
    ctx->kind = kind;
    ctx->op = NONE;

    return ctx;
}

/* -------------------------------------------------------------------------- */

static int _type(_Compiler *c, uint8_t type, uint32_t op)
{
    if (_grow(& c->types, & c->atypes, c->ntypes + 1, sizeof(*c->types)) == -1)
        return -1;

    c->types[c->ntypes].type = type;
    c->types[c->ntypes ++].op = op;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _push(_Compiler *c, uint8_t code)
{
    if (_grow(& c->stack, & c->astack, c->nstack + 1, 1) == -1)
        return -1;

    c->stack[c->nstack ++] = code;

    return 0;
}

/* -------------------------------------------------------------------------- */

static void _space(_Compiler *c)
{
    while (c->pos < c->len && (c->s[c->pos] == ' ' || c->s[c->pos] == '\t' ||
           c->s[c->pos] == '\n' || c->s[c->pos] == '\r')) c->pos ++;
}

/* -------------------------------------------------------------------------- */

static size_t _name(_Compiler *c)
{
    /** @brief consume a member name shorthand and return its length */

    size_t i = c->pos;
    uint8_t b = 0;

    while (i < c->len) {
        b = c->s[i];
        if (! ((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') ||
               b == '_' || b >= 0x80 ||
               (i > c->pos && b >= '0' && b <= '9')) ) break;
        i ++;
    }

    i -= c->pos; c->pos += i;

    return i;
}

/* -------------------------------------------------------------------------- */

static int _int(_Compiler *c, int64_t *value)
{
    /** @brief parse an integer */

    size_t i = c->pos;
    int64_t v = 0;
    int neg = 0;

    if (i < c->len && c->s[i] == '-') { neg = 1; i ++; }

    if (i >= c->len || c->s[i] < '0' || c->s[i] > '9') {
        if (! neg) return 0;
        SYNTAX(c, "expected an integer");
        return -1;
    }

    if (c->s[i] == '0') i ++;
    else while (i < c->len && c->s[i] >= '0' && c->s[i] <= '9') {
        v = v * 10 + (c->s[i ++] - '0');
        if (v > 9007199254740991) {
            SYNTAX(c, "integer out of range");
            return -1;
        }
    }

    if (neg && ! v) {
        SYNTAX(c, "-0 is not an integer");
        return -1;
    }

    c->pos = i; *value = (neg) ? -v : v;

    return 1;
}

/* -------------------------------------------------------------------------- */

static int _number(_Compiler *c, double *value)
{
    /** @brief parse a number using the parser's numeric conversion */

    const char *s = c->s + c->pos;
    size_t i = c->pos, digits = 0, mantissa = 0;
    int neg = 0, rad = 0, exp = 0;

    if (i < c->len && c->s[i] == '-') { neg = 1; i ++; }
    if (i < c->len && c->s[i] == '0') i ++;
    else while (i < c->len && c->s[i] >= '0' && c->s[i] <= '9') {
        i ++; digits ++; mantissa ++;
    }
    if (! digits && (i == c->pos || c->s[i - 1] != '0')) goto _error;

    if (i < c->len && c->s[i] == '.') {
        rad = i ++ - c->pos;
        for (digits = 0; i < c->len && c->s[i] >= '0' && c->s[i] <= '9'; i ++) {
            if (mantissa || c->s[i] != '0') mantissa ++;
            digits ++;
        }
        if (! digits) goto _error;
    }

    if (i < c->len && (c->s[i] == 'e' || c->s[i] == 'E')) {
        exp = i ++ - c->pos;
        if (i < c->len && (c->s[i] == '+' || c->s[i] == '-')) i ++;
        for (digits = 0; i < c->len && c->s[i] >= '0' && c->s[i] <= '9'; i ++)
            digits ++;
        if (! digits) goto _error;
    }

    /* the digits are gathered in 64 bits, as for a document */
    if (mantissa > 19) {
        SYNTAX(c, "number too precise");
        return -1;
    }

    *value = json_number(s, i - c->pos, neg, rad, exp);
    c->pos = i;

    return 0;

_error:
    SYNTAX(c, "malformed number");
    return -1;
}

/* -------------------------------------------------------------------------- */

static int _hex(const char *s)
{
    int i = 0, v = 0;
    char h = 0;

    for (i = 0; i < 4; i ++) {
        h = s[i];
        if (h >= '0' && h <= '9') v = (v << 4) | (h - '0');
        else if (h >= 'a' && h <= 'f') v = (v << 4) | (h - 'a' + 10);
        else if (h >= 'A' && h <= 'F') v = (v << 4) | (h - 'A' + 10);
        else return -1;
    }

    return v;
}

/* -------------------------------------------------------------------------- */

static int _string(_Compiler *c, uint32_t *off, uint32_t *len)
{
    /** @brief decode a string literal into the pool */

    char quote = c->s[c->pos ++], out[4];
    size_t i = c->pos;
    uint32_t start = c->q->plen;
    int32_t cp = 0, lo = 0;
    uint8_t b = 0;
    unsigned int n = 0;

    while (i < c->len && c->s[i] != quote) {
        b = c->s[i];
        if (b < 0x20) {
            SYNTAX(c, "control character in a string");
            return -1;
        }
        if (b != '\\') {
            if (_pool(c, c->s + i, 1) == -1) return -1;
            i ++; continue;
        }
        if (++ i >= c->len) break;
        c->pos = i;
        switch (c->s[i ++]) {
        case 'b': b = '\b'; break;
        case 'f': b = '\f'; break;
        case 'n': b = '\n'; break;
        case 'r': b = '\r'; break;
        case 't': b = '\t'; break;
        case '/': b = '/'; break;
        case '\\': b = '\\'; break;
        case '"': if (quote == '"') { b = '"'; break; } goto _escape;
        case '\'': if (quote == '\'') { b = '\''; break; } goto _escape;
        case 'u':
            if (i + 4 > c->len || (cp = _hex(c->s + i)) == -1) goto _escape;
            i += 4;
            if (cp >= 0xdc00 && cp <= 0xdfff) goto _surrogate;
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (i + 6 > c->len || c->s[i] != '\\' || c->s[i + 1] != 'u' ||
                    (lo = _hex(c->s + i + 2)) < 0xdc00 || lo > 0xdfff)
                    goto _surrogate;
                cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                i += 6;
            }
            if (cp < 0x80) { out[0] = cp; n = 1; }
            else if (cp < 0x800) {
                out[0] = 0xc0 | (cp >> 6);
                out[1] = 0x80 | (cp & 0x3f); n = 2;
            } else if (cp < 0x10000) {
                out[0] = 0xe0 | (cp >> 12);
                out[1] = 0x80 | ((cp >> 6) & 0x3f);
                out[2] = 0x80 | (cp & 0x3f); n = 3;
            } else {
                out[0] = 0xf0 | (cp >> 18);
                out[1] = 0x80 | ((cp >> 12) & 0x3f);
                out[2] = 0x80 | ((cp >> 6) & 0x3f);
                out[3] = 0x80 | (cp & 0x3f); n = 4;
            }
            if (_pool(c, out, n) == -1) return -1;
            continue;
        default: goto _escape;
        }
        out[0] = b;
        if (_pool(c, out, 1) == -1) return -1;
    }

    if (i >= c->len) {
        SYNTAX(c, "unterminated string");
        return -1;
    }

    out[0] = '\0';
    if (_pool(c, out, 1) == -1) return -1;
    c->pos = i + 1;
    *off = start; *len = c->q->plen - start - 1;

    return 0;

_surrogate:
    SYNTAX(c, "invalid surrogate");
    return -1;
_escape:
    SYNTAX(c, "invalid escape sequence");
    return -1;
}

/* -------------------------------------------------------------------------- */

static int _emit_name(_Compiler *c, const char *s, uint32_t off, size_t len)
{
    /** @brief emit a name selector escaped as a JSON Pointer token */

    _Op *op = NULL;
    const char *src = NULL;
    char *dst = NULL;
    size_t i = 0, n = len;
    int index = (len > 0);

    src = (s) ? s : c->q->pool + off;
    for (i = 0; i < len; i ++) {
        if (src[i] == '~' || src[i] == '/') n ++;
        if (src[i] < '0' || src[i] > '9' || (! i && len > 1 && src[i] == '0'))
            index = 0;
    }

    if (n > UINT16_MAX) {
        SYNTAX(c, "name too long");
        return -1;
    }

    if (! (op = _emit(c, OP_NAME)) ) return -1;
    op->a = c->q->plen; op->len = n;
    if (index) op->flags = NAME_INDEX;

    if (_grow(& c->q->pool, & c->q->psize, c->q->plen + n + 1, 1) == -1)
        return -1;

    src = (s) ? s : c->q->pool + off;
    dst = c->q->pool + c->q->plen;
    i = 0;
    while (i < len) {
        if (src[i] == '~' || src[i] == '/') {
            dst[1] = '0' + (src[i ++] == '/'); dst[0] = '~'; dst += 2;
        } else *dst ++ = src[i ++];
    }
    *dst = '\0';
    c->q->plen += n + 1;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _segment(_Compiler *c, _Ctx *ctx)
{
    size_t mark = c->pos, n = 0;
    _Op *op = NULL;
    uint32_t seg = 0;

    _space(c);

    if (AT(c) == '[') {
        c->pos ++;
        if (! _emit(c, OP_SEG)) return -1;
        seg = c->q->count - 1;
        goto _bracket;
    }

    if (AT(c) == '.') {
        c->pos ++;
        if (! (op = _emit(c, OP_SEG)) ) return -1;
        seg = c->q->count - 1;
        if (AT(c) == '.') {
            c->pos ++;
            op->flags = SEG_DESC; ctx->flags = 0;
            if (AT(c) == '[') {
                c->pos ++;
                goto _bracket;
            }
        }
        if (AT(c) == '*') {
            c->pos ++; ctx->flags = 0;
            if (! _emit(c, OP_WILD)) return -1;
        } else if ((n = _name(c))) {
            if (_emit_name(c, c->s + c->pos - n, 0, n) == -1) return -1;
        } else {
            SYNTAX(c, "expected a member name or '*'");
            return -1;
        }
        c->q->ops[seg].len = 1;
        c->q->ops[seg].next = c->q->count;
        return 0;
    }

    /* no further segment: the query ends before the whitespace */
    c->pos = mark;
    if (! _emit(c, OP_END)) return -1;

    if (ctx->op == NONE) {
        if (c->pos != c->len) {
            SYNTAX(c, "unexpected character");
            return -1;
        }
    } else {
        c->q->ops[ctx->op].next = c->q->count;
        if (ctx->flags) c->q->ops[ctx->op].flags |= QUERY_SINGULAR;
        if (_type(c, T_QUERY, ctx->op) == -1) return -1;
    }

    c->nctx --;

    return 0;

_bracket:
    if (! (ctx = _open(c, C_BRACKET)) ) return -1;
    ctx->op = seg;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _selector(_Compiler *c, _Ctx *ctx)
{
    _Op *op = NULL;
    int64_t v = 0;
    uint32_t off = 0, len = 0;
    int ret = 0;
    char q = '\0';

    _space(c);

    if (c->pos >= c->len) {
        SYNTAX(c, "unterminated bracket");
        return -1;
    }

    q = c->s[c->pos];

    if (ctx->state) {
        /* after a selector */
        c->pos ++;
        if (q == ',') { ctx->state = 0; return 0; }
        if (q != ']') {
            SYNTAX(c, "expected ',' or ']'");
            return -1;
        }
        c->q->ops[ctx->op].len = ctx->count;
        /* mark segments containing only filter selectors */
        if (ctx->filters == ctx->count)
            c->q->ops[ctx->op].flags |= SEG_FILTER;
        c->q->ops[ctx->op].next = c->q->count;
        c->nctx --;
        if (ctx->count != 1 || (ctx->last != OP_NAME && ctx->last != OP_INDEX))
            c->ctx[c->nctx - 1].flags = 0;
        return 0;
    }

    ctx->state = 1; ctx->count ++;

    switch (q) {
    case '"':
    case '\'':
        ctx->last = OP_NAME;
        if (_string(c, & off, & len) == -1) return -1;
        return _emit_name(c, NULL, off, len);
    case '*':
        c->pos ++; ctx->last = OP_WILD;
        return (_emit(c, OP_WILD)) ? 0 : -1;
    case '?':
        c->pos ++; ctx->last = OP_FILTER; ctx->filters ++;
        if (! _emit(c, OP_FILTER)) return -1;
        off = c->q->count - 1;
        if (! (ctx = _open(c, C_EXPR)) ) return -1;
        ctx->op = off; ctx->flags = X_LOGICAL;
        ctx->base = c->nstack; ctx->tbase = c->ntypes;
        return 0;
    default: break;
    }

    /* an index or a slice */
    if ( (ret = _int(c, & v)) == -1) return -1;
    _space(c);

    if (AT(c) != ':') {
        if (! ret) {
            SYNTAX(c, "expected a selector");
            return -1;
        }
        ctx->last = OP_INDEX;
        if (! (op = _emit(c, OP_INDEX)) ) return -1;
        op->a = v;
        return 0;
    }

    ctx->last = OP_SLICE;
    if (! (op = _emit(c, OP_SLICE)) ) return -1;
    op->flags = (ret) ? SLICE_START : 0; op->a = v; op->x.c = 1;
    c->pos ++; _space(c);
    if ( (ret = _int(c, & v)) == -1) return -1;
    if (ret) { op->flags |= SLICE_END; op->b = v; _space(c); }
    if (AT(c) == ':') {
        c->pos ++; _space(c);
        if ( (ret = _int(c, & v)) == -1) return -1;
        if (ret) op->x.c = v;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static unsigned int _prec(uint8_t code)
{
    switch (code) {
    case OP_OR: return 1;
    case OP_AND: return 2;
    case OP_NOT: return 4;
    case OP_PAREN: return 0;
    default: return 3;
    }
}

/* -------------------------------------------------------------------------- */

static int _apply(_Compiler *c, _Ctx *ctx, uint8_t code)
{
    /** @brief emit an operator, checking the types of its operands */

    _Type *t = NULL;
    unsigned int i = 0, n = (code == OP_NOT) ? 1 : 2;

    if (c->ntypes < ctx->tbase + n) {
        SYNTAX(c, "missing operand");
        return -1;
    }

    t = c->types + c->ntypes - n;

    for (i = 0; i < n; i ++) {
        if (code >= OP_EQ) {
            /* a comparison: literals, values, singular queries */
            if (t[i].type == T_LOGICAL) {
                SYNTAX(c, "comparison of a logical expression");
                return -1;
            }
            if (t[i].type == T_QUERY) {
                if (! (c->q->ops[t[i].op].flags & QUERY_SINGULAR)) {
                    SYNTAX(c, "comparison of a non-singular query");
                    return -1;
                }
                c->q->ops[t[i].op].flags |= QUERY_VALUE;
            }
        } else if (t[i].type != T_LOGICAL && t[i].type != T_QUERY) {
            SYNTAX(c, "logical operator applied to a value");
            return -1;
        }
    }

    if (! _emit(c, code)) return -1;

    c->ntypes -= n - 1;
    t->type = T_LOGICAL; t->op = c->q->count - 1;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _reduce(_Compiler *c, _Ctx *ctx, unsigned int prec)
{
    /** @brief emit the pending operators of higher or equal precedence */

    uint8_t code = 0;

    while (c->nstack > ctx->base) {
        code = c->stack[c->nstack - 1];
        if (code == OP_PAREN || _prec(code) < prec) break;
        c->nstack --;
        if (_apply(c, ctx, code) == -1) return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _finish(_Compiler *c, _Ctx *ctx)
{
    /** @brief finish an expression and verify its required result type */

    _Type *t = NULL;
    _Op *op = NULL;
    uint8_t mode = 0;

    if (_reduce(c, ctx, 1) == -1) return -1;

    if (c->nstack > ctx->base) {
        SYNTAX(c, "missing ')'");
        return -1;
    }

    if (c->ntypes != ctx->tbase + 1) {
        SYNTAX(c, "expected an expression");
        return -1;
    }

    t = c->types + ctx->tbase;
    op = (t->type == T_QUERY) ? & c->q->ops[t->op] : NULL;

    switch (ctx->flags) {
    case X_LOGICAL:
        if (t->type != T_LOGICAL && ! op) {
            SYNTAX(c, "expected a logical expression");
            return -1;
        }
        break;
    case X_VALUE:
        if (t->type == T_LOGICAL || (op && ! (op->flags & QUERY_SINGULAR))) {
            SYNTAX(c, "expected a value");
            return -1;
        }
        if (op) op->flags |= QUERY_VALUE;
        break;
    default:
        if (! op) {
            SYNTAX(c, "expected a query");
            return -1;
        }
        mode = (ctx->last == FN_COUNT) ? QUERY_COUNT : QUERY_VALUEOF;
        op->flags |= mode;
    }

    if (ctx->op != NONE) {
        /* a filter expression terminates its selector */
        if (! _emit(c, OP_END)) return -1;
        c->q->ops[ctx->op].next = c->q->count;
        c->ntypes --;
    }

    c->nctx --;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _function(const char *s, size_t len)
{
    unsigned int i = 0;

    for (i = 0; i < sizeof(_fn) / sizeof(*_fn); i ++)
        if (strlen(_fn[i].name) == len && ! memcmp(_fn[i].name, s, len))
            return i;

    return -1;
}

/* -------------------------------------------------------------------------- */

static int _expression(_Compiler *c, _Ctx *ctx)
{
    _Op *op = NULL;
    const char *word = NULL;
    uint32_t off = 0, len = 0;
    size_t n = 0;
    double d = 0.0;
    uint8_t code = 0;
    int fn = 0;
    char q = '\0';

    _space(c);
    q = AT(c);

    if (! ctx->state) {
        /* an operand */
        switch (q) {
        case '!': c->pos ++; return _push(c, OP_NOT);
        case '(': c->pos ++; ctx->count ++; return _push(c, OP_PAREN);
        case '@':
        case '$':
            c->pos ++;
            if (! (op = _emit(c, OP_QUERY)) ) return -1;
            if (q == '$') op->flags = QUERY_ABS;
            off = c->q->count - 1;
            ctx->state = 1;
            if (! (ctx = _open(c, C_SEGS)) ) return -1;
            ctx->op = off; ctx->flags = 1;
            return 0;
        case '"':
        case '\'':
            if (_string(c, & off, & len) == -1) return -1;
            if (len > UINT16_MAX) {
                SYNTAX(c, "string too long");
                return -1;
            }
            if (! (op = _emit(c, OP_LIT)) ) return -1;
            op->flags = LIT_STR; op->a = off; op->len = len;
            op->b = (len) ? _crc7(c->q->pool + off, len) : 0;
            break;
        default:
            if (q == '-' || (q >= '0' && q <= '9')) {
                if (_number(c, & d) == -1) return -1;
                if (! (op = _emit(c, OP_LIT)) ) return -1;
                op->flags = LIT_NUM; op->x.d = d;
                break;
            }
            if (! (n = _name(c)) ) {
                SYNTAX(c, "expected an operand");
                return -1;
            }
            word = c->s + c->pos - n;
            if (AT(c) == '(') {
                if ( (fn = _function(word, n)) == -1) {
                    SYNTAX(c, "unknown function");
                    return -1;
                }
                c->pos ++;
                ctx->state = 1;
                if (! (ctx = _open(c, C_ARGS)) ) return -1;
                ctx->last = fn;
                return 0;
            }
            if (! (op = _emit(c, OP_LIT)) ) return -1;
            if (n == 4 && ! memcmp(word, "true", 4)) op->flags = LIT_TRUE;
            else if (n == 4 && ! memcmp(word, "null", 4)) op->flags = LIT_NULL;
            else if (n == 5 && ! memcmp(word, "false", 5))
                op->flags = LIT_FALSE;
            else {
                SYNTAX(c, "unexpected identifier");
                return -1;
            }
        }
        ctx->state = 1;
        return _type(c, T_LIT, c->q->count - 1);
    }

    /* an operator, or the end of the expression */
    switch (q) {
    case '=': code = OP_EQ; break;
    case '!': code = OP_NE; break;
    case '<': code = OP_LT; break;
    case '>': code = OP_GT; break;
    case '&': code = OP_AND; break;
    case '|': code = OP_OR; break;
    case ')':
        if (! ctx->count) return _finish(c, ctx);
        c->pos ++; ctx->count --;
        if (_reduce(c, ctx, 1) == -1) return -1;
        c->nstack --;
        if (c->types[c->ntypes - 1].type == T_QUERY)
            c->types[c->ntypes - 1].type = T_LOGICAL;
        if (c->types[c->ntypes - 1].type != T_LOGICAL) {
            SYNTAX(c, "expected a logical expression in parentheses");
            return -1;
        }
        return 0;
    case ',':
    case ']': return _finish(c, ctx);
    default:
        SYNTAX(c, "expected an operator");
        return -1;
    }

    c->pos ++;
    if (code == OP_AND || code == OP_OR || code == OP_EQ || code == OP_NE) {
        if (AT(c) != ((code == OP_AND || code == OP_OR) ? q : '=')) {
            SYNTAX(c, "malformed operator");
            return -1;
        }
        c->pos ++;
    } else if (AT(c) == '=') {
        c->pos ++; code ++;
    }

    if (_reduce(c, ctx, _prec(code)) == -1) return -1;
    ctx->state = 0;

    return _push(c, code);
}

/* -------------------------------------------------------------------------- */

#ifdef HAS_PCRE
#ifndef PCRE_UCP
#define PCRE_UCP 0
#endif

static pcre *_regex(const char *pattern, size_t len, int full)
{
    /** @brief compile an I-Regexp (RFC 9485) as a PCRE pattern */

    pcre *regex = NULL;
    char *buf = NULL, *p = NULL;
    const char *err = NULL;
    size_t i = 0;
    int off = 0, cls = 0, first = 0;

    if (! (buf = malloc(7 * len + 8)) ) {
        perror(ERR(jsonpath, malloc));
        return NULL;
    }

    p = buf;
    if (full) { memcpy(p, "^(?:", 4); p += 4; }

    for (i = 0; i < len; i ++) {
        if (pattern[i] == '\\' && i + 1 < len) {
            *p ++ = pattern[i ++]; *p ++ = pattern[i]; first = 0;
            continue;
        }
        if (cls) {
            if (pattern[i] == ']' && ! first) cls = 0;
            first = (first && pattern[i] == '^');
            *p ++ = pattern[i];
        } else if (pattern[i] == '[') {
            cls = first = 1;
            *p ++ = '[';
        } else if (pattern[i] == '.') {
            /* a dot matches anything but a line break */
            memcpy(p, "[^\\n\\r]", 7); p += 7;
        } else *p ++ = pattern[i];
    }

    if (full) { memcpy(p, ")\\z", 3); p += 3; }
    *p = '\0';

    regex = pcre_compile(buf, PCRE_UTF8 | PCRE_UCP, & err, & off, NULL);
    if (! regex) debug("jsonpath: bad regular expression: %s.\n", err);

    free(buf);

    return regex;
}
#endif


/* -------------------------------------------------------------------------- */

static int _call(_Compiler *c, _Ctx *ctx)
{
    /** @brief finish a function call and emit its opcode */

    _Type *t = NULL;
    int fn = ctx->last;
    #ifdef HAS_PCRE
    _Op *op = NULL;
    int64_t slot = -1;
    #endif

    if (ctx->count != _fn[fn].arity) {
        SYNTAX(c, "wrong number of arguments");
        return -1;
    }

    t = c->types + c->ntypes - ctx->count;

    switch (fn) {
    case FN_LENGTH:
        if (! _emit(c, OP_LENGTH)) return -1;
        break;
    case FN_MATCH:
    case FN_SEARCH:
        #ifdef HAS_PCRE
        if (t[1].type == T_LIT && c->q->ops[t[1].op].flags == LIT_STR) {
            /* a constant pattern is compiled once */
            op = & c->q->ops[t[1].op];
            if (_grow(& c->q->regex, & c->q->sregex, c->q->nregex + 1,
                      sizeof(*c->q->regex)) == -1)
                return -1;
            c->q->regex[c->q->nregex] = _regex(
                c->q->pool + op->a,
                op->len,
                (fn == FN_MATCH)
            );
            if (! c->q->regex[c->q->nregex]) {
                SYNTAX(c, "invalid regular expression");
                return -1;
            }
            slot = c->q->nregex ++;
        }
        if (! (op = _emit(c, (fn == FN_MATCH) ? OP_MATCH : OP_SEARCH)) )
            return -1;
        op->a = slot;
        break;
        #else
        SYNTAX(c, "match() and search() need PCRE");
        return -1;
        #endif
    default: break;
    }

    c->ntypes -= ctx->count - 1;
    t->type = _fn[fn].result; t->op = c->q->count - 1;
    c->nctx --;

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _argument(_Compiler *c, _Ctx *ctx)
{
    int fn = ctx->last;
    char q = '\0';

    _space(c);
    q = AT(c);

    if (ctx->state == 1) {
        /* after an argument */
        c->pos ++;
        if (q == ')') return _call(c, ctx);
        if (q != ',') {
            SYNTAX(c, "expected ',' or ')'");
            return -1;
        }
        ctx->state = 2;
        return 0;
    }

    if (! ctx->state && q == ')') {
        c->pos ++;
        return _call(c, ctx);
    }

    if (ctx->count == _fn[fn].arity) {
        SYNTAX(c, "too many arguments");
        return -1;
    }

    ctx->state = 1; ctx->count ++;
    if (! (ctx = _open(c, C_EXPR)) ) return -1;
    ctx->flags = _fn[fn].param[c->ctx[c->nctx - 2].count - 1];
    ctx->last = fn;
    ctx->base = c->nstack; ctx->tbase = c->ntypes;

    return 0;
}

/* -------------------------------------------------------------------------- */

static JSONPath_Query *_compile(const char *expr, size_t len)
{
    _Compiler c;
    _Ctx *ctx = NULL;
    int ret = 0;

    memset(& c, 0, sizeof(c));
    c.s = expr; c.len = len;

    if (! (c.q = calloc(1, sizeof(*c.q))) ) {
        perror(ERR(_compile, calloc));
        return NULL;
    }

    if (! len || expr[0] != '$') {
        SYNTAX(& c, "a query starts with '$'");
        goto _err;
    }

    c.pos = 1;
    if (! (ctx = _open(& c, C_SEGS)) ) goto _err;
    ctx->flags = 1;

    while (c.nctx) {
        ctx = & c.ctx[c.nctx - 1];
        switch (ctx->kind) {
        case C_SEGS: ret = _segment(& c, ctx); break;
        case C_BRACKET: ret = _selector(& c, ctx); break;
        case C_EXPR: ret = _expression(& c, ctx); break;
        default: ret = _argument(& c, ctx);
        }
        if (ret == -1) goto _err;
    }

    free(c.ctx); free(c.stack); free(c.types);

    return c.q;

_err:
    free(c.ctx); free(c.stack); free(c.types);
    jsonpath_query_free(c.q);
    return NULL;
}

/* -------------------------------------------------------------------------- */

#endif
