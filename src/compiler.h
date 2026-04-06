#pragma once
#include <cstdint>
#include <string_view>

#include "chunk.h"
#include "parser.h"

enum class Precedence {
  None,
  Assignment, // =
  Or,         // or
  And,        // and
  Equality,   // == !=
  Comparison, // < > <= >=
  Term,       // + -
  Factor,     // * /
  Unary,      // ! -
  Call,       // . ()
  Primary
};

inline Precedence operator+(Precedence p, int n) {
  return static_cast<Precedence>(static_cast<int>(p) + n);
}

class VM;

class Compiler {
  std::string_view source{};
  Parser parser;
  Chunk *current_chunk{};
  VM &vm;

  void end();
  void emit_return();
  void emit_byte(uint8_t byte);
  void emit_bytes(uint8_t byte1, uint8_t byte2);
  void emit_constant(Value value);
  uint8_t make_constant(Value value);

  void expression();
  void parse_precedence(Precedence precedence);

public:
  Compiler(std::string_view src, VM &vm) : source(src), parser(src), vm(vm) {};
  bool compile(Chunk &chunk);
  void grouping();
  void unary();
  void binary();
  void number();
  void literal();
  void string();
};

using ParseFn = void (Compiler::*)();

struct ParseRule {
  ParseFn prefix{};
  ParseFn infix{};
  Precedence precedence;
};
