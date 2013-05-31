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

#include <string>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <vector>

#include "pipeline.h"
#include "buffer_queue.h"
#include "countdown_latch.h"
#include "source.h"

#include "gtest/gtest.h"

using std::cout;
using std::string;
using std::vector;

using namespace gcl;

// We have a Pipeline that converts string to UserIDs, and then
// UserIDs to User objects.

// Dummy User class
class User {
 public:
  User(int uid = 0) : uid_(uid) {
  }
  string get_name() const {
    std::stringstream o;
    o << "(User : " << uid_ << ")";
    return o.str();
  }
 private:
  int uid_;
};

std::ostream& operator<<(std::ostream& os, const User u) {
  os << u.get_name();
  return os;
}


// String -> UID
int find_uid(string val) {
  printf("find_uid for %s\n", val.c_str());
  return val.length();
}

// UID -> User
User get_user(int uid) {
  printf("get for %d\n", uid);
  return User(uid);
}

// Processes the User
void consume_user(User input) {
  printf("Consuming user %s\n", input.get_name().c_str());
}

void consume_string(string input) {
  printf("Consuming %s\n", input.c_str());
}

string process_string(string input) {
  return input + "(processed)";
}

void print_string(string s) {
  printf("%s", s.c_str());
}

void repeat(int i, queue_back<int> q) {
  q.push(i);
  q.push(i);
}

int sum_two(queue_front<int> q) {
  int i;
  queue_op_status status = q.wait_pop(i);
  if (status != CXX11_ENUM_QUAL(queue_op_status)success) {
    return -1;
  }
  int j;
  status = q.wait_pop(j);
  if (status != CXX11_ENUM_QUAL(queue_op_status)success) {
    return i;
  }
  return i + j;
}

void produce_strings(queue_back<string> queue) {
  printf("Producing strings\n");
  queue.push("Produced String1");
  queue.push("Produced String22");
  queue.push("Produced String333");
  queue.push("Produced String4444");
}

class PipelineTest : public testing::Test {
};

TEST_F(PipelineTest, ManualBuild) {
  simple_thread_pool pool;
  queue_object< buffer_queue<int> > queue(10);

  pipeline::segment<pipeline::terminated, int> p1 = pipeline::from(queue);

  pipeline::segment<int, int> p6 = pipeline::make(repeat);
  pipeline::segment<int, int> p7 = pipeline::make(sum_two);

  pipeline::segment<int, User> p2 = pipeline::make(get_user);

  pipeline::segment<pipeline::terminated, User> p3 = p1 | p6 | p7| p2;

  pipeline::segment<User, pipeline::terminated> p4 = pipeline::to(consume_user);

  pipeline::segment<pipeline::terminated, pipeline::terminated> p = p3 | p4;


  pipeline::execution exec = p.run(&pool);

  queue.push(10);
  queue.close();
}


TEST_F(PipelineTest, Example) {
  simple_thread_pool pool;


  // A Runnable Pipeline that reads from a queue and write to a sink
  queue_object< buffer_queue<string> > queue(10);
  queue.push("Queued Hello");

  CXX11_AUTO_VAR(p1, pipeline::make(find_uid));
  CXX11_AUTO_VAR(p2, p1 | repeat);
  CXX11_AUTO_VAR(p3, queue | p2 | get_user );

  queue_object< buffer_queue<User> > out(10);
  pipeline::plan p4 = p3 | out;

  pipeline::execution pex = p4.run(&pool);
  queue.push("More stuff");
  queue.push("Yet More stuff");
  queue.push("Are we done yet???");

  pipeline::execution pex2 = (pipeline::from(out) | consume_user).run(&pool);
  queue.close();
  pex.wait();
  pex2.wait();

  EXPECT_TRUE(pex.is_done());
  EXPECT_TRUE(out.is_closed());
  EXPECT_TRUE(pex2.is_done());
}

int pass_through(int i) { return i; }

TEST_F(PipelineTest, SimpleParallel) {
  simple_thread_pool pool;
  queue_object< buffer_queue<int> > in_queue(10);
  queue_object< buffer_queue<int> > out_queue(10);
  in_queue.push(1);
  in_queue.push(2);
  in_queue.push(3);
  in_queue.close();

  pipeline::plan p = pipeline::from(in_queue)
      | pipeline::parallel(pipeline::make(pass_through), 2)
      | out_queue;
  pipeline::execution pex = p.run(&pool);
  pex.wait();
  EXPECT_TRUE(out_queue.is_closed());
}

TEST_F(PipelineTest, ParallelExample) {
  // Two-stage pipeline. Combines string->int and int->User to make
  // string->User
  pipeline::segment<string, User> p2 =
      pipeline::make(find_uid) | get_user;
  queue_object< buffer_queue<string> > queue(10);
  queue.push("Queued Hello");
  queue.push("queued world");
  queue.push("queued 1");
  queue.push("queued 22");
  queue.push("queued 333");
  queue.push("queued 4444");

  CXX11_AUTO_VAR(p3, pipeline::parallel(p2, 2));
  CXX11_AUTO_VAR(s, pipeline::from(queue));
  CXX11_AUTO_VAR(px, s | p3);

  pipeline::plan p4 = px | consume_user;

  simple_thread_pool pool;
  pipeline::execution pex = p4.run(&pool);
  queue.push("More stuff");
  queue.push("Yet More stuff");
  queue.push("Are we done yet???");
  queue.close();
  pex.wait();
  printf("Done!!!");
}

TEST_F(PipelineTest, ProduceExample) {
  simple_thread_pool pool;
  pipeline::plan p5 =
      pipeline::from(produce_strings) | find_uid | get_user | consume_user;
  printf("Starting Execution\n");
  pipeline::execution pex3 = p5.run(&pool);
  printf("Waiting for Completion\n");
  pex3.wait();
}
