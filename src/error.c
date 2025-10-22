#include "compiler.h"
#include <errno.h>

/* Print error message and exit */
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "\033[1m\033[31merror:\033[0m ");
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
    
    /* Calculate line number */
    int line_num = 1;
    for (char *p = compiler_state->current_file; p < line; p++) {
        if (*p == '\n') {
            line_num++;
        }
    }
    
    /* Calculate column position */
    int pos = loc - line;
    
    /* Print error location with color */
    fprintf(stderr, "\033[1m%s:%d:%d: \033[31merror:\033[0m ", 
            compiler_state ? compiler_state->current_file : "unknown", line_num, pos + 1);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    
    /* Print the source line */
    fprintf(stderr, "%5d | %.*s\n", line_num, (int)(end - line), line);
    
    /* Print error indicator */
    fprintf(stderr, "      | %*s\033[32m^\033[0m\n", pos, "");
    
    va_end(ap);
    exit(1);
}

/* Report error at token */
void error_tok(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    
    if (tok && tok->loc && tok->filename) {
        /* Find line start - with bounds checking */
        char *line = tok->loc;
        int safe_lookback = 0;
        while (safe_lookback < 1000 && line[-1] != '\n' && line[-1] != '\0') {
            line--;
            safe_lookback++;
            if (line <= tok->loc - 1000) break;  /* Safety limit */
        }
        
        /* Find line end */
        char *end = tok->loc;
        while (*end && *end != '\n') {
            end++;
        }
        
        /* Calculate line position */
        int pos = tok->loc - line;
        
        /* Print error location */
        fprintf(stderr, "\033[1m%s:%d:%d: \033[31merror:\033[0m ", 
                tok->filename, tok->line, pos + 1);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
        
        /* Print the source line */
        fprintf(stderr, "%5d | %.*s\n", tok->line, (int)(end - line), line);
        
        /* Print error indicator with better visualization */
        fprintf(stderr, "      | %*s\033[32m^", pos, "");
        if (tok->len > 1) {
            for (int i = 1; i < tok->len && i < 20; i++) {
                fprintf(stderr, "~");
            }
        }
        fprintf(stderr, "\033[0m\n");
    } else if (tok) {
        fprintf(stderr, "\033[1m%s:%d: \033[31merror:\033[0m ", 
                tok->filename ? tok->filename : "unknown", tok->line);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
    } else {
        fprintf(stderr, "\033[1m\033[31merror:\033[0m ");
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
    }
    
    va_end(ap);
    exit(1);
}

/* Print a note (non-fatal) */
void note_tok(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    
    if (tok && tok->loc && tok->filename) {
        /* Find line start - with bounds checking */
        char *line = tok->loc;
        int safe_lookback = 0;
        while (safe_lookback < 1000 && line[-1] != '\n' && line[-1] != '\0') {
            line--;
            safe_lookback++;
            if (line <= tok->loc - 1000) break;  /* Safety limit */
        }
        
        /* Find line end */
        char *end = tok->loc;
        while (*end && *end != '\n') {
            end++;
        }
        
        /* Calculate line position */
        int pos = tok->loc - line;
        
        /* Print note location */
        fprintf(stderr, "\033[1m%s:%d:%d: \033[36mnote:\033[0m ", 
                tok->filename, tok->line, pos + 1);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
        
        /* Print the source line */
        fprintf(stderr, "%5d | %.*s\n", tok->line, (int)(end - line), line);
        
        /* Print note indicator */
        fprintf(stderr, "      | %*s\033[36m^\033[0m\n", pos, "");
    } else if (tok) {
        fprintf(stderr, "\033[1m%s:%d: \033[36mnote:\033[0m ", 
                tok->filename ? tok->filename : "unknown", tok->line);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
    } else {
        fprintf(stderr, "\033[1m\033[36mnote:\033[0m ");
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
    }
    
    va_end(ap);
}

