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

#ifndef GCL_BLOCKING_QUEUE_
#define GCL_BLOCKING_QUEUE_

#include <algorithm>
#include <deque>
#include <stdexcept>
#include <string>

#include "functional.h"

#include "atomic.h"
#include "mutex.h"
#include "condition_variable.h"
#include "thread.h"

#include "closed_error.h"

namespace gcl {

// A queue of elements. A calling thread that attempts to remove an element from
// the front of an empty queue will block until an element is added to the
// queue.
//
// This class is thread safe. Elements may be added and removed from multiple
// threads. If multiple threads attempt to remove an element from the queue, it
// is undefined as to which thread will retrieve the next element.
template <typename T,
          class Container = std::deque<T> >
class blocking_queue {
 public:
  typedef T value_type;
  typedef T& reference;
  typedef const T& const_reference;
  typedef std::size_t size_type;

  // Creates a blocking queue with an unlimited maximum size
  blocking_queue() : max_size_((size_t) - 1) {
    // TODO(alasdair): Initialise closed_ in the init list when we
    // have support for the necessary C++0x mechanism.
    atomic_init(&closed_, false);
  }

  // Creates a blocking queue with the specified maximum size. The size cannot
  // be zero.
  explicit blocking_queue(size_type max_size) : max_size_(max_size) {
    if (max_size == 0) {
      throw std::invalid_argument("Size cannot be zero");
    }
    atomic_init(&closed_, false);
  }

  // TODO(alasdair): consider having thread-safe iterators for copy
  // constructors etc.

  // Creates a new blocking queue as a copy of an exisiting queue.
  // This method is not thread safe. It is the caller's responsibility
  // to ensure that the other queue will not be modified during this
  // operation.
  // requires MoveConstructible<Container>
  blocking_queue(const blocking_queue& other)
      : max_size_(other.max_size_), cont_(other.cont_) {
    atomic_init(&closed_, other.closed_.load());
  }

  // Creates a new blocking queue from a range of elements defined by
  // two iterators. The maximum size must be greater than 0, and
  // greater than or equal to the range defined by the iterators.
  // requires MoveConstructible<Container>
  template <typename Iter>
  blocking_queue(size_type max_size, Iter first, Iter last)
      : max_size_(max_size), cont_(first, last) {
    if (max_size == 0) {
      throw std::invalid_argument("Size cannot be zero");
    }
    if (max_size < size()) {
      throw std::invalid_argument("Max size less than iterator size");
    }
    atomic_init(&closed_, false);
  }

  // Copies the contents of another queue into this queue.
  // This method is not thread safe. It is the caller's responsibility
  // to ensure that the other queue will not be modified during this
  // operation, and that this queue will not not be accessed until the
  // copy is complete.
  // requires MoveAssignable<Container>
  blocking_queue& operator=(const blocking_queue& other) {
    this->cont_ = other.cont_;
    this->max_size_ = other.max_size_;
    atomic_init(&this->closed_, other.closed_.load());
    return *this;
  }

  // Closes this queue. No further attempts may be made to push
  // elements onto the queue. Existing elements may still be popped.
  void close() {
    closed_.store(true, std::memory_order_release);

    // We need to notify any threads that may be waiting to push or
    // pop elements.
    unique_lock<mutex> el(container_lock_);
    empty_condition_.notify_all();
    full_condition_.notify_all();
  }

  // Returns true if the queue has been closed, and it is no longer
  // possible to push new elements.
  bool is_closed() {
    return closed_.load(std::memory_order_acquire);
  }

 // Returns true if this queue is empty
  bool empty() const {
    unique_lock<mutex> ul(container_lock_);
    return cont_.empty();
  }

  // Returns the number of elements in the queue
  size_type size() const {
    unique_lock<mutex> ul(container_lock_);
    return cont_.size();
  }

  // Returns the maximum size of the queue
  size_type max_size() const { return max_size_; }

#if 0
  // TODO(alasdair): Implement when we have C++0x compatibility mode.
  void push(value_type&& x) {
    cont_.push_back(x);
  }
#endif

  // Tries to add a new element to the rear of the queue. If the queue
  // is already at maximum capacity, returns false. Otherwise adds the
  // element and returns true. If the queue is closed, throws a
  // closed_error.
  bool try_push(const value_type& x) {
    unique_lock<mutex> ul(container_lock_);
    if (!try_push_internal(x)) {
      return false;
    }
    empty_condition_.notify_all();
    return true;
  }

  // Adds a new element to the rear of the queue. If the queue is
  // already at maximum capacity, blocks until an element has been
  // removed from the front. If the queue is closed, throws a
  // closed_error.
  void push(const value_type& x) {
    unique_lock<mutex> ul(container_lock_);
    {
      while(!try_push_internal(x)) {
        full_condition_.wait(ul);
      }
    }
    empty_condition_.notify_all();
  }

 private :
  // Internal implementation used by push and try_push. Attempts to
  // push an item onto the queue. The caller should hold the
  // container_lock_ before calling, and should notify the
  // empty_condition_ after calling.
  bool try_push_internal(const value_type& x) {
    if (is_closed()) {
      throw closed_error("Queue is closed");
    }
    if (cont_.size() >= max_size()) {
      return false;
    } else {
      cont_.push_back(x);
      return true;
    }
  }

 public:
#if 0
  // TODO(alasdair): Add this when we have the timeout type.
  // See also concurrent_priority_queue.
  bool wait_pop(value_type& out, timeout t);
#endif

 // Tries to pop an element off the front of the queue. Returns true
 // on success, or false if the queue is currently empty.
  bool try_pop(value_type& out) {
    unique_lock<mutex> ul(container_lock_);
    if (!try_pop_internal(out)) {
      return false;
    }
    full_condition_.notify_all();
    return true;
  }

  // Returns the element at the front of the queue. This will be the
  // element with the highest priority. Blocks until an element is
  // available.
  //
  // If the queue is empty and closed, throws a closed_error
  value_type pop() {
    unique_lock<mutex> ul(container_lock_);
    value_type result;
    {
      while(!try_pop_internal(result)) {
        empty_condition_.wait(ul);
      }
    }
    full_condition_.notify_all();
    return result;
  }

 private:
  // Internal implementation used by pop and try_pop. Attempts to pop
  // an item off the queue. The caller should hold the container_lock_
  // before calling, and should notify the full_condition_ after
  // calling.
  bool try_pop_internal(value_type& out) {
    if (cont_.empty()) {
      if (is_closed()) {
        throw closed_error("Queue is closed and empty");
      }
      return false;
    } else {
      out = cont_.front();
      cont_.pop_front();
      return true;
    }
  }

  // TODO(alasdair): Consider how to shut down a long-running thread
  // that reads results from a blocking_queue. We need to be able to
  // signal to a reader that is blocked in pop() that the queue has
  // now shut down. Possible implementations include:
  //
  //   Have pop return a bool (similar to try_pop() or wait_pop()).
  //   Throw an exception
  //   Have pop return an Iterator that can point to end()
  //
  // We could also combine the latter two with a revised signature for
  // wait_pop().

 private:
  size_type max_size_;
  Container cont_;
  mutable mutex container_lock_;

  // When the queue is full, block waiting on this condition. Notify
  // this condition when an element is removed.
  condition_variable full_condition_;

  // When the queue is empty, block waiting on this condition. Notify
  // this condition when an element is added. Note that separate
  // conditions are required for reading and writing, to prevent a
  // thread that is repeatedly adding to the queue from aquiring the
  // lock mutex at the expense of waiting threads.
  condition_variable empty_condition_;

  std::atomic_bool closed_;
};

}  // End namespace gcl
#endif  // GCL_BLOCKING_QUEUE_
