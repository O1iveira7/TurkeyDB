#include <iostream>
#include "db.h"
std::string_view test_string_view() {
  return "HelloWorld";
}

int main() {
  auto [db,ok] = TurkeyDB::Open("aaa");
  if (!ok.Ok()) {

  }

}
