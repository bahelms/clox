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
    {&Compiler::grouping, &Compiler::call, Precedence::Call}, // LeftParen
    {NULL, NULL, Precedence::None},                           // RightParen
    {NULL, NULL, Precedence::None},                           // LeftBrace
    {NULL, NULL, Precedence::None},                           // RightBrace
    {NULL, NULL, Precedence::None},                           // Comma
    {NULL, NULL, Precedence::None},                           // Dot
    {&Compiler::unary, &Compiler::binary, Precedence::Term},  // Minus
    {NULL, &Compiler::binary, Precedence::Term},              // Plus
    {NULL, NULL, Precedence::None},                           // Semicolon
    {NULL, &Compiler::binary, Precedence::Factor},            // Slash
    {NULL, &Compiler::binary, Precedence::Factor},            // Star
    {&Compiler::unary, NULL, Precedence::None},               // Bang
    {NULL, &Compiler::binary, Precedence::Equality},          // BangEqual
    {NULL, NULL, Precedence::None},                           // Equal
    {NULL, &Compiler::binary, Precedence::Equality},          // EqualEqual
    {NULL, &Compiler::binary, Precedence::Comparison},        // Greater
    {NULL, &Compiler::binary, Precedence::Comparison},        // GreaterEqual
    {NULL, &Compiler::binary, Precedence::Comparison},        // Less
    {NULL, &Compiler::binary, Precedence::Comparison},        // LessEqual
    {&Compiler::variable, NULL, Precedence::None},            // Identifier
    {&Compiler::string, NULL, Precedence::None},              // String
    {&Compiler::number, NULL, Precedence::None},              // Number
    {NULL, &Compiler::and_, Precedence::And},                 // And
    {NULL, NULL, Precedence::None},                           // Class
    {NULL, NULL, Precedence::None},                           // Else
    {&Compiler::literal, NULL, Precedence::None},             // False
    {NULL, NULL, Precedence::None},                           // For
    {NULL, NULL, Precedence::None},                           // Fun
    {NULL, NULL, Precedence::None},                           // If
    {&Compiler::literal, NULL, Precedence::None},             // Nil
    {NULL, &Compiler::or_, Precedence::Or},                   // Or
    {NULL, NULL, Precedence::None},                           // Print
    {NULL, NULL, Precedence::None},                           // Return
    {NULL, NULL, Precedence::None},                           // Super
    {NULL, NULL, Precedence::None},                           // This
    {&Compiler::literal, NULL, Precedence::None},             // True
    {NULL, NULL, Precedence::None},                           // Var
    {NULL, NULL, Precedence::None},                           // While
    {NULL, NULL, Precedence::None},                           // Error
    {NULL, NULL, Precedence::None},                           // EOF
};

static ParseRule &get_rule(TokenType operator_type) {
  return rules[static_cast<size_t>(operator_type)];
}

ObjFunction *Compiler::compile() {
  while (!match(TokenType::Eof)) {
    declaration();
  }
  ObjFunction *function = end();
  return parser.had_error ? NULL : function;
}

ObjFunction *compile_script(std::string_view src, VM &vm) {
  Parser parser{src};
  parser.advance();
  Compiler compiler{parser, vm, FunctionType::Script};
  return compiler.compile();
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
  if (match(TokenType::Fun)) {
    fun_declaration();
  } else if (match(TokenType::Var)) {
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

void Compiler::fun_declaration() {
  uint8_t global_var_idx = parse_variable("Expect function name.");
  mark_initialized();
  compile_function(FunctionType::Function);
  define_variable(global_var_idx);
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
  return identifier_constant(parser.previous);
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

bool Compiler::identifiers_equal(const Token &a, const Token &b) {
  return std::string_view(a.start, a.length) ==
         std::string_view(b.start, b.length);
}

void Compiler::add_local(const Token &name) {
  if (local_count == UINT8_MAX + 1) {
    parser.error("Too many local variables in function.");
    return;
  }

  Local *local = &locals[local_count++];
  local->name = name;
  local->depth = -1;
}

uint8_t Compiler::identifier_constant(const Token &name) {
  auto slot = vm.get_or_alloc_global_slot(std::string(name.start, name.length));
  if (!slot) {
    parser.error(slot.error());
    return 0;
  }
  return *slot;
}

void Compiler::define_variable(uint8_t global_var_idx) {
  if (scope_depth > 0) {
    mark_initialized();
    return;
  }
  emit_bytes(OP_DEFINE_GLOBAL, global_var_idx);
}

void Compiler::mark_initialized() {
  if (scope_depth == 0) {
    return;
  }
  locals[local_count - 1].depth = scope_depth;
}

void Compiler::compile_function(FunctionType type) {
  Compiler compiler{parser, vm, type, this};
  compiler.function->name = vm.alloc_string(
      std::string(parser.previous.start, parser.previous.length));
  compiler.function_body();
  ObjFunction *fn = compiler.end();
  emit_bytes(OP_CONSTANT, make_constant(Value::object(fn)));
}

void Compiler::function_body() {
  begin_scope();
  parser.consume(TokenType::LeftParen, "Expect '(' after function name.");
  if (!check(TokenType::RightParen)) {
    do {
      function->arity++;
      if (function->arity > 255) {
        parser.error_at_current("Can't have more than 255 parameters.");
      }
      uint8_t constant = parse_variable("Expect parameter name.");
      define_variable(constant);
    } while (match(TokenType::Comma));
  }
  parser.consume(TokenType::RightParen, "Expect ')' after parameters.");
  parser.consume(TokenType::LeftBrace, "Expect '{' before function body.");
  block();
}

// Statements have zero stack effect. After execution of these instructions,
// the stack should be how it was before execution.
void Compiler::statement() {
  if (match(TokenType::Print)) {
    print_statement();
  } else if (match(TokenType::For)) {
    for_statement();
  } else if (match(TokenType::If)) {
    if_statement();
  } else if (match(TokenType::While)) {
    while_statement();
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

void Compiler::for_statement() {
  begin_scope();
  parser.consume(TokenType::LeftParen, "Expect '(' after 'for'.");
  if (match(TokenType::Semicolon)) {
    // no initializer
  } else if (match(TokenType::Var)) {
    var_declaration();
  } else {
    expression_statement();
  }

  int loop_start = current_chunk()->size();
  int exit_jump = -1;
  if (!match(TokenType::Semicolon)) {
    expression();
    parser.consume(TokenType::Semicolon, "Expect ';' after loop condition.");
    exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
  }

  if (!match(TokenType::RightParen)) {
    int body_jump = emit_jump(OP_JUMP);
    int increment_start = current_chunk()->size();
    expression();
    emit_byte(OP_POP);
    parser.consume(TokenType::RightParen, "Expect ')' after for clauses.");
    emit_loop(loop_start);
    loop_start = increment_start;
    patch_jump(body_jump);
  }

  statement();
  emit_loop(loop_start);
  if (exit_jump != -1) {
    patch_jump(exit_jump);
    emit_byte(OP_POP);
  }
  end_scope();
}

void Compiler::if_statement() {
  parser.consume(TokenType::LeftParen, "Expect '(' after 'if'.");
  expression();
  parser.consume(TokenType::RightParen, "Expect ')' after condition.");

  int then_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  statement();
  int else_jump = emit_jump(OP_JUMP);
  patch_jump(then_jump);
  emit_byte(OP_POP);

  if (match(TokenType::Else)) {
    statement();
  }
  patch_jump(else_jump);
}

int Compiler::emit_jump(OpCode op) {
  emit_byte(op);
  emit_byte(0xff);
  emit_byte(0xff);
  return current_chunk()->size() - 2;
}

void Compiler::patch_jump(int instr_offset) {
  int jump = current_chunk()->size() - instr_offset - 2;
  if (jump > UINT16_MAX) {
    parser.error("Too much code to jump over.");
  }

  current_chunk()->set_offset(instr_offset, (jump >> 8) & 0xff);
  current_chunk()->set_offset(instr_offset + 1, jump & 0xff);
}

void Compiler::while_statement() {
  int loop_start = current_chunk()->size();
  parser.consume(TokenType::LeftParen, "Expect '(' after 'while'.");
  expression();
  parser.consume(TokenType::RightParen, "Expect ')' after condition.");

  int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  statement();
  emit_loop(loop_start);
  patch_jump(exit_jump);
  emit_byte(OP_POP);
}

void Compiler::emit_loop(int loop_start) {
  emit_byte(OP_LOOP);
  int offset = current_chunk()->size() - loop_start + 2;
  if (offset > UINT16_MAX) {
    parser.error("Loop body too large.");
  }
  emit_byte((offset >> 8) & 0xff);
  emit_byte(offset & 0xff);
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

void Compiler::call(bool can_assign) {
  uint8_t arg_count = argument_list();
  emit_bytes(OP_CALL, arg_count);
}

uint8_t Compiler::argument_list() {
  uint8_t arg_count{};
  if (!check(TokenType::RightParen)) {
    do {
      expression();
      if (arg_count == 255) {
        parser.error("Can't have more than 255 arguments.");
      }
      arg_count++;
    } while (match(TokenType::Comma));
  }
  parser.consume(TokenType::RightParen, "Expect ')' after arguments.");
  return arg_count;
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

void Compiler::named_variable(const Token &name, bool can_assign) {
  uint8_t get_op{}, set_op{};
  int arg = resolve_local(name);
  if (arg != -1) {
    get_op = OP_GET_LOCAL;
    set_op = OP_SET_LOCAL;
  } else {
    arg = identifier_constant(name);
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

void Compiler::and_(bool can_assign) {
  int end_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  parse_precedence(Precedence::And);
  patch_jump(end_jump);
}

void Compiler::or_(bool can_assign) {
  int else_jump = emit_jump(OP_JUMP_IF_FALSE);
  int end_jump = emit_jump(OP_JUMP);
  patch_jump(else_jump);
  emit_byte(OP_POP);
  parse_precedence(Precedence::Or);
  patch_jump(end_jump);
}

ObjFunction *Compiler::end() {
  emit_return();
#ifdef DEBUG_PRINT_CODE
  if (!parser.had_error) {
    disassemble_chunk(*current_chunk(),
                      function->name ? function->name->chars : "<script>");
  }
#endif
  return function;
}

void Compiler::emit_return() { emit_byte(OP_RETURN); }

void Compiler::emit_byte(uint8_t byte) {
  current_chunk()->write(byte, parser.previous.line);
}

void Compiler::emit_bytes(uint8_t byte1, uint8_t byte2) {
  emit_byte(byte1);
  emit_byte(byte2);
}

void Compiler::emit_constant(Value value) {
  emit_bytes(OP_CONSTANT, make_constant(value));
}

uint8_t Compiler::make_constant(Value value) {
  int index = current_chunk()->write_constant(value);
  if (index > UINT8_MAX) {
    parser.error("Too many constants in one chunk.");
    return 0;
  }
  return index;
}

static Chunk compile_source(std::string_view src) {
  VM vm;
  return *compile_script(src, vm)->chunk;
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

TEST_CASE("Compiler: compile returns null on error") {
  VM vm;
  suppress_stderr([&] { CHECK(compile_script("@", vm) == nullptr); });
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
  // [0] OP_CONSTANT [1] const_idx  <- initializer (slot 1; slot 0 is the
  // script) [2] OP_GET_LOCAL [3] 1         <- read x [4] OP_PRINT [5] OP_POPN
  // [6] 1 [7] OP_RETURN
  CHECK(chunk[0] == OP_CONSTANT);
  CHECK(chunk[2] == OP_GET_LOCAL);
  CHECK(chunk[3] == 1);
  CHECK(chunk[4] == OP_PRINT);
  CHECK(chunk[5] == OP_POPN);
  CHECK(chunk[6] == 1);
  CHECK(chunk[7] == OP_RETURN);
}

TEST_CASE("Compiler: local variable set emits OP_SET_LOCAL") {
  auto chunk = compile_source("{ var x = 1; x = 2; }");
  // [0] OP_CONSTANT [1] idx(1.0)   <- initializer (slot 1; slot 0 is the
  // script) [2] OP_CONSTANT [3] idx(2.0)   <- rhs of assignment [4]
  // OP_SET_LOCAL [5] 1         <- assign x [6] OP_POP                     <-
  // expression_statement discards result [7] OP_POPN [8] 1 [9] OP_RETURN
  CHECK(chunk[4] == OP_SET_LOCAL);
  CHECK(chunk[5] == 1);
  CHECK(chunk[6] == OP_POP);
  CHECK(chunk[7] == OP_POPN);
}

TEST_CASE("Compiler: multiple locals get correct slot indices") {
  auto chunk = compile_source("{ var x = 1; var y = 2; print y; print x; }");
  // slot 0 = script, slot 1 = x, slot 2 = y
  // [0] OP_CONSTANT [1] idx(1.0)
  // [2] OP_CONSTANT [3] idx(2.0)
  // [4] OP_GET_LOCAL [5] 2   <- y
  // [6] OP_PRINT
  // [7] OP_GET_LOCAL [8] 1   <- x
  // [9] OP_PRINT
  // [10] OP_POPN [11] 2
  CHECK(chunk[4] == OP_GET_LOCAL);
  CHECK(chunk[5] == 2);
  CHECK(chunk[7] == OP_GET_LOCAL);
  CHECK(chunk[8] == 1);
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
    auto chunk = compile_source("var x = 1;");
    CHECK(chunk[2] == OP_DEFINE_GLOBAL);
    CHECK(chunk[3] == 0);
  }

  SUBCASE("second distinct global gets slot 1") {
    auto chunk = compile_source("var x = 1; var y = 2;");
    CHECK(chunk[2] == OP_DEFINE_GLOBAL);
    CHECK(chunk[3] == 0);
    CHECK(chunk[6] == OP_DEFINE_GLOBAL);
    CHECK(chunk[7] == 1);
  }

  SUBCASE("same variable name reuses the same slot across interpret calls") {
    VM vm;
    Chunk chunk1 = *compile_script("var a = 1;", vm)->chunk;
    Chunk chunk2 = *compile_script("var a = 2;", vm)->chunk;
    CHECK(chunk1[3] == 0);
    CHECK(chunk2[3] == 0);
  }
}

TEST_CASE("Compiler: and operator") {
  auto chunk = compile_source("true and false;");
  // 0:  OP_TRUE
  // 1:  OP_JUMP_IF_FALSE  2: 0  3: 2   (→ pos 6, expression OP_POP)
  // 4:  OP_POP
  // 5:  OP_FALSE
  // 6:  OP_POP   (expression_statement)
  // 7:  OP_RETURN
  CHECK(chunk[0] == OP_TRUE);
  CHECK(chunk[1] == OP_JUMP_IF_FALSE);
  CHECK(chunk[2] == 0);
  CHECK(chunk[3] == 2);
  CHECK(chunk[4] == OP_POP);
  CHECK(chunk[5] == OP_FALSE);
  CHECK(chunk[6] == OP_POP);
  CHECK(chunk[7] == OP_RETURN);
}

TEST_CASE("Compiler: or operator") {
  auto chunk = compile_source("true or false;");
  // 0:  OP_TRUE
  // 1:  OP_JUMP_IF_FALSE  2: 0  3: 3   (→ pos 7, OP_POP before RHS)
  // 4:  OP_JUMP  5: 0  6: 2            (→ pos 9, expression OP_POP)
  // 7:  OP_POP
  // 8:  OP_FALSE
  // 9:  OP_POP   (expression_statement)
  // 10: OP_RETURN
  CHECK(chunk[0] == OP_TRUE);
  CHECK(chunk[1] == OP_JUMP_IF_FALSE);
  CHECK(chunk[2] == 0);
  CHECK(chunk[3] == 3);
  CHECK(chunk[4] == OP_JUMP);
  CHECK(chunk[5] == 0);
  CHECK(chunk[6] == 2);
  CHECK(chunk[7] == OP_POP);
  CHECK(chunk[8] == OP_FALSE);
  CHECK(chunk[9] == OP_POP);
  CHECK(chunk[10] == OP_RETURN);
}

TEST_CASE("Compiler: while statement") {
  auto chunk = compile_source("while (true) print \"yes\";");
  // 0:  OP_TRUE
  // 1:  OP_JUMP_IF_FALSE  2: 0  3: 7   (→ pos 11, exit OP_POP)
  // 4:  OP_POP
  // 5:  OP_CONSTANT  6: const_idx
  // 7:  OP_PRINT
  // 8:  OP_LOOP  9: 0  10: 11          (→ back to pos 0)
  // 11: OP_POP
  // 12: OP_RETURN
  CHECK(chunk[0] == OP_TRUE);
  CHECK(chunk[1] == OP_JUMP_IF_FALSE);
  CHECK(chunk[2] == 0);
  CHECK(chunk[3] == 7);
  CHECK(chunk[4] == OP_POP);
  CHECK(chunk[5] == OP_CONSTANT);
  CHECK(chunk[7] == OP_PRINT);
  CHECK(chunk[8] == OP_LOOP);
  CHECK(chunk[9] == 0);
  CHECK(chunk[10] == 11);
  CHECK(chunk[11] == OP_POP);
  CHECK(chunk[12] == OP_RETURN);
}

TEST_CASE("Compiler: if statement") {
  SUBCASE("if without else emits JUMP_IF_FALSE, then-branch, JUMP, POP") {
    auto chunk = compile_source("if (true) print \"yes\";");
    // 0:  OP_TRUE
    // 1:  OP_JUMP_IF_FALSE  2: 0  3: 7   (→ pos 11)
    // 4:  OP_POP
    // 5:  OP_CONSTANT  6: 0
    // 7:  OP_PRINT
    // 8:  OP_JUMP  9: 0  10: 1  (→ pos 12)
    // 11: OP_POP
    // 12: OP_RETURN
    CHECK(chunk[0] == OP_TRUE);
    CHECK(chunk[1] == OP_JUMP_IF_FALSE);
    CHECK(chunk[2] == 0);
    CHECK(chunk[3] == 7);
    CHECK(chunk[4] == OP_POP);
    CHECK(chunk[5] == OP_CONSTANT);
    CHECK(chunk[7] == OP_PRINT);
    CHECK(chunk[8] == OP_JUMP);
    CHECK(chunk[9] == 0);
    CHECK(chunk[10] == 1);
    CHECK(chunk[11] == OP_POP);
    CHECK(chunk[12] == OP_RETURN);
  }
  SUBCASE("if-else emits correct jump offsets around both branches") {
    auto chunk = compile_source("if (true) print \"yes\"; else print \"no\";");
    // 0:  OP_TRUE
    // 1:  OP_JUMP_IF_FALSE  2: 0  3: 7   (→ pos 11)
    // 4:  OP_POP
    // 5:  OP_CONSTANT  6: 0   ("yes")
    // 7:  OP_PRINT
    // 8:  OP_JUMP  9: 0  10: 4  (→ pos 15)
    // 11: OP_POP
    // 12: OP_CONSTANT  13: 1  ("no")
    // 14: OP_PRINT
    // 15: OP_RETURN
    CHECK(chunk[0] == OP_TRUE);
    CHECK(chunk[1] == OP_JUMP_IF_FALSE);
    CHECK(chunk[2] == 0);
    CHECK(chunk[3] == 7);
    CHECK(chunk[4] == OP_POP);
    CHECK(chunk[7] == OP_PRINT);
    CHECK(chunk[8] == OP_JUMP);
    CHECK(chunk[9] == 0);
    CHECK(chunk[10] == 4);
    CHECK(chunk[11] == OP_POP);
    CHECK(chunk[14] == OP_PRINT);
    CHECK(chunk[15] == OP_RETURN);
  }
}

TEST_CASE("Compiler: for statement") {
  SUBCASE("infinite loop emits body then LOOP back to start") {
    auto chunk = compile_source(R"(for (;;) print "x";)");
    // 0:  OP_CONSTANT  0 (const idx)
    // 2:  OP_PRINT
    // 3:  OP_LOOP  0  6  (→ back to pos 0)
    // 6:  OP_POPN  0 (count)
    // 8:  OP_RETURN
    CHECK(chunk[0] == OP_CONSTANT);
    CHECK(chunk[2] == OP_PRINT);
    CHECK(chunk[3] == OP_LOOP);
    CHECK(chunk[4] == 0);
    CHECK(chunk[5] == 6);
    CHECK(chunk[6] == OP_POPN);
    CHECK(chunk[7] == 0);
    CHECK(chunk[8] == OP_RETURN);
  }

  SUBCASE("condition-only for emits JUMP_IF_FALSE around body and exit pop") {
    auto chunk = compile_source(R"(for (; false;) print "x";)");
    // 0:  OP_FALSE
    // 1:  OP_JUMP_IF_FALSE  0  7  (→ pos 11, exit pop)
    // 4:  OP_POP
    // 5:  OP_CONSTANT  0 (const idx)
    // 7:  OP_PRINT
    // 8:  OP_LOOP  0  11  (→ back to pos 0)
    // 11: OP_POP
    // 12: OP_POPN  0 (count)
    // 14: OP_RETURN
    CHECK(chunk[0] == OP_FALSE);
    CHECK(chunk[1] == OP_JUMP_IF_FALSE);
    CHECK(chunk[2] == 0);
    CHECK(chunk[3] == 7);
    CHECK(chunk[4] == OP_POP);
    CHECK(chunk[5] == OP_CONSTANT);
    CHECK(chunk[7] == OP_PRINT);
    CHECK(chunk[8] == OP_LOOP);
    CHECK(chunk[9] == 0);
    CHECK(chunk[10] == 11);
    CHECK(chunk[11] == OP_POP);
    CHECK(chunk[12] == OP_POPN);
    CHECK(chunk[13] == 0);
    CHECK(chunk[14] == OP_RETURN);
  }

  SUBCASE(
      "full for loop: var init, condition, increment emits correct structure") {
    auto chunk = compile_source("for (var i = 0; i < 3; i = i + 1) print i;");
    // 0:  OP_CONSTANT  0 (const idx, 0.0)  <- var i = 0 (slot 1; slot 0 is
    // script) 2:  OP_GET_LOCAL  1 (slot)           <- condition: load i 4:
    // OP_CONSTANT  1 (const idx, 3.0) 6:  OP_LESS 7:  OP_JUMP_IF_FALSE  0  21
    // (→ pos 31, exit pop) 10: OP_POP 11: OP_JUMP  0  11  (→ pos 25, body) 14:
    // OP_GET_LOCAL  1 (slot)    <- increment: load i 16: OP_CONSTANT  2 (const
    // idx, 1.0) 18: OP_ADD 19: OP_SET_LOCAL  1 (slot)    <- store into i 21:
    // OP_POP 22: OP_LOOP  0  23  (→ back to pos 2, condition) 25: OP_GET_LOCAL
    // 1 (slot)    <- body: load i 27: OP_PRINT 28: OP_LOOP  0  17  (→ back to
    // pos 14, increment) 31: OP_POP                    <- exit: pop condition
    // value 32: OP_POPN  1 (count)        <- pop i 34: OP_RETURN
    CHECK(chunk[0] == OP_CONSTANT);
    CHECK(chunk[0] == 0);
    CHECK(chunk.get_constant(chunk[1]).as_number() == 0.0);
    CHECK(chunk[2] == OP_GET_LOCAL);
    CHECK(chunk[3] == 1);
    CHECK(chunk[6] == OP_LESS);
    CHECK(chunk[7] == OP_JUMP_IF_FALSE);
    CHECK(chunk[8] == 0);
    CHECK(chunk[9] == 21);
    CHECK(chunk[10] == OP_POP);
    CHECK(chunk[11] == OP_JUMP);
    CHECK(chunk[12] == 0);
    CHECK(chunk[13] == 11);
    CHECK(chunk[22] == OP_LOOP);
    CHECK(chunk[23] == 0);
    CHECK(chunk[24] == 23);
    CHECK(chunk[25] == OP_GET_LOCAL);
    CHECK(chunk[27] == OP_PRINT);
    CHECK(chunk[28] == OP_LOOP);
    CHECK(chunk[29] == 0);
    CHECK(chunk[30] == 17);
    CHECK(chunk[31] == OP_POP);
    CHECK(chunk[32] == OP_POPN);
    CHECK(chunk[33] == 1);
    CHECK(chunk[34] == OP_RETURN);
  }
}

TEST_CASE(
    "Compiler: top-level program compiles to a nameless <script> function") {
  VM vm;
  ObjFunction *script = compile_script("1 + 2;", vm);
  REQUIRE(script != nullptr);
  CHECK(script->name == nullptr);
  CHECK(script->arity == 0);
}

TEST_CASE("Compiler: function declaration") {
  // Function objects live in the VM; keep it alive while inspecting them.
  SUBCASE(
      "emits OP_CONSTANT <fn> then OP_DEFINE_GLOBAL in the enclosing chunk") {
    VM vm;
    Chunk &chunk = *compile_script("fun f() {}", vm)->chunk;
    // [0] OP_CONSTANT [1] fn_idx [2] OP_DEFINE_GLOBAL [3] slot [4] OP_RETURN
    CHECK(chunk[0] == OP_CONSTANT);
    CHECK(chunk[2] == OP_DEFINE_GLOBAL);
    CHECK(chunk[4] == OP_RETURN);

    ObjFunction *f = chunk.get_constant(chunk[1]).as_function();
    CHECK(f->name->chars == "f");
    CHECK(f->arity == 0);
    CHECK((*f->chunk)[0] == OP_RETURN); // empty body is just a return
  }

  SUBCASE("parameters set arity and become locals") {
    VM vm;
    Chunk &chunk = *compile_script("fun f(a, b) { print a; }", vm)->chunk;
    ObjFunction *f = chunk.get_constant(chunk[1]).as_function();
    CHECK(f->arity == 2);
    // Parameter `a` is local slot 1 (slot 0 is the reserved function slot),
    // read via OP_GET_LOCAL rather than OP_GET_GLOBAL.
    Chunk &body = *f->chunk;
    CHECK(body[0] == OP_GET_LOCAL);
    CHECK(body[1] == 1);
    CHECK(body[2] == OP_PRINT);
    CHECK(body[3] == OP_RETURN);
  }

  SUBCASE("nested function compiles without rewinding the cursor") {
    // Regression guard: a fresh sub-compiler must share the parser cursor so
    // the inner function is parsed in place, not from the start of source.
    VM vm;
    Chunk &chunk = *compile_script("fun outer() { fun inner() {} }", vm)->chunk;
    ObjFunction *outer = chunk.get_constant(chunk[1]).as_function();
    CHECK(outer->name->chars == "outer");
    // `inner` is a local inside outer, emitted as a constant in outer's chunk.
    Chunk &outer_body = *outer->chunk;
    CHECK(outer_body[0] == OP_CONSTANT);
    ObjFunction *inner = outer_body.get_constant(outer_body[1]).as_function();
    CHECK(inner->name->chars == "inner");
  }
}
