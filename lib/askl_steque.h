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

#ifndef ASKL_QUEUE_H

#define ASKL_QUEUE_H

#include "askl.h"

/** @defgroup queue ASKL::queue */

typedef struct _ASKL_Queue ASKL_Queue;

/* -------------------------------------------------------------------------- */

public ASKL_Queue *queue_alloc(void);

/**
 * @ingroup queue
 * @fn ASKL_Queue *queue_alloc(void)
 * @param void
 * @return a pointer to a new ASKL_Queue, or NULL.
 *
 * This function allocates and fully initializes a new concurrent queue.
 *
 * The returned queue must be destroyed with @ref queue_free() when no longer
 * needed.
 */


/* -------------------------------------------------------------------------- */

public ASKL_Queue *queue_free(ASKL_Queue *queue);

/**
 * @ingroup queue
 * @fn ASKL_Queue *queue_free(ASKL_Queue *queue)
 * @param queue a pointer to a queue
 * @return always NULL
 *
 * This function destroys an empty queue.
 *
 * To ensure all remaining nodes and their contents have been freed, call
 * @ref queue_free_nodes() before @ref queue_free().
 *
 * This function always returns NULL so it can safely be used to clean a
 * pointer:
 * @code
 * queue = queue_free(queue);
 * @endcode
 */

/* -------------------------------------------------------------------------- */

public void queue_free_nodes(ASKL_Queue *queue, void *(*free_data)(void *));

/**
 * @ingroup queue
 * @fn void queue_free_nodes(ASKL_Queue *queue, void *(*free_data)(void *))
 * @param queue a pointer to a queue
 * @param free_data a callback used to destroy the data stored in the queue
 * @return void
 *
 * This function removes all nodes from @p queue and, for each element, calls
 * the @p free_data callback with the stored pointer as argument.
 *
 * @note This function assumes that no other thread is concurrently enqueuing
 *       or dequeuing elements from @p queue.
 */

/* -------------------------------------------------------------------------- */

public int queue_enqueue(ASKL_Queue *queue, void *ptr);

/**
 * @ingroup queue
 * @fn int queue_enqueue(ASKL_Queue *queue, void *ptr)
 * @param queue a pointer to a queue
 * @param ptr a non-NULL pointer to data to enqueue
 * @return -1 if an error occurs, 0 otherwise
 *
 * This function enqueues the given pointer at the tail of @p queue, in FIFO
 * order. The data will be returned by a subsequent call to @ref queue_pop().
 *
 * @note The @p ptr argument must be non-NULL.
 */


/* -------------------------------------------------------------------------- */

public int queue_empty(ASKL_Queue *queue);

/**
 * @ingroup queue
 * @fn int queue_empty(ASKL_Queue *queue)
 * @param queue a pointer to a queue
 * @return 1 if the queue is empty, 0 otherwise
 *
 * This function tests whether @p queue currently contains any enqueued items.
 */

/* -------------------------------------------------------------------------- */

public void queue_wait(ASKL_Queue *queue, unsigned int duration);

/**
 * @ingroup queue
 * @fn void queue_wait(ASKL_Queue *queue, unsigned int microseconds)
 * @param queue a pointer to a queue
 * @param microseconds the maximum time to wait for an item to be enqueued
 * @return void
 *
 * This function waits up to @p microseconds for an item to be enqueued into
 * @p queue.
 *
 * @note This function does not guarantee that the queue is non-empty when it
 *       returns; it merely provides a convenient way to sleep until an enqueue
 *       event or timeout. Call @ref queue_empty() or @ref queue_pop() to
 *       inspect the queue after waking up.
 */

/* -------------------------------------------------------------------------- */

public void *queue_pop(ASKL_Queue *queue);

/**
 * @ingroup queue
 * @fn void *queue_pop(ASKL_Queue *queue)
 * @param queue a pointer to a queue
 * @return a pointer to the head item, or NULL if the queue is empty
 *
 * This function removes and returns the item stored at the head of @p queue.
 * The head corresponds to the next item to be processed: either the oldest
 * enqueued item or the most recently pushed item.
 * If the queue is empty, it returns NULL.
 */


/* -------------------------------------------------------------------------- */

public int queue_push(ASKL_Queue *queue, void *ptr);

/**
 * @ingroup queue
 * @fn int queue_push(ASKL_Queue *queue, void *ptr)
 * @param queue a pointer to a queue
 * @param ptr a non-NULL pointer to data to push onto the queue
 * @return -1 if an error occurs, 0 otherwise
 *
 * This function inserts the given item at the head of @p queue, so it will be
 * returned by the next call to @ref queue_pop().
 *
 * @note You can implement a stack (LIFO) by using @ref queue_push() and
 *       @ref queue_pop().
 * @note The @p ptr argument must be non-NULL.
 */


/* -------------------------------------------------------------------------- */

#endif
