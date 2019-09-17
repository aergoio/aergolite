# The SONAME sets the api version compatibility.
# It is using the same SONAME from the pre-installed sqlite3 library so
# the library can be loaded by existing applications as python. For this
# we can set the LD_LIBRARY_PATH when opening the app or set the rpath
# in the executable.

ifeq ($(OS),Windows_NT)
    CFLAGS   = -I../libuv/include -I../binn/src
    LFLAGS   = -L../libuv/Release -L../binn/src/win32/Release
    IMPLIB   = aergolite-0.1
    LIBRARY  = aergolite-0.1.dll
    LDFLAGS  += -static-libgcc -static-libstdc++
else ifeq ($(PLATFORM),iPhoneOS)
    LIBRARY = libaergolite.dylib
    CFLAGS += -fPIC
else ifeq ($(PLATFORM),iPhoneSimulator)
    LIBRARY = libaergolite.dylib
    CFLAGS += -fPIC
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        OS = OSX
        LIBRARY  = libaergolite.0.dylib
        LIBNICK1 = libaergolite.dylib
        LIBNICK2 = libsqlite3.0.dylib
        LIBNICK3 = libsqlite3.dylib
        INSTNAME = $(LIBPATH2)/libsqlite3.dylib
        CURR_VERSION   = 1.0.0
        COMPAT_VERSION = 1.0.0
        prefix  ?= /usr/local
    else
#        IMPLIB   = aergolite
        LIBRARY  = libaergolite.so.0.0.1
        LIBNICK1 = libaergolite.so.0
        LIBNICK2 = libaergolite.so
        LIBNICK3 = libsqlite3.so.0
        LIBNICK4 = libsqlite3.so
        SONAME   = libsqlite3.so.0
        prefix  ?= /usr
    endif
    LIBPATH  = $(prefix)/lib
    LIBPATH2 = $(prefix)/lib/aergolite
    INCPATH  = $(prefix)/include
    EXEPATH  = $(prefix)/bin
   #LIBFLAGS += -fPIC $(CFLAGS)
    LIBFLAGS += -fPIC
    LDFLAGS  += -lpthread
    DEBUGFLAGS = -rdynamic
    SHELLFLAGS = -DHAVE_READLINE
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


.PHONY:  install debug test valgrind clean


all:      $(LIBRARY) $(SSHELL)

debug:    $(LIBRARY) $(SSHELL)

valgrind: $(LIBRARY)

ios:      libaergolite.a libaergolite.dylib
iostest:  libaergolite.a libaergolite.dylib

debug:    export LIBFLAGS := -g -DSQLITE_DEBUG=1 -DDEBUGPRINT=1 $(DEBUGFLAGS) $(LIBFLAGS)

valgrind: export LIBFLAGS := -g -DSQLITE_DEBUG=1 $(DEBUGFLAGS) $(LIBFLAGS)


OBJECTS = sqlite3.o plugin-mini-raft.o

aergolite-0.1.dll: $(OBJECTS)
	$(CC) -shared -Wl,--out-implib,$(IMPLIB).lib $^ -o $@ $(LFLAGS) -lbinn-1.0 -llibuv -lws2_32
ifeq ($(MAKECMDGOALS),valgrind)
else ifeq ($(MAKECMDGOALS),debug)
else
	$(STRIP) $@
endif

libaergolite.0.dylib: $(OBJECTS)
	$(CC) -dynamiclib -install_name "$(INSTNAME)" -current_version $(CURR_VERSION) -compatibility_version $(COMPAT_VERSION) $^ -o $@ $(LDFLAGS) -lbinn -luv
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
	$(CC) -dynamiclib -o $@ $^ $(LDFLAGS) -lbinn -luv
ifeq ($(MAKECMDGOALS),valgrind)
else ifeq ($(MAKECMDGOALS),debug)
else
	$(STRIP) -x $@
endif

libaergolite.so.0.0.1: $(OBJECTS)
	$(CC) -shared -Wl,-soname,$(SONAME) $^ -o $@ $(LDFLAGS) -lbinn -luv
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

plugin-mini-raft.o: plugins/mini-raft/mini-raft.c plugins/mini-raft/mini-raft.h plugins/mini-raft/node_discovery.c plugins/mini-raft/allowed_nodes.c plugins/mini-raft/leader_election.c plugins/mini-raft/state_update.c plugins/mini-raft/transactions.c plugins/mini-raft/consensus.c common/sha256.c common/sha256.h plugins/common/uv_functions.c plugins/common/uv_msg_framing.c plugins/common/uv_msg_framing.h plugins/common/uv_send_message.c plugins/common/uv_callback.c plugins/common/uv_callback.h
	$(CC) $(LIBFLAGS) -c $< -o $@


$(SSHELL): shell.o $(LIBRARY)
ifeq ($(OS),Windows_NT)
	$(CC) $< -o $@ -L. -l$(IMPLIB) $(LFLAGS) -lbinn-1.0
else ifeq ($(OS),OSX)
	$(CC) $< -o $@ -L. -lsqlite3 -ldl -lbinn -lreadline
else
	$(CC) $< -o $@ -Wl,-rpath,$(LIBPATH) -L. -lsqlite3 -ldl -lbinn -lreadline
endif
	strip $(SSHELL)

shell.o: shell/shell.c
	$(CC) -c $(SHELLFLAGS) $< -o $@

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
	rm -f *.o libaergolite.a libaergolite.dylib $(LIBRARY) $(LIBNICK1) $(LIBNICK2) $(LIBNICK3) $(LIBNICK4) $(SSHELL) tests

test: test/test.c
	$(CC) $< -o test/$@ -L. -lsqlite3
	ulimit -Sn 16000
ifeq ($(OS),OSX)
	cd test && DYLD_LIBRARY_PATH=..:/usr/local/lib ./$@
else
	cd test && LD_LIBRARY_PATH=..:/usr/local/lib ./$@
endif

valgrind: test/test.c
	$(CC) $< -o test/$@ -L. -lsqlite3
	ulimit -Sn 16000
ifeq ($(OS),OSX)
	cd test && DYLD_LIBRARY_PATH=..:/usr/local/lib valgrind --leak-check=full --show-leak-kinds=all ./test
else
	cd test && LD_LIBRARY_PATH=..:/usr/local/lib valgrind --leak-check=full --show-leak-kinds=all ./test
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

tests: test.o seatest.o sqlite3dbg.o
	gcc -g $^ -o $@ -lbinn -luv -ldl
	cp tests test/tests
	cd test && ./tests

test.o: test/test.c
	gcc -c $< -o $@

seatest.o: ../common/seatest.c
	gcc -c $< -o $@

sqlite3dbg.o: sqlite3.c
	gcc -g -DSQLITE_DEBUG=1 -DDEBUGPRINT -DSQLITE_HAS_CODEC -DSQLITE_USE_URI=1 -DSQLITE_THREADSAFE=1 -DHAVE_USLEEP -DSQLITE_ENABLE_COLUMN_METADATA -c $< -o $@

# variables:
#   $@  output
#   $^  all the requirements
#   $<  first requirement
