// Copyright 2010 Google Inc. All Rights Reserved.

#include <barrier.h>
namespace tr1 = std::tr1;

namespace gcl {

barrier::barrier(size_t num_threads) throw (std::invalid_argument)
    : num_to_block_left_(num_threads),
      num_to_exit_(num_threads),
      function_(NULL) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads must be > 0");
  }
}

barrier::barrier(size_t num_threads, tr1::function<void()> function)
    throw (std::invalid_argument)
    : num_to_block_left_(num_threads),
      num_to_exit_(num_threads),
      function_(function) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads must be > 0");
  }
}

bool barrier::await()  throw (std::logic_error) {
  unique_lock<mutex> ul(lock_);
  if (num_to_block_left_ == 0) {
    throw std::logic_error("called too many times.");
  }
  if (--num_to_block_left_ > 0) {
    while (num_to_block_left_ != 0) {
      cv_.wait(ul);
    }
  } else {
    if (function_ != NULL) {
      function_();
    }
    cv_.notify_all();
  }
  num_to_exit_--;
  return (num_to_exit_ == 0);
}

}  // End namespace gcl
