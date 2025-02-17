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
#include "mem_table.h"

#include <gtest/gtest.h>

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

TEST(TEST_DB, TEST_MEM_TABLE_KEY) {
  using namespace TurkeyDB;
  MemTableKey key0(0, WRITE_TYPE::K_WRITE, "key0", "value0");
  MemTableKey key1(1, WRITE_TYPE::K_WRITE, "key0", "value0");
  EXPECT_GE(key1, key0);
  EXPECT_EQ(key0.ExtractUserkey(), "key0");
  EXPECT_EQ(key0.ExtractWriteType(), WRITE_TYPE::K_WRITE);
  EXPECT_EQ(key0.ExtractSeqNum(), 0);
  EXPECT_EQ(key0.ExtractValue(), "value0");
}

TEST(TEST_DB, TEST_MEM_TABLE_ITERATOR) {
  using namespace TurkeyDB;
  SetTable table;
  table.Put(0, "key0", "value0");
  table.Put(1, "key0", "val");
  table.Put(2, "key0", "val0");
  table.Put(3, "key1", "value1");
  table.Del(4, "key1");
  table.Put(5, "key2", "val2");

  auto beg = table.GetIter();
  while (beg != table.End()) {
    std::cout << beg->ExtractUserkey() << "  " << beg->ExtractSeqNum() << "  "
              << beg->ExtractValue() << std::endl;
    beg++;
  }
}

TEST(TEST_DB, TEST_MEM_TABLE_PUT_GET) {
  using namespace TurkeyDB;
  // SetTable table;
  // table.Put(0,  "key0", "value0");
  // auto [value0, st1] = table.Get("key0");
  // EXPECT_EQ(st1.Ok(), true);
  // EXPECT_EQ(value0, "value0");
  //
  // table.Put(1,  "key0", "val0");
  // auto [val0, st_val0] = table.Get("key0");

  // EXPECT_EQ(st_val0.Ok(), true);
  // EXPECT_EQ(val0,"val0");
  //
  // auto [_,st_not_f] = table.Get("key1");
  // EXPECT_EQ(st_not_f.curr_status_,STATUS_TYPE::K_NOT_FOUND);
}
// encoding & decoding from key -> mem table key
TEST(TEST_DB, TEST_MEM_ENTRY_ENDE) {}