/*******************************************************************************
 *  Dialflirt Messenger                                                        *
 *  Copyright (c) 2005-2020 Raphael Prevost <raph@el.bzh>                      *
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

#include "dialmsn_plugin.h"

/* plugin identifier */
static uint32_t plugin_token = 0;
static char *http_host = NULL;
static char *eurolive_feed = NULL;

/* plugin options */
static int option_logging = 0;          /* log all users conversations */
static int option_bot = 0;              /* enable the chatbot */
static int option_hostess = 0;          /* enable the eurolive hostesses */
static int option_sex_quotas = 0;       /* enable the sex quotas */
static int option_subscribers_only = 0; /* only allow subscribers to connect */

#define DIALMSN_SQL_CLRALLSESS "CALL clear_all_sessions()"
#define DIALMSN_SQL_ADDUSERFAV "CALL add_user_to_favourite(%i, %i)"
#define DIALMSN_SQL_DELUSERFAV "CALL del_user_from_favourite(%i, %i)"
#define DIALMSN_SQL_ADDUSERBLK "CALL add_user_to_blacklist(%i, %i)"
#define DIALMSN_SQL_DELUSERBLK "CALL del_user_from_blacklist(%i, %i)"
#define DIALMSN_SQL_LOGCONVERS "CALL add_chat_log(%i, %i, %.*s)"
#define DIALMSN_SQL_GRANTRIGHT "CALL grant_photo_access(%i, %i)"
#define DIALMSN_SQL_RVOKERIGHT "CALL del_photo_access(%i, %i)"
#define DIALMSN_SQL_GETMAILCNT "CALL get_unread_email_count(%i)"
#define DIALMSN_SQL_ADDACCOUNT \
"CALL add_user_account(%.*s, %.*s, %.*s, %i, %.*s, %s, %s)"

static char _userlist_allow[DIALMSN_USER_MAX];

/* -------------------------------------------------------------------------- */

private uint32_t plugin_get_token(void)
{
    return plugin_token;
}

/* -------------------------------------------------------------------------- */

private int plugin_quotas_enabled(void)
{
    return option_sex_quotas;
}

/* -------------------------------------------------------------------------- */

private int plugin_nonsubscribers_allowed(void)
{
    return ! option_subscribers_only;
}

/* -------------------------------------------------------------------------- */

private const char *plugin_get_host(void)
{
    return http_host;
}

/* -------------------------------------------------------------------------- */
/* MANDATORY PLUGIN CALLBACKS */
/* -------------------------------------------------------------------------- */

public unsigned int plugin_api(void)
{
    return DIALMSN_REQUIRED_API_REV;
}

/* -------------------------------------------------------------------------- */

public int plugin_init(uint32_t id, int argc, char **argv)
{
    m_db *con = NULL;
    const char *host = NULL, *feed = NULL;

    /* do not allow loading this plugin twice */
    if (plugin_token) {
        fprintf(stderr, "DialMessenger: plugin already loaded.\n");
        return -1;
    }

    fprintf(stderr, "DialMessenger: loading DialMessenger plugin...\n");
    fprintf(stderr, "DialMessenger: Copyright (c) 2007-2019 ");
    fprintf(stderr, "Raphael Prevost, all rights reserved.\n");
    fprintf(stderr, "DialMessenger: version "DIALMSN_VERSION" ["__DATE__"]\n");

    fprintf(stderr, "DialMessenger: initializing the user list cache.\n");

    if (dialmsn_userlist_initcache() == -1) return -1;

    fprintf(stderr, "DialMessenger: connecting to database.\n");

    if (dialmsn_db_init() == -1) goto _error_db;

    /* clear all session data before allowing new connections */
    con = dialmsn_db_borrow();
    db_exec(con, DIALMSN_SQL_CLRALLSESS);
    con = dialmsn_db_return(con);

    memset(_userlist_allow, 1, sizeof(_userlist_allow));

    /* process plugin options */

    /* check if conversations must be logged */
    if ( (option_logging = plugin_getboolopt("dialmsn_logging", argc, argv)) )
        fprintf(stderr, "DialMessenger: logging enabled.\n");

    /* HTTP host, for integration with the PHP interface */
    if ( (host = plugin_getopt("dialmsn_http_host", argc, argv)) ) {
        http_host = strndup(host, strlen(host));
        fprintf(stderr, "DialMessenger: HTTP Host: \"%s\".\n", http_host);
    }

    /* start eurolive hostesses service if enabled */
    if (plugin_getboolopt("eurolive_hostess_service", argc, argv)) {
        if ( (feed = plugin_getopt("eurolive_feed", argc, argv)) ) {
            eurolive_feed = strndup(feed, strlen(feed));
            fprintf(stderr, "DialMessenger: Eurolive feed: %s.\n",
                    eurolive_feed);
            fprintf(stderr, "DialMessenger: starting the "
                    "Eurolive hostess service... ");
            if (dialmsn_hostess_init(eurolive_feed) == -1) {
                fprintf(stderr, "failure.\n");
                goto _error_hostess;
            }
            fprintf(stderr, "success.\n"); option_hostess = 1;
        } else {
            fprintf(stderr, "DialMessenger: Eurolive hostess service requires "
                    "the URL of the XML feed in \"eurolive_feed\" option.\n");
            goto _error_hostess;
        }
    }

    /* start the bot */
    if (plugin_getboolopt("dialmsn_bot", argc, argv)) {
        fprintf(stderr, "DialMessenger: starting the bot... ");
        if (dialmsn_bot_init() == -1) {
            fprintf(stderr, "failure.\n");
            goto _error_bot;
        }
        fprintf(stderr, "success.\n"); option_bot = 1;
    }

    /* other options */
    option_sex_quotas = plugin_getboolopt("dialmsn_sex_quotas", argc, argv);
    fprintf(stderr, "DialMessenger: sex quotas %s.\n",
            (option_sex_quotas) ? "enabled" : "disabled");

    option_subscribers_only = plugin_getboolopt("dialmsn_subscribers_only",
                                                argc, argv);
    fprintf(stderr, "DialMessenger: non subscriber access is %s.\n",
            (option_subscribers_only) ? "disabled" : "enabled");

    fprintf(stderr, "DialMessenger: listening on port "DIALMSN_PORT".\n");

    if (server_open_managed_socket(id, NULL, DIALMSN_PORT,
                                   SOCKET_SERVER) == -1) {
        fprintf(stderr, "DialMessenger: could not listen to "
                        "TCP port "DIALMSN_PORT".\n");
        goto _error_socket;
    }

    plugin_token = id;

    return 0;

_error_socket:
    if (option_bot) dialmsn_bot_exit();
_error_bot:
    if (option_hostess) {
        dialmsn_hostess_exit();
        free(eurolive_feed);
    }
_error_hostess:
    dialmsn_db_exit();
    free(http_host);
_error_db:
    dialmsn_userlist_freecache();

    return -1;
}

/* -------------------------------------------------------------------------- */

public void plugin_input_handler(uint16_t id, UNUSED uint16_t ingress,
                                 m_string *buffer)
{
    uint32_t request = 0;
    uint32_t error = 0;
    uint32_t term = htonl(DIALMSN_TERM);
    m_db *con = NULL;

    while (SIZE(buffer) >= sizeof(request)) {

        request = error = 0;

        /* if this is a MESG packet we want a DIALMSN_TERM */
        if (string_peek_uint32(buffer) & htonl(DIALMSN_MESG)) {
            if (string_finds(buffer, 0, (char *) & term, sizeof(term)) == -1)
                return;
        }

        request = ntohl(string_fetch_uint32(buffer));

        /* only allow authorised request */
        if ( (error = dialmsn_user_checkrequest(id, request)) ) {
            goto _err_abort;
        }

        /* ------------------------------------------------------------------ */
        /* AUTH */
        /* ------------------------------------------------------------------ */

        /* handle client requests */
        if (request & DIALMSN_AUTH) {
            /* initialize error */
            error = DIALMSN_AUTH | DIALMSN_AUTH_SERVER_ERR;

            if (request & DIALMSN_AUTH_CLIENT_PWD) {
                /* check if we can handle this client */
                if (dialmsn_user_count() >= DIALMSN_ONLINE_MAX) {
                    /* drop the excess connections with an error */
                    debug("DialMessenger::AUTH: too many users (max=%i).\n",
                          DIALMSN_ONLINE_MAX);
                    error |= DIALMSN_AUTH_ERR_ENOMEM;
                    goto _err_abort;
                }

                if ( (string_splits(buffer, DIALMSN_SEPARATOR,
                                    DIALMSN_SEPLENGTH) == -1) ||
                     (PARTS(buffer) < 5) ) {
                    debug("DialMessenger:AUTH: could not parse the request.\n");
                    error |= DIALMSN_AUTH_ERR_EINVAL;
                    goto _err_abort;
                }

                /* check if the protocol versions match */
                if (strncmp(DIALMSN_VERSION, TOKEN_DATA(buffer, 0),
                            TOKEN_SIZE(buffer, 0)) != 0) {
                    debug("DialMessenger:AUTH: bad protocol version.\n");
                    error |= DIALMSN_AUTH_ERR_EPROTO;
                    goto _err_abort;
                }

                /* login and password are limited to 32 chars */
                if ( (TOKEN_SIZE(buffer, 1) > DIALMSN_NICK_MAX) ||
                     (TOKEN_SIZE(buffer, 2) > DIALMSN_PASS_MAX) ) {
                    debug("DialMessenger::AUTH: login or password too long.\n");
                    error |= DIALMSN_AUTH_ERR_EINVAL;
                    goto _err_abort;
                }

                /* the packet seems ok, try the authentification */
                dialmsn_auth(id, buffer);

                string_flush(buffer);

            } else {
                /* bad request */
                debug("DialMessenger::AUTH: malformed request.\n");
                error |= DIALMSN_AUTH_ERR_EINVAL;
                goto _err_abort;
            }

        /* ------------------------------------------------------------------ */
        /* LIST */
        /* ------------------------------------------------------------------ */

        } else if (request & DIALMSN_LIST) {

            /* initialize error */
            error = DIALMSN_LIST | DIALMSN_LIST_ERROR;

            if (request & DIALMSN_LIST_QLIST) {

                /* if there is no online users there is no list to send... */
                if (! dialmsn_user_count()) {
                    error |= DIALMSN_LIST_ERR_EEMPTY;
                    goto _err_nonfatal;
                }

                if (request & DIALMSN_LIST_QUERY) {
                    /* client requests the connected users list */
                    if (_userlist_allow[id]) {
                        dialmsn_userlist_getconnected(id);
                        _userlist_allow[id] = 0;
                    }
                } else {
                    /* client requests her favourite or blocked users list */
                    dialmsn_userlist_getmisc(id, request);
                }

            } else if (request & (DIALMSN_LIST_ADD | DIALMSN_LIST_REM) ) {
                /* client want to add or remove a user from a list */
                uint32_t client = 0, user = 0, errflag = 0;
                const char *query = NULL;

                if (request & DIALMSN_LIST_ADD)
                    errflag = DIALMSN_LIST_ADD | DIALMSN_LIST_ADD_ERR;
                else if (request & DIALMSN_LIST_REM)
                    errflag = DIALMSN_LIST_REM | DIALMSN_LIST_REM_ERR;

                /* get the client uid */
                if (! (client = dialmsn_user_getuid(id)) ) {
                    debug("DialMessenger::LIST: client not found.\n");
                    error |= errflag; goto _err_nonfatal;
                }

                /* get the user to add/remove */
                if (SIZE(buffer) < sizeof(user)) {
                    debug("DialMessenger::LIST: missing parameter: user.\n");
                    error |= errflag; goto _err_nonfatal;
                }

                user = ntohl(string_fetch_uint32(buffer));

                con = dialmsn_db_borrow();

                if (request & DIALMSN_LIST_ADD) {
                    /* client wants to add a user to a list */
                    if (request & DIALMSN_LIST_ADD_FAV)
                        query = DIALMSN_SQL_ADDUSERFAV;
                    else if (request & DIALMSN_LIST_ADD_BLK)
                        query = DIALMSN_SQL_ADDUSERBLK;
                    db_exec(con, query, client, user);
                } else if (request & DIALMSN_LIST_REM) {
                    /* client wants to remove a user from a list */
                    if (request & DIALMSN_LIST_REM_FAV)
                        query = DIALMSN_SQL_DELUSERFAV;
                    else if (request & DIALMSN_LIST_REM_BLK)
                        query = DIALMSN_SQL_DELUSERBLK;
                    db_exec(con, query, client, user);
                }

                con = dialmsn_db_return(con);

                if ( (request & DIALMSN_LIST_ADD_BLK) ||
                     (request & DIALMSN_LIST_REM_BLK) ) {
                    dialmsn_user_refreshblockedlist(id);
                }

                if (request & DIALMSN_LIST_ADD) {
                    server_send_response(plugin_token, id, 0x0, "%bB4i%bB4i",
                                         DIALMSN_LIST | DIALMSN_LIST_ADD |
                                         DIALMSN_LIST_ADD_ACK,
                                         DIALMSN_TERM);
                } else if (request & DIALMSN_LIST_REM) {
                    server_send_response(plugin_token, id, 0x0, "%bB4i%bB4i",
                                         DIALMSN_LIST | DIALMSN_LIST_REM |
                                         DIALMSN_LIST_REM_ACK,
                                         DIALMSN_TERM);
                }
            }

        /* ------------------------------------------------------------------ */
        /* INFO */
        /* ------------------------------------------------------------------ */

        } else if (request & DIALMSN_INFO) {

            if (request & DIALMSN_INFO_GRT) {

                uint32_t uid = 0;
                uint32_t sid = 0;

                /* get the user to add/remove */
                if (SIZE(buffer) < sizeof(uid) + sizeof(sid)) {
                    debug("DialMessenger::INFO: missing parameter.\n");
                    error |= DIALMSN_INFO_ERR | DIALMSN_INFO_ERR_EINVAL;
                    goto _err_nonfatal;
                }

                uid = ntohl(string_fetch_uint32(buffer));
                sid = ntohl(string_fetch_uint32(buffer));

                /* insert the right */
                con = dialmsn_db_borrow();
                db_exec(con, DIALMSN_SQL_GRANTRIGHT,
                        dialmsn_user_getuid(id), uid);
                con = dialmsn_db_return(con);

                server_send_response(plugin_token, sid, 0x0,
                                     "%bB4i%i%bB4i",
                                     DIALMSN_INFO | DIALMSN_INFO_GRT, id,
                                     DIALMSN_TERM);

            } else if (request & DIALMSN_INFO_RVK) {

                uint32_t uid = 0;
                uint32_t sid = 0;

                /* get the user to add/remove */
                if (SIZE(buffer) < sizeof(uid) + sizeof(sid)) {
                    debug("DialMessenger::INFO: missing parameter.\n");
                    error |= DIALMSN_INFO_ERR | DIALMSN_INFO_ERR_EINVAL;
                    goto _err_nonfatal;
                }

                uid = ntohl(string_fetch_uint32(buffer));
                sid = ntohl(string_fetch_uint32(buffer));

                /* insert the right */
                con = dialmsn_db_borrow();
                db_exec(con, DIALMSN_SQL_RVOKERIGHT,
                        dialmsn_user_getuid(id), uid);
                con = dialmsn_db_return(con);

                server_send_response(plugin_get_token(), sid, 0x0,
                                     "%bB4i%i%bB4i",
                                     DIALMSN_INFO | DIALMSN_INFO_RVK, id,
                                     DIALMSN_TERM);

            } else if (request & DIALMSN_INFO_POP) {

                uint32_t uid = 0;
                uint32_t sid = 0;

                /* get the user to add/remove */
                if (SIZE(buffer) < sizeof(uid) + sizeof(sid)) {
                    debug("DialMessenger::INFO: missing parameter.\n");
                    error |= DIALMSN_INFO_ERR | DIALMSN_INFO_ERR_EINVAL;
                    goto _err_nonfatal;
                }

                uid = ntohl(string_fetch_uint32(buffer));
                sid = ntohl(string_fetch_uint32(buffer));

                /* reroute the simplified packet to the recipient */
                server_send_response(plugin_get_token(), sid, 0x0,
                                     "%bB4i%i%bB4i",
                                     DIALMSN_INFO | DIALMSN_INFO_POP, uid,
                                     DIALMSN_TERM);
            }

        /* ------------------------------------------------------------------ */
        /* MESG */
        /* ------------------------------------------------------------------ */

        } else if (request & DIALMSN_MESG) {
            uint32_t recipient = 0;
            uint32_t espeak = 0;
            m_string *msg = NULL;
            unsigned int pos = 0;

            /* initialize error */
            error = DIALMSN_MESG | DIALMSN_MESG_ERR;

            if (SIZE(buffer) < sizeof(recipient)) {
                debug("DialMessenger::MESG: missing parameter: recipient.\n");
                error |= DIALMSN_MESG_ERR_EINVAL; goto _err_nonfatal;
            }

            /* get the recipient session id */
            recipient = ntohl(string_fetch_uint32(buffer));

            /* check if the recipient is online */
            if (! (dialmsn_user_available(recipient)) ) {
                debug("DialMessenger::MESG: user is either "
                      "offline, or unavailable.\n");
                error |= DIALMSN_MESG_ERR_EAGAIN; goto _err_mesg;
            }

            /* check if the user has the right to speak to this person */
            if ( (espeak = dialmsn_user_canspeak(id, recipient)) ) {
                debug("DialMessenger::MESG: user is either "
                      "not a subscriber, or blocked.\n");
                error |= espeak; goto _err_mesg;
            }

            /* find the termination marker */
            pos = string_finds(buffer, 0, (char *) & term, sizeof(term));

            /* encaps the buffer and split it */
            if (! (msg = string_fetch(buffer, pos)) ) {
                debug("DialMessenger::MESG: out of memory.\n");
                error |= DIALMSN_MESG_ERR_ENOMEM; goto _err_mesg;
            }

            /* check if there is some plaintext and the logging is enabled */
            if (option_logging &&
                ! string_splits(msg, DIALMSN_SEPARATOR, DIALMSN_SEPLENGTH)) {
                /* log the conversation if logging is enabled */
                if ( (request & DIALMSN_MESG_TXT) &&
                     (PARTS(msg) > 1) && (TOKEN_SIZE(msg, 1) > 1) ) {
                    /* record only non NULL messages */
                    con = dialmsn_db_borrow();
                    db_exec(con, DIALMSN_SQL_LOGCONVERS,
                            dialmsn_user_getuid(id),
                            dialmsn_user_getuid(recipient),
                            TOKEN_SIZE(msg, 1),
                            TOKEN_DATA(msg, 1));
                    con = dialmsn_db_return(con);
                }
            }

            /* build the packet */
            server_send_response(plugin_token, recipient, 0x0,
                                 "%bB4i%i%.*s%.*s%bB4i",
                                 DIALMSN_MESG | DIALMSN_MESG_RTF,
                                 id, DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                                 SIZE(msg), DATA(msg), DIALMSN_TERM);

            msg = string_free(msg);

            /* don't forget to clean up the separator */
            if (string_fetch_uint32(buffer) != term)
                debug("DialMessenger::MESG: missing footer.\n");

            continue;

        _err_mesg:
            con = dialmsn_db_return(con);

            string_flush(buffer);

            server_send_response(plugin_token, id, 0x0, "%bB4i%i%bB4i",
                                 error, recipient, DIALMSN_TERM);

        /* ------------------------------------------------------------------ */
        /* MAIL */
        /* ------------------------------------------------------------------ */

        } else if (request & DIALMSN_MAIL) {

            error = DIALMSN_MAIL | DIALMSN_MAIL_ERR;

            if (request & DIALMSN_MAIL_CNT) {
                /* get the number of unread mails */
                uint32_t uid = 0;
                m_recordset *r = NULL;
                int mailbox_count = 0;

                uid = dialmsn_user_getuid(id);

                con = dialmsn_db_borrow();

                /* check whether the user is the sender or the receiver */
                if (! (r = db_query(con, DIALMSN_SQL_GETMAILCNT, uid)) ) {
                    debug("DialMessenger::MAIL: SQL failed.\n");
                    error |= DIALMSN_MAIL_ERR_EINVAL; goto _err_nonfatal;
                }

                con = dialmsn_db_return(con);

                mailbox_count = db_integer(r, "mailbox_count");

                r = db_free(r);

                server_send_response(plugin_token, id, 0x0, "%bB4i%i%bB4i",
                                     DIALMSN_MAIL | DIALMSN_MAIL_CNT,
                                     mailbox_count, DIALMSN_TERM);
            }

        /* ------------------------------------------------------------------ */
        /* EXEC */
        /* ------------------------------------------------------------------ */

        } else if (request & DIALMSN_EXEC) {

            /* initialize error */
            error = DIALMSN_EXEC | DIALMSN_EXEC_ERR;

            if (request & DIALMSN_EXEC_ADDACCOUNT) {
                m_recordset *r = NULL;
                char host[NI_MAXHOST];
                const char *affiliation = "Non-affiliated customer";

                error |= DIALMSN_EXEC_ADDACCOUNT;

                if (string_splits(buffer, DIALMSN_SEPARATOR,
                                  DIALMSN_SEPLENGTH) == -1) {
                    debug("DialMessenger::ADDACCOUNT: out of memory (1).\n");
                    goto _err_nonfatal;
                }

                if (PARTS(buffer) < 9) {
                    debug("DialMessenger::ADDACCOUNT: bad parameters.\n");
                    goto _err_nonfatal;
                }

                /* check if the nickname was already taken */
                if (dialmsn_user_chknickname(TOKEN_DATA(buffer, 0),
                                             TOKEN_SIZE(buffer, 0))) {
                    debug("DialMessenger::ADDACCOUNT: nick already in use.\n");
                    goto _err_nonfatal;
                }

                con = dialmsn_db_borrow();

                /* get the client IP */
                socket_ip(id, host, sizeof(host), NULL);

                if (TOKEN_SIZE(buffer, 8) && *(TOKEN_DATA(buffer, 8)))
                    affiliation = TOKEN_DATA(buffer, 8);

                r = db_query(con, DIALMSN_SQL_ADDACCOUNT,
                             TOKEN_SIZE(buffer, 2), TOKEN_DATA(buffer, 2), /* email */
                             TOKEN_SIZE(buffer, 0), TOKEN_DATA(buffer, 0), /* login */
                             TOKEN_SIZE(buffer, 1), TOKEN_DATA(buffer, 1), /* password */
                             atoi(TOKEN_DATA(buffer, 3)),                  /* sex */
                             TOKEN_SIZE(buffer, 5), TOKEN_SIZE(buffer, 5), /* birthdate */
                             host,                                         /* ip */
                             affiliation);                                 /* affiliation */

                con = dialmsn_db_return(con);

                if (! r) {
                    debug("DialMessenger::ADDACCOUNT: SQL failed.\n");
                    goto _err_nonfatal;
                }

                /* success */
                r = db_free(r);

                server_send_response(plugin_token, id, SERVER_MSG_END,
                                     "%bB4i%bB4i",
                                     DIALMSN_EXEC | DIALMSN_EXEC_ADDACCOUNT,
                                     DIALMSN_TERM);

                string_flush(buffer);

                return;

            } else if (request & DIALMSN_EXEC_CHNGSTATUS) {

                uint32_t status = 0;

                if (SIZE(buffer) >= sizeof(status)) {
                    status = ntohl(string_fetch_uint32(buffer));
                    dialmsn_user_setstatus(id, status);
                } else string_flush(buffer);

            } else if (request & DIALMSN_EXEC_WEBCAMCALL) {

                uint32_t param = 0, caller = 0, callee = 0, port = 0;
                char ip[NI_MAXHOST];

                error |= DIALMSN_EXEC_ERR_EINVAL;

                if (SIZE(buffer) < sizeof(param) + sizeof(callee) +
                    sizeof(caller) + sizeof(port)) {
                    debug("DialMessenger::WEBCAMCALL: bad parameters.\n");
                    goto _err_nonfatal;
                }

                /* simply forward the param */
                param = ntohl(string_fetch_uint32(buffer));
                caller = ntohl(string_fetch_uint32(buffer));
                callee = ntohl(string_fetch_uint32(buffer));
                port = ntohl(string_fetch_uint32(buffer));

                if (! socket_ip(caller, ip, sizeof(ip), NULL)) {
                    /* send ip and session id of the caller to the callee */
                    server_send_response(plugin_token, callee, 0x0,
                                         "%bB4i%i%.*s%i%.*s%s%.*s%i%bB4i",
                                         DIALMSN_EXEC | DIALMSN_EXEC_WEBCAMCALL,
                                         param,
                                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                                         caller,
                                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                                         ip,
                                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                                         port, DIALMSN_TERM);
                }

            } else if (request & DIALMSN_EXEC_REFRESHUSR) {
                uint32_t uid = 0;
                uint32_t changes = 0;

                if (SIZE(buffer) < sizeof(uid) + sizeof(changes)) {
                    debug("DialMessenger::REFRESHUSR: bad parameters.\n");
                    string_flush(buffer);
                    return;
                }

                /* get the user session id and changes */
                uid = ntohl(string_fetch_uint32(buffer));
                changes = ntohl(string_fetch_uint32(buffer));

                debug("DialMessenger::REFRESHUSR: refreshing user #%i.\n", uid);

                /* refresh the user informations */
                dialmsn_user_refresh(uid, changes);

            } else if (request & DIALMSN_EXEC_HOSTONLINE) {
                uint32_t uid = 0;

                uid = ntohl(string_fetch_uint32(buffer));

                debug("DialMessenger::HOSTONLINE: hostess #%i ready.\n", uid);

                /* bring the hostess online */
                dialmsn_hostess_connect(uid);
            }
        }

        con = dialmsn_db_return(con);
    }

    return;

_err_abort:
    con = dialmsn_db_return(con);

    string_flush(buffer);

    server_send_response(plugin_token, id, SERVER_MSG_END, "%bB4i%bB4i",
                         error, DIALMSN_TERM);

    return;

_err_nonfatal:
    con = dialmsn_db_return(con);

    string_flush(buffer);

    server_send_response(plugin_token, id, 0x0, "%bB4i%bB4i",
                         error, DIALMSN_TERM);
}

/* -------------------------------------------------------------------------- */

public void plugin_event_handler(uint16_t id, UNUSED uint16_t ingress,
                                 unsigned int event, UNUSED void *event_data)
{
    switch (event) {

    case PLUGIN_EVENT_INCOMING_CONNECTION:
        _userlist_allow[id] = 1;
        break;

    case PLUGIN_EVENT_OUTGOING_CONNECTION:
        /* connected to remote host */ break;

    case PLUGIN_EVENT_SOCKET_DISCONNECTED: /* connection closed */
        dialmsn_user_disconnect(id);
        break;

    case PLUGIN_EVENT_REQUEST_TRANSMITTED:
        _userlist_allow[id] = 1;
        break;

    case PLUGIN_EVENT_SERVER_SHUTTINGDOWN:
        /* server shutting down */ break;

    default: fprintf(stderr, "DialMessenger: spurious event.\n");

    }
}

/* -------------------------------------------------------------------------- */

public void plugin_exit(void)
{
    fprintf(stderr, "DialMessenger: unloading...\n");

    if (option_hostess) {
        fprintf(stderr, "DialMessenger: stopping the Eurolive "
                "hostess service...\n");
        dialmsn_hostess_exit();
        free(eurolive_feed);
    }

    if (option_bot) {
        fprintf(stderr, "DialMessenger: stopping the bot...\n");
        dialmsn_bot_exit();
    }

    dialmsn_userlist_clear();
    dialmsn_db_exit();

    free(http_host);

    fprintf(stderr, "DialMessenger: succesfully unloaded.\n");
}

/* -------------------------------------------------------------------------- */
