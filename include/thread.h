// Copyright 2010 Google Inc. All Rights Reserved.
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

#ifndef STD_THREAD_
#define STD_THREAD_

#include <pthread.h>
#include <string.h>
#include <ostream>

#include "functional.h"

// This is a really simple subset of C++11's std::thread (defined in
// 30.3.1), that we'll use just for testing until we import or write a
// more complete version.
class thread {
 public:
  template<class F> explicit thread(F f);
  ~thread();

  void join();
  void detach();
  // Represents the unique ID of a thread.
  class id;
  thread::id get_id() const;
 private:
  // Deleted
  thread(const thread&);
  void operator=(const thread&);

  void start(std::function<void()> start_func);

  pthread_t native_handle_;
  bool joinable_;
};

template<class F>
thread::thread(F f) {
    start(f);
}

namespace chrono {
struct milliseconds {
  long long num;
  explicit milliseconds(long long num) : num(num) {}
};
}

namespace this_thread {
void sleep_for(const chrono::milliseconds& delay);
thread::id get_id();
}

class thread::id {
 public:
  // Constructs an invalid ID. No valid executing thread will have this ID.
  id();

  // Default copy constructor and assignment operators
 private:
  friend bool operator==(id, id);
  friend bool operator<(id, id);
  template<class charT, class traits>
  friend std::basic_ostream<charT, traits>& operator<<(
      std::basic_ostream<charT, traits>& stream, id id);
  friend thread::id this_thread::get_id();
  friend class thread;
  id(pthread_t id);
  unsigned long long_value() const;
  pthread_t id_;
  bool is_null_;
};

bool operator==(thread::id x, thread::id y);
bool operator!=(thread::id x, thread::id y);
bool operator<(thread::id x, thread::id y);
bool operator<=(thread::id x, thread::id y);
bool operator>(thread::id x, thread::id y);
bool operator>=(thread::id x, thread::id y);

template<class charT, class traits>
std::basic_ostream<charT, traits>& operator<<(
    std::basic_ostream<charT, traits>& stream, thread::id id) {
  if (id.is_null_) {
    stream << "null thread";
  } else {
    // TODO(alasdair): we only use this for EXPECT_EQ macros in testing,
    // but we should have a better implementation.
    if (sizeof(id.id_) == sizeof(unsigned long)) {
      unsigned long tid;
      memcpy(&tid, &id.id_, sizeof(unsigned long));
      stream << std::hex << tid << std::dec;
    } else {
      stream << "???";
    }
  }
  return stream;
}

#endif  // STD_THREAD_
