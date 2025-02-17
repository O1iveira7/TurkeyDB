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
#include <assert.h>

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

  void PrintRep() const {
    for (size_t i = 0; i < rep_.size(); i++) {
      printf("%02x ", static_cast<uint8_t>(rep_.data()[i]));
    }
    printf("\n");
  }

 private:
  static void EncodeKVToMemEntry(uint64_t seq, WRITE_TYPE type,
                                 std::string_view key, std::string_view value,
                                 std::string *dst);

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
  auto internal_key_size = Util::CodingHelper::Decode32Bits(rep_.data());
  auto val_offset = 4 + internal_key_size + 4;
  auto val_size =
      Util::CodingHelper::Decode32Bits(rep_.data() + 4 + internal_key_size);
  // assert(rep_.size() >= 4 + internal_key_size +4 + val_size);
  return std::string_view(rep_.data() + val_offset, val_size);
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

  // Only Get/Put is supported
  // cause delete in LSM is represented as a put with delete tag
  virtual std::tuple<std::string_view, Status> Get(
      std::string_view key, std::shared_ptr<const ReadOption> rd_opt) = 0;
  virtual Status Put(uint64_t seq, std::string_view key,
                     std::string_view value) = 0;
  virtual Status Del(uint64_t seq, std::string_view key) = 0;
  //virtual Iterator* NewIterator(std::shared_ptr<const ReadOption>) = 0;

 private:
  virtual Status WriteToInner(uint64_t seq, WRITE_TYPE type,
                              std::string_view key, std::string_view value) = 0;

 private:
};

}  // namespace TurkeyDB

#endif  // MEM_TABLE_H
