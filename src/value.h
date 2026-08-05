#pragma once

struct Value;
struct Object;
struct ObjString;
struct ObjFunction;
struct ObjClosure;
struct ObjNative;

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

  static Value object(ObjString *obj_str);
  static Value object(ObjFunction *obj_func);
  static Value object(ObjClosure *obj_closure);
  static Value object(ObjNative *obj_native);

  Object *as_object() const { return as.object; }
  bool is_object() { return type == ValueType::Object; }

  bool is_string();
  ObjString *as_string() const;

  // not used anymore
  bool is_function();
  ObjFunction *as_function() const;

  bool is_closure();
  ObjClosure *as_closure() const;

  bool is_native();
  ObjNative *as_native() const;

  const bool operator==(const Value &other_value) const {
    return values_equal(*this, other_value);
  }
};

void print_value(Value value);

// Value's object-dependent methods are defined at the bottom of object.h, once
// the Obj* types are complete. Include it here so that including value.h alone
// still yields the full Value + Object API.
#include "object.h"
