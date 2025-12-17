#!/usr/bin/env bash
# Deploy PikaPython to Solana Mainnet
#
# Usage:
#   ./deploy.sh                           # Use default wallet
#   ./deploy.sh -k /path/to/keypair.json  # Use specific wallet
#
# Prerequisites:
#   - Funded wallet (~5 SOL for rent-exempt deposit)
#   - Built program: make build
#
# Cost estimate:
#   - Rent-exempt deposit: ~4.35 SOL (recoverable via `solana program close`)
#   - Transaction fees: ~0.01 SOL
#
# To recover funds later:
#   solana program close <PROGRAM_ID> --recipient <YOUR_WALLET>

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR/.."
PROGRAM_SO="$ROOT_DIR/build/pika_python.so"
PROGRAM_KEYPAIR="$ROOT_DIR/mainnet/pika-mainnet-keypair.json"
WALLET_KEYPAIR="${HOME}/.config/solana/id.json"
RPC_URL="https://api.mainnet-beta.solana.com"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -k|--keypair)
            WALLET_KEYPAIR="$2"
            shift 2
            ;;
        -p|--program-keypair)
            PROGRAM_KEYPAIR="$2"
            shift 2
            ;;
        -u|--url)
            RPC_URL="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: ./deploy.sh -k <payer_keypair> [-p program_keypair] [-u rpc_url]"
            echo ""
            echo "Options:"
            echo "  -k, --keypair          Path to payer wallet keypair (required)"
            echo "  -p, --program-keypair  Path to program keypair (default: generates new one)"
            echo "  -u, --url              RPC URL (default: mainnet-beta)"
            echo ""
            echo "Example:"
            echo "  ./deploy.sh -k ~/my-wallet.json -p ~/program-keypair.json"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Require payer keypair
if [ ! -f "$WALLET_KEYPAIR" ]; then
    echo -e "${RED}Error: Payer keypair required. Use -k <keypair.json>${NC}"
    echo ""
    echo "Usage: ./deploy.sh -k <payer_keypair> [-p program_keypair] [-u rpc_url]"
    exit 1
fi

# Colors
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

echo ""
echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}PikaPython Mainnet Deployment${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# Check if program is built
if [ ! -f "$PROGRAM_SO" ]; then
    echo -e "${RED}Error: Program not built. Run 'make build' first.${NC}"
    exit 1
fi

echo -e "RPC URL: ${CYAN}$RPC_URL${NC}"
echo -e "Payer keypair: ${CYAN}$WALLET_KEYPAIR${NC}"
echo -e "Program keypair: ${CYAN}$PROGRAM_KEYPAIR${NC}"

# Check wallet balance
WALLET=$(solana-keygen pubkey "$WALLET_KEYPAIR")
BALANCE=$(solana balance "$WALLET" --url "$RPC_URL" | awk '{print $1}')
echo -e "Wallet: ${CYAN}$WALLET${NC}"
echo -e "Balance: ${CYAN}$BALANCE SOL${NC}"

# Calculate required balance
PROGRAM_SIZE=$(ls -l "$PROGRAM_SO" | awk '{print $5}')
REQUIRED=$(solana rent $PROGRAM_SIZE --url "$RPC_URL" 2>/dev/null | grep -o '[0-9.]*' | head -1)

echo ""
echo -e "Program size: ${CYAN}$(ls -lh "$PROGRAM_SO" | awk '{print $5}')${NC}"
echo -e "Required rent: ${CYAN}~$REQUIRED SOL${NC}"

if (( $(echo "$BALANCE < $REQUIRED" | bc -l) )); then
    echo ""
    echo -e "${RED}Error: Insufficient balance!${NC}"
    echo -e "Need at least ${YELLOW}$REQUIRED SOL${NC}, have ${RED}$BALANCE SOL${NC}"
    exit 1
fi

# Generate or use existing program keypair
if [ ! -f "$PROGRAM_KEYPAIR" ]; then
    echo ""
    echo "Generating new program keypair..."
    solana-keygen new --no-bip39-passphrase -o "$PROGRAM_KEYPAIR" --force
fi

PROGRAM_ID=$(solana-keygen pubkey "$PROGRAM_KEYPAIR")
echo ""
echo -e "Program ID: ${GREEN}$PROGRAM_ID${NC}"

# Final confirmation
echo ""
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}MAINNET DEPLOYMENT WARNING${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""
echo "This will deploy PikaPython to Solana MAINNET."
echo ""
echo "  Program ID: $PROGRAM_ID"
echo "  Cost: ~$REQUIRED SOL (rent deposit, recoverable)"
echo "  Wallet: $WALLET"
echo "  Balance: $BALANCE SOL"
echo ""
echo -e "${YELLOW}This action will spend real SOL!${NC}"
echo ""
read -p "Type 'DEPLOY' to confirm: " CONFIRM

if [ "$CONFIRM" != "DEPLOY" ]; then
    echo "Deployment cancelled."
    exit 1
fi

# Deploy
echo ""
echo "Deploying..."
solana program deploy \
    --program-id "$PROGRAM_KEYPAIR" \
    --keypair "$WALLET_KEYPAIR" \
    --url "$RPC_URL" \
    "$PROGRAM_SO"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Deployment Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "Program ID: ${GREEN}$PROGRAM_ID${NC}"
echo ""
echo "Save this program ID - you'll need it to invoke the program."
echo ""
echo "To recover rent deposit later:"
echo "  solana program close $PROGRAM_ID --recipient $WALLET"
