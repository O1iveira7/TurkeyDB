#include "block.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "block_iterator.h"

Block::Block(size_t capacity) : capacity(capacity) {}
// todo 都忘记考虑 tranc_id了
std::vector<uint8_t> Block::encode() {
  // todo
  std::vector<uint8_t> res(data.size() + offsets.size() * sizeof(uint16_t) +
                           sizeof(uint16_t));
  size_t res_sz = 0;
  // data
  std::memcpy(res.data(), data.data(), data.size());
  res_sz += data.size();
  // offsets
  std::memcpy(res.data() + res_sz, offsets.data(), offsets.size() * 2);
  res_sz += offsets.size() * 2;
  uint16_t cnt = offsets.size();
  // cnt
  std::memcpy(res.data() + res_sz, &cnt, sizeof(uint16_t));
  return res;
}

std::shared_ptr<Block> Block::decode(const std::vector<uint8_t> &encoded,
                                     bool with_hash) {
  // todo how to deal with hash...
  size_t snallest = sizeof(uint16_t) * 3 + sizeof(uint64_t);
  if (encoded.size() < snallest)
    throw std::runtime_error("encoded is too small!");
  auto cnt_pos = encoded.size() - sizeof(uint16_t);
  if (with_hash) {
    cnt_pos -= sizeof(uint32_t);
    uint32_t block_hash = 0;
    auto hash_pos = encoded.size() - sizeof(uint32_t);
    std::memcpy(&block_hash, encoded.data() + hash_pos, sizeof(uint32_t));
    std::hash<std::string_view> hasher;

    if (block_hash !=
        hasher(static_cast<std::string_view>(reinterpret_cast<const char *>(
            encoded.data(), encoded.size() - sizeof(uint32_t))))) {
      throw std::runtime_error("block has been brocken!");
    }
  }
  uint16_t cnt = 0;
  // cnt
  std::memcpy(&cnt, encoded.data() + cnt_pos, sizeof(uint16_t));
  // offset
  size_t data_sz = cnt_pos - cnt * sizeof(uint16_t);
  std::vector<uint16_t> offset_vec(cnt);
  std::memcpy(offset_vec.data(), encoded.data() + data_sz,
              cnt * sizeof(uint16_t));
  // data
  std::vector<uint8_t> data_vec(data_sz);
  std::memcpy(data_vec.data(), encoded.data(), data_sz);

  auto res = std::make_shared<Block>();
  res->data = std::move(data_vec);
  res->offsets = std::move(offset_vec);

  return res;
}

std::string Block::get_first_key() {
  if (data.empty() || offsets.empty()) {
    return "";
  }

  // 读取第一个key的长度（前2字节）
  uint16_t key_len;
  memcpy(&key_len, data.data(), sizeof(uint16_t));

  // 读取key
  std::string key(reinterpret_cast<char *>(data.data() + sizeof(uint16_t)),
                  key_len);
  return key;
}

size_t Block::get_offset_at(size_t idx) const {
  if (idx > offsets.size()) {
    throw std::runtime_error("idx out of offsets range");
  }
  return offsets[idx];
}

bool Block::add_entry(const std::string &key, const std::string &value,
                      uint64_t tranc_id, bool force_write) {
  // todo
  size_t new_entry_sz = sizeof(uint16_t) + key.size() + sizeof(uint16_t) +
                        value.size() + sizeof(uint64_t);
  size_t new_total_sz = data.size() + new_entry_sz;
  if (new_total_sz > capacity && !force_write) return false;

  // offset
  uint16_t offset = static_cast<uint16_t>(data.size());
  offsets.push_back(offset);

  // key size
  uint16_t key_sz = static_cast<uint16_t>(key.size());
  const uint8_t *key_sz_ptr = reinterpret_cast<const uint8_t *>(&key_sz);
  data.insert(data.end(), key_sz_ptr, key_sz_ptr + sizeof(uint16_t));

  // key
  data.insert(data.end(), key.begin(), key.end());

  // value size
  uint16_t val_sz = static_cast<uint16_t>(value.size());
  const uint8_t *val_sz_ptr = reinterpret_cast<const uint8_t *>(&val_sz);
  data.insert(data.end(), val_sz_ptr, val_sz_ptr + sizeof(uint16_t));

  // value
  data.insert(data.end(), value.begin(), value.end());

  // tranc_id
  const uint8_t *tranc_ptr = reinterpret_cast<const uint8_t *>(&tranc_id);
  data.insert(data.end(), tranc_ptr, tranc_ptr + sizeof(uint64_t));

  return true;
}

// 从指定偏移量获取entry的key
std::string Block::get_key_at(size_t offset) const {
  // todo
  uint16_t key_sz = 0;
  std::memcpy(&key_sz, data.data() + offset, sizeof(uint16_t));
  const char *key_ptr =
      reinterpret_cast<const char *>(data.data() + offset + sizeof(uint16_t));
  return {key_ptr, key_sz};
}

// 从指定偏移量获取entry的value
std::string Block::get_value_at(size_t offset) const {
  // todo
  uint16_t key_sz = 0, val_sz = 0;
  std::memcpy(&key_sz, data.data() + offset, sizeof(uint16_t));
  std::memcpy(&val_sz, data.data() + offset + sizeof(uint16_t) + key_sz,
              sizeof(uint16_t));
  const char *val_ptr = reinterpret_cast<const char *>(
      data.data() + offset + sizeof(uint16_t) + key_sz + sizeof(uint16_t));
  return {val_ptr, val_sz};
}
// tranc是uint16??
uint64_t Block::get_tranc_id_at(size_t offset) const {
  // todo
  uint16_t key_sz = 0, val_sz = 0;
  std::memcpy(&key_sz, data.data() + offset, sizeof(uint16_t));
  std::memcpy(&val_sz, data.data() + offset + sizeof(uint16_t) + key_sz,
              sizeof(uint16_t));
  uint64_t tranc = 0;
  std::memcpy(&tranc,
              data.data() + offset + sizeof(uint16_t) + key_sz +
                  sizeof(uint16_t) + val_sz,
              sizeof(uint64_t));

  return tranc;
}

// 比较指定偏移量处的key与目标key
int Block::compare_key_at(size_t offset, const std::string &target) const {
  std::string key = get_key_at(offset);
  return key.compare(target);
}

// 相同的key连续分布, 且相同的key的事务id从大到小排布
// 这里的逻辑是找到最接近 tranc_id 的键值对的索引位置
int Block::adjust_idx_by_tranc_id(size_t idx, uint64_t tranc_id) {
  if (idx >= offsets.size()) {
    return -1;  // 索引超出范围
  }

  auto target_key = get_key_at(offsets[idx]);

  if (tranc_id != 0) {
    auto cur_tranc_id = get_tranc_id_at(offsets[idx]);

    if (cur_tranc_id <= tranc_id) {
      // 当前记录可见，向前查找更接近的目标
      size_t prev_idx = idx;
      while (prev_idx > 0 && is_same_key(prev_idx - 1, target_key)) {
        prev_idx--;
        auto new_tranc_id = get_tranc_id_at(offsets[prev_idx]);
        if (new_tranc_id > tranc_id) {
          return prev_idx + 1;  // 更新的记录不可见
        }
      }
      return prev_idx;
    } else {
      // 当前记录不可见，向后查找
      size_t next_idx = idx + 1;
      while (next_idx < offsets.size() && is_same_key(next_idx, target_key)) {
        auto new_tranc_id = get_tranc_id_at(offsets[next_idx]);
        if (new_tranc_id <= tranc_id) {
          return next_idx;  // 找到可见记录
        }
        next_idx++;
      }
      return -1;  // 没有找到满足条件的记录
    }
  } else {
    // 没有开启事务的话, 直接选择最大的事务id的记录返回
    size_t prev_idx = idx;
    while (prev_idx > 0 && is_same_key(prev_idx - 1, target_key)) {
      prev_idx--;
    }
    return prev_idx;
  }
}

bool Block::is_same_key(size_t idx, const std::string &target_key) const {
  if (idx >= offsets.size()) {
    return false;  // 索引超出范围
  }
  return get_key_at(offsets[idx]) == target_key;
}

// 使用二分查找获取value
// 要求在插入数据时有序插入
std::optional<std::string> Block::get_value_binary(const std::string &key,
                                                   uint64_t tranc_id) {
  auto idx = get_idx_binary(key, tranc_id);
  if (!idx.has_value()) {
    return std::nullopt;
  }

  return get_value_at(offsets[*idx]);
}
// binary search
std::optional<size_t> Block::get_idx_binary(const std::string &key,
                                            uint64_t tranc_id) {
  if (offsets.empty()) return std::nullopt;
  // todo
  auto pred = [&](const Entry &curr) -> bool {
    if (curr.key == key) {
      if (tranc_id != 0 && tranc_id < curr.tranc_id) return false;
      return true;
    }
    return curr.key > key;
  };

  size_t l = 0, r = offsets.size() - 1;
  while (l < r) {
    int mid = (l + r) >> 1;
    Entry curr;
    auto curr_offset = get_offset_at(mid);
    curr.key = get_key_at(curr_offset);
    curr.tranc_id = get_tranc_id_at(curr_offset);
    if (pred(curr))
      r = mid;
    else
      l = mid + 1;
  }

  Entry finnal;
  finnal.key = get_key_at(get_offset_at(l));
  finnal.tranc_id = get_tranc_id_at(get_offset_at(l));
  if (finnal.key == key) {
    if ((tranc_id != 0 && finnal.tranc_id <= tranc_id) || tranc_id == 0) {
      return l;
    }
  }
  return std::nullopt;
}

// 返回第一个满足谓词的位置和最后一个满足谓词的位置
// 如果不存在, 范围nullptr
// 谓词作用于key, 且保证满足谓词的结果只在一段连续的区间内, 例如前缀匹配的谓词
// 返回的区间是闭区间, 开区间需要手动对返回值自增
// predicate返回值:
//   0: 满足谓词
//   >0: 不满足谓词, 需要向右移动
//   <0: 不满足谓词, 需要向左移动
std::optional<
    std::pair<std::shared_ptr<BlockIterator>, std::shared_ptr<BlockIterator>>>
Block::get_monotony_predicate_iters(
    uint64_t tranc_id, std::function<int(const std::string &)> predicate) {
  //[2,6)   tranc_id 7
  if (offsets.empty()) return std::nullopt;
  int start_idx = -1, end_idx = -1;
  size_t l = 0, r = offsets.size() - 1;
  while (l < r) {
    int mid = (l + r) >> 1;
    auto curr_offset = get_offset_at(mid);
    auto curr_key = get_key_at(curr_offset);
    auto pred_res = predicate(curr_key);
    if (pred_res < 0)
      r = mid;
    else {
      if (pred_res == 0) {
        start_idx = mid;
        r = mid;
        continue;
      }
      l = mid + 1;
    }
  }
  if (start_idx == -1) return std::nullopt;
  l = start_idx;
  r = offsets.size() - 1;
  while (l < r) {
    int mid = (l + r + 1) >> 1;
    auto curr_offset = get_offset_at(mid);
    auto curr_key = get_key_at(curr_offset);
    auto pred_res = predicate(curr_key);
    if (pred_res > 0)
      l = mid;  // 可行区间
    else {
      if (pred_res == 0) {
        end_idx = mid;
        l = mid;
        continue;
      }
      r = mid - 1;
    }
  }
  // 这边又是闭区间了????
  if (end_idx == -1) {
    auto start_p = std::make_shared<BlockIterator>(shared_from_this(),
                                                   start_idx, tranc_id);
    auto end_p = start_p;
    ++(*end_p);
    return std::make_optional(std::make_pair(start_p, end_p));
  }
  auto start_p =
      std::make_shared<BlockIterator>(shared_from_this(), start_idx, tranc_id);
  auto end_p =
      std::make_shared<BlockIterator>(shared_from_this(), end_idx + 1, tranc_id);

  return std::make_optional(std::make_pair(start_p, end_p));
}

Block::Entry Block::get_entry_at(size_t offset) const {
  Entry entry;
  entry.key = get_key_at(offset);
  entry.value = get_value_at(offset);
  entry.tranc_id = get_tranc_id_at(offset);
  return entry;
}

size_t Block::size() const { return offsets.size(); }

size_t Block::cur_size() const {
  return data.size() + offsets.size() * sizeof(uint16_t) + sizeof(uint16_t);
}

bool Block::is_empty() const { return offsets.empty(); }

BlockIterator Block::begin(uint64_t tranc_id) {
  return BlockIterator(shared_from_this(), 0, tranc_id);
}

// todo how to deal with tranc_id?
// 因为返回的是一个区间[beg,end)
// 下面：key，tranc_id
// key1,3   key1,2   key2,3
// 怎么把key1，2排除调呢？ ==>迭代器的++实现了这个🐕
// ==>只要找到开始和结束的区间就可以了
std::optional<
    std::pair<std::shared_ptr<BlockIterator>, std::shared_ptr<BlockIterator>>>
Block::iters_preffix(uint64_t tranc_id, const std::string &preffix) {
  size_t curr_idx = 0, end_idx = offsets.size();
  size_t start_idx = 0;
  while (curr_idx != end_idx) {
    if (get_key_at(get_offset_at(curr_idx)).starts_with(preffix)) {
      break;
    }
    ++curr_idx;
  }
  if (curr_idx != end_idx) {
    start_idx = curr_idx;
    while (curr_idx != end_idx) {
      if (!get_key_at(get_offset_at(curr_idx)).starts_with(preffix)) {
        break;
      }
      ++curr_idx;
    }
    auto res = std::make_pair(std::make_shared<BlockIterator>(
                                  shared_from_this(), start_idx, tranc_id),
                              std::make_shared<BlockIterator>(
                                  shared_from_this(), curr_idx, tranc_id));
    return std::make_optional(res);
  }
  return std::nullopt;
}

BlockIterator Block::end() {
  return BlockIterator(shared_from_this(), offsets.size(), 0);
}