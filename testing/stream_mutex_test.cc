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

#include <fstream>
#include <sstream>

#include "thread.h"

#include "stream_mutex.h"

#include "gmock/gmock.h"

// Compilation test for the streaming of non-copyable objects.

class non_copyable
{
    non_copyable(const non_copyable&) CXX11_DELETED
public:
    non_copyable() { }
};

std::ostream& operator <<(std::ostream& os, const non_copyable& )
{
    return os << "non-copyable";
}

void verify_non_copyable_locked_streaming(stream_mutex<std::ostream>& mstream)
{
    non_copyable var;
    mstream << var;
}

// Test implicit expression locking.
void implicit(stream_mutex<std::ostream>& mstream)
{
    mstream << "1" << "2" << "3" << "4" << "5" << std::endl;
}

// Test explicit expression locking.
void holding(stream_mutex<std::ostream>& mstream)
{
    mstream.hold() << "1" << "2" << "3" << "4" << "5" << std::endl;
}


// Test explicit block locking.
void block(stream_mutex<std::ostream>& mstream)
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
void glock(stream_mutex<std::ostream>& mstream)
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
void rlock(stream_mutex<std::ostream>& mstream)
{
    lock_guard<stream_mutex<std::ostream> > lck(mstream);
    mstream.hold() << "1" << "2" << "3";
    mstream << "4" << "5" << std::endl;
}

// Test with unique_lock.
void ulock(stream_mutex<std::ostream>& mstream)
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
void trylock(stream_mutex<std::ostream>& mstream)
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

std::ostringstream stream;

template <void Function(stream_mutex<std::ostream>& sm)>
void writer()
{
    stream_mutex<std::ostream> mostrs(stream);
    for ( int i = 0; i < limit; ++i )
        Function(mostrs);
}

void verify(std::istream& is)
{
    std::string s;
    while ( ! is.eof() ) {
        is >> s;
	ASSERT_EQ('1', s[0]);
	ASSERT_EQ('2', s[1]);
	ASSERT_EQ('3', s[2]);
	ASSERT_EQ('4', s[3]);
	ASSERT_EQ('5', s[4]);
    }
}

class LockStreamTest
:
    public testing::Test
{
};

// Verify sequential operation
TEST_F(LockStreamTest, Sequential) {
    stream_mutex<std::ostream> mostrs(stream);
    implicit(mostrs);
    holding(mostrs);
    block(mostrs);
    glock(mostrs);
    rlock(mostrs);
    ulock(mostrs);
    trylock(mostrs);
    const std::string result(stream.str());
    std::istringstream istrs(result);
    verify(istrs);
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
    const std::string result(stream.str());
    std::istringstream istrs(result);
    verify(istrs);
}
