// Copyright (c) 2025 Oliveira
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef MEM_TABLE_H
#define MEM_TABLE_H
#include <memory>
#include <mutex>
#include <set>
#include <string_view>
#include <tuple>

#include "util.h"
namespace TurkeyDB {
class Iterator;

// we need to format the memtable key as following rule:
// userkey：key
// internal_key:InternalKey= UserKey+ 8bytes SequenceNumber 7b + ValueType 1b
// memtable key:|internal_key_size4B|internal_key|value_size4B||value
class MemTableKey {
 public:
  MemTableKey(uint64_t seq, WRITE_TYPE type, std::string_view key,
              std::string_view value);
  MemTableKey(const MemTableKey &) = default;
  MemTableKey &operator=(const MemTableKey &) = default;
  ~MemTableKey() = default;

  auto operator<=>(const MemTableKey &) const;

  std::string_view ExtractUserkey() const;
  uint64_t ExtractSeqNum() const;
  WRITE_TYPE ExtractWriteType() const;
  std::string_view ExtractValue() const;

 private:
  void EncodeKVToMemEntry(uint64_t seq, WRITE_TYPE type, std::string_view key,
                          std::string_view value, std::string *dst);

  std::string rep_;  // inner buf.The name rep is copied from leveldb.I don't
                     // know the meaning hh
};

inline MemTableKey::MemTableKey(uint64_t seq, WRITE_TYPE type,
                                std::string_view key, std::string_view value) {
  EncodeKVToMemEntry(seq, type, key, value, &rep_);
}

inline auto MemTableKey::operator<=>(const MemTableKey &rhs) const {
  if (auto r = ExtractUserkey() <=> rhs.ExtractUserkey(); r != 0) return r;
  return ExtractSeqNum() <=> rhs.ExtractSeqNum();
}

inline std::string_view MemTableKey::ExtractUserkey() const {
  auto len = Util::CodingHelper::Decode32Bits(rep_.data());
  auto user_key = std::string_view(rep_.data() + 4, len - 8);
  return user_key;
}

inline uint64_t MemTableKey::ExtractSeqNum() const {
  auto len = Util::CodingHelper::Decode32Bits(rep_.data());
  auto internal_key = std::string_view(rep_.data() + len - 4, 8);
  auto seq_type = Util::CodingHelper::Decode64Bits(internal_key.data());
  return seq_type >> 1;
}

inline WRITE_TYPE MemTableKey::ExtractWriteType() const {
  auto len = Util::CodingHelper::Decode32Bits(rep_.data());
  auto internal_key = std::string_view(rep_.data() + len - 4, 8);
  auto seq_type = Util::CodingHelper::Decode64Bits(internal_key.data());
  auto type = seq_type & 1;
  if (type == 1) {
    return WRITE_TYPE::K_DELETE;
  }
  return WRITE_TYPE::K_WRITE;
}

inline std::string_view MemTableKey::ExtractValue() const {
  auto key_len = Util::CodingHelper::Decode32Bits(rep_.data());
  auto val_start = rep_.data() + 4 + key_len;
  auto val_len =
      Util::CodingHelper::Decode32Bits(std::string_view(val_start, 4).data());
  return {val_start + 4, val_len};
}

inline void MemTableKey::EncodeKVToMemEntry(uint64_t seq, WRITE_TYPE type,
                                            std::string_view key,
                                            std::string_view value,
                                            std::string *dst) {
  Util::CodingHelper::Put32BitsTo(dst,
                                  key.size() + 8);  // internal_key_size 4B
  dst->append(key);                                 // internal_key
  uint64_t seq_type = (seq << 1) | static_cast<uint8_t>(type);
  Util::CodingHelper::Put64BitsTo(dst, seq_type);      // seq & write type 8B
  Util::CodingHelper::Put32BitsTo(dst, value.size());  // val size 4B
  dst->append(value);
}
// userkey：key
// internal_key:InternalKey= UserKey+ 8bytes SequenceNumber 7b + ValueType 1b
// memtable:|internal_key_size2B|value_size2B|internal_key|sequence(7b)type(1b)|value
class MemTable {
 public:
  MemTable() = default;
  virtual ~MemTable() = default;
  using SharedIterPointer = std::shared_ptr<Iterator>;

  // Only Get/Put is supported
  // cause delete in LSM is represented as a put with delete tag
  virtual std::tuple<std::string_view, Status> Get(std::string_view key) = 0;
  virtual Status Put(uint64_t seq, WRITE_TYPE type, std::string_view key,
                     std::string_view value) = 0;

  virtual SharedIterPointer NewIterator() = 0;

 private:
};

// extract user key from memtable key
struct UserKeyCompartor {
  std::tuple<std::string_view, uint64_t> ExtractUserKeyAndSeq(
      std::string_view str) const {
    auto len = Util::CodingHelper::Decode32Bits(str.data());
    auto internal_key = std::string_view(str.data() + 4, len);
    auto tmp = Util::CodingHelper::Decode64Bits(
        internal_key.substr(len - 8, 8).data());
    auto seq = tmp >> 1;
    return {internal_key.substr(0, len - 8), seq};
  }

  bool operator()(const std::string &lhs, const std::string &rhs) const {
    auto [key1, seq1] = ExtractUserKeyAndSeq(lhs);
    auto [key2, seq2] = ExtractUserKeyAndSeq(rhs);
    if (key1 == key2) {
      return seq1 < seq2;
    }
    return key1 < key2;
  }
};

}  // namespace TurkeyDB

#endif  // MEM_TABLE_H
