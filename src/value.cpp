#include "value.h"
#include <iostream>

#include "doctest.h"
#include "test_utils.h"

void print_value(Value value) {
  switch (value.type) {
  case ValueType::Number:
    std::cout << std::format("{:g}", value.as_number());
    break;
  case ValueType::Nil:
    std::cout << "nil";
    break;
  case ValueType::Boolean:
    std::cout << std::format("{}", value.as_boolean() ? "true" : "false");
    break;
  }
}

TEST_CASE("print_value") {
  SUBCASE("prints integer-valued double without decimal point") {
    CHECK(capture_stdout([] { print_value(Value::number(3.0)); }) == "3");
  }

  SUBCASE("prints fractional double") {
    CHECK(capture_stdout([] { print_value(Value::number(3.14)); }) == "3.14");
  }

  SUBCASE("prints negative value") {
    CHECK(capture_stdout([] { print_value(Value::number(-1.5)); }) == "-1.5");
  }
}
