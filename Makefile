# C Subset Compiler Makefile
# Target platform: x86_64 Ubuntu 22

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2
LDFLAGS = 

# Source directories
SRC_DIR = src
BUILD_DIR = build
TEST_DIR = tests
DOC_DIR = docs

# Compiler source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/lexer.c \
       $(SRC_DIR)/parser.c \
       $(SRC_DIR)/ast.c \
       $(SRC_DIR)/ir.c \
       $(SRC_DIR)/optimizer.c \
       $(SRC_DIR)/codegen.c \
       $(SRC_DIR)/preprocessor.c \
       $(SRC_DIR)/utils.c \
       $(SRC_DIR)/error.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
COMPILER = $(BUILD_DIR)/mycc

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)

.PHONY: all clean test doc bootstrap install

all: $(COMPILER)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(COMPILER): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(COMPILER) $(LDFLAGS)

# Run tests
test: $(COMPILER)
	@echo "Running test suite..."
	@bash $(TEST_DIR)/run_tests.sh

# Generate documentation
doc:
	@echo "Generating documentation..."
	@mkdir -p $(DOC_DIR)

# Bootstrap: compile compiler with itself
bootstrap: $(COMPILER)
	@echo "Stage 1: Building compiler with GCC..."
	@mv $(COMPILER) $(BUILD_DIR)/mycc-stage1
	@echo "Stage 2: Building compiler with stage1 compiler..."
	@$(BUILD_DIR)/mycc-stage1 -o $(BUILD_DIR)/mycc-stage2 $(SRCS)
	@echo "Stage 3: Building compiler with stage2 compiler..."
	@$(BUILD_DIR)/mycc-stage2 -o $(BUILD_DIR)/mycc-stage3 $(SRCS)
	@echo "Verifying stage2 and stage3 are identical..."
	@cmp $(BUILD_DIR)/mycc-stage2 $(BUILD_DIR)/mycc-stage3
	@echo "Bootstrap successful!"
	@cp $(BUILD_DIR)/mycc-stage3 $(COMPILER)

# Install compiler
install: $(COMPILER)
	install -m 755 $(COMPILER) /usr/local/bin/

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TEST_DIR)/*.s $(TEST_DIR)/*.o $(TEST_DIR)/test_*_out

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all        - Build the compiler (default)"
	@echo "  test       - Run test suite"
	@echo "  doc        - Generate documentation"
	@echo "  bootstrap  - Test self-hosting capability"
	@echo "  install    - Install compiler to /usr/local/bin"
	@echo "  clean      - Remove build artifacts"
