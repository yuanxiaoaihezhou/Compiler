#include "compiler.h"

static char *current_input;
static char *current_filename;
static int current_line = 1;

/* Check if character starts identifier */
static bool is_ident_start(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

/* Check if character continues identifier */
static bool is_ident_cont(char c) {
    return is_ident_start(c) || ('0' <= c && c <= '9');
}

/* Check if string starts with substring */
static bool startswith(char *p, char *q) {
    return strncmp(p, q, strlen(q)) == 0;
}

/* Read number */
static int read_number(char **p) {
    char *start = *p;
    int val = 0;
    while (isdigit(**p)) {
        val = val * 10 + (**p - '0');
        (*p)++;
    }
    return val;
}

/* Read identifier */
static char *read_ident(char **p) {
    char *start = *p;
    while (is_ident_cont(**p)) {
        (*p)++;
    }
    return strndup_custom(start, *p - start);
}

/* Read string literal */
static char *read_string_literal(char **p) {
    (*p)++; /* Skip opening quote */
    char *start = *p;
    char buf[1024];
    int len = 0;
    
    while (**p != '"') {
        if (**p == '\0') {
            error("unclosed string literal");
        }
        if (**p == '\\') {
            (*p)++;
            switch (**p) {
                case 'n': buf[len++] = '\n'; break;
                case 't': buf[len++] = '\t'; break;
                case 'r': buf[len++] = '\r'; break;
                case '0': buf[len++] = '\0'; break;
                case '\\': buf[len++] = '\\'; break;
                case '"': buf[len++] = '"'; break;
                default: buf[len++] = **p; break;
            }
            (*p)++;
        } else {
            buf[len++] = **p;
            (*p)++;
        }
    }
    (*p)++; /* Skip closing quote */
    
    char *str = malloc(len + 1);
    memcpy(str, buf, len);
    str[len] = '\0';
    return str;
}

/* Read character literal */
static int read_char_literal(char **p) {
    (*p)++; /* Skip opening quote */
    int c;
    
    if (**p == '\\') {
        (*p)++;
        switch (**p) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case '0': c = '\0'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            default: c = **p; break;
        }
        (*p)++;
    } else {
        c = **p;
        (*p)++;
    }
    
    if (**p != '\'') {
        error("unclosed character literal");
    }
    (*p)++; /* Skip closing quote */
    return c;
}

/* Create new token */
static Token *new_token(TokenKind kind, char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    tok->loc = str;
    tok->filename = current_filename;
    tok->line = current_line;
    return tok;
}

/* Check if identifier is keyword */
static TokenKind check_keyword(char *str, int len) {
    struct {
        char *name;
        TokenKind kind;
    } keywords[] = {
        {"int", TK_INT}, {"char", TK_CHAR}, {"void", TK_VOID},
        {"if", TK_IF}, {"else", TK_ELSE}, {"while", TK_WHILE},
        {"for", TK_FOR}, {"return", TK_RETURN}, {"sizeof", TK_SIZEOF},
        {"struct", TK_STRUCT}, {"typedef", TK_TYPEDEF}, {"enum", TK_ENUM},
        {"static", TK_STATIC}, {"extern", TK_EXTERN}, {"const", TK_CONST},
        {"break", TK_BREAK}, {"continue", TK_CONTINUE},
        {"switch", TK_SWITCH}, {"case", TK_CASE}, {"default", TK_DEFAULT},
    };
    
    for (int i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (strlen(keywords[i].name) == len && 
            strncmp(str, keywords[i].name, len) == 0) {
            return keywords[i].kind;
        }
    }
    return TK_IDENT;
}

/* Tokenize input string */
Token *tokenize(char *input, char *filename) {
    current_input = input;
    current_filename = filename;
    current_line = 1;
    
    Token head = {0};
    Token *cur = &head;
    char *p = input;
    
    while (*p) {
        /* Skip whitespace */
        if (isspace(*p)) {
            if (*p == '\n') {
                current_line++;
            }
            p++;
            continue;
        }
        
        /* Skip line comments */
        if (startswith(p, "//")) {
            p += 2;
            while (*p && *p != '\n') {
                p++;
            }
            continue;
        }
        
        /* Skip block comments */
        if (startswith(p, "/*")) {
            char *q = strstr(p + 2, "*/");
            if (!q) {
                error("unclosed block comment");
            }
            for (char *c = p; c < q; c++) {
                if (*c == '\n') current_line++;
            }
            p = q + 2;
            continue;
        }
        
        /* Multi-character operators */
        if (startswith(p, "==")) {
            cur = cur->next = new_token(TK_EQ, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "!=")) {
            cur = cur->next = new_token(TK_NE, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "<=")) {
            cur = cur->next = new_token(TK_LE, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, ">=")) {
            cur = cur->next = new_token(TK_GE, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "<<")) {
            cur = cur->next = new_token(TK_SHL, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, ">>")) {
            cur = cur->next = new_token(TK_SHR, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "&&")) {
            cur = cur->next = new_token(TK_LAND, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "||")) {
            cur = cur->next = new_token(TK_LOR, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "++")) {
            cur = cur->next = new_token(TK_INC, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "--")) {
            cur = cur->next = new_token(TK_DEC, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "->")) {
            cur = cur->next = new_token(TK_ARROW, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "+=")) {
            cur = cur->next = new_token(TK_PLUS_ASSIGN, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "-=")) {
            cur = cur->next = new_token(TK_MINUS_ASSIGN, p, 2);
            p += 2;
            continue;
        }
        if (startswith(p, "...")) {
            cur = cur->next = new_token(TK_ELLIPSIS, p, 3);
            p += 3;
            continue;
        }
        
        /* Single-character operators and punctuation */
        switch (*p) {
            case '+': cur = cur->next = new_token(TK_PLUS, p, 1); p++; continue;
            case '-': cur = cur->next = new_token(TK_MINUS, p, 1); p++; continue;
            case '*': cur = cur->next = new_token(TK_STAR, p, 1); p++; continue;
            case '/': cur = cur->next = new_token(TK_SLASH, p, 1); p++; continue;
            case '%': cur = cur->next = new_token(TK_PERCENT, p, 1); p++; continue;
            case '<': cur = cur->next = new_token(TK_LT, p, 1); p++; continue;
            case '>': cur = cur->next = new_token(TK_GT, p, 1); p++; continue;
            case '=': cur = cur->next = new_token(TK_ASSIGN, p, 1); p++; continue;
            case '&': cur = cur->next = new_token(TK_AND, p, 1); p++; continue;
            case '|': cur = cur->next = new_token(TK_OR, p, 1); p++; continue;
            case '^': cur = cur->next = new_token(TK_XOR, p, 1); p++; continue;
            case '!': cur = cur->next = new_token(TK_LNOT, p, 1); p++; continue;
            case '(': cur = cur->next = new_token(TK_LPAREN, p, 1); p++; continue;
            case ')': cur = cur->next = new_token(TK_RPAREN, p, 1); p++; continue;
            case '{': cur = cur->next = new_token(TK_LBRACE, p, 1); p++; continue;
            case '}': cur = cur->next = new_token(TK_RBRACE, p, 1); p++; continue;
            case '[': cur = cur->next = new_token(TK_LBRACKET, p, 1); p++; continue;
            case ']': cur = cur->next = new_token(TK_RBRACKET, p, 1); p++; continue;
            case ';': cur = cur->next = new_token(TK_SEMICOLON, p, 1); p++; continue;
            case ',': cur = cur->next = new_token(TK_COMMA, p, 1); p++; continue;
            case '?': cur = cur->next = new_token(TK_QUESTION, p, 1); p++; continue;
            case ':': cur = cur->next = new_token(TK_COLON, p, 1); p++; continue;
            case '.': cur = cur->next = new_token(TK_DOT, p, 1); p++; continue;
        }
        
        /* Numbers */
        if (isdigit(*p)) {
            char *start = p;
            int val = read_number(&p);
            cur = cur->next = new_token(TK_NUM, start, p - start);
            cur->val = val;
            continue;
        }
        
        /* String literals */
        if (*p == '"') {
            char *start = p;
            char *str = read_string_literal(&p);
            cur = cur->next = new_token(TK_STR, start, p - start);
            cur->str = str;
            continue;
        }
        
        /* Character literals */
        if (*p == '\'') {
            char *start = p;
            int val = read_char_literal(&p);
            cur = cur->next = new_token(TK_CHAR_LIT, start, p - start);
            cur->val = val;
            continue;
        }
        
        /* Identifiers and keywords */
        if (is_ident_start(*p)) {
            char *start = p;
            char *name = read_ident(&p);
            TokenKind kind = check_keyword(name, strlen(name));
            cur = cur->next = new_token(kind, start, p - start);
            if (kind == TK_IDENT) {
                cur->str = name;
            }
            continue;
        }
        
        error("invalid token at '%c'", *p);
    }
    
    cur = cur->next = new_token(TK_EOF, p, 0);
    return head.next;
}

/* Tokenize file */
Token *tokenize_file(char *filename) {
    char *input = read_file(filename);
    return tokenize(input, filename);
}
