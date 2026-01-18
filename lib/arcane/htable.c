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

#ifdef ASKL_HASHTABLE_H

/* -------------------------------------------------------------------------- */
/* Hashtable internals */
/* -------------------------------------------------------------------------- */

#define HASH_COUNT         8    /* number of hash functions */
#define HASH_RETRY         4    /* number of retries if the bucket is full */
#define HASH_RATIO      1.23    /* threshold to grow the table (81.3%) */

/*
 * HASH_RATIO: Resize threshold (inverse of load factor)
 * Recommended values:
 *   1.23 (81.3% load) - Optimal speed/memory balance (32/64 bits) [default]
 *   1.10 (90.9% load) - 2% slower (64 bits only)
 *   1.03 (97.1% load) - Maximum density, 18% slower (64 bits only)
 */

#define RESIZE_HMAP -1
#define CREATE_ONLY  0
#define STORE_VALUE  1
#define MODIFY_ONLY  2

#if (UINTPTR_MAX == 0xffffffffffffffffULL)
    #define TAG_SHIFT 61
    #define TAG_MASK 0x7ULL
#else
    #define TAG_SHIFT 30
    #define TAG_MASK 0x3UL
#endif

/* Extract tag from hash (high bits) */
#define HASHTAG(hash) ((((uintptr_t) (hash)) >> TAG_SHIFT) & TAG_MASK)

/* Tag pointer */
#define TAG_PTR(ptr, hash) ((_item *) (((uintptr_t) (ptr)) | HASHTAG(hash)))

/* Clean pointer */
#define GET_PTR(ptr) ((_item *) (((uintptr_t) (ptr)) & ~TAG_MASK))

/* Extract tag from pointer */
#define GET_TAG(ptr) (((uintptr_t) (ptr)) & TAG_MASK)

/* -------------------------------------------------------------------------- */
#if (UINTPTR_MAX == 0xffffffffffffffffULL) /* 64 bits */
/* -------------------------------------------------------------------------- */
/* rapidhashNano (author: Nicolas De Carli) */
/* -------------------------------------------------------------------------- */

/* rapidhash secret constants */
static const uint64_t _rapid_secret[8] = {
    0x2d358dccaa6c78a5ULL, 0x8bb84b93962eacc9ULL,
    0x4b33a62ed433d4a3ULL, 0x4d5a2da51de1aa47ULL,
    0xa0761d6478bd642fULL, 0xe7037ed1a0b428dbULL,
    0x90ed1765281c388cULL, 0xaaaaaaaaaaaaaaaaULL
};

/* -------------------------------------------------------------------------- */

static uint64_t _rapid_read64(const uint8_t *p)
{
    uint64_t result;
    memcpy(& result, p, sizeof(result));
    #if defined(BIG_ENDIAN_HOST)
    return __bswap64(result);
    #else
    return result;
    #endif
}

/* -------------------------------------------------------------------------- */

static uint64_t _rapid_read32(const uint8_t *p)
{
    uint32_t result;
    memcpy(& result, p, sizeof(result));
    #if defined(BIG_ENDIAN_HOST)
    return __bswap32(result);
    #else
    return result;
    #endif
}

/* -------------------------------------------------------------------------- */

static void _rapid_mum(uint64_t *a, uint64_t *b)
{
    uint64_t high;
    uint64_t low = umul128(*a, *b, & high);
    *a = low;
    *b = high;
}

/* -------------------------------------------------------------------------- */

static uint64_t _rapid_mix(uint64_t a, uint64_t b)
{
    _rapid_mum(& a, & b);
    return a ^ b;
}

/* -------------------------------------------------------------------------- */

static uint64_t _hash(const char *key, size_t len, uint64_t seed)
{
    const uint8_t *p = (const uint8_t *) key;
    uint64_t a = 0, b = 0;
    size_t i = len;

    seed ^= _rapid_mix(seed ^ _rapid_secret[2], _rapid_secret[1]);

    if (likely(len <= 16)) {
        if (len >= 4) {
            seed ^= len;
            if (len >= 8) {
                const uint8_t *plast = p + len - 8;
                a = _rapid_read64(p);
                b = _rapid_read64(plast);
            } else {
                const uint8_t *plast = p + len - 4;
                a = _rapid_read32(p);
                b = _rapid_read32(plast);
            }
        } else if (len > 0) {
            a = (((uint64_t) p[0]) << 45) | p[len - 1];
            b = p[len >> 1];
        } else {
            a = b = 0;
        }
    } else {
        if (i > 48) {
            uint64_t see1 = seed, see2 = seed;
            do {
                seed = _rapid_mix(
                    _rapid_read64(p) ^ _rapid_secret[0],
                    _rapid_read64(p + 8) ^ seed
                );
                see1 = _rapid_mix(
                    _rapid_read64(p + 16) ^ _rapid_secret[1],
                    _rapid_read64(p + 24) ^ see1
                );
                see2 = _rapid_mix(
                    _rapid_read64(p + 32) ^ _rapid_secret[2],
                    _rapid_read64(p + 40) ^ see2
                );
                p += 48;
                i -= 48;
            } while (i > 48);
            seed ^= see1;
            seed ^= see2;
        }
        if (i > 16) {
            seed = _rapid_mix(
                _rapid_read64(p) ^ _rapid_secret[2],
                _rapid_read64(p + 8) ^ seed
            );
            if (i > 32) {
                seed = _rapid_mix(
                    _rapid_read64(p + 16) ^ _rapid_secret[2],
                    _rapid_read64(p + 24) ^ seed
                );
            }
        }
        a = _rapid_read64(p + i - 16) ^ i;
        b = _rapid_read64(p + i - 8);
    }

    a ^= _rapid_secret[1];
    b ^= seed;
    _rapid_mum(& a, & b);

    return _rapid_mix(a ^ _rapid_secret[7], b ^ _rapid_secret[1] ^ i);
}

/* -------------------------------------------------------------------------- */
#elif (UINTPTR_MAX == 0xffffffffU) /* 32 bits */
/* -------------------------------------------------------------------------- */
/* wyhash32 (author: 王一 Wang Yi <godspeed_china@yeah.net>) */
/* -------------------------------------------------------------------------- */

static uint32_t _wyr32(const uint8_t *p)
{
    uint32_t result;
    memcpy(& result, p, sizeof(result));
    #if defined(BIG_ENDIAN_HOST)
    return __bswap32(result);
    #else
    return result;
    #endif
}

/* -------------------------------------------------------------------------- */

static uint32_t _wyr24(const uint8_t *p, uint32_t k)
{
    return (((uint32_t) p[0]) << 16) | (((uint32_t) p[k >> 1]) << 8) | p[k - 1];
}

/* -------------------------------------------------------------------------- */

static void _wymix32(uint32_t *a, uint32_t *b)
{
    uint64_t c = *a ^ 0x53c5ca59u;
    c *= *b ^ 0x74743c1bu;
    *a = (uint32_t) c;
    *b = (uint32_t) (c >> 32);
}

/* -------------------------------------------------------------------------- */

static uint32_t _hash(const char *key, uint16_t len, uint32_t seed)
{
    const uint8_t *p = (const uint8_t *) key;
    uint32_t i, see1 = len;

    _wymix32(& seed, & see1);

    for (i = len; i > 8; i -= 8, p += 8) {
        seed ^= _wyr32(p);
        see1 ^= _wyr32(p + 4);
        _wymix32(& seed, & see1);
    }

    if (i >= 4) {
        seed ^= _wyr32(p);
        see1 ^= _wyr32(p + i - 4);
    } else if (i) seed ^= _wyr24(p, i);

    _wymix32(& seed, & see1);
    _wymix32(& seed, & see1);

    return seed ^ see1;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

#endif
