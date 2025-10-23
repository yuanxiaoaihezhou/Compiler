# Compiler Improvements - October 2024

## Overview

This document summarizes the major improvements made to the C compiler in October 2024, with a focus on enhancing self-hosting capabilities.

## Newly Implemented Features

### 1. Cast Expressions
**Status:** ✅ Fully Implemented

Cast expressions allow explicit type conversion using the syntax `(type)expr`.

**Implementation:**
- Parser: Added `is_typename()` helper to detect type names
- Parser: Modified `unary()` to handle cast syntax
- IR Generation: Pass through to underlying expression
- Code Generation: Handle size conversions (char, int, pointer)

**Example:**
```c
void *p = (void*)&x;
int *q = (int*)p;
int value = (int)3.14;  // Note: floats not supported yet
```

**Impact:** Critical for self-hosting as it enables NULL macro expansion `((void*)0)`

### 2. Increment/Decrement Operators (++ and --)
**Status:** ✅ Fully Implemented

Both prefix and postfix forms of ++ and -- operators.

**Implementation:**
- Lexer: Already tokenized as TK_INC and TK_DEC
- Parser: Added to `unary()` for prefix forms (++x, --x)
- Parser: Added to `postfix()` for postfix forms (x++, x--)
- Desugaring: Implemented as compound operations
  - `x++` → `(x = x + 1) - 1`
  - `++x` → `x = x + 1`
  - `x--` → `(x = x - 1) + 1`
  - `--x` → `x = x - 1`

**Example:**
```c
int arr[10];
for (int i = 0; i < 10; i++) {
    arr[i] = i * 2;
}
```

**Impact:** Used extensively in compiler source (114+ instances)

### 3. C99-Style For Loop Declarations
**Status:** ✅ Fully Implemented

Allows variable declarations in for loop initialization.

**Implementation:**
- Parser: Modified `stmt()` for loop to detect type keywords
- Parser: Reused declaration parsing logic from compound statements
- Scope: Variables are local to the for loop

**Example:**
```c
for (int i = 0; i < 10; i++) {
    printf("%d\n", i);
}
// i is out of scope here
```

**Impact:** Used 12 times in compiler source, cleaner code

### 4. sizeof(type) Syntax
**Status:** ✅ Fully Implemented

Allows sizeof with type names in addition to expressions.

**Implementation:**
- Parser: Modified sizeof handling in `unary()`
- Detects `sizeof(typename)` vs `sizeof expr`
- Returns compile-time constant

**Example:**
```c
size_t s1 = sizeof(int);           // 4
size_t s2 = sizeof(ASTNode);       // struct size
size_t s3 = sizeof(x);             // expression
```

**Impact:** Used extensively for memory allocation in compiler

### 5. Void Return Statements
**Status:** ✅ Fully Implemented

Support for `return;` without a value in void functions.

**Implementation:**
- Parser: Made return expression optional
- IR Generation: Handle NULL lhs for return
- Code Generation: Skip value generation if no return value

**Example:**
```c
void error(char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
    return;  // optional but valid
}
```

**Impact:** Common pattern in utility functions

## Preprocessor Enhancements

### 1. System Header Stubs
**Status:** ✅ Enhanced

The preprocessor now provides comprehensive stubs for standard headers:

**stdio.h:**
- Type definitions: FILE
- I/O functions: printf, fprintf, sprintf, fopen, fclose, fread, fwrite, fseek, ftell
- Constants: NULL, SEEK_SET, SEEK_CUR, SEEK_END

**stdlib.h:**
- Memory functions: malloc, calloc, realloc, free
- Utilities: exit, atoi, system

**string.h:**
- String functions: strlen, strcmp, strncmp, strcpy, strncpy, strcat, strchr, strstr, strdup
- Memory functions: memcpy, memset, memcmp

**stddef.h:**
- Type definitions: size_t, ptrdiff_t
- Constants: NULL

**errno.h:**
- Global: errno

**stdbool.h:**
- Type: bool
- Constants: true, false

### 2. Macro System
**Status:** ✅ Partially Implemented

Basic macro definition and expansion:
- `#define NAME value` - simple text replacement
- `#ifdef NAME` / `#ifndef NAME` - conditional compilation
- `#else` / `#endif` - conditional branches
- Basic macro expansion in source text

**Limitations:**
- No function-like macros with arguments
- No predefined macros (__FILE__, __LINE__)

## Error Reporting

**Status:** ✅ Already Excellent

The error reporting system already provides:
- File:line:column location
- Colored output (red for errors, cyan for notes)
- Source code context with line numbers
- Visual indicators (^~~) pointing to error location
- Helpful error messages
- Support for notes/warnings

**Example Error:**
```
src/test.c:15:10: error: undefined variable
   15 |     x = foo;
      |          ^~~
```

## Build System

### Makefile Targets

**Existing targets maintained:**
- `make` - Build compiler with GCC
- `make test` - Run test suite (15/15 tests passing)
- `make clean` - Clean build artifacts

**Bootstrap targets:**
- `make bootstrap` - Basic bootstrap test
- `make bootstrap-stage1` - Compile compiler with itself (combined file approach)
- `make bootstrap-stage1-modular` - Modular compilation (separate .o files)
- `make bootstrap-stage2` - Second stage compilation
- `make bootstrap-full` - Full 3-stage bootstrap with verification
- `make bootstrap-test` - Test suite with bootstrapped compiler

### Build Strategy

Two approaches are available:

1. **Combined Source (Current Default)**
   - `tools/combine.sh` merges all .c files into one
   - Simpler to compile (single translation unit)
   - Used by `make bootstrap-stage1`

2. **Modular Compilation (Available)**
   - Separate compilation of each .c file
   - Linking of .o files
   - More like standard C projects
   - Available via `make bootstrap-stage1-modular`

## Test Suite

All 15 tests passing:
1. test_01_return - Basic return values
2. test_02_arithmetic - Arithmetic operations
3. test_03_variables - Variable declarations
4. test_04_if - Conditional statements
5. test_05_while - While loops
6. test_06_for - For loops
7. test_07_function - Function calls
8. test_08_pointer - Pointer operations
9. test_09_comparison - Comparison operators
10. test_10_recursion - Recursive functions
11. test_11_typedef_enum - Typedef and enum
12. test_12_switch - Switch statements
13. test_13_variadic - Variadic function parsing
14. test_14_initializer - Array initializers
15. test_15_string_array - String array handling

## Current Bootstrap Status

**Stage 0 (GCC compilation):** ✅ Working
- Compiler builds successfully with GCC
- All tests pass

**Stage 1 (Self-compilation):** ❌ BLOCKED
- **Critical blocker: Struct member access (. and ->) not implemented**
- Parser creates ND_MEMBER nodes but IR and codegen don't handle them
- Causes segfault when compiling ANY code with struct member access
- Compiler source uses structs extensively (Symbol, ASTNode, Type, CompilerState, etc.)
- Both modular and combined file approaches fail

**Remaining blockers for full bootstrap:**
1. **CRITICAL: Struct member access** - Must be implemented in ir.c and codegen.c
2. String array initializers (e.g., `char *arr[] = {"str1", "str2"}`)
3. Struct array initializers (e.g., keyword tables)
4. Global variable initialization with braces
5. Some advanced preprocessor features

## Code Statistics

**Source files:** 11 (.c and .h)
**Total lines:** ~4,200 (including comments)
**Test files:** 15

**Module breakdown:**
- main.c (150 lines) - Driver program
- lexer.c (339 lines) - Lexical analysis
- parser.c (1,450+ lines) - Syntax analysis (significantly expanded)
- ast.c (165 lines) - AST operations
- ir.c (350 lines) - IR generation
- codegen.c (570 lines) - x86_64 code generation
- preprocessor.c (525 lines) - Preprocessor
- optimizer.c (71 lines) - Optimizations
- utils.c (74 lines) - Utilities
- error.c (163 lines) - Error handling
- compiler.h (270 lines) - Header definitions

## Lessons Learned

1. **Incremental Implementation:** Implementing features incrementally and testing immediately caught many issues early

2. **Desugaring:** Complex operators (++/--) can be desugared into simpler primitives, avoiding the need for new AST nodes

3. **Type Checking:** The `is_typename()` helper is crucial for disambiguating casts from other parenthesized expressions

4. **Bootstrapping is Iterative:** Each new feature enables compiling more of the compiler source, revealing the next missing feature

5. **Preprocessor is Critical:** A good preprocessor with system header stubs makes self-hosting much more practical

## Future Work

### Short Term (Needed for Full Bootstrap)
1. Implement complex initializers
2. Fix remaining parser edge cases
3. Improve struct initialization

### Medium Term (Nice to Have)
1. Function-like macros
2. Predefined macros (__FILE__, __LINE__)
3. Better optimization passes
4. Goto statements
5. Compound literals

### Long Term (Quality of Life)
1. Better error recovery (continue parsing after errors)
2. Warning system
3. Debug symbol generation
4. Multiple target architectures
5. Floating point support

## Conclusion

Significant progress has been made toward understanding the path to self-hosting:
- 6 major C language features implemented (cast, ++/--, C99 for, sizeof(type), void return)
- Preprocessor greatly enhanced with system header stubs
- Documentation comprehensively updated
- All 15 tests passing
- **Critical discovery**: Struct member access is the main blocker for self-hosting
  - Parser supports it (creates ND_MEMBER nodes)
  - IR and codegen do NOT support it (causes segfault)
  - This must be implemented before any bootstrap attempt can succeed
  
The compiler is now well-documented and ready for the next phase: implementing struct member access support in the IR generator and code generator.
