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

#include "consts.h"
#include "iterator.h"
#include "skiplist.h"
#include "sst.h"

class BlockCache;

// MemTable implementation using PIMPL idiom
MemTable::MemTable() : frozen_bytes(0) {
  current_table = std::make_shared<SkipList>(nxt_idx_);
  nxt_idx_++;
}
MemTable::~MemTable() = default;

void MemTable::put_(const std::string &key, const std::string &value,
                    uint64_t tranc_id) {
  current_table->put(key, value, tranc_id);
}

void MemTable::put(const std::string &key, const std::string &value,
                   uint64_t tranc_id) {
  std::unique_lock lock1(cur_mtx);
  put_(key, value, tranc_id);
  if (current_table->get_size() >= LSM_PER_MEM_SIZE_LIMIT) {
    std::unique_lock lock2(frozen_mtx);
    frozen_cur_table_();
  }
}

void MemTable::put_batch(
    const std::vector<std::pair<std::string, std::string>> &kvs,
    uint64_t tranc_id) {
  std::unique_lock lock1(cur_mtx);
  for (const auto &[k, v] : kvs) {
    put_(k, v, tranc_id);
    if (current_table->get_size() >= LSM_PER_MEM_SIZE_LIMIT) {
      std::unique_lock lock2(frozen_mtx);
      frozen_cur_table_();
    }
  }
}

SkipListIterator MemTable::cur_get_(const std::string &key, uint64_t tranc_id) {
  auto res = current_table->get(key, tranc_id);
  if (res.is_valid()) return res;
  return {};
}

SkipListIterator MemTable::frozen_get_(const std::string &key,
                                       uint64_t tranc_id) {
  // 新的跳表排在前面
  for (const auto &curr : frozen_tables) {
    auto res = curr->get(key, tranc_id);
    if (res.is_valid()) return res;
  }
  return {};
}

SkipListIterator MemTable::get_(const std::string &key, uint64_t tranc_id) {
  auto cur_res = cur_get_(key, tranc_id);
  if (cur_res.is_valid()) return cur_res;
  auto froz_res = frozen_get_(key, tranc_id);
  if (froz_res.is_valid()) return froz_res;
  return {};
}

SkipListIterator MemTable::get(const std::string &key, uint64_t tranc_id) {
  {
    std::shared_lock lock(cur_mtx);
    auto cur_res = cur_get_(key, tranc_id);
    if (cur_res.is_valid()) return cur_res;
  }
  {
    std::shared_lock lock(frozen_mtx);
    auto froz_res = frozen_get_(key, tranc_id);
    if (froz_res.is_valid()) return froz_res;
  }
  return {};
}

std::vector<
    std::pair<std::string, std::optional<std::pair<std::string, uint64_t>>>>
MemTable::get_batch(const std::vector<std::string> &keys, uint64_t tranc_id) {
  std::vector<
      std::pair<std::string, std::optional<std::pair<std::string, uint64_t>>>>
      results; // result 是个vector...类型声明有一点吓人...
  results.reserve(keys.size());
  // std::optional<std::pair<std::string, uint64_t> 指的是val可能不存在，存在的话就是val，tranc id
  // 1. 先获取活跃表的锁
  std::shared_lock<std::shared_mutex> slock1(cur_mtx);
  for (const auto &key : keys) {
    auto cur_res = cur_get_(key, tranc_id);
    if (cur_res.is_valid()) {
      if (cur_res.get_value().size() > 0) {
        // 值存在且不为空
        results.emplace_back(
            key, std::make_pair(cur_res.get_value(), cur_res.get_tranc_id()));
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

  if (!need_frozen_lookup) {  // 不需要查冻结表
    slock1.unlock();
    return results;
  }

  slock1.unlock();  // 释放活跃表的锁
  std::shared_lock<std::shared_mutex> slock2(frozen_mtx);
  for (auto &[key, value] : results) {
    if (value.has_value()) {
      continue;  // 已在活跃表中找到，跳过
    }

    auto frozen_result = frozen_get_(key, tranc_id);
    if (frozen_result.is_valid()) {
      if (frozen_result.get_value().size() > 0) {
        // 值存在且不为空
        value = std::make_pair(frozen_result.get_value(),
                               frozen_result.get_tranc_id());
      } else {
        // 空值表示被删除
        value = std::nullopt;
      }
    }
  }

  return results;
}

void MemTable::remove_(const std::string &key, uint64_t tranc_id) {
  current_table->put(key, "", tranc_id);
}

void MemTable::remove(const std::string &key, uint64_t tranc_id) {
  std::unique_lock lock1(cur_mtx);
  remove_(key, tranc_id);
  if (current_table->get_size() >= LSM_PER_MEM_SIZE_LIMIT) {
    std::unique_lock lock2(frozen_mtx);
    frozen_cur_table_();
  }
}

void MemTable::remove_batch(const std::vector<std::string> &keys,
                            uint64_t tranc_id) {
  std::unique_lock lock1(cur_mtx);
  for (const auto &k : keys) {
    remove_(k, tranc_id);
    if (current_table->get_size() >= LSM_PER_MEM_SIZE_LIMIT) {
      std::unique_lock lock2(frozen_mtx);
      frozen_cur_table_();
    }
  }

}

void MemTable::clear() {
  std::unique_lock<std::shared_mutex> lock1(cur_mtx);
  std::unique_lock<std::shared_mutex> lock2(frozen_mtx);
  frozen_tables.clear();
  current_table->clear();
}

// 将最老的 memtable 写入磁盘生成新的SST，并返回SST控制类
std::shared_ptr<SST> MemTable::flush_last(
    SSTBuilder &builder, std::string &sst_path, size_t sst_id,
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
    current_table = std::make_shared<SkipList>(nxt_idx_);
    nxt_idx_++;
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
  // 将sst写入文件并返回SST描述类
  auto sst = builder.build(sst_id, sst_path, block_cache);

  return sst;
}

void MemTable::frozen_cur_table_() {
  frozen_bytes += current_table->get_size();
  frozen_tables.push_front(current_table);
  current_table = std::make_shared<SkipList>(nxt_idx_);
  nxt_idx_++;
}

void MemTable::frozen_cur_table() {
  std::unique_lock lock1(cur_mtx);
  std::unique_lock lock2(frozen_mtx);
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

// todo:这边的level是啥
HeapIterator MemTable::begin(uint64_t tranc_id) {
  // 用所有kv对构造heap iterator
  std::shared_lock slock1(cur_mtx);
  std::shared_lock slock2(frozen_mtx);
  std::vector<SearchItem> search_items_vec;

  auto beg = current_table->begin();
  while (beg != current_table->end()) {
    search_items_vec.emplace_back(beg.get_key(), beg.get_value(),
                                  current_table->get_idx(), 0, tranc_id);
    ++beg;
  }
  for (const auto &curr : frozen_tables) {
    auto curr_beg = curr->begin();
    while (curr_beg != curr->end()) {
      search_items_vec.emplace_back(curr_beg.get_key(), curr_beg.get_value(),
                                    curr->get_idx(), 0, tranc_id);
      ++curr_beg;
    }
  }
  return {search_items_vec, tranc_id};
}

HeapIterator MemTable::end() {
  std::shared_lock slock1(cur_mtx);
  std::shared_lock slock2(frozen_mtx);
  return {};
}

HeapIterator MemTable::iters_preffix(const std::string &preffix,
                                     uint64_t tranc_id) {
  std::shared_lock slock1(cur_mtx);
  std::shared_lock slock2(frozen_mtx);
  std::vector<SearchItem> search_items_vec;

  auto cur_l_beg = current_table->begin_preffix(preffix);
  auto cur_l_end = current_table->end_preffix(preffix);
  while (cur_l_beg != cur_l_end) {
    search_items_vec.emplace_back(cur_l_beg.get_key(), cur_l_beg.get_value(),
                                  current_table->get_idx(), 1, tranc_id);
    ++cur_l_beg;
  }

  for (const auto &frozen : frozen_tables) {
    auto frozen_beg = frozen->begin_preffix(preffix);
    auto frozen_end = frozen->end_preffix(preffix);
    while (frozen_beg != frozen_end) {
      search_items_vec.emplace_back(frozen_beg.get_key(),
                                    frozen_beg.get_value(), frozen->get_idx(),
                                    1, tranc_id);
      ++frozen_beg;
    }
  }
  // heap iterator 会去重
  return {search_items_vec, tranc_id};
}

std::optional<std::pair<HeapIterator, HeapIterator>>
MemTable::iters_monotony_predicate(
    uint64_t tranc_id, std::function<int(const std::string &)> predicate) {
  std::shared_lock slock1(cur_mtx);
  std::shared_lock slock2(frozen_mtx);
  std::vector<SearchItem> search_items_vec;
  auto opt_curr_range = current_table->iters_monotony_predicate(predicate);
  if (opt_curr_range.has_value()) {
    auto [pred_beg, pred_end] = opt_curr_range.value();
    while (pred_beg != pred_end) {
      search_items_vec.emplace_back(pred_beg.get_key(), pred_beg.get_value(),
                                    current_table->get_idx(), 1, tranc_id);
      ++pred_beg;
    }
  }

  for (const auto &frozen : frozen_tables) {
    auto frozen_curr_range = frozen->iters_monotony_predicate(predicate);
    if (frozen_curr_range.has_value()) {
      auto [frozen_beg, frozen_end] =frozen_curr_range.value();
      while (frozen_beg != frozen_end) {
        search_items_vec.emplace_back(frozen_beg.get_key(),
                                      frozen_beg.get_value(), frozen->get_idx(),
                                      1, tranc_id);
        ++frozen_beg;
      }
    }
  }
  if (search_items_vec.empty()) return {};
  return {
      std::make_pair(HeapIterator{search_items_vec, tranc_id}, HeapIterator{})};
}
