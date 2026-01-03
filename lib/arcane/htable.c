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
#define HASH_RATIO       1.2    /* threshold to grow the table (80%) */

#define RESIZE_HMAP -1
#define CREATE_ONLY  0
#define STORE_VALUE  1
#define MODIFY_ONLY  2

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
