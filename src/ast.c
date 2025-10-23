#include "compiler.h"

/* Create new AST node */
ASTNode *new_node(NodeKind kind) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    node->kind = kind;
    return node;
}

/* Create binary operation node */
ASTNode *new_binary(NodeKind kind, ASTNode *lhs, ASTNode *rhs) {
    ASTNode *node = new_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

/* Create number node */
ASTNode *new_num(int val) {
    ASTNode *node = new_node(ND_NUM);
    node->val = val;
    return node;
}

/* Copy AST node (shallow copy for simple cases) */
ASTNode *copy_node(ASTNode *node) {
    if (!node) return NULL;
    
    ASTNode *copy = calloc(1, sizeof(ASTNode));
    memcpy(copy, node, sizeof(ASTNode));
    
    /* For safety, don't copy next pointer */
    copy->next = NULL;
    
    return copy;
}

/* Create new type */
Type *new_type(TypeKind kind, int size, int align) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = kind;
    ty->size = size;
    ty->align = align;
    return ty;
}

/* Create pointer type */
Type *pointer_to(Type *base) {
    Type *ty = new_type(TY_PTR, 8, 8);
    ty->base = base;
    return ty;
}

/* Create array type */
Type *array_of(Type *base, int len) {
    Type *ty = new_type(TY_ARRAY, base->size * len, base->align);
    ty->base = base;
    ty->array_len = len;
    return ty;
}

/* Copy type */
Type *copy_type(Type *ty) {
    Type *new = calloc(1, sizeof(Type));
    *new = *ty;
    return new;
}

/* Get size of type */
static int get_type_size(Type *ty) {
    switch (ty->kind) {
        case TY_VOID: return 0;
        case TY_CHAR: return 1;
        case TY_INT: return 4;
        case TY_PTR: return 8;
        case TY_ARRAY: return ty->base->size * ty->array_len;
        case TY_STRUCT: return ty->size;
        default: return 4;
    }
}

/* Add type information to AST nodes */
void add_type(ASTNode *node) {
    if (!node || node->ty) {
        return;
    }
    
    add_type(node->lhs);
    add_type(node->rhs);
    add_type(node->cond);
    add_type(node->then);
    add_type(node->els);
    add_type(node->init);
    add_type(node->inc);
    
    for (ASTNode *n = node->body; n; n = n->next) {
        add_type(n);
    }
    for (ASTNode *n = node->args; n; n = n->next) {
        add_type(n);
    }
    
    switch (node->kind) {
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_MOD:
        case ND_AND:
        case ND_OR:
        case ND_XOR:
        case ND_SHL:
        case ND_SHR:
        case ND_NOT:
            node->ty = node->lhs->ty;
            return;
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_GT:
        case ND_GE:
        case ND_LAND:
        case ND_LOR:
        case ND_LNOT:
            node->ty = new_type(TY_INT, 4, 4);
            return;
        case ND_ASSIGN:
            node->ty = node->lhs->ty;
            return;
        case ND_NUM:
            node->ty = new_type(TY_INT, 4, 4);
            return;
        case ND_VAR:
            node->ty = node->var->ty;
            return;
        case ND_ADDR:
            if (node->lhs->ty->kind == TY_ARRAY) {
                node->ty = pointer_to(node->lhs->ty->base);
            } else {
                node->ty = pointer_to(node->lhs->ty);
            }
            return;
        case ND_DEREF:
            if (node->lhs->ty->kind == TY_PTR) {
                node->ty = node->lhs->ty->base;
            } else {
                node->ty = new_type(TY_INT, 4, 4);
            }
            return;
        case ND_MEMBER: {
            /* Resolve member name to Member* */
            if (!node->member && node->funcname) {
                /* Get the struct type from lhs */
                Type *struct_ty = node->lhs->ty;
                if (!struct_ty) {
                    /* Type not yet determined - will retry later */
                    return;
                }
                
                /* For pointer types, get the base type */
                if (struct_ty->kind == TY_PTR) {
                    struct_ty = struct_ty->base;
                }
                
                if (struct_ty->kind != TY_STRUCT) {
                    /* Not a struct - error will be caught elsewhere */
                    node->ty = new_type(TY_INT, 4, 4); /* fallback */
                    return;
                }
                
                /* Look up member by name */
                for (Member *mem = struct_ty->members; mem; mem = mem->next) {
                    if (strcmp(mem->name, node->funcname) == 0) {
                        node->member = mem;
                        break;
                    }
                }
                
                if (!node->member) {
                    /* Member not found - error will be caught elsewhere */
                    node->ty = new_type(TY_INT, 4, 4); /* fallback */
                    return;
                }
            }
            
            if (node->member) {
                node->ty = node->member->ty;
            }
            return;
        }
        case ND_SIZEOF:
            node->ty = new_type(TY_INT, 4, 4);
            return;
        case ND_COND:
            node->ty = node->then->ty;
            return;
        case ND_COMMA:
            node->ty = node->rhs->ty;
            return;
    }
}
