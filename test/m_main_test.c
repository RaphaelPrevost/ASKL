#include "../lib/askl.h"
#include "../lib/askl_server.h"
#include <signal.h>

extern int test_socket(void);
extern int test_string(void);
extern int test_queue(void);
#ifdef _ENABLE_HASHTABLE
extern int test_hashtable(void);
#endif
#ifdef _ENABLE_TRIE
extern int test_trie(void);
#ifdef _ENABLE_FILE
extern int test_fs(void);
#endif
#endif
#ifdef _ENABLE_HTTP
extern int test_http(void);
#endif
#ifdef _ENABLE_DB
extern int test_db(void);
#endif

char *working_directory = NULL;

/* -------------------------------------------------------------------------- */

int main(void)
{
    signal(SIGPIPE, SIG_IGN);

    monotonic_timer_init();

    if (test_string() == -1) {
        printf("!!! m_string test: FAILURE !!!\n");
        exit(EXIT_FAILURE);
    } else printf("=== m_string test: SUCCESS ===\n");

    #if defined(_ENABLE_HTTP) && defined(_ENABLE_FILE)
    if (test_http() == -1) {
        printf("!!! m_http test: FAILURE !!!\n");
        exit(EXIT_FAILURE);
    } else printf("=== m_http test: SUCCESS ===\n");
    #endif

    #ifdef _ENABLE_HASHTABLE
    if (test_hashtable() == -1) {
        printf("!!! m_hashtable test: FAILURE !!!\n");
        exit(EXIT_FAILURE);
    } else printf("=== m_hashtable test: SUCCESS ===\n");
    #endif

    /*if (test_socket() == -1) {
        printf("!!! m_socket test: FAILURE !!!\n");
        exit(EXIT_FAILURE);
    } else printf("=== m_socket test: SUCCESS ===\n");*/

    if (test_queue() == -1) {
        printf("!!! m_queue test: FAILURE !!!\n");
        exit(EXIT_FAILURE);
    } else printf("=== m_queue test: SUCCESS ===\n");

    #ifdef _ENABLE_TRIE
    if (test_trie() == -1) {
        printf("!!! m_trie test: FAILURE !!!\n");
        exit(EXIT_FAILURE);
    } else printf("=== m_trie test: SUCCESS ===\n");

    #ifdef _ENABLE_FILE
    if (test_fs() == -1) {
        printf("!!! m_file test: FAILURE !!!\n");
        exit(EXIT_FAILURE);
    } else printf("=== m_file test: SUCCESS ===\n");
    #endif
    #endif

    #ifdef _ENABLE_DB
    if (test_db() == -1) {
        printf("!!! m_db test: FAILURE !!!\n");
        exit(EXIT_FAILURE);
    } else printf("=== m_db test: SUCCESS ===\n");
    #endif

    exit(EXIT_SUCCESS);
}

/* -------------------------------------------------------------------------- */
