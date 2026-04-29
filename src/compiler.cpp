#include <cstdint>
#include <ranges>
#include <sys/types.h>

#include "chunk.h"
#include "compiler.h"
#include "doctest.h"
#include "object.h"
#include "scanner.h"
#include "test_utils.h"
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
  declare_variable();
  if (scope_depth > 0) {
    return 0;
  }
  return identifier_constant(&parser.previous);
}

void Compiler::declare_variable() {
  if (scope_depth == 0) {
    return;
  }
  Token name = parser.previous;
  for (Local &local :
       std::span(locals).first(local_count) | std::views::reverse) {
    if (local.depth != -1 && local.depth < scope_depth) {
      break;
    }

    if (identifiers_equal(name, local.name)) {
      parser.error("Already a variable with this name in this scope");
    }
  }
  add_local(name);
}

bool Compiler::identifiers_equal(const Token a, const Token b) {
  return std::string_view(a.start, a.length) ==
         std::string_view(b.start, b.length);
}

void Compiler::add_local(const Token name) {
  if (local_count == UINT8_MAX + 1) {
    parser.error("Too many local variables in function.");
    return;
  }

  Local *local = &locals[local_count++];
  local->name = name;
  local->depth = -1;
}

uint8_t Compiler::identifier_constant(Token *name) {
  auto slot = vm.get_or_alloc_global_slot(std::string(name->start, name->length));
  if (!slot.has_value()) {
    parser.error("Too many global variables in one program.");
    return 0;
  }
  return slot.value();
}

void Compiler::define_variable(uint8_t global_var_idx) {
  if (scope_depth > 0) {
    mark_initialized();
    return;
  }
  emit_bytes(OP_DEFINE_GLOBAL, global_var_idx);
}

void Compiler::mark_initialized() {
  locals[local_count - 1].depth = scope_depth;
}

void Compiler::statement() {
  if (match(TokenType::Print)) {
    print_statement();
  } else if (match(TokenType::LeftBrace)) {
    begin_scope();
    block();
    end_scope();
  } else {
    expression_statement();
  }
}

void Compiler::print_statement() {
  expression();
  parser.consume(TokenType::Semicolon, "Expect ';' after value");
  emit_byte(OP_PRINT);
}

void Compiler::block() {
  while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
    declaration();
  }
  parser.consume(TokenType::RightBrace, "Expect '}' after block.");
}

void Compiler::begin_scope() { scope_depth++; }

void Compiler::end_scope() {
  scope_depth--;
  int locals_to_pop{};
  while (local_count > 0 && locals[local_count - 1].depth > scope_depth) {
    local_count--;
    locals_to_pop++;
  }
  emit_bytes(OP_POPN, locals_to_pop);
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
  bool can_assign = precedence <= Precedence::Assignment;
  (this->*prefix_rule)(can_assign);

  while (precedence <= get_rule(parser.current_type()).precedence) {
    parser.advance();
    auto &infix_rule = get_rule(parser.previous_type()).infix;
    (this->*infix_rule)(false);
  }

  if (can_assign && match(TokenType::Equal)) {
    parser.error("Invalid assignment target.");
  }
}

void Compiler::unary(bool can_assign) {
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

void Compiler::binary(bool can_assign) {
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

void Compiler::number(bool can_assign) {
  double value = strtod(parser.previous.start, NULL);
  emit_constant(Value::number(value));
}

void Compiler::grouping(bool can_assign) {
  expression();
  parser.consume(TokenType::RightParen, "Expect ')' after expression.");
}

void Compiler::literal(bool can_assign) {
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

void Compiler::string(bool can_assign) {
  ObjString *str = vm.alloc_string(
      std::string(parser.previous.start + 1, parser.previous.length - 2));
  emit_constant(Value::object(str));
}

void Compiler::variable(bool can_assign) {
  named_variable(parser.previous, can_assign);
}

void Compiler::named_variable(Token name, bool can_assign) {
  uint8_t get_op{}, set_op{};
  int arg = resolve_local(name);
  if (arg != -1) {
    get_op = OP_GET_LOCAL;
    set_op = OP_SET_LOCAL;
  } else {
    arg = identifier_constant(&name);
    get_op = OP_GET_GLOBAL;
    set_op = OP_SET_GLOBAL;
  }

  if (can_assign && match(TokenType::Equal)) {
    expression();
    emit_bytes(set_op, arg);
  } else {
    emit_bytes(get_op, arg);
  }
}

int Compiler::resolve_local(const Token &name) {
  for (int i = local_count - 1; i >= 0; i--) {
    if (identifiers_equal(name, locals[i].name)) {
      if (locals[i].depth == -1) {
        parser.error("Can't read local variable in its own initializer.");
      }
      return i;
    }
  }
  return -1;
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
  suppress_stderr([&] { CHECK(compiler.compile(chunk) == false); });
}

TEST_CASE("Compiler: local variable declaration leaves value on stack") {
  // No OP_DEFINE_GLOBAL; initializer value sits as the local's slot
  auto chunk = compile_source("{ var x = 1; }");
  CHECK(chunk[0] == OP_CONSTANT);
  CHECK(chunk.get_constant(chunk[1]).as_number() == 1.0);
  CHECK(chunk[2] == OP_POPN);
  CHECK(chunk[3] == 1);
  CHECK(chunk[4] == OP_RETURN);
}

TEST_CASE("Compiler: local variable get emits OP_GET_LOCAL") {
  auto chunk = compile_source("{ var x = 1; print x; }");
  // [0] OP_CONSTANT [1] const_idx  <- initializer (slot 0)
  // [2] OP_GET_LOCAL [3] 0         <- read x
  // [4] OP_PRINT
  // [5] OP_POPN [6] 1
  // [7] OP_RETURN
  CHECK(chunk[0] == OP_CONSTANT);
  CHECK(chunk[2] == OP_GET_LOCAL);
  CHECK(chunk[3] == 0);
  CHECK(chunk[4] == OP_PRINT);
  CHECK(chunk[5] == OP_POPN);
  CHECK(chunk[6] == 1);
  CHECK(chunk[7] == OP_RETURN);
}

TEST_CASE("Compiler: local variable set emits OP_SET_LOCAL") {
  auto chunk = compile_source("{ var x = 1; x = 2; }");
  // [0] OP_CONSTANT [1] idx(1.0)   <- initializer (slot 0)
  // [2] OP_CONSTANT [3] idx(2.0)   <- rhs of assignment
  // [4] OP_SET_LOCAL [5] 0         <- assign x
  // [6] OP_POP                     <- expression_statement discards result
  // [7] OP_POPN [8] 1
  // [9] OP_RETURN
  CHECK(chunk[4] == OP_SET_LOCAL);
  CHECK(chunk[5] == 0);
  CHECK(chunk[6] == OP_POP);
  CHECK(chunk[7] == OP_POPN);
}

TEST_CASE("Compiler: multiple locals get correct slot indices") {
  auto chunk = compile_source("{ var x = 1; var y = 2; print y; print x; }");
  // slot 0 = x, slot 1 = y
  // [0] OP_CONSTANT [1] idx(1.0)
  // [2] OP_CONSTANT [3] idx(2.0)
  // [4] OP_GET_LOCAL [5] 1   <- y
  // [6] OP_PRINT
  // [7] OP_GET_LOCAL [8] 0   <- x
  // [9] OP_PRINT
  // [10] OP_POPN [11] 2
  CHECK(chunk[4] == OP_GET_LOCAL);
  CHECK(chunk[5] == 1);
  CHECK(chunk[7] == OP_GET_LOCAL);
  CHECK(chunk[8] == 0);
  CHECK(chunk[10] == OP_POPN);
  CHECK(chunk[11] == 2);
}

TEST_CASE("Compiler: end_scope pops all locals with OP_POPN") {
  auto chunk = compile_source("{ var a = 1; var b = 2; var c = 3; }");
  // Three locals are popped when the block ends
  int popn_offset = 6; // after three OP_CONSTANT pairs
  CHECK(chunk[popn_offset] == OP_POPN);
  CHECK(chunk[popn_offset + 1] == 3);
}

TEST_CASE("Compiler: global variable assignment emits OP_SET_GLOBAL") {
  // Regression: named_variable set_op was accidentally left as OP_GET_GLOBAL
  auto chunk = compile_source("var x = 1; x = 2;");
  // [0] OP_CONSTANT [1] idx(1.0)
  // [2] OP_DEFINE_GLOBAL [3] name_idx
  // [4] OP_CONSTANT [5] idx(2.0)
  // [6] OP_SET_GLOBAL [7] name_idx
  // [8] OP_POP
  // [9] OP_RETURN
  CHECK(chunk[6] == OP_SET_GLOBAL);
}

TEST_CASE("Compiler: global variable slot assignment") {
  SUBCASE("first global gets slot 0") {
    Chunk chunk;
    VM vm;
    Compiler compiler("var x = 1;", vm);
    compiler.compile(chunk);
    CHECK(chunk[2] == OP_DEFINE_GLOBAL);
    CHECK(chunk[3] == 0);
  }

  SUBCASE("second distinct global gets slot 1") {
    Chunk chunk;
    VM vm;
    Compiler compiler("var x = 1; var y = 2;", vm);
    compiler.compile(chunk);
    CHECK(chunk[2] == OP_DEFINE_GLOBAL);
    CHECK(chunk[3] == 0);
    CHECK(chunk[6] == OP_DEFINE_GLOBAL);
    CHECK(chunk[7] == 1);
  }

  SUBCASE("same variable name reuses the same slot across interpret calls") {
    VM vm;
    Chunk chunk1;
    Compiler c1("var a = 1;", vm);
    c1.compile(chunk1);
    Chunk chunk2;
    Compiler c2("var a = 2;", vm);
    c2.compile(chunk2);
    CHECK(chunk1[3] == 0);
    CHECK(chunk2[3] == 0);
  }
}
