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
  if (other.get_type() != IteratorType::SkipListIterator)
    return false;
  // ?
  auto other2 = dynamic_cast<const SkipListIterator &>(other);
  return current == other2.current;
}

bool SkipListIterator::operator!=(const BaseIterator &other) const {
  return !(*this == other);
}

SkipListIterator::value_type SkipListIterator::operator*() const {
  if (!current)
    throw std::runtime_error("Dereferencing invalid iterator");
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
  std::uniform_int_distribution<int> uniform_dist(1, 10);
  while (uniform_dist(e1) % 2 == 0) {
    k++;
  }
  return k > max_level ? max_level : k;
}

// 插入或更新键值对
void SkipList::put(const std::string &key, const std::string &value,
                   uint64_t tranc_id) {
  std::vector<std::shared_ptr<SkipListNode>> update(max_level, nullptr);

  // 先创建一个新节点
  int new_level = std::max(random_level(), current_level);
  auto new_node =
      std::make_shared<SkipListNode>(key, value, new_level, tranc_id);

  auto current = head;

  // 从最高层开始查找插入位置
  for (int i = current_level - 1; i >= 0; --i) {
    while (current->forward_[i] && *current->forward_[i] < *new_node) {
      current = current->forward_[i];
    }
    update[i] = current;
  }

  // 移动到最底层
  current = current->forward_[0];
  // 已经有当前Key了,更新value
  if (current && current->key_ == key && current->tranc_id_ == tranc_id) {
    size_bytes += value.size() - current->value_.size();
    current->value_ = value;
    return;
  }

  // 如果key不存在，创建新节点
  if (new_level > current_level) {
    for (int i = current_level; i < new_level; ++i) {
      update[i] = head;
    }
  }

  // 生成一个随机数，用于决定是否在每一层更新节点
  int random_bits = dis_level(gen);

  size_bytes += key.size() + value.size() + sizeof(uint64_t);

  for (int i = 0; i < new_level; ++i) {
    bool need_update = false;
    if (i == 0 || (new_level > current_level) || (random_bits & (1 << i))) {
      need_update = true;
    }

    if (need_update) {
      new_node->forward_[i] = update[i]->forward_[i];
      if (new_node->forward_[i]) {
        new_node->forward_[i]->set_backward(i, new_node);
      }
      update[i]->forward_[i] = new_node;
      new_node->set_backward(i, update[i]);
    } else {
      break;
    }
  }

  current_level = new_level;
}

// 查找键值对
SkipListIterator SkipList::get(const std::string &key, uint64_t tranc_id) {
  auto current = head;
  for (int i = current_level - 1; i >= 0; --i) {
    while (current->forward_[i] && current->forward_[i]->key_ < key) {
      current = current->forward_[i];
    }
  }
  current = current->forward_[0];
  if (tranc_id == 0) {
    // 如果没有开启事务，直接比较key即可
    // 如果找到匹配的key，返回value
    if (current && current->key_ == key) {
      return SkipListIterator{current};
    }
  } else {
    while (current && current->key_ == key) {
      // 如果开启了事务，只返回小于等于事务id的值
      if (tranc_id != 0) {
        if (current->tranc_id_ <= tranc_id) {
          // 满足事务可见性
          return SkipListIterator{current};
        } else {
          // 否则跳过
          current = current->forward_[0];
        }
      } else {
        // 没有开启事务
        return SkipListIterator{current};
      }
    }
  }
  return SkipListIterator{};
}

// 删除键值对
// 这里的 remove 是跳表本身真实的 remove,  lsm自身的del应该放入一个空值
void SkipList::remove(const std::string &key) {
  std::vector<std::shared_ptr<SkipListNode>> update(max_level, nullptr);
  auto current = head;

  // 从最高层开始查找目标节点
  for (int i = current->forward_.size() - 1; i >= 0; --i) {
    while (current->forward_[i] && current->forward_[i]->key_ < key) {
      current = current->forward_[i];
    }
    update[i] = current;
  }

  // 移动到最底层
  current = current->forward_[0];

  // 如果找到目标节点，执行删除操作
  if (current && current->key_ == key) {
    // 更新每一层的 forward 指针，跳过目标节点
    for (int i = 0; i < current_level; ++i) {
      if (update[i]->forward_[i] != current) {
        break;
      }
      update[i]->forward_[i] = current->forward_[i];
    }

    // 更新 backward 指针
    for (int i = 0; i < current->backward_.size() && i < current_level; ++i) {
      if (current->forward_[i]) {
        current->forward_[i]->set_backward(i, update[i]);
      }
    }

    // 更新跳表的内存大小
    size_bytes -= key.size() + current->value_.size() + sizeof(uint64_t);

    // 如果删除的节点是最高层的节点，更新跳表的当前层级
    while (current_level > 1 && head->forward_[current_level - 1] == nullptr) {
      current_level--;
    }
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

size_t SkipList::get_size() {
  return size_bytes;
}

// 清空跳表，释放内存
void SkipList::clear() {
  head = std::make_shared<SkipListNode>("", "", max_level, 0);
  size_bytes = 0;
}

SkipListIterator SkipList::begin() {
  return SkipListIterator(head->forward_[0]);
}

SkipListIterator SkipList::end() {
  return SkipListIterator();
}


SkipListIterator SkipList::begin_preffix(const std::string &preffix) {
  auto current = head;
  // 从最高层开始查找
  for (int i = current_level - 1; i >= 0; --i) {
    while (current->forward_[i] && current->forward_[i]->key_ < preffix) {
      current = current->forward_[i];
    }
  }
  current = current->forward_[0];

  return SkipListIterator(current);
}

// 找到前缀的终结位置
SkipListIterator SkipList::end_preffix(const std::string &prefix) {
  auto current = head;
  for (int i = current_level - 1; i >= 0; --i) {
    while (current->forward_[i] && current->forward_[i]->key_ < prefix) {
      current = current->forward_[i];
    }
  }

  current = current->forward_[0];

  while (current && current->key_.substr(0, prefix.size()) == prefix) {
    current = current->forward_[0];
  }

  return SkipListIterator(current);
}

// 返回第一个满足谓词的位置和最后一个满足谓词的迭代器
// 如果不存在, 范围nullptr
// 谓词作用于key, 且保证满足谓词的结果只在一段连续的区间内, 例如前缀匹配的谓词
// predicate返回值:
//   0: 谓词
//   >0: 不满足谓词, 需要向右移动
//   <0: 不满足谓词, 需要向左移动
// Skiplist 中的谓词查询不会进行事务id的判断, 需要上层自己进行判断
std::optional<std::pair<SkipListIterator, SkipListIterator>>
SkipList::iters_monotony_predicate(
    std::function<int(const std::string &)> predicate) {
  auto current = head;
  SkipListIterator begin_iter = SkipListIterator(nullptr);
  SkipListIterator end_iter = SkipListIterator(nullptr);

  // 从最高层开始查找
  // 一开始 current == head, 所以  current_level - 1 处肯定有合法的指针
  bool find1 = false;
  for (int i = current_level - 1; i >= 0; --i) {
    while (!find1) {
      auto forward_i = current->forward_[i];
      if (forward_i == nullptr) {
        break;
      }
      auto direction = predicate(forward_i->key_);
      if (direction == 0) {
        // current 已经满足谓词了
        find1 = true;
        current = forward_i;
        break;
      } else if (direction < 0) {
        // 下一个位置不满足谓词, 且方向错误(位于目标区间右侧)
        // 需要尝试更小的步长(层级)
        break;
      } else {
        // 下一个位置不满足谓词, 但方向正确(位于目标区间左侧)
        current = forward_i;
      }
    }
  }

  if (!find1) {
    return std::nullopt;
  }

  auto current2 = current;

  for (int i = current->backward_.size() - 1; i >= 0; --i) {
    while (true) {
      if (current->backward_[i].lock() == nullptr ||
          current->backward_[i].lock() == head) {
        // 当前层没有前向节点, 或前向节点指向头结点
        break;
      }
      auto direction = predicate(current->backward_[i].lock()->key_);
      if (direction == 0) {
        current = current->backward_[i].lock();
        continue;
      } else if (direction > 0) {
        break;
      } else {
        throw std::runtime_error("iters_predicate: invalid direction");
      }
    }
  }

  begin_iter = SkipListIterator(current);


  for (int i = current2->forward_.size() - 1; i >= 0; --i) {
    while (true) {
      if (current2->forward_[i] == nullptr) {
        break;
      }
      auto direction = predicate(current2->forward_[i]->key_);
      if (direction == 0) {
        current2 = current2->forward_[i];
        continue;
      } else if (direction < 0) {
        break;
      } else {
        throw std::runtime_error("iters_predicate: invalid direction");
      }
    }
  }

  end_iter = SkipListIterator(current2);
  ++end_iter;
  return std::make_optional<std::pair<SkipListIterator, SkipListIterator>>(
      begin_iter, end_iter);
}

// for debug
void SkipList::print_skiplist() {
  for (int level = 0; level < current_level; level++) {
    std::cout << "Level " << level << ": ";
    auto current = head->forward_[level];
    while (current) {
      std::cout << current->key_;
      current = current->forward_[level];
      if (current) {
        std::cout << " -> ";
      }
    }
    std::cout << std::endl;
  }
  std::cout << std::endl;
}