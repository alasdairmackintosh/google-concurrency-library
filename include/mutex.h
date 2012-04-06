#ifndef STD_MUTEX_
#define STD_MUTEX_

#include "posix_errors.h"
#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

namespace MutexInternal {
class _posix_mutex {
 public:
  _posix_mutex(int mutex_type);
  ~_posix_mutex();

  void lock() {
    handle_err_return(pthread_mutex_lock(&native_handle_));
  }

  bool try_lock() {
    int result = pthread_mutex_trylock(&native_handle_);
    if (result == 0)
      return true;
    else if (result == EBUSY)
      return false;
    else
      handle_err_return(result);
  }

  void unlock() {
    handle_err_return(pthread_mutex_unlock(&native_handle_));
  }

  typedef pthread_mutex_t* native_handle_type;
  native_handle_type native_handle() { return &native_handle_; }

 private:
  pthread_mutex_t native_handle_;
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
    if (pm == NULL)
      throw std::system_error(std::make_error_code(EPERM));
    pm->lock();
    owns = true;
  }

  bool try_lock() {
    if (pm == NULL)
      throw std::system_error(std::make_error_code(EPERM));
    owns = pm->try_lock();
    return owns;
  }

  void unlock() {
    if (!owns)
      throw std::system_error(std::make_error_code(EPERM));
    owns = false;
    pm->unlock();
  }

  // 30.4.3.2.3 modiﬁers
  void swap(unique_lock& u) {
    using std::swap;
    swap(pm, u.pm);
    swap(owns, u.owns);
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
