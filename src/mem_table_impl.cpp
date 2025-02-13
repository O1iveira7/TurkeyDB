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
#include "mem_table_impl.h"
std::tuple<std::string_view, TurkeyDB::Status> TurkeyDB::SetTable::Get(
    std::string_view key) {
  // TODO
  std::lock_guard<std::mutex> guard(mu_);
  if (inner_.contains(key.data())) {
    // return std::make_tuple(,)
  }
  return {"", Status{STATUS_TYPE::K_NOT_FOUND}};
}

TurkeyDB::Status TurkeyDB::SetTable::Put(uint64_t seq, WRITE_TYPE type,
                                         std::string_view key,
                                         std::string_view value) {
  // we need to format the memtable key as following rule:
  // userkey：key
  // internal_key:InternalKey= UserKey+ 8bytes SequenceNumber 7b + ValueType 1b
  // memtable:|internal_key_size4B|internal_key|value_size4B||value
  using namespace TurkeyDB::Util;

  std::string memtable_key;
  CodingHelper::Put32BitsTo(&memtable_key,
                            key.size() + 8);  // internal_key_size 4b
  memtable_key.append(key);                   // internal_key
  uint64_t seq_type = (seq << 1) | static_cast<uint8_t>(type);
  CodingHelper::Put32BitsTo(&memtable_key, seq_type);      // seq & write type
  CodingHelper::Put32BitsTo(&memtable_key, value.size());  // val size
  memtable_key.append(value);

  inner_.insert(std::move(memtable_key));

  return {STATUS_TYPE::k_OK};
}

TurkeyDB::MemTable::SharedIterPointer TurkeyDB::SetTable::NewIterator() {}