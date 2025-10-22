#include "compiler.h"

static IR *code;
static int nreg = 1;
static int nlabel = 1;

/* Create new IR instruction */
static IR *new_ir(IRKind kind) {
    IR *ir = calloc(1, sizeof(IR));
    ir->kind = kind;
    return ir;
}

/* Allocate new register */
static int new_reg(void) {
    return nreg++;
}

/* Allocate new label */
static int new_label(void) {
    return nlabel++;
}

/* Add IR instruction */
static void add_ir(IR *ir) {
    if (!code) {
        code = ir;
    } else {
        IR *last = code;
        while (last->next) {
            last = last->next;
        }
        last->next = ir;
    }
}

/* Generate IR for expression */
static int gen_expr(ASTNode *node);

/* Generate IR for binary operation */
static int gen_binop(IRKind kind, ASTNode *node) {
    int lhs = gen_expr(node->lhs);
    int rhs = gen_expr(node->rhs);
    
    IR *ir = new_ir(kind);
    ir->lhs = lhs;
    ir->rhs = rhs;
    ir->dst = new_reg();
    add_ir(ir);
    
    return ir->dst;
}

/* Generate IR for expression */
static int gen_expr(ASTNode *node) {
    switch (node->kind) {
        case ND_NUM: {
            IR *ir = new_ir(IR_MOV);
            ir->dst = new_reg();
            ir->imm = node->val;
            add_ir(ir);
            return ir->dst;
        }
        case ND_VAR: {
            IR *ir = new_ir(IR_MOV);
            ir->dst = new_reg();
            ir->name = node->var->name;
            add_ir(ir);
            
            if (node->var->is_local) {
                /* Load local variable */
                IR *load = new_ir(IR_LOAD);
                load->dst = new_reg();
                load->lhs = ir->dst;
                add_ir(load);
                return load->dst;
            }
            return ir->dst;
        }
        case ND_ADD:
            return gen_binop(IR_ADD, node);
        case ND_SUB:
            return gen_binop(IR_SUB, node);
        case ND_MUL:
            return gen_binop(IR_MUL, node);
        case ND_DIV:
            return gen_binop(IR_DIV, node);
        case ND_MOD:
            return gen_binop(IR_MOD, node);
        case ND_EQ:
            return gen_binop(IR_EQ, node);
        case ND_NE:
            return gen_binop(IR_NE, node);
        case ND_LT:
            return gen_binop(IR_LT, node);
        case ND_LE:
            return gen_binop(IR_LE, node);
        case ND_GT:
            return gen_binop(IR_GT, node);
        case ND_GE:
            return gen_binop(IR_GE, node);
        case ND_AND:
            return gen_binop(IR_AND, node);
        case ND_OR:
            return gen_binop(IR_OR, node);
        case ND_XOR:
            return gen_binop(IR_XOR, node);
        case ND_SHL:
            return gen_binop(IR_SHL, node);
        case ND_SHR:
            return gen_binop(IR_SHR, node);
        case ND_ASSIGN: {
            int rhs = gen_expr(node->rhs);
            int lhs = gen_expr(node->lhs);
            
            IR *ir = new_ir(IR_STORE);
            ir->lhs = lhs;
            ir->rhs = rhs;
            add_ir(ir);
            
            return rhs;
        }
        case ND_ADDR: {
            IR *ir = new_ir(IR_ADDR);
            ir->dst = new_reg();
            if (node->lhs->kind == ND_VAR) {
                ir->name = node->lhs->var->name;
            }
            add_ir(ir);
            return ir->dst;
        }
        case ND_DEREF: {
            int addr = gen_expr(node->lhs);
            IR *ir = new_ir(IR_LOAD);
            ir->dst = new_reg();
            ir->lhs = addr;
            add_ir(ir);
            return ir->dst;
        }
        case ND_CALL: {
            /* Generate code for arguments */
            int nargs = 0;
            for (ASTNode *arg = node->args; arg; arg = arg->next) {
                gen_expr(arg);
                nargs++;
            }
            
            IR *ir = new_ir(IR_CALL);
            ir->name = node->funcname;
            ir->dst = new_reg();
            ir->imm = nargs;
            add_ir(ir);
            return ir->dst;
        }
        case ND_COMMA: {
            gen_expr(node->lhs);
            return gen_expr(node->rhs);
        }
        default:
            error("unsupported expression in IR generation");
            return 0;
    }
}

/* Generate IR for statement */
static void gen_stmt(ASTNode *node) {
    switch (node->kind) {
        case ND_RETURN: {
            int r = gen_expr(node->lhs);
            IR *ir = new_ir(IR_RET);
            ir->lhs = r;
            add_ir(ir);
            return;
        }
        case ND_EXPR_STMT: {
            gen_expr(node->lhs);
            return;
        }
        case ND_IF: {
            int r = gen_expr(node->cond);
            int lelse = new_label();
            int lend = new_label();
            
            IR *ir = new_ir(IR_JZ);
            ir->lhs = r;
            ir->imm = lelse;
            add_ir(ir);
            
            gen_stmt(node->then);
            
            if (node->els) {
                IR *jmp = new_ir(IR_JMP);
                jmp->imm = lend;
                add_ir(jmp);
                
                IR *label = new_ir(IR_LABEL);
                label->imm = lelse;
                add_ir(label);
                
                gen_stmt(node->els);
                
                label = new_ir(IR_LABEL);
                label->imm = lend;
                add_ir(label);
            } else {
                IR *label = new_ir(IR_LABEL);
                label->imm = lelse;
                add_ir(label);
            }
            return;
        }
        case ND_WHILE: {
            int lbegin = new_label();
            int lend = new_label();
            
            IR *label = new_ir(IR_LABEL);
            label->imm = lbegin;
            add_ir(label);
            
            int r = gen_expr(node->cond);
            
            IR *ir = new_ir(IR_JZ);
            ir->lhs = r;
            ir->imm = lend;
            add_ir(ir);
            
            gen_stmt(node->then);
            
            IR *jmp = new_ir(IR_JMP);
            jmp->imm = lbegin;
            add_ir(jmp);
            
            label = new_ir(IR_LABEL);
            label->imm = lend;
            add_ir(label);
            return;
        }
        case ND_FOR: {
            int lbegin = new_label();
            int lend = new_label();
            
            if (node->init) {
                gen_stmt(node->init);
            }
            
            IR *label = new_ir(IR_LABEL);
            label->imm = lbegin;
            add_ir(label);
            
            if (node->cond) {
                int r = gen_expr(node->cond);
                IR *ir = new_ir(IR_JZ);
                ir->lhs = r;
                ir->imm = lend;
                add_ir(ir);
            }
            
            gen_stmt(node->then);
            
            if (node->inc) {
                gen_expr(node->inc);
            }
            
            IR *jmp = new_ir(IR_JMP);
            jmp->imm = lbegin;
            add_ir(jmp);
            
            label = new_ir(IR_LABEL);
            label->imm = lend;
            add_ir(label);
            return;
        }
        case ND_BLOCK: {
            for (ASTNode *n = node->body; n; n = n->next) {
                gen_stmt(n);
            }
            return;
        }
        case ND_NULL_STMT:
            return;
        default:
            error("unsupported statement in IR generation");
    }
}

/* Generate IR for function */
static void gen_function(Symbol *fn) {
    nreg = 1;
    
    IR *ir = new_ir(IR_LABEL);
    ir->name = fn->name;
    add_ir(ir);
    
    /* Generate function body */
    gen_stmt(fn->body);
    
    /* Add implicit return */
    ir = new_ir(IR_RET);
    ir->lhs = 0;
    add_ir(ir);
}

/* Generate IR for program */
IR *gen_ir(Symbol *prog) {
    code = NULL;
    nreg = 1;
    nlabel = 1;
    
    for (Symbol *fn = prog; fn; fn = fn->next) {
        if (fn->is_function) {
            gen_function(fn);
        }
    }
    
    return code;
}
