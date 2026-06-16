#include <iostream>
#include <print>

#include "chunk.h"
#include "doctest.h"
#include "object.h"
#include "test_utils.h"
#include "value.h"

// This is here since the header only has a forward declaration of Chunk.
// Actually using it is an implementation.
ObjFunction::ObjFunction() : Object(ObjectType::Function), chunk(new Chunk()) {}

void print_object(const Value &value) {
  switch (value.as_object()->type) {
  case ObjectType::String:
    std::cout << value.as_string()->chars;
    break;
  case ObjectType::Function: {
    ObjFunction *fn = value.as_function();
    if (!fn->name) {
      std::print("<script>");
      return;
    }
    std::print("<fn {}>", value.as_function()->name->chars);
    break;
  }
  case ObjectType::Native:
    std::print("<native fn>");
    break;
  }
}

TEST_CASE("print_object: function") {
  SUBCASE("named function prints <fn NAME>") {
    ObjFunction fn;
    ObjString name("foo");
    fn.name = &name;
    Value value = Value::object(&fn);
    CHECK(capture_stdout([&] { print_object(value); }) == "<fn foo>");
  }

  SUBCASE("nameless function prints <script>") {
    ObjFunction fn;
    Value value = Value::object(&fn);
    CHECK(capture_stdout([&] { print_object(value); }) == "<script>");
  }
}
