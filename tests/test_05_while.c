/* Test: While loop */
int main() {
    int i;
    int sum;
    i = 0;
    sum = 0;
    while (i < 10) {
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}
