# The SONAME sets the api version compatibility.
# It is using the same SONAME from the pre-installed sqlite3 library so
# the library can be loaded by existing applications as python. For this
# we can set the LD_LIBRARY_PATH when opening the app or set the rpath
# in the executable.

ifeq ($(OS),Windows_NT)
    CFLAGS   = -I../libuv/include -I../binn/src
    LFLAGS   = -L../libuv/.libs -L../binn
    IMPLIB   = aergolite-0.1
    LIBRARY  = aergolite-0.1.dll
    LDFLAGS  += -static-libgcc -static-libstdc++
else ifeq ($(PLATFORM),iPhoneOS)
    LIBRARY = libaergolite.dylib
    CFLAGS += -fPIC -fvisibility=hidden
else ifeq ($(PLATFORM),iPhoneSimulator)
    LIBRARY = libaergolite.dylib
    CFLAGS += -fPIC -fvisibility=hidden
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        OS = OSX
        LIBRARY  = libaergolite.0.dylib
        LIBNICK1 = libaergolite.dylib
        LIBNICK2 = libsqlite3.0.dylib
        LIBNICK3 = libsqlite3.dylib
        INSTNAME = $(LIBPATH)/$(LIBNICK1)
        CURR_VERSION   = 1.0.0
        COMPAT_VERSION = 1.0.0
        prefix  ?= /usr/local
    else
        LIBRARY  = libaergolite.so.0.0.1
        LIBNICK1 = libaergolite.so.0
        LIBNICK2 = libaergolite.so
        LIBNICK3 = libsqlite3.so.0
        LIBNICK4 = libsqlite3.so
        SONAME   = $(LIBNICK2)
        prefix  ?= /usr/local
    endif
    LIBPATH  = $(prefix)/lib
    LIBPATH2 = $(prefix)/lib/aergolite
    INCPATH  = $(prefix)/include
    EXEPATH  = $(prefix)/bin
   #LIBFLAGS += -fPIC $(CFLAGS)
    LIBFLAGS += -fPIC -fvisibility=hidden
    LDFLAGS  += -lpthread
    DEBUGFLAGS = -rdynamic
    SHELLFLAGS = -DHAVE_READLINE
    IMPLIB   = aergolite
endif

CC    = gcc
STRIP = strip
AR    = ar

SHORT = sqlite3

# the item below cannot be called SHELL because it's a reserved name
ifeq ($(OS),Windows_NT)
    SSHELL = sqlite3.exe
else
    SSHELL = sqlite3
endif

#LIBFLAGS = -Wall -DSQLITE_HAS_CODEC -DSQLITE_USE_URI=1 -DSQLITE_ENABLE_JSON1 -DSQLITE_THREADSAFE=1 -DHAVE_USLEEP -DSQLITE_ENABLE_COLUMN_METADATA
LIBFLAGS := $(LIBFLAGS) $(CFLAGS) -DSQLITE_HAS_CODEC -DSQLITE_USE_URI=1 -DSQLITE_ENABLE_JSON1 -DSQLITE_THREADSAFE=1 -DHAVE_USLEEP -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DSQLITE_ENABLE_COLUMN_METADATA


.PHONY:  install debug test valgrind clean amalgamation


all:      $(LIBRARY) $(SSHELL)

debug:    $(LIBRARY) $(SSHELL)

ios:      libaergolite.a libaergolite.dylib
iostest:  libaergolite.a libaergolite.dylib

debug:    export LIBFLAGS := -g -DSQLITE_DEBUG=1 -DDEBUGPRINT=1 $(DEBUGFLAGS) $(LIBFLAGS)

valgrind: export LIBFLAGS := -g -DSQLITE_DEBUG=1 $(DEBUGFLAGS) $(LIBFLAGS)


OBJECTS = sqlite3.o plugin-no-leader.o

aergolite-0.1.dll: $(OBJECTS)
	$(CC) -shared -Wl,--out-implib,$(IMPLIB).lib $^ -o $@ $(LFLAGS) -lbinn-1.0 -llibuv -lsecp256k1-vrf -lws2_32
ifeq ($(MAKECMDGOALS),valgrind)
else ifeq ($(MAKECMDGOALS),debug)
else
	$(STRIP) $@
endif

libaergolite.0.dylib: $(OBJECTS)
	$(CC) -dynamiclib -install_name "$(INSTNAME)" -current_version $(CURR_VERSION) -compatibility_version $(COMPAT_VERSION) $^ -o $@ $(LDFLAGS) -lbinn -luv -lsecp256k1-vrf -ldl
ifeq ($(MAKECMDGOALS),valgrind)
else ifeq ($(MAKECMDGOALS),debug)
else
	$(STRIP) -x $@
endif
	install_name_tool -change libbinn.1.dylib /usr/local/lib/libbinn.1.dylib $@
	ln -sf $(LIBRARY) $(LIBNICK1)
	ln -sf $(LIBRARY) $(LIBNICK2)
	ln -sf $(LIBRARY) $(LIBNICK3)

libaergolite.a: $(OBJECTS)
	$(AR) rcs $@ $^

libaergolite.dylib: $(OBJECTS)
	$(CC) -dynamiclib -o $@ $^ $(LDFLAGS) -lbinn -luv -lsecp256k1-vrf -ldl
ifeq ($(MAKECMDGOALS),valgrind)
else ifeq ($(MAKECMDGOALS),debug)
else
	$(STRIP) -x $@
endif

libaergolite.so.0.0.1: $(OBJECTS)
	$(CC) -shared -Wl,-soname,$(SONAME) $^ -o $@ $(LDFLAGS) -lbinn -luv -lsecp256k1-vrf -ldl
ifeq ($(MAKECMDGOALS),valgrind)
else ifeq ($(MAKECMDGOALS),debug)
else
	$(STRIP) $@
endif
	ln -sf $(LIBRARY) $(LIBNICK1)
	ln -sf $(LIBNICK1) $(LIBNICK2)
	ln -sf $(LIBRARY) $(LIBNICK3)
	ln -sf $(LIBNICK3) $(LIBNICK4)


sqlite3.o: core/sqlite3.c core/aergolite.h core/single_instance.h core/single_instance.c common/sha256.c common/sha256.h common/checksum.c common/linked_list.c common/array.c
	$(CC) $(LIBFLAGS) -c $< -o $@

plugin-no-leader.o: plugins/no-leader/no-leader.c plugins/no-leader/no-leader.h plugins/no-leader/node_discovery.c plugins/no-leader/allowed_nodes.c plugins/no-leader/requests.c plugins/no-leader/state_update.c plugins/no-leader/transactions.c plugins/no-leader/block_producer.c plugins/no-leader/consensus.c common/sha256.c common/sha256.h plugins/common/uv_functions.c plugins/common/uv_msg_framing.c plugins/common/uv_msg_framing.h plugins/common/uv_send_message.c plugins/common/uv_callback.c plugins/common/uv_callback.h core/sqlite3.h
	$(CC) $(LIBFLAGS) -c $< -o $@


$(SSHELL): shell.o $(LIBRARY)
ifeq ($(OS),Windows_NT)
	$(CC) $< -o $@ -L. -l$(IMPLIB) $(LFLAGS) -lbinn-1.0
else ifeq ($(OS),OSX)
	$(CC) $< -o $@ -L. -l$(IMPLIB) -ldl -lbinn -lreadline
else
	$(CC) $< -o $@ -Wl,-rpath,$(LIBPATH) -L. -l$(IMPLIB) -lbinn -lreadline -ldl
endif
	strip $(SSHELL)

shell.o: shell/shell.c
	$(CC) -c $(CFLAGS) $(SHELLFLAGS) $< -o $@

amalgamation:
	$(CC) build_amalgamation.c -o build_amalgamation
	./build_amalgamation

install:
	mkdir -p $(LIBPATH)
	mkdir -p $(LIBPATH2)
	cp $(LIBRARY) $(LIBPATH)/
	cd $(LIBPATH) && ln -sf $(LIBRARY) $(LIBNICK1)
ifeq ($(OS),OSX)
	cd $(LIBPATH2) && ln -sf ../$(LIBNICK1) $(LIBNICK2)
	cd $(LIBPATH2) && ln -sf $(LIBNICK2) $(LIBNICK3)
else
	cd $(LIBPATH) && ln -sf $(LIBNICK1) $(LIBNICK2)
	cd $(LIBPATH2) && ln -sf ../$(LIBRARY) $(LIBNICK3)
	cd $(LIBPATH2) && ln -sf $(LIBNICK3) $(LIBNICK4)
endif
	cp core/sqlite3.h $(INCPATH)
	cp $(SSHELL) $(EXEPATH)

clean:
	rm -f *.o libaergolite.a libaergolite.dylib $(LIBRARY) $(LIBNICK1) $(LIBNICK2) $(LIBNICK3) $(LIBNICK4) $(IMPLIB).lib $(SSHELL) test/runtest

test/runtest: test/test.c test/db_functions.c
	$(CC) -std=gnu99 $< -o $@ -L. -l$(IMPLIB) -lsecp256k1-vrf

test: test/runtest
ifeq ($(OS),OSX)
	cd test && DYLD_LIBRARY_PATH=..:/usr/local/lib ./runtest
else
	cd test && LD_LIBRARY_PATH=..:/usr/local/lib ./runtest
endif

valgrind: $(LIBRARY) test/runtest
ifeq ($(OS),OSX)
	cd test && DYLD_LIBRARY_PATH=..:/usr/local/lib valgrind --leak-check=full --show-leak-kinds=all ./runtest
else
	cd test && LD_LIBRARY_PATH=..:/usr/local/lib valgrind --leak-check=full --show-leak-kinds=all ./runtest
endif

test2: test/test.py
ifeq ($(OS),Windows_NT)
ifeq ($(PY_HOME),)
	@echo "PY_HOME is not set"
else
	cd $(PY_HOME)/DLLs && [ ! -f sqlite3-orig.dll ] && mv sqlite3.dll sqlite3-orig.dll || true
	cp $(LIBRARY) $(PY_HOME)/DLLs/sqlite3.dll
	cd test && python test.py -v
endif
else
ifeq ($(OS),OSX)
#ifneq ($(shell python -c "import pysqlite2.dbapi2" 2> /dev/null; echo $$?),0)
#ifneq ($(shell [ -d $(LIBPATH2) ]; echo $$?),0)
#	@echo "run 'sudo make install' first"
#endif
#	git clone --depth=1 https://github.com/ghaering/pysqlite
#	cd pysqlite && echo "include_dirs=$(INCPATH)" >> setup.cfg
#	cd pysqlite && echo "library_dirs=$(LIBPATH2)" >> setup.cfg
#	cd pysqlite && python setup.py build
#	cd pysqlite && sudo python setup.py install
#endif
	cd test && python test.py -v
else	# Linux
	cd test && LD_LIBRARY_PATH=..:/usr/local/lib python test.py -v
endif
endif


# variables:
#   $@  output
#   $^  all the requirements
#   $<  first requirement
