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

#ifndef STREAM_MODULE_H

#define STREAM_MODULE_H

/* mandatory server API declarations */
#include "askl_server.h"
#include "askl_module.h"

/* optional, useful parts of the server public API */
#include "askl_socket.h"

#define STREAM_VERSION     ASKL_VERSION

/* ASKL supports up to 4095 ingress id, which means a maximum of 2047 streams */
#define _STREAMS_MAX       2047

#define PERSONALITY_MASTER 0x01
#define PERSONALITY_WORKER 0x02
#define PERSONALITY_HYBRID (PERSONALITY_MASTER | PERSONALITY_WORKER)

#define DESTINATION_SERVER 0x01
#define DESTINATION_MODULE 0x02

#define MASTER_ADDRESS     0x01
#define SERVER_ADDRESS     0x02
#define STREAM_INGRESS     0x04
#define STREAM_WORKERS     0x08

#define ROUTE_PUBLIC       0
#define ROUTE_WORKER       1
#define ROUTE_MASTER       2
#define ROUTE_SERVER       3
#define ROUTE_MAX          4

#define STREAM_STATUS_DOWN 0x00
#define STREAM_STATUS_CONN 0x01
#define STREAM_STATUS_WORK 0x02
#define STREAM_STATUS_WAIT 0x04
#define STREAM_STATUS_PIPE 0x10

#define WORKER_OP_HELLO    0xe110b055
#define WORKER_OP_READY    0x1e75d017
#define WORKER_OP_ALIVE    0x21         /* OOB, 8 bits max */
#define MASTER_OP_HIRED    0xa600d10b

/* -------------------------------------------------------------------------- */
/* MANDATORY MODULE CALLBACKS */
/* -------------------------------------------------------------------------- */

ASKL_API unsigned int module_api(void);

/**
 * @ingroup module
 * @fn unsigned int module_api(void)
 * @return the required ASKL revision number
 *
 * This function is called first while loading the module, it must return the
 * minimal API revision number that the module requires to function properly.
 * If the server API revision is lower, the module will not be loaded.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API int module_init(uint32_t id, int argc, char **argv);

/**
 * @ingroup module
 * @fn int module_init(uint32_t id, int argc, char **argv)
 * @param id the module identifier, required for some server services
 * @param argc number of string parameters
 * @param argv string parameters
 * @return -1 if an error occured, 0 otherwise
 *
 * This function is called by the server only once, when the module is loaded;
 * the @ref id parameter is the server-side identifier for the module and
 * should be kept if the module needs to call the server API later.
 *
 * In this function, the module has the opportunity to perform all the required
 * initialization work, like setting up database connections, loading data, ...
 *
 * The provided argc and argv parameters work the same as in the main()
 * function from the C standard, and should be processed in the same way.
 *
 * If the initialization fails, this function must return -1 to stop
 * the module loading process, 0 otherwise.
 *
 */

/* -------------------------------------------------------------------------- */

INTERNAL uint32_t module_get_token(void);

/* -------------------------------------------------------------------------- */

ASKL_API void module_input_handler(
    uint16_t socket_id,
    uint16_t ingress_id,
    String *data
);

/**
 * @ingroup module
 * @fn void module_input_handler(uint16_t socket_id, uint16_t ingress_id,
 *                               String *data)
 * @param socket_id connection identifier
 * @param ingress_id a channel identifier
 * @param data incoming data
 *
 * This function is called by the server when data become available on a
 * connection.
 *
 * The incoming data are encapsulated in a @ref String structure, so the
 * module can fetch the data it needs and the server can handle request
 * fragmentation. If the module leaves data in the buffer, they will be kept
 * by the server and subsequent incoming data will be automatically appended.
 *
 * If you don't want the server to keep these remaining data, you can discard
 * them by calling @ref string_flush() on the @ref String.
 *
 * This method can use the server functions @ref server_send_string() or
 * @ref server_send_response() to send a reply.
 *
 */

/* -------------------------------------------------------------------------- */

ASKL_API void module_exit(void);

/**
 * @ingroup module
 * @fn void module_exit(void)
 *
 * This function is called by the server when unloading the module. It gives
 * the opportunity to clean up all the resources allocated by the module, like
 * internal data, database connections...
 *
 */

/* -------------------------------------------------------------------------- */
/* OPTIONAL MODULE CALLBACKS */
/* -------------------------------------------------------------------------- */

ASKL_API void module_event_handler(
    uint16_t id,
    uint16_t ingress_id,
    Module_Event event,
    void *event_data
);

/**
 * @ingroup module
 * @fn void module_event_handler(uint16_t id, uint16_t ingress_id,
 *                               Module_Event event, void *event_data)
 * @param id connection identifier
 * @param ingress_id ingress identifier
 * @param event event code
 * @param event_data pointer to event-specific data
 *
 * This function is called by the server whenever an event occurs on
 * a connection; e.g. the connection has just been established, broken,
 * or a response has been successfully sent.
 *
 * This method can use the server functions @ref server_send_string() or
 * @ref server_send_response() to respond to the received event.
 *
 */

/* -------------------------------------------------------------------------- */
/* Configuration */
/* -------------------------------------------------------------------------- */

INTERNAL int stream_config_init(int argc, char **argv);

/**
 * @ingroup stream
 * @fn int stream_config_init(int argc, char **argv)
 * @param argc the module argument count
 * @param argv the module argument vector
 * @return 0 on success, -1 on error
 *
 * This function parses the Stream module configuration from the module
 * arguments and initializes the internal configuration tables.
 *
 * It must be called once during module initialization, before any routing
 * or socket setup is performed.
 */

INTERNAL int stream_personality(void);

/**
 * @ingroup stream
 * @fn int stream_personality(void)
 * @return a bitmask describing the current Stream role
 *
 * This function returns the Stream module "personality" bitmask.
 * The returned value is a combination of PERSONALITY_MASTER and/or
 * PERSONALITY_WORKER.
 */

INTERNAL int stream_master_streams(void);

/**
 * @ingroup stream
 * @fn int stream_master_streams(void)
 * @return the number of configured master streams, or 0 if none
 *
 * This function returns the number of stream definitions that the module
 * should expose as a MASTER (public listener + worker ingress).
 */

INTERNAL int stream_worker_streams(void);

/**
 * @ingroup stream
 * @fn int stream_worker_streams(void)
 * @return the number of configured worker streams, or 0 if none
 *
 * This function returns the number of stream definitions that the module
 * should run as a WORKER (master endpoint + service endpoint).
 */

INTERNAL const char *stream_config_host(int stream, int target);

/**
 * @ingroup stream
 * @fn const char *stream_config_host(int stream, int target)
 * @param stream the stream index (0..N-1)
 * @param target the target selector (e.g. MASTER_ADDRESS / SERVER_ADDRESS)
 * @return a pointer to a NUL-terminated host string, or NULL on error
 *
 * This function returns the configured host for a given stream and target.
 * The returned pointer remains valid until stream_config_exit() is called.
 */

INTERNAL const char *stream_config_port(int stream, int target);

/**
 * @ingroup stream
 * @fn const char *stream_config_port(int stream, int target)
 * @param stream the stream index (0..N-1)
 * @param target the target selector (e.g. MASTER_ADDRESS / SERVER_ADDRESS)
 * @return a pointer to a NUL-terminated port string, or NULL on error
 *
 * This function returns the configured port for a given stream and target.
 * The returned pointer remains valid until stream_config_exit() is called.
 */

INTERNAL int stream_config_destination(int stream);

/**
 * @ingroup stream
 * @fn int stream_config_destination(int stream)
 * @param stream the stream index (0..N-1)
 * @return an identifier describing the configured destination type, or -1 on error
 *
 * This function returns the destination selector for the given stream.
 * It is used to decide how a stream should be routed and which endpoint
 * should be opened.
 */

INTERNAL const char *stream_config_module_name(int stream);

/**
 * @ingroup stream
 * @fn const char *stream_config_module_name(int stream)
 * @param stream the stream index (0..N-1)
 * @return a pointer to the configured module name, or NULL on error
 *
 * This function returns the configured stream instance name used for logging
 * and identification.
 */

INTERNAL void stream_config_exit(void);

/**
 * @ingroup stream
 * @fn void stream_config_exit(void)
 *
 * This function releases any memory held by the Stream configuration layer.
 */

/* -------------------------------------------------------------------------- */
/* Router */
/* -------------------------------------------------------------------------- */

INTERNAL int stream_router_init(void);

/**
 * @ingroup stream
 * @fn int stream_router_init(void)
 * @return 0 on success, -1 on error
 *
 * This function initializes the routing tables for the Stream module.
 * Routes map ingress identifiers to a route type (public/master/worker/server)
 * and allow the module to find the matching egress socket for a given flow.
 *
 * It must be called after stream_config_init().
 */

INTERNAL int stream_heartbeat(int socket_id);

/**
 * @ingroup stream
 * @fn int stream_heartbeat(int socket_id)
 * @param socket_id the socket identifier to ping
 * @return 0 on success, -1 on error
 *
 * This function sends a minimal out-of-band heartbeat over the given socket.
 * It is typically used by WORKER connections to keep the link alive and detect
 * a dead MASTER.
 */

INTERNAL int stream_get_id(int hint, int personality);

/**
 * @ingroup stream
 * @fn int stream_get_id(int hint, int personality)
 * @param hint an ingress id or socket id used as lookup key (depends on personality)
 * @param personality PERSONALITY_MASTER or PERSONALITY_WORKER
 * @return the stream index (0..N-1) on success, -1 on error
 *
 * This function maps an incoming connection (or ingress identifier) to the
 * configured stream instance index, depending on whether we are acting as a
 * MASTER or a WORKER.
 */

INTERNAL void stream_set_route(int type, uint16_t s0, uint16_t s1);

/**
 * @ingroup stream
 * @fn void stream_set_route(int type, uint16_t s0, uint16_t s1)
 * @param type the route type (e.g. ROUTE_PUBLIC / ROUTE_WORKER / ROUTE_SERVER)
 * @param s0 the first socket id in the route mapping
 * @param s1 the second socket id in the route mapping
 *
 * This function records a route association between two sockets for the given
 * route type. It is used to build "pipes" (public<->worker and master<->server)
 * so that payload can be forwarded in both directions.
 */

INTERNAL int stream_get_route(int ingress);

/**
 * @ingroup stream
 * @fn int stream_get_route(int ingress)
 * @param ingress an ingress identifier
 * @return the route type for this ingress, or -1 if unknown
 *
 * This function resolves an ingress identifier to a route type so the module
 * can decide how to interpret control messages and how to forward payload.
 */

INTERNAL int stream_open_ingress(int stream_id, int type);

/**
 * @ingroup stream
 * @fn int stream_open_ingress(int stream_id, int type)
 * @param stream_id the stream index (0..N-1)
 * @param type the route type to open (e.g. ROUTE_PUBLIC / ROUTE_SERVER)
 * @return the ingress identifier on success, -1 on error
 *
 * This function opens (or allocates) an ingress identifier for the given
 * stream and route type, and registers it for later lookups.
 */

INTERNAL int stream_get_ingress(int stream_id, int type);

/**
 * @ingroup stream
 * @fn int stream_get_ingress(int stream_id, int type)
 * @param stream_id the stream index (0..N-1)
 * @param type the route type
 * @return the ingress identifier on success, -1 on error
 *
 * This function returns the ingress identifier previously opened for this
 * stream and route type.
 */

INTERNAL uint16_t stream_get_egress(uint16_t socket_id, uint16_t ingress_id);

/**
 * @ingroup stream
 * @fn uint16_t stream_get_egress(uint16_t socket_id, uint16_t ingress_id)
 * @param socket_id the socket that received the payload
 * @param ingress_id the ingress identifier of that payload
 * @return the egress socket id to forward to, or 0 if none
 *
 * This function resolves the matching egress socket for a given incoming
 * socket_id, ingress_id combination.
 *
 * It is used to implement bidirectional forwarding inside a pipe.
 */

INTERNAL void stream_router_exit(void);

/**
 * @ingroup stream
 * @fn void stream_router_exit(void)
 *
 * This function releases any resources allocated by stream_router_init().
 */

/* -------------------------------------------------------------------------- */
/* Sockets */
/* -------------------------------------------------------------------------- */

INTERNAL int stream_socket_init(void);

/**
 * @ingroup stream
 * @fn int stream_socket_init(void)
 * @return 0 on success, -1 on error
 *
 * This function initializes the socket-layer structures used by the Stream
 * module (worker pools, pending connection queues, waiting lists, packet
 * queues, link status tables, ...).
 *
 * For MASTER personality, it allocates per-stream queues.
 */

INTERNAL int stream_set_status(uint16_t socket_id, int status);

/**
 * @ingroup stream
 * @fn int stream_set_status(uint16_t socket_id, int status)
 * @param socket_id the socket identifier
 * @param status the new status value
 * @return 0 on success, -1 on error
 *
 * This function sets the internal Stream status for a socket. Status
 * transitions are constrained to prevent invalid state changes.
 *
 * The call is thread-safe.
 */

INTERNAL int stream_get_status(uint16_t socket_id);

/**
 * @ingroup stream
 * @fn int stream_get_status(uint16_t socket_id)
 * @param socket_id the socket identifier
 * @return the current socket status, or 0 on error
 *
 * This function returns the current internal status for the given socket.
 * 
 * The call is thread-safe.
 */

INTERNAL void stream_add_worker(int stream_id, uint16_t worker);

/**
 * @ingroup stream
 * @fn void stream_add_worker(int stream_id, uint16_t worker)
 * @param stream_id the stream index (0..N-1)
 * @param worker the worker socket id
 *
 * This function registers a new worker socket as available for the given
 * master stream.
 * The worker is added to the per-stream worker queue only if its status
 * can be set to STREAM_STATUS_WORK.
 */

INTERNAL uint16_t stream_borrow_worker(int stream_id);

/**
 * @ingroup stream
 * @fn uint16_t stream_borrow_worker(int stream_id)
 * @param stream_id the stream index (0..N-1)
 * @return a worker socket id, or 0 if none is available
 *
 * This function dequeues and returns an available worker for the given stream.
 * If the queue contains stale entries, it skips them until a valid worker is
 * found.
 */

INTERNAL uint16_t stream_release_worker(int stream_id, uint16_t worker);

/**
 * @ingroup stream
 * @fn uint16_t stream_release_worker(int stream_id, uint16_t worker)
 * @param stream_id the stream index (0..N-1)
 * @param worker the worker socket id
 * @return 0 on success, 0 on error (best-effort API)
 *
 * This function re-enqueues a worker into the available pool if it is still
 * in STREAM_STATUS_WORK.
 */

INTERNAL uint16_t stream_enqueue_connection(int stream_id, uint16_t conn);

/**
 * @ingroup stream
 * @fn uint16_t stream_enqueue_connection(int stream_id, uint16_t conn)
 * @param stream_id the stream index (0..N-1)
 * @param conn the connection socket id (worker-side connection to master)
 * @return 0 on success, 0 on error (best-effort API)
 *
 * This function enqueues a newly ready connection into the pending connection
 * queue. It marks the connection as STREAM_STATUS_WAIT before insertion.
 */

INTERNAL uint16_t stream_dequeue_connection(int stream_id);

/**
 * @ingroup stream
 * @fn uint16_t stream_dequeue_connection(int stream_id)
 * @param stream_id the stream index (0..N-1)
 * @return a connection socket id, or 0 if none is available
 *
 * This function dequeues and returns a pending connection from the per-stream
 * connection queue, skipping stale entries until a STREAM_STATUS_WAIT socket
 * is found.
 */

INTERNAL uint16_t stream_enqueue_waiting(int stream_id, uint16_t conn);

/**
 * @ingroup stream
 * @fn uint16_t stream_enqueue_waiting(int stream_id, uint16_t conn)
 * @param stream_id the stream index (0..N-1)
 * @param conn the public-side connection socket id
 * @return 0 on success, 0 on error (best-effort API)
 *
 * This function records a public connection as "waiting for a worker pipe".
 * The master will attempt to open a pipe as soon as a worker becomes ready.
 */

INTERNAL uint16_t stream_dequeue_waiting(int stream_id);

/**
 * @ingroup stream
 * @fn uint16_t stream_dequeue_waiting(int stream_id)
 * @param stream_id the stream index (0..N-1)
 * @return a public connection socket id, or 0 if none is waiting
 *
 * This function dequeues and returns a public connection waiting for a pipe,
 * skipping stale entries until a STREAM_STATUS_CONN socket is found.
 */

INTERNAL String *stream_enqueue_packet(uint16_t socket_id, String *data);

/**
 * @ingroup stream
 * @fn String *stream_enqueue_packet(uint16_t socket_id, String *data)
 * @param socket_id the originating socket id
 * @param data the packet payload
 * @return NULL (packets are duplicated and queued), or NULL on error
 *
 * This function enqueues a copy of the given payload for later forwarding.
 * It is used when public data arrives before a worker pipe exists.
 *
 * The call is thread-safe.
 */

INTERNAL String *stream_dequeue_packet(uint16_t socket_id);

/**
 * @ingroup stream
 * @fn String *stream_dequeue_packet(uint16_t socket_id)
 * @param socket_id the socket id whose packet queue should be read
 * @return a queued String packet, or NULL if none
 *
 * This function dequeues and returns the next buffered packet for the given socket.
 * The caller owns the returned String and must free it.
 *
 * The call is thread-safe.
 */

INTERNAL void stream_drop_packets(uint16_t socket_id);

/**
 * @ingroup stream
 * @fn void stream_drop_packets(uint16_t socket_id)
 * @param socket_id the socket id whose packet queue should be destroyed
 *
 * This function destroys the pending packet queue for a socket and frees all
 * queued String nodes.
 *
 * The call is thread-safe.
 */

INTERNAL void stream_flush_packets(uint16_t socket_id, uint16_t egress);

/**
 * @ingroup stream
 * @fn void stream_flush_packets(uint16_t socket_id, uint16_t egress)
 * @param socket_id the socket id whose queued packets should be flushed
 * @param egress the destination socket id
 *
 * This function forwards all buffered packets associated with socket_id to
 * the egress socket, in FIFO order. It may also close the egress socket if
 * no packet was flushed (best-effort cleanup of broken pipes).
 *
 * The call is thread-safe with respect to the packet queue.
 */

INTERNAL int stream_get_connection(int stream_id);

/**
 * @ingroup stream
 * @fn int stream_get_connection(int stream_id)
 * @param stream_id the stream index (0..N-1)
 * @return 0 on success, -1 on error
 *
 * This function asks an available worker to open a new pipe connection
 * (MASTER -> send MASTER_OP_HIRED to a worker).
 */

INTERNAL int stream_get_pipe(int stream_id, uint16_t socket_id);

/**
 * @ingroup stream
 * @fn int stream_get_pipe(int stream_id, uint16_t socket_id)
 * @param stream_id the stream index (0..N-1)
 * @param socket_id the public connection socket id that needs a pipe
 * @return 0 on success, -1 on error
 *
 * This function attempts to pair a public connection with a ready worker
 * connection and establish a bidirectional pipe. If no worker connection is
 * available yet, it triggers a connection request and enqueues the socket
 * in the waiting list.
 */

INTERNAL void stream_open_pipe(int stream_id);

/**
 * @ingroup stream
 * @fn void stream_open_pipe(int stream_id)
 * @param stream_id the stream index (0..N-1)
 *
 * WORKER-side entry point: this function connects to the master endpoint and
 * to the service endpoint, then records the resulting pipe mapping so that
 * raw payload can be forwarded in both directions.
 */

INTERNAL void stream_socket_exit(void);

/**
 * @ingroup stream
 * @fn void stream_socket_exit(void)
 *
 * This function releases any resources allocated by stream_socket_init()
 * (queues, packet buffers, etc.).
 */

/* -------------------------------------------------------------------------- */

#endif
