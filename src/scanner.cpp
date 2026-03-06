#include <string>

#include "scanner.h"

Token error_token(const char *msg, const int line) {
  return Token{TokenType::Error, msg, static_cast<int>(strlen(msg)), line};
}

bool is_digit(char ch) { return ch >= '0' && ch <= '9'; }

bool is_alpha(char ch) {
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
    return check_keyword(1, 4, "lse", TokenType::Else);
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
