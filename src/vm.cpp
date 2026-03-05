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

// TEST_CASE("VM::interpret") {
//   VM vm{};
//   Chunk chunk;

//   SUBCASE("OP_RETURN returns INTERPRET_OK") {
//     chunk.write(OP_RETURN, 1);
//     CHECK(vm.interpret(std::move(chunk)) == InterpretResult::Ok);
//   }

//   SUBCASE("OP_CONSTANT prints value and returns INTERPRET_OK") {
//     chunk.write(OP_CONSTANT, 1);
//     chunk.write_constant(1.5, 1);
//     chunk.write(OP_RETURN, 1);

//     std::string output = capture_stdout(
//         [&] { CHECK(vm.interpret(std::move(chunk)) == InterpretResult::Ok);
//         });

//     CHECK(output.find("1.5") != std::string::npos);
//   }

//   SUBCASE("unknown opcode returns INTERPRET_RUNTIME_ERROR") {
//     chunk.write(255, 1);

//     std::ostringstream captured_err;
//     std::streambuf *old = std::cerr.rdbuf(captured_err.rdbuf());
//     InterpretResult result = vm.interpret(std::move(chunk));
//     std::cerr.rdbuf(old);

//     CHECK(result == InterpretResult::RuntimeError);
//   }
// }
