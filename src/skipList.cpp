#include "skiplist.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <utility>


BaseIterator &SkipListIterator::operator++() {
  if (current) {
    // skiplist
    current = current->forward_[0];
  }
  return *this;
}

bool SkipListIterator::operator==(const BaseIterator &other) const {
  if (other.get_type() != IteratorType::SkipListIterator) return false;
  // ?
  auto other2 = dynamic_cast<const SkipListIterator &>(other);
  return current == other2.current;
}

bool SkipListIterator::operator!=(const BaseIterator &other) const {
  return !(*this == other);
}

SkipListIterator::value_type SkipListIterator::operator*() const {
  if (!current) throw std::runtime_error("Dereferencing invalid iterator");
  return {current->key_, current->value_};
}

IteratorType SkipListIterator::get_type() const {
  return IteratorType::SkipListIterator;
}

bool SkipListIterator::is_valid() const {
  return current && !current->key_.empty();
}
bool SkipListIterator::is_end() const { return current == nullptr; }

std::string SkipListIterator::get_key() const { return current->key_; }
std::string SkipListIterator::get_value() const { return current->value_; }
uint64_t SkipListIterator::get_tranc_id() const { return current->tranc_id_; }

SkipList::SkipList(int max_lvl) : max_level(max_lvl), current_level(1) {
  head = std::make_shared<SkipListNode>("", "", max_level, 0);
  dis_01 = std::uniform_int_distribution<>(0, 1);
  dis_level = std::uniform_int_distribution<>(0, (1 << max_lvl) - 1);
  gen = std::mt19937(std::random_device()());
}

int SkipList::random_level() {
  size_t k = 1;
  // Seed with a real random value, if available
  std::random_device r;
  // Choose a random mean between 1 and 10
  std::default_random_engine e1(r());
  std::uniform_int_distribution<int> uniform_dist(1, max_level);
  while (uniform_dist(e1) % 2 == 0) {
    k++;
  }
  return k > max_level ? max_level : k;
}

// 插入或更新键值对
// todo如何更好地保证skiplist node地生成确保上层有较大步长？
void SkipList::put(const std::string &key, const std::string &value,
                   uint64_t tranc_id) {
  std::vector<std::shared_ptr<SkipListNode>> prev_vec(max_level, nullptr);

  auto new_level = random_level();
  auto new_node =
      std::make_shared<SkipListNode>(key, value, new_level, tranc_id);

  auto curr = head;
  for (auto i = current_level - 1; i >= 0; i--) {
    while (curr->forward_[i] != nullptr && *curr->forward_[i] < *new_node) {
      curr = curr->forward_[i];
    }
    prev_vec[i] = curr;
  }

  curr = curr->forward_[0];
  if (curr != nullptr && curr->key_ == key && curr->tranc_id_ == tranc_id) {
    size_bytes -= curr->value_.size() - value.size();
    curr->value_ = value;
    return;
  }

  if (new_level > current_level) {
    for (auto i = new_level - 1; i >= current_level; i--) {
      head->forward_[i] = new_node;
      new_node->set_backward(i, head);
    }
  }

  for (auto i = current_level - 1; i >= 0; i--) {
    bool need_update = false;
    // 当new_level > curr level的时候，必定更新
    // new_level < curr level的时候，只更新新节点level
    if (i <= new_level - 1) {
      need_update = true;
    }
    if (need_update) {
      new_node->forward_[i] = prev_vec[i]->forward_[i];
      if (new_node->forward_[i] != nullptr) {
        new_node->forward_[i]->set_backward(i, new_node);
      }
      prev_vec[i]->forward_[i] = new_node;
      new_node->set_backward(i, prev_vec[i]);
    }
  }
  if (new_level > current_level) current_level = new_level;
  size_bytes += key.size() + value.size() + sizeof(uint64_t);
}

// 查找键值对
SkipListIterator SkipList::get(const std::string &key, uint64_t tranc_id) {
  auto curr = head;
  for (auto i = current_level - 1; i >= 0; i--) {
    while (curr->forward_[i] != nullptr && curr->forward_[i]->key_ < key) {
      curr = curr->forward_[i];
    }
  }

  // todo support tranc_id
  curr = curr->forward_[0];
  if (tranc_id == 0) {
    // no tranc
    if (curr != nullptr && curr->key_ == key) {
      return SkipListIterator{curr};
    }
  } else {
    // tranc
    while (curr != nullptr && curr->key_ == key && curr->tranc_id_ > tranc_id) {
      curr = curr->forward_[0];
    }

    if (curr != nullptr && curr->key_ == key && curr->tranc_id_ <= tranc_id) {
      return SkipListIterator{curr};
    }
  }

  return SkipListIterator{};
}

// 删除键值对
// 这里的 remove 是跳表本身真实的 remove,  lsm自身的del应该放入一个空值
// 不会被使用
void SkipList::remove(const std::string &key) {
  auto curr = head;
  std::vector<std::shared_ptr<SkipListNode>> prev_vec(max_level, nullptr);
  // 找到要删除节点的每层的前一个
  for (auto i = current_level - 1; i >= 0; i--) {
    while (curr->forward_[i] != nullptr && curr->forward_[i]->key_ < key) {
      curr = curr->forward_[i];
    }
    prev_vec[i] = curr;
  }
  // curr其实是要查找key节点的前一个
  auto prev_level = static_cast<int>(curr->forward_.size());
  curr = curr->forward_[0];
  // 要删除的节点不在skiplist中
  if (curr == nullptr || curr->key_ != key) return;
  size_bytes -= curr->key_.size() + curr->value_.size() + sizeof(uint64_t);
  auto curr_level = static_cast<int>(curr->forward_.size());
  auto nxt = curr->forward_[0];  // 要删除节点的后一个
  // 要删除的节点是最后一个节点
  if (nxt == nullptr) {
    for (int i = curr_level - 1; i >= 0; i--) {
      prev_vec[i]->forward_[i] = nullptr;
    }
    return;
  }
  auto nxt_level = static_cast<int>(nxt->forward_.size());
  // nxt_level上面的可能成为了dangling节点 比如 要删除的节点level为4，nxt为2
  for (int i = curr_level - 1; i >= nxt_level; i--) {
    if (prev_vec[i]->forward_[i] == curr) {
      prev_vec[i]->forward_[i] = curr->forward_[i];
      if (curr->forward_[i] != nullptr)
        curr->forward_[i]->set_backward(i, prev_vec[i]);
    }
  }

  for (int i = nxt_level - 1; i >= 0; i--) {
    prev_vec[i]->forward_[i] = nxt;
    nxt->set_backward(i, prev_vec[i]);
  }

  while (curr_level > 1 && head->forward_[curr_level - 1] == nullptr) {
    curr_level--;
  }
}

// 刷盘时可以直接遍历最底层链表
std::vector<std::tuple<std::string, std::string, uint64_t>> SkipList::flush() {
  std::vector<std::tuple<std::string, std::string, uint64_t>> data;
  auto node = head->forward_[0];
  while (node) {
    data.emplace_back(node->key_, node->value_, node->tranc_id_);
    node = node->forward_[0];
  }
  return data;
}

size_t SkipList::get_size() { return size_bytes; }

// 清空跳表，释放内存
void SkipList::clear() {
  head = std::make_shared<SkipListNode>("", "", max_level, 0);
  size_bytes = 0;
}

SkipListIterator SkipList::begin() {
  return SkipListIterator(head->forward_[0]);
}

SkipListIterator SkipList::end() { return SkipListIterator(); }

SkipListIterator SkipList::begin_preffix(const std::string &preffix) {
  auto curr = head;
  for (int i = current_level - 1; i >= 0; i--) {
    while (curr->forward_[i] != nullptr && curr->forward_[i]->key_ < preffix) {
      curr = curr->forward_[i];
    }
  }
  curr = curr->forward_[0];
  if (curr != nullptr) {
    auto curr_key = std::string_view(curr->key_);
    if (curr_key == preffix || curr_key.substr(0, preffix.size()) == preffix)
      return SkipListIterator{curr};
  }
  return SkipListIterator{nullptr};
}
// skipList.put("apple", "0", 0);
// skipList.put("apple2", "1", 0);
// skipList.put("apricot", "2", 0);
// skipList.put("banana", "3", 0);
// skipList.put("berry", "4", 0);
// skipList.put("cherry", "5", 0);
// skipList.put("cherry2", "6", 0);
// 找到前缀的终结位置
// [)
SkipListIterator SkipList::end_preffix(const std::string &prefix) {
  auto curr = head;
  for (int i = current_level - 1; i >= 0; i--) {
    while (curr->forward_[i] != nullptr && curr->forward_[i]->key_ < prefix) {
      curr = curr->forward_[i];
    }
  }
  curr = curr->forward_[0];
  if (curr != nullptr) {
    while (curr != nullptr &&
           std::string_view(curr->key_).substr(0, prefix.size()) == prefix) {
      curr = curr->forward_[0];
    }
    if (curr != nullptr) return SkipListIterator{curr};
  }
  return SkipListIterator{nullptr};
}

// 返回第一个满足谓词的位置和最后一个满足谓词的迭代器[)
// 如果不存在, 范围nullptr
// 谓词作用于key, 且保证满足谓词的结果只在一段连续的区间内, 例如前缀匹配的谓词
// predicate返回值:
//   0: 谓词
//   >0: 不满足谓词, 需要向右移动
//   <0: 不满足谓词, 需要向左移动
// Skiplist 中的谓词查询不会进行事务id的判断, 需要上层自己进行判断
// O(n)扫一边算了233
std::optional<std::pair<SkipListIterator, SkipListIterator>>
SkipList::iters_monotony_predicate(
    std::function<int(const std::string &)> predicate) {
  auto curr = head;

  std::shared_ptr<SkipListNode> node_mid, node_beg, node_end;
  bool find = false;
  for (int i = current_level - 1; i >= 0; i--) {
    while (curr->forward_[i] != nullptr) {
      auto res = predicate(curr->forward_[i]->key_);
      if (res == 0) {
        node_mid = curr->forward_[i];
        find = true;
        break;
      } else if (res < 0) {
        break;
      } else {
        curr = curr->forward_[i];
      }
    }
    if (find) break;
  }
  if (!find) return {};
  curr = node_mid;
  find = false;
  for (int i = static_cast<int>(curr->backward_.size()) - 1; i >= 0; i--) {
    while (!curr->backward_[i].expired()) {
      auto node = curr->backward_[i].lock();
      auto res = predicate(node->key_);
      if (res == 0) {
        node_beg = node;
        find = true;
        curr = node;
      } else if (res < 0) {
        throw std::runtime_error("predicate(node->key_) should not < 0 here!");
      } else {
        break;
      }
    }
  }
  if (!find) {
    node_beg = node_mid;
  }
  curr = node_mid;
  find = false;
  for (int i = static_cast<int>(curr->forward_.size() - 1); i >= 0; i--) {
    while (curr->forward_[i] != nullptr) {
      auto res = predicate(curr->forward_[i]->key_);
      if (res == 0) {
        node_end = curr->forward_[i];
        curr = curr->forward_[i];  // 必须更新
        find = true;
      } else if (res < 0) {
        break;
      } else {
        throw std::runtime_error("predicate(node->key_) should not > 0 here!");
      }
    }
  }
  if (!find) return {{node_beg, node_mid->forward_[0]}};
  return {{node_beg, node_end->forward_[0]}};
}

// for debug
void SkipList::print_skiplist() {
  for (int level = 0; level < current_level; level++) {
    std::cout << "Level " << level << ": ";
    auto current = head->forward_[level];
    while (current) {
      std::cout << current->key_ << "-" << current->tranc_id_;
      current = current->forward_[level];
      if (current) {
        std::cout << " -> ";
      }
    }
    std::cout << std::endl;
  }
  std::cout << std::endl;
}