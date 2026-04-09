#include <cstdarg>
#include <functional>
#include <iostream>

#include "chunk.h"
#include "compiler.h"
#include "doctest.h"
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

    uint8_t instr = *ip++;
    switch (instr) {
    case OP_CONSTANT: {
      push(chunk.get_constant(*ip++));
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
    case OP_RETURN: {
      print_value(pop());
      std::cout << '\n';
      return InterpretResult::Ok;
    }
    default:
      std::cerr << "Unknown opcode: " << static_cast<int>(instr) << '\n';
      return InterpretResult::RuntimeError;
    }
  }
}

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
}

TEST_CASE("VM::interpret") {
  VM vm{};

  SUBCASE("returns Ok") {
    std::string output = capture_stdout(
        [&] { CHECK(vm.interpret("1 + 2") == InterpretResult::Ok); });
  }

  SUBCASE("equality with mixed types") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("!(5 - 4 > 3 * 2 == !nil)") == InterpretResult::Ok);
    });
    CHECK(output == "true\n");
  }

  SUBCASE("equal strings are equal") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("\"hello\" == \"hello\"") == InterpretResult::Ok);
    });
    CHECK(output == "true\n");
  }

  SUBCASE("different strings are not equal") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("\"hello\" == \"world\"") == InterpretResult::Ok);
    });
    CHECK(output == "false\n");
  }

  SUBCASE("string concatenation") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("\"hello\" + \" \" + \"world\"") ==
            InterpretResult::Ok);
    });
    CHECK(output == "hello world\n");
  }
}
