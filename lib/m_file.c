/*******************************************************************************
 *  Concrete Server                                                            *
 *  Copyright (c) 2005-2023 Raphael Prevost <raph@el.bzh>                      *
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

#include "m_file.h"

/* -------------------------------------------------------------------------- */
#if defined(_ENABLE_FILE) && defined(_ENABLE_TRIE) && defined(HAS_LIBXML)
/* -------------------------------------------------------------------------- */

static m_view *default_view = NULL;

static int _fs_refcount_lock(m_file *ref)
{
    int ret = 0;

    if (! ref || ! ref->_lock) return -1;

    pthread_mutex_lock(ref->_lock);

        /* sleep until someone release the lock */
        while (ref->_lockstate == 0) pthread_cond_wait(ref->_cond, ref->_lock);

        /* we got woken up, check if the lockstate is valid */
        (ref->_lockstate == -1) ? ret = -1 : ref->_lockstate --;

    pthread_mutex_unlock(ref->_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */

static void _fs_refcount_unlock(m_file *ref)
{
    if (! ref) return;

    pthread_mutex_lock(ref->_lock);

        if (ref->_lockstate >= 0) ref->_lockstate ++;

    pthread_mutex_unlock(ref->_lock);

    pthread_cond_signal(ref->_cond);
}

/* -------------------------------------------------------------------------- */

static void _fs_refcount_breaklock(m_file *ref)
{
    if (! ref) return;

    pthread_mutex_lock(ref->_lock);

        ref->_lockstate = -1;

    pthread_mutex_unlock(ref->_lock);

    pthread_cond_broadcast(ref->_cond);
}

/* -------------------------------------------------------------------------- */

static variant CALLBACK _fs_refcount_acquire(variant v)
{
    m_file *ref = variant_to_pointer(v);

    if (! ref || _fs_refcount_lock(ref) == -1) return variant_null();

    return v;
}

/* -------------------------------------------------------------------------- */

static void _fs_orphan(variant v)
{
    m_file *file = variant_to_pointer(v);

    if ((int) file->_refcount >= 0) {
        file->_view = NULL;
        /* if the code is correct, there should not be any file with
           a refcount set to 0, so let's signal any leak */
        if (! file->_refcount) {
            debug("_fs_orphan(): found a leaked cache entry: %.*s.\n",
                  (int) file->pathlen, file->path);
            fs_closefile(file);
        }
    }
}

/* -------------------------------------------------------------------------- */
#if 0
static void *_fs_io_worker(UNUSED void *dummy)
{
    while (ok) {

        monitor = pop(monitor_queue);
        if (monitor) {
            /* check if there is events */

            /* error: destroy the monitor */

            /* process events */

            /* push back the monitor */
        }

        io_op = pop(io_queue);

        if (io_op) {
            /* do the io */

            /* destroy the io_op */
        }
    }
}
#endif

/* -------------------------------------------------------------------------- */

public int fs_api_setup(void)
{
    /* add a default physical view */
    if (! (default_view = fs_openview(NULL, 0)) ) {
        debug("fs_api_setup(): cannot open default view.\n");
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

public m_view *fs_openview(const char *root, size_t rootlen)
{
    m_view *new = NULL;

    if (! fs_isrelativepath(root, rootlen)) {
        debug("fs_openview(): %.*s is not a relative path.\n",
              (int) rootlen, root);
        return NULL;
    }

    if (! (new = malloc(sizeof(*new))) ) {
        perror(ERR(fs_openview, malloc));
        return NULL;
    }

    if (! (new->_cache = trie_alloc(_fs_orphan)) )
        goto _err_cache;
    if (! (new->_handlers = trie_alloc(NULL)) )
        goto _err_hndlr;

    new->flags = 0;
    new->root = NULL;
    new->rootlen = 0;

    if (root && rootlen) {
        if (rootlen > 1) {
            /* check that there is no trailing separator */
            if (isdirsepchr(root[rootlen - 1])) rootlen --;
        }

        if (! (new->root = malloc( (rootlen + 1) * sizeof(*new->root))) )
            goto _err_alloc;
        new->rootlen = rootlen;
        memcpy(new->root, root, rootlen + 1);

        /* check if the root is a physical path or a virtual one */
        if (access(root, F_OK) == -1) new->flags = VIEW_VIRTUAL;
    }

    return new;

_err_alloc:
    trie_free(new->_handlers);
_err_hndlr:
    trie_free(new->_cache);
_err_cache:
    free(new);
    return NULL;
}

/* -------------------------------------------------------------------------- */

public int fs_mkpath(m_view **v, const char *p, size_t l, char *out, size_t len)
{
    size_t pathlen = 0;
    m_view *view = NULL;

    if (! v || ! p || ! l || ! out || ! len) {
        debug("fs_mkpath(): bad parameters.\n");
        return -1;
    }

    /* allow the use of absolute path names (within the view root) */
    if ( (view = *v) ) {
        if (l > view->rootlen && ! memcmp(view->root, p, view->rootlen)) {
            p += view->rootlen; l -= view->rootlen;
        }

        if (view->root) {
            pathlen = view->rootlen + 1;
            /* check that there is no separator at the beginning of the path */
            if (l && isdirsepchr(*p)) { p ++; l --; }
        }
    } else *v = view = default_view;

    pathlen += l;

    if (! fs_isrelativepath(p, l)) {
        debug("fs_mkpath(): %.*s is out of root directory.\n", (int) l, p);
        return -1;
    }

    if (pathlen > len) {
        debug("fs_mkpath(): output buffer is too small.\n");
        return -1;
    }

    /* get the full path */
    if (view->root) {
        memcpy(out, view->root, view->rootlen);
        memcpy(out + view->rootlen, DIR_SEP_STR, 1);
        memcpy(out + view->rootlen + 1, p, l);
    } else memcpy(out, p, l);

    out[pathlen] = 0;

    return pathlen;
}

/* -------------------------------------------------------------------------- */

static m_file *_fs_openfile(m_view *v, unsigned int flags, const char *p,
                            size_t l, m_auth *a)
{
    m_file *new = NULL;
    char fullpath[PATH_MAX];
    int len = 0;
    int open_flags = 0x0;
    int open_mode = 0x0;
    struct stat info;
    int fd = -1;
    int retry = 0;
    m_string *data = NULL;

    if (! p || ! l) {
        debug("_fs_openfile(): bad parameters.\n");
        return NULL;
    }

    memset(& info, 0, sizeof(info));

    /* get the full path */
    if ( (len = fs_mkpath(& v, p, l, fullpath, sizeof(fullpath))) == -1)
        return NULL;

    /* TODO check if access to this file is allowed */

_try_again:
    retry = 0;

    /* check if the file exists in the cache */
    new = variant_to_pointer(
        trie_lookup(v->_cache, fullpath, len, _fs_refcount_acquire)
    );

    if (new) {
        if (~flags & FILE_OPEN_SHARED ||
            new->flags & FILE_PRIVATE ||
            new->flags & FILE_DELETED ||
            flags & FILE_OPEN_CREATE) {
            /* if the initial caller didn't allow the use of a shared copy,
               or a private copy is already in use, or the mapping has
               been marked as deleted, we cannot open the file;
               if the file exists in the cache, we cannot create it either */
            debug("_fs_openfile(): a private copy is already in use.\n");
            _fs_refcount_unlock(new);
            return NULL;
        }
        new->_refcount ++;
        _fs_refcount_unlock(new);
        return new;
    }

    /* if this is a virtual view, don't allow physical access */
    if (v->flags & VIEW_VIRTUAL) {
        debug("_fs_openfile(): access to physical files is not "
              "allowed within a virtual view.\n");
        return NULL;
    }

    /* check if it can be loaded from the filesystem */
    if (access(fullpath, F_OK) == -1) {
        if (~flags & FILE_OPEN_CREATE) {
            perror(ERR(_fs_openfile, access));
            return NULL;
        }
    } else if (flags & FILE_OPEN_CREATE) {
        debug("_fs_openfile(): this file already exists.\n");
        return NULL;
    }

    if (~flags & FILE_OPEN_CREATE) {
        /* gather informations about the file */
        if (stat(fullpath, & info) == -1) {
            perror(ERR(_fs_openfile, stat));
            return NULL;
        }

        open_flags = O_RDONLY;

        /* use requested access rights if available */
        if (a) {
            if (a->access & AUTH_ACCESS_R)
                open_flags = (a->access & AUTH_ACCESS_W) ? O_RDWR : O_RDONLY;
            else if (a->access & AUTH_ACCESS_W)
                open_flags = O_WRONLY;
        }
    } else {
        /* TODO use the access rules applied to the view to set the
           default file permissions */
        open_mode = (S_IRWXU | S_IRGRP);

        /* open with read/write privileges */
        open_flags = O_CREAT | O_EXCL | O_RDWR;
    }

    /* open the file */
    if ( (fd = open(fullpath, open_flags, open_mode)) == -1) {
        perror(ERR(_fs_openfile, open));
        return NULL;
    }

    if (! (new = malloc(sizeof(*new))) ) {
        perror(ERR(_fs_openfile, malloc));
        goto _err_file;
    }

    if (! (new->_lock = malloc(sizeof(*new->_lock) + sizeof(*new->_cond)) ) ) {
        perror(ERR(_fs_openfile, malloc));
        goto _err_lock;
    }

    if (pthread_mutex_init(new->_lock, NULL) != 0) {
        perror(ERR(_fs_openfile, pthread_mutex_init));
        goto _err_lck2;
    }

    new->_cond = (void *) (((char *) new->_lock) + sizeof(*new->_lock));

    if (pthread_cond_init(new->_cond, NULL) != 0) {
        perror(ERR(_fs_openfile, pthread_cond_init));
        goto _err_cond;
    }

    if (! (new->_rwlock = malloc(sizeof(*new->_rwlock))) ) {
        perror(ERR(_fs_openfile, malloc));
        goto _err_rwlk;
    }

    if (pthread_rwlock_init(new->_rwlock, NULL) == -1) {
        perror(ERR(_fs_openfile, pthread_rwlock_init));
        goto _err_rwl2;
    }

    if (! (new->path = malloc( (len + 1) * sizeof(*new->path))) ) {
        perror(ERR(_fs_openfile, malloc));
        goto _err_path;
    }

    new->_view = v;

    /* initialize the file structure */
    memcpy(new->path, fullpath, len + 1);
    new->pathlen = len;
    new->fd = fd;
    new->len = info.st_size;
    new->_last_modified = 0;
    new->_last_accessed = 0;
    new->flags = (~flags & FILE_OPEN_SHARED) ? FILE_PRIVATE : 0;
    new->data = data;
    new->_lockstate = 1;
    new->_refcount = 1;

    /* XXX should use different records depending on access rights */

    /* try to insert the new record in the cache */
    if (trie_insert(v->_cache, new->path, len, variant_from_pointer(new))) {
        debug("_fs_openfile(): cannot insert the new cache record.\n");
        goto _err_hash;
    }

    return new;

_err_hash:
    /* this record already exists in the cache, avoid duplication */
    free(new->path);

    if (~flags & FILE_OPEN_SHARED || flags & FILE_OPEN_CREATE) {
        debug("_fs_fileopen(): cannot open the file with exclusive access.\n");
        retry = 0;
    } else {
        /* if the user did not ask for exclusive access, try again */
        retry = 1;
    }
_err_path:
    pthread_rwlock_destroy(new->_rwlock);
_err_rwl2:
    free(new->_rwlock);
_err_rwlk:
    pthread_cond_destroy(new->_cond);
_err_cond:
    pthread_mutex_destroy(new->_lock);
_err_lck2:
    free(new->_lock);
_err_lock:
    free(new);
_err_file:
    if (fd != -1) close(fd);

    if (retry) goto _try_again;

    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_file *fs_openfile(m_view *v, const char *p, size_t l, m_auth *a)
{
    /** @brief this function opens a file within a view, if it already exists */

    return _fs_openfile(v, FILE_OPEN_SHARED, p, l, a);
}

/* -------------------------------------------------------------------------- */

public m_file *fs_reopenfile(m_file *f)
{
    variant var = { 0 };

    if (! f) {
        debug("fs_reopenfile(): bad parameters.\n");
        return NULL;
    }

    var = _fs_refcount_acquire(variant_from_pointer(f));

    if ( (f = variant_to_pointer(var)) ) {
        if (f->flags & FILE_PRIVATE || f->flags & FILE_DELETED) {
            /* if the initial opener didn't allow the use of a shared copy,
               or a private copy is already in use, or the mapping is
               marked as deleted, we cannot open the file */
            debug("fs_reopenfile(): this file cannot be reopened.\n");
            _fs_refcount_unlock(f);
            return NULL;
        }
        f->_refcount ++;
        _fs_refcount_unlock(f);
    }

    return f;
}

/* -------------------------------------------------------------------------- */

public m_file *fs_createfile(m_view *v, const char *p, size_t l)
{
    /** @brief this function creates a file within a view if it doesn't exist */

    return _fs_openfile(v, FILE_OPEN_CREATE | FILE_OPEN_SHARED, p, l, NULL);
}

/* -------------------------------------------------------------------------- */

public int fs_mkdir(m_view *v, const char *p, size_t l)
{
    char fullpath[PATH_MAX];
    int len = 0;
    m_string *path = NULL, *folder = NULL;
    unsigned int i = 0;
    char *end = NULL;

    /* get the full path */
    if ( (len = fs_mkpath(& v, p, l, fullpath, sizeof(fullpath))) == -1)
        return -1;

    if (v->flags & VIEW_VIRTUAL) {
        debug("fs_mkdir(): directories cannot be created in virtual views.\n");
        return -1;
    }

    if (! (path = string_encaps(fullpath, len)) ) return -1;

    /* skip the root */
    if (! (folder = string_select(path, v->rootlen, len - v->rootlen)) )
        goto _err_mkdir;

    /* split it using the directory separator */
    if (string_splits(folder, DIR_SEP_STR, 1) == -1) goto _err_mkdir;

    for (i = 0; i < PARTS(folder); i ++) {

        if (! TOKEN_SIZE(folder, i)) continue;

        *(end = (char *) TOKEN_END(folder, i)) = 0;

        /* create the directory if it does not already exist */
        if (access(DATA(path), F_OK) == -1) {
            /* use default permissions */
            if (mkdir(DATA(path), 0777) == -1) {
                perror(ERR(fs_mkdir, mkdir));
                goto _err_mkdir;
            }
        }

        *end = DIR_SEP_CHR;
    }

    return 0;

_err_mkdir:
    folder = string_free(folder);
    return -1;
}

/* -------------------------------------------------------------------------- */

public int fs_isopened(m_view *v, const char *p, size_t l)
{
    /** @brief this function checks if a file is already openened */

    char fullpath[PATH_MAX];
    int len = 0;

    if (! p || ! l) {
        debug("fs_isopenfile(): bad parameters.\n");
        return 0;
    }

    if ( (len = fs_mkpath(& v, p, l, fullpath, sizeof(fullpath))) == -1)
        return 0;

    /* check if the file exists in the cache */
    return (
        variant_to_pointer(trie_lookup(v->_cache, fullpath, len, NULL)) != NULL
    );
}

/* -------------------------------------------------------------------------- */

public void fs_settime(m_file *f, int which, time_t timestamp)
{
    if (! f) return;

    switch (which) {
    case FILE_LAST_MODIFIED: f->_last_modified = timestamp; break;
    case FILE_LAST_ACCESSED: f->_last_accessed = timestamp; break;
    default: return;
    }
}

/* -------------------------------------------------------------------------- */

public void fs_getpath(const char *path, size_t len, char *out, size_t outlen)
{
    if (! path || ! len || ! out || ! outlen) {
        debug("fs_getpath(): bad parameters.\n");
        return;
    }

    while (len --) {
        if (isdirsepchr(path[len])) {
            if (len + 1 < outlen) {
                memmove(out, path, len);
                out[len] = '\0';
            }
            break;
        }
    }
}

/* -------------------------------------------------------------------------- */

public void fs_getfilename(const char *path, size_t l, char *out, size_t outlen)
{
    size_t len = l;

    if (! path || ! l || ! out || ! outlen) {
        debug("fs_getfilename(): bad parameters.\n");
        return;
    }

    while (l --) {
        if (isdirsepchr(path[l]) || ! l) {
            if ( (len -= l) > 0) {
                if (len + 1 < outlen) {
                    memmove(out, path + l + !! l, len);
                    out[len] = '\0';
                }
            }
            break;
        }
    }
}

/* -------------------------------------------------------------------------- */

public int fs_isrelativepath(const char *path, size_t len)
{
    /** @brief simple routine to check if a path is a proper relative path */

    size_t off = 0;
    unsigned int i = 0;
    int indir = 0;

    if (! path || ! len) return 1;

    for (i = 0; i < len; i ++) {
        if (isdirsepchr(path[i])) {
            switch (i - off) {
            case 3: /* fallthrough */
            case 2: if (! memcmp(path + i - 2, "..", 2)) indir --; break;
            case 1: if (path[i - 1] == '.' || isdirsepchr(path[i - 1]))
            case 0: break;
            default: indir ++;
            }
            off = i;
        }
    }

    return (indir >= 0);
}

/* -------------------------------------------------------------------------- */

static int _fs_map(m_view *v, const char *p, size_t l, m_string *data, int r)
{
    char fullpath[PATH_MAX];
    int len = 0, flags = 0;
    variant val = { 0 };
    m_file *new = NULL, *prev = NULL;

    if (! p || ! l || ! data) {
        debug("_fs_map(): bad parameters.\n");
        return -1;
    }

    /* get the full path */
    if ( (len = fs_mkpath(& v, p, l, fullpath, sizeof(fullpath))) == -1)
        return -1;

    /* mapping over physical files is not allowed */
    if (access(fullpath, F_OK) != -1) {
        debug("_fs_map(): %.*s already exists.\n", len, fullpath);
        return -1;
    }

    /* check if there is a previous mapping */
    prev = variant_to_pointer(
        trie_lookup(v->_cache, fullpath, len, _fs_refcount_acquire)
    );

    if (prev) {
        /* check if the previous mapping is physical */
        flags = prev->flags; _fs_refcount_unlock(prev);

        /* if the mapping is deleted, it can be replaced */
        if (flags & FILE_DELETED) r = 1;

        /* only go further if we can replace the mapping */
        if (! r) {
            debug("_fs_map(): mapping already exists.\n");
            return -1;
        }

        /* XXX don't replace physical files by virtual ones */
        if (~flags & FILE_VIRTUAL) {
            debug("_fs_map(): cannot map over an existing, "
                  "non-virtual mapping.\n");
            goto _err_hash;
        }
    }

    /* TODO check if mapping is allowed */

    if (! (new = malloc(sizeof(*new))) ) {
        perror(ERR(_fs_map, malloc));
        return -1;
    }

    if (! (new->_lock = malloc(sizeof(*new->_lock) + sizeof(*new->_cond)) ) ) {
        perror(ERR(_fs_map, malloc));
        goto _err_lock;
    }

    if (pthread_mutex_init(new->_lock, NULL) != 0) {
        perror(ERR(_fs_map, pthread_mutex_init));
        goto _err_lck2;
    }

    new->_cond = (void *) (((char *) new->_lock) + sizeof(*new->_lock));

    if (pthread_cond_init(new->_cond, NULL) != 0) {
        perror(ERR(_fs_map, pthread_cond_init));
        goto _err_cond;
    }

    if (! (new->_rwlock = malloc(sizeof(*new->_rwlock))) ) {
        perror(ERR(_fs_map, malloc));
        goto _err_rwlk;
    }

    if (pthread_rwlock_init(new->_rwlock, NULL) == -1) {
        perror(ERR(_fs_map, pthread_rwlock_init));
        goto _err_rwl2;
    }

    if (! (new->path = malloc( (len + 2) * sizeof(*new->path))) ) {
        perror(ERR(_fs_map, malloc));
        goto _err_path;
    }

    new->_view = v;

    /* initialize the file structure */
    memcpy(new->path, fullpath, len + 1);
    new->pathlen = len;
    new->fd = -1;
    new->data = data;
    new->len = SIZE(data);
    new->_last_modified = 0;
    new->_last_accessed = 0;
    new->flags = FILE_VIRTUAL;

    new->_lockstate = 1;
    new->_refcount = 0;

    val = variant_from_pointer(new);

    if (prev) {
        /* replace the old virtual mapping by the new one */
        prev = variant_to_pointer(
            trie_update(v->_cache, new->path, new->pathlen, val)
        );

        if (prev == new) {
            debug("_fs_map(): cannot replace the existing mapping.\n");
            goto _err_hash;
        } else if (prev) {
            /* this file cannot be accessed anymore since it was just
               removed from the view, so check if it still has some users */
            prev->_view = NULL;
            if (! prev->_refcount) fs_closefile(prev);
        }
    } else {
        /* we only create the mapping if it does not already exist */
        if (trie_insert(v->_cache, new->path, new->pathlen, val)) {
            debug("_fs_map(): cannot insert the new mapping.\n");
            goto _err_hash;
        }
    }

    return 0;

_err_hash:
    free(new->path);
_err_path:
    pthread_rwlock_destroy(new->_rwlock);
_err_rwl2:
    free(new->_rwlock);
_err_rwlk:
    pthread_cond_destroy(new->_cond);
_err_cond:
    pthread_mutex_destroy(new->_lock);
_err_lck2:
    free(new->_lock);
_err_lock:
    free(new);

    return -1;
}

/* -------------------------------------------------------------------------- */

public int fs_map(m_view *v, const char *p, size_t l, m_string *s)
{
    return (_fs_map(v, p, l, s, 0));
}

/* -------------------------------------------------------------------------- */

public int fs_remap(m_view *v, const char *p, size_t l, m_string *s)
{
    return _fs_map(v, p, l, s, 1);
}

/* -------------------------------------------------------------------------- */

public int fs_onevent(m_view *v, unsigned int event, void (*function)(void))
{
    return 0;
}

/* -------------------------------------------------------------------------- */

static int _fs_utime(const char *path, time_t mtime, time_t atime)
{
    struct utimbuf time;
    struct stat info;

    if (! atime && ! mtime) return 0;

    if (stat(path, & info) == -1) {
        perror(ERR(_fs_utime, stat));
        return -1;
    }

    time.modtime = (mtime) ? mtime : info.st_mtime;
    time.actime = (atime) ? atime : info.st_atime;

    if (utime(path, & time) == -1) {
        perror(ERR(_fs_utime, utime));
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

public int fs_rename(m_view *v, const char *old, size_t oldlen,
                     const char *new, size_t newlen)
{
    char oldpath[PATH_MAX];
    char newpath[PATH_MAX];
    m_file *f = NULL;
    int physical = 0;

    /* get the full path for the new and old names */
    oldlen = fs_mkpath(& v, old, oldlen, oldpath, sizeof(oldpath));
    if (oldlen == (size_t) -1) return -1;

    newlen = fs_mkpath(& v, new, newlen, newpath, sizeof(newpath));
    if (newlen == (size_t) -1) return -1;

    /* check if the new path already exists */
    if (access(newpath, F_OK) != -1) {
        /* renaming to an existing file name is not allowed */
        debug("fs_rename(): cannot rename to an existing file name.\n");
        return -1;
    }

    /* check if the old name match a physical file */
    if (access(oldpath, F_OK) != -1) physical = 1;

    if (! physical) {
        /* get the old mapping data */
        f = variant_to_pointer(
            trie_lookup(v->_cache, oldpath, oldlen, _fs_refcount_acquire)
        );

        if (! f) {
            debug("fs_rename(): mapping not found.\n");
            return -1;
        }

        /* map it under the new name */
        if (fs_map(v, newpath, newlen, f->data) == -1) {
            /* either the mapping already exists or cannot be created */
            _fs_refcount_unlock(f);
            return -1;
        }

        /* mark the old mapping as deleted */
        f->flags |= FILE_DELETED;

        _fs_refcount_unlock(f);

    } else {
        /* check this is a physical root */
        if (v->flags & VIEW_VIRTUAL) {
            /* physical files cannot be accessed in a virtual view */
            debug("fs_rename(): files cannot be renamed in virtual views.\n");
            return -1;
        }

        /* rename the file */
        if (rename(oldpath, newpath) == -1) {
            perror(ERR(fs_rename, rename));
            return -1;
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

public int fs_delete(m_view *v, const char *p, size_t l)
{
    char fullpath[PATH_MAX];
    int len = 0, physical = 0;
    m_file *f = NULL;

    /* get the full path */
    if ( (len = fs_mkpath(& v, p, l, fullpath, sizeof(fullpath))) == -1)
        return -1;

    /* check if this is a physical file */
    if (access(fullpath, F_OK) != -1) physical = 1;

    if (physical) {
        if (v->flags & VIEW_VIRTUAL) {
            /* physical files cannot be accessed in a virtual view */
            debug("fs_delete(): files cannot be deleted in virtual views.\n");
            return -1;
        }

        if (unlink(fullpath) == -1) {
            perror(ERR(fs_delete, unlink));
            return -1;
        }
    } else {
        f = variant_to_pointer(
            trie_lookup(v->_cache, fullpath, len, _fs_refcount_acquire)
        );

        if (! f) {
            debug("fs_delete(): mapping not found.\n");
            return -1;
        }

        /* mark the mapping as deleted */
        f->flags |= FILE_DELETED;

        _fs_refcount_unlock(f);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

public m_file *fs_closefile(m_file *file)
{
    m_file *ret = NULL;

    if (! file) return NULL;

    /* allow other threads to increment the refcount before us */
    sched_yield();

    /* the file is already being deleted (likely a bug) */
    if (_fs_refcount_lock(file) == -1) {
        debug("fs_closefile(): attempt to close a file being destroyed.\n");
        return NULL;
    }

    if (file->_refcount > 0) file->_refcount --;

    if (! file->_refcount) {
        /* if we are the last user, we need to destroy the file */
        _fs_refcount_breaklock(file);

        if (file->_view) {
            /* the file is not orphaned */
            ret = variant_to_pointer(
                trie_remove(file->_view->_cache, file->path, file->pathlen)
            );
        } else ret = file;

        if (ret) {
            /* close the file and free the metadata */
            if (ret->fd != -1) close(ret->fd);
            string_free(ret->data);
            /* destroy the rwlock */
            pthread_rwlock_destroy(ret->_rwlock);
            free(ret->_rwlock);
            /* destroy the refcount */
            pthread_mutex_destroy(ret->_lock);
            pthread_cond_destroy(ret->_cond);
            free(ret->_lock);
            /* the file is really being closed, update the time */
            if (~ret->flags & FILE_VIRTUAL)
                _fs_utime(ret->path, ret->_last_modified, ret->_last_accessed);
            free(ret->path);
            free(ret);
        }
    } else {
        /* there is still other users, simply unlock the refcount */
        _fs_refcount_unlock(file);
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

public m_view *fs_closeview(m_view *view)
{
    trie_free(view->_cache);
    trie_free(view->_handlers);
    free(view->root); free(view);
    return NULL;
}

/* -------------------------------------------------------------------------- */

public void fs_api_cleanup(void)
{
    fs_closeview(default_view);
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

/* File support will not be compiled in the Concrete Library */
#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
