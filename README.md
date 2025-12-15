# PikaPython for Solana

A Python interpreter that runs on Solana as an on-chain program. Execute Python code directly on the blockchain.

## Quick Start

```bash
# 1. Build
./build.sh

# 2. Start local validator (in another terminal)
solana-test-validator

# 3. Deploy
solana program deploy --program-id pika-keypair.json build/pika_python.so

# 4. Run Python code
node invoke.js "1 + 2 + 3"
# >>> Result: 6

node invoke.js "print('Hello Solana')"
# Logs: Hello Solana
```

## Build

```bash
./build.sh
```

This compiles PikaPython to a Solana SBF program (`build/pika_python.so`).

**Requirements:**
- Solana CLI tools installed (`solana`, `solana-keygen`)
- Node.js (for invoke.js and tests)
- LLVM (for LTO builds: `brew install llvm`)

## Deploy

```bash
# Deploy to local validator
solana program deploy --program-id pika-keypair.json build/pika_python.so

# Or use deploy script
./deploy.sh
```

## Execution Modes

The program supports three execution modes (first byte of instruction data):

| Mode | Value | Description |
|------|-------|-------------|
| EXECUTE_SCRIPT | 0x00 | Parse and execute Python source on-chain |
| GENERATE_BYTECODE | 0x01 | Compile Python to bytecode, return via program data |
| EXECUTE_BYTECODE | 0x02 | Execute pre-compiled bytecode (faster, cheaper) |

### Using invoke.js

```bash
# Execute script (mode 0x00) - default
node invoke.js "x = 10; x * 2"

# Generate bytecode (mode 0x01)
node invoke.js --compile "print('hello')"

# Execute bytecode (mode 0x02)
node invoke.js --bytecode "1 + 2"

# Verbose output
VERBOSE=1 node invoke.js "42"
```

## REPL-Style Behavior

PikaPython behaves like a Python REPL:
- **Expressions** return their value as program data
- **print()** outputs to Solana logs (not program data)

```bash
node invoke.js "1 + 2"
# >>> Result: 3

node invoke.js "print('hello')"
# Logs: hello
# (no return data)

node invoke.js "print('calculating'); 42"
# Logs: calculating
# >>> Result: 42
```

## Testing

```bash
cd tests

# Run all tests
./run_tests.sh

# Run specific test suite
./run_tests.sh vm        # VM-only tests (native compiler + on-chain VM)
./run_tests.sh parser    # Parser+VM tests (full on-chain pipeline)

# Verbose output
./run_tests.sh -v
```

**Test Suites:**
- `test_vm_only.js` - 91 tests using native compiler + on-chain VM (~49K CU avg)
- `test_parser_vm.js` - 35 tests using on-chain parser + VM (~584K CU avg)
- `test_import_module.js` - Tests for exec() and import sol_N

## Supported Features

### Data Types

| Type | Example | Notes |
|------|---------|-------|
| Integer | `42`, `-5` | 64-bit signed |
| Float | `3.14`, `-1.5` | 32-bit IEEE 754 |
| String | `'hello'`, `"world"` | UTF-8 |
| Boolean | `True`, `False` | |
| List | `[1, 2, 3]` | Mutable |
| Tuple | `(1, 2, 3)` | Immutable |
| Dict | `{'a': 1}` | Key-value store |
| bytearray | `bytearray([65,66])` | Mutable byte sequence |
| None | `None` | |

### Operators

| Category | Operators |
|----------|-----------|
| Arithmetic | `+`, `-`, `*`, `/`, `//`, `%` |
| Comparison | `==`, `!=`, `<`, `>`, `>=` |
| Logical | `and`, `or`, `not` |
| Assignment | `=`, `+=`, `-=`, `*=`, `/=` |

### Control Flow

```python
# If/else
if x > 5:
    print('big')
else:
    print('small')

# While loop
i = 0
while i < 5:
    i = i + 1

# For loop with range
for i in range(10):
    print(i)

# For loop with range(start, end, step)
for i in range(0, 10, 2):
    print(i)
```

### Functions

```python
def add(a, b):
    return a + b

result = add(3, 4)  # 7

def factorial(n):
    if n <= 1:
        return 1
    return n * factorial(n - 1)
```

### Exception Handling

```python
try:
    x = 1 / 0
except:
    print('caught error')
```

Note: Only basic `try`/`except` is supported. No `finally`, `raise`, or specific exception types.

## Builtin Functions

### Standard Python Builtins

| Function | Description | Example |
|----------|-------------|---------|
| `print(*args)` | Output to Solana logs | `print('hello', 123)` |
| `range(end)` | Iterator 0 to end-1 | `range(5)` -> 0,1,2,3,4 |
| `range(start, end)` | Iterator start to end-1 | `range(2, 5)` -> 2,3,4 |
| `range(start, end, step)` | Iterator with step | `range(0, 10, 2)` -> 0,2,4,6,8 |
| `len(obj)` | Length of string/list/dict/tuple/bytearray | `len([1,2,3])` -> 3 |
| `abs(x)` | Absolute value | `abs(-5)` -> 5 |
| `bool(x)` | Convert to boolean (0/1) | `bool(1)` -> 1 |
| `int(x)` | Convert to integer | `int('42')` -> 42 |
| `max(*args)` | Maximum value | `max(1, 5, 3)` -> 5 |
| `min(*args)` | Minimum value | `min(1, 5, 3)` -> 1 |
| `iter(obj)` | Get iterator | `iter([1,2,3])` |
| `zip(*iterables)` | Iterate over multiple iterables in parallel | `zip([1,2], [3,4])` |
| `bytearray(x)` | Create mutable byte array | `bytearray([65,66,67])` |
| `exec(code)` | Execute Python source code | `exec("x = 42")` |

### Solana-Specific Builtins

| Function | Description | Example |
|----------|-------------|---------|
| `open(path, mode)` | Open account as file (VFS) | `f = open('/sol/0', 'r')` |

Note: Time functions are in the `time` module. Solana-specific functions (`slot()`, `epoch()`, `cpi()`) are in the `solana` module.

### bytearray

Create and manipulate byte arrays:

```python
# Create from size (zeros)
b = bytearray(5)
print(len(b))  # 5

# Create from list of integers
b = bytearray([65, 66, 67])
print(b[0])    # 65
print(b[1])    # 66

# Access bytes by index
b = bytearray([10, 20, 30])
print(b[1])    # 20
```

## Modules

PikaPython supports three importable modules:

| Module | Description | Import |
|--------|-------------|--------|
| `math` | Mathematical functions (trig, log, sqrt, etc.) | `import math` |
| `time` | Time functions using Solana clock sysvar | `import time` |
| `solana` | Solana-specific functions (slot, epoch, CPI) | `import solana` |

```python
import math
import time
import solana

# Use module functions
print(math.sqrt(16))      # 4.0
print(time.time())        # Unix timestamp
print(solana.slot())      # Current slot
```

## Math Module

The `math` module provides mathematical functions using 32-bit floats.

```python
import math
# or with alias
import math as m
```

### Constants

| Constant | Value |
|----------|-------|
| `math.pi` | 3.14159... |
| `math.e` | 2.71828... |

### Functions

| Function | Description | Example |
|----------|-------------|---------|
| `math.sin(x)` | Sine | `math.sin(0.5)` |
| `math.cos(x)` | Cosine | `math.cos(0.5)` |
| `math.tan(x)` | Tangent | `math.tan(0.5)` |
| `math.asin(x)` | Arc sine | `math.asin(0.5)` |
| `math.acos(x)` | Arc cosine | `math.acos(0.5)` |
| `math.atan(x)` | Arc tangent | `math.atan(0.5)` |
| `math.atan2(y, x)` | Arc tangent of y/x | `math.atan2(1, 1)` |
| `math.sinh(x)` | Hyperbolic sine | `math.sinh(1.0)` |
| `math.cosh(x)` | Hyperbolic cosine | `math.cosh(1.0)` |
| `math.tanh(x)` | Hyperbolic tangent | `math.tanh(1.0)` |
| `math.exp(x)` | e^x | `math.exp(1.0)` -> 2.718... |
| `math.log(x)` | Natural logarithm | `math.log(2.718)` -> 1.0 |
| `math.log10(x)` | Base-10 logarithm | `math.log10(100)` -> 2.0 |
| `math.log2(x)` | Base-2 logarithm | `math.log2(8)` -> 3.0 |
| `math.pow(x, y)` | x^y | `math.pow(2, 3)` -> 8.0 |
| `math.sqrt(x)` | Square root | `math.sqrt(16)` -> 4.0 |
| `math.ceil(x)` | Ceiling (round up) | `math.ceil(1.5)` -> 2 |
| `math.floor(x)` | Floor (round down) | `math.floor(1.5)` -> 1 |
| `math.trunc(x)` | Truncate to integer | `math.trunc(-1.5)` -> -1.0 |
| `math.fabs(x)` | Absolute value | `math.fabs(-5.0)` -> 5.0 |
| `math.fmod(x, y)` | Floating-point modulo | `math.fmod(5.5, 2.0)` -> 1.5 |
| `math.remainder(x, y)` | IEEE remainder | `math.remainder(5.5, 2.0)` |
| `math.degrees(x)` | Radians to degrees | `math.degrees(3.14159)` -> 180.0 |
| `math.radians(x)` | Degrees to radians | `math.radians(180)` -> 3.14159 |

### Example

```python
import math

# Calculate hypotenuse
a = 3.0
b = 4.0
c = math.sqrt(a*a + b*b)
print(c)  # 5.0

# Trigonometry
angle = math.radians(45)
print(math.sin(angle))  # 0.707...
print(math.cos(angle))  # 0.707...

# Verify sin^2 + cos^2 = 1
x = 1.0
result = math.sqrt(math.sin(x)**2 + math.cos(x)**2)
print(result)  # 1.0
```

## Time Module

The `time` module provides time functions using Solana's clock sysvar.

```python
import time
```

### Functions

| Function | Description | Example |
|----------|-------------|---------|
| `time.time()` | Unix timestamp from clock sysvar | `t = time.time()` |
| `time.ctime(secs)` | Format timestamp as string | `time.ctime(t)` -> "Sun Dec 14 23:45:25 2025" |
| `time.asctime()` | Current time as formatted string | `time.asctime()` |
| `time.gmtime(secs)` | Convert to struct_time (UTC) | `tm = time.gmtime(t)` |
| `time.localtime(secs)` | Convert to struct_time (same as gmtime on Solana) | `tm = time.localtime(t)` |
| `time.mktime(t)` | Convert struct_time tuple to timestamp | `time.mktime((2024, 1, 15, 12, 30, 0, 0, 0, 0))` |

### Example

```python
import time

# Get current unix timestamp
t = time.time()
print(t)  # 1765755911

# Format as readable string
print(time.ctime(t))  # "Sun Dec 14 23:45:25 2025"

# Get current time string
print(time.asctime())  # Same as ctime(time())
```

### mktime Usage

The `mktime()` function accepts either a 9-element tuple (Python standard) or a 6-element list:

```python
import time

# Standard 9-element tuple (year, mon, day, hour, min, sec, wday, yday, isdst)
t = (2023, 11, 14, 22, 13, 20, 0, 0, 0)
timestamp = time.mktime(t)

# Or simplified 6-element list (only first 6 are used)
t = [2023, 11, 14, 22, 13, 20]
timestamp = time.mktime(t)
```

## Solana Module

The `solana` module provides Solana-specific functions.

```python
import solana
```

### Functions

| Function | Description | Example |
|----------|-------------|---------|
| `solana.slot()` | Current slot number | `s = solana.slot()` |
| `solana.epoch()` | Current epoch number | `e = solana.epoch()` |
| `solana.cpi(program_idx, accounts, data)` | Cross-Program Invocation | `solana.cpi(0, [1, 2], b'\\x01')` |

### Example

```python
import solana

# Get current slot
print(solana.slot())  # 3358

# Get current epoch
print(solana.epoch())  # 0

# Cross-Program Invocation
# program_idx: index of target program in transaction accounts
# accounts: list of account indices to pass
# data: instruction data as bytes
result = solana.cpi(0, [1, 2], bytearray([1, 2, 3]))
print(result)  # 0 = success
```

### CPI (Cross-Program Invocation)

Call other Solana programs from Python:

```python
import solana

# Transfer SOL via System Program
# Account 0 = System Program
# Account 1 = Source (signer)
# Account 2 = Destination
# Data = Transfer instruction (type 2) + amount (8 bytes, little-endian)

amount = 1000000  # 0.001 SOL in lamports
data = bytearray(12)
data[0] = 2  # Transfer instruction
# Write amount as little-endian u64
for i in range(8):
    data[4 + i] = (amount >> (i * 8)) & 0xff

result = solana.cpi(0, [1, 2], data)
if result == 0:
    print("Transfer successful!")
```

## Account Storage (VFS)

PikaPython provides a Virtual Filesystem (VFS) to read/write Solana account data using familiar Python file operations.

### Opening Accounts

Accounts can be accessed by **index** (position in transaction) or by **address** (base58 pubkey):

```python
# By index - account position in the transaction
f = open('/sol/0', 'r')   # First account in transaction
f = open('/sol/1', 'w')   # Second account in transaction

# By address - full base58 public key
f = open('/sol/4S8bn7cpNzP15RZejwDjgDN9P5TkAPBGkzH5AgwD7kSG', 'r')
```

**Opening modes:**
- `'r'` - Read-only (works with any account, including non-writable)
- `'w'` - Write (account must be marked writable in transaction)
- `'rw'` - Read-write (account must be marked writable)

```python
# Read account data
data = f.read()
print(data)

# Write to account
f.write('Hello Solana!')

# Close when done
f.close()
```

**Note:** When opening by address, the account must still be passed in the transaction's account list. The address is matched against the accounts provided.

### File Methods

| Method | Description |
|--------|-------------|
| `read(size)` | Read up to `size` bytes (default: all) |
| `write(data)` | Write string or bytes to account |
| `seek(offset, whence)` | Seek to position (0=start, 1=current, 2=end) |
| `tell()` | Get current position |
| `close()` | Close the file |

### Example: Store and Retrieve Data

```python
# Write data to account
f = open('/sol/0', 'w')
f.write('counter=100')
f.close()

# Read it back
f = open('/sol/0', 'r')
data = f.read()
print(data)  # "counter=100"
f.close()
```

## Dynamic Code Execution

### exec() Builtin

Execute Python source code strings at runtime:

```python
# Set variables
x = 0
exec("x = 99")
print(x)  # 99

# Define functions
exec("def double(n):\n  return n * 2")
print(double(5))  # 10

# Define classes
exec("class Counter:\n  val = 0")
print(Counter.val)  # 0

# Execute with print
exec("print(100)")  # Logs: 100
```

### import sol_N - Import from Accounts

Import Python modules stored in Solana accounts by **index** or by **address**:

#### Import by Index

```python
# First, write module code to account 0
f = open('/sol/0', 'w')
f.write('PI = 314\ndef square(x):\n  return x * x')
f.close()

# Then import from account 0
import sol_0

# Access module attributes
print(sol_0.PI)        # 314
print(sol_0.square(5)) # 25
```

#### Import by Address

Import directly using a base58 public key:

```python
# Import using the account's address
import sol_4S8bn7cpNzP15RZejwDjgDN9P5TkAPBGkzH5AgwD7kSG

# Access module attributes
print(sol_4S8bn7cpNzP15RZejwDjgDN9P5TkAPBGkzH5AgwD7kSG.my_function())
```

**Why import by address?**
- **Deterministic**: Module imports work regardless of account ordering in the transaction
- **Self-documenting**: Code clearly shows which on-chain account contains the module
- **Stable references**: Account order may change, but addresses are permanent

**Note:** The account must still be passed in the transaction's account list. The address is matched against the provided accounts.

### Precompiled Bytecode Import

For maximum efficiency, store precompiled bytecode in accounts:

```python
# Account contains bytecode (starts with 0x0f magic byte)
import sol_0  # Automatically detects and executes bytecode

# ~10x faster than parsing source code!
```

Generate bytecode using mode 0x01 or the native compiler, then write the bytecode directly to an account. When imported, the bytecode is executed directly without parsing.

## Cross-Program Invocation (CPI)

Call other Solana programs from PikaPython using the `cpi()` builtin:

```python
result = cpi(program_id, accounts, data)
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `program_id` | `int` or `str` | Target program: account index or base58 pubkey |
| `accounts` | `list` | Account indices or tuples `(index, is_writable, is_signer)` |
| `data` | `bytes` or `bytearray` | Instruction data to send |

### Return Value

Returns `0` on success, or a negative error code on failure.

### Examples

```python
# Call program at account index 0, passing accounts 1 and 2
result = cpi(0, [1, 2], bytearray([1, 2, 3]))

# With explicit account metadata (index, is_writable, is_signer)
result = cpi(0, [(1, True, False), (2, False, False)], bytearray([0x00]))

# Using base58 program address
result = cpi("11111111111111111111111111111111", [1], bytearray([0]))
```

### Account Metadata

When passing a simple integer for an account, its `is_writable` and `is_signer` flags are inherited from the original transaction. To override:

```python
# Simple form - inherits flags from transaction
accounts = [1, 2, 3]

# Explicit form - (index, is_writable, is_signer)
accounts = [
    (1, True, False),   # Account 1, writable, not signer
    (2, False, False),  # Account 2, read-only, not signer
    (3, True, True),    # Account 3, writable, signer
]
```

### Error Codes

| Code | Description |
|------|-------------|
| 0 | Success |
| -1 | CPI context not initialized |
| -2 | Missing arguments |
| -3 | Program ID index out of range |
| -4 | Invalid program ID base58 |
| -5 | Invalid program ID type |
| -6 | Invalid data type |
| -7 | Accounts must be a list |
| -8 | Accounts list is empty |
| -9 | Too many accounts (max 16) |
| -10, -11 | Out of memory |
| -12 | Account index out of range |

### Notes

- The target program must be included in the transaction's account list
- All accounts referenced in the CPI must be passed to the original transaction
- CPI depth is limited by Solana (max 4 levels deep)
- See [Solana CPI documentation](https://solana.com/docs/core/cpi) for more details

## Compute Units

Approximate CU costs:

| Operation | CU (bytecode mode) | CU (script mode) |
|-----------|-------------------|------------------|
| Simple expression | ~6-8K | ~100-500K |
| Arithmetic | ~8-15K | ~500K-1.1M |
| Variable ops | ~7-15K | ~200-800K |
| Simple loop | ~100K | ~1.3M |
| Nested loop | ~270K | Exceeds limit |
| Function call | ~50-100K | ~1M |
| Tuple operations | ~50-100K | ~700K-1.1M |
| exec() | - | ~600K-1.4M |
| import sol_N (source) | - | ~500K-900K |
| import sol_N (bytecode) | - | ~50K-100K |

**Recommendation:** For complex code, use bytecode mode (0x02) with the native compiler for significant CU savings. For reusable modules, store precompiled bytecode in accounts.

## Off-Chain Bytecode Compilation

For better performance and lower compute costs, compile Python to bytecode off-chain using the native compiler, then execute the bytecode on-chain (mode 0x02).

### Build the Native Compiler

```bash
cd tools
./build_compiler.sh
```

### Compile Python to Bytecode

```bash
# Output hex bytecode to stdout
./pika_compile "print('hello')"
# Output: 0x0f70796f...

# Save to binary file
./pika_compile -o program.bin "x = 10; x * 2"

# Compile from file
./pika_compile -f script.py
```

### Execute Bytecode On-Chain

```bash
# Using invoke.js (compiles and executes in one step)
node invoke.js --bytecode "1 + 2"
# >>> Result: 3

# Or manually with pre-compiled bytecode
BYTECODE=$(./tools/pika_compile "1 + 2")
node invoke.js --raw-bytecode "$BYTECODE"
```

### Performance Comparison

| Mode | Parse + Compile | Execute | Total CU |
|------|-----------------|---------|----------|
| Script (0x00) | On-chain | On-chain | ~500K-1.4M |
| Bytecode (0x02) | Off-chain | On-chain | ~6K-100K |

Using bytecode mode reduces compute costs by 10-50x for most operations.

## Manual Invocation (without invoke.js)

You can invoke the program directly using `@solana/web3.js`:

```javascript
const {
    Connection,
    Keypair,
    Transaction,
    TransactionInstruction,
    sendAndConfirmTransaction,
    ComputeBudgetProgram,
} = require('@solana/web3.js');
const fs = require('fs');

const connection = new Connection('http://localhost:8899', 'confirmed');

// Load payer keypair
const payer = Keypair.fromSecretKey(
    new Uint8Array(JSON.parse(fs.readFileSync(
        process.env.HOME + '/.config/solana/id.json'
    )))
);

// Load program ID
const programKeypair = JSON.parse(fs.readFileSync('pika-keypair.json'));
const programId = Keypair.fromSecretKey(new Uint8Array(programKeypair)).publicKey;

// Create instruction data: [mode, ...payload]
const MODE_EXECUTE_SCRIPT = 0x00;
const pythonCode = '1 + 2';
const data = Buffer.concat([
    Buffer.from([MODE_EXECUTE_SCRIPT]),
    Buffer.from(pythonCode, 'utf8')
]);

// Build transaction
const tx = new Transaction();
tx.add(
    ComputeBudgetProgram.setComputeUnitLimit({ units: 1_400_000 }),
    ComputeBudgetProgram.requestHeapFrame({ bytes: 256 * 1024 }),
    new TransactionInstruction({
        keys: [],
        programId,
        data,
    })
);

// Send and get result
tx.feePayer = payer.publicKey;
tx.recentBlockhash = (await connection.getLatestBlockhash()).blockhash;

const simResult = await connection.simulateTransaction(tx);
if (simResult.value.returnData) {
    const result = Buffer.from(
        simResult.value.returnData.data[0],
        simResult.value.returnData.data[1]
    ).toString('utf8');
    console.log('Result:', result);  // "3"
}
```

### Passing Accounts

To use VFS or `import sol_N`, pass accounts in the transaction:

```javascript
const accountKeypair = Keypair.generate();

// Create account owned by the program
const createTx = new Transaction().add(
    SystemProgram.createAccount({
        fromPubkey: payer.publicKey,
        newAccountPubkey: accountKeypair.publicKey,
        lamports: await connection.getMinimumBalanceForRentExemption(256),
        space: 256,
        programId: programId,
    })
);
await sendAndConfirmTransaction(connection, createTx, [payer, accountKeypair]);

// Now use the account in Python
const instruction = new TransactionInstruction({
    keys: [
        { pubkey: accountKeypair.publicKey, isSigner: false, isWritable: true }
    ],
    programId,
    data: Buffer.concat([
        Buffer.from([0x00]),
        Buffer.from('f=open("/sol/0","w")\nf.write("hello")\nf.close()', 'utf8')
    ]),
});
```

## Architecture

```
+-----------------------------------------------------+
|                   Solana Program                     |
|  +-------------+  +-------------+  +-------------+  |
|  |   Parser    |->|  Compiler   |->|     VM      |  |
|  | (PikaParser)|  |(PikaCompiler|  |  (PikaVM)   |  |
|  +-------------+  +-------------+  +-------------+  |
|         ^                                |          |
|    Python Source                   Return Data      |
|    (mode 0x00)                    (expression)      |
|                                        or           |
|    Bytecode ----------------------> sol_log()       |
|    (mode 0x02)                      (print)         |
+-----------------------------------------------------+
         |                                |
         v                                v
+-----------------------------------------------------+
|                Virtual Filesystem (VFS)              |
|  /sol/0  ->  Account 0 data                         |
|  /sol/1  ->  Account 1 data                         |
|  /sol/N  ->  Account N data                         |
+-----------------------------------------------------+
```

## Limitations

- **Limited imports** - Only `import math` (built-in) and `import sol_N` (account modules); `import X as Y` and `from X import Y` supported
- **No networking** - Network operations not supported
- **Basic exceptions** - `try`/`except` supported, no `finally` or exception types
- **32-bit floats** - Single precision floating point
- **CU limits** - Complex operations may exceed compute budget
- **256KB heap** - Maximum heap size for allocations
- **Account write access** - VFS write requires accounts marked writable in transaction

## Files

```
solana/
├── build.sh            # Build script
├── deploy.sh           # Deploy script
├── invoke.js           # Invocation tool
├── pika_python.c       # Main program source
├── pika-keypair.json   # Program keypair
├── build/              # Build output
├── tests/              # Test suite
│   ├── run_tests.sh
│   ├── test_parser_vm.js
│   ├── test_vm_only.js
│   ├── test_import_module.js
│   └── test_builtins.js
└── tools/              # Native compiler
    ├── pika_compile
    └── build_compiler.sh
```

## License

PikaPython is based on [PikaPython](https://github.com/pikasTech/PikaPython) - MIT License.
