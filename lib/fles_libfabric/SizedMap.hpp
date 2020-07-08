// Copyright 2017 Farouk Salem <salem@zib.de>
#pragma once

#include "ConstVariables.hpp"
#include <assert.h>
#include <map>

namespace tl_libfabric {
template <typename KEY, typename VALUE> class SizedMap {
public:
  SizedMap(uint32_t max_map_size);
  SizedMap();
  // SizedMap(const SizedMap&) = delete;
  // SizedMap& operator=(const SizedMap&) = delete;

  bool add(const KEY key, const VALUE val);

  bool update(const KEY key, const VALUE val);

  bool remove(const KEY key);

  bool remove(const typename std::map<KEY, VALUE>::iterator iterator);

  bool contains(const KEY key) const;

  bool empty() const;

  uint32_t size() const;

  VALUE get(const KEY key) const;

  KEY get_last_key() const;

  typename std::map<KEY, VALUE>::iterator get_begin_iterator();

  typename std::map<KEY, VALUE>::iterator get_iterator(const KEY key);

  typename std::map<KEY, VALUE>::iterator get_end_iterator();

  typedef typename std::map<KEY, VALUE>::iterator iterator;

private:
  typename std::map<KEY, VALUE> map_;
  const uint32_t MAX_MAP_SIZE_;
};

template <typename KEY, typename VALUE>
SizedMap<KEY, VALUE>::SizedMap(uint32_t max_map_size)
    : MAX_MAP_SIZE_(max_map_size) {}

template <typename KEY, typename VALUE>
SizedMap<KEY, VALUE>::SizedMap()
    : MAX_MAP_SIZE_(ConstVariables::MAX_HISTORY_SIZE) {}

template <typename KEY, typename VALUE>
bool SizedMap<KEY, VALUE>::add(const KEY key, const VALUE val) {
  if (map_.find(key) != map_.end()) {
    return false;
  }

  if (map_.size() == MAX_MAP_SIZE_) {
    map_.erase(map_.begin());
    assert(map_.size() == MAX_MAP_SIZE_ - 1);
  }

  map_.insert(std::pair<KEY, VALUE>(key, val));

  return true;
}

template <typename KEY, typename VALUE>
bool SizedMap<KEY, VALUE>::update(const KEY key, const VALUE val) {
  typename std::map<KEY, VALUE>::iterator it = map_.find(key);
  if (it == map_.end()) {
    return false;
  }

  it->second = val;

  return true;
}

template <typename KEY, typename VALUE>
bool SizedMap<KEY, VALUE>::remove(const KEY key) {
  if (contains(key)) {
    map_.erase(key);
    return true;
  }
  return false;
}

template <typename KEY, typename VALUE>
bool SizedMap<KEY, VALUE>::remove(
    const typename std::map<KEY, VALUE>::iterator iterator) {
  if (iterator != map_.end()) {
    map_.erase(iterator);
    return true;
  }
  return false;
}

template <typename KEY, typename VALUE>
bool SizedMap<KEY, VALUE>::contains(const KEY key) const {
  return !empty() && map_.find(key) != map_.end() ? true : false;
}

template <typename KEY, typename VALUE>
bool SizedMap<KEY, VALUE>::empty() const {
  return size() == 0 ? true : false;
}

template <typename KEY, typename VALUE>
uint32_t SizedMap<KEY, VALUE>::size() const {

  return map_.size();
}

template <typename KEY, typename VALUE>
VALUE SizedMap<KEY, VALUE>::get(const KEY key) const {

  return map_.find(key)->second;
}

template <typename KEY, typename VALUE>
KEY SizedMap<KEY, VALUE>::get_last_key() const {

  return (--map_.end())->first;
}

template <typename KEY, typename VALUE>
typename std::map<KEY, VALUE>::iterator
SizedMap<KEY, VALUE>::get_begin_iterator() {
  return map_.begin();
}

template <typename KEY, typename VALUE>
typename std::map<KEY, VALUE>::iterator
SizedMap<KEY, VALUE>::get_iterator(const KEY key) {
  return map_.find(key);
}

template <typename KEY, typename VALUE>
typename std::map<KEY, VALUE>::iterator
SizedMap<KEY, VALUE>::get_end_iterator() {
  return map_.end();
}
} // namespace tl_libfabric
