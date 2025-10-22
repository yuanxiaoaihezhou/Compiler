# Compiler

一个用C语言实现的C子集编译器，能够自举（self-hosting）。该编译器实现了足够编译自身的C语言子集功能。

## 项目概述

本项目是一个完整的编译器实现，包含以下编译阶段：
1. 预处理（#include支持）
2. 词法分析
3. 语法分析
4. 中间代码生成
5. 优化
6. 目标代码生成（x86_64汇编）

## 环境要求

- 操作系统：Ubuntu 22.04 (x86_64)
- 编译器：GCC (用于引导编译)
- Make工具

## 编译安装

```bash
# 克隆仓库
git clone https://github.com/yuanxiaoaihezhou/Compiler.git
cd Compiler

# 编译
make

# 运行测试
make test

# 自举测试（基础）
make bootstrap

# 自举测试（多阶段）
make bootstrap-stage1    # 尝试用编译器编译自己（阶段1）
make bootstrap-stage2    # 用阶段1编译器编译自己（阶段2）
make bootstrap-full      # 完整的3阶段自举验证
make bootstrap-test      # 用自举编译器运行测试套件

# 安装（可选）
sudo make install
```

## 使用方法

```bash
# 编译C源文件
./build/mycc source.c -o output

# 仅生成汇编代码
./build/mycc source.c -S -o output.s

# 仅编译不链接
./build/mycc source.c -c -o output.o
```

## 支持的C语言特性

### 数据类型
- `int`, `char`, `void`
- 指针类型
- 数组
- 结构体（基本支持）
- 枚举类型（enum）
- 类型别名（typedef）

### 运算符
- 算术运算：`+`, `-`, `*`, `/`, `%`
- 比较运算：`==`, `!=`, `<`, `<=`, `>`, `>=`
- 逻辑运算：`&&`, `||`, `!`
- 位运算：`&`, `|`, `^`, `<<`, `>>`
- 赋值运算：`=`, `+=`, `-=`
- 其他：`&`（取地址）, `*`（解引用）, `.`, `->`, `? :`

### 控制流
- `if` / `else`
- `while` 循环
- `for` 循环
- `return` 语句
- `break` / `continue`

### 函数
- 函数定义和调用
- 参数传递（最多6个寄存器参数）
- 递归支持

### 其他特性
- 局部变量和全局变量
- 注释（`//` 和 `/* */`）
- 字符串字面量
- 字符字面量
- `sizeof` 运算符
- `#include` 预处理指令
- 多文件编译支持
- 存储类说明符（static, extern）
- 类型限定符（const - 解析支持）

## 测试用例

测试用例位于 `tests/` 目录，包括：
- 基本返回值测试
- 算术运算测试
- 变量声明和赋值测试
- 控制流测试（if/while/for）
- 函数定义和调用测试

测试方法：
```bash
make test
```

测试脚本会用我们的编译器和GCC分别编译测试用例，然后对比运行结果，确保输出一致。

## 自举验证

编译器提供了多个自举测试选项：

```bash
# 基础自举测试 - 验证编译器能编译简单程序
make bootstrap

# 阶段1自举 - 尝试用GCC编译的编译器来编译自己
make bootstrap-stage1

# 阶段2自举 - 用阶段1编译器编译自己
make bootstrap-stage2

# 完整自举 - 3阶段验证，确认阶段1和阶段2生成相同结果
make bootstrap-full

# 自举测试 - 用自举编译器运行所有测试
make bootstrap-test
```

自举过程：
1. 阶段0：使用GCC编译编译器（mycc-stage0）
2. 阶段1：使用mycc-stage0编译自己（mycc-stage1）
3. 阶段2：使用mycc-stage1编译自己（mycc-stage2）
4. 验证：确认mycc-stage1和mycc-stage2生成相同的输出

**当前状态**：基础自举测试通过，已实现typedef/enum/static/extern/const等关键特性。完整自举需要实现switch语句和可变参数函数。详见`docs/SELF_HOSTING.md`。

## 技术文档

详细的技术文档请参阅：[docs/TECHNICAL.md](docs/TECHNICAL.md)

包含以下内容：
- 编译器架构
- 各模块详细说明
- 调用约定
- 内存布局
- 测试方法
- 自举流程

## 项目结构

```
Compiler/
├── Makefile          # 构建系统
├── README.md         # 项目说明
├── docs/             # 文档
│   └── TECHNICAL.md  # 技术文档
├── src/              # 源代码
│   ├── compiler.h    # 主头文件
│   ├── main.c        # 编译器驱动程序
│   ├── lexer.c       # 词法分析器
│   ├── parser.c      # 语法分析器
│   ├── ast.c         # AST操作
│   ├── ir.c          # 中间代码生成
│   ├── optimizer.c   # 优化器
│   ├── codegen.c     # 代码生成器
│   ├── preprocessor.c # 预处理器
│   ├── utils.c       # 工具函数
│   └── error.c       # 错误处理
└── tests/            # 测试套件
    ├── run_tests.sh  # 测试运行脚本
    └── test_*.c      # 测试用例
```

## 特性实现

- [x] 词法分析
- [x] 语法分析
- [x] 中间代码生成
- [x] 代码优化
- [x] x86_64目标代码生成
- [x] 预处理器（#include）
- [x] 多文件支持
- [x] 测试套件
- [x] 技术文档
- [x] 多阶段自举框架（Makefile提供完整的自举选项）
- [x] typedef支持（类型别名）
- [x] enum支持（枚举类型）
- [x] static/extern/const关键字支持（解析）
- [ ] switch/case语句（自举所需）
- [ ] 可变参数函数（自举所需）
- [ ] 完整的自举测试（需要实现switch和variadic functions）

## 已知限制

相比完整的C语言，当前实现有以下限制：
- 不支持浮点数
- 预处理器功能有限（仅支持#include）
- 结构体支持有限
- 不支持联合体
- 不支持函数指针
- 不支持可变参数函数（variadic functions）
- 不支持switch/case语句
- 不支持goto语句
- 标准库支持有限

## 贡献

欢迎提交问题和改进建议！

## 许可证

本项目遵循MIT许可证。
