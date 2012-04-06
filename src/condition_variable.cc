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
  handle_err_return(pthread_cond_wait(native_handle(),
                                      lock.mutex()->native_handle()));
}
