#include <cstdint>
#include <iostream>
#include <set>
#include <vector>

#include "sst.h"
#include "engine.h"

// 预定义的编码数据
std::vector<uint8_t> getEncodedBlock() {
  /*
  Block layout (3 entries):
  Entry1: key="apple", value="red"
  Entry2: key="banana", value="yellow"
  Entry3: key="orange", value="orange"
  */
  std::vector<uint8_t> encoded = {
      // Data Section
      // Entry 1: "apple" -> "red"
      5, 0,                     // key_len = 5
      'a', 'p', 'p', 'l', 'e',  // key
      3, 0,                     // value_len = 3
      'r', 'e', 'd',            // value
      1, 0, 0, 0, 0, 0, 0, 0,   // tranc_id = 1

      // Entry 2: "banana" -> "yellow"
      6, 0,                          // key_len = 6
      'b', 'a', 'n', 'a', 'n', 'a',  // key
      6, 0,                          // value_len = 6
      'y', 'e', 'l', 'l', 'o', 'w',  // value
      2, 0, 0, 0, 0, 0, 0, 0,        // tranc_id = 2

      // Entry 3: "orange" -> "orange3"
      6, 0,                               // key_len = 6
      'o', 'r', 'a', 'n', 'g', 'e',       // key
      7, 0,                               // value_len = 6
      'o', 'r', 'a', 'n', 'g', 'e', '3',  // value
      3, 0, 0, 0, 0, 0, 0, 0,             // tranc_id = 3

      // Entry 4: "orange" -> "orange2"
      6, 0,                               // key_len = 6
      'o', 'r', 'a', 'n', 'g', 'e',       // key
      7, 0,                               // value_len = 6
      'o', 'r', 'a', 'n', 'g', 'e', '2',  // value
      2, 0, 0, 0, 0, 0, 0, 0,             // tranc_id = 2

      // Entry 5: "orange" -> "orange1"
      6, 0,                               // key_len = 6
      'o', 'r', 'a', 'n', 'g', 'e',       // key
      7, 0,                               // value_len = 6
      'o', 'r', 'a', 'n', 'g', 'e', '1',  // value
      1, 0, 0, 0, 0, 0, 0, 0,             // tranc_id = 1

      // Offset Section (每个entry的起始位置)
      0, 0,   // offset[0] = 0
      20, 0,  // offset[1] = 12 (第二个entry的起始位置)
      44, 0,  // offset[2] = 24 (第三个entry的起始位置)
      69, 0,  // offset[3] = 36 (第四个entry的起始位置)
      94, 0,  // offset[4] = 48 (第五个entry的起始位置)

      // Num of elements
      5, 0  // num_elements = 5
  };
  return encoded;
}

void build_sst() {
  SSTBuilder builder(1024, true);  // 1KB block size
  auto block_cache = std::make_shared<BlockCache>(1024, 8);

  // 添加一些数据
  builder.add("key1", "value1", 0);
  builder.add("key2", "value2", 0);
  builder.add("key3", "value3", 0);

  // 构建SST
  auto sst = builder.build(1, "./test_data/basic.sst", block_cache);
}

void show_block() {}

void show_sst() {}

void usage() {}

int main() {
  // create lsm instance, data_dir is the directory to store data
  LSM lsm("example_data");

  // put data
  lsm.put("key1", "value1");
  lsm.put("key2", "value2");
  lsm.put("key3", "value3");

  // Query data
  auto value1 = lsm.get("key1");
  if (value1.has_value()) {
    std::cout << "key1: " << value1.value() << std::endl;
  } else {
    std::cout << "key1 not found" << std::endl;
  }

  // Update data
  lsm.put("key1", "new_value1");
  auto new_value1 = lsm.get("key1");
  if (new_value1.has_value()) {
    std::cout << "key1: " << new_value1.value() << std::endl;
  } else {
    std::cout << "key1 not found" << std::endl;
  }

  // delete data
  lsm.remove("key2");
  auto value2 = lsm.get("key2");
  if (value2.has_value()) {
    std::cout << "key2: " << value2.value() << std::endl;
  } else {
    std::cout << "key2 not found" << std::endl;
  }

  // iterator
  std::cout << "All key-value pairs:" << std::endl;
  // begin(id): id means transaction id, 0 means disable mvcc
  for (auto it = lsm.begin(0); it != lsm.end(); ++it) {
    std::cout << it->first << ": " << it->second << std::endl;
  }

  //lsm.clear();

  return 0;
}
