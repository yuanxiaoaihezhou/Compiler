#include "compiler.h"

static Symbol *locals;
static Symbol *globals;
static Symbol *current_fn;
static Symbol *typedefs;  /* Typedef symbols */
static Symbol *enums;     /* Enum constants */

/* Labels for break/continue */
static char *current_brk_label;
static char *current_cont_label;

/* Counter for string literal labels */
static int string_label_count = 0;

/* Type definitions */
static Type *ty_void;
static Type *ty_char;
static Type *ty_int;

static void initialize_types(void) {
    ty_void = new_type(TY_VOID, 0, 1);
    ty_char = new_type(TY_CHAR, 1, 1);
    ty_int = new_type(TY_INT, 4, 4);
}

/* Forward declarations */
static ASTNode *expr(Token **rest, Token *tok);
static ASTNode *assign(Token **rest, Token *tok);
static ASTNode *equality(Token **rest, Token *tok);
static ASTNode *relational(Token **rest, Token *tok);
static ASTNode *add(Token **rest, Token *tok);
static ASTNode *mul(Token **rest, Token *tok);
static ASTNode *unary(Token **rest, Token *tok);
static ASTNode *postfix(Token **rest, Token *tok);
static ASTNode *primary(Token **rest, Token *tok);
static ASTNode *stmt(Token **rest, Token *tok);
static ASTNode *compound_stmt(Token **rest, Token *tok);
static Symbol *find_var(Token *tok);
static int eval_const_expr(ASTNode *node);
static void gen_init_code(ASTNode **cur_stmt, ASTNode *var_node, Initializer *init, Type *ty);

/* Declaration specifiers */
typedef struct {
    Type *ty;
    bool is_typedef;
    bool is_static;
    bool is_extern;
} DeclSpec;

static DeclSpec *declspec(Token **rest, Token *tok);
static Type *declarator(Token **rest, Token *tok, Type *ty);

/* Find variable by name */
static Symbol *find_var(Token *tok) {
    for (Symbol *var = locals; var; var = var->next) {
        if (strlen(var->name) == tok->len && 
            strncmp(var->name, tok->str, tok->len) == 0) {
            return var;
        }
    }
    for (Symbol *var = globals; var; var = var->next) {
        if (strlen(var->name) == tok->len && 
            strncmp(var->name, tok->str, tok->len) == 0) {
            return var;
        }
    }
    return NULL;
}

/* Find typedef by name */
static Symbol *find_typedef(Token *tok) {
    for (Symbol *td = typedefs; td; td = td->next) {
        if (strlen(td->name) == tok->len &&
            strncmp(td->name, tok->str, tok->len) == 0) {
            return td;
        }
    }
    return NULL;
}

/* Find enum constant by name */
static Symbol *find_enum(Token *tok) {
    for (Symbol *e = enums; e; e = e->next) {
        if (strlen(e->name) == tok->len &&
            strncmp(e->name, tok->str, tok->len) == 0) {
            return e;
        }
    }
    return NULL;
}

/* Evaluate constant expression (for case labels) */
static int eval_const_expr(ASTNode *node) {
    if (!node) {
        error("Expected constant expression");
    }
    
    switch (node->kind) {
        case ND_NUM:
            return node->val;
        case ND_ADD:
            return eval_const_expr(node->lhs) + eval_const_expr(node->rhs);
        case ND_SUB:
            return eval_const_expr(node->lhs) - eval_const_expr(node->rhs);
        case ND_MUL:
            return eval_const_expr(node->lhs) * eval_const_expr(node->rhs);
        case ND_DIV:
            return eval_const_expr(node->lhs) / eval_const_expr(node->rhs);
        case ND_VAR:
            /* Allow enum constants */
            if (node->var) {
                /* Check if it's an enum constant - search in enums list */
                for (Symbol *e = enums; e; e = e->next) {
                    if (strcmp(e->name, node->var->name) == 0) {
                        return e->enum_val;
                    }
                }
            }
            /* Fall through */
        default:
            error("Not a constant expression");
    }
    return 0;
}

/* Create new local variable */
static Symbol *new_lvar(char *name, Type *ty) {
    Symbol *var = calloc(1, sizeof(Symbol));
    var->name = name;
    var->ty = ty;
    var->is_local = true;
    var->next = locals;
    locals = var;
    return var;
}

/* Create new global variable */
static Symbol *new_gvar(char *name, Type *ty) {
    Symbol *var = calloc(1, sizeof(Symbol));
    var->name = name;
    var->ty = ty;
    var->is_local = false;
    var->next = globals;
    globals = var;
    return var;
}

/* Parse primary expression */
static ASTNode *primary(Token **rest, Token *tok) {
    /* ( expr ) */
    if (equal(tok, "(")) {
        ASTNode *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }
    
    /* Number */
    if (tok->kind == TK_NUM) {
        ASTNode *node = new_num(tok->val);
        *rest = tok->next;
        return node;
    }
    
    /* Character literal */
    if (tok->kind == TK_CHAR_LIT) {
        ASTNode *node = new_num(tok->val);
        *rest = tok->next;
        return node;
    }
    
    /* String literal */
    if (tok->kind == TK_STR) {
        /* Create unique label for string literal */
        char label[32];
        snprintf(label, sizeof(label), ".LC%d", string_label_count++);
        
        /* Create global variable for string with unique label */
        Symbol *var = new_gvar(strdup_custom(label), array_of(ty_char, strlen(tok->str) + 1));
        var->str_data = strdup_custom(tok->str);  /* Store actual string content */
        
        ASTNode *node = new_node(ND_VAR);
        node->var = var;
        *rest = tok->next;
        return node;
    }
    
    /* Variable */
    if (tok->kind == TK_IDENT) {
        /* Function call */
        if (equal(tok->next, "(")) {
            ASTNode *node = new_node(ND_CALL);
            node->funcname = strndup_custom(tok->str, tok->len);
            tok = tok->next->next;
            
            /* Parse arguments */
            ASTNode head = {0};
            ASTNode *cur = &head;
            
            while (!equal(tok, ")")) {
                if (cur != &head) {
                    tok = skip(tok, ",");
                }
                cur = cur->next = assign(&tok, tok);
            }
            node->args = head.next;
            *rest = skip(tok, ")");
            return node;
        }
        
        /* Variable reference */
        Symbol *var = find_var(tok);
        if (!var) {
            /* Check if it's an enum constant */
            Symbol *enum_const = find_enum(tok);
            if (enum_const) {
                *rest = tok->next;
                return new_num(enum_const->enum_val);
            }
            error_tok(tok, "undefined variable");
        }
        *rest = tok->next;
        ASTNode *node = new_node(ND_VAR);
        node->var = var;
        return node;
    }
    
    error_tok(tok, "expected an expression");
    return NULL;
}

/* Parse postfix expression */
static ASTNode *postfix(Token **rest, Token *tok) {
    ASTNode *node = primary(&tok, tok);
    
    while (true) {
        /* Array subscript */
        if (equal(tok, "[")) {
            ASTNode *idx = expr(&tok, tok->next);
            tok = skip(tok, "]");
            node = new_binary(ND_DEREF, new_binary(ND_ADD, node, idx), NULL);
            continue;
        }
        
        /* Member access */
        if (equal(tok, ".")) {
            node = new_node(ND_MEMBER);
            node->lhs = node;
            tok = tok->next;
            if (tok->kind != TK_IDENT) {
                error_tok(tok, "expected member name");
            }
            /* Member resolution will be done in type checking */
            tok = tok->next;
            continue;
        }
        
        /* Pointer member access */
        if (equal(tok, "->")) {
            ASTNode *deref = new_binary(ND_DEREF, node, NULL);
            node = new_node(ND_MEMBER);
            node->lhs = deref;
            tok = tok->next;
            if (tok->kind != TK_IDENT) {
                error_tok(tok, "expected member name");
            }
            tok = tok->next;
            continue;
        }
        
        /* Postfix increment: x++ 
         * Since we can't easily create temporaries, we approximate as:
         * In simple cases like arr[i++], this becomes: arr[i], then i = i + 1
         * For expressions that use the value, we'd need proper temp support.
         * For now, we'll just do: (x = x + 1) - 1 which works for most cases.
         */
        if (tok->kind == TK_INC) {
            add_type(node);
            ASTNode *one = new_num(1);
            /* Create: (x = x + 1) - 1 */
            /* This evaluates the new value then subtracts 1 to get old value */
            ASTNode *new_val = new_binary(ND_ADD, copy_node(node), one);
            ASTNode *assign = new_binary(ND_ASSIGN, node, new_val);
            node = new_binary(ND_SUB, assign, new_num(1));
            tok = tok->next;
            continue;
        }
        
        /* Postfix decrement: x-- becomes (x = x - 1) + 1 */
        if (tok->kind == TK_DEC) {
            add_type(node);
            ASTNode *one = new_num(1);
            ASTNode *new_val = new_binary(ND_SUB, copy_node(node), one);
            ASTNode *assign = new_binary(ND_ASSIGN, node, new_val);
            node = new_binary(ND_ADD, assign, new_num(1));
            tok = tok->next;
            continue;
        }
        
        *rest = tok;
        return node;
    }
}

/* Check if token is a type keyword */
static bool is_typename(Token *tok) {
    return tok->kind == TK_INT || tok->kind == TK_CHAR || tok->kind == TK_VOID ||
           tok->kind == TK_STRUCT || tok->kind == TK_ENUM ||
           (tok->kind == TK_IDENT && find_typedef(tok));
}

/* Parse unary expression */
static ASTNode *unary(Token **rest, Token *tok) {
    if (equal(tok, "+")) {
        return unary(rest, tok->next);
    }
    if (equal(tok, "-")) {
        return new_binary(ND_SUB, new_num(0), unary(rest, tok->next));
    }
    if (equal(tok, "&")) {
        ASTNode *node = new_node(ND_ADDR);
        node->lhs = unary(rest, tok->next);
        return node;
    }
    if (equal(tok, "*")) {
        ASTNode *node = new_node(ND_DEREF);
        node->lhs = unary(rest, tok->next);
        return node;
    }
    if (equal(tok, "!")) {
        ASTNode *node = new_node(ND_LNOT);
        node->lhs = unary(rest, tok->next);
        return node;
    }
    if (equal(tok, "~")) {
        ASTNode *node = new_node(ND_NOT);
        node->lhs = unary(rest, tok->next);
        return node;
    }
    
    /* Prefix increment: ++x becomes x = x + 1 */
    if (tok->kind == TK_INC) {
        ASTNode *operand = unary(&tok, tok->next);
        add_type(operand);
        ASTNode *one = new_num(1);
        ASTNode *new_val = new_binary(ND_ADD, operand, one);
        *rest = tok;
        return new_binary(ND_ASSIGN, copy_node(operand), new_val);
    }
    
    /* Prefix decrement: --x becomes x = x - 1 */
    if (tok->kind == TK_DEC) {
        ASTNode *operand = unary(&tok, tok->next);
        add_type(operand);
        ASTNode *one = new_num(1);
        ASTNode *new_val = new_binary(ND_SUB, operand, one);
        *rest = tok;
        return new_binary(ND_ASSIGN, copy_node(operand), new_val);
    }
    
    if (tok->kind == TK_SIZEOF) {
        tok = tok->next;
        
        /* Check if it's sizeof(type) */
        if (equal(tok, "(") && is_typename(tok->next)) {
            tok = tok->next;
            
            /* Parse type */
            DeclSpec *spec = declspec(&tok, tok);
            Type *ty = declarator(&tok, tok, spec->ty);
            *rest = skip(tok, ")");
            
            return new_num(ty->size);
        }
        
        /* Otherwise it's sizeof expr */
        ASTNode *node = unary(rest, tok);
        add_type(node);
        return new_num(node->ty->size);
    }
    
    /* Cast expression: (type)expr */
    if (equal(tok, "(") && is_typename(tok->next)) {
        Token *start = tok;
        tok = tok->next;
        
        /* Parse type */
        DeclSpec *spec = declspec(&tok, tok);
        Type *ty = declarator(&tok, tok, spec->ty);
        tok = skip(tok, ")");
        
        /* Parse the expression being cast */
        ASTNode *node = new_node(ND_CAST);
        node->ty = ty;
        node->lhs = unary(rest, tok);
        return node;
    }
    
    return postfix(rest, tok);
}

/* Parse multiplicative expression */
static ASTNode *mul(Token **rest, Token *tok) {
    ASTNode *node = unary(&tok, tok);
    
    while (true) {
        if (equal(tok, "*")) {
            node = new_binary(ND_MUL, node, unary(&tok, tok->next));
            continue;
        }
        if (equal(tok, "/")) {
            node = new_binary(ND_DIV, node, unary(&tok, tok->next));
            continue;
        }
        if (equal(tok, "%")) {
            node = new_binary(ND_MOD, node, unary(&tok, tok->next));
            continue;
        }
        *rest = tok;
        return node;
    }
}

/* Parse additive expression */
static ASTNode *add(Token **rest, Token *tok) {
    ASTNode *node = mul(&tok, tok);
    
    while (true) {
        if (equal(tok, "+")) {
            node = new_binary(ND_ADD, node, mul(&tok, tok->next));
            continue;
        }
        if (equal(tok, "-")) {
            node = new_binary(ND_SUB, node, mul(&tok, tok->next));
            continue;
        }
        *rest = tok;
        return node;
    }
}

/* Parse relational expression */
static ASTNode *relational(Token **rest, Token *tok) {
    ASTNode *node = add(&tok, tok);
    
    while (true) {
        if (equal(tok, "<")) {
            node = new_binary(ND_LT, node, add(&tok, tok->next));
            continue;
        }
        if (equal(tok, "<=")) {
            node = new_binary(ND_LE, node, add(&tok, tok->next));
            continue;
        }
        if (equal(tok, ">")) {
            node = new_binary(ND_GT, node, add(&tok, tok->next));
            continue;
        }
        if (equal(tok, ">=")) {
            node = new_binary(ND_GE, node, add(&tok, tok->next));
            continue;
        }
        *rest = tok;
        return node;
    }
}

/* Parse equality expression */
static ASTNode *equality(Token **rest, Token *tok) {
    ASTNode *node = relational(&tok, tok);
    
    while (true) {
        if (equal(tok, "==")) {
            node = new_binary(ND_EQ, node, relational(&tok, tok->next));
            continue;
        }
        if (equal(tok, "!=")) {
            node = new_binary(ND_NE, node, relational(&tok, tok->next));
            continue;
        }
        *rest = tok;
        return node;
    }
}

/* Parse logical AND expression */
static ASTNode *log_and(Token **rest, Token *tok) {
    ASTNode *node = equality(&tok, tok);
    
    while (equal(tok, "&&")) {
        node = new_binary(ND_LAND, node, equality(&tok, tok->next));
    }
    
    *rest = tok;
    return node;
}

/* Parse logical OR expression */
static ASTNode *log_or(Token **rest, Token *tok) {
    ASTNode *node = log_and(&tok, tok);
    
    while (equal(tok, "||")) {
        node = new_binary(ND_LOR, node, log_and(&tok, tok->next));
    }
    
    *rest = tok;
    return node;
}

/* Parse conditional expression */
static ASTNode *conditional(Token **rest, Token *tok) {
    ASTNode *node = log_or(&tok, tok);
    
    if (equal(tok, "?")) {
        ASTNode *cond_node = new_node(ND_COND);
        cond_node->cond = node;
        cond_node->then = expr(&tok, tok->next);
        tok = skip(tok, ":");
        cond_node->els = conditional(&tok, tok);
        *rest = tok;
        return cond_node;
    }
    
    *rest = tok;
    return node;
}

/* Parse assignment expression */
static ASTNode *assign(Token **rest, Token *tok) {
    ASTNode *node = conditional(&tok, tok);
    
    if (equal(tok, "=")) {
        node = new_binary(ND_ASSIGN, node, assign(&tok, tok->next));
    }
    
    *rest = tok;
    return node;
}

/* Parse expression */
static ASTNode *expr(Token **rest, Token *tok) {
    ASTNode *node = assign(&tok, tok);
    
    /* Comma operator */
    if (equal(tok, ",")) {
        node = new_binary(ND_COMMA, node, expr(&tok, tok->next));
    }
    
    *rest = tok;
    return node;
}

/* Parse expression statement */
static ASTNode *expr_stmt(Token **rest, Token *tok) {
    if (equal(tok, ";")) {
        *rest = tok->next;
        return new_node(ND_NULL_STMT);
    }
    
    ASTNode *node = new_node(ND_EXPR_STMT);
    node->lhs = expr(&tok, tok);
    *rest = skip(tok, ";");
    return node;
}

/* Parse statement */
static ASTNode *stmt(Token **rest, Token *tok) {
    /* Return statement */
    if (tok->kind == TK_RETURN) {
        ASTNode *node = new_node(ND_RETURN);
        
        /* Check if there's a return value */
        if (!equal(tok->next, ";")) {
            node->lhs = expr(&tok, tok->next);
        } else {
            tok = tok->next;
        }
        
        *rest = skip(tok, ";");
        return node;
    }
    
    /* If statement */
    if (tok->kind == TK_IF) {
        ASTNode *node = new_node(ND_IF);
        tok = skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = skip(tok, ")");
        node->then = stmt(&tok, tok);
        if (tok->kind == TK_ELSE) {
            node->els = stmt(&tok, tok->next);
        }
        *rest = tok;
        return node;
    }
    
    /* While statement */
    if (tok->kind == TK_WHILE) {
        ASTNode *node = new_node(ND_WHILE);
        tok = skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = skip(tok, ")");
        
        /* Save current labels and create new ones */
        char *old_brk = current_brk_label;
        char *old_cont = current_cont_label;
        static int loop_count = 0;
        char brk_label[32], cont_label[32];
        sprintf(brk_label, ".L.while.brk.%d", loop_count);
        sprintf(cont_label, ".L.while.cont.%d", loop_count++);
        current_brk_label = strdup_custom(brk_label);
        current_cont_label = strdup_custom(cont_label);
        node->brk_label = current_brk_label;
        node->cont_label = current_cont_label;
        
        node->then = stmt(&tok, tok);
        
        /* Restore labels */
        current_brk_label = old_brk;
        current_cont_label = old_cont;
        
        *rest = tok;
        return node;
    }
    
    /* For statement */
    if (tok->kind == TK_FOR) {
        ASTNode *node = new_node(ND_FOR);
        tok = skip(tok->next, "(");
        
        /* Check if init is a declaration (C99 style) */
        if (!equal(tok, ";")) {
            /* Check if it looks like a declaration */
            bool is_decl = (tok->kind == TK_INT || tok->kind == TK_CHAR || 
                           tok->kind == TK_VOID || tok->kind == TK_STATIC ||
                           tok->kind == TK_EXTERN || tok->kind == TK_CONST ||
                           tok->kind == TK_TYPEDEF || tok->kind == TK_ENUM ||
                           tok->kind == TK_STRUCT ||
                           (tok->kind == TK_IDENT && find_typedef(tok)));
            
            if (is_decl) {
                /* Parse declaration as init - simplified version */
                DeclSpec *spec = declspec(&tok, tok);
                Type *basety = spec->ty;
                
                /* Parse declarator */
                Type *ty = declarator(&tok, tok, basety);
                
                if (tok->kind != TK_IDENT) {
                    error_tok(tok, "expected variable name in for loop");
                }
                
                char *name = strndup_custom(tok->str, tok->len);
                tok = tok->next;
                
                /* Handle array declarator */
                if (equal(tok, "[")) {
                    tok = tok->next;
                    int len = 0;
                    if (tok->kind == TK_NUM) {
                        len = tok->val;
                        tok = tok->next;
                    }
                    tok = skip(tok, "]");
                    ty = array_of(ty, len);
                }
                
                /* Create local variable */
                Symbol *var = new_lvar(name, ty);
                var->is_static = spec->is_static;
                var->is_extern = spec->is_extern;
                
                /* Parse initializer if present */
                if (equal(tok, "=")) {
                    tok = tok->next;
                    ASTNode *var_node = new_node(ND_VAR);
                    var_node->var = var;
                    ASTNode *init_expr = expr(&tok, tok);
                    ASTNode *assign = new_binary(ND_ASSIGN, var_node, init_expr);
                    node->init = new_node(ND_EXPR_STMT);
                    node->init->lhs = assign;
                }
                
                tok = skip(tok, ";");
            } else {
                /* Regular expression init */
                node->init = expr_stmt(&tok, tok);
            }
        } else {
            tok = tok->next;
        }
        
        if (!equal(tok, ";")) {
            node->cond = expr(&tok, tok);
        }
        tok = skip(tok, ";");
        
        if (!equal(tok, ")")) {
            node->inc = expr(&tok, tok);
        }
        tok = skip(tok, ")");
        
        /* Save current labels and create new ones */
        char *old_brk = current_brk_label;
        char *old_cont = current_cont_label;
        static int for_count = 0;
        char brk_label[32], cont_label[32];
        sprintf(brk_label, ".L.for.brk.%d", for_count);
        sprintf(cont_label, ".L.for.cont.%d", for_count++);
        current_brk_label = strdup_custom(brk_label);
        current_cont_label = strdup_custom(cont_label);
        node->brk_label = current_brk_label;
        node->cont_label = current_cont_label;
        
        node->then = stmt(&tok, tok);
        
        /* Restore labels */
        current_brk_label = old_brk;
        current_cont_label = old_cont;
        
        *rest = tok;
        return node;
    }
    
    /* Switch statement */
    if (tok->kind == TK_SWITCH) {
        ASTNode *node = new_node(ND_SWITCH);
        tok = skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = skip(tok, ")");
        
        /* Save current break label and create new one for switch */
        char *old_brk = current_brk_label;
        static int switch_count = 0;
        char brk_label[32];
        sprintf(brk_label, ".L.switch.brk.%d", switch_count++);
        current_brk_label = strdup_custom(brk_label);
        node->brk_label = current_brk_label;
        
        /* Parse switch body (should be compound statement with case/default) */
        node->then = stmt(&tok, tok);
        
        /* Restore break label */
        current_brk_label = old_brk;
        
        *rest = tok;
        return node;
    }
    
    /* Case statement */
    if (tok->kind == TK_CASE) {
        ASTNode *node = new_node(ND_CASE);
        node->val = eval_const_expr(expr(&tok, tok->next));
        tok = skip(tok, ":");
        node->lhs = stmt(&tok, tok);
        *rest = tok;
        return node;
    }
    
    /* Default statement */
    if (tok->kind == TK_DEFAULT) {
        ASTNode *node = new_node(ND_CASE);
        node->val = -1;  /* Special value for default */
        tok = skip(tok->next, ":");
        node->lhs = stmt(&tok, tok);
        *rest = tok;
        return node;
    }
    
    /* Break statement */
    if (tok->kind == TK_BREAK) {
        ASTNode *node = new_node(ND_BREAK);
        node->brk_label = current_brk_label;
        *rest = skip(tok->next, ";");
        return node;
    }
    
    /* Continue statement */
    if (tok->kind == TK_CONTINUE) {
        ASTNode *node = new_node(ND_CONTINUE);
        node->cont_label = current_cont_label;
        *rest = skip(tok->next, ";");
        return node;
    }
    
    /* Compound statement */
    if (equal(tok, "{")) {
        return compound_stmt(rest, tok);
    }
    
    /* Expression statement */
    return expr_stmt(rest, tok);
}

/* Generate initialization code for a variable */
static void gen_init_code(ASTNode **cur_stmt, ASTNode *var_node, Initializer *init, Type *ty) {
    if (!init) return;
    
    if (init->is_expr) {
        /* Simple expression initializer */
        ASTNode *assign_node = new_binary(ND_ASSIGN, var_node, init->expr);
        *cur_stmt = (*cur_stmt)->next = new_node(ND_EXPR_STMT);
        (*cur_stmt)->lhs = assign_node;
    } else if (init->children) {
        /* Compound initializer - handle array or struct */
        if (ty->kind == TY_ARRAY) {
            /* Array initialization */
            int idx = 0;
            for (Initializer *child = init->children; child; child = child->next) {
                /* Create array element access: var[idx] */
                ASTNode *addr = new_node(ND_ADDR);
                addr->lhs = var_node;
                
                ASTNode *index = new_num(idx);
                ASTNode *ptr = new_binary(ND_ADD, addr, index);
                
                ASTNode *elem = new_node(ND_DEREF);
                elem->lhs = ptr;
                
                /* Recursively initialize element */
                gen_init_code(cur_stmt, elem, child, ty->base);
                idx++;
            }
        } else if (ty->kind == TY_STRUCT) {
            /* Struct initialization */
            Member *mem = ty->members;
            for (Initializer *child = init->children; child && mem; child = child->next, mem = mem->next) {
                /* Create member access: var.member */
                ASTNode *member_access = new_node(ND_MEMBER);
                member_access->lhs = var_node;
                member_access->member = mem;
                
                /* Recursively initialize member */
                gen_init_code(cur_stmt, member_access, child, mem->ty);
            }
        }
    }
}

/* Parse compound statement */
static ASTNode *compound_stmt(Token **rest, Token *tok) {
    tok = skip(tok, "{");
    
    ASTNode head = {0};
    ASTNode *cur = &head;
    
    while (!equal(tok, "}")) {
        /* Check if this is a declaration */
        bool is_decl = false;
        if (tok->kind == TK_INT || tok->kind == TK_CHAR || tok->kind == TK_VOID ||
            tok->kind == TK_TYPEDEF || tok->kind == TK_STATIC || tok->kind == TK_EXTERN ||
            tok->kind == TK_CONST || tok->kind == TK_ENUM || tok->kind == TK_STRUCT) {
            is_decl = true;
        } else if (tok->kind == TK_IDENT && find_typedef(tok)) {
            /* Typedef name */
            is_decl = true;
        }
        
        /* Declaration */
        if (is_decl) {
            DeclSpec *spec = declspec(&tok, tok);
            
            /* Handle typedef in local scope (unusual but legal) */
            if (spec->is_typedef) {
                Type *ty = declarator(&tok, tok, spec->ty);
                
                if (tok->kind != TK_IDENT) {
                    error_tok(tok, "expected typedef name");
                }
                Symbol *td = calloc(1, sizeof(Symbol));
                td->name = strndup_custom(tok->str, tok->len);
                td->ty = ty;
                td->is_typedef = true;
                td->next = typedefs;
                typedefs = td;
                tok = skip(tok->next, ";");
                continue;
            }
            
            Type *basety = spec->ty;
            bool first_var = true;
            
            while (!equal(tok, ";")) {
                if (!first_var) {
                    tok = skip(tok, ",");
                }
                first_var = false;
                
                Type *ty = declarator(&tok, tok, basety);
                
                if (tok->kind != TK_IDENT) {
                    error_tok(tok, "expected variable name");
                }
                
                char *name = strndup_custom(tok->str, tok->len);
                tok = tok->next;
                
                /* Handle array declarator */
                if (equal(tok, "[")) {
                    tok = tok->next;
                    int len = 0;
                    if (tok->kind == TK_NUM) {
                        len = tok->val;
                        tok = tok->next;
                    }
                    tok = skip(tok, "]");
                    ty = array_of(ty, len);
                }
                
                Symbol *var = new_lvar(name, ty);
                var->is_static = spec->is_static;
                var->is_extern = spec->is_extern;
                
                if (equal(tok, "=")) {
                    tok = tok->next;
                    
                    /* Parse initializer */
                    Initializer *init = parse_initializer(&tok, tok, ty);
                    var->init = init;
                    
                    /* Generate assignment code for the initializer */
                    ASTNode *var_node = new_node(ND_VAR);
                    var_node->var = var;
                    gen_init_code(&cur, var_node, init, ty);
                }
            }
            tok = skip(tok, ";");
            continue;
        }
        
        cur = cur->next = stmt(&tok, tok);
    }
    
    ASTNode *node = new_node(ND_BLOCK);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

/* Create a new initializer */
static Initializer *new_initializer(Type *ty) {
    Initializer *init = calloc(1, sizeof(Initializer));
    init->ty = ty;
    return init;
}

/* Parse initializer */
Initializer *parse_initializer(Token **rest, Token *tok, Type *ty) {
    Initializer *init = new_initializer(ty);
    
    /* Handle brace initializer */
    if (equal(tok, "{")) {
        tok = tok->next;
        
        /* Check for zero initializer: {0} */
        bool is_zero_init = false;
        if (tok->kind == TK_NUM && tok->val == 0) {
            Token *next = tok->next;
            if (equal(next, "}")) {
                is_zero_init = true;
                tok = next;
            }
        }
        
        if (is_zero_init) {
            /* Zero initializer - all elements/fields set to 0 */
            /* For now, we don't generate any initialization code */
            /* Local variables are already zeroed by the stack allocation */
            *rest = skip(tok, "}");
            return init;
        }
        
        /* Handle array type */
        if (ty->kind == TY_ARRAY) {
            int i = 0;
            Initializer *cur = NULL;
            
            while (!equal(tok, "}")) {
                if (i > 0) {
                    tok = skip(tok, ",");
                    if (equal(tok, "}")) break;  /* Allow trailing comma */
                }
                
                /* Parse element initializer */
                Initializer *elem = parse_initializer(&tok, tok, ty->base);
                elem->index = i;
                
                if (cur) {
                    cur->next = elem;
                } else {
                    init->children = elem;
                }
                cur = elem;
                i++;
            }
            *rest = skip(tok, "}");
            return init;
        }
        
        /* Handle struct type */
        if (ty->kind == TY_STRUCT) {
            if (!ty->members) {
                error_tok(tok, "struct has no members");
            }
            
            Member *mem = ty->members;
            Initializer *cur = NULL;
            int i = 0;
            
            while (!equal(tok, "}") && mem) {
                if (i > 0) {
                    tok = skip(tok, ",");
                    if (equal(tok, "}")) break;
                }
                
                /* Parse member initializer */
                Initializer *elem = parse_initializer(&tok, tok, mem->ty);
                elem->index = i;
                
                if (cur) {
                    cur->next = elem;
                } else {
                    init->children = elem;
                }
                cur = elem;
                mem = mem->next;
                i++;
            }
            *rest = skip(tok, "}");
            return init;
        }
        
        /* Handle simple types (int, char, ptr) - for now, treat like single value */
        if (ty->kind == TY_INT || ty->kind == TY_CHAR || ty->kind == TY_PTR) {
            /* For simple types in braces, parse a single value */
            ASTNode *expr_node = assign(&tok, tok);
            Initializer *elem = new_initializer(ty);
            elem->is_expr = true;
            elem->expr = expr_node;
            
            init->children = elem;
            /* Allow optional trailing comma */
            if (equal(tok, ",")) {
                tok = tok->next;
            }
            *rest = skip(tok, "}");
            return init;
        }
        
        error_tok(tok, "unsupported initializer type");
    }
    
    /* Simple expression initializer */
    init->is_expr = true;
    init->expr = assign(rest, tok);
    return init;
}

/* Parse type specifier */
/* Parse declaration specifiers (type + storage class) */
static DeclSpec *declspec(Token **rest, Token *tok) {
    DeclSpec *spec = calloc(1, sizeof(DeclSpec));
    
    /* Parse storage class specifiers and type qualifiers */
    while (true) {
        if (tok->kind == TK_TYPEDEF) {
            spec->is_typedef = true;
            tok = tok->next;
            continue;
        }
        if (tok->kind == TK_STATIC) {
            spec->is_static = true;
            tok = tok->next;
            continue;
        }
        if (tok->kind == TK_EXTERN) {
            spec->is_extern = true;
            tok = tok->next;
            continue;
        }
        if (tok->kind == TK_CONST) {
            /* Ignore const for now */
            tok = tok->next;
            continue;
        }
        break;
    }
    
    /* Parse type specifier */
    if (tok->kind == TK_VOID) {
        spec->ty = ty_void;
        *rest = tok->next;
        return spec;
    }
    if (tok->kind == TK_CHAR) {
        spec->ty = ty_char;
        *rest = tok->next;
        return spec;
    }
    if (tok->kind == TK_INT) {
        spec->ty = ty_int;
        *rest = tok->next;
        return spec;
    }
    
    /* Enum type */
    if (tok->kind == TK_ENUM) {
        tok = tok->next;
        
        /* Skip optional enum tag */
        if (tok->kind == TK_IDENT) {
            tok = tok->next;
        }
        
        /* Parse enum body */
        if (equal(tok, "{")) {
            tok = tok->next;
            int val = 0;
            
            while (!equal(tok, "}")) {
                if (tok->kind != TK_IDENT) {
                    error_tok(tok, "expected enum constant name");
                }
                
                char *name = strndup_custom(tok->str, tok->len);
                tok = tok->next;
                
                /* Check for explicit value */
                if (equal(tok, "=")) {
                    tok = tok->next;
                    if (tok->kind != TK_NUM) {
                        error_tok(tok, "expected number in enum");
                    }
                    val = tok->val;
                    tok = tok->next;
                }
                
                /* Create enum constant with current value */
                Symbol *e = calloc(1, sizeof(Symbol));
                e->name = name;
                e->enum_val = val;
                e->next = enums;
                enums = e;
                
                val++;
                
                if (equal(tok, ",")) {
                    tok = tok->next;
                }
            }
            tok = skip(tok, "}");
        }
        
        spec->ty = ty_int;  /* Enums are treated as int */
        *rest = tok;
        return spec;
    }
    
    /* Struct type */
    if (tok->kind == TK_STRUCT) {
        tok = tok->next;
        
        /* Optional struct tag (we'll ignore it for now) */
        if (tok->kind == TK_IDENT) {
            tok = tok->next;
        }
        
        /* Parse struct body */
        if (equal(tok, "{")) {
            tok = tok->next;
            
            Type *struct_ty = calloc(1, sizeof(Type));
            struct_ty->kind = TY_STRUCT;
            struct_ty->size = 0;
            struct_ty->align = 1;
            
            Member head = {0};
            Member *cur_mem = &head;
            int offset = 0;
            
            /* Parse struct members */
            while (!equal(tok, "}")) {
                /* Parse member type */
                DeclSpec *mem_spec = declspec(&tok, tok);
                
                /* Parse member declarator(s) */
                bool first_member = true;
                while (!equal(tok, ";")) {
                    if (!first_member) {
                        tok = skip(tok, ",");
                    }
                    first_member = false;
                    
                    Type *mem_ty = declarator(&tok, tok, mem_spec->ty);
                    
                    if (tok->kind != TK_IDENT) {
                        error_tok(tok, "expected member name");
                    }
                    
                    Member *mem = calloc(1, sizeof(Member));
                    mem->ty = mem_ty;
                    mem->name = strndup_custom(tok->str, tok->len);
                    mem->offset = offset;
                    
                    /* Update offset and size */
                    offset += mem_ty->size;
                    
                    cur_mem = cur_mem->next = mem;
                    tok = tok->next;
                }
                tok = skip(tok, ";");
            }
            tok = skip(tok, "}");
            
            struct_ty->members = head.next;
            struct_ty->size = offset;
            
            spec->ty = struct_ty;
            *rest = tok;
            return spec;
        }
        
        /* Struct without body - treat as opaque type */
        Type *struct_ty = calloc(1, sizeof(Type));
        struct_ty->kind = TY_STRUCT;
        struct_ty->size = 8;  /* Default size */
        struct_ty->align = 8;
        
        spec->ty = struct_ty;
        *rest = tok;
        return spec;
    }
    
    /* Check for typedef name */
    if (tok->kind == TK_IDENT) {
        Symbol *td = find_typedef(tok);
        if (td) {
            spec->ty = td->ty;
            *rest = tok->next;
            return spec;
        }
    }
    
    error_tok(tok, "expected type specifier");
    return NULL;
}

/* Parse declarator */
static Type *declarator(Token **rest, Token *tok, Type *ty) {
    /* Pointer */
    while (equal(tok, "*")) {
        ty = pointer_to(ty);
        tok = tok->next;
    }
    
    /* Identifier (we don't actually consume it here, just skip past it) */
    /* Note: The caller is responsible for consuming the identifier */
    
    *rest = tok;
    return ty;
}

/* Parse array/function suffix for declarator */
static Type *parse_declarator_suffix(Token **rest, Token *tok, Type *ty) {
    /* Array declarator: [size] */
    if (equal(tok, "[")) {
        tok = tok->next;
        int len = 0;
        if (tok->kind == TK_NUM) {
            len = tok->val;
            tok = tok->next;
        }
        tok = skip(tok, "]");
        ty = array_of(ty, len);
        /* Recursively handle more dimensions or function */
        ty = parse_declarator_suffix(&tok, tok, ty);
    }
    
    *rest = tok;
    return ty;
}

/* Parse function parameters */
static void parse_params(Token **rest, Token *tok, Symbol *fn) {
    tok = skip(tok, "(");
    
    if (equal(tok, ")")) {
        *rest = tok->next;
        return;
    }
    
    /* Handle void parameter (no parameters) */
    if (tok->kind == TK_VOID && equal(tok->next, ")")) {
        tok = tok->next;
        *rest = tok->next;
        return;
    }
    
    Symbol head = {0};
    Symbol *cur = &head;
    
    while (!equal(tok, ")")) {
        if (cur != &head) {
            tok = skip(tok, ",");
        }
        
        /* Check for variadic parameter (...) */
        if (tok->kind == TK_ELLIPSIS) {
            fn->is_variadic = true;
            tok = tok->next;
            break;
        }
        
        DeclSpec *spec = declspec(&tok, tok);
        Type *ty = declarator(&tok, tok, spec->ty);
        
        if (tok->kind != TK_IDENT) {
            error_tok(tok, "expected parameter name");
        }
        
        Symbol *param = calloc(1, sizeof(Symbol));
        param->name = strndup_custom(tok->str, tok->len);
        param->ty = ty;
        param->is_local = true;
        
        cur = cur->next = param;
        tok = tok->next;
    }
    
    fn->params = head.next;
    *rest = tok->next;
}

/* Parse function definition or declaration */
static Symbol *function(Token **rest, Token *tok) {
    locals = NULL;
    
    DeclSpec *spec = declspec(&tok, tok);
    Type *ty = declarator(&tok, tok, spec->ty);
    
    Symbol *fn = calloc(1, sizeof(Symbol));
    fn->name = strndup_custom(tok->str, tok->len);
    fn->is_function = true;
    fn->is_static = spec->is_static;
    fn->is_extern = spec->is_extern;
    tok = tok->next;
    
    parse_params(&tok, tok, fn);
    
    /* Check if this is a declaration (prototype) or definition */
    if (equal(tok, ";")) {
        /* Function declaration - no body */
        fn->body = NULL;
        *rest = tok->next;
        return fn;
    }
    
    /* Function definition - has body */
    /* Add parameters to locals */
    for (Symbol *param = fn->params; param; param = param->next) {
        Symbol *local = calloc(1, sizeof(Symbol));
        local->name = param->name;
        local->ty = param->ty;
        local->is_local = true;
        local->next = locals;
        locals = local;
    }
    
    current_fn = fn;
    fn->body = compound_stmt(&tok, tok);
    fn->locals = locals;
    
    *rest = tok;
    return fn;
}

/* Parse global declaration or function */
static bool is_function(Token *tok) {
    /* Skip storage class specifiers and type qualifiers */
    while (tok->kind == TK_TYPEDEF || tok->kind == TK_STATIC || 
           tok->kind == TK_EXTERN || tok->kind == TK_CONST) {
        tok = tok->next;
    }
    
    /* Skip type specifier */
    if (tok->kind == TK_INT || tok->kind == TK_CHAR || tok->kind == TK_VOID) {
        tok = tok->next;
    } else if (tok->kind == TK_ENUM) {
        tok = tok->next;
        /* Skip enum tag */
        if (tok->kind == TK_IDENT) {
            tok = tok->next;
        }
        /* Skip enum body if present */
        if (equal(tok, "{")) {
            int depth = 1;
            tok = tok->next;
            while (depth > 0 && tok->kind != TK_EOF) {
                if (equal(tok, "{")) depth++;
                else if (equal(tok, "}")) depth--;
                tok = tok->next;
            }
        }
    } else if (tok->kind == TK_STRUCT) {
        tok = tok->next;
        /* Skip struct tag */
        if (tok->kind == TK_IDENT) {
            tok = tok->next;
        }
        /* Skip struct body if present */
        if (equal(tok, "{")) {
            int depth = 1;
            tok = tok->next;
            while (depth > 0 && tok->kind != TK_EOF) {
                if (equal(tok, "{")) depth++;
                else if (equal(tok, "}")) depth--;
                tok = tok->next;
            }
        }
    } else if (tok->kind == TK_IDENT) {
        /* Could be typedef name */
        tok = tok->next;
    }
    
    /* Skip pointers */
    while (equal(tok, "*")) {
        tok = tok->next;
    }
    
    /* Check for identifier followed by ( */
    if (tok->kind == TK_IDENT && equal(tok->next, "(")) {
        return true;
    }
    
    return false;
}

/* Parse program */
Symbol *parse(Token *tok) {
    initialize_types();
    
    Symbol head = {0};
    Symbol *cur = &head;
    
    while (tok->kind != TK_EOF) {
        if (is_function(tok)) {
            cur = cur->next = function(&tok, tok);
        } else {
            /* Global variable or typedef declaration */
            DeclSpec *spec = declspec(&tok, tok);
            
            /* Handle typedef */
            if (spec->is_typedef) {
                Type *ty = declarator(&tok, tok, spec->ty);
                
                if (tok->kind != TK_IDENT) {
                    error_tok(tok, "expected typedef name");
                }
                
                Symbol *td = calloc(1, sizeof(Symbol));
                td->name = strndup_custom(tok->str, tok->len);
                td->ty = ty;
                td->is_typedef = true;
                td->next = typedefs;
                typedefs = td;
                
                tok = skip(tok->next, ";");
                continue;
            }
            
            /* Global variable declaration */
            bool first_var = true;
            while (!equal(tok, ";")) {
                if (!first_var) {
                    tok = skip(tok, ",");
                }
                first_var = false;
                
                Type *ty = declarator(&tok, tok, spec->ty);
                
                if (tok->kind != TK_IDENT) {
                    error_tok(tok, "expected variable name");
                }
                
                char *var_name = strndup_custom(tok->str, tok->len);
                tok = tok->next;
                
                /* Parse array/function suffix */
                ty = parse_declarator_suffix(&tok, tok, ty);
                
                Symbol *var = new_gvar(var_name, ty);
                var->is_static = spec->is_static;
                var->is_extern = spec->is_extern;
                
                if (equal(tok, "=")) {
                    /* Parse global initializer */
                    tok = tok->next;
                    Initializer *init = parse_initializer(&tok, tok, ty);
                    var->init = init;
                }
            }
            tok = skip(tok, ";");
        }
    }
    
    /* Merge globals (including string literals) into the result chain */
    if (globals) {
        cur->next = globals;
    }
    
    return head.next;
}
