#include <cstdarg>
#include <cstdint>
#include <ctime>
#include <functional>
#include <iostream>
#include <ranges>
#include <sys/types.h>

#include "chunk.h"
#include "compiler.h"
#include "doctest.h"
#include "object.h"
#include "test_utils.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

VM::VM() {
  stack_top = stack;
  define_native("clock", clock_native, 0);
}

VM::~VM() {
  Object *obj = objects;
  while (obj) {
    Object *next = obj->next;
    delete obj;
    obj = next;
  }
}

InterpretResult VM::interpret(std::string source) {
  ObjFunction *function = compile_script(source, *this);
  if (!function) {
    return InterpretResult::CompileError;
  }

  push(Value::object(function));
  ObjClosure *closure = alloc_closure(function);
  pop();
  push(Value::object(closure));
  call(closure, 0);

  return run();
}

InterpretResult VM::run() {
  CallFrame *frame = &frames[frame_count - 1];
  const uint8_t *ip = frame->ip;

  auto read_byte = [&ip]() -> uint8_t { return *ip++; };
  auto read_short = [&ip]() -> uint16_t {
    ip += 2;
    return static_cast<uint16_t>(ip[-2] << 8 | ip[-1]);
  };
  auto read_constant = [&]() -> Value {
    return frame->closure->function->chunk->get_constant(read_byte());
  };
  auto operands_must_be_numbers = [&] {
    frame->ip = ip;
    runtime_error("Operands must be numbers.");
  };

  while (true) {
#ifdef DEBUG_TRACE_EXECUTION
    print_stack(stack, stack_top);
    disassemble_instruction(
        *frame->closure->function->chunk,
        static_cast<int>(ip - frame->function->chunk->data()));
#endif

    uint8_t instr = read_byte();
    switch (instr) {
    case OP_CONSTANT: {
      push(read_constant());
      break;
    }
    case OP_NIL:
      push(Value::nil());
      break;
    case OP_TRUE:
      push(Value::boolean(true));
      break;
    case OP_FALSE:
      push(Value::boolean(false));
      break;
    case OP_POP:
      pop();
      break;
    case OP_POPN:
      for (int i = read_byte(); i > 0; i--) {
        pop();
      }
      break;
    case OP_GET_LOCAL: {
      uint8_t slot = read_byte();
      push(frame->slots[slot]);
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = read_byte();
      frame->slots[slot] = peek(0);
      break;
    }
    case OP_GET_GLOBAL: {
      uint8_t slot = read_byte();
      if (!globals_defined[slot]) {
        frame->ip = ip;
        runtime_error("Undefined variable '{}'", global_names[slot]);
        return InterpretResult::RuntimeError;
      }
      push(globals[slot]);
      break;
    }
    case OP_DEFINE_GLOBAL: {
      uint8_t slot = read_byte();
      globals[slot] = peek(0);
      globals_defined[slot] = true;
      pop();
      break;
    }
    case OP_SET_GLOBAL: {
      uint8_t slot = read_byte();
      if (!globals_defined[slot]) {
        frame->ip = ip;
        runtime_error("Undefined variable '{}'", global_names[slot]);
        return InterpretResult::RuntimeError;
      }
      globals[slot] = peek(0);
      break;
    }
    case OP_GET_UPVALUE: {
      uint8_t slot = read_byte();
      push(*frame->closure->upvalues[slot]->location);
      break;
    }
    case OP_SET_UPVALUE: {
      uint8_t slot = read_byte();
      *frame->closure->upvalues[slot]->location = peek(0);
      break;
    }
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(Value::boolean(values_equal(a, b)));
      break;
    }
    case OP_NOT_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(Value::boolean(!values_equal(a, b)));
      break;
    }
    case OP_GREATER:
      if (!binary_op(&Value::boolean, std::greater<double>{})) {
        operands_must_be_numbers();
        return InterpretResult::RuntimeError;
      }
      break;
    case OP_GREATER_EQUAL:
      if (!binary_op(&Value::boolean, std::greater_equal<double>{})) {
        operands_must_be_numbers();
        return InterpretResult::RuntimeError;
      }
      break;
    case OP_LESS:
      if (!binary_op(&Value::boolean, std::less<double>{})) {
        operands_must_be_numbers();
        return InterpretResult::RuntimeError;
      }
      break;
    case OP_LESS_EQUAL:
      if (!binary_op(&Value::boolean, std::less_equal<double>{})) {
        operands_must_be_numbers();
        return InterpretResult::RuntimeError;
      }
      break;
    case OP_ADD:
      if (peek(0).is_string() && peek(1).is_string()) {
        ObjString *str_b = pop().as_string();
        ObjString *str_a = pop().as_string();
        push(Value::object(alloc_string(str_a->chars + str_b->chars)));
      } else if (peek(0).is_number() && peek(1).is_number()) {
        double b = pop().as_number();
        double a = pop().as_number();
        push(Value::number(a + b));
      } else {
        frame->ip = ip;
        runtime_error("Operands must be numbers or strings.");
        return InterpretResult::RuntimeError;
      }
      break;
    case OP_SUBTRACT:
      if (!binary_op(&Value::number, std::minus<double>{})) {
        operands_must_be_numbers();
        return InterpretResult::RuntimeError;
      }
      break;
    case OP_MULTIPLY:
      if (!binary_op(&Value::number, std::multiplies<double>{})) {
        operands_must_be_numbers();
        return InterpretResult::RuntimeError;
      }
      break;
    case OP_DIVIDE:
      if (!binary_op(&Value::number, std::divides<double>{})) {
        operands_must_be_numbers();
        return InterpretResult::RuntimeError;
      }
      break;
    case OP_NOT:
      push(Value::boolean(is_falsey(pop())));
      break;
    case OP_NEGATE:
      if (!peek(0).is_number()) {
        frame->ip = ip;
        runtime_error("Operand must be a number.");
        return InterpretResult::RuntimeError;
      }

      push(Value::number(-pop().as_number()));
      break;
    case OP_PRINT: {
      print_value(pop());
      std::cout << '\n';
      break;
    }
    case OP_JUMP: {
      ip += read_short();
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = read_short();
      if (is_falsey(peek(0))) {
        ip += offset;
      }
      break;
    }
    case OP_LOOP: {
      ip -= read_short();
      break;
    }
    case OP_CALL: {
      int arg_count = read_byte();
      frame->ip = ip;
      if (!call_value(peek(arg_count), arg_count)) {
        return InterpretResult::RuntimeError;
      }
      frame = &frames[frame_count - 1];
      ip = frame->ip;
      break;
    }
    case OP_CLOSURE: {
      ObjFunction *function = read_constant().as_function();
      ObjClosure *closure = alloc_closure(function);
      push(Value::object(closure));

      for (ObjUpvalue *&upvalue : closure->upvalues) {
        uint8_t is_local = read_byte();
        uint8_t index = read_byte();
        if (is_local) {
          upvalue = capture_upvalue(frame->slots + index);
        } else {
          upvalue = frame->closure->upvalues[index];
        }
      }
      break;
    }
    case OP_RETURN: {
      Value result = pop();
      frame_count--;
      if (frame_count == 0) {
        pop();
        return InterpretResult::Ok;
      };
      stack_top = frames[frame_count].slots;
      push(result);
      frame = &frames[frame_count - 1];
      ip = frame->ip;
      break;
    }
    default:
      std::cerr << "Unknown opcode: " << static_cast<int>(instr) << '\n';
      return InterpretResult::RuntimeError;
    }
  }
}

void VM::push(Value value) {
  *stack_top = value;
  stack_top++;
}

Value VM::pop() {
  stack_top--;
  return *stack_top;
}

Value VM::peek(int distance) { return stack_top[-1 - distance]; }

void VM::reset_stack() { stack_top = stack; }

bool VM::call_value(Value callee, int arg_count) {
  if (callee.is_closure()) {
    return call(callee.as_closure(), arg_count);
  } else if (callee.is_native()) {
    ObjNative *native = callee.as_native();
    if (arg_count != native->arity) {
      runtime_error("Expected {} arguments but got {}.", native->arity,
                    arg_count);
      return false;
    }
    Value result = native->function(arg_count, stack_top - arg_count);
    stack_top -= arg_count + 1;
    push(result);
    return true;
  }
  runtime_error("Can only call functions and classes.");
  return false;
}

bool VM::call(ObjClosure *closure, int arg_count) {
  if (arg_count != closure->function->arity) {
    runtime_error("Expected {} arguments but got {}.", closure->function->arity,
                  arg_count);
    return false;
  }

  if (frame_count == FRAMES_MAX) {
    runtime_error("Stack overflow.");
    return false;
  }

  CallFrame &frame = frames[frame_count++];
  frame.closure = closure;
  frame.ip = closure->function->chunk->data();
  frame.slots = stack_top - arg_count - 1;
  return true;
}

std::expected<uint8_t, const char *>
VM::get_or_alloc_global_slot(const std::string &name) {
  auto it = global_slots.find(name);
  if (it != global_slots.end()) {
    return it->second;
  }
  if (global_slots.size() == 256) {
    return std::unexpected("Too many global variables in one program.");
  }
  uint8_t slot = static_cast<uint8_t>(global_slots.size());
  global_slots.emplace(name, slot);
  globals.push_back(Value::nil());
  globals_defined.push_back(false);
  global_names.push_back(name);
  return slot;
}

ObjString *VM::alloc_string(std::string s) {
  auto [it, inserted] = interned_strings.try_emplace(std::move(s), nullptr);
  if (!inserted) {
    return it->second;
  }
  auto *obj = new ObjString(it->first);
  obj->next = objects;
  objects = obj;
  it->second = obj;
  return obj;
}

ObjNative *VM::alloc_native(NativeFn function, int arity) {
  auto *obj = new ObjNative(function, arity);
  obj->next = objects;
  objects = obj;
  return obj;
}

ObjClosure *VM::alloc_closure(ObjFunction *fn) {
  auto *obj = new ObjClosure(fn);
  obj->next = objects;
  objects = obj;
  return obj;
}

ObjUpvalue *VM::capture_upvalue(Value *local) {
  auto *obj = new ObjUpvalue(local);
  // does it need to be put in the objects?
  // obj->next = objects;
  // objects = obj;
  return obj;
}

template <typename... Args>
void VM::runtime_error(std::format_string<Args...> fmt, Args &&...args) {
  std::cerr << std::format(fmt, std::forward<Args>(args)...) << '\n';

  CallFrame &frame = frames[frame_count - 1];
  size_t instruction = frame.ip - frame.closure->function->chunk->data() - 1;
  int line = frame.closure->function->chunk->get_line(instruction);
  std::cerr << std::format("[line {}] in script\n", line);

  for (CallFrame &frame :
       std::span(frames).first(frame_count) | std::views::reverse) {
    ObjFunction *function = frame.closure->function;
    size_t instruction = frame.ip - function->chunk->data() - 1;
    std::print(stderr, "[line {}] in ", function->chunk->get_line(instruction));
    if (!function->name) {
      std::println(stderr, "script");
    } else {
      std::println(stderr, "{}()", function->name->chars);
    }
  }
  reset_stack();
}

void VM::define_native(const char *name, NativeFn function, int arity) {
  uint8_t slot = get_or_alloc_global_slot(name).value();
  globals[slot] = Value::object(alloc_native(function, arity));
  globals_defined[slot] = true;
}

template <typename ValueBuilder, typename Op>
bool VM::binary_op(ValueBuilder builder, Op op) {
  if (!peek(0).is_number() || !peek(1).is_number()) {
    return false;
  }

  double b = pop().as_number();
  double a = pop().as_number();
  push(builder(op(a, b)));
  return true;
}

Value clock_native(int arg_count, Value *args) {
  return Value::number((double)clock() / CLOCKS_PER_SEC);
}

TEST_CASE("VM::alloc_string") {
  VM vm{};

  SUBCASE("returns same pointer for equal strings") {
    ObjString *a = vm.alloc_string("hello");
    ObjString *b = vm.alloc_string("hello");
    CHECK(a == b);
  }

  SUBCASE("returns different pointers for different strings") {
    ObjString *a = vm.alloc_string("hello");
    ObjString *b = vm.alloc_string("world");
    CHECK(a != b);
  }

  SUBCASE("interned string has correct content") {
    ObjString *a = vm.alloc_string("hello");
    CHECK(a->chars == "hello");
  }

  SUBCASE("empty string is interned") {
    ObjString *a = vm.alloc_string("");
    ObjString *b = vm.alloc_string("");
    CHECK(a == b);
    CHECK(a->chars == "");
  }
}

TEST_CASE("VM::interpret") {
  VM vm{};

  SUBCASE("returns Ok") {
    std::string output = capture_stdout(
        [&] { CHECK(vm.interpret("1 + 2;") == InterpretResult::Ok); });
  }

  SUBCASE("equality with mixed types") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print !(5 - 4 > 3 * 2 == !nil);") ==
            InterpretResult::Ok);
    });
    CHECK(output == "true\n");
  }

  SUBCASE("new chunk is used on each invocation") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print !(5 - 4 > 3 * 2 == !nil);") ==
            InterpretResult::Ok);
    });
    CHECK(output == "true\n");

    output = capture_stdout(
        [&] { CHECK(vm.interpret("print 4 + 5;") == InterpretResult::Ok); });
    CHECK(output == "9\n");
  }

  SUBCASE("equal strings are equal") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print \"hello\" == \"hello\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "true\n");
  }

  SUBCASE("different strings are not equal") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print \"hello\" == \"world\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "false\n");
  }

  SUBCASE("string concatenation") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print \"hello\" + \" \" + \"world\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "hello world\n");
  }

  SUBCASE("concatenated string is interned") {
    ObjString *a = vm.alloc_string("hello world");
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("print \"hello\" + \" world\" == \"hello world\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "true\n");
  }

  SUBCASE("returns CompileError on invalid syntax") {
    suppress_stderr(
        [&] { CHECK(vm.interpret("@") == InterpretResult::CompileError); });
  }

  SUBCASE("returns RuntimeError when negating a non-number") {
    capture_stderr([&] {
      CHECK(vm.interpret("-\"hello\";") == InterpretResult::RuntimeError);
    });
  }

  SUBCASE("returns RuntimeError for mixed types in addition") {
    capture_stderr([&] {
      CHECK(vm.interpret("\"hello\" + 1;") == InterpretResult::RuntimeError);
    });
  }

  SUBCASE("local variable is readable within its scope") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("{ var x = 42; print x; }") == InterpretResult::Ok);
    });
    CHECK(output == "42\n");
  }

  SUBCASE("local variable assignment") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("{ var x = 1; x = 2; print x; }") ==
            InterpretResult::Ok);
    });
    CHECK(output == "2\n");
  }

  SUBCASE("local variable is not accessible outside its scope") {
    capture_stderr([&] {
      CHECK(vm.interpret("{ var x = 1; } print x;") ==
            InterpretResult::RuntimeError);
    });
  }

  SUBCASE("nested scopes can access outer locals") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("{ var x = 10; { var y = 20; print x + y; } }") ==
            InterpretResult::Ok);
    });
    CHECK(output == "30\n");
  }

  SUBCASE("inner scope shadows outer local") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("{ var x = 1; { var x = 2; print x; } print x; }") ==
            InterpretResult::Ok);
    });
    CHECK(output == "2\n1\n");
  }

  SUBCASE("duplicate local variable in same scope is a compile error") {
    suppress_stderr([&] {
      CHECK(vm.interpret("{ var x = 1; var x = 2; }") ==
            InterpretResult::CompileError);
    });
  }

  SUBCASE("reading local variable in its own initializer is a compile error") {
    suppress_stderr([&] {
      CHECK(vm.interpret("{ var x = x; }") == InterpretResult::CompileError);
    });
  }

  SUBCASE("define and get global variable") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("var x = 42; print x;") == InterpretResult::Ok);
    });
    CHECK(output == "42\n");
  }

  SUBCASE("set global variable") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("var x = 1; x = 2; print x;") == InterpretResult::Ok);
    });
    CHECK(output == "2\n");
  }

  SUBCASE("get undefined variable returns RuntimeError") {
    capture_stderr([&] {
      CHECK(vm.interpret("print y;") == InterpretResult::RuntimeError);
    });
  }

  SUBCASE("set undefined variable returns RuntimeError") {
    capture_stderr([&] {
      CHECK(vm.interpret("y = 1;") == InterpretResult::RuntimeError);
    });
  }

  SUBCASE("globals persist across interpret calls") {
    capture_stdout(
        [&] { CHECK(vm.interpret("var z = 99;") == InterpretResult::Ok); });
    std::string output = capture_stdout(
        [&] { CHECK(vm.interpret("print z;") == InterpretResult::Ok); });
    CHECK(output == "99\n");
  }

  SUBCASE("if with true condition executes then-branch") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("if (true) print \"yes\";") == InterpretResult::Ok);
    });
    CHECK(output == "yes\n");
  }

  SUBCASE("if with false condition skips then-branch") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("if (false) print \"yes\";") == InterpretResult::Ok);
    });
    CHECK(output == "");
  }

  SUBCASE("if-else with true condition executes then-branch") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("if (true) print \"yes\"; else print \"no\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "yes\n");
  }

  SUBCASE("if-else with false condition executes else-branch") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("if (false) print \"yes\"; else print \"no\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "no\n");
  }

  SUBCASE("nil is falsey in if condition") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("if (nil) print \"yes\"; else print \"no\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "no\n");
  }

  SUBCASE("numbers are truthy in if condition") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("if (0) print \"yes\"; else print \"no\";") ==
            InterpretResult::Ok);
    });
    CHECK(output == "yes\n");
  }

  SUBCASE("comparison expression as if condition") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("if (1 < 2) print \"yes\";") == InterpretResult::Ok);
    });
    CHECK(output == "yes\n");
  }

  SUBCASE("top-level function declaration compiles and runs") {
    // mark_initialized must no-op at global scope (scope_depth == 0) rather
    // than stamp a depth onto the reserved script slot.
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("fun f() {} print f;") == InterpretResult::Ok);
    });
    CHECK(output == "<fn f>\n");
  }

  SUBCASE("function declared in a local scope resolves") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("{ fun g() {} print g; }") == InterpretResult::Ok);
    });
    CHECK(output == "<fn g>\n");
  }

  SUBCASE("calling a function and using its implicit nil return value") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("fun a() { print \"In A\"; } print a();") ==
            InterpretResult::Ok);
    });
    CHECK(output == "In A\nnil\n");
  }

  SUBCASE("recursion restores the caller's ip across many returns") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("fun fib(n) { if (n < 2) return n; "
                         "return fib(n - 1) + fib(n - 2); } print fib(20);") ==
            InterpretResult::Ok);
    });
    CHECK(output == "6765\n");
  }

  SUBCASE("closure reads an enclosing function's local") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("var x = \"global\"; "
                         "fun outer() { var x = \"outer\"; "
                         "fun inner() { print x; } inner(); } "
                         "outer();") == InterpretResult::Ok);
    });
    CHECK(output == "outer\n");
  }

  SUBCASE("closure writes an enclosing function's local") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("fun outer() { var x = \"before\"; "
                         "fun inner() { x = \"after\"; } "
                         "inner(); print x; } "
                         "outer();") == InterpretResult::Ok);
    });
    CHECK(output == "after\n");
  }

  SUBCASE("closure captures a variable through an intermediate function") {
    std::string output = capture_stdout([&] {
      CHECK(vm.interpret("fun outer() { var x = \"captured\"; "
                         "fun middle() { fun inner() { print x; } inner(); } "
                         "middle(); } "
                         "outer();") == InterpretResult::Ok);
    });
    CHECK(output == "captured\n");
  }

  SUBCASE("calling a native function with the correct arity succeeds") {
    capture_stdout(
        [&] { CHECK(vm.interpret("clock();") == InterpretResult::Ok); });
  }

  SUBCASE("calling a native function with the wrong arity is a runtime error") {
    capture_stderr([&] {
      CHECK(vm.interpret("clock(1);") == InterpretResult::RuntimeError);
    });
  }

  SUBCASE("comparison with non-number operand is a runtime error") {
    capture_stderr([&] {
      CHECK(vm.interpret("true < 1;") == InterpretResult::RuntimeError);
    });
  }

  SUBCASE("arithmetic with non-number operand is a runtime error") {
    capture_stderr([&] {
      CHECK(vm.interpret("1 * nil;") == InterpretResult::RuntimeError);
    });
  }
}
