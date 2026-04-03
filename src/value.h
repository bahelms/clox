#pragma once

#include "object.h"

enum class ValueType { Boolean, Nil, Number, Object };

bool values_equal(Value a, Value b);
bool is_falsey(Value value);

// using manual union instead of std::variant to NaN box later on
struct Value {
  ValueType type;
  union {
    bool boolean;
    double number;
    Object *object;
  } as;

  static Value boolean(bool v) { return {ValueType::Boolean, {.boolean = v}}; }
  bool as_boolean() const { return as.boolean; }
  bool is_boolean() { return type == ValueType::Boolean; }

  static Value nil() { return {ValueType::Nil, {.number = 0}}; }
  bool is_nil() { return type == ValueType::Nil; }

  static Value number(double v) { return {ValueType::Number, {.number = v}}; }
  double as_number() const { return as.number; }
  bool is_number() { return type == ValueType::Number; }

  // static Value object(Object *v) { return {ValueType::Object, {.object = v}};
  // }
  static Value object(ObjString &&obj_str) {
    return {ValueType::Object, {.object = new ObjString(std::move(obj_str))}};
  }
  Object *as_object() const { return as.object; }
  bool is_object() { return type == ValueType::Object; }

  bool is_string() {
    return is_object() && as_object()->type == ObjectType::String;
  }
  ObjString *as_string() const { return static_cast<ObjString *>(as_object()); }
  // #define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
};

void print_value(Value value);
