// Copyright 2009 Google Inc. All Rights Reserved.
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

#include "condition_variable.h"

condition_variable::condition_variable() {
  handle_err_return(pthread_cond_init(native_handle(), NULL));
}

condition_variable::~condition_variable() {
  handle_err_return(pthread_cond_destroy(native_handle()));
}

void condition_variable::notify_one() {
  handle_err_return(pthread_cond_signal(native_handle()));
}

void condition_variable::notify_all() {
  handle_err_return(pthread_cond_broadcast(native_handle()));
}

void condition_variable::wait(unique_lock<mutex>& lock) {
  int result = pthread_cond_wait(native_handle(),
                                 lock.mutex()->native_handle());
  if (result == EINTR)
    result = 0;
  handle_err_return(result);
}
