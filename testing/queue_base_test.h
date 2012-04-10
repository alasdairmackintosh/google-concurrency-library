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

#include <iostream>

#include "functional.h"

#include "thread.h"

#include "queue_base.h"
#include "stream_mutex.h"

#include "gmock/gmock.h"
#include "cleanup_assert.h"

stream_mutex<std::ostream> mcout(std::cout);

using namespace gcl;

inline std::ostream& operator<<(
    std::ostream& stream,
    queue_op_status status )
{
    switch ( status ) {
        case CXX0X_ENUM_QUAL(queue_op_status)success:
            stream << "success";
            break;
        case CXX0X_ENUM_QUAL(queue_op_status)empty:
            stream << "empty";
            break;
        case CXX0X_ENUM_QUAL(queue_op_status)full:
            stream << "full";
            break;
        case CXX0X_ENUM_QUAL(queue_op_status)closed:
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
    queue_front<int>* f )
{
    ASSERT_TRUE(f->is_empty());
    for ( int i = 1; i <= count; ++i ) {
        f->push(i * multiplier);
        ASSERT_FALSE(f->is_empty());
    }
}

// Test the sequential draining of any empty queue.
void seq_drain(
    int count,
    int multiplier,
    queue_back<int>* b )
{
    try {
        for ( int i = 1; i <= count; ++i ) {
            ASSERT_FALSE(b->is_empty());
            ASSERT_EQ(i * multiplier, b->pop());
        }
        ASSERT_TRUE(b->is_empty());
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in seq_drain " << std::endl;
        FAIL();
    }
}

// Test the sequential "try" filling of any empty queue.
void seq_try_fill(
    int count,
    int multiplier,
    queue_front<int>* f )
{
    ASSERT_TRUE(f->is_empty());
    for ( int i = 1; i <= count; ++i ) {
        ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)success,
                  f->try_push(i * multiplier));
        ASSERT_FALSE(f->is_empty());
    }
}

// Test the sequential "try" draining of any empty queue.
void seq_try_drain(
    int count,
    int multiplier,
    queue_back<int>* b )
{
    for ( int i = 1; i <= count; ++i ) {
        int popped;
        ASSERT_FALSE(b->is_empty());
        ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)success, b->try_pop(popped));
        ASSERT_EQ(i * multiplier, popped);
    }
    ASSERT_TRUE(b->is_empty());
}

// Test the sequential try_pop on an empty queue.
// The front and back must refer to the same queue.
void seq_try_empty(
    queue_front<int>* f,
    queue_back<int>* b )
{
    int result;
    ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)empty, b->try_pop(result));
    ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)success, f->try_push(1));
    ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)success, b->try_pop(result));
    ASSERT_EQ(1, result);
    ASSERT_TRUE(b->is_empty());
}

// Test the sequential try_push on a full queue.
// The front and back must refer to the same queue.
void seq_try_full(
    int count,
    queue_front<int>* f,
    queue_back<int>* b )
{
    seq_try_fill(count, 1, f);
    ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)full, f->try_push(count + 1));
    ASSERT_EQ(1, b->pop());
    ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)success, f->try_push(count + 1));
    for ( int i = 2; i <= count + 1; ++i ) {
        int popped;
        ASSERT_FALSE(b->is_empty());
        ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)success, b->try_pop(popped));
        ASSERT_EQ(i, popped);
    }
    ASSERT_TRUE(b->is_empty());
}

// Test sequential operations on a closed queue.
// The front and back must refer to the same queue.
void seq_push_pop_closed(
    int count,
    queue_front<int>* f,
    queue_back<int>* b )
{
    seq_fill(count, 1, f);
    f->close();
    ASSERT_TRUE(f->is_closed());
    try {
        f->push(count);
        FAIL();
    } catch (queue_op_status expected) {
        ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)closed, expected);
    }
    seq_drain(count, 1, b);
    ASSERT_TRUE(b->is_closed());
    try {
        b->pop();
        FAIL();
    } catch (queue_op_status expected) {
        ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)closed, expected);
    }
}

// Test sequential try operations on a closed queue.
// The front and back must refer to the same queue.
void seq_try_push_pop_closed(
    int count,
    queue_front<int>* f,
    queue_back<int>* b )
{
    seq_try_fill(count, 1, f);
    f->close();
    ASSERT_TRUE(f->is_closed());
    ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)closed, f->try_push(42));
    seq_try_drain(count, 1, b);
    ASSERT_TRUE(b->is_closed());
    int popped;
    ASSERT_EQ(CXX0X_ENUM_QUAL(queue_op_status)closed, b->try_pop(popped));
}

// Test queue iteration, which also tests wait_push and wait_pop.
// Note that this function will wait forever unless the queue is closed.
void iterate(
    queue_back<int>::iterator bitr,
    queue_back<int>::iterator bend,
    queue_front<int>::iterator fitr,
    queue_front<int>::iterator fend,
    int (*compute)( int ) )
{
    while ( bitr != bend && fitr != fend )
        *fitr++ = compute(*bitr++);
}

// Test queue begin and end.
void filter(
    queue_back<int>* b,
    queue_front<int>* f,
    int (*compute)( int ) )
{
    try {
        iterate(b->begin(), b->end(), f->begin(), f->end(), compute);
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in filter" << std::endl;
        b->close();
        f->close();
        FAIL();
    } catch (...) {
        mcout << "unexpected exception in filter" << std::endl;
        b->close();
        f->close();
        FAIL();
    }
}

// Test the filling of a queue.
// Suitable for concurrency.
void fill(
    int count,
    int multiplier,
    queue_front<int>* f )
{
    try {
        for ( int i = 1; i <= count; ++i ) {
            f->push(i * multiplier);
        }
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in fill " << std::endl;
        f->close();
        FAIL();
    } catch (...) {
        mcout << "unexpected exception in fill " << std::endl;
        f->close();
        FAIL();
    }
}

// Test the draining of a queue.
// Suitable for linear concurrency, but not parallel concurrency.
void drain(
    int count,
    int multiplier,
    queue_back<int>* b )
{
    try {
        for ( int i = 1; i <= count; ++i ) {
            int popped = b->pop();
            CLEANUP_ASSERT_EQ(i * multiplier, popped, b->close(); );
        }
        bool empty = b->is_empty();
        CLEANUP_ASSERT_TRUE(empty, b->close(); );
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in drain " << std::endl;
        b->close();
        FAIL();
    } catch (...) {
        mcout << "unexpected exception in drain " << std::endl;
        b->close();
        FAIL();
    }
}

// Test the draining of a queue.
// Suitable for parallel concurrency,
// but with only one path for each half of the numbers.
void drain_pos_neg(
    int count,
    int multiplier,
    queue_back<int>* b )
{
    try {
        int last_neg = 0;
        int last_pos = 0;
        for ( int i = 1; i <= count; ++i ) {
            int popped = b->pop();
            if ( popped < 0 ) {
                CLEANUP_ASSERT_LT(popped, last_neg, b->close(); );
                last_neg = popped;
            }
            else {
                CLEANUP_ASSERT_GT(popped, last_pos, b->close(); );
                last_pos = popped;
            }
        }
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in drain_pos_neg " << std::endl;
        b->close();
        FAIL();
    } catch (...) {
        mcout << "unexpected exception in drain_pos_neg " << std::endl;
        b->close();
        FAIL();
    }
}

// Test the draining of a queue.
// Suitable for parallel concurrency.
void drain_any(
    int count,
    int multiplier,
    queue_back<int>* b )
{
    int factor = multiplier < 0 ? -multiplier : multiplier;
    try {
        for ( int i = 1; i <= count; ++i ) {
            int popped = b->pop();
            if ( popped < 0 )
                CLEANUP_ASSERT_GE(popped, count * -factor, b->close(); );
            else
                CLEANUP_ASSERT_LE(popped, count * factor, b->close(); );
        }
    } catch (queue_op_status unexpected) {
        mcout << "unexpected queue_op_status::" << unexpected
              << " in drain_pos_neg " << std::endl;
        b->close();
        FAIL();
    } catch (...) {
        mcout << "unexpected exception in drain_pos_neg " << std::endl;
        b->close();
        FAIL();
    }
}

// Test the "try" filling of a queue.  Suitable for concurrency.
// Warning, this uses busy waiting.  Use real threads.
void try_fill(
    int count,
    int multiplier,
    queue_front<int>* f )
{
    try {
        for ( int i = 1; i <= count; ) {
            queue_op_status status = f->try_push(i * multiplier);
            switch ( status ) {
                case CXX0X_ENUM_QUAL(queue_op_status)success:
                    ++i;
                    break;
                case CXX0X_ENUM_QUAL(queue_op_status)full:
                    break;
                default:
                    mcout << "unexpected queue_op_status::" << status
                          << " in try_fill " << std::endl;
                    f->close();
                    FAIL();
            }
        }
    } catch (...) {
        mcout << "unexpected exception at try_fill " << std::endl;
        f->close();
        FAIL();
    }
}

// Test the "try" draining of a queue.  Suitable for front/back concurrency.
// Warning, this uses busy waiting.  Use real threads.
void try_drain(
    int count,
    int multiplier,
    queue_back<int>* b )
{
    try {
        for ( int i = 1; i <= count; ) {
            int popped;
            queue_op_status status = b->try_pop(popped);
            switch ( status ) {
                case CXX0X_ENUM_QUAL(queue_op_status)success:
                    CLEANUP_ASSERT_EQ(i * multiplier, popped, b->close(); );
                    ++i;
                    break;
                case CXX0X_ENUM_QUAL(queue_op_status)empty:
                    break;
                default:
                    mcout << "unexpected queue_op_status::" << status
                          << " in try_drain " << std::endl;
                    b->close();
                    FAIL();
            }
        }
    } catch (...) {
        mcout << "unexpected exception in try_drain " << std::endl;
        b->close();
        FAIL();
    }
}

// Test the draining of a queue.
// Suitable for parallel concurrency,
// but with only one path for each half of the numbers.
void try_drain_pos_neg(
    int count,
    int multiplier,
    queue_back<int>* b )
{
    try {
        int last_neg = 0;
        int last_pos = 0;
        for ( int i = 1; i <= count; ) {
            int popped;
            queue_op_status status = b->try_pop(popped);
            switch ( status ) {
                case CXX0X_ENUM_QUAL(queue_op_status)success:
                    if ( popped < 0 ) {
                        CLEANUP_ASSERT_LT(popped, last_neg, b->close(); );
                        last_neg = popped;
                    }
                    else {
                        CLEANUP_ASSERT_GT(popped, last_pos, b->close(); );
                        last_pos = popped;
                    }
                    ++i;
                    break;
                case CXX0X_ENUM_QUAL(queue_op_status)empty:
                    break;
                default:
                    mcout << "unexpected queue_op_status::" << status
                          << "in try_drain_pos_neg " << std::endl;
                    b->close();
                    FAIL();
            }
        }
    } catch (...) {
        mcout << "unexpected exception in try_drain_pos_neg " << std::endl;
        b->close();
        FAIL();
    }
}

// Test the draining of a queue.
// Suitable for parallel concurrency.
void try_drain_any(
    int count,
    int multiplier,
    queue_back<int>* b )
{
    int factor = multiplier < 0 ? -multiplier : multiplier;
    try {
        for ( int i = 1; i <= count; ) {
            int popped;
            queue_op_status status = b->try_pop(popped);
            switch ( status ) {
                case CXX0X_ENUM_QUAL(queue_op_status)success:
                    if ( popped < 0 )
                        CLEANUP_ASSERT_GE(popped, count * -factor,
                                          b->close(); );
                    else
                        CLEANUP_ASSERT_LE(popped, count * factor,
                                          b->close(); );
                    ++i;
                    break;
                case CXX0X_ENUM_QUAL(queue_op_status)empty:
                    break;
                default:
                    mcout << "unexpected queue_op_status::" << status
                          << "in try_drain_pos_neg " << std::endl;
                    b->close();
                    FAIL();
            }
        }
    } catch (...) {
        mcout << "unexpected exception in try_drain_pos_neg " << std::endl;
        b->close();
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
    thread t1(std::bind(drain, count, 1, &queue));
    thread t2(std::bind(fill, count, 1, &queue));
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
    thread t1(std::bind(try_drain, count, 1, &queue));
    thread t2(std::bind(try_fill, count, 1, &queue));
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
    thread t1(std::bind(drain, count, 2, &tail));
    thread t2(std::bind(filter, &head, &tail, twice));
    thread t3(std::bind(fill, count, 1, &head));
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
    thread t1(std::bind(try_drain, count, 2, &tail));
    thread t2(std::bind(filter, &head, &tail, twice));
    thread t3(std::bind(try_fill, count, 1, &head));
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
    thread t1(std::bind(drain_pos_neg, 2 * count, 2, &tail));
    thread t2(std::bind(filter, &head, &tail, twice));
    thread t3(std::bind(fill, count, 1, &head));
    thread t4(std::bind(fill, count, -1, &head));
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
    thread t1(std::bind(try_drain_pos_neg, 2 * count, 2, &tail));
    thread t2(std::bind(filter, &head, &tail, twice));
    thread t3(std::bind(try_fill, count, 1, &head));
    thread t4(std::bind(try_fill, count, -1, &head));
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
    thread t1(std::bind(drain_any, count, -2, &tail));
    thread t2(std::bind(drain_any, count, 2, &tail));
    thread t3(std::bind(filter, &head, &tail, twice));
    thread t4(std::bind(filter, &head, &tail, twice));
    thread t5(std::bind(fill, count, 1, &head));
    thread t6(std::bind(fill, count, -1, &head));
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
    thread t1(std::bind(drain_any, count, -2, &tail));
    thread t2(std::bind(try_drain_any, count, 2, &tail));
    thread t3(std::bind(filter, &head, &tail, twice));
    thread t4(std::bind(filter, &head, &tail, twice));
    thread t5(std::bind(fill, count, 1, &head));
    thread t6(std::bind(try_fill, count, -1, &head));
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
