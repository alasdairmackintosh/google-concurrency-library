// Copyright 2011 Google Inc. All Rights Reserved.

#ifndef GCL_SIMPLE_THREAD_POOL_
#define GCL_SIMPLE_THREAD_POOL_

#include <set>
#include <tr1/functional>

#include <atomic.h>
#include <condition_variable.h>
#include <mutable_thread.h>
#include <mutex.h>
#include <thread.h>

namespace tr1 = std::tr1;
using std::atomic_int;
using std::set;
using gcl::mutable_thread;

namespace gcl {

// Basic thread aggregating class. Acts as a thread factory for mutable-thread
// objects. Threads created are still owned by the thread pool until
// release_thread is called. Threads can also be donated back into the pool to
// allow them to be re-used by other callers.
//
// NOTE: the basic behavior here is to support adding/removing threads with the
// ability to cap the number of threads created as well as to re-use previously
// created threads if available.
//
// TODO: The behavior here is more like that of a thread-factory, so this will
// need to be be better refined to see if there is a good way to queue up work
// into the pool as well as just adding/removing threads from it (current
// thinking is to have a separate "thread manager" class to handle
// re-allocation of threads as well as supporting queued thread pools which
// actually handle task queueing and execution).
class simple_thread_pool {
 public:
  // Unbounded form of the simple_thread_pool
  simple_thread_pool();

  // Bounded thread-pool (cap on the number of threads).
  simple_thread_pool(size_t min_threads, size_t max_threads);

  // Deleting the thread pool should wait for all threads to complete their
  // work.
  ~simple_thread_pool();

  // Attempt to get a new mutable thread for execution, blocks if there is no
  // thread available.
  // Is this even a necessary function? It would end up blocking the caller for
  // some indefinite amount of time, which doesn't seem all that helpful.
  // mutable_thread* get_unused_thread();

  // Non-blocking version of get_unused_thread(). This thread will be tracked
  // as active by the pool until the thread is either donated back to the pool
  // or release_thread is called.
  // Returns NULL if no thread is available.
  mutable_thread* try_get_unused_thread();

  // Donates a mutable thread to the thread pool for re-use.
  // Returns false if the thread pool is full and cannot receive any more
  // threads.
  bool donate_thread(mutable_thread* t);

  // Releases a thread from being tracked by this pool.
  // Should only release an active thread (one created by
  // try_get_unused_thread).
  // Returns false if the thread is not active.
  bool release_thread(mutable_thread* t);

 private:
  // Mutex used when creating new threads. This is here to avoid race
  // conditions when threads are being manipulated (adding or deleting threads
  // from the pool).
  mutex new_thread_mu_;
  bool shutting_down_;
  set<mutable_thread*> active_threads_;
  set<mutable_thread*> unused_threads_;
  size_t min_threads_;
  size_t max_threads_;
  
  // TODO: migrate this to use the worker queue since there threads can be
  // interrupted.
  // blocking_queue<tr1::function<void()> > work_pool_;
};

}

#endif  // GCL_SIMPLE_THREAD_POOL_
