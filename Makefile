# Sample command lines:
#
# Build just the libraries:
#   $ make
#
# Build and run the tests:
#   $ make test
#
# Build and test with g++-4.4 in C++98 mode:
#   $ make test CC=gcc-4.4 CXX=g++-4.4 CXXFLAGS=-std=c++98
#
# Build and test with g++-4.4 in C++0x mode:
#   $ make test CC=gcc-4.4 CXX=g++-4.4 CXXFLAGS=-std=c++0x
#
# Build optimized libraries:
#   $ make ALLCFLAGS=-O3
#
# Remove all build output. Remember to do this when changing compilers or flags.
#   $ make clean

# Put the user's flags after the defaults, so the user can override
# the defaults.
CppFlags := $(CPPFLAGS)
AllCFlags := -pthread -Wall -Werror -g $(ALLCFLAGS)
CxxFlags := $(AllCFlags) $(CXXFLAGS)
CFlags := $(AllCFlags) $(CFLAGS)
LdFlags := $(LDFLAGS)
LdLibs := -pthread $(LDLIBS)


all: std_atomic.a std_thread.a

clean:
	find * -name '*.a' -o -name '*.o' -o -name '*.d' | xargs $(RM)
	$(RM) Tests NativeTests

AllTests: Tests NativeTests RawTests CompileTests

test: AllTests
	./Tests
	./NativeTests

GTEST_I := -Ithird_party/googletest/include
gtest.a gtest_main.a: CppFlags += -Ithird_party/googletest $(GTEST_I)
gtest.a: third_party/googletest/src/gtest-all.o
gtest_main.a: third_party/googletest/src/gtest_main.o
GTEST_MAIN_A := gtest_main.a gtest.a

GMOCK_I := -Ithird_party/googlemock/include
gmock.a gmock_main.a: CppFlags += -Ithird_party/googlemock $(GMOCK_I) $(GTEST_I)
gmock.a: third_party/googlemock/src/gmock-all.o
gmock_main.a: third_party/googlemock/src/gmock_main.o
GMOCK_A := gtest.a gmock.a
GMOCK_MAIN_A := gmock_main.a $(GMOCK_A)

# The std_atomic library.
include/atomic.h : util/atomic.sh
	sh util/atomic.sh > include/atomic.h

src/atomic.o : include/atomic.h
STD_ATOMIC_OBJS := src/atomic.o
std_atomic.a: CppFlags += -Iinclude
std_atomic.a: $(STD_ATOMIC_OBJS)

# The atomic tests.  The C and C++ tests use the same source.
# The C-like portions should be acceptable to both C and C++.
testing/atomic_c_test.c : testing/atomic_cpp_test.cc
	cp testing/atomic_cpp_test.cc testing/atomic_c_test.c

testing/atomic_c_test.o : include/atomic.h
STD_ATOMIC_C_TEST_OBJS := testing/atomic_c_test.o
atomic_c_test: CFlags += -Iinclude
atomic_c_test: $(STD_ATOMIC_C_TEST_OBJS) std_atomic.a
	$(CC) -o $@ $(LdFlags) $^ $(LOADLIBES) $(LdLibs)

testing/atomic_cpp_test.o : include/atomic.h
STD_ATOMIC_CPP_TEST_OBJS := testing/atomic_cpp_test.o
atomic_cpp_test: CppFlags += -Iinclude
atomic_cpp_test: $(STD_ATOMIC_CPP_TEST_OBJS) std_atomic.a
	$(CXX) -o $@ $(LdFlags) $^ $(LOADLIBES) $(LdLibs)

# All of the components of the std_thread library, apart from the 
# mutex/condition_variable
STD_THREAD_OBJS := src/thread.o src/countdown_latch.o src/serial_executor.o \
    src/barrier.o src/mutable_thread.o src/simple_thread_pool.cc
std_thread.a: CppFlags += -Iinclude
std_thread.a: $(STD_THREAD_OBJS)

# Normal implementations of mutex and condition variable
STD_MUTEX_OBJS := src/mutex.o src/condition_variable.o
std_mutex.a: CppFlags += -Iinclude
std_mutex.a: $(STD_MUTEX_OBJS)

# Test implementations of mutex and condition variable
TEST_MUTEX_OBJS := src/test_mutex.o
test_mutex.a: CppFlags += -Iinclude
test_mutex.a: $(TEST_MUTEX_OBJS)

# Native tests that use the standard mutex clases rather than the test
# mutex classes. These are for testing the normal
# mutex/condition_variable classes.
STD_MUTEX_TEST_OBJS := testing/thread_test.o testing/lock_test.o \
    testing/blocking_queue_test.o testing/serial_executor_test.o \
    testing/source_test.o testing/barrier_test.o
NativeTests: CppFlags += -Iinclude $(GTEST_I) $(GMOCK_I)
NativeTests: $(STD_MUTEX_TEST_OBJS) std_thread.a std_mutex.a std_atomic.a \
             $(GMOCK_MAIN_A)
	$(CXX) -o $@ $(LdFlags) $^ $(LOADLIBES) $(LdLibs)

# Tests that rely on the test mutex classes.
TEST_MUTEX_TEST_OBJS := testing/test_mutex_test.o \
    testing/condition_variable_test.o testing/lock_test.o testing/race_test.o \
    testing/concurrent_priority_queue_test.o testing/blockable_thread.o \
    testing/blocking_queue_closed_test.o
Tests: CppFlags += -Iinclude -Itesting $(GTEST_I) $(GMOCK_I)
Tests: $(TEST_MUTEX_TEST_OBJS) std_thread.a test_mutex.a std_atomic.a \
       $(GMOCK_MAIN_A)
	$(CXX) -o $@ $(LdFlags) $^ $(LOADLIBES) $(LdLibs)

# The counter tests.
COUNTER_TEST_OBJS := testing/counter_test.o
counter_test: CppFlags += -Iinclude
counter_test: $(COUNTER_TEST_OBJS) std_atomic.a std_mutex.a
	$(CXX) -o $@ $(LdFlags) $^ $(LOADLIBES) $(LdLibs)

RawTests: atomic_c_test atomic_cpp_test counter_test
	./atomic_c_test
	./atomic_cpp_test
	./counter_test

CompileTests: CppFlags += -Iinclude
CompileTests: testing/cxx0x_test.o

%.a:
	$(RM) $@
	$(AR) -rc $@ $^

# Automatically rebuild when header files change:
DEPEND_OPTIONS = -MMD -MP -MF "$*.d.tmp"
MOVE_DEPENDFILE = then mv -f "$*.d.tmp" "$*.d"; \
                  else $(RM) "$*.d.tmp"; exit 1; fi

%.o: %.c
	if $(CC) -o $@ -c $(CppFlags) $(DEPEND_OPTIONS) $(CFlags) $< ; \
		$(MOVE_DEPENDFILE)

%.o: %.cc
	if $(CXX) -o $@ -c $(CppFlags) $(DEPEND_OPTIONS) $(CxxFlags) $< ; \
		$(MOVE_DEPENDFILE)

# Include the generated dependency makefiles.
ALL_SOURCE_BASENAMES := $(basename $(shell find * -name "*.c" -o -name "*.cc"))
-include $(ALL_SOURCE_BASENAMES:%=%.d)
