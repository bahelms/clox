#include <iostream>

#include "chunk.h"
#include "doctest.h"
#include "test_utils.h"
#include "value.h"
#include "vm.h"

InterpretResult VM::interpret(Chunk *chunk) {
  this->chunk = chunk;
  this->ip = chunk->data();
  return run();
}

InterpretResult VM::run() {
  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
    disassemble_instruction(*this->chunk,
                            (int)(this->ip - this->chunk->data()));
#endif

    uint8_t instr{};
    switch (instr = *this->ip++) {
    case OP_RETURN: {
      return INTERPRET_OK;
    }
    case OP_CONSTANT: {
      Value constant = this->chunk->get_constant(*this->ip++);
      print_value(constant);
      std::cout << std::endl;
      break;
    }
    default:
      std::cerr << "Unknown opcode: " << (int)instr << std::endl;
      return INTERPRET_RUNTIME_ERROR;
    }
  }
}

TEST_CASE("VM::interpret") {
  VM vm{};
  Chunk chunk;

  SUBCASE("OP_RETURN returns INTERPRET_OK") {
    chunk.write(OP_RETURN, 1);
    CHECK(vm.interpret(&chunk) == INTERPRET_OK);
  }

  SUBCASE("OP_CONSTANT prints value and returns INTERPRET_OK") {
    chunk.write(OP_CONSTANT, 1);
    chunk.write_constant(1.5, 1);
    chunk.write(OP_RETURN, 1);

    std::string output = capture_stdout([&] {
      CHECK(vm.interpret(&chunk) == INTERPRET_OK);
    });

    CHECK(output.find("1.5") != std::string::npos);
  }

  SUBCASE("unknown opcode returns INTERPRET_RUNTIME_ERROR") {
    chunk.write(255, 1);

    std::ostringstream captured_err;
    std::streambuf *old = std::cerr.rdbuf(captured_err.rdbuf());
    InterpretResult result = vm.interpret(&chunk);
    std::cerr.rdbuf(old);

    CHECK(result == INTERPRET_RUNTIME_ERROR);
  }

  SUBCASE("multiple constants before return all execute") {
    chunk.write(OP_CONSTANT, 1);
    chunk.write_constant(1.0, 1);
    chunk.write(OP_CONSTANT, 1);
    chunk.write_constant(2.0, 1);
    chunk.write(OP_RETURN, 1);

    std::string output = capture_stdout([&] {
      CHECK(vm.interpret(&chunk) == INTERPRET_OK);
    });

    CHECK(output.find("1") != std::string::npos);
    CHECK(output.find("2") != std::string::npos);
  }
}
