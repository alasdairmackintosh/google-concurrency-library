#ifndef STD_MUTEX_
#define STD_MUTEX_

#include <assert.h>
#include <pthread.h>
#include <errno.h>

class mutex {
public:
  mutex();
  ~mutex();

  void lock() {
    assert(pthread_mutex_lock(&native_handle_) == 0);
  }

  bool try_lock() {
    int result = pthread_mutex_trylock(&native_handle_);
    if (result == 0)
      return true;
    else if (result == EBUSY)
      return false;
    else
      abort();
  }

  void unlock() {
    assert(pthread_mutex_unlock(&native_handle_) == 0);
  }

  typedef pthread_mutex_t* native_handle_type;
  native_handle_type native_handle() { return &native_handle_; }

private:
  // Deleted.
  mutex(const mutex&);
  mutex& operator=(const mutex&);

  pthread_mutex_t native_handle_;
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

#endif  // STD_MUTEX_
