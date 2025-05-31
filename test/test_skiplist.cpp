#include "skiplist.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>
#include <iomanip>
#include <latch>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

// 测试基本插入、查找和删除
TEST(SkipListTest, T) {
  std::shared_ptr<int> p(nullptr);
  if (p == nullptr) {
    std::cout << 1;
  }
}

// 测试基本插入、查找和删除
TEST(SkipListTest, BasicOperations) {
  SkipList skipList;

  // 测试插入和查找
  skipList.put("key1", "value1", 0);
  EXPECT_EQ(skipList.get("key1", 0).get_value(), "value1");
  //
  // // 测试更新
  skipList.put("key1", "new_value", 0);
  EXPECT_EQ(skipList.get("key1", 0).get_value(), "new_value");
  skipList.print_skiplist();

  // 测试删除
  skipList.remove("key1");
  EXPECT_FALSE(skipList.get("key1", 0).is_valid());
}

// 测试迭代器
TEST(SkipListTest, Iterator) {
  SkipList skipList;
  skipList.put("key1", "value1", 0);
  skipList.put("key2", "value2", 0);
  skipList.put("key3", "value3", 0);

  // 测试迭代器
  std::vector<std::pair<std::string, std::string>> result;
  for (auto it = skipList.begin(); it != skipList.end(); ++it) {
    result.push_back(*it);
  }

  EXPECT_EQ(result.size(), 3);
  EXPECT_EQ(std::get<0>(result[0]), "key1");
  EXPECT_EQ(std::get<0>(result[1]), "key2");
  EXPECT_EQ(std::get<0>(result[2]), "key3");
}

// 测试大量数据插入和查找
TEST(SkipListTest, LargeScaleInsertAndGet) {
  SkipList skipList;
  const int num_elements = 10000;

  // 插入大量数据
  for (int i = 0; i < num_elements; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string value = "value" + std::to_string(i);
    skipList.put(key, value, 0);
  }

  // 验证插入的数据
  for (int i = 0; i < num_elements; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string expected_value = "value" + std::to_string(i);
    EXPECT_EQ((skipList.get(key, 0).get_value()), expected_value);
  }
}

// 测试大量数据删除
TEST(SkipListTest, LargeScaleRemove) {
  SkipList skipList;
  const int num_elements = 10000;

  // 插入大量数据
  // std::cout << "********************** insert **********************"
  //           << std::endl;
  for (int i = 0; i < num_elements; ++i) {
    std::string key = "key" + std::to_string(i);
    std::string value = "value" + std::to_string(i);
    skipList.put(key, value, 0);

    // skipList.print_skiplist();
  }

  // std::cout << "********************** remove **********************"
  // << std::endl;
  // 删除所有数据
  for (int i = 0; i < num_elements; ++i) {
    std::string key = "key" + std::to_string(i);
    skipList.remove(key);

    // skipList.print_skiplist();
  }

  // 验证所有数据已被删除
  for (int i = 0; i < num_elements; ++i) {
    std::string key = "key" + std::to_string(i);
    EXPECT_FALSE(skipList.get(key, 0).is_valid());
  }
}

// 测试重复插入
TEST(SkipListTest, DuplicateInsert) {
  SkipList skipList;

  // 重复插入相同的key
  skipList.put("key1", "value1", 0);
  skipList.put("key1", "value2", 0);
  skipList.put("key1", "value3", 0);

  // 验证最后一次插入的值
  EXPECT_EQ((skipList.get("key1", 0).get_value()), "value3");
}

// 测试空跳表
TEST(SkipListTest, EmptySkipList) {
  SkipList skipList;

  // 验证空跳表的查找和删除
  EXPECT_FALSE(skipList.get("nonexistent_key", 0).is_valid());
  skipList.remove("nonexistent_key"); // 删除不存在的key
}

// 测试随机插入和删除
TEST(SkipListTest, RandomInsertAndRemove) {
  SkipList skipList;
  std::unordered_set<std::string> keys;
  const int num_operations = 10000;

  for (int i = 0; i < num_operations; ++i) {
    std::string key = "key" + std::to_string(rand() % 1000);
    std::string value = "value" + std::to_string(rand() % 1000);

    if (keys.find(key) == keys.end()) {
      // 插入新key
      skipList.put(key, value, 0);
      keys.insert(key);
    } else {
      // 删除已存在的key
      skipList.remove(key);
      keys.erase(key);
    }

    // 验证当前状态
    if (keys.find(key) != keys.end()) {
      EXPECT_EQ((skipList.get(key, 0).get_value()), value);
    } else {
      EXPECT_FALSE(skipList.get(key, 0).is_valid());
    }
  }
}

// 测试内存大小跟踪
TEST(SkipListTest, MemorySizeTracking) {
  SkipList skipList;

  // 插入数据
  skipList.put("key1", "value1", 0);
  skipList.put("key2", "value2", 0);

  // 验证内存大小
  size_t expected_size = sizeof("key1") - 1 + sizeof("value1") - 1 +
                         sizeof(uint64_t) + sizeof("key2") - 1 +
                         sizeof("value2") - 1 + sizeof(uint64_t);
  EXPECT_EQ(skipList.get_size(), expected_size);

  // 删除数据
  skipList.remove("key1");
  expected_size -= sizeof("key1") - 1 + sizeof("value1") - 1 + sizeof(uint64_t);
  EXPECT_EQ(skipList.get_size(), expected_size);

  skipList.clear();
  EXPECT_EQ(skipList.get_size(), 0);
}

TEST(SkipListTest, IteratorPreffix) {
  SkipList skipList;

  // 插入一些测试数据
  skipList.put("apple", "0", 0);
  skipList.put("apple2", "1", 0);
  skipList.put("apricot", "2", 0);
  skipList.put("banana", "3", 0);
  skipList.put("berry", "4", 0);
  skipList.put("cherry", "5", 0);
  skipList.put("cherry2", "6", 0);

  // 测试前缀 "ap"
  auto it = skipList.begin_preffix("ap");
  EXPECT_EQ(it.get_key(), "apple");

  // 测试前缀 "ba"
  it = skipList.begin_preffix("ba");
  EXPECT_EQ(it.get_key(), "banana");

  // 测试前缀 "ch"
  it = skipList.begin_preffix("ch");
  EXPECT_EQ(it.get_key(), "cherry");

  // 测试前缀 "z"
  it = skipList.begin_preffix("z");
  EXPECT_TRUE(it == skipList.end());

  // 测试前缀 "berr"
  it = skipList.begin_preffix("berr");
  EXPECT_EQ(it.get_key(), "berry");

  // 测试前缀 "a"
  it = skipList.begin_preffix("a");
  EXPECT_EQ(it.get_key(), "apple");

  // 测试前缀结束位置
  it = skipList.end_preffix("a");
  EXPECT_EQ(it.get_key(), "banana");

  it = skipList.end_preffix("cherry");
  EXPECT_TRUE(it == skipList.end());

  EXPECT_EQ(skipList.begin_preffix("not exist"),
            skipList.end_preffix("not exist"));
}


// int pred_range(const std::string &key) {
//   auto range = std::make_pair("l", "n"); // [l, n)
//   if (key < range.first) {
//     return 1;
//   } else if (key >= range.second) {
//     return -1;
//   } else {
//     return 0;
//   }
// }

TEST(SkipListTest, ItersPredicate_Base) {

  SkipList skipList;
  skipList.put("prefix1", "value1", 0);
  skipList.put("prefix2", "value2", 0);
  skipList.put("prefix3", "value3", 0);
  skipList.put("other", "value4", 0);
  skipList.put("longerkey", "value5", 0);
  skipList.put("averylongkey", "value6", 0);
  skipList.put("medium", "value7", 0);
  skipList.put("midway", "value8", 0);
  skipList.put("midpoint", "value9", 0);
  skipList.print_skiplist();
  // 测试前缀匹配
  auto prefix_result =
      skipList.iters_monotony_predicate([](const std::string &key) {
        auto match_str = key.substr(0, 3);
        if (match_str == "pre") {
          return 0;
        } else if (match_str < "pre") {
          return 1;
        }
        return -1;
      });
  ASSERT_TRUE(prefix_result.has_value());
  auto [prefix_begin_iter, prefix_end_iter] = prefix_result.value();
  EXPECT_EQ(prefix_begin_iter.get_key(), "prefix1");
  EXPECT_TRUE(prefix_end_iter.is_end());

  EXPECT_EQ(prefix_begin_iter.get_value(), "value1");
  ++prefix_begin_iter;
  EXPECT_EQ(prefix_begin_iter.get_value(), "value2");
  ++prefix_begin_iter;
  EXPECT_EQ(prefix_begin_iter.get_value(), "value3");

  // 测试范围匹配
  auto range = std::make_pair("l", "n"); // [l, n)
  auto range_result =
      skipList.iters_monotony_predicate([&range](const std::string &key) {
        if (key < range.first) {
          return 1;
        } else if (key >= range.second) {
          return -1;
        } else {
          return 0;
        }
      });

  ASSERT_TRUE(range_result.has_value());
  auto [range_begin_iter, range_end_iter] = range_result.value();
  EXPECT_EQ(range_end_iter.get_key(),
            "other"); // end_iter 是开区间，所以指向 "prefix1"
  EXPECT_EQ(range_begin_iter.get_key(), "longerkey");
  ++range_begin_iter;
  EXPECT_EQ(range_begin_iter.get_key(), "medium");
  ++range_begin_iter;
  EXPECT_EQ(range_begin_iter.get_key(), "midpoint");
  ++range_begin_iter;
  EXPECT_EQ(range_begin_iter.get_key(), "midway");
}

TEST(SkipListTest, ItersPredicate_Large) {
  SkipList skipList;
  int num = 10000;

  for (int i = 0; i < num; ++i) {
    std::ostringstream oss_key;
    std::ostringstream oss_value;

    // 设置数字为4位长度，不足的部分用前导零填充
    oss_key << "key" << std::setw(4) << std::setfill('0') << i;
    oss_value << "value" << std::setw(4) << std::setfill('0') << i;

    std::string key = oss_key.str();
    std::string value = oss_value.str();

    skipList.put(key, value, 0);
  }

  skipList.remove("key1015");

  auto result = skipList.iters_monotony_predicate([](const std::string &key) {
    if (key < "key1010") {
      return 1;
    } else if (key >= "key1020") {
      return -1;
    } else {
      return 0;
    }
  });

  ASSERT_TRUE(result.has_value());
  auto [range_begin_iter, range_end_iter] = result.value();
  EXPECT_EQ(range_begin_iter.get_key(), "key1010");
  EXPECT_EQ(range_end_iter.get_key(), "key1020");
  for (int i = 0; i < 5; i++) {
    ++range_begin_iter;
  }
  EXPECT_EQ(range_begin_iter.get_key(), "key1016");
}

// 测试包含事务 id 的插入和查找
TEST(SkipListTest, TransactionId) {
  SkipList skipList;
  skipList.put("key1", "value1", 1);
  skipList.put("key1", "value2", 2);

  // 验证事务 id
  // 不指定事务 id，应该返回最新的值
  EXPECT_EQ((skipList.get("key1", 0).get_value()), "value2");
  // 指定 1 表示只能查找事务 id 小于等于 1 的值
  EXPECT_EQ((skipList.get("key1", 1).get_value()), "value1");
  // 指定 2 表示只能查找事务 id 小于等于 2 的值
  EXPECT_EQ((skipList.get("key1", 2).get_value()), "value2");
}