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

// This file is a rudimentary test of the concurrent_priority_queue class

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "concurrent_priority_queue.h"
#include "test_mutex.h"

#include "gmock/gmock.h"

using namespace std;
using testing::_;
using testing::Invoke;
using testing::InSequence;

using gcl::concurrent_priority_queue;

// Iterator used with MyContainer
// TODO(alasdair): Verify if we need this. Keeping it for now until
// all functionality and multithreaded tests are complete.
class MyIterator : public iterator<random_access_iterator_tag, string> {
 public:
  MyIterator() : ptr_(NULL) {}
  explicit MyIterator(string* x) : ptr_(x) {}
  MyIterator(const MyIterator& other) : ptr_(other.ptr_) {}

  MyIterator& operator++() {
    ++ptr_;
    return *this;
  }

  MyIterator& operator++(int) {
    ptr_++;
    return *this;
  }

  MyIterator& operator--() {
    --ptr_;
    return *this;
  }

  MyIterator& operator--(int) {
    ptr_--;
    return *this;
  }

  MyIterator& operator+=(int inc) {
    ptr_ += inc;
    return *this;
  }

  MyIterator& operator-=(int dec) {
    ptr_ -= dec;
    return *this;
  }

  bool operator==(const MyIterator& rhs) const {
    return ptr_ == rhs.ptr_;
  }

  bool operator<(const MyIterator& rhs) const {
    return ptr_ < rhs.ptr_;
  }

  bool operator>(const MyIterator& rhs) const {
    return ptr_ > rhs.ptr_;
  }

  bool operator!=(const MyIterator& rhs) const {
    return ptr_ !=rhs.ptr_;
  }

  MyIterator operator+(int val) {
    return MyIterator(ptr_ + val);
  }

  MyIterator operator-(int val) {
    return MyIterator(ptr_ - val);
  }

  int operator-(const MyIterator& rhs) const {
    return ptr_ - rhs.ptr_;
  }
  string& operator*() {return *ptr_;}
 private:
  string* ptr_;
};

// Container type used with a concurrent_priority_queue
class MyContainer {
 public:
  MyContainer() : size_(0) {
  }

  MyIterator begin() {
    MyIterator it(&elems_[0]);
    return it;
  }

  MyIterator end() {
    MyIterator it(&elems_[size_]);
    return it;
  }

  void push_back(string elem) {
    elems_[size_++] = elem;
  }

  void pop_back() {
    size_--;
  }

  const string& back() const {
    return elems_[size_ - 1];
  }

  MyIterator erase(MyIterator pos) {
    int position = pos - begin();
    for (int i = position; i < size_; i++) {
      elems_[i] = elems_[i+1];
    }
    size_--;
    return MyIterator(&elems_[position]);
  }

  const string& operator[](int i) const {
    return elems_[i];
  }

  int size() const {
    return size_;
  }

  bool empty() const {
    return size_ == 0;
  }

 private:
  string elems_[100];
  int size_;
};

// Switchable comparison function
class MyCompare : public binary_function <string, string, bool> {
 public:
  MyCompare() : reverse_(false) {
  }

  explicit MyCompare(bool reverse) : reverse_(reverse) {
  }

  bool operator() (const string& x, const string& y) const {
    bool result = reverse_ ? x > y : x < y;
    return result;
  }

 private:
  bool reverse_;
};

class PriorityQueueTest : public testing::Test {
 protected:
  vector<string> create_values() {
    vector<string> values;
    values.push_back("C");
    values.push_back("A");
    values.push_back("D");
    values.push_back("B");
    return values;
  }

  // Verifies that the given queue contains the same elements as the
  // expected container. This method pops values off the queue, and
  // expects that the order of elements in the container matches the
  // queue. (The last element in the container will be the first one
  // popped.)
  template <typename Queue, typename Container>
  void validate_queue(Queue& queue, const Container& expected) {
    size_t expected_size = expected.size();
    ASSERT_EQ(expected_size, queue.size());
    for (size_t i = expected.size(); i > 0; i--) {
      string value;
      ASSERT_TRUE(queue.try_pop(value));
      EXPECT_EQ(expected[i - 1], value);
    }
  }
};

// Verify that a concurrent_priority_queue can be created from a
// vector of values.
TEST_F(PriorityQueueTest, Simple) {
  vector<string> values = create_values();
  concurrent_priority_queue<string> queue(values);
  sort(values.begin(), values.end());
  validate_queue(queue, values);
}

// Verify that a concurrent_priority_queue can be created from a
// vector of values and a custom comparator.
TEST_F(PriorityQueueTest, CustomComparator) {
  MyCompare cmp(true);
  vector<string> values = create_values();
  concurrent_priority_queue<string, vector<string>, MyCompare> queue(
      cmp, values);
  sort(values.begin(), values.end());
  reverse(values.begin(), values.end());
  validate_queue(queue, values);
}

// Verify the a concurrent_priority_queue can be created from another
// queue.
TEST_F(PriorityQueueTest, CopyConstructor) {
  vector<string> values = create_values();
  concurrent_priority_queue<string> queue(values);
  concurrent_priority_queue<string> new_queue(queue);
  // Pop an element off the original queue. Should not affect the new
  // queue
  string dummy;
  ASSERT_TRUE(queue.try_pop(dummy));
  sort(values.begin(), values.end());
  validate_queue(new_queue, values);
}

// Verify that a concurrent_priority_queue can be assigned
// the value of another queue.
TEST_F(PriorityQueueTest, AssignmentOperator) {
  vector<string> values = create_values();
  concurrent_priority_queue<string> queue(values);
  concurrent_priority_queue<string> new_queue;
  ASSERT_EQ(0U, new_queue.size());
  new_queue = queue;
  // Pop an element off the original queue. Should not affect the new
  // queue
  string dummy;
  ASSERT_TRUE(queue.try_pop(dummy));
  sort(values.begin(), values.end());
  validate_queue(new_queue, values);
}

// Verify the contents of two queues can be swapped.
TEST_F(PriorityQueueTest, Swap) {
  MyCompare first_cmp(true);
  MyCompare second_cmp(false);
  vector<string> values = create_values();
  concurrent_priority_queue<string, vector<string>, MyCompare> first_queue(
      first_cmp, values);
  concurrent_priority_queue<string, vector<string>, MyCompare> second_queue(
      second_cmp, values);

  // Make copies of the two queues. Use copy ctor and = operator (so that
  // both code paths get additional testing.
  concurrent_priority_queue<string, vector<string>, MyCompare>
      first_queue_copy(first_queue);
  concurrent_priority_queue<string, vector<string>, MyCompare>
      second_queue_copy;
  second_queue_copy = second_queue;

  first_queue.swap(second_queue);

  string new_first_elem = first_queue.pop();
  string new_second_elem = second_queue.pop();
  ASSERT_NE(new_first_elem, new_second_elem);
  ASSERT_EQ(new_first_elem, second_queue_copy.pop());
  ASSERT_EQ(new_second_elem, first_queue_copy.pop());
}

// Verify the a concurrent_priority_queue can be created from an
// iterator.
TEST_F(PriorityQueueTest, CreateFromIterator) {
  vector<string> values = create_values();
  concurrent_priority_queue<string> queue(values.begin(), values.end());
  sort(values.begin(), values.end());
  validate_queue(queue, values);
}

// Verify that a concurrent_priority_queue can be created from an
// iterator, a custom comparator and a container.
TEST_F(PriorityQueueTest, IteratorAndCustomComparator) {
  MyCompare cmp(true);
  vector<string> base;
  base.push_back("ZZZ");
  vector<string> values = create_values();

  concurrent_priority_queue<string, vector<string>, MyCompare> queue(
      values.begin(), values.end(), cmp, base);
  base.insert(base.end(), values.begin(), values.end());
  sort(base.begin(), base.end());
  reverse(base.begin(), base.end());
  validate_queue(queue, base);
}

// Verify the a concurrent_priority_queue can be created from a
// non-vector queue.
TEST_F(PriorityQueueTest, CreateFromCustomQueue) {
  MyContainer my_values;
  my_values.push_back("A");
  my_values.push_back("X");
  my_values.push_back("B");
  concurrent_priority_queue<string, MyContainer, less<string> >
      myqueue(less<string>(), my_values);
  sort(my_values.begin(), my_values.end());
  validate_queue(myqueue, my_values);
}

// Verify the a concurrent_priority_queue can be created from a
// non-vector queue and a custom comparison function.
TEST_F(PriorityQueueTest, CreateFromCustomQueueAndComparator) {
  MyContainer my_values;
  my_values.push_back("A");
  my_values.push_back("X");
  my_values.push_back("B");
  MyCompare cmp(true);
  concurrent_priority_queue<string, MyContainer, MyCompare> myqueue(
      cmp, my_values);

  sort(my_values.begin(), my_values.end());
  reverse(my_values.begin(), my_values.end());
  validate_queue(myqueue, my_values);
}

// Verify the a concurrent_priority_queue can be created from another
// queue that uses a custom comparator.
TEST_F(PriorityQueueTest, CopyConstructorWithComparator) {
  vector<string> values = create_values();
  MyCompare cmp(true);
  concurrent_priority_queue<string, vector<string>, MyCompare> myqueue(
      cmp, values);
  concurrent_priority_queue<string, vector<string>, MyCompare> new_queue(
      myqueue);
  // Pop an element off the original queue. Should not affect the new queue
  string dummy;
  ASSERT_TRUE(myqueue.try_pop(dummy));

  // Add an element to the new queue. The reverse comparator should
  // put it at the beginning.
  new_queue.push("ZZZ");
  sort(values.begin(), values.end());
  reverse(values.begin(), values.end());
  values.insert(values.begin(), "ZZZ");
  validate_queue(new_queue, values);
}

// Verify the a concurrent_priority_queue can be re-ordered on update()
// uses a custom comparator.
TEST_F(PriorityQueueTest, Update) {
  vector<string> input;
  input.push_back("C");
  input.push_back("B");
  input.push_back("A");
  input.push_back("Z");
  MyCompare cmp(false);
  concurrent_priority_queue<string, vector<string>, MyCompare> myqueue(
      cmp, input);
  string head;
  ASSERT_TRUE(myqueue.try_pop(head));
  ASSERT_EQ(head, "Z");

  // update the queue with a reversed comparator. Order should change.
  myqueue.update(MyCompare(true));
  ASSERT_TRUE(myqueue.try_pop(head));
  ASSERT_EQ(head, "A");
}


void PopElement(concurrent_priority_queue<string>& queue,
                const vector<string>& expected, size_t* num_popped) {
  for (size_t  i = 0; i < expected.size(); i++) {
    string popped = queue.pop();
    EXPECT_EQ(expected[i], popped);
    (*num_popped)++;
  }
}

// Verifies that a thread popping elements off the queue will block
// waiting when no elements are available.
TEST_F(PriorityQueueTest, Pop) {
  const string last = "ZZ";
  vector<string> expected_values = create_values();
  concurrent_priority_queue<string> queue(expected_values);

  // Reverse sort the expected_values to represent the order in which
  // we expect them to be popped. (Highest priority element is popped
  // first.)  Finally, add the 'last' element to the expected_values
  // vector. We will push this onto the queue later.
  sort(expected_values.begin(), expected_values.end());
  reverse(expected_values.begin(), expected_values.end());
  expected_values.push_back(last);

  size_t num_popped = 0;
  thread thr(std::bind(PopElement, std::ref(queue),
                       std::ref(expected_values), &num_popped));
  ThreadMonitor::GetInstance()->WaitUntilBlocked(thr.get_id());

  // PopElement should pop all of the elements from the queue, and
  // then block waiting. Verify that it has done this, and then push
  // the final element.
  EXPECT_EQ(expected_values.size() - 1, num_popped);
  queue.push(last);
  thr.join();
  EXPECT_EQ(expected_values.size(), num_popped);
}

// TODO(alasdair): Add more multithreaded tests to verify pushing from
// multiple threads.

// TODO(alasdair): Add additional tests for the methods currently
// commented out because of C++11 incompatibility.
