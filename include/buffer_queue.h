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

#ifndef BUFFER_QUEUE_H
#define BUFFER_QUEUE_H

#include "mutex.h"
#include "condition_variable.h"

#include "queue_base.h"

namespace gcl {

template <typename Element>
class buffer_queue
:
    public queue_base<Element>
{
  public:
    buffer_queue() CXX0X_DELETED
    buffer_queue(const buffer_queue&) CXX0X_DELETED
    buffer_queue(size_t max_elems, const char* name);
    explicit buffer_queue(size_t max_elems);
    template <typename Iter>
    buffer_queue(size_t max_elems, Iter first, Iter last, const char* name);
    template <typename Iter>
    buffer_queue(size_t max_elems, Iter first, Iter last);
    buffer_queue& operator =(const buffer_queue&) CXX0X_DELETED
    virtual ~buffer_queue();

    virtual void close();
    virtual bool is_closed();
    virtual bool is_empty();

    virtual Element pop();
    virtual queue_op_status try_pop(Element&);
    virtual queue_op_status wait_pop(Element&);

    virtual void push(const Element& x);
    virtual queue_op_status try_push(const Element& x);
    virtual queue_op_status wait_push(const Element& x);

    virtual const char* name();

  private:
    mutex mtx_;
    condition_variable not_empty_;
    condition_variable not_full_;
    size_t waiting_full_;
    size_t waiting_empty_;
    Element* buffer_;
    size_t push_index_;
    size_t pop_index_;
    size_t num_slots_;
    bool closed_;
    const char* name_;

    void init(size_t max_elems);

    template <typename Iter>
    void iter_init(size_t max_elems, Iter first, Iter last);

    size_t next(size_t idx) { return (idx + 1) % num_slots_; }

    void pop_from(Element& elem, size_t pdx, size_t hdx)
    {
        pop_index_ = next( pdx );
        elem = buffer_[pdx];
        if ( waiting_full_ > 0 ) {
            --waiting_full_;
            not_full_.notify_one();
        }
    }

    void push_at(const Element& elem, size_t hdx, size_t nxt, size_t pdx)
    {
        buffer_[hdx] = elem;
        push_index_ = nxt;
        if ( waiting_empty_ > 0 ) {
            --waiting_empty_;
            not_empty_.notify_one();
        }
    }

};

template <typename Element>
void buffer_queue<Element>::init(size_t max_elems)
{
    if ( max_elems < 1 )
        throw std::invalid_argument("number of elements must be at least one");
}

template <typename Element>
buffer_queue<Element>::buffer_queue(size_t max_elems, const char* name)
:
    waiting_full_( 0 ),
    waiting_empty_( 0 ),
    buffer_( new Element[max_elems+1] ),
    push_index_( 0 ),
    pop_index_( 0 ),
    num_slots_( max_elems+1 ),
    closed_( false ),
    name_( name )
{
    init(max_elems);
}

template <typename Element>
buffer_queue<Element>::buffer_queue(size_t max_elems)
:
    // would rather do buffer_queue(max_elems, "")
    waiting_full_( 0 ),
    waiting_empty_( 0 ),
    buffer_( new Element[max_elems+1] ),
    push_index_( 0 ),
    pop_index_( 0 ),
    num_slots_( max_elems+1 ),
    closed_( false ),
    name_( "" )
{
    init(max_elems);
}

template <typename Element>
template <typename Iter>
void buffer_queue<Element>::iter_init(size_t max_elems, Iter first, Iter last)
{
    size_t hdx = 0;
    for ( Iter cur = first; cur != last; ++cur ) {
        if ( hdx >= max_elems )
            throw std::invalid_argument("too few slots for iterator");
        size_t nxt = hdx + 1; // more efficient than next(hdx)
        size_t pdx = pop_index_;
        push_at( *cur, hdx, nxt, pdx );
        hdx = nxt;
    }
}

template <typename Element>
template <typename Iter>
buffer_queue<Element>::buffer_queue(size_t max_elems, Iter first, Iter last,
                                    const char* name)
:
    // would rather do buffer_queue(max_elems, name)
    waiting_full_( 0 ),
    waiting_empty_( 0 ),
    buffer_( new Element[max_elems+1] ),
    push_index_( 0 ),
    pop_index_( 0 ),
    num_slots_( max_elems+1 ),
    closed_( false ),
    name_( name )
{
    iter_init(max_elems, first, last);
}

template <typename Element>
template <typename Iter>
buffer_queue<Element>::buffer_queue(size_t max_elems, Iter first, Iter last)
:
    // would rather do buffer_queue(max_elems, first, last, "")
    waiting_full_( 0 ),
    waiting_empty_( 0 ),
    buffer_( new Element[max_elems+1] ),
    push_index_( 0 ),
    pop_index_( 0 ),
    num_slots_( max_elems+1 ),
    closed_( false ),
    name_( "" )
{
    iter_init(max_elems, first, last);
}

template <typename Element>
buffer_queue<Element>::~buffer_queue()
{
    delete[] buffer_;
}

template <typename Element>
void buffer_queue<Element>::close()
{
    lock_guard<mutex> hold( mtx_ );
    closed_ = true;
    not_empty_.notify_all();
    not_full_.notify_all();
}

template <typename Element>
bool buffer_queue<Element>::is_closed()
{
    lock_guard<mutex> hold( mtx_ );
    return closed_;
}

template <typename Element>
bool buffer_queue<Element>::is_empty()
{
    lock_guard<mutex> hold( mtx_ );
    return push_index_ == pop_index_;
}

template <typename Element>
queue_op_status buffer_queue<Element>::try_pop(Element& elem)
{
    /* This try block is here to catch exceptions from the mutex
       operations or from the user-defined copy assignment operator
       in the pop_from operation. */
    try {
        lock_guard<mutex> hold( mtx_ );
        size_t pdx = pop_index_;
        size_t hdx = push_index_;
        if ( pdx == hdx ) {
            if ( closed_ )
                return CXX0X_ENUM_QUAL(queue_op_status)closed;
            else
                return CXX0X_ENUM_QUAL(queue_op_status)empty;
        }
        pop_from( elem, pdx, hdx );
        return CXX0X_ENUM_QUAL(queue_op_status)success;
    } catch (...) {
        close();
        throw;
    }
}

template <typename Element>
queue_op_status buffer_queue<Element>::wait_pop(Element& elem)
{
    /* This try block is here to catch exceptions from the mutex
       operations or from the user-defined copy assignment operator
       in the pop_from operation. */
    try {
        unique_lock<mutex> hold( mtx_ );
        size_t pdx;
        size_t hdx;
        for (;;) {
            pdx = pop_index_;
            hdx = push_index_;
            if ( pdx != hdx )
                break;
            if ( closed_ )
                return CXX0X_ENUM_QUAL(queue_op_status)closed;
            ++waiting_empty_;
            not_empty_.wait( hold );
        }
        pop_from( elem, pdx, hdx );
        return CXX0X_ENUM_QUAL(queue_op_status)success;
    } catch (...) {
        close();
        throw;
    }
}

template <typename Element>
Element buffer_queue<Element>::pop()
{
    /* This try block is here to catch exceptions from the
       user-defined copy assignment operator. */
    try {
        Element elem;
        if ( wait_pop( elem ) == CXX0X_ENUM_QUAL(queue_op_status)closed )
            throw CXX0X_ENUM_QUAL(queue_op_status)closed;
        return elem;
    } catch (...) {
        close();
        throw;
    }
}

template <typename Element>
queue_op_status buffer_queue<Element>::try_push(const Element& elem)
{
    /* This try block is here to catch exceptions from the mutex
       operations or from the user-defined copy assignment
       operator in push_at. */
    try {
        lock_guard<mutex> hold( mtx_ );
        if ( closed_ )
            return CXX0X_ENUM_QUAL(queue_op_status)closed;
        size_t hdx = push_index_;
        size_t nxt = next( hdx );
        size_t pdx = pop_index_;
        if ( nxt == pdx )
            return CXX0X_ENUM_QUAL(queue_op_status)full;
        push_at( elem, hdx, nxt, pdx );
        return CXX0X_ENUM_QUAL(queue_op_status)success;
    } catch (...) {
        close();
        throw;
    }
}

template <typename Element>
queue_op_status buffer_queue<Element>::wait_push(const Element& elem)
{
    /* This try block is here to catch exceptions from the mutex
       operations or from the user-defined copy assignment
       operator in push_at. */
    try {
        unique_lock<mutex> hold( mtx_ );
        size_t hdx;
        size_t nxt;
        size_t pdx;
        for (;;) {
            if ( closed_ )
                return CXX0X_ENUM_QUAL(queue_op_status)closed;
            hdx = push_index_;
            nxt = next( hdx );
            pdx = pop_index_;
            if ( nxt != pdx )
                break;
            ++waiting_full_;
            not_full_.wait( hold );
        }
        push_at( elem, hdx, nxt, pdx );
        return CXX0X_ENUM_QUAL(queue_op_status)success;
    } catch (...) {
        close();
        throw;
    }
}

template <typename Element>
void buffer_queue<Element>::push(const Element& elem)
{
    /* Only wait_push can throw, and it protects itself, so there
       is no need to try/catch here. */
    if ( wait_push( elem ) == CXX0X_ENUM_QUAL(queue_op_status)closed )
        throw CXX0X_ENUM_QUAL(queue_op_status)closed;
}

template <typename Element>
const char* buffer_queue<Element>::name()
{
    return name_;
}

} // namespace gcl

#endif
