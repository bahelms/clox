#include <functional>
#include <iostream>

#include "chunk.h"
#include "compiler.h"
#include "debug.h"
#include "doctest.h"
#include "test_utils.h"
#include "value.h"
#include "vm.h"

VM::VM() { stack_top = stack; }

InterpretResult VM::interpret(std::string source) {
  compile(source);
  return InterpretResult::Ok;
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
    case OP_ADD:
      binary_op(std::plus<double>{});
      break;
    case OP_SUBTRACT:
      binary_op(std::minus<double>{});
      break;
    case OP_MULTIPLY:
      binary_op(std::multiplies<double>{});
      break;
    case OP_DIVIDE:
      binary_op(std::divides<double>{});
      break;
    case OP_NEGATE:
      *(stack_top - 1) = -*(stack_top - 1);
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

template <typename Op> void VM::binary_op(Op op) {
  double b = pop();
  double a = pop();
  push(op(a, b));
}

TEST_CASE("VM::interpret") {
  VM vm{};

  SUBCASE("returns Ok") {
    std::string output =
        capture_stdout([&] { CHECK(vm.interpret("1 + 2") == InterpretResult::Ok); });
  }
}
