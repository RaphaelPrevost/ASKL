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

#ifndef M_FILE_H

#define M_FILE_H

#if defined(_ENABLE_FILE) && defined(_ENABLE_TRIE) && defined(HAS_LIBXML)

#include "askl.h"
#include "askl_cbtrie.h"
#include "m_string.h"
#include "m_security.h"

typedef struct m_view {
    char *root;
    size_t rootlen;
    int flags;
    ASKL_Trie *_cache;
    ASKL_Trie *_handlers;
} m_view;

typedef struct m_file {
    unsigned int flags;
    char *path;
    size_t pathlen;
    int fd;
    m_string *data;
    size_t len;
    time_t _last_modified;
    time_t _last_accessed;
    m_view *_view;
    pthread_rwlock_t *_rwlock;
    pthread_mutex_t *_lock;
    pthread_cond_t *_cond;
    int _lockstate;
    unsigned int _refcount;
} m_file;

#define VIEW_VIRTUAL     0x1

#define FILE_VIRTUAL     0x1
#define FILE_DUMPMAP     0x2
#define FILE_PRIVATE     0x4
#define FILE_DELETED     0x8

#define FILE_OPEN_SHARED 0x1
#define FILE_OPEN_CREATE 0x2

#define FILE_LAST_MODIFIED 0x1
#define FILE_LAST_ACCESSED 0x2

#define VIEW_EVENT_CREATE
#define VIEW_EVENT_DELETE
#define VIEW_EVENT_MODIFY
#define VIEW_EVENT_RENAME
#define VIEW_EVENT_FCLOSE

/* -------------------------------------------------------------------------- */

public int fs_api_setup(void);

/* -------------------------------------------------------------------------- */

public m_view *fs_openview(const char *root, size_t rootlen);

/* -------------------------------------------------------------------------- */

public int fs_mkpath(m_view **v, const char *p, size_t l,
                     char *out, size_t len);

/* -------------------------------------------------------------------------- */

public m_file *fs_openfile(m_view *v, const char *p, size_t l, m_auth *a);

/* -------------------------------------------------------------------------- */

public m_file *fs_reopenfile(m_file *f);

/* -------------------------------------------------------------------------- */

public m_file *fs_createfile(m_view *v, const char *p, size_t l);

/* -------------------------------------------------------------------------- */

public int fs_mkdir(m_view *v, const char *p, size_t l);

/* -------------------------------------------------------------------------- */

public int fs_isopened(m_view *v, const char *p, size_t l);

/* -------------------------------------------------------------------------- */

public void fs_getpath(const char *path, size_t len, char *out, size_t outlen);

/* -------------------------------------------------------------------------- */

public void fs_getfilename(const char *path, size_t l, char *out, size_t outlen);

/* -------------------------------------------------------------------------- */

public int fs_isrelativepath(const char *path, size_t len);

/* -------------------------------------------------------------------------- */

public int fs_map(m_view *v, const char *p, size_t l, m_string *s);

/* -------------------------------------------------------------------------- */

public int fs_remap(m_view *v, const char *p, size_t l, m_string *s);

/* -------------------------------------------------------------------------- */

public int fs_rename(m_view *v, const char *old, size_t oldlen,
                     const char *new, size_t newlen);

/* -------------------------------------------------------------------------- */

public int fs_delete(m_view *v, const char *p, size_t l);

/* -------------------------------------------------------------------------- */

public m_file *fs_closefile(m_file *file);

/* -------------------------------------------------------------------------- */

public m_view *fs_closeview(m_view *view);

/* -------------------------------------------------------------------------- */

public void fs_api_cleanup(void);

/* -------------------------------------------------------------------------- */

/* _ENABLE_TRIE && HAS_LIBXML */
#endif

#endif
