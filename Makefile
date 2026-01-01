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
# -D_ENABLE_HASHTABLE             : enable builtin hashtable implementation
# -D_ENABLE_PRIVILEGE_SEPARATION  : drop privileges in the server process
# -D_ENABLE_BUILTIN_PLUGIN        : embed a default plugin
# -D_ENABLE_CONFIG                : XML configuration file
# -D_ENABLE_JSON                  : enable builtin JSON tokenizer
# -D_ENABLE_PARSER                : enable builtin JSON parser
# -D_USE_BIG_FDS=<int>            : enable the use of more than FD_SETSIZE fds

CONFIG  = -D_ENABLE_SERVER \
          -D_ENABLE_UDP \
          -D_ENABLE_SSL \
          -D_ENABLE_RANDOM \
          -D_ENABLE_HASHTABLE \
          -D_ENABLE_TRIE \
          -D_ENABLE_FILE \
          -D_ENABLE_PCRE \
          -D_ENABLE_JSON \
          -D_ENABLE_PARSER \
          -D_ENABLE_CONFIG \
          -D_BUILTIN_MODULE \
          -D_USE_BIG_FDS=4095

# Modules configuration flags
# Available PLGCONF flags:
# -D_DIALMSN_ENABLE_BOT           : enable the bot in the DialMessenger module

PLGCONF = -D_DIALMSN_ENABLE_BOT

# Files
OBJBIN  = $(addsuffix .o, $(basename $(wildcard *.c)))
OBJLIB  = $(addsuffix .o, $(basename $(wildcard lib/*.c))) \
          $(addsuffix .o, $(basename $(wildcard lib/util/*.c))) \
          lib/compat/askl_compat_layer.o
OBJTEST = $(addsuffix .o, $(basename $(wildcard test/*.c)))
OBJPROF = $(addsuffix .gcno, $(basename $(wildcard lib/*.c))) \
          $(addsuffix .gcno, $(basename $(wildcard lib/util/*.c))) \
          $(addsuffix .gcno, $(basename $(wildcard test/*.c))) \
          $(addsuffix .gcda, $(basename $(wildcard lib/*.c))) \
          $(addsuffix .gcda, $(basename $(wildcard lib/util/*.c))) \
          $(addsuffix .gcda, $(basename $(wildcard test/*.c))) \
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
# make test   : run the unit tests and enable the DEBUG build options

# Build settings:
# FINAL corresponds to production settings
# DEBUG is used for debug builds
# FLAGS will be passed as CFLAGS to the compiler
# SHARED is the set of required compiler options to produce a shared object
# LIBFLAGS is other specific flags used to build a library

BUILD    =
FINAL    = -O2 -pipe
DEBUG    = -O0 -g -DDEBUG -DDEBUG_SQL
TRACE    = -O0 -g -pg -fprofile-generate
FLAGS    = -std=c11 -W -Wall $(BUILD) -Wpointer-arith

SHARED   =
LIBFLAGS =

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

# GCC options
ifeq ($(CC),gcc)
GCC_ALIASED  = $(shell gcc --version | head -1 | cut -d\  -f1)
GCC_MAJ      = $(shell gcc --version | head -1 | cut -d\  -f3 | cut -d. -f1)
GCC_MIN      = $(shell gcc --version | head -1 | cut -d\  -f3 | cut -d. -f2)
GCC_INC      = gcc -o /dev/null -E -xc - 2> /dev/null
HAS_DL       = $(shell echo '\#include <dlfcn.h>' | $(GCC_INC); echo $$?)
HAS_RL       = $(shell echo '\#include <readline/readline.h>' | $(GCC_INC); echo $$?)
HAS_SSL      = $(shell echo '\#include <openssl/ssl.h>' | $(GCC_INC); echo $$?)
HAS_SHADOW   = $(shell echo '\#include <shadow.h>' | $(GCC_INC); echo $$?)
HAS_ZLIB     = $(shell echo '\#include <zlib.h>' | $(GCC_INC); echo $$?)
HAS_POLL     = $(shell echo '\#include <poll.h>' | $(GCC_INC); echo $$?)
HAS_PCRE     = $(shell echo '\#include <pcre.h>' | $(GCC_INC); echo $$?)
HAS_LIBXML   = $(shell which xml2-config 2> /dev/null)
HAS_MYSQL    = $(shell which mysql_config 2> /dev/null)
HAS_SQLITE   = $(shell echo '\#include <sqlite3.h>' | $(GCC_INC); echo $$?)
HAS_ICONV    = $(shell echo '\#include <iconv.h>' | $(GCC_INC); echo $$?)
ifeq ($(GCC_ALIASED),gcc)
FINAL   += -finline -mpreferred-stack-boundary=4 -minline-all-stringops
FLAGS   += -findirect-inlining -posix
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

# position independent code is required for x86_64 libraries
ifeq ($(ARCH), x86_64)
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
ifeq ($(CC),gcc)
FLAGS  += -rdynamic
endif
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
DBG_PARMS = -silent ./$(DBG_BIN) -ex 'handle SIGPIPE nostop' -ex r
else
ifeq ($(DBG), lldb)
DBG_PARMS = ./$(DBG_BIN) -o r
endif
endif

.PHONY: all debug lib dbglib server dbgserver modules test install clean

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
lib: BUILD = $(FINAL)
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

test: CFLAGS = $(FLAGS)
test: $(OBJTEST)
	@$(CC) $(OBJTEST) $(CFLAGS) $(CONFIG) $(LIBS) -L. -l$(LIB) -o $(DBG_BIN)
	@echo "TEST"
	@-(LD_LIBRARY_PATH=. $(DBG) $(DBG_PARMS));

testfinal: CFLAGS = $(FLAGS)
testfinal: $(OBJTEST)
	@$(CC) $(OBJTEST) $(CFLAGS) $(CONFIG) $(LIBS) -L. -l$(LIB) -o $(DBG_BIN)
	@echo "TEST"
	@-(LD_LIBRARY_PATH=. ./$(DBG_BIN))

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
	-D_ENABLE_JSON -D_ENABLE_TRIE -D_ENABLE_PARSER \
	$(FINAL) -lpthread \
	lib/util/m_util_vfscanf.c lib/util/m_util_vfprintf.c \
	lib/util/m_util_float.c lib/util/m_util_dtoa.c \
	lib/compat/askl_compat_layer.c lib/m_string.c lib/askl_cbtrie.c \
	lib/askl_variant.c lib/m_parser.c \
	test/json/json_checker.c -o json_checker

json_debug:
	@echo "JSON_CHECKER (DEBUG)"
	@$(CC) \
	-D_ENABLE_JSON -D_ENABLE_TRIE -D_ENABLE_PARSER \
	$(DEBUG) -lpthread \
	lib/util/m_util_vfscanf.c lib/util/m_util_vfprintf.c \
	lib/util/m_util_float.c lib/util/m_util_dtoa.c \
	lib/compat/askl_compat_layer.c lib/m_string.c lib/askl_cbtrie.c \
	lib/askl_variant.c lib/m_parser.c \
	test/json/json_checker.c -o json_checker

hashbench:
	@echo "HASHBENCH"
	@$(CC) \
	-D_ENABLE_HASHTABLE \
	$(FINAL) -lpthread \
	lib/compat/askl_compat_layer.c lib/askl_htable.c \
	lib/askl_variant.c \
	test/hash/hash.c -o hashbench

clean:
	@echo "CLEAN"
	@rm -f $(BIN) lib$(LIB).$(LIBEXT) $(PLG).so \
	$(OBJBIN) $(OBJLIB) $(OBJPLG) $(OBJTEST) $(OBJPROF) \
	*~ lib/*~ lib/util/*~ lib/compat/*~ test/*~ $(DBG_BIN) \
	plugins/*.so plugins/*/*~ lib/*.o lib/util/*.o lib/compat/*.o test/*.o \
	*.d lib/*.d lib/util/*.d lib/compat/*.d test/*.d plugins/*/*.d \
	json_checker hashbench
	@rm -rf *.dSYM plugins/*.dSYM

-include $(OBJBIN:.o=.d)
-include $(OBJLIB:.o=.d)
-include $(OBJTEST:.o=.d)
