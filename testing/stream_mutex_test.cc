// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <sstream>

#include "thread.h"

#include "stream_mutex.h"

#include "gmock/gmock.h"

std::ostringstream stream;
stream_mutex<std::ostream> mstream(stream);

// Test implicit expression locking.
void implicit()
{
    mstream << "1" << "2" << "3" << "4" << "5" << std::endl;
}

// Test explicit expression locking.
void holding()
{
    mstream.hold() << "1" << "2" << "3" << "4" << "5" << std::endl;
}


// Test explicit block locking.
void block()
{
    stream_guard<std::ostream> gstream(mstream);
    gstream << "1";
    gstream << "2";
    gstream << "3";
    gstream << "4";
    gstream << "5";
    gstream << std::endl;
}

// Test with lock_guard.
// The above code is a better approach, but this one must work.
void glock()
{
    lock_guard<stream_mutex<std::ostream> > lck(mstream);
    mstream.bypass() << "1";
    mstream.bypass() << "2";
    mstream.bypass() << "3";
    mstream.bypass() << "4";
    mstream.bypass() << "5";
    mstream.bypass() << std::endl;
}

// Test with recursive locking.
// Each write has automatic locking, in addition to the lock_guard.
// This double locking is not recommended, but it must work.
void rlock()
{
    lock_guard<stream_mutex<std::ostream> > lck(mstream);
    mstream.hold() << "1" << "2" << "3";
    mstream << "4" << "5" << std::endl;
}

// Test with unique_lock.
void ulock()
{
    unique_lock<stream_mutex<std::ostream> > lck(mstream, defer_lock);
    lck.lock();
    mstream.bypass() << "1";
    mstream.bypass() << "2";
    mstream.bypass() << "3";
    mstream.bypass() << "4";
    mstream.bypass() << "5";
    mstream.bypass() << std::endl;
}

// Test with try_lock.
void trylock()
{
    unique_lock<stream_mutex<std::ostream> > lck(mstream, defer_lock);
    if ( lck.try_lock() ) {
        mstream.bypass() << "1";
        mstream.bypass() << "2";
        mstream.bypass() << "3";
        mstream.bypass() << "4";
        mstream.bypass() << "5";
        mstream.bypass() << std::endl;
    }
}

const int limit = 1000;

template <void Function()>
void writer()
{
    for ( int i = 0; i < limit; ++i )
        Function();
}

void verify()
{
    const std::string result(stream.str());
    int len = result.length();
    int idx = 0;
    while ( idx < len ) {
        ASSERT_EQ('1', result[idx]); ++idx;
        ASSERT_EQ('2', result[idx]); ++idx;
        ASSERT_EQ('3', result[idx]); ++idx;
        ASSERT_EQ('4', result[idx]); ++idx;
        ASSERT_EQ('5', result[idx]); ++idx;
        ASSERT_EQ('\n', result[idx]); ++idx;
    }
}

class LockStreamTest
:
    public testing::Test
{
};

// Verifies that we cannot create a queue of size zero
TEST_F(LockStreamTest, Sequential) {
    implicit();
    holding();
    block();
    glock();
    rlock();
    ulock();
    trylock();
    verify();
}

// Verify threaded operation
TEST_F(LockStreamTest, Parallel) {
    thread thr1(writer<implicit>);
    thread thr2(writer<holding>);
    thread thr3(writer<block>);
    thread thr4(writer<glock>);
    thread thr5(writer<rlock>);
    thread thr6(writer<ulock>);
    thread thr7(writer<trylock>);
    thr1.join();
    thr2.join();
    thr3.join();
    thr4.join();
    thr5.join();
    thr6.join();
    thr7.join();
    verify();
}
