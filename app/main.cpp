#include <iostream>
#include <set>

#include "db.h"
#include "mem_table.h"

int main() {
  TurkeyDB::MemTableKey key00(0, TurkeyDB::WRITE_TYPE::K_WRITE, "key0", "val");
  TurkeyDB::MemTableKey key01(1, TurkeyDB::WRITE_TYPE::K_SEARCH, "key0",
                              "val1");

  return 0;
}
