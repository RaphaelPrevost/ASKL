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

#include "askl_test.h"
#include <signal.h>
#include <sys/types.h>
#ifndef _WIN32
    #include <sys/wait.h>
    #define HAS_FORK 1
#else
    #define HAS_FORK 0
#endif

/* default timeouts, in seconds */
#define TIMEOUT_FAST 30
#define TIMEOUT_SLOW 300

/* lines of captured output shown for a failed test */
#define LOG_HEAD 20
#define LOG_TAIL 200

/* -------------------------------------------------------------------------- */
/* suites                                                                     */
/* -------------------------------------------------------------------------- */

extern const Test_Suite test_suite_string;
extern const Test_Suite test_suite_codecs;
extern const Test_Suite test_suite_format;
extern const Test_Suite test_suite_variant;
#ifdef _ENABLE_JSON
extern const Test_Suite test_suite_json;
extern const Test_Suite test_suite_conformance;
#if defined(_ENABLE_JSON) && defined(_ENABLE_TRIE)
extern const Test_Suite test_suite_jsonpath;
extern const Test_Suite test_suite_compliance;
#endif
#endif
#ifdef _ENABLE_TRIE
extern const Test_Suite test_suite_cbtrie;
#endif
#ifdef _ENABLE_HASHMAP
extern const Test_Suite test_suite_htable;
#endif
extern const Test_Suite test_suite_socket;
extern const Test_Suite test_suite_queue;
extern const Test_Suite test_suite_rwlock;
#ifdef _ENABLE_RANDOM
extern const Test_Suite test_suite_random;
#endif
#if defined(_ENABLE_SERVER) && defined(_BUILTIN_MODULE)
extern const Test_Suite test_suite_server;
#endif
extern const Test_Suite test_suite_legacy;

static const Test_Suite *_suites[] = {
    & test_suite_string,
    & test_suite_codecs,
    & test_suite_format,
    & test_suite_variant,
    #ifdef _ENABLE_JSON
    & test_suite_json,
    & test_suite_conformance,
    #if defined(_ENABLE_JSON) && defined(_ENABLE_TRIE)
    & test_suite_jsonpath,
    & test_suite_compliance,
    #endif
    #endif
    #ifdef _ENABLE_TRIE
    & test_suite_cbtrie,
    #endif
    #ifdef _ENABLE_HASHMAP
    & test_suite_htable,
    #endif
    & test_suite_socket,
    & test_suite_queue,
    & test_suite_rwlock,
    #ifdef _ENABLE_RANDOM
    & test_suite_random,
    #endif
    #if defined(_ENABLE_SERVER) && defined(_BUILTIN_MODULE)
    & test_suite_server,
    #endif
    & test_suite_legacy
};

#define SUITES (sizeof(_suites) / sizeof(*_suites))

/* -------------------------------------------------------------------------- */
/* options and run state                                                      */
/* -------------------------------------------------------------------------- */

typedef struct _Options {
    unsigned int seed;
    unsigned int timeout;   /* 0 means the tier default */
    int slow;
    int verbose;
    int list;
    int fork;
    int filters;
    char **filter;
} _Options;

static _Options _opt = { 0, 0, 0, 0, 0, 1, 0, NULL };

/* failures recorded by CHECK() in the current test case */
static int _failures = 0;

/* -------------------------------------------------------------------------- */
/* services for the test cases                                                */
/* -------------------------------------------------------------------------- */

unsigned int test_seed(void)
{
    return _opt.seed;
}

/* -------------------------------------------------------------------------- */

int test_verbose(void)
{
    return _opt.verbose;
}

/* -------------------------------------------------------------------------- */

void test_fail(const char *file, int line, const char *fmt, ...)
{
    va_list args;

    _failures ++;

    fprintf(stderr, "%s:%d: ", file, line);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

/* -------------------------------------------------------------------------- */

int test_status(void)
{
    return _failures ? -1 : 0;
}

/* -------------------------------------------------------------------------- */
/* helpers                                                                    */
/* -------------------------------------------------------------------------- */

static unsigned int _getenv_uint(const char *name, unsigned int fallback)
{
    const char *value = getenv(name);
    char *end = NULL;
    unsigned long parsed = 0;

    if (! value || ! *value) return fallback;

    parsed = strtoul(value, & end, 10);

    if (*end) {
        fprintf(
            stderr, "runner: ignoring %s=%s (not a number).\n", name, value
        );
        return fallback;
    }

    return (unsigned int) parsed;
}

/* -------------------------------------------------------------------------- */

static int _selected(const Test_Suite *suite, const Test_Case *c)
{
    /** @brief match a test against the command line filters */

    int i = 0;
    size_t len = strlen(suite->name);
    const char *f = NULL;

    if (! _opt.filters) return 1;

    for (i = 0; i < _opt.filters; i ++) {
        f = _opt.filter[i];
        /* "suite" selects the whole suite, "suite.case" one case */
        if (strncmp(f, suite->name, len)) continue;
        if (! f[len]) return 1;
        if (f[len] == '.' && ! strcmp(f + len + 1, c->name)) return 1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static double _elapsed(const struct timespec *from)
{
    struct timespec now;

    monotonic_timer(& now);

    return (now.tv_sec - from->tv_sec) +
           (now.tv_nsec - from->tv_nsec) / 1000000000.0;
}

/* -------------------------------------------------------------------------- */

static int _run_case(const Test_Suite *suite, const Test_Case *c)
{
    /** @brief run setup, the case and teardown in the current process */

    int ret = 0;

    _failures = 0;
    srand(_opt.seed);

    if (suite->setup && suite->setup() == -1) {
        fprintf(stderr, "%s: suite setup failed.\n", suite->name);
        return -1;
    }

    ret = c->run();

    if (suite->teardown) suite->teardown();

    if (ret == 0 && _failures) ret = -1;

    return ret;
}

/* -------------------------------------------------------------------------- */

static void _print_log(FILE *log, const char *skip, size_t skiplen)
{
    /** @brief print the captured output of a test as TAP diagnostics */

    char line[4096];
    unsigned long lines = 0, i = 0, elided = 0;

    /* count the lines first, to elide the middle of a long log */
    rewind(log);
    while (fgets(line, sizeof(line), log)) lines ++;
    rewind(log);

    while (fgets(line, sizeof(line), log)) {
        i ++;
        if (lines > LOG_HEAD + LOG_TAIL &&
            i > LOG_HEAD && i <= lines - LOG_TAIL) {
            if (! elided ++) {
                printf(
                    "#   ... %lu lines elided ...\n",
                    lines - LOG_HEAD - LOG_TAIL
                );
            }
            continue;
        }
        /* the skip reason is reported on the TAP line, not repeated here */
        if (skip && ! strncmp(line, skip, skiplen)) continue;
        printf("#   %s", line);
        if (line[strlen(line) - 1] != '\n') printf("\n");
    }
}

/* -------------------------------------------------------------------------- */

static int _skip_reason(FILE *log, char *reason, size_t len)
{
    /** @brief look for a "SKIP: reason" line in the captured output */

    char line[256];
    size_t l = 0;

    rewind(log);

    while (fgets(line, sizeof(line), log)) {
        if (strncmp(line, "SKIP: ", 6)) continue;
        l = strlen(line + 6);
        if (l && line[6 + l - 1] == '\n') l --;
        if (l >= len) l = len - 1;
        memcpy(reason, line + 6, l); reason[l] = '\0';
        return 0;
    }

    reason[0] = '\0';

    return -1;
}

/* -------------------------------------------------------------------------- */
#if HAS_FORK
/* -------------------------------------------------------------------------- */

static int _run_isolated(
    const Test_Suite *suite,
    const Test_Case *c,
    unsigned int timeout,
    FILE *log,
    char *reason,
    size_t len
)
{
    /** @brief run a test case in a child process, with a timeout */

    pid_t pid = 0, r = 0;
    int status = 0, ret = 0;
    struct timespec start, nap = { 0, 2000000 }; /* 2 ms */

    fflush(NULL);

    if ( (pid = fork()) == -1) {
        perror(ERR(_run_isolated, fork));
        snprintf(reason, len, "fork failed");
        return -1;
    }

    if (! pid) {
        /* child: capture the output, run the case and report through exit() */
        if (dup2(fileno(log), STDOUT_FILENO) == -1 ||
            dup2(fileno(log), STDERR_FILENO) == -1) {
            perror(ERR(_run_isolated, dup2));
            exit(EXIT_FAILURE);
        }
        setvbuf(stdout, NULL, _IOLBF, 0);

        ret = _run_case(suite, c);

        fflush(NULL);

        /* exit() rather than _exit(): let the coverage and sanitizer
           runtimes write their reports */
        exit( (ret == 0) ? EXIT_SUCCESS :
              (ret == TEST_SKIPPED) ? TEST_SKIPPED : EXIT_FAILURE );
    }

    monotonic_timer(& start);

    while ( (r = waitpid(pid, & status, WNOHANG)) == 0) {
        if (_elapsed(& start) >= timeout) {
            kill(pid, SIGKILL);
            waitpid(pid, & status, 0);
            snprintf(reason, len, "timeout after %u s", timeout);
            return -1;
        }
        nanosleep(& nap, NULL);
    }

    if (r == -1) {
        perror(ERR(_run_isolated, waitpid));
        snprintf(reason, len, "waitpid failed");
        return -1;
    }

    if (WIFSIGNALED(status)) {
        snprintf(reason, len, "killed by signal %d", WTERMSIG(status));
        return -1;
    }

    if (! WIFEXITED(status)) {
        snprintf(reason, len, "unexpected wait status");
        return -1;
    }

    switch (WEXITSTATUS(status)) {
    case EXIT_SUCCESS: return 0;
    case TEST_SKIPPED: return TEST_SKIPPED;
    default:
        snprintf(reason, len, "exit status %d", WEXITSTATUS(status));
        return -1;
    }
}

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */

static int _run(void)
{
    /** @brief run every selected test and print the results as TAP */

    unsigned int i = 0, j = 0, n = 0, total = 0;
    unsigned int passed = 0, failed = 0, skipped = 0, todo = 0, timeout = 0;
    int ret = 0;
    const Test_Suite *suite = NULL;
    const Test_Case *c = NULL;
    FILE *log = NULL;
    char reason[128], skip[128];
    struct timespec start;

    /* the plan */
    for (i = 0; i < SUITES; i ++)
        for (j = 0; j < _suites[i]->count; j ++)
            if (_selected(_suites[i], & _suites[i]->cases[j])) total ++;

    if (_opt.filters && ! total) {
        fprintf(stderr, "no test matches the selection\n");
        return -1;
    }

    printf("TAP version 13\n");
    printf("# seed: %u\n", _opt.seed);
    printf("# tier: %s\n", _opt.slow ? "fast and slow" : "fast only");
    if (! _opt.fork) printf("# isolation: off\n");
    printf("1..%u\n", total);

    monotonic_timer(& start);

    for (i = 0; i < SUITES; i ++) {
        suite = _suites[i];
        for (j = 0; j < suite->count; j ++) {
            c = & suite->cases[j];

            if (! _selected(suite, c)) continue;

            n ++;

            if (c->flags & TEST_FLAG_SLOW && ! _opt.slow) {
                printf("ok %u - %s.%s # SKIP slow\n", n, suite->name, c->name);
                skipped ++;
                continue;
            }

            timeout = _opt.timeout ? _opt.timeout :
                      (c->flags & TEST_FLAG_SLOW) ? TIMEOUT_SLOW : TIMEOUT_FAST;
            reason[0] = skip[0] = '\0';

            #if HAS_FORK
            if (_opt.fork) {
                if (! (log = tmpfile()) ) {
                    perror(ERR(_run, tmpfile));
                    return -1;
                }
                ret = _run_isolated(
                    suite, c, timeout, log, reason, sizeof(reason)
                );
                if (ret == TEST_SKIPPED)
                    _skip_reason(log, skip, sizeof(skip));
            } else
            #endif
            {
                printf("# %s.%s\n", suite->name, c->name);
                ret = _run_case(suite, c);
                if (ret == TEST_SKIPPED)
                    snprintf(skip, sizeof(skip), "see log");
                else if (ret)
                    snprintf(reason, sizeof(reason), "returned %d", ret);
            }

            if (ret == 0) {
                printf("ok %u - %s.%s", n, suite->name, c->name);
                if (c->todo) printf(" # TODO passes now: %s", c->todo);
                printf("\n");
                passed ++;
            } else if (ret == TEST_SKIPPED) {
                printf(
                    "ok %u - %s.%s # SKIP %s\n", n, suite->name, c->name, skip
                );
                skipped ++;
            } else if (c->todo) {
                /* a known failure does not fail the run */
                printf(
                    "not ok %u - %s.%s # TODO %s\n",
                    n, suite->name, c->name, c->todo
                );
                printf("#   %s.%s: %s\n", suite->name, c->name, reason);
                todo ++;
            } else {
                printf("not ok %u - %s.%s\n", n, suite->name, c->name);
                printf("#   %s.%s: %s\n", suite->name, c->name, reason);
                failed ++;
            }

            if (log) {
                if ((ret == -1 && ! c->todo) || _opt.verbose)
                    _print_log(log, (ret == TEST_SKIPPED) ? "SKIP: " : NULL, 6);
                fclose(log); log = NULL;
            }

            fflush(stdout);
        }
    }

    printf(
        "# %u passed, %u failed, %u known failures, %u skipped (%.2f s)\n",
        passed, failed, todo, skipped, _elapsed(& start)
    );

    return failed ? -1 : 0;
}

/* -------------------------------------------------------------------------- */

static void _list(void)
{
    unsigned int i = 0, j = 0;

    for (i = 0; i < SUITES; i ++) {
        for (j = 0; j < _suites[i]->count; j ++) {
            if (! _selected(_suites[i], & _suites[i]->cases[j])) continue;
            printf(
                "%s.%s%s\n", _suites[i]->name, _suites[i]->cases[j].name,
                (_suites[i]->cases[j].flags & TEST_FLAG_SLOW) ? " [slow]" : ""
            );
        }
    }
}

/* -------------------------------------------------------------------------- */

static void _usage(const char *argv0)
{
    fprintf(
        stderr,
        "usage: %s [options] [suite | suite.case ...]\n"
        "  -l          list the selected tests and exit\n"
        "  -v          always show the output of the tests\n"
        "  -s SEED     seed for the random generator (ASKL_TEST_SEED)\n"
        "  -t SECONDS  timeout per test (ASKL_TEST_TIMEOUT)\n"
        "  --slow      also run the slow tier (ASKL_TEST_SLOW=1)\n"
        "  --no-fork   run the tests in this process, without timeout\n"
        "              (for debuggers; ASKL_TEST_NOFORK=1)\n"
        "Every test runs in its own process; the output is TAP.\n",
        argv0
    );
}

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    int i = 0;
    char **filter = NULL;

    #ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    #endif
    monotonic_timer_init();

    _opt.seed = _getenv_uint("ASKL_TEST_SEED", (unsigned int) time(NULL));
    _opt.timeout = _getenv_uint("ASKL_TEST_TIMEOUT", 0);
    _opt.slow = _getenv_uint("ASKL_TEST_SLOW", 0) != 0;
    _opt.verbose = _getenv_uint("ASKL_TEST_VERBOSE", 0) != 0;
    _opt.fork = _getenv_uint("ASKL_TEST_NOFORK", 0) == 0;

    if (! (filter = malloc(argc * sizeof(*filter))) ) {
        perror(ERR(main, malloc));
        return 2;
    }

    for (i = 1; i < argc; i ++) {
        if (! strcmp(argv[i], "-l")) _opt.list = 1;
        else if (! strcmp(argv[i], "-v")) _opt.verbose = 1;
        else if (! strcmp(argv[i], "--slow")) _opt.slow = 1;
        else if (! strcmp(argv[i], "--no-fork")) _opt.fork = 0;
        else if (! strcmp(argv[i], "-s") && i + 1 < argc)
            _opt.seed = (unsigned int) strtoul(argv[++ i], NULL, 10);
        else if (! strcmp(argv[i], "-t") && i + 1 < argc)
            _opt.timeout = (unsigned int) strtoul(argv[++ i], NULL, 10);
        else if (! strcmp(argv[i], "-h") || ! strcmp(argv[i], "--help")) {
            _usage(argv[0]); free(filter);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "%s: unknown option %s\n", argv[0], argv[i]);
            _usage(argv[0]); free(filter);
            return 2;
        } else filter[_opt.filters ++] = argv[i];
    }

    _opt.filter = filter;

    #if ! HAS_FORK
    _opt.fork = 0;
    #endif

    if (_opt.list) {
        _list(); free(filter);
        return 0;
    }

    i = _run();

    free(filter);

    return (i == -1) ? 1 : 0;
}

/* -------------------------------------------------------------------------- */
