# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
cmake -B build                              # configure (first time or after CMakeLists.txt changes)
cmake --build build                         # compile
./bin/clox                                  # run REPL
./bin/clox script.lox                       # run a file
cmake -B build -DDEBUG_TRACE_EXECUTION=ON   # build with VM instruction tracing
```

## Testing

```bash
./bin/test                     # run all tests
./bin/test -tc="test name"     # run a single test case by name
```

Tests live inside the `.cpp` file they test, using doctest. `test_utils.h` provides `capture_stdout()` for asserting printed output.

## Project Details

- **Standard:** C++20
- **Entry point:** `main.cpp` (REPL and file interpreter modes)
- **Build output:** `bin/clox` (interpreter), `bin/test` (test runner)
- `DOCTEST_CONFIG_DISABLE` is defined for the `clox` target so test code is compiled out

## Architecture

Pipeline: source string → **Scanner** → tokens → **Parser/Compiler** → bytecode in **Chunk** → **VM**

- **Scanner** (`src/scanner.h/cpp`): tokenizes source into `Token`s with type, lexeme pointer, and line
- **Parser** (`src/parser.h/cpp`): owns the Scanner; drives token advancement, error reporting, and panic-mode recovery
- **Compiler** (`src/compiler.h/cpp`): Pratt parser that consumes tokens via Parser and emits bytecode into a `Chunk`. `ParseRule` table maps each `TokenType` to prefix/infix parse functions and a precedence level
- **Chunk** (`src/chunk.h/cpp`): bytecode buffer (`vector<uint8_t>`) plus a constant pool (`vector<Value>`) and an instruction→line map for error reporting
- **VM** (`src/vm.h/cpp`): fixed-size value stack (`Value stack[256]`), an instruction pointer into the current `Chunk`, and a `run()` dispatch loop. `interpret()` compiles then executes
- **Value** (`src/value.h/cpp`): currently `typedef double Value`; `print_value()` uses `std::format`
- **Debug** (`src/debug.h/cpp`): `disassemble_chunk()` / `disassemble_instruction()` / `print_stack()`; active when `DEBUG_TRACE_EXECUTION` is defined

## Code Style
- Always use curly braces for blocks, even if they only have one line.
