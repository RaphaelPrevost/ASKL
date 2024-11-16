/*******************************************************************************
 *  Dialflirt Messenger                                                        *
 *  Copyright (c) 2005-2019 Raphael Prevost <raph@el.bzh>                      *
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

#define DIALMSN_SQL_UPDATESESS "CALL set_user_session(%i, %i, %s, %s)"
#define DIALMSN_SQL_CLRUSRSESS "CALL clear_user_session(%i)"
#define DIALMSN_SQL_GETUSERFAV "CALL get_user_favourite(%i)"
#define DIALMSN_SQL_GETUSERBLK "CALL get_user_blacklist(%i)"
#define DIALMSN_SQL_LOADBLOCKD "CALL load_user_blacklist(%i)"
#define DIALMSN_SQL_CHKUSRNICK "CALL check_user_nickname(%.*s)"
#define DIALMSN_SQL_GETPROFILE "CALL get_user_profile(%i)"
#define DIALMSN_SQL_CHANGENICK "CALL update_user_nickname(%i, %.*s)"
#define DIALMSN_SQL_REFRESHUSR "CALL get_user_updated_info(%i)"

static const char *getuserfav = DIALMSN_SQL_GETUSERFAV;
static const char *getuserblk = DIALMSN_SQL_GETUSERBLK;

/* private user cache */
static pthread_rwlock_t userlock = PTHREAD_RWLOCK_INITIALIZER;
static dialmsn_user *users[DIALMSN_USER_MAX];
static m_view *cache = NULL;
static unsigned int online = 0;
static unsigned int women_online = 0;
static unsigned int visible_men = 0;

/* simulated users id ( > SOCKET_MAX) */
static pthread_mutex_t _id_lock = PTHREAD_MUTEX_INITIALIZER;
static uint16_t _id = SOCKET_MAX;
static unsigned int _free_count = 0;
static uint16_t _free_id[SOCKET_MAX];

/* -------------------------------------------------------------------------- */

private uint32_t dialmsn_user_checkrequest(uint16_t id, uint32_t request)
{
    uint32_t error = 0;

    /* reject all non authentified requests */
    pthread_rwlock_rdlock(& userlock);

        if ( (! users[id]) && (~request & DIALMSN_AUTH)
             && (request != (DIALMSN_EXEC | DIALMSN_EXEC_ADDACCOUNT) )
             && (request != (DIALMSN_EXEC | DIALMSN_EXEC_REFRESHUSR) )
             && (request != (DIALMSN_EXEC | DIALMSN_EXEC_HOSTONLINE) )
             && (request != (DIALMSN_INFO | DIALMSN_INFO_POP) )
           ) {
            debug("DialMessenger: rejecting non authentified request.\n");
            error = DIALMSN_AUTH |
                    DIALMSN_AUTH_SERVER_ERR |
                    DIALMSN_AUTH_ERR_EINVAL;
        }

    pthread_rwlock_unlock(& userlock);

    return error;
}

/* -------------------------------------------------------------------------- */

private unsigned int dialmsn_user_count(void)
{
    unsigned int ret = 0;

    pthread_rwlock_rdlock(& userlock);
        ret = online;
    pthread_rwlock_unlock(& userlock);

    return ret;
}

/* -------------------------------------------------------------------------- */

private dialmsn_user *dialmsn_user_add(uint16_t session, dialmsn_user *user,
                                       const char *ver)
{
    m_db *con = NULL;
    m_recordset *r = NULL;
    char host[NI_MAXHOST];
    unsigned int i = 0;
    uint32_t err = 0;

    if (! user || ! user->uid || session >= DIALMSN_USER_MAX) {
        debug("dialmsn_user_add(): bad parameters.\n");
        return NULL;
    }

    /* get the client IP */
    if (strcmp(ver, "SEXBOT") != 0)
        socket_ip(session, host, sizeof(host), NULL);
    else
        memcpy(host, "127.0.0.1", strlen("127.0.0.1") + 1);

    /* add the session */
    con = dialmsn_db_borrow();
    r = db_query(con, DIALMSN_SQL_UPDATESESS, session, user->uid, host, ver);
    con = dialmsn_db_return(con);

    if (! r || ! r->rows || db_integer(r, "ret") != 1) {
        err = DIALMSN_AUTH | DIALMSN_AUTH_SERVER_ERR | DIALMSN_AUTH_ERR_EINVAL;
        server_send_response(plugin_get_token(), session, SERVER_MSG_END,
                             "%bB4i%bB4i", err, DIALMSN_TERM);
        r = db_free(r);
        return user;
    }

    r = db_free(r);

    pthread_rwlock_wrlock(& userlock);

        /* remove any other user data and replace them */
        users[session] = dialmsn_user_free(users[session]);
        users[session] = user; online ++;

        if (! plugin_quotas_enabled())
            user->visible = 1;
        else switch (user->sex) {
        /* if the user is a male, we only allow 10% visibility */
        case 0:
            user->visible = 0;
            if (visible_men <= (women_online * 10) / 100) {
                user->visible = 1;
                visible_men ++;
            }
            break;
        /* if the user is a woman, set it visible by default */
        case 1:
            user->visible = 1; women_online ++;

            /* ensure the male population is ok */
            if (visible_men <= (women_online * 10) / 100) {
                for (i = 0; i < DIALMSN_USER_MAX; i ++) {
                    if (users[i] && ! (users[i]->sex + users[i]->visible)) {
                        visible_men ++; users[i]->visible = 1;
                        break;
                    }
                }
            }
            break;
        /* couples are always visible */
        case 2: user->visible = 1; break;
        }

        dialmsn_userlist_refresh();

    pthread_rwlock_unlock(& userlock);

    return NULL;
}

/* -------------------------------------------------------------------------- */

private uint16_t dialmsn_user_simulate(uint32_t uid, unsigned int status)
{
    m_db *con = NULL;
    m_recordset *r = NULL;
    dialmsn_user *user = NULL;
    size_t size = 0;
    const char *nick = NULL, *city = NULL, *avatar = NULL;
    uint16_t simid = 0;

    if (! uid) return 0;

    con = dialmsn_db_borrow();

    r = db_query(con, DIALMSN_SQL_GETPROFILE, uid);

    if (! r) {
        debug("DialMessenger::dialmsn_user_simulate(): SQL error (0).\n");
        con = dialmsn_db_return(con);
        return 0;
    }

    if (! (uid = db_integer(r, "uid")) ) {
        /* incorrect uid */
        debug("DialMessenger::dialmsn_user_simulate(): incorrect UID.\n");
        r = db_free(r);
        con = dialmsn_db_return(con);
        return 0;
    }

    /* check if the user has a previous session */
    if (db_integer(r, "session")) {
        /* we can't simulate this user, she is already online ! */
        debug("DialMessenger::dialmsn_user_simulate(): user online.\n");
        r = db_free(r);
        con = dialmsn_db_return(con);
        return 0;
    }

    /* build a new user session */
    if (! (user = malloc(sizeof(*user))) ) {
        debug("DialMessenger::dialmsn_user_simulate(): Out of memory (0).\n");
        r = db_free(r);
        con = dialmsn_db_return(con);
        return 0;
    }

    user->uid = uid;

    /* no need for a blacklist */
    user->busers = NULL;

    /* get a simulated user id */
    pthread_mutex_lock(& _id_lock);

        if (_free_count) {
            simid = _free_id[-- _free_count];
        } else if (_id < DIALMSN_USER_MAX) {
            /* allocate a new id */
            simid = _id ++;
        }

    pthread_mutex_unlock(& _id_lock);

    if (! simid) {
        debug("DialMessenger::dialmsn_user_simulate(): no more ID available.\n");
        r = db_free(r); free(user->busers); free(user);
        con = dialmsn_db_return(con);
        return 0;
    }

    nick = db_field(r, "nickname", & size);
    user->nick = strndup(nick, size + 1);
    avatar = db_field(r, "avatar", & size);
    user->avatar = strndup(avatar, size + 1);
    user->lobby = db_integer(r, "lobby");
    user->status = status;
    user->subscriber = 1;

    user->sex = db_integer(r, "sex");
    user->dpt = db_integer(r, "zip") / 1000;
    user->age = db_integer(r, "age");
    city = db_field(r, "city", & size);
    user->city = strndup(city, size + 1);

    user = dialmsn_user_add(simid, user, "SEXBOT");

    r = db_free(r);

    con = dialmsn_db_return(con);

    if (user) {
        debug("DialMessenger::dialmsn_user_simulate(): failed to add user.\n");

        user = dialmsn_user_free(user);

        /* release the identifier */
        pthread_mutex_lock(& _id_lock);

            _free_id[_free_count ++] = simid;

        pthread_mutex_unlock(& _id_lock);

        simid = 0;
    }

    return simid;
}

/* -------------------------------------------------------------------------- */

private void dialmsn_user_endsim(uint16_t session)
{
    dialmsn_user_disconnect(session);

    /* release the identifier */
    pthread_mutex_lock(& _id_lock);

        _free_id[_free_count ++] = session;

    pthread_mutex_unlock(& _id_lock);
}

/* -------------------------------------------------------------------------- */

private void dialmsn_user_disconnect(uint16_t session)
{
    m_db *con = NULL;
    unsigned int i = 0;

    con = dialmsn_db_borrow();
    db_exec(con, DIALMSN_SQL_CLRUSRSESS, session);
    con = dialmsn_db_return(con);

    if (session >= DIALMSN_USER_MAX) return;

    /* cleanup the user data and invalidate the list cache */
    pthread_rwlock_wrlock(& userlock);
        if (users[session]) {

            if (plugin_quotas_enabled()) switch (users[session]->sex) {
            /* if the user is a male, we enforce 10% visibility */
            case 0:
                if (users[session]->visible) visible_men --;
                /* ensure the male population is ok */
                if (visible_men <= (women_online * 10) / 100) {
                    for (i = 0; i < DIALMSN_USER_MAX; i ++) {
                        if (i == session) continue;
                        if (users[i] && ! (users[i]->sex + users[i]->visible)) {
                            visible_men ++; users[i]->visible = 1;
                            break;
                        }
                    }
                }
                break;
            /* if the user is a woman, set it visible by default */
            case 1:
                if (women_online) women_online --;

                /* ensure the male population is ok */
                if (visible_men > (women_online * 10) / 100) {
                    for (i = 0; i < DIALMSN_USER_MAX; i ++) {
                        if (users[i] && ! users[i]->sex && users[i]->visible) {
                            if (visible_men) visible_men --;
                            users[i]->visible = 0;
                            break;
                        }
                    }
                }
                break;
            }

            users[session] = dialmsn_user_free(users[session]);
            online = (online) ? online - 1 : 0;
            dialmsn_userlist_refresh();
        }
    pthread_rwlock_unlock(& userlock);

    return;
}

/* -------------------------------------------------------------------------- */

private dialmsn_user *dialmsn_user_free(dialmsn_user *user)
{
    if (user) {
        free(user->nick);
        free(user->city);
        free(user->avatar);
        free(user->busers);
        free(user);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

private uint32_t dialmsn_user_getuid(uint16_t session)
{
    uint32_t ret = 0;

    if (session >= DIALMSN_USER_MAX) return 0;

    pthread_rwlock_rdlock(& userlock);

        if (users[session]) ret = users[session]->uid;

    pthread_rwlock_unlock(& userlock);

    return ret;
}

/* -------------------------------------------------------------------------- */

private int dialmsn_user_available(uint16_t session)
{
    int ret = 0;

    if (session >= DIALMSN_USER_MAX) return 0;

    pthread_rwlock_rdlock(& userlock);

        if (users[session] && users[session]->status != DIALMSN_STATUS_UNAV)
            ret = 1;

    pthread_rwlock_unlock(& userlock);

    return ret;
}


/* -------------------------------------------------------------------------- */

private void dialmsn_user_setstatus(uint16_t session, uint32_t status)
{
    if (session < DIALMSN_USER_MAX && status <= DIALMSN_STATUS_UNAV) {
        pthread_rwlock_wrlock(& userlock);

            if (users[session]) {
                users[session]->status = status;
                dialmsn_userlist_refresh();
            }

        pthread_rwlock_unlock(& userlock);
    }
}

/* -------------------------------------------------------------------------- */

private void dialmsn_user_refresh(uint32_t uid, uint32_t refresh_all)
{
    m_db *con = NULL;
    m_recordset *r = NULL;
    uint16_t session = 0;
    unsigned int subscription = 0;
    const char *field = NULL;
    size_t size = 0;
    char *new_nick = NULL, *new_avatar = NULL;
    char sex = 0, dpt = 0, age = 0, fantasy_enabled = 0;

    con = dialmsn_db_borrow();
    r = db_query(con, DIALMSN_SQL_REFRESHUSR, uid);
    con = dialmsn_db_return(con);

    if (! r) {
        debug("DialMessenger::REFRESHUSR: SQL failed.\n");
        return;
    }

    /* get the session and the subscription informations */
    session = db_integer(r, "session");

    if (session >= DIALMSN_USER_MAX) {
        r = db_free(r);
        return;
    }

    subscription = (db_integer(r, "expiration") > 0) ? 1 : 0;

    if (refresh_all) {
        field = db_field(r, "avatar", & size);
        new_avatar = strndup(field, size + 1);
        field = db_field(r, "nickname", & size);
        new_nick = strndup(field, size + 1);
        sex = db_integer(r, "sex");
        dpt = db_integer(r, "zip") / 1000;
        age = db_integer(r, "age");
        fantasy_enabled = db_integer(r, "fantasy_enabled");
    }

    r = db_free(r);

    pthread_rwlock_wrlock(& userlock);

        if (users[session]) {
            users[session]->subscriber = subscription;

            if (refresh_all) {

                if (new_nick) {
                    free(users[session]->nick);
                    users[session]->nick = new_nick;
                }

                if (new_avatar) {
                    free(users[session]->avatar);
                    users[session]->avatar = new_avatar;
                }

                users[session]->sex = sex;
                users[session]->dpt = dpt;
                users[session]->age = age;
            }

            dialmsn_userlist_refresh();
        }

    pthread_rwlock_unlock(& userlock);

    server_send_response(plugin_get_token(), session, 0x0,
                         "%bB4i%i%.*s%i%.*s%.*s%.*s%i%.*s%i%bB4i",
                         DIALMSN_EXEC | DIALMSN_EXEC_REFRESHUSR,
                         subscription,
                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                         refresh_all,
                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                         size, new_nick,
                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                         sex,
                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                         fantasy_enabled, DIALMSN_TERM);
}

/* -------------------------------------------------------------------------- */

private uint32_t dialmsn_user_getstatus(uint16_t session)
{
    uint32_t ret = 0;

    if (session >= DIALMSN_USER_MAX) return 0;

    pthread_rwlock_rdlock(& userlock);

        if (users[session]) ret = users[session]->status;

    pthread_rwlock_unlock(& userlock);

    return ret;
}

/* -------------------------------------------------------------------------- */

private uint32_t dialmsn_user_chknickname(const char *nick, size_t len)
{
    m_db *con = NULL;
    m_recordset *r = NULL;
    uint32_t uid = 0;

    con = dialmsn_db_borrow();
    r = db_query(con, DIALMSN_SQL_CHKUSRNICK, len, nick);
    con = dialmsn_db_return(con);
    if (! r) return 0;

    uid = db_integer(r, "uid");

    r = db_free(r);

    return uid;
}

/* -------------------------------------------------------------------------- */

private int dialmsn_user_canspeak(uint16_t session, uint16_t recipient)
{
    unsigned int i = 0;
    int ret = 0;

    if (session >= DIALMSN_USER_MAX || recipient >= DIALMSN_USER_MAX)
        return 0;

    pthread_rwlock_rdlock(& userlock);

        if (users[session] && users[recipient]) {

            if (/* men must pay to talk to anybody */
                 (users[session]->sex == 0) ||
                /* women are free to speak to anyone, except lesbians */
                 (users[session]->sex == 1 && users[recipient]->sex == 1) ||
                /* couples can only talk for free to men */
                 (users[session]->sex == 2 && users[recipient]->sex != 0)
               ) {
                /* check if the client is subscriber */
                if (! users[session]->subscriber)
                    ret = DIALMSN_MESG_ERR_ENOSUB;
            }

            /* check if the recipient has blocked this person */
            if (users[recipient]->busers) {
                for (i = 0; i < users[recipient]->bcount; i ++) {
                    if (users[recipient]->busers[i] == users[session]->uid)
                        ret = DIALMSN_MESG_ERR_EBLOCK;
                }
            }
        }

    pthread_rwlock_unlock(& userlock);

    return ret;
}

/* -------------------------------------------------------------------------- */

private void dialmsn_user_refreshblockedlist(uint16_t id)
{
    m_db *con = NULL;
    m_recordset *r = NULL;
    uint32_t error = 0, errflag = 0, size = 0;

    con = dialmsn_db_borrow();

    /* rebuild the user cache */
    r = db_query(con, DIALMSN_SQL_LOADBLOCKD, dialmsn_user_getuid(id));

    con = dialmsn_db_return(con);

    if (! r) {
        debug("DialMessenger::LIST: SQL error.\n");
        error |= errflag;
        goto _err_nonfatal;
    }

    pthread_rwlock_wrlock(& userlock);

        if (users[id]) {

            free(users[id]->busers); users[id]->busers = NULL;

            users[id]->bcount = r->rows;

            if (r->rows) {

                if (! (users[id]->busers =
                       malloc(r->rows * sizeof(*users[id]->busers))) ) {
                    debug("DialMessenger::LIST: Out of memory (0).\n");
                    r = db_free(r);
                    pthread_rwlock_unlock(& userlock);
                    error |= errflag;
                    goto _err_nonfatal;
                }

                do users[id]->busers[size ++] = db_integer(r, "blk");
                while (db_movenext(r) != -1);
            }
        }

    pthread_rwlock_unlock(& userlock);

    r = db_free(r);

    return;

_err_nonfatal:
    con = dialmsn_db_return(con);

    server_send_response(plugin_get_token(), id, 0x0,
                         "%bB4i%bB4i", error, DIALMSN_TERM);
}

/* -------------------------------------------------------------------------- */

private int dialmsn_userlist_initcache(void)
{
    if (! (cache = fs_openview("/dialflirt", strlen("/dialflirt"))) )
        return -1;

    return 0;
}

/* -------------------------------------------------------------------------- */

private void dialmsn_userlist_refresh(void)
{
    m_string *list = NULL, *data = NULL, *buffer = NULL;
    unsigned int i = 0, records = 0;

    if (! (list = string_alloc(NULL, (online + 1) * 256)) ) {
        debug("DialMessenger::LISTCACHE: out of memory (0).\n");
        return;
    }

    for (i = 1; i < DIALMSN_USER_MAX && records < online; i ++) {

        if (! users[i]) continue;

        /* build the datagram line */
        buffer = string_catfmt(
            list,
            "%i%.*s%i%.*s%s%.*s%i%.*s%i%.*s%i"
            "%.*s%i%.*s%s%.*s%s%.*s%i%.*s%i%.*s",
            i,                    /* session id */
            DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
            users[i]->uid,        /* user id */
            DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
            users[i]->nick,       /* nickname */
            DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
            users[i]->status,     /* status */
            DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
            users[i]->sex,        /* sex */
            DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
            users[i]->age,        /* age */
            DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
            users[i]->dpt,        /* postal code */
            DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
            users[i]->city,       /* city */
            DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
            users[i]->avatar,     /* avatar */
            DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
            users[i]->subscriber, /* subscriber ? */
            DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
            users[i]->visible,
            DIALMSN_LINESEPLENGTH,
            DIALMSN_LINESEPARATOR
        );

        if (! buffer) {
            debug("DialMessenger::LISTCACHE: out of memory (1).\n");
            list = string_free(list);
            return;
        }

        records ++;
    }

    if (! records) { string_free(list); return; }

    /* compress the list */
    data = string_compress(list);

    list = string_fmt(list, "%bB4i%bB4i%bB4i%.*bs%bB4i",
                      DIALMSN_LIST | DIALMSN_LIST_REPLY,
                      SIZE(list), SIZE(data), SIZE(data), DATA(data),
                      DIALMSN_TERM);

    /* update the cache */
    fs_remap(cache, "list", strlen("list"), list);

    data = string_free(data);
}

/* -------------------------------------------------------------------------- */

private void dialmsn_userlist_freecache(void)
{
    fs_closeview(cache);
}

/* -------------------------------------------------------------------------- */

private void dialmsn_userlist_getconnected(uint16_t session)
{
    m_reply *reply = NULL;
    m_file *list = NULL;

    pthread_rwlock_rdlock(& userlock);

    if (cache) {
        debug("DialMessenger::LISTCACHE: fetching from cache.\n");
        if (! (list = fs_openfile(cache, "list", strlen("list"), NULL)) )
            goto _err_enomem;

        if (! (reply = server_reply_init(SERVER_MSG_ACK, plugin_get_token())) )
            goto _err_reply;

        if (server_reply_setfile(reply, list, 0, 0) == -1)
            goto _err_setfile;

        server_send_reply(session, reply);
    } else {
        debug("DialMessenger::LISTCACHE: out of memory (2).\n");
        goto _err_enomem;
    }

    pthread_rwlock_unlock(& userlock);

    return;

_err_setfile:
    reply = server_reply_free(reply);
_err_reply:
    list = fs_closefile(list);
_err_enomem:
    pthread_rwlock_unlock(& userlock);

    server_send_response(plugin_get_token(), session, 0x0, "%bB4i%bB4i",
                         DIALMSN_LIST | DIALMSN_LIST_ERROR |
                         DIALMSN_LIST_ERR_ENOMEM, DIALMSN_TERM);
}

/* -------------------------------------------------------------------------- */

private void dialmsn_userlist_getmisc(uint16_t session, uint32_t request)
{
    m_db *con = NULL;
    m_recordset *r = NULL;
    uint32_t uid = 0, header = 0, error = DIALMSN_LIST | DIALMSN_LIST_ERROR;
    int favsession = 0;
    m_string *list = NULL;
    char buffer[1024];
    size_t size = 0;
    const char *query = NULL;
    unsigned int count = 0;
    uint32_t term = htonl(DIALMSN_TERM);

    header = DIALMSN_LIST;
    if (request & DIALMSN_LIST_QFLST) {
        /* client requests her favourite user list */
        header |= DIALMSN_LIST_RFLST;
        query = getuserfav;
    } else if (request & DIALMSN_LIST_QBLST) {
        /* client requests her blocked user list */
        header |= DIALMSN_LIST_RBLST;
        query = getuserblk;
    } else {
        debug("DialMessenger::LIST: bad request.\n");
        error |= DIALMSN_LIST_ERR_EINVAL;
        goto _err_abort;
    }

    header = htonl(header);

    if (! (uid = dialmsn_user_getuid(session)) ) {
        debug("DialMessenger::LIST: could not get user UID.\n");
        error |= DIALMSN_LIST_ERR_EINVAL;
        goto _err_abort;
    }

    if (! (list = string_from_uint32(header)) ) {
        debug("DialMessenger::LIST: out of memory (0).\n");
        error |= DIALMSN_LIST_ERR_ENOMEM;
        goto _err_abort;
    }

    con = dialmsn_db_borrow();

    if (! (r = db_query(con, query, uid)) ) {
        debug("DialMessenger::LIST: SQL error.\n");
        error |= DIALMSN_LIST_ERR_EINVAL;
        list = string_free(list);
        goto _err_abort;
    }

    con = dialmsn_db_return(con);

    if (! r->rows) {
        debug("DialMessenger::LIST: empty users list.\n");
        error |= DIALMSN_LIST_ERR_EEMPTY;
        r = db_free(r); list = string_free(list);
        goto _err_abort;
    }

    /* we got at least one row */
    do {
        if ( (favsession = db_integer(r, "session")) != -1) {
            /* build a datagram line */
            size = m_snprintf(buffer, sizeof(buffer),
                              "%i%.*s%i%.*s%s%.*s%i%.*s%i%.*s%i"
                              "%.*s%i%.*s%s%.*s%s%.*s%i%.*s",
                              favsession,                      /* session id */
                              DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                              db_integer(r, "uid"),            /* user id */
                              DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                              db_field(r, "nickname", NULL),   /* nickname */
                              DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                              dialmsn_user_getstatus(favsession),/* status */
                              DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                              db_integer(r, "sex"),            /* sex */
                              DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                              db_integer(r, "age"),            /* age */
                              DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                              db_integer(r, "zip") / 1000,     /* postal code */
                              DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                              db_field(r, "city", NULL),       /* city */
                              DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                              db_field(r, "avatar", NULL),     /* avatar */
                              DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                              db_integer(r, "expiration"),     /* subscriber */
                              DIALMSN_LINESEPLENGTH, DIALMSN_LINESEPARATOR);

            if (! string_cats(list, buffer, size)) {
                debug("DialMessenger::LIST: out of memory (1).\n");
                r = db_free(r); list = string_free(list);
                error |= DIALMSN_LIST_ERR_ENOMEM;
                goto _err_abort;
            }

            count ++;
        }
    } while (db_movenext(r) != -1);

    r = db_free(r);

    if (count) {
        if (! string_cats(list, (char *) & term, sizeof(term))) {
            debug("DialMessenger::LIST: out of memory (2).\n");
            list = string_free(list);
            error |= DIALMSN_LIST_ERR_ENOMEM;
            goto _err_abort;
        }

        server_send_buffer(plugin_get_token(), session, 0x0,
                           DATA(list), SIZE(list));
    } else {
        debug("DialMessenger::LIST: all users from the list are offline.\n");
        error |= DIALMSN_LIST_ERR_EEMPTY;
        list = string_free(list);
        goto _err_abort;
    }

    list = string_free(list);

    return;

_err_abort:
    con = dialmsn_db_return(con);

    server_send_response(plugin_get_token(), session, 0x0, "%bB4i%bB4i",
                         error, DIALMSN_TERM);
}

/* -------------------------------------------------------------------------- */

private void dialmsn_userlist_clear(void)
{
    unsigned int i = 0;

    /* unload the user cache */
    for (i = 0; i < DIALMSN_USER_MAX; i ++) dialmsn_user_free(users[i]);

    dialmsn_userlist_freecache();
}

/* -------------------------------------------------------------------------- */
