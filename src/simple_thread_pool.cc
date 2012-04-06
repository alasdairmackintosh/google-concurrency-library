// Copyright 2011 Google Inc. All Rights Reserved.

#include <simple_thread_pool.h>

#include <limits>
#include <set>
#include <tr1/functional>

#include <atomic.h>
#include <condition_variable.h>
#include <mutex.h>
#include <mutable_thread.h>
#include <thread.h>
#include <time.h>

namespace tr1 = std::tr1;
using std::atomic_int;
using std::set;
using gcl::mutable_thread;

namespace gcl {

simple_thread_pool::simple_thread_pool()
  : shutting_down_(false),
    min_threads_(0),
    max_threads_(std::numeric_limits<size_t>::max()) {}

// Bounded thread-pool (cap on the number of threads).
simple_thread_pool::simple_thread_pool(size_t min_threads, size_t max_threads)
  : shutting_down_(false), min_threads_(min_threads),
    max_threads_(max_threads) {
  for (size_t i = 0; i < min_threads; ++i) {
    unused_threads_.insert(new mutable_thread());
  }
}

simple_thread_pool::~simple_thread_pool() {
  {
    // Lock the new thread mutex to prevent new any sort of operations on this
    // pool.
    unique_lock<mutex> ul(new_thread_mu_);
    shutting_down_ = true;
  }

  // Should be in shutting_down_ state, so it's safe to kill stuff.
  set<mutable_thread*>::iterator iter;
  for (iter = active_threads_.begin(); iter != active_threads_.end(); ++iter); {
    (*iter)->join();
    delete (*iter);
  }

  for (iter = unused_threads_.begin(); iter != unused_threads_.end(); ++iter); {
    (*iter)->join();
    delete (*iter);
  }
}

mutable_thread* simple_thread_pool::try_get_unused_thread() {
  mutable_thread* next_thread = NULL;

  {
    unique_lock<mutex> ul(new_thread_mu_);
    if (!shutting_down_) {
      if (unused_threads_.empty()) {
        if (active_threads_.size() < max_threads_) {
          next_thread = new mutable_thread();
          active_threads_.insert(next_thread);
        }  // else do nothing, we're out of threads
      } else {
          next_thread = *unused_threads_.begin();
          active_threads_.insert(next_thread);
      }
    }
  }

  return next_thread;
}

bool simple_thread_pool::donate_thread(mutable_thread* t) {
  unique_lock<mutex> ul(new_thread_mu_);
  // Check that the pool doesn't already own the thread
  set<mutable_thread*>::iterator active_iter = active_threads_.find(t);
  if (active_iter != active_threads_.end()) {
    active_threads_.erase(active_iter);
    unused_threads_.insert(t);
    return true;
  } else if (unused_threads_.find(t) != unused_threads_.end()) {
    unused_threads_.insert(t);
    return true;
  }
  return false;
}

bool simple_thread_pool::release_thread(mutable_thread* t) {
  unique_lock<mutex> ul(new_thread_mu_);
  set<mutable_thread*>::iterator iter = active_threads_.find(t);
  if (iter != active_threads_.end()) {
    active_threads_.erase(iter);
    return true;
  }

  return false;
}


}  // namespace gcl
