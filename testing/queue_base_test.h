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

#ifndef QUEUE_BASE_TEST_H
#define QUEUE_BASE_TEST_H

#include <tr1/functional>
#include <iostream>
#include "stream_mutex.h"
#include "thread.h"
#include "gmock/gmock.h"
#include "cleanup_assert.h"
#include "queue_base.h"

namespace tr1 = std::tr1;

using namespace gcl;

inline std::ostream& operator<<(
    std::ostream& stream,
    queue_op_status status )
{
    switch ( status ) {
        case CXX11_ENUM_QUAL(queue_op_status)success:
            stream << "success";
            break;
        case CXX11_ENUM_QUAL(queue_op_status)empty:
            stream << "empty";
            break;
        case CXX11_ENUM_QUAL(queue_op_status)full:
            stream << "full";
            break;
        case CXX11_ENUM_QUAL(queue_op_status)closed:
            stream << "closed";
            break;
        default:
            stream << "FAILURE";
    }
    return stream;
}

// Test the sequential filling of any empty queue.
void seq_fill(
    int count,
    int multiplier,
    queue_back<int> bk )
{
    ASSERT_TRUE(bk.is_empty());
    for ( int i = 1; i <= count; ++i ) {
        bk.push(i * multiplier);
        ASSERT_FALSE(bk.is_empty());
    }
}

// Test the sequential draining of any empty queue.
void seq_drain(
    int count,
    int multiplier,
    queue_front<int> ft )
{
    try {
        for ( int i = 1; i <= count; ++i ) {
            ASSERT_FALSE(ft.is_empty());
            ASSERT_EQ(i * multiplier, ft.value_pop());
        }
        ASSERT_TRUE(ft.is_empty());
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in seq_drain " << std::endl;
        FAIL();
    }
}

// Test the sequential "try" filling of any empty queue.
template <typename Queue>
void seq_try_fill(
    int count,
    int multiplier,
    Queue* f )
{
    ASSERT_TRUE(f->is_empty());
    for ( int i = 1; i <= count; ++i ) {
        ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success,
                  f->try_push(i * multiplier));
        ASSERT_FALSE(f->is_empty());
    }
}

// Test the sequential "try" filling of any empty queue.
void seq_try_fill(
    int count,
    int multiplier,
    queue_back<int> bk )
{
    ASSERT_TRUE(bk.is_empty());
    for ( int i = 1; i <= count; ++i ) {
        ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success,
                  bk.try_push(i * multiplier));
        ASSERT_FALSE(bk.is_empty());
    }
}

// Test the sequential "try" draining of any empty queue.
template <typename Queue>
void seq_try_drain(
    int count,
    int multiplier,
    Queue* b )
{
    for ( int i = 1; i <= count; ++i ) {
        int popped;
        ASSERT_FALSE(b->is_empty());
        ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success, b->try_pop(popped));
        ASSERT_EQ(i * multiplier, popped);
    }
    ASSERT_TRUE(b->is_empty());
}

// Test the sequential "try" draining of any empty queue.
void seq_try_drain(
    int count,
    int multiplier,
    queue_front<int> ft )
{
    for ( int i = 1; i <= count; ++i ) {
        int popped;
        ASSERT_FALSE(ft.is_empty());
        ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success, ft.try_pop(popped));
        ASSERT_EQ(i * multiplier, popped);
    }
    ASSERT_TRUE(ft.is_empty());
}

// Test the sequential try_pop on an empty queue.
template <typename Queue>
void seq_try_empty( Queue* q )
{
    int result;
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)empty, q->try_pop(result));
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success, q->try_push(1));
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success, q->try_pop(result));
    ASSERT_EQ(1, result);
    ASSERT_TRUE(q->is_empty());
}

// Test the sequential try_pop on an empty queue.
// The back and front must refer to the same queue.
void seq_try_empty(
    queue_back<int> bk,
    queue_front<int> ft )
{
    int result;
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)empty, ft.try_pop(result));
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success, bk.try_push(1));
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success, ft.try_pop(result));
    ASSERT_EQ(1, result);
    ASSERT_TRUE(ft.is_empty());
}

// Test the sequential try_push on a full queue.
template <typename Queue>
void seq_try_full(
    int count,
    Queue* q )
{
    seq_try_fill(count, 1, q);
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)full, q->try_push(count + 1));
    // This assert might fail if try_pop doesn't return success.
    int val;
    while (q->try_pop(val) != CXX11_ENUM_QUAL(queue_op_status)success) {}
    ASSERT_EQ(1, val);
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success, q->try_push(count + 1));
    for ( int i = 2; i <= count + 1; ++i ) {
        int popped;
        ASSERT_FALSE(q->is_empty());
        ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success, q->try_pop(popped));
        ASSERT_EQ(i, popped);
    }
    ASSERT_TRUE(q->is_empty());
}

// Test the sequential try_push on a full queue.
void seq_try_full(
    int count,
    queue_back<int> bk,
    queue_front<int> ft)
{
    seq_try_fill(count, 1, bk);
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)full, bk.try_push(count + 1));
    ASSERT_EQ(1, ft.value_pop());
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success, bk.try_push(count + 1));
    for ( int i = 2; i <= count + 1; ++i ) {
        int popped;
        ASSERT_FALSE(ft.is_empty());
        ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)success, ft.try_pop(popped));
        ASSERT_EQ(i, popped);
    }
    ASSERT_TRUE(ft.is_empty());
}

// Test sequential operations on a closed queue.
// The back and front must refer to the same queue.
void seq_push_pop_closed(
    int count,
    queue_back<int> bk,
    queue_front<int> ft )
{
    seq_fill(count, 1, bk);
    bk.close();
    ASSERT_TRUE(bk.is_closed());
    try {
        bk.push(count);
        FAIL();
    } catch (queue_op_status expected) {
        ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)closed, expected);
    }
    seq_drain(count, 1, ft);
    ASSERT_TRUE(ft.is_closed());
    try {
        ft.value_pop();
        FAIL();
    } catch (queue_op_status expected) {
        ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)closed, expected);
    }
}

// Test sequential try operations on a closed queue.
// The back and front must refer to the same queue.
void seq_try_push_pop_closed(
    int count,
    queue_back<int> bk,
    queue_front<int> ft )
{
    seq_try_fill(count, 1, bk);
    bk.close();
    ASSERT_TRUE(bk.is_closed());
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)closed, bk.try_push(42));
    seq_try_drain(count, 1, ft);
    ASSERT_TRUE(ft.is_closed());
    int popped;
    queue_op_status result = ft.try_pop(popped);
    ASSERT_EQ(CXX11_ENUM_QUAL(queue_op_status)closed, result);
}

// Test queue iteration, which also tests wait_push and wait_pop.
// Note that this function will wait forever unless the queue is closed.
void iterate(
    queue_front<int>::iterator ft_itr,
    queue_front<int>::iterator ft_end,
    queue_back<int>::iterator bk_itr,
    queue_back<int>::iterator bk_end,
    int (*compute)( int ) )
{
    while ( ft_itr != ft_end && bk_itr != bk_end )
        *bk_itr++ = compute(*ft_itr++);
}

// Test queue begin and end.
void filter(
    queue_front<int> ft,
    queue_back<int> bk,
    int (*compute)( int ) )
{
    try {
        iterate(ft.begin(), ft.end(), bk.begin(), bk.end(), compute);
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in filter" << std::endl;
        ft.close();
        bk.close();
        FAIL();
    } catch (...) {
        mcout << "unexpected exception in filter" << std::endl;
        ft.close();
        bk.close();
        FAIL();
    }
}

// Test the filling of a queue.
// Suitable for concurrency.
void fill(
    int count,
    int multiplier,
    queue_back<int> bk )
{
    try {
        for ( int i = 1; i <= count; ++i ) {
            bk.push(i * multiplier);
        }
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in fill " << std::endl;
        bk.close();
        FAIL();
    } catch (...) {
        mcout << "unexpected exception in fill " << std::endl;
        bk.close();
        FAIL();
    }
}

// Test the draining of a queue.
// Suitable for linear concurrency, but not parallel concurrency.
void drain(
    int count,
    int multiplier,
    queue_front<int> ft )
{
    try {
        for ( int i = 1; i <= count; ++i ) {
            int popped = ft.value_pop();
            CLEANUP_ASSERT_EQ(i * multiplier, popped, ft.close(); );
        }
        bool empty = ft.is_empty();
        CLEANUP_ASSERT_TRUE(empty, ft.close(); );
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in drain " << std::endl;
        ft.close();
        FAIL();
    } catch (...) {
        mcout << "unexpected exception in drain " << std::endl;
        ft.close();
        FAIL();
    }
}

// Test the draining of a queue.
// Suitable for parallel concurrency,
// but with only one path for each half of the numbers.
void drain_pos_neg(
    int count,
    int multiplier,
    queue_front<int> ft )
{
    try {
        int last_neg = 0;
        int last_pos = 0;
        for ( int i = 1; i <= count; ++i ) {
            int popped = ft.value_pop();
            if ( popped < 0 ) {
                CLEANUP_ASSERT_LT(popped, last_neg, ft.close(); );
                last_neg = popped;
            }
            else {
                CLEANUP_ASSERT_GT(popped, last_pos, ft.close(); );
                last_pos = popped;
            }
        }
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in drain_pos_neg " << std::endl;
        ft.close();
        FAIL();
    } catch (...) {
        mcout << "unexpected exception in drain_pos_neg " << std::endl;
        ft.close();
        FAIL();
    }
}

// Test the draining of a queue.
// Suitable for parallel concurrency.
void drain_any(
    int count,
    int multiplier,
    queue_front<int> ft )
{
    int factor = multiplier < 0 ? -multiplier : multiplier;
    try {
        for ( int i = 1; i <= count; ++i ) {
            int popped = ft.value_pop();
            if ( popped < 0 )
                CLEANUP_ASSERT_GE(popped, count * -factor, ft.close(); );
            else
                CLEANUP_ASSERT_LE(popped, count * factor, ft.close(); );
        }
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in drain_pos_neg " << std::endl;
        ft.close();
        FAIL();
    } catch (...) {
        mcout << "unexpected exception in drain_pos_neg " << std::endl;
        ft.close();
        FAIL();
    }
}

// Test the "try" filling of a queue.  Suitable for concurrency.
// Warning, this uses busy waiting.  Use real threads.
void try_fill(
    int count,
    int multiplier,
    queue_back<int> bk )
{
    try {
        for ( int i = 1; i <= count; ) {
            queue_op_status status = bk.try_push(i * multiplier);
            switch ( status ) {
                case CXX11_ENUM_QUAL(queue_op_status)success:
                    ++i;
                    break;
                case CXX11_ENUM_QUAL(queue_op_status)full:
                    break;
                default:
                    mcout << "unexpected queue_op_status::" << status
                          << " in try_fill " << std::endl;
                    bk.close();
                    FAIL();
            }
        }
    } catch (...) {
        mcout << "unexpected exception at try_fill " << std::endl;
        bk.close();
        FAIL();
    }
}

// Test the "try" draining of a queue.  Suitable for back/front concurrency.
// Warning, this uses busy waiting.  Use real threads.
void try_drain(
    int count,
    int multiplier,
    queue_front<int> ft )
{
    try {
        for ( int i = 1; i <= count; ) {
            int popped;
            queue_op_status status = ft.try_pop(popped);
            switch ( status ) {
                case CXX11_ENUM_QUAL(queue_op_status)success:
                    CLEANUP_ASSERT_EQ(i * multiplier, popped, ft.close(); );
                    ++i;
                    break;
                case CXX11_ENUM_QUAL(queue_op_status)empty:
                    break;
                default:
                    mcout << "unexpected queue_op_status::" << status
                          << " in try_drain " << std::endl;
                    ft.close();
                    FAIL();
            }
        }
    } catch (...) {
        mcout << "unexpected exception in try_drain " << std::endl;
        ft.close();
        FAIL();
    }
}

// Test the draining of a queue.
// Suitable for parallel concurrency,
// but with only one path for each half of the numbers.
void try_drain_pos_neg(
    int count,
    int multiplier,
    queue_front<int> ft )
{
    try {
        int last_neg = 0;
        int last_pos = 0;
        for ( int i = 1; i <= count; ) {
            int popped;
            queue_op_status status = ft.try_pop(popped);
            switch ( status ) {
                case CXX11_ENUM_QUAL(queue_op_status)success:
                    if ( popped < 0 ) {
                        CLEANUP_ASSERT_LT(popped, last_neg, ft.close(); );
                        last_neg = popped;
                    }
                    else {
                        CLEANUP_ASSERT_GT(popped, last_pos, ft.close(); );
                        last_pos = popped;
                    }
                    ++i;
                    break;
                case CXX11_ENUM_QUAL(queue_op_status)empty:
                    break;
                default:
                    mcout << "unexpected queue_op_status::" << status
                          << "in try_drain_pos_neg " << std::endl;
                    ft.close();
                    FAIL();
            }
        }
    } catch (...) {
        mcout << "unexpected exception in try_drain_pos_neg " << std::endl;
        ft.close();
        FAIL();
    }
}

// Test the draining of a queue.
// Suitable for parallel concurrency.
void try_drain_any(
    int count,
    int multiplier,
    queue_front<int> ft )
{
    int factor = multiplier < 0 ? -multiplier : multiplier;
    try {
        for ( int i = 1; i <= count; ) {
            int popped;
            queue_op_status status = ft.try_pop(popped);
            switch ( status ) {
                case CXX11_ENUM_QUAL(queue_op_status)success:
                    if ( popped < 0 )
                        CLEANUP_ASSERT_GE(popped, count * -factor,
                                          ft.close(); );
                    else
                        CLEANUP_ASSERT_LE(popped, count * factor,
                                          ft.close(); );
                    ++i;
                    break;
                case CXX11_ENUM_QUAL(queue_op_status)empty:
                    break;
                default:
                    mcout << "unexpected queue_op_status::" << status
                          << "in try_drain_pos_neg " << std::endl;
                    ft.close();
                    FAIL();
            }
        }
    } catch (...) {
        mcout << "unexpected exception in try_drain_pos_neg " << std::endl;
        ft.close();
        FAIL();
    }
}

// Test producer and consumer.
void seq_producer_consumer(
    int count,
    queue_base<int>& queue )
{
    seq_fill(count, 1, &queue);
    queue.close();
    seq_drain(count, 1, &queue);
    ASSERT_TRUE(queue.is_empty());
}

// Test producer and consumer.
void producer_consumer(
    int count,
    queue_base<int>& queue )
{
    // Start drain first for extra testing of waiting
    thread t1(tr1::bind(drain, count, 1, &queue));
    thread t2(tr1::bind(fill, count, 1, &queue));
    // Join in order of expected completion.
    // Closing as we go to stop the other thread.
    t2.join();
    queue.close();
    t1.join();
    ASSERT_TRUE(queue.is_empty());
}

// Test try producer and consumer.
void try_producer_consumer(
    int count,
    queue_base<int>& queue )
{
    // Start drain first for extra testing of waiting
    thread t1(tr1::bind(try_drain, count, 1, &queue));
    thread t2(tr1::bind(try_fill, count, 1, &queue));
    // Join in order of expected completion.
    // Closing as we go to stop the other thread.
    t2.join();
    queue.close();
    t1.join();
    ASSERT_TRUE(queue.is_empty());
}

// Helper for testing queue filtering.
inline int twice( int arg )
{
    return 2 * arg;
}

// Test a sequential pipeline
void seq_pipe(
    int count,
    queue_base<int>& head,
    queue_base<int>& tail )
{
    seq_fill(count, 1, &head);
    head.close();
    filter(&head, &tail, twice);
    ASSERT_TRUE(head.is_empty());
    tail.close();
    seq_drain(count, 2, &tail);
    ASSERT_TRUE(tail.is_empty());
}

// Test a linear pipeline
void linear_pipe(
    int count,
    queue_base<int>& head,
    queue_base<int>& tail )
{
    // Start drain first for extra testing of waiting
    thread t1(tr1::bind(drain, count, 2, &tail));
    thread t2(tr1::bind(filter, &head, &tail, twice));
    thread t3(tr1::bind(fill, count, 1, &head));
    // Join in order of expected completion.
    // Closing as we go to stop the other threads.
    t3.join();
    head.close();
    t2.join();
    ASSERT_TRUE(head.is_empty());
    tail.close();
    t1.join();
    ASSERT_TRUE(tail.is_empty());
}

// Test a linear try pipeline
void linear_try_pipe(
    int count,
    queue_base<int>& head,
    queue_base<int>& tail )
{
    // Start drain first for extra testing of waiting
    thread t1(tr1::bind(try_drain, count, 2, &tail));
    thread t2(tr1::bind(filter, &head, &tail, twice));
    thread t3(tr1::bind(try_fill, count, 1, &head));
    // Join in order of expected completion.
    // Closing as we go to stop the other threads.
    t3.join();
    head.close();
    t2.join();
    ASSERT_TRUE(head.is_empty());
    tail.close();
    t1.join();
    ASSERT_TRUE(tail.is_empty());
}

// Test merging pipelines
void merging_pipe(
    int count,
    queue_base<int>& head,
    queue_base<int>& tail )
{
    // Start drain first for extra testing of waiting
    thread t1(tr1::bind(drain_pos_neg, 2 * count, 2, &tail));
    thread t2(tr1::bind(filter, &head, &tail, twice));
    thread t3(tr1::bind(fill, count, 1, &head));
    thread t4(tr1::bind(fill, count, -1, &head));
    // Join in order of expected completion.
    // Closing as we go to stop the other threads.
    t4.join();
    t3.join();
    head.close();
    t2.join();
    ASSERT_TRUE(head.is_empty());
    tail.close();
    t1.join();
    ASSERT_TRUE(tail.is_empty());
}

// Verify merging filtering try pipes
void merging_try_pipe(
    int count,
    queue_base<int>& head,
    queue_base<int>& tail )
{
    // Start drain first for extra testing of waiting
    thread t1(tr1::bind(try_drain_pos_neg, 2 * count, 2, &tail));
    thread t2(tr1::bind(filter, &head, &tail, twice));
    thread t3(tr1::bind(try_fill, count, 1, &head));
    thread t4(tr1::bind(try_fill, count, -1, &head));
    // Join in order of expected completion.
    // Closing as we go to stop the other threads.
    t4.join();
    t3.join();
    head.close();
    t2.join();
    ASSERT_TRUE(head.is_empty());
    tail.close();
    t1.join();
    ASSERT_TRUE(tail.is_empty());
}

// Test a parallel pipeline
void parallel_pipe(
    int count,
    queue_base<int>& head,
    queue_base<int>& tail )
{
    // Start drain first for extra testing of waiting
    thread t1(tr1::bind(drain_any, count, -2, &tail));
    thread t2(tr1::bind(drain_any, count, 2, &tail));
    thread t3(tr1::bind(filter, &head, &tail, twice));
    thread t4(tr1::bind(filter, &head, &tail, twice));
    thread t5(tr1::bind(fill, count, 1, &head));
    thread t6(tr1::bind(fill, count, -1, &head));
    // Join in order of expected completion.
    // Closing as we go to stop the other threads.
    t6.join();
    t5.join();
    head.close();
    t4.join();
    t3.join();
    ASSERT_TRUE(head.is_empty());
    tail.close();
    t2.join();
    t1.join();
    ASSERT_TRUE(tail.is_empty());
}

// Test a parallel mixed pipeline
void parallel_mixed_pipe(
    int count,
    queue_base<int>& head,
    queue_base<int>& tail )
{
    // Start drain first for extra testing of waiting
    thread t1(tr1::bind(drain_any, count, -2, &tail));
    thread t2(tr1::bind(try_drain_any, count, 2, &tail));
    thread t3(tr1::bind(filter, &head, &tail, twice));
    thread t4(tr1::bind(filter, &head, &tail, twice));
    thread t5(tr1::bind(fill, count, 1, &head));
    thread t6(tr1::bind(try_fill, count, -1, &head));
    // Join in order of expected completion.
    // Closing as we go to stop the other threads.
    t6.join();
    t5.join();
    head.close();
    t4.join();
    t3.join();
    ASSERT_TRUE(head.is_empty());
    tail.close();
    t2.join();
    t1.join();
    ASSERT_TRUE(tail.is_empty());
}

#endif
