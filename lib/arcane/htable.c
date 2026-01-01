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

struct _rwlock {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    _ATOMIC int state;
};

/**
 * @ingroup hashtable
 * @struct _rwlock
 *
 * Internal read/write lock used by the linked hashmap implementation.
 *
 * The lock combines a small atomic state variable with a mutex and a condition
 * variable. It provides:
 *
 * - A fast uncontended path via atomic operations on @ref state.
 * - Blocking/wakeup for contended paths via @ref mutex and @ref cond.
 * - The ability to "break" the lock when destroying a map so that threads
 *   blocked in map operations wake up and fail cleanly instead of deadlocking
 *   on freed memory.
 *
 * The @ref state field encodes ownership as follows:
 * - @c -1 : broken; further calls to _map_rdlock() / _map_wrlock() must fail
 *           and return -1.
 * - @c  0 : write-locked; exactly one writer holds the lock.
 * - @c  1 : unlocked; no active readers or writer.
 * - @c >1 : read-locked; (@c state - 1) readers hold the lock.
 *
 * The @ref mutex and @ref cond fields are used only on the slow path:
 * - Readers and writers first attempt to acquire the lock by changing
 *   @ref state atomically.
 * - If that fails, they sleep on @ref cond while holding @ref mutex.
 * - Unlock operations and _map_break_lock() broadcast on @ref cond to wake
 *   sleeping waiters.
 *
 * This type is strictly internal to the hashmap code and must not be used
 * outside this module.
 */

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
/* Read/Write lock */
/* -------------------------------------------------------------------------- */

#if (defined(HAS_ATOMICS) && (defined(__STDC_VERSION__) && \
     (__STDC_VERSION__ >= 201112L)))
#define _LOCKSTATE_GET(lk) \
    atomic_load_explicit(& (lk)->state, memory_order_relaxed)
#define _LOCKSTATE_INC(lk) \
    atomic_fetch_add_explicit(& (lk)->state, 1, memory_order_relaxed)
#define _LOCKSTATE_DEC(lk) \
    atomic_fetch_sub_explicit(& (lk)->state, 1, memory_order_release)
#else
#define _LOCKSTATE_GET(lk) ((lk)->state)
#define _LOCKSTATE_INC(lk) do { (lk)->state ++; } while (0)
#define _LOCKSTATE_DEC(lk) do { (lk)->state --; } while (0)
#endif

#ifdef HAS_ATOMICS
#define _LOCKSTATE_SET(lk, v) _atomic_str(& (lk)->state, (v))
#else
#define _LOCKSTATE_SET(lk, v) do { (lk)->state = (v); } while (0)
#endif

static int _map_rdlock(struct _rwlock *lock)
{
    int ret = 0, x = 0;

    #ifdef HAS_ATOMICS
    for (x = _atomic_ldr(& lock->state); x > 0; x = _atomic_ldr(& lock->state))
        if (_atomic_cas(& lock->state, x, x + 1)) return 0;
    if (unlikely(x == -1)) return -1;
    #endif

    pthread_mutex_lock(& lock->mutex);

        /* wait while write-locked (lockstate == 0) */
        while (! (x = _LOCKSTATE_GET(lock)) )
            pthread_cond_wait(& lock->cond, & lock->mutex);

        if (likely(x > 0))
            _LOCKSTATE_INC(lock);
        else ret = -1;

    pthread_mutex_unlock(& lock->mutex);

    return ret;
}

/* -------------------------------------------------------------------------- */

static int _map_wrlock(struct _rwlock *lock)
{
    int ret = 0, x = 0;

    #ifdef HAS_ATOMICS
    if (_atomic_cas(& lock->state, 1, 0)) return 0;
    #endif

    pthread_mutex_lock(& lock->mutex);

        #ifdef HAS_ATOMICS
        while (1) {
        #endif
            /* wait for unlock (lockstate == 1) */
            while (! (x = _LOCKSTATE_GET(lock)) || x > 1)
                pthread_cond_wait(& lock->cond, & lock->mutex);

            #ifdef HAS_ATOMICS
            /* XXX handle slippery readers */
            if (_atomic_cas(& lock->state, 1, 0)) break;
            #endif

            if (unlikely(x == -1)) {
                ret = -1; goto _err_lock;
            }
        #ifdef HAS_ATOMICS
        }
        #else
        lock->state = 0;
        #endif

_err_lock:
    pthread_mutex_unlock(& lock->mutex);

    return ret;
}

/* -------------------------------------------------------------------------- */

static void _unlock(struct _rwlock *lock)
{
    int x = 0;

    pthread_mutex_lock(& lock->mutex);

        if ( (x = _LOCKSTATE_GET(lock)) == 0)
            _LOCKSTATE_SET(lock, 1);
        else if (x > 1)
            _LOCKSTATE_DEC(lock);

    pthread_mutex_unlock(& lock->mutex);

    pthread_cond_broadcast(& lock->cond);
}

static inline void _map_unlock(struct _rwlock *lock)
{
    #ifdef HAS_ATOMICS
    int x = 0;
    for (x = _atomic_ldr(& lock->state); x > 1; x = _atomic_ldr(& lock->state))
        if (_atomic_cas(& lock->state, x, x - 1)) return;
    if (likely(x != -1))
    #endif
        _unlock(lock);
}

/* -------------------------------------------------------------------------- */

static void _map_break_lock(struct _rwlock *lock)
{
    pthread_mutex_lock(& lock->mutex);

        _LOCKSTATE_SET(lock, -1);

    pthread_mutex_unlock(& lock->mutex);

    pthread_cond_broadcast(& lock->cond);
}

/* -------------------------------------------------------------------------- */

#endif
