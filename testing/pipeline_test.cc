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

void consume_string(string input) {
  printf("Consuming %s\n", input.c_str());
}

string process_string(string input) {
  return input + "(processed)";
}

void print_string(string s) {
  printf("%s", s.c_str());
}

void repeat(int i, queue_front<int> q) {
  q.push(i);
  q.push(i);
}

void produce_strings(queue_front<string> queue) {
  printf("Producing strings\n");
  queue.push("Produced String1");
  queue.push("Produced String22");
  queue.push("Produced String333");
  queue.push("Produced String4444");
}

class PipelineTest : public testing::Test {
};

TEST_F(PipelineTest, Example) {
  simple_thread_pool pool;

  std::function <void (int, queue_front<int>)> fr = repeat;
  std::function <User (int uid)> f2 = get_user;
  std::function <void (User user)> c = consume_user;

  // A Runnable Pipeline that reads from a queue and write to a sink
  queue_object< buffer_queue<string> > queue(10, "test-queue");
  queue.push("Queued Hello");

  CXX0X_AUTO_VAR(p3, Source(queue.back()) | Filter(find_uid)
				| Filter(fr) | Filter(f2));

  queue_object< buffer_queue<User> > out(10);
  PipelinePlan p4 = p3 | SinkAndClose(out.front());

  PipelineExecution pex(p4, &pool);
  queue.push("More stuff");
  queue.push("Yet More stuff");
  queue.push("Are we done yet???");

  PipelineExecution pex2(Source(out.back()) | Consume(c, Nothing), &pool);
  queue.close();
  pex.wait();
  pex2.wait();

  EXPECT_TRUE(pex.is_done());
  EXPECT_TRUE(out.is_closed());
  EXPECT_TRUE(pex2.is_done());
}
#if 0
// TODO(alasdair): Fix Parallel test - see comments in pipeline.h
TEST_F(PipelineTest, ParallelExample) {

  // Two-stage pipeline. Combines string->int and int->User to make
  // string->User
  //FIX CXX0X_AUTO_VAR(p2, Filter(find_uid)|Filter(get_user));
  CXX0X_AUTO_VAR(f1, Filter(find_uid));
  CXX0X_AUTO_VAR(f2, Filter(get_user));
  CXX0X_AUTO_VAR(p2, f1|f2);

  queue_object< buffer_queue<string> > queue(10);
  queue.push("Queued Hello");
  queue.push("queued world");
  queue.push("queued 1");
  queue.push("queued 22");
  queue.push("queued 333");
  queue.push("queued 4444");
  queue.push("queued 55555");
  queue.push("queued 666666");

  CXX0X_AUTO_VAR(p3, Parallel(p2));
  CXX0X_AUTO_VAR(s, Source(queue.back()));

  PipelinePlan p4 = s | p3 | Consume(consume_user, Nothing);

  simple_thread_pool pool;
  PipelineExecution pex(p4, &pool);
  queue.push("More stuff");
  queue.push("Yet More stuff");
  queue.push("Are we done yet???");
  queue.close();
  pex.wait();
  printf("Done!!!");
}
#endif
TEST_F(PipelineTest, ProduceExample) {
  simple_thread_pool pool;
  PipelinePlan p5 = Produce<string>(produce_strings) | Filter(find_uid) |
      Filter(get_user) | Consume(consume_user, Nothing);
  printf("Starting Execution\n");
  PipelineExecution pex3(p5, &pool);
  printf("Waiting for Completion\n");
  pex3.wait();
}
