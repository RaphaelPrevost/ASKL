/*******************************************************************************
 *  Wamigo Daemon                                                              *
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

#include "wamigo_plugin.h"

struct _scan {
    char *dir;
    char *ext;
    int upd;
    unsigned int min;
    unsigned int max;
};

/* -------------------------------------------------------------------------- */
#ifdef WIN32
/* -------------------------------------------------------------------------- */

static void _get_utc_timestamp(FILETIME *in, unsigned long *out)
{
    FILETIME t;
    ULARGE_INTEGER date, adjust;

    if (! in || ! out) return;

    /* convert to UTC time */
    LocalFileTimeToFileTime(in, & t);

    /* convert to UNIX timestamp */
    date.HighPart = t.dwHighDateTime;
    date.LowPart = t.dwLowDateTime;
    adjust.QuadPart = 11644473600000LL * 10000;
    date.QuadPart -= adjust.QuadPart;
    *out = (unsigned long) (date.QuadPart / 10000000);

    return;
}

/* -------------------------------------------------------------------------- */

static void *_start_scan(void *scan)
{
    /** @brief this thread scans for discrepancies in the folder, then dies */

    WCHAR folder[MAX_PATH];
    WCHAR subfolder[MAX_PATH];
    WCHAR full_path[MAX_PATH];
    WCHAR directory[MAX_PATH];
    WCHAR extension[MAX_PATH];
    char utf8[MAX_PATH];
    int sz = 0, no_match = 0;
    UCHAR attr[8];
    LPWSTR last = NULL;
    HANDLE d = INVALID_HANDLE_VALUE;
    struct _WIN32_FIND_DATAW ffd;
    unsigned long timestamp = 0, rootlen = 0;
    m_queue *q = NULL;
    m_string *syncdata = NULL;
    struct _scan *s = (struct _scan *) scan;

    if (! s || ! s->dir || ! s->ext) return NULL;

    /* check the extension format is valid */
    if (s->ext[0] != '*' || (strlen(s->ext) > 1 && s->ext[1] != '.')) {
        debug("_start_scan(): bad file extension.\n");
        return NULL;
    }

    /* convert everything to from the current locale to wide chars */
    sz = MultiByteToWideChar(CP_OEMCP, MB_ERR_INVALID_CHARS, s->dir, -1,
                             directory, MAX_PATH);
    if (! (rootlen = sz) ) {
        debug("_start_scan(): cannot convert directory to unicode.\n");
        return NULL;
    }

    sz = MultiByteToWideChar(CP_OEMCP, MB_ERR_INVALID_CHARS, s->ext, -1,
                             extension, MAX_PATH);
    if (! sz) {
        debug("_start_scan(): cannot convert extension to unicode.\n");
        return NULL;
    }

    /* allocate the stack */
    if (! (q = queue_alloc()) ) return NULL;

    /* open the Wamigo folder for listing */
    PathCombineW(folder, directory, extension);
    if ( (d = FindFirstFileW(folder, & ffd)) == INVALID_HANDLE_VALUE) {
        /* try again, without extension */
        PathCombineW(folder, directory, L"*"); no_match = 1;
        if ( (d = FindFirstFileW(folder, & ffd)) == INVALID_HANDLE_VALUE) {
            debug("_start_scan(): cannot open top directory for listing.\n");
            return NULL;
        }
    }

    wcscpy(subfolder, directory);

    do {
        /* avoid special files */
        if (! wcscmp(ffd.cFileName, L".") || ! wcscmp(ffd.cFileName, L".."))
            goto _next;

        /* handle subfolders */
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

            /* push the current folder in the queue */
            if (no_match) {
               /* HACK the bottom 2 bits of kernel handles are always 0 */
               d = (void *) ((ULONG_PTR) d | 0x1); no_match = 0;
            }

            queue_unpop(q, (void *) d);

            /* open the subfolder */
            PathCombineW(subfolder, subfolder, ffd.cFileName);
            PathCombineW(folder, subfolder, extension);
            if ( (d = FindFirstFileW(folder, & ffd)) == INVALID_HANDLE_VALUE) {
                PathCombineW(folder, subfolder, L"*"); no_match = 1;
                if ( (d = FindFirstFileW(folder, & ffd)) == INVALID_HANDLE_VALUE) {
                   debug("_start_scan(): error opening folder %ls.\n", folder);
                   goto _cleanup;
                }
            }

            continue;
        } else if (no_match) goto _next;

        PathCombineW(full_path, subfolder, ffd.cFileName);

        fprintf(stderr, "Wamigo: checking file %ls\n", full_path + rootlen);
        if (s->max && ffd.nFileSizeLow > s->max) fprintf(stderr, "TOO BIG\n");
        if (s->min && ffd.nFileSizeLow < s->min) fprintf(stderr, "TOO SMALL\n");
        if (~s->upd & WAMIGO_SCAN_SYNC) goto _next;

        /* get the mtime timestamp (GMT+0) */
        _get_utc_timestamp(& ffd.ftLastWriteTime, & timestamp);
        fprintf(stderr, "Timestamp: %ld\n", timestamp);

        /* convert the filename to UTF-8 */
        WideCharToMultiByte(CP_UTF8, 0x0, full_path + rootlen,
                            lstrlenW(full_path) - rootlen,
                            utf8, MAX_PATH, NULL, FALSE);

        /* check the Wamigo NTFS attribute */
        if (! wamigo_get(full_path, "wamigo", attr, sizeof(attr))) {
            /* new file */
            //syncdata = string_catfmt(syncdata, ",0,\"/%s\",%li,3\n", utf8, timestamp);
            goto _next;
        }

        if (! memcmp(attr, "XFER", 4)) {

        } else if (! memcmp(attr, "SYNC", 4)) {
            //syncdata = string_catfmt(syncdata, "%lli,0,\"%s\",%li,0\n", 0, utf8, timestamp);
        }

_next:
        /* try to get the next file */
        if (! FindNextFileW(d, & ffd)) {
            do {
                if (! (d = (HANDLE) queue_pop(q)) ) goto _cleanup;

                if ((ULONG_PTR) d & 0x1) {
                    /* HACK the bottom 2 bits of kernel handles are always 0 */
                   d = (void *) ((ULONG_PTR) d & ~0x1); no_match = 1;
                }

                if ( (last = wcsrchr(subfolder, L'\\')) ) *last = L'\0';
            } while (! FindNextFileW(d, & ffd));
        }

    } while (wamigo_running());

_cleanup:
    q = queue_free(q);

    fprintf(stderr, "Wamigo: finished scanning folder %ls\n", s->dir);
    //fprintf(stderr, "SYNC DATA: %s\n", CSTR(syncdata));

    if (s->upd & WAMIGO_SCAN_INIT) wamigo_monitor_directory(s->dir);

    free(s->dir); free(s->ext);
    free(s);

    return 0;
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

static void *_start_scan(void *scan)
{
    struct _scan *s = (struct _scan *) scan;

    free(s->dir); free(s->ext);
    free(s);

    return NULL;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

private int wamigo_scan_directory(const char *dir, const char *ext, int sync)
{
    struct _scan *s = NULL;
    pthread_t thread;
    pthread_attr_t attr;
    char *p = NULL;
    size_t extlen = 0, dirlen = 0;
    unsigned int lim_min = 0, lim_max = 0;

    if (! dir || ! (dirlen = strlen(dir))) return -1;

    if (! ext) ext = "*";

    /* remove trailing separator in the folder name */
    if (isdirsepchr(dir[dirlen - 1])) dirlen --;

    /* parse the limit format in the extension */
    if ( (p = memchr(ext, '/', strlen(ext))) ) {
         extlen = p - ext; lim_min = strtol(++ p, & p, 10);
         if (p && *p == ':') lim_max = strtol(++ p, NULL, 10);
    } else extlen = strlen(ext);

    fprintf(stderr, "Limit min=%i Limit max=%i\n", lim_min, lim_max);

    if (pthread_attr_init(& attr) != 0) {
        perror(ERR(wamigo_scan_directory, pthread_attr_init));
        return -1;
    }

    if (pthread_attr_setdetachstate(& attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror(ERR(wamigo_scan_directory, pthread_attr_setdetachstate));
        goto _edetch;
    }

    if (! (s = malloc(sizeof(*s))) ) {
        perror(ERR(wamigo_scan_directory, malloc));
        goto _ealloc;
    }

    if (! (s->dir = strndup(dir, dirlen)) )
        goto _edups1;

    if (! (s->ext = strndup(ext, extlen)) )
        goto _edups2;

    s->upd = sync;
    s->min = lim_min;
    s->max = lim_max;

    fprintf(stderr, "Scanning: dir=%s ext=%s\n", s->dir, s->ext);

    if (pthread_create(& thread, & attr, _start_scan, s) == -1) {
        perror(ERR(wamigo_scan_directory, pthread_create));
        goto _ecreat;
    }

    return 0;

_ecreat:
    free(s->ext);
_edups2:
    free(s->dir);
_edups1:
    free(s);
_ealloc:
_edetch:
    pthread_attr_destroy(& attr);
    return -1;
}

/* -------------------------------------------------------------------------- */
