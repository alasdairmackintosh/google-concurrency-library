#ifndef STD_CONDITION_VARIABLE_
#define STD_CONDITION_VARIABLE_

#include "mutex.h"
#include "posix_errors.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

class condition_variable {
public:
  condition_variable();
  ~condition_variable();

  void notify_one() {
    handle_err_return(pthread_cond_signal(native_handle()));
  }

  void notify_all() {
    handle_err_return(pthread_cond_broadcast(native_handle()));
  }

  void wait(unique_lock<mutex>& lock) {
    handle_err_return(pthread_cond_wait(native_handle(),
                                        lock.mutex()->native_handle()));
  }

  template<class Predicate>
  void wait(unique_lock<mutex>& lock, Predicate pred) {
    while (!pred()) {
      wait(lock);
    }
  }

  typedef pthread_cond_t* native_handle_type;
  native_handle_type native_handle() { return &native_handle_; }

private:
  // Deleted.
  condition_variable(const condition_variable&);
  condition_variable& operator=(const condition_variable&);

  pthread_cond_t native_handle_;
};

#endif  // STD_CONDITION_VARIABLE_
