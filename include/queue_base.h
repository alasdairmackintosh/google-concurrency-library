#ifndef QUEUE_BASE_H
#define QUEUE_BASE_H

#include <iterator>

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
    queue_back_iter( queue_back<Element>* q) : q_(q) { }

    const Element& operator *() const { return v_; }
    const Element* operator ->() const { return &v_; }
    queue_back_iter& operator ++();
    queue_back_iter& operator ++(int) { return ++(*this); }

    bool operator ==(const queue_back_iter<Element>& y) { return q_ == y.q_; }
    bool operator !=(const queue_back_iter<Element>& y) { return q_ != y.q_; }

  private:
    queue_back<Element>* q_;
    Element v_;
};

enum class queue_op_status
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

  protected:
    virtual ~queue_common() = default;
};

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
    queue_front() = default;
    queue_front(const queue_front&) = default;
    queue_front& operator =(const queue_front&) = default;
    virtual ~queue_front() = default;
};

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
    queue_back() = default;
    queue_back(const queue_back&) = default;
    queue_back& operator =(const queue_back&) = default;
    virtual ~queue_back() = default;
};

template <typename Element>
queue_front_iter<Element>&
queue_front_iter<Element>::operator =(const Element& value)
{
    queue_op_status s = q_->wait_push(value);
    if ( s != queue_op_status::success )
        q_ = NULL;
    return *this;
    /* FIX Note that if the assignment fails, the value is not pushed,
       and hence may be lost.  It is unclear which solution is best.  */
}

template <typename Element>
queue_back_iter<Element>&
queue_back_iter<Element>::operator ++()
{
    queue_op_status s = q_->wait_pop(v_);
    if ( s == queue_op_status::closed )
        q_ = NULL;
    return *this;
}

} // namespace gcl

#endif
