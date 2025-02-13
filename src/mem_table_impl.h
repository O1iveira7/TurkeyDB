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
#ifndef MEM_TABLE_IMPL_H
#define MEM_TABLE_IMPL_H
#include <memory>
#include <mutex>
#include <set>

#include "iterator.h"
#include "mem_table.h"
#include "util.h"

namespace TurkeyDB {
// TODO
// we need implement some facilities to extract use key from the entry
// also the corresponding compartor is needed
// cause the scenario is to compare use key not the entire mem table key
class SetTable : MemTable {
 private:

  std::mutex mu_;
  std::set<std::string> inner_;

 public:
  class MemTableIterator : Iterator {};

  std::tuple<std::string_view, Status> Get(std::string_view key) override;
  Status Put(uint64_t seq,WRITE_TYPE type,std::string_view key, std::string_view value) override;
  SharedIterPointer NewIterator() override;

  ~SetTable() override = default;
};

// TODO
// class SkipListTable : MemTable {
//  public:
//   std::tuple<std::string_view, Status> Get(std::string_view key) override;
//   Status Put(std::string_view key, std::string_view value) override;
//   SharedIterPointer NewIterator() override;
//   ~SkipListTable() override = default;
// };
}  // namespace TurkeyDB

#endif  // MEM_TABLE_IMPL_H
