/* Test string array initializers - needed for self-hosting */

/* Simplified test without library functions for now */
int main() {
    /* Test string array initializer */
    char *strings[] = {"hello", "world", "test"};
    
    /* Just test that we can access the pointers */
    char *s1 = strings[0];
    char *s2 = strings[1];
    char *s3 = strings[2];
    
    /* Return 0 if not null */
    if (s1 && s2 && s3) {
        return 0;
    }
    
    return 1;
}
