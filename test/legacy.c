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

#include "askl_test.h"

/* the suites below have not been ported to the harness yet; each legacy
   entry point runs as a single test case so it already benefits from
   process isolation, timeouts and TAP reporting. remove an entry here
   once its suite has been ported to test/unit/ */

#ifdef _ENABLE_TRIE
#ifdef _ENABLE_FILE
extern int test_fs(void);
#endif
#endif
#if defined(_ENABLE_HTTP) && defined(_ENABLE_FILE)
extern int test_http(void);
#endif
#ifdef _ENABLE_DB
extern int test_db(void);
#endif

/* -------------------------------------------------------------------------- */

static const Test_Case _cases[] = {
    #ifdef _ENABLE_TRIE
    #ifdef _ENABLE_FILE
    TEST_TODO("fs", test_fs,
        "data races in fs_closefile()/_fs_map() (ThreadSanitizer)"),
    #endif
    #endif
    #if defined(_ENABLE_HTTP) && defined(_ENABLE_FILE)
    TEST("http", test_http),
    #endif
    #ifdef _ENABLE_DB
    TEST("db", test_db),
    #endif
};

TEST_SUITE(test_suite_legacy, "legacy", NULL, NULL, _cases);

/* -------------------------------------------------------------------------- */
