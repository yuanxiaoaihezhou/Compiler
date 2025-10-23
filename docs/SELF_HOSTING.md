# Self-Hosting Status

## Current Status (Updated 2024-10-23)

The compiler has made MAJOR PROGRESS towards self-hosting! With the implementation of struct member access and compound assignment operators, the compiler can now successfully compile 6 out of 10 of its own source files using modular compilation.

**Test Status:** All 16 tests pass, including new struct member access tests.

## Recently Implemented Features (2024-10-23)

### ✅ Struct Member Access (MAJOR MILESTONE)
- **Status:** ✅ FULLY IMPLEMENTED
- Parser now stores member names in ND_MEMBER nodes
- add_type() resolves member names to Member* structures
- IR generation calculates member offsets
- Code generation handles both `.` and `->` operators
- **Impact:** This was the PRIMARY BLOCKER for self-hosting
- **Testing:** New test_16_struct_members.c validates all struct operations

### ✅ Compound Assignment Operators
- **Status:** ✅ IMPLEMENTED
- `+=` and `-=` operators desugar to `a = a + b` and `a = a - b`
- Used extensively in the compiler source code (60+ instances)
- **Impact:** Essential for compiling most C code

### ✅ Source Code Refactoring for Bootstrap
- Removed all ternary operators (`? :`) from source code (14 instances)
  - Rewrote as if-else statements
  - Workaround for incomplete ternary support in IR generation
- Removed bitwise AND with NOT pattern (`& ~`) from source code (2 instances)
  - Rewrote alignment operations using division/multiplication
  - Workaround for missing binary bitwise operator parsing

## Supported Features (Implemented)

✓ Basic data types: `int`, `char`, `void`
✓ Pointers and pointer arithmetic
✓ Arrays (basic)
✓ Array initializers: `int arr[3] = {1, 2, 3}`
✓ Zero initializer: `int arr[5] = {0}`
✓ Array decay to pointer
✓ **Struct member access: `.` and `->`** ✅ NEW!
✓ **Compound assignments: `+=`, `-=`** ✅ NEW!
✓ Functions with parameters and return values
✓ Local and global variables
✓ Arithmetic operators: `+`, `-`, `*`, `/`, `%`
✓ Comparison operators: `==`, `!=`, `<`, `<=`, `>`, `>=`
✓ Logical operators: `&&`, `||`, `!`
✓ Bitwise operators: `&` (unary only), `|`, `^`, `<<`, `>>`, `~`
✓ Control flow: `if`/`else`, `while`, `for`, `return`, `break`, `continue`
✓ Address-of and dereference: `&`, `*`
✓ `sizeof` operator
✓ Function calls and recursion
✓ Comments (line and block)
✓ String and character literals
✓ Basic preprocessor (`#include`, `#define`, `#ifdef`, `#ifndef`)
✓ `typedef` - type aliasing
✓ `enum` - enumeration types
✓ `static`, `extern`, `const` keywords (parsing)
✓ `switch`/`case`/`default` statements
✓ Cast expressions `(type)expr`
✓ Increment/decrement `++`, `--` (prefix and postfix)
✓ C99 for loop declarations `for (int i = 0; ...)`

## Missing Features for Full Self-Hosting

### Remaining Blockers

1. **Assembly generation issues** (Currently blocking codegen.c compilation)
   - String literal handling in assembly output
   - Format string escaping issues
   - Estimated impact: Blocking 4 remaining source files

2. **Binary bitwise operators as expressions**
   - `&` only works as unary (address-of), not as binary AND
   - Missing parser support for `|`, `^` as binary operators in precedence chain
   - Workaround: Rewrote source code to avoid these patterns
   - **Status:** Not critical for current bootstrap

3. **Ternary operator in IR** 
   - Conditional expressions `? :` parse correctly
   - Code generation works in assembly
   - IR generation not implemented
   - Workaround: Rewrote source code to use if-else
   - **Status:** Not critical for current bootstrap
  - Compiler source uses structs extensively (Symbol, ASTNode, Type, CompilerState, etc.)
  
**Other missing features:**
- [ ] String array initializers (`char *arr[] = {"str1", "str2"}`) - **Used in compiler source**
- [ ] Array of struct initializers - **Used for keywords array**
- [ ] Global variable initialization with brace initializers
- [ ] String literal storage in .data/.rodata section
- [ ] Compound literals
- [ ] Designated initializers
- [ ] `goto` and labels
- [ ] Full va_list/va_start/va_end support (compiler built-ins)

### Preprocessor Features (Partially Implemented)
- [x] `#include` - file inclusion ✓ IMPLEMENTED
- [x] `#define` - macro definitions ✓ IMPLEMENTED (basic support)
- [x] `#ifdef`, `#ifndef`, `#else`, `#endif` - conditional compilation ✓ IMPLEMENTED
- [x] Basic macro expansion ✓ IMPLEMENTED
- [x] System header stubs (stdio.h, stdlib.h, string.h, etc.) ✓ IMPLEMENTED
- [ ] Full macro expansion with arguments
- [ ] Predefined macros (__FILE__, __LINE__, etc.)

## Workaround: Combined Source File

A tool (`tools/combine.sh`) is provided to create a single-file version of the compiler that includes all necessary standard library headers. This allows:

1. Compilation with GCC to create a working compiler
2. Testing of complex C features
3. Preparation for eventual self-hosting

## Bootstrap Progress

### Bootstrap Status: 60% Complete (6/10 files compile)

The compiler is now capable of compiling itself in a modular fashion! The following source files compile successfully:

✅ **Successfully Compiling:**
1. `main.c` - Compiler driver program
2. `lexer.c` - Lexical analyzer  
3. `parser.c` - Syntax analyzer
4. `ast.c` - AST operations
5. `ir.c` - Intermediate representation
6. `optimizer.c` - Code optimizer

❌ **Remaining Issues:**
7. `codegen.c` - Assembly generation issues (string literals)
8. `preprocessor.c` - Not yet tested
9. `utils.c` - Not yet tested
10. `error.c` - Not yet tested

### Next Steps for Full Self-Hosting

1. **Fix assembly generation** (highest priority)
   - Debug string literal handling in codegen
   - Fix format string escaping
   - This will unlock codegen.c and likely the remaining files

2. **Complete modular bootstrap** 
   - Finish compiling all 10 source files
   - Link them into mycc-stage1

3. **Stage 2 verification**
   - Compile compiler with mycc-stage1 → mycc-stage2
   - Verify stage1 and stage2 produce identical results

4. **Optional enhancements**
   - Implement binary bitwise operators in parser
   - Implement ternary operator in IR generation
   - Add more comprehensive tests

## Current Bootstrap Test

The Makefile provides comprehensive bootstrap testing options.

### Basic Bootstrap (make bootstrap) - ✓ WORKS
Tests that the compiler can compile simple programs:
```bash
make bootstrap
```

This:
1. Compiles the compiler with GCC (stage 0)
2. Creates a combined source file (workaround for missing features)
3. Tests the stage 0 compiler with basic test programs
4. Verifies correct execution

**Status: WORKING** - Demonstrates compiler can handle simple C programs

### Modular Bootstrap (make bootstrap-stage1-modular) - ✗ BLOCKED
**This approach does NOT work** due to compiler limitations:
```bash
make bootstrap-stage1-modular  # Will fail
```

**Why it fails:**
- Compiler lacks struct member access (`.` and `->`) in IR/code generation
- Parser creates ND_MEMBER nodes but they're not handled in gen_ir() or gen_expr_asm()
- Results in segfault when compiling ANY code with struct member access
- Compiler's own source uses structs extensively (Symbol, ASTNode, Type, CompilerState)

**What would be needed:**
1. Implement ND_MEMBER handling in ir.c (calculate member offset, generate address)
2. Implement ND_MEMBER handling in codegen.c (generate assembly for member access)
3. Support both `.` (struct) and `->` (pointer to struct)
4. Handle nested struct access
5. Add tests for struct operations

### Stage 1 Bootstrap (make bootstrap-stage1) - ⚠️ PARTIAL
Attempts to compile the compiler with itself using combined source approach:
```bash
make bootstrap-stage1
```

This:
1. Compiles the compiler with GCC (stage 0 → mycc-stage0)
2. Creates combined source file (mycc_combined.c) - single translation unit
3. Attempts to compile mycc_combined.c with mycc-stage0 → mycc-stage1
4. Tests the stage 1 compiler if compilation succeeds

**Current Status**: Even combined approach still requires struct support, currently FAILS

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
- ✓ test_11_typedef_enum - Typedef and enum
- ✓ test_12_switch - Switch/case statements
- ✓ test_13_variadic - Variadic function parsing
- ✓ test_14_initializer - Array initializers

This demonstrates that the compiler can handle a useful subset of C, sufficient for many programs.
