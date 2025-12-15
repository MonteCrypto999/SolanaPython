# PikaPython for Solana

A Python interpreter for Solana smart contracts. Run Python code on-chain using the Solana SBF runtime.

## Building

```bash
make          # Build the program
./deploy.sh   # Deploy to local validator
```

## Execution Modes

The program supports three modes (set by first byte of instruction data):

- `0x00` - **EXECUTE_SCRIPT**: Parse and execute Python source code
- `0x01` - **GENERATE_BYTECODE**: Parse Python, return compiled bytecode
- `0x02` - **EXECUTE_BYTECODE**: Execute pre-compiled bytecode (fastest)

## Modules

### `solana` - Solana Runtime Access

Access Solana-specific functionality including clock, hashing, and PDAs.

```python
import solana

# Clock
slot = solana.slot()      # Current slot number
epoch = solana.epoch()    # Current epoch number

# Hashing (returns 32-byte bytearray)
h = solana.sha256(data)      # SHA-256 hash
h = solana.keccak256(data)   # Keccak-256 hash
h = solana.blake3(data)      # BLAKE3 hash

# Program Derived Addresses (PDAs)
addr = solana.create_program_address(seeds, program_id)
(addr, bump) = solana.find_program_address(seeds, program_id)

# Cross-Program Invocation
result = solana.cpi(program_idx, accounts, data)
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

n = json.loads('42')         # 42
s = json.loads('"hello"')    # 'hello'
l = json.loads('[1,2,3]')    # [1, 2, 3]
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

# Unpack bytes into tuple
values = struct.unpack('<H', data)  # Returns tuple

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

# Decode base64 string to bytes
data = base64.b64decode('SGVsbG8=')  # bytes
length = len(data)                    # 5
```

## Testing

```bash
./tests/run_tests.sh    # Run full test suite
```

Test modes:
- **VM-only tests**: Compile Python to bytecode locally, execute bytecode on-chain
- **Parser+VM tests**: Parse and execute Python source on-chain

## Configuration

The Makefile supports these options:

- `PIKA_STACK_BUFF_SIZE=512` - Python stack buffer size (default 512 bytes)
- `PIKA_NAME_BUFF_SIZE=64` - Name buffer size
- `PIKA_ARG_ALIGN_ENABLE=0` - Argument alignment (disabled for SBF)

## Compute Units

Typical CU usage:
- Simple expressions: 9,000-15,000 CU
- Loop iterations: ~10,000 CU each
- Hash functions: ~130,000 CU
- JSON operations: ~20,000 CU
- Math functions: ~70,000 CU

## Limitations

- No file I/O
- No network access
- No threading
- Limited recursion depth (due to SBF stack limits)
- Software floating-point only (no hardware FPU)
