#pragma once

#include <folly/FBString.h>

using namespace folly;

typedef struct TestStruct {
  int32_t id;
  fbstring message;
} TestStruct;
