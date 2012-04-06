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

#ifndef QUEUE_BASE_H
#define QUEUE_BASE_H

#include <stddef.h>

#include <iterator>
#include <iostream>

#include "cxx0x.h"

namespace gcl {

template <typename Element>
class queue_front;

template <typename Element>
class queue_back;

template <typename Element>
class queue_front_iter
:
    public std::iterator<std::output_iterator_tag, Element,
                         ptrdiff_t, const Element*, const Element&>
{
  public:
    queue_front_iter( queue_front<Element>* q) : q_(q) { }

    queue_front_iter& operator *() { return *this; }
    queue_front_iter& operator ++() { return *this; }
    queue_front_iter& operator ++(int) { return *this; }
    queue_front_iter& operator =(const Element& value);

    bool operator ==(const queue_front_iter<Element>& y) { return q_ == y.q_; }
    bool operator !=(const queue_front_iter<Element>& y) { return q_ != y.q_; }

  private:
    queue_front<Element>* q_;
};

template <typename Element>
class queue_back_iter
:
    public std::iterator<std::input_iterator_tag, Element,
                         ptrdiff_t, const Element*, const Element&>
{
  public:
    class value
    {
      public:
        value( Element v) : v_(v) { }
        Element operator *() const { return v_; }
      private:
        Element v_;
    };

    queue_back_iter( queue_back<Element>* q) : q_(q) { if ( q ) next(); }

    const Element& operator *() const { return v_; }
    const Element* operator ->() const { return &v_; }
    queue_back_iter& operator ++() { next(); return *this; }
    value operator ++(int) { value t = v_; next(); return t; }

    bool operator ==(const queue_back_iter<Element>& y) { return q_ == y.q_; }
    bool operator !=(const queue_back_iter<Element>& y) { return q_ != y.q_; }

  private:
    void next();

    queue_back<Element>* q_;
    Element v_;
};

CXX0X_ENUM_CLASS queue_op_status
{
    success = 0,
    empty,
    full,
    closed
};

template <typename Element>
class queue_common
{
  public:
    typedef Element& reference;
    typedef const Element& const_reference;
    typedef Element value_type;

    virtual void close() = 0;
    virtual bool is_closed() = 0;
    virtual bool is_empty() = 0;

    virtual const char* name() = 0;

  protected:
    virtual ~queue_common();
};

template <typename Element>
queue_common<Element>::~queue_common() CXX0X_DEFAULTED_EASY

template <typename Element>
class queue_front
:
    public virtual queue_common<Element>
{
  public:
    typedef queue_front_iter<Element> iterator;
    typedef const queue_front_iter<Element> const_iterator;

    iterator begin() { return queue_front_iter<Element>(this); }
    iterator end() { return queue_front_iter<Element>(NULL); }
    const iterator cbegin() { return queue_front_iter<Element>(this); }
    const iterator cend() { return queue_front_iter<Element>(NULL); }

    virtual void push(const Element& x) = 0;
    virtual queue_op_status try_push(const Element& x) = 0;
    virtual queue_op_status wait_push(const Element& x) = 0;

  protected:
    virtual ~queue_front();
};

template <typename Element>
queue_front<Element>::~queue_front() CXX0X_DEFAULTED_EASY

template <typename Element>
class queue_back
:
    public virtual queue_common<Element>
{
  public:
    typedef queue_back_iter<Element> iterator;
    typedef queue_back_iter<Element> const_iterator;

    iterator begin() { return queue_back_iter<Element>(this); }
    iterator end() { return queue_back_iter<Element>(NULL); }
    const iterator cbegin() { return queue_back_iter<Element>(this); }
    const iterator cend() { return queue_back_iter<Element>(NULL); }

    virtual Element pop() = 0;
    virtual queue_op_status try_pop(Element&) = 0;
    virtual queue_op_status wait_pop(Element&) = 0;

  protected:
    virtual ~queue_back();
};

template <typename Element>
queue_back<Element>::~queue_back() CXX0X_DEFAULTED_EASY

template <typename Element>
queue_front_iter<Element>&
queue_front_iter<Element>::operator =(const Element& value)
{
    queue_op_status s = q_->wait_push(value);
    if ( s != CXX0X_ENUM_QUAL(queue_op_status)success ) {
        q_ = NULL;
        throw s;
    }
    return *this;
}


template <typename Element>
void
queue_back_iter<Element>::next()
{
    queue_op_status s = q_->wait_pop(v_);
    if ( s == CXX0X_ENUM_QUAL(queue_op_status)closed )
        q_ = NULL;
}

template <typename Element>
class queue_base
:
    public virtual queue_front<Element>, public queue_back<Element>
{
  public:
    virtual ~queue_base();
};

template <typename Element>
queue_base<Element>::~queue_base()
{
}

} // namespace gcl

#endif
