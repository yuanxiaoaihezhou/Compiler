#ifndef PIPELINE_H
#define PIPELINE_H

#include "compiler.h"

/*
 * Compiler Pipeline Framework
 * 
 * This framework defines a clear, linear pipeline for the compilation process:
 * 1. Preprocessing
 * 2. Lexical Analysis (Tokenization)
 * 3. Syntax Analysis (Parsing)
 * 4. Semantic Analysis (Type Checking)
 * 5. Intermediate Representation (IR) Generation
 * 6. Optimization
 * 7. Code Generation (Target Assembly)
 * 8. Assembly and Linking
 * 
 * Each stage takes input from the previous stage and produces output
 * for the next stage through well-defined interfaces.
 */

/* Pipeline stage results */
typedef struct {
    bool success;
    char *error_message;
    void *data;  /* Stage-specific data */
} StageResult;

/* Pipeline context - holds all compilation state */
typedef struct {
    /* Input */
    char *input_file;
    char *output_file;
    bool asm_only;
    bool compile_only;
    char **include_paths;
    int include_count;
    
    /* Stage outputs */
    char *preprocessed_source;  /* After preprocessing */
    Token *tokens;              /* After lexical analysis */
    Symbol *ast;                /* After syntax analysis (AST) */
    IR *ir_code;                /* After IR generation */
    IR *optimized_ir;           /* After optimization */
    char *assembly_code;        /* After code generation */
    
    /* Compilation state */
    CompilerState *state;
} PipelineContext;

/* Pipeline stage functions */

/* Stage 1: Preprocessing - Handle #include, #define, etc. */
StageResult pipeline_preprocess(PipelineContext *ctx);

/* Stage 2: Lexical Analysis - Convert source to tokens */
StageResult pipeline_lex(PipelineContext *ctx);

/* Stage 3: Syntax Analysis - Build AST from tokens */
StageResult pipeline_parse(PipelineContext *ctx);

/* Stage 4: Semantic Analysis - Type checking and validation */
StageResult pipeline_semantic_analysis(PipelineContext *ctx);

/* Stage 5: IR Generation - Convert AST to intermediate representation */
StageResult pipeline_generate_ir(PipelineContext *ctx);

/* Stage 6: Optimization - Optimize IR */
StageResult pipeline_optimize(PipelineContext *ctx);

/* Stage 7: Code Generation - Generate assembly from IR */
StageResult pipeline_codegen(PipelineContext *ctx);

/* Stage 8: Assemble and Link - Create executable */
StageResult pipeline_assemble_link(PipelineContext *ctx);

/* Main pipeline execution */
int run_compiler_pipeline(PipelineContext *ctx);

/* Helper functions */
PipelineContext *create_pipeline_context(void);
void free_pipeline_context(PipelineContext *ctx);
StageResult create_success_result(void *data);
StageResult create_error_result(char *error_message);

#endif /* PIPELINE_H */
