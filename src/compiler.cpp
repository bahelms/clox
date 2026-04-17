#include <cstdint>
#include <sys/types.h>

#include "chunk.h"
#include "compiler.h"
#include "object.h"
#include "scanner.h"
#include "vm.h"

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
    {&Compiler::unary, NULL, Precedence::None},              // Bang
    {NULL, &Compiler::binary, Precedence::Equality},         // BangEqual
    {NULL, NULL, Precedence::None},                          // Equal
    {NULL, &Compiler::binary, Precedence::Equality},         // EqualEqual
    {NULL, &Compiler::binary, Precedence::Comparison},       // Greater
    {NULL, &Compiler::binary, Precedence::Comparison},       // GreaterEqual
    {NULL, &Compiler::binary, Precedence::Comparison},       // Less
    {NULL, &Compiler::binary, Precedence::Comparison},       // LessEqual
    {&Compiler::variable, NULL, Precedence::None},           // Identifier
    {&Compiler::string, NULL, Precedence::None},             // String
    {&Compiler::number, NULL, Precedence::None},             // Number
    {NULL, NULL, Precedence::None},                          // And
    {NULL, NULL, Precedence::None},                          // Class
    {NULL, NULL, Precedence::None},                          // Else
    {&Compiler::literal, NULL, Precedence::None},            // False
    {NULL, NULL, Precedence::None},                          // For
    {NULL, NULL, Precedence::None},                          // Fun
    {NULL, NULL, Precedence::None},                          // If
    {&Compiler::literal, NULL, Precedence::None},            // Nil
    {NULL, NULL, Precedence::None},                          // Or
    {NULL, NULL, Precedence::None},                          // Print
    {NULL, NULL, Precedence::None},                          // Return
    {NULL, NULL, Precedence::None},                          // Super
    {NULL, NULL, Precedence::None},                          // This
    {&Compiler::literal, NULL, Precedence::None},            // True
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
  while (!match(TokenType::Eof)) {
    declaration();
  }
  end();
  return !parser.had_error;
}

bool Compiler::match(TokenType type) {
  if (!check(type)) {
    return false;
  }
  parser.advance();
  return true;
}

bool Compiler::check(TokenType type) { return parser.current_type() == type; }

void Compiler::declaration() {
  if (match(TokenType::Var)) {
    var_declaration();
  } else {
    statement();
  }

  if (parser.has_panicked()) {
    synchronize();
  }
}

void Compiler::synchronize() {
  parser.panic(false);

  while (parser.current_type() != TokenType::Eof) {
    if (parser.previous_type() == TokenType::Semicolon) {
      return;
    }

    switch (parser.current_type()) {
    case TokenType::Class:
    case TokenType::Fun:
    case TokenType::Var:
    case TokenType::For:
    case TokenType::If:
    case TokenType::While:
    case TokenType::Print:
    case TokenType::Return:
      return;

    default:; // do nothing
    }

    parser.advance();
  }
}

void Compiler::var_declaration() {
  uint8_t global_var_idx = parse_variable("Expect variable name.");
  if (match(TokenType::Equal)) {
    expression();

  } else {
    emit_byte(OP_NIL);
  }

  parser.consume(TokenType::Semicolon, "Expect ';' after variable declaration");
  define_variable(global_var_idx);
}

uint8_t Compiler::parse_variable(const char *error_msg) {
  parser.consume(TokenType::Identifier, error_msg);
  return identifier_constant(&parser.previous);
}

uint8_t Compiler::identifier_constant(Token *name) {
  ObjString *identifier =
      vm.alloc_string(std::string(name->start, name->length));
  return make_constant(Value::object(identifier));
}

void Compiler::define_variable(uint8_t global_var_idx) {
  emit_bytes(OP_DEFINE_GLOBAL, global_var_idx);
}

void Compiler::statement() {
  if (match(TokenType::Print)) {
    print_statement();
  } else {
    expression_statement();
  }
}

void Compiler::print_statement() {
  expression();
  parser.consume(TokenType::Semicolon, "Expect ';' after value");
  emit_byte(OP_PRINT);
}

void Compiler::expression_statement() {
  expression();
  parser.consume(TokenType::Semicolon, "Expect ';' after expression");
  emit_byte(OP_POP);
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
  case TokenType::Bang:
    emit_byte(OP_NOT);
    break;
  default:
    return;
  }
}

void Compiler::binary() {
  TokenType operator_type = parser.previous_type();
  const ParseRule &rule = get_rule(operator_type);
  parse_precedence(rule.precedence + 1);

  switch (operator_type) {
  case TokenType::BangEqual:
    emit_byte(OP_NOT_EQUAL);
    break;
  case TokenType::EqualEqual:
    emit_byte(OP_EQUAL);
    break;
  case TokenType::Greater:
    emit_byte(OP_GREATER);
    break;
  case TokenType::GreaterEqual:
    emit_byte(OP_GREATER_EQUAL);
    break;
  case TokenType::Less:
    emit_byte(OP_LESS);
    break;
  case TokenType::LessEqual:
    emit_byte(OP_LESS_EQUAL);
    break;
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

void Compiler::number() {
  double value = strtod(parser.previous.start, NULL);
  emit_constant(Value::number(value));
}

void Compiler::grouping() {
  expression();
  parser.consume(TokenType::RightParen, "Expect ')' after expression.");
}

void Compiler::literal() {
  switch (parser.previous_type()) {
  case TokenType::False:
    emit_byte(OP_FALSE);
    break;
  case TokenType::Nil:
    emit_byte(OP_NIL);
    break;
  case TokenType::True:
    emit_byte(OP_TRUE);
    break;
  default:
    return;
  }
}

void Compiler::string() {
  ObjString *str = vm.alloc_string(
      std::string(parser.previous.start + 1, parser.previous.length - 2));
  emit_constant(Value::object(str));
}

void Compiler::variable() { named_variable(parser.previous); }

void Compiler::named_variable(Token name) {
  uint8_t arg = identifier_constant(&name);
  emit_bytes(OP_GET_GLOBAL, arg);
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
  VM vm;
  Compiler compiler(src, vm);
  compiler.compile(chunk);
  return chunk;
}

TEST_CASE("Compiler: number literal") {
  auto chunk = compile_source("1.5;");
  CHECK(chunk[0] == OP_CONSTANT);
  CHECK(chunk.get_constant(chunk[1]).as_number() == 1.5);
  CHECK(chunk[2] == OP_POP);
  CHECK(chunk[3] == OP_RETURN);
}

TEST_CASE("Compiler: negation") {
  auto chunk = compile_source("-2;");
  CHECK(chunk[0] == OP_CONSTANT);
  CHECK(chunk.get_constant(chunk[1]).as_number() == 2.0);
  CHECK(chunk[2] == OP_NEGATE);
  CHECK(chunk[3] == OP_POP);
  CHECK(chunk[4] == OP_RETURN);
}

TEST_CASE("Compiler: binary operations") {
  SUBCASE("addition") {
    auto chunk = compile_source("1 + 2;");
    CHECK(chunk[0] == OP_CONSTANT);
    CHECK(chunk[2] == OP_CONSTANT);
    CHECK(chunk[4] == OP_ADD);
    CHECK(chunk[5] == OP_POP);
    CHECK(chunk[6] == OP_RETURN);
  }
  SUBCASE("subtraction") {
    auto chunk = compile_source("5 - 3;");
    CHECK(chunk[4] == OP_SUBTRACT);
  }
  SUBCASE("multiplication") {
    auto chunk = compile_source("2 * 4;");
    CHECK(chunk[4] == OP_MULTIPLY);
  }
  SUBCASE("division") {
    auto chunk = compile_source("8 / 2;");
    CHECK(chunk[4] == OP_DIVIDE);
  }
}

TEST_CASE("Compiler: grouping") {
  auto chunk = compile_source("(3 * 4) + 2;");
  CHECK(chunk[0] == OP_CONSTANT);
  CHECK(chunk.get_constant(chunk[1]).as_number() == 3.0);
  CHECK(chunk[2] == OP_CONSTANT);
  CHECK(chunk.get_constant(chunk[3]).as_number() == 4.0);
  CHECK(chunk[4] == OP_MULTIPLY);
  CHECK(chunk[5] == OP_CONSTANT);
  CHECK(chunk.get_constant(chunk[6]).as_number() == 2.0);
  CHECK(chunk[7] == OP_ADD);
  CHECK(chunk[8] == OP_POP);
  CHECK(chunk[9] == OP_RETURN);
}

TEST_CASE("Compiler: operator precedence") {
  auto chunk = compile_source("1 + 2 * 3;");
  CHECK(chunk[6] == OP_MULTIPLY);
  CHECK(chunk[7] == OP_ADD);
}

TEST_CASE("Compiler: boolean and nil literals") {
  SUBCASE("false") {
    auto chunk = compile_source("false;");
    CHECK(chunk[0] == OP_FALSE);
    CHECK(chunk[1] == OP_POP);
    CHECK(chunk[2] == OP_RETURN);
  }
  SUBCASE("true") {
    auto chunk = compile_source("true;");
    CHECK(chunk[0] == OP_TRUE);
    CHECK(chunk[1] == OP_POP);
    CHECK(chunk[2] == OP_RETURN);
  }
  SUBCASE("nil") {
    auto chunk = compile_source("nil;");
    CHECK(chunk[0] == OP_NIL);
    CHECK(chunk[1] == OP_POP);
    CHECK(chunk[2] == OP_RETURN);
  }
}

TEST_CASE("Compiler: comparison operators emit single opcodes") {
  SUBCASE("!=") {
    auto chunk = compile_source("1 != 2;");
    CHECK(chunk[4] == OP_NOT_EQUAL);
    CHECK(chunk[5] == OP_POP);
    CHECK(chunk[6] == OP_RETURN);
  }
  SUBCASE(">=") {
    auto chunk = compile_source("1 >= 2;");
    CHECK(chunk[4] == OP_GREATER_EQUAL);
    CHECK(chunk[5] == OP_POP);
    CHECK(chunk[6] == OP_RETURN);
  }
  SUBCASE("<=") {
    auto chunk = compile_source("1 <= 2;");
    CHECK(chunk[4] == OP_LESS_EQUAL);
    CHECK(chunk[5] == OP_POP);
    CHECK(chunk[6] == OP_RETURN);
  }
}

TEST_CASE("Compiler: compile returns false on error") {
  Chunk chunk;
  VM vm;
  Compiler compiler("@", vm);
  CHECK(compiler.compile(chunk) == false);
}
