#!/bin/bash
# Compile all Python examples to bytecode
# Usage: ./compile_all.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPILER="$SCRIPT_DIR/../tools/pika_compile"
BYTECODE_DIR="$SCRIPT_DIR/bytecode"

# Check compiler exists
if [ ! -x "$COMPILER" ]; then
    echo "Error: pika_compile not found. Run build_compiler.sh first."
    exit 1
fi

# Create bytecode directory
mkdir -p "$BYTECODE_DIR"

echo "PikaPython Bytecode Compiler"
echo "============================"
echo

# Compile each .py file
for pyfile in "$SCRIPT_DIR"/*.py; do
    if [ -f "$pyfile" ]; then
        name=$(basename "$pyfile" .py)
        binfile="$BYTECODE_DIR/$name.bin"

        echo "Compiling $name.py..."
        "$COMPILER" -f "$pyfile" -o "$binfile"

        # Show hex
        echo -n "  Hex: "
        "$COMPILER" -f "$pyfile" | head -c 60
        echo "..."
        echo
    fi
done

echo "Done! Bytecode files in: $BYTECODE_DIR"
ls -la "$BYTECODE_DIR"
