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

#ifndef M_PLUGIN_H

#define M_PLUGIN_H

#ifdef _ENABLE_SERVER

#include "m_core_def.h"
#include "m_string.h"
#include "m_socket.h"
#include "m_server.h"

/** @defgroup plugin core::plugin */

typedef struct m_plugin {
    /* private */
    uint32_t _id;
    handle_t _handle;
    pthread_rwlock_t *_lock;
    int _status;

    /* public */
    int (*plugin_init)(uint32_t, int, char **);
    void (*plugin_input_handler)(uint16_t, uint16_t, m_string *);
    void (*plugin_event_handler)(uint16_t, uint16_t, int, void *);
    void *(*plugin_suspend)(unsigned int *);
    int (*plugin_restore)(unsigned int, void *);
    void (*plugin_exit)(void);

    char name[];
} m_plugin;

#define PLUGIN_MAX     15

#define __PLUGIN_BUILTIN__ NULL

#define PLUGIN_INPUT 0x01
#define PLUGIN_EVENT 0x02

#define PLUGIN_EVENT_INCOMING_CONNECTION 0x01
#define PLUGIN_EVENT_OUTGOING_CONNECTION 0x02
#define PLUGIN_EVENT_SOCKET_DISCONNECTED 0x04
#define PLUGIN_EVENT_SOCKET_RECONNECTION 0x08
#define PLUGIN_EVENT_REQUEST_TRANSMITTED 0x10
#define PLUGIN_EVENT_REQUEST_UNDELIVERED 0x20
#define PLUGIN_EVENT_OUT_OF_BAND_MESSAGE 0x40
#define PLUGIN_EVENT_SERVER_SHUTTINGDOWN 0x80

/* -------------------------------------------------------------------------- */

private int plugin_api_setup(void);

/**
 * @ingroup plugin
 * @fn int plugin_api_setup(void)
 * @param void
 * @return -1 if an error occurs, 0 otherwise
 *
 * This function must be called prior using any plugin related function.
 *
 */

/* -------------------------------------------------------------------------- */

private int plugin_open(const char *path, const char *name);

/**
 * @ingroup plugin
 * @fn int plugin_open(const char *path)
 * @param path the path to a loadable dynamic shared object
 * @param name optional name for this plugin
 * @return 0 or -1 if an error occured
 *
 * This function tries to load the provided dynamic shared object in the
 * server process address space.
 *
 * The DSO path given in parameter should be relative to the plugin root
 * path, which is set using @ref plugin_setpath().
 *
 * If the DSO is successfully loaded, this function tries to find the mandatory
 * symbols which must be present in every Concrete Server plugin:
 * - plugin_init
 * - plugin_input_handler
 * - plugin_exit
 * If one of these symbols is missing, the function will fail.
 * Additionally, the plugin_event_handler symbol will be loaded if it is found.
 *
 * After this function is called, the plugin is loaded and publicly accessible
 * using the ID returned. To initialize the plugin, @ref plugin_start() must
 * be called.
 *
 * The plugin can be retrieved by name if a name was provided upon loading.
 *
 * A plugin loaded with this function must be unloaded with @ref plugin_close().
 *
 * @see plugin_start()
 * @see plugin_setpath()
 * @see plugin_close()
 *
 */

/* -------------------------------------------------------------------------- */

private int plugin_getid(const char *name);

/**
 * @ingroup plugin
 * @fn int plugin_getid(const char *name)
 * @param name a plugin name
 * @return a plugin id or -1 if none found
 *
 * This function will find the id of a loaded plugin matching the name provided.
 *
 */

/* -------------------------------------------------------------------------- */

private int plugin_start(unsigned int id, int argc, char **argv);

/**
 * @ingroup plugin
 * @fn int plugin_start(unsigned int id, int argc, char **argv)
 * @param id a plugin id
 * @param argc number of parameters
 * @param argv parameters
 * @return the return value of plugin_init()
 *
 * This function will run the plugin plugin_init() callback.
 *
 */

/* -------------------------------------------------------------------------- */

private void plugin_call(const char* name, int call, ...);

/**
 * @ingroup plugin
 * @fn int plugin_call(const char *name, int call, ...)
 * @param name a plugin name
 * @param call call type
 * @param ... variable argument list
 * @return void
 *
 * This function allows the server or another plugin to call either the data
 * handler (PLUGIN_INPUT) or event handler (PLUGIN_EVENT) of a plugin,
 * identified by its name.
 *
 */

/* -------------------------------------------------------------------------- */

private int plugin_setpath(const char *path, size_t len);

/**
 * @ingroup plugin
 * @fn int plugin_setpath(const char *path, size_t len)
 * @param path the new plugin root path
 * @param len path length
 * @return 0 if all went fine, -1 if an error occurs
 *
 * This function sets a new root path to load plugins from.
 *
 */

/* -------------------------------------------------------------------------- */

private char *plugin_getpath(const char *plugin, size_t len);

/**
 * @ingroup plugin
 * @fn int plugin_getpath(const char *plugin, size_t len)
 * @param plugin a plugin path
 * @param len path length
 * @return a pointer to a newly allocated NUL terminated string, or NULL
 *
 * This function returns the full path of a DSO from the given relative path.
 * Given path should be relative to the plugin root path, set using the
 * @ref plugin_setpath() function.
 *
 * This function ensures that the relative path is within the plugin root
 * directory; if it is outside, the function will fail and return NULL.
 *
 * The returned C string must be deleted using free() after use.
 *
 * @see plugin_setpath()
 */

/* -------------------------------------------------------------------------- */

public const char *plugin_getopt(const char *opt, int argc, char **argv);

/**
 * @ingroup plugin
 * @fn int plugin_getopt(const char *opt, int argc, char **argv)
 * @param opt an option name
 * @param argc arguments count
 * @param argv arguments vector
 * @return a constant C string
 *
 * This function returns the value of an argument from the arguments vector;
 * it is assumed that the arguments vector is of the form:
 * argument0 value0 [...] argumentN valueN
 *
 * The arguments vector given to the plugin is formatted by @ref configure()
 * using the options from the configuration file.
 *
 * @see configure()
 *
 */

/* -------------------------------------------------------------------------- */

public const char *plugin_getarrayopt(const char *opt, int index, int argc,
                                      char **argv);

/* -------------------------------------------------------------------------- */

public int plugin_getboolopt(const char *opt, int argc, char **argv);

/**
 * @ingroup plugin
 * @fn int plugin_getopt(const char *opt, int argc, char **argv)
 * @param opt an option name
 * @param argc arguments count
 * @param argv arguments vector
 * @return 1 or 0
 *
 * This function returns the boolean value of an argument from the arguments
 * vector; it is assumed that the arguments vector is of the form:
 * argument0 value0 [...] argumentN valueN
 *
 * The arguments vector given to the plugin is formatted by @ref configure()
 * using the options from the configuration file.
 *
 * The strings "on", "true", "1", "enabled" from the configuration file will
 * be converted to the int value 1; "off", "false", "0", "disabled" to 0.
 *
 * @see configure()
 *
 */

/* -------------------------------------------------------------------------- */

private int plugin_exists(unsigned int id);

/**
 * @fn int plugin_exists(unsigned int id)
 * @param id a plugin id
 * @return 1 if the plugin exists, 0 otherwise
 *
 * Check if a plugin matching the given id is loaded.
 *
 */

/* -------------------------------------------------------------------------- */

private m_plugin *plugin_acquire(unsigned int id);

/* -------------------------------------------------------------------------- */

private m_plugin *plugin_release(m_plugin *p);

/* -------------------------------------------------------------------------- */

private m_plugin *plugin_close(m_plugin *p);

/* -------------------------------------------------------------------------- */

private void plugin_api_shutdown(void);

/* -------------------------------------------------------------------------- */

private void plugin_api_cleanup(void);

/**
 * @ingroup plugin
 * @fn void plugin_api_cleanup(void)
 * @param void
 * @return void
 *
 * This function unconditionnaly closes all the loaded plugins.
 *
 */

/* -------------------------------------------------------------------------- */

/* _ENABLE_SERVER */
#endif

#endif
