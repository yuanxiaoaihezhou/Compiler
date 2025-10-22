# Self-Hosting Status

## Current Status

The compiler can successfully compile and run a variety of C programs, as demonstrated by the comprehensive test suite (10/10 tests passing). However, full self-hosting (compiling the compiler's own source code) requires additional features.

## Supported Features (Implemented)

✓ Basic data types: `int`, `char`, `void`
✓ Pointers and pointer arithmetic
✓ Arrays (basic)
✓ Functions with parameters and return values
✓ Local and global variables
✓ Arithmetic operators: `+`, `-`, `*`, `/`, `%`
✓ Comparison operators: `==`, `!=`, `<`, `<=`, `>`, `>=`
✓ Logical operators: `&&`, `||`, `!`
✓ Bitwise operators: `&`, `|`, `^`, `<<`, `>>`
✓ Control flow: `if`/`else`, `while`, `for`, `return`, `break`, `continue`
✓ Address-of and dereference: `&`, `*`
✓ `sizeof` operator
✓ Function calls and recursion
✓ Comments (line and block)
✓ String and character literals
✓ Basic preprocessor (`#include`)

## Missing Features for Full Self-Hosting

The following features are used in the compiler source but not yet implemented:

### Critical Features (Completed)
- [x] `typedef` - type aliasing ✓ IMPLEMENTED
- [x] `enum` - enumeration types ✓ IMPLEMENTED
- [x] `static` keyword ✓ IMPLEMENTED (parsing only)
- [x] `extern` keyword ✓ IMPLEMENTED (parsing only)
- [x] `const` keyword ✓ IMPLEMENTED (parsing only)
- [x] `switch`/`case`/`default` statements ✓ IMPLEMENTED (basic support, tested)
- [x] Variadic functions (`...`) ✓ IMPLEMENTED (parsing support, tested)

### Critical Features (Still Needed for Full Self-Hosting)
- [ ] Brace initializers (`{0}`, `{.field = value}`) - **Used in compiler source**
- [ ] Array initializers - **Used for string arrays**
- [ ] Struct member initialization
- [ ] Global variable initialization with non-constant values
- [ ] String concatenation
- [ ] Cast expressions
- [ ] Compound literals
- [ ] Designated initializers
- [ ] `goto` and labels
- [ ] Full va_list/va_start/va_end support (compiler built-ins)

### Standard Library Functions
- [ ] `calloc`, `malloc`, `free` - memory allocation
- [ ] `strcmp`, `strncmp`, `strlen`, `strcpy`, `strcat` - string functions
- [ ] `printf`, `fprintf`, `sprintf`, `vfprintf` - formatted output
- [ ] `fopen`, `fclose`, `fread`, `fwrite`, `fseek`, `ftell` - file I/O
- [ ] `exit` - program termination

### Preprocessor
- [ ] `#define` - macro definitions
- [ ] `#ifdef`, `#ifndef`, `#endif` - conditional compilation
- [ ] Macro expansion
- [ ] Predefined macros

## Workaround: Combined Source File

A tool (`tools/combine.sh`) is provided to create a single-file version of the compiler that includes all necessary standard library headers. This allows:

1. Compilation with GCC to create a working compiler
2. Testing of complex C features
3. Preparation for eventual self-hosting

## Path to Full Self-Hosting

To achieve full self-hosting, implement features in this order:

1. **Phase 1: Essential Language Features**
   - `typedef` support
   - `enum` support
   - Full struct support with initialization
   - Global variable initialization
   - Cast expressions

2. **Phase 2: Preprocessor**
   - `#define` macro support
   - Conditional compilation
   - Macro expansion

3. **Phase 3: Advanced Features**
   - `static` and `extern`
   - Compound literals
   - Designated initializers

4. **Phase 4: Standard Library**
   - Implement/link against required standard library functions
   - Or rewrite compiler to use only basic I/O

## Current Bootstrap Test

The Makefile now provides comprehensive bootstrap testing options:

### Basic Bootstrap (make bootstrap)
Tests that the compiler can compile simple programs:
```bash
make bootstrap
```

This:
1. Compiles the compiler with GCC (stage 0)
2. Creates a combined source file
3. Tests the stage 0 compiler with basic test programs
4. Verifies correct execution

### Stage 1 Bootstrap (make bootstrap-stage1)
Attempts to compile the compiler with itself:
```bash
make bootstrap-stage1
```

This:
1. Compiles the compiler with GCC (stage 0 → mycc-stage0)
2. Creates combined source file (mycc_combined.c)
3. Attempts to compile mycc_combined.c with mycc-stage0 → mycc-stage1
4. Tests the stage 1 compiler if compilation succeeds

**Current Status**: Fails due to missing language features (expected)

### Stage 2 Bootstrap (make bootstrap-stage2)
Compiles the compiler with the stage 1 compiler:
```bash
make bootstrap-stage2
```

This:
1. Runs bootstrap-stage1
2. Compiles mycc_combined.c with mycc-stage1 → mycc-stage2
3. Tests the stage 2 compiler if compilation succeeds

**Current Status**: Requires stage 1 to succeed first

### Full Bootstrap (make bootstrap-full)
Complete 3-stage bootstrap with verification:
```bash
make bootstrap-full
```

This:
1. Runs bootstrap-stage2
2. Compares mycc-stage1 and mycc-stage2 binaries
3. If binaries differ, tests functional equivalence on all test programs
4. Verifies that both compilers produce identical outputs

**Current Status**: Requires stage 1 to succeed first

### Bootstrap Test (make bootstrap-test)
Runs all test programs with the bootstrapped compiler:
```bash
make bootstrap-test
```

This:
1. Runs bootstrap-stage1
2. Compiles all test programs with mycc-stage1
3. Compares outputs with GCC-compiled versions
4. Reports pass/fail for each test

**Current Status**: Requires stage 1 to succeed first

## Test Results

All basic functionality tests pass:
- ✓ test_01_return - Simple return values
- ✓ test_02_arithmetic - Arithmetic operations
- ✓ test_03_variables - Variable declarations
- ✓ test_04_if - If/else statements
- ✓ test_05_while - While loops
- ✓ test_06_for - For loops
- ✓ test_07_function - Function calls
- ✓ test_08_pointer - Pointer operations
- ✓ test_09_comparison - Comparisons
- ✓ test_10_recursion - Recursive functions

This demonstrates that the compiler can handle a useful subset of C, sufficient for many programs.
