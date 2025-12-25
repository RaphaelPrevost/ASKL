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

#include "stream.h"

/* MASTER: number of master streams */
static int master_streams = 0;

/* MASTER: list of available workers */
static ASKL_SocketQueue **_workers = NULL;

/* MASTER: list of available connections */
static ASKL_SocketQueue **_pending = NULL;

/* MASTER: list of connections waiting for a worker */
static ASKL_SocketQueue **_waiting = NULL;

/* MASTER: pending packets */
static pthread_mutex_t _packets_lock = PTHREAD_MUTEX_INITIALIZER;
static ASKL_Queue *_packets[SOCKET_MAX];

/* ALL: link status */
static pthread_mutex_t _status_lock = PTHREAD_MUTEX_INITIALIZER;
static char _status[SOCKET_MAX];

/* -------------------------------------------------------------------------- */

private int stream_socket_init(void)
{
    int i = 0;

    if (stream_personality() & PERSONALITY_MASTER) {
        if ( (master_streams = stream_master_streams()) ) {
            if (! (_workers = malloc(master_streams * sizeof(*_workers))) )
                return -1;
            if (! (_pending = malloc(master_streams * sizeof(*_pending))) )
                return -1;
            if (! (_waiting = malloc(master_streams * sizeof(*_waiting))) )
                return -1;

            memset(_workers, 0, master_streams * sizeof(*_workers));
            memset(_pending, 0, master_streams * sizeof(*_pending));
            memset(_waiting, 0, master_streams * sizeof(*_waiting));

            for (i = 0; i < master_streams; i ++) {
                if (! (_workers[i] = socket_queue_alloc()) ) return -1;
                if (! (_pending[i] = socket_queue_alloc()) ) return -1;
                if (! (_waiting[i] = socket_queue_alloc()) ) return -1;
            }
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

private int stream_set_status(uint16_t socket_id, int status)
{
    int ret = 0, mask = 0;

    if (socket_id < 1 || socket_id >= SOCKET_MAX) {
        debug("stream_set_status(): bad parameters.\n");
        return -1;
    }

    pthread_mutex_lock(& _status_lock);
        if (_status[socket_id] && status) {
            mask = ~(_status[socket_id] | (_status[socket_id] - 1));
            if (status & mask) _status[socket_id] = status;
            else ret = -1;
        } else _status[socket_id] = status;
    pthread_mutex_unlock(& _status_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

private int stream_get_status(uint16_t socket_id)
{
    int status = 0;

    if (socket_id < 1 || socket_id >= SOCKET_MAX) {
        debug("stream_get_status(): bad parameters.\n");
        return 0;
    }

    pthread_mutex_lock(& _status_lock);
        status = _status[socket_id];
    pthread_mutex_unlock(& _status_lock);

    return status;
}

/* -------------------------------------------------------------------------- */

private void stream_add_worker(int stream_id, uint16_t worker)
{
    if (worker < 1 || worker >= SOCKET_MAX) {
        debug("stream_add_worker(): bad parameters.\n");
        return;
    }
    if (stream_id < 0 || stream_id >= _STREAMS_MAX) {
        debug("stream_add_worker(): bad parameters.\n");
        return;
    }

    if (! stream_set_status(worker, STREAM_STATUS_WORK))
        socket_enqueue(_workers[stream_id], worker);
}

/* -------------------------------------------------------------------------- */

private uint16_t stream_borrow_worker(int stream_id)
{
    uint16_t worker = 0;

    if (stream_id < 0 || stream_id >= _STREAMS_MAX) {
        debug("stream_borrow_worker(): bad parameters.\n");
        return 0;
    }

    do worker = socket_dequeue(_workers[stream_id]);
    while (worker && stream_get_status(worker) != STREAM_STATUS_WORK);

    return worker;
}

/* -------------------------------------------------------------------------- */

private uint16_t stream_release_worker(int stream_id, uint16_t worker)
{
    if (worker < 1 || worker >= SOCKET_MAX) {
        debug("stream_release_worker(): bad parameters.\n");
        return 0;
    }

    if (stream_id < 0 || stream_id >= _STREAMS_MAX) {
        debug("stream_release_worker(): bad parameters.\n");
        return 0;
    }

    if (stream_get_status(worker) == STREAM_STATUS_WORK)
        socket_enqueue(_workers[stream_id], worker);

    return 0;
}

/* -------------------------------------------------------------------------- */

private uint16_t stream_enqueue_connection(int stream_id, uint16_t conn)
{
    if (conn < 1 || conn >= SOCKET_MAX) {
        debug("stream_enqueue_connection(): bad parameters.\n");
        return 0;
    }

    if (stream_id < 0 || stream_id >= _STREAMS_MAX) {
        debug("stream_enqueue_connection(): bad parameters.\n");
        return 0;
    }

    if (! stream_set_status(conn, STREAM_STATUS_WAIT))
        socket_enqueue(_pending[stream_id], conn);

    return 0;
}

/* -------------------------------------------------------------------------- */

private uint16_t stream_dequeue_connection(int stream_id)
{
    uint16_t conn = 0;

    if (stream_id < 0 || stream_id >= _STREAMS_MAX) {
        debug("stream_dequeue_connection(): bad parameters.\n");
        return 0;
    }

    do conn = socket_dequeue(_pending[stream_id]);
    while (conn && stream_get_status(conn) != STREAM_STATUS_WAIT);

    return conn;
}

/* -------------------------------------------------------------------------- */

private uint16_t stream_enqueue_waiting(int stream_id, uint16_t conn)
{
    if (conn < 1 || conn >= SOCKET_MAX) {
        debug("stream_enqueue_waiting(): bad parameters.\n");
        return 0;
    }

    if (stream_id < 0 || stream_id >= _STREAMS_MAX) {
        debug("stream_enqueue_waiting(): bad parameters.\n");
        return 0;
    }

    socket_enqueue(_waiting[stream_id], conn);

    return 0;
}

/* -------------------------------------------------------------------------- */

private uint16_t stream_dequeue_waiting(int stream_id)
{
    uint16_t conn = 0;

    if (stream_id < 0 || stream_id >= _STREAMS_MAX) {
        debug("stream_dequeue_waiting(): bad parameters.\n");
        return 0;
    }

    do conn = socket_dequeue(_waiting[stream_id]);
    while (conn && stream_get_status(conn) != STREAM_STATUS_CONN);

    return conn;
}

/* -------------------------------------------------------------------------- */

private m_string *stream_enqueue_packet(uint16_t socket_id, m_string *data)
{
    if (socket_id < 1 || socket_id >= SOCKET_MAX || ! data) {
        debug("stream_enqueue_packet(): bad parameters.\n");
        return NULL;
    }

    pthread_mutex_lock(& _packets_lock);

        if (! _packets[socket_id]) _packets[socket_id] = queue_alloc();

        if (_packets[socket_id])
            queue_enqueue(_packets[socket_id], string_dup(data));

    pthread_mutex_unlock(& _packets_lock);

    return NULL;
}

/* -------------------------------------------------------------------------- */

private m_string *stream_dequeue_packet(uint16_t socket_id)
{
    m_string *data = NULL;

    if (socket_id < 1 || socket_id >= SOCKET_MAX) {
        debug("stream_dequeue_packet(): bad parameters.\n");
        return NULL;
    }

    pthread_mutex_lock(& _packets_lock);

        data = queue_pop(_packets[socket_id]);

    pthread_mutex_unlock(& _packets_lock);

    return data;
}

/* -------------------------------------------------------------------------- */

private void stream_drop_packets(uint16_t socket_id)
{
    if (socket_id < 1 || socket_id >= SOCKET_MAX) {
        debug("stream_drop_packets(): bad parameters.\n");
        return;
    }

    pthread_mutex_lock(& _packets_lock);

        queue_free_nodes(_packets[socket_id], (void *(*)(void *)) string_free);
        _packets[socket_id] = queue_free(_packets[socket_id]);

    pthread_mutex_unlock(& _packets_lock);
}

/* -------------------------------------------------------------------------- */

private void stream_flush_packets(uint16_t socket_id, uint16_t egress)
{
    m_string *data = NULL;
    int packets = 0;

    if (socket_id < 1 || socket_id >= SOCKET_MAX) {
        debug("stream_flush_packets(): bad parameters.\n");
        return;
    }

    pthread_mutex_lock(& _packets_lock);

        while ( (data = queue_pop(_packets[socket_id])) ) {
            debug("sending last packets...\n");
            server_send_buffer(
                module_get_token(),
                egress,
                queue_empty(_packets[socket_id]) ? SERVER_MSG_END : 0x0,
                DATA(data),
                SIZE(data)
            );
            packets ++;
        }

        _packets[socket_id] = queue_free(_packets[socket_id]);

    pthread_mutex_unlock(& _packets_lock);

    if (! packets) server_close_managed_socket(module_get_token(), egress);
}

/* -------------------------------------------------------------------------- */

private int stream_get_connection(int stream_id)
{
    uint16_t worker = 0;

    /* try to get a worker */
    if (! (worker = stream_borrow_worker(stream_id)) ) {
        debug("Stream: no worker available on stream %i.\n", stream_id);
        return -1;
    }

    /* ask the worker to connect */
    server_send_response(
        module_get_token(),
        worker,
        0x0,
        "%bB4u",
        MASTER_OP_HIRED
    );

    stream_release_worker(stream_id, worker);

    return 0;
}

/* -------------------------------------------------------------------------- */

private int stream_get_pipe(int stream_id, uint16_t socket_id)
{
    uint16_t worker = 0;
    m_string *packet = NULL;

    if (socket_id < 1 || socket_id >= SOCKET_MAX) {
        debug("stream_get_pipe(): bad parameters.\n");
        return -1;
    }

    /* try to get a connection */
    if ( (worker = stream_dequeue_connection(stream_id)) ) {
        /* record the pipe */
        stream_set_route(ROUTE_WORKER, socket_id, worker);

        /* set the proper status */
        if (stream_set_status(socket_id, STREAM_STATUS_PIPE)) return -1;
        if (stream_set_status(worker, STREAM_STATUS_PIPE)) return -1;

        /* check if there is pending packets and send them directly */
        while ( (packet = stream_dequeue_packet(socket_id)) )
            server_send_string(module_get_token(), worker, 0x0, packet);
    }

    if (! stream_get_connection(stream_id) && ! worker) {
        /* successfully asked for a connection, but none ready yet */
        stream_enqueue_waiting(stream_id, socket_id);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

private void stream_open_pipe(int stream_id)
{
    int master_id = 0, server_id = 0, ingress_id = 0;
    const char *host = NULL;
    const char *port = NULL;

    /* connect to the master */
    host = stream_config_host(stream_id, MASTER_ADDRESS);
    port = stream_config_port(stream_id, MASTER_ADDRESS);
    ingress_id = stream_get_ingress(stream_id, ROUTE_MASTER);

    master_id = server_open_managed_socket(
        module_get_token(),
        host,
        port,
        INGRESS(ingress_id)
    );

    server_send_response(
        module_get_token(),
        master_id,
        0x0,
        "%bB4u",
        WORKER_OP_READY
    );

    /* connect to the endpoint */
    host = stream_config_host(stream_id, SERVER_ADDRESS);
    port = stream_config_port(stream_id, SERVER_ADDRESS);
    ingress_id = stream_get_ingress(stream_id, ROUTE_SERVER);

    server_id = server_open_managed_socket(
        module_get_token(),
        host,
        port,
        INGRESS(ingress_id)
    );

    /* record the pipe */
    stream_set_route(ROUTE_SERVER, master_id, server_id);

    /* set the proper status */
    stream_set_status(master_id, STREAM_STATUS_PIPE);
    stream_set_status(server_id, STREAM_STATUS_PIPE);

    return;
}

/* -------------------------------------------------------------------------- */

private void stream_socket_exit(void)
{
    int i = 0;

    if (stream_personality() & PERSONALITY_MASTER) {
        if (master_streams) {
            if (_workers) {
                for (i = 0; i < master_streams; i ++)
                    _workers[i] = socket_queue_free(_workers[i]);
                free(_workers);
            }

            if (_pending) {
                for (i = 0; i < master_streams; i ++)
                    _pending[i] = socket_queue_free(_pending[i]);
                free(_pending);
            }

            if (_waiting) {
                for (i = 0; i < master_streams; i ++)
                    _waiting[i] = socket_queue_free(_waiting[i]);
                free(_waiting);
            }
        }

        for (i = 0; i < SOCKET_MAX; i ++) {
            queue_free_nodes(_packets[i], (void *(*)(void *)) string_free);
            queue_free(_packets[i]);
        }
    }
}

/* -------------------------------------------------------------------------- */
