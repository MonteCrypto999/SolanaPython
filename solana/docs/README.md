# PikaPython Frontend

Web interface for running Python on Solana mainnet.

## Quick Start

Open `index.html` in a browser or deploy to any static hosting service.

**Live Demo**: The frontend is pre-configured for mainnet with program ID `pythonKBk7JcXsbwYzMRy2tL8L9tZqUbgekxRfT1bTE`

## Deploy

```bash
# Build and prepare for deployment
./deploy.sh

# Deploy to specific platforms
./deploy.sh --vercel    # Deploy to Vercel
./deploy.sh --netlify   # Deploy to Netlify
./deploy.sh --ipfs      # Deploy to IPFS

# Or manually upload dist/ to any static host
```

### Cloudflare Pages

```bash
npx wrangler pages deploy dist --project-name=pikapython
```

### GitHub Pages

1. Push the `frontend` directory to a GitHub repository
2. Go to Settings → Pages
3. Select branch and `/frontend` folder
4. Save

## Features

- **Script Mode**: Send Python code for on-chain parsing (~600K-1.4M CU)
- **Bytecode Mode**: Send pre-compiled bytecode (~20K-100K CU)
- **Scratchpad Accounts**: Create persistent storage accounts
- **VFS API**: Read/write account data with Python file operations
- **Multi-wallet Support**: Phantom, Solflare, Coinbase, Ledger, MetaMask, etc.
- **CPI Support**: Cross-program invocation to other Solana programs

## Scratchpad (Persistent Storage)

Create accounts owned by PikaPython to store persistent data:

1. Connect your wallet
2. Click **Create Scratchpad** (costs ~0.001 SOL rent)
3. The account is automatically added to your accounts list
4. Use Python's VFS API to read/write:

```python
# Write data
f = open("/sol/0", "w")
f.write("Hello Solana!")
f.close()

# Read data
f = open("/sol/0", "r")
data = f.read(64)
f.close()
print(data)
```

Account indices map to the order in the accounts list:
- `/sol/0` = first account
- `/sol/1` = second account
- etc.

## VFS API Reference

| Function | Description |
|----------|-------------|
| `open("/sol/N", "r")` | Open account N by index (fast) |
| `open("/sol/N", "w")` | Open account N for writing |
| `open("/sol/<pubkey>", "r")` | Open account by base58 pubkey |
| `f.read(n)` | Read n bytes |
| `f.write(data)` | Write data, returns bytes written |
| `f.seek(pos)` | Seek to position |
| `f.tell()` | Get current position |
| `f.close()` | Close file |

**Path Formats:**
- `/sol/0`, `/sol/1` - Index-based (fast, ~60K CU)
- `/sol/<base58_pubkey>` - Pubkey-based (~90K CU, includes base58 decode)

## Packages

| Package | Functions |
|---------|-----------|
| `solana` | `slot()`, `epoch()`, `sha256()`, `keccak256()`, `cpi()`, `find_program_address()` |
| `math` | `sin()`, `cos()`, `sqrt()`, `pow()`, `log()`, `exp()`, `pi`, `e` |
| `time` | `time()`, `ctime()`, `gmtime()`, `localtime()` |
| `json` | `dumps()`, `loads()` |
| `struct` | `pack()`, `unpack()`, `calcsize()` |
| `base58` | `b58encode()`, `b58decode()` |
| `base64` | `b64decode()` |

## WASM Compiler

The frontend includes a WebAssembly compiler that compiles Python to bytecode in the browser:

1. Switch to **Bytecode** mode
2. Write Python code
3. Click **Compile**
4. Bytecode appears in the hex field
5. Click **Execute** to run on-chain

Bytecode mode uses ~10-100x less compute units than script mode.

## Building WASM Compiler

To rebuild the WASM compiler:

```bash
cd ../tools
./build_wasm.sh
cp pika_compile.js pika_compile.wasm ../frontend/
```

Requires Emscripten SDK.
