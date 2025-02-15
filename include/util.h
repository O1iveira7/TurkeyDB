// Copyright(c) 2025 Oliveira
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
#ifndef UTIL_H
#define UTIL_H
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
namespace TurkeyDB {

enum class WRITE_TYPE : uint8_t { K_WRITE = 0, K_DELETE = 1 };
enum class STATUS_TYPE { k_OK, K_NOT_FOUND };

struct Status {
  [[nodiscard]] bool Ok() const { return curr_status_ == STATUS_TYPE::k_OK; }
  STATUS_TYPE curr_status_;
};

namespace Config {
static uint64_t MEM_SIZE_LIMIT = 1 << 11;  // in Bytes 2MB

}

namespace Util {

// it's toooooo longgggg to use these facilities...
// TurkeyDB::Util::CodingHelper::xxxx 🤔
class CodingHelper {
 public:
  CodingHelper() = delete;

  static void Put64BitsTo(std::string *dst, uint64_t val) {
    char buf[sizeof(val) + 1];
    Encode64BitsTo(buf, val);
    dst->append(buf,sizeof(buf));
  }
  static void Encode64BitsTo(char *dst, uint64_t val) {
    std::memcpy(dst, &val, sizeof(val));
  }
  static uint64_t Decode64Bits(const char *src) {
    uint64_t val;
    std::memcpy(&val, src, sizeof(val));
    return val;
  }

  static void Put32BitsTo(std::string *dst, uint32_t val) {
    char buf[sizeof(val)];
    Encode32BitsTo(buf, val);
    dst->append(buf,sizeof(buf));
  }
  static void Encode32BitsTo(char *dst, uint32_t val) {
    std::memcpy(dst, &val, sizeof(val));
  }
  static uint32_t Decode32Bits(const char *src) {
    uint32_t val;
    std::memcpy(&val, src, sizeof(val));
    return val;
  }

  // static void Put16BitsTo(std::string *dst, uint16_t val) {
  //   char buf[sizeof(val)];
  //   Encode16BitsTo(buf, val);
  //   dst->append(buf);
  // }
  // static void Encode16BitsTo(char *dst, uint16_t val) {
  //   std::memcpy(dst, &val, sizeof(val));
  // }
  // static uint16_t Decode16Bits(const char *src) {
  //   uint16_t val;
  //   std::memcpy(&val, src, sizeof(val));
  //   return val;
  // }
};  // end of CodingHelper




}  // namespace Util



}  // namespace TurkeyDB
#endif  // UTIL_H
