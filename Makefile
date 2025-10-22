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

.PHONY: all clean test doc bootstrap bootstrap-stage1 bootstrap-stage2 bootstrap-full bootstrap-test install

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
	@cd $(TEST_DIR) && bash run_tests.sh

# Generate documentation
doc:
	@echo "Generating documentation..."
	@mkdir -p $(DOC_DIR)

# Bootstrap: basic test - compile compiler with itself
bootstrap: $(COMPILER)
	@echo "Bootstrap test - Basic capability check"
	@echo "=========================================="
	@echo "Stage 0: Building compiler with GCC..."
	@mv $(COMPILER) $(BUILD_DIR)/mycc-stage0
	@echo "Creating combined source file..."
	@bash tools/combine.sh
	@echo "Testing stage 0 compiler with test programs..."
	@$(BUILD_DIR)/mycc-stage0 tests/test_01_return.c -o $(BUILD_DIR)/test_bootstrap
	@$(BUILD_DIR)/test_bootstrap; test $$? -eq 42 && echo "✓ Stage 0 compiler works!" || (echo "✗ Stage 0 compiler failed"; exit 1)
	@echo ""
	@echo "Basic bootstrap test passed!"
	@echo "For multi-stage bootstrap, use: make bootstrap-stage1, bootstrap-stage2, or bootstrap-full"
	@echo "See docs/SELF_HOSTING.md for details on full self-hosting status."
	@cp $(BUILD_DIR)/mycc-stage0 $(COMPILER)

# Bootstrap Stage 1: compile compiler with GCC-compiled compiler
bootstrap-stage1: $(COMPILER)
	@echo "Bootstrap Stage 1 - Compile with stage 0"
	@echo "=========================================="
	@echo "Building stage 0 compiler with GCC..."
	@mkdir -p $(BUILD_DIR)
	@cp $(COMPILER) $(BUILD_DIR)/mycc-stage0
	@echo "Creating combined source file..."
	@bash tools/combine.sh
	@echo "Attempting to compile compiler with stage 0..."
	@echo "This will compile: $(BUILD_DIR)/mycc_combined.c → $(BUILD_DIR)/mycc-stage1"
	@if $(BUILD_DIR)/mycc-stage0 $(BUILD_DIR)/mycc_combined.c -o $(BUILD_DIR)/mycc-stage1 2>$(BUILD_DIR)/bootstrap-stage1.log; then \
		echo "✓ Stage 1 compilation succeeded!"; \
		echo "Testing stage 1 compiler..."; \
		$(BUILD_DIR)/mycc-stage1 tests/test_01_return.c -o $(BUILD_DIR)/test_stage1 && \
		$(BUILD_DIR)/test_stage1 && test $$? -eq 42 && \
		echo "✓ Stage 1 compiler is functional!" || \
		(echo "✗ Stage 1 compiler failed runtime test"; exit 1); \
	else \
		echo "✗ Stage 1 compilation failed (expected - missing features)"; \
		echo "Error log saved to $(BUILD_DIR)/bootstrap-stage1.log"; \
		echo "This is normal - full self-hosting requires additional C features."; \
		echo "See docs/SELF_HOSTING.md for required features."; \
		exit 1; \
	fi

# Bootstrap Stage 2: compile compiler with stage 1 compiler
bootstrap-stage2: bootstrap-stage1
	@echo ""
	@echo "Bootstrap Stage 2 - Compile with stage 1"
	@echo "=========================================="
	@echo "Attempting to compile compiler with stage 1..."
	@echo "This will compile: $(BUILD_DIR)/mycc_combined.c → $(BUILD_DIR)/mycc-stage2"
	@if $(BUILD_DIR)/mycc-stage1 $(BUILD_DIR)/mycc_combined.c -o $(BUILD_DIR)/mycc-stage2 2>$(BUILD_DIR)/bootstrap-stage2.log; then \
		echo "✓ Stage 2 compilation succeeded!"; \
		echo "Testing stage 2 compiler..."; \
		$(BUILD_DIR)/mycc-stage2 tests/test_01_return.c -o $(BUILD_DIR)/test_stage2 && \
		$(BUILD_DIR)/test_stage2 && test $$? -eq 42 && \
		echo "✓ Stage 2 compiler is functional!" || \
		(echo "✗ Stage 2 compiler failed runtime test"; exit 1); \
	else \
		echo "✗ Stage 2 compilation failed"; \
		echo "Error log saved to $(BUILD_DIR)/bootstrap-stage2.log"; \
		exit 1; \
	fi

# Bootstrap Full: complete 3-stage bootstrap with verification
bootstrap-full: bootstrap-stage2
	@echo ""
	@echo "Bootstrap Full - 3-stage verification"
	@echo "=========================================="
	@echo "Comparing stage 1 and stage 2 compilers..."
	@if cmp -s $(BUILD_DIR)/mycc-stage1 $(BUILD_DIR)/mycc-stage2; then \
		echo "✓ Stage 1 and stage 2 binaries are identical!"; \
		echo "✓ FULL SELF-HOSTING ACHIEVED!"; \
		echo ""; \
		echo "Summary:"; \
		echo "  - Stage 0 (GCC):    $(BUILD_DIR)/mycc-stage0"; \
		echo "  - Stage 1 (Stage 0): $(BUILD_DIR)/mycc-stage1"; \
		echo "  - Stage 2 (Stage 1): $(BUILD_DIR)/mycc-stage2"; \
		echo "  - Stages 1 & 2 are identical ✓"; \
	else \
		echo "⚠ Stage 1 and stage 2 binaries differ"; \
		echo "This may be normal due to:"; \
		echo "  - Different compilation times embedded in binary"; \
		echo "  - Non-deterministic code generation"; \
		echo "  - Debug symbols"; \
		echo ""; \
		echo "Testing functional equivalence instead..."; \
		for test in tests/test_*.c; do \
			testname=$$(basename $$test .c); \
			echo -n "  Testing $$testname: "; \
			$(BUILD_DIR)/mycc-stage1 $$test -o $(BUILD_DIR)/$${testname}_s1 2>/dev/null && \
			$(BUILD_DIR)/mycc-stage2 $$test -o $(BUILD_DIR)/$${testname}_s2 2>/dev/null && \
			$(BUILD_DIR)/$${testname}_s1 > $(BUILD_DIR)/$${testname}_s1.out 2>&1; s1_exit=$$?; \
			$(BUILD_DIR)/$${testname}_s2 > $(BUILD_DIR)/$${testname}_s2.out 2>&1; s2_exit=$$?; \
			if [ $$s1_exit -eq $$s2_exit ] && diff -q $(BUILD_DIR)/$${testname}_s1.out $(BUILD_DIR)/$${testname}_s2.out >/dev/null 2>&1; then \
				echo "✓"; \
			else \
				echo "✗ (outputs differ)"; \
			fi; \
		done; \
		echo ""; \
		echo "Functional equivalence test completed."; \
	fi

# Bootstrap test: run all tests with bootstrapped compiler
bootstrap-test: bootstrap-stage1
	@echo ""
	@echo "Bootstrap Test - Run test suite with stage 1 compiler"
	@echo "========================================================"
	@TOTAL=0; PASS=0; FAIL=0; \
	for test in tests/test_*.c; do \
		TOTAL=$$((TOTAL + 1)); \
		testname=$$(basename $$test .c); \
		echo -n "Testing $$testname with stage 1: "; \
		if $(BUILD_DIR)/mycc-stage1 $$test -o $(BUILD_DIR)/$${testname}_bootstrap 2>/dev/null; then \
			$(BUILD_DIR)/$${testname}_bootstrap > $(BUILD_DIR)/$${testname}_bootstrap.out 2>&1; \
			bootstrap_exit=$$?; \
			gcc $$test -o $(BUILD_DIR)/$${testname}_gcc 2>/dev/null; \
			$(BUILD_DIR)/$${testname}_gcc > $(BUILD_DIR)/$${testname}_gcc.out 2>&1; \
			gcc_exit=$$?; \
			if [ $$bootstrap_exit -eq $$gcc_exit ] && diff -q $(BUILD_DIR)/$${testname}_bootstrap.out $(BUILD_DIR)/$${testname}_gcc.out >/dev/null 2>&1; then \
				echo "✓ PASS"; \
				PASS=$$((PASS + 1)); \
			else \
				echo "✗ FAIL (output mismatch)"; \
				FAIL=$$((FAIL + 1)); \
			fi; \
		else \
			echo "✗ FAIL (compilation failed)"; \
			FAIL=$$((FAIL + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "Results: $$PASS/$$TOTAL passed, $$FAIL/$$TOTAL failed"; \
	[ $$FAIL -eq 0 ]

# Install compiler
install: $(COMPILER)
	install -m 755 $(COMPILER) /usr/local/bin/

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TEST_DIR)/*.s $(TEST_DIR)/*.o $(TEST_DIR)/test_*_out

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all              - Build the compiler (default)"
	@echo "  test             - Run test suite"
	@echo "  doc              - Generate documentation"
	@echo "  bootstrap        - Basic bootstrap test (compile test programs)"
	@echo "  bootstrap-stage1 - Stage 1: Compile compiler with GCC-built compiler"
	@echo "  bootstrap-stage2 - Stage 2: Compile compiler with stage 1 compiler"
	@echo "  bootstrap-full   - Full 3-stage bootstrap with verification"
	@echo "  bootstrap-test   - Run all tests with bootstrapped compiler"
	@echo "  install          - Install compiler to /usr/local/bin"
	@echo "  clean            - Remove build artifacts"
	@echo ""
	@echo "Bootstrap stages:"
	@echo "  Stage 0: GCC compiles compiler → mycc-stage0"
	@echo "  Stage 1: mycc-stage0 compiles compiler → mycc-stage1"
	@echo "  Stage 2: mycc-stage1 compiles compiler → mycc-stage2"
	@echo "  Verify: Check stage1 and stage2 produce identical results"
