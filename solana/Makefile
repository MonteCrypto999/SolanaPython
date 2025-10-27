# PikaPython Solana Build System
#
# Usage:
#   make              - Build pika_python.so
#   make compiler     - Build native bytecode compiler
#   make deploy       - Build and deploy to local validator
#   make test         - Run tests
#   make clean        - Remove build artifacts
#   make help         - Show this help

.PHONY: all build compiler deploy test clean help keypair

# Directories
ROOT_DIR := $(shell pwd)
PIKA_ROOT := $(ROOT_DIR)/..
PORT_DIR := $(PIKA_ROOT)/port/solana_sbf
SDK_DIR := $(PORT_DIR)/solana_sdk
BUILD_DIR := $(ROOT_DIR)/build
TOOLS_DIR := $(ROOT_DIR)/tools

# Output
PROGRAM := pika_python
OUTPUT := $(BUILD_DIR)/$(PROGRAM).so
KEYPAIR := $(ROOT_DIR)/pika-keypair.json
COMPILER := $(TOOLS_DIR)/pika_compile

# Auto-detect Solana toolchain
SOLANA_BIN := $(shell dirname "$$(command -v solana 2>/dev/null || echo "$$HOME/.local/share/solana/install/active_release/bin/solana")")
TOOLCHAIN := $(SOLANA_BIN)/platform-tools-sdk/sbf/dependencies/platform-tools/llvm/bin

# Tools
CLANG := $(TOOLCHAIN)/clang
LLC := $(TOOLCHAIN)/llc
LD := $(TOOLCHAIN)/ld.lld

# Target configuration (using sbpfv2 for v2 CPU)
SBPF_CPU := v2
LLVM_TARGET := sbpfv2-solana-solana
TARGET := sbf
MCPU := $(SBPF_CPU)

# Standard library directories
STD_LIB_DIRS := $(SOLANA_BIN)/platform-tools-sdk/sbf/dependencies/platform-tools/llvm/lib/sbpfv2
COMPILER_RT_DIR := $(SOLANA_BIN)/platform-tools-sdk/sbf/dependencies/platform-tools/rust/lib/rustlib/$(LLVM_TARGET)/lib

# Include paths for SBF build
INCLUDES := \
	-I$(SDK_DIR) \
	-I$(SDK_DIR)/sol/inc \
	-I$(PIKA_ROOT)/src \
	-I$(PORT_DIR) \
	-I$(PIKA_ROOT)/package/PikaStdLib \
	-I$(ROOT_DIR)/pikascript-api \
	-I$(TOOLCHAIN)/../include

# Force-include our unified config header
FORCE_INCLUDES := \
	-include $(PORT_DIR)/pika_sbf_config.h

# PikaPython configuration flags (shared between SBF and native)
PIKA_DEFINES := \
	-DPIKA_ASSERT_ENABLE=0 \
	-DPIKA_STD_DEVICE_UNSUPPORTED=1 \
	-DPIKA_FILEIO_ENABLE=0 \
	-DPIKA_FLOAT_TYPE_DOUBLE=0 \
	-DPIKA_STACK_BUFF_SIZE=256 \
	-DPIKA_ARG_CACHE_ENABLE=0 \
	-DPIKA_SYNTAX_SLICE_ENABLE=1 \
	-DPIKA_SYNTAX_FORMAT_ENABLE=0 \
	-DPIKA_SYNTAX_IMPORT_EX_ENABLE=0 \
	-DPIKA_SYNTAX_EXCEPTION_ENABLE=0 \
	-DPIKA_EVENT_ENABLE=0 \
	-DPIKA_GC_MARK_SWEEP_ENABLE=0 \
	-DPIKA_DEBUG_BREAK_POINT_MAX=0 \
	-DPIKA_BUILTIN_STRUCT_ENABLE=1

# SBF-only: use custom stack arg hook for return data
SBF_PIKA_DEFINES := \
	$(PIKA_DEFINES) \
	-DPIKA_HOOK_UNUSED_STACK_ARG_OVERRIDE=1

# Warning flags
WARNINGS := -Wno-int-conversion -Wno-macro-redefined -Wno-c23-extensions

# SBF Compiler flags
SBF_CFLAGS := \
	-target $(TARGET) \
	-mcpu=$(MCPU) \
	-fPIC \
	-O2 \
	-fno-builtin \
	-nostdlib \
	-fomit-frame-pointer \
	-fno-stack-protector \
	-mllvm -inline-threshold=50 \
	$(INCLUDES) \
	$(FORCE_INCLUDES) \
	-DPIKA_SOLANA_SBF=1 \
	$(SBF_PIKA_DEFINES) \
	$(WARNINGS)

# Linker flags
LDFLAGS := \
	-z notext \
	-shared \
	--Bdynamic \
	--entry entrypoint \
	--image-base 0x100000000 \
	--strip-all \
	$(SDK_DIR)/sbf.ld \
	-L $(STD_LIB_DIRS) \
	-lc \
	-lm

# Native compiler flags (for pika_compile tool)
NATIVE_CC := gcc
NATIVE_CFLAGS := \
	-O2 \
	-std=c99 \
	-fno-strict-aliasing \
	-I$(PORT_DIR) \
	-I$(PIKA_ROOT)/src \
	$(PIKA_DEFINES) \
	-DPIKA_LINUX_COMPATIBLE=1 \
	-Wno-int-conversion \
	-Wno-incompatible-pointer-types

# Main source (unity build - includes all other sources)
MAIN_SRC := $(ROOT_DIR)/pika_python.c
COMPILER_SRC := $(TOOLS_DIR)/pika_compile.c

# Default target
all: build

# Build the SBF program
build: $(OUTPUT)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(OUTPUT): $(BUILD_DIR) $(MAIN_SRC)
	@echo "========================================"
	@echo "Building PikaPython for Solana"
	@echo "========================================"
	@echo "  Compiling..."
	@$(CLANG) $(SBF_CFLAGS) -emit-llvm -c $(MAIN_SRC) -o $(BUILD_DIR)/$(PROGRAM).bc
	@echo "  Generating object..."
	@$(LLC) -march=sbf -mcpu=$(MCPU) -filetype=obj -o $(BUILD_DIR)/$(PROGRAM).o $(BUILD_DIR)/$(PROGRAM).bc
	@echo "  Linking..."
	@$(LD) $(LDFLAGS) -o $(OUTPUT) $(BUILD_DIR)/$(PROGRAM).o $(COMPILER_RT_DIR)/libcompiler_builtins-*.rlib
	@echo "  Done: $(OUTPUT)"
	@ls -lh $(OUTPUT) | awk '{print "  Size:", $$5}'

# Build native bytecode compiler
compiler: $(COMPILER)

$(COMPILER): $(COMPILER_SRC)
	@echo "========================================"
	@echo "Building PikaPython Native Compiler"
	@echo "========================================"
	@echo "  Compiling..."
	@$(NATIVE_CC) $(NATIVE_CFLAGS) $(COMPILER_SRC) -o $(COMPILER)
	@echo "  Done: $(COMPILER)"
	@echo ""
	@echo "Usage:"
	@echo "  $(COMPILER) \"print('Hello')\"         # Output hex"
	@echo "  $(COMPILER) -o out.bin \"print(1+2)\"  # Write binary"
	@echo "  $(COMPILER) -f script.py              # Compile file"

# Generate keypair if missing
keypair:
	@if [ ! -f $(KEYPAIR) ]; then \
		echo "Generating keypair: $(KEYPAIR)"; \
		solana-keygen new --no-bip39-passphrase -o $(KEYPAIR) --force; \
	fi

# Deploy to local validator
deploy: build keypair
	@echo ""
	@echo "Deploying to local validator..."
	solana program deploy --program-id $(KEYPAIR) $(OUTPUT)

# Run tests
test: deploy
	@echo ""
	@echo "Running tests..."
	@cd tests && ./run_tests.sh

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -f $(COMPILER)
	@echo "Done"

# Help
help:
	@echo "PikaPython Solana Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build pika_python.so (SBF program)"
	@echo "  make compiler     - Build native bytecode compiler"
	@echo "  make deploy       - Build and deploy to local validator"
	@echo "  make test         - Deploy and run tests"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make help         - Show this help"
	@echo ""
	@echo "Requirements:"
	@echo "  - Solana CLI (solana, solana-keygen)"
	@echo "  - Solana platform-tools (clang, llc, ld.lld)"
	@echo "  - GCC (for native compiler)"
	@echo ""
	@echo "Output:"
	@echo "  $(OUTPUT)"
	@echo "  $(COMPILER)"
