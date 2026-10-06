# ASKL unit tests

The tests are built into `test.out` and driven by the harness in
`test/runner.c`. Every test case runs in its own process, under a timeout,
and the results are printed as [TAP](https://testanything.org/).

## Running

```
make check                       # fast tier, the default for CI
make check TEST=string           # one suite
make check TEST=codecs.b64_decode
make check TEST="string json"    # several suites or cases
make check-slow                  # fast and slow tiers
make check-asan                  # AddressSanitizer + UndefinedBehaviorSanitizer (UB is fatal)
make check-tsan                  # ThreadSanitizer
make check-noatomics             # mutex-only lock and queue (no atomics)
make check-coverage              # line coverage report in coverage/index.html
make test                        # run in gdb (or lldb), without isolation
```

Run `make clean` before switching between the `check-*` targets: the object
files are shared.

The binary can also be run directly:

```
LD_LIBRARY_PATH=. ./test.out [options] [suite | suite.case ...]
  -l          list the selected tests and exit
  -v          always show the output of the tests
  -s SEED     seed for the random generator (ASKL_TEST_SEED)
  -t SECONDS  timeout per test (ASKL_TEST_TIMEOUT)
  --slow      also run the slow tier (ASKL_TEST_SLOW=1)
  --no-fork   run in this process, without timeout (ASKL_TEST_NOFORK=1)
```

The seed is printed at the start of every run and set before every test
case; a failure in a randomized test can be replayed by passing the seed
back with `-s`.

## Reading the output

```
ok 3 - string.resize
not ok 6 - string.fetch
#   string.fetch: exit status 1
#   test/unit/string.c:254: string_fetch_buffer(z, out, 1): got 0, expected -1
ok 10 - string.wchar_mbyte # SKIP no UTF-8 locale available
not ok 33 - codecs.b64_decode # TODO padding is decoded as data
# 40 passed, 1 failed, 5 known failures, 1 skipped (13.07 s)
```

* The captured output of a failed test (its own prints and the library's
  `debug()` messages) follows the `not ok` line, prefixed with `#`.
* A test that cannot run in the current environment reports `SKIP`.
* A known failure is marked `TODO` with the name of the defect. It still
  runs, its output is not shown, and it does not fail the run. Remove the
  mark when the defect is fixed; a `TODO` test that passes says so.
* A crash reports the signal, a hang reports the timeout.
* The exit status is 0 unless a test failed.
* A case registered with `TEST_RACY` is a regular case in every build and a
  known failure under ThreadSanitizer only, for code with a pinned data race
  (the server loop, D44). It flips back to `TEST` when the race is fixed.

## Writing a test

A suite is a file in `test/unit/` (or `test/integration/`) that includes
`test/askl_test.h`, defines its cases as `static int` functions returning
0 on success and -1 on failure, and exports a `Test_Suite`:

```c
#include "../askl_test.h"

static int _alloc(void)
{
    String *z = NULL;

    ASSERT_NOT_NULL(z = string_alloc("abc", 3));
    ASSERT_EQ_STR(z, "abc");
    z = string_free(z);

    return 0;
}

static const Test_Case _cases[] = {
    TEST("alloc", _alloc),
    TEST_SLOW("million_keys", _million_keys),
    TEST_TODO("b64_decode", _b64_decode, "padding is decoded as data")
};

TEST_SUITE(test_suite_example, "example", setup, teardown, _cases);
```

`setup` and `teardown` (or NULL) run before and after every case, in the
same process. Register the suite in the table in `test/runner.c`.

Assertions stop the test at the first failure and report the location:
`ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`,
`ASSERT_EQ_INT`, `ASSERT_NE_INT`, `ASSERT_EQ_UINT`, `ASSERT_EQ_PTR`,
`ASSERT_EQ_MEM`, `ASSERT_EQ_STR` (a `String` against a C string),
`ASSERT_WITHIN`, `FAIL`. `CHECK(cond)` records a failure and keeps going;
a test using it ends with `return test_status();`. `SKIP(reason)` leaves
the test.

Use `rand()` for randomized data: it is seeded from the run's seed. Print
freely, the output is only shown when the test fails or with `-v`.

## JSON conformance

`test/unit/conformance.c` runs the
[JSONTestSuite](https://github.com/nst/JSONTestSuite) corpus, held as a
table in `test/fixtures/jsontestsuite.h` (318 inputs, the two large ones
as a repeated pattern), through the tokenizer alone and through the parser
context, which adds UTF-8, escape and number validation. Each run gets a
verdict: `A` accepted as one complete value, `R` rejected, `I` incomplete,
`N` a bare number at the end of the input (a stream cannot know it ended),
`T` trailing data after a complete value, `E` nothing to parse. For both
layers, valid documents (`y_`) must be `A` or `N`, invalid ones (`n_`)
anything but `A`, implementation-defined ones (`i_`) are only recorded.

The table also pins both verdicts of every input, and `conformance.profile`
fails on any change, naming the input, the layer and both verdicts. After a
deliberate change to the tokenizer or the parser, review the diff and
re-pin:

```
make check TEST=conformance
ASKL_TEST_PROFILE=test/fixtures/jsontestsuite.h \
    LD_LIBRARY_PATH=. ./test.out conformance.profile
```

The second command rewrites the header from the table in memory with the
new verdicts, so the diff shows exactly the verdicts that moved.

## JSONPath compliance

`test/unit/compliance.c` runs the
[JSONPath Compliance Test Suite](https://github.com/jsonpath-standard/jsonpath-compliance-test-suite)
(RFC 9535), held as a table in `test/fixtures/cts.h`: 706 selectors with
their document and expected results as RFC 6901 pointers, converted from
the suite's normalized paths, with every admissible ordering when the
suite gives several. `compliance.valid_selectors` and
`compliance.invalid_selectors` fail on any deviation, naming the selector
and showing the results found and expected; `compliance.regex_selectors`
covers the 50 `match()` and `search()` cases and skips without PCRE. The
table pins the verdict of every test, and `compliance.profile` fails on
any change. To re-pin, or to import a newer `cts.json` (the importer
reads it through ASKL's own tokenizer and index, no other tool):

```
make check TEST=compliance
ASKL_TEST_PROFILE=test/fixtures/cts.h \
    LD_LIBRARY_PATH=. ./test.out compliance.profile
ASKL_TEST_CTS=path/to/cts.json ASKL_TEST_CTS_COMMIT=<hash> \
    ASKL_TEST_PROFILE=test/fixtures/cts.h \
    LD_LIBRARY_PATH=. ./test.out compliance.profile
```

The regex verdicts are only checked, and re-pinned, by a build with PCRE.

## Layout

```
test/
  askl_test.h        harness API
  runner.c           the harness
  legacy.c           wrappers around the suites not ported yet
  unit/              one file per module (string, codecs, format, json,
                     conformance, cbtrie)
  fixtures/          the JSONTestSuite and JSONPath compliance tables
  m_*_test.c         legacy suites, to be ported
  json/, hash/       standalone tools (json_checker, hashbench)
```
