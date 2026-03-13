#include <cstdint>

#include "chunk.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

ParseRule rules[] = {
    {&Compiler::grouping, NULL, Precedence::None},           // LeftParen
    {NULL, NULL, Precedence::None},                          // RightParen
    {NULL, NULL, Precedence::None},                          // LeftBrace
    {NULL, NULL, Precedence::None},                          // RightBrace
    {NULL, NULL, Precedence::None},                          // Comma
    {NULL, NULL, Precedence::None},                          // Dot
    {&Compiler::unary, &Compiler::binary, Precedence::Term}, // Minus
    {NULL, &Compiler::binary, Precedence::Term},             // Plus
    {NULL, NULL, Precedence::None},                          // Semicolon
    {NULL, &Compiler::binary, Precedence::Factor},           // Slash
    {NULL, &Compiler::binary, Precedence::Factor},           // Star
    {NULL, NULL, Precedence::None},                          // Bang
    {NULL, NULL, Precedence::None},                          // BangEqual
    {NULL, NULL, Precedence::None},                          // Equal
    {NULL, NULL, Precedence::None},                          // EqualEqual
    {NULL, NULL, Precedence::None},                          // Greater
    {NULL, NULL, Precedence::None},                          // GreaterEqual
    {NULL, NULL, Precedence::None},                          // Less
    {NULL, NULL, Precedence::None},                          // LessEqual
    {NULL, NULL, Precedence::None},                          // Identifier
    {NULL, NULL, Precedence::None},                          // String
    {&Compiler::number, NULL, Precedence::None},             // Number
    {NULL, NULL, Precedence::None},                          // And
    {NULL, NULL, Precedence::None},                          // Class
    {NULL, NULL, Precedence::None},                          // Else
    {NULL, NULL, Precedence::None},                          // False
    {NULL, NULL, Precedence::None},                          // For
    {NULL, NULL, Precedence::None},                          // Fun
    {NULL, NULL, Precedence::None},                          // If
    {NULL, NULL, Precedence::None},                          // Nil
    {NULL, NULL, Precedence::None},                          // Or
    {NULL, NULL, Precedence::None},                          // Print
    {NULL, NULL, Precedence::None},                          // Return
    {NULL, NULL, Precedence::None},                          // Super
    {NULL, NULL, Precedence::None},                          // This
    {NULL, NULL, Precedence::None},                          // True
    {NULL, NULL, Precedence::None},                          // Var
    {NULL, NULL, Precedence::None},                          // While
    {NULL, NULL, Precedence::None},                          // Error
    {NULL, NULL, Precedence::None},                          // EOF
};

static ParseRule &get_rule(TokenType operator_type) {
  return rules[static_cast<size_t>(operator_type)];
}

bool Compiler::compile(Chunk &chunk) {
  current_chunk = &chunk;
  parser.advance();
  expression();
  parser.consume(TokenType::Eof, "Expect end of expression");
  end();
  return !parser.had_error;
}

void Compiler::expression() { parse_precedence(Precedence::Assignment); }

void Compiler::parse_precedence(Precedence precedence) {
  parser.advance();
  auto &prefix_rule = get_rule(parser.previous_type()).prefix;
  if (prefix_rule == NULL) {
    parser.error("Expect expression.");
    return;
  }
  (this->*prefix_rule)();

  while (precedence <= get_rule(parser.current_type()).precedence) {
    parser.advance();
    auto &infix_rule = get_rule(parser.previous_type()).infix;
    (this->*infix_rule)();
  }
}

void Compiler::unary() {
  TokenType operator_type = parser.previous_type();
  parse_precedence(Precedence::Unary);
  switch (operator_type) {
  case TokenType::Minus:
    emit_byte(OP_NEGATE);
    break;
  default:
    return;
  }
}

void Compiler::number() {
  double value = strtod(parser.previous.start, NULL);
  emit_constant(value);
}

void Compiler::grouping() {
  expression();
  parser.consume(TokenType::RightParen, "Expect ')' after expression.");
}

void Compiler::binary() {
  TokenType operator_type = parser.previous_type();
  const ParseRule &rule = get_rule(operator_type);
  parse_precedence(rule.precedence + 1);

  switch (operator_type) {
  case TokenType::Plus:
    emit_byte(OP_ADD);
    break;
  case TokenType::Minus:
    emit_byte(OP_SUBTRACT);
    break;
  case TokenType::Star:
    emit_byte(OP_MULTIPLY);
    break;
  case TokenType::Slash:
    emit_byte(OP_DIVIDE);
    break;
  default:
    return;
  }
}

void Compiler::end() {
  emit_return();
#ifdef DEBUG_PRINT_CODE
  if (!parser.had_error) {
    disassemble_chunk(*current_chunk, "code");
  }
#endif
}

void Compiler::emit_return() { emit_byte(OP_RETURN); }

void Compiler::emit_byte(uint8_t byte) {
  current_chunk->write(byte, parser.previous.line);
}

void Compiler::emit_bytes(uint8_t byte1, uint8_t byte2) {
  emit_byte(byte1);
  emit_byte(byte2);
}

void Compiler::emit_constant(Value value) {
  emit_bytes(OP_CONSTANT, make_constant(value));
}

uint8_t Compiler::make_constant(Value value) {
  int index = current_chunk->write_constant(value);
  if (index > UINT8_MAX) {
    parser.error("Too many constants in one chunk.");
    return 0;
  }
  return index;
}

#include "doctest.h"

static Chunk compile_source(std::string_view src) {
  Chunk chunk;
  Compiler compiler(src);
  compiler.compile(chunk);
  return chunk;
}

TEST_CASE("Compiler: number literal") {
  auto chunk = compile_source("1.5");
  CHECK(chunk[0] == OP_CONSTANT);
  CHECK(chunk.get_constant(chunk[1]) == 1.5);
  CHECK(chunk[2] == OP_RETURN);
}

TEST_CASE("Compiler: negation") {
  auto chunk = compile_source("-2");
  CHECK(chunk[0] == OP_CONSTANT);
  CHECK(chunk.get_constant(chunk[1]) == 2.0);
  CHECK(chunk[2] == OP_NEGATE);
  CHECK(chunk[3] == OP_RETURN);
}

TEST_CASE("Compiler: binary operations") {
  SUBCASE("addition") {
    auto chunk = compile_source("1 + 2");
    CHECK(chunk[0] == OP_CONSTANT);
    CHECK(chunk[2] == OP_CONSTANT);
    CHECK(chunk[4] == OP_ADD);
    CHECK(chunk[5] == OP_RETURN);
  }
  SUBCASE("subtraction") {
    auto chunk = compile_source("5 - 3");
    CHECK(chunk[4] == OP_SUBTRACT);
  }
  SUBCASE("multiplication") {
    auto chunk = compile_source("2 * 4");
    CHECK(chunk[4] == OP_MULTIPLY);
  }
  SUBCASE("division") {
    auto chunk = compile_source("8 / 2");
    CHECK(chunk[4] == OP_DIVIDE);
  }
}

TEST_CASE("Compiler: grouping") {
  auto chunk = compile_source("(3 * 4) + 2");
  CHECK(chunk[0] == OP_CONSTANT);
  CHECK(chunk.get_constant(chunk[1]) == 3.0);
  CHECK(chunk[2] == OP_CONSTANT);
  CHECK(chunk.get_constant(chunk[3]) == 4.0);
  CHECK(chunk[4] == OP_MULTIPLY);
  CHECK(chunk[5] == OP_CONSTANT);
  CHECK(chunk.get_constant(chunk[6]) == 2.0);
  CHECK(chunk[7] == OP_ADD);
  CHECK(chunk[8] == OP_RETURN);
}

TEST_CASE("Compiler: operator precedence") {
  auto chunk = compile_source("1 + 2 * 3");
  CHECK(chunk[6] == OP_MULTIPLY);
  CHECK(chunk[7] == OP_ADD);
}

TEST_CASE("Compiler: compile returns false on error") {
  Chunk chunk;
  Compiler compiler("@");
  CHECK(compiler.compile(chunk) == false);
}
