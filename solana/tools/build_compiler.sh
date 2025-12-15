#!/usr/bin/env bash
# Build native PikaPython bytecode compiler

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PIKA_ROOT="$SCRIPT_DIR/../.."

echo "Building PikaPython Native Compiler..."

# Compiler flags matching SBF configuration
CFLAGS="-O0 -g -std=c99 -fno-strict-aliasing"
CFLAGS="$CFLAGS -DPIKA_ASSERT_ENABLE=0"
CFLAGS="$CFLAGS -DPIKA_STD_DEVICE_UNSUPPORTED=1"
CFLAGS="$CFLAGS -DPIKA_FILEIO_ENABLE=0"
CFLAGS="$CFLAGS -DPIKA_FLOAT_TYPE_DOUBLE=0"
CFLAGS="$CFLAGS -DPIKA_STACK_BUFF_SIZE=256"
CFLAGS="$CFLAGS -DPIKA_ARG_CACHE_ENABLE=0"
CFLAGS="$CFLAGS -DPIKA_SYNTAX_SLICE_ENABLE=1"
CFLAGS="$CFLAGS -DPIKA_SYNTAX_FORMAT_ENABLE=0"
CFLAGS="$CFLAGS -DPIKA_SYNTAX_IMPORT_EX_ENABLE=1"
CFLAGS="$CFLAGS -DPIKA_SYNTAX_EXCEPTION_ENABLE=1"
CFLAGS="$CFLAGS -DPIKA_LINUX_COMPATIBLE=1"
CFLAGS="$CFLAGS -Wno-int-conversion -Wno-incompatible-pointer-types"

# Include paths - SBF port first so headers are found, but without PIKA_SOLANA_SBF they use system libs
INCLUDES="-I$PIKA_ROOT/port/solana_sbf -I$PIKA_ROOT/src"

# Output
OUTPUT="$SCRIPT_DIR/pika_compile"

echo "  Compiling..."
gcc $CFLAGS $INCLUDES "$SCRIPT_DIR/pika_compile.c" -o "$OUTPUT" 2>&1

echo "  Done: $OUTPUT"
echo ""
echo "Usage:"
echo "  $OUTPUT \"print('Hello')\"              # Output hex"
echo "  $OUTPUT -o out.bin \"print(1+2)\"       # Write binary"
echo "  $OUTPUT -f script.py                   # Compile file"
