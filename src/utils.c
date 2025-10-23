#include "compiler.h"

CompilerState *compiler_state = NULL;

/* Read entire file into memory */
char *read_file(char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        error("cannot open %s", path);
    }
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* Read file */
    char *buf = calloc(1, size + 2);
    fread(buf, size, 1, fp);
    
    /* Ensure file ends with newline */
    if (size == 0 || buf[size - 1] != '\n') {
        buf[size++] = '\n';
    }
    buf[size] = '\0';
    
    fclose(fp);
    return buf;
}

/* Check if token matches expected string */
bool equal(Token *tok, char *op) {
    return tok && strlen(op) == tok->len && 
           strncmp(tok->str, op, tok->len) == 0;
}

/* Consume token if it matches */
bool consume(Token **rest, Token *tok, char *op) {
    if (equal(tok, op)) {
        *rest = tok->next;
        return true;
    }
    *rest = tok;
    return false;
}

/* Skip expected token */
Token *skip(Token *tok, char *op) {
    if (!equal(tok, op)) {
        error_tok(tok, "expected '%s'", op);
    }
    return tok->next;
}

/* String duplication */
char *strndup_custom(const char *s, int n) {
    char *new = malloc(n + 1);
    if (new) {
        memcpy(new, s, n);
        new[n] = '\0';
    }
    return new;
}

/* Full string duplication */
char *strdup_custom(const char *s) {
    size_t len = strlen(s);
    char *new = malloc(len + 1);
    if (new) {
        memcpy(new, s, len + 1);
    }
    return new;
}
