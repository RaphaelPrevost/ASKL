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
 *  data to be ensured and,  more generally, to use and operate in the         *
 *  same conditions as regards security.                                       *
 *                                                                             *
 *  The fact that you are presently reading this means that you have had       *
 *  knowledge of the CeCILL license and that you accept its terms.             *
 *                                                                             *
 ******************************************************************************/

#include "../askl_test.h"

/* the lock is internal to the library: test a private copy of it, which
   also exposes its state word */
#include "../../lib/askl_rwlock.c"

#define TIMEOUT_MS 5000
#define ROUNDS     200
#define UPGRADERS  4
#define STRESSERS  8
#define STRESS_MS  300

/* a flag or counter shared with a worker thread, under its own mutex */
typedef struct _Shared {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int value;
} _Shared;

typedef struct _Barrier {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    unsigned int count, waiting, generation;
} _Barrier;

typedef struct _Worker {
    RW_Lock *lock;
    _Shared *done;
    _Barrier *barrier;
    unsigned int id;
    int result;
    int order;
    unsigned int rounds;
} _Worker;

/* the critical-section monitor of the upgrade and stress cases */
static _Shared _monitor = {
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0
};
static int _readers = 0, _writers = 0, _violations = 0, _sequence = 0;

/* -------------------------------------------------------------------------- */

static void _shared_init(_Shared *s)
{
    pthread_mutex_init(& s->mutex, NULL);
    pthread_cond_init(& s->cond, NULL);
    s->value = 0;
}

/* -------------------------------------------------------------------------- */

static void _shared_add(_Shared *s, int n)
{
    pthread_mutex_lock(& s->mutex);
    s->value += n;
    pthread_cond_broadcast(& s->cond);
    pthread_mutex_unlock(& s->mutex);
}

/* -------------------------------------------------------------------------- */

static int _shared_get(_Shared *s)
{
    int value = 0;

    pthread_mutex_lock(& s->mutex);
    value = s->value;
    pthread_mutex_unlock(& s->mutex);

    return value;
}

/* -------------------------------------------------------------------------- */

static int _shared_wait(_Shared *s, int value, unsigned int ms)
{
    /** @brief wait until the shared value reaches @p value, or time out */

    unsigned int i = 0;

    for (i = 0; i < ms; i ++) {
        if (_shared_get(s) >= value) return 0;
        usleep(1000);
    }

    return -1;
}

/* -------------------------------------------------------------------------- */

static void _barrier_init(_Barrier *b, unsigned int count)
{
    pthread_mutex_init(& b->mutex, NULL);
    pthread_cond_init(& b->cond, NULL);
    b->count = count; b->waiting = 0; b->generation = 0;
}

/* -------------------------------------------------------------------------- */

static void _barrier_wait(_Barrier *b)
{
    unsigned int generation = 0;

    pthread_mutex_lock(& b->mutex);
    generation = b->generation;
    if (++ b->waiting == b->count) {
        b->waiting = 0; b->generation ++;
        pthread_cond_broadcast(& b->cond);
    } else {
        while (generation == b->generation)
            pthread_cond_wait(& b->cond, & b->mutex);
    }
    pthread_mutex_unlock(& b->mutex);
}

/* -------------------------------------------------------------------------- */

static void _monitor_enter(int writer)
{
    /** @brief account for a critical section and check exclusivity */

    pthread_mutex_lock(& _monitor.mutex);
    if (writer) {
        if (_writers || _readers) _violations ++;
        _writers ++;
    } else {
        if (_writers) _violations ++;
        _readers ++;
    }
    pthread_mutex_unlock(& _monitor.mutex);
}

/* -------------------------------------------------------------------------- */

static void _monitor_leave(int writer)
{
    pthread_mutex_lock(& _monitor.mutex);
    if (writer) _writers --; else _readers --;
    pthread_mutex_unlock(& _monitor.mutex);
}

/* -------------------------------------------------------------------------- */

static int _next_sequence(void)
{
    int n = 0;

    pthread_mutex_lock(& _monitor.mutex);
    n = ++ _sequence;
    pthread_mutex_unlock(& _monitor.mutex);

    return n;
}

/* -------------------------------------------------------------------------- */

static int _states(void)
{
    /* the state word encodes everything: check the encoding directly */

    RW_Lock *lock = NULL;

    ASSERT_NOT_NULL(lock = lock_alloc());
    ASSERT_EQ_INT(lock_init(lock), 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UNLOCKED);
    ASSERT_EQ_INT(_LOCKWFLAG_GET(lock), 0);

    /* readers stack up */
    ASSERT_EQ_INT(lock_rdlock(lock), 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), RDLOCKED);
    ASSERT_EQ_INT(lock_rdlock(lock), 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), RDLOCKED + LOCKSTEP);
    lock_unlock(lock);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), RDLOCKED);
    lock_unlock(lock);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UNLOCKED);

    /* a writer owns it */
    ASSERT_EQ_INT(lock_wrlock(lock), 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), WRLOCKED);
    lock_unlock(lock);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UNLOCKED);

    /* a lone reader upgrades on the fast path, and restores */
    ASSERT_EQ_INT(lock_rdlock(lock), 0);
    ASSERT_EQ_INT(lock_upgrade(lock), 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UPGRADED);
    lock_restore(lock);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), RDLOCKED);
    lock_unlock(lock);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UNLOCKED);

    /* unlocking an unlocked lock changes nothing */
    lock_unlock(lock);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UNLOCKED);
    ASSERT_EQ_INT(_LOCKWFLAG_GET(lock), 0);

    lock_destroy(lock);
    ASSERT_NULL(lock_free(lock));

    return 0;
}

/* -------------------------------------------------------------------------- */

static void *_reader(void *arg)
{
    _Worker *w = arg;

    w->result = lock_rdlock(w->lock);
    w->order = _next_sequence();
    if (w->result == 0) {
        _monitor_enter(0);
        usleep(1000);
        _monitor_leave(0);
        lock_unlock(w->lock);
    }
    _shared_add(w->done, 1);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static void *_writer(void *arg)
{
    _Worker *w = arg;

    w->result = lock_wrlock(w->lock);
    w->order = _next_sequence();
    if (w->result == 0) {
        _monitor_enter(1);
        usleep(1000);
        _monitor_leave(1);
        lock_unlock(w->lock);
    }
    _shared_add(w->done, 1);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static void *_holding_reader(void *arg)
{
    /** @brief take a read lock, hold it until told, release it */

    _Worker *w = arg;

    w->result = lock_rdlock(w->lock);
    _shared_add(w->done, 1);
    if (w->result == 0) {
        while (_shared_get(w->done) < 2) usleep(1000);
        lock_unlock(w->lock);
        _shared_add(w->done, 1);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _writer_and_readers_wait(void)
{
    /* a writer waits for the readers, readers wait for the writer, and
       the waiter flags say who is waiting */

    static _Worker w[3];
    static _Shared done;
    pthread_t threads[3];
    RW_Lock *lock = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(lock = lock_alloc());
    ASSERT_EQ_INT(lock_init(lock), 0);
    _shared_init(& done);
    _violations = 0; _readers = 0; _writers = 0; _sequence = 0;

    /* readers hold, a writer arrives */
    ASSERT_EQ_INT(lock_rdlock(lock), 0);
    _monitor_enter(0);
    memset(w, 0, sizeof(w));
    w[0].lock = lock; w[0].done = & done;
    ASSERT_EQ_INT(pthread_create(& threads[0], NULL, _writer, & w[0]), 0);
    usleep(50000);
    ASSERT_EQ_INT(_shared_get(& done), 0);
    ASSERT_EQ_INT(_LOCKWFLAG_GET(lock) & WRWAITER, WRWAITER);
    _monitor_leave(0);
    lock_unlock(lock);
    ASSERT_EQ_INT(_shared_wait(& done, 1, TIMEOUT_MS), 0);
    pthread_join(threads[0], NULL);
    ASSERT_EQ_INT(w[0].result, 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UNLOCKED);
    ASSERT_EQ_INT(_LOCKWFLAG_GET(lock), 0);

    /* a writer holds, readers arrive */
    ASSERT_EQ_INT(lock_wrlock(lock), 0);
    _monitor_enter(1);
    _shared_add(& done, -1);
    for (i = 1; i < 3; i ++) {
        w[i].lock = lock; w[i].done = & done;
        ASSERT_EQ_INT(pthread_create(& threads[i], NULL, _reader, & w[i]), 0);
    }
    usleep(50000);
    ASSERT_EQ_INT(_shared_get(& done), 0);
    ASSERT_EQ_INT(_LOCKWFLAG_GET(lock) & 0xfff, 0);
    ASSERT_TRUE(_LOCKWFLAG_GET(lock) & ~0xfff);
    _monitor_leave(1);
    lock_unlock(lock);
    ASSERT_EQ_INT(_shared_wait(& done, 2, TIMEOUT_MS), 0);
    for (i = 1; i < 3; i ++) pthread_join(threads[i], NULL);
    ASSERT_EQ_INT(w[1].result, 0);
    ASSERT_EQ_INT(w[2].result, 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UNLOCKED);
    ASSERT_EQ_INT(_LOCKWFLAG_GET(lock), 0);
    ASSERT_EQ_INT(_violations, 0);

    lock_destroy(lock);
    ASSERT_NULL(lock_free(lock));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _upgrade_waits_for_readers(void)
{
    /* an upgrader claims the lock, waits for the other reader to leave,
       and a reader arriving meanwhile is held back until the restore */

    static _Worker holder, late;
    static _Shared done, arrived;
    pthread_t threads[2];
    RW_Lock *lock = NULL;
    int mine = 0;

    ASSERT_NOT_NULL(lock = lock_alloc());
    ASSERT_EQ_INT(lock_init(lock), 0);
    _shared_init(& done); _shared_init(& arrived);
    _violations = 0; _readers = 0; _writers = 0; _sequence = 0;

    /* two readers */
    ASSERT_EQ_INT(lock_rdlock(lock), 0);
    memset(& holder, 0, sizeof(holder));
    holder.lock = lock; holder.done = & done;
    ASSERT_EQ_INT(pthread_create(& threads[0], NULL, _holding_reader,
                                 & holder), 0);
    ASSERT_EQ_INT(_shared_wait(& done, 1, TIMEOUT_MS), 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), RDLOCKED + LOCKSTEP);

    /* the holder leaves after a while, then the upgrade goes through */
    memset(& late, 0, sizeof(late));
    late.lock = lock; late.done = & arrived;
    _shared_add(& done, 1);
    ASSERT_EQ_INT(lock_upgrade(lock), 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UPGRADED);
    _monitor_enter(1);
    mine = _next_sequence();

    /* a reader arriving now must wait for the restore */
    ASSERT_EQ_INT(pthread_create(& threads[1], NULL, _reader, & late), 0);
    usleep(50000);
    ASSERT_EQ_INT(_shared_get(& arrived), 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UPGRADED);
    _monitor_leave(1);
    lock_restore(lock);
    ASSERT_EQ_INT(_shared_wait(& arrived, 1, TIMEOUT_MS), 0);
    pthread_join(threads[1], NULL);
    ASSERT_EQ_INT(late.result, 0);
    ASSERT_TRUE(late.order > mine);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), RDLOCKED);

    lock_unlock(lock);
    pthread_join(threads[0], NULL);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UNLOCKED);
    ASSERT_EQ_INT(_violations, 0);

    lock_destroy(lock);
    ASSERT_NULL(lock_free(lock));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _all_readers_in(void)
{
    int in = 0;

    pthread_mutex_lock(& _monitor.mutex);
    in = (_readers == UPGRADERS);
    pthread_mutex_unlock(& _monitor.mutex);

    return in;
}

/* -------------------------------------------------------------------------- */

static void *_upgrader(void *arg)
{
    /** @brief meet the others, read, give them a moment to read too,
        upgrade, restore, release, repeat; a reader that blocks while
        holding its share would starve the upgraders, so the wait for
        the others is bounded */

    _Worker *w = arg;
    unsigned int i = 0, spin = 0;

    for (i = 0; i < ROUNDS; i ++) {
        _barrier_wait(w->barrier);
        if (lock_rdlock(w->lock) == -1) break;
        _monitor_enter(0);
        for (spin = 0; spin < 200 && ! _all_readers_in(); spin ++) usleep(10);
        _monitor_leave(0);
        if (lock_upgrade(w->lock) == -1) break;
        _monitor_enter(1);
        w->rounds ++;
        _monitor_leave(1);
        lock_restore(w->lock);
        _monitor_enter(0);
        _monitor_leave(0);
        lock_unlock(w->lock);
    }
    w->result = (i == ROUNDS) ? 0 : -1;
    _shared_add(w->done, 1);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _concurrent_upgraders(void)
{
    /* several readers upgrade at once: one claims, the others step
       aside and come back, every one of them gets its turn */

    static _Worker w[UPGRADERS];
    static _Shared done;
    static _Barrier barrier;
    pthread_t threads[UPGRADERS];
    RW_Lock *lock = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(lock = lock_alloc());
    ASSERT_EQ_INT(lock_init(lock), 0);
    _shared_init(& done);
    _barrier_init(& barrier, UPGRADERS);
    _violations = 0; _readers = 0; _writers = 0;

    for (i = 0; i < UPGRADERS; i ++) {
        memset(& w[i], 0, sizeof(w[i]));
        w[i].lock = lock; w[i].done = & done; w[i].barrier = & barrier;
        w[i].id = i;
        ASSERT_EQ_INT(pthread_create(& threads[i], NULL, _upgrader, & w[i]),
                      0);
    }

    if (_shared_wait(& done, UPGRADERS, TIMEOUT_MS) == -1)
        FAIL("the upgraders did not all finish: deadlock");
    for (i = 0; i < UPGRADERS; i ++) {
        pthread_join(threads[i], NULL);
        ASSERT_EQ_INT(w[i].result, 0);
        ASSERT_EQ_UINT(w[i].rounds, ROUNDS);
    }

    ASSERT_EQ_INT(_violations, 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UNLOCKED);
    ASSERT_EQ_INT(_LOCKWFLAG_GET(lock), 0);

    lock_destroy(lock);
    ASSERT_NULL(lock_free(lock));

    return 0;
}

/* -------------------------------------------------------------------------- */

static void *_late_upgrader(void *arg)
{
    _Worker *w = arg;

    if ( (w->result = lock_rdlock(w->lock)) == 0) {
        _shared_add(w->done, 1);
        w->result = lock_upgrade(w->lock);
        if (w->result == 0) {
            lock_restore(w->lock);
            lock_unlock(w->lock);
        }
    }
    _shared_add(w->done, 1);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _break_with_waiters(void)
{
    /* breaking the lock releases every waiter with -1, including an
       upgrader spinning on its claim, and the lock stays broken */

    static _Worker w[4];
    static _Shared done;
    pthread_t threads[4];
    RW_Lock *lock = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(lock = lock_alloc());
    ASSERT_EQ_INT(lock_init(lock), 0);
    _shared_init(& done);
    _violations = 0; _readers = 0; _writers = 0; _sequence = 0;

    /* a writer holds; two readers and a writer queue up behind it */
    ASSERT_EQ_INT(lock_wrlock(lock), 0);
    memset(w, 0, sizeof(w));
    for (i = 0; i < 3; i ++) {
        w[i].lock = lock; w[i].done = & done;
        ASSERT_EQ_INT(pthread_create(
            & threads[i], NULL, (i < 2) ? _reader : _writer, & w[i]
        ), 0);
    }
    usleep(50000);
    ASSERT_EQ_INT(_shared_get(& done), 0);

    lock_break(lock);
    ASSERT_EQ_INT(_shared_wait(& done, 3, TIMEOUT_MS), 0);
    for (i = 0; i < 3; i ++) {
        pthread_join(threads[i], NULL);
        ASSERT_EQ_INT(w[i].result, -1);
    }
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), -1);

    /* the holder's unlock and any later attempt see the broken lock */
    lock_unlock(lock);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), -1);
    ASSERT_EQ_INT(lock_rdlock(lock), -1);
    ASSERT_EQ_INT(lock_wrlock(lock), -1);
    ASSERT_EQ_INT(lock_upgrade(lock), -1);
    lock_destroy(lock);
    ASSERT_NULL(lock_free(lock));

    /* a reader holds; another reader claims an upgrade and spins */
    ASSERT_NOT_NULL(lock = lock_alloc());
    ASSERT_EQ_INT(lock_init(lock), 0);
    _shared_init(& done);
    ASSERT_EQ_INT(lock_rdlock(lock), 0);
    memset(& w[3], 0, sizeof(w[3]));
    w[3].lock = lock; w[3].done = & done;
    ASSERT_EQ_INT(pthread_create(& threads[3], NULL, _late_upgrader, & w[3]),
                  0);
    ASSERT_EQ_INT(_shared_wait(& done, 1, TIMEOUT_MS), 0);
    usleep(50000);
    ASSERT_EQ_INT(_shared_get(& done), 1);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), RDLOCKED + LOCKSTEP + 1);

    lock_break(lock);
    ASSERT_EQ_INT(_shared_wait(& done, 2, TIMEOUT_MS), 0);
    pthread_join(threads[3], NULL);
    ASSERT_EQ_INT(w[3].result, -1);
    lock_unlock(lock);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), -1);

    lock_destroy(lock);
    ASSERT_NULL(lock_free(lock));

    return 0;
}

/* -------------------------------------------------------------------------- */

static void *_stresser(void *arg)
{
    /** @brief random reads, writes and upgrades until told to stop */

    _Worker *w = arg;
    unsigned int seed = test_seed() + w->id, n = 0;

    while (! _shared_get(w->done)) {
        seed = seed * 1103515245u + 12345u;
        n = (seed >> 16) % 10;
        if (n < 6) {
            if (lock_rdlock(w->lock) == -1) break;
            _monitor_enter(0);
            _monitor_leave(0);
            lock_unlock(w->lock);
        } else if (n < 8) {
            if (lock_rdlock(w->lock) == -1) break;
            _monitor_enter(0);
            _monitor_leave(0);
            if (lock_upgrade(w->lock) == -1) break;
            _monitor_enter(1);
            _monitor_leave(1);
            lock_restore(w->lock);
            _monitor_enter(0);
            _monitor_leave(0);
            lock_unlock(w->lock);
        } else {
            if (lock_wrlock(w->lock) == -1) break;
            _monitor_enter(1);
            _monitor_leave(1);
            lock_unlock(w->lock);
        }
        w->rounds ++;
    }
    w->result = _shared_get(w->done) ? 0 : -1;

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _stress(void)
{
    /* readers, writers and upgraders hammer one lock: writers and
       upgraders are always alone, and nobody gets stuck */

    static _Worker w[STRESSERS];
    static _Shared stop;
    pthread_t threads[STRESSERS];
    RW_Lock *lock = NULL;
    unsigned int i = 0, rounds = 0;

    ASSERT_NOT_NULL(lock = lock_alloc());
    ASSERT_EQ_INT(lock_init(lock), 0);
    _shared_init(& stop);
    _violations = 0; _readers = 0; _writers = 0;

    for (i = 0; i < STRESSERS; i ++) {
        memset(& w[i], 0, sizeof(w[i]));
        w[i].lock = lock; w[i].done = & stop; w[i].id = i;
        ASSERT_EQ_INT(pthread_create(& threads[i], NULL, _stresser, & w[i]),
                      0);
    }

    usleep(STRESS_MS * 1000);
    _shared_add(& stop, 1);
    for (i = 0; i < STRESSERS; i ++) {
        pthread_join(threads[i], NULL);
        ASSERT_EQ_INT(w[i].result, 0);
        rounds += w[i].rounds;
    }

    printf("%u lock rounds\n", rounds);
    ASSERT_TRUE(rounds > STRESSERS);
    ASSERT_EQ_INT(_violations, 0);
    ASSERT_EQ_INT(_LOCKSTATE_GET(lock), UNLOCKED);
    ASSERT_EQ_INT(_LOCKWFLAG_GET(lock), 0);

    lock_destroy(lock);
    ASSERT_NULL(lock_free(lock));

    return 0;
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("states", _states),
    TEST("writer_and_readers_wait", _writer_and_readers_wait),
    TEST("upgrade_waits_for_readers", _upgrade_waits_for_readers),
    TEST("concurrent_upgraders", _concurrent_upgraders),
    TEST("break_with_waiters", _break_with_waiters),
    TEST("stress", _stress)
};

TEST_SUITE(test_suite_rwlock, "rwlock", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
