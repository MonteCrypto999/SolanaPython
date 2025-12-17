#!/usr/bin/env node
/**
 * PikaPython VM-Only Tests
 *
 * Tests the on-chain VM by compiling Python to bytecode using the native
 * compiler, then executing the bytecode on-chain (mode 0x02).
 *
 * This tests VM execution WITHOUT using the on-chain parser.
 *
 * Usage:
 *   node test_vm_only.js           Run all tests
 *   node test_vm_only.js --verbose Show detailed output
 */

const { spawnSync } = require('child_process');
const {
    Connection,
    Keypair,
    Transaction,
    TransactionInstruction,
    sendAndConfirmTransaction,
    ComputeBudgetProgram,
} = require('@solana/web3.js');
const fs = require('fs');
const path = require('path');

// Configuration
const RPC_URL = 'http://localhost:8899';
const TESTS_DIR = __dirname;
const SOLANA_BUILD_DIR = path.join(TESTS_DIR, '..');
const PIKA_KEYPAIR_PATH = path.join(SOLANA_BUILD_DIR, 'pika-keypair.json');
const COMPILER_PATH = path.join(SOLANA_BUILD_DIR, 'tools', 'pika_compile');

// Execution mode for bytecode
const MODE_EXECUTE_BYTECODE = 0x02;

// Colors
const GREEN = '\x1b[32m';
const RED = '\x1b[31m';
const YELLOW = '\x1b[33m';
const CYAN = '\x1b[36m';
const DIM = '\x1b[2m';
const RESET = '\x1b[0m';

// Test cases: { name, code, expected }
// REPL-style: expressions return their value, print() goes to logs only
const TEST_CASES = [
    // === Basic Expressions ===
    { name: 'integer literal', code: `42`, expected: '42' },
    { name: 'string literal', code: `'Hello'`, expected: 'Hello' },
    { name: 'zero', code: `0`, expected: '0' },
    { name: 'negative (via subtraction)', code: `0-123`, expected: '-123' },
    { name: 'large number', code: `999999`, expected: '999999' },

    // === Arithmetic ===
    { name: 'addition', code: `1+2`, expected: '3' },
    { name: 'subtraction', code: `10-3`, expected: '7' },
    { name: 'multiplication', code: `6*7`, expected: '42' },
    { name: 'division', code: `20/4`, expected: '5.0' },
    { name: 'complex expression', code: `1+2+3`, expected: '6' },
    { name: 'operator precedence', code: `10-2*3`, expected: '4' },
    { name: 'parentheses', code: `(2+3)*4`, expected: '20' },
    { name: 'nested parentheses', code: `(1+2)*(3+4)`, expected: '21' },

    // === Float Operations ===
    { name: 'float literal', code: `1.5`, expected: '1.5' },
    { name: 'float addition', code: `1.5 + 2.5`, expected: '4.0' },
    { name: 'float multiplication', code: `2.0 * 3.0`, expected: '6.0' },
    { name: 'float division', code: `10.0 / 4.0`, expected: '2.5' },
    { name: 'float subtraction', code: `5.5 - 2.5`, expected: '3.0' },
    { name: 'negative float', code: `0.0 - 1.5`, expected: '-1.5' },
    { name: 'int-float addition', code: `1 + 0.5`, expected: '1.5' },
    { name: 'float comparison', code: `3.14 > 3.0`, expected: 'True' },

    // === Variables ===
    { name: 'variable assignment', code: `x = 5\nx`, expected: '5' },
    { name: 'variable arithmetic', code: `a = 10\nb = 20\na+b`, expected: '30' },
    { name: 'variable chain', code: `x = 1\ny = 2\nz = x + y\nz`, expected: '3' },
    { name: 'variable update', code: `x = 5\nx = x + 1\nx`, expected: '6' },
    { name: 'multi-variable expression', code: `a = 2\nb = 3\nc = 4\na*b+c`, expected: '10' },

    // === Range/Loops ===
    { name: 'range sum', code: `s = 0\nfor i in range(5):\n    s = s + i\ns`, expected: '10' },
    { name: 'range with start', code: `s = 0\nfor i in range(2, 5):\n    s = s + i\ns`, expected: '9' },
    { name: 'range with step', code: `s = 0\nfor i in range(0, 10, 2):\n    s = s + i\ns`, expected: '20' },
    { name: 'nested loops', code: `s = 0\nfor i in range(3):\n    for j in range(2):\n        s = s + 1\ns`, expected: '6' },

    // === Comparisons ===
    { name: 'greater than', code: `10 > 5`, expected: 'True' },
    { name: 'less than', code: `3 < 8`, expected: 'True' },
    { name: 'equal true', code: `5 == 5`, expected: 'True' },
    { name: 'equal false', code: `5 == 6`, expected: 'False' },

    // === Solana Module ===
    { name: 'solana.slot returns positive', code: `import solana\nsolana.slot() > 0`, expected: 'True' },
    { name: 'solana.epoch returns non-negative', code: `import solana\nsolana.epoch() >= 0`, expected: 'True' },

    // === Time Module ===
    { name: 'time.time returns timestamp', code: `import time\ntime.time() > 1700000000`, expected: 'True' },
    { name: 'time.ctime returns string', code: `import time\nlen(time.ctime(1700000000)) > 20`, expected: 'True' },
    { name: 'time.ctime format check', code: `import time\ntime.ctime(1700000000)`, expected: 'Tue Nov 14 22:13:20 2023' },
    { name: 'time.asctime returns string', code: `import time\nlen(time.asctime()) > 20`, expected: 'True' },
    { name: 'time.gmtime returns object', code: `import time\nt = time.gmtime(1700000000)\nt.tm_year`, expected: '2023' },
    { name: 'time.gmtime month', code: `import time\nt = time.gmtime(1700000000)\nt.tm_mon`, expected: '11' },
    { name: 'time.gmtime day', code: `import time\nt = time.gmtime(1700000000)\nt.tm_mday`, expected: '14' },
    { name: 'time.gmtime hour', code: `import time\nt = time.gmtime(1700000000)\nt.tm_hour`, expected: '22' },
    { name: 'time.gmtime minute', code: `import time\nt = time.gmtime(1700000000)\nt.tm_min`, expected: '13' },
    { name: 'time.gmtime second', code: `import time\nt = time.gmtime(1700000000)\nt.tm_sec`, expected: '20' },
    { name: 'time.localtime returns object', code: `import time\nt = time.localtime(1700000000)\nt.tm_year`, expected: '2023' },
    { name: 'time.mktime with 6-element list', code: `import time\ntime.mktime([2023, 11, 14, 22, 13, 20]) > 1699900000`, expected: 'True' },
    { name: 'time.mktime with 9-element tuple', code: `import time\ntime.mktime((2023, 11, 14, 22, 13, 20, 0, 0, 0)) > 1699900000`, expected: 'True' },

    // === Logical Operators ===
    { name: 'and true', code: `True and True`, expected: 'True' },
    { name: 'or false', code: `False or False`, expected: 'False' },

    // === String ===
    { name: 'string concat', code: `'Hello' + ' World'`, expected: 'Hello World' },

    // === New Builtins ===
    { name: 'abs positive', code: `abs(5)`, expected: '5' },
    { name: 'abs negative', code: `abs(0-5)`, expected: '5' },
    { name: 'abs zero', code: `abs(0)`, expected: '0' },
    { name: 'len string', code: `len('hello')`, expected: '5' },
    { name: 'len list', code: `len([1, 2, 3])`, expected: '3' },
    { name: 'int from string', code: `int('42')`, expected: '42' },
    { name: 'str from int', code: `str(42)`, expected: '42' },
    { name: 'str from negative', code: `str(0-123)`, expected: '-123' },
    { name: 'str concat', code: `'val: ' + str(255)`, expected: 'val: 255' },
    { name: 'max two args', code: `max(3, 7)`, expected: '7' },
    { name: 'max three args', code: `max(1, 5, 3)`, expected: '5' },
    { name: 'min two args', code: `min(3, 7)`, expected: '3' },
    { name: 'min three args', code: `min(1, 5, 3)`, expected: '1' },

    // === More solana module tests ===
    { name: 'solana epoch plus one', code: `import solana\nsolana.epoch() + 1`, expected: '1' },
    { name: 'bool true', code: `bool(1)`, expected: '1' },
    { name: 'bool false', code: `bool(0)`, expected: '0' },
    { name: 'bool empty string', code: `bool('')`, expected: '0' },
    { name: 'bool non-empty string', code: `bool('x')`, expected: '1' },
    { name: 'bool empty list', code: `bool([])`, expected: '0' },
    { name: 'bool non-empty list', code: `bool([1])`, expected: '1' },

    // === zip() ===
    { name: 'zip iteration count', code: `c=0\nfor t in zip([1,2],[3,4]):\n    c=c+1\nc`, expected: '2' },
    { name: 'zip unequal length', code: `c=0\nfor t in zip([1,2,3],[4,5]):\n    c=c+1\nc`, expected: '2' },

    // === bytearray() ===
    { name: 'bytearray from int', code: `b = bytearray(5)\nlen(b)`, expected: '5' },
    { name: 'bytearray from list', code: `b = bytearray([65,66,67])\nlen(b)`, expected: '3' },
    { name: 'bytearray empty', code: `b = bytearray()\nlen(b)`, expected: '0' },
    { name: 'bytearray subscript', code: `b = bytearray([65,66,67])\nb[0]`, expected: '65' },
    { name: 'bytearray subscript middle', code: `b = bytearray([10,20,30])\nb[1]`, expected: '20' },

    // === zip tuple subscript ===
    { name: 'zip tuple access', code: `s=0\nfor t in zip([1,2],[10,20]):\n    s=s+t[0]+t[1]\ns`, expected: '33' },

    // === Classes ===
    {
        name: 'class with attribute',
        code: `class Foo:\n    x = 5\nf = Foo()\nf.x`,
        expected: '5'
    },
    {
        name: 'class with method',
        code: `class Adder:\n    def add(self, a, b):\n        return a + b\nc = Adder()\nc.add(3, 4)`,
        expected: '7'
    },

    // === Math Module ===
    { name: 'math.sin(0)', code: `import math\nmath.sin(0.0)`, expected: '0.0' },
    { name: 'math.cos(0)', code: `import math\nmath.cos(0.0)`, expected: '1.0' },
    { name: 'math.sqrt(4)', code: `import math\nmath.sqrt(4.0)`, expected: '2.0' },
    { name: 'math.sqrt(9)', code: `import math\nmath.sqrt(9.0)`, expected: '3.0' },
    { name: 'math.floor(3.7)', code: `import math\nmath.floor(3.7)`, expected: '3' },
    { name: 'math.ceil(3.2)', code: `import math\nmath.ceil(3.2)`, expected: '4' },
    { name: 'math.fabs(-5.5)', code: `import math\nmath.fabs(0.0-5.5)`, expected: '5.5' },
    { name: 'math.pow(2,3)', code: `import math\nmath.pow(2.0, 3.0)`, expected: '8.0' },
    { name: 'math.log(1)', code: `import math\nmath.log(1.0)`, expected: '0.0' },
    { name: 'math.exp(0)', code: `import math\nmath.exp(0.0)`, expected: '1.0' },
    { name: 'math.pi constant', code: `import math\nmath.pi > 3.14`, expected: 'True' },
    { name: 'math.e constant', code: `import math\nmath.e > 2.71`, expected: 'True' },

    // === Base64 Module ===
    { name: 'base64.b64decode length', code: `import base64\nlen(base64.b64decode('SGVsbG8='))`, expected: '5' },
    { name: 'base64.b64decode empty', code: `import base64\nlen(base64.b64decode(''))`, expected: '0' },
    { name: 'base64.b64decode single char', code: `import base64\nlen(base64.b64decode('QQ=='))`, expected: '1' },
    { name: 'base64.b64encode', code: `import base64\nbase64.b64encode(b'Hello')`, expected: 'SGVsbG8=' },
    { name: 'base64.b64encode empty', code: `import base64\nlen(base64.b64encode(b''))`, expected: '0' },
    { name: 'base64.b64encode single', code: `import base64\nbase64.b64encode(b'A')`, expected: 'QQ==' },
    { name: 'base64 roundtrip', code: `import base64\nbase64.b64encode(base64.b64decode('SGVsbG8='))`, expected: 'SGVsbG8=' },

    // === JSON Module - dumps ===
    { name: 'json.dumps int', code: `import json\njson.dumps(42)`, expected: '42' },
    { name: 'json.dumps negative int', code: `import json\njson.dumps(0-123)`, expected: '-123' },
    { name: 'json.dumps float', code: `import json\njson.dumps(3.14)`, expected: '3.14' },
    { name: 'json.dumps string', code: `import json\njson.dumps('hello')`, expected: '"hello"' },
    { name: 'json.dumps string escape', code: `import json\njson.dumps('a"b')`, expected: '"a\\"b"' },
    { name: 'json.dumps empty list', code: `import json\njson.dumps([])`, expected: '[]' },
    { name: 'json.dumps list', code: `import json\njson.dumps([1,2,3])`, expected: '[1,2,3]' },
    { name: 'json.dumps nested list', code: `import json\njson.dumps([[1,2],[3,4]])`, expected: '[[1,2],[3,4]]' },
    { name: 'json.dumps empty dict', code: `import json\nd = {}\njson.dumps(d)`, expected: '{}' },
    { name: 'json.dumps dict', code: `import json\nd = {'a': 1}\njson.dumps(d)`, expected: '{"a":1}' },
    { name: 'json.dumps dict string val', code: `import json\nd = {'name': 'test'}\njson.dumps(d)`, expected: '{"name":"test"}' },
    { name: 'json.dumps dict float val', code: `import json\nd = {'pi': 3.14}\njson.dumps(d)`, expected: '{"pi":3.14}' },
    { name: 'json.dumps dict list val', code: `import json\nd = {'arr': [1,2]}\njson.dumps(d)`, expected: '{"arr":[1,2]}' },

    // === JSON Module - loads ===
    { name: 'json.loads int', code: `import json\njson.loads('42')`, expected: '42' },
    { name: 'json.loads negative int', code: `import json\njson.loads('-123')`, expected: '-123' },
    { name: 'json.loads float', code: `import json\njson.loads('3.14')`, expected: '3.14' },
    { name: 'json.loads string', code: `import json\njson.loads('"hello"')`, expected: 'hello' },
    { name: 'json.loads bool true', code: `import json\njson.loads('true')`, expected: '1' },
    { name: 'json.loads bool false', code: `import json\njson.loads('false')`, expected: '0' },
    { name: 'json.loads null', code: `import json\njson.loads('null')`, expected: 'None' },
    { name: 'json.loads empty list', code: `import json\nlen(json.loads('[]'))`, expected: '0' },
    { name: 'json.loads list length', code: `import json\nlen(json.loads('[1,2,3]'))`, expected: '3' },
    { name: 'json.loads list element', code: `import json\njson.loads('[10,20,30]')[1]`, expected: '20' },
    { name: 'json.loads empty dict', code: `import json\nlen(json.loads('{}'))`, expected: '0' },
    { name: 'json.loads dict', code: `import json\nd = json.loads('{"a":1}')\nd['a']`, expected: '1' },
    { name: 'json.loads dict len', code: `import json\nlen(json.loads('{"a":1,"b":2}'))`, expected: '2' },
    { name: 'json.loads dict string val', code: `import json\njson.loads('{"name":"test"}')['name']`, expected: 'test' },
    { name: 'json.loads nested list', code: `import json\njson.loads('[[1,2],[3,4]]')[0][1]`, expected: '2' },
    { name: 'json.loads nested dict', code: `import json\njson.loads('{"a":{"b":5}}')['a']['b']`, expected: '5' },

    // === JSON Module - roundtrip ===
    { name: 'json roundtrip list', code: `import json\njson.loads(json.dumps([1,2,3]))[2]`, expected: '3' },
    { name: 'json roundtrip dict', code: `import json\nd = {'x': 42}\njson.loads(json.dumps(d))['x']`, expected: '42' },

    // === Dict Operations ===
    { name: 'dict create', code: `d = {'x': 10}\nd['x']`, expected: '10' },
    { name: 'dict len', code: `d = {'a': 1, 'b': 2}\nlen(d)`, expected: '2' },

    // === Struct Module ===
    { name: 'struct.calcsize B', code: `import struct\nstruct.calcsize('B')`, expected: '1' },
    { name: 'struct.calcsize H', code: `import struct\nstruct.calcsize('H')`, expected: '2' },
    { name: 'struct.calcsize I', code: `import struct\nstruct.calcsize('I')`, expected: '4' },
    { name: 'struct.calcsize Q', code: `import struct\nstruct.calcsize('Q')`, expected: '8' },
    { name: 'struct.calcsize 4B', code: `import struct\nstruct.calcsize('4B')`, expected: '4' },
    { name: 'struct.pack unpack B', code: `import struct\nstruct.unpack('B', struct.pack('B', [255]))[0]`, expected: '255' },
    { name: 'struct.pack unpack H', code: `import struct\nstruct.unpack('<H', struct.pack('<H', [0x1234]))[0]`, expected: '4660' },

    // === Solana Hash Functions ===
    { name: 'solana.sha256 returns 32 bytes', code: `import solana\nlen(solana.sha256(bytearray([1,2,3])))`, expected: '32' },
    { name: 'solana.keccak256 returns 32 bytes', code: `import solana\nlen(solana.keccak256(bytearray([1,2,3])))`, expected: '32' },
    // Note: blake3 syscall not available on mainnet yet

    // === Hash Printing Format (test %02x fix) ===
    // sha256 and keccak256 return bytes objects that should be formatted as b'\xNN\xNN...'
    // Before the fix, they would show b'\x%02x\x%02x...' which is wrong
    { name: 'sha256 output is bytes format', code: `import solana\nh = solana.sha256(bytearray([1,2,3]))\nh`, expected: "b'\\x20\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x03\\x90\\x58\\xc6\\xf2\\xc0\\xcb\\x49\\x2c\\x53\\x3b\\x0a\\x4d\\x14\\xef\\x77\\xcc\\x0f\\x78\\xab\\xcc\\xce\\xd5\\x28'" },
    { name: 'keccak256 output is bytes format', code: `import solana\nh = solana.keccak256(bytearray([1,2,3]))\nh`, expected: "b'\\x20\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\xf1\\x88\\x5e\\xda\\x54\\xb7\\xa0\\x53\\x31\\x8c\\xd4\\x1e\\x20\\x93\\x22\\x0d\\xab\\x15\\xd6\\x53\\x81\\xb1\\x15\\x7a'" },

    // === Base58 Module ===
    { name: 'base58.b58decode pubkey', code: `import base58\nlen(base58.b58decode('11111111111111111111111111111111'))`, expected: '32' },
    { name: 'base58.b58decode short', code: `import base58\nlen(base58.b58decode('1'))`, expected: '1' },
    { name: 'base58.b58encode roundtrip', code: `import base58\nbase58.b58encode(base58.b58decode('ABC'))`, expected: 'ABC' },
    { name: 'base58.b58encode bytes', code: `import base58\nbase58.b58encode(bytearray([0, 0, 1]))`, expected: '112' },
    { name: 'base58.b58encode from decode', code: `import base58\nbase58.b58encode(base58.b58decode('112'))`, expected: '112' },
    // Token Program pubkey roundtrip
    { name: 'base58 token program roundtrip', code: `import base58\nbase58.b58encode(base58.b58decode('TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA'))`, expected: 'TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA' },
    // Associated Token Program
    { name: 'base58 ATA program roundtrip', code: `import base58\nbase58.b58encode(base58.b58decode('ATokenGPvbdGVxr1b2hvZbsiqW5xWH25efTNsLJA8knL'))`, expected: 'ATokenGPvbdGVxr1b2hvZbsiqW5xWH25efTNsLJA8knL' },
    // System program (all zeros)
    { name: 'base58 system program roundtrip', code: `import base58\nbase58.b58encode(base58.b58decode('11111111111111111111111111111111'))`, expected: '11111111111111111111111111111111' },
    // Decode length checks
    { name: 'base58.b58decode 2 char', code: `import base58\nlen(base58.b58decode('2g'))`, expected: '1' },
    { name: 'base58.b58decode token program len', code: `import base58\nlen(base58.b58decode('TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA'))`, expected: '32' },
    // Metaplex Token Metadata program
    { name: 'base58 metaplex roundtrip', code: `import base58\nbase58.b58encode(base58.b58decode('metaqbxxUerdq28cj1RbAWkYQm3ybzjb6a8bt518x1s'))`, expected: 'metaqbxxUerdq28cj1RbAWkYQm3ybzjb6a8bt518x1s' },
    // Simple encode/decode
    { name: 'base58 roundtrip 2', code: `import base58\nbase58.b58encode(base58.b58decode('2'))`, expected: '2' },
    { name: 'base58 roundtrip Z', code: `import base58\nbase58.b58encode(base58.b58decode('z'))`, expected: 'z' },
];

/**
 * Compile Python to bytecode using native compiler
 */
function compile(pythonCode) {
    const result = spawnSync(COMPILER_PATH, [pythonCode], {
        encoding: 'utf8',
        timeout: 5000,
    });

    if (result.status !== 0) {
        throw new Error(`Compiler failed: ${result.stderr || result.error}`);
    }

    const output = result.stdout.trim();
    if (!output.startsWith('0x')) {
        throw new Error(`Invalid compiler output: ${output}`);
    }

    return Buffer.from(output.slice(2), 'hex');
}

/**
 * Execute bytecode on-chain
 */
async function executeBytecode(connection, programId, payer, bytecode) {
    const modeBuffer = Buffer.alloc(1);
    modeBuffer.writeUInt8(MODE_EXECUTE_BYTECODE, 0);
    const data = Buffer.concat([modeBuffer, bytecode]);

    const instruction = new TransactionInstruction({
        keys: [],
        programId,
        data,
    });

    const transaction = new Transaction();
    transaction.add(
        ComputeBudgetProgram.setComputeUnitLimit({ units: 1_400_000 }),
        ComputeBudgetProgram.requestHeapFrame({ bytes: 256 * 1024 }),
        instruction
    );

    transaction.feePayer = payer.publicKey;
    transaction.recentBlockhash = (await connection.getLatestBlockhash()).blockhash;

    const simResult = await connection.simulateTransaction(transaction);

    if (simResult.value.err) {
        const logs = simResult.value.logs?.join('\n') || 'No logs';
        throw new Error(`Execution failed: ${JSON.stringify(simResult.value.err)}\nLogs:\n${logs}`);
    }

    // Extract CU usage
    let cu = null;
    const cuLog = simResult.value.logs?.find(log => log.includes('consumed'));
    if (cuLog) {
        const match = cuLog.match(/consumed (\d+) of/);
        if (match) cu = parseInt(match[1]);
    }

    // Extract return data
    let output = null;
    if (simResult.value.returnData) {
        const rd = simResult.value.returnData;
        output = Buffer.from(rd.data[0], rd.data[1]).toString('utf8');
    }

    // Send transaction
    await sendAndConfirmTransaction(connection, transaction, [payer], {
        commitment: 'confirmed',
        skipPreflight: true,
    });

    return { output, cu };
}

/**
 * Run a single test
 */
async function runTest(connection, programId, payer, test, verbose) {
    try {
        const bytecode = compile(test.code);
        const { output, cu } = await executeBytecode(connection, programId, payer, bytecode);

        const passed = output?.trim() === test.expected;

        if (passed) {
            console.log(`${GREEN}PASS${RESET} ${test.name} ${DIM}(${cu} CU, ${bytecode.length}B)${RESET}`);
            if (verbose) {
                console.log(`  ${DIM}Code: ${test.code.replace(/\n/g, '\\n')}${RESET}`);
            }
            return { passed: true, cu, bytecodeSize: bytecode.length };
        } else {
            console.log(`${RED}FAIL${RESET} ${test.name}`);
            console.log(`  ${DIM}Code: ${test.code.replace(/\n/g, '\\n')}${RESET}`);
            console.log(`  Expected: ${test.expected}`);
            console.log(`  Actual: ${output}`);
            return { passed: false };
        }
    } catch (err) {
        console.log(`${RED}FAIL${RESET} ${test.name}`);
        console.log(`  ${RED}Error: ${err.message}${RESET}`);
        return { passed: false };
    }
}

/**
 * Main
 */
async function main() {
    const verbose = process.argv.includes('--verbose') || process.argv.includes('-v');

    console.log(`${CYAN}========================================${RESET}`);
    console.log(`${CYAN}PikaPython VM-Only Tests${RESET}`);
    console.log(`${CYAN}(Native Compiler + On-Chain VM)${RESET}`);
    console.log(`${CYAN}========================================${RESET}\n`);

    // Check prerequisites
    if (!fs.existsSync(COMPILER_PATH)) {
        console.error(`${RED}Error: Native compiler not found at ${COMPILER_PATH}${RESET}`);
        console.error(`Run: cd tools && ./build_compiler.sh`);
        process.exit(1);
    }

    if (!fs.existsSync(PIKA_KEYPAIR_PATH)) {
        console.error(`${RED}Error: pika-keypair.json not found${RESET}`);
        console.error(`Run: ./build.sh`);
        process.exit(1);
    }

    // Connect
    const connection = new Connection(RPC_URL, 'confirmed');
    const payerKeypairPath = process.env.HOME + '/.config/solana/id.json';
    const payerKeypair = JSON.parse(fs.readFileSync(payerKeypairPath, 'utf8'));
    const payer = Keypair.fromSecretKey(new Uint8Array(payerKeypair));

    const programKeypair = JSON.parse(fs.readFileSync(PIKA_KEYPAIR_PATH, 'utf8'));
    const programId = Keypair.fromSecretKey(new Uint8Array(programKeypair)).publicKey;

    console.log(`Program: ${programId.toString()}`);
    console.log(`Mode: EXECUTE_BYTECODE (0x02)`);
    console.log(`Tests: ${TEST_CASES.length}\n`);

    // Run tests
    let passed = 0, failed = 0, skipped = 0, totalCU = 0, totalBytes = 0;

    for (const test of TEST_CASES) {
        if (test.skip) {
            console.log(`${CYAN}SKIP${RESET} ${test.name} ${DIM}(${test.skip})${RESET}`);
            skipped++;
            continue;
        }
        const result = await runTest(connection, programId, payer, test, verbose);
        if (result.passed) {
            passed++;
            totalCU += result.cu || 0;
            totalBytes += result.bytecodeSize || 0;
        } else {
            failed++;
        }
    }

    // Summary
    console.log(`\n${CYAN}========================================${RESET}`);
    console.log(`${CYAN}Summary${RESET}`);
    console.log(`${CYAN}========================================${RESET}`);
    console.log(`${GREEN}Passed: ${passed}${RESET}`);
    if (failed > 0) console.log(`${RED}Failed: ${failed}${RESET}`);
    if (skipped > 0) console.log(`${CYAN}Skipped: ${skipped}${RESET}`);
    console.log(`Total: ${passed + failed + skipped}`);
    if (passed > 0) {
        console.log(`${DIM}Average CU: ${Math.round(totalCU / passed)}${RESET}`);
        console.log(`${DIM}Average bytecode: ${Math.round(totalBytes / passed)} bytes${RESET}`);
    }

    process.exit(failed > 0 ? 1 : 0);
}

main().catch(err => {
    console.error(`${RED}Fatal error: ${err.message}${RESET}`);
    process.exit(1);
});
