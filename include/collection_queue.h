// Copyright 2011 Google Inc. All Rights Reserved.

#ifndef GCL_COLLECTION_QUEUE_
#define GCL_COLLECTION_QUEUE_
#include <algorithm>

#include <closed_error.h>
#include <gcl_string.h>
#include <thread.h>
#include <mutex.h>

namespace gcl {

// A partial queue implementation based on a std:: collection. This is
// designed to be used in conjunction with a gcl::source, and allows a
// source to be created from a collection. This is useful when
// creating gcl::Pipelines that read from a collection. The queue will
// be closed whrn all elements of the collection have been read.
//
// The collection_queue is threadsafe in that it can be accessed from
// multiple threads, but is not safe against modifications to the
// underlying collection. Once a collection_queue is created, the
// underlying collection should not be modified.
//
// TODO(alasdair): Do we need this class? If so, should this be based
// on concurrent collections?

template <typename Collection>
class collection_queue {
 public:
  collection_queue(const Collection& collection)
      : collection_(collection), it_(collection_.begin()) {
  }

  typename Collection::value_type pop() {
    unique_lock<mutex> l(lock_);
    if (it_ == collection_.end()) {
      throw closed_error("Collection at end in " +
                         to_string(this_thread::get_id()));
    }
    typename Collection::value_type result = *it_;
    ++it_;
    return result;
  }

  bool is_closed() {
    unique_lock<mutex> l(lock_);
    return (it_ == collection_.end());
  }

 private:
  const Collection& collection_;
  typename Collection::const_iterator it_;
  mutex lock_;

  collection_queue(const collection_queue& other);
  collection_queue& operator=(const collection_queue& other);
};

template <typename Collection>
collection_queue<Collection> make_queue(Collection& begin, Collection& end) {
  return collection_queue<Collection>(begin, end);
}

}  // End namespace gcl
#endif  // GCL_COLLECTION_QUEUE_
