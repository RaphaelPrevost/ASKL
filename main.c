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

#include "askl.h"
#include "askl_server.h"

/* -------------------------------------------------------------------------- */
#ifndef WIN32 /* UNIX */
/* -------------------------------------------------------------------------- */

static int interrupt = 0;

/* UNIX daemon functions */
static void daemonize(int forcefork);
static void _sigusr1_handler(UNUSED int _dummy);

#ifdef __APPLE__
#define ASKL_OS " for Mac OS X"
#else
#define ASKL_OS ""
#endif

#define MAIN_PRINTVER \
"\nASKL.\n" \
"version "ASKL_VERSION" ["__DATE__"]"ASKL_OS"\n" \
"Copyright (c) 2025 Raphael Prevost, all rights reserved.\n\n"

#define MAIN_OPTSHRT_DAEMON "-d"
#define MAIN_OPTLONG_DAEMON "--daemon"
#define MAIN_OPTSHRT_PRINTV "-v"
#define MAIN_OPTLONG_PRINTV "--version"
#if defined(_ENABLE_CONFIG) && defined(HAS_LIBXML)
#define MAIN_OPTIONS_CNFDBG "--debug"
#define MAIN_OPTIONS_CNFPRD "--production"
#endif
#define MAIN_OPTSHRT_PRINTH "-h"
#define MAIN_OPTLONG_PRINTH "--help"

#if defined(_ENABLE_CONFIG) && defined(HAS_LIBXML)
#define MAIN_PRINTHLP \
"\nASKL.\n" \
"version "ASKL_VERSION" ["__DATE__"]"ASKL_OS"\n" \
"Copyright (c) 2025 Raphael Prevost, all rights reserved.\n\n" \
"Usage: %s [OPTION]\n" \
"Start ASKL.\n" \
"\nOptions:\n\n" \
"\t"MAIN_OPTSHRT_DAEMON", "MAIN_OPTLONG_DAEMON \
"\t\tstart the server as a background process\n" \
"\t"MAIN_OPTSHRT_PRINTV", "MAIN_OPTLONG_PRINTV \
"\t\toutput version information and exit\n" \
"\t"MAIN_OPTSHRT_PRINTH", "MAIN_OPTLONG_PRINTH \
"\t\tdisplay this help and exit\n" \
"\t"MAIN_OPTIONS_CNFDBG \
"\t\t\tforcefully select the debug configuration\n" \
"\t"MAIN_OPTIONS_CNFPRD \
"\t\tforcefully select the production configuration\n\n"
#else
#define MAIN_PRINTHLP \
"\nASKL.\n" \
"version "ASKL_VERSION" ["__DATE__"]"ASKL_OS"\n" \
"Copyright (c) 2025 Raphael Prevost, all rights reserved.\n\n" \
"Usage: %s [OPTION]\n" \
"Start ASKL.\n" \
"\nOptions:\n\n" \
"\t"MAIN_OPTSHRT_DAEMON", "MAIN_OPTLONG_DAEMON \
"\t\tstart the server as a background process\n" \
"\t"MAIN_OPTSHRT_PRINTV", "MAIN_OPTLONG_PRINTV \
"\t\toutput version information and exit\n" \
"\t"MAIN_OPTSHRT_PRINTH", "MAIN_OPTLONG_PRINTH \
"\t\tdisplay this help and exit\n\n"
#endif

/* -------------------------------------------------------------------------- */
#else /* WIN32 */
/* -------------------------------------------------------------------------- */

/* WIN32 specifics */
#ifndef DISABLE_MAX_PRIVILEGE
    #define DISABLE_MAX_PRIVILEGE 0x01
#endif

#define WINMAIN_SERVICENAME \
"ASKL"
#define WINMAIN_REGISTRYKEY \
"SYSTEM\\CurrentControlSet\\Services\\ASKL\\Parameters"

typedef BOOL(WINAPI * fpCreateRestrictedToken) (HANDLE, DWORD, DWORD,
                                                PSID_AND_ATTRIBUTES,
                                                DWORD,
                                                PLUID_AND_ATTRIBUTES,
                                                DWORD,
                                                PSID_AND_ATTRIBUTES, PHANDLE);

static LPSTR service_name = NULL;
static SERVICE_STATUS_HANDLE service_controller;
static HANDLE termination_event;

/* NT service functions */
static BOOL _service_install(VOID);
static BOOL _service_uninstall(VOID);
static BOOL _service_start(VOID);
static BOOL _service_stop(VOID);
static VOID _service_status(DWORD s, DWORD r);
static VOID _service_die(VOID);
static void WINAPI _service_ctl(DWORD cmd);
static VOID WINAPI _service_main(DWORD argc, LPSTR *argv);

#define MAIN_PRINTVER \
"\nASKL.\n" \
"version "ASKL_VERSION" ["__DATE__"] for Windows NT\n" \
"Copyright (c) 2025 Raphael Prevost, all rights reserved.\n\n"

#define MAIN_OPTSHRT_DAEMON "/d"
#define MAIN_OPTLONG_DAEMON "/daemon"
#define MAIN_OPTSHRT_PRINTV "/v"
#define MAIN_OPTLONG_PRINTV "/version"
#define MAIN_OPTSHRT_PRINTH "/h"
#define MAIN_OPTLONG_PRINTH "/help"
#define MAIN_OPTSHRT_INSTAL "/i"
#define MAIN_OPTLONG_INSTAL "/install"
#define MAIN_OPTSHRT_UNINST "/u"
#define MAIN_OPTLONG_UNINST "/uninstall"
#define _WIN32_OPTLONG_STRT "/start"
#define _WIN32_OPTSHRT_STRT "/s"
#define _WIN32_OPTLONG_STOP "/stop"
#define _WIN32_OPTSHRT_STOP "/q"
#define _WIN32_OPTLONG_PRIV "/privileged"
#define _WIN32_OPTSHRT_PRIV "/p"

#if defined(_ENABLE_CONFIG) && defined(HAS_LIBXML)
#define MAIN_OPTIONS_CNFDBG "/debug"
#define MAIN_OPTIONS_CNFPRD "/production"
#endif

#if defined(_ENABLE_CONFIG) && defined(HAS_LIBXML)
#define MAIN_PRINTHLP \
"\nASKL.\n" \
"version "ASKL_VERSION" ["__DATE__"] for Windows NT\n" \
"Copyright (c) 2025 Raphael Prevost, all rights reserved.\n\n" \
"Usage: %s [OPTION]\n" \
"Start ASKL.\n" \
"\nOptions:\n\n" \
"\t"MAIN_OPTSHRT_INSTAL", "MAIN_OPTLONG_INSTAL \
"\t\tinstall the NT service\n" \
"\t"MAIN_OPTSHRT_UNINST", "MAIN_OPTLONG_UNINST \
"\t\tuninstall the NT service\n" \
"\t"_WIN32_OPTSHRT_STRT", "_WIN32_OPTLONG_STRT \
"\t\tstart the NT service\n" \
"\t"_WIN32_OPTSHRT_STOP", "_WIN32_OPTLONG_STOP \
"\t\tstop the NT service\n" \
"\t"MAIN_OPTSHRT_PRINTV", "MAIN_OPTLONG_PRINTV \
"\t\toutput version information and exit\n" \
"\t"MAIN_OPTSHRT_PRINTH", "MAIN_OPTLONG_PRINTH \
"\t\tdisplay this help and exit\n" \
"\t"MAIN_OPTIONS_CNFDBG \
"\t\t\tforcefully select the debug configuration\n" \
"\t"MAIN_OPTIONS_CNFPRD \
"\t\tforcefully select the production configuration\n\n"
#else
#define MAIN_PRINTHLP \
"\nASKL.\n" \
"version "ASKL_VERSION" ["__DATE__"] for Windows NT\n" \
"Copyright (c) 2025 Raphael Prevost, all rights reserved.\n\n" \
"Usage: %s [OPTION]\n" \
"Start the ASKL.\n" \
"\nOptions:\n\n" \
"\t"MAIN_OPTSHRT_INSTAL", "MAIN_OPTLONG_INSTAL \
"\t\tinstall the NT service\n" \
"\t"MAIN_OPTSHRT_UNINST", "MAIN_OPTLONG_UNINST \
"\t\tuninstall the NT service\n" \
"\t"_WIN32_OPTSHRT_STRT", "_WIN32_OPTLONG_STRT \
"\t\tstart the NT service\n" \
"\t"_WIN32_OPTSHRT_STOP", "_WIN32_OPTLONG_STOP \
"\t\tstop the NT service\n" \
"\t"MAIN_OPTSHRT_PRINTV", "MAIN_OPTLONG_PRINTV \
"\t\toutput version information and exit\n" \
"\t"MAIN_OPTSHRT_PRINTH", "MAIN_OPTLONG_PRINTH \
"\t\tdisplay this help and exit\n\n"
#endif

#ifdef _ENABLE_PRIVILEGE_SEPARATION
struct _child {
    SOCKET in, out;
} child;

static VOID _spawn_child(UNUSED VOID *dummy);
#endif

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

/* COMMON */
static void main_process(int option_daemon, int argc, char **argv);

/* -------------------------------------------------------------------------- */
/* ASKL SERVER MAIN */
/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    /* command line options */
    int option_daemon = 0, option_version = 0, option_help = 0;

    #if defined(HAS_LIBXML) && defined(_ENABLE_XML)
    LIBXML_TEST_VERSION
    #endif

    /* eval the command line options */
    while (argc --) {
        if (! strcmp(argv[argc], MAIN_OPTSHRT_DAEMON) ||
            ! strcmp(argv[argc], MAIN_OPTLONG_DAEMON)) {
            option_daemon = 1;
            break;
        }

        #if defined(_ENABLE_CONFIG) && defined(HAS_LIBXML)
        if (! strcmp(argv[argc], MAIN_OPTIONS_CNFDBG)) {
            config_force_profile(CONFIG_PROFILE_DBG);
        }
        if (! strcmp(argv[argc], MAIN_OPTIONS_CNFPRD)) {
            config_force_profile(CONFIG_PROFILE_PRD);
        }
        #endif

        if (! strcmp(argv[argc], MAIN_OPTSHRT_PRINTV) ||
            ! strcmp(argv[argc], MAIN_OPTLONG_PRINTV))
            option_version = 1;

        if (! strcmp(argv[argc], MAIN_OPTSHRT_PRINTH) ||
            ! strcmp(argv[argc], MAIN_OPTLONG_PRINTH))
            option_help = 1;

        #ifdef WIN32
        if (! strcmp(argv[argc], MAIN_OPTSHRT_INSTAL) ||
            ! strcmp(argv[argc], MAIN_OPTLONG_INSTAL)) {
            if (_service_install()) {
                printf(WINMAIN_SERVICENAME " was successfully installed.\n");
                exit(EXIT_SUCCESS);
            } else {
                printf(WINMAIN_SERVICENAME " installation failed.\n");
                exit(EXIT_FAILURE);
            }
        }

        if (! strcmp(argv[argc], MAIN_OPTSHRT_UNINST) ||
            ! strcmp(argv[argc], MAIN_OPTLONG_UNINST)) {
            if (_service_uninstall()) {
                printf(WINMAIN_SERVICENAME " was successfully uninstalled.\n");
                exit(EXIT_SUCCESS);
            } else {
                printf(WINMAIN_SERVICENAME " uninstallation failed.\n");
                exit(EXIT_FAILURE);
            }
        }

        if (! strcmp(argv[argc], _WIN32_OPTSHRT_STRT) ||
            ! strcmp(argv[argc], _WIN32_OPTLONG_STRT)) {
            if (_service_start()) {
                printf(WINMAIN_SERVICENAME " was successfully started.\n");
                exit(EXIT_SUCCESS);
            } else {
                printf(WINMAIN_SERVICENAME " cannot be started.\n");
                exit(EXIT_FAILURE);
            }
        }

        if (! strcmp(argv[argc], _WIN32_OPTSHRT_STOP) ||
            ! strcmp(argv[argc], _WIN32_OPTLONG_STOP)) {
            if (_service_uninstall()) {
                printf(WINMAIN_SERVICENAME " was successfully stopped.\n");
                exit(EXIT_SUCCESS);
            } else {
                printf(WINMAIN_SERVICENAME " cannot be stopped.\n");
                exit(EXIT_FAILURE);
            }
        }

        if (! strcmp(argv[argc], _WIN32_OPTSHRT_PRIV) ||
            ! strcmp(argv[argc], _WIN32_OPTLONG_PRIV)) {
            option_daemon = 2;
            break;
        }
        #endif
    }

    if (option_version) {
        /* greeting message ! */
        printf(MAIN_PRINTVER);
        exit(EXIT_SUCCESS);
    }

    if (option_help) {
        printf(MAIN_PRINTHLP, argv[0]);
        exit(EXIT_SUCCESS);
    }

    main_process(option_daemon, argc, argv);

    exit(EXIT_SUCCESS);
}

/* -------------------------------------------------------------------------- */
#ifndef WIN32
/* -------------------------------------------------------------------------- */

static void main_process(int option_daemon, UNUSED int argc, UNUSED char **argv)
{
    #ifdef _ENABLE_PRIVILEGE_SEPARATION
    pid_t pid = 0;
    int sockpair[2];
    struct passwd *nobody = NULL;
    #endif
    char cwd[PATH_MAX];

    int syslog[2]; pid_t logger = 0;
    sigset_t mask, oldmask;

    /* timezone */
    tzset();

    if (option_daemon) daemonize(1);
    else {
        /* save the current directory */
        if (getcwd(cwd, sizeof(cwd))) {
            if (! (working_directory = malloc(strlen(cwd) + 1))) {
                perror(ERR(daemonize, malloc));
            } else memcpy(working_directory, cwd, strlen(cwd) + 1);
        } else perror(ERR(daemonize, getcwd));
    }

    #ifdef _ENABLE_PRIVILEGE_SEPARATION
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == -1) {
        perror(ERR(main, socketpair)); exit(EXIT_FAILURE);
    }

    /* fork an unprivileged child */
    pid = fork();

    switch (pid) {

    case -1:
        #ifdef DEBUG
        perror(ERR(main, fork));
        #endif
        exit(EXIT_FAILURE);

    case 0:
        close(sockpair[1]);
        __server_set_privileged_channel(sockpair[0]);

        /* child, drop privileges */
        if ( (nobody = getpwnam("nobody")) && nobody->pw_uid) {
            if (setuid(nobody->pw_uid) == -1) {
                perror(ERR(main, setuid));
                /* try to drop to the original uid, at least */
                if (setuid(getuid()) == -1) {
                    perror(ERR(main, setuid));
                    exit(EXIT_FAILURE);
                }
            }
        } else {
            /* there is no 'nobody', try to fallback to the original uid */
            if (setuid(getuid()) == -1) {
                perror(ERR(main, setuid));
                exit(EXIT_FAILURE);
            }
        }

    #endif

        if (option_daemon) {
            /* redirect stderr to syslog using the 'logger' system command */
            if (pipe(syslog)) { perror(ERR(main, pipe)); exit(EXIT_FAILURE); }

            if ( (logger = fork()) == 0) {
                /* close the unused write descriptor of the pipe */
                close(syslog[1]);

                /* redirect stdin to the pipe */
                if (dup2(syslog[0], STDIN_FILENO)) {
                    perror(ERR(main, dup2)); exit(EXIT_FAILURE);
                }

                /* start the logger */
                if (execlp("logger", "logger", "-p", "daemon.alert", NULL)) {
                    perror(ERR(main, execlp)); exit(EXIT_FAILURE);
                }

                /* NOTREACHED */
                exit(EXIT_SUCCESS);

            } else if (logger == -1) {
                perror(ERR(main, fork)); exit(EXIT_FAILURE);
            } else {
                /* close the unused read descriptor of the pipe */
                close(syslog[0]);
                /* redirect stderr to the logger stdin */
                dup2(syslog[1], STDERR_FILENO);
            }
        }

        if (server_init() == 0) {
            if (! option_daemon) {

                fprintf(stderr, "-- Interactive session - Press Q to exit.\n");

                while (getchar() != 'q');

                fprintf(stderr, "-- End of the interactive session\n");

            } else {

                /* wait for the SIGUSR1 signal */
                sigemptyset(& mask); sigaddset(& mask, SIGUSR1);

                signal(SIGUSR1, _sigusr1_handler);

                sigprocmask(SIG_BLOCK, & mask, & oldmask);

                while (! interrupt) sigsuspend(& oldmask);

                sigprocmask(SIG_UNBLOCK, & mask, NULL);

            }
        }

        server_exit();

        if (option_daemon) {
            /* give enough time to logger to print the
               last stderr lines then kill it */
            fflush(stderr); sleep(1);
            kill(logger, SIGKILL); wait(NULL);
        }

    #ifdef _ENABLE_PRIVILEGE_SEPARATION
        server_privileged_call(OP_EXIT, NULL, 0);

        /* we used the same descriptor, clean only once */
        close(sockpair[0]);
        break;

    default:
        /* ignore SIGUSR1, so the child handle it */
        signal(SIGUSR1, SIG_IGN);

        /* father, handle privileges requests and wait for the child */
        close(sockpair[0]);
        __server_privileged_process(sockpair[1]);
        wait(NULL);
    }
    #endif

    free(working_directory);
}

/* -------------------------------------------------------------------------- */

static void daemonize(int forcefork)
{
    /** @brief This daemonizes the current process */

    pid_t pid;
    struct rlimit limit;
    char cwd[PATH_MAX];

    /* set to background */
    if (forcefork || getppid() != 1) {

        switch( (pid = fork()) ) {
            case -1: perror(ERR(daemonize, fork)); exit(EXIT_FAILURE); break;
            case  0: break;
            default: exit(EXIT_SUCCESS);
        }

        /* run the processus in a new session */
        setpgid(0, 0);

        switch( (pid = fork()) ) {
            case -1: perror(ERR(daemonize, fork)); exit(EXIT_FAILURE); break;
            case  0: break;
            default: exit(EXIT_SUCCESS);
        }

        /* ignore terminal signals */
        if (signal(SIGTTIN, SIG_IGN) == SIG_ERR) {
            perror(ERR(daemonize, signal)); exit(EXIT_FAILURE);
        }

        if (signal(SIGTTOU, SIG_IGN) == SIG_ERR) {
            perror(ERR(daemonize, signal)); exit(EXIT_FAILURE);
        }

        if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
            perror(ERR(daemonize, signal)); exit(EXIT_FAILURE);
        }

        if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
            perror(ERR(daemonize, signal)); exit(EXIT_FAILURE);
        }
    }

    /* save the current directory */
    if (getcwd(cwd, sizeof(cwd))) {
        if (! (working_directory = malloc(strlen(cwd) + 1))) {
            perror(ERR(daemonize, malloc));
        } else memcpy(working_directory, cwd, strlen(cwd) + 1);
    } else perror(ERR(daemonize, getcwd));

    /* change directory to / and close all file descriptors */
    if (chdir("/")) {
        perror(ERR(daemonize, chdir)); exit(EXIT_FAILURE);
    }

    if (getrlimit(RLIMIT_NOFILE, & limit) == -1) {
        perror(ERR(daemonize, getrlimit)); exit(EXIT_FAILURE);
    }

    while (limit.rlim_cur --) { close(limit.rlim_cur); }
}

/* -------------------------------------------------------------------------- */

static void _sigusr1_handler(UNUSED int _dummy)
{
    interrupt = 1;
}

/* -------------------------------------------------------------------------- */
#else /* WIN32 */
/* -------------------------------------------------------------------------- */

static void main_process(int option_daemon, int argc, char **argv)
{
    WSADATA winsock;
    HKEY key;
    DWORD error = 0, len = PATH_MAX, type = 0;
    char _working_directory[PATH_MAX];
    #ifdef _ENABLE_PRIVILEGE_SEPARATION
    SOCKET sockpair[2];
    #endif
    SERVICE_TABLE_ENTRY service_table[] =
    {
        { WINMAIN_SERVICENAME, _service_main },
        { NULL,                NULL          }
    };

    WSAStartup(MAKEWORD(2, 0), & winsock);

    /* get the working directory */
    error = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        WINMAIN_REGISTRYKEY,
        0,
        KEY_QUERY_VALUE,
        & key
    );

    if (error != ERROR_SUCCESS) {
        working_directory = ".";
        fprintf(
            stderr,
            ERR(main_process, RegOpenKeyEx)": %s\n",
            strerror(error)
        );
    } else {
        error = RegQueryValueEx(
            key,
            "ASKLPath",
            0,
            & type,
            _working_directory,
            & len
        );

        if (error != ERROR_SUCCESS) {
            working_directory = ".";
            fprintf(
                stderr,
                ERR(main_process, RegQueryValueEx)": %s\n",
                strerror(error)
            );
        } else working_directory = (type == REG_SZ) ? _working_directory : ".";

        RegCloseKey(key);
    }

    if (option_daemon == 1) {
        #ifdef _ENABLE_PRIVILEGE_SEPARATION
        /* check if the handles have been set */
        if (argc - 2 <= 0) goto _end;

        sockpair[1] = (SOCKET) atoi(argv[argc - 1]);

        /* get the unprivileged socket */
        __server_set_privileged_channel(sockpair[1]);

        /* get the termination event */
        termination_event = (HANDLE) atoi(argv[argc - 2]);

        if (server_init() == 0) {
            WaitForSingleObject(termination_event, INFINITE);
            CloseHandle(termination_event);
        }

        server_exit();

        /* tell the NT service we are shutting down */
        server_privileged_call(OP_EXIT, NULL, 0);

        /* close the unprivileged socket */
        closesocket(sockpair[1]);

        #else
        printf(
            "Please start the " WINMAIN_SERVICENAME " service "
            "using the Microsoft Windows Administrative Tools.\n"
        );
        #endif

    } else if (option_daemon == 2) {
        #ifdef _ENABLE_PRIVILEGE_SEPARATION
        /* check if the pipe handles have been set */
        if (argc - 1 <= 0) goto _end;

        sockpair[0] = (SOCKET) atoi(argv[argc - 1]);

        /* process privilege elevation requests from the unprivileged process */
        __server_privileged_process(sockpair[0]);

        /* close the privileged end of the connection */
        closesocket(sockpair[0]);
        #else
        printf(
            "Please start the " WINMAIN_SERVICENAME " service "
            "using the Microsoft Windows Administrative Tools.\n"
        );
        #endif
    } else if (! StartServiceCtrlDispatcher(service_table)) {
        if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            /* start as a regular WIN32 console application */
            #ifdef _ENABLE_PRIVILEGE_SEPARATION
            /* create the channel between privileged and unprivileged processes */
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == -1) goto _end;

            child.in = sockpair[0]; child.out = sockpair[1];

            _beginthread(_spawn_child, 0, NULL);

            /* hand the unprivileged end of the connection to the server */
            __server_set_privileged_channel(child.out);

            if (server_init() == 0) {
                fprintf(stderr, "-- Interactive session - Press Q to exit.\n");

                while (getchar() != 'q');

                fprintf(stderr, "-- End of the interactive session\n");
            }

            server_exit();

            /* tell the NT service we are shutting down */
            server_privileged_call(OP_EXIT, NULL, 0);

            /* close the unprivileged end of the connection */
            closesocket(child.out);
            #else
            if (server_init() == 0) {
                fprintf(stderr, "-- Interactive session - Press Q to exit.\n");

                while (getchar() != 'q');

                fprintf(stderr, "-- End of the interactive session\n");
            }

            server_exit();
            #endif
        }
    }

_end:
    WSACleanup();
}

/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_PRIVILEGE_SEPARATION
/* -------------------------------------------------------------------------- */

static VOID _spawn_child(UNUSED VOID *dummy)
{
    CHAR buffer[PATH_MAX];
    int bufsize = 0;
    STARTUPINFO startup_info;
    PROCESS_INFORMATION process_info;
    CHAR command[PATH_MAX + PATH_MAX];
    DEBUG_EVENT dbg;

    /* get the current binary path */
    bufsize = GetModuleFileName(NULL, buffer, sizeof(buffer));
    if (bufsize == sizeof(buffer)) {
        debug("_spawn_child(): path too long.\n");
        _endthread();
    } else if (bufsize == 0) {
        debug("main_process(): cannot get path to the executable.\n");
        _endthread();
    }

    /* build the command, passing the event and privileged socket */
    _snprintf(
        command,
        sizeof(command),
        "\"%s\" %i /p",
        buffer, child.in
    );

    memset(& startup_info, 0, sizeof(startup_info));
    memset(& process_info, 0, sizeof(process_info));
    startup_info.cb = sizeof(startup_info);

    if (! CreateProcess(
        buffer,                            /* module name */
        command,                           /* command line */
        NULL,                              /* process attr */
        NULL,                              /* thread attr */
        TRUE,                              /* inherit handles */
        CREATE_NO_WINDOW | DEBUG_PROCESS,  /* creation flags */
        NULL,                              /* environment */
        NULL,                              /* current directory */
        & startup_info,                    /* startup info */
        & process_info                     /* process info */
    )) {
        debug("main_process(): cannot create privileged process.\n");
        closesocket(child.in); closesocket(child.out);
        _endthread();
    }

    /* useless socket */
    closesocket(child.in);

    do {
        memset(& dbg, 0, sizeof(dbg));
        WaitForDebugEvent(& dbg, INFINITE);
        ContinueDebugEvent(dbg.dwProcessId, dbg.dwThreadId, DBG_CONTINUE);
    } while (1);

    _endthread();
}
/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

static BOOL _service_install(VOID)
{
    SC_HANDLE manager = NULL, service = NULL;
    CHAR buffer[PATH_MAX];
    int ret = 0;

    /* get the current binary path */
    ret = GetModuleFileName(NULL, buffer, sizeof(buffer));
    if (ret == sizeof(buffer)) {
        debug("_service_install(): path too long.\n");
        return FALSE;
    } else if (ret == 0) {
        debug("_service_install(): cannot get path to the executable.\n");
        return FALSE;
    }

    /* open the service control manager */
    manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (! manager) {
        debug("_service_install(): cannot open the SCManager.\n");
        return FALSE;
    }

    /* try to register the new service */
    service = CreateService(
        manager,
        WINMAIN_SERVICENAME,
        WINMAIN_SERVICENAME,
        GENERIC_READ | GENERIC_EXECUTE,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_IGNORE,
        buffer,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );

    if (! service) {
        const char *err = NULL;

        switch (GetLastError()) {
        case ERROR_ACCESS_DENIED: err = "access denied.\n"; break;
        case ERROR_DUPLICATE_SERVICE_NAME: err = "duplicate name.\n"; break;
        case ERROR_SERVICE_EXISTS: err = "already installed.\n"; break;
        default: err = "unknown error.\n";
        }

        fprintf(stderr, ERR(_service_install, CreateService)": %s", err);

        CloseServiceHandle(manager);

        return FALSE;
    }

    CloseServiceHandle(service); CloseServiceHandle(manager);

    return TRUE;
}

/* -------------------------------------------------------------------------- */

static BOOL _service_uninstall(VOID)
{
    SC_HANDLE manager = NULL, service = NULL;
    SERVICE_STATUS status;

    manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (! manager) {
        debug("_service_uninstall(): cannot connect to the SCManager.\n");
        return FALSE;
    }

    service = OpenService(manager,
                          WINMAIN_SERVICENAME, SERVICE_QUERY_STATUS | DELETE);

    if (! service) {
        debug("_service_uninstall(): cannot open the service.\n");
        CloseServiceHandle(manager);
        return FALSE;
    }

    if (! QueryServiceStatus(service, & status)) {
        debug("_service_uninstall(): cannot get service status.\n");
        CloseServiceHandle(service); CloseServiceHandle(manager);
        return FALSE;
    }

    if (status.dwCurrentState != SERVICE_STOPPED) {
        debug("_service_uninstall(): the service is still running.\n");
        CloseServiceHandle(service); CloseServiceHandle(manager);
        return FALSE;
    }

    if (! DeleteService(service)) {
        const char *err = NULL;

        switch (GetLastError()) {
        case ERROR_ACCESS_DENIED: err = "access denied.\n"; break;
        case ERROR_INVALID_HANDLE: err = "invalid handle.\n"; break;
        case ERROR_SERVICE_MARKED_FOR_DELETE: err = "already deleted.\n"; break;
        default: "unknown error.\n";
        }

        fprintf(stderr, ERR(_service_uninstall, DeleteService)": %s", err);

        CloseServiceHandle(service); CloseServiceHandle(manager);

        return FALSE;
    }

    CloseServiceHandle(service); CloseServiceHandle(manager);

    return TRUE;
}

/* -------------------------------------------------------------------------- */

static BOOL _service_start(VOID)
{
    SC_HANDLE manager = NULL, service = NULL;

    manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (! manager) {
        debug("_service_start(): cannot connect to the SCManager.\n");
        return FALSE;
    }

    service = OpenService(manager, WINMAIN_SERVICENAME, SERVICE_START);

    if (! service) {
        debug("_service_start(): cannot open the service.\n");
        CloseServiceHandle(manager);
        return FALSE;
    }

    if (! StartService(service, 0, 0)) {
        debug("_service_start(): cannot start the service.\n");
        CloseServiceHandle(service);
        CloseServiceHandle(manager);
        return FALSE;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(manager);

    return TRUE;
}

/* -------------------------------------------------------------------------- */

static BOOL _service_stop(VOID)
{
    SC_HANDLE manager = NULL, service = NULL;
    SERVICE_STATUS status;

    manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (! manager) {
        debug("_service_stop(): cannot connect to the SCManager.\n");
        return FALSE;
    }

    service = OpenService(manager, WINMAIN_SERVICENAME, SERVICE_STOP);

    if (! service) {
        debug("_service_stop(): cannot open the service.\n");
        CloseServiceHandle(manager);
        return FALSE;
    }

    if (! ControlService(service, SERVICE_CONTROL_STOP, & status)) {
        debug("_service_stop(): cannot stop the service.\n");
        CloseServiceHandle(service);
        CloseServiceHandle(manager);
        return FALSE;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(manager);

    return TRUE;
}

/* -------------------------------------------------------------------------- */

static VOID _service_status(DWORD s, DWORD r)
{
    SERVICE_STATUS state;

    if (s == SERVICE_START_PENDING)
        state.dwControlsAccepted = 0;
    else
        state.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    state.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    state.dwServiceSpecificExitCode = 0;
    state.dwCurrentState = s; state.dwWin32ExitCode = r;
    state.dwCheckPoint = 0; state.dwWaitHint = 0;

    /* report service status to the service controller */
    if (! SetServiceStatus(service_controller, & state)) _service_die();
}

/* -------------------------------------------------------------------------- */

static VOID _service_die(VOID)
{
    _service_status(SERVICE_STOP_PENDING, NO_ERROR);

    if (termination_event) CloseHandle(termination_event);

    _service_status(SERVICE_STOPPED, GetLastError());

    /* is that right ? */
    exit(EXIT_FAILURE);
}

/* -------------------------------------------------------------------------- */

static void WINAPI _service_ctl(DWORD cmd)
{
    DWORD state = SERVICE_RUNNING;

    if (cmd == SERVICE_CONTROL_STOP || cmd == SERVICE_CONTROL_SHUTDOWN)
         state = SERVICE_STOP_PENDING;

    _service_status(state, NO_ERROR);

    if (state == SERVICE_STOP_PENDING)
        SetEvent(termination_event);
}

/* -------------------------------------------------------------------------- */

static VOID WINAPI _service_main(DWORD argc, LPSTR *argv)
{
    SECURITY_ATTRIBUTES sec_attr;
    #ifdef _ENABLE_PRIVILEGE_SEPARATION
    HANDLE advapi32;
    fpCreateRestrictedToken _CreateRestrictedToken = NULL;
    HANDLE process_token, restricted_token;
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    SID_AND_ATTRIBUTES drop_sids[2];
    BOOL ret = FALSE;
    CHAR buffer[PATH_MAX];
    CHAR command[PATH_MAX + PATH_MAX];
    int bufsize = 0;
    STARTUPINFO startup_info;
    PROCESS_INFORMATION process_info;
    SOCKET sockpair[2];
    #endif

    service_controller = RegisterServiceCtrlHandler(argv[0], _service_ctl);
    if (! service_controller) return;

    _service_status(SERVICE_START_PENDING, NO_ERROR);

    sec_attr.nLength = sizeof(sec_attr);
    sec_attr.bInheritHandle = TRUE;
    sec_attr.lpSecurityDescriptor = NULL;

    /* create the termination event */
    termination_event = CreateEvent(& sec_attr, TRUE, FALSE, NULL);
    if (! termination_event) {
        debug("_service_main(): cannot create termination event.\n");
        _service_die();
    }

    #ifdef _ENABLE_PRIVILEGE_SEPARATION
    /* get CreateRestrictedToken() */
    if (! (advapi32 = LoadLibrary("ADVAPI32.DLL")) ) {
        debug("_service_main(): cannot open ADVAPI32.DLL\n");
        _service_die();
    }

    _CreateRestrictedToken = GetProcAddress(advapi32, "CreateRestrictedToken");
    if (! _CreateRestrictedToken) {
        debug("_service_main(): CreateRestrictedToken() was not found.\n");
        goto _error_0;
    }

    /* get the current binary path */
    bufsize = GetModuleFileName(NULL, buffer, sizeof(buffer));
    if (bufsize == sizeof(buffer)) {
        debug("_service_main(): path too long.\n");
        goto _error_0;
    } else if (bufsize == 0) {
        debug("_service_main(): cannot get path to the executable.\n");
        goto _error_0;
    }

    /* get the current process token to build a restricted one */
    if (! OpenProcessToken(
        GetCurrentProcess(),
        TOKEN_ALL_ACCESS,
        & process_token
    )) {
        debug("_service_main(): cannot get process token.\n");
        goto _error_0;
    }

    memset(& drop_sids, 0, sizeof(drop_sids));

    ret = AllocateAndInitializeSid(
        & nt,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        & drop_sids[0].Sid
    );

    if (! ret) {
        debug("_service_main(): cannot allocate SIDs.\n");
        goto _error_1;
    }

    ret = AllocateAndInitializeSid(
        & nt,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_POWER_USERS,
        0, 0, 0, 0, 0, 0,
        & drop_sids[1].Sid
    );

    if (! ret) {
        debug("_service_main(): cannot allocate SIDs.\n");
        goto _error_2;
    }

    /* drop privileges */
    ret = _CreateRestrictedToken(
        process_token,
        DISABLE_MAX_PRIVILEGE,
        sizeof(drop_sids) / sizeof(drop_sids[0]),
        drop_sids,
        0,
        NULL,
        0,
        NULL,
        & restricted_token
    );

    if (! ret) {
        debug("_service_main(): cannot create a restricted token.\n");
        goto _error_3;
    }

    FreeSid(& drop_sids[0].Sid);
    FreeSid(& drop_sids[1].Sid);
    FreeLibrary(advapi32);

    /* create the channel between privileged and unprivileged processes */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == -1) {
        CloseHandle(process_token); CloseHandle(restricted_token);
        _service_die();
    }

    /* build the command, passing the event and pipe descriptors on the CLI */
    _snprintf(
        command,
        sizeof(command),
        "\"%s\" %i %i /d",
        buffer, termination_event, sockpair[0]
    );

    memset(& startup_info, 0, sizeof(startup_info));
    memset(& process_info, 0, sizeof(process_info));
    startup_info.cb = sizeof(startup_info);

    if (! CreateProcessAsUser(
        restricted_token,  /* unprivileged token */
        buffer,            /* module name */
        command,           /* command line */
        NULL,              /* process attr */
        NULL,              /* thread attr */
        TRUE,              /* inherit handles */
        CREATE_NO_WINDOW,  /* creation flags */
        NULL,              /* environment */
        NULL,              /* current directory */
        & startup_info,    /* startup info */
        & process_info     /* process info */
    )) {
        debug("_service_main(): cannot create unprivileged process.\n");
        CloseHandle(process_token); CloseHandle(restricted_token);
        closesocket(sockpair[0]); closesocket(sockpair[1]);
        _service_die();
    }

    /* close unused handle */
    closesocket(sockpair[0]);
    #endif

    _service_status(SERVICE_RUNNING, NO_ERROR);

    #ifdef _ENABLE_PRIVILEGE_SEPARATION
    /* process privilege elevation requests from the unprivileged process */
    __server_privileged_process(sockpair[1]);

    /* close the privileged end of the connection */
    closesocket(sockpair[1]);
    #else
    server_init();

    WaitForSingleObject(termination_event, INFINITE);

    server_exit();
    #endif

    CloseHandle(termination_event);

    _service_status(SERVICE_STOPPED, NO_ERROR);

    return;

_error_3:
    FreeSid(& drop_sids[1].Sid);
_error_2:
    FreeSid(& drop_sids[0].Sid);
_error_1:
    CloseHandle(process_token);
_error_0:
    FreeLibrary(advapi32);

    _service_die();
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

/* This is a really simple example of builtin module. It simply listens on port
   8888 and echoes back what is sent to it. */

/* -------------------------------------------------------------------------- */
#ifdef _BUILTIN_MODULE
/* -------------------------------------------------------------------------- */

#define BUILTIN_PORT "8888"

/* module identifier */
static uint32_t module_token = 0;

/* -------------------------------------------------------------------------- */

export unsigned int module_api(void)
{
    unsigned int required_api_revision = 1390;
    return required_api_revision;
}

/* -------------------------------------------------------------------------- */

export int module_init(uint32_t id, UNUSED int argc, UNUSED char **argv)
{
    /* XXX
       It is possible to check if a module has already been loaded once by
       inspecting the value of module_token. Loading the same module twice
       without checking may lead to memory corruptions if some pointers are
       blindly clobbered by module_init(), since all instances of the same
       module share the same address space.
    */
    if (module_token) {
        fprintf(stderr, "BUILTIN: module already started.\n");
        return -1;
    }

    module_token = id;

    fprintf(stderr, "BUILTIN: starting builtin module...\n");
    fprintf(stderr, "BUILTIN: Copyright (c) 2025 ");
    fprintf(stderr, "Raphael Prevost, all rights reserved.\n");
    fprintf(stderr, "BUILTIN: version "ASKL_VERSION" ["__DATE__"]\n");

    fprintf(stderr, "BUILTIN: listening on port "BUILTIN_PORT".\n");

    if (server_open_managed_socket(
        id,
        NULL,
        BUILTIN_PORT,
        SOCKET_SERVER) == -1
    ) {
        fprintf(stderr, "BUILTIN: cannot listen to TCP port "BUILTIN_PORT".\n");
        return -1;
    }

    #ifdef _ENABLE_UDP
    if (server_open_managed_socket(
        id,
        NULL,
        BUILTIN_PORT,
        SOCKET_UDP | SOCKET_SERVER
    ) == -1) {
        fprintf(stderr, "BUILTIN: cannot listen to UDP port "BUILTIN_PORT".\n");
        return -1;
    }
    #endif

    return 0;
}

/* -------------------------------------------------------------------------- */

export void module_input_handler(
    uint16_t socket_id,
    UNUSED uint16_t ingress_id,
    m_string *data
)
{
    /* send back the incoming data and close the connection */
    server_send_buffer(
        module_token,
        socket_id,
        SERVER_MSG_END,
        DATA(data),
        SIZE(data)
    );
}

/* -------------------------------------------------------------------------- */

export void module_event_handler(
    UNUSED uint16_t socket_id,
    UNUSED uint16_t ingress_id,
    int event
)
{
    switch (event) {

    case MODULE_EVENT_INCOMING_CONNECTION:
        /* new connection */ break;

    case MODULE_EVENT_OUTGOING_CONNECTION:
        /* connected to remote host */ break;

    case MODULE_EVENT_SOCKET_DISCONNECTED:
        /* connection closed */ break;

    case MODULE_EVENT_REQUEST_TRANSMITTED:
        /* request sent */ break;

    case MODULE_EVENT_SERVER_SHUTTINGDOWN:
        /* server shutting down */ break;

    default: fprintf(stderr, "BUILTIN: spurious event.\n");

    }
}

/* -------------------------------------------------------------------------- */

export void module_exit(void)
{
    fprintf(stderr, "BUILTIN: successfully unloaded.\n");
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
