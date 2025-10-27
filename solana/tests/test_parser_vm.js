#!/usr/bin/env node
/**
 * PikaPython Parser+VM Tests
 *
 * Tests the on-chain parser AND VM by sending Python source code
 * directly to the program (mode 0x00 EXECUTE_SCRIPT).
 *
 * This tests the full pipeline: parsing + compilation + execution on-chain.
 *
 * Usage:
 *   node test_parser_vm.js           Run all tests
 *   node test_parser_vm.js --verbose Show detailed output
 */

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

// Execution mode for script (parser + VM)
const MODE_EXECUTE_SCRIPT = 0x00;

// Colors
const GREEN = '\x1b[32m';
const RED = '\x1b[31m';
const YELLOW = '\x1b[33m';
const CYAN = '\x1b[36m';
const DIM = '\x1b[2m';
const RESET = '\x1b[0m';

// Test cases: { name, code, expected, checkLogs }
// REPL-style: expressions return their value, print() goes to logs only
const TEST_CASES = [
    // === Basic Expressions (REPL-style return) ===
    { name: 'integer literal', code: `42`, expected: '42' },
    { name: 'string literal', code: `'Hello'`, expected: 'Hello' },
    { name: 'negative number', code: `-5`, expected: '-5' },
    { name: 'large number', code: `999999`, expected: '999999' },

    // === Arithmetic ===
    { name: 'addition', code: `1+2`, expected: '3' },
    { name: 'subtraction', code: `10-3`, expected: '7' },
    { name: 'multiplication', code: `6*7`, expected: '42' },
    { name: 'division', code: `20/4`, expected: '5' },
    { name: 'integer division', code: `20//4`, expected: '5' },
    { name: 'modulo', code: `17%5`, expected: '2' },
    { name: 'complex expression', code: `1+2+3`, expected: '6' },
    { name: 'operator precedence', code: `10-2*3`, expected: '4' },
    { name: 'parentheses', code: `(2+3)*4`, expected: '20' },

    // === Floating Point ===
    { name: 'float literal', code: `1.5`, expected: '1.5' },
    { name: 'float add', code: `1.5+2.5`, expected: '4' },
    { name: 'float multiply', code: `2.5*4`, expected: '10' },
    { name: 'float divide', code: `10/4`, expected: '2.5' },
    { name: 'negative float', code: `-1.25`, expected: '-1.25' },

    // === Variables ===
    { name: 'variable assignment', code: `x = 5\nx`, expected: '5' },
    { name: 'variable arithmetic', code: `a = 10\nb = 20\na+b`, expected: '30' },
    { name: 'variable update', code: `x = 5\nx = x + 1\nx`, expected: '6' },
    { name: 'variable chain', code: `x = 1\ny = 2\nz = x + y\nz`, expected: '3' },
    { name: 'string variable', code: `name = 'Solana'\nname`, expected: 'Solana' },

    // === Comparisons ===
    { name: 'equal true', code: `5 == 5`, expected: 'True' },
    { name: 'equal false', code: `5 == 6`, expected: 'False' },
    { name: 'greater than', code: `10 > 5`, expected: 'True' },
    { name: 'less than', code: `3 < 8`, expected: 'True' },
    { name: 'float compare', code: `3.14 > 3.0`, expected: 'True' },

    // === Conditionals ===
    {
        name: 'if true branch',
        code: `x = 10\nif x > 5:\n    r = 'big'\nelse:\n    r = 'small'\nr`,
        expected: 'big'
    },
    {
        name: 'if false branch',
        code: `x = 3\nif x > 5:\n    r = 'big'\nelse:\n    r = 'small'\nr`,
        expected: 'small'
    },

    // === Loops (simple tests - complex loops tested in VM-only) ===
    {
        name: 'for range simple',
        code: `s = 0\nfor i in range(3):\n    s = s + 1\ns`,
        expected: '3'
    },

    // === Functions ===
    {
        name: 'function definition',
        code: `def add(a, b):\n    return a + b\nadd(3, 4)`,
        expected: '7'
    },
    {
        name: 'function with local',
        code: `def double(x):\n    result = x * 2\n    return result\ndouble(21)`,
        expected: '42'
    },

    // === Print (logs, no return data) ===
    {
        name: 'print goes to logs',
        code: `print('hello')`,
        expected: null, // print returns None, which produces no return data
        checkLogs: 'hello'
    },
    {
        name: 'print then expression',
        code: `print('calculating')\n42`,
        expected: '42',
        checkLogs: 'calculating'
    },

    // === Solana Builtins ===
    {
        name: 'time returns unix timestamp',
        code: `time() > 1700000000`,
        expected: 'True'
    },
    {
        name: 'slot returns positive',
        code: `slot() > 0`,
        expected: 'True'
    },

    // === Boolean Literals ===
    { name: 'True literal', code: `True`, expected: 'True' },
    { name: 'False literal', code: `False`, expected: 'False' },
    { name: 'None is falsy', code: `not None`, expected: 'True' },

    // === Logical Operators ===
    { name: 'and true', code: `True and True`, expected: 'True' },
    { name: 'and false', code: `True and False`, expected: 'False' },
    { name: 'or true', code: `False or True`, expected: 'True' },
    { name: 'or false', code: `False or False`, expected: 'False' },

    // === More Comparisons ===
    { name: 'not equal', code: `5 != 6`, expected: 'True' },
    { name: 'greater equal', code: `6 >= 5`, expected: 'True' },

    // === Lists ===
    { name: 'list literal', code: `x = [1, 2, 3]\nx[0]`, expected: '1' },
    { name: 'list indexing', code: `x = [10, 20, 30]\nx[1]`, expected: '20' },

    // === String Operations ===
    { name: 'string concat', code: `'Hello' + ' World'`, expected: 'Hello World' },

    // === New Builtins ===
    { name: 'abs positive', code: `abs(5)`, expected: '5' },
    { name: 'abs negative', code: `abs(-5)`, expected: '5' },
    { name: 'abs zero', code: `abs(0)`, expected: '0' },
    { name: 'abs float', code: `abs(-3.5)`, expected: '3.5' },
    { name: 'len string', code: `len('hello')`, expected: '5' },
    { name: 'len empty string', code: `len('')`, expected: '0' },
    { name: 'len list', code: `len([1, 2, 3])`, expected: '3' },
    { name: 'len empty list', code: `len([])`, expected: '0' },
    { name: 'int from string', code: `int('42')`, expected: '42' },
    { name: 'int from float', code: `int(3.7)`, expected: '3' },
    { name: 'max two args', code: `max(3, 7)`, expected: '7' },
    { name: 'max three args', code: `max(1, 5, 3)`, expected: '5' },
    { name: 'max with zero', code: `max(0, 5, 2)`, expected: '5' },
    { name: 'min two args', code: `min(3, 7)`, expected: '3' },
    { name: 'min three args', code: `min(1, 5, 3)`, expected: '1' },
    { name: 'min with zero', code: `min(0, 5, 2)`, expected: '0' },

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
    {
        name: 'class instance attribute',
        code: `class Counter:\n    def set(self, v):\n        self.val = v\n    def get(self):\n        return self.val\nc = Counter()\nc.set(42)\nc.get()`,
        expected: '42'
    },
];

/**
 * Execute Python source on-chain (parser + VM)
 */
async function executeScript(connection, programId, payer, pythonCode) {
    const modeBuffer = Buffer.alloc(1);
    modeBuffer.writeUInt8(MODE_EXECUTE_SCRIPT, 0);
    const data = Buffer.concat([modeBuffer, Buffer.from(pythonCode, 'utf8')]);

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

    // Extract logs (filter out system logs)
    const logs = (simResult.value.logs || []).filter(log =>
        log.startsWith('Program log:') && !log.includes('[EXECUTE_SCRIPT]') && !log.includes('[PIKA]')
    ).map(log => log.replace('Program log: ', ''));

    // Send transaction
    await sendAndConfirmTransaction(connection, transaction, [payer], {
        commitment: 'confirmed',
        skipPreflight: true,
    });

    return { output, cu, logs };
}

/**
 * Run a single test
 */
async function runTest(connection, programId, payer, test, verbose) {
    try {
        const { output, cu, logs } = await executeScript(connection, programId, payer, test.code);

        // Check return data
        let passed = true;
        if (test.expected !== undefined) {
            if (test.expected === null) {
                passed = output === null || output === '';
            } else {
                passed = output?.trim() === test.expected;
            }
        }

        // Check logs if specified
        if (test.checkLogs && passed) {
            passed = logs.some(log => log.includes(test.checkLogs));
        }

        if (passed) {
            console.log(`${GREEN}PASS${RESET} ${test.name} ${DIM}(${cu} CU)${RESET}`);
            if (verbose) {
                console.log(`  ${DIM}Code: ${test.code.replace(/\n/g, '\\n').substring(0, 60)}${RESET}`);
                if (output) console.log(`  ${DIM}Output: ${output}${RESET}`);
                if (logs.length) console.log(`  ${DIM}Logs: ${logs.join(', ')}${RESET}`);
            }
            return { passed: true, cu };
        } else {
            console.log(`${RED}FAIL${RESET} ${test.name}`);
            console.log(`  ${DIM}Code: ${test.code.replace(/\n/g, '\\n')}${RESET}`);
            console.log(`  Expected: ${test.expected}`);
            console.log(`  Actual: ${output}`);
            if (test.checkLogs) console.log(`  Expected log: ${test.checkLogs}`);
            if (logs.length) console.log(`  Logs: ${logs.join(', ')}`);
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
    console.log(`${CYAN}PikaPython Parser+VM Tests${RESET}`);
    console.log(`${CYAN}(On-Chain Parser + On-Chain VM)${RESET}`);
    console.log(`${CYAN}========================================${RESET}\n`);

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
    console.log(`Mode: EXECUTE_SCRIPT (0x00)`);
    console.log(`Tests: ${TEST_CASES.length}\n`);

    // Run tests
    let passed = 0, failed = 0, totalCU = 0;

    for (const test of TEST_CASES) {
        const result = await runTest(connection, programId, payer, test, verbose);
        if (result.passed) {
            passed++;
            totalCU += result.cu || 0;
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
    }

    process.exit(failed > 0 ? 1 : 0);
}

main().catch(err => {
    console.error(`${RED}Fatal error: ${err.message}${RESET}`);
    process.exit(1);
});
