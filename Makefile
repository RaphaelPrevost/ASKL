################################################################################
#  ASKL.                                                                       #
#  Copyright (c) 2026 Raphael Prevost <raph@el.bzh>                            #
#                                                                              #
#  This software is a computer program whose purpose is to provide a           #
#  framework for developing and prototyping network services.                  #
#                                                                              #
#  This software is governed by the CeCILL  license under French law and       #
#  abiding by the rules of distribution of free software.  You can  use,       #
#  modify and/ or redistribute the software under the terms of the CeCILL      #
#  license as circulated by CEA, CNRS and INRIA at the following URL           #
#  "http://www.cecill.info".                                                   #
#                                                                              #
#  As a counterpart to the access to the source code and  rights to copy,      #
#  modify and redistribute granted by the license, users are provided only     #
#  with a limited warranty  and the software's author,  the holder of the      #
#  economic rights,  and the successive licensors  have only  limited          #
#  liability.                                                                  #
#                                                                              #
#  In this respect, the user's attention is drawn to the risks associated      #
#  with loading,  using,  modifying and/or developing or reproducing the       #
#  software by the user in light of its specific status of free software,      #
#  that may mean  that it is complicated to manipulate,  and  that  also       #
#  therefore means  that it is reserved for developers  and  experienced       #
#  professionals having in-depth computer knowledge. Users are therefore       #
#  encouraged to load and test the software's suitability as regards their     #
#  requirements in conditions enabling the security of their systems and/or    #
#  data to be ensured and,  more generally, to use and operate it in the       #
#  same conditions as regards security.                                        #
#                                                                              #
#  The fact that you are presently reading this means that you have had        #
#  knowledge of the CeCILL license and that you accept its terms.              #
#                                                                              #
################################################################################

PROJECT = askl
CC      = gcc
DBG     = gdb

# ASKL configuration flags
# Available CONFIG flags:
# -DDEBUG                         : enable debug messages
# -DDEBUG_SQL                     : display every SQL queries
# -D_ENABLE_DB                    : enable the database API
# -D_ENABLE_MYSQL                 : enable MySQL driver
# -D_ENABLE_SQLITE                : enable SQLite driver
# -D_ENABLE_HTTP                  : enable native HTTP handling
# -D_ENABLE_FILE                  : enable the file API
# -D_ENABLE_UDP                   : allow use of UDP sockets
# -D_ENABLE_SSL                   : allow use of SSL/TLS secure sockets
# -D_ENABLE_SERVER                : enable the server
# -D_ENABLE_RANDOM                : enable builtin PRNG
# -D_ENABLE_TRIE                  : enable builtin crit-bit trie implementation
# -D_ENABLE_HASHMAP               : enable builtin hash map implementation
# -D_ENABLE_PRIVILEGE_SEPARATION  : drop privileges in the server process
# -D_ENABLE_BUILTIN_PLUGIN        : embed a default plugin
# -D_ENABLE_CONFIG                : XML configuration file
# -D_ENABLE_JSON                  : enable builtin JSON tokenizer and index
# -D_USE_BIG_FDS=<int>            : enable the use of more than FD_SETSIZE fds

CONFIG  = -D_ENABLE_SERVER \
          -D_ENABLE_UDP \
          -D_ENABLE_SSL \
          -D_ENABLE_RANDOM \
          -D_ENABLE_HASHMAP \
          -D_ENABLE_TRIE \
          -D_ENABLE_FILE \
          -D_ENABLE_PCRE \
          -D_ENABLE_JSON \
          -D_ENABLE_CONFIG \
          -D_BUILTIN_MODULE \
          -D_USE_BIG_FDS=4095

PLGCONF =

# Files
OBJBIN  = $(addsuffix .o, $(basename $(wildcard *.c)))
OBJLIB  = $(addsuffix .o, $(basename $(wildcard lib/*.c))) \
          $(addsuffix .o, $(basename $(wildcard lib/string/*.c))) \
		  $(addsuffix .o, $(basename $(wildcard lib/string/format/*.c))) \
          lib/compat/askl_compat_layer.o
OBJTEST = $(addsuffix .o, $(basename $(wildcard test/*.c))) \
          $(addsuffix .o, $(basename $(wildcard test/unit/*.c)))
OBJPROF = $(addsuffix .gcno, $(basename $(wildcard lib/*.c))) \
          $(addsuffix .gcno, $(basename $(wildcard lib/util/*.c))) \
          $(addsuffix .gcno, $(basename $(wildcard test/*.c))) \
          $(addsuffix .gcda, $(basename $(wildcard lib/*.c))) \
          $(addsuffix .o, $(basename $(wildcard lib/string/*.c))) \
		  $(addsuffix .o, $(basename $(wildcard lib/string/format/*.c))) \
          $(addsuffix .gcda, $(basename $(wildcard test/*.c))) \
          $(addsuffix .gcno, $(basename $(wildcard test/unit/*.c))) \
          $(addsuffix .gcda, $(basename $(wildcard test/unit/*.c))) \
          $(addsuffix .gcno, $(basename $(wildcard lib/string/*.c))) \
          $(addsuffix .gcda, $(basename $(wildcard lib/string/*.c))) \
          $(addsuffix .gcno, $(basename $(wildcard lib/string/format/*.c))) \
          $(addsuffix .gcda, $(basename $(wildcard lib/string/format/*.c))) \
          lib/compat/askl_compat_layer.gcno \
		  lib/compat/askl_compat_layer.gcda \
		  *.gcno *.gcda \
          gmon.out
LIBS    = -lpthread
BIN     = askl
LIB     = askl
DBG_BIN = test.out
MODULES = $(shell find plugins/* -type d | grep -v .svn)

# Build options:
# You can select the build options with the make target.
# make        : enable FINAL build options
# make all    : "
# make modules: "
# make debug  : enable DEBUG build options
# make test   : run the unit tests in a debugger, with the DEBUG build options
# make check  : build and run the unit tests (fast tier), TAP output
#               make check TEST="string codecs.base64" selects suites or cases
# make check-slow     : same, including the slow tier
# make check-asan     : same, under AddressSanitizer and UBSan
# make check-tsan     : same, under ThreadSanitizer
# make check-noatomics: same, with the mutex-only lock and queue fallbacks
# make check-coverage : same, and write a line coverage report in coverage/
# NOTE: run make clean before switching between check-* targets, the object
#       files are shared between them

# Build settings:
# FINAL corresponds to production settings
# DEBUG is used for debug builds
# FLAGS will be passed as CFLAGS to the compiler
# SHARED is the set of required compiler options to produce a shared object
# LIBFLAGS is other specific flags used to build a library

BUILD    =
FINAL    = -O2 -pipe -DNDEBUG
DEBUG    = -O0 -g -DDEBUG -DDEBUG_SQL
TRACE    = -O0 -g -pg -fprofile-generate
FLAGS    = -std=c11 -pedantic -W -Wall $(BUILD) -Wpointer-arith

SHARED   =
LIBFLAGS =
LIBFINAL =

# Installation
PREFIX =
CONFDIR =
DESTDIR =
SHAREDIR =

# system features
OS           = $(shell uname)
ARCH         = $(shell uname -m)
LIBEXT       = so
HAS_DL       =
HAS_RL       =
HAS_SSL      =
HAS_SHADOW   =
HAS_ZLIB     =
HAS_POLL     =
HAS_PCRE     =
HAS_LIBXML   =
HAS_MYSQL    =
HAS_SQLITE   =
HAS_ICONV    =

# compiler identification
GCC_ALIASED  = $(shell $(CC) --version | head -1 | cut -d\  -f1)
GCC_CLANG    = $(shell $(CC) --version | grep -q clang; echo $$?)
GCC_MAJ      = $(shell $(CC) --version | head -1 | cut -d\  -f3 | cut -d. -f1)
GCC_MIN      = $(shell $(CC) --version | head -1 | cut -d\  -f3 | cut -d. -f2)
# header probes: preprocess an empty file that force-includes the header
# (a literal '#include' cannot be echoed from $(shell) without leaving a
# stray backslash behind, which made every probe succeed)
GCC_INC      = $(CC) -E -xc -o /dev/null -include
HAS_DL       = $(shell $(GCC_INC) dlfcn.h /dev/null 2> /dev/null; echo $$?)
HAS_RL       = $(shell $(GCC_INC) readline/readline.h /dev/null 2> /dev/null; echo $$?)
HAS_SSL      = $(shell $(GCC_INC) openssl/ssl.h /dev/null 2> /dev/null; echo $$?)
HAS_SHADOW   = $(shell $(GCC_INC) shadow.h /dev/null 2> /dev/null; echo $$?)
HAS_ZLIB     = $(shell $(GCC_INC) zlib.h /dev/null 2> /dev/null; echo $$?)
HAS_POLL     = $(shell $(GCC_INC) poll.h /dev/null 2> /dev/null; echo $$?)
HAS_PCRE     = $(shell $(GCC_INC) pcre.h /dev/null 2> /dev/null; echo $$?)
HAS_LIBXML   = $(shell which xml2-config 2> /dev/null)
HAS_MYSQL    = $(shell which mysql_config 2> /dev/null)
HAS_SQLITE   = $(shell $(GCC_INC) sqlite3.h /dev/null 2> /dev/null; echo $$?)
HAS_ICONV    = $(shell $(GCC_INC) iconv.h /dev/null 2> /dev/null; echo $$?)

# GCC options
ifeq ($(GCC_ALIASED),gcc)
FLAGS += -findirect-inlining
# disable some judgemental warnings
ifeq ($(shell test $(GCC_MAJ) -ge 6; echo $$?),0)
FLAGS += -Wno-misleading-indentation
ifeq ($(shell test $(GCC_MAJ) -ge 7; echo $$?),0)
FLAGS += -Wno-implicit-fallthrough
ifeq ($(shell test $(GCC_MAJ) -ge 8; echo $$?),0)
FLAGS += -Wno-cast-function-type
endif
endif
endif
endif

ifeq ($(GCC_CLANG),0)
FLAGS += -Wno-flexible-array-extensions
LIBFINAL += -flto=auto
endif

ifneq ($(PREFIX), )
CONFIG += -DPREFIX=$(PREFIX)
endif

ifneq ($(CONFDIR), )
CONFIG += -DCONFDIR=$(CONFDIR)
endif

ifneq ($(SHAREDIR), )
CONFIG += -DSHAREDIR=$(SHAREDIR)
endif

# position independent code is required for x86_64 and AArch64 libraries
ifneq ($(filter $(ARCH), x86_64 aarch64 arm64),)
LIBFLAGS += -fPIC
endif

# check for shadow.h
ifeq ($(HAS_SHADOW),0)
CONFIG += -DHAS_SHADOW
endif

# check for zlib
ifeq ($(HAS_ZLIB),0)
CONFIG += -DHAS_ZLIB
LIBS += -lz
endif

# check for poll(2)
ifeq ($(HAS_POLL),0)
ifneq ($(OS),Darwin)
# Mac OS X poll implementation is broken
CONFIG += -DHAS_POLL
endif
endif

# check for libxml2
ifneq ($(HAS_LIBXML), )
CONFIG += -DHAS_LIBXML
FLAGS += $(shell xml2-config --cflags)
LIBS += $(shell xml2-config --libs)
endif

# check for iconv
ifeq ($(HAS_ICONV),0)
CONFIG += -DHAS_ICONV
endif

PCRE_ENABLED=$(shell echo $(CONFIG) | grep -c D_ENABLE_PCRE)

# check for PCRE
ifeq ($(PCRE_ENABLED),1)
ifeq ($(HAS_PCRE),0)
CONFIG += -DHAS_PCRE
LIBS += -lpcre
endif
endif

# check if SSL support was enabled
SSL_ENABLED=$(shell echo $(CONFIG) | grep -c D_ENABLE_SSL)

# check for OpenSSL
ifeq ($(SSL_ENABLED),1)
ifeq ($(HAS_SSL),0)
CONFIG += -DHAS_SSL
LIBS += -lcrypt -lssl
endif
endif

# check if SQLite support was enabled
SQLITE_ENABLED=$(shell echo $(CONFIG) | grep -c D_ENABLE_SQLITE)

# check for SQLite
ifeq ($(SQLITE_ENABLED),1)
ifeq ($(HAS_SQLITE),0)
CONFIG += -DHAS_SQLITE
LIBS += -lsqlite3
endif
endif

# check if MySQL support was enabled
MYSQL_ENABLED=$(shell echo $(CONFIG) | grep -c D_ENABLE_MYSQL)

# check for MySQL
ifeq ($(MYSQL_ENABLED),1)
ifneq ($(HAS_MYSQL), )
CONFIG += -DHAS_MYSQL
FLAGS += $(shell mysql_config --cflags)
LIBS += $(shell mysql_config --libs_r)
endif
endif

# Linux specific settings
ifeq ($(OS),Linux)
LIBS   += -ldl -lrt
# required for clang
FLAGS  += -fgnu89-inline
FLAGS  += -rdynamic
SHARED += -shared
MODULE += $(SHARED)
endif

# Mac OS X specific settings
ifeq ($(OS),Darwin)
# APPLE gcc default preprocessor can't cope with variadic macros
FLAGS += -no-cpp-precomp
ifeq ($(HAS_DL),0)
# Mac OS >= 10.3
export MACOSX_DEPLOYMENT_TARGET := 10.3
DBG = lldb
LIBS   += -ldl
DEBUG  += -fsanitize=address -fsanitize=undefined
SHARED += -dynamiclib
MODULE += -mmacosx-version-min=10.3 -bundle -undefined dynamic_lookup
LIBEXT = dylib
else
# Mac OS 10.2
SHARED += -bundle
MODULE += -bundle -bundle_loader $(BIN)
endif
# iconv needs to be explicitly linked on Mac OS X
ifeq ($(HAS_ICONV),0)
LIBS += -liconv
endif
endif

ifeq ($(DBG), gdb)
DBG_PARMS = -silent -ex 'handle SIGPIPE nostop' -ex r \
            --args ./$(DBG_BIN) --no-fork $(TEST)
else
ifeq ($(DBG), lldb)
DBG_PARMS = -o r -- ./$(DBG_BIN) --no-fork $(TEST)
endif
endif

# the unit tests binary, run from the build tree
CHECK = LD_LIBRARY_PATH=. DYLD_LIBRARY_PATH=. ./$(DBG_BIN)

.PHONY: all debug lib dbglib server dbgserver modules test install clean \
        check check-slow check-asan check-tsan check-noatomics check-coverage

# Build targets

# build the server, the library and the modules with optimizations enabled
all: BUILD = $(FINAL)
all: modules
# build server, library and modules with debug enabled
debug: BUILD = $(DEBUG)
debug: modules
# build server, library and modules with tracing support
trace: BUILD = $(TRACE)
trace: modules

# build only the library, with optimizations
lib: BUILD = $(FINAL) $(LIBFINAL)
lib: $(LIB)
# library only, with debug
dbglib: BUILD = $(DEBUG)
dbglib: $(LIB)

# build the server and the library with optimizations enabled
server: BUILD = $(FINAL)
server: $(BIN)
# server and library with debug
dbgserver: BUILD = $(DEBUG)
dbgserver: $(BIN)

# build server, library and modules, and install them in the DESTDIR folder
install: BUILD = $(FINAL)

# build server, library and modules, and run the unit tests suite
test: BUILD = $(DEBUG)

# same than test, but with optimizations enabled
testfinal: BUILD = $(FINAL)

# build the unit tests and run the fast tier
check: BUILD = $(DEBUG)
# same, including the slow tier
check-slow: BUILD = $(DEBUG)
# same, under AddressSanitizer and UndefinedBehaviorSanitizer
check-asan: BUILD = $(DEBUG) -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer
# same, under ThreadSanitizer (cannot be combined with check-asan)
check-tsan: BUILD = $(DEBUG) -fsanitize=thread
# same, with the mutex-only fallbacks of the lock and the queue
check-noatomics: BUILD = $(DEBUG) -DASKL_NO_ATOMICS
# same, and generate a line coverage report
check-coverage: BUILD = $(DEBUG) --coverage

# Build rules
.c.o:
	@echo "CC: $@"
	@$(CC) -c $< -o $@ $(CFLAGS) $(CONFIG) -Ilib
	@$(CC) -MM $< $(CFLAGS) $(CONFIG) -Ilib > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
     sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp

$(BIN): CFLAGS = $(FLAGS)
$(BIN): $(OBJBIN) lib$(LIB)
	@echo "LD: $(BIN)"
	@$(CC) $(CFLAGS) $(CONFIG) $(OBJBIN) -L. -l$(LIB) \
	-o $(BIN)

lib$(LIB): CFLAGS = $(FLAGS) $(LIBFLAGS)
lib$(LIB): $(OBJLIB)
	@echo "LD: lib$(LIB).$(LIBEXT)"
	@$(CC) $(SHARED) $(CFLAGS) $(CONFIG) $(OBJLIB) $(LIBS) \
	-o lib$(LIB).$(LIBEXT)

$(OBJTEST): lib$(LIB)

modules: CFLAGS = $(FLAGS) $(LIBFLAGS)
modules: $(BIN) lib$(LIB)
	@for PLG in $(MODULES); do \
		echo "LD: $${PLG}"; \
		$(CC) $(MODULE) $(CFLAGS) $(CONFIG) $(PLGCONF) $${PLG}/*.c \
		-L. -l$(LIB) -Ilib -o $${PLG}.so; \
	done;

$(DBG_BIN): $(OBJTEST)
	@echo "LD: $(DBG_BIN)"
	@$(CC) $(OBJTEST) $(CFLAGS) $(CONFIG) $(LIBS) -L. -l$(LIB) -o $(DBG_BIN)

test: CFLAGS = $(FLAGS)
test: $(DBG_BIN)
	@echo "TEST"
	@-(LD_LIBRARY_PATH=. DYLD_LIBRARY_PATH=. $(DBG) $(DBG_PARMS));

testfinal: CFLAGS = $(FLAGS)
testfinal: $(DBG_BIN)
	@echo "TEST"
	@-($(CHECK))

check check-slow check-asan check-tsan check-noatomics check-coverage: CFLAGS = $(FLAGS)

check: $(DBG_BIN)
	@echo "CHECK"
	@$(CHECK) $(CHECKFLAGS) $(TEST)

check-slow: $(DBG_BIN)
	@echo "CHECK (slow tier)"
	@$(CHECK) --slow $(CHECKFLAGS) $(TEST)

check-asan: $(DBG_BIN)
	@echo "CHECK (AddressSanitizer, UndefinedBehaviorSanitizer)"
	@$(CHECK) $(CHECKFLAGS) $(TEST)

check-tsan: $(DBG_BIN)
	@echo "CHECK (ThreadSanitizer)"
	@$(CHECK) $(CHECKFLAGS) $(TEST)

check-noatomics: $(DBG_BIN)
	@echo "CHECK (no atomics)"
	@$(CHECK) $(CHECKFLAGS) $(TEST)

check-coverage: $(DBG_BIN)
	@echo "CHECK (coverage)"
	@$(CHECK) $(CHECKFLAGS) $(TEST); status=$$?; \
	echo "COVERAGE: coverage/index.html"; \
	lcov --quiet --capture --directory lib --output-file coverage.info \
	 --exclude '*/lib/compat/*' --exclude '*/lib/string/format/*' \
	 --exclude '/usr/*' --ignore-errors inconsistent,unused,negative \
	&& genhtml --quiet coverage.info --output-directory coverage \
	&& lcov --summary coverage.info || status=1; \
	exit $$status

install: modules
	@echo "INSTALL"
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@mkdir -p $(DESTDIR)$(PREFIX)/lib/$(LIB)/plugins
	@strip $(BIN) lib$(LIB).$(LIBEXT) plugins/*.so
	@cp $(BIN) $(DESTDIR)$(PREFIX)/bin
	@cp lib$(LIB).$(LIBEXT) $(DESTDIR)$(PREFIX)/lib
	@cp plugins/*.so $(DESTDIR)$(PREFIX)/lib/$(LIB)/plugins/

json_checker:
	@echo "JSON_CHECKER"
	@$(CC) \
	-D_ENABLE_JSON -D_ENABLE_TRIE \
	$(FINAL) $(LIBFINAL) -lpthread \
	lib/compat/askl_compat_layer.c lib/askl_string.c lib/askl_cbtrie.c \
	lib/askl_variant.c lib/askl_rwlock.c lib/string/parser.c lib/askl_json.c \
	test/json/json_checker.c -o json_checker

json_debug:
	@echo "JSON_CHECKER (DEBUG)"
	@$(CC) \
	-D_ENABLE_JSON -D_ENABLE_TRIE \
	$(DEBUG) -lpthread \
	lib/compat/askl_compat_layer.c lib/askl_string.c lib/askl_cbtrie.c \
	lib/askl_variant.c lib/askl_rwlock.c lib/string/parser.c lib/askl_json.c \
	test/json/json_checker.c -o json_checker

hashbench:
	@echo "HASHBENCH"
	@$(CC) \
	-D_ENABLE_HASHMAP \
	$(FINAL) $(LIBFINAL) -lpthread \
	lib/compat/askl_compat_layer.c lib/askl_htable.c \
	lib/askl_rwlock.c lib/askl_variant.c \
	test/hash/hash.c -o hashbench

clean:
	@echo "CLEAN"
	@rm -f $(BIN) lib$(LIB).$(LIBEXT) $(PLG).so \
	$(OBJBIN) $(OBJLIB) $(OBJPLG) $(OBJTEST) $(OBJPROF) \
	*~ lib/*~ lib/string/*~ lib/string/format/*~ lib/json/*~ \
	lib/compat/*~ test/*~ test/unit/*~ $(DBG_BIN) \
	plugins/*.so plugins/*/*~ lib/*.o lib/util/*.o lib/compat/*.o test/*.o \
	test/unit/*.o *.d lib/*.d lib/string/*.d lib/string/format/*.d \
	lib/compat/*.d test/*.d test/unit/*.d plugins/*/*.d \
	json_checker hashbench coverage.info
	@rm -rf *.dSYM plugins/*.dSYM coverage

-include $(OBJBIN:.o=.d)
-include $(OBJLIB:.o=.d)
-include $(OBJTEST:.o=.d)
