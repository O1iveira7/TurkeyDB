#include "sst.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "consts.h"
#include "sst_iterator.h"

// **************************************************
// SST
// **************************************************
// SST::open的工就是将SST文件的元信息进行解码和加载，返回一个描述类SST，
// 你可以将SST看做是SST文件的操作句柄，或者是文件描述符。
std::shared_ptr<SST> SST::open(size_t sst_id, FileObj file,
                               std::shared_ptr<BlockCache> block_cache) {
  // 读取文件末尾的元数据块
  //  meta offset 4B|bloom offset 4B|min tranc 8B|max_tranc 8B
  // 0. 读取最大和最小的事务id 各为8B todo.先不管
  // 1. 读取元数据块的偏移量,  2个 uint32_t,todo有一个bloom offset先不管
  // 分别是 meta 和 bloom 的 offset
  // 2. 读取 bloom filter todo.
  // 布隆过滤器偏移量 + 2*uint32_t 的大小小于文件大小表示存在布隆过滤器
  // 3. 读取并解码元数据块
  // 4. 设置首尾key
  // for now .....data block entries|meta entries|meta offset 4B|
  auto sst = std::make_shared<SST>();
  sst->sst_id = sst_id;
  sst->file = std::move(file);
  sst->block_cache = block_cache;

  auto file_sz = sst->file.size();
  auto meta_offset = sst->file.read_uint32(file_sz - sizeof(uint32_t));
  sst->meta_block_offset = meta_offset;

  auto metas = sst->file.read_to_slice(
      meta_offset, file_sz - sizeof(uint32_t) - meta_offset);
  auto block_meta_vec = BlockMeta::decode_meta_from_slice(metas);
  sst->meta_entries = std::move(block_meta_vec);

  sst->first_key = sst->meta_entries.front().first_key;
  sst->last_key = sst->meta_entries.back().last_key;
  return sst;
}

void SST::del_sst() { file.del_file(); }

std::shared_ptr<SST> SST::create_sst_with_meta_only(
    size_t sst_id, size_t file_size, const std::string &first_key,
    const std::string &last_key, std::shared_ptr<BlockCache> block_cache) {
  auto sst = std::make_shared<SST>();
  sst->file.set_size(file_size);
  sst->sst_id = sst_id;
  sst->first_key = first_key;
  sst->last_key = last_key;
  sst->meta_block_offset = 0;
  sst->block_cache = block_cache;
  return sst;
}

std::shared_ptr<Block> SST::read_block(size_t block_idx) {
  if (block_idx >= meta_entries.size()) {
    throw std::out_of_range("SST::read_block Block index out of range");
  }
  // the last one hh
  auto curr_offset = meta_entries[block_idx].offset;
  size_t curr_sz = 0;
  if (block_idx == meta_entries.size() - 1) {
    curr_sz = meta_block_offset - curr_offset;
  } else {
    curr_sz = meta_entries[block_idx + 1].offset - curr_offset;
  }

  auto block_vec = file.read_to_slice(curr_offset, curr_sz);
  auto target = Block::decode(block_vec, false);
  return target;
}

// TODO: Lab 3.6 二分查找
// ? 给定一个 `key`, 返回其所属的 `block` 的索引
// ? 如果没有找到包含该 `key` 的 Block，返回-1
// 会不会出现这种情况：(先假设没有把)
// 相同的key被分散到了前后两个block   如BLOCK1:......key1 BLOCK2:key1 .....
// 会的兄弟会的。。。
int SST::find_block_idx(const std::string &key) {
  // todo后面的lab，先不管 先在布隆过滤器判断key是否存在 bloom
  // 二分查找
  auto pred = [&](size_t mid) -> bool {
    auto curr_meta = meta_entries[mid];
    return key >= curr_meta.first_key && key <= curr_meta.last_key;
  };
  size_t left = 0, right = meta_entries.size() - 1;
  while (left <= right) {
    int mid = left + (right - left) / 2;  // 防止溢出
    if (pred(mid)) {
      return mid;  // 找到目标，返回索引
    } else if (key < meta_entries[mid].first_key) {
      right = mid - 1;  // 去右边 [mid+1, right]
    } else if (key > meta_entries[mid].last_key) {
      left = mid + 1;  // 去左边 [left, mid-1]
    }
  }
  return -1;  // 未找到目标
}

SstIterator SST::get(const std::string &key, uint64_t tranc_id) {
  // todo后面的lab bloom，先不管  在布隆过滤器判断key是否存在
  return SstIterator(shared_from_this(), key, tranc_id);
}

size_t SST::num_blocks() const { return meta_entries.size(); }

std::string SST::get_first_key() const { return first_key; }

std::string SST::get_last_key() const { return last_key; }

size_t SST::sst_size() const { return file.size(); }

size_t SST::get_sst_id() const { return sst_id; }

// todo
SstIterator SST::begin(uint64_t tranc_id) {
  return SstIterator(shared_from_this(), tranc_id);
}

// todo
SstIterator SST::end() {
  SstIterator iter(shared_from_this(), 0);
  iter.set_block_idx(meta_entries.size());
}

std::pair<uint64_t, uint64_t> SST::get_tranc_id_range() const {
  return std::make_pair(min_tranc_id_, max_tranc_id_);
}

// **************************************************
// SSTBuilder
// **************************************************

SSTBuilder::SSTBuilder(size_t block_size, bool has_bloom) : block(block_size) {
  // 初始化第一个block
  if (has_bloom) {
    bloom_filter = std::make_shared<BloomFilter>(
        BLOOM_FILTER_EXPECTED_SIZE, BLOOM_FILTER_EXPECTED_ERROR_RATE);
  }
  meta_entries.clear();
  data.clear();
  first_key.clear();
  last_key.clear();
}

void SSTBuilder::add(const std::string &key, const std::string &value,
                     uint64_t tranc_id) {
  if (block.is_empty()) first_key = key;
  min_tranc_id_ = std::min(min_tranc_id_, tranc_id);
  max_tranc_id_ = std::max(max_tranc_id_, tranc_id);
  last_key = key;
  if (!block.add_entry(key, value, tranc_id, false)) {
    last_key = key;
    block.add_entry(key, value, tranc_id, true);
    finish_block();
  }
}

size_t SSTBuilder::estimated_size() const { return data.size(); }

// 写入当前block到data中，更新相关元信息
// 清空block
void SSTBuilder::finish_block() {
  const auto curr_data = block.encode();
  meta_entries.emplace_back(data.size(), first_key, last_key);
  data.insert(data.end(), curr_data.begin(), curr_data.end());
  block = Block(block_size);
  first_key.clear();
  last_key.clear();
}

std::shared_ptr<SST> SSTBuilder::build(
    size_t sst_id, const std::string &path,
    std::shared_ptr<BlockCache> block_cache) {
  if (!block.is_empty()) {
    finish_block();
  }
  if (meta_entries.empty()) {
    throw std::runtime_error("SSTBuilder::build empty sst can not build!");
  }
  auto sst_file = FileObj::create_and_write(path, data);
  auto meta_block_offset = data.size();

  std::vector<uint8_t> meta_vec;
  BlockMeta::encode_meta_to_slice(meta_entries, meta_vec);
  if (!sst_file.append(meta_vec)) {
    throw std::runtime_error("SSTBuilder:sst file write meta error!");
  }

  std::vector<uint8_t> offset_u8;
  const char *off_sz_p = reinterpret_cast<const char *>(&meta_block_offset);
  offset_u8.insert(offset_u8.end(), off_sz_p, off_sz_p + sizeof(uint32_t));
  if (!sst_file.append(offset_u8)) {
    throw std::runtime_error("SSTBuilder:sst file write meta offset error!");
  }
  //std::cout << sst_file.size();
  auto sst = std::make_shared<SST>();
  sst->file = std::move(sst_file);
  // WTF????
  auto sz = sst->file.size();
  sst->meta_entries = std::move(meta_entries);
  sst->bloom_offset = 0;  // todo
  sst->meta_block_offset = meta_block_offset;
  sst->sst_id = sst_id;
  sst->first_key = sst->meta_entries.front().first_key;  // ???
  sst->last_key = sst->meta_entries.back().last_key;
  sst->bloom_filter = nullptr;  // todo
  sst->block_cache = block_cache;
  sst->min_tranc_id_ = min_tranc_id_;
  sst->max_tranc_id_ = max_tranc_id_;

  return sst;
}
