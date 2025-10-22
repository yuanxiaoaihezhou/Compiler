/* Test cast expressions */

int main() {
    /* Test basic casts */
    int x = (int)42;
    char c = (char)65;
    
    /* Test pointer casts */
    int *p = (int*)0;
    void *vp = (void*)p;
    
    /* Test cast to void pointer (NULL) */
    int *null_ptr = (void*)0;
    
    /* Test expression in cast */
    int y = (int)(x + 1);
    
    return x + c;  /* Should be 42 + 65 = 107 */
}
