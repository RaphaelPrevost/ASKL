/*******************************************************************************
 *  ASKL.                                                                      *
 *  Copyright (c) 2026 Raphael Prevost <raph@el.bzh>                           *
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

#include "askl_module.h"

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_SERVER
/* -------------------------------------------------------------------------- */

#include "arcane/module.c"

/**
 * @var _module
 *
 * This private array contains all the dynamic shared objects loaded by ASKL.
 * Many of the module related functions use it for direct access, and it must
 * NEVER be tampered with without proper locking.
 *
 * An acquired module is always readlocked, so as to prevent the destruction
 * of a module while it's in use; module destruction requires writelocking
 * the module.
 *
 */

static pthread_rwlock_t _module_lock;
static Map *_module;
static Trie *_module_name;
static Random *_random;

/** @var _module_path
 *
 * This private string tells the module api where to look for modules if
 * it is set.
 *
 */

static char *_module_path = NULL;
static size_t _module_path_len = 0;

/* -------------------------------------------------------------------------- */

static void _module_destroy(Variant v)
{
    _Module *mod = NULL;

    if (! is_pointer(v)) return;

    mod = variant_to_pointer(v);

    /* acquire the module */
    pthread_rwlock_wrlock(mod->_lock);

    /* call the destructor */
    if (mod->_status != -1) mod->interface.exit();

    /* unmap the shared object */
    dlclose(mod->_handle);

    /* finally destroy the structure */
    pthread_rwlock_unlock(mod->_lock);
    pthread_rwlock_destroy(mod->_lock);
    free(mod->_lock); free(mod);
}

/* -------------------------------------------------------------------------- */

static Variant _module_wrlock(Variant v)
{
    if (is_pointer(v)) {
        _Module *mod = variant_to_pointer(v);
        pthread_rwlock_wrlock(mod->_lock);
        return v;
    }

    return variant_null();
}

/* -------------------------------------------------------------------------- */

static Variant _module_rdlock(Variant v)
{
    if (is_pointer(v)) {
        _Module *mod = variant_to_pointer(v);
        /* XXX modules cannot be acquired before or during initialization */
        if (mod->_status != -1) {
            pthread_rwlock_rdlock(mod->_lock);
            return v;
        }
    }

    return variant_null();
}

/* -------------------------------------------------------------------------- */

INTERNAL int module_api_init(void)
{
    uint32_t seed[4];

    if (random_seed(seed, 4) == -1) return -1;

    if (! (_random = random_arrayinit(seed, 4)) ) return -1;

    if (pthread_rwlock_init(& _module_lock, NULL) == -1) {
        perror(ERR(module_api_init, pthread_rwlock_init));
        goto _err_lock;
    }

    if (! (_module = map_alloc(_module_destroy)) ) goto _err_hash;

    if (! (_module_name = trie_alloc(NULL)) ) goto _err_name;

    return 0;

_err_name:
    map_free(_module);
_err_hash:
    pthread_rwlock_destroy(& _module_lock);
_err_lock:
    random_free(_random);

    return -1;
}

/* -------------------------------------------------------------------------- */

INTERNAL uint32_t module_open(const char *path, const char *name)
{
    _Module *mod = NULL;
    unsigned int (*module_api)(void) = NULL;
    char *fullpath = NULL;
    uint32_t module_id = 0;
    size_t namelen = 0;
    Variant v;

    /* check if a name was provided */
    namelen = (name) ? strlen(name) : 1;

    if (! (mod = malloc(sizeof(*mod) + namelen + 1)) ) {
        perror(ERR(module_open, malloc));
        return -1;
    }

    mod->_handle = NULL;
    mod->_status = -1;

    /* set the module name (NUL string if none provided) */
    if (name) memcpy(mod->_name, name, namelen);
    mod->_name[namelen] = 0;
    mod->interface.name = mod->_name;

    if (! (mod->_lock = malloc(sizeof(*mod->_lock))) ) {
        perror(ERR(module_open, malloc));
        goto _err_malloc;
    }

    if (pthread_rwlock_init(mod->_lock, NULL) == -1) {
        perror(ERR(module_open, pthread_rwlock_init));
        goto _err_lock;
    }

    /* map the shared library in the process address space */
    if (path != __MODULE_BUILTIN__) {
        if (path && (fullpath = module_getpath(path, strlen(path))) ) {
            mod->_handle = dlopen(fullpath, RTLD_LAZY);
            free(fullpath);
            if (! mod->_handle) {
                fprintf(stderr, ERR(module_open, dlopen)": %s\n", dlerror());
                goto _err_dlopen;
            }
        } else goto _err_dlopen;
    } else {
        #ifndef WIN32
        if (! (mod->_handle = dlopen(NULL, RTLD_LAZY)) ) {
        #else
        if (! (mod->_handle = GetModuleHandle(NULL)) ) {
        #endif
            fprintf(
                stderr,
                ERR(module_open, GetModuleHandle)": %s\n",
                dlerror()
            );
            goto _err_dlopen;
        }
    }

    /* load mandatory symbols */
    module_api = (unsigned int (*)(void)) dlsym(
        mod->_handle,
        "module_api"
    );
    if (! module_api) goto _err_dlget;

    /* check the required API revision */
    if (module_api() > __ASKL__) {
        fprintf(
            stderr,
            "module_open(): %s requires a newer API revision.\n",
            path
        );
        goto _err_dlsym;
    }

    mod->interface.init = (int (*)(uint32_t, int, char **)) dlsym(
        mod->_handle,
        "module_init"
    );
    if (! mod->interface.init) goto _err_dlget;

    mod->interface.exit = (void (*)(void)) dlsym(mod->_handle, "module_exit");
    if (! mod->interface.exit) goto _err_dlget;

    mod->interface.input = (void (*)(uint16_t, uint16_t, String *)) dlsym(
        mod->_handle,
        "module_input_handler"
    );
    if (! mod->interface.input) goto _err_dlget;

    /* optional symbol, module interrupt handler */
    mod->interface.event = (void (*)(uint16_t, uint16_t, Module_Event, void *)) dlsym(
        mod->_handle,
        "module_event_handler"
    );

    #ifdef WIN32
    /* if we opened the calling process, we need to invalidate the handle
       to avoid a disaster */
    if (path == __MODULE_BUILTIN__) mod->_handle = NULL;
    #endif

    /* register the module */
    pthread_rwlock_wrlock(mod->_lock);
        do {
            pthread_rwlock_wrlock(& _module_lock);
                /* generate the 32 bits random id */
                module_id = random_uint32(_random);
            pthread_rwlock_unlock(& _module_lock);

            v = map_insert(
                _module,
                (char *) & module_id,
                sizeof(module_id),
                variant_from_pointer(mod)
            );
        } while (! is_null(v));

        /* allow id lookup from name */
        if (name) {
            trie_insert(
                _module_name,
                name,
                namelen,
                variant_from_integer(module_id)
            );
        }
    pthread_rwlock_unlock(mod->_lock);

    return module_id;

_err_dlget:
    fprintf(stderr, ERR(module_open, dlsym)": %s\n", dlerror());
_err_dlsym:
    dlclose(mod->_handle);
_err_dlopen:
    pthread_rwlock_destroy(mod->_lock);
_err_lock:
    free(mod->_lock);
_err_malloc:
    free(mod);
    return 0;
}

/* -------------------------------------------------------------------------- */

INTERNAL uint32_t module_getid(const char *name)
{
    Variant v;

    if (! name) {
        debug("module_getid(): bad parameters.\n");
        return 0;
    }

    v = trie_lookup(_module_name, name, strlen(name), NULL);

    return (is_null(v)) ? 0 : variant_to_integer(v);
}

/* -------------------------------------------------------------------------- */

INTERNAL int module_start(uint32_t id, int argc, char **argv)
{
    Variant v;
    int ret = -1;

    if (! id) {
        debug("module_start(): bad parameters.\n");
        return -1;
    }

    v = map_get_with(_module, (char *) & id, sizeof(id), _module_wrlock);

    if (is_pointer(v)) {
        _Module *mod = variant_to_pointer(v);
        if (mod->_status == -1) {
            mod->_status = mod->interface.init(id, argc, argv);
            ret = mod->_status;
        }
        pthread_rwlock_unlock(mod->_lock);
    }

    return ret;
}

/* -------------------------------------------------------------------------- */

INTERNAL void module_call(const char* name, int call, ...)
{
    uint32_t module_id = 0;
    Module *mod = NULL;
    uint16_t socket_id = 0;
    uint16_t ingress_id = 0;
    String *buffer = NULL;
    unsigned int event = 0;
    void *event_data = NULL;
    va_list ap;

    /* check if the name matches an existing module */
    if ( (module_id = module_getid(name)) == 0) {
        debug("module_call(): module not found.\n");
        return;
    }

    if (! (mod = module_acquire(module_id)) ) {
        debug("module_call(): cannot acquire the module.\n");
        return;
    }

    va_start(ap, call);

    switch (call) {

    case MODULE_INPUT: {
        socket_id = va_arg(ap, int);
        ingress_id = va_arg(ap, int);
        buffer = va_arg(ap, String *);
        mod->input(socket_id, ingress_id, buffer);
    } break;

    case MODULE_EVENT: {
        socket_id = va_arg(ap, int);
        ingress_id = va_arg(ap, int);
        event = va_arg(ap, int);
        event_data = va_arg(ap, void *);
        /* module_event_handler is optional */
        if (mod->event)
            mod->event(socket_id, ingress_id, event, event_data);
    } break;

    }

    va_end(ap);

    mod = module_release(mod);

    return;
}

/* -------------------------------------------------------------------------- */

INTERNAL int module_setpath(const char *path, size_t len)
{
    char *p = NULL;

    if (! path || ! len) {
        debug("module_setpath(): bad parameters.\n");
        return -1;
    }

    if (access(path, R_OK) == -1) {
        perror(ERR(module_setpath, access));
        return -1;
    }

    if (! (p = malloc( (len + 1) * sizeof(*p))) ) {
        perror(ERR(module_setpath, malloc));
        return -1;
    }

    memcpy(p, path, len + 1);

    pthread_rwlock_wrlock(& _module_lock);

    if (_module_path) free(_module_path);
    _module_path = p; _module_path_len = len;

    pthread_rwlock_unlock(& _module_lock);

    return 0;
}

/* -------------------------------------------------------------------------- */

INTERNAL char *module_getpath(const char *dso, size_t len)
{
    char *ret = NULL;
    size_t off = 0;

    if (! dso || ! len) {
        debug("module_getpath(): bad parameters.\n");
        return NULL;
    }

    #ifdef _ENABLE_FILE
    if (! fs_isrelativepath(dso, len)) {
        debug("module_getpath(): %s is outside the modules directory.\n", dso);
        return NULL;
    }
    #endif

    pthread_rwlock_rdlock(& _module_lock);

    if (! _module_path) {
        pthread_rwlock_unlock(& _module_lock);
        return NULL;
    }

    if (! (ret = malloc( (_module_path_len + len + 2) * sizeof(*ret))) ) {
        perror(ERR(module_getpath, malloc));
        pthread_rwlock_unlock(& _module_lock);
        return NULL;
    }

    memcpy(ret, _module_path, _module_path_len + 1);
    off = _module_path_len;

    pthread_rwlock_unlock(& _module_lock);

    if (ret[off] != DIR_SEP_CHR) ret[off ++] = DIR_SEP_CHR;
    memcpy(ret + off, dso, len + 1);

    return ret;
}

/* -------------------------------------------------------------------------- */

ASKL_API const char *module_getopt(const char *opt, int argc, char **argv)
{
    int i = 0;

    if (! opt || ! argv || argc <= 0) return NULL;

    for (i = 0; i < argc; i ++) {
        if (argv[i] && i + 1 < argc && ! strcmp(argv[i], opt))
            return argv[i + 1];
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API const char *module_getarrayopt(
    const char *opt,
    int index,
    int argc,
    char **argv
)
{
    int i = 0;
    size_t optlen = 0, curlen = 0;
    char buffer[BUFSIZ];

    if (! opt || ! argv || argc <= 0) return NULL;

    optlen = strlen(opt);

    for (i = 0; i < argc; i ++) {
        /* check if there is an option */
        if (! argv[i] || i + 1 == argc) continue;

        /* check if the length can match */
        if ( (curlen = strlen(argv[i])) < optlen) continue;

        /* allow syntax without index for [0] */
        if (index == 0 && curlen == optlen) {
            if (! memcmp(argv[i], opt, optlen))
                return argv[i + 1];
        } else {
            /* index is mandatory */
            snprintf(buffer, sizeof(buffer), "%s[%i]", opt, index);
            if (! strcmp(argv[i], buffer))
                return argv[i + 1];
        }
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

ASKL_API int module_getboolopt(const char *opt, int argc, char **argv)
{
    const char *optval = module_getopt(opt, argc, argv);
    return (optval && atoi(optval) == 1) ? 1 : 0;
}

/* -------------------------------------------------------------------------- */

INTERNAL int module_exists(uint32_t id)
{
    if (! id) {
        debug("module_exists(): bad parameters.\n");
        return 0;
    }

    /* simple check, no need to lock */
    return is_pointer(map_get(_module, (char *) & id, sizeof(id)));
}

/* -------------------------------------------------------------------------- */

INTERNAL Module *module_acquire(uint32_t id)
{
    Variant v;

    if (! id) {
        debug("module_acquire(): bad parameters.\n");
        return NULL;
    }

    v = map_get_with(_module, (char *) & id, sizeof(id), _module_rdlock);

    if (is_pointer(v)) {
        _Module *mod = variant_to_pointer(v);
        return module_public_interface(mod);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

INTERNAL Module *module_release(Module *module)
{
    _Module *mod = module_private_interface(module);

    if (mod) pthread_rwlock_unlock(mod->_lock);

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int _module_shutdown(
    UNUSED const char *k,
    UNUSED size_t l,
    Variant v,
    UNUSED void *context
)
{
    Module *module = NULL;

    if (! is_pointer(v)) return 0;

    module = variant_to_pointer(v);

    if (module->event)
        module->event(0, 0, MODULE_EVENT_SERVER_SHUTTINGDOWN, NULL);
    
    return 0;
}

/* -------------------------------------------------------------------------- */

INTERNAL void module_api_shutdown(void)
{
    map_foreach(_module, _module_shutdown, NULL);
}

/* -------------------------------------------------------------------------- */

INTERNAL void module_api_exit(void)
{
    trie_free(_module_name);
    map_free(_module);

    free(_module_path);

    pthread_rwlock_destroy(& _module_lock);
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
