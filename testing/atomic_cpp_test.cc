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

#include "atomic.h"

#ifdef __cplusplus
using namespace std;
#endif

//TODO(Crowl): Add more meaningful tests.

atomic_flag af = ATOMIC_FLAG_INIT;

void flag_example( void )
{
    if ( ! atomic_flag_test_and_set_explicit( &af, memory_order_acquire ) )
        atomic_flag_clear_explicit( &af, memory_order_release );
#ifdef __cplusplus
    if ( ! af.test_and_set() )
        af.clear();
#endif
}


atomic_bool lazy_ready = { false };
atomic_bool lazy_assigned = { false };
int lazy_value;

#ifdef __cplusplus

int lazy_example_strong_cpp( void )
{
    if ( ! lazy_ready.load() ) {
        /* the value is not yet ready */
        if ( lazy_assigned.exchange( true ) ) {
            /* initialization assigned to another thread; wait */
            while ( ! lazy_ready.load() );
        }
        else {
            lazy_value = 42;
            lazy_ready = true;
        }
    }
    return lazy_value;
}

#endif

int lazy_example_weak_c( void )
{
    if ( ! atomic_load_explicit( &lazy_ready, memory_order_acquire ) ) {
        if ( atomic_exchange_explicit( &lazy_assigned, true,
                                   memory_order_relaxed ) ) {
            while ( ! atomic_load_explicit( &lazy_ready,
                                            memory_order_acquire ) );
        }
        else {
            lazy_value = 42;
            (void)atomic_store_explicit( &lazy_ready, true, memory_order_release );
        }
    }
    return lazy_value;
}

#ifdef __cplusplus

int lazy_example_fence_cpp( void )
{
    if ( lazy_ready.load( memory_order_relaxed ) )
        atomic_thread_fence( memory_order_acquire );
    else if ( lazy_assigned.exchange( true, memory_order_relaxed ) ) {
        while ( ! lazy_ready.load( memory_order_relaxed ) );
        atomic_thread_fence( memory_order_acquire );
    }
    else {
        lazy_value = 42;
        lazy_ready.store( true, memory_order_release );
    }
    return lazy_value;
}

#endif


atomic_ulong volatile aulv = { 0 };
atomic_ulong auln = { 1 };
#ifdef __cplusplus
atomic< unsigned long > taul( 3 );
#endif

int integer_example( void )
{
    atomic_ulong a = { 3 };
    unsigned long x = atomic_load( &auln ) + atomic_load( &a );
    (void)atomic_store_explicit( &aulv, x, memory_order_release );
    unsigned long y = atomic_fetch_add_explicit( &aulv, 1,
                                                 memory_order_relaxed );
    unsigned long z = atomic_fetch_xor( &auln, 4 );
#ifdef __cplusplus
    // x = auln; // implicit conversion disallowed
    x = auln.load();
    aulv = x;
    auln += 1;
    aulv ^= 4;
    // auln = aulv; // uses a deleted operator
    aulv -= auln++;
    auln |= --aulv;
    aulv &= 7;
    atomic_store_explicit( &taul, 7, memory_order_release );
    x = taul.load( memory_order_acquire );
    y = atomic_fetch_add_explicit( & taul, 1, memory_order_acquire );
    z = atomic_fetch_xor( &taul, 4 );
    x = taul.load();
    // auln = taul; // uses a deleted operator
    // taul = aulv; // uses a deleted operator
    taul = x;
    taul += 1;
    taul ^= 4;
    taul -= taul++;
    taul |= --taul;
    taul &= 7;

    // Verify const operations
    const atomic_ulong b = { 4 };
    x = b.load();
    y = b;
#endif
    return y + z;
}


#ifdef __cplusplus

struct event_counter
{
    void inc() { au.fetch_add( 1, memory_order_relaxed ); }
    unsigned long get() { return au.load( memory_order_relaxed ); }
    atomic_ulong au;
};
event_counter ec = { { 0 } };

void generate_events()
{
    ec.inc();
    ec.inc();
    ec.inc();
}

int read_events()
{
    return ec.get();
}

int event_example()
{
    generate_events(); // possibly in multiple threads
    // join all other threads, ensuring that final value is written
    return read_events();
}

#endif


#ifdef __cplusplus

struct data;
struct node
{
    node* next;
    data* value;
};

atomic< node* > head( (node*)0 );

void list_example_simple( data* item )
{
    node* candidate = new node;
    candidate->value = item;
    candidate->next = head.load();
    while ( ! head.compare_exchange_weak( candidate->next, candidate ) );
}

void list_example_efficient( data* item )
{
    node* candidate = new node;
    candidate->value = item;
    candidate->next = head.load( memory_order_relaxed );
    while ( ! head.compare_exchange_weak( candidate->next, candidate,
                                 memory_order_release, memory_order_relaxed ) );
}

#endif


#if ATOMIC_LONG_LOCK_FREE <= 1
atomic_flag pseudo_mutex = ATOMIC_FLAG_INIT;
unsigned long regular_variable = 1;
#endif
#if ATOMIC_LONG_LOCK_FREE >= 1
atomic_ulong atomic_variable = { 1 };
#endif

void update()
{
#if ATOMIC_LONG_LOCK_FREE == 1
    if ( atomic_is_lock_free( &atomic_variable ) ) {
#endif
#if ATOMIC_LONG_LOCK_FREE > 0
        unsigned long full = atomic_load( atomic_variable );
        unsigned long half = full / 2;
        while ( ! atomic_compare_exchange_weak( &atomic_variable, &full, half ) )
            half = full / 2;
#endif
#if ATOMIC_LONG_LOCK_FREE == 1
    } else {
#endif
#if ATOMIC_LONG_LOCK_FREE < 2
        __atomic_flag_wait_explicit__( &pseudo_mutex, memory_order_seq_cst );
        regular_variable /= 2 ;
        atomic_flag_clear( &pseudo_mutex );
#endif
#if ATOMIC_LONG_LOCK_FREE == 1
    }
#endif
}


int main()
{
    return 0;
}
