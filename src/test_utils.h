#pragma once
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

inline std::string capture_stdout(std::function<void()> fn) {
  std::ostringstream captured;
  std::streambuf *old = std::cout.rdbuf(captured.rdbuf());
  fn();
  std::cout.rdbuf(old);
  return captured.str();
}
