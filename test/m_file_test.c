/* -------------------------------------------------------------------------- */
#ifdef _ENABLE_FILE
/* -------------------------------------------------------------------------- */

#include "../lib/askl_server.h"
#include "../lib/m_file.h"

/* thread start control switch */
static pthread_mutex_t mx_switch = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cd_switch = PTHREAD_COND_INITIALIZER;
static int start_switch = 0;

static m_view *view = NULL;

/* -------------------------------------------------------------------------- */

static void *threadA(UNUSED void *dummy)
{
    m_file *orig = NULL;

    /* wait for it... */
    pthread_mutex_lock(& mx_switch);
        while (! start_switch) pthread_cond_wait(& cd_switch, & mx_switch);
    pthread_mutex_unlock(& mx_switch);

    /* open the file */
    if (! (orig = fs_openfile(view, "resource", strlen("resource"), NULL)) ) {
        printf("(!) Opening a virtual file: FAILURE\n");
        pthread_exit(NULL);
    } else printf("(*) Opening a virtual file: SUCCESS\n");

    /* wait for threadB remapping */
    sleep(2);

    printf("(*) Opened file %.*s\n", (int) orig->pathlen, orig->path);
    printf("(*) Thread A got: %s\n", DATA(orig->data));

    orig = fs_closefile(orig);

    pthread_exit(NULL);
}

/* -------------------------------------------------------------------------- */

static void *threadB(UNUSED void *dummy)
{
    m_file *file = NULL;
    m_string *test = NULL;

    if (! (test = string_alloc("new_test", sizeof("new_test"))) )
        pthread_exit(NULL);

    /* wait for it... */
    pthread_mutex_lock(& mx_switch);
        while (! start_switch) pthread_cond_wait(& cd_switch, & mx_switch);
    pthread_mutex_unlock(& mx_switch);

    sleep(1);

    /* remap a new resource */
    if (fs_remap(view, "resource", strlen("resource"), test) == -1) {
        printf("(!) Remapping: FAILURE\n");
        pthread_exit(NULL);
    } else printf("(*) Remapping: SUCCESS\n");

    /* open the remapped file */
    if (! (file = fs_openfile(view, "resource", strlen("resource"), NULL)) ) {
        printf("(!) Opening a remapped virtual file: FAILURE\n");
        pthread_exit(NULL);
    } else printf("(*) Opening a remapped virtual file: SUCCESS\n");

    printf("(*) Opened file %.*s\n", (int) file->pathlen, file->path);
    printf("(*) Thread B got: %s\n", DATA(file->data));

    file = fs_closefile(file);

    pthread_exit(NULL);
}

/* -------------------------------------------------------------------------- */

int test_fs(void)
{
    pthread_t a, b;
    m_string *s = NULL;
    char buffer[BUFSIZ];
    m_view *v = NULL;

    printf("(-) Testing filesystem cache implementation.\n");

    if (fs_api_setup() == -1) {
        printf("(!) File API setup: FAILURE\n");
        return -1;
    } else printf("(*) File API setup: SUCCESS\n");

    if (! (s = string_alloc("test_data1", sizeof("test_data1"))) )
        return -1;

    if (! (view = fs_openview("/virtual", strlen("/virtual"))) ) {
        printf("(!) Opening a virtual view: FAILURE\n");
        return -1;
    } else printf("(*) Opening a virtual view: SUCCESS\n");

    if (fs_remap(view, "resource", strlen("resource"), s) == -1) {
        printf("(!) Mapping a resource to a virtual file: FAILURE\n");
        return -1;
    } else printf("(*) Mapping a resource to a virtual file: SUCCESS\n");

    if (fs_map(view, "resource", strlen("resource"), s) != -1) {
        printf("(!) Mapping over an existing mapping should fail: FAILURE\n");
        return -1;
    } else printf("(*) Mapping over an existing mapping should fail: SUCCESS\n");

    if (fs_map(view, "../test", strlen("../test"), s) == 0) {
        printf("(!) Prevent bogus relative path mapping: FAILURE\n");
        return -1;
    } else printf("(*) Prevent bogus relative path mapping: SUCCESS\n");

    if (pthread_create(& a, NULL, threadA, NULL) == -1) {
        perror(ERR(test_fs, pthread_create));
        return -1;
    }

    if (pthread_create(& b, NULL, threadB, NULL) == -1) {
        perror(ERR(test_fs, pthread_create));
        return -1;
    }

    /* start the threads */
    pthread_mutex_lock(& mx_switch);
    start_switch = 1;
    pthread_cond_broadcast(& cd_switch);
    pthread_mutex_unlock(& mx_switch);

    pthread_join(a, NULL); pthread_join(b, NULL);

    printf("(*) Closing the view.\n");
    view = fs_closeview(view);

    fs_getpath("/home/raphael/test/file/file.txt",
               strlen("/home/raphael/test/file/file.txt"),
               buffer, sizeof(buffer));
    printf("(*) fs_getpath: %s\n", buffer);

    fs_getfilename("/home/raphael/test/file/file.txt",
                   strlen("/home/raphael/test/file/file.txt"),
                   buffer, sizeof(buffer));
    printf("(*) fs_getfilename: %s\n", buffer);

    fs_getpath("filename.txt",
               strlen("filename.txt"),
               buffer, sizeof(buffer));
    printf("(*) fs_getpath: %s\n", buffer);

    fs_getfilename("filename.txt",
                   strlen("filename.txt"),
                   buffer, sizeof(buffer));
    printf("(*) fs_getfilename: %s\n", buffer);

    fs_mkdir(NULL, "/home/jaguarwan/prout/lapin/machin", strlen("/home/jaguarwan/prout/lapin/machin"));

    fs_mkdir(NULL, "/home/jaguarwan/prout/lapin/machin/henry", strlen("/home/jaguarwan/prout/lapin/machin/henry"));

    v = fs_openview("/home/jaguarwan", strlen("/home/jaguarwan"));
    fs_mkdir(v, "/prout/lapin/machin/henry/caca", strlen("/prout/lapin/machin/henry/caca"));
    v = fs_closeview(v);

    fs_api_cleanup();

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
