// Copyright 2010 Google Inc. All Rights Reserved.

#ifndef GCL_BARRIER_
#define GCL_BARRIER_

#include <stddef.h>
#include <stdexcept>
#include <tr1/functional>

#include <condition_variable.h>
#include <mutex.h>

namespace tr1 = std::tr1;

namespace gcl {

// Allows a set of threads to wait until all threads have reached a
// common point.
class barrier {
 public:
  // Creates a new barrier that will block until num_threads threads
  // are waiting on it. Throws invalid_argument if num_threads == 0.
  explicit barrier(size_t num_threads) throw (std::invalid_argument);

  // Creates a new barrier that will block until num_threads threads
  // are waiting on it. When the barrier is released, function will
  // be invoked. Throws invalid_argument if num_threads == 0.
  barrier(size_t num_threads, tr1::function<void()> function)
      throw (std::invalid_argument);

  // Blocks until num_threads have call await(). Invokes the function
  // specified in the constructor before releasing any thread. Returns
  // true to one of the callers, and false to the others, allowing one
  // caller to delete the barrier.
  //
  // Memory ordering: For threads X and Y that call await(), the call
  // to await() in X happens before the return from await() in Y.
  //
  // A logic_error will be thrown if more than num_threads call await().
  bool await() throw (std::logic_error);

 private:
  mutex lock_;
  size_t num_to_block_left_;
  size_t num_to_exit_;
  condition_variable cv_;
  tr1::function<void()> function_;
};
}
#endif // GCL_BARRIER_
