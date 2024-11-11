/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_TRIE
/* -------------------------------------------------------------------------- */

#include "../lib/m_trie.h"
#include <signal.h>

#define _CACHE_ITEMS   800000
#define _CACHE_KEYFM   "%" PRIuPTR
#define _CACHE_RNDDL    100000

static int timeout = 0;

/* -------------------------------------------------------------------------- */

static int callback(const char *element, size_t len, variant arg)
{
    printf("%.*s [size=%zu] = %" PRIuPTR "\n",
           (int) len, element, len, (uintptr_t) value_to_integer(arg));
    return 1;
}

/* -------------------------------------------------------------------------- */

static void _timeout(int dummy)
{
    dummy = 1;
    if (! timeout) printf("(!) Random deletion timed out.\n");
    timeout = dummy;
}

/* -------------------------------------------------------------------------- */

int test_trie(void)
{
    m_trie *t = NULL;
    variant val = { 0 };
    uintptr_t i = 0, j = 0;
    int missing = 0;
    char key[BUFSIZ];
    clock_t start, stop;
    size_t len = 0;

    signal(SIGALRM, _timeout);

    printf("(-) Testing trie implementation.\n");
    if (! (t = trie_alloc(NULL)) ) {
        printf("(!) Allocating trie: FAILURE\n");
        return -1;
    } else printf("(*) Allocating trie: SUCCESS\n");

    printf("(*) Inserting key-value pairs.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i); key[len] = 0;
        trie_insert(t, key, len, value_from_integer(i));
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

    if (trie_insert(t, key, len, value_from_integer(i)) != -1) {
        printf("(!) Inserting duplicate key: FAILURE\n");
        return -1;
    } else printf("(*) Inserting duplicate key: SUCCESS\n");

    printf("(*) Getting back values from keys.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i); key[len] = 0;
        val = trie_lookup(t, key, len, NULL);
        if (! is_integer(val) || (j = value_to_integer(val)) != i) {
            missing ++;
            if (is_integer(val)) {
                printf(
                    "(!) Key %" PRIuPTR " is missing ! "
                    "(found %" PRIuPTR " instead)\n",
                    i, j
                );
            }
        }
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    trie_foreach_prefix(t, "800", strlen("800"), callback);

    missing = 0;

    alarm(2);
    printf("(*) Randomly deleting 100k keys.\n");
    while (! timeout && missing < _CACHE_RNDDL) {
        i = rand() % _CACHE_ITEMS;
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        if (is_integer(trie_remove(t, key, len))) missing ++;
    }
    timeout = 1;

    missing = 0;

    printf("(*) Replacing all keys values.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        trie_update(t, key, len, value_from_integer(i + 1));
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    missing = 0;

    /* remove all the keys */
    printf("(*) Removing all the data from the table.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        val = trie_remove(t, key, len);
        if (is_integer(val)) {
            if (value_to_integer(val) != i + 1)
                missing ++;
        } else missing ++;
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    missing = 0;

    printf("(*) Checking that all the keys have been deleted.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        val = trie_lookup(t, key, len, NULL);
        if (! is_integer(val) || value_to_integer(val) != i + 1)
            missing ++;
        else printf("(!) found phantom key %" PRIuPTR " !\n", i);
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    printf("(*) Overwriting a key.\n");
    if (trie_insert(t, key, len, value_from_integer(0x888)) == -1)
        printf("(!) Key insertion failed\n");
    val = trie_update(t, key, len, value_from_integer(0x8989));
    if (value_to_integer(val) != 0x888)
        printf("(!) Key overwrite returned 0x%" PRIxPTR  "\n", i);
    if (value_to_integer(trie_remove(t, key, len)) != 0x8989)
        printf("(!) Retrieved key is 0x%" PRIxPTR  ", expected 0x8989.\n", i);
    else
        printf("(*) Value was successfully overwritten.\n");

    trie_free(t);

    return 0;
}

/* -------------------------------------------------------------------------- */
#else
/* -------------------------------------------------------------------------- */

/* This unit test will not be compiled */
#ifdef __GNUC__
__attribute__ ((unused)) static int __dummy__ = 0;
#endif

/* -------------------------------------------------------------------------- */
#endif
/* -------------------------------------------------------------------------- */
