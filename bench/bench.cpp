#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "engine.h"
// 随机字符串生成器
std::string random_string(size_t length) {
  static const std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  static thread_local std::mt19937_64 rng(std::random_device{}());
  static thread_local std::uniform_int_distribution<size_t> dist(
      0, chars.size() - 1);

  std::string s;
  s.reserve(length);
  for (size_t i = 0; i < length; ++i) {
    s += chars[dist(rng)];
  }
  return s;
}

// 生成随机键值对
std::vector<std::pair<std::string, std::string>> generate_kvs(size_t count,
                                                              size_t key_len,
                                                              size_t val_len) {
  std::vector<std::pair<std::string, std::string>> kvs;
  kvs.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    kvs.emplace_back(random_string(key_len), random_string(val_len));
  }
  return kvs;
}

int main() {
  const size_t N = 30000;  // 写入 30 万个键值对
  const size_t M = 10000;   // 读取 1 万个随机键
  const size_t key_len = 16;
  const size_t val_len = 1000;  // value 更大，总体接近 300MB

  const std::string db_path = "./test_lsm_db";

  LSM db(db_path);

  // 生成键值对
  std::cout << "Generating random key-value pairs...\n";
  auto kvs = generate_kvs(N, key_len, val_len);

  // 写入测试
  std::cout << "Starting write test...\n";
  size_t total_write_bytes = 0;
  for (const auto& [k, v] : kvs) total_write_bytes += k.size() + v.size();

  auto start_write = std::chrono::steady_clock::now();
  db.put_batch(kvs);
  db.flush();
  auto end_write = std::chrono::steady_clock::now();

  double write_time =
      std::chrono::duration<double>(end_write - start_write).count();
  double write_mb = static_cast<double>(total_write_bytes) / 1'000'000.0;

  std::cout << "Wrote " << N << " entries (≈ " << write_mb << " MB) in "
            << write_time << " seconds.\n";
  std::cout << "Write throughput: " << write_mb / write_time << " MB/s\n";

  // 读取测试
  std::cout << "Starting read test...\n";
  std::vector<std::string> random_keys;
  random_keys.reserve(M);
  std::mt19937_64 rng(std::random_device{}());
  std::uniform_int_distribution<size_t> dist(0, N - 1);
  for (size_t i = 0; i < M; ++i) {
    random_keys.push_back(kvs[dist(rng)].first);
  }

  auto start_read = std::chrono::steady_clock::now();
  auto results = db.get_batch(random_keys);
  auto end_read = std::chrono::steady_clock::now();

  size_t total_read_bytes = 0;
  for (const auto& [k, v] : results)
    total_read_bytes += k.size() + (v ? v->size() : 0);

  double read_time =
      std::chrono::duration<double>(end_read - start_read).count();
  double read_mb = static_cast<double>(total_read_bytes) / 1'000'000.0;

  std::cout << "Read " << M << " entries (≈ " << read_mb << " MB) in "
            << read_time << " seconds.\n";
  std::cout << "Read throughput: " << read_mb / read_time << " MB/s\n";
}
