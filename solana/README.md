# PikaPython on Solana

Run Python smart contracts on Solana. Execute Python code directly on-chain with full access to Solana's runtime features.

## Quick Start

### Web Frontend

Open `frontend/index.html` in a browser or serve it locally:

```bash
cd frontend && python3 -m http.server 8080
# Visit http://localhost:8080
```

### Local Development

```bash
# Start local validator
solana-test-validator --reset

# Build the program
make

# Deploy
solana program deploy --program-id pika-keypair.json build/pika_python.so -u localhost

# Run tests
node tests/test_vm_only.js
```

## Execution Modes

The program supports three modes (set by first byte of instruction data):

- `0x00` - **EXECUTE_SCRIPT**: Parse and execute Python source code (~600K-1.4M CU)
- `0x01` - **GENERATE_BYTECODE**: Parse Python, return compiled bytecode
- `0x02` - **EXECUTE_BYTECODE**: Execute pre-compiled bytecode (~20K-100K CU)

## Modules

### `solana` - Solana Runtime Access

Access Solana-specific functionality including clock, hashing, CPIs, and PDAs.

```python
import solana

# Clock
slot = solana.slot()          # Current slot number
epoch = solana.epoch()        # Current epoch number

# Program ID
pid = solana.program_id()     # Current program's public key (32 bytes)

# Hashing (returns 32-byte bytearray)
h = solana.sha256(data)       # SHA-256 hash
h = solana.keccak256(data)    # Keccak-256 hash

# Program Derived Addresses (PDAs)
addr = solana.create_program_address(seeds, program_id)
(addr, bump) = solana.find_program_address(seeds, program_id)

# Cross-Program Invocation
# accounts = [(index, is_writable, is_signer), ...]
result = solana.cpi(program_idx, accounts, data)

# CPI with PDA signing
# signer_seeds = [[seed1, seed2, ..., bump_byte], ...]
result = solana.invoke_signed(program_idx, accounts, data, signer_seeds)
```

### `math` - Mathematical Functions

Standard math operations using software floating-point.

```python
import math

math.sin(x)      # Sine
math.cos(x)      # Cosine
math.tan(x)      # Tangent
math.sqrt(x)     # Square root
math.pow(x, y)   # Power
math.log(x)      # Natural logarithm
math.exp(x)      # Exponential
math.floor(x)    # Floor
math.ceil(x)     # Ceiling
math.fabs(x)     # Absolute value

# Constants
math.pi          # Pi (3.14159...)
math.e           # Euler's number (2.71828...)
```

### `time` - Time Functions

Access to Solana's clock sysvar for time operations.

```python
import time

ts = time.time()         # Unix timestamp from Solana clock
s = time.ctime(ts)       # Format timestamp as string
s = time.asctime()       # Current time as string
t = time.gmtime(ts)      # Parse to time struct
t = time.localtime(ts)   # Parse to time struct (same as gmtime)
ts = time.mktime(t)      # Convert time struct/list to timestamp
```

### `json` - JSON Encoding/Decoding

Serialize and deserialize JSON data.

```python
import json

s = json.dumps(42)           # '42'
s = json.dumps("hello")      # '"hello"'
s = json.dumps([1, 2, 3])    # '[1,2,3]'
s = json.dumps({'a': 1})     # '{"a":1}'

n = json.loads('42')         # 42
s = json.loads('"hello"')    # 'hello'
l = json.loads('[1,2,3]')    # [1, 2, 3]
d = json.loads('{"a":1}')    # {'a': 1}
```

### `struct` - Binary Packing

Pack and unpack binary data structures.

```python
import struct

# Calculate size
struct.calcsize('B')     # 1 (byte)
struct.calcsize('H')     # 2 (uint16)
struct.calcsize('I')     # 4 (uint32)
struct.calcsize('Q')     # 8 (uint64)

# Pack values into bytes
data = struct.pack('<H', [0x1234])  # Little-endian uint16
data = struct.pack('<Q', [1000000]) # Little-endian uint64

# Unpack bytes into list
values = struct.unpack('<H', data)  # Returns list

# Format codes:
# < = little-endian, > = big-endian
# b/B = int8/uint8
# h/H = int16/uint16
# i/I = int32/uint32
# q/Q = int64/uint64
# x = padding byte
```

### `base64` - Base64 Encoding

Encode and decode base64 strings.

```python
import base64

# Encode bytes to base64 string
s = base64.b64encode(b'Hello')    # 'SGVsbG8='

# Decode base64 string to bytes
data = base64.b64decode('SGVsbG8=')  # b'Hello'
```

### `base58` - Base58 Encoding (Solana Addresses)

Encode and decode Solana public keys and addresses.

```python
import base58

# Encode bytes to base58 string
s = base58.b58encode(pubkey_bytes)

# Decode base58 string to bytes
data = base58.b58decode('11111111111111111111111111111111')  # 32 bytes
```

## Account Access (VFS)

Access Solana accounts via the virtual filesystem:

```python
# Read account data (account #1)
f = open("/sol/1", "r")
data = f.read(32)
f.close()

# Write to writable account
f = open("/sol/1", "w")
f.write("Hello Solana!")
f.close()
```

Account indices correspond to the order in the transaction's account list (starting from 0).

## Example: Create PDA Account

```python
import solana
import struct

# Derive PDA
seeds = [b"vault"]
pid = solana.program_id()
pda, bump = solana.find_program_address(seeds, pid)

# Build CreateAccount instruction for System Program
lamports = 1000000  # rent
space = 64
data = struct.pack('<I', [0])  # CreateAccount = 0
data = data + struct.pack('<Q', [lamports])
data = data + struct.pack('<Q', [space])
data = data + pid  # owner

# CPI to System Program with PDA signer
# Account #0 = wallet, #1 = System Program, #2 = PDA
accounts = [(0, 1, 1), (2, 1, 1)]  # (idx, writable, signer)
signer_seeds = [[b"vault", bytes([bump])]]

solana.invoke_signed(1, accounts, data, signer_seeds)
```

## Testing

```bash
# Run all VM tests (requires local validator)
node tests/test_vm_only.js

# Run PDA tests
node tests/test_pda.js

# Run with verbose output
node tests/test_vm_only.js --verbose
```

## Building

Requirements:
- Solana CLI tools (3.0+)
- Node.js (for tests)
- clang (for native compiler)

```bash
# Build Solana program
make

# Build native compiler (for off-chain compilation)
./tools/build_compiler.sh

# Clean
make clean
```

## Compute Units

Typical CU usage:
- Simple expressions: ~20K CU
- Module imports: ~50K CU
- Hash operations: ~60K CU
- PDA derivation: ~100K CU
- Script parsing (on-chain): ~600K-1.4M CU

## Directory Structure

```
solana/
├── build/              # Compiled program output
├── frontend/           # Web UI for testing
├── pikascript-api/     # Module implementations
├── tests/              # Test suite
├── tools/              # Compiler and utilities
├── pika_python.c       # Main entrypoint
├── Makefile            # Build configuration
└── pika-keypair.json   # Program keypair
```

## Configuration

The Makefile supports these options:

- `PIKA_STACK_BUFF_SIZE=512` - Python stack buffer size
- `PIKA_NAME_BUFF_SIZE=64` - Name buffer size
- `PIKA_ARG_ALIGN_ENABLE=0` - Argument alignment (disabled for SBF)

## Limitations

- No file I/O (use VFS for account access)
- No network access
- No threading
- Limited recursion depth (due to SBF stack limits)
- Software floating-point only (no hardware FPU)

## License

MIT
