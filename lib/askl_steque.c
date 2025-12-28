/*******************************************************************************
 *  ASKL.                                                                      *
 *  Copyright (c) 2025 Raphael Prevost <raph@el.bzh>                           *
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

#include "askl_steque.h"

typedef struct _node {
    struct _node *next;
    void *data;
} _node;

struct _ASKL_Queue {
    pthread_mutex_t _head_lock;
    pthread_mutex_t _tail_lock;
    pthread_cond_t _empty;

    _node *_head;
    _node *_tail;
};

/**
 * @ingroup queue
 * @struct ASKL_Queue
 *
 * This structure implements a two-lock concurrent stack-ended queue (steque).
 * Its fields are private and should not be directly accessed.
 * 
 * A steque supports the following operations:
 * - push at the head (LIFO-style insertion),
 * - pop from the head,
 * - enqueue at the tail (FIFO-style insertion).
 * 
 * Internally, this implementation uses two locks to reduce contention:
 * one protects the head, and one protects the tail.
 *
 * The head and tail are _node structures.
 * The _node structure holds the data enqueued or pushed onto the queue:
 *
 * typedef struct _node {
 *   struct _node *_next;
 *   void *_data;
 * } _node;
 *
 * @b private _data is used to carry a non-NULL pointer, or any integer type
 *            that fits within uintptr_t.
 * @b private _next is used to link the nodes of the queue.
 */

/* -------------------------------------------------------------------------- */

public ASKL_Queue *queue_alloc(void)
{
    pthread_condattr_t attr;
    ASKL_Queue *ret = malloc(sizeof(*ret));

    if (! ret) {
        perror(ERR(queue_alloc, malloc));
        return NULL;
    }

    if (pthread_mutex_init(& ret->_head_lock, NULL) == -1) {
        perror(ERR(queue_alloc, pthread_mutex_init));
        goto _err_head_lock;
    }

    if (pthread_mutex_init(& ret->_tail_lock, NULL) == -1) {
        perror(ERR(queue_alloc, pthread_mutex_init));
        goto _err_tail_lock;
    }

    pthread_condattr_init(& attr);

    #if ! defined(WIN32) && ! defined(__APPLE__)
    /* use the monotonic clock on POSIX compliant systems */
    pthread_condattr_setclock(& attr, CLOCK_MONOTONIC);
    #endif

    if (pthread_cond_init(& ret->_empty, & attr) == -1) {
        perror(ERR(queue_alloc, pthread_cond_init));
        goto _err_cond_init;
    }

    if (! (ret->_head = ret->_tail = malloc(sizeof(*ret->_head))) ) {
        perror(ERR(queue_alloc, malloc));
        goto _err_node_alloc;
    }

    ret->_head->next = ret->_head->data = NULL;

    pthread_condattr_destroy(& attr);

    return ret;

_err_node_alloc:
    pthread_cond_destroy(& ret->_empty);
_err_cond_init:
    pthread_condattr_destroy(& attr);
    pthread_mutex_destroy(& ret->_tail_lock);
_err_tail_lock:
    pthread_mutex_destroy(& ret->_head_lock);
_err_head_lock:
    free(ret);

    return NULL;
}

/* -------------------------------------------------------------------------- */

public ASKL_Queue *queue_free(ASKL_Queue *queue)
{
    if (! queue) return NULL;

    pthread_mutex_destroy(& queue->_head_lock);
    pthread_mutex_destroy(& queue->_tail_lock);
    pthread_cond_destroy(& queue->_empty);
    free(queue->_head); free(queue);

    return NULL;
}

/* -------------------------------------------------------------------------- */

public void queue_free_nodes(ASKL_Queue *queue, void *(*free_data)(void *))
{
    void *ptr = NULL;

    if (queue && free_data)
        while ( (ptr = queue_pop(queue)) ) free_data(ptr);
}

/* -------------------------------------------------------------------------- */

public int queue_enqueue(ASKL_Queue *queue, void *ptr)
{
    _node *node = NULL;

    if (! queue || ! ptr) {
        debug("queue_enqueue(): bad parameters.\n");
        return -1;
    }

    if (! (node = malloc(sizeof(*node))) ) {
        debug("queue_enqueue(): out of memory.\n");
        return -1;
    }

    node->data = ptr; node->next = NULL;

    /* use the Michael & Scott two-lock concurrent queue algorithm */
    pthread_mutex_lock(& queue->_tail_lock);

        /* swing the tail to the new node */
        queue->_tail->next = node;
        queue->_tail = node;
        /* if a thread was waiting to pop an element, wake it up */
        pthread_cond_signal(& queue->_empty);

    pthread_mutex_unlock(& queue->_tail_lock);

    return 0;
}

/* -------------------------------------------------------------------------- */

public int queue_empty(ASKL_Queue *queue)
{
    int ret = 0;

    if (! queue) return 1;

    pthread_mutex_lock(& queue->_head_lock);

        ret = (! queue->_head->next);

    pthread_mutex_unlock(& queue->_head_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

public void queue_wait(ASKL_Queue *q, unsigned int duration)
{
    struct timespec ts = { 0, 0 };

    if (! q || ! duration) return;

    #ifdef WIN32
    struct timeval tv;
    gettimeofday(& tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
    #elif ! defined(__APPLE__)
    monotonic_timer(& ts);
    #endif

    ts.tv_sec += duration / 1000000;
    ts.tv_nsec += (duration % 1000000) * 1000;

    #ifndef __APPLE__
    if (ts.tv_nsec >= 1000000000) {
        /* handle overflow */
        ts.tv_sec += ts.tv_nsec / 1000000000;
        ts.tv_nsec %= 1000000000;
    }
    #endif

    pthread_mutex_lock(& q->_tail_lock);

    while (queue_empty(q)) {
        #ifdef __APPLE__
        struct timespec start, awake;
        long elapsed;
        monotonic_timer(& start);
        pthread_cond_timedwait_relative_np(& q->_empty, & q->_tail_lock, & ts);
        monotonic_timer(& awake);
        elapsed = (awake.tv_nsec - start.tv_nsec) / 1000;
        elapsed += (awake.tv_sec - start.tv_sec) * 1000000;
        if (elapsed >= (long) duration) break;
        duration -= elapsed;
        ts.tv_sec = duration / 1000000;
        ts.tv_nsec = (duration % 1000000) * 1000;
        #else
        int ret;
        ret = pthread_cond_timedwait(& q->_empty, & q->_tail_lock, & ts);
        if (ret == ETIMEDOUT) break;
        #endif
    }

    pthread_mutex_unlock(& q->_tail_lock);

    return;
}

/* -------------------------------------------------------------------------- */

public void *queue_pop(ASKL_Queue *queue)
{
    _node *node = NULL, *next = NULL;
    void *ret = NULL;

    if (! queue) {
        debug("queue_pop(): bad parameters.\n");
        return NULL;
    }

    /* use the Michael & Scott two-lock concurrent queue algorithm */
    pthread_mutex_lock(& queue->_head_lock);

        node = queue->_head; next = node->next;

        /* pop the head and fetch the data */
        if (next) {
            ret = next->data;
            queue->_head = next;
            free(node);
        }

    pthread_mutex_unlock(& queue->_head_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

public int queue_push(ASKL_Queue *queue, void *ptr)
{
    _node *node = NULL;

    if (! queue || ! ptr) {
        debug("queue_push(): bad parameters.\n");
        return -1;
    }

    if (! (node = malloc(sizeof(*node))) ) {
        debug("queue_push(): out of memory.\n");
        return -1;
    }

    node->data = NULL;

    pthread_mutex_lock(& queue->_head_lock);

        /* store the data in the head, replace it with a dummy node */
        queue->_head->data = ptr;
        node->next = queue->_head;
        queue->_head = node;
        /* if a thread was waiting to pop an element, wake it up */
        pthread_cond_signal(& queue->_empty);

    pthread_mutex_unlock(& queue->_head_lock);

    return 0;
}

/* -------------------------------------------------------------------------- */
