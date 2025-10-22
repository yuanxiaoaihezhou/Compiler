# C Subset Compiler - Technical Documentation

## Overview

This is a self-hosting C subset compiler written in C that compiles a subset of the C programming language sufficient for compiling itself. The compiler generates x86_64 assembly code for Ubuntu 22.04.

## Architecture

The compiler follows the classical multi-phase architecture:

1. **Preprocessing** - Handles `#include` directives
2. **Lexical Analysis** - Tokenizes source code
3. **Syntax Analysis** - Builds Abstract Syntax Tree (AST)
4. **Type Checking** - Adds type information to AST nodes
5. **Intermediate Representation** - Generates IR
6. **Optimization** - Performs IR optimizations
7. **Code Generation** - Emits x86_64 assembly

## Supported Language Features

### Data Types
- `int` - 32-bit signed integer
- `char` - 8-bit character
- `void` - void type for functions
- Pointers to any type
- Arrays (single and multi-dimensional)
- Structs (basic support)

### Operators
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Logical: `&&`, `||`, `!`
- Bitwise: `&`, `|`, `^`, `<<`, `>>`
- Assignment: `=`, `+=`, `-=`
- Address/Dereference: `&`, `*`
- Member access: `.`, `->`
- Ternary: `? :`

### Control Flow
- `if` / `else`
- `while` loops
- `for` loops
- `return` statement
- `break` / `continue`

### Functions
- Function definitions
- Function calls
- Parameters (up to 6 passed in registers)
- Recursion

### Other Features
- Local and global variables
- Comments (// and /* */)
- String literals
- Character literals
- `sizeof` operator

## Module Descriptions

### lexer.c - Lexical Analyzer
Converts source code into a stream of tokens. Handles:
- Keywords (int, char, void, if, else, while, for, return, etc.)
- Identifiers
- Number literals
- String literals
- Character literals
- Operators (single and multi-character)
- Comments (line and block)

### parser.c - Syntax Analyzer
Implements a recursive descent parser that builds an Abstract Syntax Tree:
- Parses declarations (variables and functions)
- Parses statements (expression, if, while, for, return, block)
- Parses expressions with correct precedence
- Symbol table management for variables and functions

### ast.c - AST Operations
Provides functions for:
- Creating AST nodes
- Managing type information
- Type checking and inference

### ir.c - Intermediate Representation
Generates a simple three-address code IR:
- Register allocation
- Label generation
- Expression lowering
- Statement lowering

### optimizer.c - IR Optimizer
Performs optimization passes:
- Constant folding
- Dead code elimination
- (More optimizations can be added)

### codegen.c - Code Generator
Generates x86_64 assembly code:
- Function prologue/epilogue
- Register allocation
- Stack frame management
- Calling convention (System V AMD64 ABI)
- Binary operations
- Control flow (jumps, conditional jumps)

### preprocessor.c - Preprocessor
Handles preprocessor directives:
- `#include` directive
- Include path searching
- File inclusion

## Compilation Process

### Command Line Usage
```bash
mycc [options] file.c

Options:
  -o <file>  Write output to <file>
  -S         Generate assembly only
  -c         Compile only (do not link)
  -h         Display help
```

### Internal Workflow
1. Preprocess the source file
2. Tokenize the preprocessed source
3. Parse tokens into AST
4. Add type information to AST
5. Generate IR from AST
6. Optimize IR
7. Generate assembly from IR
8. Invoke GCC to assemble and link (unless -S flag)

## Calling Convention

The compiler uses the System V AMD64 ABI calling convention:
- First 6 integer arguments: `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`
- Additional arguments: pushed on stack (right to left)
- Return value: `rax`
- Caller-saved registers: `rax`, `rcx`, `rdx`, `rsi`, `rdi`, `r8-r11`
- Callee-saved registers: `rbx`, `rbp`, `r12-r15`
- Stack alignment: 16 bytes before call

## Memory Layout

### Stack Frame
```
Higher addresses
+------------------+
| Return address   |
+------------------+
| Saved RBP        | <-- RBP
+------------------+
| Local variable 1 |
| Local variable 2 |
| ...              |
+------------------+ <-- RSP
Lower addresses
```

### Variable Storage
- Local variables: stored on stack, accessed via RBP offset
- Global variables: stored in data section
- Function parameters: first 6 in registers, rest on stack

## Testing

### Test Suite
The `tests/` directory contains test cases:
- `test_01_return.c` - Simple return values
- `test_02_arithmetic.c` - Arithmetic operations
- `test_03_variables.c` - Variable declarations and assignments
- `test_04_if.c` - If/else statements
- `test_05_while.c` - While loops
- `test_06_for.c` - For loops
- `test_07_function.c` - Function definitions and calls

### Running Tests
```bash
make test
```

This compiles each test with both our compiler and GCC, runs both executables, and compares their outputs.

## Self-Hosting

The compiler is designed to be self-hosting, meaning it can compile itself:

```bash
make bootstrap
```

This performs a three-stage bootstrap:
1. Compile compiler with GCC (stage 1)
2. Compile compiler with stage 1 compiler (stage 2)
3. Compile compiler with stage 2 compiler (stage 3)
4. Verify stage 2 and stage 3 are identical

## Limitations

Current limitations compared to full C:
- No floating-point support
- Limited preprocessor (only #include)
- No typedef (partial support)
- No enum (partial support)
- Limited struct support (no initialization)
- No unions
- No function pointers
- No variadic functions
- No inline assembly
- Limited standard library support

## Building

```bash
make          # Build compiler
make test     # Run tests
make clean    # Clean build artifacts
make bootstrap # Test self-hosting
```

## File Organization

```
Compiler/
├── Makefile          # Build system
├── README.md         # Project overview
├── docs/             # Documentation
│   └── TECHNICAL.md  # This file
├── src/              # Source code
│   ├── compiler.h    # Main header file
│   ├── main.c        # Compiler driver
│   ├── lexer.c       # Lexical analyzer
│   ├── parser.c      # Syntax analyzer
│   ├── ast.c         # AST operations
│   ├── ir.c          # IR generation
│   ├── optimizer.c   # IR optimizer
│   ├── codegen.c     # Code generator
│   ├── preprocessor.c # Preprocessor
│   ├── utils.c       # Utility functions
│   └── error.c       # Error handling
└── tests/            # Test suite
    ├── run_tests.sh  # Test runner
    └── test_*.c      # Test cases
```

## Future Enhancements

Possible improvements:
1. Better error messages with location tracking
2. More comprehensive preprocessor
3. Support for more C features
4. Better optimization passes
5. Code generation improvements
6. More extensive test suite
7. Support for debugging symbols
8. Better type system
9. Struct initialization
10. Switch statements

## References

- C11 Standard (ISO/IEC 9899:2011)
- System V AMD64 ABI
- x86-64 Assembly Language Reference
- Compiler Design Principles
