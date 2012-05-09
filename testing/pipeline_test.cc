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
#include "blocking_queue.h"
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
  string get_name() {
    std::stringstream o;
    o << "(User : " << uid_ << ")";
    return o.str();
  }
 private:
  int uid_;
};

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
  printf("Consuming %s\n", input.get_name().c_str());
}


void print_string(string s) {
  printf("%s", s.c_str());
}

void repeat(int i, queue_front<int>* q) {
  q->push(i);
  q->push(i);
}

void produce_strings(queue_front<string>* queue) {
  printf("Producing strings\n");
  queue->push("Produced String1");
  queue->push("Produced String22");
  queue->push("Produced String333");
  queue->push("Produced String4444");
}

class PipelineTest : public testing::Test {
};

TEST_F(PipelineTest, Example) {
  simple_thread_pool pool;

  std::function <void (int, queue_front<int>*)> fr = repeat;
  std::function <User (int uid)> f2 = get_user;
  std::function <void (User user)> c = consume_user;

  // Simple one-stage pipeline
  CXX0X_AUTO_VAR(p1, Filter(find_uid));
  int uid = p1.Apply("hello");
  printf("Got uid %d\n", uid);

  // Two-stage pipeline. Combines string->int and int->User to make
  // string->User
  printf("Creating p2\n");
  CXX0X_AUTO_VAR(p2, Filter(find_uid) | Filter(f2));
  User a2 = p2.Apply("hello world");
  printf("Got %s from pipeline\n", a2.get_name().c_str());

  // A Runnable Pipeline that reads from a queue and write to a sink
  buffer_queue<string> queue(10);
  queue.push("Queued Hello");

  printf("Creating p3\n");
  CXX0X_AUTO_VAR(p3,
                 Source(&queue) | Filter(find_uid) | Filter(fr) | Filter(f2));

  printf("Creating p4\n");
  buffer_queue<User> out(10);
  PipelinePlan p4 = p3|SinkAndClose(&out);

  printf("Starting p4\n");
  PipelineExecution pex(p4, &pool);
  printf("Running p4\n");
  queue.push("More stuff");
  queue.push("Yet More stuff");
  EXPECT_FALSE(pex.is_done());
  queue.push("Are we done yet???");
  PipelineExecution pex2(Source(&out)|Consume(c), &pool);
  EXPECT_FALSE(pex2.is_done());
  queue.close();

  sleep(1);
  // RACY!!!!
  EXPECT_TRUE(pex.is_done());
  printf("Ending p4\n");
  pex.wait();
  printf("Test Done\n");
  EXPECT_TRUE(out.is_closed());
  EXPECT_TRUE(pex2.is_done());
}

TEST_F(PipelineTest, ParallelExample) {

  // Two-stage pipeline. Combines string->int and int->User to make
  // string->User
  CXX0X_AUTO_VAR(p2, Filter(find_uid)|Filter(get_user));

  buffer_queue<string> queue(10);
  queue.push("Queued Hello");
  queue.push("queued world");
  queue.push("queued 1");
  queue.push("queued 22");
  queue.push("queued 333");
  queue.push("queued 4444");
  queue.push("queued 55555");
  queue.push("queued 666666");

  CXX0X_AUTO_VAR(p3, Parallel(p2));
  CXX0X_AUTO_VAR(s, Source(&queue));

  PipelinePlan p4 = s|p3|Consume(consume_user);

  simple_thread_pool pool;
  PipelineExecution pex(p4, &pool);
  queue.push("More stuff");
  queue.push("Yet More stuff");
  queue.push("Are we done yet???");
  queue.close();
  pex.wait();
  printf("Done!!!");
}

TEST_F(PipelineTest, ProduceExample) {
  simple_thread_pool pool;
  PipelinePlan p5 = Produce<string>(produce_strings) | Filter(find_uid) |
      Filter(get_user) | Consume(consume_user);
  printf("Starting Execution\n");
  PipelineExecution pex3(p5, &pool);
  printf("Waiting for Completion\n");
  pex3.wait();
}
