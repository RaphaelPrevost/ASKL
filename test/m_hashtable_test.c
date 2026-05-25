#include "../lib/askl_server.h"

#define _CACHE_CONCURRENCY 2

#define _CACHE_ITEMS 1000000
#define _CACHE_RNDDL 100000
#define _CACHE_THRNG 400000

#define _CACHE_KEYFM "%" PRIuPTR

/* thread start control switch */
static pthread_mutex_t mx_switch = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cd_switch = PTHREAD_COND_INITIALIZER;
static int start_switch = 0;

/* -------------------------------------------------------------------------- */

static int _print_and_delete_key(
    const char *key,
    size_t len,
    UNUSED Variant val,
    UNUSED void *context
)
{
    printf("%.*s\n", (int) len, key);
    return -1;
}

/* -------------------------------------------------------------------------- */

static int _print_key_intval(
    const char *key,
    size_t len,
    Variant val,
    UNUSED void *context
)
{
    printf("%.*s = %llu\n", (int) len, key, variant_to_integer(val));
    return 0;
}

/* -------------------------------------------------------------------------- */

static Variant _merge(
    UNUSED const char *key,
    UNUSED size_t len,
    UNUSED Variant dest,
    Variant src
)
{
    /* overwrite */
    return src;
}

/* -------------------------------------------------------------------------- */
/* Test: Random differential torture test                                     */
/* -------------------------------------------------------------------------- */

#define DIFF_KEY_UNIVERSE   256
#define DIFF_OPS            100000
#define DIFF_MAX_KEYLEN     16

typedef struct {
    unsigned char bytes[DIFF_MAX_KEYLEN];
    size_t len;
} diff_key_t;

typedef struct {
    unsigned char *key;
    size_t len;
    uint64_t value;
    int alive;
} diff_ref_t;

static diff_key_t _diff_keys[DIFF_KEY_UNIVERSE];
static diff_ref_t _diff_ref[DIFF_KEY_UNIVERSE];

static int _diff_sort_desc = 0;

static int _bytes_cmp(const unsigned char *a, size_t alen,
                      const unsigned char *b, size_t blen)
{
    size_t n = (alen < blen) ? alen : blen;
    int r = memcmp(a, b, n);
    if (r) return r;
    return (alen > blen) - (alen < blen);
}

static int _diff_ref_cmp_qsort(const void *pa, const void *pb)
{
    const diff_ref_t *a = *(const diff_ref_t * const *) pa;
    const diff_ref_t *b = *(const diff_ref_t * const *) pb;
    int r = _bytes_cmp(a->key, a->len, b->key, b->len);
    return _diff_sort_desc ? -r : r;
}

static void _diff_init_keys(void)
{
    for (int i = 0; i < DIFF_KEY_UNIVERSE; i ++) {
        size_t len = 4 + (rand() % (DIFF_MAX_KEYLEN - 3));
        _diff_keys[i].len = len;

        /* nasty prefix: repeated prefixes + embedded NULs */
        _diff_keys[i].bytes[0] = (unsigned char) ('A' + (i % 8));
        _diff_keys[i].bytes[1] = (unsigned char) ((i % 5) ? 0 : 'x');

        for (size_t j = 2; j < len - 2; j ++) {
            unsigned char byte;
            if ((rand() % 5) == 0) byte = 0;
            else byte = (unsigned char) (rand() & 0xff);
            _diff_keys[i].bytes[j] = byte;
        }

        /* unique suffix: encodes i, guarantees uniqueness */
        _diff_keys[i].bytes[len - 2] = (unsigned char) (i & 0xff);
        _diff_keys[i].bytes[len - 1] = (unsigned char) ((i >> 8) & 0xff);

        _diff_ref[i].key = _diff_keys[i].bytes;
        _diff_ref[i].len = _diff_keys[i].len;
        _diff_ref[i].value = 0;
        _diff_ref[i].alive = 0;
    }
}

static int _diff_ref_live_count(void)
{
    int n = 0;
    for (int i = 0; i < DIFF_KEY_UNIVERSE; i ++)
        if (_diff_ref[i].alive) n ++;
    return n;
}

static int _diff_ref_match(const char *key, size_t len)
{
    for (int i = 0; i < DIFF_KEY_UNIVERSE; i ++) {
        if (_diff_ref[i].alive &&
            _diff_ref[i].len == len &&
            memcmp(_diff_ref[i].key, key, len) == 0)
            return i;
    }
    return -1;
}

static int _diff_validate_membership(Map *h)
{
    int seen[DIFF_KEY_UNIVERSE] = { 0 };
    int count = 0;

    for (Map_Iterator *it = map_each(h); it; it = map_next(it)) {
        int idx = _diff_ref_match(it->key, it->len);
        if (idx < 0) {
            printf("(!) Differential test: iterator returned unknown key\n");
            map_break(it);
            return -1;
        }
        if (seen[idx]) {
            printf("(!) Differential test: duplicate key in iteration\n");
            map_break(it);
            return -1;
        }
        seen[idx] = 1;
        count ++;

        if (! is_integer(it->val) ||
            variant_to_integer(it->val) != _diff_ref[idx].value) {
            printf("(!) Differential test: value mismatch in iteration\n");
            map_break(it);
            return -1;
        }
    }

    if (count != _diff_ref_live_count()) {
        printf("(!) Differential test: live-count mismatch (%d vs %d)\n",
               count, _diff_ref_live_count());
        return -1;
    }

    /* verify point lookups too */
    for (int i = 0; i < DIFF_KEY_UNIVERSE; i ++) {
        Variant v = map_get(
            h,
            (const char *) _diff_keys[i].bytes,
            _diff_keys[i].len
        );
        int has = map_has(
            h,
            (const char *) _diff_keys[i].bytes,
            _diff_keys[i].len
        );

        if (_diff_ref[i].alive) {
            if (! is_integer(v) || variant_to_integer(v) != _diff_ref[i].value) {
                printf("(!) Differential test: lookup mismatch on live key %d\n", i);
                return -1;
            }
            if (! has) {
                printf("(!) Differential test: map_has false on live key %d\n", i);
                return -1;
            }
        } else {
            if (has) {
                printf("(!) Differential test: map_has true on dead key %d\n", i);
                return -1;
            }
            if (is_integer(v)) {
                printf("(!) Differential test: lookup returned value on dead key %d\n", i);
                return -1;
            }
        }
    }

    return 0;
}

static int _diff_validate_sorted(Map *h, int desc)
{
    diff_ref_t *live[DIFF_KEY_UNIVERSE];
    int n = 0, i = 0;

    _diff_sort_desc = desc;

    for (int k = 0; k < DIFF_KEY_UNIVERSE; k ++)
        if (_diff_ref[k].alive)
            live[n ++] = & _diff_ref[k];

    qsort(live, n, sizeof(*live), _diff_ref_cmp_qsort);

    for (Map_Iterator *it = map_each(h); it; it = map_next(it), i ++) {
        if (i >= n) {
            printf("(!) Differential sort test: iterator too long\n");
            map_break(it);
            return -1;
        }

        if (it->len != live[i]->len ||
            memcmp(it->key, live[i]->key, it->len) != 0) {
            printf("(!) Differential sort test: key order mismatch at %d\n", i);
            map_break(it);
            return -1;
        }

        if (! is_integer(it->val) ||
            variant_to_integer(it->val) != live[i]->value) {
            printf("(!) Differential sort test: value mismatch at %d\n", i);
            map_break(it);
            return -1;
        }
    }

    if (i != n) {
        printf("(!) Differential sort test: iterator too short (%d vs %d)\n", i, n);
        return -1;
    }

    return 0;
}

static int test_random_differential_map(void)
{
    Map *h = NULL;

    printf("(-) Testing random differential torture against reference model.\n");

    srand(0xC0FFEE);
    _diff_init_keys();

    h = map_alloc(NULL);
    if (! h) {
        printf("(!) Failed to allocate map\n");
        return -1;
    }

    for (int op = 0; op < DIFF_OPS; op ++) {
        int which = rand() % DIFF_KEY_UNIVERSE;
        uint64_t newv = ((uint64_t) op << 16) ^ (uint64_t) which;
        diff_key_t *k = & _diff_keys[which];
        Variant got;

        switch (rand() % 10) {
            case 0:
            case 1:
            case 2:
                /* set */
                map_set(h, (const char *) k->bytes, k->len,
                        variant_from_integer(newv));
                _diff_ref[which].alive = 1;
                _diff_ref[which].value = newv;
                break;

            case 3:
                got = map_insert(h, (const char *) k->bytes, k->len,
                                 variant_from_integer(newv));
                if (_diff_ref[which].alive) {
                    if (!is_integer(got) ||
                        variant_to_integer(got) != newv) {
                        printf("(!) Differential test: map_insert wrong existing value\n");
                        goto _fail;
                    }
                } else {
                    if (!is_null(got)) {
                        printf("(!) Differential test: map_insert should have inserted\n");
                        goto _fail;
                    }
                    _diff_ref[which].alive = 1;
                    _diff_ref[which].value = newv;
                }
                break;

            case 4:
                /* update */
                got = map_update(h, (const char *) k->bytes, k->len,
                                 variant_from_integer(newv));
                if (_diff_ref[which].alive) {
                    uint64_t oldv = _diff_ref[which].value;

                    if (oldv == newv) {
                        if (!is_null(got)) {
                            printf("(!) Differential test: map_update should return null on alias\n");
                            goto _fail;
                        }
                    } else {
                        if (!is_integer(got) ||
                            variant_to_integer(got) != oldv) {
                            printf("(!) Differential test: map_update wrong old value\n");
                            goto _fail;
                        }
                    }

                    _diff_ref[which].value = newv;
                } else {
                    if (!is_integer(got) ||
                        variant_to_integer(got) != newv) {
                        printf("(!) Differential test: map_update wrong reject on missing key\n");
                        goto _fail;
                    }
                }
                break;

            case 5:
            case 6:
                /* remove */
                got = map_remove(h, (const char *) k->bytes, k->len);
                if (_diff_ref[which].alive) {
                    if (! is_integer(got) ||
                        variant_to_integer(got) != _diff_ref[which].value) {
                        printf("(!) Differential test: map_remove wrong old value\n");
                        goto _fail;
                    }
                    _diff_ref[which].alive = 0;
                } else {
                    if (is_integer(got)) {
                        printf("(!) Differential test: map_remove returned live value for dead key\n");
                        goto _fail;
                    }
                }
                break;

            case 7:
            case 8:
                /* get/has */
                got = map_get(h, (const char *) k->bytes, k->len);
                if (_diff_ref[which].alive) {
                    if (! is_integer(got) ||
                        variant_to_integer(got) != _diff_ref[which].value) {
                        printf("(!) Differential test: map_get mismatch\n");
                        goto _fail;
                    }
                    if (! map_has(h, (const char *) k->bytes, k->len)) {
                        printf("(!) Differential test: map_has mismatch on live key\n");
                        goto _fail;
                    }
                } else {
                    if (map_has(h, (const char *) k->bytes, k->len)) {
                        printf("(!) Differential test: map_has mismatch on dead key\n");
                        goto _fail;
                    }
                }
                break;

            case 9:
                /* sort */
                if ((rand() & 1) == 0) {
                    if (map_sort(h, MAP_ASC, map_sort_keys) == -1) {
                        printf("(!) Differential test: map_sort asc failed\n");
                        goto _fail;
                    }
                    if (_diff_validate_sorted(h, 0) == -1) goto _fail;
                } else {
                    if (map_sort(h, MAP_DESC, map_sort_keys) == -1) {
                        printf("(!) Differential test: map_sort desc failed\n");
                        goto _fail;
                    }
                    if (_diff_validate_sorted(h, 1) == -1) goto _fail;
                }
                break;
        }

        if ((op % 250) == 0) {
            if (_diff_validate_membership(h) == -1) goto _fail;
        }
    }

    if (_diff_validate_membership(h) == -1) goto _fail;

    if (map_sort(h, MAP_ASC, map_sort_keys) == -1) goto _fail;
    if (_diff_validate_sorted(h, 0) == -1) goto _fail;

    if (map_sort(h, MAP_DESC, map_sort_keys) == -1) goto _fail;
    if (_diff_validate_sorted(h, 1) == -1) goto _fail;

    map_free(h);
    printf("(*) Random differential torture test PASSED!\n");
    return 0;

_fail:
    map_free(h);
    printf("(!) Random differential torture test FAILED!\n");
    return -1;
}

/* -------------------------------------------------------------------------- */
/* Test: Embedded NUL bytes in keys                                           */
/* -------------------------------------------------------------------------- */

static int test_embedded_nul_keys(void)
{
    Map *h = NULL;
    Variant v = { 0 };
    Map_Iterator *it = NULL;
    int count = 0;

    static const unsigned char k1[] = { 'a', '\0', 'x' };
    static const unsigned char k2[] = { 'a', '\0', 'y' };
    static const unsigned char k3[] = { 'a' };
    static const unsigned char k4[] = { 'a', '\0' };
    static const unsigned char k5[] = { '\0', 'z' };

    printf("(-) Testing embedded NUL bytes in keys.\n");

    h = map_alloc(NULL);
    if (! h) {
        printf("(!) Allocating map: FAILURE\n");
        return -1;
    }

    map_set(h, (const char *) k1, sizeof(k1), variant_from_integer(11));
    map_set(h, (const char *) k2, sizeof(k2), variant_from_integer(22));
    map_set(h, (const char *) k3, sizeof(k3), variant_from_integer(33));
    map_set(h, (const char *) k4, sizeof(k4), variant_from_integer(44));
    map_set(h, (const char *) k5, sizeof(k5), variant_from_integer(55));

    v = map_get(h, (const char *) k1, sizeof(k1));
    if (! is_integer(v) || variant_to_integer(v) != 11) goto _fail;

    v = map_get(h, (const char *) k2, sizeof(k2));
    if (! is_integer(v) || variant_to_integer(v) != 22) goto _fail;

    v = map_get(h, (const char *) k3, sizeof(k3));
    if (! is_integer(v) || variant_to_integer(v) != 33) goto _fail;

    v = map_get(h, (const char *) k4, sizeof(k4));
    if (! is_integer(v) || variant_to_integer(v) != 44) goto _fail;

    v = map_get(h, (const char *) k5, sizeof(k5));
    if (! is_integer(v) || variant_to_integer(v) != 55) goto _fail;

    /* Distinct byte strings must not alias each other */
    if (map_has(h, (const char *) k1, sizeof(k1)) != 1) goto _fail;
    if (map_has(h, (const char *) k2, sizeof(k2)) != 1) goto _fail;
    if (map_has(h, (const char *) k3, sizeof(k3)) != 1) goto _fail;
    if (map_has(h, (const char *) k4, sizeof(k4)) != 1) goto _fail;
    if (map_has(h, (const char *) k5, sizeof(k5)) != 1) goto _fail;

    /* Remove one embedded-NUL key and ensure others survive */
    v = map_remove(h, (const char *) k2, sizeof(k2));
    if (! is_integer(v) || variant_to_integer(v) != 22) goto _fail;

    if (map_has(h, (const char *) k2, sizeof(k2)) != 0) goto _fail;

    v = map_get(h, (const char *) k1, sizeof(k1));
    if (! is_integer(v) || variant_to_integer(v) != 11) goto _fail;

    v = map_get(h, (const char *) k4, sizeof(k4));
    if (! is_integer(v) || variant_to_integer(v) != 44) goto _fail;

    /* Sort should also preserve exact byte-string identity */
    if (map_sort(h, MAP_ASC, map_sort_keys) == -1) goto _fail;

    count = 0;
    for (it = map_each(h); it; it = map_next(it)) {
        count ++;
        if (! is_integer(it->val)) goto _fail;
    }

    if (count != 4) goto _fail;

    map_free(h);
    printf("(*) Embedded NUL key test PASSED!\n");
    return 0;

_fail:
    printf("(!) Embedded NUL key test FAILED!\n");
    map_free(h);
    return -1;
}

/* -------------------------------------------------------------------------- */
/* Test: Return values contract                                               */
/* -------------------------------------------------------------------------- */

static Variant _keep_old(
    UNUSED const char *k,
    UNUSED size_t l,
    Variant old,
    UNUSED Variant new
)
{
    return old;
}

static Variant _take_new(
    UNUSED const char *k,
    UNUSED size_t l,
    UNUSED Variant old,
    Variant new
)
{
    return new;
}

static Variant _third_value(
    UNUSED const char *k,
    UNUSED size_t l,
    UNUSED Variant old,
    UNUSED Variant new
)
{
    return variant_from_integer(333);
}

static int test_map_return_contracts(void)
{
    Map *h = map_alloc(NULL);
    Variant r, v;

    if (!h) return -1;

    printf("(-) Testing map return-value ownership contracts.\n");

    /* insert success: map retains value, caller gets null */
    r = map_insert(h, "a", 1, variant_from_integer(1));
    if (!is_null(r)) goto _fail;

    /* duplicate insert: map rejects proposed value, caller gets proposed */
    r = map_insert(h, "a", 1, variant_from_integer(2));
    if (!is_integer(r) || variant_to_integer(r) != 2) goto _fail;

    v = map_get(h, "a", 1);
    if (!is_integer(v) || variant_to_integer(v) != 1) goto _fail;

    /* set replacement: caller gets displaced old value */
    r = map_set(h, "a", 1, variant_from_integer(2));
    if (!is_integer(r) || variant_to_integer(r) != 1) goto _fail;

    /* set same value: no distinct value to return */
    r = map_set(h, "a", 1, variant_from_integer(2));
    if (!is_null(r)) goto _fail;

    /* update missing: proposed value was not retained */
    r = map_update(h, "missing", 7, variant_from_integer(9));
    if (!is_integer(r) || variant_to_integer(r) != 9) goto _fail;

    /* update existing: caller gets displaced old value */
    r = map_update(h, "a", 1, variant_from_integer(10));
    if (!is_integer(r) || variant_to_integer(r) != 2) goto _fail;

    /* update same value: no distinct value to return */
    r = map_update(h, "a", 1, variant_from_integer(10));
    if (!is_null(r)) goto _fail;

    /* callback keeps old: proposed value is returned */
    r = map_set_with(h, "a", 1, variant_from_integer(20), _keep_old);
    if (!is_integer(r) || variant_to_integer(r) != 20) goto _fail;

    v = map_get(h, "a", 1);
    if (!is_integer(v) || variant_to_integer(v) != 10) goto _fail;

    /* callback takes new: old value is returned */
    r = map_set_with(h, "a", 1, variant_from_integer(30), _take_new);
    if (!is_integer(r) || variant_to_integer(r) != 10) goto _fail;

    v = map_get(h, "a", 1);
    if (!is_integer(v) || variant_to_integer(v) != 30) goto _fail;

    /* callback returns third value: proposed value is returned */
    r = map_set_with(h, "a", 1, variant_from_integer(40), _third_value);
    if (!is_integer(r) || variant_to_integer(r) != 40) goto _fail;

    v = map_get(h, "a", 1);
    if (!is_integer(v) || variant_to_integer(v) != 333) goto _fail;

    map_free(h);
    printf("(*) Map return-value contract test PASSED!\n");
    return 0;

_fail:
    map_free(h);
    printf("(!) Map return-value contract test FAILED!\n");
    return -1;
}

/* -------------------------------------------------------------------------- */
/* Test: Concurrent write stress - Tests unlock fast path with real workload  */
/* -------------------------------------------------------------------------- */

#define WRITE_STRESS_THREADS 8
#define WRITE_STRESS_OPS_PER_THREAD 5000

typedef struct {
    Map *map;
    int thread_id;
    int ops_completed;
    int errors;
} write_stress_args;

static void* write_stress_thread(void* arg)
{
    write_stress_args *args = (write_stress_args*)arg;
    Variant val;
    char keybuf[32];

    printf("(-) Write stress thread %d: Starting...\n", args->thread_id);

    for (int op = 0; op < WRITE_STRESS_OPS_PER_THREAD; op++) {
        /* Generate key with thread_id + op to ensure variety */
        snprintf(keybuf, sizeof(keybuf), "key_%d_%d", args->thread_id, op);

        /* Create a value tied to this thread */
        val = variant_from_integer((uint64_t)(args->thread_id * 1000000 + op));

        /* Write to map (acquires write lock internally) */
        map_set(args->map, keybuf, strlen(keybuf), val);
        args->ops_completed++;

        /* Occasionally yield to increase lock contention */
        if (op % 500 == 0)
            sched_yield();
    }

    printf("(-) Write stress thread %d: Completed %d operations\n",
           args->thread_id, args->ops_completed);

    return NULL;
}

static int test_write_stress_concurrent(void)
{
    Map *map = NULL;
    pthread_t threads[WRITE_STRESS_THREADS];
    write_stress_args args[WRITE_STRESS_THREADS] = {{0}};
    int total_ops = 0;

    printf("(-) Testing concurrent write stress on hashmap.\n");

    /* Create map */
    map = map_alloc(NULL);
    if (map == NULL) {
        printf("(!) Failed to allocate map\n");
        return -1;
    }

    printf("(*) Spawning %d concurrent writer threads.\n", WRITE_STRESS_THREADS);
    printf("(*) Each thread will perform %d write operations.\n", WRITE_STRESS_OPS_PER_THREAD);
    printf("(*) This heavily stresses the unlock() fast path.\n");

    /* Spawn threads */
    for (int i = 0; i < WRITE_STRESS_THREADS; i ++) {
        args[i].map = map;
        args[i].thread_id = i;
        args[i].ops_completed = 0;
        args[i].errors = 0;

        if (pthread_create(&threads[i], NULL, write_stress_thread, & args[i]) != 0) {
            printf("(!) Failed to create thread %d\n", i);
            map_free(map);
            return -1;
        }
    }

    /* Wait for all threads */
    printf("(*) Waiting for all threads to complete...\n");
    for (int i = 0; i < WRITE_STRESS_THREADS; i ++) {
        pthread_join(threads[i], NULL);
        total_ops += args[i].ops_completed;

        if (args[i].errors > 0) {
            printf("(!) Thread %d had %d errors\n", i, args[i].errors);
        }
    }

    printf("(-) Total operations completed: %d\n", total_ops);

    /* Verify: count items in map */
    int final_count = 0;
    for (Map_Iterator *it = map_each(map); it; it = map_next(it)) {
        final_count ++;
    }

    int expected_count = WRITE_STRESS_THREADS * WRITE_STRESS_OPS_PER_THREAD;
    printf("(-) Final map size: %d (expected %d)\n", final_count, expected_count);

    /* Verify all operations succeeded */
    if (total_ops != expected_count) {
        printf("(!) Operations mismatch! Expected %d, got %d\n", expected_count, total_ops);
        map_free(map);
        return -1;
    }

    if (final_count != expected_count) {
        printf("(!) Item count mismatch! Expected %d, got %d\n", expected_count, final_count);
        map_free(map);
        return -1;
    }

    /* Cleanup */
    map_free(map);

    printf("(*) Write stress test PASSED!\n");
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Iterator Concurrent Test (No External Writers)                            */
/* -------------------------------------------------------------------------- */

#define _ITER_THREADS 4
#define _ITER_ITEMS 10000

static pthread_t _iter_thread[_ITER_THREADS];
static Map *_iter_map = NULL;
static volatile int _iter_errors = 0;

/* Thread that reads via iterator (no modifications) */
static void *_iterator_reader(void *arg)
{
    uintptr_t tid = (uintptr_t) arg;
    Map_Iterator *it;
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
    Map_Iterator *it;
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

                Variant old = map_set_at(it, variant_from_integer(new_val));

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
    Map_Iterator *it;
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
/* Test: map_remove_at() - Concurrent removal by multiple iterators           */
/* -------------------------------------------------------------------------- */

typedef struct {
    Map *map;
    int thread_id;
    int removed_count;
    int iterations;
} removal_test_args;

static void* removal_thread(void* arg)
{
    removal_test_args *args = (removal_test_args*) arg;
    Map_Iterator *it = NULL;
    Variant removed;
    int count = 0;

    printf("(-) Removal thread %d: Starting...\n", args->thread_id);

    /* Iterate over map entries */
    for (it = map_each(args->map); it; it = map_next(it)) {
        args->iterations ++;

        /* Remove every 3rd item seen by this iterator */
        if ((count % 3) == (args->thread_id % 3)) {
            removed = map_remove_at(it);

            /* Check if removal succeeded (non-null variant) */
            if (! is_null(removed)) {
                args->removed_count ++;
            }
        }
        count ++;
    }

    printf("(-) Removal thread %d: Iterated %d times, removed %d items\n",
           args->thread_id, args->iterations, args->removed_count);

    return NULL;
}

static int test_map_remove_at_concurrent(void)
{
    Map *map = NULL;
    pthread_t threads[3];
    removal_test_args args[3] = {{0}};
    Variant val;
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
    for (Map_Iterator *it = map_each(map); it; it = map_next(it))
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
/* Test: Writer Starvation Detection                                          */
/* -------------------------------------------------------------------------- */

#define STARVATION_TEST_DURATION_SEC 5
#define STARVATION_READER_THREADS 16
#define STARVATION_WRITER_THREADS 2

typedef struct {
    Map *map;
    int thread_id;
    volatile int stop;

    /* Statistics */
    uint64_t operations;
    uint64_t total_latency_ns;
    uint64_t max_latency_ns;
    uint64_t timeouts;  // Latencies > 100ms
} starvation_test_args;

/* Helper: Get time in nanoseconds */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/* Reader thread: Continuously read from map */
static void* starvation_reader_thread(void* arg) {
    starvation_test_args *args = (starvation_test_args*)arg;
    char keybuf[32];

    while (! args->stop) {
        /* Pick a random key */
        unsigned int key_id = rand() % 10000;
        snprintf(keybuf, sizeof(keybuf), "key_%04u", key_id);

        /* Read operation (acquires read lock) */
        uint64_t start = get_time_ns();
        Variant val = map_get(args->map, keybuf, strlen(keybuf));
        uint64_t latency = get_time_ns() - start;

        args->operations ++;
        args->total_latency_ns += latency;

        if (latency > args->max_latency_ns) {
            args->max_latency_ns = latency;
        }

        /* Do a tiny bit of work */
        if (is_integer(val)) {
            volatile uint64_t x = variant_to_integer(val);
            (void) x;
        }

        /* Yield occasionally to increase contention */
        if (args->operations % 100 == 0) {
            sched_yield();
        }
    }

    return NULL;
}

/* Writer thread: Continuously write to map */
static void *starvation_writer_thread(void *arg) {
    starvation_test_args *args = (starvation_test_args *) arg;
    char keybuf[32];
    uint64_t write_count = 0;

    while (! args->stop) {
        /* Pick a random key */
        unsigned int key_id = rand() % 10000;
        snprintf(keybuf, sizeof(keybuf), "key_%04u", key_id);

        /* Write operation (acquires write lock) */
        uint64_t start = get_time_ns();
        map_set(args->map, keybuf, strlen(keybuf),
                variant_from_integer(write_count));
        uint64_t latency = get_time_ns() - start;

        args->operations ++;
        args->total_latency_ns += latency;

        if (latency > args->max_latency_ns) {
            args->max_latency_ns = latency;
        }

        /* Count latencies > 100ms as "timeouts" (starvation indicator) */
        if (latency > 100000000ULL) {  // 100ms
            args->timeouts ++;
            printf("(!) Writer %d: Experienced %.1f ms latency (possible starvation)\n",
                   args->thread_id, latency / 1000000.0);
        }

        write_count ++;

        /* Writers should write less frequently than readers read */
        usleep(1000);  // 1ms delay between writes
    }

    return NULL;
}

static int test_writer_starvation(void) {
    Map *map = NULL;
    pthread_t reader_threads[STARVATION_READER_THREADS];
    pthread_t writer_threads[STARVATION_WRITER_THREADS];
    starvation_test_args reader_args[STARVATION_READER_THREADS] = {{0}};
    starvation_test_args writer_args[STARVATION_WRITER_THREADS] = {{0}};

    printf("(-) Testing for writer starvation under heavy read load.\n");
    printf("(*) Configuration:\n");
    printf("    - %d reader threads (continuous)\n", STARVATION_READER_THREADS);
    printf("    - %d writer threads (1 write/ms)\n", STARVATION_WRITER_THREADS);
    printf("    - Test duration: %d seconds\n", STARVATION_TEST_DURATION_SEC);

    /* Create and populate map */
    map = map_alloc(NULL);
    if (map == NULL) {
        printf("(!) Failed to allocate map\n");
        return -1;
    }

    printf("(*) Populating map with 10000 items...\n");
    for (unsigned int i = 0; i < 10000; i ++) {
        char keybuf[32];
        snprintf(keybuf, sizeof(keybuf), "key_%04u", i);
        map_set(map, keybuf, strlen(keybuf), variant_from_integer(i));
    }

    /* Spawn reader threads */
    printf("(*) Spawning %d reader threads...\n", STARVATION_READER_THREADS);
    for (int i = 0; i < STARVATION_READER_THREADS; i ++) {
        reader_args[i].map = map;
        reader_args[i].thread_id = i;
        reader_args[i].stop = 0;
        if (pthread_create(& reader_threads[i], NULL,
                          starvation_reader_thread, & reader_args[i]) != 0) {
            printf("(!) Failed to create reader thread %d\n", i);
            return -1;
        }
    }

    /* Spawn writer threads */
    printf("(*) Spawning %d writer threads...\n", STARVATION_WRITER_THREADS);
    for (int i = 0; i < STARVATION_WRITER_THREADS; i++) {
        writer_args[i].map = map;
        writer_args[i].thread_id = i;
        writer_args[i].stop = 0;
        if (pthread_create(&writer_threads[i], NULL,
                          starvation_writer_thread, &writer_args[i]) != 0) {
            printf("(!) Failed to create writer thread %d\n", i);
            return -1;
        }
    }

    printf("(*) Running test for %d seconds...\n", STARVATION_TEST_DURATION_SEC);
    sleep(STARVATION_TEST_DURATION_SEC);

    /* Stop all threads */
    printf("(*) Stopping threads...\n");
    for (int i = 0; i < STARVATION_READER_THREADS; i ++) {
        reader_args[i].stop = 1;
    }
    for (int i = 0; i < STARVATION_WRITER_THREADS; i ++) {
        writer_args[i].stop = 1;
    }

    /* Join all threads */
    for (int i = 0; i < STARVATION_READER_THREADS; i ++) {
        pthread_join(reader_threads[i], NULL);
    }
    for (int i = 0; i < STARVATION_WRITER_THREADS; i ++) {
        pthread_join(writer_threads[i], NULL);
    }

    /* Analyze results */
    printf("\n(*) Results:\n\n");

    printf("Readers:\n");
    uint64_t total_reader_ops = 0;
    for (int i = 0; i < STARVATION_READER_THREADS; i ++) {
        double avg_latency = (double) reader_args[i].total_latency_ns /
                             reader_args[i].operations;
        printf("  Reader %2d: %8llu ops, avg %.2f µs, max %.2f µs\n",
               i,
               (unsigned long long) reader_args[i].operations,
               avg_latency / 1000.0,
               reader_args[i].max_latency_ns / 1000.0);
        total_reader_ops += reader_args[i].operations;
    }
    printf("  Total read ops: %llu\n\n", (unsigned long long) total_reader_ops);

    printf("Writers:\n");
    uint64_t total_writer_ops = 0;
    uint64_t total_timeouts = 0;
    for (int i = 0; i < STARVATION_WRITER_THREADS; i ++) {
        double avg_latency = (double) writer_args[i].total_latency_ns /
                             writer_args[i].operations;
        printf("  Writer %2d: %8llu ops, avg %.2f µs, max %.2f ms, timeouts: %llu\n",
               i,
               (unsigned long long) writer_args[i].operations,
               avg_latency / 1000.0,
               writer_args[i].max_latency_ns / 1000000.0,
               (unsigned long long) writer_args[i].timeouts);
        total_writer_ops += writer_args[i].operations;
        total_timeouts += writer_args[i].timeouts;
    }
    printf("  Total write ops: %llu\n", (unsigned long long) total_writer_ops);
    printf("  Total starvation events (>100ms): %llu\n\n",
           (unsigned long long) total_timeouts);

    /* Verdict */
    if (total_timeouts > 0) {
        printf("(!) STARVATION DETECTED: Writers experienced %llu delays >100ms\n",
               (unsigned long long) total_timeouts);
        printf("(!) This indicates writers were starved by continuous reader load.\n");
    } else {
        printf("(*) No starvation detected. Writers acquired locks promptly.\n");
    }

    /* Check fairness: writers should complete at least some operations */
    uint64_t expected_min_writes = STARVATION_TEST_DURATION_SEC * 250;
    if (total_writer_ops < expected_min_writes) {
        printf("(!) Writers completed only %llu ops (expected >%llu)\n",
               (unsigned long long) total_writer_ops,
               (unsigned long long) expected_min_writes);
        printf("(!) This suggests writers were significantly delayed.\n");
    }

    /* Cleanup */
    map_free(map);

    return (total_timeouts > 0) ? -1 : 0;
}

/* -------------------------------------------------------------------------- */
/* Test: Sticky/lazy sorting torture                                          */
/* -------------------------------------------------------------------------- */

#define STICKY_KEY_UNIVERSE 128
#define STICKY_MAX_KEYLEN   16
#define STICKY_SORT_KEY     0
#define STICKY_SORT_VALUE   1

typedef struct {
    unsigned char key[STICKY_MAX_KEYLEN];
    size_t len;
    uint64_t value;
    int alive;
} sticky_ref_t;

static sticky_ref_t _sticky_ref[STICKY_KEY_UNIVERSE];
static int _sticky_qsort_mode = STICKY_SORT_KEY;
static int _sticky_qsort_desc = 0;

static int _sticky_bytes_cmp(
    const unsigned char *a,
    size_t alen,
    const unsigned char *b,
    size_t blen
)
{
    size_t n = (alen < blen) ? alen : blen;
    int r = memcmp(a, b, n);

    if (r)
        return r;

    return (alen > blen) - (alen < blen);
}

static int _sticky_key_cmp(
    const char *key0,
    size_t len0,
    UNUSED Variant val0,
    const char *key1,
    size_t len1,
    UNUSED Variant val1
)
{
    return _sticky_bytes_cmp(
        (const unsigned char *) key0,
        len0,
        (const unsigned char *) key1,
        len1
    );
}

static int _sticky_value_cmp(
    const char *key0,
    size_t len0,
    Variant val0,
    const char *key1,
    size_t len1,
    Variant val1
)
{
    uint64_t v0 = variant_to_integer(val0);
    uint64_t v1 = variant_to_integer(val1);

    if (v0 < v1)
        return -1;

    if (v0 > v1)
        return 1;

    return _sticky_bytes_cmp(
        (const unsigned char *) key0,
        len0,
        (const unsigned char *) key1,
        len1
    );
}

static int _sticky_ref_cmp(const sticky_ref_t *a, const sticky_ref_t *b)
{
    int r = 0;

    if (_sticky_qsort_mode == STICKY_SORT_VALUE) {
        if (a->value < b->value)
            r = -1;
        else if (a->value > b->value)
            r = 1;
    }

    if (! r) {
        r = _sticky_bytes_cmp(
            a->key,
            a->len,
            b->key,
            b->len
        );
    }

    return _sticky_qsort_desc ? -r : r;
}

static int _sticky_ref_cmp_qsort(const void *pa, const void *pb)
{
    const sticky_ref_t *a = *(const sticky_ref_t * const *) pa;
    const sticky_ref_t *b = *(const sticky_ref_t * const *) pb;

    return _sticky_ref_cmp(a, b);
}

static void _sticky_init_refs(void)
{
    for (int i = 0; i < STICKY_KEY_UNIVERSE; i ++) {
        size_t len = 4 + (rand() % (STICKY_MAX_KEYLEN - 3));

        _sticky_ref[i].len = len;

        /*
         * Repeated prefixes + embedded NULs + unique suffix.
         * This stresses comparator/length handling and hash equality.
         */
        _sticky_ref[i].key[0] = (unsigned char) ('a' + (i % 7));
        _sticky_ref[i].key[1] = (unsigned char) ((i % 4) ? 0 : 'X');

        for (size_t j = 2; j < len - 2; j ++) {
            if ((rand() % 4) == 0)
                _sticky_ref[i].key[j] = 0;
            else
                _sticky_ref[i].key[j] = (unsigned char) (rand() & 0xff);
        }

        _sticky_ref[i].key[len - 2] = (unsigned char) (i & 0xff);
        _sticky_ref[i].key[len - 1] = (unsigned char) ((i >> 8) & 0xff);

        _sticky_ref[i].value = 0;
        _sticky_ref[i].alive = 0;
    }
}

static int _sticky_live_sorted(
    sticky_ref_t **live,
    int mode,
    int desc
)
{
    int n = 0;

    _sticky_qsort_mode = mode;
    _sticky_qsort_desc = desc;

    for (int i = 0; i < STICKY_KEY_UNIVERSE; i ++) {
        if (_sticky_ref[i].alive)
            live[n ++] = & _sticky_ref[i];
    }

    qsort(live, n, sizeof(*live), _sticky_ref_cmp_qsort);

    return n;
}

static int _sticky_validate_order(
    Map *h,
    int mode,
    int desc,
    const char *where
)
{
    sticky_ref_t *live[STICKY_KEY_UNIVERSE];
    int n = _sticky_live_sorted(live, mode, desc);
    int i = 0;

    for (Map_Iterator *it = map_each(h); it; it = map_next(it), i ++) {
        if (i >= n) {
            printf("(!) Sticky sort: iterator too long at %s\n", where);
            map_break(it);
            return -1;
        }

        if (it->len != live[i]->len ||
            memcmp(it->key, live[i]->key, it->len) != 0) {
            printf("(!) Sticky sort: key order mismatch at %s, index %d\n",
                   where, i);
            map_break(it);
            return -1;
        }

        if (! is_integer(it->val) ||
            variant_to_integer(it->val) != live[i]->value) {
            printf("(!) Sticky sort: value mismatch at %s, index %d\n",
                   where, i);
            map_break(it);
            return -1;
        }
    }

    if (i != n) {
        printf("(!) Sticky sort: iterator too short at %s (%d vs %d)\n",
               where, i, n);
        return -1;
    }

    return 0;
}

static int _sticky_validate_from_at(
    Map *h,
    int mode,
    int desc,
    int start_seed,
    const char *where
)
{
    sticky_ref_t *live[STICKY_KEY_UNIVERSE];
    int n = _sticky_live_sorted(live, mode, desc);
    int pos, i;
    Map_Iterator *it = NULL;

    if (! n)
        return 0;

    pos = start_seed % n;

    it = map_at(
        h,
        (const char *) live[pos]->key,
        live[pos]->len
    );

    if (! it) {
        printf("(!) Sticky sort: map_at failed at %s\n", where);
        return -1;
    }

    for (i = pos; it; it = map_next(it), i ++) {
        if (i >= n) {
            printf("(!) Sticky sort: map_at iterator too long at %s\n", where);
            map_break(it);
            return -1;
        }

        if (it->len != live[i]->len ||
            memcmp(it->key, live[i]->key, it->len) != 0) {
            printf("(!) Sticky sort: map_at continuation mismatch at %s, index %d\n",
                   where, i);
            map_break(it);
            return -1;
        }

        if (! is_integer(it->val) ||
            variant_to_integer(it->val) != live[i]->value) {
            printf("(!) Sticky sort: map_at value mismatch at %s, index %d\n",
                   where, i);
            map_break(it);
            return -1;
        }
    }

    if (i != n) {
        printf("(!) Sticky sort: map_at iterator too short at %s (%d vs %d)\n",
               where, i, n);
        return -1;
    }

    return 0;
}

static int test_sticky_sort_active_iterator(void)
{
    Map *h = map_alloc(NULL);
    Map_Iterator *it = NULL;
    Variant old;
    const char *tail[] = { "B", "C", "D", "E" };
    const char *fresh[] = { "B", "C", "D", "E", "A" };
    int i = 0;

    if (! h)
        return -1;

    /*
     * Sort by value. Then mutate the first iterator entry so it should move
     * to the end. The active iterator must not be relinked under its feet.
     * A fresh iterator must see the re-sorted order.
     */
    map_set(h, "A", 1, variant_from_integer(1));
    map_set(h, "B", 1, variant_from_integer(2));
    map_set(h, "C", 1, variant_from_integer(3));
    map_set(h, "D", 1, variant_from_integer(4));
    map_set(h, "E", 1, variant_from_integer(5));

    if (map_sort(h, MAP_ASC, _sticky_value_cmp) == -1)
        goto _fail;

    it = map_each(h);
    if (! it)
        goto _fail;

    if (it->len != 1 || memcmp(it->key, "A", 1) != 0)
        goto _fail_iter;

    old = map_set_at(it, variant_from_integer(100));
    if (! is_integer(old) || variant_to_integer(old) != 1)
        goto _fail_iter;

    if (! is_integer(it->val) || variant_to_integer(it->val) != 100)
        goto _fail_iter;

    /*
     * Active iterator should continue in the old traversal order, not jump
     * around after map_set_at().
     */
    it = map_next(it);
    for (i = 0; it; it = map_next(it), i ++) {
        if (i >= 4)
            goto _fail_iter;

        if (it->len != 1 || memcmp(it->key, tail[i], 1) != 0)
            goto _fail_iter;
    }

    if (i != 4)
        goto _fail;

    /*
     * Fresh iterator should lazily re-sort and put A at the end.
     */
    i = 0;
    for (it = map_each(h); it; it = map_next(it), i ++) {
        if (i >= 5)
            goto _fail_iter;

        if (it->len != 1 || memcmp(it->key, fresh[i], 1) != 0)
            goto _fail_iter;
    }

    if (i != 5)
        goto _fail;

    map_free(h);
    return 0;

_fail_iter:
    if (it)
        map_break(it);

_fail:
    map_free(h);
    return -1;
}

static int test_sticky_sort_torture(void)
{
    Map *h = NULL;

    printf("(-) Testing persistent/lazy sticky sorting.\n");

    srand(0x571c4b7);
    _sticky_init_refs();

    h = map_alloc(NULL);
    if (! h) {
        printf("(!) Sticky sort: failed to allocate map\n");
        return -1;
    }

    /*
     * Populate a subset, then install persistent key order.
     */
    for (int i = 0; i < STICKY_KEY_UNIVERSE; i += 2) {
        uint64_t v = (uint64_t) (1000 + i);

        map_set(
            h,
            (const char *) _sticky_ref[i].key,
            _sticky_ref[i].len,
            variant_from_integer(v)
        );

        _sticky_ref[i].alive = 1;
        _sticky_ref[i].value = v;
    }

    if (map_sort(h, MAP_ASC, _sticky_key_cmp) == -1)
        goto _fail;

    if (_sticky_validate_order(h, STICKY_SORT_KEY, 0, "initial key asc") == -1)
        goto _fail;

    /*
     * Batch random insert/set/remove without traversal. The next traversal
     * must lazily sort by the persistent key comparator.
     */
    for (int op = 0; op < 1000; op ++) {
        int idx = rand() % STICKY_KEY_UNIVERSE;
        uint64_t v = ((uint64_t) op << 16) ^ (uint64_t) idx;

        switch (rand() % 4) {
        case 0:
        case 1:
        case 2:
            map_set(
                h,
                (const char *) _sticky_ref[idx].key,
                _sticky_ref[idx].len,
                variant_from_integer(v)
            );
            _sticky_ref[idx].alive = 1;
            _sticky_ref[idx].value = v;
            break;

        case 3:
            map_remove(
                h,
                (const char *) _sticky_ref[idx].key,
                _sticky_ref[idx].len
            );
            _sticky_ref[idx].alive = 0;
            break;
        }
    }

    if (_sticky_validate_from_at(h, STICKY_SORT_KEY, 0, 17, "key asc map_at after batch") == -1)
        goto _fail;

    if (_sticky_validate_order(h, STICKY_SORT_KEY, 0, "key asc after batch") == -1)
        goto _fail;

    /*
     * Switch persistent policy to value-descending. Then mutate values.
     * map_at() should force lazy sorting before returning the iterator.
     */
    if (map_sort(h, MAP_DESC, _sticky_value_cmp) == -1)
        goto _fail;

    if (_sticky_validate_order(h, STICKY_SORT_VALUE, 1, "value desc initial") == -1)
        goto _fail;

    for (int op = 0; op < 1000; op ++) {
        int idx = rand() % STICKY_KEY_UNIVERSE;
        uint64_t v = ((uint64_t) rand() << 32) ^ (uint64_t) op;

        map_set(
            h,
            (const char *) _sticky_ref[idx].key,
            _sticky_ref[idx].len,
            variant_from_integer(v)
        );

        _sticky_ref[idx].alive = 1;
        _sticky_ref[idx].value = v;
    }

    if (_sticky_validate_from_at(h, STICKY_SORT_VALUE, 1, 23, "value desc map_at after updates") == -1)
        goto _fail;

    if (_sticky_validate_order(h, STICKY_SORT_VALUE, 1, "value desc after updates") == -1)
        goto _fail;

    /*
     * One-shot sort should temporarily override traversal order but must not
     * replace the persistent value-desc policy.
     */
    if (map_sort(h, MAP_DESC | MAP_SORT_ONCE, _sticky_key_cmp) == -1)
        goto _fail;

    if (_sticky_validate_order(h, STICKY_SORT_KEY, 1, "one-shot key desc") == -1)
        goto _fail;

    /*
     * Removals do not make the order stale; they should preserve the current
     * one-shot order among remaining entries.
     */
    for (int op = 0; op < 32; op ++) {
        int idx = rand() % STICKY_KEY_UNIVERSE;

        map_remove(
            h,
            (const char *) _sticky_ref[idx].key,
            _sticky_ref[idx].len
        );

        _sticky_ref[idx].alive = 0;
    }

    if (_sticky_validate_order(h, STICKY_SORT_KEY, 1, "one-shot key desc after removals") == -1)
        goto _fail;

    /*
     * A later value update must make the old persistent value-desc policy
     * take effect again on the next traversal.
     */
    for (int i = 0; i < STICKY_KEY_UNIVERSE; i ++) {
        if (!_sticky_ref[i].alive) {
            map_set(
                h,
                (const char *) _sticky_ref[i].key,
                _sticky_ref[i].len,
                variant_from_integer(UINT64_MAX - (uint64_t) i)
            );

            _sticky_ref[i].alive = 1;
            _sticky_ref[i].value = UINT64_MAX - (uint64_t) i;
            break;
        }
    }

    if (_sticky_validate_order(h, STICKY_SORT_VALUE, 1, "persistent value desc restored") == -1)
        goto _fail;

    if (test_sticky_sort_active_iterator() == -1) {
        printf("(!) Sticky sort: active iterator stability failed\n");
        goto _fail;
    }

    map_free(h);
    printf("(*) Sticky sort torture test PASSED!\n");
    return 0;

_fail:
    map_free(h);
    printf("(!) Sticky sort torture test FAILED!\n");
    return -1;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int test_hashtable(void)
{
    Map *h = NULL, *h2 = NULL;
    Map_Iterator *it = NULL;
    uintptr_t i = 0, j = 0;
    Variant val = { 0 };
    int missing = 0;
    size_t len = 0;
    char key[BUFSIZ];
    clock_t start, stop;

    printf("(-) Testing hash table implementation.\n");
    if (! (h = map_alloc(NULL)) ) {
        printf("(!) Allocating hash table: FAILURE\n");
        return -1;
    } else printf("(*) Allocating hash table: SUCCESS\n");

    printf("(*) Insert, delete, insert and read back the same key: ");
    map_insert(h, "test_key", 8, variant_from_integer(86));
    map_remove(h, "test_key", 8);
    map_insert(h, "test_key", 8, variant_from_integer(68));
    val = map_get(h, "test_key", 8);
    if (variant_to_integer(val) == 68) {
        printf("SUCCESS\n");
    } else printf("FAILURE\n");
    map_remove(h, "test_key", 8);

    printf("(*) Inserting key-value pairs.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        map_insert(h, key, len, variant_from_integer(i));
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

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

    printf("(*) Randomly deleting 100k keys.\n");
    while (missing < _CACHE_RNDDL) {
        i = rand() % _CACHE_ITEMS;
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        if (is_integer(map_remove(h, key, len))) missing ++;
    }

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
    map_foreach(h, _print_and_delete_key, NULL);

    map_set(h, "zzzzz", strlen("zzzzz"), variant_from_integer(0x0));
    map_set(h, "tedst", strlen("tedst"), variant_from_integer(0x1));
    map_set(h, "testa", strlen("testa"), variant_from_integer(0x2));
    map_set(h, "btest", strlen("btest"), variant_from_integer(0x4));
    map_set(h, "tcest", strlen("tcest"), variant_from_integer(0x8));
    map_sort(h, MAP_DESC, map_sort_keys);
    map_foreach(h, _print_and_delete_key, NULL);

    map_set(h, "btest", strlen("btest"), variant_from_integer(0x4));
    map_set(h, "tcest", strlen("tcest"), variant_from_integer(0x8));
    map_sort(h, MAP_DESC, map_sort_keys);
    map_foreach(h, _print_and_delete_key, NULL);

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
    map_foreach(h, _print_key_intval, NULL);

    /* map_at test */
    printf("(*) Iterating from H\n");
    for (it = map_at(h, "H", 1); it; it = map_next(it)) {
        printf(
            "Key: %.*s Value: %llu\n",
            (int) it->len,
            it->key,
            variant_to_integer(it->val)
        );
    }

    h = map_free(h);

    if (test_map_return_contracts() == -1) {
        return -1;
    }

    if (test_random_differential_map() == -1) {
        return -1;
    }

    if (test_embedded_nul_keys() == -1) {
        return -1;
    }

    if (test_iterator_concurrency() == -1) {
        return -1;
    }

    if (test_map_remove_at_concurrent() == -1) {
        return -1;
    }

    if (test_write_stress_concurrent() == -1) {
        return -1;
    }

    if (test_writer_starvation() == -1) {
        return -1;
    }

    if (test_sticky_sort_torture() == -1) {
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
