#pragma once
#include <string>

struct Value;

enum class ObjectType {
  String,
};

class Object {
public:
  ObjectType type{};
  Object *next{};

  explicit Object(ObjectType t) : type(t) {}
};

class ObjString : public Object {
public:
  std::string chars;

  explicit ObjString(std::string s)
      : Object(ObjectType::String), chars(std::move(s)) {}
};

void print_object(const Value &value);
