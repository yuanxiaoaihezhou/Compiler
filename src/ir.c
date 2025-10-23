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
        case ND_LAND:
        case ND_LOR:
        case ND_LNOT:
        case ND_NOT:
            /* For now, treat logical operators as expressions that will be handled in codegen */
            /* This is a simplified approach - just evaluate both sides */
            if (node->lhs) gen_expr(node->lhs);
            if (node->rhs) gen_expr(node->rhs);
            return new_reg();
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
        case ND_CAST: {
            /* For casts, just evaluate the expression */
            /* Type conversion is handled at code generation */
            return gen_expr(node->lhs);
        }
        case ND_MEMBER: {
            /* Generate address of struct member */
            /* struct_addr = address of lhs (the struct) */
            int struct_reg = gen_expr(node->lhs);
            
            /* For member access, we need to:
             * 1. Get the address of the struct
             * 2. Add the member offset to get member address
             * 3. Load the value from that address
             */
            
            /* If lhs is not already an address, we need special handling */
            /* For now, assume lhs gives us a value and we need to get its address */
            /* This is tricky - for x.member, x might be a variable (need address) */
            /* Let's handle the common case where lhs is ND_VAR or ND_DEREF */
            
            int member_addr;
            if (node->lhs->kind == ND_VAR) {
                /* Get address of the variable */
                IR *addr_ir = new_ir(IR_ADDR);
                addr_ir->dst = new_reg();
                addr_ir->name = node->lhs->var->name;
                add_ir(addr_ir);
                member_addr = addr_ir->dst;
            } else if (node->lhs->kind == ND_DEREF) {
                /* For ptr->member (which becomes (*ptr).member), 
                 * lhs is ND_DEREF, so we want the address that would be dereferenced */
                member_addr = gen_expr(node->lhs->lhs);
            } else {
                /* For other cases, use the evaluated result as an address */
                member_addr = struct_reg;
            }
            
            /* Add member offset */
            if (node->member && node->member->offset > 0) {
                IR *offset_ir = new_ir(IR_MOV);
                offset_ir->dst = new_reg();
                offset_ir->imm = node->member->offset;
                add_ir(offset_ir);
                
                IR *add_ir_inst = new_ir(IR_ADD);
                add_ir_inst->lhs = member_addr;
                add_ir_inst->rhs = offset_ir->dst;
                add_ir_inst->dst = new_reg();
                add_ir(add_ir_inst);
                member_addr = add_ir_inst->dst;
            }
            
            /* Load value from member address */
            IR *load = new_ir(IR_LOAD);
            load->dst = new_reg();
            load->lhs = member_addr;
            add_ir(load);
            return load->dst;
        }
        case ND_VA_START:
        case ND_VA_ARG:
        case ND_VA_END:
            /* Variadic function built-ins are handled directly in codegen */
            /* Just return a placeholder register for now */
            return new_reg();
        case ND_COND:
            /* Conditional expression (a ? b : c) */
            /* For now, just evaluate all parts and return last */
            if (node->cond) gen_expr(node->cond);
            if (node->then) gen_expr(node->then);
            if (node->els) gen_expr(node->els);
            return new_reg();
        case ND_SIZEOF:
            /* sizeof is evaluated at compile time, just return a constant */
            return new_reg();
        default:
            error("unsupported expression in IR generation");
            return 0;
    }
}

/* Generate IR for statement */
static void gen_stmt(ASTNode *node) {
    switch (node->kind) {
        case ND_RETURN: {
            IR *ir = new_ir(IR_RET);
            if (node->lhs) {
                ir->lhs = gen_expr(node->lhs);
            } else {
                ir->lhs = 0;  /* No return value */
            }
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
        case ND_SWITCH:
            /* Switch statements are handled directly in codegen, not IR */
            /* For now, just generate the body */
            gen_stmt(node->then);
            return;
        case ND_CASE:
            /* Case labels are handled by switch */
            if (node->lhs) {
                gen_stmt(node->lhs);
            }
            return;
        case ND_BREAK:
        case ND_CONTINUE:
            /* Break/continue are handled directly in codegen */
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
        if (fn->is_function && fn->body) {
            /* Only generate IR for functions with bodies (not declarations) */
            gen_function(fn);
        }
    }
    
    return code;
}
