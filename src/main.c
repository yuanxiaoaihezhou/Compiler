#include "compiler.h"
#include <unistd.h>

/* Print usage */
static void usage(void) {
    fprintf(stderr, "Usage: mycc [options] file\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o <file>  Write output to <file>\n");
    fprintf(stderr, "  -S         Generate assembly only\n");
    fprintf(stderr, "  -c         Compile only (do not link)\n");
    fprintf(stderr, "  -I <dir>   Add directory to include search path\n");
    fprintf(stderr, "  -h         Display this help\n");
    exit(1);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
    }
    
    char *input_file = NULL;
    char *output_file = NULL;
    bool asm_only = false;
    bool compile_only = false;
    char *include_dirs[10] = {0};
    int include_dir_count = 0;
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                error("missing output file");
            }
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-S") == 0) {
            asm_only = true;
        } else if (strcmp(argv[i], "-c") == 0) {
            compile_only = true;
        } else if (strcmp(argv[i], "-I") == 0) {
            if (i + 1 >= argc) {
                error("missing include directory");
            }
            if (include_dir_count < 10) {
                include_dirs[include_dir_count++] = argv[++i];
            }
        } else if (strcmp(argv[i], "-h") == 0) {
            usage();
        } else if (argv[i][0] == '-') {
            error("unknown option: %s", argv[i]);
        } else {
            input_file = argv[i];
        }
    }
    
    if (!input_file) {
        error("no input file");
    }
    
    /* Initialize compiler state */
    compiler_state = calloc(1, sizeof(CompilerState));
    compiler_state->current_file = input_file;
    compiler_state->include_paths = malloc(sizeof(char*) * (include_dir_count + 3));
    compiler_state->include_count = 0;
    
    /* Add specified include directories */
    for (int i = 0; i < include_dir_count; i++) {
        compiler_state->include_paths[compiler_state->include_count++] = include_dirs[i];
    }
    
    /* Add default include paths */
    compiler_state->include_paths[compiler_state->include_count++] = ".";
    compiler_state->include_paths[compiler_state->include_count++] = "/usr/include";
    compiler_state->include_paths[compiler_state->include_count++] = "/usr/local/include";
    
    /* Preprocess */
    char *preprocessed = preprocess(input_file);
    
    /* Tokenize */
    Token *tok = tokenize(preprocessed, input_file);
    
    /* Parse */
    Symbol *prog = parse(tok);
    
    /* Add type information */
    for (Symbol *fn = prog; fn; fn = fn->next) {
        if (fn->is_function && fn->body) {
            add_type(fn->body);
        }
    }
    
    /* Generate IR */
    IR *ir = gen_ir(prog);
    
    /* Optimize */
    ir = optimize(ir);
    
    /* Determine output file name */
    if (!output_file) {
        if (asm_only) {
            /* Replace .c with .s */
            int len = strlen(input_file);
            output_file = malloc(len + 2);
            strcpy(output_file, input_file);
            if (len > 2 && strcmp(output_file + len - 2, ".c") == 0) {
                strcpy(output_file + len - 2, ".s");
            } else {
                strcat(output_file, ".s");
            }
        } else {
            output_file = "a.out";
        }
    }
    
    /* Generate assembly */
    char *asm_file;
    if (asm_only) {
        asm_file = output_file;
    } else {
        asm_file = "/tmp/mycc_tmp.s";
    }
    
    FILE *out = fopen(asm_file, "w");
    if (!out) {
        error("cannot open output file: %s", asm_file);
    }
    
    codegen(prog, out);
    fclose(out);
    
    /* Assemble and link if needed */
    if (!asm_only) {
        char cmd[1024];
        if (compile_only) {
            snprintf(cmd, sizeof(cmd), "gcc -c %s -o %s", asm_file, output_file);
        } else {
            snprintf(cmd, sizeof(cmd), "gcc %s -o %s", asm_file, output_file);
        }
        
        if (system(cmd) != 0) {
            error("assembly/linking failed");
        }
        
        /* Remove temporary assembly file */
        if (!asm_only) {
            unlink(asm_file);
        }
    }
    
    return 0;
}
