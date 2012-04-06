#ifndef BUFFER_QUEUE_H
#define BUFFER_QUEUE_H

#include "mutex.h"
#include "condition_variable.h"
#include "queue_base.h"

namespace gcl {

template <typename Element>
class buffer_queue
:
    public queue_front<Element>, public queue_back<Element>
{
  public:
    buffer_queue() = delete;
    buffer_queue(const buffer_queue&) = delete;
    buffer_queue(size_t max_elems);
    template <typename Iter>
    buffer_queue(size_t max_elems, Iter first, Iter last);
    buffer_queue& operator =(const buffer_queue&) = delete;
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

  private:
    mutex mtx_;
    condition_variable not_empty_;
    condition_variable not_full_;
    Element* buffer_;
    size_t push_index_;
    size_t pop_index_;
    size_t num_slots_;
    bool closed_;

    size_t next(size_t idx) { return (idx + 1) % num_slots_; }

    void pop_from(Element& elem, size_t idx, size_t hdx)
    {
        pop_index_ = next( idx );
        elem = buffer_[idx];
        if ( next( hdx ) == idx )
            not_full_.notify_one();
    }

    void push_at(const Element& elem, size_t idx, size_t nxt)
    {
        buffer_[idx] = elem;
        push_index_ = nxt;
        if ( idx == pop_index_ )
            not_empty_.notify_one();
    }

};

template <typename Element>
buffer_queue<Element>::buffer_queue(size_t max_elems)
:
    buffer_( new Element[max_elems+1] ),
    push_index_( 0 ),
    pop_index_( 0 ),
    num_slots_( max_elems+1 ),
    closed_( false )
{
    if ( max_elems < 1 )
        throw std::invalid_argument("number of elements must be at least one");
}

template <typename Element>
template <typename Iter>
buffer_queue<Element>::buffer_queue(size_t num_elems, Iter first, Iter last)
:
    buffer_( new Element[num_elems+1] ),
    push_index_( 0 ),
    pop_index_( 0 ),
    num_slots_( num_elems+1 ),
    closed_( false )
{
    size_t idx = 0;
    for ( Iter cur = first; cur != last; ++cur ) {
        if ( idx >= num_elems )
            throw std::invalid_argument("two few slots for iterator");
        size_t nxt = idx + 1;
        push_at( *cur, idx, nxt );
        idx = nxt;
    }
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
    lock_guard<mutex> hold( mtx_ );
    size_t idx = pop_index_;
    size_t hdx = push_index_;
    if ( idx == hdx ) {
        if ( closed_ )
            return queue_op_status::closed;
        else
            return queue_op_status::empty;
    }
    pop_from( elem, idx, hdx );
    return queue_op_status::success;
}

template <typename Element>
queue_op_status buffer_queue<Element>::wait_pop(Element& elem)
{
    unique_lock<mutex> hold( mtx_ );
    size_t idx;
    size_t hdx;
    for (;;) {
        if ( closed_ )
            return queue_op_status::closed;
        idx = pop_index_;
        hdx = push_index_;
        if ( idx != hdx )
            break;
        not_empty_.wait( hold );
    }
    pop_from( elem, idx, hdx );
    return queue_op_status::success;
}

template <typename Element>
Element buffer_queue<Element>::pop()
{
    Element elem;
    if ( wait_pop( elem ) == queue_op_status::closed )
        throw queue_op_status::closed;
    return elem;
}

template <typename Element>
queue_op_status buffer_queue<Element>::try_push(const Element& elem)
{
    lock_guard<mutex> hold( mtx_ );
    if ( closed_ )
        return queue_op_status::closed;
    size_t idx = push_index_;
    size_t nxt = next( idx );
    if ( nxt == pop_index_ ) {
        return queue_op_status::full;
    }
    push_at( elem, idx, nxt );
    return queue_op_status::success;
}

template <typename Element>
queue_op_status buffer_queue<Element>::wait_push(const Element& elem)
{
    unique_lock<mutex> hold( mtx_ );
    size_t idx;
    size_t nxt;
    for (;;) {
        if ( closed_ )
            return queue_op_status::closed;
        idx = push_index_;
        nxt = next( idx );
        if ( nxt != pop_index_ )
            break;
        not_full_.wait( hold );
    }
    push_at( elem, idx, nxt );
    return queue_op_status::success;
}

template <typename Element>
void buffer_queue<Element>::push(const Element& elem)
{
    if ( wait_push( elem ) == queue_op_status::closed )
        throw queue_op_status::closed;
}

} // namespace gcl

#endif
