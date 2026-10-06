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
#include "../../lib/askl_steque.h"

#define ITEMS   200000
#define WORKERS 4

/* items are small integers disguised as pointers, never 0 */
#define ITEM(i) ((void *) (uintptr_t) (i))
#define VALUE(p) ((uintptr_t) (p))

typedef struct _Worker {
    Queue *queue;
    unsigned int id;
    uint64_t sum;
    unsigned int count;
} _Worker;

static unsigned int _freed = 0;

/* -------------------------------------------------------------------------- */

static void *_count_free(void *data)
{
    _freed += (data != NULL);
    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _fifo_and_lifo(void)
{
    Queue *q = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(q = queue_alloc());
    ASSERT_EQ_INT(queue_empty(q), 1);
    ASSERT_NULL(queue_pop(q));

    /* enqueue at the tail: first in, first out */
    for (i = 1; i <= 5; i ++) ASSERT_EQ_INT(queue_enqueue(q, ITEM(i)), 0);
    ASSERT_EQ_INT(queue_empty(q), 0);
    for (i = 1; i <= 5; i ++) ASSERT_EQ_UINT(VALUE(queue_pop(q)), i);
    ASSERT_EQ_INT(queue_empty(q), 1);
    ASSERT_NULL(queue_pop(q));

    /* push at the head: last in, first out */
    for (i = 1; i <= 5; i ++) ASSERT_EQ_INT(queue_push(q, ITEM(i)), 0);
    for (i = 5; i >= 1; i --) ASSERT_EQ_UINT(VALUE(queue_pop(q)), i);
    ASSERT_EQ_INT(queue_empty(q), 1);

    /* a push goes in front of what is queued */
    ASSERT_EQ_INT(queue_enqueue(q, ITEM(1)), 0);
    ASSERT_EQ_INT(queue_enqueue(q, ITEM(2)), 0);
    ASSERT_EQ_INT(queue_push(q, ITEM(3)), 0);
    ASSERT_EQ_INT(queue_enqueue(q, ITEM(4)), 0);
    ASSERT_EQ_UINT(VALUE(queue_pop(q)), 3);
    ASSERT_EQ_UINT(VALUE(queue_pop(q)), 1);
    ASSERT_EQ_UINT(VALUE(queue_pop(q)), 2);
    ASSERT_EQ_UINT(VALUE(queue_pop(q)), 4);
    ASSERT_NULL(queue_pop(q));

    /* a NULL item is refused, since NULL means empty on the way out */
    ASSERT_EQ_INT(queue_enqueue(q, NULL), -1);
    ASSERT_EQ_INT(queue_push(q, NULL), -1);
    ASSERT_EQ_INT(queue_empty(q), 1);

    /* the queue is reusable once drained */
    for (i = 1; i <= 3; i ++) ASSERT_EQ_INT(queue_enqueue(q, ITEM(i)), 0);
    for (i = 1; i <= 3; i ++) ASSERT_EQ_UINT(VALUE(queue_pop(q)), i);
    ASSERT_EQ_INT(queue_empty(q), 1);

    ASSERT_NULL(queue_free(q));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _free_nodes(void)
{
    Queue *q = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(q = queue_alloc());
    _freed = 0;

    for (i = 1; i <= 10; i ++) ASSERT_EQ_INT(queue_enqueue(q, ITEM(i)), 0);
    queue_free_nodes(q, _count_free);
    ASSERT_EQ_UINT(_freed, 10);
    ASSERT_EQ_INT(queue_empty(q), 1);
    ASSERT_NULL(queue_pop(q));

    /* nothing to free is fine, and so is a missing callback */
    queue_free_nodes(q, _count_free);
    ASSERT_EQ_INT(queue_enqueue(q, ITEM(1)), 0);
    queue_free_nodes(q, NULL);
    ASSERT_EQ_UINT(_freed, 10);
    ASSERT_EQ_UINT(VALUE(queue_pop(q)), 1);

    ASSERT_NULL(queue_free(q));

    return 0;
}

/* -------------------------------------------------------------------------- */

static void *_late_producer(void *arg)
{
    usleep(20000);
    queue_enqueue(arg, ITEM(1));

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _wait(void)
{
    /* waiting on an empty queue returns after the timeout, and earlier
       when an item arrives */

    Queue *q = NULL;
    pthread_t producer;
    struct timespec before, after;
    long elapsed = 0;

    ASSERT_NOT_NULL(q = queue_alloc());

    queue_wait(q, 0);
    ASSERT_EQ_INT(queue_empty(q), 1);

    clock_gettime(CLOCK_MONOTONIC, & before);
    queue_wait(q, 10000);
    clock_gettime(CLOCK_MONOTONIC, & after);
    elapsed = (after.tv_sec - before.tv_sec) * 1000000L +
              (after.tv_nsec - before.tv_nsec) / 1000L;
    ASSERT_TRUE(elapsed >= 9000);
    ASSERT_EQ_INT(queue_empty(q), 1);

    ASSERT_EQ_INT(pthread_create(& producer, NULL, _late_producer, q), 0);
    clock_gettime(CLOCK_MONOTONIC, & before);
    queue_wait(q, 5000000);
    clock_gettime(CLOCK_MONOTONIC, & after);
    pthread_join(producer, NULL);
    elapsed = (after.tv_sec - before.tv_sec) * 1000000L +
              (after.tv_nsec - before.tv_nsec) / 1000L;
    ASSERT_TRUE(elapsed < 4000000);
    ASSERT_EQ_UINT(VALUE(queue_pop(q)), 1);

    /* waiting on a queue with items returns at once */
    ASSERT_EQ_INT(queue_enqueue(q, ITEM(2)), 0);
    clock_gettime(CLOCK_MONOTONIC, & before);
    queue_wait(q, 1000000);
    clock_gettime(CLOCK_MONOTONIC, & after);
    elapsed = (after.tv_sec - before.tv_sec) * 1000000L +
              (after.tv_nsec - before.tv_nsec) / 1000L;
    ASSERT_TRUE(elapsed < 500000);
    ASSERT_EQ_UINT(VALUE(queue_pop(q)), 2);

    ASSERT_NULL(queue_free(q));

    return 0;
}

/* -------------------------------------------------------------------------- */

static void *_enqueuer(void *arg)
{
    _Worker *w = arg;
    unsigned int i = 0;

    for (i = 1; i <= ITEMS; i ++) queue_enqueue(w->queue, ITEM(i));

    return NULL;
}

/* -------------------------------------------------------------------------- */

static void *_pusher(void *arg)
{
    _Worker *w = arg;
    unsigned int i = 0;

    for (i = 1; i <= ITEMS; i ++) queue_push(w->queue, ITEM(i));

    return NULL;
}

/* -------------------------------------------------------------------------- */

static void *_ordered_consumer(void *arg)
{
    /** @brief pop every item, which must come out in enqueue order */

    _Worker *w = arg;
    void *item = NULL;
    unsigned int expected = 1;

    while (expected <= ITEMS) {
        if (! (item = queue_pop(w->queue)) ) {
            queue_wait(w->queue, 1000);
            continue;
        }
        if (VALUE(item) != expected) {
            w->count = expected;
            return NULL;
        }
        expected ++;
    }

    w->count = 0;

    return NULL;
}

/* -------------------------------------------------------------------------- */

static void *_consumer(void *arg)
{
    /** @brief pop items until the sentinel, summing them up */

    _Worker *w = arg;
    void *item = NULL;

    while (1) {
        if (! (item = queue_pop(w->queue)) ) {
            queue_wait(w->queue, 1000);
            continue;
        }
        if (VALUE(item) == ITEMS + 1) return NULL;
        w->sum += VALUE(item); w->count ++;
    }
}

/* -------------------------------------------------------------------------- */

static int _concurrent_enqueue_pop(void)
{
    /* one producer, one consumer: every item comes out, in order */

    _Worker producer = { 0 }, consumer = { 0 };
    pthread_t threads[2];
    Queue *q = NULL;

    ASSERT_NOT_NULL(q = queue_alloc());
    producer.queue = consumer.queue = q;

    ASSERT_EQ_INT(pthread_create(& threads[0], NULL, _enqueuer, & producer), 0);
    ASSERT_EQ_INT(pthread_create(& threads[1], NULL, _ordered_consumer,
                                 & consumer), 0);
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    if (consumer.count)
        FAIL("an item came out of order");
    ASSERT_EQ_INT(queue_empty(q), 1);

    ASSERT_NULL(queue_free(q));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _concurrent_push_pop(void)
{
    /* one pusher, one consumer: every item comes out exactly once */

    _Worker producer = { 0 }, consumer = { 0 };
    pthread_t threads[2];
    Queue *q = NULL;

    ASSERT_NOT_NULL(q = queue_alloc());
    producer.queue = consumer.queue = q;

    ASSERT_EQ_INT(pthread_create(& threads[0], NULL, _pusher, & producer), 0);
    pthread_join(threads[0], NULL);
    ASSERT_EQ_INT(queue_enqueue(q, ITEM(ITEMS + 1)), 0);
    ASSERT_EQ_INT(pthread_create(& threads[1], NULL, _consumer, & consumer), 0);
    pthread_join(threads[1], NULL);

    ASSERT_EQ_UINT(consumer.count, ITEMS);
    ASSERT_EQ_UINT(consumer.sum, (uint64_t) ITEMS * (ITEMS + 1) / 2);
    ASSERT_EQ_INT(queue_empty(q), 1);

    ASSERT_NULL(queue_free(q));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _many_producers_consumers(void)
{
    /* several producers and consumers at once: the consumers see every
       item exactly once, whatever the interleaving */

    static _Worker producers[WORKERS], consumers[WORKERS];
    pthread_t threads[2 * WORKERS];
    Queue *q = NULL;
    unsigned int i = 0, count = 0;
    uint64_t sum = 0;

    ASSERT_NOT_NULL(q = queue_alloc());

    for (i = 0; i < WORKERS; i ++) {
        memset(& producers[i], 0, sizeof(producers[i]));
        memset(& consumers[i], 0, sizeof(consumers[i]));
        producers[i].queue = consumers[i].queue = q;
        producers[i].id = consumers[i].id = i;
        ASSERT_EQ_INT(pthread_create(
            & threads[i], NULL, (i & 1) ? _pusher : _enqueuer, & producers[i]
        ), 0);
        ASSERT_EQ_INT(pthread_create(
            & threads[WORKERS + i], NULL, _consumer, & consumers[i]
        ), 0);
    }

    for (i = 0; i < WORKERS; i ++) pthread_join(threads[i], NULL);
    for (i = 0; i < WORKERS; i ++)
        ASSERT_EQ_INT(queue_enqueue(q, ITEM(ITEMS + 1)), 0);
    for (i = 0; i < WORKERS; i ++) {
        pthread_join(threads[WORKERS + i], NULL);
        count += consumers[i].count;
        sum += consumers[i].sum;
    }

    ASSERT_EQ_UINT(count, (uint64_t) WORKERS * ITEMS);
    ASSERT_EQ_UINT(sum, (uint64_t) WORKERS * ITEMS * (ITEMS + 1) / 2);
    ASSERT_EQ_INT(queue_empty(q), 1);

    ASSERT_NULL(queue_free(q));

    return 0;
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("fifo_and_lifo", _fifo_and_lifo),
    TEST("free_nodes", _free_nodes),
    TEST("wait", _wait),
    TEST("concurrent_enqueue_pop", _concurrent_enqueue_pop),
    TEST("concurrent_push_pop", _concurrent_push_pop),
    TEST("many_producers_consumers", _many_producers_consumers)
};

TEST_SUITE(test_suite_queue, "queue", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
