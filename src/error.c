#include "compiler.h"
#include <errno.h>

/* Print error message and exit */
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

/* Report error at specific location */
void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    
    /* Find line start */
    char *line = loc;
    while (line > compiler_state->current_file && line[-1] != '\n') {
        line--;
    }
    
    /* Find line end */
    char *end = loc;
    while (*end && *end != '\n') {
        end++;
    }
    
    /* Print line */
    int line_num = 1;
    for (char *p = compiler_state->current_file; p < line; p++) {
        if (*p == '\n') {
            line_num++;
        }
    }
    
    fprintf(stderr, "%s:%d: ", compiler_state ? compiler_state->current_file : "unknown", line_num);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);
    
    /* Print error indicator */
    int pos = loc - line;
    fprintf(stderr, "%*s", pos, "");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

/* Report error at token */
void error_tok(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    
    if (tok) {
        fprintf(stderr, "%s:%d: ", tok->filename, tok->line);
    }
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}
