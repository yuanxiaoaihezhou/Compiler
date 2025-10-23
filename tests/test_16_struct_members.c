/* Test struct member access */

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    char c;
    int value;
    Point p;
} Complex;

int main() {
    /* Test basic struct member access */
    Point pt;
    pt.x = 10;
    pt.y = 20;
    int sum1 = pt.x + pt.y;
    
    /* Test pointer member access */
    Point *ptr = &pt;
    ptr->x = 30;
    ptr->y = 40;
    int sum2 = ptr->x + ptr->y;
    
    /* Test nested struct */
    Complex comp;
    comp.c = 'A';
    comp.value = 100;
    comp.p.x = 5;
    comp.p.y = 6;
    int sum3 = comp.p.x + comp.p.y;
    
    /* Test pointer to nested struct */
    Complex *cptr = &comp;
    cptr->p.x = 7;
    cptr->p.y = 8;
    int sum4 = cptr->p.x + cptr->p.y;
    
    /* Verify all operations worked correctly */
    if (sum1 != 30) return 1;
    if (sum2 != 70) return 2;
    if (sum3 != 11) return 3;
    if (sum4 != 15) return 4;
    return 0;
}
