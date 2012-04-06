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

#include "unordered_set.h"

#include "atomic.h"
#include "mutex.h"

namespace gcl {

/*


COUNTERS

This header implements highly concurrent counters.
The intent is to minimize the cost of incrementing the counter,
accepting increased costs to obtain the count.
That is, these counters are appropriate to code with
very frequent counter increments but relatively rare counter reads.

These counters are parameterized by the base integer type
that maintains the count.
Avoid situations that overflow the integer.

The base of the design is an atomic_counter,
which provides atomicity, but without really reducing increment cost
over the plain atomic<int>.
The counter methods are:

    <constructor>( integer ):
    The parameter is the initial counter value.

    <constructor>():
    Equivalent to an initial value of zero.

    void inc( integer ):
    Add a value to the counter.  There is no default value.
    void dec( integer ):
    Subtract a value from the counter.  There is no default value.

    integer load():
    Returns the value of the counter.

    integer exchange( integer ):
    Replaces the existing count by the count in the parameter
    and returns the previous count,
    which enables safe concurrent count extraction.

    There are no copy or assignment operations.

A serial_counter is available
to ease conversion from concurrent to serial code.
It provides the same interface,
but without the cost of atomicity.


COUNTER BUFFERS

The cost of incrementing the counter is reduced
by placing a serial_counter_buffer in front of the atomic_counter.

    atomic_counter<int> suspicious_count( 0 );

    void count_suspicious( std::vector< something > arg )
        serial_counter_buffer cntbuf( suspicious_count );
        for ( arg::iterator i = arg.begin(); i != arg.end(); i++ )
            if ( is_suspicious( *i ) )
                cntbuf.inc(1);
    }

The lifetime of the counter must be strictly larger than
the lifetimes of any buffers attached to it.
Transfer the count within a buffer to its counter
with the push method.
Any increments on buffers since the last push
will not be reflected in the value reported by a load of the counter.
The destructor does an implicit push.

An atomic_counter_buffer is available for processor-local buffers,
which can substantially reduce machine-to-machine communication.

You can also place a serial_counter_buffer in front of an
atomic_counter_buffer to further reduce costs.

    atomic_counter<int> global_counter( 0 );
    atomic_counter_buffer<int> processor_local( global_counter );
    serial_counter_buffer<int> function_local( processor_local );


DUPLEX COUNTERS

The push model of counters sometimes yields an unacceptable lag
in the observed value of the count.
To avoid this lag,
the duplex_counter and its buffer provide a pull model of counters.

    duplex_counter<int> suspicious_count( 3 );

    void count_suspicious( std::vector< something > arg )
        duplex_counter_buffer cntbuf( suspicious_count );
        for ( arg::iterator i = arg.begin(); i != arg.end(); i++ )
            if ( is_suspicious( *i ) )
                cntbuf.inc(1);
    }

Another thread may call suspicious_count.load() and get the current count.
That operation will poll each buffer for its count and return the sum.
Naturally, any incs done to a buffer after it is polled will be missed,
but no counts will be lost.

The exchange operation works by atomically transfering
buffer counts to the main counter.
That is, every count will be extracted by one and only one exchange operation.


WEAK COUNTERS

Duplex counters can are expensive because 
the counter exchange operation and the buffer inc/dec operations
require write concurrency to the same object.
To reduce that concurrency cost,
the weak counter and its buffer
do not provide the exchange operation.
This difference
means that the polling is a read-only operation
and requires less synchronization.
Use this counter when you do not intend to extract values.

*/

/*
    The plain counters come in serial and atomic flavors

    The counter_bumper class provides the minimal inc/dec interface and
    serves as the base class for atomic_counter, atomic_counter_buffer
    and duplex_counter_buffer.
*/

template< typename Integral > class serial_counter
{
public:
    CXX0X_CONSTEXPR_CTOR serial_counter() : value_( 0 ) {}
    CXX0X_CONSTEXPR_CTOR serial_counter( Integral in ) : value_( in ) {}
    serial_counter( const serial_counter& ) CXX0X_DELETED
    serial_counter& operator=( const serial_counter& ) CXX0X_DELETED
    void inc( Integral by ) { value_ += by; }
    void dec( Integral by ) { value_ -= by; }
    Integral load() { return value_; }
    Integral exchange( Integral to )
        { Integral tmp = value_; value_ = to; return tmp; }
private:
    Integral value_;
};

template< typename Integral > class counter_bumper
{
public:
    CXX0X_CONSTEXPR_CTOR counter_bumper( Integral in ) : value_( in ) {}
    counter_bumper( const counter_bumper& ) CXX0X_DELETED
    counter_bumper& operator=( const counter_bumper& ) CXX0X_DELETED
    void inc( Integral by )
        { value_.fetch_add( by, std::memory_order_relaxed ); }
    void dec( Integral by )
        { value_.fetch_sub( by, std::memory_order_relaxed ); }

protected:
    std::atomic< Integral > value_;
};

template< typename Integral > class atomic_counter
: public counter_bumper< Integral >
{
public:
    CXX0X_CONSTEXPR_CTOR atomic_counter() : counter_bumper< Integral >( 0 ) {}
    CXX0X_CONSTEXPR_CTOR atomic_counter( Integral in )
        : counter_bumper< Integral >( in ) {}
    atomic_counter( const atomic_counter& ) CXX0X_DELETED
    atomic_counter& operator=( const atomic_counter& ) CXX0X_DELETED
    Integral load() { return value_.load( std::memory_order_relaxed ); }
    Integral exchange( Integral to )
        { return value_.exchange( to, std::memory_order_relaxed ); }
private:
    using counter_bumper< Integral >::value_;
};

/*
   Counter buffers reduce contention on plain counters.
   They require an atomic_counter as their constructor parameter,
   i.e. the primary counter.
   The lifetime of the prime must cover the lifetime of the buffer.
   Any increments in the buffer since the last buffer::push call
   are not reflected in the queries on the prime.
*/

template< typename Integral > class serial_counter_buffer
{
public:
    serial_counter_buffer( atomic_counter< Integral >& p )
        : value_( 0 ), prime_( p ) {}
    serial_counter_buffer() CXX0X_DELETED
    serial_counter_buffer( const serial_counter_buffer& ) CXX0X_DELETED
    serial_counter_buffer& operator=( const serial_counter_buffer& )
        CXX0X_DELETED
    void inc( Integral by ) { value_ += by; }
    void dec( Integral by ) { value_ -= by; }
    void push() { prime_.inc( value_ ); value_ = 0; }
    ~serial_counter_buffer() { push(); }
private:
    Integral value_;
    atomic_counter< Integral >& prime_;
};

template< typename Integral > class atomic_counter_buffer
: public counter_bumper< Integral >
{
public:
    atomic_counter_buffer( counter_bumper< Integral >& p )
        : counter_bumper< Integral >( 0 ), prime_( p ) {}
    atomic_counter_buffer() CXX0X_DELETED
    atomic_counter_buffer( const atomic_counter_buffer& ) CXX0X_DELETED
    atomic_counter_buffer& operator=( const atomic_counter_buffer& )
        CXX0X_DELETED

    void push()
        { prime_.inc( value_.exchange( 0, std::memory_order_relaxed ) ); }

    ~atomic_counter_buffer() { push(); }
private:
    counter_bumper< Integral >& prime_;
    using counter_bumper< Integral >::value_;
};


/*
   Weak and duplex counters enable a "pull" model of buffering.
   Each counter, the prime, may have one or more buffers.
   The lifetime of the prime must cover the lifetime of the buffers.
   The duplex counter queries may fail to detect any incs done concurrently.
   The query operations are expected to be rare relative to the incs.
   The weak counter does not support exchange or drain operations,
   which means that you cannot extract counts early.
   The weak counter also does not support push buffers.
   These restrictions enable a more efficient inc.
*/

template< typename Integral > class weak_counter_buffer;

template< typename Integral > class weak_counter
{
public:
    weak_counter() : value_( 0 ) {}
    weak_counter( Integral in ) : value_( in ) {}
    void inc( Integral by );
    void dec( Integral by );
    Integral load();
    ~weak_counter();
private:
    friend class weak_counter_buffer<Integral>;
    void insert( weak_counter_buffer< Integral >* child );
    void erase( weak_counter_buffer< Integral >* child, Integral by );
    mutex serializer_;
    Integral value_;
    typedef std::unordered_set< weak_counter_buffer< Integral >* > set_type;
    set_type children_;
};

template< typename Integral >
class weak_counter_buffer
{
public:
    weak_counter_buffer( weak_counter< Integral >& p );
    weak_counter_buffer() CXX0X_DELETED
    weak_counter_buffer( const weak_counter_buffer& ) CXX0X_DELETED
    weak_counter_buffer& operator=( const weak_counter_buffer& ) CXX0X_DELETED
    void inc( Integral by ) {
        value_.store( value_.load( std::memory_order_relaxed ) + by,
                      std::memory_order_relaxed ); }
    void dec( Integral by ) {
        value_.store( value_.load( std::memory_order_relaxed ) - by,
                      std::memory_order_relaxed ); }
    ~weak_counter_buffer();
private:
    friend class weak_counter<Integral>;
    Integral poll() { return value_.load( std::memory_order_relaxed ); }
    std::atomic< Integral > value_;
    weak_counter< Integral >& prime_;
};

template< typename Integral > class duplex_counter_buffer;

template< typename Integral > class duplex_counter
: public counter_bumper< Integral >
{
public:
    duplex_counter() : counter_bumper< Integral >( 0 ) {}
    duplex_counter( Integral in ) : counter_bumper< Integral >( in ) {}
    Integral load();
    Integral exchange( Integral to );
    ~duplex_counter();
private:
    friend class duplex_counter_buffer<Integral>;
    void insert( duplex_counter_buffer< Integral >* child );
    void erase( duplex_counter_buffer< Integral >* child, Integral by );
    mutex serializer_;
    typedef std::unordered_set< duplex_counter_buffer< Integral >* > set_type;
    set_type children_;
    using counter_bumper< Integral >::value_;
};

template< typename Integral >
class duplex_counter_buffer
: public counter_bumper< Integral >
{
public:
    duplex_counter_buffer( duplex_counter< Integral >& p );
    duplex_counter_buffer() CXX0X_DELETED
    duplex_counter_buffer( const duplex_counter_buffer& ) CXX0X_DELETED
    duplex_counter_buffer& operator=( const duplex_counter_buffer& )
        CXX0X_DELETED
    ~duplex_counter_buffer();
private:
    friend class duplex_counter<Integral>;
    Integral poll() { return value_.load( std::memory_order_relaxed ); }
    Integral drain() { return value_.exchange( 0, std::memory_order_relaxed ); }
    duplex_counter< Integral >& prime_;
    using counter_bumper< Integral >::value_;
};

/*
   The following operations are defined out-of-line
   to break cyclic dependences in the class definitions.
   They are expected to be rarely called, and efficiency is less of a concern.
*/

template< typename Integral >
void
weak_counter< Integral >::insert( weak_counter_buffer< Integral >* child )
{
    lock_guard< mutex > _( serializer_ );
    children_.insert( child );
}

template< typename Integral >
void
weak_counter< Integral >::erase( weak_counter_buffer< Integral >* child,
                                 Integral by )
{
    lock_guard< mutex > _( serializer_ );
    value_ += by;
    children_.erase( child );
}

template< typename Integral >
void
weak_counter< Integral >::inc( Integral by )
{
    lock_guard< mutex > _( serializer_ );
    value_ += by;
}

template< typename Integral >
void
weak_counter< Integral >::dec( Integral by )
{
    lock_guard< mutex > _( serializer_ );
    value_ -= by;
}

template< typename Integral >
Integral
weak_counter< Integral >::load()
{
    typedef typename set_type::iterator iterator;
    Integral tmp = 0;
    {
        lock_guard< mutex > _( serializer_ );
        iterator rollcall = children_.begin();
        for ( ; rollcall != children_.end(); rollcall++ )
            tmp += (*rollcall)->poll();
        tmp += value_;
    }
    return tmp;
}

template< typename Integral >
weak_counter< Integral >::~weak_counter()
{
    lock_guard< mutex > _( serializer_ );
    assert( children_.size() == 0 );
}

template< typename Integral >
weak_counter_buffer< Integral >::weak_counter_buffer(
    weak_counter< Integral >& p )
:
    value_( 0 ), prime_( p )
{
    prime_.insert( this );
}

template< typename Integral >
weak_counter_buffer< Integral >::~weak_counter_buffer()
{
    prime_.erase( this, value_.load( std::memory_order_relaxed ) );
}

template< typename Integral >
void
duplex_counter< Integral >::insert( duplex_counter_buffer< Integral >* child )
{
    lock_guard< mutex > _( serializer_ );
    children_.insert( child );
}

template< typename Integral >
void
duplex_counter< Integral >::erase( duplex_counter_buffer< Integral >* child,
                                   Integral by )
{
    value_.fetch_add( by, std::memory_order_relaxed );
    lock_guard< mutex > _( serializer_ );
    children_.erase( child );
}

template< typename Integral >
Integral
duplex_counter< Integral >::load()
{
    typedef typename set_type::iterator iterator;
    Integral tmp = 0;
    {
        lock_guard< mutex > _( serializer_ );
        iterator rollcall = children_.begin();
        for ( ; rollcall != children_.end(); rollcall++ )
            tmp += (*rollcall)->poll();
    }
    return tmp + value_.load( std::memory_order_relaxed );
}

template< typename Integral >
Integral
duplex_counter< Integral >::exchange( Integral to )
{
    typedef typename set_type::iterator iterator;
    Integral tmp = 0;
    {
        lock_guard< mutex > _( serializer_ );
        iterator rollcall = children_.begin();
        for ( ; rollcall != children_.end(); rollcall++ )
            tmp += (*rollcall)->drain();
    }
    return tmp + value_.exchange( to, std::memory_order_relaxed );
}

template< typename Integral >
duplex_counter< Integral >::~duplex_counter()
{
    lock_guard< mutex > _( serializer_ );
    assert( children_.size() == 0 );
}

template< typename Integral >
duplex_counter_buffer< Integral >::duplex_counter_buffer(
    duplex_counter< Integral >& p )
:
    counter_bumper< Integral >( 0 ), prime_( p )
{
    prime_.insert( this );
}

template< typename Integral >
duplex_counter_buffer< Integral >::~duplex_counter_buffer()
{
    prime_.erase( this, value_.load( std::memory_order_relaxed ) );
}

} // namespace gcl
