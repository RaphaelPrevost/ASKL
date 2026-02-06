#include "../lib/askl_server.h"
#include "../lib/askl_steque.h"

#define ITEMS 800000

#define enqueue(i) do { queue_enqueue(q, (void *) (uintptr_t) (i)); } while (0)
#define dequeue() ((uintptr_t) queue_pop(q))

static Queue *q = NULL;

static void *_enqueue(UNUSED void *dummy)
{
    unsigned int i = 0;

    for (i = 1; i < ITEMS; i ++) queue_enqueue(q, (void *) (uintptr_t) i);

    return NULL;
}

static void *_dequeue(UNUSED void *dummy)
{
    unsigned int i = 0;

    for (i = 1; i < ITEMS; i ++) {
        if (queue_pop(q) != (void *) (uintptr_t) i) {
            i --; queue_wait(q, 1000);
        }
    }

    return NULL;
}

static void *_push(UNUSED void *dummy)
{
    unsigned int i = 0;

    for (i = 1; i < ITEMS; i ++) queue_push(q, (void *) (uintptr_t) i);

    return NULL;
}

static void *_pop(UNUSED void *dummy)
{
    unsigned int i = 0;

    for (i = 1; i < ITEMS; i ++) {
        if (! queue_pop(q)) {
            i --; queue_wait(q, 1000);
        }
    }

    return NULL;
}

int test_queue(void)
{
    pthread_t enq, deq;
    uintptr_t ret = 0;
    clock_t start, stop;

    if ( (q = queue_alloc()) == NULL) return -1;

    printf("(-) Concurrent enqueue and dequeue ("STR(ITEMS)" items).\n");

    start = clock();
    pthread_create(& enq, NULL, _enqueue, NULL);
    pthread_create(& deq, NULL, _dequeue, NULL);
    pthread_join(enq, NULL); pthread_join(deq, NULL);
    stop = clock();

    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s.\n");

    if (queue_empty(q)) printf("(*) Queue empty.\n");
    else {
        printf("(!) Failed to dequeue all the items !\n");
        return -1;
    }

    queue_push(q, (void *) 0x8888);
    ret = (uintptr_t) queue_pop(q);
    if (ret != 0x8888) {
        printf("(!) Push/Pop: missing value !\n");
        return -1;
    }
    enqueue(0x9999);
    ret = dequeue();
    if (ret != 0x9999) {
        printf("(!) Enqueue/Dequeue using macros: missing value !\n");
        return -1;
    }

    printf("(-) Concurrent push and pop ("STR(ITEMS)" items).\n");

    start = clock();
    pthread_create(& enq, NULL, _push, NULL);
    pthread_create(& deq, NULL, _pop, NULL);
    pthread_join(enq, NULL); pthread_join(deq, NULL);
    stop = clock();

    printf("(-) Time elapsed = ");
    printf("%.3f", (double)( stop - start ) / CLOCKS_PER_SEC);
    printf(" s.\n");

    if (queue_empty(q)) printf("(*) Queue empty.\n");
    else {
        printf("(!) Failed to dequeue all the items !\n");
        return -1;
    }

    q = queue_free(q);

    return 0;
}
