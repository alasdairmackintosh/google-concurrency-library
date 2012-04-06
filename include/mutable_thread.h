// Copyright 2011 Google Inc. All Rights Reserved.

#ifndef GCL_MUTABLE_THREAD__
#define GCL_MUTABLE_THREAD__

#include <tr1/functional>

#include <atomic.h>
#include <condition_variable.h>
#include <mutex.h>
#include <thread.h>

namespace tr1 = std::tr1;
using std::atomic_int;

namespace gcl {

// Variation on the thread class which allows threads to be put to sleep when
// not working on anything and awoken again with new work. The class allows
// work to be queued up one-deep (a single extra item of execution). The thread
//
// This is a building block for more complex thread execution classes which
// need to be able to stop and restart threads with new work as well as to
// re-allocate threads to new work queues or tasks.
//
// TODO: Refine the execute behavior slightly to see if it should be extended
// to allow the thread to enqueue arbitrary numbers of items or not. The
// current behavior allows only a single task to enqueue which means behavior
// is slightly inconsistent (execute will return even if the task is not
// guaranteed to be able to execute).
class mutable_thread {
 public:
  // Mutable thread works a little differently from a normal thread in that it
  // can start in an empty state.
  mutable_thread();
  ~mutable_thread();

  template<class F>explicit mutable_thread(F f);

  // Thread join. Will not complete joining until all queued work is completed
  // (makes no guarantees that the thread will terminate).
  void join();

  // Setup function for execution if there isn't currently something executing
  // or if there is only a single task currently executing.
  // Return false if thread is currently doing other work.
  bool try_execute(tr1::function<void()> fn);

  // Like try_execute, but blocks until there is an empty spot to queue up for
  // execution.
  // Return false if the thread is in the process of joining (and thus cannot
  // accept new work).
  bool execute(tr1::function<void()> fn);

  // Join has been called but the thread is still executing.
  bool is_joining();

  // Thread has fully joined and will not accept any more work.
  bool is_done();

 private:
  enum thread_state {
    IDLE = 0,
    RUNNING = 1,
    JOINING = 2,
    DONE = 3
  };

  // Run function for the thread -- this is a wrapper around the work which
  // actually should get done.
  void run();

  // Function to complete execution of the thread.
  void finish_run();

  // Check if the thread has some action to take (either something is queued to
  // run or it is time to exit).
  bool ready_to_continue();

  // Function to wait the thread until there is work to be done or the thread
  // should finish up.
  // Returns true if there is work to be done.
  bool thread_wait();

  thread* t_;

  mutex thread_state_mu_;
  condition_variable thread_paused_cond_;
  atomic_int thread_state_;

  // Actively running function.
  tr1::function<void()> run_fn_;
  tr1::function<void()> queued_fn_;
};

}  // namespace gcl

#endif
