#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "compact.h"
#include "transaction.h"
#include "two_merge_iterator.h"
#include "memtable.h"
#include "sst.h"

class LSMEngine {
public:
  std::string data_dir;
  // memtable自己实现了并发控制
  MemTable memtable;
  // level -> 当前level内的sst id
  std::unordered_map<size_t, std::deque<size_t>> level_sst_ids;
  // sst id -> sst meta
  std::unordered_map<size_t, std::shared_ptr<SST>> ssts;
  // lock while read/write sst file
  std::shared_mutex ssts_mtx;
  std::shared_ptr<BlockCache> block_cache;
  size_t next_sst_id = 0;
  size_t cur_max_level = 0;

public:
  LSMEngine(std::string path);
  ~LSMEngine();

  std::optional<std::pair<std::string, uint64_t>> get(const std::string &key,
                                                      uint64_t tranc_id);
  std::vector<std::pair<std::string, std::optional<std::pair<std::string, uint64_t>>>>
                                                  get_batch(const std::vector<std::string> &keys, uint64_t tranc_id);

  std::optional<std::pair<std::string, uint64_t>>
  sst_get_(const std::string &key, uint64_t tranc_id);

  // 如果触发了刷盘, 返回当前刷入sst的最大事务id
  uint64_t put(const std::string &key, const std::string &value,
               uint64_t tranc_id);

  uint64_t
  put_batch(const std::vector<std::pair<std::string, std::string>> &kvs,
            uint64_t tranc_id);

  uint64_t remove(const std::string &key, uint64_t tranc_id);
  uint64_t remove_batch(const std::vector<std::string> &keys,
                        uint64_t tranc_id);
  void clear();
  uint64_t flush();

  std::string get_sst_path(size_t sst_id, size_t target_level);

  std::optional<std::pair<TwoMergeIterator, TwoMergeIterator>>
  lsm_iters_monotony_predicate(
      uint64_t tranc_id, std::function<int(const std::string &)> predicate);

  TwoMergeIterator begin(uint64_t tranc_id);
  TwoMergeIterator end();

  static size_t get_sst_size(size_t level);

private:
  void full_compact(size_t src_level);
  std::vector<std::shared_ptr<SST>>
  full_l0_l1_compact(std::vector<size_t> &l0_ids, std::vector<size_t> &l1_ids);

  std::vector<std::shared_ptr<SST>>
  full_common_compact(std::vector<size_t> &lx_ids, std::vector<size_t> &ly_ids,
                      size_t level_y);

  std::vector<std::shared_ptr<SST>> gen_sst_from_iter(BaseIterator &iter,
                                                      size_t target_sst_size,
                                                      size_t target_level);
};

class LSM {
private:
  std::shared_ptr<LSMEngine> engine;
  std::shared_ptr<TranManager> tran_manager_;

public:
  LSM(std::string path);
  ~LSM();

  std::optional<std::string> get(const std::string &key);
  std::vector<std::pair<std::string, std::optional<std::string>>>
                             get_batch(const std::vector<std::string> &keys);

  void put(const std::string &key, const std::string &value);
  void put_batch(const std::vector<std::pair<std::string, std::string>> &kvs);

  void remove(const std::string &key);
  void remove_batch(const std::vector<std::string> &keys);

  using LSMIterator = TwoMergeIterator;
  LSMIterator begin(uint64_t tranc_id);
  LSMIterator end();
  std::optional<std::pair<TwoMergeIterator, TwoMergeIterator>>
  lsm_iters_monotony_predicate(
      uint64_t tranc_id, std::function<int(const std::string &)> predicate);
  void clear();
  void flush();
  void flush_all();

  // 开启一个事务
  std::shared_ptr<TranContext> begin_tran();
};