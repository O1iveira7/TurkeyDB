#include "sst_iterator.h"

#include <cstddef>
#include <optional>
#include <stdexcept>

#include "sst.h"
// todo
// predicate返回值:
//   0: 谓词
//   >0: 不满足谓词, 需要向右移动
//   <0: 不满足谓词, 需要向左移动

std::optional<std::pair<SstIterator, SstIterator>> sst_iters_monotony_predicate(
    std::shared_ptr<SST> sst, uint64_t tranc_id,
    std::function<int(const std::string &)> predicate) {}

SstIterator::SstIterator(std::shared_ptr<SST> sst, uint64_t tranc_id)
    : m_sst(sst), m_block_idx(0), m_block_it(nullptr), max_tranc_id_(tranc_id) {
  if (m_sst) {
    seek_first();
  }
}

SstIterator::SstIterator(std::shared_ptr<SST> sst, const std::string &key,
                         uint64_t tranc_id)
    : m_sst(sst), m_block_idx(0), m_block_it(nullptr), max_tranc_id_(tranc_id) {
  if (m_sst) {
    seek(key);
  }
}

void SstIterator::set_block_idx(size_t idx) { m_block_idx = idx; }
void SstIterator::set_block_it(std::shared_ptr<BlockIterator> it) {
  m_block_it = it;
}

// TODO: Lab 3.6 将迭代器定位到第一个key
void SstIterator::seek_first() {
  m_block_idx = 0;
  auto first_block = m_sst->read_block(m_block_idx);
  m_block_it = std::make_shared<BlockIterator>(first_block->begin());
  if (!cached_value.has_value()) {
    cached_value = m_block_it->operator*();
  }
}

// Hint 这里的逻辑也很简单,
// 就是先使用记录在sst中的meta_entries找到包含要查找的key的Block(find_block_idx),
// 从文件中读取这个Block(read_block),
// 然后再读取的Block中调用获取指定key的迭代器的构造函数,
// 通过BlockIterator实现在Block中的定位。l
void SstIterator::seek(const std::string &key) {
  m_block_idx = m_sst->find_block_idx(key);
  if (m_block_idx == -1)
    throw std::runtime_error(
        "SstIterator::seek can not find this key in blocks");
  auto block = m_sst->read_block(m_block_idx);
  m_block_it = std::make_shared<BlockIterator>(BlockIterator(block, key, 0));
  if (!cached_value.has_value()) {
    cached_value = m_block_it->operator*();
  }
}

std::string SstIterator::key() {
  if (!m_block_it) {
    throw std::runtime_error("Iterator is invalid");
  }
  return (*m_block_it)->first;
}

std::string SstIterator::value() {
  if (!m_block_it) {
    throw std::runtime_error("Iterator is invalid");
  }
  return (*m_block_it)->second;
}

// todo
// 如果当前block的最后应该直接跳到下一个block
BaseIterator &SstIterator::operator++() {
  m_block_it->operator++();
  if (m_block_it->is_end()) {
    ++m_block_idx;
    if (m_block_idx >= m_sst->num_blocks()) {
      cached_value.reset();
    } else {
      auto block = m_sst->read_block(m_block_idx);
      m_block_it = std::make_shared<BlockIterator>(block, 0, max_tranc_id_);
      m_block_idx = 0;
      cached_value = m_block_it->operator*();
    }
  }
  return *this;
}

bool SstIterator::operator==(const BaseIterator &other) const {
  if (other.get_type() != IteratorType::SstIterator) return false;
  auto rhs = dynamic_cast<const SstIterator *>(&other);
  if (rhs == nullptr) return false;
  if (m_sst == nullptr || m_sst != nullptr ||
      m_sst == nullptr && rhs->m_sst == nullptr)
    return false;
  if (m_sst->get_sst_id() != rhs->m_sst->get_sst_id()) return false;
  return *m_block_it == *rhs->m_block_it;
}

bool SstIterator::operator!=(const BaseIterator &other) const {
  return !(*this == other);
}

SstIterator::value_type SstIterator::operator*() const {}

IteratorType SstIterator::get_type() const { return IteratorType::SstIterator; }

uint64_t SstIterator::get_tranc_id() const { return max_tranc_id_; }
bool SstIterator::is_end() const { return !m_block_it; }

bool SstIterator::is_valid() const {
  return m_block_it && !m_block_it->is_end() &&
         m_block_idx < m_sst->num_blocks();
}
SstIterator::pointer SstIterator::operator->() const {
  update_current();
  return &(*cached_value);
}

void SstIterator::update_current() const {
  if (!cached_value && m_block_it && !m_block_it->is_end()) {
    cached_value = *(*m_block_it);
  }
}

std::pair<HeapIterator, HeapIterator> SstIterator::merge_sst_iterator(
    std::vector<SstIterator> iter_vec, uint64_t tranc_id) {
  if (iter_vec.empty()) {
    return std::make_pair(HeapIterator(), HeapIterator());
  }

  HeapIterator it_begin;
  for (auto &iter : iter_vec) {
    while (iter.is_valid() && !iter.is_end()) {
      it_begin.items.emplace(
          iter.key(), iter.value(), -iter.m_sst->get_sst_id(), 0,
          tranc_id);  // ! 此处的level暂时没有作用, 都作用于同一层的比较
      ++iter;
    }
  }
  return std::make_pair(it_begin, HeapIterator());
}
