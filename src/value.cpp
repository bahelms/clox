#include "value.h"
#include <iostream>

#include "doctest.h"
#include "test_utils.h"

bool is_falsey(Value value) {
  return value.is_nil() || (value.is_boolean() && !value.as_boolean());
}

bool values_equal(const Value a, const Value b) {
  if (a.type != b.type) {
    return false;
  }

  switch (a.type) {
  case ValueType::Boolean:
    return a.as_boolean() == b.as_boolean();
  case ValueType::Number:
    return a.as_number() == b.as_number();
  case ValueType::Object:
    return a.as_string()->chars == b.as_string()->chars;
  case ValueType::Nil:
    return true;
  }
}

void print_value(Value value) {
  switch (value.type) {
  case ValueType::Number:
    std::cout << std::format("{:g}", value.as_number());
    break;
  case ValueType::Object:
    print_object(value);
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
