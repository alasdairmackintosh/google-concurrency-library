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

#ifndef GCL_MAP_REDUCE_IMPL_
#define GCL_MAP_REDUCE_IMPL_

#include "buffer_queue.h"
#include "functional.h"
#include "queue_base.h"

namespace gcl {

template <typename Iter,
          typename OutIter,
          typename Mapper,
          typename InputSplitter>
void mapper_helper(
    Iter iter,
    Iter iter_end,
    OutIter out_iter,
    countdown_latch* done_latch) {
  typedef typename Mapper::input_type I;
  typedef typename Mapper::key_type K;
  typedef typename Mapper::value_type V;

  Mapper* mapper_obj = new Mapper;

  // Assumes input is completely done before moving on.
  // Without this we need an external signal that there is no additional input.
  //
  // NOTE: For streaming inputs or other things, this is probably the wrong
  // abstraction (we need some arbitrary, closeable input source). The problem
  // is that spinning on the is_empty() function on a standard queue is really
  // slow...
  int num_mapped = 0;
  InputSplitter splitter;
  std::function<void(const I&)> apply_fn =
      std::bind(&Mapper::template map<OutIter>,
                mapper_obj,
                std::placeholders::_1,
                out_iter);
  while (iter != iter_end) {
    try {
      typename Iter::value_type value = *(iter++);
      mapper_obj->start();
      splitter.apply(value, apply_fn);
      mapper_obj->flush();
      ++num_mapped;
    } catch (gcl::closed_error expected) {
      break;
    } catch (...) {
      break;
    }
  }

  done_latch->count_down();
  delete mapper_obj;
}

template <typename K, typename V>
struct map_key_task {
  std::set<K>* work_key_set;
  const std::multimap<K, V>* data_map;
  unsigned int shard_id;

  map_key_task() {
    work_key_set = new std::set<K>();
    data_map = NULL;
    shard_id = 0U;
  }

  map_key_task(const std::multimap<K, V>* data_map, unsigned int shard_id) {
    work_key_set = new std::set<K>();
    this->data_map = data_map;
    this->shard_id = shard_id;
  }

  map_key_task(const map_key_task<K, V>& other) {
    work_key_set = new std::set<K>();
    work_key_set->insert(other.work_key_set->begin(),
                         other.work_key_set->end());
    data_map = other.data_map;
    shard_id = other.shard_id;
  }

  map_key_task<K, V>& operator=(const map_key_task<K, V>& other) {
    work_key_set = new std::set<K>();
    work_key_set->insert(other.work_key_set->begin(),
                         other.work_key_set->end());
    data_map = other.data_map;
    shard_id = other.shard_id;
    return *this;
  }

  ~map_key_task() {
    delete work_key_set;
  }
};

template <typename K, typename V>
class map_key_task_reducer_splitter {
 public:
  template <typename fn>
  void apply(const map_key_task<K, V>& input, fn f) {
    for (typename std::set<K>::const_iterator task_iter =
             input.work_key_set->begin();
         task_iter != input.work_key_set->end();
         ++task_iter) {
      std::pair<typename std::multimap<K, V>::const_iterator,
           typename std::multimap<K, V>::const_iterator> begin_end =
          (*input.data_map).equal_range(*task_iter);
      multimap_value_iterator<V, typename std::multimap<K, V>::const_iterator>
          begin(begin_end.first);
      multimap_value_iterator<V, typename std::multimap<K, V>::const_iterator>
          end(begin_end.second);
      f(*task_iter, &begin, &end);
    }
  }
};

template <typename Iter, typename OutIter,
          typename Reducer,
          typename WorkSplitter>
void reducer_helper(
    Iter iter,
    Iter iter_end,
    OutIter out,
    countdown_latch* done_latch) {
  typedef typename Reducer::key_type K;
  typedef typename Reducer::value_type V;
  typedef typename Reducer::output_value_type O;

  Reducer* reducer_obj = new Reducer();

  //std::function<void(const K&, const O&)> out_fn =
  //    std::bind(&OutputHandler::output,
  //              out,
  //              std::placeholders::_1,
  //              std::placeholders::_2);
  typedef multimap_value_iterator<
      V, typename std::multimap<K, V>::const_iterator> ReducerIter;
  std::function<void(const K&,
      ReducerIter*, ReducerIter*)> reducer_fn = std::bind(
    &Reducer::template reduce<ReducerIter, OutIter>,
    reducer_obj,
    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
    out);
  WorkSplitter splitter;
  // NOTE: is_closed isn't a great signal here since it means we can't take any
  // more input.
  while(iter != iter_end) {
    try {
      map_key_task<K, V> task = *(iter++);
      reducer_obj->start(task.shard_id);
      splitter.apply(task, reducer_fn);
      reducer_obj->flush();
    } catch (gcl::closed_error expected) {
      break;
    }
  }

  done_latch->count_down();
  delete reducer_obj;
}

}  // namespace gcl

#endif  // GCL_MAP_REDUCE_IMPL_
