// Copyright 2011 Google Inc. All Rights Reserved.
//

#include "pipeline.h"
#include "blocking_queue.h"
#include "source.h"

#include "gmock/gmock.h"
#include <tr1/functional>
#include <string>
#include <sstream>
#include <iostream>
#include <stdio.h>

using std::cout;
using std::string;
using std::tr1::function;
using std::tr1::bind;
using std::tr1::placeholders::_1;

using gcl::blocking_queue;
using gcl::simple_thread_pool;
using gcl::source;
using gcl::Pipeline;
using gcl::StartablePipeline;
using gcl::RunnablePipeline;

// We have a Pipeline that converts string to UserIDs, and then
// UserIDs to User objects.

// Dummy User class
class User {
 public:
  User(int uid) : uid_(uid) {
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

class PipelineTest : public testing::Test {
};

// TODO(alasdair): Not really a test - just shows a (very basic)
// pipeline in action. Add proper tests once the interface is sorted
// out.
TEST_F(PipelineTest, Example) {

  function <int (string input)> f1 = find_uid;
  function <User (int uid)> f2 = get_user;
  function <void (User user)> c = consume_user;

  // Simple one-stage pipeline
  Pipeline<string, int> p1(f1);
  int uid = p1.apply("hello");
  printf("Got uid %d\n", uid);

  // Two-stage pipeline. Combines string->int and int->User to make
  // string->User
  Pipeline<string, User> p2 = Pipeline<string, int>(f1).Filter<User>(f2);
  User a2 = p2.apply("hello world");
  printf("Got %s from pipeline\n", a2.get_name().c_str());


  // RunnablePipeline that reads from a queue and writers to a sink
  typedef source<string, blocking_queue<string> > string_source;
  blocking_queue<string> queue;
  queue.push("Queued Hello");
  queue.push("queued world");
  string_source in_source(queue);
  StartablePipeline<string, User, string_source> p3 =
    Pipeline<string, int>(f1).Filter<User>(f2)
      .Source<string_source>(&in_source);

  simple_thread_pool pool;
  RunnablePipeline<string, User, string_source> p4 = p3.Consume(c);
  p4.run(pool);

  queue.close();
  // Short sleep just to give the threads some time to execute.

  // TODO(alasdair): How do we wait for pipelines to terminate?
  this_thread::sleep_for(chrono::milliseconds(1));
}
