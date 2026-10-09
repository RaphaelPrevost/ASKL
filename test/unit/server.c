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

/* -------------------------------------------------------------------------- */
#if defined(_ENABLE_SERVER) && defined(_BUILTIN_MODULE)
/* -------------------------------------------------------------------------- */

#include "../askl_test.h"
#include "../../lib/askl_server.h"
#include "../../lib/askl_socket.h"
#include "../../lib/askl_module.h"
#include "../../lib/arcane/socket.c"

#ifndef _WIN32
    #include <poll.h>
    #include <sys/resource.h>
#endif

/* the builtin module of the test binary: the server finds it in the
   executable through dlsym(), as it finds the one in main.c; each test
   case runs in its own process, so the statics below start fresh */

#define HOST     "127.0.0.1"
#define ATTEMPTS 300
#define EVENTS   64

typedef enum _Mode {
    MODE_SILENT,
    MODE_ECHO_END,
    MODE_ECHO_ACK,
    MODE_RESPONSE
} _Mode;

typedef struct _Event {
    uint16_t socket;
    Module_Event event;
} _Event;

static uint32_t _token = 0;
static _Mode _mode = MODE_SILENT;
static char _port[8], _peer_port[8];
static int _udp = 0;
static int _listening = 0;
static int _client_id = 0;
static int _exited = 0;
static int _shutdowns = 0;

static pthread_mutex_t _mx_log = PTHREAD_MUTEX_INITIALIZER;
static _Event _events[EVENTS];
static unsigned int _event_count = 0;
static char _input[256];
static size_t _input_len = 0;
static unsigned int _inputs = 0;

static void _set_done(int *done);

/* -------------------------------------------------------------------------- */

CALLBACK unsigned int module_api(void)
{
    return 1390;
}

/* -------------------------------------------------------------------------- */

CALLBACK int module_init(uint32_t id, UNUSED int argc, UNUSED char **argv)
{
    int flags = SOCKET_SERVER | (_udp ? SOCKET_UDP : 0);

    _token = id;

    if (_port[0]) {
        if (server_open_managed_socket(id, HOST, _port, flags) == -1)
            return -1;
        _listening = 1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

CALLBACK void module_input_handler(
    uint16_t socket_id,
    UNUSED uint16_t ingress_id,
    String *data
)
{
    Response *response = NULL;

    pthread_mutex_lock(& _mx_log);
    _input_len = (data->len < sizeof(_input)) ? data->len : sizeof(_input);
    memcpy(_input, data->data, _input_len);
    _inputs ++;
    pthread_mutex_unlock(& _mx_log);

    switch (_mode) {
    case MODE_ECHO_END:
        server_send_buffer(_token, socket_id, SERVER_MSG_END,
                           data->data, data->len);
        break;
    case MODE_ECHO_ACK:
        server_send(_token, socket_id, SERVER_MSG_ACK, "%.*s",
                    (int) data->len, data->data);
        break;
    case MODE_RESPONSE:
        if (! (response = server_response_init(SERVER_MSG_END, _token)) )
            break;
        server_response_setheader(response, string_alloc("[", 1));
        server_response_setheader(response,
                                  string_alloc(data->data, data->len));
        server_response_setfooter(response, string_alloc("]", 1));
        server_send_response(socket_id, response);
        break;
    default: break;
    }

    /* a module consumes what it has processed, the rest is kept for the
       next call */
    string_flush(data);
}

/* -------------------------------------------------------------------------- */

CALLBACK void module_event_handler(
    uint16_t socket_id,
    UNUSED uint16_t ingress_id,
    Module_Event event,
    UNUSED void *data
)
{
    pthread_mutex_lock(& _mx_log);
    if (_event_count < EVENTS) {
        _events[_event_count].socket = socket_id;
        _events[_event_count].event = event;
        _event_count ++;
    }
    if (event == MODULE_EVENT_SERVER_SHUTTINGDOWN) _shutdowns ++;
    pthread_mutex_unlock(& _mx_log);
}

/* -------------------------------------------------------------------------- */

CALLBACK void module_exit(void)
{
    _set_done(& _exited);
}

/* -------------------------------------------------------------------------- */

static int _count_events(Module_Event event)
{
    unsigned int i = 0, n = 0;

    pthread_mutex_lock(& _mx_log);
    for (i = 0; i < _event_count; i ++) n += (_events[i].event == event);
    pthread_mutex_unlock(& _mx_log);

    return n;
}

/* -------------------------------------------------------------------------- */

static int _wait_event(Module_Event event, unsigned int count)
{
    /** @brief wait until @p event was delivered @p count times */

    unsigned int i = 0;

    for (i = 0; i < ATTEMPTS; i ++) {
        if (_count_events(event) >= (int) count) return 0;
        usleep(10000);
    }

    return -1;
}

/* -------------------------------------------------------------------------- */

static int _event_before(Module_Event first, Module_Event second)
{
    unsigned int i = 0, a = EVENTS, b = EVENTS;

    pthread_mutex_lock(& _mx_log);
    for (i = 0; i < _event_count; i ++) {
        if (a == EVENTS && _events[i].event == first) a = i;
        if (b == EVENTS && _events[i].event == second) b = i;
    }
    pthread_mutex_unlock(& _mx_log);

    return (a < b);
}

/* -------------------------------------------------------------------------- */

static void _free_port(char *port, size_t len, int udp)
{
    /** @brief find a free loopback port for the module to listen on */

    Socket *s = NULL;
    unsigned int i = 0;

    for (i = 0; i < ATTEMPTS; i ++) {
        snprintf(port, len, "%u", 20000 + rand() % 40000);
        s = socket_open(HOST, port, SOCKET_SERVER | (udp ? SOCKET_UDP : 0));
        if (! s) continue;
        if (socket_listen(s) == 0) {
            socket_close(s);
            return;
        }
        socket_close(s);
    }

    port[0] = 0;
}

/* -------------------------------------------------------------------------- */

static void _set_done(int *done)
{
    pthread_mutex_lock(& _mx_log);
    *done = 1;
    pthread_mutex_unlock(& _mx_log);
}

/* -------------------------------------------------------------------------- */

static int _wait_done(int *done, unsigned int ms)
{
    unsigned int i = 0;
    int value = 0;

    for (i = 0; i < ms; i ++) {
        pthread_mutex_lock(& _mx_log);
        value = *done;
        pthread_mutex_unlock(& _mx_log);
        if (value) return 0;
        usleep(1000);
    }

    return -1;
}

/* -------------------------------------------------------------------------- */

static unsigned int _input_count(void)
{
    unsigned int n = 0;

    pthread_mutex_lock(& _mx_log);
    n = _inputs;
    pthread_mutex_unlock(& _mx_log);

    return n;
}

/* -------------------------------------------------------------------------- */

static int _last_input(const char *expected, size_t len)
{
    int same = 0;

    pthread_mutex_lock(& _mx_log);
    same = (_input_len == len && ! memcmp(_input, expected, len));
    pthread_mutex_unlock(& _mx_log);

    return same;
}

/* -------------------------------------------------------------------------- */

static void *_exit_thread(void *arg)
{
    server_exit();
    _set_done(arg);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _exit_within(unsigned int ms)
{
    /** @brief server_exit() must return within @p ms, or it is stuck */

    static int done = 0;
    pthread_t thread;

    done = 0;
    if (pthread_create(& thread, NULL, _exit_thread, & done)) return -1;
    if (_wait_done(& done, ms) == -1) return -1;
    pthread_join(thread, NULL);

    return 0;
}

/* -------------------------------------------------------------------------- */

static ssize_t _recv(Socket *s, char *out, size_t len)
{
    ssize_t r = 0;
    unsigned int i = 0;

    for (i = 0; i < ATTEMPTS; i ++) {
        if ( (r = socket_read(s, out, len)) != SOCKET_EAGAIN) return r;
        usleep(10000);
    }

    return r;
}

/* -------------------------------------------------------------------------- */

static ssize_t _send(Socket *s, const char *data, size_t len)
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

static SOCKET _raw_listen(char *port, size_t len)
{
    /** @brief a listener the hooks never see, on a port the kernel picks */

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    SOCKET fd = INVALID_SOCKET;

    memset(& addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
        return INVALID_SOCKET;

    if (bind(fd, (struct sockaddr *) & addr, addrlen) == -1 ||
        listen(fd, 16) == -1 ||
        getsockname(fd, (struct sockaddr *) & addr, & addrlen) == -1) {
        closesocket(fd);
        return INVALID_SOCKET;
    }

    snprintf(port, len, "%u", ntohs(addr.sin_port));

    return fd;
}

/* -------------------------------------------------------------------------- */

static int _raw_wait(SOCKET fd, int ms)
{
    /** @brief wait for one descriptor to become readable: poll(2) where it
        exists, because the flood case watches descriptors past FD_SETSIZE
        and FD_SET() would write past its set, and select() on Windows,
        which the library uses there too and where that case is skipped */

    #ifndef _WIN32
    struct pollfd p = { 0, POLLIN, 0 };

    p.fd = fd;

    return poll(& p, 1, ms);
    #else
    struct timeval tv;
    fd_set r;

    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    FD_ZERO(& r); FD_SET(fd, & r);

    /* Winsock ignores the first argument, and a SOCKET does not fit in it */
    return select(0, & r, NULL, NULL, & tv);
    #endif
}

/* -------------------------------------------------------------------------- */

static SOCKET _raw_accept(SOCKET listener)
{
    if (_raw_wait(listener, ATTEMPTS * 10) < 1) return INVALID_SOCKET;

    return accept(listener, NULL, NULL);
}

/* -------------------------------------------------------------------------- */

static ssize_t _raw_read(SOCKET fd, char *out, size_t len)
{
    if (_raw_wait(fd, ATTEMPTS * 10) < 1) return -1;

    return recv(fd, out, len, 0);
}

/* -------------------------------------------------------------------------- */

static SOCKET _raw_connect(const char *port)
{
    struct sockaddr_in addr;
    SOCKET fd = INVALID_SOCKET;

    memset(& addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(atoi(port));

    if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
        return INVALID_SOCKET;

    if (connect(fd, (struct sockaddr *) & addr, sizeof(addr)) == -1) {
        closesocket(fd);
        return INVALID_SOCKET;
    }

    return fd;
}

/* -------------------------------------------------------------------------- */

static unsigned int _raw_closed(
    const SOCKET *fd,
    unsigned int n,
    int *first
)
{
    /** @brief count the connections the peer closed, note the first one */

    unsigned int i = 0, closed = 0;
    char c = 0;

    if (first) *first = -1;

    for (i = 0; i < n; i ++) {
        if (_raw_wait(fd[i], 0) < 1) continue;
        if (recv(fd[i], & c, 1, MSG_PEEK) > 0) continue;
        if (first && *first == -1) *first = i;
        closed ++;
    }

    return closed;
}

/* -------------------------------------------------------------------------- */

static int _raw_talk(SOCKET fd)
{
    /** @brief one exchange with the echo module */

    char buffer[64];

    if (send(fd, "ping", 4, 0) != 4) return -1;
    if (_raw_read(fd, buffer, sizeof(buffer)) != 4) return -1;

    return memcmp(buffer, "ping", 4) ? -1 : 0;
}

/* -------------------------------------------------------------------------- */

static SOCKET _raw_client(const char *port, unsigned int *attempts)
{
    /** @brief connect and get served, again when the server refuses */

    SOCKET fd = INVALID_SOCKET;

    for (*attempts = 1; *attempts <= 10; (*attempts) ++) {
        if ( (fd = _raw_connect(port)) == INVALID_SOCKET)
            return INVALID_SOCKET;
        if (_raw_talk(fd) == 0) return fd;
        closesocket(fd);
        usleep(50000);
    }

    return -1;
}

/* -------------------------------------------------------------------------- */

static int _raw_udp_port(char *port, size_t len)
{
    /** @brief a free UDP port, found without the socket layer */

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    SOCKET fd = INVALID_SOCKET;

    memset(& addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if ( (fd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) return -1;
    if (bind(fd, (struct sockaddr *) & addr, addrlen) == -1 ||
        getsockname(fd, (struct sockaddr *) & addr, & addrlen) == -1) {
        closesocket(fd);
        return -1;
    }
    snprintf(port, len, "%u", ntohs(addr.sin_port));
    closesocket(fd);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _raw_udp_echo(const char *port)
{
    /** @brief one datagram to the module and its echo back */

    struct sockaddr_in addr;
    char buffer[64];
    SOCKET fd = INVALID_SOCKET;
    int ret = -1;

    memset(& addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(atoi(port));

    if ( (fd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) return -1;
    if (sendto(fd, "ping", 4, 0, (struct sockaddr *) & addr, sizeof(addr))
        == 4) {
        if (_raw_wait(fd, ATTEMPTS * 10) == 1 && recv(fd, buffer, 4, 0) == 4)
            ret = memcmp(buffer, "ping", 4) ? -1 : 0;
    }
    closesocket(fd);

    return ret;
}

/* -------------------------------------------------------------------------- */

static Socket *_connect(const char *port, int udp)
{
    Socket *s = NULL;
    int r = 0;

    if (! (s = socket_open(HOST, port, udp ? SOCKET_UDP : 0)) ) return NULL;
    r = socket_connect(s);
    if (r != 0 && r != SOCKET_EAGAIN) return socket_close(s);

    return s;
}

/* -------------------------------------------------------------------------- */

static int _start(_Mode mode, int udp)
{
    /** @brief start the server with the builtin module listening, and
        make sure the workers are past their start gate before the case
        goes on (see exit_right_after_init) */

    Socket *c = NULL;

    working_directory = "test/fixtures/server";
    _mode = mode; _udp = udp;

    ASSERT_EQ_INT(socket_api_init(), 0);
    _free_port(_port, sizeof(_port), udp);
    ASSERT_TRUE(_port[0]);
    socket_api_exit();

    ASSERT_EQ_INT(server_init(), 0);
    ASSERT_TRUE(_token > 0);
    ASSERT_EQ_INT(_listening, 1);

    if (! udp) {
        ASSERT_NOT_NULL(c = _connect(_port, 0));
        ASSERT_EQ_INT(_wait_event(MODULE_EVENT_INCOMING_CONNECTION, 1), 0);
        socket_close(c);
        ASSERT_EQ_INT(_wait_event(MODULE_EVENT_SOCKET_DISCONNECTED, 1), 0);
        pthread_mutex_lock(& _mx_log);
        _event_count = 0;
        pthread_mutex_unlock(& _mx_log);
    } else usleep(50000);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _stop(void)
{
    if (_exit_within(5000) == -1) FAIL("server_exit() did not return");
    ASSERT_EQ_INT(_wait_done(& _exited, 1), 0);
    ASSERT_TRUE(_shutdowns >= 1);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _lifecycle(void)
{
    /* the server starts, finds and starts the builtin module, and stops
       it cleanly: the module sees its token, then the shutdown event
       and its exit call */

    if (_start(MODE_SILENT, 0) == -1) return -1;

    ASSERT_EQ_UINT(_input_count(), 0);
    ASSERT_EQ_INT(_count_events(MODULE_EVENT_SERVER_SHUTTINGDOWN), 0);
    ASSERT_EQ_INT(_wait_done(& _exited, 1), -1);

    return _stop();
}

/* -------------------------------------------------------------------------- */

static void *_init_exit_thread(void *arg)
{
    /** @brief start and stop back to back, the window the race needs */

    if (server_init() == 0) server_exit();
    _set_done(arg);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _reinit(void)
{
    /* the server can be started again in the same process after it has
       exited, with its workers gated again (D60) */

    Socket *c = NULL;
    char buffer[64];

    if (_start(MODE_ECHO_END, 0) == -1) return -1;
    if (_stop() == -1) return -1;

    _exited = 0; _listening = 0;
    if (_start(MODE_ECHO_END, 0) == -1) return -1;
    ASSERT_NOT_NULL(c = _connect(_port, 0));
    ASSERT_EQ_INT(_send(c, "ping", 4), 4);
    ASSERT_EQ_INT(_recv(c, buffer, sizeof(buffer)), 4);
    ASSERT_EQ_MEM(buffer, "ping", 4);
    socket_close(c);

    return _stop();
}

/* -------------------------------------------------------------------------- */

static int _exit_right_after_init(void)
{
    /* stopping the server before its workers have passed their start
       gate must still stop it */

    static int done = 0;
    pthread_t thread;

    working_directory = "test/fixtures/server";
    _mode = MODE_SILENT;
    ASSERT_EQ_INT(socket_api_init(), 0);
    _free_port(_port, sizeof(_port), 0);
    ASSERT_TRUE(_port[0]);
    socket_api_exit();

    ASSERT_EQ_INT(pthread_create(& thread, NULL, _init_exit_thread, & done),
                  0);
    if (_wait_done(& done, 3000) == -1)
        FAIL("server_exit() hangs: the workers wait at the gate");
    pthread_join(thread, NULL);
    ASSERT_EQ_INT(_wait_done(& _exited, 1), 0);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _echo_and_close(void)
{
    /* a request reaches the module, the reply comes back, and the
       connection is closed after it as the flag says */

    Socket *c = NULL;
    char buffer[64];

    if (_start(MODE_ECHO_END, 0) == -1) return -1;

    ASSERT_NOT_NULL(c = _connect(_port, 0));
    ASSERT_EQ_INT(_send(c, "ping", 4), 4);
    ASSERT_EQ_INT(_recv(c, buffer, sizeof(buffer)), 4);
    ASSERT_EQ_MEM(buffer, "ping", 4);
    ASSERT_EQ_INT(_recv(c, buffer, sizeof(buffer)), SOCKET_ECLOSE);
    socket_close(c);

    ASSERT_EQ_INT(_wait_event(MODULE_EVENT_SOCKET_DISCONNECTED, 1), 0);
    ASSERT_EQ_INT(_count_events(MODULE_EVENT_INCOMING_CONNECTION), 1);
    ASSERT_TRUE(_event_before(MODULE_EVENT_INCOMING_CONNECTION,
                              MODULE_EVENT_SOCKET_DISCONNECTED));
    ASSERT_EQ_UINT(_input_count(), 1);
    ASSERT_TRUE(_last_input("ping", 4));

    return _stop();
}

/* -------------------------------------------------------------------------- */

static int _send_with_ack(void)
{
    /* a formatted reply flagged for acknowledgment leaves the connection
       open and notifies the module once it is on the wire */

    Socket *c = NULL;
    char buffer[64];

    if (_start(MODE_ECHO_ACK, 0) == -1) return -1;

    ASSERT_NOT_NULL(c = _connect(_port, 0));
    ASSERT_EQ_INT(_send(c, "first", 5), 5);
    ASSERT_EQ_INT(_recv(c, buffer, sizeof(buffer)), 5);
    ASSERT_EQ_MEM(buffer, "first", 5);
    ASSERT_EQ_INT(_wait_event(MODULE_EVENT_REQUEST_TRANSMITTED, 1), 0);

    /* still open: a second exchange on the same connection */
    ASSERT_EQ_INT(_send(c, "second", 6), 6);
    ASSERT_EQ_INT(_recv(c, buffer, sizeof(buffer)), 6);
    ASSERT_EQ_MEM(buffer, "second", 6);
    ASSERT_EQ_INT(_wait_event(MODULE_EVENT_REQUEST_TRANSMITTED, 2), 0);
    ASSERT_EQ_UINT(_input_count(), 2);

    /* the client hangs up */
    socket_close(c);
    ASSERT_EQ_INT(_wait_event(MODULE_EVENT_SOCKET_DISCONNECTED, 1), 0);

    return _stop();
}

/* -------------------------------------------------------------------------- */

static int _response_header_footer(void)
{
    /* a response assembles its header and footer parts in order */

    Socket *c = NULL;
    char buffer[64];

    if (_start(MODE_RESPONSE, 0) == -1) return -1;

    ASSERT_NOT_NULL(c = _connect(_port, 0));
    ASSERT_EQ_INT(_send(c, "ping", 4), 4);
    ASSERT_EQ_INT(_recv(c, buffer, sizeof(buffer)), 6);
    ASSERT_EQ_MEM(buffer, "[ping]", 6);
    ASSERT_EQ_INT(_recv(c, buffer, sizeof(buffer)), SOCKET_ECLOSE);
    socket_close(c);

    return _stop();
}

/* -------------------------------------------------------------------------- */

static int _several_clients(void)
{
    /* the workers serve concurrent connections independently */

    Socket *c[8];
    char buffer[64], message[16];
    unsigned int i = 0;
    int len = 0;

    if (_start(MODE_ECHO_END, 0) == -1) return -1;

    for (i = 0; i < 8; i ++) ASSERT_NOT_NULL(c[i] = _connect(_port, 0));
    for (i = 0; i < 8; i ++) {
        len = snprintf(message, sizeof(message), "client %u", i);
        ASSERT_EQ_INT(_send(c[i], message, len), len);
    }
    for (i = 0; i < 8; i ++) {
        len = snprintf(message, sizeof(message), "client %u", i);
        if (_recv(c[i], buffer, sizeof(buffer)) != len ||
            memcmp(buffer, message, len)) {
            test_fail(__FILE__, __LINE__, "client %u: wrong echo", i);
            break;
        }
    }
    ASSERT_EQ_INT(test_status(), 0);
    for (i = 0; i < 8; i ++) socket_close(c[i]);

    ASSERT_EQ_INT(_wait_event(MODULE_EVENT_SOCKET_DISCONNECTED, 8), 0);
    ASSERT_EQ_INT(_count_events(MODULE_EVENT_INCOMING_CONNECTION), 8);
    ASSERT_EQ_UINT(_input_count(), 8);

    return _stop();
}

/* -------------------------------------------------------------------------- */

static int _outgoing_connection(void)
{
    /* the module opens a managed connection to a peer: the server
       completes it, reports it, and sends through it on request */

    SOCKET listener = INVALID_SOCKET, peer = INVALID_SOCKET;
    char buffer[64];

    if (_start(MODE_SILENT, 0) == -1) return -1;

    /* the peer listens outside of the hooks, so that the server cannot
       race this test for the connection; then the module asks for it */
    listener = _raw_listen(_peer_port, sizeof(_peer_port));
    ASSERT_TRUE(listener != INVALID_SOCKET);
    _client_id = server_open_managed_socket(_token, HOST, _peer_port, 0);
    ASSERT_TRUE(_client_id > 0);
    ASSERT_TRUE( (peer = _raw_accept(listener)) != INVALID_SOCKET);
    ASSERT_EQ_INT(_wait_event(MODULE_EVENT_OUTGOING_CONNECTION, 1), 0);

    /* the module side sends, the peer receives */
    ASSERT_EQ_INT(server_send_buffer(_token, _client_id, 0, "hello", 5), 0);
    ASSERT_EQ_INT(_raw_read(peer, buffer, sizeof(buffer)), 5);
    ASSERT_EQ_MEM(buffer, "hello", 5);

    /* the server closes it on request */
    server_close_managed_socket(_token, _client_id);
    ASSERT_EQ_INT(_raw_read(peer, buffer, sizeof(buffer)), 0);
    ASSERT_EQ_INT(_wait_event(MODULE_EVENT_SOCKET_DISCONNECTED, 1), 0);

    closesocket(peer); closesocket(listener);

    return _stop();
}

static int _flood(void)
{
    /* silent connections fill the identifier table; nothing is pruned
       within the second they arrive, then each refused accept prunes the
       two oldest connections never served and the served one idle the
       longest, so a client that talks gets in on its next attempt */

    static SOCKET fd[SOCKET_MAX];
    static char udp_port[8];
    #ifndef _WIN32
    struct rlimit limit;
    #endif
    struct timespec ts;
    char buffer[64];
    unsigned int n = 0, i = 0, refused = 0, pruned = 0, attempt = 0;
    SOCKET a = INVALID_SOCKET, b = INVALID_SOCKET, c = INVALID_SOCKET;
    int first = -1;

    /* select() closes every socket whose descriptor reaches FD_SETSIZE, so
       on a build without poll(2) the descriptors run out long before the
       identifier table fills and the pruning cannot be reached */
    #if ! defined(_USE_BIG_FDS) || ! defined(HAS_POLL) || defined(WIN32)
    SKIP("select() drops descriptors above FD_SETSIZE before the ids run out");
    #endif

    #ifndef _WIN32
    if (getrlimit(RLIMIT_NOFILE, & limit) == -1) SKIP("getrlimit() failed");
    if (limit.rlim_max < 2 * SOCKET_MAX + 256) SKIP("descriptor limit too low");
    limit.rlim_cur = 2 * SOCKET_MAX + 256;
    if (setrlimit(RLIMIT_NOFILE, & limit) == -1) SKIP("setrlimit() failed");
    #endif

    if (_start(MODE_ECHO_ACK, 0) == -1) return -1;

    /* a UDP listener, older than the flood: bound, so never a candidate
       for pruning however long it stays silent (D52) */
    ASSERT_EQ_INT(_raw_udp_port(udp_port, sizeof(udp_port)), 0);
    ASSERT_EQ_INT(server_open_managed_socket(
        _token, HOST, udp_port, SOCKET_SERVER | SOCKET_UDP
    ), 0);
    ASSERT_EQ_INT(_raw_udp_echo(udp_port), 0);

    /* the flood, at the start of a second so that it is stamped as one age,
       and paced so that the listen backlog never overflows */
    monotonic_timer(& ts);
    usleep((1000000000 - ts.tv_nsec) / 1000 + 10000);
    for (n = 0; n < SOCKET_MAX; n ++) {
        if ( (fd[n] = _raw_connect(_port)) == INVALID_SOCKET) break;
        if (n % 256 == 255) usleep(20000);
    }
    ASSERT_EQ_UINT(n, SOCKET_MAX);

    /* the table is full once the server has accepted the flood: a few of
       the last connections were refused, nothing else was touched */
    for (i = 0; i < ATTEMPTS; i ++) {
        usleep(10000);
        if ( (pruned = _raw_closed(fd, n, & first)) && pruned == refused) break;
        refused = pruned;
    }
    printf("flood: %u connections, %u refused from #%i after %u ms\n",
           n, refused, first, i * 10);
    ASSERT_TRUE(refused > 0 && refused < 64);
    ASSERT_TRUE(first > 0);

    /* past the grace, clients that talk get in: the first attempt is
       refused and prunes two holders, the next one is served, and the
       slot left over serves the following client at once */
    usleep(1100000);
    a = _raw_client(_port, & attempt);
    printf("flood: a served on attempt %u\n", attempt);
    ASSERT_TRUE(a != INVALID_SOCKET);
    ASSERT_EQ_UINT(attempt, 2);
    b = _raw_client(_port, & attempt);
    printf("flood: b served on attempt %u\n", attempt);
    ASSERT_TRUE(b != INVALID_SOCKET);
    ASSERT_EQ_UINT(attempt, 1);

    /* a second later a is the served connection idle the longest: the
       next refusal takes it along with two holders, while b, served in
       the current second, is left alone */
    monotonic_timer(& ts);
    usleep((1000000000 - ts.tv_nsec) / 1000 + 10000);
    ASSERT_EQ_INT(_raw_talk(b), 0);
    c = _raw_client(_port, & attempt);
    printf("flood: c served on attempt %u\n", attempt);
    ASSERT_TRUE(c != INVALID_SOCKET);
    ASSERT_EQ_UINT(attempt, 2);
    ASSERT_TRUE(_raw_read(a, buffer, sizeof(buffer)) <= 0);
    ASSERT_EQ_INT(_raw_talk(b), 0);
    pruned = _raw_closed(fd, n, & first) - refused;
    printf("flood: %u holders pruned\n", pruned);
    ASSERT_EQ_UINT(pruned, 4);
    ASSERT_EQ_INT(_raw_udp_echo(udp_port), 0);

    closesocket(a); closesocket(b); closesocket(c);
    for (i = 0; i < n; i ++) closesocket(fd[i]);

    return _stop();
}

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_UDP
/* -------------------------------------------------------------------------- */

static int _udp_echo(void)
{
    /* a datagram reaches the module and the reply goes back to its
       sender */

    Socket *c = NULL;
    char buffer[64];

    if (_start(MODE_ECHO_ACK, 1) == -1) return -1;

    ASSERT_NOT_NULL(c = _connect(_port, 1));
    ASSERT_EQ_INT(_send(c, "ping", 4), 4);
    ASSERT_EQ_INT(_recv(c, buffer, sizeof(buffer)), 4);
    ASSERT_EQ_MEM(buffer, "ping", 4);
    ASSERT_EQ_UINT(_input_count(), 1);
    socket_close(c);

    return _stop();
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

static int _guards(void)
{
    /* the entry points refuse what they cannot do */

    Response *r = NULL;

    if (_start(MODE_SILENT, 0) == -1) return -1;

    ASSERT_EQ_INT(server_open_managed_socket(0, HOST, _port, 0), -1);
    ASSERT_EQ_INT(server_open_managed_socket(_token, HOST, NULL, 0), -1);
    ASSERT_EQ_INT(server_send_buffer(_token, 0, 0, "x", 1), -1);
    ASSERT_EQ_INT(server_send(_token, 0, 0, "%s", "x"), -1);
    ASSERT_NULL(server_response_init(0, 0));
    ASSERT_NOT_NULL(r = server_response_init(0, _token));
    ASSERT_EQ_INT(server_response_setheader(r, NULL), -1);
    ASSERT_EQ_INT(server_response_setdelay(r, 3601), -1);
    ASSERT_EQ_INT(server_response_setdelay(r, 1), 0);
    ASSERT_NULL(server_send_response(0, r));
    ASSERT_NULL(server_response_free(NULL));

    return _stop();
}

/* -------------------------------------------------------------------------- */

static int _module_options(void)
{
    /* the option helpers read the vector the configuration builds */

    static char *argv[] = {
        "host", "127.0.0.1", "listen", "8000", "listen[1]", "8001",
        "verbose", "1", "quiet", "0"
    };
    int argc = 10;

    ASSERT_EQ_MEM(module_getopt("host", argc, argv), "127.0.0.1", 10);
    ASSERT_NULL(module_getopt("missing", argc, argv));
    ASSERT_EQ_MEM(module_getarrayopt("listen", 0, argc, argv), "8000", 5);
    ASSERT_EQ_MEM(module_getarrayopt("listen", 1, argc, argv), "8001", 5);
    ASSERT_NULL(module_getarrayopt("listen", 2, argc, argv));
    ASSERT_EQ_INT(module_getboolopt("verbose", argc, argv), 1);
    ASSERT_EQ_INT(module_getboolopt("quiet", argc, argv), 0);
    ASSERT_EQ_INT(module_getboolopt("missing", argc, argv), 0);
    ASSERT_NULL(module_getopt("host", 0, argv));

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _module_boolopt_words(void)
{
    /* the documented spellings of a boolean option, as concrete.xml
       uses them */

    static char *argv[] = {
        "a", "on", "b", "true", "c", "enabled",
        "d", "off", "e", "false", "f", "disabled"
    };
    int argc = 12;

    ASSERT_EQ_INT(module_getboolopt("a", argc, argv), 1);
    ASSERT_EQ_INT(module_getboolopt("b", argc, argv), 1);
    ASSERT_EQ_INT(module_getboolopt("c", argc, argv), 1);
    ASSERT_EQ_INT(module_getboolopt("d", argc, argv), 0);
    ASSERT_EQ_INT(module_getboolopt("e", argc, argv), 0);
    ASSERT_EQ_INT(module_getboolopt("f", argc, argv), 0);

    return 0;
}

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("lifecycle", _lifecycle),
    TEST("exit_right_after_init", _exit_right_after_init),
    TEST("reinit", _reinit),
    TEST("echo_and_close", _echo_and_close),
    TEST("send_with_ack", _send_with_ack),
    TEST("response_header_footer", _response_header_footer),
    TEST_RACY("several_clients", _several_clients,
        "D47: _socket_reg() reads the id counter outside its lock"),
    TEST("outgoing_connection", _outgoing_connection),
    TEST_RACY("flood", _flood,
        "_socket_prune() reads _state and _tx without the socket lock"),
    #ifdef _ENABLE_UDP
    TEST("udp_echo", _udp_echo),
    #endif
    TEST("guards", _guards),
    TEST("module_options", _module_options),
    TEST_TODO("module_boolopt_words", _module_boolopt_words,
        "module_getboolopt() only accepts \"1\", not the documented words")
};

TEST_SUITE(test_suite_server, "server", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
