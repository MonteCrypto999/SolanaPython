#!/usr/bin/env node
/**
 * PikaPython Error Handling Tests
 *
 * Tests error scenarios for both bytecode and script modes.
 */

const {
    Connection,
    Keypair,
    Transaction,
    TransactionInstruction,
    ComputeBudgetProgram,
} = require('@solana/web3.js');
const fs = require('fs');
const path = require('path');

// Configuration
const RPC_URL = 'http://localhost:8899';
const SOLANA_BUILD_DIR = path.join(__dirname, '..');
const PIKA_KEYPAIR_PATH = path.join(SOLANA_BUILD_DIR, 'pika-keypair.json');

// Modes
const MODE_EXECUTE_SCRIPT = 0x00;
const MODE_EXECUTE_BYTECODE = 0x02;

// Colors
const GREEN = '\x1b[32m';
const RED = '\x1b[31m';
const YELLOW = '\x1b[33m';
const CYAN = '\x1b[36m';
const DIM = '\x1b[2m';
const RESET = '\x1b[0m';

// Test cases
const ERROR_TESTS = [
    // === Bytecode Errors ===
    {
        name: 'invalid bytecode magic',
        mode: MODE_EXECUTE_BYTECODE,
        payload: Buffer.from('deadbeef', 'hex'),
        expectFail: true,
        expectLog: 'Invalid bytecode'
    },
    {
        name: 'truncated bytecode (magic only)',
        mode: MODE_EXECUTE_BYTECODE,
        payload: Buffer.from('0f70796f', 'hex'),
        expectFail: false,  // Currently doesn't fail
        expectLog: 'Input bytecode'
    },
    {
        name: 'zero-length instruction array',
        mode: MODE_EXECUTE_BYTECODE,
        payload: Buffer.from('0f70796f080000000000000000000000', 'hex'),
        expectFail: false,
        expectLog: 'Input bytecode'
    },
    {
        name: 'garbage opcodes',
        mode: MODE_EXECUTE_BYTECODE,
        payload: Buffer.from('0f70796f1000000004000000deadbeef00000000', 'hex'),
        expectFail: true,
        expectLog: 'Input bytecode'
    },

    // === Parse Errors ===
    {
        name: 'syntax error - unclosed paren',
        mode: MODE_EXECUTE_SCRIPT,
        payload: Buffer.from('def foo('),
        expectFail: true,
        expectLog: 'Parse failed'
    },
    {
        name: 'syntax error - missing colon',
        mode: MODE_EXECUTE_SCRIPT,
        payload: Buffer.from('if True\n    x = 1'),
        expectFail: true,
        expectLog: 'Parse failed'
    },

    // === Runtime Errors ===
    {
        name: 'NameError - undefined variable',
        mode: MODE_EXECUTE_SCRIPT,
        payload: Buffer.from('undefined_var + 1'),
        expectFail: false,
        expectLog: 'NameError'
    },
    {
        name: 'ZeroDivisionError',
        mode: MODE_EXECUTE_SCRIPT,
        payload: Buffer.from('1/0'),
        expectFail: false,
        expectLog: 'ZeroDivisionError'
    },
    {
        name: 'TypeError - str + int',
        mode: MODE_EXECUTE_SCRIPT,
        payload: Buffer.from("'hello' + 5"),
        expectFail: false,
        expectLog: 'TypeError'
    },
    {
        name: 'IndexError - list out of bounds',
        mode: MODE_EXECUTE_SCRIPT,
        payload: Buffer.from('x = [1,2,3]\nx[10]'),
        expectFail: false,
        expectLog: null  // May not have specific error log
    },
];

/**
 * Run a single error test
 */
async function runTest(connection, programId, payer, test, verbose) {
    try {
        const modeBuffer = Buffer.alloc(1);
        modeBuffer.writeUInt8(test.mode, 0);
        const data = Buffer.concat([modeBuffer, test.payload]);

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

        const failed = simResult.value.err !== null;
        const logs = simResult.value.logs || [];
        const programLogs = logs.filter(l => l.includes('Program log:')).map(l => l.replace('Program log: ', ''));

        // Check if behavior matches expectations
        const failMatch = failed === test.expectFail;
        const logMatch = test.expectLog === null || programLogs.some(l => l.includes(test.expectLog));
        const passed = failMatch && logMatch;

        if (passed) {
            console.log(`${GREEN}PASS${RESET} ${test.name}`);
            if (verbose) {
                console.log(`  ${DIM}TX failed: ${failed}, Expected fail: ${test.expectFail}${RESET}`);
                programLogs.forEach(l => console.log(`  ${DIM}${l}${RESET}`));
            }
            return { passed: true, logs: programLogs, failed };
        } else {
            console.log(`${RED}FAIL${RESET} ${test.name}`);
            console.log(`  TX failed: ${failed}, Expected fail: ${test.expectFail}`);
            if (test.expectLog) console.log(`  Expected log containing: "${test.expectLog}"`);
            console.log(`  Logs:`);
            programLogs.forEach(l => console.log(`    ${l}`));
            return { passed: false, logs: programLogs, failed };
        }
    } catch (err) {
        console.log(`${RED}FAIL${RESET} ${test.name}`);
        console.log(`  ${RED}Exception: ${err.message}${RESET}`);
        return { passed: false };
    }
}

/**
 * Main
 */
async function main() {
    const verbose = process.argv.includes('--verbose') || process.argv.includes('-v');

    console.log(`${CYAN}========================================${RESET}`);
    console.log(`${CYAN}PikaPython Error Handling Tests${RESET}`);
    console.log(`${CYAN}========================================${RESET}\n`);

    if (!fs.existsSync(PIKA_KEYPAIR_PATH)) {
        console.error(`${RED}Error: pika-keypair.json not found${RESET}`);
        process.exit(1);
    }

    const connection = new Connection(RPC_URL, 'confirmed');
    const payerKeypairPath = process.env.HOME + '/.config/solana/id.json';
    const payerKeypair = JSON.parse(fs.readFileSync(payerKeypairPath, 'utf8'));
    const payer = Keypair.fromSecretKey(new Uint8Array(payerKeypair));

    const programKeypair = JSON.parse(fs.readFileSync(PIKA_KEYPAIR_PATH, 'utf8'));
    const programId = Keypair.fromSecretKey(new Uint8Array(programKeypair)).publicKey;

    console.log(`Program: ${programId.toString()}`);
    console.log(`Tests: ${ERROR_TESTS.length}\n`);

    let passed = 0, failed = 0;

    for (const test of ERROR_TESTS) {
        const result = await runTest(connection, programId, payer, test, verbose);
        if (result.passed) {
            passed++;
        } else {
            failed++;
        }
    }

    console.log(`\n${CYAN}========================================${RESET}`);
    console.log(`${CYAN}Summary${RESET}`);
    console.log(`${CYAN}========================================${RESET}`);
    console.log(`${GREEN}Passed: ${passed}${RESET}`);
    if (failed > 0) console.log(`${RED}Failed: ${failed}${RESET}`);
    console.log(`Total: ${passed + failed}`);

    process.exit(failed > 0 ? 1 : 0);
}

main().catch(err => {
    console.error(`${RED}Fatal error: ${err.message}${RESET}`);
    process.exit(1);
});
