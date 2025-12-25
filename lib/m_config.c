/*******************************************************************************
 *  Concrete Server                                                            *
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

#include "m_config.h"

/* -------------------------------------------------------------------------- */
#if defined(_ENABLE_CONFIG) && defined(HAS_LIBXML)
/* -------------------------------------------------------------------------- */

struct _conf {
    int profile;
    int force;
    int threads;
};

struct _db_conf {
    char *host;
    char *port;
    char *user;
    char *pass;
    int con;
    int drv;
};

#ifdef _ENABLE_DB
/* database connection pools */
static ASKL_LinkedMap *db_pool = NULL;
#endif

#ifdef _ENABLE_SSL
static ASKL_LinkedMap *ssl_ctx = NULL;
#endif

/* default configuration: profile="any" threads="SERVER_CONCURRENCY" */
static struct _conf server_conf = { CONFIG_PROFILE_ANY, 0, SERVER_CONCURRENCY };

/* the default working directory */
char *working_directory = NULL;

/* -------------------------------------------------------------------------- */

#ifndef _ENABLE_PRIVILEGE_SEPARATION
static int _config_load(const char *fullpath)
{
    struct stat info;
    char *conf = NULL;
    int fd = -1, ret = 0;

    if (stat(fullpath, & info) == -1) {
        perror(ERR(_config_load, stat));
        return -1;
    }

    if ( (fd = open(fullpath, O_RDONLY)) == -1) {
        perror(ERR(_config_load, open));
        return -1;
    }

    conf = mmap(NULL, info.st_size, PROT_READ, MAP_SHARED, fd, 0);

    if (conf == MAP_FAILED) {
        perror(ERR(_config_load, mmap));
        close(fd);
        return -1;
    }

    ret = config_process(conf, info.st_size);

    munmap(conf, info.st_size); close(fd);

    return ret;
}
#endif

/* -------------------------------------------------------------------------- */

public void config_force_profile(int profile)
{
    if (server_conf.force) {
        debug("config_force_profile(): configuration profile "
              "can only be forced once.\n");
        return;
    }

    switch (profile) {
    case CONFIG_PROFILE_PRD:
    case CONFIG_PROFILE_DBG:
    case CONFIG_PROFILE_ANY:
        server_conf.profile = profile;
        server_conf.force = 1;
        break;
    default:
        debug("config_force_profile(): unknown profile.\n");
    }
}

/* -------------------------------------------------------------------------- */

private int configure(const char *path, const char *configfile)
{
    char fullpath[PATH_MAX];
    size_t pathlen = 0;

    if (! configfile) {
        debug("configure(): bad parameters.\n");
        return -1;
    }

    memset(fullpath, 0, sizeof(fullpath));

    /* check the path */
    if (path && *path) {
        if (access(path, F_OK) == -1) {
            fprintf(stderr, "configure(): wrong path to configuration file.\n");
            return -1;
        } else {
            strncpy(fullpath, path, sizeof(fullpath) - 1);
            pathlen = strlen(fullpath) + 1;
            strncat(fullpath, DIR_SEP_STR, sizeof(fullpath) - pathlen);
        }
    }

    strncat(fullpath, configfile, sizeof(fullpath) - (pathlen + 1));

    if (access(fullpath, F_OK) == -1) {
        fprintf(stderr, "configure(): configuration file %s not found.\n",
                fullpath);
        return -1;
    }

    #ifdef _ENABLE_PRIVILEGE_SEPARATION
    if (server_privileged_call(OP_CONF, fullpath, strlen(fullpath)) == -1) {
        fprintf(stderr, "configure(): cannot process configuration file.\n");
        return -1;
    }
    #else
    if (_config_load(fullpath) == -1) {
        fprintf(stderr, "configure(): cannot process configuration file.\n");
        return -1;
    }
    #endif

    return 0;
}

/* -------------------------------------------------------------------------- */

private void configure_cleanup(void)
{
    #ifdef _ENABLE_SSL
    map_free(ssl_ctx);
    #endif

    #ifdef _ENABLE_DB
    map_free(db_pool);
    #endif

    return;
}

/* -------------------------------------------------------------------------- */

public unsigned int config_get_concurrency(void)
{
    return server_conf.threads;
}

/* -------------------------------------------------------------------------- */

#ifdef _ENABLE_DB
public m_dbpool *config_get_db(const char *id)
{
    return map_get(db_pool, id, strlen(id));
}
#endif

/* -------------------------------------------------------------------------- */

#ifdef _ENABLE_SSL
private SSL_CTX *config_get_ssl(unsigned int id)
{
    return map_get(ssl_ctx, (void *) & id, sizeof(id));
}
#endif

/* -------------------------------------------------------------------------- */

#ifdef _ENABLE_SSL
static int _config_ssl_password(char *buf, int len, UNUSED int rw, void *data)
{
    int l = 0;

    if (! data || ! (l = strlen(data)) || l > len) return 0;

    memcpy(buf, data, l + 1);

    return l;
}
#endif

/* -------------------------------------------------------------------------- */

static int _config_ssl(unsigned int id, xmlNode *ssl)
{
    #ifdef _ENABLE_SSL
    xmlAttr *attr = NULL;
    char *cert = NULL, *key = NULL, *ca = NULL, *password = NULL;
    const char *attrname = NULL;
    SSL_CTX *ctx = NULL;
    const SSL_METHOD *meth = NULL;
    int ret = 0;

    int _sc = 1;

    for (attr = ssl->properties; attr; attr = attr->next) {
        attrname = (const char *) attr->name;
        if (! strcmp(attrname, "cert")) {
            cert = (char *) xmlGetProp(ssl, attr->name);
        } else if (! strcmp(attrname, "key")) {
            key = (char *) xmlGetProp(ssl, attr->name);
        } else if (! strcmp(attrname, "ca")) {
            ca = (char *) xmlGetProp(ssl, attr->name);
        } else if (! strcmp(attrname, "password")) {
            password = (char *) xmlGetProp(ssl, attr->name);
        }
    }

    /* to be consistent, we need either cert+key for server use,
       or ca for client use */
    if ( (! cert || ! key) && ! ca) {
        fprintf(stderr, "_config_ssl(): missing parameters.\n");
        ret = -1; goto _err_ctx;
    }

    /* check if there is an existing SSL context */
    if (! (ctx = map_get(ssl_ctx, (void *) & id, sizeof(id))) ) {
        /* create and initialize a new SSL context */
        if (! (meth = SSLv23_method()) ) {
            sslerror(ERR(_config_ssl, SSLv23_method));
            ret = -1; goto _err_ctx;
        }

        if (! (ctx = SSL_CTX_new(meth)) ) {
            sslerror(ERR(_config_ssl, SSL_CTX_new));
            ret = -1; goto _err_ctx;
        }

        /* load the certificate */
        if (cert && ! SSL_CTX_use_certificate_chain_file(ctx, cert)) {
            sslerror(ERR(_config_ssl, SSL_CTX_use_certificate_chain_file));
            ret = -1; goto _err_crt;
        }

        /* set the password callback */
        if (password) {
            SSL_CTX_set_default_passwd_cb(ctx, _config_ssl_password);
            SSL_CTX_set_default_passwd_cb_userdata(ctx, password);
        }

        /* load the private key */
        if (key && ! SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM)) {
            sslerror(ERR(_config_ssl, SSL_CTX_use_PrivateKey_file));
            ret = -1; goto _err_crt;
        }

        /* load the CA file */
        if (ca && ! SSL_CTX_load_verify_locations(ctx, ca, 0)) {
            sslerror(ERR(_config_ssl, SSL_CTX_load_verify_locations));
            ret = -1; goto _err_crt;
        }

        #if (OPENSSL_VERSION_NUMBER < 0x00905100L)
        SSL_CTX_set_verify_depth(ctx, 1);
        #endif

        if (cert && key) {
            /* possible server, so set a session id */
            SSL_CTX_set_session_id_context(ctx, (void *) & _sc, sizeof(_sc));
        }

        /* store the context */
        map_set(ssl_ctx, (void *) & id, sizeof(id), ctx);
        debug("_config_ssl(): successfully added SSL context.");
    }

    xmlFree(cert); xmlFree(key); xmlFree(ca); xmlFree(password);

    return ret;

_err_crt:
    SSL_CTX_free(ctx);
_err_ctx:
    xmlFree(cert); xmlFree(key); xmlFree(ca); xmlFree(password);

    return ret;

    #else

    return 0;

    #endif
}

/* -------------------------------------------------------------------------- */

static char **_config_load_plugin_opt(unsigned int id, xmlNode *opt,
                                      int *argc, char **argv)
{
    xmlNode *node = NULL;
    xmlAttr *attr = NULL;
    char *attrval = NULL, *optname = NULL, *optval = NULL;
    const char *nodename = NULL, *attrname = NULL;
    char **tmp = NULL;

    for (node = opt; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            nodename = (const char *) node->name;

            for (attr = node->properties; attr; attr = attr->next) {
                attrname = (const char *) attr->name;
                attrval = (char *) xmlGetProp(node, attr->name);

                if (! strcmp(nodename, "option")) {
                    if (! strcmp(attrname, "name")) {
                        /* XXX "reload" is not allowed
                           as an option name, in order
                           to avoid interfering with
                           the plugin reloading mechanism */
                        if (strcmp(attrval, "reload")) {
                            optname = attrval;
                            attrval = NULL;
                        }
                    } else if (! strcmp(attrname, "value")) {
                        if ( (! strcmp(attrval, "enabled")) ||
                             (! strcmp(attrval, "true")) ||
                             (! strcmp(attrval, "on")) ) {
                            optval = (char *) xmlStrndup((xmlChar *) "1", 1);
                        } else if ( (! strcmp(attrval, "disabled")) ||
                                    (! strcmp(attrval, "false")) ||
                                    (! strcmp(attrval, "off")) ) {
                            optval = (char *) xmlStrndup((xmlChar *) "0", 1);
                        } else {
                            optval = attrval; attrval = NULL;
                        }
                    }
                } else if (! strcmp(nodename, "port")) {
                    if (! strcmp(attrname, "protocol")) {
                        optname = attrval; attrval = NULL;
                    } else if (! strcmp(attrname, "number")) {
                        optval = attrval; attrval = NULL;
                    }
                } else if (! strcmp(nodename, "ssl"))
                    _config_ssl(id, node);

                xmlFree(attrval);
            }

            if ( (! strcmp(nodename, "option") && optname) ||
                 (! strcmp(nodename, "port") && optname && optval) ) {

                if (! (tmp = realloc(argv, (*argc + 2) * sizeof(*argv))) ) {
                    perror(ERR(_config_load_plugin_opt, realloc));
                    xmlFree(optval);
                    while (*argc --) xmlFree(argv[*argc]);
                    free(argv); argv = NULL; *argc = 0;
                    return NULL;
                }

                argv = tmp;

                if (! optval) optval = (char *) xmlStrndup((xmlChar *) "", 1);

                argv[(*argc) ++] = optname; argv[(*argc) ++] = optval;
            }
        }

        argv = _config_load_plugin_opt(id, node->children, argc, argv);
    }

    return argv;
}

/* -------------------------------------------------------------------------- */

static int _config_load_plugin(xmlNode *plugin)
{
    xmlAttr *attr = NULL;
    char *image = NULL, *id = NULL;
    const char *attrname = NULL;
    int argc = 0;
    char **argv = NULL;
    int plugin_id = 0;
    int ret = 0;

    for (attr = plugin->properties; attr; attr = attr->next) {
        attrname = (const char *) attr->name;
        if (! strcmp(attrname, "image")) {
            image = (char *) xmlGetProp(plugin, attr->name);
        } else if (! strcmp(attrname, "id")) {
            id = (char *) xmlGetProp(plugin, attr->name);
        }
    }

    /* check if the plugin id is properly unique */
    if ( (plugin_id = module_getid(id)) != 0) {
        fprintf(stderr, "_config_load_plugin(): "
                "there is already a loaded plugin with id %s\n", id);
        ret = -1; goto _err_load;
    }

    /* load the plugin */
    if ( (plugin_id = module_open(image, id)) == -1) {
        fprintf(stderr, "_config_load_plugin(): "
                "failed to load plugin %s (%s)\n", id, image);
        ret = -1; goto _err_load;
    }

    /* configure the plugin */
    argv = _config_load_plugin_opt(plugin_id, plugin->children, & argc, argv);

    /* initialize the plugin */
    if (module_start(plugin_id, argc, argv) == -1) {
        fprintf(stderr, "_config_load_plugin(): "
                "failed to initialize plugin %s (%s)\n", id, image);
        ret = -1;
    }

    while (argc --) xmlFree(argv[argc]); free(argv);

_err_load:
    xmlFree(image); xmlFree(id);

    return ret;
}

/* -------------------------------------------------------------------------- */

static int _config_load_db_opt(xmlNode *opt, struct _db_conf *conf)
{
    #ifdef _ENABLE_DB
    xmlNode *node = NULL;
    xmlAttr *attr = NULL;
    char *attrval = NULL;
    int intval = 0;
    const char *nodename = NULL, *attrname = NULL;

    for (node = opt; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            nodename = (const char *) node->name;

            for (attr = node->properties; attr; attr = attr->next) {
                attrname = (const char *) attr->name;
                attrval = (char *) xmlGetProp(node, attr->name);

                if (! strcmp(nodename, "location")) {
                    if (! strcmp(attrname, "host")) {
                        conf->host = attrval; attrval = NULL;
                    } else if (! strcmp(attrname, "port")) {
                        conf->port = attrval; attrval = NULL;
                    }
                } else if (! strcmp(nodename, "credentials")) {
                    if (! strcmp(attrname, "login")) {
                        conf->user = attrval; attrval = NULL;
                    } else if (! strcmp(attrname, "password")) {
                        conf->pass = attrval; attrval = NULL;
                    }
                } else if (! strcmp(nodename, "connections")) {
                    if (! strcmp(attrname, "number")) {
                        if ( (intval = atoi(attrval)) > 0 && intval < 256) {
                            conf->con = intval;
                        } else {
                            fprintf(stderr, "configure(): error: "
                                    "wrong database CONNECTIONs number "
                                    "(\"%s\") at line %i.\n"
                                    "configure(): CONNECTIONs number must be: "
                                    "(0 < CONNECTION number < 256).\n",
                                    attrval, node->line);
                            xmlFree(attrval);
                            return -1;
                        }
                    }
                }

                xmlFree(attrval);
            }
        }

        if (_config_load_db_opt(node->children, conf) == -1)
            return -1;
    }
    #endif

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _config_open_db(xmlNode *db)
{
    #ifdef _ENABLE_DB
    xmlAttr *attr = NULL;
    char *dbname = NULL, *id = NULL, *driver = NULL;
    const char *attrname = NULL;
    struct _db_conf conf = { NULL, NULL, NULL, NULL, 0, 0 };
    int ret = 0;
    m_dbpool *pool = NULL;

    for (attr = db->properties; attr; attr = attr->next) {
        attrname = (const char *) attr->name;
        if (! strcmp(attrname, "name")) {
            dbname = (char *) xmlGetProp(db, attr->name);
        } else if (! strcmp(attrname, "driver")) {
            driver = (char *) xmlGetProp(db, attr->name);
            if (! strcmp(driver, "mysql"))
                conf.drv = DB_DRIVER_MYSQL;
            else if (! strcmp(driver, "postgresql"))
                conf.drv = DB_DRIVER_PGSQL;
            else if (! strcmp(driver, "odbc"))
                conf.drv = DB_DRIVER_DODBC;
            else if (! strcmp(driver, "sqlite"))
                conf.drv = DB_DRIVER_SLITE;
        } else if (! strcmp(attrname, "id")) {
            id = (char *) xmlGetProp(db, attr->name);
        }
    }

    if ( (ret = _config_load_db_opt(db->children, & conf)) == 0) {
        /* check if the database was already open */
        if (! (pool = map_get(db_pool, id, strlen(id))) ) {
            if ( (pool = dbpool_open(conf.drv, conf.con, conf.user, conf.pass,
                                     dbname, conf.host, atoi(conf.port), 0)) ) {
                /* register the pool in the hashtable */
                map_set(db_pool, id, strlen(id), pool);
            } else {
                fprintf(stderr, "_config_open_db(): "
                        "failed to open database %s.\n", id);
                ret = -1;
            }
        }
    }

    xmlFree(conf.host); xmlFree(conf.port);
    xmlFree(conf.user); xmlFree(conf.pass);

    xmlFree(dbname); xmlFree(driver); xmlFree(id);

    return ret;

    #else

    return 0;

    #endif
}

/* -------------------------------------------------------------------------- */

static int _config_apply(xmlNode *root)
{
    xmlNode *node = NULL;
    xmlAttr *attr = NULL;
    char *value = NULL;
    int intval = 0, skip = 0;
    const char *nodename = NULL, *attrname = NULL;
    char *pluginpath = NULL;
    char path[PATH_MAX + PATH_MAX];

    for (node = root; node; node = node->next) {
        skip = 0;

        if (node->type == XML_ELEMENT_NODE) {
            nodename = (const char *) node->name;

            /* complex subnodes */
            if (! strcmp(nodename, "plugin"))
                _config_load_plugin(node);
            else if (! strcmp(nodename, "database"))
                _config_open_db(node);

            for (attr = node->properties; attr; attr = attr->next) {
                attrname = (const char *) attr->name;
                value = (char *) xmlGetProp(node, attr->name);

                /* main nodes */
                if (! strcmp(nodename, "concrete")) {
                    if (! strcmp(attrname, "configuration")) {
                        if (! server_conf.force) {
                            /* select the configuration profile unless
                               it was forcefully set with a command line
                               option */
                            if (! strcmp(value, "production"))
                                server_conf.profile = CONFIG_PROFILE_PRD;
                            else if (! strcmp(value, "debug"))
                                server_conf.profile = CONFIG_PROFILE_DBG;
                        }
                    }
                /* OPTIONS */
                } else if (! strcmp(nodename, "options")) {
                    if (! strcmp(attrname, "profile")) {
                        if (! strcmp(value, "production")) {
                            if (~server_conf.profile & CONFIG_PROFILE_PRD) {
                                xmlFree(value);
                                skip = 1; break;
                            }
                        } else if (! strcmp(value, "debug")) {
                            if (~server_conf.profile & CONFIG_PROFILE_DBG) {
                                xmlFree(value);
                                skip = 1; break;
                            }
                        }
                    }
                } else if (! strcmp(nodename, "threads")) {
                    if (! strcmp(attrname, "number")) {
                        if ( (intval = atoi(value)) > 0 && intval < 256) {
                            server_conf.threads = intval;
                        } else {
                            fprintf(stderr, "configure(): error: "
                                    "wrong THREADS number "
                                    "(\"%s\") at line %i.\n"
                                    "configure(): THREADS number must be: "
                                    "(0 < THREADS number < 256).\n",
                                    value, node->line);
                            xmlFree(value);
                            return -1;
                        }
                    }
                /*} else if (! strcmp(nodename, "instances")) {
                    if (! strcmp(attrname, "number")) {
                        if ( (intval = atoi(value)) > 0 && intval < 16) {
                            server_conf.instances = intval;
                        } else {
                            fprintf(stderr, "configure(): error: "
                                    "wrong INSTANCES number "
                                    "(\"%s\") at line %i.\n"
                                    "configure(): INSTANCES number must be: "
                                    "(0 < INSTANCES number < 16).\n",
                                    value, node->line);
                            xmlFree(value);
                            return -1;
                        }
                    }*/
                /* PLUGINS */
                } else if (! strcmp(nodename, "plugins")) {
                    if (! strcmp(attrname, "profile")) {
                        if (! strcmp(value, "production")) {
                            if (~server_conf.profile & CONFIG_PROFILE_PRD) {
                                xmlFree(value);
                                skip = 1; break;
                            }
                        } else if (! strcmp(value, "debug")) {
                            if (~server_conf.profile & CONFIG_PROFILE_DBG) {
                                xmlFree(value);
                                skip = 1; break;
                            }
                        }
                    } else if (! strcmp(attrname, "path")) {
                        pluginpath = value; value = NULL;
                    }
                /* DATABASES */
                } else if (! strcmp(nodename, "databases")) {
                    if (! strcmp(attrname, "profile")) {
                        if (! strcmp(value, "production")) {
                            if (~server_conf.profile & CONFIG_PROFILE_PRD) {
                                xmlFree(value);
                                skip = 1; break;
                            }
                        } else if (! strcmp(value, "debug")) {
                            if (~server_conf.profile & CONFIG_PROFILE_DBG) {
                                xmlFree(value);
                                skip = 1; break;
                            }
                        }
                    }
                }

                xmlFree(value);
            }

            if (skip) {
                if (pluginpath) {
                    xmlFree(pluginpath);
                    pluginpath = NULL;
                }
                continue;
            }

            if (! strcmp(nodename, "plugins") && pluginpath) {
                if (module_setpath(pluginpath, strlen(pluginpath)) == -1) {
                    fprintf(stderr, "configure(): error: "
                            "wrong PLUGIN path "
                            "(\"%s\") at line %i.\n"
                            "configure(): PLUGIN path must be: "
                            "valid path.\n",
                            pluginpath, node->line);
                    /* prepend the working directory */
                    memcpy(path, working_directory,
                           MIN(sizeof(path), strlen(working_directory) + 1));
                    memcpy(path + strlen(path), DIR_SEP_STR,
                           MIN(sizeof(path) - strlen(path),
                           strlen(DIR_SEP_STR) + 1));
                    memcpy(path + strlen(path), pluginpath,
                           MIN(sizeof(path) - strlen(path),
                               strlen(pluginpath) + 1));
                    /* try again */
                    if (module_setpath(path, strlen(path)) == -1) {
                        xmlFree(pluginpath); return -1;
                    }
                }
                xmlFree(pluginpath); pluginpath = NULL;
            }

            if (_config_apply(node->children) == -1) return -1;
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

private int config_process(const char *config, size_t len)
{
    xmlDoc *xml = NULL;
    xmlDtd *dtd = NULL;
    xmlValidCtxt *v = NULL;
    xmlChar dtd_path[PATH_MAX];

    if (! config || ! len) {
        debug("config_process(): bad parameters.\n");
        return -1;
    }

    if (! (xml = xmlParseMemory(config, len)) ) {
        debug("config_process(): cannot parse XML configuration file.\n");
        return -1;
    }

    if (! (v = xmlNewValidCtxt()) ) {
        debug("config_process(): cannot allocate validation context.\n");
        goto _err_ctx;
    }

    memcpy(dtd_path, SHAREDIR, strlen(SHAREDIR) + 1);
    if (dtd_path[strlen(SHAREDIR)] != DIR_SEP_CHR) {
        dtd_path[strlen(SHAREDIR)] = DIR_SEP_CHR;
        memcpy(dtd_path + strlen(SHAREDIR) + 1,
               "concrete.dtd", sizeof("concrete.dtd"));
    } else {
        memcpy(dtd_path + strlen(SHAREDIR),
               "concrete.dtd", sizeof("concrete.dtd"));
    }

    if (! (dtd = xmlParseDTD(NULL, dtd_path)) ) {
        debug("config_process(): cannot load Concrete Server DTD.\n");
        goto _err_dtd;
    }

    /* display the validation errors */
    v->userData = (void *) stderr;
    v->error = (xmlValidityErrorFunc) fprintf;
    v->warning = (xmlValidityWarningFunc) fprintf;

    if (xmlValidateDtd(v, xml, dtd) != 1) {
        debug("config_process(): invalid configuration.\n");
        goto _err_val;
    }

    #ifdef _ENABLE_DB
    if (! db_pool) db_pool = map_alloc((void *(*)(void *)) dbpool_close);
    #endif

    #ifdef _ENABLE_SSL
    if (! ssl_ctx) ssl_ctx = map_alloc((void *(*)(void *)) SSL_CTX_free);
    #endif

    /* the configuration file is valid, process it */
    if (_config_apply(xmlDocGetRootElement(xml)) == -1) {
        debug("config_process(): error applying the configuration.\n");
        goto _err_cnf;
    }

    xmlFreeDtd(dtd); xmlFreeValidCtxt(v); xmlFreeDoc(xml);

    return 0;

_err_cnf:
    #ifdef _ENABLE_SSL
    ssl_ctx = map_free(ssl_ctx);
    #endif
    #ifdef _ENABLE_DB
    db_pool = map_free(db_pool);
    #endif
_err_val:
    xmlFreeDtd(dtd);
_err_dtd:
    xmlFreeValidCtxt(v);
_err_ctx:
    xmlFreeDoc(xml);

    return -1;
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

/* Configuration support will not be compiled in the Concrete Library */
#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
