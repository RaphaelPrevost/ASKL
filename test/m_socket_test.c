#include "../lib/askl_server.h"
#include "../lib/askl_socket.h"
#include "../lib/arcane/socket.c"

#define TESTPORT 8986

static pthread_mutex_t m0 = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t c0 = PTHREAD_COND_INITIALIZER;
static int init_done = 0;

/* -------------------------------------------------------------------------- */

static void *_server(void *params)
{
    Socket *client = NULL;
    Socket *s = NULL;
    char buffer[SOCKET_BUFFER];
    ssize_t r = 0;

    s = socket_open("127.0.0.1", ""STR(TESTPORT)"", *((unsigned int *) params));

    if (! s) {
        printf("(!) Opening server socket on port "STR(TESTPORT)": FAILURE\n");
        pthread_mutex_lock(& m0);
            init_done = 1;
            pthread_cond_broadcast(& c0);
        pthread_mutex_unlock(& m0);
        pthread_exit(NULL);
    } else printf("(*) Opening server socket on port "STR(TESTPORT)": SUCCESS\n");

    if (socket_listen(s) == SOCKET_EFATAL) {
        printf("(!) Listening: FAILURE\n");
        pthread_mutex_lock(& m0);
            init_done = 1;
            pthread_cond_broadcast(& c0);
        pthread_mutex_unlock(& m0);
        s = socket_close(s);
        pthread_exit(NULL);
    } else printf("(*) Listening: SUCCESS\n");

    pthread_mutex_lock(& m0);
        init_done = 1;
        pthread_cond_broadcast(& c0);
    pthread_mutex_unlock(& m0);

    if (! (client = socket_accept(s)) ) {
        if (socket_option_isset(s, SOCKET_UDP))
            client = s;
        else {
            printf("(!) Accepting: FAILURE\n");
            s = socket_close(s);
            pthread_exit(NULL);
        }
    } else printf("(*) Accepting: SUCCESS\n");

    do r = socket_read(client, buffer, sizeof(buffer));
    while (r == SOCKET_EAGAIN);

    if (r < 0) {
        printf("(!) Reading incoming request: FAILURE\n");
        s = socket_close(s);
        client = socket_close(client);
        pthread_exit(NULL);
    } else printf("(*) Reading incoming request (\"%s\"): SUCCESS\n", buffer);

    if (socket_option_isset(s, SOCKET_UDP)) {
        client = s = socket_close(client);
    }
    s = socket_close(s);

    pthread_exit(NULL);
}

/* -------------------------------------------------------------------------- */

static void *_client(void *params)
{
    Socket *client = NULL;
    char host[NI_MAXHOST];
    uint16_t port = 0;
    ssize_t r = 0;

    if (! (client = socket_open("127.0.0.1", ""STR(TESTPORT)"", *(unsigned *)params)) ) {
        printf("(!) Creating client socket: FAILURE\n");
        pthread_exit(NULL);
    } else printf("(*) Creating client socket: SUCCESS\n");

    if (socket_connect(client) == -1) {
        printf("(!) Connecting to TCP port "STR(TESTPORT)": FAILURE\n");
        client = socket_close(client);
        pthread_exit(NULL);
    } else printf("(*) Connecting to TCP port "STR(TESTPORT)": SUCCESS\n");

    if (socket_ip(socket_get_id(client), host, sizeof(host), & port) == -1) {
        printf("(!) Getting server IP: FAILURE\n");
        client = socket_close(client);
        pthread_exit(NULL);
    } else printf("(*) Getting server IP: \"%s\":\"%i\": SUCCESS\n", host, port);

    do r = socket_write(client,
                        (const char *) "Test string",
                        sizeof("Test string"));
    while (r == SOCKET_EAGAIN);

    if (r < 0) {
        printf("(!) Writing a request to the server: FAILURE\n");
        client = socket_close(client);
        pthread_exit(NULL);
    } else printf("(*) Writing a request to the server: SUCCESS\n");

    client = socket_close(client);

    pthread_exit(NULL);
}

/* -------------------------------------------------------------------------- */

int test_socket(void)
{
    pthread_t server;
    pthread_t client;
    unsigned int params = 0x0;

    socket_api_init();

    /* TCP, blocking */
    printf("(-) TCP\n");
    params = SOCKET_SERVER | SOCKET_BIO;

    init_done = 0;

    if (pthread_create(& server, NULL, _server, & params) == -1) {
        printf("(!) Starting server thread: FAILURE\n");
        exit(EXIT_FAILURE);
    } else printf("(*) Starting server thread: SUCCESS\n");

    pthread_mutex_lock(& m0);
        while (! init_done) pthread_cond_wait(& c0, & m0);
    pthread_mutex_unlock(& m0);

    params = SOCKET_BIO;

    if (pthread_create(& client, NULL, _client, & params) == -1) {
        printf("(!) Starting client thread: FAILURE\n");
        exit(EXIT_FAILURE);
    } else printf("(*) Starting client thread: SUCCESS\n");

    pthread_join(client, NULL);
    pthread_join(server, NULL);

    #ifdef _ENABLE_UDP
    /* UDP, blocking */
    printf("(-) UDP\n");
    params = SOCKET_SERVER | SOCKET_UDP | SOCKET_BIO;

    init_done = 0;

    if (pthread_create(& server, NULL, _server, & params) == -1) {
        printf("(!) Starting server thread: FAILURE\n");
        exit(EXIT_FAILURE);
    } else printf("(*) Starting server thread: SUCCESS\n");

    pthread_mutex_lock(& m0);
        while (! init_done) pthread_cond_wait(& c0, & m0);
    pthread_mutex_unlock(& m0);

    params = SOCKET_UDP | SOCKET_BIO;

    if (pthread_create(& client, NULL, _client, & params) == -1) {
        printf("(!) Starting client thread: FAILURE\n");
        exit(EXIT_FAILURE);
    } else printf("(*) Starting client thread: SUCCESS\n");

    pthread_join(client, NULL);
    pthread_join(server, NULL);
    #endif

    #ifdef _ENABLE_SSL
    /* SSL, blocking */
    printf("(-) SSL\n");
    params = SOCKET_SERVER | SOCKET_SSL | SOCKET_BIO;

    init_done = 0;

    if (pthread_create(& server, NULL, _server, & params) == -1) {
        printf("(!) Starting server thread: FAILURE\n");
        exit(EXIT_FAILURE);
    } else printf("(*) Starting server thread: SUCCESS\n");

    pthread_mutex_lock(& m0);
        while (! init_done) pthread_cond_wait(& c0, & m0);
    pthread_mutex_unlock(& m0);

    params = SOCKET_SSL | SOCKET_BIO;

    if (pthread_create(& client, NULL, _client, & params) == -1) {
        printf("(!) Starting client thread: FAILURE\n");
        exit(EXIT_FAILURE);
    } else printf("(*) Starting client thread: SUCCESS\n");

    pthread_join(client, NULL);
    pthread_join(server, NULL);
    #endif

    socket_api_exit();

    return 0;
}

/* -------------------------------------------------------------------------- */
