#pragma once

enum class ValueType {
  Bool,
  Nil,
  Number,
};

// using manual union instead of std::variant to NaN box later on
struct Value {
  ValueType type;
  union {
    bool boolean;
    double number;
  } as;

  static Value boolean(bool v) { return {ValueType::Bool, {.boolean = v}}; }
  double as_boolean() const { return as.boolean; }
  bool is_boolean() { return type == ValueType::Bool; }

  static Value nil() { return {ValueType::Nil, {.number = 0}}; }
  bool is_nil() { return type == ValueType::Nil; }

  static Value number(double v) { return {ValueType::Number, {.number = v}}; }
  double as_number() const { return as.number; }
  bool is_number() { return type == ValueType::Number; }
};

void print_value(Value value);
