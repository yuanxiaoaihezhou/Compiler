#include "compiler.h"

static FILE *output;
static int stack_depth;
static Symbol *current_function;
static int label_count = 0;

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

/* Escape a string for assembly .string directive */
static void emit_escaped_string(char *s) {
    fprintf(output, "  .string \"");
    for (char *p = s; *p; p++) {
        int c = *p;
        /* Convert to unsigned range 0-255 */
        if (c < 0) {
            c = c + 256;
        }
        if (c == 10) {  /* \n */
            fprintf(output, "\\n");
        } else if (c == 9) {  /* \t */
            fprintf(output, "\\t");
        } else if (c == 13) {  /* \r */
            fprintf(output, "\\r");
        } else if (c == 92) {  /* \\ */
            fprintf(output, "\\\\");
        } else if (c == 34) {  /* \" */
            fprintf(output, "\\\"");
        } else if (c >= 32 && c < 127) {
            /* Printable ASCII */
            fprintf(output, "%c", c);
        } else {
            /* Non-printable - use octal escape */
            fprintf(output, "\\%03o", c);
        }
    }
    fprintf(output, "\"\n");
}

/* Register name lookup tables (global for self-hosting compatibility) */
static char *regs64[] = {"rax", "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11"};
static char *regs32[] = {"eax", "edi", "esi", "edx", "ecx", "r8d", "r9d", "r10d", "r11d"};
static char *regs8[] = {"al", "dil", "sil", "dl", "cl", "r8b", "r9b", "r10b", "r11b"};
static char *argregs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

/* Get register name */
static char *reg_name(int r, int size) {
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
        /* Align to 8 bytes: round up to next multiple of 8 */
        offset = ((offset + 7) / 8) * 8;
        var->offset = offset;
    }
    /* Align to 16 bytes: round up to next multiple of 16 */
    fn->stack_size = ((offset + 15) / 16) * 16;
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
    
    if (node->kind == ND_MEMBER) {
        /* Generate address of struct member */
        /* First get address of the struct itself */
        gen_addr(node->lhs);
        /* Then add the member offset */
        if (node->member && node->member->offset > 0) {
            emit("  add rax, %d", node->member->offset);
        }
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
            /* Arrays decay to pointers - don't dereference */
            if (node->var->ty->kind != TY_ARRAY) {
                /* Load with correct size based on type */
                if (node->var->ty->size == 1) {
                    emit("  movsx rax, byte ptr [rax]");
                } else if (node->var->ty->size == 4) {
                    emit("  movsxd rax, dword ptr [rax]");
                } else {
                    emit("  mov rax, [rax]");
                }
            }
            return;
        case ND_ADDR:
            gen_addr(node->lhs);
            return;
        case ND_DEREF:
            gen_expr_asm(node->lhs);
            /* Load with correct size based on type */
            if (node->ty && node->ty->size == 1) {
                emit("  movsx rax, byte ptr [rax]");
            } else if (node->ty && node->ty->size == 4) {
                emit("  movsxd rax, dword ptr [rax]");
            } else {
                emit("  mov rax, [rax]");
            }
            return;
        case ND_MEMBER:
            /* Generate address and load value */
            gen_addr(node);
            /* Load with correct size based on member type */
            if (node->member && node->member->ty) {
                if (node->member->ty->size == 1) {
                    emit("  movsx rax, byte ptr [rax]");
                } else if (node->member->ty->size == 4) {
                    emit("  movsxd rax, dword ptr [rax]");
                } else {
                    emit("  mov rax, [rax]");
                }
            } else {
                emit("  mov rax, [rax]");
            }
            return;
        case ND_LNOT:
            gen_expr_asm(node->lhs);
            emit("  cmp rax, 0");
            emit("  sete al");
            emit("  movzb rax, al");
            return;
        case ND_NOT:
            gen_expr_asm(node->lhs);
            emit("  not rax");
            return;
        case ND_CAST:
            /* Generate code for the expression being cast */
            gen_expr_asm(node->lhs);
            /* For simple casts (int/char/pointer), just keep the value in rax */
            /* Sign/zero extension handled implicitly by register usage */
            if (node->ty && node->ty->size == 1) {
                /* Cast to char - ensure only low byte is used */
                emit("  movsx rax, al");
            } else if (node->ty && node->ty->size == 4) {
                /* Cast to int - sign extend from 32-bit */
                emit("  movsxd rax, eax");
            }
            /* For pointer casts, rax already contains the value */
            return;
        case ND_ASSIGN:
            gen_addr(node->lhs);
            push("rax");
            gen_expr_asm(node->rhs);
            pop("rdi");
            /* Store with correct size based on type */
            if (node->lhs->ty->size == 1) {
                emit("  mov [rdi], al");
            } else if (node->lhs->ty->size == 4) {
                emit("  mov [rdi], eax");
            } else {
                emit("  mov [rdi], rax");
            }
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
            for (i = 0; i < nargs && i < 6; i++) {
                gen_expr_asm(args[i]);
                push("rax");
            }
            
            int end_i;
            if (nargs < 6) {
                end_i = nargs - 1;
            } else {
                end_i = 5;
            }
            for (i = end_i; i >= 0; i--) {
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
            /* Handle pointer arithmetic - scale the right operand if left is a pointer */
            if (node->lhs->ty && (node->lhs->ty->kind == TY_PTR || node->lhs->ty->kind == TY_ARRAY)) {
                int size = 1;
                if (node->lhs->ty->base) {
                    size = node->lhs->ty->base->size;
                }
                if (size > 1) {
                    emit("  imul rdi, %d", size);
                }
            }
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
        case ND_COND: {
            /* Conditional expression: cond ? then : els */
            int c = label_count++;
            gen_expr_asm(node->cond);
            emit("  cmp rax, 0");
            emit("  je .L.else.%d", c);
            gen_expr_asm(node->then);
            emit("  jmp .L.end.%d", c);
            emit(".L.else.%d:", c);
            gen_expr_asm(node->els);
            emit(".L.end.%d:", c);
            return;
        }
    }
    
    error("invalid expression");
}

/* Generate assembly for statement */
static void gen_stmt_asm(ASTNode *node) {
    switch (node->kind) {
        case ND_RETURN:
            if (node->lhs) {
                gen_expr_asm(node->lhs);
            }
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
            emit("%s:", node->cont_label);
            gen_expr_asm(node->cond);
            emit("  cmp rax, 0");
            emit("  je %s", node->brk_label);
            gen_stmt_asm(node->then);
            emit("  jmp %s", node->cont_label);
            emit("%s:", node->brk_label);
            return;
        }
        case ND_FOR: {
            if (node->init) {
                gen_stmt_asm(node->init);
            }
            emit("%s:", node->cont_label);
            if (node->cond) {
                gen_expr_asm(node->cond);
                emit("  cmp rax, 0");
                emit("  je %s", node->brk_label);
            }
            gen_stmt_asm(node->then);
            if (node->inc) {
                gen_expr_asm(node->inc);
            }
            emit("  jmp %s", node->cont_label);
            emit("%s:", node->brk_label);
            return;
        }
        case ND_BLOCK:
            for (ASTNode *n = node->body; n; n = n->next) {
                gen_stmt_asm(n);
            }
            return;
        case ND_SWITCH: {
            /* Evaluate switch expression */
            gen_expr_asm(node->cond);
            
            /* Switch body must be a compound statement */
            if (!node->then || node->then->kind != ND_BLOCK) {
                error("switch statement body must be a compound statement");
            }
            
            /* Track case values and their labels */
            #define MAX_CASES 256
            static struct { int val; int label; } case_map[MAX_CASES];
            int num_cases = 0;
            int default_label = -1;
            static int case_label_base = 0;
            int my_label_base = case_label_base;
            
            /* First pass: assign labels to all cases */
            for (ASTNode *stmt = node->then->body; stmt; stmt = stmt->next) {
                if (stmt->kind == ND_CASE) {
                    if (stmt->val >= 0) {
                        if (num_cases < MAX_CASES) {
                            case_map[num_cases].val = stmt->val;
                            case_map[num_cases].label = case_label_base++;
                            num_cases++;
                        }
                    } else {
                        default_label = case_label_base++;
                    }
                }
            }
            
            /* Generate jump table */
            for (int i = 0; i < num_cases; i++) {
                emit("  cmp rax, %d", case_map[i].val);
                emit("  je .L.case.%d", case_map[i].label);
            }
            
            /* Jump to default or break */
            if (default_label >= 0) {
                emit("  jmp .L.case.%d", default_label);
            } else {
                emit("  jmp %s", node->brk_label);
            }
            
            /* Second pass: generate case bodies with labels */
            for (ASTNode *stmt = node->then->body; stmt; stmt = stmt->next) {
                if (stmt->kind == ND_CASE) {
                    /* Emit label for this case */
                    if (stmt->val >= 0) {
                        /* Find label for this case value */
                        for (int i = 0; i < num_cases; i++) {
                            if (case_map[i].val == stmt->val) {
                                emit(".L.case.%d:", case_map[i].label);
                                break;
                            }
                        }
                    } else {
                        emit(".L.case.%d:", default_label);
                    }
                    if (stmt->lhs) {
                        gen_stmt_asm(stmt->lhs);
                    }
                } else {
                    gen_stmt_asm(stmt);
                }
            }
            
            emit("%s:", node->brk_label);
            return;
        }
        case ND_CASE:
            /* Case labels are handled by ND_SWITCH */
            gen_stmt_asm(node->lhs);
            return;
        case ND_BREAK:
            if (node->brk_label) {
                emit("  jmp %s", node->brk_label);
            }
            return;
        case ND_CONTINUE:
            if (node->cont_label) {
                emit("  jmp %s", node->cont_label);
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
        if (fn->is_function && fn->body) {
            /* Only generate code for functions with bodies (not declarations) */
            gen_function_asm(fn);
        }
    }
    
    /* Generate data section */
    emit(".data");
    for (Symbol *var = prog; var; var = var->next) {
        if (!var->is_function && !var->is_local && !var->is_extern) {
            /* Only emit .globl for non-string-literal globals */
            if (var->name[0] != '.' || var->name[1] != 'L' || var->name[2] != 'C') {
                emit(".globl %s", var->name);
            }
            emit("%s:", var->name);
            
            if (var->ty->kind == TY_ARRAY && var->ty->base->kind == TY_CHAR) {
                /* String literal - use str_data if available */
                if (var->str_data) {
                    emit_escaped_string(var->str_data);
                } else {
                    emit_escaped_string(var->name);
                }
            } else if (var->init) {
                /* Global variable with initializer */
                if (var->ty->kind == TY_ARRAY) {
                    /* Array initializer */
                    Type *elem_ty = var->ty->base;
                    for (Initializer *init = var->init->children; init; init = init->next) {
                        if (elem_ty->kind == TY_PTR || elem_ty->kind == TY_ARRAY) {
                            /* Pointer or array element - emit as quad (8 bytes) */
                            if (init->is_expr && init->expr->kind == ND_VAR) {
                                /* Pointer to string literal or global */
                                emit("  .quad %s", init->expr->var->name);
                            } else {
                                emit("  .quad 0");
                            }
                        } else if (elem_ty->kind == TY_INT) {
                            /* Integer element - emit as long (4 bytes) */
                            if (init->is_expr && init->expr->kind == ND_NUM) {
                                emit("  .long %d", init->expr->val);
                            } else {
                                emit("  .long 0");
                            }
                        } else if (elem_ty->kind == TY_CHAR) {
                            /* Char element - emit as byte */
                            if (init->is_expr && init->expr->kind == ND_NUM) {
                                emit("  .byte %d", init->expr->val);
                            } else {
                                emit("  .byte 0");
                            }
                        } else {
                            /* Unknown type - use zero */
                            emit("  .zero %d", elem_ty->size);
                        }
                    }
                } else if (var->ty->kind == TY_INT) {
                    /* Simple int initializer */
                    if (var->init->is_expr && var->init->expr->kind == ND_NUM) {
                        emit("  .long %d", var->init->expr->val);
                    } else {
                        emit("  .long 0");
                    }
                } else if (var->ty->kind == TY_CHAR) {
                    /* Simple char initializer */
                    if (var->init->is_expr && var->init->expr->kind == ND_NUM) {
                        emit("  .byte %d", var->init->expr->val);
                    } else {
                        emit("  .byte 0");
                    }
                } else if (var->ty->kind == TY_PTR) {
                    /* Pointer initializer */
                    if (var->init->is_expr && var->init->expr->kind == ND_VAR) {
                        emit("  .quad %s", var->init->expr->var->name);
                    } else {
                        emit("  .quad 0");
                    }
                } else {
                    /* Other types - zero fill */
                    emit("  .zero %d", var->ty->size);
                }
            } else {
                /* No initializer - zero fill */
                emit("  .zero %d", var->ty->size);
            }
        }
    }
}
