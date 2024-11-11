/*******************************************************************************
 *  Concrete Server                                                            *
 *  Copyright (c) 2005-2024 Raphael Prevost <raph@el.bzh>                      *
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

#include "m_server.h"

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_SERVER
/* -------------------------------------------------------------------------- */

/* server execution control */
static pthread_mutex_t start_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t start = PTHREAD_COND_INITIALIZER;
static int server_running = 0;

/* worker threads */
static pthread_t *_thread;
static unsigned int _concurrency = 0;

#ifdef _ENABLE_UDP
/* UDP sockets registry */
static m_hashtable *_UDP = NULL;
#endif

#define _POLL_MAX      1024

/* server socket queues */
static m_socket_queue *_blocking = NULL;
static m_socket_queue *_readable = NULL;
static m_socket_queue *_writable = NULL;
static m_socket_queue *_incoming = NULL;

#define server_enqueue_blocking(s) \
do { socket_queue_add(_blocking, SOCKET_ID((s))); } while (0)
#define server_enqueue_readable(s) \
do { socket_queue_add(_readable, SOCKET_ID((s))); } while (0)
#define server_enqueue_writable(s) \
do { socket_queue_add(_writable, SOCKET_ID((s))); } while (0)
#define server_enqueue_listener(s) \
do { socket_queue_add(_incoming, SOCKET_ID((s))); } while (0)

#define server_dequeue_blocking() (socket_queue_get(_blocking))
#define server_dequeue_readable() (socket_queue_get(_readable))
#define server_dequeue_writable() (socket_queue_get(_writable))
#define server_dequeue_listener() (socket_queue_get(_incoming))

/* poll locks */
static pthread_mutex_t _server_blocking = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _server_incoming = PTHREAD_MUTEX_INITIALIZER;

/* sockets work queues */
static m_queue *_work[SOCKET_MAX];
#define SOCKET_IDLE(s) (queue_empty(_work[SOCKET_ID(s)]))

/* sockets fragmentation cache */
static m_string *_frag[SOCKET_MAX];

#ifdef _ENABLE_PRIVILEGE_SEPARATION
#define _OP_LEN 4
static pthread_mutex_t _priv_lock = PTHREAD_MUTEX_INITIALIZER;
static int _priv_com = -1;
#endif

/* -------------------------------------------------------------------------- */
/* Server internal data structures */
/* -------------------------------------------------------------------------- */

public m_reply *server_reply_init(uint16_t flags, uint32_t token)
{
    m_reply *new = NULL;

    if (! token || (token >> _SOCKET_RSS) > PLUGIN_MAX) {
        debug("server_reply_init(): bad parameters.\n");
        return NULL;
    }

    if (! (new = malloc(sizeof(*new))) ) {
        perror(ERR(server_reply_init, malloc));
        return NULL;
    }

    /* initialize the struct */
    new->timer = 0; new->delay = 0;
    new->op = flags;
    new->token = token;
    new->header = new->footer = NULL;
    #ifdef _ENABLE_FILE
    new->file = NULL;
    new->off = new->len = 0;
    #endif

    return new;
}

/* -------------------------------------------------------------------------- */

public m_reply *server_reply_free(m_reply *r)
{
    if (! r) return NULL;

    #ifdef _ENABLE_FILE
    if (r->file) fs_closefile(r->file);
    #endif

    string_free(r->header);
    string_free(r->footer);

    free(r);

    return NULL;
}

/* -------------------------------------------------------------------------- */

public int server_reply_setheader(m_reply *reply, m_string *data)
{
    if (! reply || ! data) {
        debug("server_reply_setheader(): bad parameters.\n");
        return -1;
    }

    if (reply->header) {
        string_cat(reply->header, data);
        string_free(data);
    } else reply->header = data;

    return 0;
}

/* -------------------------------------------------------------------------- */

public int server_reply_setfooter(m_reply *reply, m_string *data)
{
    if (! reply || ! data) {
        debug("server_reply_setfooter(): bad parameters.\n");
        return -1;
    }

    if (reply->footer) {
        string_cat(reply->footer, data);
        string_free(data);
    } else reply->footer = data;

    return 0;
}

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_FILE
/* -------------------------------------------------------------------------- */

public int server_reply_setfile(m_reply *reply, m_file *f, off_t o, size_t len)
{
    if (! reply || ! f) {
        debug("server_reply_setfile(): bad parameters.\n");
        return -1;
    }

    if (reply->op & SERVER_MSG_OOB) {
        debug("server_reply_setfile(): cannot send file out of band.\n");
        return -1;
    }

    if (reply->file) fs_closefile(reply->file);
    reply->file = f;
    reply->off = o;
    reply->len = (len) ? len : f->len;

    return 0;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public int server_reply_setdelay(m_reply *reply, unsigned int nsec)
{
    struct timespec ts;

    if (! reply || ! nsec) {
        debug("server_reply_setdelay(): bad parameters.\n");
        return -1;
    }

    if (nsec > 3600) {
        debug("server_reply_setdelay(): delay cannot exceed one hour.");
        return -1;
    }

    monotonic_timer(& ts);

    reply->timer = ts.tv_sec;
    reply->delay = nsec;

    return 0;
}

/* -------------------------------------------------------------------------- */

public m_reply *server_send_reply(uint16_t sockid, m_reply *r)
{
    /* basic sanity checks (destroy any broken task) */
    if (! r || sockid > SOCKET_MAX) {
        debug("server_send_reply(): bad parameters.\n");
        return server_reply_free(r);
    }

    /* check that there is an actual socket matching the id */
    if (! socket_exists(sockid)) {
        debug("server_send_reply(): no such socket.\n");
        return server_reply_free(r);
    }

    /* allocate work queue if necessary */
    if (! _work[sockid] && ! (_work[sockid] = queue_alloc()) ) {
        debug("server_send_reply(): socket work queue allocation failed.\n");
        /* FIXME what should we do here ? silently drop the task ? */
        return server_reply_free(r);
    }

    /* queue the task */
    queue_add(_work[sockid], (void *) r);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int server_reply_process(m_reply *r, m_socket *s)
{
    ssize_t w = 0;
    struct timespec ts;

    if (! r || ! s) return SOCKET_EPARAM;

    if ( (s->_flags & _SOCKET_RSV) != r->token) {
        debug("server_reply_process(): this socket does not belong to you!\n");
        return SOCKET_EPARAM;
    }

    /* check if task processing should be delayed;
       the delay has a second resolution and only warranties
       that the task will not be processed before it has
       elapsed. */
    if (r->timer && r->delay) {
        monotonic_timer(& ts);
        if (r->timer + r->delay > ts.tv_sec)
            return SOCKET_EDELAY;
    }

    /* header */
    if (r->header) {
        if (r->op & SERVER_MSG_OOB)
            w = socket_oob_write(s, DATA(r->header), SIZE(r->header));
        else w = socket_write(s, DATA(r->header), SIZE(r->header));
        if (w < (ssize_t) SIZE(r->header)) {
            if (w > 0) {
                debug("server_reply_process(): partial header write.\n");
                string_suppr(r->header, 0, w);
                return SOCKET_EAGAIN;
            } else if (w == SOCKET_EAGAIN) {
                return SOCKET_EAGAIN;
            } else return SOCKET_EFATAL;
        }
    }

    r->header = string_free(r->header);

    #ifdef _ENABLE_FILE
    /* file */
    if (r->file) {
        if ( (w = socket_sendfile(s, r->file, & r->off, r->len)) > 0) {
            if (! (r->len -= w) )
                r->file = fs_closefile(r->file);
            else return SOCKET_EAGAIN;
        } else return w;
    }
    #endif

    /* footer */
    if (r->footer) {
        w = socket_write(s, DATA(r->footer), SIZE(r->footer));
        if (w < (ssize_t) SIZE(r->footer)) {
            if (w > 0) {
                debug("server_reply_process(): partial footer write.\n");
                string_suppr(r->footer, 0, w);
                return SOCKET_EAGAIN;
            } else if (w == SOCKET_EAGAIN) {
                return SOCKET_EAGAIN;
            } else return SOCKET_EFATAL;
        }
    }

    r->footer = string_free(r->footer);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* Server polling routines */
/* -------------------------------------------------------------------------- */

#ifdef _ENABLE_UDP
static int _server_poll_udp(UNUSED const char *k, UNUSED size_t l, variant val)
{
    unsigned int id = value_to_integer(val);

    if (! queue_empty(_work[id])) {
        socket_queue_add(_writable, id);
        return -1;
    }

    return 0;
}
#endif

/* -------------------------------------------------------------------------- */

static void _server_poll(void)
{
    m_socket *s[_POLL_MAX], *new = NULL;
    int i = 0, pending = 0;

    if (! server_running) return;

    if (pthread_mutex_trylock(& _server_incoming) == 0) {
        pending = socket_queue_poll(_incoming, s, _POLL_MAX, 10);
        pthread_mutex_unlock(& _server_incoming);

        for (i = 0; i < pending; i ++) {
            if (SOCKET_INCOMING(s[i])) {
                if ( (new = socket_accept(s[i])) )
                    server_enqueue_blocking(new);
            }
            socket_release(s[i]); server_enqueue_listener(s[i]);
        }
    }

    if (pthread_mutex_trylock(& _server_blocking) == 0) {
        pending = socket_queue_poll(_blocking, s, _POLL_MAX, 10);
        pthread_mutex_unlock(& _server_blocking);

        for (i = 0; i < pending; i ++) {
            if (SOCKET_HASERROR(s[i])) {
                if (~s[i]->_flags & SOCKET_CLIENT) {
                    socket_release(s[i]); s[i] = socket_close(s[i]);
                    continue;
                } else if (! SOCKET_OUTGOING(s[i])) {
                    socket_persist(s[i]);
                    goto _wait;
                }
            }

            if (SOCKET_READABLE(s[i])) {
                socket_release(s[i]); server_enqueue_readable(s[i]);
                continue;
            }

            if (SOCKET_WRITABLE(s[i]) && ! SOCKET_IDLE(s[i])) {
                socket_release(s[i]); server_enqueue_writable(s[i]);
                continue;
            }

_wait:
            /* the socket is blocking, put it back in the waiting room */
            socket_release(s[i]); server_enqueue_blocking(s[i]);
        }
    }

    #ifdef _ENABLE_UDP
    /* poll the virtual udp sockets for writing */
    hashtable_foreach(_UDP, _server_poll_udp);
    #endif

    socket_queue_wait(_readable, 10000);
}

/* -------------------------------------------------------------------------- */
/* SERVER MAIN LOOP */
/* -------------------------------------------------------------------------- */

static m_socket *_server_receive(m_string *buffer)
{
    uint16_t socket_id = 0;
    m_socket *s = NULL;
    char *sockbuf = NULL;
    m_string *request = NULL;
    #ifdef _ENABLE_HTTP
    int http = 0;
    m_string *input = NULL;
    #endif
    m_plugin *p = NULL;
    int ret = 0;

    /* try to get a readable socket */
    socket_id = server_dequeue_readable();

    if (! socket_id || ! (s = socket_acquire(socket_id)) ) return NULL;

    #ifdef _ENABLE_HTTP
    /* check if there is a big pending request */
    if (_frag[SOCKET_ID(s)] && IS_LARGE(_frag[SOCKET_ID(s)])) {
        /* prepare for streaming */
        if (STRING_AVL(_frag[SOCKET_ID(s)]) < SOCKET_BUFFER) {
            string_dim(
                _frag[SOCKET_ID(s)],
                SIZE(_frag[SOCKET_ID(s)]) + SOCKET_BUFFER
            );
        }
        sockbuf = (char *) STRING_END(_frag[SOCKET_ID(s)]);
    } else
    #endif
    sockbuf = (char *) DATA(buffer);

    /* read the incoming data */
    if ( (ret = socket_read(s, sockbuf, SOCKET_BUFFER)) <= 0) {
        if (ret != SOCKET_EAGAIN) {
            if (socket_persist(s) == -1) {
                #ifdef _ENABLE_UDP
                if (~s->_flags & SOCKET_UDP) {
                #endif
                    /* close the socket immediately */
                    socket_release(s); s = socket_close(s);
                    return NULL;
                #ifdef _ENABLE_UDP
                } else goto _release;
                #endif
            } else goto _release;
        } else ret = 0;
    }

    /* prepare the input buffer */
    #ifdef _ENABLE_HTTP
    if (sockbuf == DATA(buffer)) {
    #endif
        buffer->_len = ret;
    #ifdef _ENABLE_HTTP
        input = buffer;
    } else {
        /* streaming - use the pending request buffer */
        input = _frag[SOCKET_ID(s)]; input->_len += ret;
        _frag[SOCKET_ID(s)] = NULL;
    }
    #endif

    #ifdef _ENABLE_UDP
    /* XXX
       to make UDP handling seamless at plugin level, we have to
       manually spawn a "virtual" socket for each client.
       these virtual sockets are actually using the same descriptor
       as the UDP listener.
       the mapping between IP addresses and Concrete socket ids
       is stored in the _UDP hashtable. */
    if (s->_flags & SOCKET_UDP) {
        unsigned int udp = 0;
        m_socket *z = NULL;
        variant val = hashtable_find(
            _UDP,
            (char *) s->info->ai_addr,
            s->info->ai_addrlen
        );

        if (! is_integer(val) || ! (udp = value_to_integer(val))) {
            SOCKET fd = 0;
            char host[NI_MAXHOST];
            char serv[NI_MAXSERV];

            /* duplicate the socket */
            if ( (fd = dupsocket(s->_fd)) == -1) goto _release;

            /* get the remote host address */
            ret = socket_rhost(s, host, sizeof(host), serv, sizeof(serv));
            if (ret != 0) { closesocket(fd); goto _release; }

            /* create a virtual UDP socket */
            z = socket_open(host, serv, SOCKET_UDP | SOCKET_NEW);
            if (! z) { closesocket(fd); goto _release; }

            z->_fd = fd; z->_flags |= (s->_flags & _SOCKET_RSV);

            /* lock the socket - no need to check since it
                is not yet visible to other threads */
            socket_lock(z);

            /* store the client socket for later retrieval */
            hashtable_insert(
                _UDP,
                (char *) z->info->ai_addr,
                z->info->ai_addrlen,
                value_from_integer(SOCKET_ID(z))
            );

        } else {
            /* get the virtual socket */
            z = socket_acquire(udp);
        }

        if (z) {
            /* replace the server socket by the virtual one */
            server_enqueue_blocking(s); s = socket_release(s); s = z;
        }
    }
    #endif

    #ifdef _ENABLE_HTTP
    request = http_get_request(& http, & _frag[SOCKET_ID(s)], input);
    #else
    /* check if there is already some data to be processed */
    if (_frag[SOCKET_ID(s)]) {
        /* append the new data to the fragmentation buffer */
        if (string_cat(_frag[SOCKET_ID(s)], buffer)) {
            request = _frag[SOCKET_ID(s)];
        } else {
            /* something is wrong */
            _frag[SOCKET_ID(s)] = string_free(_frag[SOCKET_ID(s)]);
            request = buffer;
        }
    } else request = buffer;
    #endif

    while (request) {
        /* get the plugin */
        if (! (p = plugin_acquire(PLUGIN_ID(s))) ) {
            /* no plugin for this socket, discard it */
            debug("_server_process_input(): socket without plugin !\n");
            socket_release(s); s = socket_close(s);
            return NULL;
        }

        /* call the plugin or the socket callback */
        if (s->callback) s->callback(SOCKET_ID(s), INGRESS_ID(s), request);
        else p->plugin_input_handler(SOCKET_ID(s), INGRESS_ID(s), request);

        /* release the plugin */
        p = plugin_release(p);

        #ifdef _ENABLE_HTTP
        request = http_get_request(& http, & _frag[SOCKET_ID(s)], input);
        #else
        /* check the buffer status */
        if (request == buffer) {
            /* check if the buffer has been entirely processed */
            if (! EMPTY(request))
                _frag[SOCKET_ID(s)] = string_dup(request);
            request = NULL;
        } else {
            if (EMPTY(request)) {
                /* free the fragmentation buffer if it is empty
                   or only contains a trailing NUL */
                _frag[SOCKET_ID(s)] = request = string_free(request);
            } else break;
        }
        #endif
    }

    /* check if there is pending tasks */
    if (SOCKET_IDLE(s)) {
        _release: server_enqueue_blocking(s); s = socket_release(s);
    }

    return s;
}

/* -------------------------------------------------------------------------- */

static int _server_respond(m_socket *s)
{
    uint16_t socket_id = 0;
    m_plugin *p = NULL;
    m_reply *r = NULL;
    int ret = 0;

    if (! s) {
        /* try to get a writable socket */
        socket_id = server_dequeue_writable();

        if (! socket_id || ! (s = socket_acquire(socket_id)) ) return 0;
    }

    while (1) {
        /* get a task */
        if (! (r = queue_get(_work[SOCKET_ID(s)])) ) goto _release;

        /* process it */
        switch (server_reply_process(r, s)) {
        case SOCKET_EPARAM:
            server_reply_free(r);
            goto _release;
        case SOCKET_EDELAY:
            ret = queue_empty(_work[SOCKET_ID(s)]);
            queue_add(_work[SOCKET_ID(s)], (void *) r);
            if (ret) goto _release; else continue;
        case SOCKET_EFATAL:
            if (socket_persist(s) == -1) {
                /* write error, close the socket immediately */
                socket_release(s); s = socket_close(s);
                server_reply_free(r);
                goto _continue;
            }
            /* FALLTHRU */
        case SOCKET_EAGAIN:
            queue_push(_work[SOCKET_ID(s)], (void *) r);
            goto _release;
        }

        /* the task was completed, notify the plugin if necessary */
        if ( (r->op & SERVER_MSG_ACK) && (p = plugin_acquire(PLUGIN_ID(s))) ) {
            /* TODO allow request tagging ? */
            if (p->plugin_event_handler) {
                p->plugin_event_handler(
                    SOCKET_ID(s),
                    INGRESS_ID(s),
                    PLUGIN_EVENT_REQUEST_TRANSMITTED,
                    NULL
                );
            }
            plugin_release(p);
        }

        /* Connection: close */
        if (r->op & SERVER_MSG_END) {
            socket_release(s); s = socket_close(s);
            server_reply_free(r);
            goto _continue;
        }

        /* destroy the completed task */
        server_reply_free(r);
    }

_release:
    #ifdef _ENABLE_UDP
    if (s->_flags & SOCKET_UDP) {
        hashtable_insert(
            _UDP,
            (char *) s->info->ai_addr,
            s->info->ai_addrlen,
            value_from_integer(SOCKET_ID(s))
        );
    }
    else
    #endif
        server_enqueue_blocking(s);

    s = socket_release(s);

_continue:
    return 1;
}

/* -------------------------------------------------------------------------- */

static void *_server_loop(UNUSED void *dummy)
{
    m_socket *s = NULL;
    char data[SOCKET_BUFFER];
    m_string buffer = STRING_STATIC_INITIALIZER(data, sizeof(data));

    #ifndef WIN32
    signal(SIGPIPE, SIG_IGN);
    #endif

    /* wait for it... */
    pthread_mutex_lock(& start_lock);
        while (! server_running) pthread_cond_wait(& start, & start_lock);
    pthread_mutex_unlock(& start_lock);

    /* server worker threads main loop */
    while (server_running) {
        s = _server_receive(& buffer);

        /* clean the input buffer */
        buffer._flags &= _STRING_EXTENSION;
        string_free_token(& buffer);
        buffer._data = data; buffer._alloc = sizeof(data);
        buffer._len = 0;

        /* poll if there is nothing else to do */
        if (! _server_respond(s) && ! s) _server_poll();
    }

    pthread_exit(NULL);
}

/* -------------------------------------------------------------------------- */
/* Socket API callbacks */
/* -------------------------------------------------------------------------- */

static int _server_listen_cb(m_socket *s)
{
    #ifdef _ENABLE_UDP
    if (~s->_flags & SOCKET_UDP)
    #endif
        /* TCP: put the socket in the accept queue */
        server_enqueue_listener(s);
    #ifdef _ENABLE_UDP
    else {
        /* UDP: handle the socket as a connected client */
        server_enqueue_blocking(s);
    }
    #endif

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _server_accept_cb(m_socket *s)
{
    m_plugin *p = NULL;

    /* notify the plugin that a new client has been accepted */
    if ( (p = plugin_acquire(PLUGIN_ID(s))) ) {
        if (p->plugin_event_handler) {
            p->plugin_event_handler(
                SOCKET_ID(s),
                INGRESS_ID(s),
                PLUGIN_EVENT_INCOMING_CONNECTION,
                NULL
            );
        }
        plugin_release(p);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _server_opened_cb(m_socket *s)
{
    m_plugin *p = NULL;

    /* connection successfully opened */
    if ( (p = plugin_acquire(PLUGIN_ID(s))) ) {
        if (p->plugin_event_handler) {
            p->plugin_event_handler(
                SOCKET_ID(s),
                INGRESS_ID(s),
                PLUGIN_EVENT_OUTGOING_CONNECTION,
                NULL
            );
        }
        plugin_release(p);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _server_reinit_cb(m_socket *s)
{
    m_plugin *p = NULL;
    m_reply *r = NULL, *retransmit = NULL;

    /* flush the work queue */
    if (_work[SOCKET_ID(s)]) {
        while ( (r = queue_get(_work[SOCKET_ID(s)])) ) {
            if (r->op & SERVER_MSG_ACK && ! retransmit) {
                retransmit = r; r = NULL;
            }
            r = server_reply_free(r);
        }
        _work[SOCKET_ID(s)] = queue_free(_work[SOCKET_ID(s)]);
    }

    /* ensure the fragmentation buffer is clean */
    _frag[SOCKET_ID(s)] = string_free(_frag[SOCKET_ID(s)]);

    if ( (p = plugin_acquire(PLUGIN_ID(s))) ) {
        if (p->plugin_event_handler) {
            /* notify the plugin that the socket needs to be reinitialized */
            p->plugin_event_handler(
                SOCKET_ID(s),
                INGRESS_ID(s),
                PLUGIN_EVENT_SOCKET_RECONNECTION,
                NULL
            );
            /* return the first failed response to the plugin for examination
               or retransmission */
            if (retransmit) {
                p->plugin_event_handler(
                    SOCKET_ID(s),
                    INGRESS_ID(s),
                    PLUGIN_EVENT_REQUEST_UNDELIVERED,
                    retransmit
                );
            }
        }
        plugin_release(p);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _server_urgent_cb(m_socket *s)
{
    char buffer[1024];
    ssize_t len = 0;
    m_plugin *p = NULL;
    m_string *m = NULL;

    /* try to read the OOB data */
    if ( (len = socket_oob_read(s, buffer, sizeof(buffer))) <= 0)
        return (len == SOCKET_EAGAIN) ? 0 : -1;

    m = string_encaps(buffer, len);

    /* notify the plugin */
    if ( (p = plugin_acquire(PLUGIN_ID(s))) ) {
        if (p->plugin_event_handler) {
            p->plugin_event_handler(
                SOCKET_ID(s),
                INGRESS_ID(s),
                PLUGIN_EVENT_OUT_OF_BAND_MESSAGE,
                m
            );
        }
        plugin_release(p);
    }

    string_free(m);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _server_closed_cb(m_socket *s)
{
    m_plugin *p = NULL;
    m_reply *r = NULL;

    if ( (p = plugin_acquire(PLUGIN_ID(s))) ) {
        /* notify the plugin that the socket is about to be closed */
        if (p->plugin_event_handler) {
            p->plugin_event_handler(
                SOCKET_ID(s),
                INGRESS_ID(s),
                PLUGIN_EVENT_SOCKET_DISCONNECTED,
                NULL
            );
        }
        plugin_release(p);
    }

    if (_work[SOCKET_ID(s)]) {
        while ( (r = queue_get(_work[SOCKET_ID(s)])) )
            r = server_reply_free(r);
        _work[SOCKET_ID(s)] = queue_free(_work[SOCKET_ID(s)]);
    }

    /* ensure the fragmentation buffer is clean */
    _frag[SOCKET_ID(s)] = string_free(_frag[SOCKET_ID(s)]);

    #ifdef _ENABLE_UDP
    /* if it is an UDP socket, remove it from the hashtable */
    if (s->_flags & SOCKET_UDP)
        hashtable_remove(_UDP, (char *) s->info->ai_addr, s->info->ai_addrlen);
    #endif

    return 0;
}

/* -------------------------------------------------------------------------- */
/* Public server API */
/* -------------------------------------------------------------------------- */

public int server_init(void)
{
    unsigned int i = 0;
    pthread_attr_t attr;
    int builtin = 0;

    monotonic_timer_init();

    /* greeting message ! */
    fprintf(stderr, "\nConcrete Server\n");
    fprintf(stderr, "version "CONCRETE_VERSION" ["__DATE__"]\n");
    fprintf(stderr, "Copyright (c) 2005-2020 ");
    fprintf(stderr, "Raphael Prevost, all rights reserved.\n\n");
    fprintf(stderr, "Concrete: starting engine.\n");

    if (string_api_setup() == -1) {
        fprintf(stderr, "server_init(): string API setup failed.\n");
        goto _err_string;
    }

    if (socket_api_setup() == -1) {
        fprintf(stderr, "server_init(): socket API setup failed.\n");
        goto _err_socket;
    }

    if (plugin_api_setup() == -1) {
        fprintf(stderr, "server_init(): plugin API setup failed.\n");
        goto _err_plugin;
    }

    #ifdef _ENABLE_FILE
    if (fs_api_setup() == -1) {
        fprintf(stderr, "server_init(): file API setup failed.\n");
        goto _err_file;
    }
    #endif

    if (pthread_attr_init(& attr) != 0) {
        perror(ERR(server_init, pthread_attr_init));
        goto _err_attr;
    }

    #ifdef _ENABLE_UDP
    /* create the UDP sockets registry */
    if (! (_UDP = hashtable_alloc(NULL)) ) {
        fprintf(stderr, "server_init(): failed to allocate the UDP registry.\n");
        goto _err_udp;
    }
    #endif

    /* hook the socket API */
    if ( (socket_hook(_HOOK_LISTEN, _server_listen_cb) == -1) ||
         (socket_hook(_HOOK_ACCEPT, _server_accept_cb) == -1) ||
         (socket_hook(_HOOK_OPENED, _server_opened_cb) == -1) ||
         (socket_hook(_HOOK_REINIT, _server_reinit_cb) == -1) ||
         (socket_hook(_HOOK_URGENT, _server_urgent_cb) == -1) ||
         (socket_hook(_HOOK_CLOSED, _server_closed_cb) == -1)) {
        fprintf(stderr, "server_init(): failed to hook the socket API.\n");
        goto _err_hook;
    }

    /* allocate the socket queues */
    if (! (_blocking = socket_queue_alloc()) ) return -1;
    if (! (_readable = socket_queue_alloc()) ) goto _err_rdq;
    if (! (_writable = socket_queue_alloc()) ) goto _err_wrq;
    if (! (_incoming = socket_queue_alloc()) ) goto _err_inq;

    #if defined(_ENABLE_CONFIG) && defined(HAS_LIBXML)
    if (configure(CONFDIR, "concrete.xml") == -1) {
        fprintf(stderr, "server_init(): server configuration failed.\n");
        goto _err_config;
    }

    _concurrency = config_get_concurrency();
    #else
    _concurrency = SERVER_CONCURRENCY;
    #endif

    /* spawn the worker threads */
    if (! (_thread = malloc(_concurrency * sizeof(*_thread))) ) {
        perror(ERR(server_init, malloc));
        goto _err_config;
    }

    pthread_attr_setstacksize(& attr, SERVER_STACKSIZE);

    for (i = 0; i < _concurrency; i ++) {
        if (pthread_create(& _thread[i], & attr, _server_loop, NULL) == -1) {
            perror(ERR(server_init, pthread_create));
            goto _err_start;
        }
    }

    pthread_attr_destroy(& attr);

    #ifdef _BUILTIN_PLUGIN
    /* try to load a builtin plugin */
    fprintf(stderr, "Concrete: trying to load builtin plugin...\n");
    if ( (builtin = plugin_open(__PLUGIN_BUILTIN__, "builtin")) != -1) {
        fprintf(stderr, "Concrete: builtin plugin found.\n");
        plugin_start(builtin, 0, NULL);
    } else fprintf(stderr, "Concrete: no builtin plugin.\n");
    #endif

    /* everything is ready, start the worker threads */
    pthread_mutex_lock(& start_lock);
        server_running = 1;
        pthread_cond_broadcast(& start);
    pthread_mutex_unlock(& start_lock);

    return 0;

_err_start:
    free(_thread);
    fprintf(stderr, "server_init(): failed to start server threads.\n");
_err_config:
    plugin_api_cleanup();
    socket_api_cleanup();
    _incoming = socket_queue_free(_incoming);
_err_inq:
    _writable = socket_queue_free(_writable);
_err_wrq:
    _readable = socket_queue_free(_readable);
_err_rdq:
    _blocking = socket_queue_free(_blocking);
_err_hook:
#ifdef _ENABLE_UDP
    _UDP = hashtable_free(_UDP);
_err_udp:
#endif
    pthread_attr_destroy(& attr);
_err_attr:
#ifdef _ENABLE_FILE
    fs_api_cleanup();
_err_file:
#endif
    plugin_api_cleanup();
_err_plugin:
    socket_api_cleanup();
_err_socket:
    string_api_cleanup();
_err_string:
    return -1;
}

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_PRIVILEGE_SEPARATION
/* -------------------------------------------------------------------------- */

public void __server_set_privileged_channel(int channel)
{
    _priv_com = channel;
}

/* -------------------------------------------------------------------------- */

public void __server_privileged_process(int channel)
{
    /* this is the only piece of code running with privileges.
     * it may perform UNIX authentication, or bind to privileged ports.
     * see the documentation for further details.
     */

    /* TODO:
     * EXEC exec an arbitrary command as root (hmmm)
     */

    int shutdown = 0;
    char buffer[BUFSIZ];
    off_t ret = 1;

    while (! shutdown) {

        /* cleanup - ret keeps its default value of 1 (error) */
        memset(buffer, 0, sizeof(buffer)); ret = 1;

        if (recv(channel, buffer, sizeof(buffer), 0x0) <= 0) {
            debug("server_privileged_process(): child process died.\n");
            serror(ERR(server_privileged_process, recv));
            exit(EXIT_FAILURE);
        }

        /* opcodes are *always* four chars long */
        if (memcmp(buffer, "BIND", _OP_LEN) == 0) {
            /* bind a socket to a privileged port */
            char *sep1 = NULL, *sep2 = NULL;
            char host[NI_MAXHOST], serv[NI_MAXSERV];
            int flags = 0, fd = 0;
            struct addrinfo hint, *a = NULL;

            /* params format: ip|port|flags */
            if (! (sep1 = memchr(buffer, '|', sizeof(buffer))) ) {
                if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                    serror(ERR(_privileged_process, send));
                continue;
            }

            sep2 = memchr(sep1 + 1, '|', sizeof(buffer) - (sep1 - buffer));
            if (! sep2) {
                if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                    serror(ERR(server_privileged_process, write));
                continue;
            }

            /* get the params */
            memset(host, 0, sizeof(host));
            memset(serv, 0, sizeof(serv));
            memcpy(host, buffer + _OP_LEN, sep1 - (buffer + _OP_LEN));
            memcpy(serv, sep1 + 1, (sep2 - sep1) - 1);
            /* we are only interested in the lower 16 bits of the flags */
            memcpy(& flags, sep2 + 1, sizeof(flags)); flags &= 0xFFFF;

            /* ignore the request if it is not a tcp server socket */
            if (flags & SOCKET_UDP || ~flags & SOCKET_SERVER) {
                if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                    serror(ERR(server_privileged_process, send));
                continue;
            }

            memset(& hint, 0, sizeof(hint));
            hint.ai_family = (flags & SOCKET_IP6) ? PF_INET6 : PF_INET;
            hint.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
            hint.ai_socktype = SOCK_STREAM;
            hint.ai_protocol = IPPROTO_TCP;

            if ( (ret = getaddrinfo(host, serv, & hint, & a)) != 0) {
                _gai_perror(ERR(server_privileged_process, getaddrinfo), ret);
                ret = 1;
                if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                    perror(ERR(server_privileged_process, send));
                continue;
            }

            /* all seems ok, ask for the fd */
            ret = 0;
            if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0) {
                /* if we fail here we'll never get the fd, so FATAL */
                serror(ERR(server_privileged_process, send));
                freeaddrinfo(a); continue;
            }
            ret = 1;

            /* get the fd */
            fd = socket_recvfd(channel);

            /* bind to the privileged port */
            if (bind(fd, a->ai_addr, a->ai_addrlen) == -1) {
                perror(ERR(server_privileged_process, bind));
                if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                    serror(ERR(server_privileged_process, send));
                freeaddrinfo(a);
                continue;
            }

            /* all went fine */
            ret = 0;
            if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                serror(ERR(server_privileged_process, send));
            socket_sendfd(channel, fd);

            freeaddrinfo(a);

        } else if (memcmp(buffer, "AUTH", _OP_LEN) == 0) {
            /* authenticate a user */
            #ifndef WIN32
            char *sep = NULL, *pass = NULL, usr[BUFSIZ], pwd[BUFSIZ];
            struct passwd *user_info;
            #ifdef HAS_SHADOW
            struct spwd *spwd;
            #endif

            /* params format: login:passwd */
            if (! (sep = memchr(buffer, ':', sizeof(buffer))) ) {
                if (send(channel, & ret, sizeof(int), 0x0) <= 0)
                    serror(ERR(server_privileged_process, send));
                continue;
            }

            /* get the user provided login and password */
            memset(usr, 0, sizeof(usr));
            memset(pwd, 0, sizeof(pwd));
            memcpy(usr, buffer + _OP_LEN, sep - (buffer + _OP_LEN));
            memcpy(pwd, sep + 1, buffer + sizeof(buffer) - sep);

            /* perform auth */
            if (! (user_info = getpwnam(usr)) ) {
                perror(ERR(_privileged_process, getpwnam));
                if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                    serror(ERR(server_privileged_process, send));
                continue;
            }

            #ifdef HAS_SHADOW
            if (! (spwd = getspnam(usr)) ) {
                /* permission denied ? */
                perror(ERR(server_privileged_process, getspnam));
                if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                    serror(ERR(server_privileged_process, send));
                continue;
            }

            user_info->pw_passwd = spwd->sp_pwdp;
            #endif

            pass = crypt(pwd, user_info->pw_passwd);

            if (! pass || strcmp(user_info->pw_passwd, pass) != 0) {
                /* passwords do not match */
                if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                    serror(ERR(server_privileged_process, send));
                continue;
            }

            /* all went fine */
            ret = 0;
            if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                serror(ERR(server_privileged_process, send));
            #else
            /* not implemented for now on win32 */
            ret = 1;
            if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                serror(ERR(server_privileged_process, send));
            #endif

        } else if (memcmp(buffer, "EXIT", _OP_LEN) == 0) {
            ret = 0;
            if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                serror(ERR(server_privileged_process, send));
            shutdown = 1;
        } else if (memcmp(buffer, "CONF", _OP_LEN) == 0) {
            #if defined(_ENABLE_CONFIG) && defined(HAS_LIBXML)
            struct stat info;
            int fd = -1;
            char *conf = NULL;

            /* for this call, the semantic is reversed;
               0 means an error occured, > 0 is the size of the file */
            ret = 0;

            if (stat(buffer + _OP_LEN, & info) == -1) {
                perror(ERR(_privileged_process, stat));
                if (send(channel, (char *) & ret, sizeof(int), 0x0) <= 0)
                    serror(ERR(server_privileged_process, send));
                continue;
            }

            if ( (fd = open(buffer + _OP_LEN, O_RDONLY)) == -1) {
                perror(ERR(_privileged_process, open));
                if (send(channel, (char *) & ret, sizeof(int), 0x0) <= 0)
                    serror(ERR(server_privileged_process, send));
                continue;
            }

            conf = mmap(NULL, info.st_size, PROT_READ, MAP_SHARED, fd, 0);

            if (conf == MAP_FAILED) {
                perror(ERR(_privileged_process, mmap));
                close(fd);
                if (send(channel, (char *) & ret, sizeof(int), 0x0) <= 0)
                    serror(ERR(server_privileged_process, send));
                continue;
            }

            if (send(channel, (char *) & info.st_size,
                     sizeof(info.st_size), 0x0) <= 0) {
                serror(ERR(server_privileged_process, send));
                munmap(conf, info.st_size); close(fd);
                continue;
            }

            if (send(channel, conf, info.st_size, 0x0) <= 0) {
                serror(ERR(server_privileged_process, send));
                munmap(conf, info.st_size); close(fd);
                continue;
            }

            munmap(conf, info.st_size); close(fd);
            #else
            ret = 0;
            if (send(channel, (char *) & ret, sizeof(ret), 0x0) <= 0)
                serror(ERR(server_privileged_process, send));
            #endif
        }
    }

    close(channel);
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

#ifdef _ENABLE_PRIVILEGE_SEPARATION
public int server_privileged_call(int opcode, const void *cmd, size_t len)
{
    off_t ret = -1;
    char *buf = NULL;
    m_socket *s = NULL;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    char *b = NULL;

    switch (opcode) {
        case OP_BIND:
            /* cmd is a m_socket here */
            if (len != sizeof(m_socket)) return -1;

            len = NI_MAXHOST + NI_MAXSERV + 1;

            s = (m_socket *) cmd;

            /* get the textual bind address */
            ret = getnameinfo(
                s->info->ai_addr,
                s->info->ai_addrlen,
                host,
                sizeof(host),
                serv,
                sizeof(serv),
                NI_NUMERICHOST | NI_NUMERICSERV
            );

            if (ret != 0) {
                _gai_perror(ERR(privileged, getnameinfo), ret);
                return -1;
            }

            if (! (buf = calloc(sizeof("BIND") + len, sizeof(char))) ) {
                perror(ERR(privileged, calloc));
                return -1;
            }

            b = buf;
            memcpy(b, "BIND", sizeof("BIND") - 1);
            b += sizeof("BIND") - 1;
            memcpy(b, host, strlen(host));
            b += strlen(host); *b ++ = '|';
            memcpy(b, serv, strlen(serv));
            b += strlen(serv); *b ++ = '|';
            memcpy(b, & s->_flags, sizeof(s->_flags));
            b += sizeof(s->_flags);

            pthread_mutex_lock(& _priv_lock);

            if (send(_priv_com, buf, b - buf, 0x0) <= 0) {
                serror(ERR(privileged, send));
                pthread_mutex_unlock(& _priv_lock);
                goto _err_bind;
            }
            if (recv(_priv_com, (char *) & ret, sizeof(ret), 0x0) <= 0) {
                serror(ERR(privileged, recv));
                pthread_mutex_unlock(& _priv_lock);
                goto _err_bind;
            }
            if (ret == 0) socket_sendfd(_priv_com, s->_fd);
            if (recv(_priv_com, (char *) & ret, sizeof(ret), 0x0) <= 0) {
                serror(ERR(privileged, recv));
                pthread_mutex_unlock(& _priv_lock);
                goto _err_bind;
            }
            if (ret == 0) s->_fd = socket_recvfd(_priv_com);

            pthread_mutex_unlock(& _priv_lock);

            free(buf);

            return ret;

        _err_bind:
            free(buf);
            return -1;

        case OP_AUTH:
            if (! cmd || ! len) return -1;
            if (! (buf = calloc(sizeof("AUTH") + len, sizeof(char))) ) {
                perror(ERR(privileged, calloc));
                return -1;
            }
            memcpy(buf, "AUTH", sizeof("AUTH") - 1);
            memcpy(buf + sizeof("AUTH") - 1, cmd, len);

            pthread_mutex_lock(& _priv_lock);

            if (send(_priv_com, buf, sizeof("AUTH") + len, 0x0) <= 0) {
                serror(ERR(privileged, send));
                pthread_mutex_unlock(& _priv_lock);
                goto _err_auth;
            }
            if (recv(_priv_com, (char *) & ret, sizeof(ret), 0x0) <= 0) {
                serror(ERR(privileged, recv));
                pthread_mutex_unlock(& _priv_lock);
                goto _err_auth;
            }

            pthread_mutex_unlock(& _priv_lock);

            free(buf);

            return ret;

        _err_auth:
            free(buf);
            return -1;

        case OP_EXIT:
            pthread_mutex_lock(& _priv_lock);

            if (send(_priv_com, "EXIT", sizeof("EXIT") - 1, 0x0) <= 0) {
                pthread_mutex_unlock(& _priv_lock);
                serror(ERR(privileged, send));
                return -1;
            }
            if (recv(_priv_com, (char *) & ret, sizeof(ret), 0x0) <= 0) {
                pthread_mutex_unlock(& _priv_lock);
                perror(ERR(privileged, recv));
                return -1;
            }

            pthread_mutex_unlock(& _priv_lock);

            return ret;

        #if defined(_ENABLE_CONFIG) && defined(HAS_LIBXML)
        case OP_CONF:
            if (! (buf = calloc(sizeof("CONF") + len, sizeof(char))) ) {
                perror(ERR(privileged, calloc));
                return -1;
            }

            b = buf;
            memcpy(b, "CONF", sizeof("CONF") - 1);
            b += sizeof("CONF") - 1;
            memcpy(b, cmd, len);

            pthread_mutex_lock(& _priv_lock);

            if (send(_priv_com, buf, sizeof("CONF") + len, 0x0) <= 0) {
                pthread_mutex_unlock(& _priv_lock);
                serror(ERR(privileged, send));
                free(buf);
                return -1;
            }

            free(buf);

            if (recv(_priv_com, (char *) & ret, sizeof(ret), 0x0) <= 0) {
                pthread_mutex_unlock(& _priv_lock);
                serror(ERR(privileged, recv));
                return -1;
            }

            if (ret) {
                if (! (buf = malloc(ret * sizeof(*buf))) ) {
                    pthread_mutex_unlock(& _priv_lock);
                    perror(ERR(privileged, malloc));
                    return -1;
                }
            } else {
                pthread_mutex_unlock(& _priv_lock);
                fprintf(stderr, "server_privileged_call(): "
                                "unable to access configuration file.\n");
                return -1;
            }

            if (recv(_priv_com, buf, ret, 0x0) <= 0) {
                pthread_mutex_unlock(& _priv_lock);
                serror(ERR(privileged, recv));
                free(buf);
                return -1;
            }

            pthread_mutex_unlock(& _priv_lock);

            /* we got the configuration file text, process it */
            ret = config_process(buf, ret);

            free(buf);

            return ret;
        #endif

        default:
            return -1;
    }
}
#else
public int server_privileged_call(UNUSED int o, UNUSED const void *c,
                                  UNUSED size_t l)
{
    /* notify the user that the privileges separation is disabled */
    fprintf(
        stderr,
        "server_privileged_call(): not implemented.\n"
        "==== WARNING ====\n"
        "This copy of Concrete Server is built "
        "without privileges separation.\n"
        "Running this build with system level access is unsafe.\n"
    );
    return -1;
}
#endif


/* -------------------------------------------------------------------------- */

public int server_open_managed_socket(uint32_t token, const char *ip,
                                      const char *port, int flags)
{
    m_socket *sock = NULL;
    int ret = 0;

    if ((token >> _SOCKET_RSS) > PLUGIN_MAX || ! port) {
        debug("server_open_managed_socket(): bad parameters.\n");
        return -1;
    }

    /* brand the socket as belonging to the plugin */
    flags |= (token & _SOCKET_RSV);

    if (! (sock = socket_open(ip, port, flags)) ) return -1;

    if (socket_lock(sock) != 0) { socket_close(sock); return -1; }

    if (flags & SOCKET_SERVER) {
        if (socket_listen(sock) != 0) {
            debug("server_open_managed_socket(): listen failed.\n");
            socket_unlock(sock);
            sock = socket_close(sock);
            return -1;
        }

        ret = 0;
    } else {
        ret = socket_connect(sock);

        if (ret != 0 && ret != SOCKET_EAGAIN) {
            debug("server_open_managed_socket(): connect failed.\n");
            socket_unlock(sock);
            sock = socket_close(sock);
            return -1;
        }

        ret = SOCKET_ID(sock);

        server_enqueue_blocking(sock);
    }

    socket_unlock(sock);

    return ret;
}

/* -------------------------------------------------------------------------- */

public void server_close_managed_socket(uint32_t token, uint16_t socket_id)
{
    m_reply *reply = NULL;

    /* generate the packet */
    if (! (reply = server_reply_init(SERVER_MSG_END, token)) ) {
        debug("server_close_managed_socket(): cannot allocate a reply.\n");
        return;
    }

    reply = server_send_reply(socket_id, reply);
}

/* -------------------------------------------------------------------------- */

public uint64_t server_socket_sentbytes(uint16_t sockid)
{
    if (sockid < 1 || sockid > SOCKET_MAX) {
        debug("server_socket_sentbytes(): bad parameters.\n");
        return 0;
    }

    return socket_sentbytes(sockid);
}

/* -------------------------------------------------------------------------- */

public uint64_t server_socket_recvbytes(uint16_t sockid)
{
    unsigned int buffered = 0;

    if (sockid < 1 || sockid > SOCKET_MAX) {
        debug("server_socket_recvbytes(): bad parameters.\n");
        return 0;
    }

    if (_frag[sockid]) buffered = SIZE(_frag[sockid]);

    return socket_recvbytes(sockid) - buffered;
}

/* -------------------------------------------------------------------------- */

public int server_set_socket_callback(uint32_t token, uint16_t sockid,
                                      void (*cb)(uint16_t, uint16_t, m_string *))
{
    m_socket *s = NULL;

    if (! token || ! sockid || ! cb) {
        debug("server_set_socket_callback(): bad parameters.\n");
        return -1;
    }

    if ((token >> _SOCKET_RSS) > PLUGIN_MAX) {
        debug("server_set_socket_callback(): bad token.\n");
        return -1;
    }

    if (! (s = socket_acquire(sockid)) ) {
        debug("server_set_socket_callback(): bad socket id.\n");
        return -1;
    }

    if ( (s->_flags & _SOCKET_RSV) != token) {
        debug("server_set_socket_callback(): this socket does not belong to you!\n");
        s = socket_release(s);
        return -1;
    }

    s->callback = cb;

    socket_release(s);

    return 0;
}

/* -------------------------------------------------------------------------- */

public int server_send_response(uint32_t token, uint16_t sockid, uint16_t flags,
                                const char *format, ...)
{
    m_reply *reply = NULL;
    m_string *string = NULL;
    va_list args;

    if (! token || ! sockid || ! format) {
        debug("server_send_response(): bad parameters.\n");
        return -1;
    }

    va_start(args, format);

    /* generate the packet */
    if (! (reply = server_reply_init(flags, token)) ) {
        debug("server_send_response(): cannot allocate reply.\n");
        goto _err_rep;
    }

    if (! (string = string_vfmt(NULL, format, args)) ) {
        debug("server_send_response(): cannot allocate header.\n");
        goto _err_fmt;
    }

    if (server_reply_setheader(reply, string) == -1) {
        debug("server_send_response(): cannot allocate task data.\n");
        goto _err_set;
    }

    va_end(args);

    /* store the new task */
    reply = server_send_reply(sockid, reply);

    return 0;

_err_set:
    string_free(string);
_err_fmt:
    reply = server_reply_free(reply);
_err_rep:
    va_end(args);
    return -1;
}

/* -------------------------------------------------------------------------- */

public int server_send_string(uint32_t token, uint16_t sockid, uint16_t flags,
                              m_string *string)
{
    m_reply *reply = NULL;

    if (! token || ! sockid || ! string || ! string->_data) {
        debug("server_send_string(): bad parameters.\n");
        return -1;
    }

    /* generate the packet */
    if (! (reply = server_reply_init(flags, token)) ) {
        debug("server_send_string(): cannot allocate reply.\n");
        return -1;
    }

    if (server_reply_setheader(reply, string) == -1) {
        debug("server_send_string(): cannot set reply header.\n");
        reply = server_reply_free(reply);
        return -1;
    }

    /* store the new task */
    reply = server_send_reply(sockid, reply);

    return 0;
}

/* -------------------------------------------------------------------------- */

public int server_send_buffer(uint32_t token, uint16_t sockid, uint16_t flags,
                              const char *data, size_t len)
{
    m_reply *reply = NULL;
    m_string *string = NULL;

    if (! token || ! sockid || ! data || ! len) {
        debug("server_send_buffer(): bad parameters.\n");
        return -1;
    }

    /* generate the packet */
    if (! (reply = server_reply_init(flags, token)) ) {
        debug("server_send_buffer(): cannot allocate reply.\n");
        return -1;
    }

    if (! (string = string_alloc(data, len)) ) {
        debug("server_send_buffer(): cannot allocate header.\n");
        reply = server_reply_free(reply);
        return -1;
    }

    if (server_reply_setheader(reply, string) == -1) {
        debug("server_send_buffer(): cannot set reply header.\n");
        reply = server_reply_free(reply);
        string = string_free(string);
        return -1;
    }

    /* store the new task */
    reply = server_send_reply(sockid, reply);

    return 0;
}

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_HTTP
/* -------------------------------------------------------------------------- */

public int server_send_http(uint32_t token, uint16_t sockid, uint16_t flags,
                            m_http *request, int method, const char *action,
                            const char *host)
{
    m_reply *reply = NULL;
    m_string *http = NULL, *header = NULL, *footer = NULL;
    unsigned int i = 0;

    if (! token || ! sockid || ! request) {
        debug("server_send_http(): bad parameters.\n");
        return -1;
    }

    /* format the HTTP request (allow for 64k inline attachments) */
    http = http_compile(request, method, action, host, 65535);

    if (http->parts && ~http->parts & 0x1) {
        /* got a multipart request with file attachments */

        /* rewind the HTTP request */
        while (request->prev) request = request->prev;

        for (i = 0; i < http->parts; i += 2) {
            /* generate a new request */
            if (! (reply = server_reply_init(flags, token)) ) {
                debug("server_send_http(): cannot allocate a reply.\n");
                return -1;
            }

            if (! (header = string_dup(TOKEN(http, i))) ) {
                debug("server_send_http(): cannot allocate header.\n");
                http = string_free(http);
                reply = server_reply_free(reply);
                return -1;
            }

            if (server_reply_setheader(reply, header) == -1) {
                debug("server_send_http(): cannot set reply header.\n");
                http = string_free(http);
                reply = server_reply_free(reply);
                header = string_free(header);
                return -1;
            }

            #ifdef _ENABLE_FILE
            /* get the matching file in the http request and attach it */
            while (! request->file && request->next)
                request = request->next;

            if (! (reply->file = fs_reopenfile(request->file)) ) {
                debug("server_send_http(): cannot reopen file.\n");
                http = string_free(http);
                server_reply_free(reply);
                return -1;
            }

            reply->off = request->offset;
            reply->len = (request->length) ? request->length :
                         reply->file->len - reply->off;
            #endif

            /* set the footer */
            if (! (footer = string_dup(TOKEN(http, i + 1))) ) {
                debug("server_send_http(): cannot allocate footer.\n");
                http = string_free(http);
                reply = server_reply_free(reply);
                return -1;
            }

            if (server_reply_setfooter(reply, footer) == -1) {
                debug("server_send_http(): cannot set reply header.\n");
                http = string_free(http);
                reply = server_reply_free(reply);
                footer = string_free(footer);
                return -1;
            }

            /* every two parts complete a request */
            reply = server_send_reply(sockid, reply);
        }

        http = string_free(http);
    } else {
        /* generate a new request */
        if (! (reply = server_reply_init(flags, token)) ) {
            debug("server_send_http(): cannot allocate reply.\n");
            return -1;
        }

        if (server_reply_setheader(reply, http) == -1) {
            debug("server_send_http(): cannot set reply header.\n");
            http = string_free(http);
            reply = server_reply_free(reply);
            return -1;
        }

        reply = server_send_reply(sockid, reply);
    }

    /* clean up the original request */
    request = http_close(request);

    return 0;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public void server_exit(void)
{
    unsigned int i = 0;

    /* if the server was never started to begin with, do nothing */
    if (! server_running) return;

    /* tell all the workers the server is going down... */
    server_running = 0;

    /* broadcast a shutdown message to all plugins */
    plugin_api_shutdown();

    for (i = 0; i < _concurrency; i ++)
        pthread_join(_thread[i], NULL);
    free(_thread);

    /* close plugins and any socket left open */
    socket_api_cleanup();
    plugin_api_cleanup();

    /* destroy all the queues */
    _blocking = socket_queue_free(_blocking);
    _readable = socket_queue_free(_readable);
    _writable = socket_queue_free(_writable);
    _incoming = socket_queue_free(_incoming);

    #ifdef _ENABLE_UDP
    _UDP = hashtable_free(_UDP);
    #endif

    #if defined(_ENABLE_CONFIG) && defined(HAS_LIBXML)
    configure_cleanup();
    #endif

    #ifdef _ENABLE_FILE
    fs_api_cleanup();
    #endif

    string_api_cleanup();

    /* bye bye */
    fprintf(stderr, "Concrete: thank you for using Concrete Server !\n");
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

/* Server API will not be compiled in the Concrete Library */
#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
