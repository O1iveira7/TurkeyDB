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
  // todo considering concurrency problems of MemTableIterator
  // 为什么不能在这边直接继承Iterator呢？？？
  // leveldb也不是直接继承
  // scenario:iterate over Memtable keys whose seq num <= given read option
  class SetTableIterator {
   public:
    explicit SetTableIterator(const SetTable *table,
                              std::shared_ptr<const ReadOption> rd)
        : table_(std::move(table)),
          iter_(table_->inner_.begin()),
          rd_opt_(rd) {}
    ~SetTableIterator() = default;

    bool Valid() const;

    void SeekToFirst();
    void SeekToLast();
    // Position at the first key in the source that is at or past target.
    void Seek(std::string_view target);
    void Next();
    void Prev();
    [[nodiscard]] std::string_view Key() const;
    [[nodiscard]] std::string_view Value() const;
    Status GetStatus() const;

    uint64_t GetSeqNum() {
      return iter_->ExtractSeqNum();
    }
   private:
    friend class MemTableIterator;

    Status status_{STATUS_TYPE::k_OK};
    // why not shared_ptr
    const SetTable *table_;
    std::set<MemTableKey>::iterator iter_;
    std::shared_ptr<const ReadOption> rd_opt_;
  };  // end of MemTableIterator

  std::tuple<std::string_view, Status> Get(
      std::string_view key, std::shared_ptr<const ReadOption> rd_opt) override;
  Status Put(uint64_t seq, std::string_view key,
             std::string_view value) override;
  Status Del(uint64_t seq, std::string_view key) override;
  std::shared_ptr<Iterator> NewIterator(
      std::shared_ptr<const ReadOption>) const override;
  ~SetTable() override = default;

 private:
  Status WriteToInner(uint64_t seq, WRITE_TYPE type, std::string_view key,
                      std::string_view value);
};

class MemTableIterator : public Iterator {
 public:
  MemTableIterator(const SetTable *table, std::shared_ptr<const ReadOption> rd)
      : iter_(table, rd) {}
  ~MemTableIterator() override = default;
  bool Valid() const override { return iter_.Valid(); }
  void SeekToFirst() override { iter_.SeekToFirst(); }
  void SeekToLast() override { iter_.SeekToLast(); }
  void Seek(std::string_view target) override { iter_.Seek(target); }
  void Next() override { iter_.Next(); }
  void Prev() override { iter_.Prev(); }
  std::string_view Key() const override { return iter_.Key(); }
  std::string_view Value() const override { return iter_.Value(); }
  Status GetStatus() const override { return iter_.GetStatus(); }

  uint64_t GetSeqNum() {
    return iter_.GetSeqNum();
  }
 private:
  SetTable::SetTableIterator iter_;
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
