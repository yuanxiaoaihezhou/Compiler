/* Test variadic functions */

/* Note: Full variadic function implementation requires va_list, va_start, va_end
 * from stdarg.h, which are compiler built-ins. For this test, we just verify
 * that the parser accepts variadic function parameters.
 */

/* Simple variadic function - just counts fixed parameters */
int count_args(int a, int b, ...) {
    /* We can't actually access variadic args without stdarg.h support */
    /* Just return the count of fixed args for now */
    return a + b;
}

int main() {
    /* Test that variadic function can be called */
    if (count_args(1, 2) != 3) return 1;
    return 0;
}

