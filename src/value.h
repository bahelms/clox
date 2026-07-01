#pragma once

#include "object.h"

enum class ValueType { Boolean, Nil, Number, Object };

bool values_equal(const Value a, const Value b);
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

  static Value object(ObjString *obj_str) {
    return {ValueType::Object, {.object = obj_str}};
  }

  static Value object(ObjFunction *obj_func) {
    return {ValueType::Object, {.object = obj_func}};
  }

  static Value object(ObjClosure *obj_closure) {
    return {ValueType::Object, {.object = obj_closure}};
  }

  static Value object(ObjNative *obj_native) {
    return {ValueType::Object, {.object = obj_native}};
  }

  Object *as_object() const { return as.object; }
  bool is_object() { return type == ValueType::Object; }

  bool is_string() {
    return is_object() && as_object()->type == Object::Type::String;
  }
  ObjString *as_string() const { return static_cast<ObjString *>(as_object()); }

  // not used anymore
  bool is_function() {
    return is_object() && as_object()->type == Object::Type::Function;
  }
  ObjFunction *as_function() const {
    return static_cast<ObjFunction *>(as_object());
  }

  bool is_closure() {
    return is_object() && as_object()->type == Object::Type::Closure;
  }
  ObjClosure *as_closure() const {
    return static_cast<ObjClosure *>(as_object());
  }

  bool is_native() {
    return is_object() && as_object()->type == Object::Type::Native;
  }
  ObjNative *as_native() const { return static_cast<ObjNative *>(as_object()); }

  const bool operator==(const Value &other_value) const {
    return values_equal(*this, other_value);
  }
};

void print_value(Value value);
