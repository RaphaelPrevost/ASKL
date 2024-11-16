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

static pthread_mutex_t mx_monitor = PTHREAD_MUTEX_INITIALIZER;
static unsigned int monitor_running = 0;
static pthread_t monitor;

#ifdef WIN32
static HANDLE completion_event;
static char buffer[65536];
static WCHAR monitored_dir[MAX_PATH];
static DWORD bytes;
static OVERLAPPED overlapped;
static HANDLE dir;
#endif

/* -------------------------------------------------------------------------- */
#ifdef WIN32
/* -------------------------------------------------------------------------- */

static VOID CALLBACK _notification(DWORD err, DWORD bytes, LPOVERLAPPED o)
{
    char *p = NULL;
    FILE_NOTIFY_INFORMATION *fni = NULL;
    struct _stat s;
    WCHAR buf[MAX_PATH];
    WCHAR full_path[MAX_PATH];
    char filename[MAX_PATH];
    size_t sz = 0;
    LPCWSTR f = NULL;
    BOOL shortname = FALSE;

    if (err == ERROR_OPERATION_ABORTED) {
        SetEvent(completion_event);
        return;
    }

    /* possibly overflow (too much notifications) */
    if (! bytes) { SetEvent(completion_event); return; }

    /* process the buffer */
    p = (char *) buffer;
    while (1) {

        fni = (FILE_NOTIFY_INFORMATION *) p;

        /* handle possible short names */
        f = PathFindFileNameW(fni->FileName);
        if (lstrlenW(f) <= 12 && wcschr(f, L'~')) {
            if (GetLongPathNameW(f, buf, sizeof(buf) / sizeof(WCHAR)) <= 0) {
                PathCombineW(full_path, monitored_dir, buf);
                shortname = TRUE;
            }
        }

        if (! shortname) {
            PathCombineW(full_path, monitored_dir, fni->FileName);
        }

        sz = WideCharToMultiByte(CP_OEMCP, 0x0, full_path, lstrlenW(full_path),
                                 filename, MAX_PATH, NULL, FALSE);
        if (! sz) {
            SetEvent(completion_event); return;
        }

        fs_getfilename(filename, strlen(filename), filename, sizeof(filename));
        fprintf(stderr, "wide char fullpath: %ls multibyte filename: %s\n",
                full_path, filename);

        switch (fni->Action) {

        case FILE_ACTION_ADDED: {
            fprintf(stderr, "Wamigo: file %ls was added.\n", full_path);
            sleep(1);
            wamigo_call_upload(filename);
        }
        break;

        case FILE_ACTION_REMOVED: {
            /* queue a deletion notification */
            fprintf(stderr, "Wamigo: file %ls was removed.\n", full_path);
        }
        break;

        case FILE_ACTION_MODIFIED: {
            /* queue an update */
            fprintf(stderr, "Wamigo: file %ls was modified.\n", full_path);

            /* tag the file as being synced */
        }
        break;

        case FILE_ACTION_RENAMED_OLD_NAME: {
            /* queue a renaming notification */
            fprintf(stderr, "Wamigo: file %ls was renamed.\n", full_path);
        }
        break;

        case FILE_ACTION_RENAMED_NEW_NAME: {
            /* finish renaming */
        }
        break;

        default: /* error */
        fprintf(stderr, "bad notification\n");
        }

        /* get timestamp */
        if (! _wstat(full_path, & s)) {
            /* file exists, get the last modified time */

            if (! wamigo_set(full_path, "wamigo", "SYNC", sizeof("SYNC"))) {
                debug("_notification(): wamigo_set() failed.\n");
            }
        }

        if (! fni->NextEntryOffset)
            break;
        p += fni->NextEntryOffset;
    }
}

/* -------------------------------------------------------------------------- */

static void *_monitoring(void *d)
{
    /** @brief this thread monitors the Wamigo folder and push notifications */

    SECURITY_ATTRIBUTES sec_attr;
    BOOL ret = FALSE;
    size_t sz = 0;
    const char *directory = (char *) d;

    if (! directory) return NULL;

    /* convert everything to from the current locale to wide chars */
    sz = MultiByteToWideChar(CP_OEMCP, MB_ERR_INVALID_CHARS, directory, -1,
                             monitored_dir, MAX_PATH);
    if (! sz) goto _error;

    /* open the directory to monitor */
    dir = CreateFileW(monitored_dir,
                      FILE_LIST_DIRECTORY,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      NULL,
                      OPEN_EXISTING,
                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                      NULL);

    if (dir == INVALID_HANDLE_VALUE) goto _error;

    fprintf(stderr, "watching directory: %ls\n", monitored_dir);

    while (wamigo_running()) {

        memset(& overlapped, 0, sizeof(overlapped));
        memset(buffer, 0, sizeof(buffer));
        sec_attr.nLength = sizeof(sec_attr);
        sec_attr.bInheritHandle = FALSE;
        sec_attr.lpSecurityDescriptor = NULL;

        /* create the completion event */
        completion_event = CreateEvent(& sec_attr, TRUE, FALSE, NULL);

        /* wait for notifications */
        ret = ReadDirectoryChangesW(dir, buffer, sizeof(buffer), FALSE,
                                    FILE_NOTIFY_CHANGE_LAST_WRITE |
                                    FILE_NOTIFY_CHANGE_CREATION |
                                    FILE_NOTIFY_CHANGE_FILE_NAME,
                                    & bytes,
                                    & overlapped,
                                    _notification);
        if (ret) {
            /* the call was successful, wait on the completion event */
            fprintf(stderr, "waiting for the completion event\n");
            WaitForSingleObjectEx(completion_event, INFINITE, TRUE);
        }

        /* close the completion event and try again */
        CloseHandle(completion_event);
    }

    CloseHandle(dir);

_error:
    free(d);

    return NULL;
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

static void *_monitoring(UNUSED void *d)
{
    return NULL;
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

private int wamigo_monitor_directory(const char *dir)
{
    char *d = NULL;

    pthread_mutex_lock(& mx_monitor);
    if (monitor_running) {
        pthread_mutex_unlock(& mx_monitor);
        return -1;
    }

    if (! (d = strndup(dir, strlen(dir))) ) goto _err_strdup;

    if (pthread_create(& monitor, NULL, _monitoring, (void *) d) == -1) {
        perror(ERR(wamigo_monitor_directory, pthread_create));
        goto _err_thread;
    }

    monitor_running = 1;

    pthread_mutex_unlock(& mx_monitor);

    return 0;

_err_thread:
    pthread_mutex_unlock(& mx_monitor);
_err_strdup:
    free(d);

    return -1;
}

/* -------------------------------------------------------------------------- */

private void wamigo_stop_monitoring(void)
{
    unsigned int running = 0;

    pthread_mutex_lock(& mx_monitor);
        running = monitor_running;
        monitor_running = 0;
    pthread_mutex_unlock(& mx_monitor);

    #ifdef WIN32
    SetEvent(completion_event);
    #endif

    if (running) pthread_join(monitor, NULL);
}

/* -------------------------------------------------------------------------- */
