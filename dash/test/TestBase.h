#ifndef DASH__TEST__TEST_BASE_H_
#define DASH__TEST__TEST_BASE_H_

#include <gtest/gtest.h>

namespace testing {
namespace internal {

enum GTestColor {
    COLOR_DEFAULT,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW
};

extern void ColoredPrintf(
  GTestColor color,
  const char* fmt,
  ...);

} // namespace internal
} // namespace testing

#define LOG_MESSAGE(...) do { \
  testing::internal::ColoredPrintf(\
    testing::internal::COLOR_GREEN, \
    "[          ] "); \
  testing::internal::ColoredPrintf(\
    testing::internal::COLOR_YELLOW, \
    __VA_ARGS__); \
} while(0)

#endif // DASH__TEST__TEST_BASE_H_