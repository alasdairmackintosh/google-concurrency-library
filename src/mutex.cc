#include "mutex.h"

const defer_lock_t defer_lock = {};
const try_to_lock_t try_to_lock = {};
const adopt_lock_t adopt_lock = {};

mutex::mutex() {
  assert(pthread_mutex_init(&native_handle_, NULL) == 0);
}

mutex::~mutex() {
  assert(pthread_mutex_destroy(&native_handle_) == 0);
}
