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

#ifndef ASKL_SERVER_H

#define ASKL_SERVER_H

#ifdef _ENABLE_SERVER

/* required build configuration */
#ifndef _ENABLE_HASHTABLE
#error "ASKL: the server module requires the builtin hashtable."
#endif

#include "askl.h"
#include "askl_steque.h"
#include "m_string.h"
#include "askl_socket.h"
#include "askl_module.h"
#include "askl_htable.h"
#include "m_file.h"
#include "m_config.h"
#ifdef _ENABLE_HTTP
#include "m_http.h"
#endif
#ifndef WIN32
#include <signal.h>
#include <pwd.h>
#include <sys/wait.h>
#ifdef HAS_SHADOW
    #include <shadow.h>
#endif
#endif

/** @defgroup server core::server */

#define SERVER_TIMEOUT      10          /* millisecond */
#define SERVER_CONCURRENCY  48          /* threads */
#define SERVER_STACKSIZE    524288      /* bytes */

/** End: this flag instructs the server to close the connection
         after the flagged message has been sent. */
#define SERVER_MSG_END     0x0001
/** Acknowledgment: this flag instructs the server to notify the
                    module when the flagged message is sent. */
#define SERVER_MSG_ACK     0x0002
/** Out Of Band: this flag instructs the server to send the message
                 out of band. */
#define SERVER_MSG_OOB     0x0004

/* the reply stucture is used to store informations about packets to process
   and queue them if they can not be sent immediately */

typedef struct m_reply {
    /* private */
    long timer;
    uint16_t delay;

    uint16_t op;

    uint32_t token;

    m_string *header;
    m_string *footer;

    #ifdef _ENABLE_FILE
    m_file *file;
    off_t off;
    size_t len;
    #endif
} m_reply;

/* -------------------------------------------------------------------------- */

public int server_init(void);

/**
 * @ingroup server
 * @fn int server_init(void)
 * @return 0 if all went fine, -1 otherwise
 *
 * This function starts the server subsystem, causing all incoming connections
 * to be handled by workers thread, and outbound connections to be managed the
 * same way.
 *
 * When you want to stop the server, you must call server_exit().
 *
 * @see server_exit()
 *
 */

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_PRIVILEGE_SEPARATION
/* -------------------------------------------------------------------------- */

public void __server_set_privileged_channel(int channel);

/**
 * @ingroup server
 * @fn void server_set_privileged_channel(int mode, int fd)
 * @param fd a valid file descriptor
 *
 * This function should be used only by a host binary to setup the privileges
 * separation by establishing the connection between the privileged process
 * and the user process. Please see the @ref main() function code for further
 * indications.
 *
 * @warning This function is public only to allow bootstrapping from
 *          a host binary.
 *
 * @see main()
 *
 */

/* -------------------------------------------------------------------------- */

public void __server_privileged_process(int channel);

/**
 * @ingroup server
 * @fn void server_privileged_process(int channel)
 * @param channel privileged communication channel
 *
 * This function processes messages from the channel, coming from the
 * user process, performs privileged tasks such as authenticating a user
 * or binding to a privileged port, then send an answer back.
 * Please see the source code for further indications.
 *
 * @warning This function is public only to allow bootstrapping from
 *          a host binary.
 *
 */

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public int server_privileged_call(int opcode, const void *cmd, size_t len);

/**
 * @ingroup server
 * @fn int server_privileged_call(int opcode, const void *cmd, size_t len);
 * @param opcode a privileged task
 * @param cmd some optional, task dependant parameter
 * @param len length in bytes of the optional parameter
 * @return -1 if an error occurs, 0 otherwise
 *
 * This function accepts 4 opcodes:
 *
 * - OP_AUTH: authenticate a user; @ref cmd format: "login:password"
 * - OP_BIND: binds a ASKL_Socket given in @ref cmd to a privileged port
 * - OP_CONF: reads the configuration file and returns its content
 * - OP_EXIT: shutdown the privileged process
 *
 */

/* -------------------------------------------------------------------------- */

public int server_open_managed_socket(
    uint32_t token,
    const char *ip,
    const char *port,
    int flags
);

/**
 * @ingroup server
 * @fn server_open_managed_socket(uint32_t token, const char *ip,
 *                                const char *port, int flags)
 * @param token the token given at module initialization
 * @param ip the ip on which to connect/listen
 * @param port the port on which to connect/listen
 * @param prot the IP protocol to use
 * @return -1 if an error occurs, the identifier of the socket otherwise.
 *
 * The parameters of this function are the same required by @ref socket_open(),
 * except the @ref token parameter which is the unique identifier attributed
 * to the module by the server.
 *
 * This function opens and registers a new socket as belonging to the module
 * identified by the token. This socket will be fully managed by the server
 * (establishing a non blocking connection, accepting new connections, and
 *  closing the socket if an error occurs)
 *
 * The connection will be automatically established by the server, and the
 * module will be notified upon completion.
 *
 * All connections and data coming through this port will then be handed to
 * this module.
 *
 * This function should be called by a module inside the module_init()
 * function to listen on specific ports if necessary.
 *
 * @attention
 * If you use this function to create a listening socket, the return value
 * will be 0 instead of the socket identifier if the call is successfull.
 * This is to prevent hijacking of a listening socket by an overzealous
 * module.
 *
 * @see socket_open()
 * @see module_open()
 *
 */

/* -------------------------------------------------------------------------- */

public void server_close_managed_socket(uint32_t token, uint16_t socket_id);

/* -------------------------------------------------------------------------- */

public int server_set_socket_callback(uint32_t token, uint16_t sockid,
                                      void (*cb)(uint16_t, uint16_t, m_string *));

/* -------------------------------------------------------------------------- */

public m_reply *server_reply_init(uint16_t flags, uint32_t token);

/* -------------------------------------------------------------------------- */

public int server_reply_setheader(m_reply *reply, m_string *data);

/* -------------------------------------------------------------------------- */

public int server_reply_setfooter(m_reply *reply, m_string *data);

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_FILE
/* -------------------------------------------------------------------------- */

public int server_reply_setfile(m_reply *reply, m_file *f, off_t o, size_t len);

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public int server_reply_setdelay(m_reply *reply, unsigned int nsec);

/* -------------------------------------------------------------------------- */

public m_reply *server_send_reply(uint16_t sockid, m_reply *r);

/* -------------------------------------------------------------------------- */

public m_reply *server_reply_free(m_reply *r);

/* -------------------------------------------------------------------------- */

public int server_send_response(uint32_t token, uint16_t sockid, uint16_t flags,
                                const char *format, ...);

/**
 * @ingroup server
 * @fn int server_send_response(uint32_t token, uint16_t sockid, uint16_t flags,
 *                              const char *format, ...)
 * @param token the module token (@see @ref module_init())
 * @param sockid the 16 bit socket identifier for the output socket
 * @param flags specific commands to execute after sending the payload
 * @param format format of the payload (@see @ref m_vsnprinf())
 * @param ... the payload elements
 * @return -1 if an error occurs, 0 otherwise
 *
 * This function will build the payload out of the payload elements, according
 * to the given format, then send it over the socket identified by @ref sockid.
 *
 * Additionaly, if the @ref SERVER_MSG_END flag is given, the connection will
 * be closed after sending the payload. The @ref SERVER_MSG_ACK flag will
 * cause the module to receive a notification after the packet is sent.
 *
 */

/* -------------------------------------------------------------------------- */

public int server_send_string(
    uint32_t token,
    uint16_t sockid,
    uint16_t flags,
    m_string *string
);

/**
 * @ingroup server
 * @fn int server_send_string(uint32_t token, uint16_t sockid, uint16_t flags,
 *                            m_string *string);
 * @param token the module token (@see @ref module_init())
 * @param sockid the 16 bit socket identifier for the output socket
 * @param flags specific commands to execute after sending the payload
 * @param string the payload
 * @return -1 if an error occurs, 0 otherwise
 *
 * This function is identical to @ref server_send_response() in its working,
 * except that it simply send the payload given in parameter.
 *
 * This function should be used preferably to @ref server_send_response() when
 * the payload does not need any processing; please avoid:
 * server_send_response(token, sockid, 0x0, "%.*s", SIZE(string), CSTR(string));
 * but use:
 * server_send_string(token, sockid, 0x0, string);
 * instead for better performances.
 *
 * @note if you send repeatedly the same data, you can use the @ref string_use()
 * function on the given string before calling this function; then, the server
 * will be able to use the string data without copying them. @ref string_use()
 * is more efficient if you keep the original string in a cache, and destroy
 * it when the data it contains will never be sent again.
 *
 * @warning using @ref string_use() incurs a small memory overhead, so it is
 * better to use it for long strings which are sent frequently.
 *
 */

/* -------------------------------------------------------------------------- */

public int server_send_buffer(
    uint32_t token,
    uint16_t sockid,
    uint16_t flags,
    const char *data,
    size_t len
);

/**
 * @ingroup server
 * @fn int server_send_buffer(uint32_t token, uint16_t sockid, uint16_t flags,
 *                            const char *data, size_t len);
 * @param token the module token (@see @ref module_init())
 * @param sockid the 16 bit socket identifier for the output socket
 * @param flags specific commands to execute after sending the payload
 * @param data the payload
 * @param len payload size in bytes
 * @return -1 if an error occurs, 0 otherwise
 *
 * This function is identical to @ref server_send_response() in its working,
 * except that it simply send the payload given in parameter.
 *
 * This function should be used preferably to @ref server_send_response() when
 * the payload does not need any processing; please avoid:
 * server_send_response(token, sockid, 0x0, "%.*s", length, my_payload);
 * but use:
 * server_send_buffer(token, sockid, 0x0, my_payload, length);
 * instead for better performances.
 *
 */

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_HTTP
/* -------------------------------------------------------------------------- */

public int server_send_http(
    uint32_t token,
    uint16_t sockid,
    uint16_t flags,
    m_http *request,
    int method,
    const char *action,
    const char *host
);

/**
 * @ingroup server
 * @fn int server_send_http(uint32_t token, uint16_t sockid,
 *                          uint16_t flags, m_http *request,
 *                          int method, const char *action,
 *                          const char *host);
 * @param token the module token (@see @ref module_init())
 * @param sockid the 16 bit socket identifier for the output socket
 * @param flags specific commands to execute after sending the payload
 * @param request an http request
 * @param method an HTTP method (HTTP_GET or HTTP_POST)
 * @param action the uri to use
 * @param host hostname
 * @return -1 if an error occurs, 0 otherwise
 *
 * Send the given HTTP request and clean it up afterward.
 *
 */

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

public void server_exit(void);

/**
 * @ingroup server
 * @fn void server_exit(void)
 *
 * This function stops the server and clean all of its internal state, including
 * connections and data created while it was running.
 *
 */

/* -------------------------------------------------------------------------- */

/* _ENABLE_SERVER */
#endif

#endif
