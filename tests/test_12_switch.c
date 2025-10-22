/* Test switch statement */

int test_switch(int x) {
    switch (x) {
        case 1:
            return 10;
        case 2:
            return 20;
        case 3:
            return 30;
        default:
            return 99;
    }
}

int test_switch_fallthrough(int x) {
    int result = 0;
    switch (x) {
        case 1:
            result = result + 1;
        case 2:
            result = result + 2;
            break;
        case 3:
            result = result + 3;
            break;
        default:
            result = 100;
    }
    return result;
}

int main() {
    /* Test basic switch */
    if (test_switch(1) != 10) return 1;
    if (test_switch(2) != 20) return 2;
    if (test_switch(3) != 30) return 3;
    if (test_switch(99) != 99) return 4;
    
    /* Test fallthrough */
    if (test_switch_fallthrough(1) != 3) return 5;
    if (test_switch_fallthrough(2) != 2) return 6;
    if (test_switch_fallthrough(3) != 3) return 7;
    if (test_switch_fallthrough(99) != 100) return 8;
    
    return 0;
}
