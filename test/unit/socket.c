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
#include "../../lib/askl_socket.h"
#include "../../lib/arcane/socket.c"

#ifndef _WIN32
#include <fcntl.h>
#endif

#define HOST     "127.0.0.1"
#define REQUEST  "Test string"
#define REPLY    "OK"
#define ATTEMPTS 200

/* a server running in its own thread: it reports its first failure */
typedef struct _Server {
    Socket *listener;
    unsigned int flags;
    const char *error;
    char request[64];
    ssize_t received;
    uint16_t peer_port;
    char peer_host[NI_MAXHOST];
    uint64_t rx, tx;
} _Server;

static pthread_mutex_t _mx_ready = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t _cd_ready = PTHREAD_COND_INITIALIZER;
static int _ready = 0;

/* -------------------------------------------------------------------------- */

static int _setup(void)
{
    return socket_api_init();
}

/* -------------------------------------------------------------------------- */

static int _nop(UNUSED uint16_t id, UNUSED void *arg)
{
    return 0;
}

/* -------------------------------------------------------------------------- */

static void _teardown(void)
{
    socket_api_exit();
}

/* -------------------------------------------------------------------------- */

static Socket *_listen(unsigned int flags, char *port, size_t len)
{
    /** @brief listen on a free port of the loopback, chosen at random */

    Socket *s = NULL;
    unsigned int i = 0;

    for (i = 0; i < ATTEMPTS; i ++) {
        snprintf(port, len, "%u", 20000 + rand() % 40000);
        if (! (s = socket_open(HOST, port, flags | SOCKET_SERVER)) )
            return NULL;
        if (socket_listen(s) == 0) return s;
        s = socket_close(s);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static ssize_t _read(Socket *s, char *out, size_t len)
{
    /** @brief read, retrying while the socket asks for it */

    ssize_t r = 0;
    unsigned int i = 0;

    for (i = 0; i < ATTEMPTS; i ++) {
        if ( (r = socket_read(s, out, len)) != SOCKET_EAGAIN) return r;
        usleep(10000);
    }

    return r;
}

/* -------------------------------------------------------------------------- */

static ssize_t _write(Socket *s, const char *data, size_t len)
{
    ssize_t r = 0;
    unsigned int i = 0;

    for (i = 0; i < ATTEMPTS; i ++) {
        if ( (r = socket_write(s, data, len)) != SOCKET_EAGAIN) return r;
        usleep(10000);
    }

    return r;
}

/* -------------------------------------------------------------------------- */

static void _signal_ready(void)
{
    pthread_mutex_lock(& _mx_ready);
    _ready = 1;
    pthread_cond_broadcast(& _cd_ready);
    pthread_mutex_unlock(& _mx_ready);
}

/* -------------------------------------------------------------------------- */

static void _wait_ready(void)
{
    pthread_mutex_lock(& _mx_ready);
    while (! _ready) pthread_cond_wait(& _cd_ready, & _mx_ready);
    pthread_mutex_unlock(& _mx_ready);
}

/* -------------------------------------------------------------------------- */

static void *_server(void *arg)
{
    /** @brief accept one connection, read one request, send one reply */

    _Server *srv = arg;
    Socket *client = NULL;
    unsigned int i = 0;

    _signal_ready();

    if (srv->flags & SOCKET_UDP) {
        /* a datagram socket has no connections to accept */
        if (socket_accept(srv->listener)) {
            srv->error = "socket_accept() accepted on a UDP socket";
            return NULL;
        }
        client = srv->listener;
    } else {
        for (i = 0; i < ATTEMPTS && ! client; i ++) {
            if (! (client = socket_accept(srv->listener)) ) usleep(10000);
        }
        if (! client) {
            srv->error = "socket_accept() returned nothing";
            return NULL;
        }
        if (socket_ip(socket_get_id(client), srv->peer_host,
                      sizeof(srv->peer_host), & srv->peer_port) == -1) {
            srv->error = "socket_ip() failed on the accepted socket";
            goto _close;
        }
    }

    srv->received = _read(client, srv->request, sizeof(srv->request));
    if (srv->received < 0) {
        srv->error = "socket_read() failed";
        goto _close;
    }

    if (_write(client, REPLY, sizeof(REPLY)) != sizeof(REPLY)) {
        srv->error = "socket_write() of the reply failed";
        goto _close;
    }

    srv->rx = socket_recvbytes(socket_get_id(client));
    srv->tx = socket_sentbytes(socket_get_id(client));

_close:
    if (client != srv->listener) socket_close(client);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _round_trip(unsigned int flags)
{
    static _Server srv;
    pthread_t thread;
    Socket *client = NULL;
    char port[8], reply[64], host[NI_MAXHOST];
    uint16_t peer = 0, id = 0;
    ssize_t r = 0;

    memset(& srv, 0, sizeof(srv));
    srv.flags = flags;
    ASSERT_NOT_NULL(srv.listener = _listen(flags, port, sizeof(port)));
    _ready = 0;
    ASSERT_EQ_INT(pthread_create(& thread, NULL, _server, & srv), 0);
    _wait_ready();

    ASSERT_NOT_NULL(client = socket_open(HOST, port, flags));
    id = socket_get_id(client);
    ASSERT_EQ_INT(socket_exists(id, _nop, NULL), 0);

    /* a non-blocking connection is completed by the first i/o call */
    r = socket_connect(client);
    ASSERT_TRUE(r == 0 || (r == SOCKET_EAGAIN && ~flags & SOCKET_BIO));

    /* the client socket knows where it is connected to */
    ASSERT_EQ_INT(socket_ip(id, host, sizeof(host), & peer), 0);
    ASSERT_EQ_MEM(host, HOST, sizeof(HOST));
    ASSERT_EQ_UINT(peer, atoi(port));

    r = _write(client, REQUEST, sizeof(REQUEST));
    ASSERT_EQ_INT(r, sizeof(REQUEST));
    ASSERT_EQ_UINT(socket_sentbytes(id), sizeof(REQUEST));

    r = _read(client, reply, sizeof(reply));
    ASSERT_EQ_INT(r, sizeof(REPLY));
    ASSERT_EQ_MEM(reply, REPLY, sizeof(REPLY));
    ASSERT_EQ_UINT(socket_recvbytes(id), sizeof(REPLY));

    pthread_join(thread, NULL);
    if (srv.error) FAIL(srv.error);
    ASSERT_EQ_INT(srv.received, sizeof(REQUEST));
    ASSERT_EQ_MEM(srv.request, REQUEST, sizeof(REQUEST));
    ASSERT_EQ_UINT(srv.rx, sizeof(REQUEST));
    ASSERT_EQ_UINT(srv.tx, sizeof(REPLY));
    if (~flags & SOCKET_UDP) {
        ASSERT_EQ_MEM(srv.peer_host, HOST, sizeof(HOST));
        ASSERT_TRUE(srv.peer_port > 0);
    }

    ASSERT_NULL(socket_close(client));
    ASSERT_EQ_INT(socket_exists(id, _nop, NULL), -1);
    ASSERT_NULL(socket_close(srv.listener));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _tcp_round_trip(void)
{
    return _round_trip(SOCKET_BIO);
}

/* -------------------------------------------------------------------------- */

static int _tcp_round_trip_nonblocking(void)
{
    return _round_trip(0);
}

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_UDP
/* -------------------------------------------------------------------------- */

static int _udp_round_trip(void)
{
    return _round_trip(SOCKET_UDP | SOCKET_BIO);
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

static Socket *_accept(Socket *listener)
{
    Socket *accepted = NULL;
    unsigned int i = 0;

    for (i = 0; i < ATTEMPTS && ! accepted; i ++) {
        if (! (accepted = socket_accept(listener)) ) usleep(10000);
    }

    return accepted;
}

/* -------------------------------------------------------------------------- */

static int _nonblocking_peek_and_close(void)
{
    /* on a non-blocking client: a read before any data asks to retry, a
       peek leaves the data in place, and the peer closing is reported */

    Socket *listener = NULL, *client = NULL, *accepted = NULL;
    char port[8], buffer[64];
    unsigned int i = 0;
    ssize_t r = 0;

    ASSERT_NOT_NULL(listener = _listen(0, port, sizeof(port)));
    ASSERT_NOT_NULL(client = socket_open(HOST, port, 0));
    ASSERT_EQ_INT(socket_connect(client), SOCKET_EAGAIN);
    ASSERT_NOT_NULL(accepted = _accept(listener));

    /* nothing sent yet: the pending connection completes, no data */
    ASSERT_EQ_INT(socket_read(client, buffer, sizeof(buffer)),
                  SOCKET_EAGAIN);
    ASSERT_EQ_INT(socket_connect(client), 0);

    ASSERT_EQ_INT(_write(accepted, REQUEST, sizeof(REQUEST)),
                  sizeof(REQUEST));

    /* peek, then the same bytes are still there for read */
    memset(buffer, 0, sizeof(buffer));
    for (i = 0; i < ATTEMPTS; i ++) {
        r = socket_peek(client, buffer, sizeof(buffer));
        if (r != SOCKET_EAGAIN) break;
        usleep(10000);
    }
    ASSERT_EQ_INT(r, sizeof(REQUEST));
    ASSERT_EQ_MEM(buffer, REQUEST, sizeof(REQUEST));
    memset(buffer, 0, sizeof(buffer));
    ASSERT_EQ_INT(_read(client, buffer, sizeof(buffer)), sizeof(REQUEST));
    ASSERT_EQ_MEM(buffer, REQUEST, sizeof(REQUEST));
    ASSERT_EQ_INT(socket_read(client, buffer, sizeof(buffer)),
                  SOCKET_EAGAIN);

    /* the peer goes away */
    ASSERT_NULL(socket_close(accepted));
    ASSERT_EQ_INT(_read(client, buffer, sizeof(buffer)), SOCKET_ECLOSE);

    ASSERT_NULL(socket_close(client));
    ASSERT_NULL(socket_close(listener));

    return 0;
}

/* -------------------------------------------------------------------------- */
#ifndef _WIN32
/* -------------------------------------------------------------------------- */

static int _accepted_socket_mode(void)
{
    /* a connection accepted on a non-blocking listener must be
       non-blocking too: the server reads and writes it as such */

    Socket *listener = NULL, *client = NULL, *accepted = NULL;
    char port[8];
    int flags = 0;

    ASSERT_NOT_NULL(listener = _listen(0, port, sizeof(port)));
    ASSERT_NOT_NULL(client = socket_open(HOST, port, 0));
    socket_connect(client);
    ASSERT_NOT_NULL(accepted = _accept(listener));

    flags = fcntl(socket_private_interface(listener)->_fd, F_GETFL);
    ASSERT_TRUE(flags & O_NONBLOCK);
    flags = fcntl(socket_private_interface(accepted)->_fd, F_GETFL);
    ASSERT_TRUE(flags & O_NONBLOCK);

    ASSERT_NULL(socket_close(accepted));
    ASSERT_NULL(socket_close(client));
    ASSERT_NULL(socket_close(listener));

    return 0;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

static int _connection_refused(void)
{
    /* nobody listens on the port any more */

    Socket *listener = NULL, *client = NULL;
    char port[8];

    ASSERT_NOT_NULL(listener = _listen(0, port, sizeof(port)));
    ASSERT_NULL(socket_close(listener));

    ASSERT_NOT_NULL(client = socket_open(HOST, port, SOCKET_BIO));
    ASSERT_NE_INT(socket_connect(client), 0);
    ASSERT_NULL(socket_close(client));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _registry(void)
{
    /* identifiers, existence, acquisition and the accessors' guards */

    Socket *s = NULL, *acquired = NULL;
    char host[NI_MAXHOST];
    uint16_t id = 0, port = 0;

    ASSERT_EQ_INT(socket_exists(0, _nop, NULL), -1);
    ASSERT_EQ_INT(socket_exists(SOCKET_MAX, _nop, NULL), -1);
    ASSERT_EQ_INT(socket_exists(1, NULL, NULL), -1);
    ASSERT_NULL(socket_acquire(0));
    ASSERT_EQ_INT(socket_ip(0, host, sizeof(host), & port), -1);
    ASSERT_EQ_UINT(socket_sentbytes(0), (uint64_t) -1);
    ASSERT_EQ_UINT(socket_recvbytes(SOCKET_MAX), (uint64_t) -1);

    /* an unused identifier: no socket, nothing to acquire */
    for (id = 1; id < SOCKET_MAX && socket_exists(id, _nop, NULL) == 0; id ++);
    ASSERT_TRUE(id < SOCKET_MAX);
    ASSERT_NULL(socket_acquire(id));
    ASSERT_EQ_INT(socket_ip(id, host, sizeof(host), & port), SOCKET_EFATAL);

    ASSERT_NOT_NULL(s = socket_open(HOST, "1", 0));
    id = socket_get_id(s);
    ASSERT_TRUE(id > 0 && id < SOCKET_MAX);
    ASSERT_EQ_INT(socket_exists(id, _nop, NULL), 0);
    ASSERT_EQ_INT(socket_option_isset(s, SOCKET_UDP), 0);
    ASSERT_EQ_UINT(socket_sentbytes(id), 0);
    ASSERT_EQ_UINT(socket_recvbytes(id), 0);

    /* the configured address is reported before any connection */
    ASSERT_EQ_INT(socket_ip(id, host, sizeof(host), & port), 0);
    ASSERT_EQ_MEM(host, HOST, sizeof(HOST));
    ASSERT_EQ_UINT(port, 1);

    ASSERT_NOT_NULL(acquired = socket_acquire(id));
    ASSERT_EQ_PTR(acquired, s);
    ASSERT_NULL(socket_release(acquired));

    ASSERT_NULL(socket_close(s));
    ASSERT_EQ_INT(socket_exists(id, _nop, NULL), -1);
    ASSERT_NULL(socket_acquire(id));

    /* a server needs a port, a client does not */
    ASSERT_NULL(socket_open(HOST, NULL, SOCKET_SERVER));
    ASSERT_NULL(socket_open(NULL, NULL, 0));
    ASSERT_NOT_NULL(s = socket_open(HOST, NULL, 0));
    ASSERT_NULL(socket_close(s));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _queue(void)
{
    /* the socket queue is a ring of identifiers, first in first out */

    Socket_Queue *q = NULL;
    unsigned int i = 0, n = 0;

    ASSERT_NOT_NULL(q = socket_queue_alloc());
    ASSERT_EQ_INT(socket_queue_empty(q), 1);
    ASSERT_EQ_UINT(socket_dequeue(q), 0);

    ASSERT_EQ_INT(socket_enqueue(q, 0), -1);
    ASSERT_EQ_INT(socket_enqueue(q, SOCKET_MAX), -1);
    ASSERT_EQ_INT(socket_queue_empty(q), 1);

    for (i = 1; i <= 5; i ++) ASSERT_EQ_INT(socket_enqueue(q, i), 0);
    ASSERT_EQ_INT(socket_queue_empty(q), 0);
    for (i = 1; i <= 5; i ++) ASSERT_EQ_UINT(socket_dequeue(q), i);
    ASSERT_EQ_INT(socket_queue_empty(q), 1);
    ASSERT_EQ_UINT(socket_dequeue(q), 0);

    /* the ring wraps around */
    for (n = 0; n < 3 * SOCKET_MAX; n ++) {
        i = 1 + n % (SOCKET_MAX - 1);
        if (socket_enqueue(q, i) == -1 || socket_dequeue(q) != i) {
            test_fail(__FILE__, __LINE__, "wrap-around broke at %u", n);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_INT(socket_queue_empty(q), 1);

    /* waiting on an empty queue returns after the timeout */
    socket_queue_wait(q, 1000);
    socket_queue_wait(q, 0);
    ASSERT_EQ_INT(socket_queue_empty(q), 1);

    ASSERT_NULL(socket_queue_free(q));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _queue_full(void)
{
    /* the ring holds SOCKET_RING identifiers, the same one any number
       of times: all of them come back in order, and one more is refused
       rather than silently corrupting the queue */

    Socket_Queue *q = NULL;
    unsigned int i = 0;

    ASSERT_NOT_NULL(q = socket_queue_alloc());

    for (i = 0; i < SOCKET_RING; i ++) {
        if (socket_enqueue(q, 1 + i % (SOCKET_MAX - 1)) == -1) {
            test_fail(__FILE__, __LINE__, "refused at %u of %u",
                      i, SOCKET_RING);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_INT(socket_queue_empty(q), 0);

    /* full: the next one must be refused, and nothing must be lost */
    ASSERT_EQ_INT(socket_enqueue(q, 1), -1);

    for (i = 0; i < SOCKET_RING; i ++) {
        if (socket_dequeue(q) != 1 + i % (SOCKET_MAX - 1)) {
            test_fail(__FILE__, __LINE__, "entry %u lost or out of order",
                      i);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_INT(socket_queue_empty(q), 1);
    ASSERT_EQ_UINT(socket_dequeue(q), 0);

    /* and the queue is usable afterwards */
    ASSERT_EQ_INT(socket_enqueue(q, 7), 0);
    ASSERT_EQ_UINT(socket_dequeue(q), 7);

    ASSERT_NULL(socket_queue_free(q));

    return 0;
}

/* -------------------------------------------------------------------------- */

#define PRODUCERS 2
#define CONSUMERS 2
#define PER_PRODUCER 400

typedef struct _Queue_Worker {
    Socket_Queue *queue;
    unsigned int id;
    unsigned int count;
    uint64_t sum;
} _Queue_Worker;

static pthread_mutex_t _mx_queue = PTHREAD_MUTEX_INITIALIZER;
static unsigned int _dequeued = 0;

/* -------------------------------------------------------------------------- */

static void *_producer(void *arg)
{
    _Queue_Worker *w = arg;
    unsigned int i = 0;

    for (i = 1; i <= PER_PRODUCER; i ++) {
        while (socket_enqueue(w->queue, w->id * PER_PRODUCER + i) == -1)
            usleep(100);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static void *_consumer(void *arg)
{
    _Queue_Worker *w = arg;
    uint16_t id = 0;
    unsigned int total = 0;

    while (1) {
        pthread_mutex_lock(& _mx_queue);
        total = _dequeued;
        pthread_mutex_unlock(& _mx_queue);
        if (total >= PRODUCERS * PER_PRODUCER) break;
        if (! (id = socket_dequeue(w->queue)) ) {
            socket_queue_wait(w->queue, 1000);
            continue;
        }
        w->count ++; w->sum += id;
        pthread_mutex_lock(& _mx_queue);
        _dequeued ++;
        pthread_mutex_unlock(& _mx_queue);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _queue_concurrent(void)
{
    /* producers and consumers on one queue: every identifier comes out
       exactly once */

    static _Queue_Worker w[PRODUCERS + CONSUMERS];
    pthread_t threads[PRODUCERS + CONSUMERS];
    Socket_Queue *q = NULL;
    unsigned int i = 0, count = 0;
    uint64_t sum = 0, expected = 0;

    ASSERT_NOT_NULL(q = socket_queue_alloc());
    _dequeued = 0;

    for (i = 0; i < PRODUCERS + CONSUMERS; i ++) {
        memset(& w[i], 0, sizeof(w[i]));
        w[i].queue = q; w[i].id = i;
        ASSERT_EQ_INT(pthread_create(
            & threads[i], NULL, (i < PRODUCERS) ? _producer : _consumer, & w[i]
        ), 0);
    }
    for (i = 0; i < PRODUCERS + CONSUMERS; i ++) {
        pthread_join(threads[i], NULL);
        count += w[i].count; sum += w[i].sum;
    }

    for (i = 0; i < PRODUCERS; i ++)
        expected += (uint64_t) PER_PRODUCER * (i * PER_PRODUCER) +
                    (uint64_t) PER_PRODUCER * (PER_PRODUCER + 1) / 2;
    ASSERT_EQ_UINT(count, PRODUCERS * PER_PRODUCER);
    ASSERT_EQ_UINT(sum, expected);
    ASSERT_EQ_INT(socket_queue_empty(q), 1);

    ASSERT_NULL(socket_queue_free(q));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _new_socket_keeps_fd0(void)
{
    /* a socket opened with SOCKET_NEW has no descriptor yet: closing it,
       or failing to register it, must not close descriptor 0 (D49) */

    int pipefd[2] = { -1, -1 };
    Socket *s = NULL;

    ASSERT_EQ_INT(pipe(pipefd), 0);
    ASSERT_TRUE(dup2(pipefd[0], 0) == 0);
    close(pipefd[0]);

    ASSERT_NOT_NULL(s = socket_open(NULL, NULL, SOCKET_NEW));
    ASSERT_NULL(socket_close(s));

    ASSERT_TRUE(fcntl(0, F_GETFD) != -1);

    close(pipefd[1]); close(0);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _queue_wraps_counters(void)
{
    /* the positions are 32-bit sequence numbers that wrap: a queue whose
       counters are about to cross the signed maximum keeps its order and
       its capacity (D53) */

    Socket_Queue *q = NULL;
    unsigned int base = 0x7fffc000u, i = 0, n = 0;

    ASSERT_NOT_NULL(q = socket_queue_alloc());

    /* rewind the ring to a lap that ends past INT_MAX */
    q->_enqueue = base; q->_dequeue = (int) base;
    for (i = 0; i < SOCKET_RING; i ++) q->_ring[i] = _RING_CELL(base + i, 0);
    ASSERT_EQ_INT(socket_queue_empty(q), 1);

    for (n = 0; n < 4 * SOCKET_RING; n ++) {
        i = 1 + n % (SOCKET_MAX - 1);
        if (socket_enqueue(q, i) == -1 || socket_dequeue(q) != i) {
            test_fail(__FILE__, __LINE__, "order broke at %u", n);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    ASSERT_EQ_INT(socket_queue_empty(q), 1);

    /* a full lap straddling the boundary */
    for (i = 0; i < SOCKET_RING; i ++)
        if (socket_enqueue(q, 1 + i % (SOCKET_MAX - 1)) == -1) break;
    ASSERT_EQ_UINT(i, SOCKET_RING);
    ASSERT_EQ_INT(socket_enqueue(q, 1), -1);
    for (i = 0; i < SOCKET_RING; i ++)
        if (socket_dequeue(q) != 1 + i % (SOCKET_MAX - 1)) break;
    ASSERT_EQ_UINT(i, SOCKET_RING);
    ASSERT_EQ_INT(socket_queue_empty(q), 1);

    ASSERT_NULL(socket_queue_free(q));

    return 0;
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("tcp_round_trip", _tcp_round_trip),
    TEST("tcp_round_trip_nonblocking", _tcp_round_trip_nonblocking),
    #ifdef _ENABLE_UDP
    TEST("udp_round_trip", _udp_round_trip),
    #endif
    TEST("nonblocking_peek_and_close", _nonblocking_peek_and_close),
    #ifndef _WIN32
    TEST("accepted_socket_mode", _accepted_socket_mode),
    #endif
    TEST("connection_refused", _connection_refused),
    #ifndef _WIN32
    TEST("new_socket_keeps_fd0", _new_socket_keeps_fd0),
    #endif
    TEST("registry", _registry),
    TEST("queue", _queue),
    TEST("queue_full", _queue_full),
    TEST("queue_wraps_counters", _queue_wraps_counters),
    TEST("queue_concurrent", _queue_concurrent)
};

TEST_SUITE(test_suite_socket, "socket", _setup, _teardown, _cases);

/* -------------------------------------------------------------------------- */
