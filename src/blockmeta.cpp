#include "blockmeta.h"

#include <cstring>
#include <functional>
#include <stdexcept>
#include <cassert>
BlockMeta::BlockMeta() : offset(0), first_key(""), last_key("") {}

BlockMeta::BlockMeta(size_t offset, const std::string &first_key,
                     const std::string &last_key)
    : offset(offset), first_key(first_key), last_key(last_key) {}

void BlockMeta::encode_meta_to_slice(std::vector<BlockMeta> &meta_entries,
                                     std::vector<uint8_t> &metadata) {
  // todo
  auto total_size = sizeof(uint32_t) * 2;  // n + hash
  for (const auto &curr : meta_entries) {
    // entries
    size_t curr_total = sizeof(uint32_t) + sizeof(uint16_t) * 2 +
                        curr.first_key.size() + curr.last_key.size();
    total_size += curr_total;
  }
  metadata.reserve(total_size);

  uint32_t n = meta_entries.size();
  const uint8_t *n_p = reinterpret_cast<const uint8_t *>(&n);
  metadata.insert(metadata.end(), n_p, n_p + sizeof(uint32_t));

  for (const auto &curr : meta_entries) {
    // entry:offset
    auto offset = curr.offset;
    const uint8_t * offset_p = reinterpret_cast<const uint8_t *>(&offset);
    metadata.insert(metadata.end(), offset_p, offset_p + sizeof(uint32_t));

    // entry:first_key_len first_key  last_key_len last_key
    auto first_key_sz = curr.first_key.size();

    const uint8_t * first_p = reinterpret_cast<const uint8_t *>(&first_key_sz);
    metadata.insert(metadata.end(), first_p, first_p + sizeof(uint16_t));
    metadata.insert(metadata.end(), curr.first_key.begin(),
                    curr.first_key.end());

    auto last_key_sz = curr.last_key.size();
    const uint8_t * last_p = reinterpret_cast<const uint8_t *>(&last_key_sz);
    metadata.insert(metadata.end(), last_p, last_p + sizeof(uint16_t));
    metadata.insert(metadata.end(), curr.last_key.begin(), curr.last_key.end());
  }


  std::hash<std::string_view> hasher;
  auto str = std::string_view(reinterpret_cast<const char *>(
      metadata.data()),metadata.size() - sizeof(uint32_t));
  auto hash = hasher(str);
  auto hash_p = reinterpret_cast<const uint8_t *>(&hash);
  metadata.insert(metadata.end(), hash_p, hash_p + sizeof(uint32_t));
}

std::vector<BlockMeta> BlockMeta::decode_meta_from_slice(
    const std::vector<uint8_t> &metadata) {
  if (metadata.size() < sizeof(uint32_t) * 2)throw std::runtime_error("BlockMeta:metadata too small!!!");
  std::vector<BlockMeta> meta_entries;
  uint32_t cnt = 0;
  uint32_t off = 0;
  std::memcpy(&cnt, metadata.data(), sizeof(uint32_t));
  off += sizeof(uint32_t);
  for (int i = 0; i < cnt; i++) {
    uint32_t block_offset = 0;
    std::memcpy(&block_offset,metadata.data() + off,sizeof(uint32_t));
    off += sizeof(uint32_t);

    uint16_t first_key_len = 0;
    std::memcpy(&first_key_len,metadata.data() + off,sizeof(uint16_t));
    off += sizeof(uint16_t);

    std::string first_key(metadata.data() + off,metadata.data() + off + first_key_len);
    off += first_key_len;

    uint16_t last_key_len = 0;
    std::memcpy(&last_key_len,metadata.data() + off,sizeof(uint16_t));
    off += sizeof(uint16_t);

    std::string last_key(metadata.data() + off,metadata.data() + off + last_key_len);
    off += last_key_len;

    meta_entries.emplace_back(block_offset,first_key,last_key);
  }
  size_t curr_hash = 0;
  std::memcpy(&curr_hash,metadata.data() + off,sizeof(uint32_t));
  std::hash<std::string_view> hasher;
  auto str = std::string_view(reinterpret_cast<const char *>(
      metadata.data()),metadata.size() - sizeof(uint32_t));
  auto hash = hasher(str);
  if (hash != curr_hash)throw std::runtime_error("BlockMeta:hash error!!");

  return meta_entries;
}
