/* Runtime support functions for self-hosted compiler */

/* Variadic function support stubs */
/* These work with our simplified calling convention */
/* The ap parameter represents the address of the next argument on the stack */
/* We don't actually use these - the compiler directly accesses arguments */
/* But we need the symbols to exist for linking */

#ifndef __GNUC__
/* When compiled by our compiler, provide actual stub functions */
void va_start(void *ap, void *last) {
    /* No-op - our compiler handles this differently */
    (void)ap;
    (void)last;
}

void va_end(void *ap) {
    /* No-op */
    (void)ap;
}

void *va_arg(void *ap, int size) {
    /* No-op */
    (void)ap;
    (void)size;
    return 0;
}
#endif
