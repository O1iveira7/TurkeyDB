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
#ifndef DB_H
#define DB_H
#include <memory>
#include <string_view>
#include <tuple>
namespace TurkeyDB {

class DB {
 public:
  using SharedDBPointerT = std::shared_ptr<DB>;

  virtual ~DB() = default;

  static std::tuple<SharedDBPointerT, bool> Open(std::string_view name) {
    return std::make_tuple(nullptr, false);
  }

  virtual bool Put(std::string_view key, std::string_view value) = 0;
  virtual bool Del(std::string_view key) = 0;
  virtual bool Get(std::string_view key) = 0;
};

}  // namespace TurkeyDB

#endif  // DB_H
