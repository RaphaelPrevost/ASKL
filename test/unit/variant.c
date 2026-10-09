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
 *  data to be ensured and,  more generally, to use and operate in the         *
 *  same conditions as regards security.                                       *
 *                                                                             *
 *  The fact that you are presently reading this means that you have had       *
 *  knowledge of the CeCILL license and that you accept its terms.             *
 *                                                                             *
 ******************************************************************************/

#include "../askl_test.h"
#include "../../lib/askl_variant.h"

#include <signal.h>
#ifndef _WIN32
    #include <sys/wait.h>
#endif

/* -------------------------------------------------------------------------- */
#ifndef _WIN32
/* -------------------------------------------------------------------------- */

static void _caught(int sig)
{
    /* leave through exit() so that the coverage runtime of every object
       writes its counters, which a death by signal would lose */
    exit(128 + sig);
}

/* -------------------------------------------------------------------------- */

static void _misuse(int which)
{
    String *s = NULL;

    switch (which) {
    case 0: variant_to_integer(variant_null()); break;
    case 1: variant_to_decimal(variant_from_integer(1)); break;
    case 2: variant_to_boolean(variant_from_decimal(1.0)); break;
    case 3: s = variant_to_string(variant_true()); break;
    case 4: variant_to_pointer(variant_from_string(s)); break;
    default: break;
    }
}

/* -------------------------------------------------------------------------- */

static int _aborts(int which)
{
    /* the offending call runs in a child of the test process, which must
       reach the abort() behind die() and nothing else */

    pid_t pid = 0;
    int status = 0;

    if ( (pid = fork()) == -1) return 0;

    if (! pid) {
        signal(SIGABRT, _caught);
        _misuse(which);
        _exit(EXIT_SUCCESS);
    }

    if (waitpid(pid, & status, 0) == -1) return 0;

    return (WIFEXITED(status) && WEXITSTATUS(status) == 128 + SIGABRT);
}

/* -------------------------------------------------------------------------- */
#endif

/* -------------------------------------------------------------------------- */

static int _constructors(void)
{
    /* every constructor sets its tag, its value and nothing else */

    int anchor = 0;
    String *s = NULL;
    Variant v = { 0 };

    v = variant_from_pointer(& anchor);
    ASSERT_TRUE(is_pointer(v));
    ASSERT_EQ_PTR(v.value.pointer, & anchor);
    ASSERT_EQ_UINT(v.metadata.fields.dword, 0);

    v = variant_from_integer(UINT64_MAX);
    ASSERT_TRUE(is_integer(v));
    ASSERT_TRUE(v.value.integer == UINT64_MAX);

    v = variant_from_decimal(-2.5);
    ASSERT_TRUE(is_decimal(v));
    ASSERT_TRUE(v.value.decimal == -2.5);

    v = variant_from_boolean(42);
    ASSERT_TRUE(is_boolean(v));
    ASSERT_EQ_UINT(v.value.integer, 1);
    v = variant_from_boolean(0);
    ASSERT_TRUE(is_boolean(v));
    ASSERT_EQ_UINT(v.value.integer, 0);

    ASSERT_NOT_NULL(s = string_alloc("value", 5));
    v = variant_from_string(s);
    ASSERT_TRUE(is_string(v));
    ASSERT_EQ_PTR(v.value.pointer, s);
    s = string_free(s);

    v = variant_null();
    ASSERT_TRUE(is_null(v));
    ASSERT_NULL(v.value.pointer);
    ASSERT_FALSE(is_pointer(v));
    ASSERT_FALSE(_is_object(v));

    v = variant_true();
    ASSERT_TRUE(is_boolean(v));
    ASSERT_EQ_UINT(v.value.integer, 1);

    v = variant_false();
    ASSERT_TRUE(is_boolean(v));
    ASSERT_EQ_UINT(v.value.integer, 0);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _accessors(void)
{
    /* every accessor returns what its constructor was given; the pointer
       accessor also tolerates the null value and object variants */

    int anchor = 0;
    String *s = NULL;
    Variant v = { 0 };

    ASSERT_EQ_PTR(variant_to_pointer(variant_from_pointer(& anchor)), & anchor);
    ASSERT_TRUE(variant_to_integer(variant_from_integer(UINT64_MAX)) ==
                UINT64_MAX);
    ASSERT_EQ_UINT(variant_to_integer(variant_from_integer(0)), 0);
    ASSERT_TRUE(variant_to_decimal(variant_from_decimal(-2.5)) == -2.5);
    ASSERT_EQ_INT(variant_to_boolean(variant_true()), 1);
    ASSERT_EQ_INT(variant_to_boolean(variant_false()), 0);
    ASSERT_EQ_INT(variant_to_boolean(variant_from_boolean(-1)), 1);

    ASSERT_NOT_NULL(s = string_alloc("value", 5));
    ASSERT_EQ_PTR(variant_to_string(variant_from_string(s)), s);
    s = string_free(s);

    ASSERT_NULL(variant_to_pointer(variant_null()));

    v = variant_from_pointer(& anchor);
    v.metadata.fields.type = _VALUE_OBJECT;
    ASSERT_TRUE(_is_object(v));
    ASSERT_EQ_PTR(variant_to_pointer(v), & anchor);

    return 0;
}

/* -------------------------------------------------------------------------- */

static int _equality(void)
{
    /* values compare by type first, then by value; pointers and strings
       by identity, decimals by representation */

    int a = 0, b = 0;
    double zero = 0.0, nan = 0.0;
    String *s = NULL, *t = NULL;
    Variant u = { 0 }, v = { 0 };

    ASSERT_TRUE(variant_equal(variant_null(), variant_null()));
    ASSERT_FALSE(variant_equal(variant_null(), variant_from_integer(0)));
    ASSERT_FALSE(variant_equal(variant_false(), variant_from_integer(0)));

    u = variant_from_integer(7);
    ASSERT_TRUE(variant_equal(u, variant_from_integer(7)));
    ASSERT_FALSE(variant_equal(u, variant_from_integer(8)));

    ASSERT_TRUE(variant_equal(variant_true(), variant_from_boolean(3)));
    ASSERT_FALSE(variant_equal(variant_true(), variant_false()));

    u = variant_from_decimal(1.5);
    ASSERT_TRUE(variant_equal(u, variant_from_decimal(1.5)));
    ASSERT_FALSE(variant_equal(u, variant_from_decimal(2.5)));
    u = variant_from_decimal(zero);
    ASSERT_FALSE(variant_equal(u, variant_from_decimal(-zero)));
    nan = zero / zero;
    u = variant_from_decimal(nan);
    ASSERT_TRUE(variant_equal(u, variant_from_decimal(nan)));

    u = variant_from_pointer(& a);
    ASSERT_TRUE(variant_equal(u, variant_from_pointer(& a)));
    ASSERT_FALSE(variant_equal(u, variant_from_pointer(& b)));

    ASSERT_NOT_NULL(s = string_alloc("same", 4));
    ASSERT_NOT_NULL(t = string_alloc("same", 4));
    u = variant_from_string(s);
    ASSERT_TRUE(variant_equal(u, variant_from_string(s)));
    ASSERT_FALSE(variant_equal(u, variant_from_string(t)));
    s = string_free(s); t = string_free(t);

    u = v = variant_from_pointer(& a);
    u.metadata.fields.type = v.metadata.fields.type = _VALUE_OBJECT;
    ASSERT_TRUE(variant_equal(u, v));
    v.value.pointer = & b;
    ASSERT_FALSE(variant_equal(u, v));

    /* an unknown tag never compares equal, even to itself */
    u.metadata.fields.type = v.metadata.fields.type = 0x7f;
    ASSERT_FALSE(variant_equal(u, u));

    return 0;
}

/* -------------------------------------------------------------------------- */

#ifndef _WIN32
/* -------------------------------------------------------------------------- */

static int _type_errors(void)
{
    /* reading a value as the wrong type is fatal */

    int i = 0;

    for (i = 0; i < 5; i ++) ASSERT_TRUE(_aborts(i));

    return 0;
}

/* -------------------------------------------------------------------------- */
#endif

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    TEST("constructors", _constructors),
    TEST("accessors", _accessors),
    TEST("equality", _equality),
    #ifndef _WIN32
    TEST("type_errors", _type_errors)
    #endif
};

TEST_SUITE(test_suite_variant, "variant", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
