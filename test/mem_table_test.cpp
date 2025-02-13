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
TEST(TESTDB, TEST_MEM_TABLE_IMPL) {
  using namespace TurkeyDB;
  auto table = SetTable();
  table.Put(0,TurkeyDB::WRITE_TYPE::K_WRITE,"Hello", "World");
  auto [val, status] = table.Get("Hello");
  EXPECT_EQ(status.Ok(),true);
  EXPECT_EQ(val,"World");

  auto [val2, status2] = table.Get("Apple");
  EXPECT_EQ(status.Ok(),false);
  EXPECT_EQ(val,"");
}