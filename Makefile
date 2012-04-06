# Sample command lines:
#
# Build just the libraries:
#   $ make
#
# Build and run the tests:
#   $ make test
#
# Build and test with g++-4.4 in C++98 mode:
#   $ make test CXX=g++-4.4 CXXFLAGS=-std=c++98
#
# Build and test with g++-4.4 in C++0x mode:
#   $ make test CXX=g++-4.4 CXXFLAGS=-std=c++0x
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
LdLibs := -pthread $(LDLIBS)


all: std_thread.a

clean:
	find * -name '*.a' -o -name '*.o' -o -name '*.d' | xargs $(RM)
	$(RM) AllTests

test: AllTests
	./AllTests

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

STD_THREAD_OBJS := src/thread.o src/mutex.o src/condition_variable.o
std_thread.a: CppFlags += -Iinclude
std_thread.a: $(STD_THREAD_OBJS)

TEST_OBJS := testing/thread_test.o testing/lock_test.o testing/race_test.o \
		testing/concurrent_priority_queue_test.o
AllTests: CppFlags += -Iinclude $(GTEST_I) $(GMOCK_I)
AllTests: $(TEST_OBJS) std_thread.a $(GMOCK_MAIN_A)
	$(CXX) -o $@ $(LdFlags) $^ $(LOADLIBES) $(LdLibs)


%.a:
	$(RM) $@
	$(AR) -rc $@ $^

# Automatically rebuild when header files change:
DEPEND_OPTIONS = -MMD -MP -MF "$*.d.tmp"
MOVE_DEPENDFILE = then mv -f "$*.d.tmp" "$*.d"; \
                  else $(RM) "$*.d.tmp"; exit 1; fi
%.o: %.cc
	if $(CXX) -o $@ -c $(CppFlags) $(DEPEND_OPTIONS) $(CxxFlags) $< ; \
		$(MOVE_DEPENDFILE)

# Include the generated dependency makefiles.
ALL_SOURCE_BASENAMES := $(basename $(shell find * -name "*.c" -o -name "*.cc"))
-include $(ALL_SOURCE_BASENAMES:%=%.d)
