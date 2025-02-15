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
#include <gtest/gtest.h>

#include "mem_table.h"
#include "mem_table_impl.h"
#include "util.h"
// TEST(TESTDB, TEST_MEM_TABLE_IMPL) {
//   using namespace TurkeyDB;
//   auto table = SetTable();
//   table.Put(0,TurkeyDB::WRITE_TYPE::K_WRITE,"Hello", "World");
//   auto [val, status] = table.Get("Hello");
//   EXPECT_EQ(status.Ok(),true);
//   EXPECT_EQ(val,"World");
//
//   auto [val2, status2] = table.Get("Apple");
//   EXPECT_EQ(status.Ok(),false);
//   EXPECT_EQ(val,"");
// }

#include <fstream>

std::pair<std::string_view, uint64_t> DecodeMemTableKey(const std::string_view& memtable_key) {
  if (memtable_key.size() < 16) {  // 至少需要 4B(internal_key_size) + 8B(seq_number|type)
    throw std::invalid_argument("memtable_key is too short to decode");
  }

  // 1. 解码 internal_key_size
  uint32_t internal_key_size = TurkeyDB::Util::CodingHelper::Decode32Bits(memtable_key.data());
  if (internal_key_size > memtable_key.size() - 4) {
    throw std::out_of_range("Invalid internal_key_size");
  }

  // 2. 提取 key
  size_t key_len = internal_key_size - 8;  // key 的长度为 internal_key_size - 8
  std::string_view key(memtable_key.data() + 4, key_len);  // 跳过前 4 字节

  // 3. 解码 seq_number 和 type
  uint64_t seq_type = TurkeyDB::Util::CodingHelper::Decode64Bits(memtable_key.data() + 4 + key_len);
  uint64_t seq_number = seq_type >> 1;  // 提取 seq_number
  uint8_t type = seq_type & 0x1;        // 提取 type

  // 返回 key 和 seq_number
  return {key, seq_number};
}
// encoding & decoding from key -> mem table key
TEST(TEST_DB,TEST_MEM_ENTRY_ENDE) {
  using namespace TurkeyDB;
  std::string str;
  EncodeKVToMemEntry(0,WRITE_TYPE::K_WRITE,"key0","val0",&str);

  // auto [key,seq] = DecodeMemTableKey(str);
  // std::cout << key << "  " << seq;
  UserKeyCompartor cmp;
  auto  [key,seq] = cmp.ExtractUserKeyAndSeq(str);
  EXPECT_EQ("key0",key);
  EXPECT_EQ(0,seq);
}