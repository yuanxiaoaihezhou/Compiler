#include "compiler.h"

static FILE *output;
static int stack_depth;
static Symbol *current_function;

/* Forward declaration */
static void gen_expr_asm(ASTNode *node);

/* Emit assembly code */
static void emit(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(output, fmt, ap);
    fprintf(output, "\n");
    va_end(ap);
}

/* Get register name */
static char *reg_name(int r, int size) {
    static char *regs64[] = {"rax", "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11"};
    static char *regs32[] = {"eax", "edi", "esi", "edx", "ecx", "r8d", "r9d", "r10d", "r11d"};
    static char *regs8[] = {"al", "dil", "sil", "dl", "cl", "r8b", "r9b", "r10b", "r11b"};
    
    if (r < 0 || r >= 9) {
        return "rax";
    }
    
    if (size == 8) {
        return regs64[r];
    } else if (size == 4) {
        return regs32[r];
    } else {
        return regs8[r];
    }
}

/* Push register to stack */
static void push(char *reg) {
    emit("  push %s", reg);
    stack_depth += 8;
}

/* Pop register from stack */
static void pop(char *reg) {
    emit("  pop %s", reg);
    stack_depth -= 8;
}

/* Assign local variable offsets */
static void assign_lvar_offsets(Symbol *fn) {
    int offset = 0;
    for (Symbol *var = fn->locals; var; var = var->next) {
        offset += var->ty->size;
        offset = (offset + 7) & ~7; /* Align to 8 bytes */
        var->offset = offset;
    }
    fn->stack_size = (offset + 15) & ~15; /* Align to 16 bytes */
}

/* Load variable address */
static void gen_addr(ASTNode *node) {
    if (node->kind == ND_VAR) {
        if (node->var->is_local) {
            emit("  lea rax, [rbp-%d]", node->var->offset);
        } else {
            emit("  lea rax, %s[rip]", node->var->name);
        }
        return;
    }
    
    if (node->kind == ND_DEREF) {
        gen_expr_asm(node->lhs);
        return;
    }
    
    error("not an lvalue");
}

/* Generate assembly for expression */
static void gen_expr_asm(ASTNode *node) {
    switch (node->kind) {
        case ND_NUM:
            emit("  mov rax, %d", node->val);
            return;
        case ND_VAR:
            gen_addr(node);
            emit("  mov rax, [rax]");
            return;
        case ND_ADDR:
            gen_addr(node->lhs);
            return;
        case ND_DEREF:
            gen_expr_asm(node->lhs);
            emit("  mov rax, [rax]");
            return;
        case ND_ASSIGN:
            gen_addr(node->lhs);
            push("rax");
            gen_expr_asm(node->rhs);
            pop("rdi");
            emit("  mov [rdi], rax");
            return;
        case ND_CALL: {
            /* Save caller-saved registers */
            int nargs = 0;
            for (ASTNode *arg = node->args; arg; arg = arg->next) {
                nargs++;
            }
            
            /* Evaluate arguments in reverse order */
            ASTNode **args = calloc(nargs, sizeof(ASTNode*));
            int i = 0;
            for (ASTNode *arg = node->args; arg; arg = arg->next) {
                args[i++] = arg;
            }
            
            /* Generate code for arguments */
            char *argregs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            for (i = 0; i < nargs && i < 6; i++) {
                gen_expr_asm(args[i]);
                push("rax");
            }
            
            for (i = nargs < 6 ? nargs - 1 : 5; i >= 0; i--) {
                pop(argregs[i]);
            }
            
            /* Align stack to 16 bytes */
            int seq = stack_depth / 8;
            if (seq % 2 == 1) {
                emit("  sub rsp, 8");
                stack_depth += 8;
            }
            
            emit("  call %s", node->funcname);
            
            /* Restore stack alignment */
            if (seq % 2 == 1) {
                emit("  add rsp, 8");
                stack_depth -= 8;
            }
            
            free(args);
            return;
        }
        case ND_COMMA:
            gen_expr_asm(node->lhs);
            gen_expr_asm(node->rhs);
            return;
    }
    
    /* Binary operations */
    gen_expr_asm(node->rhs);
    push("rax");
    gen_expr_asm(node->lhs);
    pop("rdi");
    
    switch (node->kind) {
        case ND_ADD:
            emit("  add rax, rdi");
            return;
        case ND_SUB:
            emit("  sub rax, rdi");
            return;
        case ND_MUL:
            emit("  imul rax, rdi");
            return;
        case ND_DIV:
            emit("  cqo");
            emit("  idiv rdi");
            return;
        case ND_MOD:
            emit("  cqo");
            emit("  idiv rdi");
            emit("  mov rax, rdx");
            return;
        case ND_EQ:
            emit("  cmp rax, rdi");
            emit("  sete al");
            emit("  movzb rax, al");
            return;
        case ND_NE:
            emit("  cmp rax, rdi");
            emit("  setne al");
            emit("  movzb rax, al");
            return;
        case ND_LT:
            emit("  cmp rax, rdi");
            emit("  setl al");
            emit("  movzb rax, al");
            return;
        case ND_LE:
            emit("  cmp rax, rdi");
            emit("  setle al");
            emit("  movzb rax, al");
            return;
        case ND_GT:
            emit("  cmp rax, rdi");
            emit("  setg al");
            emit("  movzb rax, al");
            return;
        case ND_GE:
            emit("  cmp rax, rdi");
            emit("  setge al");
            emit("  movzb rax, al");
            return;
        case ND_LAND:
            emit("  test rax, rax");
            emit("  setne al");
            emit("  test rdi, rdi");
            emit("  setne dil");
            emit("  and al, dil");
            emit("  movzb rax, al");
            return;
        case ND_LOR:
            emit("  or rax, rdi");
            emit("  setne al");
            emit("  movzb rax, al");
            return;
        case ND_AND:
            emit("  and rax, rdi");
            return;
        case ND_OR:
            emit("  or rax, rdi");
            return;
        case ND_XOR:
            emit("  xor rax, rdi");
            return;
        case ND_SHL:
            emit("  mov rcx, rdi");
            emit("  shl rax, cl");
            return;
        case ND_SHR:
            emit("  mov rcx, rdi");
            emit("  shr rax, cl");
            return;
    }
    
    error("invalid expression");
}

static int label_count = 0;

/* Generate assembly for statement */
static void gen_stmt_asm(ASTNode *node) {
    switch (node->kind) {
        case ND_RETURN:
            gen_expr_asm(node->lhs);
            emit("  jmp .L.return.%s", current_function->name);
            return;
        case ND_EXPR_STMT:
            gen_expr_asm(node->lhs);
            return;
        case ND_NULL_STMT:
            return;
        case ND_IF: {
            int c = label_count++;
            gen_expr_asm(node->cond);
            emit("  cmp rax, 0");
            emit("  je .L.else.%d", c);
            gen_stmt_asm(node->then);
            emit("  jmp .L.end.%d", c);
            emit(".L.else.%d:", c);
            if (node->els) {
                gen_stmt_asm(node->els);
            }
            emit(".L.end.%d:", c);
            return;
        }
        case ND_WHILE: {
            int c = label_count++;
            emit(".L.begin.%d:", c);
            gen_expr_asm(node->cond);
            emit("  cmp rax, 0");
            emit("  je .L.end.%d", c);
            gen_stmt_asm(node->then);
            emit("  jmp .L.begin.%d", c);
            emit(".L.end.%d:", c);
            return;
        }
        case ND_FOR: {
            int c = label_count++;
            if (node->init) {
                gen_stmt_asm(node->init);
            }
            emit(".L.begin.%d:", c);
            if (node->cond) {
                gen_expr_asm(node->cond);
                emit("  cmp rax, 0");
                emit("  je .L.end.%d", c);
            }
            gen_stmt_asm(node->then);
            if (node->inc) {
                gen_expr_asm(node->inc);
            }
            emit("  jmp .L.begin.%d", c);
            emit(".L.end.%d:", c);
            return;
        }
        case ND_BLOCK:
            for (ASTNode *n = node->body; n; n = n->next) {
                gen_stmt_asm(n);
            }
            return;
    }
    
    error("invalid statement");
}

/* Generate assembly for function */
static void gen_function_asm(Symbol *fn) {
    current_function = fn;
    assign_lvar_offsets(fn);
    
    emit(".globl %s", fn->name);
    emit("%s:", fn->name);
    
    /* Prologue */
    push("rbp");
    emit("  mov rbp, rsp");
    emit("  sub rsp, %d", fn->stack_size);
    
    /* Save parameters to stack */
    char *argregs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    int i = 0;
    for (Symbol *param = fn->params; param && i < 6; param = param->next, i++) {
        /* Find this parameter in locals to get its offset */
        Symbol *local = NULL;
        for (Symbol *l = fn->locals; l; l = l->next) {
            if (strcmp(l->name, param->name) == 0) {
                local = l;
                break;
            }
        }
        if (local) {
            emit("  mov [rbp-%d], %s", local->offset, argregs[i]);
        }
    }
    
    stack_depth = 0;
    
    /* Generate function body */
    gen_stmt_asm(fn->body);
    
    /* Epilogue */
    emit(".L.return.%s:", fn->name);
    emit("  mov rsp, rbp");
    pop("rbp");
    emit("  ret");
}

/* Generate assembly code */
void codegen(Symbol *prog, FILE *out) {
    output = out;
    
    emit(".intel_syntax noprefix");
    emit(".text");
    
    /* Generate code for functions */
    for (Symbol *fn = prog; fn; fn = fn->next) {
        if (fn->is_function) {
            gen_function_asm(fn);
        }
    }
    
    /* Generate data section */
    emit(".data");
    for (Symbol *var = prog; var; var = var->next) {
        if (!var->is_function && !var->is_local) {
            emit(".globl %s", var->name);
            emit("%s:", var->name);
            if (var->ty->kind == TY_ARRAY && var->ty->base->kind == TY_CHAR) {
                /* String literal */
                emit("  .string \"%s\"", var->name);
            } else {
                emit("  .zero %d", var->ty->size);
            }
        }
    }
}
