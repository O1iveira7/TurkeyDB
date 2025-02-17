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
  std::set<MemTableKey> inner_;

 public:
  // // todo considering concurrency problems of MemTableIterator
  // class MemTableIterator : Iterator {
  //  public:
  //   explicit MemTableIterator(std::shared_ptr<SetTable> shared_table,
  //                             std::shared_ptr<const ReadOption>);
  //   ~MemTableIterator() override = default;
  //
  //   bool Valid() const override;
  //
  //   void SeekToFirst() override;
  //   void SeekToLast() override;
  //   // Position at the first key in the source that is at or past target.
  //   void Seek(std::string_view target) override;
  //   void Next() override;
  //   void Prev() override;
  //   [[nodiscard]] std::string_view Key() const override;
  //   [[nodiscard]] std::string_view Value() const override;
  //   Status GetStatus() const override;
  //
  //  private:
  //   friend class MemTableIterator;
  //
  //   Status status_;
  //   std::shared_ptr<SetTable> table_;
  //   std::set<MemTableKey>::iterator iter_;
  //   std::shared_ptr<const ReadOption> rd_opt_;
  // };  // end of MemTableIterator

  std::tuple<std::string_view, Status> Get(
      std::string_view key, std::shared_ptr<const ReadOption> rd_opt) override;
  Status Put(uint64_t seq, std::string_view key,
             std::string_view value) override;
  Status Del(uint64_t seq, std::string_view key) override;
  //Iterator* NewIterator(std::shared_ptr<const ReadOption>) override;
  ~SetTable() override = default;

  std::set<MemTableKey>::iterator GetIter() { return inner_.begin(); }

  std::set<MemTableKey>::iterator End() { return inner_.end(); }

 private:
  virtual Status WriteToInner(uint64_t seq, WRITE_TYPE type,
                              std::string_view key, std::string_view value);
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
