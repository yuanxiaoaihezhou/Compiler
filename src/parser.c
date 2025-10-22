#include "compiler.h"

static Symbol *locals;
static Symbol *globals;
static Symbol *current_fn;

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
static Type *declspec(Token **rest, Token *tok);
static Type *declarator(Token **rest, Token *tok, Type *ty);
static Symbol *find_var(Token *tok);

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
        Symbol *var = new_gvar(tok->str, array_of(ty_char, strlen(tok->str) + 1));
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
        
        *rest = tok;
        return node;
    }
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
    if (tok->kind == TK_SIZEOF) {
        ASTNode *node = unary(rest, tok->next);
        add_type(node);
        ASTNode *size_node = new_num(node->ty->size);
        return size_node;
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
        node->lhs = expr(&tok, tok->next);
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
        node->then = stmt(&tok, tok);
        *rest = tok;
        return node;
    }
    
    /* For statement */
    if (tok->kind == TK_FOR) {
        ASTNode *node = new_node(ND_FOR);
        tok = skip(tok->next, "(");
        
        if (!equal(tok, ";")) {
            node->init = expr_stmt(&tok, tok);
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
        
        node->then = stmt(&tok, tok);
        *rest = tok;
        return node;
    }
    
    /* Break statement */
    if (tok->kind == TK_BREAK) {
        *rest = skip(tok->next, ";");
        return new_node(ND_BREAK);
    }
    
    /* Continue statement */
    if (tok->kind == TK_CONTINUE) {
        *rest = skip(tok->next, ";");
        return new_node(ND_CONTINUE);
    }
    
    /* Compound statement */
    if (equal(tok, "{")) {
        return compound_stmt(rest, tok);
    }
    
    /* Expression statement */
    return expr_stmt(rest, tok);
}

/* Parse compound statement */
static ASTNode *compound_stmt(Token **rest, Token *tok) {
    tok = skip(tok, "{");
    
    ASTNode head = {0};
    ASTNode *cur = &head;
    
    while (!equal(tok, "}")) {
        /* Declaration */
        if (tok->kind == TK_INT || tok->kind == TK_CHAR || tok->kind == TK_VOID) {
            Type *basety = declspec(&tok, tok);
            
            while (!equal(tok, ";")) {
                if (cur != &head) {
                    tok = skip(tok, ",");
                }
                
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
                
                if (equal(tok, "=")) {
                    ASTNode *lhs = new_node(ND_VAR);
                    lhs->var = var;
                    ASTNode *rhs = assign(&tok, tok->next);
                    ASTNode *node = new_binary(ND_ASSIGN, lhs, rhs);
                    cur = cur->next = new_node(ND_EXPR_STMT);
                    cur->lhs = node;
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

/* Parse type specifier */
static Type *declspec(Token **rest, Token *tok) {
    if (tok->kind == TK_VOID) {
        *rest = tok->next;
        return ty_void;
    }
    if (tok->kind == TK_CHAR) {
        *rest = tok->next;
        return ty_char;
    }
    if (tok->kind == TK_INT) {
        *rest = tok->next;
        return ty_int;
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
    
    Symbol head = {0};
    Symbol *cur = &head;
    
    while (!equal(tok, ")")) {
        if (cur != &head) {
            tok = skip(tok, ",");
        }
        
        Type *basety = declspec(&tok, tok);
        Type *ty = declarator(&tok, tok, basety);
        
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

/* Parse function definition */
static Symbol *function(Token **rest, Token *tok) {
    locals = NULL;
    
    Type *ty = declspec(&tok, tok);
    ty = declarator(&tok, tok, ty);
    
    Symbol *fn = calloc(1, sizeof(Symbol));
    fn->name = strndup_custom(tok->str, tok->len);
    fn->is_function = true;
    tok = tok->next;
    
    parse_params(&tok, tok, fn);
    
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
    /* Skip type specifier */
    if (tok->kind == TK_INT || tok->kind == TK_CHAR || tok->kind == TK_VOID) {
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
            /* Global variable declaration */
            Type *basety = declspec(&tok, tok);
            
            while (!equal(tok, ";")) {
                if (cur != &head) {
                    tok = skip(tok, ",");
                }
                
                Type *ty = declarator(&tok, tok, basety);
                
                if (tok->kind != TK_IDENT) {
                    error_tok(tok, "expected variable name");
                }
                
                Symbol *var = new_gvar(strndup_custom(tok->str, tok->len), ty);
                tok = tok->next;
                
                if (equal(tok, "=")) {
                    /* Skip initializer for now */
                    tok = tok->next;
                    while (!equal(tok, ",") && !equal(tok, ";")) {
                        tok = tok->next;
                    }
                }
            }
            tok = skip(tok, ";");
        }
    }
    
    return head.next;
}
