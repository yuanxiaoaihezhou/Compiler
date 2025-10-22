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
    include_paths = calloc(10, sizeof(char*));
    include_paths[0] = ".";
    include_paths[1] = "/usr/include";
    include_paths[2] = "/usr/local/include";
    include_count = 3;
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
            defines[i].value = value ? strdup_custom(value) : NULL;
            return;
        }
    }
    
    /* Add new define */
    defines[define_count].name = strdup_custom(name);
    defines[define_count].value = value ? strdup_custom(value) : NULL;
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
            error("unclosed include directive");
        }
        filename = strndup_custom(p, end - p);
    } else if (*p == '<') {
        /* System include */
        is_system_header = true;
        p++;
        char *end = strchr(p, '>');
        if (!end) {
            error("unclosed include directive");
        }
        filename = strndup_custom(p, end - p);
    } else {
        /* Skip malformed include */
        return;
    }
    
    /* Skip system headers - we don't need to parse them */
    if (is_system_header) {
        /* For certain headers, output necessary typedefs */
        if (strcmp(filename, "stdbool.h") == 0 || strstr(filename, "stdbool.h")) {
            const char *bool_defs = "\ntypedef int bool;\n";
            int len = strlen(bool_defs);
            memcpy(output + *out_len, bool_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
        } else if (strcmp(filename, "stddef.h") == 0 || strstr(filename, "stddef.h")) {
            const char *stddef_defs = "\ntypedef unsigned long size_t;\ntypedef long ptrdiff_t;\n";
            int len = strlen(stddef_defs);
            memcpy(output + *out_len, stddef_defs, len);
            *out_len += len;
            output[*out_len] = '\0';
        }
        
        free(filename);
        return;
    }
    
    /* Check include depth */
    if (include_depth >= MAX_INCLUDE_DEPTH) {
        /* Too deep - skip */
        free(filename);
        return;
    }
    
    char *path = find_include_file(filename);
    free(filename);
    
    if (!path) {
        /* File not found - skip */
        return;
    }
    
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
    
    char *value = (p > value_start) ? strndup_custom(value_start, p - value_start) : NULL;
    
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
                p += is_ifndef ? 6 : 5;
                while (isspace(*p)) p++;
                
                char *name_start = p;
                while (isalnum(*p) || *p == '_') p++;
                char *name = strndup_custom(name_start, p - name_start);
                
                bool defined = is_defined(name);
                bool condition = is_ifndef ? !defined : defined;
                
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
            }
            /* Skip all other preprocessor directives */
        } else if (skip_depth < 0) {
            /* Copy line to output if not skipping */
            int line_len = line_end - line;
            if (line_len > 0 || *line_end == '\n') {
                memcpy(output + *out_len, line, line_len);
                *out_len += line_len;
                if (*line_end == '\n') {
                    output[*out_len] = '\n';
                    (*out_len)++;
                }
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
