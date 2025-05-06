#include <gtest/gtest.h>
#include "block.h"
#include "block_iterator.h"
class BlockTest : public ::testing::Test {
protected:
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
        5, 0,                    // key_len = 5
        'a', 'p', 'p', 'l', 'e', // key
        3, 0,                    // value_len = 3
        'r', 'e', 'd',           // value
        1, 0, 0, 0, 0, 0, 0, 0,  // tranc_id = 1

        // Entry 2: "banana" -> "yellow"
        6, 0,                         // key_len = 6
        'b', 'a', 'n', 'a', 'n', 'a', // key
        6, 0,                         // value_len = 6
        'y', 'e', 'l', 'l', 'o', 'w', // value
        2, 0, 0, 0, 0, 0, 0, 0,       // tranc_id = 2

        // Entry 3: "orange" -> "orange3"
        6, 0,                              // key_len = 6
        'o', 'r', 'a', 'n', 'g', 'e',      // key
        7, 0,                              // value_len = 6
        'o', 'r', 'a', 'n', 'g', 'e', '3', // value
        3, 0, 0, 0, 0, 0, 0, 0,            // tranc_id = 3

        // Entry 4: "orange" -> "orange2"
        6, 0,                              // key_len = 6
        'o', 'r', 'a', 'n', 'g', 'e',      // key
        7, 0,                              // value_len = 6
        'o', 'r', 'a', 'n', 'g', 'e', '2', // value
        2, 0, 0, 0, 0, 0, 0, 0,            // tranc_id = 2

        // Entry 5: "orange" -> "orange1"
        6, 0,                              // key_len = 6
        'o', 'r', 'a', 'n', 'g', 'e',      // key
        7, 0,                              // value_len = 6
        'o', 'r', 'a', 'n', 'g', 'e', '1', // value
        1, 0, 0, 0, 0, 0, 0, 0,            // tranc_id = 1

        // Offset Section (每个entry的起始位置)
        0, 0,  // offset[0] = 0
        20, 0, // offset[1] = 12 (第二个entry的起始位置)
        44, 0, // offset[2] = 24 (第三个entry的起始位置)
        69, 0, // offset[3] = 36 (第四个entry的起始位置)
        94, 0, // offset[4] = 48 (第五个entry的起始位置)

        // Num of elements
        5, 0 // num_elements = 5
    };
    return encoded;
  }
};

TEST_F(BlockTest, DecodeTest) {
  auto encoded = getEncodedBlock();
  auto block = Block::decode(encoded);

  // 验证第一个key
  EXPECT_EQ(block->get_first_key(), "apple");

  // 验证所有key-value对
  EXPECT_EQ(block->get_value_binary("apple", 0).value(), "red");
  EXPECT_EQ(block->get_value_binary("banana", 0).value(), "yellow");
  EXPECT_EQ(block->get_value_binary("orange", 0).value(), "orange3");

  // 指定事务id查询
  EXPECT_EQ(block->get_value_binary("orange", 1).value(), "orange1");
  EXPECT_EQ(block->get_value_binary("orange", 2).value(), "orange2");
  EXPECT_EQ(block->get_value_binary("orange", 3).value(), "orange3");
}

// 测试编码
TEST_F(BlockTest, EncodeTest) {
  Block block(1024);
  block.add_entry("apple", "red", 1, false);
  block.add_entry("banana", "yellow", 2, false);
  block.add_entry("orange", "orange3", 3, false);
  block.add_entry("orange", "orange2", 2, false);
  block.add_entry("orange", "orange1", 1, false);

  auto encoded = block.encode();

  // 解码并验证
  auto decoded = Block::decode(encoded);
  EXPECT_EQ(decoded->get_value_binary("apple", 1).value(), "red");
  EXPECT_EQ(decoded->get_value_binary("banana", 2).value(), "yellow");
  EXPECT_EQ(decoded->get_value_binary("orange", 0).value(), "orange3");
}

// 测试大数据量
TEST_F(BlockTest, LargeDataTest) {
  Block block(1024 * 32);
  const int n = 1000;

  // 添加大量数据
  for (int i = 0; i < n; i++) {
    char key_buf[16];
    snprintf(key_buf, sizeof(key_buf), "key%03d", i);
    std::string key = key_buf;

    char value_buf[16];
    snprintf(value_buf, sizeof(value_buf), "value%03d", i);
    std::string value = value_buf;

    block.add_entry(key, value, 0, false);
  }

  for (int i = 0; i < n; i++) {
    char key_buf[16];
    snprintf(key_buf, sizeof(key_buf), "key%03d", i);
    std::string key = key_buf;

    char value_buf[16];
    snprintf(value_buf, sizeof(value_buf), "value%03d", i);
    std::string expected_value = value_buf;

    EXPECT_EQ(block.get_value_binary(key, 0).value(), expected_value);
  }
}