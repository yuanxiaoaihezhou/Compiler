/* Test variadic function support (va_start, va_arg, va_end) */
#include <stdarg.h>

int sum(int count, ...) {
    va_list ap;
    int total = 0;
    int i = 0;
    
    va_start(ap, count);
    
    while (i < count) {
        int val = va_arg(ap, int);
        total = total + val;
        i = i + 1;
    }
    
    va_end(ap);
    return total;
}

int main() {
    /* Test with 3 variadic args */
    if (sum(3, 10, 20, 30) != 60) return 1;
    
    /* Test with 1 variadic arg */
    if (sum(1, 42) != 42) return 2;
    
    /* Test with 5 variadic args */
    if (sum(5, 1, 2, 3, 4, 5) != 15) return 3;
    
    return 0;
}
