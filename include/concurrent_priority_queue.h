// Copyright 2009 Google Inc. All Rights Reserved.
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

#ifndef GCL_CONCURRENT_PRIORITY_QUEUE_
#define GCL_CONCURRENT_PRIORITY_QUEUE_

#include <algorithm>
#include <string>
#include <vector>

#include "functional.h"

#include "mutex.h"
#include "condition_variable.h"
#include "thread.h"

namespace gcl {

// TODO(alasdair): Initial version being checked in without
// locking. Make this genuinely threadsafe. Document exact details of
// thread safety.
//
//
// A queue of elements, ordered by priority. Elements within the queue
// are compared using std::less, and an element with a high priority
// is considered to be greater than one with a lower priority.
//
// An element may be removed from the head of the queue using
// pop(). The element so removed will be the highest priority element
// currently in the queue. (If multiple elements have the same highest
// priority, there is no guarantee as to which element will be removed
// first.)
//
// This class is thread safe. Elements may be added and removed from
// multiple threads.

template <typename T,
          class Container = std::vector<T>,
          class Less = std::less<T> >
class concurrent_priority_queue {
 public:
  typedef T value_type;
  typedef T& reference;
  typedef const T& const_reference;
  typedef std::size_t size_type;

  concurrent_priority_queue() {
  }

  // requires CopyConstructible<Container>
  explicit concurrent_priority_queue(const Container& cont) : cont_(cont) {
    make_heap();
  }

  // requires CopyConstructible<Container> & CopyConstructible<Less>
  concurrent_priority_queue(const Less& c, const Container& cont)
      : less_(c), cont_(cont) {
    make_heap();
  }

  // requires CopyConstructible<Container> &
  // RangeInsertionContainer<Container, Iter> &
  // CopyConstructible<Less>
  template <typename Iter>
  concurrent_priority_queue(Iter first,
                            Iter last,
                            const Less& less,
                            const Container& cont)
    : less_(less), cont_(cont) {
    cont_.insert(cont_.end(), first, last);
    make_heap();
  }

  // requires MoveConstructible<Container> &
  // RangeInsertionContainer<Container, Iter>
  template <typename Iter>
  concurrent_priority_queue(Iter first, Iter last) : cont_(first, last) {
    make_heap();
  }

  // Creates a new priority queue as a copy of an exisiting queue.
  // This method is not thread safe. It is the caller's responsibility
  // to ensure that the other queue will not be modified during this
  // operation.
  // requires MoveConstructible<Container>
  concurrent_priority_queue(const concurrent_priority_queue& other)
     : less_(other.less_), cont_(other.cont_) {
  }

  // Copies the contents of another queue into this queue. 
  // This method is not thread safe. It is the caller's responsibility
  // to ensure that the other queue will not be modified during this
  // operation, and that this queue will not not be accessed until the
  // copy is complete.
  // requires MoveAssignable<Container>
  concurrent_priority_queue& operator=(const concurrent_priority_queue& other) {
    this->cont_ = other.cont_;
    this->less_ = other.less_;
    return *this;
  }

  // Exchanges the contents of this queue with the contents of
  // another.
  // This method is not thread safe. It is the caller's responsibility
  // to ensure that the neither queue will be acessed by another
  // thread until this operation is complete.
  // requires Swappable<Container>
  void swap(concurrent_priority_queue& other) {
    std::swap(cont_, other.cont_);
    std::swap(less_, other.less_);
  }

  // Re-evaluates the order of elements in the queue, using a new
  // comparator. This may change the order in which elements are
  // removed from the front of the queue. 
  // TODO(alasdair): verify what thread-safety guarantees we can make
  // for this operation. We do not want to lock the entire queue.
  void update(const Less& less) {
    less_ = less;
    make_heap();
  }

  // Returns true if this queue is empty
  bool empty() const { return cont_.empty(); }

  // Returns the number of elements in the queue
  size_type size() const { return cont_.size(); }

  // Adds a new element to the queue.
  void push(const value_type& x) {
    unique_lock<mutex> l(pop_mutex_);
    cont_.push_back(x);
    push_heap(cont_.begin(), cont_.end(), less_);
    pop_var_.notify_all();
  }

#if 0
  // TODO(alasdair): Implement when we have C++11 compatibility mode.
  void push(value_type&& x) {
    cont_.push_back(x);
    push_heap(cont_.begin(), cont_.end(), less_);
  }
#endif

#if 0
  // TODO(alasdair): Add this when we have support for the '...' construct
  // requires BackEmplacementContainer<Container, Args&...>
  template <class... Args>
  void emplace(Args&... args);
#endif

  // requires MoveAssignable<value_type>
  bool try_pop(value_type& out) {
    unique_lock<mutex> lock(pop_mutex_);
    return do_pop(out);
  }

#if 0
  // TODO(alasdair): Add this when we have the timeout type
  // requires MoveAssignable<value_type>
  bool wait_pop(value_type& out, timeout t);
#endif

  // Returns the element at the front of the queue. This will be the
  // element with the highest priority. Blocks until an element is
  // available.
  // requires MoveConstructible<value_type>
  value_type pop() {
    unique_lock<mutex> lock(pop_mutex_);
    value_type result;
    while (!do_pop(result)) {
      pop_var_.wait(lock);
    }
    return result;
  }

 private:
  void make_heap() {
    std::make_heap(cont_.begin(), cont_.end(), less_);
  }

  // Pops a value off the front of the queue. Used internally by pop
  // and try_pop. Caller must lock pop_mutex_ before calling.
  bool do_pop(value_type& out) {
    if (cont_.empty()) {
      return false;
    } else {
      pop_heap(cont_.begin(), cont_.end(), less_);
      out = cont_.back();
      cont_.pop_back();
      return true;
    }
  }
 
  Less less_;
  Container cont_;
  mutex pop_mutex_;
  condition_variable pop_var_; 
};

}  // End namespace gcl
#endif  // GCL_CONCURRENT_PRIORITY_QUEUE_
