#include "condition_variable.h"

condition_variable::condition_variable() {
  handle_err_return(pthread_cond_init(&native_handle_, NULL));
}

condition_variable::~condition_variable() {
  handle_err_return(pthread_cond_destroy(&native_handle_));
}
