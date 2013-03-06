// Copyright 2012 Google Inc. All Rights Reserved.
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
#include <iterator>
#include <map>

#include "buffer_queue.h"
#include "functional.h"
#include "map_reduce.h"
#include "map_reduce_helpers.h"
#include "queue_base.h"
#include "simple_thread_pool.h"
#include "gtest/gtest.h"
#include "thread.h"

using gcl::buffer_queue;
using gcl::map_reduce_options;
using gcl::map_reduce;
using gcl::mapper;
using gcl::queue_front;
using gcl::queue_front_iter;
using gcl::queue_base;
using gcl::queue_wrapper;
using gcl::reducer;
using gcl::simple_thread_pool;

class BucketizingMapper {
 public:
  typedef int input_type;
  typedef int key_type;
  typedef float value_type;

  void start() {}

  template <typename OutIter>
  void map(const int& input,
           OutIter output) {
    output = std::pair<int, float>(input % 10, static_cast<float>(input));
  }

  void flush() {}
};

class AveragingReducer {
 public:
  typedef int key_type;
  typedef float value_type;
  typedef float output_value_type;

  void start(unsigned int shard_id) {}

  template <typename InIter, typename OutIter>
  void reduce(
      const int& key,
      InIter* value_start,
      InIter* value_end,
      OutIter output) {
    double value_sum = 0.0;
    int num_values = 0;
    for (InIter* value_iter = value_start;
         (*value_iter) != (*value_end); ++(*value_iter)) {
      value_sum += *(*value_iter);
      ++num_values;
    }

    std::cout << "reduce output " << key << " " << value_sum / num_values << std::endl;
    output = std::pair<int, float>(key, value_sum / num_values);
  }
  void flush() {}
};

class MapReduceTest : public testing::Test {
};

TEST_F(MapReduceTest, TestMR) {

  typedef gcl::blocking_map<std::map<int, float> > blocking_output_map;
  typedef gcl::map_output_iter<blocking_output_map> output_iter;
  typedef queue_front_iter<queue_base<int> > input_iter;

  blocking_output_map out_map;
  map_reduce_options<int, int, float, float, output_iter> opts;
  opts.num_mappers = 3;
  opts.num_reduce_shards = 19;
  opts.num_reducers = 5;
  opts.thread_pool = new simple_thread_pool;

  output_iter out_iter(&out_map);
  opts.out = out_iter;

  map_reduce<input_iter, output_iter,
             BucketizingMapper,
             AveragingReducer> mr(opts);
  buffer_queue<int> input_queue(1000);
  input_queue.push(10); // avg = 10.0

  input_queue.push(12);
  input_queue.push(12);
  input_queue.push(22);
  input_queue.push(22); // avg = 17.0

  // avg = 103.0
  for (int i = 0; i < 20; ++i) input_queue.push(103);

  // avg = 2754.0
  for (int i = 0; i < 20; ++i) input_queue.push(1004);
  for (int i = 0; i < 20; ++i) input_queue.push(2004);
  for (int i = 0; i < 40; ++i) input_queue.push(4004);

  // avg = 1025
  for (int i = 0; i < 40; ++i) input_queue.push(1005);
  for (int i = 0; i < 40; ++i) input_queue.push(1015);
  for (int i = 0; i < 40; ++i) input_queue.push(1025);
  for (int i = 0; i < 40; ++i) input_queue.push(1035);
  for (int i = 0; i < 40; ++i) input_queue.push(1045);
  input_queue.close();
  queue_wrapper<buffer_queue<int> > input_wrap(input_queue);
  mr.run(input_wrap.begin(), input_wrap.end());

  // Check the outputs!
  EXPECT_EQ(10.0, out_map[0]);
  EXPECT_EQ(17.0, out_map[2]);
  EXPECT_EQ(103.0, out_map[3]);
  EXPECT_EQ(2754.0, out_map[4]);
  EXPECT_EQ(1025.0, out_map[5]);
}
