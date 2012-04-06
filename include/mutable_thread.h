// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GCL_MUTABLE_THREAD__
#define GCL_MUTABLE_THREAD__

#include "functional.h"

#include "atomic.h"
#include "mutex.h"
#include "condition_variable.h"
#include "thread.h"

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
  bool try_execute(std::function<void()> fn);

  // Like try_execute, but blocks until there is an empty spot to queue up for
  // execution.
  // Return false if the thread is in the process of joining (and thus cannot
  // accept new work).
  bool execute(std::function<void()> fn);

  // Join has been called but the thread is still executing.
  bool is_joining();

  // Thread has fully joined and will not accept any more work.
  bool is_done();

  // Returns the id of this mutable thread.
  thread::id get_id();

 private:
  enum thread_state {
    IDLE = 0,    // Ready to run
    RUNNING = 1, // Running a task
    JOINING = 2, // Running but with tasks waiting to join
    DONE = 3,    // Running completed
    JOINED = 4   // Running complete and join call completed
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
  std::atomic<int> thread_state_;

  // Actively running function.
  std::function<void()> run_fn_;
  std::function<void()> queued_fn_;
};

}  // namespace gcl

#endif
