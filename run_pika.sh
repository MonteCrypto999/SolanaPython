#!/usr/bin/env bash
set -e

# Script to build, deploy, and run PikaPython on Solana in one go

echo "=== 1. Building Program ==="
cd "$(dirname "$0")/solana_build"
./build.sh

echo "=== 2. Managing Keypair ==="
KEYPAIR="program-keypair.json"
# Generate keypair if it doesn't exist to ensure consistent Program ID
if [ ! -f "$KEYPAIR" ]; then
    echo "Generating new keypair..."
    solana-keygen new -o "$KEYPAIR" --no-bip39-passphrase --force --silent
else
    echo "Using existing keypair: $KEYPAIR"
fi

# Get Program ID from keypair
PROGRAM_ID=$(solana address -k "$KEYPAIR")
echo "Program ID: $PROGRAM_ID"

echo "=== 3. Deploying Program ==="
# Force localhost URL to ensure we deploy to the same cluster invoke.js uses
solana program deploy --url http://localhost:8899 --program-id "$KEYPAIR" build/demo_python.so

echo "=== 4. Running Python Code on Solana ==="
INVOKE_SCRIPT="invoke.js"

# You can pass a python script as argument to this bash script, or use default
PYTHON_SCRIPT="${1:-print('Hello REPL')}"

echo "Executing: $PYTHON_SCRIPT"
# We need NODE_PATH to find modules in C-Nocchio if running from here
# Assuming C-Nocchio is at ../../C-Nocchio from solana_build, which is PikaPython/solana_build
NODE_PATH=../../C-Nocchio/node_modules node "$INVOKE_SCRIPT" "$PROGRAM_ID" "$PYTHON_SCRIPT"

echo "=== Done ==="

