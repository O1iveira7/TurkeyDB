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

#include <skiplist.h>
#include  <gtest/gtest.h>

TEST(SkipListTest, BasicOperations) {
  SkipList skipList;
  skipList.put("key1", "value1", 0);
  EXPECT_EQ(skipList.get("key1", 0).get_value(), "value1");
  skipList.put("key1", "new_value", 0);
  EXPECT_EQ(skipList.get("key1", 0).get_value(), "new_value");
  skipList.remove("key1");
  EXPECT_FALSE(skipList.get("key1", 0).is_valid());
}


