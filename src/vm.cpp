#include <cstdarg>
#include <cstdint>
#include <functional>
#include <iostream>

#include "chunk.h"
#include "compiler.h"
#include "doctest.h"
#include "object.h"
#include "test_utils.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

VM::VM() { stack_top = stack; }

VM::~VM() {
  Object *obj = objects;
  while (obj != nullptr) {
    Object *next = obj->next;
    delete obj;
    obj = next;
  }
}

InterpretResult VM::interpret(std::string source) {
  Chunk new_chunk{};
  Compiler compiler{source, *this};
  if (!compiler.compile(new_chunk)) {
    return InterpretResult::CompileError;
  }
  chunk = std::move(new_chunk);
  ip = chunk.data();
  return run();
}

InterpretResult VM::run() {
  while (true) {
#ifdef DEBUG_TRACE_EXECUTION
    print_stack(stack, stack_top);
    disassemble_instruction(chunk, static_cast<int>(ip - chunk.data()));
#endif

    uint8_t instr = read_byte();
    switch (instr) {
    case OP_CONSTANT: {
      push(chunk.get_constant(read_byte()));
      break;
    }
    case OP_NIL:
      push(Value::nil());
      break;
    case OP_TRUE:
      push(Value::boolean(true));
      break;
    case OP_FALSE:
      push(Value::boolean(false));
      break;
    case OP_POP:
      pop();
      break;
    case OP_GET_GLOBAL: {
      uint8_t slot = read_byte();
      if (!globals_defined[slot]) {
        runtime_error("Undefined variable '{}'", global_names[slot]);
        return InterpretResult::RuntimeError;
      }
      push(globals[slot]);
      break;
    }
    case OP_DEFINE_GLOBAL: {
      uint8_t slot = read_byte();
      globals[slot] = peek(0);
      globals_defined[slot] = true;
      pop();
      break;
    }
    case OP_SET_GLOBAL: {
      uint8_t slot = read_byte();
      if (!globals_defined[slot]) {
        runtime_error("Undefined variable '{}'", global_names[slot]);
        return InterpretResult::RuntimeError;
      }
      globals[slot] = peek(0);
      break;
    }
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(Value::boolean(values_equal(a, b)));
      break;
    }
    case OP_NOT_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(Value::boolean(!values_equal(a, b)));
      break;
    }
    case OP_GREATER:
      binary_op(&Value::boolean, std::greater<double>{});
      break;
    case OP_GREATER_EQUAL:
      binary_op(&Value::boolean, std::greater_equal<double>{});
      break;
    case OP_LESS:
      binary_op(&Value::boolean, std::less<double>{});
      break;
    case OP_LESS_EQUAL:
      binary_op(&Value::boolean, std::less_equal<double>{});
      break;
    case OP_ADD:
      if (peek(0).is_string() && peek(1).is_string()) {
        ObjString *str_b = pop().as_string();
        ObjString *str_a = pop().as_string();
        push(Value::object(alloc_string(str_a->chars + str_b->chars)));
      } else if (peek(0).is_number() && peek(1).is_number()) {
        double b = pop().as_number();
        double a = pop().as_number();
        push(Value::number(a + b));
      } else {
        runtime_error("Operands must be numbers or strings.");
        return InterpretResult::RuntimeError;
      }
      break;
    case OP_SUBTRACT:
      binary_op(&Value::number, std::minus<double>{});
      break;
    case OP_MULTIPLY:
      binary_op(&Value::number, std::multiplies<double>{});
      break;
    case OP_DIVIDE:
      binary_op(&Value::number, std::divides<double>{});
      break;
    case OP_NOT:
      push(Value::boolean(is_falsey(pop())));
      break;
    case OP_NEGATE:
      if (!peek(0).is_number()) {
        runtime_error("Operand must be a number.");
        return InterpretResult::RuntimeError;
      }

      push(Value::number(-pop().as_number()));
      break;
    case OP_PRINT: {
      print_value(pop());
      std::cout << '\n';
      break;
    }
    case OP_RETURN: {
      return InterpretResult::Ok;
    }
    default:
      std::cerr << "Unknown opcode: " << static_cast<int>(instr) << '\n';
      return InterpretResult::RuntimeError;
    }
  }
}

uint8_t VM::read_byte() { return *ip++; }

void VM::push(Value value) {
  *stack_top = value;
  stack_top++;
}

Value VM::pop() {
  stack_top--;
  return *stack_top;
}

Value VM::peek(int distance) { return stack_top[-1 - distance]; }

void VM::reset_stack() { stack_top = stack; }

std::optional<uint8_t> VM::get_or_alloc_global_slot(const std::string &name) {
  auto it = global_slots.find(name);
  if (it != global_slots.end()) {
    return it->second;
  }
  if (global_slots.size() == 256) {
    return std::nullopt;
  }
  uint8_t slot = static_cast<uint8_t>(global_slots.size());
  global_slots.emplace(name, slot);
  globals.push_back(Value::nil());
  globals_defined.push_back(false);
  global_names.push_back(name);
  return slot;
}

ObjString *VM::alloc_string(std::string s) {
  auto [it, inserted] = interned_strings.try_emplace(std::move(s), nullptr);
  if (!inserted) {
    return it->second;
  }
  auto *obj = new ObjString(it->first);
  obj->next = objects;
  objects = obj;
  it->second = obj;
  return obj;
}

template <typename... Args>
void VM::runtime_error(std::format_string<Args...> fmt, Args &&...args) {
  std::cerr << std::format(fmt, std::forward<Args>(args)...) << '\n';

  size_t instruction = ip - chunk.data() - 1;
  int line = chunk.get_line(instruction);
  std::cerr << std::format("[line {}] in script\n", line);
  reset_stack();
}

template <typename ValueBuilder, typename Op>
InterpretResult VM::binary_op(ValueBuilder builder, Op op) {
  if (!peek(0).is_number() || !peek(1).is_number()) {
    runtime_error("Operands must be numbers.");
    return InterpretResult::RuntimeError;
  }

  double b = pop().as_number();
  double a = pop().as_number();
  push(builder(op(a, b)));
  return InterpretResult::Ok;
}

TEST_CASE("VM::alloc_string") {
  VM vm{};

  SUBCASE("returns same pointer for equal strings") {
    ObjString *a = vm.alloc_string("hello");
    ObjString *b = vm.alloc_string("hello");
    CHECK(a == b);
  }

  SUBCASE("returns different pointers for different strings") {
    ObjString *a = vm.alloc_string("hello");
    ObjString *b = vm.alloc_string("world");
    CHECK(a != b);
  }

  SUBCASE("interned string has correct content") {
    ObjString *a = vm.alloc_string("hello");
    CHECK(a->chars == "hello");
  }

  SUBCASE("empty string is interned") {
    ObjString *a = vm.alloc_string("");
    ObjString *b = vm.alloc_string("");
    CHECK(a == b);
    CHECK(a->chars == "");
  }
}

TEST_CASE("VM::interpret") {
  VM vm{};

  SUBCASE("returns Ok") {
    std::string output = capture_stdout(
        [&] { CHECK(vm.interpret("1 + 2;") == InterpretResult::Ok); });
  }

  SUBCASE("equality with mixed types") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print !(5 - 4 > 3 * 2 == !nil);") ==
            InterpretResult::Ok);
    });
    CHECK(output == "true\n");
  }

  SUBCASE("new chunk is used on each invocation") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print !(5 - 4 > 3 * 2 == !nil);") ==
            InterpretResult::Ok);
    });
    CHECK(output == "true\n");

    output = capture_stdout(
        [&] { CHECK(vm.interpret("print 4 + 5;") == InterpretResult::Ok); });
    CHECK(output == "9\n");
  }

  SUBCASE("equal strings are equal") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print \"hello\" == \"hello\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "true\n");
  }

  SUBCASE("different strings are not equal") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print \"hello\" == \"world\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "false\n");
  }

  SUBCASE("string concatenation") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print \"hello\" + \" \" + \"world\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "hello world\n");
  }

  SUBCASE("concatenated string is interned") {
    ObjString *a = vm.alloc_string("hello world");
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print \"hello\" + \" world\" == \"hello world\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "true\n");
  }

  SUBCASE("returns CompileError on invalid syntax") {
    suppress_stderr(
        [&] { CHECK(vm.interpret("@") == InterpretResult::CompileError); });
  }

  SUBCASE("returns RuntimeError when negating a non-number") {
    suppress_stderr([&] {
      CHECK(vm.interpret("-\"hello\";") == InterpretResult::RuntimeError);
    });
  }

  SUBCASE("returns RuntimeError for mixed types in addition") {
    suppress_stderr([&] {
      CHECK(vm.interpret("\"hello\" + 1;") == InterpretResult::RuntimeError);
    });
  }

  SUBCASE("define and get global variable") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("var x = 42; print x;") == InterpretResult::Ok);
    });
    CHECK(output == "42\n");
  }

  SUBCASE("set global variable") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("var x = 1; x = 2; print x;") == InterpretResult::Ok);
    });
    CHECK(output == "2\n");
  }

  SUBCASE("get undefined variable returns RuntimeError") {
    suppress_stderr([&] {
      CHECK(vm.interpret("print y;") == InterpretResult::RuntimeError);
    });
  }

  SUBCASE("set undefined variable returns RuntimeError") {
    suppress_stderr([&] {
      CHECK(vm.interpret("y = 1;") == InterpretResult::RuntimeError);
    });
  }

  SUBCASE("globals persist across interpret calls") {
    capture_stdout([&] {
      CHECK(vm.interpret("var z = 99;") == InterpretResult::Ok);
    });
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print z;") == InterpretResult::Ok);
    });
    CHECK(output == "99\n");
  }
}
