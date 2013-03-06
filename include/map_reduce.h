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

// TODO:
// - implement map-reduce chaining
// - implement the combiner

#ifndef GCL_MAP_REDUCE_
#define GCL_MAP_REDUCE_

#include <deque>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdio.h>
#include <vector>

#include "buffer_queue.h"
#include "closed_error.h"
#include "countdown_latch.h"
#include "functional.h"
#include "map_reduce_helpers.h"
#include "map_reduce_impl.h"
#include "mutable_thread.h"
#include "mutex.h"
#include "queue_base.h"
#include "simple_thread_pool.h"
#include "thread.h"

namespace gcl {

template <typename I, typename K, typename V, typename O,
          typename OutIter>
struct map_reduce_options {
  size_t num_mappers;
  // Number of shards for splitting data. Controls how much chunking there is
  // in the source data to speed up completion of work units.
  size_t num_reduce_shards;
  size_t num_reducers;

  // Distributed mapreduce requires named mapper/reducers and a mapper/reducer
  // factory. When creating mapper tasks, we need to be able to clone mappers.
  // This is a feature of the implementation, though.

  simple_thread_pool* thread_pool;

  // NOTE: this needs to be a thread-safe output function.
  // In the shared-memory form of this can be a template parameter to remove
  // the virtual function calls.
  OutIter out;
};

// map_reduce class is templatized on the mapper/reducer classes as well as
// some helpers which define how sharding is handled and if input needs to be
// split before processing. Uses a thread-pool to create mapper tasks.
//
// Must override shard_fn if the key type does not have a std::hash
// specialization defined or you want to use a non-hash based sharding function.
//
// template params:
// Iter - iterator type over input
// OutIter - output iterator where reduced values will go
// Mapper - mapper class which supports start/map/flush functions
// Reducer - reducer class which supports start/reduce/flush functions
// Combiner - reducer used to combine values prior to sending to the shuffler
// shard_fn - sharding function which defines which values go to which reducers
// input_splitter -
template <typename Iter,
          typename OutIter,
          typename Mapper,
          typename Reducer,
          typename Combiner = identity_reducer<typename Reducer::key_type,
                                               typename Reducer::value_type>,
          typename shard_fn = default_shard<typename Mapper::key_type>,
          typename input_splitter =
              identity_splitter<typename Iter::value_type,
                                typename Mapper::input_type> >
class map_reduce {
 private:
  typedef typename Iter::value_type I;
  typedef typename Mapper::input_type J;
  typedef typename Mapper::key_type K;
  typedef typename Mapper::value_type V;
  typedef typename Reducer::output_value_type O;

 public:
  map_reduce(
      const map_reduce_options<typename Mapper::input_type,
                               typename Mapper::key_type,
                               typename Mapper::value_type,
                               typename Reducer::output_value_type,
                               OutIter>& opts)
     : opts_(opts) {}

  // Input to the mr job is done by using the basic queue structures and if
  // there needs to be iteration over source data this can be done by creating
  // work units passed through the queues.

  void run(Iter iter, Iter end) {
    countdown_latch mapper_latch(opts_.num_mappers);
    mapper_outputs_ = new std::multimap<K, V>[opts_.num_mappers];

    std::deque<mutable_thread*> mapper_threads(opts_.num_mappers);
    for (size_t i = 0; i < opts_.num_mappers; ++i) {
      mapper_threads[i] = opts_.thread_pool->try_get_unused_thread();
      if (NULL != mapper_threads[i]) {
        std::cout << "start mapper " << i << std::endl;

        // TODO: add combiner here by calling the combiner when there is an
        // input in the output iter already and we want to merge the outputs?
        // NOTE: we don't know from the iterator if there is a matching value
        // in the map already
        mapper_threads[i]->execute(
            std::bind(&mapper_helper<
                          Iter,
                          map_output_iter<std::multimap<K, V> >,
                          Mapper,
                          input_splitter>,
                      iter,
                      end,
                      map_output_iter<std::multimap<K, V> >(
                          &mapper_outputs_[i]),
                      &mapper_latch));
      }
    }
    mapper_latch.wait();

    // TODO: create configurable shuffler to enable disk-based shuffle.
    // then shuffle the output from the mappers.
    for (size_t i = 0; i < opts_.num_mappers; ++i) {
      // return the thread to the pool
      opts_.thread_pool->release_thread(mapper_threads[i]);
      reducer_input_.insert(mapper_outputs_[i].begin(),
                            mapper_outputs_[i].end());

    }

    std::map<V, int> val_counts;
    for (typename std::multimap<K, V>::iterator reducer_iter =
            reducer_input_.begin();
         reducer_iter != reducer_input_.end();
         ++reducer_iter) {
      val_counts[reducer_iter->second]++;
    }
    for (typename std::map<V, int>::iterator val_iter = val_counts.begin();
         val_iter != val_counts.end();
         ++val_iter) {
      std::cout << val_iter->first << " " << val_iter->second << std::endl;
    }

    shard_fn sharder;
    std::map<size_t, map_key_task<K, V> > work_shards;
    typename std::multimap<K, V>::iterator prev_iter = reducer_input_.end();
    for (typename std::multimap<K, V>::iterator reducer_iter =
            reducer_input_.begin();
         reducer_iter != reducer_input_.end();
         ++reducer_iter) {
      if (reducer_iter->first == prev_iter->first) {
        continue;
      }
      // Create work units for the reducer tasks by using the shard
      // function in the mapper object.
      size_t shard_number = sharder(reducer_iter->first,
                                    opts_.num_reduce_shards);
      if (work_shards.find(shard_number) == work_shards.end()) {
        work_shards[shard_number] = map_key_task<K, V>(&reducer_input_,
                                                       shard_number);
      }
      work_shards[shard_number].work_key_set->insert(reducer_iter->first);
      prev_iter = reducer_iter;
    }

    // Copy work tasks to the reducer queue
    //
    // TODO: find a simpler way of passing work directly to the reducers rather
    // than having to create work-sets which are a little expensive to make.
    // this requires an interface change where we don't use queues to the next
    // stage in the pipeline.
    buffer_queue<map_key_task<K, V> > reducer_inputs(work_shards.size());
    queue_wrapper<buffer_queue<map_key_task<K, V> > > reducer_input_wrap(
        reducer_inputs);
    for (typename std::map<size_t, map_key_task<K, V> >::iterator work_iter =
             work_shards.begin();
         work_iter != work_shards.end();
         ++work_iter) {
      reducer_inputs.push(work_iter->second);
    }
    reducer_inputs.close();

    countdown_latch reducer_latch(opts_.num_reducers);
    std::deque<mutable_thread*> reducer_threads;
    for (size_t r = 0; r < opts_.num_reducers; ++r) {
      reducer_threads.push_back(opts_.thread_pool->try_get_unused_thread());
      std::cout << "start reducer " << r << std::endl;
      if (reducer_threads[r]) {
        reducer_threads[r]->execute(
            std::bind(&reducer_helper<
                          queue_front_iter<queue_base<map_key_task<K, V> > >,
                          OutIter,
                          Reducer,
                          map_key_task_reducer_splitter<K, V> >,
                      reducer_input_wrap.begin(),
                      reducer_input_wrap.end(),
                      opts_.out,
                      &reducer_latch));
      }
    }

    reducer_latch.wait();
    std::cout << "wait done" << std::endl;

    std::cout << "release threads" << std::endl;
    // release threads back to pool
    for (size_t r = 0; r < opts_.num_reducers; ++r) {
      opts_.thread_pool->release_thread(reducer_threads[r]);
    }
  }

 private:
  const map_reduce_options<I, K, V, O, OutIter>& opts_;
  // Note that these could be thread-local objects managed by the mapper-helper.
  std::multimap<K, V>* mapper_outputs_;
  std::multimap<K, V> reducer_input_;
};

// map_reduce_lite is a simplified form of map_reduce which doesn't have an
// input splitter, so you only have to provide a mapper and reducer class.
//
// Must override shard_fn if the key type does not have a std::hash
// specialization defined or you want to use a non-hash based sharding function.
//
// template <typename Iter,
//           typename Mapper,
//           typename Reducer,
//           typename shard_fn = default_shard<typename Mapper::key_type> >
// class map_reduce_lite {
//  private:
//   typedef typename Mapper::input_type J;
//   typedef typename Mapper::key_type K;
//   typedef typename Mapper::value_type V;
//   typedef typename Reducer::output_value_type O;
// 
//  public:
//   map_reduce_lite(
//       const map_reduce_options<typename Mapper::input_type,
//                                typename Mapper::key_type,
//                                typename Mapper::value_type,
//                                typename Reducer::output_value_type>& opts)
//      : map_reducer_(opts) {}
// 
//   void run(Iter iter, Iter iter_end) {
//     map_reducer_.run(iter, iter_end);
//   }
// 
//  private:
//   // This is just a simple mapper around map_reduce
//   map_reduce<J, Mapper, Reducer, shard_fn> map_reducer_;
// };

// NOTE: for the in-memory version of mapreduce, inheritance is not required,
// but for distributed mapreduce you need to be able to push some default
// behavior into the mapper (like notifying the parent mapreduce that a given
// map-shard was completed successfuly). It also helps in building a more
// generic mapreduce class which can be configured at runtime instead of
// statically at compile time.
template <typename I, typename K, typename V>
class mapper {
 public:
  typedef I input_type;
  typedef K key_type;
  typedef V value_type;

  virtual void start() {}
  template <typename OutIter>
  void map(const I& input,
           std::function<void(const K&, const V&)> output) {};
  virtual void flush() {}

  // Missing:
  // - accept_shard()
  // - discard_remaining_inputs()
  // - abort()
  // - is_abortable()
  // - output_to_every_shard()
  // - get_reader()
};

// Abstract reducer class interface. This defines how a reducer should be
// implemented to work with the map_reduce framework.
// InputIterator is a value iterator over all values for a given key.
//
// NOTE: I hate having InputIterator as a template parameter because it exposes
// the internals of how the reducer receives input to the outside world. But
// forcing the reducer to receive a subclass of an interface doesn't strike me
// as all that great either.
template <typename K, typename V, typename O>
class reducer {
 public:
  typedef K key_type;
  typedef V value_type;
  typedef O output_value_type;

  virtual reducer<K, V, O>* clone() = 0;

  // Initialization function which clears out the state before the reducer
  // begins handling an input shard. This may be called 1 or more times
  // depending on the number of input shards being processed by the reducer.
  virtual void start() {}

  // Called after at least one key has been completely processed by the reducer
  // NOTE: should this only be called on work-shard boundaries instead of at
  // the key-level?
  virtual void flush() {}

  // Reduce a single key. Reducer will call the output function to write the
  // completed value. Receives an input iterator over a set of values.
  template <typename InIter, typename OutIter>
  void reduce(
      const K& key,
      InIter* value_start,
      InIter* value_end,
      OutIter output) {};

 protected:
  // Missing:
  // - abort()
  // - is_abortable()
  // - worker_id()
  // - is_combine()
  // - current_reduce_shard()
};

}  // End namespace gcl

#endif // GCL_MAP_REDUCE_
