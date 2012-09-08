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

#ifndef GCL_MAP_REDUCE_HELPERS_
#define GCL_MAP_REDUCE_HELPERS_

#include <iterator>
#include <map>
#include <vector>

#include "mutex.h"

namespace gcl {

template <typename MapType>
class blocking_map : public MapType {
 public:
  std::pair<typename MapType::iterator, bool> insert(
      const typename MapType::value_type& value) {
    lock_guard<mutex> map_lock(mu_);
    return MapType::insert(value);
  }
 private:
  mutex mu_;
};

// Output iterator over a map
template <typename MapType>
class map_output_iter :
    std::iterator<std::output_iterator_tag,
                  typename MapType::value_type,
                  void, void, void> {
 public:
  map_output_iter(MapType* m) {
    m_ = m;
  }
  map_output_iter() : m_(static_cast<MapType*>(NULL)) {}

  map_output_iter& operator *() { return *this; }
  map_output_iter& operator ++() { return *this; }
  map_output_iter& operator ++(int) { return *this; }
  map_output_iter& operator =(const typename MapType::value_type& value) {
    // Value may already be in there, but we have no way to communicate this to
    // the caller.
    m_->insert(value);
    return *this;
  }
  
 private:
  // Unowned
  MapType* m_;
};

template <typename Container>
class push_back_output_iter :
    std::iterator<std::output_iterator_tag,
                  typename Container::value_type,
                  void, void, void> {
 public:
  push_back_output_iter(Container* m) {
    m_ = m;
  }
  push_back_output_iter() : m_(static_cast<Container*>(NULL)) {}

  push_back_output_iter& operator *() { return *this; }
  push_back_output_iter& operator ++() { return *this; }
  push_back_output_iter& operator ++(int) { return *this; }
  push_back_output_iter& operator =(
      const typename Container::value_type& value) {
    m_->push_back(value);
    return *this;
  }
  
 private:
  // Unowned
  Container* m_;
};

template <typename V>
class value_iterator {
 public:
  virtual bool operator ==(const value_iterator<V>& other) = 0;
  virtual bool operator !=(const value_iterator<V>& other) = 0;
  virtual void operator =(const value_iterator<V>& other) = 0;

  virtual value_iterator<V>& operator ++() = 0;
  virtual void operator ++(int) = 0; 
  
  virtual const V& operator *() = 0;
  virtual const V* operator ->() = 0;
};

// Special iterator which converts a key-value iterator (iterator over pairs)
// into a value iterator. Useful in reducer stage to wrap map iterators.
template <typename V, typename InputIterator>
class multimap_value_iterator : public value_iterator<V> {
 public:
  multimap_value_iterator(InputIterator iter) : iter_(iter) {}
  virtual bool operator ==(
      const value_iterator<V>& other) {
    const multimap_value_iterator<V, InputIterator>& other_cast =
        dynamic_cast<const multimap_value_iterator<V, InputIterator>&>(other);
    return iter_ == other_cast.iter_;
  }
  virtual bool operator !=(const value_iterator<V>& other) {
    return !((*this) == other);
  }
  virtual void operator =(const value_iterator<V>& other) {
    const multimap_value_iterator<V, InputIterator>& other_cast =
        dynamic_cast<const multimap_value_iterator<V, InputIterator>&>(other);
    iter_ = other_cast.iter_;
  }

  // Iterator interface
  // TODO: maybe use iterator_traits? Post-increment?
  virtual multimap_value_iterator<V, InputIterator>& operator ++() {
      ++iter_; return *this; }
  virtual void operator ++(int) { ++(*this); }
  
  virtual const V& operator *() { return iter_->second; }
  virtual const V* operator ->() { return &iter_->second; }
 private:
  InputIterator iter_;
};

template <typename T>
class default_shard {
 public:
  default_shard() {}
  size_t operator()(const T& val, size_t num_shards) {
    return hash_fn(val) % num_shards;
  }
 private:
  std::hash<T> hash_fn;
};

template <typename I, typename J>
class identity_splitter {
 public:
  template <typename fn>
  void apply(const I& input, fn f) {
    f(input);
  }
};

// ------------------------
// Special Mappers/Reducers
// ------------------------

// TODO: figure out how to make this work in a template based mapreduce.
//template <typename I, typename K, typename V>
//class lambda_mapper {
// public:
//  lambda_mapper(std::function<void(const I&,
//                              std::function<void(const K&, const V&)>)> f)
//    : fn_(f) {}
//
//  lambda_mapper*
//  virtual void flush() {}
//  virtual lambda_mapper<I, K, V>* clone() {
//    return new lambda_mapper<I, K, V>(fn_);
//  }
//  virtual void map(const I& input,
//                   std::function<void(const K&, const V&)> output) {
//    fn_(input, output);
//  }
//  
// private:
//  std::function<void(const I&, std::function<void(const K&, const V&)>)> fn_;
//};

// TODO: figure out how to make this work with template based mapreduce.
//template <typename K, typename V, typename O>
//class lambda_reducer : public reducer<K, V, O> {
// public:
//  lambda_reducer(std::function<void(const K&,
//                                    value_iterator<V>*,
//                                    value_iterator<V>*,
//                                    std::function<void(const K&, const V&)>)> f)
//    : fn_(f) {}
//  virtual void flush() {}
//  virtual lambda_reducer<K, V, O>* clone() {
//    return new lambda_reducer<K, V, O>(fn_);
//  }
//  virtual void reducer(const K& key,
//                       value_iterator<V>* value_start,
//                       value_iterator<V>* value_end,
//                       std::function<void(const K&, const O&)> output) {
//    fn_(key, value_start, value_end, output);
//  }
//  
// private:
//  std::function<void(const K&, value_iterator<V>*, value_iterator<V>*,
//                     std::function<void(const K&, const V&)>)> fn_;
//};

template<typename K, typename V>
class identity_reducer {
 public:
  typedef K key_type;
  typedef V value_type;
  typedef V output_value_type;

  void start(unsigned int shard_id) {}
  void reduce(
      const int& key,
      value_iterator<V>* value_start,
      value_iterator<V>* value_end,
      std::function<void(const K&, const V&)> output) {
    for (value_iterator<float>* value_iter = value_start;
         (*value_iter) != (*value_end); ++(*value_iter)) {
      output(key, *(*value_iter));
    }
  }
  void flush() {}
};


}  // namespace gcl

#endif GCL_MAP_REDUCE_HELPERS_
