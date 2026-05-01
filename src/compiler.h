#pragma once
#include <cstdint>
#include <string_view>

#include "chunk.h"
#include "parser.h"
#include "scanner.h"

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

struct Local {
  Token name{};
  int depth{};
};

class Compiler {
  std::string_view source{};
  Parser parser;
  Chunk *current_chunk{};
  VM &vm;

  int local_count{};
  int scope_depth{};
  std::array<Local, UINT8_MAX + 1> locals{};

  void end();
  void emit_return();
  void emit_byte(uint8_t byte);
  void emit_bytes(uint8_t byte1, uint8_t byte2);
  void emit_constant(Value value);
  uint8_t make_constant(Value value);
  int emit_jump(OpCode op);
  void patch_jump(int instr_offset);

  void declaration();
  void synchronize();
  void var_declaration();
  uint8_t parse_variable(const char *error_msg);
  void declare_variable();
  void add_local(Token name);
  bool identifiers_equal(Token a, Token b);
  uint8_t identifier_constant(Token *name);
  void define_variable(uint8_t global_var_idx);
  void mark_initialized();
  void statement();
  void expression();
  void parse_precedence(Precedence precedence);
  bool match(TokenType type);
  bool check(TokenType type);
  void print_statement();
  void if_statement();
  void expression_statement();
  void named_variable(Token name, bool can_assign);
  int resolve_local(const Token &name);
  void block();
  void begin_scope();
  void end_scope();

public:
  Compiler(std::string_view src, VM &vm) : source(src), parser(src), vm(vm) {};
  bool compile(Chunk &chunk);
  void grouping(bool can_assign = false);
  void unary(bool can_assign = false);
  void binary(bool can_assign = false);
  void number(bool can_assign = false);
  void literal(bool can_assign = false);
  void string(bool can_assign = false);
  void variable(bool can_assign);
};

using ParseFn = void (Compiler::*)(bool can_assign);

struct ParseRule {
  ParseFn prefix{};
  ParseFn infix{};
  Precedence precedence;
};
