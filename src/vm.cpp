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

static bool is_falsey(Value value) {
  return value.is_nil() || (value.is_boolean() && !value.as_boolean());
}

static bool values_equal(Value a, Value b) {
  if (a.type != b.type) {
    return false;
  }

  switch (a.type) {
  case ValueType::Boolean:
    return a.as_boolean() == b.as_boolean();
  case ValueType::Number:
    return a.as_number() == b.as_number();
  case ValueType::Nil:
    return true;
  }
}

VM::VM() { stack_top = stack; }

InterpretResult VM::interpret(std::string source) {
  Compiler compiler{source};
  if (!compiler.compile(chunk)) {
    return InterpretResult::CompileError;
  }
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
    case OP_GREATER:
      binary_op(&Value::boolean, std::greater<double>{});
      break;
    case OP_LESS:
      binary_op(&Value::boolean, std::less<double>{});
      break;
    case OP_ADD:
      binary_op(&Value::number, std::plus<double>{});
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

TEST_CASE("VM::interpret") {
  VM vm{};

  SUBCASE("returns Ok") {
    std::string output = capture_stdout(
        [&] { CHECK(vm.interpret("1 + 2") == InterpretResult::Ok); });
  }

  SUBCASE("equality with mixed types") {
    std::string output = capture_stdout(
        [&] { CHECK(vm.interpret("!(5 - 4 > 3 * 2 == !nil)") == InterpretResult::Ok); });
    CHECK(output == "true\n");
  }
}
