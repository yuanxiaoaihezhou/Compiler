#include "compiler.h"
#include "pipeline.h"
#include <unistd.h>

/* Create pipeline context */
PipelineContext *create_pipeline_context(void) {
    PipelineContext *ctx = calloc(1, sizeof(PipelineContext));
    if (!ctx) {
        error("Failed to allocate pipeline context");
    }
    return ctx;
}

/* Free pipeline context */
void free_pipeline_context(PipelineContext *ctx) {
    if (!ctx) return;
    /* Note: Most memory is allocated during compilation and not freed
     * until program exit for simplicity. In a production compiler,
     * we would carefully free all allocated memory here. */
    free(ctx);
}

/* Helper to create success result */
StageResult create_success_result(void *data) {
    StageResult result;
    result.success = true;
    result.error_message = NULL;
    result.data = data;
    return result;
}

/* Helper to create error result */
StageResult create_error_result(char *error_message) {
    StageResult result;
    result.success = false;
    result.error_message = error_message;
    result.data = NULL;
    return result;
}

/* Stage 1: Preprocessing */
StageResult pipeline_preprocess(PipelineContext *ctx) {
    if (!ctx->input_file) {
        return create_error_result("No input file specified");
    }
    
    /* Initialize compiler state */
    ctx->state = calloc(1, sizeof(CompilerState));
    ctx->state->current_file = ctx->input_file;
    ctx->state->include_paths = ctx->include_paths;
    ctx->state->include_count = ctx->include_count;
    
    /* Make global for legacy code */
    compiler_state = ctx->state;
    
    /* Run preprocessor */
    ctx->preprocessed_source = preprocess(ctx->input_file);
    if (!ctx->preprocessed_source) {
        return create_error_result("Preprocessing failed");
    }
    
    return create_success_result(ctx->preprocessed_source);
}

/* Stage 2: Lexical Analysis */
StageResult pipeline_lex(PipelineContext *ctx) {
    if (!ctx->preprocessed_source) {
        return create_error_result("No preprocessed source available");
    }
    
    /* Tokenize the preprocessed source */
    ctx->tokens = tokenize(ctx->preprocessed_source, ctx->input_file);
    if (!ctx->tokens) {
        return create_error_result("Tokenization failed");
    }
    
    return create_success_result(ctx->tokens);
}

/* Stage 3: Syntax Analysis */
StageResult pipeline_parse(PipelineContext *ctx) {
    if (!ctx->tokens) {
        return create_error_result("No tokens available");
    }
    
    /* Parse tokens into AST */
    ctx->ast = parse(ctx->tokens);
    if (!ctx->ast) {
        return create_error_result("Parsing failed");
    }
    
    return create_success_result(ctx->ast);
}

/* Stage 4: Semantic Analysis */
StageResult pipeline_semantic_analysis(PipelineContext *ctx) {
    if (!ctx->ast) {
        return create_error_result("No AST available");
    }
    
    /* Add type information to AST nodes */
    for (Symbol *fn = ctx->ast; fn; fn = fn->next) {
        if (fn->is_function && fn->body) {
            add_type(fn->body);
        }
    }
    
    return create_success_result(ctx->ast);
}

/* Stage 5: IR Generation */
StageResult pipeline_generate_ir(PipelineContext *ctx) {
    if (!ctx->ast) {
        return create_error_result("No AST available");
    }
    
    /* Generate intermediate representation */
    ctx->ir_code = gen_ir(ctx->ast);
    if (!ctx->ir_code) {
        return create_error_result("IR generation failed");
    }
    
    return create_success_result(ctx->ir_code);
}

/* Stage 6: Optimization */
StageResult pipeline_optimize(PipelineContext *ctx) {
    if (!ctx->ir_code) {
        return create_error_result("No IR code available");
    }
    
    /* Optimize the intermediate representation */
    ctx->optimized_ir = optimize(ctx->ir_code);
    if (!ctx->optimized_ir) {
        /* If optimization returns NULL, keep original IR */
        ctx->optimized_ir = ctx->ir_code;
    }
    
    return create_success_result(ctx->optimized_ir);
}

/* Stage 7: Code Generation */
StageResult pipeline_codegen(PipelineContext *ctx) {
    if (!ctx->ast) {
        return create_error_result("No AST available for code generation");
    }
    
    /* Determine assembly file name */
    char *asm_file;
    if (ctx->asm_only) {
        asm_file = ctx->output_file;
    } else {
        asm_file = "/tmp/mycc_tmp.s";
    }
    
    /* Open output file */
    FILE *out = fopen(asm_file, "w");
    if (!out) {
        return create_error_result("Cannot open assembly output file");
    }
    
    /* Generate assembly code
     * Note: Currently generates from AST. In future, this should
     * generate from optimized IR for better architecture. */
    codegen(ctx->ast, out);
    fclose(out);
    
    /* Store assembly file path for next stage */
    ctx->assembly_code = asm_file;
    
    return create_success_result((void*)asm_file);
}

/* Stage 8: Assemble and Link */
StageResult pipeline_assemble_link(PipelineContext *ctx) {
    if (!ctx->assembly_code) {
        return create_error_result("No assembly code available");
    }
    
    /* If assembly only mode, we're done */
    if (ctx->asm_only) {
        return create_success_result(NULL);
    }
    
    /* Build command for gcc assembler/linker */
    char cmd[1024];
    if (ctx->compile_only) {
        snprintf(cmd, sizeof(cmd), "gcc -c %s -o %s", 
                 ctx->assembly_code, ctx->output_file);
    } else {
        snprintf(cmd, sizeof(cmd), "gcc %s -o %s", 
                 ctx->assembly_code, ctx->output_file);
    }
    
    /* Execute assembler/linker */
    if (system(cmd) != 0) {
        return create_error_result("Assembly/linking failed");
    }
    
    /* Remove temporary assembly file */
    if (!ctx->asm_only) {
        unlink(ctx->assembly_code);
    }
    
    return create_success_result(NULL);
}

/* Main pipeline execution */
int run_compiler_pipeline(PipelineContext *ctx) {
    StageResult result;
    
    /* Stage 1: Preprocessing */
    result = pipeline_preprocess(ctx);
    if (!result.success) {
        error("Preprocessing stage failed: %s", result.error_message);
        return 1;
    }
    
    /* Stage 2: Lexical Analysis */
    result = pipeline_lex(ctx);
    if (!result.success) {
        error("Lexical analysis stage failed: %s", result.error_message);
        return 1;
    }
    
    /* Stage 3: Syntax Analysis */
    result = pipeline_parse(ctx);
    if (!result.success) {
        error("Syntax analysis stage failed: %s", result.error_message);
        return 1;
    }
    
    /* Stage 4: Semantic Analysis */
    result = pipeline_semantic_analysis(ctx);
    if (!result.success) {
        error("Semantic analysis stage failed: %s", result.error_message);
        return 1;
    }
    
    /* Stage 5: IR Generation */
    result = pipeline_generate_ir(ctx);
    if (!result.success) {
        error("IR generation stage failed: %s", result.error_message);
        return 1;
    }
    
    /* Stage 6: Optimization */
    result = pipeline_optimize(ctx);
    if (!result.success) {
        error("Optimization stage failed: %s", result.error_message);
        return 1;
    }
    
    /* Stage 7: Code Generation */
    result = pipeline_codegen(ctx);
    if (!result.success) {
        error("Code generation stage failed: %s", result.error_message);
        return 1;
    }
    
    /* Stage 8: Assemble and Link */
    result = pipeline_assemble_link(ctx);
    if (!result.success) {
        error("Assembly/linking stage failed: %s", result.error_message);
        return 1;
    }
    
    return 0;
}
