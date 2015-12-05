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

#ifndef GCL_ITERATOR_QUEUE_
#define GCL_ITERATOR_QUEUE_

#include <algorithm>

#include <mutex>

#include "closed_error.h"

namespace gcl {

// A partial queue implementation based on std:: iterators. This is
// designed to be used in conjunction with a gcl::source, and allows a
// source to be created from a collection. This is useful when
// creating gcl::Pipelines that read from a collection. The queue will
// be closed when the iterator reaches the end
//
// The iterator_queue is threadsafe in that it can be accessed from
// multiple threads, but is not safe against modifications to the
// underlying collection from which the iterators are derived. Once an
// iterator_queue is created, the underlying collection should not
// be modified.
//
// TODO(alasdair): Do we need this class? If so, should this be based
// on concurrent collections?

template <typename Iterator>
class iterator_queue {
 public:
  // Creates a new queue.
  iterator_queue(const Iterator& start, const Iterator& end)
      : it_(start), end_(end) {
  }

  typename std::iterator_traits<Iterator>::value_type pop() {
    std::unique_lock<std::mutex> l(lock_);
    if (it_ == end_) {
      throw closed_error("Iterator at end");
    }
    typename std::iterator_traits<Iterator>::value_type result = *it_;
    ++it_;
    return result;
  }

  bool is_closed() {
    std::unique_lock<std::mutex> l(lock_);
    return (it_ == end_);
  }

 private:
  Iterator it_;
  Iterator end_;
  std::mutex lock_;

  iterator_queue(const iterator_queue& other);
  iterator_queue& operator=(const iterator_queue& other);
};

template <typename I>
iterator_queue<I> make_queue(I& begin, I& end) {
  return iterator_queue<I>(begin, end);
}

}  // End namespace gcl
#endif  // GCL_ITERATOR_QUEUE_
