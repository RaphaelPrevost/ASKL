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

#define DIALMSN_SQL_LOGININFOS "CALL get_user_login_info(%.*s)"
#define DIALMSN_SQL_LOADBLOCKD "CALL get_user_blacklist(%i)"

/* -------------------------------------------------------------------------- */

private void dialmsn_auth(uint16_t id, m_string *s)
{
    /* Authentification packet:
       uint32_t Header
       (string) Protocol | Login | Password | Webcam port | Client version
    */

    m_db *con = NULL;
    m_recordset *r = NULL, *blacklist = NULL;
    dialmsn_user *user = NULL;
    size_t size = 0;
    uint32_t error = DIALMSN_AUTH | DIALMSN_AUTH_SERVER_ERR;
    const char *nick = NULL, *city = NULL, *avatar = NULL, *pwd = NULL;
    uint16_t old_session = 0;

    if (! s || PARTS(s) < 5) {
        debug("DialMessenger::dialmsn_auth(): malformed request.\n");
        error |= DIALMSN_AUTH_ERR_EINVAL;
        goto _err_abort;
    }

    con = dialmsn_db_borrow();

    r = db_query(
        con,
        DIALMSN_SQL_LOGININFOS,
        TOKEN_SIZE(s, 1),
        TOKEN_DATA(s, 1)
    );

    if (! r) {
        debug("DialMessenger::dialmsn_auth(): SQL error (0).\n");
        error |= DIALMSN_AUTH_ERR_EINVAL;
        goto _err_abort;
    }

    /* check if the login was found */
    if (! r->rows) {
        debug("DialMessenger::dialmsn_auth(): login not found.\n");
        r = db_free(r); error |= DIALMSN_AUTH_ERR_ELOGIN;
        goto _err_abort;
    }

    pwd = db_field(r, "password", & size);

    /* drop connection if password do not match */
    if ( (TOKEN_SIZE(s, 2) != size) || strncmp(pwd, TOKEN_DATA(s, 2), TOKEN_SIZE(s, 2)) != 0) {
        debug("DialMessenger::dialmsn_auth(): wrong password.\n");
        r = db_free(r); error |= DIALMSN_AUTH_ERR_EPASSW;
        goto _err_abort;
    }

    /* check if the user is a subscriber */
    if (! plugin_nonsubscribers_allowed() &&
        (db_integer(r, "expiration") <= 0) &&
        (db_integer(r, "sex") != 1) ) {
        debug("DialMessenger::dialmsn_auth(): sorry, subscribers only.\n");
        r = db_free(r); error |= DIALMSN_AUTH_ERR_EINVAL;
        goto _err_abort;
    }

    /* check if the user has a previous session */
    if ( (old_session = db_integer(r, "session")) > 0) {
        /* we overwrite the previous session */
        debug("DialMessenger::dialmsn_auth(): disconnecting old session %i.\n",
              old_session);
        dialmsn_user_disconnect(old_session);
    }

    /* build a new user session */
    if (! (user = malloc(sizeof(*user))) ) {
        debug("DialMessenger::dialmsn_auth(): Out of memory (0).\n");
        r = db_free(r); error |= DIALMSN_AUTH_ERR_ENOMEM;
        goto _err_abort;
    }

    user->uid = db_integer(r, "uid");

    /* load the blacklist */
    user->busers = NULL;

    if (! (blacklist = db_query(con, DIALMSN_SQL_LOADBLOCKD, user->uid)) ) {
        debug("DialMessenger::dialmsn_auth(): SQL error (1).\n");
        free(user); r = db_free(r);
        error |= DIALMSN_AUTH_ERR_EINVAL;
        goto _err_abort;
    }

    if (blacklist->rows) {
        user->bcount = blacklist->rows;
        user->busers = malloc(blacklist->rows * sizeof(*user->busers));
        if (! user->busers) {
            debug("DialMessenger::dialmsn_auth(): Out of memory (1).\n");
            free(user); r = db_free(r);
            error |= DIALMSN_AUTH_ERR_ENOMEM;
            goto _err_abort;
        }
        size = 0;
        do user->busers[size ++] = db_integer(blacklist, "blk");
        while (db_movenext(blacklist) != -1);
    }

    blacklist = db_free(blacklist);

    nick = db_field(r, "nickname", & size);
    user->nick = strndup(nick, size + 1);
    avatar = db_field(r, "avatar", & size);
    user->avatar = strndup(avatar, size + 1);
    user->lobby = db_integer(r, "lobby");
    user->status = DIALMSN_STATUS_NRML;
    user->subscriber = (db_integer(r, "expiration") > 0) ? 1 : 0;
    user->sex = db_integer(r, "sex");
    user->dpt = db_integer(r, "zip") / 1000;
    user->age = db_integer(r, "age");
    city = db_field(r, "city", & size);
    user->city = strndup(city, size + 1);

    server_send_response(plugin_get_token(), id, 0x0,
                         "%bB4i%i%.*s%i%.*s%s%.*s%i%.*s%s%.*s%i%.*s%i%bB4i",
                         DIALMSN_AUTH | DIALMSN_AUTH_SERVER_ACK,
                         id,
                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                         user->uid,
                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                         user->nick,
                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                         user->sex,
                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                         user->avatar,
                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                         user->subscriber,
                         DIALMSN_SEPLENGTH, DIALMSN_SEPARATOR,
                         db_integer(r, "profile"), DIALMSN_TERM);

    user = dialmsn_user_add(id, user, TOKEN_DATA(s, 4));

    r = db_free(r);

    con = dialmsn_db_return(con);

    if (user) dialmsn_user_free(user);

    return;

_err_abort:
    con = dialmsn_db_return(con);

    server_send_response(plugin_get_token(), id, SERVER_MSG_END,
                         "%bB4i%bB4i", error, DIALMSN_TERM);
}

/* -------------------------------------------------------------------------- */
