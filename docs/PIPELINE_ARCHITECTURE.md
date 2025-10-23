# Compiler Pipeline Architecture

## Overview

本编译器采用严格的流水线架构，确保代码严格按照编译原理的标准阶段顺序处理。每个阶段都有明确的输入和输出接口，数据流向清晰明确。

The compiler uses a strict pipeline architecture that ensures code is processed in the standard compilation phases order defined by compiler theory. Each stage has clear input and output interfaces with well-defined data flow.

## Pipeline Stages

编译流程包含8个严格顺序执行的阶段：

The compilation process consists of 8 strictly ordered stages:

```
Source Code (.c)
    ↓
[1. Preprocessing] → Preprocessed Source
    ↓
[2. Lexical Analysis] → Token Stream
    ↓
[3. Syntax Analysis] → Abstract Syntax Tree (AST)
    ↓
[4. Semantic Analysis] → Typed AST
    ↓
[5. IR Generation] → Intermediate Representation (IR)
    ↓
[6. Optimization] → Optimized IR
    ↓
[7. Code Generation] → Assembly Code (.s)
    ↓
[8. Assemble & Link] → Executable/Object File
```

### Stage 1: Preprocessing (预处理)

**输入 (Input):** 源代码文件路径 (Source file path)  
**输出 (Output):** 预处理后的源代码字符串 (Preprocessed source string)

**功能 (Functions):**
- 处理 `#include` 指令 (Process #include directives)
- 处理 `#define` 宏定义 (Process #define macros)
- 条件编译 `#ifdef`, `#ifndef` (Conditional compilation)
- 搜索包含文件路径 (Search include paths)

**实现 (Implementation):** `pipeline_preprocess()` in `src/pipeline.c`

### Stage 2: Lexical Analysis (词法分析)

**输入 (Input):** 预处理后的源代码 (Preprocessed source)  
**输出 (Output):** Token流 (Token stream)

**功能 (Functions):**
- 将源代码分解为Token (Break source into tokens)
- 识别关键字、标识符、运算符、字面量 (Recognize keywords, identifiers, operators, literals)
- 跟踪位置信息用于错误报告 (Track location for error reporting)
- 处理注释 (Handle comments)

**实现 (Implementation):** `pipeline_lex()` → `tokenize()` in `src/lexer.c`

**Token类型 (Token Types):**
- 关键字: `int`, `char`, `if`, `while`, `return`, etc.
- 标识符: 变量名、函数名 (variable names, function names)
- 字面量: 数字、字符串、字符 (numbers, strings, characters)
- 运算符: `+`, `-`, `*`, `/`, `==`, `!=`, etc.
- 标点: `(`, `)`, `{`, `}`, `;`, etc.

### Stage 3: Syntax Analysis (语法分析)

**输入 (Input):** Token流 (Token stream)  
**输出 (Output):** 抽象语法树 AST (Abstract Syntax Tree)

**功能 (Functions):**
- 构建抽象语法树 (Build AST)
- 语法检查 (Syntax checking)
- 符号表管理 (Symbol table management)
- 处理运算符优先级和结合性 (Handle operator precedence and associativity)

**实现 (Implementation):** `pipeline_parse()` → `parse()` in `src/parser.c`

**AST节点类型 (AST Node Types):**
- 表达式节点: 二元运算、一元运算、函数调用等
- 语句节点: if、while、for、return等
- 声明节点: 变量声明、函数定义等

### Stage 4: Semantic Analysis (语义分析)

**输入 (Input):** AST  
**输出 (Output):** 类型化的AST (Typed AST)

**功能 (Functions):**
- 类型检查 (Type checking)
- 类型推断 (Type inference)
- 语义验证 (Semantic validation)
- 添加类型信息到AST节点 (Add type information to AST nodes)

**实现 (Implementation):** `pipeline_semantic_analysis()` → `add_type()` in `src/ast.c`

**类型系统 (Type System):**
- 基本类型: `void`, `char`, `int`
- 派生类型: 指针、数组、结构体、函数
- 类型别名: `typedef`
- 枚举类型: `enum`

### Stage 5: IR Generation (中间代码生成)

**输入 (Input):** 类型化的AST (Typed AST)  
**输出 (Output):** 中间表示 IR (Intermediate Representation)

**功能 (Functions):**
- 将AST转换为三地址码 (Convert AST to three-address code)
- 寄存器分配准备 (Prepare for register allocation)
- 控制流图构建 (Build control flow graph)
- 平台无关的中间形式 (Platform-independent intermediate form)

**实现 (Implementation):** `pipeline_generate_ir()` → `gen_ir()` in `src/ir.c`

**IR指令类型 (IR Instruction Types):**
- 算术运算: ADD, SUB, MUL, DIV, MOD
- 逻辑运算: AND, OR, XOR, SHL, SHR
- 内存操作: LOAD, STORE, MOV
- 控制流: LABEL, JMP, JZ, JNZ
- 函数调用: CALL, RET
- 比较: EQ, NE, LT, LE, GT, GE

### Stage 6: Optimization (优化)

**输入 (Input):** IR代码 (IR code)  
**输出 (Output):** 优化后的IR (Optimized IR)

**功能 (Functions):**
- 常量折叠 (Constant folding)
- 死代码消除 (Dead code elimination)
- 公共子表达式消除 (Common subexpression elimination)
- 其他优化技术 (Other optimization techniques)

**实现 (Implementation):** `pipeline_optimize()` → `optimize()` in `src/optimizer.c`

**优化技术 (Optimization Techniques):**
- 常量折叠: 编译期计算常量表达式
- 死代码消除: 移除不可达代码
- 可扩展框架: 易于添加新的优化pass

### Stage 7: Code Generation (目标代码生成)

**输入 (Input):** 优化后的IR (Optimized IR)  
**输出 (Output):** 汇编代码 (Assembly code)

**功能 (Functions):**
- 生成目标平台汇编代码 (Generate target assembly)
- 寄存器分配 (Register allocation)
- 指令选择 (Instruction selection)
- 栈帧管理 (Stack frame management)

**实现 (Implementation):** `pipeline_codegen()` → `codegen()` in `src/codegen.c`

**目标平台 (Target Platform):**
- x86_64架构 (x86_64 architecture)
- System V AMD64 ABI调用约定
- AT&T汇编语法

### Stage 8: Assemble and Link (汇编与链接)

**输入 (Input):** 汇编代码文件 (Assembly file)  
**输出 (Output):** 可执行文件或目标文件 (Executable or object file)

**功能 (Functions):**
- 调用系统汇编器(GCC) (Invoke system assembler)
- 链接目标文件 (Link object files)
- 生成最终可执行文件 (Generate final executable)

**实现 (Implementation):** `pipeline_assemble_link()` in `src/pipeline.c`

## Pipeline Context (流水线上下文)

`PipelineContext` 结构体保存整个编译过程的状态：

The `PipelineContext` structure holds the state throughout compilation:

```c
typedef struct {
    /* Input configuration */
    char *input_file;           // 输入文件
    char *output_file;          // 输出文件
    bool asm_only;              // 仅生成汇编
    bool compile_only;          // 仅编译不链接
    char **include_paths;       // 包含路径
    int include_count;          // 包含路径数量
    
    /* Stage outputs */
    char *preprocessed_source;  // 预处理后的源代码
    Token *tokens;              // Token流
    Symbol *ast;                // 抽象语法树
    IR *ir_code;                // 中间表示
    IR *optimized_ir;           // 优化后的IR
    char *assembly_code;        // 汇编代码
    
    /* Compilation state */
    CompilerState *state;       // 编译器状态
} PipelineContext;
```

## Error Handling (错误处理)

每个阶段都返回 `StageResult` 结构：

Each stage returns a `StageResult` structure:

```c
typedef struct {
    bool success;           // 是否成功
    char *error_message;    // 错误信息
    void *data;            // 阶段输出数据
} StageResult;
```

如果任何阶段失败，流水线立即停止并报告错误。

If any stage fails, the pipeline immediately stops and reports the error.

## Usage Example (使用示例)

```c
// 创建流水线上下文
PipelineContext *ctx = create_pipeline_context();

// 配置输入输出
ctx->input_file = "program.c";
ctx->output_file = "program";
ctx->asm_only = false;
ctx->compile_only = false;

// 设置包含路径
ctx->include_paths = ...;
ctx->include_count = ...;

// 运行完整的编译流水线
int result = run_compiler_pipeline(ctx);

// 清理
free_pipeline_context(ctx);
```

## Architecture Benefits (架构优势)

1. **清晰的阶段分离 (Clear Stage Separation)**
   - 每个阶段职责单一明确
   - 便于理解和维护

2. **严格的数据流 (Strict Data Flow)**
   - 数据按照固定顺序在阶段间传递
   - 避免阶段间的混乱依赖

3. **易于扩展 (Easy to Extend)**
   - 可以在任何阶段添加新功能
   - 优化pass可以独立添加

4. **更好的错误处理 (Better Error Handling)**
   - 每个阶段独立的错误检测
   - 明确的错误报告位置

5. **便于测试 (Easy to Test)**
   - 可以独立测试每个阶段
   - 便于单元测试和集成测试

6. **符合编译原理 (Follows Compiler Theory)**
   - 严格遵循编译器教科书的标准流程
   - 代码结构反映理论模型

## Future Improvements (未来改进)

1. **完整的IR使用 (Complete IR Usage)**
   - 当前代码生成仍从AST生成，未来应从IR生成
   - 更好地利用中间表示的优势

2. **更多优化Pass (More Optimization Passes)**
   - 添加更多优化技术
   - 实现数据流分析

3. **并行编译 (Parallel Compilation)**
   - 多文件并行处理
   - 流水线并行化

4. **更好的错误恢复 (Better Error Recovery)**
   - 错误恢复机制
   - 继续编译报告多个错误

5. **可视化工具 (Visualization Tools)**
   - AST可视化
   - IR可视化
   - 优化效果展示

## References (参考资料)

- Dragon Book: Compilers: Principles, Techniques, and Tools
- Modern Compiler Implementation in C
- Engineering a Compiler

## Summary (总结)

新的流水线架构提供了清晰、严格的编译流程，每个阶段都有明确的职责和接口。这种设计既符合编译原理的标准流程，又为未来的扩展和优化提供了良好的基础。

The new pipeline architecture provides a clear, strict compilation flow where each stage has well-defined responsibilities and interfaces. This design both follows the standard compiler theory process and provides a solid foundation for future extensions and optimizations.
