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

#include "crypto.h"

typedef struct {
    uint32_t h0, h1, h2, h3, h4;
    uint32_t nblocks;
    unsigned char buf[64];
    int count;
} SHA1_CONTEXT;

static const char _hex[] = "0123456789abcdef";

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

ASKL_API String *string_sha1s(const char *string, size_t len)
{
    /** @brief returns the sha1 sum of the given string */

    SHA1_CONTEXT ctx;
    char *buf = NULL;
    unsigned int i = 0;
    String *ret = NULL;

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
        ret->internal.flags = 0; ret->data = buf;
        ret->len = 40; ret->internal.capacity = 41;
        ret->internal.tokens_capacity = ret->count = 0;
        ret->tokens = NULL;
    }

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API String *string_sha1(String *s)
{
    return string_sha1s(s->data, s->len);
}

/* -------------------------------------------------------------------------- */
/* -- end of SHA1 implementation */
/* -------------------------------------------------------------------------- */
