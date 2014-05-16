# Copyright 2011 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

######## General Configuration

#### Source Locations

VPATH = ../src ../testing \
	../third_party/googletest/src ../third_party/googlemock/src

#### Compilation Flags

DFlags = -MMD -MP
EFlags = -Wall -Werror -Wno-error=c++0x-compat
IFlags = -I. -I../include
BFlags = $(DFlags) $(EFlags) $(IFlags) $(BFLAGS)
CFlags = $(BFlags) $(CFLAGS)
CxxFlags = $(BFlags) $(CXXFLAGS)
LFlags = -pthread $(CXXFLAGS)

#### Compilation Rules

%.o : %.c
	$(CC) $(CFlags) $< -c -o $*.o

%.o : %.cc
	$(CXX) $(CxxFlags) $< -c -o $*.o

%.a :
	/bin/rm -f $@
	/usr/bin/ar -rc $@ $^

%.exe :
	$(CXX) $^ $(LFlags) -o $*.exe

%.pass : %.exe
	sh ../util/exetest.sh $*

-include *.d

######## Source File Generation

generate: atomic.h atomic_c_test.c

# The atomic header has a lot of redundancy, so it is generated.
atomic.h : ../util/atomic.sh
	/bin/bash ../util/atomic.sh > atomic.h

# The atomic test is the same for both C++ and C, so copy to the latter.
atomic_c_test.c : ../testing/atomic_cpp_test.cc
	cp ../testing/atomic_cpp_test.cc atomic_c_test.c

######## Library Build

build : stdfake.a lower.a

stdfake.a : atomic.o mutex.o condition_variable.o thread.o

lower.a : stream_mutex.o countdown_latch.o latch.o serial_executor.o barrier.o \
	mutable_thread.o simple_thread_pool.o debug.o notifying_barrier.o

######## Testing

test : cxx11_test.o dynarray_test.pass \
        atomic_c_test.pass atomic_cpp_test.pass counter_test.pass \
	stdfake_test.pass lower_test.pass mutex_test.pass higher_test.pass \
	pipeline_test.pass queue_perf_test.exe lock_free_buffer_queue_test.pass \
	scoped_guard_test.pass

#### Simple Tests

dynarray_test.exe : dynarray_test.o stdfake.a
dynarray_test.pass : dynarray_test.exe

atomic_c_test.exe : atomic_c_test.o stdfake.a
atomic_c_test.pass : atomic_c_test.exe

atomic_cpp_test.exe : atomic_cpp_test.o stdfake.a
atomic_cpp_test.pass : atomic_cpp_test.exe

counter_test.exe : counter_test.o stdfake.a
counter_test.pass : counter_test.exe

#### GTest GMock Builds

GTEST_INC := -I../third_party/googletest/include
GTEST_SRC := -I../third_party/googletest
GTEST_OBJ := gtest-all.o gtest_main.o
$(GTEST_OBJ) : CxxFlags += $(GTEST_INC) $(GTEST_SRC)

GMOCK_INC := -I../third_party/googlemock/include
GMOCK_SRC := -I../third_party/googlemock
GMOCK_OBJ := gtest-all.o gmock-all.o gmock_main.o
$(GMOCK_OBJ) : CxxFlags += $(GTEST_INC) $(GTEST_SRC) $(GMOCK_INC) $(GMOCK_SRC)

#### Tests for the Fake Standard Library

STDFAKE_TESTS := lock_test.o thread_test.o
$(STDFAKE_TESTS) : CxxFlags += $(GTEST_INC) $(GMOCK_INC)
stdfake_test.exe : $(STDFAKE_TESTS) $(GMOCK_OBJ) stdfake.a
stdfake_test.pass : stdfake_test.exe

#### Test for the Google Concurrency Library Proper

LOWER_TESTS := barrier_test.o notifying_barrier_test.o serial_executor_test.o \
	simple_thread_pool_test.o latch_test.o mutable_thread_test.o
$(LOWER_TESTS) : CxxFlags += $(GTEST_INC) $(GMOCK_INC)
lower_test.exe : $(LOWER_TESTS) $(GMOCK_OBJ) lower.a stdfake.a
lower_test.pass : lower_test.exe

MUTEX_TESTS := test_mutex_test.o condition_variable_test.o lock_test.o \
	race_test.o concurrent_priority_queue_test.o blockable_thread.o \
	test_mutex.o
$(MUTEX_TESTS) : CxxFlags += $(GTEST_INC) $(GMOCK_INC)
mutex_test.exe : $(MUTEX_TESTS) $(GMOCK_OBJ) lower.a stdfake.a
mutex_test.pass : mutex_test.exe

HIGHER_TESTS := source_test.o iterator_queue_test.o \
	stream_mutex_test.o buffer_queue_test.o
$(HIGHER_TESTS) : CxxFlags += $(GTEST_INC) $(GMOCK_INC)
higher_test.exe : $(HIGHER_TESTS) $(GMOCK_OBJ) lower.a stdfake.a
higher_test.pass : higher_test.exe

LOCK_FREE_BUFFER_QUEUE_TESTS := lock_free_buffer_queue_test.o
$(LOCK_FREE_BUFFER_QUEUE_TESTS) : CxxFlags += $(GTEST_INC) $(GMOCK_INC)
lock_free_buffer_queue_test.exe : $(LOCK_FREE_BUFFER_QUEUE_TESTS) $(GMOCK_OBJ) \
    lower.a stdfake.a
lock_free_buffer_queue_test.pass : lock_free_buffer_queue_test.exe

MAP_REDUCE_TESTS := map_reduce_test.o
$(MAP_REDUCE_TESTS) : CxxFlags += $(GTEST_INC) $(GMOCK_INC)
map_reduce_test.exe : $(MAP_REDUCE_TESTS) $(GMOCK_OBJ) lower.a stdfake.a
map_reduce_test.run : map_reduce_test.exe
	./map_reduce_test.exe

PIPELINE_TESTS := pipeline_test.o
$(PIPELINE_TESTS) : CxxFlags += $(GTEST_INC) $(GMOCK_INC)
pipeline_test.exe : $(PIPELINE_TESTS) $(GMOCK_OBJ) lower.a stdfake.a
pipeline_test.pass : pipeline_test.exe

QUEUE_PERF_TESTS := queue_perf_test.o
$(QUEUE_PERF_TESTS) : CxxFlags += $(GTEST_INC)
queue_perf_test.exe : $(QUEUE_PERF_TESTS) lower.a stdfake.a
queue_perf_test.run : queue_perf_test.exe
	./queue_perf_test.exe

SCOPED_GUARD_TESTS := scoped_guard_test.o
$(SCOPED_GUARD_TESTS) : CxxFlags += $(GTEST_INC) $(GMOCK_INC)
scoped_guard_test.exe : $(SCOPED_GUARD_TESTS) $(GMOCK_OBJ)
scoped_guard_test.pass : scoped_guard_test.exe
