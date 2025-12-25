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

/* streams */
static struct {
    int16_t route[4];
} streams[_STREAMS_MAX + 1];

/* ingress id */
static int16_t ingress = 1;

/* map an ingress id to a route type */
static int16_t route_type[INGRESS_MAX + 1];

/* map an ingress id to a stream */
static int16_t route_stream[INGRESS_MAX + 1];

/* map a master socket to a worker stream */
static int16_t worker_stream[SOCKET_MAX + 1];

/* MASTER: socket pairs */
static pthread_rwlock_t _master_lock;
static int16_t _public_to_worker[SOCKET_MAX + 1];
static int16_t _worker_to_public[SOCKET_MAX + 1];

/* WORKER: socket pairs */
static pthread_rwlock_t _worker_lock;
static int16_t _server_to_master[SOCKET_MAX + 1];
static int16_t _master_to_server[SOCKET_MAX + 1];

/* -------------------------------------------------------------------------- */

private int stream_router_init(void)
{
    const char *host = NULL;
    const char *port = NULL;
    int master = 0;
    int i = 0, s = 0, iid = 0, ret = 0;

    if (pthread_rwlock_init(& _master_lock, NULL) == -1) {
        perror(ERR(stream_router_init, pthread_rwlock_init));
        return -1;
    }

    if (pthread_rwlock_init(& _worker_lock, NULL) == -1) {
        perror(ERR(stream_router_init, pthread_rwlock_init));
        goto _err_lock;
    }

    if (stream_personality() & PERSONALITY_MASTER) {

        for (i = 0, s = stream_master_streams(); i < s; i ++) {
            /* ingress end */
            port = stream_config_port(i, STREAM_INGRESS);
            fprintf(stderr, "Stream[%i]: ingress port set to %s\n", i, port);

            iid = stream_open_ingress(i, ROUTE_PUBLIC);
            ret = server_open_managed_socket(
                module_get_token(),
                NULL,
                port,
                SOCKET_LISTEN(iid)
            );

            if (ret == -1) {
                fprintf(stderr, "Stream: cannot listen to port %s.\n", port);
                goto _err_init;
            }

            /* workers' end */
            port = stream_config_port(i, STREAM_WORKERS);
            fprintf(stderr, "Stream[%i]: workers port set to %s\n", i, port);

            iid = stream_open_ingress(i, ROUTE_WORKER);
            ret = server_open_managed_socket(
                module_get_token(),
                NULL,
                port,
                SOCKET_LISTEN(iid)
            );

            if (ret == -1) {
                fprintf(stderr, "Stream: cannot listen to port %s.\n", port);
                goto _err_init;
            }
        } /* STREAMS */
    } /* MASTER */

    /* WORKER */
    if (stream_personality() & PERSONALITY_WORKER) {

        memset(worker_stream, -1, sizeof(worker_stream));

        for (i = 0, s = stream_worker_streams(); i < s; i ++) {
            host = stream_config_host(i, MASTER_ADDRESS);
            port = stream_config_port(i, MASTER_ADDRESS);

            master = server_open_managed_socket(
                module_get_token(),
                host,
                port,
                SOCKET_CLIENT
            );

            if (master == -1) {
                fprintf(
                    stderr,
                    "Stream[%i]: connection to Master (%s:%s) failed.\n",
                    i, host, port
                );
                goto _err_init;
            }

            if (stream_heartbeat(master) == -1)
                fprintf(stderr, "Stream[%i]: failed to send heartbeat.\n", i);

            worker_stream[master] = i;

            /* allocate the ingress ids for the pipes */
            stream_open_ingress(i, ROUTE_MASTER);
            stream_open_ingress(i, ROUTE_SERVER);
        }
    }

    return 0;

_err_init:
    pthread_rwlock_destroy(& _worker_lock);
_err_lock:
    pthread_rwlock_destroy(& _master_lock);

    return -1;
}

/* -------------------------------------------------------------------------- */

private int stream_heartbeat(int socket_id)
{
    m_reply *reply = NULL;
    m_string *data = NULL;

    reply = server_reply_init(
        SERVER_MSG_ACK | SERVER_MSG_OOB,
        module_get_token()
    );

    if (! reply) goto _err_rep;

    if (! (data = string_alloc("!", 1)) ) goto _err_fmt;

    if (server_reply_setheader(reply, data) == -1) goto _err_set;

    /* send a heartbeat every minute */
    if (server_reply_setdelay(reply, 60) == -1) goto _err_set;

    reply = server_send_reply(socket_id, reply);

    return 0;

_err_set:
    string_free(data);
_err_fmt:
    server_reply_free(reply);
_err_rep:
    return -1;
}

/* -------------------------------------------------------------------------- */

private int stream_get_id(int hint, int personality)
{
    int ret = -1;

    if (personality == PERSONALITY_MASTER) {
        /* the hint must be a valid ingress id */
        if (hint < 1 || hint >= INGRESS_MAX) {
            debug("stream_get_id(): bad parameters.\n");
            return -1;
        }
        ret = route_stream[hint];
    } else if (personality == PERSONALITY_WORKER) {
        /* the hint must be a valid socket id */
        if (hint < 1 || hint >= SOCKET_MAX) {
            debug("stream_get_id(): bad parameters.\n");
            return -1;
        }
        ret = worker_stream[hint];
    }

    return ret;
}

/* -------------------------------------------------------------------------- */

private void stream_set_route(int type, uint16_t s0, uint16_t s1)
{
    if (type == ROUTE_WORKER) {
        pthread_rwlock_wrlock(& _master_lock);

            _public_to_worker[s0] = s1;
            _worker_to_public[s1] = s0;

        pthread_rwlock_unlock(& _master_lock);
    } else if (type == ROUTE_SERVER) {
        pthread_rwlock_wrlock(& _worker_lock);

            _master_to_server[s0] = s1;
            _server_to_master[s1] = s0;

        pthread_rwlock_unlock(& _worker_lock);
    }
}

/* -------------------------------------------------------------------------- */

private int stream_get_route(int ingress)
{
    return route_type[ingress];
}

/* -------------------------------------------------------------------------- */

private int stream_open_ingress(int stream_id, int type)
{
    if (stream_id < 0 || stream_id >= _STREAMS_MAX) {
        debug("stream_alloc_ingress(): bad parameters.\n");
        return 0;
    }

    if (type < 0 || type > ROUTE_MAX) {
        debug("stream_alloc_ingress(): bad parameters.\n");
        return 0;
    }

    if (ingress + 1 > INGRESS_MAX) {
        debug("stream_alloc_ingress(): all ids are in use!\n");
        return 0;
    }

    route_type[(int) ingress] = type;
    route_stream[(int) ingress] = stream_id;
    streams[stream_id].route[type] = ingress;

    return ingress ++;
}

/* -------------------------------------------------------------------------- */

private int stream_get_ingress(int stream_id, int type)
{
    if (stream_id < 0 || stream_id >= _STREAMS_MAX) {
        debug("stream_get_ingress(): bad parameters.\n");
        return 0;
    }

    if (type < 0 || type >= ROUTE_MAX) {
        debug("stream_get_ingress(): bad parameters.\n");
        return 0;
    }

    return streams[stream_id].route[type];
}

/* -------------------------------------------------------------------------- */

private uint16_t stream_get_egress(uint16_t socket_id, uint16_t ingress_id)
{
    uint16_t match = 0;

    if (socket_id < 1 || socket_id >= SOCKET_MAX) {
        debug("stream_get_egress(): bad parameters.\n");
        return 0;
    }

    switch (stream_get_route(ingress_id)) {

    case ROUTE_PUBLIC: { /* master <-> public : return matching worker socket */
        pthread_rwlock_rdlock(& _master_lock);
        match = _public_to_worker[socket_id];
        pthread_rwlock_unlock(& _master_lock);
    } break;

    case ROUTE_WORKER: { /* master <-> worker: return matching public socket */
        pthread_rwlock_rdlock(& _master_lock);
        match = _worker_to_public[socket_id];
        pthread_rwlock_unlock(& _master_lock);
    } break;

    case ROUTE_MASTER: { /* worker <-> server: return matching server socket */
        pthread_rwlock_rdlock(& _worker_lock);
        match = _master_to_server[socket_id];
        pthread_rwlock_unlock(& _worker_lock);
    } break;

    case ROUTE_SERVER: { /* worker <-> master: return matching master socket */
        pthread_rwlock_rdlock(& _worker_lock);
        match = _server_to_master[socket_id];
        pthread_rwlock_unlock(& _worker_lock);
    } break;

    default: debug("Stream: broken pipe.\n");

    }

    if (stream_get_status(match) == STREAM_STATUS_DOWN) match = 0;

    return match;
}

/* -------------------------------------------------------------------------- */

private void stream_router_exit(void)
{
    pthread_rwlock_destroy(& _worker_lock);
    pthread_rwlock_destroy(& _master_lock);
}

/* -------------------------------------------------------------------------- */
