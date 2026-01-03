#include "../lib/askl_server.h"
#include "hashlib.h"
#include <signal.h>

#define _CACHE_CONCURRENCY 2

#define _CACHE_ITEMS 1000000
#define _CACHE_RNDDL 100000
#define _CACHE_THRNG 400000

#define _CACHE_KEYFM "%" PRIuPTR

/* thread start control switch */
static pthread_mutex_t mx_switch = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cd_switch = PTHREAD_COND_INITIALIZER;
static int start_switch = 0;

static int timeout = 0;

/* -------------------------------------------------------------------------- */

typedef struct _item {
    size_t len;
    char *key;
    char *val;
} _item;

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* wyhash32 (author: 王一 Wang Yi <godspeed_china@yeah.net>) */
/* -------------------------------------------------------------------------- */

static uint32_t _wyr32(const uint8_t *p)
{
    uint32_t result;
    memcpy(& result, p, sizeof(result));
    #if defined(BIG_ENDIAN_HOST)
    return __bswap32(result);
    #else
    return result;
    #endif
}

/* -------------------------------------------------------------------------- */

static uint32_t _wyr24(const uint8_t *p, uint32_t k)
{
    return (((uint32_t) p[0]) << 16) | (((uint32_t) p[k >> 1]) << 8) | p[k - 1];
}

/* -------------------------------------------------------------------------- */

static void _wymix32(uint32_t *a, uint32_t *b)
{
    uint64_t c = *a ^ 0x53c5ca59u;
    c *= *b ^ 0x74743c1bu;
    *a = (uint32_t) c;
    *b = (uint32_t) (c >> 32);
}

/* -------------------------------------------------------------------------- */

static uint32_t __hash(_item *item, uint32_t seed)
{
    const uint8_t *p = (const uint8_t *) item->key;
    uint32_t i, see1 = item->len;

    _wymix32(& seed, & see1);

    for (i = item->len; i > 8; i -= 8, p += 8) {
        seed ^= _wyr32(p);
        see1 ^= _wyr32(p + 4);
        _wymix32(& seed, & see1);
    }

    if (i >= 4) {
        seed ^= _wyr32(p);
        see1 ^= _wyr32(p + i - 4);
    } else if (i) seed ^= _wyr24(p, i);

    _wymix32(& seed, & see1);
    _wymix32(& seed, & see1);

    return seed ^ see1;
}

/* -------------------------------------------------------------------------- */

static unsigned long _hash(const void *i)
{
    return __hash((_item *) i, 0x54662478);
}

/* -------------------------------------------------------------------------- */

static unsigned long _rehash(const void *i)
{
    return __hash((_item *) i, 0x97566321);
}

/* -------------------------------------------------------------------------- */

static int _hashcmp(const void *ia, const void *ib)
{
    unsigned int i = 0;
    const char *a = NULL, *b = NULL;
    size_t len = 0;

    if (((_item *) ia)->len != ((_item *) ib)->len) return -1;

    a = ((_item *) ia)->key; b = ((_item *) ib)->key;
    len = ((_item *) ia)->len;

    /*if (len <= sizeof(uint32_t))*/ return memcmp(a, b, len);

    for (i = 0; i < len - sizeof(uint32_t); i += sizeof(uint32_t)) {
        if (*((uint32_t *) (a + i)) != *((uint32_t *) (b + i)))
            return -1;

        if (i >= len - (i + sizeof(uint32_t)))
            return 0;

        if (*((uint32_t *) (a + len - (i + sizeof(uint32_t)))) !=
            *((uint32_t *) (b + len - (i + sizeof(uint32_t)))))
            return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */

static void *_hashdup(const void *p)
{
    _item *i = (_item *) p, *new = malloc(sizeof(*new));
    if (! new) return NULL;
    if (! (new->key = malloc(i->len * sizeof(*new->key))) ) {
        free(new); return NULL;
    }
    new->len = i->len; new->val = i->val;
    memcpy(new->key, i->key, i->len);
    return new;
}

/* -------------------------------------------------------------------------- */

static void _hashfree(void *i)
{
    free(((_item *) i)->key); free(i);
}

/* -------------------------------------------------------------------------- */

static void _timeout(int dummy)
{
    dummy = 1;
    if (! timeout) printf("(!) Random deletion timed out.\n");
    timeout = dummy;
}

/* -------------------------------------------------------------------------- */

static int _print_and_delete_key(const char *key, size_t len, UNUSED variant val)
{
    printf("%.*s\n", (int) len, key);
    return -1;
}

/* -------------------------------------------------------------------------- */

static int _print_key_intval(const char *key, size_t len, variant val)
{
    printf("%.*s = %llu\n", (int) len, key, variant_to_integer(val));
    return 0;
}

/* -------------------------------------------------------------------------- */

static variant _merge(
    UNUSED const char *key,
    UNUSED size_t len,
    UNUSED variant dest,
    variant src
)
{
    /* overwrite */
    return src;
}

/* -------------------------------------------------------------------------- */
/* Iterator Concurrent Test (No External Writers)                            */
/* -------------------------------------------------------------------------- */

#define _ITER_THREADS 4
#define _ITER_ITEMS 10000

static pthread_t _iter_thread[_ITER_THREADS];
static ASKL_LinkedMap *_iter_map = NULL;
static volatile int _iter_errors = 0;

/* Thread that reads via iterator (no modifications) */
static void *_iterator_reader(void *arg)
{
    uintptr_t tid = (uintptr_t) arg;
    ASKL_MapIterator *it;
    int iterations = 0;

    pthread_mutex_lock(& mx_switch);
    while (! start_switch) pthread_cond_wait(& cd_switch, & mx_switch);
    pthread_mutex_unlock(& mx_switch);

    /* Iterate multiple times */
    for (int pass = 0; pass < 3; pass++) {
        int count = 0;
        for (it = map_each(_iter_map); it; it = map_next(it)) {
            if (! is_integer(it->val)) {
                printf("(!) Reader thread %zu: Found non-integer value!\n", tid);
                __sync_fetch_and_add(& _iter_errors, 1);
                map_break(it);
                pthread_exit(NULL);
            }
            count ++;

            if (count % 500 == 0) {
                usleep(10);
            }
        }
        iterations += count;
    }

    printf("(-) Reader thread %zu: Completed %d total iterations\n", tid, iterations);
    pthread_exit(NULL);
}

/* Thread that iterates and updates values using map_set_at */
static void *_iterator_updater(void *arg)
{
    uintptr_t tid = (uintptr_t) arg;
    ASKL_MapIterator *it;
    int updates = 0;
    int iterations = 0;

    pthread_mutex_lock(& mx_switch);
    while (! start_switch) pthread_cond_wait(& cd_switch, & mx_switch);
    pthread_mutex_unlock(& mx_switch);

    printf("(-) Updater thread %zu: Starting...\n", tid);

    /* Perform several passes, updating some values each time */
    for (int pass = 0; pass < 2; pass ++) {
        int count = 0;

        printf("(-) Updater thread %zu: Starting pass %d\n", tid, pass);

        for (it = map_each(_iter_map); it; it = map_next(it)) {
            count ++;
            iterations ++;

            if (!is_integer(it->val)) {
                printf("(!) Updater thread %zu: Iterator found non-integer value!\n", tid);
                __sync_fetch_and_add(&_iter_errors, 1);
                map_break(it);
                pthread_exit(NULL);
            }

            /* Update every 10th entry */
            if (count % 10 == (int) tid) {
                unsigned int old_val = variant_to_integer(it->val);
                unsigned int new_val = old_val + 2000000;

                variant old = map_set_at(it, variant_from_integer(new_val));

                if (! is_integer(old)) {
                    printf("(!) Updater thread %zu: map_set_at returned non-integer!\n", tid);
                    __sync_fetch_and_add(&_iter_errors, 1);
                    map_break(it);
                    pthread_exit(NULL);
                }

                if (variant_to_integer(old) != old_val) {
                    printf("(!) Updater thread %zu: Expected old=%u, got=%llu\n",
                           tid, old_val, variant_to_integer(old));
                    __sync_fetch_and_add(&_iter_errors, 1);
                }

                /* Verify the iterator cache was updated */
                if (! is_integer(it->val) || variant_to_integer(it->val) != new_val) {
                    printf("(!) Updater thread %zu: Iterator cache not updated! Expected %u, got %llu\n",
                           tid, new_val, variant_to_integer(it->val));
                    __sync_fetch_and_add(&_iter_errors, 1);
                }

                updates ++;

                if (updates % 500 == 0) {
                    printf("(-) Updater thread %zu: %d updates so far...\n", tid, updates);
                }
            }

            if (count % 100 == 0) {
                usleep(1);
            }
        }

        printf("(-) Updater thread %zu: Completed pass %d (%d items)\n", tid, pass, count);

        if (count != _ITER_ITEMS) {
            printf("(!) Updater thread %zu: Expected %d items, got %d\n",
                   tid, _ITER_ITEMS, count);
            __sync_fetch_and_add(&_iter_errors, 1);
        }
    }

    printf("(-) Updater thread %zu: Updated %d items across %d iterations\n",
           tid, updates, iterations);
    pthread_exit(NULL);
}

static int test_iterator_concurrency(void)
{
    uintptr_t i;
    size_t len;
    char key[BUFSIZ];
    clock_t start, stop;

    printf("(-) Testing iterator with concurrent reads and updates.\n");

    if (!(_iter_map = map_alloc(NULL))) {
        printf("(!) Allocating map for iterator test: FAILURE\n");
        return -1;
    }

    printf("(*) Populating map with %d items.\n", _ITER_ITEMS);
    for (i = 1; i <= _ITER_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        map_set(_iter_map, key, len, variant_from_integer(i));
    }

    _iter_errors = 0;
    start_switch = 0;

    printf("(*) Spawning %d concurrent threads.\n", _ITER_THREADS);
    printf("    - 2 iterator readers (read-only)\n");
    printf("    - 2 iterator updaters (map_set_at)\n");

    if (pthread_create(&_iter_thread[0], NULL, _iterator_reader, (void *) 0) == -1) {
        perror("pthread_create reader 1");
        return -1;
    }

    if (pthread_create(&_iter_thread[1], NULL, _iterator_reader, (void *) 1) == -1) {
        perror("pthread_create reader 2");
        return -1;
    }

    if (pthread_create(&_iter_thread[2], NULL, _iterator_updater, (void *) 2) == -1) {
        perror("pthread_create updater 1");
        return -1;
    }

    if (pthread_create(&_iter_thread[3], NULL, _iterator_updater, (void *) 3) == -1) {
        perror("pthread_create updater 2");
        return -1;
    }

    pthread_mutex_lock(& mx_switch);
    start_switch = 1;
    pthread_cond_broadcast(& cd_switch);
    pthread_mutex_unlock(& mx_switch);

    start = clock();

    for (i = 0; i < _ITER_THREADS; i ++) {
        pthread_join(_iter_thread[i], NULL);
    }

    stop = clock();

    printf("(-) Time elapsed = %.3f s\n", (double)(stop - start) / CLOCKS_PER_SEC);

    printf("(*) Verifying final map integrity.\n");
    int final_count = 0;
    ASKL_MapIterator *it;
    for (it = map_each(_iter_map); it; it = map_next(it)) {
        if (!is_integer(it->val)) {
            printf("(!) Found non-integer value after test!\n");
            _iter_errors ++;
        }
        final_count ++;
    }

    printf("(-) Final map contains %d items (expected %d)\n",
           final_count, _ITER_ITEMS);

    if (final_count != _ITER_ITEMS) {
        printf("(!) Item count mismatch!\n");
        _iter_errors ++;
    }

    map_free(_iter_map);
    _iter_map = NULL;

    if (_iter_errors > 0) {
        printf("(!) Iterator concurrency test FAILED with %d errors!\n", _iter_errors);
        return -1;
    } else {
        printf("(*) Iterator concurrency test PASSED!\n");
        return 0;
    }
}

/* -------------------------------------------------------------------------- */
/* Test: map_remove_at() - Concurrent removal by multiple iterators          */
/* -------------------------------------------------------------------------- */

typedef struct {
    ASKL_LinkedMap *map;
    int thread_id;
    int removed_count;
    int iterations;
} removal_test_args;

static void* removal_thread(void* arg)
{
    removal_test_args *args = (removal_test_args*) arg;
    ASKL_MapIterator *it = NULL;
    variant removed;
    int count = 0;

    printf("(-) Removal thread %d: Starting...\n", args->thread_id);

    /* Iterate over map entries */
    it = map_each(args->map);

    while ((it = map_next(it)) != NULL) {
        args->iterations ++;

        /* Remove every 3rd item seen by this iterator */
        if ((count % 3) == (args->thread_id % 3)) {
            removed = map_remove_at(it);

            /* Check if removal succeeded (non-null variant) */
            if (! is_null(removed)) {
                args->removed_count ++;
            }
        }
        count++;
    }

    printf("(-) Removal thread %d: Iterated %d times, removed %d items\n",
           args->thread_id, args->iterations, args->removed_count);

    return NULL;
}

static int test_map_remove_at_concurrent(void)
{
    ASKL_LinkedMap *map = NULL;
    pthread_t threads[3];
    removal_test_args args[3] = {{0}};
    variant val;
    int total_removed = 0;
    int total_iterations = 0;
    int remaining = 0;

    printf("(-) Testing concurrent map_remove_at().\n");

    /* Create map */
    map = map_alloc(NULL);
    if (map == NULL) {
        printf("(!) Failed to allocate map\n");
        return -1;
    }

    /* Populate map with 1000 items */
    printf("(*) Populating map with 1000 items.\n");
    for (unsigned int i = 0; i < 1000; i ++) {
        char keybuf[32];
        snprintf(keybuf, sizeof(keybuf), "key_%04d", i);

        val = variant_from_integer(i);

        map_set(map, keybuf, strlen(keybuf), val);
    }

    printf("(*) Spawning 3 concurrent removal threads.\n");

    /* Spawn 3 concurrent removal threads */
    for (int i = 0; i < 3; i ++) {
        args[i].map = map;
        args[i].thread_id = i;
        args[i].removed_count = 0;
        args[i].iterations = 0;

        if (pthread_create(&threads[i], NULL, removal_thread, & args[i]) != 0) {
            printf("(!) Failed to create thread %d\n", i);
            map_free(map);
            return -1;
        }
    }

    /* Wait for all threads */
    for (int i = 0; i < 3; i ++) {
        pthread_join(threads[i], NULL);
        total_removed += args[i].removed_count;
        total_iterations += args[i].iterations;
    }

    printf("(-) Total items removed: %d\n", total_removed);
    printf("(-) Total iterations: %d\n", total_iterations);

    /* Count remaining items */
    for (ASKL_MapIterator *it = map_each(map); it; it = map_next(it))
        remaining ++;

    printf("(-) Remaining items: %d\n", remaining);
    printf("(-) Removed + Remaining = %d (expected 1000)\n",
           total_removed + remaining);

    /* Verify total consistency */
    if (total_removed + remaining != 1000) {
        printf("(!) FAILED: Data inconsistency detected!\n");
        printf("(!) Expected 1000, got %d\n", total_removed + remaining);
        map_free(map);
        return -1;
    }

    /* try to add some new entries */
    for (unsigned int i = 0; i < 1000; i ++) {
        char keybuf[32];
        snprintf(keybuf, sizeof(keybuf), "key_%04d", i);

        val = variant_from_integer(i + 1);

        map_set(map, keybuf, strlen(keybuf), val);
    }

    /* and read them back */
    for (unsigned int i = 0; i < 1000; i ++) {
        char keybuf[32];
        snprintf(keybuf, sizeof(keybuf), "key_%04d", i);
        if (variant_to_integer(map_get(map, keybuf, strlen(keybuf))) != i + 1) {
            map_free(map);
            return -1;
        }
    }

    /* Cleanup */
    map_free(map);

    printf("(*) Concurrent map_remove_at() test PASSED!\n");
    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int test_hashtable(void)
{
    ASKL_LinkedMap *h = NULL, *h2 = NULL;
    uintptr_t i = 0, j = 0;
    variant val = { 0 };
    int missing = 0;
    size_t len = 0;
    char key[BUFSIZ];
    clock_t start, stop;
    hashtable *x = NULL;
    _item *z = NULL, tmp;

    signal(SIGALRM, _timeout);

    printf("(-) Testing hash table implementation.\n");
    if (! (h = map_alloc(NULL)) ) {
        printf("(!) Allocating hash table: FAILURE\n");
        return -1;
    } else printf("(*) Allocating hash table: SUCCESS\n");

    printf("(*) Inserting key-value pairs.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        map_set(h, key, len, variant_from_integer(i));
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

    //printf("(-) Size of the hashtable: %zu items/%zu buckets\n",
    //        h->_bucket_count, h->_bucket_size);
    printf("(-) Memory footprint: %zu bytes.\n", map_footprint(h, & len));
    printf("(-) Overhead: %zu bytes (%zu KiB).\n", len, len / 1024);

    printf("(*) Getting back values from keys.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        val = map_get(h, key, len);
        if (is_integer(val)) {
            if ( (j = variant_to_integer(val)) != i) {
                missing ++;
                printf(
                    "(!) Key %" PRIuPTR " is missing ! "
                    "(found %" PRIuPTR " instead)\n",
                    i, j
                );
            }
        } else {
            missing ++;
            printf("(!) Key %" PRIuPTR " is missing !\n", i);
        }
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    missing = 0;

    alarm(2);
    printf("(*) Randomly deleting 100k keys.\n");
    while (! timeout && missing < _CACHE_RNDDL) {
        i = rand() % _CACHE_ITEMS;
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        if (is_integer(map_remove(h, key, len))) missing ++;
    }
    timeout = 1;

    /* sorting */
    printf("(*) Sorting.\n");
    start = clock();
    map_sort(h, MAP_ASC, map_sort_keys);
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

    missing = 0;

    printf("(*) Getting back values from keys.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        val = map_get(h, key, len);
        if (is_integer(val)) {
            if ( (j = variant_to_integer(val)) != i)
                missing ++;
        } else missing ++;
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    missing = 0;

    printf("(*) Replacing all keys values.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        map_set(h, key, len, variant_from_integer(i + 1));
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
        val = map_remove(h, key, len);
        if (! is_integer(val) || variant_to_integer(val) != (i + 1))
            missing ++;
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
        val = map_get(h, key, len);
        if (! is_integer(val) || variant_to_integer(val) != (i + 1))
            missing ++;
        else printf("(!) found phantom key %" PRIuPTR " !\n", i);
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    printf("(*) Overwriting a key.\n");
    val = map_set(h, key, len, variant_from_integer(0xc0ffee));
    if (is_integer(val)) {
        printf(
            "(!) Key insertion returned 0x%" PRIxPTR "\n",
            (uintptr_t) variant_to_integer(val)
        );
    }
    val = map_set(h, key, len, variant_from_integer(0xcafe));
    if (is_integer(val)) {
        if (variant_to_integer(val) != 0xc0ffee) {
            printf(
                "(!) Key overwrite returned 0x%" PRIxPTR "\n",
                (uintptr_t) variant_to_integer(val)
            );
        }
    } else printf("(!) Key overwrite failure!\n");
    val = map_remove(h, key, len);
    if (is_integer(val)) {
        if (variant_to_integer(val) != 0xcafe) {
            printf(
                "(!) Retrieved key is 0x%" PRIxPTR ", expected 0xCAFE.\n",
                (uintptr_t) variant_to_integer(val)
            );
        } else printf("(*) Value was successfully overwritten.\n");
    } else printf("(!) Key was not overwritten!\n");

    /* sort test */
    map_set(h, "zzzzz", strlen("zzzzz"), variant_from_integer(0x0));
    map_set(h, "tedst", strlen("tedst"), variant_from_integer(0x1));
    map_set(h, "testa", strlen("testa"), variant_from_integer(0x2));
    map_set(h, "btest", strlen("btest"), variant_from_integer(0x4));
    map_set(h, "tcest", strlen("tcest"), variant_from_integer(0x8));
    map_sort(h, MAP_ASC, map_sort_keys);
    map_foreach(h, _print_and_delete_key);

    map_set(h, "zzzzz", strlen("zzzzz"), variant_from_integer(0x0));
    map_set(h, "tedst", strlen("tedst"), variant_from_integer(0x1));
    map_set(h, "testa", strlen("testa"), variant_from_integer(0x2));
    map_set(h, "btest", strlen("btest"), variant_from_integer(0x4));
    map_set(h, "tcest", strlen("tcest"), variant_from_integer(0x8));
    map_sort(h, MAP_DESC, map_sort_keys);
    map_foreach(h, _print_and_delete_key);

    map_set(h, "btest", strlen("btest"), variant_from_integer(0x4));
    map_set(h, "tcest", strlen("tcest"), variant_from_integer(0x8));
    map_sort(h, MAP_DESC, map_sort_keys);
    map_foreach(h, _print_and_delete_key);

    map_set(h, "A", 1, variant_from_integer(0x1));
    map_set(h, "B", 1, variant_from_integer(0x2));
    map_set(h, "C", 1, variant_from_integer(0x3));
    map_set(h, "D", 1, variant_from_integer(0x4));

    h2 = map_alloc(NULL);
    map_set(h2, "D", 1, variant_from_integer(0x44));
    map_set(h2, "E", 1, variant_from_integer(0x5));
    map_set(h2, "F", 1, variant_from_integer(0x6));
    map_set(h2, "G", 1, variant_from_integer(0x7));
    map_set(h2, "H", 1, variant_from_integer(0x8));

    map_merge(h, h2, _merge);
    map_foreach(h, _print_key_intval);

    h = map_free(h);

    if (test_iterator_concurrency() == -1) {
        return -1;
    }

    if (test_map_remove_at_concurrent() == -1) {
        return -1;
    }

    printf("(-) Testing C.B. Falconer Hashlib for comparison.\n");
    if (! (x = hashlib_alloc(_hash, _rehash, _hashcmp, _hashdup, _hashfree, 0)) ) {
        printf("(!) Allocating hash table: FAILURE\n");
        return -1;
    } else printf("(*) Allocating hash table: SUCCESS\n");

    tmp.key = key;

    printf("(*) Inserting key-value pairs.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        tmp.len = snprintf(tmp.key, sizeof(key), _CACHE_KEYFM, i);
        tmp.val = (void *) (uintptr_t) i;
        hashlib_insert(x, & tmp);
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

    printf("(-) Memory footprint (keys and data not included): %zu bytes.\n",
            hashlib_footprint(x));

    missing = 0;

    printf("(*) Getting back values from keys.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        tmp.len = snprintf(tmp.key, sizeof(key), _CACHE_KEYFM, i);
        if (! (z = hashlib_find(x, & tmp)) ) {
            missing ++;
            printf("(!) Key %" PRIuPTR " is missing !\n", i);
        }
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    missing = 0;

    timeout = 0;

    alarm(2);
    printf("(*) Randomly deleting 100k keys.\n");
    while (! timeout && missing < _CACHE_RNDDL) {
        i = rand() % _CACHE_ITEMS;
        tmp.len = snprintf(tmp.key, sizeof(key), _CACHE_KEYFM, i);
        if (hashlib_remove(x, & tmp)) missing ++;
    }
    timeout = 1;

    missing = 0;

    printf("(*) Replacing all keys values.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        tmp.len = snprintf(tmp.key, sizeof(key), _CACHE_KEYFM, i);
        tmp.val = (void *) (uintptr_t) (i + 1);
        hashlib_insert(x, & tmp);
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    /* remove all the keys */
    printf("(*) Removing all the data from the table.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        tmp.len = snprintf(tmp.key, sizeof(key), _CACHE_KEYFM, i);
        hashlib_remove(x, & tmp);
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

    missing = 0;

    printf("(*) Checking that all the keys have been deleted.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        tmp.len = snprintf(tmp.key, sizeof(key), _CACHE_KEYFM, i);
        if (! (hashlib_find(x, & tmp)) ) missing ++;
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    hashlib_free(x);

    return 0;
}

/* -------------------------------------------------------------------------- */
