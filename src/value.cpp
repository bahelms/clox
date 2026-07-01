#include "value.h"
#include <print>

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
  case ValueType::Object: {
    Object *obj_a = a.as_object();
    Object *obj_b = b.as_object();
    if (obj_a->type != obj_b->type) {
      return false;
    }
    switch (obj_a->type) {
    case Object::Type::String:
      return a.as_string()->chars == b.as_string()->chars;
    case Object::Type::Function:
      return obj_a == obj_b;
    case Object::Type::Closure:
      return obj_a == obj_b;
    case Object::Type::Native:
      return obj_a == obj_b;
    }
  }
  case ValueType::Nil:
    return true;
  }
}

void print_value(Value value) {
  switch (value.type) {
  case ValueType::Number:
    std::print("{:g}", value.as_number());
    break;
  case ValueType::Object:
    print_object(value);
    break;
  case ValueType::Nil:
    std::print("nil");
    break;
  case ValueType::Boolean:
    std::print("{}", value.as_boolean() ? "true" : "false");
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

TEST_CASE("Value: function object helpers") {
  ObjFunction fn;
  Value value = Value::object(&fn);
  CHECK(value.is_object());
  CHECK(value.is_function());
  CHECK_FALSE(value.is_string());
  CHECK(value.as_function() == &fn);
}

TEST_CASE("values_equal") {
  SUBCASE("distinct function objects are not equal") {
    ObjFunction a;
    ObjFunction b;
    CHECK_FALSE(values_equal(Value::object(&a), Value::object(&b)));
  }

  SUBCASE("a function object equals itself") {
    ObjFunction fn;
    CHECK(values_equal(Value::object(&fn), Value::object(&fn)));
  }

  SUBCASE("strings with equal contents are equal") {
    ObjString a{"hello"};
    ObjString b{"hello"};
    CHECK(values_equal(Value::object(&a), Value::object(&b)));
  }

  SUBCASE("strings with different contents are not equal") {
    ObjString a{"hello"};
    ObjString b{"world"};
    CHECK_FALSE(values_equal(Value::object(&a), Value::object(&b)));
  }

  SUBCASE("a string and a function are not equal") {
    ObjString s{"hello"};
    ObjFunction fn;
    CHECK_FALSE(values_equal(Value::object(&s), Value::object(&fn)));
  }
}
