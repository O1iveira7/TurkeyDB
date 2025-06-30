#include "engine.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "concact_iterator.h"
#include "consts.h"
#include "sst.h"
#include "sst_iterator.h"

// *********************** LSMEngine ***********************
// sst的文件路径格式为: data_dir/sst_{id}.level sst_id格式化为32位数字
LSMEngine::LSMEngine(std::string path) : data_dir(path) {
  // 初始化 block_cahce
  // TODO: Lab 4.2 引擎初始化
  using namespace std::filesystem;
  block_cache = std::make_shared<BlockCache>(LSMmm_BLOCK_CACHE_CAPACITY,
                                             LSMmm_BLOCK_CACHE_K);
  if (!exists(path)) {
    create_directory(path);
  } else {
    size_t max_sst_id = 0;
    size_t max_sst_level = 0;
    for (const auto &entry : directory_iterator(path)) {
      if (entry.path().filename().string().starts_with("sst_")) {
        auto curr_file = FileObj::open(entry.path().string(), false);
        auto file_name = entry.path().filename().stem().string();
        auto curr_sst_id = std::stoul(file_name.substr(4));

        auto curr_sst_level =
            std::stoul(entry.path().extension().string().substr(1));

        max_sst_id = std::max(max_sst_id, curr_sst_id);
        max_sst_level = std::max(max_sst_level, curr_sst_level);

        auto curr_sst =
            SST::open(curr_sst_id, std::move(curr_file), block_cache);

        ssts[curr_sst_id] = curr_sst;
        level_sst_ids[curr_sst_level].push_back(curr_sst_id);
      }
    }

    // todo 其它level的文件优先级可能也要处理
    for (int i = 0; i < level_sst_ids.size(); i++) {
      std::ranges::sort(level_sst_ids[i]);
      if (i == 0) {
        std::ranges::reverse(level_sst_ids[0]);
      }
    }
  }
}

LSMEngine::~LSMEngine() = default;

std::optional<std::pair<std::string, uint64_t>> LSMEngine::get(
    const std::string &key, uint64_t tranc_id) {
  // todo
  // 1. 先查找 memtable
  // 值存在且不为空（没有被删除）
  // memtable返回的kv的value为空值表示被删除了
  // 2. l0 sst中查询
  //  中的 sst_id 是按从大到小的顺序排列,
  // sst_id 越大, 表示是越晚刷入的, 优先查询
  // 值存在且不为空（没有被删除）
  // 空值表示被删除了
  // 3. 其他level的sst中查询
  // 二分查询
  // 如果sst_id在中, 则在sst中查询
  // 值存在且不为空（没有被删除）
  // 空值表示被删除了

  // 现在mem table中查找
  auto mem_ret = memtable.get(key, tranc_id);
  if (mem_ret.is_valid()) {
    auto kv = *mem_ret;
    auto curr_tranc_id = mem_ret.get_tranc_id();
    if (kv.second.empty()) {
      return std::nullopt;
    }
    return {{kv.second, curr_tranc_id}};
  }

  // sst
  auto res = sst_get_(key, tranc_id);
  return res;
}

std::vector<
    std::pair<std::string, std::optional<std::pair<std::string, uint64_t>>>>
LSMEngine::get_batch(const std::vector<std::string> &keys, uint64_t tranc_id) {
  // TODO: Lab 4.2 批量查询
  // 1. 先从 memtable 中批量查找
  // 2. 如果所有键都在memtable 中找到，直接返回
  // 2. 从 L0 层 SST 文件中批量查找未命中的键
  // 值存在且不为空
  // 空值表示被删除
  // 停止继续查找
  // 3. 从其他层级 SST 文件中批量查找未命中的键
  // 已找到，跳过
  // 二分查找确定键可能所在的 SST 文件
  // 如果键在当前 SST 文件范围内，则在 SST 中查找
  // 值存在且不为空
  // 空值表示被删除
  // 停止继续查找

  auto res = memtable.get_batch(keys, tranc_id);
  for (auto &curr : res) {
    if (curr.second.has_value()) continue;
    curr.second = sst_get_(curr.first, tranc_id);
  }
  return res;
}
// TODO: Lab 4.2 sst 内部查询
// 返回key对应的optional，<val,tranc>
std::optional<std::pair<std::string, uint64_t>> LSMEngine::sst_get_(
    const std::string &key, uint64_t tranc_id) {
  // 1. l0 sst中查询
  //  中的 sst_id 是按从大到小的顺序排列,
  // sst_id 越大, 表示是越晚刷入的, 优先查询
  // 值存在且不为空（没有被删除）
  // 空值表示被删除了
  // 2. 其他level的sst中查询
  // 二分查询
  // 如果sst_id在中, 则在sst中查询
  // 值存在且不为空（没有被删除）
  // 空值表示被删除了
  // 假设sst已经按照优先级拍好了序
  // start from level 0
  auto search_curr_level =
      [&](size_t i) -> std::optional<std::pair<std::string, uint64_t>> {
    const auto &curr_level_vec = level_sst_ids[i];
    for (const auto &curr_sst_id : curr_level_vec) {
      const auto &curr_sst = ssts[curr_sst_id];
      auto res = curr_sst->get(key, tranc_id);
      if (res.is_valid()) {
        auto val = res.value();
        if (val.size()) {
          return {{val, res.get_tranc_id()}};
        }
      }
      // todo.
      return std::nullopt;
    }
  };

  for (int i = 0; i < level_sst_ids.size(); i++) {
    auto res = search_curr_level(i);
    if (res.has_value()) return res;
  }
  return std::nullopt;
}

uint64_t LSMEngine::put(const std::string &key, const std::string &value,
                        uint64_t tranc_id) {
  // TODO: Lab 4.1 插入
  // ? 由于 put 操作可能触发 flush
  // ? 如果触发了 flush 则返回新刷盘的 sst 的 id
  // ? 在没有实现  flush 的情况下，你返回 0即可
  memtable.put(key, value, tranc_id);
  if (memtable.get_total_size() >= LSM_TOL_MEM_SIZE_LIMIT) {
    return flush();
  }
  return 0;  // todo ?
}

uint64_t LSMEngine::put_batch(
    const std::vector<std::pair<std::string, std::string>> &kvs,
    uint64_t tranc_id) {
  // TODO: Lab 4.1 批量插入
  // ? 由于 put 操作可能触发 flush
  // ? 如果触发了 flush 则返回新刷盘的 sst 的 id
  // ? 在没有实现  flush 的情况下，你返回 0即可
  memtable.put_batch(kvs, tranc_id);
  // todo 有没有可能kvs过去庞大，flush一个sst后剩余容量依然 >=
  // LSM_TOL_MEM_SIZE_LIMIT
  if (memtable.get_total_size() >= LSM_TOL_MEM_SIZE_LIMIT) {
    return flush();
  }
  return 0;  // todo ?
}
uint64_t LSMEngine::remove(const std::string &key, uint64_t tranc_id) {
  // TODO: Lab 4.1 删除
  // ? 在 LSM 中，删除实际上是插入一个空值
  // ? 由于 put 操作可能触发 flush
  // ? 如果触发了 flush 则返回新刷盘的 sst 的 id
  // ? 在没有实现  flush 的情况下，你返回 0即可
  memtable.remove(key, tranc_id);
  if (memtable.get_total_size() >= LSM_TOL_MEM_SIZE_LIMIT) {
    return flush();
  }
  return 0;
}

uint64_t LSMEngine::remove_batch(const std::vector<std::string> &keys,
                                 uint64_t tranc_id) {
  // TODO: Lab 4.1 批量删除
  // ? 在 LSM 中，删除实际上是插入一个空值
  // ? 由于 put 操作可能触发 flush
  // ? 如果触发了 flush 则返回新刷盘的 sst 的 id
  // ? 在没有实现  flush 的情况下，你返回 0即可
  memtable.remove_batch(keys, tranc_id);
  if (memtable.get_total_size() >= LSM_TOL_MEM_SIZE_LIMIT) {
    return flush();
  }
  return 0;
}

void LSMEngine::clear() {
  memtable.clear();
  level_sst_ids.clear();
  ssts.clear();
  // 清空当前文件夹的所有内容
  try {
    for (const auto &entry : std::filesystem::directory_iterator(data_dir)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      std::filesystem::remove(entry.path());
    }
  } catch (const std::filesystem::filesystem_error &e) {
    // 处理文件系统错误
    std::cerr << "Error clearing directory: " << e.what() << std::endl;
  }
}

// 按这个意思就是一个对于level 0来说一个skiplist上的数据对应一个sst喽?
uint64_t LSMEngine::flush() {
  // TODO: Lab 4.1 刷盘形成sst文件
  auto curr_sst_id = next_sst_id++;
  auto path = get_sst_path(curr_sst_id, 0);
  SSTBuilder builder(LSM_BLOCK_SIZE, true);

  std::unique_lock lock(ssts_mtx);
  auto flushed_sst =
      memtable.flush_last(builder, path, curr_sst_id, block_cache);
  level_sst_ids[0].push_front(curr_sst_id);
  ssts[curr_sst_id] = flushed_sst;

  // todo return what?
  return 0;
}

std::string LSMEngine::get_sst_path(size_t sst_id, size_t target_level) {
  // sst的文件路径格式为: data_dir/sst_<sst_id>，sst_id格式化为32位数字
  std::stringstream ss;
  ss << data_dir << "/sst_" << std::setfill('0') << std::setw(32) << sst_id
     << '.' << target_level;
  return ss.str();
}

std::optional<std::pair<TwoMergeIterator, TwoMergeIterator>>
LSMEngine::lsm_iters_monotony_predicate(
    uint64_t tranc_id, std::function<int(const std::string &)> predicate) {
  //  先从 memtable 中查询
  auto mem_result = memtable.iters_monotony_predicate(tranc_id, predicate);

  // 再从 sst 中查询
  std::vector<SearchItem> item_vec;
  for (auto &[sst_level, sst_ids] : level_sst_ids) {
    for (auto &sst_id : sst_ids) {
      auto sst = ssts[sst_id];
      auto result = sst_iters_monotony_predicate(sst, tranc_id, predicate);
      if (!result.has_value()) {
        continue;
      }
      auto [it_begin, it_end] = result.value();
      for (; it_begin != it_end && it_begin.is_valid(); ++it_begin) {
        // l0中, 这里越古老的sst的idx越小, 我们需要让新的sst优先在堆顶
        // 让新的sst(拥有更大的idx)排序在前面, 反转符号就行了
        if (tranc_id != 0 && it_begin.get_tranc_id() > tranc_id) {
          // 如果开启了事务, 比当前事务 id 更大的记录是不可见的
          continue;
        }
        if (!item_vec.empty() && item_vec.back().key_ == it_begin.key()) {
          // 如果key相同，则只保留最新的事务修改的记录即可
          // 且这个记录既然已经存在于item_vec中，则其肯定满足了事务的可见性判断
          continue;
        }
        item_vec.emplace_back(it_begin.key(), it_begin.value(), -sst_id,
                              sst_level, it_begin.get_tranc_id());
      }
    }
  }

  std::shared_ptr<HeapIterator> l0_iter_ptr =
      std::make_shared<HeapIterator>(item_vec, tranc_id);

  if (!mem_result.has_value() && item_vec.empty()) {
    return std::nullopt;
  }
  if (mem_result.has_value()) {
    auto [mem_start, mem_end] = mem_result.value();
    std::shared_ptr<HeapIterator> mem_start_ptr =
        std::make_shared<HeapIterator>();
    *mem_start_ptr = mem_start;
    auto start = TwoMergeIterator(mem_start_ptr, l0_iter_ptr, tranc_id);
    auto end = TwoMergeIterator{};
    return std::make_optional<std::pair<TwoMergeIterator, TwoMergeIterator>>(
        start, end);
  } else {
    auto start = TwoMergeIterator(std::make_shared<HeapIterator>(), l0_iter_ptr,
                                  tranc_id);
    auto end = TwoMergeIterator{};
    return std::make_optional<std::pair<TwoMergeIterator, TwoMergeIterator>>(
        start, end);
  }
}

TwoMergeIterator LSMEngine::begin(uint64_t tranc_id) {
  std::vector<SstIterator> iter_vec;
  std::shared_lock<std::shared_mutex> lock(ssts_mtx);  // 读锁
  for (auto &sst_id : level_sst_ids[0]) {
    auto sst = ssts[sst_id];
    for (auto iter = sst->begin(tranc_id); iter != sst->end(); ++iter) {
      // 这里越新的sst的idx越大, 我们需要让新的sst优先在堆顶
      // 让新的sst(拥有更大的idx)排序在前面, 反转符号就行了
      iter_vec.push_back(iter);
    }
  }

  std::shared_ptr<HeapIterator> mem_iter_ptr = std::make_shared<HeapIterator>();
  *mem_iter_ptr = memtable.begin(tranc_id);

  std::shared_ptr<HeapIterator> l0_iter_ptr = std::make_shared<HeapIterator>();
  *l0_iter_ptr = SstIterator::merge_sst_iterator(iter_vec, tranc_id).first;

  return TwoMergeIterator(mem_iter_ptr, l0_iter_ptr, tranc_id);
}

TwoMergeIterator LSMEngine::end() { return TwoMergeIterator{}; }

void LSMEngine::full_compact(size_t src_level) {
  // 将 src_level 的 sst 全体压缩到 src_level + 1

  // 递归地判断下一级 level 是否需要 full compact
  if (level_sst_ids[src_level + 1].size() >= LSM_SST_LEVEL_RATIO) {
    full_compact(src_level + 1);
  }

  // 获取源level和目标level的 sst_id
  auto old_level_id_x = level_sst_ids[src_level];
  auto old_level_id_y = level_sst_ids[src_level + 1];
  std::vector<std::shared_ptr<SST>> new_ssts;
  std::vector<size_t> lx_ids(old_level_id_x.begin(), old_level_id_x.end());
  std::vector<size_t> ly_ids(old_level_id_y.begin(), old_level_id_y.end());
  if (src_level == 0) {
    // l0这一层不同sst的key有重叠, 需要额外处理
    new_ssts = full_l0_l1_compact(lx_ids, ly_ids);
  } else {
    new_ssts = full_common_compact(lx_ids, ly_ids, src_level + 1);
  }
  // 完成 compact 后移除旧的sst记录
  for (auto &old_sst_id : old_level_id_x) {
    ssts[old_sst_id]->del_sst();
    ssts.erase(old_sst_id);
  }
  for (auto &old_sst_id : old_level_id_y) {
    ssts[old_sst_id]->del_sst();
    ssts.erase(old_sst_id);
  }
  level_sst_ids[src_level].clear();
  level_sst_ids[src_level + 1].clear();

  cur_max_level = std::max(cur_max_level, src_level + 1);

  // 添加新的sst
  for (auto &new_sst : new_ssts) {
    level_sst_ids[src_level + 1].push_back(new_sst->get_sst_id());
    ssts[new_sst->get_sst_id()] = new_sst;
  }
  // 此处没必要reverse了
  std::sort(level_sst_ids[src_level + 1].begin(),
            level_sst_ids[src_level + 1].end());
}

std::vector<std::shared_ptr<SST>> LSMEngine::full_l0_l1_compact(
    std::vector<size_t> &l0_ids, std::vector<size_t> &l1_ids) {
  // TODO: 这里需要补全的是对已经完成事务的删除
  std::vector<SstIterator> l0_iters;
  std::vector<std::shared_ptr<SST>> l1_ssts;

  for (auto id : l0_ids) {
    auto sst_it = ssts[id]->begin(0);
    l0_iters.push_back(sst_it);
  }
  for (auto id : l1_ids) {
    l1_ssts.push_back(ssts[id]);
  }
  // l0 的sst之间的key有重叠, 需要合并
  auto [l0_begin, l0_end] = SstIterator::merge_sst_iterator(l0_iters, 0);

  std::shared_ptr<HeapIterator> l0_begin_ptr = std::make_shared<HeapIterator>();
  *l0_begin_ptr = l0_begin;

  std::shared_ptr<ConcactIterator> old_l1_begin_ptr =
      std::make_shared<ConcactIterator>(l1_ssts, 0);

  TwoMergeIterator l0_l1_begin(l0_begin_ptr, old_l1_begin_ptr, 0);

  return gen_sst_from_iter(l0_l1_begin,
                           LSM_PER_MEM_SIZE_LIMIT * LSM_SST_LEVEL_RATIO, 1);
}

std::vector<std::shared_ptr<SST>> LSMEngine::full_common_compact(
    std::vector<size_t> &lx_ids, std::vector<size_t> &ly_ids, size_t level_y) {
  // TODO 需要补全已完成事务的滤除
  std::vector<std::shared_ptr<SST>> lx_iters;
  std::vector<std::shared_ptr<SST>> ly_iters;

  for (auto id : lx_ids) {
    lx_iters.push_back(ssts[id]);
  }
  for (auto id : ly_ids) {
    ly_iters.push_back(ssts[id]);
  }

  std::shared_ptr<ConcactIterator> old_lx_begin_ptr =
      std::make_shared<ConcactIterator>(lx_iters, 0);

  std::shared_ptr<ConcactIterator> old_ly_begin_ptr =
      std::make_shared<ConcactIterator>(ly_iters, 0);

  TwoMergeIterator lx_ly_begin(old_lx_begin_ptr, old_ly_begin_ptr, 0);

  // TODO:如果目标 level 的下一级 level+1 不存在, 则为底层的level,
  // 可以清理掉删除标记

  return gen_sst_from_iter(lx_ly_begin, LSMEngine::get_sst_size(level_y),
                           level_y);
}

std::vector<std::shared_ptr<SST>> LSMEngine::gen_sst_from_iter(
    BaseIterator &iter, size_t target_sst_size, size_t target_level) {
  // TODO: 这里需要补全的是对已经完成事务的删除
  std::vector<std::shared_ptr<SST>> new_ssts;
  auto new_sst_builder = SSTBuilder(LSM_BLOCK_SIZE, true);
  while (iter.is_valid() && !iter.is_end()) {
    new_sst_builder.add((*iter).first, (*iter).second, 0);
    ++iter;

    if (new_sst_builder.estimated_size() >= target_sst_size) {
      size_t sst_id = next_sst_id++;  // TODO: 后续优化并发性
      std::string sst_path = get_sst_path(sst_id, target_level);
      auto new_sst = new_sst_builder.build(sst_id, sst_path, this->block_cache);
      new_ssts.push_back(new_sst);
      new_sst_builder = SSTBuilder(LSM_BLOCK_SIZE, true);  // 重置builder
    }
  }
  if (new_sst_builder.estimated_size() > 0) {
    size_t sst_id = next_sst_id++;  // TODO: 后续优化并发性
    std::string sst_path = get_sst_path(sst_id, target_level);
    auto new_sst = new_sst_builder.build(sst_id, sst_path, this->block_cache);
    new_ssts.push_back(new_sst);
  }

  return new_ssts;
}

size_t LSMEngine::get_sst_size(size_t level) {
  if (level == 0) {
    return LSM_PER_MEM_SIZE_LIMIT;
  } else {
    return LSM_PER_MEM_SIZE_LIMIT *
           static_cast<size_t>(std::pow(LSM_SST_LEVEL_RATIO, level));
  }
}

// *********************** LSM ***********************
LSM::LSM(std::string path)
    : engine(std::make_shared<LSMEngine>(path)),
      tran_manager_(std::make_shared<TranManager>(path)) {
  tran_manager_->set_engine(engine);
  auto check_recover_res = tran_manager_->check_recover();
  for (auto &[tranc_id, records] : check_recover_res) {
    tran_manager_->update_max_finished_tranc_id(tranc_id);
    for (auto &record : records) {
      if (record.getOperationType() == OperationType::PUT) {
        engine->put(record.getKey(), record.getValue(), tranc_id);
      } else if (record.getOperationType() == OperationType::DELETE) {
        engine->remove(record.getKey(), tranc_id);
      }
    }
  }
  tran_manager_->init_new_wal();
}

LSM::~LSM() {
  flush_all();
  tran_manager_->write_tranc_id_file();
}

std::optional<std::string> LSM::get(const std::string &key) {
  auto tranc_id = tran_manager_->getNextTransactionId();
  auto res = engine->get(key, tranc_id);

  if (res.has_value()) {
    return res.value().first;
  }
  return std::nullopt;
}

std::vector<std::pair<std::string, std::optional<std::string>>> LSM::get_batch(
    const std::vector<std::string> &keys) {
  // 1. 获取事务ID
  auto tranc_id = tran_manager_->getNextTransactionId();

  // 2. 调用 engine 的批量查询接口
  auto batch_results = engine->get_batch(keys, tranc_id);

  // 3. 构造最终结果
  std::vector<std::pair<std::string, std::optional<std::string>>> results;
  for (const auto &[key, value] : batch_results) {
    if (value.has_value()) {
      results.emplace_back(key, value->first);  // 提取值部分
    } else {
      results.emplace_back(key, std::nullopt);  // 键不存在
    }
  }

  return results;
}

void LSM::put(const std::string &key, const std::string &value) {
  auto tranc_id = tran_manager_->getNextTransactionId();
  engine->put(key, value, tranc_id);
}

void LSM::put_batch(
    const std::vector<std::pair<std::string, std::string>> &kvs) {
  auto tranc_id = tran_manager_->getNextTransactionId();
  engine->put_batch(kvs, tranc_id);
}
void LSM::remove(const std::string &key) {
  auto tranc_id = tran_manager_->getNextTransactionId();
  engine->remove(key, tranc_id);
}

void LSM::remove_batch(const std::vector<std::string> &keys) {
  auto tranc_id = tran_manager_->getNextTransactionId();
  engine->remove_batch(keys, tranc_id);
}

void LSM::clear() { engine->clear(); }

void LSM::flush() { auto max_tranc_id = engine->flush(); }

void LSM::flush_all() {
  while (engine->memtable.get_total_size() > 0) {
    auto max_tranc_id = engine->flush();
    tran_manager_->update_max_flushed_tranc_id(max_tranc_id);
  }
}

LSM::LSMIterator LSM::begin(uint64_t tranc_id) {
  return engine->begin(tranc_id);
}

LSM::LSMIterator LSM::end() { return engine->end(); }

std::optional<std::pair<TwoMergeIterator, TwoMergeIterator>>
LSM::lsm_iters_monotony_predicate(
    uint64_t tranc_id, std::function<int(const std::string &)> predicate) {
  return engine->lsm_iters_monotony_predicate(tranc_id, predicate);
}

// 开启一个事务
std::shared_ptr<TranContext> LSM::begin_tran() {
  auto tranc_context = tran_manager_->new_tranc();
  return tranc_context;
}