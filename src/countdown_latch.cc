// Copyright 2009 Google Inc. All Rights Reserved.

#include "countdown_latch.h"

#include "mutex.h"
#include "condition_variable.h"

namespace gcl {

countdown_latch::countdown_latch(unsigned int count)
 : count_(count) {
}

void countdown_latch::wait() {
  unique_lock<mutex> lock(condition_mutex_);
  while(count_ > 0) {
    condition_.wait(lock);
  }
}

void countdown_latch::count_down() {
  lock_guard<mutex> lock(condition_mutex_);
  if (count_ <= 0) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }
  if (--count_ == 0) {
    condition_.notify_all();
  }
}

}  // End namespace gcl
