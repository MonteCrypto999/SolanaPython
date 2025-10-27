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

    // === Solana Builtins ===
    { name: 'time returns unix timestamp', code: `time() > 1700000000`, expected: 'True' },
    { name: 'slot returns positive', code: `slot() > 0`, expected: 'True' },

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
    { name: 'max two args', code: `max(3, 7)`, expected: '7' },
    { name: 'max three args', code: `max(1, 5, 3)`, expected: '5' },
    { name: 'min two args', code: `min(3, 7)`, expected: '3' },
    { name: 'min three args', code: `min(1, 5, 3)`, expected: '1' },

    // === epoch() and bool() ===
    { name: 'epoch returns value', code: `epoch() + 1`, expected: '1' },
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
    let passed = 0, failed = 0, totalCU = 0, totalBytes = 0;

    for (const test of TEST_CASES) {
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
    console.log(`Total: ${passed + failed}`);
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
