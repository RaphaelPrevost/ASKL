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

#include "codecs.h"

/* -------------------------------------------------------------------------- */
/* Base 58 */
/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */

ASKL_API String *string_b58s(const char *s, size_t size)
{
    /** @brief convert a C string to base58 encoding */

    size_t i = 0, zcount = 0;
    int j = 0, high = 0, carry = 0;
    int32_t b58size = 0;
    uint8_t *buffer = NULL;
    String *ret = NULL;

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

    if (zcount) memset(ret->data, '1', zcount);

    for (i = zcount; j < b58size; i ++, j ++)
        ret->data[i] = _b58[buffer[j]];

    ret->data[i] = '\0'; ret->len = i;

_err_alloc:
    free(buffer);

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_b58(const String *s)
{
    /** @brief convert a string to base58 encoding */

    return string_b58s(s->data, s->len);
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_deb58s(const char *s, size_t size)
{
    /** @brief convert a base58 encoded C string to plain text */

    uint32_t *r = NULL;
    size_t i = 0, zcount = 0, remain = 0;
    int j = 0, outlen = 0;
    uint64_t b = 0;
    uint32_t c = 0, mask = 0;
    String *ret = NULL;

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

    c = r[(i = 0)]; ret->data = (char *) r;

    switch (remain) {
    case 3: { ret->data[i ++] = (c & 0xff0000) >> 16; }
    case 2: { ret->data[i ++] = (c & 0xff00) >> 8; }
    case 1: { ret->data[i ++] = (c & 0xff); j = 1; goto _loop; }
    }

    for (j = 0; j < outlen; j ++) {
_loop:  c = r[j];
        ret->data[i ++] = (c >> 0x18) & 0xff;
        ret->data[i ++] = (c >> 0x10) & 0xff;
        ret->data[i ++] = (c >> 0x08) & 0xff;
        ret->data[i ++] = (c & 0xff);
    }

    /* check if there is spurious remaining 0s */
    for (remain = 0; remain < i && ! ret->data[remain]; remain ++);
    if (zcount > remain) goto _panic;

    /* real length */
    ret->len = i - (remain - zcount);

    /* remove spurious leading 0s */
    memmove(ret->data, ret->data + remain - zcount, ret->len);
    ret->data[ret->len] = '\0';

    /* package the buffer */
    ret->internal.flags = 0; ret->internal.capacity = outlen * sizeof(*r);
    ret->internal.tokens_capacity = ret->count = 0; ret->tokens = NULL;

    return ret;

_panic:
    free(r); string_free(ret);
    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_deb58(const String *s)
{
    /** @brief convert a base58 encoded string to plain text */

    return string_deb58s(s->data, s->len);
}

/* -------------------------------------------------------------------------- */
/* Base 64 */
/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */

ASKL_API String *string_b64s(const char *s, size_t size, size_t linesize)
{
    /** @brief convert a C string to base64 encoding */

    size_t i = 0;
    int j = 0, crlf = 0;
    int32_t b64size = 0;
    char *r = NULL;
    String *ret = NULL;

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
        ret->internal.flags = 0; ret->data = r;
        ret->len = b64size - 1; ret->internal.capacity = b64size -1;
        ret->internal.tokens_capacity = ret->count = 0;
        ret->tokens = NULL;
    }

    return ret;

_panic:
    debug("string_b64s(): base64 encoding failed.\n");
    free(r);
    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_b64(const String *s, size_t linesize)
{
    /** @brief convert a string to base64 encoding */

    return string_b64s(s->data, s->len, linesize);
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_deb64s(const char *s, size_t size)
{
    /** @brief convert a base64 encoded C string to plain text */

    char *r = NULL;
    size_t i = 0, j = 0;
    String *ret = NULL;
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
    ret->internal.flags = 0; ret->data = r;
    ret->len = j; ret->internal.capacity = i;
    ret->internal.tokens_capacity = ret->count = 0;
    ret->tokens = NULL;

    return ret;

_panic:
    ret = string_free(ret);
    free(r);
    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_deb64(const String *s)
{
    /** @brief convert a base64 encoded string to plain text */

    return string_deb64s(s->data, s->len);
}

/* -------------------------------------------------------------------------- */

static const char _hex[] = "0123456789abcdef";

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

ASKL_API char *string_rawurlencode(const char *url, size_t len, int flags)
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

ASKL_API int string_urlencode(String *url, int flags)
{
    char *encoded = NULL;
    size_t len = 0;

    if (! url || ! url->data) return -1;

    if (! (encoded = string_rawurlencode(url->data, url->len, flags)) )
        return -1;

    len = strlen(encoded);

    /* replace the original data */
    if (string_extend(url, len) == -1) {
        debug("string_urlencode(): cannot resize the string.\n");
        free(encoded);
        return -1;
    }

    memcpy(url->data, encoded, len);

    free(encoded);

    return 0;
}

/* -------------------------------------------------------------------------- */
#ifdef HAS_ZLIB
/* -------------------------------------------------------------------------- */

ASKL_API String *string_deflate(String *s)
{
    String *z = NULL;
    Bytef *dest = NULL;
    uLongf dlen = 0;

    if (! s || ! s->data || ! s->len) return NULL;

    if (! (z = string_alloc(NULL, (s->len * 11)/10 + 12)) ) {
        debug("string_deflate(): cannot allocate string.\n");
        return NULL;
    }

    dest = (Bytef *) z->data; dlen = z->len;

    if (compress2(dest, & dlen, (Bytef *) s->data, (uLong) s->len, 9) != 0) {
        debug("string_deflate(): gzip compression failed.\n");
        return string_free(z);
    }

    z->len = dlen;

    return z;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_inflate(String *s, size_t original_size)
{
    String *z = NULL;
    Bytef *dest = NULL;
    uLongf dlen = 0;

    if (! s || ! s->data || ! s->len) return NULL;

    if (! (z = string_alloc(NULL, original_size)) ) {
        debug("string_inflate(): cannot allocate string.\n");
        return NULL;
    }

    dest = (Bytef *) z->data; dlen = z->len;

    switch (uncompress(dest, & dlen, (Bytef *) s->data, (uLong) s->len)) {
    case Z_OK: z->len = dlen; return z;
    case Z_MEM_ERROR: debug("string_inflate(): out of memory.\n"); break;
    case Z_DATA_ERROR: debug("string_inflate(): data corruption.\n"); break;
    case Z_BUF_ERROR: debug("string_inflate(): not enough memory.\n"); break;
    default: debug("string_inflate(): error uncompressing data.\n");
    }

    return string_free(z);
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
