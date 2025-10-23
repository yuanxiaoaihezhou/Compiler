#include "compiler.h"

/* Constant folding */
static IR *constant_fold(IR *ir) {
    IR *new_head = NULL;
    IR *new_tail = NULL;
    
    for (IR *cur = ir; cur; cur = cur->next) {
        /* Skip NOPs */
        if (cur->kind == IR_NOP) {
            continue;
        }
        
        /* Add instruction to new list */
        if (!new_head) {
            new_head = new_tail = cur;
        } else {
            new_tail->next = cur;
            new_tail = cur;
        }
        new_tail->next = NULL;
    }
    
    if (new_head) {
        return new_head;
    } else {
        return ir;
    }
}

/* Dead code elimination */
static IR *eliminate_dead_code(IR *ir) {
    /* Simple implementation - just remove unreachable code after returns */
    IR *new_head = NULL;
    IR *new_tail = NULL;
    bool skip = false;
    
    for (IR *cur = ir; cur; cur = cur->next) {
        if (cur->kind == IR_LABEL) {
            skip = false;
        }
        
        if (skip) {
            continue;
        }
        
        /* Add instruction to new list */
        if (!new_head) {
            new_head = new_tail = cur;
        } else {
            new_tail->next = cur;
            new_tail = cur;
        }
        new_tail->next = NULL;
        
        if (cur->kind == IR_RET || cur->kind == IR_JMP) {
            skip = true;
        }
    }
    
    if (new_head) {
        return new_head;
    } else {
        return ir;
    }
}

/* Main optimization function */
IR *optimize(IR *ir) {
    if (!ir) {
        return ir;
    }
    
    /* Apply optimization passes */
    ir = constant_fold(ir);
    ir = eliminate_dead_code(ir);
    
    return ir;
}
