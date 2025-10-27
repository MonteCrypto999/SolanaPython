#!/usr/bin/env bash
# Deploy PikaPython programs to Solana

set -e

# Colors
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${CYAN}========================================"
echo "Deploying PikaPython Programs"
echo -e "========================================${NC}"

# Check if build directory exists
if [ ! -d "build" ]; then
    echo -e "${RED}Error: build/ directory not found. Run ./build.sh first.${NC}"
    exit 1
fi

# Deploy function
deploy_program() {
    local name="$1"
    local so_file="$2"
    local keypair="$3"

    if [ ! -f "$so_file" ]; then
        echo -e "${RED}Error: $so_file not found${NC}"
        return 1
    fi

    if [ ! -f "$keypair" ]; then
        echo -e "${RED}Error: $keypair not found${NC}"
        return 1
    fi

    local program_id=$(solana-keygen pubkey "$keypair")
    echo -e "\n${YELLOW}Deploying $name...${NC}"
    echo "  Program ID: $program_id"
    echo "  Binary: $so_file"

    solana program deploy "$so_file" --program-id "$keypair"

    echo -e "${GREEN}  Deployed!${NC}"
}

# Deploy programs
echo ""

# Default: deploy unified program
if [ -z "$1" ] || [ "$1" == "pika" ]; then
    deploy_program "PikaPython (unified)" "build/pika_python.so" "pika-keypair.json"
fi

# Legacy combined program
if [ "$1" == "legacy" ] || [ "$1" == "all" ]; then
    deploy_program "Legacy (demo_python)" "build/demo_python.so" "program-keypair.json"
fi

if [ "$1" == "all" ]; then
    deploy_program "PikaPython (unified)" "build/pika_python.so" "pika-keypair.json"
fi

if [ -n "$1" ] && [ "$1" != "pika" ] && [ "$1" != "legacy" ] && [ "$1" != "all" ]; then
    echo -e "${YELLOW}Usage: ./deploy.sh [pika|legacy|all]${NC}"
    echo "  pika   - Deploy unified PikaPython program (default)"
    echo "  legacy - Deploy legacy demo_python program"
    echo "  all    - Deploy all programs"
    exit 1
fi

echo ""
echo -e "${CYAN}========================================"
echo "Deployment complete!"
echo -e "========================================${NC}"
echo ""
echo "Program IDs:"
if [ -f "pika-keypair.json" ]; then
    echo "  PikaPython: $(solana-keygen pubkey pika-keypair.json)"
fi
if [ -f "program-keypair.json" ]; then
    echo "  Legacy:     $(solana-keygen pubkey program-keypair.json)"
fi
echo ""
echo "Modes (first byte of instruction data):"
echo "  0x00 = EXECUTE_SCRIPT    - Parse and execute Python source"
echo "  0x01 = GENERATE_BYTECODE - Parse Python, return bytecode"
echo "  0x02 = EXECUTE_BYTECODE  - Execute pre-compiled bytecode"
