#include "iterator.h"

#include <tuple>
#include <vector>
#include <algorithm>
// *************************** SearchItem ***************************
// todo 这边tranc_id的语义可能有问题，看后续测试
bool operator<(const SearchItem &a, const SearchItem &b) {
  if (a.key_ != b.key_) {
    return a.key_ < b.key_;
  }
  // tranc_id 和 level同理，值越大越新，更应该排在前面
  if (a.tranc_id_ != b.tranc_id_) {
    return a.tranc_id_ > b.tranc_id_;
  }

    return a.idx_ > b.idx_;
}

bool operator>(const SearchItem &a, const SearchItem &b) {
  if (a.key_ != b.key_) {
    return a.key_ > b.key_;
  }
  if (a.tranc_id_ != b.tranc_id_) {
    return a.tranc_id_ < b.tranc_id_;
  }
  return a.idx_ < b.idx_;
}

bool operator==(const SearchItem &a, const SearchItem &b) {
  return a.key_ == b.key_ && a.idx_ == b.idx_ && a.tranc_id_ == b.tranc_id_;
}

// *************************** HeapIterator ***************************
HeapIterator::HeapIterator(std::vector<SearchItem> item_vec,
                           uint64_t max_tranc_id)
    : max_tranc_id_(max_tranc_id) {
  // 有点浪费时间了，多排序了一次，堆本身会排序的
  // if (item_vec.empty()) return;
  // std::sort(item_vec.begin(),item_vec.end());
  // auto beg = item_vec.begin();
  // while (beg != item_vec.end()) {
  //   const auto& curr = *beg;
  //   if (curr.tranc_id_ > max_tranc_id_) {
  //     ++beg;
  //     continue;
  //   }
  //   while (beg != item_vec.end() && curr.key_ == beg->key_) {
  //     ++beg;
  //   }
  //   if (!curr.value_.empty()) {
  //     items.push(curr);
  //   }
  // }
  // update_current();
  for (const auto&curr : item_vec) {
    items.push(curr);
  }
  // illegal: tranc > max or empty
  while (!top_value_legal()) {
    skip_by_tranc_id();
    auto curr = items.top();
    while (!items.empty() && curr.value_.empty() && curr.key_  == items.top().key_) {
      items.pop();
    }
  }
  update_current();
}

HeapIterator::pointer HeapIterator::operator->() const {
  return current.get();
}

HeapIterator::value_type HeapIterator::operator*() const {
  return {{current->first},{current->second}};
}

BaseIterator &HeapIterator::operator++() {
  // if (!items.empty()) {
  //   items.pop();
  //   update_current();
  // }
  // return *this;

  if (!items.empty()) {
    const auto prev = items.top(); // 删除与上一个key相同的所有键值对
    while (!items.empty() && prev.key_ == items.top().key_) {
      items.pop();
    }
    while (!top_value_legal()) {
      skip_by_tranc_id();
      auto curr = items.top();
      while (!items.empty() && curr.value_.empty() && curr.key_  == items.top().key_) {
        items.pop();
      }
    }
  }
  update_current();
  return *this;
}

bool HeapIterator::operator==(const BaseIterator &other) const {
  if (other.get_type() != IteratorType::HeapIterator)return false;
  auto rhs = dynamic_cast<const HeapIterator*>(&other);
  if (rhs->items.empty() && items.empty())return true;
  if (rhs->items.empty() || items.empty())return false;
  if (this->current->first == rhs->current->first && this->current->second == rhs->current->second)return true;
  return false;
}

bool HeapIterator::operator!=(const BaseIterator &other) const {
  if (other.get_type() != IteratorType::HeapIterator)return false;
  auto rhs = dynamic_cast<const HeapIterator*>(&other);
  return !(*this == *rhs);
}

// 没有开启事务， 不为空的 value 才合法
// 事务id可见,不为空的 value 才合法
// 事务id不可见, 即不合法
// items空合法
bool HeapIterator::top_value_legal() const {
  // todo
  if (items.empty())return true;
  const auto &curr = items.top();
  if (max_tranc_id_ == 0 || max_tranc_id_ > curr.tranc_id_) {
    return !curr.value_.empty();
  }
  return false;
}

void HeapIterator::skip_by_tranc_id() {
  if (max_tranc_id_ == 0)return;
  while (!items.empty() && items.top().tranc_id_ > max_tranc_id_) {
    items.pop();
  }
  update_current();
}

bool HeapIterator::is_end() const { return items.empty(); }
bool HeapIterator::is_valid() const { return !items.empty(); }

// current存储了当前指向的键值对
// 每当items的元素发生变化记得update_current.
void HeapIterator::update_current() const {
  if (!items.empty()) {
    current =
        std::make_shared<value_type>(items.top().key_, items.top().value_);
  } else {
    current.reset();
  }
}

IteratorType HeapIterator::get_type() const {
  return IteratorType::HeapIterator;
}

uint64_t HeapIterator::get_tranc_id() const { return max_tranc_id_; }
