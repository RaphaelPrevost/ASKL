/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_TRIE
/* -------------------------------------------------------------------------- */

#include "../lib/askl_cbtrie.h"
#include <signal.h>

#define _CACHE_ITEMS   800000
#define _CACHE_KEYFM   "%" PRIuPTR
#define _CACHE_RNDDL    100000

static int timeout = 0;

typedef struct _Node {
    void *child[2];
    uint16_t pos;
    uint8_t val;
    uint8_t bit;
} _Node;

struct _Trie {
    RW_Lock *_lock;
    void *_root;
    void (*_freeval)(Variant);
};

static void _print_tree(void *p, int depth)
{
    if (! p) return;

    if (! ((uintptr_t) p & 0x1)) {
        /* Leaf */
        Trie_Leaf *leaf = (Trie_Leaf *) ((char *) p - offsetof(Trie_Leaf, key));
        printf("%*sLEAF: '%.*s'\n", depth * 2, "", (int) leaf->len, leaf->key);
        return;
    }

    /* Internal node */
    _Node *node = (void *) ((char *) p - 1);
    printf("%*sNODE: pos=%u bit=0x%02x val='%c'(0x%02x)\n",
           depth * 2, "", node->pos, node->bit,
           (node->val >= 32 && node->val < 127) ? node->val : '?', node->val);

    printf("%*s  LEFT (bit=0):\n", depth * 2, "");
    _print_tree(node->child[0], depth + 2);

    printf("%*s  RIGHT (bit=1):\n", depth * 2, "");
    _print_tree(node->child[1], depth + 2);
}

/* -------------------------------------------------------------------------- */

static int callback(const char *element, size_t len, Variant arg)
{
    printf("%.*s [size=%zu] = %" PRIuPTR "\n",
           (int) len, element, len, (uintptr_t) variant_to_integer(arg));
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
    Trie *t = NULL;
    Variant val = { 0 };
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
        trie_insert(t, key, len, variant_from_integer(i));
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

    if (trie_insert(t, key, len, variant_from_integer(i)) != -1) {
        printf("(!) Inserting duplicate key: FAILURE\n");
        return -1;
    } else printf("(*) Inserting duplicate key: SUCCESS\n");

    printf("(*) Getting back values from keys.\n");
    start = clock();
    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i); key[len] = 0;
        val = trie_lookup(t, key, len, NULL);
        if (! is_integer(val) || (j = variant_to_integer(val)) != i) {
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

    printf("(*) Iterator.\n");
    start = clock();
    Trie_Iterator *it = NULL;
    for (it = trie_each_prefix(t, "800", 3); it; it = trie_next(it))
        callback(it->key, it->len, it->val);
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.8f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");

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
        trie_update(t, key, len, variant_from_integer(i + 1));
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
            if (variant_to_integer(val) != i + 1)
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
        if (! is_integer(val) || variant_to_integer(val) != i + 1)
            missing ++;
        else printf("(!) found phantom key %" PRIuPTR " !\n", i);
    }
    stop = clock();
    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s\n");
    printf("(-) %i missing keys\n", missing);

    printf("(*) Overwriting a key.\n");
    if (trie_insert(t, key, len, variant_from_integer(0x888)) == -1)
        printf("(!) Key insertion failed\n");
    val = trie_update(t, key, len, variant_from_integer(0x8989));
    if (variant_to_integer(val) != 0x888)
        printf("(!) Key overwrite returned 0x%" PRIxPTR  "\n", i);
    if (variant_to_integer(trie_remove(t, key, len)) != 0x8989)
        printf("(!) Retrieved key is 0x%" PRIxPTR  ", expected 0x8989.\n", i);
    else
        printf("(*) Value was successfully overwritten.\n");

    trie_free(t);

    t = trie_alloc(NULL);

    printf("(*) Testing batch insert.\n");

    /* First, insert a few regular keys to establish tree structure */
    const char *regular_keys[] = {
        "zero",
        "user/alice/name",
        "user/alice/age",
        "user/bob/name",
        "user/bob/age",
        "bob/secure",
        "bus",
        "bubble",
        "baz",
        "beef",
        "bob/secret",
        "bob/secrets",
        "bob/second",
        "bob/sales",
        "alpha",
        "alice/secret",
        "armada",
        "alma",
        "aztec",
        "bot/enabled",
        "config/server/host",
        "config/server/port",
        "conf",
        "can",
        "cool",
        "comb",
        "config/client/timeout"
    };

    for (i = 0; i < sizeof(regular_keys) / sizeof(regular_keys[0]); i++) {
        len = strlen(regular_keys[i]);
        if (trie_insert(t, regular_keys[i], len, variant_from_integer(100 + i)) == -1) {
            printf("(!) Regular insert failed for key: %s\n", regular_keys[i]);
            return -1;
        }
    }
    printf("(*) Inserted %zu regular keys\n", sizeof(regular_keys) / sizeof(regular_keys[0]));

    printf("(*) Tree structure after regular inserts:\n");
    _print_tree(t->_root, 0);

    /* Print entire trie to see structure */
    printf("(*) All keys in trie:\n");
    for (it = trie_each(t); it; it = trie_next(it)) {
        callback(it->key, it->len, it->val);
    }

    /* Now prepare a batch with common prefix "features/0/properties/" */
    const char *prefix = "features/0/properties/";
    size_t prefix_len = strlen(prefix);

    #define BATCH_SIZE 3
    Trie_Leaf **batch = malloc(BATCH_SIZE * sizeof(Trie_Leaf *));
    if (! batch) {
        printf("(!) Failed to allocate batch array\n");
        return -1;
    }

    /* Build batch leaves manually */
    const char *suffixes[] = {"STREET", "MAPBLKLOT", "BLOCK_NUM"};
    uintptr_t values[] = {1001, 1002, 1003};

    for (i = 0; i < BATCH_SIZE; i++) {
        size_t suffix_len = strlen(suffixes[i]);
        size_t full_len = prefix_len + suffix_len;

        /* Allocate leaf (same as _insert expects) */
        batch[i] = malloc(sizeof(Trie_Leaf) + full_len + 1);
        if (! batch[i]) {
            printf("(!) Failed to allocate batch leaf %zu\n", i);
            /* Free previously allocated leaves */
            for (j = 0; j < i; j++) free(batch[j]);
            free(batch);
            return -1;
        }

        /* Construct full key */
        memcpy(batch[i]->key, prefix, prefix_len);
        memcpy(batch[i]->key + prefix_len, suffixes[i], suffix_len);
        batch[i]->key[full_len] = '\0';
        batch[i]->len = full_len;
        batch[i]->val = variant_from_integer(values[i]);
    }

    printf("(*) Prepared batch of %d leaves with prefix: %s\n", BATCH_SIZE, prefix);

    /* Insert the batch */
    start = clock();
    int batch_result = trie_insert_prefix_list(t, prefix_len, batch, BATCH_SIZE);
    stop = clock();

    if (batch_result == -1) {
        printf("(!) Batch insert reported errors\n");
    } else {
        printf("(*) Batch insert: SUCCESS\n");
    }

    printf("(-) Batch insert time = %.8f s\n",
           (double)(stop - start) / CLOCKS_PER_SEC);

    /* Free the batch array (leaves are now owned by trie or were freed) */
    free(batch);

    /* Verify batch keys were inserted */
    printf("(*) Verifying batch-inserted keys:\n");
    missing = 0;
    for (i = 0; i < BATCH_SIZE; i++) {
        char full_key[256];
        len = snprintf(full_key, sizeof(full_key), "%s%s", prefix, suffixes[i]);
        val = trie_lookup(t, full_key, len, NULL);

        if (! is_integer(val)) {
            printf("(!) Key '%s' not found\n", full_key);
            missing++;
        } else {
            uintptr_t found_val = variant_to_integer(val);
            if (found_val != values[i]) {
                printf("(!) Key '%s' has wrong value: %" PRIuPTR " (expected %" PRIuPTR ")\n",
                       full_key, found_val, values[i]);
                missing++;
            } else {
                printf("    '%s' = %" PRIuPTR " ✓\n", full_key, found_val);
            }
        }
    }

    if (missing > 0) {
        printf("(!) %d batch keys missing or incorrect\n", missing);
        return -1;
    } else {
        printf("(*) All batch keys verified: SUCCESS\n");
    }

    /* Print all keys with the batch prefix */
    printf("(*) All keys under prefix '%s':\n", prefix);
    Trie_Iterator *batch_it = NULL;
    int count = 0;
    for (batch_it = trie_each_prefix(t, prefix, prefix_len);
         batch_it;
         batch_it = trie_next(batch_it)) {
        callback(batch_it->key, batch_it->len, batch_it->val);
        count++;
    }
    printf("(-) Found %d keys under prefix\n", count);

    /* Print entire trie to see structure */
    printf("(*) All keys in trie:\n");
    for (it = trie_each(t); it; it = trie_next(it)) {
        callback(it->key, it->len, it->val);
    }

    printf("(*) Batch insert test: SUCCESS\n");

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
