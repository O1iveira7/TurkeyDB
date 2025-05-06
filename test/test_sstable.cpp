#include <gtest/gtest.h>
#include "sst.h"

class SSTTest : public ::testing::Test {
protected:
  void SetUp() override {
    // 创建测试目录
    if (!std::filesystem::exists("test_data")) {
      std::filesystem::create_directory("test_data");
    }
  }

  void TearDown() override {
    // 清理测试文件
    std::filesystem::remove_all("test_data");
  }

  // 辅助函数：创建一个包含有序数据的SST
  std::shared_ptr<SST> create_test_sst(size_t block_size, size_t num_entries) {
    SSTBuilder builder(block_size, true);

    for (size_t i = 0; i < num_entries; i++) {
      std::string key = "key" + std::to_string(i);
      std::string value = "value" + std::to_string(i);
      builder.add(key, value, 0);
    }

    auto block_cache = std::make_shared<BlockCache>(
        1024,
        8);

    return builder.build(1, "test_data/test.sst", block_cache);
  }
};

// 测试基本的写入和读取
TEST_F(SSTTest, BasicWriteAndRead) {
  SSTBuilder builder(1024, true); // 1KB block size
  auto block_cache = std::make_shared<BlockCache>(
      1024,
      8);

  // 添加一些数据
  builder.add("key1", "value1", 0);
  builder.add("key2", "value2", 0);
  builder.add("key3", "value3", 0);

  // 构建SST
  auto sst = builder.build(1, "test_data/basic.sst", block_cache);

  // 验证基本属性
  EXPECT_EQ(sst->get_first_key(), "key1");
  EXPECT_EQ(sst->get_last_key(), "key3");
  EXPECT_EQ(sst->get_sst_id(), 1);
  EXPECT_GT(sst->sst_size(), 0);

  // 读取并验证数据
  auto block = sst->read_block(0);
  EXPECT_TRUE(block != nullptr);
  auto value = block->get_value_binary("key2", 0);
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(*value, "value2");
}

// 测试block分裂
TEST_F(SSTTest, BlockSplitting) {
  // 使用小的block size强制分裂
  SSTBuilder builder(64, true); // 很小的block size
  auto block_cache = std::make_shared<BlockCache>(
      1024,
      8);

  // 添加足够多的数据以触发分裂
  for (int i = 0; i < 10; i++) {
    std::string key = "key" + std::to_string(i);
    std::string value = std::string(20, 'v') + std::to_string(i); // 较大的value
    builder.add(key, value, 0);
  }

  auto sst = builder.build(1, "test_data/split.sst", block_cache);

  // 验证有多个block
  EXPECT_GT(sst->num_blocks(), 1);

  // 验证每个block都可以正确读取
  for (size_t i = 0; i < sst->num_blocks(); i++) {
    auto block = sst->read_block(i);
    EXPECT_TRUE(block != nullptr);
  }
}

// 测试key查找
TEST_F(SSTTest, KeySearch) {
  auto sst = create_test_sst(256, 100); // 创建包含100个entry的SST

  // 测试find_block_idx
  size_t idx = sst->find_block_idx("key50");
  auto block = sst->read_block(idx);
  auto value = block->get_value_binary("key50", 0);
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(*value, "value50");

  // 测试边界情况
  EXPECT_EQ(sst->find_block_idx("key999"), -1);
}

// 测试元数据
TEST_F(SSTTest, Metadata) {
  auto sst = create_test_sst(512, 10);

  // 验证block数量
  EXPECT_GT(sst->num_blocks(), 0);

  // 验证首尾key
  EXPECT_EQ(sst->get_first_key(), "key0");
  EXPECT_EQ(sst->get_last_key(), "key9");
}