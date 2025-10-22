# C Subset Compiler - Usage Examples

## Basic Usage

### Compiling a Simple Program

```c
/* hello.c */
int main() {
    return 42;
}
```

```bash
./build/mycc hello.c -o hello
./hello
echo $?  # Output: 42
```

### Generating Assembly Output

```bash
./build/mycc hello.c -S -o hello.s
cat hello.s
```

### Compile Without Linking

```bash
./build/mycc hello.c -c -o hello.o
gcc hello.o -o hello
```

## Example Programs

### 1. Arithmetic Calculator

```c
/* calculator.c */
int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

int main() {
    int x;
    int y;
    int result;
    
    x = 5;
    y = 3;
    
    result = add(x, y);
    result = multiply(result, 2);
    
    return result;  /* Returns 16 */
}
```

```bash
./build/mycc calculator.c -o calculator
./calculator
echo $?  # Output: 16
```

### 2. Factorial (Recursion)

```c
/* factorial.c */
int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    return factorial(5);  /* Returns 120 */
}
```

```bash
./build/mycc factorial.c -o factorial
./factorial
echo $?  # Output: 120
```

### 3. Fibonacci Sequence

```c
/* fibonacci.c */
int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
    return fibonacci(10);  /* Returns 55 */
}
```

### 4. Pointer Arithmetic

```c
/* pointers.c */
int main() {
    int x;
    int *p;
    int **pp;
    
    x = 100;
    p = &x;
    pp = &p;
    
    return **pp;  /* Returns 100 */
}
```

### 5. Array Processing

```c
/* arrays.c */
int sum_array(int *arr, int size) {
    int sum;
    int i;
    
    sum = 0;
    for (i = 0; i < size; i = i + 1) {
        sum = sum + arr[i];
    }
    
    return sum;
}

int main() {
    int numbers[5];
    
    numbers[0] = 1;
    numbers[1] = 2;
    numbers[2] = 3;
    numbers[3] = 4;
    numbers[4] = 5;
    
    return sum_array(numbers, 5);  /* Returns 15 */
}
```

### 6. Conditional Logic

```c
/* max.c */
int max(int a, int b) {
    if (a > b) {
        return a;
    }
    return b;
}

int max3(int a, int b, int c) {
    int temp;
    temp = max(a, b);
    return max(temp, c);
}

int main() {
    return max3(5, 12, 8);  /* Returns 12 */
}
```

### 7. Loop Iterations

```c
/* loops.c */
int sum_to_n(int n) {
    int sum;
    int i;
    
    sum = 0;
    i = 1;
    
    while (i <= n) {
        sum = sum + i;
        i = i + 1;
    }
    
    return sum;
}

int main() {
    return sum_to_n(100);  /* Returns 5050 */
}
```

### 8. Bitwise Operations

```c
/* bitwise.c */
int is_power_of_two(int n) {
    if (n <= 0) {
        return 0;
    }
    return (n & (n - 1)) == 0;
}

int main() {
    return is_power_of_two(16);  /* Returns 1 (true) */
}
```

## Testing Your Program

### Comparing with GCC

```bash
# Compile with both compilers
./build/mycc myprogram.c -o myprogram_mycc
gcc myprogram.c -o myprogram_gcc

# Run both and compare
./myprogram_mycc
echo "mycc exit code: $?"

./myprogram_gcc
echo "gcc exit code: $?"
```

### Automated Testing

Create a test script:

```bash
#!/bin/bash

TEST_FILE="myprogram.c"

# Compile with mycc
./build/mycc $TEST_FILE -o test_mycc || exit 1

# Compile with gcc
gcc $TEST_FILE -o test_gcc || exit 1

# Run both
./test_mycc
MYCC_EXIT=$?

./test_gcc
GCC_EXIT=$?

# Compare
if [ $MYCC_EXIT -eq $GCC_EXIT ]; then
    echo "✓ Test passed (both returned $MYCC_EXIT)"
    exit 0
else
    echo "✗ Test failed (mycc: $MYCC_EXIT, gcc: $GCC_EXIT)"
    exit 1
fi
```

## Debugging

### View Generated Assembly

```bash
./build/mycc program.c -S -o program.s
less program.s
```

### Check Parser Output

For debugging the compiler itself, add print statements in the appropriate phase:
- Lexer: Check token stream
- Parser: Check AST structure  
- IR: Check intermediate representation
- Codegen: Check assembly output

## Limitations

The compiler currently does NOT support:

- Floating-point numbers
- Strings (beyond literals)
- Complex struct operations
- typedef, enum (partial support)
- Standard library functions
- Most preprocessor directives (except #include)
- Multi-dimensional array initialization
- Function pointers
- Variable argument functions

## Performance Tips

1. The compiler is optimized for correctness, not speed
2. Generated code is not heavily optimized
3. For production use, compile the output with gcc -O2
4. Large files may take significant time to compile

## Getting Help

- Check technical documentation: `docs/TECHNICAL.md`
- Review test cases in `tests/` directory
- See self-hosting status: `docs/SELF_HOSTING.md`
