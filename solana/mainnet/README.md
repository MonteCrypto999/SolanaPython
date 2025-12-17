# PikaPython Mainnet Deployment

Scripts for deploying and running PikaPython on Solana mainnet.

## Cost Estimate

| Item | Cost | Notes |
|------|------|-------|
| Program rent deposit | ~4.35 SOL | Recoverable via `solana program close` |
| Transaction fee | ~0.000005 SOL | Per transaction (base fee) |
| Compute units | Free | Unless priority fee is set |

At $220/SOL, deployment costs ~$957 in locked rent (recoverable).

## Prerequisites

```bash
# Build the program
make build

# Build the compiler
make compiler

# Configure Solana CLI for mainnet
solana config set --url mainnet-beta

# Ensure wallet is funded
solana balance
```

## Deploy

```bash
./deploy.sh
```

This will:
1. Check your wallet balance
2. Generate a program keypair (if needed)
3. Confirm before deploying
4. Deploy to mainnet

Save the Program ID - you'll need it to invoke the program.

## Run Python Code

### Hello World

```bash
# Run inline code
node invoke.js <PROGRAM_ID> "print('Hello World')"

# Run from file
node invoke.js <PROGRAM_ID> -f hello.py
```

### Transfer SOL

```bash
# Transfer 0.001 SOL to a recipient
node transfer.js <PROGRAM_ID> <RECIPIENT_PUBKEY> 0.001
```

### With Priority Fee

```bash
# Add priority fee for faster inclusion
node invoke.js <PROGRAM_ID> "print('fast!')" --priority 10000
```

## Examples

| File | Description |
|------|-------------|
| `hello.py` | Simple Hello World |
| `transfer.py` | Transfer SOL via CPI |

## Recover Funds

To close the program and recover the rent deposit:

```bash
solana program close <PROGRAM_ID> --recipient <YOUR_WALLET>
```

## Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `RPC_URL` | Solana RPC endpoint | `https://api.mainnet-beta.solana.com` |
| `KEYPAIR_PATH` | Path to wallet keypair | `~/.config/solana/id.json` |
