#include "../lib/m_server.h"
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

static m_hashtable *v;

/* -------------------------------------------------------------------------- */

typedef struct _item {
    size_t len;
    char *key;
    char *val;
} _item;

/* -------------------------------------------------------------------------- */

static unsigned long __hash(_item *i, uint32_t initval)
{
    /* Bob Jenkins' lookup3 hash function, we use it for the bucket index */
    uint32_t a, b, c;                /* internal state */
    union { const void *ptr; size_t i; } u;   /* needed for Mac Powerbook G4 */

    const char *data = NULL;
    size_t len = 0;

    data = i->key;
    len = i->len;

    #if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
         __BYTE_ORDER == __LITTLE_ENDIAN) || \
        (defined(i386) || defined(__i386__) || defined(__i486__) || \
         defined(__i586__) || defined(__i686__) || defined(__x86_64__) || \
         defined(vax) || defined(MIPSEL))
        #define HASH_LITTLE_ENDIAN 1
        #define HASH_BIG_ENDIAN 0
    #elif (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && \
           __BYTE_ORDER == __BIG_ENDIAN) || \
          (defined(sparc) || defined(POWERPC) || defined(mc68000) || \
           defined(sel))
        #define HASH_LITTLE_ENDIAN 0
        #define HASH_BIG_ENDIAN 1
    #else
        #define HASH_LITTLE_ENDIAN 0
        #define HASH_BIG_ENDIAN 0
    #endif

    #define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

    #define mix(a, b, c) \
    { \
        a -= c; a ^= rot(c, 4); c += b; \
        b -= a; b ^= rot(a, 6); a += c; \
        c -= b; c ^= rot(b, 8); b += a; \
        a -= c; a ^= rot(c,16); c += b; \
        b -= a; b ^= rot(a,19); a += c; \
        c -= b; c ^= rot(b, 4); b += a; \
    }

    #define final(a, b, c) \
    { \
        c ^= b; c -= rot(b,14); \
        a ^= c; a -= rot(c,11); \
        b ^= a; b -= rot(a,25); \
        c ^= b; c -= rot(b,16); \
        a ^= c; a -= rot(c,4);  \
        b ^= a; b -= rot(a,14); \
        c ^= b; c -= rot(b,24); \
    }

    /* set up the internal state */
    a = b = c = 0x52661314 + ((uint32_t) len) + initval;

    u.ptr = data;

    if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
        const uint32_t *k = (const uint32_t *) data; /* read 32-bit chunks */
        #ifdef DEBUG
        const uint8_t *k8;
        #endif

        /* all but last block: aligned reads and affect 32 bits of (a, b, c) */
        while (len > 12) {
            a += k[0]; b += k[1]; c += k[2];
            mix(a, b, c);
            len -= 12; k += 3;
        }

        /* - handle the last (probably partial) block
         *
         * "k[2]&0xffffff" actually reads beyond the end of the string, but
         * then masks off the part it's not allowed to read.  Because the
         * string is aligned, the masked-off tail is in the same word as the
         * rest of the string.  Every machine with memory protection I've seen
         * does it on word boundaries, so is OK with this.  But VALGRIND will
         * still catch it and complain.  The masking trick does make the hash
         * noticably faster for short strings (like English words).
        */
        #ifndef DEBUG

        switch(len) {
            case 12: c += k[2]; b += k[1]; a += k[0]; break;
            case 11: c += k[2] & 0xffffff; b += k[1]; a += k[0]; break;
            case 10: c += k[2] & 0xffff; b += k[1]; a += k[0]; break;
            case 9 : c += k[2] & 0xff; b += k[1]; a += k[0]; break;
            case 8 : b += k[1]; a += k[0]; break;
            case 7 : b += k[1] & 0xffffff; a += k[0]; break;
            case 6 : b += k[1] & 0xffff; a += k[0]; break;
            case 5 : b += k[1] & 0xff; a += k[0]; break;
            case 4 : a += k[0]; break;
            case 3 : a += k[0] & 0xffffff; break;
            case 2 : a += k[0] & 0xffff; break;
            case 1 : a += k[0] & 0xff; break;
            case 0 : return c; /* zero length strings require no mixing */
        }

        #else /* make valgrind happy */

        k8 = (const uint8_t *) k;
        switch(len) {
            case 12: c += k[2]; b += k[1]; a += k[0]; break;
            case 11: c += ((uint32_t) k8[10]) << 16;  /* fall through */
            case 10: c += ((uint32_t) k8[9]) << 8;    /* fall through */
            case 9 : c += k8[8];                      /* fall through */
            case 8 : b += k[1]; a += k[0]; break;
            case 7 : b += ((uint32_t) k8[6]) << 16;   /* fall through */
            case 6 : b += ((uint32_t) k8[5]) << 8;    /* fall through */
            case 5 : b += k8[4];                      /* fall through */
            case 4 : a += k[0]; break;
            case 3 : a += ((uint32_t) k8[2]) << 16;   /* fall through */
            case 2 : a += ((uint32_t) k8[1]) << 8;    /* fall through */
            case 1 : a += k8[0]; break;
            case 0 : return c;
        }

        #endif /* !valgrind */

    } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
        const uint16_t *k = (const uint16_t *) data; /* read 16-bit chunks */
        const uint8_t *k8;

        /* - all but last block: aligned reads and different mixing */
        while (len > 12) {
            a += k[0] + (((uint32_t) k[1]) << 16);
            b += k[2] + (((uint32_t) k[3]) << 16);
            c += k[4] + (((uint32_t) k[5]) << 16);
            mix(a, b, c);
            len -= 12; k += 6;
        }

        /* - handle the last (probably partial) block */
        k8 = (const uint8_t *) k;
        switch(len) {
            case 12: c += k[4] + (((uint32_t) k[5]) << 16);
                     b += k[2] + (((uint32_t) k[3]) << 16);
                     a += k[0] + (((uint32_t) k[1]) << 16);
                     break;
            case 11: c += ((uint32_t) k8[10]) << 16;     /* fall through */
            case 10: c += k[4];
                     b += k[2] + (((uint32_t) k[3]) << 16);
                     a += k[0] + (((uint32_t) k[1]) << 16);
                     break;
            case 9 : c += k8[8];                         /* fall through */
            case 8 : b += k[2] + (((uint32_t) k[3]) << 16);
                     a += k[0] + (((uint32_t) k[1]) << 16);
                     break;
            case 7 : b += ((uint32_t) k8[6]) << 16;      /* fall through */
            case 6 : b += k[2];
                     a += k[0]+(((uint32_t) k[1]) << 16);
                     break;
            case 5 : b += k8[4];                         /* fall through */
            case 4 : a += k[0] + (((uint32_t) k[1]) << 16);
                     break;
            case 3 : a += ((uint32_t) k8[2]) << 16;      /* fall through */
            case 2 : a += k[0];
                     break;
            case 1 : a += k8[0];
                     break;
            case 0 : return c; /* zero length requires no mixing */
        }

    } else if (HASH_BIG_ENDIAN && ((u.i & 0x3) == 0)) {
        const uint32_t *k = (const uint32_t *) data; /* read 32-bit chunks */
        #ifdef DEBUG
        const uint8_t *k8;
        #endif

        /* all but last block: aligned reads and affect 32 bits of (a, b, c) */
        while (len > 12) {
            a += k[0]; b += k[1]; c += k[2];
            mix(a, b, c);
            len -= 12; k += 3;
        }

        /* - handle the last (probably partial) block */
        #ifndef DEBUG

        switch(len) {
            case 12: c += k[2]; b += k[1]; a += k[0]; break;
            case 11: c += k[2] & 0xffffff00; b += k[1]; a += k[0]; break;
            case 10: c += k[2] & 0xffff0000; b += k[1]; a += k[0]; break;
            case 9 : c += k[2] & 0xff000000; b += k[1]; a += k[0]; break;
            case 8 : b += k[1]; a += k[0]; break;
            case 7 : b += k[1] & 0xffffff00; a += k[0]; break;
            case 6 : b += k[1] & 0xffff0000; a += k[0]; break;
            case 5 : b += k[1] & 0xff000000; a += k[0]; break;
            case 4 : a += k[0]; break;
            case 3 : a += k[0] & 0xffffff00; break;
            case 2 : a += k[0] & 0xffff0000; break;
            case 1 : a += k[0] & 0xff000000; break;
            case 0 : return c; /* zero length strings require no mixing */
        }

        #else  /* make valgrind happy */

        k8 = (const uint8_t *) k;
        switch(len) {
            case 12: c += k[2]; b += k[1]; a += k[0]; break;
            case 11: c += ((uint32_t) k8[10]) << 8;  /* fall through */
            case 10: c += ((uint32_t) k8[9]) << 16;  /* fall through */
            case 9 : c += ((uint32_t) k8[8]) << 24;  /* fall through */
            case 8 : b += k[1]; a += k[0]; break;
            case 7 : b += ((uint32_t) k8[6]) << 8;   /* fall through */
            case 6 : b += ((uint32_t) k8[5]) << 16;  /* fall through */
            case 5 : b += ((uint32_t) k8[4]) << 24;  /* fall through */
            case 4 : a += k[0]; break;
            case 3 : a += ((uint32_t) k8[2]) << 8;   /* fall through */
            case 2 : a += ((uint32_t) k8[1]) << 16;  /* fall through */
            case 1 : a += ((uint32_t) k8[0]) << 24; break;
            case 0 : return c;
        }

        #endif /* !VALGRIND */

    } else { /* need to read the key one byte at a time */
        const uint8_t *k = (const uint8_t *) data;

        /* all but the last block: affect some 32 bits of (a, b, c) */
        while (len > 12) {
            a += k[0];
            a += ((uint32_t) k[1]) << 8;
            a += ((uint32_t) k[2]) << 16;
            a += ((uint32_t) k[3]) << 24;
            b += k[4];
            b += ((uint32_t) k[5]) << 8;
            b += ((uint32_t) k[6]) << 16;
            b += ((uint32_t) k[7]) << 24;
            c += k[8];
            c += ((uint32_t) k[9]) << 8;
            c += ((uint32_t) k[10]) << 16;
            c += ((uint32_t) k[11]) << 24;
            mix(a, b, c);
            len -= 12; k += 12;
        }

        /* - last block: affect all 32 bits of (c) */
        switch(len) {
            case 12: c += ((uint32_t) k[11]) << 24;
            case 11: c += ((uint32_t) k[10]) << 16;
            case 10: c += ((uint32_t) k[9]) << 8;
            case 9 : c += k[8];
            case 8 : b += ((uint32_t) k[7]) << 24;
            case 7 : b += ((uint32_t) k[6]) << 16;
            case 6 : b += ((uint32_t) k[5]) << 8;
            case 5 : b += k[4];
            case 4 : a += ((uint32_t) k[3]) << 24;
            case 3 : a += ((uint32_t) k[2]) << 16;
            case 2 : a += ((uint32_t) k[1]) << 8;
            case 1 : a += k[0]; break;
            case 0 : return c;
        }
    }

    final(a, b, c);

    return c;
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
        hashtable_insert(v, key, len, (void *) i);
    }

    pthread_exit(NULL);
}

/* -------------------------------------------------------------------------- */

static void *_read_loop(void *range)
{
    uintptr_t i = 0, j = 0;
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
        if ( (j = (uintptr_t) hashtable_find(v, key, len)) != i) {
            missing ++;
            printf("(!) Key %" PRIuPTR " is missing ! (found %" PRIuPTR " instead)\n", i, j);
        }
    }

    printf("(-) %i missing keys\n", missing);

    pthread_exit(NULL);
}

/* -------------------------------------------------------------------------- */

static int _print_and_delete_key(const char *key, size_t len, UNUSED void *val)
{
    printf("%.*s\n", (int) len, key);
    return -1;
}

/* -------------------------------------------------------------------------- */

int test_hashtable(void)
{
    m_cache *h = NULL;
    uintptr_t i = 0, j = 0;
    int missing = 0;
    size_t len = 0;
    char key[BUFSIZ];
    clock_t start, stop;
    hashtable *x = NULL;
    _item *z = NULL, tmp;

    signal(SIGALRM, _timeout);

    printf("(-) Testing hash table implementation.\n");
    if (! (h = cache_alloc(NULL)) ) {
        printf("(!) Allocating hash table: FAILURE\n");
        return -1;
    } else printf("(*) Allocating hash table: SUCCESS\n");

    printf("(*) Inserting key-value pairs.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        cache_push(h, key, len, (void *) i);
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

    printf("(-) Size of the hashtable: %zu items/%zu buckets\n",
            h->_bucket_count, h->_bucket_size);
    printf("(-) Memory footprint: %zu bytes.\n", cache_footprint(h, & len));
    printf("(-) Overhead: %zu bytes (%zu KiB).\n", len, len / 1024);

    printf("(*) Getting back values from keys.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        if ( (j = (uintptr_t) cache_find(h, key, len)) != i) {
            missing ++;
            printf("(!) Key %" PRIuPTR " is missing ! (found %" PRIuPTR " instead)\n", i, j);
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
        if (cache_pop(h, key, len)) missing ++;
    }
    timeout = 1;

    /* sorting */
    printf("(*) Sorting.\n");
    start = clock();
    cache_sort(h, CACHE_ASC, cache_sort_keys);
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

    missing = 0;

    printf("(*) Getting back values from keys.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        if ( (j = (uintptr_t) cache_find(h, key, len)) != i)
            missing ++;
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
        cache_push(h, key, len, (void *) (i + 1) );
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
        if ((uintptr_t) cache_pop(h, key, len) != i + 1) missing ++;
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
        if ((uintptr_t) cache_find(h, key, len) != i + 1) missing ++;
        else printf("(!) found phantom key %" PRIuPTR " !\n", i);
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    printf("(*) Overwriting a key.\n");
    i = (uintptr_t) cache_push(h, key, len, (void *) 0xc0ffee);
    if (i) printf("(!) Key insertion returned 0x%" PRIxPTR "\n", i);
    i = (uintptr_t) cache_push(h, key, len, (void *) 0xcafe);
    if (i != 0xc0ffee) printf("(!) Key overwrite returned 0x%" PRIxPTR "\n", i);
    if ( (i = (uintptr_t) cache_pop(h, key, len)) != 0xcafe)
        printf("(!) Retrieved key is 0x%" PRIxPTR ", expected 0xCAFE.\n", i);
    else
        printf("(*) Value was successfully overwritten.\n");

    /* sort test */
    cache_push(h, "zzzzz", strlen("zzzzz"), (void *) 0x0);
    cache_push(h, "tedst", strlen("tedst"), (void *) 0x1);
    cache_push(h, "testa", strlen("testa"), (void *) 0x2);
    cache_push(h, "btest", strlen("btest"), (void *) 0x4);
    cache_push(h, "tcest", strlen("tcest"), (void *) 0x8);
    cache_sort(h, CACHE_ASC, cache_sort_keys);
    cache_foreach(h, _print_and_delete_key);

    cache_push(h, "zzzzz", strlen("zzzzz"), (void *) 0x0);
    cache_push(h, "tedst", strlen("tedst"), (void *) 0x1);
    cache_push(h, "testa", strlen("testa"), (void *) 0x2);
    cache_push(h, "btest", strlen("btest"), (void *) 0x4);
    cache_push(h, "tcest", strlen("tcest"), (void *) 0x8);
    cache_sort(h, CACHE_DESC, cache_sort_keys);
    cache_foreach(h, _print_and_delete_key);

    cache_push(h, "btest", strlen("btest"), (void *) 0x4);
    cache_push(h, "tcest", strlen("tcest"), (void *) 0x8);
    cache_sort(h, CACHE_DESC, cache_sort_keys);
    cache_foreach(h, _print_and_delete_key);

    h = cache_free(h);

    printf("(-) Testing hash table implementation.\n");
    if (! (v = hashtable_alloc(NULL)) ) {
        printf("(!) Allocating hash table: FAILURE\n");
        return -1;
    } else printf("(*) Allocating hash table: SUCCESS\n");

    /* spawn the worker threads */
    for (i = 0; i < _CACHE_CONCURRENCY; i ++) {
        if (pthread_create(& _thread[i], NULL,
                           _insert_loop,
                           (void *) (uintptr_t) (i * _CACHE_THRNG)) == -1) {
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

    printf("(-) Memory footprint: %zu bytes.\n", hashtable_footprint(v, & len));
    printf("(-) Overhead: %zu bytes (%zu KiB).\n", len, len / 1024);

    /* spawn the worker threads */
    for (i = 0; i < _CACHE_CONCURRENCY; i ++) {
        if (pthread_create(& _thread[i], NULL,
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

    v = hashtable_free(v);

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
