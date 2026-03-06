#include <string>
#include <vector>

#include "doctest.h"
#include "scanner.h"

static Token error_token(const char *msg, const int line) {
  return Token{TokenType::Error, msg, static_cast<int>(strlen(msg)), line};
}

static bool is_digit(char ch) { return ch >= '0' && ch <= '9'; }

static bool is_alpha(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

Token Scanner::scan_token() {
  skip_whitespace();
  start = current;
  if (is_at_end()) {
    return make_token(TokenType::Eof);
  }

  char c = advance();
  if (is_alpha(c)) {
    return identifier();
  }
  if (is_digit(c)) {
    return number();
  }

  switch (c) {
  case '(':
    return make_token(TokenType::LeftParen);
  case ')':
    return make_token(TokenType::RightParen);
  case '{':
    return make_token(TokenType::LeftBrace);
  case '}':
    return make_token(TokenType::RightBrace);
  case ';':
    return make_token(TokenType::Semicolon);
  case ',':
    return make_token(TokenType::Comma);
  case '.':
    return make_token(TokenType::Dot);
  case '-':
    return make_token(TokenType::Minus);
  case '+':
    return make_token(TokenType::Plus);
  case '/':
    return make_token(TokenType::Slash);
  case '*':
    return make_token(TokenType::Star);
  case '!':
    return make_token(match('=') ? TokenType::BangEqual : TokenType::Bang);
  case '=':
    return make_token(match('=') ? TokenType::EqualEqual : TokenType::Equal);
  case '<':
    return make_token(match('=') ? TokenType::LessEqual : TokenType::Less);
  case '>':
    return make_token(match('=') ? TokenType::GreaterEqual
                                 : TokenType::Greater);
  case '"':
    return string();
  }

  return error_token("Unexpected character.", line);
}

bool Scanner::is_at_end() { return current >= src.end(); }

Token Scanner::make_token(TokenType type) {
  return Token{type, start, (int)(current - start), line};
}

char Scanner::advance() {
  current++;
  return current[-1];
}

bool Scanner::match(char expected) {
  if (is_at_end()) {
    return false;
  }
  if (*current != expected) {
    return false;
  }
  current++;
  return true;
}

void Scanner::skip_whitespace() {
  while (true) {
    switch (*current) {
    case ' ':
    case '\r':
    case '\t':
      advance();
      break;
    case '\n':
      line++;
      advance();
      break;
    case '/':
      if (peek_next() == '/') {
        while (*current != '\n' && !is_at_end()) {
          advance();
        }
      } else {
        return;
      }
      break;
    default:
      return;
    }
  }
}

char Scanner::peek_next() {
  if (is_at_end()) {
    return '\0';
  }
  return current[1];
}

Token Scanner::string() {
  while (*current != '"' && !is_at_end()) {
    if (*current == '\n') {
      line++;
    }
    advance();
  }
  if (is_at_end()) {
    return error_token("Unterminated string.", line);
  }
  advance();
  return make_token(TokenType::String);
}

Token Scanner::number() {
  while (is_digit(*current)) {
    advance();
  }
  if (*current == '.' && is_digit(peek_next())) {
    advance();
    while (is_digit(*current)) {
      advance();
    }
  }
  return make_token(TokenType::Number);
}

Token Scanner::identifier() {
  while (is_alpha(*current) || is_digit(*current)) {
    advance();
  }
  return make_token(identifier_type());
}

TokenType Scanner::identifier_type() {
  switch (start[0]) {
  case 'a':
    return check_keyword(1, 2, "nd", TokenType::And);
  case 'c':
    return check_keyword(1, 4, "lass", TokenType::Class);
  case 'e':
    return check_keyword(1, 3, "lse", TokenType::Else);
  case 'f':
    if (current - start > 1) {
      switch (start[1]) {
      case 'a':
        return check_keyword(2, 3, "lse", TokenType::False);
      case 'o':
        return check_keyword(2, 1, "r", TokenType::For);
      case 'u':
        return check_keyword(2, 1, "n", TokenType::Fun);
      }
    }
    break;
  case 'i':
    return check_keyword(1, 1, "f", TokenType::If);
  case 'n':
    return check_keyword(1, 2, "il", TokenType::Nil);
  case 'o':
    return check_keyword(1, 1, "r", TokenType::Or);
  case 'p':
    return check_keyword(1, 4, "rint", TokenType::Print);
  case 'r':
    return check_keyword(1, 5, "eturn", TokenType::Return);
  case 's':
    return check_keyword(1, 4, "uper", TokenType::Super);
  case 't':
    if (current - start > 1) {
      switch (start[1]) {
      case 'h':
        return check_keyword(2, 2, "is", TokenType::This);
      case 'r':
        return check_keyword(2, 2, "ue", TokenType::True);
      }
    }
    break;
  case 'v':
    return check_keyword(1, 2, "ar", TokenType::Var);
  case 'w':
    return check_keyword(1, 4, "hile", TokenType::While);
  }
  return TokenType::Identifier;
}

TokenType Scanner::check_keyword(int start_offset, int length, const char *rest,
                                 TokenType type) {
  if (current - start == start_offset + length &&
      memcmp(start + start_offset, rest, length) == 0) {
    return type;
  }
  return TokenType::Identifier;
}

// Tests

static std::vector<Token> scan_all(const std::string &source) {
  Scanner scanner(source);
  std::vector<Token> tokens;
  while (true) {
    Token token = scanner.scan_token();
    tokens.push_back(token);
    if (token.type == TokenType::Eof || token.type == TokenType::Error) {
      break;
    }
  }
  return tokens;
}

TEST_CASE("Scanner: single character tokens") {
  auto tokens = scan_all("(){},.-+;/*");
  REQUIRE(tokens.size() == 12);
  CHECK(tokens[0].type == TokenType::LeftParen);
  CHECK(tokens[1].type == TokenType::RightParen);
  CHECK(tokens[2].type == TokenType::LeftBrace);
  CHECK(tokens[3].type == TokenType::RightBrace);
  CHECK(tokens[4].type == TokenType::Comma);
  CHECK(tokens[5].type == TokenType::Dot);
  CHECK(tokens[6].type == TokenType::Minus);
  CHECK(tokens[7].type == TokenType::Plus);
  CHECK(tokens[8].type == TokenType::Semicolon);
  CHECK(tokens[9].type == TokenType::Slash);
  CHECK(tokens[10].type == TokenType::Star);
  CHECK(tokens[11].type == TokenType::Eof);
}

TEST_CASE("Scanner: one or two character tokens") {
  SUBCASE("single character variants") {
    auto tokens = scan_all("! = < >");
    CHECK(tokens[0].type == TokenType::Bang);
    CHECK(tokens[1].type == TokenType::Equal);
    CHECK(tokens[2].type == TokenType::Less);
    CHECK(tokens[3].type == TokenType::Greater);
  }

  SUBCASE("two character variants") {
    auto tokens = scan_all("!= == <= >=");
    CHECK(tokens[0].type == TokenType::BangEqual);
    CHECK(tokens[1].type == TokenType::EqualEqual);
    CHECK(tokens[2].type == TokenType::LessEqual);
    CHECK(tokens[3].type == TokenType::GreaterEqual);
  }
}

TEST_CASE("Scanner: string literals") {
  SUBCASE("simple string") {
    auto tokens = scan_all("\"hello\"");
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].type == TokenType::String);
    CHECK(std::string_view(tokens[0].start, tokens[0].length) == "\"hello\"");
  }

  SUBCASE("multiline string") {
    auto tokens = scan_all("\"hello\nworld\"");
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].type == TokenType::String);
  }

  SUBCASE("unterminated string") {
    auto tokens = scan_all("\"hello");
    CHECK(tokens.back().type == TokenType::Error);
  }
}

TEST_CASE("Scanner: number literals") {
  SUBCASE("integer") {
    auto tokens = scan_all("123");
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].type == TokenType::Number);
    CHECK(std::string_view(tokens[0].start, tokens[0].length) == "123");
  }

  SUBCASE("decimal") {
    auto tokens = scan_all("3.14");
    REQUIRE(tokens.size() == 2);
    CHECK(tokens[0].type == TokenType::Number);
    CHECK(std::string_view(tokens[0].start, tokens[0].length) == "3.14");
  }

  SUBCASE("trailing dot is not part of number") {
    auto tokens = scan_all("123.");
    CHECK(tokens[0].type == TokenType::Number);
    CHECK(std::string_view(tokens[0].start, tokens[0].length) == "123");
    CHECK(tokens[1].type == TokenType::Dot);
  }
}

TEST_CASE("Scanner: keywords") {
  SUBCASE("all keywords") {
    auto tokens = scan_all("and class else false for fun if nil or print "
                           "return super this true var while");
    CHECK(tokens[0].type == TokenType::And);
    CHECK(tokens[1].type == TokenType::Class);
    CHECK(tokens[2].type == TokenType::Else);
    CHECK(tokens[3].type == TokenType::False);
    CHECK(tokens[4].type == TokenType::For);
    CHECK(tokens[5].type == TokenType::Fun);
    CHECK(tokens[6].type == TokenType::If);
    CHECK(tokens[7].type == TokenType::Nil);
    CHECK(tokens[8].type == TokenType::Or);
    CHECK(tokens[9].type == TokenType::Print);
    CHECK(tokens[10].type == TokenType::Return);
    CHECK(tokens[11].type == TokenType::Super);
    CHECK(tokens[12].type == TokenType::This);
    CHECK(tokens[13].type == TokenType::True);
    CHECK(tokens[14].type == TokenType::Var);
    CHECK(tokens[15].type == TokenType::While);
  }

  SUBCASE("keyword prefixes are identifiers") {
    auto tokens = scan_all("android classy iffy");
    CHECK(tokens[0].type == TokenType::Identifier);
    CHECK(tokens[1].type == TokenType::Identifier);
    CHECK(tokens[2].type == TokenType::Identifier);
  }
}

TEST_CASE("Scanner: identifiers") {
  auto tokens = scan_all("foo _bar baz123");
  CHECK(tokens[0].type == TokenType::Identifier);
  CHECK(std::string_view(tokens[0].start, tokens[0].length) == "foo");
  CHECK(tokens[1].type == TokenType::Identifier);
  CHECK(std::string_view(tokens[1].start, tokens[1].length) == "_bar");
  CHECK(tokens[2].type == TokenType::Identifier);
  CHECK(std::string_view(tokens[2].start, tokens[2].length) == "baz123");
}

TEST_CASE("Scanner: whitespace handling") {
  SUBCASE("spaces and tabs are skipped") {
    auto tokens = scan_all("  \t  +  \t  -");
    CHECK(tokens[0].type == TokenType::Plus);
    CHECK(tokens[1].type == TokenType::Minus);
  }

  SUBCASE("newlines increment line count") {
    auto tokens = scan_all("\n\n+");
    CHECK(tokens[0].type == TokenType::Plus);
    CHECK(tokens[0].line == 3);
  }
}

TEST_CASE("Scanner: comments") {
  SUBCASE("line comment is skipped") {
    auto tokens = scan_all("+ // this is a comment\n-");
    CHECK(tokens[0].type == TokenType::Plus);
    CHECK(tokens[1].type == TokenType::Minus);
  }

  SUBCASE("comment at end of input") {
    auto tokens = scan_all("+ // comment");
    CHECK(tokens[0].type == TokenType::Plus);
    CHECK(tokens[1].type == TokenType::Eof);
  }
}

TEST_CASE("Scanner: eof") {
  auto tokens = scan_all("");
  REQUIRE(tokens.size() == 1);
  CHECK(tokens[0].type == TokenType::Eof);
}

TEST_CASE("Scanner: unexpected character") {
  auto tokens = scan_all("@");
  CHECK(tokens.back().type == TokenType::Error);
}

TEST_CASE("Scanner: line tracking") {
  auto tokens = scan_all("var x = 1;\nvar y = 2;");
  CHECK(tokens[0].line == 1);
  CHECK(tokens[4].line == 1);
  CHECK(tokens[5].line == 2);
}

TEST_CASE("Scanner: complex expression") {
  auto tokens = scan_all("var x = 1 + 2.5;");
  CHECK(tokens[0].type == TokenType::Var);
  CHECK(tokens[1].type == TokenType::Identifier);
  CHECK(tokens[2].type == TokenType::Equal);
  CHECK(tokens[3].type == TokenType::Number);
  CHECK(tokens[4].type == TokenType::Plus);
  CHECK(tokens[5].type == TokenType::Number);
  CHECK(tokens[6].type == TokenType::Semicolon);
  CHECK(tokens[7].type == TokenType::Eof);
}
