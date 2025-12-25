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

#ifndef ASKL_MODULE_H

#define ASKL_MODULE_H

#ifdef _ENABLE_SERVER

#include "askl.h"
#include "m_string.h"
#include "m_file.h"
#include "askl_random.h"
#include "askl_htable.h"
#include "askl_cbtrie.h"

/** @defgroup module ASKL::module */

typedef enum {
    MODULE_EVENT_INCOMING_CONNECTION = 0x01,
    MODULE_EVENT_OUTGOING_CONNECTION = 0x02,
    MODULE_EVENT_SOCKET_DISCONNECTED = 0x04,
    MODULE_EVENT_SOCKET_RECONNECTION = 0x08,
    MODULE_EVENT_REQUEST_TRANSMITTED = 0x10,
    MODULE_EVENT_REQUEST_UNDELIVERED = 0x20,
    MODULE_EVENT_OUT_OF_BAND_MESSAGE = 0x40,
    MODULE_EVENT_SERVER_SHUTTINGDOWN = 0x80
} ASKL_ModuleEvent;

typedef struct ASKL_Module {
    int (*init)(uint32_t, int, char **);
    void (*input)(uint16_t, uint16_t, m_string *);
    void (*event)(uint16_t, uint16_t, ASKL_ModuleEvent, void *);
    void *(*suspend)(unsigned int *);
    int (*restore)(unsigned int, void *);
    void (*exit)(void);
    const char *name;
} ASKL_Module;

#define __MODULE_BUILTIN__ NULL

#define MODULE_INPUT 0x01
#define MODULE_EVENT 0x02

/* -------------------------------------------------------------------------- */

private int module_api_init(void);

/**
 * @ingroup module
 * @fn int module_api_init(void)
 * @param void
 * @return 0 on success, or -1 if an error occurs
 *
 * This function initializes the internal state required by the module API.
 * It must be called before any other module-related function is used.
 *
 * It is called once during server startup.
 */

/* -------------------------------------------------------------------------- */

private uint32_t module_open(const char *path, const char *name);

 /**
 * @ingroup module
 * @fn uint32_t module_open(const char *path, const char *name)
 *
 * @param path the path to a loadable dynamic shared object (DSO), relative to
 *             the module root path set by @ref module_setpath()
 * @param name optional logical name to associate with this module; if NULL,
 *             an empty name is stored
 *
 * @return the module id (>= 1) on success, or 0 if an error occurs
 *
 * This function attempts to load the given shared object into the server
 * process address space and register it as a module.
 *
 * Once the DSO is mapped, @ref module_open() looks up the mandatory symbols
 * that must be present in every ASKL module:
 *   - @c module_api
 *   - @c module_init
 *   - @c module_input_handler
 *   - @c module_exit
 *
 * If one of these symbols is missing, or if @c module_api() reports a newer
 * API revision than the running server supports, the function will fail.
 *
 * If the optional symbol @c module_event_handler is present, it is also
 * registered and will be used to deliver @ref ASKL_ModuleEvent notifications.
 *
 * After this function returns a valid module id, the module is considered
 * loaded but not yet initialized. The @ref module_start() function must be
 * called to run the module's @c module_init() entry point.
 * 
 * The module can be retrieved by name if one was provided.
 *
 * A module opened with this function must be unloaded with @ref module_close().
 *
 * @see module_start()
 * @see module_setpath()
 * @see module_close()
 */

/* -------------------------------------------------------------------------- */

private uint32_t module_getid(const char *name);

/**
 * @ingroup module
 * @fn uint32_t module_getid(const char *name)
 *
 * @param name a module name as passed to @ref module_open()
 * 
 * @return the module id associated with @p name, or 0 if none is found
 *
 * This function looks up a loaded module by its logical name and returns
 * its numeric identifier. The lookup is case-sensitive and only considers
 * modules that are currently registered.
 */

/* -------------------------------------------------------------------------- */

private int module_start(uint32_t id, int argc, char **argv);

/**
 * @ingroup module
 * @fn int module_start(unsigned int id, int argc, char **argv)
 *
 * @param id   a valid module id as returned by @ref module_open()
 * @param argc number of arguments passed to the module
 * @param argv argument vector passed to the module
 *
 * @return the return value of the module's @c module_init() function, or -1
 *         if the module could not be started
 *
 * This function acquires the target module and invokes its @c module_init()
 * callback once. If the module has already been initialized (i.e. its status
 * is no longer @c -1), this function is a no-op and returns the stored status.
 *
 */

/* -------------------------------------------------------------------------- */

private void module_call(const char* name, int call, ...);

/**
 * @ingroup module
 * @fn void module_call(const char *name, int call, ...)
 * @param name a module name
 * @param call call type
 * @param ... variable argument list
 * @return void
 *
 * This function allows the server or another module to call either the data
 * handler (MODULE_INPUT) or event handler (MODULE_EVENT) of a module,
 * identified by its name.
 *
 */

/* -------------------------------------------------------------------------- */

private int module_setpath(const char *path, size_t len);

/**
 * @ingroup module
 * @fn int module_setpath(const char *path, size_t len)
 *
 * @param path the new module root directory
 * @param len  the length of @p path, excluding the terminating NUL byte
 *
 * @return 0 if the path was successfully set, or -1 on error
 *
 * This function sets the base directory used to resolve a module path.
 * Subsequent calls to @ref module_open() will interpret relative @p path
 * values with respect to this root directory.
 *
 * The directory must be readable; otherwise, the function fails.
 *
 * @note If a previous root path was configured, it is freed and replaced.
 */

/* -------------------------------------------------------------------------- */

private char *module_getpath(const char *module, size_t len);

 /**
 * @ingroup module
 * @fn char *module_getpath(const char *module, size_t len)
 *
 * @param module a module-relative path
 * @param len    the length of @p module, excluding the terminating NUL byte
 *
 * @return a newly allocated NUL-terminated string containing the absolute
 *         path to the DSO, or NULL on error
 *
 * This function builds an absolute path to a module DSO by combining the
 * current module root path (set with @ref module_setpath()) with the
 * provided relative @p module path.
 *
 * If the resulting path would escape the configured module root directory,
 * the function fails and returns NULL.
 *
 * The returned string must be freed with @c free() after use.
 *
 * @see module_setpath()
 */

/* -------------------------------------------------------------------------- */

public const char *module_getopt(const char *opt, int argc, char **argv);

/**
 * @ingroup module
 * @fn const char *module_getopt(const char *opt, int argc, char **argv)
 * @param opt an option name
 * @param argc arguments count
 * @param argv arguments vector
 * @return a constant C string
 *
 * This function returns the value of an argument from the arguments vector;
 * it is assumed that the arguments vector is of the form:
 * argument0 value0 [...] argumentN valueN
 *
 * The arguments vector given to the module is formatted by @ref configure()
 * using the options from the configuration file.
 *
 * @see configure()
 *
 */

/* -------------------------------------------------------------------------- */

public const char *module_getarrayopt(
    const char *opt,
    int index,
    int argc,
    char **argv
);

/**
 * @ingroup module
 * @fn const char *module_getarrayopt(const char *opt, int index,
 *                                    int argc, char **argv)
 *
 * @param opt   an option base name
 * @param index the array index to look up (0-based)
 * @param argc  argument count
 * @param argv  argument vector, formatted as @c key/value pairs
 *
 * @return a pointer to the value associated with @p opt and @p index,
 *         or NULL if no such entry exists
 *
 * This function retrieves indexed options from the argument vector.
 * It supports both:
 *   - @c opt        (for @p index == 0)
 *   - @c opt[index] (for @p index > 0)
 *
 * For example, if the configuration contains:
 *
 *   @code
 *   listen[0] = "127.0.0.1";
 *   listen[1] = "0.0.0.0";
 *   @endcode
 *
 * then @c module_getarrayopt("listen", 0, ...) returns "127.0.0.1" and
 * @c module_getarrayopt("listen", 1, ...) returns "0.0.0.0".
 */


/* -------------------------------------------------------------------------- */

public int module_getboolopt(const char *opt, int argc, char **argv);

/**
 * @ingroup module
 * @fn int module_getopt(const char *opt, int argc, char **argv)
 * @param opt an option name
 * @param argc arguments count
 * @param argv arguments vector
 * @return 1 or 0
 *
 * This function returns the boolean value of an argument from the arguments
 * vector; it is assumed that the arguments vector is of the form:
 * argument0 value0 [...] argumentN valueN
 *
 * The arguments vector given to the module is formatted by @ref configure()
 * using the options from the configuration file.
 *
 * The strings "on", "true", "1", "enabled" from the configuration file will
 * be converted to the int value 1; "off", "false", "0", "disabled" to 0.
 *
 * @see configure()
 *
 */

/* -------------------------------------------------------------------------- */

private int module_exists(uint32_t id);

/**
 * @fn int module_exists(uint32_t id)
 * @param id a module id
 * @return 1 if the module exists, 0 otherwise
 *
 * Check if a module matching the given id is currently loaded.
 * 
 * @note this function performs a best-effort check that a module with the
 * given ID is currently registered. It does not provide any concurrency
 * guarantees: the module may be unloaded immediately after this call returns.
 *
 */

/* -------------------------------------------------------------------------- */

private ASKL_Module *module_acquire(uint32_t id);

/**
 * @ingroup module
 * @fn ASKL_Module *module_acquire(uint32_t id)
 *
 * @param id a module id
 * @return a pointer to the acquired module, or NULL if none is available
 *
 * This function acquires a module for read access. If the module is found
 * and has been successfully initialized, a read lock is taken on its
 * internal lock so that it cannot be destroyed while in use.
 *
 * The returned pointer must be released with @ref module_release() when
 * no longer needed.
 */


/* -------------------------------------------------------------------------- */

private ASKL_Module *module_release(ASKL_Module *mod);

/**
 * @ingroup module
 * @fn ASKL_Module *module_release(ASKL_Module *mod)
 *
 * @param mod an acquired module
 * @return always NULL
 *
 * This function releases a module previously acquired with
 * @ref module_acquire(), dropping the read lock held on it.
 *
 * The return value is always NULL so that callers can conveniently write:
 *
 * @code
 * mod = module_release(mod);
 * @endcode
 */

/* -------------------------------------------------------------------------- */

private void module_api_shutdown(void);

/**
 * @ingroup module
 * @fn void module_api_shutdown(void)
 * @param void
 * @return void
 *
 * This function notifies all loaded modules that the server is shutting
 * down by sending them the @ref MODULE_EVENT_SERVER_SHUTTINGDOWN event,
 * if they implement an event handler.
 *
 * It does not unload the modules; use @ref module_api_cleanup() for that.
 */

/* -------------------------------------------------------------------------- */

private void module_api_exit(void);

/**
 * @ingroup module
 * @fn void module_api_exit(void)
 * @param void
 * @return void
 *
 * This function unconditionally unloads all currently loaded modules and
 * releases any resources owned by the module API itself (including the
 * module root path and the internal locks).
 *
 * It is called once during server shutdown, after @ref module_api_shutdown().
 */

/* -------------------------------------------------------------------------- */

/* _ENABLE_SERVER */
#endif

#endif
