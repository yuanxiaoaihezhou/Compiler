/* Runtime support functions for self-hosted compiler */

/* Variadic function support stubs */
/* These are no-ops because our compiler uses the simplified calling convention */
/* In x86_64 System V ABI, variadic args are passed normally */
void va_start(int ap, int last) {
    /* No-op - compiler handles argument passing */
}

void va_end(int ap) {
    /* No-op - nothing to clean up */
}
