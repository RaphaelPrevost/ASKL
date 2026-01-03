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

#include "askl_rwlock.h"
#include "arcane/bitops.c"

struct _ASKL_RWLock {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    #ifndef HAS_ATOMICS
    pthread_mutex_t claim_mutex;
    pthread_cond_t claim_cond;
    #endif
    _ATOMIC int state;
    _ATOMIC int claim;
};

/**
 * @ingroup rwlock
 * @struct ASKL_RWLock
 *
 * Internal read/write lock.
 *
 * The lock combines a small atomic state variable with mutexes and a condition
 * variables. It provides:
 *
 * - A fast uncontended path via atomic operations on @ref state.
 * - Blocking/wakeup for contended paths via @ref mutex and @ref cond.
 * - Support for upgrading a read lock to a write lock via @ref lock_upgrade()
 *   with a single "claimant" at a time and cooperative help from other readers.
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
 * The @ref claim field coordinates upgrades:
 * - @c 0 : no thread is currently attempting to upgrade.
 * - @c 1 : a single thread has successfully claimed the lock and is waiting to
 *          become the only reader so it can transition the lock to write mode.
 *
 * The @ref mutex and @ref cond fields are used only on the slow path:
 * - Readers and writers first attempt to acquire the lock by changing
 *   @ref state atomically.
 * - If that fails, they sleep on @ref cond while holding @ref mutex.
 * - Unlock operations and lock_break() broadcast on @ref cond to wake
 *   sleeping waiters.
 *
 * In non-atomic builds, @ref claim_mutex and @ref claim_cond are used to
 * wait for the current upgrader to finish while temporarily releasing the
 * main state mutex.
 */

#if (defined(HAS_ATOMICS) && (defined(__STDC_VERSION__) && \
     (__STDC_VERSION__ >= 201112L)))
#define _LOCKSTATE_GET(lk) \
    atomic_load_explicit(& (lk)->state, memory_order_relaxed)
#define _LOCKSTATE_INC(lk) \
    atomic_fetch_add_explicit(& (lk)->state, 1, memory_order_relaxed)
#define _LOCKSTATE_DEC(lk) \
    atomic_fetch_sub_explicit(& (lk)->state, 1, memory_order_release)
#define _LOCKCLAIM_GET(lk) \
    atomic_load_explicit(& (lk)->claim, memory_order_relaxed)
#else
#define _LOCKSTATE_GET(lk) ((lk)->state)
#define _LOCKSTATE_INC(lk) do { (lk)->state ++; } while (0)
#define _LOCKSTATE_DEC(lk) do { (lk)->state --; } while (0)
#define _LOCKCLAIM_GET(lk) ((lk)->claim)
#endif

#ifdef HAS_ATOMICS
#define _LOCKSTATE_SET(lk, v) _atomic_str(& (lk)->state, (v))
#define _LOCKCLAIM_SET(lk, v) _atomic_str(& (lk)->claim, (v))
#else
#define _LOCKSTATE_SET(lk, v) do { (lk)->state = (v); } while (0)
#define _LOCKCLAIM_SET(lk, v) do { (lk)->claim = (v); } while (0)
#endif

/* -------------------------------------------------------------------------- */

private ASKL_RWLock *lock_alloc(void)
{
    ASKL_RWLock *ret = malloc(sizeof(*ret));
    if (! ret) {
        perror(ERR(lock_alloc, malloc));
        return NULL;
    }
    return ret;
}

/* -------------------------------------------------------------------------- */

private int lock_init(ASKL_RWLock *lock)
{
    if (pthread_mutex_init(& lock->mutex, NULL) == -1) {
        perror(ERR(lock_init, pthread_mutex_init));
        return -1;
    }

    if (pthread_cond_init(& lock->cond, NULL) == -1) {
        perror(ERR(lock_init, pthread_cond_init));
        goto _err_cond;
    }

    #ifndef HAS_ATOMICS
    if (pthread_mutex_init(& lock->claim_mutex, NULL) == -1) {
        perror(ERR(lock_init, pthread_mutex_init));
        goto _err_clmx;
    }

    if (pthread_cond_init(& lock->claim_cond, NULL) == -1) {
        perror(ERR(lock_init, pthread_cond_init));
        goto _err_clcd;
    }
    #endif

    lock->state = 1; /* unlocked */
    lock->claim = 0;

    return 0;

#ifndef HAS_ATOMICS
_err_clcd:
    pthread_mutex_destroy(& lock->claim_mutex);
_err_clmx:
    pthread_cond_destroy(& lock->cond);
#endif
_err_cond:
    pthread_mutex_destroy(& lock->mutex);
    return -1;
}

/* -------------------------------------------------------------------------- */

private int CALLBACK lock_rdlock(ASKL_RWLock *lock)
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

static int _lock(ASKL_RWLock *lock, int state)
{
    int ret = 0, x = 0;

    pthread_mutex_lock(& lock->mutex);

        #ifdef HAS_ATOMICS
        while (1) {
        #endif
            /* wait for unlock (lockstate == 1) */
            while (! (x = _LOCKSTATE_GET(lock)) || x > state)
                pthread_cond_wait(& lock->cond, & lock->mutex);

            #ifdef HAS_ATOMICS
            /* XXX handle slippery readers */
            if (_atomic_cas(& lock->state, state, 0)) break;
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

private inline int lock_wrlock(ASKL_RWLock *lock)
{
    #ifdef HAS_ATOMICS
    if (unlikely(_atomic_cas(& lock->state, 1, 0))) return 0;
    #endif
    return _lock(lock, 1);
}

/* -------------------------------------------------------------------------- */

static int _cooperate(ASKL_RWLock *lock)
{
    int cooperative = 0;
    int state = _LOCKSTATE_GET(lock);

    if (state > 2) {
        /* there is multiple readers, help the claimant by releasing our lock */
        #ifdef HAS_ATOMICS
        if (! _atomic_cas(& lock->state, state, state - 1))
            return 0;
        #else
        _LOCKSTATE_DEC(lock);
        pthread_cond_broadcast(& lock->cond);
        #endif
        cooperative = 1;
    } else if (state == -1) return -1;

    #ifndef HAS_ATOMICS
    pthread_mutex_unlock(& lock->mutex);

    pthread_mutex_lock(& lock->claim_mutex);
    #endif

    /* wait for the claim to be relinquished */
    while (_LOCKCLAIM_GET(lock) != 0) {
        if (_LOCKSTATE_GET(lock) == -1)
            return -1;
        #ifdef HAS_ATOMICS
        usleep(1);
        #else
        pthread_cond_wait(& lock->claim_cond, & lock->claim_mutex);
        #endif
    }

    #ifndef HAS_ATOMICS
    pthread_mutex_unlock(& lock->claim_mutex);

    pthread_mutex_lock(& lock->mutex);
    #endif

    if (cooperative) {
        /* re-take our lock */
        while (1) {
            state = _LOCKSTATE_GET(lock);
            if (state > 0) {
                #ifdef HAS_ATOMICS
                if (_atomic_cas(& lock->state, state, state + 1))
                #else
                _LOCKSTATE_INC(lock);
                pthread_cond_broadcast(& lock->cond);
                #endif
                    return 0;
            } else if (state == -1) return -1;

            #ifdef HAS_ATOMICS
            usleep(1);
            #else
            pthread_cond_wait(& lock->cond, & lock->mutex);
            #endif
        }
    }
    
    return 0;
}

/* -------------------------------------------------------------------------- */

private inline int lock_upgrade(ASKL_RWLock *lock)
{
    #ifdef HAS_ATOMICS
    /* fast path: only reader */
    if (unlikely(_atomic_cas(& lock->state, 2, 0)))
        return 0;
    
    /* claim the lock */
    while (1) {
        if (_atomic_cas(& lock->claim, 0, 1)) {
            /* only I will remain */
            while (! _atomic_cas(& lock->state, 2, 0)) {
                if (_LOCKSTATE_GET(lock) == -1)
                    return -1;
                usleep(1);
            }

            return 0;
        }

        if (_cooperate(lock) == -1) return -1;
    }
    #else
    while (1) {
        int state;

        pthread_mutex_lock(& lock->mutex);

        if ( (state = _LOCKSTATE_GET(lock)) == -1) goto _failure;

        pthread_mutex_lock(& lock->claim_mutex);
        if (_LOCKCLAIM_GET(lock) == 0) {
            if (state == 2) {
                /* fast path: only reader */
                _LOCKSTATE_SET(lock, 0);
                pthread_mutex_unlock(& lock->claim_mutex);
                goto _success;
            }

            _LOCKCLAIM_SET(lock, 1);

            pthread_mutex_unlock(& lock->claim_mutex);

            /* wait for the other readers to go away */
            while ( (state = _LOCKSTATE_GET(lock)) != 2)
                pthread_cond_wait(& lock->cond, & lock->mutex);
            if (state == -1) goto _failure;

            /* take the lock */
            _LOCKSTATE_SET(lock, 0);
            goto _success;
        }
        pthread_mutex_unlock(& lock->claim_mutex);

        /* call cooperate while holding the state mutex */
        if (_cooperate(lock) == -1) goto _failure;

        pthread_mutex_unlock(& lock->mutex);
    }
_failure:
    pthread_mutex_unlock(& lock->mutex);
    return -1;
_success:
    pthread_mutex_unlock(& lock->mutex);
    return 0;
    #endif
}

/* -------------------------------------------------------------------------- */

private void CALLBACK lock_restore(ASKL_RWLock *lock)
{
    #ifndef HAS_ATOMICS
    pthread_mutex_lock(& lock->mutex);
    pthread_mutex_lock(& lock->claim_mutex);
    #endif

        _LOCKSTATE_SET(lock, 2);
        _LOCKCLAIM_SET(lock, 0);

    #ifndef HAS_ATOMICS
    pthread_mutex_unlock(& lock->claim_mutex);
    pthread_mutex_unlock(& lock->mutex);
    #endif

    pthread_cond_broadcast(& lock->cond);
    #ifndef HAS_ATOMICS
    pthread_cond_broadcast(& lock->claim_cond);
    #endif
}

/* -------------------------------------------------------------------------- */

private void CALLBACK lock_break(ASKL_RWLock *lock)
{
    pthread_mutex_lock(& lock->mutex);

        _LOCKSTATE_SET(lock, -1);

    pthread_mutex_unlock(& lock->mutex);

    pthread_cond_broadcast(& lock->cond);
}

/* -------------------------------------------------------------------------- */

static void _unlock(ASKL_RWLock *lock)
{
    int x;

    pthread_mutex_lock(& lock->mutex);

        if ( (x = _LOCKSTATE_GET(lock)) == 0)
            _LOCKSTATE_SET(lock, 1);
        else if (x > 1)
            _LOCKSTATE_DEC(lock);
        pthread_cond_broadcast(& lock->cond);

    pthread_mutex_unlock(& lock->mutex);
}

/* -------------------------------------------------------------------------- */

private inline void lock_unlock(ASKL_RWLock *lock)
{
    #ifdef HAS_ATOMICS
    int x;
    for (x = _atomic_ldr(& lock->state); x > 1; x = _atomic_ldr(& lock->state))
        if (_atomic_cas(& lock->state, x, x - 1)) return;
    if (likely(x != -1))
    #endif
        _unlock(lock);
}

/* -------------------------------------------------------------------------- */

private void lock_destroy(ASKL_RWLock *lock)
{
    pthread_cond_destroy(& lock->cond);
    pthread_mutex_destroy(& lock->mutex);
    #ifndef HAS_ATOMICS
    pthread_cond_destroy(& lock->claim_cond);
    pthread_mutex_destroy(& lock->claim_mutex);
    #endif
}

/* -------------------------------------------------------------------------- */

private ASKL_RWLock *lock_free(ASKL_RWLock *lock)
{
    free(lock);
    return NULL;
}

/* -------------------------------------------------------------------------- */
