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

#ifdef ASKL_SOCKET_H

/* -------------------------------------------------------------------------- */
/* Socket internals */
/* -------------------------------------------------------------------------- */

typedef struct _Socket {
    struct ASKL_Socket interface;

    uint32_t _flags;
    SOCKET _fd;

    uint64_t _tx;
    uint64_t _rx;

    uint32_t _tag;

    uint16_t _state;
    int16_t _lockstate;

    #ifdef _ENABLE_SSL
    /* private, ssl */
    SSL *_ssl;
    #endif

    struct addrinfo *info;

    pthread_mutex_t *_lock;
    pthread_cond_t *_cond;
} _Socket;

/* the public interface must be the first member of the structure */
STATIC_ASSERT(offsetof(_Socket, interface) == 0, interface_must_be_first);

/* flags field structure (32 bits):
   [  ....  ....  .... ....  ....  ....  ....  ....  ]
     0           8         16          24          32
   (socket opt.) (  ingress id  ) (socket identifier)
   with:
   * socket opt.: socket options flags (protocol, status)
                  mask: 0xff000000
   * ingr. id   : ingress identifier
                  mask: 0x00fff000
   * socket id  : socket identifier
                  mask: 0x00000fff
*/

/* bitmasks */
#define _SOCKET_SOCK_ID 0x00000fff
#define _SOCKET_INGRESS 0x00fff000
#define _SOCKET_OPTIONS 0xff000000

/* socket status */
#define _SOCKET_B 0x0001    /* the socket is bound */
#define _SOCKET_A 0x0002    /* the socket is being accepted */
#define _SOCKET_C 0x0004    /* the socket is being connected */
#define _SOCKET_I 0x0008    /* inbound connection (accept()'ed socket) */
#define _SOCKET_O 0x0010    /* outbound connection (connect()'ed socket) */

#define _SOCKET_E 0x0020    /* an error occured on this socket */
#define _SOCKET_R 0x0040    /* the socket is readable */
#define _SOCKET_W 0x0080    /* the socket is writable */

/* -------------------------------------------------------------------------- */
/* Socket Helpers */
/* -------------------------------------------------------------------------- */

static inline _Socket *socket_private_interface(ASKL_Socket *socket)
{
    return (_Socket *) socket;
}

/* -------------------------------------------------------------------------- */

static inline ASKL_Socket *socket_public_interface(_Socket *socket)
{
    return (ASKL_Socket *) socket;
}

/* -------------------------------------------------------------------------- */

static inline uint16_t socket_get_id(ASKL_Socket *socket)
{
    _Socket *s = socket_private_interface(socket);
    return (s->_flags & _SOCKET_SOCK_ID);
}

/* -------------------------------------------------------------------------- */

static inline uint32_t socket_get_options(ASKL_Socket *socket)
{
    _Socket *s = socket_private_interface(socket);
    return (s->_flags & _SOCKET_OPTIONS);
}

/* -------------------------------------------------------------------------- */

static inline int socket_option_isset(ASKL_Socket *socket, uint32_t opt)
{
    _Socket *s = socket_private_interface(socket);
    return (s->_flags & opt);
}

/* -------------------------------------------------------------------------- */

static inline uint16_t _get_ingress_id(uint32_t flags)
{
    return ((flags & _SOCKET_INGRESS) >> 12);
}

/* -------------------------------------------------------------------------- */

static inline uint32_t _set_ingress_id(uint16_t ingress)
{
    return ((ingress << 12) & _SOCKET_INGRESS);
}

/* -------------------------------------------------------------------------- */

static inline uint16_t socket_get_ingress(ASKL_Socket *socket)
{
    _Socket *s = socket_private_interface(socket);
    return _get_ingress_id(s->_flags);
}

/* -------------------------------------------------------------------------- */

static inline const struct sockaddr *socket_get_sockaddr(ASKL_Socket *socket)
{
    _Socket *s = socket_private_interface(socket);
    return s->info->ai_addr;
}

/* -------------------------------------------------------------------------- */

static inline socklen_t socket_get_socklen(ASKL_Socket *socket)
{
    _Socket *s = socket_private_interface(socket);
    return s->info->ai_addrlen;
}

/* -------------------------------------------------------------------------- */

static inline uint32_t socket_get_tag(ASKL_Socket *socket)
{
    _Socket *s = socket_private_interface(socket);
    if (! s) return 0;
    return s->_tag;
}

/* -------------------------------------------------------------------------- */

static inline int socket_set_tag(ASKL_Socket *socket, uint32_t tag)
{
    _Socket *s = socket_private_interface(socket);
    if (! s) return -1;
    s->_tag = tag;
    return 0;
}

/* -------------------------------------------------------------------------- */

static inline int socket_haserror(ASKL_Socket *socket)
{
    _Socket *s = socket_private_interface(socket);
    return (s->_state & _SOCKET_E);
}

/* -------------------------------------------------------------------------- */

static inline int socket_readable(ASKL_Socket *socket)
{
    _Socket *s = socket_private_interface(socket);
    return (s->_state & _SOCKET_R);
}

/* -------------------------------------------------------------------------- */

static inline int socket_writable(ASKL_Socket *socket)
{
    _Socket *s = socket_private_interface(socket);
    return (s->_state & _SOCKET_W);
}

/* -------------------------------------------------------------------------- */

static inline int socket_incoming(ASKL_Socket *socket)
{
    _Socket *s = socket_private_interface(socket);
    return (s->_state & _SOCKET_R);
}

/* -------------------------------------------------------------------------- */

static inline int socket_outgoing(ASKL_Socket *socket)
{
    _Socket *s = socket_private_interface(socket);
    return (s->_state & _SOCKET_C);
}

/* -------------------------------------------------------------------------- */
/* Socket Queue internals */
/* -------------------------------------------------------------------------- */

struct _ASKL_SocketQueue {
    pthread_mutex_t _head_lock;
    pthread_mutex_t _tail_lock;
    pthread_cond_t _empty;

    uint16_t _head_index;
    uint16_t _tail_index;

    uint16_t *_ring;
};

/* -------------------------------------------------------------------------- */

#endif
