#ifndef STD_CONDITION_VARIABLE_
#define STD_CONDITION_VARIABLE_

#include "mutex.h"
#include "posix_errors.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

struct ConditionMonitor;
class condition_variable {
public:
  condition_variable();
  ~condition_variable();

  void notify_one();
  void notify_all();

  void wait(unique_lock<mutex>& lock);

  template<class Predicate>
  void wait(unique_lock<mutex>& lock, Predicate pred) {
    while (!pred()) {
      wait(lock);
    }
  }

private:
  // Contains either the normal native handle or the test handle. See
  // condition_variable.cc and test_mutex.cc.
  union handle {
    pthread_cond_t native_handle;
    ConditionMonitor* monitor;
  };
  handle handle_;

  typedef pthread_cond_t* native_handle_type;
  native_handle_type native_handle() {
    return &(handle_.native_handle);
  }

  ConditionMonitor* monitor() {
    return handle_.monitor;
  }

  // Deleted.
  condition_variable(const condition_variable&);
  condition_variable& operator=(const condition_variable&);
};

#endif  // STD_CONDITION_VARIABLE_
