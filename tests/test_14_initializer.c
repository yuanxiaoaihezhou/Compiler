/* Test basic array initializers */

int main() {
    /* Test local array initializer */
    int arr[3] = {10, 20, 30};
    
    /* Test array access */
    int a = arr[0];
    int b = arr[1];
    int c = arr[2];
    
    return a + b + c;  /* Should return 60 */
}
