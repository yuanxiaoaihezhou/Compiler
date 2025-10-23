#!/bin/bash
# Script to create a single-file version of the compiler for self-hosting

OUTPUT="build/mycc_combined.c"

# Create output file with all necessary includes
cat > "$OUTPUT" << 'EOF'
/* Combined compiler source for self-hosting */

/* Standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>

EOF

# Add the header content (without #includes)
echo "/* ========== compiler.h ========== */" >> "$OUTPUT"
grep -v "^#include" src/compiler.h | grep -v "^#ifndef COMPILER_H" | grep -v "^#define COMPILER_H" | grep -v "^#endif" >> "$OUTPUT"
echo "" >> "$OUTPUT"

# Add each C file (without #includes)
for file in src/runtime.c src/utils.c src/error.c src/ast.c src/lexer.c src/parser.c src/ir.c src/optimizer.c src/codegen.c src/preprocessor.c src/main.c; do
    echo "/* ========== $file ========== */" >> "$OUTPUT"
    grep -v "^#include" "$file" >> "$OUTPUT"
    echo "" >> "$OUTPUT"
done

echo "Combined source created at $OUTPUT"
echo "Lines: $(wc -l < $OUTPUT)"
