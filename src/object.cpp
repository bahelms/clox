#include <iostream>

#include "object.h"
#include "value.h"

void print_object(const Value &value) {
  switch (value.as_object()->type) {
  case ObjectType::String:
    std::cout << value.as_string()->chars;
    break;
  }
}
