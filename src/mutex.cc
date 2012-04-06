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
  handle_err_return(pthread_mutex_init(&native_handle_, &mutex_attribute));
  handle_err_return(pthread_mutexattr_destroy(&mutex_attribute));
}

_posix_mutex::~_posix_mutex() {
  handle_err_return(pthread_mutex_destroy(&native_handle_));
}
}
