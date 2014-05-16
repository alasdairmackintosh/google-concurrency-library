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

// This is really a performance test of different concurrent queue
// implementations and functions. It's not a test in the sense of a unittest,
// though.

#include <iomanip>
#include <iostream>
#include <math.h>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sys/time.h>
#include <vector>

#include "atomic.h"
#include "barrier.h"
#include "buffer_queue.h"
#include "debug.h"
#include "functional.h"
#include "lock_free_buffer_queue.h"
#include "queue_base.h"
#include "thread.h"


using namespace std;
using gcl::buffer_queue;

namespace gcl {

double do_work(int size, double* arr) {
    for (int i = 0; i < size; ++i) {
        // Do a bunch of expensive fp math
        arr[i] = pow(3, log2(acos(sin(arr[i]))));
    }
    return arr[0];
}

void test_enq(function<queue_op_status(const unsigned int&)> enq_fn,
              barrier* b,
              atomic<unsigned long long>* total,
              size_t total_enq) {
    unsigned long long total_val = 0ULL;
    unsigned int i = 0;
    int num_failed = 0;
    // Only start the thread when all the threads doing the same operation have
    // started.

    // Create work.
    srandom(time(NULL));
    const int size = 100;
    double arr[size];
    for (int j = 0; j < size; ++j) {
        arr[j] = (double)random() / (double)((2 << 31) - 1);
    }

    b->arrive_and_wait();
    while (i < total_enq) {
        do_work(size, arr);
        // Will always retry and not count the enq until success
        if (enq_fn(i) == CXX11_ENUM_QUAL(queue_op_status)success) {
            ++i;
            total_val += i;
        } else {
            ++num_failed;
        }
    }
    (*total) += total_val;
}

void test_deq(function<queue_op_status(unsigned int&)> deq_fn,
              barrier* b,
              atomic<unsigned long long>* total,
              size_t total_deq) {
    unsigned long long total_val = 0ULL;
    unsigned int i = 0;
    double res = 1.45;
    int num_failed = 0;

    // Only start the thread when all the threads doing the same operation have
    // started.
    b->arrive_and_wait();
    while (i < total_deq) {
        // Will always retry and not count the enq until success
        unsigned int val;
        if (deq_fn(val) == CXX11_ENUM_QUAL(queue_op_status)success) {
            ++i;
            total_val += val;
            //res = acos(sin(res));
            //if (i % 1000 == 0) {
            //    chrono::milliseconds slp(1LL);
            //    this_thread::sleep_for(slp);
            //}
        } else {
            ++num_failed;
        }
    }
    (*total) += total_val + (int)res;
}


void test_harness(std::string test_name,
                  function<queue_op_status(const unsigned int&)> enq_fn,
                  function<queue_op_status(unsigned int&)> deq_fn,
                  size_t num_threads,
                  size_t ops_per_thread) {
    // Create start barriers for each thread type
    barrier enq_barrier(num_threads);
    atomic<unsigned long long> total_enq(0);

    barrier deq_barrier(num_threads);
    atomic<unsigned long long> total_deq(0);

    vector<thread*> enq_threads;
    vector<thread*> deq_threads;

    struct timeval start;
    struct timeval end;
    gettimeofday(&start, NULL);
    for (unsigned int i = 0; i < num_threads; ++i) {
        enq_threads.push_back(new thread(
            std::bind(test_enq, enq_fn, &enq_barrier, &total_enq,
                      ops_per_thread)));
        deq_threads.push_back(new thread(
            std::bind(test_deq, deq_fn, &deq_barrier, &total_deq,
                      ops_per_thread)));
    }
    for (vector<thread*>::iterator t = enq_threads.begin();
         t != enq_threads.end();
         ++t) {
        (*t)->join();
    }
    for (vector<thread*>::iterator t = deq_threads.begin();
         t != deq_threads.end();
         ++t) {
        (*t)->join();
    }
    gettimeofday(&end, NULL);
    unsigned long long diff_usec = (end.tv_sec - start.tv_sec) * 1000000;
    diff_usec += end.tv_usec - start.tv_usec;
    int real_total_ops = ops_per_thread * num_threads;
    double elapsed_secs = (double)diff_usec / 1000000.0;
    double time_per_op = elapsed_secs / real_total_ops;


    // Done Test! Report the time.
    DBG << "Test " << test_name << " done " << real_total_ops << " total ops "
        << std::setprecision(4) << elapsed_secs << " elapsed secs "
        << time_per_op << " time per op. "
        << " Totals " << total_enq.load() << " " << total_deq.load()
        << endl;
}

struct buffer_queue_wait_func {
    buffer_queue<unsigned int>* q;
    explicit buffer_queue_wait_func(buffer_queue<unsigned int>* newq)
      : q(newq) {}

    queue_op_status operator() (const unsigned int& value) {
        return q->wait_push(value);
    }
};

struct buffer_queue_try_func {
    buffer_queue<unsigned int>* q;
    explicit buffer_queue_try_func(buffer_queue<unsigned int>* newq)
      : q(newq) {}

    queue_op_status operator() (const unsigned int& value) {
        return q->try_push(value);
    }
};

struct buffer_queue_nonblock_func {
    buffer_queue<unsigned int>* q;
    explicit buffer_queue_nonblock_func(buffer_queue<unsigned int>* newq)
      : q(newq) {}

    queue_op_status operator() (const unsigned int& value) {
        return q->nonblocking_push(value);
    }
};

struct lock_free_buffer_queue_nonblock_func {
    lock_free_buffer_queue<unsigned int>* q;
    explicit lock_free_buffer_queue_nonblock_func(
        lock_free_buffer_queue<unsigned int>* newq) : q(newq) {}

    queue_op_status operator() (const unsigned int& value) {
        return q->nonblocking_push(value);
    }
};

}  // namespace gcl

int main(int argc, char** argv) {
    const size_t MAX_THREADS = 16;
    const size_t TOTAL_OPS = 1000000;
    const size_t QUEUE_SIZE = 1000;

    buffer_queue<unsigned int> q(QUEUE_SIZE);
    gcl::buffer_queue_wait_func f(&q);
    for (size_t n_threads = 1; n_threads <= MAX_THREADS;
         n_threads = (n_threads + 2) & 0xfffe) {
        size_t ops_per_thread = TOTAL_OPS / n_threads;
        stringstream ss;
        ss << "buffer_queue wait_" << n_threads;
        gcl::test_harness(ss.str(),
                          f,
                          std::bind(&buffer_queue<unsigned int>::wait_pop,
                                    &q, placeholders::_1),
                          n_threads, ops_per_thread);
    }
    cout << endl;

    gcl::buffer_queue_try_func tf(&q);
    for (size_t n_threads = 1; n_threads <= MAX_THREADS;
         n_threads = (n_threads + 2) & 0xfffe) {
        size_t ops_per_thread = TOTAL_OPS / n_threads;
        stringstream ss;
        ss << "buffer_queue try_" << n_threads;
        gcl::test_harness(ss.str(),
                          tf,
                          std::bind(&buffer_queue<unsigned int>::try_pop,
                                    &q, placeholders::_1),
                          n_threads, ops_per_thread);
    }
    cout << endl;

    gcl::buffer_queue_nonblock_func nf(&q);
    for (size_t n_threads = 1; n_threads <= MAX_THREADS;
         n_threads = (n_threads + 2) & 0xfffe) {
        size_t ops_per_thread = TOTAL_OPS / n_threads;
        stringstream ss;
        ss << "buffer_queue nonblocking_" << n_threads;
        gcl::test_harness(
            ss.str(),
            nf,
            std::bind(&buffer_queue<unsigned int>::nonblocking_pop,
                      &q, placeholders::_1),
            n_threads,
            ops_per_thread);
    }
    cout << endl;

    gcl::lock_free_buffer_queue<unsigned int> q2(QUEUE_SIZE);
    gcl::lock_free_buffer_queue_nonblock_func nf2(&q2);
    for (size_t n_threads = 1; n_threads <= MAX_THREADS;
         n_threads = (n_threads + 2) & 0xfffe) {
        size_t ops_per_thread = TOTAL_OPS / n_threads;
        stringstream ss;
        ss << "lock_free_buffer_queue nonblocking_" << n_threads;
        gcl::test_harness(
            ss.str(),
            nf2,
            std::bind(&gcl::lock_free_buffer_queue<unsigned int>
                          ::nonblocking_pop,
                      &q2, placeholders::_1),
            n_threads,
            ops_per_thread);
    }

    return 0;
}
