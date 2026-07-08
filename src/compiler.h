#pragma once
#include <cstdint>
#include <string_view>

#include "chunk.h"
#include "object.h"
#include "parser.h"
#include "scanner.h"
#include "vm.h"

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

struct Upvalue {
  uint8_t index{};
  bool is_local{};
};

enum class FunctionType {
  Script,
  Function,
};

class Compiler {
  Parser &parser;
  VM &vm;
  Compiler *enclosing{};
  ObjFunction *function{new ObjFunction()};
  FunctionType type{};

  int local_count{};
  int scope_depth{};
  std::array<Local, UINT8_COUNT> locals{};
  std::array<Upvalue, UINT8_COUNT> upvalues{};

  Chunk *current_chunk() { return function->chunk; };
  ObjFunction *end();
  void emit_return();
  void emit_byte(uint8_t byte);
  void emit_bytes(uint8_t byte1, uint8_t byte2);
  void emit_constant(Value value);
  uint8_t make_constant(Value value);
  int emit_jump(OpCode op);
  void patch_jump(int instr_offset);
  void emit_loop(int loop_start);

  void declaration();
  void synchronize();
  void fun_declaration();
  void var_declaration();
  uint8_t parse_variable(const char *error_msg);
  void declare_variable();
  void add_local(const Token &name);
  bool identifiers_equal(const Token &a, const Token &b);
  uint8_t identifier_constant(const Token &name);
  void define_variable(uint8_t global_var_idx);
  void mark_initialized();
  void compile_function(FunctionType type);
  void function_body();
  void statement();
  void expression();
  void parse_precedence(Precedence precedence);
  bool match(TokenType type);
  bool check(TokenType type);
  void print_statement();
  void for_statement();
  void if_statement();
  void return_statement();
  void while_statement();
  void expression_statement();
  void named_variable(const Token &name, bool can_assign);
  int resolve_local(const Token &name);
  int resolve_upvalue(const Token &name);
  int add_upvalue(uint8_t index, bool is_local);
  void block();
  void begin_scope();
  void end_scope();
  uint8_t argument_list();

public:
  Compiler(Parser &parser, VM &vm, FunctionType type,
           Compiler *enclosing = nullptr)
      : parser(parser), vm(vm), enclosing(enclosing), type(type) {
    Local *local = &locals[local_count++];
    local->name.start = "";
  };

  ObjFunction *compile();
  void grouping(bool can_assign = false);
  void call(bool can_assign = false);
  void unary(bool can_assign = false);
  void binary(bool can_assign = false);
  void number(bool can_assign = false);
  void literal(bool can_assign = false);
  void string(bool can_assign = false);
  void variable(bool can_assign);
  void and_(bool can_assign);
  void or_(bool can_assign);
};

struct ParseRule {
  using ParseFn = void (Compiler::*)(bool can_assign);

  ParseFn prefix{};
  ParseFn infix{};
  Precedence precedence;
};

// Owns the Parser for the whole compile and primes the first token, then drives
// the top-level (script) Compiler. Function bodies reuse the same Parser via a
// nested Compiler so the scanner cursor never rewinds.
ObjFunction *compile_script(std::string_view src, VM &vm);
