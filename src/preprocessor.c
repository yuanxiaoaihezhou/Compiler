#include "compiler.h"

#define MAX_INCLUDE_DEPTH 10
#define MAX_DEFINES 256

static char **include_paths;
static int include_count;
static int include_depth = 0;

/* Macro definition */
typedef struct Define {
    char *name;
    char *value;
} Define;

static Define defines[MAX_DEFINES];
static int define_count = 0;

/* Track included files to avoid multiple includes */
typedef struct IncludedFile {
    char *path;
    struct IncludedFile *next;
} IncludedFile;

static IncludedFile *included_files = NULL;

/* Check if file was already included */
static bool was_included(const char *path) {
    for (IncludedFile *f = included_files; f; f = f->next) {
        if (strcmp(f->path, path) == 0) {
            return true;
        }
    }
    return false;
}

/* Mark file as included */
static void mark_included(const char *path) {
    IncludedFile *f = calloc(1, sizeof(IncludedFile));
    f->path = strdup_custom(path);
    f->next = included_files;
    included_files = f;
}

/* Initialize include paths */
static void init_include_paths(void) {
    if (include_paths) return;  /* Already initialized */
    
    /* Use include paths from compiler state if available */
    if (compiler_state && compiler_state->include_paths) {
        include_paths = compiler_state->include_paths;
        include_count = compiler_state->include_count;
    } else {
        /* Fallback to default paths */
        include_paths = calloc(10, sizeof(char*));
        include_paths[0] = ".";
        include_paths[1] = "/usr/include";
        include_paths[2] = "/usr/local/include";
        include_count = 3;
    }
}

/* Find include file */
static char *find_include_file(char *filename) {
    /* Try absolute path */
    FILE *fp = fopen(filename, "r");
    if (fp) {
        fclose(fp);
        return strdup_custom(filename);
    }
    
    /* Try include paths */
    for (int i = 0; i < include_count; i++) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", include_paths[i], filename);
        fp = fopen(path, "r");
        if (fp) {
            fclose(fp);
            return strdup_custom(path);
        }
    }
    
    return NULL;
}

/* Get macro value */
static char *get_define(const char *name) {
    for (int i = 0; i < define_count; i++) {
        if (strcmp(defines[i].name, name) == 0) {
            return defines[i].value;
        }
    }
    return NULL;
}

/* Expand macros in a line */
static char *expand_macros(char *line) {
    static char expanded[4096];
    char *out = expanded;
    char *p = line;
    
    while (*p) {
        /* Check if this could be an identifier */
        if (isalpha(*p) || *p == '_') {
            char *id_start = p;
            while (isalnum(*p) || *p == '_') p++;
            
            int id_len = p - id_start;
            char id[256];
            if (id_len < 256) {
                strncpy(id, id_start, id_len);
                id[id_len] = '\0';
                
                /* Check if it's a defined macro */
                char *value = get_define(id);
                if (value) {
                    /* Replace with macro value */
                    strcpy(out, value);
                    out += strlen(value);
                } else {
                    /* Copy identifier as is */
                    strncpy(out, id_start, id_len);
                    out += id_len;
                }
            } else {
                /* Identifier too long, copy as is */
                strncpy(out, id_start, id_len);
                out += id_len;
            }
        } else {
            /* Copy character as is */
            *out++ = *p++;
        }
    }
    
    *out = '\0';
    return strdup_custom(expanded);
}

/* Check if macro is defined */
static bool is_defined(const char *name) {
    for (int i = 0; i < define_count; i++) {
        if (strcmp(defines[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

/* Add macro definition */
static void add_define(const char *name, const char *value) {
    if (define_count >= MAX_DEFINES) return;
    
    /* Check if already defined */
    for (int i = 0; i < define_count; i++) {
        if (strcmp(defines[i].name, name) == 0) {
            /* Redefine */
            if (defines[i].value) free(defines[i].value);
            if (value) {
                defines[i].value = strdup_custom(value);
            } else {
                defines[i].value = NULL;
            }
            return;
        }
    }
    
    /* Add new define */
    defines[define_count].name = strdup_custom(name);
    if (value) {
        defines[define_count].value = strdup_custom(value);
    } else {
        defines[define_count].value = NULL;
    }
    define_count++;
}

/* Recursively preprocess text */
static char *preprocess_recursive(char *input, int *out_len);

/* Process #include directive */
static void process_include(char *line, char *output, int *out_len) {
    /* Skip #include */
    char *p = line + 8;
    while (isspace(*p)) p++;
    
    char *filename;
    bool is_system_header = false;
    
    if (*p == '"') {
        /* Local include */
        p++;
        char *end = strchr(p, '"');
        if (!end) {
            fprintf(stderr, "\033[1m\033[33mwarning:\033[0m unclosed include directive\n");
            return;
        }
        filename = strndup_custom(p, end - p);
    } else if (*p == '<') {
        /* System include */
        is_system_header = true;
        p++;
        char *end = strchr(p, '>');
        if (!end) {
            fprintf(stderr, "\033[1m\033[33mwarning:\033[0m unclosed include directive\n");
            return;
        }
        filename = strndup_custom(p, end - p);
    } else {
        /* Skip malformed include */
        fprintf(stderr, "\033[1m\033[33mwarning:\033[0m malformed #include directive\n");
        return;
    }
    
    /* Skip system headers - we don't need to parse them */
    if (is_system_header) {
        /* For certain headers, output necessary typedefs and function declarations */
        if (strcmp(filename, "stdio.h") == 0 || strstr(filename, "stdio.h")) {
            const char *stdio_defs = "\ntypedef int FILE;\nextern int stderr;\nextern int stdout;\nextern int stdin;\nint printf(char *fmt, ...);\nint fprintf(int stream, char *fmt, ...);\nint sprintf(char *str, char *fmt, ...);\nint snprintf(char *str, int size, char *fmt, ...);\nint vfprintf(int stream, char *fmt, int ap);\nint fopen(char *filename, char *mode);\nint fclose(int stream);\nint fread(int ptr, int size, int nmemb, int stream);\nint fwrite(int ptr, int size, int nmemb, int stream);\nint fseek(int stream, int offset, int whence);\nint ftell(int stream);\nint feof(int stream);\nint ferror(int stream);\nint putchar(int c);\nint puts(char *s);\n";
            int len = strlen(stdio_defs);
            memcpy(output + *out_len, stdio_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
            /* Add NULL define for convenience */
            add_define("NULL", "((void*)0)");
            /* Add SEEK_* defines */
            add_define("SEEK_SET", "0");
            add_define("SEEK_CUR", "1");
            add_define("SEEK_END", "2");
        } else if (strcmp(filename, "stdlib.h") == 0 || strstr(filename, "stdlib.h")) {
            const char *stdlib_defs = "\nint malloc(int size);\nint calloc(int nmemb, int size);\nint realloc(int ptr, int size);\nvoid free(int ptr);\nvoid exit(int status);\nint atoi(char *str);\nint system(char *command);\n";
            int len = strlen(stdlib_defs);
            memcpy(output + *out_len, stdlib_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
        } else if (strcmp(filename, "string.h") == 0 || strstr(filename, "string.h")) {
            const char *string_defs = "\nint strlen(char *s);\nint strcmp(char *s1, char *s2);\nint strncmp(char *s1, char *s2, int n);\nchar *strcpy(char *dest, char *src);\nchar *strncpy(char *dest, char *src, int n);\nchar *strcat(char *dest, char *src);\nchar *strchr(char *s, int c);\nchar *strstr(char *haystack, char *needle);\nchar *strdup(char *s);\nint memcpy(int dest, int src, int n);\nint memset(int s, int c, int n);\nint memcmp(int s1, int s2, int n);\n";
            int len = strlen(string_defs);
            memcpy(output + *out_len, string_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
        } else if (strcmp(filename, "ctype.h") == 0 || strstr(filename, "ctype.h")) {
            const char *ctype_defs = "\nint isspace(int c);\nint isalpha(int c);\nint isalnum(int c);\nint isdigit(int c);\nint isupper(int c);\nint islower(int c);\nint toupper(int c);\nint tolower(int c);\n";
            int len = strlen(ctype_defs);
            memcpy(output + *out_len, ctype_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
        } else if (strcmp(filename, "stdarg.h") == 0 || strstr(filename, "stdarg.h")) {
            const char *stdarg_defs = "\ntypedef int va_list;\n";
            int len = strlen(stdarg_defs);
            memcpy(output + *out_len, stdarg_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
            /* Note: va_start, va_arg, va_end are compiler built-ins */
        } else if (strcmp(filename, "errno.h") == 0 || strstr(filename, "errno.h")) {
            const char *errno_defs = "\nextern int errno;\n";
            int len = strlen(errno_defs);
            memcpy(output + *out_len, errno_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
        } else if (strcmp(filename, "unistd.h") == 0 || strstr(filename, "unistd.h")) {
            const char *unistd_defs = "\nint unlink(char *pathname);\n";
            int len = strlen(unistd_defs);
            memcpy(output + *out_len, unistd_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
        } else if (strcmp(filename, "stdbool.h") == 0 || strstr(filename, "stdbool.h")) {
            const char *bool_defs = "\ntypedef int bool;\n";
            int len = strlen(bool_defs);
            memcpy(output + *out_len, bool_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
            /* Add defines for true and false */
            add_define("true", "1");
            add_define("false", "0");
        } else if (strcmp(filename, "stddef.h") == 0 || strstr(filename, "stddef.h")) {
            const char *stddef_defs = "\ntypedef int size_t;\ntypedef int ptrdiff_t;\n";
            int len = strlen(stddef_defs);
            memcpy(output + *out_len, stddef_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
            /* Add define for NULL */
            add_define("NULL", "((void*)0)");
        } else if (strcmp(filename, "stdint.h") == 0 || strstr(filename, "stdint.h")) {
            const char *stdint_defs = "\ntypedef int int32_t;\ntypedef int uint32_t;\ntypedef int int64_t;\ntypedef int uint64_t;\n";
            int len = strlen(stdint_defs);
            memcpy(output + *out_len, stdint_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
        }
        
        free(filename);
        return;
    }
    
    /* Check include depth */
    if (include_depth >= MAX_INCLUDE_DEPTH) {
        fprintf(stderr, "\033[1m\033[33mwarning:\033[0m #include nested too deeply, skipping %s\n", filename);
        free(filename);
        return;
    }
    
    char *path = find_include_file(filename);
    
    if (!path) {
        fprintf(stderr, "\033[1m\033[33mwarning:\033[0m cannot find include file: %s\n", filename);
        free(filename);
        return;
    }
    
    free(filename);
    
    /* Check if already included */
    if (was_included(path)) {
        free(path);
        return;
    }
    
    /* Mark as included */
    mark_included(path);
    
    /* Read and recursively preprocess file contents */
    char *contents = read_file(path);
    free(path);
    
    include_depth++;
    int sub_len = 0;
    char *processed = preprocess_recursive(contents, &sub_len);
    include_depth--;
    
    /* Append to output */
    memcpy(output + *out_len, processed, sub_len);
    *out_len += sub_len;
    output[*out_len] = '\0';
    
    free(contents);
    free(processed);
}

/* Process #define directive */
static void process_define(char *line) {
    /* Skip #define */
    char *p = line + 7;
    while (isspace(*p)) p++;
    
    /* Get macro name */
    char *name_start = p;
    while (isalnum(*p) || *p == '_') p++;
    
    if (p == name_start) return;  /* No name */
    
    char *name = strndup_custom(name_start, p - name_start);
    
    /* Skip whitespace */
    while (isspace(*p)) p++;
    
    /* Get value (rest of line, trimmed) */
    char *value_start = p;
    char *value_end = p;
    while (*value_end && *value_end != '\n') {
        if (!isspace(*value_end)) {
            p = value_end + 1;
        }
        value_end++;
    }
    
    char *value = NULL;
    if (p > value_start) {
        value = strndup_custom(value_start, p - value_start);
    }
    
    add_define(name, value);
    
    free(name);
    if (value) free(value);
}

/* Recursively preprocess text */
static char *preprocess_recursive(char *input, int *out_len) {
    char *output = calloc(1, 1024 * 1024); /* 1MB buffer */
    *out_len = 0;
    
    char *line = input;
    int if_depth = 0;
    int skip_depth = -1;  /* -1 means not skipping, >= 0 means skipping */
    
    while (*line) {
        /* Find line end */
        char *line_end = line;
        while (*line_end && *line_end != '\n') {
            line_end++;
        }
        
        /* Check for preprocessor directives (may be indented) */
        char *directive_start = line;
        while (isspace(*directive_start) && *directive_start != '\n') directive_start++;  /* Skip leading whitespace */
        
        if (*directive_start == '#') {
            char *p = directive_start + 1;
            while (isspace(*p)) p++;
            
            if (strncmp(p, "include", 7) == 0 && isspace(p[7])) {
                if (skip_depth < 0) {
                    process_include(directive_start, output, out_len);
                }
            } else if (strncmp(p, "define", 6) == 0 && (isspace(p[6]) || p[6] == '\0' || p[6] == '\n')) {
                if (skip_depth < 0) {
                    process_define(directive_start);
                }
            } else if (strncmp(p, "ifndef", 6) == 0 || strncmp(p, "ifdef", 5) == 0) {
                /* Parse condition */
                bool is_ifndef = (strncmp(p, "ifndef", 6) == 0);
                if (is_ifndef) {
                    p += 6;
                } else {
                    p += 5;
                }
                while (isspace(*p)) p++;
                
                char *name_start = p;
                while (isalnum(*p) || *p == '_') p++;
                char *name = strndup_custom(name_start, p - name_start);
                
                bool defined = is_defined(name);
                bool condition;
                if (is_ifndef) {
                    condition = !defined;
                } else {
                    condition = defined;
                }
                
                if (skip_depth < 0 && !condition) {
                    skip_depth = if_depth;
                }
                if_depth++;
                
                free(name);
            } else if (strncmp(p, "else", 4) == 0) {
                if (if_depth > 0) {
                    if (skip_depth == if_depth - 1) {
                        skip_depth = -1;  /* Start including */
                    } else if (skip_depth < 0) {
                        skip_depth = if_depth - 1;  /* Start skipping */
                    }
                }
            } else if (strncmp(p, "endif", 5) == 0) {
                if (if_depth > 0) {
                    if_depth--;
                    if (skip_depth == if_depth) {
                        skip_depth = -1;
                    }
                }
            } else if (strncmp(p, "undef", 5) == 0) {
                /* Skip #undef */
            } else if (strncmp(p, "pragma", 6) == 0) {
                /* Skip #pragma */
            } else if (strncmp(p, "error", 5) == 0) {
                /* Skip #error */
            } else if (strncmp(p, "warning", 7) == 0) {
                /* Skip #warning */
            } else if (strncmp(p, "line", 4) == 0) {
                /* Skip #line */
            }
            /* All preprocessor directives are now handled - they don't get copied to output */
        } else if (skip_depth < 0) {
            /* Expand macros and copy line to output if not skipping */
            int line_len = line_end - line;
            if (line_len > 0) {
                /* Create a null-terminated copy of the line */
                char *line_copy = strndup_custom(line, line_len);
                char *expanded = expand_macros(line_copy);
                int expanded_len = strlen(expanded);
                
                memcpy(output + *out_len, expanded, expanded_len);
                *out_len += expanded_len;
                
                free(line_copy);
                free(expanded);
            }
            
            if (*line_end == '\n') {
                output[*out_len] = '\n';
                (*out_len)++;
            }
        }
        
        /* Move to next line */
        line = line_end;
        if (*line == '\n') {
            line++;
        }
    }
    
    output[*out_len] = '\0';
    return output;
}

/* Preprocess file */
char *preprocess(char *filename) {
    init_include_paths();
    
    char *input = read_file(filename);
    int out_len = 0;
    char *output = preprocess_recursive(input, &out_len);
    
    free(input);
    return output;
}
