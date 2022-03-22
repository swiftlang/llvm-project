#include "test_foo.h"

int bar(int x) {
  return x * x;
}

int a() {
  return foo<int>(2);
}
