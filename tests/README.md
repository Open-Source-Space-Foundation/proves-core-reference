# Unit Tests

This directory contains vanilla C++ unit tests that do not depend on F Prime or Zephyr libraries. These tests can be run independently of the embedded firmware build system.

## Running Tests

```bash
make test-unit
```

## Test Framework

- **Framework**: Google Test (gtest)
- **Language**: C++ only
- **Dependencies**: None (no F Prime, no Zephyr)
- **Purpose**: Test pure C++ logic and algorithms independently of embedded systems

## Test Structure

Tests in this directory should:

- Focus on pure C++ components and utilities
- Not include F Prime headers (`Fw/`, `Svc/`, etc.)
- Not include Zephyr headers (`<zephyr/...>`)
- Use Google Test assertions (`EXPECT_EQ`, `ASSERT_TRUE`, etc.)
- Be fast and deterministic

## Adding New Tests

1. Create a new `.cpp` file in this directory
2. Include Google Test: `#include <gtest/gtest.h>`
3. Write test cases using `TEST()` or `TEST_F()` macros
4. Run `make test-unit` to verify

## Examples

```cpp
#include <gtest/gtest.h>

TEST(MyComponentTest, BasicFunctionality) {
    EXPECT_EQ(2 + 2, 4);
}
```

For integration tests that require F Prime and hardware, see `FprimeZephyrReference/test/int/`.
