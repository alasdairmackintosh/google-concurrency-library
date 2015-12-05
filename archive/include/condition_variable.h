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

#ifndef STD_CONDITION_VARIABLE_
#define STD_CONDITION_VARIABLE_

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include "system_error.h"
#include "mutex.h"

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
