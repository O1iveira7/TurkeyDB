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

#include "util.h"

#include <gtest/gtest.h>

TEST(TEST_DB, TEST_CODING) {
  using namespace TurkeyDB::Util;
  uint64_t val64 = 64;
  char buf64[sizeof(val64)];
  std::string str64;
  CodingHelper::Encode64BitsTo(buf64,val64);
  auto tmp = CodingHelper::Decode64Bits(buf64);
  EXPECT_EQ(tmp,val64);
  CodingHelper::Put64BitsTo(&str64,val64);
  EXPECT_EQ(str64,buf64);

  uint32_t val32 = 32;
  char buf32[sizeof(val32)];
  std::string str32;
  CodingHelper::Encode32BitsTo(buf32,val32);
  auto tmp32 = CodingHelper::Decode32Bits(buf32);
  EXPECT_EQ(tmp32,val32);
  CodingHelper::Put32BitsTo(&str32,val32);
  EXPECT_EQ(str32,buf32);
}