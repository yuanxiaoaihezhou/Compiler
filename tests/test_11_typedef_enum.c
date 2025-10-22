/* Test typedef and enum features */

/* Test typedef */
typedef int myint;
typedef char* string;

/* Test enum */
enum Color {
    RED,
    GREEN = 5,
    BLUE
};

/* Test typedef with enum */
typedef enum {
    FALSE,
    TRUE
} bool_t;

/* Test function with typedef parameters */
myint add(myint a, myint b) {
    return a + b;
}

/* Test extern (parsing only) */
extern myint global_extern;

int main() {
    /* Test typedef variables */
    myint x = 42;
    myint y = 8;
    
    /* Test enum constants */
    enum Color c1 = RED;
    enum Color c2 = GREEN;
    enum Color c3 = BLUE;
    
    /* Test typedef enum */
    bool_t flag = TRUE;
    
    /* Test const (parsing only) */
    const myint z = 100;
    
    /* Test function with typedef */
    myint sum = add(x, y);
    
    /* Calculate result */
    myint result = sum + c1 + c2 + c3 + flag;
    
    /* Should be: 50 + 0 + 5 + 6 + 1 = 62 */
    return result;
}
