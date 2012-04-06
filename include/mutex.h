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

#ifndef STD_MUTEX_
#define STD_MUTEX_

#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include "system_error.h"

class condition_variable;

namespace MutexInternal {
struct MutexMonitor;
class _posix_mutex {
 public:
  _posix_mutex(int mutex_type);
  ~_posix_mutex();

  void lock();
  bool try_lock();
  void unlock();

  friend class ::condition_variable;
 private:
  // Contains either the normal native handle or the test handle. See
  // mutex.cc and test_mutex.cc.
  union handle {
    pthread_mutex_t native_handle;
    MutexMonitor* monitor;
  };
  handle handle_;
  
  typedef pthread_mutex_t* native_handle_type;
  native_handle_type native_handle() {
    return &handle_.native_handle;
  }

  MutexMonitor* monitor() {
    return handle_.monitor;
  }
};
}  // namespace MutexInternal

class recursive_mutex : public MutexInternal::_posix_mutex {
 public:
  recursive_mutex() :
      MutexInternal::_posix_mutex(PTHREAD_MUTEX_RECURSIVE) {
  }

 private:
  // Deleted.
  recursive_mutex(const recursive_mutex&);
  recursive_mutex& operator=(const recursive_mutex&);
};

class mutex : public MutexInternal::_posix_mutex {
 public:
  mutex() : MutexInternal::_posix_mutex(PTHREAD_MUTEX_DEFAULT) {
  }

 private:
  // Deleted.
  mutex(const mutex&);
  mutex& operator=(const mutex&);
};

// Locks, from 30.4.3

struct defer_lock_t { }; // do not acquire ownership of the mutex
struct try_to_lock_t { }; // try to acquire ownership of the mutex
                          // without blocking
struct adopt_lock_t { }; // assume the calling thread has already
                         // obtained mutex ownership and manage it
extern const defer_lock_t defer_lock;
extern const try_to_lock_t try_to_lock;
extern const adopt_lock_t adopt_lock;

template <class Mutex>
class lock_guard {
public:
  typedef Mutex mutex_type;
  explicit lock_guard(mutex_type& m) : pm(m) {
    pm.lock();
  }

  lock_guard(mutex_type& m, adopt_lock_t) : pm(m) {}

  ~lock_guard() {
    pm.unlock();
  }

private:
  // Deleted:
  lock_guard(lock_guard const&);
  lock_guard& operator=(lock_guard const&);

  mutex_type& pm;
};

template <class Mutex>
class unique_lock {
public:
  typedef Mutex mutex_type;

  unique_lock() : pm(NULL), owns(false) {}

  explicit unique_lock(mutex_type& m) : pm(&m) {
    pm->lock();
    owns = true;
  }

  unique_lock(mutex_type& m, defer_lock_t) : pm(&m), owns(false) {}

  unique_lock(mutex_type& m, try_to_lock_t) : pm(&m) {
    owns = pm->try_lock();
  }

  unique_lock(mutex_type& m, adopt_lock_t) : pm(&m), owns(true) {}

  ~unique_lock() {
    release();
  }

  // 30.4.3.2.2 locking
  void lock() {
    if (pm == NULL) {
      throw std::system_error(std::make_error_code(
                                std::errc::operation_not_permitted));
    }
    pm->lock();
    owns = true;
  }

  bool try_lock() {
    if (pm == NULL) {
      throw std::system_error(std::make_error_code(
                                std::errc::operation_not_permitted));
    }
    owns = pm->try_lock();
    return owns;
  }

  void unlock() {
    if (!owns) {
      throw std::system_error(std::make_error_code(
                                std::errc::operation_not_permitted));
    }
    owns = false;
    pm->unlock();
  }

  // 30.4.3.2.3 modiﬁers
  void swap(unique_lock& u) {
    std::swap(pm, u.pm);
    std::swap(owns, u.owns);
  }

  mutex_type *release() {
    if (owns)
      unlock();
    mutex_type *result = pm;
    pm = NULL;
    return result;
  }

  // 30.4.3.2.4 observers
  bool owns_lock() const { return owns; }
  mutex_type* mutex() const { return pm; }

private:
  // Deleted:
  unique_lock(unique_lock const&);
  unique_lock& operator=(unique_lock const&);

  mutex_type* pm;
  bool owns;
};

#endif  // STD_MUTEX_
