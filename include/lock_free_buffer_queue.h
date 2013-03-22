// Copyright 2013 Google Inc. All Rights Reserved.
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

#ifndef LOCK_FREE_BUFFER_QUEUE_H
#define LOCK_FREE_BUFFER_QUEUE_H

#include <atomic.h>
#include <iostream>

#include "debug.h"
#include "queue_base.h"
#include "system_error.h"

using std::atomic;

namespace gcl {

// Special queue node which knows to delete it's next pointer as needed.
template <typename Value>
class lock_free_buffer_queue
{
  public:
    typedef Value value_type;

    // Deleted constructors
    lock_free_buffer_queue() CXX11_DELETED
    lock_free_buffer_queue(const lock_free_buffer_queue&) CXX11_DELETED
    lock_free_buffer_queue& operator =(const lock_free_buffer_queue&) CXX11_DELETED

    explicit lock_free_buffer_queue(size_t max_elems);
    lock_free_buffer_queue(size_t max_elems, const char* name);
    template <typename Iter>
    lock_free_buffer_queue(size_t max_elems, Iter first, Iter last, const char* name);
    template <typename Iter>
    lock_free_buffer_queue(size_t max_elems, Iter first, Iter last);
    ~lock_free_buffer_queue();

    const char* name();
    bool is_empty();

    queue_op_status try_pop(Value&);
    queue_op_status nonblocking_pop(Value&);
    queue_op_status try_push(const Value& x);
    queue_op_status nonblocking_push(const Value& x);
#ifdef HAS_CXX11_RVREF
    queue_op_status try_push(Value&& x);
    queue_op_status nonblocking_push(Value&& x);
#endif

  private:
    const char* name_;
    const size_t MAX_SIZE;
    atomic<unsigned long long> head_;
    atomic<unsigned long long> tail_;
    atomic<Value*> *values_;

    void init();

    template <typename Iter>
    void iter_init(Iter begin, Iter end);

    bool is_full();
};

template <typename Value>
void lock_free_buffer_queue<Value>::init()
{
    if ( MAX_SIZE < 1 ) {
        throw std::invalid_argument("number of elements must be at least one");
    }
                
    // Set everything empty with no value.
    head_ = 0ULL;
    tail_ = 0ULL;
    values_ = new atomic<Value*>[MAX_SIZE];    
    for (unsigned int i = 0; i < MAX_SIZE; ++i) {
        // Reset the value.
        values_[i] = CXX11_NULLPTR;
    }
}

template <typename Value>
template <typename Iter>
void lock_free_buffer_queue<Value>::iter_init(Iter first, Iter last)
{
    // Do basic initialization first.
    init();

    // Operations should not fail in a serial context, so just spin in case try
    // ops fail.
    for ( Iter cur = first; cur != last; ++cur ) {
        if (is_full()) {
            throw std::invalid_argument("too few slots for iterator");
        }
        // Could technically do this in a more efficient way since this will
        // only get called from the constructor so synchronization could be
        // cheaper.
        while(try_push(*cur) != CXX11_ENUM_QUAL(queue_op_status)success) {
            // keep trying until we can try no longer.
        }
    }
}

template <typename Value>
lock_free_buffer_queue<Value>::lock_free_buffer_queue(size_t max_elems)
  : name_( "" ), MAX_SIZE(max_elems)
{
    init();
}

template <typename Value>
lock_free_buffer_queue<Value>::lock_free_buffer_queue(
    size_t max_elems, const char* name)
  : name_( name ), MAX_SIZE(max_elems)
{
    init();
}

template <typename Value>
template <typename Iter>
lock_free_buffer_queue<Value>::lock_free_buffer_queue(
    size_t max_elems, Iter first, Iter last, const char* name)
  : name_( name ), MAX_SIZE(max_elems)
{
    iter_init(first, last);
}

template <typename Value>
template <typename Iter>
lock_free_buffer_queue<Value>::lock_free_buffer_queue(
    size_t max_elems, Iter first, Iter last)
  : name_( "" ), MAX_SIZE(max_elems)
{
    iter_init(first, last);
}

template <typename Value>
lock_free_buffer_queue<Value>::~lock_free_buffer_queue() {
    for (unsigned int i = 0; i < MAX_SIZE; ++i) {
        Value* old_value = values_[i].exchange(CXX11_NULLPTR);
        delete old_value;
    }
}

template <typename Value>
bool lock_free_buffer_queue<Value>::is_empty()
{
    // Will only return true iff queue truly was empty at that point since head
    // cannot have advanced past tail if the queue was empty.
    //
    // This could return false if there are some in-progress pushes, but no
    // actual values, though.
    return head_.load() == tail_.load();
}

template <typename Value>
bool lock_free_buffer_queue<Value>::is_full()
{
    // This is true as tail cannot have moved past head.
    return tail_.load() == (head_.load() + MAX_SIZE);
}

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::try_pop(Value& elem)
{
    return nonblocking_pop(elem);
}

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::nonblocking_pop(Value& elem)
{
    /* This try block is here to catch exceptions from the user-defined copy
     * assignment operator in push_at. */
    try {
        do {
            unsigned long long head = head_.load();
            //DBG << "Head loaded" << std::endl;
            if (head == tail_.load()) {
                //DBG << "Queue empty" << std::endl;
                return CXX11_ENUM_QUAL(queue_op_status)empty;
            }

            // Check that there is a value and head didn't move.
            unsigned long long pos = head % MAX_SIZE;
            if (values_[pos].load(std::memory_order_relaxed) != CXX11_NULLPTR) {
                //DBG << "Found a value at head" << std::endl;
                // Found a value, now see if we can keep it.
                if (head_.compare_exchange_strong(head, head + 1)) {
                    //DBG << "Head Swapped (t-1)" << std::endl;
                    // The only place where blocking can occur between threads is
                    // here, between the head update and the swap of the pointer to
                    // NULL.
                    Value* old_value = values_[pos].exchange(
                        CXX11_NULLPTR, std::memory_order_relaxed);
                    elem = *old_value;
                    //DBG << "Exchange complete (done)" << std::endl;
                    return CXX11_ENUM_QUAL(queue_op_status)success;
                }
            } else if (head == head_.load()) {
                //DBG << "Empty value, waiting on push" << std::endl;
                // Null but head is still the same, so waiting on a value.
                return CXX11_ENUM_QUAL(queue_op_status)busy;
            }

            // Other active dequeue threads could cause this one to stall
            // indefinitely, but someone is always trying to make progres.
        } while (true);
    } catch (...) {
        throw;
    }
}

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::try_push(const Value& elem)
{
    return nonblocking_push(elem);
}

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::nonblocking_push(const Value& elem)
{
    /* This try block is here to catch exceptions from the user-defined copy
     * assignment operator in push_at. */
    try {
        do {
            unsigned long long tail = tail_.load();
            //DBG << "Load tail" << std::endl;
            if (tail == (head_.load() + MAX_SIZE)) {
                //DBG << "Full" << std::endl;
                return CXX11_ENUM_QUAL(queue_op_status)full;
            }

            // Write into the current tail position.
            unsigned long long pos = tail % MAX_SIZE;
            if (values_[pos].load(std::memory_order_relaxed) != CXX11_NULLPTR) {
                //DBG << "Non null at current tail" << std::endl;
                // Pop from same position is still pending. We can't do much to
                // help him along unfortunately.
                return CXX11_ENUM_QUAL(queue_op_status)busy;
            }
            // Try to reserve the tail.
            if (tail_.compare_exchange_strong(tail, tail + 1)) {
                //DBG << "Swapped tail!" << std::endl;
                // Success! Now push the new element into the value. The only
                // blocking time is between the inc() and the completion of writing
                // the atomic pointer.
                values_[pos].exchange(new Value(elem),
                                      std::memory_order_relaxed);
                //DBG << "Exchange complete" << std::endl;
                return CXX11_ENUM_QUAL(queue_op_status)success;
            }
        } while (true);
    } catch (...) {
        throw;
    }
}

#ifdef HAS_CXX11_RVREF

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::try_push(Value&& elem)
{
    // May return busy
    return nonblocking_push(elem);
}

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::nonblocking_push(Value&& elem)
{
    /* This try block is here to catch exceptions from the user-defined copy
     * assignment operator in push_at. */
    try {
        do {
            unsigned long long tail = tail_.load();
            if (tail == (head_.load() + MAX_SIZE)) {
                return CXX11_ENUM_QUAL(queue_op_status)full;
            }

            // Write into the current tail position.
            unsigned long long pos = tail % MAX_SIZE;
            if (values_[pos].load(std::memory_order_relaxed) == CXX11_NULLPTR) {
                // Pop from same position is still pending. We can't do much to
                // help him along unfortunately.
                return CXX11_ENUM_QUAL(queue_op_status)busy;
            }
            // Try to reserve the tail.
            if (tail_.compare_exchange_strong(tail, tail + 1)) {
                // Success! Now push the new element into the value. The only
                // blocking time is between the inc() and the completion of writing
                // the atomic pointer.
                values_[pos].exchange(new Value(elem),
                                      std::memory_order_relaxed);
                return CXX11_ENUM_QUAL(queue_op_status)success;
            }
        } while (true);
    } catch (...) {
        throw;
    }
}

// HAS_CXX11_RVREF
#endif

template <typename Value>
const char* lock_free_buffer_queue<Value>::name()
{
    return name_;
}

} // namespace gcl

#endif
