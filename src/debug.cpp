#include <cstdint>
#include <iostream>

#include "chunk.h"
#include "debug.h"
#include "doctest.h"
#include "test_utils.h"
#include "value.h"

int simple_instruction(std::string_view, int);
int constant_instruction(std::string_view, const Chunk &, int);
int byte_instruction(std::string_view, const Chunk &, int);

void disassemble_chunk(const Chunk &chunk, std::string name) {
  std::cout << "== " << name << " ==\n";

  for (int offset = 0; offset < chunk.size();) {
    offset = disassemble_instruction(chunk, offset);
  }
}

int disassemble_instruction(const Chunk &chunk, int offset) {
  std::cout << std::format("{:04d} ", offset);

  if (offset > 0 && chunk.get_line(offset) == chunk.get_line(offset - 1)) {
    std::cout << "   | ";
  } else {
    std::cout << std::format("{:4d} ", chunk.get_line(offset));
  }

  uint8_t instruction = chunk[offset];
  switch (instruction) {
  case OP_PRINT:
    return simple_instruction("OP_PRINT", offset);
  case OP_RETURN:
    return simple_instruction("OP_RETURN", offset);
  case OP_CONSTANT:
    return constant_instruction("OP_CONSTANT", chunk, offset);
  case OP_NIL:
    return simple_instruction("OP_NIL", offset);
  case OP_TRUE:
    return simple_instruction("OP_TRUE", offset);
  case OP_FALSE:
    return simple_instruction("OP_FALSE", offset);
  case OP_POP:
    return simple_instruction("OP_POP", offset);
  case OP_POPN:
    return simple_instruction("OP_POPN", offset);
  case OP_GET_LOCAL:
    return byte_instruction("OP_GET_LOCAL", chunk, offset);
  case OP_SET_LOCAL:
    return byte_instruction("OP_SET_LOCAL", chunk, offset);
  case OP_GET_GLOBAL:
    return constant_instruction("OP_GET_GLOBAL", chunk, offset);
  case OP_DEFINE_GLOBAL:
    return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
  case OP_SET_GLOBAL:
    return constant_instruction("OP_SET_GLOBAL", chunk, offset);
  case OP_EQUAL:
    return simple_instruction("OP_EQUAL", offset);
  case OP_NOT_EQUAL:
    return simple_instruction("OP_NOT_EQUAL", offset);
  case OP_GREATER:
    return simple_instruction("OP_GREATER", offset);
  case OP_GREATER_EQUAL:
    return simple_instruction("OP_GREATER_EQUAL", offset);
  case OP_LESS:
    return simple_instruction("OP_LESS", offset);
  case OP_LESS_EQUAL:
    return simple_instruction("OP_LESS_EQUAL", offset);
  case OP_ADD:
    return simple_instruction("OP_ADD", offset);
  case OP_SUBTRACT:
    return simple_instruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simple_instruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simple_instruction("OP_DIVIDE", offset);
  case OP_NOT:
    return simple_instruction("OP_NOT", offset);
  case OP_NEGATE:
    return simple_instruction("OP_NEGATE", offset);
  default:
    std::cout << "Unknown opcode " << instruction << std::endl;
    return offset + 1;
  }

  return offset;
}

int byte_instruction(std::string_view name, const Chunk &chunk, int offset) {
  uint8_t slot = chunk[offset + 1];
  std::cout << std::format("{:<16s} {:4d}\n", name, slot);
  return offset + 2;
}

int simple_instruction(std::string_view name, int offset) {
  std::cout << name << std::endl;
  return offset + 1;
}

int constant_instruction(std::string_view name, const Chunk &chunk,
                         int offset) {
  uint8_t constant_idx = chunk[offset + 1];
  std::cout << std::format("{:<16s} {:4d} '", name, constant_idx);
  print_value(chunk.get_constant(constant_idx));
  std::cout << "'" << std::endl;
  return offset + 2;
}

void print_stack(Value *stack, Value *stack_top) {
  std::cout << "          ";
  while (stack < stack_top) {
    std::cout << "[ ";
    print_value(*stack);
    std::cout << " ]";
    stack++;
  }
  std::cout << "\n";
}

TEST_CASE("disassemble_chunk") {
  Chunk chunk;
  chunk.write(OP_CONSTANT, 1);
  chunk.write(chunk.write_constant(Value::number(1.5)), 1);
  chunk.write(OP_RETURN, 2);

  std::string output =
      capture_stdout([&] { disassemble_chunk(chunk, "test chunk"); });

  CHECK(output.find("== test chunk ==") != std::string::npos);
  CHECK(output.find("OP_CONSTANT") != std::string::npos);
  CHECK(output.find("OP_RETURN") != std::string::npos);
}

TEST_CASE("disassemble_instruction") {
  SUBCASE("OP_RETURN advances offset by 1") {
    Chunk chunk;
    chunk.write(OP_RETURN, 1);

    std::string output = capture_stdout([&] {
      int next_offset = disassemble_instruction(chunk, 0);
      CHECK(next_offset == 1);
    });

    CHECK(output.find("OP_RETURN") != std::string::npos);
  }

  SUBCASE("OP_CONSTANT advances offset by 2") {
    Chunk chunk;
    chunk.write(OP_CONSTANT, 1);
    chunk.write(chunk.write_constant(Value::number(3.14)), 1);

    std::string output = capture_stdout([&] {
      int next_offset = disassemble_instruction(chunk, 0);
      CHECK(next_offset == 2);
    });

    CHECK(output.find("OP_CONSTANT") != std::string::npos);
    CHECK(output.find("3.14") != std::string::npos);
  }

  SUBCASE("same-line instructions print pipe instead of line number") {
    Chunk chunk;
    chunk.write(OP_RETURN, 1);
    chunk.write(OP_RETURN, 1);

    std::string output =
        capture_stdout([&] { disassemble_instruction(chunk, 1); });

    CHECK(output.find("|") != std::string::npos);
  }
}
