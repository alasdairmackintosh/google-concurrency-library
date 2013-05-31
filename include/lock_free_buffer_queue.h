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
#include <stdint.h>

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
    template <typename Iter>
    lock_free_buffer_queue(size_t max_elems, Iter first, Iter last);
    ~lock_free_buffer_queue();

    // Note  that these two functions may be approximate as there may have been
    // exceptions thrown and the state is not fully correct (all values could
    // be invalid but we say it's not).
    bool is_empty();
    bool is_full();

    queue_op_status try_pop(Value&);
    queue_op_status nonblocking_pop(Value&);
    queue_op_status try_push(const Value& x);
    queue_op_status nonblocking_push(const Value& x);
#ifdef HAS_CXX11_RVREF
    queue_op_status try_push(Value&& x);
    queue_op_status nonblocking_push(Value&& x);
#endif

  private:
    CXX11_ENUM_CLASS value_state
    {
        valid = 0, /* The value can safely be read */
        waiting,     /* The value is coming but not ready yet */
        invalid    /* The value is invalid and will never exist */
    };


    const size_t cardinality_;
    // Head and tail will always be increasing (hence the 64bit value so we
    // don't have to deal with overflow for now). Note, we could handle
    // overflow by a bit of overflow detection logic when the value wraps, but
    // this will take some coordination to do correctly and actually introduces
    // a very small chance of ABA (as in, you need a very fast computer to
    // push/pop 2^64 times while one thread sleeps).
    atomic<uint_least64_t> head_;
    atomic<uint_least64_t> tail_;
    atomic<value_state> *value_state_;
    Value *values_;

    // head_ and tail_ reserve a location and that is what is written to the
    // actual buffer.
    atomic<size_t> *value_location_;
    Value *values_array_;

    void init();

    template <typename Iter>
    void iter_init(Iter begin, Iter end);

    // Helper functions.
    void clear_value(value_state old_value, size_t pos);
    void set_state(value_state new_value, size_t pos);
};

template <typename Value>
void lock_free_buffer_queue<Value>::init()
{
    if ( cardinality_ < 1 ) {
        throw std::invalid_argument("number of elements must be at least one");
    }
                
    // Set everything empty with no value.
    head_ = 0ULL;
    tail_ = 0ULL;
    values_ = new Value[cardinality_];    
    value_state_ = new atomic<value_state>[cardinality_];    
    for (unsigned int i = 0; i < cardinality_; ++i) {
        // Reset the value to empty since it doesn't have a value yet.
        value_state_[i] = CXX11_ENUM_QUAL(value_state)waiting;
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
  : cardinality_(max_elems)
{
    init();
}

template <typename Value>
template <typename Iter>
lock_free_buffer_queue<Value>::lock_free_buffer_queue(
    size_t max_elems, Iter first, Iter last)
  : cardinality_(max_elems)
{
    iter_init(first, last);
}

template <typename Value>
lock_free_buffer_queue<Value>::~lock_free_buffer_queue() {
  delete [] values_;
  delete [] value_state_;
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
    return tail_.load() == (head_.load() + cardinality_);
}

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::try_pop(Value& elem)
{
    // Loop while busy to try to get a value.
    queue_op_status status;
    do {
        status = nonblocking_pop(elem);
    } while (status == CXX11_ENUM_QUAL(queue_op_status)busy);
    return status;
}

template <typename Value>
void lock_free_buffer_queue<Value>::clear_value(value_state old_value,
                                                size_t pos) {
    if (!value_state_[pos].compare_exchange_weak(
            old_value, CXX11_ENUM_QUAL(value_state)waiting,
            std::memory_order_release)) {
        // This should never happen for real
        // DBG << "Invalid Pop" << std::endl;
    }
}

template <typename Value>
void lock_free_buffer_queue<Value>::set_state(value_state new_value,
                                              size_t pos) {
    value_state old_value = CXX11_ENUM_QUAL(value_state)waiting;
    if (!value_state_[pos].compare_exchange_weak(
            old_value, new_value,
            std::memory_order_release)) {
        // This should never happen for real
        // DBG << "Invalid Pop" << std::endl;
    }
}

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::nonblocking_pop(Value& elem)
{
    uint_least64_t head = head_.load(std::memory_order_relaxed);
    if (head == tail_.load(std::memory_order_relaxed)) {
        return CXX11_ENUM_QUAL(queue_op_status)empty;
    }

    // Check that there is a value and head didn't move.
    uint_least64_t pos = head % cardinality_;
    value_state old_state = value_state_[pos].load(std::memory_order_acquire);
    if (old_state == CXX11_ENUM_QUAL(value_state)valid) {
        // Found a value, now see if we can keep it.
        if (head_.compare_exchange_strong(head, head + 1,
                                          std::memory_order_relaxed)) {
            // The only place where blocking can occur between threads
            // is here, between the head update and the swap of the
            // pointer to NULL.
            try {
                elem = values_[pos];
            } catch (...) {
                // If the copy fails, we still complete the rest of the
                // operation or else 
                clear_value(CXX11_ENUM_QUAL(value_state)valid, pos);
                // Rethrow to indicate the exception to the caller.
                throw;
            }
            clear_value(CXX11_ENUM_QUAL(value_state)valid, pos);
            return CXX11_ENUM_QUAL(queue_op_status)success;
        }
    } else if (old_state == CXX11_ENUM_QUAL(value_state)invalid) {
        // Have to move the pointer forward since this entry is not going
        // to be filled in.
        head_.compare_exchange_strong(head, head + 1);
        // And fix the state back to empty.
        clear_value(CXX11_ENUM_QUAL(value_state)invalid, pos);
    }
    // NOTE: This last condition is a repeat of the busy state, so no need to
    // check these conditions. If default state changes to non-busy or
    // something, then we have to add this condition back.
    //
    // else if (old_state == CXX11_ENUM_QUAL(value_state)waiting &&
    //           head == head_.load(std::memory_order_relaxed)) {
    //    // Null but head is still the same, so waiting on a value since the
    //    // value_state must be empty right now.
    //    return CXX11_ENUM_QUAL(queue_op_status)busy;
    //}

    // Fall through case, weren't able to complete the operation because
    // someone else popped underneath us.
    return CXX11_ENUM_QUAL(queue_op_status)busy;

    // Other active dequeue threads could cause this one to stall
    // indefinitely, but someone is always trying to make progres.
}

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::try_push(const Value& elem)
{
    queue_op_status status;
    do {
        status = nonblocking_push(elem);
    } while (status == CXX11_ENUM_QUAL(queue_op_status)busy);
    return status;
}

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::nonblocking_push(
    const Value& elem)
{
    // Ordering of head and tail lookups don't matter too much since seeing a
    // stale version of head would cause us to think that the queue is full
    // when it no longer is. Though seeing a stale version of tail would be
    // corrected when we attempt to do the compare-exchange.
    uint_least64_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == (head_.load(std::memory_order_relaxed) + cardinality_)) {
        return CXX11_ENUM_QUAL(queue_op_status)full;
    }

    // Write into the current tail position.
    uint_least64_t pos = tail % cardinality_;
    value_state old_state = value_state_[pos].load(std::memory_order_acquire);
    if (old_state == CXX11_ENUM_QUAL(value_state)valid) {
        // Pop from same position is still pending. We can't do much to
        // help him along unfortunately.
        return CXX11_ENUM_QUAL(queue_op_status)busy;
    } else if (old_state == CXX11_ENUM_QUAL(value_state)invalid) {
        // Someone ended up throwing an exception here, just return
        // busy for now and let the other guy clean it up. Should probably
        // try to move head forward in this case.
        return CXX11_ENUM_QUAL(queue_op_status)busy;
    }

    // Try to reserve the tail, current value_state should be empty!
    if (old_state == CXX11_ENUM_QUAL(value_state)waiting &&
        tail_.compare_exchange_strong(tail, tail + 1,
                                      std::memory_order_relaxed)) {
        // Success! Now push the new element into the value. The only
        // blocking time is between the inc() and the completion of writing
        // the new value_state_ bit.
        try {
            values_[pos] = elem;
            // Force the values_ update to be visible before setting state valid.
        } catch (...) {
            // Set the value invalid since the copy threw an exception.
            // This doesn't guarantee that we can clean up the mess we
            // made. NOTE: might be able to undo the tail update, but it
            // could also cause some ABA issues on the tail value, so need
            // to walk through that logic a bit.
            set_state(CXX11_ENUM_QUAL(value_state)invalid, pos);
            throw;
        }
        set_state(CXX11_ENUM_QUAL(value_state)valid, pos);
        return CXX11_ENUM_QUAL(queue_op_status)success;
    }
    return CXX11_ENUM_QUAL(queue_op_status)busy;
}

#ifdef HAS_CXX11_RVREF

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::try_push(Value&& elem)
{
    queue_op_status status;
    do {
        status = nonblocking_push(std::move(elem));
    } while (status == CXX11_ENUM_QUAL(queue_op_status)busy);
    return status;
}

template <typename Value>
queue_op_status lock_free_buffer_queue<Value>::nonblocking_push(Value&& elem)
{
    // Ordering of head and tail lookups don't matter too much since seeing a
    // stale version of head would cause us to think that the queue is full
    // when it no longer is. Though seeing a stale version of tail would be
    // corrected when we attempt to do the compare-exchange.
    uint_least64_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == (head_.load(std::memory_order_relaxed) + cardinality_)) {
        return CXX11_ENUM_QUAL(queue_op_status)full;
    }

    // Write into the current tail position.
    uint_least64_t pos = tail % cardinality_;
    value_state old_state = value_state_[pos].load(std::memory_order_acquire);
    if (old_state == CXX11_ENUM_QUAL(value_state)valid) {
        // Pop from same position is still pending. We can't do much to
        // help him along unfortunately.
        return CXX11_ENUM_QUAL(queue_op_status)busy;
    } else if (old_state == CXX11_ENUM_QUAL(value_state)invalid) {
        // Someone ended up throwing an exception here, just return
        // busy for now and let the other guy clean it up. Should probably
        // try to move head forward in this case.
        return CXX11_ENUM_QUAL(queue_op_status)busy;
    }

    // Try to reserve the tail, current value_state should be empty!
    if (old_state == CXX11_ENUM_QUAL(value_state)waiting &&
        tail_.compare_exchange_strong(tail, tail + 1,
                                      std::memory_order_relaxed)) {
        // Success! Now push the new element into the value. The only
        // blocking time is between the inc() and the completion of writing
        // the new value_state_ bit.
        try {
            values_[pos] = std::move(elem);
            // Force the values_ update to be visible before setting state valid.
        } catch (...) {
            // Set the value invalid since the copy threw an exception.
            // This doesn't guarantee that we can clean up the mess we
            // made. NOTE: might be able to undo the tail update, but it
            // could also cause some ABA issues on the tail value, so need
            // to walk through that logic a bit.
            set_state(CXX11_ENUM_QUAL(value_state)invalid, pos);
            throw;
        }
        set_state(CXX11_ENUM_QUAL(value_state)valid, pos);
        return CXX11_ENUM_QUAL(queue_op_status)success;
    }
    return CXX11_ENUM_QUAL(queue_op_status)busy;
}

// HAS_CXX11_RVREF
#endif

} // namespace gcl

#endif
