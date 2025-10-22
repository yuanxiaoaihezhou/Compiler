#include "compiler.h"

static char **include_paths;
static int include_count;

/* Initialize include paths */
static void init_include_paths(void) {
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
        return filename;
    }
    
    /* Try include paths */
    for (int i = 0; i < include_count; i++) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", include_paths[i], filename);
        fp = fopen(path, "r");
        if (fp) {
            fclose(fp);
            return strdup(path);
        }
    }
    
    return NULL;
}

/* Process #include directive */
static char *process_include(char *line, char *output, int *out_len) {
    /* Skip #include */
    char *p = line + 8;
    while (isspace(*p)) p++;
    
    char *filename;
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
        p++;
        char *end = strchr(p, '>');
        if (!end) {
            error("unclosed include directive");
        }
        filename = strndup_custom(p, end - p);
    } else {
        error("invalid include directive");
        return NULL;
    }
    
    char *path = find_include_file(filename);
    if (!path) {
        /* For system headers that we don't have, just skip */
        return output;
    }
    
    /* Read and append file contents */
    char *contents = read_file(path);
    int len = strlen(contents);
    memcpy(output + *out_len, contents, len);
    *out_len += len;
    output[*out_len] = '\0';
    
    free(contents);
    return output;
}

/* Preprocess file */
char *preprocess(char *filename) {
    init_include_paths();
    
    char *input = read_file(filename);
    char *output = calloc(1, 1024 * 1024); /* 1MB buffer */
    int out_len = 0;
    
    char *line = input;
    while (*line) {
        /* Find line end */
        char *line_end = line;
        while (*line_end && *line_end != '\n') {
            line_end++;
        }
        
        /* Check for preprocessor directives */
        if (*line == '#') {
            if (strncmp(line, "#include", 8) == 0) {
                process_include(line, output, &out_len);
            }
            /* Skip other preprocessor directives */
        } else {
            /* Copy line to output */
            int line_len = line_end - line + 1;
            memcpy(output + out_len, line, line_len);
            out_len += line_len;
        }
        
        /* Move to next line */
        line = line_end;
        if (*line == '\n') {
            line++;
        }
    }
    
    output[out_len] = '\0';
    return output;
}
