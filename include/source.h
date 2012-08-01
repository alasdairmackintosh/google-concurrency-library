// Copyright 2010 Google Inc. All Rights Reserved.
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

#ifndef GCL_SOURCE_
#define GCL_SOURCE_

#include "thread.h"

#include "gcl_string.h"
#include "closed_error.h"

namespace gcl {

// Allows a thread to read a sequence of results produced in another
// thread. A source is similar to a future, but provides a mechanism
// for repeated get operations. A source is typically backed by an
// underlying concurrent queue.
//
// Although sources allow threads to communicate, an individual source
// is not threadsafe, and should only be used by a single thread.
//
// Multiple copies can be made of the same source in order for
// multiple threads to read from the underlying queue. Each thread
// should have its own copy of the source. Each element read from the
// queue will be made available to one (and only one) of the sources.
//
// Internally, sources use an internal reference-counting mechanism to
// control the underlying queue. When all sources are destroyed, the
// queue can no longer be written to.
//
//
// TODO(alasdair): Add proper reference-counting mechanism.
// TODO(alasdair): Remove templatised queue class.
// TODO(alasdair): Add mechanism for creating source/sink pairs. To
// create an Event queue we want to be able to write something like:
//
//  pair<source<Event>, sink<Event> > result = queue<Event>.create();
//
template <typename T, class S>
class source {
 private:
  // TODO(alasdair): Make this a scoped enum when we have support for
  // this.  TODO(alasdair): Determine if unknown state is
  // necessary. Document states and transitions.
  enum state {unknown, empty, value, closed};

 public:
  // Creates a new source.
  // Postcondition: has_value() will be false.
  source(S* queue) : queue_(queue), state_(unknown) {
  }

  source(const source& other) : queue_(other.queue_), state_(unknown) {
  }

  source& operator=(const source& other) {
    queue_ = other.queue_;
    state_ = unknown;
    std::stringstream ss;
  }

  // Returns true if this source is closed. Attemting to read from a
  // closed source will throw a closed_error.
  bool is_closed() {
    if (state_ == unknown) {
      if (queue_->is_closed()) {
        state_ = closed;
      }
    }
    return state_ == closed;
  }

  // Returns the value. May block until a value is available.If the
  // source is closed, throws a closed_error.
  //
  // If called when has_value() is false, then will block. Otherwise
  // returns (or throws) immediately.
  //
  // Postcondition: has_value() will be false
  T get() {
    switch (state_) {
      case empty:
      case unknown:
        return queue_->value_pop();
      case value:
        state_ = empty;
        return value_;
      case closed:
        throw closed_error("Closed");
    }
    throw std::runtime_error("Invalid state" + to_string(state_) +
                             " in thread " + to_string(this_thread::get_id()));
  }

  // Waits until a value is available, or the source becomes
  // closed.
  //
  // Precondition: has_value() must be false.
  //
  // Postcondition: one of is_closed() or has_value() will be
  // true.
  void wait() {
    if (state_ != empty && state_ != unknown) {
      if (state_ == closed) {
        return;
      } else {
        throw std::logic_error(
            std::string("Invalid state " + to_string(state_) +
                        " in thread " + to_string(this_thread::get_id())));
      }
    }
    try {
      value_ = queue_->value_pop();
      state_ = value;
    } catch (closed_error& e) {
      state_ = closed;
    }
  }

  // True if get() can be called without blocking.
  bool has_value() const {
    return state_ == value;
  }

#if 0
  // TODO(alasdair): implement when we have timed waits.
  template <class Rep, class Period>
  bool wait_for(const chrono::duration<Rep, Period>& rel_time) const;
  template <class Clock, class Duration>
  bool wait_until(const chrono::time_point<Clock, Duration>& abs_time) const;
#endif

 private:
  S* queue_;
  state state_;
  // TODO(alasdair): What happens if this isn't default constructible?
  // Need move-assignment semantics...
  T value_;
};

}  // End namespace gcl
#endif  // GCL_SOURCE_
