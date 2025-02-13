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
  virtual Status Put(uint64_t seq,WRITE_TYPE type,std::string_view key, std::string_view value) = 0;

  virtual SharedIterPointer NewIterator() = 0;

 private:
};
}  // namespace TurkeyDB

#endif  // MEM_TABLE_H
