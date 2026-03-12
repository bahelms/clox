#include "value.h"
#include <iostream>

#include "doctest.h"
#include "test_utils.h"

void print_value(Value value) {
  std::cout << std::format("Value: {:g}", value);
}

TEST_CASE("print_value") {
  SUBCASE("prints integer-valued double without decimal point") {
    CHECK(capture_stdout([] { print_value(3.0); }) == "Value: 3");
  }

  SUBCASE("prints fractional double") {
    CHECK(capture_stdout([] { print_value(3.14); }) == "Value: 3.14");
  }

  SUBCASE("prints negative value") {
    CHECK(capture_stdout([] { print_value(-1.5); }) == "Value: -1.5");
  }
}
