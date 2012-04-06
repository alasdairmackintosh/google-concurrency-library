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

#include <errno.h>

#include "mutex.h"

const defer_lock_t defer_lock = {};
const try_to_lock_t try_to_lock = {};
const adopt_lock_t adopt_lock = {};

namespace MutexInternal {
_posix_mutex::_posix_mutex(int mutex_type) {
  pthread_mutexattr_t mutex_attribute;
  handle_err_return(pthread_mutexattr_init(&mutex_attribute));
  handle_err_return(pthread_mutexattr_settype(&mutex_attribute,
                                              mutex_type));
  handle_err_return(pthread_mutex_init(native_handle(), &mutex_attribute));
  handle_err_return(pthread_mutexattr_destroy(&mutex_attribute));
}

_posix_mutex::~_posix_mutex() {
  handle_err_return(pthread_mutex_destroy(native_handle()));
}

void _posix_mutex::lock() {
  handle_err_return(pthread_mutex_lock(native_handle()));
}

bool _posix_mutex::try_lock() {
  int result = pthread_mutex_trylock(native_handle());
  if (result == 0) {
    return true;
  } else if (result != EBUSY) {
    handle_err_return(result);
  }
  return false;
}

void _posix_mutex::unlock() {
  handle_err_return(pthread_mutex_unlock(native_handle()));
}
}
