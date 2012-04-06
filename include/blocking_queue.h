// Copyright 2010 Google Inc. All Rights Reserved.

#ifndef STD_BLOCKING_QUEUE_
#define STD_BLOCKING_QUEUE_

#include <algorithm>
#include <functional>
#include <deque>
#include <stdexcept>
#include <string>
#include <tr1/functional>

#include <condition_variable.h>
#include <mutex.h>
#include <thread.h>

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
  }

  // Creates a blocking queue with the specified maximum size. The size cannot
  // be zero.
  explicit blocking_queue(size_type max_size)  : max_size_(max_size) {
    if (max_size == 0) {
      throw std::invalid_argument("Size cannot be zero");
    }
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
    return *this;
  }

  // Returns true if this queue is empty
  bool empty() const { return cont_.empty(); }

  // Returns the number of elements in the queue
  size_type size() const { return cont_.size(); }

  // Returns the maximum size of the queue
  size_type max_size() const { return max_size_; }

  // Adds a new element to the rear of the queue. If the queue is already at
  // maximum capacity, blocks until an element has been removed from the front.
  void push(const value_type& x) {
    while(!try_push(x)) {
      unique_lock<mutex> ul(full_lock_);
      full_condition_.wait(ul);
    }
  }

  // Tries to add a new element to the rear of the queue. If the queue is
  // already at maximum capacity, returns false. Otherwise adds the element and
  // returns true.
  bool try_push(const value_type& x) {
    {
      unique_lock<mutex> ul(container_lock_);
      if (size() >= max_size()) {
        return false;
      } else {
        cont_.push_back(x);
      }
    }
    unique_lock<mutex> ul(empty_lock_);
    empty_condition_.notify_all();
    return true;
  }

#if 0
  // TODO(alasdair): Implement when we have C++0x compatibility mode.
  void push(value_type&& x) {
    cont_.push_back(x);
  }
#endif

 // Tries to pop an element off the front of the queue. Returns true
 // on success, or false if the queue is currently empty.
  bool try_pop(value_type& out) {
    {
      unique_lock<mutex> ul(container_lock_);
      if (cont_.empty()) {
        return false;
      } else {
        out = cont_.front();
        cont_.pop_front();
      }
    }
    unique_lock<mutex> ul(full_lock_);
    full_condition_.notify_all();
    return true;
  }

#if 0
  // TODO(alasdair): Add this when we have the timeout type.
  // See also concurrent_priority_queue.
  bool wait_pop(value_type& out, timeout t);
#endif

  // Returns the element at the front of the queue. This will be the
  // element with the highest priority. Blocks until an element is
  // available.
  value_type pop() {
    value_type result;
    while(!try_pop(result)) {
      unique_lock<mutex> ul(empty_lock_);
      empty_condition_.wait(ul);
    }
    return result;
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
  mutex container_lock_;

  // When the queue is full, block waiting on this condition. Notify
  // this condition when an element is removed.
  mutex full_lock_;
  condition_variable full_condition_;

  // When the queue is empty, block waiting on this condition. Notify
  // this condition when an element is added. Note that separate
  // conditions are required for reading and writing, to prevent a
  // thread that is repeatedly adding to the queue from aquiring the
  // lock mutex at the expense of waiting threads.
  mutex empty_lock_;
  condition_variable empty_condition_;
};

#endif  // STD_BLOCKING_QUEUE_
