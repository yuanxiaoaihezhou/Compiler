#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

/* Forward declarations */
typedef struct Token Token;
typedef struct ASTNode ASTNode;
typedef struct Type Type;
typedef struct Symbol Symbol;
typedef struct IR IR;

/* Token types for lexical analysis */
typedef enum {
    /* Keywords */
    TK_INT, TK_CHAR, TK_VOID, TK_IF, TK_ELSE, TK_WHILE, TK_FOR, 
    TK_RETURN, TK_SIZEOF, TK_STRUCT, TK_TYPEDEF, TK_ENUM,
    TK_STATIC, TK_EXTERN, TK_CONST, TK_BREAK, TK_CONTINUE,
    
    /* Identifiers and literals */
    TK_IDENT, TK_NUM, TK_STR, TK_CHAR_LIT,
    
    /* Operators */
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE,
    TK_ASSIGN, TK_PLUS_ASSIGN, TK_MINUS_ASSIGN,
    TK_LAND, TK_LOR, TK_LNOT,
    TK_AND, TK_OR, TK_XOR, TK_SHL, TK_SHR,
    TK_INC, TK_DEC, TK_ARROW, TK_DOT,
    
    /* Punctuation */
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET, TK_SEMICOLON, TK_COMMA,
    TK_QUESTION, TK_COLON,
    
    /* Special */
    TK_EOF, TK_NEWLINE
} TokenKind;

/* Token structure */
struct Token {
    TokenKind kind;
    Token *next;
    int val;           /* For TK_NUM */
    char *str;         /* Token string */
    int len;           /* Token length */
    char *loc;         /* Source location */
    char *filename;    /* Source filename */
    int line;          /* Line number */
};

/* AST node types */
typedef enum {
    ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD,
    ND_EQ, ND_NE, ND_LT, ND_LE, ND_GT, ND_GE,
    ND_ASSIGN, ND_NUM, ND_VAR, ND_CALL, ND_ADDR, ND_DEREF,
    ND_RETURN, ND_IF, ND_WHILE, ND_FOR, ND_BLOCK,
    ND_FUNC_DEF, ND_EXPR_STMT, ND_NULL_STMT,
    ND_LAND, ND_LOR, ND_LNOT,
    ND_AND, ND_OR, ND_XOR, ND_SHL, ND_SHR,
    ND_MEMBER, ND_CAST, ND_SIZEOF, ND_COMMA,
    ND_COND, ND_BREAK, ND_CONTINUE
} NodeKind;

/* Type kinds */
typedef enum {
    TY_VOID, TY_CHAR, TY_INT, TY_PTR, TY_ARRAY, 
    TY_STRUCT, TY_FUNC, TY_ENUM
} TypeKind;

/* Type structure */
struct Type {
    TypeKind kind;
    int size;
    int align;
    Type *base;        /* Pointer or array base type */
    int array_len;     /* Array length */
    struct Member *members; /* Struct members */
    Type *return_ty;   /* Function return type */
    Type *params;      /* Function parameters */
    Type *next;        /* Next parameter */
};

/* Struct member */
typedef struct Member {
    struct Member *next;
    Type *ty;
    char *name;
    int offset;
} Member;

/* AST node structure */
struct ASTNode {
    NodeKind kind;
    ASTNode *next;
    Type *ty;
    
    ASTNode *lhs;      /* Left-hand side */
    ASTNode *rhs;      /* Right-hand side */
    
    /* For ND_IF, ND_WHILE, ND_FOR */
    ASTNode *cond;
    ASTNode *then;
    ASTNode *els;
    ASTNode *init;
    ASTNode *inc;
    
    /* For ND_BLOCK */
    ASTNode *body;
    
    /* For ND_NUM */
    int val;
    
    /* For ND_VAR */
    Symbol *var;
    
    /* For ND_CALL */
    char *funcname;
    ASTNode *args;
    
    /* For ND_MEMBER */
    Member *member;
};

/* Symbol (variable/function) */
struct Symbol {
    Symbol *next;
    char *name;
    Type *ty;
    bool is_local;
    int offset;        /* Offset from RBP for local variables */
    bool is_function;
    ASTNode *body;     /* Function body */
    Symbol *params;    /* Function parameters */
    Symbol *locals;    /* Local variables */
    int stack_size;    /* Stack size for function */
};

/* Intermediate representation */
typedef enum {
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,
    IR_MOV, IR_LOAD, IR_STORE,
    IR_CALL, IR_RET, IR_LABEL, IR_JMP, IR_JZ, IR_JNZ,
    IR_EQ, IR_NE, IR_LT, IR_LE, IR_GT, IR_GE,
    IR_AND, IR_OR, IR_XOR, IR_SHL, IR_SHR,
    IR_ADDR, IR_NOP
} IRKind;

struct IR {
    IRKind kind;
    int dst;           /* Destination register */
    int lhs;           /* Left operand */
    int rhs;           /* Right operand */
    int imm;           /* Immediate value */
    char *name;        /* For labels and function calls */
    IR *next;
};

/* Global compilation state */
typedef struct {
    Token *token;      /* Current token */
    Symbol *globals;   /* Global symbols */
    Symbol *functions; /* Function list */
    char **include_paths; /* Include search paths */
    int include_count;
    char *current_file;
} CompilerState;

/* Lexer functions */
Token *tokenize(char *input, char *filename);
Token *tokenize_file(char *filename);

/* Parser functions */
Symbol *parse(Token *tok);
Type *copy_type(Type *ty);

/* AST functions */
ASTNode *new_node(NodeKind kind);
ASTNode *new_binary(NodeKind kind, ASTNode *lhs, ASTNode *rhs);
ASTNode *new_num(int val);

/* Type functions */
Type *new_type(TypeKind kind, int size, int align);
Type *pointer_to(Type *base);
Type *array_of(Type *base, int len);
void add_type(ASTNode *node);

/* IR generation */
IR *gen_ir(Symbol *prog);

/* Optimization */
IR *optimize(IR *ir);

/* Code generation */
void codegen(Symbol *prog, FILE *out);

/* Preprocessor */
char *preprocess(char *filename);

/* Error handling */
void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);

/* Utility functions */
char *read_file(char *path);
bool consume(Token **rest, Token *tok, char *op);
bool equal(Token *tok, char *op);
Token *skip(Token *tok, char *op);
char *strndup_custom(const char *s, size_t n);

/* Global state */
extern CompilerState *compiler_state;

#endif /* COMPILER_H */
