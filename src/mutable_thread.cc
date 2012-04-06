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

#include "functional.h"

#include "atomic.h"
#include "mutex.h"
#include "condition_variable.h"
#include "thread.h"

#include "mutable_thread.h"

namespace gcl {

mutable_thread::mutable_thread() {
  atomic_init(&thread_state_, IDLE);
  t_ = new thread(std::bind(&mutable_thread::run, this));
}

template<class F>
mutable_thread::mutable_thread(F f) {
  atomic_init(&thread_state_, IDLE);
  execute(f);
  t_ = new thread(std::bind(&mutable_thread::run, this));
}

mutable_thread::~mutable_thread() {
  if (!is_done() && !is_joining()) {
    join();
  }
  delete t_;
}

thread::id mutable_thread::get_id() {
  return t_->get_id();
}

void mutable_thread::join() {
  {
    if (thread_state_.load() != DONE) {
      unique_lock<mutex> ul(thread_state_mu_);
      thread_state_.store(JOINING);

      // There is only one paused thread at a time.
      thread_paused_cond_.notify_one();
    }
  }

  t_->join();
  thread_state_.store(JOINED);
}

// Setup function for execution if there isn't currently something executing.
bool mutable_thread::try_execute(std::function<void()> fn) {
  if (!run_fn_ || !queued_fn_) {
    unique_lock<mutex> ul(thread_state_mu_);
    if (!is_done() && !is_joining()) {
      // Thread can still execute, so check if we want to queue up some work or
      // run it in the thread.
      if (run_fn_) {
        queued_fn_.swap(fn);
      } else {
        run_fn_.swap(fn);
      }

      // Communicate to the thread that there is now some new work to execute.
      thread_paused_cond_.notify_one();
      return true;
    } else {
      // thread is done, should this be signalled somewhere?
      return false;
    }
  } else {
    // Couldn't execute since something is currently executing.
    return false;
  }
}

bool mutable_thread::execute(std::function<void()> fn) {
  while ((run_fn_ && queued_fn_) && !is_done() && !is_joining()) {
    // Currently spin -- maybe swap this with a condvar instead.
    this_thread::sleep_for(chrono::milliseconds(1));
  }

  if (is_done() || is_joining()) {
    return false;
  } else {
    unique_lock<mutex> ul(thread_state_mu_);
    if  (!run_fn_) {
      run_fn_.swap(fn);
    } else {
      queued_fn_.swap(fn);
    }

    // Notify the paused thread to wake up now that there's work to do.
    thread_paused_cond_.notify_one();
    return true;
  }
}

bool mutable_thread::is_joining() {
  return thread_state_.load() == JOINING;
}

bool mutable_thread::is_done() {
  int thread_state = thread_state_.load();
  return thread_state == DONE || thread_state == JOINED;
}

bool mutable_thread::ready_to_continue() {
  return is_joining() || is_done() || run_fn_;
}

void mutable_thread::run() {
  while(true) {
    if (thread_wait()) {
      run_fn_();
      finish_run();
    } else {
      thread_state_.store(DONE);
      break;
    }
  }
}

bool mutable_thread::thread_wait() {
  unique_lock<mutex> ul(thread_state_mu_);
  // Wait for the thread to be in a state where it should do something.
  thread_paused_cond_.wait(
      ul, std::bind(&mutable_thread::ready_to_continue, this));
  if (is_joining() || is_done()) {
    return false;
  } else {
    // Something to execute!
    return true;
  }
}

// Function to complete 
void mutable_thread::finish_run() {
  if (queued_fn_) {
    // If there is a queued function swap that into the run_fn.
    std::function<void()> empty;
    run_fn_.swap(queued_fn_);
    queued_fn_.swap(empty);
  } else {
    // Set the run function to an empty function.
    std::function<void()> empty;

    // Is swap thread safe? probably not.
    run_fn_.swap(empty);
  }

  // And set the run state to IDLE if it was in running state.
  int new_state = RUNNING;
  thread_state_.compare_exchange_strong(new_state, IDLE);
}
 
}  // namespace gcl
