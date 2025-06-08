#include "block_iterator.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "block.h"

class Block;

// todo
BlockIterator::BlockIterator(std::shared_ptr<Block> b, size_t index,
                             uint64_t tranc_id)
    : block(b),
      current_index(index),
      tranc_id_(tranc_id),
      cached_value(std::nullopt) {
  skip_by_tranc_id();
  update_current();
}

// 假设了key在当前block中
BlockIterator::BlockIterator(std::shared_ptr<Block> b, const std::string &key,
                             uint64_t tranc_id)
    : block(b), tranc_id_(tranc_id), cached_value(std::nullopt) {
  // todo
  auto idx_opt = block->get_idx_binary(key, tranc_id);
  if (idx_opt.has_value()) {
    current_index = idx_opt.value();
    update_current();
  } else {
    // throw std::runtime_error("BlockIterator:key doesn't exits!");
    block.reset();
  }
}

BlockIterator::pointer BlockIterator::operator->() const {
  // todo
  if (!cached_value.has_value()) {
    update_current();
  }
  return &cached_value.value();
}

BlockIterator &BlockIterator::operator++() {
  // todo
  if (block == nullptr || current_index >= block->size()) return *this;
  if (tranc_id_ == 0) {
    while (current_index < block->size()) {
      current_index++;
      const auto &curr_key =
          block->get_key_at(block->get_offset_at(current_index));
      if (prev_keys_.find(curr_key) == prev_keys_.end()) {
        update_current();
        prev_keys_.insert(curr_key);
        break;
      }
    }
  } else {
    while (current_index < block->size()) {
      current_index++;
      auto curr_tranc =
          block->get_tranc_id_at(block->get_offset_at(current_index));
      const auto &curr_key =
          block->get_key_at(block->get_offset_at(current_index));
      if (curr_tranc <= tranc_id_ && prev_keys_.find(curr_key) == prev_keys_.end()) {
        update_current();
        prev_keys_.insert(curr_key);
        break;
      }
    }
  }

  return *this;
}

bool BlockIterator::operator==(const BlockIterator &other) const {
  // todo
  if (block == nullptr && other.block == nullptr) return true;
  if ((block && other.block == nullptr) || (block == nullptr && other.block))
    return false;
  if (block == other.block && current_index == other.current_index &&
      tranc_id_ == other.tranc_id_)
    return true;
  return false;
}

bool BlockIterator::operator!=(const BlockIterator &other) const {
  // todo
  return !(*this == other);
}

BlockIterator::value_type BlockIterator::operator*() const {
  // todo
  if (block == nullptr) throw std::runtime_error("BlockIterator:* on nullptr");
  if (!cached_value.has_value()) {
    update_current();
  }
  return {cached_value.value().first, cached_value.value().second};
}

bool BlockIterator::is_end() const {  // todo
  if (block == nullptr) return true;
  return false;
}

void BlockIterator::update_current() const {
  if (block == nullptr) return;
  if (current_index < block->size()) {
    const auto &curr_key =
        block->get_key_at(block->get_offset_at(current_index));
    const auto &curr_val =
        block->get_value_at(block->get_offset_at(current_index));
    cached_value = {curr_key, curr_val};
  }
}

void BlockIterator::skip_by_tranc_id() {
  if (block == nullptr || tranc_id_ == 0) return;

  while (current_index < block->size() &&
         tranc_id_ <
             block->get_tranc_id_at(block->get_offset_at(current_index))) {
    current_index++;
  }
}
