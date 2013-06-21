// Copyright 2009,2013 Google Inc. All Rights Reserved.
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

#include <iostream>
#include <vector>

#include "counter.h"


// Set up test data.

struct something {
    bool wierd;
};

bool is_suspicious( const something& arg ) { return arg.wierd; }

typedef std::vector< something > container;

container crowd(10);

void initialize_crowd()
{
  for ( unsigned int i = 0; i < crowd.capacity(); i++ )
    if ( i % 4 == 0 ) {
      crowd[i].wierd = true;
    }
};

int number = 3; // expected number of true elements


//FIX Would be nice to have concurrent counting.  :-)


using namespace gcl::counter;

static const atomicity non_atomic = CXX11_ENUM_QUAL(atomicity)none;
static const atomicity semi_atomic = CXX11_ENUM_QUAL(atomicity)semi;
static const atomicity full_atomic = CXX11_ENUM_QUAL(atomicity)full;


template< typename Bumper >
void count_suspicious( Bumper& ctr )
{
    for ( container::iterator i = crowd.begin(); i != crowd.end(); i++ )
        if ( is_suspicious( *i ) )
            ++ctr;
}

template< typename Counter >
void test_counter( Counter& ctr, int number )
{
    assert( ctr.load() == 0 );
    count_suspicious( ctr );
    assert( ctr.load() == number );
}

template< typename Buffer, typename Broker, typename Counter >
void test_buffer( Broker& bkr, Counter& ctr, int number )
{
    {
        Buffer buf( bkr );
        count_suspicious( buf );
        buf.push();
        assert( ctr.load() == 2*number );
        count_suspicious( buf );
        assert( ctr.load() == 2*number );
    }
    assert( ctr.load() == 3*number );
}

template< typename Counter, typename Buffer >
void test_simplex( int number )
{
    static Counter ctr;
    test_counter( ctr, number );
    test_buffer< Buffer >( ctr, ctr, number );
    assert( ctr.exchange( 0 ) == 3*number );
    test_counter( ctr, number );
    test_buffer< Buffer >( ctr, ctr, number );
}

void test_strong_duplex( int number )
{
    static strong_duplex< int > ctr;
    static CXX11_TRIVIAL_THREAD_LOCAL strong_broker< int >* bkr;
    bkr = new strong_broker< int >( ctr );
    test_counter( ctr, number );
    test_buffer< buffer< int > >( ctr, ctr, number );
    assert( ctr.exchange( 0 ) == 3*number );
    test_counter( ctr, number );
    test_buffer< buffer< int > >( *bkr, ctr, number );
    delete bkr;
}

void test_weak_duplex( int number )
{
    static weak_duplex< int > ctr;
    static CXX11_TRIVIAL_THREAD_LOCAL weak_broker< int >* bkr;
    bkr = new weak_broker< int >( ctr );
    test_counter( ctr, number );
    test_buffer< buffer< int, semi_atomic > >( *bkr, ctr, number );
    delete bkr;
}

void test_single_counters()
{
    test_simplex< simplex< int >,
                  buffer< int > >( number );
    test_simplex< simplex< int >,
                  buffer< int, full_atomic, semi_atomic > >( number );
    test_simplex< simplex< int >,
                  buffer< int, full_atomic, full_atomic > >( number );

    test_simplex< simplex< int, semi_atomic >,
                  buffer< int, semi_atomic > >( number );
    test_simplex< simplex< int, semi_atomic >,
                  buffer< int, semi_atomic, semi_atomic > >( number );
    test_simplex< simplex< int, semi_atomic >,
                  buffer< int, semi_atomic, full_atomic > >( number );

    test_simplex< simplex< int, non_atomic >,
                  buffer< int, non_atomic > >( number );
    test_simplex< simplex< int, non_atomic >,
                  buffer< int, non_atomic, semi_atomic > >( number );
    test_simplex< simplex< int, non_atomic >,
                  buffer< int, non_atomic, full_atomic > >( number );

    test_strong_duplex( number );
    test_weak_duplex( number );
}

int modulus = 3;

template< typename Bumper >
void hist_suspicious( Bumper& ctr )
{
    typename Bumper::size_type x = 0;
    for ( container::iterator i = crowd.begin(); i != crowd.end(); i++ ) {
        if ( is_suspicious( *i ) )
            ++ctr[ x % modulus ];
        ++x;
    }
}

template< typename Counter >
void verify_array_values( Counter& ctr, int number )
{
    for ( int i = 0; i < modulus; ++i )
        assert( ctr.load( i ) == number / modulus );
}

template< typename Counter >
void test_counter_array( Counter& ctr, int number )
{
    verify_array_values( ctr, 0 );
    hist_suspicious( ctr );
    verify_array_values( ctr, number );
}

template< typename Buffer, typename Broker, typename Counter >
void test_buffer_array( Broker& bkr, Counter& ctr, int number )
{
    {
        Buffer buf( bkr );
        hist_suspicious( buf );
        buf.push();
        verify_array_values( ctr, 2*number );
        hist_suspicious( buf );
        verify_array_values( ctr, 2*number );
    }
    verify_array_values( ctr, 3*number );
}

template< typename Counter, typename Buffer >
void test_simplex_array( int number )
{
    Counter ctr( modulus );
    test_counter_array( ctr, number );
    test_buffer_array< Buffer >( ctr, ctr, number );
    for ( int i = 0; i < modulus; ++i )
        assert( ctr.exchange( i, 0 ) == 3*number / modulus );
    test_counter_array( ctr, number );
    test_buffer_array< Buffer >( ctr, ctr, number );
}

void test_arrays_counters()
{
    test_simplex_array< simplex_array< int >,
                  buffer_array< int > >( number );
    test_simplex_array< simplex_array< int >,
                  buffer_array< int, full_atomic, semi_atomic > >( number );
    test_simplex_array< simplex_array< int >,
                  buffer_array< int, full_atomic, full_atomic > >( number );

    test_simplex_array< simplex_array< int, semi_atomic >,
                  buffer_array< int, semi_atomic > >( number );
    test_simplex_array< simplex_array< int, semi_atomic >,
                  buffer_array< int, semi_atomic, semi_atomic > >( number );
    test_simplex_array< simplex_array< int, semi_atomic >,
                  buffer_array< int, semi_atomic, full_atomic > >( number );

    test_simplex_array< simplex_array< int, non_atomic >,
                  buffer_array< int, non_atomic > >( number );
    test_simplex_array< simplex_array< int, non_atomic >,
                  buffer_array< int, non_atomic, semi_atomic > >( number );
    test_simplex_array< simplex_array< int, non_atomic >,
                  buffer_array< int, non_atomic, full_atomic > >( number );
}

int main()
{
    initialize_crowd();
    test_single_counters();
    test_arrays_counters();
    return 0;
}
