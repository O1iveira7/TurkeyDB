//// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
//// Use of this source code is governed by a BSD-style license that can be
//// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
//
//// Copyright (c) 2025 Oliveira
////
//// Licensed under the Apache License, Version 2.0 (the "License");
//// you may not use this file except in compliance with the License.
//// You may obtain a copy of the License at
////
////     http://www.apache.org/licenses/LICENSE-2.0
////
//// Unless required by applicable law or agreed to in writing, software
//// distributed under the License is distributed on an "AS IS" BASIS,
//// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//// See the License for the specific language governing permissions and
//// limitations under the License.
//
//#ifndef STORAGE_LEVELDB_DB_SKIPLIST_H_
//#define STORAGE_LEVELDB_DB_SKIPLIST_H_
//
//// Thread safety
//// -------------
////
//// Writes require external synchronization, most likely a mutex.
//// Reads require a guarantee that the SkipList will not be destroyed
//// while the read is in progress.  Apart from that, reads progress
//// without any internal locking or synchronization.
////
//// Invariants:
////
//// (1) Allocated nodes are never deleted until the SkipList is
//// destroyed.  This is trivially guaranteed by the code since we
//// never delete any skip list nodes.
////
//// (2) The contents of a Node except for the next/prev pointers are
//// immutable after the Node has been linked into the SkipList.
//// Only Insert() modifies the list, and it is careful to initialize
//// a node and use release-stores to publish the nodes in one or
//// more lists.
////
//// ... prev vs. next pointer ordering ...
//
//#include <assert.h>
//#include <stdlib.h>
//#include <iostream>
//#include "port/port.h"
//#include "util/arena.h"
//#include "util/random.h"
//
//namespace leveldb {
//
//class Arena;
//
////Comparator.operator(lh, rh)返回值说明:
////lh < rh，返回-1
////lh > rh，返回+1
////lh = rh，返回0
////
//template<typename Key, class Comparator>
//class SkipList {
// private:
//  struct Node;
//
// public:
//  // Create a new SkipList object that will use "cmp" for comparing keys,
//  // and will allocate memory using "*arena".  Objects allocated in the arena
//  // must remain allocated for the lifetime of the skiplist object.
//  explicit SkipList(Comparator cmp, Arena* arena);
//
//  // Insert key into the list.
//  // REQUIRES: nothing that compares equal to key is currently in the list.
//  void Insert(const Key& key);
//
//  // Returns true iff an entry that compares equal to key is in the list.
//  bool Contains(const Key& key) const;
//
//  //REQUIRES: Key.operator<< implemented.
//  void dump() const;
//
//  // Iteration over the contents of a skip list
//  class Iterator {
//   public:
//    // Initialize an iterator over the specified list.
//    // The returned iterator is not valid.
//    explicit Iterator(const SkipList* list);
//
//    // Returns true iff the iterator is positioned at a valid node.
//    bool Valid() const;
//
//    // Returns the key at the current position.
//    // REQUIRES: Valid()
//    const Key& key() const;
//
//    // Advances to the next position.
//    // REQUIRES: Valid()
//    void Next();
//
//    // Advances to the previous position.
//    // REQUIRES: Valid()
//    void Prev();
//
//    // Advance to the first entry with a key >= target
//    void Seek(const Key& target);
//
//    // Position at the first entry in list.
//    // Final state of iterator is Valid() iff list is not empty.
//    void SeekToFirst();
//
//    // Position at the last entry in list.
//    // Final state of iterator is Valid() iff list is not empty.
//    void SeekToLast();
//
//   private:
//    const SkipList* list_;
//    Node* node_;
//    // Intentionally copyable
//  };
//
// private:
//  enum { kMaxHeight = 12 };
//
//  // Immutable after construction
//  Comparator const compare_;
//  //leveldb内存池
//  Arena* const arena_;    // Arena used for allocations of nodes
//
//  Node* const head_;
//
//  // Modified only by Insert().  Read racily by readers, but stale
//  // values are ok.
//  port::AtomicPointer max_height_;   // Height of the entire list
//
//  inline int GetMaxHeight() const {
//    return static_cast<int>(
//        reinterpret_cast<intptr_t>(max_height_.NoBarrier_Load()));
//  }
//
//  // Read/written only by Insert().
//  // 随机数产生器，用于决定skiplist节点有几层
//  Random rnd_;
//
//  Node* NewNode(const Key& key, int height);
//  int RandomHeight();//通过抛硬币的方法(1/4概率)决定高度值，范围[1, kMaxHeight]
//  bool Equal(const Key& a, const Key& b) const { return (compare_(a, b) == 0); }
//
//  // Return true if key is greater than the data stored in "n"
//  bool KeyIsAfterNode(const Key& key, Node* n) const;
//
//  // Return the earliest node that comes at or after key.
//  // Return nullptr if there is no such node.
//  //
//  // If prev is non-null, fills prev[level] with pointer to previous
//  // node at "level" for every level in [0..max_height_-1].
//  // 找到第一个>=key的Node
//  // 如果prev不为nullptr，则记录每层[0..max_height_-1]最后一个<key的Node
//  Node* FindGreaterOrEqual(const Key& key, Node** prev) const;
//
//  // Return the latest node with a key < key.
//  // Return head_ if there is no such node.
//  Node* FindLessThan(const Key& key) const;
//
//  // Return the last node in the list.
//  // Return head_ if list is empty.
//  Node* FindLast() const;
//
//  // No copying allowed
//  SkipList(const SkipList&);
//  void operator=(const SkipList&);
//};
//
//// Implementation details follow
//template<typename Key, class Comparator>
//struct SkipList<Key,Comparator>::Node {
//  explicit Node(const Key& k) : key(k) { }
//
//  Key const key;//数据本身
//
//  //获取或者设置该节点在第n层的后继节点
//  // Accessors/mutators for links.  Wrapped in methods so we can
//  // add the appropriate barriers as necessary.
//  Node* Next(int n) {
//    assert(n >= 0);
//    // Use an 'acquire load' so that we observe a fully initialized
//    // version of the returned Node.
//    return reinterpret_cast<Node*>(next_[n].Acquire_Load());
//  }
//  void SetNext(int n, Node* x) {
//    assert(n >= 0);
//    // Use a 'release store' so that anybody who reads through this
//    // pointer observes a fully initialized version of the inserted node.
//    next_[n].Release_Store(x);
//  }
//
//  // No-barrier variants that can be safely used in a few locations.
//  Node* NoBarrier_Next(int n) {
//    assert(n >= 0);
//    return reinterpret_cast<Node*>(next_[n].NoBarrier_Load());
//  }
//  void NoBarrier_SetNext(int n, Node* x) {
//    assert(n >= 0);
//    next_[n].NoBarrier_Store(x);
//  }
//
// private:
//  // Array of length equal to the node height.  next_[0] is lowest level link.
//  // 作为Node的最后一个成员变量
//  // 由于Node通过placement new的方式构造，因此next_实际上是一个不定长的数组
//  // 数组长度即该节点的高度
//  // next_记录了该节点在所有层的后继节点，0是最底层链表。
//  port::AtomicPointer next_[1];
//};
//
//template<typename Key, class Comparator>
//typename SkipList<Key,Comparator>::Node*
//SkipList<Key,Comparator>::NewNode(const Key& key, int height) {
//    //额外存储(height - 1)个port::AtomicPointer
//  char* mem = arena_->AllocateAligned(
//      sizeof(Node) + sizeof(port::AtomicPointer) * (height - 1));
//  return new (mem) Node(key);
//}
//
//template<typename Key, class Comparator>
//inline SkipList<Key,Comparator>::Iterator::Iterator(const SkipList* list) {
//  list_ = list;
//  node_ = nullptr;
//}
//
//template<typename Key, class Comparator>
//inline bool SkipList<Key,Comparator>::Iterator::Valid() const {
//  return node_ != nullptr;
//}
//
//template<typename Key, class Comparator>
//inline const Key& SkipList<Key,Comparator>::Iterator::key() const {
//  assert(Valid());
//  return node_->key;
//}
//
//template<typename Key, class Comparator>
//inline void SkipList<Key,Comparator>::Iterator::Next() {
//  assert(Valid());
//  node_ = node_->Next(0);
//}
//
//template<typename Key, class Comparator>
//inline void SkipList<Key,Comparator>::Iterator::Prev() {
//  // Instead of using explicit "prev" links, we just search for the
//  // last node that falls before key.
//  assert(Valid());
//  node_ = list_->FindLessThan(node_->key);
//  if (node_ == list_->head_) {
//    node_ = nullptr;
//  }
//}
//
//template<typename Key, class Comparator>
//inline void SkipList<Key,Comparator>::Iterator::Seek(const Key& target) {
//  node_ = list_->FindGreaterOrEqual(target, nullptr);
//}
//
//template<typename Key, class Comparator>
//inline void SkipList<Key,Comparator>::Iterator::SeekToFirst() {
//  node_ = list_->head_->Next(0);
//}
//
//template<typename Key, class Comparator>
//inline void SkipList<Key,Comparator>::Iterator::SeekToLast() {
//  node_ = list_->FindLast();
//  if (node_ == list_->head_) {
//    node_ = nullptr;
//  }
//}
//
//template<typename Key, class Comparator>
//int SkipList<Key,Comparator>::RandomHeight() {
//  // Increase height with probability 1 in kBranching
//  static const unsigned int kBranching = 4;
//  int height = 1;
//  while (height < kMaxHeight && ((rnd_.Next() % kBranching) == 0)) {
//    height++;
//  }
//  assert(height > 0);
//  assert(height <= kMaxHeight);
//  return height;
//}
//
//template<typename Key, class Comparator>
//bool SkipList<Key,Comparator>::KeyIsAfterNode(const Key& key, Node* n) const {
//  // null n is considered infinite
//  return (n != nullptr) && (compare_(n->key, key) < 0);
//}
//
//template<typename Key, class Comparator>
//typename SkipList<Key,Comparator>::Node* SkipList<Key,Comparator>::FindGreaterOrEqual(const Key& key, Node** prev)
//    const {
//  Node* x = head_;
//  int level = GetMaxHeight() - 1;
//  while (true) {
//    Node* next = x->Next(level);
//    if (KeyIsAfterNode(key, next)) {//如果next->key < key
//      // Keep searching in this list
//      x = next;
//    } else {//如果next->key >= key，如果单纯为了判断是否相等，这里可以加一个判断直接返回了，没必要level--到0再返回
//      if (prev != nullptr) prev[level] = x;//prev记录该level最后一个<key的节点
//      if (level == 0) {//到达最底层则返回next (next是第一个>=key的节点)
//        return next;
//      } else {
//        // Switch to next list
//        level--;
//      }
//    }
//  }
//}
//
//template<typename Key, class Comparator>
//typename SkipList<Key,Comparator>::Node*
//SkipList<Key,Comparator>::FindLessThan(const Key& key) const {
//  Node* x = head_;
//  int level = GetMaxHeight() - 1;//获取当前最大height
//  while (true) {
//    assert(x == head_ || compare_(x->key, key) < 0);
//    Node* next = x->Next(level);
//    if (next == nullptr || compare_(next->key, key) >= 0) {//如果key >= next->key
//      if (level == 0) {
//        return x;
//      } else {
//        // Switch to next list
//        level--;
//      }
//    } else {
//      x = next;
//    }
//  }
//}
//
//template<typename Key, class Comparator>
//typename SkipList<Key,Comparator>::Node* SkipList<Key,Comparator>::FindLast()
//    const {
//  Node* x = head_;
//  int level = GetMaxHeight() - 1;
//  while (true) {
//    Node* next = x->Next(level);
//    if (next == nullptr) {//到达最后一个节点
//      if (level == 0) {//level=0，则返回
//        return x;
//      } else {
//        // Switch to next list
//        // level>0则到下一层
//        level--;
//      }
//    } else {
//        //不是最后一个节点则继续递增
//      x = next;
//    }
//  }
//}
//
//template<typename Key, class Comparator>
//SkipList<Key,Comparator>::SkipList(Comparator cmp, Arena* arena)
//    : compare_(cmp),
//      arena_(arena),
//      head_(NewNode(0 /* any key will do */, kMaxHeight)),
//      max_height_(reinterpret_cast<void*>(1)),
//      rnd_(0xdeadbeef) {
//  for (int i = 0; i < kMaxHeight; i++) {
//    head_->SetNext(i, nullptr);
//  }
//}
//
//template<typename Key, class Comparator>
//void SkipList<Key,Comparator>::Insert(const Key& key) {
//  // TODO(opt): We can use a barrier-free variant of FindGreaterOrEqual()
//  // here since Insert() is externally synchronized.
//  Node* prev[kMaxHeight];
//  //prev记录每一层最大一个 < key的节点，也就是待插入节点的前驱节点
//  Node* x = FindGreaterOrEqual(key, prev);
//
//  // Our data structure does not allow duplicate insertion
//  assert(x == nullptr || !Equal(key, x->key));
//
//  //随机决定节点高度height
//  int height = RandomHeight();
//  //如果新的高度比当前所有节点高度都大，那么填充prev更高层为head_，同时更新max_height_
//  if (height > GetMaxHeight()) {
//    for (int i = GetMaxHeight(); i < height; i++) {
//      prev[i] = head_;
//    }
//    //fprintf(stderr, "Change height from %d to %d\n", max_height_, height);
//
//    // It is ok to mutate max_height_ without any synchronization
//    // with concurrent readers.  A concurrent reader that observes
//    // the new value of max_height_ will see either the old value of
//    // new level pointers from head_ (nullptr), or a new value set in
//    // the loop below.  In the former case the reader will
//    // immediately drop to the next level since nullptr sorts after all
//    // keys.  In the latter case the reader will use the new node.
//    max_height_.NoBarrier_Store(reinterpret_cast<void*>(height));
//  }
//
//  //构造Node，高度为height
//  x = NewNode(key, height);
//  //插入节点x到prev及prev->next中间
//  for (int i = 0; i < height; i++) {
//    // NoBarrier_SetNext() suffices since we will add a barrier when
//    // we publish a pointer to "x" in prev[i].
//    // 先修改x节点，再修改prev节点
//    x->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));
//    prev[i]->SetNext(i, x);
//  }
//}
//
//template<typename Key, class Comparator>
//bool SkipList<Key,Comparator>::Contains(const Key& key) const {
//  //x记录第一个>= key的Node
//  //注意FindGreaterOrEqual是查找>=key的Node，因此会迭代直到level = 0才返回
//  //实际上可以实现一个接口直接查找==key的Node，这样会在level >=0 时就能返回，查找的时间复杂度不变，不过可以预见减少比较次数。
//  Node* x = FindGreaterOrEqual(key, nullptr);
//  //判断x->key == key
//  if (x != nullptr && Equal(key, x->key)) {
//    return true;
//  } else {
//    return false;
//  }
//}
//
//template<typename Key, class Comparator>
//void SkipList<Key,Comparator>::dump() const {
//    Node* x = head_;
//    int level = GetMaxHeight() - 1;//获取当前最大height
//    for (int i = 0; i < level; ++i) {
//        Node* next = head_->Next(i);
//        while (next != nullptr) {
//            std::cout << next->key << " -> ";
//            next = next->Next(i);
//        }
//        std::cout << "\n";
//    }
//}
//
//}  // namespace leveldb
//
//#endif  // STORAGE_LEVELDB_DB_SKIPLIST_H_
