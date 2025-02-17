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

#include <algorithm>
#include <utility>

std::tuple<std::string_view, TurkeyDB::Status> TurkeyDB::SetTable::Get(
    std::string_view key, std::shared_ptr<const ReadOption> rd_opt) {
  std::lock_guard<std::mutex> guard(mu_);
  // todo in the future we will implement snapshot and MVCC

  // todo binary search
  // auto check = [this, key](int mid) -> bool {
  //   auto iter = inner_.begin();
  //   while (mid) {
  //     ++iter;
  //     --mid;
  //   }
  //   if (iter->ExtractUserkey() >= key) return true;
  //   return false;
  // };
  //
  // auto l = 0;
  // auto r = inner_.size();
  // while (l < r) {
  //   if (int mid = l + r >> 1; check(mid))
  //     r = mid;
  //   else
  //     l = mid + 1;
  // }

  auto iter = std::find_if(inner_.begin(), inner_.end(),
                           [key](const MemTableKey& curr) -> auto {
                             return curr.ExtractUserkey() == key;
                           });
  if (iter == inner_.end()) return {"", Status{STATUS_TYPE::K_NOT_FOUND}};

  auto prev = iter;
  ++iter;
  while (iter != inner_.end()) {
    if (iter->ExtractUserkey() != key) {
      break;
    }
    ++iter;
  }
  return {prev->ExtractValue(), Status{STATUS_TYPE::k_OK}};
}

TurkeyDB::Status TurkeyDB::SetTable::Put(uint64_t seq, std::string_view key,
                                         std::string_view value) {
  return WriteToInner(seq, WRITE_TYPE::K_WRITE, key, value);
}
TurkeyDB::Status TurkeyDB::SetTable::Del(uint64_t seq, std::string_view key) {
  return WriteToInner(seq, WRITE_TYPE::K_DELETE, key, "");
}

// TurkeyDB::Iterator* TurkeyDB::SetTable::NewIterator(
//     std::shared_ptr<const ReadOption> rd) {
//   //
// }

TurkeyDB::Status TurkeyDB::SetTable::WriteToInner(uint64_t seq, WRITE_TYPE type,
                                                  std::string_view key,
                                                  std::string_view value) {
  MemTableKey mem_table_key(seq, type, key, value);
  std::lock_guard<std::mutex> guard(mu_);
  auto [_, st] = inner_.insert(std::move(mem_table_key));
  if (st)
    return Status{STATUS_TYPE::k_OK};
  else
    return Status{STATUS_TYPE::K_ERROR};
}

// TurkeyDB::SetTable::MemTableIterator::MemTableIterator(
//     std::shared_ptr<SetTable> shared_table,
//     std::shared_ptr<const ReadOption> rd)
//     : table_(std::move(shared_table)),
//       iter_(table_->inner_.begin()),
//       rd_opt_(rd) {}
//
// bool TurkeyDB::SetTable::MemTableIterator::Valid() const {
//   return table_ != nullptr && iter_ != table_->inner_.end();
// }
//
// void TurkeyDB::SetTable::MemTableIterator::SeekToFirst() {
//   iter_ = table_->inner_.begin();
// }
// void TurkeyDB::SetTable::MemTableIterator::SeekToLast() {
//   auto iter = table_->inner_.begin();
//   auto end = table_->inner_.end();
//   while (iter != end) {
//     iter_ = iter;
//     ++iter;
//   }
// }
//
// void TurkeyDB::SetTable::MemTableIterator::Seek(std::string_view target) {
//   // todo
//   auto curr_iter = std::upper_bound(
//       table_->inner_.begin(), table_->inner_.end(),
//       MemTableKey(0, WRITE_TYPE::K_SEARCH, target, ""),
//       [](const MemTableKey& lhs, const MemTableKey& rhs) { return lhs < rhs; });
//   if (curr_iter == table_->inner_.end()) {
//     status_ = {STATUS_TYPE::K_NOT_FOUND};
//     return;
//   }
//   iter_ = curr_iter;
//   ++curr_iter;
//   while (curr_iter != table_->inner_.end()) {
//     if (curr_iter->ExtractUserkey() != target ||
//         curr_iter->ExtractSeqNum() > rd_opt_->seq_num_) {
//       break;
//     }
//     iter_ = curr_iter;
//   }
// }
//
// void TurkeyDB::SetTable::MemTableIterator::Next() { ++iter_; }
// void TurkeyDB::SetTable::MemTableIterator::Prev() { --iter_; }
// std::string_view TurkeyDB::SetTable::MemTableIterator::Key() const {
//   if (iter_ != table_->inner_.end()) {
//     return iter_->ExtractUserkey();
//   }
//   return "";
// }
// std::string_view TurkeyDB::SetTable::MemTableIterator::Value() const {
//   if (iter_ != table_->inner_.end()) {
//     return iter_->ExtractValue();
//   }
//   return "";
// }
// TurkeyDB::Status TurkeyDB::SetTable::MemTableIterator::GetStatus() const {
//   return status_;
// }