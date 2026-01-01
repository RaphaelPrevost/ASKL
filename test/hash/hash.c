#include "../../lib/askl_htable.h"

#define _CACHE_ITEMS 1000000
#define _CACHE_RNDDL 100000
#define _CACHE_THRNG 400000

#define _CACHE_KEYFM "%" PRIuPTR

int main(UNUSED int argc, UNUSED char **argv)
{
    ASKL_LinkedMap *h = NULL;
    uintptr_t i = 0, j = 0;
    variant val = { 0 };
    int missing = 0;
    size_t len = 0;
    char key[BUFSIZ];

    if (! (h = map_alloc(NULL)) ) {
        printf("(!) Allocating hash table: FAILURE\n");
        return -1;
    }

    for (i = 1; i <= _CACHE_ITEMS; i ++) {
        len = snprintf(key, sizeof(key), _CACHE_KEYFM, i);
        map_set(h, key, len, variant_from_integer(i));
    }

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

    h = map_free(h);

    exit(EXIT_SUCCESS);
}
