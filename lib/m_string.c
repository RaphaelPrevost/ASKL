/*******************************************************************************
 *  Concrete Server                                                            *
 *  Copyright (c) 2005-2025 Raphael Prevost <raph@el.bzh>                      *
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

#include "m_string.h"

#define _UINT(c) ((unsigned int) ((unsigned char) c))

/* -- hexadecimal digits -- */
static const char _hex[] = "0123456789abcdef";

/* -- base58 encoding/decoding -- */

/* encoding lookup table */
static const char _b58[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZ"
                           "abcdefghijkmnopqrstuvwxyz";

/* decoding lookup table */
static const char _d58[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*  12 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*  24 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*  36 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*  48 */
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8, -1, -1, /*  60 */
    -1, -1, -1, -1, -1,  9, 10, 11, 12, 13, 14, 15, /*  72 */
    16, -1, 17, 18, 19, 20, 21, -1, 22, 23, 24, 25, /*  84 */
    26, 27, 28, 29, 30, 31, 32, -1, -1, -1, -1, -1, /*  96 */
    -1, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, /* 108 */
    -1, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, /* 120 */
    55, 56, 57, -1, -1, -1, -1, -1                  /* 128 */
};

/* macro to ease the use of the decode table and prevent out of bound access */
#define _D58(c) (_d58[(c) & 0x7f])

/* -- base64 encoding/decoding -- */

/* encoding lookup table */
static const char _b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz"
                           "0123456789+/";

/* decoding lookup table */
static const char _d64[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -2, -1, /*  12 */
    -1, -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /*  24 */
    -1, -1, -1, -1, -1, -1, -1, -1, -2, -1, -1, -1, /*  36 */
    -1, -1, -1, -1, -1, -1, -1, 62, -1, 62, -1, 63, /*  48 */
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, /*  60 */
    -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6, /*  72 */
     7,  8, 9,  10, 11, 12, 13, 14, 15, 16, 17, 18, /*  84 */
    19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63, /*  96 */
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, /* 108 */
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, /* 120 */
    49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 140 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 152 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 164 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 176 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 188 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 200 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 212 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 224 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 236 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 248 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1              /* 256 */
};

#ifdef _ENABLE_JSON
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

#define QUOTE       0 /* ', " */
#define ESCAPESEQ   1 /* \ */
#define NON_PRINT   2 /* ASCII non-printing characters */
#define WHITE       3 /* \n, \r, \t */
#define SPACE       4 /* 0x20 */
#define PRIMITIVE   5 /* f(alse), n(ull), t(rue) */
#define DIGIT_EXP   6 /* e, E */
#define DIGIT_RAD   7 /* . */
#define DIGIT_POS   8 /* + */
#define DIGIT_NEG   9 /* - */
#define DIGIT      10 /* 0-9 */
#define COMMA      11 /* , */
#define COLON      12 /* : */
#define OBJ_START  13 /* {, [ */
#define OBJ_CLOSE  14 /* }, ] */
#define ASCII      15 /* ASCII characters */
#define EXT_ASCII  16 /* Extended ASCII */
#endif

/* -- sha1 private context -- */
typedef struct {
    uint32_t h0, h1, h2, h3, h4;
    uint32_t nblocks;
    unsigned char buf[64];
    int count;
} SHA1_CONTEXT;

/* -- rawurlencode special chars -- */

/*  1: unsafe chars
    2: control chars
    3: 0x7f
    4: non US-ASCII chars
    5: reserved chars
    6: escape char */

static const char _unsafe[256] = {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0x0f */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0x1f */
    1, 0, 1, 1, 0, 6, 5, 1, 0, 0, 0, 0, 0, 0, 0, 5, /* 0x2f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 1, 5, 1, 5, /* 0x3f */
    5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x4f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, /* 0x5f */
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x6f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 3, /* 0x7f */
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, /* 0x8f */
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, /* 0x9f */
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, /* 0xaf */
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, /* 0xbf */
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, /* 0xcf */
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, /* 0xdf */
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, /* 0xef */
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, /* 0xff */
};

/* -------------------------------------------------------------------------- */
/* BITWISE OPERATIONS */
/* -------------------------------------------------------------------------- */

#include "ports/m_port_bitops.c"

/* -------------------------------------------------------------------------- */
/* 0. Initialization */
/* -------------------------------------------------------------------------- */

public int string_api_setup(void)
{
    return 0;
}

/* -------------------------------------------------------------------------- */
/* 1. Basic string allocation, conversion and initialization functions        */
/* -------------------------------------------------------------------------- */

public m_string *string_prealloc(const char *string, size_t len, size_t total)
{
    /** @brief allocate a m_string and initialize it with the given data */

    m_string *new = NULL;
    int32_t allocsize = 0;

    if (total < len) {
        debug("string_prealloc(): 'total' should be greater than length.\n");
        total = len;
    }

    if (! (new = malloc(sizeof(*new))) ) {
        perror(ERR(string_prealloc, malloc));
        return NULL;
    }

    /* allocate the internal buffer, ensure it is on wchar_t boundary */
    if (! total) {
        new->_data = NULL; new->_len = new->_alloc = 0; new->_flags = 0;
        new->parent = NULL; new->_parts_alloc = new->parts = 0;
        new->token = NULL;
        return new;
    }

    allocsize = (total + sizeof(wchar_t)) * sizeof(*new->_data);

    if ((size_t) allocsize < total) {
        debug("string_prealloc(): integer overflow.\n");
        free(new);
        return NULL;
    }

    if (! (new->_data = malloc(allocsize)) ) {
        perror(ERR(string_prealloc, malloc));
        free(new);
        return NULL;
    }
    new->_alloc = total * sizeof(*new->_data);

    /* copy the given data in the internal buffer */
    if (string) memcpy(new->_data, string, len);

    memset(new->_data + len, 0, sizeof(wchar_t));

    new->_len = len; new->_flags = 0;
    new->parent = NULL; new->token = NULL;
    new->_parts_alloc = new->parts = 0;

    return new;
}

/* -------------------------------------------------------------------------- */

public m_string *string_alloc(const char *string, size_t len)
{
    return string_prealloc(string, len, len);
}

/* -------------------------------------------------------------------------- */

public m_string *string_encaps(const char *string, size_t len)
{
    /** @brief encapsulate an existing buffer in a static m_string structure */

    m_string *new = NULL;

    if (! string || ! len) return NULL;

    if (! (new = string_alloc(NULL, 0)) ) {
        debug("string_encaps(): out of memory.\n");
        return NULL;
    }

    new->_data = (char *) string;
    new->_len = len; new->_alloc = len;
    new->_flags = _STRING_ENCAPSULATED;
    new->parent = NULL;
    new->_parts_alloc = new->parts = 0;
    new->token = NULL;

    return new;
}

/* -------------------------------------------------------------------------- */

public m_string *string_vfmt(m_string *str, const char *fmt, va_list args)
{
    int ret = 0;
    va_list copy;

    /* sanity check */
    if (! fmt) {
        debug("string_vfmt(): bad parameters.\n");
        return NULL;
    }

    if (str && str->_flags & _STRING_READ_ONLY) {
        debug("string_vfmt(): illegal write attempt.\n");
        return NULL;
    }

    /* XXX va_lists are mangled in GNU C and cannot be reused */
    va_copy(copy, args);

    if ( (ret = m_vsnprintf(NULL, 0, fmt, args)) <= 0) {
        debug("string_vfmt(): wrong format or vsnprintf() error.\n");
        va_end(copy);
        return NULL;
    }

    if (! str) {
        if (! (str = string_alloc(NULL, ret)) ) {
            debug("string_vfmt(): allocation failure.\n");
            va_end(copy);
            return NULL;
        }
    } else if (string_extend(str, ret) == -1) {
        debug("string_vfmt(): resize failure.\n");
        va_end(copy);
        return NULL;
    }

    /* the length returned by vsnprintf does not include the trailing \0 */
    str->_len = m_vsnprintf(str->_data, ret + 1, fmt, copy);

    va_end(copy);

    return str;
}

/* -------------------------------------------------------------------------- */

public m_string *string_fmt(m_string *str, const char *fmt, ...)
{
    /** @brief overwrite a string with the given format or allocate it */

    va_list args;

    va_start(args, fmt);
    str = string_vfmt(str, fmt, args);
    va_end(args);

    return str;
}

/* -------------------------------------------------------------------------- */

public m_string *string_catfmt(m_string *str, const char *fmt, ...)
{
    int ret = 0;
    size_t off = 0;

    va_list args, copy;

    va_start(args, fmt);

    /* sanity check */
    if (! fmt) {
        debug("string_catfmt(): bad parameters.\n");
        va_end(args);
        return NULL;
    }

    if (str && str->_flags & _STRING_READ_ONLY) {
        debug("string_catfmt(): illegal write attempt.\n");
        va_end(args);
        return NULL;
    }

    /* XXX va_lists are mangled in GNU C and cannot be reused */
    va_copy(copy, args);

    if ( (ret = m_vsnprintf(NULL, 0, fmt, args)) <= 0) {
        debug("string_catfmt(): wrong format or vsnprintf() error.\n");
        va_end(args); va_end(copy);
        return NULL;
    }

    /* discard the mangled list */
    va_end(args);

    if (! str) {
        if (! (str = string_alloc(NULL, ret)) ) {
            debug("string_catfmt(): allocation failure.\n");
            va_end(copy);
            return NULL;
        }
    } else {
        if (string_extend(str, (off = SIZE(str)) + ret) == -1) {
            debug("string_catfmt(): resize failure.\n");
            va_end(copy);
            return NULL;
        } else str->_len += ret;
    }

    /* the length returned by vsnprintf does not include the trailing \0 */
    m_vsnprintf(str->_data + off, ret + 1, fmt, copy);

    va_end(copy);

    return str;
}

/* -------------------------------------------------------------------------- */

public m_string *string_from_uint8(uint8_t u8)
{
    /** @brief create a string from an integer */

    return string_alloc((char *) & u8, sizeof(u8));
}

/* -------------------------------------------------------------------------- */

public int string_to_uint8s(const char *string, uint8_t *out)
{
    if (! string || ! out) {
        debug("string_to_uint8s: bad parameters.\n");
        return -1;
    }

    memcpy(out, string, sizeof(*out));

    return 0;
}

/* -------------------------------------------------------------------------- */

public int string_to_uint8(const m_string *string, uint8_t *out)
{
    /** @brief convert a binary string to an integer */

    if (! string || ! out || ! DATA(string) || SIZE(string) < sizeof(*out)) {
        debug("string_to_uint8(): bad parameters.\n");
        return -1;
    }

    memcpy(out, string->_data, sizeof(*out));

    return 0;
}

/* -------------------------------------------------------------------------- */

public uint8_t string_peek_uint8(const m_string *string)
{
    uint8_t ret = 0;

    string_to_uint8(string, & ret);

    return ret;
}

/* -------------------------------------------------------------------------- */

public uint8_t string_fetch_uint8(m_string *string)
{
    uint8_t ret = 0;

    if (string_to_uint8(string, & ret) == 0)
        string_suppr(string, 0, sizeof(ret));

    return ret;
}

/* -------------------------------------------------------------------------- */

public m_string *string_from_uint16(uint16_t u16)
{
    /** @brief create a string from an integer */

    return string_alloc((char *) & u16, sizeof(u16));
}

/* -------------------------------------------------------------------------- */

public int string_to_uint16s(const char *string, uint16_t *out)
{
    if (! string || ! out) {
        debug("string_to_uint16s: bad parameters.\n");
        return -1;
    }

    memcpy(out, string, sizeof(*out));

    return 0;
}

/* -------------------------------------------------------------------------- */

public int string_to_uint16(const m_string *string, uint16_t *out)
{
    /** @brief convert a binary string to an integer */

    if (! string || ! out || ! DATA(string) || SIZE(string) < sizeof(*out)) {
        debug("string_to_uint16(): bad parameters.\n");
        return -1;
    }

    memcpy(out, string->_data, sizeof(*out));

    return 0;
}

/* -------------------------------------------------------------------------- */

public uint16_t string_peek_uint16(const m_string *string)
{
    uint16_t ret = 0;

    string_to_uint16(string, & ret);

    return ret;
}

/* -------------------------------------------------------------------------- */

public uint16_t string_fetch_uint16(m_string *string)
{
    uint16_t ret = 0;

    if (string_to_uint16(string, & ret) == 0)
        string_suppr(string, 0, sizeof(ret));

    return ret;
}

/* -------------------------------------------------------------------------- */

public m_string *string_from_uint32(uint32_t u32)
{
    /** @brief create a string from an integer */

    return string_alloc((char *) & u32, sizeof(u32));
}

/* -------------------------------------------------------------------------- */

public int string_to_uint32s(const char *string, uint32_t *out)
{
    if (! string || ! out) {
        debug("string_to_uint32s: bad parameters.\n");
        return -1;
    }

    memcpy(out, string, sizeof(*out));

    return 0;
}

/* -------------------------------------------------------------------------- */

public int string_to_uint32(const m_string *string, uint32_t *out)
{
    /** @brief convert a binary string to an integer */

    if (! string || ! out || ! DATA(string) || SIZE(string) < sizeof(*out)) {
        debug("string_to_uint32(): bad parameters.\n");
        return -1;
    }

    memcpy(out, string->_data, sizeof(*out));

    return 0;
}

/* -------------------------------------------------------------------------- */

public uint32_t string_peek_uint32(const m_string *string)
{
    uint32_t ret = 0;

    string_to_uint32(string, & ret);

    return ret;
}

/* -------------------------------------------------------------------------- */

public uint32_t string_fetch_uint32(m_string *string)
{
    uint32_t ret = 0;

    if (string_to_uint32(string, & ret) == 0)
        string_suppr(string, 0, sizeof(ret));

    return ret;
}

/* -------------------------------------------------------------------------- */

public m_string *string_from_uint64(uint64_t u64)
{
    /** @brief create a string from an integer */

    return string_alloc((char *) & u64, sizeof(u64));
}

/* -------------------------------------------------------------------------- */

public int string_to_uint64s(const char *string, uint64_t *out)
{
    if (! string || ! out) {
        debug("string_to_uint64s: bad parameters.\n");
        return -1;
    }

    memcpy(out, string, sizeof(*out));

    return 0;
}

/* -------------------------------------------------------------------------- */

public int string_to_uint64(const m_string *string, uint64_t *out)
{
    /** @brief convert a binary string to an integer */

    if (! string || ! out || ! DATA(string) || SIZE(string) < sizeof(*out)) {
        debug("string_to_uint64(): bad parameters.\n");
        return -1;
    }

    memcpy(out, string->_data, sizeof(*out));

    return 0;
}

/* -------------------------------------------------------------------------- */

public uint64_t string_peek_uint64(const m_string *string)
{
    uint64_t ret = 0;

    string_to_uint64(string, & ret);

    return ret;
}

/* -------------------------------------------------------------------------- */

public uint64_t string_fetch_uint64(m_string *string)
{
    uint64_t ret = 0;

    if (string_to_uint64(string, & ret) == 0)
        string_suppr(string, 0, sizeof(ret));

    return ret;
}

/* -------------------------------------------------------------------------- */

public int string_peek_fmt(m_string *string, const char *fmt, ...)
{
    /** @brief copy data from the string according to the given format string */

    int ret = 0;
    va_list args;

    if (! string || ! DATA(string) || ! fmt) return -1;

    va_start(args, fmt);

    if (! (ret = m_vsnscanf(DATA(string), SIZE(string), fmt, args)) ) {
        debug("string_peek_fmt(): wrong format or vsnscanf() error.\n");
        return -1;
    }

    va_end(args);

    return 0;
}

/* -------------------------------------------------------------------------- */

public int string_fetch_fmt(m_string *string, const char *fmt, ...)
{
    /** @brief move data from the string according to the given format string */

    int ret = 0;
    va_list args;

    if (! string || ! DATA(string) || ! fmt) return -1;

    va_start(args, fmt);

    if (! (ret = m_vsnscanf(DATA(string), SIZE(string), fmt, args)) ) {
        debug("string_fetch_fmt(): wrong format or vsnscanf() error.\n");
        return -1;
    }

    string_suppr(string, 0, ret);

    va_end(args);

    return 0;
}

/* -------------------------------------------------------------------------- */

public int string_peek_buffer(m_string *string, char *out, size_t len)
{
    /** @brief copy data from the string to an external buffer */

    if (! string || ! DATA(string) || ! out || ! len) return -1;

    len = MIN(SIZE(string), len);

    memcpy(out, DATA(string), MIN(SIZE(string), len));

    return 0;
}

/* -------------------------------------------------------------------------- */

public int string_fetch_buffer(m_string *string, char *out, size_t len)
{
    /** @brief move data from the string to an external buffer */

    if (! string || ! DATA(string) || ! out || ! len) return -1;

    len = MIN(SIZE(string), len);

    memcpy(out, DATA(string), MIN(SIZE(string), len));

    string_suppr(string, 0, len);

    return 0;
}

/* -------------------------------------------------------------------------- */

public m_string *string_peek(m_string *string, size_t len)
{
    /** @brief copy data from the string */

    if (! string || ! DATA(string)) return NULL;

    if (! len) len = SIZE(string);

    return string_alloc(DATA(string), len);
}

/* -------------------------------------------------------------------------- */

public m_string *string_fetch(m_string *string, size_t len)
{
    /** @brief extract data from the string */

    m_string *ret = NULL;

    if (! string || ! DATA(string)) return NULL;

    if (! len) len = SIZE(string);

    ret = string_alloc(DATA(string), len);

    string_suppr(string, 0, len);

    return ret;
}

/* -------------------------------------------------------------------------- */

public void string_flush(m_string *string)
{
    if (! string) return;

    string_free_token(string);

    string_suppr(string, 0, SIZE(string));
}

/* -------------------------------------------------------------------------- */

public int string_wchar(m_string *string)
{
    /** @brief convert a multibyte string to a wide character string */

    char *buffer = NULL;
    size_t bufsize = 0;
    size_t ret = 0;

    if (! string || ! DATA(string)) return -1;

    /* check if writing and resizing the string is allowed */
    if (string->_flags & _STRING_IMMUTABLE) return -1;

    /* find the size of the wchar string */
    if ( (bufsize = mbstowcs(NULL, DATA(string), 0)) != (size_t) -1) {
        /* allocate the wchar buffer */
        buffer = malloc( (bufsize + 1) * sizeof(wchar_t));
        if (buffer == NULL) {
            perror(ERR(string_wchar, malloc)); return -1;
        }

        /* convert the multibyte string to wchar */
        ret = mbstowcs((wchar_t *) buffer, DATA(string), bufsize + 1);

        if (ret == (size_t) -1) {
            free(buffer); perror(ERR(string_wchar, mbstowcs));
            return -1;
        }

        /* update the string */
        free(string->_data); string->_data = buffer;
        string->_len = bufsize * sizeof(wchar_t);
        string->_alloc = (bufsize + 1) * sizeof(wchar_t);
    } else {
        perror(ERR(string_wchar, mbstowcs)); return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

public int string_mbyte(m_string *string)
{
    /** @brief convert a wide character string to a multibyte string */

    char *buffer = NULL;
    size_t l = 0;

    if (! string || ! DATA(string)) return -1;

    /* check if writing and resizing the string is allowed */
    if (string->_flags & _STRING_IMMUTABLE) return -1;

    /* find the size of the multibyte buffer */
    if ( (l = wcstombs(NULL, (wchar_t *) DATA(string), 0)) != (size_t) -1) {
        /* allocate it */
        buffer = malloc( (l + 1) * sizeof(*buffer));
        if (buffer == NULL) {
            perror(ERR(string_mbyte, malloc)); return -1;
        }

        /* perform the conversion */
        if (wcstombs(buffer, (wchar_t *) DATA(string), l + 1) == (size_t) -1) {
            perror(ERR(string_mbyte, wcstombs));
            free(buffer); return -1;
        }

        /* update the string */
        free(string->_data); string->_data = buffer;
        string->_len = l; string->_alloc = l + 1;
    } else {
        perror(ERR(string_mbyte, wcstombs)); return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
#ifdef HAS_ICONV
/* -------------------------------------------------------------------------- */

public size_t string_convs(const char *src, size_t srclen, const char *src_enc,
                           char *dst, size_t dstlen, const char *dst_enc)
{
    char buffer[BUFSIZ], *out = dst;
    size_t inlen = srclen, outlen = dstlen;
    #ifdef __APPLE__
    #if ! defined(MAC_OS_X_VERSION_10_5) && ! defined(__MAC_10_5)
    const /* SUSv2 definition */
    #endif
    #endif
    char *in = (char *) src;
    iconv_t conv;
    size_t allocsize = 0;

    if (! src || ! srclen) {
        debug("string_convs(): bad parameters.\n");
        return -1;
    }

    if (! (conv = iconv_open(dst_enc, src_enc)) ) {
        perror(ERR(string_convs, iconv_open)); return -1;
    }

    /* dry run: use a static buffer for output */
    if (! dst) { out = buffer; outlen = sizeof(buffer); }

    while (iconv(conv, & in, & inlen, & out, & outlen) == (size_t) -1) {
        if (errno == E2BIG && ! dst) {
            allocsize += sizeof(buffer) - outlen;
            out = buffer; outlen = sizeof(buffer);
            continue;
        }
        /* conversion failure */
        perror(ERR(string_convs, iconv));
        goto _err_conv;
    }

    /* dry run: evaluate the size required for the output */
    if (! dst) allocsize += sizeof(buffer) - outlen;

    iconv_close(conv);

    return allocsize;

_err_conv:
    iconv_close(conv);

    return -1;
}

/* -------------------------------------------------------------------------- */

public int string_conv(m_string *s, const char *src_enc, const char *dst_enc)
{
    char buffer[BUFSIZ], *out = buffer;
    size_t inlen = 0, outlen = sizeof(buffer);
    #ifdef __APPLE__
    #if ! defined(MAC_OS_X_VERSION_10_5) && ! defined(__MAC_10_5)
    const /* SUSv2 definition */
    #endif
    #endif
    char *in = NULL;
    iconv_t conv;
    size_t allocsize = 0;

    if (! s) {
        debug("string_conv(): bad parameters.\n");
        return -1;
    }

    if (! (conv = iconv_open(dst_enc, src_enc)) ) {
        perror(ERR(string_conv, iconv_open)); return -1;
    }

    in = s->_data; inlen = SIZE(s);

    while (iconv(conv, & in, & inlen, & out, & outlen) == (size_t) -1) {
        if (errno == E2BIG) {
            allocsize += sizeof(buffer) - outlen;
            out = buffer; outlen = sizeof(buffer);
            continue;
        }
        /* incomplete input or invalid multibyte sequence */
        perror(ERR(string_conv, iconv));
        goto _err_conv;
    }

    allocsize += sizeof(buffer) - outlen;

    if (allocsize != SIZE(s)) {
        if (allocsize > SIZE(s)) {
            /* extend the string */
            if (string_extend(s, allocsize) == -1)
                goto _err_conv;
        }
        string_free_token(s);
    }

    if (allocsize > sizeof(buffer)) {
        in = s->_data; inlen = SIZE(s);
        out = s->_data; outlen = allocsize;
        /* convert again */
        if (iconv(conv, & in, & inlen, & out, & outlen) == (size_t) -1) {
            perror(ERR(string_conv, iconv));
            goto _err_conv;
        }
    } else memcpy(s->_data, buffer, allocsize);

    s->_len = allocsize;

    iconv_close(conv);

    return 0;

_err_conv:
    iconv_close(conv);

    return -1;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public int string_swap(m_string *string)
{
    /** @brief swap a binary string */

    unsigned int i = 0;
    unsigned char tmp = 0;

    if (! string || ! DATA(string) || SIZE(string) % 2) {
        debug("string_swap(): bad parameters.\n");
        return -1;
    }

    if (string->_flags & _STRING_READ_ONLY) {
        debug("string_swap(): illegal write attempt.\n");
        return -1;
    }

    for (i = 0; i < SIZE(string); i += 2) {
        tmp = string->_data[i];
        string->_data[i] = string->_data[SIZE(string) - 1 - i];
        string->_data[SIZE(string) - 1 - i] = tmp;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _string_copy_tokens(m_string *dst, const m_string *src)
{
    unsigned int i = 0;

    if (! dst || ! src || ! src->parts) return 0;

    dst->parts = src->parts;

    if (! (dst->token = malloc(dst->parts * sizeof(*dst->token))) ) return -1;

    for (i = 0; i < dst->parts; i ++) {
        dst->token[i]._data = dst->_data + (src->token[i]._data - src->_data);
        dst->token[i]._len = src->token[i]._len;
        dst->token[i]._alloc = src->token[i]._alloc;
        dst->token[i]._flags = src->token[i]._flags;
        dst->token[i].parent = dst;
        /* the parts and token members of the struct will be
           modified by subsequent calls */
        dst->token[i]._parts_alloc = dst->token[i].parts = 0;
        dst->token[i].token = NULL;
        if (_string_copy_tokens(& dst->token[i], & src->token[i]) == -1)
            return -1;
    }

    dst->_parts_alloc = dst->parts;

    return 0;
}

/* -------------------------------------------------------------------------- */

public m_string *string_dup(const m_string *string)
{
    /** @brief duplicate a string */

    return string_dupextend(string, 0);
}

/* -------------------------------------------------------------------------- */

public m_string *string_dupextend(const m_string *s, size_t x)
{
    m_string *ret = NULL;

    if (! s) return NULL;

    /* copy the string */
    if (! (ret = string_prealloc(DATA(s), SIZE(s), SIZE(s) + x)) ) return NULL;

    /* copy the tokens */
    if (_string_copy_tokens(ret, s) == -1) return string_free(ret);

    return ret;
}

/* -------------------------------------------------------------------------- */

public m_string *string_free(m_string *string)
{
    /** @brief clean up a m_string and its internal buffer */

    m_string *parent = string;

    if (! string) return NULL;

    /* check this is a parent string */
    while (parent->parent) parent = parent->parent;
    #ifdef DEBUG
    if (string != parent)
        debug("string_free(): warning: freeing the parent of the token.\n");
    #endif
    string = parent;

    string_free_token(string);

    if (string->_flags & _STRING_STATIC_ALLOC) return NULL;

    if (~string->_flags & _STRING_DISABLE_FREE)
        free(string->_data);
    free(string);

    return NULL;
}

/* -------------------------------------------------------------------------- */
/* 2. String size functions                                                   */
/* -------------------------------------------------------------------------- */

public size_t string_len(const m_string *string)
{
    /** @brief return the length of the string data */

    return string ? string->_len : (size_t) -1;
}

/* -------------------------------------------------------------------------- */

public size_t string_spc(const m_string *string)
{
    /** @brief return the string allocated buffer size */

    return string ? string->_alloc : (size_t) -1;
}

/* -------------------------------------------------------------------------- */

public size_t string_avl(const m_string *string)
{
    /** @brief return the space still available without extending the string */

    return string ? string->_alloc - string->_len : (size_t) -1;
}

/* -------------------------------------------------------------------------- */

static void _string_rebase_token(m_string *s, char *oldbase, char *newbase)
{
    unsigned int i = 0;

    if (! s || ! s->token || ! oldbase || ! newbase) return;

    if (oldbase == newbase) return;

    for (i = 0; i < PARTS(s); i ++) {

        char *addr = newbase + (TOKEN_DATA(s, i) - oldbase);

        if (addr <= (s->_data + s->_alloc)) {
            /* the rebased token is inbound, fine */
            s->token[i]._data = addr;
        } else {
            unsigned int j = 0;

            /* clear all subsequent tokens */
            for (j = i; j < PARTS(s); j ++) string_free_token(TOKEN(s, j));
            memset(TOKEN(s, i), 0, (PARTS(s) - i) * sizeof(m_string));

            s->parts = i;
            break;
        }

        /* this token is rebased, rebase its subtoken too */
        _string_rebase_token(TOKEN(s, i), oldbase, newbase);
    }
}

/* -------------------------------------------------------------------------- */

static void __move_subtokens(m_string *token, int shift, const char *guardian)
{
    unsigned int i = 0;

    if (token) {
        /* recursively shift the token and its subtokens, if any */
        for (i = 0; i < PARTS(token); i ++)
            __move_subtokens(& token->token[i], shift, guardian);
        if (! guardian || token->_data > guardian)
            token->_data += shift;
    }
}

/* -------------------------------------------------------------------------- */

static void _string_shift_token(m_string *s, unsigned int off,
                                unsigned int end, int shift)
{
    unsigned int i = 0, j = 0;

    if (! s || ! PARTS(s) || ! shift) {
        debug("_string_shift_token(): bad parameters.\n");
        return;
    }

    /* find the beginning of the shift */
    for (i = 0; i < PARTS(s); i ++) {
        if (TOKEN_DATA(s, i) >= DATA(s) + off) {
            /* update the position of all subsequent tokens */
            for (j = i; j < PARTS(s); j ++) {
                if (j == i && shift < 0)
                    __move_subtokens(TOKEN(s, j), shift, DATA(s) + off + end);
                else
                    __move_subtokens(TOKEN(s, j), shift, NULL);
            }

            s = TOKEN(s, i);
            do {
                if ( (int) s->_len + shift < 0 || (int) s->_alloc + shift < 0) {
                    m_string *parent = s->parent;
                    /* check parent's tokens bound */
                    for (i = 0; i < PARTS(parent); i ++) {
                        if (TOKEN_END(parent, i) >= STRING_END(parent)) {
                            while ( (-- parent->parts) > i + 1)
                                string_free_token(TOKEN(parent, PARTS(parent)));
                            break;
                        }
                    }
                }
            } while (PARTS(s) && (s = TOKEN(s, PARTS(s) - 1)) );

            break;
        }
    }
}

/* -------------------------------------------------------------------------- */

public int string_dim(m_string *string, size_t size)
{
    /** @brief force resize the internal buffer of a string */

    char *data = NULL, *base = NULL;
    m_string *token = NULL;
    unsigned int off = 0, len = 0;
    int diff = 0, need = 0;
    int32_t allocsize = 0;

    /* sanity checks */
    if (! string || ! size) {
        debug("string_dim(): bad parameters.\n");
        return -1;
    }

    if (string->_alloc == size) return 0;

    /* XXX "need" is the number of bytes that should be added or removed
       to fit the new string size. "diff" is the difference between the
       new and the previous length of the string content. */
    need = size - string->_alloc; diff = size - string->_len;

    /* if this string is a token, we need to resize its parent instead */
    if (string->parent) {
        token = string;
        while (string->parent) string = string->parent;
        /* store the token relative position */
        off = token->_data - string->_data; len = token->_len;
    }

    /* check if the resize makes sense */
    if ( (string->_flags & _STRING_ENCAPSULATED && need > 0) ||
          string->_alloc + need <= 0) {
        debug("string_dim(): illegal resize attempt.\n");
        return -1;
    }

    /* if a token is shrunk the data need to be moved beforehand */
    if (token && token != LAST_TOKEN(string) && need < 0) {
        data = string->_data + off + len;
        memmove(data + diff, data, string->_len - (off + len));
        _string_shift_token(string, off, len, diff);
    }

    /* resize the internal buffer, ensure the buffer is on wchar_t boundary */
    if (need > 0 && ~string->_flags & _STRING_STATIC_ALLOC) {
        /* a string is never actually shrunk, only realloc to expand */
        allocsize = (string->_alloc + need + sizeof(wchar_t)) * sizeof(*data);
        if ((size_t) allocsize < SIZE(string)) {
            debug("string_dim(): integer overflow.\n");
            return -1;
        }

        base = string->_data;

        if (! (data = realloc(string->_data, allocsize)) ) {
            perror(ERR(string_dim, realloc)); return -1;
        }

        string->_data = data; string->_alloc += need * sizeof(*string->_data);
        _string_rebase_token(string, base, data);
    }

    /* if the string was a token, move the data along */
    if (token) {
        if (diff > 0) {
            data = string->_data + off + len;
            memmove(data + diff, data, string->_len - (off + len));
            /* XXX only move subsequent tokens */
            _string_shift_token(string, off + len + 1, 0, diff);
        }

        token->_len = token->_alloc = size;

        /* update the parents' size */
        while ( (token = token->parent) ) {
            token->_len += diff;
            if (token != string) token->_alloc = token->_len;
        }
    } else if (diff < 0) string->_len += diff;

    /* zero out the remainder of the buffer */
    if (SIZE(string) < string->_alloc) string->_data[SIZE(string)] = '\0';

    return 0;
}

/* -------------------------------------------------------------------------- */

public int string_extend(m_string *string, size_t size)
{
    /** @brief resize a string only if the new size is greater */

    if (! string || ! size) {
        debug("string_extend(): bad parameters.\n");
        return -1;
    }

    return (size > string->_alloc) ? string_dim(string, size) : 0;
}

/* -------------------------------------------------------------------------- */

public int string_shrink(m_string *string, size_t size)
{
    /** @brief resize a string only if the new size is lower */

    if (! string || ! size) {
        debug("string_shrink(): bad parameters.\n");
        return -1;
    }

    return (size < string->_alloc) ? string_dim(string, size) : 0;
}

/* -------------------------------------------------------------------------- */
/* 3. Basic string operations (char * flavor)                                 */
/* -------------------------------------------------------------------------- */

static m_string *_string_mov(m_string *to, off_t o, const char * from, size_t l)
{
    m_string *parent = to;
    int within = 0, alloc = 0;
    ptrdiff_t within_offset = 0;

    /* noop */
    if (l == 0) return to;

    /* sanity checks */
    if (o < 0 || ! from) {
        debug("string_movs(): bad parameters.\n");
        goto _error;
    }

    /* if the destination string does not exist, allocate it */
    if (! to) {
        if (! (to = string_alloc(NULL, l)) ) {
            debug("string_movs(): allocation failure.\n");
            goto _error;
        } else alloc = 1;
    } else while (parent->parent) parent = parent->parent;

    /* check if the source is inside the destination */
    if ( (from >= DATA(parent)) && (from < STRING_END(parent)) )
        within = 1, within_offset = from - DATA(parent);

    /* reject out of bound offsets and resize the destination string */
    if (string_extend(to, o + l) == -1) {
        debug("string_movs(): resize failure.\n");
        goto _error;
    } else if (within) {
        /* if the source was within destination, correct the pointer */
        from = DATA(parent) + within_offset;

        /* check if the offset is still in bound */
        if ( (from < DATA(parent)) || (from >= STRING_END(parent)) ) {
            debug("string_movs(): offset out of bound.\n");
            goto _error;
        }
    }

    memmove(to->_data + o, from, l);

    /* update the string */
    to->_len = (SIZE(to) < o + l) ? o + l : SIZE(to);
    if (! to->parent) memset(to->_data + SIZE(to), 0x0, sizeof(wchar_t));

    /* NOTE it is up to the caller to deal with the tokens */

    return to;

_error:
    if (alloc) string_free(to);
    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_string *string_movs(m_string *to, off_t o, const char *from, size_t l)
{
    /** @brief move a C string or a part of it into a m_string */

    m_string *ret = NULL;
    unsigned int i = 0, deleted = 0;

    if ( (ret = _string_mov(to, o, from, l)) ) {
        if (o) {
            /* preserve tokens before the offset */
            for (i = 0; i < PARTS(ret); i ++) {
                if (TOKEN_END(ret, i) >= DATA(ret) + o) {
                    string_free_token(TOKEN(ret, i));
                    deleted ++;
                }
            }
            ret->parts -= deleted;
        } else string_free_token(ret);
    }

    return ret;
}

/* -------------------------------------------------------------------------- */

static void CALLBACK _string_update_size(m_string *s, ssize_t diff)
{
    while (s) {
        s->_len += diff;
        if (s->parent) s->_alloc = s->_len; else s->_data[SIZE(s)] = '\0';
        s = s->parent;
    }
}

/* -------------------------------------------------------------------------- */

public int string_suppr(m_string *string, off_t offset, size_t length)
{
    /** @brief remove a subsection of a string */
    unsigned int i = 0, j = 0;
    off_t off = 0, end = 0;
    int within = -1, after = -1, straddling = 0;
    m_string *parent = string, *top = NULL, *last = NULL;

    if (! string || offset < 0 || ! length || offset + length > SIZE(string)) {
        debug("string_suppr(): bad parameters.\n");
        return -1;
    }

    /* get the parent */
    while (parent->parent) parent = parent->parent;

    off = offset + DATA(string) - DATA(parent);
    end = off + length;
    top = parent;

    /* if the subsection is actually the last token, cut to the chase */
    if (PARTS(parent) && (DATA(LAST_TOKEN(parent)) == DATA(parent) + off)) {
        if (likely(STRING_END(LAST_TOKEN(parent)) == DATA(parent) + end))
            goto _update_length;
    }

    /* find the first tokens within or after the deleted subsection */
    do {
        straddling = 0;
        for (i = 0; i < PARTS(top); i ++) {
            off_t token_off, token_end;
            token_off = TOKEN_DATA(top, i) - DATA(parent);
            token_end = TOKEN_END(top, i) - DATA(parent);

            if (within == -1 && token_off >= off && token_off < end) {
                /* found the first token within the subsection */
                if (token_end > end) {
                    /* found a straddling token, check subtokens instead */
                    top = TOKEN(top, i);
                    straddling = 1;
                    break;
                } else within = i;
            } else if (after == -1 && token_off >= end) {
                /* found the first token after the subsection */
                after = i; break;
            }
        }
    } while (straddling);

    if (! _string_mov(parent, off, DATA(parent) + end, SIZE(parent) + 1 - end))
        return -1;

    if (within > 0) {
        /* truncate the last tokens before the subsection, if any */
        for (last = TOKEN(top, within - 1); PARTS(last); last = LAST_TOKEN(last)) {
            int diff = DATA(parent) + off - STRING_END(LAST_TOKEN(last));
            if (diff > 0) {
                last->_len -= diff;
                last->_alloc -= diff;
            }
        }
    }

    /* move the subsequent tokens, if any */
    if (after > 0) {
        for (i = after; i < PARTS(top); i ++) {
            __move_subtokens(TOKEN(top, i), -length, NULL);
            if (within != -1) {
                if (within < (int) i) {
                    string_free_token(TOKEN(top, within));
                    top->token[within] = top->token[i];
                    for (j = 0; j < top->token[within].parts; j ++)
                        SUBTOKEN(top, within, j)->parent = TOKEN(
                            top, within
                        );
                    within ++;
                } else within = -1;
            }
        }
    }

    /* update the subtoken count */
    if (within != -1 && after != -1) top->parts = within;

_update_length:
    /* find the last subtoken aligned with the end of the parent string */
    for (last = top; PARTS(last); last = LAST_TOKEN(last))
        if (STRING_END(LAST_TOKEN(last)) != STRING_END(parent)) break;

    /* truncate from that last subtoken up to the parent string */
    _string_update_size(last, -length);

    return 0;
}

/* -------------------------------------------------------------------------- */

public m_string *string_cats(m_string *to, const char *from, size_t len)
{
    /** @brief append a C string to a m_string */

    m_string *orig = to;

    /* find the last token first */
    if (to) {
        while (to->parent) to = to->parent;
        while (to->parts) {
            if (STRING_END(to) == STRING_END(LAST_TOKEN(to))) {
                if (LAST_TOKEN(to)->_flags & _STRING_VALIDATED) break;
                to = LAST_TOKEN(to);
            } else break;
        }
    }

    return (to = string_movs(to, (to) ? SIZE(to) : 0, from, len)) ?
           ((orig) ? orig : to) :
           NULL;
}

/* -------------------------------------------------------------------------- */

public m_string *string_pres(m_string *to, const char *from, size_t len)
{
    /** @brief prepend a C string to a m_string */

    /* move the original string to the right */
    if (to && ! (string_movs(to, len, DATA(to), to->_len)) ) return NULL;

    return string_movs(to, 0, from, len);
}

/* -------------------------------------------------------------------------- */

public m_string *string_cpys(m_string *to, const char *from, size_t len)
{
    /** @brief copy data from a C string to a m_string */

    return string_movs(to, 0, from, len);
}

/* -------------------------------------------------------------------------- */

public int string_cmps(const m_string *a, const char *b, size_t len)
{
    /** @brief compare a m_string and a C string */

    if (! a || ! DATA(a) || ! b || ! len) {
        debug("string_cmps(): bad parameters.\n");
        return 2;
    }

    return memcmp(DATA(a), b, MIN(SIZE(a), len));
}

/* -------------------------------------------------------------------------- */
/* 4. Basic string operations (m_string version)                              */
/* -------------------------------------------------------------------------- */

public m_string *string_mov(m_string *to, off_t off,
                            const m_string *from, size_t len)
{
    /** @brief move a string or a part of it into another string */

    if (! from) return NULL;

    return string_movs(to, off, DATA(from), len);
}

/* -------------------------------------------------------------------------- */

public m_string *string_cat(m_string *to, const m_string *from)
{
    /** @brief append a string to another */

    if (! from) return NULL;

    return string_cats(to, DATA(from), SIZE(from));
}

/* -------------------------------------------------------------------------- */

public m_string *string_pre(m_string *to, const m_string *from)
{
    /** @brief prepend a string to another */

    if (! from) return NULL;

    return string_pres(to, DATA(from), SIZE(from));
}

/* -------------------------------------------------------------------------- */

public m_string *string_cpy(m_string *to, const m_string *from)
{
    /** @brief copy data from a string to another */

    if (! from) return NULL;

    return string_cpys(to, DATA(from), SIZE(from));
}

/* -------------------------------------------------------------------------- */

public int string_cmp(const m_string *a, const m_string *b)
{
    /** @brief compare two strings */

    if (! b) return 2;

    return string_cmps(a, DATA(b), SIZE(b));
}

/* -------------------------------------------------------------------------- */

public int string_upper(m_string *string)
{
    /** @brief convert an ASCII string to upper case */

    unsigned int i = 0;

    if (! string || ! DATA(string)) {
        debug("string_upper(): bad parameters.\n");
        return -1;
    }

    /* check if writing to the string is allowed */
    if (string->_flags & _STRING_READ_ONLY) {
        debug("string_upper(): illegal write attempt.\n");
        return -1;
    }

    for (i = 0; i < SIZE(string) && string->_data[i]; i ++)
        string->_data[i] = toupper(string->_data[i]);

    /* since it is an in place transformation, keep existings tokens */

    return 0;
}

/* -------------------------------------------------------------------------- */

public int string_lower(m_string *string)
{
    /** @brief convert an ASCII string to lower case */

    unsigned int i = 0;

    if (! string || ! DATA(string)) {
        debug("string_lower(): bad parameters.\n");
        return -1;
    }

    /* check if writing to the string is allowed */
    if (string->_flags & _STRING_READ_ONLY) {
        debug("string_lower(): illegal write attempt.\n");
        return -1;
    }

    for (i = 0; i < SIZE(string) && string->_data[i]; i ++)
        string->_data[i] = tolower(string->_data[i]);

    /* since it is an in place transformation, keep existings tokens */

    return 0;
}

/* -------------------------------------------------------------------------- */
/* 5. Advanced string manipulation functions                                  */
/* -------------------------------------------------------------------------- */

public int string_compile_searchstring(m_search_string *c,
                                       const char *s, size_t len)
{
    /** @brief compute the Boyer-Moore lookup table for the given substring */

    unsigned int i = 0;

    if (! c || ! s || ! len || len > 255) {
        debug("string_compile_searchstring(): bad parameters.\n");
        return -1;
    }

    /* register the distance between each possible byte and the last byte
       of the substring in a lookup table */
    memset(c->_lut, len, UCHAR_MAX + 1);

    /*  fix the length */
    len --;

    for (i = 0; i < len; i ++) c->_lut[_UINT(s[i])] = len - i;

    /* get the shift length and set up the guard char */
    c->_shift = c->_lut[_UINT(s[len])];
    c->_lut[_UINT(s[len])] = 0; c->_len = len;

    return 0;
}

/* -------------------------------------------------------------------------- */

public off_t string_compile_find(const char *s, size_t len, size_t o,
                                 const char *sub, m_search_string *c)
{
    /** @brief find a C substring within a string using a precompiled LUT */

    size_t i = 0, k = 0;
    const char *ptr = NULL;

    if (! s || ! len || ! sub || ! c) {
        debug("string_compile_find(): bad parameters.\n");
        return -1;
    }

    if (o + c->_len < len) {
        if (len < UCHAR_MAX) {
            /* naive search algorithm is faster for short strings */
            while ( (ptr = memchr(s + o, sub[0], len - (o + c->_len))) ) {
                if (! c->_len || ! memcmp(ptr + 1, sub + 1, c->_len))
                    return ptr - s;
                if ( (o = (ptr - s) + c->_len) > len) break;
            }
            /* not found */
            return -1;
        }
    } else return -1;

    /* search loop (Tuned Boyer-Moore algorithm) */
    for (i = o; i < len; i += c->_shift) {
        /* use the lookup table to skip impossible matches */
        do {
            if ( (k = i + c->_len) >= len)
                return -1;
            k = c->_lut[_UINT(s[k])];
        } while ( (k != 0) && (i += k) );

        /* try a possible match */
        if (! memcmp(sub, s + i, c->_len)) break;
    }

    return (i >= len) ? -1 : (off_t) i;
}

/* -------------------------------------------------------------------------- */

public off_t string_sfinds(const char *str, size_t slen, size_t o,
                           const char *sub, size_t len            )
{
    /** @brief find a C susbstring within a string */

    m_search_string c;

    /* sanity checks */
    if (! str || ! slen || ! sub || ! len) {
        debug("string_sfinds(): bad parameters.\n");
        return -1;
    }

    if ( (len > slen) || (o > slen) ) {
        debug("string_sfinds(): search string out of bound.\n");
        return -1;
    }

    if (string_compile_searchstring(& c, sub, len) == -1) return -1;

    return string_compile_find(str, slen, o, sub, & c);
}

/* -------------------------------------------------------------------------- */

public off_t string_finds(const m_string *str, size_t o, const char *sub,
                          size_t len)
{
    if (! str || ! DATA(str) || ! sub || ! len) return -1;

    return string_sfinds(DATA(str), SIZE(str), o, sub, len);
}

/* -------------------------------------------------------------------------- */

public off_t string_find(const m_string *string, size_t o, const m_string *sub)
{
    /** @brief find a susbstring within a string */

    if (! string || ! sub || ! DATA(string) || ! DATA(sub) ) return -1;

    return string_sfinds(DATA(string), SIZE(string), o, DATA(sub), SIZE(sub));
}

/* -------------------------------------------------------------------------- */

public off_t string_pos(const m_string *string, const char *sub)
{
    if (! string || ! sub || ! DATA(string)) return -1;

    return string_finds(string, 0, sub, strlen(sub));
}

/* -------------------------------------------------------------------------- */

public int string_splits(m_string *s, const char *pattern, size_t len)
{
    /** @brief split the string into multiple tokens using a separator */

    off_t offset[16] = { -1 }, off = 0, prev = 0, o = 0, p = 0;
    unsigned int i = 0, j = 0;
    m_string *t = NULL;
    m_search_string compiled_pattern;

    if (! s || ! DATA(s) || ! pattern || ! len) {
        debug("string_splits(): bad parameters.\n");
        return -1;
    }

    /* the pattern delimiter cannot be longer than the source */
    if (SIZE(s) <= len + 1) {
        debug("string_splits(): delimiter out of bound.\n");
        return -1;
    }

    if (SIZE(s) >= UCHAR_MAX) {
        /* compile the search string */
        if (string_compile_searchstring(& compiled_pattern, pattern, len) == -1)
            return -1;
    } else compiled_pattern._len = len - 1;

    if (s->_parts_alloc) {
        /* try to reuse existing tokens */
        for (i = 0; i < s->parts; i ++)
            string_free_token(& s->token[i]);
        s->parts = 0; t = s->token;
    }

    /* fill the offsets array with the locations of the pattern */
    while (off != -1) {
        off = string_compile_find(DATA(s), SIZE(s), prev,
                                  pattern, & compiled_pattern);

        offset[j ++] = prev = off; prev += len;

        if (off == -1 || j >= 16) {
            /* allocate enough room for new tokens */
            if (s->_parts_alloc < s->parts + j) {
                if (! (t = realloc(s->token, (s->parts + j) * sizeof(*t))) )
                    goto _err_realloc;
                s->token = t; s->_parts_alloc = s->parts + j;
            }

            if (s->parts) p = t[s->parts - 1]._data - s->_data;

            /* write the tokens */
            for (i = s->parts; i < s->parts + j; i ++, p = o + len) {
                /* the token inherit parent's flags and set the "no free" bit */
                o = offset[i - s->parts];
                t[i].parent = s;
                t[i]._flags = s->_flags | _STRING_DISABLE_FREE;
                t[i]._data = s->_data + p;
                t[i]._len = (o != -1) ? (size_t) o : SIZE(s);
                t[i]._len -= p;
                t[i]._alloc = t[i]._len;
                t[i]._parts_alloc = t[i].parts = 0;
                t[i].token = NULL;
            }

            j = 0; s->parts = i;
        }
    }

    return 0;

_err_realloc:
    perror(ERR(string_splits, realloc));
    string_free_token(s);
    return -1;
}

/* -------------------------------------------------------------------------- */

public int string_split(m_string *string, const m_string *pattern)
{
    if (! string || ! pattern || ! DATA(pattern)) return -1;

    return string_splits(string, DATA(pattern), SIZE(pattern));
}

/* -------------------------------------------------------------------------- */

static void __bcpy(const char *src, char *dst, size_t len)
{
    size_t c = 0;

    src += len;
    dst += len;

    c = len >> 2;

    if (c) do {
        src -= 4; dst -= 4;
        *(unaligned_uint32_t *) dst = *(unaligned_uint32_t *) src;
    } while (-- c);

    c = len & 0x3;

    if (c) do { *-- dst = *-- src; } while (-- c);
}

/* -------------------------------------------------------------------------- */

public int string_merges(m_string *string, const char *pattern, size_t len)
{
    /** @brief replaces separators in a split string by a new separator */

    int i = 0, p = 0, diff = 0, new_size = 0;
    unsigned int last = 0, known_good = 0;

    if (! string || ! DATA(string)) {
        debug("string_merges(): bad parameters.\n");
        return -1;
    }

    /* allow NULL pattern only if a 0 length is specified */
    if (! pattern && len) {
        debug("string_merges(): NULL pattern with non-0 length.\n");
        return -1;
    }

    /* at least 2 tokens are required to merge anything */
    if (! string->token || string->parts < 2) {
        debug("string_merges(): not enough tokens.\n");
        return -1;
    }

    /* get the size difference with the previous delimiter length */
    for (i = 0; i < PARTS(string) - 1; i ++) {
        diff = len - (TOKEN_DATA(string, i + 1) - TOKEN_END(string, i));
        if (! (p += diff) && ! diff) {
            known_good = i + 1;
            if (likely(len == 1))
                *((char *) TOKEN_END(string, i)) = *pattern;
            else memcpy((char *) TOKEN_END(string, i), pattern, len);
        }
    }

    if (known_good == (unsigned int) i) return 0;

    new_size = SIZE(string) + p;

    if (p > 0) {
        string_extend(string, new_size);

        /* move every token starting from the end */
        for (last = PARTS(string) - 1; last -- > known_good; p -= diff) {
            diff = len - (TOKEN_DATA(string, last + 1) - TOKEN_END(string, last));

            __bcpy(TOKEN_DATA(string, last + 1),
                   (char *) TOKEN_DATA(string, last + 1) + p,
                   TOKEN_SIZE(string, last + 1));

            __move_subtokens(TOKEN(string, last + 1), p, NULL);

            if (likely(len == 1))
                *((char *) TOKEN_DATA(string, last + 1) - len) = *pattern;
            else memcpy((char *) TOKEN_DATA(string, last + 1) - len, pattern, len);
        }

        _string_update_size(string, new_size - SIZE(string));
    } else {
        /* move every token from the start */
        for (i = known_good; i < PARTS(string) - 1; i ++) {
            diff = len - (TOKEN_DATA(string, i + 1) - TOKEN_END(string, i));

            memmove((char *) TOKEN_DATA(string, i + 1) + diff,
                   TOKEN_DATA(string, i + 1), TOKEN_SIZE(string, i + 1));

            __move_subtokens(TOKEN(string, i + 1), diff, NULL);

            if (likely(len == 1))
                *((char *) TOKEN_DATA(string, i + 1) - len) = *pattern;
            else memcpy((char *) TOKEN_DATA(string, i + 1) - len, pattern, len);
        }

        string_dim(string, new_size);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

public int string_merge(m_string *string, const m_string *pattern)
{
    if (! string || ! pattern || ! DATA(pattern)) return -1;

    return string_merges(string, DATA(pattern), SIZE(pattern));
}

/* -------------------------------------------------------------------------- */

public m_string *string_reps(m_string *string, const char *search, size_t slen,
                             const char *rep, size_t rlen                      )
{
    /** @brief replace all occurences of a C substring in a string */

    size_t count = 0;
    off_t *offset = NULL, off = 0, src = 0, dst = 0;
    unsigned int i = 0;

    /* a NULL replacement string is allowed (deletion) */
    if (! string || ! DATA(string) || ! search || ! slen) {
        debug("string_reps(): bad parameters.\n");
        return NULL;
    }

    /* we can't replace a substring longer than the source string */
    if (SIZE(string) <= slen + 1) {
        debug("string_reps(): search string out of bound.\n");
        return NULL;
    }

    /* find the maximal number of replacement and allocate the offset array */
    count = SIZE(string) / slen + 1;

    if (! (offset = malloc(count * sizeof(*offset))) ) {
        perror(ERR(string_reps, malloc));
        return NULL;
    }

    /* fill the offset array with the location of the search strings */
    while ( (offset[i] = string_finds(string, off, search, slen)) >= 0) {
        off = offset[i] + slen; if (i < count) i ++; else break;
    }

    if ( (count = i) == 0) {
        debug("string_reps(): search string not found.\n");
        goto _err_nfnd;
    }

    /* XXX ensure the string has enough room for the replacement
       we could rely on string_movs for the resizing, but we really don't
       want subsequent calls to string_movs() to fail with a possible
       out-of-memory error and end up with a garbled string */
    if (string_extend(string, SIZE(string) + count * (rlen - slen)) == -1) {
        debug("string_reps(): resize failure.\n");
        goto _err_size;
    }

    /* perform the replacement */
    for (i = 0; i < count; i ++) {
        /* copy the data between the strings that are going to be replaced */
        if (! string_movs(string, dst, DATA(string) + src, offset[i] - src))
            goto _err_move;
        dst += offset[i] - src; src = offset[i] + slen;

        /* loop no further without proper replacement string */
        if (! rep || ! rlen) continue;

        /* replace the string */
        if (! string_movs(string, dst, rep, rlen)) goto _err_move;
        dst += rlen;
    }

    /* update the size */
    _string_update_size(string, count * (rlen - slen));

    /* ensure the string is NUL terminated */
    if (! string->parent) string->_data[string->_len] = '\0';

    free(offset);

    return string;

_err_move: /* this should never happen */
    debug("string_reps(): string_movs() failed !\n");
_err_size: /* string_extend() failure */
_err_nfnd: /* not found */
    free(offset);
    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_string *string_rep(m_string *string, const m_string *search,
                            const char *rep, size_t rlen             )
{
    /** @brief replace all occurences of a substring in a string */

    if (! string || ! search || ! DATA(search)) return NULL;

    return string_reps(string, DATA(search), SIZE(search), rep, rlen);
}

/* -------------------------------------------------------------------------- */

public m_string *string_rems(m_string *str, const char *rem, size_t len)
{
    /** @brief remove all occurences of a C substring in a string */

    if (! str || ! rem || ! len) {
        debug("string_rems(): bad parameters.\n");
        return NULL;
    }

    return string_reps(str, rem, len, NULL, 0);
}

/* -------------------------------------------------------------------------- */

public m_string *string_rem(m_string *string, const m_string *rem)
{
    /** @brief remove all occurences of a substring in a string */

    if (! string || ! DATA(string) || ! rem || ! DATA(rem)) return NULL;

    return string_reps(string, DATA(rem), SIZE(rem), NULL, 0);
}

/* -------------------------------------------------------------------------- */

public m_string *string_select(m_string *string, unsigned int off, size_t len)
{
    /** @brief create a string token encompassing the given part of string */

    if (! string || ! len) {
        debug("string_select(): bad parameters.\n");
        return NULL;
    }

    string_free_token(string);

    return string_add_token(string, off, off + len);
}

/* -------------------------------------------------------------------------- */

public m_string *string_add_token(m_string *s, off_t start, off_t end)
{
    /** @brief adds a new token to the string, selecting the delimited part */

    m_string *tokens = NULL, *token = NULL;
    unsigned int i = 0, j = 0;
    int p = 0;

    if (unlikely(! s || s->parts == 65535)) {
        debug("string_add_token(): cannot add token.\n");
        return NULL;
    }

    #ifdef DEBUG
    if (! DATA(s) || (size_t) end > SIZE(s) || end <= start) {
        debug("string_add_token(): bad parameters.\n");
        return NULL;
    }
    #endif

    if (s->_parts_alloc < s->parts + 1) {
        /* 1.5 growth factor */
        if (s->_parts_alloc < 43690)
            p = (s->_parts_alloc >> 1) + ! (s->_parts_alloc >> 1);
        else p = 65535 - s->_parts_alloc;

        tokens = realloc(s->token, (s->_parts_alloc + p) * sizeof(*tokens));
        if (unlikely(! tokens)) {
            perror(ERR(string_add_token, realloc));
            return NULL;
        }

        /* XXX update all the subtokens' pointers */
        if (unlikely(tokens != s->token)) {
            for (i = 0; i < s->parts; i ++) {
                for (j = 0; j < tokens[i].parts; j ++)
                    tokens[i].token[j].parent = & tokens[i];
            }
            s->token = tokens;
        }

        s->_parts_alloc += p;
    }

    s->parts ++; token = LAST_TOKEN(s);

    /* the token inherits the parent's flags and adds the "no free" bit */
    token->parent = s;
    token->_flags = s->_flags | _STRING_DISABLE_FREE;
    token->_data = s->_data + start;
    token->_alloc = token->_len = end - start;
    token->_parts_alloc = token->parts = 0;
    token->token = NULL;

    return token;
}

/* -------------------------------------------------------------------------- */

public int string_push_token(m_string *s, m_string *token)
{
    if (! s || ! token || ! DATA(s) || ! DATA(token)) {
        debug("string_push_token(): bad parameters.\n");
        return -1;
    }

    return string_push_tokens(s, DATA(token), SIZE(token));
}

/* -------------------------------------------------------------------------- */

public int string_push_tokens(m_string *s, const char *strtoken, size_t len)
{
    size_t slen = 0;

    if (! s || ! strtoken || ! len) {
        debug("string_push_tokens(): bad parameters.\n");
        return -1;
    }

    if (PARTS(s))
        slen = DATA(LAST_TOKEN(s)) - DATA(s) + SIZE(LAST_TOKEN(s));
    else slen = 0;

    /* make room for the new data */
    if (string_extend(s, slen + len + 1) == -1) return -1;

    /* copy the data */
    memcpy(s->_data + slen, strtoken, len);
    s->_len = slen + len; s->_data[SIZE(s)] = '\0';

    /* append the token */
    if (! string_add_token(s, slen, slen + len)) return -1;

    return 0;
}

/* -------------------------------------------------------------------------- */

public m_string *string_suppr_token(m_string *s, unsigned int index)
{
    #ifdef DEBUG
    if (! s || ! DATA(s) || ! PARTS(s) || index >= PARTS(s)) {
        debug("string_suppr_token(): bad parameters.\n");
        return NULL;
    }
    #endif

    if (string_suppr(s, TOKEN_DATA(s, index) - DATA(s), TOKEN_SIZE(s, index)) == -1)
        return NULL;

    if (index == -- s->parts && ! s->parent) s->_data[SIZE(s)] = '\0';

    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_string *string_pop_token(m_string *s)
{
    m_string *ret = NULL;

    if (! s || ! DATA(s) || ! PARTS(s) || ! TOKEN_SIZE(s, 0)) {
        debug("string_pop_token(): bad parameters.\n");
        return NULL;
    }

    if (! (ret = string_alloc(TOKEN_DATA(s, 0), TOKEN_SIZE(s, 0))) )
        return NULL;

    string_suppr(TOKEN(s, 0), 0, TOKEN_SIZE(s, 0));

    return ret;
}

/* -------------------------------------------------------------------------- */
#ifdef HAS_PCRE
/* -------------------------------------------------------------------------- */

public int string_parse(m_string *string, const char *pattern, size_t len)
{
    /** @brief parses a string into multiple tokens matching a regex */

    int *off = NULL;
    unsigned int i = 0, j = 0, k = 0, last = 0, size = 0;
    m_string *token = NULL, *new_token = NULL;
    pcre *regex = NULL;
    int erroff = 0;
    int r = 0;
    const char *err = NULL;

    if (! string || ! DATA(string) || ! pattern || ! len) {
        debug("string_parse(): bad parameters.\n");
        return -1;
    }

    if (! (regex = pcre_compile(pattern, PCRE_DOTALL, & err, & erroff, NULL)) ) {
        debug("string_parse(): %s\n", err);
        return -1;
    }

    size = SIZE(string);

    if (size * sizeof(*off) < size) {
        debug("string_parse(): integer overflow.\n");
        return -1;
    }

    if (! (off = malloc(size * sizeof(*off))) ) {
        perror(ERR(string_parse, malloc));
        goto _err_malloc;
    }

    for (i = 0, last = 0; ; last = off[1], i ++) {
        /* match the pattern */
        r = pcre_exec(regex, NULL, DATA(string), SIZE(string), last, 0x0, off, size);

        if (r <= 0) {
            if (i) break;
            debug("string_parse(): error matching pattern %s\n", pattern);
            goto _err_exec;
        }

        /* found something, add a token and possibly subtokens */
        if (! (new_token = realloc(token, (i + 1) * sizeof(*token))) ) {
            perror(ERR(string_parse, realloc));
            if (token) while (i --) free(token[i].token); free(token);
            goto _err_token;
        }

        token = new_token;
        token[i].parent = string;
        token[i]._flags = string->_flags | _STRING_DISABLE_FREE;
        token[i]._data = string->_data + off[0];
        token[i]._len = off[1] - off[0] + ! (off[1] - off[0]);
        token[i]._alloc = token[i]._len;

        if (r - 1 < 1) {
            token[i]._parts_alloc = token[i].parts = 0;
            token[i].token = NULL;
            continue;
        }

        token[i]._parts_alloc = token[i].parts = r - 1;

        if (! (token[i].token = malloc(token[i].parts * sizeof(token[i]))) ) {
            perror(ERR(string_parse, malloc));
            goto _err_token;
        }

        for (j = 2, k = 0; j + 1 < (unsigned) r * 2; j += 2, k ++) {
            token[i].token[k].parent = & token[i];
            token[i].token[k]._flags = string->_flags | _STRING_DISABLE_FREE;
            token[i].token[k]._data = string->_data + off[j];
            token[i].token[k]._len = off[j + 1] - off[j] + ! (off[j + 1] - off[j]);
            token[i].token[k]._alloc = token[i].token[k]._len;
            token[i].token[k]._parts_alloc = token[i].token[k].parts = 0;
            token[i].token[k].token = NULL;
        }
    }

    pcre_free(regex);

    free(off);

    string_free_token(string); string->parts = i; string->token = token;

    return 0;

_err_token:
    if (token) while (i --) free(token[i].token); free(token);
_err_exec:
    free(off);
_err_malloc:
    pcre_free(regex);

    return -1;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public m_string *string_b58s(const char *s, size_t size)
{
    /** @brief convert a C string to base58 encoding */

    size_t i = 0, zcount = 0;
    int j = 0, high = 0, carry = 0;
    int32_t b58size = 0;
    uint8_t *buffer = NULL;
    m_string *ret = NULL;

    if (! s || ! size) {
        debug("string_b58s(): bad parameters.\n");
        return NULL;
    }

    /* get the number of leading NUL chars */
    while (zcount < size && ! s[zcount]) zcount ++;

    /* compute the size of the base58 encoded string, with a trailing NUL */
    b58size = (size - zcount) * 138 / 100 + 1;

    if ((size_t) b58size < size) {
        debug("string_b58s(): integer overflow.\n");
        return NULL;
    }

    if (! (buffer = calloc(b58size, sizeof(*buffer))) ) {
        perror(ERR(string_b58s, calloc));
        return NULL;
    }

    for (i = zcount, high = b58size - 1; i < size; i ++, high = j) {
        for (carry = (uint8_t) s[i], j = b58size - 1; j > high || carry; j --) {
            carry += 256 * buffer[j];
            buffer[j] = carry % 58;
            carry /= 58;
        }
    }

    for (j = 0; j < b58size && ! buffer[j]; j ++);

    if (! (ret = string_alloc(NULL, zcount + b58size - j)) )
        goto _err_alloc;

    if (zcount) memset(ret->_data, '1', zcount);

    for (i = zcount; j < b58size; i ++, j ++)
        ret->_data[i] = _b58[buffer[j]];

    ret->_data[i] = '\0'; ret->_len = i;

_err_alloc:
    free(buffer);

    return ret;
}

/* -------------------------------------------------------------------------- */

public m_string *string_b58(const m_string *s)
{
    /** @brief convert a string to base58 encoding */

    return string_b58s(DATA(s), SIZE(s));
}

/* -------------------------------------------------------------------------- */

public m_string *string_deb58s(const char *s, size_t size)
{
    /** @brief convert a base58 encoded C string to plain text */

    uint32_t *r = NULL;
    size_t i = 0, zcount = 0, remain = 0;
    int j = 0, outlen = 0;
    uint64_t b = 0;
    uint32_t c = 0, mask = 0;
    m_string *ret = NULL;

    if (! s || ! size) {
        debug("string_deb58s(): bad parameters.\n");
        return NULL;
    }

    /* output buffer */
    if (! (r = calloc( (outlen = (size + 3) / 4) + 1, sizeof(*r))) ) {
        perror(ERR(string_deb58s, calloc));
        return NULL;
    }

    /* string wrapper */
    if (! (ret = string_alloc(NULL, 0)) ) {
        debug("string_deb58s(): out of memory.\n");
        free(r);
        return NULL;
    }

    /* legitimate leading 0s */
    while (zcount < size && s[zcount] == '1') zcount ++;

    if ( (remain = size & 3) ) mask = 0xffffffff << (remain * 8);

    for (i = zcount; i < size; i ++) {
        if ((int) (c = _D58(s[i])) == -1) goto _panic;

        for (j = outlen - 1; j; j --) {
            b = ((uint64_t) r[j]) * 58 + c;
            c = (b & 0x3f00000000) >> 32;
            r[j] = b & 0xffffffff;
        }

        /* output was too large */
        if (c || r[0] & mask) goto _panic;
    }

    c = r[(i = 0)]; ret->_data = (char *) r;

    switch (remain) {
    case 3: { ret->_data[i ++] = (c & 0xff0000) >> 16; }
    case 2: { ret->_data[i ++] = (c & 0xff00) >> 8; }
    case 1: { ret->_data[i ++] = (c & 0xff); j = 1; goto _loop; }
    }

    for (j = 0; j < outlen; j ++) {
_loop:  c = r[j];
        ret->_data[i ++] = (c >> 0x18) & 0xff;
        ret->_data[i ++] = (c >> 0x10) & 0xff;
        ret->_data[i ++] = (c >> 0x08) & 0xff;
        ret->_data[i ++] = (c & 0xff);
    }

    /* check if there is spurious remaining 0s */
    for (remain = 0; remain < i && ! ret->_data[remain]; remain ++);
    if (zcount > remain) goto _panic;

    /* real length */
    ret->_len = i - (remain - zcount);

    /* remove spurious leading 0s */
    memmove(ret->_data, ret->_data + remain - zcount, ret->_len);
    ret->_data[ret->_len] = '\0';

    /* package the buffer */
    ret->_flags = 0; ret->_alloc = outlen * sizeof(*r);
    ret->_parts_alloc = ret->parts = 0; ret->token = NULL;

    return ret;

_panic:
    free(r); string_free(ret);
    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_string *string_deb58(const m_string *s)
{
    /** @brief convert a base58 encoded string to plain text */

    return string_deb58s(DATA(s), SIZE(s));
}

/* -------------------------------------------------------------------------- */

public m_string *string_b64s(const char *s, size_t size, size_t linesize)
{
    /** @brief convert a C string to base64 encoding */

    size_t i = 0;
    int j = 0, crlf = 0;
    int32_t b64size = 0;
    char *r = NULL;
    m_string *ret = NULL;

    if (! s || ! size) {
        debug("string_b64s(): bad parameters.\n");
        return NULL;
    }

    /* reject bogus linesize */
    if ( (linesize) && ((linesize % 4) || (linesize > 76)) ) {
        debug("string_b64s(): bad line size.\n");
        return NULL;
    }

    /* compute the size of the base64 encoded string, with a trailing NUL */
    b64size = (((size + 3 - (size % 3)) / 3) * 4) + 1;

    /* add some room for CRLF depending on the linesize */
    if (linesize) b64size += ((b64size / linesize) * 2) + 2;

    if ((size_t) b64size < size) {
        debug("string_b64s(): integer overflow.\n");
        return NULL;
    }

    if (! (r = calloc(b64size, sizeof(*r))) ) {
        perror(ERR(string_b64s, calloc));
        return NULL;
    }

    while (i + 3 < size) {

        if (j + 4 < b64size) {
            /* convert three bytes in ASCII characters */
            r[j ++] = _b64[(s[i] & 0xfc) >> 2];
            r[j ++] = _b64[((s[i] & 0x03) << 4) | ((s[i + 1] & 0xf0) >> 4)];
            r[j ++] = _b64[((s[i + 1] & 0x0f) << 2) | ((s[i + 2] & 0xc0) >> 6)];
            r[j ++] = _b64[s[i + 2] & 0x3f]; i += 3;
        } else goto _panic;

        /* if a linesize was given, check if we should insert a CRLF */
        if (linesize && ((j - crlf) % linesize == 0) ) {
            if (j + 2 < b64size) {
                r[j ++] = '\r'; r[j ++] = '\n'; crlf += 2;
            } else goto _panic;
        }
    }

    if ((size + 1 - i) && j + 4 < b64size) {
        /* add some padding */
        memset(r + j, '=', 4);

        /* process the remaining */
        switch (size + 1 - i) {
        case 4:
        r[j + 3] = _b64[s[i + 2] & 0x3f];
        case 3:
        r[j + 2] = _b64[(s[i + 1] & 0x0f) << 2 | (s[i + 2] & 0xc0) >> 6];
        case 2:
        r[j + 1] = _b64[(s[i] & 0x03) << 4 | (s[i + 1] & 0xf0) >> 4];
        case 1:
        r[j] = _b64[(s[i] & 0xfc) >> 2]; j += 4;
        }
    }

    /* add the last CRLF if a linesize was provided */
    if ( (linesize) && j + 2 < b64size) {
        r[j] = '\r'; r[j + 1] = '\n';
    }

    /* package the buffer in a string_m */
    if (! (ret = string_alloc(NULL, 0)) ) {
        free(r);
    } else {
        ret->_flags = 0; ret->_data = r;
        ret->_len = b64size - 1; ret->_alloc = b64size -1;
        ret->_parts_alloc = ret->parts = 0;
        ret->token = NULL;
    }

    return ret;

_panic:
    debug("string_b64s(): base64 encoding failed.\n");
    free(r);
    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_string *string_b64(const m_string *s, size_t linesize)
{
    /** @brief convert a string to base64 encoding */

    return string_b64s(DATA(s), SIZE(s), linesize);
}

/* -------------------------------------------------------------------------- */

public m_string *string_deb64s(const char *s, size_t size)
{
    /** @brief convert a base64 encoded C string to plain text */

    char *r = NULL;
    size_t i = 0, j = 0;
    m_string *ret = NULL;
    int state = 4;

    if (! s || ! size) {
        debug("string_deb64s(): bad parameters.\n");
        return NULL;
    }

    /* the number of CRLF or other trailing characters is unknown,
       so allocate the same size to be safe */
    if (! (r = calloc(size + 1, sizeof(*r))) ) {
        perror(ERR(string_deb64s, calloc));
        return NULL;
    }

    if (! (ret = string_alloc(NULL, 0)) ) {
        debug("string_deb64s(): out of memory.\n");
        free(r);
        return NULL;
    }

    /* discard trailing characters */
    while (_d64[(unsigned char) s[size - 1]] == -1 && -- size);

    for (i = 0; i < size; i ++) {
        int d = _d64[(unsigned char) s[i]];
        if (likely(d >= 0)) {
            switch (state) {
            case 4: r[j] = d << 2; state --; break;
            case 3: r[j ++] |= d >> 4; r[j] = d << 4; state --; break;
            case 2: r[j ++] |= d >> 2; r[j] = (d << 6) & 0xc0; state --; break;
            case 1: r[j++] |= d; state = 4;
            }
        } else if (d == -1) goto _panic;
    }

    if (state == 3 || ! j) goto _panic;

    /* package the buffer */
    ret->_flags = 0; ret->_data = r;
    ret->_len = j; ret->_alloc = i;
    ret->_parts_alloc = ret->parts = 0;
    ret->token = NULL;

    return ret;

_panic:
    ret = string_free(ret);
    free(r);
    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_string *string_deb64(const m_string *s)
{
    /** @brief convert a base64 encoded string to plain text */

    return string_deb64s(DATA(s), SIZE(s));
}

/* -------------------------------------------------------------------------- */
#ifdef HAS_ZLIB
/* -------------------------------------------------------------------------- */

public m_string *string_compress(m_string *s)
{
    m_string *z = NULL;
    Bytef *dest = NULL;
    uLongf dlen = 0;

    if (! s || ! s->_data || ! SIZE(s)) return NULL;

    if (! (z = string_alloc(NULL, ((SIZE(s) * 11)/10 + 12))) ) {
        debug("string_compress(): cannot allocate string.\n");
        return NULL;
    }

    dest = (Bytef *) z->_data; dlen = z->_len;

    if (compress2(dest, & dlen, (Bytef *) DATA(s), (uLong) SIZE(s), 9) != 0) {
        debug("string_compress(): gzip compression failed.\n");
        return string_free(z);
    }

    z->_len = dlen;

    return z;
}

/* -------------------------------------------------------------------------- */

public m_string *string_uncompress(m_string *s, size_t original_size)
{
    m_string *z = NULL;
    Bytef *dest = NULL;
    uLongf dlen = 0;

    if (! s || ! s->_data || ! SIZE(s)) return NULL;

    if (! (z = string_alloc(NULL, original_size)) ) {
        debug("string_uncompress(): cannot allocate string.\n");
        return NULL;
    }

    dest = (Bytef *) z->_data; dlen = z->_len;

    switch (uncompress(dest, & dlen, (Bytef *) DATA(s), (uLong) SIZE(s))) {
    case Z_OK: z->_len = dlen; return z;
    case Z_MEM_ERROR: debug("string_uncompress(): out of memory.\n"); break;
    case Z_DATA_ERROR: debug("string_uncompress(): data corruption.\n"); break;
    case Z_BUF_ERROR: debug("string_uncompress(): not enough memory.\n"); break;
    default: debug("string_uncompress(): error uncompressing data.\n");
    }

    return string_free(z);
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* SHA1 */
/* -------------------------------------------------------------------------- */

#if defined(__GNUC__) && defined(__i386__)
static inline uint32_t rol(uint32_t x, int n)
{
    __asm__("roll %%cl,%0" :"=r" (x) :"0" (x),"c" (n));
    return x;
}
#else
#define rol(x,n) ( ((x) << (n)) | ((x) >> (32 - (n))) )
#endif

/* -------------------------------------------------------------------------- */

static void _sha1_init(SHA1_CONTEXT *hd)
{
    hd->h0 = 0x67452301;
    hd->h1 = 0xefcdab89;
    hd->h2 = 0x98badcfe;
    hd->h3 = 0x10325476;
    hd->h4 = 0xc3d2e1f0;
    hd->nblocks = 0;
    hd->count = 0;
}

/* -------------------------------------------------------------------------- */

static void _sha1_transform(SHA1_CONTEXT *hd, unsigned char *data)
{
    /* transform the message X which consists of 16 32-bit-words */

    uint32_t a, b, c, d, e, tm;
    uint32_t x[16];

    /* get values from the chaining vars */
    a = hd->h0;
    b = hd->h1;
    c = hd->h2;
    d = hd->h3;
    e = hd->h4;

    #ifdef BIG_ENDIAN_HOST
    memcpy(x, data, 64);
    #else /* little endian */
    {
        int i;
        unsigned char *p2;

        for (i = 0, p2 = (unsigned char *) x; i < 16; i ++, p2 += 4) {
            p2[3] = *data ++;
            p2[2] = *data ++;
            p2[1] = *data ++;
            p2[0] = *data ++;
        }
    }
    #endif

    #define K1 0x5a827999L
    #define K2 0x6ed9eba1L
    #define K3 0x8f1bbcdcL
    #define K4 0xca62c1d6L
    #define F1(x, y, z) (z ^ (x & (y ^ z)))
    #define F2(x, y, z) (x ^ y ^ z)
    #define F3(x, y, z) ( (x & y) | (z & (x | y)) )
    #define F4(x, y, z) (x ^ y ^ z)

    #define M(i) (tm = x[i & 0xf] ^ x[(i - 14) & 0xf]   \
                  ^ x[(i - 8) & 0xf] ^ x[(i - 3) & 0xf] \
                  , (x[i & 0xf] = rol(tm, 1)) )

    #define R(a,b,c,d,e,f,k,m)               \
    do {                                     \
        e += rol(a, 5) + f(b, c, d) + k + m; \
        b = rol(b, 30);                      \
    } while (0)

    R(a, b, c, d, e, F1, K1, x[ 0]);
    R(e, a, b, c, d, F1, K1, x[ 1]);
    R(d, e, a, b, c, F1, K1, x[ 2]);
    R(c, d, e, a, b, F1, K1, x[ 3]);
    R(b, c, d, e, a, F1, K1, x[ 4]);
    R(a, b, c, d, e, F1, K1, x[ 5]);
    R(e, a, b, c, d, F1, K1, x[ 6]);
    R(d, e, a, b, c, F1, K1, x[ 7]);
    R(c, d, e, a, b, F1, K1, x[ 8]);
    R(b, c, d, e, a, F1, K1, x[ 9]);
    R(a, b, c, d, e, F1, K1, x[10]);
    R(e, a, b, c, d, F1, K1, x[11]);
    R(d, e, a, b, c, F1, K1, x[12]);
    R(c, d, e, a, b, F1, K1, x[13]);
    R(b, c, d, e, a, F1, K1, x[14]);
    R(a, b, c, d, e, F1, K1, x[15]);
    R(e, a, b, c, d, F1, K1, M(16));
    R(d, e, a, b, c, F1, K1, M(17));
    R(c, d, e, a, b, F1, K1, M(18));
    R(b, c, d, e, a, F1, K1, M(19));
    R(a, b, c, d, e, F2, K2, M(20));
    R(e, a, b, c, d, F2, K2, M(21));
    R(d, e, a, b, c, F2, K2, M(22));
    R(c, d, e, a, b, F2, K2, M(23));
    R(b, c, d, e, a, F2, K2, M(24));
    R(a, b, c, d, e, F2, K2, M(25));
    R(e, a, b, c, d, F2, K2, M(26));
    R(d, e, a, b, c, F2, K2, M(27));
    R(c, d, e, a, b, F2, K2, M(28));
    R(b, c, d, e, a, F2, K2, M(29));
    R(a, b, c, d, e, F2, K2, M(30));
    R(e, a, b, c, d, F2, K2, M(31));
    R(d, e, a, b, c, F2, K2, M(32));
    R(c, d, e, a, b, F2, K2, M(33));
    R(b, c, d, e, a, F2, K2, M(34));
    R(a, b, c, d, e, F2, K2, M(35));
    R(e, a, b, c, d, F2, K2, M(36));
    R(d, e, a, b, c, F2, K2, M(37));
    R(c, d, e, a, b, F2, K2, M(38));
    R(b, c, d, e, a, F2, K2, M(39));
    R(a, b, c, d, e, F3, K3, M(40));
    R(e, a, b, c, d, F3, K3, M(41));
    R(d, e, a, b, c, F3, K3, M(42));
    R(c, d, e, a, b, F3, K3, M(43));
    R(b, c, d, e, a, F3, K3, M(44));
    R(a, b, c, d, e, F3, K3, M(45));
    R(e, a, b, c, d, F3, K3, M(46));
    R(d, e, a, b, c, F3, K3, M(47));
    R(c, d, e, a, b, F3, K3, M(48));
    R(b, c, d, e, a, F3, K3, M(49));
    R(a, b, c, d, e, F3, K3, M(50));
    R(e, a, b, c, d, F3, K3, M(51));
    R(d, e, a, b, c, F3, K3, M(52));
    R(c, d, e, a, b, F3, K3, M(53));
    R(b, c, d, e, a, F3, K3, M(54));
    R(a, b, c, d, e, F3, K3, M(55));
    R(e, a, b, c, d, F3, K3, M(56));
    R(d, e, a, b, c, F3, K3, M(57));
    R(c, d, e, a, b, F3, K3, M(58));
    R(b, c, d, e, a, F3, K3, M(59));
    R(a, b, c, d, e, F4, K4, M(60));
    R(e, a, b, c, d, F4, K4, M(61));
    R(d, e, a, b, c, F4, K4, M(62));
    R(c, d, e, a, b, F4, K4, M(63));
    R(b, c, d, e, a, F4, K4, M(64));
    R(a, b, c, d, e, F4, K4, M(65));
    R(e, a, b, c, d, F4, K4, M(66));
    R(d, e, a, b, c, F4, K4, M(67));
    R(c, d, e, a, b, F4, K4, M(68));
    R(b, c, d, e, a, F4, K4, M(69));
    R(a, b, c, d, e, F4, K4, M(70));
    R(e, a, b, c, d, F4, K4, M(71));
    R(d, e, a, b, c, F4, K4, M(72));
    R(c, d, e, a, b, F4, K4, M(73));
    R(b, c, d, e, a, F4, K4, M(74));
    R(a, b, c, d, e, F4, K4, M(75));
    R(e, a, b, c, d, F4, K4, M(76));
    R(d, e, a, b, c, F4, K4, M(77));
    R(c, d, e, a, b, F4, K4, M(78));
    R(b, c, d, e, a, F4, K4, M(79));

    /* update chaining vars */
    hd->h0 += a;
    hd->h1 += b;
    hd->h2 += c;
    hd->h3 += d;
    hd->h4 += e;
}

/* -------------------------------------------------------------------------- */

static void _sha1_write(SHA1_CONTEXT *hd, const char *inbuf, size_t inlen)
{
    /* update the message digest with the contents of INBUF with length INLEN */

    if (hd->count == 64) {
        /* flush the buffer */
        _sha1_transform(hd, hd->buf);
        hd->count = 0;
        hd->nblocks ++;
    }

    if (! inbuf) return;

    if (hd->count) {
        for ( ; inlen && hd->count < 64; inlen --)
            hd->buf[hd->count ++] = *inbuf ++;

        _sha1_write(hd, NULL, 0);

        if (! inlen) return;
    }

    while (inlen >= 64) {
        _sha1_transform(hd, (unsigned char *) inbuf);
        hd->count = 0;
        hd->nblocks ++;
        inlen -= 64;
        inbuf += 64;
    }

    for ( ; inlen && hd->count < 64; inlen --)
        hd->buf[hd->count ++] = *inbuf ++;
}

/* -------------------------------------------------------------------------- */

static void _sha1_final(SHA1_CONTEXT *hd)
{
    /* the routine final terminates the computation and returns the digest.
       the handle is prepared for a new cycle, but adding bytes to the handle
       will destroy the returned buffer.
       returns: 20 bytes representing the digest. */

    uint32_t t, msb, lsb;
    unsigned char *p;

    /* flush */
    _sha1_write(hd, NULL, 0);

    t = hd->nblocks;

    /* multiply by 64 to make a byte count */
    lsb = t << 6;
    msb = t >> 26;

    /* add the count */
    t = lsb;
    if ( (lsb += hd->count) < t)
    msb ++;

    /* multiply by 8 to make a bit count */
    t = lsb;
    lsb <<= 3;
    msb <<= 3;
    msb |= t >> 29;

    if (hd->count < 56) {
        /* enough room, add some padding */
        hd->buf[hd->count ++] = 0x80;
        while (hd->count < 56) hd->buf[hd->count ++] = 0;
    } else {
        /* need one extra block */
        hd->buf[hd->count ++] = 0x80; /* pad character */
        while (hd->count < 64) hd->buf[hd->count ++] = 0;
        /* flush */
        _sha1_write(hd, NULL, 0);
        /* fill next block with zeroes */
        memset(hd->buf, 0, 56);
    }

    /* append the 64 bit count */
    hd->buf[56] = msb >> 24;
    hd->buf[57] = msb >> 16;
    hd->buf[58] = msb >>  8;
    hd->buf[59] = msb      ;
    hd->buf[60] = lsb >> 24;
    hd->buf[61] = lsb >> 16;
    hd->buf[62] = lsb >>  8;
    hd->buf[63] = lsb      ;
    _sha1_transform(hd, hd->buf);

    p = hd->buf;

    #ifdef BIG_ENDIAN_HOST
    #define X(a) do { *(uint32_t *) p = hd->h##a ; p += 4; } while(0)
    #else /* little endian */
    #define X(a)                                        \
    do {                                                \
        *p ++ = hd->h##a >> 24; *p ++ = hd->h##a >> 16; \
        *p ++ = hd->h##a >> 8; *p ++ = hd->h##a;        \
    } while(0)
    #endif

    X(0);
    X(1);
    X(2);
    X(3);
    X(4);

    #undef X
}

/* -------------------------------------------------------------------------- */

public m_string *string_sha1s(const char *string, size_t len)
{
    /** @brief returns the sha1 sum of the given string */

    SHA1_CONTEXT ctx;
    char *buf = NULL;
    unsigned int i = 0;
    m_string *ret = NULL;

    if (! string || ! len) return NULL;

    if (! (buf = malloc(41)) ) {
        perror(ERR(string_sha1s, malloc));
        return NULL;
    }

    _sha1_init(& ctx);
    _sha1_write(& ctx, (const char *) string, len);
    _sha1_final(& ctx);

    for (i = 0; i < 20; i ++) {
        buf[i * 2] = _hex[(ctx.buf[i] >> 4) & 0xf];
        buf[(i * 2) + 1] = _hex[ctx.buf[i] & 0xf];
    }
    buf[40] = 0;

    /* package the buffer in a string_m */
    if (! (ret = string_alloc(NULL, 0)) ) {
        free(buf);
    } else {
        ret->_flags = 0; ret->_data = buf;
        ret->_len = 40; ret->_alloc = 41;
        ret->_parts_alloc = ret->parts = 0;
        ret->token = NULL;
    }

    return ret;
}

/* -------------------------------------------------------------------------- */

public m_string *string_sha1(m_string *s)
{
    return string_sha1s(DATA(s), SIZE(s));
}

/* -------------------------------------------------------------------------- */
/* -- end of SHA1 implementation */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_HTTP
/* -------------------------------------------------------------------------- */

public char *string_rawurlencode(const char *url, size_t len, int flags)
{
    char *buf = NULL;
    int32_t bufsize = 0;
    const char *p = url;
    char *q = NULL;

    bufsize = (len * 3) + 1;

    if (bufsize * sizeof(*buf) < len) {
        debug("string_rawurlencode(): integer overflow.\n");
        return NULL;
    }

    if (! (q = buf = malloc(bufsize * sizeof(*buf))) ) {
        perror(ERR(string_rawurlencode, malloc));
        return NULL;
    }

    do {
        switch (_unsafe[(int) *p]) {
        case 1: /* unsafe chars */
        case 2: /* control chars */
        case 3: /* 0x7f */
        case 4: /* non US-ASCII */
                goto _encode;
        case 5: /* reserved chars */
                if (flags & RFC1738_ESCAPE_RESERVED) goto _encode;
        case 6: /* escape char (%) */
                if (~flags & RFC1738_ESCAPE_UNESCAPED) goto _encode;
        default:
                /* copy the char unencoded */
                *q ++ = *p ++;
                continue;
        }

_encode:
        *q ++ = '%';
        *q ++ = _hex[(*p >> 4) & 0xf];
        *q ++ = _hex[*p ++ & 0xf];
    } while (p < url + len && q < buf + bufsize);

    *q = '\0';

    return buf;
}

/* -------------------------------------------------------------------------- */

public int string_urlencode(m_string *url, int flags)
{
    char *encoded = NULL;
    size_t len = 0;

    if (! url || ! DATA(url)) return -1;

    if (! (encoded = string_rawurlencode(DATA(url), SIZE(url), flags)) )
        return -1;

    len = strlen(encoded);

    /* replace the original data */
    if (string_extend(url, len) == -1) {
        debug("string_urlencode(): cannot resize the string.\n");
        free(encoded);
        return -1;
    }

    memcpy((void *) DATA(url), encoded, len);

    free(encoded);

    return 0;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_JSON
/* -------------------------------------------------------------------------- */

/* tokenizer states */
#define _KEY 0x01   /* key expected */
#define _VAL 0x02   /* value expected */
#define _SIG 0x04   /* '+' or '-' sign found */
#define _RAD 0x08   /* '.' decimal separator (radix point) found */
#define _EXP 0x10   /* 'e' or 'E' exponent found */
#define _BUG 0x20   /* only used in quirks mode */
#define _EOF 0x40   /* reached the End Of File */
#define _INC 0x80   /* incomplete input */

public int string_parse_json(m_string *s, char strict, m_json_parser *ctx)
{
    unsigned int pos = 0;
    unsigned char class = 0;
    char *p = NULL, state = 0, leading_digit = 0;
    m_string *json = s, *parent = NULL;
    int callback = 0, prealloc = 2;

    if (! json) {
        debug("string_parse_json(): bad parameters.\n");
        return -1;
    }

    /* check if we should resume parsing */
    if (PARTS(json) && IS_TYPE(LAST_TOKEN(json), JSON_TYPE)) {
        if (IS_BUFFER(s)) {
            /* the maximum amount of tokens was reached */
            for (json = LAST_TOKEN(s); PARTS(json); json = LAST_TOKEN(json)) {
                if (PARTS(LAST_TOKEN(json)) == 65535) {
                    int i = 0;

                    json = LAST_TOKEN(json);

                    pos = DATA(LAST_TOKEN(json)) - DATA(json) +
                          SIZE(LAST_TOKEN(json)) + 1;

                    for (i = 0; i < 65535; i ++)
                        string_free_token(json->token + i);
                    json->parts = 0;

                    s->_flags &= ~_STRING_BUFFERING;
                    break;
                }
            }
        } else if (IS_ERROR(LAST_TOKEN(json))) {
            /* input was incomplete */
            state |= _INC;

            while (PARTS(json)) {
                if (! IS_ERROR(LAST_TOKEN(json))) {
                    /* restart parsing at the last known-good character */
                    pos = STRING_END(LAST_TOKEN(json)) - DATA(json);

                    if (IS_STRING(LAST_TOKEN(json))) {
                        /* last character must be a quote */
                        if (json->_data[pos] == LAST_TOKEN(json)->_data[-1])
                            pos ++;
                    }

                    /* odd count of object elements means a key is expected */
                    if (IS_OBJECT(json) && (PARTS(json) & 1)) state |= _KEY;

                    break;
                } else {
                    /* number, as bool/null only emit a token if complete */
                    if (IS_PRIMITIVE(LAST_TOKEN(json))) {
                        /* discard incomplete number */
                        json->parts --;
                        continue;
                    }
                    json = LAST_TOKEN(json);
                }
            }

            if (! PARTS(json)) {
                if (IS_OBJECT(json->parent) && (PARTS(json->parent) & 1))
                    state |= _KEY;

                if (IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                    /* skip the opening bracket */
                    pos = (
                        (IS_OBJECT(json) && *json->_data == '{') ||
                        (IS_ARRAY(json) && *json->_data == '[')
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
        if (! memcmp(DATA(json), "\xef\xbb\xbf", MIN(SIZE(json), 3)))
            pos += 3;
        string_free_token(json);
    }

    if (ctx) ctx->strict = strict;

    for ( ; pos < SIZE(json); pos ++) {

        class = _j[(uint8_t) json->_data[pos]];

        if (class > 3) {
            if (unlikely(IS_STRING(json))) {
                for (++ pos; pos < SIZE(json); pos ++) {
                    class = _j[(uint8_t) json->_data[pos]];
                    if (class < 4) goto _parse;
                }
                return 0; /* EOF */
            } else p = (char *) DATA(json) + pos;
        }

_parse: switch (class) {

        /* ', " */
        case QUOTE: {
            if (IS_STRING(json)) {
                /* check if the quotes are matching */
                if (json->_data[-1] != json->_data[pos])
                    break;
                json->_len = json->_alloc = pos;
                goto _delim;
            } else if (unlikely(IS_PRIMITIVE(json))) {
                debug("string_parse_json(): unexpected quotation mark.\n");
                goto _error;
            }

            if (strict) {
                if (unlikely(json->_data[pos] == '\'')) {
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

            if (likely(pos + 1 < SIZE(json))) {
                json = string_add_token(json, pos + 1, SIZE(json));
                if (unlikely(! json)) goto _nomem;

                json->_flags &= ~JSON_TYPE;
                json->_flags |= (JSON_STRING | _STRING_HAS_ERROR);
                state &= ~_VAL; pos = -1;
            } else return 0; /* EOF */

            {   /* optimize for long strings */
                uint32_t bytes;
                if (likely(pos + sizeof(bytes) < SIZE(json))) {
                    memcpy(& bytes, json->_data, sizeof(bytes));
                    if (__zero(bytes ^ (~0U / 255 * json->_data[-1])))
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

            if (unlikely(pos + 1 == SIZE(json))) {
                debug("string_parse_json(): incomplete escape sequence.\n");
                return 0; /* EOF */
            }

            switch (json->_data[++ pos]) {
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
                if (likely(pos + 1 + sizeof(bytes) < SIZE(json))) {
                    memcpy(& bytes, json->_data + pos + 1, sizeof(bytes));
                    if (! __less(bytes, '0') && ! __more(bytes, 'f'))
                    if (likely(! __between(bytes, '9', 'A')))
                    if (likely(! __between(bytes, 'F', 'a'))) {
                        pos += sizeof(bytes); break;
                    }
                } else {
                    for (pos = pos + 1; pos < SIZE(json); pos ++) {
                        if (json->_data[pos] < '0' || json->_data[pos] > 'f')
                            goto _error;
                        if (json->_data[pos] > '9' && json->_data[pos] < 'A')
                            goto _error;
                        if (json->_data[pos] > 'F' && json->_data[pos] < 'a')
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
            json->_flags = (json->_flags & ~JSON_TYPE) |
                           (-(state & _BUG) & JSON_STRING) |
                           (-(! (state & _BUG)) & JSON_PRIMITIVE);
            state = (state & ~_KEY) | (-(IS_STRING(json) > 0) & _KEY);
            state &= ~_BUG;
            leading_digit = 0;
            json->_len = json->_alloc = pos;
            goto _delim;
            break;
        } break;

        /* f, n, t */
        case PRIMITIVE: if (likely(! IS_PRIMITIVE(json))) {
            if (IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                if (state & _KEY) {
                    /* QUIRK unquoted keys */
                    if (! strict) {
                        json = string_add_token(json, pos, SIZE(json));
                        if (unlikely(! json)) goto _nomem;

                        json->_flags &= ~JSON_TYPE;
                        json->_flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
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
            case 'f': switch (MIN((SIZE(json) - (pos + 1)), 4)) {
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
            case 'n': switch (MIN((SIZE(json) - (pos + 1)), 3)) {
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
            case 't': switch (MIN((SIZE(json) - (pos + 1)), 3)) {
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

            json = string_add_token(json, pos, SIZE(json));
            if (unlikely(! json)) goto _nomem;

            json->_flags &= ~JSON_TYPE; json->_flags |= JSON_PRIMITIVE;

            pos = p - DATA(json);

            goto _token;
        } else if (state & _BUG) break; goto _error; /* QUIRK unquoted keys */

        /* e, E */
        case DIGIT_EXP: {
            /* exponent cannot be placed right after a radix point */
            if ((state & _RAD) && p[-1] == '.') {
                debug("string_parse_json(): a fractional part is expected.\n");
                goto _error;
            }

            /* NOTE while it would seem more intuitive to check if a previous
                    exponent was set first, leading_digit may not be in cache
                    and accessing it below helps avoiding an expensive stall */
            if ((state & _EXP) || leading_digit == 0) {
                /* QUIRK unquoted keys */
                if (! strict) {
                    if (state & _KEY) {
                        json = string_add_token(json, pos, SIZE(json));
                        if (unlikely(! json)) goto _nomem;

                        json->_flags &= ~JSON_TYPE;
                        json->_flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
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
                if (likely(pos + 1 + sizeof(bytes) < SIZE(json))) {
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

                json = string_add_token(json, pos, SIZE(json));
                if (unlikely(! json)) goto _nomem;

                json->_flags &= ~JSON_TYPE;
                json->_flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
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
                if (likely(pos + 1 + sizeof(bytes) < SIZE(json))) {
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
                            json = string_add_token(json, pos, SIZE(json));
                            if (unlikely(! json)) goto _nomem;

                            json->_flags &= ~JSON_TYPE;
                            json->_flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
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

                json = string_add_token(json, pos, SIZE(json));
                if (unlikely(! json)) goto _nomem;

                json->_flags &= ~JSON_TYPE;
                json->_flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
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
                    json->_flags &= ~JSON_TYPE;
                    json->_flags |= JSON_STRING;
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
                ctx->key.current = DATA(LAST_TOKEN(json));
                ctx->key.len = SIZE(LAST_TOKEN(json));
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

            json = string_add_token(json, pos, SIZE(json));
            if (unlikely(! json)) goto _nomem;

            /* try to prealloc tokens */
            if (likely(json->token = malloc(prealloc * sizeof(*json->token))))
                json->_parts_alloc = prealloc;

            pos = (*p == '{');

            json->_flags &= ~JSON_TYPE;
            json->_flags |= (-(pos) & JSON_OBJECT) | (-(! pos) & JSON_ARRAY);
            json->_flags |= _STRING_HAS_ERROR;

            state = (state & ~_KEY) | (-(pos) & _KEY) | _VAL;

            /* skip space */
            pos = (p[1] == ' ');

            /* parser callback */
            if (ctx && ctx->init) {
                if (json->parent)
                    ctx->parent = json->parent->_flags & JSON_TYPE;
                else ctx->parent = 0;

                if ( (callback = ctx->init(json->_flags & JSON_TYPE, ctx)) )
                    return (callback == 1) ? 0 : -1;

                ctx->key.current = NULL; ctx->key.len = 0;
            }
        } break;

        /* }, ] */
        case OBJ_CLOSE: {
            if (strict) {
                if ((state & _KEY) && json->parts & 0x1) {
                    debug("string_parse_json(): a key is expected.\n");
                    goto _error;
                }

                if ((state & _VAL) && json->parts) {
                    debug("string_parse_json(): a value is expected.\n");
                    goto _error;
                }
            }

            if ((state & _VAL) && ! IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                debug("string_parse_json(): a value is expected.\n");
                goto _error;
            }

            state &= ~(_KEY | _VAL);
            prealloc = json->parts;

            /* NOTE checking in that order helps avoiding a costly stall */
            if (IS_PRIMITIVE(json)) {
                if (IS_TYPE(json->parent, JSON_ARRAY | JSON_OBJECT)) {
                    pos --; leading_digit = 0;
                    goto _token;
                }
            } else if (IS_TYPE(json, JSON_ARRAY | JSON_OBJECT)) {
                /* check if the brackets are matching */
                if (*json->_data == *p - 2) goto _token;

                if (state & _INC) goto _token;
                debug("string_parse_json(): mismatched bracket (%c).\n",
                      *json->_data);
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
                    json->_flags &= ~JSON_TYPE;
                    json->_flags |= JSON_STRING;
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
                } while (p < DATA(json) + SIZE(json));
            } else if (*p ++ == '*') {
                do {
                    uint32_t bytes;
                    memcpy(& bytes, p, sizeof(bytes));
                    if (__zero(bytes ^ 0x2a2a2a2aU)) {
                        while (*p ++ != '*');
                        if (*p == '/') break;
                    } else p += sizeof(bytes);
                } while (p < DATA(json) + SIZE(json));
            }

            if ((p - DATA(json)) - pos > 2) {
                pos = p - DATA(json); break;
            } else goto _error;
        }

        /* QUIRK unquoted keys */
        if (state & _KEY) {
            json = string_add_token(json, pos, SIZE(json));
            if (unlikely(! json)) goto _nomem;

            json->_flags &= ~JSON_TYPE;
            json->_flags |= (JSON_PRIMITIVE | _STRING_HAS_ERROR);
            pos = 0; state &= ~(_KEY | _VAL); state |= _BUG;
            break;
        } else if ((state & _BUG) && (IS_PRIMITIVE(json))) break;

        /* QUIRK hexadecimal numbers */
        if (json->_data[pos] == 'x') {
            if (leading_digit != '0' || ++ pos > 2) goto _error;

            do {
                uint32_t bytes;
                memcpy(& bytes, json->_data + pos, sizeof(bytes));
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
                if (json->_data[pos] < '0' || json->_data[pos] > 'f')
                    break;
                if (json->_data[pos] > '9' && json->_data[pos] < 'A')
                    break;
                if (json->_data[pos] > 'F' && json->_data[pos] < 'a')
                    break;
                pos ++;
            }

            /* '0x' without digits is not allowed */
            if (-- pos == 1) goto _error;

            break;
        }

        goto _error;

        }

        continue;

_token: if (likely(json->_len != pos))
        json->_len = json->_alloc = pos + 1;
_delim: json->_flags = (json->_flags & ~_STRING_HAS_ERROR) | _STRING_VALIDATED;
        parent = json->parent;

        /* parser callback */
        if (ctx) {
            int type = json->_flags & JSON_TYPE;

            if (likely(parent))
                ctx->parent = parent->_flags & JSON_TYPE;
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
            pos += json->_data - parent->_data;
            if (ctx && callback == 0 && (state & _KEY) == 0)
                string_free_token(json);
            json = parent;
        } else break;
    }

    return 0;

_error:
    debug("string_parse_json(): illegal character \'%c\' at %i.\n",
          json->_data[pos], (int) (json->_data - s->_data) + pos + 1);
    string_free_token(s);
    return -1;

_nomem:
    s->_flags |= _STRING_BUFFERING;
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
#endif
/* -------------------------------------------------------------------------- */

static void CALLBACK __free_token(m_string *string)
{
    unsigned int i = 0;

    /* recursively clean the tokens' tokens, if any */
    for (i = 0; i < PARTS(string); i ++) {
        if (likely(string->token[i].token))
            __free_token(string->token + i);
    }

    free(string->token);
}

public void string_free_token(m_string *string)
{
    if (string && string->token) {
        __free_token(string);
        string->token = NULL;
        string->_parts_alloc = string->parts = 0;
    }
}

/* -------------------------------------------------------------------------- */

public void string_api_cleanup(void)
{
    return;
}

/* -------------------------------------------------------------------------- */
