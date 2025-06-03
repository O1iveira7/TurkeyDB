#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>

enum class IteratorType {
  SkipListIterator,
  MemTableIterator,
  SstIterator,
  HeapIterator,
  TwoMergeIterator,
  ConcactIterator,
  LevelIterator,
};

class BaseIterator {
 public:
  // why pair<str,str>??==》k/v
  using value_type = std::pair<std::string, std::string>;
  using pointer = value_type *;
  using reference = value_type &;

  virtual BaseIterator &operator++() = 0;
  virtual bool operator==(const BaseIterator &other) const = 0;
  virtual bool operator!=(const BaseIterator &other) const = 0;
  virtual value_type operator*() const = 0;
  virtual IteratorType get_type() const = 0;
  virtual uint64_t get_tranc_id() const = 0;
  virtual bool is_end() const = 0;
  virtual bool is_valid() const = 0;
};

class SstIterator;
// 排序规则
// 1. 先看key升序
// 2. 看tranc_id_降序
// 3， 看idx_降序
// 假设value不为空，为空就意味着当前key被删除了
struct SearchItem {
  std::string key_;
  std::string value_;
  uint64_t tranc_id_;
  int idx_; // 表明skiplist的新旧
  int level_;  // 来自sst的level

  SearchItem() = default;
  SearchItem(std::string k, std::string v, int i, int l, uint64_t tranc_id)
      : key_(std::move(k)),
        value_(std::move(v)),
        idx_(i),
        level_(l),
        tranc_id_(tranc_id) {}
};

bool operator<(const SearchItem &a, const SearchItem &b);
bool operator>(const SearchItem &a, const SearchItem &b);
bool operator==(const SearchItem &a, const SearchItem &b);

class HeapIterator : public BaseIterator {
  friend class SstIterator;

 public:
  HeapIterator() = default;
  HeapIterator(std::vector<SearchItem> item_vec, uint64_t max_tranc_id);
  pointer operator->() const;
  virtual value_type operator*() const override;
  BaseIterator &operator++() override;
  virtual bool operator==(const BaseIterator &other) const override;
  virtual bool operator!=(const BaseIterator &other) const override;

  virtual IteratorType get_type() const override;
  virtual uint64_t get_tranc_id() const override;
  virtual bool is_end() const override;
  virtual bool is_valid() const override;

 private:
  bool top_value_legal() const;

  // 跳过当前不可见事务的id (如果开启了事务功能)
  void skip_by_tranc_id();

  void update_current() const;

 private:
  std::priority_queue<SearchItem, std::vector<SearchItem>,
                      std::greater<SearchItem>>
      items;
  mutable std::shared_ptr<value_type> current;  // 用于存储当前元素 k/v pair
  uint64_t max_tranc_id_ = 0;
};