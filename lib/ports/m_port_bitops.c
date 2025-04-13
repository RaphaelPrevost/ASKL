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

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#include <intrin.h>
#endif

#define max32(a, b) \
((a) - (((a) - (b)) & -((int32_t)((uint32_t)((a) - (b)) >> 31))))

/* fast macros to test if at least one byte in a word is < n, or > n, or = 0 */
#define __zero(x)    (((x) - 0x01010101U) & ~(x) & 0x80808080U)
#define __less(x, n) (((x) - ~0U / 255 * (n)) & ~(x) & ~0U / 255 * 128)
#define __more(x, n) ((((x) + ~0U / 255 * (127 - (n))) | (x)) & ~0U / 255 * 128)
#define __between(x, m, n) \
(((~0U / 255 * (127 + (n)) - ((x) & ~0U / 255 * 127)) & ~(x) & \
 (((x) & ~0U / 255 * 127) + ~0U / 255 * (127 - (m)))) & ~0U / 255 * 128)

/* -------------------------------------------------------------------------- */
/* Bitwise operations */
/* -------------------------------------------------------------------------- */

static inline unsigned int __ctz(uint32_t i)
{
    unsigned long c;

    if (likely(i)) {
        /* hardware implementation */
        #if (defined(__GNUC__) && \
             ((__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))) && \
            (defined(__i386__) || defined(__x86_64__)) || \
            (defined(__arm__) || defined(__aarch64__))
        
        c = __builtin_ctz(i);

        #elif (defined(_MSC_VER) && (_MSC_VER >= 1400)) && \
              (defined(_M_IX86) || defined(_M_AMD64) || defined(_M_ARM))

        #pragma intrinsic(_BitScanForward)

        _BitScanForward(& c, (unsigned long) i);

        #else
        /* portable software implementation */
        i &= -i;
        c = 0;
        if (i & 0xaaaaaaaaU) c |= 1;
        if (i & 0xccccccccU) c |= 2;
        if (i & 0xf0f0f0f0U) c |= 4;
        if (i & 0xff00ff00U) c |= 8;
        if (i & 0xffff0000U) c |= 16;
        
        #endif
    } else c = 32;

    return c;
}

/* -------------------------------------------------------------------------- */

static inline unsigned int __ctzll(uint64_t i)
{
    if (likely(i)) {
        /* hardware implementation */
        #if (defined(__GNUC__) && \
             ((__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))) && \
            (defined(__i386__) || defined(__x86_64__)) || \
            (defined(__arm__) || defined(__aarch64__))

        return __builtin_ctzll(i);

        #elif (defined(_MSC_VER) && (_MSC_VER >= 1400)) && \
              (defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM))

        #pragma intrinsic(_BitScanForward64)

        unsigned long ret;
        _BitScanForward64(& ret, (unsigned __int64) i);
        return ret;

        #else
        /* portable software implementation (de Bruijn sequence) */
        static const uint8_t seq[64] = {
             0,  1,  2,  7,  3, 13,  8, 19,  4, 25, 14, 28,  9, 34, 20, 40,
             5, 17, 26, 38, 15, 46, 29, 48, 10, 31, 35, 54, 21, 50, 41, 57,
            63,  6, 12, 18, 24, 27, 33, 39, 16, 37, 45, 47, 30, 53, 49, 56,
            62, 11, 23, 32, 36, 44, 52, 55, 61, 22, 43, 51, 60, 42, 59, 58
        };


        return seq[((i & -i) * 0x0218a392cd3d5dbfULL) >> 58];

        #endif
    } else return 64;
}

/* -------------------------------------------------------------------------- */

static inline unsigned int __clz(uint32_t i)
{
    if (likely(i)) {
        /* hardware implementation */
        #if (defined(__GNUC__) && \
             ((__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))) && \
            (defined(__i386__) || defined(__x86_64__)) || \
            (defined(__arm__) || defined(__aarch64__))
        
        return __builtin_clz(i);
        
        #elif (defined(_MSC_VER) && (_MSC_VER >= 1400)) && \
              (defined(_M_IX86) || defined(_M_AMD64) || defined(_M_ARM))

        #pragma intrinsic(_BitScanReverse)

        unsigned long ret;

        _BitScanReverse(& ret, (unsigned long) i);

        return 31 - ret;
        
        #else
        /* portable software implementation (de Bruijn sequence) */
        static const char seq[32] = {
            0, 31, 9, 30, 3,  8, 13, 29,  2,  5,  7, 21, 12, 24, 28, 19,
            1, 10, 4, 14, 6, 22, 25, 20, 11, 15, 23, 26, 16, 27, 17, 18
        };
        
        i |= i >> 1;
        i |= i >> 2;
        i |= i >> 4;
        i |= i >> 8;
        i |= i >> 16;
        i ++;
        
        return seq[i * 0x076be629 >> 27];
        
        #endif
    } else return 32;
}

/* -------------------------------------------------------------------------- */

static inline unsigned int __clzll(uint64_t i)
{
    if (likely(i)) {
        /* hardware implementation */
        #if (defined(__GNUC__) && \
             ((__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))) && \
            (defined(__i386__) || defined(__x86_64__)) || \
            (defined(__arm__) || defined(__aarch64__))
        
        return __builtin_clzll(i);
        
        #elif (defined(_MSC_VER) && (_MSC_VER >= 1400)) && \
              (defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM))

        #pragma intrinsic(_BitScanReverse64)

        unsigned long ret;

        _BitScanReverse64(& ret, (unsigned __int64) i);

        return 63 - ret;

        #else
        /* portable software implementation (de Bruijn sequence) */
        static const char seq[64] = {
             0, 47,  1, 56, 48, 27,  2, 60, 57, 49, 41, 37, 28, 16,  3, 61,
            54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11,  4, 62,
            46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45,
            25, 39, 14, 33, 19, 30,  9, 24, 13, 18,  8, 12,  7,  6,  5, 63
        };

        i |= i >> 1; 
        i |= i >> 2;
        i |= i >> 4;
        i |= i >> 8;
        i |= i >> 16;
        i |= i >> 32;

        return 63 - seq[(i * 0x03f79d71b4cb0a89ULL) >> 58];

    #endif
    } else return 64;
}

/* -------------------------------------------------------------------------- */

static inline uint32_t __msb(uint32_t i)
{
    /* hardware implementation */
    #if (defined(__GNUC__) && \
         ((__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))) && \
        (defined(__i386__) || defined(__x86_64__)) || \
        (defined(__arm__) || defined(__aarch64__))

    return 1 << (__builtin_clz(i) ^ 31);

    #elif (defined(_MSC_VER) && (_MSC_VER >= 1400)) && \
          (defined(_M_IX86) || defined(_M_AMD64) || defined(_M_ARM))

    #pragma intrinsic(_BitScanReverse)

    unsigned long idx;

    _BitScanReverse(& idx, (unsigned long) i);

    return 1 << (idx ^ 31);

    #else
    /* portable software implementation (de Bruijn sequence) */
    static const uint8_t seq[] = { 0, 5, 1, 6, 4, 3, 2, 7 };

    i |= i >> 1; i |= i >> 2; i |= i >> 4;

    return 1 << seq[(uint8_t) (i * 0x1D) >> 5];

    #endif
}

/* -------------------------------------------------------------------------- */

static inline uint32_t __bswap32(uint32_t i)
{
    #if (defined(__GNUC__))

    return __builtin_bswap32(i);

    #elif (defined(_MSC_VER) && (_MSC_VER >= 1400))

    #pragma intrinsic(_byteswap_ulong)

    return _byteswap_ulong(i);

    #else

    /* portable software implementation */
    return (
        ((i >> 24) & 0x000000ff) |
        ((i >> 8)  & 0x0000ff00) |
        ((i << 8)  & 0x00ff0000) |
        ((i << 24) & 0xff000000)
    );

    #endif
}

/* -------------------------------------------------------------------------- */

static inline uint64_t __bswap64(uint64_t i)
{
    #if (defined(__GNUC__))

    return __builtin_bswap64(i);

    #elif (defined(_MSC_VER) && (_MSC_VER >= 1400))

    #pragma intrinsic(_byteswap_uint64)

    return _byteswap_uint64(i);

    #else

    /* portable software implementation */
    return (
        (i << 56) |
        ((i & 0xff00) << 40) |
        ((i & 0xff0000) << 24) |
        ((i & 0xff000000) << 8) |
        ((i & 0xff00000000) >> 8) |
        ((i & 0xff0000000000) >> 24) |
        ((i & 0xff000000000000) >> 40) |
        ((i & 0xff00000000000000) >> 56)
    );

    #endif
}

/* -------------------------------------------------------------------------- */

static inline unsigned int __zero_idx(uint32_t i)
{
    #if defined(BIG_ENDIAN_HOST)
    i = _bswap32(i);
    #endif
    return __ctz(i) >> 3;
}

/* -------------------------------------------------------------------------- */

static inline unsigned int __zero_idx64(uint64_t i)
{
    #if defined(BIG_ENDIAN_HOST)
    i = _bswap64(i);
    #endif
    return __ctzll(i) >> 3;
}

/* -------------------------------------------------------------------------- */

static inline unsigned int __msb_idx(uint32_t i)
{
    return 31 - __clz(i);
}

/* -------------------------------------------------------------------------- */

static inline unsigned int __msb_idx64(const uint64_t i)
{
    return 63 - __clzll(i);
}

/* -------------------------------------------------------------------------- */

static inline int __is_pow2_multiple(uint64_t value, uint32_t p)
{
    return (value & ((1ULL << p) - 1)) == 0;
}

/* -------------------------------------------------------------------------- */

static inline int __is_pow5_multiple(uint64_t value, const uint32_t p)
{
    /* returns true if value is divisible by 5^p */
    const uint64_t m_inv_5 = 14757395258967641293U;
    const uint64_t n_div_5 = 3689348814741910323U;
    uint32_t count = 0;

    while (1) {
        /* simulate a division by using the modular inverse of 5 */
        value *= m_inv_5;
        /* n_div_5 is the largest 64 bits multiple of 5 */
        if (value > n_div_5) break;
        count ++;
    }

    return (count >= p);
}

/* -------------------------------------------------------------------------- */
/* Other operations */
/* -------------------------------------------------------------------------- */

static inline uint64_t umul128(uint64_t a, uint64_t b, uint64_t *high)
{
    #if defined(__SIZEOF_INT128__)
    __uint128_t result = (__uint128_t) a * (__uint128_t) b;
    *high = (uint64_t) (result >> 64);
    return (uint64_t) result;
    #elif (defined(_MSC_VER) && (_MSC_VER >= 1400)) && \
          (defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM))
    return __umul128(a, b, high);
    #else
    const uint32_t a_lo = (uint32_t) a;
    const uint32_t a_hi = (uint32_t) (a >> 32);
    const uint32_t b_lo = (uint32_t) b;
    const uint32_t b_hi = (uint32_t) (b >> 32);

    const uint64_t b00 = (uint64_t) a_lo * b_lo;
    const uint64_t b01 = (uint64_t) a_lo * b_hi;
    const uint64_t b10 = (uint64_t) a_hi * b_lo;
    const uint64_t b11 = (uint64_t) a_hi * b_hi;

    const uint32_t b00_lo = (uint32_t) b00;
    const uint32_t b00_hi = (uint32_t) (b00 >> 32);

    const uint64_t mid1 = b10 + b00_hi;
    const uint32_t mid1_lo = (uint32_t) (mid1);
    const uint32_t mid1_hi = (uint32_t) (mid1 >> 32);

    const uint64_t mid2 = b01 + mid1_lo;
    const uint32_t mid2_lo = (uint32_t) (mid2);
    const uint32_t mid2_hi = (uint32_t) (mid2 >> 32);

    const uint64_t r_hi = b11 + mid1_hi + mid2_hi;
    const uint64_t r_lo = ((uint64_t) mid2_lo << 32) | b00_lo;

    *high = r_hi;
    return r_lo;
    #endif
}

/* -------------------------------------------------------------------------- */

#define shr128(lo, hi, shift) (((hi) << (64 - (shift))) | ((lo) >> (shift)))

static inline uint64_t mul_shift64(uint64_t n, const uint64_t *mul, int32_t i)
{
    uint64_t high1;                                    // 128
    const uint64_t low1 = umul128(n, mul[1], & high1); // 64
    uint64_t high0;                                    // 64
    umul128(n, mul[0], & high0);                       // 0
    const uint64_t sum = high0 + low1;
    if (sum < high0) high1 ++; /* carry over into high1 */
    return shr128(sum, high1, i - 64);
}

/* -------------------------------------------------------------------------- */
