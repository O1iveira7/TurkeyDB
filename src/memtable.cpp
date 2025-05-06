#include "memtable.h"

#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <utility>
#include <vector>

#include "iterator.h"
#include "consts.h"
#include "skiplist.h"
#include "sst.h"

class BlockCache;

// MemTable implementation using PIMPL idiom
MemTable::MemTable() : frozen_bytes(0) {
  current_table = std::make_shared<SkipList>();
}
MemTable::~MemTable() = default;

void MemTable::put_(const std::string &key, const std::string &value,
                    uint64_t tranc_id) {
  current_table->put(key, value, tranc_id);
}

void MemTable::put(const std::string &key, const std::string &value,
                   uint64_t tranc_id) {
  std::unique_lock<std::shared_mutex> lock1(cur_mtx);
  put_(key, value, tranc_id);
  if (current_table->get_size() > LSM_PER_MEM_SIZE_LIMIT) {
    // 冻结当前表还需要获取frozen_mtx的写锁
    std::unique_lock<std::shared_mutex> lock2(frozen_mtx);
    frozen_cur_table_();
  }
}

void MemTable::put_batch(
    const std::vector<std::pair<std::string, std::string>> &kvs,
    uint64_t tranc_id) {
  std::unique_lock<std::shared_mutex> lock1(cur_mtx);
  for (auto &[k, v] : kvs) {
    put_(k, v, tranc_id);
  }
  if (current_table->get_size() > LSM_PER_MEM_SIZE_LIMIT) {
    // 冻结当前表还需要获取frozen_mtx的写锁
    std::unique_lock<std::shared_mutex> lock2(frozen_mtx);
    frozen_cur_table_();
  }
}

SkipListIterator MemTable::cur_get_(const std::string &key, uint64_t tranc_id) {
  // 检查当前活跃的memtable
  auto result = current_table->get(key, tranc_id);
  if (result.is_valid()) {
    // 只要找到了 key, 不管 value 是否为空都返回
    return result;
  }

  // 没有找到，返回空
  return SkipListIterator{};
}

SkipListIterator MemTable::frozen_get_(const std::string &key,
                                       uint64_t tranc_id) {
  // 检查frozen memtable
  for (auto &tabe : frozen_tables) {
    auto result = tabe->get(key, tranc_id);
    if (result.is_valid()) {
      return result;
    }
  }

  // 都没有找到，返回空
  return SkipListIterator{};
}

SkipListIterator MemTable::get(const std::string &key, uint64_t tranc_id) {
  // 先获取当前活跃表的锁
  std::shared_lock<std::shared_mutex> slock1(cur_mtx);
  auto cur_res = cur_get_(key, tranc_id);
  if (cur_res.is_valid()) {
    return cur_res;
  }
  // 活跃表没有找到，再获取冻结表的锁
  slock1.unlock();
  std::shared_lock<std::shared_mutex> slock2(frozen_mtx);
  auto frozen_result = frozen_get_(key, tranc_id);
  if (frozen_result.is_valid()) {
    return frozen_result;
  }
  return SkipListIterator{};
}

SkipListIterator MemTable::get_(const std::string &key, uint64_t tranc_id) {

  auto cur_res = cur_get_(key, tranc_id);
  if (cur_res.is_valid()) {
    return cur_res;
  }

  auto frozen_result = frozen_get_(key, tranc_id);
  if (frozen_result.is_valid()) {
    return frozen_result;
  }
  return SkipListIterator{};
}

std::vector<std::pair<std::string, std::optional<std::pair<std::string, uint64_t>>>>
    MemTable::get_batch(const std::vector<std::string> &keys, uint64_t tranc_id) {
    std::vector<std::pair<std::string, std::optional<std::pair<std::string, uint64_t>>>> results;
    results.reserve(keys.size());

    // 1. 先获取活跃表的锁
    std::shared_lock<std::shared_mutex> slock1(cur_mtx);
    for (const auto &key : keys) {
        auto cur_res = cur_get_(key, tranc_id);
        if (cur_res.is_valid()) {
            if (cur_res.get_value().size() > 0) {
                // 值存在且不为空
                results.emplace_back(key, std::make_pair(cur_res.get_value(), cur_res.get_tranc_id()));
            } else {
                // 空值表示被删除
                results.emplace_back(key, std::nullopt);
            }
        } else {
            // 如果活跃表中未找到，标记为待查冻结表
            results.emplace_back(key, std::nullopt);
        }
    }

    // 2. 如果某些键在活跃表中未找到，获取冻结表的锁
    bool need_frozen_lookup = false;
    for (const auto &[key, value] : results) {
        if (!value.has_value()) {
            need_frozen_lookup = true;
            break;
        }
    }

    if (!need_frozen_lookup) { // 不需要查冻结表
        slock1.unlock();
        return results;
    }

    slock1.unlock(); // 释放活跃表的锁
    std::shared_lock<std::shared_mutex> slock2(frozen_mtx);
    for (auto &[key, value] : results) {
        if (value.has_value()) {
            continue; // 已在活跃表中找到，跳过
        }

        auto frozen_result = frozen_get_(key, tranc_id);
        if (frozen_result.is_valid()) {
            if (frozen_result.get_value().size() > 0) {
                // 值存在且不为空
                value = std::make_pair(frozen_result.get_value(), frozen_result.get_tranc_id());
            } else {
                // 空值表示被删除
                value = std::nullopt;
            }
        }
    }


    return results;
}

void MemTable::remove_(const std::string &key, uint64_t tranc_id) {
  // 删除的方式是写入空值
  current_table->put(key, "", tranc_id);
}

void MemTable::remove(const std::string &key, uint64_t tranc_id) {
  std::unique_lock<std::shared_mutex> lock(cur_mtx);
  remove_(key, tranc_id);
  if (current_table->get_size() > LSM_PER_MEM_SIZE_LIMIT) {
    // 冻结当前表还需要获取frozen_mtx的写锁
    std::unique_lock<std::shared_mutex> lock2(frozen_mtx);
    frozen_cur_table_();
  }
}

void MemTable::remove_batch(const std::vector<std::string> &keys,
                            uint64_t tranc_id) {
  std::unique_lock<std::shared_mutex> lock(cur_mtx);
  // 删除的方式是写入空值
  for (auto &key : keys) {
    remove_(key, tranc_id);
  }
  if (current_table->get_size() > LSM_PER_MEM_SIZE_LIMIT) {
    // 冻结当前表还需要获取frozen_mtx的写锁
    std::unique_lock<std::shared_mutex> lock2(frozen_mtx);
    frozen_cur_table_();
  }
}

void MemTable::clear() {
  std::unique_lock<std::shared_mutex> lock1(cur_mtx);
  std::unique_lock<std::shared_mutex> lock2(frozen_mtx);
  frozen_tables.clear();
  current_table->clear();
}

// 将最老的 memtable 写入 SST, 并返回控制类
std::shared_ptr<SST>
MemTable::flush_last(SSTBuilder &builder, std::string &sst_path, size_t sst_id,
                     std::shared_ptr<BlockCache> block_cache) {
  // 由于 flush 后需要移除最老的 memtable, 因此需要加写锁
  std::unique_lock<std::shared_mutex> lock(frozen_mtx);

  uint64_t max_tranc_id = 0;
  uint64_t min_tranc_id = UINT64_MAX;

  if (frozen_tables.empty()) {
    // 如果当前表为空，直接返回nullptr
    if (current_table->get_size() == 0) {
      return nullptr;
    }
    // 将当前表加入到frozen_tables头部
    frozen_tables.push_front(current_table);
    frozen_bytes += current_table->get_size();
    // 创建新的空表作为当前表
    current_table = std::make_shared<SkipList>();
  }

  // 将最老的 memtable 写入 SST
  std::shared_ptr<SkipList> table = frozen_tables.back();
  frozen_tables.pop_back();
  frozen_bytes -= table->get_size();

  std::vector<std::tuple<std::string, std::string, uint64_t>> flush_data =
      table->flush();
  for (auto &[k, v, t] : flush_data) {
    max_tranc_id = std::max(t, max_tranc_id);
    min_tranc_id = std::min(t, min_tranc_id);
    builder.add(k, v, t);
  }
  auto sst = builder.build(sst_id, sst_path, block_cache);

  return sst;
}

void MemTable::frozen_cur_table_() {
  frozen_bytes += current_table->get_size();
  frozen_tables.push_front(std::move(current_table));
  current_table = std::make_shared<SkipList>();
}

void MemTable::frozen_cur_table() {
  std::unique_lock<std::shared_mutex> lock1(cur_mtx);
  std::unique_lock<std::shared_mutex> lock2(frozen_mtx);
  frozen_cur_table_();
}

size_t MemTable::get_cur_size() {
  std::shared_lock<std::shared_mutex> slock(cur_mtx);
  return current_table->get_size();
}

size_t MemTable::get_frozen_size() {
  std::shared_lock<std::shared_mutex> slock(frozen_mtx);
  return frozen_bytes;
}

size_t MemTable::get_total_size() {
  std::shared_lock<std::shared_mutex> slock1(cur_mtx);
  std::shared_lock<std::shared_mutex> slock2(frozen_mtx);
  return get_frozen_size() + get_cur_size();
}

HeapIterator MemTable::begin(uint64_t tranc_id) {
  std::shared_lock<std::shared_mutex> slock1(cur_mtx);
  std::shared_lock<std::shared_mutex> slock2(frozen_mtx);
  std::vector<SearchItem> item_vec;

  for (auto iter = current_table->begin(); iter != current_table->end();
       ++iter) {
    if (tranc_id != 0 && iter.get_tranc_id() > tranc_id) {
      continue;
    }
    item_vec.emplace_back(iter.get_key(), iter.get_value(), 0, 0,
                          iter.get_tranc_id());
  }

  int table_idx = 1;
  for (auto ft = frozen_tables.begin(); ft != frozen_tables.end(); ft++) {
    auto table = *ft;
    for (auto iter = table->begin(); iter != table->end(); ++iter) {
      if (tranc_id != 0 && iter.get_tranc_id() > tranc_id) {
        continue;
      }
      item_vec.emplace_back(iter.get_key(), iter.get_value(), table_idx, 0,
                            iter.get_tranc_id());
    }
    table_idx++;
  }

  return HeapIterator(item_vec, tranc_id);
}

HeapIterator MemTable::end() {
  std::shared_lock<std::shared_mutex> slock1(cur_mtx);
  std::shared_lock<std::shared_mutex> slock2(frozen_mtx);
  return HeapIterator{};
}

HeapIterator MemTable::iters_preffix(const std::string &preffix,
                                     uint64_t tranc_id) {
  std::shared_lock<std::shared_mutex> slock1(cur_mtx);
  std::shared_lock<std::shared_mutex> slock2(frozen_mtx);
  std::vector<SearchItem> item_vec;

  for (auto iter = current_table->begin_preffix(preffix);
       iter != current_table->end_preffix(preffix); ++iter) {
    if (tranc_id != 0 && iter.get_tranc_id() > tranc_id) {
      // 如果开启了事务, 比当前事务 id 更大的记录是不可见的
      continue;
    }
    if (!item_vec.empty() && item_vec.back().key_ == iter.get_key()) {
      // 如果key相同，则只保留最新的事务修改的记录即可
      // 且这个记录既然已经存在于item_vec中，则其肯定满足了事务的可见性判断
      continue;
    }
    item_vec.emplace_back(iter.get_key(), iter.get_value(), 0, 0,
                          iter.get_tranc_id());
  }

  int table_idx = 1;
  for (auto ft = frozen_tables.begin(); ft != frozen_tables.end(); ft++) {
    auto table = *ft;
    for (auto iter = table->begin_preffix(preffix);
         iter != table->end_preffix(preffix); ++iter) {
      if (tranc_id != 0 && iter.get_tranc_id() > tranc_id) {
        // 如果开启了事务, 比当前事务 id 更大的记录是不可见的
        continue;
      }
      if (!item_vec.empty() && item_vec.back().key_ == iter.get_key()) {
        // 如果key相同，则只保留最新的事务修改的记录即可
        // 且这个记录既然已经存在于item_vec中，则其肯定满足了事务的可见性判断
        continue;
      }
      item_vec.emplace_back(iter.get_key(), iter.get_value(), table_idx, 0,
                            iter.get_tranc_id());
    }
    table_idx++;
  }

  return HeapIterator(item_vec, tranc_id);
}

std::optional<std::pair<HeapIterator, HeapIterator>>
MemTable::iters_monotony_predicate(
    uint64_t tranc_id, std::function<int(const std::string &)> predicate) {
  std::shared_lock<std::shared_mutex> slock1(cur_mtx);
  std::shared_lock<std::shared_mutex> slock2(frozen_mtx);

  std::vector<SearchItem> item_vec;

  auto cur_result = current_table->iters_monotony_predicate(predicate);
  if (cur_result.has_value()) {
    auto [begin, end] = cur_result.value();
    for (auto iter = begin; iter != end; ++iter) {
      if (tranc_id != 0 && iter.get_tranc_id() > tranc_id) {
        // 如果开启了事务, 比当前事务 id 更大的记录是不可见的
        continue;
      }
      if (!item_vec.empty() && item_vec.back().key_ == iter.get_key()) {
        // 如果key相同，则只保留最新的事务修改的记录即可
        // 且这个记录既然已经存在于item_vec中，则其肯定满足了事务的可见性判断
        continue;
      }
      item_vec.emplace_back(iter.get_key(), iter.get_value(), 0, 0,
                            iter.get_tranc_id());
    }
  }

  int table_idx = 1;
  for (auto ft = frozen_tables.begin(); ft != frozen_tables.end(); ft++) {
    auto table = *ft;
    auto result = table->iters_monotony_predicate(predicate);
    if (result.has_value()) {
      auto [begin, end] = result.value();
      for (auto iter = begin; iter != end; ++iter) {
        if (tranc_id != 0 && iter.get_tranc_id() > tranc_id) {
          // 如果开启了事务, 比当前事务 id 更大的记录是不可见的
          continue;
        }
        if (!item_vec.empty() && item_vec.back().key_ == iter.get_key()) {
          // 如果key相同，则只保留最新的事务修改的记录即可
          // 且这个记录既然已经存在于item_vec中，则其肯定满足了事务的可见性判断
          continue;
        }
        item_vec.emplace_back(iter.get_key(), iter.get_value(), table_idx, 0,
                              iter.get_tranc_id());
      }
    }
    table_idx++;
  }

  if (item_vec.empty()) {
    return std::nullopt;
  }
  return std::make_pair(HeapIterator(item_vec, tranc_id), HeapIterator{});
}
