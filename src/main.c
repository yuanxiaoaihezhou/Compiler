#include "compiler.h"
#include "pipeline.h"

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
    
    /* Create pipeline context */
    PipelineContext *ctx = create_pipeline_context();
    
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
    
    /* Setup pipeline context */
    ctx->input_file = input_file;
    ctx->asm_only = asm_only;
    ctx->compile_only = compile_only;
    
    /* Setup include paths */
    ctx->include_paths = malloc(sizeof(char*) * (include_dir_count + 3));
    ctx->include_count = 0;
    
    /* Add specified include directories */
    for (int i = 0; i < include_dir_count; i++) {
        ctx->include_paths[ctx->include_count++] = include_dirs[i];
    }
    
    /* Add default include paths */
    ctx->include_paths[ctx->include_count++] = ".";
    ctx->include_paths[ctx->include_count++] = "/usr/include";
    ctx->include_paths[ctx->include_count++] = "/usr/local/include";
    
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
    ctx->output_file = output_file;
    
    /* Run the compiler pipeline
     * This executes all compilation stages in strict order:
     * 1. Preprocessing
     * 2. Lexical Analysis
     * 3. Syntax Analysis
     * 4. Semantic Analysis
     * 5. IR Generation
     * 6. Optimization
     * 7. Code Generation
     * 8. Assembly and Linking
     */
    int result = run_compiler_pipeline(ctx);
    
    /* Cleanup */
    free_pipeline_context(ctx);
    
    return result;
}
