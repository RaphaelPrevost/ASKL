/*******************************************************************************
 *  ASKL.                                                                      *
 *  Copyright (c) 2026 Raphael Prevost <raph@el.bzh>                           *
 *                                                                             *
 *  This software is a computer program whose purpose is to provide a          *
 *  framework for developing and prototyping network services.                 *
 *                                                                             *
 *  This software is governed by the CeCILL  license under French law and      *
 *  abiding by the rules of distribution of free software.  You can  use,      *
 *  modify and/ or redistribute the software under the terms of the CeCILL     *
 *  license as circulated by CEA, CNRS and INRIA at the following URL          *
 *  "http://www.cecill.info".                                                  *
 *                                                                             *
 *  As a counterpart to the access to the source code and  rights to copy,     *
 *  modify and redistribute granted by the license, users are provided only    *
 *  with a limited warranty  and the software's author,  the holder of the     *
 *  economic rights,  and the successive licensors  have only  limited         *
 *  liability.                                                                 *
 *                                                                             *
 *  In this respect, the user's attention is drawn to the risks associated     *
 *  with loading,  using,  modifying and/or developing or reproducing the      *
 *  software by the user in light of its specific status of free software,     *
 *  that may mean  that it is complicated to manipulate,  and  that  also      *
 *  therefore means  that it is reserved for developers  and  experienced      *
 *  professionals having in-depth computer knowledge. Users are therefore      *
 *  encouraged to load and test the software's suitability as regards their    *
 *  requirements in conditions enabling the security of their systems and/or   *
 *  data to be ensured and,  more generally, to use and operate it in the      *
 *  same conditions as regards security.                                       *
 *                                                                             *
 *  The fact that you are presently reading this means that you have had       *
 *  knowledge of the CeCILL license and that you accept its terms.             *
 *                                                                             *
 ******************************************************************************/

#ifndef ASKL_TEST_H

#define ASKL_TEST_H

#include "../lib/askl.h"
#include "../lib/askl_string.h"

/* exit status of a test case that could not run in this environment */
#define TEST_SKIPPED 77

/* test case flags */
#define TEST_FLAG_FAST 0x0
#define TEST_FLAG_SLOW 0x1 /* only run with --slow or ASKL_TEST_SLOW=1 */

typedef struct Test_Case {
    const char *name;
    int (*run)(void);
    unsigned int flags;
    const char *todo;
} Test_Case;

typedef struct Test_Suite {
    const char *name;
    int (*setup)(void);
    void (*teardown)(void);
    const Test_Case *cases;
    unsigned int count;
} Test_Suite;

/**
 * @ingroup test
 * @struct Test_Case
 *
 * A single test. @ref run returns 0 on success, -1 on failure, or
 * @ref TEST_SKIPPED if the test cannot run in the current environment.
 *
 * @ref flags marks the tier the test belongs to (@ref TEST_FLAG_FAST or
 * @ref TEST_FLAG_SLOW). The fast tier is meant to complete in under a minute
 * and runs on every push.
 *
 * @ref todo, if not NULL, marks a known failure: the test still runs and
 * its result is reported with a TAP "TODO" directive, which does not fail
 * the run. The text should name the defect, and the mark should be removed
 * as soon as the defect is fixed.
 *
 * @struct Test_Suite
 *
 * A named group of test cases. @ref setup and @ref teardown, if not NULL,
 * are run before and after every case, in the same process as the case.
 * A suite is registered by adding it to the table in @c test/runner.c.
 */

#define TEST(name, fn)           { name, fn, TEST_FLAG_FAST, NULL }
#define TEST_SLOW(name, fn)      { name, fn, TEST_FLAG_SLOW, NULL }
#define TEST_TODO(name, fn, why) { name, fn, TEST_FLAG_FAST, why }

/* a case that ThreadSanitizer is known to fail on (a race in the code it
   exercises that is pinned in TEST_PROPOSALS.md): a known failure under
   ThreadSanitizer, a regular case in every other build */
#if defined(__SANITIZE_THREAD__)
#define TEST_RACY(name, fn, why) TEST_TODO(name, fn, why)
#elif (defined(__has_feature))
    #if __has_feature(thread_sanitizer)
        #define TEST_RACY(name, fn, why) TEST_TODO(name, fn, why)
    #endif
#endif

#ifndef TEST_RACY
#define TEST_RACY(name, fn, why) TEST(name, fn)
#endif

#define TEST_SUITE(symbol, name, setup, teardown, cases) \
    const Test_Suite symbol = { \
        name, setup, teardown, cases, sizeof(cases) / sizeof(*(cases)) \
    }

/* -------------------------------------------------------------------------- */
/* runtime services, implemented in runner.c                                  */
/* -------------------------------------------------------------------------- */

unsigned int test_seed(void);

/**
 * @ingroup test
 * @fn unsigned int test_seed(void)
 * @return the seed of the current run
 *
 * The seed is printed at the start of every run and can be set with
 * @c -s or @c ASKL_TEST_SEED to replay a failure. The C library
 * generator is seeded with it before each test case.
 */

int test_verbose(void);

void test_fail(const char *file, int line, const char *fmt, ...);

/**
 * @ingroup test
 * @fn void test_fail(const char *file, int line, const char *fmt, ...)
 * @param file location of the failed check
 * @param line location of the failed check
 * @param fmt printf(3) format describing the failure
 *
 * Records a failure and prints it. This is the backend of the assertion
 * macros; a test case should not normally call it directly.
 */

int test_status(void);

/**
 * @ingroup test
 * @fn int test_status(void)
 * @return -1 if a @ref CHECK failed since the test case started, 0 otherwise
 *
 * Return value for test cases written with @ref CHECK instead of the
 * @c ASSERT_* macros: @code return test_status(); @endcode
 */

/* -------------------------------------------------------------------------- */
/* assertions: on failure, report the location and return -1 from the test    */
/* -------------------------------------------------------------------------- */

#define FAIL(msg) do { \
    test_fail(__FILE__, __LINE__, "%s", msg); \
    return -1; \
} while (0)

#define SKIP(reason) do { \
    printf("SKIP: %s\n", reason); \
    return TEST_SKIPPED; \
} while (0)

#define ASSERT_TRUE(cond) do { \
    if (! (cond)) { \
        test_fail(__FILE__, __LINE__, "expected true: %s", #cond); \
        return -1; \
    } \
} while (0)

#define ASSERT_FALSE(cond) do { \
    if (cond) { \
        test_fail(__FILE__, __LINE__, "expected false: %s", #cond); \
        return -1; \
    } \
} while (0)

#define ASSERT_NULL(ptr) do { \
    const void *_p = (ptr); \
    if (_p) { \
        test_fail(__FILE__, __LINE__, "expected NULL: %s is %p", #ptr, _p); \
        return -1; \
    } \
} while (0)

#define ASSERT_NOT_NULL(ptr) do { \
    if (! (ptr)) { \
        test_fail(__FILE__, __LINE__, "unexpected NULL: %s", #ptr); \
        return -1; \
    } \
} while (0)

#define ASSERT_EQ_INT(actual, expected) do { \
    long long _a = (long long) (actual), _e = (long long) (expected); \
    if (_a != _e) { \
        test_fail( \
            __FILE__, __LINE__, "%s: got %lld, expected %lld (%s)", \
            #actual, _a, _e, #expected \
        ); \
        return -1; \
    } \
} while (0)

#define ASSERT_NE_INT(actual, unexpected) do { \
    long long _a = (long long) (actual), _u = (long long) (unexpected); \
    if (_a == _u) { \
        test_fail( \
            __FILE__, __LINE__, "%s: got %lld, expected anything else (%s)", \
            #actual, _a, #unexpected \
        ); \
        return -1; \
    } \
} while (0)

#define ASSERT_EQ_UINT(actual, expected) do { \
    unsigned long long _a = (actual), _e = (expected); \
    if (_a != _e) { \
        test_fail( \
            __FILE__, __LINE__, "%s: got %llu, expected %llu (%s)", \
            #actual, _a, _e, #expected \
        ); \
        return -1; \
    } \
} while (0)

#define ASSERT_EQ_PTR(actual, expected) do { \
    const void *_a = (actual), *_e = (expected); \
    if (_a != _e) { \
        test_fail( \
            __FILE__, __LINE__, "%s: got %p, expected %p (%s)", \
            #actual, _a, _e, #expected \
        ); \
        return -1; \
    } \
} while (0)

#define ASSERT_EQ_MEM(actual, expected, len) do { \
    const void *_a = (actual), *_e = (expected); \
    size_t _l = (len); \
    if (! _a || ! _e || memcmp(_a, _e, _l)) { \
        test_fail( \
            __FILE__, __LINE__, "%s: %zu bytes differ from %s", \
            #actual, _l, #expected \
        ); \
        return -1; \
    } \
} while (0)

/* compare the content of a String with a C string, length included */
#define ASSERT_EQ_STR(string, expected) do { \
    const String *_s = (string); \
    const char *_e = (expected); \
    size_t _l = strlen(_e); \
    if (! _s || ! _s->data || _s->len != _l || memcmp(_s->data, _e, _l)) { \
        test_fail( \
            __FILE__, __LINE__, "%s: got \"%.*s\" (len %u), expected \"%s\"", \
            #string, (_s && _s->data) ? (int) _s->len : 0, \
            (_s && _s->data) ? _s->data : "", _s ? _s->len : 0, _e \
        ); \
        return -1; \
    } \
} while (0)

#define ASSERT_WITHIN(actual, expected, tolerance) do { \
    double _a = (actual), _e = (expected), _t = (tolerance); \
    if ((_a > _e ? _a - _e : _e - _a) > _t) { \
        test_fail( \
            __FILE__, __LINE__, "%s: got %g, expected %g +/- %g", \
            #actual, _a, _e, _t \
        ); \
        return -1; \
    } \
} while (0)

/* -------------------------------------------------------------------------- */
/* non-fatal check: record the failure, keep going, return test_status()      */
/* -------------------------------------------------------------------------- */

#define CHECK(cond) do { \
    if (! (cond)) test_fail(__FILE__, __LINE__, "check failed: %s", #cond); \
} while (0)

#endif
