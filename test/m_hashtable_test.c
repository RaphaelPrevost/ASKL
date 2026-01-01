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

/* worker threads */
static pthread_t _thread[_CACHE_CONCURRENCY];

static int timeout = 0;

static ASKL_HashTable *v;

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

static void *_insert_loop(void *range)
{
    uintptr_t i = 0;
    size_t len = 0;
    char key[BUFSIZ];
    uintptr_t r = (uintptr_t) range;

    /* wait for it... */
    pthread_mutex_lock(& mx_switch);
        while (! start_switch) pthread_cond_wait(& cd_switch, & mx_switch);
    pthread_mutex_unlock(& mx_switch);

    for (i = r; i <= r + _CACHE_THRNG; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        htable_insert(v, key, len, variant_from_integer(i));
    }

    pthread_exit(NULL);
}

/* -------------------------------------------------------------------------- */

static void *_read_loop(void *range)
{
    uintptr_t i = 0, j = 0;
    variant val;
    int missing = 0;
    size_t len = 0;
    char key[BUFSIZ];
    uintptr_t r = (uintptr_t) range;

    /* wait for it... */
    pthread_mutex_lock(& mx_switch);
        while (! start_switch) pthread_cond_wait(& cd_switch, & mx_switch);
    pthread_mutex_unlock(& mx_switch);

    for (i = r; i <= r + _CACHE_THRNG; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        val = htable_get(v, key, len);
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
            printf("(!) Key %" PRIuPTR " is missing ! ", i);
        }
    }

    printf("(-) %i missing keys\n", missing);

    pthread_exit(NULL);
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
    printf("%.*s = %i\n", (int) len, key, variant_to_integer(val));
    return 0;
}

/* -------------------------------------------------------------------------- */

static variant _merge(const char *key, size_t len, variant dest, variant src)
{
    /* overwrite */
    return src;
}

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

    printf("(-) Testing hash table implementation.\n");
    if (! (v = htable_alloc(NULL)) ) {
        printf("(!) Allocating hash table: FAILURE\n");
        return -1;
    } else printf("(*) Allocating hash table: SUCCESS\n");

    /* spawn the worker threads */
    for (i = 0; i < _CACHE_CONCURRENCY; i ++) {
        if (pthread_create(
                & _thread[i], NULL,
                _insert_loop,
                (void *) (uintptr_t) (i * _CACHE_THRNG)
            ) == -1) {
            perror(ERR(test_hashtable, pthread_create));
            return -1;
        }
    }

    /* everything is ready, start the worker threads */
    pthread_mutex_lock(& mx_switch);
    start_switch = 1;
    pthread_cond_broadcast(& cd_switch);
    pthread_mutex_unlock(& mx_switch);

    printf("(*) Inserting key-value pairs.\n");
    start = clock();
    for (i = 0; i < _CACHE_CONCURRENCY; i ++)
        pthread_join(_thread[i], NULL);
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

    printf("(-) Memory footprint: %zu bytes.\n", htable_footprint(v, & len));
    printf("(-) Overhead: %zu bytes (%zu KiB).\n", len, len / 1024);

    /* spawn the worker threads */
    for (i = 0; i < _CACHE_CONCURRENCY; i ++) {
        if (pthread_create(
                & _thread[i], NULL,
                _read_loop,
                (void *) (uintptr_t) (i * _CACHE_THRNG)) == -1) {
            perror(ERR(test_hashtable, pthread_create));
            return -1;
        }
    }

    start_switch = 0;

    /* everything is ready, start the worker threads */
    pthread_mutex_lock(& mx_switch);
    start_switch = 1;
    pthread_cond_broadcast(& cd_switch);
    pthread_mutex_unlock(& mx_switch);

    printf("(*) Getting back values from keys.\n");
    start = clock();
    for (i = 0; i < _CACHE_CONCURRENCY; i ++)
        pthread_join(_thread[i], NULL);
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

    v = htable_free(v);

    signal(SIGALRM, _timeout);

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
