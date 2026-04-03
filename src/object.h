#pragma once
#include <string>

struct Value;

enum class ObjectType {
  String,
};

class Object {
public:
  ObjectType type{};
};

class ObjString : public Object {
public:
  std::string chars;

  explicit ObjString(std::string s) : chars(std::move(s)) {}
};

void print_object(const Value &value);
